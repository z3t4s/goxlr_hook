#include <windows.h>
#include <stdint.h>
#include <map>
#include <psapi.h>
#include "minhook/include/MinHook.h"

// We dont want the compiler to re-order our struct
//
#pragma pack(push, 0x1)
class Pimpl
{
public:
	char pad_0000[80]; //0x0000
	double Value; //0x0050
	char pad_0058[52]; //0x0058
}; //Size: 0x008C

class Slider
{
public:
	char pad_0000[4]; //0x0000
	char* sliderName; //0x0004
	char pad_0008[312]; //0x0008
	class Pimpl* pimpl; //0x0140
	char pad_0144[8]; //0x0144
}; //Size: 0x014C
#pragma pack(pop)

// Just comment this define to remove debug output
//
#define DBG_OUT
#include <stdio.h>
#ifdef DBG_OUT
#define DBGPRINT(x, ...) printf(x, __VA_ARGS__)
#else
#define DBGPRINT(x, ...)
#endif

typedef void (__fastcall *Slider_ctor_t)(Slider* ecx, void* edx, int arg3);
Slider_ctor_t orgSlider_ctor;

HANDLE thread_handle;
uintptr_t addr_slider_ctor;
std::map<Slider*, double> sliders;

// We only need this so we can add the DLL to GoXLR's IAT
//
__declspec(dllexport) void dummyexport()
{

}

uintptr_t find_pattern(uintptr_t start, size_t length, const unsigned char* pattern, const char* mask)
{
	size_t pos = 0;
	auto maskLength = std::strlen(mask) - 1;

	auto startAdress = start;
	for (auto it = startAdress; it < startAdress + length; ++it)
	{
		if (*reinterpret_cast<unsigned char*>(it) == pattern[pos] || mask[pos] == '?')
		{
			if (mask[pos + 1] == '\0')
			{
				return it - maskLength;
			}

			pos++;
		}
		else
		{
			pos = 0;
		}
	}

	return -1;
}

uintptr_t find_pattern(void* module, const unsigned char* pattern, const char* mask, uintptr_t offset)
{
	MODULEINFO info = {};
	GetModuleInformation(GetCurrentProcess(), reinterpret_cast<HMODULE>(module), &info, sizeof(MODULEINFO));

	return find_pattern(reinterpret_cast<uintptr_t>(module) + offset, info.SizeOfImage, pattern, mask);
}

// Hooked Slider constructor
// We're grabbing all Sliders for later usage
//
void __fastcall Slider_ctor(Slider* ecx, void* edx, int arg3)
{	
	sliders.insert(std::pair< Slider*, double>(ecx, 0.0));
	
	DBGPRINT("Created a new slider at 0x%p\n", ecx);
	orgSlider_ctor(ecx, edx, arg3);
}

// Implement your logic in here
//
void callback_valuechange(const char* slider_name, const double slider_value)
{
	DBGPRINT("Slider \"%s\" updated value to %f\n", slider_name, slider_value);
}

// Worker thread. You shouldn't need to adjust anything in there
//
DWORD __stdcall winapi_thread(void* arg)
{	
	// This is verry hackish, but works for now
	// Idealy we had a better way to detect if the GUI is fully setup and in forground, but FindWindow doesnt work
	//
	while (sliders.size() < 14)
		Sleep(100);

	DBGPRINT("Starting GoXLR volume monitoring\n");

	// This endless loop will be broken by TerminateThread later on
	//
	while (true)
	{
		for (auto& slider : sliders)
		{
			// Sanity checks 
			//
			if (!slider.first || !slider.first->pimpl)
				continue;
						
			if (slider.second != slider.first->pimpl->Value)
			{
				slider.second = slider.first->pimpl->Value;
				callback_valuechange(slider.first->sliderName, slider.second);
			}
		}

		// We don't want to hog cpu
		//
		Sleep(100);
	}

	return 0x0;
}

