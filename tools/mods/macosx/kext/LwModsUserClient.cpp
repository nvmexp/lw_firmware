/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2010-2011,2014 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

// Mac MODS kernel extension user client

// TODO:
// Check non-static methods
// check that all included headers are necessary
// header has variable names in method prototypes
// debug parameter to change output at runtime
// parameter to switch between IOLog and kprintf

// TODO: mach_test helper function
// device mem mapping ignoring exeutable bit?  can we have non-readable and writable?

#define KLOG kprintf
//#define KLOG IOLog

#include <IOKit/pci/IOPCIDevice.h>
#include <IOKit/IOUserClient.h>
#include <IOKit/IOBufferMemoryDescriptor.h>
#include <IOKit/IOLib.h>
#include <IOKit/IOPlatformExpert.h>
#include <architecture/i386/pio.h>

#include "LwModsUserClient.h"
#include "LwModsLog.h"

#define super IOUserClient

#define LW_PMC_INTR_0 0x100
#define LW_PMC_INTR_EN_0 0x140

#define INTERRUPT_QUEUE_SIZE 64

OSDefineMetaClassAndStructors(InterruptController, OSObject);

InterruptController* InterruptController::interruptController
(
    IOPCIDevice *iopci,
    uint32_t type,
    uint32_t index,
    IODataQueue *queue
)
{
    InterruptController *controller;

    int intr_type;
    int intr_index = 0;
    while (1) {
        if (iopci->getInterruptType(intr_index, &intr_type) != kIOReturnSuccess)
        {
            SLOG_ERR("failed to find interrupt");
            return NULL;
        }

        if (intr_type & kIOInterruptTypeLevel && type == kLwModsInterruptTypeInt)
            break;

        if (intr_type & kIOInterruptTypePCIMessaged && type == kLwModsInterruptTypeMsi)
            break;

        intr_index++;
    }

    controller = new InterruptController;
    if (!controller)
    {
        SLOG_ERR("failed to allocate InterruptController");
        return NULL;
    }

    controller->bar0_map = 0;
    controller->source = 0;
    controller->index = index;
    controller->queue = queue;

    // MSI interrupts are not shared and they don't need filter function
    if (type == kLwModsInterruptTypeMsi)
    {
        controller->source = IOInterruptEventSource::interruptEventSource(
            controller,
            (IOInterruptEventAction) OSMemberFunctionCast(IOInterruptEventAction, controller, &InterruptController::handle),
            iopci,
            intr_index
        );
    }
    else
    {
        controller->source = IOFilterInterruptEventSource::filterInterruptEventSource(
            controller,
            (IOInterruptEventAction) OSMemberFunctionCast(IOInterruptEventAction, controller, &InterruptController::handle),
            (IOFilterInterruptAction) OSMemberFunctionCast(IOFilterInterruptAction, controller, &InterruptController::filter),
            iopci,
            intr_index
        );

        // map BAR0 to control interrupts registers
        controller->bar0_map = iopci->mapDeviceMemoryWithRegister(kIOPCIConfigBaseAddress0);
        if (!controller->bar0_map)
        {
            SLOG_ERR("failed to map bar0");
            controller->release();
            return NULL;
        }

        controller->intr_state = (uint32_t*)(controller->bar0_map->getAddress() + LW_PMC_INTR_0);
        controller->intr_enabled = (uint32_t*)(controller->bar0_map->getAddress() + LW_PMC_INTR_EN_0);
    }

    if (!controller->source)
    {
        SLOG_ERR("failed to create event source");
        controller->release();
        return NULL;
    }

    return controller;
}

void InterruptController::free()
{
    if (source)
        source->release();

    if (bar0_map)
        bar0_map->release();

    OSObject::free();
}

void InterruptController::handle(IOInterruptEventSource *source, int count)
{
    LwModsInterrupt interrupt;
    interrupt.index = index;

    if (!queue->enqueue(&interrupt, sizeof(interrupt)))
    {
        KLOG("%s: WARNING: queue is full, interrupt thread may not be receiving interrupts\n", __FUNCTION__);
    }
}

bool InterruptController::filter(IOFilterInterruptEventSource *source)
{
    // if this is our interrupt, clear registers and schedule interrupt for handling
    if (*intr_enabled && *intr_state)
    {
        *intr_enabled = 0;
        return true;
    }
    return false;
}

// Must not user "super" here
OSDefineMetaClassAndStructors(com_lwidia_driver_LwModsUserClient, IOUserClient);

// initWithTask is called as a result of the user process calling IOServiceOpen.
// The default implementation simply calls the IOService init method, ignoring the parameters.
bool LwModsUserClient::initWithTask
(
    task_t owningTask,
    void* selwrityToken,
    UInt32 type
)
{
    bool success;

    success = super::initWithTask(owningTask, selwrityToken, type);

    // Must fullow call to super::initWithTask or getName will not work
    LOG_ENT();

    if (!success)
    {
        LOG_ERR_EXT("failure in super::%s", __FUNCTION__);
        return false;
    }

    lock = IOLockAlloc();
    if (NULL == lock)
    {
        LOG_ERR_EXT("failure in IOLockAlloc");
        return false;
    }

    IOLockLock(lock);
    LIST_INIT(&MemoryAllocations);
    LIST_INIT(&MemoryMappings);
    IOLockUnlock(lock);

    LIST_INIT(&registeredInterrupts);

    LOG_EXT();
    return true;
}

