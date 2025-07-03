/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 1999-2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

// Hardware implementation of Platform -- mostly just maps to Xp

#include "core/include/xp.h"
#include "gpu/include/gpudev.h"
#include "core/include/display.h"
#include "core/include/platform.h"
#include "core/include/pci.h"
#include "pltfmpvt.h"
#include "core/include/cpu.h"
#include "core/include/massert.h"
#include "core/include/utility.h"
#include "core/include/jscript.h"
#include "core/include/globals.h"
#ifdef INCLUDE_GPU
#include "gpu/include/vgpu.h"
#include "gpu/vmiop/vmiopelwmgr.h"
#include "gpu/include/gpumgr.h"
#include "core/include/mgrmgr.h"
#endif
#include <cstring>  // memcpy
#include "cheetah/include/tegrasocdevice.h"
#include "cheetah/include/tegrasocdeviceptr.h"


extern SObject Platform_Object;
extern SProperty Platform_SwapEndian;

RC Platform::Initialize()
{
    JavaScriptPtr pJs;
    RC rc;

    bool SwapEndian;
    CHECK_RC(pJs->GetProperty(Platform_Object, Platform_SwapEndian, &SwapEndian));
    MASSERT(!SwapEndian); // XXX endian swap is no longer supported -- rip me out!

    pJs->HookCtrlC();

    return OK;
}

void Platform::Cleanup(CleanupMode Mode)
{
    if (Mode == MininumCleanup)
    {
        return;
    }

    Platform::CallAndClearCleanupFunctions();
}

RC Platform::Reset()
{
    MASSERT(!"Function not implemented in non-SIM platforms");
    return RC::UNSUPPORTED_FUNCTION;
}

bool Platform::IsInitialized()
{
    // Good enough, since init basically doesn't do anything
    return true;
}

///------------------------------------------------------------------------------
RC Platform::DisableUserInterface
(
    UINT32 Width,
    UINT32 Height,
    UINT32 Depth,
    UINT32 RefreshRate,
    UINT32 FilterTaps,
    UINT32 ColorComp,
    UINT32 DdScalerMode,
    GpuDevice *pGrDev
)
{
    RC rc;

    if (CheetAh::IsInitialized() && CheetAh::SocPtr()->HasFeature(Device::SOC_SUPPORTS_DISPLAY_WITH_FE))
    {
        Printf(Tee::PriLow, "Skip disabling user interface on T234\n");
        return OK;
    }

    CHECK_RC(DisableUserInterface());

#ifdef INCLUDE_GPU
    GpuDevMgr* pGpuDevMgr = static_cast<GpuDevMgr*>(DevMgrMgr::d_GraphDevMgr);

    GpuDevice* pDev = pGrDev ? pGrDev : pGpuDevMgr->GetFirstGpuDevice();

    while (pDev)
    {
        Display *pDisplay = pDev->GetDisplay();

        UINT32 Selected = pDisplay->Selected();

        // Only one display device can be selected.
        if (Selected & (Selected - 1))
        {
            Printf(Tee::PriHigh, "Only one active display can be selected.\n");
            return RC::ILWALID_DISPLAY_MASK;
        }

        if (OK != pDisplay->SetMode(
                pDisplay->Selected(),
                Width,
                Height,
                Depth,
                RefreshRate,
                Display::FilterTaps(FilterTaps),
                Display::ColorCompression(ColorComp),
                DdScalerMode))
        {
            return RC::CANNOT_DISABLE_USER_INTERFACE;
        }

        if (!pGrDev)
            break; // The function was request to run on one specific device.

        pDev = pGpuDevMgr->GetNextGpuDevice(pDev);
    }
#endif

    return rc;
}

//! \brief Pass additional verification requirements
//!
//! This does nothing for hwpltfrm right now.
bool Platform::PassAdditionalVerification(GpuDevice *pGpuDevice, const char* path)
{
    // nothing for hw right now
    return true;
}

bool Platform::IsTegra()
{
#if defined(LWCPU_FAMILY_ARM)
    static bool bIsTegra     = false;
    static bool bInitialized = false;

    if (!bInitialized)
    {
        bInitialized = true;

        #if defined(QNX)
        if (Xp::DoesFileExist("/dev/lwbpmpdebugfs")) {
            bIsTegra = true;
            return true;
        }
        #endif

        // Simple check for LWPU device tree node
        if (Xp::DoesFileExist("/proc/device-tree/lwpu,dtsfilename"))
        {
            bIsTegra = true;
            return true;
        }

        // Look for lwhost in device list
        string devices;
        vector<string> deviceLines;
        if (Xp::DoesFileExist("/proc/devices") &&
            OK == Xp::InteractiveFileRead("/proc/devices", &devices) &&
            OK == Utility::Tokenizer(devices, "\n", &deviceLines))
        {
            for (UINT32 i=0; i < deviceLines.size(); i++)
            {
                vector<string> columns;
                if (OK == Utility::Tokenizer(deviceLines[i], " \t", &columns) &&
                    columns[1] == "lwhost")
                {
                    bIsTegra = true;
                    return true;
                }
            }
        }
    }

    return bIsTegra;
#else
    return false;
#endif
}

///------------------------------------------------------------------------------
// Find a PCI device with given 'DeviceId', 'VendorId', and 'Index'.
// Return '*pDomainNumber', '*pBusNumber', '*pDeviceNumber', and '*pFunctionNumber'.
//
RC Platform::FindPciDevice
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
    return Xp::FindPciDevice(DeviceId, VendorId, Index,
                             pDomainNumber, pBusNumber,
                             pDeviceNumber, pFunctionNumber);
}

//------------------------------------------------------------------------------
// @param ClassCode The entire 24 bit register baseclass/subclass/pgmIF,
//                  a byte for each field
//
RC Platform::FindPciClassCode
(
    INT32   ClassCode,
    INT32   Index,
    UINT32* pDomainNumber   /* =0  */,
    UINT32* pBusNumber      /* = 0 */,
    UINT32* pDeviceNumber   /* = 0 */,
    UINT32* pFunctionNumber /* = 0 */
)
{
    return Xp::FindPciClassCode(ClassCode, Index, pDomainNumber, pBusNumber,
                                pDeviceNumber, pFunctionNumber);
}

