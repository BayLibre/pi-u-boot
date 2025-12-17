:: Script for flashing images using fastboot

@echo off

echo ^< waiting for any device ^>
fastboot getvar product 2> getvar.tmp
find "product: a2" "getvar.tmp" >nul

IF %errorlevel% equ 0 (
    echo ###### Start flash images ######
) ELSE (
	IF EXIST "bootzero-rvbl.bin" (
		echo ###### Load flashing tool bootzero and uboot
		fastboot flash ram bootzero-rvbl.bin || goto :error
		fastboot reboot
		fastboot flash ram spl-with-fit-rvbl.bin || goto :error
		fastboot reboot 
	) ELSE (
		echo ###### Load flashing tool uboot
		fastboot flash ram spl-with-fit-rvbl.bin || goto :error
		fastboot reboot 
	)
	echo ###### Wait for the flashing tool to be ready
	ping 127.0.0.1 -n 5 >nul
)

echo ###### Flash gpt
fastboot flash gpt emmc-gpt_primary.img || goto :error
echo ###### Flash loader
fastboot flash mmc0boot0 emmc_boot-loader.img || goto :error
echo ###### Flash uboot_env
fastboot flash uboot_env emmc-uboot_env.img || goto :error
echo ###### Flash partition boot
fastboot flash boot emmc-boot_a.img || goto :error
echo ###### Flash partition system
fastboot flash system emmc-system_a.img || goto :error
echo ###### Flash partition app
fastboot flash app emmc-app_a.img || goto :error
echo ###### Flash partition data
fastboot flash data emmc-data.img || goto :error

echo ###### Images flashed success ######
pause
exit /b 0

:error
echo ###### Images flashing failed ######
pause
exit /b 1
