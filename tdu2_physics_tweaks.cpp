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

// json
#include <json.hpp>

#define STR(s) #s

using json = nlohmann::json;

pthread_mutex_t log_mutex;
int log_fd = -1;

void init_logging(){
	pthread_mutex_init(&log_mutex, NULL);
	const char *log_path = "./tdu2_physics_tweaks_log.txt";
	unlink(log_path);
	log_fd = open(log_path, O_WRONLY | O_CREAT | O_TRUNC);
}

int read_data_from_fd(int fd, char *buffer, int len){
	int bytes_read = 0;
	while(bytes_read < len){
		int bytes_read_this_cycle = read(fd, &buffer[bytes_read], len - bytes_read);
		if(bytes_read_this_cycle < 0){
			return bytes_read_this_cycle;
		}
		bytes_read += bytes_read_this_cycle;
	}
	return bytes_read;
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

struct overrides{
	// sets on boot only, part of the physics engine seems obfuscated and rather annoying to hook run time
	float gravity;

	// overrides Physics.cpr, applies right away
	float min_extra_gravity;
	float max_extra_gravity;
	float extra_gravity_accel_duration;
	float extra_gravity_accel_delay;
};

struct multipliers{
	// some values that can be set with https://turboduck.net/forums/topic/33748-tdu2vpe-release/
	// applies during car spawn
	float suspension_length_front;
	float suspension_length_rear;
	float dampers_front;
	float dampers_rear;
	float ride_height_front;
	float ride_height_rear;
	float anti_roll_bar_front;
	float anti_roll_bar_rear;
	float anti_roll_bar_damping_front;
	float anti_roll_bar_damping_rear;

	float lift_drag_ratio;
	float down_force_velocity;
	float down_force_front;
	float down_force_rear;

	float lateral_grip_front;
	float lateral_grip_rear;
	float grip_front;
	float grip_rear;
};

struct config{
	struct overrides o;
	struct multipliers m;
	bool only_modify_player_vehicle;
};

struct config current_config = {0};
pthread_mutex_t current_config_mutex;

void log_config(struct config *c){
	#define PRINT_OVERRIDE_FLOAT(key) {\
		LOG("override " STR(key) ": %f\n", c->o.key);\
	}
	PRINT_OVERRIDE_FLOAT(gravity);
	PRINT_OVERRIDE_FLOAT(min_extra_gravity);
	PRINT_OVERRIDE_FLOAT(max_extra_gravity);
	PRINT_OVERRIDE_FLOAT(extra_gravity_accel_duration);
	PRINT_OVERRIDE_FLOAT(extra_gravity_accel_delay);

	#define PRINT_MULTIPLIER_FLOAT(key) {\
		LOG("multiplier " STR(key) ": %f\n", c->m.key);\
	}
	PRINT_MULTIPLIER_FLOAT(suspension_length_front);
	PRINT_MULTIPLIER_FLOAT(suspension_length_rear);
	PRINT_MULTIPLIER_FLOAT(dampers_front);
	PRINT_MULTIPLIER_FLOAT(dampers_rear);
	PRINT_MULTIPLIER_FLOAT(ride_height_front);
	PRINT_MULTIPLIER_FLOAT(ride_height_rear);
	PRINT_MULTIPLIER_FLOAT(anti_roll_bar_front);
	PRINT_MULTIPLIER_FLOAT(anti_roll_bar_rear);
	PRINT_MULTIPLIER_FLOAT(anti_roll_bar_damping_front);
	PRINT_MULTIPLIER_FLOAT(anti_roll_bar_damping_rear);
	PRINT_MULTIPLIER_FLOAT(lift_drag_ratio);
	PRINT_MULTIPLIER_FLOAT(down_force_velocity);
	PRINT_MULTIPLIER_FLOAT(down_force_front);
	PRINT_MULTIPLIER_FLOAT(down_force_rear);
	PRINT_MULTIPLIER_FLOAT(lateral_grip_front);
	PRINT_MULTIPLIER_FLOAT(lateral_grip_rear);
	PRINT_MULTIPLIER_FLOAT(grip_front);
	PRINT_MULTIPLIER_FLOAT(grip_rear);
	#undef PRINT_OVERRIDE_FLOAT
}

