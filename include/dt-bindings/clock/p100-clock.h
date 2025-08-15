#ifndef _P100_CLK_H
#define _P100_CLK_H

// PLL_WRAP
#define AUDIO0_PLL_FOUTVCO                   100
#define AUDIO0_PLL_FOUTPOSTDIV               101
#define AUDIO0_PLL_FOUT2                     102
#define AUDIO1_PLL_FOUTVCO                   103
#define AUDIO1_PLL_FOUTPOSTDIV               104
#define AUDIO1_PLL_FOUT2                     105
#define GMAC_PLL_FOUTVCO                     106
#define GMAC_PLL_FOUTPOSTDIV                 107
#define GMAC_PLL_FOUT1PH0                    108
#define DVFS_PLL_FOUTVCO                     109
#define DPU0_PLL_FOUTPOSTDIV                 110
#define DPU1_PLL_FOUTVCO                     111
#define DPU1_PLL_FOUTPOSTDIV                 112
#define DPU2_PLL_FOUTVCO                     113
#define DPU2_PLL_FOUTPOSTDIV                 114
#define VIDEO_PLL_FOUTVCO                    115
#define VIDEO_PLL_FOUTPOSTDIV                116
#define VIDEO_PLL_FOUT1PH0                   117
#define VIDEO_PLL_FOUT3                      118
#define TEE_PLL_FOUTVCO                      119
// TOP_CRG                                   
#define IOMMU_PTW_ACLK_DIV                   120
#define NOC_CCLK_DIV                         121
#define NOC_CCLK_MUX                         122
#define TOP_CFG_ACLK_DIV                     123
#define TOP_PCLK_DIV                         124
#define TOP_CPU_BAK_PLL0_CLK_DIV4            125
#define TOP_CPUSYS_PIC_CLK_DIV               126
#define TOP_CPU_DDR1_ACLK_MUX                127
#define TOP_CPU_DDR0_ACLK_MUX                128
#define TOP_CPU_BAK_PLL0_CLK_MUX0            129
#define TOP_CPU_BAK_PLL0_CLK_DIV0            130
#define TOP_CPU_BAK_PLL0_CLK_DIV1            131
#define TOP_CPU_BAK_PLL0_CLK_DIV3            132
#define TOP_CPUSYS_PIC_CLK_MUX               133
#define TOP_CPU_BAK_PLL0_CLK_MUX             134
#define TOP_CPU_BAK_PLL1_CLK_DIV1            135
#define TOP_CPUSYS_BUS_CLK_DIV               136
#define TOP_CPUSYS_BUS_CLK_MUX               137
#define TOP_CPU_BAK_PLL1_CLK_DIV0            138
#define TOP_CPU_BAK_PLL1_CLK_MUX0            139
#define TOP_CPU_BAK_PLL1_CLK_DIV3            140
#define TOP_CPU_BAK_PLL1_CLK_MUX             141
#define TOP_D2D_REF_CLK_MUX                  142
#define TOP_D2D_SCAN_CLK0_DIV                143
#define TOP_D2D_SCAN_CLK1_DIV                144
#define TOP_D2D_ACLK_DIV                     145
#define TOP_D2D_ACLK_MUX                     146
#define AMUX_CLK_DIV                         147
#define TOP_PERI_SPI_SSI_CLK0_DIV            148
#define TOP_PERI_MST_ACLK0_DIV               149
#define TOP_PERI_QSPI_SSI_CLK_MUX0           150
#define TOP_PERI_QSPI0_SSI_CLK_DIV0          151
#define TOP_PERI_QSPI0_SSI_CLK_DIV1          152
#define TOP_PERI_SPI_SSI_CLK1_DIV            153
#define TOP_PERI_HIRES_CLK0_DIV              154
#define TOP_PERI_HIRES_CLK1_DIV              155
#define TOP_UART_SCLK_MUX                    156
#define TOP_PERI_QSPI_SSI_CLK_MUX1           157
#define TOP_PERI_QSPI1_SSI_CLK_DIV0          158
#define TOP_PERI_QSPI1_SSI_CLK_DIV1          159
#define TOP_PERI_MST_CLK1_DIV                160
#define TOP_PERI_EMMC_REF_CLK_DIV            161
#define TOP_PERI_EMMC_REF_CLK_MUX            162
#define TOP_CPU_BAK_PLL1_CLK_DIV4            163
// CPU_SS_CLK_SYSREG                         
#define C908_CPU_TO_CDE_CLK_MUX              164
#define C920_CPU_TO_CDE_CLK_MUX              165
#define C908_CPU_CLK_CCU_RATIO_NORMAL        166
#define C920_CPU_CLK_CCU_RATIO_NORMAL        167
// CPU_SS_CPU_PLL                            
#define C908_PLL_FOUTVCO                     168
#define C920_PLL_FOUTVCO                     169
// DDR0_SYSREG
#define DDR0_PHY_DFICLK_EN                   170
#define DDR0_DDRC_ACLK_EN                    171
#define DDR0_DDRC_CCLK_EN                    172
#define DDR0_SBR_CLK_EN                      173
// DDR1_SYSREG                               
#define DDR1_PHY_DFICLK_EN                   174
#define DDR1_DDRC_ACLK_EN                    175
#define DDR1_DDRC_CCLK_EN                    176
#define DDR1_SBR_CLK_EN                      177
// SLC_DUAL_SYSREG                           
#define DDR_PLL_FOUTVCO                      178
#define DDR_PLL_FOUTPOSTDIV                  179
#define DDR_PLL_FOUT1PH0                     180
#define DDR_PLL_FOUT2                        181
#define DDR_PLL_FOUT4                        182
#define DDR_PLL_CLK_SEL                      183
#define DDR_PLL_CLK_EN                       184
#define DDR_CBUS2DDR_ACLK0                   185
#define DDR_CBUS2DDR_ACLK1                   186
// D2D_CRG_REG_T                             
#define D2D_SS_CTRL0_CLK_EN                  187
#define D2D_SS_CTRL1_CLK_EN                  188

