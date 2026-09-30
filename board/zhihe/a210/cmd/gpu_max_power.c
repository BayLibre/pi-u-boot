#include <stdint.h>
#include <stdio.h>
#include <command.h>
#include "../include/utils/io.h"

typedef uint32_t u32;
typedef uint64_t u64;

#define AP_AON_PMIC_CTRL_BADDR  0x0030892000
#define LP_REG_PMIC_CTRL_ADDR_OFFSET  0x50
#define LP_CHIP_OFFSET_ADDR 0x2000000000
#define LP_REG_PPU_SW_STATE_ADDR_OFFSET 0x8
#define LP_REG_PPU_SW_REQ_ADDR_OFFSET 0x4
#define LP_REQ_W1 1
#define LP_REG_PCU_CUR_STATE_OFFSET 0x48
#define LP_REG_PPU_PWR_STS_ADDR_OFFSET 0x40
#define LP_MASK_ALL1 0xFFFFFFFF

#define LP_REG_PCU_SW_LPSTATE_ADDR_OFFSET 0xC

#define LP_REG_PCU_SW_LPREQ_ADDR_OFFSET 0x8
#define LP_REG_PCU_ISR_ADDR_OFFSET 0x30
#define DISABLE_LP_READ_POWER_STATE 0xFF
#define LP_REG_PCU_ICR_ADDR_OFFSET 0x28
#define PCU_INT_ALL_CLR 0x0b111111
#define LP_POWER_STATE_OFF 0x0
#define LP_POWER_STATE_ON 0x1F
#define LP_REG_BPC_SW_CTR_ADDR_OFFSET 0x0

#define LP_REG_PCU_IER_ADDR_OFFSET 0x24
#define PCU_INT_ALL_ON 0xb111111
#define LP_REG_PCU_DEVICE_ENABLE_LOW2HIGH_OFFSET 0x70
#define SIM_INTER_REG_ADDR 0xb
#define LP_REG_CCU_INT_EN_ADDR_OFFSET 0x28
#define LP_REG_CCU_CTRL_ADDR_OFFSET 0x4
#define LP_REG_CCU_AUTOGATE_FIELD_OFFSET 0x2
#define LP_REG_CCU_TIME_ADDR_OFFSET 0x0

#define AP_GPU_TOP_PCU_BADDR 0x0006E00000
#define AP_GPU_TOP_BPC_BADDR 0x0006E00400

#define LP_SV_POWER_ON_TOP  0x1001
#define LP_SV_POWER_OFF_TOP 0x10010
#define LP_SV_POWER_ON_GPU 0x5001
#define LP_SV_POWER_OFF_GPU 0x50010

#define LP_SV_POWER_DEFAULT 0x9999

#define AP_GPU_TOP_CFG_ACLK_CCU_BADDR 0x0006E01200
#define AP_GPU_TOP_ACLK_CCU_BADDR     0x0006E01400
#define AP_GPU_TOP_PCLK_CCU_BADDR     0x0006E01600
#define AP_GPU_ACLK_CCU_BADDR         0x0006D30200
#define AP_GPU_PCLK_CCU_BADDR         0x0006D30600
#define AP_GPU_CORE_CLK_CCU_BADDR     0x0006D30400
typedef enum {  LP_TOP_CCU,//0
                LP_NPUSS_CCU,//1
                LP_NPUSS_AXI_CCU,//2
                LP_NPUSS_PTW_CCU,//3
                LP_DDR0_CCU_SCL,//4
                LP_DDR1_CCU_SCL,//5
                LP_PCIE_CFG_CCU,//6
                LP_PCIE_AXI_CCU,//7
                LP_PCIE_PTW_CCU,//8
                LP_USB_CFG_CCU,//9
                LP_USB_AXI_CCU,//10
                LP_USB_PTW_CCU,//11
                LP_VI_CFG_CCU,//12
                LP_VI_AXI_CCU,//13
                LP_VI_PTW_CCU,//14
                LP_VO_CFG_CCU,//15
                LP_VO_AXI_CCU,//16
                LP_VO_PTW_CCU,//17
                LP_VP_CFG_CCU,//18
                LP_VP_AXI_CCU,//19
                LP_VP_PTW_CCU,//20
                LP_D2D_CFG_CCU,//21
                LP_D2D_AXI_CCU,//22
                LP_D2D_PTW_CCU,//no use
                LP_GPU_TOP_CFG_ACLK_CCU,
                LP_GPU_TOP_ACLK_CCU,
                LP_GPU_TOP_PCLK_CCU,
                LP_GPU_ACLK_CCU,
                LP_GPU_CORE_CLK_CCU,
                LP_GPU_PCLK_CCU,
                LP_TEE_CCU,
                LP_DEFAULT_CCU

} lp_ss_ccu_name_enum;
typedef enum {  LP_TOP_SS,//0
                LP_NPU_SS,
                LP_NPU_IP_SS,
                LP_NPU_R2P_SS,
                LP_DDR0_SCL_SS,
                LP_DDR0_DDRC_SS,
                LP_DDR1_SCL_SS,
                LP_DDR1_DDRC_SS,
                LP_PCIE_CTRL0_SS,
                LP_PCIE_CTRL1_SS,
                LP_PCIE_SATA_SS,//10
                LP_PCIE_R2P_SS,
                LP_USB_SS,
                LP_AON_SS,
                LP_AON_IP_SS,
                LP_AON_R2P_SS,
                LP_GPU_SS,
                LP_VO_SS,
                LP_VP_TOP_SS,
                LP_VP_ENC_SS,
                LP_VP_DEC_SS,//20
                LP_VP_R2P_SS,
                LP_VI_ENC_SS,//NOUSE
                LP_VI_DEC_SS,
                LP_VI_TOP_SS,
                LP_VI_R2P_SS,
                LP_D2D_SS,
                //LP_PERI0_SS,
                //LP_PERI1_SS,
                //LP_PERI2_SS,
                LP_C908_CORE0,
                LP_C908_CORE1,
                LP_C908_CORE2,
                LP_C908_CORE3,
                LP_C920_CORE0,
                LP_C920_CORE1,
                LP_C920_CORE2,
                LP_C920_CORE3,
                LP_C908_SS,
                LP_C920_SS,
                LP_CPU_TOP,
                LP_DEFAULT

} low_power_name_enum;

