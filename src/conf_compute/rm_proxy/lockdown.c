/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2022 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

#include <lwtypes.h>
#include <lwstatus.h>
#include <dev_gsp_prgnlcl.h>
#include <dev_gsp.h>

#include <riscvifriscv.h>

#include <liblwriscv/ptimer.h>
#include <lwriscv/csr.h>
#include <liblwriscv/print.h>
#include <liblwriscv/io.h>

#include "rm_proxy.h"
#include "cmdmgmt.h"
#include "partitions.h"

static LwBool areSwGenClear(void)
{
    LwU32 reg = localRead(LW_PRGNLCL_FALCON_IRQSTAT);

    return  FLD_TEST_DRF(_PRGNLCL, _FALCON_IRQSTAT, _SWGEN0, _FALSE, reg) &&
            FLD_TEST_DRF(_PRGNLCL, _FALCON_IRQSTAT, _SWGEN1, _FALSE, reg);
}

/*
 * WARNING: this probably should be part of SDK, but there is not enough time
 * to put it for initial signing.
 */
static LwBool isPrintBufferEmpty(void)
{
    return priRead(LW_PGSP_QUEUE_HEAD(RM_RISCV_DEBUG_BUFFER_QUEUE)) ==
           priRead(LW_PGSP_QUEUE_TAIL(RM_RISCV_DEBUG_BUFFER_QUEUE));
}

LwBool preLockdownSync(LwU64 timeout_ms)
{
    LwU64 lockdownSyncDeadline = (LwU64)ptimer_read() + timeout_ms * 1000U * 1000U;

    // Wait for prints to be processed.
    while (!isPrintBufferEmpty())
    {
        if (lockdownSyncDeadline < ptimer_read())
        {
            return LW_FALSE;
        }
    }

    // Wait for message queue to be empty
    while (!msgQueueIsEmpty())
    {
        if (lockdownSyncDeadline < ptimer_read())
        {
            return LW_FALSE;
        }
    }

    // Wait for SWGEN0/1 to get idle
    while (!areSwGenClear())
    {
        if (lockdownSyncDeadline < ptimer_read())
        {
            return LW_FALSE;
        }
    }

    return LW_TRUE;
}
