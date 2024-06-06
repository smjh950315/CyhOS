#pragma once
#include "os_.hpp"
namespace cyh::os {
	class ResourceMonitor {
	public:
		static double GetCpuClock();

		static long GetProcessorCount();
		static double GetProcessorUsage(uint cpu_no);
		static std::vector<double> GetAllProcessorUsage();
		static MemoryStatus GetMemoryStatus();
	};
};