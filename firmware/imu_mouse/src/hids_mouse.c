#include <stddef.h>
#include <string.h>
#include <errno.h>
#include <inttypes.h>
#include <soc.h>
#include <assert.h>

#include <zephyr/types.h>
#include <zephyr/sys/printk.h>
#include <zephyr/sys/byteorder.h>
#include <zephyr/kernel.h>
#include <zephyr/drivers/gpio.h>

#include <zephyr/settings/settings.h>

#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/bluetooth/hci.h>
#include <zephyr/bluetooth/conn.h>
#include <zephyr/bluetooth/uuid.h>
#include <zephyr/bluetooth/gatt.h>

#include <zephyr/bluetooth/services/bas.h>
#include <bluetooth/services/hids.h>
#include <zephyr/bluetooth/services/dis.h>

/*
 * Origin: Nordic "peripheral_hids_mouse" sample, trimmed for a single-host IMU mouse.
 *
 * Feature inventory (status after the cleanup):
 *
 * | Feature                                      | Controlled by                            | Status  |
 * |----------------------------------------------|------------------------------------------|---------|
 * | Passkey/MITM pairing (button 1 confirms)     | SAMPLE_BT_HIDS_SECURITY_MITM             | kept    |
 * | Security and bonding (SMP, privacy, settings)| SAMPLE_BT_HIDS_SECURITY                  | kept    |
 * | Directed advertising to bonded peers         | SAMPLE_BT_DIRECTED_ADVERTISING           | kept    |
 * | Boot-mode mouse reports                      | HIDS pm event handler                    | kept    |
 * | Movement batching (queue, in-flight limit)   | application code                         | kept    |
 * | Fast connection parameter request            | FAST_CONN_PARAM                          | kept    |
 * | Battery service (static level, no updates)   | BT_BAS                                   | kept    |
 * | Device Information Service                   | BT_DIS, BT_DIS_*                         | kept    |
 * | HID report map: buttons/wheel + media report | hid_init()                               | kept, unused (candidates to trim, or
 * |                                              |                                          | extend for clicks)                   |
 * | Two simultaneous connections                 | BT_MAX_CONN, BT_HIDS_MAX_CLIENT_COUNT    | removed (one host at a time)         |
 * | Connection SCI (subrated intervals)          | BT_HIDS_SCI                              | removed |
 * | Continuous report sending                    | SAMPLE_BT_HIDS_CONTINUOUS_REPORT_SENDING | removed |
 */

#include "hids_mouse.h"
#include "imu_ble.h"

#define DEVICE_NAME     CONFIG_BT_DEVICE_NAME
#define DEVICE_NAME_LEN (sizeof(DEVICE_NAME) - 1)

#define BASE_USB_HID_SPEC_VERSION   0x0101

/* Number of input reports in this application. */
#define INPUT_REPORT_COUNT          3
/* Length of Mouse Input Report containing button data. */
#define INPUT_REP_BUTTONS_LEN       3
/* Length of Mouse Input Report containing movement data. */
#define INPUT_REP_MOVEMENT_LEN      3
/* Length of Mouse Input Report containing media player data. */
#define INPUT_REP_MEDIA_PLAYER_LEN  1
/* Index of Mouse Input Report containing button data. */
#define INPUT_REP_BUTTONS_INDEX     0
/* Index of Mouse Input Report containing movement data. */
#define INPUT_REP_MOVEMENT_INDEX    1
/* Index of Mouse Input Report containing media player data. */
#define INPUT_REP_MPLAYER_INDEX     2
/* Id of reference to Mouse Input Report containing button data. */
#define INPUT_REP_REF_BUTTONS_ID    1
/* Id of reference to Mouse Input Report containing movement data. */
#define INPUT_REP_REF_MOVEMENT_ID   2
/* Id of reference to Mouse Input Report containing media player data. */
#define INPUT_REP_REF_MPLAYER_ID    3

/* HIDs queue size. */
#define HIDS_QUEUE_SIZE 10

/* HIDS instance. */
BT_HIDS_DEF(hids_obj, INPUT_REP_BUTTONS_LEN, INPUT_REP_MOVEMENT_LEN, INPUT_REP_MEDIA_PLAYER_LEN);

static struct k_work hids_work;

