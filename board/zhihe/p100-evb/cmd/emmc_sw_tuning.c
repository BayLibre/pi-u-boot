// SPDX-License-Identifier: GPL-2.0+
/*
 * (C) Copyright 2025 Zhihe Computing Technology (Shenzhen) Co., Ltd.
 */

#include <exports.h>
#include "../include/utils/utils.h"

#define EMMC_MSH8_BADDR 0x00500000
#define SD_MSH8_BADDR 0x00510000

static void hw_wr8(u64 addr, u8 data) {
    wr8(addr,data);
}

static u8 hw_rd8(u64 addr) {
    return rd8(addr);
}

static void hw_wr16(u64 addr, u16 data) {
    wr16(addr,data);
}

static u16 hw_rd16(u64 addr) {
    return rd16(addr);
}

static void hw_wr32(u64 addr, u32 data) {
    wr(addr,data);
}

static u32 hw_rd32(u64 addr) {
    return rd(addr);
}

static void hw_wait_ns(u64 ns) {
    ndelay(ns);
}

static void hw_emmc_cmd_send(u64 emmc_msh8_baddr, u32 arg, u16 cmd){
    u64 addr;
    int rdata = 0x0;

    printf("Send CMD%d \n",((cmd>>8) & 0x003f));
    u64 argument_r_offset = 0x8;
    u64 cmd_r_offset = 0xe;
    u64 normale_int_stat_r_offset = 0x30;
    u64 error_int_stat_r_offset = 0x32;

    //Set Argument
    addr = emmc_msh8_baddr + argument_r_offset ;                        
    hw_wr32(addr, arg);                                 
    //Set Command
    addr = emmc_msh8_baddr + cmd_r_offset ;                        
    hw_wr16(addr, cmd);                                 
    //Wait for command complete
    addr = emmc_msh8_baddr + normale_int_stat_r_offset ;                        
    rdata = hw_rd16(addr)     ;                                
    while(0x1 != (rdata & 0x1)){
        rdata = hw_rd16(addr)    ;                             
    } 
    hw_wr16(addr, 0x1);
    //Check CMD_TOUT_ERR
    addr = emmc_msh8_baddr + error_int_stat_r_offset ; 
    rdata = hw_rd16(addr)     ;                                
    if(0x1 == (rdata & 0x1)){
        printf("*****CMD_TOUT_ERR!!!\n");
        hw_wr16(addr, 0x1);
    } 

    //Check CMD_CRC_ERR
    addr = emmc_msh8_baddr + error_int_stat_r_offset ; 
    rdata = hw_rd16(addr)     ;                                
    if(0x2 == (rdata & 0x2)){
        printf("*****CMD_CRC_ERR!!!\n");
        hw_wr16(addr, 0x2);
    } 

    //Check CMD_END_BIT_ERR
    addr = emmc_msh8_baddr + error_int_stat_r_offset ; 
    rdata = hw_rd16(addr)     ;                                
    if(0x4 == (rdata & 0x4)){
        printf("*****CMD_END_BIT_ERR!!!\n");
        hw_wr16(addr, 0x4);
    } 

    //Check CMD_IDX_ERR
    addr = emmc_msh8_baddr + error_int_stat_r_offset ; 
    rdata = hw_rd16(addr)     ;                                
    if(0x8 == (rdata & 0x8)){
        printf("*****CMD_IDX_ERR!!!\n");
        hw_wr16(addr, 0x8);
    } 
}

static void hw_emmc_clk_change(u64 emmc_base_addr, u32 clk_divisor){
    u64 clk_ctrl_r = emmc_base_addr + 0x2c; //16b

    //Stop SD/eMMC clock
    u16 sd_clk_en = 0x0;
    u16 internal_clk_en = 0x0;
    u16 pll_enable = 0x0;
    u16 clk_ctrl_data = (pll_enable << 3) + (sd_clk_en << 2) + internal_clk_en;
    hw_wr16(clk_ctrl_r , clk_ctrl_data);           
    //Clock change
    clk_ctrl_data = hw_rd16(clk_ctrl_r);
    u16 freq_sel = 0x0;
    u16 upper_freq_sel = 0x0;
    if(clk_divisor <= 510){
        freq_sel = clk_divisor/2;
    }
    else{
        freq_sel = ((clk_divisor/2)&0xff);
        upper_freq_sel = (((clk_divisor/2)&0x300)>>8);
    }
    clk_ctrl_data = (clk_ctrl_data & 0x3F) + (freq_sel << 8) + (upper_freq_sel << 6);
    hw_wr16(clk_ctrl_r , clk_ctrl_data);                 
    //Enable SD/eMMC clock
    clk_ctrl_data = rd16(clk_ctrl_r);
    sd_clk_en = 0x1;
    internal_clk_en = 0x1;
    clk_ctrl_data = clk_ctrl_data | (sd_clk_en << 2) | (internal_clk_en);
    wr16(clk_ctrl_r , clk_ctrl_data);           
    //Check Clock Stability
    u16 rdata = rd16(clk_ctrl_r)     ;
    while(0x2 != (rdata & 0x2)){
        rdata = rd16(clk_ctrl_r)    ;                             
    } 
    //Enable SD/eMMC clock
    clk_ctrl_data = rd16(clk_ctrl_r);
    pll_enable = 0x1;
    clk_ctrl_data = clk_ctrl_data | (pll_enable << 3);
    wr16(clk_ctrl_r , clk_ctrl_data);           
    //Check Clock Stability
    rdata = rd16(clk_ctrl_r)     ;                                
    while(0x2 != (rdata & 0x2)){
        rdata = rd16(clk_ctrl_r)    ;                             
    } 
}

