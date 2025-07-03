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

/***************** Interface between mods and kernel *********************\
 *                                                                       *
 * Module: libmods.c                                                     *
 *   The bridge between the user space RM in mods and the kernel.        *
 *                                                                       *
\*************************************************************************/

#include <stdlib.h>
#include <string.h>
#include <IOKit/IOKitLib.h>
#include <sys/queue.h>
#include <stdio.h>

#include "libmods.h"
#include "core/include/modsdrv.h"

typedef struct _PCI_INFO
{
    io_service_t service;
    uint8_t bus_num;
    uint8_t dev_num;
    uint8_t func_num;
    uint16_t dev_id;
    uint16_t vendor_id;
    uint32_t class_code;
    LIST_ENTRY(_PCI_INFO) entries;
} PCI_INFO;

typedef LIST_HEAD(_PCI_LIST, _PCI_INFO) PCI_LIST;

typedef struct
{
    io_connect_t connect;
    PCI_LIST pci_list;
} MODS_INFO;

static MODS_INFO *s_mods = NULL;

#define CHECK_MODS() \
    if (s_mods == NULL) \
    { \
        return -1; \
    }

#define PRINTF_IOERROR(ret, msg) ModsDrvPrintf(PRI_HIGH, "%s: " msg " %x.%x.%x\n", __FUNCTION__, err_get_system(ret), err_get_sub(ret), err_get_code(ret))

#define CALL_CONNECT(m, in, inSize, out, outSize) \
    do { \
        CHECK_MODS(); \
        size_t outSizeTmp = (outSize); \
        kern_return_t kr = IOConnectCallMethod(s_mods->connect, (m), NULL, 0, (in), (inSize), \
                NULL, 0, (out), &outSizeTmp); \
        if (outSizeTmp != (outSize)) \
        { \
            ModsDrvPrintf(PRI_HIGH, "%s: IOConnectCallMethod error: wrong output size\n", __FUNCTION__); \
            return -1; \
        } \
        if (kr != kIOReturnSuccess) \
        { \
            PRINTF_IOERROR(kr, "IOConnectCallMethod error"); \
            return -1; \
        } \
    } while (0);

#define CALL_CONNECT_METHOD_EMPTY(m) CALL_CONNECT(m, NULL, 0, NULL, 0)
#define CALL_CONNECT_METHOD_OUT(m, out) CALL_CONNECT(m, NULL, 0, out, sizeof(*(out)))
#define CALL_CONNECT_METHOD_IN(m, in) CALL_CONNECT(m, in, sizeof(*(in)), NULL, 0)
#define CALL_CONNECT_METHOD(m, in, out) CALL_CONNECT(m, in, sizeof(*(in)), out, sizeof(*(out)))

int add_pci_info
(
    PCI_LIST *pl,
    io_service_t service,
    uint8_t bus_num,
    uint8_t dev_num,
    uint8_t func_num,
    uint16_t dev_id,
    uint16_t vendor_id,
    uint32_t class_code
)
{
    PCI_INFO *pi;

    pi = (PCI_INFO *)malloc(sizeof (PCI_INFO));
    if (pi == NULL)
    {
        return -1;
    }

    pi->service = service;
    pi->bus_num = bus_num;
    pi->dev_num = dev_num;
    pi->func_num = func_num;
    pi->dev_id = dev_id;
    pi->vendor_id = vendor_id;
    pi->class_code = class_code;

    LIST_INSERT_HEAD(pl, pi, entries);
    return 0;
}

int copy_service_path
(
    io_service_t service,
    char *path,
    size_t len
)
{
    io_string_t str;

    if (IORegistryEntryGetPath(service, kIODeviceTreePlane, str) !=
            KERN_SUCCESS)
    {
        return -1;
    }

    strncpy(path, str, len - 1);
    path[len-1] = 0;

    return 0;
}

int populate_pci_list
(
    io_connect_t connect,
    PCI_LIST *pci_list
)
{
    io_service_t service;
    io_iterator_t iterator;

    if (IOServiceGetMatchingServices(kIOMasterPortDefault,
            IOServiceMatching("IOPCIDevice"), &iterator) != KERN_SUCCESS)
    {
        return -1;
    }

    while ((service = IOIteratorNext(iterator)) != IO_OBJECT_NULL)
    {
        LwModsDrvGetPciInfoIn in;
        LwModsDrvGetPciInfoOut out;
        size_t outSize = sizeof (out);

        if (copy_service_path(service, in.Path, sizeof (in.Path)) < 0)
        {
            IOObjectRelease(service);
            return -1;
        }

        if ((IOConnectCallMethod(connect, kLwModsDrvGetPciInfo, NULL, 0,
                &in, sizeof (in), NULL, 0, &out, &outSize) != KERN_SUCCESS) ||
            (outSize != sizeof (out)))
        {
            IOObjectRelease(service);
            return -1;
        }

        if (add_pci_info(pci_list, service, out.BusNum, out.DevNum,
                out.FuncNum, out.DeviceId, out.VendorId,
                out.ClassCode) < 0)
        {
            IOObjectRelease(service);
            return -1;
        }
    }

    IOObjectRelease(iterator);
    return 0;
}

