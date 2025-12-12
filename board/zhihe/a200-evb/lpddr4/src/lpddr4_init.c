#include "../include/common_lib.h"
#include "../include/ddr_common_func.h"
#include "../include/ddr_retention.h"
#include "../include/lpddr4_init.h"

extern void lp4x_4266_phy_train1d2d_1rank(void);
extern void lp4x_4266_phy_train1d2d_2rank(void);
extern void lp4x_3733_phy_train1d2d_1rank(void);
extern void lp4x_3733_phy_train1d2d_2rank(void);
extern void lp4x_3200_phy_train1d2d_1rank(void);
extern void lp4x_3200_phy_train1d2d_2rank(void);

void lpddr4_init(enum DDR_TYPE type, int rank_num, int speed, enum DDR_BITWIDTH bits)
{ 
  //4266 3733 3200 2133
  //Others RSVD
  pll_config(speed);
  	  
  deassert_pwrok_apb(bits);
  
  //4266 3733 3200 2133
  //Others RSVD
  ctrl_init(rank_num, speed);

  //mode support: 16 32 64
  addrmap(rank_num, bits);

  de_assert_other_reset_ddr();

  dq_pinmux(bits); // pinmux config before training

  if (type == DDR_TYPE_LPDDR4X && rank_num == 1 && speed == 4266) {
#ifdef CONFIG_DDR_LP4X_4266_SINGLERANK
    lp4x_4266_phy_train1d2d_1rank();
#endif
  } else if(type == DDR_TYPE_LPDDR4X && rank_num == 2 && speed == 4266) {
#ifdef CONFIG_DDR_LP4X_4266_DUALRANK
    lp4x_4266_phy_train1d2d_2rank();
#endif
  } else if (type == DDR_TYPE_LPDDR4X && rank_num == 1 && speed == 3733) {
#ifdef CONFIG_DDR_LP4X_3733_SINGLERANK
    lp4x_3733_phy_train1d2d_1rank();
#endif
  } else if(type == DDR_TYPE_LPDDR4X && rank_num == 2 && speed == 3733) {
#ifdef CONFIG_DDR_LP4X_3733_DUALRANK
    lp4x_3733_phy_train1d2d_2rank();
#endif
  } else if (type == DDR_TYPE_LPDDR4X && rank_num == 1 && speed == 3200) {
#ifdef CONFIG_DDR_LP4X_3200_SINGLERANK
    lp4x_3200_phy_train1d2d_1rank();
#endif
  } else if(type == DDR_TYPE_LPDDR4X && rank_num == 2 && speed == 3200) {
#ifdef CONFIG_DDR_LP4X_3200_DUALRANK
    lp4x_3200_phy_train1d2d_2rank();
#endif
  } else {
    printf("ERROR: unsupport ddr config\n");
    while(1);
  }

  dwc_ddrphy_phyinit_regInterface(saveRegs, rank_num);

  ctrl_en(bits);

  enable_axi_port(0x1f);

  enable_auto_refresh();

  lpddr4_auto_selref();
}

int fixup_ddr_addrmap(unsigned long size)
{
  enum DDR_TYPE type = get_ddr_type();
  int rank_num = get_ddr_rank_number();
  int speed = get_ddr_freq();
  enum DDR_BITWIDTH bits = get_ddr_bitwidth();

  return lpddr4_reinit_ctrl(type, rank_num, speed, bits, size);
}

int query_ddr_boundary(unsigned long size)
{
  enum DDR_TYPE type = get_ddr_type();
  int rank_num = get_ddr_rank_number();
  int speed = get_ddr_freq();
  enum DDR_BITWIDTH bits = get_ddr_bitwidth();

  return lpddr4_query_boundary(type, rank_num, speed, bits, size);
}

