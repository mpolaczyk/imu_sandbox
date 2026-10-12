#ifndef IM_IMU_H
#define IM_IMU_H

/* Called from the IMU thread with gyro rates in rad/s and acceleration in m/s^2. */
typedef void (*im_imu_sample_cb_t)(float gx, float gy, float gz, float ax, float ay, float az);

void im_imu_set_sample_cb(im_imu_sample_cb_t cb);
bool im_imu_thread_start(void);
bool im_imu_init(const bool delay_init, const bool configure_sampling);

bool im_imu_collect(const bool do_print);

#endif