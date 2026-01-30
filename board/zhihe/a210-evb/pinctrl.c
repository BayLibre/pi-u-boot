// SPDX-License-Identifier: GPL-2.0+
/*
 * (C) Copyright 2024-2025 Zhihe Computing Technology (Shenzhen) Co., Ltd.
 */

#include <command.h>
#include <asm/io.h>
#include <asm/gpio.h>

#include "include/addr_defines.h"
#include "include/board.h"
#include <linux/delay.h>

#define PAD_GRP_BASE_SET(x)          (x << 12)
#define PAD_GRP_IDX_GET(x)           ((x >> 12) & 0xF)
#define PAD_INDEX(x)                 (x & 0xFFF)

#define PIN_DRV_MAX                  (15)
#define PADMUX_REG_ADDR(base,index)  ((void*)(base) + ((index >> 3) << 2))
#define PADMUX_REG_BIT_POS(index)    ((index & 0x7) << 2)
#define PADMUX_REG_MASK(index)       (0xF << PADMUX_REG_BIT_POS(index))

#define PADCFG_REG_ADDR(base,index)  ((void*)(base) + ((index >> 1) << 2))
#define PADCFG_REG_BIT_POS(index)    ((index & 0x1) << 4)
#define PADCFG_REG_PULL_MASK(index)  (0x70 << PADCFG_REG_BIT_POS(index))
#define PADCFG_REG_DRV_MASK(index)   (0xF << PADCFG_REG_BIT_POS(index))
#define PADCFG_REG_SLEW_MASK(index)  (0x100 << PADCFG_REG_BIT_POS(index))

enum {
	SOC_PIN_AON = 0x0,
	SOC_PIN_PERI1,
	SOC_PIN_PERI2,
	SOC_PIN_PERI3,
};

enum {
	PIN_PD,
	PIN_PU,
	PIN_PN,
};

enum {
	PIN_SPEED_NORMAL,
	PIN_SPEED_FAST
};

typedef enum {
	OSC_CLK_IN = PAD_GRP_BASE_SET(SOC_PIN_AON),
	OSC_CLK_OUT,
	RST_N_IN,
	RTC_CLK_IN,
	RTC_CLK_OUT,
	TEST_MODE,
	POR_SEL,
	RST_N_OUT,
	BOOT_SEL0,
	BOOT_SEL1,
	DEBUG_MODE,
	AOUART_TXD,
	AOUART_RXD,
	AOI2C0_SCL,
	AOI2C0_SDA,
	AOI2C1_SCL,
	AOI2C1_SDA,
	CPU_JTG_TCLK,
	CPU_JTG_TMS,
	CPU_JTG_TDI,
	CPU_JTG_TDO,
	CPU_JTG_TRST,
	AOGPIO0_21,
	AOGPIO0_22,
	AOGPIO0_23,
	AOGPIO0_24,
	AOGPIO0_25,
	AOGPIO0_26,
	AOGPIO0_27,
	AOGPIO0_28,
	AOGPIO0_29,
	AOGPIO0_30,
	AOGPIO0_31,
	AOGPIO1_0,
	AOGPIO1_1,
	AOGPIO1_2,
	AOGPIO1_3,
	AOGPIO1_4,
	AOGPIO1_5,
	AOGPIO1_6,
	AOGPIO1_7,
	AOGPIO1_8,
	AOGPIO1_9,

	GPIO0_0 = PAD_GRP_BASE_SET(SOC_PIN_PERI1),
	GPIO0_1,
	GPIO0_2,
	GPIO0_3,
	GPIO0_4,
	GPIO0_5,
	GPIO0_6,
	GPIO0_7,
	GPIO0_8,
	GPIO0_9,
	GPIO0_10,
	GPIO0_11,
	GPIO0_12,
	GPIO0_13,
	GPIO0_14,
	GPIO0_15,
	GPIO0_16,
	GPIO0_17,
	GPIO0_18,
	GPIO0_19,
	GPIO0_20,
	GPIO0_21,
	GPIO0_22,
	GPIO0_23,
	GPIO0_24,
	GPIO0_25,
	GPIO0_26,
	GPIO0_27,
	GPIO0_28,
	GPIO0_29,
	GPIO0_30,
	GPIO0_31,
	GPIO1_0,
	GPIO1_1,
	GPIO1_2,
	GPIO1_3,
	GPIO1_4,
	GPIO1_5,
	GPIO1_6,
	GPIO1_7,
	GPIO1_8,
	GPIO1_9,
	GPIO1_10,
	GPIO1_11,
	GPIO1_12,
	GPIO1_13,
	GPIO1_14,
	GPIO1_15,
	GPIO1_16,
	MULTI_DIE_PACKAGE,

	GPIO2_0 = PAD_GRP_BASE_SET(SOC_PIN_PERI2),
	GPIO2_1,
	GPIO2_2,
	GPIO2_3,
	GPIO2_4,
	GPIO2_5,
	GPIO2_6,
	GPIO2_7,
	GPIO2_8,
	GPIO2_9,
	GPIO2_10,
	GPIO2_11,
	GPIO2_12,
	GPIO2_13,
	GPIO2_14,
	GPIO2_15,
	GPIO2_16,
	GPIO2_17,
	GPIO2_18,
	GPIO2_19,
	GPIO2_20,
	GPIO2_21,
	GPIO2_22,
	GPIO2_23,
	GPIO2_24,
	GPIO2_25,
	GPIO2_26,
	GPIO2_27,
	GPIO2_28,
	GPIO2_29,
	GPIO2_30,
	GPIO2_31,
	GPIO3_0,
	GPIO3_1,
	GPIO3_2,
	GPIO3_3,
	GPIO3_4,
	GPIO3_5,
	GPIO3_6,
	GPIO3_7,
	GPIO3_8,
	GPIO3_9,
	GPIO3_10,

	SDIO_CLK = PAD_GRP_BASE_SET(SOC_PIN_PERI3),
	SDIO_CMD,
	SDIO_DAT0,
	SDIO_DAT1,
	SDIO_DAT2,
	SDIO_DAT3,
} pin_name_t;

