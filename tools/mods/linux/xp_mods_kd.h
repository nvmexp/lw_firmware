/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2014-2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#pragma once

#include "core/include/rc.h"
#include "core/include/types.h"
#include "core/include/memtypes.h"
#include "core/include/tasker.h"
#include "core/include/isrdata.h"
#include "core/include/platform.h"
#include <memory>
#include <map>
#include "mods_kd_api.h"

namespace Xp
{
    class ClockManager;

    class MODSKernelDriver
    {
        public:
            MODSKernelDriver(const MODSKernelDriver&) = delete;
            MODSKernelDriver& operator=(const MODSKernelDriver&) = delete;

            enum class AccessTokenMode
            {
                VERIFY
               ,ACQUIRE
               ,RELEASE
            };

            static constexpr UINT32 s_NoPCIDev = 0xFFFFU;

                           MODSKernelDriver() = default;
                           ~MODSKernelDriver();
                           /// Informs whether /dev/mods is open.
                           /// If /dev/mods is not open, /dev/mem can still be open on CheetAh.
            bool           IsOpen() const { return m_bOpen; }
                           /// Informs whether /dev/mem is open.
            bool           IsDevMemOpen() const { return m_bDevMemOpen; }
                           /// Opens the MODS kernel driver - /dev/mods.
                           /// On CheetAh, if /dev/mods is unavailable, it will open /dev/mem instead.
            RC             Open(const char* exePath);
                           /// Closes the connection to the driver.
            RC             Close();
                           /// Returns version of the MODS kernel driver (if open).
            UINT32         GetVersion() const { return m_Version; }
                           /// Returns kernel version, as reported by the driver.
            RC             GetKernelVersion(UINT64* pVersion);
                           /// Issues an ioctl to the MODS kernel driver.
            int            Ioctl(unsigned long fn, void* param) const;
            UINT32         GetAccessToken() const { return m_AccessToken; }
            void           SetAccessToken(UINT32 token) { m_AccessToken = token; }
            void           SetAccessTokenMode(AccessTokenMode m) { m_AccessTokenMode = m; }

                           /// Determines if the GPU is an SOC GPU.
            static bool    GpuIsSoc(UINT32 gpuInst);
                           /// Returns location of the GPU on the PCI bus.
            static RC      GetGpuPciLocation
                           (
                               UINT32  gpuInst,
                               UINT32* pDomainNumber,
                               UINT32* pBusNumber,
                               UINT32* pDeviceNumber,
                               UINT32* pFunctionNumber
                           );

            // Memory allocation and mapping
            // =============================

