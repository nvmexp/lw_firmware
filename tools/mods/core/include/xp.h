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

/**
 * @file   xp.h
 * @brief  Cross-platform (XP) interface.
 *
 * Declares the Xp namespace, which contains many platform-specific
 * functions and enums.
 *
 */

#pragma once

#ifndef INCLUDED_XP_H
#define INCLUDED_XP_H

#include "core/include/irqparams.h"
#include "core/include/memtypes.h"
#include "core/include/rc.h"
#include "core/include/types.h"
#include "core/include/utility.h"
#include "core/include/modsdrv_defines.h"
#include <cstdarg>
#include <cstdio>
#include <map>
#include <memory>
#include <string>
#include <vector>

class SocketMods;
class GpuSubdevice;

//! Namespace for platform-specific functions.
namespace Xp
{
   enum OperatingSystem
   {
       OS_NONE      = MODS_OS_NONE
      ,OS_DOS       = MODS_OS_DOS
      ,OS_WINDOWS   = MODS_OS_WINDOWS
      ,OS_LINUX     = MODS_OS_LINUX
      ,OS_LINUXRM   = MODS_OS_LINUXRM
      ,OS_MAC       = MODS_OS_MAC
      ,OS_MACRM     = MODS_OS_MACRM
      ,OS_WINSIM    = MODS_OS_WINSIM
      ,OS_LINUXSIM  = MODS_OS_LINUXSIM
      ,OS_WINMFG    = MODS_OS_WINMFG
      ,OS_UEFI      = MODS_OS_UEFI
      ,OS_ANDROID   = MODS_OS_ANDROID
      ,OS_WINDA     = MODS_OS_WINDA
      ,OS_LINDA     = MODS_OS_LINDA
   };

   const INT32 PCI_MAX_DOMAINS = 65536;
   const INT32 PCI_MAX_BUSSES = 256;
   const INT32 PCI_MAX_DEVICES = 32;
   const INT32 PCI_MAX_FUNCTIONS = 8;

   enum ExelwtableType
   {
       EXE_TYPE_UNKNOWN
      ,EXE_TYPE_32
      ,EXE_TYPE_64
   };

   OperatingSystem GetOperatingSystem();
   RC GetOSVersion(UINT64*);

   //! Returns the number of CPU cores on this system.
   //! Note: In sim MODS this applies to the host CPU where MODS is running, not DUT.
   UINT32 GetNumCpuCores();

   //! Platform-specific initialization and cleanup code.
   //! OnExit returns the exit code we return to the calling shell.
   RC OnEntry(vector<string> *pArgs);
   RC OnExit(RC rc);
   RC PreOnExit();
   RC OnQuickExit(RC rc);
      //!< Perform minimum cleanup to allow for fastest exit, leaks are allowed
   RC PauseKernelLogger(bool pause);

   enum Feature
   {
       //! Indicates whether full HW access is possible.
       //!
       //! Full/advanced HW access includes mapping arbitrary PA ranges,
       //! hooking interrupts, etc.
       //!
       //! On Linux, this indicates the presence of /dev/mods.
       HAS_KERNEL_LEVEL_ACCESS

       //! Has Unified Virtual Address support (for LWCA P2P applications)
      ,HAS_UVA_SUPPORT
   };

   //! Indicates whether a particular XP feature is available.
   bool HasFeature(Feature feat);

   //! Platform-specific exit frunction that can be used during global
   //! destruction
   void ExitFromGlobalObjectDestructor(INT32 exitValue);

   //! Dynamically load libraries and get functions from them
   enum LoadDLLFlags
   {
       NoDLLFlags       = 0
      ,UnloadDLLOnExit  = (1 << 0)
      ,GlobalDLL        = (1 << 1)
      ,DeepBindDLL      = (1 << 2)
      ,LoadLazy         = (1 << 3)
      ,LoadNoDelete     = (1 << 4)
   };

   RC LoadDLL(const string& fileName,
              void**        moduleHandle,
              UINT32        loadDLLFlags); //!< enum LoadDLLFlags
   RC UnloadDLL(void * ModuleHandle);
   void * GetDLLProc(void * ModuleHandle, const char * FuncName);
   string GetDLLSuffix();
   ExelwtableType GetExelwtableType(const char * FileName);

   //! \brief Disable the user interface.
   //!
   //! DisableUserInterface should take care of a few things:
   //!   - Stop the system display driver (if any) from sending methods to
   //!     LWPU GPUs.
   //!   - Disable the VGA BIOS from touching HW, and enable the resman.
   //!   - Hide the cursor.
   //!   - Ensure that framebuffer memory is available for
   //!     LwRm::VidHeap* (for instance, under Windows, GDI
   //!     claims the memory for itself, and we have to use
   //!     DirectDraw to get GDI to release it).
   RC DisableUserInterface();

   //! \brief Enable the user interface.
   //!
   //! This function should undo the effects of DisableUserInterface and restore
   //! the system to its previous state.
   RC EnableUserInterface();

   bool IsRemoteConsole();
      //!< Determine if running with remote console support.

   bool IsInHypervisor();
      //!< Determine if running in Hypervisor.

   char GetElwPathDelimiter();
       //!< Get the path delimiter for extracting paths from elw variables

   SocketMods * GetRemoteConsole();
      //!< Get the Socket used for the remote interface.

   SocketMods * GetSocket();
      //!< Get a new Socket.

