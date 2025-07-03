/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2007-2012,2014-2020 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

// Cross platform (XP) interface.

#include "core/include/xp.h"
#include "core/include/isrdata.h"
#include "core/include/tee.h"
#include "core/include/massert.h"
#include "core/include/globals.h"
#include "core/include/filesink.h"
#include "core/include/utility.h"
#include "core/include/script.h"
#include "core/include/jscript.h"
#include "core/include/platform.h"
#include "core/include/cpu.h"
#include "core/include/pool.h"
#include "core/include/tasker.h"
#include "core/include/evntthrd.h"
#include "core/include/tasker_p.h"
#include "rm.h"
#include "core/include/pci.h"
#include <map>
#include <sys/poll.h>
#include <sys/time.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/mman.h>
#include <unistd.h>
#include <signal.h>
#include <fcntl.h>
#include <pthread.h>

#if defined(INCLUDE_GPU)
#include "gpu/include/gpusbdev.h"
#include "core/include/mgrmgr.h"
#include "gpu/include/gpumgr.h"
#endif

#include "libmods.h"

#include "lwtypes.h"

#include "ctrl/ctrl2080.h"
#include "core/include/memcheck.h"

#include <mach/mach_port.h>
#include <IOKit/IOKitLib.h>
#include <IOKit/IODataQueueClient.h>

// Keeps information about memory mappings
static map<void *, pair<int, size_t> > s_MemoryMapping;

namespace
{
    struct Interrupt
    {
        UINT32 domain, bus, device, function;
        UINT32 type;
        std::vector<IsrData> hooks;
    };

    Tasker::ThreadID  s_IrqThread = -1;
    Tasker::Mutex     s_InterruptMutex   = Tasker::NoMutex();
    Tasker::Mutex     s_IrqRegisterMutex = Tasker::NoMutex();
    Platform::Irql    LwrrentIrql = Platform::NormalIrql;
    UINT32            IrqlNestCount = 0;
    IODataQueueMemory *s_IrqQueue = 0;
    mach_port_t       s_IrqPort = 0;

    static std::vector<Interrupt> s_Isrs;

    RC GetMemoryType(Memory::Attrib Attrib, MODS_ATTRIB* MemType)
    {
        switch (Attrib)
        {
            case Memory::UC:
                *MemType = MODS_ATTRIB_UNCACHED;
                return OK;
            case Memory::WC:
                *MemType = MODS_ATTRIB_WRITECOMBINE;
                return OK;
            case Memory::WT:
                *MemType = MODS_ATTRIB_WRITETHROUGH;
                return OK;
            case Memory::WP:
                *MemType = MODS_ATTRIB_WRITEPROTECT;
                return OK;
            case Memory::WB:
                *MemType = MODS_ATTRIB_CACHED;
                return OK;
            default:
                Printf(Tee::PriHigh, "Unrecognized memory type %d\n", static_cast<int>(Attrib));
                return RC::SOFTWARE_ERROR;
        }
    }

} // anonymous namespace

// function declaration
RC GetAPIVersion(UINT32 *pVersion);

static RC AlarmIrqHandler(UINT32 index)
{
    bool wasServiced = false;
    Tasker::MutexHolder lock(s_InterruptMutex);

    MASSERT(index < s_Isrs.size());
    const Interrupt & intr = s_Isrs[index];

    MASSERT(!intr.hooks.empty());

    for (UINT32 i = 0; i < intr.hooks.size(); ++i)
    {
        if (intr.hooks[i].GetIsr() == 0)
        {
            GpuDevMgr* const pGpuDevMgr = static_cast<GpuDevMgr*>(DevMgrMgr::d_GraphDevMgr);
            if (OK == pGpuDevMgr->ServiceInterrupt(intr.domain, intr.bus, intr.device, intr.function, ~0U))
            {
                wasServiced = true;
            }
        }
        else
        {
            if (Xp::ISR_RESULT_SERVICED == intr.hooks[i]())
            {
                wasServiced = true;
            }
        }
    }

    if (!wasServiced && intr.type == kLwModsInterruptTypeMsi)
    {
        Printf(Tee::PriLow, "Warning: Interrupt for device %04x:%x:%02x.%x not serviced\n",
                intr.domain, intr.bus, intr.device, intr.function);
    }

    return OK;
}

static void IrqThreadFunc(void*)
{
    IOReturn ret;
    LwModsInterrupt interrupt;
    uint32_t size = sizeof(interrupt);
    Tasker::DetachThread detach;

    while (true)
    {
        ret = IODataQueueDequeue(s_IrqQueue, &interrupt, &size);
        if (ret == kIOReturnSuccess)
        {
            if (size == 0)
            {
                // This message means that there is no more interrupts
                break;
            }
            else if (size == sizeof(interrupt))
            {
                if (OK != AlarmIrqHandler(interrupt.index))
                    break;
            }
            else
            {
                Printf(Tee::PriHigh, "Error: Incorrect size of interrupt event (%u instead of %lu)\n", size, sizeof(interrupt));
                break;
            }
        }
        else if (ret == kIOReturnUnderrun)
        {
            // No data in interrupt queue, wait for some
            ret = IODataQueueWaitForAvailableData(s_IrqQueue, s_IrqPort);
            if (ret != kIOReturnSuccess)
            {
                Printf(Tee::PriHigh, "Error: IODataQueueWaitForAvailableData returned %x\n", ret);
                return;
            }
        }
        else
        {
            Printf(Tee::PriHigh, "Error: IODataQueueDequeue returned %x\n", ret);
            break;
        }
    }
}