//------------------------------------------------------------------------------
// Force a rescan of PCI bus.
RC Platform::RescanPciBus
(
    INT32 domain,
    INT32 bus
)
{
    return Xp::RescanPciBus(domain, bus);
}

//------------------------------------------------------------------------------
// Read and write virtual addresses
//
void Platform::VirtualRd(const volatile void *Addr, void *Data, UINT32 Count)
{
    MemCopy(Data, Addr, Count);
}

void Platform::VirtualWr(volatile void *Addr, const void *Data, UINT32 Count)
{
    MemCopy(Addr, Data, Count);
}

UINT08 Platform::VirtualRd08(const volatile void *Addr)
{
    return *(const volatile UINT08 *)Addr;
}

UINT16 Platform::VirtualRd16(const volatile void *Addr)
{
    return *(const volatile UINT16 *)Addr;
}

UINT32 Platform::VirtualRd32(const volatile void *Addr)
{
    return *(const volatile UINT32 *)Addr;
}

UINT64 Platform::VirtualRd64(const volatile void *Addr)
{
    return *(const volatile UINT64 *)Addr;
}

void Platform::VirtualWr08(volatile void *Addr, UINT08 Data)
{
    *(volatile UINT08 *)Addr = Data;
}

void Platform::VirtualWr16(volatile void *Addr, UINT16 Data)
{
    *(volatile UINT16 *)Addr = Data;
}

void Platform::VirtualWr32(volatile void *Addr, UINT32 Data)
{
    *(volatile UINT32 *)Addr = Data;
}

void Platform::VirtualWr64(volatile void *Addr, UINT64 Data)
{
    *(volatile UINT64 *)Addr = Data;
}

UINT32 Platform::VirtualXchg32(volatile void *Addr, UINT32 Data)
{
    return Cpu::AtomicXchg32((volatile UINT32 *)Addr, Data);
}

//------------------------------------------------------------------------------
// Read and write physical addresses
//
RC Platform::PhysRd(PHYSADDR Addr, void *Data, UINT32 Count)
{
    void *VirtAddr = PhysicalToVirtual(Addr);
    if (VirtAddr == 0)
    {
        Printf(Tee::PriHigh, "Error: Physical address 0x%llx is not mapped\n", Addr);
        MASSERT(VirtAddr);
        return RC::COULD_NOT_MAP_PHYSICAL_ADDRESS;
    }
    VirtualRd(VirtAddr, Data, Count);
    return OK;
}

RC Platform::PhysWr(PHYSADDR Addr, const void *Data, UINT32 Count)
{
    void *VirtAddr = PhysicalToVirtual(Addr);
    if (VirtAddr == 0)
    {
        Printf(Tee::PriHigh, "Error: Physical address 0x%llx is not mapped\n", Addr);
        MASSERT(VirtAddr);
        return RC::COULD_NOT_MAP_PHYSICAL_ADDRESS;
    }
    VirtualWr(VirtAddr, Data, Count);
    return OK;
}

UINT08 Platform::PhysRd08(PHYSADDR Addr)
{
    void *VirtAddr = PhysicalToVirtual(Addr);
    if (VirtAddr == 0)
    {
        Printf(Tee::PriHigh, "Error: Physical address 0x%llx is not mapped\n", Addr);
        MASSERT(VirtAddr);
        return 0xFFU;
    }
    return VirtualRd08(VirtAddr);
}

UINT16 Platform::PhysRd16(PHYSADDR Addr)
{
    void *VirtAddr = PhysicalToVirtual(Addr);
    if (VirtAddr == 0)
    {
        Printf(Tee::PriHigh, "Error: Physical address 0x%llx is not mapped\n", Addr);
        MASSERT(VirtAddr);
        return 0xFFFFU;
    }
    return VirtualRd16(VirtAddr);
}

UINT32 Platform::PhysRd32(PHYSADDR Addr)
{
    void *VirtAddr = PhysicalToVirtual(Addr);
    if (VirtAddr == 0)
    {
        Printf(Tee::PriHigh, "Error: Physical address 0x%llx is not mapped\n", Addr);
        MASSERT(VirtAddr);
        return ~0U;
    }
    return VirtualRd32(VirtAddr);
}

UINT64 Platform::PhysRd64(PHYSADDR Addr)
{
    void *VirtAddr = PhysicalToVirtual(Addr);
    if (VirtAddr == 0)
    {
        Printf(Tee::PriHigh, "Error: Physical address 0x%llx is not mapped\n", Addr);
        MASSERT(VirtAddr);
        return ~static_cast<UINT64>(0U);
    }
    return VirtualRd64(VirtAddr);
}

void Platform::PhysWr08(PHYSADDR Addr, UINT08 Data)
{
    void *VirtAddr = PhysicalToVirtual(Addr);
    if (VirtAddr == 0)
    {
        Printf(Tee::PriHigh, "Error: Physical address 0x%llx is not mapped\n", Addr);
        MASSERT(VirtAddr);
        return;
    }
    VirtualWr08(VirtAddr, Data);
}

void Platform::PhysWr16(PHYSADDR Addr, UINT16 Data)
{
    void *VirtAddr = PhysicalToVirtual(Addr);
    if (VirtAddr == 0)
    {
        Printf(Tee::PriHigh, "Error: Physical address 0x%llx is not mapped\n", Addr);
        MASSERT(VirtAddr);
        return;
    }
    VirtualWr16(VirtAddr, Data);
}

void Platform::PhysWr32(PHYSADDR Addr, UINT32 Data)
{
    void *VirtAddr = PhysicalToVirtual(Addr);
    if (VirtAddr == 0)
    {
        Printf(Tee::PriHigh, "Error: Physical address 0x%llx is not mapped\n", Addr);
        MASSERT(VirtAddr);
        return;
    }
    VirtualWr32(VirtAddr, Data);
}

void Platform::PhysWr64(PHYSADDR Addr, UINT64 Data)
{
    void *VirtAddr = PhysicalToVirtual(Addr);
    if (VirtAddr == 0)
    {
        Printf(Tee::PriHigh, "Error: Physical address 0x%llx is not mapped\n", Addr);
        MASSERT(VirtAddr);
        return;
    }
    VirtualWr64(VirtAddr, Data);
}

