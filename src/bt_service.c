#include "bt_service.h"

#include <logging/log.h>

#include <bluetooth/bluetooth.h>
#include <bluetooth/hci.h>
#include <bluetooth/uuid.h>
#include <bluetooth/conn.h>
#include <bluetooth/gatt.h>

#define NUM_GPIOS 5

LOG_MODULE_REGISTER(bt_service);

// Prototypes

static void        bt_ready(int err);
static void        connected(struct bt_conn *connected, uint8_t err);
static void        disconnected(struct bt_conn *disconn, uint8_t reason);
static ssize_t     write_feature_0(struct bt_conn *conn, const struct bt_gatt_attr *attr, const void *buf, uint16_t len, uint16_t offset, uint8_t flags);
static ssize_t     write_feature_1(struct bt_conn *conn, const struct bt_gatt_attr *attr, const void *buf, uint16_t len, uint16_t offset, uint8_t flags);
static ssize_t     write_feature_2(struct bt_conn *conn, const struct bt_gatt_attr *attr, const void *buf, uint16_t len, uint16_t offset, uint8_t flags);
static ssize_t     write_feature_3(struct bt_conn *conn, const struct bt_gatt_attr *attr, const void *buf, uint16_t len, uint16_t offset, uint8_t flags);
static ssize_t     write_feature_4(struct bt_conn *conn, const struct bt_gatt_attr *attr, const void *buf, uint16_t len, uint16_t offset, uint8_t flags);
static ssize_t     write_feature(uint8_t idx, const void *buf, uint16_t len, uint16_t offset);
static void        ccc_cfg_changed(const struct bt_gatt_attr *attr, uint16_t value);

static void        set_gpio(uint8_t idx, uint16_t value);

// Static structures

static struct bt_conn *conn = NULL;

static struct bt_conn_cb conn_callbacks = {
    .connected = connected,
    .disconnected = disconnected,
};

#define MANUF_DATA_LEN 7
static uint8_t manuf_data[MANUF_DATA_LEN] = {
    0xFF, 0xFF,         // Manufacturer ID 
    0xDE,               // Feature #1
    0xAD,               // Feature #2
    0xBE,               // Feature #3
    0xEF,               // Feature #4
    0x00                // Feature #5
};

static const struct bt_data addvertisement_data[] = {
    { 
        .type = BT_DATA_FLAGS,
        .data_len = 1,
        .data = (uint8_t []) { (BT_LE_AD_GENERAL | BT_LE_AD_NO_BREDR) },
    },
    { 
        .type = BT_DATA_MANUFACTURER_DATA,
        .data_len = MANUF_DATA_LEN,
        .data = (const uint8_t *)(manuf_data),
    }
};

static struct bt_uuid_128 service_uuid = 
    BT_UUID_INIT_128(   0x5c, 0xf2, 0xab, 0x1a, 0x8b, 0x77, 0x45, 0x3e, 
                        0x9b, 0x21, 0x57, 0x51, 0x2e, 0x81, 0x00, 0xda);

static struct bt_uuid_128 feature_char_uuids[] =  {
    BT_UUID_INIT_128(   0xe1, 0x5b, 0xf6, 0x0d, 0x89, 0x44, 0x47, 0x5f,
                        0xaa, 0x56, 0x84, 0xf7, 0x97, 0x95, 0xd8, 0x98),
    BT_UUID_INIT_128(   0x5e, 0xe6, 0x02, 0x02, 0x12, 0x5d, 0x48, 0xf5,
                        0xaa, 0x98, 0xd7, 0x48, 0xa2, 0x52, 0x4b, 0x6c),
    BT_UUID_INIT_128(   0xb5, 0x21, 0x4d, 0x3f, 0xd6, 0xf6, 0x47, 0x51,
                        0xbc, 0x9f, 0x22, 0x9e, 0xbc, 0x63, 0xdf, 0x81),
    BT_UUID_INIT_128(   0xa7, 0x7c, 0x33, 0xb3, 0x05, 0x80, 0x46, 0x7a,
                        0xa3, 0x84, 0x8f, 0xd6, 0x83, 0x05, 0x05, 0x07),
    BT_UUID_INIT_128(   0xc0, 0x64, 0xe2, 0xb7, 0x2b, 0xfb, 0x4d, 0x45,
                        0xbb, 0x4e, 0x9b, 0x9e, 0x06, 0x5b, 0xea, 0x0a)
};

