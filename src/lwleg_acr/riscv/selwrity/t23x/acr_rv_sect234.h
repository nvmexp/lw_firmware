/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file: acr_rv_boott234.h
 */

#ifndef ACR_RV_SECT234_H
#define ACR_RV_SECT234_H

#include "rv_acr.h"

/*
 * HAL definition.
 */
#define acrSetupFbhubDecodeTrap_HAL() \
        acrSetupFbhubDecodeTrap_T234()
#define acrSetupSctlDecodeTrap_HAL() \
        acrSetupSctlDecodeTrap_T234()
#define acrlibLockFalconRegSpace_HAL(sourceId, pTargetFlcnCfg, pTargetMaskPlmOldValue, pTargetMaskOldValue, setLock) \
        acrlibLockFalconRegSpace_T234(sourceId, pTargetFlcnCfg, pTargetMaskPlmOldValue, pTargetMaskOldValue, setLock)
#define acrEmulateMode_HAL(gpcNum) \
        acrEmulateMode_T234(gpcNum)
#define acrVprSmcCheck_HAL() \
		acrVprSmcCheck_T234()
#define acrSetupVprProtectedAccess_HAL() \
        acrSetupVprProtectedAccess_T234()
#define acrCheckIfBuildIsSupported_HAL() \
        acrCheckIfBuildIsSupported_T234()
#define acrSetupPbusDebugAccess_HAL() \
        acrSetupPbusDebugAccess_T234()
#define acrLowerClockGatingPLM_HAL() \
        acrLowerClockGatingPLM_T234()
#ifdef ACR_CHIP_PROFILE_T239
#define acrGetTimeBasedRandomCanary_HAL() \
        acrGetTimeBasedRandomCanary_T239()
#endif

/*
 * Function declarations.
 */
void       acrSetupFbhubDecodeTrap_T234(void);
void       acrSetupSctlDecodeTrap_T234(void);
ACR_STATUS acrlibLockFalconRegSpace_T234(LwU32 sourceId,
                        PACR_FLCN_CONFIG pTargetFlcnCfg,
                        LwU32 *pTargetMaskPlmOldValue,
                        LwU32 *pTargetMaskOldValue, LwBool setLock);
void acrEmulateMode_T234(LwU32 gpcNum);
ACR_STATUS acrVprSmcCheck_T234(void);
void acrSetupVprProtectedAccess_T234(void);
ACR_STATUS acrCheckIfBuildIsSupported_T234(void);
void acrSetupPbusDebugAccess_T234(void);
void acrLowerClockGatingPLM_T234(void);
#ifdef ACR_CHIP_PROFILE_T239
LwU64 acrGetTimeBasedRandomCanary_T239(void);
#endif
#endif //ACR_RV_SECT234_H
