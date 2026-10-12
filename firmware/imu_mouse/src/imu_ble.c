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

#include "imu_ble.h"
#include "threads.h"

static struct bt_uuid_128 s_service_uuid = BT_UUID_INIT_128(IMU_BLE_SERVICE_UUID_BYTES);
static struct bt_uuid_128 s_pose_uuid = BT_UUID_INIT_128(IMU_BLE_POSE_UUID_BYTES);
static struct bt_uuid_128 s_info_uuid = BT_UUID_INIT_128(IMU_BLE_INFO_UUID_BYTES);
static uint16_t s_sequence;

// Latest encoded frame; a newer pose overwrites an unsent one
static uint8_t s_pending_frame[IMU_BLE_POSE_FRAME_SIZE];
static bool s_pending;
static struct k_spinlock s_pending_lock;
static K_SEM_DEFINE(s_frame_ready, 0, 1);
static volatile uint16_t s_conn_interval;

// Diagnostics, printed once per second
static uint32_t s_stat_ok, s_stat_err, s_stat_max_us, s_stat_dropped, s_stat_gated;
static int s_stat_last_err;
static int64_t s_stat_next_ms;

// Send only once the fast connection interval (1.25 ms units) is in effect; 0 = no connection
#define MAX_SEND_INTERVAL_UNITS 12

/* Requested connection interval 7.5-11.25 ms (1.25 ms units), latency 0, 4 s timeout. */
#define FAST_CONN_PARAM BT_LE_CONN_PARAM(6, 9, 0, 400)

static const struct bt_data s_ad[] =
{
	BT_DATA_BYTES(BT_DATA_FLAGS, (BT_LE_AD_GENERAL | BT_LE_AD_NO_BREDR)),
	BT_DATA_BYTES(BT_DATA_UUID128_ALL, IMU_BLE_SERVICE_UUID_BYTES),
};

static const struct bt_data s_sd[] =
{
	BT_DATA(BT_DATA_NAME_COMPLETE, CONFIG_BT_DEVICE_NAME, sizeof(CONFIG_BT_DEVICE_NAME) - 1),
};

static struct k_work s_adv_work;

static void s_adv_process(struct k_work *work)
{
	ARG_UNUSED(work);
	const int err = bt_le_adv_start(BT_LE_ADV_CONN_FAST_2, s_ad, ARRAY_SIZE(s_ad), s_sd, ARRAY_SIZE(s_sd));
	if (err)
	{
		printk("Advertising failed to start (err %d)\n", err);
		return;
	}
	printk("Advertising started\n");
}

static void s_connected(struct bt_conn *conn, uint8_t err)
{
	if (err)
	{
		printk("Connection failed (err 0x%02x)\n", err);
		k_work_submit(&s_adv_work);
		return;
	}
	// Reads the interval when connection starts
	// Interval is set by the central, peripherial can request different interval later
	struct bt_conn_info info;
	if (bt_conn_get_info(conn, &info) == 0)
	{
		s_conn_interval = info.le.interval;
		printk("Conn begin: interval=%u\n", s_conn_interval);
	}
	const int param_err = bt_conn_le_param_update(conn, FAST_CONN_PARAM);
	if (param_err)
	{
		printk("Connection parameter update request failed (err %d)\n", param_err);
	}
}

static void s_disconnected(struct bt_conn *conn, uint8_t reason)
{
	ARG_UNUSED(conn);
	ARG_UNUSED(reason);
	// 0 means that no connection, nothing is set
	s_conn_interval = 0;
	printk("Conn end\n");
	k_work_submit(&s_adv_work);
}

static void s_le_param_updated(struct bt_conn *conn, uint16_t interval, uint16_t latency, uint16_t timeout)
{
	// This is called when the central accepts new interval values
	ARG_UNUSED(conn);
	printk("Conn params: interval=%u (%u.%02u ms) latency=%u timeout=%u\n", interval, interval * 5 / 4, (interval * 125) % 100, latency, timeout);
	s_conn_interval = interval;
}

BT_CONN_CB_DEFINE(s_imu_ble_conn_cb) =
{
	.connected = s_connected,
	.disconnected = s_disconnected,
	.le_param_updated = s_le_param_updated,
};

