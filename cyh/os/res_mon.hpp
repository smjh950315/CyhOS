#pragma once
#include "os_.hpp"
namespace cyh::os {
	class ResourceMonitor {
	public:
		// Get clock Hz of Processor ( Unfinished )
		static double GetCpuClock();
		static long GetProcessorCount();
		static double GetProcessorUsage(uint cpu_no);
		static double GetLogicDiskUsage(const char* disk_label, uint physical_no = ~uint());
		
		//static std::vector<DiskAbstract> GetDiskInfoAbs();
		// 
		// get disk no such as C:,D:...
		static std::vector<std::string> GetLogicDiskNos();
		static std::vector<double> GetAllProcessorUsage();
		static std::vector<LogicDiskInformation> GetAllLogicDiskInfo();
		static MemoryStatus GetMemoryStatus();
	};
};