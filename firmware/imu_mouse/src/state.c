#include <zephyr/kernel.h>

#include "state.h"

static atomic_t s_state = ATOMIC_INIT(IM_APP_STATE_INIT_1);

enum im_app_state im_state_get(void)
{
	return (enum im_app_state)atomic_get(&s_state);
}

void im_state_set(enum im_app_state state)
{
	atomic_set(&s_state, (atomic_val_t)state);
	printk("Entering state: %d\n", state);
}