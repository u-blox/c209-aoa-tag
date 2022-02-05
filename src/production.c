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
#include "bt_adv.h"
#include "leds.h"

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
#include <drivers/i2c.h>
#include <power/power.h>
#include <power/reboot.h>
#include "bt_util.h"
#include "storage.h"
#include "version_config.h"
#include <logging/log.h>


LOG_MODULE_REGISTER(production, CONFIG_APPLICATION_MODULE_LOG_LEVEL);

#define NUM_ADV_INTERVALS       3
#define THREAD_STACKSIZE       1024
#define THREAD_PRIORITY        7

#define I2C_DEV DT_LABEL(DT_NODELABEL(i2c0))
#define APDS_9306_065_ADDRESS   0x52
#define APDS_9306_065_REG_ID    0x06
#define APDS_9306_065_CHIP_ID   0xB3

#define UART_RX_BUF_NUM	2
#define UART_RX_LEN	    256
#define UART_RX_TIMEOUT 1
#define AT_MAX_CMD_LEN  100
#define OK_STR          "\r\nOK\r\n"
#define ERROR_STR       "\r\nERROR\r\n"

#define GET_MAC_COMMAND "AT+UMLA=1"
#define GET_MAC_COMMAND_LENGTH 9
#define GET_NAMESPACE_COMMAND "AT+GMM"
#define GET_NAMESPACE_COMMAND_LENGTH 6
#define ID_COMMAND "AT+ID="
#define ID_COMMAND_LENGTH 6
#define ADV_COMMAND "AT+ADV="
#define ADV_COMMAND_LENGTH 7
#define MIN_LED_ADV_IND_MS      20

K_THREAD_STACK_DEFINE(threadStack, THREAD_STACKSIZE);

static void resetParser(void);
static void sendString(char* str);

static bool testLis2dw(void);
static bool testBme280(void);
static bool testApds(void);

static void uartCallback(const struct device *dev, struct uart_event *evt, void *user_data);
static void handleCommand(struct k_work *work);

static void disableProdModeTimerCallback(struct k_timer *unused);
void disableProductionMode(struct k_work *item);

static uint8_t uartRxBuf[UART_RX_BUF_NUM][UART_RX_LEN];
static uint8_t *pNextUartBuf = uartRxBuf[1];

extern bool isAdvRunning;
static uint8_t atBuf[AT_MAX_CMD_LEN];
static size_t atBufLen;
static struct k_work handleCommandWork;
static struct k_work genericWork;
extern uint8_t pDefaultGroupNamespace[EDDYSTONE_NAMESPACE_LENGTH];
extern uint8_t uuid[EDDYSTONE_INSTANCE_ID_LEN];

static uint16_t advIntervals[NUM_ADV_INTERVALS] = {20, 100, 1000};
static uint8_t advIntervalIndex = 0;
extern struct k_timer blinkTimer;
int8_t txPower = 0;

static const struct device *pUartDev;

static const struct uart_config uart_cfg = {
    .baudrate = 115200,
    .parity = UART_CFG_PARITY_NONE,
    .stop_bits = UART_CFG_STOP_BITS_1,
    .data_bits = UART_CFG_DATA_BITS_8,
    .flow_ctrl = UART_CFG_FLOW_CTRL_NONE
};

K_TIMER_DEFINE(disableProdModeTimer, disableProdModeTimerCallback, NULL);

