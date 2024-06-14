#include "res_mon.hpp"
#include "common_internal.hpp"
#include <cstring>
#include <string>
#ifndef __WINDOWS_PLATFORM__
#include <future>
#endif
namespace cyh::os {
#ifdef __WINDOWS_PLATFORM__
	static std::string get_diskUsage_queryString(const char* disk_label, uint physical_no = ~uint()) {
		std::string queryStr = "\\PhysicalDisk(";
		if (physical_no == ~uint()) {
			queryStr += '*';
		} else {
			queryStr += std::to_string(physical_no);
		}
		queryStr += ' ';
		queryStr += disk_label;
		queryStr += ':';
		queryStr += ")\\% Idle Time";
		return queryStr;
	}
#else
	struct _unixDiskInfo {
		long major;
		long minor;
		std::string device;
		long reads;
		long readMerges;
		long readSectors;
		long readTicks;
		long writes;
		long writeMerges;
		long writeSectors;
		long writeTicks;
		long inFlight;
		long ioTicks;
		long timeInQueue;
	};
	struct _unixCpuInfo {
		long user;
		long nice;
		long system;
		long idle;
		long iowait;
		long irq;
		long softirq;
		long steal;
	};
	static void read_unix_disk_info(_unixDiskInfo* pInfo, const std::string& rawStr) {
		if (!pInfo) { return; }
		_unixDiskInfo& diskUsage = *pInfo;
		std::istringstream ss(rawStr);
		ss >> diskUsage.major >> diskUsage.minor >> diskUsage.device >> diskUsage.reads >> diskUsage.readMerges >> diskUsage.readSectors >> diskUsage.readTicks >> diskUsage.writes >> diskUsage.writeMerges >> diskUsage.writeSectors >> diskUsage.writeTicks >> diskUsage.inFlight >> diskUsage.ioTicks >> diskUsage.timeInQueue;
	}
	static void read_unix_cpu_info(_unixCpuInfo* pInfo, const std::string& rawStr) {
		if (!pInfo) { return; }
		_unixCpuInfo& cpuUsage = *pInfo;
		std::istringstream ss(rawStr);
		std::string _cpu;
		ss >> _cpu >> cpuUsage.user >> cpuUsage.nice >> cpuUsage.system >> cpuUsage.idle >> cpuUsage.iowait >> cpuUsage.irq >> cpuUsage.softirq >> cpuUsage.steal;
	}
	static float read_disk_usage(const std::string& rawStr) {
		_unixDiskInfo diskUsage1{};
		_unixDiskInfo diskUsage2{};
		static auto fn_get_percentage = [=] (_unixDiskInfo* pInfo1, _unixDiskInfo* pInfo2) {
			long ticks1 = pInfo1->readTicks + pInfo1->writeTicks;
			long ticks2 = pInfo2->readTicks + pInfo2->writeTicks;
			long deltaTicks = ticks2 - ticks1;
			long time = pInfo2->ioTicks - pInfo1->ioTicks;
			return static_cast<float>(static_cast<float>(deltaTicks) * 100.f / static_cast<float>(time));
		};
		return fn_get_percentage(&diskUsage1, &diskUsage2);
	}
	static float read_cpu_usage(const std::string& rawStr) {
		_unixCpuInfo cpuUsage{};
		static auto fn_get_percentage = [=] (_unixCpuInfo* pInfo) {
			long idle = pInfo->idle + pInfo->iowait;
			long nonIdle = pInfo->user + pInfo->nice + pInfo->system + pInfo->irq + pInfo->softirq + pInfo->steal;
			long total = idle + nonIdle;
			return static_cast<float>(static_cast<float>(total - idle) * 100.f / static_cast<float>(total));
		};
		read_unix_cpu_info(&cpuUsage, rawStr);
		return fn_get_percentage(&cpuUsage);
	}
#endif

