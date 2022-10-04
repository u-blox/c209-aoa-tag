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

#include "bt_util.h"
#include <zephyr.h>
#include <string.h>
#include <ctype.h>

#define EMPTY_REGISTER  0xFFFFFFFF

void utilGetBtAddr(bt_addr_le_t *addr)
{
    if (NRF_UICR->CUSTOMER[0] != EMPTY_REGISTER || NRF_UICR->CUSTOMER[1] != EMPTY_REGISTER) {
        addr->a.val[0] = (uint8_t)(NRF_UICR->CUSTOMER[1] >> 8);
        addr->a.val[1] = (uint8_t)NRF_UICR->CUSTOMER[1];
        addr->a.val[2] = (uint8_t)(NRF_UICR->CUSTOMER[0] >> 24);
        addr->a.val[3] = (uint8_t)(NRF_UICR->CUSTOMER[0] >> 16);
        addr->a.val[4] = (uint8_t)(NRF_UICR->CUSTOMER[0] >> 8);
        addr->a.val[5] = (uint8_t)NRF_UICR->CUSTOMER[0];

        addr->type = BT_ADDR_LE_PUBLIC;
    } else {
        *((uint8_t *)addr->a.val + 0) = (uint8_t)((NRF_FICR->DEVICEADDR[1] >> 8) | 0xC0);
        *((uint8_t *)addr->a.val + 1) = (uint8_t)NRF_FICR->DEVICEADDR[1];
        *((uint8_t *)addr->a.val + 2) = (uint8_t)(NRF_FICR->DEVICEADDR[0] >> 24);
        *((uint8_t *)addr->a.val + 3) = (uint8_t)(NRF_FICR->DEVICEADDR[0] >> 16);
        *((uint8_t *)addr->a.val + 4) = (uint8_t)(NRF_FICR->DEVICEADDR[0] >> 8);
        *((uint8_t *)addr->a.val + 5) = (uint8_t)(NRF_FICR->DEVICEADDR[0]);
        addr->type = BT_ADDR_LE_RANDOM;
    }
}

void utilToupper(char *str)
{
    int len = strlen(str);

    for (int i = 0; i < len; i++) {
        str[i] = toupper(str[i]);
    }
}
