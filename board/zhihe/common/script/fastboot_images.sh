#!/bin/sh

FAIL="###### Images flashing failed"

echo "###### Start the flashing tool"

if [ -n "$1" ]; then
    device="-s $1"
fi

if [ -e bootzero-rvbl.bin ]; then
    fastboot ${device} flash ram bootzero-rvbl.bin || { echo $FAIL; exit 1; }
    fastboot ${device} reboot
    fastboot ${device} flash ram spl-with-fit-rvbl.bin || { echo $FAIL; exit 1; }
    fastboot ${device} reboot
else
    fastboot ${device} flash ram spl-with-fit-rvbl.bin || { echo $FAIL; exit 1; }
    fastboot ${device} reboot
fi

echo "###### Wait for the flashing tool to be ready"
sleep 5

echo "###### Flash gpt"
fastboot ${device} flash gpt emmc-gpt_primary.img || { echo $FAIL; exit 1; }
echo "###### Flash loader"
fastboot ${device} flash mmc0boot0 emmc_boot-loader.img || { echo $FAIL; exit 1; }
echo "###### Flash partition uboot_env"
fastboot ${device} flash uboot_env emmc-uboot_env.img || { echo $FAIL; exit 1; }
echo "###### Flash partition boot"
fastboot ${device} flash boot emmc-boot_a.img || { echo $FAIL; exit 1; }
echo "###### Flash partition system"
fastboot ${device} flash system emmc-system_a.img || { echo $FAIL; exit 1; }
echo "###### Flash partition app"
fastboot ${device} flash app emmc-app_a.img || { echo $FAIL; exit 1; }
echo "###### Flash partition data"
fastboot ${device} flash data emmc-data.img || { echo $FAIL; exit 1; }

echo "###### Images flashed success"
