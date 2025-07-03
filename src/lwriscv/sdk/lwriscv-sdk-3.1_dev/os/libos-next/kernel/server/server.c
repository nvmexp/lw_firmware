#include "kernel.h"
#include "libos.h"
#include "sched/port.h"
#include "task.h"
#include "scheduler.h"
#include "spinlock.h"
#include "scheduler.h"
#include "mm/memoryset.h"
#include "loader.h"

Port          * kernelServerPort;
LibosPortHandle kernelServerPortHandle;
Task          * kernelServer;

void KernelMessageReleaseObjects(LibosMessageHeader * message) 
{
    for (int i = 0; i < message->count; i++)
        KernelPoolRelease(message->handles[i].object);
    message->count = 0;
}

void KernelMessageAttachObject(LibosMessageHeader * message, void * object, LwU64 grant)
{
    LwU64 index = message->count++;
    message->handles[index].object = object;
    message->handles[index].grant = grant;
}

LibosStatus KernelServerDispatch(LibosMessageHeader * message, LwU64 messageSize)
{
    switch(message->messageId)
    {
        case LibosCommandMemoryAddressSpaceCreate:
            {
                if (messageSize != sizeof(LibosMemoryAddressSpaceCreate))
                    return LibosErrorIncomplete;

                LibosMemoryAddressSpaceCreate * addressSpaceCreate = (LibosMemoryAddressSpaceCreate *)message;               
                if (addressSpaceCreate->handleCount != 0)
                    return LibosErrorArgument;

                AddressSpace * addressSpace;
                LibosStatus status = KernelAllocateAddressSpace(LW_TRUE, &addressSpace);
                
                // Return the handle
                KernelMessageReleaseObjects(message);
                KernelMessageAttachObject(message, addressSpace, LibosGrantAddressSpaceAll);
                addressSpaceCreate->status = status;

                return LibosOk;
            }

        case LibosCommandMemoryAddressSpaceMapPhysical:
            {
                if (messageSize != sizeof(LibosMemoryAddressSpaceMapPhysical))
                    return LibosErrorIncomplete;

                LibosMemoryAddressSpaceMapPhysical * mapPhysical = (LibosMemoryAddressSpaceMapPhysical *)message;               

                // Ensure the object is an address space, and that the caller has the right
                // permissions on the address space object
                if (mapPhysical->handleCount != 1)
                    return LibosErrorArgument;
                AddressSpace * addressSpace = DynamicCastAddressSpace(message->handles[0].object);
                if (!addressSpace || message->handles[0].grant != LibosGrantAddressSpaceAll) 
                    return LibosErrorFailed;

                // Create a mapping
                AddressSpaceRegion * region = KernelAddressSpaceAllocate(addressSpace, mapPhysical->virtualAddress, mapPhysical->size);
                if (region)
                    mapPhysical->status = KernelAddressSpaceMapContiguous(addressSpace, region, 0, mapPhysical->physicalAddress, mapPhysical->size, mapPhysical->flags);
                else
                    mapPhysical->status = LibosErrorOutOfMemory;

                KernelMessageReleaseObjects(message);    // Drop reference count on objects passed in
                KernelMessageAttachObject(message, region, LibosGrantRegionAll);

                return LibosOk;
            }

        
        case LibosCommandSchedulerPortCreate:
            {
                LibosSchedulerPortCreate * portCreate = (LibosSchedulerPortCreate *)message;               
                if (messageSize != sizeof(*portCreate))
                    return LibosErrorIncomplete;

                // Ensure the object is an address space, and that the caller has the right
                // permissions on the address space object
                if (portCreate->handleCount != 0)
                    return LibosErrorArgument;
        
                // Create the port
                Port * port = 0;
                portCreate->status = KernelPortAllocate(&port);

                KernelMessageReleaseObjects(message);    // Drop reference count on objects passed in
                KernelMessageAttachObject(message, port, LibosGrantPortAll);

                return LibosOk;
            }

        case LibosCommandSchedulerTaskCreate:
            {
                LibosSchedulerTaskCreate * createTask = (LibosSchedulerTaskCreate *)message;               
                if (messageSize != sizeof(*createTask))
                    return LibosErrorIncomplete;

                // Ensure the object is an address space, and that the caller has the right
                // permissions on the address space object
                if (createTask->handleCount != 2)
                    return LibosErrorArgument;

                AddressSpace * addressSpace = DynamicCastAddressSpace(message->handles[0].object);
                if (!addressSpace || message->handles[0].grant != LibosGrantAddressSpaceAll) 
                    return LibosErrorFailed;

                MemorySet * memorySet = DynamicCastMemorySet(message->handles[1].object);
                if (!memorySet || message->handles[1].grant != LibosGrantMemorySetAll) 
                    return LibosErrorFailed;

                // Create the task
                Task * task = 0;
                createTask->status = KernelTaskCreate(createTask->priority, addressSpace, memorySet,  createTask->entryPoint, LW_TRUE /* waiting on self */, &task);

                KernelMessageReleaseObjects(message);    // Drop reference count on objects passed in
                KernelMessageAttachObject(message, task, LibosGrantTaskAll);

                return LibosOk;
            }

        case LibosCommandLoaderElfOpen:
            {
                LibosLoaderElfOpen * elfOpen = (LibosLoaderElfOpen *)message;               
                if (messageSize != sizeof(*elfOpen))
                    return LibosErrorIncomplete;

                // Ensure the object is an address space, and that the caller has the right
                // permissions on the address space object
                if (elfOpen->handleCount != 0)
                    return LibosErrorArgument;
        
                // Open the ELF
                Elf * elf = 0;
                elfOpen->status = KernelElfOpen(&elfOpen->filename[0], &elf);

                KernelMessageReleaseObjects(message);    // Drop reference count on objects passed in
                KernelMessageAttachObject(message, elf, LibosGrantElfAll);

                return LibosOk;
            }        
        
        case LibosCommandLoaderElfMap:
            {
                LibosLoaderElfMap * elfMap = (LibosLoaderElfMap *)message;               
                if (messageSize != sizeof(*elfMap))
                    return LibosErrorIncomplete;

                // Ensure the object is an address space, and that the caller has the right
                // permissions on the address space object
                if (elfMap->handleCount != 2)
                    return LibosErrorArgument;
        
                AddressSpace * addressSpace = DynamicCastAddressSpace(message->handles[0].object);
                if (!addressSpace || message->handles[0].grant != LibosGrantAddressSpaceAll) 
                    return LibosErrorFailed;

                Elf * elf = DynamicCastElf(message->handles[1].object);
                if (!elf || message->handles[1].grant != LibosGrantElfAll) 
                    return LibosErrorFailed;

                // Map the elf
                LibosElf64Header *elfHdr = LibosBootFindElfHeader(rootFs, elf->elf);
                
                // Return entry point to user-space
                if (elfHdr) 
                    elfMap->entryPoint = elfHdr->entry;
                else
                    elfMap->entryPoint = 0;

                elfMap->status = KernelElfMap(addressSpace, elf->elf);

                KernelMessageReleaseObjects(message);

                return LibosOk;
            }
    }
         
    return LibosErrorArgument;
}

