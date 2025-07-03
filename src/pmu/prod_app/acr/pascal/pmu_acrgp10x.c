/* _LWPMU_COPYRIGHT_BEGIN_
 *
 * Copyright 2017-2022 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWPMU_COPYRIGHT_END_
 */

/*!
 * @file   pmu_acrgp10x.c
 * @brief  GP10X Access Control Region
 */

/* ------------------------ System includes -------------------------------- */
#include "pmusw.h"

/* ------------------------ Application includes --------------------------- */
#include "acr/pmu_acr.h"
#include "acr/pmu_acrutils.h"
#include "acr/pmu_objacr.h"
#include "mmu/mmucmn.h"
#include "pmu_bar0.h"
#include "dev_fb.h"
#include "dev_fuse.h"
#include "dev_pwr_csb.h"
#include "dev_pwr_pri.h"
#include "dev_pmgr.h"
#include "dev_pri_ringmaster.h"
#include "dbgprintf.h"
#include "dev_bus.h"
#include "config/g_acr_private.h"
#if (PMUCFG_FEATURE_ENABLED(PMU_ACR_DPU_PRESENT))
#include "dev_disp_falcon.h"
#endif
#include "dpu/dpuifcmn.h"

#include "dev_sec_pri.h"
#include "dev_pmgr_addendum.h"
#include "dev_graphics_nobundle.h"
#include "dev_master.h"

#include "dev_gc6_island.h"
#include "dev_gc6_island_addendum.h"
#include "sec2/sec2ifcmn.h"

/* ------------------------ Types definitions ------------------------------ */
/* ------------------------ External definitions --------------------------- */
/* ------------------------ Static variables ------------------------------- */
/* ------------------------ Global variables ------------------------------- */
// Starting from Ampere, LW_PMC_ENABLE is getting deprecated and move to LW_PMC_DEVICE_ENABLE(i) to disable/enable Falcon device.
#ifdef LW_PMC_DEVICE_ENABLE
    #define READ_DEVICE_ENABLE_REG()  ACR_REG_RD32(BAR0, LW_PMC_DEVICE_ENABLE(pFlcnCfg->pmcEnableRegIdx))
    #define WRITE_DEVICE_ENABLE_REG() ACR_REG_WR32(BAR0, LW_PMC_DEVICE_ENABLE(pFlcnCfg->pmcEnableRegIdx), data);
#else
    #define READ_DEVICE_ENABLE_REG()  ACR_REG_RD32(BAR0, LW_PMC_ENABLE)
    #define WRITE_DEVICE_ENABLE_REG() ACR_REG_WR32(BAR0, LW_PMC_ENABLE, data);
#endif
/* ------------------------ Prototypes ------------------------------------- */
/* ------------------------ Macros ----------------------------------------- */
#define ACR_POWER_OF_2_FOR_1MB          (20)

/* ------------------------ Local Functions  ------------------------------- */
/* ------------------------ Public Functions  ------------------------------ */

/*!
 * Read the max VPR range from BRSS and return.
 *
 * @param [out] pBIsVprSupported : Whether VPR is supported on this chip.
 * @param [out] minStartAddr     : The minimum start address posible for VPR on this chip.
 * @param [out] maxEndAddr       : The maximum end   address posible for VPR on this chip.
 *
 * @return      FLCN_OK    : If the VPR range is correctly found and returned.
 * @return      FLCN_ERROR : If any error oclwrred in locating the VPR range.
 *
 */
FLCN_STATUS
acrGetMaxVprRange_GP10X
(
    LwBool  *pBIsVprSupported,
    LwU64   *pMinStartAddr,
    LwU64   *pMaxEndAddr
)
{
    LwU64 sizeInBytes;
    LwU64 one = 1;

    if ((pBIsVprSupported == NULL) || (pMinStartAddr == NULL) || (pMaxEndAddr == NULL))
    {
        return FLCN_ERR_ILWALID_ARGUMENT;
    }

    // Initialize all return values to 0 first.
    *pMinStartAddr        = 0;
    *pMaxEndAddr          = 0;
    *pBIsVprSupported     = LW_FALSE;

    // Read start address and change to byte alignment.
    *pMinStartAddr = REG_FLD_RD_DRF(BAR0, _PGC6, _BSI_VPR_SELWRE_SCRATCH_15,
                                _VPR_MAX_RANGE_START_ADDR_MB_ALIGNED);
    lw64Lsl(pMinStartAddr, pMinStartAddr, ACR_POWER_OF_2_FOR_1MB);

    // Callwlate end address based on start address and size, change to byte alignment.
    sizeInBytes = REG_FLD_RD_DRF(BAR0, _PGC6, _BSI_VPR_SELWRE_SCRATCH_13, _MAX_VPR_SIZE_MB);
    lw64Lsl(&sizeInBytes, &sizeInBytes, ACR_POWER_OF_2_FOR_1MB);

    lw64Add(pMaxEndAddr, pMinStartAddr, &sizeInBytes);
    lw64Sub(pMaxEndAddr, pMaxEndAddr, &one);

    *pBIsVprSupported = lwU64CmpGT(pMaxEndAddr, pMinStartAddr);

    return FLCN_OK;
}

