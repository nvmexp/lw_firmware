/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2020-2022 by LWPU Corporation.  All rights reserved.  All
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
#include "acrdrfmacros.h"
#include "dev_se_seb.h"
#include "dev_se_seb_addendum.h"
#include "dev_pwr_pri.h"
#include "dev_falcon_v4.h"
#include "dev_gsp.h"
#include "dev_fb.h"
#include "dev_top.h"
#include "dev_master.h"
#include "dev_fbfalcon_pri.h"
#include "dev_sec_pri.h"
#include "dev_graphics_nobundle.h"
#include "dev_fbif_v4.h"
#include "sec2/sec2ifcmn.h"
#include "gsp/gspifcmn.h"
#include "dev_pri_ringstation_sys.h"
#include "dev_smcarb_addendum.h"
#include "dev_pri_ringstation_gpc.h"
#include "lw_riscv_address_map.h"
#include "dev_sec_addendum.h"
#include "hw_rts_msr_usage.h"
#include <lwriscv/csr.h>
#include "dev_pri_ringstation_sys_addendum.h"
#include "dev_fuse_off.h"

#if LW_SCAL_LITTER_NUM_SYSB != 0
#include "dev_pri_ringstation_sysb.h"
#endif

#if LW_SCAL_LITTER_NUM_SYSC != 0
#include "dev_pri_ringstation_sysc.h"
#endif


#define SMC_LEGACY_UNICAST_ADDR(addr, instance) (addr + (LW_GPC_PRI_STRIDE * instance))

#ifdef ACR_RISCV_LS
#include "dev_riscv_pri.h"
#endif


#include "config/g_acr_private.h"

#if defined(LWDEC_PRESENT) || defined(LWDEC1_PRESENT) || defined(LWDEC_RISCV_PRESENT) || defined(LWDEC_RISCV_EB_PRESENT)
#include "dev_lwdec_pri.h"
#endif

#if defined(LWDEC_RISCV_PRESENT) || defined(LWDEC_RISCV_EB_PRESENT)
#include "dev_pri_ringstation_sys.h"
#include "dev_riscv_pri.h"
#include "dev_top.h"
#endif

#ifdef LWENC_PRESENT
#include "dev_lwenc_pri_sw.h"
#include "dev_top.h"
#endif

#ifdef LWJPG_PRESENT
#include "dev_lwjpg_pri_sw.h"
#include "dev_pri_ringstation_sys.h"
#include "dev_top.h"
#endif

#ifdef OFA_PRESENT
#include "dev_ofa_pri.h"
#include "dev_top.h"
#endif

static void _acrInitRegMapping_GH100(ACR_REG_MAPPING acrRegMap[REG_LABEL_END])   ATTR_OVLY(OVL_NAME);

//
// This is added to fix SEC2 and PMU preTuring and Ampere_and_later ACRLIB builds
// TODO: Get this define added in Ampere_and_later manuals
// HW bug tracking priv latency improvement - 2117032
// Current value of this timeout is taken from http://lwbugs/2198976/285
//
#ifndef LW_CSEC_BAR0_TMOUT_CYCLES_PROD
#define LW_CSEC_BAR0_TMOUT_CYCLES_PROD    (0x01312d00)
#endif //LW_CSEC_BAR0_TMOUT_CYCLES_PROD

/*!
 * @brief Find reg mapping using reg label
 *
 */
ACR_STATUS
acrFindRegMapping_GH100
(
    PACR_FLCN_CONFIG    pFlcnCfg,
    ACR_FLCN_REG_LABEL  acrLabel,
    PACR_REG_MAPPING    pMap,
    PFLCN_REG_TGT       pTgt
)
{
    ACR_REG_MAPPING acrRegMap[REG_LABEL_END];
    if ((acrLabel >= REG_LABEL_END) || (!pMap) || (!pTgt) ||
        (acrLabel == REG_LABEL_FLCN_END) ||
#ifdef ACR_RISCV_LS
        (acrLabel == REG_LABEL_RISCV_END) ||
#endif
        (acrLabel == REG_LABEL_FBIF_END))
        return ACR_ERROR_REG_MAPPING;

    if (pFlcnCfg->bIsBoOwner)
    {
        *pTgt = PRGNLCL_RISCV;
    }
    else if (acrLabel < REG_LABEL_FLCN_END)
    {
        *pTgt = BAR0_FLCN;
    }
#ifdef ACR_RISCV_LS
    else if (acrLabel < REG_LABEL_RISCV_END)
    {
        if (pFlcnCfg->uprocType != ACR_TARGET_ENGINE_CORE_RISCV &&
            pFlcnCfg->uprocType != ACR_TARGET_ENGINE_CORE_RISCV_EB)
        {
            return ACR_ERROR_REG_MAPPING;
        }

        *pTgt = BAR0_RISCV;
    }
#endif
    else
    {
        *pTgt = BAR0_FBIF;
    }

    _acrInitRegMapping_GH100(acrRegMap);

    pMap->bar0Off   = acrRegMap[acrLabel].bar0Off;
    pMap->prgnlclOff = acrRegMap[acrLabel].prgnlclOff;

    return ACR_OK;
}

/*!
 * @brief Acr util function to write falcon registers using register label
 *
 * @param[in]  pFlcnCfg  Falcon config
 * @param[in]  pIsFbif   TRUE if register is offset from FBIF base
 * @param[in]  regOff    Register offset
 */
ACR_STATUS
acrFlcnRegLabelWrite_GH100
(
    PACR_FLCN_CONFIG   pFlcnCfg,
    ACR_FLCN_REG_LABEL reglabel,
    LwU32              data
)
{
    ACR_REG_MAPPING regMapping = {0};
    FLCN_REG_TGT     tgt        = ILWALID_TGT;

    acrFindRegMapping_HAL(pAcr, pFlcnCfg, reglabel, &regMapping, &tgt);

    switch(tgt)
    {
        case PRGNLCL_RISCV:
            return acrFlcnRegWrite_HAL(pAcr, pFlcnCfg, tgt, regMapping.prgnlclOff, data);
        case BAR0_FLCN:
        case BAR0_FBIF:
#ifdef ACR_RISCV_LS
        case BAR0_RISCV:
#endif
            return acrFlcnRegWrite_HAL(pAcr, pFlcnCfg, tgt, regMapping.bar0Off, data);
        default:
            return ACR_ERROR_REG_MAPPING;
    }

    return ACR_OK;
}

/*!
 * @brief Acr util function to read falcon registers
 *
 * @param[in]  pFlcnCfg  Falcon config
 * @param[in]  pIsFbif   TRUE if register is offset from FBIF base
 * @param[in]  regOff    Register offset
 */
ACR_STATUS
acrFlcnRegRead_GH100
(
    PACR_FLCN_CONFIG pFlcnCfg,
    FLCN_REG_TGT     tgt,
    LwU32            regOff,
    LwU32            *pRegVal
)
{
    if (pFlcnCfg && (tgt == BAR0_FLCN))
    {
        *pRegVal = ACR_REG_RD32(BAR0, ((pFlcnCfg->registerBase) + regOff));
    }
    else if (tgt == PRGNLCL_RISCV)
    {
        *pRegVal = ACR_REG_RD32(PRGNLCL, regOff);
    }
    else if (pFlcnCfg && (tgt == BAR0_FBIF))
    {
        *pRegVal = ACR_REG_RD32(BAR0, ((pFlcnCfg->fbifBase) + regOff));
    }
    else if (pFlcnCfg && (tgt == BAR0_RISCV))
    {
        *pRegVal = ACR_REG_RD32(BAR0, ((pFlcnCfg->riscvRegisterBase) + regOff));
    }
    else
    {
        return ACR_ERROR_ILWALID_ARGUMENT;
    }

    return ACR_OK;
}

/*!
 * @brief Acr util function to write falcon registers
 *
 * @param[in]  pFlcnCfg  Falcon config
 * @param[in]  pIsFbif   TRUE if register is offset from FBIF base
 * @param[in]  regOff    Register offset
 */
