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
 * @file: acr_register_access_ad10x.c
 */

#include "acr.h"
#include "hwproject.h"
#include "acr_objacrlib.h"

#include "dev_top.h"
#include "dev_master.h"
#include "dev_fuse.h"
#include "dev_fuse_off.h"
#include "dev_fuse_addendum.h"

#include "dev_lwenc_pri_sw.h"
#include "dev_fb.h"
#include "dev_lwjpg_pri_sw.h"
#include "dev_pri_ringstation_sys.h"
#include "dev_lwdec_pri.h"

#ifdef LWJPG_PRESENT
/*!
 * @brief Given falcon ID and instance ID, get the falcon
 *        specifications such as registerBase, pmcEnableMask etc
 *
 * @param[in]  falconId   Falcon ID
 * @param[in]  falconInstance   Instance
 * @param[out] pFlcnCfg   Structure to hold falcon config
 *
 * @return ACR_OK if falconId and instance are valid
 */
ACR_STATUS
acrlibGetLwjpgFalconConfig_AD10X
(
    LwU32                falconId,
    LwU32                falconInstance,
    PACR_FLCN_CONFIG     pFlcnCfg
)
{

    ACR_STATUS status                = ACR_OK;
    pFlcnCfg->ctxDma                 = LSF_BOOTSTRAP_CTX_DMA_VIDEO;
    pFlcnCfg->registerBase           = DRF_BASE(LW_PLWJPG(falconInstance));
    pFlcnCfg->fbifBase               = LW_PLWJPG_FBIF_TRANSCFG(falconInstance,0);
    pFlcnCfg->subWprRangePlmAddr     = LW_PFB_PRI_MMU_LWJPG_PRIV_LEVEL_MASK;
    pFlcnCfg->maxSubWprSupportedByHw = LW_PFB_PRI_MMU_FALCON_LWJPG0_CFGA__SIZE_1;
    pFlcnCfg->bFbifPresent           = LW_TRUE;
    pFlcnCfg->bStackCfgSupported     = LW_TRUE;

    switch (falconInstance)
    {
        case 0:
            pFlcnCfg->subWprRangeAddrBase    = LW_PFB_PRI_MMU_FALCON_LWJPG0_CFGA(0);
            pFlcnCfg->targetMaskIndex        = LW_PPRIV_SYS_PRI_MASTER_fecs2lwjpg_pri0;
            pFlcnCfg->pmcEnableMask          = 1 << (DRF_BASE(LW_PTOP_DEVICE_INFO_RESET_FIELD_FOR_UCODE_AND_VBIOS_ONLY_LWJPG0)%32);
            pFlcnCfg->pmcEnableRegIdx        = (DRF_BASE(LW_PTOP_DEVICE_INFO_RESET_FIELD_FOR_UCODE_AND_VBIOS_ONLY_LWJPG0)/32);
            break;

        case 1:
            pFlcnCfg->subWprRangeAddrBase    = LW_PFB_PRI_MMU_FALCON_LWJPG1_CFGA(0);
            pFlcnCfg->targetMaskIndex        = LW_PPRIV_SYS_PRI_MASTER_fecs2lwjpg_pri1;
            pFlcnCfg->pmcEnableMask          = 1 << (DRF_BASE(LW_PTOP_DEVICE_INFO_RESET_FIELD_FOR_UCODE_AND_VBIOS_ONLY_LWJPG1)%32);
            pFlcnCfg->pmcEnableRegIdx        = (DRF_BASE(LW_PTOP_DEVICE_INFO_RESET_FIELD_FOR_UCODE_AND_VBIOS_ONLY_LWJPG1)/32);
            break;

        case 2:
            pFlcnCfg->subWprRangeAddrBase    = LW_PFB_PRI_MMU_FALCON_LWJPG2_CFGA(0);
            pFlcnCfg->targetMaskIndex      = LW_PPRIV_SYS_PRI_MASTER_fecs2lwjpg_pri2;
            pFlcnCfg->pmcEnableMask          = 1 << (DRF_BASE(LW_PTOP_DEVICE_INFO_RESET_FIELD_FOR_UCODE_AND_VBIOS_ONLY_LWJPG2)%32);
            pFlcnCfg->pmcEnableRegIdx        = (DRF_BASE(LW_PTOP_DEVICE_INFO_RESET_FIELD_FOR_UCODE_AND_VBIOS_ONLY_LWJPG2)/32);
            break;

        case 3:
            pFlcnCfg->subWprRangeAddrBase    = LW_PFB_PRI_MMU_FALCON_LWJPG3_CFGA(0);
            pFlcnCfg->targetMaskIndex        = LW_PPRIV_SYS_PRI_MASTER_fecs2lwjpg_pri3;
            pFlcnCfg->pmcEnableMask          = 1 << (DRF_BASE(LW_PTOP_DEVICE_INFO_RESET_FIELD_FOR_UCODE_AND_VBIOS_ONLY_LWJPG3)%32);
            pFlcnCfg->pmcEnableRegIdx        = (DRF_BASE(LW_PTOP_DEVICE_INFO_RESET_FIELD_FOR_UCODE_AND_VBIOS_ONLY_LWJPG3)/32);
            break;

        default:
	    status = ACR_ERROR_FLCN_ID_NOT_FOUND;
            break;
    }

    return status;
}
#endif // LWJPG_PRESENT

