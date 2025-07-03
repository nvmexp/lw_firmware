/* _LWPMU_COPYRIGHT_BEGIN_
 *
 * Copyright 2015-2022 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWPMU_COPYRIGHT_END_
 */

/*!
 * @file   pmu_acrgm20xgp10x_only.c
 * @brief  GM20X and GP10X only Access Control Region
 */

/* ------------------------ System includes -------------------------------- */
#include "pmusw.h"

/* ------------------------ Application includes --------------------------- */
#include "dev_pwr_csb.h"
#include "dev_pwr_pri.h"
#include "dev_master.h"
#if (PMUCFG_FEATURE_ENABLED(PMU_ACR_SEC2_PRESENT))
#include "dev_sec_pri.h"
#endif
#include "sec2/sec2ifcmn.h"
#include "dev_graphics_nobundle.h"
#include "dev_graphics_nobundle_addendum.h"
#include "dev_ltc.h"
#include "pmu_bar0.h"
#if ((PMUCFG_FEATURE_ENABLED(PMU_ACR_TEGRA_PROFILE)))
#include "acr.h"
#else
#include "acr/pmu_acr.h"
#include "acr/pmu_acrutils.h"
#endif
#include "dev_falcon_v4.h"
#if (PMUCFG_FEATURE_ENABLED(PMU_ACR_DPU_PRESENT))
#include "dev_disp_falcon.h"
#endif
#include "dpu/dpuifcmn.h"
#if (PMUCFG_FEATURE_ENABLED(PMU_ACR_LWENC_PRESENT))
#include "dev_lwenc_pri_sw.h"
#endif
#include "config/g_acr_private.h"

/* ------------------------ Types definitions ------------------------------ */
/* ------------------------ External definitions --------------------------- */
/* ------------------------ Static variables ------------------------------- */
/* ------------------------ Global variables ------------------------------- */
/* ------------------------ Prototypes ------------------------------------- */
/* ------------------------ Macros ----------------------------------------- */
/* ------------------------ Local Functions  ------------------------------- */
/* ------------------------ Public Functions  ------------------------------ */

/*!
 * Sanity check the CBC base address provided by RM against the VPR region
 * and write the base addr to chip if ok. 
 *
 * @param [in]  cbcBase   The CBC base value to write to the CBC_BASE register.
 * @return      FLCN_OK   : If backing store base addr is successfully written.
 * @return      FLCN_ERROR : If there was a conflict between the base addr provided
 *                          by RM with the VPR region, and the addr is rejected.
 */
FLCN_STATUS
acrWriteCbcBase_GM20X
(
    LwU32 cbcBase
)
{
    static LwBool bCbcBaseWritten = LW_FALSE;

    if (bCbcBaseWritten)
    {
        // If we have done this already after PMU booted up, reject all further writes.
        return FLCN_ERROR;
    }

    REG_WR32(BAR0, LW_PLTCG_LTCS_LTSS_CBC_BASE, cbcBase);

    // Set the flag to indicate we've done this once.
    bCbcBaseWritten = LW_TRUE;

    return FLCN_OK;
}

/*!
 * Reset the CBC_BASE register back to its init value. This should be called
 * during cleanup.
 */
void
acrResetCbcBase_GM20X(void)
{
    /* The CBC base can be written if the PMU is in LS mode */
    if (REG_FLD_RD_DRF(CSB, _CPWR, _FALCON_SCTL, _LSMODE) ==
        LW_CPWR_FALCON_SCTL_LSMODE_TRUE)
    {
        REG_WR32(BAR0, LW_PLTCG_LTCS_LTSS_CBC_BASE,
                 DRF_DEF(_PLTCG, _LTCS_LTSS_CBC_BASE, _ADDRESS, _INIT));
    }
}