bool LwModsUserClient::start
(
    IOService *provider
)
{
    LOG_ENT();

    lwdrv = OSDynamicCast(LwModsDriver, provider);
    if (!lwdrv)
    {
        LOG_ERR_EXT("provider is not of type LwModsDriver");
        return false;
    }

    if (!super::start(provider))
    {
        LOG_ERR_EXT("failure in %s::%s", super::getName(), __FUNCTION__);
        return false;
    }

    if (!provider->open(this)) {
        LOG_ERR_EXT("driver is busy");
        return false;
    }

    LOG_EXT();

    return true;
}

void LwModsUserClient::stop
(
    IOService *provider
)
{
    LOG_ENT();

    IOLockLock(lock);

    // Free mappings
    while (!LIST_EMPTY(&MemoryMappings))
    {
        Mapping *map = LIST_FIRST(&MemoryMappings);
        SLOG_ERR("memory still mapped at %p", (void *)map->iomm->getAddress());
        UnregisterMapping(map);
    }

    // Free allocations
    while (!LIST_EMPTY(&MemoryAllocations))
    {
        Allocation *alloc = LIST_FIRST(&MemoryAllocations);
        SLOG_ERR("memory still allocated (%p)", alloc);
        if (alloc->RefCount > 1)
        {
            SLOG_ERR("memory allocation (%p) still has %d references", alloc, alloc->RefCount - 1);
        }
        UnregisterAllocation(alloc);
    }

    IOLockUnlock(lock);
    IOLockFree(lock);

    while (!LIST_EMPTY(&registeredInterrupts))
    {
        InterruptController *interrupt = LIST_FIRST(&registeredInterrupts);

        SLOG_ERR("interrupt still registered!");

        LIST_REMOVE(interrupt, list);
        getWorkLoop()->removeEventSource(interrupt->source);
        interrupt->release();
    }

    if (interruptQueue)
    {
        SLOG_ERR("interrupt queue not released!");
        interruptQueue->release();
        interruptQueueMapping->release();
    }

    super::stop(provider);

    provider->close(this);

    LOG_EXT();
}

IOReturn LwModsUserClient::clientClose(void)
{
    LOG_ENT();

    if (lwdrv)
    {
        // TODO: Is this right?
        stop(lwdrv);
        detach(lwdrv);
        lwdrv = NULL;
    }

    LOG_EXT();
    // super::clientClose always returns error. Do not call it!
    return kIOReturnSuccess;
}

LwModsUserClient::Allocation *LwModsUserClient::RegisterAllocation
(
    AllocationList *list,
    IOMemoryDescriptor *iomd,
    uint32_t Attrib
)
{
    Allocation *alloc = new Allocation;

    if (!alloc)
    {
        SLOG_ERR("failed to allocate memory");
        return 0;
    }

    bzero(alloc, sizeof(*alloc));

    alloc->iomd = iomd;
    alloc->Attrib = Attrib;
    alloc->RefCount = 0;

    LIST_INSERT_HEAD(list, alloc, List);

    return alloc;
}

void LwModsUserClient::UnregisterAllocation(Allocation *alloc)
{
    alloc->RefCount--;
    if (alloc->RefCount == 0)
    {
        if (alloc->iomd->complete() != kIOReturnSuccess)
            KLOG("%s: WARNING: complete failed\n", __FUNCTION__);

        alloc->iomd->release();
        LIST_REMOVE(alloc, List);
        delete alloc;
    }
}

LwModsUserClient::Mapping *LwModsUserClient::RegisterMapping
(
    MappingList *list,
    Allocation *alloc,
    IOMemoryMap *iomm
)
{
    Mapping *map = new Mapping;

    if (!map)
    {
        SLOG_ERR("failed to allocate memory");
        return 0;
    }

    bzero(map, sizeof(*map));

    alloc->RefCount++;
    map->iomm = iomm;
    map->alloc = alloc;

    LIST_INSERT_HEAD(list, map, List);

    return map;
}

void LwModsUserClient::UnregisterMapping(Mapping *map)
{
    map->iomm->release();
    UnregisterAllocation(map->alloc);
    LIST_REMOVE(map, List);
    delete map;
}

IOPCIDevice *LwModsUserClient::getPciFromPath(const char *devPath)
{
    IORegistryEntry *iore = IORegistryEntry::fromPath(devPath);
    if (NULL == iore)
    {
        SLOG_ERR("failure in IORegistryEntry::fromPath(%s)", devPath);
        return NULL;
    }

    IOPCIDevice *iopci = OSDynamicCast(IOPCIDevice, iore);
    if (NULL == iopci)
    {
        SLOG_ERR("IORegistryEntry at \"%s\" is not an IOPCIDevice", devPath);
        return NULL;
    }

    return iopci;
}

IOOptionBits LwModsUserClient::getCaching(uint32_t attrib)
{
    switch (attrib)
    {
    case kLwModsMemoryCached:
        return kIOMapCopybackCache;
    case kLwModsMemoryUncached:
        return kIOMapInhibitCache;
    case kLwModsMemoryWriteCombine:
        return kIOMapWriteCombineCache;
    case kLwModsMemoryWriteThrough:
        return kIOMapWriteThruCache;
    case kLwModsMemoryWriteProtect:
        return kIOMapReadOnly;
    default:
        // TODO: assert?  Return a different cache type by default?
        SLOG_ERR("unsupported memory type (%" PRIu32 ")", attrib);
        return kIOMapDefaultCache;
    }
}

bool LwModsUserClient::isValidMemoryHandle
(
    AllocationList *list,
    Allocation *mh
)
{
    // lock should be held before entering
    Allocation *alloc;
    LIST_FOREACH(alloc, list, List)
    {
        if (alloc == mh)
        {
            return true;
        }
    }
    return false;
}

