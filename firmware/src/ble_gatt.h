#ifndef IM_BLE_GATT_H
#define IM_BLE_GATT_H

#include <stdbool.h>
#include "math3d.h"

typedef void (*im_ble_gatt_session_cb_t)(bool ready);

bool im_ble_gatt_init(void);
bool im_ble_gatt_start(void);
void im_ble_gatt_set_session_callback(im_ble_gatt_session_cb_t callback);

// Returns true when queued; replaces an older pending frame if one has not been sent. */
bool im_ble_gatt_push_frame(im_quat orientation, im_vec3 gyro_rate);

#endif
