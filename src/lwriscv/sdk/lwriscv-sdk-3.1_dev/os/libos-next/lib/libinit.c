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

typedef struct DaemonMessage
{
    struct {
        LwU64 interface;
        LwU64 message;
    } header;

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


#if LIBOS_TINY
void LibosBootstrapMmap(LwU64 index, LwU64 va, LwU64 pa, LwU64 size, LwU64 attributes)
{
#ifndef LIBOS_SYSCALL_DIRECT_DISPATCH
    register LwU64 t0 __asm__("t0") = LIBOS_SYSCALL_BOOTSTRAP_MMAP;
#else
    extern LibosStatus KernelSyscallBootstrapMmap();
    register LwU64 t0 __asm__("t0") = (LwU64)KernelSyscallBootstrapMmap;
#endif
    register LwU64 a0 __asm__("a0") = index;
    register LwU64 a1 __asm__("a1") = va;
    register LwU64 a2 __asm__("a2") = pa;
    register LwU64 a3 __asm__("a3") = size;
    register LwU64 a4 __asm__("a4") = attributes;
    __asm__ __volatile__(LIBOS_SYSCALL_INSTRUCTION : : "r"(t0), "r"(a0), "r"(a1), "r"(a2), "r"(a3), "r"(a4) : "memory");
}
#endif


#   if LIBOS_LWRISCV >= LIBOS_LWRISCV_2_0

void LibosPartitionSwitchInternal
(
    LwU64 partitionId,
    const LwU64 inArgs[LIBOS_PARTITION_SWITCH_ARG_COUNT],
    LwU64 outArgs[LIBOS_PARTITION_SWITCH_ARG_COUNT]
)
{
#ifndef LIBOS_SYSCALL_DIRECT_DISPATCH
    register LwU64 t0 __asm__("t0") = LIBOS_SYSCALL_SWITCH_TO;
#else
    extern LibosStatus KernelSyscallPartitionSwitch();
    register LwU64 t0 __asm__("t0") = (LwU64)KernelSyscallPartitionSwitch;
#endif
    register LwU64 a0 __asm__("a0") = inArgs[0];
    register LwU64 a1 __asm__("a1") = inArgs[1];
    register LwU64 a2 __asm__("a2") = inArgs[2];
    register LwU64 a3 __asm__("a3") = inArgs[3];
    register LwU64 a4 __asm__("a4") = inArgs[4];
    register LwU64 a5 __asm__("a5") = partitionId;
    __asm__ __volatile__(
        LIBOS_SYSCALL_INSTRUCTION
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
        .header.message = LIBOS_DAEMON_SWITCH_TO,
        .switchTo =
        {
            .params = {inArgs[0], inArgs[1], inArgs[2], inArgs[3], inArgs[4]},
            .partitionId = partitionId
        }
    };
    LibosPortSyncSendRecv(ID(libosDaemonPort), &message.header, sizeof(message), 0, &outArgs, sizeof(LwU64) * LIBOS_PARTITION_SWITCH_ARG_COUNT, 0, LibosTimeoutInfinite, 0);
}

#else


#endif


#if LIBOS_FEATURE_SHUTDOWN
void LibosProcessorShutdown(void)
{
    register LwU64 t0 __asm__("t0") = LIBOS_SYSCALL_SHUTDOWN;
    register LwU64 a0 __asm__("a0");
    __asm__ __volatile__(LIBOS_SYSCALL_INSTRUCTION : "=r"(a0) : "r"(t0) : "memory");
}

#if !LIBOS_TINY
LibosStatus LibosPartitionExit(void)
{
    register LwU64 t0 __asm__("t0") = LIBOS_SYSCALL_PARTITION_EXIT;
    register LwU64 a0 __asm__("a0");
    __asm__ __volatile__(LIBOS_SYSCALL_INSTRUCTION : "=r"(a0) : "r"(t0) : "memory");
    return a0;
}
#endif // !LIBOS_TINY
#endif

#if LIBOS_TINY
LwU32 LibosInitDaemon()
{
    DaemonMessage message;
    LwU64 messageSize = 0;
    while(1)
    {
        if (LibosPortSyncRecv(ID(libosDaemonPort), &message.header, sizeof(message), &messageSize, LibosTimeoutInfinite, 0) != LibosOk)
            return 1;

#   if LIBOS_LWRISCV >= LIBOS_LWRISCV_2_0
        if (message.header.message == LIBOS_DAEMON_SWITCH_TO)
        {
            LwU64 inArgs[LIBOS_PARTITION_SWITCH_ARG_COUNT] =
            {
                message.switchTo.params[0],
                message.switchTo.params[1],
                message.switchTo.params[2],
                message.switchTo.params[3],
                message.switchTo.params[4]
            };
            LwU64 outArgs[LIBOS_PARTITION_SWITCH_ARG_COUNT];
            LibosCriticalSectionEnter();
            {
                LibosPartitionSwitchInternal(
                message.switchTo.partitionId,
                inArgs,
                outArgs
                );
                LibosInitHookMpu();
            }
            LibosCriticalSectionLeave();
            LibosPortSyncReply(&outArgs, sizeof(outArgs), 0, 0);
        }
#else
        if (message.header.message == LIBOS_DAEMON_SUSPEND_PROCESSOR)
        {
            LibosCriticalSectionEnter();
            {
                LibosProcessorSuspendFromInit();
                LibosInitHookMpu();
            }
            LibosCriticalSectionLeave();
            LibosPortSyncReply(0, 0, 0, 0);
        }
#endif
  }
  return 0;
}
#endif


__attribute__((used)) int main();
__attribute__((used,aligned(16))) static LwU8 stack[16384];

__attribute__((used,naked)) int _start()
{
    __asm __volatile__(
        "la   ra, LibosTaskExit;"
        "la   sp, stack;"
        "li   t0, %[STACK_SIZE];"
        "add  sp, sp, t0;"
        "j    main;"
        
        : : [STACK_SIZE] "i"(sizeof(stack))
        : "memory");
}

LibosStatus LibosInterruptBind(LibosPortHandle designatedPort)
{
    register LwU64 t0 __asm__("t0") = LIBOS_SYSCALL_INTERRUPT_BIND;
    register LwU64 a0 __asm__("a0") = designatedPort;
    __asm__ __volatile__(LIBOS_SYSCALL_INSTRUCTION : "+r"(a0) : "r"(t0) : "memory");
    return (LibosStatus)a0;
}
