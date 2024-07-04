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

int write_data_to_fd(int fd, const char *buffer, int len){
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

struct global_store{
	float brake_pedal_level;
};

struct global_store global = {0};

struct overrides{
	// sets on boot only, part of the physics engine seems obfuscated and rather annoying to hook run time
	float gravity;

	// overrides Physics.cpr, applies right away
	float min_extra_gravity;
	float max_extra_gravity;
	float extra_gravity_accel_duration;
	float extra_gravity_accel_delay;

	// beta, current investigation seems to suggest that it does not even derive brake wheel slip from brake power and pedal input
	bool abs_off;
	bool tcs_off;
	bool hand_brake_abs_off;

	// override angular damping values in Physics.cpr
	bool override_angular_damping;
	float new_angular_damping;
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

	float brake_power;
};

struct config{
	struct overrides o;
	struct multipliers m;
	bool only_modify_player_vehicle;
};

struct config current_config = {0};
pthread_mutex_t current_config_mutex;

void log_config(struct config *c){
	#define PRINT_SETTING_BOOL(key) {\
		LOG("setting " STR(key) ": %s\n", c->key? "true" : "false");\
	}
	PRINT_SETTING_BOOL(only_modify_player_vehicle);

	#define PRINT_OVERRIDE_FLOAT(key) {\
		LOG("override " STR(key) ": %f\n", c->o.key);\
	}
	#define PRINT_OVERRIDE_BOOL(key) {\
		LOG("override " STR(key) ": %s\n", c->o.key? "true": "false");\
	}
	PRINT_OVERRIDE_FLOAT(gravity);
	PRINT_OVERRIDE_FLOAT(min_extra_gravity);
	PRINT_OVERRIDE_FLOAT(max_extra_gravity);
	PRINT_OVERRIDE_FLOAT(extra_gravity_accel_duration);
	PRINT_OVERRIDE_FLOAT(extra_gravity_accel_delay);
	PRINT_OVERRIDE_BOOL(override_angular_damping);
	PRINT_OVERRIDE_FLOAT(new_angular_damping);

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
	PRINT_MULTIPLIER_FLOAT(brake_power);
	#undef PRINT_OVERRIDE_FLOAT
	#undef PRINT_MULTIPLIER_FLOAT
	#undef PRINT_SETTING_BOOL
}