static void emmc_init_seq(void) {
    //init sequence variable
    u64 emmc_base_addr = EMMC_MSH8_BADDR; //need change
    u64 pwr_ctrl_r = emmc_base_addr + 0x29; //8b
    u64 host_ctrl2_r = emmc_base_addr + 0x3e; //16b

    u64 p_vendor_specific_area = emmc_base_addr + 0xe8; //16b
    u16 vendor_point;
    vendor_point = hw_rd16(p_vendor_specific_area);

    u64 emmc_ctrl_r = emmc_base_addr + vendor_point + 0x2c; //16b
    u64 phy_regs_offset = 0x300;
    u64 phy_cnfg = emmc_base_addr + phy_regs_offset; //32b
    u64 cmdpad_cnfg = emmc_base_addr + phy_regs_offset + 0x4; //16b
    u64 datpad_cnfg = emmc_base_addr + phy_regs_offset + 0x6; //16b
    u64 clkpad_cnfg = emmc_base_addr + phy_regs_offset + 0x8; //16b
    u64 stbpad_cnfg = emmc_base_addr + phy_regs_offset + 0xA; //16b
    u64 rstnpad_cnfg = emmc_base_addr + phy_regs_offset + 0xc; //16b
    u64 dll_ctrl = emmc_base_addr + phy_regs_offset + 0x24; //8b
    u64 dll_cnfg1 = emmc_base_addr + phy_regs_offset + 0x25; //8b

    u64 clk_ctrl_r = emmc_base_addr + 0x2c; //16b
    u64 sdclkdl_cnfg = emmc_base_addr + phy_regs_offset + 0x1d; //8b
    u64 sdclkdl_dc = emmc_base_addr + phy_regs_offset + 0x1e; //8b

    //init seq
    hw_wr8(pwr_ctrl_r, 0xd); //enable power
    hw_wr16(host_ctrl2_r, 0x7c08); //enable pemmc interface
    hw_wr16(emmc_ctrl_r, 0x1); //set emmc mode
    hw_wr16(clk_ctrl_r, 0x0); //disable clk

    //set pad setting
    u32 pad_sn = 0x8;
    u32 pad_sp = 0x8;
    u32 phy_rstn = 0x1;
    u32 phy_cnfg_data = (pad_sn << 20) + (pad_sp << 16) + phy_rstn;
    hw_wr32(phy_cnfg, phy_cnfg_data); //set phy commen
    u32 txslew_ctrl_n = 0x3;
    u32 txslew_ctrl_p = 0x0;
    u32 weakpull_en = 0x1;
    u32 rxsel = 0x1;
    u32 cmdpad_cnfg_data = (txslew_ctrl_n << 9) + (txslew_ctrl_p << 5) + (weakpull_en << 3) + rxsel;
    hw_wr16(cmdpad_cnfg, cmdpad_cnfg_data);
    u32 datpad_cnfg_data = (txslew_ctrl_n << 9) + (txslew_ctrl_p << 5) + (weakpull_en << 3) + rxsel;
    hw_wr16(datpad_cnfg, datpad_cnfg_data);
    u32 rstnpad_cnfg_data = (txslew_ctrl_n << 9) + (txslew_ctrl_p << 5) + (weakpull_en << 3) + rxsel;
    hw_wr16(rstnpad_cnfg, rstnpad_cnfg_data);
    weakpull_en = 0x0;
    rxsel = 0x0;
    u32 clkpad_cnfg_data = (txslew_ctrl_n << 9) + (txslew_ctrl_p << 5) + (weakpull_en << 3) + rxsel;
    hw_wr16(clkpad_cnfg, clkpad_cnfg_data);
    weakpull_en = 0x2;
    rxsel = 0x1;
    u32 stbpad_cnfg_data = (txslew_ctrl_n << 9) + (txslew_ctrl_p << 5) + (weakpull_en << 3) + rxsel;
    hw_wr16(stbpad_cnfg, stbpad_cnfg_data);

    //init interrupt
    u64 normal_int_stat_en_r = emmc_base_addr + 0x34;
    u64 error_int_stat_en_r = emmc_base_addr + 0x36;
    hw_wr16(normal_int_stat_en_r, 0xffff);
    hw_wr16(error_int_stat_en_r, 0xffff);

    //init dll
    hw_wr8(dll_ctrl, 0x1);
    hw_wr8(dll_cnfg1, 0x5);

    //init tx delay line code
    u32 update_dc = 0x1;
    u32 inpsel_cnfg = 0x0;
    u32 bypass_en = 0x0;
    u32 extdly_en = 0x0;
    u32 sdclkdl_cnfg_data = (update_dc << 4) + (inpsel_cnfg << 2) + (bypass_en << 1) + extdly_en;
    hw_wr8(sdclkdl_cnfg, sdclkdl_cnfg_data); //set delay line update en, disable bypass and extdly
    hw_wr8(sdclkdl_dc, 0x40); //set tx delay line to center
    update_dc = 0x0;
    sdclkdl_cnfg_data = (update_dc << 4) + (inpsel_cnfg << 2) + (bypass_en << 1) + extdly_en;
    hw_wr8(sdclkdl_cnfg, sdclkdl_cnfg_data); //set delay line update finish, disable bypass and extdly

    //init card
    hw_emmc_clk_change(emmc_base_addr, 520);
    // cmd0
    u16 cmd0 = 0x0;
    hw_emmc_cmd_send(emmc_base_addr, 0x0, cmd0);
    // cmd1
    u16 cmd1 = 0x102;
    hw_emmc_cmd_send(emmc_base_addr, 0xc0ff8080, cmd1);
    u64 resp01_r = emmc_base_addr + 0x10;
    u32 resp_cmd1 = hw_rd32(resp01_r);
    while(0x80000000 != (resp_cmd1 & 0x80000000)){
        hw_emmc_cmd_send(emmc_base_addr, 0xc0ff8080, cmd1);
        resp_cmd1 = hw_rd32(resp01_r);
    }
    // cmd2
    u16 cmd2 = 0x209;
    hw_emmc_cmd_send(emmc_base_addr, 0x0, cmd2);
    u32 rca = 0x00010000;
    // cmd3
    u16 cmd3 = 0x31A;
    hw_emmc_cmd_send(emmc_base_addr, rca, cmd3);
    u32 resp_cmd3 = hw_rd32(resp01_r);
    if(resp_cmd3 != 0x500){
        printf("*****Resp for CMD3 is %x.\n", resp_cmd3);
    }
    // cmd7
    u16 cmd7 = 0x71A;
    hw_emmc_cmd_send(emmc_base_addr, rca, cmd7);
    u32 resp_cmd7 = hw_rd32(resp01_r);
    if(resp_cmd7 != 0x700){
        printf("*****Resp for CMD7 is %x.\n", resp_cmd7);
    }

    //set bus width
    u64 host_ctrl1_r = emmc_base_addr + 0x28; //8bit
    hw_wr8(host_ctrl1_r, 0x20); //set 8bit bus width
    //cmd6 set bus width
    u16 cmd6 = 0x61B;
    hw_emmc_cmd_send(emmc_base_addr, 0x03b70200, cmd6);
    u32 resp_cmd6 = hw_rd32(resp01_r);
    if(resp_cmd6 != 0x900){
        printf("*****Resp for CMD6 is %x.\n", resp_cmd6);
    }

    //change emmc speed to hs200
    u16 host_ctrl2_data;
    host_ctrl2_data = hw_rd16(host_ctrl2_r);
    host_ctrl2_data = (host_ctrl2_data & 0xfff8) + 0x3;
    hw_wr16(host_ctrl2_r, host_ctrl2_data);
    //cmd6 set speed
    hw_emmc_cmd_send(emmc_base_addr, 0x03b90200, cmd6);
    resp_cmd6 = hw_rd32(resp01_r);
    if(resp_cmd6 != 0x900){
        printf("*****Resp for CMD6 is %x.\n", resp_cmd6);
    }

    hw_emmc_clk_change(emmc_base_addr, 0);

}

