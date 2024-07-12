#include <windows.h>

#include <dinput.h>

#include "logging.h"
#include "config.h"
#include "common.h"

#define STR(s) #s

int (*hook_create_device_methods)() = NULL;

void (*init_lib_logging)() = NULL;
void (*log_effect)(LPGUID effect_guid, LPDIEFFECT params, DWORD *modified_items, bool should_log) = NULL;
void (*log_device_prop)(LPGUID prop_guid, LPDIPROPHEADER propheader, bool should_log) = NULL;

void (*set_set_param_cb)(void (__attribute__((stdcall)) *cb)(LPGUID effect_guid, LPDIEFFECT params, DWORD *dwFlags)) = NULL;
void (*set_create_effect_cb)(void (__attribute__((stdcall)) *cb)(LPGUID effect_guid, LPDIEFFECT params)) = NULL;
void (*set_set_device_property_cb)(void (__attribute__((stdcall)) *cb)(LPGUID prop_guid, LPDIPROPHEADER propheader)) = NULL;

HMODULE (*get_dinput8_handle)() = NULL;

HRESULT (WINAPI *DirectInput8Create_fetched)(HINSTANCE,DWORD,REFIID,LPVOID *,LPUNKNOWN);

static void __attribute__((stdcall))set_param_cb(LPGUID effect_guid, LPDIEFFECT params, DWORD *modified_items){
	//log_effect(effect_guid, params, modified_items, current_config.f.log_effects);

	// not grabbing config mutex here, called too often
	if(!current_config.f.enabled){
		return;
	}
	if(*effect_guid == GUID_Spring && *modified_items & DIEP_TYPESPECIFICPARAMS && params->lpvTypeSpecificParams != NULL){
		DICONDITION *condition = (DICONDITION *)params->lpvTypeSpecificParams;
		// potential settings
		int new_spring_effect_max = 10000;
		int new_spring_effect_min = 6000;
		float current_slip_ratio = calculate_new_slip_ratio(global.brake_pedal_level);

		// roughly 1000 - 9500, rescale it and spice it up
		#define SCALE_VALUE(v){ \
			int to_scale = (v); \
			to_scale = to_scale - 1000; \
			if(to_scale < 0){ \
				(v) = 0; \
			}else{ \
				float level = to_scale / 8500.0; \
				int diff = new_spring_effect_max - new_spring_effect_min; \
				int lower_level = level > 0.1? new_spring_effect_min: (level / 0.1) * new_spring_effect_min; \
				int upper_level = level > 0.1? (level - 0.1) * diff: 0; \
				int new_level = lower_level + upper_level; \
				new_level = new_level * (1.0 + current_slip_ratio / 0.5); \
				if(new_level < 0){ \
					new_level = 0; \
				} \
				(v) = new_level; \
			} \
		}
		SCALE_VALUE(condition->lPositiveCoefficient);
		SCALE_VALUE(condition->lNegativeCoefficient);
		SCALE_VALUE(condition->dwPositiveSaturation);
		SCALE_VALUE(condition->dwNegativeSaturation);
		#undef SCALE_VALUE

		//condition->lPositiveCoefficient = 10000;
		//condition->lNegativeCoefficient = 10000;

		//condition->dwPositiveSaturation = 10000;
		//condition->dwNegativeSaturation = 10000;
	}

	if(*effect_guid == GUID_Damper && *modified_items & DIEP_TYPESPECIFICPARAMS && params->lpvTypeSpecificParams != NULL && current_config.f.reduce_damper){
		DICONDITION *condition = (DICONDITION *)params->lpvTypeSpecificParams;
		condition->dwPositiveSaturation = condition->lPositiveCoefficient;
		condition->dwNegativeSaturation = condition->lNegativeCoefficient;
	}

	log_effect(effect_guid, params, modified_items, current_config.f.log_effects);
}

static void __attribute__((stdcall))create_effect_cb(LPGUID effect_guid, LPDIEFFECT params){
	// TODO add settings
	DWORD modified_items = DIEP_ALLPARAMS;
	log_effect(effect_guid, params, &modified_items, false);
}

static void __attribute__((stdcall)) set_device_property_cb(LPGUID prop_guid, LPDIPROPHEADER propheader){
	// TODO add settings
	log_device_prop(prop_guid, propheader, false);
}

int init_dinput8_hook(){
	const char *lib_path = "dinput8_ffb_tweaks_i686.dll";
	HMODULE lib = LoadLibraryA(lib_path);
	if(lib == NULL){
		LOG("Failed loading %s, ffb tweaks disabled\n", lib_path);
		return -1;
	}

	hook_create_device_methods = (int (*)())GetProcAddress(lib, "hook_create_device_methods");
	init_lib_logging = (void (*)())GetProcAddress(lib, "init_logging");
	log_effect = (void (*)(LPGUID, LPDIEFFECT, DWORD *, bool))GetProcAddress(lib, "log_effect");
	log_device_prop = (void (*)(LPGUID prop_guid, LPDIPROPHEADER propheader, bool should_log))GetProcAddress(lib, "log_device_prop");
	get_dinput8_handle = (HMODULE (*)())GetProcAddress(lib, "get_dinput8_handle");
	set_set_param_cb = (void (*)(void (__attribute__((stdcall)) *cb)(LPGUID effect_guid, LPDIEFFECT params, DWORD *dwFlags)))GetProcAddress(lib, "set_set_param_cb");
	set_create_effect_cb = (void (*)(void (__attribute__((stdcall)) *cb)(LPGUID effect_guid, LPDIEFFECT params)))GetProcAddress(lib, "set_create_effect_cb");
	set_set_device_property_cb = (void (*)(void (__attribute__((stdcall)) *cb)(LPGUID prop_guid, LPDIPROPHEADER propheader)))GetProcAddress(lib, "set_set_device_property_cb");

	#define CHECK_HOOK(name) { \
		if(name == NULL){ \
			LOG("Failed fetching function %s, ffb tweaks disabled\n", STR(name)); \
			return -1; \
		} \
	}
	CHECK_HOOK(hook_create_device_methods);
	CHECK_HOOK(init_lib_logging);
	CHECK_HOOK(log_effect);
	CHECK_HOOK(log_device_prop);
	CHECK_HOOK(get_dinput8_handle);
	CHECK_HOOK(set_set_param_cb);
	CHECK_HOOK(set_create_effect_cb);
	CHECK_HOOK(set_set_device_property_cb);
	#undef CHECK_HOOK

	init_lib_logging();

	set_set_param_cb(set_param_cb);
	set_create_effect_cb(create_effect_cb);
	set_set_device_property_cb(set_device_property_cb);

	if(hook_create_device_methods() != 0){
		LOG("Failed dinput8 create device methods, ffb tweaks disabled\n");
		return -1;
	}

	return 0;
}
