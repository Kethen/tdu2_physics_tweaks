#include <windows.h>

#include <dinput.h>

#include "logging.h"
#include "config.h"

#define STR(s) #s

int (*hook_create_device_A)(LPDIRECTINPUT8A dinput8_interface_A) = NULL;
int (*hook_create_device_W)(LPDIRECTINPUT8W dinput8_interface_W) = NULL;
void (*log_effect)(LPGUID effect_guid, LPDIEFFECT params, DWORD *modified_items, bool should_log) = NULL;
void (*init_lib_logging)() = NULL;
HMODULE (*get_dinput8_handle)() = NULL;

HRESULT (WINAPI *DirectInput8Create_fetched)(HINSTANCE,DWORD,REFIID,LPVOID *,LPUNKNOWN);

int init_dinput8_hook(){
	const char *lib_path = "dinput8_ffb_tweaks_i686.dll";
	HMODULE lib = LoadLibraryA(lib_path);
	if(lib == NULL){
		LOG("Failed loading %s, ffb tweaks disabled\n");
		return -1;
	}

	hook_create_device_A = (int (*)(LPDIRECTINPUT8A))GetProcAddress(lib, "hook_create_device_A");
	hook_create_device_W = (int (*)(LPDIRECTINPUT8W))GetProcAddress(lib, "hook_create_device_W");
	log_effect = (void (*)(LPGUID, LPDIEFFECT, DWORD *, bool))GetProcAddress(lib, "log_effect");
	init_lib_logging = (void (*)())GetProcAddress(lib, "init_logging");
	get_dinput8_handle = (HMODULE (*)())GetProcAddress(lib, "get_dinput8_handle");

	#define CHECK_HOOK(name) { \
		if(name == NULL){ \
			LOG("Failed fetching function %s, ffb tweaks disabled\n", STR(name)); \
			return -1; \
		} \
	}
	CHECK_HOOK(hook_create_device_A);
	CHECK_HOOK(hook_create_device_W);
	CHECK_HOOK(log_effect);
	CHECK_HOOK(init_lib_logging);
	CHECK_HOOK(get_dinput8_handle);
	#undef CHECK_HOOK

	init_lib_logging();

	HMODULE dinput8 = get_dinput8_handle();
	if(dinput8 == NULL){
		LOG("Failed fetching dinput8 handle, ffb tweaks disabled\n");
		return -1;
	}
	DirectInput8Create_fetched = (HRESULT (WINAPI *)(HINSTANCE,DWORD,REFIID,LPVOID *,LPUNKNOWN))GetProcAddress(dinput8, "DirectInput8Create");

	// this instance will be a memory leak? how the hek are you supposed to free these
	LPDIRECTINPUT8A direct_input_8_interface_A;
	HRESULT res = DirectInput8Create_fetched((HINSTANCE)init_dinput8_hook, 0x0800, IID_IDirectInput8A, (LPVOID *)&direct_input_8_interface_A, NULL);
	if(res != DI_OK){
		LOG("Failed creating dinput8 interface, ffb tweaks disabled\n");
		return -1;
	}
	if(hook_create_device_A(direct_input_8_interface_A) != 0){
		LOG("Failed hooking dinput8 A interface, ffb tweaks disabled\n");
		return -1;
	}

	return 0;
}
