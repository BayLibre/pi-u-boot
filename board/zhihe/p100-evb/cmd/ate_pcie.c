#include <console.h>
#include <stdint.h>
#include <cli.h>
#include <command.h>
#include <linux/delay.h>

#define E16PHY_CR0_REG_BASE	0xa100000
#define E16PHY_CR1_REG_BASE	0xa140000

#define MEM32(addr) *((volatile uint32_t *)(addr))

static inline void write_reg(uint64_t addr,uint32_t offset,uint32_t value)
{
	if (addr == E16PHY_CR0_REG_BASE)
		MEM32(E16PHY_CR0_REG_BASE + (offset << 2)) = value;
	else
		MEM32(E16PHY_CR1_REG_BASE + (offset << 2)) = value;
	return;
}

static inline void read_reg(uint64_t addr, uint32_t offset, uint32_t expected, uint32_t mask)
{
	uint32_t data;

	if (addr == E16PHY_CR0_REG_BASE) {
		data = MEM32(E16PHY_CR0_REG_BASE + (offset << 2));
		if ((data & mask) != expected)
			printf("Unexpected value at addr(offset) 0x%llx: 0x%x != 0x%x\n",
			addr, data, expected);
	}
	else {
		data = MEM32(E16PHY_CR1_REG_BASE + (offset << 2));
		if ((data & mask) != expected)
			printf("Unexpected value at addr(offset) 0x%llx: 0x%x != 0x%x\n",
			addr, data, expected);
	}
		return;
}

static inline void wait_us(unsigned int us)
{
        udelay(us);
}

