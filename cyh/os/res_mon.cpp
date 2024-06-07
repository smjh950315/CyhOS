#include "res_mon.hpp"
#include "common_internal.hpp"
#include <cstring>
#include <string>
namespace cyh::os {
#ifndef __WINDOWS_PLATFORM__
	static float read_cpu_usage(const std::string& rawStr) {
		struct _unixCpuInfo {
			long user;
			long nice;
			long system;
			long idle;
			long iowait;
			long irq;
			long softirq;
			long steal;
		} cpuUsage;
		static auto fn_get_percentage = [=](_unixCpuInfo* pInfo) {
			long idle = pInfo->idle + pInfo->iowait;
			long nonIdle = pInfo->user + pInfo->nice + pInfo->system + pInfo->irq + pInfo->softirq + pInfo->steal;
			long total = idle + nonIdle;
			return static_cast<float>(static_cast<float>(total - idle) * 100.f / static_cast<float>(total));
			};
		memset(&cpuUsage, 0, sizeof(_unixCpuInfo));
		std::istringstream ss(rawStr);
		std::string _cpu;
		ss >> _cpu >> cpuUsage.user >> cpuUsage.nice >> cpuUsage.system >> cpuUsage.idle >> cpuUsage.iowait >> cpuUsage.irq >> cpuUsage.softirq >> cpuUsage.steal;
		return fn_get_percentage(&cpuUsage);
	}
#endif

	double ResourceMonitor::GetCpuClock() {
		static double result{};
		if (result != 0.0) { return result; }
#ifdef __WINDOWS_PLATFORM__
		LARGE_INTEGER clock_per_sec{};
		if (QueryPerformanceFrequency(&clock_per_sec)) {
			result = static_cast<double>(clock_per_sec.QuadPart);
		} else {
			result = -1;
		}
#else
		result = static_cast<double>(sysconf(_SC_CLK_TCK));
#endif
		return result;
	}
	long ResourceMonitor::GetProcessorCount() {
#ifdef __WINDOWS_PLATFORM__
		SYSTEM_INFO info{};
		GetSystemInfo(&info);
		return static_cast<long>(info.dwNumberOfProcessors);
#else
		std::ifstream file("/proc/stat");
		std::string line;

		if (file.is_open()) {
			long count = 0;
			while (getline(file, line)) {

				auto begin_index = line.find("cpu");
				// break if line is not start with cpu
				if (begin_index == std::string::npos) { break; }
				// pass first line
				if (line.find("cpu ") == 0) { continue; }
				++count;
			}
			file.close();
			return count;
		} else {
			return 0;
		}
#endif
	}
	double ResourceMonitor::GetProcessorUsage(uint cpu_no) {
#ifdef __WINDOWS_PLATFORM__
		static auto callback_getQueryCommand = [=](uint cpu_no) {
			std::string cmd = "\\Processor Information(0,";
			cmd += std::to_string(cpu_no);
			cmd += ")\\% Processor Time";
			return cmd;
			};

		auto cmd = callback_getQueryCommand(cpu_no);
		double res{};
		WinPerfmonQuery::QueryForDoubleResult(cmd, &res, 1000u);
		return res;
#else
		std::ifstream file("/proc/stat");
		std::string line;

		if (file.is_open()) {
			std::string key = "cpu";
			key += std::to_string(cpu_no);
			float usage = 0.0f;
			while (getline(file, line)) {
				auto begin_index = line.find("cpu");
				// break if line is not start with cpu
				if (begin_index == std::string::npos) { break; }

				if (line.find(key) == 0) {
					usage = read_cpu_usage(line);
					break;
				}
			}
			file.close();
			return usage;
		} else {
			return 0.0f;
		}
#endif
	}
	std::vector<double> ResourceMonitor::GetAllProcessorUsage() {
		std::vector<double> result{};
#ifdef __WINDOWS_PLATFORM__
		auto count = GetProcessorCount();
		if (!count) { return result; }
		result.reserve(count);

		std::vector<std::future<double>> tasks{};
		for (auto i = 0; i < count; ++i) {
			tasks.push_back(std::async(std::launch::async, GetProcessorUsage, i));
		}
		for (auto& task : tasks) {
			result.push_back(task.get());
		}
#else
		std::ifstream file("/proc/stat");
		std::string line;
		if (file.is_open()) {
			while (getline(file, line)) {
				auto begin_index = line.find("cpu");
				// break if line is not start with cpu
				if (begin_index == std::string::npos) { break; }
				// pass the total usage
				if (line.find("cpu ") == 0) { continue; }
				result.push_back(read_cpu_usage(line));
			}
			file.close();
			return result;
		} else {
			return {};
		}
#endif
		return result;
	}
	MemoryStatus ResourceMonitor::GetMemoryStatus() {
		MemoryStatus mstat{};
		// both
		double phy_total{};
		// both
		double phy_avail{};
		// windows only
		double vir_total{};
		// windows only
		double vir_avail{};
		// unix only
		double swap_total{};
		// unix only
		double swap_avail{};

#ifdef __WINDOWS_PLATFORM__
		MEMORYSTATUSEX WinSysMemoryState{};
		MEMORYSTATUSEX* mem_stat_ptr = &WinSysMemoryState;

		memset(mem_stat_ptr, 0, sizeof(MEMORYSTATUSEX));
		WinSysMemoryState.dwLength = sizeof(MEMORYSTATUSEX);
		GlobalMemoryStatusEx(mem_stat_ptr);

		phy_total = static_cast<double>(WinSysMemoryState.ullTotalPhys);
		phy_avail = static_cast<double>(WinSysMemoryState.ullAvailPhys);
		vir_total = static_cast<double>(WinSysMemoryState.ullTotalPageFile);
		vir_avail = static_cast<double>(WinSysMemoryState.ullAvailPageFile);
#else

		std::ifstream file("/proc/meminfo");
		std::string line;
		if (file.is_open()) {
			while (std::getline(file, line)) {
				std::istringstream ss(line);
				std::string key;
				long value{};
				std::string unit;
				ss >> key >> value >> unit;
				if (key == "MemTotal:") {
					phy_total = UnitConvert::GetRatioToByte(unit) * value;
				} else if (key == "MemAvailable:") {
					phy_avail = UnitConvert::GetRatioToByte(unit) * value;
				} else if (key == "SwapTotal:") {
					swap_total = UnitConvert::GetRatioToByte(unit) * value;
				} else if (key == "SwapFree:") {
					swap_avail = UnitConvert::GetRatioToByte(unit) * value;
				}
			}
			file.close();
		} else {
			return mstat;
		}
		vir_total = phy_total + swap_total;
		vir_avail = phy_avail + swap_avail;
#endif
		mstat.Physical.total = phy_total;
		mstat.Physical.avail = phy_avail;
		mstat.Pagefile.total = vir_total;
		mstat.Pagefile.avail = vir_avail;
		return mstat;
	}
};