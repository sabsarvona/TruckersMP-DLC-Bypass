/*----------------------------------------*/
// Project	: TruckersMP DLC Bypass
// File		: dllmain.cpp
// 
// Author	: Baldywaldy09 | Hunter
// Created	: 13-12-2025
// Updated	: 21-12-2025
/*-----------------------------------------*/
// v1.0.1

#include <Windows.h>
#include "ini.h"
#include "bmem.h"
#include "logger.h"
#include "dstorage_wrapper.h"



uint64_t original_size;
uint64_t base;


typedef uint64_t(*original_mount_dlcs_t)(uint64_t a1);
original_mount_dlcs_t original_mount_dlcs;

uint64_t hooked_mount_dlcs(uint64_t a1)
{
	Logger::get()->info("mount_dlcs", "Request to mount DLCs...");

	uint64_t result = original_mount_dlcs(a1);

	if (result)
	{
		base = result;
		Logger::get()->info("mount_dlcs", "Saved base %p, returning to game", result);
	}
	else
		Logger::get()->error("mount_dlcs", "It seems the game failed.");

	return result;
}


typedef uint64_t(*original_load_plugins_t)(uint64_t a1, uint64_t a2);
original_load_plugins_t original_load_plugins;

bool spoofed = false;
uint64_t hooked_load_plugins(uint64_t a1, uint64_t a2)
{
	if (!spoofed)
	{
		Logger::get()->info("load_plugins", "First time request to load plugins, so game has just loaded...");

		if (base)
		{
			// shouldnt use a fixed offset
			// note to self: offset is found at 0x39BE21 ( 1.57.2.7 )
			uint64_t* array_size = (uint64_t*)(base + 0xF0);

			original_size = *array_size;

			*array_size = 0;

			spoofed = true;

			Logger::get()->info("load_plugins", "Spoofed dlc size of %d to 0, returning to game", original_size);
		}
		else
			Logger::get()->warn("load_plugins", "Unable to spoof, missing base! Expect a kick.");
	}

	return original_load_plugins(a1, a2);
}


namespace prism
{
	// basically a const char** (if a prism::string*)
	class string            // Size: 0x08
	{
	public:
		const char* value;  //0x0000 (0x08)

		string(const char* str) {
			size_t len = strlen(str) + 1;
			value = new char[len];
			strcpy_s(const_cast<char*>(value), len, str);
		}
	};
	static_assert(sizeof(string) == 0x08);
}

typedef uint64_t(*original_get_dlc_t)(uint64_t a1, prism::string* name);
original_get_dlc_t original_get_dlc;

uint64_t hooked_get_dlc(uint64_t a1, prism::string* name)
{
	Logger::get()->info("get_dlc", "Game is fetching DLC '%s', spoofing back array size", name->value);

	if (base)
	{

		uint64_t* array_size = (uint64_t*)(base + 0xF0);
		*array_size = original_size;

		uint64_t result = original_get_dlc(a1, name);

		if (result)
			Logger::get()->info("get_dlc", "Successfully fetched DLC data, spoofing DLC size back to 0 and returning");
		else
			Logger::get()->error("get_dlc", "Unexpected Error! Game failed to fetch DLC, expect a crash.");

		*array_size = 0;

		return result;
	}
	else
		Logger::get()->warn("get_dlc", "Unable to spoof, missing base! Expect a kick or crash.");

	return original_get_dlc(a1, name);
}