void parse_config(){
	const char *config_path = "./tdu2_physics_tweaks_config.json";
	bool updated = false;
	json parsed_config;

	int config_fd = open(config_path, O_BINARY | O_RDONLY);
	if(config_fd < 0){
		LOG("warning, failed opening %s for reading, using defaults\n", config_path);
	}

	while(config_fd >= 0){
		int file_size = lseek(config_fd, 0, SEEK_END);
		if(file_size < 0){
			LOG("failed fetching config file size, using defaults\n");
			break;
		}

		int seek_result = lseek(config_fd, 0, SEEK_SET);
		if(seek_result < 0){
			LOG("failed rewinding config file, using defaults\n");
			break;
		}

		char *buffer = (char *)malloc(file_size);
		if(buffer == NULL){
			LOG("failed allocating buffer for reading the config file, using defaults\n");
			free(buffer);
			break;
		}

		int bytes_read = read_data_from_fd(config_fd, buffer, file_size);
		if(bytes_read < 0){
			LOG("failed reading tdu2_physics_tweaks_config.json, using defaults\n");
			free(buffer);
			break;
		}

		try{
			parsed_config = json::parse(std::string(buffer, bytes_read));
		}catch(...){
			LOG("failed parsing tdu2_physics_tweaks_config.json, using defaults\n");
			parsed_config = {};
			free(buffer);
			break;
		}
		free(buffer);
		break;
	}
	if(config_fd >= 0){
		close(config_fd);
	}

	struct config incoming_config = {0};

	#define FETCH_SETTING(key, d) { \
		try{ \
			incoming_config.key = parsed_config.at(STR(key)); \
		}catch(...){ \
			LOG("warning: failed fetching config " STR(key) " from json, adding default") \
			updated = true; \
			incoming_config.key = d; \
			parsed_config[STR(key)] = d; \
		} \
	}
	FETCH_SETTING(only_modify_player_vehicle, true);
	#undef FETCH_SETTING

	#define FETCH_OVERRIDE(key, d) { \
		try{ \
			incoming_config.o.key = parsed_config.at("overrides").at(STR(key)); \
		}catch(...){ \
			LOG("warning: failed fetching override " STR(key) " from json, adding default\n"); \
			updated = true; \
			incoming_config.o.key = d; \
			parsed_config["overrides"][STR(key)] = d; \
		} \
	}
	FETCH_OVERRIDE(gravity, -9.81);
	FETCH_OVERRIDE(min_extra_gravity, 0.1);
	FETCH_OVERRIDE(max_extra_gravity, 1.0);
	FETCH_OVERRIDE(extra_gravity_accel_duration, 0.1);
	FETCH_OVERRIDE(extra_gravity_accel_delay, 0.1);
	FETCH_OVERRIDE(abs_off, false);
	FETCH_OVERRIDE(tcs_off, false);
	FETCH_OVERRIDE(hand_brake_abs_off, false);
	FETCH_OVERRIDE(override_angular_damping, false);
	FETCH_OVERRIDE(new_angular_damping, 0.0);
	#undef FETCH_OVERRIDE

	#define FETCH_MULTIPLIER(key, d) { \
		try{ \
			incoming_config.m.key = parsed_config.at("multipliers").at(STR(key)); \
		}catch(...){ \
			LOG("failed fetching multiplier " STR(key) " from json, adding default\n"); \
			updated = true; \
			incoming_config.m.key = d; \
			parsed_config["multipliers"][STR(key)] = d; \
		} \
	}
	FETCH_MULTIPLIER(suspension_length_front, 1.0);
	FETCH_MULTIPLIER(suspension_length_rear, 1.0);
	FETCH_MULTIPLIER(dampers_front, 1.0);
	FETCH_MULTIPLIER(dampers_rear, 1.0);
	FETCH_MULTIPLIER(ride_height_front, 1.0);
	FETCH_MULTIPLIER(ride_height_rear, 1.0);
	FETCH_MULTIPLIER(anti_roll_bar_front, 1.0);
	FETCH_MULTIPLIER(anti_roll_bar_rear, 1.0);
	FETCH_MULTIPLIER(anti_roll_bar_damping_front, 1.0);
	FETCH_MULTIPLIER(anti_roll_bar_damping_rear, 1.0);
	FETCH_MULTIPLIER(lift_drag_ratio, 1.0);
	FETCH_MULTIPLIER(down_force_velocity, 1.0);
	FETCH_MULTIPLIER(down_force_front, 1.0);
	FETCH_MULTIPLIER(down_force_rear, 1.0);
	FETCH_MULTIPLIER(lateral_grip_front, 1.0);
	FETCH_MULTIPLIER(lateral_grip_rear, 1.0);
	FETCH_MULTIPLIER(grip_front, 1.0);
	FETCH_MULTIPLIER(grip_rear, 1.0);
	FETCH_MULTIPLIER(brake_power, 1.0);
	#undef FETCH_MULTIPLIER

	if(memcmp(&current_config, &incoming_config, sizeof(struct config)) != 0){
		pthread_mutex_lock(&current_config_mutex);
		memcpy(&current_config, &incoming_config, sizeof(struct config));
		pthread_mutex_unlock(&current_config_mutex);
		log_config(&current_config);
	}

	if(!updated){
		return;
	}
	config_fd = open(config_path, O_BINARY | O_WRONLY | O_TRUNC | O_CREAT);

	if(config_fd < 0){
		LOG("warning: failed opening %s for updating, please make sure the file is not readonly\n", config_path);
		return;
	}

	std::string out_json = parsed_config.dump(4);
	int write_result = write_data_to_fd(config_fd, out_json.c_str(), out_json.size());
	if(write_result != out_json.size()){
		LOG("warning: failed writing updated config to %s\n", config_path);
	}
	close(config_fd);
}

// unknown call convention, crashes
/*
void *f00bbc650_ref = (void *)0x00bbc650;
void (__attribute__((thiscall)) *f00bbc650_orig)(uint32_t unknown_1, uint32_t unknown_2, uint32_t unknown_3);
void __attribute__((thiscall)) f00bbc650_patched(uint32_t unknown_1, uint32_t unknown_2, uint32_t unknown_3){
	f00bbc650_orig(unknown_1, unknown_2, unknown_3);

	float *brake_pedal_value = (float *)(unknown_1 + 0x284);
	global.brake_pedal_value = *brake_pedal_value;
	//if(__builtin_return_address(1) == (void *)0x2a86ea10){
	if(global.brake_pedal_value > 0.1){
		LOG_VERBOSE("brake pedal value %f\n", global.brake_pedal_value);
		LOG_VERBOSE("return stack 0x%08x -> 0x%08x -> 0x%08x -> 0x%08x -> 0x%08x -> 0x%08x\n", __builtin_return_address(0), __builtin_return_address(1), __builtin_return_address(2), __builtin_return_address(3), __builtin_return_address(4), __builtin_return_address(5));
	}
}
*/