#if (!(PMUCFG_FEATURE_ENABLED(PMU_ACR_TEGRA_PROFILE)))
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
acrGetFalconConfig_GM20X
(
    LwU32                falconId,
    LwU32                falconInstance,
    ACR_FLCN_CONFIG     *pFlcnCfg
)
{
    ACR_STATUS  status  = ACR_OK;

    pFlcnCfg->falconId           = falconId;
    pFlcnCfg->bIsBoOwner         = LW_FALSE;
    pFlcnCfg->imemPLM            = ACR_PLMASK_READ_L0_WRITE_L2;
    pFlcnCfg->dmemPLM            = ACR_PLMASK_READ_L0_WRITE_L2;
    pFlcnCfg->bStackCfgSupported = LW_FALSE;
    pFlcnCfg->pmcEnableMask      = 0;
    pFlcnCfg->regSelwreResetAddr = 0;
    switch (falconId)
    {
        case LSF_FALCON_ID_PMU:
            pFlcnCfg->registerBase      = LW_PPWR_FALCON_IRQSSET;
            pFlcnCfg->fbifBase          = LW_PPWR_FBIF_TRANSCFG(0);
            pFlcnCfg->bFbifPresent      = LW_TRUE;
            pFlcnCfg->ctxDma            = RM_PMU_DMAIDX_UCODE;
#ifdef LW_CPWR_FALCON_DMEM_PRIV_RANGE0
            pFlcnCfg->bOpenCarve        = LW_TRUE;
#if (PMUCFG_FEATURE_ENABLED(PMU_ACR_UNLOAD))
            pFlcnCfg->range0Addr        = LW_CPWR_FALCON_DMEM_PRIV_RANGE0;
            pFlcnCfg->range1Addr        = LW_CPWR_FALCON_DMEM_PRIV_RANGE0;
            pFlcnCfg->bIsBoOwner        = LW_TRUE; // PMU is the Bootstrap Owner
#else
            pFlcnCfg->range0Addr        = LW_PPWR_FALCON_DMEM_PRIV_RANGE0;
            pFlcnCfg->range1Addr        = LW_PPWR_FALCON_DMEM_PRIV_RANGE0;
            pFlcnCfg->pmcEnableMask     = DRF_DEF(_PMC, _ENABLE, _PWR, _ENABLED);
#endif
#else
            pFlcnCfg->bOpenCarve        = LW_FALSE;
            pFlcnCfg->range0Addr        = 0;
            pFlcnCfg->range1Addr        = 0;
#endif
            pFlcnCfg->bStackCfgSupported = LW_TRUE;
            break;

#if (PMUCFG_FEATURE_ENABLED(PMU_ACR_SEC2_PRESENT))
        case LSF_FALCON_ID_SEC2:
            pFlcnCfg->registerBase      = LW_PSEC_FALCON_IRQSSET;
            pFlcnCfg->fbifBase          = LW_PSEC_FBIF_TRANSCFG(0);
            pFlcnCfg->range0Addr        = 0;
            pFlcnCfg->range1Addr        = 0;
            pFlcnCfg->bOpenCarve        = LW_FALSE;
            pFlcnCfg->bFbifPresent      = LW_TRUE;
            pFlcnCfg->ctxDma            = RM_SEC2_DMAIDX_UCODE;
            pFlcnCfg->bStackCfgSupported = LW_TRUE;
#if defined(AHESASC) || defined(BSI_LOCK)
            pFlcnCfg->bIsBoOwner        = LW_TRUE;
#endif
            pFlcnCfg->pmcEnableMask     = DRF_DEF(_PMC, _ENABLE, _SEC, _ENABLED);
            break;
#endif

#if (PMUCFG_FEATURE_ENABLED(PMU_ACR_DPU_PRESENT)) // DPU is not present on GV100+ but this hal is shared
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
            pFlcnCfg->dmemPLM           = ACR_PLMASK_READ_L2_WRITE_L2;
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

/*!
 * @brief Put the given falcon into reset state
 */
ACR_STATUS
acrPutFalconInReset_GM20X
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
    if ((pFlcnCfg->pmcEnableMask == 0) || bForceFlcnOnlyReset)
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
            data = ACR_REG_RD32(BAR0, LW_PMC_ENABLE);
            data &= ~pFlcnCfg->pmcEnableMask;
            ACR_REG_WR32(BAR0, LW_PMC_ENABLE, data);
            ACR_REG_RD32(BAR0, LW_PMC_ENABLE);
        }
    }

    return ACR_OK;
}

/*!
 * @brief Bring the given falcon out of reset state
 */
ACR_STATUS
acrBringFalconOutOfReset_GM20X
(
    ACR_FLCN_CONFIG *pFlcnCfg
)
{
    LwU32 data = 0;

    if (pFlcnCfg == NULL)
    {
        return ACR_ERROR_ILWALID_ARGUMENT;
    }

    if (pFlcnCfg->pmcEnableMask != 0)
    {
        data = ACR_REG_RD32(BAR0, LW_PMC_ENABLE);
        data |= pFlcnCfg->pmcEnableMask;
        ACR_REG_WR32(BAR0, LW_PMC_ENABLE, data);
        ACR_REG_RD32(BAR0, LW_PMC_ENABLE);
    }

    return ACR_OK;
}
#endif // !(PMUCFG_FEATURE_ENABLED(PMU_ACR_TEGRA_PROFILE))
