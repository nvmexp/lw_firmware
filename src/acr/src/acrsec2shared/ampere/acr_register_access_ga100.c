/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2019-2020 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */
/*!
 * @file: acr_register_access_ga100.c
 */
#include "acr.h"
#include "dev_falcon_v4.h"
#include "dev_gsp.h"
#include "dev_fb.h"
#include "dev_graphics_nobundle.h"
#include "dev_pwr_pri.h"
#include "dev_pwr_csb.h"
#include "dev_sec_pri.h"
#include "dev_fbfalcon_pri.h"
#include "dev_smcarb.h"
#include "dev_smcarb_addendum.h"
#include "hwproject.h"
#include "dev_top.h"
#include "sec2/sec2ifcmn.h"
#include "dpu/dpuifcmn.h"
#include "dev_pri_ringmaster.h"
#include "dev_pri_ringstation_control_gpc.h"
#include "dev_riscv_pri.h"

#ifdef ACR_RISCV_LS
#include "gsp/gspifcmn.h"
#endif

#if defined(AHESASC) || defined(ASB) || defined(BSI_LOCK) || defined(ACR_UNLOAD_ON_SEC2)
#include "dev_sec_csb.h"
#endif

#ifdef LWDEC_PRESENT
#include "dev_lwdec_pri.h"
#endif

#ifdef ACR_BUILD_ONLY
#include "config/g_acrlib_private.h"
#include "config/g_acr_private.h"
#else
#include "g_acrlib_private.h"
#endif

#define SMC_LEGACY_UNICAST_ADDR(addr, instance) (addr + (LW_GPC_PRI_STRIDE * instance))

/*!
 * @brief Given falcon ID and instance ID, get the falcon
 *        specifications such as registerBase, pmcEnableMask etc
 *
 * @param[in]  falconId                      Falcon ID
 * @param[in]  falconInstance                Falcon index/Instance of same falcon
 * @param[out] pFlcnCfg                      Structure to hold falcon config
 *
 * @return ACR_OK if falconId and instance are valid
 */
