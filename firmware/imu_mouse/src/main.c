#include <stdio.h>
#include <zephyr/kernel.h>

#include "gpio.h"
#include "imu.h"
#include "usb.h"
#include "log.h"
#include "state.h"

#define SLEEP_TIME_MS 100

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
					im_state_set(IM_APP_STATE_RUNNING); // Before the IMU thread starts, so it can set FAIL afterwards
					if(im_imu_thread_start())
					{
						break;
					}
				}
			}
			im_state_set(IM_APP_STATE_FAIL);
			break;

		case IM_APP_STATE_RUNNING:
			k_sleep(K_MSEC(SLEEP_TIME_MS));
			break;

		case IM_APP_STATE_FAIL:
			return -1;
		}
	}
}