ACR_STATUS
acrFlcnRegWrite_GH100
(
    PACR_FLCN_CONFIG pFlcnCfg,
    FLCN_REG_TGT     tgt,
    LwU32            regOff,
    LwU32            data
)
{
    if (pFlcnCfg && (tgt == BAR0_FLCN))
    {
        ACR_REG_WR32(BAR0, ((pFlcnCfg->registerBase) + regOff), data);
    }
    else if (tgt == PRGNLCL_RISCV)
    {
        ACR_REG_WR32(PRGNLCL, regOff, data);
    }
    else if (pFlcnCfg && (tgt == BAR0_FBIF))
    {
        ACR_REG_WR32(BAR0, ((pFlcnCfg->fbifBase) + regOff), data);
    }
    else if (pFlcnCfg && (tgt == BAR0_RISCV))
    {
        ACR_REG_WR32(BAR0, ((pFlcnCfg->riscvRegisterBase) + regOff), data);
    }
    else
    {
        return ACR_ERROR_ILWALID_ARGUMENT;
    }

    return ACR_OK;
}

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
acrGetFalconConfig_GH100
(
    LwU32                falconId,
    LwU32                falconInstance,
    PACR_FLCN_CONFIG     pFlcnCfg
)
{
    ACR_STATUS  status               = ACR_OK;

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
    pFlcnCfg->pmcEnableRegIdx        = 0;
    pFlcnCfg->ctxDma                 = 0;
    pFlcnCfg->maxSubWprSupportedByHw = 0;
    pFlcnCfg->bOpenCarve             = LW_FALSE;
    pFlcnCfg->bFbifPresent           = LW_FALSE;
    pFlcnCfg->uprocType              = ACR_TARGET_ENGINE_CORE_FALCON;
    pFlcnCfg->targetMaskIndex        = 0;
    pFlcnCfg->msrIndex               = RTS_MSR_INDEX_ILWALID;
    pFlcnCfg->bFmcFlushShadow        = LW_TRUE;
    pFlcnCfg->sizeOfRandNum          = 0;
    pFlcnCfg->bIsNsFalcon            = LW_FALSE;

    switch (falconId)
    {
        case LSF_FALCON_ID_GPCCS:
            //
            // SEC should boot all GPCCS falcons in legacy unicast address space in both SMC/LEGACY modes so as to skip the check of
            // unprotected priv SMC_INFO(for SMC/LEGACY modes) /RS_MAP(for extended priv space) which is vulnerable to attacks.
            // Tracked/More info in bug 2565457.
            //
            pFlcnCfg->registerBase        = SMC_LEGACY_UNICAST_ADDR(LW_PGRAPH_PRI_GPC0_GPCCS_FALCON_IRQSSET, falconInstance);
            // RegCfgAddr and RegCfgMaskAddr are needed only for falcons with no FBIF
            pFlcnCfg->regCfgAddr          = SMC_LEGACY_UNICAST_ADDR(LW_PGRAPH_PRI_GPC0_GPCCS_ARB_FALCON_REGIONCFG, falconInstance);
            pFlcnCfg->regCfgMaskAddr      = SMC_LEGACY_UNICAST_ADDR(LW_PGRAPH_PRI_GPC0_GPCCS_ARB_FALCON_REGIONCFG_PRIV_LEVEL_MASK, falconInstance);
            pFlcnCfg->fbifBase            = 0;
            pFlcnCfg->range0Addr          = 0;
            pFlcnCfg->range1Addr          = 0;
            pFlcnCfg->bOpenCarve          = LW_FALSE;
            pFlcnCfg->bFbifPresent        = LW_FALSE;
            pFlcnCfg->ctxDma              = LSF_BOOTSTRAP_CTX_DMA_FECS;
            pFlcnCfg->targetMaskIndex     = LW_PPRIV_GPC_PRI_MASTER_gpc_pri_hub2gpccs_pri;
            pFlcnCfg->regSelwreResetAddr  = SMC_LEGACY_UNICAST_ADDR(LW_PGRAPH_PRI_GPC0_GPCCS_FALCON_ENGINE,falconInstance);
            pFlcnCfg->msrIndex            = RTS_MSR_INDEX_UCODE_GPCCS0;
            pFlcnCfg->sizeOfRandNum       = ACR_RANDOM_NUM_SIZE_IN_DWORD_1;
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
            // RegCfgAddr and RegCfgMaskAddr are needed only for falcons with no FBIF
            pFlcnCfg->regCfgAddr          = LW_GET_PGRAPH_SMC_REG(LW_PGRAPH_PRI_FECS_ARB_FALCON_REGIONCFG, falconInstance);
            pFlcnCfg->regCfgMaskAddr      = LW_GET_PGRAPH_SMC_REG(LW_PGRAPH_PRI_FECS_ARB_FALCON_REGIONCFG_PRIV_LEVEL_MASK, falconInstance);
            pFlcnCfg->fbifBase            = 0;
            pFlcnCfg->range0Addr          = 0;
            pFlcnCfg->range1Addr          = 0;
            pFlcnCfg->bOpenCarve          = LW_FALSE;
            pFlcnCfg->bFbifPresent        = LW_FALSE;
            pFlcnCfg->ctxDma              = LSF_BOOTSTRAP_CTX_DMA_FECS;
            pFlcnCfg->msrIndex            = RTS_MSR_INDEX_UCODE_FECS0;
            status = acrGetFecsTargetMaskIndex_HAL(pAcr, falconInstance, &pFlcnCfg->targetMaskIndex);
            pFlcnCfg->regSelwreResetAddr  = LW_GET_PGRAPH_SMC_REG(LW_PGRAPH_PRI_FECS_FALCON_ENGINE,falconInstance);
            pFlcnCfg->sizeOfRandNum       = ACR_RANDOM_NUM_SIZE_IN_DWORD_1;
            break;

        case LSF_FALCON_ID_PMU_RISCV:
            pFlcnCfg->targetMaskIndex     = LW_PPRIV_SYS_PRI_MASTER_fecs2pwr_pri;
#ifdef ACR_RISCV_LS
            pFlcnCfg->subWprRangeAddrBase = LW_PFB_PRI_MMU_FALCON_PMU_CFGA(0);
            pFlcnCfg->subWprRangePlmAddr  = LW_PFB_PRI_MMU_PMU_PRIV_LEVEL_MASK;
            pFlcnCfg->registerBase        = LW_PPWR_FALCON_IRQSSET;
            pFlcnCfg->fbifBase            = LW_PPWR_FBIF_TRANSCFG(0);
            pFlcnCfg->bOpenCarve          = LW_TRUE;
            pFlcnCfg->bFbifPresent        = LW_TRUE;
            pFlcnCfg->ctxDma              = RM_PMU_DMAIDX_UCODE;
            pFlcnCfg->range0Addr          = LW_PPWR_FALCON_DMEM_PRIV_RANGE0;
            pFlcnCfg->range1Addr          = LW_PPWR_FALCON_DMEM_PRIV_RANGE1;
            pFlcnCfg->regSelwreResetAddr  = LW_PPWR_FALCON_ENGINE;
            pFlcnCfg->msrIndex            = RTS_MSR_INDEX_UCODE_PMU;
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
#endif // ACR_RISCV_LS
            break;

        case LSF_FALCON_ID_SEC2:
            pFlcnCfg->falcon2RegisterBase = LW_FALCON2_SEC_BASE;
            pFlcnCfg->targetMaskIndex     = LW_PPRIV_SYS_PRI_MASTER_fecs2sec_pri;
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
            pFlcnCfg->regSelwreResetAddr  = LW_PSEC_FALCON_ENGINE;
            break;

        case LSF_FALCON_ID_SEC2_RISCV:
            pFlcnCfg->targetMaskIndex     = LW_PPRIV_SYS_PRI_MASTER_fecs2sec_pri;
#ifdef ACR_RISCV_LS
            pFlcnCfg->subWprRangeAddrBase = LW_PFB_PRI_MMU_FALCON_SEC_CFGA(0);
            pFlcnCfg->subWprRangePlmAddr  = LW_PFB_PRI_MMU_SEC_PRIV_LEVEL_MASK;
            pFlcnCfg->registerBase        = LW_PSEC_FALCON_IRQSSET;
            pFlcnCfg->riscvRegisterBase   = LW_FALCON2_SEC_BASE;
            pFlcnCfg->fbifBase            = LW_PSEC_FBIF_TRANSCFG(0);
            pFlcnCfg->bOpenCarve          = LW_FALSE;
            pFlcnCfg->bFbifPresent        = LW_TRUE;
            pFlcnCfg->ctxDma              = RM_SEC2_DMAIDX_UCODE;
            pFlcnCfg->range0Addr          = 0;
            pFlcnCfg->range1Addr          = 0;
            pFlcnCfg->regSelwreResetAddr  = LW_PSEC_FALCON_ENGINE;
            pFlcnCfg->bStackCfgSupported  = LW_TRUE;
            pFlcnCfg->uprocType           = ACR_TARGET_ENGINE_CORE_RISCV;
            pFlcnCfg->msrIndex            = RTS_MSR_INDEX_UCODE_SEC2;
#endif // ACR_RISCV_LS
            break;

        // QS and onwards prod signed only support GSP falcon LS boot
#ifndef BOOT_FROM_HS_BUILD
        case LSF_FALCON_ID_GSP_RISCV:
            pFlcnCfg->targetMaskIndex     = LW_PPRIV_SYS_PRI_MASTER_fecs2gsp_pri;
#if defined(ACR_RISCV_LS)
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
            pFlcnCfg->msrIndex            = RTS_MSR_INDEX_UCODE_GSP;
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
#endif // ACR_RISCV_LS
            break;
#endif // BOOT_FROM_HS_BUILD

        case LSF_FALCON_ID_LWDEC:
            pFlcnCfg->falcon2RegisterBase = LW_FALCON2_LWDEC0_BASE;
            pFlcnCfg->subWprRangeAddrBase = LW_PFB_PRI_MMU_FALCON_LWDEC0_CFGA(0);
            pFlcnCfg->subWprRangePlmAddr  = LW_PFB_PRI_MMU_LWDEC_PRIV_LEVEL_MASK;
            //
            // HW changes BIT define to FIELD define,  INFO_RESET_BIT_XXX <bit_offset> to INFO_RESET_FIELD_XXX(bit_end : bit_start)
            // Even bit offset exceeds 32, HW stills use 32:32, 33:33 ...  likewise definitions;
            // so we can callwlate reg index and bit index for LW_PMC_DEVICE_ENABLE(i).
            //
            pFlcnCfg->registerBase        = LW_FALCON_LWDEC_BASE;
            pFlcnCfg->fbifBase            = LW_PLWDEC_FBIF_TRANSCFG(0,0);
            pFlcnCfg->range0Addr          = 0;
            pFlcnCfg->range1Addr          = 0;
            pFlcnCfg->bOpenCarve          = LW_FALSE;
            pFlcnCfg->bFbifPresent        = LW_TRUE;
            pFlcnCfg->ctxDma              = LSF_BOOTSTRAP_CTX_DMA_VIDEO;
            pFlcnCfg->bStackCfgSupported  = LW_TRUE;
            pFlcnCfg->regSelwreResetAddr  = LW_PLWDEC_FALCON_ENGINE(0);
            status = acrGetLwdecTargetMaskIndex_HAL(pAcr, pFlcnCfg, &pFlcnCfg->targetMaskIndex);
            break;

        case LSF_FALCON_ID_FBFALCON:
            status = acrGetFbFalconTargetMaskIndex_HAL(pAcr, &pFlcnCfg->targetMaskIndex);
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
            pFlcnCfg->msrIndex            = RTS_MSR_INDEX_UCODE_FBFLCN;
            break;

#ifdef LWENC_PRESENT
        case LSF_FALCON_ID_LWENC:
            status = acrGetLwencFalconConfig_HAL(pAcr, falconId, falconInstance, pFlcnCfg);
            break;
#endif

#ifdef LWDEC1_PRESENT
        case LSF_FALCON_ID_LWDEC1:
            pFlcnCfg->registerBase           = DRF_BASE(LW_PLWDEC(1));
            pFlcnCfg->fbifBase               = LW_PLWDEC_FBIF_TRANSCFG(1,0);
            pFlcnCfg->ctxDma                 = LSF_BOOTSTRAP_CTX_DMA_VIDEO;
            pFlcnCfg->subWprRangeAddrBase    = LW_PFB_PRI_MMU_FALCON_LWDEC1_CFGA(0);
            pFlcnCfg->subWprRangePlmAddr     = LW_PFB_PRI_MMU_LWDEC_PRIV_LEVEL_MASK;
            pFlcnCfg->maxSubWprSupportedByHw = LW_PFB_PRI_MMU_FALCON_LWDEC1_CFGA__SIZE_1;
            pFlcnCfg->bFbifPresent           = LW_TRUE;
            pFlcnCfg->bStackCfgSupported     = LW_TRUE;
            pFlcnCfg->falcon2RegisterBase    = LW_FALCON2_LWDEC1_BASE;
            pFlcnCfg->regSelwreResetAddr     = LW_PLWDEC_FALCON_ENGINE(1);
            pFlcnCfg->bIsNsFalcon            = LW_TRUE;
            status = acrGetLwdecTargetMaskIndex_HAL(pAcr, pFlcnCfg, &pFlcnCfg->targetMaskIndex);
        break;
#endif

#ifdef LWDEC_RISCV_PRESENT
        case LSF_FALCON_ID_LWDEC_RISCV:
            pFlcnCfg->sizeOfRandNum          = ACR_RANDOM_NUM_SIZE_IN_DWORD_2;
#endif

#ifdef LWDEC_RISCV_EB_PRESENT
        case LSF_FALCON_ID_LWDEC_RISCV_EB:
            pFlcnCfg->sizeOfRandNum          = ACR_RANDOM_NUM_SIZE_IN_DWORD_2;
#endif

#if  defined(LWDEC_RISCV_PRESENT) || defined(LWDEC_RISCV_EB_PRESENT)
            status = acrGetLwdecFalconConfig_HAL(pAcr, falconId, falconInstance, pFlcnCfg);
            break;
#endif

#ifdef LWJPG_PRESENT
        case LSF_FALCON_ID_LWJPG:
            pFlcnCfg->sizeOfRandNum   = ACR_RANDOM_NUM_SIZE_IN_DWORD_1;
            status                    = acrGetLwjpgFalconConfig_HAL(pAcr, falconId, falconInstance, pFlcnCfg);
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
            pFlcnCfg->msrIndex               = RTS_MSR_INDEX_UCODE_OFA;
            pFlcnCfg->sizeOfRandNum          = ACR_RANDOM_NUM_SIZE_IN_DWORD_1;
            pFlcnCfg->bIsNsFalcon            = LW_TRUE;
        break;
#endif

        default:
            status = ACR_ERROR_FLCN_ID_NOT_FOUND;
            break;
    }

    return status;
}