void empty_pci_list(PCI_LIST *pci_list)
{
    PCI_INFO *pi;

    while (!LIST_EMPTY(pci_list))
    {
        pi = LIST_FIRST(pci_list);
        LIST_REMOVE(pi, entries);
        IOObjectRelease(pi->service);
        free(pi);
    }
}

int get_pci_info
(
    PCI_LIST *pl,
    uint8_t bus_num,
    uint8_t dev_num,
    uint8_t func_num,
    PCI_INFO **pci_info
)
{
    PCI_INFO *pi;

    LIST_FOREACH(pi, pl, entries)
    {
        if ((pi->bus_num == bus_num) && (pi->dev_num == dev_num) &&
            (pi->func_num == func_num))
        {
            *pci_info = pi;
            return 0;
        }
    }

    return -1;
}

int get_pci_path
(
    uint8_t bus_num,
    uint8_t dev_num,
    uint8_t func_num,
    char *path,
    size_t len
)
{
    PCI_INFO *pi;
    if (get_pci_info(&s_mods->pci_list, bus_num, dev_num, func_num, &pi) < 0)
    {
        return -1;
    }

    if (copy_service_path(pi->service, path, len) < 0)
    {
        return -1;
    }
    return 0;
}

int mods_open()
{
    kern_return_t kr;
    io_service_t service;

    service = IOServiceGetMatchingService(kIOMasterPortDefault, IOServiceMatching(kLwModsDriver));
    if (service == IO_OBJECT_NULL)
    {
        return -1;
    }

    s_mods = (MODS_INFO *)malloc(sizeof (MODS_INFO));
    if (s_mods == NULL)
    {
        return -1;
    }

    kr = IOServiceOpen(service, mach_task_self(), 0, &s_mods->connect);
    IOObjectRelease(service);
    if (kr != KERN_SUCCESS)
    {
        free(s_mods);
        return -1;
    }

    LIST_INIT(&s_mods->pci_list);
    if (populate_pci_list(s_mods->connect, &s_mods->pci_list) < 0)
    {
        empty_pci_list(&s_mods->pci_list);
        free(s_mods);
        return -1;
    }

    return 0;
}

int mods_close()
{
    if (s_mods) {
        empty_pci_list(&s_mods->pci_list);
        IOServiceClose(s_mods->connect);
        free(s_mods);
    }

    return 0;
}

void * mods_get()
{
    return s_mods;
}

int mods_get_api_version(uint32_t *version)
{
    LwModsDrvGetApiVersionOut out;

    CALL_CONNECT_METHOD_OUT(kLwModsDrvGetApiVersion, &out);

    *version = out.Version;
    return 0;
}

int mods_find_pci_device
(
    uint32_t dev_id,
    uint32_t vendor_id,
    int idx,
    uint8_t *bus_num,
    uint8_t *dev_num,
    uint8_t *func_num
)
{
    PCI_INFO *pi;
    int i = 0;
    CHECK_MODS();

    LIST_FOREACH(pi, &s_mods->pci_list, entries)
    {
        if ((pi->dev_id == dev_id) && (pi->vendor_id == vendor_id))
        {
            if (i == idx)
            {
                *bus_num = pi->bus_num;
                *dev_num = pi->dev_num;
                *func_num = pi->func_num;
                return 0;
            }

            i++;
        }
    }

    return -1;
}

int mods_find_pci_class_code
(
    uint32_t class_code,
    int idx,
    uint8_t *bus_num,
    uint8_t *dev_num,
    uint8_t *func_num
)
{
    PCI_INFO *pi;
    int i = 0;
    CHECK_MODS();

    LIST_FOREACH(pi, &s_mods->pci_list, entries)
    {
        if (pi->class_code == class_code)
        {
            if (i == idx)
            {
                *bus_num = pi->bus_num;
                *dev_num = pi->dev_num;
                *func_num = pi->func_num;
                return 0;
            }

            i++;
        }
    }

    return -1;
}

