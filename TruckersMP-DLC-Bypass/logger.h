#pragma once

#include <string>
#include <fstream>
#include <chrono>
#include <mutex>

enum LogType
{
	INFO=0,
	WARN=1,
	ERR=2,
};

class Logger
{
	static Logger* instance;
	static std::mutex instance_mutex;

	std::fstream file;
	static std::mutex file_mutex;

	std::chrono::milliseconds start_time;
	static std::mutex start_time_mutex;

	bool console_created = false;
	static std::mutex console_created_mutex;
public:
	Logger		(std::wstring log_folder, std::wstring log_filename, bool create_console, const char* console_title);
	~Logger		();

	static Logger* get() {
		return instance;
	};
	
	void info	(const char* module_name, const char* format, ...);
	void warn	(const char* module_name, const char* format, ...);
	void error	(const char* module_name, const char* format, ...);

	// optional logging method instead of info(), warn() or error()
	void log	(LogType type, const char* module_name, const char* format, ...);
};