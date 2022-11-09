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
#include <bluetooth/hci.h>
#include <bluetooth/hci_vs.h>
#include <sys/byteorder.h>
#include "at_host.h"
#include "storage.h"
#include <logging/log.h>
#include "sensors.h"

#if defined(CONFIG_BT_NUS)
#include <bluetooth/services/nus.h>
#endif

LOG_MODULE_REGISTER(app, CONFIG_APPLICATION_MODULE_LOG_LEVEL);

#define BLINK_STACKSIZE         1024
#define BLINK_PRIORITY          7
#define LED_BLINK_INTERVAL_MS   150
#define LOOP_SLEEP_INTERVAL     5000

// In order to avoid accidental collisions between tags we restart adv. every now and then.
// Comment out to disable this.
#define ADV_RESTART_INTERVAL    (10 * 60 * 1000) // 10 min

static void btReadyCb(int err);
static void onButtonPressCb(buttonPressType_t type);
static void setTxPower(uint8_t handleType, uint16_t handle, int8_t txPwrLvl);
static void blink(void);

#if defined(CONFIG_BT_NUS)
static void connected(struct bt_conn *conn, uint8_t err);
static void disconnected(struct bt_conn *conn, uint8_t reason);
static void nus_send_data(char *data);
static void bt_receive_cb(struct bt_conn *conn, const uint8_t *const data, uint16_t len);
BT_CONN_CB_DEFINE(conn_callbacks) = {
    .connected    = connected,
    .disconnected = disconnected,
};
static struct bt_conn *currentConn;
static uint32_t nusMaxSendLen;

static struct bt_nus_cb nus_cb = {
    .received = bt_receive_cb,
};
#endif

static bool isAdvRunning = true;
static uint16_t advIntervals[] = {50, 100, 250, 1000};
static uint8_t advIntervalIndex = 0;
static char *pDefaultGroupNamespace = "NINA-B4TAG";

struct k_timer blinkTimer;
static uint8_t bluetoothReady;
static uint8_t uuid[EDDYSTONE_INSTANCE_ID_LEN];

K_THREAD_DEFINE(blinkThreadId, BLINK_STACKSIZE, blink, NULL, NULL, NULL, BLINK_PRIORITY, 0,
                K_TICKS_FOREVER);

void main(void)
{
    uint8_t randDelayMs;
    bt_addr_le_t addr;

    // If all tags are powered on at once their advertisements may collide.
    // Use a random delay in order to give them some random offset.
    randDelayMs = (uint8_t)(sys_rand32_get() & 0xFF);
    k_msleep(randDelayMs);
    LOG_DBG("Slept %dms", randDelayMs);
    utilGetBtAddr(&addr);
    bluetoothReady = 0;

    storageInit();
    sensorsInit();

    // Only swap public address. It's done like this in u-connect.
    if (addr.type == BT_ADDR_LE_PUBLIC) {
        for (uint8_t i = 0; i < EDDYSTONE_INSTANCE_ID_LEN; i++) {
            uuid[i] = addr.a.val[(EDDYSTONE_INSTANCE_ID_LEN - 1) - i];
        }
    } else {
        memcpy(uuid, addr.a.val, MAC_ADDR_LEN);
    }
    LOG_HEXDUMP_INF(uuid, EDDYSTONE_INSTANCE_ID_LEN, "InstanceId (MAC)");

    atHostStart();

    ledsInit();
    ledsSetState(LED_RED, 0);
    ledsSetState(LED_GREEN, 0);
    ledsSetState(LED_BLUE, 0);

    buttonsInit(&onButtonPressCb);

    __ASSERT(bt_enable(btReadyCb) == 0, "Bluetooth init failed");

#if defined(CONFIG_BT_NUS)
    int err = bt_nus_init(&nus_cb);
    if (err) {
        LOG_ERR("Failed to initialize UART service (err: %d)", err);
        return;
    }
#endif
    k_thread_start(blinkThreadId);
}

