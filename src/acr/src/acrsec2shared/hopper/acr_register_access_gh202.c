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
 * @file: acr_register_access_gh202.c
 */
#include "acr.h"
#include "hwproject.h"
#include "dev_pri_ringstation_sys.h"

#if LW_SCAL_LITTER_NUM_SYSB != 0
#include "dev_pri_ringstation_sysb.h"
#endif

#if LW_SCAL_LITTER_NUM_SYSC != 0
#include "dev_pri_ringstation_sysc.h"
#endif

#ifdef ACR_BUILD_ONLY
#include "config/g_acrlib_private.h"
#include "config/g_acr_private.h"
#else
#include "g_acrlib_private.h"
#endif


/*!
 * @brief Get the target mask index for FECS engine
 * @param[in] falconInstance    : Engine instance id to be used for SMC use case
 * @param[out] pTargetMaskIndex : Target Mask index to be used
 * @returns ACR_STATUS, ACR_OK if sucessfull else ERROR
 */
ACR_STATUS
acrlibGetFecsTargetMaskIndex_GH202
(
    LwU32 falconInstance, 
    LwU32 *pTargetMaskIndex
)
{
    if (pTargetMaskIndex == NULL)
    {
        return ACR_ERROR_ILWALID_ARGUMENT;
    }
    
    *pTargetMaskIndex = LW_PPRIV_SYS_PRI_MASTER_sys_pri_hub2fecs_pri0; 
    return ACR_OK;
}
