#include <windows.h>
#include <heapapi.h>

#include <MinHook.h>

#include "logging.h"
#include "process.h"

#define STR(s) #s

// why is the game polling about heap status? freeing heap should perform defragmentation already..?
typedef SIZE_T (WINAPI *HEAP_COMPACT)(HANDLE, DWORD);
HEAP_COMPACT HeapCompat_orig = NULL;
WINAPI SIZE_T HeapCompat_patched(HANDLE heap_handle, DWORD flags){
	SIZE_T ret = HeapCompat_orig(heap_handle, flags);
	LOG("%s: handle 0x%08lx, flags 0x%08lx, ret 0x%08lx\n", __func__, heap_handle, flags, ret);
	return ret;
}

int hook_heap_compact(){
	int ret = MH_Initialize();
	if(ret != MH_OK && ret != MH_ERROR_ALREADY_INITIALIZED){
		LOG("Failed initializing MinHook, %d\n", ret);
		return -1;
	}

	HANDLE threads_snap;
	if(suspend_threads(&threads_snap) != 0){
		LOG("failed suspending threads before hooking HeapCompact\n");
		return -1;
	}

	#define CREATE_ENABLE_HOOK(target, patched, trampoline){ \
		int r = MH_CreateHook((LPVOID)target, (LPVOID)&patched, (LPVOID *)&trampoline); \
		if(r != MH_OK){ \
			LOG("Failed creating hook for %s, %d\n", STR(target), r); \
			return -1; \
		} \
		r = MH_EnableHook((LPVOID)target); \
		if(r != MH_OK){ \
			LOG("Failed enableing hook for target, %d\n", STR(target), r); \
			return -1; \
		} \
	}
	CREATE_ENABLE_HOOK(HeapCompact, HeapCompat_patched, HeapCompat_orig);
	#undef CREATE_ENABLE_HOOK

	if(resume_threads(threads_snap) != 0){
		LOG("failed resuming threads after hooking HeapCompact\n");
		return -1;
	}

	return 0;
}