/*
    Core issue:
        Kernel Daemons cannot call the user-mode ABI
        Options.
            1. Define an in kernel alternate to ecall
            2. Switch to using ebreak for SBI
            3. Don't run the kernel server in S-mode

*/
void KernelServerEntry()
{
    KernelPrintf("Kernel daemon started.\n");

    __attribute__((aligned(8))) LwU8 messageBuffer[1024];

    while (LW_TRUE) {
        LwU64 messageSize;
        LibosStatus status = LibosPortSyncRecv(
            kernelServerPortHandle, &messageBuffer, sizeof(messageBuffer), &messageSize, LibosTimeoutInfinite, LibosPortFlagTransferObjects);
        
        // @todo: Document kernel will not transit a message with TransferObjects whose headers is malformed.
        if (status != LibosOk)
            continue;

        LibosMessageHeader * message = (LibosMessageHeader *)&messageBuffer[0];

        /**
         * Handle passing is kind of slow.
         *   Ideas:
         *      - Handle area in message.  Register contains the handle count
         *        coming in and coming out. 
         */

        // @todo: Fine grain locking. Right now even the MM pool is covered by the scheduler 
        //        spinlock.
        KernelSpinlockAcquire(&KernelSchedulerLock);
        status = KernelServerDispatch((LibosMessageHeader *)&messageBuffer[0], messageSize);
        KernelSpinlockRelease(&KernelSchedulerLock);

        if (status == LibosOk)
        {
            // Attach the new address space object
            LibosPortSyncReply(message, messageSize, 0, LibosPortFlagTransferObjects);
        }
        else /*if (status != LibosOk)*/
        {
            KernelMessageReleaseObjects(message);
            LibosPortSyncReply(0, 0, 0, 0);
        }
    }
}

void KernelInitServer()
{
    // Allocate the port
    KernelPanilwnless(LibosOk == KernelPortAllocate(&kernelServerPort));
}

void KernelServerRun()
{
    // Create the task
    KernelPanilwnless(LibosOk == 
        KernelTaskCreate(LIBOS_PRIORITY_NORMAL, kernelAddressSpace, KernelPoolMemorySet, 
                        (LwU64)KernelServerEntry, LW_FALSE /* not waiting on port */, &kernelServer));

    // Register the port
    KernelPanilwnless(LibosOk == 
        KernelTaskRegisterObject(kernelServer, &kernelServerPortHandle, kernelServerPort, LibosGrantPortAll, 0));

    kernelServer->priority = 0;

    // Place the server on the run-list
    KernelSpinlockAcquire(&KernelSchedulerLock);
    KernelSchedulerRun(kernelServer);
    KernelSpinlockRelease(&KernelSchedulerLock);
}