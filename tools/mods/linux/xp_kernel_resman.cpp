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
#include "gpu/include/gpumgr.h"
#include "core/include/mgrmgr.h"
#include "core/include/lwrm.h"
#include "lwRmApi.h"
#include "Lwcm.h"
#include "core/include/pci.h"
#include "ctrl/ctrl0073.h"  // for LW0073_CTRL_CMD_SPECIFIC_ENABLE_VGA_MODE

#define WE_NEED_LINUX_XP
#include "xp_linux_internal.h"

#include <poll.h>
#include <fcntl.h>

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
    return RC::UNSUPPORTED_FUNCTION;
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
    return RC::UNSUPPORTED_FUNCTION;
}

//------------------------------------------------------------------------------
//! \brief Alloc system memory with pages aligned as requested
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
    MASSERT(0);
    return RC::UNSUPPORTED_FUNCTION;
}

//------------------------------------------------------------------------------
//!< Free pages allocated by AllocPages or AllocPagesAligned.
void Xp::FreePages(void * Descriptor)
{
    MASSERT(0);
}

RC Xp::MergePages(void** pOutDescriptor, void** pInDescBegin, void** pInDescEnd)
{
    MASSERT(0);
    return RC::UNSUPPORTED_FUNCTION;
}

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
    MASSERT(0);
    return RC::UNSUPPORTED_FUNCTION;
}

//------------------------------------------------------------------------------
void Xp::UnMapPages(void * VirtualAddress)
{
    MASSERT(0);
}

//------------------------------------------------------------------------------
PHYSADDR Xp::GetPhysicalAddress(void *Descriptor, size_t Offset)
{
    MASSERT(0);
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
    return (void *)(uintptr_t)PhysicalAddress;
}

//------------------------------------------------------------------------------
// Colwert virtual address to physical address.
//
PHYSADDR Xp::VirtualToPhysical(volatile void * VirtualAddress, Tee::Priority pri)
{
    return (uintptr_t)VirtualAddress;
}

RC Xp::IommuDmaMapMemory(void  *pDescriptor, string DevName, PHYSADDR *pIova)
{
    MASSERT(0);
    return RC::UNSUPPORTED_FUNCTION;
}

RC Xp::IommuDmaUnmapMemory(void  *pDescriptor, string DevName)
{
    MASSERT(0);
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
    MASSERT(0);
    return RC::UNSUPPORTED_FUNCTION;
}

//------------------------------------------------------------------------------
//!< Undo the mapping created by MapDeviceMemory.
void Xp::UnMapDeviceMemory(void * VirtualAddress)
{
    MASSERT(0);
}

// Stub functions for watchdog functionality
RC Xp::InitWatchdogs(UINT32 TimeoutSecs)
{
    Printf(Tee::PriLow, "Skipping watchdog initialization since"
                        " it is not supported\n");
    return OK;
}

RC Xp::ResetWatchdogs()
{
    Printf(Tee::PriLow, "Skipping watchdog resetting since it is"
                        " not supported\n");
    return OK;
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
    MASSERT(0);
}

void Xp::DisableInterrupts()
{
    MASSERT(0);
}

Platform::Irql Xp::GetLwrrentIrql()
{
    // Since we can't actually raise the IRQL...
    return Platform::NormalIrql;
}

Platform::Irql Xp::RaiseIrql(Platform::Irql NewIrql)
{
    // Since we can't actually raise the IRQL...
    //MASSERT(NewIrql == Platform::NormalIrql);
    return Platform::NormalIrql;
}

void Xp::LowerIrql(Platform::Irql NewIrql)
{
    // Since we can't actually raise the IRQL...
    //MASSERT(NewIrql == Platform::NormalIrql);
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
    return OK;
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
    return OK;
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
// Enable or disable SR-IOV.  Lwrrently, linux mods doesn't support SR-IOV.
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

//------------------------------------------------------------------------------
// Read byte, word, or dword at 'Address' from a PCI device with the given
// 'DomainNumber', 'BusNumber', 'DeviceNumber', and 'FunctionNumber'.
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
    *pData = 0xFFU;
    return OK;
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
    *pData = 0xFFFFU;
    return OK;
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
    *pData = ~0U;
    return OK;
}

