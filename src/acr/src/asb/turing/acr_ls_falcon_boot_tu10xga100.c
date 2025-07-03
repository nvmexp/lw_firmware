/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2019 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */
/*!
 * @file: acr_ls_falcon_boot_tu10x.c
 */
#include "acr.h"
#include "dev_sec_pri.h"
#include "dev_falcon_v4.h"
#include "dev_sec_pri.h"
#include "dev_lwdec_pri.h"
#include "dev_gsp.h"
#include "dev_pwr_pri.h"

#ifdef ACR_BUILD_ONLY
#include "config/g_acrlib_private.h"
#include "config/g_acr_private.h"
#else
#include "g_acrlib_private.h"
#endif

/*!
 * @brief Set up SEC2-specific registers
 */
void
acrlibSetupSec2Registers_TU10X
(
    PACR_FLCN_CONFIG pFlcnCfg
)
{
    ACR_REG_WR32(BAR0, LW_PSEC_FBIF_CTL2_PRIV_LEVEL_MASK, ACR_PLMASK_READ_L0_WRITE_L2);
    ACR_REG_WR32(BAR0, LW_PSEC_BLOCKER_PRIV_LEVEL_MASK, ACR_PLMASK_READ_L0_WRITE_L2);
    ACR_REG_WR32(BAR0, LW_PSEC_IRQTMR_PRIV_LEVEL_MASK, ACR_PLMASK_READ_L0_WRITE_L2);
}

/*!
 * @brief Setup PLMs of target falcons.
 *        PLMs raised per falcon are
 *        For all falcons setting FALCON_PRIVSTATE_PRIV_LEVEL_MASK to Level0 Read and Level3 Write
 *        GSP   - 
 *                LW_PGSP_FALCON_DIODT_PRIV_LEVEL_MASK,    Level0 Read and Level2 Write 
 *                LW_PGSP_FALCON_DIODTA_PRIV_LEVEL_MASK,   Level0 Read and Level2 Write
 *                LW_PGSP_IRQTMR_PRIV_LEVEL_MASK           Level0 Read and Level2 Write
 *
 *        LWDEC - LW_PLWDEC_FALCON_DIODT_PRIV_LEVEL_MASK   Level0 Read and Level2 Write
 *
 *        PMU   - LW_PPWR_FALCON_DIODT_PRIV_LEVEL_MASK     Level0 Read and Level2 Write
 *
 *        SEC2  - 
 *                LW_PSEC_FALCON_DIODT_PRIV_LEVEL_MASK     Level0 Read and Level2 Write
 *                LW_PSEC_FALCON_DIODTA_PRIV_LEVEL_MASK    Level0 Read and Level2 Write
 *
 * @param[out] pFlcnCfg   Structure to hold falcon config
 */
