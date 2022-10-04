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

#include <device.h>
#include <sys/__assert.h>
#include <drivers/gpio.h>
#include <string.h>
#include "leds.h"


struct ledCfg_t {
    const struct gpio_dt_spec gpio;
    uint8_t state;
};

struct LedsState_s {
    struct ledCfg_t leds[LED_END];
    bool initialized;
};

static struct LedsState_s state = {
    .initialized = false,
    .leds = {
        [LED_RED] = {.gpio = GPIO_DT_SPEC_GET(DT_NODELABEL(led0), gpios), .state = 0},
        [LED_GREEN] = {.gpio = GPIO_DT_SPEC_GET(DT_NODELABEL(led1), gpios), .state = 0},
        [LED_BLUE] = {.gpio = GPIO_DT_SPEC_GET(DT_NODELABEL(led2), gpios), .state = 0}
    }
};

void ledsInit(void)
{
    __ASSERT_NO_MSG(device_is_ready(state.leds[LED_RED].gpio.port));
    __ASSERT_NO_MSG(device_is_ready(state.leds[LED_GREEN].gpio.port));
    __ASSERT_NO_MSG(device_is_ready(state.leds[LED_BLUE].gpio.port));

    for (int i = 0; i < LED_END; i++) {
        gpio_pin_configure_dt(&state.leds[i].gpio, GPIO_OUTPUT | GPIO_OUTPUT_HIGH);
    }
    state.initialized = true;
}

void ledsSetState(leds_t led, uint8_t on)
{
    __ASSERT_NO_MSG(state.initialized);
    __ASSERT_NO_MSG(led < LED_END);
    __ASSERT_NO_MSG(on == 0 || on == 1);

    state.leds[led].state = on;

    __ASSERT_NO_MSG(gpio_pin_set_dt(&state.leds[led].gpio, on) == 0);
}

void ledsToggle(leds_t led)
{
    __ASSERT_NO_MSG(state.initialized);
    __ASSERT_NO_MSG(led < LED_END);

    if (state.leds[led].state == 0) {
        state.leds[led].state = 1;
    } else {
        state.leds[led].state = 0;
    }

    __ASSERT_NO_MSG(gpio_pin_set_dt(&state.leds[led].gpio, state.leds[led].state) == 0);
}