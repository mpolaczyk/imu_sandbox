#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/kernel.h>
#include <zephyr/sys/printk.h>

#define SLEEP_TIME_MS 500

static const struct gpio_dt_spec led = GPIO_DT_SPEC_GET(DT_ALIAS(led0), gpios);

int main(void)
{
	int ret;
	bool is_on = false;

	if (!gpio_is_ready_dt(&led)) {
		printk("Error: LED device %s is not ready\n", led.port->name);
		return 0;
	}

	ret = gpio_pin_configure_dt(&led, GPIO_OUTPUT_INACTIVE);
	if (ret < 0) {
		printk("Error: could not configure LED pin (%d)\n", ret);
		return 0;
	}

	printk("imu_sandbox blinky on xiao_ble/nrf52840/sense\n");

	while (1) {
		ret = gpio_pin_set_dt(&led, (int)is_on);
		if (ret < 0) {
			printk("Error: could not set LED pin (%d)\n", ret);
			return 0;
		}

		is_on = !is_on;
		k_msleep(SLEEP_TIME_MS);
	}
}
