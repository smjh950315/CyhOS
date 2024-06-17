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
	struct GlobalVariables {
		static uint ProbingTime;
	};
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
	struct _unixProcStat {
		// (1) Process ID
		long pid;
		// (2) Executable filename
		//std::string comm;
		// (3) R: running, S: sleeping, D: disk sleep, T: stopped, Z: zombie, X: dead
		//std::string state;
		// (4) The PID of the parent of this process
		long ppid;
		// (5) The process group ID of the process
		long pgid;
		// (6) The session ID of the process
		long sid;
		// (7) The controlling terminal of the process
		long tty_nr;
		// (8) The ID of the foreground process group of the controlling terminal of the process
		long tty_pgrp;
		// (9) The kernel flags word of the process
		long flags;
		// (10) The number of minor faults the process has made which have not required loading a memory page from disk
		long min_flt;
		// (11) The number of minor faults that the process's waited-for children have made
		long cmin_flt;
		// (12) The number of major faults the process has made which have required loading a memory page from disk
		long maj_flt;
		// (13) The number of major faults that the process's waited-for children have made
		long cmaj_flt;
		// (14) Amount of time that this process has been scheduled in user mode, measured in clock ticks (divide by sysconf(_SC_CLK_TCK))
		long utime;
		// (15) Amount of time that this process has been scheduled in kernel mode, measured in clock ticks (divide by sysconf(_SC_CLK_TCK))
		long stime;
		// (16) Amount of time that this process's waited-for children have been scheduled in user mode, measured in clock ticks (divide by sysconf(_SC_CLK_TCK))
		long cutime;
		// (17) Amount of time that this process's waited-for children have been scheduled in kernel mode, measured in clock ticks (divide by sysconf(_SC_CLK_TCK))
		long cstime;
		// (18) For processes running a real-time scheduling policy, this is the negated scheduling priority, minus one; that is, a number in the
		// range -2 to -100, corresponding to real-time priorities 1 to 99
		long priority;
		// (19) The nice value (see setpriority(2)), a value in the range 19 (low priority) to -20 (high priority)
		long nice;
		// (20) Number of threads in this process (since Linux 2.6). Before kernel 2.6, this field was hard coded to 0 as a placeholder for an earlier removed field
		long num_threads;
		// (21) The time in jiffies before the next SIGALRM is sent to the process due to an interval timer
		long it_real_value;
		// (22) The time the process started after system boot
		long start_time;
		// (23) Virtual memory size in bytes
		long vsize;
		// (24) Resident Set Size: number of pages the process has in real memory
		long rss;
		// (25) Current soft limit in bytes on the rss of the process
		long rsslim;
		long total_cpu_time() const {
			return this->stime + this->utime + this->cstime + this->cutime;
		}
	};
	struct UnixInfoParser {
		static void read_unix_disk_info(_unixDiskInfo* pInfo, const std::string& rawStr);
		static void read_unix_cpu_info(_unixCpuInfo* pInfo, const std::string& rawStr);
		static void read_unix_proc_info(_unixProcStat* pInfo, const std::string& rawStr);

		// read [/proc/diskstats]
		static _unixDiskInfo read_disk_info(const std::string& disk_label);
		// read [/proc/diskstats]
		static std::vector<_unixDiskInfo> read_disks_info();
		static double calculate_disk_usage(_unixDiskInfo* pInfo1, _unixDiskInfo* pInfo2);

		// read [/proc/stat]
		static _unixCpuInfo read_total_cpu_info();
		// read [/proc/stat]
		static _unixCpuInfo read_cpu_info(uint cpu_no);
		// read [/proc/stat]
		static std::vector<_unixCpuInfo> read_cpus_info();
		static double calculate_cpu_usage(_unixCpuInfo* pInfo1, _unixCpuInfo* pInfo2);

		// read [/proc/pid/stat]
		static _unixProcStat read_proc_stat(uint pid);
		static std::vector<_unixProcStat> read_procs_stat();
		static double calculate_proc_cpu_usage(_unixProcStat* pInfo1, _unixProcStat* pInfo2, _unixCpuInfo* pCInfo1, _unixCpuInfo* pCInfo2);
	};
#endif
	struct UnitConvert {
		static double GetRatioToByte(const std::string unit);
	};
};
