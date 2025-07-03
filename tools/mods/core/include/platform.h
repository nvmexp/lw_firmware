/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 1999-2022 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#pragma once

#include "irqparams.h"
#include "memtypes.h"
#include "rc.h"

#ifdef INCLUDE_PEATRANS
#include "cheetah/peatrans/peatrans.h"
#endif

#include <string>

class GpuDevice;

//! Platform abstraction to hide details of HW vs. simulation
namespace Platform
{
    RC Initialize();
    RC Reset();
    enum CleanupMode {CheckForLeaks, DontCheckForLeaks, MininumCleanup};
    void Cleanup(CleanupMode Mode);
    bool IsInitialized();
    void SetForcedTermination();
    bool IsForcedTermination();

    // Platform::Cleanup iterates through this list of cleanup functions
    // in REVERSE order (ie stack behavior - LIFO)
    void AddCleanupCallback(void (*pCleanupFn)(void *), void *context);

    //! Return true if the native UI is disabled (i.e. mods owns the gpus).
    bool IsUserInterfaceDisabled();

    //! Disable the user interface.
    RC DisableUserInterface();

    //! Disable the user interface and set the display mode.
    /**
     * DisableUserInterface should take care of a few things:
     *   - Blank the screen.
     *   - Set the resolution, depth, and refresh rate correctly.
     *   - Hide the cursor (if there is one).
     *   - Ensure that framebuffer memory is available for
     *     LwRm::VidHeap* (for instance, under Windows, GDI
     *     claims the memory for itself, and we have to use
     *     DirectDraw to get GDI to release it).
     *
     * @param Width         Desired display width (in pixels).
     * @param Height        Desired display height (in pixels).
     * @param Depth         Desired display depth (bpp).
     * @param RefreshRate   Desired refresh rate.
     * @param FilterTaps    Desired filter-on-scanout filter taps.
     * @param ColorComp     Desired compressed-color-surface decompression mode.
     * @param DdScalerMode  Desired digital display scaler mode.
     * @return A MODS return code.
     */
    RC DisableUserInterface
    (
        UINT32 Width,
        UINT32 Height,
        UINT32 Depth,
        UINT32 RefreshRate,
        UINT32 FilterTaps   = 1,     // default no filtering (1-tap filter)
        UINT32 ColorComp    = 1,     // default no compression (1-pixel tiles)
        UINT32 DdScalerMode = 2,     // default is scaled
        GpuDevice *pGrDev = 0
    );

    //!< Enable the user interface.
    RC EnableUserInterface();
    RC EnableUserInterface(GpuDevice* /*unused*/);

    bool IsTegra(); //!< Indicate whether we're running on TegraSOC.
    bool IsPPC();

    //! Find a PCI device with given 'DeviceId', 'VendorId', and 'Index'.
    //! Return '*pBusNumber', '*pDeviceNumber', and '*pFunctionNumber'.
    RC FindPciDevice(INT32 DeviceId, INT32 VendorId, INT32 Index,
                     UINT32 * pDomainNumber = 0,
                     UINT32 * pBusNumber = 0, UINT32 * pDeviceNumber = 0,
                     UINT32 * pFunctionNumber = 0);

    //! Find a PCI class code given 'ClassCode' and 'Index'.
    //! Return '*pBusNumber', '*pDeviceNumber', and '*pFunctionNumber'.
    RC FindPciClassCode(INT32 ClassCode, INT32 Index,
                        UINT32 * pDomainNumber = 0,
                        UINT32 * pBusNumber = 0, UINT32 * pDeviceNumber = 0,
                        UINT32 * pFunctionNumber = 0);

    //! Force a rescan of PCI bus.
    RC RescanPciBus(INT32 domain, INT32 bus);

    //! Read and write virtual addresses
    void   VirtualRd(const volatile void *Addr, void *Data, UINT32 Count);
    void   VirtualWr(volatile void *Addr, const void *Data, UINT32 Count);
    UINT08 VirtualRd08(const volatile void *Addr);
    UINT16 VirtualRd16(const volatile void *Addr);
    UINT32 VirtualRd32(const volatile void *Addr);
    UINT64 VirtualRd64(const volatile void *Addr);
    void   VirtualWr08(volatile void *Addr, UINT08 Data);
    void   VirtualWr16(volatile void *Addr, UINT16 Data);
    void   VirtualWr32(volatile void *Addr, UINT32 Data);
    void   VirtualWr64(volatile void *Addr, UINT64 Data);
    UINT32 VirtualXchg32(volatile void *Addr, UINT32 Data);

    //! Read and write physical addresses
    RC     PhysRd(PHYSADDR Addr, void *Data, UINT32 Count);
    RC     PhysWr(PHYSADDR Addr, const void *Data, UINT32 Count);
    UINT08 PhysRd08(PHYSADDR Addr);
    UINT16 PhysRd16(PHYSADDR Addr);
    UINT32 PhysRd32(PHYSADDR Addr);
    UINT64 PhysRd64(PHYSADDR Addr);
    void   PhysWr08(PHYSADDR Addr, UINT08 Data);
    void   PhysWr16(PHYSADDR Addr, UINT16 Data);
    void   PhysWr32(PHYSADDR Addr, UINT32 Data);
    void   PhysWr64(PHYSADDR Addr, UINT64 Data);
    UINT32 PhysXchg32(PHYSADDR Addr, UINT32 Data);

