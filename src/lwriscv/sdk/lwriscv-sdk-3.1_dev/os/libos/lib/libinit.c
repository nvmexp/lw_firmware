/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2019-2020 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

#include "libos.h"
#include "libos_log.h"

typedef struct DaemonMessage
{
    LwU64 type;
    union
    {
#if LIBOS_LWRISCV >= LIBOS_LWRISCV_2_0
        struct
        {
            LwU64 params[5];
            LwU64 partitionId;
        } switchTo;
#endif
    };
} DaemonMessage;

LibosPortNameExtern(libosDaemonPort);

#if LIBOS_FEATURE_PAGING
void LibosInitRegisterPagetables(libos_pagetable_entry_t *hash, LwU64 lengthPow2, LwU64 probeCount)
{
    register LwU64 t0 __asm__("t0") = LIBOS_SYSCALL_INIT_REGISTER_PAGETABLES;
    register LwU64 a0 __asm__("a0");
    register LwU64 a1 __asm__("a1") = (LwU64)hash;
    register LwU64 a2 __asm__("a2") = lengthPow2;
    register LwU64 a3 __asm__("a3") = probeCount;
    __asm__ __volatile__("ecall" : "=r"(a0): "r"(t0), "r"(a1), "r"(a2), "r"(a3) : "memory");
}
#endif

#if LIBOS_CONFIG_PROFILER_SAVE_ON_SUSPEND
void LibosProfilerEnable(void)
{
    register LwU64 t0 __asm__("t0") = LIBOS_SYSCALL_PROFILER_ENABLE;
    __asm__ __volatile__("ecall" : : "r"(t0) : "memory");
}
#endif


#if !LIBOS_FEATURE_PAGING
void LibosBootstrapMmap(LwU64 index, LwU64 va, LwU64 pa, LwU64 size, LwU64 attributes)
{
    register LwU64 t0 __asm__("t0") = LIBOS_SYSCALL_BOOTSTRAP_MMAP;
    register LwU64 a0 __asm__("a0") = index;
    register LwU64 a1 __asm__("a1") = va;
    register LwU64 a2 __asm__("a2") = pa;
    register LwU64 a3 __asm__("a3") = size;
    register LwU64 a4 __asm__("a4") = attributes;
    __asm__ __volatile__("ecall" : : "r"(t0), "r"(a0), "r"(a1), "r"(a2), "r"(a3), "r"(a4) : "memory");
}

#endif

#if LIBOS_FEATURE_PARTITION_SWITCH 
#   if LIBOS_LWRISCV >= LIBOS_LWRISCV_2_0

void LibosPartitionSwitchInternal
(
    LwU64 partitionId,
    const LwU64 inArgs[LIBOS_PARTITION_SWITCH_ARG_COUNT],
    LwU64 outArgs[LIBOS_PARTITION_SWITCH_ARG_COUNT]
)
{
    register LwU64 t0 __asm__("t0") = LIBOS_SYSCALL_SWITCH_TO;
    register LwU64 a0 __asm__("a0") = inArgs[0];
    register LwU64 a1 __asm__("a1") = inArgs[1];
    register LwU64 a2 __asm__("a2") = inArgs[2];
    register LwU64 a3 __asm__("a3") = inArgs[3];
    register LwU64 a4 __asm__("a4") = inArgs[4];
    register LwU64 a5 __asm__("a5") = partitionId;
    __asm__ __volatile__(
        "ecall"
        : "+r"(a0), "+r"(a1), "+r"(a2), "+r"(a3), "+r"(a4)
        : "r"(t0), "r"(a5)
        : "memory"
    );
    outArgs[0] = a0;
    outArgs[1] = a1;
    outArgs[2] = a2;
    outArgs[3] = a3;
    outArgs[4] = a4;
}

void LibosPartitionSwitch
(
    LwU64 partitionId,
    const LwU64 inArgs[LIBOS_PARTITION_SWITCH_ARG_COUNT],
    LwU64 outArgs[LIBOS_PARTITION_SWITCH_ARG_COUNT]
)
{
    DaemonMessage message =
    {
        .type = LIBOS_DAEMON_SWITCH_TO,
        .switchTo =
        {
            .params = {inArgs[0], inArgs[1], inArgs[2], inArgs[3], inArgs[4]},
            .partitionId = partitionId
        }
    };
    LibosPortSyncSendRecv(ID(libosDaemonPort), &message, sizeof(message), 0, outArgs, sizeof(LwU64) * LIBOS_PARTITION_SWITCH_ARG_COUNT, 0, LibosTimeoutInfinite);
}

#else