IOReturn LwModsUserClient::getApiVersion
(
    LwModsUserClient *target,
    void *reference,
    IOExternalMethodArguments *arguments
)
{
    SLOG_ENT();
    LwModsDrvGetApiVersionOut *out = (LwModsDrvGetApiVersionOut *)arguments->structureOutput;
    out->Version = LWMODSDRIVER_VERSION;
    SLOG_EXT();
    return kIOReturnSuccess;
}

IOReturn LwModsUserClient::enterDebugger
(
    LwModsUserClient *target,
    void *reference,
    IOExternalMethodArguments *arguments
)
{
    SLOG_ENT();
    PE_enter_debugger((char *)arguments->structureInput);
    SLOG_EXT();
    return kIOReturnSuccess;
}

IOReturn LwModsUserClient::getPciInfo
(
    LwModsUserClient *target,
    void *reference,
    IOExternalMethodArguments *arguments
)
{
    LwModsDrvGetPciInfoIn *in = (LwModsDrvGetPciInfoIn *)arguments->structureInput;
    LwModsDrvGetPciInfoOut *out = (LwModsDrvGetPciInfoOut *)arguments->structureOutput;

    SLOG_ENT();

    IOPCIDevice *iopci = getPciFromPath(in->Path);
    if (!iopci)
    {
        SLOG_ERR_EXT("invalid Path");
        return kIOReturnBadArgument;
    }

    out->BusNum = iopci->getBusNumber();
    out->DevNum = iopci->getDeviceNumber();
    out->FuncNum = iopci->getFunctionNumber();
    out->DeviceId = iopci->extendedConfigRead16(PCI_CONFIG_ADDR_DEVICE_ID);
    out->VendorId = iopci->extendedConfigRead16(PCI_CONFIG_ADDR_VENDOR_ID);
    out->ClassCode = iopci->extendedConfigRead32(PCI_CONFIG_ADDR_CLASS_CODE) >> PCI_CONFIG_SHIFT_CLASS_CODE;

    iopci->release();

    SLOG_EXT();

    return kIOReturnSuccess;
}

IOReturn LwModsUserClient::pciConfigRead
(
    LwModsUserClient *target,
    void *reference,
    IOExternalMethodArguments *arguments
)
{
    LwModsDrvPciConfigReadIn *in = (LwModsDrvPciConfigReadIn *)arguments->structureInput;
    LwModsDrvPciConfigReadOut *out = (LwModsDrvPciConfigReadOut *)arguments->structureOutput;

    SLOG_ENT();

    IOPCIDevice *iopci = getPciFromPath(in->Path);
    if (!iopci)
    {
        SLOG_ERR_EXT("invalid Path");
        return kIOReturnBadArgument;
    }

    switch (in->DataSize)
    {
    case sizeof(uint8_t):
        out->Data = iopci->extendedConfigRead8(in->Address);
        break;
    case sizeof(uint16_t):
        out->Data = iopci->extendedConfigRead16(in->Address);
        break;
    case sizeof(uint32_t):
        out->Data = iopci->extendedConfigRead32(in->Address);
        break;
    default:
        SLOG_ERR_EXT("invalid DataSize (%" PRIu32 ")", in->DataSize);
        iopci->release();
        return kIOReturnBadArgument;
    }

    iopci->release();

    SLOG_EXT();

    return kIOReturnSuccess;
}

IOReturn LwModsUserClient::pciConfigWrite
(
    LwModsUserClient *target,
    void *reference,
    IOExternalMethodArguments *arguments
)
{
    LwModsDrvPciConfigWriteIn *in = (LwModsDrvPciConfigWriteIn *)arguments->structureInput;

    SLOG_ENT();

    IOPCIDevice *iopci = getPciFromPath(in->Path);
    if (!iopci)
    {
        SLOG_ERR_EXT("invalid Path");
        return kIOReturnBadArgument;
    }

    switch (in->DataSize)
    {
    case sizeof(uint8_t):
        iopci->extendedConfigWrite8(in->Address, in->Data);
        break;
    case sizeof(uint16_t):
        iopci->extendedConfigWrite16(in->Address, in->Data);
        break;
    case sizeof(uint32_t):
        iopci->extendedConfigWrite32(in->Address, in->Data);
        break;
    default:
        SLOG_ERR_EXT("invalid DataSize(%" PRIu32 ")", in->DataSize);
        iopci->release();
        return kIOReturnBadArgument;
    }

    iopci->release();

    SLOG_EXT();

    return kIOReturnSuccess;
}

