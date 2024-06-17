#pragma once
#include "os_.hpp"
#ifdef __WINDOWS_PLATFORM__
#include <Windows.h>
#include <pdh.h>
#include <pdhmsg.h>
#pragma comment(lib, "pdh.lib")
#include <thread>
#include <future>
#else
#include <fcntl.h>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <sys/mman.h>
#include <unistd.h>
#endif
namespace cyh::os {
#ifdef __WINDOWS_PLATFORM__
	class WinPerfmonQuery {
	protected:
		struct _queryArgs {
			PDH_HQUERY m_query{};
			PDH_HCOUNTER m_counter{};
			PDH_FMT_COUNTERVALUE m_counterVal{};
			~_queryArgs();
		};
		static bool __on_query(_queryArgs* pquery, int format_counter_enum, const char* queryStr, uint wait_millis = 100, std::vector<std::string>* pmsgs = nullptr);
		static bool __on_query_string_result(_queryArgs* pquery, int format_counter_enum, const char* queryStr, uint wait_millis = 100, std::vector<std::string>* pmsgs = nullptr);
	public:
		static bool QueryForDoubleResult(const std::string& queryStr, double* output, uint wait_millis = 100, std::vector<std::string>* pmsgs = nullptr);
		static bool QueryForLongResult(const std::string& queryStr, long* output, uint wait_millis = 100, std::vector<std::string>* pmsgs = nullptr);
		static bool QueryForStringResult(const std::string& queryStr, std::string* output, uint wait_millis = 100, std::vector<std::string>* pmsgs = nullptr);
	};

	struct Win32Debug {
		static void ErrorExit(const char* error_item_name);
	};
	struct _unixProcessInfo {
		long pid;
		std::string comm;
		std::string task_state;
		long ppid;
		long pgid;
		long sid;
		long tty_nr;
		long tty_pgrp;
		long flags;
		long min_flt;
		long cmin_flt;
		long maj_flt;
		long cmaj_flt;
		long utime;
		long stime;
		long priority;
		long nice;
		long num_threads;
		long it_real_value;
		long start_time;
		long vsize;
		long rss;
	};
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
		long total_time() {
			return this->readTicks + this->writeTicks;
		}
		long idle_time() {
			return this->ioTicks;
		}
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
		long total_time() {
			return this->user + this->nice + this->system + this->idle + this->iowait + this->irq + this->softirq + this->steal;
		}
		long idle_time() {
			return this->idle;
		}
	};
	struct UnixInfoParser {
		static void read_unix_disk_info(_unixDiskInfo* pInfo, const std::string& rawStr);
		static void read_unix_cpu_info(_unixCpuInfo* pInfo, const std::string& rawStr);
		static _unixDiskInfo read_disk_info(const std::string& disk_label);
		static std::vector<_unixDiskInfo> read_disk_infos();
		static double calculate_disk_usage(_unixDiskInfo* pInfo1, _unixDiskInfo* pInfo2);
		static _unixCpuInfo read_cpu_info(uint cpu_no);
		static std::vector<_unixCpuInfo> read_cpus_info();
		static double calculate_cpu_usage(_unixCpuInfo* pInfo1, _unixCpuInfo* pInfo2);
	};
#endif
	struct UnitConvert {
		static double GetRatioToByte(const std::string unit);
	};
};