// Standard WinAPI entrypoint for DLLs
//
BOOL WINAPI DllMain(HINSTANCE hinstDLL, DWORD fdwReason, LPVOID lpReserved)
{
	switch (fdwReason)
	{
		case DLL_PROCESS_ATTACH:
		{
			#ifdef DBG_OUT
			// Attach a debug console
			// 
			FILE* f;
			AllocConsole();
			AttachConsole(GetCurrentProcessId());
			freopen_s(&f, "CONOUT$", "w", stdout);
			SetConsoleTitleA("goxlr_hook");
			#endif

			// Minhook is the hooking library of choice. Its pretty small but handles relative jumps and other ugly stuff
			//
			if (MH_Initialize() != MH_STATUS::MH_OK)
			{
				DBGPRINT("Failed to initialize minhook\n");
				return 1;
			}

			// We're grabbing the ASLR based module base of the app
			//
			HMODULE module = GetModuleHandle(NULL);
			if (!module || module == INVALID_HANDLE_VALUE)
			{
				DBGPRINT("This DLL needs to be injected / IAT loaded by \"GoXLR App.exe\"\n");
				return 1;
			}

			// Pattern scanning for the constructor
			//
			// If the pattern breaks you can find the constructor by searching xrefs for the string "gameSlider"
			// Go for the only xref and go two calls down. Thats the constructor
			//
			//55 8B EC 6A ? 68 ? ? ? ? 64 A1 ? ? ? ? 50 83 EC 08 56 A1 ? ? ? ? 33 C5 50 8D 45 F4 64 A3 ? ? ? ? 8B F1 89 75 F0 FF 75 08
			addr_slider_ctor = find_pattern(module, reinterpret_cast<const unsigned char*>("\x55\x8B\xEC\x6A\x00\x68\x00\x00\x00\x00\x64\xA1\x00\x00\x00\x00\x50\x83\xEC\x08\x56\xA1\x00\x00\x00\x00\x33\xC5\x50\x8D\x45\xF4\x64\xA3\x00\x00\x00\x00\x8B\xF1\x89\x75\xF0\xFF\x75\x08"), "xxxx?x????xx????xxxxxx????xxxxxxxx????xxxxxxxx", 0);
			if (addr_slider_ctor == -1)
			{
				DBGPRINT("Pattern for Slider::Slider broke. IDA it is...\n");
				return 1;
			}
			DBGPRINT("Found pattern for Slider::Slider at 0x%08x\n", addr_slider_ctor);
		
			// Create a trampoline for Slider::Slider
			//
			if (MH_CreateHook(reinterpret_cast<void*>(addr_slider_ctor), &Slider_ctor, reinterpret_cast<LPVOID*>(&orgSlider_ctor)) != MH_OK)
			{
				DBGPRINT("Hooking Slider::Slider failed. Does the address 0x%08x make sense?\n", addr_slider_ctor);
				return 1;
			}

			// Activate the hook
			//
			if (MH_EnableHook(reinterpret_cast<void*>(addr_slider_ctor)) != MH_OK)
			{
				DBGPRINT("Enabling the hook on Slider::Slider failed\n");
				return FALSE;
			}

			// Create a worker thread
			//
			thread_handle = CreateThread(0, 0, &winapi_thread, 0, 0, 0);
			return TRUE;
		}
		case DLL_PROCESS_DETACH:
		{
			DBGPRINT("Detaching from process\n");

			// If we never had a setup, we don't need to tear down
			//
			if (addr_slider_ctor == 0)
				return FALSE;

			// Disable the hook, doesn't remove it!
			//
			if (MH_DisableHook(reinterpret_cast<void*>(addr_slider_ctor)) != MH_OK)
			{
				DBGPRINT("Failed to disable the the hook on Slider::Slider\n");
				return FALSE;
			}

			// Finally remove it completly
			//
			if (MH_RemoveHook(reinterpret_cast<void*>(addr_slider_ctor)) != MH_OK)
			{
				DBGPRINT("Failed to remove the the hook on Slider::Slider\n");
				return FALSE;
			}

			// If the application is bailing we quickly need to get rid of the infinite thread, or we crash the host process
			//
			if (thread_handle != INVALID_HANDLE_VALUE)
				TerminateThread(thread_handle, 0);

			return TRUE;
		}
	}

	return FALSE;
}