/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2013-2020 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

// Cross platform (XP) interface.

#include "core/include/xp.h"
#include "core/include/massert.h"
#include "core/include/pool.h"
#include "cheetah/include/clocklog.h"
#include "core/include/pci.h"

#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>

#define WE_NEED_LINUX_XP
#include "linux/xp_linux_internal.h"

namespace
{
    //!
    //! @struct MemoryBlockDescriptor
    //! @brief meta data which stores the length of block and its physical
    //!        address
    struct MemoryBlockDescriptor
    {
        size_t numBytes;
        PHYSADDR physicalAddress;
    };

    //!
    //! @class VirtualMemoryBookKeeper
    //! @brief Virtual memory to physical memory book keeper
    //
    //! Book keeper class to cache virtual to physical memory and reverse mappings.
    //! Prevents frequent OS API call ilwocations, by caching mappings and also
    //! keeps meta data about virtual memory regions mapped or contigous memory
    //! blocks which are allocated by OS. This information is also required when its
    //! time to free these mappings
    //!
    class VirtualMemoryBookKeeper
    {
        public:
            VirtualMemoryBookKeeper() = default;
            ~VirtualMemoryBookKeeper() = default;

            //! @brief Cache virtual address to physical address and other meta data.
            //!        Also create reverse mapping from physical to virtual address
            void CacheMapping
            (
                void* virtualAddress
               ,const MemoryBlockDescriptor descriptor
            )
            {
                MASSERT(virtualAddress);
                MASSERT(descriptor.numBytes);
                MASSERT(descriptor.physicalAddress);

                m_VirtualPhysicalMapping[virtualAddress] = descriptor;
                m_PhysicalVirtualMapping[descriptor.physicalAddress]= virtualAddress;
            };

            //! @brief remove a cached mapping from virtual to physical address
            //!        as well as the reverse mapping.
            void RemoveMapping(void* virtualAddress)
            {
                PHYSADDR physAddr = 0;

                if (m_VirtualPhysicalMapping.count(virtualAddress))
                {
                    physAddr = m_VirtualPhysicalMapping[virtualAddress].physicalAddress;
                    m_VirtualPhysicalMapping.erase(virtualAddress);
                }

                if (m_PhysicalVirtualMapping.count(physAddr))
                {
                    m_PhysicalVirtualMapping.erase(physAddr);
                }
            }

            //! @brief get the length of a memory region associated with a virtual
            //!        address
            //! @return the number of bytes mapped for a virtual memory mapping
            //!         return 0 if mapping is not found
            ssize_t GetNumBytes(void* virtualAddress)
            {
               MASSERT(virtualAddress);
               ssize_t len = 0;

               if (m_VirtualPhysicalMapping.count(virtualAddress))
               {
                  len = m_VirtualPhysicalMapping[virtualAddress].numBytes;
               }

               return len;
            }

            //! @brief get the physical address for which the virtual address
            //!        is mapped.
            //! @return the physical address from the reverse cache if valid, return
            //!         0 otherwise.
            PHYSADDR GetCachedPhysicalAddress(void* const virtualAddress)
            {
                MASSERT(virtualAddress);
                PHYSADDR physAddr = 0;

                if (m_VirtualPhysicalMapping.count(virtualAddress))
                {
                    physAddr = m_VirtualPhysicalMapping[virtualAddress].physicalAddress;
                }

                return physAddr;
            }

            //! @brief get the cached virtual address for a mapped physical address
            void* GetCachedVirtualAddress(const PHYSADDR physicalAddress)
            {
                MASSERT(physicalAddress);
                void* addr = nullptr;

                if (m_PhysicalVirtualMapping.count(physicalAddress))
                {
                    addr = m_PhysicalVirtualMapping[physicalAddress];
                }

                return addr;
            }

        private:
            map<void*, MemoryBlockDescriptor> m_VirtualPhysicalMapping;
            map<PHYSADDR, void*> m_PhysicalVirtualMapping;
    };

