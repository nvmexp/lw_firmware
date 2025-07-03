/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2020 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file   lpwr_seq.c
 * @brief  Implements LPWR Sequencer infrastructure
 */

/* ------------------------- System Includes -------------------------------- */
#include "pmusw.h"

/* ------------------------- System Includes -------------------------------- */
/* ------------------------- Application Includes --------------------------- */
#include "main.h"
#include "pmu_objhal.h"
#include "pmu_objpg.h"
#include "pmu_objlpwr.h"
#include "dbgprintf.h"

/* ------------------------- Macros and Defines ----------------------------- */
/* ------------------------- Static Variables ------------------------------- */
/* ------------------------- Global Variables ------------------------------- */
/* ------------------------- Prototypes ------------------------------------- */
FLCN_STATUS s_lpwrSeqSramIsTransitiolwalid(LPWR_SEQ_CTRL *pSramSeqCtrl, LwU8 targetState);

/* ------------------------- Public Functions ------------------------------- */

/*!
 * @brief Initialize the LPWR SEQ State
 *
 *  @return    FLCN_OK          Success
 *             FLCN_ERR_xxx     Failure
 */
FLCN_STATUS
lpwrSeqInit(void)
{
    LPWR_SEQ   *pLpwrSeq = NULL;
    FLCN_STATUS status   = FLCN_OK;

    // SW State initialization
    if (pLpwrSeq == NULL)
    {
        pLpwrSeq = lwosCallocType(OVL_INDEX_DMEM(lpwr), 1, LPWR_SEQ);
        if (pLpwrSeq == NULL)
        {
            PMU_BREAKPOINT();
            status = FLCN_ERR_NO_FREE_MEM;

            goto lpwrSeqInit_exit;
        }

        Lpwr.pLpwrSeq = pLpwrSeq;
    }

    // HW State Initilization

    // SRAM Seq Initialization
    lpwrSeqSramInit_HAL(&Lpwr);

    // Logic Sequencer PLM config
    lpwrSeqPlmConfig_HAL(&Lpwr);

lpwrSeqInit_exit:

    return status;
}

/*!
 * @brief Initialize the SRAM Sequencer Ctrl
 *
 * @params[in] ctrlId  SRAM Sequencer CTRL Id
 *
 *  @return    FLCN_OK          Success
 *             FLCN_ERR_xxx     Failure
 */
FLCN_STATUS
lpwrSeqSramCtrlInit
(
    LwU8 ctrlId
)
{
    LPWR_SEQ_CTRL              *pSramSeqCtrl      = LPWR_SEQ_GET_SRAM_CTRL(ctrlId);
    RM_PMU_LPWR_SEQ_CTRL_STATS *pSramSeqCtrlStats = NULL;
    FLCN_STATUS                 status            = FLCN_OK;

    //
    // Skip the initialization if Seq Ctrl is already supported.
    // This might be a duplicate call.
    //
    if (LPWR_SEQ_IS_SRAM_CTRL_SUPPORTED(ctrlId))
    {
        PMU_BREAKPOINT();
        status = FLCN_ERR_ILWALID_STATE;

        goto lpwrSeqSramCtrlInit_exit;
    }

    //
    // Allocate space for SRAM SEQ Ctrl. Return if there is no free space.
    // Avoid the duplicate allocations.
    //
    if (pSramSeqCtrl == NULL)
    {
        pSramSeqCtrl = lwosCallocType(OVL_INDEX_DMEM(lpwr), 1, LPWR_SEQ_CTRL);
        if (pSramSeqCtrl == NULL)
        {
            PMU_BREAKPOINT();
            status = FLCN_ERR_NO_FREE_MEM;

            goto lpwrSeqSramCtrlInit_exit;
        }

        Lpwr.pLpwrSeq->pSramSeqCtrl[ctrlId] = pSramSeqCtrl;
    }

    // Set the default state of sequencer
    pSramSeqCtrl->state = LPWR_SEQ_STATE_PWR_ON;

    // Set the timeout value for the sequencer transition
    pSramSeqCtrl->timeoutUs = LPWR_SEQ_SRAM_CTRL_COMPLETION_TIMEOUT_US;

    //
    // Allocate space for SRAM Seq Ctrl stats. Return if there is no free space.
    // Avoid the duplicate allocations.
    //
    if (pSramSeqCtrlStats == NULL)
    {
        pSramSeqCtrlStats = lwosCallocResidentType(1, RM_PMU_LPWR_SEQ_CTRL_STATS);
        if (pSramSeqCtrlStats == NULL)
        {
            PMU_BREAKPOINT();
            status = FLCN_ERR_NO_FREE_MEM;

            goto lpwrSeqSramCtrlInit_exit;
        }

        pSramSeqCtrl->pStats = pSramSeqCtrlStats;
    }

    // Update support mask
    Lpwr.pLpwrSeq->sramSeqCtrlSupportMask |= BIT(ctrlId);

lpwrSeqSramCtrlInit_exit:

    return status;
}