#define PADMUX_CFG(grp_addr_base, index, pin_func)          \
	{                                                       \
		uint32_t val;                                       \
		val = readl(PADMUX_REG_ADDR(grp_addr_base, index)); \
		val &= ~PADMUX_REG_MASK(index);                     \
		val |= pin_func << PADMUX_REG_BIT_POS(index);       \
		writel(val, PADMUX_REG_ADDR(grp_addr_base, index)); \
	}

#define PADCFG_PULL(grp_addr_base, index, bpull, bpullup)                    \
	{                                                                        \
		uint32_t val;                                                        \
		val = readl(PADCFG_REG_ADDR(grp_addr_base, index));                  \
		val &= ~PADCFG_REG_PULL_MASK(index);                                 \
		val |= ((bpull << 4) | (bpullup << 5)) << PADCFG_REG_BIT_POS(index); \
		writel(val, PADCFG_REG_ADDR(grp_addr_base, index));                  \
	}

#define PADCFG_SLEW(grp_addr_base, index, bfast)            \
	{                                                       \
		uint32_t val;                                       \
		val = readl(PADCFG_REG_ADDR(grp_addr_base, index)); \
		val &= ~PADCFG_REG_SLEW_MASK(index);                \
		val |= (bfast << 8) << PADCFG_REG_BIT_POS(index);   \
		writel(val, PADCFG_REG_ADDR(grp_addr_base, index)); \
	}

#define PADCFG_DRV(grp_addr_base, index, strength)          \
	{                                                       \
		uint32_t val;                                       \
		val = readl(PADCFG_REG_ADDR(grp_addr_base, index)); \
		val &= ~PADCFG_REG_DRV_MASK(index);                 \
		val |= (strength) << PADCFG_REG_BIT_POS(index);     \
		writel(val, PADCFG_REG_ADDR(grp_addr_base, index)); \
	}

struct pinmux_addr_t {
	uint32_t pin_grp_id;
	uint32_t *pin_grp_cfg_base;
	uint32_t *pin_grp_mux_base;
};

#define PIN_GRP_ADDR_DEF(grp_idx, cfg_base, mux_base) \
	{                                                 \
		.pin_grp_id = grp_idx,                        \
		.pin_grp_cfg_base = (uint32_t *)cfg_base,     \
		.pin_grp_mux_base = (uint32_t *)mux_base,     \
	}