static void s_pose_ccc_changed(const struct bt_gatt_attr *attr, uint16_t value)
{
	ARG_UNUSED(attr);
	if (value & BT_GATT_CCC_NOTIFY)
	{
		printk("Pose notifications enabled\n");
	}
	else
	{
		printk("Pose notifications disabled\n");
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

BT_GATT_SERVICE_DEFINE(imu_pose_service,
	BT_GATT_PRIMARY_SERVICE(&s_service_uuid),
	BT_GATT_CHARACTERISTIC(&s_pose_uuid.uuid, BT_GATT_CHRC_NOTIFY, BT_GATT_PERM_NONE, NULL, NULL, NULL),
	BT_GATT_CCC(s_pose_ccc_changed, BT_GATT_PERM_READ | BT_GATT_PERM_WRITE),
	BT_GATT_CHARACTERISTIC(&s_info_uuid.uuid, BT_GATT_CHRC_READ, BT_GATT_PERM_READ, s_read_info, NULL, NULL),
);

static void s_sender_thread_fn(void *p1, void *p2, void *p3)
{
	ARG_UNUSED(p1);
	ARG_UNUSED(p2);
	ARG_UNUSED(p3);
	while (1)
	{
		k_sem_take(&s_frame_ready, K_MSEC(1000));

		uint8_t frame[IMU_BLE_POSE_FRAME_SIZE];
		k_spinlock_key_t key = k_spin_lock(&s_pending_lock);
		const bool have = s_pending;
		if (have)
		{
			memcpy(frame, s_pending_frame, sizeof(frame));
			s_pending = false;
		}
		k_spin_unlock(&s_pending_lock, key);

		if (have)
		{
			const uint32_t t0 = k_cycle_get_32();
			const int err = bt_gatt_notify(NULL, &imu_pose_service.attrs[2], frame, sizeof(frame));
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

		const int64_t now_ms = k_uptime_get();
		if (now_ms >= s_stat_next_ms)
		{
			printk("BLE notify: ok=%u err=%u (last %d) dropped=%u gated=%u max_call=%u us\n", s_stat_ok, s_stat_err, s_stat_last_err, s_stat_dropped, s_stat_gated, s_stat_max_us);
			s_stat_ok = s_stat_err = s_stat_max_us = s_stat_dropped = s_stat_gated = 0;
			s_stat_next_ms = now_ms + 1000;
		}
	}
}

K_THREAD_DEFINE(s_imu_ble_sender, IM_BLE_SENDER_THREAD_STACK_SIZE, s_sender_thread_fn, NULL, NULL, NULL, IM_BLE_SENDER_THREAD_PRIORITY, 0, 0);

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

bool imu_ble_init(void)
{
	printk("Initiating Bluetooth IMU\n");
	const int err = bt_enable(NULL);
	if (err)
	{
		printk("Bluetooth init failed (err %d)\n", err);
		return false;
	}
	printk("Bluetooth initialized\n");
	return true;
}

/* Must be called after bt_enable(). */
bool imu_ble_start(void)
{
	k_work_init(&s_adv_work, s_adv_process);
	if (IS_ENABLED(CONFIG_SETTINGS))
	{
		settings_load();
	}
	k_work_submit(&s_adv_work);
	printk("IMU BLE started\n");
	return true;
}

void imu_ble_push_pose(const struct imu_pose *pose)
{
	if (!pose)
	{
		return;
	}

	uint8_t frame[IMU_BLE_POSE_FRAME_SIZE] = {0};
	sys_put_le16(s_sequence++, &frame[0]);
	sys_put_le32(0, &frame[2]);
	sys_put_le16(s_clamp_i16(pose->qw * 16384.0f), &frame[6]);
	sys_put_le16(s_clamp_i16(pose->qx * 16384.0f), &frame[8]);
	sys_put_le16(s_clamp_i16(pose->qy * 16384.0f), &frame[10]);
	sys_put_le16(s_clamp_i16(pose->qz * 16384.0f), &frame[12]);
	/* Gyro is rad/s internally; the wire unit is 0.01 degree/s. */
	sys_put_le16(s_clamp_i16(pose->gx_rad_s * 5729.57795f), &frame[14]);
	sys_put_le16(s_clamp_i16(pose->gy_rad_s * 5729.57795f), &frame[16]);
	sys_put_le16(s_clamp_i16(pose->gz_rad_s * 5729.57795f), &frame[18]);

	const uint16_t interval = s_conn_interval;
	if (interval == 0 || interval > MAX_SEND_INTERVAL_UNITS)
	{
		s_stat_gated++;
		return;
	}

	k_spinlock_key_t key = k_spin_lock(&s_pending_lock);
	if (s_pending)
	{
		s_stat_dropped++;
	}
	memcpy(s_pending_frame, frame, sizeof(frame));
	s_pending = true;
	k_spin_unlock(&s_pending_lock, key);
	k_sem_give(&s_frame_ready);
}

