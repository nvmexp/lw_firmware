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
 * @file   pmu_objei.c
 * @brief  Engine Idle Framework Code
 *
 */

/* ------------------------- System Includes -------------------------------- */
#include "pmusw.h"

/* ------------------------- Application Includes --------------------------- */
#include "pmu_objhal.h"
#include "pmu_objei.h"
#include "pmu_objlpwr.h"
#include "pmu_objpg.h"
#include "pmu_objms.h"
#include "main.h"
#include "main_init_api.h"
#include "dmemovl.h"

/* ------------------------- Application Includes --------------------------- */
/* ------------------------- Type Definitions ------------------------------- */
/* ------------------------- External Definitions --------------------------- */
/* ------------------------- Static Variables ------------------------------- */
/* ------------------------- Global Variables ------------------------------- */
#ifdef DMEM_VA_SUPPORTED
OBJEI Ei
    GCC_ATTRIB_SECTION("dmem_lpwrLp", "Ei");
#else
OBJEI Ei;
#endif

/* ------------------------- Prototypes --------------------------------------*/
static FLCN_STATUS s_eiEntry(void)
                   GCC_ATTRIB_NOINLINE();
static FLCN_STATUS s_eiExit(void)
                   GCC_ATTRIB_NOINLINE();
static FLCN_STATUS s_eiFsmEventSend(LwU8)
                   GCC_ATTRIB_NOINLINE();
static void        s_eiFsmReenable(void)
                   GCC_ATTRIB_SECTION("imem_lpwrLp", "s_eiFsmReenable");
/* ------------------------- Private Variables ------------------------------ */
/* ------------------------- Macros and Defines ----------------------------- */
/* ------------------------- Public Functions ------------------------------- */

/*!
 * Construct the EI object.
 *
 * @return  FLCN_OK     On success
 */
FLCN_STATUS
constructEi(void)
{
   // Add actions here to be performed prior to lpwr task is scheduled
   return FLCN_OK;
}

/*!
 * @brief EI initialization post init
 *
 * Initialization of EI related structures immediately after scheduling
 * the lpwr task in scheduler.
 */
FLCN_STATUS
eiPostInit(void)
{
    FLCN_STATUS     status = FLCN_OK;
    OSTASK_OVL_DESC ovlDescList[] = {
        OSTASK_OVL_DESC_BUILD(_DMEM, _ATTACH, lpwrLp)
    };

    // Attach lpwrLp DMEM overlay since it contains OBJEI object.
    OSTASK_ATTACH_OR_DETACH_OVERLAYS_ENTER(ovlDescList);
    {
        // All members are by default initialize to Zero.
        memset(&Ei, 0, sizeof(Ei));
    }
    OSTASK_ATTACH_OR_DETACH_OVERLAYS_EXIT(ovlDescList);

    return status;
}

/*!
 * @brief Initialization of EI FSM
 */
void
eiInit
(
    RM_PMU_RPC_STRUCT_LPWR_LOADIN_PG_CTRL_INIT *pParams
)
{
    OBJPGSTATE *pEiState = GET_OBJPGSTATE(RM_PMU_LPWR_CTRL_ID_EI);
    OSTASK_OVL_DESC ovlDescList[] = {
        OSTASK_OVL_DESC_BUILD(_DMEM, _ATTACH, lpwrLp)
    };

    // Initialize PG interfaces for EI
    pEiState->pgIf.lpwrEntry = s_eiEntry;
    pEiState->pgIf.lpwrExit  = s_eiExit;

    // Attach lpwrLp DMEM overlay since it contains OBJEI object
    OSTASK_ATTACH_OR_DETACH_OVERLAYS_ENTER(ovlDescList);
    {
        // Initialize EI callback parameters
        eiCallbackParamsInit();

        // Initialize the Idle Mask for EI FSM
        eiIdleMaskInit_HAL(&Ei);
    }
    OSTASK_ATTACH_OR_DETACH_OVERLAYS_EXIT(ovlDescList);
}

