/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

#ifndef LIBOS_IPC_H
#define LIBOS_IPC_H

#include <lwtypes.h>
#include "sched/port.h"
#include "../root/root.h"

struct Partition
{
    Port port;
    LwU8 partitionId;
};

extern LwU8 selfPartitionId;
extern LwU64 imagePhysical;

void KernelInitPartition(LwU64 globalPageStart, LwU64 globalPageSize);
struct GlobalPage *KernelGlobalPage(void);

void KernelGlobalPortMapInsert(Port *port);
Port *KernelGlobalPortMapFind(LwU32 globalId);
void KernelGlobalPortMapRemove(Port *port);

void LIBOS_NORETURN KernelHandleGlobalMessagePerformSend(LwU32 globalId, LwU32 size, LwU8 callingPartition);
void LIBOS_NORETURN KernelHandleGlobalMessagePerformRecv(LwU8 callingPartition);

void KernelUpdateGlobalPortSend(Port *port);
void KernelUpdateGlobalPortRecv(Port *port);

void KernelHandlePartitionExit(LwU8 partitionId);

extern Port *KernelPartitionParentPort;

#endif // LIBOS_IPC_H
