#include <stdio.h>
#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/sensor.h>

#include "imu.h"

static struct sensor_value accel[3];
static struct sensor_value gyro[3];
static const struct device* const imu = DEVICE_DT_GET_ONE(st_lsm6dsl);

bool im_imu_init(const bool delay_init, const bool configure_sampling)
{
	struct sensor_value odr_attr;
	if (!imu) 
	{
        printk("Error: IMU device pointer not found\n");
        return false;
    }
	if(delay_init)
	{
		// Delayed init is implemented because normal init does not work (timing)
		printk("IMU Manual delayed initialization\n");
    	const int err = device_init(imu);
    	if (err)
		{
        	printk("Error: IMU initialization failed, error code: %d\n", err);
       		return false;
    	}
	}
	if(!device_is_ready(imu)) 
	{
		const uint8_t state = imu->state->init_res;
		const bool initialized = imu->state->initialized;
		printk("Error: IMU not ready, init_res=%d init=%d\n", state, initialized);
		return false;
	}
	if(configure_sampling)
	{
		printk("IMU configuring sampling\n");
		odr_attr.val1 = 104; // 104Hz
		odr_attr.val2 = 0;
		if (sensor_attr_set(imu, SENSOR_CHAN_ACCEL_XYZ, SENSOR_ATTR_SAMPLING_FREQUENCY, &odr_attr) < 0) 
		{
			printk("Error: IMU Cannot set sampling frequency for accelerometer.\n");
			return false;
		}
		if (sensor_attr_set(imu, SENSOR_CHAN_GYRO_XYZ, SENSOR_ATTR_SAMPLING_FREQUENCY, &odr_attr) < 0) 
		{
			printk("Error: IMU Cannot set sampling frequency for gyro.\n");
			return false;
		}
	}
	printk("IMU Ok\n");
	return true;
}

bool im_imu_collect(const bool do_print)
{
	if (sensor_sample_fetch(imu) < 0) 
	{
		printk("Error: IMU Sensor sample update error\n");
		return false;
	}

	sensor_channel_get(imu, SENSOR_CHAN_ACCEL_XYZ, accel);
	sensor_channel_get(imu, SENSOR_CHAN_GYRO_XYZ, gyro);

	if(do_print)
	{
		char out_str[64];
		sprintf(out_str, "accel x:%f ms/2 y:%f ms/2 z:%f ms/2", sensor_value_to_double(&accel[0]), sensor_value_to_double(&accel[1]), sensor_value_to_double(&accel[2]));
		printk("%s\n", out_str);
		sprintf(out_str, "gyro x:%f dps y:%f dps z:%f dps", sensor_value_to_double(&gyro[0]), sensor_value_to_double(&gyro[1]), sensor_value_to_double(&gyro[2]));
		printk("%s\n", out_str);
	}
	return true;
}