#ifndef _SS_H_
#define _SS_H_

typedef enum {
    VP_CPR,
    VI_CPR,
    NPU_CPR,
    VO_CPR,
    PERI_CPR,
    PCIE_SATA_CPR,
    USB_CPR,
    TEE_CPR,
    GPU_CPR,
    D2D_CPR,
    D2D_CPU_CPR,
    MAX_CPR,
} ss_contrl;

//#define SS_CFG_DEFAULT (1 << PERI_CPR) | (1 << USB_CPR) | (1 << PCIE_SATA_CPR)
#define SS_CFG_DEFAULT 0x7FF

#endif /* _SS_H_ */
