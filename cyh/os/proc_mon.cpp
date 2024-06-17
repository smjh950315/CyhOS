#include "proc_mon.hpp"
#include "res_mon.hpp"
#include "os_internal.hpp"
#include <map>
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
	static void close_no_handle(void*) {}
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
			_unixProcStat stat = UnixInfoParser::read_proc_stat(pinfo->pid);
			pinfo->user_time = stat.utime + stat.cutime;
			pinfo->kernal_time = stat.stime + stat.cstime;
		}
#endif
	}
#ifdef __WINDOWS_PLATFORM__
	static double get_win_process_cpuPercentage(ProcessInformation* pInfo) {
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
#else
	static double get_unix_delta_cpuTime() {
		auto cpu0 = UnixInfoParser::read_total_cpu_info();
		std::this_thread::sleep_for(std::chrono::milliseconds(1000u));
		auto cpu1 = UnixInfoParser::read_total_cpu_info();
		return static_cast<double>(cpu0.total_time() - cpu1.total_time());
	}
#endif
	// On windows, get cpu usage of process id directly
	// On unix, get proc cpu delta time instead
	static void measure_process_cputime(uint pid, double* pCpuTime) {
		if (pid == ~uint() || !pCpuTime) { return; }
		*pCpuTime = 0.0;
#ifdef __WINDOWS_PLATFORM__
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
		*pCpuTime = get_win_process_cpuPercentage(&result0);
#else
		auto info0 = UnixInfoParser::read_proc_stat(pid);
		std::this_thread::sleep_for(std::chrono::milliseconds(1000u));
		auto info1 = UnixInfoParser::read_proc_stat(pid);
		*pCpuTime = static_cast<double>(info0.total_cpu_time() - info1.total_cpu_time());
#endif
	}
	static void measure_process_cpuTime_batch(uint* ppid, double* pCpuTimes, nuint count) {

		std::vector<std::future<void>> tasks;
		tasks.reserve(count);
#ifndef __WINDOWS_PLATFORM__
		std::future<double> taskDeltaCpuTime = std::async(std::launch::async, get_unix_delta_cpuTime);
#endif
		for (nuint i = 0; i < count; ++i) {
			tasks.push_back(std::async(std::launch::async, measure_process_cputime, ppid[i], pCpuTimes + i));
		}
		for (auto& task : tasks) {
			task.get();
		}
#ifndef __WINDOWS_PLATFORM__
		// for unix measure_process_cputime() will get proc cpu delta time instead
		auto cpuDeltaTime = taskDeltaCpuTime.get();
		if (cpuDeltaTime == 0.0) {
			for (nuint i = 0; i < count; ++i) {
				pCpuTimes[i] = 0.0;
			}
		} else {
			for (nuint i = 0; i < count; ++i) {
				pCpuTimes[i] /= cpuDeltaTime;
			}
		}
#endif
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
		auto procs = GetAllProcessInfo(false);
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
	std::vector<ProcessInformation> ProcessMonitor::GetAllProcessInfo(bool with_details) {
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
#ifndef __WINDOWS_PLATFORM__
		std::future<double> taskGetDeltaCpu = std::async(std::launch::async, get_unix_delta_cpuTime);
#endif
		measure_process_cputime(pid, &result);
#ifndef __WINDOWS_PLATFORM__
		auto deltaCpu = taskGetDeltaCpu.get();
		if (deltaCpu == 0.0) {
			result = 0;
		} else {
			result /= deltaCpu;
		}
#endif
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
	std::vector<ProcessGroup> ProcessMonitor::GetProcessGroups() {
		std::vector<ProcessGroup> result;
		auto procs = GetAllProcessInfo(true);
		std::map<std::string, std::vector<ProcessInformation>> procInfoDict;
		for (auto& proc : procs) {
			procInfoDict[proc.name].push_back(proc);
		}
		result.reserve(procInfoDict.size());

		for (auto& pair : procInfoDict) {
			ProcessGroup group{};
			group.name = pair.first;
			group.sub_procs = std::move(pair.second);
			for (auto& sub_proc : group.sub_procs) {
				group.cpu_time_percentage += sub_proc.cpu_time_percentage;
				group.memory += sub_proc.memory;
			}
			result.push_back(std::move(group));
		}

		return result;
	}
};