    //! Copies large blocks of memory efficiently.
    //! On some platforms, will use SSE for 16-byte reads & writes.
    void MemCopy(volatile void *Dst, const volatile void *Src, size_t Count);
    void MemSet(volatile void *Dst, UINT08 Data, size_t Count);

    //! Copies large blocks of memory.
    //! Will never use SSE (used mainly for devices where SSE is not supported).
    void MemCopy4Byte
    (
        volatile void *Dst,
        const volatile void *Src,
        size_t Count
    );

    //! Returns true if this platform uses "wide" (i.e. > 4 byte)
    //! CPU load/store instructions for MemCopy.
    bool HasWideMemCopy();

    size_t GetPageSize();
    //!< Return the page size in bytes for this platform.

    void DumpPageTable();
        //!< Dump the lwrrently mapped pages

    //! Test for system memory size.
    RC TestAllocPages
    (
        size_t      NumBytes
    );

    enum { UNSPECIFIED_GPUINST = ~0 };

    //! Allocate system memory.
    //!
    //! Allocates whole pages of system memory (numBytes will be rounded up).
    //! All allocated pages are locked (non-movable) and are usable with other
    //! HW devices (e.g. GPUs).
    //!
    //! @param numBytes    Number of bytes to allocate, rounded up to page size.
    //! @param pDescriptor Returned descriptor/identifier for the allocated memory.
    //! @param contiguous  If true, the allocated pages are adjacent and in-order.
    //! @param addressBits Number of bits required to represent physical addresses
    //!                    of each allocated page.
    //! @param attrib      Cache attributes, typically WB (write-back) or
    //!                    WC (write-combine).  Avoid UC (uncached) which is not
    //!                    supported by some architectures.  Other attributes
    //!                    are not supported.
    //! @param domain      Location of a device on the PCI bus for which the memory
    //!                    is allocated.  The memory will be allocated from the
    //!                    same NUMA node on which the PCI device is.  Also, the
    //!                    memory will be DMA-mapped to that PCI device.  This
    //!                    parameter is supported only in linuxmfg MODS, it is
    //!                    ignored in other builds.
    //! @param bus         See above.
    //! @param device      See above.
    //! @param function    See above.
    RC AllocPages
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
    );

    //! Alloc system memory.
    //! A whole number of pages will be mapped (NumBytes will be rounded up).
    //! All pages will be locked and will be below physical address 2**AddressBits.
    //! If Contiguous is true, physical pages will be adjacent and in-order.
    RC AllocPages
    (
        size_t          NumBytes,
        void **         pDescriptor,
        bool            Contiguous,
        UINT32          AddressBits,
        Memory::Attrib  Attrib,
        UINT32          GpuInst,
        Memory::Protect Protect = Memory::ProtectDefault
    );

    //! Alloc contiguous System memory pages, with the starting physical
    //! address aligned as requested.
    //! A whole number of pages will be mapped (NumBytes will be rounded up).
    //! All pages will be locked and will be below physical address 2**AddressBits.
    //! PhysicalAlignment must be a power of 2, values less than the system
    //! page size will be ignored.
    RC AllocPagesAligned
    (
        size_t         NumBytes,
        void **        pDescriptor,
        size_t         PhysicalAlignment,
        UINT32         AddressBits,
        Memory::Attrib Attrib,
        UINT32         GpuInst
    );

    //! Alloc system memory.
    //! A whole number of pages will be mapped (NumBytes will be rounded up).
    //! All pages will be locked and will be below physical address 2**AddressBits.
    //! If Contiguous is true, physical pages will be adjacent and in-order.
    //! The pages will be allocated in the 4GB arena in which the address ConstrainAddr
    //! is located.
    RC AllocPagesConstrained
    (
        size_t         NumBytes,
        void **        pDescriptor,
        bool           Contiguous,
        UINT32         AddressBits,
        Memory::Attrib Attrib,
        PHYSADDR       ConstrainAddr,
        UINT32         GpuInst
    );

    //! Alloc contiguous System memory pages, with the starting physical
    //! address aligned as requested.
    //! A whole number of pages will be mapped (NumBytes will be rounded up).
    //! All pages will be locked and will be below physical address 2**AddressBits.
    //! PhysicalAlignment must be a power of 2, values less than the system
    //! page size will be ignored.
    //! The pages will be allocated in the 4GB arena in which the address ConstrainAddr
    //! is located.
    RC AllocPagesConstrainedAligned
    (
        size_t         NumBytes,
        void **        pDescriptor,
        size_t         PhysicalAlignment,
        UINT32         AddressBits,
        Memory::Attrib Attrib,
        PHYSADDR       ConstrainAddr,
        UINT32         GpuInst
    );

    //! Free pages allocated by AllocPages or AllocPagesAligned.
    void FreePages(void * Descriptor);

    //! Merge pages from multiple allocations into one allocation.
    //! @param pOutDescriptor Pointer where the new descriptor is placed by this function.
    //! @param pInDescBegin   pInDescBegin and pInDescEnd together specify an STL-style range
    //!                       of descriptors to merge.  All descriptors to merge must have
    //!                       the same attributes (cache, address bits, same PCI device)
    //!                       and DmaMapMemory() must not have been called on them.
    //!                       The input descriptors are gutted during the merge and
    //!                       if the function fails, some of them will become unusable
    //!                       and will be set to nullptr.  Upon failure, any memory
    //!                       merged will be put back in one of the input descriptors.
    //!                       The caller is always responsible for freeing
    //!                       all these descriptors with FreePages(), regardless of whether 
    //!                       this function passed or failed.
    //! @param pInDescEnd     Pointer to the next element after the range of input
    //!                       descriptors to merge.
    RC MergePages(void** pOutDescriptor, void** pInDescBegin, void** pInDescEnd);

    //! Map pages allocated by AllocPages or AllocPagesAligned into the virtual
    //! address space.
    RC MapPages
    (
        void **pReturnedVirtualAddress,
        void * Descriptor,
        size_t Offset,
        size_t Size,
        Memory::Protect Protect
    );

    //! Unmap pages mapped by MapPages.
    void UnMapPages(void * VirtualAddress);

    //! Gets the physical address of a byte inside an allocation from AllocPages
    //! or AllocPagesAligned, without having to map it.
    PHYSADDR GetPhysicalAddress(void *Descriptor, size_t Offset);

    //! Gets the address returned by pci_map_page()
    PHYSADDR GetMappedPhysicalAddress
    (
        UINT32 domain,
        UINT32 bus,
        UINT32 device,
        UINT32 func,
        void *Descriptor,
        size_t Offset
    );
    //! Maps the memory onto the specified device
    RC DmaMapMemory
    (
        UINT32 domain,
        UINT32 bus,
        UINT32 device,
        UINT32 func,
        void  *descriptor
    );
    //! Unmaps the memory from the specified device
    RC DmaUnmapMemory
    (
        UINT32 domain,
        UINT32 bus,
        UINT32 device,
        UINT32 func,
        void  *descriptor
    );
    //! Tells the kernel about the size of addressable range by the PCI device
    //!
    //! @param numBits Size of addressable PCI DMA range, in bits.
    RC SetDmaMask
    (
        UINT32 domain,
        UINT32 bus,
        UINT32 device,
        UINT32 func,
        UINT32 numBits
    );

    //! Reserve resources for subsequent allocations of the same Attrib
    //! by AllocPages or AllocPagesAligned.
    //! Under DOS, this will pre-allocate a contiguous heap and MTRR reg.
    //! Platforms that use real paging memory managers may ignore this.
    void ReservePages
    (
        size_t NumBytes,
        Memory::Attrib Attrib
    );

    //! Map device memory into the page table, get virtual address to use.
    //! Multiple mappings are legal, and give different virtual addresses
    //! on paging systems.
    //! Under DOS, this is faked more-or-less correctly using MTRRs.
    RC MapDeviceMemory
    (
        void **     pReturnedVirtualAddress,
        PHYSADDR    PhysicalAddress,
        size_t      NumBytes,
        Memory::Attrib Attrib,
        Memory::Protect Protect
    );

    //! Undo the mapping created by MapDeviceMemory.
    void UnMapDeviceMemory(void * VirtualAddress);

    RC SetMemRange
    (
        PHYSADDR    PhysicalAddress,
        size_t      NumBytes,
        Memory::Attrib Attrib
    );
        //!< Program a Memory Type and Range Register to control cache mode
        //!< for a range of physical addresses.
        //!< Called by the resman to make the (unmapped) AGP aperture WC.
    RC UnSetMemRange
    (
        PHYSADDR    PhysicalAddress,
        size_t      NumBytes
    );

    RC GetAtsTargetAddressRange
    (
        UINT32     gpuInst,
        UINT32    *pAddr,
        UINT32    *pGuestAddr,
        UINT32    *pAddrWidth,
        UINT32    *pMask,
        UINT32    *pMaskWidth,
        UINT32    *pGranularity,
        MODS_BOOL  bIsPeer,
        UINT32     peerIndex
    );

    RC GetLwLinkLineRate
    (
        UINT32 domain,
        UINT32 bus,
        UINT32 device,
        UINT32 func,
        UINT32 *pSpeed
    );

    //! Colwert physical address to virtual address.
    void * PhysicalToVirtual(PHYSADDR physicalAddress, Tee::Priority pri);
    inline void * PhysicalToVirtual(PHYSADDR physicalAddress)
    {
        return PhysicalToVirtual(physicalAddress, Tee::PriError);
    }

    //! Colwert virtual address to physical address.
    UINT32   VirtualToPhysical32(volatile void * virtualAddress);
    PHYSADDR VirtualToPhysical(volatile void * virtualAddress, Tee::Priority pri);
    inline PHYSADDR VirtualToPhysical(volatile void * virtualAddress)
    {
        return VirtualToPhysical(virtualAddress, Tee::PriError);
    }

    RC    VirtualAlloc(void **ppReturnedAddr, void *pBase, size_t size, UINT32 protect, UINT32 vaFlags);
    void *VirtualFindFreeVaInRange(size_t size, void *pBase, void *pLimit, size_t align);
    RC    VirtualFree(void *pAddr, size_t size, Memory::VirtualFreeMethod vfMethod);
    RC    VirtualProtect(void *pAddr, size_t size, UINT32 protect);
    RC    VirtualMadvise(void *pAddr, size_t size, Memory::VirtualAdvice advice);

    RC GetCarveout(PHYSADDR* pAddr, UINT64* pSize);
        //!< Get physical address and size of carveout.

    RC GetVPR(PHYSADDR* pAddr, UINT64* pSize, void **pMemDesc = nullptr);
        //!< Get video protection region preallocated by MODS.

    RC GetGenCarveout(PHYSADDR *pAddr, UINT64 *pSize, UINT32 id, UINT64 align);

    RC CallACPIGeneric(UINT32 GpuInst,
                       UINT32 Function,
                       void *Param1,
                       void *Param2,
                       void *Param3);
    RC LWIFMethod(UINT32 Function, UINT32 SubFunction, void *InParams,
                  UINT16 InParamSize, UINT32 *OutStatus, void *OutData,
                  UINT16 *OutDataSize);

    RC IlwalidateCpuCache();
        //!< Ilwalidate the CPU's cache without flushing it.  (Very dangerous!)

    RC FlushCpuCache();
        //!< Flush the CPU's cache.

    RC FlushCpuCacheRange(void * StartAddress, void * EndAddress, UINT32 Flags);
        //!< Flush the CPU's cache from 'StartAddress' to 'EndAddress'.

    RC FlushCpuWriteCombineBuffer();
        //!< Flush the CPU's write combine buffer.

    enum PpcTceBypassMode
    {
        PPC_TCE_BYPASS_DEFAULT
       ,PPC_TCE_BYPASS_FORCE_ON
       ,PPC_TCE_BYPASS_FORCE_OFF
    };
    RC SetupDmaBase
    (
        UINT16           domain,
        UINT08           bus,
        UINT08           dev,
        UINT08           func,
        PpcTceBypassMode bypassMode,
        UINT64           devDmaMask,
        PHYSADDR *       pDmaBaseAddr
    );
        //!< PPC specific interface for setting up dma bypass

    //! Reads BAR info from PCI device
    /**
     * @param domainNumber   PCI domain of the device to get BAR info from
     * @param busNumber      PCI bus of the device to get BAR info from
     * @param deviceNumber   PCI device index of the device to get BAR info from
     * @param functionNumber PCI function index of the device to get BAR info from
     * @param barIndex       Index of of BAR to get information about
     * @param pBaseAdress    Pointer to location to store base address of inidicated BAR
     * @param pBarSize       Pointer to location to store size of indicated BAR
     *
     * @return RC
     *
     * @sa PciGetBarInfo
     */
    RC PciGetBarInfo(UINT32 domainNumber, UINT32 busNumber,
                     UINT32 deviceNumber, UINT32 functionNumber,
                     UINT32 barIndex, PHYSADDR* pBaseAddress, UINT64* pBarSize);

    //! Enable or disable SR-IOV on a PCIe device
    //!
    //! \param domain,bus,device,function Identifies the physical PCIe device
    //! \param vfCount Number of VF (virtual functions) to enable, or 0
    //!     to disable SR-IOV
    RC PciEnableSriov(UINT32 domain, UINT32 bus, UINT32 device,
                      UINT32 function, UINT32 vfCount);

    //! Reads IRQ number of pci device
        /**
         * @param domainNumber   PCI domain of the device to get IRQ from
         * @param busNumber      PCI bus of the device to get IRQ from
         * @param deviceNumber   PCI device index of the device to get IRQ from
         * @param functionNumber PCI function index of the device to get IRQ from
         * @param pIrq           Pointer to location to store IRQ number
         *
         * @return RC
         *
         * @sa PciGetIRQ
         */
    RC PciGetIRQ(UINT32 domainNumber, UINT32 busNumber,
                 UINT32 deviceNumber, UINT32 functionNumber,
                 UINT32 * pIrq);

    /**
     * @name PCI config read functions.
     */
    //@{

    /**
     * Read byte, word, or dword at @a Address from a PCI device with the given
     * @a DomainNumber, @a BusNumber, @a DeviceNumber, and @a FunctionNumber'.
     * Return read data in @a *pData.
     */
    RC PciRead08(INT32 DomainNumber, INT32 BusNumber, INT32 DeviceNumber,
                 INT32 FunctionNumber, INT32 Address, UINT08 * pData);
    RC PciRead16(INT32 DomainNumber, INT32 BusNumber, INT32 DeviceNumber,
                 INT32 FunctionNumber, INT32 Address, UINT16 * pData);
    RC PciRead32(INT32 DomainNumber, INT32 BusNumber, INT32 DeviceNumber,
                 INT32 FunctionNumber, INT32 Address, UINT32 * pData);
    //@}

    /**
     * @name PCI config write functions.
     */
    //@{

    /**
    * Write @a Data at @a Address to a PCI device with the given
    * @a DomainNumber, @a BusNumber, @a DeviceNumber, and @a FunctionNumber.
    */
    RC PciWrite08(INT32 DomainNumber, INT32 BusNumber, INT32 DeviceNumber,
                  INT32 FunctionNumber, INT32 Address, UINT08 Data);
    RC PciWrite16(INT32 DomainNumber, INT32 BusNumber, INT32 DeviceNumber,
                  INT32 FunctionNumber, INT32 Address, UINT16 Data);
    RC PciWrite32(INT32 DomainNumber, INT32 BusNumber, INT32 DeviceNumber,
                  INT32 FunctionNumber, INT32 Address, UINT32 Data);
    //@}

    RC PciRemove(INT32 domainNumber, INT32 busNumber, INT32 deviceNumber,
                  INT32 functionNumber);
    RC PciResetFunction(INT32 domainNumber, INT32 busNumber,
                        INT32 deviceNumber, INT32 functionNumber);

    /**
     * @name Port read functions.
     */
    //@{
    /**
     * Read byte, word, or dword from @a Port.  Return read data in @a pData.
     * Return RC::UNSUPPORTED_FUNCTION if not implemented.
     */
    RC PioRead08(UINT16 Port, UINT08 * pData);
    RC PioRead16(UINT16 Port, UINT16 * pData);
    RC PioRead32(UINT16 Port, UINT32 * pData);
    //@}

    /** @name Port write functions.
     */
    //@{
    //! Write @a Data to @a Port.  Return RC::UNSUPPORTED_FUNCTION if not
    //! implemented.
    RC PioWrite08(UINT16 Port, UINT08 Data);
    RC PioWrite16(UINT16 Port, UINT16 Data);
    RC PioWrite32(UINT16 Port, UINT32 Data);
    //@}

    /**
     * @name Interrupt functions.
     */
    //@{

    //! Called to determine if specific irq type is supported by platform
    /**
     * @param irqType     IRQ type being queried
     *
     * @return bool
     *
     */
    bool IRQTypeSupported(UINT32 irqType);

    //! Called to allocate IRQs
    //! Allocates and maps a number of IRQs.
    //! and passes them back to the caller
    //! so that the caller can use the logical irq number to hook
    //! the mods isr
    /**
     * @param pciDomain   PCI domain of the device we are allocating interrupts for
     * @param pciBus      PCI bus of the device we are allocating interrupts for
     * @param pciDevice   PCI device index of the device we are allocating interrupts for
     * @param pciFunction PCI function index of the device we are allocating interrupts for
     * @param lwecs       Number of vectors requested
     * @param irqs        Returns array of allocated IRQs
     * @param flags       type and afinity of IRQs to allocate
     *
     * @return RC
     *
     */
    RC AllocateIRQs
    (
        UINT32 pciDomain,
        UINT32 pciBus,
        UINT32 pciDevice,
        UINT32 pciFunction,
        UINT32 lwecs,
        UINT32 *irqs,
        UINT32 flags
    );

    //! Called to free IRQs
    /**
     * @param pciDomain   PCI domain of the device we are freeing interrupts for
     * @param pciBus      PCI bus of the device we are freeing interrupts for
     * @param pciDevice   PCI device index of the device we are freeing interrupts for
     * @param pciFunction PCI function index of the device we are freeing interrupts for
     *
     */
    void FreeIRQs(UINT32 pciDomain, UINT32 pciBus, UINT32 pciDevice, UINT32 pciFunction);

    //! Process any pending interrupts
    void ProcessPendingInterrupts();

    //! Report processed interrupt
    void InterruptProcessed();

    //! Called when an interrupt is received.
    void HandleInterrupt(UINT32 Irq);

   //! Allocates and maps an IRQ. Maps the physical Irq to a logical
   //! irq number, enables it, and passes it back to the caller
   //! so that the caller can use the logical irq number to hook
   //! the mods isr
    /**
     * @param logicalIrq Store the number of logical irq that is allocated
     * @param physIrq    Physical IRQ index.
     * @param DtName     DT name corresponding to the device
     * @param FullName   Full name of device as define in device tree
     *
     * @return RC
     *
     */
    RC MapIRQ(UINT32 * logicalIrq, UINT32 physIrq, string dtName, string fullName);

    //! Hooks an ISR to an IRQ.
    /**
     * @param Irq    IRQ to hook the ISR to.
     * @param func   ISR to hook
     * @param cookie data to pass to ISR
     *
     * @return RC
     *
     * @sa UnhookIRQ
     */
    RC HookIRQ(UINT32 Irq, ISR func, void* cookie);

    //! Unhooks an ISR from an IRQ.
    /**
     * @param Irq    IRQ to unhook the ISR from.
     * @param func   ISR to unhook
     * @param cookie data to pass to ISR
     *
     * @return RC
     *
     * @sa HookIRQ
     */
    RC UnhookIRQ(UINT32 Irq, ISR func, void* cookie);

    //! Enable an IRQ.
    /**
     * @param Irq    IRQ to be enabled
     *
     * @return RC
     *
     * @sa DisableIRQ
     */
    RC EnableIRQ(UINT32 Irq);

    //! Disable an IRQ
    /**
     * @param Irq    IRQ to be disable
     *
     * @return RC
     *
     * @sa EnableIRQ
     */
    RC DisableIRQ(UINT32 Irq);

    //! check whether an IRQ is hooked.
    /**
     * @param Irq    IRQ to be checked
     *
     * @return bool
     */
    bool IsIRQHooked(UINT32 Irq);

    //! Hook an ISR to a PCIe interrupt and specify how to mask the interrupt.
    /**
     * @param IrqParams   Structure containing all the required masking info.
     * @param func       ISR to hook.
     * @param cookie     Data to pass to ISR
     *
     * @return RC
     *
     * @sa UnhookInt
     */
    RC HookInt(const IrqParams &irqParams, ISR func, void* cookie);

    //! Unhooks an ISR from a PCIe interrupt.
    /**
     * @param irqInfo     Same data that was passed to HookInt()
     * @param func        ISR to hook.
     * @param cookie      data to pass to ISR
     *
     * @return RC
     *
     * @sa HookInt
     */
    RC UnhookInt(const IrqParams& irqParams, ISR func, void* cookie);

    //! Enable external hardware interrupts.
    //! Deprecated in favor of LowerIrql
    void EnableInterrupts();

    //! Disable external hardware interrupts.
    //! Deprecated in favor of RaiseIrql
    void DisableInterrupts();

    //! Returns the current IRQL
    Irql GetLwrrentIrql();

    //! Increases the IRQL and returns the previous IRQL.  Asserts that you are
    //! actually raising the IRQL and not lowering it.
    Irql RaiseIrql(Irql NewIrql);

    //! Decreases the IRQL.  Asserts that you are actually lowering the IRQL and
    //! not raising it.
    void LowerIrql(Irql NewIrql);

    enum SimulationMode
    {
        Hardware,
        RTL,
        Fmodel,
        Amodel,
    };

    //! Pause for a short period of time, guaranteeing that simulators will
    //! advance, and trying to put the CPU into a low-power state or a state
    //! that consumes less resources on an SMT system if possible.  This
    //! function should primarily be used in cases where spin loops that do
    //! not yield are unavoidable -- usually inside ISRs only.
    void Pause();

    //! Pause the thread for given amount of 'Microseconds'.
    //! This function DOES NOT yield the CPU to other threads.
    //! Warning, keep the delay as short as possible so the other threads
    //! do not starve.
    //! Also see Tasker::Sleep() for longer delay periods (milliseconds)
    //! Also see Platform::SleepUs for short delays that do yield the CPU
    void Delay(UINT32 Microseconds);
    void DelayNS(UINT32 Nanoseconds);

    //! In non-HW elwironments this function relies on a correct implementation
    //! of IChip::GetSimulatorTimeUnitsNS.  That is, it assumes that the
    //! hardware simulator correctly reports how much time has passed.
    //
    //! It only guarantees that it will sleep for at *least*
    //! MinMicrosecondsToSleep because yielding the processor could result
    //! in a delay longer than the time specified.
    void SleepUS(UINT32 MinMicrosecondsToSleep);

    //! Return the amount of time that has elapsed since the beginning of time.
    //! Return simulator time on fmodel, amodel, and RTL,
    //! and wall-clock time on hardware and emulation.
    UINT64 GetTimeNS(); // nsecs
    UINT64 GetTimeUS(); // usecs
    UINT64 GetTimeMS(); // msecs

    //! Should gdb load sim symbols for stack dumping?
    bool LoadSimSymbols();

    //! Do we need to dump call stack?
    bool DumpCallStack();

    //! Returns true on platforms where the Resource Manager driver runs as a
    //! part of MODS (static or dynamic library) rather than as part
    //! of the OS kernel.
    //!
    //! When this is true, MODS acts as the operating system, and is responsible
    //! for initializing the resman, hooking GPU interrupts, supplying sysmem
    //! alloc/free callbacks, etc.
    //!
    //! True in SIM packages (even with silicon).
