/* _LWPMU_COPYRIGHT_BEGIN_
 *
 * Copyright 2017-2020 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWPMU_COPYRIGHT_END_
 */

/*!
 * @file   pmu_acrgv10x.c
 * @brief  GV10X Access Control Region
 */

/* ------------------------ System includes -------------------------------- */
#include "pmusw.h"

/* ------------------------ Application includes --------------------------- */
#include "acr/pmu_acr.h"
#include "acr/pmu_objacr.h"
#include "dev_gsp.h"
#include "dpu/dpuifcmn.h"
#include "config/g_acr_private.h"

#include "pmu_bar0.h"
/* ------------------------ Types definitions ------------------------------ */
/* ------------------------ External definitions --------------------------- */
/* ------------------------ Static variables ------------------------------- */
/* ------------------------ Global variables ------------------------------- */
/* ------------------------ Prototypes ------------------------------------- */
/* ------------------------ Macros ----------------------------------------- */
/* ------------------------ Local Functions  ------------------------------- */
/* ------------------------ Public Functions  ------------------------------ */

/*!
 * Check if particular falcon bootstrap is blocked from acr
 */
LwBool
acrIsFalconBootstrapBlocked_GV100(LwU32 falconId)
{
    //
    // On GV100, we want to block GSPLite bootstrap from Acrlib task
    // since it will be bootstrapped by ACR load binary to protect its
    // IMEM and DMEM from all clients except level3
    //
    if (falconId == LSF_FALCON_ID_GSPLITE)
    {
        return LW_TRUE;
    }
    return LW_FALSE;
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
acrGetFalconConfig_GV100
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
#if (PMUCFG_FEATURE_ENABLED(PMU_ACR_GSP_PRESENT))
#if (PMUCFG_FEATURE_ENABLED(PMU_ACR_DPU_PRESENT))
            // Control is not supposed to come here. DPU and GSPlite both should not be present at the same time.
            ct_assert(0); 
#endif
           //
           // Adding any new LSF id increases LSF_FALCON_ID_END, and as it is used in definition of 
           // LSF_UCODE_DESC structure, size of this structure also increases. So if older ACR profiles are not compiled 
           // then size of LSF header which RM generates and what ACR binary on older chips reads is different. 
           // So ID of DPU is reused for GSP lite as DPU does not exist on GV100 onwards chip.
           //
        case LSF_FALCON_ID_GSPLITE:
            pFlcnCfg->pmcEnableMask      = 0;
            pFlcnCfg->registerBase       = LW_PGSP_FALCON_IRQSSET;
            pFlcnCfg->fbifBase           = LW_PGSP_FBIF_TRANSCFG(0);
            pFlcnCfg->range0Addr         = 0;
            pFlcnCfg->range1Addr         = 0;
            pFlcnCfg->bOpenCarve         = LW_FALSE;
            pFlcnCfg->bFbifPresent       = LW_TRUE;
            pFlcnCfg->ctxDma             = RM_GSPLITE_DMAIDX_UCODE;
            pFlcnCfg->bStackCfgSupported = LW_TRUE;
            pFlcnCfg->regSelwreResetAddr = LW_PGSP_FALCON_ENGINE;
            break;
#endif // ACR_GSP_PRESENT
        default:
            status = acrGetFalconConfig_GP10X(falconId, falconInstance, pFlcnCfg);
    }

    return status;
}

