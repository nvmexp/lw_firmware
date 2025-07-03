/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2013-2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file   gcxgc5.c
 * @brief  SSC object and infrastructure.
 */

/* ------------------------- System Includes -------------------------------- */
#include "pmusw.h"

/* ------------------------- Application Includes --------------------------- */
#include "pmu_objgcx.h"
#include "pmu_objdi.h"

#include "config/g_gcx_private.h"
#if(DIDLE_MOCK_FUNCTIONS_GENERATED)
#include "config/g_gcx_mock.c"
#endif

/* ------------------------- Application Includes --------------------------- */
/* ------------------------- Type Definitions ------------------------------- */
/* ------------------------- External Definitions --------------------------- */
/* ------------------------- Static Variables ------------------------------- */
/* ------------------------- Global Variables ------------------------------- */
/* ------------------------- Macros and Defines ----------------------------- */
/* ------------------------- Private Functions ------------------------------ */
/* ------------------------- Public Functions ------------------------------- */
/*!
 * @brief Triggers DIOS entry
 *
 * This API is used to sends notification to GCX task to engage DIOS. GCX task
 * will engage DIOS if all pre-conditions are satisfied. Lwrrently only Perfmon
 * task uses this API.
 *
 * @returns FLCN_OK
 */
void
gcxDiosAttemptToEngage(void)
{
    DISPATCH_DIDLE dispatchDios;

    //
    // Optimization: Send notification event if DIOS is ARMed and MSCG is
    // engaged. This will avoid adding un-necessary event in GCX queue.
    // We don't need to send trigger event if MSCG is not engage. PG task
    // will send MSCG_ENGAGED event that will trigger DIOS entry.
    //
    if ((DidleLwrState == DIDLE_STATE_ARMED_OS)      &&
        (PG_IS_ENGAGED(RM_PMU_LPWR_CTRL_ID_MS_MSCG)) &&
        (Gcx.pGc4 != NULL)                      &&
        (Gcx.pGc4->bInDeepL1))
    {
        dispatchDios.hdr.eventType     = DIDLE_SIGNAL_EVTID;
        dispatchDios.sigGen.signalType = DIDLE_SIGNAL_DIOS_ATTEMPT_TO_ENGAGE;
        lwrtosQueueIdSendBlocking(LWOS_QUEUE_ID(PMU, GCX),
                                    &dispatchDios, sizeof(dispatchDios));
    }
}