// can't easily identify player
/*
void *f0086ad90_ref = (void *)0x0086ad90;
void (__attribute__((fastcall)) *f0086ad90_orig)(uint32_t unknown_1);
void __attribute__((fastcall)) f0086ad90_patched(uint32_t unknown_1){
	float *brake_pedal_value = (float *)(*(uint32_t *)(unknown_1 + 8) + 0xd4);
	global.brake_pedal_value = *brake_pedal_value;
	//if(__builtin_return_address(1) == (void *)0x2a86ea10){
	if(global.brake_pedal_value > 0.1){
		LOG_VERBOSE("brake pedal value %f\n", global.brake_pedal_value);
		LOG_VERBOSE("return stack 0x%08x -> 0x%08x -> 0x%08x -> 0x%08x -> 0x%08x -> 0x%08x\n", __builtin_return_address(0), __builtin_return_address(1), __builtin_return_address(2), __builtin_return_address(3), __builtin_return_address(4), __builtin_return_address(5));
	}
	f0086ad90_orig(unknown_1);
}
*/

float calculate_new_slip_ratio(float brake_pedal_level){
	// the current observation is pretty cursed
	// the slip ratio that affects turning is almost constant to the value given by db here, no matter what brake input is given with different brake power values
	// scale slip ratio here against brake pedal level to simulate lock up

	// make it gran turismo 6-ish, only full locks at like 80%
	// 0 to -0.2 from 0% to 75%
	// -0.2 to -0.7 from 75% to 100%

	// sadly weight transfer in tdu2 doesn't really increase front grip in a noticible way

	float lower = brake_pedal_level >= 0.75? 1.0: brake_pedal_level / 0.75;
	float upper = brake_pedal_level > 0.75? (brake_pedal_level - 0.75) / 0.25: 0;
	return lower * (-0.2) + upper * (-0.5);
}

void *f00bacd50_ref = (void *)0x00bacd50;
void (__attribute__((stdcall)) *f00bacd50_orig)(uint32_t unknown_1, uint32_t unknown_2, uint32_t unknown_3);
void __attribute__((stdcall)) f00bacd50_patched(uint32_t unknown_1, uint32_t unknown_2, uint32_t unknown_3){
	LOG_VERBOSE("fixing brake wheel locking mechanism, unknown_1 0x%08x, unknown_2 0x%08x, unknown_3 0x%08x\n", unknown_1, unknown_2, unknown_3);
	LOG_VERBOSE("return stack 0x%08x -> 0x%08x -> 0x%08x -> 0x%08x\n", __builtin_return_address(0), __builtin_return_address(1), __builtin_return_address(2), __builtin_return_address(3));

	float *slip_ratio_hypersport = (float *)(unknown_1 + 0x68);
	float *slip_ratio_off = (float *)(unknown_1 + 0x78);
	float *slip_ratio_secure = (float *)(unknown_1 + 0x60);
	float *slip_ratio_sport = (float *)(unknown_1 + 0x64);
	float *brake_pedal_level = (float *)(unknown_3 + 0x4);

	// flagged as player from 00baddd0 hook
	if(900000 > *slip_ratio_hypersport){
		f00bacd50_orig(unknown_1, unknown_2, unknown_3);
		return;
	}

	global.brake_pedal_level = *brake_pedal_level;

	#define BACKUP_FLOAT(key) \
		float bak_##key = *key;
	BACKUP_FLOAT(slip_ratio_hypersport);
	BACKUP_FLOAT(slip_ratio_off);
	BACKUP_FLOAT(slip_ratio_secure);
	BACKUP_FLOAT(slip_ratio_sport);
	BACKUP_FLOAT(brake_pedal_level);
	#undef BACKUP_FLOAT

	// moved to 0087ebf0 hook, which is called later by the original function
	/*
	float lower = *brake_pedal_level >= 0.75? 1.0: *brake_pedal_level / 0.75;
	float upper = *brake_pedal_level > 0.75? (*brake_pedal_level - 0.75) / 0.25: 0;
	float new_slip_ratio = calculate_new_slip_ratio(*brake_pedal_level);

	*slip_ratio_hypersport = new_slip_ratio;
	*slip_ratio_off = new_slip_ratio;
	*slip_ratio_secure = new_slip_ratio;
	*slip_ratio_sport = new_slip_ratio;
	*/

	LOG_VERBOSE("slip ratio hypersport %f\n", *slip_ratio_hypersport);
	LOG_VERBOSE("slip ratio off %f\n", *slip_ratio_off);
	LOG_VERBOSE("slip ratio secure %f\n", *slip_ratio_secure);
	LOG_VERBOSE("slip ratio sport %f\n", *slip_ratio_sport);
	LOG_VERBOSE("brake pedal level %f\n", *brake_pedal_level);
	f00bacd50_orig(unknown_1, unknown_2, unknown_3);

	#define RESTORE_VALUE(key) \
		*key = bak_##key;
	RESTORE_VALUE(slip_ratio_hypersport);
	RESTORE_VALUE(slip_ratio_off);
	RESTORE_VALUE(slip_ratio_secure);
	RESTORE_VALUE(slip_ratio_sport);
	RESTORE_VALUE(brake_pedal_level);
	#undef RESTORE_VALUE

	return;
}

