
#ifndef HIDS_MOUSE_H_
#define HIDS_MOUSE_H_

#include <zephyr/types.h>

bool im_hids_mouse_init(void);
bool im_hids_mouse_start(void);

/* True while a pairing passkey awaits confirmation. */
bool im_hids_mouse_pairing_pending(void);

/* Accepts or rejects the pending pairing passkey. */
void im_hids_mouse_pairing_reply(bool accept);
void im_bas_notify(void);

/* Queues a relative movement; drops it and warns if the queue is full. */
void im_hids_mouse_move(int16_t dx, int16_t dy);

#endif /* HIDS_MOUSE_H_ */
