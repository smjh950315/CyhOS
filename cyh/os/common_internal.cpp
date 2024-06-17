#include "common_internal.hpp"
#ifdef __WINDOWS_PLATFORM__
#include <strsafe.h>
// WinPerfmonQuery
namespace cyh::os {
	WinPerfmonQuery::_queryArgs::~_queryArgs() {
		if (this->m_query) {
			PdhCloseQuery(this->m_query);
		}
	}
	bool WinPerfmonQuery::__on_query(_queryArgs* pquery, int format_counter_enum, const char* queryStr, uint wait_millis, std::vector<std::string>* pmsgs) {
		struct _MsgListHolder {
			std::vector<std::string>* m_ptr_msgs{};
			_MsgListHolder(std::vector<std::string>* p_msgs) : m_ptr_msgs(p_msgs) {}
			void log(const char* msg) {
				if (!this->m_ptr_msgs || !msg) { return; }
				this->m_ptr_msgs->push_back(msg);
			}
		} msg{ pmsgs };

		if (!pquery) {
			msg.log("Query object not initialized.");
			return false;
		}

		if (PdhOpenQuery(NULL, 0, &pquery->m_query) != ERROR_SUCCESS) {
			msg.log("PdhOpenQuery failed.");
			pquery->m_query = 0;
			return false;
		}
		if (PdhAddCounter(pquery->m_query, TEXT(queryStr), 0, &pquery->m_counter) != ERROR_SUCCESS) {
			msg.log("PdhAddCounter failed.");
			return false;
		}
		if (PdhCollectQueryData(pquery->m_query) != ERROR_SUCCESS) {
			msg.log("PdhCollectQueryData failed.");
			return false;
		}
		if (wait_millis) {
			Sleep(wait_millis);
			if (PdhCollectQueryData(pquery->m_query) != ERROR_SUCCESS) {
				msg.log("PdhCollectQueryData failed.");
				return false;
			}
		}
		if (PdhGetFormattedCounterValue(pquery->m_counter, format_counter_enum, NULL, &pquery->m_counterVal) != ERROR_SUCCESS) {
			msg.log("PdhGetFormattedCounterValue failed.");
			return false;
		}
		return true;
	}
	bool WinPerfmonQuery::__on_query_string_result(_queryArgs* pquery, int format_counter_enum, const char* queryStr, uint wait_millis, std::vector<std::string>* pmsgs) {
		struct _MsgListHolder {
			std::vector<std::string>* m_ptr_msgs{};
			_MsgListHolder(std::vector<std::string>* p_msgs) : m_ptr_msgs(p_msgs) {}
			void log(const char* msg) {
				if (!this->m_ptr_msgs || !msg) { return; }
				this->m_ptr_msgs->push_back(msg);
			}
		} msg{ pmsgs };

		if (!pquery) {
			msg.log("Query object not initialized.");
			return false;
		}

		if (PdhOpenQuery(NULL, 0, &pquery->m_query) != ERROR_SUCCESS) {
			msg.log("PdhOpenQuery failed.");
			pquery->m_query = 0;
			return false;
		}
		if (PdhAddCounter(pquery->m_query, TEXT(queryStr), 0, &pquery->m_counter) != ERROR_SUCCESS) {
			msg.log("PdhAddCounter failed.");
			return false;
		}
		if (PdhCollectQueryData(pquery->m_query) != ERROR_SUCCESS) {
			msg.log("PdhCollectQueryData failed.");
			return false;
		}
		if (wait_millis) {
			Sleep(wait_millis);
			if (PdhCollectQueryData(pquery->m_query) != ERROR_SUCCESS) {
				msg.log("PdhCollectQueryData failed.");
				return false;
			}
		}
		PDH_STATUS pdhStatus;
		DWORD bufferSize = 0;
		DWORD instanceCount = 0;

		// Get the buffer size needed for the counter information
		pdhStatus = PdhGetRawCounterArray(pquery->m_counter, &bufferSize, &instanceCount, NULL);
		if (pdhStatus != PDH_MORE_DATA) {
			std::wcerr << L"PdhGetRawCounterArray failed with status: " << pdhStatus << std::endl;
			return false;
		}
		// Allocate buffer and get the raw counter array
		std::vector<PDH_RAW_COUNTER_ITEM> counterItems(instanceCount);
		pdhStatus = PdhGetRawCounterArray(pquery->m_counter, &bufferSize, &instanceCount, counterItems.data());
		if (pdhStatus != ERROR_SUCCESS) {
			std::wcerr << L"PdhGetRawCounterArray failed with status: " << pdhStatus << std::endl;
			return false;
		}
		throw std::exception("unfinished!");
		return true;
	}
	bool WinPerfmonQuery::QueryForDoubleResult(const std::string& queryStr, double* output, uint wait_millis, std::vector<std::string>* pmsgs) {
		if (!output) { return false; }
		_queryArgs query{};
		if (__on_query(&query, PDH_FMT_DOUBLE, queryStr.c_str(), wait_millis, pmsgs)) {
			*output = query.m_counterVal.doubleValue;
			return true;
		}
		return false;
	}
	bool WinPerfmonQuery::QueryForLongResult(const std::string& queryStr, long* output, uint wait_millis, std::vector<std::string>* pmsgs) {
		if (!output) { return false; }
		_queryArgs query{};
		if (__on_query(&query, PDH_FMT_LONG, queryStr.c_str(), wait_millis, pmsgs)) {
			*output = query.m_counterVal.longValue;
			return true;
		}
		return false;
	}
	bool WinPerfmonQuery::QueryForStringResult(const std::string& queryStr, std::string* output, uint wait_millis, std::vector<std::string>* pmsgs) {
		if (!output) { return false; }
		_queryArgs query{};
		if (__on_query_string_result(&query, PDH_FMT_ANSI, queryStr.c_str(), wait_millis, pmsgs)) {
			*output = query.m_counterVal.AnsiStringValue;
			return true;
		}
		return false;
	}
};
// Win32Debug
namespace cyh::os {
	void Win32Debug::ErrorExit(const char* error_item_name) {
		// Retrieve the system error message for the last-error code

		LPVOID lpMsgBuf{};
		LPVOID lpDisplayBuf;
		DWORD dw = GetLastError();

		FormatMessage(
			FORMAT_MESSAGE_ALLOCATE_BUFFER |
			FORMAT_MESSAGE_FROM_SYSTEM |
			FORMAT_MESSAGE_IGNORE_INSERTS,
			NULL,
			dw,
			MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
			(LPTSTR)&lpMsgBuf,
			0, NULL);

		// Display the error message and exit the process

		lpDisplayBuf = (LPVOID)LocalAlloc(LMEM_ZEROINIT,
			(lstrlen((LPCTSTR)lpMsgBuf) + lstrlen((LPCTSTR)error_item_name) + 40) * sizeof(TCHAR));
		if (!lpDisplayBuf) {
			std::cerr << "failed on allocating message buffer" << std::endl;
		}
		StringCchPrintf((LPTSTR)lpDisplayBuf,
			LocalSize(lpDisplayBuf) / sizeof(TCHAR),
			TEXT("%s failed with error %d: %s"),
			error_item_name, dw, lpMsgBuf);
		MessageBox(NULL, (LPCTSTR)lpDisplayBuf, TEXT("Error"), MB_OK);

		LocalFree(lpMsgBuf);
		LocalFree(lpDisplayBuf);
		ExitProcess(dw);
	}
};
#else
#include "res_mon.hpp"
namespace cyh::os {
	void UnixInfoParser::read_unix_disk_info(_unixDiskInfo* pInfo, const std::string& rawStr) {
		if (!pInfo) { return; }
		_unixDiskInfo& diskUsage = *pInfo;
		std::istringstream ss(rawStr);
		ss >> diskUsage.major >> diskUsage.minor >> diskUsage.device >> diskUsage.reads >> diskUsage.readMerges >> diskUsage.readSectors >> diskUsage.readTicks >> diskUsage.writes >> diskUsage.writeMerges >> diskUsage.writeSectors >> diskUsage.writeTicks >> diskUsage.inFlight >> diskUsage.ioTicks >> diskUsage.timeInQueue;
	}
	void UnixInfoParser::read_unix_cpu_info(_unixCpuInfo* pInfo, const std::string& rawStr) {
		if (!pInfo) { return; }
		_unixCpuInfo& cpuUsage = *pInfo;
		std::istringstream ss(rawStr);
		std::string _cpu;
		ss >> _cpu >> cpuUsage.user >> cpuUsage.nice >> cpuUsage.system >> cpuUsage.idle >> cpuUsage.iowait >> cpuUsage.irq >> cpuUsage.softirq >> cpuUsage.steal;
	}

