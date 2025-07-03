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
 * @file: acr_register_access_gh100_only.c
 */
#include "acr.h"
#include "hwproject.h"
#include "dev_pri_ringstation_sys.h"
#include "dev_fb.h"
#include "dev_falcon_v4.h"
#include "dev_graphics_nobundle.h"
#include "dev_graphics_nobundle_addendum.h"
#include "dev_smcarb_addendum.h"
#include "dev_pri_ringstation_gpc.h"
#include "dev_falcon_second_pri.h"
#include "lw_riscv_address_map.h"
#include "dev_lwdec_pri.h"
#include "dev_sec_addendum.h"

#include "dev_bus.h"

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

#if defined(LWDEC_PRESENT) || defined(LWDEC1_PRESENT) || defined(LWDEC_RISCV_PRESENT) || defined(LWDEC_RISCV_EB_PRESENT)
#include "dev_lwdec_pri.h"
#endif

#if defined(LWDEC_RISCV_PRESENT) || defined(LWDEC_RISCV_EB_PRESENT)
#include "dev_pri_ringstation_sys.h"
#include "dev_riscv_pri.h"
#include "dev_top.h"
#endif

#ifdef LWJPG_PRESENT
#include "dev_lwjpg_pri_sw.h"
#include "dev_pri_ringstation_sys.h"
#include "dev_top.h"
#endif

#ifdef OFA_PRESENT
#include "dev_ofa_pri.h"
#endif

// GH100 Netlist number in which HW team fixed the H-187 reset issue seen with FECS & GPCCS engines
#define NETLIST_22              (22)

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
acrlibGetFalconConfig_GH100
(
    LwU32                falconId,
    LwU32                falconInstance,
    PACR_FLCN_CONFIG     pFlcnCfg
)
{
    LwU32       netlistVersion = 0x0;
    ACR_STATUS  status         = ACR_OK;

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
    pFlcnCfg->falcon2RegisterBase    = 0;
    pFlcnCfg->scpP2pCtl              = 0;
    pFlcnCfg->pmcEnableRegIdx        = 0;
    pFlcnCfg->ctxDma                 = 0;
    pFlcnCfg->maxSubWprSupportedByHw = 0;
    pFlcnCfg->bOpenCarve             = LW_FALSE;
    pFlcnCfg->bFbifPresent           = LW_FALSE;
    pFlcnCfg->uprocType              = ACR_TARGET_ENGINE_CORE_FALCON;
    pFlcnCfg->targetMaskIndex        = 0;

    switch (falconId)
    {
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
            pFlcnCfg->regSelwreResetAddr  = SMC_LEGACY_UNICAST_ADDR(LW_PGRAPH_PRI_GPC0_GPCCS_FALCON_ENGINE,falconInstance);
            pFlcnCfg->targetMaskIndex     = LW_PPRIV_GPC_PRI_MASTER_gpc_pri_hub2gpccs_pri;

            //
            // For NELIST < NL22, H-187 reset fix is not present hence use older i.e SRESET
            // FMODEL also uses the H-187 secure reset
            // TODO: Use only H-187 once NETLIST < 22 are depricated
            // JIRA ID: HOPPER-1801 track removal of this
            //
            netlistVersion = ACR_REG_RD32(BAR0, LW_PBUS_EMULATION_REV0);
            if( (netlistVersion == 0) || (netlistVersion >= NETLIST_22) )
            {
                pFlcnCfg->regSelwreResetAddr  = SMC_LEGACY_UNICAST_ADDR(LW_PGRAPH_PRI_GPC0_GPCCS_FALCON_ENGINE,falconInstance);
            }
            else
            {
                pFlcnCfg->regSelwreResetAddr  = 0;
            }
            break;

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
            status = acrlibGetFecsTargetMaskIndex_HAL(pAcrLib, falconInstance, &pFlcnCfg->targetMaskIndex);
            //
            // For NELIST < NL22, H-187 reset fix is not present hence use older i.e SRESET
            // FMODEL also uses the H-187 secure reset
            // TODO: Use only H-187 once NETLIST < 22 are depricated
            // JIRA ID: HOPPER-1801 track removal of this
            //

            netlistVersion = ACR_REG_RD32(BAR0, LW_PBUS_EMULATION_REV0);
            if( (netlistVersion == 0) || (netlistVersion >= NETLIST_22) )
            {
                pFlcnCfg->regSelwreResetAddr  = LW_GET_PGRAPH_SMC_REG(LW_PGRAPH_PRI_FECS_FALCON_ENGINE,falconInstance);
            }
            else
            {
                pFlcnCfg->regSelwreResetAddr  = 0;
            }
            break;

#ifdef LWDEC_PRESENT
       case LSF_FALCON_ID_LWDEC:
           pFlcnCfg->subWprRangeAddrBase = LW_PFB_PRI_MMU_FALCON_LWDEC0_CFGA(0);
           pFlcnCfg->subWprRangePlmAddr  = LW_PFB_PRI_MMU_LWDEC_PRIV_LEVEL_MASK;
           pFlcnCfg->regSelwreResetAddr  = LW_PLWDEC_FALCON_ENGINE(0);
           pFlcnCfg->registerBase        = LW_FALCON_LWDEC_BASE;
           pFlcnCfg->fbifBase            = LW_PLWDEC_FBIF_TRANSCFG(0,0);
           pFlcnCfg->range0Addr          = 0;
           pFlcnCfg->range1Addr          = 0;
           pFlcnCfg->bOpenCarve          = LW_FALSE;
           pFlcnCfg->bFbifPresent        = LW_TRUE;
           pFlcnCfg->ctxDma              = LSF_BOOTSTRAP_CTX_DMA_VIDEO;
           pFlcnCfg->bStackCfgSupported  = LW_TRUE;
           pFlcnCfg->falcon2RegisterBase = LW_FALCON2_LWDEC0_BASE;
           status = acrlibGetLwdecTargetMaskIndex_HAL(pAcrLib, pFlcnCfg, &pFlcnCfg->targetMaskIndex);
           break;
#endif

#ifdef LWDEC1_PRESENT
        case LSF_FALCON_ID_LWDEC1:
            if (!acrlibValidateLwdecId_HAL(pAcrLib, falconId, falconInstance))
            {
                status = ACR_ERROR_FLCN_ID_NOT_FOUND;
                return status;
            }
            pFlcnCfg->registerBase           = DRF_BASE(LW_PLWDEC(1));
            pFlcnCfg->fbifBase               = LW_PLWDEC_FBIF_TRANSCFG(1,0);
            pFlcnCfg->ctxDma                 = LSF_BOOTSTRAP_CTX_DMA_VIDEO;
            pFlcnCfg->subWprRangeAddrBase    = LW_PFB_PRI_MMU_FALCON_LWDEC1_CFGA(0);
            pFlcnCfg->subWprRangePlmAddr     = LW_PFB_PRI_MMU_LWDEC_PRIV_LEVEL_MASK;
            pFlcnCfg->maxSubWprSupportedByHw = LW_PFB_PRI_MMU_FALCON_LWDEC1_CFGA__SIZE_1;
            pFlcnCfg->regSelwreResetAddr     = LW_PLWDEC_FALCON_ENGINE(falconInstance);
            pFlcnCfg->bFbifPresent           = LW_TRUE;
            pFlcnCfg->bStackCfgSupported     = LW_TRUE;
            pFlcnCfg->falcon2RegisterBase    = LW_FALCON2_LWDEC1_BASE;
            status = acrlibGetLwdecTargetMaskIndex_HAL(pAcrLib, pFlcnCfg, &pFlcnCfg->targetMaskIndex);
            break;
#endif

#ifdef LWDEC_RISCV_PRESENT
        case LSF_FALCON_ID_LWDEC_RISCV:
#endif

#ifdef LWDEC_RISCV_EB_PRESENT
        case LSF_FALCON_ID_LWDEC_RISCV_EB:
#endif

#if  defined(LWDEC_RISCV_PRESENT) || defined(LWDEC_RISCV_EB_PRESENT)
            status = acrlibGetLwdecFalconConfig_HAL(pAcrLib, falconId, falconInstance, pFlcnCfg);
            break;
#endif

#ifdef LWJPG_PRESENT
        case LSF_FALCON_ID_LWJPG:
            status = acrlibGetLwjpgFalconConfig_HAL(pAcrLib, falconId, falconInstance, pFlcnCfg);
            break;
#endif

#ifdef OFA_PRESENT
        case LSF_FALCON_ID_OFA:
            pFlcnCfg->registerBase           = DRF_BASE(LW_POFA);
            pFlcnCfg->regSelwreResetAddr     = LW_POFA_FALCON_ENGINE;
            pFlcnCfg->fbifBase               = LW_POFA_FBIF_TRANSCFG(0);
            pFlcnCfg->ctxDma                 = LSF_BOOTSTRAP_CTX_DMA_VIDEO;
            pFlcnCfg->subWprRangeAddrBase    = LW_PFB_PRI_MMU_FALCON_OFA0_CFGA(0);
            pFlcnCfg->subWprRangePlmAddr     = LW_PFB_PRI_MMU_OFA_PRIV_LEVEL_MASK;
            pFlcnCfg->maxSubWprSupportedByHw = LW_PFB_PRI_MMU_FALCON_OFA0_CFGA__SIZE_1;
            pFlcnCfg->bFbifPresent           = LW_TRUE;
            pFlcnCfg->bStackCfgSupported     = LW_TRUE;
            pFlcnCfg->targetMaskIndex        = LW_PPRIV_SYS_PRI_MASTER_fecs2ofa_pri;
            break;
#endif
#ifndef BOOT_FROM_HS_BUILD
        case LSF_FALCON_ID_PMU:
#endif
        case LSF_FALCON_ID_PMU_RISCV:
        case LSF_FALCON_ID_SEC2:
        case LSF_FALCON_ID_GSPLITE:
#ifndef BOOT_FROM_HS_BUILD
        case LSF_FALCON_ID_GSP_RISCV:
#endif
        case LSF_FALCON_ID_FBFALCON:
#ifdef LWENC_PRESENT
        case LSF_FALCON_ID_LWENC:
#endif
            break;

        default:
            status = ACR_ERROR_FLCN_ID_NOT_FOUND;
            break;
    }

    if (status == ACR_OK && pFlcnCfg->registerBase == 0)
    {
        status = acrlibGetFalconConfig_GA10X(falconId, falconInstance, pFlcnCfg);
    }

    return status;
}