BT_GATT_SERVICE_DEFINE(gpio_svc,
    BT_GATT_PRIMARY_SERVICE(&service_uuid),
    BT_GATT_CHARACTERISTIC(&feature_char_uuids[0].uuid,
            BT_GATT_CHRC_READ | BT_GATT_CHRC_WRITE_WITHOUT_RESP,
            BT_GATT_PERM_WRITE, NULL, write_feature_0, (void *)1),
    BT_GATT_CHARACTERISTIC(&feature_char_uuids[1].uuid,
            BT_GATT_CHRC_READ | BT_GATT_CHRC_WRITE_WITHOUT_RESP,
            BT_GATT_PERM_WRITE, NULL, write_feature_1, (void *)1),
    BT_GATT_CHARACTERISTIC(&feature_char_uuids[2].uuid,
            BT_GATT_CHRC_READ | BT_GATT_CHRC_WRITE_WITHOUT_RESP,
            BT_GATT_PERM_WRITE, NULL, write_feature_2, (void *)1),
    BT_GATT_CHARACTERISTIC(&feature_char_uuids[3].uuid,
            BT_GATT_CHRC_READ | BT_GATT_CHRC_WRITE_WITHOUT_RESP,
            BT_GATT_PERM_WRITE, NULL, write_feature_3, (void *)1),
    BT_GATT_CHARACTERISTIC(&feature_char_uuids[4].uuid,
            BT_GATT_CHRC_READ | BT_GATT_CHRC_WRITE_WITHOUT_RESP,
            BT_GATT_PERM_WRITE, NULL, write_feature_4, (void *)1),
    BT_GATT_CCC(ccc_cfg_changed, BT_GATT_PERM_READ | BT_GATT_PERM_WRITE),
);


int32_t bt_service_start()
{
    int32_t ret;

    LOG_INF("Initialize Bluetooth");

    bt_conn_cb_register(&conn_callbacks);

    ret = bt_enable(bt_ready);
    if(ret) {
        LOG_ERR("Bluetooth init failed (err %d)", ret);
    }

    return ret;
}

static void bt_ready(int err)
{
    LOG_INF("Bluetooth ready");

    char addr_s[BT_ADDR_LE_STR_LEN];

    bt_addr_le_t addr = {0};
    size_t count = 1;

    if(err) {
        LOG_ERR("Bluetooth init failed (err %d)", err);
        return;
    }

    err = bt_le_adv_start(  BT_LE_ADV_CONN_NAME,
                            addvertisement_data,    ARRAY_SIZE(addvertisement_data),
                            NULL,                   0);

    if (err) {
        LOG_ERR("Advertising failed to start (err %d)\n", err);
        return;
    }

    bt_id_get(&addr, &count);
    bt_addr_le_to_str(&addr, addr_s, sizeof(addr_s));

    LOG_INF("Beacon started, advertising as %s\n", addr_s);
}

void connected(struct bt_conn *connected, uint8_t err)
{
    if (err) {
        LOG_ERR("Client connection failed (err %u)", err);
    } else {
        LOG_INF("Connected");
        if (!conn) {
            conn = bt_conn_ref(connected);
        }
    }
}

void disconnected(struct bt_conn *disconn, uint8_t reason)
{
    if (conn) {
        bt_conn_unref(conn);
        conn = NULL;
    }

    LOG_INF("Disconnected (reason %u)", reason);
}

ssize_t write_feature_0(   struct bt_conn *conn,
                const struct bt_gatt_attr *attr, const void *buf,
                uint16_t len, uint16_t offset, uint8_t flags)
{
    return write_feature(0, buf, len, offset);
}

ssize_t write_feature_1(   struct bt_conn *conn,
                const struct bt_gatt_attr *attr, const void *buf,
                uint16_t len, uint16_t offset, uint8_t flags)
{
    return write_feature(1, buf, len, offset);
}

ssize_t write_feature_2(   struct bt_conn *conn,
                const struct bt_gatt_attr *attr, const void *buf,
                uint16_t len, uint16_t offset, uint8_t flags)
{
    return write_feature(2, buf, len, offset);
}

ssize_t write_feature_3(   struct bt_conn *conn,
                const struct bt_gatt_attr *attr, const void *buf,
                uint16_t len, uint16_t offset, uint8_t flags)
{
    return write_feature(3, buf, len, offset);
}

ssize_t write_feature_4(   struct bt_conn *conn,
                const struct bt_gatt_attr *attr, const void *buf,
                uint16_t len, uint16_t offset, uint8_t flags)
{
    return write_feature(4, buf, len, offset);
}

ssize_t write_feature(uint8_t idx, const void *buf, uint16_t len, uint16_t offset)
{
    uint16_t value = 0; 

    if (offset + len > sizeof(value)) {
        LOG_ERR("Invalid offset received");
        return BT_GATT_ERR(BT_ATT_ERR_INVALID_OFFSET);
    }

    memcpy(&value, buf, len);

    set_gpio(idx, value);

    return 0;
}

void set_gpio(uint8_t idx, uint16_t value)
{
    LOG_INF("Set gpio:%u, value:%u", (uint32_t)idx, (uint32_t)value);

    if(idx >= NUM_GPIOS) {
        LOG_ERR("Invalid GPIO idx");
    }
}

void ccc_cfg_changed(const struct bt_gatt_attr *attr, uint16_t value)
{
    ARG_UNUSED(attr);
    LOG_INF("Notification state changed:%u", (uint32_t)value);
}