    VirtualMemoryBookKeeper s_MemoryBookkeeper;
    constexpr UINT32 EMEM_BASE_START = 0x80000000;
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
//! \param numBytes    : Number of bytes to allocate (actual allocation will be
//!                      a whole number of pages (numBytes will be rounded up).
//! \param pDescriptor : Returned descriptor for the allocated memory
//! \param contig      : If true, physical pages will be adjacent and in-order.
//! \param addressBits : All pages will be locked and will be below physical
//!                      address 2**addressBits.
//! \param attrib      : Memory attributes to use when allocating
//! \param gpuInst     : IGNORED (For other platforms, if gpuInst is valid, then
//!                      on a NUMA system the system memory will be on the node
//!                      local to the GPU)
//!
//! \return OK if memory was successfully allocated and mapped, not OK otherwise
RC Xp::AllocPages
(
    size_t         numBytes,
    void **        pDescriptor,
    bool           contig,
    UINT32         addressBits,
    Memory::Attrib attrib,
    UINT32         gpuInst
)
{
    MASSERT(pDescriptor);
    MASSERT(numBytes);

    // To allocate contigous physical memory, QNX requires the mmap() call to
    // be called with specific flag settings. Its simple and straightforward.
    // http://www.qnx.com/developers/docs/7.0.0/#com.qnx.doc.neutrino.sys_arch/topic/ipc_mmap.html
    // As per QNX docs, filedes should be NOFD if allocating private or anon memory
    INT32 filedes = NOFD;
    INT32 flags = MAP_ANON;
    INT32 prot = PROT_READ | PROT_WRITE;
    UINT32 pageSize = sysconf(_SC_PAGESIZE);

    // round off numBytes to whole number of pages
    numBytes = numBytes%pageSize ? ((numBytes/pageSize)+1)*pageSize :
               numBytes;

    // to allocate contigous physical memory, both MAP_PHYS and MAP_SHARED have
    // to be enabled.
    if (contig)
    {
        flags |= MAP_SHARED | MAP_PHYS;
    }

    // set NOCACHE flag when requested uncached memory
    if (attrib == Memory::UC)
    {
        prot |= PROT_NOCACHE;
    }

    if (addressBits == 32)
    {
         *pDescriptor = mmap(0, numBytes, prot, flags, filedes, 0);
    }
    else
    {
        *pDescriptor = mmap64(0, numBytes, prot, flags, filedes, 0);
    }

    if (!(*pDescriptor))
    {
       Printf(Tee::PriError, "Unable to allocate memory\n");
       return RC::CANNOT_ALLOCATE_MEMORY;
    }

    MemoryBlockDescriptor blockDescriptor =
    {
        .numBytes = numBytes,
        .physicalAddress = Xp::VirtualToPhysical(*pDescriptor, Tee::PriError)
    };

    s_MemoryBookkeeper.CacheMapping(pDescriptor, blockDescriptor);
    return OK;
}

//------------------------------------------------------------------------------
//! \brief Alloc system memory with pages aligned as requested
//!
//! \param numBytes    : Number of bytes to allocate (actual allocation will be
//!                      a whole number of pages (numBytes will be rounded up).
//! \param pDescriptor : Returned descriptor for the allocated memory
//! \param addressBits : All pages will be locked and will be below physical
//!                      address 2**addressBits.
//! \param attrib      : Memory attributes to use when allocating
//! \param gpuInst     : IGNORED (For other platforms, if gpuInst is valid, then
//!                      on a NUMA system the system memory will be on the node
//!                      local to the GPU)
//!
//! \return OK if memory was successfully allocated and mapped, not OK otherwise
RC Xp::AllocPagesAligned
(
    size_t         numBytes,
    void **        pDescriptor,
    size_t         PhysicalAlignment,
    UINT32         addressBits,
    Memory::Attrib attrib,
    UINT32         gpuInst
)
{
    return AllocPages(numBytes, pDescriptor, true, addressBits, attrib, gpuInst);
}

//------------------------------------------------------------------------------
//!< Free pages allocated by AllocPages or AllocPagesAligned.
void Xp::FreePages(void* descriptor)
{
    MASSERT(descriptor);
    munmap(descriptor, s_MemoryBookkeeper.GetNumBytes(descriptor));
    s_MemoryBookkeeper.RemoveMapping(descriptor);
    return;
}

RC Xp::MergePages(void** pOutDescriptor, void** pInDescBegin, void** pInDescEnd)
{
    MASSERT(0);
    return RC::UNSUPPORTED_FUNCTION;
}

//------------------------------------------------------------------------------
RC Xp::MapPages
(
    void**          pRetVirtAddr,
    void*           descriptor,
    size_t          offset,
    size_t          size,
    Memory::Protect protect
)
{
    *pRetVirtAddr = descriptor;
    return OK;
}

//------------------------------------------------------------------------------
void Xp::UnMapPages(void* virtAddr)
{
    FreePages(virtAddr);
    return;
}

//------------------------------------------------------------------------------
PHYSADDR Xp::GetPhysicalAddress(void* descriptor, size_t offset)
{
    return 0;
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
RC Xp::SetDmaMask(UINT32 domain, UINT32 bus, UINT32 device, UINT32 func, UINT32 numBits)
{
    return RC::UNSUPPORTED_FUNCTION;
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
    return RC::UNSUPPORTED_FUNCTION;
}

//------------------------------------------------------------------------------
// Colwert physical address to virtual address.
//
void * Xp::PhysicalToVirtual(PHYSADDR physAddr, Tee::Priority pri)
{
    MASSERT(physAddr);
    void* pVirtAddr = nullptr;

    // first check if a cached mapping exists, and return the mapping. If physical
    // address is device memory then map it as device memory.
    pVirtAddr = s_MemoryBookkeeper.GetCachedVirtualAddress(physAddr);

    if (pVirtAddr)
    {
        return pVirtAddr;
    }

    if (physAddr < EMEM_BASE_START)
    {
        MapDeviceMemory(&pVirtAddr,
                       physAddr,
                       4,
                       Memory::UC,
                       Memory::ReadWrite);
    }

    if (pVirtAddr == nullptr)
    {
        Printf(pri, "Unable to translate physical address %llu\n",
            physAddr);
    }

    return pVirtAddr;
}

//------------------------------------------------------------------------------
// Colwert virtual address to physical address.
//
PHYSADDR Xp::VirtualToPhysical(volatile void* virtAddr, Tee::Priority pri)
{
    MASSERT(virtAddr);
    PHYSADDR cachedAddr = s_MemoryBookkeeper.GetCachedPhysicalAddress(const_cast<void*>(virtAddr));

    if (cachedAddr)
    {
       return cachedAddr;
    }

    off64_t physAddr = 0;
    // QNX uses the mem_offset* functions to get the physical address of a memory
    // mapped block.
    if (0 != mem_offset64(const_cast<const void*>(virtAddr), // address of block
                          NOFD,                              // fd = NOFD for QNX
                          1,                                 // length of memory block
                          &physAddr,                         // pointer to offset
                          nullptr))                          // pointer to length
    {
        Printf(pri, "Unable to map virtual address %p to physical address\n",
                              virtAddr);
    }

    return static_cast<PHYSADDR>(physAddr);
}

//------------------------------------------------------------------------------
RC Xp::IommuDmaMapMemory(void  *pDescriptor, string DevName, PHYSADDR *pIova)
{
    return RC::UNSUPPORTED_FUNCTION;
}

//------------------------------------------------------------------------------
RC Xp::IommuDmaUnmapMemory(void  *pDescriptor, string DevName)
{
    return RC::UNSUPPORTED_FUNCTION;
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
    return RC::UNSUPPORTED_FUNCTION;
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
    return RC::UNSUPPORTED_FUNCTION;
}

//!< Map device memory into the page table, get virtual address to use.
//!< Multiple mappings are legal, and give different virtual addresses
//!< on paging systems.
RC Xp::MapDeviceMemory
(
    void **         pRetVirtAddr,
    PHYSADDR        physAddr,
    size_t          size,
    Memory::Attrib  attrib,
    Memory::Protect protect
)
{
    MASSERT(pRetVirtAddr);
    MASSERT(physAddr);
    MASSERT(size);

    // QNX provides a special function to map MMIO addresses. Also on T18x, T19x
    // pushing a hard limit on physical address range which can be mapped (since
    // the OS puts a limit as well).
    if (physAddr >= EMEM_BASE_START)
    {
        Printf(Tee::PriError, "Can't map MMIO addresses beyond EMEM start\n");
        return RC::BAD_PARAMETER;
    }

    INT32 prot = PROT_READ | PROT_WRITE | PROT_NOCACHE;
    INT32 flags = 0;

    *pRetVirtAddr = mmap_device_memory(*pRetVirtAddr,
                                        size,
                                        prot,
                                        flags,
                                        physAddr);

    if (*pRetVirtAddr == MAP_FAILED)
    {
        Printf(Tee::PriError, "Unable to map device memory\n");
        return RC::SOFTWARE_ERROR;
    }

    MemoryBlockDescriptor blockDescriptor =
    {
        .numBytes = size,
        .physicalAddress = physAddr
    };

    s_MemoryBookkeeper.CacheMapping(pRetVirtAddr, blockDescriptor);
    return OK;
}

//------------------------------------------------------------------------------
//!< Undo the mapping created by MapDeviceMemory.
void Xp::UnMapDeviceMemory(void* virtAddr)
{
    MASSERT(virtAddr);
    munmap_device_memory(virtAddr, s_MemoryBookkeeper.GetNumBytes(virtAddr));
    s_MemoryBookkeeper.RemoveMapping(virtAddr);
}

// Stub functions for watchdog functionality
RC Xp::InitWatchdogs(UINT32 TimeoutSecs)
{
    Printf(Tee::PriLow, "Skipping watchdog initialization since"
                        " it is not supported\n");
    return RC::UNSUPPORTED_FUNCTION;
}

RC Xp::ResetWatchdogs()
{
    Printf(Tee::PriLow, "Skipping watchdog resetting since it is"
                        " not supported\n");
    return RC::UNSUPPORTED_FUNCTION;
}

RC Xp::MapIRQ(UINT32 * logicalIrq, UINT32 physIrq, string dtName, string fullName)
{
    return RC::UNSUPPORTED_FUNCTION;
}

// Interrupt functions.
//

bool Xp::IRQTypeSupported(UINT32 irqType)
{
    return false;
}

void Xp::ProcessPendingInterrupts()
{
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
    return RC::UNSUPPORTED_FUNCTION;
}

//------------------------------------------------------------------------------
// Port read and write functions.
//
RC Xp::PioRead08(UINT16 Port, UINT08 * pData)
{
    return RC::UNSUPPORTED_FUNCTION;
}

RC Xp::PioRead16(UINT16 Port, UINT16 * pData)
{
    return RC::UNSUPPORTED_FUNCTION;
}

RC Xp::PioRead32(UINT16 Port, UINT32 * pData)
{
    return RC::UNSUPPORTED_FUNCTION;
}

RC Xp::PioWrite08(UINT16 Port, UINT08 Data)
{
    return RC::UNSUPPORTED_FUNCTION;
}

RC Xp::PioWrite16(UINT16 Port, UINT16 Data)
{
    return RC::UNSUPPORTED_FUNCTION;
}

RC Xp::PioWrite32(UINT16 Port, UINT32 Data)
{
    return RC::UNSUPPORTED_FUNCTION;
}

//------------------------------------------------------------------------------
// Set display to a specified VESA 'Mode'.
// If 'Windowed is true, use a windowed frame buffer model,
// otherwise linear/flat model.
// If 'ClearMemory' is true, clear the display memory.
// If an invalid mode is requested, the mode will not change,
// and an error code will be returned.
//
RC Xp::SetVesaMode
(
    UINT32 Mode,
    bool   Windowed,     // = true
    bool   ClearMemory   // = true
)
{
    return RC::SET_MODE_FAILED;
}

//------------------------------------------------------------------------------
// Get the OS version
// returns UNSUPPORTED_FUNCTION
// -----------------------------------------------------------------------------
RC Xp::GetOSVersion(UINT64* pVer)
{
    return RC::UNSUPPORTED_FUNCTION;
}

void Xp::EnableInterrupts()
{
    return;
}

void Xp::DisableInterrupts()
{
    return;
}

Platform::Irql Xp::GetLwrrentIrql()
{
    Platform::Irql irq = Platform::NormalIrql;
    return irq;
}

Platform::Irql Xp::RaiseIrql(Platform::Irql newIrql)
{
    Platform::Irql irq = Platform::NormalIrql;
    return irq;
}

void Xp::LowerIrql(Platform::Irql newIrql)
{
    return;
}

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
    return RC::UNSUPPORTED_FUNCTION;
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
    return RC::UNSUPPORTED_FUNCTION;
}

//------------------------------------------------------------------------------
// Force a rescan of PCI bus.  NO-OP in Sim.
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
// Enable or disable SR-IOV.  Lwrrently, qnx doesn't support SR-IOV.
RC Xp::PciEnableSriov
(
    UINT32 domain,
    UINT32 bus,
    UINT32 device,
    UINT32 function,
    UINT32 vfCount
)
{
    return (vfCount > 0) ? RC::UNSUPPORTED_FUNCTION : OK;
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
    return RC::UNSUPPORTED_FUNCTION;
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
    return RC::UNSUPPORTED_FUNCTION;
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
    return RC::UNSUPPORTED_FUNCTION;
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
    return RC::UNSUPPORTED_FUNCTION;
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
    return RC::UNSUPPORTED_FUNCTION;
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
    return RC::UNSUPPORTED_FUNCTION;
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

//------------------------------------------------------------------------------
RC CallACPI_DSM_Eval
(
    UINT32 GpuInst
    , UINT32 Function
    , UINT32 DSMSubfunction
    , UINT32 *pInOut
    , UINT16 *pSize
)
{
    return RC::UNSUPPORTED_FUNCTION;
}

//------------------------------------------------------------------------------
RC Xp::CallACPIGeneric(UINT32 GpuInst,
                       UINT32 Function,
                       void *Param1,
                       void *Param2,
                       void *Param3)
{
    return RC::UNSUPPORTED_FUNCTION;
}

//------------------------------------------------------------------------------
RC Xp::LWIFMethod
(
    UINT32 Function,
    UINT32 SubFunction,
    void *InParams,
    UINT16 InParamSize,
    UINT32 *OutStatus,
    void *OutData,
    UINT16 *OutDataSize
)
{
    return RC::UNSUPPORTED_FUNCTION;
}

//------------------------------------------------------------------------------
Xp::OptimusMgr* Xp::GetOptimusMgr(UINT32 domain, UINT32 bus, UINT32 device, UINT32 function)
{
    return nullptr;
}

//-----------------------------------------------------------------------------
Xp::ClockManager* Xp::GetClockManager()
{
    Xp::ClockManager* pClkMgr = nullptr;
    return pClkMgr;
}

//-----------------------------------------------------------------------------
namespace CheetAh
{
    RC ClockGetSys(string& device, string& controller, LwU64* pClockHandle)
    {
        return RC::UNSUPPORTED_FUNCTION;
    }
}

//-----------------------------------------------------------------------------
RC Xp::FlushWriteCombineBuffer()
{
    return RC::UNSUPPORTED_FUNCTION;
}

//-----------------------------------------------------------------------------
RC Xp::FlushCpuCacheRange(void * StartAddress, void * EndAddress, CacheFlushFlags Flags)
{
    return RC::UNSUPPORTED_FUNCTION;
}

//-----------------------------------------------------------------------------
RC Xp::LinuxInternal::OpenDriver(const vector<string> &args)
{
    return OK;
}

//-----------------------------------------------------------------------------
RC Xp::LinuxInternal::CloseModsDriver()
{
    return OK;
}

//-----------------------------------------------------------------------------
RC Xp::LinuxInternal::AcpiDsmEval
(
      UINT16 domain
    , UINT16 bus
    , UINT08 dev
    , UINT08 func
    , UINT32 Function
    , UINT32 DSMMXMSubfunction
    , UINT32 *pInOut
    , UINT16 *size
)
{
    return RC::SOFTWARE_ERROR;
}

//-----------------------------------------------------------------------------
RC Xp::LinuxInternal::AcpiRootPortEval
(
    UINT16 domain
    , UINT16 bus
    , UINT08 device
    , UINT08 function
    , Xp::LinuxInternal::AcpiPowerMethod method
    , UINT32 *pStatus
)
{
    return RC::SOFTWARE_ERROR;
}

//------------------------------------------------------------------------------
//! \brief Get the CPU mask of CPUs that are local to the specified PCI device
//!        for NUMA enabled systems
//!
//! \param BusNumber      : PCI bus number of the device.
//! \param DeviceNumber   : PCI device number of the device
//! \param FunctionNumber : PCI function number of the device
//! \param pLocalCpuMasks : (NOT SUPPORTED) Empty vector returned
//!
//! \return UNSUPPORTED_FUNCTION
RC Xp::GetDeviceLocalCpus
(
    INT32 DomainNumber,
    INT32 BusNumber,
    INT32 DeviceNumber,
    INT32 FunctionNumber,
    vector<UINT32> *pLocalCpuMasks
)
{
    MASSERT(pLocalCpuMasks);
    pLocalCpuMasks->clear();
    return RC::UNSUPPORTED_FUNCTION;
}

INT32 Xp::GetDeviceNumaNode
(
    INT32 domainNumber,
    INT32 busNumber,
    INT32 deviceNumber,
    INT32 functionNumber
)
{
    return -1;
}

//------------------------------------------------------------------------------
RC Xp::GetFbConsoleInfo(PHYSADDR *pBaseAddress, UINT64 *pSize)
{
    return RC::UNSUPPORTED_FUNCTION;
}

//------------------------------------------------------------------------------
void Xp::SuspendConsole(GpuSubdevice *pGpuSubdevice)
{
}

//------------------------------------------------------------------------------
void Xp::ResumeConsole(GpuSubdevice *pGpuSubdevice)
{
}

//------------------------------------------------------------------------------
// Returns the type of operating system.
//
Xp::OperatingSystem Xp::GetOperatingSystem()
{
    return OS_ANDROID;
}

//------------------------------------------------------------------------------
RC Xp::ValidateGpuInterrupt(GpuSubdevice  *pGpuSubdevice,
                            UINT32 whichInterrupts)
{
    return RC::UNSUPPORTED_FUNCTION;
}

//------------------------------------------------------------------------------
RC Xp::GetNTPTime
(
   UINT32 &secs,
   UINT32 &msecs,
   const char *hostname
)
{
    return RC::UNSUPPORTED_FUNCTION;
}

//------------------------------------------------------------------------------
void* Xp::AllocOsEvent(UINT32 hClient, UINT32 hDevice)
{
    return nullptr;
}

//------------------------------------------------------------------------------
void Xp::FreeOsEvent(void* pFd, UINT32 hClient, UINT32 hDevice)
{
}

//------------------------------------------------------------------------------
RC Xp::WaitOsEvents
(
   void**  pOsEvents,
   UINT32  numOsEvents,
   UINT32* pCompletedIndices,
   UINT32  maxCompleted,
   UINT32* pNumCompleted,
   FLOAT64 timeoutMs
)
{
    return RC::TIMEOUT_ERROR;
}

//------------------------------------------------------------------------------
void Xp::SetOsEvent(void* pFd)
{
}

//------------------------------------------------------------------------------
// Disable the user interface, i.e. disable VGA.
//
RC Xp::DisableUserInterface()
{
    return RC::UNSUPPORTED_FUNCTION;
}

//------------------------------------------------------------------------------
// Enable the user interface.
//
RC Xp::EnableUserInterface()
{
    return RC::UNSUPPORTED_FUNCTION;
}

//------------------------------------------------------------------------------
bool Xp::HasFeature(Feature feat)
{
    switch (feat)
    {
        default:
            //MASSERT(!"Not implemented");
            return false;
    }
}

//-----------------------------------------------------------------------------
RC Xp::SendMessageToFTB
(
    UINT32 domain,
    UINT32 bus,
    UINT32 device,
    UINT32 function,
    const vector<tuple<UINT32, UINT32>>& messages
)
{
    return RC::UNSUPPORTED_FUNCTION;
}

//-----------------------------------------------------------------------------
RC Xp::ConfirmUsbAltModeConfig
(
    UINT32 domain,
    UINT32 bus,
    UINT32 device,
    UINT32 function,
    UINT08 desiredAltMode
)
{
    return RC::UNSUPPORTED_FUNCTION;
}

//-----------------------------------------------------------------------------
RC Xp::EnableUsbAutoSuspend
(
    UINT32 hcPciDomain,
    UINT32 hcPciBus,
    UINT32 hcPciDevice,
    UINT32 hcPciFunction,
    UINT32 autoSuspendDelayMs
)
{
    return RC::UNSUPPORTED_FUNCTION;
}

//-----------------------------------------------------------------------------
RC Xp::GetLwrrUsbAltMode
(
    UINT32 domain,
    UINT32 bus,
    UINT32 device,
    UINT32 function,
    UINT08 *pLwrrAltMode
)
{
    return RC::UNSUPPORTED_FUNCTION;
}

//-----------------------------------------------------------------------------
RC Xp::GetUsbPowerConsumption
(
    UINT32 domain,
    UINT32 bus,
    UINT32 device,
    UINT32 function,
    FLOAT64 *pVoltageVolt,
    FLOAT64 *pLwrrentAmp
)
{
    return RC::UNSUPPORTED_FUNCTION;
}

//-----------------------------------------------------------------------------
RC Xp::GetPpcFwVersion
(
    UINT32 domain,
    UINT32 bus,
    UINT32 device,
    UINT32 function,
    string *pPrimaryVersion,
    string *pSecondaryVersion
)
{
    return RC::UNSUPPORTED_FUNCTION;
}

//-----------------------------------------------------------------------------
RC Xp::GetPpcDriverVersion
(
    UINT32 domain,
    UINT32 bus,
    UINT32 device,
    UINT32 function,
    string *pDriverVersion
)
{
    return RC::UNSUPPORTED_FUNCTION;
}

//-----------------------------------------------------------------------------
RC Xp::GetFtbFwVersion
(
    UINT32 domain,
    UINT32 bus,
    UINT32 device,
    UINT32 function,
    string *pFtbFwVersion
)
{
    return RC::UNSUPPORTED_FUNCTION;
}

//-----------------------------------------------------------------------------
RC Xp::VirtualMadvise(void *pAddr, size_t size, Memory::VirtualAdvice advice)
{
    int madviseAdvice;
    switch (advice)
    {
        case Memory::Normal:
            madviseAdvice = POSIX_MADV_NORMAL;
            break;
        default:
            Printf(Tee::PriError, "%s : Unknown advice %d\n", __FUNCTION__, advice);
            return RC::BAD_PARAMETER;
    }

    if (posix_madvise(pAddr, size, madviseAdvice) != 0)
    {
        Printf(Tee::PriError, "Advising virtual memory failed : %d", errno);
        return RC::SOFTWARE_ERROR;
    }

    return OK;
}

RC Xp::SystemDropCaches()
{
    return RC::UNSUPPORTED_FUNCTION;
}

RC Xp::SystemCompactMemory()
{
    return RC::UNSUPPORTED_FUNCTION;
}
