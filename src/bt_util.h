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

#ifndef _U_UTIL_H_
#define _U_UTIL_H_

#include <stdbool.h>
#include <stdint.h>
#include <bluetooth/addr.h>

#define MAC_ADDR_LEN 6

/*
* Get local BT MAC address. Returns customer address if set in UICR, otherwise NRF address.
*/
void utilGetBtAddr(bt_addr_le_t *addr);

/**
 * @brief Change all lower case letters in string to upper.
 */
void utilToupper(char *str);
#endif