// replaces 00bdb970 hooking currently
void *f00baddd0_ref = (void *)0x00baddd0;
void (__attribute__((stdcall)) *f00baddd0_orig)(uint32_t, uint32_t, uint32_t, uint32_t, uint32_t, uint32_t);
void __attribute__((stdcall)) f00baddd0_patched(uint32_t unknown_1, uint32_t unknown_2, uint32_t unknown_3, uint32_t unknown_4, uint32_t unknown_5, uint32_t unknown_6){
	LOG_VERBOSE("converting grip/aero performance, unknown_1 0x%08x, unknown_2 0x%08x, unknown_3 0x%08x, unknown_4 0x%08x, unknown_5 0x%08x, unknown_6 0x%08x\n", unknown_1, unknown_2, unknown_3, unknown_4, unknown_5, unknown_6);
	LOG_VERBOSE("return stack 0x%08x -> 0x%08x -> 0x%08x -> 0x%08x -> 0x%08x -> 0x%08x\n", __builtin_return_address(0), __builtin_return_address(1), __builtin_return_address(2), __builtin_return_address(3), __builtin_return_address(4), __builtin_return_address(5));

	bool is_player = __builtin_return_address(4) == (void *)0x00a943b7 || __builtin_return_address(4) == (void *)0x009d5ade;

	if(!is_player && current_config.only_modify_player_vehicle){
		f00baddd0_orig(unknown_1, unknown_2, unknown_3, unknown_4, unknown_5, unknown_6);
		return;
	}

	// unknown_2 is likely car db
	float *source_lift_drag_ratio = (float *)(unknown_2 + 0x230);

	float *source_down_force_velocity = (float *)(unknown_2 + 0x23c);
	float *source_down_force_front = (float *)(unknown_2 + 0x234);
	float *source_down_force_rear = (float *)(unknown_2 + 0x238);

	float *source_lateral_grip_front = (float *)(unknown_2 + 0x244);
	float *source_lateral_grip_rear = (float *)(unknown_2 + 0x248);
	float *source_grip_front = (float *)(unknown_2 + 0x254);
	float *source_grip_rear = (float *)(unknown_2 + 0x258);

	float *source_car_abs_slip_ratio_hypersport = (float *)(unknown_2 + 0x4);
	float *source_car_abs_slip_ratio_off = (float *)(unknown_2 + 0x8);
	float *source_car_abs_slip_ratio_secure = (float *)(unknown_2 + 0xc);
	float *source_car_abs_slip_ratio_sport = (float *)(unknown_2 + 0x10);

	float *source_car_tcs_slip_ratio_hypersport = (float *)(unknown_2 + 0x408);
	// off seems unused entirely
	float *source_car_tcs_slip_ratio_off = (float *)(unknown_2 + 0x40c);
	float *source_car_tcs_slip_ratio_secure = (float *)(unknown_2 + 0x410);
	float *source_car_tcs_slip_ratio_sport = (float *)(unknown_2 + 0x414);

	float *source_car_hand_brake_slip_ratio = (float *)(unknown_2 + 0x1ec);

	int *source_brake_power = (int *)(unknown_2 + 0x68);

	// unknown_4 is likely parsed Physics.cpr
	float *source_abs_min_velocity = (float *)(unknown_4 + 0x338);
	float *source_abs_max_velocity = (float *)(unknown_4 + 0x33c);
	float *source_abs_slip_ratio_secure = (float *)(unknown_4 + 0x340);
	float *source_abs_slip_ratio_sport = (float *)(unknown_4 + 0x344);
	float *source_abs_slip_ratio_hypersport = (float *)(unknown_4 + 0x348);
	float *source_tcs_min_velocity = (float *)(unknown_4 + 0x360);
	float *source_tcs_max_velocity = (float *)(unknown_4 + 0x364);
	float *source_tcs_slip_ratio_secure = (float *)(unknown_4 + 0x368);
	float *source_tcs_slip_ratio_sport = (float *)(unknown_4 + 0x36c);
	float *source_tcs_slip_ratio_hypersport = (float *)(unknown_4 + 0x370);
	float *source_hand_brake_min_velocity = (float *)(unknown_4 + 0x34c);
	float *source_hand_brake_max_velocity = (float *)(unknown_4 + 0x350);
	float *source_hand_brake_slip_ratio_secure = (float *)(unknown_4 + 0x354);
	float *source_hand_brake_slip_ratio_sport = (float *)(unknown_4 + 0x358);
	float *source_hand_brake_slip_ratio_hypersport = (float *)(unknown_4 + 0x35c);

	float *angular_damping_off_array = (float *)(unknown_4 + 0x1ec);
	float *angular_damping_secure_array = (float *)(unknown_4 + 0x1ec + 10 * 4);
	float *angular_damping_sport_array = (float *)(unknown_4 + 0x1ec + 10 * 4 * 2);
	float *angular_damping_hypersport_array = (float *)(unknown_4 + 0x1ec + 10 * 4 * 3);

	static float bak_angular_damping[10 * 4];
	static bool angular_damping_backed_up = false;
	if(!angular_damping_backed_up){
		memcpy(bak_angular_damping, angular_damping_off_array, sizeof(bak_angular_damping));
		angular_damping_backed_up = true;
	}

	#define BACKUP_FLOAT(name) \
		float bak_##name = *source_##name;

	#define BACKUP_INT(name) \
		int bak_##name = *source_##name;

	BACKUP_FLOAT(lift_drag_ratio);

	BACKUP_FLOAT(down_force_velocity);
	BACKUP_FLOAT(down_force_front);
	BACKUP_FLOAT(down_force_rear);

	BACKUP_FLOAT(lateral_grip_front);
	BACKUP_FLOAT(lateral_grip_rear);
	BACKUP_FLOAT(grip_front);
	BACKUP_FLOAT(grip_rear);
	BACKUP_FLOAT(car_abs_slip_ratio_hypersport);
	BACKUP_FLOAT(car_abs_slip_ratio_off);
	BACKUP_FLOAT(car_abs_slip_ratio_secure);
	BACKUP_FLOAT(car_abs_slip_ratio_sport);

	BACKUP_FLOAT(car_tcs_slip_ratio_hypersport);
	BACKUP_FLOAT(car_tcs_slip_ratio_off);
	BACKUP_FLOAT(car_tcs_slip_ratio_secure);
	BACKUP_FLOAT(car_tcs_slip_ratio_sport);

	BACKUP_FLOAT(car_hand_brake_slip_ratio);

	BACKUP_INT(brake_power);

	BACKUP_FLOAT(abs_min_velocity);
	BACKUP_FLOAT(abs_max_velocity);
	BACKUP_FLOAT(abs_slip_ratio_secure);
	BACKUP_FLOAT(abs_slip_ratio_sport);
	BACKUP_FLOAT(abs_slip_ratio_hypersport);
	BACKUP_FLOAT(tcs_min_velocity);
	BACKUP_FLOAT(tcs_max_velocity);
	BACKUP_FLOAT(tcs_slip_ratio_secure);
	BACKUP_FLOAT(tcs_slip_ratio_sport);
	BACKUP_FLOAT(tcs_slip_ratio_hypersport);
	BACKUP_FLOAT(hand_brake_min_velocity);
	BACKUP_FLOAT(hand_brake_max_velocity);
	BACKUP_FLOAT(hand_brake_slip_ratio_secure);
	BACKUP_FLOAT(hand_brake_slip_ratio_sport);
	BACKUP_FLOAT(hand_brake_slip_ratio_hypersport);

	#undef BACKUP_FLOAT
	#undef BACKUP_INT

	pthread_mutex_lock(&current_config_mutex);
	*source_lift_drag_ratio *= current_config.m.lift_drag_ratio;

	*source_down_force_velocity *= current_config.m.down_force_velocity;
	*source_down_force_front *= current_config.m.down_force_front;
	*source_down_force_rear *= current_config.m.down_force_rear;

	*source_lateral_grip_front *= current_config.m.lateral_grip_front;
	*source_lateral_grip_rear *= current_config.m.lateral_grip_rear;
	*source_grip_front *= current_config.m.grip_front;
	*source_grip_rear *= current_config.m.grip_rear;

	*source_brake_power = *source_brake_power * current_config.m.brake_power;
	pthread_mutex_unlock(&current_config_mutex);

	if(current_config.o.abs_off && is_player){
		*source_abs_min_velocity = 0;
		*source_abs_max_velocity = 0;

		*source_car_abs_slip_ratio_hypersport = 999990;
		*source_car_abs_slip_ratio_off = 999991;
		*source_car_abs_slip_ratio_secure = 999992;
		*source_car_abs_slip_ratio_sport = 999993;

		*source_abs_slip_ratio_secure = -0.2;
		*source_abs_slip_ratio_sport = -0.2;
		*source_abs_slip_ratio_hypersport = -0.2;
	}

	if(current_config.o.tcs_off && is_player){
		*source_tcs_min_velocity = 0;
		*source_tcs_max_velocity = 0;

		*source_car_tcs_slip_ratio_hypersport = -9999.0;
		*source_car_tcs_slip_ratio_off = -9999.0;
		*source_car_tcs_slip_ratio_secure = -9999.0;
		*source_car_tcs_slip_ratio_sport = -9999.0;

		*source_tcs_slip_ratio_secure = -9999.0;
		*source_tcs_slip_ratio_sport = -9999.0;
		*source_tcs_slip_ratio_hypersport = -9999.0;
	}

	if(current_config.o.hand_brake_abs_off && is_player){
		*source_hand_brake_min_velocity = 0;
		*source_hand_brake_max_velocity = 0;

		*source_car_hand_brake_slip_ratio = -0.2;

		*source_hand_brake_slip_ratio_secure = -0.6;
		*source_hand_brake_slip_ratio_sport = -0.6;
		*source_hand_brake_slip_ratio_hypersport = -0.6;
	}

	if(current_config.o.override_angular_damping){
		for(int i = 0;i < 4; i++){
			for(int j = 1;j < 10; j+=2){
				angular_damping_off_array[i * 10 + j] = current_config.o.new_angular_damping;
			}
		}
	}else{
		memcpy(angular_damping_off_array, bak_angular_damping, sizeof(bak_angular_damping));
	}

	f00baddd0_orig(unknown_1, unknown_2, unknown_3, unknown_4, unknown_5, unknown_6);

	LOG("source lift drag ratio %f\n", *source_lift_drag_ratio);

	LOG("source down force velocity %f\n", *source_down_force_velocity);
	LOG("source down force front %f\n", *source_down_force_front);
	LOG("source down force rear %f\n", *source_down_force_rear);

	LOG("source lateral grip front %f\n", *source_lateral_grip_front);
	LOG("source lateral grip rear %f\n", *source_lateral_grip_rear);
	LOG("source grip front %f\n", *source_grip_front);
	LOG("source grip rear %f\n", *source_grip_rear);

	LOG_VERBOSE("source abs slip ratio hypersport %f\n", *source_car_abs_slip_ratio_hypersport);
	LOG_VERBOSE("source abs slip ratio off %f\n", *source_car_abs_slip_ratio_off);
	LOG_VERBOSE("source abs slip ratio secure %f\n", *source_car_abs_slip_ratio_secure);
	LOG_VERBOSE("source abs slip ratio sport %f\n", *source_car_abs_slip_ratio_sport);

	LOG_VERBOSE("source tcs slip ratio hypersport %f\n", *source_car_tcs_slip_ratio_hypersport);
	LOG_VERBOSE("source tcs slip ratio off %f\n", *source_car_tcs_slip_ratio_off);
	LOG_VERBOSE("source tcs slip ratio secure %f\n", *source_car_tcs_slip_ratio_secure);
	LOG_VERBOSE("source tcs slip ratio sport %f\n", *source_car_tcs_slip_ratio_sport);

	LOG_VERBOSE("source hand brake slip ratio %f\n", *source_car_hand_brake_slip_ratio);

	LOG("source brake power %d\n", *source_brake_power);

	LOG_VERBOSE("source Physics.cpr abs min velocity %f\n", *source_abs_min_velocity);
	LOG_VERBOSE("source Physics.cpr abs max velocity %f\n", *source_abs_max_velocity);
	LOG_VERBOSE("source Physics.cpr abs slip ratio secure %f\n", *source_abs_slip_ratio_secure);
	LOG_VERBOSE("source Physics.cpr abs slip ratio sport %f\n", *source_abs_slip_ratio_sport);
	LOG_VERBOSE("source Physics.cpr abs slip ratio hypersport %f\n", *source_abs_slip_ratio_hypersport);
	LOG_VERBOSE("source Physics.cpr tcs min velocity %f\n", *source_tcs_min_velocity);
	LOG_VERBOSE("source Physics.cpr tcs max velocity %f\n", *source_tcs_max_velocity);
	LOG_VERBOSE("source Physics.cpr tcs slip ratio secure %f\n", *source_tcs_slip_ratio_secure);
	LOG_VERBOSE("source Physics.cpr tcs slip ratio sport %f\n", *source_tcs_slip_ratio_sport);
	LOG_VERBOSE("source Physics.cpr tcs slip ratio hypersport %f\n", *source_tcs_slip_ratio_hypersport);
	LOG_VERBOSE("source Physics.cpr hand brake min velocity %f\n", *source_hand_brake_min_velocity);
	LOG_VERBOSE("source Physics.cpr hand brake max velocity %f\n", *source_hand_brake_max_velocity);
	LOG_VERBOSE("source Physics.cpr hand brake slip ratio secure %f\n", *source_hand_brake_slip_ratio_secure);
	LOG_VERBOSE("source Physics.cpr hand brake slip ratio sport %f\n", *source_hand_brake_slip_ratio_sport);
	LOG_VERBOSE("source Physics.cpr hand brake slip ratio hypersport %f\n", *source_hand_brake_slip_ratio_hypersport);

	LOG("source angular damping table\n");
	for(int i = 0;i < 4; i++){
		for(int j = 0;j < 10; j++){
			LOG("%f ", angular_damping_off_array[i * 10 + j]);
		}
		LOG("\n");
	}

	#define RESTORE_VALUE(name) \
		*source_##name = bak_##name;

	RESTORE_VALUE(lift_drag_ratio);

	RESTORE_VALUE(down_force_velocity);
	RESTORE_VALUE(down_force_front);
	RESTORE_VALUE(down_force_rear);

	RESTORE_VALUE(lateral_grip_front);
	RESTORE_VALUE(lateral_grip_rear);
	RESTORE_VALUE(grip_front);
	RESTORE_VALUE(grip_rear);

	RESTORE_VALUE(car_abs_slip_ratio_hypersport);
	RESTORE_VALUE(car_abs_slip_ratio_off);
	RESTORE_VALUE(car_abs_slip_ratio_secure);
	RESTORE_VALUE(car_abs_slip_ratio_sport);

	RESTORE_VALUE(car_tcs_slip_ratio_hypersport);
	RESTORE_VALUE(car_tcs_slip_ratio_off);
	RESTORE_VALUE(car_tcs_slip_ratio_secure);
	RESTORE_VALUE(car_tcs_slip_ratio_sport);

	RESTORE_VALUE(car_hand_brake_slip_ratio);

	RESTORE_VALUE(brake_power);

	RESTORE_VALUE(abs_min_velocity);
	RESTORE_VALUE(abs_max_velocity);
	RESTORE_VALUE(abs_slip_ratio_secure);
	RESTORE_VALUE(abs_slip_ratio_sport);
	RESTORE_VALUE(abs_slip_ratio_hypersport);
	RESTORE_VALUE(tcs_min_velocity);
	RESTORE_VALUE(tcs_max_velocity);
	RESTORE_VALUE(tcs_slip_ratio_secure);
	RESTORE_VALUE(tcs_slip_ratio_sport);
	RESTORE_VALUE(tcs_slip_ratio_hypersport);
	RESTORE_VALUE(hand_brake_min_velocity);
	RESTORE_VALUE(hand_brake_max_velocity);
	RESTORE_VALUE(hand_brake_slip_ratio_secure);
	RESTORE_VALUE(hand_brake_slip_ratio_sport);
	RESTORE_VALUE(hand_brake_slip_ratio_hypersport);

	#undef RESTORE_VALUE

	return;
}

