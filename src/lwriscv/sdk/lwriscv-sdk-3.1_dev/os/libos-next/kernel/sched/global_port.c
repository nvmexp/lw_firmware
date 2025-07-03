/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

#include "sched/global_port.h"
#include "libos.h"
#include "kernel.h"
#include "mm/objectpool.h"
#include "rootfs.h"
#include "task.h"
#include "scheduler.h"
#include "libriscv.h"
#include "librbtree.h"
#include "diagnostics.h"

LwU8 selfPartitionId;
LwU64 imagePhysical;


LibosTreeHeader globalPortMap;
OWNED_PTR Port *KernelPartitionParentPort = 0;

LIBOS_SECTION_TEXT_COLD void KernelInitPartition(LwU64 globalPageStart, LwU64 globalPageSize)
{
    KernelPanilwnless(LibosOk==KernelAddressSpaceMapContiguous(
        kernelAddressSpace, 
        kernelGlobalPageMapping,
        0,
        globalPageStart, 
        globalPageSize, 
        LibosMemoryReadable | LibosMemoryWriteable)); 

    // Save partitionId to a global variable
    selfPartitionId = KernelGlobalPage()->args.libosInit.partitionId;
    KernelTrace("Libos: Our partition ID is %u.\n", selfPartitionId);

    // Set ourselves as initialized in the global page
    KernelGlobalPage()->initializedPartitionMask |= 1ULL << selfPartitionId;

    // Initialize the Partition object pool
    KernelPoolConstruct(sizeof(Partition), LIBOS_POOL_PARTITION, 0);

    // Initialize the map of global port map 
    LibosTreeConstruct(&globalPortMap);

    // Setup parent port
    LwU32 parentPortId = KernelGlobalPage()->args.libosInit.parentGlobalPort;
    if (parentPortId != LWOS_ROOTP_GLOBAL_PORT_NONE)
    {
        KernelTrace("Libos: Our parent passed a global port: %08x.\n", parentPortId);
        KernelPanilwnless(KernelPortAllocate(&KernelPartitionParentPort) == LibosOk);
        KernelPartitionParentPort->globalPort = parentPortId;
        KernelPoolAddref(KernelPartitionParentPort);
        KernelGlobalPortMapInsert(KernelPartitionParentPort);
    }
    else
        KernelTrace("Libos: Our parent didn't pass a global port.\n");
}

LIBOS_SECTION_IMEM_PINNED struct GlobalPage *KernelGlobalPage()
{
    return (struct GlobalPage *) LIBOS_CONFIG_GLOBAL_PAGE_MAPPING_BASE;
}

LIBOS_SECTION_TEXT_COLD void KernelPartitionInitialize(Partition *partition, LwU8 partitionId)
{
    // We inherit from port
    KernelPortConstruct(&partition->port);

    partition->partitionId = partitionId;
}

__attribute__((used)) LIBOS_SECTION_TEXT_COLD LibosStatus KernelSyscallPartitionCreate(LwU8 partitionId)
{
    Task *self = KernelSchedulerGetTask();

    if ((KernelGlobalPage()->initializedPartitionMask & (1ULL << partitionId)) != 0)
    {
        // There is already a partition with the same ID
        return LibosErrorAccess;
    }

    Partition *partition;
    partition = (Partition *) KernelPoolAllocate(LIBOS_POOL_PARTITION);
    
    if (!partition)
    {
        return LibosErrorOutOfMemory;
    }

    KernelPartitionInitialize(partition, partitionId);

    LibosHandle outPartition;
    LibosStatus status = KernelTaskRegisterObject(self, &outPartition, partition, LibosGrantPartitionAll, 0);
    if (status != LibosOk)
    {
        return status;
    }

    KernelPoolRelease(partition);

    self->state.registers[RISCV_T0] = outPartition;
    return LibosOk;
}

__attribute__((used)) LIBOS_SECTION_TEXT_COLD LibosStatus KernelSyscallPartitionMemoryGrantPhysical(LibosPartitionHandle handle, LwU64 base, LwU64 size)
{
    OWNED_PTR Partition *partition = KernelObjectResolve(KernelSchedulerGetTask(), (LwU64) handle, Partition, LibosGrantPartitionAll);
    if (!partition)
    {
        return LibosErrorAccess;
    }

    LibosStatus status = LwosRootPartitionMapPhysical(partition->partitionId, base, size, PMP_ACCESS_MODE_READ | PMP_ACCESS_MODE_WRITE | PMP_ACCESS_MODE_EXELWTE);
    KernelPoolRelease(partition);
    return status;
}

