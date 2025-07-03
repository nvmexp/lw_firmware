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
 * @file: acr_register_access_tu10x_ga10x_only.c
 */
#include "acr.h"
#include "acrdrfmacros.h"
#include "dev_falcon_v4.h"
#include "dev_bus.h"
#include "dev_pwr_pri.h"
#include "dev_falcon_v4.h"
#include "dev_gsp.h"
#include "dev_fb.h"
#include "dev_master.h"
#include "dev_fbfalcon_pri.h"
#include "dev_sec_pri.h"
#include "dev_pwr_csb.h"
#include "dev_graphics_nobundle.h"
#include "dev_fbif_v4.h"
#include "sec2/sec2ifcmn.h"
#include "dpu/dpuifcmn.h"

#ifdef LWDEC_PRESENT
#include "dev_lwdec_pri.h"
#endif

#if defined(AHESASC) || defined(ASB) || defined(BSI_LOCK) || defined(ACR_UNLOAD_ON_SEC2)
#include "dev_sec_csb.h"
#endif

#ifdef ACR_BUILD_ONLY
#include "config/g_acrlib_private.h"
#include "config/g_acr_private.h"
#else
#include "g_acrlib_private.h"
#endif

/*!
 * @brief Given falcon ID and instance ID, get the falcon
 *        specifications such as registerBase, pmcEnableMask etc
 *
 * @param[in]  falconId   Falcon ID
 * @param[in]  falconInstance   Instance (either 0 or 1)
 * @param[out] pFlcnCfg   Structure to hold falcon config
 *
 * @return ACR_OK if falconId and instance are valid
 */
