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
 * @file: acr_rv_wprt234.h
 */

#ifndef ACR_RV_WPRT234_H
#define ACR_RV_WPRT234_H

#include "rv_acr.h"

/*
 * HAL definition.
 */
#define acrFindWprRegions_HAL(pWprIndex) \
        acrFindWprRegions_T234(pWprIndex)
#define acrReadWprHeader_HAL(void)       \
        acrReadWprHeader_T234(void)
#define acrWriteWprHeader_HAL(void)       \
        acrWriteWprHeader_T234(void)
#define acrCopyUcodesToWpr_HAL(void)     \
        acrCopyUcodesToWpr_T234(void)
#define acrProgramFalconSubWpr_HAL(falconId, flcnSubWprId, startAddr, endAddr, readMask, writeMask) \
        acrProgramFalconSubWpr_T234(falconId, flcnSubWprId, startAddr, endAddr, readMask, writeMask)
#define acrSetupFalconCodeAndDataSubWprs_HAL(falconId, pLsfHeader) \
        acrSetupFalconCodeAndDataSubWprs_T234(falconId, pLsfHeader)

/*
 * Function declarations.
 */
ACR_STATUS acrFindWprRegions_T234(LwU32 *pWprIndex);
ACR_STATUS acrReadWprHeader_T234(void);
ACR_STATUS acrWriteWprHeader_T234(void);
ACR_STATUS acrCopyUcodesToWpr_T234(void);
ACR_STATUS acrProgramFalconSubWpr_T234(LwU32 falconId, LwU8 flcnSubWprId, LwU32 startAddr, LwU32 endAddr, LwU8 readMask, LwU8 writeMask);
ACR_STATUS acrSetupFalconCodeAndDataSubWprs_T234(LwU32 falconId, PLSF_LSB_HEADER_V2 pLsfHeader);
#endif //ACR_RV_WPRT234_H
