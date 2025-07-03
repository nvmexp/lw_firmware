/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2019-2022 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file    shlib.c
 * @brief   Library for code shared between kernel and userspace tasks.
 */

/* ------------------------ System Includes -------------------------------- */

/* ------------------------ LW Includes ------------------------------------ */

/* ------------------------ SafeRTOS Includes ------------------------------ */
#include <sections.h>
#include <lwrtos.h>
/* ------------------------ Module Includes -------------------------------- */
#include <lwriscv/print.h>
#include <drivers/drivers.h>
#include "shlib/syscall.h"
#if LWRISCV_PARTITION_SWITCH
# include "shlib/partswitch.h"
# ifdef DMA_SUSPENSION
#  include <lwos_dma.h>
# endif // DMA_SUSPENSION
#endif // LWRISCV_PARTITION_SWITCH

#if LWRISCV_PARTITION_SWITCH

sysKERNEL_CODE __attribute__((weak)) void lwrtosHookPartitionPreSwitch(LwU64 partition)
{
    (void)partition;

    // MMINTS-TODO: add default impl here?
}

sysKERNEL_CODE __attribute__((weak)) void lwrtosHookPartitionReentry(LwU64 partition)
{
    (void)partition;

    // MMINTS-TODO: add default impl here?
}

sysSHARED_CODE LwU64
partitionSwitch(LwU64 partition, LwU64 arg0, LwU64 arg1, LwU64 arg2, LwU64 arg3, LwU64 arg4)
{
    LwU64 ret;

    if (lwrtosIS_KERNEL())
    {
#ifdef DMA_SUSPENSION
        if (fbhubGatedGet())
        {
            //
            // Partitions might want to perform DMA and/or access memory in general. Make sure that we're not
            // going into the other partition while FBhub is gated.
            //
            dbgPrintf(LEVEL_CRIT, "Cannot perform partition switch if memory is not accessible!\n");
            kernelVerboseCrash(0);
        }
#endif // DMA_SUSPENSION

        lwrtosHookPartitionPreSwitch(partition);

        ret = partitionSwitchKernel(arg0, arg1, arg2, arg3, arg4, partition);

        lwrtosHookPartitionReentry(partition);
    }
    else
    {
#ifdef DMA_SUSPENSION
        // Make sure we are out of DMA suspension before trying to do a partition switch.

        lwrtosTaskCriticalEnter();
        {
            lwosDmaLockAcquire();
            lwosDmaExitSuspension();
#endif // DMA_SUSPENSION

            ret = syscall6(LW_APP_SYSCALL_PARTITION_SWITCH, arg0, arg1, arg2, arg3, arg4, partition);

#ifdef DMA_SUSPENSION
            lwosDmaLockRelease();
        }
        lwrtosTaskCriticalExit();
#endif // DMA_SUSPENSION
    }

    return ret;
}
#endif // LWRISCV_PARTITION_SWITCH
