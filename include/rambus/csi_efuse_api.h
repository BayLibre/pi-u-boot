/*
 * Copyright (C) 2025 Zhihe Group Holding Limited
 */
#ifndef __CSI_EFUSE_API_H__
#define __CSI_EFUSE_API_H__

#include <stdint.h>
#include "rambus_common.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
  \brief       efuse api init
  \param[in]   void
  \return      Error code \ref csi_error_t
*/
csi_error_t csi_efuse_api_init(void);

/**
  \brief       efuse api uninit
  \param[in]   void
*/
void csi_efuse_api_uninit(void);

/**
  \brief       efuse read api
  \param[in]   addr efuse map offset addr
  \param[out]  data efuse read output
  \param[int]  cnt  efuse read output count
  \return      Error code \ref csi_error_t
*/
csi_error_t csi_efuse_read_raw(uint32_t addr, void *data, uint32_t cnt);

/**
  \brief       efuse writ api
  \param[in]   addr efuse map offset addr
  \param[in]  data efuse write data
  \param[int]  cnt  efuse write data count
  \return      Error code \ref csi_error_t
*/
csi_error_t csi_efuse_write_raw(uint32_t addr, const void *data, uint32_t cnt);

/**
  \brief       efuse get life cycle init
  \param[in]   lc efuse life cycle value: 0:init, 1:dev, 2: oem, 3: pro, 4: rma, 5:rip
  \return      Error code \ref csi_error_t
*/
csi_error_t csi_efuse_get_lc(int *lc);

#ifdef __cplusplus
}
#endif

#endif	/* __CSI_EFUSE_API_H__ */
