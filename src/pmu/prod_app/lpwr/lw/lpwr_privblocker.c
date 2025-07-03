/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2019-2022 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file   lpwr_privblocker.c
 * @brief  Implements LPWR Priv Blcoker infrastructure
 */

/* ------------------------- System Includes -------------------------------- */
#include "pmusw.h"

/* ------------------------- System Includes -------------------------------- */
/* ------------------------- Application Includes --------------------------- */
#include "main.h"
#include "pmu_objhal.h"
#include "pmu_objlpwr.h"
#include "dbgprintf.h"

/* ------------------------- Macros and Defines ----------------------------- */
/* ------------------------- Static Variables ------------------------------- */
/* ------------------------- Global Variables ------------------------------- */
/* ------------------------- Prototypes ------------------------------------- */
FLCN_STATUS s_lpwrPrivBlockerIsTransitiolwalid(LwU32 blockerId,
                                                  LwU32 requestedMode,
                                                  LwU32 requestedProfile);
void        s_lpwrPrivBlockerTargetStateGet   (LPWR_PRIV_BLOCKER_CTRL *pLpwrPrivBlockerCtrl,
                                               LwU32 requestedMode,
                                               LwU32 requestedProfile,
                                               LwU32 *pTargetBlockerMode,
                                               LwU32 *pTargetProfileMask);
void        s_lpwrPrivBlockerAllowDisallowRangesInit(void)
            GCC_ATTRIB_SECTION("imem_lpwrLoadin", "s_lpwrPrivBlockerAllowDisallowRangesInit");

void        s_lpwrPrivBlockerTimeoutInit(void)
            GCC_ATTRIB_SECTION("imem_lpwrLoadin", "s_lpwrPrivBlockerTimeoutInit");

/* ------------------------- Public Functions ------------------------------- */

/*!
 * @brief Initialize the Priv Blockers
 *
 * @params[in] blockerInitMask  Bit Mask of blockerId to init
 *
 *  @return    FLCN_OK          Success
 *             FLCN_ERR_xxx     Failure
 *
 */
FLCN_STATUS
lpwrPrivBlockerInit
(
    LwU32 blockerSupportMask
)
{
    FLCN_STATUS        status           = FLCN_OK;
    LPWR_PRIV_BLOCKER *pLpwrPrivBlocker = LPWR_GET_PRIV_BLOCKER();

    // Till this point memory should be allocated for Priv blockers
    if (pLpwrPrivBlocker == NULL)
    {
        PMU_BREAKPOINT();
        status = FLCN_ERROR;

        goto lpwrPrivBlockerInit_exit;
    }

    // update the supportMask
    pLpwrPrivBlocker->supportMask = blockerSupportMask;

    // Init and Enable the Priv Blocker Interrupts
    status = lpwrPrivBlockerIntrInit_HAL(&Lpwr);
    if (status != FLCN_OK)
    {
        PMU_BREAKPOINT();
        goto lpwrPrivBlockerInit_exit;
    }

    // Init the Blocker Timeout Functionality
    s_lpwrPrivBlockerTimeoutInit();

    // Init and enable the WL/BL ranges
    s_lpwrPrivBlockerAllowDisallowRangesInit();

lpwrPrivBlockerInit_exit:

    return status;
}

/*!
 * @brief Set the wakeup reason for Priv Blocker
 *
 * @params[in] ctrlIdMask  Bit Mask of LPWR_CTRL Ids
 * @Params[in] wakeupReason
 */
void
lpwrPrivBlockerWakeupMaskSet
(
    LwU32 ctrlIdMask,
    LwU32 wakeUpReason
)
{
    LPWR_PRIV_BLOCKER *pLpwrPrivBlocker = LPWR_GET_PRIV_BLOCKER();
    LwU32              ctrlId;

    FOR_EACH_INDEX_IN_MASK(32, ctrlId, ctrlIdMask)
    {
        pLpwrPrivBlocker->wakeUpReason[ctrlId] |= wakeUpReason;
    }
    FOR_EACH_INDEX_IN_MASK_END;
}

/*!
 * @brief Clear the wakeup reason for Priv Blocker
 *
 * @params[in] ctrlIdMask  Bit Mask of LPWR_CTRL Ids
 * @Params[in] wakeupReason
 */
void
lpwrPrivBlockerWakeupMaskClear
(
    LwU32 ctrlIdMask
)
{
    LPWR_PRIV_BLOCKER *pLpwrPrivBlocker = LPWR_GET_PRIV_BLOCKER();
    LwU32              ctrlId;

    FOR_EACH_INDEX_IN_MASK(32, ctrlId, ctrlIdMask)
    {
        // Clear the wakeup Reason
        pLpwrPrivBlocker->wakeUpReason[ctrlId] = 0;
    }
    FOR_EACH_INDEX_IN_MASK_END;
}

