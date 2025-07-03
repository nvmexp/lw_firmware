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

// Cross platform (XP) interface.

#include "core/include/xp.h"

#include "core/include/cpu.h"
#include "core/include/evntthrd.h"
#include "core/include/filesink.h"
#include "core/include/globals.h"
#include "core/include/jscript.h"
#include "core/include/massert.h"
#include "core/include/mgrmgr.h"
#include "core/include/pci.h"
#include "core/include/platform.h"
#include "core/include/script.h"
#include "core/include/sockmods.h"
#include "core/include/tasker.h"
#include "core/include/tee.h"
#include "core/include/utility.h"
#include "device/interface/pcie.h"
#include "gpu/include/gpumgr.h"
#include "gpu/include/gpusbdev.h"
#include "jt.h"
#include "mxm_spec.h"
#include "nbci.h"
#include "lwtypes.h"
#include "rm.h"
#include "rmjt.h"
#include "cheetah/include/sysutil.h"
#include "cheetah/include/tegrasocdevice.h"
#include "cheetah/include/tegrasocdeviceptr.h"

#define WE_NEED_LINUX_XP
#include "xp_linux_internal.h"

// For restoring Azalia on the GPU
#ifdef INCLUDE_AZALIA
#include "device/azalia/azactrlmgr.h"
#include "device/azalia/azactrl.h"
#endif

#include <errno.h>
#include <fcntl.h>
#include <map>
#include <netdb.h>
#include <netinet/in.h>
#include <signal.h>
#include <string>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>

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
    MASSERT(0);
    return RC::UNSUPPORTED_FUNCTION;
}

