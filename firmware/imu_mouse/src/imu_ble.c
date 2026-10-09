#include <math.h>
#include <stdint.h>

#include <zephyr/kernel.h>
#include <zephyr/sys/byteorder.h>
#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/bluetooth/gatt.h>
#include <zephyr/bluetooth/uuid.h>

#include "imu_ble.h"

static struct bt_uuid_128 s_service_uuid = BT_UUID_INIT_128(IMU_BLE_SERVICE_UUID_BYTES);
static struct bt_uuid_128 s_pose_uuid = BT_UUID_INIT_128(IMU_BLE_POSE_UUID_BYTES);
static struct bt_uuid_128 s_info_uuid = BT_UUID_INIT_128(IMU_BLE_INFO_UUID_BYTES);
static uint16_t s_sequence;
static uint32_t s_last_send_ms;

static ssize_t s_read_info(struct bt_conn *conn, const struct bt_gatt_attr *attr, void *buf,
			   uint16_t len, uint16_t offset)
{
	ARG_UNUSED(attr);
	const uint8_t info[] = {
		IMU_BLE_PROTOCOL_VERSION,
		CONFIG_IMU_BLE_SAMPLE_RATE_HZ & 0xff,
		(CONFIG_IMU_BLE_SAMPLE_RATE_HZ >> 8) & 0xff,
		IS_ENABLED(CONFIG_IMU_BLE_SYNTHETIC_DATA) ? 1 : 0,
	};
	return bt_gatt_attr_read(conn, attr, buf, len, offset, info, sizeof(info));
}

BT_GATT_SERVICE_DEFINE(imu_pose_service,
	BT_GATT_PRIMARY_SERVICE(&s_service_uuid),
	BT_GATT_CHARACTERISTIC(&s_pose_uuid.uuid, BT_GATT_CHRC_NOTIFY, BT_GATT_PERM_NONE,
			       NULL, NULL, NULL),
	BT_GATT_CCC(NULL, BT_GATT_PERM_READ | BT_GATT_PERM_WRITE),
	BT_GATT_CHARACTERISTIC(&s_info_uuid.uuid, BT_GATT_CHRC_READ, BT_GATT_PERM_READ,
			       s_read_info, NULL, NULL),
);

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

int imu_ble_init(void)
{
	return 0;
}

void imu_ble_send_pose(const struct imu_pose *pose)
{
	if (!pose)
	{
		return;
	}

	const uint32_t now = k_uptime_get_32();
	const uint32_t period_ms = 1000 / CONFIG_IMU_BLE_SAMPLE_RATE_HZ;
	if ((uint32_t)(now - s_last_send_ms) < period_ms)
	{
		return;
	}
	s_last_send_ms = now;

	uint8_t frame[IMU_BLE_POSE_FRAME_SIZE] = {0};
	frame[0] = IMU_BLE_PROTOCOL_VERSION;
	frame[1] = (pose->orientation_valid ? BIT(0) : 0) |
		   (pose->button_pressed ? BIT(1) : 0);
	sys_put_le16(s_sequence++, &frame[2]);
	sys_put_le32(now, &frame[4]);
	sys_put_le16(s_clamp_i16(pose->qw * 16384.0f), &frame[8]);
	sys_put_le16(s_clamp_i16(pose->qx * 16384.0f), &frame[10]);
	sys_put_le16(s_clamp_i16(pose->qy * 16384.0f), &frame[12]);
	sys_put_le16(s_clamp_i16(pose->qz * 16384.0f), &frame[14]);
	/* Gyro is rad/s internally; the wire unit is 0.01 degree/s. */
	sys_put_le16(s_clamp_i16(pose->gx_rad_s * 5729.57795f), &frame[16]);
	sys_put_le16(s_clamp_i16(pose->gy_rad_s * 5729.57795f), &frame[18]);
	sys_put_le16(s_clamp_i16(pose->gz_rad_s * 5729.57795f), &frame[20]);

	/* BT_GATT_NOTIFY returns -ENOTCONN when there is no subscribed connection. */
	(void)bt_gatt_notify(NULL, &imu_pose_service.attrs[2], frame, sizeof(frame));
}
