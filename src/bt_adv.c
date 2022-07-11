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

#include <bluetooth/bluetooth.h>
#include <bluetooth/hci.h>
#include <bluetooth/direction.h>
#include <sys/byteorder.h>
#include <sys/util.h>
#include <random/rand32.h>

#include <bluetooth/services/nus.h>

LOG_MODULE_REGISTER(bt_adv_aoa, CONFIG_APPLICATION_MODULE_LOG_LEVEL);

/* Length of CTE in unit of 8[us] */
#define CTE_LEN (0x14U)

/* Number of CTE send in single periodic advertising train */
#define PER_ADV_EVENT_CTE_COUNT 1

#define PER_ADV_DATA_LEN 10

static void adv_sent_cb(struct bt_le_ext_adv *adv, struct bt_le_ext_adv_sent_info *info);

static struct bt_le_ext_adv_cb adv_callbacks = {
    .sent = adv_sent_cb,
};

static struct bt_le_ext_adv *adv_set;

static struct bt_le_adv_param param =
        // Below intervals are a tradeoff between power consumption and the time
        // it takes for the scanner to start tracking this tag. Set it accordingly.
        BT_LE_ADV_PARAM_INIT(BT_LE_ADV_OPT_EXT_ADV |
                     BT_LE_ADV_OPT_USE_NAME | BT_LE_ADV_OPT_NO_2M,
                     BT_GAP_ADV_FAST_INT_MIN_2 * 2, // 200ms
                     BT_GAP_ADV_FAST_INT_MAX_2 * 2, // 300ms
                     NULL);

static struct bt_le_adv_param param_nus =
        BT_LE_ADV_PARAM_INIT(BT_LE_ADV_OPT_USE_NAME | BT_LE_ADV_OPT_CONNECTABLE,
                     BT_GAP_ADV_SLOW_INT_MIN,
                     BT_GAP_ADV_SLOW_INT_MAX,
                     NULL);

static struct bt_le_ext_adv_start_param ext_adv_start_param = {
    .timeout = 0,
    .num_events = 0,
};

struct bt_df_adv_cte_tx_param cte_params = { .cte_len = CTE_LEN,
                         .cte_count = PER_ADV_EVENT_CTE_COUNT,
                         .cte_type = BT_DF_CTE_TYPE_AOA,
                         .num_ant_ids = 0,
                         .ant_ids = NULL
};

static void adv_sent_cb(struct bt_le_ext_adv *adv,
            struct bt_le_ext_adv_sent_info *info)
{
    LOG_INF("Advertiser[%d] %p sent %d\n", bt_le_ext_adv_get_index(adv),
           (void*)adv, info->num_sent);
}

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

static uint8_t per_ad_data[PER_ADV_DATA_LEN];
static struct bt_data per_ad[] = {
    BT_DATA(BT_DATA_MANUFACTURER_DATA, per_ad_data, sizeof(per_ad_data))
};

static const struct bt_data ad_nus[] = {
    BT_DATA_BYTES(BT_DATA_FLAGS, (BT_LE_AD_GENERAL | BT_LE_AD_NO_BREDR)),
    BT_DATA_BYTES(BT_DATA_UUID128_ALL, BT_UUID_NUS_VAL),
};