RC HookInterrupt(UINT32 pciDomain, UINT32 pciBus, UINT32 pciDevice, UINT32 pciFunction, ISR func, void* cookie, UINT32 type)
{
    Tasker::MutexHolder lock(s_IrqRegisterMutex);

    // Create thread only once, when first HookInterrupt is called
    if (s_Isrs.empty())
    {
        s_IrqPort = IODataQueueAllocateNotificationPort();
        if (s_IrqPort == MACH_PORT_NULL)
        {
            Printf(Tee::PriHigh, "Error: failed to create notification port\n");
            return RC::CANNOT_HOOK_INTERRUPT;
        }

        if (mods_create_interrupt_queue(s_IrqPort, &s_IrqQueue) != 0)
        {
            Printf(Tee::PriHigh, "Error: failed to set interrupt queue\n");
            mach_port_destroy(mach_task_self(), s_IrqPort);
            return RC::CANNOT_HOOK_INTERRUPT;
        }

        s_IrqThread = Tasker::CreateThread(IrqThreadFunc, 0, Tasker::MIN_STACK_SIZE, "interrupt");
    }

    {
        // Protect s_Isrs from being changed while IrqThreadFunc uses it
        Tasker::MutexHolder lock(s_InterruptMutex);

        // Find device or empty space
        UINT32 index, free = s_Isrs.size();
        for (index = 0; index < s_Isrs.size(); ++index)
        {
            if (s_Isrs[index].hooks.empty())
            {
                free = index;
            }
            else if (s_Isrs[index].domain == pciDomain &&
                s_Isrs[index].bus == pciBus &&
                s_Isrs[index].device == pciDevice &&
                s_Isrs[index].function == pciFunction)
            {
                break;
            }

        }

        // Register new interrupt
        if (index == s_Isrs.size())
        {
            // Use free space from unregistered interrupt
            index = free;

            Interrupt interrupt;
            interrupt.type = type;
            interrupt.domain = pciDomain;
            interrupt.bus = pciBus;
            interrupt.device = pciDevice;
            interrupt.function = pciFunction;

            if (mods_register_interrupt(pciBus, pciDevice, pciFunction, type, index) != 0)
            {
                Printf(Tee::PriHigh, "Error: failed to register interrupt\n");
                return RC::CANNOT_HOOK_INTERRUPT;
            }

            if (index == s_Isrs.size())
                s_Isrs.push_back(interrupt);
            else
                s_Isrs[index] = interrupt;
        }
        else
        {
            MASSERT(s_Isrs[index].type == type);

            // Do not allow the same handlers
            for (UINT32 i = 0; i < s_Isrs[index].hooks.size(); ++i)
            {
                if (s_Isrs[index].hooks[i] == IsrData(func, cookie))
                {
                    Printf(Tee::PriHigh, "Error: hook already registered for this device\n");
                    return RC::CANNOT_HOOK_INTERRUPT;
                }
            }
        }

        // Add handler
        s_Isrs[index].hooks.push_back(IsrData(func, cookie));
    }

    return OK;
}

RC UnhookInterrupt(UINT32 pciDomain, UINT32 pciBus, UINT32 pciDevice, UINT32 pciFunction, ISR func, void* cookie, UINT32 type)
{
    Tasker::MutexHolder lock(s_IrqRegisterMutex);

    {
        // Protect s_Isrs from being changed while IrqThreadFunc uses it
        Tasker::MutexHolder lock(s_InterruptMutex);

        UINT32 i;
        for (i = 0; i < s_Isrs.size(); ++i)
        {
            if (s_Isrs[i].domain == pciDomain &&
                s_Isrs[i].bus == pciBus &&
                s_Isrs[i].device == pciDevice &&
                s_Isrs[i].function == pciFunction)
            {
                break;
            }
        }

        if (i == s_Isrs.size())
        {
            Printf(Tee::PriHigh, "Error: interrupt not found\n");
            return RC::CANNOT_UNHOOK_ISR;
        }

        MASSERT(s_Isrs[i].type == type);

        // Remove handler
        for (UINT32 j = 0; j < s_Isrs[i].hooks.size(); ++j)
        {
            if (s_Isrs[i].hooks[j] == IsrData(func, cookie))
            {
                // Swap with last hook to remove
                s_Isrs[i].hooks[j] = s_Isrs[i].hooks.back();
                s_Isrs[i].hooks.pop_back();
                break;
            }
        }

        // Unregister interrupt if it has not other hooks
        if (s_Isrs[i].hooks.empty())
        {
            if (mods_unregister_interrupt(pciBus, pciDevice, pciFunction, type) != 0)
            {
                return RC::CANNOT_UNHOOK_ISR;
            }
        }

        // Remove not needed entries
        while (!s_Isrs.empty() && s_Isrs.back().hooks.empty())
        {
            s_Isrs.pop_back();
        }

        // Wait for the thread if there are no interrupts left.
        // Thread will stop because it will be communicated by kernel
        // that there is no more work to do
        if (s_Isrs.empty())
        {
            Tasker::Join(s_IrqThread);

            if (mods_release_interrupt_queue() != 0)
            {
                Printf(Tee::PriHigh, "Warning: interrupt unhooked but failed to release interrupt queue\n");
            }

            mach_port_destroy(mach_task_self(), s_IrqPort);
        }
    }

    return OK;
}