struct pinmux_addr_t g_soc_pin_grp_addr[] = {
	PIN_GRP_ADDR_DEF(SOC_PIN_AON, AON_AON_PADCTRL_BADDR, (AON_AON_PADCTRL_BADDR + 0x400)),
	PIN_GRP_ADDR_DEF(SOC_PIN_PERI1, AP_PERI1_PADCTRL_BADDR, (AP_PERI1_PADCTRL_BADDR + 0x400)),
	PIN_GRP_ADDR_DEF(SOC_PIN_PERI2, AP_PERI2_PADCTRL_BADDR, (AP_PERI2_PADCTRL_BADDR + 0x400)),
	PIN_GRP_ADDR_DEF(SOC_PIN_PERI3, AP_PERI3_PADCTRL_BADDR, (AP_PERI3_PADCTRL_BADDR + 0x200)),
};

static int gpio_pinmx_get_cfg_base(pin_name_t pin_name, uint32_t **cfg_base)
{
	uint32_t grp_num = ARRAY_SIZE(g_soc_pin_grp_addr);
	uint32_t grp_idx = PAD_GRP_IDX_GET(pin_name);
	uint32_t i;

	for (i = 0; i < grp_num; i++) {
		if (grp_idx == g_soc_pin_grp_addr[i].pin_grp_id)
			break;
	}

	if (i == grp_num)
		return -1;

	*cfg_base = g_soc_pin_grp_addr[i].pin_grp_cfg_base;
	return 0;
}

static int gpio_pinmx_get_mux_base(pin_name_t pin_name, uint32_t **mux_base)
{
	uint32_t grp_num = ARRAY_SIZE(g_soc_pin_grp_addr);
	uint32_t grp_idx = PAD_GRP_IDX_GET(pin_name);
	uint32_t i;

	for (i = 0; i < grp_num; i++) {
		if (grp_idx == g_soc_pin_grp_addr[i].pin_grp_id)
			break;
	}

	if (i == grp_num)
		return -1;

	*mux_base = g_soc_pin_grp_addr[i].pin_grp_mux_base;
	return 0;
}

static inline int gpio_pin_pllmode(pin_name_t pin_name, uint32_t mode)
{
	uint32_t bpull, bpullup;
	uint32_t *reg_addr;
	int ret;

	ret = gpio_pinmx_get_cfg_base(pin_name, &reg_addr);
	if (ret)
		return ret;

	switch (mode) {
	case PIN_PD:
		bpull = 1;
		bpullup = 0;
		/* code */
		break;
	case PIN_PU:
		bpull = 1;
		bpullup = 1;
		/* code */
		break;
	case PIN_PN:
		bpull = 0;
		bpullup = 0;
		/* code */
		break;
	default:
		return -1;
		break;
	}
	PADCFG_PULL(reg_addr, PAD_INDEX(pin_name), bpull, bpullup);

	return 0;
}

/**
  \brief       set pin speed
  \param[in]   pin_name pin name, defined in soc.h.
  \param[in]   speed    io speed
  \return      error code
*/
static inline int gpio_pin_speed(pin_name_t pin_name, uint32_t speed)
{
	uint32_t bfast;
	uint32_t *reg_addr;
	int ret;

	ret = gpio_pinmx_get_cfg_base(pin_name, &reg_addr);
	if (ret)
		return ret;

	switch (speed)
	{
	case PIN_SPEED_NORMAL:
		bfast = 0;
		break;
	case PIN_SPEED_FAST:
		bfast = 1;
		break;
	default:
		return -1;
	}

	PADCFG_SLEW(reg_addr, PAD_INDEX(pin_name), bfast);
	return 0;
}

/**
  \brief       set pin drive
  \param[in]   pin_name pin name, defined in soc.h.
  \param[in]   drive    io drive
  \return      error code
*/
static inline int gpio_pin_drv_strength(pin_name_t pin_name, uint32_t strength)
{
	uint32_t *reg_addr;
	int ret;

	ret = gpio_pinmx_get_cfg_base(pin_name, &reg_addr);
	if (ret)
		return ret;

	if (strength > PIN_DRV_MAX) {
		return -1;
	}

	PADCFG_DRV(reg_addr, PAD_INDEX(pin_name), strength);
	return 0;
}