typedef enum {
    LP_USE_R2P,
    LP_USE_PPU,
    LP_USE_PCU
}lp_ss_use_type_enum;

/*----------------------------------------------------------------------------*/
#define wait_ns(ns) ndelay(ns)

#define write_register(addr,value) do{\
        printf("%s:%d write value:0x%x --> addr:0x%x\n", __FUNCTION__,__LINE__, (value), (uint32_t)(uintptr_t)(addr));\
        wr(addr, value); \
    }while(0)

#define read_register(addr) ({\
        u32 data = rd(addr); \
        printf("%s():%d read addr:0x%x -> data:0x%x\n", __FUNCTION__,__LINE__, (uint32_t)(uintptr_t)(addr), data), data;\
    })
#define cmp32(str, data1, data2, mask) do{ \
    printf("\033[031m%s():%d %s\033[0m\n", __func__, __LINE__,str);\
}while(0)
/*----------------------------------------------------------------------------*/
//================================
//get_lp_pcu_ppu_base_addr
//================================
u64 get_lp_pcu_ppu_base_addr(low_power_name_enum name_e,int chip_id) {

    u64 base_addr;

    switch(name_e) {
        case LP_GPU_SS: {
            base_addr = AP_GPU_TOP_PCU_BADDR;
            break;
        };
        default : printf("UVM_ERROR(Error): %d is not pcu/ppu domain\n",name_e);
    }
    
    base_addr = base_addr + chip_id*LP_CHIP_OFFSET_ADDR;
    return base_addr;

}
int lp_change_by_ppu(low_power_name_enum name_e,int state,int rd_en,int chip_id) {

    u64 base_addr;

    base_addr = get_lp_pcu_ppu_base_addr(name_e,chip_id);
    //1. write state
    write_register(base_addr+LP_REG_PPU_SW_STATE_ADDR_OFFSET,state);
    write_register(base_addr+LP_REG_PPU_SW_REQ_ADDR_OFFSET,LP_REQ_W1);
    wait_ns(500);
    return 0;
}

//================================
//get_lp_pctrl_name
//================================
char * get_lp_pctrl_name(low_power_name_enum name_e) {

    char *str;

    switch(name_e) {
        case LP_TOP_SS        : {str = "LP_TOP_SS";break;};
        case LP_NPU_SS        : {str = "LP_NPU_SS       ";break;};
        case LP_NPU_IP_SS     : {str = "LP_NPU_IP_SS    ";break;};
        case LP_NPU_R2P_SS    : {str = "LP_NPU_R2P_SS";break;};
        case LP_DDR0_SCL_SS   : {str = "LP_DDR0_SCL_SS  ";break;}
        case LP_DDR0_DDRC_SS  : {str = "LP_DDR0_DDRC_SS ";break;}
        case LP_DDR1_SCL_SS   : {str = "LP_DDR1_SCL_SS  ";break;}
        case LP_DDR1_DDRC_SS  : {str = "LP_DDR1_DDRC_SS ";break;}
        case LP_PCIE_CTRL0_SS : {str = "LP_PCIE_CTRL0_SS";break;}
        case LP_PCIE_CTRL1_SS : {str = "LP_PCIE_CTRL1_SS";break;}
        case LP_PCIE_SATA_SS  : {str = "LP_PCIE_SATA_SS ";break;}
        case LP_PCIE_R2P_SS   : {str = "LP_PCIE_R2P_SS";break;};
        case LP_USB_SS        : {str = "LP_USB_SS       ";break;}
        case LP_AON_SS        : {str = "LP_AON_SS       ";break;}
        case LP_AON_IP_SS     : {str = "LP_AON_IP_SS    ";break;}
        case LP_AON_R2P_SS    : {str = "LP_AON_R2P_SS   ";break;}
        case LP_GPU_SS        : {str = "LP_GPU_SS       ";break;}
        case LP_VO_SS         : {str = "LP_VO_SS        ";break;}
        case LP_VP_TOP_SS     : {str = "LP_VP_TOP_SS    ";break;}
        case LP_VP_ENC_SS     : {str = "LP_VP_ENC_SS    ";break;}
        case LP_VP_DEC_SS     : {str = "LP_VP_DEC_SS    ";break;}
        case LP_VP_R2P_SS     : {str = "LP_VP_R2P_SS";break;};
        case LP_VI_DEC_SS     : {str = "LP_VI_DEC_SS    ";break;}
        case LP_VI_TOP_SS     : {str = "LP_VI_TOP_SS    ";break;}
        case LP_VI_R2P_SS     : {str = "LP_VI_R2P_SS";break;};
        case LP_D2D_SS        : {str = "LP_D2D_SS       ";break;}
        default : {str = "please give a vld name!!!";printf("UVM_ERROR(Error): %d is not a vld name\n",name_e);};
    }
    
    return str;
}

