#include <stdio.h>
#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/uart.h>
#include <zephyr/logging/log_ctrl.h>
#include <zephyr/drivers/sensor.h>
#include <zephyr/sys/util.h>
#include <zephyr/drivers/gpio.h>

#include "gpio.h"
#include "imu.h"
#include "usb.h"

#define SLEEP_TIME_MS 100

enum im_app_state
{
	IM_APP_STATE_INIT_1,
	IM_APP_STATE_INIT_2,
	IM_APP_STATE_RUNNING,
	IM_APP_STATE_FAIL
};

int main(void)
{
	enum im_app_state state = IM_APP_STATE_INIT_1;
	while (1) 
	{
		switch (state)
		{
		case IM_APP_STATE_INIT_1:
			state = im_gpio_init() ? IM_APP_STATE_INIT_2 : IM_APP_STATE_FAIL;
			break;

		case IM_APP_STATE_INIT_2:
			im_gpio_led_set_all(0, 1, 0);
			if(im_usb_init() && im_imu_init(true, true))
			{
				state = IM_APP_STATE_RUNNING;
				im_gpio_led_set_all(0, 0, 1);
			}
			else
			{
				state = IM_APP_STATE_FAIL;
			}
			break;

		case IM_APP_STATE_RUNNING:
			while(log_process()) {}
			if(!im_gpio_led_toggle(IM_GPIO_LED_BLUE) || !im_imu_collect(true))
			{
				state = IM_APP_STATE_FAIL;
				break;
			}
			k_sleep(K_MSEC(SLEEP_TIME_MS));
			break;

		case IM_APP_STATE_FAIL:
			while(log_process()) {}
			im_gpio_led_set_all(1, 0, 0);
			return -1;
		}
	}
}