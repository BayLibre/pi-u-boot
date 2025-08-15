/*
 * Copyright (C) 2025 Zhihe Group Holding Limited
 */

#ifndef _SOC_PARAMETER_H_
#define _SOC_PARAMETER_H_

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif
typedef struct {
	uint8_t efuse_lc_rma_update_offset;
	uint8_t efuse_dbg_key1_off;
	uint64_t tee_sys_base;
	uint64_t efuse_base;
	uint64_t eip150_base;
	uint64_t eip28_base;
	uint64_t eip120_i_base;
	uint64_t eip120_ii_base;
	uint64_t eip120_iii_base;
	uint64_t keyram_base;
}csi_soc_parmeter_t;

void csi_init_soc_parameter(const csi_soc_parmeter_t *soc_parameter);

#ifdef __cplusplus
}
#endif

#endif