/* Movement notifications awaiting completion. */
static atomic_t movement_in_flight = ATOMIC_INIT(0);

/* Allows a report to be queued for the next connection event while the previous one is sent. */
#define MOVEMENT_MAX_IN_FLIGHT 2

/* Requested connection interval 7.5-11.25 ms (1.25 ms units), latency 0, 4 s timeout. */
#define FAST_CONN_PARAM BT_LE_CONN_PARAM(6, 9, 0, 400)

struct mouse_pos 
{
	int16_t x_val;
	int16_t y_val;
};

/* Mouse movement queue. */
K_MSGQ_DEFINE(hids_queue, sizeof(struct mouse_pos), HIDS_QUEUE_SIZE, 4);

#if CONFIG_SAMPLE_BT_DIRECTED_ADVERTISING
/* Bonded address queue. */
K_MSGQ_DEFINE(bonds_queue, sizeof(bt_addr_le_t), CONFIG_BT_MAX_PAIRED, 4);
#endif /* CONFIG_SAMPLE_BT_DIRECTED_ADVERTISING */

static const struct bt_data ad[] = 
{
	BT_DATA_BYTES(BT_DATA_GAP_APPEARANCE, (CONFIG_BT_DEVICE_APPEARANCE >> 0) & 0xff, (CONFIG_BT_DEVICE_APPEARANCE >> 8) & 0xff),
	BT_DATA_BYTES(BT_DATA_FLAGS, (BT_LE_AD_GENERAL | BT_LE_AD_NO_BREDR)),
	BT_DATA_BYTES(BT_DATA_UUID16_ALL, BT_UUID_16_ENCODE(BT_UUID_HIDS_VAL), BT_UUID_16_ENCODE(BT_UUID_BAS_VAL)),
	BT_DATA_BYTES(BT_DATA_UUID128_ALL, IMU_BLE_SERVICE_UUID_BYTES),
};

static const struct bt_data sd[] = 
{
	BT_DATA(BT_DATA_NAME_COMPLETE, DEVICE_NAME, DEVICE_NAME_LEN),
};

/* Single connected host. */
static struct bt_conn *s_conn;
static bool s_in_boot_mode;

static bool is_adv_running;

static struct k_work adv_work;

static struct k_work pairing_work;

struct pairing_data_mitm 
{
	struct bt_conn *conn;
	unsigned int passkey;
};

K_MSGQ_DEFINE(mitm_queue, sizeof(struct pairing_data_mitm), CONFIG_BT_HIDS_MAX_CLIENT_COUNT, 4);

static void advertising_continue(void);

#if CONFIG_SAMPLE_BT_DIRECTED_ADVERTISING
static void bond_find(const struct bt_bond_info *info, void *user_data)
{
	if (k_msgq_put(&bonds_queue, (void *) &info->addr, K_NO_WAIT)) 
    {
		printk("No space in the queue for the bond.\n");
	}
}
#endif /* CONFIG_SAMPLE_BT_DIRECTED_ADVERTISING */

static void advertising_continue(void)
{
	struct bt_le_adv_param adv_param;

#if CONFIG_SAMPLE_BT_DIRECTED_ADVERTISING
	bt_addr_le_t addr;

	if (!k_msgq_get(&bonds_queue, &addr, K_NO_WAIT)) 
    {
		char addr_buf[BT_ADDR_LE_STR_LEN];
		int err;

		if (is_adv_running) 
        {
			err = bt_le_adv_stop();
			if (err) 
            {
				printk("Advertising failed to stop (err %d)\n", err);
				return;
			}
			is_adv_running = false;
		}

		adv_param = *BT_LE_ADV_CONN_DIR(&addr);
		adv_param.options |= BT_LE_ADV_OPT_DIR_ADDR_RPA;

		err = bt_le_adv_start(&adv_param, NULL, 0, NULL, 0);

		if (err) 
        {
			printk("Directed advertising failed to start (err %d)\n", err);
			return;
		}

		bt_addr_le_to_str(&addr, addr_buf, BT_ADDR_LE_STR_LEN);
		printk("Direct advertising to %s started\n", addr_buf);
		/* High-duty directed advertising expires on its own; connected() then gets an error code. */
	} 
    else
#endif /* CONFIG_SAMPLE_BT_DIRECTED_ADVERTISING */
	{
		int err;

		if (is_adv_running) 
        {
			return;
		}

		adv_param = *BT_LE_ADV_CONN_FAST_2;
		err = bt_le_adv_start(&adv_param, ad, ARRAY_SIZE(ad), sd, ARRAY_SIZE(sd));
		if (err) 
        {
			printk("Advertising failed to start (err %d)\n", err);
			return;
		}

		printk("Regular advertising started\n");
	}

	is_adv_running = true;
}

