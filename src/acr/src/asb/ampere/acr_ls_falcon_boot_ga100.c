/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2019 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */
/*!
 * @file: acr_ls_falcon_boot_ga100.c
 */
#include "acr.h"
#include "hwproject.h"
#include "dev_lwdec_pri.h"

#ifdef ACR_BUILD_ONLY
#include "config/g_acrlib_private.h"
#include "config/g_acr_private.h"
#else
#include "g_acrlib_private.h"
#endif
/*!
 * @brief Check if falconInstance is valid
 */
LwBool
acrlibIsFalconInstanceValid_GA100
(
    LwU32  falconId,
    LwU32  falconInstance,
    LwBool bIsSmcActive
)
{
    switch (falconId)
    {
        case LSF_FALCON_ID_GPCCS:
            if (falconInstance < LW_SCAL_LITTER_MAX_NUM_SMC_ENGINES)
            {
                return LW_TRUE;
            }
            return LW_FALSE;
            break; 
        case LSF_FALCON_ID_FECS:
            if (bIsSmcActive)
            {
                if (falconInstance < LW_SCAL_LITTER_MAX_NUM_SMC_ENGINES)
                {
                    return LW_TRUE;
                }
                return LW_FALSE;
                break; 
            }
        default:
            if ((falconId < LSF_FALCON_ID_END) && (falconInstance == LSF_FALCON_INSTANCE_DEFAULT_0))
            {
                return LW_TRUE;
            }
            return LW_FALSE;
            break;
    }
}

/*!
 * @brief Check if count if GPC's is valid in case of Unicast boot scheme
 */
LwBool
acrlibIsFalconIndexMaskValid_GA100
(
    LwU32  falconId,
    LwU32  falconIndexMask
)
{
    switch (falconId)
    {
        case LSF_FALCON_ID_GPCCS:
            if (falconIndexMask <= (BIT(LSF_FALCON_INSTANCE_FECS_GPCCS_MAX)-1))
            {
                return LW_TRUE;
            }
            return LW_FALSE;
            break;
        default:
            if (falconIndexMask == LSF_FALCON_INDEX_MASK_DEFAULT_0)
            {
                return LW_TRUE;
            }
            return LW_FALSE;
            break;
    }
}

/*
 * @brief Enable AES HS mode for the HS falcon, if they have not colwerted to PKC based
 *          HS signing. See bug 2635738 for details.
 */
ACR_STATUS acrlibEnableAndSelectAES_GA100
(
    LwU32 falconId
)
{
    LwU32 val;

    // Only enable it on LWDEC0 falcon
    if (falconId == LSF_FALCON_ID_LWDEC)
    {
        // Enabling AES and RSA3K based HS boot
        val = ACR_REG_RD32(BAR0, LW_PLWDEC_FALCON_MOD_EN(0));
        val = FLD_SET_DRF(_PLWDEC_FALCON, _MOD_EN, _AES, _ENABLE, val);
        val = FLD_SET_DRF(_PLWDEC_FALCON, _MOD_EN, _RSA3K, _ENABLE, val);
        ACR_REG_WR32(BAR0,  LW_PLWDEC_FALCON_MOD_EN(0), val);

        // Forcing to select AES algorithm
        val = ACR_REG_RD32(BAR0, LW_PLWDEC_FALCON_MOD_SEL(0));
        val = FLD_SET_DRF(_PLWDEC_FALCON, _MOD_SEL, _ALGO, _AES, val);
        ACR_REG_WR32(BAR0, LW_PLWDEC_FALCON_MOD_SEL(0), val);
    }

    return ACR_OK;
}