/*!
 * @brief Handle the EI's Client Request
 *
 * @return      FLCN_OK                     On success
 *              FLCN_ERR_ILWALID_ARGUMENT   Wrong Events
 */
FLCN_STATUS
eiProcessClientRequest
(
    DISPATCH_LPWR_LP_EI_CLIENT_REQUEST *pEiClientRequest
)
{
    FLCN_STATUS status = FLCN_ERR_QUEUE_TASK_ILWALID_EVENT_TYPE;

    switch (pEiClientRequest->eventId)
    {
        case LPWR_LP_EVENT_ID_EI_CLIENT_NOTIFICATION_ENABLE:
        {
            // Enable the client's notification
            status = eiClientNotificationEnable(pEiClientRequest->clientId,
                                                LW_TRUE);
            break;
        }

        case LPWR_LP_EVENT_ID_EI_CLIENT_NOTIFICATION_DISABLE:
        {
            // Disable the client's notification
            status = eiClientNotificationEnable(pEiClientRequest->clientId,
                                                LW_FALSE);
            break;
        }

        default:
        {
            status = FLCN_ERR_ILWALID_ARGUMENT;
            break;
        }
    }

    return status;
}

/*!
 * @brief Handle the EI FSM events - ENTRY/EXIT Events
 *
 * @return      FLCN_OK                     On success
 *              FLCN_ERR_ILWALID_ARGUMENT   Wrong Events
 */

FLCN_STATUS
eiProcessFsmEvents
(
    DISPATCH_LPWR_LP_EI_FSM_IDLE_EVENT *pEiFsmIdleEvent
)
{
    FLCN_STATUS status     = FLCN_ERR_QUEUE_TASK_ILWALID_EVENT_TYPE;
    OBJPGSTATE *pMscgState = GET_OBJPGSTATE(RM_PMU_LPWR_CTRL_ID_MS_MSCG);

    switch (pEiFsmIdleEvent->eventId)
    {
        case LPWR_LP_EVENT_ID_EI_FSM_ENTRY_DONE:
        {
            //
            // If MS-RPG is supported and enabled at current Pstate,
            // execute the pre entry steps
            //
            if ((PMUCFG_FEATURE_ENABLED(PMU_MS_RPG))                             &&
                (pgCtrlIsSubFeatureSupported(RM_PMU_LPWR_CTRL_ID_MS_MSCG,
                                            RM_PMU_LPWR_CTRL_FEATURE_ID_MS_RPG)) &&
                 (LPWR_IS_SF_MASK_SET(MS, RPG,
                                      pMscgState->enabledMaskClient[LPWR_CTRL_SFM_CLIENT_ID_PERF])))
            {
                // Disable MS Group
                lpwrGrpDisallowExt(RM_PMU_LPWR_GRP_CTRL_ID_MS);

                // MS-RPG Pre Entry Steps
                status = msRpgPreEntrySeq_HAL(&Ms);
                if (status == FLCN_OK)
                {
                    Ms.pMsRpg->bRpgPreEntryStepDone = LW_TRUE;
                }

                // Allow MS Group
                lpwrGrpAllowExt(RM_PMU_LPWR_GRP_CTRL_ID_MS);
            }

            // Send the Entry notification to clients
            status = eiClientEntryNotificationSend();
            break;
        }

        case LPWR_LP_EVENT_ID_EI_FSM_EXIT_DONE:
        {
            // Send the Exit Notification to clients
            status = eiClientExitNotificationSend();

            // Reverse the Pre entry steps for MS-RPG (L2-RPG)
            if ((PMUCFG_FEATURE_ENABLED(PMU_MS_RPG)) &&
                (Ms.pMsRpg->bRpgPreEntryStepDone))
            {
                // Disable MS Group
                lpwrGrpDisallowExt(RM_PMU_LPWR_GRP_CTRL_ID_MS);

                // MS-RPG Post Exit Steps
                msRpgPostExitSeq_HAL(&Ms);

                // Clear the flag
                Ms.pMsRpg->bRpgPreEntryStepDone = LW_FALSE;

                // Allow MS Group
                lpwrGrpAllowExt(RM_PMU_LPWR_GRP_CTRL_ID_MS);
            }

            //
            // Send the command to re enable EI FSM to LPWR task
            // since EXIT_NOTIFICATION is sent to all the clients,
            // now we can safely reenable the EI FSM.
            //
            s_eiFsmReenable();

            break;
        }

        default:
        {
            status = FLCN_ERR_ILWALID_ARGUMENT;
            break;
        }
    }

    return status;
}