/*!
 * @brief Generic function to update the mode of priv blocker
 *
 * @param[in]   blockerId        BlockerId for which we need to update the mode
 * @param[in]   requestedMode    New requested mode for the blocker
 *                               It can have the following values
 *                               a. LPWR_PRIV_BLOCKER_MODE_BLOCK_NONE
 *                               b. LPWR_PRIV_BLOCKER_MODE_BLOCK_EQUATION
 *                               c. LPWR_PRIV_BLOCKER_MODE_BLOCK_ALL
 *
 * @Param[in]   requestedProfile Id of the Blocker Range profile. It can have
 *                               the following values:
 *                               a. LPWR_PRIV_BLOCKER_PROFILE_GR -> GR Range Profile
 *                               b. LPWR_PRIV_BLOCKER_PROFILE_MS -> MS Range Profile
 *
 * @return      FLCN_OK          On success, Blocker mode is changed
 *              FLCN_ERROR       If Blocker mode is not changed
 */
FLCN_STATUS
lpwrPrivBlockerModeSet
(
    LwU32  blockerId,
    LwU32  requestedMode,
    LwU32  requestedProfile
)
{
    LPWR_PRIV_BLOCKER_CTRL *pLpwrPrivBlockerCtrl = NULL;
    FLCN_STATUS             status               = FLCN_OK;
    LwU32                   targetBlockerMode    = 0;
    LwU32                   targetProfileMask    = 0;

    // Check if the requested mode/profile transition are valid or not.
    status = s_lpwrPrivBlockerIsTransitiolwalid(blockerId, requestedMode,
                                                requestedProfile);
    if (status != FLCN_OK)
    {
        goto lpwrPrivBlockerModeSet_exit;
    }

    // Get the blocker control structure
    pLpwrPrivBlockerCtrl = LPWR_GET_PRIV_BLOCKER_CTRL(blockerId);

    //
    // Get the target Blocker Mode and Target Profile mask based on the
    // current Blocker state and new requested state
    //
    s_lpwrPrivBlockerTargetStateGet(pLpwrPrivBlockerCtrl, requestedMode,
                                    requestedProfile, &targetBlockerMode,
                                    &targetProfileMask);

    // Ask the Priv Blocker HW to move the blocker to new mode
    lpwrPrivBlockerModeSet_HAL(&Lpwr, blockerId, targetBlockerMode,
                               targetProfileMask);

    // Update the SW Cache with latest mode and profile mask
    pLpwrPrivBlockerCtrl->lwrrentMode        = targetBlockerMode;
    pLpwrPrivBlockerCtrl->profileEnabledMask = targetProfileMask;

lpwrPrivBlockerModeSet_exit:
    return status;
}

/*!
 * @brief Generic function to update the mode of Priv Blockers for LPWR_CTRLs
 *
 * @param[in]   ctrlId           LWRR_CTRL ID for which we need to update the
 *                               priv blocker modes
 * @param[in]   blockerMode      New requested mode for the blocker
 *                               It can have the following values
 *                               a. LPWR_PRIV_BLOCKER_MODE_BLOCK_NONE
 *                               b. LPWR_PRIV_BLOCKER_MODE_BLOCK_EQUATION
 *                               c. LPWR_PRIV_BLOCKER_MODE_BLOCK_ALL
 *
 * @Param[in]   profileId        Id of the Blocker Range profile. It can have
 *                               the following values:
 *                               a. LPWR_PRIV_BLOCKER_PROFILE_GR -> GR Range Profile
 *                               b. LPWR_PRIV_BLOCKER_PROFILE_MS -> MS Range Profile
 *
 * @return      FLCN_OK          On success, Blocker mode is changed
 *              FLCN_ERROR_xxx   If Blocker mode is not changed
 */