UINT32 Platform::PhysXchg32(PHYSADDR Addr, UINT32 Data)
{
    void *VirtAddr = PhysicalToVirtual(Addr);
    if (VirtAddr == 0)
    {
        Printf(Tee::PriHigh, "Error: Physical address 0x%llx is not mapped\n", Addr);
        MASSERT(VirtAddr);
        return ~0U;
    }
    return VirtualXchg32(VirtAddr, Data);
}

//------------------------------------------------------------------------------
void Platform::MemCopy(volatile void *Dst, const volatile void *Src, size_t Count)
{
    if (GetUse4ByteMemCopy())
        Platform::Hw4ByteMemCopy (Dst, Src, Count);
    else
        Cpu::MemCopy(const_cast<void *>(Dst), const_cast<const void *>(Src), Count);
}

//------------------------------------------------------------------------------
void Platform::MemCopy4Byte(volatile void *Dst, const volatile void *Src, size_t Count)
{
    Platform::Hw4ByteMemCopy (Dst, Src, Count);
}

//------------------------------------------------------------------------------
void Platform::MemSet(volatile void *Dst, UINT08 Data, size_t Count)
{
    Cpu::MemSet(const_cast<void *>(Dst), Data, Count);
}

//------------------------------------------------------------------------------
//!< Return the page size in bytes for this platform.
size_t Platform::GetPageSize()
{
   return Xp::GetPageSize();
}

//------------------------------------------------------------------------------
//!< Dump the lwrrently mapped pages.
void Platform::DumpPageTable()
{
   Xp::DumpPageTable();
}

//------------------------------------------------------------------------------
//!< Test for system memory size - used only for sim.
RC Platform::TestAllocPages
(
    size_t      NumBytes
)
{
    MASSERT(0);
    return RC::UNSUPPORTED_FUNCTION;
}

