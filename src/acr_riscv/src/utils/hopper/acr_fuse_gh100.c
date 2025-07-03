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
#include "dev_fuse.h"
#include "dev_fuse_off.h"
#include "dev_fuse_addendum.h"
#include "dev_top.h"

#include "config/g_acr_private.h"

#if defined(LWDEC_PRESENT) || defined(LWDEC1_PRESENT) || defined(LWDEC_RISCV_PRESENT) || defined(LWDEC_RISCV_EB_PRESENT)
#include "dev_lwdec_pri.h"
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
acrGetFuseRegAddress_GH100
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
acrGetFalconInstanceStatus_GH100
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
#ifdef LWDEC_RISCV_PRESENT
        case LSF_FALCON_ID_LWDEC_RISCV:
            // Only falconInstance 0 is supported for LWDEC_RISCV in GH100.
            if (falconInstance != 0)
            {                
                return ACR_ERROR_ILWALID_ARGUMENT;
            }
            fuseOffset = LW_FUSE_OFFSET_STATUS_OPT_LWDEC;
            bitStart   = DRF_BASE(LW_FUSE_STATUS_OPT_LWDEC_DATA);
            bitMask    = DRF_MASK(LW_FUSE_STATUS_OPT_LWDEC_DATA);
            break;
#endif

#ifdef LWDEC_RISCV_EB_PRESENT
        case LSF_FALCON_ID_LWDEC_RISCV_EB:
            // falconInstance propogated from RM for LWDEC-RISCV-EB is from Instance 0 ~ 6
            if(falconInstance > (LW_PLWDEC__SIZE_1 - 2))
            {
                return ACR_ERROR_ILWALID_ARGUMENT;   
            }
            // LWDEC-RISCV-EB is from LWDEC 1 ~ 7, so instance = 0, we need to check LWDEC 1 in fuse register.  
            falconInstance = falconInstance + 1;
            fuseOffset = LW_FUSE_OFFSET_STATUS_OPT_LWDEC;
            bitStart = DRF_BASE(LW_FUSE_STATUS_OPT_LWDEC_DATA);
            bitMask  = DRF_MASK(LW_FUSE_STATUS_OPT_LWDEC_DATA);
            break;
#endif

#ifdef LWJPG_PRESENT
        case LSF_FALCON_ID_LWJPG:
            fuseOffset = LW_FUSE_OFFSET_STATUS_OPT_LWJPG;
            bitStart = DRF_BASE(LW_FUSE_STATUS_OPT_LWJPG_DATA);
            bitMask  = DRF_MASK(LW_FUSE_STATUS_OPT_LWJPG_DATA);
            break;
#endif
        case LSF_FALCON_ID_FECS:
            fuseOffset = LW_FUSE_OFFSET_STATUS_OPT_SYS_PIPE;
            bitStart = DRF_BASE(LW_FUSE_OPT_SYS_PIPE_DISABLE_DATA);
            bitMask  = DRF_MASK(LW_FUSE_OPT_SYS_PIPE_DISABLE_DATA);
            break;
        case LSF_FALCON_ID_GPCCS:
            fuseOffset = LW_FUSE_OFFSET_STATUS_OPT_GPC;
            bitStart = DRF_BASE(LW_FUSE_OPT_GPC_DISABLE_DATA);
            bitMask  = DRF_MASK(LW_FUSE_OPT_GPC_DISABLE_DATA);
            break;
        default:
            *pStatusEn = LW_TRUE;
            return ACR_OK; 
    }

    CHECK_STATUS_AND_RETURN_IF_NOT_OK(acrGetFuseRegAddress_HAL(pAcr, fuseOffset, &fuseAddr));

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
