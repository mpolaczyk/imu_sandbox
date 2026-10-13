#include <math.h>
#include <stdint.h>
#include <errno.h>
#include <string.h>

#include <zephyr/kernel.h>
#include <zephyr/sys/byteorder.h>
#include <zephyr/sys/printk.h>
#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/bluetooth/gatt.h>
#include <zephyr/bluetooth/uuid.h>
#include <zephyr/settings/settings.h>

#include "ble_gatt.h"
#include "threads.h"

// Internal linkage

// Thread and synchronisation
static K_THREAD_STACK_DEFINE(s_ble_gatt_thread_stack, IM_BLE_GATT_THREAD_STACK_SIZE);
static struct k_thread s_ble_gatt_thread_data;
static K_SEM_DEFINE(s_pending_frame_ready, 0, 1);

// Latest encoded frame; a newer pose overwrites an unsent one
#define BLE_POSE_FRAME_SIZE 20
static bool s_pending;
static struct k_spinlock s_pending_lock;
static uint8_t s_pending_frame[BLE_POSE_FRAME_SIZE];
static uint16_t s_frame_sequence;
static bool s_link_connected;
static bool s_pose_notifications_enabled;
static volatile uint16_t s_connection_interval;
static im_ble_gatt_session_cb_t s_session_callback;

/* Connection intervals are expressed in 1.25 ms units. */
#define MIN_SEND_INTERVAL_UNITS 6
#define MAX_SEND_INTERVAL_UNITS 9

static void s_update_streaming(void)
{
	const bool fast_enough = s_connection_interval >= MIN_SEND_INTERVAL_UNITS && s_connection_interval <= MAX_SEND_INTERVAL_UNITS;
	const bool ready = s_link_connected && s_pose_notifications_enabled && fast_enough;
	if (!ready)
	{
		k_spinlock_key_t key = k_spin_lock(&s_pending_lock);
		s_pending = false;
		k_spin_unlock(&s_pending_lock, key);
	}
	if (s_session_callback)
	{
		s_session_callback(ready);
	}
}

// Diagnostics, printed once per second
#define PRINT_PERIODIC_STATS 1
#define STATS_PERIOD_MS 1000
static uint32_t s_stat_ok, s_stat_err, s_stat_max_us, s_stat_dropped, s_stat_gated;
static int s_stat_last_err;
static int64_t s_stat_next_ms;

// Service definition
// UUIDs match simulator/src/ble/ble-client.ts and docs/ble.md.
#define BLE_SERVICE_UUID_BYTES 0x9e, 0xca, 0xcc, 0x24, 0x0e, 0xe5, 0xa9, 0xe0, 0x93, 0xf3, 0xa3, 0xb5, 0x01, 0x00, 0x40, 0x7e
#define BLE_POSE_UUID_BYTES 0x9e, 0xca, 0xcc, 0x24, 0x0e, 0xe5, 0xa9, 0xe0, 0x93, 0xf3, 0xa3, 0xb5, 0x02, 0x00, 0x40, 0x7e
#define BLE_INFO_UUID_BYTES 0x9e, 0xca, 0xcc, 0x24, 0x0e, 0xe5, 0xa9, 0xe0, 0x93, 0xf3, 0xa3, 0xb5, 0x03, 0x00, 0x40, 0x7e
static struct bt_uuid_128 s_service_uuid = BT_UUID_INIT_128(BLE_SERVICE_UUID_BYTES);
static struct bt_uuid_128 s_pose_uuid = BT_UUID_INIT_128(BLE_POSE_UUID_BYTES);
static struct bt_uuid_128 s_info_uuid = BT_UUID_INIT_128(BLE_INFO_UUID_BYTES);

static void s_pose_ccc_changed(const struct bt_gatt_attr *attr, uint16_t value)
{
	// Client Characteristic Configuration Descriptor (CCCD)
	// Notifications enabled means that a client subscribed to the service
	// This is only informational
	ARG_UNUSED(attr);
	s_pose_notifications_enabled = (value & BT_GATT_CCC_NOTIFY) != 0;
	s_update_streaming();
	if (s_pose_notifications_enabled)
	{
		printk("BLE GATT pose notifications enabled\n");
	}
	else
	{
		printk("BLE GATT pose notifications disabled\n");
	}
}
static ssize_t s_read_info(struct bt_conn *conn, const struct bt_gatt_attr *attr, void *buf, uint16_t len, uint16_t offset)
{
	ARG_UNUSED(attr);
	const uint8_t info[] = 
	{
		CONFIG_IM_IMU_SAMPLE_RATE_HZ & 0xff,
		(CONFIG_IM_IMU_SAMPLE_RATE_HZ >> 8) & 0xff,
	};
	return bt_gatt_attr_read(conn, attr, buf, len, offset, info, sizeof(info));
}
BT_GATT_SERVICE_DEFINE(ble_gatt_service,
	BT_GATT_PRIMARY_SERVICE(&s_service_uuid),
	BT_GATT_CHARACTERISTIC(&s_pose_uuid.uuid, BT_GATT_CHRC_NOTIFY, BT_GATT_PERM_NONE, NULL, NULL, NULL),
	BT_GATT_CCC(s_pose_ccc_changed, BT_GATT_PERM_READ | BT_GATT_PERM_WRITE),
	BT_GATT_CHARACTERISTIC(&s_info_uuid.uuid, BT_GATT_CHRC_READ, BT_GATT_PERM_READ, s_read_info, NULL, NULL),
);

