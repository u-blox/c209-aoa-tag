/*
 * Copyright 2022 u-blox
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

#include <logging/log.h>
#include <device.h>
#include <drivers/sensor.h>
#include <drivers/i2c.h>
#include <pm/pm.h>
#include <pm/device.h>
#include <lis2dw12_reg.h>

LOG_MODULE_REGISTER(sensors, LOG_LEVEL_DBG);

#define I2C_DEV DT_NODELABEL(i2c0)

#define APDS_9306_065_ADDRESS   0x52
#define APDS_9306_065_REG_ID    0x06
#define APDS_9306_065_CHIP_ID   0xB3

static int configureLis2dw12Default(const struct device *lis2dw12Dev);

int sensorsInit(void)
{
    int err = 0;
    const struct device *bme_device = DEVICE_DT_GET_ANY(bosch_bme280);
    const struct device *lis2dw12 = DEVICE_DT_GET_ANY(st_lis2dw12);

    // Power down to save power
    if (bme_device != NULL) {
        err = pm_device_action_run(bme_device, PM_DEVICE_ACTION_SUSPEND);
        if (err != 0) {
            LOG_ERR("bosch_bme280 suspend err: %d", err);
        }
    }

    if (device_is_ready(lis2dw12)) {
        err = configureLis2dw12Default(lis2dw12);
    }

    return err;
}

bool sensorsGetBme280Data(struct sensor_value *temp, struct sensor_value *press,
                          struct sensor_value *humidity)
{
    int err;
    const struct device *sensor = DEVICE_DT_GET_ANY(bosch_bme280);

    if (sensor == NULL) {
        LOG_ERR("Error: no device found.");
        return false;
    }

    if (!device_is_ready(sensor)) {
        LOG_ERR("Error: Device \"%s\" is not ready; "
                "check the driver initialization logs for errors.",
                sensor->name);
        return false;
    }

    err = pm_device_action_run(sensor, PM_DEVICE_ACTION_RESUME);
    if (err != 0 && err != -EALREADY) {
        LOG_ERR("Error: Could not resume from suspended state BME280");
        return false;
    }

    err = sensor_sample_fetch(sensor);
    if (!err) {
        sensor_channel_get(sensor, SENSOR_CHAN_AMBIENT_TEMP, temp);
        sensor_channel_get(sensor, SENSOR_CHAN_PRESS, press);
        sensor_channel_get(sensor, SENSOR_CHAN_HUMIDITY, humidity);

        LOG_DBG("temp: %d.%06d; press: %d.%06d; humidity: %d.%06d",
                temp->val1, temp->val2, press->val1, press->val2,
                humidity->val1, humidity->val2);
    } else {
        LOG_ERR("Failed fetching sample from %s", sensor->name);
        return false;
    }

    err = pm_device_action_run(sensor, PM_DEVICE_ACTION_SUSPEND);
    if (err != 0 && err != -EALREADY) {
        LOG_ERR("Error: Could not PM_DEVICE_ACTION_SUSPEND from resumed state BME280");
        return false;
    }

    return true;
}

bool sensorsGetLis2dw12(int16_t *x, int16_t *y, int16_t *z)
{
    struct sensor_value acc_val[3];
    const struct device *sensor = DEVICE_DT_GET_ANY(st_lis2dw12);

    if (sensor == NULL) {
        LOG_ERR("Could not get %s device", DT_LABEL(DT_INST(0, st_lis2dw12)));
        return false;
    }

    if (!device_is_ready(sensor)) {
        LOG_ERR("Error: Device \"%s\" is not ready; "
                "check the driver initialization logs for errors.",
                sensor->name);
        return false;
    }

    int err = sensor_sample_fetch(sensor);
    if (err) {
        LOG_ERR("Could not fetch sample from %s", sensor->name);
        return false;
    }
    if (!err) {
        sensor_channel_get(sensor, SENSOR_CHAN_ACCEL_XYZ, acc_val);
        *x = (int16_t)(sensor_value_to_double(&acc_val[0]) * (32768 / 16));
        *y = (int16_t)(sensor_value_to_double(&acc_val[1]) * (32768 / 16));
        *z = (int16_t)(sensor_value_to_double(&acc_val[2]) * (32768 / 16));
        LOG_DBG("x: %d y: %d z: %d", *x, *y, *z);
    } else {
        LOG_ERR("Failed fetching sample from %s", sensor->name);
        return false;
    }

    return true;
}

bool sensorsDetectApds(void)
{
    uint8_t id = 0;
    const struct device *i2c_dev = DEVICE_DT_GET_OR_NULL(I2C_DEV);

    if (i2c_dev == NULL) {
        LOG_ERR("Error: no APDS device found.");
        return false;
    }

    if (!device_is_ready(i2c_dev)) {
        LOG_ERR("Error: Device \"%s\" is not ready; "
                "check the driver initialization logs for errors.",
                i2c_dev->name);
        return false;
    }

    /* Verify sensor working by reading the ID */
    int err = i2c_reg_read_byte(i2c_dev, APDS_9306_065_ADDRESS, APDS_9306_065_REG_ID, &id);
    if (err) {
        LOG_ERR("Failed reading device id from APDS");
        return false;
    }

    if (id == APDS_9306_065_CHIP_ID) {
        LOG_INF("APDS id: %d", id);
        return true;
    }

    return false;
}

static int configureLis2dw12Default(const struct device *lis2dw12Dev)
{
    int err;
    struct sensor_value val;
    stmdev_ctx_t *ctx = (stmdev_ctx_t *)lis2dw12Dev->config;

    /*
    Due to the design of the C209 HW by default there is a ~300uA current leak
    coming from the LIS_INT pin. This is due to a external pullup resistor on this pin and the fact
    that LIS2DW12 by default have an internal pulldown on the same pin.
    Fortunately LIS2DW12 have a configuration to disable the internal
    pulldown on INT1 pin and to make the INT1 pin active low instead.

    The current codebase does not use the LIS_INT/INT1 pin functionality, however if
    in future it is make sure to configure and check so that the Zephyr driver can handle
    swapped polarity of INT1/2 pins of the LIS2DW12.
    */
    err = lis2dw12_pin_polarity_set(ctx, LIS2DW12_ACTIVE_LOW);
    if (err) {
        LOG_ERR("Updating INT pin polarity");
        return err;
    }
    err = lis2dw12_pin_mode_set(ctx, LIS2DW12_OPEN_DRAIN);
    if (err) {
        LOG_ERR("Updating INT pin mode");
        return err;
    }
    val.val1 = 0; // ODR 0 will set LIS2DW12 into power down mode
    val.val2 = 0;
    err = sensor_attr_set(lis2dw12Dev, SENSOR_CHAN_ACCEL_XYZ, SENSOR_ATTR_SAMPLING_FREQUENCY, &val);
    if (err < 0) {
        LOG_ERR("lis2dw12 ODR set zero failed: %d", err);
    }

    return err;
}