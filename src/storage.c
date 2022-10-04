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

#include "storage.h"
#include <device.h>
#include <drivers/flash.h>
#include <storage/flash_map.h>
#include <fs/nvs.h>
#include <logging/log.h>

LOG_MODULE_REGISTER(storage, CONFIG_APPLICATION_MODULE_LOG_LEVEL);

#define TX_POWER_NVS_ID     1

#define DEFAULT_TX_POWER    ((int8_t)4)

static struct nvs_fs fs;

int storageInit(void)
{
    struct flash_pages_info info;
    int rc = 0;
    /* define the nvs file system by settings with:
     *  sector_size equal to the pagesize,
     *  starting at FLASH_AREA_OFFSET(storage)
     */
    fs.offset = FLASH_AREA_OFFSET(storage);
    fs.flash_device = FLASH_AREA_DEVICE(storage);
    if (!device_is_ready(fs.flash_device)) {
        LOG_ERR("Flash device %s is not ready\n", fs.flash_device->name);
        return -1;
    }

    rc = flash_get_page_info_by_offs(
             fs.flash_device,
             fs.offset, &info);
    if (rc) {
        LOG_ERR("Unable to get page info");
        return -1;
    }
    fs.sector_size = info.size;
    fs.sector_count = FLASH_AREA_SIZE(storage) / info.size;

    rc = nvs_mount(&fs);
    if (rc) {
        LOG_ERR("Flash Init failed, trying to erase nvs flash sectors");
        if (flash_erase(fs.flash_device, fs.offset, fs.sector_size * fs.sector_count) == 0) {
            rc = nvs_mount(&fs);
        } else {
            LOG_ERR("Flash erase failed after fail of init nvs");
        }
    }
    LOG_INF("NVS Init done");
    return rc;
}

void storageWriteTxPower(int8_t power)
{
    int ret;
    ret = nvs_write(&fs, TX_POWER_NVS_ID, &power, sizeof(int8_t));
    __ASSERT(ret == sizeof(int8_t) ||
             ret == 0, "nvs_write failed for ID: %d err: %d", TX_POWER_NVS_ID, ret);
}

void storageGetTxPower(int8_t *pPower)
{
    uint32_t nBytes;

    nBytes = nvs_read(&fs, TX_POWER_NVS_ID, pPower, sizeof(int8_t));
    if (nBytes != sizeof(int8_t)) {
        *pPower = DEFAULT_TX_POWER;
    }
}
