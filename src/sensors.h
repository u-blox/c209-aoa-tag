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

#ifndef __SENSORS_H
#define __SENSORS_H
#include <inttypes.h>
#include <drivers/sensor.h>

/**
 * @brief   Init the sensors.
 *
 * @return  0, if init was ok.
 * @return  negative error code, if init failed.
 */
int sensorsInit(void);

/**
 * @brief   Get temp, pressure and humidity
 * @details Turns on BME280, samples and then shuts it off again.
 *
 * @param   temp           [out] temperature stored in sensor_value, val1 is integer part and val2 is decimal part.
 * @param   press          [out] pressure stored in sensor_value, val1 is integer part and val2 is decimal part.
 * @param   humidity       [out] humidity stored in sensor_value, val1 is integer part and val2 is decimal part.
 *
 * @return  true if successful, else false.
 */
bool sensorsGetBme280Data(struct sensor_value *temp, struct sensor_value *press,
                          struct sensor_value *humidity);

/**
 * @brief   Get the acceleration on x, y, z axes.
 * @details Turns on BME280, samples and then shuts it off again.
 *
 * @param   x           [out] acceleration on x axis.
 * @param   y           [out] acceleration on y axis.
 * @param   z           [out] acceleration on z axis.
 *
 * @return  true if successful, else false.
 */
bool sensorsGetLis2dw12(int16_t *x, int16_t *y, int16_t *z);

/**
 * @brief   Detect if APDS9306 is connected.
 * @details Rading from APDS9306 is not implemented. This function just checks if it's alive.
 *
 * @return  true if successful, else false.
 */
bool sensorsDetectApds(void);

#endif