// replaced by 00baddd0 hooking currently
/*
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

	// unknown_1 is likely parsed Physics.cpr
	float *source_abs_min_velocity = (float *)(unknown_1 + 0x338);
	float *source_abs_max_velocity = (float *)(unknown_1 + 0x33c);
	float *source_abs_slip_ratio_secure = (float *)(unknown_1 + 0x340);
	float *source_abs_slip_ratio_sport = (float *)(unknown_1 + 0x344);
	float *source_abs_slip_ratio_hypersport = (float *)(unknown_1 + 0x348);
	float *source_tcs_min_velocity = (float *)(unknown_1 + 0x360);
	float *source_tcs_max_velocity = (float *)(unknown_1 + 0x364);
	float *source_tcs_slip_ratio_secure = (float *)(unknown_1 + 0x368);
	float *source_tcs_slip_ratio_sport = (float *)(unknown_1 + 0x36c);
	float *source_tcs_slip_ratio_hypersport = (float *)(unknown_1 + 0x370);

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
	LOG_VERBOSE("source abs min velocity %f\n", *source_abs_min_velocity);
	LOG_VERBOSE("source abs max velocity %f\n", *source_abs_max_velocity);
	LOG_VERBOSE("source abs slip ratio secure %f\n", *source_abs_slip_ratio_secure);
	LOG_VERBOSE("source abs slip ratio sport %f\n", *source_abs_slip_ratio_sport);
	LOG_VERBOSE("source abs slip ratio hypersport %f\n", *source_abs_slip_ratio_hypersport);
	LOG_VERBOSE("source tcs min velocity %f\n", *source_tcs_min_velocity);
	LOG_VERBOSE("source tcs max velocity %f\n", *source_tcs_max_velocity);
	LOG_VERBOSE("source tcs slip ratio secure %f\n", *source_tcs_slip_ratio_secure);
	LOG_VERBOSE("source tcs slip ratio sport %f\n", *source_tcs_slip_ratio_sport);
	LOG_VERBOSE("source tcs slip ratio hypersport %f\n", *source_tcs_slip_ratio_hypersport);


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
*/

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
	// this function gets called by the hooked 00bacd50

	LOG_VERBOSE("changing extra gravity and slip ratio, context 0x%08x\n", context);
	LOG_VERBOSE("return stack 0x%08x -> 0x%08x -> 0x%08x -> 0x%08x -> 0x%08x -> 0x%08x\n", __builtin_return_address(0), __builtin_return_address(1), __builtin_return_address(2), __builtin_return_address(3), __builtin_return_address(4), __builtin_return_address(5));

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

	LOG_VERBOSE("extra gravity min %f, max %f, duration %f, delay %f\n", *min_extra_gravity, *max_extra_gravity, *extra_gravity_accel_duration, *extra_gravity_accel_delay);

	float *slip_ratio = (float *)(context - 0x154);
	f0087ebf0_orig(context);
	float slip_ratio_orig = *slip_ratio;
	// flagged for slip ratio modification
	if(900000 < *slip_ratio){
		*slip_ratio = calculate_new_slip_ratio(global.brake_pedal_level);
		LOG_VERBOSE("overriding slip ratio from %f to %f\n", slip_ratio_orig, *slip_ratio);
	}
}

