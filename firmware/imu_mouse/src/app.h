#ifndef IM_APP_H
#define IM_APP_H

#include <stdbool.h>

void im_app_on_sample(float gx, float gy, float gz, float ax, float ay, float az);

/* Button 1 edge: left mouse button press/release, or pairing confirmation while a passkey is pending. */
void im_app_on_button(bool pressed);

#endif