// Advertisement
static struct k_work s_adv_work;
static const struct bt_data s_ad[] =
{
	BT_DATA_BYTES(BT_DATA_FLAGS, (BT_LE_AD_GENERAL | BT_LE_AD_NO_BREDR)),
	BT_DATA_BYTES(BT_DATA_UUID128_ALL, BLE_SERVICE_UUID_BYTES),
};
static const struct bt_data s_sd[] =
{
	BT_DATA(BT_DATA_NAME_COMPLETE, CONFIG_BT_DEVICE_NAME, sizeof(CONFIG_BT_DEVICE_NAME) - 1),
};
static void s_adv_process(struct k_work *work)
{
	ARG_UNUSED(work);
	const int err = bt_le_adv_start(BT_LE_ADV_CONN_FAST_2, s_ad, ARRAY_SIZE(s_ad), s_sd, ARRAY_SIZE(s_sd));
	if (err)
	{
		printk("ERROR: BLE GATT advertising failed to start (err %d)\n", err);
		return;
	}
	printk("BLE GATT advertising started\n");
}

// Connection lifecycle and timing
// Requested connection interval 7.5-11.25 ms, latency 0, 4s timeout.
#define FAST_CONN_PARAM BT_LE_CONN_PARAM(MIN_SEND_INTERVAL_UNITS, MAX_SEND_INTERVAL_UNITS, 0, 400)
static void s_connected(struct bt_conn *conn, uint8_t err)
{
	if (err)
	{
		s_link_connected = false;
		s_pose_notifications_enabled = false;
		s_update_streaming();
		printk("Error: BLE GATT connection failed (err 0x%02x)\n", err);
		k_work_submit(&s_adv_work);
		return;
	}
	s_link_connected = true;
	s_connection_interval = 0;
	// Reads the interval when connection starts
	// Interval is set by the central, peripherial can request different interval later
	struct bt_conn_info info;
	if (bt_conn_get_info(conn, &info) == 0)
	{
		s_connection_interval = info.le.interval;
		printk("BLE GATT connection begin: interval=%u\n", s_connection_interval);
	}
	s_update_streaming();
	const int param_err = bt_conn_le_param_update(conn, FAST_CONN_PARAM);
	if (param_err)
	{
		printk("Error: BLE GATT connection parameter update request failed (err %d)\n", param_err);
	}
}
static void s_disconnected(struct bt_conn *conn, uint8_t reason)
{
	ARG_UNUSED(conn);
	ARG_UNUSED(reason);
	s_link_connected = false;
	s_pose_notifications_enabled = false;
	s_connection_interval = 0;
	s_update_streaming();
	printk("BLE GATT connection end\n");
	k_work_submit(&s_adv_work);
}
static void s_le_param_updated(struct bt_conn *conn, uint16_t interval, uint16_t latency, uint16_t timeout)
{
	// This is called when the central accepts new interval values
	ARG_UNUSED(conn);
	printk("BLE GATT connection params: interval=%u (%u.%02u ms) latency=%u timeout=%u\n", interval, interval * 5 / 4, (interval * 125) % 100, latency, timeout);
	s_connection_interval = interval;
	s_update_streaming();
}
BT_CONN_CB_DEFINE(s_connection_cb) =
{
	.connected = s_connected,
	.disconnected = s_disconnected,
	.le_param_updated = s_le_param_updated,
};

