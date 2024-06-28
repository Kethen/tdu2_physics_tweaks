## TDU2 Physics Tweak

Tune and apply global multipliers to some aspects of tdu2 handling

### Installation

Place `dinput8.dll`, `MinHook.x86.dll`, `tdu2_physics_tweaks_config.json` and `tdu2_physics_tweaks_i686.asi` next to TestDrive2.exe

### Usage

Edit `tdu2_physics_tweaks_config.json` to adjust some aspects of tdu2 handling to your liking. The following example adjusts vanilla cars to feel somewhere between tdu2 and tdu1 hc.

```
{
	"only_modify_player_vehicle":true,
	"overrides":{
		"d1":"changes gravity constant, only effective per game boot",
		"gravity":-9.81,

		"d2":"overrides extra gravity values in Physics.cpr, applies run time. set to 0 to remove extra downforce when airborne",
		"min_extra_gravity":0.0,
		"max_extra_gravity":0.0,
		"extra_gravity_accel_duration":0.1,
		"extra_gravity_accel_delay":0.1
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
		"grip_rear":0.7
	}
}
```

### Trouble shooting

Check `tdu2_physics_tweaks_log.txt` if the game refuses to start/self closes

### Credits

- TDU2VPE https://turboduck.net/forums/topic/33748-tdu2vpe-release/
	- I would not know where to start at all without TDU2VPE
- tdudec https://aluigi.altervista.org/papers.htm
	- For Physics.cpr encryption and decryption

### External projects used

- MinHook https://github.com/TsudaKageyu/minhook
- json https://github.com/nlohmann/json
- Ultimate-ASI-Loader https://github.com/ThirteenAG/Ultimate-ASI-Loader
