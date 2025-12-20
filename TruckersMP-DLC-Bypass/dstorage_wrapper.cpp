#include "dstorage_wrapper.h"

#include <Windows.h>
#include <filesystem>
#include <cstdarg>
#include <string>
#include <locale>
#include <codecvt>

#define DLL_EXPORT extern "C" __declspec( dllexport ) 

std::string TranslateHResult(HRESULT hr)
{
	LPVOID msgBuffer;
	DWORD size = FormatMessage(
		FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
		nullptr,
		hr,
		MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
		(LPWSTR)&msgBuffer,
		0,
		nullptr
	);

	if (size > 0)
	{
		int size_needed = WideCharToMultiByte(CP_UTF8, 0, (LPWSTR)msgBuffer, -1, nullptr, 0, nullptr, nullptr);

		std::string res(size_needed, 0);

		WideCharToMultiByte(CP_UTF8, 0, (LPWSTR)msgBuffer, -1, &res[0], size_needed, nullptr, nullptr);

		if (!res.empty() && res.back() == '\0')
			res.pop_back();

		if (!res.empty() && res.back() == '\n')
			res.pop_back();

		if (!res.empty() && res.back() == '\r')
			res.pop_back();

		LocalFree(msgBuffer);

		return res;
	}
	else
	{
		return "Unknown error";
	}
}

DLL_EXPORT HRESULT DStorageCreateCompressionCodec(DSTORAGE_COMPRESSION_FORMAT format, UINT32 numThreads, REFIID riid, void** ppv)
{
	Logger::get()->info("dstorage::DStorageCreateCompressionCodec", "Redirecting call...");

	if (DStorageWrapper::get_wait(std::chrono::seconds(2))) 
	{
		HRESULT res = DStorageWrapper::get()->DStorageCreateCompressionCodec(format, numThreads, riid, ppv);
		Logger::get()->info("dstorage::DStorageCreateCompressionCodec", "Redirected (%s)", TranslateHResult(res).c_str());
		return res;
	}
	else 
	{
		Logger::get()->error("dstorage::DStorageCreateCompressionCodec", "Failed to get DStorageWrapper after 2 seconds!");
		return E_FAIL;
	}
}

DLL_EXPORT HRESULT DStorageSetConfiguration1(DSTORAGE_CONFIGURATION1 const* configuration)
{
	Logger::get()->info("dstorage::DStorageSetConfiguration1", "Redirecting call...");

	if (DStorageWrapper::get_wait(std::chrono::seconds(2))) 
	{
		HRESULT res = DStorageWrapper::get()->DStorageSetConfiguration1(configuration);
		Logger::get()->info("dstorage::DStorageSetConfiguration1", "Redirected (%s)", TranslateHResult(res).c_str());
		return res;
	}
	else {
		Logger::get()->error("dstorage::DStorageSetConfiguration1", "Failed to get DStorageWrapper after 2 seconds!");
		return E_FAIL;
	}
}

DLL_EXPORT HRESULT DStorageSetConfiguration(DSTORAGE_CONFIGURATION const* configuration)
{
	Logger::get()->info("dstorage::DStorageSetConfiguration", "Redirecting call...");

	if (DStorageWrapper::get_wait(std::chrono::seconds(2))) 
	{
		HRESULT res = DStorageWrapper::get()->DStorageSetConfiguration(configuration);
		Logger::get()->info("dstorage::DStorageSetConfiguration", "Redirected (%s)", TranslateHResult(res).c_str());
		return res;
	}
	else 
	{
		Logger::get()->error("dstorage::DStorageSetConfiguration", "Failed to get DStorageWrapper after 2 seconds!");
		return E_FAIL;
	}
}

DLL_EXPORT HRESULT DStorageGetFactory(REFIID riid, void** ppv)
{
	Logger::get()->info("dstorage::DStorageGetFactory", "Redirecting call...");

	if (DStorageWrapper::get_wait(std::chrono::seconds(2))) 
	{
		HRESULT res = DStorageWrapper::get()->DStorageGetFactory(riid, ppv);
		Logger::get()->info("dstorage::DStorageGetFactory", "Redirected (%s)", TranslateHResult(res).c_str());
		return res;
	}
	else {
		Logger::get()->error("dstorage::DStorageGetFactory", "Failed to get DStorageWrapper after 2 seconds!");
		return E_FAIL;
	}
}




DStorageWrapper* DStorageWrapper::instance = nullptr;
std::mutex DStorageWrapper::instance_mutex;
std::condition_variable DStorageWrapper::instance_cv;
std::mutex DStorageWrapper::hmodule_mutex;

DStorageWrapper::DStorageWrapper(LPCSTR dllname)
{
	if (DStorageWrapper::instance)
	{
		throw std::runtime_error("DStorageWrapper instance already exists!");
	}

	std::lock_guard<std::mutex> module_lock(DStorageWrapper::hmodule_mutex);
	DStorageWrapper::hmodule = LoadLibraryA(dllname);

	if (!DStorageWrapper::hmodule)
	{
		std::string msg = "Failed to load " + std::string(dllname) + "!";

		throw std::runtime_error(msg);
	}

	std::lock_guard<std::mutex> instance_lock(DStorageWrapper::instance_mutex);
	DStorageWrapper::instance = this;
	DStorageWrapper::instance_cv.notify_all();
}

HRESULT DStorageWrapper::DStorageCreateCompressionCodec(DSTORAGE_COMPRESSION_FORMAT format, UINT32 numThreads, REFIID riid, void** ppv)
{
	using FuncType = HRESULT(__cdecl*)(DSTORAGE_COMPRESSION_FORMAT, UINT32, REFIID, void**);
	FuncType func = reinterpret_cast<FuncType>(GetProcAddress(DStorageWrapper::hmodule, "DStorageCreateCompressionCodec"));
	
	if (!func)
		return E_FAIL;

	return func(format, numThreads, riid, ppv);
}

HRESULT DStorageWrapper::DStorageSetConfiguration1(DSTORAGE_CONFIGURATION1 const* configuration)
{
	using FuncType = HRESULT(__cdecl*)(DSTORAGE_CONFIGURATION1 const*);
	FuncType func = reinterpret_cast<FuncType>(GetProcAddress(DStorageWrapper::hmodule, "DStorageSetConfiguration1"));
	
	if (!func)
		return E_FAIL;

	return func(configuration);
}

HRESULT DStorageWrapper::DStorageSetConfiguration(DSTORAGE_CONFIGURATION const* configuration)
{
	using FuncType = HRESULT(__cdecl*)(DSTORAGE_CONFIGURATION const*);
	FuncType func = reinterpret_cast<FuncType>(GetProcAddress(DStorageWrapper::hmodule, "DStorageSetConfiguration"));
	
	if (!func)
		return E_FAIL;

	return func(configuration);
}

HRESULT DStorageWrapper::DStorageGetFactory(REFIID riid, void** ppv)
{
	using FuncType = HRESULT(__cdecl*)(REFIID, void**);
	FuncType func = reinterpret_cast<FuncType>(GetProcAddress(DStorageWrapper::hmodule, "DStorageGetFactory"));
	
	if (!func)
		return E_FAIL;
	
	return func(riid, ppv);
}