ACR_STATUS
acrlibGetFalconConfig_GA100
(
    LwU32                falconId,
    LwU32                falconInstance,
    PACR_FLCN_CONFIG     pFlcnCfg
)
{
    ACR_STATUS  status  = ACR_OK;

    pFlcnCfg->falconId               = falconId;
    pFlcnCfg->falconInstance         = falconInstance;
    pFlcnCfg->bIsBoOwner             = LW_FALSE;
    pFlcnCfg->imemPLM                = ACR_PLMASK_READ_L3_WRITE_L3;
    pFlcnCfg->dmemPLM                = ACR_PLMASK_READ_L3_WRITE_L3;
    pFlcnCfg->bStackCfgSupported     = LW_FALSE;
    pFlcnCfg->pmcEnableMask          = 0;
    pFlcnCfg->regSelwreResetAddr     = 0;
    pFlcnCfg->subWprRangeAddrBase    = ILWALID_REG_ADDR;
    pFlcnCfg->subWprRangePlmAddr     = ILWALID_REG_ADDR;
    pFlcnCfg->range0Addr             = 0;
    pFlcnCfg->range1Addr             = 0;
    pFlcnCfg->regCfgAddr             = 0;
    pFlcnCfg->regCfgMaskAddr         = 0;
    pFlcnCfg->registerBase           = 0;
    pFlcnCfg->fbifBase               = 0;
    pFlcnCfg->riscvRegisterBase      = 0;
    pFlcnCfg->pmcEnableRegIdx        = 0;
    pFlcnCfg->ctxDma                 = 0;
    pFlcnCfg->maxSubWprSupportedByHw = 0;
    pFlcnCfg->bOpenCarve             = LW_FALSE;
    pFlcnCfg->bFbifPresent           = LW_FALSE;
    pFlcnCfg->uprocType              = ACR_TARGET_ENGINE_CORE_FALCON;

    switch (falconId)
    {
#ifdef ACR_RISCV_LS
        case LSF_FALCON_ID_PMU_RISCV:
            pFlcnCfg->subWprRangeAddrBase = LW_PFB_PRI_MMU_FALCON_PMU_CFGA(0);
            pFlcnCfg->subWprRangePlmAddr  = LW_PFB_PRI_MMU_PMU_PRIV_LEVEL_MASK;
            pFlcnCfg->registerBase        = LW_PPWR_FALCON_IRQSSET;
            pFlcnCfg->fbifBase            = LW_PPWR_FBIF_TRANSCFG(0);
            pFlcnCfg->bOpenCarve          = LW_TRUE;
            pFlcnCfg->bFbifPresent        = LW_TRUE;
            pFlcnCfg->ctxDma              = RM_PMU_DMAIDX_UCODE;
#if defined(ACR_UNLOAD)
            pFlcnCfg->range0Addr          = LW_CPWR_FALCON_DMEM_PRIV_RANGE0;
            pFlcnCfg->range1Addr          = LW_CPWR_FALCON_DMEM_PRIV_RANGE1;
#else
            pFlcnCfg->range0Addr          = LW_PPWR_FALCON_DMEM_PRIV_RANGE0;
            pFlcnCfg->range1Addr          = LW_PPWR_FALCON_DMEM_PRIV_RANGE1;
            pFlcnCfg->regSelwreResetAddr  = LW_PPWR_FALCON_ENGINE;
#endif
            //
            // WARNING WARNING WARNING WARNING WARNING
            //
            // We need to lower PLM because DMA on GA100 is subject to
            // post-level-checking and ODP requires DMA access to both
            // IMEM and DMEM. Pri-source-isolation would help here but
            // unfortunately it is not supported on GA100 for IMEM/DMEM.
            // At any rate, the GA100 RISC-V LS support is debug-only and temporary.
            //
            // A different approach will be taken for GA10X as these PLMs cannot be
            // modified by ACR anyway (due to lockdown) and DMAs are no longer subject
            // to post-level-checking.
            //
            // TODO(bvanryn)/TODO(pjambhlekar) to revert this after GA10X support is working
            // at TOT and GA100 support is no longer needed. The setting of these PLMs on
            // GA10X will be decided as part of COREUCODES-598 and http://lwbugs/2720167.
            //
            // WARNING WARNING WARNING WARNING WARNING
            //
            pFlcnCfg->imemPLM             = ACR_PLMASK_READ_L0_WRITE_L2;
            // Allow read of PMU DMEM to level 0 for debug of PMU code
            pFlcnCfg->dmemPLM             = ACR_PLMASK_READ_L0_WRITE_L2;
            pFlcnCfg->bStackCfgSupported  = LW_TRUE;

            pFlcnCfg->uprocType           = ACR_TARGET_ENGINE_CORE_RISCV;
            pFlcnCfg->riscvRegisterBase   = LW_FALCON2_PWR_BASE;
            break;
#endif

        case LSF_FALCON_ID_PMU:
            pFlcnCfg->subWprRangeAddrBase = LW_PFB_PRI_MMU_FALCON_PMU_CFGA(0);
            pFlcnCfg->subWprRangePlmAddr  = LW_PFB_PRI_MMU_PMU_PRIV_LEVEL_MASK;
            pFlcnCfg->registerBase        = LW_PPWR_FALCON_IRQSSET;
            pFlcnCfg->fbifBase            = LW_PPWR_FBIF_TRANSCFG(0);
            pFlcnCfg->bOpenCarve          = LW_TRUE;
            pFlcnCfg->bFbifPresent        = LW_TRUE;
            pFlcnCfg->ctxDma              = RM_PMU_DMAIDX_UCODE;
#if defined(ACR_UNLOAD)
            pFlcnCfg->range0Addr          = LW_CPWR_FALCON_DMEM_PRIV_RANGE0;
            pFlcnCfg->range1Addr          = LW_CPWR_FALCON_DMEM_PRIV_RANGE1;
            pFlcnCfg->bIsBoOwner          = LW_TRUE; // PMU is the Bootstrap Owner
#else
            pFlcnCfg->range0Addr          = LW_PPWR_FALCON_DMEM_PRIV_RANGE0;
            pFlcnCfg->range1Addr          = LW_PPWR_FALCON_DMEM_PRIV_RANGE1;
            pFlcnCfg->regSelwreResetAddr  = LW_PPWR_FALCON_ENGINE;
#endif
            // Allow read of PMU DMEM to level 0 for debug of PMU code
            pFlcnCfg->dmemPLM             = ACR_PLMASK_READ_L0_WRITE_L2;
            pFlcnCfg->bStackCfgSupported  = LW_TRUE;
            pFlcnCfg->riscvRegisterBase   = LW_FALCON2_PWR_BASE;
            break;

#ifdef SEC2_PRESENT
        case LSF_FALCON_ID_SEC2:
            pFlcnCfg->subWprRangeAddrBase = LW_PFB_PRI_MMU_FALCON_SEC_CFGA(0);
            pFlcnCfg->subWprRangePlmAddr  = LW_PFB_PRI_MMU_SEC_PRIV_LEVEL_MASK;
            pFlcnCfg->registerBase        = LW_PSEC_FALCON_IRQSSET;
            pFlcnCfg->riscvRegisterBase   = LW_FALCON2_SEC_BASE;
            pFlcnCfg->fbifBase            = LW_PSEC_FBIF_TRANSCFG(0);
            pFlcnCfg->range0Addr          = 0;
            pFlcnCfg->range1Addr          = 0;
            pFlcnCfg->bOpenCarve          = LW_FALSE;
            pFlcnCfg->bFbifPresent        = LW_TRUE;
            pFlcnCfg->ctxDma              = RM_SEC2_DMAIDX_UCODE;
            pFlcnCfg->bStackCfgSupported  = LW_TRUE;
#if defined(AHESASC) || defined(BSI_LOCK) || defined(ACR_UNLOAD_ON_SEC2)
            pFlcnCfg->bIsBoOwner         = LW_TRUE;
#endif
            pFlcnCfg->regSelwreResetAddr = LW_PSEC_FALCON_ENGINE;
            break;
#endif


#ifndef ACR_ONLY_BO

#ifdef FBFALCON_PRESENT
        case LSF_FALCON_ID_FBFALCON:
            pFlcnCfg->subWprRangeAddrBase = LW_PFB_PRI_MMU_FALCON_FBFALCON_CFGA(0);
            pFlcnCfg->subWprRangePlmAddr  = LW_PFB_PRI_MMU_FBFALCON_PRIV_LEVEL_MASK;
            pFlcnCfg->registerBase        = LW_FALCON_FBFALCON_BASE;
            pFlcnCfg->fbifBase            = LW_PFBFALCON_FBIF_TRANSCFG(0);
            pFlcnCfg->range0Addr          = LW_PFBFALCON_FALCON_DMEM_PRIV_RANGE0;
            pFlcnCfg->range1Addr          = LW_PFBFALCON_FALCON_DMEM_PRIV_RANGE1;
            pFlcnCfg->bOpenCarve          = LW_TRUE;
            pFlcnCfg->bFbifPresent        = LW_TRUE;
            pFlcnCfg->ctxDma              = LSF_BOOTSTRAP_CTX_DMA_FBFLCN;
            pFlcnCfg->regSelwreResetAddr  = LW_PFBFALCON_FALCON_ENGINE;
            break;
#endif // FBFALCON_PRESENT

        case LSF_FALCON_ID_FECS:
            // ENGINE GR shouldnt be reset
            pFlcnCfg->subWprRangeAddrBase = LW_PFB_PRI_MMU_FALCON_FECS_CFGA(0);
            pFlcnCfg->subWprRangePlmAddr  = LW_PFB_PRI_MMU_FECS_PRIV_LEVEL_MASK;
            //
            // SEC always uses extended address space to bootload FECS-0 of a GR. And thus we also skip the check of
            // unprotected priv SMC_INFO which is vulnerable to attacks.
            // Tracked/More info in bug 2565457.
            //
            pFlcnCfg->registerBase        = LW_GET_PGRAPH_SMC_REG(LW_PGRAPH_PRI_FECS_FALCON_IRQSSET, falconInstance);
            pFlcnCfg->regCfgAddr          = LW_GET_PGRAPH_SMC_REG(LW_PGRAPH_PRI_FECS_ARB_FALCON_REGIONCFG, falconInstance);
            pFlcnCfg->regCfgMaskAddr      = LW_GET_PGRAPH_SMC_REG(LW_PGRAPH_PRI_FECS_ARB_FALCON_REGIONCFG_PRIV_LEVEL_MASK, falconInstance);
            pFlcnCfg->fbifBase            = 0;
            pFlcnCfg->range0Addr          = 0;
            pFlcnCfg->range1Addr          = 0;
            pFlcnCfg->bOpenCarve          = LW_FALSE;
            pFlcnCfg->bFbifPresent        = LW_FALSE;
            // RegCfgAddr and RegCfgMaskAddr are needed only for falcons with no FBIF
            pFlcnCfg->ctxDma              = LSF_BOOTSTRAP_CTX_DMA_FECS;
            break;

        case LSF_FALCON_ID_GPCCS:
            //
            // SEC should boot all GPCCS falcons in legacy unicast address space in both SMC/LEGACY modes so as to skip the check of
            // unprotected priv SMC_INFO(for SMC/LEGACY modes) /RS_MAP(for extended priv space) which is vulnerable to attacks.
            // Tracked/More info in bug 2565457.
            //
            pFlcnCfg->registerBase        = SMC_LEGACY_UNICAST_ADDR(LW_PGRAPH_PRI_GPC0_GPCCS_FALCON_IRQSSET, falconInstance);
            pFlcnCfg->regCfgAddr          = SMC_LEGACY_UNICAST_ADDR(LW_PGRAPH_PRI_GPC0_GPCCS_ARB_FALCON_REGIONCFG, falconInstance);
            pFlcnCfg->regCfgMaskAddr      = SMC_LEGACY_UNICAST_ADDR(LW_PGRAPH_PRI_GPC0_GPCCS_ARB_FALCON_REGIONCFG_PRIV_LEVEL_MASK, falconInstance);
            pFlcnCfg->fbifBase            = 0;
            pFlcnCfg->range0Addr          = 0;
            pFlcnCfg->range1Addr          = 0;
            pFlcnCfg->bOpenCarve          = LW_FALSE;
            pFlcnCfg->bFbifPresent        = LW_FALSE;
            // RegCfgAddr and RegCfgMaskAddr are needed only for falcons with no FBIF
            pFlcnCfg->ctxDma              = LSF_BOOTSTRAP_CTX_DMA_FECS;
            break;

 #ifdef LWDEC_PRESENT
        case LSF_FALCON_ID_LWDEC:
            pFlcnCfg->subWprRangeAddrBase = LW_PFB_PRI_MMU_FALCON_LWDEC0_CFGA(0);
            pFlcnCfg->subWprRangePlmAddr  = LW_PFB_PRI_MMU_LWDEC_PRIV_LEVEL_MASK;
            //
            // HW changes BIT define to FIELD define,  INFO_RESET_BIT_XXX <bit_offset> to INFO_RESET_FIELD_XXX(bit_end : bit_start)
            // Even bit offset exceeds 32, HW stills use 32:32, 33:33 ...  likewise definitions;
            // so we can callwlate reg index and bit index for LW_PMC_DEVICE_ENABLE(i).
            //
            pFlcnCfg->pmcEnableMask       = 1 << (DRF_BASE(LW_PTOP_DEVICE_INFO_RESET_FIELD_FOR_UCODE_AND_VBIOS_ONLY_LWDEC0)%32);
            pFlcnCfg->pmcEnableRegIdx     = (DRF_BASE(LW_PTOP_DEVICE_INFO_RESET_FIELD_FOR_UCODE_AND_VBIOS_ONLY_LWDEC0)/32);
            pFlcnCfg->registerBase        = LW_FALCON_LWDEC_BASE;
            pFlcnCfg->fbifBase            = LW_PLWDEC_FBIF_TRANSCFG(0,0);
            pFlcnCfg->range0Addr          = 0;
            pFlcnCfg->range1Addr          = 0;
            pFlcnCfg->bOpenCarve          = LW_FALSE;
            pFlcnCfg->bFbifPresent        = LW_TRUE;
            pFlcnCfg->ctxDma              = LSF_BOOTSTRAP_CTX_DMA_VIDEO;
            pFlcnCfg->bStackCfgSupported  = LW_TRUE;
            break;
 #endif

 #ifdef GSP_PRESENT
 #ifdef ACR_RISCV_LS
        case LSF_FALCON_ID_GSP_RISCV:
            pFlcnCfg->subWprRangeAddrBase = LW_PFB_PRI_MMU_FALCON_GSP_CFGA(0);
            pFlcnCfg->subWprRangePlmAddr  = LW_PFB_PRI_MMU_GSP_PRIV_LEVEL_MASK;
            pFlcnCfg->registerBase        = LW_PGSP_FALCON_IRQSSET;
            pFlcnCfg->fbifBase            = LW_PGSP_FBIF_TRANSCFG(0);
            pFlcnCfg->range0Addr          = 0;
            pFlcnCfg->range1Addr          = 0;
            pFlcnCfg->bOpenCarve          = LW_FALSE;
            pFlcnCfg->bFbifPresent        = LW_TRUE;
            pFlcnCfg->ctxDma              = RM_GSP_DMAIDX_UCODE;
            pFlcnCfg->bStackCfgSupported  = LW_TRUE;
            pFlcnCfg->regSelwreResetAddr  = LW_PGSP_FALCON_ENGINE;

            pFlcnCfg->uprocType           = ACR_TARGET_ENGINE_CORE_RISCV;
            pFlcnCfg->riscvRegisterBase   = LW_FALCON2_GSP_BASE;
            //
            // WARNING WARNING WARNING WARNING WARNING
            //
            // We need to lower PLM because DMA on GA100 is subject to
            // post-level-checking and ODP requires DMA access to both
            // IMEM and DMEM. Pri-source-isolation would help here but
            // unfortunately it is not supported on GA100 for IMEM/DMEM.
            // At any rate, the GA100 RISC-V LS support is debug-only and temporary.
            //
            // A different approach will be taken for GA10X as these PLMs cannot be
            // modified by ACR anyway (due to lockdown) and DMAs are no longer subject
            // to post-level-checking.
            //
            // TODO(bvanryn)/TODO(pjambhlekar) to revert this after GA10X support is working
            // at TOT and GA100 support is no longer needed. The setting of these PLMs on
            // GA10X will be decided as part of COREUCODES-598 and http://lwbugs/2720167.
            //
            // WARNING WARNING WARNING WARNING WARNING
            //
            pFlcnCfg->dmemPLM             = ACR_PLMASK_READ_L2_WRITE_L2;
            pFlcnCfg->imemPLM             = ACR_PLMASK_READ_L0_WRITE_L2;
            break;
 #endif // ACR_RISCV_LS

        case LSF_FALCON_ID_GSPLITE:
            pFlcnCfg->subWprRangeAddrBase = LW_PFB_PRI_MMU_FALCON_GSP_CFGA(0);
            pFlcnCfg->subWprRangePlmAddr  = LW_PFB_PRI_MMU_GSP_PRIV_LEVEL_MASK;
            pFlcnCfg->registerBase        = LW_PGSP_FALCON_IRQSSET;
            pFlcnCfg->riscvRegisterBase   = LW_FALCON2_GSP_BASE;
            pFlcnCfg->fbifBase            = LW_PGSP_FBIF_TRANSCFG(0);
            pFlcnCfg->range0Addr          = 0;
            pFlcnCfg->range1Addr          = 0;
            pFlcnCfg->bOpenCarve          = LW_FALSE;
            pFlcnCfg->bFbifPresent        = LW_TRUE;
            pFlcnCfg->ctxDma              = RM_GSPLITE_DMAIDX_UCODE;
            pFlcnCfg->bStackCfgSupported  = LW_TRUE;
            pFlcnCfg->regSelwreResetAddr  = LW_PGSP_FALCON_ENGINE;
#if defined(ASB)
            pFlcnCfg->bIsBoOwner          = LW_TRUE;
#endif
            break;
#endif // GSP_PRESENT
#endif // ACR_ONLY_BO
        default:
            status = ACR_ERROR_FLCN_ID_NOT_FOUND;
    }

    return status;
}
