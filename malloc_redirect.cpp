#include <windows.h>
#include <libloaderapi.h>

// https://github.com/TsudaKageyu/minhook
#include "MinHook.h"

#include "logging.h"

typedef void * (__attribute__((cdecl)) *MALLOC)(size_t);
static MALLOC _malloc = NULL;
static MALLOC _malloc_crt_orig = NULL;
void __attribute__((cdecl)) *_malloc_crt_patched(size_t size){
	void *ret = _malloc(size);
	LOG("%s: malloc for %d bytes, 0x%08x\n", __func__, size, ret);
	return ret;
}

typedef void * (__attribute__((cdecl)) *REALLOC)(void *, size_t);
static REALLOC _realloc = NULL;
static REALLOC _realloc_crt_orig = NULL;
void __attribute__((cdecl)) *_realloc_crt_patched(void *ptr, size_t size){
	void *ret = _realloc(ptr, size);
	LOG("%s: realloc 0x%08x for %d bytes, 0x%08x", __func__, ptr, size, ret);
	return ret;
}

int malloc_redirect(){
	LOG("malloc_redirect begin\n");
	int ret = 0;

	ret = MH_Initialize();
	if(ret != MH_OK && ret != MH_ERROR_ALREADY_INITIALIZED){
		LOG("Failed initializing MinHook, %d\n", ret);
		return -1;
	}

	HMODULE lib = LoadLibraryA("MSVCR90.dll");
	if(lib == NULL){
		LOG("Failed loading MSVCR90.dll, 0x%08x\n", GetLastError());
		return -1;
	}

	_malloc = (MALLOC)GetProcAddress(lib, "malloc");
	if(_malloc == NULL){
		LOG("Failed fetching malloc\n");
		return -1;
	}
	LOG("found MSVCR90 malloc at 0x%08x\n", _malloc);

	MALLOC _malloc_crt = (MALLOC)GetProcAddress(lib, "_malloc_crt");
	if(_malloc_crt == NULL){
		LOG("Failed fetching _malloc_crt\n");
		return -1;
	}
	LOG("found MSVCR90 _malloc_crt at 0x%08x\n", _malloc_crt);

	_realloc = (REALLOC)GetProcAddress(lib, "realloc");
	if(_realloc == NULL){
		LOG("Failed fetching reallloc\n");
		return -1;
	}
	LOG("found MSVCR90 realloc at 0x%08x\n", _realloc);

	REALLOC _realloc_crt = (REALLOC)GetProcAddress(lib, "_realloc_crt");
	if(_realloc_crt == NULL){
		LOG("Failed fetching _realloc_crt");
		return -1;
	}
	LOG("found MSVCR90 _realloc_crt at 0x%08x\n", _realloc_crt);

	#define STR(s) #s
	#define CREATE_ENABLE_HOOK(target, replacement, original) {\
		int ret = MH_CreateHook((void *)&target, (void *)&replacement, (void **)&original); \
		if(ret != MH_OK){ \
			LOG("Failed hooking " STR(target) ", %d\n", ret); \
			return -1; \
		} \
		ret = MH_EnableHook((void *)&target); \
		if(ret != MH_OK){ \
			LOG("Failed enabling " STR(target) " hook, %d\n", ret); \
			return -1; \
		} \
	}
	CREATE_ENABLE_HOOK(_malloc_crt, _malloc_crt_patched, _malloc_crt_orig);
	CREATE_ENABLE_HOOK(_realloc_crt, _realloc_crt_patched, _realloc_crt_orig);

	return 0;
}
