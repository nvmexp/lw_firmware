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

#include "core/include/cpu.h"
#include "core/include/jscript.h"
#include "core/include/platform.h"
#include "core/include/xp.h"
#include "core/platform/pltfmpvt.h"
#include "opengl/modsgl.h"

struct __GLLWChipInfo;
static const __GLLWChipInfo* s_AmodelChipInfo = nullptr;

static const char *EXTRA_AMODEL_LOGGING_KNOBS[] =
{
    "Logging::toFile 1 amodel.log",
    nullptr
};

const __GLLWChipInfo* GetAmodelChipInfo()
{
    return s_AmodelChipInfo;
}

RC Platform::Initialize()
{
    RC rc;
    JavaScriptPtr pJs;
    const char *const *extraAmodelKnobs[] = {EXTRA_AMODEL_LOGGING_KNOBS, nullptr};

    dglDirectAmodelSetExtraChipKnobs(extraAmodelKnobs);

    CHECK_RC(dglProcessAttach());

    s_AmodelChipInfo = static_cast<const __GLLWChipInfo *>(dglDirectAmodelGetChipInfo());
    if (s_AmodelChipInfo == nullptr)
    {
        Printf(Tee::PriError,
            "Amodel start failure. Check if \"OGL_ChipName\" elw variable is set correctly.\n");
        return RC::DLL_LOAD_FAILED;
    }

    return rc;
}

void Platform::Cleanup(CleanupMode Mode)
{
    if (Mode == MininumCleanup)
    {
        return;
    }

    CallAndClearCleanupFunctions();
}

RC Platform::Reset()
{
    return RC::UNSUPPORTED_FUNCTION;
}

bool Platform::IsInitialized()
{
    return true;
}

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
    return RC::UNSUPPORTED_FUNCTION;
}

bool Platform::IsTegra()
{
    return false;
}

RC Platform::FindPciDevice
(
    INT32   DeviceId,
    INT32   VendorId,
    INT32   Index,
    UINT32* pDomainNumber,
    UINT32* pBusNumber,
    UINT32* pDeviceNumber,
    UINT32* pFunctionNumber
)
{
    return RC::UNSUPPORTED_FUNCTION;
}

RC Platform::FindPciClassCode
(
    INT32   ClassCode,
    INT32   Index,
    UINT32* pDomainNumber,
    UINT32* pBusNumber,
    UINT32* pDeviceNumber,
    UINT32* pFunctionNumber
)
{
    return RC::UNSUPPORTED_FUNCTION;
}

RC Platform::RescanPciBus
(
    INT32 domain,
    INT32 bus
)
{
    return RC::UNSUPPORTED_FUNCTION;
}

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
    return *static_cast<const volatile UINT08 *>(Addr);
}

UINT16 Platform::VirtualRd16(const volatile void *Addr)
{
    return *static_cast<const volatile UINT16 *>(Addr);
}

UINT32 Platform::VirtualRd32(const volatile void *Addr)
{
    return *static_cast<const volatile UINT32 *>(Addr);
}

UINT64 Platform::VirtualRd64(const volatile void *Addr)
{
    return *static_cast<const volatile UINT64 *>(Addr);
}

void Platform::VirtualWr08(volatile void *Addr, UINT08 Data)
{
    *static_cast<volatile UINT08 *>(Addr) = Data;
}

void Platform::VirtualWr16(volatile void *Addr, UINT16 Data)
{
    *static_cast<volatile UINT16 *>(Addr) = Data;
}

void Platform::VirtualWr32(volatile void *Addr, UINT32 Data)
{
    *static_cast<volatile UINT32 *>(Addr) = Data;
}

void Platform::VirtualWr64(volatile void *Addr, UINT64 Data)
{
    *static_cast<volatile UINT64 *>(Addr) = Data;
}

UINT32 Platform::VirtualXchg32(volatile void *Addr, UINT32 Data)
{
    return Cpu::AtomicXchg32(static_cast<volatile UINT32 *>(Addr), Data);
}

RC Platform::PhysRd(PHYSADDR Addr, void *Data, UINT32 Count)
{
    return RC::UNSUPPORTED_FUNCTION;
}

