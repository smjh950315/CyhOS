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
	struct ProcessDetails {
		uint pid{};
		std::string name;
		std::string path;
		nuint memory{};
		nuint kernal_time{};
		nuint user_time{};
		double cpu_time_percentage{};
	};
	struct ProcessAbstract {
		uint pid{};
		std::string name;
	};
	struct DiskAbstract {
		std::string physical{};
		std::string mount{};
	};
	struct DiskInfo {
		uint physical_no{};
		double idle_time{};
		std::string mount{};
	};
};
