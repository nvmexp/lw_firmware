/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2019-2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */
/*!
 * @file: acr_register_access_ga10x.c
 */
#include "acr.h"
#include "dev_graphics_nobundle.h"
#include "dev_fb.h"
#include "dev_sec_csb.h"
#include "dev_sec_pri.h"
#include "dev_pwr_pri.h"
#include "dev_gsp.h"
#include "dev_lwdec_pri.h"
#include "dev_falcon_v4.h"
#include "dev_falcon_second_pri.h"

#include "dev_pri_ringstation_sys.h"
#include "dev_pri_ringstation_gpc.h"
#include "dev_falcon_v4.h"

#ifdef ACR_BUILD_ONLY
#include "config/g_acrlib_private.h"
#include "config/g_acr_private.h"
#else
#include "g_acrlib_private.h"
#endif

#ifdef  LWDEC1_PRESENT
#include "dev_lwdec_pri.h"
#include "dev_top.h"
#endif

#ifdef LWENC_PRESENT
#include "dev_lwenc_pri_sw.h"
#include "dev_top.h"
#endif

#ifdef OFA_PRESENT
#include "dev_ofa_pri.h"
#include "dev_top.h"
#endif

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
acrlibGetFalconConfig_GA10X
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
            // SEC should boot all GPCCS falcons using broadcast scheme in legacy address space for non SMC (GA10X and later)
            //
            pFlcnCfg->registerBase        = LW_PGRAPH_PRI_GPCS_GPCCS_FALCON_IRQSSET;
            pFlcnCfg->regCfgAddr          = LW_PGRAPH_PRI_GPCS_GPCCS_ARB_FALCON_REGIONCFG;
            pFlcnCfg->regCfgMaskAddr      = LW_PGRAPH_PRI_GPCS_GPCCS_ARB_FALCON_REGIONCFG_PRIV_LEVEL_MASK;
            pFlcnCfg->fbifBase            = 0;
            pFlcnCfg->range0Addr          = 0;
            pFlcnCfg->range1Addr          = 0;
            pFlcnCfg->bOpenCarve          = LW_FALSE;
            pFlcnCfg->bFbifPresent        = LW_FALSE;
            // RegCfgAddr and RegCfgMaskAddr are needed only for falcons with no FBIF
            pFlcnCfg->ctxDma              = LSF_BOOTSTRAP_CTX_DMA_FECS;
            pFlcnCfg->targetMaskIndex     = LW_PPRIV_GPC_PRI_MASTER_gpc_pri_hub2gpccs_pri;
            break;

        case LSF_FALCON_ID_FECS:
            pFlcnCfg->targetMaskIndex     = LW_PPRIV_SYS_PRI_MASTER_fecs2fecs_pri0;
            break;
        // For boot from hs binaries we enforce use of pmu-riscv images only
        // i.e QS and onwards prod signed only support PMU RISCV LS boot
#ifndef BOOT_FROM_HS_BUILD
        case LSF_FALCON_ID_PMU:
            pFlcnCfg->falcon2RegisterBase = LW_FALCON2_PWR_BASE;
            pFlcnCfg->scpP2pCtl           = LW_PPWR_PMU_SCP_CTL_P2PRX;
            pFlcnCfg->targetMaskIndex     = LW_PPRIV_SYS_PRI_MASTER_fecs2pwr_pri;
            break;
#endif

        case LSF_FALCON_ID_PMU_RISCV:
            pFlcnCfg->targetMaskIndex     = LW_PPRIV_SYS_PRI_MASTER_fecs2pwr_pri;
            break;

        case LSF_FALCON_ID_SEC2:
            pFlcnCfg->falcon2RegisterBase = LW_FALCON2_SEC_BASE;
            pFlcnCfg->scpP2pCtl           = LW_PSEC_SCP_CTL_P2PRX;
            pFlcnCfg->targetMaskIndex     = LW_PPRIV_SYS_PRI_MASTER_fecs2sec_pri;
            break;

        case LSF_FALCON_ID_GSPLITE:
            pFlcnCfg->falcon2RegisterBase = LW_FALCON2_GSP_BASE;
            pFlcnCfg->scpP2pCtl           = LW_PGSP_SCP_CTL_P2PRX;
            pFlcnCfg->targetMaskIndex     = LW_PPRIV_SYS_PRI_MASTER_fecs2gsp_pri;
            break;

        // QS and onwards prod signed only support GSP falcon LS boot
