#include <log.h>
#include <string.h>

#include "../include/ddr_common_func.h"

static void dq_pinmux_p1 (enum DDR_BITWIDTH bits) {
ddr_phy_broadcast_en(0);    

ddr_phy0_reg_wr(0x100a0,0x1);
ddr_phy0_reg_wr(0x100a1,0x5);
ddr_phy0_reg_wr(0x100a2,0x3);
ddr_phy0_reg_wr(0x100a3,0x0);
ddr_phy0_reg_wr(0x100a4,0x2);
ddr_phy0_reg_wr(0x100a5,0x4);
ddr_phy0_reg_wr(0x100a6,0x6);
ddr_phy0_reg_wr(0x100a7,0x7);
//PHY0 DBYTE1
ddr_phy0_reg_wr(0x110a0,0x7);
ddr_phy0_reg_wr(0x110a1,0x4);
ddr_phy0_reg_wr(0x110a2,0x3);
ddr_phy0_reg_wr(0x110a3,0x0);
ddr_phy0_reg_wr(0x110a4,0x2);
ddr_phy0_reg_wr(0x110a5,0x1);
ddr_phy0_reg_wr(0x110a6,0x5);
ddr_phy0_reg_wr(0x110a7,0x6);
//PHY0 DBYTE2
ddr_phy0_reg_wr(0x120a0,0x7);
ddr_phy0_reg_wr(0x120a1,0x4);
ddr_phy0_reg_wr(0x120a2,0x3);
ddr_phy0_reg_wr(0x120a3,0x0);
ddr_phy0_reg_wr(0x120a4,0x2);// FullMask version
ddr_phy0_reg_wr(0x120a5,0x1);// FullMask version
ddr_phy0_reg_wr(0x120a6,0x5);
ddr_phy0_reg_wr(0x120a7,0x6);
//PHY0 DBYTE3
ddr_phy0_reg_wr(0x130a0,0x7);
ddr_phy0_reg_wr(0x130a1,0x5);
ddr_phy0_reg_wr(0x130a2,0x0);
ddr_phy0_reg_wr(0x130a3,0x2);
ddr_phy0_reg_wr(0x130a4,0x1);
ddr_phy0_reg_wr(0x130a5,0x4);
ddr_phy0_reg_wr(0x130a6,0x3);
ddr_phy0_reg_wr(0x130a7,0x6);
if(bits==DDR_BITWIDTH_64) {
//PHY1 DBYTE0
ddr_phy1_reg_wr(0x100a0,0x7);
ddr_phy1_reg_wr(0x100a1,0x4);
ddr_phy1_reg_wr(0x100a2,0x3);
ddr_phy1_reg_wr(0x100a3,0x0);
ddr_phy1_reg_wr(0x100a4,0x1);
ddr_phy1_reg_wr(0x100a5,0x2);
ddr_phy1_reg_wr(0x100a6,0x5);
ddr_phy1_reg_wr(0x100a7,0x6);
//PHY1 DBYTE1
ddr_phy1_reg_wr(0x110a0,0x7);
ddr_phy1_reg_wr(0x110a1,0x5);
ddr_phy1_reg_wr(0x110a2,0x0);
ddr_phy1_reg_wr(0x110a3,0x2);
ddr_phy1_reg_wr(0x110a4,0x1);
ddr_phy1_reg_wr(0x110a5,0x4);
ddr_phy1_reg_wr(0x110a6,0x3);
ddr_phy1_reg_wr(0x110a7,0x6);
//PHY1 DBYTE2
ddr_phy1_reg_wr(0x120a0,0x1);
ddr_phy1_reg_wr(0x120a1,0x5);
ddr_phy1_reg_wr(0x120a2,0x3);
ddr_phy1_reg_wr(0x120a3,0x0);
ddr_phy1_reg_wr(0x120a4,0x2);
ddr_phy1_reg_wr(0x120a5,0x4);
ddr_phy1_reg_wr(0x120a6,0x6);
ddr_phy1_reg_wr(0x120a7,0x7);
//PHY1 DBYTE3
ddr_phy1_reg_wr(0x130a0,0x7);
ddr_phy1_reg_wr(0x130a1,0x4);
ddr_phy1_reg_wr(0x130a2,0x3);
ddr_phy1_reg_wr(0x130a3,0x0);
ddr_phy1_reg_wr(0x130a4,0x2);
ddr_phy1_reg_wr(0x130a5,0x1);
ddr_phy1_reg_wr(0x130a6,0x5);
ddr_phy1_reg_wr(0x130a7,0x6);

ddr_phy_broadcast_en(1);    
}

}