/*
emmc tuning main function
 */
int emmc_read_tuning_seq(void) {
    //init emmc card HS200 8bit bus finished
    emmc_init_seq();

    /******************************************************************/
    //init emmc variable
    /******************************************************************/
    u64 emmc_base_addr = EMMC_MSH8_BADDR; //need change
    u64 blocksize_r = emmc_base_addr + 0x4; //16b
    u64 xfer_mode_r = emmc_base_addr + 0xc; //16b
    u64 clk_ctrl_r = emmc_base_addr + 0x2c; //16b
    u64 host_ctrl2_r = emmc_base_addr + 0x3e; //16b
    u64 normal_int_stat_r = emmc_base_addr + 0x30; //16b
    u64 error_int_stat_r = emmc_base_addr + 0x32; //16b
    u64 sw_rst_r = emmc_base_addr + 0x2f; //8b

    u64 p_vendor_specific_area = emmc_base_addr + 0xe8; //16b
    u16 vendor_point;
    vendor_point = hw_rd16(p_vendor_specific_area);

    u64 at_ctrl_r = emmc_base_addr + vendor_point + 0x40; //32b
    u64 at_stat_r = emmc_base_addr + vendor_point + 0x44; //32b

    /******************************************************************/
    //start sw tuning seq
    /******************************************************************/
    //turn off SD clk out
    u16 clk_ctrl_r_data;
    clk_ctrl_r_data = hw_rd16(clk_ctrl_r);
    clk_ctrl_r_data = clk_ctrl_r_data & 0xfffb; //set SD_CLK_EN = 0
    hw_wr16(clk_ctrl_r, clk_ctrl_r_data);
    //reset runing engine
    u16 host_ctrl2_data;
    host_ctrl2_data = hw_rd16(host_ctrl2_r);
    host_ctrl2_data = host_ctrl2_data & 0xff7f; //set SAMPLE_CLK_SEL = 0
    hw_wr16(host_ctrl2_r, host_ctrl2_data);
    //init CMD21 block setting
    hw_wr16(blocksize_r,0x80);
    hw_wr16(xfer_mode_r,0x10);
    //enable software tuning
    u32 at_ctrl_data;
    at_ctrl_data = hw_rd32(at_ctrl_r);
    at_ctrl_data = at_ctrl_data | 0x10; //set SW_TUNE_EN = 1
    hw_wr32(at_ctrl_r, at_ctrl_data);
    //set read delay line to 0
    u8 read_dc = 0;
    u32 at_stat_data;
    at_stat_data = hw_rd32(at_stat_r);
    at_stat_data = at_stat_data & 0xffffff00;
    at_stat_data = at_stat_data + (read_dc & 0xff); //set CENTER_PH_CODE = 0
    hw_wr32(at_stat_data, at_stat_data);
    //turn on SD clk out
    clk_ctrl_r_data = hw_rd16(clk_ctrl_r);
    clk_ctrl_r_data = clk_ctrl_r_data | 0x4; //set SD_CLK_EN = 1
    hw_wr16(clk_ctrl_r, clk_ctrl_r_data);
    //tuning sequence
    u8 tuning_result[128];
    u16 normal_int_data;
    u16 error_int_data;
    u8 buf_rd_ready;
    for (read_dc=0; read_dc <= 127; read_dc++) {
        hw_emmc_cmd_send(emmc_base_addr, 0x0, 0x153a);
        while(1) {
            normal_int_data = hw_rd16(normal_int_stat_r);
            error_int_data = hw_rd16(error_int_stat_r);
            buf_rd_ready = normal_int_data & 0x20;
            error_int_data = error_int_data & 0x7f;
            if(buf_rd_ready) break;
            if(error_int_data) break;
        }
        error_int_data = hw_rd16(error_int_stat_r);
        error_int_data = error_int_data & 0x7f;
        if(error_int_data) {
            hw_wr16(error_int_stat_r,error_int_data);
            tuning_result[read_dc] = 0;
        } else {
            hw_wr16(normal_int_stat_r, 0x20);
            tuning_result[read_dc] = 1;
        }
        hw_wr8(sw_rst_r,0x6); //sw rst DAT and CMD
        while(1) {
            u8 sw_rst_data;
            sw_rst_data = hw_rd8(sw_rst_r);
            if (sw_rst_data == 0) break;
        }
    }
    //print result
    printf("Scan finish: \n");
    for (int i = 0; i <= 127; i++) {
        if (tuning_result[i]) {
            printf("P");
        } else {
            printf("X");
        }
    }
    printf("\n");
    return 0;
}


