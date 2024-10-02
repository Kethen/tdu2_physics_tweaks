#include <fcntl.h>

#include <json.hpp>

#include <pthread.h>

#include "logging.h"

#include "config.h"

#define STR(s) #s

using json = nlohmann::json;

extern "C" {

struct config current_config = {0};
pthread_mutex_t current_config_mutex;

void init_config(){
	pthread_mutex_init(&current_config_mutex, NULL);
}

static void log_config(struct config *c){
	#define PRINT_SETTING_BOOL(key) {\
		LOG("setting " STR(key) ": %s\n", c->key? "true" : "false");\
	}
	PRINT_SETTING_BOOL(only_modify_player_vehicle);
	PRINT_SETTING_BOOL(allow_road_cars_on_dirt);
	#undef PRINT_SETTING_BOOL

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
	PRINT_OVERRIDE_BOOL(steering_wheel_mode);
	PRINT_OVERRIDE_FLOAT(steering_velocity);
	PRINT_OVERRIDE_FLOAT(steering_max_angle);
	PRINT_OVERRIDE_BOOL(override_analog_settings);
	PRINT_OVERRIDE_FLOAT(ffb_strength);
	PRINT_OVERRIDE_FLOAT(ffb_vibration);
	PRINT_OVERRIDE_FLOAT(steering_sensitivity);
	PRINT_OVERRIDE_FLOAT(steering_speed_factor);
	PRINT_OVERRIDE_FLOAT(steering_damping);
	PRINT_OVERRIDE_FLOAT(steering_deadzone);
	PRINT_OVERRIDE_FLOAT(clutch_linearity);
	PRINT_OVERRIDE_FLOAT(throttle_linearity);
	PRINT_OVERRIDE_FLOAT(brake_linearity);
	#undef PRINT_OVERRIDE_FLOAT
	#undef PRINT_OVERRIDE_BOOL

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
	PRINT_MULTIPLIER_FLOAT(spring_front);
	PRINT_MULTIPLIER_FLOAT(spring_rear);
	#undef PRINT_MULTIPLIER_FLOAT

	#define PRINT_FFB_TWEAK_BOOL(name){ \
		LOG("ffb tweak %s: %s\n", STR(name), current_config.f.name? "true": "false"); \
	}
	#define PRINT_FFB_TWEAK_INT(name){ \
		LOG("ffb tweak %s: %d\n", STR(name), current_config.f.name); \
	}
	PRINT_FFB_TWEAK_BOOL(enabled);
	PRINT_FFB_TWEAK_BOOL(reduce_damper);
	PRINT_FFB_TWEAK_BOOL(log_effects);
	PRINT_FFB_TWEAK_INT(spring_effect_min);
	PRINT_FFB_TWEAK_INT(spring_effect_max);
	#undef PRINT_FFB_TWEAK_BOOL
	#undef PRINT_FFB_TWEAK_INT

	#define PRINT_FFB_TWEAK_

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

		/*
		char *buffer = (char *)malloc(file_size);
		if(buffer == NULL){
			LOG("failed allocating buffer for reading the config file, using defaults\n");
			break;
		}
		*/
		char buffer[8192];
		if(file_size > sizeof(buffer)){
			LOG("config file is too big, using defaults\n");
			break;
		}

		int bytes_read = read_data_from_fd(config_fd, buffer, file_size);
		if(bytes_read < 0){
			LOG("failed reading tdu2_physics_tweaks_config.json, using defaults\n");
			//free(buffer);
			break;
		}

		try{
			parsed_config = json::parse(std::string(buffer, bytes_read));
		}catch(...){
			LOG("failed parsing tdu2_physics_tweaks_config.json, using defaults\n");
			parsed_config = {};
			//free(buffer);
			break;
		}
		//free(buffer);
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
			LOG("warning: failed fetching config " STR(key) " from json, adding default\n") \
			updated = true; \
			incoming_config.key = d; \
			parsed_config[STR(key)] = d; \
		} \
	}
	FETCH_SETTING(only_modify_player_vehicle, true);
	FETCH_SETTING(allow_road_cars_on_dirt, false);
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
	FETCH_OVERRIDE(steering_wheel_mode, false);
	FETCH_OVERRIDE(steering_velocity, 900.0);
	FETCH_OVERRIDE(steering_max_angle, 40.0);
	FETCH_OVERRIDE(override_analog_settings, false);
	FETCH_OVERRIDE(ffb_strength, 1.0);
	FETCH_OVERRIDE(ffb_vibration, 0.5);
	FETCH_OVERRIDE(steering_sensitivity, 0.25);
	FETCH_OVERRIDE(steering_speed_factor, 0.0);
	FETCH_OVERRIDE(steering_damping, 0.0);
	FETCH_OVERRIDE(steering_deadzone, 0.0);
	FETCH_OVERRIDE(clutch_linearity, 0.5);
	FETCH_OVERRIDE(throttle_linearity, 0.5);
	FETCH_OVERRIDE(brake_linearity, 0.5);
	#undef FETCH_OVERRIDE

	#define FETCH_MULTIPLIER(key, d) { \
		try{ \
			incoming_config.m.key = parsed_config.at("multipliers").at(STR(key)); \
		}catch(...){ \
			LOG("warning: failed fetching multiplier " STR(key) " from json, adding default\n"); \
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
	FETCH_MULTIPLIER(spring_front, 1.0);
	FETCH_MULTIPLIER(spring_rear, 1.0);
	#undef FETCH_MULTIPLIER

	#define FETCH_FFB_TWEAK(key, d) { \
		try{ \
			incoming_config.f.key = parsed_config.at("ffb_tweaks").at(STR(key)); \
		}catch(...){ \
			LOG("warning: failed fetching ffb tweak " STR(key) " from json, adding default\n"); \
			updated = true; \
			incoming_config.f.key = d; \
			parsed_config["ffb_tweaks"][STR(key)] = d; \
		} \
	}
	FETCH_FFB_TWEAK(enabled, false);
	FETCH_FFB_TWEAK(reduce_damper, false);
	FETCH_FFB_TWEAK(log_effects, false);
	FETCH_FFB_TWEAK(spring_effect_min, 6500);
	FETCH_FFB_TWEAK(spring_effect_max, 10000);
	#undef FETCH_FFB_TWEAK

	if(memcmp(&current_config, &incoming_config, sizeof(struct config)) != 0){
		pthread_mutex_lock(&current_config_mutex);
		memcpy(&current_config, &incoming_config, sizeof(struct config));
		pthread_mutex_unlock(&current_config_mutex);
		log_config(&current_config);
	}

	if(!updated){
		return;
	}
	config_fd = open(config_path, O_BINARY | O_WRONLY | O_TRUNC | O_CREAT, 00644);

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

}