IOReturn LwModsUserClient::allocPages
(
    LwModsUserClient *target,
    void *reference,
    IOExternalMethodArguments *arguments
)
{
    IOReturn kr;
    LwModsDrvAllocPagesIn *in = (LwModsDrvAllocPagesIn *)arguments->structureInput;
    LwModsDrvAllocPagesOut *out = (LwModsDrvAllocPagesOut *)arguments->structureOutput;
    IOBufferMemoryDescriptor *mem;
    IOOptionBits options;

    SLOG_ENT();

    switch (in->Attrib)
    {
    case kLwModsMemoryCached:
    case kLwModsMemoryUncached:
    case kLwModsMemoryWriteCombine:
    case kLwModsMemoryWriteThrough:
    case kLwModsMemoryWriteProtect:
        break;
    default:
        SLOG_ERR_EXT("unsupported memory type (%" PRIu32 ")", in->Attrib);
        return kIOReturnBadArgument;
    }

    options = kIODirectionInOut | getCaching(in->Attrib);
    if (in->Contiguous)
    {
        options |= kIOMemoryPhysicallyContiguous;
    }

    if (in->AddressBits == 64)
    {
        mem = IOBufferMemoryDescriptor::inTaskWithOptions(kernel_task, options, in->NumBytes);
    }
    else if (in->AddressBits > 0 && in->AddressBits < 64)
    {
        // For physicalMasks bigger than 32-bit, kernel refuses to allocate after some time
        mach_vm_address_t physicalMask = 0x00000000FFFFFFFFULL;

        if (in->AddressBits < 32)
            physicalMask >>= (32 - in->AddressBits);

        mem = IOBufferMemoryDescriptor::inTaskWithPhysicalMask(kernel_task, options, in->NumBytes, physicalMask);
    }
    else
    {
        SLOG_ERR_EXT("invalid AddressBits (%" PRIu32 ")", in->AddressBits);
        return kIOReturnBadArgument;
    }

    if (!mem)
    {
        SLOG_ERR_EXT("allocation of %" PRIu64 " bytes failed", in->NumBytes);
        return kIOReturnNoMemory;
    }

    IOLockLock(target->lock);
    Allocation *alloc = RegisterAllocation(&target->MemoryAllocations, mem, in->Attrib);
    if (!alloc)
    {
        mem->release();
        SLOG_ERR_EXT("could not add client mapping");
        IOLockUnlock(target->lock);
        return kIOReturnError;
    }
    out->MemoryHandle = (uint64_t)alloc;
    alloc->RefCount++;
    IOLockUnlock(target->lock);

    if ((kr = mem->prepare()) != kIOReturnSuccess)
    {
        UnregisterAllocation(alloc);
        SLOG_ERR_EXT("prepare failed");
        return kr;
    }

    SLOG_EXT();

    return kIOReturnSuccess;
}

IOReturn LwModsUserClient::freePages
(
    LwModsUserClient *target,
    void *reference,
    IOExternalMethodArguments *arguments
)
{
    LwModsDrvFreePagesIn *in = (LwModsDrvFreePagesIn *)arguments->structureInput;
    Allocation *alloc = (Allocation *)in->MemoryHandle;

    SLOG_ENT();

    IOLockLock(target->lock);
    if (!isValidMemoryHandle(&target->MemoryAllocations, alloc))
    {
        IOLockUnlock(target->lock);
        SLOG_ERR_EXT("invalid MemoryHandle (%p)", alloc);
        return kIOReturnError;
    }

    if (alloc->RefCount > 1)
    {
        Mapping *map;
        Mapping *next;

        SLOG_ERR("memory allocation (%p) still has %d references", alloc, alloc->RefCount - 1);

        LIST_FOREACH_SAFE(map, &target->MemoryMappings, List, next)
        {
            if (map->alloc == alloc)
            {
                SLOG_ERR("freeing mapped memory at (%p)", (void *)map->iomm->getAddress());
                UnregisterMapping(map);
            }
        }
    }

    UnregisterAllocation(alloc);

    IOLockUnlock(target->lock);

    SLOG_EXT();
    return kIOReturnSuccess;
}

IOReturn LwModsUserClient::mapSystemPages
(
    LwModsUserClient *target,
    void *reference,
    IOExternalMethodArguments *arguments
)
{
    LwModsDrvMapSystemPagesIn *in = (LwModsDrvMapSystemPagesIn *)arguments->structureInput;
    LwModsDrvMapSystemPagesOut *out = (LwModsDrvMapSystemPagesOut *)arguments->structureOutput;
    Allocation *alloc = (Allocation *)in->MemoryHandle;
    IOOptionBits options;
    IOMemoryMap *iomm;

    SLOG_ENT();

    IOLockLock(target->lock);
    if (!isValidMemoryHandle(&target->MemoryAllocations, alloc))
    {
        IOLockUnlock(target->lock);
        SLOG_ERR_EXT("invalid MemoryHandle (%p)", alloc);
        return kIOReturnError;
    }

    options = kIOMapAnywhere | kIOMapUnique | getCaching(alloc->Attrib);
    iomm = alloc->iomd->createMappingInTask(lwrrent_task(), NULL, options, in->Offset, in->NumBytes);
    if (!iomm || !RegisterMapping(&target->MemoryMappings, alloc, iomm))
    {
        IOLockUnlock(target->lock);
        if (iomm)
           iomm->release();
        SLOG_ERR_EXT("failed to map MemoryHandle (%p)", alloc);
        return kIOReturnError;
    }

    out->VirtualAddress = iomm->getAddress();
    IOLockUnlock(target->lock);

    SLOG_EXT();

    return kIOReturnSuccess;
}