//------------------------------------------------------------------------------
//! \brief Allocate system memory.
//!
//! \param numBytes    : Number of bytes to allocate (actual allocation will be
//!                      a whole number of pages (numBytes will be rounded up).
//! \param pDescriptor : Returned descriptor for the allocated memory
//! \param contiguous  : If true, physical pages will be adjacent and in-order.
//! \param addressBits : All pages will be locked and will be below physical
//!                      address 2**AddressBits.
//! \param attrib      : Memory attributes to use when allocating
//! \param domain      : PCI domain of the device for which to allocate the memory.
//! \param bus         : PCI bus of the device for which to allocate the memory.
//! \param device      : PCI device of the device for which to allocate the memory.
//! \param function    : PCI function of the device for which to allocate the memory.
//!
//! \return OK if memory was successfully allocated and mapped, not OK otherwise
RC Platform::AllocPages
(
    size_t          numBytes,
    void **         pDescriptor,
    bool            contiguous,
    UINT32          addressBits,
    Memory::Attrib  attrib,
    UINT32          domain,
    UINT32          bus,
    UINT32          device,
    UINT32          function
)
{
    return Xp::AllocPages(numBytes,
                          pDescriptor,
                          contiguous,
                          GetAllowedAddressBits(addressBits),
                          attrib,
                          domain,
                          bus,
                          device,
                          function);
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
//! \param GpuInst     : If GpuInst is valid, then on a NUMA system the system
//!                      memory will be on the node local to the GPU
//!
//! \return OK if memory was successfully allocated and mapped, not OK otherwise
RC Platform::AllocPages
(
    size_t          NumBytes,
    void **         pDescriptor,
    bool            Contiguous,
    UINT32          AddressBits,
    Memory::Attrib  Attrib,
    UINT32          GpuInst,
    Memory::Protect Protect
)
{
    return Xp::AllocPages(NumBytes, pDescriptor,
                          Contiguous, GetAllowedAddressBits(AddressBits),
                          Attrib, GpuInst);
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
//! \param GpuInst     : If GpuInst is valid, then on a NUMA system the system
//!                      memory will be on the node local to the GPU
//!
//! \return OK if memory was successfully allocated and mapped, not OK otherwise
RC Platform::AllocPagesAligned
(
    size_t         NumBytes,
    void **        pDescriptor,
    size_t         PhysicalAlignment,
    UINT32         AddressBits,
    Memory::Attrib Attrib,
    UINT32         GpuInst
)
{
    return Xp::AllocPagesAligned(NumBytes, pDescriptor,
                                 PhysicalAlignment,
                                 GetAllowedAddressBits(AddressBits), Attrib,
                                 GpuInst);
}

//------------------------------------------------------------------------------
//!< Alloc system memory.
//!< A whole number of pages will be mapped (NumBytes will be rounded up).
//!< All pages will be locked and will be below physical address 2**32.
//!< If Contiguous is true, physical pages will be adjacent and in-order.
//! The pages will be allocated in the 4GB arena in which the address ConstrainAddr
//! is located.
RC Platform::AllocPagesConstrained
(
    size_t         NumBytes,
    void **        pDescriptor,
    bool           Contiguous,
    UINT32         AddressBits,
    Memory::Attrib Attrib,
    PHYSADDR       ConstrainAddr,
    UINT32         GpuInst
)
{
    return RC::UNSUPPORTED_FUNCTION;
}

//------------------------------------------------------------------------------
//!< Alloc contiguous System memory pages, with the starting physical
//!< address aligned as requested.
//!< A whole number of pages will be mapped (NumBytes will be rounded up).
//!< All pages will be locked and will be below physical address 2**32.
//!< PhysicalAlignment must be a power of 2, values less than the system
//!< page size will be ignored.
//! The pages will be allocated in the 4GB arena in which the address ConstrainAddr
//! is located.
RC Platform::AllocPagesConstrainedAligned
(
    size_t         NumBytes,
    void **        pDescriptor,
    size_t         PhysicalAlignment,
    UINT32         AddressBits,
    Memory::Attrib Attrib,
    PHYSADDR       ConstrainAddr,
    UINT32         GpuInst
)
{
   return RC::UNSUPPORTED_FUNCTION;
}

//------------------------------------------------------------------------------
//!< Free pages allocated by AllocPages or AllocPagesAligned.
void Platform::FreePages(void * Descriptor)
{
    Xp::FreePages(Descriptor);
}

RC Platform::MergePages(void** pOutDescriptor, void** pInDescBegin, void** pInDescEnd)
{
    return Xp::MergePages(pOutDescriptor, pInDescBegin, pInDescEnd);
}

//------------------------------------------------------------------------------
RC Platform::MapPages
(
    void **pReturnedVirtualAddress,
    void * Descriptor,
    size_t Offset,
    size_t Size,
    Memory::Protect Protect
)
{
    return Xp::MapPages(pReturnedVirtualAddress, Descriptor, Offset, Size, Protect);
}

//------------------------------------------------------------------------------
void Platform::UnMapPages(void * VirtualAddress)
{
    Xp::UnMapPages(VirtualAddress);
}

//------------------------------------------------------------------------------
PHYSADDR Platform::GetPhysicalAddress(void *Descriptor, size_t Offset)
{
    return Xp::GetPhysicalAddress(Descriptor, Offset);
}

//------------------------------------------------------------------------------
PHYSADDR Platform::GetMappedPhysicalAddress
(
    UINT32 domain,
    UINT32 bus,
    UINT32 device,
    UINT32 func,
    void  *descriptor,
    size_t offset
)
{
    return Xp::GetMappedPhysicalAddress(domain, bus, device, func, descriptor, offset);
}

//------------------------------------------------------------------------------
RC Platform::DmaMapMemory
(
    UINT32 domain,
    UINT32 bus,
    UINT32 device,
    UINT32 func,
    void  *descriptor
)
{
    return Xp::DmaMapMemory(domain, bus, device, func, descriptor);
}

//------------------------------------------------------------------------------
RC Platform::DmaUnmapMemory
(
    UINT32 domain,
    UINT32 bus,
    UINT32 device,
    UINT32 func,
    void  *descriptor
)
{
    return Xp::DmaUnmapMemory(domain, bus, device, func, descriptor);
}

//------------------------------------------------------------------------------
RC Platform::SetDmaMask(UINT32 domain, UINT32 bus, UINT32 device, UINT32 func, UINT32 numBits)
{
    return Xp::SetDmaMask(domain, bus, device, func, numBits);
}

//------------------------------------------------------------------------------
//!< Reserve resources for subsequent allocations of the same Attrib
//!< by AllocPages or AllocPagesAligned.
//!< Under DOS, this will pre-allocate a contiguous heap and MTRR reg.
//!< Platforms that use real paging memory managers may ignore this.
void Platform::ReservePages
(
    size_t NumBytes,
    Memory::Attrib Attrib
)
{
    Xp::ReservePages(NumBytes, Attrib);
}

//------------------------------------------------------------------------------
//!< Map device memory into the page table, get virtual address to use.
//!< Multiple mappings are legal, and give different virtual addresses
//!< on paging systems.
//!< Under DOS, this is faked more-or-less correctly using MTRRs.
RC Platform::MapDeviceMemory
(
    void **     pReturnedVirtualAddress,
    PHYSADDR    PhysicalAddress,
    size_t      NumBytes,
    Memory::Attrib Attrib,
    Memory::Protect Protect
)
{
    // Called by the resman for each GPU BAR.
    return Xp::MapDeviceMemory(pReturnedVirtualAddress, PhysicalAddress,
                               NumBytes, Attrib, Protect);
}

//------------------------------------------------------------------------------
//!< Undo the mapping created by MapDeviceMemory.
void Platform::UnMapDeviceMemory(void * VirtualAddress)
{
    Xp::UnMapDeviceMemory(VirtualAddress);
}

//------------------------------------------------------------------------------
//!< Program a Memory Type and Range Register to control cache mode
//!< for a range of physical addresses.
//!< Called by the resman to make the (unmapped) AGP aperture write-combined.
RC Platform::SetMemRange
(
    PHYSADDR    PhysicalAddress,
    size_t      NumBytes,
    Memory::Attrib Attrib
)
{
    return Xp::SetMemRange(PhysicalAddress, NumBytes, Attrib);
}

//------------------------------------------------------------------------------
RC Platform::UnSetMemRange
(
    PHYSADDR    PhysicalAddress,
    size_t      NumBytes
)
{
    return Xp::UnSetMemRange(PhysicalAddress, NumBytes);
}

//------------------------------------------------------------------------------
// Get ATS target address range.
//
RC Platform::GetAtsTargetAddressRange
(
    UINT32     gpuInst,
    UINT32    *pAddr,
    UINT32    *pAddrGuest,
    UINT32    *pAddrWidth,
    UINT32    *pMask,
    UINT32    *pMaskWidth,
    UINT32    *pGranularity,
    MODS_BOOL  bIsPeer,
    UINT32     peerIndex
)
{
    return Xp::GetAtsTargetAddressRange(gpuInst, pAddr, pAddrGuest, pAddrWidth,
                                        pMask, pMaskWidth, pGranularity, bIsPeer,
                                        peerIndex);
}

//------------------------------------------------------------------------------
// Get the lwlink speed supported by NPU from device-tree
//
RC Platform::GetLwLinkLineRate
(
    UINT32 domain,
    UINT32 bus,
    UINT32 device,
    UINT32 func,
    UINT32 *pSpeed
)
{
    return Xp::GetLwLinkLineRate(domain, bus, device, func, pSpeed);
}

//------------------------------------------------------------------------------
// Colwert physical address to virtual address.
//
void * Platform::PhysicalToVirtual(PHYSADDR PhysicalAddress, Tee::Priority pri)
{
   return Xp::PhysicalToVirtual(PhysicalAddress, pri);
}

//------------------------------------------------------------------------------
// Colwert virtual address to physical address.
//
PHYSADDR Platform::VirtualToPhysical(volatile void * VirtualAddress, Tee::Priority pri)
{
   return Xp::VirtualToPhysical(VirtualAddress, pri);
}

UINT32 Platform::VirtualToPhysical32(volatile void * VirtualAddress)
{
   // This is a shortlwt used because a lot of hardware only accepts 32 bits.
   // Assert to try to catch bugs caused by using this function where it's unsafe.
   PHYSADDR PhysicalAddress = VirtualToPhysical(VirtualAddress);
   MASSERT(PhysicalAddress <= 0xFFFFFFFF);
   return (UINT32)PhysicalAddress;
}

RC Platform::GetCarveout(PHYSADDR* pPhysAddr, UINT64* pSize)
{
    return Xp::GetCarveout(pPhysAddr, pSize);
}

RC Platform::GetVPR(PHYSADDR* pPhysAddr, UINT64* pSize, void **pMemDesc)
{
    // On Silicon the bootloader sets up VPR, we can't modify it.
    Printf(Tee::PriLow, "VPR preallocation is not supported on Silicon\n");
    return RC::UNSUPPORTED_FUNCTION;
}

RC Platform::GetGenCarveout(PHYSADDR *pPhysAddr, UINT64 *pSize, UINT32 id, UINT64 align)
{
    // On Silicon the boot loader sets up additional carve-outs, we can't modify it.
    Printf(Tee::PriLow, "General carve-out preallocation is not supported on Silicon\n");
    return RC::UNSUPPORTED_FUNCTION;
}

RC Platform::CallACPIGeneric(UINT32 GpuInst,
                             UINT32 Function,
                             void *Param1,
                             void *Param2,
                             void *Param3)
{
    return Xp::CallACPIGeneric(GpuInst,
                               Function,
                               Param1,
                               Param2,
                               Param3);
}

RC Platform::LWIFMethod
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
   return Xp::LWIFMethod(Function, SubFunction, InParams,
        InParamSize, OutStatus, OutData, OutDataSize);
}

//------------------------------------------------------------------------------
// Ilwalidate the CPU's cache without flushing it.  (Very dangerous!)
//
RC Platform::IlwalidateCpuCache()
{
   return Cpu::IlwalidateCache();
}

//------------------------------------------------------------------------------
// Flush the CPU's cache.
//
RC Platform::FlushCpuCache()
{
   return Cpu::FlushCache();
}

//------------------------------------------------------------------------------
// Flush the CPU's cache from 'StartAddress' to 'EndAddress'.
//
RC Platform::FlushCpuCacheRange
(
   void * StartAddress /* = 0 */,
   void * EndAddress   /* = 0xFFFFFFFF */,
   UINT32 Flags
)
{
   return Cpu::FlushCpuCacheRange(StartAddress, EndAddress, Flags);
}

//------------------------------------------------------------------------------
// Flush the CPU's write combine buffer.
//
RC Platform::FlushCpuWriteCombineBuffer()
{
   return Cpu::FlushWriteCombineBuffer();
}

RC Platform::SetupDmaBase
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
    Xp::PpcTceBypassMode xpBypassMode;
    switch (bypassMode)
    {
        case PPC_TCE_BYPASS_DEFAULT:
            xpBypassMode = Xp::PPC_TCE_BYPASS_DEFAULT;
            break;
        case PPC_TCE_BYPASS_FORCE_ON:
            xpBypassMode = Xp::PPC_TCE_BYPASS_FORCE_ON;
            break;
        case PPC_TCE_BYPASS_FORCE_OFF:
            xpBypassMode = Xp::PPC_TCE_BYPASS_FORCE_OFF;
            break;
        default:
            Printf(Tee::PriHigh, "Error: Unknown bypass mode %d\n", bypassMode);
            return RC::SOFTWARE_ERROR;
    }
    return Xp::SetupDmaBase(domain, bus, dev, func, xpBypassMode, devDmaMask, pDmaBaseAddr);
}