RC Platform::PhysWr(PHYSADDR Addr, const void *Data, UINT32 Count)
{
    return RC::UNSUPPORTED_FUNCTION;
}

UINT08 Platform::PhysRd08(PHYSADDR Addr)
{
    return 0xFFU;
}

UINT16 Platform::PhysRd16(PHYSADDR Addr)
{
    return 0xFFFFU;
}

UINT32 Platform::PhysRd32(PHYSADDR Addr)
{
    return ~0U;
}

UINT64 Platform::PhysRd64(PHYSADDR Addr)
{
    return ~static_cast<UINT64>(0U);
}

void Platform::PhysWr08(PHYSADDR Addr, UINT08 Data)
{
}

void Platform::PhysWr16(PHYSADDR Addr, UINT16 Data)
{
}

void Platform::PhysWr32(PHYSADDR Addr, UINT32 Data)
{
}

void Platform::PhysWr64(PHYSADDR Addr, UINT64 Data)
{
}

UINT32 Platform::PhysXchg32(PHYSADDR Addr, UINT32 Data)
{
    return ~0U;
}

void Platform::MemCopy(volatile void *Dst, const volatile void *Src, size_t Count)
{
    if (GetUse4ByteMemCopy())
        Platform::Hw4ByteMemCopy(Dst, Src, Count);
    else
        Cpu::MemCopy(const_cast<void *>(Dst), const_cast<const void *>(Src), Count);
}

void Platform::MemCopy4Byte(volatile void *Dst, const volatile void *Src, size_t Count)
{
    Platform::Hw4ByteMemCopy(Dst, Src, Count);
}

void Platform::MemSet(volatile void *Dst, UINT08 Data, size_t Count)
{
    Cpu::MemSet(const_cast<void *>(Dst), Data, Count);
}

size_t Platform::GetPageSize()
{
    return Xp::GetPageSize();
}

void Platform::DumpPageTable()
{
}

RC Platform::TestAllocPages
(
    size_t      NumBytes
)
{
    MASSERT(0);
    return RC::UNSUPPORTED_FUNCTION;
}

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
    return RC::UNSUPPORTED_FUNCTION;
}

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
    return RC::UNSUPPORTED_FUNCTION;
}

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
    return RC::UNSUPPORTED_FUNCTION;
}

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

void Platform::FreePages(void * Descriptor)
{
}

RC Platform::MergePages(void** pOutDescriptor, void** pInDescBegin, void** pInDescEnd)
{
    MASSERT(0);
    return RC::UNSUPPORTED_FUNCTION;
}

RC Platform::MapPages
(
    void **pReturnedVirtualAddress,
    void * Descriptor,
    size_t Offset,
    size_t Size,
    Memory::Protect Protect
)
{
    return RC::UNSUPPORTED_FUNCTION;
}

void Platform::UnMapPages(void * VirtualAddress)
{
}

PHYSADDR Platform::GetPhysicalAddress(void *Descriptor, size_t Offset)
{
    return ~static_cast<PHYSADDR>(0U);
}

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
    return RC::UNSUPPORTED_FUNCTION;
}

RC Platform::DmaMapMemory
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

RC Platform::DmaUnmapMemory
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

void Platform::ReservePages
(
    size_t NumBytes,
    Memory::Attrib Attrib
)
{
}

RC Platform::MapDeviceMemory
(
    void **     pReturnedVirtualAddress,
    PHYSADDR    PhysicalAddress,
    size_t      NumBytes,
    Memory::Attrib Attrib,
    Memory::Protect Protect
)
{
    return RC::UNSUPPORTED_FUNCTION;
}

void Platform::UnMapDeviceMemory(void * VirtualAddress)
{
}

RC Platform::SetMemRange
(
    PHYSADDR    PhysicalAddress,
    size_t      NumBytes,
    Memory::Attrib Attrib
)
{
    return RC::UNSUPPORTED_FUNCTION;
}

RC Platform::UnSetMemRange
(
    PHYSADDR    PhysicalAddress,
    size_t      NumBytes
)
{
    return RC::UNSUPPORTED_FUNCTION;
}

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
    return RC::UNSUPPORTED_FUNCTION;
}