IOReturn LwModsUserClient::mapDeviceMemory
(
    LwModsUserClient *target,
    void *reference,
    IOExternalMethodArguments *arguments
)
{
    IOReturn kr;
    LwModsDrvMapDeviceMemoryIn *in = (LwModsDrvMapDeviceMemoryIn *)arguments->structureInput;
    LwModsDrvMapDeviceMemoryOut *out = (LwModsDrvMapDeviceMemoryOut *)arguments->structureOutput;
    IOAddressRange range;

    SLOG_ENT();

    // TODO: Do we want to search allocations to see if this memory has been allocated and return error?
    range.address = in->PhysicalAddress;
    range.length = in->NumBytes;
    IOMemoryDescriptor *iomd = IOMemoryDescriptor::withOptions(
        &range, 1 /* count */,
        0, // Only used when options:type = UPL
        0, // Only used options:type = Virtual
        kIODirectionInOut | kIOMemoryTypePhysical64 /* options */);
    if (NULL == iomd)
    {
        SLOG_ERR_EXT("failure in IOMemoryDescriptor::withOptions(%p, %" PRIu64 ")",
                     (void *)in->PhysicalAddress, in->NumBytes);
        return kIOReturnError;
    }

    if ((kr = iomd->prepare()) != kIOReturnSuccess)
    {
        iomd->release();
        SLOG_ERR_EXT("prepare failed");
        return kr;
    }

    IOMemoryMap *iomm = iomd->createMappingInTask(lwrrent_task(), NULL, kIOMapAnywhere | kIOMapUnique | getCaching(in->Attrib));
    if (NULL == iomm)
    {
        if (iomd->complete() != kIOReturnSuccess)
            KLOG("%s: WARNING: complete failed\n", __FUNCTION__);

        iomd->release();
        SLOG_ERR_EXT("failed to map device memory at address %p", (void *)in->PhysicalAddress);
        return kIOReturnError;
    }

    IOLockLock(target->lock);
    Allocation *alloc = RegisterAllocation(&target->MemoryAllocations, iomd, in->Attrib);
    if (alloc == NULL)
    {
        IOLockUnlock(target->lock);
        iomm->release();

        if (iomd->complete() != kIOReturnSuccess)
            KLOG("%s: WARNING: complete failed\n", __FUNCTION__);

        iomd->release();
        SLOG_ERR_EXT("failed to add allocation");
        return kIOReturnError;
    }

    // Retain alloc in case of RegisterMapping failure
    alloc->RefCount++;

    if (RegisterMapping(&target->MemoryMappings, alloc, iomm) == NULL)
    {
        UnregisterAllocation(alloc);
        IOLockUnlock(target->lock);
        iomm->release();
        SLOG_ERR_EXT("failed to add mapping");
        return kIOReturnError;
    }

    // Mapping is the only reference to alloc
    alloc->RefCount--;

    IOLockUnlock(target->lock);

    out->VirtualAddress = (uint64_t)iomm->getAddress();

    SLOG_EXT();

    return kIOReturnSuccess;
}

IOReturn LwModsUserClient::unmapMemory
(
    LwModsUserClient *target,
    void *reference,
    IOExternalMethodArguments *arguments
)
{
    LwModsDrvUnmapMemoryIn *in = (LwModsDrvUnmapMemoryIn *)arguments->structureInput;
    Mapping *map;

    SLOG_ENT();

    IOLockLock(target->lock);
    LIST_FOREACH(map, &target->MemoryMappings, List)
    {
        if ((map->iomm->getAddress() == in->VirtualAddress) &&
            (map->iomm->getSize() == in->NumBytes))
        {
            UnregisterMapping(map);
            IOLockUnlock(target->lock);
            SLOG_EXT();
            return kIOReturnSuccess;
        }
    }

    IOLockUnlock(target->lock);
    SLOG_ERR_EXT("could not find mapping at address %p", (void *)in->VirtualAddress);
    return kIOReturnError;
}

IOReturn LwModsUserClient::getPhysicalAddress
(
    LwModsUserClient *target,
    void *reference,
    IOExternalMethodArguments *arguments
)
{
    LwModsDrvGetPhysicalAddressIn *in = (LwModsDrvGetPhysicalAddressIn *)arguments->structureInput;
    LwModsDrvGetPhysicalAddressOut *out = (LwModsDrvGetPhysicalAddressOut *)arguments->structureOutput;
    Allocation *alloc = (Allocation *)in->MemoryHandle;

    SLOG_ENT();

    IOLockLock(target->lock);
    if (!isValidMemoryHandle(&target->MemoryAllocations, alloc))
    {
        IOLockUnlock(target->lock);
        SLOG_ERR_EXT("invalid MemoryHandle (%p)", alloc);
        return kIOReturnError;
    }
    out->PhysicalAddress = (uint64_t)alloc->iomd->getPhysicalSegment(in->Offset, NULL);
    IOLockUnlock(target->lock);

    if (out->PhysicalAddress == 0)
    {
        SLOG_ERR_EXT("invalid offset (0x016%" PRIx64 ")", in->Offset);
        return kIOReturnError;
    }

    SLOG_EXT();
    return kIOReturnSuccess;
}

IOReturn LwModsUserClient::virtualToPhysical
(
    LwModsUserClient *target,
    void *reference,
    IOExternalMethodArguments *arguments
)
{
    LwModsDrvVirtualToPhysicalIn *in = (LwModsDrvVirtualToPhysicalIn *)arguments->structureInput;
    LwModsDrvVirtualToPhysicalOut *out = (LwModsDrvVirtualToPhysicalOut *)arguments->structureOutput;
    Mapping *map;

    SLOG_ENT();

    IOLockLock(target->lock);
    LIST_FOREACH(map, &target->MemoryMappings, List)
    {
        uint64_t vaddr = map->iomm->getAddress();
        if ((vaddr <= in->VirtualAddress) &&
            ((vaddr + map->iomm->getSize()) > in->VirtualAddress))
        {
            out->PhysicalAddress = map->alloc->iomd->getPhysicalSegment(in->VirtualAddress - vaddr, NULL, kIOMemoryMapperNone);
            IOLockUnlock(target->lock);
            SLOG_EXT();
            return kIOReturnSuccess;
        }
    }

    IOLockUnlock(target->lock);
    SLOG_ERR_EXT("could not find mapping at virtual address %p", (void *)in->VirtualAddress);
    return kIOReturnError;
}

