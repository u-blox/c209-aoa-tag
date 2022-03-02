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

#include "bt_adv.h"
#include <bluetooth/bluetooth.h>
#include <bluetooth/hci.h>
#include <sys/__assert.h>
#include <logging/log.h>

LOG_MODULE_REGISTER(bt_adv, CONFIG_APPLICATION_MODULE_LOG_LEVEL);

extern bool isAdvRunning;
static uint16_t minAdvInterval;
static uint16_t maxAdvInterval;

// Offsets of the different data in bt_data ad[] below
#define ADV_DATA_OFFSET_NAMESPACE   4
#define ADV_DATA_OFFSET_INSTANCE    14
#define ADV_DATA_OFFSET_TX_POWER    3

static struct bt_data ad[] = {
    BT_DATA_BYTES(BT_DATA_FLAGS, BT_LE_AD_NO_BREDR),
    BT_DATA_BYTES(BT_DATA_UUID16_ALL, 0xaa, 0xfe),
    BT_DATA_BYTES(BT_DATA_SVC_DATA16,
        0xaa, 0xfe, /* Eddystone UUID */
        0x00, /* Eddystone-UID frame type */
        0x00, /* TX Power */
        'N', 'I', 'N', 'A', '-', 'B', '4', 'T', 'A', 'G', /* Namespace */
        'i', 'n', 's', 't', 'a', 'e', /* Instance Id */
        0x00, /* reserved */
        0x00 /* reserved */
    )
};

void btAdvInit(uint16_t min_int, uint16_t max_int, uint8_t* namespace, uint8_t* instance_id, int8_t txPower) {
    if( min_int == max_int){
      max_int++;
    }
    minAdvInterval = min_int / 0.625;
    maxAdvInterval = max_int / 0.625;

    memcpy((uint8_t*)&ad[2].data[ADV_DATA_OFFSET_NAMESPACE], namespace, EDDYSTONE_NAMESPACE_LENGTH);
    memcpy((uint8_t*)&ad[2].data[ADV_DATA_OFFSET_INSTANCE], instance_id, EDDYSTONE_INSTANCE_ID_LEN);
    memcpy((uint8_t*)&ad[2].data[ADV_DATA_OFFSET_TX_POWER], &txPower, sizeof(txPower));
}

void btAdvStart(void) {
    int err = bt_le_adv_start(BT_LE_ADV_PARAM(BT_LE_ADV_OPT_USE_IDENTITY, minAdvInterval, maxAdvInterval, NULL), ad, ARRAY_SIZE(ad), NULL, 0);
    if (err) {
        LOG_ERR("Advertising failed to start (err %d)", err);
        return;
    }
    LOG_INF("adv started");
}

void btAdvStop(void) {
    bt_le_adv_stop();
    LOG_INF("Adv stopped");
}

void btAdvUpdateAdvInterval(uint16_t min_int, uint16_t max_int) {
    if( min_int == max_int){
      max_int++;
    }
    minAdvInterval = min_int / 0.625;
    maxAdvInterval = max_int / 0.625;
    if(isAdvRunning){
      int err = bt_le_adv_stop();
      if (err) {
          LOG_ERR("Failed stopping adv when changing adv interval: %d", err);
      }
      btAdvStart();
    }
}