P_(Global_Get_KernelModuleVersion);
static SProperty Global_KernelModuleVersion
(
    "KernelModuleVersion",
    0,
    Global_Get_KernelModuleVersion,
    0,
    0,
    "Version of the MODS kernel module."
);

P_(Global_Get_KernelModuleVersion)
{
    MASSERT(pContext != 0);
    MASSERT(pValue   != 0);

    UINT32 Version;
    if (OK != GetAPIVersion(&Version))
    {
        *pValue = JSVAL_NULL;
        return JS_FALSE;
    }
    char VersionString[16];
    sprintf(VersionString, "%x.%02x", (Version>>8), (Version&0xFF));
    if (OK != JavaScriptPtr()->ToJsval(string(VersionString), pValue))
    {
        *pValue = JSVAL_NULL;
        return JS_FALSE;
    }

    return JS_TRUE;
}

RC GetAPIVersion(UINT32* pVersion)
{
    uint32_t kextVersion;
    if (mods_get_api_version(&kextVersion) != 0)
    {
        return RC::SOFTWARE_ERROR;
    }
    *pVersion = (UINT32)kextVersion;
    return OK;
}

RC InitializeKernelCalls()
{
    MASSERT(!s_InterruptMutex.get());
    MASSERT(!s_IrqRegisterMutex.get());
    s_InterruptMutex = Tasker::CreateMutex("Xp::s_InterruptMutex",
                                           Tasker::mtxUnchecked);
    s_IrqRegisterMutex = Tasker::CreateMutex("Xp::s_IrqRegisterMutex",
                                             Tasker::mtxUnchecked);

    return OK;
}

RC FinalizeKernelCalls()
{
    MASSERT(s_InterruptMutex.get());
    MASSERT(s_IrqRegisterMutex.get());
    s_InterruptMutex   = Tasker::NoMutex();
    s_IrqRegisterMutex = Tasker::NoMutex();

    return OK;
}

//------------------------------------------------------------------------------
// Get the Linux kernel version a.b.c
// returns ((a) << 16 + (b) << 8 + (c))
// -----------------------------------------------------------------------------
RC Xp::GetOSVersion(UINT64* pVer)
{
    // TODO: Unsupported function?
    *pVer = 0x1060;
    return OK;
}

RC Xp::AllocPages
(
    size_t         numBytes,
    void **        pDescriptor,
    bool           contiguous,
    UINT32         addressBits,
    Memory::Attrib attrib,
    UINT32         domain,
    UINT32         bus,
    UINT32         device,
    UINT32         function
)
{
    return AllocPages(numBytes, pDescriptor, contiguous, addressBits, attrib, ~0U);
}

//------------------------------------------------------------------------------
//! \brief Alloc system memory.
//!
//! \param NumBytes    : Number of bytes to allocate (actual allocation will be
//!                      a whole number of pages (NumBytes will be rounded up).
//! \param pDescriptor : Returned descriptor for the allocated memory
//! \param Contiguous  : If true, physical pages will be adjacent and in-order.
//! \param AddressBits : All pages will be locked and will be below physical
//!                      address 2**AddressBits.
//! \param Attrib      : Memory attributes to use when allocating
//! \param GpuInst     : IGNORED (For other platforms, if GpuInst is valid, then
//!                      on a NUMA system the system memory will be on the node
//!                      local to the GPU)
//!
//! \return OK if memory was successfully allocated and mapped, not OK otherwise
RC Xp::AllocPages
(
    size_t         NumBytes,
    void **        pDescriptor,
    bool           Contiguous,
    UINT32         AddressBits,
    Memory::Attrib Attrib,
    UINT32         GpuInst
)
{
    LwU64   *p;
    MODS_ATTRIB MemoryType = MODS_ATTRIB_CACHED;
    RC rc;

    CHECK_RC(GetMemoryType(Attrib, &MemoryType));

    p = (LwU64 *)ExecMalloc(sizeof (LwU64));
    if (p == NULL)
    {
        Printf(Tee::PriHigh, "Error: Can't allocate memory for pointer\n");
        return RC::SOFTWARE_ERROR;
    }

    if (mods_alloc_pages(NumBytes, Contiguous, AddressBits, MemoryType, p) < 0)
    {
        Printf(Tee::PriHigh, "Error: Can't allocate system pages\n");
        return RC::CANNOT_ALLOCATE_MEMORY;
    }

    *pDescriptor = p;

    return OK;
}