void LibosProcessorSuspendFromInit(void)
{
    register LwU64 t0 __asm__("t0") = LIBOS_SYSCALL_PROCESSOR_SUSPEND;
    register LwU64 a0 __asm__("a0");
    __asm__ __volatile__("ecall" : "=r"(a0) : "r"(t0) : "memory");
}

void LibosPartitionSuspend()
{
    DaemonMessage message = { .type = LIBOS_DAEMON_SUSPEND_PROCESSOR };
    LibosPortSyncSendRecv(ID(libosDaemonPort), &message, sizeof(message), 0, 0, 0, 0, LibosTimeoutInfinite);
}

#endif
#endif


#if LIBOS_FEATURE_SHUTDOWN
void LibosProcessorShutdown(void)
{
    register LwU64 t0 __asm__("t0") = LIBOS_SYSCALL_SHUTDOWN;
    // coverity[set_but_not_used]
    register LwU64 a0 __asm__("a0");
    __asm__ __volatile__("ecall" : "=r"(a0) : "r"(t0) : "memory");
}
#endif

#if LIBOS_FEATURE_USERSPACE_TRAPS
LibosTaskNameExtern(taskInit);
LibosShuttleNameDecl(taskInitShuttleTrap);
LibosShuttleNameDecl(taskInitShuttleDaemon);
LibosManifestShuttle(taskInit, taskInitShuttleTrap);
LibosManifestShuttle(taskInit, taskInitShuttleDaemon)
#endif

LwU32 LibosInitDaemon()
{
    DaemonMessage message;
    LwU64 messageSize = 0;

#if LIBOS_FEATURE_USERSPACE_TRAPS
    LibosTaskState taskState;
    LibosPortAsyncRecv(
        ID(taskInitShuttleTrap), ID(panicPort), &taskState, sizeof(taskState));

    LibosPortAsyncRecv(
        ID(taskInitShuttleDaemon), ID(libosDaemonPort), &message, sizeof(message));
#endif

    while(1)
    {
#if !LIBOS_FEATURE_USERSPACE_TRAPS
    if (LibosPortSyncRecv(ID(libosDaemonPort), &message, sizeof(message), &messageSize, LibosTimeoutInfinite) != LibosOk)
      return 1;
#else
    LibosShuttleId whichShuttle;
    LibosPortWait(
        ID(shuttleAny),
        &whichShuttle,
        &messageSize,
        0,
        LibosTimeoutInfinite);

    if (whichShuttle == ID(taskInitShuttleTrap))
    {
        LibosInitHookTaskTrap(&taskState, ID(taskInitShuttleTrap));

        LibosPortAsyncRecv(
            ID(taskInitShuttleTrap), ID(panicPort), &taskState, sizeof(taskState));

        continue;
    }
#endif

#if LIBOS_FEATURE_PARTITION_SWITCH
#   if LIBOS_LWRISCV >= LIBOS_LWRISCV_2_0
    // coverity[uninit_use]
    if (message.type == LIBOS_DAEMON_SWITCH_TO)
    {
        // coverity[uninit_use]
        LwU64 inArgs[LIBOS_PARTITION_SWITCH_ARG_COUNT] =
        {
            message.switchTo.params[0],
            message.switchTo.params[1],
            message.switchTo.params[2],
            message.switchTo.params[3],
            message.switchTo.params[4]
        };
        LwU64 outArgs[LIBOS_PARTITION_SWITCH_ARG_COUNT];
        LibosCriticalSectionEnter(LibosCriticalSectionPanicOnTrap);
        {
            LibosPartitionSwitchInternal(
                message.switchTo.partitionId,
                inArgs,
                outArgs
            );
            LibosInitHookMpu();
        }
        LibosCriticalSectionLeave();
#if !LIBOS_FEATURE_USERSPACE_TRAPS
        LibosPortSyncReply(outArgs, sizeof(outArgs), 0);
#else
        LibosPortAsyncReply(ID(taskInitShuttleDaemon), outArgs, sizeof(outArgs), 0);
#endif      
    }
#else
    if (message.type == LIBOS_DAEMON_SUSPEND_PROCESSOR)
    {
        LibosCriticalSectionEnter(LibosCriticalSectionPanicOnTrap);
        {
            LibosProcessorSuspendFromInit();
            LibosInitHookMpu();
        }
        LibosCriticalSectionLeave();
#if !LIBOS_FEATURE_USERSPACE_TRAPS
        LibosPortSyncReply(0, 0, 0);
#else
        LibosPortAsyncReply(ID(taskInitShuttleDaemon), 0, 0, 0);
#endif
    }
#endif
#endif
#if LIBOS_FEATURE_USERSPACE_TRAPS
    LibosPortAsyncRecv(
        ID(taskInitShuttleDaemon), ID(libosDaemonPort), &message, sizeof(message));
#endif        
    }
    return 0;
}