//-----------------------------------------------------------------------------
RC Platform::SetLwLinkSysmemTrained(UINT32 domain, UINT32 bus, UINT32 device, UINT32 func, bool trained)
{
    return Xp::SetLwLinkSysmemTrained(domain, bus, device, func, trained);
}

//-----------------------------------------------------------------------------
RC Platform::PciGetBarInfo
(
    UINT32    domainNumber,
    UINT32    busNumber,
    UINT32    deviceNumber,
    UINT32    functionNumber,
    UINT32    barIndex,
    PHYSADDR* pBaseAddress,
    UINT64*   pBarSize
)
{
    RC rc;

    // Requires MODS driver 3.51+
    rc = Xp::PciGetBarInfo(domainNumber, busNumber,
                           deviceNumber, functionNumber,
                           barIndex, pBaseAddress, pBarSize);
    if (rc == RC::UNSUPPORTED_FUNCTION)
    {
        // Avoid possible lost RC warnings
        rc.Clear();
        CHECK_RC(Pci::GetBarInfo(domainNumber, busNumber,
                                 deviceNumber, functionNumber,
                                 barIndex, pBaseAddress, pBarSize));
    }

    return rc;
}

//-----------------------------------------------------------------------------
RC Platform::PciEnableSriov
(
    UINT32 domain,
    UINT32 bus,
    UINT32 device,
    UINT32 function,
    UINT32 vfCount
)
{
    MASSERT(Platform::IsPhysFunMode());
    return Xp::PciEnableSriov(domain, bus, device, function, vfCount);
}

RC Platform::PciGetIRQ
(
    UINT32  domainNumber,
    UINT32  busNumber,
    UINT32  deviceNumber,
    UINT32  functionNumber,
    UINT32* pIrq
)
{
    RC rc;

    // Requires MODS driver 3.51+
    rc = Xp::PciGetIRQ(domainNumber, busNumber,
                       deviceNumber, functionNumber, pIrq);
    if (rc == RC::UNSUPPORTED_FUNCTION)
    {
        // Avoid possible lost RC warnings
        rc.Clear();
        CHECK_RC(Pci::GetIRQ(domainNumber, busNumber,
                             deviceNumber, functionNumber, pIrq));
    }

    return rc;
}