// TODO: eddichang g000 AXL still needs this function. Once we move all LWDEC to RISCV based ; we can remove this HAL function.
/*!
 * @brief Get the target mask index for FbFalcon for Hopper
 */
ACR_STATUS
acrlibGetFbFalconTargetMaskIndex_GH100(LwU32 *pTargetMaskIndex)
{
    if (pTargetMaskIndex == NULL)
    {
        return ACR_ERROR_ILWALID_ARGUMENT;
    }

#ifdef LW_PPRIV_SYS_PRI_MASTER_fecs2fbfalcon_pri
    *pTargetMaskIndex = LW_PPRIV_SYS_PRI_MASTER_fecs2fbfalcon_pri;
#elif defined(LW_PPRIV_SYSB_PRI_MASTER_fecs2fbfalcon_pri)
    *pTargetMaskIndex = LW_PPRIV_SYSB_PRI_MASTER_fecs2fbfalcon_pri;
#elif defined(LW_PPRIV_SYSC_PRI_MASTER_fecs2fbfalcon_pri)
    *pTargetMaskIndex = LW_PPRIV_SYSC_PRI_MASTER_fecs2fbfalcon_pri;
#else
    #error "Register not defined for LW_PPRIV_SYS*_PRI_MASTER_fecs2fbfalcon_pri";
#endif

    return ACR_OK;
}

/*!
 * @brief Get the target mask index for LWDEC
 */