static void advertising_start(void)
{
#if CONFIG_SAMPLE_BT_DIRECTED_ADVERTISING
	k_msgq_purge(&bonds_queue);
	bt_foreach_bond(BT_ID_DEFAULT, bond_find, NULL);
#endif /* CONFIG_SAMPLE_BT_DIRECTED_ADVERTISING */

	k_work_submit(&adv_work);
}

static void advertising_process(struct k_work *work)
{
	advertising_continue();
}

static void pairing_process(struct k_work *work)
{
	int err;
	struct pairing_data_mitm pairing_data;

	char addr[BT_ADDR_LE_STR_LEN];

	err = k_msgq_peek(&mitm_queue, &pairing_data);
	if (err) 
    {
		return;
	}

	bt_addr_le_to_str(bt_conn_get_dst(pairing_data.conn), addr, sizeof(addr));

	printk("Passkey for %s: %06u\n", addr, pairing_data.passkey);
	printk("Press Button 1 to confirm.\n");
}


static void connected(struct bt_conn *conn, uint8_t err)
{
	char addr[BT_ADDR_LE_STR_LEN];

	is_adv_running = false;

	bt_addr_le_to_str(bt_conn_get_dst(conn), addr, sizeof(addr));

	if (err) 
    {
		if (err == BT_HCI_ERR_ADV_TIMEOUT) 
        {
			printk("High-duty directed advertising to %s timed out\n", addr);
			k_work_submit(&adv_work);
		} 
        else
        {
			printk("Failed to connect to %s 0x%02x %s\n", addr, err, bt_hci_err_to_str(err));
		}
		return;
	}

	printk("Connected %s\n", addr);

	err = bt_hids_connected(&hids_obj, conn);

	if (err) 
    {
		printk("Failed to notify HID service about connection\n");
		return;
	}

	s_conn = conn;
	s_in_boot_mode = false;

	err = bt_conn_le_param_update(conn, FAST_CONN_PARAM);
	if (err)
	{
		printk("Connection parameter update request failed (err %d)\n", err);
	}
}


static void disconnected(struct bt_conn *conn, uint8_t reason)
{
	int err;
	char addr[BT_ADDR_LE_STR_LEN];

	bt_addr_le_to_str(bt_conn_get_dst(conn), addr, sizeof(addr));

	printk("Disconnected from %s, reason 0x%02x %s\n", addr, reason, bt_hci_err_to_str(reason));

	err = bt_hids_disconnected(&hids_obj, conn);

	if (err) 
    {
		printk("Failed to notify HID service about disconnection\n");
	}

	if (s_conn == conn)
	{
		s_conn = NULL;
	}

	/* Completion callbacks are not guaranteed after a disconnect. */
	atomic_set(&movement_in_flight, 0);

	advertising_start();
}


#ifdef CONFIG_SAMPLE_BT_HIDS_SECURITY
static void security_changed(struct bt_conn *conn, bt_security_t level, enum bt_security_err err)
{
	char addr[BT_ADDR_LE_STR_LEN];

	bt_addr_le_to_str(bt_conn_get_dst(conn), addr, sizeof(addr));

	if (!err) 
    {
		printk("Security changed: %s level %u\n", addr, level);
	} 
    else 
    {
		printk("Security failed: %s level %u err %d %s\n", addr, level, err, bt_security_err_to_str(err));
	}
}
#endif /* CONFIG_SAMPLE_BT_HIDS_SECURITY */

BT_CONN_CB_DEFINE(conn_callbacks) = {
	.connected = connected,
	.disconnected = disconnected,
#ifdef CONFIG_SAMPLE_BT_HIDS_SECURITY
	.security_changed = security_changed,
#endif /* CONFIG_SAMPLE_BT_HIDS_SECURITY */
};