FLCN_STATUS
lpwrCtrlPrivBlockerModeSet
(
    LwU32 ctrlId,
    LwU32 blockerMode,
    LwU32 profileId,
    LwU32 timeOutNs
)
{
    LwU32          blockerId;
    FLCN_TIMESTAMP privBlockerPollStartTimeNs;
    FLCN_STATUS    status   = FLCN_OK;
    OBJPGSTATE    *pPgState = GET_OBJPGSTATE(ctrlId);

    // Start time for Blocker engagement
    osPTimerTimeNsLwrrentGet(&privBlockerPollStartTimeNs);

    FOR_EACH_INDEX_IN_MASK(32, blockerId, pPgState->privBlockerEnabledMask)
    {
        status = lpwrPrivBlockerModeSet(blockerId, blockerMode, profileId);
        if (status != FLCN_OK)
        {
            return status;
        }
    }
    FOR_EACH_INDEX_IN_MASK_END;

    //
    // if Timeout is non zero value, then wait for Blocker Mode to get changed
    // Time will be non zero only if blocker mode is equal to either
    // LPWR_PRIV_BLOCKER_MODE_BLOCK_ALL or LPWR_PRIV_BLOCKER_MODE_BLOCK_EQUATION.
    //
    if (timeOutNs != 0)
    {
        FOR_EACH_INDEX_IN_MASK(32, blockerId, pPgState->privBlockerEnabledMask)
        {
            status = lpwrPrivBlockerWaitForModeChangeCompletion_HAL(&Lpwr, blockerId,
                                                                    timeOutNs);
            if (status != FLCN_OK)
            {
                return status;
            }
        }
        FOR_EACH_INDEX_IN_MASK_END;

        //
        // Check if Priv Blocker Mode change took more time than allowed
        // timeout value If so, send the timeout error
        //
        // TODO-pankumar: We need to revisit this timeout logic to see if we
        // can optimize it. Lwrrently assumption is we need to give the preference
        // to Feature sepcifc timeout.
        //
        if (osPTimerTimeNsElapsedGet(&privBlockerPollStartTimeNs) >= timeOutNs)
        {
            return FLCN_ERR_TIMEOUT;
        }
    }

    return status;
}

/* ------------------------- Private Functions ------------------------------ */
/*!
 * @brief Program Allow list and Disallow list ranges for all
 * the supported priv blockers and enable them
 */
void
s_lpwrPrivBlockerAllowDisallowRangesInit(void)
{
    LPWR_PRIV_BLOCKER *pLpwrPrivBlocker = LPWR_GET_PRIV_BLOCKER();

    if ((pLpwrPrivBlocker->supportMask & BIT(RM_PMU_PRIV_BLOCKER_ID_XVE)) != 0)
    {
        // Program the disallow Ranges for XVE Blocker MS Profile
        lpwrPrivBlockerXveMsProfileDisallowRangesInit_HAL(&Lpwr);

        // Program the allow Ranges for XVE Blocker MS Profile
        lpwrPrivBlockerXveMsProfileAllowRangesInit_HAL(&Lpwr);

        // Program the disallow Ranges for XVE Blocker GR Profile
        lpwrPrivBlockerXveGrProfileDisallowRangesInit_HAL(&Lpwr);

        // Program the Allow Ranges for XVE Blocker GR Profile
        lpwrPrivBlockerXveGrProfileAllowRangesInit_HAL(&Lpwr);
    }

    // Initialize the allow and disallow ranges for all the Priv Blockers
    lpwrPrivBlockerAllowDisallowRangesInit_HAL(&Lpwr);
}

/*!
 * @brief Program the priv blocker timeout functionality
 * for all the supported priv blockers and enable them
 */
void
s_lpwrPrivBlockerTimeoutInit(void)
{
    LPWR_PRIV_BLOCKER *pLpwrPrivBlocker = LPWR_GET_PRIV_BLOCKER();

    // Init the XVE Priv Blocker Timeout
    if ((pLpwrPrivBlocker->supportMask & BIT(RM_PMU_PRIV_BLOCKER_ID_XVE)) != 0)
    {
        lpwrPrivBlockerXveTimeoutInit_HAL(&Lpwr);
    }
}

/*!
 * @brief Sanity check the mode and profile transition of the priv blocker
 *
 * @param[in]   blockerId        BlockerId for which we need to update the mode
 * @param[in]   requestedMode    New requested mode for the blocker
 *                               It can have the following values
 *                               a. LPWR_PRIV_BLOCKER_MODE_BLOCK_NONE
 *                               b. LPWR_PRIV_BLOCKER_MODE_BLOCK_EQUATION
 *                               c. LPWR_PRIV_BLOCKER_MODE_BLOCK_ALL
 *
 * @Param[in]   requestedProfile Id of the Blocker Range profile. It can have
 *                               the following values:
 *                               a. LPWR_PRIV_BLOCKER_PROFILE_GR -> GR Range Profile
 *                               b. LPWR_PRIV_BLOCKER_PROFILE_MS -> MS Range Profile
 *
 * @return      FLCN_OK          On success, Blocker mode/Profile is valid for transition
 *              FLCN_ERROR       Invalid transition
 */