#ifdef LWENC_PRESENT
/*!
 * @brief Given falcon ID and instance ID, get the falcon
 *        specifications such as registerBase, pmcEnableMask etc
 *
 * @param[in]  falconId   Falcon ID
 * @param[in]  falconInstance   Instance.
 * @param[out] pFlcnCfg   Structure to hold falcon config
 *
 * @return ACR_OK if falconId and instance are valid
 */
ACR_STATUS
acrlibGetLwencFalconConfig_AD10X
(
    LwU32                falconId,
    LwU32                falconInstance,
    PACR_FLCN_CONFIG     pFlcnCfg
)
{
    ACR_STATUS status                = ACR_OK;

    pFlcnCfg->ctxDma                 = LSF_BOOTSTRAP_CTX_DMA_VIDEO;
    pFlcnCfg->registerBase           = DRF_BASE(LW_PLWENC(falconInstance));
    pFlcnCfg->fbifBase               = LW_PLWENC_FBIF_TRANSCFG(falconInstance,0);
    pFlcnCfg->subWprRangePlmAddr     = LW_PFB_PRI_MMU_LWENC_PRIV_LEVEL_MASK;
    pFlcnCfg->maxSubWprSupportedByHw = LW_PFB_PRI_MMU_FALCON_LWENC0_CFGA__SIZE_1;
    pFlcnCfg->bFbifPresent           = LW_TRUE;
    pFlcnCfg->bStackCfgSupported     = LW_TRUE;

    switch (falconInstance)
    {
        case 0:
            pFlcnCfg->subWprRangeAddrBase    = LW_PFB_PRI_MMU_FALCON_LWENC0_CFGA(0);
            pFlcnCfg->pmcEnableMask          = 1 << (DRF_BASE(LW_PTOP_DEVICE_INFO_RESET_FIELD_FOR_UCODE_AND_VBIOS_ONLY_LWENC0)%32);
            pFlcnCfg->pmcEnableRegIdx        = (DRF_BASE(LW_PTOP_DEVICE_INFO_RESET_FIELD_FOR_UCODE_AND_VBIOS_ONLY_LWENC0)/32);
            pFlcnCfg->targetMaskIndex        = LW_PPRIV_SYS_PRI_MASTER_fecs2lwenc_pri0;
            break;

        case 1:
            pFlcnCfg->subWprRangeAddrBase    = LW_PFB_PRI_MMU_FALCON_LWENC1_CFGA(0);
            pFlcnCfg->pmcEnableMask          = 1 << (DRF_BASE(LW_PTOP_DEVICE_INFO_RESET_FIELD_FOR_UCODE_AND_VBIOS_ONLY_LWENC1)%32);
            pFlcnCfg->pmcEnableRegIdx        = (DRF_BASE(LW_PTOP_DEVICE_INFO_RESET_FIELD_FOR_UCODE_AND_VBIOS_ONLY_LWENC1)/32);
            pFlcnCfg->targetMaskIndex        = LW_PPRIV_SYS_PRI_MASTER_fecs2lwenc_pri1;
            break;

        case 2:
            pFlcnCfg->subWprRangeAddrBase    = LW_PFB_PRI_MMU_FALCON_LWENC2_CFGA(0);
            pFlcnCfg->pmcEnableMask          = 1 << (DRF_BASE(LW_PTOP_DEVICE_INFO_RESET_FIELD_FOR_UCODE_AND_VBIOS_ONLY_LWENC2)%32);
            pFlcnCfg->pmcEnableRegIdx        = (DRF_BASE(LW_PTOP_DEVICE_INFO_RESET_FIELD_FOR_UCODE_AND_VBIOS_ONLY_LWENC2)/32);
            pFlcnCfg->targetMaskIndex        = LW_PPRIV_SYS_PRI_MASTER_fecs2lwenc_pri2;
            break;

        default:
	    status = ACR_ERROR_FLCN_ID_NOT_FOUND;
            break;

    }

    return status;
}
#endif // LWENC_PRESENT