static void dq_pinmux_a200 (enum DDR_BITWIDTH bits) {
ddr_phy_broadcast_en(0);    
//PHY0 DBYTE0 CHA dq0~dq7
// (mem DQx, phy DQx)
ddr_phy0_reg_wr(0x100a0,0x5);//mem dq0
ddr_phy0_reg_wr(0x100a1,0x4);//mem dq1
ddr_phy0_reg_wr(0x100a2,0x7);//mem dq2
ddr_phy0_reg_wr(0x100a3,0x6);//mem dq3
ddr_phy0_reg_wr(0x100a4,0x0);//mem dq4
ddr_phy0_reg_wr(0x100a5,0x1);//mem dq5
ddr_phy0_reg_wr(0x100a6,0x2);//mem dq6
ddr_phy0_reg_wr(0x100a7,0x3);//mem dq7
//PHY0 DBYTE1 CHA dq8~dq15(minus 8)
ddr_phy0_reg_wr(0x110a0,0x4);//mem dq8
ddr_phy0_reg_wr(0x110a1,0x5);//mem dq9
ddr_phy0_reg_wr(0x110a2,0x3);//mem dq10
ddr_phy0_reg_wr(0x110a3,0x0);//mem dq11
ddr_phy0_reg_wr(0x110a4,0x2);//mem dq12
ddr_phy0_reg_wr(0x110a5,0x1);//mem dq13
ddr_phy0_reg_wr(0x110a6,0x7);//mem dq14
ddr_phy0_reg_wr(0x110a7,0x6);//mem dq15
//PHY0 DBYTE2 CHB dq0~dq7
ddr_phy0_reg_wr(0x120a0,0x4);//mem dq0
ddr_phy0_reg_wr(0x120a1,0x6);//mem dq1
ddr_phy0_reg_wr(0x120a2,0x7);//mem dq2
ddr_phy0_reg_wr(0x120a3,0x3);//mem dq3
ddr_phy0_reg_wr(0x120a4,0x0);//mem dq4
ddr_phy0_reg_wr(0x120a5,0x1);//mem dq5
ddr_phy0_reg_wr(0x120a6,0x2);//mem dq6
ddr_phy0_reg_wr(0x120a7,0x5);//mem dq7
//PHY0 DBYTE3 CHB dq8~dq15(minus 8)
ddr_phy0_reg_wr(0x130a0,0x5);//mem dq8
ddr_phy0_reg_wr(0x130a1,0x4);//mem dq9
ddr_phy0_reg_wr(0x130a2,0x3);//mem dq10
ddr_phy0_reg_wr(0x130a3,0x1);//mem dq11
ddr_phy0_reg_wr(0x130a4,0x2);//mem dq12
ddr_phy0_reg_wr(0x130a5,0x0);//mem dq13
ddr_phy0_reg_wr(0x130a6,0x7);//mem dq14
ddr_phy0_reg_wr(0x130a7,0x6);//mem dq15
if(bits==DDR_BITWIDTH_64) {
//PHY1 DBYTE0 CHA dq0~dq7
ddr_phy1_reg_wr(0x100a0,0x7);//mem dq0
ddr_phy1_reg_wr(0x100a1,0x6);//mem dq1
ddr_phy1_reg_wr(0x100a2,0x1);//mem dq2
ddr_phy1_reg_wr(0x100a3,0x2);//mem dq3
ddr_phy1_reg_wr(0x100a4,0x0);//mem dq4
ddr_phy1_reg_wr(0x100a5,0x3);//mem dq5
ddr_phy1_reg_wr(0x100a6,0x4);//mem dq6
ddr_phy1_reg_wr(0x100a7,0x5);//mem dq7
//PHY1 DBYTE1 CHA dq8~dq15(minus 8)
ddr_phy1_reg_wr(0x110a0,0x6);//mem dq8
ddr_phy1_reg_wr(0x110a1,0x4);//mem dq9
ddr_phy1_reg_wr(0x110a2,0x3);//mem dq10
ddr_phy1_reg_wr(0x110a3,0x2);//mem dq11
ddr_phy1_reg_wr(0x110a4,0x0);//mem dq12
ddr_phy1_reg_wr(0x110a5,0x1);//mem dq13
ddr_phy1_reg_wr(0x110a6,0x5);//mem dq14
ddr_phy1_reg_wr(0x110a7,0x7);//mem dq15
//PHY1 DBYTE2 CHB dq0~dq7
ddr_phy1_reg_wr(0x120a0,0x6);//mem dq0
ddr_phy1_reg_wr(0x120a1,0x7);//mem dq1
ddr_phy1_reg_wr(0x120a2,0x3);//mem dq2
ddr_phy1_reg_wr(0x120a3,0x2);//mem dq3
ddr_phy1_reg_wr(0x120a4,0x0);//mem dq4
ddr_phy1_reg_wr(0x120a5,0x1);//mem dq5
ddr_phy1_reg_wr(0x120a6,0x4);//mem dq6
ddr_phy1_reg_wr(0x120a7,0x5);//mem dq7
//PHY1 DBYTE3 CHB dq8~dq15(minus 8)
ddr_phy1_reg_wr(0x130a0,0x6);//mem dq8
ddr_phy1_reg_wr(0x130a1,0x3);//mem dq9
ddr_phy1_reg_wr(0x130a2,0x2);//mem dq10
ddr_phy1_reg_wr(0x130a3,0x0);//mem dq11
ddr_phy1_reg_wr(0x130a4,0x1);//mem dq12
ddr_phy1_reg_wr(0x130a5,0x4);//mem dq13
ddr_phy1_reg_wr(0x130a6,0x5);//mem dq14
ddr_phy1_reg_wr(0x130a7,0x7);//mem dq15

ddr_phy_broadcast_en(1);    
}

}

void dq_pinmux (enum DDR_BITWIDTH bits) {
	enum DDR_PINMUX pinmux = get_ddr_pinmux();
	switch(pinmux) {
	case DDR_PINMUX_TH1520:
		dq_pinmux_p1(bits);
		break;
	case DDR_PINMUX_A200:
		dq_pinmux_a200(bits);
		break;
	default:
		printf("Error: unknown ddr pinmux");
		while(1);
	}
}
