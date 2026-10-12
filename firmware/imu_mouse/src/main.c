#include <stdio.h>
#include <zephyr/kernel.h>

#include "gpio.h"
#include "imu.h"
#include "usb.h"
#include "log.h"
#include "state.h"
#include "threads.h"
#include "hids_mouse.h"
#include "app.h"
#include "ble_gatt.h"

int main(void)
{
	while (1) 
	{
		switch (im_state_get())
		{
		case IM_APP_STATE_INIT_1:
			if (im_gpio_init())
			{
				im_gpio_thread_start();
				im_state_set(IM_APP_STATE_INIT_2);
				break;
			}
			im_state_set(IM_APP_STATE_FAIL);
			break;

		case IM_APP_STATE_INIT_2:
			if(im_usb_init())
			{
				im_log_thread_start();
				if(im_imu_init())
				{
					im_imu_set_sample_cb(im_app_on_sample);
					// Change the state before the IMU thread starts, so it can set FAIL afterwards
					im_state_set(IM_APP_STATE_INIT_3);
					if(im_imu_thread_start())
					{
						break;
					}
				}
			}
			im_state_set(IM_APP_STATE_FAIL);
			break;

		case IM_APP_STATE_INIT_3:
#if defined(CONFIG_IM_BLE_DEVICE_MOUSE)
			if (im_hids_mouse_init() && im_hids_mouse_start())
#elif defined(CONFIG_IM_BLE_DEVICE_GATT)
			if (im_ble_gatt_init() && im_ble_gatt_start())
#endif
			{
				if(im_gpio_button_init(im_app_on_button))
				{
					im_state_set(IM_APP_STATE_CALIBRATING);
					break;
				}
			}
			im_state_set(IM_APP_STATE_FAIL);
			break;

		case IM_APP_STATE_CALIBRATING:
		case IM_APP_STATE_RUNNING:
			k_sleep(K_MSEC(IM_MAIN_THREAD_PERIOD_MS));
			break;

		case IM_APP_STATE_FAIL:
			return -1;
		}
	}
}

// Bluetooth issues in Chrome
// chrome://flags/
// Use the new permissions backend for Web Bluetooth: Enabled -> Relaunch
// chrome://settings/content/bluetoothDevices -> Sites can ask...
// https://googlechrome.github.io/samples/web-bluetooth/device-info.html?allDevices=true
// To check if Chrome detects devices