static void pci_ate_cfg_cr_reg(unsigned int base_addr)
{
	printf("pcie_ate test begin!\n");

	wait_us(200);
	write_reg(base_addr, 0xa006, 0x0007);
	write_reg(base_addr, 0xa006, 0x0007);
	write_reg(base_addr, 0xa001, 0x0f07);
	write_reg(base_addr, 0xa001, 0x0f07);
	write_reg(base_addr, 0x9005, 0x0001);
	write_reg(base_addr, 0x9005, 0x2001);
	write_reg(base_addr, 0x9001, 0x0002);
	write_reg(base_addr, 0x9001, 0x8002);
	wait_us(5);
	write_reg(base_addr, 0x001d, 0x0001);
	write_reg(base_addr, 0x000f, 0x0000);
	write_reg(base_addr, 0x000f, 0x0000);
	write_reg(base_addr, 0x000f, 0x0200);
	write_reg(base_addr, 0x000f, 0x8200);
	write_reg(base_addr, 0x0010, 0x0000);
	write_reg(base_addr, 0x0010, 0x0200);
	write_reg(base_addr, 0x0020, 0x0008);
	write_reg(base_addr, 0x002d, 0x0008);
	write_reg(base_addr, 0x9025, 0x0515);
	write_reg(base_addr, 0x0002, 0x0435);
	wait_us(5);
	write_reg(base_addr, 0x0002, 0x1435);
	write_reg(base_addr, 0x0012, 0x0151);
	write_reg(base_addr, 0x0012, 0x0371);
	write_reg(base_addr, 0x0008, 0xa037);
	write_reg(base_addr, 0x000d, 0xa037);
	write_reg(base_addr, 0x0006, 0x0060);
	write_reg(base_addr, 0x000b, 0x0060);
	write_reg(base_addr, 0x0007, 0x0051);
	write_reg(base_addr, 0x000c, 0x0051);
	write_reg(base_addr, 0x0004, 0x0a06);
	write_reg(base_addr, 0x0004, 0x8a06);
	write_reg(base_addr, 0x0009, 0x0506);
	write_reg(base_addr, 0x0009, 0x4506);
	write_reg(base_addr, 0x0005, 0x0000);
	write_reg(base_addr, 0x0005, 0x0000);
	write_reg(base_addr, 0x000a, 0x0000);
	write_reg(base_addr, 0x000a, 0x0000);
	write_reg(base_addr, 0x0003, 0x0000);
	write_reg(base_addr, 0x202d, 0x0000);
	write_reg(base_addr, 0x202d, 0x0024);
	write_reg(base_addr, 0x000e, 0x0018);
	write_reg(base_addr, 0x000e, 0x001b);
	wait_us(200);
	read_reg(base_addr, 0x001b, 0x919e, 0xffff0000);
	write_reg(base_addr, 0x000e, 0x001a);
	wait_us(20);
	read_reg(base_addr, 0x001b, 0x909e, 0xffff0000);
	write_reg(base_addr, 0x0009, 0x4507);
	wait_us(4000);
	read_reg(base_addr, 0x001b, 0xd09e, 0xffff0000);
	write_reg(base_addr, 0xa0c1, 0x0015);
	write_reg(base_addr, 0xa0c1, 0x0015);
	write_reg(base_addr, 0x9000, 0x0001);
	write_reg(base_addr, 0x9000, 0x0005);
	write_reg(base_addr, 0x9002, 0x50f8);
	write_reg(base_addr, 0x9002, 0xd1f8);
	write_reg(base_addr, 0x9003, 0x0000);
	write_reg(base_addr, 0x9003, 0x2040);
	write_reg(base_addr, 0x9001, 0xb103);
	write_reg(base_addr, 0x9001, 0xb103);
	write_reg(base_addr, 0x901a, 0x0000);
	write_reg(base_addr, 0xa001, 0x0f06);
	write_reg(base_addr, 0xa001, 0x0f06);
	write_reg(base_addr, 0x9001, 0xb101);
	write_reg(base_addr, 0x9001, 0xb101);
	wait_us(2000);
	read_reg(base_addr, 0x1010, 0x0000, 0xffff0000);
	read_reg(base_addr, 0x1110, 0x0000, 0xffff0000);
	write_reg(base_addr, 0x902b, 0x0001);
	write_reg(base_addr, 0x9005, 0x2405);
	write_reg(base_addr, 0x9005, 0x2405);
	write_reg(base_addr, 0x9006, 0x0ca2);
	write_reg(base_addr, 0x9006, 0x0ea2);
	write_reg(base_addr, 0x9007, 0x0550);
	write_reg(base_addr, 0x9007, 0x2550);
	write_reg(base_addr, 0x9008, 0x0641);
	write_reg(base_addr, 0x9008, 0x0e41);
	write_reg(base_addr, 0x9009, 0x1968);
	write_reg(base_addr, 0x900a, 0x0002);
	write_reg(base_addr, 0x900a, 0x0402);
	write_reg(base_addr, 0x901b, 0x000a);
	write_reg(base_addr, 0xa008, 0x0000);
	write_reg(base_addr, 0xa008, 0x0120);
	write_reg(base_addr, 0xa019, 0x0004);
	write_reg(base_addr, 0xa019, 0x0016);
	write_reg(base_addr, 0xa01b, 0x0000);
	write_reg(base_addr, 0xa01b, 0x1010);
	write_reg(base_addr, 0xa019, 0x0256);
	write_reg(base_addr, 0xa019, 0x0656);
	write_reg(base_addr, 0xa006, 0x0006);
	write_reg(base_addr, 0xa006, 0x0006);
	write_reg(base_addr, 0x9005, 0x2404);
	write_reg(base_addr, 0x9005, 0x2404);
	wait_us(500);
	read_reg(base_addr, 0x1017, 0x000c, 0xffff0000);
	read_reg(base_addr, 0x1117, 0x000c, 0xffff0000);
	write_reg(base_addr, 0x9051, 0x0001);
	write_reg(base_addr, 0x9051, 0x0001);
	write_reg(base_addr, 0x9051, 0x0011);
	write_reg(base_addr, 0x9051, 0x0001);
	write_reg(base_addr, 0x9051, 0x0011);
	write_reg(base_addr, 0x9051, 0x0001);
	wait_us(10);
	read_reg(base_addr, 0x1052, 0x0000, 0xffff0000);
	read_reg(base_addr, 0x1052, 0x0000, 0xffff0000);
	read_reg(base_addr, 0x1152, 0x0000, 0xffff0000);
	read_reg(base_addr, 0x1152, 0x0000, 0xffff0000);
	write_reg(base_addr, 0x902b, 0x0011);
	write_reg(base_addr, 0x902b, 0x0001);
	wait_us(10);
	read_reg(base_addr, 0x1052, 0x0001, 0xffff0000);
	read_reg(base_addr, 0x1052, 0x0001, 0xffff0000);
	read_reg(base_addr, 0x1152, 0x0001, 0xffff0000);
	read_reg(base_addr, 0x1152, 0x0001, 0xffff0000);
	wait_us(10);
	printf("pcie_ate test over!\n");
}

 static unsigned int do_pci_ate(struct cmd_tbl *cmdtp, int flag, int argc, char * const argv[])
{
	const char *cmd;

	if (argc < 2)
		return CMD_RET_USAGE;

	cmd = argv[1];
	if (strcmp(cmd, "cr0") == 0)
		pci_ate_cfg_cr_reg(E16PHY_CR0_REG_BASE);
	else if	(strcmp(cmd, "cr1") == 0)
		pci_ate_cfg_cr_reg(E16PHY_CR1_REG_BASE);

	return 0;
}

 U_BOOT_CMD(
	ate_pcie,	2,	1,	do_pci_ate,
	"support ate verify for PCIe",
	"ate_pcie cr0:config E16PHY_CR0_REG\n"
	"ate_pcie cr1:config E16PHY_CR0_REG\n"
);
