/*
 * Copyright 2021 u-blox Ltd
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

#define EDDYSTONE_INSTANCE_ID_LEN   6
#define EDDYSTONE_NAMESPACE_LENGFTH 10

void btAdvInit(uint16_t min_int, uint16_t max_int, uint8_t* namespace, uint8_t* instance_id, int8_t txPower);
void btAdvStart(void);
void btAdvStop(void);
void btAdvUpdateAdvInterval(uint16_t min, uint16_t max);

#endif