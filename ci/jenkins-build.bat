PATH=.\bin;C:\u-blox\ncs\v2.1.0\toolchain\opt\bin;C:\u-blox\ncs\v2.1.0\toolchain\opt\bin\Scripts;%PATH%

SET ZEPHYR_BASE=C:\u-blox\ncs\v2.1.0\zephyr

west build -c -p -b ubx_evkninab4_nrf52833 -d build/debug-no-boot -- -DBUILD_NUMBER=%1 || goto :error

west build -c -p -b ubx_evkninab4_nrf52833 -d build/release-no-boot -- -DRELEASE=1 -DBUILD_NUMBER=%1 || goto :error

west build -c -p -b ubx_evkninab4_nrf52833 -d build/debug -- -DNRF_DFU_BOOT_SUPPORT=1 -DBUILD_NUMBER=%1 || goto :error

west build -c -p -b ubx_evkninab4_nrf52833 -d build/release -- -DNRF_DFU_BOOT_SUPPORT=1 -DRELEASE=1 -DBUILD_NUMBER=%1 || goto :error

goto :EOF

:error
echo Failed with error #%errorlevel%.
exit /b %errorlevel%