// PERI0_SYSREG
#define PERI0_WDT0_PCLK_EN                   200
// PERI1_SYSREG
#define PERI1_SPI0_SSI_CLK_EN                250
#define PERI1_QSPI0_SSI_CLK_EN               251
#define PERI1_MST_BUS_PCLK_EN                252
#define PERI1_MST_BUS_ACLK_EN                253
#define PERI1_I2C2_IC_CLK_EN                 254
#define PERI1_I2C1_IC_CLK_EN                 255
#define PERI1_I2C0_IC_CLK_EN                 256
#define PERI1_GPIO1_PCLK_EN                  257
#define PERI1_GPIO0_PCLK_EN                  258
#define PERI1_GMAC1_HCLK_EN                  259
#define PERI1_GMAC1_ACLK_EN                  260
#define PERI1_GMAC0_HCLK_EN                  261
#define PERI1_GMAC0_ACLK_EN                  262
// PERI2_SYSREG
#define PERI2_I2C7_IC_CLK_EN                 300
#define PERI2_I2C6_IC_CLK_EN                 301
#define PERI2_I2C5_IC_CLK_EN                 302
#define PERI2_I2C4_IC_CLK_EN                 303
#define PERI2_I2C3_IC_CLK_EN                 304
#define PERI2_GPIO3_PCLK_EN                  305
#define PERI2_GPIO2_PCLK_EN                  306
#define PERI2_UART4_SCLK_EN                  307
#define PERI2_SPI1_SSI_CLK_EN                308
#define PERI2_QSPI1_SSI_CLK_EN               309
// PERI3_SYSREG
#define PERI3_GPIO4_PCLK_EN                  350
#define PERI3_EMMC_SDIO_REF_CLK_CG_EN        351
#define PERI3_EMMC_SDIO_REF_CLK              352

#define OSC_24M                              400
#define AON_OSC_CLK_LOGIC                    401

#define UART_SCLK_100M                       402
#define I2C_IC_CLK                           403

#endif
