#ifndef __COMMON_H
#define __COMMON_H

struct global_store{
	float brake_pedal_level;
	float current_slip_ratio;
};

#ifdef __cplusplus
extern "C" {
#endif

extern struct global_store global;
float calculate_new_slip_ratio(float brake_pedal_level);

#ifdef __cplusplus
};
#endif

#endif