int mods_pci_read
(
    uint8_t bus_num,
    uint8_t dev_num,
    uint8_t func_num,
    uint32_t addr,
    uint32_t *data,
    uint32_t data_size
)
{
    PCI_INFO *pi;
    LwModsDrvPciConfigReadIn in;
    LwModsDrvPciConfigReadOut out;
    CHECK_MODS();

    if (get_pci_info(&s_mods->pci_list, bus_num, dev_num, func_num, &pi) < 0)
    {
        return -1;
    }

    if (copy_service_path(pi->service, in.Path, sizeof (in.Path)) < 0)
    {
        return -1;
    }

    in.Address = addr;
    in.DataSize = data_size;
    CALL_CONNECT_METHOD(kLwModsDrvPciConfigRead, &in, &out);
    *data = out.Data;

    return 0;
}
int mods_pci_write
(
    uint8_t bus_num,
    uint8_t dev_num,
    uint8_t func_num,
    uint32_t addr,
    uint32_t data,
    uint32_t data_size
)
{
    PCI_INFO *pi;
    LwModsDrvPciConfigWriteIn in;
    CHECK_MODS();

    if (get_pci_info(&s_mods->pci_list, bus_num, dev_num, func_num, &pi) < 0)
    {
        return -1;
    }

    if (copy_service_path(pi->service, in.Path, sizeof (in.Path)) < 0)
    {
        return -1;
    }

    in.Address = addr;
    in.Data = data;
    in.DataSize = data_size;
    CALL_CONNECT_METHOD_IN(kLwModsDrvPciConfigWrite, &in);

    return 0;
}

int mods_alloc_pages
(
    uint64_t num_bytes,
    int contiguous,
    uint32_t addr_bits,
    MODS_ATTRIB attr,
    uint64_t *mem
)
{
    LwModsDrvAllocPagesIn in;
    LwModsDrvAllocPagesOut out;

    in.NumBytes = num_bytes;
    in.Contiguous = contiguous ? 1 : 0;
    in.AddressBits = addr_bits;

    switch (attr)
    {
    case MODS_ATTRIB_CACHED:
        in.Attrib = kLwModsMemoryCached;
        break;
    case MODS_ATTRIB_UNCACHED:
        in.Attrib = kLwModsMemoryUncached;
        break;
    case MODS_ATTRIB_WRITECOMBINE:
        in.Attrib = kLwModsMemoryWriteCombine;
        break;
    case MODS_ATTRIB_WRITETHROUGH:
        in.Attrib = kLwModsMemoryWriteThrough;
        break;
    case MODS_ATTRIB_WRITEPROTECT:
        in.Attrib = kLwModsMemoryWriteProtect;
        break;
    default:
        return -1;
    }

    CALL_CONNECT_METHOD(kLwModsDrvAllocPages, &in, &out);

    *mem = out.MemoryHandle;

    return 0;
}

int mods_free_pages(uint64_t mem)
{
    LwModsDrvFreePagesIn in;

    in.MemoryHandle = mem;

    CALL_CONNECT_METHOD_IN(kLwModsDrvFreePages, &in);

    return 0;
}

int mods_map_pages
(
    uint64_t mem,
    uint64_t offset,
    uint64_t num_bytes,
    void **virt_addr
)
{
    LwModsDrvMapSystemPagesIn in;
    LwModsDrvMapSystemPagesOut out;

    in.MemoryHandle = mem;
    in.Offset = offset;
    in.NumBytes = num_bytes;

    CALL_CONNECT_METHOD(kLwModsDrvMapSystemPages, &in, &out);

    *virt_addr = (void *)(uintptr_t)out.VirtualAddress;

    return 0;
}

int mods_map_dev_mem
(
    uint64_t phys_addr,
    uint64_t num_bytes,
    MODS_ATTRIB attr,
    void **virt_addr
)
{
    LwModsDrvMapDeviceMemoryIn in;
    LwModsDrvMapDeviceMemoryOut out;

    in.PhysicalAddress = phys_addr;
    in.NumBytes = num_bytes;

    switch (attr)
    {
    case MODS_ATTRIB_CACHED:
        in.Attrib = kLwModsMemoryCached;
        break;
    case MODS_ATTRIB_UNCACHED:
        in.Attrib = kLwModsMemoryUncached;
        break;
    case MODS_ATTRIB_WRITECOMBINE:
        in.Attrib = kLwModsMemoryWriteCombine;
        break;
    case MODS_ATTRIB_WRITETHROUGH:
        in.Attrib = kLwModsMemoryWriteThrough;
        break;
    case MODS_ATTRIB_WRITEPROTECT:
        in.Attrib = kLwModsMemoryWriteProtect;
        break;
    default:
        return -1;
    }

    CALL_CONNECT_METHOD(kLwModsDrvMapDeviceMemory, &in, &out);

    *virt_addr = (void *)(uintptr_t)out.VirtualAddress;

    return 0;
}

int mods_unmap_mem
(
    void *virt_addr,
    uint64_t num_bytes
)
{
    LwModsDrvUnmapMemoryIn in;

    in.VirtualAddress = (uint64_t)(uintptr_t)virt_addr;
    in.NumBytes = num_bytes;

    CALL_CONNECT_METHOD_IN(kLwModsDrvUnmapMemory, &in);

    return 0;
}