/* ------------------------- Private Functions ------------------------------ */

/*!
 * @brief Entery Sequence for EI FSM
 *
 * @return FLCN_OK      Success.
 * @return FLCN_ERROR   Otherwise
 */
static FLCN_STATUS
s_eiEntry(void)
{
    FLCN_STATUS status    = FLCN_ERROR;
    OBJPGSTATE *pPgState  = GET_OBJPGSTATE(RM_PMU_LPWR_CTRL_ID_EI);

    // check If the idleness is flipped for not
    if (pgCtrlIdleFlipped_HAL(&Pg, RM_PMU_LPWR_CTRL_ID_EI))
    {
        pPgState->stats.abortReason |= EI_ABORT_REASON_IDLE_FLIPPED;
        return status;
    }

    // Send the Idle Entry Event to LPWR_LP task (EI Framework)
    status = s_eiFsmEventSend(LPWR_LP_EVENT_ID_EI_FSM_ENTRY_DONE);

    return status;
}

/*!
 * @brief Exit Sequence for EI FSM
 *
 * @return FLCN_OK      Success.
 * @return FLCN_ERROR   Otherwise
 */
static FLCN_STATUS
s_eiExit(void)
{
    // Send the Idle Exit Event to LPWR_LP task (EI Framework)
    s_eiFsmEventSend(LPWR_LP_EVENT_ID_EI_FSM_EXIT_DONE);

    return FLCN_OK;
}

/*!
 * @brief Helper function to send the EI FSM Entry/Exit
 * events to EI Framework (LPWR_LP task)
 *
 * @param[in] eventId   FSM Event ID i.e ENTRY DONE or EXIT DONE
 *                      (Defined as LPWR_LP_EVENT_ID_EI_FSM_xxx_DONE)
 *
 * @return FLCN_OK      Success.
 * @return FLCN_ERROR   Otherwise
*/
static FLCN_STATUS
s_eiFsmEventSend
(
    LwU8 eventId
)
{
    FLCN_STATUS      status      = FLCN_OK;
    DISPATCH_LPWR_LP disp2LpwrLp = {{ 0 }};

    // Fill up the structure to send event to LPWR_LP Task
    disp2LpwrLp.hdr.eventType      = LPWR_LP_EVENT_EI_FSM;
    disp2LpwrLp.eiFsmEvent.eventId = eventId;

    // Send the event to LPWR_LP task
    status = lwrtosQueueIdSendBlocking(LWOS_QUEUE_ID(PMU, LPWR_LP),
                                       &disp2LpwrLp, sizeof(disp2LpwrLp));
    return status;
}

/*!
 * @brief Helper function to send the command to re enable EI FSM.
 *
 * EI FSM was self disabled during the EI FSM Exit sequence.
 *
*/
static void
s_eiFsmReenable(void)
{
    DISPATCH_LPWR disp2Lpwr = {{ 0 }};
    FLCN_STATUS   status;

    disp2Lpwr.hdr.eventType    = LPWR_EVENT_ID_PMU_EXT;
    disp2Lpwr.extEvent.ctrlId  = (LwU8)RM_PMU_LPWR_CTRL_ID_EI;
    disp2Lpwr.extEvent.eventId = (LwU8)PMU_PG_EXT_EVENT_SELF_ALLOW;

    status = lwrtosQueueIdSendBlocking(LWOS_QUEUE_ID(PMU, LPWR),
                                       &disp2Lpwr, sizeof(disp2Lpwr));
    if (status != FLCN_OK)
    {
        PMU_BREAKPOINT();
    }
}
