//TODO:define update
//#define DDR_SYSREG_BADDR 0xFFFF005000//--P1
//#define _DDR_PHY_BADDR   0xfffd000000//--P1
//#define _DDR_PHY1_BADDR   _DDR_PHY_BADDR+0x1000000//--P1
//#define _DDR_CTRL_BADDR  _DDR_PHY_BADDR+0x2000000//--P1
#define _DDR_PHY_BADDR              0x04000000
#define _DDR_PHY1_BADDR             _DDR_PHY_BADDR
#define _DDR_CTRL_BADDR             0x04800000
#define _MP                         0x000003f8
#define _FREQ1                      0x00002000
#define _FREQ2                      0x00003000
#define _FREQ3                      0x00004000
//P100 85P update by 20240311//{{{
#define _DDRSS_BASE_ADDR            0x00000000
#define _PHY_BADDR                  0x04000000
#define _DDRC_BADDR                 _DDR_CTRL_BADDR
#define _SYSREG_DDR_BADDR           0x04810000
#define _SYSREG_BADDR               _SYSREG_DDR_BADDR
#define _PERFLOG_BADDR              0x04811000
#define _MT_BADDR                   0x04820000
#define _SCR_BADDR                  0x24830000
#define _SLC_BADDR                  0x04840000
#define _BMU_BADDR                  0x04850000
#define _QGEN_BADDR                 0x04860000
#define _SYSREG_SLC_BADDR           0x04861000
#define _SLC_DUAL_SC_BADDR          0x04900000
#define DDR_SYSREG_BADDR            _SYSREG_DDR_BADDR//function on ddr_sysreg_wr/rd
#define SLC_DUAL_SYSREG_BADDR       _SLC_DUAL_SC_BADDR//function on slc_dual_sysreg_wr/rd
#define SLC_WRAP_SYSREG_BADDR       _SYSREG_SLC_BADDR//function on slc_wrap_sysreg_wr/rd
#define BMU_BADDR                   _BMU_BADDR//function on bmu_int
#define DDR_CFG0                    0x0
#define DDR_CFG1                    0x4
#define DDR_PLL_CFG0                0x8
#define DDR_PLL_CFG1                0xc
#define DDR_PLL_CFG2                0x10
#define DDR_PLL_CFG3                0x14
#define DDR_PLL_STS                 0x18
//}}}

#define _LPDDR4_BADDR DDR_AXI4S0_BADDR
#define _LPDDR4_EADDR DDR_AXI4S0_EADDR
#define  PUB_RDIMMCR1              0x00000055 
#define DDR_DCH1_OFFSET            0x1000000

#include "ddr_reg_define.h"


#ifdef USE_LPDDR4
//TODO:define update
//#define DDR_ADDRMAP0      0x0000001f//only one active rank,cs_bit0 not used    
//#define DDR_ADDRMAP1      0x00080808//b2 b1 b0                                 
//#define DDR_ADDRMAP2      0x00000000//c5 c4 c3 c2                              
//#define DDR_ADDRMAP3      0x00000000//c9 c8 c7 c6                              
//#define DDR_ADDRMAP4      0x00000f0f//c11 c10(not used,so config f)            
//#define DDR_ADDRMAP5      0x07070707//r11 r10-r2 r1 r0                         
//#define DDR_ADDRMAP6      0x0f070707//r15(not used) r14 r13 r12                
//#define DDR_ADDRMAP7      0xffffffff//not used
//#define DDR_ADDRMAP8      0xffffffff//not used
//#define DDR_ADDRMAP9      0xffffffff//not used
//#define DDR_ADDRMAP10     0xffffffff//not used
//#define DDR_ADDRMAP11     0xffffffff//not used
#define DDR_ADDRMAP0      0x001f001f//only one active rank,cs_bit0 not used    
#define DDR_ADDRMAP1      0x00080808//b2 b1 b0                                 
#define DDR_ADDRMAP2      0x00000000//c5 c4 c3 c2                              
#define DDR_ADDRMAP3      0x00000000//c9 c8 c7 c6                              
#define DDR_ADDRMAP4      0x00001f1f//c11 c10(not used,so config f)            
#define DDR_ADDRMAP5      0x070f0707//r11 r10-r2 r1 r0                         
#define DDR_ADDRMAP6      0x07070707//r15(not used) r14 r13 r12                
#define DDR_ADDRMAP7      0x00000f0f//
//#define DDR_ADDRMAP8      0x07070707//not used
#define DDR_ADDRMAP9      0x07070707//
#define DDR_ADDRMAP10     0x07070707//
#define DDR_ADDRMAP11     0x00000007//
#endif

#ifdef USE_DDR4
#define DDR_ADDRMAP0     0x0000001f
#define DDR_ADDRMAP1     0x003f0909
#define DDR_ADDRMAP2     0x01010100
#define DDR_ADDRMAP3     0x01010101
#define DDR_ADDRMAP4     0x00001f1f
#define DDR_ADDRMAP5     0x070f0707
#define DDR_ADDRMAP6     0x07070707
#define DDR_ADDRMAP7     0x00000f0f
#define DDR_ADDRMAP8     0x00003f01
#define DDR_ADDRMAP9     0x07070707
#define DDR_ADDRMAP10    0x07070707
#define DDR_ADDRMAP11    0x00000007
#endif

#define DDR_INIT_STATUS_MAILBOX           SIM_CTRL_BADDR+0x000000f0
#define DDR_INIT_FINISH_FLAG              0x88888888
#define DDR_CODE_DOWNLOAD_FINISH_FLAG     0x99999999
#define DDR_INIT_LOAD_FW_FLAG             0x11111111
#define DDR_INIT_LOAD_FW_FINISH_FLAG      0x22222222
