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

int main(void)
{
	while (1) 
	{
		switch (im_state_get())
		{
		case IM_APP_STATE_INIT_1:
			if(im_gpio_init())
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
				if(im_imu_init(true, true))
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
			if(im_hids_mouse_init() && im_hids_mouse_start() && im_gpio_button_init(im_app_on_button))
			{
				im_state_set(IM_APP_STATE_RUNNING);
				break;
			}
			im_state_set(IM_APP_STATE_FAIL);
			break;

		case IM_APP_STATE_RUNNING:
			k_sleep(K_MSEC(IM_MAIN_THREAD_PERIOD_MS));
			break;

		case IM_APP_STATE_FAIL:
			return -1;
		}
	}
}