                           /// Maps a given physical address range and returns a CPU pointer.
                           /// @param physAddr Physical address in system memory to be mapped.
                           /// @param size     Size of the mapping (number of bytes).
                           /// @param protect  Memory access flags (readable/writeable).
                           /// @param attrib   Memory caching type.
            void*          Mmap(PHYSADDR physAddr, UINT64 size, Memory::Protect protect, Memory::Attrib attrib);
                           /// Unmaps a previously mapped virtual address.
            void           Munmap(void* virtAddr);
                           /// Colwerts Memory::Attrib to a driver-friendly memory caching type.
            static RC      GetMemoryType(Memory::Attrib attrib, UINT32* pType);
                           /// Allocates system memory.
                           /// @param size      Number of bytes requested.
                           /// @param contig    Whether the pages must be contiguous or not.
                           /// @param addrBits  Number of bits in the address
                           /// @param attrib    Memory caching type.
                           /// @param domain    Domain of PCI device for which to allocate memory, or 0xFFFFU.
                           /// @param bus       Bus of PCI device for which to allocate memory, or 0xFFFFU.
                           /// @param device    Device of PCI device for which to allocate memory, or 0xFFFFU.
                           /// @param function  Function of PCI device for which to allocate memory, or 0xFFFFU.
                           /// @param pHandle   Returned memory handle, use GetPhysicalAddress() and Mmap() to access on the CPU,
                           ///                   or GetMappedPhysicalAddress() to access on the GPU.
            RC             AllocPages
                           (
                               UINT64         size,
                               bool           contig,
                               UINT32         addrBits,
                               Memory::Attrib attrib,
                               UINT32         domain,
                               UINT32         bus,
                               UINT32         device,
                               UINT32         function,
                               UINT64*        pHandle
                           );
                           /// Releases previously allocated system memory.
            void           FreePages(UINT64 handle);
                           /// Merge two or more allocations into one.
                           /// @param pHandle Returned new handle.
                           /// @param pInHandles Array of 2 or more handles.  The handles are
                           ///                   from AllocPages().  The driver has a limit of
                           ///                   how many handles it supports.  On success these
                           ///                   input handles become invalid.
                           /// @param numInHandles Number of handles in pInHandles array.
            RC             MergePages(UINT64* pHandle, UINT64* pInHandles, UINT32 numInHandles);
                           /// Returns physical address of the given allocation at a given offset.
                           /// @param handle Memory handle returned by AllocPages().
                           /// @param offset Offset within the allocation.
            PHYSADDR       GetPhysicalAddress(UINT64 handle, size_t offset);
                           /// Returns pci_map_page() address of the given allocation at given offset
                           ///
                           /// If the D:B:D.F provided does not indicate a valid device then
                           /// the physical address is returned
                           ///
                           /// Otherwise if a mapping was not found 0 is returned
                           ///
                           /// @param domain       DomainNumber of PCI device
                           /// @param bus          BusNumber of PCI device
                           /// @param device       DeviceNumber of PCI device
                           /// @param func         FunctionNumber of PCI device
                           /// @param handle Memory handle returned by AllocPages().
                           /// @param offset Offset within the allocation.
            PHYSADDR       GetMappedPhysicalAddress(UINT32 domain, UINT32 bus, UINT32 device, UINT32 func, UINT64 handle, size_t offset);
                           /// DMA map the memory to the specified device (required to allow a
                           /// device to utilize an allocation on a system with an IOMMU)
                           ///
                           /// @param domain       DomainNumber of PCI device
                           /// @param bus          BusNumber of PCI device
                           /// @param device       DeviceNumber of PCI device
                           /// @param func         FunctionNumber of PCI device
                           /// @param handle Memory handle returned by AllocPages().
            RC             DmaMapMemory(UINT32 domain, UINT32 bus, UINT32 device, UINT32 func, UINT64 handle);
                           /// Unmap a DMA mapping the memory to the specified device and memory
                           ///
                           /// @param domain       DomainNumber of PCI device
                           /// @param bus          BusNumber of PCI device
                           /// @param device       DeviceNumber of PCI device
                           /// @param func         FunctionNumber of PCI device
                           /// @param handle Memory handle returned by AllocPages().
            RC             DmaUnmapMemory(UINT32 domain, UINT32 bus, UINT32 device, UINT32 func, UINT64 handle);
                           /// Tell the kernel how many bits of PCI addr space the device supports.
                           ///
                           /// @param domain       DomainNumber of PCI device
                           /// @param bus          BusNumber of PCI device
                           /// @param device       DeviceNumber of PCI device
                           /// @param func         FunctionNumber of PCI device
                           /// @param numBits      Number of bits supported by the device (size of
                           ///                     PCI address space).
            RC             SetDmaMask(UINT32 domain, UINT32 bus, UINT32 device, UINT32 func,
                                      UINT32 numBits);
                           /// Retrieves physical address for a given CPU pointer.
            PHYSADDR       VirtualToPhysical(volatile void* addr, Tee::Priority pri);
                           /// Retrieves CPU pointer to the given physical address.
            void*          PhysicalToVirtual(PHYSADDR physAddr, Tee::Priority pri);
                           /// Map dma memory by iommu
                           /// @param handle        Memory handle returned by AllocPages()
                           /// @param DevName       Smmu device name
                           /// @param pIova         Pointer to returned smmu mapped address
            RC             IommuDmaMapMemory(UINT64 handle, string DevName, PHYSADDR *pIova);
                           /// Unmap dma memory by iommu
                           /// @param handle        Memory handle returned by AllocPages()
                           /// @param DevName       Smmu device name
            RC             IommuDmaUnmapMemory(UINT64 handle, string DevName);
                           /// PPC specific functionality to setup dma bypass mode
                           /// @param domain       DomainNumber of PCI device
                           /// @param bus          BusNumber of PCI device
                           /// @param dev          DeviceNumber of PCI device
                           /// @param func         FunctionNumber of PCI device
                           /// @param bypassMode   Bypass mode to set
                           /// @param devDmaMask   Dma mask supported by the device
                           /// @param pDmaBaseAddr Pointer to returned DMA base address
            RC             SetupDmaBase(UINT16 domain, UINT08 bus, UINT08 dev, UINT08 func, UINT08 bypassMode, UINT64 devDmaMask, PHYSADDR *pDmaBaseAddr);
                           /// Indicate to the driver that the LwLink Sysmem links have been
                           /// trained and to change the WARs for memory allocation and mapping
                           /// @param domain       DomainNumber of PCI device
                           /// @param bus          BusNumber of PCI device
                           /// @param dev          DeviceNumber of PCI device
                           /// @param func         FunctionNumber of PCI device
                           /// @param trained      Whether the lwlink has been trained or not
            RC             SetLwLinkSysmemTrained(UINT16 domain, UINT08 bus, UINT08 dev, UINT08 func, bool trained);