void btAdvInit(uint16_t min_int, uint16_t max_int, uint8_t* namespace, uint8_t* instance_id, int8_t txPower) {
    minAdvInterval = min_int / 1.25;
    maxAdvInterval = max_int / 1.25;

    memcpy((uint8_t*)&ad[2].data[ADV_DATA_OFFSET_NAMESPACE], namespace, EDDYSTONE_NAMESPACE_LENGFTH);
    memcpy((uint8_t*)&ad[2].data[ADV_DATA_OFFSET_INSTANCE], instance_id, EDDYSTONE_INSTANCE_ID_LEN);
    memcpy((uint8_t*)&ad[2].data[ADV_DATA_OFFSET_TX_POWER], &txPower, sizeof(txPower));
    memset(per_ad_data, 0, sizeof(per_ad_data));

    // Just send some dummy data for now, this can be any customer data
    // later and may also include som u-blox reserved parts for sensors etc.
    for (int i = 0; i < sizeof(per_ad_data); i++) {
        per_ad_data[i] = 'A' + (i % ('Z' - 'A'));
    }

    int err = bt_le_ext_adv_create(&param, &adv_callbacks, &adv_set);
    if (err) {
        LOG_ERR("failed (err %d)\n", err);
        return;
    }
    LOG_INF("success\n");

    LOG_INF("Set ext adv data...");
    err = bt_le_ext_adv_set_data(adv_set, ad, ARRAY_SIZE(ad), NULL, 0);
    if (err) {
        LOG_ERR("Failed setting ext adv data: %d\n", err);
    }
    LOG_INF("success\n");

    LOG_INF("Update CTE params...");
    err = bt_df_set_adv_cte_tx_param(adv_set, &cte_params);
    if (err) {
        LOG_ERR("failed (err %d)\n", err);
        return;
    }
    LOG_INF("success\n");

    LOG_INF("Periodic advertising params set...");
    struct bt_le_per_adv_param per_adv_param = {
        .interval_min = minAdvInterval,
        .interval_max = maxAdvInterval,
        .options = BT_LE_ADV_OPT_USE_TX_POWER | BT_LE_ADV_OPT_NO_2M,
    };
    err = bt_le_per_adv_set_param(adv_set, &per_adv_param);
    if (err) {
        LOG_ERR("failed (err %d)\n", err);
        return;
    }
    LOG_INF("success\n");

    LOG_INF("Set per adv data...");
    err = bt_le_per_adv_set_data(adv_set, per_ad, ARRAY_SIZE(per_ad));
    if (err) {
        LOG_ERR("failed (err %d)\n", err);
        return;
    }
    LOG_INF("success\n");

    LOG_INF("Enable CTE...");
    err = bt_df_adv_cte_tx_enable(adv_set);
    if (err) {
        LOG_ERR("failed (err %d)\n", err);
        return;
    }
    LOG_INF("success\n");

    LOG_INF("Legacy advertising NUS enable...");
	err = bt_le_adv_start(&param_nus, ad_nus, ARRAY_SIZE(ad_nus),
			      NULL, 0);
	if (err) {
		printk("Advertising failed to start (err %d)\n", err);
		return;
	}
    LOG_INF("success\n");
}

void btAdvStart(void) {
    LOG_INF("Periodic advertising enable...");
    int err = bt_le_per_adv_start(adv_set);
    if (err) {
        LOG_ERR("failed (err %d)\n", err);
        return;
    }
    LOG_INF("success\n");

    LOG_INF("Extended advertising enable...");
    err = bt_le_ext_adv_start(adv_set, &ext_adv_start_param);
    if (err) {
        LOG_ERR("failed (err %d)\n", err);
        return;
    }
    LOG_INF("success\n");
}

void btAdvStop(void) {
    bt_le_per_adv_stop(adv_set);
    bt_le_ext_adv_stop(adv_set);
    LOG_INF("Adv stopped");
}

void btAdvUpdateAdvInterval(uint16_t min, uint16_t max) {
    minAdvInterval = min / 1.25;
    maxAdvInterval = max / 1.25;
    btAdvStop();
    struct bt_le_per_adv_param per_adv_param = {
        .interval_min = minAdvInterval,
        .interval_max = maxAdvInterval,
        .options = BT_LE_ADV_OPT_USE_TX_POWER | BT_LE_ADV_OPT_NO_2M,
    };
    int err = bt_le_per_adv_set_param(adv_set, &per_adv_param);
    if (err) {
        LOG_ERR("failed (err %d)\n", err);
        return;
    }
    LOG_INF("success\n");
    btAdvStart();
}