ACR_STATUS
acrlibGetLwdecTargetMaskIndex_GH100
(
    PACR_FLCN_CONFIG pFlcnCfg,
    LwU32 *pTargetMaskIndex
)
{
    LwU32 falconInstance;

    if (pTargetMaskIndex == NULL || pFlcnCfg == NULL)
    {
        return ACR_ERROR_ILWALID_ARGUMENT;
    }

    falconInstance = pFlcnCfg->falconInstance;

    if (pFlcnCfg->falconId == LSF_FALCON_ID_LWDEC_RISCV_EB)
    {
        falconInstance++;
    }

    if ((pFlcnCfg->falconId == LSF_FALCON_ID_LWDEC && pFlcnCfg->falconInstance == LSF_FALCON_INSTANCE_DEFAULT_0) ||
        (pFlcnCfg->falconId == LSF_FALCON_ID_LWDEC_RISCV && pFlcnCfg->falconInstance == LSF_FALCON_INSTANCE_DEFAULT_0))
    {
#ifdef LW_PPRIV_SYS_PRI_MASTER_fecs2lwdec_pri0
    *pTargetMaskIndex = LW_PPRIV_SYS_PRI_MASTER_fecs2lwdec_pri0;
#elif defined(LW_PPRIV_SYSB_PRI_MASTER_fecs2lwdec_pri0)
    *pTargetMaskIndex = LW_PPRIV_SYSB_PRI_MASTER_fecs2lwdec_pri0;
#elif defined(LW_PPRIV_SYSC_PRI_MASTER_fecs2lwdec_pri0)
    *pTargetMaskIndex = LW_PPRIV_SYSC_PRI_MASTER_fecs2lwdec_pri0;
#else
    #error "Register not defined for LW_PPRIV_SYS*_PRI_MASTER_fecs2lwdec_pri0";
#endif
    }
    else if (pFlcnCfg->falconId == LSF_FALCON_ID_LWDEC1 && pFlcnCfg->falconInstance == LSF_FALCON_INSTANCE_DEFAULT_0)
    {
#ifdef LW_PPRIV_SYS_PRI_MASTER_fecs2lwdec_pri1
    *pTargetMaskIndex = LW_PPRIV_SYS_PRI_MASTER_fecs2lwdec_pri1;
#elif defined(LW_PPRIV_SYSB_PRI_MASTER_fecs2lwdec_pri1)
    *pTargetMaskIndex = LW_PPRIV_SYSB_PRI_MASTER_fecs2lwdec_pri1;
#elif defined(LW_PPRIV_SYSC_PRI_MASTER_fecs2lwdec_pri1)
    *pTargetMaskIndex = LW_PPRIV_SYSC_PRI_MASTER_fecs2lwdec_pri1;
#else
    #error "Register not defined for LW_PPRIV_SYS*_PRI_MASTER_fecs2lwdec_pri1";
#endif
    }
#ifdef LWDEC_RISCV_EB_PRESENT
    else if (pFlcnCfg->falconId == LSF_FALCON_ID_LWDEC_RISCV_EB)
    {
        switch (falconInstance)
        {
            case 1:
                #ifdef LW_PPRIV_SYS_PRI_MASTER_fecs2lwdec_pri1
                    *pTargetMaskIndex = LW_PPRIV_SYS_PRI_MASTER_fecs2lwdec_pri1;
                #elif defined(LW_PPRIV_SYSB_PRI_MASTER_fecs2lwdec_pri1)
                    *pTargetMaskIndex = LW_PPRIV_SYSB_PRI_MASTER_fecs2lwdec_pri1;
                #elif defined(LW_PPRIV_SYSC_PRI_MASTER_fecs2lwdec_pri1)
                    *pTargetMaskIndex = LW_PPRIV_SYSC_PRI_MASTER_fecs2lwdec_pri1;
                #else
                    #error "Register not defined for LW_PPRIV_SYS*_PRI_MASTER_fecs2lwdec_pri1";
                #endif
                break;

           case 2:
                #ifdef LW_PPRIV_SYS_PRI_MASTER_fecs2lwdec_pri2
                    *pTargetMaskIndex = LW_PPRIV_SYS_PRI_MASTER_fecs2lwdec_pri2;
                #elif defined(LW_PPRIV_SYSB_PRI_MASTER_fecs2lwdec_pri2)
                    *pTargetMaskIndex = LW_PPRIV_SYSB_PRI_MASTER_fecs2lwdec_pri2;
                #elif defined(LW_PPRIV_SYSC_PRI_MASTER_fecs2lwdec_pri2)
                    *pTargetMaskIndex = LW_PPRIV_SYSC_PRI_MASTER_fecs2lwdec_pri2;
                #else
                    #error "Register not defined for LW_PPRIV_SYS*_PRI_MASTER_fecs2lwdec_pri2";
                #endif
                break;

           case 3:
                #ifdef LW_PPRIV_SYS_PRI_MASTER_fecs2lwdec_pri3
                    *pTargetMaskIndex = LW_PPRIV_SYS_PRI_MASTER_fecs2lwdec_pri3;
                #elif defined(LW_PPRIV_SYSB_PRI_MASTER_fecs2lwdec_pri3)
                    *pTargetMaskIndex = LW_PPRIV_SYSB_PRI_MASTER_fecs2lwdec_pri3;
                #elif defined(LW_PPRIV_SYSC_PRI_MASTER_fecs2lwdec_pri3)
                    *pTargetMaskIndex = LW_PPRIV_SYSC_PRI_MASTER_fecs2lwdec_pri3;
                #else
                    #error "Register not defined for LW_PPRIV_SYS*_PRI_MASTER_fecs2lwdec_pri3";
                #endif
                break;

           case 4:
                #ifdef LW_PPRIV_SYS_PRI_MASTER_fecs2lwdec_pri4
                    *pTargetMaskIndex = LW_PPRIV_SYS_PRI_MASTER_fecs2lwdec_pri4;
                #elif defined(LW_PPRIV_SYSB_PRI_MASTER_fecs2lwdec_pri4)
                    *pTargetMaskIndex = LW_PPRIV_SYSB_PRI_MASTER_fecs2lwdec_pri4;
                #elif defined(LW_PPRIV_SYSC_PRI_MASTER_fecs2lwdec_pri4)
                    *pTargetMaskIndex = LW_PPRIV_SYSC_PRI_MASTER_fecs2lwdec_pri4;
                #else
                    #error "Register not defined for LW_PPRIV_SYS*_PRI_MASTER_fecs2lwdec_pri4";
                #endif
                break;

           case 5:
                #ifdef LW_PPRIV_SYS_PRI_MASTER_fecs2lwdec_pri5
                    *pTargetMaskIndex = LW_PPRIV_SYS_PRI_MASTER_fecs2lwdec_pri5;
                #elif defined(LW_PPRIV_SYSB_PRI_MASTER_fecs2lwdec_pri5)
                    *pTargetMaskIndex = LW_PPRIV_SYSB_PRI_MASTER_fecs2lwdec_pri5;
                #elif defined(LW_PPRIV_SYSC_PRI_MASTER_fecs2lwdec_pri5)
                    *pTargetMaskIndex = LW_PPRIV_SYSC_PRI_MASTER_fecs2lwdec_pri5;
                #else
                    #error "Register not defined for LW_PPRIV_SYS*_PRI_MASTER_fecs2lwdec_pri5";
                #endif
                break;

           case 6:
                #ifdef LW_PPRIV_SYS_PRI_MASTER_fecs2lwdec_pri6
                    *pTargetMaskIndex = LW_PPRIV_SYS_PRI_MASTER_fecs2lwdec_pri6;
                #elif defined(LW_PPRIV_SYSB_PRI_MASTER_fecs2lwdec_pri6)
                    *pTargetMaskIndex = LW_PPRIV_SYSB_PRI_MASTER_fecs2lwdec_pri6;
                #elif defined(LW_PPRIV_SYSC_PRI_MASTER_fecs2lwdec_pri6)
                    *pTargetMaskIndex = LW_PPRIV_SYSC_PRI_MASTER_fecs2lwdec_pri6;
                #else
                    #error "Register not defined for LW_PPRIV_SYS*_PRI_MASTER_fecs2lwdec_pri6";
                #endif
                break;

           case 7:
                #ifdef LW_PPRIV_SYS_PRI_MASTER_fecs2lwdec_pri7
                    *pTargetMaskIndex = LW_PPRIV_SYS_PRI_MASTER_fecs2lwdec_pri7;
                #elif defined(LW_PPRIV_SYSB_PRI_MASTER_fecs2lwdec_pri7)
                    *pTargetMaskIndex = LW_PPRIV_SYSB_PRI_MASTER_fecs2lwdec_pri7;
                #elif defined(LW_PPRIV_SYSC_PRI_MASTER_fecs2lwdec_pri7)
                    *pTargetMaskIndex = LW_PPRIV_SYSC_PRI_MASTER_fecs2lwdec_pri7;
                #else
                    #error "Register not defined for LW_PPRIV_SYS*_PRI_MASTER_fecs2lwdec_pri7";
                #endif
                break;

            default:
                return ACR_ERROR_ILWALID_ARGUMENT;
        }
    }
#endif
    else
    {
        return ACR_ERROR_ILWALID_ARGUMENT;
    }

    return ACR_OK;
}

/*!
 * @brief Return the target mask register details for FbFalcon, for Hopper
 */
ACR_STATUS
acrlibGetFbFalconTargetMaskRegisterDetails_GH100
(
    LwU32 *pTargetMaskRegister,
    LwU32 *pTargetMaskPlmRegister,
    LwU32 *pTargetMaskIndexRegister
)
{
    //
    // All the three argument pointers need to be valid for this function.
    // Thus, return error if any of them is NULL.
    //
    if ((pTargetMaskRegister == NULL) || (pTargetMaskPlmRegister == NULL) || (pTargetMaskIndexRegister == NULL))
    {
        return ACR_ERROR_ILWALID_ARGUMENT;
    }

#ifdef LW_PPRIV_SYS_PRI_MASTER_fecs2fbfalcon_pri
    *pTargetMaskRegister      = LW_PPRIV_SYS_TARGET_MASK;
    *pTargetMaskPlmRegister   = LW_PPRIV_SYS_TARGET_MASK_PRIV_LEVEL_MASK;
    *pTargetMaskIndexRegister = LW_PPRIV_SYS_TARGET_MASK_INDEX;
#elif defined(LW_PPRIV_SYSB_PRI_MASTER_fecs2fbfalcon_pri)
    *pTargetMaskRegister      = LW_PPRIV_SYSB_TARGET_MASK;
    *pTargetMaskPlmRegister   = LW_PPRIV_SYSB_TARGET_MASK_PRIV_LEVEL_MASK;
    *pTargetMaskIndexRegister = LW_PPRIV_SYSB_TARGET_MASK_INDEX;
#elif defined(LW_PPRIV_SYSC_PRI_MASTER_fecs2fbfalcon_pri)
    *pTargetMaskRegister      = LW_PPRIV_SYSC_TARGET_MASK;
    *pTargetMaskPlmRegister   = LW_PPRIV_SYSC_TARGET_MASK_PRIV_LEVEL_MASK;
    *pTargetMaskIndexRegister = LW_PPRIV_SYSC_TARGET_MASK_INDEX;
#else
        #error "Register LW_PPRIV_SYS*_TARGET_MASK* not defined for LW_PPRIV_SYS*_PRI_MASTER_fecs2fbfalcon_pri";
#endif

    return ACR_OK;
}

/*!
 * @brief Return the target mask register details for LWDEC
 */