                           /// Get the lwlink speed supported by NPU from device-tree
                           /// @param domain       DomainNumber of PCI device
                           /// @param bus          BusNumber of PCI device
                           /// @param dev          DeviceNumber of PCI device
                           /// @param func         FunctionNumber of PCI device
                           /// @param speed        Speed supported by NPU
            RC             GetLwLinkLineRate(UINT32 domain, UINT32 bus, UINT32 device, UINT32 func, UINT32 *pSpeed);

                           /// Return Domain/Bus/Device/Function of PCI device with deviceId and vendorId
                           /// @param deviceId The deviceId to search for
                           /// @param vendorId The vendorId to search for
                           /// @param index    Return the index'th PCI device with deviceId and vendorId
                           /// @param pDomainNumber   Returned DomainNumber of found PCI device
                           /// @param pBusNumber      Returned BusNumber of found PCI device
                           /// @param pDeviceNumber   Returned DeviceNumber of found PCI device
                           /// @param pFunctionNumber Returned FunctionNumber of found PCI device
            RC             FindPciDevice(INT32 deviceId, INT32 vendorId, INT32 index,
                                         UINT32* pDomainNumber, UINT32* pBusNumber,
                                         UINT32* pDeviceNumber, UINT32* pFunctionNumber);
                           /// Return Domain/Bus/Device/Function of PCI device with classCode
                           /// @param classCode The class code to search for
                           /// @param index     Return the index'th PCI device with classCode
                           /// @param pDomainNumber   Returned DomainNumber of found PCI device
                           /// @param pBusNumber      Returned BusNumber of found PCI device
                           /// @param pDeviceNumber   Returned DeviceNumber of found PCI device
                           /// @param pFunctionNumber Returned FunctionNumber of found PCI device
            RC             FindPciClassCode(INT32 classCode, INT32 index,
                                            UINT32* pDomainNumber, UINT32* pBusNumber,
                                            UINT32* pDeviceNumber, UINT32* pFunctionNumber);
                           /// Return base address and size of BarIndex on specified PCI device
                           /// @param DomainNumber   DomainNumber of PCI device
                           /// @param BusNumber      BusNumber of PCI device
                           /// @param DeviceNumber   DeviceNumber of PCI device
                           /// @param FunctionNumber FunctionNumber of PCI device
                           /// @param BarIndex       Index of bar to get info for
                           /// @param pBaseAddress   Returned base address of the BAR
                           /// @param pBarSize       Returned size of the BAR
            RC             PciGetBarInfo(INT32 DomainNumber, INT32 BusNumber,
                                         INT32 DeviceNumber, INT32 functionNumber,
                                         INT32 BarIndex, UINT64* pBaseAddress, UINT64* pBarSize);
                           /// Enable or disable SR-IOV on a PCIe device
                           ///
                           /// @param domain,bus,device,function Identifies the
                           ///                       physical PCIe device
                           /// @param vfCount        Number of VF (virtual
                           ///                       functions) to enable,
                           ///                       or 0 to disable SR-IOV
            RC             PciEnableSriov(UINT32 domain, UINT32 bus,
                                          UINT32 device, UINT32 function,
                                          UINT32 vfCount);
                           /// Return the IRQ number of the specified PCI device
                           /// @param DomainNumber   DomainNumber of PCI device
                           /// @param BusNumber      BusNumber of PCI device
                           /// @param DeviceNumber   DeviceNumber of PCI device
                           /// @param functionNumber FunctionNumber of PCI device
                           /// @param pIrq           Returned irq number
            RC             PciGetIRQ(INT32 DomainNumber, INT32 BusNumber, INT32 DeviceNumber,
                                     INT32 functionNumber, UINT32* pIrq);
                           /// Read from address on specified PCI device
                           /// @param DomainNumber   DomainNumber of PCI device
                           /// @param BusNumber      BusNumber of PCI device
                           /// @param DeviceNumber   DeviceNumber of PCI device
                           /// @param functionNumber FunctionNumber of PCI device
                           /// @param Address        The address to read from
                           /// @param pData          Returned read data
                           template<typename UINTXX>
            RC             PciRead(INT32 DomainNumber, INT32 BusNumber, INT32 DeviceNumber,
                                   INT32 functionNumber, INT32 address, UINTXX* pData);
                           /// Write data to address on specified PCI device
                           /// @param domainNumber   DomainNumber of PCI device
                           /// @param busNumber      BusNumber of PCI device
                           /// @param deviceNumber   DeviceNumber of PCI device
                           /// @param functionNumber FunctionNumber of PCI device
                           /// @param address        The address to read from
                           /// @param pData          Returned read data
                           template<typename UINTXX>
            RC             PciWrite(INT32 domainNumber, INT32 busNumber, INT32 deviceNumber,
                                    INT32 functionNumber, INT32 address, UINTXX data);
            RC             PciRemove(INT32 domainNumber, INT32 busNumber, INT32 deviceNumber,
                                    INT32 functionNumber);
            RC             PciResetFunction(INT32 domainNumber, INT32 busNumber,
                                            INT32 deviceNumber, INT32 functionNumber);
                           /// EvalDevAcpiMethod
            RC             EvalDevAcpiMethod(UINT32 domainNumber, UINT32 busNumber,
                                             UINT32 deviceNumber, UINT32 functionNumber,
                                             UINT32 acpiId, MODS_EVAL_ACPI_METHOD* method);
                           /// EvalDevAcpiMethod
            RC             EvalDevAcpiMethod(UINT32 gpuInst, UINT32 acpiId,
                                             MODS_EVAL_ACPI_METHOD* method);
                           /// AcpiGetDDC
            RC             AcpiGetDDC(UINT32 domainNumber, UINT32 busNumber, UINT32 deviceNumber,
                                      UINT32 functionNumber, UINT32* pOutDataSize,
                                      UINT08* pOutBuffer);
                           /// AcpiGetDDC
            RC             AcpiGetDDC(UINT32 gpuInst, UINT32* pOutDataSize,
                                      UINT08* pOutBuffer);
                           /// AcpiGetDevChildren
            RC             AcpiGetDevChildren(UINT32 domainNumber, UINT32 busNumber,
                                              UINT32 deviceNumber, UINT32 functionNumber,
                                              UINT32* pOutDataSize, UINT32* pOutBuffer);
                           /// Get the CPU mask of CPUs that are local to the specified PCI device
                           ///        for NUMA enabled systems
                           /// @param domain   PCI domain number of the device.
                           /// @param bus      PCI bus number of the device.
                           /// @param device   PCI device number of the device
                           /// @param function PCI function number of the device
                           /// @param pLocalCpuMasks If running on a correctly configured NUMA system
                           ///                       this will be an array of masks indicating which CPUs
                           ///                       are on the same NUMA node as the specified device.
                           ///                       Index 0 in the masks vector contains the LSBs of all
                           ///                       the masks.  To compute CPU number from mask use
                           ///                           CPU = (index * 32) + bit
                           ///                       So bit 27 in index 3 would correspond to
                           ///                           CPU = (3 * 32) + 27 = 123
                           ///                       If the vector is empty then MODS was not able to
                           ///                       determine which CPUs are local to the device
            RC             DeviceNumaInfo(INT32 domain, INT32 bus, INT32 device, INT32 function,
                                          vector<UINT32> *pLocalCpuMasks);
                           /// Returns NUMA node number where the PCI device is located.
                           ///
                           /// Returns -1 if NUMA node cannot be identified or if NUMA is disabled or not supported.
            INT32          GetNumaNode(INT32 domain, INT32 bus, INT32 device, INT32 function);
                           /// Retrieve ATS-specific information (IBM P9)
            RC             GetAtsTargetAddressRange(UINT32 gpuInst, UINT32 npuInst,
                                                    UINT64* pPhysAddr, UINT64* pGuestAddr,
                                                    UINT64* pApertureSize, INT32* numaMemNode);
                           /// Returns CheetAh clock manager.
            ClockManager*  GetClockManager();