#ifdef CLIENT_SIDE_RESMAN
    constexpr bool HasClientSideResman() { return true; }
#else
    constexpr bool HasClientSideResman() { return false; }
#endif

#ifdef CLIENT_SIDE_LWLINK
    constexpr bool HasClientSideLwLink() { return true; }
#else
    constexpr bool HasClientSideLwLink() { return false; }
#endif

#ifdef CLIENT_SIDE_LWSWITCH
    constexpr bool HasClientSideLwSwitch() { return true; }
#else
    constexpr bool HasClientSideLwSwitch() { return false; }
#endif

    //! Returns true if MODS uses the lwgpu driver instead of RM.
#ifdef TEGRA_MODS
    constexpr bool UsesLwgpuDriver() { return true; }
#else
    constexpr bool UsesLwgpuDriver() { return false; }
#endif

    //! Simulator functions
    bool IsChiplibLoaded();
    void ClockSimulator(int Cycles);
    UINT64 GetSimulatorTime();         //!< Returns clock ticks
    double GetSimulatorTimeUnitsNS();  //!< Returns ns per clock tick
    SimulationMode GetSimulationMode();
    string SimulationModeToString(SimulationMode mode);
    bool IsAmodel();
    int EscapeWrite(const char *Path, UINT32 Index, UINT32 Size, UINT32 Value);
    int EscapeRead(const char *Path, UINT32 Index, UINT32 Size, UINT32 *Value);

    int GpuEscapeWrite(UINT32 GpuId, const char *Path, UINT32 Index, UINT32 Size, UINT32 Value);
    int GpuEscapeRead(UINT32 GpuId, const char *Path, UINT32 Index, UINT32 Size, UINT32 *Value);
    int GpuEscapeWriteBuffer(UINT32 GpuId, const char *Path, UINT32 Index, size_t Size, const void *Buf);
    int GpuEscapeReadBuffer(UINT32 GpuId, const char *Path, UINT32 Index, size_t Size, void *Buf);
    RC VerifyGpuId(UINT32 pciDomainAddress, UINT32 pciBusAddress, UINT32 pciDeviceAddress, UINT32 pciFunctionAddress, UINT32 resmanGpuId);

    //! Amodel hooks to communicate information normally stored in instmem
    void AmodAllocChannelDma(UINT32 ChID, UINT32 Class, UINT32 CtxDma,
                             UINT32 ErrorNotifierCtxDma);
    void AmodAllocChannelGpFifo(UINT32 ChID, UINT32 Class, UINT32 CtxDma,
                                UINT64 GpFifoOffset, UINT32 GpFifoEntries,
                                UINT32 ErrorNotifierCtxDma);
    void AmodFreeChannel(UINT32 ChID);
    void AmodAllocContextDma(UINT32 ChID, UINT32 Handle, UINT32 Class,
                             UINT32 Target, UINT32 Limit, UINT32 Base,
                             UINT32 Protect);
    void AmodAllocObject(UINT32 ChID, UINT32 Handle, UINT32 Class);
    void AmodFreeObject(UINT32 ChID, UINT32 Handle);
    bool PassAdditionalVerification(GpuDevice *pGpuDevice, const char* TracePath);
    UINT32 ChipLibVersion();
    const char* ModelIdentifier();

    bool ManualCachingEnabled();

    // Helper functions used in Initialize()
    RC SetupQueryIface(void* Module);
    RC SetupIInterrupt(void* Module);
    RC SetupIInterrupt3(void* Module);
    RC SetupIBusMem(void* Module);

    //! Is extended memory allocation/mapping supported
    bool ExtMemAllocSupported();

    //! Print the Address and Data
    void DumpAddrData(const volatile void* Addr, const void* Data,
                      UINT32 Count, const char* Prefix);
    void DumpAddrData(const PHYSADDR Addr, const void* Data,
                      UINT32 Count, const char* Prefix);
    void DumpCfgAccessData(INT32 DomainNumber, INT32 BusNumber,
                           INT32 DeviceNumber, INT32 FunctionNumber,
                           INT32 Address, const void* Data,
                           UINT32 Count, const char* Prefix);
    void DumpEscapeData(UINT32 GpuId, const char *Path,
                        UINT32 Index, size_t Size,
                        const void* Buf, const char* Prefix);

    //! Enable the physical dump access at the PolicyManager
    void SetDumpPhysicalAccess(bool flag);
    bool GetDumpPhysicalAccess();

    //! Common breakpoint handling before calling Xp::Breakpoint.
    void BreakPoint(RC reasonRc, const char *file, int line);
    RC IgnoreBreakPoint(const string &file, int line);
    enum { IGNORE_ALL_LINES = ~0 };

    typedef void (*BreakPointHandler)(void*);

    //! Safe way to make sure that the breakpoint handler will be unhooked
    class BreakPointHooker
    {
        private:
            bool   m_Hooked;
            void * m_pOrigHook;
        public:
            BreakPointHooker();
            BreakPointHooker(BreakPointHandler pFunction, void* pdata);
            void Hook(BreakPointHandler pFunction, void* pdata);
            ~BreakPointHooker();
    };

    //! Temporarily disable breakpoints for the duration of this object's
    //! lifetime. Useful for negative testing of code which normally
    //! throws breakpoints. Can be used in conjunction with breakpoint
    //! hooker to ensure that breakpoints are triggered.
    class DisableBreakPoints
    {
        public:
            DisableBreakPoints();
            ~DisableBreakPoints();
        private:
            DisableBreakPoints(const DisableBreakPoints&);
            DisableBreakPoints& operator=(const DisableBreakPoints&);
    };

    //! Set/Get the maximum number of address bits available when calling
    //! AllocPages*
    RC SetMaxAllocPagesAddressBits(UINT32 bits);

    //! When ForceVgaPrint is set, allow prints to VGA regardless of whether user
    //! interface has been enabled
    bool GetForceVgaPrint();
    RC   SetForceVgaPrint(bool ToForce);

    //! When DumpPciAccess is set, addresses and values will be print out
    //! when the Pci is read from or written to.
    bool GetDumpPciAccess();
    RC   SetDumpPciAccess(bool dump);

    typedef bool (* PollFunc)(void *);
    bool PollfuncWrap(PollFunc pollFunc, void *pArgs, const char* pollFuncName);

    void PreGpuMonitorThread();

    RC GetForcedLwLinkConnections(UINT32 gpuInst, UINT32 numLinks, UINT32 *pLinkArray);
    RC GetForcedC2CConnections(UINT32 gpuInst, UINT32 numLinks, UINT32 *pLinkArray);

    RC SetLwLinkSysmemTrained(UINT32 domain, UINT32 bus, UINT32 device, UINT32 func, bool trained);

    bool IsVirtFunMode();
    bool IsPhysFunMode();
    bool IsDefaultMode();

    //!< Determine if running in Confidential Compute mode
    bool IsCCMode();
    
    //!< Determine if running in Hypervisor.
    bool IsInHypervisor();

    //Get reserved sysmem size which is specified by mdiag option "-reserve_sysmem"
    UINT32 GetReservedSysmemMB();
    //Get chip args which is specified by mdiag option "-chipargs"
    const vector<const char*>& GetChipLibArgV();
    // Validate -chipargs LWLink config with actual reported by RM
    int VerifyLWLinkConfig(size_t Size, const void* Buf, bool supersetOK);

    bool IsSelfHosted();
}

