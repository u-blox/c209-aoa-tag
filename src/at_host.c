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
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/__assert.h>
#include <assert.h>
#include <drivers/uart.h>
#include <device.h>
#include <drivers/sensor.h>
#include <pm/pm.h>
#include <pm/device.h>
#include <sys/reboot.h>
#include "bt_util.h"
#include "storage.h"
#include "version_config.h"
#include <logging/log.h>
#include "bt_adv.h"
#include "at_host.h"
#include "sensors.h"

LOG_MODULE_REGISTER(at_host, CONFIG_APPLICATION_MODULE_LOG_LEVEL);

#define UART_RX_BUF_NUM 2
#define UART_RX_LEN     256
#define UART_RX_TIMEOUT 1
#define AT_MAX_CMD_LEN  100
#define OK_STR          "\r\nOK\r\n"
#define ERROR_STR       "\r\nERROR\r\n"

static void resetUartAtBuffer(void);
static void sendString(char *str);

static bool testLis2dw(void);
static bool testBme280(void);
static bool testApds(void);

static void uartCallback(const struct device *dev, struct uart_event *evt, void *user_data);
static void doCommandWork(struct k_work *work);

static void disableAtUartModeTimerCallback(struct k_timer *unused);
void disableAtUartMode(struct k_work *item);
void restartUartRxAfterError(struct k_work *item);

extern const char ubxVersionString[];

static uint8_t uartRxBuf[UART_RX_BUF_NUM][UART_RX_LEN];
static uint8_t *pNextUartBuf = uartRxBuf[1];

static uint8_t atBuf[AT_MAX_CMD_LEN];
static size_t atBufLen;
static struct k_work handleCommandWork;
static struct k_work cancelUartAtWork;
static struct k_work restartRxWork;
static int uartErr = false;

static const struct device *pUartDev;

static const struct uart_config uart_cfg = {
    .baudrate = 115200,
    .parity = UART_CFG_PARITY_NONE,
    .stop_bits = UART_CFG_STOP_BITS_1,
    .data_bits = UART_CFG_DATA_BITS_8,
    .flow_ctrl = UART_CFG_FLOW_CTRL_NONE
};

K_TIMER_DEFINE(disableAtUartModeTimer, disableAtUartModeTimerCallback, NULL);

int atHostStart(void)
{
    int err;
    uint32_t start_time;

    pUartDev = DEVICE_DT_GET_OR_NULL(DT_NODELABEL(uart0));

    if (!device_is_ready(pUartDev)) {
        LOG_ERR("Cannot get UART device");
        return -EFAULT;
    }

    /* Wait for the UART line to become valid */
    start_time = k_uptime_get_32();
    do {
        err = uart_err_check(pUartDev);
        if (err) {
            if (k_uptime_get_32() - start_time > 1000) {
                LOG_ERR("UART check failed: %d. "
                        "UART initialization timed out.", err);
                return -EIO;
            }
        }
    } while (err);

    err = uart_callback_set(pUartDev, &uartCallback, NULL);
    if (err) {
        LOG_ERR("Cannot set callback: %d", err);
        return -EFAULT;
    }

    pm_device_action_run(pUartDev, PM_DEVICE_ACTION_RESUME);

    err = uart_rx_enable(pUartDev, uartRxBuf[0], sizeof(uartRxBuf[0]), UART_RX_TIMEOUT);
    if (err) {
        LOG_ERR("Cannot enable rx: %d", err);
        return -EFAULT;
    }

    resetUartAtBuffer();

    err = uart_configure(pUartDev, &uart_cfg);

    if (err == 0) {
        k_work_init(&handleCommandWork, doCommandWork);
        k_work_init(&cancelUartAtWork, disableAtUartMode);
        k_work_init(&restartRxWork, restartUartRxAfterError);
        k_timer_start(&disableAtUartModeTimer, K_SECONDS(10), K_NO_WAIT);
    } else {
        LOG_ERR("uart_configure failed: %d", err);
        return -EFAULT;
    }


    return err;
}

void disableAtUartMode(struct k_work *item)
{
    int err;
    LOG_DBG("Exit AT over UART mode, disabling UART\n");
    err = uart_rx_disable(pUartDev);
    if (err) {
        LOG_ERR("disableAtUartMode failed to stop rx, err: %d. Trying to disabe anyway.", err);
    }
    k_sleep(K_MSEC(100));
    err = pm_device_action_run(pUartDev, PM_DEVICE_ACTION_SUSPEND);
    if (err) {
        LOG_ERR("Can't power off uart: %d", err);
    }
}