ACR_STATUS
acrlibGetLwdecTargetMaskRegisterDetails_GH100
(
    PACR_FLCN_CONFIG pFlcnCfg,
    LwU32 *pTargetMaskRegister,
    LwU32 *pTargetMaskPlmRegister,
    LwU32 *pTargetMaskIndexRegister
)
{
    //
    // All the three argument pointers need to be valid for this function.
    // Thus, return error if any of them is NULL.
    //
    if ((pTargetMaskRegister == NULL) || (pTargetMaskPlmRegister == NULL) || (pTargetMaskIndexRegister == NULL))
    {
        return ACR_ERROR_ILWALID_ARGUMENT;
    }

    //
    // GH100 still needs to support LWDEC Falcon; once video side remove all LWDEC test and replace with LWDEC-RISCV/LWDEC-RISCV-EB test
    // we can remove below check section.
    //
    if (pFlcnCfg->falconId == LSF_FALCON_ID_LWDEC && pFlcnCfg->falconInstance == LSF_FALCON_INSTANCE_DEFAULT_0)
    {
        // LSF_FALCON_ID_LWDEC means LWDEC(0) Falcon
        #ifdef LW_PPRIV_SYS_PRI_MASTER_fecs2lwdec_pri0
            *pTargetMaskRegister      = LW_PPRIV_SYS_TARGET_MASK;
            *pTargetMaskPlmRegister   = LW_PPRIV_SYS_TARGET_MASK_PRIV_LEVEL_MASK;
            *pTargetMaskIndexRegister = LW_PPRIV_SYS_TARGET_MASK_INDEX;
        #elif defined(LW_PPRIV_SYSB_PRI_MASTER_fecs2lwdec_pri0)
            *pTargetMaskRegister      = LW_PPRIV_SYSB_TARGET_MASK;
            *pTargetMaskPlmRegister   = LW_PPRIV_SYSB_TARGET_MASK_PRIV_LEVEL_MASK;
            *pTargetMaskIndexRegister = LW_PPRIV_SYSB_TARGET_MASK_INDEX;
        #elif defined(LW_PPRIV_SYSC_PRI_MASTER_fecs2lwdec_pri0)
            *pTargetMaskRegister      = LW_PPRIV_SYSC_TARGET_MASK;
            *pTargetMaskPlmRegister   = LW_PPRIV_SYSC_TARGET_MASK_PRIV_LEVEL_MASK;
            *pTargetMaskIndexRegister = LW_PPRIV_SYSC_TARGET_MASK_INDEX;
        #else
            #error "Register not defined for LW_PPRIV_SYS*_PRI_MASTER_fecs2lwdec_pri0";
        #endif
        return ACR_OK;
    }

    if (pFlcnCfg->falconId == LSF_FALCON_ID_LWDEC1 && pFlcnCfg->falconInstance == LSF_FALCON_INSTANCE_DEFAULT_0)
    {
        // LSF_FALCON_ID_LWDEC1 means LWDEC(1) Falcon
        #ifdef LW_PPRIV_SYS_PRI_MASTER_fecs2lwdec_pri1
            *pTargetMaskRegister      = LW_PPRIV_SYS_TARGET_MASK;
            *pTargetMaskPlmRegister   = LW_PPRIV_SYS_TARGET_MASK_PRIV_LEVEL_MASK;
            *pTargetMaskIndexRegister = LW_PPRIV_SYS_TARGET_MASK_INDEX;
        #elif defined(LW_PPRIV_SYSB_PRI_MASTER_fecs2lwdec_pri1)
            *pTargetMaskRegister      = LW_PPRIV_SYSB_TARGET_MASK;
            *pTargetMaskPlmRegister   = LW_PPRIV_SYSB_TARGET_MASK_PRIV_LEVEL_MASK;
            *pTargetMaskIndexRegister = LW_PPRIV_SYSB_TARGET_MASK_INDEX;
        #elif defined(LW_PPRIV_SYSC_PRI_MASTER_fecs2lwdec_pri1)
            *pTargetMaskRegister      = LW_PPRIV_SYSC_TARGET_MASK;
            *pTargetMaskPlmRegister   = LW_PPRIV_SYSC_TARGET_MASK_PRIV_LEVEL_MASK;
            *pTargetMaskIndexRegister = LW_PPRIV_SYSC_TARGET_MASK_INDEX;
        #else
            #error "Register not defined for LW_PPRIV_SYS*_PRI_MASTER_fecs2lwdec_pri1";
        #endif
        return ACR_OK;
    }

#if defined(LWDEC_RISCV_PRESENT) || defined(LWDEC_RISCV_EB_PRESENT)
    LwU32 falconInstance;

    if (pFlcnCfg->falconId != LSF_FALCON_ID_LWDEC_RISCV &&
        pFlcnCfg->falconId != LSF_FALCON_ID_LWDEC_RISCV_EB)
    {
        return ACR_ERROR_ILWALID_ARGUMENT;
    }

    falconInstance = pFlcnCfg->falconInstance;

    if (pFlcnCfg->falconId == LSF_FALCON_ID_LWDEC_RISCV_EB)
    {
        // LWDEC-RISCV-EB instance 0 is physical LWDEC 1; so we increase falconInstance by 1
        falconInstance++;
    }

    switch (falconInstance)
    {
        case 0:
            #ifdef LW_PPRIV_SYS_PRI_MASTER_fecs2lwdec_pri0
                *pTargetMaskRegister      = LW_PPRIV_SYS_TARGET_MASK;
                *pTargetMaskPlmRegister   = LW_PPRIV_SYS_TARGET_MASK_PRIV_LEVEL_MASK;
                *pTargetMaskIndexRegister = LW_PPRIV_SYS_TARGET_MASK_INDEX;
            #elif defined(LW_PPRIV_SYSB_PRI_MASTER_fecs2lwdec_pri0)
                *pTargetMaskRegister      = LW_PPRIV_SYSB_TARGET_MASK;
                *pTargetMaskPlmRegister   = LW_PPRIV_SYSB_TARGET_MASK_PRIV_LEVEL_MASK;
                *pTargetMaskIndexRegister = LW_PPRIV_SYSB_TARGET_MASK_INDEX;
            #elif defined(LW_PPRIV_SYSC_PRI_MASTER_fecs2lwdec_pri0)
                *pTargetMaskRegister      = LW_PPRIV_SYSC_TARGET_MASK;
                *pTargetMaskPlmRegister   = LW_PPRIV_SYSC_TARGET_MASK_PRIV_LEVEL_MASK;
                *pTargetMaskIndexRegister = LW_PPRIV_SYSC_TARGET_MASK_INDEX;
            #else
                #error "Register not defined for LW_PPRIV_SYS*_PRI_MASTER_fecs2lwdec_pri0";
            #endif
            break;

        case 1:
            #ifdef LW_PPRIV_SYS_PRI_MASTER_fecs2lwdec_pri1
                *pTargetMaskRegister      = LW_PPRIV_SYS_TARGET_MASK;
                *pTargetMaskPlmRegister   = LW_PPRIV_SYS_TARGET_MASK_PRIV_LEVEL_MASK;
                *pTargetMaskIndexRegister = LW_PPRIV_SYS_TARGET_MASK_INDEX;
            #elif defined(LW_PPRIV_SYSB_PRI_MASTER_fecs2lwdec_pri1)
                *pTargetMaskRegister      = LW_PPRIV_SYSB_TARGET_MASK;
                *pTargetMaskPlmRegister   = LW_PPRIV_SYSB_TARGET_MASK_PRIV_LEVEL_MASK;
                *pTargetMaskIndexRegister = LW_PPRIV_SYSB_TARGET_MASK_INDEX;
            #elif defined(LW_PPRIV_SYSC_PRI_MASTER_fecs2lwdec_pri1)
                *pTargetMaskRegister      = LW_PPRIV_SYSC_TARGET_MASK;
                *pTargetMaskPlmRegister   = LW_PPRIV_SYSC_TARGET_MASK_PRIV_LEVEL_MASK;
                *pTargetMaskIndexRegister = LW_PPRIV_SYSC_TARGET_MASK_INDEX;
            #else
                #error "Register not defined for LW_PPRIV_SYS*_PRI_MASTER_fecs2lwdec_pri1";
            #endif
            break;

        case 2:
            #ifdef LW_PPRIV_SYS_PRI_MASTER_fecs2lwdec_pri2
                *pTargetMaskRegister      = LW_PPRIV_SYS_TARGET_MASK;
                *pTargetMaskPlmRegister   = LW_PPRIV_SYS_TARGET_MASK_PRIV_LEVEL_MASK;
                *pTargetMaskIndexRegister = LW_PPRIV_SYS_TARGET_MASK_INDEX;
            #elif defined(LW_PPRIV_SYSB_PRI_MASTER_fecs2lwdec_pri2)
                *pTargetMaskRegister      = LW_PPRIV_SYSB_TARGET_MASK;
                *pTargetMaskPlmRegister   = LW_PPRIV_SYSB_TARGET_MASK_PRIV_LEVEL_MASK;
                *pTargetMaskIndexRegister = LW_PPRIV_SYSB_TARGET_MASK_INDEX;
            #elif defined(LW_PPRIV_SYSC_PRI_MASTER_fecs2lwdec_pri2)
                *pTargetMaskRegister      = LW_PPRIV_SYSC_TARGET_MASK;
                *pTargetMaskPlmRegister   = LW_PPRIV_SYSC_TARGET_MASK_PRIV_LEVEL_MASK;
                *pTargetMaskIndexRegister = LW_PPRIV_SYSC_TARGET_MASK_INDEX;
            #else
                #error "Register not defined for LW_PPRIV_SYS*_PRI_MASTER_fecs2lwdec_pri2";
            #endif
            break;

        case 3:
            #ifdef LW_PPRIV_SYS_PRI_MASTER_fecs2lwdec_pri3
                *pTargetMaskRegister      = LW_PPRIV_SYS_TARGET_MASK;
                *pTargetMaskPlmRegister   = LW_PPRIV_SYS_TARGET_MASK_PRIV_LEVEL_MASK;
                *pTargetMaskIndexRegister = LW_PPRIV_SYS_TARGET_MASK_INDEX;
            #elif defined(LW_PPRIV_SYSB_PRI_MASTER_fecs2lwdec_pri3)
                *pTargetMaskRegister      = LW_PPRIV_SYSB_TARGET_MASK;
                *pTargetMaskPlmRegister   = LW_PPRIV_SYSB_TARGET_MASK_PRIV_LEVEL_MASK;
                *pTargetMaskIndexRegister = LW_PPRIV_SYSB_TARGET_MASK_INDEX;
            #elif defined(LW_PPRIV_SYSC_PRI_MASTER_fecs2lwdec_pri3)
                *pTargetMaskRegister      = LW_PPRIV_SYSC_TARGET_MASK;
                *pTargetMaskPlmRegister   = LW_PPRIV_SYSC_TARGET_MASK_PRIV_LEVEL_MASK;
                *pTargetMaskIndexRegister = LW_PPRIV_SYSC_TARGET_MASK_INDEX;
            #else
                #error "Register not defined for LW_PPRIV_SYS*_PRI_MASTER_fecs2lwdec_pri3";
            #endif
            break;

        case 4:
            #ifdef LW_PPRIV_SYS_PRI_MASTER_fecs2lwdec_pri4
                *pTargetMaskRegister      = LW_PPRIV_SYS_TARGET_MASK;
                *pTargetMaskPlmRegister   = LW_PPRIV_SYS_TARGET_MASK_PRIV_LEVEL_MASK;
                *pTargetMaskIndexRegister = LW_PPRIV_SYS_TARGET_MASK_INDEX;
            #elif defined(LW_PPRIV_SYSB_PRI_MASTER_fecs2lwdec_pri4)
                *pTargetMaskRegister      = LW_PPRIV_SYSB_TARGET_MASK;
                *pTargetMaskPlmRegister   = LW_PPRIV_SYSB_TARGET_MASK_PRIV_LEVEL_MASK;
                *pTargetMaskIndexRegister = LW_PPRIV_SYSB_TARGET_MASK_INDEX;
            #elif defined(LW_PPRIV_SYSC_PRI_MASTER_fecs2lwdec_pri4)
                *pTargetMaskRegister      = LW_PPRIV_SYSC_TARGET_MASK;
                *pTargetMaskPlmRegister   = LW_PPRIV_SYSC_TARGET_MASK_PRIV_LEVEL_MASK;
                *pTargetMaskIndexRegister = LW_PPRIV_SYSC_TARGET_MASK_INDEX;
            #else
                #error "Register not defined for LW_PPRIV_SYS*_PRI_MASTER_fecs2lwdec_pri4";
            #endif
            break;

        case 5:
            #ifdef LW_PPRIV_SYS_PRI_MASTER_fecs2lwdec_pri5
                *pTargetMaskRegister      = LW_PPRIV_SYS_TARGET_MASK;
                *pTargetMaskPlmRegister   = LW_PPRIV_SYS_TARGET_MASK_PRIV_LEVEL_MASK;
                *pTargetMaskIndexRegister = LW_PPRIV_SYS_TARGET_MASK_INDEX;
            #elif defined(LW_PPRIV_SYSB_PRI_MASTER_fecs2lwdec_pri5)
                *pTargetMaskRegister      = LW_PPRIV_SYSB_TARGET_MASK;
                *pTargetMaskPlmRegister   = LW_PPRIV_SYSB_TARGET_MASK_PRIV_LEVEL_MASK;
                *pTargetMaskIndexRegister = LW_PPRIV_SYSB_TARGET_MASK_INDEX;
            #elif defined(LW_PPRIV_SYSC_PRI_MASTER_fecs2lwdec_pri5)
                *pTargetMaskRegister      = LW_PPRIV_SYSC_TARGET_MASK;
                *pTargetMaskPlmRegister   = LW_PPRIV_SYSC_TARGET_MASK_PRIV_LEVEL_MASK;
                *pTargetMaskIndexRegister = LW_PPRIV_SYSC_TARGET_MASK_INDEX;
            #else
                #error "Register not defined for LW_PPRIV_SYS*_PRI_MASTER_fecs2lwdec_pri5";
            #endif
            break;

        case 6:
            #ifdef LW_PPRIV_SYS_PRI_MASTER_fecs2lwdec_pri6
                *pTargetMaskRegister      = LW_PPRIV_SYS_TARGET_MASK;
                *pTargetMaskPlmRegister   = LW_PPRIV_SYS_TARGET_MASK_PRIV_LEVEL_MASK;
                *pTargetMaskIndexRegister = LW_PPRIV_SYS_TARGET_MASK_INDEX;
            #elif defined(LW_PPRIV_SYSB_PRI_MASTER_fecs2lwdec_pri6)
                *pTargetMaskRegister      = LW_PPRIV_SYSB_TARGET_MASK;
                *pTargetMaskPlmRegister   = LW_PPRIV_SYSB_TARGET_MASK_PRIV_LEVEL_MASK;
                *pTargetMaskIndexRegister = LW_PPRIV_SYSB_TARGET_MASK_INDEX;
            #elif defined(LW_PPRIV_SYSC_PRI_MASTER_fecs2lwdec_pri6)
                *pTargetMaskRegister      = LW_PPRIV_SYSC_TARGET_MASK;
                *pTargetMaskPlmRegister   = LW_PPRIV_SYSC_TARGET_MASK_PRIV_LEVEL_MASK;
                *pTargetMaskIndexRegister = LW_PPRIV_SYSC_TARGET_MASK_INDEX;
            #else
                #error "Register not defined for LW_PPRIV_SYS*_PRI_MASTER_fecs2lwdec_pri6";
            #endif
            break;

        case 7:
            #ifdef LW_PPRIV_SYS_PRI_MASTER_fecs2lwdec_pri7
                *pTargetMaskRegister      = LW_PPRIV_SYS_TARGET_MASK;
                *pTargetMaskPlmRegister   = LW_PPRIV_SYS_TARGET_MASK_PRIV_LEVEL_MASK;
                *pTargetMaskIndexRegister = LW_PPRIV_SYS_TARGET_MASK_INDEX;
            #elif defined(LW_PPRIV_SYSB_PRI_MASTER_fecs2lwdec_pri7)
                *pTargetMaskRegister      = LW_PPRIV_SYSB_TARGET_MASK;
                *pTargetMaskPlmRegister   = LW_PPRIV_SYSB_TARGET_MASK_PRIV_LEVEL_MASK;
                *pTargetMaskIndexRegister = LW_PPRIV_SYSB_TARGET_MASK_INDEX;
            #elif defined(LW_PPRIV_SYSC_PRI_MASTER_fecs2lwdec_pri7)
                *pTargetMaskRegister      = LW_PPRIV_SYSC_TARGET_MASK;
                *pTargetMaskPlmRegister   = LW_PPRIV_SYSC_TARGET_MASK_PRIV_LEVEL_MASK;
                *pTargetMaskIndexRegister = LW_PPRIV_SYSC_TARGET_MASK_INDEX;
            #else
                #error "Register not defined for LW_PPRIV_SYS*_PRI_MASTER_fecs2lwdec_pri7";
            #endif
            break;

        default:
            return ACR_ERROR_HAL_NOT_DEFINED_FOR_CHIP;
    }

    return ACR_OK;
#else
    return ACR_OK;
#endif
}

