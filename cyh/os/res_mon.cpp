#include "res_mon.hpp"
#include "os_internal.hpp"
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
		queryStr += ")\\% Idle Time";
		return queryStr;
	}
#endif
	static void get_logicDisk_usage_ref(const char* disk_label, uint physical_no, double* pUsage) {
		if (!pUsage ||!disk_label) { return; }
		*pUsage = ResourceMonitor::GetLogicDiskUsage(disk_label, physical_no);
	}
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
		result = static_cast<double>(sysconf(_SC_CLK_TCK)) / 100000.0;
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
		auto info0 = UnixInfoParser::read_cpu_info(cpu_no);
		std::this_thread::sleep_for(std::chrono::milliseconds(1000));
		auto info1 = UnixInfoParser::read_cpu_info(cpu_no);
		return UnixInfoParser::calculate_cpu_usage(&info0, &info1);
#endif
	}
	double ResourceMonitor::GetLogicDiskUsage(const char* disk_label, uint physical_no) {
		double result{};
#ifdef __WINDOWS_PLATFORM__
		std::string queryStr = get_diskUsage_queryString(disk_label, physical_no);
		WinPerfmonQuery::QueryForDoubleResult(queryStr, &result, 1000u);
		result = 100.0 - result;
#else
		_unixDiskInfo diskUsage1 = UnixInfoParser::read_disk_info(disk_label);
		std::this_thread::sleep_for(std::chrono::milliseconds(1000));
		_unixDiskInfo diskUsage2 = UnixInfoParser::read_disk_info(disk_label);
		result = UnixInfoParser::calculate_disk_usage(&diskUsage1, &diskUsage2);
#endif
		return result;
	}
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
			UnixInfoParser::read_unix_disk_info(&diskUsage, line);
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
		auto infos0 = UnixInfoParser::read_cpus_info();
		std::this_thread::sleep_for(std::chrono::milliseconds(1000));
		auto infos1 = UnixInfoParser::read_cpus_info();
		if (infos0.size() != infos1.size()) {
			return result;
		}
		auto pInfos0 = infos0.data();
		auto pInfos1 = infos1.data();
		for (nuint i = 0; i < infos0.size(); ++i) {
			result.push_back(UnixInfoParser::calculate_cpu_usage(pInfos0 + i, pInfos1 + i));
		}
#endif
		return result;
	}
	std::vector<LogicDiskInformation> ResourceMonitor::GetAllLogicDiskInfo() {
		std::vector<LogicDiskInformation> result{};	
#ifdef __WINDOWS_PLATFORM__
		auto labels = GetLogicDiskNos();
		auto diskCount = labels.size();
		std::vector<std::future<void>> tasks{};
		result.resize(diskCount);
		nuint currentIndex = 0;
		for (auto& label : labels) {
			auto& currentInfo = result[currentIndex];
			currentInfo.mount_or_label = label;
			tasks.push_back(std::async(std::launch::async, get_logicDisk_usage_ref, label.c_str(), ~uint(), &currentInfo.io_time_percentage));
			++currentIndex;
		}
		for (auto& task : tasks) {
			task.get();
		}
#else
		auto infos0 = UnixInfoParser::read_disks_info();
		std::this_thread::sleep_for(std::chrono::milliseconds(1000));
		auto infos1 = UnixInfoParser::read_disks_info();
		if (infos0.size() != infos1.size()) {
			return result;
		}
		result.resize(infos0.size());
		auto pInfos0 = infos0.data();
		auto pInfos1 = infos1.data();
		nuint currentIndex = 0;
		for (nuint i = 0; i < infos0.size(); ++i) {
			auto& currentInfo = result[currentIndex];
			currentInfo.mount_or_label = pInfos0[currentIndex].device;
			currentInfo.io_time_percentage = UnixInfoParser::calculate_disk_usage(pInfos0 + i, pInfos1 + i);
			++currentIndex;
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