static void disableAtUartModeTimerCallback(struct k_timer *unused)
{
    // Cannot disable inside of an ISR
    k_work_submit(&cancelUartAtWork);
}

void restartUartRxAfterError(struct k_work *item)
{
    LOG_INF("restartUartRxAfterError");
    int err = 1;
    err = uart_rx_enable(pUartDev, uartRxBuf[0], sizeof(uartRxBuf[0]), UART_RX_TIMEOUT);
    if (err) {
        LOG_ERR("UART RX failed: %d", err);
    }
    resetUartAtBuffer();
}

static void uartRxHandler(uint8_t character)
{
    if (character == '\r' || atBufLen > AT_MAX_CMD_LEN) {
        uart_rx_disable(pUartDev);
        k_work_submit(&handleCommandWork);
    } else {
        atBuf[atBufLen] = character;
        atBufLen += 1;
        uart_poll_out(pUartDev, character);
    }
}

static int validTxPowers(long txPower)
{
    int error = -EINVAL;

    switch (txPower) {
        case -40:
        case -30:
        case -20:
        case -16:
        case -12:
        case -8:
        case -4:
        case 0:
        case 2:
        case 3:
        case 4:
        case 5:
        case 6:
        case 7:
        case 8:
            error = 0;
        default:
            break;
    }

    return error;
}

bool atHostHandleCommand(const uint8_t *const inAtBuf, uint32_t commandLen, atOutput outputRsp)
{
    bool validCommand = true;
    char outBuf[100];
    memset(outBuf, 0, sizeof(outBuf));

    if (strncmp("ATI9", inAtBuf, 4) == 0 && commandLen == 4) {
        sprintf(outBuf, "\r\n\"%s\",\"%s\",\"%s\"\r\n", getGitSha(), getBuildTime(), ubxVersionString);
        outputRsp(outBuf);
        outputRsp("OK\r\n");
    } else if (strncmp("AT+UMLA=1", inAtBuf, 9) == 0 && commandLen == 9) {
        bt_addr_le_t addr;
        char macHex[MAC_ADDR_LEN * 2 + 1];
        uint8_t macSwapped[MAC_ADDR_LEN + 1];
        memset(macHex, 0, sizeof(macHex));
        utilGetBtAddr(&addr);
        if (addr.type == BT_ADDR_LE_PUBLIC) {
            macSwapped[0] = addr.a.val[5];
            macSwapped[1] = addr.a.val[4];
            macSwapped[2] = addr.a.val[3];
            macSwapped[3] = addr.a.val[2];
            macSwapped[4] = addr.a.val[1];
            macSwapped[5] = addr.a.val[0];
        } else {
            memcpy(macSwapped, addr.a.val, MAC_ADDR_LEN);
        }
        bin2hex(macSwapped, MAC_ADDR_LEN, macHex, MAC_ADDR_LEN * 2 + 1);
        utilToupper(macHex);
        sprintf(outBuf, "\r\n+UMLA:%s\r\n", macHex);
        outputRsp(outBuf);
        outputRsp("OK\r\n");
    } else if (strncmp("AT+TEST", inAtBuf, 7) == 0 && commandLen == 7) {
        bool ok = true;
        if (!testLis2dw()) {
            ok = false;
            outputRsp("\r\nLIS_ERROR\r\n");
        }
        if (!testBme280()) {
            ok = false;
            outputRsp("\r\nBME_ERROR\r\n");
        }
        if (!testApds()) {
            ok = false;
            outputRsp("\r\nAPDS_ERROR\r\n");
        }

        if (ok) {
            outputRsp(OK_STR);
        } else {
            outputRsp(ERROR_STR);
        }
    } else if (strncmp("AT", inAtBuf, 2) == 0 && commandLen == 2) {
        outputRsp(OK_STR);
    } else if (strncmp("AT+GMM", inAtBuf, 6) == 0 && commandLen == 6) {
        outputRsp("\r\n\"NINA-B4-TAG\"\r\n");
        outputRsp("OK\r\n");
    } else if (strncmp("AT+CPWROFF", inAtBuf, 10) == 0 && commandLen == 10) {
        outputRsp(OK_STR);
        k_sleep(K_MSEC(200));
        sys_reboot(SYS_REBOOT_COLD);
    } else if (strncmp("AT+TXPWR=", inAtBuf, 9) == 0 && commandLen > 9) {
        errno = 0;
        long power = strtol(&inAtBuf[9], NULL, 10);
        if (errno == 0 && validTxPowers(power) == 0) {
            storageWriteTxPower(power);
            outputRsp(OK_STR);
        } else {
            validCommand = false;
            outputRsp(ERROR_STR);
        }
    } else if (strncmp("AT+TXPWR?", inAtBuf, 9) == 0 && commandLen == 9) {
        int8_t pwr;
        storageGetTxPower(&pwr);
        sprintf(outBuf, "\r\n+TXPWR:%d", pwr);
        outputRsp(outBuf);
        outputRsp(OK_STR);
    }  else if (strncmp("AT+ADVENABLE=", inAtBuf, 9) == 0 && commandLen > 13) {
        errno = 0;
        long enable = strtol(&inAtBuf[13], NULL, 10);
        if (errno == 0) {
            if (enable == 0) {
                btAdvStop();
                outputRsp(OK_STR);
            } else if (enable == 1) {
                btAdvStart();
                outputRsp(OK_STR);
            } else {
                outputRsp(ERROR_STR);
                validCommand = false;
            }
        } else {
            outputRsp(ERROR_STR);
        }

    } else if (strncmp("AT+ADVINT=", inAtBuf, 9) == 0 && commandLen > 9) {
        errno = 0;
        long advInt = strtol(&inAtBuf[10], NULL, 10);
        if (errno == 0) {
            if (!btAdvUpdateAdvInterval(advInt, advInt)) {
                validCommand = false;
            }
        } else {
            validCommand = false;
        }
        if (validCommand) {
            outputRsp(OK_STR);
        } else {
            outputRsp(ERROR_STR);
        }
    } else {
        validCommand = false;
        outputRsp(ERROR_STR);
    }

    return validCommand;
}