/*!
 * @brief Given falcon ID and instance ID (except LWDEC0), valid LWDEC ID to see if request Id is valid.
 *        specifications such as registerBase, pmcEnableMask etc
 *
 * @param[in]  falconId                      Falcon ID
 * @param[in]  falconInstance                Falcon index/Instance of same falcon
 *
 * @return LW_TRUE if falconId and instance are valid
 */
#if defined(LWDEC_PRESENT) || defined(LWDEC1_PRESENT) || defined(LWDEC_RISCV_PRESENT) || defined(LWDEC_RISCV_EB_PRESENT)
LwBool
acrlibValidateLwdecId_GH100
(
    LwU32 falconId,
    LwU32 falconInstance
)
{
    if ((falconId == LSF_FALCON_ID_LWDEC) || (falconId == LSF_FALCON_ID_LWDEC_RISCV))
    {
        return (falconInstance == 0 ? LW_TRUE : LW_FALSE);
    }

    if (falconId == LSF_FALCON_ID_LWDEC_RISCV_EB)
    {
        //
        // GH100 LWDEC_RISCV-EB is for LWDEC1 ~ LWDEC7 only.
        // But in ACR/RM, we use falcon instance + Falcon Id to distinguish different LWDEC RISCV-EB
        // So LWDEC-1-RISCV-EB, falconId = LSF_FALCON_ID_LWDEC_RISCV_EB and falconInstance = 0
        //    LWDEC-2-RISCV-EB, falconId = LSF_FALCON_ID_LWDEC_RISCV_EB and falconInstance = 1
        //    ... etc
        //
        if (falconInstance >= 0 && falconInstance < (LW_PLWDEC__SIZE_1 - 1))
        {
            return LW_TRUE;
        }
        else
        {
            return LW_FALSE;
        }
    }

    return LW_FALSE;
}

