#include <stdio.h>
#include <zephyr/kernel.h>

#include "state.h"
#include "imu.h"
#include "hids_mouse.h"

#define MOUSE_PX_PER_RAD 1000.0f
#define MOUSE_PX_PER_RAD_SAMPLE (MOUSE_PX_PER_RAD / IM_IMU_SAMPLE_RATE_HZ)
// Mounting-dependent: yaw (gyro Z) -> X, pitch (gyro Y) -> Y; roll (gyro X) is ignored
#define MOUSE_X_SIGN (-1.0f)
#define MOUSE_Y_SIGN (-1.0f)

// Sub-pixel remainders carried between samples
static float s_mouse_rem_x;
static float s_mouse_rem_y;

void im_app_on_gyro(float gx, float gy, float gz)
{
	ARG_UNUSED(gx);
	if (im_state_get() != IM_APP_STATE_RUNNING)
	{
		return;
	}
	const float fx = s_mouse_rem_x + MOUSE_X_SIGN * gz * MOUSE_PX_PER_RAD_SAMPLE;
	const float fy = s_mouse_rem_y + MOUSE_Y_SIGN * gy * MOUSE_PX_PER_RAD_SAMPLE;
	const int16_t dx = (int16_t)fx;
	const int16_t dy = (int16_t)fy;
	s_mouse_rem_x = fx - dx;
	s_mouse_rem_y = fy - dy;
	if (dx || dy)
	{
		im_hids_mouse_move(dx, dy);
	}
}