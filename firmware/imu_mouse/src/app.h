#ifndef IM_APP_H
#define IM_APP_H

void im_app_on_sample(float gx, float gy, float gz, float ax, float ay, float az);

/* Button 1 press: confirms a pending pairing, otherwise cycles the cursor sensitivity. */
void im_app_on_button1(void);

#endif