#ifndef BOOT_FROM_HS_BUILD
        case LSF_FALCON_ID_GSP_RISCV:
            pFlcnCfg->targetMaskIndex     = LW_PPRIV_SYS_PRI_MASTER_fecs2gsp_pri;
            break;
#endif

        case LSF_FALCON_ID_LWDEC:
            pFlcnCfg->falcon2RegisterBase = LW_FALCON2_LWDEC0_BASE;
            pFlcnCfg->scpP2pCtl           = LW_PLWDEC_SCP_CTL_P2PRX(0);
            status = acrlibGetLwdecTargetMaskIndex_HAL(pAcrLib, pFlcnCfg, &pFlcnCfg->targetMaskIndex);
            break;

        case LSF_FALCON_ID_FBFALCON:
            status = acrlibGetFbFalconTargetMaskIndex_HAL(pAcrLib, &pFlcnCfg->targetMaskIndex);
            break;

#ifdef LWENC_PRESENT
        case LSF_FALCON_ID_LWENC:
            status = acrlibGetLwencFalconConfig_HAL(pAcrlib, falconId, falconInstance, pFlcnCfg);
            break;
#endif

#ifdef LWDEC1_PRESENT
        case LSF_FALCON_ID_LWDEC1:
            status = acrlibGetLwdec1FalconConfig_HAL(pAcrlib, falconId, falconInstance, pFlcnCfg);

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
            pFlcnCfg->pmcEnableMask          = 1 << (DRF_BASE(LW_PTOP_DEVICE_INFO_RESET_FIELD_FOR_UCODE_AND_VBIOS_ONLY_OFA0)%32);
            pFlcnCfg->pmcEnableRegIdx        = (DRF_BASE(LW_PTOP_DEVICE_INFO_RESET_FIELD_FOR_UCODE_AND_VBIOS_ONLY_OFA0)/32);
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

        default:
            status = ACR_ERROR_FLCN_ID_NOT_FOUND;
            break;
    }

    if (status == ACR_OK && pFlcnCfg->registerBase == 0)
    {
        status = acrlibGetFalconConfig_GA100(falconId, falconInstance, pFlcnCfg);
    }

    return status;
}

/*!
 * @brief Wait for BAR0 to become idle, without CSB error checking.
 * CSB error checking is not required as per HW team, since there is
 * no chance for a CSB error to occur at this point.
 * See http://lwbugs/200616744/78 for more information.
 */
GCC_ATTRIB_ALWAYSINLINE()
inline static void
acrlibBar0WaitIdleNoCsbErrchk(void)
{
    LwBool bDone = LW_FALSE;

    // As this is an infinite loop, timeout should be considered on using below loop elsewhere.
    do
    {
        switch (DRF_VAL(_CSEC, _BAR0_CSR, _STATUS,
                    falc_ldxb_i((LwU32*)LW_CSEC_BAR0_CSR, 0)))
        {
            case LW_CSEC_BAR0_CSR_STATUS_IDLE:
                bDone = LW_TRUE;
                break;

            case LW_CSEC_BAR0_CSR_STATUS_BUSY:
                break;

            // unexpected error conditions
            case LW_CSEC_BAR0_CSR_STATUS_ERR:
            case LW_CSEC_BAR0_CSR_STATUS_TMOUT:
            case LW_CSEC_BAR0_CSR_STATUS_DIS:
            default:
                FAIL_ACR_WITH_ERROR(ACR_ERROR_FLCN_REG_ACCESS);
        }
    }
    while (!bDone);

}

/*!
 * @brief Acr util function to write using BAR0 in a non-blocking manner,
 * without CSB error checking.
 * CSB error checking is not required as per HW team, since there is
 * no chance for a CSB error to occur in this sequence.
 * See http://lwbugs/200616744/78 for more information.
 *
 * Note that the only usecase for this function at the time of this writing
 * is for GPCCS bootstrap using posted writes.
 *
 * @param[in]   addr    Address on which to perform the write operation
 * @param[in]   data    Data to write at the specified address
 */
