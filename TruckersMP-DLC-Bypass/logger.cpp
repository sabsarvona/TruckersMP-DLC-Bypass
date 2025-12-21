/*----------------------------------------*/
// Project	: TruckersMP DLC Bypass
// File		: logger.cpp
// 
// Author	: Baldywaldy09 | Hunter
// Created	: 13-12-2025
// Updated	: 21-12-2025
/*-----------------------------------------*/

#include "logger.h"

#include <Windows.h>
#include <filesystem>
#include <cstdarg>
namespace fs = std::filesystem;
using namespace std::chrono;


Logger* Logger::instance = nullptr;

std::mutex Logger::instance_mutex;
std::condition_variable Logger::instance_cv;
std::mutex Logger::file_mutex;
std::mutex Logger::start_time_mutex;
std::mutex Logger::console_created_mutex;

milliseconds get_current_time_ms()
{
	return duration_cast<milliseconds>(system_clock::now().time_since_epoch());
}

std::string get_time_since_start_string(milliseconds start_time)
{
	milliseconds current_time = get_current_time_ms();
	milliseconds elapsed = current_time - start_time;

	hours hr = duration_cast<hours>(elapsed);
	minutes min = duration_cast<minutes>(elapsed % 1h);
	seconds sec = duration_cast<seconds>(elapsed % 1min);
	milliseconds ms = duration_cast<milliseconds>(elapsed % 1s);

	char buffer[84];
	sprintf_s(buffer, 84, "%02lld:%02lld:%02lld.%03lld",
		static_cast<long long>(hr.count()),
		static_cast<long long>(min.count()),
		static_cast<long long>(sec.count()),
		static_cast<long long>(ms.count())
	);

	return std::string(buffer);
}

std::string get_current_datetime_string()
{
	time_point now = system_clock::now();
	std::time_t now_c = system_clock::to_time_t(now);
	
	tm time;
	localtime_s(&time, &now_c);

	char buffer[84]; 
	std::strftime(buffer, sizeof(buffer), "%A %B %d %Y @ %H:%M:%S", &time);
	return std::string(buffer);
}


enum ConsoleColour {
	RED,
	YELLOW,
	DEFAULT
};

void set_colour(ConsoleColour colour) {
	HANDLE hConsole = GetStdHandle(STD_OUTPUT_HANDLE);

	switch (colour) {
	case RED:
		SetConsoleTextAttribute(hConsole, FOREGROUND_RED | FOREGROUND_INTENSITY);
		break;
	case YELLOW:
		SetConsoleTextAttribute(hConsole, FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_INTENSITY);
		break;
	case DEFAULT:
		SetConsoleTextAttribute(hConsole, FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE);
		break;
	}
}


Logger::Logger(std::wstring log_folder, std::wstring log_filename, bool create_console, const char* console_title)
{
	std::lock_guard<std::mutex> instance_lock(Logger::instance_mutex);
	std::lock_guard<std::mutex> file_lock(Logger::file_mutex);
	std::lock_guard<std::mutex> console_lock(Logger::console_created_mutex);
	std::lock_guard<std::mutex> time_lock(Logger::start_time_mutex);

	if (Logger::instance != nullptr)
	{
		throw std::runtime_error("Logger instance already exists!");
	}

	fs::path folder(log_folder);

	if (!folder.empty())
		fs::create_directories(folder);
	else
		folder = fs::current_path();

	fs::path file = folder / log_filename;

	Logger::file = std::fstream(file, std::ios::out | std::ios::trunc);

	if (!Logger::file.is_open())
	{
		throw std::runtime_error("Failed to open log file.");
	}

	if (create_console)
	{
		AllocConsole();
		freopen_s((FILE**)stdin, "CONIN$", "r", stdin);
		freopen_s((FILE**)stdout, "CONOUT$", "w", stdout);
		freopen_s((FILE**)stderr, "CONOUT$", "w", stderr);

		SetConsoleTitleA(console_title);

		Logger::console_created = true;
	}

	Logger::instance = this;

	Logger::start_time = get_current_time_ms();
	Logger::file << "************ : log created on : " << get_current_datetime_string() << "\n";

	Logger::instance_cv.notify_all();
}

Logger::~Logger()
{
	std::lock_guard<std::mutex> console_lock(Logger::console_created_mutex);
	std::lock_guard<std::mutex> file_lock(Logger::file_mutex);

	if (Logger::console_created)
	{
		FreeConsole();
	}

	if (Logger::file.is_open())
	{
		Logger::file.close();
	}
}

void Logger::info(const char* module_name, const char* format, ...)
{
	if (!this)
		return;

	std::lock_guard<std::mutex> time_lock(Logger::start_time_mutex);
	std::lock_guard<std::mutex> file_lock(Logger::file_mutex);

	va_list args;
	va_start(args, format);
	char buffer[2048];
	vsnprintf_s(buffer, sizeof(buffer), format, args);
	va_end(args);

	char buffer_final[2048];
	sprintf_s(buffer_final, 512, "%s : [%s] %s\n", get_time_since_start_string(Logger::start_time).c_str(), module_name, buffer);

	set_colour(ConsoleColour::DEFAULT);
	printf("%s", buffer_final);

	Logger::file << buffer_final;
	Logger::file.flush();
}

void Logger::warn(const char* module_name, const char* format, ...)
{
	if (!this)
		return;

	std::lock_guard<std::mutex> time_lock(Logger::start_time_mutex);
	std::lock_guard<std::mutex> file_lock(Logger::file_mutex);

	va_list args;
	va_start(args, format);
	char buffer[2048];
	vsnprintf_s(buffer, sizeof(buffer), format, args);
	va_end(args);

	char buffer_final[2048];
	sprintf_s(buffer_final, 512, "%s : <WARNING> [%s] %s\n", get_time_since_start_string(Logger::start_time).c_str(), module_name, buffer);

	set_colour(ConsoleColour::YELLOW);
	printf("%s", buffer_final);

	Logger::file << buffer_final;
	Logger::file.flush();
}

void Logger::error(const char* module_name, const char* format, ...)
{
	if (!this)
		return;

	std::lock_guard<std::mutex> time_lock(Logger::start_time_mutex);
	std::lock_guard<std::mutex> file_lock(Logger::file_mutex);

	va_list args;
	va_start(args, format);
	char buffer[2048];
	vsnprintf_s(buffer, sizeof(buffer), format, args);
	va_end(args);

	char buffer_final[2048];
	sprintf_s(buffer_final, 512, "%s : <ERROR> [%s] %s\n", get_time_since_start_string(Logger::start_time).c_str(), module_name, buffer);

	set_colour(ConsoleColour::RED);
	printf("%s", buffer_final);

	Logger::file << buffer_final;
	Logger::file.flush();
}

void Logger::log(LogType type, const char* module_name, const char* format, ...)
{
	if (!this)
		return;

	va_list args;
	va_start(args, format);
	char buffer[2048];
	vsnprintf_s(buffer, sizeof(buffer), format, args);
	va_end(args);

	switch (type)
	{
	case INFO:
		Logger::info(module_name, buffer);
		break;
	case WARN:
		Logger::warn(module_name, buffer);
		break;
	case ERR:
		Logger::error(module_name, buffer);
		break;
	}
}