//================================
//judge_use_type
//================================
lp_ss_use_type_enum judge_use_type(low_power_name_enum name_e) {

    lp_ss_use_type_enum type_e;

    switch(name_e) {
        case LP_DDR0_SCL_SS :
        case LP_DDR1_SCL_SS :
        case LP_DDR0_DDRC_SS :
        case LP_DDR1_DDRC_SS : {type_e = LP_USE_PPU;break;};
        case LP_C908_CORE0 :
        case LP_C908_CORE1 :
        case LP_C908_CORE2 :
        case LP_C908_CORE3 :
        case LP_C920_CORE0 :
        case LP_C920_CORE1 :
        case LP_C920_CORE2 :
        case LP_C920_CORE3 :
        case LP_C908_SS :
        case LP_C920_SS :
        case LP_CPU_TOP : {type_e = LP_USE_PPU;break;};
        case LP_AON_R2P_SS :
        case LP_PCIE_R2P_SS :
        case LP_VP_R2P_SS : 
        case LP_VI_R2P_SS : {type_e = LP_USE_R2P;break;};
        default : {type_e = LP_USE_PCU;};
    }
    
    return type_e;
}

//================================
//lp_compare_power_state
//================================
void lp_compare_power_state(low_power_name_enum name_e,int exp_state,int chip_id) {

    u32 rdata;
    u64 addr,offset;
    lp_ss_use_type_enum use_type_e;
    char *str;
    

    use_type_e = judge_use_type(name_e);
    if(use_type_e == LP_USE_PCU) {
        offset = LP_REG_PCU_CUR_STATE_OFFSET;
    }
    else if(use_type_e == LP_USE_PPU) {
        offset = LP_REG_PPU_PWR_STS_ADDR_OFFSET;
    }
    addr = get_lp_pcu_ppu_base_addr(name_e,chip_id) + offset;
    rdata = read_register(addr);
    str = get_lp_pctrl_name(name_e);
    //printf("%s power compare result:\n",str);
    if(exp_state == rdata) {
        printf("%s power compare result good: exp='h%x real='h%x\n",str,exp_state,rdata);
    }
    else {
        printf("UVM_ERROR(Error): %s power compare result bad: exp='h%x real='h%x\n",str,exp_state,rdata);
    }
    cmp32("power state exp vs. real:",exp_state,rdata,LP_MASK_ALL1);

}

//================================
//lp_sync_wr
//================================
void lp_sync_wr(u32 wdata) {

    #ifdef LP_SOC_SUPPLY_CTRL
    //LP_TEST_SUPPLY_SYNC
    tb_write_register(SIM_INTER_REG_ADDR,wdata);
    //while(1) {
    //}    
    wait_ns(100);    
    #endif

}
//================================
//lp_change_by_pcu
//================================
int lp_change_by_pcu(low_power_name_enum name_e,int state,int rd_en,int chip_id) {
    
    u64 base_addr;
    u32 rdata;
    u32 result;
    char *str;

    base_addr = get_lp_pcu_ppu_base_addr(name_e,chip_id); //0x0006E00000
    //printf("base_addr lp_change_by_pcu low %x high %x chip_id %x \n",base_addr, base_addr>>32,chip_id);
    //1 wr state
    write_register(base_addr+LP_REG_PCU_SW_LPSTATE_ADDR_OFFSET,state);  //0x0006E00000+0xc 
    write_register(base_addr+LP_REG_PCU_SW_LPREQ_ADDR_OFFSET,LP_REQ_W1);//0x0006E00000+0x8
    //2. read interrupt
    while(rd_en) {
      rdata = read_register(base_addr+LP_REG_PCU_ISR_ADDR_OFFSET); // 0x30 , get interrupt status
        if(rdata != 0) {
            str = get_lp_pctrl_name(name_e);
            if(name_e != LP_AON_SS) {
                printf("%s has interrupt(change by pcu,chip_id=%d), state=%x ,value=%x\n",str,chip_id,state,rdata);
            }
            break;
        }
        if(name_e != LP_AON_SS) {
            wait_ns(100);
        }
    }
    //judge
    result = (rd_en == 0)? 0 : (rdata == 1)? 1 : 0;
    if((rdata >> 2) == 1) {
        printf("Warning: pchl ack timeout\n");
    }
    #ifndef DISABLE_LP_READ_POWER_STATE
    if(result == 1) {
        lp_compare_power_state(name_e,state,chip_id);
    }
    #endif
    //3. clr interrupt
    if(rd_en == 1) {
        write_register(base_addr+LP_REG_PCU_ICR_ADDR_OFFSET, PCU_INT_ALL_CLR);
        if((name_e == LP_AON_SS) && (state == LP_POWER_STATE_OFF)) {
            //wait_ns(100);
            lp_sync_wr(LP_SV_POWER_OFF_TOP);
        }
        else {
            wait_ns(500);
        }
        rdata = read_register(base_addr+LP_REG_PCU_ISR_ADDR_OFFSET);
        cmp32("pcu clean all interrupt",0,rdata,LP_MASK_ALL1);
    }
    wait_ns(1000);
    return result;
}

