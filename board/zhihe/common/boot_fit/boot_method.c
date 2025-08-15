// SPDX-License-Identifier: GPL-2.0+
/*
 * (C) Copyright 2025 Zhihe Computing Technology (Shenzhen) Co., Ltd.
 */

#include <log.h>
#include <serial.h>
#include <time.h>
#include "../include/spl_fit.h"

/*
 * Boot method check
 * Return
 *   1: boot u-boot on spl payload
 *   0: boot emmc
 */
int board_spl_boot_check(void)
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
