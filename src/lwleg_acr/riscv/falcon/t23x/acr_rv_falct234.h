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
 * @file: acr_rv_falct234.h
 */

#ifndef ACR_RV_FALCT234_H
#define ACR_RV_FALCT234_H

#include "rv_acr.h"

/*
 * HAL definition.
 */
#define acrlibIsFalconHalted_HAL(pFlcnCfg)                               \
        acrlibIsFalconHalted_T234(pFlcnCfg)
#define acrlibSetupBootvec_HAL(pFlcnCfg, bootvec)                        \
        acrlibSetupBootvec_T234(pFlcnCfg, bootvec)
#define acrlibEnableICD(pFlcnCfg)                                        \
        acrlibEnableICD(pFlcnCfg)
#define acrlibGetLwrrentTimeNs_HAL(pTime)                                \
        acrlibGetLwrrentTimeNs_T234(pTime)
#define acrlibGetElapsedTimeNs_HAL(pTime)                                \
        acrlibGetElapsedTimeNs_T234(pTime)
#define acrlibFindFarthestImemBl_HAL(pFlcnCfg, codeSizeInBytes)          \
        acrlibFindFarthestImemBl_T234(pFlcnCfg, codeSizeInBytes)
#define acrlibCheckIfUcodeFitsFalcon_HAL(pFlcnCfg, ucodeSize, bIsDstImem) \
        acrlibCheckIfUcodeFitsFalcon_T234(pFlcnCfg, ucodeSize, bIsDstImem)
#define acrlibFindRegMapping_HAL(pFlcnCfg,  acrLabel, pMap, pTgt)        \
        acrlibFindRegMapping_T234(pFlcnCfg, acrLabel, pMap, pTgt)
#define acrlibFlcnRegLabelWrite_HAL(pFlcnCfg, reglabel, data)            \
        acrlibFlcnRegLabelWrite_T234(pFlcnCfg, reglabel, data)
#define acrlibFlcnRegLabelRead_HAL(pFlcnCfg, reglabel)                   \
        acrlibFlcnRegLabelRead_T234(pFlcnCfg, reglabel)
#define acrlibFlcnRegRead_HAL(pFlcnCfg, tgt, regOff)                     \
        acrlibFlcnRegRead_T234(pFlcnCfg, tgt, regOff)
#define acrlibFlcnRegWrite_HAL(pFlcnCfg, tgt, regOff, data)              \
        acrlibFlcnRegWrite_T234(pFlcnCfg, tgt, regOff, data)
#define acrlibGetFalconConfig_HAL(falconId, falconInstance, pFlcnCfg)    \
        acrlibGetFalconConfig_T234(falconId, falconInstance, pFlcnCfg)
#define acrlibPollForScrubbing_HAL(pFlcnCfg)                             \
        acrlibPollForScrubbing_T234(pFlcnCfg)
#define acrlibResetFalcon_HAL(pFlcnCfg, bForceFlcnOnlyReset)             \
        acrlibResetFalcon_T234(pFlcnCfg, bForceFlcnOnlyReset)

/*
 * Function declarations.
 */
LwBool     acrlibIsFalconHalted_T234(PACR_FLCN_CONFIG pFlcnCfg);
void       acrlibSetupBootvec_T234(PACR_FLCN_CONFIG pFlcnCfg, LwU32 bootvec);
void       acrlibEnableICD(PACR_FLCN_CONFIG pFlcnCfg);
void       acrlibGetLwrrentTimeNs_T234(PACR_TIMESTAMP pTime);
LwU32      acrlibGetElapsedTimeNs_T234(const PACR_TIMESTAMP pTime);
LwU32      acrlibFindFarthestImemBl_T234(PACR_FLCN_CONFIG pFlcnCfg, LwU32 codeSizeInBytes);
LwBool     acrlibCheckIfUcodeFitsFalcon_T234(PACR_FLCN_CONFIG pFlcnCfg, LwU32 ucodeSize, LwBool bIsDstImem);
ACR_STATUS acrlibFindRegMapping_T234(PACR_FLCN_CONFIG pFlcnCfg, ACR_FLCN_REG_LABEL acrLabel, PACR_REG_MAPPING pMap, PFLCN_REG_TGT pTgt);
void       acrlibFlcnRegLabelWrite_T234(PACR_FLCN_CONFIG pFlcnCfg, ACR_FLCN_REG_LABEL reglabel, LwU32 data);
LwU32      acrlibFlcnRegLabelRead_T234(PACR_FLCN_CONFIG pFlcnCfg, ACR_FLCN_REG_LABEL reglabel);
LwU32      acrlibFlcnRegRead_T234(PACR_FLCN_CONFIG pFlcnCfg, FLCN_REG_TGT tgt, LwU32 regOff);
void       acrlibFlcnRegWrite_T234(PACR_FLCN_CONFIG pFlcnCfg, FLCN_REG_TGT tgt, LwU32 regOff, LwU32 data);
ACR_STATUS acrlibGetFalconConfig_T234(LwU32 falconId, LwU32 falconInstance, PACR_FLCN_CONFIG pFlcnCfg);
ACR_STATUS acrlibPollForScrubbing_T234(PACR_FLCN_CONFIG  pFlcnCfg);
ACR_STATUS acrlibResetFalcon_T234(PACR_FLCN_CONFIG pFlcnCfg, LwBool bForceFlcnOnlyReset);
#endif //ACR_RV_WPRT234_H