ACR_STATUS
acrlibGetFalconConfig_TU10X
(
    LwU32                falconId,
    LwU32                falconInstance,
    PACR_FLCN_CONFIG     pFlcnCfg
)
{
    ACR_STATUS  status  = ACR_OK;

    pFlcnCfg->falconId                       = falconId;
    pFlcnCfg->bIsBoOwner                     = LW_FALSE;
    pFlcnCfg->imemPLM                        = ACR_PLMASK_READ_L0_WRITE_L3;
    pFlcnCfg->dmemPLM                        = ACR_PLMASK_READ_L3_WRITE_L3;
    pFlcnCfg->bStackCfgSupported             = LW_FALSE;
    pFlcnCfg->pmcEnableMask                  = 0;
    pFlcnCfg->regSelwreResetAddr             = 0;
    pFlcnCfg->subWprRangeAddrBase            = ILWALID_REG_ADDR;
    pFlcnCfg->subWprRangePlmAddr             = ILWALID_REG_ADDR;
    pFlcnCfg->maxSubWprSupportedByHw         = 0;
    pFlcnCfg->registerBase                   = 0;
    pFlcnCfg->uprocType                      = ACR_TARGET_ENGINE_CORE_FALCON;

    switch (falconId)
    {
        case LSF_FALCON_ID_PMU:
            pFlcnCfg->registerBase           = LW_PPWR_FALCON_IRQSSET;
            pFlcnCfg->fbifBase               = LW_PPWR_FBIF_TRANSCFG(0);
            pFlcnCfg->bOpenCarve             = LW_TRUE;
            pFlcnCfg->bFbifPresent           = LW_TRUE;
            pFlcnCfg->ctxDma                 = RM_PMU_DMAIDX_UCODE;
#if defined(ACR_UNLOAD)
            pFlcnCfg->range0Addr             = LW_CPWR_FALCON_DMEM_PRIV_RANGE0;
            pFlcnCfg->range1Addr             = LW_CPWR_FALCON_DMEM_PRIV_RANGE1;
            pFlcnCfg->bIsBoOwner             = LW_TRUE; // PMU is the Bootstrap Owner
#else
            pFlcnCfg->range0Addr             = LW_PPWR_FALCON_DMEM_PRIV_RANGE0;
            pFlcnCfg->range1Addr             = LW_PPWR_FALCON_DMEM_PRIV_RANGE1;
            pFlcnCfg->regSelwreResetAddr     = LW_PPWR_FALCON_ENGINE;
#endif
            // Allow read of PMU DMEM to level 0 for debug of PMU code
            pFlcnCfg->dmemPLM                = ACR_PLMASK_READ_L0_WRITE_L2;
            pFlcnCfg->bStackCfgSupported     = LW_TRUE;
            pFlcnCfg->subWprRangeAddrBase    = LW_PFB_PRI_MMU_FALCON_PMU_CFGA(0);
            pFlcnCfg->subWprRangePlmAddr     = LW_PFB_PRI_MMU_PMU_PRIV_LEVEL_MASK;
            pFlcnCfg->maxSubWprSupportedByHw = LW_PFB_PRI_MMU_FALCON_PMU_CFGA__SIZE_1;
            break;

#ifdef SEC2_PRESENT
        case LSF_FALCON_ID_SEC2:
            pFlcnCfg->registerBase           = LW_PSEC_FALCON_IRQSSET;
            pFlcnCfg->fbifBase               = LW_PSEC_FBIF_TRANSCFG(0);
            pFlcnCfg->range0Addr             = 0;
            pFlcnCfg->range1Addr             = 0;
            pFlcnCfg->bOpenCarve             = LW_FALSE;
            pFlcnCfg->bFbifPresent           = LW_TRUE;
            pFlcnCfg->ctxDma                 = RM_SEC2_DMAIDX_UCODE;
            pFlcnCfg->bStackCfgSupported     = LW_TRUE;
#if defined(AHESASC) || defined(BSI_LOCK) || defined(ACR_UNLOAD_ON_SEC2)
            pFlcnCfg->bIsBoOwner             = LW_TRUE;
#endif
            pFlcnCfg->pmcEnableMask          = DRF_DEF(_PMC, _ENABLE, _SEC, _ENABLED);
            pFlcnCfg->regSelwreResetAddr     = LW_PSEC_FALCON_ENGINE;
            pFlcnCfg->subWprRangeAddrBase    = LW_PFB_PRI_MMU_FALCON_SEC_CFGA(0);
            pFlcnCfg->subWprRangePlmAddr     = LW_PFB_PRI_MMU_SEC_PRIV_LEVEL_MASK;
            pFlcnCfg->maxSubWprSupportedByHw = LW_PFB_PRI_MMU_FALCON_SEC_CFGA__SIZE_1;
            break;
#endif

#ifndef ACR_ONLY_BO
        case LSF_FALCON_ID_FBFALCON:
            pFlcnCfg->registerBase           = LW_FALCON_FBFALCON_BASE;
            pFlcnCfg->fbifBase               = LW_PFBFALCON_FBIF_TRANSCFG(0);
            pFlcnCfg->range0Addr             = LW_PFBFALCON_FALCON_DMEM_PRIV_RANGE0;
            pFlcnCfg->range1Addr             = LW_PFBFALCON_FALCON_DMEM_PRIV_RANGE1;
            pFlcnCfg->bOpenCarve             = LW_TRUE;
            pFlcnCfg->bFbifPresent           = LW_TRUE;
            pFlcnCfg->ctxDma                 = LSF_BOOTSTRAP_CTX_DMA_FBFLCN;
            pFlcnCfg->regSelwreResetAddr     = LW_PFBFALCON_FALCON_ENGINE;
            pFlcnCfg->subWprRangeAddrBase    = LW_PFB_PRI_MMU_FALCON_FBFALCON_CFGA(0);
            pFlcnCfg->subWprRangePlmAddr     = LW_PFB_PRI_MMU_FBFALCON_PRIV_LEVEL_MASK;
            pFlcnCfg->maxSubWprSupportedByHw = LW_PFB_PRI_MMU_FALCON_FBFALCON_CFGA__SIZE_1;
            break;
        case LSF_FALCON_ID_FECS:
            // ENGINE GR shouldnt be reset
            pFlcnCfg->pmcEnableMask          = 0;
            pFlcnCfg->registerBase           = LW_PGRAPH_PRI_FECS_FALCON_IRQSSET;
            pFlcnCfg->fbifBase               = 0;
            pFlcnCfg->range0Addr             = 0;
            pFlcnCfg->range1Addr             = 0;
            pFlcnCfg->bOpenCarve             = LW_FALSE;
            pFlcnCfg->bFbifPresent           = LW_FALSE;
            // RegCfgAddr and RegCfgMaskAddr are needed only for falcons with no FBIF
            pFlcnCfg->regCfgAddr             = LW_PGRAPH_PRI_FECS_ARB_FALCON_REGIONCFG;
            pFlcnCfg->regCfgMaskAddr         = LW_PGRAPH_PRI_FECS_ARB_FALCON_REGIONCFG_PRIV_LEVEL_MASK;
            pFlcnCfg->ctxDma                 = LSF_BOOTSTRAP_CTX_DMA_FECS;
            pFlcnCfg->subWprRangeAddrBase    = LW_PFB_PRI_MMU_FALCON_FECS_CFGA(0);
            pFlcnCfg->subWprRangePlmAddr     = LW_PFB_PRI_MMU_FECS_PRIV_LEVEL_MASK;
            pFlcnCfg->maxSubWprSupportedByHw = LW_PFB_PRI_MMU_FALCON_FECS_CFGA__SIZE_1;
            break;
        case LSF_FALCON_ID_GPCCS:
            // ENGINE GR shouldnt be reset
            pFlcnCfg->pmcEnableMask          = 0;
            pFlcnCfg->registerBase           = LW_PGRAPH_PRI_GPCS_GPCCS_FALCON_IRQSSET;
            pFlcnCfg->fbifBase               = 0;
            pFlcnCfg->range0Addr             = 0;
            pFlcnCfg->range1Addr             = 0;
            pFlcnCfg->bOpenCarve             = LW_FALSE;
            pFlcnCfg->bFbifPresent           = LW_FALSE;
            // RegCfgAddr and RegCfgMaskAddr are needed only for falcons with no FBIF
            pFlcnCfg->regCfgAddr             = LW_PGRAPH_PRI_GPCS_GPCCS_ARB_FALCON_REGIONCFG;
            pFlcnCfg->regCfgMaskAddr         = LW_PGRAPH_PRI_GPCS_GPCCS_ARB_FALCON_REGIONCFG_PRIV_LEVEL_MASK;
            pFlcnCfg->ctxDma                 = LSF_BOOTSTRAP_CTX_DMA_FECS;
            pFlcnCfg->maxSubWprSupportedByHw = LW_PFB_PRI_MMU_FALCON_GPCCS_CFGA__SIZE_1;
            break;
#endif // ACR_ONLY_BO

#ifdef LWDEC_PRESENT
        case LSF_FALCON_ID_LWDEC:
            pFlcnCfg->subWprRangeAddrBase    = LW_PFB_PRI_MMU_FALCON_LWDEC0_CFGA(0);
            pFlcnCfg->subWprRangePlmAddr     = LW_PFB_PRI_MMU_LWDEC_PRIV_LEVEL_MASK;
            pFlcnCfg->maxSubWprSupportedByHw = LW_PFB_PRI_MMU_FALCON_LWDEC0_CFGA__SIZE_1;

            acrlibGetLwdecConfig_HAL(pAcrlib, pFlcnCfg);
            break;
#endif

#ifdef GSP_PRESENT
#ifdef DPU_PRESENT
            // Control is not supposed to come here. DPU and GSPlite both should not be present at the same time.
            ct_assert(0);
#endif
        case LSF_FALCON_ID_GSPLITE:
#if defined(ASB)
            pFlcnCfg->bIsBoOwner             = LW_TRUE;
#endif
            pFlcnCfg->pmcEnableMask          = 0;
            pFlcnCfg->registerBase           = LW_PGSP_FALCON_IRQSSET;
            pFlcnCfg->fbifBase               = LW_PGSP_FBIF_TRANSCFG(0);
            pFlcnCfg->range0Addr             = 0;
            pFlcnCfg->range1Addr             = 0;
            pFlcnCfg->bOpenCarve             = LW_FALSE;
            pFlcnCfg->bFbifPresent           = LW_TRUE;
            pFlcnCfg->ctxDma                 = RM_GSPLITE_DMAIDX_UCODE;
            pFlcnCfg->bStackCfgSupported     = LW_TRUE;
            pFlcnCfg->regSelwreResetAddr     = LW_PGSP_FALCON_ENGINE;
            pFlcnCfg->subWprRangeAddrBase    = LW_PFB_PRI_MMU_FALCON_GSP_CFGA(0);
            pFlcnCfg->subWprRangePlmAddr     = LW_PFB_PRI_MMU_GSP_PRIV_LEVEL_MASK;
            pFlcnCfg->maxSubWprSupportedByHw = LW_PFB_PRI_MMU_FALCON_GSP_CFGA__SIZE_1;
            break;
#endif // GSP_PRESENT
        default:
            status = ACR_ERROR_FLCN_ID_NOT_FOUND;
    }

    return status;
}

#ifdef LWDEC_PRESENT
/*!
 * @brief Get the Lwenc2 falcon specifications such as registerBase, pmcEnableMask etc
 *
 * @param[out] pFlcnCfg   Structure to hold falcon config
 */
void
acrlibGetLwdecConfig_TU10X
(
    PACR_FLCN_CONFIG     pFlcnCfg
)
{
    pFlcnCfg->pmcEnableMask = DRF_DEF(_PMC, _ENABLE, _LWDEC, _ENABLED);
    pFlcnCfg->registerBase  = LW_FALCON_LWDEC_BASE;
    pFlcnCfg->fbifBase      = LW_PLWDEC_FBIF_TRANSCFG(0,0);
    pFlcnCfg->range0Addr    = 0;
    pFlcnCfg->range1Addr    = 0;
    pFlcnCfg->bOpenCarve    = LW_FALSE;
    pFlcnCfg->bFbifPresent  = LW_TRUE;
    pFlcnCfg->ctxDma        = LSF_BOOTSTRAP_CTX_DMA_VIDEO;
    pFlcnCfg->bStackCfgSupported = LW_TRUE;
}
#endif