//------------------------------------------------------------------------------
// Read byte, word, or dword at 'Address' from a PCI device with the given
// 'BusNumber', 'DeviceNumber', and 'FunctionNumber'.
// Return read data in '*pData'.
//
RC Platform::PciRead08
(
   INT32    DomainNumber,
   INT32    BusNumber,
   INT32    DeviceNumber,
   INT32    FunctionNumber,
   INT32    Address,
   UINT08 * pData
)
{
    RC rc = Xp::PciRead08(DomainNumber, BusNumber, DeviceNumber, FunctionNumber, Address, pData);
    if (Platform::GetDumpPciAccess())
    {
        if (OK == rc)
        {
            Printf(Tee::PriNormal,
                   "Platform::PciRead08 : Addr:0x%08x -> Value:0x%08x\n",
                   Address, *pData);
        }
        else
        {
            Printf(Tee::PriNormal,
                   "Platform::PciRead08 : Addr:0x%08x FAILED (%s)\n",
                   Address, rc.Message());
        }
    }
    return rc;
}

RC Platform::PciRead16
(
   INT32    DomainNumber,
   INT32    BusNumber,
   INT32    DeviceNumber,
   INT32    FunctionNumber,
   INT32    Address,
   UINT16 * pData
)
{
    RC rc = Xp::PciRead16(DomainNumber, BusNumber, DeviceNumber, FunctionNumber, Address, pData);
    if (Platform::GetDumpPciAccess())
    {
        if (OK == rc)
        {
            Printf(Tee::PriNormal,
                   "Platform::PciRead16 : Addr:0x%08x -> Value:0x%08x\n",
                   Address, *pData);
        }
        else
        {
            Printf(Tee::PriNormal,
                   "Platform::PciRead16 : Addr:0x%08x FAILED (%s)\n",
                   Address, rc.Message());
        }
    }
    return rc;
}

RC Platform::PciRead32
(
   INT32    DomainNumber,
   INT32    BusNumber,
   INT32    DeviceNumber,
   INT32    FunctionNumber,
   INT32    Address,
   UINT32 * pData
)
{
    RC rc = Xp::PciRead32(DomainNumber, BusNumber, DeviceNumber, FunctionNumber, Address, pData);
    if (Platform::GetDumpPciAccess())
    {
        if (OK == rc)
        {
            Printf(Tee::PriNormal,
                   "Platform::PciRead32 : Addr:0x%08x -> Value:0x%08x\n",
                   Address, *pData);
        }
        else
        {
            Printf(Tee::PriNormal,
                   "Platform::PciRead32 : Addr:0x%08x FAILED (%s)\n",
                   Address, rc.Message());
        }
    }
    return rc;
}

//------------------------------------------------------------------------------
// Write 'Data' at 'Address' to a PCI device with the given
// 'BusNumber', 'DeviceNumber', and 'FunctionNumber'.
//
RC Platform::PciWrite08
(
   INT32  DomainNumber,
   INT32  BusNumber,
   INT32  DeviceNumber,
   INT32  FunctionNumber,
   INT32  Address,
   UINT08 Data
)
{
    if (Platform::GetDumpPciAccess())
    {
        Printf(Tee::PriNormal,
               "Platform::PciWrite08 : Addr:0x%08x <- Value:0x%08x\n",
               Address, Data);
    }
    return Xp::PciWrite08(DomainNumber, BusNumber, DeviceNumber, FunctionNumber, Address, Data);
}

RC Platform::PciWrite16
(
   INT32  DomainNumber,
   INT32  BusNumber,
   INT32  DeviceNumber,
   INT32  FunctionNumber,
   INT32  Address,
   UINT16 Data
)
{
    if (Platform::GetDumpPciAccess())
    {
        Printf(Tee::PriNormal,
               "Platform::PciWrite16 : Addr:0x%08x <- Value:0x%08x\n",
               Address, Data);
    }
    return Xp::PciWrite16(DomainNumber, BusNumber, DeviceNumber, FunctionNumber, Address, Data);
}

RC Platform::PciWrite32
(
   INT32  DomainNumber,
   INT32  BusNumber,
   INT32  DeviceNumber,
   INT32  FunctionNumber,
   INT32  Address,
   UINT32 Data
)
{
    if (Platform::GetDumpPciAccess())
    {
        Printf(Tee::PriNormal,
               "Platform::PciWrite32 : Addr:0x%08x <- Value:0x%08x\n",
               Address, Data);
    }
    return Xp::PciWrite32(DomainNumber, BusNumber, DeviceNumber, FunctionNumber, Address, Data);
}

RC Platform::PciRemove
(
   INT32    domainNumber,
   INT32    busNumber,
   INT32    deviceNumber,
   INT32    functionNumber
)
{
    return Xp::PciRemove(domainNumber, busNumber, deviceNumber, functionNumber);
}

RC Platform::PciResetFunction
(
   INT32    domainNumber,
   INT32    busNumber,
   INT32    deviceNumber,
   INT32    functionNumber
)
{
    return Xp::PciResetFunction(domainNumber, busNumber, deviceNumber, functionNumber);
}

//------------------------------------------------------------------------------
// Port read and write functions.
//
RC Platform::PioRead08(UINT16 Port, UINT08 * pData)
{
   return Xp::PioRead08(Port, pData);
}

RC Platform::PioRead16(UINT16 Port, UINT16 * pData)
{
   return Xp::PioRead16(Port, pData);
}

RC Platform::PioRead32(UINT16 Port, UINT32 * pData)
{
   return Xp::PioRead32(Port, pData);
}

RC Platform::PioWrite08(UINT16 Port, UINT08 Data)
{
   return Xp::PioWrite08(Port, Data);
}

RC Platform::PioWrite16(UINT16 Port, UINT16 Data)
{
   return Xp::PioWrite16(Port, Data);
}

RC Platform::PioWrite32(UINT16 Port, UINT32 Data)
{
   return Xp::PioWrite32(Port, Data);
}

//------------------------------------------------------------------------------
// Interrupt functions.
//

void Platform::HandleInterrupt(UINT32 Irq)
{
    MASSERT(!"Platform::HandleInterrupt not supported on hardware");
}

RC Platform::HookIRQ(UINT32 Irq, ISR func, void* cookie)
{
    return Xp::HookIRQ(Irq, func, cookie);
}

RC Platform::MapIRQ(UINT32 * logicalIrq, UINT32 physIrq, string dtName, string fullName)
{
    return Xp::MapIRQ(logicalIrq, physIrq, dtName, fullName);
}

RC Platform::UnhookIRQ(UINT32 Irq, ISR func, void* cookie)
{
    return Xp::UnhookIRQ(Irq, func, cookie);
}