/*!
 * @brief Acr util function to read falcon registers using register label
 *
 * @param[in]  pFlcnCfg  Falcon config
 * @param[in]  pIsFbif   TRUE if register is offset from FBIF base
 * @param[in]  regOff    Register offset
 */
ACR_STATUS
acrFlcnRegLabelRead_GH100
(
    PACR_FLCN_CONFIG   pFlcnCfg,
    ACR_FLCN_REG_LABEL reglabel,
    LwU32              *pRegVal
)
{
    ACR_STATUS      status     = ACR_OK;
    ACR_REG_MAPPING regMapping = {0};
    FLCN_REG_TGT    tgt        = ILWALID_TGT;
    acrFindRegMapping_HAL(pAcr, pFlcnCfg, reglabel, &regMapping, &tgt);
    switch(tgt)
    {
        case PRGNLCL_RISCV:
            CHECK_STATUS_AND_RETURN_IF_NOT_OK(acrFlcnRegRead_HAL(pAcr, pFlcnCfg, tgt, regMapping.prgnlclOff, pRegVal));
            break;
        case BAR0_FLCN:
        case BAR0_FBIF:
#ifdef ACR_RISCV_LS
        case BAR0_RISCV:
#endif
            CHECK_STATUS_AND_RETURN_IF_NOT_OK(acrFlcnRegRead_HAL(pAcr, pFlcnCfg, tgt, regMapping.bar0Off, pRegVal));
            break;
        default:
            return ACR_ERROR_REG_MAPPING;
    }

    return status;
}

/*!
 * @brief: initializes ACR Register map
 *
 * @param[out] acrRegMap    Table to place register mappings into.
 */
static void
_acrInitRegMapping_GH100(ACR_REG_MAPPING acrRegMap[REG_LABEL_END])
{
   // Initialize all mappings
    acrRegMap[REG_LABEL_FLCN_SCTL_PRIV_LEVEL_MASK]       = (ACR_REG_MAPPING){LW_PFALCON_FALCON_SCTL_PRIV_LEVEL_MASK,           LW_PRGNLCL_FALCON_SCTL_PRIV_LEVEL_MASK};
    acrRegMap[REG_LABEL_FLCN_SCTL]                       = (ACR_REG_MAPPING){LW_PFALCON_FALCON_SCTL,                           LW_PRGNLCL_FALCON_SCTL};
    acrRegMap[REG_LABEL_FLCN_CPUCTL]                     = (ACR_REG_MAPPING){LW_PFALCON_FALCON_CPUCTL,                         LW_PRGNLCL_FALCON_CPUCTL};
    acrRegMap[REG_LABEL_FLCN_DMACTL]                     = (ACR_REG_MAPPING){LW_PFALCON_FALCON_DMACTL,                         LW_PRGNLCL_FALCON_DMACTL};
    acrRegMap[REG_LABEL_FLCN_DMATRFBASE]                 = (ACR_REG_MAPPING){LW_PFALCON_FALCON_DMATRFBASE,                     LW_PRGNLCL_FALCON_DMATRFBASE};
    acrRegMap[REG_LABEL_FLCN_DMATRFCMD]                  = (ACR_REG_MAPPING){LW_PFALCON_FALCON_DMATRFCMD,                      LW_PRGNLCL_FALCON_DMATRFCMD};
    acrRegMap[REG_LABEL_FLCN_DMATRFMOFFS]                = (ACR_REG_MAPPING){LW_PFALCON_FALCON_DMATRFMOFFS,                    LW_PRGNLCL_FALCON_DMATRFMOFFS};
    acrRegMap[REG_LABEL_FLCN_DMATRFFBOFFS]               = (ACR_REG_MAPPING){LW_PFALCON_FALCON_DMATRFFBOFFS,                   LW_PRGNLCL_FALCON_DMATRFFBOFFS};
    acrRegMap[REG_LABEL_FLCN_IMEM_PRIV_LEVEL_MASK]       = (ACR_REG_MAPPING){LW_PFALCON_FALCON_IMEM_PRIV_LEVEL_MASK,           LW_PRGNLCL_FALCON_IMEM_PRIV_LEVEL_MASK};
    acrRegMap[REG_LABEL_FLCN_DMEM_PRIV_LEVEL_MASK]       = (ACR_REG_MAPPING){LW_PFALCON_FALCON_DMEM_PRIV_LEVEL_MASK,           LW_PRGNLCL_FALCON_DMEM_PRIV_LEVEL_MASK};
    acrRegMap[REG_LABEL_FLCN_CPUCTL_PRIV_LEVEL_MASK]     = (ACR_REG_MAPPING){LW_PFALCON_FALCON_CPUCTL_PRIV_LEVEL_MASK,         LW_PRGNLCL_FALCON_CPUCTL_PRIV_LEVEL_MASK};
    acrRegMap[REG_LABEL_FLCN_EXE_PRIV_LEVEL_MASK]        = (ACR_REG_MAPPING){LW_PFALCON_FALCON_EXE_PRIV_LEVEL_MASK,            LW_PRGNLCL_FALCON_EXE_PRIV_LEVEL_MASK};
    acrRegMap[REG_LABEL_FLCN_IRQTMR_PRIV_LEVEL_MASK]     = (ACR_REG_MAPPING){LW_PFALCON_FALCON_IRQTMR_PRIV_LEVEL_MASK,         LW_PRGNLCL_FALCON_IRQTMR_PRIV_LEVEL_MASK};
    acrRegMap[REG_LABEL_FLCN_MTHDCTX_PRIV_LEVEL_MASK]    = (ACR_REG_MAPPING){LW_PFALCON_FALCON_MTHDCTX_PRIV_LEVEL_MASK,        LW_PRGNLCL_FALCON_MTHDCTX_PRIV_LEVEL_MASK};
    acrRegMap[REG_LABEL_FLCN_HWCFG]                      = (ACR_REG_MAPPING){LW_PFALCON_FALCON_HWCFG,                          LW_PRGNLCL_FALCON_HWCFG};
    acrRegMap[REG_LABEL_FLCN_BOOTVEC]                    = (ACR_REG_MAPPING){LW_PFALCON_FALCON_BOOTVEC,                        LW_PRGNLCL_FALCON_BOOTVEC};
    acrRegMap[REG_LABEL_FLCN_DBGCTL]                     = (ACR_REG_MAPPING){LW_PFALCON_FALCON_DBGCTL,                         LW_PRGNLCL_FALCON_DBGCTL};
    acrRegMap[REG_LABEL_FLCN_SCPCTL]                     = (ACR_REG_MAPPING){0xFFFFFFFF,                                       LW_PRGNLCL_SCP_CTL_STAT};
    acrRegMap[REG_LABEL_FLCN_BOOTVEC_PRIV_LEVEL_MASK]    = (ACR_REG_MAPPING){LW_PFALCON_FALCON_BOOTVEC_PRIV_LEVEL_MASK,        LW_PRGNLCL_FALCON_BOOTVEC_PRIV_LEVEL_MASK};
    acrRegMap[REG_LABEL_FLCN_DMA_PRIV_LEVEL_MASK]        = (ACR_REG_MAPPING){LW_PFALCON_FALCON_DMA_PRIV_LEVEL_MASK,            LW_PRGNLCL_FALCON_DMA_PRIV_LEVEL_MASK};
    acrRegMap[REG_LABEL_FLCN_RESET_PRIV_LEVEL_MASK]      = (ACR_REG_MAPPING){LW_PFALCON_FALCON_RESET_PRIV_LEVEL_MASK,          LW_PRGNLCL_FALCON_RESET_PRIV_LEVEL_MASK};
    acrRegMap[REG_LABEL_FBIF_REGIONCFG_PRIV_LEVEL_MASK]  = (ACR_REG_MAPPING){LW_PFALCON_FBIF_REGIONCFG_PRIV_LEVEL_MASK,        LW_PRGNLCL_FBIF_REGIONCFG_PRIV_LEVEL_MASK};
    acrRegMap[REG_LABEL_FBIF_REGIONCFG]                  = (ACR_REG_MAPPING){LW_PFALCON_FBIF_REGIONCFG,                        LW_PRGNLCL_FBIF_REGIONCFG};
    acrRegMap[REG_LABEL_FBIF_CTL]                        = (ACR_REG_MAPPING){LW_PFALCON_FBIF_CTL,                              LW_PRGNLCL_FBIF_REGIONCFG};

#ifdef ACR_RISCV_LS
    acrRegMap[REG_LABEL_RISCV_BOOT_VECTOR_LO]            = (ACR_REG_MAPPING){LW_PRISCV_RISCV_BOOT_VECTOR_LO,                   LW_PRGNLCL_RISCV_BOOT_VECTOR_LO};
    acrRegMap[REG_LABEL_RISCV_BOOT_VECTOR_HI]            = (ACR_REG_MAPPING){LW_PRISCV_RISCV_BOOT_VECTOR_HI,                   LW_PRGNLCL_RISCV_BOOT_VECTOR_HI};
    acrRegMap[REG_LABEL_RISCV_CPUCTL]                    = (ACR_REG_MAPPING){LW_PRISCV_RISCV_CPUCTL,                           LW_PRGNLCL_RISCV_CPUCTL};
#endif
}


