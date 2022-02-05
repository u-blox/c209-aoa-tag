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
#define NAMESPACE_NVS_ID     2
#define INSTANCEID_NVS_ID     4

#define DEFAULT_TX_POWER    ((int8_t)4)
#define DEFAULT_NAMESPACE  "NINA-B4TAG"
#define DEFAULT_INSTANCEID  "instae"

static struct nvs_fs fs;

int storageInit(void)
{
    struct flash_pages_info info;
    int rc = 0;
    /* define the nvs file system by settings with:
     *	sector_size equal to the pagesize,
     *	starting at FLASH_AREA_OFFSET(storage)
     */
    fs.offset = FLASH_AREA_OFFSET(storage);
    rc = flash_get_page_info_by_offs(
            device_get_binding(DT_CHOSEN_ZEPHYR_FLASH_CONTROLLER_LABEL),
            fs.offset, &info);
    if (rc) {
        LOG_ERR("Unable to get page info");
        return -1;
    }
    fs.sector_size = info.size;
    fs.sector_count = FLASH_AREA_SIZE(storage) / info.size;

    rc = nvs_init(&fs, DT_CHOSEN_ZEPHYR_FLASH_CONTROLLER_LABEL);
    if (rc) {
        LOG_ERR("Flash Init failed, trying to erase nvs flash sectors");
        if (flash_erase(fs.flash_device, fs.offset, fs.sector_size * fs.sector_count) == 0) {
            rc = nvs_init(&fs, DT_CHOSEN_ZEPHYR_FLASH_CONTROLLER_LABEL);
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
    __ASSERT(ret == sizeof(int8_t) || ret == 0, "nvs_write failed for ID: %d err: %d", TX_POWER_NVS_ID, ret);
}

void storageGetTxPower(int8_t* pPower)
{
    uint32_t nBytes;

    nBytes = nvs_read(&fs, TX_POWER_NVS_ID, pPower, sizeof(int8_t));
    if (nBytes != sizeof(int8_t)) {
        *pPower = DEFAULT_TX_POWER;
    }
}

void storageWriteNameSpace(uint8_t* namespace, int8_t length)
{
    int ret;
    ret = nvs_write(&fs, NAMESPACE_NVS_ID,namespace, length);
    __ASSERT(ret == length || ret == 0, "nvs_write failed for ID: %d err: %d", NAMESPACE_NVS_ID, ret);
}

/**
 * @brief   Read namespace from nvs storage
 *
 * @param   power            pointer to store value in.
 */
void storageGetNameSpace(uint8_t* pNamespace, int8_t length)
{
    uint32_t nBytes;

    nBytes = nvs_read(&fs, NAMESPACE_NVS_ID,pNamespace, length);
    if (nBytes != length) {
      memcpy(pNamespace,DEFAULT_NAMESPACE,length);
    }
}
/**
 * @brief   Write instance id to nvs storage
 *
 * @param   power            Value to write
 */
void storageWriteInstanceID(uint8_t* id, int8_t length)
{
    int ret;
    ret = nvs_write(&fs,INSTANCEID_NVS_ID,id, length);
    __ASSERT(ret == length || ret == 0, "nvs_write failed for ID: %d err: %d", INSTANCEID_NVS_ID, ret);
}

/**
 * @brief   Read instance id from nvs storage
 *
 * @param   power            pointer to store value in.
 */
void storageGetInstanceID(uint8_t* pID, int8_t length)
{
    uint32_t nBytes;

    nBytes = nvs_read(&fs, INSTANCEID_NVS_ID,pID, length);
    if (nBytes != length) {
      memcpy(pID,DEFAULT_INSTANCEID,length);
    }
}