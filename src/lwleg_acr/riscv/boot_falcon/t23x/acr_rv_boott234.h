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
 * @file: acr_rv_boott234.h
 */

#ifndef ACR_RV_BOOTT234_H
#define ACR_RV_BOOTT234_H

#include "rv_acr.h"
#include "external/lwuproc.h"

#include <liblwriscv/dma.h>

/*
 * HAL definition.
 */
#define acrIsDebugModeEnabled_HAL()          \
        acrIsDebugModeEnabled_T234()
#define acrResetEngineFalcon_HAL(validIndexMap)          \
        acrResetEngineFalcon_T234(validIndexMap)
#define acrPollForResetCompletion_HAL(validIndexMap)     \
        acrPollForResetCompletion_T234(validIndexMap)
#define acrSetupLSFalcon_HAL(pLsbHeaders, validIndexMap) \
        acrSetupLSFalcon_T234(pLsbHeaders, validIndexMap)

/*
 * Function declarations.
 */
LwBool     acrIsDebugModeEnabled_T234(void);
ACR_STATUS acrPollForResetCompletion_T234(LwU32 validIndexMap);
ACR_STATUS acrResetEngineFalcon_T234(LwU32 validIndexMap);
ACR_STATUS acrSetupLSFalcon_T234(PLSF_LSB_HEADER_V2 pLsbHeaders, LwU32 validIndexMap);
ACR_STATUS acrBootstrapUcode(void);
#endif //ACR_RV_WPRT234_H
