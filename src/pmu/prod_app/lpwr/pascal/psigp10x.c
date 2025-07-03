/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2012-2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file   pmu_pgpsigp10x.c
 * @brief  PMU PSI related Hal functions for Pascal and Later GPU.
 *
 */

/* ------------------------- System Includes -------------------------------- */
#include "pmusw.h"

#include "dev_pmgr.h"

/* ------------------------- Application Includes --------------------------- */
#include "lib_mutex.h"
#include "pmu_objpg.h"
#include "pmu_objpmu.h"
#include "pmu_objpmgr.h"
#include "pmu_objtimer.h"

#include "dbgprintf.h"

#include "config/g_pg_private.h"
#include "config/g_psi_private.h"
#include "config/g_psi_hal.h"

/* ------------------------- Type Definitions ------------------------------- */
/* ------------------------- External Definitions --------------------------- */
/* ------------------------- Macros and Defines ----------------------------- */

/*!
 * @brief Maximum latency to flush GPIO
 */
#define GPIO_CNTL_TRIGGER_UDPATE_DONE_TIME_US               (50)

/* ------------------------- Static Variables ------------------------------- */
/* ------------------------- Global Variables ------------------------------- */
/* ------------------------- Prototypes ------------------------------------- */
static LwBool     s_psiIsLwrrentBelowThreshold_GP10X(LwU8 ctrlId, LwU32 railId, LwU32 stepMask)
                  GCC_ATTRIB_SECTION("imem_libPsi", "s_psiIsLwrrentBelowThreshold_GP10X");
static FLCN_STATUS s_psiCheckEntryCondition_GP10X(LwU8 ctrlId, LwU32* pRailMask, LwU32 stepMask)
                  GCC_ATTRIB_SECTION("imem_libPsi", "s_psiCheckEntryCondition_GP10X");
static FLCN_STATUS s_psiCheckExitCondition_GP10X(LwU8 ctrlId, LwU32* pRailMask, LwU32 stepMask)
                  GCC_ATTRIB_SECTION("imem_libPsi", "s_psiCheckExitCondition_GP10X");

/* ------------------------- Public Functions ------------------------------- */

/*!
 * @brief Engage PSI
 *
 * Engage PSI only if PSI is not already engaged by some other power feature.
 *
 * NOTE: PSI entry sequence cant be paralleled with Power feature's entry
 *       sequence. This API is not designed for that.
 *
 * @param[in]   featureId       PSI Ctrl Id
 * @param[in]   stepMask        Mask of PSI_ENTRY_EXIT_STEP_*
 *
 * @return  FLCN_OK         PSI engaged successfully OR PSI was already
 *                          engaged
 */
FLCN_STATUS
psiEngagePsi_GP10X
(
    LwU8   featureId,
    LwU32  railMask,
    LwU32  stepMask
)
{
    FLCN_STATUS status = FLCN_OK;
    LwU32       railId;

    railMask &= Psi.railSupportMask;

    // check if all the enry condition are met to trigger PSI entry
    status = s_psiCheckEntryCondition_GP10X(featureId, &railMask, stepMask);
    if (status != FLCN_OK)
    {
        return status;
    }

    // check if mask has any valid rail on which PSI can be engaged.
    if (railMask == 0)
    {
        return FLCN_OK;
    }

    //
    // Engage PSI
    // if its PSTATE based PSI, then we need to call I2C interfaces otherwise, normal GPIO
    // Interface
    //
    // Note: for Pstate PSI assumption is that all the rail will have I2C based VR Interface
    // settings
    //
    if (PMUCFG_FEATURE_ENABLED(PMU_PSI_I2C_INTERFACE) &&
        (featureId == RM_PMU_PSI_CTRL_ID_PSTATE))
    {
        if (FLCN_OK != psiI2cCommandTrigger_HAL(&Psi, featureId, railMask, LW_TRUE))
        {
            return FLCN_ERROR;
        }
    }
    else
    {
        if (FLCN_OK != psiGpioTrigger_HAL(&Psi, featureId, railMask, stepMask, LW_TRUE))
        {
            return FLCN_ERROR;
        }
    }

    // Take the ownership of PSI
    FOR_EACH_INDEX_IN_MASK(32, railId, railMask)
    {
        PSI_GET_RAIL(railId)->lwrrentOwnerId = featureId;
    }
    FOR_EACH_INDEX_IN_MASK_END;

    return FLCN_OK;
}