RC Platform::DisableIRQ(UINT32 Irq)
{
    MASSERT(!"Function not implemented in non-SIM platforms");
    return RC::UNSUPPORTED_FUNCTION;
}

RC Platform::EnableIRQ(UINT32 Irq)
{
    MASSERT(!"Function not implemented in non-SIM platforms");
    return RC::UNSUPPORTED_FUNCTION;
}

bool Platform::IsIRQHooked(UINT32 irq)
{
    MASSERT(!"Function not implemented in non-SIM platforms");
    return false;
}

bool Platform::IRQTypeSupported(UINT32 irqType)
{
    return Xp::IRQTypeSupported(irqType);
}

// Allocate some nominal IRQ numbers for MSIX.
//
// In sim mods, AllocateIRQs() does the actual MSIX hooking, and
// HookInt() is mostly ignored for MSIX.  For hardware, it was more
// straightforward to keep doing the hooking in HookInt().
//
// This method assigns the MSIX IRQ numbers in reverse order, so that
// the first time we call HookInt() for MSIX, the IRQ number will tell
// our kernel-interface code (e.g. linux/xp_mods_kd.cpp) how many
// times we plan to call HookInt().
RC Platform::AllocateIRQs
(
    UINT32 pciDomain,
    UINT32 pciBus,
    UINT32 pciDevice,
    UINT32 pciFunction,
    UINT32 lwecs,
    UINT32 *irqs,
    UINT32 flags
)
{
    for (UINT32 ii = 0; ii < lwecs; ++ii)
    {
        irqs[ii] = lwecs - ii;
    }
    return OK;
}

void Platform::FreeIRQs
(
    UINT32 pciDomain,
    UINT32 pciBus,
    UINT32 pciDevice,
    UINT32 pciFunction
)
{
}

void Platform::InterruptProcessed()
{
}

void Platform::ProcessPendingInterrupts()
{
    Xp::ProcessPendingInterrupts();
}

RC Platform::HookInt(const IrqParams& irqParams, ISR func, void* cookie)
{
    return Xp::HookInt(irqParams, func, cookie);
}

RC Platform::UnhookInt(const IrqParams& irqParams, ISR func, void* cookie)
{
    return Xp::UnhookInt(irqParams, func, cookie);
}

void Platform::EnableInterrupts()
{
    Xp::EnableInterrupts();
}

void Platform::DisableInterrupts()
{
    Xp::DisableInterrupts();
}

Platform::Irql Platform::GetLwrrentIrql()
{
    return Xp::GetLwrrentIrql();
}

Platform::Irql Platform::RaiseIrql(Platform::Irql NewIrql)
{
    return Xp::RaiseIrql(NewIrql);
}

void Platform::LowerIrql(Platform::Irql NewIrql)
{
    Xp::LowerIrql(NewIrql);
}

//------------------------------------------------------------------------------
void Platform::Pause()
{
    Cpu::Pause();
}

//------------------------------------------------------------------------------
void Platform::Delay(UINT32 Microseconds)
{
    Utility::Delay(Microseconds);
}

//------------------------------------------------------------------------------
void Platform::DelayNS(UINT32 Nanoseconds)
{
    Utility::DelayNS(Nanoseconds);
}

//------------------------------------------------------------------------------
void Platform::SleepUS(UINT32 MinMicrosecondsToSleep)
{
    Utility::SleepUS(MinMicrosecondsToSleep);
}

//------------------------------------------------------------------------------
// Return the amount of time that has elapsed since the beginning of time
UINT64 Platform::GetTimeNS() // nsecs
{
    return Xp::GetWallTimeNS();
}

UINT64 Platform::GetTimeUS() // usecs
{
    return Xp::GetWallTimeUS();
}

UINT64 Platform::GetTimeMS() // msecs
{
    return Xp::GetWallTimeMS();
}

//------------------------------------------------------------------------------
bool Platform::LoadSimSymbols()
{
    MASSERT(!"Platform::LoadSimSymbols not supported on hardware");
    return false;
}

///------------------------------------------------------------------------------
// Simulator functions
//
bool Platform::IsChiplibLoaded()
{
    return false;
}

void Platform::ClockSimulator(int Cycles)
{
    MASSERT(!"Platform::ClockSimulator not supported on hardware");
}

UINT64 Platform::GetSimulatorTime()
{
    MASSERT(!"Platform::GetSimulatorTime not supported on hardware");
    return 0;
}

double Platform::GetSimulatorTimeUnitsNS()
{
    MASSERT(!"Platform::GetSimulatorTimeUnitsNS not supported on hardware");
    return 0;
}

Platform::SimulationMode Platform::GetSimulationMode()
{
#if defined(INCLUDE_GPU)
    // Always set Fmodel for VDK runs
    // Bug - 2718565
    if ((IsTegra() && Vgpu::IsVdk()) ||
        Vgpu::IsSupported())
    {
        return Platform::Fmodel;
    }
#endif
    return Platform::Hardware;
}

bool Platform::ExtMemAllocSupported()
{
    return false;
}

int Platform::EscapeWrite(const char *Path, UINT32 Index, UINT32 Size, UINT32 Value)
{
#if defined(INCLUDE_GPU)
    Vgpu* pVgpu = Vgpu::GetPtr();
    if (pVgpu)
    {
        Printf(Tee::PriDebug, "EscapeWrite ignored in this MODS build\n");
        return 0;
    }
#endif
    MASSERT(!"Platform::EscapeWrite not supported on hardware");
    return 0;
}

int Platform::EscapeRead(const char *Path, UINT32 Index, UINT32 Size, UINT32 *Value)
{
#if defined(INCLUDE_GPU)
    Vgpu* pVgpu = Vgpu::GetPtr();
    if (pVgpu)
    {
        return pVgpu->EscapeRead(Path, Index, Size, Value);
    }
    else
    {
        return 1;
    }
#else
    MASSERT(!"Platform::EscapeRead not supported on hardware");
    return 0;
#endif
}

int Platform::GpuEscapeWrite(UINT32 GpuId, const char *Path, UINT32 Index, UINT32 Size, UINT32 Value)
{
#if defined(INCLUDE_GPU)
    return EscapeWrite(Path, Index, Size, Value);
#else
    MASSERT(!"Platform::GpuEscapeWrite not supported on hardware");
    return 0;
#endif
}

