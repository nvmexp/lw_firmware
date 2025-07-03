/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2018-2022 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file   disp_imp.c
 * @brief  Handles IMP related code.
 */

/* ------------------------- System Includes -------------------------------- */
#include "pmusw.h"
#include "pmu/ssurface.h"

/* ------------------------- Application Includes --------------------------- */
#include "pmu_objdisp.h"
#include "pmu_disp.h"
#include "cmdmgmt/cmdmgmt_supersurface.h"
#include "g_pmurpc.h"

/* ------------------------- Type Definitions ------------------------------- */
/* ------------------------- External Definitions --------------------------- */
/* ------------------------- Macros and Defines ----------------------------- */
/* ------------------------- Static Variables ------------------------------- */
/* ------------------------- Global Variables ------------------------------- */
RM_PMU_DISP_IMP_MCLK_SWITCH_SETTINGS DispImpMclkSwitchSettings;

/* ------------------------- Prototypes ------------------------------------- */
/* ------------------------- Private Function Prototypes -------------------- */
/* ------------------------- Default Text Section --------------------------- */
/* ------------------------- Public Functions ------------------------------- */

/*!
 * @brief   Handle IMP_MCLK_SWITCH_SETTINGS RPC
 *
 * @param[in]   pParams     Pointer to RM_PMU_RPC_STRUCT_DISP_IMP_MCLK_SWITCH_SETTINGS
 */
FLCN_STATUS
pmuRpcDispImpMclkSwitchSettings
(
    RM_PMU_RPC_STRUCT_DISP_IMP_MCLK_SWITCH_SETTINGS *pParams
)
{
    LwU8 idx;
    FLCN_STATUS status = FLCN_OK;

    if (pParams->impMclkSwitchSettings.vrrBasedMclk.numVrrHeads >=
        RM_PMU_DISP_VRR_MCLK_RASTER_LOCKED_HEAD_COUNT_MAX)
    {
        status = FLCN_ERR_RPC_ILWALID_INPUT;
        goto pmuRpcDispImpMclkSwitchSettings_exit;
    }

    if (PMUCFG_FEATURE_ENABLED(PMU_SELWRITY_HARDENING))
    {
        if (DISP_IS_PRESENT() &&
            pParams->impMclkSwitchSettings.numWindows > RM_PMU_DISP_NUMBER_OF_WINDOWS_MAX)
        {
            status = FLCN_ERR_RPC_ILWALID_INPUT;
            goto pmuRpcDispImpMclkSwitchSettings_exit;
        }
    }

    if ((DispImpMclkSwitchSettings.mclkSwitchTypesSupportedMask &
         LWBIT(RM_PMU_DISP_VRR)) != 0U)
    {
        if (PMUCFG_FEATURE_ENABLED(PMU_SELWRITY_HARDENING))
        {
            for (idx = 0U;
                 idx < pParams->impMclkSwitchSettings.vrrBasedMclk.numVrrHeads; idx++)
            {
                if (pParams->impMclkSwitchSettings.vrrBasedMclk.vrrInfo[idx].vrrHeadIndex >=
                    Disp.numHeads)
                {
                    status = FLCN_ERR_RPC_ILWALID_INPUT;
                    goto pmuRpcDispImpMclkSwitchSettings_exit;
                }
            }
        }

        if (pParams->impMclkSwitchSettings.vrrBasedMclk.bUpdateVrrOnly)
        {
            DispImpMclkSwitchSettings.vrrBasedMclk =
                pParams->impMclkSwitchSettings.vrrBasedMclk;
        }
        else
        {
            // Initialize/copy all the IMP settings sent from RM to PMU here.
            DispImpMclkSwitchSettings = pParams->impMclkSwitchSettings;
        }
    }
    else
    {
        // Initialize/copy all the IMP settings sent from RM to PMU here.
        DispImpMclkSwitchSettings = pParams->impMclkSwitchSettings;
    }

pmuRpcDispImpMclkSwitchSettings_exit:
    return status;
}

