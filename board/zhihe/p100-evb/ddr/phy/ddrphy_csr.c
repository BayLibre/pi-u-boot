// SPDX-License-Identifier: GPL-2.0+
/*
 * (C) Copyright 2024 Zhihe Computing Technology (Shenzhen) Co., Ltd.
 */

#include <string.h>
#include "../../include/utils/utils.h"
#include "../ddr_init.h"

//phy csr wr
void ddr_phy_reg_wr(unsigned long int addr, unsigned int wr_data)
{
    addr <<= 1;
    wr16(DDR_PHY_CH0 + addr, wr_data);
    wr16(DDR_PHY_CH1 + addr, wr_data);
}

void ddr_phy0_reg_wr(unsigned long int addr, unsigned int wr_data)
{
    addr <<= 1;
    wr16(DDR_PHY_CH0 + addr, wr_data);
}

void ddr_phy1_reg_wr(unsigned long int addr, unsigned int wr_data)
{
    addr <<= 1;
    wr16(DDR_PHY_CH1 + addr, wr_data);
}

//phy csr rd
unsigned int ddr_phy_reg_rd(unsigned long int addr)
{
    unsigned int rd_data;
    addr <<= 1;
    rd_data = rd16(DDR_PHY_CH0 + addr);
    return rd_data;
}

unsigned int ddr_phy0_reg_rd(unsigned long int addr)
{
    unsigned int rd_data;
    addr <<= 1;
    rd_data = rd16(DDR_PHY_CH0 + addr);
    return rd_data;
}

unsigned int ddr_phy1_reg_rd(unsigned long int addr)
{
    unsigned int rd_data;
    addr <<= 1;
    rd_data = rd16(DDR_PHY_CH1 + addr);
    return rd_data;
}

void ddr_phys_reg_wr(unsigned char ch, unsigned long int addr, unsigned int wr_data)
{
    addr <<= 1;
    if (ch == 0)
        wr16(DDR_PHY_CH0 + addr, wr_data);
    else if (ch == 1)
        wr16(DDR_PHY_CH1 + addr, wr_data);
}

unsigned short ddr_phys_reg_rd(unsigned char ch, unsigned long int addr)
{
    unsigned short rdata;
    addr <<= 1;
    if (ch == 0)
        rdata = rd16(DDR_PHY_CH0 + addr);
    else if (ch == 1)
        rdata = rd16(DDR_PHY_CH1 + addr);
    else
        rdata = 0;
    return rdata;
}