void Main()
{
	inih::INIReader* config = nullptr;

	try {
		config = new inih::INIReader("./dstorage.ini");
		config->ParseError();
	}
	catch (const std::exception& e)
	{
		MessageBoxA(NULL, e.what(), "dstorage.dll", MB_OK | MB_ICONERROR);
		return exit(1);
	}

	try
	{
		const bool& console = config->Get<bool>("logger", "console");

		new Logger(L"", L"dstorage.log", console, "dstorage");
	}
	catch (const std::exception& e)
	{
		MessageBoxA(NULL, e.what(), "dstorage.dll", MB_OK | MB_ICONERROR);
		return exit(1);
	}

	std::string art = R"(  _____ __  __ ____            ____  _     ____   ______   ______   _    ____ ____  
 |_   _|  \/  |  _ \          |  _ \| |   / ___| | __ ) \ / /  _ \ / \  / ___/ ___| 
   | | | |\/| | |_) |  _____  | | | | |  | |     |  _ \\ V /| |_) / _ \ \___ \___ \ 
   | | | |  | |  __/  |_____| | |_| | |__| |___  | |_) || | |  __/ ___ \ ___) |__) |
   |_| |_|  |_|_|             |____/|_____\____| |____/ |_| |_| /_/   \_\____/____/ 
)";
	printf("%s\n", art.c_str());

	const bool& dstorage_enabled = config->Get<bool>("dstorage", "enabled");

	if (dstorage_enabled)
	{
		Logger::get()->info("dllmain", "Attempting to load dstorage module...");
		try
		{
			const std::string& orgapi = config->Get<std::string>("dstorage", "orgapi");

			new DStorageWrapper(orgapi.c_str());
			if (DStorageWrapper::get()) {
				Logger::get()->info("dllmain", "DStorage Module Created @ %p", DStorageWrapper::get()->getHmodule());
			}
		}
		catch (const std::exception& e)
		{
			Logger::get()->error("dllmain", "Failed to load dstorage module. (%s)", e.what());
		}
	}
	else
		Logger::get()->warn("dllmain", "Skipping dstorage initialisation");


	const bool& bypass_enabled = config->Get<bool>("dlcbypass", "enabled");

	if (bypass_enabled)
	{
		Logger::get()->info("dllmain", "Applying hooks...");

		// First, find the function that does the dlc mounting
		uint64_t mount_dlcs_func_addr = bmem::patternScan("40 53 48 81 EC ?? ?? ?? ?? 48 8D 84 24 ?? ?? ?? ?? C7 84");
		if (!bmem::isAddressValid(mount_dlcs_func_addr))
		{
			Logger::get()->error("dllmain", "Failed to find function 'mount_dlcs' for some reason (probably too old or new game ver)");
			return;
		}

		// Then we hook it
		MH_STATUS res = bmem::hookFunction((LPVOID)mount_dlcs_func_addr, hooked_mount_dlcs, (LPVOID*)&original_mount_dlcs);
		if (bmem::assert_minhook(res))
		{
			Logger::get()->error("dllmain", "Failed to hook function 'mount_dlcs' @ %p, %s", mount_dlcs_func_addr, bmem::translateMinhookStatus(res));
			return;
		}
		Logger::get()->info("dllmain", "Hooked function 'mount_dlcs' successfully @ %p", mount_dlcs_func_addr);



		// Now we do it for load plugins
		uint64_t load_plugins_func_addr = bmem::patternScan("48 89 4C 24 ?? 55 53 56 57 41 57 48 8D AC 24 ?? ?? ?? ?? 48 81 EC ?? ?? ?? ?? 45 33 FF 48 8D 05");
		if (!bmem::isAddressValid(load_plugins_func_addr))
		{
			Logger::get()->error("dllmain", "Failed to find function 'load_plugins' for some reason (probably too old or new game ver)");
			return;
		}

		// hook it
		res = bmem::hookFunction((LPVOID)load_plugins_func_addr, hooked_load_plugins, (LPVOID*)&original_load_plugins);
		if (bmem::assert_minhook(res))
		{
			Logger::get()->error("dllmain", "Failed to hook function 'load_plugins' @ %p, %s", load_plugins_func_addr, bmem::translateMinhookStatus(res));
			return;
		}
		Logger::get()->info("dllmain", "Hooked function 'load_plugins' successfully @ %p", load_plugins_func_addr);


		
		// Then for the get dlc function
		uint64_t get_dlc_func_addr = bmem::patternScan("48 89 5C 24 ?? 48 89 74 24 ?? 57 48 83 EC ?? 4C 8B 91 ?? ?? ?? ?? 45");
		if (!bmem::isAddressValid(get_dlc_func_addr))
		{
			Logger::get()->error("dllmain", "Failed to find function 'get_dlc' for some reason (probably too old or new game ver)");
			return;
		}

		// hook it
		res = bmem::hookFunction((LPVOID)get_dlc_func_addr, hooked_get_dlc, (LPVOID*)&original_get_dlc);
		if (bmem::assert_minhook(res))
		{
			Logger::get()->error("dllmain", "Failed to hook function 'get_dlc' @ %p, %s", get_dlc_func_addr, bmem::translateMinhookStatus(res));
			return;
		}
		Logger::get()->info("dllmain", "Hooked function 'get_dlc' successfully @ %p", get_dlc_func_addr);
		

		Logger::get()->info("dllmain", "All hooks applied");
	}
	else
		Logger::get()->warn("dllmain", "TruckersMP Kick Bypass NOT applied");
}


BOOL WINAPI DllMain(HINSTANCE hinstDLL, DWORD fdwReason, LPVOID lpvReserved)
{
	if (fdwReason == DLL_PROCESS_ATTACH)
	{
		//MessageBoxA(0, "a", "a", 0);
		Main(); // We wont thread this since there is no loop and we need to hook before the game gets there
	}

	if (fdwReason == DLL_PROCESS_DETACH)
	{
		if (Logger::get())
			delete Logger::get();
	}

	return TRUE;
}