int gpio_pin_mux(pin_name_t pin_name, uint32_t pin_func)
{
	uint32_t *reg_addr;
	int ret;

	ret = gpio_pinmx_get_mux_base(pin_name, &reg_addr);
	if (ret)
		return ret;

	PADMUX_CFG(reg_addr, PAD_INDEX(pin_name), pin_func);
	return 0;
}

int gpio_pin_cfg(pin_name_t pin_name, uint32_t slew_rate, uint32_t pullmode, uint32_t drvstrength)
{
	int ret;

	ret = gpio_pin_speed(pin_name, slew_rate);
	if (ret)
		return ret;
	ret = gpio_pin_pllmode(pin_name, pullmode);
	if (ret)
		return ret;
	ret = gpio_pin_drv_strength(pin_name, drvstrength);

	return ret;
}

int gpio_pin_output(const char *name, int value)
{
	int ret;
	unsigned gpio;

	ret = gpio_lookup_name(name, NULL, NULL, &gpio);
	if (ret)
		return ret;

	ret = gpio_request(gpio, "pinctrl");
	if (ret)
		return ret;

	ret = gpio_direction_output(gpio, value);
	if (ret)
		return ret;

	gpio_free(gpio);

	return 0;
}

static int gmac_phy_rst(const char *str_gpio)
{
	unsigned int gpio;

	if (!str_gpio)
		return -1;

	int ret = gpio_lookup_name(str_gpio, NULL, NULL, &gpio);
	if (!ret) {
		ret = gpio_request(gpio, "phy_rst");
		if (!ret) {
			gpio_direction_output(gpio, 0);
			/* At least 10ms in databook 6.5 Reset */
			mdelay(50);
			gpio_direction_output(gpio, 1);
			gpio_free(gpio);
			return 0;
		}
	}

	return ret;
}

