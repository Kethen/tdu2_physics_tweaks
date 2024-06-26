#include <windows.h>

// https://github.com/TsudaKageyu/minhook
#include "MinHook.h"

// pthread
#include <pthread.h>

// unix-ish
#include <fcntl.h>
#include <unistd.h>

// std
#include <stdio.h>
#include <stdint.h>

// windows
#include <memoryapi.h>

static pthread_mutex_t log_mutex;
int log_fd = -1;

int init_logging(){
	pthread_mutex_init(&log_mutex, NULL);
	log_fd = open("./tdu2_physics_tweaks.txt", O_WRONLY | O_CREAT | O_TRUNC | O_BINARY);
	return 0;
}

int write_data_to_fd(int fd, char *buffer, int len){
	int bytes_written = 0;
	while(bytes_written < len){
		int bytes_written_this_cycle = write(fd, &buffer[bytes_written], len - bytes_written);
		if(bytes_written_this_cycle < 0){
			return bytes_written_this_cycle;
		}
		bytes_written += bytes_written_this_cycle;
	}
	return bytes_written;
}

#define LOG(...){ \
	pthread_mutex_lock(&log_mutex); \
	if(log_fd >= 0){ \
		char _log_buffer[1024]; \
		int _log_len = sprintf(_log_buffer, __VA_ARGS__); \
		write_data_to_fd(log_fd, _log_buffer, _log_len); \
	} \
	pthread_mutex_unlock(&log_mutex); \
}

#if 0
#define LOG_VERBOSE(...) LOG(__VA_ARGS__)
#else
#define LOG_VERBOSE(...)
#endif


void *f0087ebf0_ref = (void *)0x0087ebf0;
void (__attribute__((fastcall)) *f0087ebf0_orig)(uint32_t context);
void __attribute__((fastcall)) f0087ebf0_patched(uint32_t context){
	static bool logged = false;
	float *min_extra_gravity = (float *)(context + 5 * 4);
	float *max_extra_gravity = (float *)(context + 6 * 4);
	float *extra_gravity_accel_duration = (float *)(context + 7 * 4);
	float *extra_gravity_accel_delay = (float *)(context + 8 * 4);
	if(!logged){
		LOG_VERBOSE("extra gravity min %f, max %f, duration %f, delay %f\n", *min_extra_gravity, *max_extra_gravity, *extra_gravity_accel_duration, *extra_gravity_accel_delay);
		logged = true;
	}
	*min_extra_gravity = 0.0;
	*max_extra_gravity = 0.0;
	f0087ebf0_orig(context);
}

int hook_functions(){
	LOG("hook_functions begin\n");

	int ret = 0;

	ret = MH_Initialize();
	if(ret != MH_OK && ret != MH_ERROR_ALREADY_INITIALIZED){
		LOG("Failed initializing MinHook, %d\n", ret);
		return -1;
	}

	ret = MH_CreateHook(f0087ebf0_ref, (LPVOID)&f0087ebf0_patched, (LPVOID *)&f0087ebf0_orig);
	if(ret != MH_OK){
		LOG("Failed creating hook for 0x0087ebf0, %d\n", ret);
		return -1;
	}
	ret = MH_EnableHook(f0087ebf0_ref);
	if(ret != MH_OK){
		LOG("Failed enableing hook for 0x0087ebf0, %d\n", ret);
		return -1;
	}
	return 0;
}

void patch_memory(void *location, void *buffer, int len){
	DWORD orig_protect;
	VirtualProtect(location, len, PAGE_EXECUTE_READWRITE, &orig_protect);
	memcpy(location, buffer, len);
	DWORD dummy;
	VirtualProtect(location, len, orig_protect, &dummy);
}

void patch_gravity(){
	float gravity = -9.81;
	//gravity = gravity / 1.5;
	patch_memory((void *)0x00f4b4d0, (void *)&gravity, sizeof(float));
}

void *delayed_init(void *args){
	sleep(10);
	if(hook_functions() != 0){
		LOG("hooking failed, terminating process :(\n");
		exit(1);
	}
	LOG("done hooking functions\n");

	return NULL;
}

// entrypoint
__attribute__((constructor))
int init(){
	if(init_logging() != 0){
		LOG("pthread mutex init failed for logger, terminating process :(\n");
		exit(1);
	}
	LOG("log initialized\n");
	patch_gravity();
	LOG("done patching gravity\n");

	pthread_t delayed_init_thread;
	pthread_create(&delayed_init_thread, NULL, delayed_init, NULL);
	return 0;
}
