#include <stdio.h>
#include <zephyr/kernel.h>

#include "state.h"
#include "imu.h"
#include "fusion.h"
#include "hids_mouse.h"
#include "imu_ble.h"
#include "gpio.h"

// Integers on purpose: sensitivity is changed from a non-FPU thread
static const int s_px_per_rad[] = {1500, 2000, 2500, 3000, 3500, 4000};
static volatile uint8_t s_sens_idx = 2;
// Mounting-dependent: yaw (gyro Z) -> X, pitch (gyro Y) -> Y; roll (gyro X) is ignored
#define MOUSE_X_SIGN (-1.0f)
#define MOUSE_Y_SIGN (-1.0f)

// Sub-pixel remainders carried between samples
static float s_mouse_rem_x;
static float s_mouse_rem_y;

void im_app_on_button1(void)
{
	if (im_hids_mouse_pairing_pending())
	{
		im_hids_mouse_pairing_reply(true);
		return;
	}
	s_sens_idx = (s_sens_idx + 1) % ARRAY_SIZE(s_px_per_rad);
	printk("Sensitivity: %d px/rad\n", s_px_per_rad[s_sens_idx]);
}

void im_app_on_sample(float gx, float gy, float gz, float ax, float ay, float az)
{
	struct im_fusion_rates rates;
	const bool fusion_valid = im_fusion_update(gx, gy, gz, ax, ay, az, &rates);
	struct imu_pose pose = {0};
#if CONFIG_IMU_BLE_SYNTHETIC_DATA
	const float phase = (float)k_uptime_get() * 0.0005f;
	pose.orientation_valid = true;
	pose.qw = cosf(phase * 0.5f);
	pose.qy = sinf(phase * 0.5f);
	pose.gz_rad_s = 0.5f;
#else
	if (fusion_valid)
	{
		pose.orientation_valid = true;
		pose.qw = rates.qw;
		pose.qx = rates.qx;
		pose.qy = rates.qy;
		pose.qz = rates.qz;
		pose.gx_rad_s = rates.gx;
		pose.gy_rad_s = rates.gy;
		pose.gz_rad_s = rates.gz;
	}
#endif
	pose.button_pressed = im_gpio_button_is_pressed();
	imu_ble_send_pose(&pose);

	if (!fusion_valid || im_state_get() != IM_APP_STATE_RUNNING)
	{
		return;
	}

	const float px_per_rad_sample = (float)s_px_per_rad[s_sens_idx] / IM_IMU_SAMPLE_RATE_HZ;
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
}