/*!
 * Pre-init the ACR engine.
 */
void
acrPreInit_GP10X(void)
{
    LwU32  regVal;

    if (!acrCanReadWritePmgrSelwreScratch_HAL(&Acr))
    {
        return;
    }

    regVal = REG_RD32(BAR0, LW_PMGR_SELWRE_WR_SCRATCH_3);

    //
    // The WPR ranges may change after the PMU is unloaded, making the CBC_BASE
    // ZERO value conflict with the WPR range. Force a new check against the
    // current VPR and WPR regions before we write the ZERO value on PMU detach
    //
    regVal = FLD_SET_DRF(_PMGR, _SELWRE_WR_SCRATCH_3, _PMU_CBC_BASE_ZERO_PERMITTED,
                         _FALSE, regVal);

    // Reset this field since we haven't written the CBC_BASE register yet.
    regVal = FLD_SET_DRF(_PMGR, _SELWRE_WR_SCRATCH_3, _PMU_CBC_BASE_WRITTEN,
                         _FALSE, regVal);

    REG_WR32(BAR0, LW_PMGR_SELWRE_WR_SCRATCH_3, regVal);
}

/*!
 * Check if the PMGR secure scratch registers are accessible
 */
LwBool
acrCanReadWritePmgrSelwreScratch_GP10X(void)
{
    LwU32  privMaskVal = REG_RD32(BAR0, LW_PMGR_SELWRE_WR_SCRATCH_PRIV_LEVEL_MASK);
    LwU32  ucodeLevel  = REG_FLD_RD_DRF(CSB, _CPWR, _FALCON_SCTL, _UCODE_LEVEL);

    if (((ucodeLevel == 0) &&
         FLD_TEST_DRF(_PMGR, _SELWRE_WR_SCRATCH_PRIV_LEVEL_MASK,
                      _READ_PROTECTION_LEVEL0, _DISABLE, privMaskVal)) ||
        ((ucodeLevel == 1) &&
         FLD_TEST_DRF(_PMGR, _SELWRE_WR_SCRATCH_PRIV_LEVEL_MASK,
                      _READ_PROTECTION_LEVEL1, _DISABLE, privMaskVal)) ||
        ((ucodeLevel == 2) &&
         FLD_TEST_DRF(_PMGR, _SELWRE_WR_SCRATCH_PRIV_LEVEL_MASK,
                      _READ_PROTECTION_LEVEL2, _DISABLE, privMaskVal)))
    {
        return LW_FALSE;
    }

    if (((ucodeLevel == 0) &&
         FLD_TEST_DRF(_PMGR, _SELWRE_WR_SCRATCH_PRIV_LEVEL_MASK,
                      _WRITE_PROTECTION_LEVEL0, _DISABLE, privMaskVal)) ||
        ((ucodeLevel == 1) &&
         FLD_TEST_DRF(_PMGR, _SELWRE_WR_SCRATCH_PRIV_LEVEL_MASK,
                      _WRITE_PROTECTION_LEVEL1, _DISABLE, privMaskVal)) ||
        ((ucodeLevel == 2) &&
         FLD_TEST_DRF(_PMGR, _SELWRE_WR_SCRATCH_PRIV_LEVEL_MASK,
                      _WRITE_PROTECTION_LEVEL2, _DISABLE, privMaskVal)))
    {
        return LW_FALSE;
    }
    return LW_TRUE;
}

#if (!(PMUCFG_FEATURE_ENABLED(PMU_ACR_TEGRA_PROFILE)))
/*!
 * @brief Put the given falcon into reset state
 */