#else

LwBool
acrlibValidateLwdecId_GH100
(
    LwU32 falconId,
    LwU32 falconInstance
)
{
    return LW_FALSE;
}

#endif // LWDEC_PRESENT || LWDEC1_PRESENT || LWDEC_RISCV_PRESENT || LWDEC_RISCV_EB_PRESENT

/*!
 * @brief Given falcon ID and instance ID, valid LWJPG ID to see if request Id is valid.
 *        specifications such as registerBase, pmcEnableMask etc
 *
 * @param[in]  falconId                      Falcon ID
 * @param[in]  falconInstance                Falcon index/Instance of same falcon
 *
 * @return LW_TRUE if falconId and instance are valid
 */
#ifdef LWJPG_PRESENT
LwBool
acrlibValidateLwjpgId_GH100
(
    LwU32 falconId,
    LwU32 falconInstance
)
{
    if (falconId == LSF_FALCON_ID_LWJPG)
    {
        if (falconInstance >= 0 && falconInstance < LW_PLWJPG__SIZE_1)
        {
            return LW_TRUE;
        }
        else
        {
            return LW_FALSE;
        }
    }

    return LW_FALSE;
}

#else

LwBool
acrlibValidateLwjpgId_GH100
(
    LwU32 falconId,
    LwU32 falconInstance
)
{
    return LW_FALSE;
}
#endif // LWJPG_PRESENT

/*!
 * @brief Given falcon ID and instance ID, get the LWDEC falcon
 *        specifications such as registerBase, pmcEnableMask etc
 *
 * @param[in]  falconId                      Falcon ID
 * @param[in]  falconInstance                Falcon index/Instance of same falcon
 * @param[out] pFlcnCfg                      Structure to hold falcon config
 *
 * @return ACR_OK if falconId and instance are valid
 */
#if defined(LWDEC_RISCV_PRESENT) || defined (LWDEC_RISCV_EB_PRESENT)
ACR_STATUS
acrlibGetLwdecFalconConfig_GH100
(
    LwU32                falconId,
    LwU32                falconInstance,
    PACR_FLCN_CONFIG     pFlcnCfg
)
{
    LwBool bValid  = LW_FALSE;
    ACR_STATUS  status = ACR_ERROR_ILWALID_OPERATION;

    bValid = acrlibValidateLwdecId_HAL(pAcrlib, falconId, falconInstance);

    if (!bValid)
    {
        return ACR_ERROR_FLCN_ID_NOT_FOUND;
    }

    pFlcnCfg->subWprRangePlmAddr  = LW_PFB_PRI_MMU_LWDEC_PRIV_LEVEL_MASK;
    pFlcnCfg->range0Addr          = 0;
    pFlcnCfg->range1Addr          = 0;
    pFlcnCfg->bOpenCarve          = LW_FALSE;
    pFlcnCfg->bFbifPresent        = LW_TRUE;
    pFlcnCfg->ctxDma              = LSF_BOOTSTRAP_CTX_DMA_VIDEO;
    pFlcnCfg->bStackCfgSupported  = LW_TRUE;

    if (falconId == LSF_FALCON_ID_LWDEC_RISCV && falconInstance == 0)
    {
        pFlcnCfg->registerBase        = DRF_BASE(LW_PLWDEC(0));
        pFlcnCfg->falcon2RegisterBase = LW_FALCON2_LWDEC0_BASE;
        pFlcnCfg->targetMaskIndex     = LW_PPRIV_SYSC_PRI_MASTER_sys_pri_hub2lwdec_pri0;
        pFlcnCfg->subWprRangeAddrBase = LW_PFB_PRI_MMU_FALCON_LWDEC0_CFGA(0);
        pFlcnCfg->regSelwreResetAddr  = LW_PLWDEC_FALCON_ENGINE(0);
        pFlcnCfg->fbifBase            = LW_PLWDEC_FBIF_TRANSCFG(0,0);
        pFlcnCfg->riscvRegisterBase   = LW_FALCON2_LWDEC0_BASE;
        pFlcnCfg->uprocType           = ACR_TARGET_ENGINE_CORE_RISCV;
        CHECK_STATUS_AND_RETURN_IF_NOT_OK(acrlibGetLwdecTargetMaskIndex_HAL(pAcrLib, pFlcnCfg, &pFlcnCfg->targetMaskIndex));
        bValid                        = LW_TRUE;
    }
    else
    {
        if (falconId == LSF_FALCON_ID_LWDEC_RISCV_EB)
        {
            pFlcnCfg->uprocType = ACR_TARGET_ENGINE_CORE_RISCV_EB;
            pFlcnCfg->registerBase        = DRF_BASE(LW_PLWDEC(falconInstance + 1));
            pFlcnCfg->regSelwreResetAddr  = LW_PLWDEC_FALCON_ENGINE(falconInstance + 1);
            pFlcnCfg->fbifBase            = LW_PLWDEC_FBIF_TRANSCFG(falconInstance + 1,0);

            switch (falconInstance)
            {
                case 0:
                    pFlcnCfg->falcon2RegisterBase = LW_FALCON2_LWDEC1_BASE;
                    pFlcnCfg->subWprRangeAddrBase = LW_PFB_PRI_MMU_FALCON_LWDEC1_CFGA(0);
                    pFlcnCfg->riscvRegisterBase   = LW_FALCON2_LWDEC1_BASE;
                    CHECK_STATUS_AND_RETURN_IF_NOT_OK(acrlibGetLwdecTargetMaskIndex_HAL(pAcrLib, pFlcnCfg, &pFlcnCfg->targetMaskIndex));
                    bValid                        = LW_TRUE;
                    break;

                case 1:
                    pFlcnCfg->falcon2RegisterBase = LW_FALCON2_LWDEC2_BASE;
                    pFlcnCfg->subWprRangeAddrBase = LW_PFB_PRI_MMU_FALCON_LWDEC2_CFGA(0);
                    pFlcnCfg->riscvRegisterBase   = LW_FALCON2_LWDEC2_BASE;
                    CHECK_STATUS_AND_RETURN_IF_NOT_OK(acrlibGetLwdecTargetMaskIndex_HAL(pAcrLib, pFlcnCfg, &pFlcnCfg->targetMaskIndex));
                    bValid                        = LW_TRUE;
                    break;

                case 2:
                    pFlcnCfg->falcon2RegisterBase = LW_FALCON2_LWDEC3_BASE;
                    pFlcnCfg->subWprRangeAddrBase = LW_PFB_PRI_MMU_FALCON_LWDEC3_CFGA(0);
                    pFlcnCfg->riscvRegisterBase   = LW_FALCON2_LWDEC3_BASE;
                    CHECK_STATUS_AND_RETURN_IF_NOT_OK(acrlibGetLwdecTargetMaskIndex_HAL(pAcrLib, pFlcnCfg, &pFlcnCfg->targetMaskIndex));
                    bValid                        = LW_TRUE;
                    break;

                case 3:
                    pFlcnCfg->falcon2RegisterBase = LW_FALCON2_LWDEC4_BASE;
                    pFlcnCfg->subWprRangeAddrBase = LW_PFB_PRI_MMU_FALCON_LWDEC4_CFGA(0);
                    pFlcnCfg->riscvRegisterBase   = LW_FALCON2_LWDEC4_BASE;
                    CHECK_STATUS_AND_RETURN_IF_NOT_OK(acrlibGetLwdecTargetMaskIndex_HAL(pAcrLib, pFlcnCfg, &pFlcnCfg->targetMaskIndex));
                    bValid                        = LW_TRUE;
                    break;

                case 4:
                    pFlcnCfg->falcon2RegisterBase = LW_FALCON2_LWDEC5_BASE;
                    pFlcnCfg->subWprRangeAddrBase = LW_PFB_PRI_MMU_FALCON_LWDEC5_CFGA(0);
                    pFlcnCfg->riscvRegisterBase   = LW_FALCON2_LWDEC5_BASE;
                    CHECK_STATUS_AND_RETURN_IF_NOT_OK(acrlibGetLwdecTargetMaskIndex_HAL(pAcrLib, pFlcnCfg, &pFlcnCfg->targetMaskIndex));
                    bValid                        = LW_TRUE;
                    break;

                case 5:
                    pFlcnCfg->falcon2RegisterBase = LW_FALCON2_LWDEC6_BASE;
                    pFlcnCfg->subWprRangeAddrBase = LW_PFB_PRI_MMU_FALCON_LWDEC6_CFGA(0);
                    pFlcnCfg->riscvRegisterBase   = LW_FALCON2_LWDEC6_BASE;
                    CHECK_STATUS_AND_RETURN_IF_NOT_OK(acrlibGetLwdecTargetMaskIndex_HAL(pAcrLib, pFlcnCfg, &pFlcnCfg->targetMaskIndex));
                    bValid                        = LW_TRUE;
                    break;

                case 6:
                    pFlcnCfg->falcon2RegisterBase = LW_FALCON2_LWDEC7_BASE;
                    pFlcnCfg->subWprRangeAddrBase = LW_PFB_PRI_MMU_FALCON_LWDEC7_CFGA(0);
                    pFlcnCfg->riscvRegisterBase   = LW_FALCON2_LWDEC7_BASE;
                    CHECK_STATUS_AND_RETURN_IF_NOT_OK(acrlibGetLwdecTargetMaskIndex_HAL(pAcrLib, pFlcnCfg, &pFlcnCfg->targetMaskIndex));
                    bValid                        = LW_TRUE;
                    break;

                default:
                    bValid = LW_FALSE;
            }
        }
    }

    return (bValid == LW_TRUE ? ACR_OK : ACR_ERROR_FLCN_ID_NOT_FOUND);
}

