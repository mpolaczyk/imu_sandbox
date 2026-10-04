#include <zephyr/logging/log_ctrl.h>

#include "log.h"
#include "threads.h"

// Internal linkage

static K_THREAD_STACK_DEFINE(s_log_thread_stack, IM_LOG_THREAD_STACK_SIZE);
static struct k_thread s_log_thread_data;

static void s_log_thread_fn(void *p1, void *p2, void *p3)
{
	ARG_UNUSED(p1);
	ARG_UNUSED(p2);
	ARG_UNUSED(p3);
	while (1)
	{
		if (!log_process())
		{
			k_msleep(IM_LOG_THREAD_PERIOD_MS);
		}
	}
}

// External linkage

void im_log_thread_start()
{
    log_flush();
	k_tid_t tid = k_thread_create(&s_log_thread_data, s_log_thread_stack,
				      K_THREAD_STACK_SIZEOF(s_log_thread_stack),
				      s_log_thread_fn, NULL, NULL, NULL,
				      IM_LOG_THREAD_PRIORITY, 0, K_NO_WAIT);
	k_thread_name_set(tid, "log");
}

