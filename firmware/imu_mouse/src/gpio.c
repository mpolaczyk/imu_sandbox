#include <stdio.h>
#include <zephyr/kernel.h>
#include <zephyr/drivers/gpio.h>

#include "gpio.h"
#include "state.h"

#define GPIO_THREAD_STACK_SIZE 512
#define GPIO_THREAD_PRIORITY K_LOWEST_APPLICATION_THREAD_PRIO
#define GPIO_THREAD_PERIOD_MS 100

// Internal linkage

static K_THREAD_STACK_DEFINE(s_gpio_thread_stack, GPIO_THREAD_STACK_SIZE);
static struct k_thread s_gpio_thread_data;

static void s_gpio_thread_fn(void *p1, void *p2, void *p3)
{
	ARG_UNUSED(p1);
	ARG_UNUSED(p2);
	ARG_UNUSED(p3);
	static enum im_app_state previous_state = IM_APP_STATE_INIT_1;
	while (1)
	{
		enum im_app_state state = im_state_get();
		switch (state)
		{
		case IM_APP_STATE_INIT_2:
			if(previous_state == IM_APP_STATE_INIT_1)
			{
				im_gpio_led_set_all(0, 1, 0);
			}
			break;

		case IM_APP_STATE_RUNNING:
			if(previous_state == IM_APP_STATE_INIT_2)
			{
				im_gpio_led_set_all(0, 0, 1);
			}
			if (!im_gpio_led_toggle(IM_GPIO_LED_BLUE))
			{
				im_state_set(IM_APP_STATE_FAIL);
			}
			break;

		case IM_APP_STATE_FAIL:
			im_gpio_led_set_all(1, 0, 0);
			break;

		default:
			break;
		}
		previous_state = state;
		k_msleep(GPIO_THREAD_PERIOD_MS);
	}
}

static const struct gpio_dt_spec s_leds[] = 
{
    GPIO_DT_SPEC_GET(DT_ALIAS(led0), gpios),	// IM_GPIO_LED_RED
    GPIO_DT_SPEC_GET(DT_ALIAS(led1), gpios),    // IM_GPIO_LED_GREEN
    GPIO_DT_SPEC_GET(DT_ALIAS(led2), gpios),    // IM_GPIO_LED_BLUE
};

// External linkage

void im_gpio_thread_start()
{
	k_tid_t tid = k_thread_create(&s_gpio_thread_data, s_gpio_thread_stack,
				      K_THREAD_STACK_SIZEOF(s_gpio_thread_stack),
				      s_gpio_thread_fn, NULL, NULL, NULL,
				      GPIO_THREAD_PRIORITY, 0, K_NO_WAIT);
	k_thread_name_set(tid, "gpio");
}

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