#else

ACR_STATUS
acrlibGetLwdecFalconConfig_GH100
(
    LwU32                falconId,
    LwU32                falconInstance,
    PACR_FLCN_CONFIG     pFlcnCfg
)
{
    return ACR_ERROR_FLCN_ID_NOT_FOUND;
}

#endif // LWDEC_RISCV_PRESENT || LWDEC_RISCV_EB_PRESENT

#ifdef LWJPG_PRESENT
/*!
 * @brief Return the target mask register details for LWDEC
 */
ACR_STATUS
acrlibGetLwjpgTargetMaskRegisterDetails_GH100
(
    PACR_FLCN_CONFIG pFlcnCfg,
    LwU32 *pTargetMaskRegister,
    LwU32 *pTargetMaskPlmRegister,
    LwU32 *pTargetMaskIndexRegister
)
{
    //
    // All the three argument pointers need to be valid for this function.
    // Thus, return error if any of them is NULL.
    //
    if ((pTargetMaskRegister == NULL) || (pTargetMaskPlmRegister == NULL) || (pTargetMaskIndexRegister == NULL))
    {
        return ACR_ERROR_ILWALID_ARGUMENT;
    }

    switch (pFlcnCfg->falconInstance)
    {
        case 0:
        case 1:
        case 2:
            *pTargetMaskRegister      = LW_PPRIV_SYSC_TARGET_MASK;
            *pTargetMaskPlmRegister   = LW_PPRIV_SYSC_TARGET_MASK_PRIV_LEVEL_MASK;
            *pTargetMaskIndexRegister = LW_PPRIV_SYSC_TARGET_MASK_INDEX;
            break;
        case 3:
        case 4:
        case 5:
        case 6:
        case 7:
            *pTargetMaskRegister      = LW_PPRIV_SYSB_TARGET_MASK;
            *pTargetMaskPlmRegister   = LW_PPRIV_SYSB_TARGET_MASK_PRIV_LEVEL_MASK;
            *pTargetMaskIndexRegister = LW_PPRIV_SYSB_TARGET_MASK_INDEX;
            break;
        default:
            return ACR_ERROR_ILWALID_ARGUMENT;

    }

    return ACR_OK;
}

/*!
 * @brief Given falcon ID and instance ID, get the LWJPG falcon
 *        specifications such as registerBase, pmcEnableMask etc
 *
 * @param[in]  falconId                      Falcon ID
 * @param[in]  falconInstance                Falcon index/Instance of same falcon
 * @param[out] pFlcnCfg                      Structure to hold falcon config
 *
 * @return ACR_OK if falconId and instance are valid
 */



ACR_STATUS
acrlibGetLwjpgFalconConfig_GH100
(
    LwU32                falconId,
    LwU32                falconInstance,
    PACR_FLCN_CONFIG     pFlcnCfg
)
{
    LwBool bValid  = LW_FALSE;

    bValid = acrlibValidateLwjpgId_HAL(pAcrlib, falconId, falconInstance);

    if (!bValid)
    {
        return ACR_ERROR_FLCN_ID_NOT_FOUND;
    }

    pFlcnCfg->falcon2RegisterBase = 0; // TODO: eddichang: need to set this field?
    pFlcnCfg->scpP2pCtl           = 0;
    pFlcnCfg->subWprRangePlmAddr  = LW_PFB_PRI_MMU_LWJPG_PRIV_LEVEL_MASK;
    pFlcnCfg->registerBase        = DRF_BASE(LW_PLWJPG(falconInstance));
    pFlcnCfg->range0Addr          = 0;
    pFlcnCfg->range1Addr          = 0;
    pFlcnCfg->bOpenCarve          = LW_FALSE;
    pFlcnCfg->bFbifPresent        = LW_TRUE;
    pFlcnCfg->ctxDma              = LSF_BOOTSTRAP_CTX_DMA_VIDEO;
    pFlcnCfg->bStackCfgSupported  = LW_TRUE;

    bValid = LW_FALSE;

    pFlcnCfg->regSelwreResetAddr  = LW_PLWJPG_FALCON_ENGINE(falconInstance);
    pFlcnCfg->fbifBase            = LW_PLWJPG_FBIF_TRANSCFG(falconInstance, 0);

    switch (falconInstance)
    {
        case 0:
            pFlcnCfg->targetMaskIndex     = LW_PPRIV_SYSC_PRI_MASTER_sys_pri_hub2lwjpg_pri0;
            pFlcnCfg->subWprRangeAddrBase = LW_PFB_PRI_MMU_FALCON_LWJPG0_CFGA(0);
            bValid                        = LW_TRUE;
            break;

        case 1:
            pFlcnCfg->targetMaskIndex     = LW_PPRIV_SYSC_PRI_MASTER_sys_pri_hub2lwjpg_pri1;
            pFlcnCfg->subWprRangeAddrBase = LW_PFB_PRI_MMU_FALCON_LWJPG1_CFGA(0);
            bValid                        = LW_TRUE;
            break;

        case 2:
            pFlcnCfg->targetMaskIndex     = LW_PPRIV_SYSC_PRI_MASTER_sys_pri_hub2lwjpg_pri2;
            pFlcnCfg->subWprRangeAddrBase = LW_PFB_PRI_MMU_FALCON_LWJPG2_CFGA(0);
            bValid                        = LW_TRUE;
            break;

        case 3:
            pFlcnCfg->targetMaskIndex     = LW_PPRIV_SYSB_PRI_MASTER_sys_pri_hub2lwjpg_pri3;
            pFlcnCfg->subWprRangeAddrBase = LW_PFB_PRI_MMU_FALCON_LWJPG3_CFGA(0);
            bValid                        = LW_TRUE;
            break;

        case 4:
            pFlcnCfg->targetMaskIndex     = LW_PPRIV_SYSB_PRI_MASTER_sys_pri_hub2lwjpg_pri4;
            pFlcnCfg->subWprRangeAddrBase = LW_PFB_PRI_MMU_FALCON_LWJPG4_CFGA(0);
            bValid                        = LW_TRUE;
            break;

        case 5:
            pFlcnCfg->targetMaskIndex     = LW_PPRIV_SYSB_PRI_MASTER_sys_pri_hub2lwjpg_pri5;
            pFlcnCfg->subWprRangeAddrBase = LW_PFB_PRI_MMU_FALCON_LWJPG5_CFGA(0);
            bValid                        = LW_TRUE;
            break;

        case 6:
            pFlcnCfg->targetMaskIndex     = LW_PPRIV_SYSB_PRI_MASTER_sys_pri_hub2lwjpg_pri6;
            pFlcnCfg->subWprRangeAddrBase = LW_PFB_PRI_MMU_FALCON_LWJPG6_CFGA(0);
            bValid                        = LW_TRUE;
            break;

        case 7:
            pFlcnCfg->targetMaskIndex     = LW_PPRIV_SYSB_PRI_MASTER_sys_pri_hub2lwjpg_pri7;
            pFlcnCfg->subWprRangeAddrBase = LW_PFB_PRI_MMU_FALCON_LWJPG7_CFGA(0);
            bValid                        = LW_TRUE;
            break;

        default:
            bValid = LW_FALSE;
    }

    return (bValid == LW_TRUE ? ACR_OK : ACR_ERROR_FLCN_ID_NOT_FOUND);
}