__attribute__((used)) LIBOS_SECTION_TEXT_COLD LibosStatus KernelSyscallPartitionMemoryAllocate(LibosPartitionHandle handle, LwU64 size, LwU64 alignment)
{
    Task *self = KernelSchedulerGetTask();

    OWNED_PTR Partition *partition = KernelObjectResolve(self, (LwU64) handle, Partition, LibosGrantPartitionAll);
    if (!partition)
    {
        return LibosErrorAccess;
    }

    LwU64 outAddress;
    LibosStatus status = LwosRootMemoryAllocate(partition->partitionId, &outAddress, size, alignment, PMP_ACCESS_MODE_READ | PMP_ACCESS_MODE_WRITE | PMP_ACCESS_MODE_EXELWTE);
    KernelPoolRelease(partition);

    if (status != LibosOk)
    {
        return status;
    }

    self->state.registers[RISCV_T0] = outAddress;
    return LibosOk;
}

__attribute__((used)) LIBOS_SECTION_TEXT_COLD LibosStatus KernelSyscallPartitionSpawn(LibosPartitionHandle handle, LwU64 memoryStart, LwU64 memorySize)
{
    OWNED_PTR Partition *partition = KernelObjectResolve(KernelSchedulerGetTask(), (LwU64) handle, Partition, LibosGrantPartitionAll);
    if (!partition)
    {
        return LibosErrorAccess;
    }

    if (KernelGlobalPage()->initializedPartitionMask & (1ULL << partition->partitionId))
    {
        return LibosErrorAccess;
    }

    if (partition->port.globalPort == LWOS_ROOTP_GLOBAL_PORT_NONE)
    {
        LibosStatus status = LwosRootGlobalPortAllocate(&partition->port.globalPort);
        if (status != LibosOk)
        {
            return status;
        }
        KernelGlobalPortMapInsert(&partition->port);
    }

    LibosStatus status = LwosRootGlobalPortGrant(partition->port.globalPort, partition->partitionId, LWOS_ROOTP_GLOBAL_PORT_GRANT_RECV);
    if (status != LibosOk)
    {
        return status;
    }

    status = LwosRootPartitionInit(partition->partitionId, imagePhysical, memoryStart, memorySize, partition->port.globalPort);
    KernelPoolRelease(partition);
    return status;
}

void KernelGlobalPortMapInsert(Port *port)
{
    KernelAssert(port);
    KernelAssert(port->globalPort != LWOS_ROOTP_GLOBAL_PORT_NONE);

    LibosTreeHeader **slot = &globalPortMap.parent;
    port->globalPortMapNode.parent = port->globalPortMapNode.left = port->globalPortMapNode.right = &globalPortMap;
    while (!(*slot)->isNil)
    {
        port->globalPortMapNode.parent = *slot;
        if (port->globalPort < CONTAINER_OF(*slot, Port, globalPortMapNode)->globalPort)
        {
            slot = &(*slot)->left;
        }
        else
        {
            slot = &(*slot)->right;
        }
    }
    *slot = &port->globalPortMapNode;
    LibosTreeInsertFixup(&port->globalPortMapNode, 0);
}

Port *KernelGlobalPortMapFind(LwU32 globalId)
{
    LibosTreeHeader *i = globalPortMap.parent;
    while (!i->isNil)
    {
        if (globalId < CONTAINER_OF(i, Port, globalPortMapNode)->globalPort)
        {
            i = i->left;
        }
        else if (globalId > CONTAINER_OF(i, Port, globalPortMapNode)->globalPort)
        {
            i = i->right;
        }
        else
        {
            return CONTAINER_OF(i, Port, globalPortMapNode);
        }
    }
    return NULL;
}

// Find a global port with owner `owner`
Port *KernelGlobalPortMapFindOwner(LwU8 owner)
{
    LibosTreeHeader *i = globalPortMap.parent;
    while (!i->isNil)
    {
        LwU8 lwrOwner = LWOS_ROOTP_GLOBAL_PORT_OWNER(CONTAINER_OF(i, Port, globalPortMapNode)->globalPort);

        if (owner < lwrOwner)
        {
            i = i->left;
        }
        else if (owner > lwrOwner)
        {
            i = i->right;
        }
        else
        {
            return CONTAINER_OF(i, Port, globalPortMapNode);
        }
    }
    return NULL;
}