void parse_config(){
	int config_fd = open("./tdu2_physics_tweaks_config.json", O_BINARY | O_RDONLY);
	if(config_fd < 0){
		LOG("failed opening tdu2_physics_tweaks_config.json for reading, ending process :(\n");
		exit(1);
	}

	int file_size = lseek(config_fd, 0, SEEK_END);
	if(file_size < 0){
		LOG("failed fetching config file size, ending process :(\n");
		exit(1);
	}

	int seek_result = lseek(config_fd, 0, SEEK_SET);
	if(seek_result < 0){
		LOG("failed rewinding config file, ending process :(\n");
		exit(1);
	}

	char *buffer = (char *)malloc(file_size);
	if(buffer == NULL){
		LOG("failed allocating buffer for reading the config file, ending process :(\n");
		exit(1);
	}

	int bytes_read = read_data_from_fd(config_fd, buffer, file_size);
	if(bytes_read < 0){
		LOG("failed reading tdu2_physics_tweaks_config.json, ending process :(\n");
		exit(1);
	}

	struct config incoming_config = {0};
	json parsed_config;
	try{
		parsed_config = json::parse(std::string(buffer, bytes_read));
	}catch(...){
		LOG("failed parsing tdu2_physics_tweaks_config.json, ending process :(\n");
		exit(1);
	}

	#define FETCH_BOOL(key) { \
		try{ \
			incoming_config.key = parsed_config.at(STR(key)); \
		}catch(...){ \
			LOG("failed fetching config " STR(key) " from json, ending process :(\n") \
		} \
	}
	FETCH_BOOL(only_modify_player_vehicle);
	#undef FETCH_BOOL

	#define FETCH_OVERRIDE(key) { \
		try{ \
			incoming_config.o.key = parsed_config.at("overrides").at(STR(key)); \
		}catch(...){ \
			LOG("failed fetching override " STR(key) " from json, ending process :(\n"); \
		} \
	}
	FETCH_OVERRIDE(gravity);
	FETCH_OVERRIDE(min_extra_gravity);
	FETCH_OVERRIDE(max_extra_gravity);
	FETCH_OVERRIDE(extra_gravity_accel_duration);
	FETCH_OVERRIDE(extra_gravity_accel_delay);
	#undef FETCH_OVERRIDE

	#define FETCH_MULTIPLIER(key) { \
		try{ \
			incoming_config.m.key = parsed_config.at("multipliers").at(STR(key)); \
		}catch(...){ \
			LOG("failed fetching multiplier " STR(key) " from json, ending process :(\n"); \
		} \
	}
	FETCH_MULTIPLIER(suspension_length_front);
	FETCH_MULTIPLIER(suspension_length_rear);
	FETCH_MULTIPLIER(dampers_front);
	FETCH_MULTIPLIER(dampers_rear);
	FETCH_MULTIPLIER(ride_height_front);
	FETCH_MULTIPLIER(ride_height_rear);
	FETCH_MULTIPLIER(anti_roll_bar_front);
	FETCH_MULTIPLIER(anti_roll_bar_rear);
	FETCH_MULTIPLIER(anti_roll_bar_damping_front);
	FETCH_MULTIPLIER(anti_roll_bar_damping_rear);
	FETCH_MULTIPLIER(lift_drag_ratio);
	FETCH_MULTIPLIER(down_force_velocity);
	FETCH_MULTIPLIER(down_force_front);
	FETCH_MULTIPLIER(down_force_rear);
	FETCH_MULTIPLIER(lateral_grip_front);
	FETCH_MULTIPLIER(lateral_grip_rear);
	FETCH_MULTIPLIER(grip_front);
	FETCH_MULTIPLIER(grip_rear);
	#undef FETCH_OVERRIDE

	if(memcmp(&current_config, &incoming_config, sizeof(struct config)) != 0){
		pthread_mutex_lock(&current_config_mutex);
		memcpy(&current_config, &incoming_config, sizeof(struct config));
		pthread_mutex_unlock(&current_config_mutex);
		log_config(&current_config);
	}

	free(buffer);
	close(config_fd);
}

