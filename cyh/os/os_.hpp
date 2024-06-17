#pragma once
#include <iostream>
#include <vector>
#include <type_traits>
#if defined(_WIN32) || defined(_WIN64)
#define __WINDOWS_PLATFORM__
#else
#endif

#define UNSUPPORT_UNIX

namespace cyh::os {
	using uint = unsigned int;
	using nint = std::make_signed_t<size_t>;
	using nuint = size_t;

	struct MemoryStatus {
		struct _details {
			double total{};
			double avail{};
		} Physical, Pagefile;
	};
	struct ProcessInformation {
		uint pid{};
		std::string name;
		std::string path;
		nuint memory{};
		nuint kernal_time{};
		nuint user_time{};
		double cpu_time_percentage{};
	};
	struct LogicDiskInformation {
		std::string mount_or_label;
		double io_time_percentage{};
	};
	struct ProcessGroup {
		std::string name;
		std::string path;
		double cpu_time_percentage{};
		nuint memory{};
		std::vector<ProcessInformation> sub_procs;
	};
};
