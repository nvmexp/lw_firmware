/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2010-2011 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#ifndef INCLUDED_LWMODSUSERCLIENT_H
#define INCLUDED_LWMODSUSERCLIENT_H

#include <IOKit/pci/IOPCIDevice.h>
#include <IOKit/IOUserClient.h>
#include <IOKit/IODeviceTreeSupport.h>
#include <IOKit/IOMemoryDescriptor.h>
#include <IOKit/IOFilterInterruptEventSource.h>
#include <IOKit/IODataQueue.h>
#include <sys/queue.h>

#include "ModsDriver.h"
#include "LwModsDriverApi.h"

#define LwModsUserClient com_lwidia_driver_LwModsUserClient

#define PCI_CONFIG_ADDR_DEVICE_ID   0x0
#define PCI_CONFIG_ADDR_VENDOR_ID   0x2
#define PCI_CONFIG_ADDR_CLASS_CODE  0x8
#define PCI_CONFIG_SHIFT_CLASS_CODE 8

// Must be "struct" not "class" because LIST_ENTRY uses "struct"
struct InterruptController : public OSObject
{
    OSDeclareDefaultStructors(InterruptController);

protected:
    IODataQueue *queue;
    uint32_t index;

    IOMemoryMap *bar0_map;
    uint32_t *intr_enabled, *intr_state;

public:
    IOInterruptEventSource *source;
    LIST_ENTRY(InterruptController) list;

    virtual void free();
    void handle(IOInterruptEventSource *source, int count);
    bool filter(IOFilterInterruptEventSource *source);

    static InterruptController* interruptController(IOPCIDevice *iopci, uint32_t type, uint32_t index, IODataQueue *queue);
};

class LwModsUserClient : public IOUserClient
{
    OSDeclareDefaultStructors(com_lwidia_driver_LwModsUserClient);

protected:
    LwModsDriver *lwdrv;
    static const IOExternalMethodDispatch sMethods[kLwModsDrvMethodCount];

    IOLock *lock;
    IOWorkLoop *workLoop;
    IODataQueue *interruptQueue;
    IOMemoryMap *interruptQueueMapping;

    struct Allocation
    {
        IOMemoryDescriptor *iomd;
        uint32_t Attrib;
        int RefCount;
        LIST_ENTRY(Allocation) List;
    };
    LIST_HEAD(AllocationList, Allocation) MemoryAllocations;

    struct Mapping
    {
        IOMemoryMap *iomm;
        Allocation *alloc;
        LIST_ENTRY(Mapping) List;
    };
    LIST_HEAD(MappingList, Mapping) MemoryMappings;

    LIST_HEAD(InterruptList, InterruptController) registeredInterrupts;

    static Allocation *RegisterAllocation(AllocationList *list, IOMemoryDescriptor *iomd, uint32_t Attrib);
    static void UnregisterAllocation(Allocation *alloc);
    static Mapping *RegisterMapping(MappingList *list, Allocation *alloc, IOMemoryMap *iomm);
    static void UnregisterMapping(Mapping *map);
    static IOPCIDevice *getPciFromPath(const char *path);
    static IOOptionBits getCaching(uint32_t attrib);
    static bool isValidMemoryHandle(AllocationList *list, Allocation *mh);

    IOWorkLoop * getWorkLoop();

public:
    // OSObject methods
    // TODO: init, then start
    virtual bool initWithTask(task_t owningTask, void* selwrityToken, UInt32 type);                        // allocate stuff, don't really have

    // IOService methods
    virtual bool start(IOService *provider);    // Save the ioservice and tell us we've started, prob not necessary if we dont care about the driver
    virtual void stop(IOService *provider);     // deallocate and call useless parent function

    // didTerminate: should we override?  sample calls clientclose and sets defer = false

    // IOUserClient methods
    virtual IOReturn clientClose();             // Return success. free up driver provider

protected:
    virtual IOReturn registerNotificationPort(mach_port_t port, UInt32 type, UInt32 refCon);
    virtual IOReturn externalMethod(uint32_t selector, IOExternalMethodArguments *arguments,
                                    IOExternalMethodDispatch *dispatch, OSObject *target, void *reference);

    // User methods
    // Other
    static IOReturn getApiVersion(LwModsUserClient *target, void *reference, IOExternalMethodArguments *arguments);

    // Debug
    static IOReturn enterDebugger(LwModsUserClient *target, void *reference, IOExternalMethodArguments *arguments);

    // PCI
    static IOReturn getPciInfo(LwModsUserClient *target, void *reference, IOExternalMethodArguments *arguments);
    static IOReturn pciConfigRead(LwModsUserClient *target, void *reference, IOExternalMethodArguments *arguments);
    static IOReturn pciConfigWrite(LwModsUserClient *target, void *reference, IOExternalMethodArguments *arguments);

    // Memory
    static IOReturn allocPages(LwModsUserClient *target, void *reference, IOExternalMethodArguments *arguments);
    static IOReturn freePages(LwModsUserClient *target, void *reference, IOExternalMethodArguments *arguments);
    static IOReturn mapSystemPages(LwModsUserClient *target, void *reference, IOExternalMethodArguments *arguments);
    static IOReturn mapDeviceMemory(LwModsUserClient *target, void *reference, IOExternalMethodArguments *arguments);
    static IOReturn unmapMemory(LwModsUserClient *target, void *reference, IOExternalMethodArguments *arguments);
    static IOReturn getPhysicalAddress(LwModsUserClient *target, void *reference, IOExternalMethodArguments *arguments);
    static IOReturn virtualToPhysical(LwModsUserClient *target, void *reference, IOExternalMethodArguments *arguments);
    static IOReturn physicalToVirtual(LwModsUserClient *target, void *reference, IOExternalMethodArguments *arguments);

    // PIO
    static IOReturn ioRead(LwModsUserClient *target, void *reference, IOExternalMethodArguments *arguments);
    static IOReturn ioWrite(LwModsUserClient *target, void *reference, IOExternalMethodArguments *arguments);

    // Console
    static IOReturn setConsoleInfo(LwModsUserClient *target, void *reference, IOExternalMethodArguments *arguments);

    // Interrupts
    static IOReturn registerInterrupt(LwModsUserClient *target, void *reference, IOExternalMethodArguments *arguments);
    static IOReturn unregisterInterrupt(LwModsUserClient *target, void *reference, IOExternalMethodArguments *arguments);
    static IOReturn createInterruptQueue(LwModsUserClient *target, void *reference, IOExternalMethodArguments *arguments);
    static IOReturn releaseInterruptQueue(LwModsUserClient *target, void *reference, IOExternalMethodArguments *arguments);
};

#endif // INCLUDED_LWMODSUSERCLIENT_H