   void FreeSocket(SocketMods * sock);
      //!< Delete a Socket.

   void *AllocOsEvent(UINT32 hClient, UINT32 hDevice);
      //!< Allocate a new OS Event

   void FreeOsEvent(void *pOsEvent, UINT32 hClient, UINT32 hDevice);
      //!< Free an OS Event

   void SetOsEvent(void* pOsEvent);
      //!< Trigger OS event

   RC WaitOsEvents
   (
       void**  pOsEvents,
       UINT32  numOsEvents,
       UINT32* pCompletedIndices,
       UINT32  maxCompleted,
       UINT32* pNumCompleted,
       FLOAT64 timeoutMs
   );
      //!< Wait for OS Event (blocking call)

   RC SetVesaMode(UINT32 Mode, bool Windowed = true, bool ClearMemory = true);
      //!< Set display to a specified VESA 'Mode'.
      //!< If 'Windowed is true, use a windowed frame buffer model,
      //!< otherwise linear/flat model.
      //!< If 'ClearMemory' is true, clear the display memory.
      //!< If an invalid mode is requested, the mode will not change,
      //!< and an error code will be returned.

   void BreakPoint();
      //!< Issue a break point.

   void DebugPrint(const char* str, size_t strLen);
      //!< Send a string to the attached debugger.

   INT32 VsnPrintf(char * str, size_t size, const char * format, va_list args);
      //!< Printf data to a string with maximum number of characters size.
      //!< If successful, return the number of characters that would be written
      //!< to str had n been sufficiently large (null terminating byte is *not*
      //!< included in the count).
      //!<
      //!< If an error oclwrs, a negative value is returned
      //!<
      //!< Note : This is *not* destructive to the va_list (unlike the default
      //!< library version)

   size_t GetPageSize();
      //!< Return the page size in bytes for this platform.

   //! Class used to run another process in parallel with mods
   class Process
   {
   public:
       Process()                           {}
       Process(const Process&)             = delete;
       Process& operator=(const Process&)  = delete;
       virtual ~Process()                  {}
       //! Set an environment variable in the subprocess, which
       //! otherwise inherits its variables from mods.  Must be called
       //! before Run().
       void SetElw(const string& var, const string& val) { m_Elw[var] = val; }
       //! Run the process with the indicated command line.
       virtual RC   Run(const string& cmd) = 0;
       //! Test whether the process is still running.
       virtual bool IsRunning()            = 0;
       //! Wait for process to finish, and get the exit status.
       virtual RC   Wait(INT32* pStatus)   = 0;
       //! Forcibly kill the process.
       virtual RC   Kill()                 = 0;
   protected:
       map<string, string> m_Elw;
   };
   unique_ptr<Process> AllocProcess();

   UINT32 GetPid();
      //!< Return the process id

   bool IsPidRunning(UINT32 pid);
      //!< Return true if the indicated process is running.
      //!< Note: May be unreliable because PID can be reused by another process

   string GetUsername();
      //!< Return the username

   void DumpPageTable();
      //!< Dump the lwrrently mapped pages

    //! Check write permission for the defined file
    RC CheckWritePerm(const char *FileName);

