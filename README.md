## TDU2 Physics Tweak

Nudge tdu2 overall car handling to your own liking :D

### Installation

Place `dinput8.dll`, `MinHook.x86.dll`, `tdu2_physics_tweaks_config.json` and `tdu2_physics_tweaks_i686.asi` next to TestDrive2.exe.

### Usage

After installation, edit `tdu2_physics_tweaks_config.json` to nudge the game closer to your taste.

Some values can be adjusted while the game is running. Check the in-json string comments for when values are applied.

The following example adjusts vanilla cars to feel somewhere between tdu2 and tdu1 hc. Time trials and license tests will become relatively more challenging.

```
{
	"only_modify_player_vehicle":true,
	"allow_road_cars_on_dirt": true,
	"overrides":{
		"d1":"changes gravity constant, applies once on game start",
		"gravity":-9.81,

		"d2":"overrides extra gravity values in Physics.cpr, applies run time. set to 0 to remove extra downforce when airborne",
		"min_extra_gravity":0.0,
		"max_extra_gravity":0.0,
		"extra_gravity_accel_duration":0.1,
		"extra_gravity_accel_delay":0.1,

		"d3":"beta, disabling abs and tcs, weird to do in tdu2, let me know how each item feels",
		"abs_off":true,
		"tcs_off":true,
		"hand_brake_abs_off":true,

		"d4":"override angular damping values in Physics.cpr, higher values means more resistance to rotations (both turning and rolling), applies on car spawn/change",
		"override_angular_damping":true,
		"new_angular_damping":0.0,

		"d5":"steering wheel mode like in tdu1, values overrides only applies when enabled, applies on car spawn/change",
		"steering_wheel_mode":false,
		"steering_velocity":900.0,
		"steering_max_angle":40.0,

		"d6":"analog settings (controller configuration), applies runtime",
		"override_analog_settings":false,
		"ffb_strength":1.0,
		"ffb_vibration":0.5,
		"steering_sensitivity":0.25,
		"steering_speed_factor":0.0,
		"steering_damping":0.0,
		"steering_deadzone":0.0,
		"clutch_linearity":0.5,
		"throttle_linearity":0.5,
		"brake_linearity":0.5
	},
	"multipliers":{
		"d1":"multiply some values available in tdu2vpe during load, applies on car spawn/change",
		"suspension_length_front":2.4,
		"suspension_length_rear":2.4,
		"dampers_front":0.7,
		"dampers_rear":0.7,
		"ride_height_front":1.2,
		"ride_height_rear":1.2,
		"anti_roll_bar_front":0.0,
		"anti_roll_bar_rear":0.0,
		"anti_roll_bar_damping_front":0.5,
		"anti_roll_bar_damping_rear":0.5,
		"lift_drag_ratio":1.0,
		"down_force_velocity":1.0,
		"down_force_front":1.0,
		"down_force_rear":1.0,
		"lateral_grip_front":0.6,
		"lateral_grip_rear":0.6,
		"grip_front":0.7,
		"grip_rear":0.7,
		"brake_power":1.0
	}
}
```

Changed values are logged in `tdu2_physics_tweaks_log.txt` for db tuning reference.

### Trouble shooting

Make sure your edited config is a valid json file and contains all the keys this tool needs.

Check `tdu2_physics_tweaks_log.txt` if the game refuses to start/self closes.

### Building

On windows, install cygwin, along with mingw64-i686-gcc-g++ tool chain, then run `build.sh` at the project root in cygwin shell.

On linux, install podman from your package manager then run `build_podman.sh`.

### Credits

- TDU2VPE https://turboduck.net/forums/topic/33748-tdu2vpe-release/
	- I would not know where to start at all without TDU2VPE
- tdudec https://aluigi.altervista.org/papers.htm
	- For Physics.cpr encryption and decryption

### External projects used

- MinHook https://github.com/TsudaKageyu/minhook
- json https://github.com/nlohmann/json
- Ultimate-ASI-Loader https://github.com/ThirteenAG/Ultimate-ASI-Loader