ACR_STATUS
acrlibSetupTargetFalconPlms_TU10X
(
    PACR_FLCN_CONFIG     pFlcnCfg
)
{
    LwU32 plmVal = 0;       
    //
    // TODO: For ACR_PLMASK_READ_L0_WRITE_L3, either
    // 1. Use DRF_DEF in this RegWrite code  OR
    // 2. Add lots of ct_asserts + build that define using one of the registers using DRF_DEF
    // This is to be done at all paces where this define is used
    //
    acrlibFlcnRegWrite_HAL(pAcrlib, pFlcnCfg, BAR0_FLCN, LW_PFALCON_FALCON_PRIVSTATE_PRIV_LEVEL_MASK, ACR_PLMASK_READ_L0_WRITE_L3);

    switch(pFlcnCfg->falconId)
    {
        case LSF_FALCON_ID_PMU:
            plmVal = ACR_REG_RD32(BAR0, LW_PPWR_FALCON_DIODT_PRIV_LEVEL_MASK);
            plmVal = FLD_SET_DRF(_PPWR, _FALCON_DIODT_PRIV_LEVEL_MASK, _WRITE_PROTECTION_LEVEL0, _DISABLE, plmVal);
            plmVal = FLD_SET_DRF(_PPWR, _FALCON_DIODT_PRIV_LEVEL_MASK, _WRITE_PROTECTION_LEVEL1, _DISABLE, plmVal);
            ACR_REG_WR32(BAR0, LW_PPWR_FALCON_DIODT_PRIV_LEVEL_MASK, plmVal); 
            break;

        case LSF_FALCON_ID_LWDEC: 
            plmVal = ACR_REG_RD32(BAR0, LW_PLWDEC_FALCON_DIODT_PRIV_LEVEL_MASK(0));
            plmVal = FLD_SET_DRF(_PLWDEC, _FALCON_DIODT_PRIV_LEVEL_MASK, _WRITE_PROTECTION_LEVEL0, _DISABLE, plmVal);
            plmVal = FLD_SET_DRF(_PLWDEC, _FALCON_DIODT_PRIV_LEVEL_MASK, _WRITE_PROTECTION_LEVEL1, _DISABLE, plmVal);
            ACR_REG_WR32(BAR0, LW_PLWDEC_FALCON_DIODT_PRIV_LEVEL_MASK(0), plmVal); 
            break;

        case LSF_FALCON_ID_GSPLITE: 
            plmVal = ACR_REG_RD32(BAR0, LW_PGSP_FALCON_DIODT_PRIV_LEVEL_MASK);
            plmVal = FLD_SET_DRF(_PGSP, _FALCON_DIODT_PRIV_LEVEL_MASK, _WRITE_PROTECTION_LEVEL0, _DISABLE, plmVal);
            plmVal = FLD_SET_DRF(_PGSP, _FALCON_DIODT_PRIV_LEVEL_MASK, _WRITE_PROTECTION_LEVEL1, _DISABLE, plmVal);
            ACR_REG_WR32(BAR0, LW_PGSP_FALCON_DIODT_PRIV_LEVEL_MASK, plmVal); 

            plmVal = ACR_REG_RD32(BAR0, LW_PGSP_FALCON_DIODTA_PRIV_LEVEL_MASK);
            plmVal = FLD_SET_DRF(_PGSP, _FALCON_DIODTA_PRIV_LEVEL_MASK, _WRITE_PROTECTION_LEVEL0, _DISABLE, plmVal);
            plmVal = FLD_SET_DRF(_PGSP, _FALCON_DIODTA_PRIV_LEVEL_MASK, _WRITE_PROTECTION_LEVEL1, _DISABLE, plmVal);
            ACR_REG_WR32(BAR0, LW_PGSP_FALCON_DIODTA_PRIV_LEVEL_MASK, plmVal); 

            plmVal = ACR_REG_RD32(BAR0, LW_PGSP_IRQTMR_PRIV_LEVEL_MASK);
            plmVal = FLD_SET_DRF(_PGSP, _IRQTMR_PRIV_LEVEL_MASK, _WRITE_PROTECTION_LEVEL0, _DISABLE, plmVal);
            plmVal = FLD_SET_DRF(_PGSP, _IRQTMR_PRIV_LEVEL_MASK, _WRITE_PROTECTION_LEVEL1, _DISABLE, plmVal);
            ACR_REG_WR32(BAR0, LW_PGSP_IRQTMR_PRIV_LEVEL_MASK, plmVal); 
            break;

        case LSF_FALCON_ID_SEC2:
            plmVal = ACR_REG_RD32(BAR0, LW_PSEC_FALCON_DIODT_PRIV_LEVEL_MASK);
            plmVal = FLD_SET_DRF(_PSEC, _FALCON_DIODT_PRIV_LEVEL_MASK, _WRITE_PROTECTION_LEVEL0, _DISABLE, plmVal);
            plmVal = FLD_SET_DRF(_PSEC, _FALCON_DIODT_PRIV_LEVEL_MASK, _WRITE_PROTECTION_LEVEL1, _DISABLE, plmVal);
            ACR_REG_WR32(BAR0, LW_PSEC_FALCON_DIODT_PRIV_LEVEL_MASK, plmVal); 

            plmVal = ACR_REG_RD32(BAR0, LW_PSEC_FALCON_DIODTA_PRIV_LEVEL_MASK);
            plmVal = FLD_SET_DRF(_PSEC, _FALCON_DIODTA_PRIV_LEVEL_MASK, _WRITE_PROTECTION_LEVEL0, _DISABLE, plmVal);
            plmVal = FLD_SET_DRF(_PSEC, _FALCON_DIODTA_PRIV_LEVEL_MASK, _WRITE_PROTECTION_LEVEL1, _DISABLE, plmVal);
            ACR_REG_WR32(BAR0, LW_PSEC_FALCON_DIODTA_PRIV_LEVEL_MASK, plmVal);
            break;
    }
    return ACR_OK;
}