IOReturn LwModsUserClient::physicalToVirtual
(
    LwModsUserClient *target,
    void *reference,
    IOExternalMethodArguments *arguments
)
{
    LwModsDrvPhysicalToVirtualIn *in = (LwModsDrvPhysicalToVirtualIn *)arguments->structureInput;
    LwModsDrvPhysicalToVirtualOut *out = (LwModsDrvPhysicalToVirtualOut *)arguments->structureOutput;
    Mapping *map;

    SLOG_ENT();

    IOLockLock(target->lock);
    LIST_FOREACH(map, &target->MemoryMappings, List)
    {
        IOByteCount offset = 0;
        while (offset < map->iomm->getSize())
        {
            IOByteCount length;
            uint64_t paddr = map->iomm->getPhysicalSegment(offset, &length);
            if ((paddr <= in->PhysicalAddress) && ((paddr + length) > in->PhysicalAddress))
            {
                out->VirtualAddress = map->iomm->getAddress() + offset + (in->PhysicalAddress - paddr);
                IOLockUnlock(target->lock);
                SLOG_EXT();
                return kIOReturnSuccess;
            }
            offset += length;
        }
    }

    IOLockUnlock(target->lock);
    SLOG_ERR_EXT("physical address %p has not been allocated", (void *)in->PhysicalAddress);
    return kIOReturnError;
}

IOReturn LwModsUserClient::ioRead
(
    LwModsUserClient *target,
    void *reference,
    IOExternalMethodArguments *arguments
)
{
    LwModsDrvIoReadIn *in = (LwModsDrvIoReadIn *)arguments->structureInput;
    LwModsDrvIoReadOut *out = (LwModsDrvIoReadOut *)arguments->structureOutput;

    SLOG_ENT();

    switch (in->DataSize)
    {
    case sizeof(uint8_t):
        out->Data = inb(in->Address);
        break;
    case sizeof(uint16_t):
        out->Data = inw(in->Address);
        break;
    case sizeof(uint32_t):
        out->Data = inl(in->Address);
        break;
    default:
        SLOG_ERR_EXT("invalid DataSize (%" PRIu32 ")", in->DataSize);
        return kIOReturnBadArgument;
    }

    SLOG_EXT();
    return kIOReturnSuccess;
}

IOReturn LwModsUserClient::ioWrite
(
    LwModsUserClient *target,
    void *reference,
    IOExternalMethodArguments *arguments
)
{
    LwModsDrvIoWriteIn *in = (LwModsDrvIoWriteIn *)arguments->structureInput;

    SLOG_ENT();

    switch (in->DataSize)
    {
    case sizeof(uint8_t):
        outb(in->Address, in->Data);
        break;
    case sizeof(uint16_t):
        outw(in->Address, in->Data);
        break;
    case sizeof(uint32_t):
        outl(in->Address, in->Data);
        break;
    default:
        SLOG_ERR_EXT("invalid DataSize (%" PRIu32 ")", in->DataSize);
        return kIOReturnBadArgument;
    }

    SLOG_EXT();
    return kIOReturnSuccess;
}

IOReturn LwModsUserClient::setConsoleInfo
(
    LwModsUserClient *target,
    void *reference,
    IOExternalMethodArguments *arguments
)
{
    LwModsDrvSetConsoleInfoIn *in = (LwModsDrvSetConsoleInfoIn *)arguments->structureInput;
    IOPlatformExpert *platform;
    IOReturn kr;
    uint32_t op;

    SLOG_ENT();

    switch (in->op) {
    case kLwModsConsoleInfoDisable:
        op = kPEDisableScreen;
        break;
    case kLwModsConsoleInfoEnable:
        op = kPEEnableScreen;
        break;
    default:
        SLOG_ERR_EXT("invalid operation (%u)", in->op);
        return kIOReturnBadArgument;
    }

    if (! (platform = IOService::getPlatform())) {
        SLOG_ERR_EXT("cannot get IOPlatformExpert");
        return kIOReturnNotFound;
    }

    kr = platform->setConsoleInfo(0, op);
    if (kr != kIOReturnSuccess) {
        SLOG_ERR_EXT("cannot set console info");
        return kr;
    }

    SLOG_EXT();
    return kIOReturnSuccess;
}

IOWorkLoop * LwModsUserClient::getWorkLoop()
{
    // Based on Apple documentation

    // Do we have a work loop already?, if so return it NOW.
    if ((vm_address_t) workLoop >> 1)
        return workLoop;

    if (OSCompareAndSwap(0, 1, (UInt32 *) &workLoop))
    {
        // Construct the workloop and set the workLoop variable
        // to whatever the result is and return
        workLoop = IOWorkLoop::workLoop();
    }
    else while ((IOWorkLoop *) workLoop == (IOWorkLoop *) 1)
        // Spin around the workLoop variable until the
        // initialization finishes.
        thread_block(0);

    return workLoop;
}

IOReturn LwModsUserClient::registerNotificationPort(mach_port_t port, UInt32 type, UInt32 refCon)
{
    LOG_ENT();

    switch (type)
    {
    case kLwModsMachPortInterruptsQueue:
        if (!interruptQueue) {
            LOG_ERR_EXT("interrupt queue not created");
            return kIOReturnError;
        }
        interruptQueue->setNotificationPort(port);
        break;
    default:
        LOG_ERR_EXT("unknown type");
        return kIOReturnBadArgument;
    }

    LOG_EXT();

    return kIOReturnSuccess;
}