//================================
//get_lp_sync_data
//================================
u32 get_lp_sync_data(low_power_name_enum name_e,int on_en) {

    u32 data;

    switch(name_e) {
        case LP_GPU_SS : {
            if(on_en == 1) {
                data = LP_SV_POWER_ON_GPU;
            } else {
                data = LP_SV_POWER_OFF_GPU;
            } 
            break;
        };
        default:
            printf("\033[031m%s():%d\033[0m\n", __func__, __LINE__);
    }
    return data;
} 
//================================
//lp_sync_power
//================================
void lp_sync_power(low_power_name_enum name_e,int state,int on_en,int chip_id) {
    
    u32 data;

    #ifndef LP_SOC_DISABLE_SYNC_POWER_DIRECE
        if(((state == LP_POWER_STATE_ON) && (on_en == 1)) || ((state == LP_POWER_STATE_OFF) && (on_en == 0))) {
            data = get_lp_sync_data(name_e,on_en);
            lp_sync_wr(data);    
        }
    #endif
}
int _lp_change_by_name(low_power_name_enum name_e,int state,int rd_en,int chip_id) {

    u32 result;
    lp_ss_use_type_enum use_type_e;

    lp_sync_power(name_e,state,1,chip_id);
    use_type_e = judge_use_type(name_e);
    if(use_type_e == LP_USE_PCU) {
        result = lp_change_by_pcu(name_e,state,rd_en,chip_id);
    }
    else if(use_type_e == LP_USE_PPU) {
        result = lp_change_by_ppu(name_e,state,rd_en,chip_id);
    }else{
        printf("\033[031m%s():%d\033[0m\n", __func__, __LINE__);
    }
    lp_sync_power(name_e,state,0,chip_id);
    return result;   

}

//================================
//get_lp_bpc_base_addr
//================================
u64 get_lp_bpc_base_addr(low_power_name_enum name_e,int chip_id) {

    u64 base_addr;

    switch(name_e) {
        case LP_GPU_SS        : {base_addr = AP_GPU_TOP_BPC_BADDR;break;};
        default : printf("UVM_ERROR(Error): %d is not bpc domain\n",name_e);
    }

    base_addr = base_addr + chip_id*LP_CHIP_OFFSET_ADDR;
    return base_addr;
}
#if 0
//================================
//lp_pctrl_pca_reg_init
//================================
void lp_pctrl_pca_reg_init(low_power_name_enum name_e) {

    u64 base_addr;

    //base_addr = get_lp_pca_base_addr(name_e);
    //write_register(base_addr+LP_REG_PCA_ENABLE_ADDR_OFFSET,1);

}
#endif
//================================
//lp_pctrl_bpc_reg_init
//================================
void lp_pctrl_bpc_reg_init(low_power_name_enum name_e,int chip_id) {

    u64 base_addr;
    u32 data;

    base_addr = get_lp_bpc_base_addr(name_e,chip_id);  
    data = read_register(base_addr+LP_REG_BPC_SW_CTR_ADDR_OFFSET);  //0x0006E00400 + 0x0
    data = data & 0xfffffffe;
    write_register(base_addr+LP_REG_BPC_SW_CTR_ADDR_OFFSET,data);

}
//================================
//lp_pctrl_pcu_reg_init
//================================
void lp_pctrl_pcu_reg_init(low_power_name_enum name_e,int chip_id) {

    u64 base_addr;

    base_addr = get_lp_pcu_ppu_base_addr(name_e,chip_id);

    write_register(base_addr+LP_REG_PCU_IER_ADDR_OFFSET, PCU_INT_ALL_ON);   //0x0006E00000 + 0x24
    write_register(base_addr+LP_REG_PCU_DEVICE_ENABLE_LOW2HIGH_OFFSET, 0x1);//0x0006E00000 + 0x70   TODO: can del this, default vl
}

//================================
//lp_gpu_ss_pctrl_init_by_name
//================================
int _lp_gpu_ss_pctrl_init_by_name(low_power_name_enum name_e,int chip_id) {

    int result;

    lp_sync_wr(LP_SV_POWER_ON_GPU);
    lp_pctrl_pcu_reg_init(name_e,chip_id);
  //lp_pctrl_pca_reg_init(name_e);
    lp_pctrl_bpc_reg_init(name_e,chip_id);
    result = _lp_change_by_name(name_e, LP_POWER_STATE_ON, 1, chip_id);
    lp_sync_wr(LP_SV_POWER_DEFAULT);
    
    return result;

}

int _lp_pctrl_common_init(low_power_name_enum name_e,int chip_id){
    int result;

    if(name_e == LP_GPU_SS) {
        result = _lp_gpu_ss_pctrl_init_by_name(name_e,chip_id);
    }else {
        cmp32("no pctrl", 1, 0, LP_MASK_ALL1);
    }

    return result;

}
//================================
//lp_pmic_init
//power management integrated circuit
//================================
void lp_pmic_init(int chip_id) {

    u32 data;
    
    data = read_register(AP_AON_PMIC_CTRL_BADDR+LP_REG_PMIC_CTRL_ADDR_OFFSET+chip_id*LP_CHIP_OFFSET_ADDR);
    //data = data | 24;
    data = data | 0xa3a3018;
    write_register(AP_AON_PMIC_CTRL_BADDR+LP_REG_PMIC_CTRL_ADDR_OFFSET+chip_id*LP_CHIP_OFFSET_ADDR, data);
    write_register(AP_AON_PMIC_CTRL_BADDR+0x5c+chip_id*LP_CHIP_OFFSET_ADDR, 0x3084a000+0x4  );
    write_register(AP_AON_PMIC_CTRL_BADDR+0x58+chip_id*LP_CHIP_OFFSET_ADDR, 0x3084a000+0x10);
    

}