//------------------------------------------------------------------------------
//! \brief Alloc contiguous system memory with pages aligned as requested
//!
//! \param NumBytes    : Number of bytes to allocate (actual allocation will be
//!                      a whole number of pages (NumBytes will be rounded up).
//! \param pDescriptor : Returned descriptor for the allocated memory
//! \param Contiguous  : If true, physical pages will be adjacent and in-order.
//! \param AddressBits : All pages will be locked and will be below physical
//!                      address 2**AddressBits.
//! \param Attrib      : Memory attributes to use when allocating
//! \param GpuInst     : IGNORED (For other platforms, if GpuInst is valid, then
//!                      on a NUMA system the system memory will be on the node
//!                      local to the GPU)
//!
//! \return OK if memory was successfully allocated and mapped, not OK otherwise
RC Xp::AllocPagesAligned
(
    size_t         NumBytes,
    void **        pDescriptor,
    size_t         PhysicalAlignment,
    UINT32         AddressBits,
    Memory::Attrib Attrib,
    UINT32         GpuInst
)
{
    return AllocPages(NumBytes, pDescriptor, true, AddressBits, Attrib, GpuInst);
}

//------------------------------------------------------------------------------
//!< Free pages allocated by AllocPages or AllocPagesAligned.
void Xp::FreePages(void * Descriptor)
{
    if (mods_free_pages(*((uint64_t *)Descriptor)) < 0)
    {
        Printf(Tee::PriHigh, "Error: Can't free system pages\n");
    }

    ExecFree(Descriptor, sizeof(LwU64));
}

RC Xp::MergePages(void** pOutDescriptor, void** pInDescBegin, void** pInDescEnd)
{
    MASSERT(0);
    return RC::UNSUPPORTED_FUNCTION;
}

// TODO: We ignore Protect for now.
#if 0
static INT32 TranslateProtect(Memory::Protect Protect)
{
    INT32 retProtect = 0;

    if (Protect & Memory::Readable)
        retProtect |= PROT_READ;
    if (Protect & Memory::Writeable)
        retProtect |= PROT_WRITE;
    if (Protect & Memory::Exelwtable)
        retProtect |= PROT_EXEC;

    return retProtect;
}
#endif

//------------------------------------------------------------------------------
RC Xp::MapPages
(
    void **pReturnedVirtualAddress,
    void * Descriptor,
    size_t Offset,
    size_t Size,
    Memory::Protect Protect
)
{
    // TODO: We ignore Protect
    //INT32 mmapProtect       = TranslateProtect(Protect);
    //Printf(Tee::PriHigh, "Protect = 0x%x\n", Protect);

    if (mods_map_pages(*((LwU64 *)Descriptor), Offset, Size, pReturnedVirtualAddress) < 0)
    {
        Printf(Tee::PriHigh, "Error: Can't map system pages\n");
        return RC::SOFTWARE_ERROR;
    }

    if (s_MemoryMapping.find(*pReturnedVirtualAddress) != s_MemoryMapping.end())
    {
        s_MemoryMapping[*pReturnedVirtualAddress].first = s_MemoryMapping[*pReturnedVirtualAddress].first + 1;
        MASSERT(s_MemoryMapping[*pReturnedVirtualAddress].second == Size);
        return OK;
    }

    s_MemoryMapping[*pReturnedVirtualAddress].first = 1;
    s_MemoryMapping[*pReturnedVirtualAddress].second = Size;

    Pool::AddNonPoolAlloc(*pReturnedVirtualAddress, Size);

    return OK;
}

//------------------------------------------------------------------------------
void Xp::UnMapPages(void * VirtualAddress)
{
    // Allow unmaps of NULL
    if (!VirtualAddress)
    {
        return;
    }

    // Look up the size of the mapping
    MASSERT(s_MemoryMapping.find(VirtualAddress) != s_MemoryMapping.end());
    if (s_MemoryMapping[VirtualAddress].first > 1)
    {
        s_MemoryMapping[VirtualAddress].first = s_MemoryMapping[VirtualAddress].first - 1;
        return;
    }

    //void   *virtualAddressOrginal = s_MemoryMapping[VirtualAddress].first;
    size_t  NumBytes              = s_MemoryMapping[VirtualAddress].second;

    s_MemoryMapping.erase(VirtualAddress);

    // Unmap it
    Pool::RemoveNonPoolAlloc(VirtualAddress, NumBytes);
    mods_unmap_mem(VirtualAddress, NumBytes);
}

//------------------------------------------------------------------------------
PHYSADDR Xp::GetPhysicalAddress(void *Descriptor, size_t Offset)
{
    LwU64 PhysicalAddress;

    if (mods_get_phys_addr(*((LwU64 *)Descriptor), Offset, &PhysicalAddress) < 0)
    {
        Printf(Tee::PriHigh, "Error: Can't get physical address of given allocation\n");
        return 0;
    }

    return PhysicalAddress;
}

//------------------------------------------------------------------------------
bool Xp::CanEnableIova
(
    UINT32 domain,
    UINT32 bus,
    UINT32 device,
    UINT32 func
)
{
    return false;
}

