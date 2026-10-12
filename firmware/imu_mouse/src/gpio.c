#include <stdio.h>
#include <zephyr/kernel.h>
#include <zephyr/drivers/gpio.h>

#include "gpio.h"
#include "state.h"
#include "threads.h"

// Internal linkage

static K_THREAD_STACK_DEFINE(s_gpio_thread_stack, IM_GPIO_THREAD_STACK_SIZE);
static struct k_thread s_gpio_thread_data;

static void s_gpio_thread_fn(void *p1, void *p2, void *p3)
{
	ARG_UNUSED(p1);
	ARG_UNUSED(p2);
	ARG_UNUSED(p3);
	while (1)
	{
		enum im_app_state state = im_state_get();
		switch (state)
		{
		case IM_APP_STATE_INIT_1:
		case IM_APP_STATE_INIT_2:
		case IM_APP_STATE_INIT_3:
			if (!im_gpio_led_toggle_only(IM_GPIO_LED_GREEN))
			{
				im_state_set(IM_APP_STATE_FAIL);
			}
			break;

		case IM_APP_STATE_CALIBRATING:
			if (!im_gpio_led_toggle_only(IM_GPIO_LED_BLUE))
			{
				im_state_set(IM_APP_STATE_FAIL);
			}
			break;

		case IM_APP_STATE_RUNNING:
			if (!im_gpio_led_set_only(IM_GPIO_LED_BLUE))
			{
				im_state_set(IM_APP_STATE_FAIL);
			}
			break;

		case IM_APP_STATE_FAIL:
			im_gpio_led_set_only(IM_GPIO_LED_RED);
			break;

		default:
			break;
		}
		k_msleep(IM_GPIO_THREAD_PERIOD_MS);
	}
}

static const struct gpio_dt_spec s_leds[] = 
{
    GPIO_DT_SPEC_GET(DT_ALIAS(led0), gpios),	// IM_GPIO_LED_RED
    GPIO_DT_SPEC_GET(DT_ALIAS(led1), gpios),    // IM_GPIO_LED_GREEN
    GPIO_DT_SPEC_GET(DT_ALIAS(led2), gpios),    // IM_GPIO_LED_BLUE
};

#define BUTTON_DEBOUNCE_MS 30

static const struct gpio_dt_spec s_button = GPIO_DT_SPEC_GET(DT_NODELABEL(button0), gpios);
static struct gpio_callback s_button_gpio_cb;
static struct k_work_delayable s_button_work;
static im_gpio_button_cb_t s_button_cb;
static atomic_t s_button_down;

static void s_button_isr(const struct device *dev, struct gpio_callback *cb, uint32_t pins)
{
	ARG_UNUSED(dev);
	ARG_UNUSED(cb);
	ARG_UNUSED(pins);
	k_work_reschedule(&s_button_work, K_MSEC(BUTTON_DEBOUNCE_MS));
}

static void s_button_work_fn(struct k_work *work)
{
	ARG_UNUSED(work);
	const bool down = gpio_pin_get_dt(&s_button) > 0;
	const bool was_down = atomic_get(&s_button_down) != 0;
	if (down && !was_down && s_button_cb)
	{
		s_button_cb();
	}
	atomic_set(&s_button_down, down);
}

// External linkage

void im_gpio_thread_start()
{
	k_tid_t tid = k_thread_create(&s_gpio_thread_data, s_gpio_thread_stack,
				      K_THREAD_STACK_SIZEOF(s_gpio_thread_stack),
				      s_gpio_thread_fn, NULL, NULL, NULL,
				      IM_GPIO_THREAD_PRIORITY, 0, K_NO_WAIT);
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

bool im_gpio_button_is_pressed(void)
{
	return atomic_get(&s_button_down) != 0;
}

bool im_gpio_button_init(im_gpio_button_cb_t cb)
{
	if (!gpio_is_ready_dt(&s_button))
	{
		printk("Error: Button GPIO is not ready\n");
		return false;
	}

	if (gpio_pin_configure_dt(&s_button, GPIO_INPUT) < 0)
	{
		printk("Error: Button GPIO configuration failed\n");
		return false;
	}
	s_button_cb = cb;
	k_work_init_delayable(&s_button_work, s_button_work_fn);
	gpio_init_callback(&s_button_gpio_cb, s_button_isr, BIT(s_button.pin));
	if (gpio_add_callback(s_button.port, &s_button_gpio_cb) < 0 ||
	    gpio_pin_interrupt_configure_dt(&s_button, GPIO_INT_EDGE_BOTH) < 0)
	{
		printk("Error: Button GPIO interrupt setup failed\n");
		return false;
	}
	return true;
}

bool im_gpio_led_toggle(unsigned int leds)
{
	bool result = true;
	for (size_t i = 0; i < ARRAY_SIZE(s_leds); i++)
	{
		if ((leds & BIT(i)) && gpio_pin_toggle_dt(&s_leds[i]) < 0)
		{
			printk("Error: GPIO Unable to toggle led {%d}\n", (int)i);
			result = false;
		}
	}
	return result;
}

bool im_gpio_led_set(unsigned int leds, int value)
{
	bool result = true;
	for (size_t i = 0; i < ARRAY_SIZE(s_leds); i++)
	{
		if ((leds & BIT(i)) && gpio_pin_set_dt(&s_leds[i], value) < 0)
		{
			printk("Error: GPIO Unable to set led {%d} to value {%d}\n", (int)i, value);
			result = false;
		}
	}
	return result;
}

bool im_gpio_led_set_only(unsigned int leds)
{
	const unsigned int all = 0xffffu;
	const bool result_on = im_gpio_led_set(leds & all, 1);
	const bool result_off = im_gpio_led_set(~leds & all, 0);
	return result_on && result_off;
}

bool im_gpio_led_toggle_only(unsigned int leds)
{
	const unsigned int all = 0xffffu;
	const bool result_off = im_gpio_led_set(~leds & all, 0);
	const bool result_toggle = im_gpio_led_toggle(leds & all);
	return result_off && result_toggle;
}