ACR_STATUS
acrPutFalconInReset_GP10X
(
    ACR_FLCN_CONFIG *pFlcnCfg,
    LwBool           bForceFlcnOnlyReset
)
{
    LwU32      data   = 0;
    ACR_STATUS status = ACR_OK;

    if (pFlcnCfg == NULL)
    {
        return ACR_ERROR_ILWALID_ARGUMENT;
    }

    //
    // Either the Falcon doesn't have a full-blown engine reset, or we have
    // been asked to do a soft reset
    //
    if (((pFlcnCfg->pmcEnableMask == 0) && (pFlcnCfg->regSelwreResetAddr == 0)) ||
        bForceFlcnOnlyReset)
    {
        status = acrFalconOnlyReset_HAL(&Acr, pFlcnCfg);
        if (status != ACR_OK)
        {
            return status;
        }
    }
    else
    {
        if (pFlcnCfg->pmcEnableMask != 0)
        {
            data = READ_DEVICE_ENABLE_REG();
            data &= ~pFlcnCfg->pmcEnableMask;
            WRITE_DEVICE_ENABLE_REG();
            READ_DEVICE_ENABLE_REG();
        }

        if (pFlcnCfg->regSelwreResetAddr != 0)
        {
            data = ACR_REG_RD32(BAR0, pFlcnCfg->regSelwreResetAddr);
            data = FLD_SET_DRF(_PPWR, _FALCON_ENGINE, _RESET, _TRUE, data);
            ACR_REG_WR32(BAR0, pFlcnCfg->regSelwreResetAddr, data);
        }
    }

    return ACR_OK;
}

/*!
 * @brief Bring the given falcon out of reset state
 */
ACR_STATUS
acrBringFalconOutOfReset_GP10X
(
    ACR_FLCN_CONFIG *pFlcnCfg
)
{
    LwU32 data = 0;

    if (pFlcnCfg == NULL)
    {
        return ACR_ERROR_ILWALID_ARGUMENT;
    }

    if (pFlcnCfg->regSelwreResetAddr != 0)
    {
        data = ACR_REG_RD32(BAR0, pFlcnCfg->regSelwreResetAddr);
        data = FLD_SET_DRF(_PPWR, _FALCON_ENGINE, _RESET, _FALSE, data);
        ACR_REG_WR32(BAR0, pFlcnCfg->regSelwreResetAddr, data);
    }

    if (pFlcnCfg->pmcEnableMask != 0)
    {
        data = READ_DEVICE_ENABLE_REG();
        data |= pFlcnCfg->pmcEnableMask;
        WRITE_DEVICE_ENABLE_REG();
        READ_DEVICE_ENABLE_REG();
    }

    return ACR_OK;
}


/*!
 * @brief Check falcons for which we use bootloader to load actual LS ucode
 *        We need to restrict such LS falcons to be loaded only from WPR, Bug 1969345
 */
LwBool
acrCheckIfFalconIsBootstrappedWithLoader_GP10X
(
    LwU32 falconId
)
{
    // From GP10X onwards, PMU, SEC2 and DPU(GSPLite for GV100) are loaded via bootloader
    if ((falconId == LSF_FALCON_ID_PMU) || (falconId == LSF_FALCON_ID_DPU) || (falconId == LSF_FALCON_ID_SEC2))
    {
        return LW_TRUE;
    }
    return LW_FALSE;
}

/*!
 * @brief Set up SEC2-specific registers
 */
void
acrSetupSec2Registers_GP10X
(
    ACR_FLCN_CONFIG *pFlcnCfg
)
{
    ACR_REG_WR32(BAR0, LW_PSEC_FBIF_CTL2_PRIV_LEVEL_MASK, ACR_PLMASK_READ_L0_WRITE_L2);
    ACR_REG_WR32(BAR0, LW_PSEC_BLOCKER_PRIV_LEVEL_MASK, ACR_PLMASK_READ_L0_WRITE_L2);
    ACR_REG_WR32(BAR0, LW_PSEC_IRQTMR_PRIV_LEVEL_MASK, ACR_PLMASK_READ_L0_WRITE_L2);
}

/*!
 * @brief Given falcon ID and instance ID, get the falcon
 *        specifications such as registerBase, pmcEnableMask etc
 *
 * @param[in]  falconId   Falcon ID
 * @param[in]  instance   Instance (either 0 or 1)
 * @param[out] pFlcnCfg   Structure to hold falcon config
 *
 * @return ACR_OK if falconId and instance are valid
 */
