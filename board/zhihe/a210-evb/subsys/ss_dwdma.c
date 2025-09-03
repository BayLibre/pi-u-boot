// SPDX-License-Identifier: GPL-2.0+
/*
 * (C) Copyright 2025 Zhihe Computing Technology (Shenzhen) Co., Ltd.
 */

#include <asm/io.h>
#include "../include/utils/utils.h"

#define PERI3_MSH_BASE 0x00540000
#define PERI3_MSH_BASE_DIE1 0x2000540000

#define PERI3_MSH_CLK_ENABLE 0x200
#define PERI3_MSH_RST_N 0x400

#define DMAC_ACLK_EN_POS 0x0
#define DMAC_ACLK_EN_MASK BIT(DMAC_ACLK_EN_POS)

#define DMAC_HCLK_EN_POS 0x1
#define DMAC_HCLK_EN_MASK BIT(DMAC_HCLK_EN_POS)

#define SW_DMAC_ARESETN_POS 0x0
#define SW_DMAC_ARESETN_MASK BIT(SW_DMAC_ARESETN_POS)

#define SW_DMAC_HRESETN_POS 0x1
#define SW_DMAC_HRESETN_MASK BIT(SW_DMAC_HRESETN_POS)

#define PERI0_SYSREG 0x00300000
#define PERI0_CLK_EN 0x200
#define PERI0_RST_N 0x400
#define TIMER0_CCLK_EN_POS 0
#define TIMER0_CCLK_EN_EN BIT(TIMER0_CCLK_EN_POS)
#define TIMER0_PCLK_EN_POS 1
#define TIMER0_PCLK_EN_EN BIT(TIMER0_PCLK_EN_POS)
#define TIMER1_CCLK_EN_POS 2
#define TIMER1_CCLK_EN_EN BIT(TIMER1_CCLK_EN_POS)
#define TIMER1_PCLK_EN_POS 3
#define TIMER1_PCLK_EN_EN BIT(TIMER1_PCLK_EN_POS)
#define TIMER0_CRST_N_POS 0
#define TIMER0_CRST_N_MASK BIT(TIMER0_CRST_N_POS)
#define TIMER0_PRST_N_POS 1
#define TIMER0_PRST_N_MASK BIT(TIMER0_PRST_N_POS)
#define TIMER1_CRST_N_POS 2
#define TIMER1_CRST_N_MASK BIT(TIMER1_CRST_N_POS)
#define TIMER1_PRST_N_POS 3
#define TIMER1_PRST_N_MASK BIT(TIMER1_PRST_N_POS)

static void dw_axi_dma_hw_init(volatile void __iomem *base)
{
    u32 val;

    /* Enable clock. */
    val = readl(base + PERI3_MSH_CLK_ENABLE);
    val |= DMAC_ACLK_EN_MASK | DMAC_HCLK_EN_MASK;
    writel(val, base + PERI3_MSH_CLK_ENABLE);

    /* reset. */
    val = readl(base + PERI3_MSH_RST_N);
    val |= SW_DMAC_ARESETN_MASK | SW_DMAC_HRESETN_MASK;
    writel(val, base + PERI3_MSH_RST_N);

    return;
}

static void dw_apb_timer_hw_init(volatile void __iomem *base)
{
    u32 val;

    /* Enable timer0 and timer1 clock. */
    val = readl(base + PERI0_CLK_EN);
    val |= TIMER0_CCLK_EN_EN | TIMER0_PCLK_EN_EN | TIMER1_CCLK_EN_EN | TIMER1_PCLK_EN_EN;
    writel(val, base + PERI0_CLK_EN);

    /* Reset timer0 and timer1. */
    val = readl(base + PERI0_RST_N);
    val &= ~(TIMER0_CRST_N_MASK | TIMER0_PRST_N_MASK | TIMER1_CRST_N_MASK | TIMER1_PRST_N_MASK);
    writel(val, base + PERI0_RST_N);
    val |= TIMER0_CRST_N_MASK | TIMER0_PRST_N_MASK | TIMER1_CRST_N_MASK | TIMER1_PRST_N_MASK;
    writel(val, base + PERI0_RST_N);

    return;
}

void ss_dwdma_config(void)
{
    dw_axi_dma_hw_init((volatile void __iomem *)PERI3_MSH_BASE);
#ifdef CONFIG_SOC_ZHIHE_D2D
    dw_axi_dma_hw_init((volatile void __iomem *)PERI3_MSH_BASE_DIE1);
#endif
    dw_apb_timer_hw_init((volatile void __iomem *)PERI0_SYSREG);
}
