/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2017-2018 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file   dpu_ic0207.c
 * @brief  DPU 02.07 Hal Functions
 *
 * DPU HAL functions implement chip-specific initialization, helper functions
 */

/* ------------------------- System Includes -------------------------------- */
#include "dpusw.h"
#include "dev_disp.h"
#include "dispflcn_regmacros.h"

/* ------------------------- Application Includes --------------------------- */
#include "class/cl947d.h"
#include "dpu_objdpu.h"
#include "dpu_objic.h"
#include "dpu_vrr.h"
#include "dpu_task.h"

#include "config/g_ic_private.h"

/* ------------------------- Type Definitions ------------------------------- */
/* ------------------------- External Definitions --------------------------- */
/* ------------------------- Static Variables ------------------------------- */
/* ------------------------- Macros and Defines ----------------------------- */
/* ------------------------- Global Variables ------------------------------- */
/* ------------------------- Prototypes ------------------------------------- */
/* ------------------------- Implementation --------------------------------- */

/*!
 * Service the PMU RG line intr specific to head passed from RM
 *
 * Once a PMU RG line interrupt is received by DPU, it will notify this as a event
 * to Vrr task to schedule timer callback for forcing frame
 */
void
icServicePmuRgLineIntr_v02_07(LwU32 headNum)
{
    LwU32                    intrHead;
    LwU32                    pendingIntr = 0;
    DISPATCH_VRR             vrrRgVblankEvent;
    LwU32                    stallLock;

    intrHead = REG_RD32(CSB, LW_PDISP_DSI_DPU_INTR_HEAD(headNum));
    pendingIntr = (intrHead & DRF_SHIFTMASK(LW_PDISP_DSI_DPU_INTR_HEAD_PMU_RG_LINE));

    if (DPUCFG_FEATURE_ENABLED(DPUTASK_VRR))
    {
        if (VrrContext.bEnableVrrForceFrameRelease)
        {
            // Get STALL_LOCK to check OSM is active or suspended
            stallLock = REG_RD32(CSB, LW_UDISP_DSI_CHN_ARMED_BASEADR(LW_PDISP_907D_CHN_CORE) +
                                       LW947D_HEAD_SET_STALL_LOCK(VrrContext.head));

            if ((VrrContext.head == headNum) &&
                (FLD_TEST_DRF(947D, _HEAD_SET_STALL_LOCK, _ENABLE, _TRUE,     stallLock)) &&
                (FLD_TEST_DRF(947D, _HEAD_SET_STALL_LOCK, _MODE,   _ONE_SHOT, stallLock)))
            {
                vrrRgVblankEvent.eventType = DISP2UNIT_EVT_SIGNAL;

                // task is queued
                lwrtosQueueSendFromISR(VrrQueue, &vrrRgVblankEvent,
                                       sizeof(vrrRgVblankEvent));
            }
        }
    }

    // reset the handled interrupt
    REG_WR32(CSB, LW_PDISP_DSI_DPU_INTR_HEAD(headNum),
                 pendingIntr);
}
