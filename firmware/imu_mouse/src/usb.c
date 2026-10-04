#include <stdio.h>
#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/uart.h>
#include <zephyr/logging/log_ctrl.h>
#include <zephyr/sys/util.h>
#include <zephyr/usb/usb_device.h>

#include "usb.h"

#define USB_WAIT_FOR_DTR 0 // 0: do not block until a USB terminal connects

bool im_usb_init()
{
    const struct device *const usb_uart = DEVICE_DT_GET_ONE(zephyr_cdc_acm_uart);
    const int err = usb_enable(NULL); // Initialize and enable the old USB stack
    if (err != 0) 
	{
        return false;
    }
	int count = 0;
#if USB_WAIT_FOR_DTR
	uint32_t dtr = 0;
    while (!dtr) // Wait until terminal connects USB DTR is received 
	{
        uart_line_ctrl_get(usb_uart, UART_LINE_CTRL_DTR, &dtr);
        if (!dtr) 
		{
            k_msleep(100); // USB threads can work
			count++;
        }
    }
#endif
	printk("USB Ok, attemps: %d\n", count);
	return true;
}