#pragma once
#include "os_.hpp"
namespace cyh::os {
	class ProcessMonitor {
	public:
		static std::vector<uint> GetProcessIDs();
		static std::vector<uint> GetProcessIDs(const char* name);
		static ProcessInformation GetProcessInfo(uint pid, bool with_details = true);
		static std::vector<ProcessInformation> GetAllProcessInfo(bool with_details = true);

		static std::string GetProcessName(uint pid);
		static double GetProcessCpuTime(uint pid);
		static bool ForceKillProcess(uint pid);
		
		static std::vector<ProcessGroup> GetProcessGroups();
	};
};