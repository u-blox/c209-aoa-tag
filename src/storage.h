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

#ifndef __STORAGE_H
#define __STORAGE_H
#include <inttypes.h>

/**
 * @brief   Init the NVS storage backend.
 *
 * @return  0, if init was ok.
 * @return  negative error code, if init failed.
 */
int storageInit(void);

/**
 * @brief   Write TX power to nvs storage
 *
 * @param   power            Value to write
 */
void storageWriteTxPower(int8_t power);

/**
 * @brief   Read TX power from nvs storage
 *
 * @param   power            pointer to store value in.
 */
void storageGetTxPower(int8_t *pPower);

#endif