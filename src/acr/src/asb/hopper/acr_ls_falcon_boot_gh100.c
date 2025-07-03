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
 * @file: acr_ls_falcon_boot_gh100.c
 */

#include "acr.h"
#include "dev_falcon_v4.h"
#include "dev_lwdec_pri.h"
#include "hwproject.h"

#if defined(LWDEC_RISCV_PRESENT) || defined(LWDEC_RISCV_EB_PRESENT)
#include "dev_lwdec_pri.h"
#endif

#ifdef LWJPG_PRESENT
#include "dev_lwjpg_pri_sw.h"
#endif

#ifdef ACR_BUILD_ONLY
#include "config/g_acrlib_private.h"
#include "config/g_acr_private.h"
#else
#include "g_acrlib_private.h"
#endif


#define ACR_TIMEOUT_FOR_RESET (10 * 1000) // 10 us

/*!
 * @brief Put the given falcon into reset state
 */
ACR_STATUS
acrlibAssertEngineReset_GH100
(
    PACR_FLCN_CONFIG pFlcnCfg
)
{

    ACR_TIMESTAMP startTimeNs;
    LwU32         data          = 0;
    LwS32         timeoutLeftNs = 0;
    ACR_STATUS    status        = ACR_OK;

    if (pFlcnCfg == NULL)
    {
        return ACR_ERROR_ILWALID_ARGUMENT;
    }


    if (pFlcnCfg->regSelwreResetAddr == 0)
    {
        return ACR_ERROR_ILWALID_RESET_ADDR;
    }

    data = FLD_SET_DRF(_PFALCON, _FALCON_ENGINE, _RESET, _ASSERT, data);
    ACR_REG_WR32(BAR0, pFlcnCfg->regSelwreResetAddr, data);

    acrlibGetLwrrentTimeNs_HAL(pAcrlib, &startTimeNs);
    do
    {
        CHECK_STATUS_AND_RETURN_IF_NOT_OK(acrlibCheckTimeout_HAL(pAcrlib, ACR_TIMEOUT_FOR_RESET,
                                                   startTimeNs, &timeoutLeftNs));
        data = ACR_REG_RD32(BAR0, pFlcnCfg->regSelwreResetAddr);
    }while(!FLD_TEST_DRF(_PFALCON, _FALCON_ENGINE, _RESET_STATUS, _ASSERTED, data));

    return ACR_OK;
}

/*!
 * @brief Bring the given falcon out of reset state
 */
ACR_STATUS
acrlibDeassertEngineReset_GH100
(
    PACR_FLCN_CONFIG pFlcnCfg
)
{

    ACR_TIMESTAMP startTimeNs;
    LwU32         data          = 0;
    LwS32         timeoutLeftNs = 0;
    ACR_STATUS    status        = ACR_OK;

    if (pFlcnCfg == NULL)
    {
        return ACR_ERROR_ILWALID_ARGUMENT;
    }

    if (pFlcnCfg->regSelwreResetAddr == 0)
    {
        return ACR_ERROR_ILWALID_RESET_ADDR;
    }

    data = FLD_SET_DRF(_PFALCON, _FALCON_ENGINE, _RESET, _DEASSERT, data);
    ACR_REG_WR32(BAR0, pFlcnCfg->regSelwreResetAddr, data);

    acrlibGetLwrrentTimeNs_HAL(pAcrlib, &startTimeNs);
    do
    {
        CHECK_STATUS_AND_RETURN_IF_NOT_OK(acrlibCheckTimeout_HAL(pAcrlib, ACR_TIMEOUT_FOR_RESET,
                                                   startTimeNs, &timeoutLeftNs));
        data = ACR_REG_RD32(BAR0, pFlcnCfg->regSelwreResetAddr);
    }while(!FLD_TEST_DRF(_PFALCON, _FALCON_ENGINE, _RESET_STATUS, _DEASSERTED, data));

    return ACR_OK;
}

/*!
 * @brief Check if falconInstance is valid
 */
LwBool
acrlibIsFalconInstanceValid_GH100
(
    LwU32  falconId,
    LwU32  falconInstance,
    LwBool bIsSmcActive
)
{
    switch (falconId)
    {

#ifdef LWDEC_RISCV_EB_PRESENT
        case LSF_FALCON_ID_LWDEC_RISCV_EB:
            if (falconInstance >= 0 && falconInstance < (LW_PLWDEC__SIZE_1 - 1))
            {
                return LW_TRUE;
            }
            break;
#endif

#ifdef LWJPG_PRESENT
        case LSF_FALCON_ID_LWJPG:
            if (falconInstance >= 0 && falconInstance < LW_PLWJPG__SIZE_1)
            {
                return LW_TRUE;
            }
            break;
#endif

        case LSF_FALCON_ID_FECS:
        case LSF_FALCON_ID_GPCCS:
            if (falconInstance < LW_SCAL_LITTER_MAX_NUM_SMC_ENGINES)
            {
                return LW_TRUE;
            }
            return LW_FALSE;
            break;

        default:
            if ((falconId < LSF_FALCON_ID_END) && (falconInstance == LSF_FALCON_INSTANCE_DEFAULT_0))
            {
                return LW_TRUE;
            }
            break;
    }

    return LW_FALSE;
}

