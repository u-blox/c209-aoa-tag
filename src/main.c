/*
 * Copyright 2021 u-blox
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *  http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <zephyr.h>

#include <string.h>
#include <bluetooth/bluetooth.h>
#include <bluetooth/hci.h>
#include "leds.h"
#include "bt_adv.h"
#include "buttons.h"
#include <random/rand32.h>
#include <sys/__assert.h>
#include <device.h>
#include <drivers/sensor.h>
#include "bt_util.h"
#include <bluetooth/bluetooth.h>
#include <bluetooth/hci.h>
#include <bluetooth/hci_vs.h>
#include <sys/byteorder.h>
#include "production.h"
#include "storage.h"
#include <logging/log.h>

#include <bluetooth/services/nus.h>


LOG_MODULE_REGISTER(app, CONFIG_APPLICATION_MODULE_LOG_LEVEL);

#define BLINK_STACKSIZE         512
#define BLINK_PRIORITY          7
#define NUM_ADV_INTERVALS       3
#define LED_BLINK_INTERVAL_MS   150
#define BLINK_INTERVAL_MS       5000

static void btReadyCb(int err);
static void onButtonPressCb(buttonPressType_t type);
static void setTxPower(uint8_t handleType, uint16_t handle, int8_t txPwrLvl);
static void blink(void);

static void connected(struct bt_conn *conn, uint8_t err);
static void disconnected(struct bt_conn *conn, uint8_t reason);
static void nus_send_data(char* data);
static void bt_receive_cb(struct bt_conn *conn, const uint8_t *const data, uint16_t len);

BT_CONN_CB_DEFINE(conn_callbacks) = {
    .connected    = connected,
    .disconnected = disconnected,
};

static bool isAdvRunning = true;
static uint16_t advIntervals[NUM_ADV_INTERVALS] = {50, 100, 1000};
static uint8_t advIntervalIndex = 0;
static char* pDefaultGroupNamespace = "NINA-B4TAG";

struct k_timer blinkTimer;
static uint8_t bluetoothReady;
static uint8_t uuid[EDDYSTONE_INSTANCE_ID_LEN];

static struct bt_conn *current_conn;
static uint32_t nus_max_send_len;

static struct bt_nus_cb nus_cb = {
    .received = bt_receive_cb,
};

K_THREAD_DEFINE(blinkThreadId, BLINK_STACKSIZE, blink, NULL, NULL, NULL, BLINK_PRIORITY, 0, K_TICKS_FOREVER);


void main(void)
{
    bt_addr_le_t addr;
    utilGetBtAddr(&addr);
    bluetoothReady = 0;

    storageInit();

    // Only swap public address. It's done like this in u-connect.
    if (addr.type == BT_ADDR_LE_PUBLIC) {
        for (uint8_t i = 0; i < EDDYSTONE_INSTANCE_ID_LEN; i++) {
            uuid[i] = addr.a.val[(EDDYSTONE_INSTANCE_ID_LEN-1) - i];
        }
    } else {
        memcpy(uuid, addr.a.val, MAC_ADDR_LEN);
    }
    LOG_HEXDUMP_INF(uuid, EDDYSTONE_INSTANCE_ID_LEN, "InstanceId (MAC)");

    productionStart();

    ledsInit();
    ledsSetState(LED_RED, 0);
    ledsSetState(LED_GREEN, 0);
    ledsSetState(LED_BLUE, 0);

    buttonsInit(&onButtonPressCb);

    __ASSERT(bt_enable(btReadyCb) == 0, "Bluetooth init failed");

    int err = bt_nus_init(&nus_cb);
    if (err) {
        LOG_ERR("Failed to initialize UART service (err: %d)", err);
        return;
    }

    k_thread_start(blinkThreadId);
}

static void blink(void) {
    LOG_INF("Started blink thread");
    while (1) {
        if (isAdvRunning) {
            ledsSetState(LED_BLUE, 1);
            k_sleep(K_MSEC(10));
            ledsSetState(LED_BLUE, 0);
        }
        k_msleep(BLINK_INTERVAL_MS);
    }
}


static void btReadyCb(int err)
{
    int8_t txPower = 0;
    __ASSERT(err == 0, "Bluetooth init failed (err %d)", err);
    LOG_INF("Bluetooth initialized");
    bluetoothReady = 1;

    storageGetTxPower(&txPower);
    LOG_INF("Setting TxPower: %d", txPower);
    setTxPower(BT_HCI_VS_LL_HANDLE_TYPE_ADV, 0, txPower);

    btAdvInit(advIntervals[advIntervalIndex], advIntervals[advIntervalIndex], pDefaultGroupNamespace, uuid, txPower);
    btAdvStart();
}

static void onButtonPressCb(buttonPressType_t type) {
    LOG_INF("Pressed, type: %d", type);

    if (type == BUTTONS_SHORT_PRESS) {
        // If stopped, then restart if adv. interval was changed
        isAdvRunning = true;
        advIntervalIndex = (advIntervalIndex + 1) % NUM_ADV_INTERVALS;
        uint16_t new_adv_interval = advIntervals[advIntervalIndex];
        LOG_INF("New interval: %d => %d", advIntervalIndex, new_adv_interval);
        btAdvUpdateAdvInterval(new_adv_interval, new_adv_interval);

        // Blink advertising interval index times
        ledsSetState(LED_BLUE, 1);
        for (int i = 0; i < advIntervalIndex + 1; i++) {
            k_sleep(K_MSEC(LED_BLINK_INTERVAL_MS));
            ledsSetState(LED_BLUE, 0);
            k_sleep(K_MSEC(LED_BLINK_INTERVAL_MS));
            ledsSetState(LED_BLUE, 1);
        }
        ledsSetState(LED_BLUE, 0);
    } else {
        isAdvRunning = !isAdvRunning;
        if (isAdvRunning) {
            LOG_INF("Adv started");
            btAdvStart();
        } else {
            LOG_INF("Adv stopped");
            btAdvStop();
        }
    }
}

static void setTxPower(uint8_t handleType, uint16_t handle, int8_t txPwrLvl)
{
    struct bt_hci_cp_vs_write_tx_power_level *cp;
    struct bt_hci_rp_vs_write_tx_power_level *rp;
    struct net_buf *buf, *rsp = NULL;
    int err;

    buf = bt_hci_cmd_create(BT_HCI_OP_VS_WRITE_TX_POWER_LEVEL, sizeof(*cp));
    __ASSERT(buf, "Unable to allocate command buffer");

    cp = net_buf_add(buf, sizeof(*cp));
    cp->handle = sys_cpu_to_le16(handle);
    cp->handle_type = handleType;
    cp->tx_power_level = txPwrLvl;

    err = bt_hci_cmd_send_sync(BT_HCI_OP_VS_WRITE_TX_POWER_LEVEL,
                     buf, &rsp);
    if (err) {
        uint8_t reason = rsp ?
            ((struct bt_hci_rp_vs_write_tx_power_level *)
                rsp->data)->status : 0;
        LOG_ERR("Set Tx power err: %d reason 0x%02x", err, reason);
        return;
    }

    rp = (void *)rsp->data;
    LOG_INF("Set Tx Power: %d", rp->selected_tx_power);

    net_buf_unref(rsp);
}

static void connected(struct bt_conn *conn, uint8_t err)
{
    char addr[BT_ADDR_LE_STR_LEN];

    if (err) {
        LOG_ERR("Connection failed (err %u)", err);
        return;
    }
    current_conn = bt_conn_ref(conn);;
    nus_max_send_len = 20;

    bt_addr_le_to_str(bt_conn_get_dst(conn), addr, sizeof(addr));
    LOG_INF("Connected %s", log_strdup(addr));
}

static void disconnected(struct bt_conn *conn, uint8_t reason)
{
    char addr[BT_ADDR_LE_STR_LEN];

    bt_addr_le_to_str(bt_conn_get_dst(conn), addr, sizeof(addr));

    LOG_INF("Disconnected: %s (reason %u)", log_strdup(addr), reason);

    if (current_conn) {
        bt_conn_unref(current_conn);
    }
}

static void nus_send_data(char* data)
{
    int err = bt_nus_send(current_conn, (uint8_t*)data, strlen(data));
    if (err) {
        LOG_WRN("Failed to send data over BLE connection, err: %d", err);
    }
}

static void bt_receive_cb(struct bt_conn *conn, const uint8_t *const data, uint16_t len)
{
    char addr[BT_ADDR_LE_STR_LEN] = {0};

    bt_addr_le_to_str(bt_conn_get_dst(conn), addr, ARRAY_SIZE(addr));

    LOG_INF("Received data from: %s", log_strdup(addr));

    productionHandleCommand(data, len, nus_send_data);
}
