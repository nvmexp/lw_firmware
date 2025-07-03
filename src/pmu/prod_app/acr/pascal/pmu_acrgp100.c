/* _LWPMU_COPYRIGHT_BEGIN_
 *
 * Copyright 2017 - 2019 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWPMU_COPYRIGHT_END_
 */

/*!
 * @file   pmu_acrgp100.c
 */

/* ------------------------ System includes -------------------------------- */
#include "pmusw.h"

/* ------------------------ Application includes --------------------------- */
#include "acr/pmu_acr.h"
#include "acr/pmu_acrutils.h"
#include "acr/pmu_objacr.h"
#include "dev_falcon_v4.h"
#include "dev_master.h"
#if (PMUCFG_FEATURE_ENABLED(PMU_ACR_LWENC_PRESENT))
#include "dev_lwenc_pri_sw.h"
#endif // ACR_LWENC_PRESENT 
#include "dbgprintf.h"
#include "config/g_acr_private.h"

/*!
 * @brief: Initializes STACKCFG register
 *
 * @param[in] pFlcnCfg:     Falcon config pointer
 * @param[in] cfg           cfg value of STACKCFG
 *
 * @return void
 */
void
acrInitializeStackCfg_GP100
(
    ACR_FLCN_CONFIG *pFlcnCfg,
    LwU32            cfg
)
{
    //
    // STACKCFG register is used to capture stack overflow execption. it is
    // used in RTOS.
    // program value if STACKCFG is supported.
    //
    if (pFlcnCfg->bStackCfgSupported)
    {
        ACR_REG_WR32(BAR0, (LW_PFALCON_FALCON_STACKCFG + pFlcnCfg->registerBase), cfg);
    }
}

/*!
 * @brief Program DMA base register 
 *
 * @param[in] pFlcnCfg   Structure to hold falcon config
 * @param[in] fbBase     Base address of fb region to be copied
 */
void
acrProgramDmaBase_GP100
(
    ACR_FLCN_CONFIG    *pFlcnCfg,
    LwU64               fbBase
)
{
    acrFlcnRegWrite_HAL(&Acr, pFlcnCfg, BAR0_FLCN, LW_PFALCON_FALCON_DMATRFBASE, 
                       (LwU32)LwU64_LO32(fbBase));

    acrFlcnRegWrite_HAL(&Acr, pFlcnCfg, BAR0_FLCN, LW_PFALCON_FALCON_DMATRFBASE1, 
                       (LwU32)LwU64_HI32(fbBase));
}

//
// Compile the function below only if LWENC2 is present in ref manual.
// Haldef should already be set up correctly to use STUB when LWENC2 is absent.
// Linker would choke if haldef is not set up correctly.
//
#ifdef LW_FALCON_LWENC2_BASE
#if (PMUCFG_FEATURE_ENABLED(PMU_ACR_LWENC_PRESENT))
/*!
 * @brief Get the Lwenc2 falcon specifications such as registerBase, pmcEnableMask etc
 *
 * @param[out] pFlcnCfg   Structure to hold falcon config
 */ 
void
acrGetLwenc2Config_GP100
(
    ACR_FLCN_CONFIG     *pFlcnCfg
)
{
    pFlcnCfg->pmcEnableMask = DRF_DEF(_PMC, _ENABLE, _LWENC2, _ENABLED);
    pFlcnCfg->registerBase  = LW_FALCON_LWENC2_BASE;
    pFlcnCfg->fbifBase      = LW_PLWENC_FBIF_TRANSCFG(2,0);
    pFlcnCfg->range0Addr    = 0;
    pFlcnCfg->range1Addr    = 0;
    pFlcnCfg->bOpenCarve    = LW_FALSE;
    pFlcnCfg->bFbifPresent  = LW_TRUE;
    pFlcnCfg->ctxDma        = LSF_BOOTSTRAP_CTX_DMA_VIDEO;
}
#endif
#endif