	_unixDiskInfo UnixInfoParser::read_disk_info(const std::string& disk_label) {
		_unixDiskInfo result{};
		std::ifstream diskstats("/proc/diskstats");
		std::string line;
		if (diskstats.is_open()) {
			while (std::getline(diskstats, line)) {
				if (line.find(disk_label) != std::string::npos) {
					read_unix_disk_info(&result, line);
					break;
				}
			}
			diskstats.close();
		}
		return result;
	}
	std::vector<_unixDiskInfo> UnixInfoParser::read_disk_infos() {
		std::vector<_unixDiskInfo> result{};

		std::ifstream diskstats("/proc/diskstats");
		std::string line;
		if (diskstats.is_open()) {
			while (std::getline(diskstats, line)) {
				_unixDiskInfo diskUsage{};
				read_unix_disk_info(&diskUsage, line);
				result.push_back(diskUsage);
			}
		}

		return result;
	}
	double UnixInfoParser::calculate_disk_usage(_unixDiskInfo* pInfo1, _unixDiskInfo* pInfo2) {
		long delta_total_time = pInfo1->total_time() - pInfo2->total_time();
		long delta_idle_time = pInfo1->idle_time() - pInfo2->idle_time();
		return static_cast<double>(delta_total_time - delta_idle_time) / static_cast<double>(delta_total_time) * 100.0;
	}