void
acrlibBar0RegWriteNonBlocking_GA10X
(
    LwU32 addr,
    LwU32 data
)
{

    falc_stxb_i((LwU32*)LW_CSEC_BAR0_ADDR, 0, addr);
    falc_stxb_i((LwU32*)LW_CSEC_BAR0_DATA, 0, data);

    falc_stxb_i((LwU32*)LW_CSEC_BAR0_CSR, 0,
        DRF_DEF(_CSEC, _BAR0_CSR, _CMD ,        _POSTED_WRITE ) |
        DRF_NUM(_CSEC, _BAR0_CSR, _BYTE_ENABLE, 0xF           ) |
        DRF_DEF(_CSEC, _BAR0_CSR, _TRIG,        _TRUE         ));

    acrlibBar0WaitIdleNoCsbErrchk();
}

/*!
 * @brief Acr util function to write falcon registers in non-blocking manner
 *
 * @param[in]  pFlcnCfg  Falcon config
 * @param[in]  pIsFbif   TRUE if register is offset from FBIF base
 * @param[in]  regOff    Register offset
 */
void
acrlibFlcnRegWriteNonBlocking_GA10X
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
        FAIL_ACR_WITH_ERROR(ACR_ERROR_ILWALID_ARGUMENT);
    }
}
#ifdef NEW_WPR_BLOBS
/*
 * @brief acrlibUpdateBootvecPlmLevel3Writeable raises LW_PFALCON_FALCON_BOOTVEC_PRIV_LEVEL_MASK
 *        so that BOOTVEC is writable by only level 3 binaries and readable to all
 *
 */
ACR_STATUS
acrlibUpdateBootvecPlmLevel3Writeable_GA10X
(
  PACR_FLCN_CONFIG   pFlcnCfg
)
{
    LwU32       plmValue = 0;

    if (pFlcnCfg == NULL)
    {
        return ACR_ERROR_ILWALID_ARGUMENT;
    }
    plmValue = acrlibFlcnRegLabelRead_HAL(pAcrlib, pFlcnCfg, REG_LABEL_FLCN_BOOTVEC_PRIV_LEVEL_MASK);
    plmValue = FLD_SET_DRF(_PFALCON, _FALCON_BOOTVEC_PRIV_LEVEL_MASK, _WRITE_PROTECTION_LEVEL0,  _DISABLE, plmValue);
    plmValue = FLD_SET_DRF(_PFALCON, _FALCON_BOOTVEC_PRIV_LEVEL_MASK, _WRITE_PROTECTION_LEVEL1,  _DISABLE, plmValue);
    plmValue = FLD_SET_DRF(_PFALCON, _FALCON_BOOTVEC_PRIV_LEVEL_MASK, _WRITE_PROTECTION_LEVEL2,  _DISABLE, plmValue);
    acrlibFlcnRegLabelWrite_HAL(pAcrlib, pFlcnCfg, REG_LABEL_FLCN_BOOTVEC_PRIV_LEVEL_MASK, plmValue);
    return ACR_OK;
}

/*
 * @brief acrlibUpdateDmaPlmLevel3Writeable_GA10X raises LW_PFALCON_FALCON_DMA_PRIV_LEVEL_MASK
 *        so that DMA registers are writable by only level 3 binaries and readable to all
 *
 */