/*!
 * @brief Given falcon ID and instance ID, get the falcon
 *        specifications such as registerBase, pmcEnableMask etc
 *
 * @param[in]  falconId   Falcon ID
 * @param[in]  falconInstance   Instance.
 * @param[out] pFlcnCfg   Structure to hold falcon config
 *
 * @return ACR_OK if falconId and instance are valid
 */
ACR_STATUS
acrlibGetLwdec1FalconConfig_AD10X
(
    LwU32                falconId,
    LwU32                falconInstance,
    PACR_FLCN_CONFIG     pFlcnCfg
)
{
    ACR_STATUS status                = ACR_OK;

    pFlcnCfg->ctxDma                 = LSF_BOOTSTRAP_CTX_DMA_VIDEO;
    pFlcnCfg->registerBase           = DRF_BASE(LW_PLWDEC(falconInstance));
    pFlcnCfg->fbifBase               = LW_PLWDEC_FBIF_TRANSCFG(falconInstance,0);
    pFlcnCfg->subWprRangePlmAddr     = LW_PFB_PRI_MMU_LWDEC_PRIV_LEVEL_MASK;
    pFlcnCfg->maxSubWprSupportedByHw = LW_PFB_PRI_MMU_FALCON_LWDEC1_CFGA__SIZE_1;
    pFlcnCfg->bFbifPresent           = LW_TRUE;
    pFlcnCfg->bStackCfgSupported     = LW_TRUE;

    switch (falconInstance)
    {
        case 1:
            pFlcnCfg->subWprRangeAddrBase    = LW_PFB_PRI_MMU_FALCON_LWDEC1_CFGA(0);
            pFlcnCfg->pmcEnableMask          = 1 << (DRF_BASE(LW_PTOP_DEVICE_INFO_RESET_FIELD_FOR_UCODE_AND_VBIOS_ONLY_LWDEC1)%32);
            pFlcnCfg->pmcEnableRegIdx        = (DRF_BASE(LW_PTOP_DEVICE_INFO_RESET_FIELD_FOR_UCODE_AND_VBIOS_ONLY_LWDEC1)/32);
            pFlcnCfg->targetMaskIndex        = LW_PPRIV_SYS_PRI_MASTER_fecs2lwdec_pri1;
            break;

        case 2:
            pFlcnCfg->subWprRangeAddrBase    = LW_PFB_PRI_MMU_FALCON_LWDEC2_CFGA(0);
            pFlcnCfg->pmcEnableMask          = 1 << (DRF_BASE(LW_PTOP_DEVICE_INFO_RESET_FIELD_FOR_UCODE_AND_VBIOS_ONLY_LWDEC2)%32);
            pFlcnCfg->pmcEnableRegIdx        = (DRF_BASE(LW_PTOP_DEVICE_INFO_RESET_FIELD_FOR_UCODE_AND_VBIOS_ONLY_LWDEC2)/32);
            pFlcnCfg->targetMaskIndex        = LW_PPRIV_SYS_PRI_MASTER_fecs2lwdec_pri2;
            break;

        case 3:
            pFlcnCfg->subWprRangeAddrBase    = LW_PFB_PRI_MMU_FALCON_LWDEC3_CFGA(0);
            pFlcnCfg->pmcEnableMask          = 1 << (DRF_BASE(LW_PTOP_DEVICE_INFO_RESET_FIELD_FOR_UCODE_AND_VBIOS_ONLY_LWDEC3)%32);
            pFlcnCfg->pmcEnableRegIdx        = (DRF_BASE(LW_PTOP_DEVICE_INFO_RESET_FIELD_FOR_UCODE_AND_VBIOS_ONLY_LWDEC3)/32);
            pFlcnCfg->targetMaskIndex        = LW_PPRIV_SYS_PRI_MASTER_fecs2lwdec_pri3;
            break;

        default:
	    status = ACR_ERROR_FLCN_ID_NOT_FOUND;
            break;

    }

    return status;
}