/*!
 * @brief Dis-engage PSI
 *
 * Disengage PSI only if PSI was engaged by requesting power feature. Wait for
 * settleTimeUs after dis-engaging PSI. The regulator needs some time to
 * complete the switch before we can proceed further.
 *
 * @param[in]   featureId       PSI Ctrl Id
 * @param[in]   stepMask        Mask of PSI_ENTRY_EXIT_STEP_*
 *
 * @return  FLCN_OK         PSI dis-engaged successfully if it was engaged by
 *                          requesting power feature engaged.
 */
FLCN_STATUS
psiDisengagePsi_GP10X
(
    LwU8   featureId,
    LwU32  railMask,
    LwU32  stepMask
)
{
    FLCN_STATUS status = FLCN_OK;
    LwU32       railId;

    railMask &= Psi.railSupportMask;

    // check if all the exit condition are met to trigger PSI exit
    status = s_psiCheckExitCondition_GP10X(featureId, &railMask, stepMask);
    if (status != FLCN_OK)
    {
        return status;
    }

    // check if mask has any valid rail on which PSI can be engaged.
    if (railMask == 0)
    {
        return FLCN_OK;
    }

    // Disengage PSI
    if (PMUCFG_FEATURE_ENABLED(PMU_PSI_I2C_INTERFACE) &&
        (featureId == RM_PMU_PSI_CTRL_ID_PSTATE))
    {
        if (FLCN_OK != psiI2cCommandTrigger_HAL(&Psi, featureId, railMask, LW_FALSE))
        {
            return FLCN_ERROR;
        }
    }
    else
    {
        if (FLCN_OK != psiGpioTrigger_HAL(&Psi, featureId, railMask, stepMask, LW_FALSE))
        {
            PMU_BREAKPOINT();
        }
    }

    // Update current PSI owner
    if (PSI_STEP_ENABLED(stepMask, RELEASE_OWNERSHIP))
    {
        FOR_EACH_INDEX_IN_MASK(32, railId, railMask)
        {
            PSI_GET_RAIL(railId)->lwrrentOwnerId = RM_PMU_PSI_CTRL_ID__ILWALID;
        }
        FOR_EACH_INDEX_IN_MASK_END;
    }
    return FLCN_OK;
}

/*!
 * @brief Check Sleep Current for Current Aware PSI
 *
 * @return  LW_TRUE     Isleep less than Icross
 *          LW_FALSE    Otherwise
 */
static LwBool
s_psiIsLwrrentBelowThreshold_GP10X
(
    LwU8   ctrlId,
    LwU32  railId,
    LwU32  stepMask
)
{
    PSI_CTRL *pPsiCtrl;
    LwU8      railDependencyMask = PSI_GET_RAIL(railId)->railDependencyMask;
    LwU8      depRailId;

    if (PSI_FLAVOUR_IS_ENABLED(LWRRENT_AWARE, railId) &&
        PSI_STEP_ENABLED(stepMask, CHECK_LWRRENT))
    {
        pPsiCtrl = PSI_GET_CTRL(ctrlId);

        //
        // Sleep current should be less than crossover current to
        // engage current aware PSI for all voltage rails.
        // Check for Logic/sram rail below.
        //
        // i.e. Isleep < Icross
        //
        if (pPsiCtrl->pRailStat[railId]->iSleepmA >=
            PSI_GET_RAIL(railId)->iCrossmA)
        {
            return LW_FALSE;
        }

        // Check the current on dependent rail as well.
        FOR_EACH_INDEX_IN_MASK(8, depRailId, railDependencyMask)
        {
            if (pPsiCtrl->pRailStat[depRailId]->iSleepmA >=
                PSI_GET_RAIL(depRailId)->iCrossmA)
            {
                return LW_FALSE;
            }
        }
        FOR_EACH_INDEX_IN_MASK_END;
    }

    return LW_TRUE;
}

