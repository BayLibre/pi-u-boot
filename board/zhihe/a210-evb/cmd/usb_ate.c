// SPDX-License-Identifier: GPL-2.0+
/*
 * Copyright 2025 Zhihe Computing Ltd.
 */
#include <stdint.h>
#include <command.h>
#include <console.h>
#include <linux/delay.h>

#define C10PHY_CR_ADDR 0x8040000
#define MEM32(addr) *((volatile uint32_t *)(addr))

static inline void wr32(uint64_t addr, uint32_t data)
{
        MEM32(C10PHY_CR_ADDR + (addr << 2)) = data;
        return;
}

static inline void rd32(uint64_t addr, uint32_t expected, uint32_t mask)
{
        uint32_t data;
        data = MEM32(C10PHY_CR_ADDR + (addr << 2));
        if ((data & mask) != expected)
                printf("Unexpected value at addr(offset) 0x%llx: 0x%x != 0x%x\n",
                addr, data, expected);
        return;
}

static inline void wait_ns(int32_t ns)
{
        ndelay(ns);
        return;
}

static int do_usb_ate(struct cmd_tbl *cmdtp, int flag, int argc,
                      char *const argv[])
{
        printf("Start command usb_ate\n");
        wr32(0xa018, 0x8000); 
        wr32(0xa020, 0x5000); 

        for (uint32_t i = 0x6c85; i < 0x6fff; i++)
                wr32(i, 0x0000);

        wait_ns(25000); 
        wr32(0xa080, 0x0001); 
        wr32(0xa016, 0x0007); 
        wr32(0xa018, 0x8faf); 
        wr32(0x200a, 0x002e); 
        wr32(0x0002, 0x1011); 
        wr32(0x0002, 0x38bb); 
        wait_ns(5000); 
        wr32(0x203b, 0x003a); 
        wait_ns(5000); 
        wr32(0xa01a, 0x0002); 
        wr32(0xa01a, 0x000a); 
        wr32(0xa01a, 0x002a); 
        wr32(0xa01a, 0x00aa); 
        wr32(0x0020, 0x0008); 
        wr32(0x002c, 0x0008); 
        wr32(0x000f, 0x00d1); 
        wr32(0x000f, 0x02f1); 
        wr32(0x0008, 0x002f); 
        wr32(0x000c, 0x0000); 
        wr32(0x0005, 0x0688); 
        wr32(0x0005, 0x8688); 
        wr32(0x0009, 0x0000); 
        wr32(0x0009, 0x8000); 
        wr32(0x0007, 0x0188); 
        wr32(0x002a, 0x1026); 
        wr32(0x002b, 0x0166); 
        wr32(0x0006, 0x0000); 
        wr32(0x0006, 0x0001); 
        wr32(0x0006, 0x0181); 
        wr32(0x000b, 0x0000); 
        wr32(0x0036, 0x1000); 
        wr32(0x0037, 0x0100); 
        wr32(0x000a, 0x0000); 
        wr32(0x000a, 0x0000); 
        wr32(0x000a, 0x0080); 
        wr32(0x000a, 0x0080); 
        wr32(0x0003, 0x0209); 
        wr32(0x0004, 0x0200); 
        wr32(0xa0c1, 0x0005); 
        wr32(0xa0c1, 0x0005); 
        wr32(0x9002, 0x28f8); 
        wr32(0x9002, 0xa9f8); 
        wr32(0x9003, 0x0800); 
        wr32(0x9003, 0x2840); 
        wr32(0xa000, 0x1638); 
        wr32(0x9004, 0x002b); 
        wr32(0x9001, 0x8000); 
        wr32(0xa0c0, 0x001e); 
        wr32(0xa019, 0x0003); 
        wr32(0xa019, 0x0013); 
        wr32(0xa01e, 0x0700); 
        wr32(0xa01d, 0x0080); 
        wr32(0x9008, 0x0007); 
        wr32(0x9008, 0x0087); 
        wr32(0x9009, 0x05b2); 
        wr32(0x9009, 0x25b2); 
        wr32(0x900a, 0x6eac); 
        wr32(0x900b, 0x0180); 
        wr32(0x900b, 0x0380); 
        wait_ns(5000); 
        wr32(0x000d, 0x0018); 
        wr32(0x0009, 0x8001); 
        wr32(0x0005, 0x8689); 
        wr32(0x900b, 0x0380); 
        wr32(0x900b, 0x038a); 
        wr32(0xa01f, 0x5de0); 
        wr32(0x9007, 0x0040); 
        wr32(0xa008, 0x1000); 
        wr32(0xa06d, 0x0400); 
        wr32(0xa020, 0x7000); 
        wr32(0xa080, 0x0001); 
        wr32(0xa081, 0x0000); 
        wr32(0xa082, 0x0000); 
        wr32(0xa083, 0x0000); 
        wr32(0xa084, 0x0000); 
        wr32(0xa085, 0x0000); 
        wr32(0xa086, 0x0000); 
        wr32(0xa08d, 0x0000); 
        wr32(0xa08e, 0x0000); 
        wr32(0xa087, 0x0001); 
        wr32(0xa088, 0x0001); 
        wr32(0xa089, 0x0001); 
        wr32(0xa08a, 0x0001); 
        wr32(0xa08b, 0x0001); 
        wr32(0xa08c, 0x0001); 
        wr32(0xa08f, 0x0001); 
        wr32(0xa020, 0x6000); 
        wr32(0xa020, 0x2000); 
        wr32(0xa018, 0x8faa); 
        wait_ns(500000); 
        rd32(0x3056, 1, 1); 
        rd32(0x3156, 1, 1); 
        rd32(0x3256, 1, 1); 
        rd32(0x3356, 1, 1); 
        rd32(0x3004, 0, 1); 
        rd32(0x3104, 0, 1); 
        rd32(0x3204, 0, 1); 
        rd32(0x3304, 0, 1); 
        wr32(0x900f, 0x0002); 
        wr32(0x9007, 0x1ec0); 
        wr32(0x9007, 0x1ec3); 
        wait_ns(40000); 
        rd32(0x101b, 1, 1); 
        rd32(0x111b, 1, 1); 
        rd32(0x121b, 1, 1); 
        rd32(0x131b, 1, 1); 
        wr32(0x9007, 0x1ec2); 
        wait_ns(30000); 
        rd32(0x101b, 0, 1); 
        rd32(0x111b, 0, 1); 
        rd32(0x121b, 0, 1); 
        rd32(0x131b, 0, 1); 
        wr32(0x9007, 0x1ec0); 
        wait_ns(2000); 
        wr32(0x900f, 0x0000); 
        wr32(0x902a, 0x0001); 
        wait_ns(2000); 
        wr32(0xa01f, 0x5df0); 
        wait_ns(500000); 
        rd32(0x101b, 4, 5); 
        rd32(0x111b, 4, 5); 
        rd32(0x121b, 4, 5); 
        rd32(0x131b, 4, 5); 
        wr32(0x9051, 0x0001); 
        wr32(0x9051, 0x0001); 
        wr32(0x9051, 0x0011); 
        wr32(0x9051, 0x0001); 
        wr32(0x9051, 0x0001); 
        wr32(0x9051, 0x0001); 
        wr32(0x9051, 0x0011); 
        wr32(0x9051, 0x0001); 
        wait_ns(10000); 
        rd32(0x1052, 0x0, 0xffffffff);
        wait_ns(1000); 
        rd32(0x1052, 0, 0xffff0000); 
        rd32(0x1152, 0x0, 0xffffffff);
        wait_ns(1000); 
        rd32(0x1152, 0, 0xffff0000); 
        rd32(0x1252, 0x0, 0xffffffff);
        wait_ns(1000); 
        rd32(0x1252, 0, 0xffff0000); 
        rd32(0x1352, 0x0, 0xffffffff);
        wait_ns(1000); 
        rd32(0x1352, 0, 0xffff0000); 
        wr32(0x902a, 0x0011); 
        wr32(0x902a, 0x0001); 
        wait_ns(10000); 
        rd32(0x1052, 0x0, 0x0);
        wait_ns(1000); 
        rd32(0x1052, 1, 0xffff); 
        rd32(0x1152, 0x0, 0x0);
        wait_ns(1000); 
        rd32(0x1152, 1, 0xffff); 
        rd32(0x1252, 0x0, 0x0);
        wait_ns(1000); 
        rd32(0x1252, 1, 0xffff); 
        rd32(0x1352, 0x0, 0x0);
        wait_ns(1000); 
        rd32(0x1352, 1, 0xffff); 
        wait_ns(10000);

        printf("Finished command usb_ate!!!\n");
        return 0;
}

U_BOOT_CMD(
	usb_ate, CONFIG_SYS_MAXARGS, 1, do_usb_ate,
	"run USB&DP C10PHY ATE test",
	"No parameters\n"
);
