#ifndef __CONFIG_H
#define __CONFIG_H

struct ffb_tweaks{
	bool enabled;
	bool log_effects;
	bool reduce_damper;
	// 0-10000
	int spring_effect_min;
	int spring_effect_max;
};

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

	// steering wheel mode, override values in Physics.cpr
	bool steering_wheel_mode;
	float steering_velocity;
	float steering_max_angle;

	// override analog settings
	bool override_analog_settings;
	float ffb_strength;
	float ffb_vibration;
	float steering_sensitivity;
	float steering_speed_factor;
	float steering_damping;
	float steering_deadzone;
	float clutch_linearity;
	float throttle_linearity;
	float brake_linearity;
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

	float suspension_rate_front;
	float suspension_rate_rear;
};

struct config{
	struct overrides o;
	struct multipliers m;
	struct ffb_tweaks f;
	bool only_modify_player_vehicle;
	bool allow_road_cars_on_dirt;
};

#ifdef __cplusplus
extern "C" {
#endif

extern struct config current_config;
extern pthread_mutex_t current_config_mutex;

void init_config();
void parse_config();

#ifdef __cplusplus
};
#endif

#endif
