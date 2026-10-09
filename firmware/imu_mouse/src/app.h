#ifndef IM_APP_H
#define IM_APP_H

void im_app_on_gyro(float gx, float gy, float gz, float ax, float ay, float az);

/* Cycles to the next cursor sensitivity level (wraps around). */
void im_app_sensitivity_next(void);

#endif