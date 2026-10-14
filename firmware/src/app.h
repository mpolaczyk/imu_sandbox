#ifndef IM_APP_H
#define IM_APP_H

#include <stdbool.h>
#include "math3d.h"

// Application events
void im_app_on_imu_sample(im_vec3 gyro_rate, im_vec3 acceleration, float dt_s);
void im_app_on_error(void);
void im_app_on_ble_session(bool ready);
void im_app_on_button(bool pressed);

#endif