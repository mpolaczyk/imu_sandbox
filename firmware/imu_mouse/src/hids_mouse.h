
#ifndef HIDS_MOUSE_H_
#define HIDS_MOUSE_H_

#include <zephyr/types.h>

bool im_hids_mouse_init(void);
bool im_hids_mouse_start(void);
void im_bas_notify(void);

#endif /* HIDS_MOUSE_H_ */