RC Platform::GetLwLinkLineRate
(
    UINT32 domain,
    UINT32 bus,
    UINT32 device,
    UINT32 func,
    UINT32 *pSpeed
)
{
    return RC::UNSUPPORTED_FUNCTION;
}

void * Platform::PhysicalToVirtual(PHYSADDR PhysicalAddress, Tee::Priority pri)
{
    return reinterpret_cast<void*>(~static_cast<size_t>(0U));
}

PHYSADDR Platform::VirtualToPhysical(volatile void * VirtualAddress, Tee::Priority pri)
{
    return ~static_cast<PHYSADDR>(0U);
}

UINT32 Platform::VirtualToPhysical32(volatile void * VirtualAddress)
{
    return ~0U;
}

RC Platform::GetCarveout(PHYSADDR* pPhysAddr, UINT64* pSize)
{
    return RC::UNSUPPORTED_FUNCTION;
}

RC Platform::GetVPR(PHYSADDR* pPhysAddr, UINT64* pSize, void **pMemDesc)
{
    return RC::UNSUPPORTED_FUNCTION;
}

RC Platform::GetGenCarveout(PHYSADDR *pPhysAddr, UINT64 *pSize, UINT32 id, UINT64 align)
{
    return RC::UNSUPPORTED_FUNCTION;
}

RC Platform::CallACPIGeneric
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
    return RC::UNSUPPORTED_FUNCTION;
}

RC Platform::IlwalidateCpuCache()
{
    return RC::OK;
}

RC Platform::FlushCpuCache()
{
    return RC::OK;
}

RC Platform::FlushCpuCacheRange
(
    void * StartAddress,
    void * EndAddress,
    UINT32 Flags
)
{
    return RC::OK;
}

RC Platform::FlushCpuWriteCombineBuffer()
{
    return RC::OK;
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
    return RC::UNSUPPORTED_FUNCTION;
}

RC Platform::SetLwLinkSysmemTrained
(
    UINT32 domain,
    UINT32 bus,
    UINT32 device,
    UINT32 func,
    bool trained
)
{
    return RC::UNSUPPORTED_FUNCTION;
}

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
    return RC::UNSUPPORTED_FUNCTION;
}

RC Platform::PciEnableSriov
(
    UINT32 domain,
    UINT32 bus,
    UINT32 device,
    UINT32 function,
    UINT32 vfCount
)
{
    return RC::UNSUPPORTED_FUNCTION;
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
    return RC::UNSUPPORTED_FUNCTION;
}

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
    return RC::UNSUPPORTED_FUNCTION;
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
    return RC::UNSUPPORTED_FUNCTION;
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
    return RC::UNSUPPORTED_FUNCTION;
}

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
    return RC::UNSUPPORTED_FUNCTION;
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
    return RC::UNSUPPORTED_FUNCTION;
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
    return RC::UNSUPPORTED_FUNCTION;
}

RC Platform::PciRemove
(
   INT32    domainNumber,
   INT32    busNumber,
   INT32    deviceNumber,
   INT32    functionNumber
)
{
    return RC::UNSUPPORTED_FUNCTION;
}

RC Platform::PciResetFunction
(
   INT32    domainNumber,
   INT32    busNumber,
   INT32    deviceNumber,
   INT32    functionNumber
)
{
    return RC::UNSUPPORTED_FUNCTION;
}

RC Platform::PioRead08(UINT16 Port, UINT08 * pData)
{
    return RC::UNSUPPORTED_FUNCTION;
}

RC Platform::PioRead16(UINT16 Port, UINT16 * pData)
{
    return RC::UNSUPPORTED_FUNCTION;
}

RC Platform::PioRead32(UINT16 Port, UINT32 * pData)
{
    return RC::UNSUPPORTED_FUNCTION;
}

RC Platform::PioWrite08(UINT16 Port, UINT08 Data)
{
    return RC::UNSUPPORTED_FUNCTION;
}

RC Platform::PioWrite16(UINT16 Port, UINT16 Data)
{
    return RC::UNSUPPORTED_FUNCTION;
}

