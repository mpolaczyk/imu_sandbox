#ifndef IM_IMU_H
#define IM_IMU_H

#include <stdbool.h>

/* Called with gyro rates in rad/s, acceleration in m/s^2, and elapsed time in seconds. */
typedef void (*im_imu_sample_cb_t)(float gx, float gy, float gz, float ax, float ay, float az, float dt_s);
typedef void (*im_imu_error_cb_t)(void);

void im_imu_set_sample_cb(im_imu_sample_cb_t cb);
void im_imu_set_error_callback(im_imu_error_cb_t cb);
/* Requests streaming; the IMU thread applies ODR and trigger changes asynchronously. */
void im_imu_set_streaming(bool enabled);
bool im_imu_thread_start(void);
bool im_imu_init();

#endif