FLCN_STATUS
s_lpwrPrivBlockerIsTransitiolwalid
(
    LwU32 blockerId,
    LwU32 requestedMode,
    LwU32 requestedProfile
)
{
    LPWR_PRIV_BLOCKER      *pLpwrPrivBlocker     = LPWR_GET_PRIV_BLOCKER();
    LPWR_PRIV_BLOCKER_CTRL *pLpwrPrivBlockerCtrl = LPWR_GET_PRIV_BLOCKER_CTRL(blockerId);

    // Check if the blocker is supported or not.
    if ((pLpwrPrivBlocker->supportMask & BIT(blockerId)) == 0)
    {
        PMU_BREAKPOINT();
        return FLCN_ERROR;
    }

    // Check if this is a duplicate request
    if ((pLpwrPrivBlockerCtrl->lwrrentMode == requestedMode) &&
        ((pLpwrPrivBlockerCtrl->profileEnabledMask & BIT(requestedProfile)) != 0))
    {
        PMU_BREAKPOINT();
        return FLCN_ERROR;
    }

    // Check for Duplicate BLOCK_NONE Request for already disabled profile
    if (requestedMode == LPWR_PRIV_BLOCKER_MODE_BLOCK_NONE)
    {
        //
        // We are checking the following duplicate request e.g
        // 1. Blocker is in BLOCK_NONE Mode.
        // 2. GR profile is already disabled.
        // 3. So if we again get the request to move the blocker
        //    to BLOCKE_NONE Mode for GR Profile then its a duplicate
        //    blocker disengage request.
        //    Such request we will siliently drop.
        //
        if ((pLpwrPrivBlockerCtrl->lwrrentMode == requestedMode) &&
            ((pLpwrPrivBlockerCtrl->profileEnabledMask & BIT(requestedProfile)) == 0))
        {
            return FLCN_ERROR;
        }
    }

    //
    // TODO-pankumar: Add more sanity checking in this function
    // All the transition are listed here:
    // https://confluence.lwpu.com/display/LS/Priv+Blockers%3A+Unified+Design#PrivBlockers:UnifiedDesign-ValidBlockerMode/RangeTransitionTable:
    //
    return FLCN_OK;
}

/*!
 * @brief Function to compute the target Mode and Profile Mask for the blocker
 *
 * @param[in]   pLpwrPrivBlockerCtrl Priv Blocker Ctrl Structure pointer
 * @param[in]   requestedMode        New requested mode for the blocker
 *                                   It can have the following values
 *                                   a. LPWR_PRIV_BLOCKER_MODE_BLOCK_NONE
 *                                   b. LPWR_PRIV_BLOCKER_MODE_BLOCK_EQUATION
 *                                   c. LPWR_PRIV_BLOCKER_MODE_BLOCK_ALL
 *
 * @Param[in]   requestedProfile     Id of the Blocker Range profile. It can have
 *                                   the following values:
 *                                   a. LPWR_PRIV_BLOCKER_PROFILE_GR -> GR Range Profile
 *                                   b. LPWR_PRIV_BLOCKER_PROFILE_MS -> MS Range Profile
 *
 * @param[in/out]   targetBlockerMode TargetMode for the Blocker after checking
 *                                    the current state
 *
 * @Param[in/out]   targetProfileMask Target Profile mask for the blocker after
 *                                    checking current state
 */
void
s_lpwrPrivBlockerTargetStateGet
(
    LPWR_PRIV_BLOCKER_CTRL *pLpwrPrivBlockerCtrl,
    LwU32                   requestedMode,
    LwU32                   requestedProfile,
    LwU32                  *pTargetBlockerMode,
    LwU32                  *pTargetProfileMask
)
{
    // BLOCK_NONE Mode
    if (requestedMode == LPWR_PRIV_BLOCKER_MODE_BLOCK_NONE)
    {
        //
        // Remove the profile bit from current profile enabled Mask
        // We are not checking if the profile was already enabled or not
        // Ideally we should never get such requests
        //
        *pTargetProfileMask = pLpwrPrivBlockerCtrl->profileEnabledMask & ~BIT(requestedProfile);

        //
        // If still any profile is remaining, move the blocker
        // to BLOCK_EQUATION mode. We can not move the blocker
        // to BLOCK_NONE mode because at least one profile is
        // still enabled.
        //
        if (*pTargetProfileMask != 0)
        {
            *pTargetBlockerMode = LPWR_PRIV_BLOCKER_MODE_BLOCK_EQUATION;
        }
        else
        {
            *pTargetBlockerMode = LPWR_PRIV_BLOCKER_MODE_BLOCK_NONE;
        }
    }
    else
    {
        //
        // Rest of the blocker Modes i.e requested mode can be
        // 1. BLOCK_EQUATION or
        // 2. BLOCK_ALL
        //

        // Set the Target Profile Enabled Mask
        *pTargetProfileMask = pLpwrPrivBlockerCtrl->profileEnabledMask | BIT(requestedProfile);

        // Set the Target Blocker Mode
        *pTargetBlockerMode = requestedMode;

    }
}