/*!
 * @brief Get the target mask index for FbFalcon for Hopper
 */
ACR_STATUS
acrGetFbFalconTargetMaskIndex_GH100(LwU32 *pTargetMaskIndex)
{
    if (pTargetMaskIndex == NULL)
    {
        return ACR_ERROR_ILWALID_ARGUMENT;
    }

#ifdef LW_PPRIV_SYSB_PRI_MASTER_fecs2fbfalcon_pri
    *pTargetMaskIndex = LW_PPRIV_SYSB_PRI_MASTER_fecs2fbfalcon_pri;
#elif defined(LW_PPRIV_SYSC_PRI_MASTER_fecs2fbfalcon_pri)
    *pTargetMaskIndex = LW_PPRIV_SYSC_PRI_MASTER_fecs2fbfalcon_pri;
#else
    #error "Register not defined for LW_PPRIV_SYS*_PRI_MASTER_fecs2fbfalcon_pri";
#endif

    return ACR_OK;
}

/*!
 * @brief Return the target mask register details for FbFalcon, for Hopper
 */
ACR_STATUS
acrGetFbFalconTargetMaskRegisterDetails_GH100
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

#ifdef LW_PPRIV_SYSB_PRI_MASTER_fecs2fbfalcon_pri
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
 * @brief Get the target mask index for LWDEC
 */
ACR_STATUS
acrGetLwdecTargetMaskIndex_GH100
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
#endif // LWDEC_RISCV_EB_PRESENT
    else
    {
        return ACR_ERROR_ILWALID_ARGUMENT;
    }

    return ACR_OK;
}

/*!
 * @brief Return the target mask register details for LWDEC
 */
ACR_STATUS
acrGetLwdecTargetMaskRegisterDetails_GH100
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
            #error "Register LW_PPRIV_SYS*_TARGET_MASK* not defined for LW_PPRIV_SYS*_PRI_MASTER_fecs2lwdec_pri0";
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
 * @brief Acr util function to write falcon registers in non-blocking manner
 *
 * @param[in]  pFlcnCfg  Falcon config
 * @param[in]  pIsFbif   TRUE if register is offset from FBIF base
 * @param[in]  regOff    Register offset
 */
ACR_STATUS
acrFlcnRegWriteNonBlocking_GH100
(
    PACR_FLCN_CONFIG pFlcnCfg,
    FLCN_REG_TGT     tgt,
    LwU32            regOff,
    LwU32            data
)
{
    if (pFlcnCfg && (tgt == BAR0_FLCN))
    {
        ACR_REG_WR32_NON_BLOCKING(BAR0, ((pFlcnCfg->registerBase) + regOff), data);
    }
    else
    {
        return ACR_ERROR_ILWALID_ARGUMENT;
    }

    return ACR_OK;
}




/*
 * @brief acrUpdateBootvecPlmLevel3Writeable raises LW_PFALCON_FALCON_BOOTVEC_PRIV_LEVEL_MASK
 *        so that BOOTVEC is writable by only level 3 binaries and readable to all
 *
 */
ACR_STATUS
acrUpdateBootvecPlmLevel3Writeable_GH100
(
  PACR_FLCN_CONFIG   pFlcnCfg
)
{
    ACR_STATUS  status   = ACR_OK;
    LwU32       plmValue = 0;

    if (pFlcnCfg == NULL)
    {
        return ACR_ERROR_ILWALID_ARGUMENT;
    }
    CHECK_STATUS_AND_RETURN_IF_NOT_OK(acrFlcnRegLabelRead_HAL(pAcr, pFlcnCfg, REG_LABEL_FLCN_BOOTVEC_PRIV_LEVEL_MASK, &plmValue));
    plmValue = FLD_SET_DRF(_PFALCON, _FALCON_BOOTVEC_PRIV_LEVEL_MASK, _WRITE_PROTECTION_LEVEL0,  _DISABLE, plmValue);
    plmValue = FLD_SET_DRF(_PFALCON, _FALCON_BOOTVEC_PRIV_LEVEL_MASK, _WRITE_PROTECTION_LEVEL1,  _DISABLE, plmValue);
    plmValue = FLD_SET_DRF(_PFALCON, _FALCON_BOOTVEC_PRIV_LEVEL_MASK, _WRITE_PROTECTION_LEVEL2,  _DISABLE, plmValue);
    CHECK_STATUS_AND_RETURN_IF_NOT_OK(acrFlcnRegLabelWrite_HAL(pAcr, pFlcnCfg, REG_LABEL_FLCN_BOOTVEC_PRIV_LEVEL_MASK, plmValue));

    return status;
}

/*
 * @brief acrUpdateDmaPlmLevel3Writeable_GA10X raises LW_PFALCON_FALCON_DMA_PRIV_LEVEL_MASK
 *        so that DMA registers are writable by only level 3 binaries and readable to all
 *
 */
ACR_STATUS
acrUpdateDmaPlmLevel3Writeable_GH100
(
  PACR_FLCN_CONFIG   pFlcnCfg
)
{
    ACR_STATUS  status   = ACR_OK;
    LwU32       plmValue = 0;
    if (pFlcnCfg == NULL)
    {
        return ACR_ERROR_ILWALID_ARGUMENT;
    }
    CHECK_STATUS_AND_RETURN_IF_NOT_OK(acrFlcnRegLabelRead_HAL(pAcr, pFlcnCfg, REG_LABEL_FLCN_DMA_PRIV_LEVEL_MASK, &plmValue));
    plmValue = FLD_SET_DRF(_PFALCON, _FALCON_DMA_PRIV_LEVEL_MASK, _WRITE_PROTECTION_LEVEL0,  _DISABLE, plmValue);
    plmValue = FLD_SET_DRF(_PFALCON, _FALCON_DMA_PRIV_LEVEL_MASK, _WRITE_PROTECTION_LEVEL2,  _DISABLE, plmValue);
    plmValue = FLD_SET_DRF(_PFALCON, _FALCON_DMA_PRIV_LEVEL_MASK, _WRITE_PROTECTION_LEVEL1,  _DISABLE, plmValue);
    CHECK_STATUS_AND_RETURN_IF_NOT_OK(acrFlcnRegLabelWrite_HAL(pAcr, pFlcnCfg, REG_LABEL_FLCN_DMA_PRIV_LEVEL_MASK, plmValue));

    return status;
}

#ifdef LWENC_PRESENT
ACR_STATUS
acrGetLwencFalconConfig_GH100
(
    LwU32                falconId,
    LwU32                falconInstance,
    PACR_FLCN_CONFIG     pFlcnCfg
)
{
    ACR_STATUS status = FLCN_ERR_ILWALID_ARGUMENT;

    switch (falconInstance)
    {
        case LSF_FALCON_INSTANCE_DEFAULT_0:
            pFlcnCfg->registerBase           = DRF_BASE(LW_PLWENC(0));
            pFlcnCfg->pmcEnableMask          = 1 << (DRF_BASE(LW_PTOP_DEVICE_INFO_RESET_FIELD_FOR_UCODE_AND_VBIOS_ONLY_LWENC0)%32);
            pFlcnCfg->pmcEnableRegIdx        = (DRF_BASE(LW_PTOP_DEVICE_INFO_RESET_FIELD_FOR_UCODE_AND_VBIOS_ONLY_LWENC0)/32);
            pFlcnCfg->fbifBase               = LW_PLWENC_FBIF_TRANSCFG(0,0);
            pFlcnCfg->ctxDma                 = LSF_BOOTSTRAP_CTX_DMA_VIDEO;
            pFlcnCfg->subWprRangeAddrBase    = LW_PFB_PRI_MMU_FALCON_LWENC0_CFGA(0);
            pFlcnCfg->subWprRangePlmAddr     = LW_PFB_PRI_MMU_LWENC_PRIV_LEVEL_MASK;
            pFlcnCfg->maxSubWprSupportedByHw = LW_PFB_PRI_MMU_FALCON_LWENC0_CFGA__SIZE_1;
            pFlcnCfg->bFbifPresent           = LW_TRUE;
            pFlcnCfg->bStackCfgSupported     = LW_TRUE;
            pFlcnCfg->targetMaskIndex        = LW_PPRIV_SYS_PRI_MASTER_fecs2lwenc_pri0;
            pFlcnCfg->bIsNsFalcon            = LW_TRUE;
            status = ACR_OK;
        break;
    }

    return status;
}
#endif

/*!
 * @brief Get the target mask index for FECS engine
 * @param[in] falconInstance    : Engine instance id to be used for SMC use case
 * @param[out] pTargetMaskIndex : Target Mask index to be used
 * @returns ACR_STATUS, ACR_OK if sucessfull else ERROR
 */