//================================
//get_lp_ccu_base_addr
//================================
u64 get_lp_ccu_base_addr(lp_ss_ccu_name_enum name_e,int chip_id) {

    u64 base_addr;

    switch(name_e) {
        case LP_GPU_TOP_CFG_ACLK_CCU : {base_addr = AP_GPU_TOP_CFG_ACLK_CCU_BADDR;break;};
        case LP_GPU_TOP_ACLK_CCU     : {base_addr = AP_GPU_TOP_ACLK_CCU_BADDR;break;};
        case LP_GPU_TOP_PCLK_CCU     : {base_addr = AP_GPU_TOP_PCLK_CCU_BADDR;break;};
        case LP_GPU_ACLK_CCU         : {base_addr = AP_GPU_ACLK_CCU_BADDR;break;};
        case LP_GPU_CORE_CLK_CCU     : {base_addr = AP_GPU_CORE_CLK_CCU_BADDR;break;};
        case LP_GPU_PCLK_CCU         : {base_addr = AP_GPU_PCLK_CCU_BADDR;break;};
        default : {
            printf("\033[031m%s():%d\033[0m\n", __func__, __LINE__);
            printf("UVM_ERROR(Error): %d is not ccu domain\n",name_e);
        }
    }

    base_addr = base_addr + chip_id*LP_CHIP_OFFSET_ADDR;
    return base_addr;

}

//================================
//lp_ccu_reg_init_with_gating
// //================================
void _lp_ccu_reg_init_with_gating(lp_ss_ccu_name_enum name_e,int chip_id) {

    u32 data;
    u64 base_addr;

    base_addr = get_lp_ccu_base_addr(name_e,chip_id);
    //1.open interrupt
    write_register(base_addr+LP_REG_CCU_INT_EN_ADDR_OFFSET,1);
    //2.open auto clock gating
    data = read_register(base_addr+LP_REG_CCU_CTRL_ADDR_OFFSET);
    data = data | (1 << LP_REG_CCU_AUTOGATE_FIELD_OFFSET);
    write_register(base_addr+LP_REG_CCU_CTRL_ADDR_OFFSET,data);
    //3.mode: 0:gating 1:frequency is reduced 
    data = data & 0xfffffffd;
    write_register(base_addr+LP_REG_CCU_CTRL_ADDR_OFFSET,data);
    //4.change to hw mode
    data = data & 0xfffffffe;
    write_register(base_addr+LP_REG_CCU_CTRL_ADDR_OFFSET,data);

}
//================================
//lp_gpu_ss_ccu_init_by_name
//================================
int _lp_gpu_ss_ccu_init_by_name(lp_ss_ccu_name_enum name_e,int chip_id) {

    _lp_ccu_reg_init_with_gating(name_e,chip_id);
    
    return 1;

}
//================================
//judge_name_is_ddr_ss
//================================
int judge_name_is_ddr_ss(low_power_name_enum name_e) {

    u32 result;

    if((name_e == LP_DDR0_SCL_SS) || (name_e == LP_DDR1_SCL_SS) || (name_e == LP_DDR0_DDRC_SS) || (name_e == LP_DDR1_DDRC_SS)) {
        result = 1;
    }
    else {
        result = 0;    
    }
    
    return result;

}


//================================
//lp_ddr_ss_ccu_init_by_name
//================================
int _lp_ddr_ss_ccu_init_by_name(lp_ss_ccu_name_enum name_e,int chip_id) {

    u64 base_addr;
    u32 data;

    base_addr = get_lp_ccu_base_addr(name_e,chip_id);
    //1.open interrupt
    write_register(base_addr+LP_REG_CCU_INT_EN_ADDR_OFFSET,1);
    //2.open auto clock gating
    data = read_register(base_addr+LP_REG_CCU_CTRL_ADDR_OFFSET);
    data = data | (1 << LP_REG_CCU_AUTOGATE_FIELD_OFFSET);
    write_register(base_addr+LP_REG_CCU_CTRL_ADDR_OFFSET,data);
    //3. config time
    write_register(base_addr+LP_REG_CCU_TIME_ADDR_OFFSET,(0x40 << 0 | 0x0 << 8 | 0x8 << 16));
    //4.mode: 0:gating 1:frequency is reduced 
    data = data & 0xfffffffd;//(0 << LP_REG_CCU_CTRL_MODE_FIELD_OFFSET);
    write_register(base_addr+LP_REG_CCU_CTRL_ADDR_OFFSET,data);
    //5.change to hw mode
    data = data & 0xfffffffe;//(0 << LP_REG_CCU_CTRL_BYPASS_FIELD_OFFSET);
    write_register(base_addr+LP_REG_CCU_CTRL_ADDR_OFFSET,data);    
    return 1;

}
//================================
//lp_ccu_common_init
//================================
int _lp_ccu_common_init(lp_ss_ccu_name_enum name_e,int chip_id) {

    u64 base_addr;
    u32 is_ddr;
    
    base_addr = get_lp_ccu_base_addr(name_e,chip_id);
    is_ddr = judge_name_is_ddr_ss(name_e);
    if(is_ddr == 1) {
        _lp_ddr_ss_ccu_init_by_name(name_e,chip_id);
    }
    else if((name_e == LP_GPU_TOP_CFG_ACLK_CCU) || (name_e == LP_GPU_TOP_ACLK_CCU) || (name_e == LP_GPU_TOP_PCLK_CCU) || (name_e == LP_GPU_ACLK_CCU) || (name_e == LP_GPU_CORE_CLK_CCU) || (name_e == LP_GPU_PCLK_CCU)) {
        _lp_gpu_ss_ccu_init_by_name(name_e,chip_id);
    }else {
        cmp32("no ccu",1,0,LP_MASK_ALL1);
    }

    return 1;

}
// power control
int _lp_gpu_ss_pctrl_init(int chip_id) {

    int result;

    lp_pmic_init(chip_id);
    result = _lp_pctrl_common_init(LP_GPU_SS, chip_id);
    return result;
}
//================================
//lp_gpu_ss_ccu_init
//clock control unit
//================================
void _lp_gpu_ss_ccu_init(int chip_id) {

    _lp_ccu_common_init(LP_GPU_TOP_CFG_ACLK_CCU,chip_id);
    _lp_ccu_common_init(LP_GPU_TOP_ACLK_CCU,chip_id);
    _lp_ccu_common_init(LP_GPU_TOP_PCLK_CCU,chip_id);
    _lp_ccu_common_init(LP_GPU_ACLK_CCU,chip_id);
    _lp_ccu_common_init(LP_GPU_CORE_CLK_CCU,chip_id);
    _lp_ccu_common_init(LP_GPU_PCLK_CCU,chip_id);

}

