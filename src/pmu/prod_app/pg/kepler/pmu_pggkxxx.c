/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2010-2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file   pmu_pggkxxx.c
 * @brief  PMU Power Gating Object
 *
 * PG - KEPLER PG
 * Features implemented : ELPG and MSCG/SW-ASR
 */

/* ------------------------- System Includes -------------------------------- */
#include "pmusw.h"

#include "dev_pwr_csb.h"
#include "dev_therm.h"

/* ------------------------- Application Includes --------------------------- */
#include "main.h"
#include "pmu_objms.h"
#include "pmu_objpg.h"
#include "pmu_objap.h"
#include "pmu_objpmu.h"
#include "pmu_objgr.h"
#include "pmu_objfifo.h"
#include "dbgprintf.h"

#include "config/g_pg_private.h"

/* ------------------------- Type Definitions ------------------------------- */
/* ------------------------- External Definitions --------------------------- */
/* ------------------------- Macros and Defines ----------------------------- */
/* ------------------------- Global Variables ------------------------------- */
/* ------------------------- Prototypes ------------------------------------- */
/* ------------------------  Public Functions  ------------------------------ */

/*!
 * @brief Colwert HW interrupts to SW Events
 *
 * This function handles various kepler specific PG events such as ELPG
 * interrupts, MSCG wakeup interrupts
 *
 * @param[in]   pLpwrEvt        This contains eventType, and the event's
 *                              private data
 * @param[out]  pPgLogicState   Pointer to PG Logic state
 *
 * @return      FLCN_OK                                 On success
 * @return      FLCN_ERR_NOT_SUPPORTED                  PgCtrl is not supported
 * @return      FLCN_ERR_QUEUE_TASK_ILWALID_EVENT_TYPE  Otherwise
 */
FLCN_STATUS
pgProcessInterrupts_GMXXX
(
    DISPATCH_LPWR   *pLpwrEvt,
    PG_LOGIC_STATE  *pPgLogicState
)
{
    LwU32       intrStat = 0;
    LwU32       ctrlId   = 0;
    FLCN_STATUS status   = FLCN_OK;
    OBJPGSTATE *pPgState = NULL;

    switch (pLpwrEvt->hdr.eventType)
    {
        case LPWR_EVENT_ID_PG_INTR:
        {
            ctrlId = pLpwrEvt->intr.ctrlId;

            //
            // Process FSM event only if PgCtrl is supported otherwise ignore
            // the event.
            //
            if (pgIsCtrlSupported(ctrlId))
            {
                pPgLogicState->ctrlId = ctrlId;

                intrStat = REG_RD32(CSB, LW_CPWR_PMU_PG_INTRSTAT(PG_GET_ENG_IDX(ctrlId)));
                pgColwertPgInterrupts_HAL(&Pg, pPgLogicState, intrStat);
            }
            else
            {
                PMU_BREAKPOINT();
                status = FLCN_ERR_NOT_SUPPORTED;
            }
            break;
        }
        case LPWR_EVENT_ID_PG_HOLDOFF_INTR:
        {
            FOR_EACH_INDEX_IN_MASK(32, ctrlId, Pg.supportedMask)
            {
                pPgState = GET_OBJPGSTATE(ctrlId);
                if (pPgState->holdoffMask & pLpwrEvt->intr.intrStat)
                {
                    // Send wake-up event
                    pgCtrlEventSend(ctrlId, PMU_PG_EVENT_WAKEUP, PG_WAKEUP_EVENT_HOLDOFF);
                }
            }
            FOR_EACH_INDEX_IN_MASK_END

            break;
        }
        default:
        {
            PMU_BREAKPOINT();
            status = FLCN_ERR_QUEUE_TASK_ILWALID_EVENT_TYPE;
        }
    }

    return status;
}

/*!
 * @brief Configures AP Supplemental Idle Counter.
 *
 * It sets Idle Mask for Supplemental Idle Counter
 *
 * @param[in]  suppMaskHwIdx    Index to HW counter
 * @param[in]  pSuppMask        Mask of Supplemental Idle Signal
 */
