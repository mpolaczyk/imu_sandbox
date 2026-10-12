#ifndef IM_STATE_H
#define IM_STATE_H

enum im_app_state
{
	IM_APP_STATE_INIT_1,
	IM_APP_STATE_INIT_2,
	IM_APP_STATE_INIT_3,
	IM_APP_STATE_CALIBRATING,
	IM_APP_STATE_RUNNING,
	IM_APP_STATE_FAIL
};

enum im_app_state im_state_get(void);
void im_state_set(enum im_app_state state);

#endif
