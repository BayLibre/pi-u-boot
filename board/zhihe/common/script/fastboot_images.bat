:: Script for flashing images using fastboot

::@echo off

echo ###### Start the flashing tool

IF EXIST "bootzero-rvbl.bin" (
    fastboot flash ram bootzero-rvbl.bin || goto :error
    fastboot reboot
    fastboot flash ram spl-with-fit-rvbl.bin || goto :error
    fastboot reboot 
) ELSE (
    fastboot flash ram emmc_boot-loader.img || goto :error
    fastboot reboot 
)

echo ###### Wait for the flashing tool to be ready
ping 127.0.0.1 -n 5 >nul

echo ###### Flash gpt
fastboot flash gpt emmc-gpt_primary.img || goto :error
echo ###### Flash loader
fastboot flash mmc0boot0 emmc_boot-loader.img || goto :error
echo ###### Flash partition boot
fastboot flash boot emmc-boot_a.img || goto :error
echo ###### Flash partition system
fastboot flash system emmc-system_a.img || goto :error
echo ###### Flash partition app
fastboot flash app emmc-app_a.img || goto :error
echo ###### Flash partition home
fastboot flash home emmc-home.img || goto :error

echo ###### Images flashed success
pause
exit /b 0

:error
echo ###### Images flashing failed
pause
exit /b 1