	double ResourceMonitor::GetCpuClock() {
		static double result{};
		if (result != 0.0) {
			return result;
		}
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
				if (begin_index == std::string::npos) {
					break;
				}
				// pass first line
				if (line.find("cpu ") == 0) {
					continue;
				}
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
		static auto callback_getQueryCommand = [=] (uint cpu_no) {
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
				if (begin_index == std::string::npos) {
					break;
				}

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
	double ResourceMonitor::GetLogicDiskUsage(const char* disk_label, uint physical_no) {
		double result{};
#ifdef __WINDOWS_PLATFORM__
		std::string queryStr = get_diskUsage_queryString(disk_label, physical_no);
		WinPerfmonQuery::QueryForDoubleResult(queryStr, &result, 1000u);
		result = 100.0 - result;
#else
		_unixDiskInfo diskUsage1{};
		_unixDiskInfo diskUsage2{};
		static auto fn_get_percentage = [=] (_unixDiskInfo* pInfo1, _unixDiskInfo* pInfo2) {
			long ticks1 = pInfo1->readTicks + pInfo1->writeTicks;
			long ticks2 = pInfo2->readTicks + pInfo2->writeTicks;
			long deltaTicks = ticks2 - ticks1;
			long time = pInfo2->ioTicks - pInfo1->ioTicks;
			return static_cast<float>(static_cast<float>(deltaTicks) * 100.f / static_cast<float>(time));
		};
		std::ifstream diskstats1("/proc/diskstats");
		std::string line;
		bool found = false;
		while (std::getline(diskstats1, line)) {
			if (line.find(disk_label) != std::string::npos) {
				read_unix_disk_info(&diskUsage1, line);
				found = true;
				break;
			}
		}
		diskstats1.close();
		sleep(1);
		if (!found) { return 0.0; }
		std::ifstream diskstats2("/proc/diskstats");
		while (std::getline(diskstats1, line)) {
			if (line.find(disk_label) != std::string::npos) {
				read_unix_disk_info(&diskUsage1, line);
				found = true;
				break;
			}
		}
		diskstats2.close();
#endif
		return result;
	}
	//	std::vector<DiskAbstract> ResourceMonitor::GetDiskInfoAbs() {
	//		std::vector<DiskAbstract> result{};
	// #ifdef __WINDOWS_PLATFORM__
	//		auto drives = GetLogicalDrives();
	// #else
	//
	// #endif
	//		return result;
	//	}
	std::vector<std::string> ResourceMonitor::GetLogicDiskNos() {
		std::vector<std::string> result;
#ifdef __WINDOWS_PLATFORM__
		auto drives = GetLogicalDrives();
		std::string _labeltext = "A:\\";
		char* labeltext = _labeltext.data();
		char& clabel = labeltext[0];
		int count = 0;
		for (char current = 'A'; current < 'Z'; ++current) {
			clabel = current;
			if (drives & 0x1) {
				if (GetDriveTypeA(labeltext) == DRIVE_FIXED) {
					result.push_back(_labeltext.substr(0, 2));
				}
			}
			drives >>= 1;
		}
#else
		std::ifstream diskstats("/proc/diskstats");
		std::string line;
		_unixDiskInfo diskUsage{};
		while (std::getline(diskstats, line)) {
			read_unix_disk_info(&diskUsage, line);
			result.push_back(diskUsage.device);
		}
#endif
		return result;
	}
	std::vector<double> ResourceMonitor::GetAllProcessorUsage() {
		std::vector<double> result{};
#ifdef __WINDOWS_PLATFORM__
		auto count = GetProcessorCount();
		if (!count) {
			return result;
		}
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
				if (begin_index == std::string::npos) {
					break;
				}
				// pass the total usage
				if (line.find("cpu ") == 0) {
					continue;
				}
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
	std::vector<double> ResourceMonitor::GetAllLogicDiskUsage() {
		auto labels = GetLogicDiskNos();
		std::vector<double> result{};
		result.resize(labels.size());
		std::vector<std::future<double>> tasks{};
		for (auto& label : labels) {
			std::cout << label << std::endl;
			tasks.push_back(std::async(std::launch::async, GetLogicDiskUsage, label.c_str(), ~uint()));
		}
		for (auto& task : tasks) {
			result.push_back(task.get());
		}
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