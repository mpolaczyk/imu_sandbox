#include <stdio.h>
#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/sensor.h>
#include <zephyr/drivers/i2c.h>
#include <zephyr/drivers/gpio.h>

#include "fusion.h"
#include "imu.h"
#include "state.h"
#include "threads.h"

#define IMU_REG_WHO_AM_I 0x0F

// Internal linkage

// Thread and synchronisation
static K_THREAD_STACK_DEFINE(s_imu_thread_stack, IM_IMU_THREAD_STACK_SIZE);
static struct k_thread s_imu_thread_data;
static K_SEM_DEFINE(s_imu_data_ready, 0, 1);

// Streaming state desired state
static atomic_t s_streaming_requested;

// Measurement statistics
#define IMU_READY_TIMEOUT_MS 2000
#define IMU_READY_POLL_MS 5
#define IMU_WATCHDOG_MS 50

#if CONFIG_IM_IMU_PRINT_PERIODIC_STATS
static atomic_t s_sample_count; 		// Successfully collected samples
static atomic_t s_data_ready_count; 	// Data-ready interrupts, incremented by the ISR
static int64_t s_next_report_ms;
static atomic_t s_not_acquired_count; 	// Semaphore timeouts, when samples don't arrive
#endif

// Measurement data
static struct sensor_value s_accel[3];
static struct sensor_value s_gyro[3];
static uint32_t s_last_sample_cycles;
static bool s_sample_time_valid;
static im_imu_sample_cb_t s_sample_cb;
static im_imu_error_cb_t s_error_cb;

// Hardware
static const struct device* const s_imu = DEVICE_DT_GET_ONE(st_lsm6dsl);
static const struct i2c_dt_spec s_i2c_bus = I2C_DT_SPEC_GET(DT_NODELABEL(lsm6ds3tr_c));

static bool s_imu_set_sampling_frequency(uint16_t frequency_hz)
{
	const struct sensor_value odr = { .val1 = frequency_hz, .val2 = 0 };
	if (sensor_attr_set(s_imu, SENSOR_CHAN_ACCEL_XYZ, SENSOR_ATTR_SAMPLING_FREQUENCY, &odr) < 0)
	{
		printk("Error: IMU failed to set accelerometer ODR to %u Hz\n", frequency_hz);
		return false;
	}
	if (sensor_attr_set(s_imu, SENSOR_CHAN_GYRO_XYZ, SENSOR_ATTR_SAMPLING_FREQUENCY, &odr) < 0)
	{
		printk("Error: IMU failed to set gyroscope ODR to %u Hz\n", frequency_hz);
		return false;
	}
	return true;
}
// IMU data-ready interrupt
static void s_imu_data_ready_handler(const struct device *dev, const struct sensor_trigger *trig)
{
	ARG_UNUSED(dev);
	ARG_UNUSED(trig);
#if CONFIG_IM_IMU_PRINT_PERIODIC_STATS
	atomic_inc(&s_data_ready_count);
#endif
	k_sem_give(&s_imu_data_ready);
}

bool s_imu_collect()
{
	// I2C moves data to driver memory, clears the interrupt
	if (sensor_sample_fetch(s_imu) < 0)
	{
		printk("Error: IMU Sensor sample update error\n");
		return false;
	}
	const uint32_t sample_cycles = k_cycle_get_32();
	float dt_s = 0.0f;
	if (s_sample_time_valid)
	{
		const uint32_t elapsed_cycles = sample_cycles - s_last_sample_cycles;
		dt_s = (float)k_cyc_to_us_floor64(elapsed_cycles) * 1e-6f;
	}
	s_last_sample_cycles = sample_cycles;
	s_sample_time_valid = true;
	sensor_channel_get(s_imu, SENSOR_CHAN_ACCEL_XYZ, s_accel);
	sensor_channel_get(s_imu, SENSOR_CHAN_GYRO_XYZ, s_gyro);
	const im_vec3 gyro_rate = {
		sensor_value_to_float(&s_gyro[0]),
		sensor_value_to_float(&s_gyro[1]),
		sensor_value_to_float(&s_gyro[2]),
	};
	const im_vec3 acceleration = {
		sensor_value_to_float(&s_accel[0]),
		sensor_value_to_float(&s_accel[1]),
		sensor_value_to_float(&s_accel[2]),
	};
	// Notify callback
	if (s_sample_cb)
	{
		s_sample_cb(gyro_rate, acceleration, dt_s);
	}
#if CONFIG_IM_IMU_PRINT_SAMPLE
	char out_str[64];
	sprintf(out_str, "accel x:%f ms/2 y:%f ms/2 z:%f ms/2", sensor_value_to_float(&s_accel[0]), sensor_value_to_float(&s_accel[1]), sensor_value_to_float(&s_accel[2]));
	printk("%s\n", out_str);
	sprintf(out_str, "gyro x:%f dps y:%f dps z:%f dps", sensor_value_to_float(&s_gyro[0]), sensor_value_to_float(&s_gyro[1]), sensor_value_to_float(&s_gyro[2]));
	printk("%s\n", out_str);
#endif
	return true;
}

