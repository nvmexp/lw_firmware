/*
* LWIDIA_COPYRIGHT_BEGIN
*
* Copyright 2018-2020 by LWPU Corporation.  All rights reserved.  All
* information contained herein is proprietary and confidential to LWPU
* Corporation.  Any use, reproduction, or disclosure without the written
* permission of LWPU Corporation is prohibited.
*
* LWIDIA_COPYRIGHT_END
*/

#include "core/include/xp.h"

#ifdef LIN_DA
#define WE_NEED_LINUX_XP
#include "xp_linux_internal.h"

void* Xp::AllocOsEvent(UINT32 hClient, UINT32 hDevice)
{
    return nullptr;
}

void Xp::FreeOsEvent(void* pFd, UINT32 hClient, UINT32 hDevice)
{
}

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

void Xp::SetOsEvent(void* pFd)
{
}

Xp::OperatingSystem Xp::GetOperatingSystem()
{
    return OS_LINDA;
}

Xp::ClockManager* Xp::GetClockManager()
{
    return nullptr;
}

RC Xp::CallACPIGeneric
(
	UINT32 GpuInst,
    UINT32 Function,
    void *Param1,
    void *Param2,
    void *Param3
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
    return RC::OK;
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

RC Xp::InitWatchdogs(UINT32 TimeoutSecs)
{
    return RC::OK;
}

RC Xp::ResetWatchdogs()
{
    return RC::OK;
}

RC Xp::SetVesaMode
(
    UINT32 Mode,
    bool   Windowed,
    bool   ClearMemory
)
{
    return RC::SET_MODE_FAILED;
}

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

RC Xp::DisableUserInterface()
{
    return RC::OK;
}

RC Xp::EnableUserInterface()
{
    return RC::OK;
}

RC Xp::LinuxInternal::OpenDriver(const vector<string> &args)
{
    return RC::OK;
}

RC Xp::LinuxInternal::CloseModsDriver()
{
    return RC::OK;
}

RC Xp::LinuxInternal::AcpiDsmEval
(
    UINT16 domain,
    UINT16 bus,
    UINT08 dev,
    UINT08 func,
    UINT32 Function,
    UINT32 DSMMXMSubfunction,
    UINT32 *pInOut,
    UINT16 *size
)
{
    return RC::SOFTWARE_ERROR;
}

RC Xp::LinuxInternal::AcpiRootPortEval
(
    UINT16 domain,
    UINT16 bus,
    UINT08 device,
    UINT08 function,
    Xp::LinuxInternal::AcpiPowerMethod method,
    UINT32 *pStatus
)
{
    return RC::SOFTWARE_ERROR;
}

Xp::OptimusMgr* Xp::GetOptimusMgr(UINT32 domain, UINT32 bus, UINT32 device, UINT32 function)
{
    return nullptr;
}

RC Xp::GetNTPTime
(
   UINT32 &secs,
   UINT32 &msecs,
   const char *hostname
)
{
    return RC::OK;
}

RC Xp::GetFbConsoleInfo(PHYSADDR *pBaseAddress, UINT64 *pSize)
{
    return RC::UNSUPPORTED_FUNCTION;
}

void Xp::SuspendConsole(GpuSubdevice *pGpuSubdevice)
{
}

void Xp::ResumeConsole(GpuSubdevice *pGpuSubdevice)
{
}

RC Xp::GetOSVersion(UINT64* pVer)
{
    return RC::UNSUPPORTED_FUNCTION;
}

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

RC Xp::SetDmaMask(UINT32 domain, UINT32 bus, UINT32 device, UINT32 func, UINT32 numBits)
{
    return RC::UNSUPPORTED_FUNCTION;
}

RC Xp::ValidateGpuInterrupt
(
    GpuSubdevice *pGpuSubdevice,
    UINT32 whichInterrupts
)
{
    return RC::OK;
}

RC Xp::IommuDmaMapMemory(void  *pDescriptor, string DevName, PHYSADDR *pIova)
{
    return RC::UNSUPPORTED_FUNCTION;
}

RC Xp::IommuDmaUnmapMemory(void  *pDescriptor, string DevName)
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

#endif // LIN_DA
