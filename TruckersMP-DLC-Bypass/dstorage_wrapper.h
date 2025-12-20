/*----------------------------------------*/
// Project	: TruckersMP DLC Bypass
// File		: dstorage_wrapper.h
// 
// Author	: Baldywaldy09 | Hunter
// Created	: 13-12-2025
// Updated	: 20-12-2025
/*-----------------------------------------*/

#pragma once
#include <Windows.h>
#include <mutex>
#include "logger.h"

enum DSTORAGE_COMPRESSION_FORMAT;
struct DSTORAGE_CONFIGURATION1;
struct DSTORAGE_CONFIGURATION;

class DStorageWrapper
{
	static DStorageWrapper* instance;
	static std::mutex instance_mutex;
	static std::condition_variable instance_cv;

	HMODULE hmodule;
	static std::mutex hmodule_mutex;
public:
	DStorageWrapper(LPCSTR dllname);

	static DStorageWrapper* get() { 
		return instance;
	}

	static DStorageWrapper* get_wait(std::chrono::seconds time)
	{
		if (instance)
			return instance;

		// If the instance doesnt exist, wait for a max of x seconds to see if the constructor will finish
		std::unique_lock<std::mutex> lock(instance_mutex);
		instance_cv.wait_for(lock, time);
		
		return instance;
	}

	HMODULE getHmodule() {
		return hmodule;
	}

	HRESULT DStorageCreateCompressionCodec(DSTORAGE_COMPRESSION_FORMAT format, UINT32 numThreads, REFIID riid, void** ppv);
	HRESULT DStorageSetConfiguration1(DSTORAGE_CONFIGURATION1 const* configuration);
	HRESULT DStorageSetConfiguration(DSTORAGE_CONFIGURATION const* configuration);
	HRESULT DStorageGetFactory(REFIID riid, void** ppv);
};