static void s_ble_gatt_thread_fn(void *p1, void *p2, void *p3)
{
	ARG_UNUSED(p1);
	ARG_UNUSED(p2);
	ARG_UNUSED(p3);
	while (1)
	{
		k_sem_take(&s_pending_frame_ready, K_FOREVER);

		// Copy frame from pending buffer
		uint8_t frame[BLE_POSE_FRAME_SIZE];
		k_spinlock_key_t key = k_spin_lock(&s_pending_lock);
		const bool have = s_pending;
		if (have)
		{
			memcpy(frame, s_pending_frame, sizeof(frame));
			s_pending = false;
		}
		k_spin_unlock(&s_pending_lock, key);
		// Send the local frame
		if (have)
		{
			const uint32_t t0 = k_cycle_get_32();
			const int err = bt_gatt_notify(NULL, &ble_gatt_service.attrs[2], frame, sizeof(frame));
			const uint32_t dt_us = k_cyc_to_us_floor32(k_cycle_get_32() - t0);
			if (dt_us > s_stat_max_us)
			{
				s_stat_max_us = dt_us;
			}
			if (err == 0)
			{
				s_stat_ok++;
			}
			else if (err != -ENOTCONN)
			{
				s_stat_err++;
				s_stat_last_err = err;
			}
		}
#if PRINT_PERIODIC_STATS
		const int64_t now_ms = k_uptime_get();
		if (have && now_ms >= s_stat_next_ms)
		{
			printk("BLE GATT notify: ok=%u err=%u (last %d) dropped=%u gated=%u max_call=%u us\n", s_stat_ok, s_stat_err, s_stat_last_err, s_stat_dropped, s_stat_gated, s_stat_max_us);
			s_stat_ok = s_stat_err = s_stat_max_us = s_stat_dropped = s_stat_gated = 0;
			s_stat_next_ms = now_ms + 1000;
		}
#endif
	}
}

static int16_t s_clamp_i16(float value)
{
	if (value > INT16_MAX)
	{
		return INT16_MAX;
	}
	if (value < INT16_MIN)
	{
		return INT16_MIN;
	}
	return (int16_t)lrintf(value);
}

// External linkage

bool im_ble_gatt_init(void)
{
	printk("BLE GATT initiating\n");
	const int err = bt_enable(NULL);
	if (err)
	{
		printk("BLE GATT init failed (err %d)\n", err);
		return false;
	}
	k_work_init(&s_adv_work, s_adv_process);
	if (IS_ENABLED(CONFIG_SETTINGS))
	{
		settings_load();
	}
	printk("BLE GATT initialized\n");
	return true;
}

void im_ble_gatt_set_session_callback(im_ble_gatt_session_cb_t callback)
{
	s_session_callback = callback;
	s_update_streaming();
}

bool im_ble_gatt_start(void)
{
	k_tid_t tid = k_thread_create(&s_ble_gatt_thread_data, s_ble_gatt_thread_stack,
					      K_THREAD_STACK_SIZEOF(s_ble_gatt_thread_stack),
					      s_ble_gatt_thread_fn, NULL, NULL, NULL,
					      IM_BLE_GATT_THREAD_PRIORITY, 0, K_NO_WAIT);
	if (!tid)
	{
		printk("Error: BLE GATT thread failed to start\n");
		return false;
	}
	k_thread_name_set(tid, "ble_gatt");
	k_work_submit(&s_adv_work);	// This begins the advertisement
	printk("BLE GATT started\n");
	return true;
}

bool im_ble_gatt_push_frame(float qw, float qx, float qy, float qz, float gx_rad_s, float gy_rad_s, float gz_rad_s)
{
	// Gate poses to honor the agreed interval
	const uint16_t interval = s_connection_interval;
	if (!s_link_connected || !s_pose_notifications_enabled || interval == 0 ||
	    interval > MAX_SEND_INTERVAL_UNITS)
	{
		s_stat_gated++;
		return false;
	}

	// Build the frame
	uint8_t frame[BLE_POSE_FRAME_SIZE] = {0};
	sys_put_le16(s_frame_sequence++, &frame[0]);
	sys_put_le32(0, &frame[2]);	// TODO remove
	sys_put_le16(s_clamp_i16(qw * 16384.0f), &frame[6]);
	sys_put_le16(s_clamp_i16(qx * 16384.0f), &frame[8]);
	sys_put_le16(s_clamp_i16(qy * 16384.0f), &frame[10]);
	sys_put_le16(s_clamp_i16(qz * 16384.0f), &frame[12]);
	// Gyro is rad/s internally; the wire unit is 0.01 degree/s.
	sys_put_le16(s_clamp_i16(gx_rad_s * 5729.57795f), &frame[14]);
	sys_put_le16(s_clamp_i16(gy_rad_s * 5729.57795f), &frame[16]);
	sys_put_le16(s_clamp_i16(gz_rad_s * 5729.57795f), &frame[18]);

	// Signal the thread but only if it processed previous frame
	// Otherwise, update the pending frame - last frame will be sent
	k_spinlock_key_t key = k_spin_lock(&s_pending_lock);
	if (s_pending)
	{
		s_stat_dropped++;
	}
	memcpy(s_pending_frame, frame, sizeof(frame));
	s_pending = true;
	k_spin_unlock(&s_pending_lock, key);
	k_sem_give(&s_pending_frame_ready);
	return true;
}

