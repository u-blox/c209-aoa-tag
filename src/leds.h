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

#ifndef __LEDS_H
#define __LEDS_H

/**
 * @brief LED specifier
 */
typedef enum leds_t {
    LED_RED = 0,
    LED_GREEN,
    LED_BLUE,
    LED_END
} leds_t;


/**
 * @brief   Init RGB LEDs control
 */
void ledsInit(void);

/**
 * @brief   Set state of a specific LED
 * @details Sets the specified LED either on or off.
 *
 * @param   led           The LED to change state of
 * @param   on            If led shall be set to on or off.
 */
void ledsSetState(leds_t led, uint8_t on);

/**
 * @brief   Toggle a specific LED
 * @details Sets the specified LED to on of it was off or off if it was on.
 *
 * @param   led          The LED to toggle
 */
void ledsToggle(leds_t led);

#endif