#pragma once

#include <windows.h>
#include <cstdint>
#include <string>
#include <sstream>
#include <vector>
#include <map>

#define MINHOOK_ENABLED 1

#if MINHOOK_ENABLED
	#include <MinHook/MinHook.h>
	#pragma comment(lib, "minhook.x64.lib")
#endif

namespace bmem
{
	namespace
	{
		// custom byte type
		class pattern_byte
		{
		public:
			uint8_t byte;
			bool ignore;
		};

		inline uintptr_t moduleBase;
		inline uint64_t moduleSize;

		inline bool wasModuleSet = false;
	}

	static bool setModule(const char* module)
	{
		if (std::string(module) == "current")
		{
			if (!wasModuleSet)
				moduleBase = (uintptr_t)GetModuleHandleA(nullptr);
			else
				return true;
		}
		else
			moduleBase = (uintptr_t)GetModuleHandleA(module);


		if (!moduleBase)
		{
			//printf("[BMEM] Failed to set module to: '%s'. Module not found! | Did you mean '%s.dll' or '%s.exe'?\n", module, module, module);
			return false;
		}

		const auto* header = (IMAGE_DOS_HEADER*)moduleBase;
		const auto* nt_header = (IMAGE_NT_HEADERS64*)((uint8_t*)header + header->e_lfanew);

		moduleSize = nt_header->OptionalHeader.SizeOfImage;

		//printf("[BMEM] Module set to: '%s' (Base: 0x%llx, Size: %llu)\n", module, moduleBase, moduleSize);

		wasModuleSet = true;
		return true;
	}


	inline std::map<std::string, uintptr_t> scanCache;

	static uintptr_t patternScan(const char* patternSTR, bool useCache = false, const char* moduleToSet = "current")
	{
		//printf("[BMEM] bmem::patternScan: Finding pattern '%s'\n", patternSTR);

		if (useCache)
		{
			std::string patternStdStr = std::string(patternSTR);
			if (scanCache.find(patternStdStr) != scanCache.end())
			{
				uintptr_t address = scanCache[patternStdStr];
				//printf("[BMEM] bmem::patternScan: Found using cache at: 0x%llx\n", address);

				return address;
			}
		}

		if (!setModule(moduleToSet))
		{
			//printf("[BMEM] bmem::patternScan: Failed to initialize a module\n");
			return 0;
		}

		std::vector<pattern_byte> pattern;

		std::istringstream stream(patternSTR);
		std::string token;
		while (stream >> token) {
			pattern_byte pbyte;

			if (token == "??" || token == "?") {
				pbyte.ignore = true;
			}
			else
			{
				if (token.length() > 2 || token.length() < 2)
				{
					//printf("[BMEM] bmem::patternScan: Invalid token: %s\n", token.c_str());
					return 0;
				}

				pbyte.ignore = false;

				unsigned int byte;
				std::istringstream(token) >> std::hex >> byte;
				pbyte.byte = (uint8_t)byte;
			}

			pattern.push_back(pbyte);
		}


		if (pattern[0].ignore)
		{
			//printf("[BMEM] bmem::patternScan: Invalid byte (0)! | The first byte should never be a wildcard!\n");
			return 0;
		}


		bool foundFirstByte = false;
		int patternIndex = 0;
		uintptr_t patternStart = 0;
		for (uint64_t i = 0; i < moduleSize; i++)
		{
			uintptr_t currentAddress = moduleBase + i;
			uint8_t currentByte = *reinterpret_cast<uint8_t*>(currentAddress);

			if (!foundFirstByte)
			{
				if (currentByte == pattern[patternIndex].byte)
				{
					patternStart = currentAddress;
					patternIndex++;
					foundFirstByte = true;
				}
			}
			else
			{
				if (currentByte == pattern[patternIndex].byte) {
					patternIndex++;
				}
				else if (pattern[patternIndex].ignore)
				{
					patternIndex++;
				}
				else
				{
					patternStart = 0;
					patternIndex = 0;
					foundFirstByte = false;
				}

				if (patternIndex == pattern.size())
				{
					break;
				}
			}
		}

		if (patternStart != NULL)
		{
			//printf("[BMEM] bmem::patternScan: Found At: 0x%llx\n", patternStart);
			if (useCache) scanCache.emplace(std::string(patternSTR), patternStart);
		}
		else
		{
			//printf("[BMEM] bmem::patternScan: Pattern not found!\n");
		}

		return patternStart;
	}

	static bool isAddressValid(uintptr_t address)
	{
		if (address < moduleBase || address > (moduleBase + moduleSize))
			return false;

		return true;
	}

	// Converts a relative address to an absolute address
	static uintptr_t relativeToAbsolute(uintptr_t instructionAddress, int offsetToAbsolute, int instructionSize)
	{
		int32_t relativeOffset = *reinterpret_cast<int32_t*>(instructionAddress + offsetToAbsolute);
		uintptr_t absoluteAddress = instructionAddress + instructionSize + relativeOffset;
		return absoluteAddress;
	}



#if MINHOOK_ENABLED
	// Will return true if error
	static bool assert_minhook(MH_STATUS result)
	{
		if (result != MH_OK)
			return true;
		
		return false;
	}

	// create and enable a hook using minhook 
	static MH_STATUS hookFunction(LPVOID target, LPVOID detour, LPVOID* original)
	{
		MH_STATUS status = MH_Initialize();
		if (status != MH_OK && status != MH_ERROR_ALREADY_INITIALIZED)
		{
			//printf("[BMEM] Failed to initialize MinHook! Status: %d\n", status);
			return status;
		}


		status = MH_CreateHook(target, detour, original);
		if (status != MH_OK)
		{
			//printf("[BMEM] bmem::hookFunction: Failed to create hook! Target: 0x%llx, Detour: 0x%llx\n", target, detour);
			return status;
		}

		status = MH_EnableHook((LPVOID)target);
		if (status != MH_OK)
		{
			//printf("[BMEM] bmem::hookFunction: Failed to enable hook! Target: 0x%llx, Detour: 0x%llx\n", target, detour);
			return status;
		}

		return status;
	}

	static const char* translateMinhookStatus(MH_STATUS status)
	{
		return MH_StatusToString(status);
	}
#endif
}