// Don't define these in mdiag, they collide with other mdiag stuff
#ifndef MDIAG_LIBRARY
// Macros that wrap all accesses to chip-visible memory, for use in simulator
// builds.  Please try to use these macros wherever possible, since accessing
// the memory directly will cause sim builds to crash!
#ifdef SIM_BUILD
#define MEM_RD08(a) Platform::VirtualRd08((const volatile void *)(a))
#define MEM_RD16(a) Platform::VirtualRd16((const volatile void *)(a))
#define MEM_RD32(a) Platform::VirtualRd32((const volatile void *)(a))
#define MEM_RD64(a) Platform::VirtualRd64((const volatile void *)(a))

#define MEM_WR08(a, d) Platform::VirtualWr08((volatile void *)(a), d)
#define MEM_WR16(a, d) Platform::VirtualWr16((volatile void *)(a), d)
#define MEM_WR32(a, d) Platform::VirtualWr32((volatile void *)(a), d)
#define MEM_WR64(a, d) Platform::VirtualWr64((volatile void *)(a), d)
#elif defined(INCLUDE_PEATRANS)
#define MEM_RD08(a) Peatrans::MemRd08((const volatile void *)(a))
#define MEM_RD16(a) Peatrans::MemRd16((const volatile void *)(a))
#define MEM_RD32(a) Peatrans::MemRd32((const volatile void *)(a))
#define MEM_RD64(a) Peatrans::MemRd64((const volatile void *)(a))

#define MEM_WR08(a, d) Peatrans::MemWr08((volatile void *)(a), d)
#define MEM_WR16(a, d) Peatrans::MemWr16((volatile void *)(a), d)
#define MEM_WR32(a, d) Peatrans::MemWr32((volatile void *)(a), d)
#define MEM_WR64(a, d) Peatrans::MemWr64((volatile void *)(a), d)
#else
#define MEM_RD08(a) (*(const volatile UINT08 *)(a))
#define MEM_RD16(a) (*(const volatile UINT16 *)(a))
#define MEM_RD32(a) (*(const volatile UINT32 *)(a))
#define MEM_RD64(a) (*(const volatile UINT64 *)(a))

#define MEM_WR08(a, d) do { *(volatile UINT08 *)(a) = (d); } while (0)
#define MEM_WR16(a, d) do { *(volatile UINT16 *)(a) = (d); } while (0)
#define MEM_WR32(a, d) do { *(volatile UINT32 *)(a) = (d); } while (0)
#define MEM_WR64(a, d) do { *(volatile UINT64 *)(a) = (d); } while (0)
#endif
#endif