int uboot_gpio_pin_init(const char *board_name)
{
	// Common IO pamdmux
	// uart4
	gpio_pin_mux(GPIO2_0, 1);
	gpio_pin_mux(GPIO2_1, 1);
	gpio_pin_cfg(GPIO2_0, PIN_SPEED_NORMAL, PIN_PN, 0x2);
	gpio_pin_cfg(GPIO2_1, PIN_SPEED_NORMAL, PIN_PN, 0x2);
	// gmac0
	gpio_pin_mux(GPIO0_0, 1);
	gpio_pin_mux(GPIO0_1, 1);
	gpio_pin_mux(GPIO0_2, 1);
	gpio_pin_mux(GPIO0_3, 1);
	gpio_pin_mux(GPIO0_4, 1);
	gpio_pin_mux(GPIO0_5, 1);
	gpio_pin_mux(GPIO0_6, 1);
	gpio_pin_mux(GPIO0_7, 1);
	gpio_pin_mux(GPIO0_8, 1);
	gpio_pin_mux(GPIO0_9, 1);
	gpio_pin_mux(GPIO0_10, 1);
	gpio_pin_mux(GPIO0_11, 1);
	gpio_pin_mux(GPIO0_12, 1);
	gpio_pin_mux(GPIO0_13, 1);
	gpio_pin_cfg(GPIO0_0, PIN_SPEED_NORMAL, PIN_PN, 0x8);
	gpio_pin_cfg(GPIO0_1, PIN_SPEED_NORMAL, PIN_PN, 0x8);
	gpio_pin_cfg(GPIO0_2, PIN_SPEED_NORMAL, PIN_PN, 0xC);
	gpio_pin_cfg(GPIO0_3, PIN_SPEED_NORMAL, PIN_PN, 0xC);
	gpio_pin_cfg(GPIO0_4, PIN_SPEED_NORMAL, PIN_PN, 0xC);
	gpio_pin_cfg(GPIO0_5, PIN_SPEED_NORMAL, PIN_PN, 0xC);
	gpio_pin_cfg(GPIO0_6, PIN_SPEED_NORMAL, PIN_PN, 0xC);
	gpio_pin_cfg(GPIO0_7, PIN_SPEED_NORMAL, PIN_PN, 0x8);
	gpio_pin_cfg(GPIO0_8, PIN_SPEED_NORMAL, PIN_PN, 0x8);
	gpio_pin_cfg(GPIO0_9, PIN_SPEED_NORMAL, PIN_PN, 0x8);
	gpio_pin_cfg(GPIO0_10, PIN_SPEED_NORMAL, PIN_PN, 0x8);
	gpio_pin_cfg(GPIO0_11, PIN_SPEED_NORMAL, PIN_PN, 0x8);
	gpio_pin_cfg(GPIO0_12, PIN_SPEED_NORMAL, PIN_PN, 0x8);
	gpio_pin_cfg(GPIO0_13, PIN_SPEED_NORMAL, PIN_PN, 0x8);
	// qspi0-0
	gpio_pin_mux(GPIO0_18, 1);
	gpio_pin_mux(GPIO0_19, 0); // cs0 gpio
	gpio_pin_mux(GPIO0_20, 1);
	gpio_pin_mux(GPIO0_21, 1);
	gpio_pin_mux(GPIO0_22, 1);
	gpio_pin_mux(GPIO0_23, 1);
	gpio_pin_cfg(GPIO0_18, PIN_SPEED_NORMAL, PIN_PN, 0x8);
	gpio_pin_cfg(GPIO0_19, PIN_SPEED_NORMAL, PIN_PN, 0x8);
	gpio_pin_cfg(GPIO0_20, PIN_SPEED_NORMAL, PIN_PN, 0x8);
	gpio_pin_cfg(GPIO0_21, PIN_SPEED_NORMAL, PIN_PN, 0x8);
	gpio_pin_cfg(GPIO0_22, PIN_SPEED_NORMAL, PIN_PN, 0x8);
	gpio_pin_cfg(GPIO0_23, PIN_SPEED_NORMAL, PIN_PN, 0x8);
	// i2c4-2
	gpio_pin_mux(GPIO2_26, 1);
	gpio_pin_mux(GPIO2_27, 1);
	gpio_pin_cfg(GPIO2_26, PIN_SPEED_NORMAL, PIN_PN, 0x4);
	gpio_pin_cfg(GPIO2_27, PIN_SPEED_NORMAL, PIN_PN, 0x4);
	// i2c7-0
	gpio_pin_mux(GPIO2_10, 5);
	gpio_pin_mux(GPIO2_11, 5);
	gpio_pin_cfg(GPIO2_10, PIN_SPEED_NORMAL, PIN_PN, 0x4);
	gpio_pin_cfg(GPIO2_11, PIN_SPEED_NORMAL, PIN_PN, 0x4);

	if (strcmp("a210-dev", board_name) == 0) {
		gmac_phy_rst("ao_gpio@0_24");	// PHY1_nRST
		gmac_phy_rst("ao_gpio@0_25");	// PHY0_nRST

		// gmac1
		gpio_pin_mux(GPIO1_2, 1);
		gpio_pin_mux(GPIO1_3, 1);
		gpio_pin_mux(GPIO1_4, 1);
		gpio_pin_mux(GPIO1_5, 1);
		gpio_pin_mux(GPIO1_6, 1);
		gpio_pin_mux(GPIO1_7, 1);
		gpio_pin_mux(GPIO1_8, 1);
		gpio_pin_mux(GPIO1_9, 1);
		gpio_pin_mux(GPIO1_10, 1);
		gpio_pin_mux(GPIO1_11, 1);
		gpio_pin_mux(GPIO1_12, 1);
		gpio_pin_mux(GPIO1_13, 1);
		gpio_pin_mux(GPIO1_14, 1);
		gpio_pin_mux(GPIO1_15, 1);
		gpio_pin_cfg(GPIO1_2, PIN_SPEED_NORMAL, PIN_PN, 0x8);
		gpio_pin_cfg(GPIO1_3, PIN_SPEED_NORMAL, PIN_PN, 0x8);
		gpio_pin_cfg(GPIO1_4, PIN_SPEED_NORMAL, PIN_PN, 0xC);
		gpio_pin_cfg(GPIO1_5, PIN_SPEED_NORMAL, PIN_PN, 0xC);
		gpio_pin_cfg(GPIO1_6, PIN_SPEED_NORMAL, PIN_PN, 0xC);
		gpio_pin_cfg(GPIO1_7, PIN_SPEED_NORMAL, PIN_PN, 0xC);
		gpio_pin_cfg(GPIO1_8, PIN_SPEED_NORMAL, PIN_PN, 0xC);
		gpio_pin_cfg(GPIO1_9, PIN_SPEED_NORMAL, PIN_PN, 0x8);
		gpio_pin_cfg(GPIO1_10, PIN_SPEED_NORMAL, PIN_PN, 0x8);
		gpio_pin_cfg(GPIO1_11, PIN_SPEED_NORMAL, PIN_PN, 0x8);
		gpio_pin_cfg(GPIO1_12, PIN_SPEED_NORMAL, PIN_PN, 0x8);
		gpio_pin_cfg(GPIO1_13, PIN_SPEED_NORMAL, PIN_PN, 0x8);
		gpio_pin_cfg(GPIO1_14, PIN_SPEED_NORMAL, PIN_PN, 0x8);
		gpio_pin_cfg(GPIO1_15, PIN_SPEED_NORMAL, PIN_PN, 0x8);

		// i2c5-0
		gpio_pin_mux(GPIO2_28, 1);
		gpio_pin_mux(GPIO2_29, 1);
		gpio_pin_cfg(GPIO2_28, PIN_SPEED_NORMAL, PIN_PN, 0x4);
		gpio_pin_cfg(GPIO2_29, PIN_SPEED_NORMAL, PIN_PN, 0x4);

		// i2c6-0
		gpio_pin_mux(GPIO2_8, 5);
		gpio_pin_mux(GPIO2_9, 5);
		gpio_pin_cfg(GPIO2_8, PIN_SPEED_NORMAL, PIN_PN, 0x4);
		gpio_pin_cfg(GPIO2_9, PIN_SPEED_NORMAL, PIN_PN, 0x4);

		// cfg bootsel0 to gpio
		gpio_pin_mux(BOOT_SEL0, 3);

		// POWER_3V3_EN
		gpio_pin_output("ao_gpio@0_26", 1);
		// POWER_5V_EN
		gpio_pin_output("ao_gpio@0_29", 1);
		// USBtypeC_PWREN
		gpio_pin_output("gpio@0_27", 1);
		// SOM2: Fan power SOM1: pci-e device reset
		gpio_pin_output("gpio@0_30", 1);
	} else if (strcmp("a210-evb-d2d", board_name) == 0) {
		// PHY0_nRST
		gmac_phy_rst("gpio@1_15");

		// pci-e device reset
		gpio_pin_output("gpio@0_30", 1);
	} else if (strcmp("a210-evb", board_name) == 0) {
		// PHY0_nRST
		gmac_phy_rst("ao_gpio@1_5");
		// PHY1_nRST
		gmac_phy_rst("ao_gpio@1_6");

		// chip debug
		gpio_pin_mux(GPIO1_6, 4);
		gpio_pin_mux(GPIO1_7, 4);
		gpio_pin_mux(GPIO1_8, 4);
		gpio_pin_cfg(GPIO1_6, PIN_SPEED_NORMAL, PIN_PN, 0x4);
		gpio_pin_cfg(GPIO1_7, PIN_SPEED_NORMAL, PIN_PN, 0x4);
		gpio_pin_cfg(GPIO1_8, PIN_SPEED_NORMAL, PIN_PN, 0x4);
		// spi0-0
		gpio_pin_mux(GPIO0_28, 2);
		gpio_pin_mux(GPIO0_29, 2);
		gpio_pin_mux(GPIO0_30, 0); // cs0 gpio
		gpio_pin_mux(GPIO0_31, 0); // cs1 gpio
		gpio_pin_mux(GPIO1_1, 6);
		gpio_pin_cfg(GPIO0_28, PIN_SPEED_NORMAL, PIN_PN, 0x8);
		gpio_pin_cfg(GPIO0_29, PIN_SPEED_NORMAL, PIN_PN, 0x8);
		gpio_pin_cfg(GPIO0_30, PIN_SPEED_NORMAL, PIN_PN, 0x8);
		gpio_pin_cfg(GPIO0_31, PIN_SPEED_NORMAL, PIN_PN, 0x8);
		gpio_pin_cfg(GPIO1_1, PIN_SPEED_NORMAL, PIN_PN, 0x8);

		// spi1-1
		gpio_pin_mux(GPIO2_17, 2);
		gpio_pin_mux(GPIO2_18, 0); // cs0 gpio
		gpio_pin_mux(GPIO2_19, 0); // cs1 gpio
		gpio_pin_mux(GPIO2_21, 2);
		gpio_pin_mux(GPIO2_22, 2);
		gpio_pin_cfg(GPIO2_17, PIN_SPEED_NORMAL, PIN_PN, 0x8);
		gpio_pin_cfg(GPIO2_18, PIN_SPEED_NORMAL, PIN_PN, 0x8);
		gpio_pin_cfg(GPIO2_19, PIN_SPEED_NORMAL, PIN_PN, 0x8);
		gpio_pin_cfg(GPIO2_21, PIN_SPEED_NORMAL, PIN_PN, 0x8);
		gpio_pin_cfg(GPIO2_22, PIN_SPEED_NORMAL, PIN_PN, 0x8);

		// qspi1-1
		gpio_pin_mux(GPIO2_29, 0); // cs0 gpio
		gpio_pin_mux(GPIO3_2, 1);
		gpio_pin_mux(GPIO3_5, 1);
		gpio_pin_mux(GPIO3_6, 1);
		gpio_pin_mux(GPIO3_7, 1);
		gpio_pin_mux(GPIO3_8, 1);
		gpio_pin_cfg(GPIO2_29, PIN_SPEED_NORMAL, PIN_PN, 0x8);
		gpio_pin_cfg(GPIO3_2, PIN_SPEED_NORMAL, PIN_PN, 0x8);
		gpio_pin_cfg(GPIO3_5, PIN_SPEED_NORMAL, PIN_PN, 0x8);
		gpio_pin_cfg(GPIO3_6, PIN_SPEED_NORMAL, PIN_PN, 0x8);
		gpio_pin_cfg(GPIO3_7, PIN_SPEED_NORMAL, PIN_PN, 0x8);
		gpio_pin_cfg(GPIO3_8, PIN_SPEED_NORMAL, PIN_PN, 0x8);

		// i2c0-1
		gpio_pin_mux(GPIO0_24, 2);
		gpio_pin_mux(GPIO0_25, 2);
		gpio_pin_cfg(GPIO0_24, PIN_SPEED_NORMAL, PIN_PN, 0x4);
		gpio_pin_cfg(GPIO0_25, PIN_SPEED_NORMAL, PIN_PN, 0x4);
		// i2c1-1
		gpio_pin_mux(GPIO0_26, 2);
		gpio_pin_mux(GPIO0_27, 2);
		gpio_pin_cfg(GPIO0_26, PIN_SPEED_NORMAL, PIN_PN, 0x4);
		gpio_pin_cfg(GPIO0_27, PIN_SPEED_NORMAL, PIN_PN, 0x4);
		// i2c2-0: Conflict with qspi0
		// gpio_pin_mux(GPIO0_22, 2);
		// gpio_pin_mux(GPIO0_23, 2);
		// gpio_pin_cfg(GPIO0_22, PIN_SPEED_NORMAL, PIN_PN, 0x4);
		// gpio_pin_cfg(GPIO0_23, PIN_SPEED_NORMAL, PIN_PN, 0x4);
		// i2c3-2
		gpio_pin_mux(GPIO2_24, 1);
		gpio_pin_mux(GPIO2_25, 1);
		gpio_pin_cfg(GPIO2_24, PIN_SPEED_NORMAL, PIN_PN, 0x4);
		gpio_pin_cfg(GPIO2_25, PIN_SPEED_NORMAL, PIN_PN, 0x4);
		// i2c5-0
		gpio_pin_mux(GPIO2_2, 5);
		gpio_pin_mux(GPIO2_3, 5);
		gpio_pin_cfg(GPIO2_2, PIN_SPEED_NORMAL, PIN_PN, 0x4);
		gpio_pin_cfg(GPIO2_3, PIN_SPEED_NORMAL, PIN_PN, 0x4);
		// i2c6-0
		gpio_pin_mux(GPIO2_4, 5);
		gpio_pin_mux(GPIO2_5, 5);
		gpio_pin_cfg(GPIO2_4, PIN_SPEED_NORMAL, PIN_PN, 0x4);
		gpio_pin_cfg(GPIO2_5, PIN_SPEED_NORMAL, PIN_PN, 0x4);
	} else {
		printf("Unknown board name %s\n", board_name);
		while(1);
		return -1;
	}
	return 0;
}