int hook_functions(){
	LOG("hook_functions begin\n");

	int ret = 0;

	ret = MH_Initialize();
	if(ret != MH_OK && ret != MH_ERROR_ALREADY_INITIALIZED){
		LOG("Failed initializing MinHook, %d\n", ret);
		return -1;
	}

	/*
	ret = MH_CreateHook(f00bbc650_ref, (LPVOID)&f00bbc650_patched, (LPVOID *)&f00bbc650_orig);
	if(ret != MH_OK){
		LOG("Failed creating hook for 0x00bbc650, %d\n", ret);
		return -1;
	}
	ret = MH_EnableHook(f00bbc650_ref);
	if(ret != MH_OK){
		LOG("Failed enableing hook for 0x00bbc650, %d\n", ret);
		return -1;
	}*/

	/*
	ret = MH_CreateHook(f0086ad90_ref, (LPVOID)&f0086ad90_patched, (LPVOID *)&f0086ad90_orig);
	if(ret != MH_OK){
		LOG("Failed creating hook for 0x0086ad90, %d\n", ret);
		return -1;
	}
	ret = MH_EnableHook(f0086ad90_ref);
	if(ret != MH_OK){
		LOG("Failed enableing hook for 0x0086ad90, %d\n", ret);
		return -1;
	}
	*/

	ret = MH_CreateHook(f00bacd50_ref, (LPVOID)&f00bacd50_patched, (LPVOID *)&f00bacd50_orig);
	if(ret != MH_OK){
		LOG("Failed creating hook for 0x00bacd50, %d\n", ret);
		return -1;
	}
	ret = MH_EnableHook(f00bacd50_ref);
	if(ret != MH_OK){
		LOG("Failed enableing hook for 0x00bacd50, %d\n", ret);
		return -1;
	}

	ret = MH_CreateHook(f00baddd0_ref, (LPVOID)&f00baddd0_patched, (LPVOID *)&f00baddd0_orig);
	if(ret != MH_OK){
		LOG("Failed creating hook for 0x00baddd0, %d\n", ret);
		return -1;
	}
	ret = MH_EnableHook(f00baddd0_ref);
	if(ret != MH_OK){
		LOG("Failed enableing hook for 0x00baddd0, %d\n", ret);
		return -1;
	}

	/*
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
	*/

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