static void hids_pm_evt_handler(enum bt_hids_pm_evt evt, struct bt_conn *conn)
{
	if (conn != s_conn) 
    {
		return;
	}

	switch (evt) 
    {
	case BT_HIDS_PM_EVT_BOOT_MODE_ENTERED:
		printk("Boot mode entered\n");
		s_in_boot_mode = true;
		break;

	case BT_HIDS_PM_EVT_REPORT_MODE_ENTERED:
		printk("Report mode entered\n");
		s_in_boot_mode = false;
		break;

	default:
		break;
	}
}

static void hid_init(void)
{
	int err;
	struct bt_hids_init_param hids_init_param = { 0 };
	struct bt_hids_inp_rep *hids_inp_rep;
	static const uint8_t mouse_movement_mask[DIV_ROUND_UP(INPUT_REP_MOVEMENT_LEN, 8)] = {0};

	static const uint8_t report_map[] = 
    {
		0x05, 0x01,     /* Usage Page (Generic Desktop) */
		0x09, 0x02,     /* Usage (Mouse) */

		0xA1, 0x01,     /* Collection (Application) */

		/* Report ID 1: Mouse buttons + scroll/pan */
		0x85, 0x01,       /* Report Id 1 */
		0x09, 0x01,       /* Usage (Pointer) */
		0xA1, 0x00,       /* Collection (Physical) */
		0x95, 0x05,       /* Report Count (3) */
		0x75, 0x01,       /* Report Size (1) */
		0x05, 0x09,       /* Usage Page (Buttons) */
		0x19, 0x01,       /* Usage Minimum (01) */
		0x29, 0x05,       /* Usage Maximum (05) */
		0x15, 0x00,       /* Logical Minimum (0) */
		0x25, 0x01,       /* Logical Maximum (1) */
		0x81, 0x02,       /* Input (Data, Variable, Absolute) */
		0x95, 0x01,       /* Report Count (1) */
		0x75, 0x03,       /* Report Size (3) */
		0x81, 0x01,       /* Input (Constant) for padding */
		0x75, 0x08,       /* Report Size (8) */
		0x95, 0x01,       /* Report Count (1) */
		0x05, 0x01,       /* Usage Page (Generic Desktop) */
		0x09, 0x38,       /* Usage (Wheel) */
		0x15, 0x81,       /* Logical Minimum (-127) */
		0x25, 0x7F,       /* Logical Maximum (127) */
		0x81, 0x06,       /* Input (Data, Variable, Relative) */
		0x05, 0x0C,       /* Usage Page (Consumer) */
		0x0A, 0x38, 0x02, /* Usage (AC Pan) */
		0x95, 0x01,       /* Report Count (1) */
		0x81, 0x06,       /* Input (Data,Value,Relative,Bit Field) */
		0xC0,             /* End Collection (Physical) */

		/* Report ID 2: Mouse motion */
		0x85, 0x02,       /* Report Id 2 */
		0x09, 0x01,       /* Usage (Pointer) */
		0xA1, 0x00,       /* Collection (Physical) */
		0x75, 0x0C,       /* Report Size (12) */
		0x95, 0x02,       /* Report Count (2) */
		0x05, 0x01,       /* Usage Page (Generic Desktop) */
		0x09, 0x30,       /* Usage (X) */
		0x09, 0x31,       /* Usage (Y) */
		0x16, 0x01, 0xF8, /* Logical maximum (2047) */
		0x26, 0xFF, 0x07, /* Logical minimum (-2047) */
		0x81, 0x06,       /* Input (Data, Variable, Relative) */
		0xC0,             /* End Collection (Physical) */
		0xC0,             /* End Collection (Application) */

		/* Report ID 3: Advanced buttons */
		0x05, 0x0C,       /* Usage Page (Consumer) */
		0x09, 0x01,       /* Usage (Consumer Control) */
		0xA1, 0x01,       /* Collection (Application) */
		0x85, 0x03,       /* Report Id (3) */
		0x15, 0x00,       /* Logical minimum (0) */
		0x25, 0x01,       /* Logical maximum (1) */
		0x75, 0x01,       /* Report Size (1) */
		0x95, 0x01,       /* Report Count (1) */

		0x09, 0xCD,       /* Usage (Play/Pause) */
		0x81, 0x06,       /* Input (Data,Value,Relative,Bit Field) */
		0x0A, 0x83, 0x01, /* Usage (Consumer Control Configuration) */
		0x81, 0x06,       /* Input (Data,Value,Relative,Bit Field) */
		0x09, 0xB5,       /* Usage (Scan Next Track) */
		0x81, 0x06,       /* Input (Data,Value,Relative,Bit Field) */
		0x09, 0xB6,       /* Usage (Scan Previous Track) */
		0x81, 0x06,       /* Input (Data,Value,Relative,Bit Field) */

		0x09, 0xEA,       /* Usage (Volume Down) */
		0x81, 0x06,       /* Input (Data,Value,Relative,Bit Field) */
		0x09, 0xE9,       /* Usage (Volume Up) */
		0x81, 0x06,       /* Input (Data,Value,Relative,Bit Field) */
		0x0A, 0x25, 0x02, /* Usage (AC Forward) */
		0x81, 0x06,       /* Input (Data,Value,Relative,Bit Field) */
		0x0A, 0x24, 0x02, /* Usage (AC Back) */
		0x81, 0x06,       /* Input (Data,Value,Relative,Bit Field) */
		0xC0              /* End Collection */
	};

	hids_init_param.rep_map.data = report_map;
	hids_init_param.rep_map.size = sizeof(report_map);

	hids_init_param.info.bcd_hid = BASE_USB_HID_SPEC_VERSION;
	hids_init_param.info.b_country_code = 0x00;
	hids_init_param.info.flags = (BT_HIDS_REMOTE_WAKE | BT_HIDS_NORMALLY_CONNECTABLE);

	hids_inp_rep = &hids_init_param.inp_rep_group_init.reports[0];
	hids_inp_rep->size = INPUT_REP_BUTTONS_LEN;
	hids_inp_rep->id = INPUT_REP_REF_BUTTONS_ID;
	hids_init_param.inp_rep_group_init.cnt++;

	hids_inp_rep++;
	hids_inp_rep->size = INPUT_REP_MOVEMENT_LEN;
	hids_inp_rep->id = INPUT_REP_REF_MOVEMENT_ID;
	hids_inp_rep->rep_mask = mouse_movement_mask;
	hids_init_param.inp_rep_group_init.cnt++;

	hids_inp_rep++;
	hids_inp_rep->size = INPUT_REP_MEDIA_PLAYER_LEN;
	hids_inp_rep->id = INPUT_REP_REF_MPLAYER_ID;
	hids_init_param.inp_rep_group_init.cnt++;

	hids_init_param.is_mouse = true;
	hids_init_param.pm_evt_handler = hids_pm_evt_handler;

	err = bt_hids_init(&hids_obj, &hids_init_param);
	__ASSERT(err == 0, "HIDS initialization failed\n");
}

