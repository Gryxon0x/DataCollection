#include "ble_data_service.h"

#include <errno.h>
#include <stddef.h>
#include <string.h>

#include <zephyr/kernel.h>
#include <zephyr/sys/printk.h>
#include <zephyr/sys/util.h>

#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/bluetooth/conn.h>
#include <zephyr/bluetooth/gatt.h>
#include <zephyr/bluetooth/uuid.h>

/*
 * Custom BLE profile:
 *
 * Service:
 *   12345678-1234-5678-1234-56789abcdef0
 *
 * Command characteristic, WRITE:
 *   12345678-1234-5678-1234-56789abcdef1
 *
 * Data characteristic, NOTIFY:
 *   12345678-1234-5678-1234-56789abcdef2
 */

#define BT_UUID_BMA400_SERVICE_VAL \
    BT_UUID_128_ENCODE(0x12345678, 0x1234, 0x5678, 0x1234, 0x56789abcdef0)

#define BT_UUID_BMA400_COMMAND_VAL \
    BT_UUID_128_ENCODE(0x12345678, 0x1234, 0x5678, 0x1234, 0x56789abcdef1)

#define BT_UUID_BMA400_DATA_VAL \
    BT_UUID_128_ENCODE(0x12345678, 0x1234, 0x5678, 0x1234, 0x56789abcdef2)

#define BT_UUID_BMA400_SERVICE BT_UUID_DECLARE_128(BT_UUID_BMA400_SERVICE_VAL)
#define BT_UUID_BMA400_COMMAND BT_UUID_DECLARE_128(BT_UUID_BMA400_COMMAND_VAL)
#define BT_UUID_BMA400_DATA    BT_UUID_DECLARE_128(BT_UUID_BMA400_DATA_VAL)

#define COMMAND_BUFFER_SIZE 64

static struct bt_conn *current_conn;
static bool data_notify_enabled;

static void data_ccc_cfg_changed(const struct bt_gatt_attr *attr, uint16_t value);

static ssize_t command_write_cb(struct bt_conn *conn,
                                const struct bt_gatt_attr *attr,
                                const void *buf,
                                uint16_t len,
                                uint16_t offset,
                                uint8_t flags);

static const uint8_t adv_flags[] = {
    BT_LE_AD_GENERAL | BT_LE_AD_NO_BREDR
};

static const uint8_t adv_service_uuid[] = {
    BT_UUID_BMA400_SERVICE_VAL
};

static const struct bt_data ad[] = {
    BT_DATA(BT_DATA_FLAGS, adv_flags, sizeof(adv_flags)),
    BT_DATA(BT_DATA_NAME_COMPLETE,
            CONFIG_BT_DEVICE_NAME,
            sizeof(CONFIG_BT_DEVICE_NAME) - 1),
};

static const struct bt_data sd[] = {
    BT_DATA(BT_DATA_UUID128_ALL,
            adv_service_uuid,
            sizeof(adv_service_uuid)),
};

static ble_command_handler_t command_handler;

/*
 * Attribute indexes:
 * 0 - Primary service
 * 1 - Command characteristic declaration
 * 2 - Command characteristic value
 * 3 - Data characteristic declaration
 * 4 - Data characteristic value
 * 5 - Data CCC
 */
BT_GATT_SERVICE_DEFINE(bma400_svc,
    BT_GATT_PRIMARY_SERVICE(BT_UUID_BMA400_SERVICE),

    BT_GATT_CHARACTERISTIC(BT_UUID_BMA400_COMMAND,
                           BT_GATT_CHRC_WRITE | BT_GATT_CHRC_WRITE_WITHOUT_RESP,
                           BT_GATT_PERM_WRITE,
                           NULL,
                           command_write_cb,
                           NULL),

    BT_GATT_CHARACTERISTIC(BT_UUID_BMA400_DATA,
                           BT_GATT_CHRC_NOTIFY,
                           BT_GATT_PERM_NONE,
                           NULL,
                           NULL,
                           NULL),

    BT_GATT_CCC(data_ccc_cfg_changed,
                BT_GATT_PERM_READ | BT_GATT_PERM_WRITE)
);

