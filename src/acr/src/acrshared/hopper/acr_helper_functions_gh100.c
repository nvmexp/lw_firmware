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
 * @file: acr_helper_functions_gh100.c
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


/*
* @brief: Returns the HW state of MMU secure lock and if WPR is allowed with secure lock enabled
*/
ACR_STATUS
acrGetMmuSelwreLockStateFromHW_GH100
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
    *pbIsMmuSelwreLockEnabledInHW     = FLD_TEST_DRF(_PFB, _PRI_MMU_WPR_MODE, _SELWRE_LOCK, _TRUE,
                                            ACR_REG_RD32(BAR0, LW_PFB_PRI_MMU_WPR_MODE));

    *pbIsWprAllowedWithSelwreLockInHW = FLD_TEST_DRF(_PFB, _PRI_MMU_WPR_MODE, _WPR_WITH_SELWRE_LOCK, _TRUE,
                                            ACR_REG_RD32(BAR0, LW_PFB_PRI_MMU_WPR_MODE));

    return ACR_OK;
}