ACR_STATUS
acrGetFecsTargetMaskIndex_GH100
(
    LwU32 falconInstance,
    LwU32 *pTargetMaskIndex
)
{
    if (pTargetMaskIndex == NULL)
    {
        return ACR_ERROR_ILWALID_ARGUMENT;
    }

    switch (falconInstance)
    {
        case 0:
             *pTargetMaskIndex = LW_PPRIV_SYS_PRI_MASTER_sys_pri_hub2fecs_pri0;
            break;
        case 1:
             *pTargetMaskIndex = LW_PPRIV_SYS_PRI_MASTER_sys_pri_hub2fecs_pri1;
            break;
        case 2:
             *pTargetMaskIndex = LW_PPRIV_SYS_PRI_MASTER_sys_pri_hub2fecs_pri2;
            break;
        case 3:
             *pTargetMaskIndex = LW_PPRIV_SYS_PRI_MASTER_sys_pri_hub2fecs_pri3;
            break;
        case 4:
             *pTargetMaskIndex = LW_PPRIV_SYS_PRI_MASTER_sys_pri_hub2fecs_pri4;
            break;
        case 5:
             *pTargetMaskIndex = LW_PPRIV_SYS_PRI_MASTER_sys_pri_hub2fecs_pri5;
            break;
        case 6:
             *pTargetMaskIndex = LW_PPRIV_SYS_PRI_MASTER_sys_pri_hub2fecs_pri6;
            break;
        case 7:
             *pTargetMaskIndex = LW_PPRIV_SYS_PRI_MASTER_sys_pri_hub2fecs_pri7;
            break;
        default:
            return ACR_ERROR_ILWALID_ARGUMENT;

    }

    return ACR_OK;
}

/*!
 * @brief Return the target mask register details for GPCCS
 * @param[in] falconInstance            : Engine instance id to be used for SMC use case
 * @param[out] pTargetMaskRegister      : Holds the value of TargetMask Register to be used
 * @param[out] pTargetMaskPlmRegister   : Holds the value of TargeMaskPLM register to be used
 * @param[out] pTargetMaskIndexRegister : Holds the value of TargetMaskIndex register to be used
 *
 * @return ACR_OK if sucessfull else ACR error code.
 *
 */
