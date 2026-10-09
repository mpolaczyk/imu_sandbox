#include <stdio.h>
#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/sensor.h>
#include <zephyr/drivers/i2c.h>

#include "imu.h"
#include "state.h"
#include "threads.h"

#define PRINT_MEASUREMENT false

#define IMU_REG_WHO_AM_I 0x0F
#define IMU_READY_TIMEOUT_MS 2000
#define IMU_READY_POLL_MS 5

// Internal linkage

static K_THREAD_STACK_DEFINE(s_imu_thread_stack, IM_IMU_THREAD_STACK_SIZE);
static struct k_thread s_imu_thread_data;
static K_SEM_DEFINE(s_imu_data_ready, 0, 1);

static struct sensor_value s_accel[3];
static struct sensor_value s_gyro[3];
static const struct device* const s_imu = DEVICE_DT_GET_ONE(st_lsm6dsl);
static im_imu_gyro_cb_t s_gyro_cb;

static void s_imu_data_ready_handler(const struct device *dev, const struct sensor_trigger *trig)
{
	ARG_UNUSED(dev);
	ARG_UNUSED(trig);
	k_sem_give(&s_imu_data_ready);
}

static void s_imu_thread_fn(void *p1, void *p2, void *p3)
{
	ARG_UNUSED(p1);
	ARG_UNUSED(p2);
	ARG_UNUSED(p3);
	while (1)
	{
		k_sem_take(&s_imu_data_ready, K_FOREVER);
		if (!im_imu_collect(PRINT_MEASUREMENT))
		{
			im_state_set(IM_APP_STATE_FAIL);
			return;
		}
	}
}

static bool s_imu_wait_ready(void)
{
	// The driver init fails for good if the chip does not ACK yet, so probe first
	static const struct i2c_dt_spec bus = I2C_DT_SPEC_GET(DT_NODELABEL(lsm6ds3tr_c));
	uint8_t who_am_i;
	if (!i2c_is_ready_dt(&bus))
	{
		printk("Error: IMU I2C bus is not ready\n");
		return false;
	}
	const int64_t start = k_uptime_get();
	while (i2c_reg_read_byte_dt(&bus, IMU_REG_WHO_AM_I, &who_am_i) < 0)
	{
		if (k_uptime_get() - start > IMU_READY_TIMEOUT_MS)
		{
			printk("Error: IMU does not respond on I2C\n");
			return false;
		}
		k_msleep(IMU_READY_POLL_MS);
	}
	printk("IMU responded on I2C after %d ms, who_am_i=0x%x\n", (int)(k_uptime_get() - start), who_am_i);
	return true;
}

// External linkage

void im_imu_set_gyro_cb(im_imu_gyro_cb_t cb)
{
	s_gyro_cb = cb;
}

bool im_imu_thread_start()
{
	static const struct sensor_trigger trig =
	{
		.type = SENSOR_TRIG_DATA_READY,
		.chan = SENSOR_CHAN_ACCEL_XYZ,
	};
	if (sensor_trigger_set(s_imu, &trig, s_imu_data_ready_handler) < 0)
	{
		printk("Error: IMU Cannot set data ready trigger\n");
		return false;
	}
	k_tid_t tid = k_thread_create(&s_imu_thread_data, s_imu_thread_stack,
				      K_THREAD_STACK_SIZEOF(s_imu_thread_stack),
				      s_imu_thread_fn, NULL, NULL, NULL,
				      IM_IMU_THREAD_PRIORITY, IM_IMU_THREAD_OPTIONS, K_NO_WAIT);
	k_thread_name_set(tid, "imu");
	k_sem_give(&s_imu_data_ready);
	return true;
}

bool im_imu_init(const bool delay_init, const bool configure_sampling)
{
	struct sensor_value odr_attr;
	if (!s_imu) 
	{
        printk("Error: IMU device pointer not found\n");
        return false;
    }
	if(delay_init)
	{
		// Delayed init is implemented because normal init does not work (timing)
		if (!s_imu_wait_ready())
		{
			return false;
		}
		printk("IMU Manual delayed initialization\n");
    	const int err = device_init(s_imu);
    	if (err)
		{
        	printk("Error: IMU initialization failed, error code: %d\n", err);
       		return false;
    	}
	}
	if(!device_is_ready(s_imu)) 
	{
		const uint8_t state = s_imu->state->init_res;
		const bool initialized = s_imu->state->initialized;
		printk("Error: IMU not ready, init_res=%d init=%d\n", state, initialized);
		return false;
	}
	if(configure_sampling)
	{
		printk("IMU configuring sampling\n");
		odr_attr.val1 = IM_IMU_SAMPLE_RATE_HZ;
		odr_attr.val2 = 0;
		if (sensor_attr_set(s_imu, SENSOR_CHAN_ACCEL_XYZ, SENSOR_ATTR_SAMPLING_FREQUENCY, &odr_attr) < 0) 
		{
			printk("Error: IMU Cannot set sampling frequency for accelerometer.\n");
			return false;
		}
		if (sensor_attr_set(s_imu, SENSOR_CHAN_GYRO_XYZ, SENSOR_ATTR_SAMPLING_FREQUENCY, &odr_attr) < 0) 
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
	if (sensor_sample_fetch(s_imu) < 0) // I2C move to driver memory, clear interrupt
	{
		printk("Error: IMU Sensor sample update error\n");
		return false;
	}

	sensor_channel_get(s_imu, SENSOR_CHAN_ACCEL_XYZ, s_accel);
	sensor_channel_get(s_imu, SENSOR_CHAN_GYRO_XYZ, s_gyro);

	if(do_print)
	{
		char out_str[64];
		sprintf(out_str, "accel x:%f ms/2 y:%f ms/2 z:%f ms/2", sensor_value_to_float(&s_accel[0]), sensor_value_to_float(&s_accel[1]), sensor_value_to_float(&s_accel[2]));
		printk("%s\n", out_str);
		sprintf(out_str, "gyro x:%f dps y:%f dps z:%f dps", sensor_value_to_float(&s_gyro[0]), sensor_value_to_float(&s_gyro[1]), sensor_value_to_float(&s_gyro[2]));
		printk("%s\n", out_str);
	}
	if (s_gyro_cb)
	{
		s_gyro_cb(sensor_value_to_float(&s_gyro[0]), sensor_value_to_float(&s_gyro[1]), sensor_value_to_float(&s_gyro[2]));
	}
	return true;
}