void *f00bdb970_ref = (void *)0x00bdb970;
uint32_t (__attribute__((stdcall)) *f00bdb970_orig)(uint32_t, uint32_t, uint32_t, uint32_t);
uint32_t __attribute__((stdcall)) f00bdb970_patched(uint32_t source, uint32_t unknown_1, uint32_t unknown_2, uint32_t unknown_3){
	LOG_VERBOSE("converting grip/aero performance data from 0x%08x, unknown_1 0x%08x, unknown_2 0x%08x, unknown_3 0x%08x\n", source, unknown_1, unknown_2, unknown_3);
	LOG_VERBOSE("return stack 0x%08x -> 0x%08x -> 0x%08x -> 0x%08x -> 0x%08x -> 0x%08x\n", __builtin_return_address(0), __builtin_return_address(1), __builtin_return_address(2), __builtin_return_address(3), __builtin_return_address(4), __builtin_return_address(5));

	if(__builtin_return_address(5) != (void *)0x00a943b7 && __builtin_return_address(5) != (void *)0x009d5ade && current_config.only_modify_player_vehicle){
		return f00bdb970_orig(source, unknown_1, unknown_2, unknown_3);
	}

	float *source_lift_drag_ratio = (float *)(source + 0x230);

	float *source_down_force_velocity = (float *)(source + 0x23c);
	float *source_down_force_front = (float *)(source + 0x234);
	float *source_down_force_rear = (float *)(source + 0x238);

	float *source_lateral_grip_front = (float *)(source + 0x244);
	float *source_lateral_grip_rear = (float *)(source + 0x248);
	float *source_grip_front = (float *)(source + 0x254);
	float *source_grip_rear = (float *)(source + 0x258);


	float lift_drag_ratio = *source_lift_drag_ratio;

	float down_force_velocity = *source_down_force_velocity;
	float down_force_front = *source_down_force_front;
	float down_force_rear = *source_down_force_rear;

	float lateral_grip_front = *source_lateral_grip_front;
	float lateral_grip_rear = *source_lateral_grip_rear;
	float grip_front = *source_grip_front;
	float grip_rear  = *source_grip_rear;

	pthread_mutex_lock(&current_config_mutex);
	*source_lift_drag_ratio *= current_config.m.lift_drag_ratio;

	*source_down_force_velocity *= current_config.m.down_force_velocity;
	*source_down_force_front *= current_config.m.down_force_front;
	*source_down_force_rear *= current_config.m.down_force_rear;

	*source_lateral_grip_front *= current_config.m.lateral_grip_front;
	*source_lateral_grip_rear *= current_config.m.lateral_grip_rear;
	*source_grip_front *= current_config.m.grip_front;
	*source_grip_rear *= current_config.m.grip_rear;
	pthread_mutex_unlock(&current_config_mutex);

	uint32_t ret = f00bdb970_orig(source, unknown_1, unknown_2, unknown_3);

	LOG("source lift drag ratio %f\n", *source_lift_drag_ratio);
	LOG("source down force velocity %f\n", *source_down_force_velocity);
	LOG("source down force front %f\n", *source_down_force_front);
	LOG("source down force rear %f\n", *source_down_force_rear);
	LOG("source lateral grip front %f\n", *source_lateral_grip_front);
	LOG("source lateral grip rear %f\n", *source_lateral_grip_rear);
	LOG("source grip front %f\n", *source_grip_front);
	LOG("source grip rear %f\n", *source_grip_rear);

	*source_lift_drag_ratio = lift_drag_ratio;

	*source_down_force_velocity = down_force_velocity;
	*source_down_force_front = down_force_front;
	*source_down_force_rear = down_force_rear;

	*source_lateral_grip_front = lateral_grip_front;
	*source_lateral_grip_rear = lateral_grip_rear;
	*source_grip_front = grip_front;
	*source_grip_rear = grip_rear;

	return ret;
}