//------------------------------------------------------------------------------
PHYSADDR Xp::GetMappedPhysicalAddress
(
    UINT32 domain,
    UINT32 bus,
    UINT32 device,
    UINT32 func,
    void  *descriptor,
    size_t offset
)
{
    return GetPhysicalAddress(descriptor, offset);
}

//------------------------------------------------------------------------------
RC Xp::DmaMapMemory
(
    UINT32 domain,
    UINT32 bus,
    UINT32 device,
    UINT32 func,
    void  *descriptor
)
{
    MASSERT(0);
    return RC::UNSUPPORTED_FUNCTION;
}

//------------------------------------------------------------------------------
RC Xp::DmaUnmapMemory
(
    UINT32 domain,
    UINT32 bus,
    UINT32 device,
    UINT32 func,
    void  *descriptor
)
{
    MASSERT(0);
    return RC::UNSUPPORTED_FUNCTION;
}

//------------------------------------------------------------------------------
RC Xp::SetDmaMask(UINT32 domain, UINT32 bus, UINT32 device, UINT32 func, UINT32 numBits)
{
    return RC::UNSUPPORTED_FUNCTION;
}

//------------------------------------------------------------------------------
// Colwert physical address to virtual address.
//
void * Xp::PhysicalToVirtual(PHYSADDR PhysicalAddress, Tee::Priority pri)
{
    void *VirtualAddress;

    if (mods_phys_to_virt(PhysicalAddress, &VirtualAddress) < 0)
    {
        Printf(pri, "Can't get virtual address of given physical\n");
        return 0;
    }

    return (void*)VirtualAddress;
}

//------------------------------------------------------------------------------
// Colwert virtual address to physical address.
//
PHYSADDR Xp::VirtualToPhysical(volatile void * VirtualAddress, Tee::Priority pri)
{
    LwU64 PhysicalAddress;

    if (mods_virt_to_phys(const_cast<void *>(VirtualAddress),
                          &PhysicalAddress) < 0)
    {
        Printf(pri,
               "Unable to colwert virtual address %p to physical\n",
               VirtualAddress);
        return 0;
    }

    return PhysicalAddress;
}

//------------------------------------------------------------------------------
RC Xp::SetupDmaBase
(
    UINT16           domain,
    UINT08           bus,
    UINT08           dev,
    UINT08           func,
    PpcTceBypassMode bypassMode,
    UINT64           devDmaMask,
    PHYSADDR *       pDmaBaseAddr
)
{
    MASSERT(pDmaBaseAddr);
    Printf(Tee::PriLow,
           "Setup dma base not supported on current platform skipping\n");
    *pDmaBaseAddr = static_cast<PHYSADDR>(0);
    return OK;
}

//-----------------------------------------------------------------------------
RC Xp::SetLwLinkSysmemTrained
(
    UINT32 domain,
    UINT32 bus,
    UINT32 dev,
    UINT32 func,
    bool   trained
)
{
    Printf(Tee::PriLow, "SetLwLinkSysmemTrained not supported on current platform, skipping\n");
    return OK;
}

//!< Map device memory into the page table, get virtual address to use.
//!< Multiple mappings are legal, and give different virtual addresses
//!< on paging systems.
RC Xp::MapDeviceMemory
(
    void **     pReturnedVirtualAddress,
    PHYSADDR    PhysicalAddress,
    size_t      NumBytes,
    Memory::Attrib Attrib,
    Memory::Protect Protect
)
{
    MODS_ATTRIB MemoryType = MODS_ATTRIB_CACHED;
    RC rc;
    CHECK_RC(GetMemoryType(Attrib, &MemoryType));

    if (mods_map_dev_mem(PhysicalAddress, NumBytes, MemoryType, pReturnedVirtualAddress) < 0)
    {
        Printf(Tee::PriHigh, "Error: Can't map device memory\n");
        return RC::SOFTWARE_ERROR;
    }

    if (s_MemoryMapping.find(*pReturnedVirtualAddress) != s_MemoryMapping.end())
    {
        s_MemoryMapping[*pReturnedVirtualAddress].first = s_MemoryMapping[*pReturnedVirtualAddress].first + 1;
        MASSERT(s_MemoryMapping[*pReturnedVirtualAddress].second == NumBytes);
        return OK;
    }
    s_MemoryMapping[*pReturnedVirtualAddress].first = 1;
    s_MemoryMapping[*pReturnedVirtualAddress].second = NumBytes;

    Pool::AddNonPoolAlloc(*pReturnedVirtualAddress, NumBytes);

    return OK;
}

//------------------------------------------------------------------------------
//!< Undo the mapping created by MapDeviceMemory.
void Xp::UnMapDeviceMemory(void * VirtualAddress)
{
    // Allow unmaps of NULL
    if (!VirtualAddress)
    {
        return;
    }

    // Look up the size of the mapping
    MASSERT(s_MemoryMapping.find(VirtualAddress) != s_MemoryMapping.end());
    if (s_MemoryMapping[VirtualAddress].first > 1)
    {
        s_MemoryMapping[VirtualAddress].first = s_MemoryMapping[VirtualAddress].first - 1;
        return;
    }

    //void   *virtualAddressOrginal = s_MemoryMapping[VirtualAddress].first;
    size_t  NumBytes              = s_MemoryMapping[VirtualAddress].second;

    s_MemoryMapping.erase(VirtualAddress);

    // Unmap it
    Pool::RemoveNonPoolAlloc(VirtualAddress, NumBytes);
    mods_unmap_mem(VirtualAddress, NumBytes);
}

