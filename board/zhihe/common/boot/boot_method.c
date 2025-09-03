// SPDX-License-Identifier: GPL-2.0+
/*
 * (C) Copyright 2025 Zhihe Computing Technology (Shenzhen) Co., Ltd.
 */

#include <log.h>
#include <serial.h>
#include <time.h>
#include <spl.h>

/*
 * Save current boot device
 */
static u32 _first_boot_device = BOOT_DEVICE_MMC1;
static void boot_set_device(u32 device)
{
	_first_boot_device = device;
}

u32 spl_boot_get_device(void)
{
	return _first_boot_device;
}

/*
 * Uart boot select
 * Return
 *   1: get BOOT_DEVICE_BOOTROM Magic
 *   0: get nothing
 */
static int uart_boot_check(void)
{
	unsigned long us_start = timer_get_us();
	int getc_count = 0;

	while (timer_get_us() - us_start < 10000) {
		if (serial_tstc()) {
			char c = serial_getc();
			//printf("%c\n", c);
			if (c == '!') {
				getc_count ++;
				if (getc_count >= 1) {
					printf("Boot SPL u-boot\n");
					return 1;
				}
			}
		}
	}
	return 0;
}

/*
 * Get first boot device
 */
extern int board_bootrom_fastboot(void);
u32 spl_boot_device(void)
{
	if(uart_boot_check()) {
		/* Uart boot check */
		boot_set_device(BOOT_DEVICE_BOOTROM);
	} else if (board_bootrom_fastboot()) {
		/* Bootsel boot check */
		boot_set_device(BOOT_DEVICE_BOOTROM);
	} else {
		/* Default boot device */
		boot_set_device(BOOT_DEVICE_MMC1);
	}

	return _first_boot_device;
}
