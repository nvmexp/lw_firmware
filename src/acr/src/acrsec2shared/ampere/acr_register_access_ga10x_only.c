/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2020-2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */
/*!
 * @file: acr_register_access_ga10x_only.c
 */
#include "acr.h"
#include "dev_pri_ringstation_sys.h"

#ifdef ACR_BUILD_ONLY
#include "config/g_acrlib_private.h"
#include "config/g_acr_private.h"
#else
#include "g_acrlib_private.h"
#endif

/*!
 * @brief Get the target mask index for FbFalcon for GA10X
 */
ACR_STATUS
acrlibGetFbFalconTargetMaskIndex_GA10X(LwU32 *pTargetMaskIndex)
{
    if (pTargetMaskIndex == NULL)
    {
        return ACR_ERROR_ILWALID_ARGUMENT;
    }

    *pTargetMaskIndex = LW_PPRIV_SYS_PRI_MASTER_fecs2fbfalcon_pri;
    return ACR_OK;
}

/*!
 * @brief Get the target mask index for LWDEC
 */
ACR_STATUS
acrlibGetLwdecTargetMaskIndex_GA10X
(
    PACR_FLCN_CONFIG pFlcnCfg,
    LwU32 *pTargetMaskIndex
)
{
    if (pTargetMaskIndex == NULL || pFlcnCfg == NULL)
    {
        return ACR_ERROR_ILWALID_ARGUMENT;
    }

    if (pFlcnCfg->falconId == LSF_FALCON_ID_LWDEC && pFlcnCfg->falconInstance == LSF_FALCON_INSTANCE_DEFAULT_0)
    {
        *pTargetMaskIndex = LW_PPRIV_SYS_PRI_MASTER_fecs2lwdec_pri0;
    }
    else if (pFlcnCfg->falconId == LSF_FALCON_ID_LWDEC1 && pFlcnCfg->falconInstance == LSF_FALCON_INSTANCE_DEFAULT_0)
    {
        *pTargetMaskIndex = LW_PPRIV_SYS_PRI_MASTER_fecs2lwdec_pri1;
    }
    else
    {
        return ACR_ERROR_ILWALID_ARGUMENT;
    }

    return ACR_OK;
}