///------------------------------------------------------------------------------
// Find a PCI device with given 'DeviceId', 'VendorId', and 'Index'.
// Return '*pBusNumber', '*pDeviceNumber', and '*pFunctionNumber'.
//
RC Xp::FindPciDevice
(
    INT32   DeviceId,
    INT32   VendorId,
    INT32   Index,
    UINT32* pDomainNumber   /* = 0 */,
    UINT32* pBusNumber      /* = 0 */,
    UINT32* pDeviceNumber   /* = 0 */,
    UINT32* pFunctionNumber /* = 0 */
)
{
#if defined(SKIP_PCI_CALLS)
    return RC::UNSUPPORTED_FUNCTION;
#else
    LwU8 bus, dev, func;

    if (mods_find_pci_device(DeviceId, VendorId, Index, &bus, &dev, &func) < 0)
    {
        Printf(Tee::PriHigh, "Error: Can't find Pci device\n");
        return RC::PCI_DEVICE_NOT_FOUND;
    }

    *pDomainNumber = 0;
    *pBusNumber = bus;
    *pDeviceNumber = dev;
    *pFunctionNumber = func;

    return OK;
#endif
}

//------------------------------------------------------------------------------
// @param ClassCode The entire 24 bit register baseclass/subclass/pgmIF,
//                  a byte for each field
//
RC Xp::FindPciClassCode
(
    INT32   ClassCode,
    INT32   Index,
    UINT32* pDomainNumber   /* = 0 */,
    UINT32* pBusNumber      /* = 0 */,
    UINT32* pDeviceNumber   /* = 0 */,
    UINT32* pFunctionNumber /* = 0 */
)
{
#if defined(SKIP_PCI_CALLS)
    return RC::UNSUPPORTED_FUNCTION;
#else
    LwU8 bus, dev, func;

    if (mods_find_pci_class_code(ClassCode, Index, &bus, &dev, &func) < 0)
    {
        Printf(Tee::PriDebug, "Error: Can't find Pci device with class code 0x%x, index %d \n",
                                                    ClassCode, Index);
        return RC::PCI_DEVICE_NOT_FOUND;
    }

    *pDomainNumber = 0;
    *pBusNumber = bus;
    *pDeviceNumber = dev;
    *pFunctionNumber = func;

    return OK;
#endif
}

// Force a scan of PCI bus.
RC Xp::RescanPciBus
(
    INT32 domain,
    INT32 bus
)
{
    return RC::UNSUPPORTED_FUNCTION;
}

//------------------------------------------------------------------------------
RC Xp::PciResetFunction
(
    INT32 DomainNumber,
    INT32 BusNumber,
    INT32 DeviceNumber,
    INT32 FunctionNumber
)
{
    return RC::UNSUPPORTED_FUNCTION;
}

//------------------------------------------------------------------------------
// Return Base Address and Size for specified GPU BAR.
RC Xp::PciGetBarInfo
(
    INT32 DomainNumber,
    INT32 BusNumber,
    INT32 DeviceNumber,
    INT32 FunctionNumber,
    INT32 BarIndex,
    UINT64* pBaseAddress,
    UINT64* pBarSize
)
{
    return Pci::GetBarInfo(DomainNumber, BusNumber,
                           DeviceNumber, FunctionNumber,
                           BarIndex, pBaseAddress, pBarSize);
}

//------------------------------------------------------------------------------
// Return the IRQ number for the specified GPU.
RC Xp::PciGetIRQ
(
    INT32 DomainNumber,
    INT32 BusNumber,
    INT32 DeviceNumber,
    INT32 FunctionNumber,
    UINT32* pIrq
)
{
    return Pci::GetIRQ(DomainNumber, BusNumber,
                       DeviceNumber, FunctionNumber, pIrq);
}

//-----------------------------------------------------------------------------
// Implementation for Xp::PciRead(08,16,32)
//
RC static PciRead
(
    INT32    DomainNumber,
    INT32    BusNumber,
    INT32    DeviceNumber,
    INT32    FunctionNumber,
    INT32    Address,
    UINT32 * pData,
    INT32    DataSize
)
{
#if defined(SKIP_PCI_CALLS)
    return RC::UNSUPPORTED_FUNCTION;
#else
    uint32_t data;
    if (mods_pci_read(BusNumber, DeviceNumber, FunctionNumber,
                      Address, &data, DataSize) < 0)
    {
        *pData = 0xffffffff;
        return RC::PCI_DEVICE_NOT_FOUND;
    }

    *pData = data;
    return OK;
#endif
}