static void blink(void)
{
    LOG_INF("Started blink thread");
#ifdef ADV_RESTART_INTERVAL
    uint64_t lastAdvRestartMs = k_uptime_get();
    uint64_t currentTime;
    uint8_t randDelayMs;
#endif
#ifdef CONFIG_SEND_SENSOR_DATA_IN_PER_ADV_DATA
#define NUM_SENSOR_DATA 6
    struct sensor_value temp, press, humidity;
    struct bt_data adData;
    int32_t sensorData[NUM_SENSOR_DATA];
#endif

    while (1) {
#ifdef CONFIG_PERIODIC_LED_BLINK
        if (isAdvRunning) {
            ledsSetState(LED_BLUE, 1);
            k_sleep(K_MSEC(10));
            ledsSetState(LED_BLUE, 0);
        }
#endif
#ifdef ADV_RESTART_INTERVAL
        currentTime = k_uptime_get();
        if (currentTime - lastAdvRestartMs >= ADV_RESTART_INTERVAL) {
            if (isAdvRunning) {
                randDelayMs = (uint8_t)(sys_rand32_get() & 0xFF);
                LOG_INF("Restarting per. adv. to avoid collisions. Delay: %d", randDelayMs);
                btAdvStop();
                k_msleep(randDelayMs);
                btAdvStart();
                lastAdvRestartMs = currentTime;
            }
        }
#endif
#ifdef CONFIG_SEND_SENSOR_DATA_IN_PER_ADV_DATA
        if (isAdvRunning) {
            if (sensorsGetBme280Data(&temp, &press, &humidity)) {
                sensorData[0] = temp.val1;
                sensorData[1] = temp.val2;
                sensorData[2] = press.val1;
                sensorData[3] = press.val2;
                sensorData[4] = humidity.val1;
                sensorData[5] = humidity.val2;
                adData.type = BT_DATA_MANUFACTURER_DATA;
                adData.data = (uint8_t *)sensorData;
                adData.data_len = sizeof(int32_t) * NUM_SENSOR_DATA;
                btAdvSetPerAdvData(&adData, 1);
            }
        }
#endif
        k_msleep(LOOP_SLEEP_INTERVAL);
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

    btAdvInit(advIntervals[advIntervalIndex], advIntervals[advIntervalIndex], pDefaultGroupNamespace,
              uuid, txPower);
    btAdvStart();
}

static void onButtonPressCb(buttonPressType_t type)
{
    LOG_INF("Pressed, type: %d", type);

    if (type == BUTTONS_SHORT_PRESS) {
        // If stopped, then restart if adv. interval was changed
        isAdvRunning = true;
        advIntervalIndex = (advIntervalIndex + 1) % ARRAY_SIZE(advIntervals);
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

#if defined(CONFIG_BT_NUS)
static void connected(struct bt_conn *conn, uint8_t err)
{
    char addr[BT_ADDR_LE_STR_LEN];

    if (err) {
        LOG_ERR("Connection failed (err %u)", err);
        return;
    }
    currentConn = bt_conn_ref(conn);
    nusMaxSendLen = 20;

    bt_addr_le_to_str(bt_conn_get_dst(conn), addr, sizeof(addr));
    LOG_INF("Connected %s", addr);
}

static void disconnected(struct bt_conn *conn, uint8_t reason)
{
    char addr[BT_ADDR_LE_STR_LEN];

    bt_addr_le_to_str(bt_conn_get_dst(conn), addr, sizeof(addr));

    LOG_INF("Disconnected: %s (reason %u)", addr, reason);

    if (currentConn) {
        bt_conn_unref(currentConn);
    }
}

static void nus_send_data(char *data)
{
    int err = bt_nus_send(currentConn, (uint8_t *)data, strlen(data));
    if (err) {
        LOG_WRN("Failed to send data over BLE connection, err: %d", err);
    }
}

static void bt_receive_cb(struct bt_conn *conn, const uint8_t *const data, uint16_t len)
{
    char addr[BT_ADDR_LE_STR_LEN] = {0};

    bt_addr_le_to_str(bt_conn_get_dst(conn), addr, ARRAY_SIZE(addr));

    LOG_INF("Received data from: %s", addr);

    atHostHandleCommand(data, len, nus_send_data);
}
#endif