RC Platform::PioWrite32(UINT16 Port, UINT32 Data)
{
    return RC::UNSUPPORTED_FUNCTION;
}

void Platform::HandleInterrupt(UINT32 Irq)
{
}

RC Platform::HookIRQ(UINT32 Irq, ISR func, void* cookie)
{
    return RC::UNSUPPORTED_FUNCTION;
}

RC Platform::MapIRQ(UINT32 * logicalIrq, UINT32 physIrq, string dtName, string fullName)
{
    return RC::UNSUPPORTED_FUNCTION;
}

RC Platform::UnhookIRQ(UINT32 Irq, ISR func, void* cookie)
{
    return RC::UNSUPPORTED_FUNCTION;
}

RC Platform::DisableIRQ(UINT32 Irq)
{
    return RC::UNSUPPORTED_FUNCTION;
}

RC Platform::EnableIRQ(UINT32 Irq)
{
    return RC::UNSUPPORTED_FUNCTION;
}

bool Platform::IsIRQHooked(UINT32 irq)
{
    return false;
}

bool Platform::IRQTypeSupported(UINT32 irqType)
{
    return false;
}

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
    return RC::UNSUPPORTED_FUNCTION;
}

void Platform::FreeIRQs(UINT32 pciDomain, UINT32 pciBus, UINT32 pciDevice, UINT32 pciFunction)
{
}

void Platform::InterruptProcessed()
{
}

void Platform::ProcessPendingInterrupts()
{
}

RC Platform::HookInt(const IrqParams& irqInfo, ISR func, void* cookie)
{
    return RC::UNSUPPORTED_FUNCTION;
}

RC Platform::UnhookInt(const IrqParams& irqInfo, ISR func, void* cookie)
{
    return RC::UNSUPPORTED_FUNCTION;
}

void Platform::EnableInterrupts()
{
}

void Platform::DisableInterrupts()
{
}

Platform::Irql Platform::GetLwrrentIrql()
{
    return NormalIrql;
}

Platform::Irql Platform::RaiseIrql(Platform::Irql NewIrql)
{
    return NormalIrql;
}

void Platform::LowerIrql(Platform::Irql NewIrql)
{
}

void Platform::Pause()
{
    Cpu::Pause();
}

void Platform::Delay(UINT32 Microseconds)
{
    Utility::Delay(Microseconds);
}

void Platform::DelayNS(UINT32 Nanoseconds)
{
    Utility::DelayNS(Nanoseconds);
}

void Platform::SleepUS(UINT32 MinMicrosecondsToSleep)
{
    Utility::SleepUS(MinMicrosecondsToSleep);
}

UINT64 Platform::GetTimeNS()
{
    return Xp::GetWallTimeNS();
}

UINT64 Platform::GetTimeUS()
{
    return Xp::GetWallTimeUS();
}

UINT64 Platform::GetTimeMS()
{
    return Xp::GetWallTimeMS();
}

bool Platform::LoadSimSymbols()
{
    return false;
}

bool Platform::IsChiplibLoaded()
{
    return false;
}

void Platform::ClockSimulator(int Cycles)
{
}

UINT64 Platform::GetSimulatorTime()
{
    return 0;
}

double Platform::GetSimulatorTimeUnitsNS()
{
    return 0;
}

Platform::SimulationMode Platform::GetSimulationMode()
{
    return Amodel;
}

bool Platform::ExtMemAllocSupported()
{
    return false;
}

int Platform::EscapeWrite(const char *Path, UINT32 Index, UINT32 Size, UINT32 Value)
{
    return 0;
}

int Platform::EscapeRead(const char *Path, UINT32 Index, UINT32 Size, UINT32 *Value)
{
    return 0;
}

int Platform::GpuEscapeWrite
(
    UINT32 GpuId,
    const char *Path,
    UINT32 Index,
    UINT32 Size,
    UINT32 Value
)
{
    return 0;
}

int Platform::GpuEscapeRead
(
    UINT32 GpuId,
    const char *Path,
    UINT32 Index,
    UINT32 Size,
    UINT32 *Value
)
{
    return 0;
}