static void movement_sent(struct bt_conn *conn, void *user_data)
{
	ARG_UNUSED(conn);
	ARG_UNUSED(user_data);

	if (atomic_dec(&movement_in_flight) > 0)
	{
		k_work_submit(&hids_work);
	}
}

static void mouse_movement_send(int16_t x_delta, int16_t y_delta)
{
	int err;

	if (!s_conn)
	{
		return;
	}

	atomic_inc(&movement_in_flight);

	if (s_in_boot_mode) 
    {
		int8_t x_delta_sat = MAX(MIN(x_delta, SCHAR_MAX), SCHAR_MIN);
		int8_t y_delta_sat = MAX(MIN(y_delta, SCHAR_MAX), SCHAR_MIN);

		err = bt_hids_boot_mouse_inp_rep_send(&hids_obj, s_conn, NULL, x_delta_sat, y_delta_sat, movement_sent);
	} 
    else 
    {
		uint8_t x_buff[2];
		uint8_t y_buff[2];
		uint8_t buffer[INPUT_REP_MOVEMENT_LEN];

		int16_t x = MAX(MIN(x_delta, 0x07ff), -0x07ff);
		int16_t y = MAX(MIN(y_delta, 0x07ff), -0x07ff);

		/* Convert to little-endian. */
		sys_put_le16(x, x_buff);
		sys_put_le16(y, y_buff);

		/* Encode report. */
		BUILD_ASSERT(sizeof(buffer) == 3, "Only 2 axis, 12-bit each, are supported");

		buffer[0] = x_buff[0];
		buffer[1] = (y_buff[0] << 4) | (x_buff[1] & 0x0f);
		buffer[2] = (y_buff[1] << 4) | (y_buff[0] >> 4);

		err = bt_hids_inp_rep_send_userdata(&hids_obj, s_conn, INPUT_REP_MOVEMENT_INDEX, buffer, sizeof(buffer), movement_sent, NULL);
	}

	if (err) 
    {
		printk("Failed to send report: %d\n", err);
		atomic_dec(&movement_in_flight);
	}
}