//------------------------------------------------------------------------------
//!< Free pages allocated by AllocPages or AllocPagesAligned.
void Xp::FreePages(void * Descriptor)
{
    MASSERT(0);
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

RC Xp::PciGetBarInfo
(
    INT32 domainNumber,
    INT32 busNumber,
    INT32 deviceNumber,
    INT32 functionNumber,
    INT32 barIndex,
    UINT64* pBaseAddress,
    UINT64* pBarSize
)
{
    return Pci::GetBarInfo(domainNumber, busNumber,
                           deviceNumber, functionNumber,
                           barIndex, pBaseAddress, pBarSize);
}

RC Xp::PciEnableSriov
(
    UINT32 domain,
    UINT32 bus,
    UINT32 device,
    UINT32 function,
    UINT32 vfCount
)
{
    MASSERT(Platform::GetSimulationMode() == Platform::Hardware);

    UINT32 supported = 0;
    Platform::EscapeRead("SET_NUM_VF_SUPPORTED", 0,
                         sizeof(supported), &supported);
    if (!supported)
    {
        return RC::UNSUPPORTED_FUNCTION;
    }

    const UINT32 configAddr = Pci::GetConfigAddress(domain, bus,
                                                    device, function, 0);
    if (Platform::GpuEscapeWriteBuffer(configAddr, "SET_NUM_VF", 0,
                                       sizeof(vfCount), &vfCount) != 0)
    {
        return RC::SOFTWARE_ERROR;
    }

    return OK;
}

RC Xp::PciGetIRQ
(
    INT32 domainNumber,
    INT32 busNumber,
    INT32 deviceNumber,
    INT32 functionNumber,
    UINT32 * pIrq
)
{
    return Pci::GetIRQ(domainNumber, busNumber,
                       deviceNumber, functionNumber, pIrq);
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

RC Xp::PciResetFunction
(
    INT32    domainNumber,
    INT32    busNumber,
    INT32    deviceNumber,
    INT32    functionNumber
)
{
    return RC::UNSUPPORTED_FUNCTION;
}

namespace
{
    RC FakeTegraDCB(UINT32 DSMMXMSubfunction, UINT32* pInOut, UINT16* size)
    {
        const UINT32 NBSI_TEGRA_DCB = 0x5444;
        if (DSMMXMSubfunction == LW_NBCI_FUNC_SUPPORT && *size >= 4)
        {
            *pInOut = (1<<LW_NBCI_FUNC_SUPPORT) | (1<<LW_NBCI_FUNC_GETOBJBYTYPE);
            *size = 4;
            return OK;
        }
        else if (DSMMXMSubfunction == LW_NBCI_FUNC_GETOBJBYTYPE &&
                 *pInOut == ((NBSI_TEGRA_DCB)<<16))
        {
            // Fake first 4 DWORDs (DSM header)
            const UINT32 fourDwords = 4*sizeof(UINT32);
            const UINT32 minDcbSize = 5*sizeof(UINT32);
            if (*size < fourDwords + minDcbSize)
            {
                return RC::MEMORY_SIZE_ERROR;
            }
            memset(pInOut, 0, fourDwords);
            UINT32 dcbSize = *size - fourDwords;
            const RC rc = CheetAh::SocPtr()->GetDCB(pInOut+4, &dcbSize);
            *size = dcbSize + fourDwords;
            return rc;
        }
        return RC::UNSUPPORTED_FUNCTION;
    }

    RC FakeTegraDDC(UINT32 ddcPort, UINT08* pOut, UINT32* pOutSize)
    {
        vector<UINT08> buf;
        RC rc = CheetAh::SocPtr()->GetEDID(ddcPort, &buf);
        if (rc != RC::UNSUPPORTED_DEVICE)
        {
            if (rc != OK)
            {
                return rc;
            }
            const UINT32 inSize = *pOutSize;
            *pOutSize = buf.size();
            if (inSize < buf.size())
            {
                return RC::MEMORY_SIZE_ERROR;
            }
            memcpy(pOut, &buf[0], buf.size());
            return OK;
        }
        return rc;
    }
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
    RC rc;
    GpuDevMgr* const pGpuDevMgr = static_cast<GpuDevMgr*>(DevMgrMgr::d_GraphDevMgr);
    MASSERT(pGpuDevMgr);
    GpuSubdevice* const pSubdev = pGpuDevMgr->GetSubdeviceByGpuInst(GpuInst);
    if (!pSubdev)
    {
        return (RC::SOFTWARE_ERROR);
    }

    // If running on real silicon, use the SmbEC controller.
    if (Platform::GetSimulationMode() == Platform::Hardware &&
        !pSubdev->IsEmulation())
    {
        Xp::JTMgr * pJT = Xp::GetJTMgr(GpuInst);
        auto pGpuPcie = pSubdev->GetInterface<Pcie>();
        MASSERT(pGpuPcie);

        if (pJT)
        {
            return(pJT->DSM_Eval(pGpuPcie->DomainNumber(),
                                 pGpuPcie->BusNumber(),
                                 pGpuPcie->DeviceNumber(),
                                 pGpuPcie->FunctionNumber(),
                                 Function,
                                 DSMSubfunction,
                                 pInOut,
                                 pSize));
        }
        else
        {
            return(RC::UNSUPPORTED_FUNCTION);
        }
    }

    // From here on we are running on Emulation or RTL
    static UINT32 lastGc6Cmd = DRF_DEF(_JT_FUNC,_POWERCONTROL,_GPU_POWER_CONTROL,_XGIS);
    UINT32 action = *pInOut;

    switch (DSMSubfunction)
    {
        case 0: //JT_FUNC_SUPPORT
            *pInOut =
                (1 << 0)          | // JT_FUNC_SUPPORT
                (1<<JT_FUNC_CAPS) |
                (1 << JT_FUNC_POWERCONTROL);
            *pSize = sizeof(UINT32);
            break;

        case JT_FUNC_CAPS:
            *pInOut = DRF_DEF(_JT_FUNC,_CAPS,_JT_ENABLED,_TRUE);
            if (Platform::GetSimulationMode() == Platform::Hardware)
            {
                if (pSubdev->IsEmulation())
                {
                    // For emulation, force power-off to be synchronous.
                    *pInOut |= DRF_DEF(_JT_FUNC,_CAPS,_REVISION_ID,_VERIF_0_03);
                }
                else
                {
                    // For silicon, EC/smbus is only supported in MFG MODS.
                    *pInOut = DRF_DEF(_JT_FUNC,_CAPS,_JT_ENABLED,_FALSE);
                }
            }
            else if (Platform::GetSimulationMode() == Platform::RTL)
            {
                // For RTL, the production POWERCONTROL sequences are supported.
                *pInOut |= DRF_DEF(_JT_FUNC,_CAPS,_REVISION_ID,_2_00);
            }
            else
            {
                // For FMODEL, skip POWERCONTROL sequences.
                *pInOut |= DRF_DEF(_JT_FUNC,_CAPS,_REVISION_ID,_VERIF_0_01);
            }
            *pSize = sizeof(UINT32);
            break;

        case JT_FUNC_POWERCONTROL:
            if (Platform::GetSimulationMode() == Platform::RTL)
            {
                switch(DRF_VAL(_JT_FUNC, _POWERCONTROL,_GPU_POWER_CONTROL,action))
                {
                    // enter GC6 requires disabling the PCIE root complex port.
                    case LW_JT_FUNC_POWERCONTROL_GPU_POWER_CONTROL_EGNS:
                    case LW_JT_FUNC_POWERCONTROL_GPU_POWER_CONTROL_EGIS:
                        // Force the Azalia controller to enumerate via call to GetAzaliaController()
                        // before we shutdown the link. This required to prevent any Azalia cfg reads
                        // prior to bringing the link back up(See bug 1220496).
                        #ifdef INCLUDE_AZALIA
                        {
                            AzaliaController* const pAzalia = pSubdev->GetAzaliaController();
                            if (pAzalia)
                            {
                                SysUtil::PciInfo azaPciInfo = {0};
                                CHECK_RC(pAzalia->GetPciInfo(&azaPciInfo));
                                Printf(Tee::PriNormal,"Found Azalia(Domain Bus Dev Func):%x %x %x %x prior to disabling the link.\n",
                                    azaPciInfo.DomainNumber,azaPciInfo.BusNumber,azaPciInfo.DeviceNumber,azaPciInfo.FunctionNumber);
                            }
                        }
                        #endif

                        Platform::EscapeWrite("gc6plus_pcie_link_escape_hi", 0, 1, 1);
                        lastGc6Cmd = action;
                        *pInOut = DRF_DEF(_JT_FUNC,_POWERCONTROL,_GPU_GC_STATE,_TRANSITION);
                        *pSize = sizeof(UINT32);
                        break;

                    // exit GC6 requires enabling the PCIE root complex port.
                    case LW_JT_FUNC_POWERCONTROL_GPU_POWER_CONTROL_XGXS:
                    case LW_JT_FUNC_POWERCONTROL_GPU_POWER_CONTROL_XGIS:
                    {
                        Platform::EscapeWrite("gc6plus_pcie_link_escape_hi", 0, 1, 0);
                        Platform::EscapeWrite("gc6plus_pcie_link_escape_lo", 0, 1, 1);
                        lastGc6Cmd = action;
                        *pInOut = DRF_DEF(_JT_FUNC,_POWERCONTROL,_GPU_GC_STATE,_TRANSITION);
                        *pSize = sizeof(UINT32);

                        // we want to prevent PEX errors see B1220496
                        auto pGpuPcie = pSubdev->GetInterface<Pcie>();
                        // we need to wait for the link training to complete
                        // before sending the write transaction. B1220496
                        Tasker::Sleep(10);
                        Platform::PciWrite32(pGpuPcie->DomainNumber(),
                                             pGpuPcie->BusNumber(),
                                             pGpuPcie->DeviceNumber(),
                                             pGpuPcie->FunctionNumber(),
                                             0, //offset
                                             0); // data

                        #ifdef INCLUDE_AZALIA
                        AzaliaController* const pAzalia = pSubdev->GetAzaliaController();
                        if (pAzalia)
                        {
                            SysUtil::PciInfo azaPciInfo = {0};
                            CHECK_RC(pAzalia->GetPciInfo(&azaPciInfo));
                            Printf(Tee::PriNormal,"Writing to Azalia(Domain Bus Dev Func):%x %x %x %x after enabling the link.\n",
                                azaPciInfo.DomainNumber, azaPciInfo.BusNumber,azaPciInfo.DeviceNumber,azaPciInfo.FunctionNumber);
                            Platform::PciWrite32(azaPciInfo.DomainNumber,
                                                 azaPciInfo.BusNumber,
                                                 azaPciInfo.DeviceNumber,
                                                 azaPciInfo.FunctionNumber,
                                                 0, //offset
                                                 0); // data
                        }
                        #endif

                        break;
                    }

                    case LW_JT_FUNC_POWERCONTROL_GPU_POWER_CONTROL_GSS:
                    {
                        UINT32 linkDisabled = 0;
                        UINT32 linkEnabled = 0;
                        Platform::EscapeRead("gc6plus_pcie_link_escape_hi",0,1,&linkDisabled);
                        Platform::EscapeRead("gc6plus_pcie_link_escape_lo",0,1,&linkEnabled);

                        if (FLD_TEST_DRF(_JT_FUNC,_POWERCONTROL,_GPU_POWER_CONTROL,_XGXS,lastGc6Cmd) ||
                            FLD_TEST_DRF(_JT_FUNC,_POWERCONTROL,_GPU_POWER_CONTROL,_XGIS,lastGc6Cmd))
                        {
                            if (linkEnabled > 0 )
                            {
                                *pInOut = DRF_DEF(_JT_FUNC,_POWERCONTROL,_GPU_GC_STATE,_ON) |
                                          DRF_DEF(_JT_FUNC,_POWERCONTROL,_GPU_POWER_STATE,_PWOK);
                            }
                            else
                            {
                                *pInOut = DRF_DEF(_JT_FUNC,_POWERCONTROL,_GPU_GC_STATE,_TRANSITION) |
                                          DRF_DEF(_JT_FUNC,_POWERCONTROL,_GPU_POWER_STATE,_OFF);
                            }
                        }
                        else if (FLD_TEST_DRF(_JT_FUNC,_POWERCONTROL,_GPU_POWER_CONTROL,_EGNS,lastGc6Cmd) ||
                                 FLD_TEST_DRF(_JT_FUNC,_POWERCONTROL,_GPU_POWER_CONTROL,_EGIS,lastGc6Cmd))
                        {
                            if (linkDisabled > 0)
                            {
                                *pInOut = DRF_DEF(_JT_FUNC,_POWERCONTROL,_GPU_GC_STATE,_GC6) |
                                          DRF_DEF(_JT_FUNC,_POWERCONTROL,_GPU_POWER_STATE,_OFF);
                            }
                            else
                            {
                                *pInOut = DRF_DEF(_JT_FUNC,_POWERCONTROL,_GPU_GC_STATE,_TRANSITION) |
                                          DRF_DEF(_JT_FUNC,_POWERCONTROL,_GPU_POWER_STATE,_PWOK);
                            }
                        }
                        else // this shouldn't happen.
                        {
                            *pInOut = DRF_DEF(_JT_FUNC,_POWERCONTROL,_GPU_GC_STATE,_TRANSITION);
                        }
                        *pSize = sizeof(UINT32);
                        break;
                    }
                    default:
                        rc = RC::SOFTWARE_ERROR;
                        break;
                }
                break;
            }

            if (Platform::GetSimulationMode() == Platform::Hardware)
            {
                PexDevice *pPexDev = NULL;
                UINT32 Port;
                pSubdev->GetInterface<Pcie>()->GetUpStreamInfo(&pPexDev, &Port);
                if(!pPexDev)
                {
                    rc = RC::UNSUPPORTED_FUNCTION;
                    break;
                }

                switch(DRF_VAL(_JT_FUNC, _POWERCONTROL,_GPU_POWER_CONTROL,action))
                {
                    // exit GC6 requires enabling the PCIE root complex port.
                    case LW_JT_FUNC_POWERCONTROL_GPU_POWER_CONTROL_XGIS:
                    case LW_JT_FUNC_POWERCONTROL_GPU_POWER_CONTROL_XGXS:
                    {
                        Printf(Tee::PriNormal,"xp_sim:Exit GC6:\n");
                        // Tell the EC to power up the GPU(Kepler style GC6)
                        pSubdev->SetBackdoorGpuPowerGc6(true);

                        rc = pPexDev->EnableDownstreamPort(Port);
                        if(OK != rc)
                        {
                            Printf(Tee::PriNormal,"EnableDownstreamPort failed:%d\n",rc.Get());
                        }
                        lastGc6Cmd = action;
                        *pInOut = DRF_DEF(_JT_FUNC,_POWERCONTROL,_GPU_GC_STATE,_TRANSITION) |
                                  DRF_DEF(_JT_FUNC,_POWERCONTROL,_GPU_POWER_STATE,_OFF);
                        *pSize = sizeof(UINT32);

                        // B1493276 we need to give the emulator's speedbrigde time to stablize
                        // There appears to be a window after enabling the link where the pci config
                        // space returns incorrect values. If we don't wait long enough for the
                        // config space to stablize then subsequent calls to GSS below will fake
                        // the wrong value and cause RM to read from the config space before the
                        // link is active. Once B1493276 has been fixed you can remove this initial
                        // delay, but you need to add 10 seconds to the second delay.
                        Printf(Tee::PriNormal,"Sleeping for 15 sec before checking DLLAR(B1493276).\n");
                        Tasker::Sleep(15000);

                        bool bSupported = false;
                        CHECK_RC(pPexDev->IsDLLAReportingSupported(Port,&bSupported));
                        if (!bSupported)
                        {
                            Printf(Tee::PriNormal,"Sleeping for 30 sec for link to come up.\n");
                            Tasker::Sleep(30000);
                        }
                        break;

                    }

                    // enter GC6 requires disabling the PCIE root complex port.
                    case LW_JT_FUNC_POWERCONTROL_GPU_POWER_CONTROL_EGIS:
                    case LW_JT_FUNC_POWERCONTROL_GPU_POWER_CONTROL_EGNS:
                        Printf(Tee::PriNormal,"xp_sim:Enter GC6:\n");
                        // There may be some pending PEX transactions from RM so lets delay
                        // to allow them to flush before disabling the root complex port.
                        Printf(Tee::PriNormal,"Sleeping for 2 sec before disabling the link.\n");
                        Tasker::Sleep(2000);
                        rc = pPexDev->DisableDownstreamPort(Port);
                        if(OK != rc)
                        {
                            Printf(Tee::PriNormal,"DisableDownstreamPort failed:%d\n",rc.Get());
                        }
                        lastGc6Cmd = action;
                        *pInOut = DRF_DEF(_JT_FUNC,_POWERCONTROL,_GPU_GC_STATE,_TRANSITION) |
                                  DRF_DEF(_JT_FUNC,_POWERCONTROL,_GPU_POWER_STATE,_PWOK);
                        *pSize = sizeof(UINT32);

                        // Tell the EC to power down the GPU (Kepler style GC6)
                        pSubdev->SetBackdoorGpuPowerGc6(false);
                        break;

                    case LW_JT_FUNC_POWERCONTROL_GPU_POWER_CONTROL_GSS:
                        Printf(Tee::PriNormal,"xp_sim:Get Power Status:\n");

                        bool bActive;
                        if (FLD_TEST_DRF(_JT_FUNC,_POWERCONTROL,_GPU_POWER_CONTROL,_EGNS,lastGc6Cmd) ||
                            FLD_TEST_DRF(_JT_FUNC,_POWERCONTROL,_GPU_POWER_CONTROL,_EGIS,lastGc6Cmd))
                        {
                            bActive = false;
                        }
                        else
                        {
                            bActive = true;
                        }

                        // returns OK if bActive matches hardware link status
                        Printf(Tee::PriNormal,"PollingDownstreamPortLinkStatus for link to be %s\n",bActive ? "active" : "inactive");
                        rc = pPexDev->PollDownstreamPortLinkStatus(Port, 10000,
                            bActive ? PexDevice::LINK_ENABLE : PexDevice::LINK_DISABLE);
                        if (OK == rc || rc == RC::UNSUPPORTED_FUNCTION)
                        {
                            if(rc == RC::UNSUPPORTED_FUNCTION)
                                Printf(Tee::PriNormal,"Unsupported function! faking link status to %s\n",bActive ? "active" : "inactive");
                            else
                                Printf(Tee::PriNormal,"Success,link is %s\n",bActive ? "active" : "inactive");

                            rc.Clear();
                            // we found what we were looking for or we are going to fake it.
                            if (bActive)
                            {
                                *pInOut = DRF_DEF(_JT_FUNC,_POWERCONTROL,_GPU_GC_STATE,_ON) |
                                          DRF_DEF(_JT_FUNC,_POWERCONTROL,_GPU_POWER_STATE,_PWOK);
                            }
                            else
                            {
                                *pInOut = DRF_DEF(_JT_FUNC,_POWERCONTROL,_GPU_GC_STATE,_GC6) |
                                          DRF_DEF(_JT_FUNC,_POWERCONTROL,_GPU_POWER_STATE,_OFF);
                            }
                        }
                        else
                        {
                            *pInOut = DRF_DEF(_JT_FUNC,_POWERCONTROL,_GPU_GC_STATE,_TRANSITION);
                            rc.Clear();
                        }
                        *pSize = sizeof(UINT32);
                        break;

                    default:
                        rc = RC::SOFTWARE_ERROR;
                        break;
                }
            }
            break;

        default:
            rc = RC::UNSUPPORTED_FUNCTION;
            break;
    }
    return rc;
}

//------------------------------------------------------------------------------
RC Xp::CallACPIGeneric(UINT32 GpuInst,
                       UINT32 Function,
                       void *Param1,
                       void *Param2,
                       void *Param3)
{
    // Fake DCB on CheetAh.
    if (Platform::IsTegra() && Function == MODS_DRV_CALL_ACPI_GENERIC_DSM_NBCI)
    {
        return FakeTegraDCB(
                static_cast<UINT32>(reinterpret_cast<uintptr_t>(Param1)),
                static_cast<UINT32*>(Param2),
                static_cast<UINT16*>(Param3));
    }

    // Fake DDC (EDID) on CheetAh
    if (Platform::IsTegra() && Function == MODS_DRV_CALL_ACPI_GENERIC_DDC)
    {
        return FakeTegraDDC(
                static_cast<UINT32>(reinterpret_cast<uintptr_t>(Param1)),
                static_cast<UINT08*>(Param2),
                static_cast<UINT32*>(Param3));
    }

    if (Function == MODS_DRV_CALL_ACPI_GENERIC_DSM_JT)
    {
        return CallACPI_DSM_Eval(GpuInst,
                                Function,
                                (UINT32)(size_t)Param1, //DSMSubfunction
                                (UINT32*)Param2,        //pInOut
                                (UINT16*)Param3);       //pSize
    }

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
namespace Platform
{
    Xp::ClockManager* GetClockManager();
}
Xp::ClockManager* Xp::GetClockManager()
{
    return Platform::GetClockManager();
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
   return OS_LINUXSIM;
}

#ifndef ILWALID_SOCKET
#define ILWALID_SOCKET -1
#endif

#ifndef SOCKET_ERROR
#define SOCKET_ERROR -1
#endif

struct NtpTimePacket
{
    UINT32 dwInteger;
    UINT32 dwFractional;
};

struct NtpBasicInfo
{
  unsigned char liVnMode;
  unsigned char stratum;
  char poll;
  char precision;
  INT32 rootDelay;
  INT32 rootDispersion;
  char referenceID[4];
  NtpTimePacket referenceTimestamp;
  NtpTimePacket originateTimestamp;
  NtpTimePacket receiveTimestamp;
  NtpTimePacket transmitTimestamp;
};

struct NtpAuthenticationInfo
{
  UINT32 keyID;
  unsigned char messageDigest[16];
};

struct NtpFullPacket
{
  NtpBasicInfo          basic;
  NtpAuthenticationInfo auth;
};

/**
 *------------------------------------------------------------------------------
 * @function(Xp::ValidateGpuInterrupt)
 *
 * Confirm that the GPU's interrupt mechanism is working.
 *------------------------------------------------------------------------------
 */

RC Xp::ValidateGpuInterrupt(GpuSubdevice  *pGpuSubdevice,
                            UINT32 whichInterrupts)
{
    JavaScriptPtr pJs;
    bool validateInterrupts = false;
    pJs->GetProperty(pJs->GetGlobalObject(), "g_ValidateInterrupts",
            &validateInterrupts);
    if (validateInterrupts)
    {
        UINT32 intrToCheck = 0;
        if (whichInterrupts & IntaIntrCheck)
        {
            intrToCheck |= GpuSubdevice::HOOKED_INTA;
        }
        if (whichInterrupts & MsiIntrCheck)
        {
            intrToCheck |= GpuSubdevice::HOOKED_MSI;
        }
        if (whichInterrupts & MsixIntrCheck)
        {
            intrToCheck |= GpuSubdevice::HOOKED_MSIX;
        }
        RC rc = pGpuSubdevice->ValidateInterrupts(intrToCheck);
        Tee::FlushToDisk();
        CHECK_RC(rc);
    }
    return OK;
}

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
RC Xp::GetNTPTime
(
   UINT32 &secs,
   UINT32 &msecs,
   const char *hostname
)
{
    int socket_handle;
    sockaddr_in addr_info;
    struct hostent *he;
    INT32 ipaddr;
    string str_hostname;

    if (hostname == NULL)
    {
        str_hostname = "bigben.lwpu.com";
    }
    else
    {
        str_hostname = hostname;
    }

    secs = msecs = 0;

    //if ((ipaddr = inet_addr(str_hostname.c_str())) < 0)
    {
        if ((he = gethostbyname(str_hostname.c_str())) == NULL )
        {
            printf("Utility::GetNTPLwrrentTime: failed to connect to resolve host\n");
            return OK;
        }
        memcpy(&ipaddr, he->h_addr, he->h_length);
    }

    memset(&addr_info, 0, sizeof(addr_info));
    addr_info.sin_family = AF_INET;
    addr_info.sin_port = htons(123);
    addr_info.sin_addr.s_addr = ipaddr;

    socket_handle = socket(AF_INET, SOCK_DGRAM, 0);
    if (socket_handle < 0)
    {
        printf("Utility::GetNTPLwrrentTime: failed to open socket to host\n");
        return OK;
    }

    // Set socket to non-blocking
    int flags;

    if ((flags = fcntl(socket_handle, F_GETFL, 0)) < 0)
    {
        printf("Utility::GetNTPLwrrentTime: Error retrieving flags for socket file descriptor\n");
        close(socket_handle);
        return OK;
    }

    if (fcntl(socket_handle, F_SETFL, flags | O_NONBLOCK) < 0)
    {
        printf("Utility::GetNTPLwrrentTime: Error setting flags for socket file descriptor\n");
        close(socket_handle);
        return OK;
    }

    int result = connect(socket_handle, (sockaddr*)&addr_info, sizeof(addr_info));

    if (result == ILWALID_SOCKET)
    {
        printf("Utility::GetNTPLwrrentTime: Error trying to connect to host\n");
        close(socket_handle);
        return OK;
    }

    NtpBasicInfo nbi;

    memset(&nbi, 0, sizeof(NtpBasicInfo));
    nbi.liVnMode = 27; // NTP Client Request for NTP Version 3.0
    nbi.transmitTimestamp.dwInteger = 0;
    nbi.transmitTimestamp.dwFractional = 0;

    fd_set writefds;
    timeval timeout;
    int sel_result;
    int bytes_written = 0;

    FD_ZERO(&writefds);
    FD_SET(socket_handle, &writefds);
    timeout.tv_sec = 2;
    timeout.tv_usec = 0;

    sel_result = select((int)socket_handle + 1, NULL, &writefds, NULL, &timeout);

    if ( sel_result )
    {
        // received data or timed out
        if ( sel_result < 0 )
        {
            // error
            printf("Utility::GetNTPLwrrentTime: Error sending: %d\n", errno);
            close(socket_handle);
            return OK;
        }
        else if ( FD_ISSET(socket_handle, &writefds) )
        {
            bytes_written = send(socket_handle, (char*)&nbi, sizeof(nbi), 0);

            if ( bytes_written == SOCKET_ERROR )
            {
                printf("Utility::GetNTPLwrrentTime: Error writing to socket\n");
                close(socket_handle);
                return OK;
            }
            else
            {
                if ( bytes_written != sizeof(nbi) )
                {
                    printf("Utility::GetNTPLwrrentTime:: Wrote %u bytes instead of %u\n",
                        bytes_written, (UINT32)sizeof(nbi));
                    close(socket_handle);
                    return OK;
                }
            }
        }
    }
    else
    {
        printf("Utility::GetNTPLwrrentTime:: Timed out waiting to write\n");
        close(socket_handle);
        return OK;
    }

    NtpFullPacket nfp;

    memset(&nfp, 0, sizeof(NtpFullPacket));

    fd_set readfds;
    int bytes_read = 0;

    FD_ZERO(&readfds);
    FD_SET(socket_handle, &readfds);
    timeout.tv_sec = 2;
    timeout.tv_usec = 0;

    sel_result = select((int)socket_handle + 1, &readfds, NULL, NULL, &timeout);

    if ( sel_result )
    {
        // received data or timed out
        if ( sel_result < 0 )
        {
            // error
            printf("Utility::GetNTPLwrrentTime: Error receiving: %d\n", errno);
            close(socket_handle);
            return OK;
        }
        else if ( FD_ISSET(socket_handle, &readfds) )
        {
            bytes_read = recv(socket_handle, (char*)&nfp, sizeof(NtpFullPacket), 0);

            if ( bytes_read == SOCKET_ERROR )
            {
                printf("Utility::GetNTPLwrrentTime: Error reading to socket\n");
                close(socket_handle);
                return OK;
            }
            else
            {
                if ( bytes_read == 0 )
                {
                    printf("Utility::GetNTPLwrrentTime:: Read 0 bytes\n");
                    close(socket_handle);
                    return OK;
                }
            }
        }
    }

    UINT32 lwrrent_time = htonl(nfp.basic.receiveTimestamp.dwInteger);
    float frac = (float)htonl(nfp.basic.receiveTimestamp.dwFractional) / (float)0xFFFFFFFF;

    secs = lwrrent_time;
    msecs = (UINT32)(frac * 1000);

    close(socket_handle);

    return OK;
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
    RC rc = OK;

#ifdef INCLUDE_GPU
    // We do not touch user interface on devices we did not enumerate
    GpuDevMgr* pGpuDevMgr = (GpuDevMgr*)DevMgrMgr::d_GraphDevMgr;
    if (!pGpuDevMgr || !pGpuDevMgr->GetPrimaryDevice())
    {
        Printf(Tee::PriLow, "Skipping DisableUserInterface.\n");
        return OK;
    }

    if (DevMgrMgr::d_GraphDevMgr)
    {
        // Sim mods, i.e. client-side resman.
        // We can call right into RmDisableVga (rmdos.c) to enable RM's ISR and
        // take ownership of the gpus.
        // Note that we start up in !vga mode in linux-sim anyway, so this is
        // is probably harmless but unnecessary.

        if (Platform::GetSimulationMode() == Platform::Hardware)
        {
            for (GpuDevice * pGpuDev = pGpuDevMgr->GetFirstGpuDevice();
                    pGpuDev != NULL;
                            pGpuDev = pGpuDevMgr->GetNextGpuDevice(pGpuDev))
            {
                RmDisableVga(pGpuDev->GetDeviceInst());
            }
        }
    }
#endif // INCLUDE_GPU

    return rc;
}

//------------------------------------------------------------------------------
// Enable the user interface.
//
RC Xp::EnableUserInterface()
{
    RC rc = OK;

#ifdef INCLUDE_GPU
    // We do not touch user interface on devices we did not enumerate
    GpuDevMgr* pGpuDevMgr = (GpuDevMgr*)DevMgrMgr::d_GraphDevMgr;
    if (!pGpuDevMgr || !pGpuDevMgr->GetPrimaryDevice() ||
            Platform::GetSimulationMode() != Platform::Hardware)
    {
        Printf(Tee::PriLow, "Skipping EnableUserInterface.\n");
        return OK;
    }

    if (DevMgrMgr::d_GraphDevMgr)
    {
        for (GpuDevice * pGpuDev = pGpuDevMgr->GetFirstGpuDevice();
                pGpuDev != NULL;
                pGpuDev = pGpuDevMgr->GetNextGpuDevice(pGpuDev))
        {
            for (UINT32 si = 0; si < pGpuDev->GetNumSubdevices(); si++)
            {
                GpuSubdevice * pSubdev = pGpuDev->GetSubdevice(si);
                if (pSubdev && pSubdev->IsPrimary())
                {
                    // This is required to avoid VGA bundle timeout error we see in Volta.
                    // It was marked invalid during initialization of display channels
                    // in LwDisplay::InitializeDisplayHW
                    if (pSubdev->HasBug(1923924))
                    {
                        pSubdev->Regs().Write32(MODS_PDISP_VGA_BASE_STATUS_VALID);
                    }
                }
            }
        }
    }
#endif // INCLUDE_GPU

    return rc;
}

//------------------------------------------------------------------------------
bool Xp::HasFeature(Feature feat)
{
    switch (feat)
    {
        case HAS_KERNEL_LEVEL_ACCESS:
            return true;
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

//------------------------------------------------------------------------------
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
