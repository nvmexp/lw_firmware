/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2020 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file: acr_rv_dmat234.h
 */

#ifndef ACR_RV_DMAT234_H
#define ACR_RV_DMAT234_H

#include "rv_acr.h"

/*
 *  HAL Defines.
 */
#define acrlibSetupDmaCtl_HAL(pFlcnCfg, bIsCtxRequired) \
        acrlibSetupDmaCtl_T234(pFlcnCfg, bIsCtxRequired)
#define acrlibProgramDmaBase_HAL(pFlcnCfg, fbBase) \
        acrlibProgramDmaBase_T234(pFlcnCfg, fbBase)
#define acrlibProgramRegionCfg_HAL(pFlcnCfg, bUseCsb, ctxDma, regionID) \
        acrlibProgramRegionCfg_T234(pFlcnCfg, bUseCsb, ctxDma, regionID)
#define acrPopulateDMAParameters_HAL(wprRegIndex)  \
        acrPopulateDMAParameters_T234(wprRegIndex)
#define acrlibSetupCtxDma_HAL(pFlcnCfg, ctxDma, bIsPhysical) \
        acrlibSetupCtxDma_T234(pFlcnCfg, ctxDma, bIsPhysical)
#define acrlibIssueTargetFalconDma_HAL(dstOff, fbBase, fbOff, sizeInBytes, regionID, bIsSync, bIsDstImem, pFlcnCfg) \
        acrlibIssueTargetFalconDma_T234(dstOff, fbBase, fbOff, sizeInBytes, regionID, bIsSync, bIsDstImem, pFlcnCfg)
/*
 * Function declarations.
 */
void       acrlibSetupDmaCtl_T234(PACR_FLCN_CONFIG pFlcnCfg, LwBool bIsCtxRequired);
void       acrlibProgramDmaBase_T234(PACR_FLCN_CONFIG pFlcnCfg, LwU64 fbBase);
ACR_STATUS acrlibProgramRegionCfg_T234(PACR_FLCN_CONFIG pFlcnCfg, LwBool bUseCsb, LwU32 ctxDma, LwU32 regionID);
ACR_STATUS acrPopulateDMAParameters_T234(LwU32 wprRegIndex);
ACR_STATUS acrlibSetupCtxDma_T234(PACR_FLCN_CONFIG pFlcnCfg, LwU32 ctxDma, LwBool bIsPhysical);
ACR_STATUS acrlibIssueTargetFalconDma_T234(LwU32 dstOff, LwU64 fbBase, LwU32 fbOff,
                LwU32 sizeInBytes, LwU32 regionID, LwBool bIsSync,
                LwBool bIsDstImem, PACR_FLCN_CONFIG pFlcnCfg);
#endif