//------------------------------------------------------------------------------
// Read byte, word, or dword at 'Address' from a PCI device with the given
// 'BusNumber', 'DeviceNumber', and 'FunctionNumber'.
// Return read data in '*pData'.
//
RC Xp::PciRead08
(
    INT32    DomainNumber,
    INT32    BusNumber,
    INT32    DeviceNumber,
    INT32    FunctionNumber,
    INT32    Address,
    UINT08 * pData
)
{
    RC rc;
    UINT32 Data;

    rc = PciRead(DomainNumber, BusNumber, DeviceNumber, FunctionNumber, Address, &Data, sizeof(UINT08));
    *pData = Data;

    return rc;
}

RC Xp::PciRead16
(
    INT32    DomainNumber,
    INT32    BusNumber,
    INT32    DeviceNumber,
    INT32    FunctionNumber,
    INT32    Address,
    UINT16 * pData
)
{
    RC rc;
    UINT32 Data;

    rc = PciRead(DomainNumber, BusNumber, DeviceNumber, FunctionNumber, Address, &Data, sizeof(UINT16));
    *pData = Data;

    return rc;
}

RC Xp::PciRead32
(
    INT32    DomainNumber,
    INT32    BusNumber,
    INT32    DeviceNumber,
    INT32    FunctionNumber,
    INT32    Address,
    UINT32 * pData
)
{
    RC rc;
    UINT32 Data;

    rc = PciRead(DomainNumber, BusNumber, DeviceNumber, FunctionNumber, Address, &Data, sizeof(UINT32));
    *pData = Data;

    return rc;
}

//-----------------------------------------------------------------------------
// Implementation for Xp::PciWrite(08,16,32)
//
RC static PciWrite
(
    INT32    DomainNumber,
    INT32    BusNumber,
    INT32    DeviceNumber,
    INT32    FunctionNumber,
    INT32    Address,
    UINT32   Data,
    INT32    DataSize
)
{
#if defined(SKIP_PCI_CALLS)
    return RC::UNSUPPORTED_FUNCTION;
#else
    if (mods_pci_write(BusNumber, DeviceNumber, FunctionNumber, Address, Data, DataSize) < 0)
    {
        Printf(Tee::PriHigh, "Error: Can't write to PCI\n");
        return RC::PCI_DEVICE_NOT_FOUND;
    }

    return OK;
#endif
}

//------------------------------------------------------------------------------
// Write 'Data' at 'Address' to a PCI device with the given
// 'BusNumber', 'DeviceNumber', and 'FunctionNumber'.
//
RC Xp::PciWrite08
(
    INT32  DomainNumber,
    INT32  BusNumber,
    INT32  DeviceNumber,
    INT32  FunctionNumber,
    INT32  Address,
    UINT08 Data
)
{
    return PciWrite(DomainNumber, BusNumber, DeviceNumber, FunctionNumber, Address, Data, sizeof(Data));
}

RC Xp::PciWrite16
(
    INT32  DomainNumber,
    INT32  BusNumber,
    INT32  DeviceNumber,
    INT32  FunctionNumber,
    INT32  Address,
    UINT16 Data
)
{
    return PciWrite(DomainNumber, BusNumber, DeviceNumber, FunctionNumber, Address, Data, sizeof(Data));
}

RC Xp::PciWrite32
(
   INT32  DomainNumber,
   INT32  BusNumber,
   INT32  DeviceNumber,
   INT32  FunctionNumber,
   INT32  Address,
   UINT32 Data
)
{
    return PciWrite(DomainNumber, BusNumber, DeviceNumber, FunctionNumber, Address, Data, sizeof(Data));
}

RC Xp::PciRemove
(
    INT32    domainNumber,
    INT32    busNumber,
    INT32    deviceNumber,
    INT32    functionNumber
)
{
    return RC::UNSUPPORTED_FUNCTION;
}

// Interrupt functions.
//

bool Xp::IRQTypeSupported(UINT32 irqType)
{
    switch (irqType)
    {
        case MODS_XP_IRQ_TYPE_INT:
            return true;
        case MODS_XP_IRQ_TYPE_MSI:
            return true;
        case MODS_XP_IRQ_TYPE_CPU:
            return true;
        case MODS_XP_IRQ_TYPE_MSIX:
            return false;
        default:
            Printf(Tee::PriWarn, "Unknown interrupt type %d\n", irqType);
            break;
    }

    return false;
}

void Xp::ProcessPendingInterrupts()
{
}

RC Xp::MapIRQ(UINT32 * logicalIrq, UINT32 physIrq, string dtName, string fullName)
{
    return RC::UNSUPPORTED_FUNCTION;
}

RC Xp::HookIRQ(UINT32 Irq, ISR func, void* cookie)
{
    return RC::UNSUPPORTED_FUNCTION;
}

RC Xp::UnhookIRQ(UINT32 Irq, ISR func, void* cookie)
{
    return RC::UNSUPPORTED_FUNCTION;
}

RC Xp::HookInt(const IrqParams& irqInfo, ISR func, void* cookie)
{
    return RC::UNSUPPORTED_FUNCTION;
}

RC Xp::UnhookInt(const IrqParams& irqInfo, ISR func, void* cookie)
{
    return UnhookInterrupt(irqInfo.pciDev.domain, irqInfo.pciDev.bus,
                           irqInfo.pciDev.device, irqInfo.pciDev.function,
                           func, cookie, kLwModsInterruptTypeInt);
}

