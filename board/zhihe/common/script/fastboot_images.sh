#!/bin/sh

FAIL="###### Images flashing failed"

echo "###### Start the flashing tool"

if [ -e bootzero-rvbl.bin ]; then
    fastboot flash ram bootzero-rvbl.bin || { echo $FAIL; exit 1; }
    fastboot reboot
    fastboot flash ram spl-with-fit-rvbl.bin || { echo $FAIL; exit 1; }
    fastboot reboot 
else
    fastboot flash ram emmc_boot-loader.img || { echo $FAIL; exit 1; }
    fastboot reboot
fi

echo "###### Wait for the flashing tool to be ready"
sleep 5

echo "###### Flash gpt"
fastboot flash gpt emmc-gpt_primary.img || { echo $FAIL; exit 1; }
echo "###### Flash loader"
fastboot flash mmc0boot0 emmc_boot-loader.img || { echo $FAIL; exit 1; }
echo "###### Flash partition boot"
fastboot flash boot emmc-boot_a.img || { echo $FAIL; exit 1; }
echo "###### Flash partition system"
fastboot flash system emmc-system_a.img || { echo $FAIL; exit 1; }
echo "###### Flash partition app"
fastboot flash app emmc-app_a.img || { echo $FAIL; exit 1; }
echo "###### Flash partition home"
fastboot flash home emmc-home.img || { echo $FAIL; exit 1; }

echo "###### Images flashed success"
