"""
AT commands over Nordic UART Service
"""

import asyncio

from bleak import BleakScanner, BleakClient
import numpy as np

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
        print(device, "received:", data)
        rsps.append(data.decode("utf-8"))
        cmd_sent_evt.set()

    for i in range(max_tries):
        try:
            print("Try connecting to", device)
            async with BleakClient(
                device, timeout=10.0, disconnected_callback=handle_disconnect
            ) as client:
                await client.start_notify(UART_TX_CHAR_UUID, handle_rx)

                for at in command_list:
                    cmd_sent_evt.clear()
                    print("Send:", at)
                    tries = 0
                    while tries < 3:
                        await client.write_gatt_char(UART_RX_CHAR_UUID, str.encode(at))
                        try:
                            await asyncio.wait_for(cmd_sent_evt.wait(), timeout=2)
                            break
                        except asyncio.TimeoutError:
                            tries = tries + 1
                            print("Timeout error no rsp. Try:", tries)

                print("Done diconnect")
                await client.disconnect()
                return rsps
        except Exception as e:
            num_tries = num_tries + 1
            if num_tries == max_tries:
                return []
                pass
    return None


def ble_send_at_commands(devices, command_list):
    if isinstance(devices, list):
        slices = len(devices) / 4
        for devices_chunk in np.array_split(devices, slices):
            print("devices_chunk", devices_chunk)
            loop = asyncio.new_event_loop()
            asyncio.set_event_loop(loop)
            group = asyncio.gather(
                *(send_at_commands(device, command_list) for device in devices_chunk)
            )
            results = loop.run_until_complete(group)
            loop.close()
        return results
    else:
        return asyncio.run(send_at_commands(devices, command_list))


def ble_list_uart_devices(timeout):
    return asyncio.run(list_uart_devices(timeout))