ACR_STATUS
acrGetFalconConfig_GP10X
(
    LwU32                falconId,
    LwU32                falconInstance,
    ACR_FLCN_CONFIG     *pFlcnCfg
)
{
    ACR_STATUS  status  = ACR_OK;

    pFlcnCfg->falconId           = falconId;
    pFlcnCfg->bIsBoOwner         = LW_FALSE;
    pFlcnCfg->imemPLM            = ACR_PLMASK_READ_L0_WRITE_L3;
    pFlcnCfg->dmemPLM            = ACR_PLMASK_READ_L3_WRITE_L3;
    pFlcnCfg->bStackCfgSupported = LW_FALSE;
    pFlcnCfg->pmcEnableMask      = 0;
    pFlcnCfg->regSelwreResetAddr = 0;
    switch (falconId)
    {
        case LSF_FALCON_ID_PMU:
            pFlcnCfg->registerBase      = LW_PPWR_FALCON_IRQSSET;
            pFlcnCfg->fbifBase          = LW_PPWR_FBIF_TRANSCFG(0);
            pFlcnCfg->bOpenCarve        = LW_TRUE;
            pFlcnCfg->bFbifPresent      = LW_TRUE;
            pFlcnCfg->ctxDma            = RM_PMU_DMAIDX_UCODE;
#if (PMUCFG_FEATURE_ENABLED(PMU_ACR_UNLOAD))
            pFlcnCfg->range0Addr        = LW_CPWR_FALCON_DMEM_PRIV_RANGE0;
            pFlcnCfg->range1Addr        = LW_CPWR_FALCON_DMEM_PRIV_RANGE1;
            pFlcnCfg->bIsBoOwner        = LW_TRUE; // PMU is the Bootstrap Owner
#else
            pFlcnCfg->range0Addr         = LW_PPWR_FALCON_DMEM_PRIV_RANGE0;
            pFlcnCfg->range1Addr         = LW_PPWR_FALCON_DMEM_PRIV_RANGE1;
            pFlcnCfg->regSelwreResetAddr = LW_PPWR_FALCON_ENGINE;
#endif
            // Allow read of PMU DMEM to level 0 for debug of PMU code
            pFlcnCfg->dmemPLM            = ACR_PLMASK_READ_L0_WRITE_L2;
            pFlcnCfg->bStackCfgSupported = LW_TRUE;
            break;

#if (PMUCFG_FEATURE_ENABLED(PMU_ACR_SEC2_PRESENT))
        case LSF_FALCON_ID_SEC2:
            pFlcnCfg->registerBase       = LW_PSEC_FALCON_IRQSSET;
            pFlcnCfg->fbifBase           = LW_PSEC_FBIF_TRANSCFG(0);
            pFlcnCfg->range0Addr         = 0;
            pFlcnCfg->range1Addr         = 0;
            pFlcnCfg->bOpenCarve         = LW_FALSE;
            pFlcnCfg->bFbifPresent       = LW_TRUE;
            pFlcnCfg->ctxDma             = RM_SEC2_DMAIDX_UCODE;
            pFlcnCfg->bStackCfgSupported = LW_TRUE;
#if defined(AHESASC) || defined(BSI_LOCK)
            pFlcnCfg->bIsBoOwner         = LW_TRUE;
#endif
            pFlcnCfg->pmcEnableMask      = DRF_DEF(_PMC, _ENABLE, _SEC, _ENABLED);
            pFlcnCfg->regSelwreResetAddr = LW_PSEC_FALCON_ENGINE;
            break;
#endif

#if (PMUCFG_FEATURE_ENABLED(PMU_ACR_DPU_PRESENT))
        case LSF_FALCON_ID_DPU:
            // ENGINE DISP shouldnt be reset
#if (PMUCFG_FEATURE_ENABLED(PMU_ACR_GSP_PRESENT))
            // Control is not supposed to come here. DPU and GSPlite both should not be present at the same time.
            ct_assert(0);
#endif
            pFlcnCfg->pmcEnableMask     = 0;
            pFlcnCfg->registerBase      = LW_FALCON_DISP_BASE;
            pFlcnCfg->fbifBase          = LW_PDISP_FBIF_TRANSCFG(0);
            pFlcnCfg->range0Addr        = LW_PDISP_FALCON_DMEM_PRIV_RANGE0;
            pFlcnCfg->range1Addr        = LW_PDISP_FALCON_DMEM_PRIV_RANGE1;
            pFlcnCfg->bOpenCarve        = LW_TRUE;
            pFlcnCfg->bFbifPresent      = LW_TRUE;
            pFlcnCfg->ctxDma            = RM_DPU_DMAIDX_UCODE;
            // Allow read/write of DPU DMEM to level 2. It is yet to be debugged why leaving this at ACR_PLMASK_READ_L3_WRITE_L3 causes failures
            pFlcnCfg->dmemPLM            = ACR_PLMASK_READ_L2_WRITE_L2;
            pFlcnCfg->bStackCfgSupported = LW_TRUE;
            break;
#endif
        case LSF_FALCON_ID_FECS:
            // ENGINE GR shouldnt be reset
            pFlcnCfg->pmcEnableMask = 0;
            pFlcnCfg->registerBase  = LW_PGRAPH_PRI_FECS_FALCON_IRQSSET;
            pFlcnCfg->fbifBase      = 0;
            pFlcnCfg->range0Addr    = 0;
            pFlcnCfg->range1Addr    = 0;
            pFlcnCfg->bOpenCarve    = LW_FALSE;
            pFlcnCfg->bFbifPresent  = LW_FALSE;
            // RegCfgAddr and RegCfgMaskAddr are needed only for falcons with no FBIF
            pFlcnCfg->regCfgAddr    = LW_PGRAPH_PRI_FECS_ARB_FALCON_REGIONCFG;
            pFlcnCfg->regCfgMaskAddr= LW_PGRAPH_PRI_FECS_ARB_FALCON_REGIONCFG_PRIV_LEVEL_MASK;
            pFlcnCfg->ctxDma        = LSF_BOOTSTRAP_CTX_DMA_FECS;
            break;
        case LSF_FALCON_ID_GPCCS:
            // ENGINE GR shouldnt be reset
            pFlcnCfg->pmcEnableMask = 0;
            pFlcnCfg->registerBase  = LW_PGRAPH_PRI_GPCS_GPCCS_FALCON_IRQSSET;
            pFlcnCfg->fbifBase      = 0;
            pFlcnCfg->range0Addr    = 0;
            pFlcnCfg->range1Addr    = 0;
            pFlcnCfg->bOpenCarve    = LW_FALSE;
            pFlcnCfg->bFbifPresent  = LW_FALSE;
            // RegCfgAddr and RegCfgMaskAddr are needed only for falcons with no FBIF
            pFlcnCfg->regCfgAddr    = LW_PGRAPH_PRI_GPCS_GPCCS_ARB_FALCON_REGIONCFG;
            pFlcnCfg->regCfgMaskAddr= LW_PGRAPH_PRI_GPCS_GPCCS_ARB_FALCON_REGIONCFG_PRIV_LEVEL_MASK;
            pFlcnCfg->ctxDma        = LSF_BOOTSTRAP_CTX_DMA_FECS;
            break;
#if (PMUCFG_FEATURE_ENABLED(PMU_ACR_LWENC_PRESENT))
        case LSF_FALCON_ID_LWENC0:
            pFlcnCfg->pmcEnableMask     = DRF_DEF(_PMC, _ENABLE, _LWENC0, _ENABLED);
            pFlcnCfg->registerBase      = LW_FALCON_LWENC0_BASE;
            pFlcnCfg->fbifBase          = LW_PLWENC_FBIF_TRANSCFG(0,0);
            pFlcnCfg->range0Addr        = 0;
            pFlcnCfg->range1Addr        = 0;
            pFlcnCfg->bOpenCarve        = LW_FALSE;
            pFlcnCfg->bFbifPresent      = LW_TRUE;
            pFlcnCfg->ctxDma            = LSF_BOOTSTRAP_CTX_DMA_VIDEO;
            break;
        case LSF_FALCON_ID_LWENC1:
            pFlcnCfg->pmcEnableMask     = DRF_DEF(_PMC, _ENABLE, _LWENC1, _ENABLED);
            pFlcnCfg->registerBase      = LW_FALCON_LWENC1_BASE;
            pFlcnCfg->fbifBase          = LW_PLWENC_FBIF_TRANSCFG(1,0);
            pFlcnCfg->range0Addr        = 0;
            pFlcnCfg->range1Addr        = 0;
            pFlcnCfg->bOpenCarve        = LW_FALSE;
            pFlcnCfg->bFbifPresent      = LW_TRUE;
            pFlcnCfg->ctxDma            = LSF_BOOTSTRAP_CTX_DMA_VIDEO;
            break;
        case LSF_FALCON_ID_LWENC2:
            acrGetLwenc2Config_HAL(&Acr, pFlcnCfg);
            break;
#endif

#if (PMUCFG_FEATURE_ENABLED(PMU_ACR_LWDEC_PRESENT))
        case LSF_FALCON_ID_LWDEC:
            acrGetLwdecConfig_HAL(&Acr, pFlcnCfg);
            break;
#endif

        default:
            status = ACR_ERROR_FLCN_ID_NOT_FOUND;
    }

    return status;
}
#endif // !(PMUCFG_FEATURE_ENABLED(PMU_ACR_TEGRA_PROFILE)
