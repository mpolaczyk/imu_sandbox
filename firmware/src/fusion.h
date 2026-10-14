#ifndef IM_FUSION_H
#define IM_FUSION_H

#include <stdbool.h>
#include "math3d.h"

struct im_fusion_rates
{
	float yaw;   // rad/s about gravity
	float pitch; // rad/s about the horizontal part of body Y
	im_quat orientation;
	im_vec3 gyro_rate; // bias-corrected body rate, rad/s
};

void im_fusion_reset(void);

// Feeds one sample (gyro rad/s, accel m/s^2, elapsed time in seconds).
// Returns false while the startup bias calibration runs.
bool im_fusion_update(im_vec3 gyro_rate, im_vec3 acceleration, float dt_s, struct im_fusion_rates *out);

#endif
