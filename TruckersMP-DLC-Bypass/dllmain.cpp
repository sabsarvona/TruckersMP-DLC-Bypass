#include <Windows.h>
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
		Logger::get()->info("mount_dlcs", "Saved base %p", result);
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
			Logger::get()->info("load_plugins", "Unable to spoof, missing base! Expect a kick.");
	}

	return original_load_plugins(a1, a2);
}

// This is commented because in theory the patch should be undone, however it seems to work if i dont, and the fact that tmp have another check somewhere i cba to deal with
/*
typedef uint64_t(*original_start_game_t)(uint64_t launchpad_handle_u);
original_start_game_t original_start_game;

uint64_t hooked_start_game(uint64_t launchpad_handle_u)
{
	Logger::get()->info("start_game", "Request to start game...");

	if (base)
	{
		uint64_t* array_size = (uint64_t*)(base + 0xF0);

		*array_size = original_size;

		Logger::get()->info("start_game", "Array size returned back to original size of %d, returning to game", original_size);
	}
	else
		Logger::get()->error("start_game", "Unable to undo spoof, missing base!");

	return original_start_game(launchpad_handle_u);
}
*/



void Main()
{
	try
	{
		new Logger(L"", L"dstorage.log", true, "dstorage");
	}
	catch (const std::exception& e)
	{
		MessageBoxA(NULL, e.what(), "dstorage.dll", MB_OK | MB_ICONERROR);
		return;
	}


	Logger::get()->info("dllmain", "Attempting to load dstorage module...");
	try
	{
		new DStorageWrapper();
		if (DStorageWrapper::get()) {
			Logger::get()->info("dllamain", "DStorage Module Created @ %p", DStorageWrapper::get()->getHmodule());
		}
	}
	catch (const std::exception& e)
	{
		Logger::get()->error("dllmain", "Failed to load dstorage module. (%s)", e.what());
	}


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


	/*
	// Then for the continue/load game button
	uint64_t start_game_func_addr = bmem::patternScan("48 89 4C 24 ?? 55 41 55 48 8D AC 24 ?? ?? ?? ?? 48 81 EC ?? ?? ?? ?? 48 8B 81");
	if (!bmem::isAddressValid(start_game_func_addr))
	{
		Logger::get()->error("dllmain", "Failed to find function 'start_game' for some reason (probably too old or new game ver)");
		return;
	}

	// hook it
	res = bmem::hookFunction((LPVOID)start_game_func_addr, hooked_start_game, (LPVOID*)&original_start_game);
	if (bmem::assert_minhook(res))
	{
		Logger::get()->error("dllmain", "Failed to hook function 'start_game' @ %p, %s", start_game_func_addr, bmem::translateMinhookStatus(res));
		return;
	}
	Logger::get()->info("dllmain", "Hooked function 'start_game' successfully @ %p", start_game_func_addr);
	*/

	Logger::get()->info("dllmain", "All hooks applied");
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