int productionStart(void)
{
    int err;
    uint32_t start_time;
    pUartDev = device_get_binding(DT_LABEL(DT_NODELABEL(uart0)));

    if (!pUartDev) {
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

    device_set_power_state(pUartDev, DEVICE_PM_ACTIVE_STATE, NULL, NULL);

    err = uart_rx_enable(pUartDev, uartRxBuf[0], sizeof(uartRxBuf[0]), UART_RX_TIMEOUT);
    if (err) {
        LOG_ERR("Cannot enable rx: %d", err);
        return -EFAULT;
    }

    resetParser();

    err = uart_configure(pUartDev, &uart_cfg);

    if (err == 0) {
        k_work_init(&handleCommandWork, handleCommand);
        //k_work_init(&genericWork, disableProductionMode);
        // k_timer_start(&disableProdModeTimer, K_SECONDS(10), K_NO_WAIT);
    }


    return err;
}

void disableProductionMode(struct k_work *item)
{
    int err;
    printk("Exit production mode, disabling UART\n");
    err = uart_rx_disable(pUartDev);
    if (err) {
        LOG_ERR("disableProductionMode failed to stop rx, err: %d. Trying to disabe anyway.", err);
    }
    k_sleep(K_MSEC(100));
    err = device_set_power_state(pUartDev, DEVICE_PM_OFF_STATE, NULL, NULL);
    if (err) {
        LOG_ERR("Can't power off uart: %d", err);
    }
}

static void disableProdModeTimerCallback(struct k_timer *unused)
{
    // Cannot disable inside of an ISR
    k_work_submit(&genericWork);
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

    switch(txPower){
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

static void handleCommand(struct k_work *work)
{    
    int err;
    int commandLen;
    bool validCommand = true;
    char outBuf[100];
    memset(outBuf, 0, sizeof(outBuf));

    atBuf[MIN(atBufLen, AT_MAX_CMD_LEN - 1)] = 0;
    commandLen = strlen(atBuf);


    if (strncmp("ATI9", atBuf, 4) == 0 && commandLen == 4) {
        sprintf(outBuf, "\r\n\"%s\",\"%s\"\r\n", getGitSha(), getBuildTime());
        sendString(outBuf);
        sendString("OK\r\n");
    } else if (strncmp(GET_MAC_COMMAND, atBuf, GET_MAC_COMMAND_LENGTH) == 0 && commandLen == GET_MAC_COMMAND_LENGTH) {
        bt_addr_le_t addr;
        char macHex[MAC_ADDR_LEN * 2 + 1];
        uint8_t macSwapped[MAC_ADDR_LEN];
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
        bin2hex(macSwapped, MAC_ADDR_LEN, macHex, MAC_ADDR_LEN * 2);
        utilToupper(macHex);
        sprintf(outBuf, "\r\n+UMLA:%s\r\n", macHex);
        sendString(outBuf);
        sendString("OK\r\n");
    } else if (strncmp("AT+TEST", atBuf, 7) == 0 && commandLen == 7) {
        bool ok = true;
        if (!testLis2dw()) {
            ok = false;
            sendString("\r\nLIS_ERROR\r\n");
        }
        if (!testBme280()) {
            ok = false;
            sendString("\r\nBME_ERROR\r\n");
        }
        if (!testApds()) {
            ok = false;
            sendString("\r\nAPDS_ERROR\r\n");
        }

        if (ok) {
            sendString(OK_STR);
        } else {
            sendString(ERROR_STR);
        }
    } else if (strncmp("AT", atBuf, 2) == 0 && commandLen == 2) {
        sendString(OK_STR);
    } else if (strncmp(GET_NAMESPACE_COMMAND, atBuf, GET_NAMESPACE_COMMAND_LENGTH) == 0 && commandLen == GET_NAMESPACE_COMMAND_LENGTH ) {
        sprintf(outBuf, "\r\n+namespace:%s\r\n", pDefaultGroupNamespace);
        sendString(outBuf);
        sendString("OK\r\n");
    } else if (strncmp("AT+CPWROFF", atBuf, 10) == 0 && commandLen == 10) {
        sendString(OK_STR);
        k_sleep(K_MSEC(200));
        sys_reboot(SYS_REBOOT_COLD);
    } else if (strncmp("AT+TXPWR=", atBuf, 9) == 0 && commandLen > 9) {
        errno = 0;
        long power = strtol(&atBuf[9], NULL, 10);
        if (errno == 0 && validTxPowers(power) == 0) {
            storageWriteTxPower(power);
            sendString(OK_STR);
        } else {
            validCommand = false;
            sendString(ERROR_STR);
        }
    }  else if (strncmp( ADV_COMMAND, atBuf, ADV_COMMAND_LENGTH) == 0 && commandLen > ADV_COMMAND_LENGTH) {
        errno = 0;
        long advstate  = strtol(&atBuf[ADV_COMMAND_LENGTH], NULL, 10);
        if (errno == 0 && (advstate == 0 || advstate ==1 )) {
            if ( advstate == 0 ) {
                if(isAdvRunning== true){
                btAdvStop();                
                ledsSetState(LED_BLUE, 0);
                k_sleep(K_MSEC(MIN_LED_ADV_IND_MS));
                ledsSetState(LED_BLUE, 1);
                k_timer_start(&blinkTimer, K_MSEC(advIntervals[advIntervalIndex]), K_NO_WAIT);
                isAdvRunning=false;
              } else {
                  sendString(ERROR_STR);
              }
            } else {
                btAdvStart();
                isAdvRunning=true;
            }
            //CONFID-NAV
            //434f4e4649442d4e4156
            //AT+UDFFILT=1,2,"434f4e4649442d4e4156"\r
            sendString(OK_STR);
        } else {
            validCommand = false;
            sendString(ERROR_STR);
        }

    } else if (strncmp(ID_COMMAND, atBuf, ID_COMMAND_LENGTH) == 0 && commandLen == ID_COMMAND_LENGTH+EDDYSTONE_NAMESPACE_LENGTH+EDDYSTONE_INSTANCE_ID_LEN) {

        if(isAdvRunning==false){
          storageGetTxPower(&txPower);
          memcpy(pDefaultGroupNamespace, atBuf+ID_COMMAND_LENGTH, EDDYSTONE_NAMESPACE_LENGTH);
          memcpy(uuid, atBuf+ID_COMMAND_LENGTH+EDDYSTONE_NAMESPACE_LENGTH, EDDYSTONE_INSTANCE_ID_LEN);
          storageWriteNameSpace(pDefaultGroupNamespace, EDDYSTONE_NAMESPACE_LENGTH);
          storageWriteInstanceID(uuid,EDDYSTONE_INSTANCE_ID_LEN);
          btAdvInit(advIntervals[advIntervalIndex], advIntervals[advIntervalIndex], pDefaultGroupNamespace , uuid, txPower);
          //btAdvStart();
          sendString(OK_STR);
        } else {
           sendString("\r\nAdvertising\r\n");
        }

    } else {
        validCommand = false;
        sendString(ERROR_STR);
    }

    //if (validCommand) {
    //    k_timer_stop(&disableProdModeTimer);
    //}

    resetParser();

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
        LOG_INF("RX_STOPPED (%d)", evt->data.rx_stop.reason);
        break;
    case UART_RX_DISABLED:
        break;
    default:
        break;
    }
}

static void resetParser(void)
{
    memset(atBuf, 0, sizeof(atBuf));
    atBufLen = 0;
}

static void sendString(char* str)
{
    for (int i = 0; i < strlen(str); i++) {
        uart_poll_out(pUartDev, str[i]);
    }
}

static bool testLis2dw(void)
{
    struct sensor_value acc_val[3];
    const struct device *sensor = device_get_binding(DT_LABEL(DT_INST(0, st_lis2dw12)));

    if (sensor == NULL) {
        LOG_ERR("Could not get %s devicen", DT_LABEL(DT_INST(0, st_lis2dw12)));
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
        int16_t x_scaled = (int16_t)(sensor_value_to_double(&acc_val[0])*(32768/16));
        int16_t y_scaled = (int16_t)(sensor_value_to_double(&acc_val[1])*(32768/16));
        int16_t z_scaled = (int16_t)(sensor_value_to_double(&acc_val[2])*(32768/16));
        LOG_INF("x: %d y: %d z: %d", x_scaled, y_scaled, z_scaled);
    } else {
        LOG_ERR("Failed fetching sample from %s", sensor->name);
        return false;
    }

    return true;
}

static bool testBme280(void)
{
    struct sensor_value temp, press, humidity;
	const struct device *sensor = device_get_binding(DT_LABEL(DT_INST(0, bosch_bme280)));

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

    int err = sensor_sample_fetch(sensor);
    if (!err) {
        sensor_channel_get(sensor, SENSOR_CHAN_AMBIENT_TEMP, &temp);
        sensor_channel_get(sensor, SENSOR_CHAN_PRESS, &press);
        sensor_channel_get(sensor, SENSOR_CHAN_HUMIDITY, &humidity);

        LOG_INF("temp: %d.%06d; press: %d.%06d; humidity: %d.%06d",
                temp.val1, temp.val2, press.val1, press.val2,
                humidity.val1, humidity.val2);
    } else {
        LOG_ERR("Failed fetching sample from %s", sensor->name);
        return false;
    }

	return true;
}

static bool testApds(void)
{
    uint8_t id = 0;
    const struct device *i2c_dev = device_get_binding(I2C_DEV);

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
