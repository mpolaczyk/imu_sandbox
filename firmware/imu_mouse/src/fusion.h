#ifndef IM_FUSION_H
#define IM_FUSION_H

#include <stdbool.h>

struct im_fusion_rates
{
	float yaw;   // rad/s about gravity
	float pitch; // rad/s about the horizontal part of body Y
};

/* Feeds one sample (gyro rad/s, accel m/s^2). Returns false while the startup bias calibration runs. */
bool im_fusion_update(float gx, float gy, float gz, float ax, float ay, float az, struct im_fusion_rates *out);

#endif