static u32 iommu_rd(u64 addr)
{
    u32 rdata;
    rdata = read_register(addr);
    return rdata;
}

static void iommu_wr(u64 addr, u32 wdata)
{
    write_register(addr, wdata);
}
void set_iopmp(
    u32 device_id,
    u32 device_global,
    u64 addr_low,
    u64 addr_high,
    u32 flag,
    u64 iopmp_base_addr,
    u32 cnt
)
{
    u64 pmpaddr_low_sr2;
    u32 pmpaddr_low_wdata_l;
    u32 pmpaddr_low_wdata_h;
    u64 pmpaddr_low_regaddr_l;
    u64 pmpaddr_low_regaddr_h;

    u64 pmpaddr_high_sr2;
    u32 pmpaddr_high_wdata_l;
    u32 pmpaddr_high_wdata_h;
    u64 pmpaddr_high_regaddr_l;
    u64 pmpaddr_high_regaddr_h;

    u32 pmpcfg_wdata_mask;

    u32 pmpcfg_wdata;
    u32 pmpcfg_wdata_pre;
    u32 pmpcfg_rdata;
    u64 pmpcfg_regaddr;

    pmpaddr_low_sr2     = addr_low>>2;
    pmpaddr_low_wdata_l = pmpaddr_low_sr2&0xffffffff;
    pmpaddr_low_wdata_h = (pmpaddr_low_sr2>>32) + (device_id<<14) + (device_global<<31) ;

    pmpaddr_high_sr2     = addr_high>>2;
    pmpaddr_high_wdata_l = pmpaddr_high_sr2&0xffffffff;
    pmpaddr_high_wdata_h = (pmpaddr_high_sr2>>32) + (device_id<<14) + (device_global<<31) ;

    pmpaddr_low_regaddr_l  = iopmp_base_addr + 0x800 + cnt*16;
    pmpaddr_low_regaddr_h  = iopmp_base_addr + 0x800 + cnt*16+4 ;

    pmpaddr_high_regaddr_l = iopmp_base_addr + 0x800 + cnt*16+8 ;
    pmpaddr_high_regaddr_h = iopmp_base_addr + 0x800 + cnt*16+12;

    if (cnt%2 == 0) {
        pmpcfg_wdata_mask = 0xffff00ff;
        pmpcfg_wdata_pre = (0x1<<11) + (flag<<8);
    } else {
        pmpcfg_wdata_mask = 0x00ffffff;
        pmpcfg_wdata_pre = (0x1<<27) + (flag<<24);
    }

    pmpcfg_regaddr = iopmp_base_addr + (cnt/2)*4;
    pmpcfg_rdata = iommu_rd(pmpcfg_regaddr);
    pmpcfg_wdata = (pmpcfg_rdata & pmpcfg_wdata_mask) | pmpcfg_wdata_pre;

    iommu_wr(pmpaddr_low_regaddr_l, pmpaddr_low_wdata_l);
    iommu_wr(pmpaddr_low_regaddr_h, pmpaddr_low_wdata_h);
    iommu_wr(pmpaddr_high_regaddr_l, pmpaddr_high_wdata_l);
    iommu_wr(pmpaddr_high_regaddr_h, pmpaddr_high_wdata_h);
    iommu_wr(pmpcfg_regaddr,   pmpcfg_wdata);

}


#define AP_DDR0_SLC_SYSREG_BADDR 0x0004861000
#define AP_DDR1_SLC_SYSREG_BADDR 0x0005861000

#define CHIP_CRG_BASE_ADDR  0x00250000
#define AP_TEE_SYSREG_BADDR 0x27400000
#define AP_GPU_SYSREG_BADDR 0x06d02000
#define AP_GPU_DFMU_IOPMP_BADDR  0x0026D12000

