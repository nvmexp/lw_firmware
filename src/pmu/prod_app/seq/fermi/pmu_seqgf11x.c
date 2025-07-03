/*
 * Copyright 2010-2022 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file   pmu_seqgf11x.c
 * @brief  SEQ Hal Functions for GF11X
 *
 * SEQ Hal Functions implement chip specific helper functions
 */

/* ------------------------- System Includes -------------------------------- */
#include "pmusw.h"
#include "dev_pwr_csb.h"
#include "dev_disp.h"

/* ------------------------- Application Includes --------------------------- */
#include "pmu_objseq.h"
#include "pmu_objtimer.h"
#include "pmu_objpmu.h"
#include "config/g_seq_private.h"

/* ------------------------- Type Definitions ------------------------------- */
/* ------------------------- External Definitions --------------------------- */
/* ------------------------- Static Variables ------------------------------- */
/* ------------------------- Macros and Defines ----------------------------- */
/* ------------------------- Global Variables ------------------------------- */
/* ------------------------- Prototypes ------------------------------------- */
static LwBool seqCheckVblank_GMXXX(LwU32 *pArgs)
    GCC_ATTRIB_USED();

/* ------------------------- Private Functions ------------------------------ */

/*!
 * Used to check for a vblank signal if the scanout owner is DRIVER.  The
 * signature of this function is such that it may be used in conjunction with
 * @ref OS_PTIMER_SPIN_WAIT_NS_COND as way of waiting for a signal to assert or
 * deassert in a specific direction.
 *
 * @bug 740001
 *
 * @param[in]  pArgs
 *     An array of arguments for the function.
 *
 *     pArgs[0] - head number
 *     pArgs[1] - polarity
 *
 * @return A postive non-zero integer if the signal-state matches the state
 *         which is being checked for (logically-asserted '1' or logically-
 *         deasserted '0')
 *
 * @return Zero if the signal-state does not match the expected state
 */
static LwBool
seqCheckVblank_GMXXX
(
    LwU32 *pArgs
)
{
    LwU32  head    = pArgs[0];
    LwU32  bWaitHi = pArgs[1];
    LwBool bVblank;

    //
    // Bug 740001 : DMI_POS can't be used if the scanout owner is VBIOS. So we
    // need to pretend as if we got a vblank if the owner is not DRIVER.
    //
    if (REG_FLD_IDX_TEST_DRF_DEF(BAR0, _PDISP, _DSI_CORE_HEAD_STATE, head,
                         _SHOWING_ACTIVE, _VBIOS))
    {
        return LW_TRUE;
    }

    bVblank = REG_FLD_IDX_TEST_DRF_DEF(BAR0, _PDISP, _DMI_POS, head,
                               _STATUS, _VBLANK);

    return bWaitHi ? bVblank : !bVblank;
}

/*!
 * Wait for the a PMU signal to (de-)assert for up to 'delayNs' nanoseconds.
 * This function is typically used for polling on GPIO signals.
 *
 * @param[in]  signal
 *      The type of signal we're waiting for
 *
 * @param[in]  delayNs
 *      The amount of time in ns to wait while waiting for the signal to occur
 *
 * @return 'FLCN_OK'
 *     The signal we're waiting for took place within delayNs ns.
 *
 * @return 'FLCN_ERR_TIMEOUT'
 *     Returned when timed out after delayNs ns while waiting for the signal
 *
 * @return 'FLCN_ERR_NOT_SUPPORTED'
 *     Returned when 'signal' is not one of the supported signals for this chip
 *
 */
FLCN_STATUS
seqWaitSignal_GM10X
(
    LwU32 signal,
    LwU32 delayNs
)
{
    LwU32   checkSignalArgs[3];
    LwU32  *pSignalMask;
    LwU32  *pHead;
    LwU32  *pBWaitHi;
    LwU32  *pEvtRegAddr;
    LwBool  bWaitHi;
    LwBool  bWaitSuccess;

    bWaitHi = DRF_VAL(_PMU_SEQ, _EVENT, _VALUE, signal) != 0;

    switch (DRF_VAL(_PMU_SEQ, _EVENT, _TYPE, signal))
    {
        case LW_PMU_SEQ_EVENT_TYPE_VBLANK:
        {
            pHead    = &checkSignalArgs[0];
            pBWaitHi = &checkSignalArgs[1];

            *pHead    = DRF_VAL(_PMU_SEQ, _EVENT, _ARG, signal);
            *pBWaitHi = bWaitHi;

            bWaitSuccess = OS_PTIMER_SPIN_WAIT_NS_COND(
                               seqCheckVblank_GMXXX, 
                               checkSignalArgs, delayNs);

            break;
        }
        case LW_PMU_SEQ_EVENT_TYPE_IHUB_OK:
        {
            pSignalMask = &checkSignalArgs[0];
            pBWaitHi    = &checkSignalArgs[1];
            pEvtRegAddr = &checkSignalArgs[2];

            *pEvtRegAddr = LW_CPWR_PMU_GPIO_1_INPUT;

            *pBWaitHi = bWaitHi;

            *pSignalMask = DRF_DEF(_CPWR_PMU, _GPIO_1_INPUT,
                                   _IHUB_OK_TO_SWITCH, _TRUE);

            bWaitSuccess = seqWaitForOk2Switch_HAL(&Seq, checkSignalArgs,
                                                   delayNs);
            break;
        }
        default:
        {
            return FLCN_ERR_NOT_SUPPORTED;
        }
    }

    return bWaitSuccess ? FLCN_OK : FLCN_ERR_TIMEOUT;
}
