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

	std::vector<uint> ProcessMonitor::GetPIDs(const char* name) {
		std::vector<uint> result;
		auto procs = GetProcessAbstracts();
		for (auto& proc : procs) {
			if (proc.name == name) {
				result.push_back(proc.pid);
			}
		}
		return result;
	}

	std::vector<uint> ProcessMonitor::GetPIDList() {
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

	ProcessAbstract ProcessMonitor::GetProcessAbstract(uint pid) {
		ProcessAbstract abs{};
		abs.pid = -1;
		abs.name = "<unknown>";
#ifdef __WINDOWS_PLATFORM__	
		HANDLE hProcess = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, FALSE, pid);
		if (hProcess == NULL) {
			return abs;
		}
		CHAR exeName[MAX_PATH];
		if (GetModuleBaseName(hProcess, NULL, exeName, MAX_PATH)) {
			abs.name = exeName;
			abs.pid = pid;
		}
#else
		std::string basePath = "/proc/";
		basePath += std::to_string(pid);
		if (!std::filesystem::exists(basePath)) {
			return abs;
		}
		abs.pid = pid;
		{
			std::ifstream comm_file(basePath + "/comm");
			if (comm_file.is_open()) {
				std::getline(comm_file, abs.name);
				comm_file.close();
			}
		}
#endif
		return abs;
	}
	ProcessDetails ProcessMonitor::GetProcessDetail(uint pid) {
		ProcessDetails detail = { ~uint{}, "<unknown>", "<unknown>", 0, 0, 0, 0.0 };
#ifdef __WINDOWS_PLATFORM__	
		HANDLE hProcess = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, FALSE, pid);
		if (hProcess == NULL) {
			return detail;
		}
		CHAR exeName[MAX_PATH];
		if (GetModuleBaseName(hProcess, NULL, exeName, MAX_PATH)) {
			detail.name = exeName;
			detail.pid = pid;
		}
		CHAR exePath[MAX_PATH];
		if (GetModuleFileNameEx(hProcess, NULL, exePath, MAX_PATH)) {
			detail.path = exePath;
		}
		PROCESS_MEMORY_COUNTERS pmc;
		if (GetProcessMemoryInfo(hProcess, &pmc, sizeof(pmc))) {
			detail.memory = pmc.WorkingSetSize;
		}
		FILETIME creationTime, exitTime, kernelTime, userTime;
		if (GetProcessTimes(hProcess, &creationTime, &exitTime, &kernelTime, &userTime)) {
			detail.kernal_time = (((ULONGLONG)kernelTime.dwHighDateTime) << 32) + kernelTime.dwLowDateTime;
			detail.user_time = (((ULONGLONG)userTime.dwHighDateTime) << 32) + userTime.dwLowDateTime;
		}
		CloseHandle(hProcess);
#else
		std::string basePath = "/proc/";
		basePath += std::to_string(pid);
		if (!std::filesystem::exists(basePath)) {
			return detail;
		}
		detail.pid = pid;
		{
			std::ifstream comm_file(basePath + "/comm");
			if (comm_file.is_open()) {
				std::getline(comm_file, detail.name);
				comm_file.close();
			}
		}
		{
			std::ifstream cmdline_file(basePath + "/cmdline");
			if (cmdline_file.is_open()) {
				std::getline(cmdline_file, detail.path, '\0');
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
						detail.memory = UnitConvert::GetRatioToByte(unit) * value;
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
					detail.user_time = std::stol(stat_values[13]);
					detail.kernal_time = std::stol(stat_values[14]);
				}
				stat_file.close();
			}
		}
#endif
		return detail;
		}

	std::vector<ProcessAbstract> ProcessMonitor::GetProcessAbstracts(std::vector<std::string>* pmsgs) {
		std::vector<ProcessAbstract> result{};
		auto pids = GetPIDList();
		for (auto pid : pids) {
			result.push_back(GetProcessAbstract(pid));
		}
		return result;
	}
	std::vector<ProcessDetails> ProcessMonitor::GetProcessDetails(std::vector<std::string>* pmsgs) {
		std::vector<ProcessDetails> results{};
		static auto callback_getCpuTimeRef = [=](uint _pid, double* p_value) {
			if (!p_value) { return; }
			*p_value = GetProcessCpuTime(_pid);
			};
		auto pids = GetPIDList();
		for (auto pid : pids) {
			results.push_back(GetProcessDetail(pid));
		}
		std::vector<std::future<void>> tasksGetCpuTime{};
		for (auto& result : results) {
			tasksGetCpuTime.push_back(std::async(std::launch::async, callback_getCpuTimeRef, result.pid, &result.cpu_time_percentage));
		}
		for (auto& task : tasksGetCpuTime) {
			task.get();
		}
		return results;
	}
	std::string ProcessMonitor::GetProcessName(uint pid) {
		auto proc_abs = GetProcessAbstract(pid);
		return proc_abs.name;
	}
	double ProcessMonitor::GetProcessCpuTime(uint pid) {
		double result{};
		auto detail0 = ProcessMonitor::GetProcessDetail(pid);
		std::this_thread::sleep_for(std::chrono::milliseconds(1000));
		auto detail1 = ProcessMonitor::GetProcessDetail(pid);
		auto delta_time = (detail1.kernal_time + detail1.user_time) - (detail0.kernal_time + detail0.user_time);
#ifdef __WINDOWS_PLATFORM__
		LARGE_INTEGER clock_per_sec{};
		if (QueryPerformanceFrequency(&clock_per_sec)) {
			result = static_cast<double>(delta_time) / ResourceMonitor::GetCpuClock();
		}
		//long instance_count{};
		//std::string query = "\\Process(";
		//query += std::to_string(pid);
		//query += ")\\% Processor Time";
		// "\\Process(Taskmgr)\\% Processor Time"
		//WinPerfmonQuery::QueryForDoubleResult(query, &result, 1000);
#else
		result = (static_cast<double>(delta_time) / ResourceMonitor::GetCpuClock()) / 100000.0;
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

};