#include <stdio.h>
#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/sys/util.h>
#include <zephyr/drivers/gpio.h>

#include "gpio.h"

static const struct gpio_dt_spec s_leds[] = 
{
    GPIO_DT_SPEC_GET(DT_ALIAS(led0), gpios),	// IM_GPIO_LED_RED
    GPIO_DT_SPEC_GET(DT_ALIAS(led1), gpios),    // IM_GPIO_LED_GREEN
    GPIO_DT_SPEC_GET(DT_ALIAS(led2), gpios),    // IM_GPIO_LED_BLUE
};

bool im_gpio_init()
{
	for (size_t i = 0; i < ARRAY_SIZE(s_leds); i++)
    {
		if (!gpio_is_ready_dt(&s_leds[i]))
		{
			printk("Error: GPIO is not ready\n");
			return false;
		}
		if (gpio_pin_configure_dt(&s_leds[i], GPIO_OUTPUT_ACTIVE)) 
		{
			printk("Error: GPIO configuration failed for led\n");
			return false;
		}
	}
	printk("GPIO Ok\n");
	return true;
}

bool im_gpio_led_toggle(enum im_gpio_led index)
{
	if (gpio_pin_toggle_dt(&s_leds[(int)index]) < 0) 
	{
		printk("Error: GPIO Unable to toggle led {%d}\n", (int)index);
		return false;
	}
	return true;
}

bool im_gpio_led_set(enum im_gpio_led index, int value)
{
	if (gpio_pin_set_dt(&s_leds[(int)index], value) < 0)
	{
		printk("Error: GPIO Unable to set led {%d} to value {%d}\n", (int)index, value);
		return false;
	}
	return true;
}

bool im_gpio_led_set_all(int value_red, int value_green, int value_blue)
{
	bool result = false;
	result |= im_gpio_led_set(IM_GPIO_LED_RED, value_red);
	result |= im_gpio_led_set(IM_GPIO_LED_GREEN, value_green);
	result |= im_gpio_led_set(IM_GPIO_LED_BLUE, value_blue);
	return result;
}