	_unixCpuInfo UnixInfoParser::read_cpu_info(uint cpu_no) {
		auto cpu_count = ResourceMonitor::GetProcessorCount();

		_unixCpuInfo result{};
		std::ifstream file("/proc/stat");
		std::string line;

		if (file.is_open()) {
			std::string key = "cpu";
			key += std::to_string(cpu_no);
			while (getline(file, line)) {
				auto begin_index = line.find("cpu");
				// break if line is not start with cpu
				if (begin_index == std::string::npos) {
					break;
				}
				// read info of specific cpu_no only
				if (line.find(key) == 0) {
					_unixCpuInfo cpuUsage{};
					read_unix_cpu_info(&cpuUsage, line);
					break;
				}
			}
			file.close();
		}
		return result;
	}
	std::vector<_unixCpuInfo> UnixInfoParser::read_cpus_info() {
		auto cpu_count = ResourceMonitor::GetProcessorCount();

		std::vector<_unixCpuInfo> result{};
		std::ifstream file("/proc/stat");
		std::string line;
		uint cpu_no = 0;

		if (file.is_open()) {
			std::string key = "cpu";
			key += std::to_string(cpu_no);
			while (getline(file, line)) {
				auto begin_index = line.find("cpu");
				// break if line is not start with cpu
				if (begin_index == std::string::npos) {
					break;
				}
				// read info of specific cpu_no only
				if (line.find(key) == 0) {
					_unixCpuInfo cpuUsage{};
					read_unix_cpu_info(&cpuUsage, line);
					result.push_back(cpuUsage);
					++cpu_no;
				}
			}
			file.close();
		}
		return result;
	}
	double UnixInfoParser::calculate_cpu_usage(_unixCpuInfo* pInfo1, _unixCpuInfo* pInfo2) {
		long delta_total_time = pInfo2->total_time() - pInfo1->total_time();
		long delta_idle_time = pInfo2->idle_time() - pInfo1->idle_time();
		return static_cast<double>(delta_total_time - delta_idle_time) / static_cast<double>(delta_total_time) * 100.0;
	}
};
#endif
namespace cyh::os {
	double UnitConvert::GetRatioToByte(const std::string unit) {
		double result{ 1.0 };
		static auto callback_getRatio = [](char unitChar) {
			switch (unitChar)
			{
			case 'K': case'k':
				return 1024.0;
			case 'M': case 'm':
				return 1048576.0;
			case 'G': case 'g':
				return 1073741824.0;
			case 'T': case 't':
				return 1099511627776.0;
			case 'P': case 'p':
				return 1099511627776.0 * 1024.0;
			default:
				return 1.0;
			}
			};
		for (const auto& ch : unit) {
			result *= callback_getRatio(ch);
		}
		return result;
	}
};