int mods_get_phys_addr
(
    uint64_t mem,
    uint64_t offset,
    uint64_t *phys_addr
)
{
    LwModsDrvGetPhysicalAddressIn in;
    LwModsDrvGetPhysicalAddressOut out;

    in.MemoryHandle = mem;
    in.Offset = offset;

    CALL_CONNECT_METHOD(kLwModsDrvGetPhysicalAddress, &in, &out);

    *phys_addr = out.PhysicalAddress;

    return 0;
}

int mods_virt_to_phys(void *virt_addr, uint64_t *phys_addr)
{
    LwModsDrvVirtualToPhysicalIn in;
    LwModsDrvVirtualToPhysicalOut out;

    in.VirtualAddress = (uint64_t)(uintptr_t)virt_addr;

    CALL_CONNECT_METHOD(kLwModsDrvVirtualToPhysical, &in, &out);

    *phys_addr = out.PhysicalAddress;

    return 0;
}

int mods_phys_to_virt(uint64_t phys_addr, void **virt_addr)
{
    LwModsDrvPhysicalToVirtualIn in;
    LwModsDrvPhysicalToVirtualOut out;

    in.PhysicalAddress = phys_addr;

    CALL_CONNECT_METHOD(kLwModsDrvPhysicalToVirtual, &in, &out);

    *virt_addr = (void *)(uintptr_t)out.VirtualAddress;

    return 0;
}

int mods_pio_read
(
    uint64_t addr,
    uint64_t *data,
    uint32_t data_size
)
{
    LwModsDrvIoReadIn in;
    LwModsDrvIoReadOut out;

    in.Address = addr;
    in.DataSize = data_size;

    CALL_CONNECT_METHOD(kLwModsDrvIoRead, &in, &out);

    *data = out.Data;

    return 0;
}

int mods_pio_write
(
    uint64_t addr,
    uint64_t data,
    uint32_t data_size
)
{
    LwModsDrvIoWriteIn in;

    in.Address = addr;
    in.Data = data;
    in.DataSize = data_size;

    CALL_CONNECT_METHOD_IN(kLwModsDrvIoWrite, &in);

    return 0;
}

int mods_set_console_info(uint32_t op)
{
    LwModsDrvSetConsoleInfoIn in;

    switch (op)
    {
    case MODS_CONSOLE_INFO_DISABLE:
        in.op = kLwModsConsoleInfoDisable;
        break;
    case MODS_CONSOLE_INFO_ENABLE:
        in.op = kLwModsConsoleInfoEnable;
        break;
    default:
        return -1;
    }

    CALL_CONNECT_METHOD_IN(kLwModsDrvSetConsoleInfo, &in);

    return 0;
}

int mods_register_interrupt
(
    uint32_t bus,
    uint32_t device,
    uint32_t function,
    uint32_t type,
    uint32_t index
)
{
    LwModsDrvRegisterInterruptIn in;

    CHECK_MODS();

    if (get_pci_path(bus, device, function, in.Path, sizeof(in.Path)) < 0)
    {
        return -1;
    }
    in.Type = type;
    in.Index = index;

    CALL_CONNECT_METHOD_IN(kLwModsDrvRegisterInterrupt, &in);

    return 0;
}

int mods_unregister_interrupt
(
    uint32_t bus,
    uint32_t device,
    uint32_t function,
    uint32_t type
)
{
    LwModsDrvUnregisterInterruptIn in;

    CHECK_MODS();

    if (get_pci_path(bus, device, function, in.Path, sizeof(in.Path)) < 0)
    {
        return -1;
    }
    in.Type = type;

    CALL_CONNECT_METHOD_IN(kLwModsDrvUnregisterInterrupt, &in);

    return 0;

}

int mods_create_interrupt_queue
(
    mach_port_t port,
    IODataQueueMemory **queue
)
{
    LwModsDrvCreateInterruptQueueOut out;
    IOReturn ret;

    CHECK_MODS();

    CALL_CONNECT_METHOD_OUT(kLwModsDrvCreateInterruptQueue, &out);

    *queue = (IODataQueueMemory *)(uintptr_t)out.QueueAddress;

    ret = IOConnectSetNotificationPort(s_mods->connect, kLwModsMachPortInterruptsQueue, port, 0);
    if (ret != kIOReturnSuccess)
    {
        PRINTF_IOERROR(ret, "failed to set notification port");
        return -1;
    }

    return 0;
}

int mods_release_interrupt_queue
(
)
{
    CHECK_MODS()

    CALL_CONNECT_METHOD_EMPTY(kLwModsDrvReleaseInterruptQueue);

    return 0;
}
