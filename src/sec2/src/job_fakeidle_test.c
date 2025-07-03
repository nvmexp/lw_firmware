/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2015-2022 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file   job_fakeidle_test.c
 * @brief  Fake Idle test job
 */
/* ------------------------- System Includes -------------------------------- */
#include "lwuproc.h"
#include "sec2sw.h"
#include "dev_sec_csb.h"

/* ------------------------- Application Includes --------------------------- */
#include "sec2_objsec2.h"
#include "sec2_objic.h"

/* ------------------------- Type Definitions ------------------------------- */
/* ------------------------- Macros and Defines ----------------------------- */
LwU32 FAKE_IDLE_TIMEOUT_NS = 0x8000;

/* ------------------------- External Definitions --------------------------- */
/* ------------------------- Static Variables ------------------------------- */
/* ------------------------- Global Variables ------------------------------- */
/* ------------------------- Static Function Prototypes  -------------------- */
/* ------------------------- Public Functions  ------------------------------ */
/*!
 * Entry point for the the Fake Idle test.
 * @param[in]  op         Operation to perform on behalf of MODS test
 * 
 * @return     FLCN_OK    => the operation was successful
 *             FLCN_ERROR => the operation failed
 */
FLCN_STATUS
fakeidle_test_entry
(
    LwU8 op
)
{
    FLCN_TIMESTAMP timeStart;
    switch (op)
    {
        // Unmask method and context switch interrupts
        case FAKEIDLE_UNMASK_INTR:
            lwrtosENTER_CRITICAL();
            {
                icHostIfIntrUnmask_HAL();
            }
            lwrtosEXIT_CRITICAL();
            break;

        // Mask off method and context switch interrupts
        case FAKEIDLE_MASK_INTR:
            lwrtosENTER_CRITICAL();
            {
                REG_WR32_STALL(CSB, LW_CSEC_FALCON_IRQMCLR,
                    (DRF_DEF(_CSEC, _FALCON_IRQMCLR, _MTHD,  _SET) |
                     DRF_DEF(_CSEC, _FALCON_IRQMCLR, _CTXSW, _SET)));
            }
            lwrtosEXIT_CRITICAL();
            break;

        // Set engine to SW-IDLE
        case FAKEIDLE_PROGRAM_IDLE:
            lwrtosENTER_CRITICAL();
            {
                sec2HostIdleProgramIdle_HAL();
            }
            lwrtosEXIT_CRITICAL();
            break;

        // Set engine to SW-BUSY
        case FAKEIDLE_PROGRAM_BUSY:
            lwrtosENTER_CRITICAL();
            {
                sec2HostIdleProgramBusy_HAL();
            }
            lwrtosEXIT_CRITICAL();
            break;

        // Wait until ctxsw is pending, return error on timeout
        case FAKEIDLE_CTXSW_DETECT:
            osPTimerTimeNsLwrrentGet(&timeStart);
            do
            {
                if (sec2IsNextChannelCtxValid_HAL(&Sec2, NULL))
                {
                    return FLCN_OK;
                }
                lwrtosYIELD();
            } while (osPTimerTimeNsElapsedGet(&timeStart) <
                     FAKE_IDLE_TIMEOUT_NS);

            return FLCN_ERROR;

        // Wait until method is pending, return error on timeout
        case FAKEIDLE_MTHD_DETECT:
            osPTimerTimeNsLwrrentGet(&timeStart);
            do
            {
                if (REG_FLD_RD_DRF(CSB, _CSEC, _FALCON_MTHDCOUNT, _COUNT) > 0)
                {
                    return FLCN_OK;
                }
                lwrtosYIELD();
            } while (osPTimerTimeNsElapsedGet(&timeStart) <
                     FAKE_IDLE_TIMEOUT_NS);

            return FLCN_ERROR;

        // Wait until all methods are handled, return error on timeout
        case FAKEIDLE_COMPLETE:
            osPTimerTimeNsLwrrentGet(&timeStart);
            do
            {
                if (REG_FLD_RD_DRF(CSB, _CSEC, _FALCON_MTHDCOUNT, _COUNT) == 0 &&
                    REG_FLD_TEST_DRF_DEF(CSB, _CSEC, _FALCON_IDLESTATE,
                                 _ENGINE_BUSY_CYA, _SW_IDLE))

                {
                    return FLCN_OK;
                }
                lwrtosYIELD();
            } while (osPTimerTimeNsElapsedGet(&timeStart) <
                     FAKE_IDLE_TIMEOUT_NS);

            return FLCN_ERROR;
    }
    return FLCN_OK;
}

/* ------------------------ Static Functions ------------------------------- */