//------------------------------------------------------------------------------
// Write 'Data' at 'Address' to a PCI device with the given
// 'DomainNumber', 'BusNumber', 'DeviceNumber', and 'FunctionNumber'.
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
    return OK;
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
    return OK;
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
    return OK;
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
    Printf(Tee::PriLow, "System-level clock setting is not supported on this platform\n");
    return nullptr;
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
//! \param DomainNumber   : PCI domain number of the device.
//! \param BusNumber      : PCI bus number of the device.
//! \param DeviceNumber   : PCI device number of the device
//! \param FunctionNumber : PCI function number of the device
//! \param pLocalCpuMasks : (NOT SUPPORTED) Empty vector returned
//!
//! \return OK
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
    return OK;
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
   return OS_LINUXRM;
}

//------------------------------------------------------------------------------
RC Xp::ValidateGpuInterrupt(GpuSubdevice  *pGpuSubdevice,
                            UINT32 whichInterrupts)
{
    return OK;
}

//------------------------------------------------------------------------------
RC Xp::GetNTPTime
(
   UINT32 &secs,
   UINT32 &msecs,
   const char *hostname
)
{
    return OK;
}

namespace
{
    struct RmEventHandle
    {
        int    fd        = -1;
        int    waitFd[2] = { -1, -1 };
        UINT32 hClient   = 0;
        UINT32 hDevice   = 0;

        RmEventHandle(UINT32 hClient, UINT32 hDevice)
        : hClient(hClient), hDevice(hDevice)
        {
        }

        ~RmEventHandle()
        {
            if (fd != -1)
            {
                LwRmFreeOsEvent(hClient, hDevice, fd);
            }
            if (waitFd[0] != -1)
            {
                close(waitFd[0]);
            }
            if (waitFd[1] != -1)
            {
                close(waitFd[1]);
            }
        }
    };
}

namespace
{
    bool MakeNonBlocking(int fd)
    {
        const int flags = fcntl(fd, F_GETFL);
        if (fcntl(fd, F_SETFL, flags | O_NONBLOCK) == -1)
        {
            return false;
        }
        return true;
    }
}

//------------------------------------------------------------------------------
// Allocate an OS event - Returns pointer to file descriptor
void* Xp::AllocOsEvent(UINT32 hClient, UINT32 hDevice)
{
    auto pEvent = make_unique<RmEventHandle>(hClient, hDevice);

    if (pipe(pEvent->waitFd))
    {
        return nullptr;
    }

    if (!MakeNonBlocking(pEvent->waitFd[0]) || !MakeNonBlocking(pEvent->waitFd[1]))
    {
        return nullptr;
    }

    if (hClient || hDevice)
    {
        const int status = LwRmAllocOsEvent(
                hClient,
                hDevice,
                nullptr,
                reinterpret_cast<void**>(&pEvent->fd));
        MASSERT(status == LW_OK);
        if (status != LW_OK)
        {
            return nullptr;
        }
    }

    return pEvent.release();
}

//------------------------------------------------------------------------------
// Free an OS event
void Xp::FreeOsEvent(void* pEvent, UINT32 hClient, UINT32 hDevice)
{
    delete static_cast<RmEventHandle*>(pEvent);
}

//------------------------------------------------------------------------------
void Xp::SetOsEvent(void* pEvent)
{
    char buf = 'X';
    if (write(static_cast<RmEventHandle*>(pEvent)->waitFd[1], &buf, 1) != 1)
        Printf(Tee::PriNormal, "Failed to set OS event\n");
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
    *pNumCompleted = 0;

    vector<struct pollfd> fds(numOsEvents * 2);
    for (UINT32 i = 0; i < fds.size(); i += 2)
    {
        const auto pEvent = static_cast<RmEventHandle*>(pOsEvents[i >> 1]);

        fds[i].fd          = pEvent->fd;
        fds[i].events      = POLLIN | POLLPRI;
        fds[i].revents     = 0;
        fds[i + 1].fd      = pEvent->waitFd[0];
        fds[i + 1].events  = POLLIN | POLLPRI;
        fds[i + 1].revents = 0;
    }

    const int timeout = (timeoutMs == Tasker::NO_TIMEOUT) ? -1 : static_cast<int>(timeoutMs);
    const int ret = poll(fds.data(), fds.size(), timeout);

    if (ret == 0)
    {
        return RC::TIMEOUT_ERROR;
    }

    if (ret < 0)
    {
        RC rc = Utility::InterpretFileError();
        MASSERT(rc != RC::OK);
        return rc;
    }

    for (UINT32 i = 0; i < fds.size(); i++)
    {
        if (!fds[i].revents)
        {
            continue;
        }

        const UINT32 evIdx = i >> 1;
        const auto pEvent = static_cast<RmEventHandle*>(pOsEvents[evIdx]);

        // Odd events are the fake events set through a pipe just to interrupt waiting
        if (i & 1)
        {
            // Empty buffer of events aclwmulated through SetOsEvent() (mainly on exit)
            const int fd = pEvent->waitFd[0];
            char      buf;
            while (read(fd, &buf, 1) == 1);
        }
        // Even events are the actual OS events
        else
        {
            MASSERT(pEvent->fd != -1);
            LwUnixEvent unixEvent;
            LwU32       moreEvents;

            // Empty the event queue
            do
            {
                RC rc = RmApiStatusToModsRC(LwRmGetEventData(
                            pEvent->hClient, pEvent->fd, &unixEvent, &moreEvents));
                if (rc != RC::OK)
                {
                    if (*pNumCompleted)
                    {
                        rc.Clear();
                    }
                    return rc;
                }
            } while (moreEvents);
        }

        if ((*pNumCompleted < maxCompleted) &&
            // Count each event once
            (!*pNumCompleted || (evIdx != pCompletedIndices[*pNumCompleted - 1])))
        {
            pCompletedIndices[(*pNumCompleted)++] = evIdx;
        }
    }

    return *pNumCompleted ? RC::OK : RC::TIMEOUT_ERROR;
}

