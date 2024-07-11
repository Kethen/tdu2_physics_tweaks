#include "common.h"

struct global_store global = {0};

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
