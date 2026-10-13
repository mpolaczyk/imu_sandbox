#ifndef IM_APP_H
#define IM_APP_H

#include <stdbool.h>

// Application events
void im_app_on_imu_sample(float gx, float gy, float gz, float ax, float ay, float az);
void im_app_on_error(void);
void im_app_on_ble_session(bool ready);
void im_app_on_button(bool pressed);

#endif