/* Movement accumulated while a notification is still in flight. */
static int32_t pending_x;
static int32_t pending_y;

static void mouse_handler(struct k_work *work)
{
	struct mouse_pos pos;

	while (!k_msgq_get(&hids_queue, &pos, K_NO_WAIT)) 
    {
		pending_x += pos.x_val;
		pending_y += pos.y_val;
	}

	/* movement_sent() resubmits this work once an in-flight report completes. */
	if (atomic_get(&movement_in_flight) >= MOVEMENT_MAX_IN_FLIGHT || (!pending_x && !pending_y))
	{
		return;
	}

	mouse_movement_send(CLAMP(pending_x, -0x7ff, 0x7ff), CLAMP(pending_y, -0x7ff, 0x7ff));
	pending_x = 0;
	pending_y = 0;
}

#if defined(CONFIG_SAMPLE_BT_HIDS_SECURITY_MITM)
static void auth_passkey_display(struct bt_conn *conn, unsigned int passkey)
{
	char addr[BT_ADDR_LE_STR_LEN];

	bt_addr_le_to_str(bt_conn_get_dst(conn), addr, sizeof(addr));

	printk("Passkey for %s: %06u\n", addr, passkey);
}


static void auth_passkey_confirm(struct bt_conn *conn, unsigned int passkey)
{
	int err;

	struct pairing_data_mitm pairing_data;

	pairing_data.conn    = bt_conn_ref(conn);
	pairing_data.passkey = passkey;

	k_sched_lock();
	err = k_msgq_put(&mitm_queue, &pairing_data, K_NO_WAIT);
	if (err) 
    {
		printk("Pairing queue is full. Purge previous data.\n");
	}

	/* In the case of multiple pairing requests, trigger
	 * pairing confirmation which needed user interaction only
	 * once to avoid display information about all devices at
	 * the same time. Passkey confirmation for next devices will
	 * be proccess from queue after handling the earlier ones.
	 */
	if (k_msgq_num_used_get(&mitm_queue) == 1) 
    {
		k_work_submit(&pairing_work);
	}
	k_sched_unlock();
}


static void auth_cancel(struct bt_conn *conn)
{
	char addr[BT_ADDR_LE_STR_LEN];

	bt_addr_le_to_str(bt_conn_get_dst(conn), addr, sizeof(addr));

	printk("Pairing cancelled: %s\n", addr);
}

static struct bt_conn_auth_cb conn_auth_callbacks = {
	.passkey_display = auth_passkey_display,
	.passkey_confirm = auth_passkey_confirm,
	.cancel = auth_cancel,
};
#else
static struct bt_conn_auth_cb conn_auth_callbacks;
#endif /* CONFIG_SAMPLE_BT_HIDS_SECURITY_MITM */

#if defined(CONFIG_SAMPLE_BT_HIDS_SECURITY)
static void pairing_complete(struct bt_conn *conn, bool bonded)
{
	char addr[BT_ADDR_LE_STR_LEN];

	bt_addr_le_to_str(bt_conn_get_dst(conn), addr, sizeof(addr));

	printk("Pairing completed: %s, bonded: %d\n", addr, bonded);
}