IOReturn LwModsUserClient::registerInterrupt
(
    LwModsUserClient *target,
    void *reference,
    IOExternalMethodArguments *arguments
)
{
    LwModsDrvRegisterInterruptIn *in = (LwModsDrvRegisterInterruptIn *)arguments->structureInput;
    IOPCIDevice *provider;
    InterruptController *controller;
    IOReturn ret;

    SLOG_ENT();

    if (in->Type != kLwModsInterruptTypeInt && in->Type != kLwModsInterruptTypeMsi)
    {
        SLOG_ERR_EXT("invalid interrupt type");
        return kIOReturnBadArgument;
    }

    provider = (IOPCIDevice*)getPciFromPath(in->Path);
    if (!provider)
    {
        SLOG_ERR_EXT("failed to get device");
        return kIOReturnBadArgument;
    }

    if (!target->interruptQueue)
    {
        SLOG_ERR_EXT("interrupt queue is not ready!");
        return kIOReturnError;
    }

    LIST_FOREACH(controller, &target->registeredInterrupts, list)
    {
        if (controller->source->getProvider() == provider)
        {
            SLOG_ERR_EXT("interrupt already registered for this device");
            return kIOReturnError;
        }
    }

    controller = InterruptController::interruptController(provider, in->Type, in->Index, target->interruptQueue);
    if (!controller)
    {
        SLOG_ERR_EXT("failed to create interrupt controller");
        return kIOReturnError;
    }

    if ((ret = target->getWorkLoop()->addEventSource(controller->source)) != kIOReturnSuccess)
    {
        SLOG_ERR_EXT("failed to add interrupt event source to work loop");
        controller->release();
        return ret;
    }

    LIST_INSERT_HEAD(&target->registeredInterrupts, controller, list);

    controller->source->enable();

    KLOG("Registered %s interrupt for device %x:%02x.%x\n",
        in->Type == kLwModsInterruptTypeInt ? "INTA" : "MSI", provider->getBusNumber(),
        provider->getDeviceNumber(), provider->getFunctionNumber());

    SLOG_EXT();

    return kIOReturnSuccess;
}

IOReturn LwModsUserClient::unregisterInterrupt
(
    LwModsUserClient *target,
    void *reference,
    IOExternalMethodArguments *arguments
)
{
    LwModsDrvUnregisterInterruptIn *in = (LwModsDrvUnregisterInterruptIn *)arguments->structureInput;
    IOPCIDevice *provider;
    InterruptController *controller;
    bool found = false;

    SLOG_ENT();

    if (in->Type != kLwModsInterruptTypeInt && in->Type != kLwModsInterruptTypeMsi)
    {
        SLOG_ERR_EXT("invalid interrupt type");
        return kIOReturnBadArgument;
    }

    provider = getPciFromPath(in->Path);
    if (!provider)
    {
        SLOG_ERR_EXT("failed to get device");
        return kIOReturnBadArgument;
    }

    LIST_FOREACH(controller, &target->registeredInterrupts, list)
    {
        if (controller->source->getProvider() == provider)
        {
            found = true;
            break;
        }
    }
    if (!found)
    {
        SLOG_ERR_EXT("interrupt not registered");
        return kIOReturnError;
    }

    controller->source->disable();

    IOReturn rc = target->getWorkLoop()->removeEventSource(controller->source);
    if (rc != kIOReturnSuccess)
    {
        SLOG_ERR_EXT("failed to remove event source from work loop");
        return kIOReturnError;
    }

    LIST_REMOVE(controller, list);
    controller->release();

    // Say 'bye' to interrupt thread if there are no more registered interrupts
    if (LIST_EMPTY(&target->registeredInterrupts))
    {
        target->interruptQueue->enqueue(NULL, 0);
    }

    KLOG("Unregistered %s interrupt for device %x:%02x.%x\n",
        in->Type == kLwModsInterruptTypeInt ? "INTA" : "MSI", provider->getBusNumber(),
        provider->getDeviceNumber(), provider->getFunctionNumber());

    SLOG_EXT();

    return kIOReturnSuccess;
}

IOReturn LwModsUserClient::createInterruptQueue
(
    LwModsUserClient *target,
    void *reference,
    IOExternalMethodArguments *arguments
)
{
    LwModsDrvCreateInterruptQueueOut *out = (LwModsDrvCreateInterruptQueueOut *)arguments->structureOutput;
    IOMemoryDescriptor *iomd;
    IOMemoryMap *iomm;
    IODataQueue *queue;

    SLOG_ENT();

    if (target->interruptQueue)
    {
        SLOG_ERR_EXT("queue already created");
        return kIOReturnError;
    }

    queue = IODataQueue::withEntries(INTERRUPT_QUEUE_SIZE, sizeof(LwModsInterrupt));
    if (!queue)
    {
        SLOG_ERR_EXT("failed to create IODataQueue");
        return kIOReturnError;
    }

    iomd = queue->getMemoryDescriptor();
    if (!iomd)
    {
        SLOG_ERR_EXT("failed to get memory descriptor");
        queue->release();
        return kIOReturnError;
    }

    // Map IODataQueue for user
    iomm = iomd->createMappingInTask(lwrrent_task(), NULL, kIOMapAnywhere | kIOMapUnique);
    if (!iomm)
    {
        SLOG_ERR_EXT("failed to map interrupt queue");
        queue->release();
        return kIOReturnError;
    }

    out->QueueAddress = iomm->getAddress();

    target->interruptQueue = queue;
    target->interruptQueueMapping = iomm;

    SLOG_EXT();

    return kIOReturnSuccess;
}

IOReturn LwModsUserClient::releaseInterruptQueue
(
    LwModsUserClient *target,
    void *reference,
    IOExternalMethodArguments *arguments
)
{
    SLOG_ENT();

    if (!LIST_EMPTY(&target->registeredInterrupts))
    {
        SLOG_ERR_EXT("interrupts still registered!");
        return kIOReturnError;
    }

    if (!target->interruptQueue)
    {
        SLOG_ERR_EXT("queue not created!");
        return kIOReturnError;
    }

    target->interruptQueueMapping->release();
    target->interruptQueue->release();
    target->interruptQueue = 0;

    SLOG_EXT();

    return kIOReturnSuccess;
}

