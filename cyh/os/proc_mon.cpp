#include "proc_mon.hpp"
#include "res_mon.hpp"
#include "common_internal.hpp"
#ifdef __WINDOWS_PLATFORM__
#include <tlhelp32.h>
#include <Psapi.h>
#include <string>
#else
#include <future>
#endif
namespace cyh::os {
	using FnCloseHandle = void(*)(void*);
	static void close_win_handle(void* handle) {
#ifdef __WINDOWS_PLATFORM__
		HANDLE hndl = (HANDLE*)handle;
		CloseHandle(hndl);
#endif
	}
	static void close_no_handle(void*) { }
	static bool is_valid_process(ProcessInformation* pInfo) {
		if (!pInfo) { return false; }
		return pInfo->pid != ~uint();
	}
	static bool prepare_process_info(uint pid, void** phandle, std::string& unixPath, FnCloseHandle* p_callback_closeHandle) {
		if (!phandle || !p_callback_closeHandle) { return false; }
#ifdef __WINDOWS_PLATFORM__
		* phandle = (void*)OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, FALSE, pid);
		if (*phandle == NULL) {
			return false;
		}
		*p_callback_closeHandle = close_win_handle;
#else		
		unixPath = "/proc/";
		unixPath += std::to_string(pid);
		if (!std::filesystem::exists(unixPath)) {
			return false;
		}
		*p_callback_closeHandle = close_no_handle;
#endif
		return true;
	}
	static void get_process_abstraction(void* handle, ProcessInformation* pinfo, uint pid, const std::string& basePath) {
#ifdef __WINDOWS_PLATFORM__	
		CHAR exeName[MAX_PATH];
		HANDLE hProcess = (HANDLE)handle;
		if (GetModuleBaseName(hProcess, NULL, exeName, MAX_PATH)) {
			pinfo->name = exeName;
			pinfo->pid = pid;
		}
#else
		pinfo->pid = pid;
		{
			std::ifstream comm_file(basePath + "/comm");
			if (comm_file.is_open()) {
				std::getline(comm_file, pinfo->name);
				comm_file.close();
			}
		}
#endif
	}
	static void get_process_detail(void* handle, ProcessInformation* pinfo, const std::string& basePath) {
		if (!pinfo) { return; }
#ifdef __WINDOWS_PLATFORM__	
		HANDLE hProcess = (HANDLE)handle;
		CHAR exePath[MAX_PATH];
		if (GetModuleFileNameEx(hProcess, NULL, exePath, MAX_PATH)) {
			pinfo->path = exePath;
		}
		PROCESS_MEMORY_COUNTERS pmc;
		if (GetProcessMemoryInfo(hProcess, &pmc, sizeof(pmc))) {
			pinfo->memory = pmc.WorkingSetSize;
		}
		FILETIME creationTime, exitTime, kernelTime, userTime;
		if (GetProcessTimes(hProcess, &creationTime, &exitTime, &kernelTime, &userTime)) {
			pinfo->kernal_time = (((ULONGLONG)kernelTime.dwHighDateTime) << 32) + kernelTime.dwLowDateTime;
			pinfo->user_time = (((ULONGLONG)userTime.dwHighDateTime) << 32) + userTime.dwLowDateTime;
		}
#else
		{
			std::ifstream cmdline_file(basePath + "/cmdline");
			if (cmdline_file.is_open()) {
				std::getline(cmdline_file, pinfo->path, '\0');
				cmdline_file.close();
			}
		}
		{
			std::ifstream status_file(basePath + "/status");
			if (status_file.is_open()) {
				std::string line;
				while (std::getline(status_file, line)) {
					if (line.compare(0, 6, "VmRSS:") == 0) {
						std::istringstream iss(line);
						std::string key, unit;
						long value{};
						iss >> key >> value >> unit;
						pinfo->memory = UnitConvert::GetRatioToByte(unit) * value;
						break;
					}
				}
				status_file.close();
			}
		}
		{
			std::ifstream stat_file(basePath + "/stat");
			if (stat_file.is_open()) {
				std::string stat_line;
				std::getline(stat_file, stat_line);
				std::istringstream iss(stat_line);
				std::vector<std::string> stat_values;
				std::string value;
				while (iss >> value) {
					stat_values.push_back(value);
				}
				if (stat_values.size() > 21) {
					pinfo->user_time = std::stol(stat_values[13]);
					pinfo->kernal_time = std::stol(stat_values[14]);
				}
				stat_file.close();
			}
		}
#endif
	}
#ifdef __WINDOWS_PLATFORM__
	static double get_win_process_cpuTime(ProcessInformation* pInfo) {
		if (!pInfo) { return 0.0; }
		if (!is_valid_process(pInfo)) { return 0.0; }

		std::string query = "\\Process V2(";
		{
			nuint extNameBegin = pInfo->name.find(".exe");
			if (extNameBegin != std::string::npos) {
				query += pInfo->name.substr(0, extNameBegin);
			} else {
				query += pInfo->name;
			}
		}
		query += ':';
		query += std::to_string(pInfo->pid);
		query += ")\\% Processor Time";
		double result{};
		return WinPerfmonQuery::QueryForDoubleResult(query, &result, 1000u) ? result : 0.0;
	}