static void pairing_failed(struct bt_conn *conn, enum bt_security_err reason)
{
	char addr[BT_ADDR_LE_STR_LEN];

	if (IS_ENABLED(CONFIG_SAMPLE_BT_HIDS_SECURITY_MITM)) 
    {
		struct pairing_data_mitm pairing_data;

		k_sched_lock();
		uint32_t cnt = k_msgq_num_used_get(&mitm_queue);

		for (size_t i = 0; i < cnt; i++) 
        {
			int err = k_msgq_get(&mitm_queue, &pairing_data, K_NO_WAIT);

			__ASSERT(!err, "k_msgq_get failed: %d", err);

			if (pairing_data.conn == conn) 
            {
				bt_conn_unref(pairing_data.conn);
			} 
            else 
            {
				err = k_msgq_put(&mitm_queue, &pairing_data, K_NO_WAIT);
				__ASSERT(!err, "k_msgq_put failed: %d", err);
			}
		}
		k_sched_unlock();
	}

	bt_addr_le_to_str(bt_conn_get_dst(conn), addr, sizeof(addr));

	printk("Pairing failed conn: %s, reason %d %s\n", addr, reason, bt_security_err_to_str(reason));
}


static struct bt_conn_auth_info_cb conn_auth_info_callbacks = {
	.pairing_complete = pairing_complete,
	.pairing_failed = pairing_failed
};
#else
static struct bt_conn_auth_info_cb conn_auth_info_callbacks;
#endif /* defined(CONFIG_SAMPLE_BT_HIDS_SECURITY) */


static void num_comp_reply(bool accept)
{
	struct pairing_data_mitm pairing_data;
	struct bt_conn *conn;

	k_sched_lock();
	if (k_msgq_get(&mitm_queue, &pairing_data, K_NO_WAIT) != 0) 
    {
		k_sched_unlock();
		return;
	}

	conn = pairing_data.conn;

	if (accept) 
    {
		bt_conn_auth_passkey_confirm(conn);
		printk("Numeric Match, conn %p\n", conn);
	} 
    else 
    {
		bt_conn_auth_cancel(conn);
		printk("Numeric Reject, conn %p\n", conn);
	}

	bt_conn_unref(pairing_data.conn);

	if (k_msgq_num_used_get(&mitm_queue)) 
    {
		k_work_submit(&pairing_work);
	}
	k_sched_unlock();
}

// External linkage

bool im_hids_mouse_pairing_pending(void)
{
	return k_msgq_num_used_get(&mitm_queue) > 0;
}

void im_hids_mouse_pairing_reply(bool accept)
{
	num_comp_reply(accept);
}

void im_hids_mouse_move(int16_t dx, int16_t dy)
{
	const struct mouse_pos pos = { .x_val = dx, .y_val = dy };

	if (k_msgq_put(&hids_queue, &pos, K_NO_WAIT))
	{
		printk("Warning: HID queue full, movement dropped (dx=%d dy=%d)\n", dx, dy);
		return;
	}
	k_work_submit(&hids_work);
}

bool im_hids_mouse_init(void)
{
    printk("Initiating Bluetooth Mouse\n");
	int err;
    if (IS_ENABLED(CONFIG_SAMPLE_BT_HIDS_SECURITY)) 
    {
		if (IS_ENABLED(CONFIG_SAMPLE_BT_HIDS_SECURITY_MITM)) 
        {
			err = bt_conn_auth_cb_register(&conn_auth_callbacks);
			if (err) 
            {
				printk("Failed to register authorization callbacks.\n");
				return false;
			}
		}
		err = bt_conn_auth_info_cb_register(&conn_auth_info_callbacks);
		if (err) 
        {
			printk("Failed to register authorization info callbacks.\n");
			return false;
		}
	}

	/* DIS initialized at system boot with SYS_INIT macro. */
	hid_init();

    err = bt_enable(NULL);
	if(err)
    {
	    printk("Bluetooth init failed (err %d)\n", err);
        return false;
    }

    printk("Bluetooth initialized\n");
	return true;
}

/* Must be called after bt_enable(). */
bool im_hids_mouse_start(void)
{
	k_work_init(&hids_work, mouse_handler);
	k_work_init(&adv_work, advertising_process);

	if (IS_ENABLED(CONFIG_SAMPLE_BT_HIDS_SECURITY_MITM)) 
    { 
        k_work_init(&pairing_work, pairing_process);
	}

	if (IS_ENABLED(CONFIG_SETTINGS)) 
    {
		settings_load();
	}

	advertising_start();

    printk("Mouse started\n");
	return true;
}