void
pgApConfigIdleMask_GMXXX
(
    LwU32  suppMaskHwIdx,
    LwU32 *pSuppMask
)
{
    REG_WR32(CSB, LW_CPWR_PMU_IDLE_MASK_SUPP(suppMaskHwIdx), pSuppMask[0]);
    REG_WR32(CSB, LW_CPWR_PMU_IDLE_MASK_1_SUPP(suppMaskHwIdx), pSuppMask[1]);
}

/*!
 * @brief  Configures AP supplemental idle counter mode.
 *
 * This function configures operation mode of Supplemental Idle Counter to
 * PG_SUPP_IDLE_COUNTER_MODE_xyz.
 * Refer @PG_SUPP_IDLE_COUNTER_MODE_xyz for further details.
 *
 * @param[in]  suppMaskHwIdx    Index to HW counter
 * @param[in]  operationMode    Define by PG_SUPP_IDLE_COUNTER_MODE_xyz
 */
void
pgApConfigCntrMode_GMXXX
(
    LwU32 suppMaskHwIdx,
    LwU32 operationMode
)
{
    switch (operationMode)
    {
        case PG_SUPP_IDLE_COUNTER_MODE_FORCE_IDLE:
        {
            REG_WR32(CSB, LW_CPWR_PMU_IDLE_CTRL_SUPP(suppMaskHwIdx),
                          DRF_DEF(_CPWR_PMU, _IDLE_CTRL_SUPP, _VALUE, _ALWAYS));
            break;
        }
        case PG_SUPP_IDLE_COUNTER_MODE_FORCE_BUSY:
        {
            REG_WR32(CSB, LW_CPWR_PMU_IDLE_CTRL_SUPP(suppMaskHwIdx),
                          DRF_DEF(_CPWR_PMU, _IDLE_CTRL_SUPP, _VALUE, _NEVER));
            break;
        }
        case PG_SUPP_IDLE_COUNTER_MODE_AUTO_IDLE:
        {
            REG_WR32(CSB, LW_CPWR_PMU_IDLE_CTRL_SUPP(suppMaskHwIdx),
                          DRF_DEF(_CPWR_PMU, _IDLE_CTRL_SUPP, _VALUE, _IDLE));
            break;
        }
        default:
        {
            PMU_HALT();
        }
    }
}

/*!
 * @brief Colwert HW FSM interrupts to SW Events
 *
 * This function colwerts FSM interrupts to SW events. This is interrupt
 * colwersion/service order -
 * 1) CFG_ERR
 * 2) PG_ON
 * 3) PG_ON_DONE
 * 4) ENG_RST
 * 5) CTX_RESTORE
 *
 * @param[in/out]  pPgLogicState  Pointer to PG Logic state
 * @                              Translates the dispatch pg (RM Command or
 *                                HW interrupts) into PG event identifier
 * @param[in]   intrStat          Translates the dispatch pg (RM Command or
 *                                HW interrupts) into PG event identifier
 */
void
pgColwertPgInterrupts_GMXXX
(
    PG_LOGIC_STATE *pPgLogicState,
    LwU32           intrStat
)
{
    //
    // Config error is treated as fatal error. Highlight that by asserting
    // breakpoint.
    //
    if (FLD_TEST_DRF(_CPWR, _PMU_PG_INTRSTAT, _CFG_ERR, _SET, intrStat))
    {
        PMU_BREAKPOINT();
    }

    //
    // There is possibility that other FSM interrupts are pending along with
    // config error. Try to process them.
    //
    if (FLD_TEST_DRF(_CPWR, _PMU_PG_INTRSTAT, _PG_ON, _SET, intrStat))
    {
        pPgLogicState->eventId = PMU_PG_EVENT_PG_ON;
    }
    else if (FLD_TEST_DRF(_CPWR, _PMU_PG_INTRSTAT, _PG_ON_DONE, _SET, intrStat))
    {
        pPgLogicState->eventId = PMU_PG_EVENT_PG_ON_DONE;
    }
    else if (FLD_TEST_DRF(_CPWR, _PMU_PG_INTRSTAT, _ENG_RST, _SET, intrStat))
    {
        pPgLogicState->eventId = PMU_PG_EVENT_ENG_RST;
    }
    else if (FLD_TEST_DRF(_CPWR, _PMU_PG_INTRSTAT, _CTX_RESTORE, _SET, intrStat))
    {
        pPgLogicState->eventId = PMU_PG_EVENT_CTX_RESTORE;
    }
}