int Platform::GpuEscapeRead(UINT32 GpuId, const char *Path, UINT32 Index, UINT32 Size, UINT32 *Value)
{
#if defined(INCLUDE_GPU)
    return EscapeRead(Path, Index, Size, Value);
#else
    MASSERT(!"Platform::GpuEscapeRead not supported on hardware");
    return 0;
#endif
}

int Platform::GpuEscapeWriteBuffer(UINT32 GpuId, const char *Path, UINT32 Index, size_t Size, const void *Buf)
{
    MASSERT(!"Platform::GpuEscapeWriteBuffer not supported on hardware");
    return 0;
}

int Platform::GpuEscapeReadBuffer(UINT32 GpuId, const char *Path, UINT32 Index, size_t Size, void *Buf)
{
    MASSERT(!"Platform::GpuEscapeReadBuffer not supported on hardware");
    return 0;
}

RC Platform::VerifyGpuId(UINT32 pciDomainAddress, UINT32 pciBusAddress, UINT32 pciDeviceAddress, UINT32 pciFunctionAddress, UINT32 resmanGpuId)
{
    // we don't want this to fail, or to force high-level sw to test for !hw
    return OK;
}

///------------------------------------------------------------------------------
// Amodel hooks to communicate information normally stored in instmem
//
void Platform::AmodAllocChannelDma(UINT32 ChID, UINT32 Class, UINT32 CtxDma,
                                   UINT32 ErrorNotifierCtxDma)
{
}

void Platform::AmodAllocChannelGpFifo(UINT32 ChID, UINT32 Class, UINT32 CtxDma,
                                      UINT64 GpFifoOffset, UINT32 GpFifoEntries,
                                      UINT32 ErrorNotifierCtxDma)
{
}

void Platform::AmodFreeChannel(UINT32 ChID)
{
}

void Platform::AmodAllocContextDma(UINT32 ChID, UINT32 Handle, UINT32 Class,
                                   UINT32 Target, UINT32 Limit, UINT32 Base,
                                   UINT32 Protect)
{
}

void Platform::AmodAllocObject(UINT32 ChID, UINT32 Handle, UINT32 Class)
{
}

void Platform::AmodFreeObject(UINT32 ChID, UINT32 Handle)
{
}

const char* Platform::ModelIdentifier(void)
{
    MASSERT(!"AModel is not supported");
    return "h";
}

void Platform::DumpAddrData(const volatile void* Addr, const void* Data,
                            UINT32 Count, const char* Prefix)
{
}

void Platform::DumpAddrData(const PHYSADDR Addr, const void* Data,
                            UINT32 Count, const char* Prefix)
{
}

void Platform::DumpCfgAccessData(INT32 DomainNumber, INT32 BusNumber,
                                 INT32 DeviceNumber, INT32 FunctionNumber,
                                 INT32 Address, const void* Data,
                                 UINT32 Count, const char* Prefix)
{
}

void Platform::DumpEscapeData(UINT32 GpuId, const char *Path,
                              UINT32 Index, size_t Size,
                              const void* Buf, const char* Prefix)
{
}

bool Platform::PollfuncWrap(PollFunc pollFunc, void *pArgs,
                            const char* pollFuncName)
{
    return pollFunc(pArgs);
}

void Platform::PreGpuMonitorThread()
{
}

RC Platform::GetForcedLwLinkConnections(UINT32 gpuInst, UINT32 numLinks, UINT32 *pLinkArray)
{
    return RC::UNSUPPORTED_FUNCTION;
}

RC Platform::GetForcedC2CConnections(UINT32 gpuInst, UINT32 numLinks, UINT32 *pLinkArray)
{
    return RC::UNSUPPORTED_FUNCTION;
}

RC Platform::VirtualAlloc(void **ppReturnedAddr, void *pBase, size_t size, UINT32 protect, UINT32 vaFlags)
{
    return Xp::AllocAddressSpace(size,
                                 ppReturnedAddr,
                                 pBase,
                                 protect,
                                 vaFlags);
}

void *Platform::VirtualFindFreeVaInRange(size_t size, void *pBase, void *pLimit, size_t align)
{
    return Xp::VirtualFindFreeVaInRange(size, pBase, pLimit, align);
}

RC Platform::VirtualFree(void *pAddr, size_t size, Memory::VirtualFreeMethod vfMethod)
{
    return Xp::FreeAddressSpace(pAddr, size, vfMethod);
}

RC Platform::VirtualProtect(void *pAddr, size_t size, UINT32 protect)
{
    return Xp::VirtualProtect(pAddr, size, protect);
}

RC Platform::VirtualMadvise(void *pAddr, size_t size, Memory::VirtualAdvice advice)
{
    return Xp::VirtualMadvise(pAddr, size, advice);
}

bool Platform::IsVirtFunMode()
{
#ifdef INCLUDE_GPU
    static bool s_IsVirtFunModeCache = !Xp::GetElw("SRIOV_GFID").empty();
    return s_IsVirtFunModeCache;
#else
    return false;
#endif
}

bool Platform::IsPhysFunMode()
{
#ifdef INCLUDE_GPU
    static bool s_IsPhysFunModeCache =
        !VmiopElwManager::Instance()->GetConfigFile().empty();
    return s_IsPhysFunModeCache;
#else
    return false;
#endif
}

bool Platform::IsDefaultMode()
{
    return !IsVirtFunMode() && !IsPhysFunMode();
}

UINT32 Platform::GetReservedSysmemMB()
{
    MASSERT(!"Function not implemented in non-SIM platforms");
    return 0;
}

const vector<const char*>& Platform::GetChipLibArgV()
{
    MASSERT(!"Function not implemented in non-SIM platforms");
    const static vector<const char*> args;
    return args;
}

int Platform::VerifyLWLinkConfig(size_t Size, const void* Buf, bool supersetOK)
{
    MASSERT(!"Function not implemented in non-SIM platforms");
    return 0;
}

bool Platform::IsSelfHosted()
{
    return false;
}

bool Platform::ManualCachingEnabled()
{
    return false;
}
