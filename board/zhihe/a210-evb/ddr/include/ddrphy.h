#ifndef __DDRPHY_API_H
#define __DDRPHY_API_H

void ddr_phy_reg_wr(unsigned long int addr, unsigned int wr_data);
unsigned int ddr_phy_reg_rd(unsigned long int addr);
void ddr_phys_reg_wr(unsigned char ch, unsigned long int addr, unsigned int wr_data);
unsigned short ddr_phys_reg_rd(unsigned char ch, unsigned long int addr);

#endif
