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

void *f00bdb970_ref = (void *)0x00bdb970;
uint32_t (__attribute__((stdcall)) *f00bdb970_orig)(uint32_t, uint32_t, uint32_t, uint32_t);
uint32_t __attribute__((stdcall)) f00bdb970_patched(uint32_t source, uint32_t unknown_1, uint32_t unknown_2, uint32_t unknown_3){
	LOG_VERBOSE("converting aero performance data from 0x%08x, unknown_1 0x%08x, unknown_2 0x%08x, unknown_3 0x%08x\n", source, unknown_1, unknown_2, unknown_3);
	float *source_lift_drag_ratio = (float *)(source + 0x230);

	float *source_down_force_velocity = (float *)(source + 0x23c);
	float *source_down_force_front = (float *)(source + 0x234);
	float *source_down_force_rear = (float *)(source + 0x238);

	uint32_t ret = f00bdb970_orig(source, unknown_1, unknown_2, unknown_3);

	LOG_VERBOSE("source lift drag ratio %f\n", *source_lift_drag_ratio);
	LOG_VERBOSE("source down force velocity %f\n", *source_down_force_velocity);
	LOG_VERBOSE("source down force front %f\n", *source_down_force_front);
	LOG_VERBOSE("source down force rear %f\n", *source_down_force_back);
	return ret;
}

void *f00df3b30_ref = (void *)0x00df3b30;
uint32_t (__attribute__((stdcall)) *f00df3b30_orig)(uint32_t target, uint32_t source, uint32_t unknown);
uint32_t __attribute__((stdcall)) f00df3b30_patched(uint32_t target, uint32_t source, uint32_t unknown){
	LOG_VERBOSE("converting suspension performance data from 0x%08x to 0x%08x, unknown argument is 0x%08x\n", source, target, unknown);

	float *converted_suspension_length_front = (float *)(target + 0x44);
	float *converted_suspension_length_back = (float *)(target + 0x48);

	// 2 copies for some reason
	float *converted_dampers_front_1 = (float *)(target + 0x64);
	float *converted_dampers_rear_1 = (float *)(target + 0x68);
	float *converted_dampers_front_2 = (float *)(target + 0x6c);
	float *converted_dampers_rear_2 = (float *)(target + 0x70);

	float *converted_ride_height_front = (float *)(target + 0x34);
	float *converted_ride_height_rear = (float *)(target + 0x38);

	float *converted_anti_roll_bar_front = (float *)(target + 0x74);
	float *converted_anti_roll_bar_rear = (float *)(target + 0x78);

	float *converted_anti_roll_bar_damping_front = (float *)(target + 0x7c);
	float *converted_anti_roll_bar_damping_rear = (float *)(target + 0x80);

	float *unknowns[128];

	unknowns[0] = (float *)(target + 0x44);
	unknowns[1] = (float *)(target + 0x48);
	unknowns[2] = (float *)(target + 0x3c);
	unknowns[3] = (float *)(target + 0x40);
	unknowns[4] = (float *)(target + 0x5c);
	unknowns[5] = (float *)(target + 0x60);
	unknowns[6] = (float *)(target + 0x64);
	unknowns[7] = (float *)(target + 0x68);
	unknowns[8] = (float *)(target + 0x6c);
	unknowns[9] = (float *)(target + 0x70);
	unknowns[10] = (float *)(target + 0x74);
	unknowns[11] = (float *)(target + 0x78);
	unknowns[12] = (float *)(target + 0x7c);
	unknowns[13] = (float *)(target + 0x80);
	unknowns[14] = (float *)(target);
	unknowns[15] = (float *)(target + 0x8);
	unknowns[16] = (float *)(target + 0x10);
	unknowns[17] = (float *)(target + 0x14);
	unknowns[18] = (float *)(target + 0x1c);
	unknowns[19] = (float *)(target + 0x20);
	unknowns[20] = (float *)(target + 0x24);
	unknowns[21] = (float *)(target + 0x28);
	unknowns[22] = (float *)(target + 0x2c);
	unknowns[23] = (float *)(target + 0x30);
	unknowns[24] = (float *)(target + 0x84);
	unknowns[25] = (float *)(target + 0x88);

	uint32_t ret = f00df3b30_orig(target, source, unknown);
	LOG_VERBOSE("converted ride height front %f\n", *converted_ride_height_front);
	LOG_VERBOSE("converted ride height rear %f\n", *converted_ride_height_rear);

	for(int i = 14;i < 26;i++){
		LOG_VERBOSE("unknown converted value %d %f\n", i, *(unknowns[i]));
	}
	return ret;
}

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

	ret = MH_CreateHook(f00bdb970_ref, (LPVOID)&f00bdb970_patched, (LPVOID *)&f00bdb970_orig);
	if(ret != MH_OK){
		LOG("Failed creating hook for 0x00bdb970, %d\n", ret);
		return -1;
	}
	ret = MH_EnableHook(f00bdb970_ref);
	if(ret != MH_OK){
		LOG("Failed enableing hook for 0x00bdb970, %d\n", ret);
		return -1;
	}

	ret = MH_CreateHook(f00df3b30_ref, (LPVOID)&f00df3b30_patched, (LPVOID *)&f00df3b30_orig);
	if(ret != MH_OK){
		LOG("Failed creating hook for 0x00df3b30, %d\n", ret);
		return -1;
	}
	ret = MH_EnableHook(f00df3b30_ref);
	if(ret != MH_OK){
		LOG("Failed enableing hook for 0x00df3b30, %d\n", ret);
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
