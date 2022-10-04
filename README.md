# AoA Tag Sample Application
This repository contains an example Tag application for use with u-blox C211 Bluetooth Direction Finding Anchor. It is intended to be flashed on the u-blox C209 Tag which comes bundled with a C211 in the XPLR-AOA-1 and XPLR-AOA-2 kits.

# Getting the code
`git clone repo_url`

# Setup
## Building with command line
- Open nRF Connect and open Toolchain Manager
- Download nRF Connect SDK v2.0.0.
- Right next to the "nRF Connect SDK v2.0.0" you have a dropdown, click "Open bash". This step is not required, but it will set up the necessary environment variables for you.
- `cd` to project folder
- `west build -b ubx_evkninab4_nrf52833`
- `west flash`
- `west flash` only should be enough for building + flashing after that, specifying the board is only needed on the first build.

## Building nRF Connect VS Code plugin
In Visual Studio Code go to marketplace and search for "nrf connect"
Install the nRF Connect Extension Pack (this will install all required extensions at once)

In VS Code:
- Click on the nRF Connect extention and select "Add an existing application"
- Choose the folder c209-aoa-tag
- Press ctrl+shift+p and search/select "nRF Connect: Select nRF Connect SDK"
- Press "Yes" in the popup asking if you want to change it
- In the dropdown you should see the path where nRF Connect was installed before. Choose it.
- Under "applications" press in "No build configurations Click to create one"
- Choose custom board and select ubx_evkninab4_nrf52833 in the drowdown
- Press "Build Configuration" button
- Now project should build successfully

## Release vs. Debug build
The `prj.conf` is split into multiple files, first there is `prj_base.conf` and that contains all common config for both release and debug.
Then there are `prj_debug.conf` and `prj_release.conf`, these contain configurations specific to debug or release, for example compiler optimization level and logging config. By default a debug build is made, to build release run `west build -p -b ubx_evkninab4_nrf52833 -- -DRELEASE`. Those options can also be input when adding the application in the nRF Connect VS Code plugin under "Extra CMake arguments".

## Running on other boards
This sample application primarily supports the u-blox **C209** application board bundled together with the u-blox **C211** in the **XPLR-AOA-1**.

However getting it up and running on other boards which either use NINA-B4 module (like **NINA-B4-EVK**) or a bare NRF52833 should only be a matter of selecting the appropriate board file.

## Building the application to use with OpenCPU DFU Bootloader
C209 boards come pre flashed with a DFU bootloader. To build a binary that is compatible with that the `CONFIG_FLASH_BASE_ADDRESS` needs to be changed. If the pre-flashed bootloader has been erased or overwritten then flash the dfu_bootloader/mbr_nrf52_2.4.1_mbr.hex and dfu_bootloader/nrf52833_xxaa_bootloader.hex using using J-Flash Lite/nrfjprog or similar tool to restore it. See the following complete steps to build and flash.
- In menuconfig change "Flash Base Address" from 0x0 to 0x1000 (or uncomment CONFIG_FLASH_BASE_ADDRESS=0x1000 in prj_base.conf)
- Re-Run CMAKE and re-build
- Copy the zephyr.bin file from build/zephyr folder to dfu_bootloader.
- In dfu_bootloader run `nrfutil pkg generate --hw-version 52 --sd-req 0x00 --application-version 0 --application zephyr.bin app.zip`
- To enter DFU mode when an application is flashed hold down SWITCH_2 and click reset.
- In dfu_bootloader run `nrfutil dfu serial -pkg app.zip -p COMXXX -b 115200 -fc 1`
- Now the module will be flashed. It's also possible to keep the "Flash Base Address" to 0x1000 and develop as usual and flashing with SeS or west as it will flash the application on the correct address and it will be started by the open bootloader.

`nrfutil` executable for flashing with OpenCPU DFU Bootloader can be downloaded from here: https://github.com/NordicSemiconductor/pc-nrfutil/releases 

# Communication with AT commands
In `src/at_host` there is a __very__ basic AT command handler.
# Over UART
When the application boots it will accept AT commands over the UART for 10s before it shuts off the UART in order to save power.
If a successful AT command was sent within 10s the application will keep UART enabled until it's reset.
## Over NUS
If Kconfig `CONFIG_SEND_SENSOR_DATA_IN_PER_ADV_DATA` is enabled (default yes) then the application will accept AT commands over the Nordic UART Service.
Each write will be parsed as an AT command so no need for line termination characters etc.

# Using the Sensors on the C209
The C209 application board comes with some sensors. Study `src/sensors.c` for example how to get data from the sensors.

# Code formatting and style
Code formatting and style follows [ubxlib](https://github.com/u-blox/ubxlib/blob/master/astyle.cfg).

To automatically format the code run:

`./tools/format_code.sh`

# Using the application
Please review the [user guide](https://www.u-blox.com/en/docs/UBX-21004616) for more details on how to use the XPLR-AOA-1 and XPLR-AOA-2 kits.

# More information about the hardware kits
For more information about the hardware kits as well as ordering information, please visit the [XPLR-AOA-1 product page](https://www.u-blox.com/en/product/xplr-aoa-1-kit) and [XPLR-AOA-2 product page](https://www.u-blox.com/en/product/xplr-aoa-2-kit).

# License
The software in this repository is Apache 2.0 licensed and copyright u-blox:

In all cases copyright, and our thanks, remain with the original authors.