void *f00df3b30_ref = (void *)0x00df3b30;
uint32_t (__attribute__((stdcall)) *f00df3b30_orig)(uint32_t target, uint32_t source, uint32_t unknown);
uint32_t __attribute__((stdcall)) f00df3b30_patched(uint32_t target, uint32_t source, uint32_t unknown){
	LOG_VERBOSE("converting suspension performance data from 0x%08x to 0x%08x, unknown argument is 0x%08x\n", source, target, unknown);
	LOG_VERBOSE("return stack 0x%08x -> 0x%08x -> 0x%08x -> 0x%08x -> 0x%08x -> 0x%08x\n", __builtin_return_address(0), __builtin_return_address(1), __builtin_return_address(2), __builtin_return_address(3), __builtin_return_address(4), __builtin_return_address(5));

	uint32_t ret = f00df3b30_orig(target, source, unknown);

	if(__builtin_return_address(4) != (void *)0x00a943b7 && __builtin_return_address(4) != (void *)0x009d5ade && current_config.only_modify_player_vehicle){
		return ret;
	}

	float *converted_suspension_length_front = (float *)(target + 0x44);
	float *converted_suspension_length_rear = (float *)(target + 0x48);

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

	pthread_mutex_lock(&current_config_mutex);
	*converted_suspension_length_front *= current_config.m.suspension_length_front;
	*converted_suspension_length_rear *= current_config.m.suspension_length_rear;

	*converted_dampers_front_1 *= current_config.m.dampers_front;
	*converted_dampers_rear_1 *= current_config.m.dampers_rear;
	*converted_dampers_front_2 *= current_config.m.dampers_front;
	*converted_dampers_rear_2 *= current_config.m.dampers_rear;

	*converted_ride_height_front *= current_config.m.ride_height_front;
	*converted_ride_height_rear *= current_config.m.ride_height_rear;

	*converted_anti_roll_bar_front *= current_config.m.anti_roll_bar_front;
	*converted_anti_roll_bar_rear *= current_config.m.anti_roll_bar_rear;

	*converted_anti_roll_bar_damping_front *= current_config.m.anti_roll_bar_damping_front;
	*converted_anti_roll_bar_damping_rear *= current_config.m.anti_roll_bar_damping_rear;
	pthread_mutex_unlock(&current_config_mutex);

	LOG("converted suspension length front %f\n", *converted_suspension_length_front);
	LOG("converted suspension length rear %f\n", *converted_suspension_length_rear);

	LOG("converted damper front 1 %f\n", *converted_dampers_front_1);
	LOG("converted damper rear 1 %f\n", *converted_dampers_rear_1);
	LOG("converted damper front 2 %f\n", *converted_dampers_front_2);
	LOG("converted damper rear 2 %f\n", *converted_dampers_rear_2);

	LOG("converted ride height front %f\n", *converted_ride_height_front);
	LOG("converted ride height rear %f\n", *converted_ride_height_rear);

	LOG("converted anti roll bar front %f\n", *converted_anti_roll_bar_front);
	LOG("converted anti roll bar rear %f\n", *converted_anti_roll_bar_rear);

	LOG("converted anti roll bar damping front %f\n", *converted_anti_roll_bar_damping_front);
	LOG("converted anti roll bar damping rear %f\n", *converted_anti_roll_bar_damping_rear);

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

	pthread_mutex_lock(&current_config_mutex);
	*min_extra_gravity = current_config.o.min_extra_gravity;
	*max_extra_gravity = current_config.o.max_extra_gravity;
	*extra_gravity_accel_duration = current_config.o.extra_gravity_accel_duration;
	*extra_gravity_accel_delay = current_config.o.extra_gravity_accel_delay;
	pthread_mutex_unlock(&current_config_mutex);

	if(!logged){
		LOG_VERBOSE("extra gravity min %f, max %f, duration %f, delay %f\n", *min_extra_gravity, *max_extra_gravity, *extra_gravity_accel_duration, *extra_gravity_accel_delay);
		logged = true;
	}
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
	pthread_mutex_lock(&current_config_mutex);
	gravity = current_config.o.gravity;
	pthread_mutex_unlock(&current_config_mutex);
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

void *config_parser_loop(void *args){
	while(true){
		sleep(2);
		parse_config();
	}
	return NULL;
}

// entrypoint
__attribute__((constructor))
int init(){
	init_logging();
	pthread_mutex_init(&current_config_mutex, NULL);
	LOG("log initialized\n");

	parse_config();
	LOG("done parsing initial config\n");

	patch_gravity();
	LOG("done patching gravity\n");

	pthread_t delayed_init_thread;
	pthread_create(&delayed_init_thread, NULL, delayed_init, NULL);

	pthread_t config_reparse_thread;
	pthread_create(&config_reparse_thread, NULL, config_parser_loop, NULL);

	return 0;
}