void KernelGlobalPortMapRemove(Port *port)
{
    LibosTreeUnlink(&port->globalPortMapNode, 0);
}

static LibosStatus KernelHandleGlobalMessageComplete
(
    LibosStatus status,
    Shuttle *shuttle
)
{
    if (status != LibosOk)
    {
        shuttle->payloadSize = 0;
        shuttle->errorCode = status;
    }
    ShuttleRetireSingle(shuttle);
    return status;
}

/*
 * This function is called when another partition requests us to perform a send
 * with verb LWOS_VERB_LIBOS_GLOBAL_MESSAGE_PERFORM_SEND. It basically follows
 * the same logic of PortSendGlobal.
 */
static LibosStatus KernelHandleGlobalMessagePerformSendInternal
(
    LwU32 globalId,
    LwU32 size,
    LwU8 callingPartition
)
{
    KernelTrace("KernelHandleGlobalMessagePerformSendInternal\n");
    if ((KernelGlobalPage()->globalPortGrants[LWOS_ROOTP_GLOBAL_PORT_SEQ(globalId)].partitionsGrantRecv & (1ULL << callingPartition)) == 0)
    {
        return LibosErrorAccess;
    }

    Port *sendPort = KernelGlobalPortMapFind(globalId);
    if (!sendPort || !PortHasSender(sendPort))
        return LibosErrorAccess;

    Shuttle *sendShuttle = PortDequeSender(sendPort);

    if (sendShuttle->payloadSize > size)
    {
        return KernelHandleGlobalMessageComplete(
            LibosErrorIncomplete,
            sendShuttle
        );
    }

    Shuttle recvShuttle = {
        .payloadAddress = (LwU64) KernelGlobalPage()->args.globalMessage.message,
        .copyFlags = 
            sendShuttle->copyFlags & (LIBOS_SHUTTLE_COPY_FLAG_MAP_HANDLES | LIBOS_SHUTTLE_COPY_FLAG_MAP_OBJECTS)
                ?
                    LIBOS_SHUTTLE_COPY_FLAG_KERNEL_BUFFER | LIBOS_SHUTTLE_COPY_FLAG_MAP_GLOBAL_PORT_ID
                : 
                    LIBOS_SHUTTLE_COPY_FLAG_KERNEL_BUFFER,
        .targetPartition = callingPartition
    };

    KernelGlobalPage()->args.globalMessage.toGlobalPort = globalId;
    KernelGlobalPage()->args.globalMessage.size = sendShuttle->payloadSize;
    LibosStatus status = KernelCopyMessage(&recvShuttle, sendShuttle, sendShuttle->payloadSize);
    if (status != LibosOk)
    {
        return KernelHandleGlobalMessageComplete(
            LibosErrorIncomplete,
            sendShuttle
        );
    }

    return KernelHandleGlobalMessageComplete(
        LibosOk,
        sendShuttle
    );
}

/*
 * This function is called when another partition requests us to perform a
 * receive with verb LWOS_VERB_LIBOS_GLOBAL_MESSAGE_PERFORM_RECV. It basically
 * follows the same logic of PortRecvGlobal.
 */