            // MSR (Model-Specific Registers) related functions
            // To access some Intel CPU registers

                           //! Read a MSR register on a given gpu
                           // @param cpu       Cpu index
                           // @param reg       Address of the register to be read
                           // @param pHigh     Upper 32 bits of read data to be returned in
                           // @param pLow      Lower 32 bits of read data to be returned in
            RC             RdMsrOnCpu(UINT32 cpu, UINT32 reg, UINT32 *pHigh, UINT32 *pLow);
                           //! Write a MSR register
                           // @param cpu       Cpu index
                           // @param reg       Address of the register to be written to
                           // @param high      Upper 32 bits of data to write
                           // @param low       Lower 32 bits of data to write
            RC             WrMsrOnCpu(UINT32 cpu, UINT32 reg, UINT32 high, UINT32 low);

            // Interrupt management
            // ====================
                           //! Allocates and maps an IRQ. Maps the physical Irq to a logical
                           //! irq number, enables it, and passes it back to the caller
                           //! so that the caller can use the logical irq number to hook
                           //! the mods isr
                           // @param logicalIrq Store the number of logical irq that is allocated
                           // @param physIrq    Physical IRQ index.
                           // @param DtName     DT name corresponding to the device
                           // @param FullName   Full name of device as define in device tree
            RC             MapIRQ(UINT32 * logicalIrq, UINT16 physIrq, string dtName, string fullName);
                           /// Hooks an interrupt for the given device and specifies how to mask out
                           /// the interrupts.
                           ///@param IrqInfo   Structure containing all the required masking info.
                           ///                 see irqinfo.h
                           /// @param func        Interrupt handler function.
                           /// @param cookie      Value to be passed to the handler when an interrupt oclwrs.
            RC             HookIsr(const IrqParams& irqInfo, ISR func, void* cookie);