/*!
 * @brief Set the state of SRAM Sequencer Ctrl
 *
 * @params[in] ctrlId      SRAM Sequencer CTRL Id
 * @params[in] targetState Requested SRAM Sequencer State
 * @params[in] bBlocking   Blocking or non Blocking call
 *
 *  @return    FLCN_OK          Success
 *             FLCN_ERR_xxx     Failure
 *
 */
FLCN_STATUS
lpwrSeqSramStateSet
(
    LwU8   ctrlId,
    LwU8   targetState,
    LwBool bBlocking
)
{
    LPWR_SEQ_CTRL *pSramSeqCtrl = LPWR_SEQ_GET_SRAM_CTRL(ctrlId);
    FLCN_STATUS    status       = FLCN_OK;

    // Check if we have the valid state transition for SRAM Sequencer
    status = s_lpwrSeqSramIsTransitiolwalid(pSramSeqCtrl, targetState);
    if (status != FLCN_OK)
    {
        PMU_BREAKPOINT();
        goto lpwrSeqSramStateSet_exit;
    }

    // Ask the HW to move the sequencer to valid state
    status = lpwrSeqSramStateSetHal_HAL(&Lpwr, ctrlId, targetState, bBlocking);
    if (status != FLCN_OK)
    {
        PMU_BREAKPOINT();
        goto lpwrSeqSramStateSet_exit;
    }

    // Update the current state of the sequencer Ctrl
    pSramSeqCtrl->state = targetState;

    // Update the statistics based on the profile setting
    if (pSramSeqCtrl->state == LPWR_SEQ_STATE_PWR_ON)
    {
        pSramSeqCtrl->pStats->exitCount++;
    }
    else
    {
        pSramSeqCtrl->pStats->entryCount++;
    }

lpwrSeqSramStateSet_exit:

    return status;
}

/* ------------------------- Private Functions ------------------------------ */

/*!
 * @brief Set the state of SRAM Sequencer Ctrl
 *
 * @params[in] pSramSeqCtrl SRAM Sequencer CTRL pointer
 * @params[in] targetState  Requested SRAM Sequencer State
 *
 *  @return    FLCN_OK                Valid State Transition
 *             FLCN_ERR_ILWALID_STATE Invalid State Transition
 *
 */
FLCN_STATUS
s_lpwrSeqSramIsTransitiolwalid
(
    LPWR_SEQ_CTRL *pSramSeqCtrl,
    LwU8           targetState
)
{
    FLCN_STATUS status = FLCN_OK;

    // Reterive the required SRAM Sequencer Profile
    switch (targetState)
    {
        case LPWR_SEQ_STATE_SRAM_RPG:
        case LPWR_SEQ_STATE_SRAM_RPPG:
        {
            // Make sure current state of sequencer is POWER_ON
            if (pSramSeqCtrl->state != LPWR_SEQ_STATE_PWR_ON)
            {
                PMU_BREAKPOINT();
                status = FLCN_ERR_ILWALID_STATE;
            }

            break;
        }

        case LPWR_SEQ_STATE_PWR_ON:
        {
            // Make sure current state of sequencer is not POWER_ON
            if (pSramSeqCtrl->state == LPWR_SEQ_STATE_PWR_ON)
            {
                PMU_BREAKPOINT();
                status = FLCN_ERR_ILWALID_STATE;
            }

            break;
        }

        default:
        {
            PMU_BREAKPOINT();
            status = FLCN_ERR_ILWALID_STATE;
        }
    }

    return status;
}