void Xp::EnableInterrupts()
{
    MASSERT(IrqlNestCount > 0);
    IrqlNestCount--;
    if (IrqlNestCount == 0)
    {
        LwrrentIrql = Platform::NormalIrql;
    }
    else
    {
        MASSERT(0);
    }

    Tasker::ReleaseMutex(s_InterruptMutex.get());
}

void Xp::DisableInterrupts()
{
    Tasker::AcquireMutex(s_InterruptMutex.get());
    IrqlNestCount++;
    if (IrqlNestCount == 1)
    {
        LwrrentIrql = Platform::ElevatedIrql;
    }
    else
    {
        MASSERT(!"Nesting DisableInterrupts may cause a deadlock!");
    }
}

Platform::Irql Xp::GetLwrrentIrql()
{
    return LwrrentIrql;
}

Platform::Irql Xp::RaiseIrql(Platform::Irql NewIrql)
{
    Tasker::MutexHolder lock(s_InterruptMutex);

    Platform::Irql OldIrql = GetLwrrentIrql();
    MASSERT(NewIrql >= OldIrql);

    if ((NewIrql == Platform::ElevatedIrql) &&
        (NewIrql != OldIrql))
    {
        DisableInterrupts();
    }

    return OldIrql;
}

void Xp::LowerIrql(Platform::Irql NewIrql)
{
    Tasker::MutexHolder lock(s_InterruptMutex);

    MASSERT(NewIrql <= GetLwrrentIrql());
    if ((NewIrql == Platform::NormalIrql) &&
        (NewIrql != GetLwrrentIrql()))
    {
        EnableInterrupts();
    }
}

//-----------------------------------------------------------------------------
// Implementation for Xp::PioRead(08,16,32)
//
RC static PioRead
(
    UINT16  Port,
    UINT32  *pData,
    INT32   DataSize
)
{
    LwU64 data;

    if (mods_pio_read(Port, &data, DataSize) < 0)
    {
        Printf(Tee::PriError, "Error: Can't read from Port\n");
        return RC::SOFTWARE_ERROR;

    }

    *pData = (UINT32)data;

    return OK;
}

//------------------------------------------------------------------------------
// Port read and write functions.
//
RC Xp::PioRead08(UINT16 Port, UINT08 * pData)
{
    RC rc;
    UINT32 Data;

    rc = PioRead(Port, &Data, 1);

    *pData = (UINT08)Data;
    return rc;
}

RC Xp::PioRead16(UINT16 Port, UINT16 * pData)
{
    RC rc;
    UINT32 Data;

    rc = PioRead(Port, &Data, 2);

    *pData = (UINT16)Data;
    return rc;
}

RC Xp::PioRead32(UINT16 Port, UINT32 * pData)
{
    RC rc;
    UINT32 Data;

    rc = PioRead(Port, &Data, 4);

    *pData = Data;
    return rc;
}

//-----------------------------------------------------------------------------
// Implementation for Xp::PioWrite(08,16,32)
//
RC static PioWrite
(
    UINT16  Port,
    UINT32  Data,
    INT32   DataSize
)
{
    if (mods_pio_write(Port, Data, DataSize) < 0)
    {
        Printf(Tee::PriError, "Error: Can't write to Port\n");
        return RC::SOFTWARE_ERROR;

    }

    return OK;
}

RC Xp::PioWrite08(UINT16 Port, UINT08 Data)
{
    return PioWrite(Port, Data, 1);
}

RC Xp::PioWrite16(UINT16 Port, UINT16 Data)
{
    return PioWrite(Port, Data, 2);
}

RC Xp::PioWrite32(UINT16 Port, UINT32 Data)
{
    return PioWrite(Port, Data, 4);
}

//------------------------------------------------------------------------------
RC Xp::CallACPIGeneric(UINT32 GpuInst, UINT32 Function, void *Param1, void *Param2, void *Param3)
{
    return RC::UNSUPPORTED_FUNCTION;
}

//------------------------------------------------------------------------------
RC Xp::LWIFMethod
(
    UINT32 Function,
    UINT32 SubFunction,
    void   *InParams,
    UINT16 InParamSize,
    UINT32 *OutStatus,
    void   *OutData,
    UINT16 *OutDataSize
)
{
    return RC::UNSUPPORTED_FUNCTION;
}

//------------------------------------------------------------------------------
#if defined(INCLUDE_GPU)
RC Xp::ValidateGpuInterrupt(GpuSubdevice  *pGpuSubdevice, UINT32 whichInterrupts)
{
    UINT32 intrToCheck = 0;
    if (whichInterrupts & IntaIntrCheck)
    {
        intrToCheck |= GpuSubdevice::HOOKED_INTA;
    }
    if ((whichInterrupts & MsiIntrCheck))
    {
        intrToCheck |= GpuSubdevice::HOOKED_MSI;
    }
    RC rc = pGpuSubdevice->ValidateInterrupts(intrToCheck);
    Tee::FlushToDisk();
    return rc;
}
#endif