                           /// Unhooks a previously hooked interrupt.
                           /// See HookIsr() for parameters.
            RC             UnHookIsr(UINT32 pciDomain, UINT32 pciBus, UINT32 pciDevice, UINT32 pciFunction, UINT32 type, ISR func, void* cookie);
                           /// Enables interrupts. DOS legacy stuff.
            void           EnableInterrupts();
                           /// Disables interrupts. DOS legacy stuff. Just prevents any handler from being called.
            void           DisableInterrupts();

                           /// Returns current interrupt level (enabled/disabled).
            Platform::Irql GetLwrrentIrql() const;
                           /// Disables interrupts. DOS legacy stuff. Just prevents any handler from being called.
            Platform::Irql RaiseIrql(Platform::Irql newIrql);
                           /// Enables interrupts. DOS legacy stuff.
            void           LowerIrql(Platform::Irql newIrql);
                           /// Update /sys/ nodes with provided data
            RC             WriteSysfsNode(string filePath, string fileContents);
                           /// Write a value into a /proc/sys/ node.
            RC             SysctlWrite(const char* path, INT64 value);
                           /// Set specific pcie controller's state thru BPMP
            RC             SetPcieStateByBpmp(INT32 controller, INT32 enable);
                           /// Initialize specific pcie ep's PLL thru BPMP
            RC             InitPcieEpPllByBpmp(INT32 epId);