extern int gpu_binlite_main(int argc, char *argv[]);

static int do_gpu_init(struct cmd_tbl *cmdtp, int flag, int argc,
             char *const argv[]){
    int rdata = 0;
    
    rdata = read_register(CHIP_CRG_BASE_ADDR+0x0);
    write_register(CHIP_CRG_BASE_ADDR+0x0,((rdata & 0x7F00FF)|0x200));//TOP_ONLY_CLK_CFG [10:8]:top_cfg_aclk_div_div_num
    rdata = read_register(CHIP_CRG_BASE_ADDR+0x18);

    write_register(CHIP_CRG_BASE_ADDR+0X18,((rdata & 0x0FF)|0x400));//GPU_CLK_CFG [11:8]: amux_clk_div_div_num
    write_register(AP_TEE_SYSREG_BADDR+0xE4,0x1);

    _lp_gpu_ss_pctrl_init(0);
    // _lp_gpu_ss_ccu_init(0);
    set_iopmp( 0x0, 0x1, 0x0, 0x8000000000, 0b111, AP_GPU_DFMU_IOPMP_BADDR, 1);

    write_register(AP_DDR0_SLC_SYSREG_BADDR+0x0, 0x00000003);
    write_register(AP_DDR1_SLC_SYSREG_BADDR+0x0, 0x00000003);

    rdata = read_register(0x80800000);

    rdata = read_register(0xC0010000);

    return 0;
}
static int do_gpu_binlite_main(struct cmd_tbl *cmdtp, int flag, int argc,
             char *const argv[]){
        gpu_binlite_main(0, NULL);
        return 0;
}
#define GPU_TOP_SYSREG_ADDR  0x06e06000
#define GPU_SYSREG_ADDR      AP_GPU_SYSREG_BADDR
static int do_dump_gpu_reg(struct cmd_tbl *cmdtp, int flag, int argc,
             char *const argv[]){
    uintptr_t reg_addrs[] = {
        (uintptr_t)0x06c0a100,  
        (uintptr_t)0x06c00100,
        (uintptr_t)0x06c00104,
        (uintptr_t)0x06c03800,
        (uintptr_t)0x06c00810,
        (uintptr_t)0x06c00818,
        (uintptr_t)0x06c0081c,
        (uintptr_t)0x06c00820,
        (uintptr_t)0x06c00824,
        (uintptr_t)0x06c00858,
        (uintptr_t)0x06c0085c,
        (uintptr_t)0x06c00860,
        (uintptr_t)0x06c00864,
        (uintptr_t)0x06c00828,
        (uintptr_t)0x06c0082c,
        (uintptr_t)0x06c00830,
        (uintptr_t)0x06c00834,
        (uintptr_t)0x06c00838,
        (uintptr_t)0x06c0083c,
        (uintptr_t)0x06c00840,
        (uintptr_t)0x06c00844,
        (uintptr_t)0x06c00850,
        (uintptr_t)0x06c00854,
        (uintptr_t)0x06c008c8,
        (uintptr_t)0x06c038c0,
        (uintptr_t)0x06c038c4        
    };

    uintptr_t gpu_top_sysreg[] ={
        (uintptr_t)(GPU_TOP_SYSREG_ADDR + 0x0),
        (uintptr_t)(GPU_TOP_SYSREG_ADDR + 0x200),
        (uintptr_t)(GPU_TOP_SYSREG_ADDR + 0x600),
        (uintptr_t)(GPU_TOP_SYSREG_ADDR + 0x800),
        (uintptr_t)(GPU_TOP_SYSREG_ADDR + 0xa00),
        (uintptr_t)(GPU_TOP_SYSREG_ADDR + 0xe00),
        (uintptr_t)(GPU_TOP_SYSREG_ADDR + 0xe04),
        (uintptr_t)(GPU_TOP_SYSREG_ADDR + 0xe08),
        (uintptr_t)(GPU_TOP_SYSREG_ADDR + 0xe0c),
        (uintptr_t)(GPU_TOP_SYSREG_ADDR + 0xe10),
        (uintptr_t)(GPU_TOP_SYSREG_ADDR + 0xe14),
        (uintptr_t)(GPU_TOP_SYSREG_ADDR + 0xe18),
        (uintptr_t)(GPU_TOP_SYSREG_ADDR + 0xe1c)
    };
    
    uintptr_t gpu_sysreg[] = {
        (uintptr_t)(GPU_SYSREG_ADDR + 0x0),
        (uintptr_t)(GPU_SYSREG_ADDR + 0x200),
        (uintptr_t)(GPU_SYSREG_ADDR + 0x600),
        (uintptr_t)(GPU_SYSREG_ADDR + 0x800),
        (uintptr_t)(GPU_SYSREG_ADDR + 0x804),
        (uintptr_t)(GPU_SYSREG_ADDR + 0x808),
        (uintptr_t)(GPU_SYSREG_ADDR + 0x80c),
        (uintptr_t)(GPU_SYSREG_ADDR + 0x810),
        (uintptr_t)(GPU_SYSREG_ADDR + 0x814),
        (uintptr_t)(GPU_SYSREG_ADDR + 0x818),
        (uintptr_t)(GPU_SYSREG_ADDR + 0xa08),
        (uintptr_t)(GPU_SYSREG_ADDR + 0xa0c),
        (uintptr_t)(GPU_SYSREG_ADDR + 0xc00),
        (uintptr_t)(GPU_SYSREG_ADDR + 0xe00),
        (uintptr_t)(GPU_SYSREG_ADDR + 0xe04),
        (uintptr_t)(GPU_SYSREG_ADDR + 0xe08),
        (uintptr_t)(GPU_SYSREG_ADDR + 0xe0c),
        (uintptr_t)(GPU_SYSREG_ADDR + 0xe10),
        (uintptr_t)(GPU_SYSREG_ADDR + 0xe14),
        (uintptr_t)(GPU_SYSREG_ADDR + 0xe18),
        (uintptr_t)(GPU_SYSREG_ADDR + 0xe1c),
        (uintptr_t)(GPU_SYSREG_ADDR + 0xe20),
        (uintptr_t)(GPU_SYSREG_ADDR + 0xe24),
        (uintptr_t)(GPU_SYSREG_ADDR + 0xe28),
    };

    // uintptr_t GPU_ACLK_CCU = 0x06D30200;
    // uintptr_t gpu_aclk_ccu[] = {
    //     (uintptr_t)(GPU_ACLK_CCU + 0x0),
    //     (uintptr_t)(GPU_ACLK_CCU + 0x4),
    //     (uintptr_t)(GPU_ACLK_CCU + 0x8),
    //     (uintptr_t)(GPU_ACLK_CCU + 0xC),
    //     (uintptr_t)(GPU_ACLK_CCU + 0x10),
    //     (uintptr_t)(GPU_ACLK_CCU + 0x18),
    //     (uintptr_t)(GPU_ACLK_CCU + 0x1C),
    //     (uintptr_t)(GPU_ACLK_CCU + 0x20),
    //     (uintptr_t)(GPU_ACLK_CCU + 0x24),
    //     (uintptr_t)(GPU_ACLK_CCU + 0x28)
    // };    

    // uintptr_t GPU_ACLK_LPEC = 0x06d30000;
    // uintptr_t gpu_aclk_lpec[] = {
    //     (uintptr_t)(GPU_ACLK_LPEC + 0x0),
    //     (uintptr_t)(GPU_ACLK_LPEC + 0x4),
    //     (uintptr_t)(GPU_ACLK_LPEC + 0x8),
    //     (uintptr_t)(GPU_ACLK_LPEC + 0xC),
    //     (uintptr_t)(GPU_ACLK_LPEC + 0x10),
    //     (uintptr_t)(GPU_ACLK_LPEC + 0x14),
    //     (uintptr_t)(GPU_ACLK_LPEC + 0x18)
    // };
    
    // uintptr_t gpu_core_clk_ccu[] = {
    //     (uintptr_t)(0x06D30400 + 0x0),
    //     (uintptr_t)(0x06D30400 + 0x4),
    //     (uintptr_t)(0x06D30400 + 0x8),
    //     (uintptr_t)(0x06D30400 + 0xC),
    //     (uintptr_t)(0x06D30400 + 0x10),
    //     (uintptr_t)(0x06D30400 + 0x18),
    //     (uintptr_t)(0x06D30400 + 0x1C),
    //     (uintptr_t)(0x06D30400 + 0x20),
    //     (uintptr_t)(0x06D30400 + 0x24),
    //     (uintptr_t)(0x06D30400 + 0x28)
    // };    
    // uint32_t gpu_core_clk_lpec[] = {
    //     (uintptr_t)(0x06D30800 + 0x0),
    //     (uintptr_t)(0x06D30800 + 0x4),
    //     (uintptr_t)(0x06D30800 + 0x8),
    //     (uintptr_t)(0x06D30800 + 0xC),
    //     (uintptr_t)(0x06D30800 + 0x10),
    //     (uintptr_t)(0x06D30800 + 0x14),
    //     (uintptr_t)(0x06D30800 + 0x18)
    // };
    uint32_t data = 0;

    printf("%-10s --> %-10s\n", "addr", "value");  // 表头对齐
    for(int i=0; i<(sizeof(reg_addrs)/sizeof(reg_addrs[0])); i++){
        data = rd(reg_addrs[i]);
        printf("0x%-8x --> 0x%-8x\n", (unsigned int)reg_addrs[i], data);  // 固定宽度对齐
    }

    printf("-----------------------GPU_TOP_SYSREG--------------------------------\n");
    for(int i=0; i<(sizeof(gpu_top_sysreg)/sizeof(gpu_top_sysreg[0])); i++){
        data = rd(gpu_top_sysreg[i]);
        printf("0x%-8x --> 0x%-8x\n", (unsigned int)gpu_top_sysreg[i], data);  // 固定宽度对齐
    }    

    printf("-----------------------GPU_SYSREG--------------------------------\n");
    for(int i=0; i<(sizeof(gpu_sysreg)/sizeof(gpu_sysreg[0])); i++){
        data = rd(gpu_sysreg[i]);
        printf("0x%-8x --> 0x%-8x\n", (unsigned int)gpu_sysreg[i], data);  // 固定宽度对齐
    }        

    return 0;
}

U_BOOT_CMD(
    gpu_init, 1,   1,  do_gpu_init,
    "run gpu register init",
    "run gpu register init"
);

U_BOOT_CMD(
    dump_gpu_reg, 1, 1, do_dump_gpu_reg,
    "dump all gpu register",
    "dump all gpu register"
);

U_BOOT_CMD(
    gpu_test, 1,   1,  do_gpu_binlite_main,
    "run gpu max power test",
    "run gpu max power test"    
);