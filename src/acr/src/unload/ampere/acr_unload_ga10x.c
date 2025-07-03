/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2019-2020 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file: acr_unload_ga10x.c
 */
#include "acr.h"
#include "dev_falcon_v4.h"
#include "dev_fbif_v4.h"

#include "config/g_acr_private.h"
#include "config/g_acrlib_private.h"


/*!
 * @brief Reset falcon with given ID
 *        Note that here we reset and bring the falcons out of reset, so that falcons are accessible
 *        This is done for usecases like we use SEC2 mutex for general purpose on GP10X, so if we just
 *        reset sec2 and don't bring it back, the mutex acquire/release would fail
 *
 * @return     ACR_OK     If reset falcon is successful
 */
ACR_STATUS
acrLockFalconDmaRegion_GA10X(LwU32 falconId)
{
    ACR_STATUS      status = ACR_OK;
#if defined(ACR_UNLOAD_ON_SEC2)
    ACR_FLCN_CONFIG flcnCfg;
    LwU32           regionCfg;
    LwU32           regionCfgPlm;

    //
    // We don't have a way yet to distinguish GC6/GCOFF
    // For GC6 entry, PMU is responsible for
    // 1. Wait for PCIE link to be L2
    // 2. If L2 is engaged, put FB into self refresh
    // 3. Trigger SCI/BSI to transfer control to island
    // So given PMU is having functional requirement in GC6 we cannot reset PMU
    // GCOFF behavior has to align as we don't have a way to distinguish GC6/GCOFF
    // https://confluence.lwpu.com/display/GC6Software/IFR+Driven+GC6+-+Tear+Down+WPR for more information
    //
    if (LSF_FALCON_ID_PMU_RISCV == falconId)
    {
        // Get the specifics of this falconID
        CHECK_STATUS_AND_RETURN_IF_NOT_OK(acrlibGetFalconConfig_HAL(pAcrlib, falconId, LSF_FALCON_INSTANCE_DEFAULT_0, &flcnCfg));
        regionCfg    = acrlibFlcnRegLabelRead_HAL(pAcrlib, &flcnCfg, REG_LABEL_FBIF_REGIONCFG);
        regionCfgPlm = acrlibFlcnRegLabelRead_HAL(pAcrlib, &flcnCfg, REG_LABEL_FBIF_REGIONCFG_PRIV_LEVEL_MASK);

        // Only allow DMA to WPR1
        regionCfg = (DRF_NUM(_PFALCON, _FBIF_REGIONCFG, _T0, LSF_WPR_EXPECTED_REGION_ID) |
                     DRF_NUM(_PFALCON, _FBIF_REGIONCFG, _T1, LSF_WPR_EXPECTED_REGION_ID) |
                     DRF_NUM(_PFALCON, _FBIF_REGIONCFG, _T2, LSF_WPR_EXPECTED_REGION_ID) |
                     DRF_NUM(_PFALCON, _FBIF_REGIONCFG, _T3, LSF_WPR_EXPECTED_REGION_ID) |
                     DRF_NUM(_PFALCON, _FBIF_REGIONCFG, _T4, LSF_WPR_EXPECTED_REGION_ID) |
                     DRF_NUM(_PFALCON, _FBIF_REGIONCFG, _T5, LSF_WPR_EXPECTED_REGION_ID) |
                     DRF_NUM(_PFALCON, _FBIF_REGIONCFG, _T6, LSF_WPR_EXPECTED_REGION_ID) |
                     DRF_NUM(_PFALCON, _FBIF_REGIONCFG, _T7, LSF_WPR_EXPECTED_REGION_ID));
        acrlibFlcnRegLabelWrite_HAL(pAcrlib, &flcnCfg, REG_LABEL_FBIF_REGIONCFG, regionCfg);

        // Raise PLM so the DMA region cannot be changed
        regionCfgPlm = FLD_SET_DRF(_PFALCON, _FBIF_REGIONCFG_PRIV_LEVEL_MASK, _WRITE_PROTECTION_LEVEL0, _DISABLE, regionCfgPlm);
        regionCfgPlm = FLD_SET_DRF(_PFALCON, _FBIF_REGIONCFG_PRIV_LEVEL_MASK, _WRITE_PROTECTION_LEVEL1, _DISABLE, regionCfgPlm);
        regionCfgPlm = FLD_SET_DRF(_PFALCON, _FBIF_REGIONCFG_PRIV_LEVEL_MASK, _WRITE_PROTECTION_LEVEL2, _DISABLE, regionCfgPlm);
        acrlibFlcnRegLabelWrite_HAL(pAcrlib, &flcnCfg, REG_LABEL_FBIF_REGIONCFG_PRIV_LEVEL_MASK, regionCfgPlm);
    }
#endif
    return status;
}