int Platform::GpuEscapeWriteBuffer
(
    UINT32 GpuId,
    const char *Path,
    UINT32 Index,
    size_t Size,
    const void *Buf
)
{
    return 0;
}

int Platform::GpuEscapeReadBuffer
(
    UINT32 GpuId,
    const char *Path,
    UINT32 Index,
    size_t Size,
    void *Buf
)
{
    return 0;
}

RC Platform::VerifyGpuId
(
    UINT32 pciDomainAddress,
    UINT32 pciBusAddress,
    UINT32 pciDeviceAddress,
    UINT32 pciFunctionAddress,
    UINT32 resmanGpuId
)
{
    return RC::OK;
}

void Platform::AmodAllocChannelDma
(
    UINT32 ChID,
    UINT32 Class,
    UINT32 CtxDma,
    UINT32 ErrorNotifierCtxDma
)
{
}

void Platform::AmodAllocChannelGpFifo
(
    UINT32 ChID,
    UINT32 Class,
    UINT32 CtxDma,
    UINT64 GpFifoOffset,
    UINT32 GpFifoEntries,
    UINT32 ErrorNotifierCtxDma
)
{
}

void Platform::AmodFreeChannel(UINT32 ChID)
{
}

void Platform::AmodAllocContextDma
(
    UINT32 ChID,
    UINT32 Handle,
    UINT32 Class,
    UINT32 Target,
    UINT32 Limit,
    UINT32 Base,
    UINT32 Protect
)
{
}

void Platform::AmodAllocObject(UINT32 ChID, UINT32 Handle, UINT32 Class)
{
}

void Platform::AmodFreeObject(UINT32 ChID, UINT32 Handle)
{
}

const char* Platform::ModelIdentifier()
{
    return "a";
}

void Platform::DumpAddrData
(
    const volatile void* Addr,
    const void* Data,
    UINT32 Count,
    const char* Prefix
)
{
}

void Platform::DumpAddrData
(
    const PHYSADDR Addr,
    const void* Data,
    UINT32 Count,
    const char* Prefix
)
{
}

void Platform::DumpCfgAccessData
(
    INT32 DomainNumber,
    INT32 BusNumber,
    INT32 DeviceNumber,
    INT32 FunctionNumber,
    INT32 Address,
    const void* Data,
    UINT32 Count,
    const char* Prefix
)
{
}

void Platform::DumpEscapeData
(
    UINT32 GpuId,
    const char *Path,
    UINT32 Index,
    size_t Size,
    const void* Buf,
    const char* Prefix
)
{
}

bool Platform::PollfuncWrap(PollFunc pollFunc, void *pArgs, const char* pollFuncName)
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

RC Platform::VirtualAlloc
(
    void **ppReturnedAddr,
    void *pBase,
    size_t size,
    UINT32 protect,
    UINT32 vaFlags
)
{
    return RC::UNSUPPORTED_FUNCTION;
}

void *Platform::VirtualFindFreeVaInRange(size_t size, void *pBase, void *pLimit, size_t align)
{
    return reinterpret_cast<void*>(~static_cast<size_t>(0U));
}

RC Platform::VirtualFree(void *pAddr, size_t size, Memory::VirtualFreeMethod vfMethod)
{
    return RC::UNSUPPORTED_FUNCTION;
}

RC Platform::VirtualProtect(void *pAddr, size_t size, UINT32 protect)
{
    return RC::UNSUPPORTED_FUNCTION;
}

RC Platform::VirtualMadvise(void *pAddr, size_t size, Memory::VirtualAdvice advice)
{
    return RC::UNSUPPORTED_FUNCTION;
}

bool Platform::IsVirtFunMode()
{
    return false;
}

bool Platform::IsPhysFunMode()
{
    return false;
}

bool Platform::IsDefaultMode()
{
    return !IsVirtFunMode() && !IsPhysFunMode();
}

bool Platform::IsSelfHosted()
{
    return false;
}

RC Platform::SetDmaMask(UINT32 domain, UINT32 bus, UINT32 device, UINT32 func, UINT32 numBits)
{
    return RC::UNSUPPORTED_FUNCTION;
}

bool Platform::ManualCachingEnabled()
{
    return false;
}