static void hw_sd_cmd_send(u64 sd_msh4_baddr, u32 arg, u16 cmd) {
    hw_emmc_cmd_send(sd_msh4_baddr, arg, cmd);
}

static void hw_sd_clk_change(u64 sd_base_addr, u32 clk_divisor) {
    hw_emmc_clk_change(sd_base_addr, clk_divisor);
}

static void sdio_init_seq(void) {
    //init sequence variable
    u64 sdio_base_addr = SD_MSH8_BADDR; //need change
    u64 pwr_ctrl_r = sdio_base_addr + 0x29; //8b
    u64 host_ctrl2_r = sdio_base_addr + 0x3e; //16b

    u64 p_vendor_specific_area = sdio_base_addr + 0xe8; //16b
    u16 vendor_point;
    vendor_point = hw_rd16(p_vendor_specific_area);

    u64 emmc_ctrl_r = sdio_base_addr + vendor_point + 0x2c; //16b
    u64 phy_regs_offset = 0x300;
    u64 phy_cnfg = sdio_base_addr + phy_regs_offset; //32b
    u64 cmdpad_cnfg = sdio_base_addr + phy_regs_offset + 0x4; //16b
    u64 datpad_cnfg = sdio_base_addr + phy_regs_offset + 0x6; //16b
    u64 clkpad_cnfg = sdio_base_addr + phy_regs_offset + 0x8; //16b
    u64 stbpad_cnfg = sdio_base_addr + phy_regs_offset + 0xA; //16b
    u64 rstnpad_cnfg = sdio_base_addr + phy_regs_offset + 0xc; //16b
    u64 dll_ctrl = sdio_base_addr + phy_regs_offset + 0x24; //8b
    u64 dll_cnfg1 = sdio_base_addr + phy_regs_offset + 0x25; //8b

    u64 clk_ctrl_r = sdio_base_addr + 0x2c; //16b
    u64 sdclkdl_cnfg = sdio_base_addr + phy_regs_offset + 0x1d; //8b
    u64 sdclkdl_dc = sdio_base_addr + phy_regs_offset + 0x1e; //8b

    //init seq
    //pad mux seq
    u64 pad_ctrl_baddr = 0x2026000;
    u64 pad_mux_addr = pad_ctrl_baddr + 0x410; //GPIO1_0 ALT5
    u32 pad_ctrl_data;
    pad_ctrl_data = hw_rd32(pad_mux_addr);
    pad_ctrl_data = (pad_ctrl_data & 0xFFFFFFF0) | 0x5;
    hw_wr32(pad_mux_addr, pad_ctrl_data);
    
    pad_mux_addr = pad_ctrl_baddr + 0x418; //GPIO1_16 ALT5
    pad_ctrl_data = hw_rd32(pad_mux_addr);
    pad_ctrl_data = (pad_ctrl_data & 0xFFFFFFF0) | 0x5;
    hw_wr32(pad_mux_addr, pad_ctrl_data);
    //enable sdio system clk
    u32 rdata;
    u64 peri3_sysreg_baddr = 0x540000;
    u64 sysreg_addr;
    sysreg_addr = peri3_sysreg_baddr + 0x200;
    rdata = hw_rd32(sysreg_addr);
    hw_wr32(sysreg_addr, rdata|0x1f00);

    sysreg_addr = peri3_sysreg_baddr + 0x400;
    rdata = hw_rd32(sysreg_addr);
    hw_wr32(sysreg_addr, rdata|0x1c0);

    hw_wr8(pwr_ctrl_r, 0xf); //enable power
    hw_wr16(host_ctrl2_r, 0x7c00); //enable pemmc interface
    hw_wr16(emmc_ctrl_r, 0x0); //set sdio mode
    hw_wr16(clk_ctrl_r, 0x0); //disable clk

    //set pad setting
    u32 pad_sn = 0x8;
    u32 pad_sp = 0x8;
    u32 phy_rstn = 0x1;
    u32 phy_cnfg_data = (pad_sn << 20) + (pad_sp << 16) + phy_rstn;
    hw_wr32(phy_cnfg, phy_cnfg_data); //set phy commen
    u32 txslew_ctrl_n = 0x3;
    u32 txslew_ctrl_p = 0x0;
    u32 weakpull_en = 0x1;
    u32 rxsel = 0x1;
    u32 cmdpad_cnfg_data = (txslew_ctrl_n << 9) + (txslew_ctrl_p << 5) + (weakpull_en << 3) + rxsel;
    hw_wr16(cmdpad_cnfg, cmdpad_cnfg_data);
    u32 datpad_cnfg_data = (txslew_ctrl_n << 9) + (txslew_ctrl_p << 5) + (weakpull_en << 3) + rxsel;
    hw_wr16(datpad_cnfg, datpad_cnfg_data);
    u32 rstnpad_cnfg_data = (txslew_ctrl_n << 9) + (txslew_ctrl_p << 5) + (weakpull_en << 3) + rxsel;
    hw_wr16(rstnpad_cnfg, rstnpad_cnfg_data);
    weakpull_en = 0x0;
    rxsel = 0x0;
    u32 clkpad_cnfg_data = (txslew_ctrl_n << 9) + (txslew_ctrl_p << 5) + (weakpull_en << 3) + rxsel;
    hw_wr16(clkpad_cnfg, clkpad_cnfg_data);
    weakpull_en = 0x2;
    rxsel = 0x1;
    u32 stbpad_cnfg_data = (txslew_ctrl_n << 9) + (txslew_ctrl_p << 5) + (weakpull_en << 3) + rxsel;
    hw_wr16(stbpad_cnfg, stbpad_cnfg_data);

    //init interrupt
    u64 normal_int_stat_en_r = sdio_base_addr + 0x34;
    u64 error_int_stat_en_r = sdio_base_addr + 0x36;
    hw_wr16(normal_int_stat_en_r, 0xffff);
    hw_wr16(error_int_stat_en_r, 0xffff);
    
    //init dll
    hw_wr8(dll_ctrl, 0x1);
    hw_wr8(dll_cnfg1, 0x5);

    //init tx delay line code
    u32 update_dc = 0x1;
    u32 inpsel_cnfg = 0x0;
    u32 bypass_en = 0x0;
    u32 extdly_en = 0x1;
    u32 sdclkdl_cnfg_data = (update_dc << 4) + (inpsel_cnfg << 2) + (bypass_en << 1) + extdly_en;
    hw_wr8(sdclkdl_cnfg, sdclkdl_cnfg_data); //set delay line update en, disable bypass and extdly
    hw_wr8(sdclkdl_dc, 0x40); //set tx delay line to center
    update_dc = 0x0;
    sdclkdl_cnfg_data = (update_dc << 4) + (inpsel_cnfg << 2) + (bypass_en << 1) + extdly_en;
    hw_wr8(sdclkdl_cnfg, sdclkdl_cnfg_data); //set delay line update finish, disable bypass and extdly

    //wait card insert
    u64 normal_int_stat_r = sdio_base_addr + 0x30;
    u64 error_int_stat_r = sdio_base_addr + 0x32;
    rdata = hw_rd16(normal_int_stat_r);
    while (0x40 != (rdata & 0x40))
    {
        rdata = hw_rd16(normal_int_stat_r);
    }
    hw_wr16(normal_int_stat_r, 0x40);

    //set 400KHz
    hw_sd_clk_change(sdio_base_addr, 500);

    // cmd0
    u16 cmd0 = 0x0;
    hw_sd_cmd_send(sdio_base_addr, 0x0, cmd0);
    // cmd8
    u16 cmd8 = 0x81A;
    hw_sd_cmd_send(sdio_base_addr, 0x1aa, cmd8);
    u64 resp01_r = sdio_base_addr + 0x10;
    u32 resp_cmd8 = hw_rd32(resp01_r);
    if (resp_cmd8 != 0x1aa){
        printf("*****Resp for CMD8 is %x.\n", resp_cmd8);
    }
    // cmd55
    u16 cmd55 = 0x371A;
    hw_sd_cmd_send(sdio_base_addr, 0x0, cmd55);
    u32 resp_cmd55 = hw_rd32(resp01_r);
    if (resp_cmd55 != 0x120){
        printf("*****Resp for CMD55 is %x.\n", resp_cmd55);
    }
    //cmd41
    u16 cmd41 = 0x2902;
    u32 resp_cmd41;
    do {
        hw_sd_cmd_send(sdio_base_addr, 0x51008000, cmd41);
        resp_cmd41 = hw_rd32(resp01_r);
    } while ((resp_cmd41 & 0x80000000) != 0x80000000);
    if (resp_cmd41 != 0xc1008000)
    {
        printf("*****Resp for CMD41 is %x.\n", resp_cmd41);
    }
    //switch voltage
    //cmd11
    u16 cmd11 = 0xb1a;
    hw_sd_cmd_send(sdio_base_addr, 0x0, cmd11);
    u32 resp_cmd11 = hw_rd32(resp01_r);
    if (resp_cmd11 != 0x300) {
        printf("*****Resp for CMD11 is %x.\n", resp_cmd11);
    }

    //disable clk
    rdata = hw_rd16(clk_ctrl_r);
    rdata = rdata & 0xfffb; //set sd_clk_en = 0
    hw_wr16(clk_ctrl_r, rdata);

    //check data line value
    u64 pstate_reg = sdio_base_addr + 0x24;
    rdata = hw_rd32(pstate_reg);
    if (0x0 != (rdata & 0xf00000))
    {
        printf("DAT is NOT 0000b!\n");
    }

    //change 1.8v
    rdata = hw_rd16(host_ctrl2_r);
    rdata = rdata | 0x8; //set signaling_en = 1
    hw_wr16(host_ctrl2_r, rdata);

    //wait 5ms
    hw_wait_ns(5000000);
    rdata = hw_rd16(host_ctrl2_r);
    if ((rdata & 0x8) != 0x8) {
        printf("Switch to 1.8V error\n");
    }

    //enable clk
    rdata = hw_rd16(clk_ctrl_r);
    rdata = rdata | 0x4;
    hw_wr16(clk_ctrl_r, rdata);

    hw_wait_ns(1000000);

    //check data line value
    rdata = hw_rd32(pstate_reg);
    if (0xf00000 != (rdata & 0xf00000))
    {
        printf("DAT is NOT 1111b!\n");
    }
    //voltage change finish
    
    // cmd2
    u16 cmd2 = 0x209;
    hw_sd_cmd_send(sdio_base_addr, 0x0, cmd2);
    u32 rca = 0x00010000;
    // cmd3
    u16 cmd3 = 0x31A;
    hw_sd_cmd_send(sdio_base_addr, 0x0, cmd3);
    u32 resp_cmd3 = hw_rd32(resp01_r);
    rca = resp_cmd3 & 0xffff0000;
    if((resp_cmd3 & 0xffff) != 0x500){
        printf("*****Resp for CMD3 is %x.\n", resp_cmd3);
    }
    // cmd7
    u16 cmd7 = 0x71A;
    hw_sd_cmd_send(sdio_base_addr, rca, cmd7);
    u32 resp_cmd7 = hw_rd32(resp01_r);
    if(resp_cmd7 != 0x700){
        printf("*****Resp for CMD7 is %x.\n", resp_cmd7);
    }

    //set bus width 4
    u64 host_ctrl1_r = sdio_base_addr + 0x28; //8bit
    hw_wr8(host_ctrl1_r, 0x20); //set 4bit bus width

    //acmd6
    hw_sd_cmd_send(sdio_base_addr, rca, cmd55);
    u16 cmd6 = 0x61a;
    hw_sd_cmd_send(sdio_base_addr, 0x2, cmd6);
    u32 resp_cmd6 = hw_rd32(resp01_r);
    if (resp_cmd6 != 0x920) {
        printf("*****Resp for CMD6 is %x.\n", resp_cmd6);
    }

    //clear all interrupt
    hw_wr16(normal_int_stat_r, 0x40ff);
    hw_wr16(error_int_stat_r, 0x1fff);

    //change sd speed to sdr104
    u16 host_ctrl2_data;
    host_ctrl2_data = hw_rd16(host_ctrl2_r);
    host_ctrl2_data = (host_ctrl2_data & 0xfff8) + 0x3;
    hw_wr16(host_ctrl2_r, host_ctrl2_data);
    //cmd6 set speed
    hw_sd_cmd_send(sdio_base_addr, 0x80000003, cmd6);
    resp_cmd6 = hw_rd32(resp01_r);
    if(resp_cmd6 != 0x900){
        printf("*****Resp for CMD6 is %x.\n", resp_cmd6);
    }

    //update tx delay line code
    update_dc = 0x1;
    inpsel_cnfg = 0x0;
    bypass_en = 0x0;
    extdly_en = 0x0;
    sdclkdl_cnfg_data = (update_dc << 4) + (inpsel_cnfg << 2) + (bypass_en << 1) + extdly_en;
    hw_wr8(sdclkdl_cnfg, sdclkdl_cnfg_data); //set delay line update en, disable bypass and extdly
    hw_wr8(sdclkdl_dc, 0x40); //set tx delay line to center
    update_dc = 0x0;
    sdclkdl_cnfg_data = (update_dc << 4) + (inpsel_cnfg << 2) + (bypass_en << 1) + extdly_en;
    hw_wr8(sdclkdl_cnfg, sdclkdl_cnfg_data); //set delay line update finish, disable bypass and extdly


    hw_sd_clk_change(sdio_base_addr, 0);
}

