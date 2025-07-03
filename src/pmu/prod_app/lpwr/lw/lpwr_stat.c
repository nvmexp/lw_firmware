/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2022 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file   lpwr_stat.c
 * @brief  LPWR Statistics Framework
 *
 */

/* ------------------------- System Includes -------------------------------- */
#include "pmusw.h"

/* ------------------------- Application Includes --------------------------- */
#include "pmu_objpg.h"
#include "pmu_objlpwr.h"
#include "pmu_objtimer.h"

/* ------------------------- Macros and Defines ----------------------------- */
/* ------------------------- Static Variables ------------------------------- */
/* ------------------------- Global Variables ------------------------------- */
/* ------------------------- External Definitions --------------------------- */
/* ------------------------- Prototypes ------------------------------------- */
/* ------------------------- Public Functions  ------------------------------ */

/*!
 * @brief Initialize the LPWR STAT Infrastructure
 *
 *  @return    FLCN_OK          Success
 *             FLCN_ERR_xxx     Failure
 */
FLCN_STATUS
lpwrStatInit(void)
{
    LPWR_STAT   *pLpwrStat = NULL;
    FLCN_STATUS  status    = FLCN_OK;

    // SW State initialization
    if (pLpwrStat == NULL)
    {
        pLpwrStat = lwosCallocType(OVL_INDEX_DMEM(lpwr), 1, LPWR_STAT);
        if (pLpwrStat == NULL)
        {
            PMU_BREAKPOINT();
            status = FLCN_ERR_NO_FREE_MEM;

            goto lpwrStatInit_exit;
        }

        // Update the pointer to OBJLPWR
        Lpwr.pLpwrStat = pLpwrStat;
    }

lpwrStatInit_exit:

    return status;
}

/*!
 * @brief Init the Stat Ctrl for given LowPower Feature.
 *
 * @param[in]  ctrlId Id of the LowPower Feature
 */
FLCN_STATUS
lpwrStatCtrlInit(LwU8 ctrlId)
{
    LPWR_STAT      *pLpwrStat     = LPWR_GET_STAT();
    LPWR_STAT_CTRL *pLpwrStatCtrl = LPWR_GET_STAT_CTRL(ctrlId);
    FLCN_STATUS     status    = FLCN_OK;

    if (pLpwrStatCtrl == NULL)
    {
        pLpwrStatCtrl = lwosCallocResidentType(1, LPWR_STAT_CTRL);
        if (pLpwrStatCtrl == NULL)
        {
            PMU_BREAKPOINT();
            status = FLCN_ERR_NO_FREE_MEM;

            goto lpwrStatCtrlInit_exit;
        }

        pLpwrStat->pStatCtrl[ctrlId] = pLpwrStatCtrl;
    }

lpwrStatCtrlInit_exit:

    return status;
}

/*!
 * @brief Update the Stat for given LowPower Feature.
 *
 * @param[in]  ctrlId       Id of the LowPower Feature
 * @param[in]  featureState LowPower Feature state when we are updating the statistics
 */
FLCN_STATUS
lpwrStatCtrlUpdate(LwU8 ctrlId, LwU8 featureState)
{
    LPWR_STAT_CTRL *pLpwrStatCtrl = LPWR_GET_STAT_CTRL(ctrlId);
    LwU32           elapsedTimeUs   = 0;
    FLCN_STATUS     status        = FLCN_OK;

    if (pLpwrStatCtrl == NULL)
    {
        status =  FLCN_ERR_NOT_SUPPORTED;

        goto lpwrStatCtrlUpdate_exit;
    }

    appTaskCriticalEnter();
    {
        switch (featureState)
        {
            case LPWR_STAT_ENTRY_START:
            {
                // Get the Entry Start Time Stamp
                osPTimerTimeNsLwrrentGet(&pLpwrStatCtrl->timerNs);
                break;
            }

            case LPWR_STAT_ENTRY_END:
            {
                // Get the delta time since entry started.
                elapsedTimeUs = (osPTimerTimeNsElapsedGet(&pLpwrStatCtrl->timerNs) / 1000);

                // Restart the timer for Resident Time.
                osPTimerTimeNsLwrrentGet(&pLpwrStatCtrl->timerNs);

                // Mark the feature as engaged.
                pLpwrStatCtrl->bEngaged = LW_TRUE;

                //
                // Callwlate rolling average entry latency
                //
                // For first cycle, average latency is same as current latency. For
                // successive cycles we use rolling average to callwlate average latency.
                //
                // Rolling average entry latency =
                // (Average entry latency * (Rolling average count - 1) + entry latency of current cycle) /
                // (Rolling average count)
                //

                // Update the Entry Latency
                if (pLpwrStatCtrl->entryCount == 0)
                {
                    pLpwrStatCtrl->avgEntryLatencyUs = elapsedTimeUs;
                }
                else
                {
                    pLpwrStatCtrl->avgEntryLatencyUs =
                        (pLpwrStatCtrl->avgEntryLatencyUs * (RM_PMU_PG_ROLLING_AVG_COUNT - 1) +
                         elapsedTimeUs) / RM_PMU_PG_ROLLING_AVG_COUNT;
                }

                // Update the Entry Count
                pLpwrStatCtrl->entryCount++;

                break;
            }

            case LPWR_STAT_EXIT_START:
            {
                // Get the delta time since entry started.
                elapsedTimeUs = (osPTimerTimeNsElapsedGet(&pLpwrStatCtrl->timerNs) / 1000);

                // Restart the timer for Exit Latency.
                osPTimerTimeNsLwrrentGet(&pLpwrStatCtrl->timerNs);

                // Mark the feature as not engaged.
                pLpwrStatCtrl->bEngaged = LW_FALSE;

                // Update the Resident Time
                pLpwrStatCtrl->residentTimeUs += elapsedTimeUs;

                break;
            }

            case LPWR_STAT_EXIT_END:
            {
                // Get the delta time since entry started.
                elapsedTimeUs = (osPTimerTimeNsElapsedGet(&pLpwrStatCtrl->timerNs) / 1000);

                //
                // Callwlate rolling average exit latency
                //
                // For first cycle, average latency is same as current latency. For
                // successive cycles we use rolling average to callwlate average latency.
                //
                // Rolling average entry latency =
                // (Average entry latency * (Rolling average count - 1) + entry latency of current cycle) /
                // (Rolling average count)
                //

                // Update the Exit Latency
                if (pLpwrStatCtrl->entryCount == 1)
                {
                    pLpwrStatCtrl->avgExitLatencyUs = elapsedTimeUs;
                }
                else
                {
                    pLpwrStatCtrl->avgExitLatencyUs =
                        (pLpwrStatCtrl->avgExitLatencyUs * (RM_PMU_PG_ROLLING_AVG_COUNT - 1) +
                         elapsedTimeUs) / RM_PMU_PG_ROLLING_AVG_COUNT;
                }

                break;
            }
        }
    }
    appTaskCriticalExit();

lpwrStatCtrlUpdate_exit:
    return status;
}