ACR_STATUS
acrlibUpdateDmaPlmLevel3Writeable_GA10X
(
  PACR_FLCN_CONFIG   pFlcnCfg
)
{
    LwU32       plmValue = 0;

    if (pFlcnCfg == NULL)
    {
        return ACR_ERROR_ILWALID_ARGUMENT;
    }
    plmValue = acrlibFlcnRegLabelRead_HAL(pAcrlib, pFlcnCfg, REG_LABEL_FLCN_DMA_PRIV_LEVEL_MASK);
    plmValue = FLD_SET_DRF(_PFALCON, _FALCON_DMA_PRIV_LEVEL_MASK, _WRITE_PROTECTION_LEVEL0,  _DISABLE, plmValue);
    plmValue = FLD_SET_DRF(_PFALCON, _FALCON_DMA_PRIV_LEVEL_MASK, _WRITE_PROTECTION_LEVEL2,  _DISABLE, plmValue);
    plmValue = FLD_SET_DRF(_PFALCON, _FALCON_DMA_PRIV_LEVEL_MASK, _WRITE_PROTECTION_LEVEL1,  _DISABLE, plmValue);
    acrlibFlcnRegLabelWrite_HAL(pAcrlib, pFlcnCfg, REG_LABEL_FLCN_DMA_PRIV_LEVEL_MASK, plmValue);
    return ACR_OK;
}
#endif

#ifdef LWENC_PRESENT
ACR_STATUS
acrlibGetLwencFalconConfig_GA10X
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
            status = ACR_OK;
        break;
    }

    return status;
}
#endif

#ifdef LWDEC1_PRESENT
ACR_STATUS
acrlibGetLwdec1FalconConfig_GA10X
(
    LwU32                falconId,
    LwU32                falconInstance,
    PACR_FLCN_CONFIG     pFlcnCfg
)
{
    ACR_STATUS status                = ACR_OK;
    pFlcnCfg->registerBase           = DRF_BASE(LW_PLWDEC(1));
    pFlcnCfg->fbifBase               = LW_PLWDEC_FBIF_TRANSCFG(1,0);
    pFlcnCfg->ctxDma                 = LSF_BOOTSTRAP_CTX_DMA_VIDEO;
    pFlcnCfg->subWprRangeAddrBase    = LW_PFB_PRI_MMU_FALCON_LWDEC1_CFGA(0);
    pFlcnCfg->subWprRangePlmAddr     = LW_PFB_PRI_MMU_LWDEC_PRIV_LEVEL_MASK;
    pFlcnCfg->maxSubWprSupportedByHw = LW_PFB_PRI_MMU_FALCON_LWDEC1_CFGA__SIZE_1;
    pFlcnCfg->pmcEnableMask          = 1 << (DRF_BASE(LW_PTOP_DEVICE_INFO_RESET_FIELD_FOR_UCODE_AND_VBIOS_ONLY_LWDEC1)%32);
    pFlcnCfg->pmcEnableRegIdx        = (DRF_BASE(LW_PTOP_DEVICE_INFO_RESET_FIELD_FOR_UCODE_AND_VBIOS_ONLY_LWDEC1)/32);
    pFlcnCfg->bFbifPresent           = LW_TRUE;
    pFlcnCfg->bStackCfgSupported     = LW_TRUE;
    pFlcnCfg->falcon2RegisterBase    = LW_FALCON2_LWDEC1_BASE;
    pFlcnCfg->scpP2pCtl              = LW_PLWDEC_SCP_CTL_P2PRX(1);
    status = acrlibGetLwdecTargetMaskIndex_HAL(pAcrlib, pFlcnCfg, &pFlcnCfg->targetMaskIndex);
    return status;
}
#endif

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
acrlibGetGpccsTargetMaskRegisterDetails_GA10X
(
    LwU32 falconInstance,
    LwU32 *pTargetMaskRegister,
    LwU32 *pTargetMaskPlmRegister,
    LwU32 *pTargetMaskIndexRegister
)
{
    if (pTargetMaskRegister == NULL || pTargetMaskPlmRegister == NULL || pTargetMaskIndexRegister == NULL)
    {
        return ACR_ERROR_ILWALID_ARGUMENT;
    }
    
    // All falcons except GPC can be locked through PPRIV_SYS, GPC is locked through PPRIV_GPC
    *pTargetMaskRegister      = LW_PPRIV_GPC_GPCS_TARGET_MASK;
    *pTargetMaskPlmRegister   = LW_PPRIV_GPC_GPCS_TARGET_MASK_PRIV_LEVEL_MASK;
    *pTargetMaskIndexRegister = LW_PPRIV_GPC_GPCS_TARGET_MASK_INDEX;

    return ACR_OK;
}
