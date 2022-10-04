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

#include "buttons.h"

#include <zephyr.h>
#include <device.h>
#include <sys/__assert.h>
#include <assert.h>
#include <drivers/gpio.h>
#include <logging/log.h>

LOG_MODULE_REGISTER(buttons, CONFIG_APPLICATION_MODULE_LOG_LEVEL);

#define STACKSIZE               1024
#define PRIORITY                7

#define BTN_LONG_PRESS_LIMIT  1000

static void buttonPressedIsr(const struct device *dev, struct gpio_callback *cb, uint32_t pins);
static void handleButtonThread(void);

static buttonHandlerCallback_t callback;
static struct gpio_callback buttonCallbackData;
static const struct gpio_dt_spec button = GPIO_DT_SPEC_GET_OR(DT_ALIAS(sw1), gpios, {
                                                                  0
                                                              });

static struct k_sem btnSem;
static int buttonCount = 0;

K_THREAD_DEFINE(buttonThreadId, STACKSIZE, handleButtonThread, NULL, NULL, NULL, PRIORITY, 0,
                K_TICKS_FOREVER);


void buttonsInit(buttonHandlerCallback_t handler)
{
    callback = handler;

    if (!device_is_ready(button.port)) {
        LOG_ERR("Error: didn't find button device");
        return;
    }

    int ret = gpio_pin_configure_dt(&button, GPIO_INPUT | GPIO_PULL_UP);
    if (ret != 0) {
        LOG_ERR("Error %d: failed to configure %s pin %d", ret, button.port->name, button.pin);
        return;
    }

    k_sem_init(&btnSem, 0, 1);
    k_thread_start(buttonThreadId);
    gpio_init_callback(&buttonCallbackData, buttonPressedIsr, BIT(button.pin));
    gpio_add_callback(button.port, &buttonCallbackData);
    gpio_pin_interrupt_configure_dt(&button, GPIO_INT_EDGE_TO_ACTIVE);
    LOG_INF("Set up button at %s pin %d", button.port->name, button.pin);
}

static void buttonPressedIsr(const struct device *dev, struct gpio_callback *cb, uint32_t pins)
{
    uint32_t val;
    gpio_remove_callback(button.port, &buttonCallbackData);
    val = gpio_pin_get_dt(&button);
    k_sem_give(&btnSem);
}

static void handleButtonThread(void)
{
    int ret;
    uint32_t val;
    int64_t btn_press_start_ms;
    int64_t btn_pressed_ms;
    buttonPressType_t press_type;

    while (true) {
        ret = k_sem_take(&btnSem, K_FOREVER);
        assert(ret == 0);
        btn_press_start_ms = k_uptime_get_32();
        k_sleep(K_MSEC(100)); // Let debounce stabilize
        val = gpio_pin_get_dt(&button);

        while (val) {
            val = gpio_pin_get_dt(&button);
            k_sleep(K_MSEC(10));
        }

        buttonCount++;

        btn_pressed_ms = k_uptime_delta(&btn_press_start_ms);

        if (btn_pressed_ms < BTN_LONG_PRESS_LIMIT) {
            press_type = BUTTONS_SHORT_PRESS;
        } else {
            press_type = BUTTONS_LONG_PRESS;
        }
        callback(press_type);
        gpio_add_callback(button.port, &buttonCallbackData);
    }
}