//------------------------------------------------------------------------------
// Disable the user interface, i.e. disable VGA.
//
RC Xp::DisableUserInterface()
{
    RC rc = OK;

    // We do not touch user interface on devices we did not enumerate
    GpuDevMgr* pGpuDevMgr = (GpuDevMgr*)DevMgrMgr::d_GraphDevMgr;
    if (!pGpuDevMgr || !pGpuDevMgr->GetPrimaryDevice())
    {
        Printf(Tee::PriLow, "Skipping DisableUserInterface.\n");
        return OK;
    }

    if (DevMgrMgr::d_GraphDevMgr)
    {
        // Non-Sim, i.e. linux with kernel resman.
        //
        // Just guessing here, this code used to be in EvoDisplay::DoModeSet,
        // for HW non-dos, non-macos platforms.
        // My guess is that this is harmless but unnecessary.

        LwRmPtr pLwRm;
        for (GpuSubdevice * pSubdev = pGpuDevMgr->GetFirstGpu();
             pSubdev != NULL; pSubdev = pGpuDevMgr->GetNextGpu(pSubdev))
        {
            // If the subdevice hasnt been initialized then calling the config
            // set will crash/fail
            if (pSubdev->IsInitialized())
            {
                LW0073_CTRL_SPECIFIC_ENABLE_VGA_MODE_PARAMS params = {};
                CHECK_RC(pLwRm->ControlBySubdevice(pSubdev, LW0073_CTRL_CMD_SPECIFIC_ENABLE_VGA_MODE, &params, sizeof(params)));
            }
        }
    }

    return rc;
}

//------------------------------------------------------------------------------
// Enable the user interface.
//
RC Xp::EnableUserInterface()
{
    return OK;
}

//------------------------------------------------------------------------------
bool Xp::HasFeature(Feature feat)
{
    switch (feat)
    {
        case HAS_KERNEL_LEVEL_ACCESS:
            return false;
        default:
            MASSERT(!"Not implemented");
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
RC Xp::DisablePciDeviceDriver
(
    UINT32 domain,
    UINT32 bus,
    UINT32 device,
    UINT32 function,
    string* pDriverName
)
{
    return RC::UNSUPPORTED_FUNCTION;
}

RC Xp::EnablePciDeviceDriver
(
    UINT32 domain,
    UINT32 bus,
    UINT32 device,
    UINT32 function,
    const string& driverName
)
{
    return RC::UNSUPPORTED_FUNCTION;
}

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

RC Xp::RdMsrOnCpu(UINT32 cpu, UINT32 reg, UINT32 *pHigh, UINT32 *pLow)
{
    return RC::UNSUPPORTED_FUNCTION;
}

RC Xp::WrMsrOnCpu(UINT32 cpu, UINT32 reg, UINT32 high, UINT32 low)
{
    return RC::UNSUPPORTED_FUNCTION;
}

RC Xp::SystemDropCaches()
{
    return RC::UNSUPPORTED_FUNCTION;
}

RC Xp::SystemCompactMemory()
{
    return RC::UNSUPPORTED_FUNCTION;
}