        private:
            RC AllocPagesOld
            (
                UINT64         size,
                bool           contig,
                UINT32         addrBits,
                Memory::Attrib attrib,
                UINT32         domain,
                UINT32         bus,
                UINT32         device,
                UINT32         function,
                UINT64*        pHandle
            );

            static void IsrThreadFunc(void*);
            void        IsrThread();
            void        HandleIrq();
            RC          CreateIrqHookPipe();

            typedef vector<IsrData> IrqHooks;
            struct IrqDesc
            {
                IrqHooks hooks;
                UINT32   type;
            };
            typedef map<UINT64, IrqDesc> Isrs;

            struct IrqStats
            {
                UINT32 minIrqDelay;
                UINT32 maxIrqDelay;
                UINT64 totalIrqDelay;
                UINT32 numIrqs;
            };

            typedef map<UINT64, pair<void*, UINT64>> AddressMap;
            AddressMap             m_AddressMap;
            Tasker::Mutex          m_AddressMapMutex      = Tasker::NoMutex();

            bool                   m_bOpen                = false;
            bool                   m_bDevMemOpen          = false;

            int                    m_KrnFd                = -1;
            int                    m_DevMemFd             = -1;

            UINT32                 m_Version              = 0;
            bool                   m_bSetMemTypeSupported = true;

            unique_ptr<ClockManager> m_pClockManager;

            // IRQ-related members

            // This Unix pipe signals the IsrThread() to keep servicing
            // interrupts while the write-end of the pipe is open.
            // HookIsr() creates the pipe and the main thread holds onto the
            // write-end of the pipe. IsrThread() listens on the read-end.
            // When UnHookIsr() closes the write-end, IsrThread() will get a
            // POLLHUP signal and stop processing all IRQs immediately because
            // no ISRs can service them anymore.
            volatile UINT32        m_IrqHookPipe[2]     = { ~0U, ~0U };
            Tasker::ThreadID       m_IrqThread          = -1;
            Isrs                   m_Isrs;
            Tasker::Mutex          m_InterruptMutex     = Tasker::NoMutex();
            Tasker::Mutex          m_IrqRegisterMutex   = Tasker::NoMutex();
            IrqStats               m_IrqStats;
            volatile UINT32        m_LwrrentIrql        = Platform::NormalIrql;
            UINT32                 m_IrqlNestCount      = 0;
            UINT32                 m_AccessToken        = MODS_ACCESS_TOKEN_NONE;
            AccessTokenMode        m_AccessTokenMode    = AccessTokenMode::VERIFY;

            // Version check members
            bool                   m_MsrSupport         = false;
            bool                   m_NoPciDevFFFF       = false;
            bool                   m_AcpiIdApiSupport   = false;
            bool                   m_BetterAlloc        = false;
            bool                   m_MergePages         = false;
            bool                   m_ProcSysWrite       = false;
            bool                   m_AcpiDevChildren    = false;
    };

    MODSKernelDriver *GetMODSKernelDriver();
}
