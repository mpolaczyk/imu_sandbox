#ifndef IM_BLE_GATT_H
#define IM_BLE_GATT_H

#include <stdbool.h>

bool im_ble_gatt_init(void);
bool im_ble_gatt_start(void);

// Returns true when queued; replaces an older pending frame if one has not been sent. */
bool im_ble_gatt_push_frame(float qw, float qx, float qy, float qz, float gx_rad_s, float gy_rad_s, float gz_rad_s);

#endif