/*
* @brief: Return Falcon instance count on the current chip
* @param[in]  falconId         Falcon ID.
* @param[out] pInstanceCount   Falcon instance count on the current chip
*/	
ACR_STATUS
acrlibGetFalconInstanceCount_AD10X
(
    LwU32  falconId,
    LwU32 *pInstanceCount
)
{
    if (pInstanceCount == NULL)
    {
         return ACR_ERROR_ILWALID_ARGUMENT;
    }

#ifdef LWENC_PRESENT
    if (falconId == LSF_FALCON_ID_LWENC)
    {
        *pInstanceCount = LW_PLWENC__SIZE_1;
        return ACR_OK;
    }
#endif

#ifdef LWJPG_PRESENT
    if (falconId == LSF_FALCON_ID_LWJPG)
    {
        *pInstanceCount = LW_PLWJPG__SIZE_1;
        return ACR_OK;
    }
#endif

    if (falconId == LSF_FALCON_ID_LWDEC1)
    {
        *pInstanceCount = LW_PLWDEC__SIZE_1;
        return ACR_OK;
    }
    *pInstanceCount = LSF_FALCON_INSTANCE_COUNT_DEFAULT_1;
    return ACR_OK;
}

/*!
 * @brief Given fuse offset then return fuse address
 *
 * @param[in]  fuseOffset  fuse offset address
 *
 * @return fuse register address on priv BUS
 */
ACR_STATUS
acrlibGetFuseRegAddress_AD10X
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
acrlibGetFalconInstanceStatus_AD10X
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
#ifdef LWJPG_PRESENT
        case LSF_FALCON_ID_LWJPG:
            fuseOffset = LW_FUSE_OFFSET_OPT_LWJPG_DISABLE;
            bitStart = DRF_BASE(LW_FUSE_OPT_LWJPG_DISABLE_DATA);
            bitMask  = DRF_MASK(LW_FUSE_OPT_LWJPG_DISABLE_DATA);
            break;
#endif

#ifdef LWENC_PRESENT
        case LSF_FALCON_ID_LWENC:
            fuseOffset = LW_FUSE_OFFSET_OPT_LWENC_DISABLE;
            bitStart = DRF_BASE(LW_FUSE_OPT_LWENC_DISABLE_DATA);
            bitMask  = DRF_MASK(LW_FUSE_OPT_LWENC_DISABLE_DATA);
            break;
#endif

        case LSF_FALCON_ID_LWDEC1:
            fuseOffset = LW_FUSE_OFFSET_OPT_LWDEC_DISABLE;
            bitStart = DRF_BASE(LW_FUSE_OPT_LWDEC_DISABLE_DATA);
            bitMask  = DRF_MASK(LW_FUSE_OPT_LWDEC_DISABLE_DATA);
            break;

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

/*!
 * @brief Check if falconInstance is valid
 */
LwBool
acrlibIsFalconInstanceValid_AD10X
(
    LwU32  falconId,
    LwU32  falconInstance,
    LwBool bIsSmcActive
)
{
    switch (falconId)
    {

        case LSF_FALCON_ID_LWENC:
            if (falconInstance >= 0 && falconInstance < LW_PLWENC__SIZE_1)
            {
                return LW_TRUE;
            }
            break;

        case LSF_FALCON_ID_LWJPG:
            if (falconInstance >= 0 && falconInstance < LW_PLWJPG__SIZE_1)
            {
                return LW_TRUE;
            }
            break;

        case LSF_FALCON_ID_LWDEC1:
            if (falconInstance >= 1 && falconInstance < LW_PLWDEC__SIZE_1)
            {
                return LW_TRUE;
            }
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
