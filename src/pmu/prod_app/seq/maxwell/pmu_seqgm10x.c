/*
 * Copyright 2010-2020 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file   pmu_seqgm10x.c
 * @brief  SEQ Hal Functions for GM10x
 *
 * SEQ Hal Functions implement chip specific helper functions
 */

/* ------------------------- System Includes -------------------------------- */
#include "pmusw.h"
#include "dev_pwr_csb.h"
#include "dev_disp.h"
#include "dev_ihub.h"

/* ------------------------- Application Includes --------------------------- */
#include "pmu_objseq.h"
#include "pmu_objtimer.h"
#include "pmu_objpmu.h"
#include "pmu_objdisp.h"
#include "config/g_seq_private.h"

/* ------------------------- Type Definitions ------------------------------- */
/* ------------------------- External Definitions --------------------------- */
/* ------------------------- Static Variables ------------------------------- */
/* ------------------------- Macros and Defines ----------------------------- */
/* ------------------------- Global Variables ------------------------------- */
/* ------------------------- Prototypes ------------------------------------- */
/* ------------------------- Private Functions ------------------------------ */


/*!
 * Wait for the ok2switch to (de-)assert for up to 'delayNs' nanoseconds.
 * 
 * Resolve issue where ok2switch fails to assert when VRR in oneshot mode is
 * enabled.  The VBLANK is prolonged indefinitely due to VRR not releasing the
 * frame, and ok2switch fails to assert.  However, the frame is not released
 * because is blocked by the MLCK switch.
 * 
 * If a head's RG is stalled, disable that head for ok2switch determination,
 * so that it does not prevent the signal for asserting.  Then restore the
 * setting after the ok2switch wait.
 * 
 * This is just an interim solution.
 *
 * @param[in]  pArgs
 *      Pointer to array of signal arguments
 *
 * @param[in]  delayNs
 *      The amount of time in ns to wait while waiting for the signal to occur
 *
 * @return TRUE if the signal we're waiting for took place within delayNs ns.
 */
LwBool
seqWaitForOk2Switch_GM10X
(
    LwU32 *pArgs,
    LwU32 delayNs
)
{
    LwU32  i;
    LwU32  numHeads;
    LwU32  temp;
    LwBool bWaitSuccess;
    LwU32  ok2switchSaved[LW_PDISP_ISOHUB_SWITCH_CTLA_HEAD__SIZE_1];
    LwU32  ok2switchMask = 0;

    numHeads = DISP_IS_PRESENT() ?
        DRF_VAL(_PDISP, _CLK_REM_MISC_CONFIGA, _NUM_HEADS,
            REG_RD32(FECS, LW_PDISP_CLK_REM_MISC_CONFIGA)) : 0;
    PMU_HALT_COND(numHeads <= 32);

    // Disable heads that are stalled for ok2switch determination
    for (i = 0; i < numHeads; i++)
    {
        temp = REG_RD32(FECS, LW_PDISP_RG_STATUS(i));
        if (FLD_TEST_DRF(_PDISP, _RG_STATUS, _STALLED, _YES, temp))
        {
            temp = REG_RD32(FECS, LW_PDISP_ISOHUB_SWITCH_CTLA_HEAD(i));
            ok2switchSaved[i] = temp;
            ok2switchMask |= BIT(i);

            temp = FLD_SET_DRF(_PDISP, _ISOHUB_SWITCH_CTLA_HEAD, _MODE, _IGNORE, temp);
            REG_WR32(FECS, LW_PDISP_ISOHUB_SWITCH_CTLA_HEAD(i), temp);
        }
    }

    bWaitSuccess = OS_PTIMER_SPIN_WAIT_NS_COND(seqCheckSignal, pArgs, delayNs);

    // Restore heads that were disabled
    for (i = 0; i < numHeads; i++)
    {
        if (BIT(i) & ok2switchMask)
        {
            REG_WR32(FECS, LW_PDISP_ISOHUB_SWITCH_CTLA_HEAD(i),
                     ok2switchSaved[i]);
        }
    }

    return bWaitSuccess;
}