/*
 * sdio tuning main function
 */
int sdio_read_tuning_seq(void) {
    //init sdio card SDR104 4bit bus finished
    sdio_init_seq();

    /******************************************************************/
    //init sdio variable
    /******************************************************************/
    u64 sdio_base_addr = SD_MSH8_BADDR; //need change
    u64 blocksize_r = sdio_base_addr + 0x4; //16b
    u64 xfer_mode_r = sdio_base_addr + 0xc; //16b
    u64 clk_ctrl_r = sdio_base_addr + 0x2c; //16b
    u64 host_ctrl2_r = sdio_base_addr + 0x3e; //16b
    u64 normal_int_stat_r = sdio_base_addr + 0x30; //16b
    u64 error_int_stat_r = sdio_base_addr + 0x32; //16b
    u64 sw_rst_r = sdio_base_addr + 0x2f; //8b

    u64 p_vendor_specific_area = sdio_base_addr + 0xe8; //16b
    u16 vendor_point;
    vendor_point = hw_rd16(p_vendor_specific_area);

    u64 at_ctrl_r = sdio_base_addr + vendor_point + 0x40; //32b
    u64 at_stat_r = sdio_base_addr + vendor_point + 0x44; //32b

    /******************************************************************/
    //start sw tuning seq
    /******************************************************************/
    //turn off SD clk out
    u16 clk_ctrl_r_data;
    clk_ctrl_r_data = hw_rd16(clk_ctrl_r);
    clk_ctrl_r_data = clk_ctrl_r_data & 0xfffb; //set SD_CLK_EN = 0
    hw_wr16(clk_ctrl_r, clk_ctrl_r_data);
    //reset runing engine
    u16 host_ctrl2_data;
    host_ctrl2_data = hw_rd16(host_ctrl2_r);
    host_ctrl2_data = host_ctrl2_data & 0xff7f; //set SAMPLE_CLK_SEL = 0
    hw_wr16(host_ctrl2_r, host_ctrl2_data);
    //init CMD19 block setting
    hw_wr16(blocksize_r,0x80);
    hw_wr16(xfer_mode_r,0x10);
    //enable software tuning
    u32 at_ctrl_data;
    at_ctrl_data = hw_rd32(at_ctrl_r);
    at_ctrl_data = at_ctrl_data | 0x10; //set SW_TUNE_EN = 1
    hw_wr32(at_ctrl_r, at_ctrl_data);
    //set read delay line to 0
    u8 read_dc = 0;
    u32 at_stat_data;
    at_stat_data = hw_rd32(at_stat_r);
    at_stat_data = at_stat_data & 0xffffff00;
    at_stat_data = at_stat_data + (read_dc & 0xff); //set CENTER_PH_CODE = 0
    hw_wr32(at_stat_data, at_stat_data);
    //turn on SD clk out
    clk_ctrl_r_data = hw_rd16(clk_ctrl_r);
    clk_ctrl_r_data = clk_ctrl_r_data | 0x4; //set SD_CLK_EN = 1
    hw_wr16(clk_ctrl_r, clk_ctrl_r_data);
    //tuning sequence
    u8 tuning_result[128];
    u16 normal_int_data;
    u16 error_int_data;
    u8 buf_rd_ready;
    for (read_dc=0; read_dc <= 127; read_dc++) {
        hw_sd_cmd_send(sdio_base_addr, 0x0, 0x133a);
        while(1) {
            normal_int_data = hw_rd16(normal_int_stat_r);
            error_int_data = hw_rd16(error_int_stat_r);
            buf_rd_ready = normal_int_data & 0x20;
            error_int_data = error_int_data & 0x7f;
            if(buf_rd_ready) break;
            if(error_int_data) break;
        }
        error_int_data = hw_rd16(error_int_stat_r);
        error_int_data = error_int_data & 0x7f;
        if(error_int_data) {
            hw_wr16(error_int_stat_r,error_int_data);
            tuning_result[read_dc] = 0;
        } else {
            hw_wr16(normal_int_stat_r, 0x20);
            tuning_result[read_dc] = 1;
        }
        hw_wr8(sw_rst_r,0x6); //sw rst DAT and CMD
        while(1) {
            u8 sw_rst_data;
            sw_rst_data = hw_rd8(sw_rst_r);
            if (sw_rst_data == 0) break;
        }
    }
    //print result
    printf("Scan finish: \n");
    for (int i = 0; i <= 127; i++) {
        if (tuning_result[i]) {
            printf("P");
        } else {
            printf("X");
        }
    }
    printf("\n");
    return 0;
}