static void s_imu_thread_fn(void *p1, void *p2, void *p3)
{
	ARG_UNUSED(p1);
	ARG_UNUSED(p2);
	ARG_UNUSED(p3);
	bool streaming = false;
	while (1)
	{
		if (!streaming)
		{
			k_sem_take(&s_imu_data_ready, K_FOREVER);
			if (!atomic_get(&s_streaming_requested))
			{
				continue;
			}

			// Start streaming after the request wakes the idle thread.
			k_sem_reset(&s_imu_data_ready);
			if (!s_imu_set_sampling_frequency(CONFIG_IM_IMU_SAMPLE_RATE_HZ))
			{
				if (s_error_cb) s_error_cb();
				return;
			}
			streaming = true;
			s_sample_time_valid = false;
			im_fusion_reset();
			continue;
		}
		const bool acquired = k_sem_take(&s_imu_data_ready, K_MSEC(IMU_WATCHDOG_MS)) == 0;
		if (!atomic_get(&s_streaming_requested))
		{
			// Stop before collecting another sample.
			if (!s_imu_set_sampling_frequency(0))
			{
				if (s_error_cb) s_error_cb();
				return;
			}
			streaming = false;
			s_sample_time_valid = false;
			continue;
		}

		// The watchdog read can recover a sample if the interrupt edge was missed.
#if CONFIG_IM_IMU_PRINT_PERIODIC_STATS
		if (!acquired)
		{
			atomic_inc(&s_not_acquired_count);
		}
#else
		ARG_UNUSED(acquired);
#endif
		if (!s_imu_collect())
		{
			if (s_error_cb) s_error_cb();
			return;
		}
		// Statistics reporting
#if CONFIG_IM_IMU_PRINT_PERIODIC_STATS
		atomic_inc(&s_sample_count);
		const int64_t now_ms = k_uptime_get();
		if (now_ms >= s_next_report_ms)
		{
			printk("IMU: data_ready_count=%d sample_count=%d not_acquired=%d\n", 
				(int)atomic_set(&s_data_ready_count, 0), 
				(int)atomic_set(&s_sample_count, 0), 
				(int)atomic_set(&s_not_acquired_count, 0));
			s_next_report_ms = now_ms + CONFIG_IM_IMU_STATS_PERIOD_MS;
		}
#endif
	}
}

static bool s_imu_wait_ready(void)
{
	// The driver init fails, if the chip does not ACK, probe I2C first
	uint8_t who_am_i;
	if (!i2c_is_ready_dt(&s_i2c_bus))
	{
		printk("Error: IMU I2C bus is not ready\n");
		return false;
	}
	const int64_t start = k_uptime_get();
	while (i2c_reg_read_byte_dt(&s_i2c_bus, IMU_REG_WHO_AM_I, &who_am_i) < 0)
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

void im_imu_set_sample_cb(im_imu_sample_cb_t cb)
{
	s_sample_cb = cb;
}

void im_imu_set_error_callback(im_imu_error_cb_t cb)
{
	s_error_cb = cb;
}

void im_imu_set_streaming(bool enabled)
{
	// Give the semaphore to cycle the thread
	// Bool flag will control the IMU device init/deinit
	if (enabled)
	{
		if (atomic_cas(&s_streaming_requested, 0, 1))
		{
			k_sem_give(&s_imu_data_ready);
		}
	}
	else
	{
		if (atomic_cas(&s_streaming_requested, 1, 0))
		{
			k_sem_give(&s_imu_data_ready);
		}
	}
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
	return true;
}

bool im_imu_init()
{
	if (!s_imu) 
	{
        printk("Error: IMU device pointer not found\n");
        return false;
    }
	// Delayed init - normal (OS) init does not work (I2C powerline not ready)
	if (!s_imu_wait_ready())
	{
		return false;
	}
	printk("IMU Manual delayed initialization\n");
    const int err = device_init(s_imu);
    if (err)
	{
      	printk("Error: I2C initialization failed, error code: %d\n", err);
    	return false;
    }
	if(!device_is_ready(s_imu)) 
	{
		const uint8_t state = s_imu->state->init_res;
		const bool initialized = s_imu->state->initialized;
		printk("Error: IMU not ready, init_res=%d init=%d\n", state, initialized);
		return false;
	}
	printk("IMU stopping sampling until BLE session starts\n");
	if (!s_imu_set_sampling_frequency(0))
	{
		return false;
	}
	printk("IMU Ok\n");
	return true;
}