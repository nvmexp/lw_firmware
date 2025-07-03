/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2022 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file: booter_boot_gsprm_ga10x.c
 */

//
// Includes
//
#include "booter.h"

#include "dev_gsp.h"
#include "dev_riscv_csr_64.h"
#include "dev_riscv_csr_64_addendum.h"
#include "lw_gsp_riscv_address_map.h"

/*!
 * @brief Programs the bootvector for RISC-Vs
 *
 * @param[in] regionId   WPR region ID
 * @param[in] pWprMeta   GSP-RM Metadata struct
 */
BOOTER_STATUS
booterSetupBootvec_GA10X
(
    LwU32         regionId,
    GspFwWprMeta *pWprMeta
)
{
    LwU64 val               = 0;
    LwU32 data              = 0;

    data = BOOTER_REG_RD32(BAR0, LW_PGSP_RISCV_BCR_PRIV_LEVEL_MASK);
    data = FLD_SET_DRF(_PRISCV, _RISCV_BCR_PRIV_LEVEL, _MASK_READ_PROTECTION, _ALL_LEVELS_ENABLED, data);
    data = FLD_SET_DRF(_PRISCV, _RISCV_BCR_PRIV_LEVEL, _MASK_WRITE_PROTECTION, _ALL_LEVELS_ENABLED, data); //TODO(suppal): Change level later
    // TODO-30952877: dmiyani: should we update this to PL3 write protected?
    BOOTER_REG_WR32(BAR0, LW_PGSP_RISCV_BCR_PRIV_LEVEL_MASK, data);

    val = (pWprMeta->bootBinOffset + pWprMeta->bootloaderCodeOffset)>>8;
    BOOTER_REG_WR32(BAR0, LW_PGSP_RISCV_BCR_DMAADDR_FMCCODE_LO, LwU64_LO32(val));
    BOOTER_REG_WR32(BAR0, LW_PGSP_RISCV_BCR_DMAADDR_FMCCODE_HI, LwU64_HI32(val));

    val = (pWprMeta->bootBinOffset + pWprMeta->bootloaderDataOffset)>>8;
    BOOTER_REG_WR32(BAR0, LW_PGSP_RISCV_BCR_DMAADDR_FMCDATA_LO,  LwU64_LO32(val));
    BOOTER_REG_WR32(BAR0, LW_PGSP_RISCV_BCR_DMAADDR_FMCDATA_HI,  LwU64_HI32(val));

    val = (pWprMeta->bootBinOffset + pWprMeta->bootloaderManifestOffset)>>8;
    BOOTER_REG_WR32(BAR0, LW_PGSP_RISCV_BCR_DMAADDR_PKCPARAM_HI, LwU64_HI32(val));
    BOOTER_REG_WR32(BAR0, LW_PGSP_RISCV_BCR_DMAADDR_PKCPARAM_LO, LwU64_LO32(val));

    BOOTER_REG_WR32(BAR0, LW_PGSP_RISCV_BCR_DMACFG_SEC, regionId);

    // Make sure to override HW INIT Values. Do not RMW
    LwU32 dmaCfg = DRF_DEF(_PRISCV_RISCV, _BCR_DMACFG, _TARGET, _LOCAL_FB)    |
                   DRF_DEF(_PRISCV_RISCV, _BCR_DMACFG, _LOCK,   _LOCKED);
    BOOTER_REG_WR32(BAR0, LW_PGSP_RISCV_BCR_DMACFG, dmaCfg);

    return BOOTER_OK;
}