    //! Allocate system memory.
    //! A whole number of pages will be mapped (numBytes will be rounded up).
    //! All pages will be locked and will be below physical address 2**AddressBits.
    //! If contiguous is true, physical pages will be adjacent and in-order.
    RC AllocPages
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
    );

    //! Alloc system memory.
    //! A whole number of pages will be mapped (NumBytes will be rounded up).
    //! All pages will be locked and will be below physical address 2**AddressBits.
    //! If Contiguous is true, physical pages will be adjacent and in-order.
    RC AllocPages
    (
        size_t         NumBytes,
        void **        pDescriptor,
        bool           Contiguous,
        UINT32         AddressBits,
        Memory::Attrib Attrib,
        UINT32         GpuInst
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

    //! Determine whether we need to call DmaMapMemory()
    bool CanEnableIova
    (
        UINT32 domain,
        UINT32 bus,
        UINT32 device,
        UINT32 func
    );
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

    //! Returns carveout physical base and size on SOC
    RC GetCarveout(PHYSADDR* pPhysAddr, UINT64* pSize);

    //! Returns the kernel driver's file descriptor
    //! This is required fon non-XP methods which have to make ioctl calls.
    int GetDriverHandle();

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

    //! Allocate and free virtual address space with no underlying memory
    RC AllocAddressSpace
    (
        size_t  numBytes,
        void ** ppReturnedVirtualAddress,
        void *  pBase,
        UINT32  protect,
        UINT32  vaFlags
    );
    RC FreeAddressSpace
    (
        void *                    pVirtualAddress,
        size_t                    numBytes,
        Memory::VirtualFreeMethod vfMethod
    );

    void *VirtualFindFreeVaInRange(size_t size, void *pBase, void *pLimit, size_t align);
    RC    VirtualProtect(void *pAddr, size_t size, UINT32 protect);
    RC    VirtualMadvise(void *pAddr, size_t size, Memory::VirtualAdvice advice);

    //! Remap pages in a reserved virtual address range to a sequence of
    //! contiguous physical pages
    RC RemapPages
    (
        void *          VirtualAddress,
        PHYSADDR        PhysicalAddress,
        size_t          NumBytes,
        Memory::Attrib  Attrib,
        Memory::Protect Protect
    );

    //! Allocate and free memory with execute permissions
    void * ExecMalloc(size_t NumBytes);
    void ExecFree(void *Address, size_t NumBytes);

    //! Colwert physical address to virtual address.
    void * PhysicalToVirtual(PHYSADDR physicalAddress, Tee::Priority pri);
    inline void * PhysicalToVirtual(PHYSADDR physicalAddress)
    {
        return PhysicalToVirtual(physicalAddress, Tee::PriError);
    }

    //! Colwert virtual address to physical address.
    PHYSADDR VirtualToPhysical(volatile void * virtualAddress, Tee::Priority pri);
    inline PHYSADDR VirtualToPhysical(volatile void * virtualAddress)
    {
        return VirtualToPhysical(virtualAddress, Tee::PriError);
    }

    //! Maps the dma memory by iommu
    //! @param pDescriptor  Memory handle returned by AllocPages()
    //! @param DevName      Mods smmu device name in dtb
    //! @param pIova        Pointer to returned smmu mapped address
    RC IommuDmaMapMemory(void *pDescriptor, string DevName, PHYSADDR *pIova);
    //! Unmaps the dma memory by iommu
    //! @param pDescriptor  Memory handle returned by AllocPages()
    //! @param DevName      Mods smmu device name in dtb
    RC IommuDmaUnmapMemory(void *pDescriptor, string DevName);

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
        //!< PPC specific functionality to setup bypass mode

    //! PPC-specific function to retrieve ATS address range
    RC GetAtsTargetAddressRange
    (
        UINT32  gpuInst,
        UINT32* pAddr,
        UINT32* pGuestAddr,
        UINT32* pAddrWidth,
        UINT32* pMask,
        UINT32* pMaskWidth,
        UINT32* pGranularity,
        bool    bIsPeer,
        UINT32  peerIndex
    );

    //! PPC-specific function to inform the driver once the lwlink has been trained
    RC SetLwLinkSysmemTrained(UINT32 domain, UINT32 bus, UINT32 device, UINT32 func, bool trained);

    //! PPC-specific function to get the lwlink speed supported by NPU from device-tree
    RC GetLwLinkLineRate(UINT32 domain, UINT32 bus, UINT32 device, UINT32 func, UINT32 *pSpeed);

    RC CallACPIGeneric(UINT32 DevInst,  // gpuinst
                       UINT32 Function,
                       void *Param1,
                       void *Param2,
                       void *Param3);

    RC LWIFMethod
    (
        UINT32 Function,
        UINT32 SubFunction,
        void *InParams,
        UINT16 InParamSize,
        UINT32 *OutStatus,
        void *OutData,
        UINT16 *OutDataSize
    );

    class OptimusMgr
    {
    public:
        virtual ~OptimusMgr() { }
        virtual bool IsDynamicPowerControlSupported() const = 0;
        virtual RC EnableDynamicPowerControl() = 0;
        virtual RC DisableDynamicPowerControl() = 0;
        virtual RC PowerOff() = 0;
        virtual RC PowerOn() = 0;

    protected:
        OptimusMgr() { }

    private:
        // Non-copyable
        OptimusMgr(const OptimusMgr&);
        OptimusMgr& operator=(const OptimusMgr&);
    };

    OptimusMgr* GetOptimusMgr(UINT32 domain, UINT32 bus, UINT32 device, UINT32 function);
/* refer to the following link for more information:
https://wiki.lwpu.com/engwiki/index.php/Software_Power/Jefferson_Technology
*/
    enum class PowerState
    {
        PowerOff,
        PowerOn
    };

    class JTMgr
    {
    public:
        struct CycleStats
        {
            UINT64 entryTimeMs = 0;
            INT32  entryStatus = 0;
            UINT16 entryPwrStatus = 0;
            UINT64 enterLinkDnMs = 0;   // only used with Ver 0.9 and higher FW

            UINT64 exitTimeMs = 0;
            UINT64 exitLinkUpMs = 0;
            INT32  exitStatus = 0;
            UINT16 exitPwrStatus = 0;
            UINT32 exitWakeupEvent = 0;
            INT32  exitWakeupEventStatus = 0;
            INT32  exitGpuWakeStatus = 0;
        };
        struct DebugOpts
        {
            // how long to delay after issuing the SMBus command to enter GC6 before
            // reading the power status.
            UINT32 enterCmdToStatDelayMs;
            // how long to delay after issuing the SMBus command to exit GC6 before
            // reading the power status.
            UINT32 exitCmdToStatDelayMs;
            // how long to delay after getting good power status before returning
            // to RM.
            UINT32 delayAfterLinkStatusMs;
            // How long to delay between L2 entry and PEX_RST assert for RTD3
            UINT32 delayAfterL2EntryMs;
            // How long to delay after L2 exit of RTD3
            UINT32 delayAfterL2ExitMs;
            // how long to poll for the PEX link status to change
            UINT32 pollLinkStatusMs;
            // Print priority to use for verbose output
            Tee::Priority verbosePrint;
        };
        virtual ~JTMgr() {}
        virtual UINT32 GetCapabilities() = 0;
        virtual RC GC6PowerControl(UINT32 *pInOut) = 0;
        virtual void GetCycleStats(CycleStats * pStats) = 0;
        virtual RC GetDebugOptions(DebugOpts* pOpts) = 0;
        virtual void SetDebugOptions(DebugOpts* pOpts) = 0;
        virtual RC SetWakeupEvent(UINT32 eventId) = 0;
        virtual RC AssertHpd(bool bAssert)= 0;
        virtual RC GetLwrrentFsmMode(UINT08 *pFsmMode) = 0;
        virtual RC SetLwrrentFsmMode(UINT08 fsmMode) = 0;
        virtual bool IsJTACPISupported() = 0;
        virtual bool IsNativeACPISupported() = 0;
        virtual bool IsSMBusSupported() = 0;
        virtual RC RTD3PowerCycle(Xp::PowerState state) = 0;

        // Generic ACPI Device State Method Evaluator
        virtual RC DSM_Eval
        (
            UINT16 domain,
            UINT16 bus,
            UINT08 dev,
            UINT08 func,
            UINT32 acpiFunc,
            UINT32 acpiSubFunc,
            UINT32 *pInOut,
            UINT16 *pSize
        ) = 0;

    protected:
        CycleStats  m_ACPIStats;
        JTMgr() {}
    private:
        // Non-copyable
        JTMgr(const JTMgr&);
        JTMgr& operator=(const JTMgr*);
    };

    JTMgr* GetJTMgr(UINT32 GpuInstance);
    void CleanupJTMgr();

    enum CacheFlushFlags
    {
        FLUSH_CPU_CACHE                 = 1,
        ILWALIDATE_CPU_CACHE            = 2,
        FLUSH_AND_ILWALIDATE_CPU_CACHE  = FLUSH_CPU_CACHE | ILWALIDATE_CPU_CACHE
    };

#if defined(LWCPU_FAMILY_ARM)
    RC FlushWriteCombineBuffer();
        //!< Flush WC buffer.

    RC FlushCpuCacheRange(void * StartAddress, void * EndAddress, CacheFlushFlags Flags);
        //!< Flush User cache, takes a range
#endif

    void FlushFstream(FILE * pFile);
        //!< Force recent changes to an open file to be written to disk immediately.

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

    //! Process any pending interrupts
    void ProcessPendingInterrupts();

   //! Hooks an ISR to an IRQ.
   /**
    * @param Irq    IRQ to hook the ISR to.
    * @param func   ISR to hook.
    * @param cookie data to pass to ISR
    *
    * @return RC
    *
    * @sa UnhookIRQ
    */
   RC HookIRQ(UINT32 Irq, ISR func, void* cookie);

   //! Hook an ISR to a PCIe interrupt and specify how to mask the interrupt.
   /**
    * @param IrqParams   Structure containing all the required masking info.
    *    .irqNumber        PCI IRQ number to hook
    *    .barAddr          Physical address of the Base Address Register region
    *    .barSzie          Number of bytes to map within the Bar
    *    .pciDev           PCI's domain/bus/device/function values
    *    .irqType          Type of IRQ to hook, s/b one of the following:
    *                      - MODS_XP_IRQ_TYPE_INT
    *                      - MODS_XP_IRQ_TYPE_MSI
    *                      - MODS_XP_IRQ_TYPE_CPU
    *    .maskInfoCount    Number of entries in the maskInfoList
    *    .maskInfoList[]   Array of register masking definitions (see irqinfo.h)
    * @param func        ISR to hook.
    * @param cookie      data to pass to ISR
    *
    * @return RC
    *
    * @sa UnhookInt
    */
   RC HookInt(const IrqParams& irqInfo, ISR func, void* cookie);

   enum WhichInterrupts
   {
       NoIntrCheck      = 0
       ,IntaIntrCheck   = 1
       ,MsiIntrCheck    = 2
       ,MsixIntrCheck   = 4
   };
   RC ValidateGpuInterrupt( GpuSubdevice  *pGpuSubdevice,
                            UINT32 whichInterrupts);
   //!< Confirm that the GPU's interrupt mechanism is working

   enum IsrResult
   {
       ISR_RESULT_NOT_SERVICED = 0,
       ISR_RESULT_SERVICED     = 1,
       ISR_RESULT_TIMEOUT      = 2
   };

   //! Allocates and maps an IRQ. Maps the physical Irq to a logical
   //! irq number, enables it, and passes it back to the caller
   //! so that the caller can use the logical irq number to hook
   //! the mods isr
   /**
   * @param logicalIrq Store the number of logical irq that is allocated
   * @param physIrq    Physical IRQ index.
   * @param dtName     DT name corresponding to the device
   * @param fullName   Full name of device as define in device tree
   *
   * @return RC
   *
   *
   */
   RC MapIRQ(UINT32 * logicalIrq, UINT32 physIrq, string dtName, string fullName);

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

   //! Unhooks an ISR from a PCIe interrupt.
   /**
    * @param irqInfo     Same data that was passed to HookInt()
    * @param func        ISR to unhook.
    * @param cookie      data to pass to ISR
    *
    * @return RC
    *
    * @sa HookInt
    */
   RC UnhookInt(const IrqParams& irqInfo, ISR func, void* cookie);

    //! Enable external hardware interrupts.
    //! Deprecated in favor of LowerIrql
    void EnableInterrupts();

    //! Disable external hardware interrupts.
    //! Deprecated in favor of RaiseIrql
    void DisableInterrupts();

    //! Returns the current IRQL
    Platform::Irql GetLwrrentIrql();

    //! Increases the IRQL and returns the previous IRQL.  Asserts that you are
    //! actually raising the IRQL and not lowering it.
    Platform::Irql RaiseIrql(Platform::Irql NewIrql);

    //! Decreases the IRQL.  Asserts that you are actually lowering the IRQL and
    //! not raising it.
    void LowerIrql(Platform::Irql NewIrql);

   RC FindPciDevice(INT32 DeviceId, INT32 VendorId, INT32 Index,
            UINT32 * pDomainNumber = 0,
            UINT32 * pBusNumber = 0, UINT32 * pDeviceNumber = 0,
            UINT32 * pFunctionNumber = 0);
      //!< Find a PCI device with given 'DeviceId', 'VendorId', and 'Index'.
      //!< Return '*pDomainNumber', '*pBusNumber', '*pDeviceNumber', and '*pFunctionNumber'.

   RC FindPciClassCode(INT32 ClassCode, INT32 Index,
            UINT32 * pDomainNumber = 0,
            UINT32 * pBusNumber = 0, UINT32 * pDeviceNumber = 0,
            UINT32 * pFunctionNumber = 0);
      //!< Find a PCI class code given 'ClassCode' and 'Index'.
      //!< Return '*pDomainNumber', '*pBusNumber', '*pDeviceNumber', and '*pFunctionNumber'.

    //! Force a rescan of PCI bus.
    RC RescanPciBus(INT32 domain, INT32 bus);

    //! Initialize the watchdogs to turn off MODS if test takes too long
    //! or hangs
    RC InitWatchdogs(UINT32 TimeoutSecs);

    //! Reset the watchdogs
    RC ResetWatchdogs();

    RC PciGetBarInfo(INT32 DomainNumber, INT32 BusNumber, INT32 DeviceNumber, INT32 FunctionNumber,
                     INT32 BarIndex, UINT64* pBaseAddress, UINT64* pBarSize);
      //!< Get PCI BAR info given 'BusNumber', 'DeviceNumber', 'FuncitonNumber', and 'BarIndex'
      //!< Return '*pBaseAddress' and '*pBarSize'

    //! Enable or disable SR-IOV
    //!
    //! \param domain,bus,device,function Identifies the physical PCIe device
    //! \param vfCount Number of VF (virtual functions) to enable, or 0
    //!     to disable SR-IOV
    RC PciEnableSriov(UINT32 domain, UINT32 bus, UINT32 device,
                      UINT32 function, UINT32 vfCount);

    RC PciGetIRQ(INT32 DomainNumber, INT32 BusNumber, INT32 DeviceNumber, INT32 FunctionNumber,
                 UINT32 * pIrq);
      //!< Get PCI IRQ given 'BusNumber', 'DeviceNumber', and 'FunctionNumber'
      //!< Return '*pIrq'

   /**
    * @name PCI config read functions.
    */
   //@{

   /**
    * Read byte, word, or dword at @a Address from a PCI device with the given
    * @a BusNumber, @a DeviceNumber, and @a FunctionNumber'.
    * Return read data in @a *pData.
    */
   RC PciRead08(INT32 DomainNumber, INT32 BusNumber, INT32 DeviceNumber, INT32 FunctionNumber,
            INT32 Address, UINT08 * pData);
   RC PciRead16(INT32 DomainNumber, INT32 BusNumber, INT32 DeviceNumber, INT32 FunctionNumber,
            INT32 Address, UINT16 * pData);
   RC PciRead32(INT32 DomainNumber, INT32 BusNumber, INT32 DeviceNumber, INT32 FunctionNumber,
            INT32 Address, UINT32 * pData);
   //@}

   /**
    * @name PCI config write functions.
    */
   //@{

   /**
    * Write @a Data at @a Address to a PCI device with the given
    * @a BusNumber, @a DeviceNumber, and @a FunctionNumber.
    */
   RC PciWrite08(INT32 DomainNumber, INT32 BusNumber, INT32 DeviceNumber, INT32 FunctionNumber,
            INT32 Address, UINT08 Data);
   RC PciWrite16(INT32 DomainNumer, INT32 BusNumber, INT32 DeviceNumber, INT32 FunctionNumber,
            INT32 Address, UINT16 Data);
   RC PciWrite32(INT32 DomainNumber, INT32 BusNumber, INT32 DeviceNumber, INT32 FunctionNumber,
            INT32 Address, UINT32 Data);
   //@}

   RC PciRemove(INT32 domainNumber, INT32 busNumber, INT32 deviceNumber, INT32 functionNumber);
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

   /** @name MemCopy Function
    */
   //@{
   /**
    * Copy 'length' bytes of Data at 'src' to 'dst' using hardware
    * specific optimization, optionally selected by 'type'.
    */
   const UINT32 ASMCOPY    = 0;
   const UINT32 CCOPY      = 1;
   void MemCopy(void * dst, void * src, UINT32 length, UINT32 type = CCOPY);
   //@}

   bool DoesFileExist(string strFilename);
      //!< Return true if file exists.  strFilename
      //!< should include the path to the file.

   RC EraseFile(const string& FileName);
      //! Delete a file.
      //!< should include the path to the file.

   FILE *Fopen(const char *FileName, const char *Mode);
      //!< Drop-in replacement for fopen() in K&R, including setting errno.
      //!< This replacement handles long filenames on some platforms.

   FILE* Popen(const char* command, const char* type);
   INT32 Pclose(FILE* stream);

   /**
    * @name Timing Functions
    */
   //@{
   UINT64 QueryPerformanceCounter();
      //!< Get a timestamp from the performance counter.

   UINT64 QueryPerformanceFrequency();
      //!< Get the frequency of the performance counter (ticks per second).

   UINT32 QueryIntervalTimer();
      //!< Get a timestamp from the interval counter.

   UINT32 QueryIntervalFrequency();
      //!< Get the frequency of the interval counter (ticks per second).

   //! Returns system time in nanoseconds since Epoch
   UINT64 GetWallTimeNS();

   //! Returns system time in microseconds since Epoch
   UINT64 GetWallTimeUS();

   //! Returns system time in milliseconds since Epoch
   UINT64 GetWallTimeMS();
   //@}

   /**
    * @name Environment variable functions.
    */
   //@{
   string GetElw(string Variable);
      //! Get the specified environment variable value. If the variable does
      //! not exist return "".

   RC SetElw(string Variable, string Value);
      //! Set the specified environment variable to the specified value.
      //! If the variable already exists, overwrite the value with the specified
      //! value. If the variable does not exist, create it and set the value.
      //! If the variable does exist, and the value is "", delete the variable.

   void * GetVbiosFromLws(UINT32 GpuInst);

   //! \brief Start a background thread to blinks LEDs to show mods isn't hung.
   //!
   //! This is useful for tests that cannot display any other user interface,
   //! for example an LVDS display loopback test.
   //!
   //! On a DOS PC, we alternately turn on the num/caps/scroll-lock LEDs on the
   //! keyboard.   All other platforms lwrrently return RC::UNSUPPORTED_FUNCTION.
   RC StartBlinkLightsThread();

    /**
    * @name Power Management Functions
    */
   //! \brief (SwitchSystemPowerState): Change to the requested power state.
   //!
   //! This is used to put system in Win32 "suspend" power-state for
   //! iSleepTimeInSec many seconds, then restore to normal power state.
   //! The win32 suspend/hibernate depends upon the TRUE/FALSE bPowerState
   //! value. Win32 power change needs privilege to do so and is
   //! enabled/disabled using AdjustPrivileges().
   //! This function lwrrently returns error and asserts on non-win32
   //! platforms, we'll implement this for other OSs in furture.
    RC SwitchSystemPowerState(bool bPowerState, INT32 iSleepTimeInSec);

   //! Save/restore the display-driver's current display settings.
   //! Translates to EnumDisplaySettings/ChangeDisplaySettingsEx in Win32.

    UINT32 GetOsDisplaySettingsBufferSize();
    RC SaveOsDisplaySettings(void *pOsDisplaySettingsBuffer);
    RC RestoreOsDisplaySettings(void * pOsDisplaySettingsBuffer);

    //! Returns true on platforms where the Resource Manager driver runs as a
    //! part of MODS (static or dynamic library) rather than as part
    //! of the OS kernel.
    //!
    //! When this is true, MODS acts as the operating system, and is responsible
    //! for initializing the resman, hooking GPU interrupts, supplying sysmem
    //! alloc/free callbacks, etc.
#ifdef CLIENT_SIDE_RESMAN
    constexpr bool HasClientSideResman() { return true; }
#else
    constexpr bool HasClientSideResman() { return false; }
#endif

    //!
    //!
    //! Implemented per RFC 1305 (spec for NTPv3)
    //!
    //! @param secs returns the number of seconds since 1900
    //! @param msecs returns the millisecond portion of the time
    //! @param hostname name of the server to connect to (i.e. pool.ntp.org)
    //!
    //! @return A string representing the current time of day recieved from the server. will contain an error string
    //!         if a time is not received.
    //!
    //! @sa Xp::DoesFileExist
    //!
    RC GetNTPTime(UINT32 &secs, UINT32 &msecs, const char *hostname = NULL);

    //! Reads the ECID values of the chipset
    RC ReadChipId(UINT32 * fuseVal240, UINT32 * fuseVal244);

    /**
     * @name Memory Monitoring Functions
     */
    //! \brief (GetVirtualMemoryUsed): Get total virtual memory used by the process, in byte.
    //!
    size_t GetVirtualMemoryUsed();

    //!
    //!
    //! \brief Get the CPUs that are local to a PCI device (if unsupported then
    //!        an empty array of CPU masks is returned)
    //!
    //! \param DomainNumber   Domain number of the PCI device
    //! \param BusNumber      Bus number of the PCI device
    //! \param DeviceNumber   Device number of the PCI device
    //! \param FunctionNumber Function number of the PCI device
    //! \param pLocalCpuMasks Returned vector of CPU masks local to the device
    //!
    //! \return OK if successful or not supported, not OK if supported but an
    //!         error oclwrred when retrieving the CPU masks
    RC GetDeviceLocalCpus
    (
        INT32 DomainNumber,
        INT32 BusNumber,
        INT32 DeviceNumber,
        INT32 FunctionNumber,
        vector<UINT32> *pLocalCpuMasks
    );

    //! Returns NUMA node number where the PCI device is located.
    //!
    //! Returns -1 if NUMA node cannot be identified or if NUMA is disabled or not supported.
    INT32 GetDeviceNumaNode
    (
        INT32 domainNumber,
        INT32 busNumber,
        INT32 deviceNumber,
        INT32 functionNumber
    );

    RC GetFbConsoleInfo(PHYSADDR *pBaseAddress, UINT64 *pSize);
    void SuspendConsole(GpuSubdevice *pGpuSubdevice);
    void ResumeConsole(GpuSubdevice *pGpuSubdevice);

    //! ClockManager class which allows system-wide clocks manipulation.
    //! Specifically designed for CheetAh SOC/Silicon where we talk to clocks driver
    //! in the Linux kernel.
    class ClockManager
    {
    public:
        ClockManager() { }
        virtual ~ClockManager() { }
        virtual RC GetResetHandle
        (
            const char* resetName,
            UINT64* pHandle
        ) = 0;
        virtual RC GetClockHandle
        (
            const char* device,
            const char* controller,
            UINT64* pHandle
        ) = 0;
        virtual RC EnableClock
        (
            UINT64 clockHandle
        ) = 0;
        virtual RC DisableClock
        (
            UINT64 clockHandle
        ) = 0;
        virtual RC GetClockEnabled
        (
            UINT64 clockHandle,
            UINT32 *pEnableCount
        ) = 0;
        virtual RC SetClockRate
        (
            UINT64 clockHandle,
            UINT64 rateHz
        ) = 0;
        virtual RC GetClockRate
        (
            UINT64 clockHandle,
            UINT64* pRateHz
        ) = 0;
        virtual RC SetMaxClockRate
        (
            UINT64 clockHandle,
            UINT64 rateHz
        ) = 0;
        virtual RC GetMaxClockRate
        (
            UINT64 clockHandle,
            UINT64* pRateHz
        ) = 0;
        virtual RC SetClockParent
        (
            UINT64 clockHandle,
            UINT64 parentClockHandle
        ) = 0;
        virtual RC GetClockParent
        (
            UINT64 clockHandle,
            UINT64* pParentClockHandle
        ) = 0;

        //! \brief Reset a device specified by handle
        //! \param [in] Handle to reset structure for particular
        //! device in reset array managed by kernel driver
        //! \param [in] whether we are asserting or deasserting
        virtual RC Reset
        (
            UINT64 handle,
            bool assert
        ) = 0;
    };

    ClockManager* GetClockManager();

    struct BoardInfo
    {
        string boardName;
        string boardSerial;
        UINT32 boardId;
        string cpuArchitecture;
        UINT32 cpuId;
    };

    RC GetBoardInfo(BoardInfo* pBoardInfo);
    RC ReadSystemUUID(string *systemuuid);
    RC ReadHDiskSerialnum(string * hdiskserno);

    struct DriverStats
    {
        UINT64 allocationCount;
        UINT64 mappingCount;
        UINT64 pagesCount;
    };
    RC GetDriverStats(DriverStats *pDriverStats);

    /// Provides access to interactive files, such as in sysfs.
    class InteractiveFile
    {
    public:
        enum OpenFlags
        {
            /// Open file as read-only, file must exist.
            /// Read() will read entire file every time.
            ReadOnly = 1,
            /// Open file as write-only and truncate, file must exist.
            /// Write() will truncate and write entire file every time.
            WriteOnly,
            /// Open file as read-write, file must exist.
            /// Read() will read entire file every time.
            /// Write() will truncate and write entire file every time.
            ReadWrite,
            /// Open file as read-write and truncate, create new file if it does not exist.
            /// Read() will read entire file every time.
            /// Write() will truncate and write entire file every time.
            Create,
            /// Open file as write-only and seek to the end, create new file if it does not exist.
            /// Write() will append more data at the end of the file.
            Append
        };
        static constexpr int IlwalidFd = -1;

        InteractiveFile() = default;
        ~InteractiveFile() { Close(); }
        InteractiveFile(const InteractiveFile&)            = delete;
        InteractiveFile& operator=(const InteractiveFile&) = delete;
        /// Opens a particular file with this object.
        /// Fails if another file has already be opened with this object.
        RC Open(const char* path, OpenFlags flags = ReadWrite);
        void Close();
        string GetPath() const { return m_Path; }
        /// Waits for contents of the file to change.
        /// Only some sysfs files support this. For files
        /// which don't support it, this function will wait
        /// until it times out and returns TIMEOUT_ERROR.
        RC Wait(FLOAT64 timeoutMs);
        /// Read the contents of the file as an integer.
        RC Read(int* pValue);
        /// Read the contents of the file as an integer in hex format.
        RC ReadHex(int* pValue);
        /// Write an integer to the file.
        RC Write(int value);
        /// Read the contents of the file as an unsigned 64-bit integer.
        RC Read(UINT64* pValue);
        /// Write an unsigned 64-bit integer to the file.
        RC Write(UINT64 value);
        /// Read the contents of a double into the file
        RC Read(double *pValue);
        /// Write a double to the file
        RC Write(double value);
        /// Read the contents of the file as a string.
        RC Read(string* pValue);
        /// Read the contents of the file as a vector of unsigned char.
        RC ReadBinary(vector<UINT08> * pValue);
        /// Write a string to the file (std::string).
        RC Write(const string& value) { return Write(value.c_str(), value.size()); }
        /// Write a string to the file (const char*).
        RC Write(const char* value);
        /// Write a string to the file (const char*).
        RC Write(const char* value, size_t size);

    private:
#ifdef _WIN32
        FILE*     m_File      = nullptr;
#else
        int       m_Fd        = IlwalidFd;
#endif
        string    m_Path;
        OpenFlags m_Flags     = ReadWrite;
        bool      m_bNeedSeek = false;
    };

    template<typename T>
    inline RC InteractiveFileRead(const char* filename, T* pValue)
    {
        Xp::InteractiveFile file;
        RC rc;
        CHECK_RC(file.Open(filename, Xp::InteractiveFile::ReadOnly));
        CHECK_RC(file.Read(pValue));
        return OK;
    }

    template<typename T>
    inline RC InteractiveFileReadHex(const char* filename, T* pValue)
    {
        Xp::InteractiveFile file;
        RC rc;
        CHECK_RC(file.Open(filename, Xp::InteractiveFile::ReadOnly));
        CHECK_RC(file.ReadHex(pValue));
        return OK;
    }

    template<typename T>
    inline RC InteractiveFileWrite(const char* filename, T value)
    {
        Xp::InteractiveFile file;
        RC rc;
        CHECK_RC(file.Open(filename, Xp::InteractiveFile::WriteOnly));
        CHECK_RC(file.Write(value));
        return OK;
    }

    template<typename T>
    inline RC InteractiveFileWrite(const char* filename, T value, size_t size)
    {
        Xp::InteractiveFile file;
        RC rc;
        CHECK_RC(file.Open(filename, Xp::InteractiveFile::WriteOnly));
        CHECK_RC(file.Write(value, size));
        return OK;
    }

    template<typename T>
    RC InteractiveFileRead(const char* filename, T* pValue, Tee::Priority pri)
    {
        RC rc = InteractiveFileRead(filename, pValue);
        if (OK == rc)
        {
            Printf(pri, "file read %s with %s\n", filename, Utility::ToString(*pValue).c_str());
        }
        return rc;
    }

    template<typename T>
    RC InteractiveFileReadHex(const char* filename, T* pValue, Tee::Priority pri)
    {
        RC rc = InteractiveFileReadHex(filename, pValue);
        if (OK == rc)
        {
            Printf(pri, "file read %s with %s\n", filename, Utility::ToString(*pValue).c_str());
        }
        return rc;
    }

    template<typename T>
    RC InteractiveFileWrite(const char* filename, T value, Tee::Priority pri)
    {
        RC rc = InteractiveFileWrite(filename, value);
        if (OK == rc)
        {
            Printf(pri, "file write %s with %s\n", filename, Utility::ToString(value).c_str());
        }
        return rc;
    }

    template<typename T>
    RC InteractiveFileReadSilent(const char* filename, T* pValue)
    {
        if (!DoesFileExist(filename))
        {
            return RC::FILE_DOES_NOT_EXIST;
        }
        return Xp::InteractiveFileRead(filename, pValue);
    }

    RC SendMessageToFTB
    (
        UINT32 domain,
        UINT32 bus,
        UINT32 device,
        UINT32 function,
        const vector<tuple<UINT32, UINT32>>& messages
    );

    RC ConfirmUsbAltModeConfig
    (
        UINT32 domain,
        UINT32 bus,
        UINT32 device,
        UINT32 function,
        UINT08 desiredAltMode
    );

    RC EnableUsbAutoSuspend
    (
        UINT32 hcPciDomain,
        UINT32 hcPciBus,
        UINT32 hcPciDevice,
        UINT32 hcPciFunction,
        UINT32 autoSuspendDelayMs
    );

    RC DisablePciDeviceDriver
    (
        UINT32 domain,
        UINT32 bus,
        UINT32 device,
        UINT32 function,
        string* pDriverName
    );

    RC EnablePciDeviceDriver
    (
        UINT32 domain,
        UINT32 bus,
        UINT32 device,
        UINT32 function,
        const string& driverName
    );

    RC GetLwrrUsbAltMode
    (
        UINT32 domain,
        UINT32 bus,
        UINT32 device,
        UINT32 function,
        UINT08 *pLwrrAltMode
    );

    RC GetUsbPowerConsumption
    (
        UINT32 domain,
        UINT32 bus,
        UINT32 device,
        UINT32 function,
        FLOAT64 *pVoltageV,
        FLOAT64 *pLwrrentA
    );

    RC GetPpcFwVersion
    (
        UINT32 domain,
        UINT32 bus,
        UINT32 device,
        UINT32 function,
        string *pPrimaryVersion,
        string *pSecondaryVersion
    );

    RC GetPpcDriverVersion
    (
        UINT32 domain,
        UINT32 bus,
        UINT32 device,
        UINT32 function,
        string *pDriverVersion
    );

    RC GetFtbFwVersion
    (
        UINT32 domain,
        UINT32 bus,
        UINT32 device,
        UINT32 function,
        string *pFtbFwVersion
    );

    RC RdMsrOnCpu(UINT32 cpu, UINT32 reg, UINT32 *pHigh, UINT32 *pLow);
    RC WrMsrOnCpu(UINT32 cpu, UINT32 reg, UINT32 high,   UINT32 low);

    //! Returns CPU usage of the MODS process
    //!
    //! Returns number of seconds (as float) that MODS has been running
    //! for since it was started.  On Linux this can have microsecond
    //! precision.  This includes all oclwpied CPU cores.  For example,
    //! if MODS was running for 10 seconds wall clock time and fully
    //! utilizing two CPU cores, this function will return 20.  The returned
    //! value includes time spent both in user mode and in kernel mode.
    FLOAT64 GetCpuUsageSec();

    //! Tells the kernel to drop all it's buffers and caches
    RC SystemDropCaches();

    //! Tells the kernel to compact free memory
    RC SystemCompactMemory();

    //! Starts kernel logger thread
    RC StartKernelLogger();
} // Xp

#endif // !INCLUDED_XP_H
