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
 * @file: acruc_rv_libt234.h
 */

#ifndef ACRUC_RV_LIBT234_H
#define ACRUC_RV_LIBT234_H

#include "rv_acr.h"

/*
 * HAL definition.
 */
#define acrlibIsBootstrapOwner_HAL(falconId)                           \
        acrlibIsBootstrapOwner_T234(falconId)
#define acrlibSetupTargetRegisters_HAL(pFlcnCfg)                       \
        acrlibSetupTargetRegisters_T234(pFlcnCfg)
#define acrlibCheckTimeout_HAL(timeoutNs, startTimeNs, pTimeoutLeftNs) \
        acrlibCheckTimeout_T234(timeoutNs, startTimeNs, pTimeoutLeftNs)

/*
 * Function declarations.
 */
void       acrWriteStatusToFalconMailbox(ACR_STATUS status);
LwBool     acrlibIsBootstrapOwner_T234(LwU32 falconId);
ACR_STATUS acrlibSetupTargetRegisters_T234(PACR_FLCN_CONFIG pFlcnCfg);
ACR_STATUS acrlibCheckTimeout_T234(LwU32 timeoutNs, ACR_TIMESTAMP startTimeNs, LwS32 *pTimeoutLeftNs);
#endif //ACR_RV_WPRT234_H