static LibosStatus KernelHandleGlobalMessagePerformRecvInternal(LwU8 callingPartition)
{
    KernelTrace("KernelHandleGlobalMessagePerformRecvInternal\n");
    LwU32 globalId = KernelGlobalPage()->args.globalMessage.toGlobalPort;

    if ((KernelGlobalPage()->globalPortGrants[LWOS_ROOTP_GLOBAL_PORT_SEQ(globalId)].partitionsGrantSend & (1ULL << callingPartition)) == 0)
    {
        return LibosErrorAccess;
    }

    Port *recvPort = KernelGlobalPortMapFind(globalId);
    if (!recvPort || !PortHasReceiver(recvPort))  
        return LibosErrorAccess;

    Shuttle *recvShuttle = PortDequeReceiver(recvPort);

    if (KernelGlobalPage()->args.globalMessage.size > recvShuttle->payloadSize)
    {
        return KernelHandleGlobalMessageComplete(
            LibosErrorIncomplete,
            recvShuttle
        );
    }

    Shuttle sendShuttle = {
        .payloadAddress = (LwU64) KernelGlobalPage()->args.globalMessage.message,
        .copyFlags = 
            recvShuttle->copyFlags & (LIBOS_SHUTTLE_COPY_FLAG_MAP_HANDLES | LIBOS_SHUTTLE_COPY_FLAG_MAP_OBJECTS)
                ?
                    LIBOS_SHUTTLE_COPY_FLAG_KERNEL_BUFFER | LIBOS_SHUTTLE_COPY_FLAG_MAP_GLOBAL_PORT_ID
                : 
                    LIBOS_SHUTTLE_COPY_FLAG_KERNEL_BUFFER,
        .targetPartition = callingPartition
    };

    LibosStatus status = KernelCopyMessage(recvShuttle, &sendShuttle, KernelGlobalPage()->args.globalMessage.size);
    if (status != LibosOk)
    {
        return KernelHandleGlobalMessageComplete(
            LibosErrorIncomplete,
            recvShuttle
        );
    }

    return KernelHandleGlobalMessageComplete(
        LibosOk,
        recvShuttle
    );
}

void LIBOS_NORETURN KernelHandleGlobalMessagePerformSend
(
    LwU32 globalId,
    LwU32 size,
    LwU8 callingPartition
)
{
    SeparationKernelPartitionSwitch(LWOS_A0(LWOS_VERB_RETURN,
        KernelHandleGlobalMessagePerformSendInternal(globalId, size, callingPartition)), 0, 0, 0, 0, callingPartition);
}

void LIBOS_NORETURN KernelHandleGlobalMessagePerformRecv(LwU8 callingPartition)
{
    SeparationKernelPartitionSwitch(LWOS_A0(LWOS_VERB_RETURN,
        KernelHandleGlobalMessagePerformRecvInternal(callingPartition)), 0, 0, 0, 0, callingPartition);
}

void KernelUpdateGlobalPortSend(Port *port)
{
    KernelTrace("KernelUpdateGlobalPortSend(%p)\n", port);
    if (port->globalPort == LWOS_ROOTP_GLOBAL_PORT_NONE)
        return;

    LwU64 *pendingSend = &KernelGlobalPage()->globalPortStatuses[LWOS_ROOTP_GLOBAL_PORT_SEQ(port->globalPort)].partitionsPendingSend;
    LwU64 selfMask = 1ULL << selfPartitionId;

    if (PortHasSender(port))
        *pendingSend |= selfMask;
    else
        *pendingSend &= ~selfMask;
}


void KernelUpdateGlobalPortRecv(Port *port)
{
    KernelTrace("KernelUpdateGlobalPortRecv(%p)\n", port);
    if (port->globalPort == LWOS_ROOTP_GLOBAL_PORT_NONE)
        return;

    LwU64 *pendingRecv = &KernelGlobalPage()->globalPortStatuses[LWOS_ROOTP_GLOBAL_PORT_SEQ(port->globalPort)].partitionsPendingRecv;

    LwU64 selfMask = 1ULL << selfPartitionId;

    if (PortHasReceiver(port))
        *pendingRecv |= selfMask;
    else
        *pendingRecv &= ~selfMask;
}

__attribute__((used)) LibosStatus KernelSyscallPartitionExit()
{
    if (KernelSchedulerGetTask() != TaskInit)
    {
        return LibosErrorAccess;
    }
    LwosRootPartitionExit();
}

void KernelHandlePartitionExit(LwU8 partitionId)
{
    Port *p;
    while ((p = KernelGlobalPortMapFindOwner(partitionId)) != NULL)
    {
        KernelTrace("Libos: Broadcasting loss of remote port %08x owned by partition %d.\n", p->globalPort, partitionId);
        KernelGlobalPortMapRemove(p);
        p->globalPortLost = LW_TRUE;

        while (PortHasReceiver(p))
        {
            Shuttle *recver = PortDequeReceiver(p);
            recver->errorCode = LibosErrorPortLost;
            recver->payloadSize = 0;
            ShuttleRetireSingle(recver);
        }
        while (PortHasSender(p))
        {
            Shuttle *sender = PortDequeSender(p);
            sender->errorCode = LibosErrorPortLost;
            sender->payloadSize = 0;
            ShuttleRetireSingle(sender);
        }
    }
}
