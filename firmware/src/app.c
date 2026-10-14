#include <stdio.h>
#include <zephyr/kernel.h>

#include "state.h"
#include "imu.h"
#include "fusion.h"
#include "gpio.h"

#if defined(CONFIG_IM_BLE_DEVICE_MOUSE)
#include "hids_mouse.h"

// Integers on purpose: level 2 (2500) is the default
static const int s_px_per_rad[] = {1500, 2000, 2500, 3000, 3500, 4000};
static uint8_t s_sens_idx = 2;
// Mounting-dependent: yaw (gyro Z) -> X, pitch (gyro Y) -> Y; roll (gyro X) is ignored
#define MOUSE_X_SIGN (-1.0f)
#define MOUSE_Y_SIGN (1.0f) // Cable to back

// Sub-pixel remainders carried between samples
static float s_mouse_rem_x;
static float s_mouse_rem_y;

// A press that confirmed pairing must not produce a click release
static bool s_press_used_for_pairing;
#elif defined(CONFIG_IM_BLE_DEVICE_GATT)
#include "ble_gatt.h"
#endif

static atomic_t s_streaming_active;

void im_app_on_error(void)
{
	im_state_set(IM_APP_STATE_FAIL);
}

void im_app_on_ble_session(bool ready)
{
	const atomic_val_t new_state = ready ? 1 : 0;
	const atomic_val_t old_state = atomic_set(&s_streaming_active, new_state);
	if (old_state == new_state)
	{
		return;
	}
	im_state_set(ready ? IM_APP_STATE_CALIBRATING : IM_APP_STATE_WAITING);
	im_imu_set_streaming(ready);
}

void im_app_on_button(bool pressed)
{
#if defined(CONFIG_IM_BLE_DEVICE_MOUSE)
	if (pressed && im_hids_mouse_pairing_pending())
	{
		s_press_used_for_pairing = true;
		im_hids_mouse_pairing_reply(true);
		return;
	}
	if (!pressed && s_press_used_for_pairing)
	{
		s_press_used_for_pairing = false;
		return;
	}
	if (IS_ENABLED(CONFIG_IM_BUTTON_MODE_SENSITIVITY))
	{
		if (pressed)
		{
			s_sens_idx = (s_sens_idx + 1) % ARRAY_SIZE(s_px_per_rad);
			printk("Sensitivity: %d px/rad\n", s_px_per_rad[s_sens_idx]);
		}
	}
	else if(IS_ENABLED(CONFIG_IM_BUTTON_MODE_CLICK))
	{
		im_hids_mouse_button(pressed);
	}
#elif defined(CONFIG_IM_BLE_DEVICE_GATT)
	ARG_UNUSED(pressed);
#endif
}

void im_app_on_imu_sample(im_vec3 gyro_rate, im_vec3 acceleration, float dt_s)
{
	enum im_app_state state = im_state_get();
	if (state != IM_APP_STATE_CALIBRATING && state != IM_APP_STATE_RUNNING)
	{
		return;
	}

	struct im_fusion_rates rates;
	const bool fusion_valid = im_fusion_update(gyro_rate, acceleration, dt_s, &rates);
	if (!fusion_valid)
	{
		return;
	}
	if (state == IM_APP_STATE_CALIBRATING)
	{
		im_state_set(IM_APP_STATE_RUNNING);
	}

#if defined(CONFIG_IM_BLE_DEVICE_MOUSE)
	const float px_per_rad = (float)s_px_per_rad[s_sens_idx];
	const float fx = s_mouse_rem_x + MOUSE_X_SIGN * rates.yaw * px_per_rad * dt_s;
	const float fy = s_mouse_rem_y + MOUSE_Y_SIGN * rates.pitch * px_per_rad * dt_s;
	const int16_t dx = (int16_t)fx;
	const int16_t dy = (int16_t)fy;
	s_mouse_rem_x = fx - dx;
	s_mouse_rem_y = fy - dy;
	if (dx || dy)
	{
		im_hids_mouse_move(dx, dy);
	}
#elif defined(CONFIG_IM_BLE_DEVICE_GATT)
	im_ble_gatt_push_frame(rates.orientation, rates.gyro_rate);
#endif
}