/*!
 * @brief Check PSI entry conditions i.e ctrlId, current.
 * This function also updates the railMask if for any given
 * rail input parameters are not as per the requirement.
 *
 * It will clear the bit for that rail in the railMask.
 *
 * @return  FLCN_OK     If all the input parameters are correct
 *          FLCN_ERROR  Otherwise
 */
static FLCN_STATUS
s_psiCheckEntryCondition_GP10X
(
    LwU8  ctrlId,
    LwU32 *pRailMask,
    LwU32 stepMask
)
{
    PSI_RAIL *pPsiRail;
    LwU8      railId;

    // Return early if requested feature ID is invalid
    if (ctrlId == RM_PMU_PSI_CTRL_ID__ILWALID)
    {
        return FLCN_ERROR;
    }

    FOR_EACH_INDEX_IN_MASK(32, railId, *pRailMask)
    {
        pPsiRail = PSI_GET_RAIL(railId);

        //
        // Clear the mask of rail if PSI for this rail is owned by other
        // valid feature.
        // i.e.
        // LwrrentOwner != Requested Feature Owner AND
        // LwrrentOwner != invalid feature
        //
        if ((pPsiRail->lwrrentOwnerId != ctrlId)  &&
            (pPsiRail->lwrrentOwnerId != RM_PMU_PSI_CTRL_ID__ILWALID))
        {
            *pRailMask &= ~BIT(railId);
            continue;
        }

        //
        // Clear the mask of rail if Sleep Current is not less than crossover current
        // for Current Aware PSI.
        //
        if (PSI_FLAVOUR_IS_ENABLED(LWRRENT_AWARE, railId) &&
            !s_psiIsLwrrentBelowThreshold_GP10X(ctrlId, railId, stepMask))
        {
            *pRailMask &= ~BIT(railId);
            continue;

        }

        //
        // if GPC-RG is engaged, then we should not engage the
        // PSI on LWVDD Rail because Rail is already gated. So
        // remove the LWVDD rail form the mask
        //
        if (PMUCFG_FEATURE_ENABLED(PMU_PG_GR_RG)                &&
            pgIsCtrlSupported(RM_PMU_LPWR_CTRL_ID_GR_RG)   &&
            (!PG_IS_FULL_POWER(RM_PMU_LPWR_CTRL_ID_GR_RG)) &&
            (railId == RM_PMU_PSI_RAIL_ID_LWVDD))
        {
            *pRailMask &= ~BIT(railId);
        }

    }
    FOR_EACH_INDEX_IN_MASK_END;

    return FLCN_OK;
}

/*!
 * @brief Check PSI exit conditions
 * This function also updates the railMask if for any given
 * rail input parameters are not as per the requirement.
 *
 * It will clear the bit for that rail in the railMask.
 *
 * @return  FLCN_OK     If all the input parameters are correct
 *          FLCN_ERROR  Otherwise
 */
static FLCN_STATUS
s_psiCheckExitCondition_GP10X
(
    LwU8   ctrlId,
    LwU32  *pRailMask,
    LwU32  stepMask
)
{
    LwU32   railId;

    // Return early if requested feature ID is invalid
    if (ctrlId == RM_PMU_PSI_CTRL_ID__ILWALID)
    {
        return FLCN_ERROR;
    }

    FOR_EACH_INDEX_IN_MASK(32, railId, *pRailMask)
    {
        //
        // If requested ctrlId is invalid OR PSI rail is owned by some
        // other power feature than ctrlId, clear the mask of that railId.
        //
        if ((ctrlId == RM_PMU_PSI_CTRL_ID__ILWALID) ||
            (PSI_GET_RAIL(railId)->lwrrentOwnerId != ctrlId))
        {
            *pRailMask &= ~BIT(railId);
            continue;
        }
    }
    FOR_EACH_INDEX_IN_MASK_END;

    return FLCN_OK;
}