ACR_STATUS
acrGetGpccsTargetMaskRegisterDetails_GH100
(
    LwU32 falconInstance,
    LwU32 *pTargetMaskRegister,
    LwU32 *pTargetMaskPlmRegister,
    LwU32 *pTargetMaskIndexRegister
)
{
    if (pTargetMaskRegister == NULL || pTargetMaskPlmRegister == NULL || pTargetMaskIndexRegister ==NULL)
    {
        return ACR_ERROR_ILWALID_ARGUMENT;
    }
    switch (falconInstance)
    {
        case 0:
            *pTargetMaskRegister      = LW_PPRIV_GPC_GPC0_TARGET_MASK;
            *pTargetMaskPlmRegister   = LW_PPRIV_GPC_GPC0_TARGET_MASK_PRIV_LEVEL_MASK;
            *pTargetMaskIndexRegister = LW_PPRIV_GPC_GPC0_TARGET_MASK_INDEX;
             break;
        case 1:
            *pTargetMaskRegister      = LW_PPRIV_GPC_GPC1_TARGET_MASK;
            *pTargetMaskPlmRegister   = LW_PPRIV_GPC_GPC1_TARGET_MASK_PRIV_LEVEL_MASK;
            *pTargetMaskIndexRegister = LW_PPRIV_GPC_GPC1_TARGET_MASK_INDEX;
             break;
        case 2:
            *pTargetMaskRegister      = LW_PPRIV_GPC_GPC2_TARGET_MASK;
            *pTargetMaskPlmRegister   = LW_PPRIV_GPC_GPC2_TARGET_MASK_PRIV_LEVEL_MASK;
            *pTargetMaskIndexRegister = LW_PPRIV_GPC_GPC2_TARGET_MASK_INDEX;
             break;
        case 3:
            *pTargetMaskRegister      = LW_PPRIV_GPC_GPC3_TARGET_MASK;
            *pTargetMaskPlmRegister   = LW_PPRIV_GPC_GPC3_TARGET_MASK_PRIV_LEVEL_MASK;
            *pTargetMaskIndexRegister = LW_PPRIV_GPC_GPC3_TARGET_MASK_INDEX;
             break;
        case 4:
            *pTargetMaskRegister      = LW_PPRIV_GPC_GPC4_TARGET_MASK;
            *pTargetMaskPlmRegister   = LW_PPRIV_GPC_GPC4_TARGET_MASK_PRIV_LEVEL_MASK;
            *pTargetMaskIndexRegister = LW_PPRIV_GPC_GPC4_TARGET_MASK_INDEX;
             break;
        case 5:
            *pTargetMaskRegister      = LW_PPRIV_GPC_GPC5_TARGET_MASK;
            *pTargetMaskPlmRegister   = LW_PPRIV_GPC_GPC5_TARGET_MASK_PRIV_LEVEL_MASK;
            *pTargetMaskIndexRegister = LW_PPRIV_GPC_GPC5_TARGET_MASK_INDEX;
             break;
        case 6:
            *pTargetMaskRegister      = LW_PPRIV_GPC_GPC6_TARGET_MASK;
            *pTargetMaskPlmRegister   = LW_PPRIV_GPC_GPC6_TARGET_MASK_PRIV_LEVEL_MASK;
            *pTargetMaskIndexRegister = LW_PPRIV_GPC_GPC6_TARGET_MASK_INDEX;
             break;
        case 7:
            *pTargetMaskRegister      = LW_PPRIV_GPC_GPC7_TARGET_MASK;
            *pTargetMaskPlmRegister   = LW_PPRIV_GPC_GPC7_TARGET_MASK_PRIV_LEVEL_MASK;
            *pTargetMaskIndexRegister = LW_PPRIV_GPC_GPC7_TARGET_MASK_INDEX;
             break;
        default:
            return ACR_ERROR_ILWALID_ARGUMENT;
    }

    return ACR_OK;
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
acrValidateLwdecId_GH100
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
        if (falconInstance < (LW_PLWDEC__SIZE_1 - 1))
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
#endif // LWDEC_PRESENT || LWDEC1_PRESENT || LWDEC_RISCV_PRESENT || LWDEC_RISCV_EB_PRESENT

#ifdef LWJPG_PRESENT
/*
 * @brief Given falcon ID and instance ID, valid LWJPG ID to see if request Id is valid.
 *        specifications such as registerBase, pmcEnableMask etc
 *
 * @param[in]  falconId                      Falcon ID
 * @param[in]  falconInstance                Falcon index/Instance of same falcon
 *
 * @return LW_TRUE if falconId and instance are valid
 */
LwBool
acrValidateLwjpgId_GH100
(
    LwU32 falconId,
    LwU32 falconInstance
)
{
    if (falconId == LSF_FALCON_ID_LWJPG)
    {
        if (falconInstance < LW_PLWJPG__SIZE_1)
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
acrGetLwdecFalconConfig_GH100
(
    LwU32                falconId,
    LwU32                falconInstance,
    PACR_FLCN_CONFIG     pFlcnCfg
)
{
    LwBool bValid = LW_FALSE;
    ACR_STATUS  status = ACR_ERROR_ILWALID_OPERATION;

    bValid = acrValidateLwdecId_HAL(pAcr, falconId, falconInstance);

    if (!bValid)
    {
        return ACR_ERROR_FLCN_ID_NOT_FOUND;
    }

    pFlcnCfg->subWprRangePlmAddr = LW_PFB_PRI_MMU_LWDEC_PRIV_LEVEL_MASK;
    pFlcnCfg->range0Addr = 0;
    pFlcnCfg->range1Addr = 0;
    pFlcnCfg->bOpenCarve = LW_FALSE;
    pFlcnCfg->bFbifPresent = LW_TRUE;
    pFlcnCfg->ctxDma = LSF_BOOTSTRAP_CTX_DMA_VIDEO;
    pFlcnCfg->bStackCfgSupported = LW_TRUE;

    if (falconId == LSF_FALCON_ID_LWDEC_RISCV && falconInstance == 0)
    {
        pFlcnCfg->registerBase          = DRF_BASE(LW_PLWDEC(0));
        pFlcnCfg->falcon2RegisterBase   = LW_FALCON2_LWDEC0_BASE;
        pFlcnCfg->targetMaskIndex       = LW_PPRIV_SYSC_PRI_MASTER_sys_pri_hub2lwdec_pri0;
        pFlcnCfg->subWprRangeAddrBase   = LW_PFB_PRI_MMU_FALCON_LWDEC0_CFGA(0);
        pFlcnCfg->regSelwreResetAddr    = LW_PLWDEC_FALCON_ENGINE(0);
        pFlcnCfg->fbifBase              = LW_PLWDEC_FBIF_TRANSCFG(0, 0);
        pFlcnCfg->riscvRegisterBase     = LW_FALCON2_LWDEC0_BASE;
        pFlcnCfg->uprocType             = ACR_TARGET_ENGINE_CORE_RISCV;
        pFlcnCfg->msrIndex              = RTS_MSR_INDEX_UCODE_LWDEC0;
        CHECK_STATUS_AND_RETURN_IF_NOT_OK(acrGetLwdecTargetMaskIndex_HAL(pAcr, pFlcnCfg, &pFlcnCfg->targetMaskIndex));
        bValid = LW_TRUE;
    }
    else
    {
        if (falconId == LSF_FALCON_ID_LWDEC_RISCV_EB)
        {
            pFlcnCfg->uprocType = ACR_TARGET_ENGINE_CORE_RISCV_EB;
            pFlcnCfg->registerBase = DRF_BASE(LW_PLWDEC(falconInstance + 1));
            pFlcnCfg->regSelwreResetAddr = LW_PLWDEC_FALCON_ENGINE(falconInstance + 1);
            pFlcnCfg->fbifBase = LW_PLWDEC_FBIF_TRANSCFG(falconInstance + 1, 0);
            pFlcnCfg->bIsNsFalcon = LW_TRUE;
            //
            // All LWDEC RISCV-EBs (LWDEC1-7) have same ucode image which
            // is different from LWDEC0.
            //
            pFlcnCfg->msrIndex              = RTS_MSR_INDEX_UCODE_LWDEC1;

            switch (falconInstance)
            {
            case 0:
                pFlcnCfg->falcon2RegisterBase = LW_FALCON2_LWDEC1_BASE;
                pFlcnCfg->subWprRangeAddrBase = LW_PFB_PRI_MMU_FALCON_LWDEC1_CFGA(0);
                pFlcnCfg->riscvRegisterBase = LW_FALCON2_LWDEC1_BASE;
                CHECK_STATUS_AND_RETURN_IF_NOT_OK(acrGetLwdecTargetMaskIndex_HAL(pAcr, pFlcnCfg, &pFlcnCfg->targetMaskIndex));
                bValid = LW_TRUE;
                break;

            case 1:
                pFlcnCfg->falcon2RegisterBase = LW_FALCON2_LWDEC2_BASE;
                pFlcnCfg->subWprRangeAddrBase = LW_PFB_PRI_MMU_FALCON_LWDEC2_CFGA(0);
                pFlcnCfg->riscvRegisterBase = LW_FALCON2_LWDEC2_BASE;
                CHECK_STATUS_AND_RETURN_IF_NOT_OK(acrGetLwdecTargetMaskIndex_HAL(pAcr, pFlcnCfg, &pFlcnCfg->targetMaskIndex));
                bValid = LW_TRUE;
                break;

            case 2:
                pFlcnCfg->falcon2RegisterBase = LW_FALCON2_LWDEC3_BASE;
                pFlcnCfg->subWprRangeAddrBase = LW_PFB_PRI_MMU_FALCON_LWDEC3_CFGA(0);
                pFlcnCfg->riscvRegisterBase = LW_FALCON2_LWDEC3_BASE;
                CHECK_STATUS_AND_RETURN_IF_NOT_OK(acrGetLwdecTargetMaskIndex_HAL(pAcr, pFlcnCfg, &pFlcnCfg->targetMaskIndex));
                bValid = LW_TRUE;
                break;

            case 3:
                pFlcnCfg->falcon2RegisterBase = LW_FALCON2_LWDEC4_BASE;
                pFlcnCfg->subWprRangeAddrBase = LW_PFB_PRI_MMU_FALCON_LWDEC4_CFGA(0);
                pFlcnCfg->riscvRegisterBase = LW_FALCON2_LWDEC4_BASE;
                CHECK_STATUS_AND_RETURN_IF_NOT_OK(acrGetLwdecTargetMaskIndex_HAL(pAcr, pFlcnCfg, &pFlcnCfg->targetMaskIndex));
                bValid = LW_TRUE;
                break;

            case 4:
                pFlcnCfg->falcon2RegisterBase = LW_FALCON2_LWDEC5_BASE;
                pFlcnCfg->subWprRangeAddrBase = LW_PFB_PRI_MMU_FALCON_LWDEC5_CFGA(0);
                pFlcnCfg->riscvRegisterBase = LW_FALCON2_LWDEC5_BASE;
                CHECK_STATUS_AND_RETURN_IF_NOT_OK(acrGetLwdecTargetMaskIndex_HAL(pAcr, pFlcnCfg, &pFlcnCfg->targetMaskIndex));
                bValid = LW_TRUE;
                break;

            case 5:
                pFlcnCfg->falcon2RegisterBase = LW_FALCON2_LWDEC6_BASE;
                pFlcnCfg->subWprRangeAddrBase = LW_PFB_PRI_MMU_FALCON_LWDEC6_CFGA(0);
                pFlcnCfg->riscvRegisterBase = LW_FALCON2_LWDEC6_BASE;
                CHECK_STATUS_AND_RETURN_IF_NOT_OK(acrGetLwdecTargetMaskIndex_HAL(pAcr, pFlcnCfg, &pFlcnCfg->targetMaskIndex));
                bValid = LW_TRUE;
                break;

            case 6:
                pFlcnCfg->falcon2RegisterBase = LW_FALCON2_LWDEC7_BASE;
                pFlcnCfg->subWprRangeAddrBase = LW_PFB_PRI_MMU_FALCON_LWDEC7_CFGA(0);
                pFlcnCfg->riscvRegisterBase = LW_FALCON2_LWDEC7_BASE;
                CHECK_STATUS_AND_RETURN_IF_NOT_OK(acrGetLwdecTargetMaskIndex_HAL(pAcr, pFlcnCfg, &pFlcnCfg->targetMaskIndex));
                bValid = LW_TRUE;
                break;

            default:
                bValid = LW_FALSE;
            }
        }
    }

    return (bValid == LW_TRUE ? ACR_OK : ACR_ERROR_FLCN_ID_NOT_FOUND);
}
#endif // LWDEC_RISCV_PRESENT || LWDEC_RISCV_EB_PRESENT

/*!
 * @brief Return the target mask register details for LWDEC
 */
ACR_STATUS
acrGetLwjpgTargetMaskRegisterDetails_GH100
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
acrGetLwjpgFalconConfig_GH100
(
    LwU32                falconId,
    LwU32                falconInstance,
    PACR_FLCN_CONFIG     pFlcnCfg
)
{
    LwBool bValid  = LW_FALSE;

    bValid = acrValidateLwjpgId_HAL(pAcr, falconId, falconInstance);

    if (!bValid)
    {
        return ACR_ERROR_FLCN_ID_NOT_FOUND;
    }

    pFlcnCfg->falcon2RegisterBase = 0; // since LWJPG is not moved to riscv yet
    pFlcnCfg->scpP2pCtl           = 0;
    pFlcnCfg->subWprRangePlmAddr  = LW_PFB_PRI_MMU_LWJPG_PRIV_LEVEL_MASK;
    pFlcnCfg->registerBase        = DEVICE_BASE(LW_PLWJPG(falconInstance));
    pFlcnCfg->range0Addr          = 0;
    pFlcnCfg->range1Addr          = 0;
    pFlcnCfg->bOpenCarve          = LW_FALSE;
    pFlcnCfg->bFbifPresent        = LW_TRUE;
    pFlcnCfg->ctxDma              = LSF_BOOTSTRAP_CTX_DMA_VIDEO;
    pFlcnCfg->bStackCfgSupported  = LW_TRUE;
    pFlcnCfg->msrIndex            = RTS_MSR_INDEX_UCODE_LWJPG0;
    pFlcnCfg->bIsNsFalcon         = LW_TRUE;
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

LwBool
acrValidateLwjpgId_GH100
(
    LwU32 falconId,
    LwU32 falconInstance
)
{
    return LW_FALSE;
}

ACR_STATUS
acrGetLwjpgFalconConfig_GH100
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
acrGetImemSize_GH100
(
    PACR_FLCN_CONFIG pFlcnCfg,
    LwU32 *pSizeInByte
)
{
    ACR_STATUS status = ACR_OK;
    LwU32      hwcfg;
    LwU32      imemSize;

    CHECK_STATUS_AND_RETURN_IF_NOT_OK(acrFlcnRegLabelRead_HAL(pAcr, pFlcnCfg, REG_LABEL_FLCN_HWCFG, &hwcfg));
    imemSize = DRF_VAL(_PFALCON_FALCON, _HWCFG, _IMEM_SIZE, hwcfg);

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
acrGetDmemSize_GH100
(
    PACR_FLCN_CONFIG pFlcnCfg,
    LwU32 *pSizeInByte
)
{
    ACR_STATUS status = ACR_OK;
    LwU32      hwcfg;
    LwU32      dmemSize;

    CHECK_STATUS_AND_RETURN_IF_NOT_OK(acrFlcnRegLabelRead_HAL(pAcr, pFlcnCfg, REG_LABEL_FLCN_HWCFG, &hwcfg));
    dmemSize = DRF_VAL(_PFALCON_FALCON, _HWCFG, _DMEM_SIZE, hwcfg);

    if (pSizeInByte == NULL)
    {
        return ACR_ERROR_ILWALID_ARGUMENT;
    }

    *pSizeInByte = (dmemSize << FLCN_DMEM_BLK_ALIGN_BITS);

    return status;
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
acrRiscvGetPhysicalAddress_GH100
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
            CHECK_LIBLWRISCV_STATUS_AND_RETURN_IF_NOT_OK(LWRV_ERR_ILWALID_INDEX, ACR_ERROR_ILWALID_ARGUMENT);
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
acrGetFalconInstanceCount_GH100
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

    switch(falconId)
    {

    #ifdef LWDEC_RISCV_EB_PRESENT
        case LSF_FALCON_ID_LWDEC_RISCV_EB:
        {
            *pInstanceCount = LW_PLWDEC__SIZE_1 - 1;
            break;
        }
    #endif

    #ifdef LWJPG_PRESENT
        case LSF_FALCON_ID_LWJPG:
        {
            *pInstanceCount = LW_PLWJPG__SIZE_1;
            break;
        }
    #endif
        case LSF_FALCON_ID_FECS:
        {
            *pInstanceCount = LW_SCAL_LITTER_MAX_NUM_SMC_ENGINES;
            break;
        }
        default:
            *pInstanceCount = LSF_FALCON_INSTANCE_COUNT_DEFAULT_1;
    }

    return ACR_OK;
}

/*
* @brief: Return Falcon Active instance count on the current chip
*
* @param[out] *pInstanceCount              Pointer to active instance count
*
* @return      ACR_OK                      if no error
*              ACR_ERROR_ILWALID_ARGUMENT  Invalid argument passed
*/
ACR_STATUS
acrGetNumActiveGpccsInstances_GH100
(
    LwU32  *pInstanceCount
)
{
    ACR_STATUS  status             = ACR_OK;
    LwU32       fuseOffset;
    LwU32       fuseAddr;
    LwU32       fuseData;
    LwU32       instance;

    if (pInstanceCount == NULL)
    {
        return ACR_ERROR_ILWALID_ARGUMENT;
    }

    // Get fuse register offset for GPCCS engine 
    fuseOffset = LW_FUSE_OFFSET_STATUS_OPT_GPC;

    CHECK_STATUS_AND_RETURN_IF_NOT_OK(acrGetFuseRegAddress_HAL(pAcr, fuseOffset, &fuseAddr));

    // Get fuse register data
    fuseData = ACR_REG_RD32(BAR0, fuseAddr);

    *pInstanceCount = 0;

    // Count number of Active Instances
    for (instance = 0; instance < LW_SCAL_LITTER_MAX_NUM_SMC_ENGINES; instance++)
    {
        if (!(fuseData & 1))
        {
            *pInstanceCount += 1;
        }
     
        fuseData >>= 1;
    }

    return status;
}

/*!
 * @brief  Allow FSP to reset GSP at end of ACR exit.
 */
void
acrSetFspToResetGsp_GH100(void)
{
    LwU32 regval;

#ifdef ALLOW_FSP_TO_RESET_GSP
    LwU32 sourceMask = 0;
    sourceMask       = BIT(LW_PRIV_LEVEL_MASK_SOURCE_ENABLE_GSP) | BIT(LW_PRIV_LEVEL_MASK_SOURCE_ENABLE_FSP);

    regval = ACR_REG_RD32(PRGNLCL, LW_PRGNLCL_FALCON_RESET_PRIV_LEVEL_MASK);
    regval = FLD_SET_DRF_NUM( _PRGNLCL, _FALCON_RESET_PRIV_LEVEL_MASK, _SOURCE_ENABLE, sourceMask, regval);
    regval = FLD_SET_DRF( _PRGNLCL, _FALCON_RESET_PRIV_LEVEL_MASK, _WRITE_PROTECTION, _ALL_LEVELS_ENABLED, regval);
    ACR_REG_WR32(PRGNLCL, LW_PRGNLCL_FALCON_RESET_PRIV_LEVEL_MASK, regval);

    regval = ACR_REG_RD32(PRGNLCL, LW_PRGNLCL_RISCV_CPUCTL_PRIV_LEVEL_MASK);
    regval = FLD_SET_DRF_NUM( _PRGNLCL, _RISCV_CPUCTL_PRIV_LEVEL_MASK, _SOURCE_ENABLE, sourceMask, regval);
    regval = FLD_SET_DRF( _PRGNLCL, _RISCV_CPUCTL_PRIV_LEVEL_MASK, _WRITE_PROTECTION, _ALL_LEVELS_ENABLED, regval);
    ACR_REG_WR32(PRGNLCL, LW_PRGNLCL_RISCV_CPUCTL_PRIV_LEVEL_MASK, regval);

    regval = ACR_REG_RD32(PRGNLCL, LW_PRGNLCL_FALCON_EXE_PRIV_LEVEL_MASK);
    regval = FLD_SET_DRF_NUM( _PRGNLCL, _FALCON_EXE_PRIV_LEVEL_MASK, _SOURCE_ENABLE, sourceMask, regval);
    regval = FLD_SET_DRF( _PRGNLCL, _FALCON_EXE_PRIV_LEVEL_MASK, _WRITE_PROTECTION, _ALL_LEVELS_ENABLED, regval);
    ACR_REG_WR32(PRGNLCL, LW_PRGNLCL_FALCON_EXE_PRIV_LEVEL_MASK, regval);
#else
    regval = ACR_REG_RD32(PRGNLCL, LW_PRGNLCL_FALCON_RESET_PRIV_LEVEL_MASK);
    regval = FLD_SET_DRF( _PRGNLCL, _FALCON_RESET_PRIV_LEVEL_MASK, _WRITE_PROTECTION, _ALL_LEVELS_ENABLED, regval);
    regval = FLD_SET_DRF( _PRGNLCL, _FALCON_RESET_PRIV_LEVEL_MASK, _SOURCE_ENABLE, _ALL_SOURCES_ENABLED, regval);
    ACR_REG_WR32(PRGNLCL, LW_PRGNLCL_FALCON_RESET_PRIV_LEVEL_MASK, regval);

    regval = ACR_REG_RD32(PRGNLCL, LW_PRGNLCL_RISCV_CPUCTL_PRIV_LEVEL_MASK);
    regval = FLD_SET_DRF( _PRGNLCL, _RISCV_CPUCTL_PRIV_LEVEL_MASK, _WRITE_PROTECTION, _ALL_LEVELS_ENABLED, regval);
    regval = FLD_SET_DRF( _PRGNLCL, _RISCV_CPUCTL_PRIV_LEVEL_MASK, _SOURCE_ENABLE, _ALL_SOURCES_ENABLED, regval);
    ACR_REG_WR32(PRGNLCL, LW_PRGNLCL_RISCV_CPUCTL_PRIV_LEVEL_MASK, regval);

    regval = ACR_REG_RD32(PRGNLCL, LW_PRGNLCL_FALCON_EXE_PRIV_LEVEL_MASK);
    regval = FLD_SET_DRF( _PRGNLCL, _FALCON_EXE_PRIV_LEVEL_MASK, _WRITE_PROTECTION, _ALL_LEVELS_ENABLED, regval);
    regval = FLD_SET_DRF( _PRGNLCL, _FALCON_EXE_PRIV_LEVEL_MASK, _SOURCE_ENABLE, _ALL_SOURCES_ENABLED, regval);
    ACR_REG_WR32(PRGNLCL, LW_PRGNLCL_FALCON_EXE_PRIV_LEVEL_MASK, regval);

#endif // ALLOW_FSP_TO_RESET_GSP
}

/*!
 * @brief Set target engine CPUCTL PLM and source registers based on engine type
 */
ACR_STATUS
acrSetupTargetResetPLMs_GH100
(
    PACR_FLCN_CONFIG    pFlcnCfg
)
{
    LwU32 data        = 0;
    ACR_STATUS status = ACR_OK;
  
    //
    // TODO : Setting _READ_VOILATION and _WRITE_VOILATION bits for every PLM is repetitive here, Create a macro to avoid this repetitive work
    //
    if (pFlcnCfg->uprocType == ACR_TARGET_ENGINE_CORE_RISCV || pFlcnCfg->uprocType == ACR_TARGET_ENGINE_CORE_RISCV_EB)
    {
        data = ACR_REG_RD32(BAR0,  pFlcnCfg->riscvRegisterBase + LW_PRISCV_RISCV_BCR_PRIV_LEVEL_MASK);   
        data = FLD_SET_DRF(_PRISCV, _RISCV_BCR_PRIV_LEVEL_MASK, _READ_PROTECTION,      _ALL_LEVELS_ENABLED,  data);
        data = FLD_SET_DRF(_PRISCV, _RISCV_BCR_PRIV_LEVEL_MASK, _WRITE_PROTECTION,     _ONLY_LEVEL3_ENABLED, data);
        data = FLD_SET_DRF(_PRISCV, _RISCV_BCR_PRIV_LEVEL_MASK, _READ_VIOLATION,       _REPORT_ERROR,        data);
        data = FLD_SET_DRF(_PRISCV, _RISCV_BCR_PRIV_LEVEL_MASK, _WRITE_VIOLATION,      _REPORT_ERROR,        data);
        data = FLD_SET_DRF(_PRISCV, _RISCV_BCR_PRIV_LEVEL_MASK, _SOURCE_READ_CONTROL,  _LOWERED,             data);
        data = FLD_SET_DRF(_PRISCV, _RISCV_BCR_PRIV_LEVEL_MASK, _SOURCE_WRITE_CONTROL, _BLOCKED,             data);
        data = FLD_SET_DRF_NUM(_PRISCV, _RISCV_BCR_PRIV_LEVEL_MASK, _SOURCE_ENABLE,  BIT(LW_PRIV_LEVEL_MASK_SOURCE_ENABLE_GSP) | BIT(LW_PRIV_LEVEL_MASK_SOURCE_ENABLE_LOCAL_SOURCE_FALCON), data);
        ACR_REG_WR32(BAR0, pFlcnCfg->riscvRegisterBase + LW_PRISCV_RISCV_BCR_PRIV_LEVEL_MASK, data);

        //
        // For RISCV_EB, ACR starts the engine so CPUCTL can be PL3 write and source isolated to GSP
        // For RISCV engines, CPUCTL is supposed to be updated by the ucode
        // 
        if (pFlcnCfg->uprocType == ACR_TARGET_ENGINE_CORE_RISCV_EB)
        {
            // Set RISCV CPUCTL register PLM and source
            data = ACR_REG_RD32(BAR0,  pFlcnCfg->riscvRegisterBase + LW_PRISCV_RISCV_CPUCTL_PRIV_LEVEL_MASK);
            data = FLD_SET_DRF(_PRISCV, _RISCV_CPUCTL_PRIV_LEVEL_MASK, _READ_PROTECTION,      _ALL_LEVELS_ENABLED,  data);
            data = FLD_SET_DRF(_PRISCV, _RISCV_CPUCTL_PRIV_LEVEL_MASK, _WRITE_PROTECTION,     _ONLY_LEVEL3_ENABLED, data); 
            data = FLD_SET_DRF(_PRISCV, _RISCV_CPUCTL_PRIV_LEVEL_MASK, _READ_VIOLATION,       _REPORT_ERROR,        data);
            data = FLD_SET_DRF(_PRISCV, _RISCV_CPUCTL_PRIV_LEVEL_MASK, _WRITE_VIOLATION,      _REPORT_ERROR,        data);
            data = FLD_SET_DRF(_PRISCV, _RISCV_CPUCTL_PRIV_LEVEL_MASK, _SOURCE_READ_CONTROL,  _LOWERED,             data);
            data = FLD_SET_DRF(_PRISCV, _RISCV_CPUCTL_PRIV_LEVEL_MASK, _SOURCE_WRITE_CONTROL, _BLOCKED,             data);
            data = FLD_SET_DRF_NUM(_PRISCV, _RISCV_CPUCTL_PRIV_LEVEL_MASK, _SOURCE_ENABLE, BIT(LW_PRIV_LEVEL_MASK_SOURCE_ENABLE_GSP), data);
            ACR_REG_WR32(BAR0, pFlcnCfg->riscvRegisterBase + LW_PRISCV_RISCV_CPUCTL_PRIV_LEVEL_MASK, data);
        }
    }
    else
    {
        //
        // Set FALCON CPUCTL register PLM and Source 
        //
        CHECK_STATUS_AND_RETURN_IF_NOT_OK(acrFlcnRegLabelRead_HAL(pAcr, pFlcnCfg, REG_LABEL_FLCN_CPUCTL_PRIV_LEVEL_MASK, &data));
        data = FLD_SET_DRF(_PRGNLCL, _FALCON_CPUCTL_PRIV_LEVEL_MASK, _READ_PROTECTION,      _ALL_LEVELS_ENABLED,  data);
        data = FLD_SET_DRF(_PRGNLCL, _FALCON_CPUCTL_PRIV_LEVEL_MASK, _WRITE_PROTECTION,     _ONLY_LEVEL3_ENABLED, data);
        data = FLD_SET_DRF(_PRGNLCL, _FALCON_CPUCTL_PRIV_LEVEL_MASK, _READ_VIOLATION,       _REPORT_ERROR,        data);
        data = FLD_SET_DRF(_PRGNLCL, _FALCON_CPUCTL_PRIV_LEVEL_MASK, _WRITE_VIOLATION,      _REPORT_ERROR,        data);
        // Lowering the read control as CPUCTL is used in the RM code
        data = FLD_SET_DRF(_PRGNLCL, _FALCON_CPUCTL_PRIV_LEVEL_MASK, _SOURCE_READ_CONTROL,  _LOWERED,             data);
        data = FLD_SET_DRF(_PRGNLCL, _FALCON_CPUCTL_PRIV_LEVEL_MASK, _SOURCE_WRITE_CONTROL, _BLOCKED,             data);
        
        if (pFlcnCfg->falconId == LSF_FALCON_ID_GPCCS || pFlcnCfg->falconId == LSF_FALCON_ID_FECS 
#ifdef LWDEC_PRESENT
            || pFlcnCfg->falconId == LSF_FALCON_ID_LWDEC
#endif // LWDEC_PRESENT
        )
        {
            data = FLD_SET_DRF_NUM(_PRGNLCL, _FALCON_CPUCTL_PRIV_LEVEL_MASK, _SOURCE_ENABLE, BIT(LW_PRIV_LEVEL_MASK_SOURCE_ENABLE_GSP) | BIT(LW_PRIV_LEVEL_MASK_SOURCE_ENABLE_LOCAL_SOURCE_FALCON), data);
        }
        else
        {
            data = FLD_SET_DRF_NUM(_PRGNLCL, _FALCON_CPUCTL_PRIV_LEVEL_MASK, _SOURCE_ENABLE, BIT(LW_PRIV_LEVEL_MASK_SOURCE_ENABLE_GSP), data);
        }
        CHECK_STATUS_AND_RETURN_IF_NOT_OK(acrFlcnRegLabelWrite_HAL(pAcr, pFlcnCfg, REG_LABEL_FLCN_CPUCTL_PRIV_LEVEL_MASK, data));
    }

    //
    // Set FALCON RESET register PLM and Source 
    //
    CHECK_STATUS_AND_RETURN_IF_NOT_OK(acrFlcnRegLabelRead_HAL(pAcr, pFlcnCfg, REG_LABEL_FLCN_RESET_PRIV_LEVEL_MASK, &data));
    data = FLD_SET_DRF(_PRGNLCL, _FALCON_RESET_PRIV_LEVEL_MASK, _READ_PROTECTION,      _ALL_LEVELS_ENABLED,  data);
    data = FLD_SET_DRF(_PRGNLCL, _FALCON_RESET_PRIV_LEVEL_MASK, _WRITE_PROTECTION,     _ONLY_LEVEL3_ENABLED, data);
    data = FLD_SET_DRF(_PRGNLCL, _FALCON_RESET_PRIV_LEVEL_MASK, _READ_VIOLATION,       _REPORT_ERROR,        data);
    data = FLD_SET_DRF(_PRGNLCL, _FALCON_RESET_PRIV_LEVEL_MASK, _WRITE_VIOLATION,      _REPORT_ERROR,        data);
    data = FLD_SET_DRF(_PRGNLCL, _FALCON_RESET_PRIV_LEVEL_MASK, _SOURCE_READ_CONTROL,  _LOWERED,             data);
    data = FLD_SET_DRF(_PRGNLCL, _FALCON_RESET_PRIV_LEVEL_MASK, _SOURCE_WRITE_CONTROL, _BLOCKED,             data);

    // Allow Local source enabled for FECS & GPCCS 
    if (pFlcnCfg->falconId == LSF_FALCON_ID_GPCCS || pFlcnCfg->falconId == LSF_FALCON_ID_FECS)
    {
        data = FLD_SET_DRF_NUM(_PRGNLCL, _FALCON_RESET_PRIV_LEVEL_MASK, _SOURCE_ENABLE, BIT(LW_PRIV_LEVEL_MASK_SOURCE_ENABLE_GSP) | BIT(LW_PRIV_LEVEL_MASK_SOURCE_ENABLE_LOCAL_SOURCE_FALCON), data);
    }
    else
    {
        data = FLD_SET_DRF_NUM(_PRGNLCL, _FALCON_RESET_PRIV_LEVEL_MASK, _SOURCE_ENABLE, BIT(LW_PRIV_LEVEL_MASK_SOURCE_ENABLE_GSP), data);
    }
    CHECK_STATUS_AND_RETURN_IF_NOT_OK(acrFlcnRegLabelWrite_HAL(pAcr, pFlcnCfg, REG_LABEL_FLCN_RESET_PRIV_LEVEL_MASK, data));
 
    //
    // Set FALCON EXE register PLM and Source for LS Falcon, NS Falcons & RISCV_EB engines    
    //
    if(pFlcnCfg->uprocType == ACR_TARGET_ENGINE_CORE_RISCV_EB || pFlcnCfg->uprocType == ACR_TARGET_ENGINE_CORE_FALCON) 
    {
        CHECK_STATUS_AND_RETURN_IF_NOT_OK(acrFlcnRegLabelRead_HAL(pAcr, pFlcnCfg, REG_LABEL_FLCN_EXE_PRIV_LEVEL_MASK, &data));
        data = FLD_SET_DRF(_PRGNLCL, _FALCON_EXE_PRIV_LEVEL_MASK, _READ_PROTECTION, _ALL_LEVELS_ENABLED, data);      
        switch (pFlcnCfg->falconId)
        {
#ifdef LWDEC1_PRESENT
            case LSF_FALCON_ID_LWDEC1:
#endif // LWDEC1_PRESENT
            case LSF_FALCON_ID_LWDEC_RISCV_EB:
            case LSF_FALCON_ID_LWJPG:
            case LSF_FALCON_ID_LWENC:
            case LSF_FALCON_ID_OFA:
            {
                data = FLD_SET_DRF(_PRGNLCL, _FALCON_EXE_PRIV_LEVEL_MASK, _WRITE_PROTECTION,     _ALL_LEVELS_ENABLED, data);
                break;
            }
            case LSF_FALCON_ID_GPCCS:
            case LSF_FALCON_ID_FECS :
#ifdef LWDEC_PRESENT
            case LSF_FALCON_ID_LWDEC:
#endif // LWDEC_PRESENT
            {
                data = FLD_SET_DRF(_PRGNLCL, _FALCON_EXE_PRIV_LEVEL_MASK, _WRITE_PROTECTION_LEVEL0, _DISABLE, data);
                data = FLD_SET_DRF(_PRGNLCL, _FALCON_EXE_PRIV_LEVEL_MASK, _WRITE_PROTECTION_LEVEL1, _DISABLE, data);
                data = FLD_SET_DRF(_PRGNLCL, _FALCON_EXE_PRIV_LEVEL_MASK, _WRITE_PROTECTION_LEVEL2, _ENABLE,  data);
                data = FLD_SET_DRF(_PRGNLCL, _FALCON_EXE_PRIV_LEVEL_MASK, _WRITE_PROTECTION_LEVEL3, _ENABLE,  data);
                break;
            }
            default:
            {
                data = FLD_SET_DRF(_PRGNLCL, _FALCON_EXE_PRIV_LEVEL_MASK, _WRITE_PROTECTION,     _ONLY_LEVEL3_ENABLED, data);
            }
        }
        data = FLD_SET_DRF(_PRGNLCL, _FALCON_EXE_PRIV_LEVEL_MASK, _READ_VIOLATION,       _REPORT_ERROR, data);
        data = FLD_SET_DRF(_PRGNLCL, _FALCON_EXE_PRIV_LEVEL_MASK, _WRITE_VIOLATION,      _REPORT_ERROR, data);
        data = FLD_SET_DRF(_PRGNLCL, _FALCON_EXE_PRIV_LEVEL_MASK, _SOURCE_READ_CONTROL,  _LOWERED,      data);
        data = FLD_SET_DRF(_PRGNLCL, _FALCON_EXE_PRIV_LEVEL_MASK, _SOURCE_WRITE_CONTROL, _BLOCKED,      data);

        if (pFlcnCfg->falconId == LSF_FALCON_ID_GPCCS)
        {
            // Allow FECS to access GPCCS registers under EXE_PRIV_LEVEL_MASK
            data = FLD_SET_DRF_NUM(_PRGNLCL, _FALCON_EXE_PRIV_LEVEL_MASK, _SOURCE_ENABLE, BIT(LW_PRIV_LEVEL_MASK_SOURCE_ENABLE_GSP) | BIT(LW_PRIV_LEVEL_MASK_SOURCE_ENABLE_FECS) | BIT(LW_PRIV_LEVEL_MASK_SOURCE_ENABLE_LOCAL_SOURCE_FALCON), data);
        }
        else
        {    
            data = FLD_SET_DRF_NUM(_PRGNLCL, _FALCON_EXE_PRIV_LEVEL_MASK, _SOURCE_ENABLE, BIT(LW_PRIV_LEVEL_MASK_SOURCE_ENABLE_GSP) | BIT(LW_PRIV_LEVEL_MASK_SOURCE_ENABLE_LOCAL_SOURCE_FALCON), data);
        }
        CHECK_STATUS_AND_RETURN_IF_NOT_OK(acrFlcnRegLabelWrite_HAL(pAcr, pFlcnCfg, REG_LABEL_FLCN_EXE_PRIV_LEVEL_MASK, data));
    }

    return status;
}