/*!
 * @brief   Handle PRE_MODESET RPC
 *
 * This function is called prior to a modeset.
 *
 * @param[in]   pParams     Pointer to RM_PMU_RPC_STRUCT_DISP_PRE_MODESET
 */
FLCN_STATUS
pmuRpcDispPreModeset
(
    RM_PMU_RPC_STRUCT_DISP_PRE_MODESET *pParams
)
{
    OSTASK_OVL_DESC ovlDescList[] = {
        OSTASK_OVL_DESC_BUILD(_IMEM, _ATTACH, dispImp)
    };

    OSTASK_ATTACH_OR_DETACH_OVERLAYS_ENTER(ovlDescList);
    {
        // Disable MSCG.
        isohubMscgEnableDisable_HAL(&Disp,
                                    LW_FALSE,
                                    RM_PMU_DISP_MSCG_DISABLE_REASON_MODESET);
    }
    OSTASK_ATTACH_OR_DETACH_OVERLAYS_EXIT(ovlDescList);

    return FLCN_OK;
}

/*!
 * @brief   Handle POST_MODESET RPC
 * 
 * This function is called towards the end of a modeset, after all of the 
 * supervisor interrupts have completed, and after all of the ASSY->ACTIVE 
 * transitions have completed. 
 * 
 * @param[in]   pParams     Pointer to RM_PMU_RPC_STRUCT_DISP_POST_MODESET
 */
FLCN_STATUS
pmuRpcDispPostModeset
(
    RM_PMU_RPC_STRUCT_DISP_POST_MODESET *pParams
)
{
    FLCN_STATUS status;
    OSTASK_OVL_DESC ovlDescList[] = {
        OSTASK_OVL_DESC_BUILD(_IMEM, _ATTACH, dispImp)
        OSTASK_OVL_DESC_BUILD(_DMEM, _ATTACH, lpwr)
    };

    // Read the RM modeset data.
    status =
        SSURFACE_RD(&Disp.rmModesetData,
            turingAndLater.disp.dispImpWatermarks.data.rmModesetData);
    if (status != FLCN_OK)
    {
        PMU_BREAKPOINT();
        return status;
    }

    // Store VBlank disable Frame Timer to be used Post-Mclk switch
    DispContext.frameTimeVblankMscgDisableUs = Disp.rmModesetData.frameTimeActualUs;

    if (Disp.rmModesetData.bEnableMscg)
    {
        OSTASK_ATTACH_OR_DETACH_OVERLAYS_ENTER(ovlDescList);
        {
            //
            // Enable MSCG by default after modeset. DIFR can be enabled after modeset
            // by LPWR
            //
            Disp.bUseMscgWatermarks = LW_TRUE;
            //
            // RM callwlates new MSCG watermarks at the end of every modeset.  Here we
            // pick up the updated watermarks and program them into the HW registers.
            //
            status = isohubProgramMscgWatermarks_HAL(&Disp);

            // Enable MSCG.
            isohubMscgEnableDisable_HAL(&Disp,
                                        LW_TRUE,
                                        RM_PMU_DISP_MSCG_DISABLE_REASON_MODESET);
        }
        OSTASK_ATTACH_OR_DETACH_OVERLAYS_EXIT(ovlDescList);
    }

    return status;
}

/*!
 * @brief   Enable or disable MSCG in display
 *
 * @param[in]   pParams     Pointer to RM_PMU_RPC_STRUCT_DISP_SET_MSCG_STATE
 */
FLCN_STATUS
pmuRpcDispSetMscgState
(
    RM_PMU_RPC_STRUCT_DISP_SET_MSCG_STATE *pParams
)
{
    OSTASK_OVL_DESC ovlDescList[] = {
        OSTASK_OVL_DESC_BUILD(_IMEM, _ATTACH, dispImp)
    };

    OSTASK_ATTACH_OR_DETACH_OVERLAYS_ENTER(ovlDescList);
    {
        isohubMscgEnableDisable_HAL(&Disp,
                                    pParams->bEnable,
                                    pParams->mscgDisableReason);
    }
    OSTASK_ATTACH_OR_DETACH_OVERLAYS_EXIT(ovlDescList);

    return FLCN_OK;
}