#else

ACR_STATUS
acrlibGetLwjpgFalconConfig_GH100
(
    LwU32                falconId,
    LwU32                falconInstance,
    PACR_FLCN_CONFIG     pFlcnCfg
)
{
    return ACR_ERROR_FLCN_ID_NOT_FOUND;
}

#endif //LWJPG_PRESENT

/*!
 * @brief Given pFlcnCfg to get IMEM size in byte per current chip base.
 *
 * @param[in]  pFlcnCfg      Structure to hold falcon config
 * @param[out] *pSizeInByte  A pointer to save return IMEM size
 *
 * @return ACR_OK
 */
ACR_STATUS
acrlibGetImemSize_GH100
(
    PACR_FLCN_CONFIG pFlcnCfg,
    LwU32 *pSizeInByte
)
{
    LwU32 hwcfg    = acrlibFlcnRegLabelRead_HAL(pAcrlib, pFlcnCfg, REG_LABEL_FLCN_HWCFG);
    LwU32 imemSize = DRF_VAL(_PFALCON_FALCON, _HWCFG, _IMEM_SIZE, hwcfg);

    if (pSizeInByte == NULL)
    {
        return ACR_ERROR_ILWALID_ARGUMENT;
    }

    *pSizeInByte = (imemSize << FLCN_IMEM_BLK_ALIGN_BITS);

    return ACR_OK;
}

/*!
 * @brief Given pFlcnCfg to get DMEM size in byte per current chip base.
 *
 * @param[in]  pFlcnCfg      Structure to hold falcon config
 * @param[out] *pSizeInByte  A pointer to save return DMEM size
 *
 * @return ACR_OK
 */
ACR_STATUS
acrlibGetDmemSize_GH100
(
    PACR_FLCN_CONFIG pFlcnCfg,
    LwU32 *pSizeInByte
)
{
    LwU32 hwcfg    = acrlibFlcnRegLabelRead_HAL(pAcrlib, pFlcnCfg, REG_LABEL_FLCN_HWCFG);
    LwU32 dmemSize = DRF_VAL(_PFALCON_FALCON, _HWCFG, _DMEM_SIZE, hwcfg);

    if (pSizeInByte == NULL)
    {
        return ACR_ERROR_ILWALID_ARGUMENT;
    }

    *pSizeInByte = (dmemSize << FLCN_DMEM_BLK_ALIGN_BITS);

    return ACR_OK;
}

/*!
 * @brief Given memTarget to get corresponding RISCV physical address.
 *
 * @param[in]  memTarget      RISCV memory target type
 * @param[in]  offset         offset in range of memory target
 * @param[out] *pPhyAddr      A pointer to save return physical address
 *
 * @return ACR_OK if no error.
 */
ACR_STATUS
acrlibRiscvGetPhysicalAddress_GH100
(
    LW_RISCV_MEM_TARGET memTarget,
    LwU64  offset,
    LwU64 *pPhyAddr
)
{
    LwU64 rangeStart, rangeSize;

    if (pPhyAddr == NULL)
    {
        return ACR_ERROR_ILWALID_ARGUMENT;
    }

    switch(memTarget)
    {
        case LW_RISCV_MEM_IROM:
            rangeStart = LW_RISCV_AMAP_IROM_START;
            rangeSize  = LW_RISCV_AMAP_IROM_SIZE;
            break;
        case LW_RISCV_MEM_IMEM:
            rangeStart = LW_RISCV_AMAP_IMEM_START;
            rangeSize  = LW_RISCV_AMAP_IMEM_SIZE;
            break;
        case LW_RISCV_MEM_DMEM:
            rangeStart = LW_RISCV_AMAP_DMEM_START;
            rangeSize  = LW_RISCV_AMAP_DMEM_SIZE;
            break;
        case LW_RISCV_MEM_EMEM:
            rangeStart = LW_RISCV_AMAP_EMEM_START;
            rangeSize  = LW_RISCV_AMAP_EMEM_SIZE;
            break;
        case LW_RISCV_MEM_PRIV:
            rangeStart = LW_RISCV_AMAP_PRIV_START;
            rangeSize  = LW_RISCV_AMAP_PRIV_SIZE;
            break;
        case LW_RISCV_MEM_FBGPA:
            rangeStart = LW_RISCV_AMAP_FBGPA_START;
            rangeSize  = LW_RISCV_AMAP_FBGPA_SIZE;
            break;
        case LW_RISCV_MEM_SYSGPA:
            rangeStart = LW_RISCV_AMAP_SYSGPA_START;
            rangeSize  = LW_RISCV_AMAP_SYSGPA_SIZE;
            break;
        case LW_RISCV_MEM_GVA:
            rangeStart = LW_RISCV_AMAP_GVA_START;
            rangeSize  = LW_RISCV_AMAP_GVA_SIZE;
            break;
        default:
            return LW_ERR_ILWALID_INDEX;
    }
    if (offset > rangeSize)
    {
        return ACR_ERROR_ILWALID_ARGUMENT;
    }

    *pPhyAddr = rangeStart + offset;

    return ACR_OK;
}

/*
* @brief: Return Falcon instance count on the current chip
*/
ACR_STATUS
acrlibGetFalconInstanceCount_GH100
(
    LwU32  falconId,
    LwU32 *pInstanceCount
)
{
    //
    // TODO: need to use flow sweep to check engine count rather than hard code.
    // Refer to  lwdecIsInstanceAllowed_GA102
    //
    if (pInstanceCount == NULL)
    {
        return ACR_ERROR_ILWALID_ARGUMENT;
    }

#ifdef LWDEC_RISCV_EB_PRESENT
    if (falconId == LSF_FALCON_ID_LWDEC_RISCV_EB)
    {
        *pInstanceCount = LW_PLWDEC__SIZE_1 - 1;
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

    *pInstanceCount = LSF_FALCON_INSTANCE_COUNT_DEFAULT_1;

    return ACR_OK;
}