const IOExternalMethodDispatch LwModsUserClient::sMethods[kLwModsDrvMethodCount] =
{
    // method,
    // number of scalar inputs, size of input strulwtre,
    // number of scalar outputs, size of output structure
    {   // kLwModsDrvGetApiVersion
        (IOExternalMethodAction)&LwModsUserClient::getApiVersion,
        0, 0, 0, sizeof (LwModsDrvGetApiVersionOut)
    },
    {   // kLwModsDrvEnterDebugger
        (IOExternalMethodAction)&LwModsUserClient::enterDebugger,
        0, kIOUCVariableStructureSize, 0, 0
    },
    {   // kLwModsDrvGetPciInfo
        (IOExternalMethodAction)&LwModsUserClient::getPciInfo,
        0, sizeof (LwModsDrvGetPciInfoIn), 0, sizeof (LwModsDrvGetPciInfoOut)
    },
    {   // kLwModsDrvPciConfigRead
        (IOExternalMethodAction)&LwModsUserClient::pciConfigRead,
        0, sizeof (LwModsDrvPciConfigReadIn), 0, sizeof (LwModsDrvPciConfigReadOut)
    },
    {   // kLwModsDrvPciConfigWrite
        (IOExternalMethodAction)&LwModsUserClient::pciConfigWrite,
        0, sizeof (LwModsDrvPciConfigWriteIn), 0, 0
    },
    {   // kLwModsDrvAllocPages
        (IOExternalMethodAction)&LwModsUserClient::allocPages,
        0, sizeof (LwModsDrvAllocPagesIn), 0, sizeof (LwModsDrvAllocPagesOut)
    },
    {   // kLwModsDrvFreePages
        (IOExternalMethodAction)&LwModsUserClient::freePages,
        0, sizeof (LwModsDrvFreePagesIn), 0, 0
    },
    {   // kLwModsDrvMapSystemPages
        (IOExternalMethodAction)&LwModsUserClient::mapSystemPages,
        0, sizeof (LwModsDrvMapSystemPagesIn), 0, sizeof (LwModsDrvMapSystemPagesOut)
    },
    {   // kLwModsDrvMapDeviceMemory
        (IOExternalMethodAction)&LwModsUserClient::mapDeviceMemory,
        0, sizeof (LwModsDrvMapDeviceMemoryIn), 0, sizeof (LwModsDrvMapDeviceMemoryOut)
    },
    {   // kLwModsDrvUnmapMemory
        (IOExternalMethodAction)&LwModsUserClient::unmapMemory,
        0, sizeof (LwModsDrvUnmapMemoryIn), 0, 0
    },
    {   // kLwModsDrvGetPhysicalAddress
        (IOExternalMethodAction)&LwModsUserClient::getPhysicalAddress,
        0, sizeof (LwModsDrvGetPhysicalAddressIn), 0, sizeof (LwModsDrvGetPhysicalAddressOut)
    },
    {   // kLwModsDrvVirtualToPhysical
        (IOExternalMethodAction)&LwModsUserClient::virtualToPhysical,
        0, sizeof (LwModsDrvVirtualToPhysicalIn), 0, sizeof (LwModsDrvVirtualToPhysicalOut)
    },
    {   // kLwModsDrvPhysicalToVirtual
        (IOExternalMethodAction)&LwModsUserClient::physicalToVirtual,
        0, sizeof (LwModsDrvPhysicalToVirtualIn), 0, sizeof (LwModsDrvPhysicalToVirtualOut)
    },
    {   // kLwModsDrvIoRead
        (IOExternalMethodAction)&LwModsUserClient::ioRead,
        0, sizeof (LwModsDrvIoReadIn), 0, sizeof (LwModsDrvIoReadOut)
    },
    {   // kLwModsDrvIoWrite
        (IOExternalMethodAction)&LwModsUserClient::ioWrite,
        0, sizeof (LwModsDrvIoWriteIn), 0, 0
    },
    {
        // kLwModsDrvSetConsoleInfo
        (IOExternalMethodAction)&LwModsUserClient::setConsoleInfo,
        0, sizeof (LwModsDrvSetConsoleInfoIn), 0, 0
    },
    {
        // kLwModsDrvRegisterInterrupt
        (IOExternalMethodAction)&LwModsUserClient::registerInterrupt,
        0, sizeof (LwModsDrvRegisterInterruptIn), 0, 0
    },
    {
        // kLwModsDrvUnregisterInterrupt
        (IOExternalMethodAction)&LwModsUserClient::unregisterInterrupt,
        0, sizeof (LwModsDrvUnregisterInterruptIn), 0, 0
    },
    {
        // kLwModsDrvCreateInterruptQueue
        (IOExternalMethodAction)&LwModsUserClient::createInterruptQueue,
        0, 0, 0, sizeof (LwModsDrvCreateInterruptQueueOut)
    },
    {
        // kLwModsDrvReleaseInterruptQueue
        (IOExternalMethodAction)&LwModsUserClient::releaseInterruptQueue,
        0, 0, 0, 0
    },
};

IOReturn LwModsUserClient::externalMethod
(
    uint32_t selector,
    IOExternalMethodArguments* arguments,
    IOExternalMethodDispatch* dispatch,
    OSObject* target,
    void* reference
)
{
    IOReturn result;

    LOG_ENT();

    if (selector >= (uint32_t)kLwModsDrvMethodCount)
    {
        LOG_ERR_EXT("invalid selector (%" PRIu32 ")", selector);
        return kIOReturnError;
    }

    dispatch = (IOExternalMethodDispatch *)&sMethods[selector];

    if (!target)
    {
        target = this;
    }

    result = super::externalMethod(selector, arguments, dispatch, target, reference);
    if (result != kIOReturnSuccess)
    {
        LOG_ERR_EXT("failure in %s::%s", super::getName(), __FUNCTION__);
        return result;
    }

    LOG_EXT();
    return kIOReturnSuccess;
}
