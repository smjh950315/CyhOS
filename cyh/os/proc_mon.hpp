#pragma once
#include "os_.hpp"
namespace cyh::os {
	class ProcessMonitor {
	public:
		static std::vector<uint> GetPIDs(const char* name);
		static std::vector<uint> GetPIDList();

		static ProcessAbstract GetProcessAbstract(uint pid);
		static ProcessDetails GetProcessDetail(uint pid);
		static std::string GetProcessName(uint pid);
		static double GetProcessCpuTime(uint pid);
		static std::vector<ProcessAbstract> GetProcessAbstracts(std::vector<std::string>* pmsgs = nullptr);
		static std::vector<ProcessDetails> GetProcessDetails(std::vector<std::string>* pmsgs = nullptr);
		static bool ForceKillProcess(uint pid);
	};
};