static void connected_cb(struct bt_conn *conn, uint8_t err)
{
    if (err) {
        printk("BLE connection failed, err=%u\n", err);
        return;
    }

    current_conn = bt_conn_ref(conn);
    printk("BLE connected\n");
}

static void disconnected_cb(struct bt_conn *conn, uint8_t reason)
{
    printk("BLE disconnected, reason=%u\n", reason);

    data_notify_enabled = false;

    if (current_conn) {
        bt_conn_unref(current_conn);
        current_conn = NULL;
    }
}

BT_CONN_CB_DEFINE(conn_callbacks) = {
    .connected = connected_cb,
    .disconnected = disconnected_cb,
};

static void data_ccc_cfg_changed(const struct bt_gatt_attr *attr, uint16_t value)
{
    data_notify_enabled = (value == BT_GATT_CCC_NOTIFY);

    printk("Data notifications %s\n",
           data_notify_enabled ? "enabled" : "disabled");
}

bool ble_data_service_is_connected(void)
{
    return current_conn != NULL;
}

bool ble_data_service_notifications_enabled(void)
{
    return data_notify_enabled;
}

int ble_data_service_send_text(const char *text)
{
    size_t len;
    size_t offset = 0;

    if (text == NULL) {
        return -EINVAL;
    }

    if (current_conn == NULL) {
        printk("BLE send failed: not connected\n");
        return -ENOTCONN;
    }

    if (!data_notify_enabled) {
        printk("BLE send failed: notifications not enabled\n");
        return -EACCES;
    }

    len = strlen(text);

    while (offset < len) {
        uint16_t mtu = bt_gatt_get_mtu(current_conn);
        uint16_t payload_max = 20;

        if (mtu > 3) {
            payload_max = mtu - 3;
        }

        /*
         * Keep a conservative upper bound for now.
         * This makes the first test robust even without MTU negotiation.
         */
        payload_max = MIN(payload_max, 20);

        size_t chunk_len = MIN((size_t)payload_max, len - offset);

        int ret = bt_gatt_notify(current_conn,
                                 &bma400_svc.attrs[4],
                                 &text[offset],
                                 chunk_len);

        if (ret != 0) {
            printk("bt_gatt_notify failed: %d\n", ret);
            return ret;
        }

        offset += chunk_len;

        /*
         * Give BLE stack a little breathing room.
         * Later we can optimize this.
         */
        k_msleep(10);
    }

    return 0;
}

void ble_data_service_set_command_handler(ble_command_handler_t handler)
{
    command_handler = handler;
}

static ssize_t command_write_cb(struct bt_conn *conn,
                                const struct bt_gatt_attr *attr,
                                const void *buf,
                                uint16_t len,
                                uint16_t offset,
                                uint8_t flags)
{
    char command[COMMAND_BUFFER_SIZE];

    if (offset != 0) {
        return BT_GATT_ERR(BT_ATT_ERR_INVALID_OFFSET);
    }

    if (len >= sizeof(command)) {
        return BT_GATT_ERR(BT_ATT_ERR_INVALID_ATTRIBUTE_LEN);
    }

    memcpy(command, buf, len);
    command[len] = '\0';

    printk("BLE command received: %s\n", command);

    if (command_handler != NULL) {
        command_handler(command);
    } else {
        if (strcmp(command, "PING") == 0) {
            int ret = ble_data_service_send_text("PONG\n");
            printk("PONG send ret=%d\n", ret);
        } else {
            ble_data_service_send_text("UNKNOWN_COMMAND\n");
        }
    }

    return len;
}

int ble_data_service_init(void)
{
    int ret;

    printk("Initializing BLE\n");

    ret = bt_enable(NULL);
    if (ret != 0) {
        printk("bt_enable failed: %d\n", ret);
        return ret;
    }

    printk("BLE enabled\n");

    ret = bt_le_adv_start(BT_LE_ADV_CONN_FAST_1,
                          ad,
                          ARRAY_SIZE(ad),
                          sd,
                          ARRAY_SIZE(sd));

    if (ret != 0) {
        printk("bt_le_adv_start failed: %d\n", ret);
        return ret;
    }

    printk("BLE advertising as %s\n", CONFIG_BT_DEVICE_NAME);

    return 0;
}