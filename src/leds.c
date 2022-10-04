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


#define NUM_LEDS            3

#define RED_NODE DT_ALIAS(led0)
#define GREEN_NODE DT_ALIAS(led1)
#define BLUE_NODE DT_ALIAS(led2)

#define RED_LED_PIN         DT_GPIO_PIN_BY_IDX(RED_NODE, gpios, 0)
#define RED_LED_PIN_PORT    DT_GPIO_LABEL_BY_IDX(RED_NODE, gpios, 0)

#define GREEN_LED_PIN       DT_GPIO_PIN_BY_IDX(GREEN_NODE, gpios, 0)
#define GREEN_LED_PIN_PORT  DT_GPIO_LABEL_BY_IDX(GREEN_NODE, gpios, 0)

#define BLUE_LED_PIN        DT_GPIO_PIN_BY_IDX(BLUE_NODE, gpios, 0)
#define BLUE_LED_PIN_PORT   DT_GPIO_LABEL_BY_IDX(BLUE_NODE, gpios, 0)


struct ledCfg_t {
    const struct device *dev;
    uint8_t pin;
    uint8_t state;
};

struct LedsState_s {
    struct ledCfg_t leds[NUM_LEDS];
    bool initialized;
};

static struct LedsState_s state;

void ledsInit(void)
{
    const struct device *dev;
    memset(&state, 0, sizeof(state));

    dev = device_get_binding(RED_LED_PIN_PORT);
    __ASSERT_NO_MSG(dev != NULL);
    state.leds[LED_RED].dev = dev;
    state.leds[LED_RED].pin = RED_LED_PIN;
    __ASSERT_NO_MSG(gpio_pin_configure(state.leds[LED_RED].dev, state.leds[LED_RED].pin,
                                       GPIO_OUTPUT | GPIO_OUTPUT_HIGH) == 0);

    dev = device_get_binding(GREEN_LED_PIN_PORT);
    __ASSERT_NO_MSG(dev != NULL);
    state.leds[LED_GREEN].dev = dev;
    state.leds[LED_GREEN].pin = GREEN_LED_PIN;
    __ASSERT_NO_MSG(gpio_pin_configure(state.leds[LED_GREEN].dev, state.leds[LED_GREEN].pin,
                                       GPIO_OUTPUT | GPIO_OUTPUT_HIGH) == 0);

    dev = device_get_binding(BLUE_LED_PIN_PORT);
    __ASSERT_NO_MSG(dev != NULL);
    state.leds[LED_BLUE].dev = dev;
    state.leds[LED_BLUE].pin = BLUE_LED_PIN;
    __ASSERT_NO_MSG(gpio_pin_configure(state.leds[LED_BLUE].dev, state.leds[LED_BLUE].pin,
                                       GPIO_OUTPUT | GPIO_OUTPUT_HIGH) == 0);

    state.initialized = true;
}

void ledsSetState(leds_t led, uint8_t on)
{
    __ASSERT_NO_MSG(state.initialized);
    __ASSERT_NO_MSG(led <= LED_LAST);
    __ASSERT_NO_MSG(on == 0 || on == 1);

    state.leds[led].state = on;

    __ASSERT_NO_MSG(gpio_pin_set(state.leds[led].dev, state.leds[led].pin, !on) == 0);
}

void ledsToggle(leds_t led)
{
    __ASSERT_NO_MSG(state.initialized);
    __ASSERT_NO_MSG(led <= LED_LAST);

    if (state.leds[led].state == 0) {
        state.leds[led].state = 1;
    } else {
        state.leds[led].state = 0;
    }

    __ASSERT_NO_MSG(gpio_pin_set(state.leds[led].dev, state.leds[led].pin, state.leds[led].state) == 0);
}