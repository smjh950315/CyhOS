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
#endif
	struct UnitConvert {
		static double GetRatioToByte(const std::string unit);
	};
};