static void doCommandWork(struct k_work *work)
{
    int err;
    int commandLen;
    bool validCommand = true;

    atBuf[MIN(atBufLen, AT_MAX_CMD_LEN - 1)] = 0;
    commandLen = strlen(atBuf);

    validCommand = atHostHandleCommand(atBuf, commandLen, sendString);

    if (validCommand) {
        k_timer_stop(&disableAtUartModeTimer);
    }

    resetUartAtBuffer();

    err = 1;
    while (err) {
        err = uart_rx_enable(pUartDev, uartRxBuf[0], sizeof(uartRxBuf[0]), UART_RX_TIMEOUT);
        if (err) {
            LOG_ERR("UART RX failed: %d", err);
        }
        k_sleep(K_MSEC(5));
    }
}

static void uartCallback(const struct device *dev, struct uart_event *evt, void *user_data)
{
    int err;
    static uint16_t pos;

    ARG_UNUSED(user_data);

    switch (evt->type) {
        case UART_TX_DONE:
            break;
        case UART_TX_ABORTED:
            break;
        case UART_RX_RDY:
            for (int i = pos; i < (pos + evt->data.rx.len); i++) {
                uartRxHandler(evt->data.rx.buf[i]);
            }
            pos += evt->data.rx.len;
            break;
        case UART_RX_BUF_REQUEST:
            pos = 0;
            err = uart_rx_buf_rsp(pUartDev, pNextUartBuf, sizeof(uartRxBuf[0]));
            if (err) {
                LOG_ERR("UART RX buf rsp: %d", err);
            }
            break;
        case UART_RX_BUF_RELEASED:
            pNextUartBuf = evt->data.rx_buf.buf;
            break;
        case UART_RX_STOPPED:
            uartErr = evt->data.rx_stop.reason;
            break;
        case UART_RX_DISABLED:
            if (uartErr != 0) {
                uartErr = 0;
                k_work_submit(&restartRxWork);
            }
            break;
        default:
            break;
    }
}

static void resetUartAtBuffer(void)
{
    memset(atBuf, 0, sizeof(atBuf));
    atBufLen = 0;
}

static void sendString(char *str)
{
    for (int i = 0; i < strlen(str); i++) {
        uart_poll_out(pUartDev, str[i]);
    }
}

static bool testLis2dw(void)
{
    int16_t x;
    int16_t y;
    int16_t z;

    return sensorsGetLis2dw12(&x, &y, &z);
}

static bool testBme280(void)
{
    struct sensor_value temp, press, humidity;
    return sensorsGetBme280Data(&temp, &press, &humidity);
}

static bool testApds(void)
{
    return sensorsDetectApds();
}
