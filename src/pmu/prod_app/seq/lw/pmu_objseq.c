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
 * @file   pmu_objseq.c
 * @brief  Container-object for the PMU Sequencer routines.  Contains generic
 * non-HAL interrupt-routines plus logic required to hook-up chip-specific
 * interrupt HAL-routines.
 *
 */

/* ------------------------- System Includes -------------------------------- */
#include "pmusw.h"

/* ------------------------- Application Includes --------------------------- */
#include "main.h"
#include "main_init_api.h"
#include "pmu_objhal.h"
#include "pmu_oslayer.h"
#include "pmu_objpmu.h"
#include "pmu_objseq.h"

#include "config/g_seq_private.h"
#if(SEQ_MOCK_FUNCTIONS_GENERATED)
#include "config/g_seq_mock.c"
#endif

/* ------------------------- Static Variables ------------------------------- */
/* ------------------------- Global Variables ------------------------------- */
OBJSEQ Seq;

/* ------------------------- External Definitions --------------------------- */
/* ------------------------- Public Interfaces ------------------------------ */
/*!
 * Used to check for certain signal values by reading the input register
 * and comparing the return value with a signal mask. Can be used to determine
 * if a signal is asserted or deasserted depending on the values provided as
 * arguments.
 *
 * The signature of this function is such that it may be used in conjunction
 * with @ref OS_PTIMER_SPIN_WAIT_NS_COND as way of waiting for a signal to
 * assert or deassert in a specific direction.
 *
 * @param[in]  pArgs
 *     An array of arguments for the function.  pArgs[0] is expected to be
 *     the signal-mask (AND-mask) to apply to the value of the input register
 *     (pArgs[2]) to specify the exact signal(s) of interest. pArgs[1] specifies
 *     if we are checking for the signal to be asserted high or low.  It is
 *     expected that the bit-indices of pArgs[0] and pArgs[1] match 1:1.
 *
 *     pArgs[0] - signal mask
 *     pArgs[1] - polarity
 *     pArgs[2] - input register
 *
 * @return A postive non-zero integer if the signal-state matches the state
 *         which is being checked for (logically-asserted '1' or logically-
 *         deasserted '0')
 *
 * @return Zero if the signal-state does not match the expected state
 */
LwBool
seqCheckSignal
(
    LwU32 *pArgs
)
{
    LwU32  signalMask;
    LwU32  bWaitHi;
    LwU32  reg32;
    LwU32  stat;

    signalMask = pArgs[0];
    bWaitHi    = pArgs[1];
    reg32      = pArgs[2];

    stat = REG_RD32(CSB, reg32);
    if (!bWaitHi)
    {
        stat = ~stat;
    }
    return (stat & signalMask) != 0;
}
