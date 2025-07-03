/*
 * Copyright 2016-2022 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file pmu_seqgv10x.c
 */

/* ------------------------- System Includes -------------------------------- */
#include "pmusw.h"
#include "dev_disp.h"
#include "dev_fbpa.h"
#include "dev_pri_ringstation_fbp.h"

/* ------------------------- Application Includes --------------------------- */
#include "pmu_objseq.h"
#include "pmu_bar0.h"
#include "dbgprintf.h"
#include "config/g_seq_private.h"

/* ------------------------- Type Definitions ------------------------------- */
/* ------------------------- External Definitions --------------------------- */
/* ------------------------- Static Variables ------------------------------- */
/* ------------------------- Macros and Defines ----------------------------- */
/* ------------------------- Global Variables ------------------------------- */
/* ------------------------- Prototypes ------------------------------------- */
static LwBool s_seqCheckVblank_GV10X(LwU32 *pArgs)
    GCC_ATTRIB_USED();

/* ------------------------- Public Functions ------------------------------- */

/*!
 * Used to check for a vblank signal if the scanout owner is DRIVER. The signature
 * of this function is such that it may be used in conjunction with @ref
 * OS_PTIMER_SPIN_WAIT_NS_COND as a way of waiting for a signal to assert or
 * deassert.
 *
 * @param[in]  pArgs
 *     An array of arguments:
 *     pArgs[0] - head number
 *     pArgs[1] - polarity
 *
 * @return TRUE if the signal state matches the state which is being checked
 *         for (logically-asserted '1' or logically-deasserted '0')
 * @return FALSE if the signal state does not match the expected state
 */
static LwBool
s_seqCheckVblank_GV10X
(
    LwU32 *pArgs
)
{
    LwU32  head    = pArgs[0];
    LwU32  bWaitHi = pArgs[1];
    LwBool bVblank;

    bVblank = REG_FLD_IDX_TEST_DRF_DEF(BAR0, _PDISP, _RG_STATUS, head,
                               _VBLNK, _ACTIVE);

    return bWaitHi ? bVblank : !bVblank;
}

/*!
 * Wait for the PMU signal to assert/de-assert for up to 'delayNs' nanoseconds.
 *
 * @param[in]  signal
 *      The type of signal we need to wait for
 * @param[in]  delayNs
 *      The amount of time in ns to wait for the signal to be asserted
 *
 * @return 'FLCN_OK'
 *     Signal we're waiting for is asserted within delayNs ns
 * @return 'FLCN_ERR_TIMEOUT'
 *     Timed out waiting for the signal (oclwrs only after delayNs ns are elapsed)
 * @return 'FLCN_ERR_NOT_SUPPORTED'
 *     Given signal' is not one of the signals supported by this function
 *
 */
FLCN_STATUS
seqWaitSignal_GV10X
(
    LwU32 signal,
    LwU32 delayNs
)
{
    LwU32   checkSignalArgs[3];
    LwU32  *pHead;
    LwU32  *pWaitHi;
    LwBool  bWaitHi;
    LwBool  bWaitSuccess;

    bWaitHi = DRF_VAL(_PMU_SEQ, _EVENT, _VALUE, signal) > 0;

    switch (DRF_VAL(_PMU_SEQ, _EVENT, _TYPE, signal))
    {
        case LW_PMU_SEQ_EVENT_TYPE_VBLANK:
        {
            pHead    = &checkSignalArgs[0];
            pWaitHi  = &checkSignalArgs[1];

            *pHead    = DRF_VAL(_PMU_SEQ, _EVENT, _ARG, signal);
            *pWaitHi  = bWaitHi;

            bWaitSuccess = OS_PTIMER_SPIN_WAIT_NS_COND(
                               s_seqCheckVblank_GV10X,
                               checkSignalArgs, delayNs);
            break;
        }
        default:
        {
            return FLCN_ERR_NOT_SUPPORTED;
        }
    }

    return bWaitSuccess ? FLCN_OK : FLCN_ERR_TIMEOUT;
}

/*!
 * Update the tRP value in FBPA CONFIG0 register
 *
 * @param[in]  value    Value with which the TRP field needs to be updated
 */
void
seqUpdateFbpaConfig0Trp_GV10X
(
    LwU32 value
)
{
    LwU32   fbpaConfig0 = REG_RD32(BAR0, LW_PFB_FBPA_CONFIG0);

    fbpaConfig0 = FLD_SET_DRF_NUM(_PFB_FBPA, _CONFIG0, _RP, value, fbpaConfig0);
    PMU_BAR0_WR32_SAFE(LW_PFB_FBPA_CONFIG0, fbpaConfig0);

    //
    // Write to the PRI_FENCE register ensures that all earlier accesses in the
    // same cluster have completed before any subsequent accesses in that cluster
    // are allowed to proceed.
    // Without this the system ends up in a bad state even though the tRP value
    // is updated correctly - check http://lwbugs/2046774/44 for more details.
    //
    PMU_BAR0_WR32_SAFE(LW_PPRIV_FBP_FBPS_PRI_FENCE, 0x0);
}