#endif
	static void measure_process_cputime(uint pid, double* pCpuTime) {
		if (pid == ~uint() || !pCpuTime) { return; }
		*pCpuTime = 0.0;
		static auto callbackGetProcDetail = [] (ProcessInformation* presult, uint _pid) {
			void* handle{};
			std::string unixPath;
			FnCloseHandle callback_closeHandle{};
			if (prepare_process_info(_pid, &handle, unixPath, &callback_closeHandle)) {
				get_process_abstraction(handle, presult, _pid, unixPath);
				get_process_detail(handle, presult, unixPath);
				callback_closeHandle(handle);
			}
		};
		ProcessInformation result0 = { ~uint{}, "<unknown>", "<unknown>", 0, 0, 0, 0.0 };
		callbackGetProcDetail(&result0, pid);
#ifdef __WINDOWS_PLATFORM__
		* pCpuTime = get_win_process_cpuTime(&result0);
#else
		if (is_valid_process(&result0)) {
			ProcessInformation result1 = { ~uint{}, "<unknown>", "<unknown>", 0, 0, 0, 0.0 };
			callbackGetProcDetail(&result1, pid);
			std::this_thread::sleep_for(std::chrono::milliseconds(1000));
			auto delta_time = (result1.kernal_time + result1.user_time) - (result0.kernal_time + result0.user_time);
			*pCpuTime = static_cast<double>(delta_time) / ResourceMonitor::GetCpuClock();;
		}
#endif
	}
	static void measure_process_cpuTime_batch(uint* ppid, double* pCpuTimes, nuint count) {
		std::vector<std::future<void>> tasks;
		tasks.reserve(count);
		for (nuint i = 0; i < count; ++i) {
			tasks.push_back(std::async(std::launch::async, measure_process_cputime, ppid[i], pCpuTimes + i));
		}
		for (auto& task : tasks) {
			task.get();
		}
	}

	std::vector<uint> ProcessMonitor::GetProcessIDs() {
		std::vector<uint> result;
#ifdef __WINDOWS_PLATFORM__
		HANDLE hProcessSnap;
		PROCESSENTRY32 pe32{};
		// Take a snapshot of all processes in the system.
		hProcessSnap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
		if (hProcessSnap == INVALID_HANDLE_VALUE) {
			return result;
		}
		// Set the size of the structure before using it.
		pe32.dwSize = sizeof(PROCESSENTRY32);
		// Retrieve information about the first process, and exit if unsuccessful
		if (!Process32First(hProcessSnap, &pe32)) {
			// Clean the snapshot object
			CloseHandle(hProcessSnap);
			return result;
		}
		// Now walk the snapshot of processes, and display information about each process in turn
		do {
			result.push_back(pe32.th32ProcessID);
		} while (Process32Next(hProcessSnap, &pe32));
		// Clean the snapshot object
		CloseHandle(hProcessSnap);
#else
		for (const auto& entry : std::filesystem::directory_iterator("/proc")) {
			if (entry.is_directory()) {
				std::string pid = entry.path().filename().string();
				if (std::all_of(pid.begin(), pid.end(), ::isdigit)) {
					result.push_back(std::stoul(pid));
				}
			}
		}
#endif
		return result;
	}
	std::vector<uint> ProcessMonitor::GetProcessIDs(const char* name) {
		std::vector<uint> result;
		auto procs = GetProcessInfos(false);
		for (auto& proc : procs) {
			if (proc.name == name) {
				result.push_back(proc.pid);
			}
		}
		return result;
	}
	ProcessInformation ProcessMonitor::GetProcessInfo(uint pid, bool with_details) {
		ProcessInformation result = { ~uint{}, "<unknown>", "<unknown>", 0, 0, 0, 0.0 };
		void* handle{};
		std::string unixPath;
		FnCloseHandle callback_closeHandle{};
		if (prepare_process_info(pid, &handle, unixPath, &callback_closeHandle)) {
			get_process_abstraction(handle, &result, pid, unixPath);
			if (with_details) {
				get_process_detail(handle, &result, unixPath);
			}
			callback_closeHandle(handle);
		}
		return result;
	}
	std::vector<ProcessInformation> ProcessMonitor::GetProcessInfos(bool with_details) {
		std::vector<ProcessInformation> result;
		auto pids = GetProcessIDs();
		auto count = pids.size();
		if (!count) { return result; }
		std::vector<double> cpuTimes{};
		cpuTimes.resize(count);
		uint* ppids = pids.data();
		double* pTimes = cpuTimes.data();
		std::future<void> taskGetCpuTimes = std::async(std::launch::async, measure_process_cpuTime_batch, ppids, pTimes, count);
		for (auto& pid : pids) {
			result.push_back(GetProcessInfo(pid, with_details));
		}
		ProcessInformation* presult = result.data();
		taskGetCpuTimes.get();
		for (nuint i = 0; i < count; ++i) {
			presult[i].cpu_time_percentage = pTimes[i];
		}
		return result;
	}
	std::string ProcessMonitor::GetProcessName(uint pid) {
		return GetProcessInfo(pid, false).name;
	}
	double ProcessMonitor::GetProcessCpuTime(uint pid) {
		double result{};
		measure_process_cputime(pid, &result);
		return result;
	}
	bool ProcessMonitor::ForceKillProcess(uint pid) {
#ifdef __WINDOWS_PLATFORM__	
		HANDLE hndl = (HANDLE)OpenProcess(PROCESS_ALL_ACCESS, FALSE, pid);
		if (hndl) {
			auto res = TerminateProcess(hndl, -1);
			if (!res) { return false; }
			DWORD exitCode{};
			res = GetExitCodeProcess(hndl, &exitCode);
			if (!res) { return false; }
			if (exitCode == STATUS_ACCESS_VIOLATION) {
				return false;
			}
			while (exitCode == STILL_ACTIVE && res != 0) {
				res = GetExitCodeProcess(hndl, &exitCode);
				Sleep(1000);
			}
			return true;
		}
#endif
		return false;
	}
};