/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2020-2022 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */
#include <lwtypes.h>
#include <lwmisc.h>

#include <libc/string.h>
#include <liblwriscv/print.h>
#include <liblwriscv/io.h>
#include <liblwriscv/shutdown.h>
#include <liblwriscv/ssp.h>
#include <liblwriscv/mpu.h>
#include <liblwriscv/libc.h>
#include <lwriscv/fence.h>
#include <lwriscv/peregrine.h>
#include <lwriscv/csr.h>
#include <lwriscv/sbi.h>
#include <lwriscv/status.h>
#include <riscvifriscv.h>
#include <dev_gsp.h>

#include "partitions.h"
#include "cmdmgmt.h"
#include "rm_proxy.h"
#include "rpc.h"
#include "misc.h"

extern LwU8 __dmem_print_start[];
extern LwU8 __dmem_print_size[];

// Command processing code
void cmdQueueProcess(RMPROXY_DISPATCHER_REQUEST *pRequest)
{
    LwBool replyNeeded = LW_TRUE;
    
    switch(pRequest->cmd.hdr.unitId)
    {
    case RM_GSP_UNIT_ACR:
        replyNeeded = LW_FALSE;
        dispatchAcrCommand(&pRequest->cmd);
        break;
#if RM_PROXY_ENABLE_PROXY_COMMANDS
    case RM_GSP_UNIT_RMPROXY:
        replyNeeded = LW_FALSE;
        dispatchRmProxyCommand(&pRequest->cmd);
        break;
#endif // RM_PROXY_ENABLE_PRI_COMMANDS
    case RM_GSP_UNIT_SPDM:
        replyNeeded = LW_FALSE;
        dispatchSpdmCommand(&pRequest->cmd);
        break;

    case RM_GSP_UNIT_UNLOAD:
        // Unload handler below - we need to ACK it first.
        break;

    case RM_GSP_UNIT_NULL:
        break;

    case RM_GSP_UNIT_TEST:
        break;

    case RM_GSP_UNIT_HDCP:
    case RM_GSP_UNIT_HDCP22WIRED:
    case RM_GSP_UNIT_SCHEDULER:
    default:
        rmPrintf("UNIT %d not managed.\n", pRequest->cmd.hdr.unitId);
        break;
    }
    cmdQueueSweep(pRequest->hdrOffset);

    // ACK is good enough, handle invalid units here as well
    if (replyNeeded)
    {
        RM_FLCN_MSG_GSP gspMsg;

        memset(&gspMsg, 0, sizeof(gspMsg));

        gspMsg.hdr.unitId    = pRequest->cmd.hdr.unitId;
        gspMsg.hdr.ctrlFlags = 0;
        gspMsg.hdr.seqNumId  = pRequest->cmd.hdr.seqNumId;
        gspMsg.hdr.size      = RM_FLCN_QUEUE_HDR_SIZE;

        msgQueuePostBlocking(&gspMsg.hdr, &gspMsg.msg);

        if (gspMsg.hdr.unitId == RM_GSP_UNIT_UNLOAD)
        {
            rmPrintf("Shutdown requested. So shutting down.\n");
            riscvShutdown();
        }
    }
}

LwU64
proxyPartitionSwitch(LwU64 arg0, LwU64 arg1, LwU64 arg2, LwU64 arg3, LwU64 partition_origin, LwU64 partition)
{
    riscvLwfenceRWIO();

    if (!preLockdownSync(PRE_LOCKDOWN_TIMEOUT_MS))
    {
        rmPrintf("Timed out waiting for CPU.\n");
        ccPanic();
    }

    // Mask/unmask all interrupts routed to CPU except for halt
    LwU32 irqMaskBackup, irqMset;
    irqMaskBackup = localRead(LW_PRGNLCL_RISCV_IRQMASK);

    // Bits set in IRQDEST mean "route to CPU"
    irqMset = localRead(LW_PRGNLCL_RISCV_IRQDEST);

    localWrite(LW_PRGNLCL_RISCV_IRQMSET, irqMset);
    localWrite(LW_PRGNLCL_RISCV_IRQMCLR, ~irqMset);

    LwU64 ret;
    ret = partitionSwitch(arg0, arg1, arg2, arg3, partition_origin, partition);

    // Restore interrupts
    riscvLwfenceRWIO();

    localWrite(LW_PRGNLCL_RISCV_IRQMSET, irqMaskBackup);
    localWrite(LW_PRGNLCL_RISCV_IRQMCLR, ~irqMaskBackup);

    return ret;
}

// C "entry" to partition, has stack and liblwriscv
int partition_rm(void)
{
    if (sspGenerateAndSetCanaryWithInit() != LWRV_OK)
    {
        ccPanic();
    }

    // configure print to the start of DMEM if not setup by loader (or init)
#if CC_IS_RESIDENT && !CC_PERMIT_DEBUG
    printInitEx((LwU8*)__dmem_print_start,
                (LwU16)((LwUPtr)__dmem_print_size & 0xFFFF),
                LW_PGSP_QUEUE_HEAD(RM_RISCV_DEBUG_BUFFER_QUEUE),
                LW_PGSP_QUEUE_TAIL(RM_RISCV_DEBUG_BUFFER_QUEUE), 1);
#endif // CC_IS_RESIDENT && !CC_PERMIT_DEBUG

#if CC_IS_RESIDENT
    rmPrintf("GSP-RM-Proxy, running from TCM.\n");
#else // CC_IS_RESIDENT
    rmPrintf("GSP-RM-Proxy, running from FB.\n");
#endif // CC_IS_RESIDENT

    initCmdmgmt();

    cmdmgmtCmdLoop();

    // We should never reach here. If we do, that's fatal error.
    ccPanic();

    return 0;
}

#if !CC_IS_RESIDENT
/* Non-resident code does not have access to shared panic code.
 * MK TODO: unify them somehow.
 */
__attribute__((noreturn)) void ccPanic(void)
{
#if CC_PERMIT_DEBUG
    localWrite(LW_PRGNLCL_RISCV_ICD_CMD, 0);
#endif // CC_PERMIT_DEBUG

    riscvFenceIO();

    // Just shut down the core.
    riscvShutdown();

    // In case SBI fails. Not much more we can do here.
    // coverity[no_escape]
    while(1)
    {
    }
    __builtin_unreachable();
}
#endif

void rm_proxy_trap(void)
{
    ccPanic();
}
