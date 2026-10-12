#include <stdio.h>
#include <zephyr/kernel.h>

#include "state.h"
#include "imu.h"
#include "fusion.h"
#include "gpio.h"

#if defined(CONFIG_IM_BLE_DEVICE_MOUSE)
#include "hids_mouse.h"

// Integers on purpose: sensitivity is changed from a non-FPU thread
static const int s_px_per_rad[] = {1500, 2000, 2500, 3000, 3500, 4000};
static volatile uint8_t s_sens_idx = 2;
// Mounting-dependent: yaw (gyro Z) -> X, pitch (gyro Y) -> Y; roll (gyro X) is ignored
#define MOUSE_X_SIGN (-1.0f)
#define MOUSE_Y_SIGN (-1.0f)

// Sub-pixel remainders carried between samples
static float s_mouse_rem_x;
static float s_mouse_rem_y;
#else
#include "imu_ble.h"
#endif

void im_app_on_button1(void)
{
#if defined(CONFIG_IM_BLE_DEVICE_MOUSE)
	if (im_hids_mouse_pairing_pending())
	{
		im_hids_mouse_pairing_reply(true);
		return;
	}
	s_sens_idx = (s_sens_idx + 1) % ARRAY_SIZE(s_px_per_rad);
	printk("Sensitivity: %d px/rad\n", s_px_per_rad[s_sens_idx]);
#endif
}

void im_app_on_sample(float gx, float gy, float gz, float ax, float ay, float az)
{
	enum im_app_state state = im_state_get();
	if (state != IM_APP_STATE_CALIBRATING && state != IM_APP_STATE_RUNNING)
	{
		return;
	}

	struct im_fusion_rates rates;
	const bool fusion_valid = im_fusion_update(gx, gy, gz, ax, ay, az, &rates);
	if (!fusion_valid)
	{
		return;
	}
	if (state == IM_APP_STATE_CALIBRATING)
	{
		im_state_set(IM_APP_STATE_RUNNING);
	}

#if defined(CONFIG_IM_BLE_DEVICE_IMU)
	struct imu_pose pose = {0};
	pose.qw = rates.qw;
	pose.qx = rates.qx;
	pose.qy = rates.qy;
	pose.qz = rates.qz;
	pose.gx_rad_s = rates.gx;
	pose.gy_rad_s = rates.gy;
	pose.gz_rad_s = rates.gz;
	imu_ble_push_pose(&pose);
#else
	const float px_per_rad_sample = (float)s_px_per_rad[s_sens_idx] / CONFIG_IM_IMU_SAMPLE_RATE_HZ;
	const float fx = s_mouse_rem_x + MOUSE_X_SIGN * rates.yaw * px_per_rad_sample;
	const float fy = s_mouse_rem_y + MOUSE_Y_SIGN * rates.pitch * px_per_rad_sample;
	const int16_t dx = (int16_t)fx;
	const int16_t dy = (int16_t)fy;
	s_mouse_rem_x = fx - dx;
	s_mouse_rem_y = fy - dy;
	if (dx || dy)
	{
		im_hids_mouse_move(dx, dy);
	}
#endif
}