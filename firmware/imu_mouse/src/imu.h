#ifndef IM_IMU_H
#define IM_IMU_H

#define IM_IMU_SAMPLE_RATE_HZ 104

/* Called from the IMU thread with gyro rates in rad/s. */
typedef void (*im_imu_gyro_cb_t)(float gx, float gy, float gz);

void im_imu_set_gyro_cb(im_imu_gyro_cb_t cb);
bool im_imu_thread_start(void);
bool im_imu_init(const bool delay_init, const bool configure_sampling);

bool im_imu_collect(const bool do_print);

#endif