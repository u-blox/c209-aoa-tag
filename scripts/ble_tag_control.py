"""
AT commands over Nordic UART Service
"""

import asyncio
import sys
import time

from bleak import BleakScanner, BleakClient
from bleak.backends.scanner import AdvertisementData
from bleak.backends.device import BLEDevice

# Nordic UART Service UUIDs
UART_SERVICE_UUID = "6E400001-B5A3-F393-E0A9-E50E24DCCA9E"
UART_RX_CHAR_UUID = "6E400002-B5A3-F393-E0A9-E50E24DCCA9E"
UART_TX_CHAR_UUID = "6E400003-B5A3-F393-E0A9-E50E24DCCA9E"


def handle_disconnect(device: BleakClient):
    print("Tag disconnected", device.address)


async def list_uart_devices(timeout=5.0):
    devices = {}

    def detection_callback(device, advertisement_data):
        if UART_SERVICE_UUID.lower() in advertisement_data.service_uuids:
            devices[device.address] = device

    scanner = BleakScanner()
    scanner.register_detection_callback(detection_callback)
    await scanner.start()
    await asyncio.sleep(timeout)
    await scanner.stop()
    print("Done")
    return devices


async def send_at_commands(device, command_list):
    rsps = []
    cmd_sent_evt = asyncio.Event()
    num_tries = 0
    max_tries = 8

    def handle_rx(_: int, data: bytearray):
        print("received:", data)
        rsps.append(data.decode("utf-8"))
        cmd_sent_evt.set()

    for i in range(max_tries):
        try:
            print("Try connecting to", device.address)
            async with BleakClient(
                device, timeout=10.0, disconnected_callback=handle_disconnect
            ) as client:
                await client.start_notify(UART_TX_CHAR_UUID, handle_rx)

                for at in command_list:
                    cmd_sent_evt.clear()
                    print("Send:", at)
                    await client.write_gatt_char(UART_RX_CHAR_UUID, str.encode(at))
                    try:
                        await asyncio.wait_for(cmd_sent_evt.wait(), timeout=2)
                    except asyncio.TimeoutError:
                        print("Timeout error no rsp")

                print("Done diconnect")
                await client.disconnect()
                return rsps
        except Exception as e:
            num_tries = num_tries + 1
            await asyncio.sleep(3)
            if num_tries == max_tries:
                raise e
    return None


def main(devices, command_list):
    return asyncio.gather(*(dummy(device, command_list) for device in devices))


def ble_send_at_commands(devices, command_list):
    loop = asyncio.new_event_loop()
    asyncio.set_event_loop(loop)
    if isinstance(devices, list):
        print("Connect to: ", devices)
        group = asyncio.gather(
            *(send_at_commands(device, command_list) for device in devices)
        )
        results = loop.run_until_complete(group)
        print(results)
        loop.close()
        return results
    else:
        return asyncio.run(send_at_commands(device, command_list))


def ble_list_uart_devices(timeout):
    return asyncio.run(list_uart_devices(timeout))
