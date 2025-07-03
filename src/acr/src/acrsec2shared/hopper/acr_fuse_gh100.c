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
 * @file: acr_fuse_gh100.c
 */
#include "acr.h"
#include "dev_falcon_v4.h"
#include "dev_fuse.h"
#include "dev_fuse_off.h"
#include "dev_fuse_addendum.h"
#include "dev_top.h"

#ifdef ACR_BUILD_ONLY
#include "config/g_acrlib_private.h"
#include "config/g_acr_private.h"
#else
#include "g_acrlib_private.h"
#endif

#if defined(LWDEC_PRESENT) || defined(LWDEC1_PRESENT) || defined(LWDEC_RISCV_PRESENT) || defined(LWDEC_RISCV_EB_PRESENT)
#include "dev_lwdec_pri.h"
#endif

#if defined(LWDEC_RISCV_PRESENT) || defined(LWDEC_RISCV_EB_PRESENT)
#include "dev_pri_ringstation_sys.h"
#include "dev_riscv_pri.h"

#endif

#ifdef LWJPG_PRESENT
#include "dev_lwjpg_pri_sw.h"
#include "dev_pri_ringstation_sys.h"
#endif

/*!
 * @brief Given fuse offset then return fue address
 *
 * @param[in]  fuseOffset  fuse offset address
 *
 * @return fuse register address on priv BUS
 */
ACR_STATUS
acrlibGetFuseRegAddress_GH100
(
    LwU32 fuseOffset,
    LwU32 *pFuseAddr
)
{
    *pFuseAddr = DRF_BASE(LW_FUSE) + (fuseOffset % DRF_BASE(LW_FUSE));

    return ACR_OK;;
}

/*!
 * @brief Given Falcon Id and Falcon instance to get if engine exist on HW
 *
 * @param[in]  falconId   Falcon LSF Id
 * @param[in]  instance   Associated instance with input Falcon Id
 * @param[out] *pStatusEn A pointer to save engine enabled status
 *
 * @return ACR_OK if no error
 */
ACR_STATUS
acrlibGetFalconInstanceStatus_GH100
(
    LwU32   falconId,
    LwU32   falconInstance,
    LwBool *pStatusEn
)
{
    // Fmodel seems Fmodel doesn't support these Fuse option.
#ifdef ACR_FMODEL_BUILD
    *pStatusEn = LW_TRUE;
    return ACR_OK;
#else
    LwU32            bitMask;
    LwU32            bitStart;
    LwU32            fuseOffset;
    LwU32            fuseAddr;
    LwU32            fuseData;
    ACR_STATUS       status = ACR_ERROR_ILWALID_OPERATION;

    switch (falconId)
    {
#ifdef LWDEC_RISCV_EB_PRESENT
        case LSF_FALCON_ID_LWDEC_RISCV_EB:
            fuseOffset = LW_FUSE_OFFSET_OPT_LWDEC_DISABLE;
            bitStart = DRF_BASE(LW_FUSE_OPT_LWDEC_DISABLE_DATA);
            bitMask  = DRF_MASK(LW_FUSE_OPT_LWDEC_DISABLE_DATA);
            break;
#endif

#ifdef LWJPG_PRESENT
        case LSF_FALCON_ID_LWJPG:
            fuseOffset = LW_FUSE_OFFSET_OPT_LWJPG_DISABLE;
            bitStart = DRF_BASE(LW_FUSE_OPT_LWJPG_DISABLE_DATA);
            bitMask  = DRF_MASK(LW_FUSE_OPT_LWJPG_DISABLE_DATA);
            break;
#endif
        default:
            *pStatusEn = LW_TRUE;
            return ACR_OK; 
    }

    CHECK_STATUS_AND_RETURN_IF_NOT_OK(acrlibGetFuseRegAddress_HAL(pAcrlib, fuseOffset, &fuseAddr));

    // Get the data
    fuseData = ACR_REG_RD32(BAR0, fuseAddr);

    fuseData = (fuseData >> bitStart) & bitMask;

    if (!(fuseData & BIT(falconInstance)))
    {
        *pStatusEn = LW_TRUE;
    }
    else
    {
        *pStatusEn = LW_FALSE;
    }

    return ACR_OK;
#endif // ACR_FMODEL_BUILD
}
