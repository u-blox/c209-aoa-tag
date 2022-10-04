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

#ifndef __BT_ADV_H
#define __BT_ADV_H

#include <zephyr.h>
#include <bluetooth/bluetooth.h>

#define EDDYSTONE_INSTANCE_ID_LEN   6
#define EDDYSTONE_NAMESPACE_LENGFTH 10

/**
 * @brief   Init BT advertising
 * @details Initializes advertising, but does not start it.
 *
 * @param   min_int         Min adv. interval in milliseconds
 * @param   max_int         Max adv. interval in milliseconds
 * @param   namespace       Pointer to the namespace to be sent in Eddystone beacon.
 * @param   instance_id     Pointer to the instance ID to be sent in Eddystone beacon.
 * @param   txPower         The TX power put in advertising data
 */
void btAdvInit(uint16_t min_int, uint16_t max_int, uint8_t *namespace, uint8_t *instance_id,
               int8_t txPower);

/**
 * @brief   Start BT advertising
 * @details Start advertising, will not stop until stop function called.
 */
void btAdvStart(void);

/**
 * @brief   Stop BT advertising
 * @details Stop advertising, call btAdvStart to enable advertising again.
 */
void btAdvStop(void);

/**
 * @brief   Change the advertsing interval
 * @details Change the advertising interval, advertsing will be stopped and restarted with the new interval.
 *
 * @param   min_int         Min adv. interval in milliseconds
 * @param   max_int         Max adv. interval in milliseconds
 *
 * @return                  True if success, false otherwise.
 */
bool btAdvUpdateAdvInterval(uint16_t min, uint16_t max);

/**
 * @brief Set or update the periodic advertising data.
 *
 * Advertising must be initialized before calling.
 *
 * @param ad        Advertising data.
 * @param ad_len    Advertising data length.
 *
 */
void btAdvSetPerAdvData(struct bt_data *data, int len);

#endif