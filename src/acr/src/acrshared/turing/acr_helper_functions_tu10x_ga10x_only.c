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
 * @file: acr_helper_functions_tu10x_ga10x_only.c
 */
//
// Includes
//
#include "acr.h"
#include "acrdrfmacros.h"
#include "acr_objacr.h"

#include "dev_fb.h"
#include "config/g_acr_private.h"
#include "config/g_acrlib_private.h"
#include "dev_fuse.h"
/*
* @brief: Returns the HW state of MMU secure lock and if WPR is allowed with secure lock enabled
*/
ACR_STATUS
acrGetMmuSelwreLockStateFromHW_TU10X
(
    LwBool *pbIsMmuSelwreLockEnabledInHW, 
    LwBool *pbIsWprAllowedWithSelwreLockInHW
)
{
    if (NULL == pbIsMmuSelwreLockEnabledInHW ||
        NULL == pbIsWprAllowedWithSelwreLockInHW)
    {
        return ACR_ERROR_ILWALID_ARGUMENT;
    }
    *pbIsMmuSelwreLockEnabledInHW     = FLD_TEST_DRF(_PFB, _PRI_MMU_VPR_MODE, _SELWRE_LOCK, _TRUE,
                                            ACR_REG_RD32(BAR0, LW_PFB_PRI_MMU_VPR_MODE));

    *pbIsWprAllowedWithSelwreLockInHW = FLD_TEST_DRF(_PFB, _PRI_MMU_VPR_MODE, _WPR_WITH_SELWRE_LOCK, _TRUE,
                                            ACR_REG_RD32(BAR0, LW_PFB_PRI_MMU_VPR_MODE));

    return ACR_OK;
}

/*!
 * @brief Get the HW fuse version
 */
ACR_STATUS
acrGetUcodeFuseVersion_TU10X(LwU32* pUcodeFuseVersion)
{
    *pUcodeFuseVersion = ACR_REG_RD32(BAR0, LW_FUSE_OPT_FUSE_UCODE_ACR_HS_REV);
    return ACR_OK;
}

/*!
 * @brief Check the SW fuse version revocation against HW fuse version
 */
ACR_STATUS
acrCheckFuseRevocationAgainstHWFuseVersion_TU10X(LwU32 ucodeBuildVersion)
{
    ACR_STATUS  status           = ACR_OK;
    LwU32       ucodeFuseVersion = 0;

    status = acrGetUcodeFuseVersion_HAL(pAcr, &ucodeFuseVersion);
    if(status != ACR_OK)
    {
        return ACR_ERROR_ILWALID_CHIP_ID;
    }

    if (ucodeBuildVersion < ucodeFuseVersion)
    {
        // Halt if this ucode is not supposed to run in this chip
        return ACR_ERROR_UCODE_REVOKED;
    }

    return ACR_OK;
}
