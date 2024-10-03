#include <windows.h>

// processes and threads
#include <processthreadsapi.h>
#include <tlhelp32.h>

// module information
#define PSAPI_VERSION 2
#include <psapi.h>
#include <libloaderapi.h>

#include "logging.h"

int suspend_threads(HANDLE *threads_snap){
	// https://learn.microsoft.com/en-us/windows/win32/toolhelp/traversing-the-thread-list
	DWORD this_thread = GetCurrentThreadId();
	DWORD this_process = GetCurrentProcessId();
	*threads_snap = CreateToolhelp32Snapshot(TH32CS_SNAPTHREAD, 0);
	THREADENTRY32 te32;

	if(*threads_snap == INVALID_HANDLE_VALUE){
		LOG("cannot obtain threads snapshot\n");
		return -1;
	}

	te32.dwSize = sizeof(THREADENTRY32);
	if(!Thread32First(*threads_snap, &te32)){
		CloseHandle(*threads_snap);
		LOG("failed fetching the first thread entry, not suspending threads\n");
		return -1;
	}

	do{
		if(te32.th32OwnerProcessID == this_process && te32.th32ThreadID != this_thread){
			HANDLE thread_handle = OpenThread(THREAD_SUSPEND_RESUME, 0, te32.th32ThreadID);
			if(thread_handle == INVALID_HANDLE_VALUE){
				LOG("failed fetching handle for thread with id %u during thread suspension\n", te32.th32ThreadID);
				CloseHandle(*threads_snap);
				return -1;
			}
			int ret = SuspendThread(thread_handle);
			if(ret == -1){
				LOG("failed suspending thread with id %u\n", te32.th32ThreadID);
				CloseHandle(thread_handle);
				CloseHandle(*threads_snap);
				return -1;
			}
			CloseHandle(thread_handle);
		}
	}while(Thread32Next(*threads_snap, &te32));
	return 0;
}

int resume_threads(HANDLE threads_snap){
	DWORD this_thread = GetCurrentThreadId();
	DWORD this_process = GetCurrentProcessId();

	THREADENTRY32 te32;

	if(!Thread32First(threads_snap, &te32)){
		CloseHandle(threads_snap);
		LOG("failed fetching the first thread entry, not resuming threads\n");
		return -1;
	}

	do{
		if(te32.th32OwnerProcessID == this_process && te32.th32ThreadID != this_thread){
			HANDLE thread_handle = OpenThread(THREAD_SUSPEND_RESUME, 0, te32.th32ThreadID);
			if(thread_handle == INVALID_HANDLE_VALUE){
				LOG("failed fetching handle for thread with id %u during thread resuming\n", te32.th32ThreadID);
				CloseHandle(threads_snap);
				return -1;
			}
			int ret = ResumeThread(thread_handle);
			if(ret == -1){
				LOG("failed resuming thread with id %u\n", te32.th32ThreadID);
				CloseHandle(thread_handle);
				CloseHandle(threads_snap);
				return -1;
			}
			CloseHandle(thread_handle);
		}
	}while(Thread32Next(threads_snap, &te32));
	CloseHandle(threads_snap);
	return 0;
}
