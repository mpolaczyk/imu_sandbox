#ifndef IM_THREADS_H
#define IM_THREADS_H

#include <zephyr/kernel.h>

// IMU thread - Cooperative: only ISRs preempt it
#define IM_IMU_THREAD_STACK_SIZE 2048
#define IM_IMU_THREAD_PRIORITY K_HIGHEST_APPLICATION_THREAD_PRIO

// Main thread - Preemptible: runs at CONFIG_MAIN_THREAD_PRIORITY, preempted by the IMU thread
#define IM_MAIN_THREAD_PERIOD_MS 500

// GPIO thread - Preemptible: lowest priority, runs only when other threads are blocked
#define IM_GPIO_THREAD_STACK_SIZE 512
#define IM_GPIO_THREAD_PRIORITY K_LOWEST_APPLICATION_THREAD_PRIO
#define IM_GPIO_THREAD_PERIOD_MS 100

// Log thread - Preemptible: lowest priority, runs only when other threads are blocked
#define IM_LOG_THREAD_STACK_SIZE 1024
#define IM_LOG_THREAD_PRIORITY K_LOWEST_APPLICATION_THREAD_PRIO
#define IM_LOG_THREAD_PERIOD_MS 250

#endif
