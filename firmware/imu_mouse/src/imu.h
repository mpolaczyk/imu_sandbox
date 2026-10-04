#ifndef IM_IMU_H
#define IM_IMU_H

bool im_imu_thread_start(void);
bool im_imu_init(const bool delay_init, const bool configure_sampling);

bool im_imu_collect(const bool do_print);

#endif