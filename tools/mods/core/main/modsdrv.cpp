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

// MODS Device Driver (ModsDrv) interface -- a C API that other drivers that use
// MODS can call into.

//#define DO_STACK_CHECKS 1
#define DO_LEAK_CHECK 1

#include "core/include/cmdline.h"
#include "core/include/cpu.h"
#include "core/include/errormap.h"
#include "core/include/globals.h"
#include "core/include/gpu.h"
#include "core/include/jscript.h"
#include "core/include/log.h"
#include "core/include/memcheck.h"
#include "core/include/mle_protobuf.h"
#include "core/include/modsdrv.h"
#include "core/include/lwrm.h"
#include "core/include/platform.h"
#include "core/include/pool.h"
#include "core/include/registry.h"
#include "core/include/script.h"
#include "core/include/tar.h"
#include "core/include/tasker.h"
#include "core/include/tee.h"
#include "core/include/utility.h"
#include "core/include/xp.h"
#include "core/include/modsdrv_defines.h"
#include "core/utility/errloggr.h"
#include "core/utility/sharedsysmem.h"
#include "cheetah/include/sysutil.h"
#include "cheetah/include/tegchip.h"
#include "cheetah/include/tegclk.h"
#include "cheetah/include/tegrasocdevice.h"
#include "cheetah/include/tegrasocdeviceptr.h"

#if defined(INCLUDE_GPU)
#include "core/include/mgrmgr.h"
#include "device/interface/pcie.h"
#include "gpu/include/gpumgr.h"
#include "gpu/include/protmanager.h"
#include "gpu/include/testdevicemgr.h"
#include "gpu/utility/fakeproc.h"
#include "gpu/vmiop/vmiopelw.h"
#include "gpu/vmiop/vmiopelwmgr.h"
#endif

#include <string.h>

/* windows.h defines Yield macro which conflicts with our function
* Theoretically we should not be including windows.h anywhere so the next step
* is to figure out where that happens.
*/

#if defined(Yield)
 #undef Yield
#endif
#if defined(MODS_EXPORT)
 #undef MODS_EXPORT
#endif

#if defined(__GNUC__) && (__GNUC__ >= 4) && (__GNUC_MINOR__ >= 6)
#define MODS_EXPORT __attribute__((externally_visible))
#else
#define MODS_EXPORT
#endif
//------------------------------------------------------------------------------
// Load the Dynamically Linked Library or Dynamic System Obeject (DS0) and return an opaque handle
// for the dynamic library. If unsuccessful nullptr will be returned.
// By default the library is opened with the RTLD_NOW flags. Additional flags can be included
// using the following Xp::LoadDLLFlags.
// Xp flags                 dlopen flags
// ---------------------    ----------------------------------------------------------------------
// Xp::NoDLLFlags           none
// Xp::GlobalDLL            RTLD_GLOBAL
// Xp::DeepBindDLL          RTLD_DEEPBIND
// Xp::UnloadDLLOnExit      none, causes the module to be registered with the dllHelper so it can
//                          be automatically unloaded on program exit. If this flag is not used
//                          it is the responsibility of the caller to close the DLL.
MODS_EXPORT void * ModsDrvLoadLibrary(const char* filename, int flags)
{
    Printf(Tee::PriLow, "ModsDrvLoadLibrary %s\n", filename);

    void * pFileHandle = nullptr;
    if (RC::OK != Utility::LoadDLLFromPackage(filename, &pFileHandle, flags))
    {
        pFileHandle = nullptr;
    }

    return pFileHandle;
}

//------------------------------------------------------------------------------
#if defined(INCLUDE_LWDA)
#include "lwca.h"
#endif
MODS_EXPORT void ModsDrvLwGetExportTable(const void **ppExportTable, const void *pExportTableId)
{
#if defined(INCLUDE_LWDA)
    lwGetExportTable(ppExportTable, (const LWuuid *)pExportTableId);
#endif
}

//------------------------------------------------------------------------------
MODS_EXPORT void ModsDrvInterpretModsRC(MODS_RC rc)
{
    ErrorMap em(rc);

    ModsDrvPrintf(Tee::PriNormal, "%s\n", em.Message());
}

///-----------------------------------------------------------------------------
// Debugging functions
//
MODS_EXPORT void ModsDrvBreakPointWithErrorInfo
(
    int errorCategory,
    const char *file,
    int line,
    ModsDrvErrorInfo *pErrorInfo
)
{
    MASSERT(ErrorMap::DevInst() >= 0);
    RC modsRc = RmApiStatusToModsRC(errorCategory);
    switch (pErrorInfo->type)
    {
        case MODSDRV_LWLINK_ERROR_TYPE:
        {
            const ModsDrvLwlinkErrorInfo& lwlinkInfo = pErrorInfo->lwlinkInfo;

            Mle::LwlinkErrorInfo::SubCode subcode = Mle::LwlinkErrorInfo::not_an_error;
            switch (lwlinkInfo.subCode)
            {
                case MODSDRV_INITPHY_ERROR:           subcode = Mle::LwlinkErrorInfo::initphy_error; break;
                case MODSDRV_RXCAL_ERROR:             subcode = Mle::LwlinkErrorInfo::rxcal_error; break;
                case MODSDRV_INITDLPL_ERROR:          subcode = Mle::LwlinkErrorInfo::initdlpl_error; break;
                case MODSDRV_INITLANEENABLE_ERROR:    subcode = Mle::LwlinkErrorInfo::initlaneenable_error; break;
                case MODSDRV_SAFE_TRANSITION_ERROR:   subcode = Mle::LwlinkErrorInfo::safe_transition_error; break;
                case MODSDRV_MINION_BOOT_ERROR:       subcode = Mle::LwlinkErrorInfo::minion_boot_error; break;
                case MODSDRV_DLCMD_TIMEOUT_ERROR:     subcode = Mle::LwlinkErrorInfo::dlcmd_timeout_error; break;
                case MODSDRV_INITPLL_ERROR:           subcode = Mle::LwlinkErrorInfo::initpll_error; break;
                case MODSDRV_TXCLKSWITCH_PLL_ERROR:   subcode = Mle::LwlinkErrorInfo::txclkswitch_pll_error; break;
                case MODSDRV_INITOPTIMIZE_ERROR:      subcode = Mle::LwlinkErrorInfo::initoptimize_error; break;
                case MODSDRV_ACTIVE_TRANSITION_ERROR: subcode = Mle::LwlinkErrorInfo::active_transition_error; break;
                case MODSDRV_INITTL_ERROR:            subcode = Mle::LwlinkErrorInfo::inittl_error; break;
                case MODSDRV_TL_TRANSITION_ERROR:     subcode = Mle::LwlinkErrorInfo::tl_transition_error; break;
                case MODSDRV_INITPHASE1_ERROR:        subcode = Mle::LwlinkErrorInfo::initphase1_error; break;
                case MODSDRV_INITNEGOTIATE_ERROR:     subcode = Mle::LwlinkErrorInfo::initnegotiate_error; break;
                case MODSDRV_INITPHASE5_ERROR:        subcode = Mle::LwlinkErrorInfo::initphase5_error; break;
                default:
                    MASSERT(!"Unknown LwlinkErrorSubCode colwersion");
                    break;
            }

            Mle::Print(Mle::Entry::lwlink_error_info)
                .lwlink_mask(lwlinkInfo.lwlinkMask)
                .sub_code(subcode)
                .rc(modsRc);
            ErrorMap::PrintRcDesc(modsRc);

            Printf(Tee::PriError, "Lwlink Error on Device %d: Lwlink Mask %llu, SubCode %d\n",
                    ErrorMap::DevInst(), lwlinkInfo.lwlinkMask, lwlinkInfo.subCode);
            break;
        }
    }

    Platform::BreakPoint(modsRc, file, line);
}

MODS_EXPORT void ModsDrvBreakPoint(const char *file, int line)
{
    Platform::BreakPoint(RC::ASSERT_FAILURE_DETECTED, file, line);
}

MODS_EXPORT void ModsDrvBreakPointWithReason
(
    int reason,
    const char *file,
    int line
)
{
    Platform::BreakPoint(RmApiStatusToModsRC(reason), file, line);
}

MODS_EXPORT void ModsDrvPrintString
(
    int Priority,
    const char * str
)
{
    ModsDrvPrintf((Tee::Priority)Priority, "%s", str);
}

MODS_EXPORT void ModsDrvIsrPrintString
(
    int Priority,
    const char * str
)
{
    // Inside a real ISR, we have to be careful; but on simulation, an ISR is
    // nothing special
    ModsDrvPrintf((Tee::Priority)Priority, "%s", str);
}

MODS_EXPORT void ModsDrvFlushLogFile(void)
{
    Tee::FlushToDisk();
}

static TeeModule s_moduleDrv("ModsDrv");

MODS_EXPORT int ModsDrvPrintf(int Priority, const char *Format, ...)
{
    int CharsWritten;

    va_list Arguments;
    va_start(Arguments, Format);

    CharsWritten = ModsExterlwAPrintf(Priority, s_moduleDrv.GetCode(),
        Tee::SPS_NORMAL, Format, Arguments);

    va_end(Arguments);

    return CharsWritten;
}

//------------------------------------------------------------------------------
MODS_EXPORT int ModsDrvVPrintf(int Priority, const char *Fmt, va_list Args)
{
    return ModsExterlwAPrintf(Priority, s_moduleDrv.GetCode(), Tee::SPS_NORMAL,
                              Fmt, Args);
}

//------------------------------------------------------------------------------
MODS_EXPORT int ModsDrvSetGpuId(int gpuInst)
{
#ifdef INCLUDE_GPU
    const UINT32 prevGpuId = DevMgrMgr::d_TestDeviceMgr->GetDriverId(ErrorMap::DevInst());
    const UINT32 devInst   = DevMgrMgr::d_GraphDevMgr->GetDevInstByGpuInst(gpuInst);
    ErrorMap::SetDevInst(devInst);
    return static_cast<int>(prevGpuId);
#else
    return -1;
#endif
}

//------------------------------------------------------------------------------
MODS_EXPORT UINT32 ModsDrvSetSwitchId(UINT32 switchInst)
{
#ifdef INCLUDE_GPU
    if (!DevMgrMgr::d_TestDeviceMgr)
    {
        // UTL initializes LwSwitch driver before TestDeviceMgr!
        return ~0U;
    }
    const UINT32 prevSwitchInst = DevMgrMgr::d_TestDeviceMgr->GetDriverId(ErrorMap::DevInst());
    const UINT32 devInst        = DevMgrMgr::d_TestDeviceMgr->GetDevInst(
                                        TestDevice::TYPE_LWIDIA_LWSWITCH, switchInst);
    ErrorMap::SetDevInst(devInst);
    return prevSwitchInst;
#else
    return ~0U;
#endif
}

//------------------------------------------------------------------------------
MODS_EXPORT void ModsDrvLogError(const char *String)
{
    ErrorLogger::LogError(String, ErrorLogger::LT_ERROR);
}

//------------------------------------------------------------------------------
MODS_EXPORT void ModsDrvLogInfo(const char *String)
{
    ErrorLogger::LogError(String, ErrorLogger::LT_INFO);
}

//------------------------------------------------------------------------------
MODS_EXPORT void ModsDrvFlushLog(void)
{
    ErrorLogger::FlushError();
}

///-----------------------------------------------------------------------------
// Mods Pooling memory allocator functions
//
MODS_EXPORT void * ModsDrvAlloc(size_t nbytes)
{
    void *p = Pool::Alloc(nbytes);
    if (!p)
        return NULL;
    POOL_ADDTRACE(p, nbytes, Pool::CID_EXTERN);
    // @@@ Workaround for bug 73273: r40 GL driver reads uninitialized memory.
    memset(p, 0xEE, nbytes);
    STACK_CHECK();
    return p;
}

MODS_EXPORT void * ModsDrvCalloc(size_t numElements, size_t nbytes)
{
    void *p = Pool::Alloc(numElements * nbytes);
    if (!p)
        return NULL;
    POOL_ADDTRACE(p, numElements * nbytes, Pool::CID_EXTERN);
    memset(p, 0, numElements * nbytes);
    STACK_CHECK();
    return p;
}

MODS_EXPORT void * ModsDrvRealloc(void * pOld, size_t nbytes)
{
    void *p = Pool::Realloc(pOld, nbytes, Pool::CID_EXTERN);
    STACK_CHECK();
    return p;
}

MODS_EXPORT void ModsDrvFree(void * p)
{
    STACK_CHECK();
    POOL_REMOVETRACE(p, Pool::CID_EXTERN);
    Pool::Free(p);
}

MODS_EXPORT void * ModsDrvAlignedMalloc
(
    UINT32 ByteSize,
    UINT32 AlignSize,
    UINT32 Offset
)
{
    void *p = Pool::AllocAligned(ByteSize, AlignSize, Offset);
    POOL_ADDTRACE(p, ByteSize, Pool::CID_EXTERN);
    return p;
}

MODS_EXPORT void ModsDrvAlignedFree(void * Addr)
{
    POOL_REMOVETRACE(Addr, Pool::CID_EXTERN);
    Pool::Free(Addr);
}

MODS_EXPORT void * ModsDrvExecMalloc(size_t NumBytes)
{
    return Xp::ExecMalloc(NumBytes);
}

MODS_EXPORT void ModsDrvExecFree(void * Address, size_t NumBytes)
{
    Xp::ExecFree(Address, NumBytes);
}

///-----------------------------------------------------------------------------
// Tasker functions
//

struct ModsDrvThreadParams
{
    void (*ThreadFuncPtr)(void*);
    void *Data;
};

namespace
{
    void DetachedModsDrvThread(void *params)
    {
        Tasker::DetachThread detach;

        unique_ptr<ModsDrvThreadParams> pThreadParams(static_cast<ModsDrvThreadParams*>(params));

#ifdef INCLUDE_GPU
        ErrorMap::SetDevInst(-1);
#endif

        pThreadParams->ThreadFuncPtr(pThreadParams->Data);
    }
}

MODS_EXPORT UINT32 ModsDrvCreateThread
(
    void (*ThreadFuncPtr)(void*),
    void* data,
    UINT32 StackSize,
    const char* Name
)
{
    ModsDrvThreadParams *params = new ModsDrvThreadParams;
    params->ThreadFuncPtr = ThreadFuncPtr;
    params->Data = data;

    Tasker::ThreadID tid = Tasker::CreateThread(DetachedModsDrvThread, params, StackSize, Name);

    if (tid == -1)
    {
        delete params;
        return -1;
    }

    // 1000 is to avoid GL's ID 0
    return 1000 + tid;
}

MODS_EXPORT void ModsDrvJoinThread(UINT32 ThreadID)
{
    Tasker::Join(ThreadID - 1000, Tasker::NO_TIMEOUT);
}

MODS_EXPORT UINT32 ModsDrvGetLwrrentThreadId(void)
{
    // GL uses thread ID 0 as a guard value.
    return 1000 + Tasker::LwrrentThread();
}

MODS_EXPORT void ModsDrvExitThread(void)
{
    Tasker::ExitThread();
}

MODS_EXPORT int ModsDrvYield(void)
{
    RC rc;

    if (GetCriticalEvent())
    {
        return RC::USER_ABORTED_SCRIPT;
    }

    FLOAT64 maxPollHz = Tasker::GetMaxHwPollHz();

    if (maxPollHz > 0.0)
    {
        // User specified -poll_hw_hz, delay returning to RM or GL long enough
        // to keep the maximum HW register polling frequency below the spec.
        UINT64 tmp = Platform::GetTimeNS() + UINT64(1e9/maxPollHz + 0.5);
        do
        {
            CHECK_RC(Tasker::Yield());
        }
        while (tmp > Platform::GetTimeNS());
    }
    else
    {
        CHECK_RC(Tasker::Yield());
    }
    return 0; // success
}

// Create a TLS variable
MODS_EXPORT MODS_BOOL ModsDrvTlsAlloc(unsigned long * idx)
{
    *idx = Tasker::TlsAlloc();
    return MODS_TRUE;
}

// Destroy a TLS variable
MODS_EXPORT void ModsDrvTlsFree(unsigned long idx)
{
    Tasker::TlsFree(idx);
}

// Get a TLS variable's value
MODS_EXPORT void * ModsDrvTlsGet(unsigned long tlsIndex)
{
    return Tasker::TlsGetValue(tlsIndex);
}

// Set a TLS variable's value
MODS_EXPORT void ModsDrvTlsSet(unsigned long tlsIndex, void* value)
{
    Tasker::TlsSetValue(tlsIndex, value);
}

// Create a mutex object
MODS_EXPORT void * ModsDrvAllocMutex(void)
{
    return Tasker::AllocMutex("ModsDrv", Tasker::mtxUnchecked);
}

// Destroy a mutex object
MODS_EXPORT void ModsDrvFreeMutex(void *hMutex)
{
    Tasker::FreeMutex(hMutex);
}

// Take possession of a mutex object
MODS_EXPORT void ModsDrvAcquireMutex(void *hMutex)
{
    Tasker::AcquireMutex(hMutex);
}

// Take possession of a mutex object
MODS_EXPORT MODS_BOOL ModsDrvTryAcquireMutex(void *hMutex)
{
    return Tasker::TryAcquireMutex(hMutex) ? MODS_TRUE : MODS_FALSE;
}

// Check the mutex object is possessed
MODS_EXPORT MODS_BOOL ModsDrvIsMutexOwner(void *hMutex)
{
    return Tasker::CheckMutexOwner(hMutex) ? MODS_TRUE : MODS_FALSE;
}

// Release possession of a mutex object
MODS_EXPORT void ModsDrvReleaseMutex(void *hMutex)
{
    Tasker::ReleaseMutex(hMutex);
}

MODS_EXPORT void * ModsDrvAllocCondition(void)
{
    void * c = new ModsCondition();
    return c;
}

MODS_EXPORT void ModsDrvFreeCondition(void *hCond)
{
    ModsCondition* c = static_cast<ModsCondition*>(hCond);
    delete c;
}

MODS_EXPORT MODS_BOOL ModsDrvWaitCondition(void *hCond, void *hMutex)
{
    if (RC::OK != static_cast<ModsCondition*>(hCond)->Wait(hMutex))
    {
        return MODS_FALSE;
    }

    return MODS_TRUE;
}

MODS_EXPORT MODS_BOOL ModsDrvWaitConditionTimeout(void *hCond, void *hMutex, FLOAT64 timeoutMs)
{
    if (RC::OK != static_cast<ModsCondition*>(hCond)->Wait(hMutex, timeoutMs))
    {
        return MODS_FALSE;
    }

    return MODS_TRUE;
}

MODS_EXPORT void ModsDrvSignalCondition(void *hCond)
{
    static_cast<ModsCondition*>(hCond)->Signal();
}

MODS_EXPORT void ModsDrvBroadcastCondition(void *hCond)
{
    static_cast<ModsCondition*>(hCond)->Broadcast();
}

// Create an event object
MODS_EXPORT void * ModsDrvAllocEvent(const char *Name)
{
    return Tasker::AllocEvent(Name);
}

// Create an event object
MODS_EXPORT void * ModsDrvAllocEventAutoReset(const char *Name)
{
    //auto trigger: set ModsEvent::m_ManualReset = false
    return Tasker::AllocEvent(Name, MODS_FALSE);
}

MODS_EXPORT void* ModsDrvAllocSystemEvent(const char* name)
{
    return Tasker::AllocSystemEvent(name);
}

// Destroy an event object
MODS_EXPORT void ModsDrvFreeEvent(void *hEvent)
{
    Tasker::FreeEvent((ModsEvent*)hEvent);
}

// Reset an event object
MODS_EXPORT void ModsDrvResetEvent(void *hEvent)
{
    Tasker::ResetEvent((ModsEvent*)hEvent);
}

// Set an event object, thereby waking up threads waiting on it
MODS_EXPORT void ModsDrvSetEvent(void *hEvent)
{
    Tasker::SetEvent((ModsEvent*)hEvent);
}

// Wait on an event object
MODS_EXPORT MODS_BOOL ModsDrvWaitOnEvent(void *EventHandle, FLOAT64 timeSec)
{
    const FLOAT64 timeMs = (timeSec == Tasker::NO_TIMEOUT)
                ? Tasker::NO_TIMEOUT : (timeSec * 1000);
    if (OK != Tasker::WaitOnEvent((ModsEvent*)EventHandle, timeMs))
    {
        return MODS_FALSE;
    }
    return MODS_TRUE;
}

MODS_EXPORT MODS_BOOL ModsDrvWaitOnMultipleEvents
(
    void**  hEvents,
    UINT32  numEvents,
    UINT32* pCompletedIndices,
    UINT32  maxCompleted,
    UINT32* pNumCompleted,
    FLOAT64 timeoutSec
)
{
    const FLOAT64 timeMs = (timeoutSec == Tasker::NO_TIMEOUT)
                ? Tasker::NO_TIMEOUT : (timeoutSec * 1000);
    const RC rc = Tasker::WaitOnMultipleEvents
                  (
                      reinterpret_cast<ModsEvent**>(hEvents),
                      numEvents,
                      pCompletedIndices,
                      maxCompleted,
                      pNumCompleted,
                      timeMs
                  );

    return rc == RC::OK ? MODS_TRUE : MODS_FALSE;
}

MODS_EXPORT void ModsDrvHandleResmanEvent(void *pOsEvent, UINT32 GpuInstance)
{
    LwRmPtr pLwRm;
    pLwRm->HandleResmanEvent(pOsEvent, GpuInstance);
}

MODS_EXPORT void* ModsDrvGetOsEvent(void* hEvent, UINT32 hClient, UINT32 hDevice)
{
    return Tasker::GetOsEvent(static_cast<ModsEvent*>(hEvent), hClient, hDevice);
}

MODS_EXPORT void* ModsDrvCreateSemaphore(UINT32 initCount, const char* name)
{
    return Tasker::CreateSemaphore(initCount, name);
}

MODS_EXPORT void ModsDrvDestroySemaphore(void *hSemaphore)
{
    Tasker::DestroySemaphore((SemaID)hSemaphore);
}

MODS_EXPORT MODS_BOOL ModsDrvAcquireSemaphore(void *hSemaphore, FLOAT64 timeSec)
{
    const FLOAT64 timeMs = (timeSec == Tasker::NO_TIMEOUT)
                ? Tasker::NO_TIMEOUT : (timeSec * 1000);
    if(Tasker::AcquireSemaphore((SemaID)hSemaphore, timeMs) == OK)
        return MODS_TRUE;
    else
        return MODS_FALSE;
}

MODS_EXPORT void ModsDrvReleaseSemaphore(void *hSemaphore)
{
    Tasker::ReleaseSemaphore((SemaID)hSemaphore);
}

MODS_EXPORT void *ModsDrvAllocSpinLock(void)
{
    return Tasker::AllocRmSpinLock();
}

MODS_EXPORT void ModsDrvDestroySpinLock(void *hSpinLock)
{
    Tasker::DestroyRmSpinLock((Tasker::SpinLockID)hSpinLock);
}

MODS_EXPORT void ModsDrvAcquireSpinLock(void *hSpinLock)
{
    Tasker::AcquireRmSpinLock((Tasker::SpinLockID)hSpinLock);
}

MODS_EXPORT void ModsDrvReleaseSpinLock(void *hSpinLock)
{
    Tasker::ReleaseRmSpinLock((Tasker::SpinLockID)hSpinLock);
}

///-----------------------------------------------------------------------------
// CPU atomic functions
//
//
MODS_EXPORT UINT32 ModsDrvAtomicRead(volatile UINT32* addr)
{
    return Cpu::AtomicRead(reinterpret_cast<volatile INT32*>(addr));
}

MODS_EXPORT void ModsDrvAtomicWrite(volatile UINT32* addr, UINT32 data)
{
    Cpu::AtomicWrite(reinterpret_cast<volatile INT32*>(addr), static_cast<INT32>(data));
}

MODS_EXPORT UINT32 ModsDrvAtomicXchg32(volatile UINT32* addr, UINT32 data)
{
    return Cpu::AtomicXchg32(addr, data);
}

MODS_EXPORT MODS_BOOL ModsDrvAtomicCmpXchg32(volatile UINT32* addr, UINT32 oldVal, UINT32 newVal)
{
    return Cpu::AtomicCmpXchg32(addr, oldVal, newVal) ? MODS_TRUE : MODS_FALSE;
}

MODS_EXPORT MODS_BOOL ModsDrvAtomicCmpXchg64(volatile UINT64* addr, UINT64 oldVal, UINT64 newVal)
{
    return Cpu::AtomicCmpXchg64(addr, oldVal, newVal) ? MODS_TRUE : MODS_FALSE;
}

MODS_EXPORT INT32 ModsDrvAtomicAdd(volatile INT32* addr, INT32 data)
{
    return Cpu::AtomicAdd(addr, data);
}

///-----------------------------------------------------------------------------
// CPU caching functions
//
MODS_EXPORT void ModsDrvFlushCpuCache(void)
{
    Platform::FlushCpuCache();
}

MODS_EXPORT void ModsDrvFlushCpuCacheRange
(
    void * StartAddress,
    void * EndAddress,
    UINT32 Flags
)
{
    Platform::FlushCpuCacheRange(StartAddress, EndAddress, Flags);
}

MODS_EXPORT void ModsDrvFlushCpuWriteCombineBuffer(void)
{
    Platform::FlushCpuWriteCombineBuffer();
}

///-----------------------------------------------------------------------------
// PCI config space functions
//
// Isn't this function confused?  Shouldn't it be RcToPciReturnCode?
static PciReturnCode PciReturnCodeToRc
(
    UINT32 rc
)
{
    switch (rc)
    {
        // Since these functions are called by the resman during cleanup after
        // a Ctrl-C error oclwrred, let's not confuse the resman with this
        // user-interface error that doesn't indicate an actual PCI problem.
        case RC::USER_ABORTED_SCRIPT:
            return PCI_OK;

        case OK:
            return PCI_OK;

        case RC::PCI_BIOS_NOT_PRESENT:
            return PCI_BIOS_NOT_PRESENT;

        case RC::PCI_FUNCTION_NOT_SUPPORTED:
            return PCI_FUNCTION_NOT_SUPPORTED;

        case RC::PCI_ILWALID_VENDOR_IDENTIFICATION:
            return PCI_ILWALID_VENDOR_IDENTIFICATION;

        case RC::PCI_DEVICE_NOT_FOUND:
            return PCI_DEVICE_NOT_FOUND;

        case RC::PCI_ILWALID_REGISTER_NUMBER:
            return PCI_ILWALID_REGISTER_NUMBER;

        default:
            ModsDrvPrintf(Tee::PriHigh,
                   "ModsDrv Unexpected RC in PciReturnCodeToRc: %d\n", rc);
            MASSERT(false);
            return PCI_BIOS_NOT_PRESENT;
    }
}

MODS_EXPORT PciReturnCode ModsDrvFindPciDevice
(
    UINT32   DeviceId,
    UINT32   VendorId,
    UINT32   Index,
    UINT32 * pDomain,
    UINT32 * pBus,
    UINT32 * pDevice,
    UINT32 * pFunction
)
{
    UINT32 rc = Platform::FindPciDevice(DeviceId, VendorId, Index,
                                        pDomain, pBus, pDevice, pFunction);

    return PciReturnCodeToRc(rc);
}

MODS_EXPORT PciReturnCode ModsDrvFindPciClassCode
(
    UINT32   ClassCode,
    UINT32   Index,
    UINT32 * pDomain,
    UINT32 * pBus,
    UINT32 * pDevice,
    UINT32 * pFunction
)
{
    UINT32 rc = Platform::FindPciClassCode(ClassCode, Index,
                                           pDomain, pBus, pDevice, pFunction);

    return PciReturnCodeToRc(rc);
}

MODS_EXPORT PciReturnCode ModsDrvPciInitHandle(UINT32 domain,
                                               UINT32 bus,
                                               UINT32 device,
                                               UINT32 function,
                                               UINT16* pVendorId,
                                               UINT16* pDeviceId)
{
    struct PCIDevice
    {
        UINT16 vendorId;
        UINT16 deviceId;
        UINT16 domain;
        UINT08 bus;
        UINT08 device;
        UINT08 function;
    };

    static bool s_Initialized = false;
    static vector<PCIDevice> s_FoundPCIDevices;

    if (!s_Initialized)
    {
        const UINT32 classes[] =
        {
            0x030000, // VGA
            0x030200, // 3D
            0x040300, // Azalia
            // Bridges, as defined in PCI spec
            0x060000,
            0x060100,
            0x060200,
            0x060300,
            0x060400,
            0x060401,
            0x060500,
            0x060600,
            0x060700,
            0x060800,
            0x060940,
            0x060980,
            0x060A00,
            0x060B00,
            0x060B01,
            0x068000,
        };

        s_FoundPCIDevices.reserve(32);

        for (const auto classC0de : classes)
        {
            for (INT32 idx = 0; ; ++idx)
            {
                UINT32 domain   = ~0U;
                UINT32 bus      = ~0U;
                UINT32 device   = ~0U;
                UINT32 function = ~0U;
                RC rc = Platform::FindPciClassCode(classC0de, idx,
                        &domain, &bus, &device, &function);
                if (rc != RC::OK)
                {
                    break;
                }

                UINT16 vendorId = 0xFFFFU;
                UINT16 deviceId = 0xFFFFU;

                rc = Platform::PciRead16(domain, bus, device, function, 0, &vendorId);
                if (rc != RC::OK)
                {
                    Printf(Tee::PriWarn, "Failed to read vendor id from PCI device "
                           "%04x:%02x:%02x.%x\n",
                           domain, bus, device, function);
                    continue;
                }

                rc = Platform::PciRead16(domain, bus, device, function, 2, &deviceId);
                if (rc != RC::OK)
                {
                    Printf(Tee::PriWarn, "Failed to read device id from PCI device "
                           "%04x:%02x:%02x.%x\n",
                           domain, bus, device, function);
                    continue;
                }

                const PCIDevice dev =
                {
                    vendorId,
                    deviceId,
                    static_cast<UINT16>(domain),
                    static_cast<UINT08>(bus),
                    static_cast<UINT08>(device),
                    static_cast<UINT08>(function)
                };
                s_FoundPCIDevices.push_back(dev);
            }
        }

        s_Initialized = true;
    }

    for (const auto& dev : s_FoundPCIDevices)
    {
        if (dev.domain   == domain &&
            dev.bus      == bus &&
            dev.device   == device &&
            dev.function == function)
        {
            // Special case for GCx - re-read vendor id from PCI cfg space
            constexpr UINT32 lwpu = 0x10DE;
            if (dev.vendorId == lwpu)
            {
                UINT16 vendorId = 0xFFFFU;
                const RC rc = Platform::PciRead16(domain, bus, device, function, 0, &vendorId);
                if (rc != RC::OK)
                {
                    Printf(Tee::PriWarn, "Failed to read vendor id from PCI device "
                           "%04x:%02x:%02x.%x\n",
                           domain, bus, device, function);
                    return PciReturnCodeToRc(RC::PCI_DEVICE_NOT_FOUND);
                }
                if (vendorId == 0xFFFFU)
                {
                    Printf(Tee::PriLow,
                           "PCI device %04x:%02x:%02x.%x (%04x:%04x) dropped off the bus\n",
                           domain, bus, device, function, dev.vendorId, dev.deviceId);
                    return PciReturnCodeToRc(RC::PCI_DEVICE_NOT_FOUND);
                }
            }

            if (pVendorId)
            {
                *pVendorId = dev.vendorId;
            }
            if (pDeviceId)
            {
                *pDeviceId = dev.deviceId;
            }

            Printf(Tee::PriDebug, "RM queried PCI device %04x:%02x:%02x.%x (%04x:%04x)\n",
                   domain, bus, device, function, dev.vendorId, dev.deviceId);

            return PciReturnCode(OK);
        }
    }

    return PciReturnCodeToRc(RC::PCI_DEVICE_NOT_FOUND);
}

MODS_EXPORT PciReturnCode ModsDrvPciGetBarInfo
(
    UINT32    domain,
    UINT32    bus,
    UINT32    device,
    UINT32    function,
    UINT32    barIndex,
    PHYSADDR* pBaseAddress,
    UINT64*   pBarSize
)
{
    UINT32 rc = Platform::PciGetBarInfo(domain, bus, device,
                                        function, barIndex,
                                        pBaseAddress, pBarSize);

    return PciReturnCodeToRc(rc);
}

MODS_EXPORT PciReturnCode ModsDrvPciGetIRQ
(
    UINT32  domain,
    UINT32  bus,
    UINT32  device,
    UINT32  function,
    UINT32* pIrq
)
{
    UINT32 rc = Platform::PciGetIRQ(domain, bus, device, function, pIrq);

    return PciReturnCodeToRc(rc);
}

MODS_EXPORT UINT08 ModsDrvPciRd08
(
    UINT32 Domain,
    UINT32 Bus,
    UINT32 Device,
    UINT32 Function,
    UINT32 Address
)
{
    UINT08 Data;
    Platform::PciRead08(Domain, Bus, Device, Function, Address, &Data);
    // Ignore errors.  Otherwise, a Ctrl-C will prevent the resman from
    // from cleaning up properly on exit.
    return Data;
}

MODS_EXPORT UINT16 ModsDrvPciRd16
(
    UINT32 Domain,
    UINT32 Bus,
    UINT32 Device,
    UINT32 Function,
    UINT32 Address
)
{
    UINT16 Data;
    Platform::PciRead16(Domain, Bus, Device, Function, Address, &Data);
    // Ignore errors.  Otherwise, a Ctrl-C will prevent the resman from
    // from cleaning up properly on exit.
    return Data;
}

MODS_EXPORT UINT32 ModsDrvPciRd32
(
    UINT32 Domain,
    UINT32 Bus,
    UINT32 Device,
    UINT32 Function,
    UINT32 Address
)
{
    UINT32 Data;
    Platform::PciRead32(Domain, Bus, Device, Function, Address, &Data);
    // Ignore errors.  Otherwise, a Ctrl-C will prevent the resman from
    // from cleaning up properly on exit.
    return Data;
}

MODS_EXPORT void ModsDrvPciWr08
(
    UINT32 Domain,
    UINT32 Bus,
    UINT32 Device,
    UINT32 Function,
    UINT32 Address,
    UINT08 Data
)
{
    Platform::PciWrite08(Domain, Bus, Device, Function, Address, Data);
}

MODS_EXPORT void ModsDrvPciWr16
(
    UINT32 Domain,
    UINT32 Bus,
    UINT32 Device,
    UINT32 Function,
    UINT32 Address,
    UINT16 Data
)
{
    Platform::PciWrite16(Domain, Bus, Device, Function, Address, Data);
}

MODS_EXPORT void ModsDrvPciWr32
(
    UINT32 Domain,
    UINT32 Bus,
    UINT32 Device,
    UINT32 Function,
    UINT32 Address,
    UINT32 Data
)
{
    Platform::PciWrite32(Domain, Bus, Device, Function, Address, Data);
}

MODS_EXPORT MODS_RC ModsDrvPciEnableAtomics
(
    UINT32      domain,
    UINT32      bus,
    UINT32      device,
    UINT32      func,
    MODS_BOOL * pbAtomicsEnabled,
    UINT32    * pAtomicTypes
)
{
    RC rc;
#if defined(INCLUDE_GPU)
    MASSERT(pbAtomicsEnabled);
    MASSERT(pAtomicTypes);

    TestDeviceMgr* pTestDeviceMgr = static_cast<TestDeviceMgr*>(DevMgrMgr::d_TestDeviceMgr);

    if (pTestDeviceMgr)
    {
        TestDevice::Id devId(domain, bus, device, func);
        for (auto pLwrDevice : *pTestDeviceMgr)
        {
            auto pPcie = pLwrDevice->GetInterface<Pcie>();
            MASSERT(pPcie); // PCIE interface is required for all devices

            if (devId == pLwrDevice->GetId())
            {
                bool bEnabled = false;
                CHECK_RC(pPcie->EnableAtomics(&bEnabled, pAtomicTypes));
                *pbAtomicsEnabled = bEnabled ? MODS_TRUE : MODS_FALSE;
                return RC::OK;
            }
        }
    }
    return RC::DEVICE_NOT_FOUND;
#else
    return rc;
#endif
}

MODS_EXPORT MODS_RC ModsDrvPciFlr
(
    UINT32 domain,
    UINT32 bus,
    UINT32 device,
    UINT32 func
)
{
    return Platform::PciResetFunction(domain, bus, device, func);
}

///-----------------------------------------------------------------------------
// Port I/O functions
//
MODS_EXPORT UINT08 ModsDrvIoRd08(UINT16 Address)
{
    UINT08 Data = 0;
    Platform::PioRead08(Address, &Data);
    return Data;
}

MODS_EXPORT UINT16 ModsDrvIoRd16(UINT16 Address)
{
    UINT16 Data = 0;
    Platform::PioRead16(Address, &Data);
    return Data;
}

MODS_EXPORT UINT32 ModsDrvIoRd32(UINT16 Address)
{
    UINT32 Data = 0;
    Platform::PioRead32(Address, &Data);
    return Data;
}

MODS_EXPORT void ModsDrvIoWr08(UINT16 Address, UINT08 Data)
{
    Platform::PioWrite08(Address, Data);
}

MODS_EXPORT void ModsDrvIoWr16(UINT16 Address, UINT16 Data)
{
    Platform::PioWrite16(Address, Data);
}

MODS_EXPORT void ModsDrvIoWr32(UINT16 Address, UINT32 Data)
{
    Platform::PioWrite32(Address, Data);
}

MODS_EXPORT MODS_BOOL ModsDrvGetSOCDeviceAperture
(
    UINT32 DevType,         // Device type, types defined in relocation table
    UINT32 Index,           // Index within the type (0 is the first device of
                            // that type)
    void** ppLinAddr,       // Returned CPU pointer to device aperture
    UINT32* pSize           // Returned size of device aperture
)
{
    if (!Platform::IsTegra())
    {
        return MODS_FALSE;
    }

    vector<CheetAh::DeviceDesc> desc;
    if (OK != CheetAh::GetDeviceDescByType(DevType, &desc))
    {
        return MODS_FALSE;
    }

    if (Index >= desc.size())
    {
        return MODS_FALSE;
    }

    if (OK != CheetAh::SocPtr()->GetAperture(desc[Index].devIndex, ppLinAddr))
    {
        return MODS_FALSE;
    }

    *pSize = static_cast<UINT32>(desc[Index].apertureSize);
    return MODS_TRUE;
}

MODS_EXPORT MODS_BOOL ModsDrvGetSOCDeviceApertureByName
(
    const char* Name,       // Device Name
    void** ppLinAddr,       // Returned CPU pointer to device aperture
    UINT32* pSize           // Returned size of device aperture
)
{
    if (!Platform::IsTegra())
    {
        return MODS_FALSE;
    }

    CheetAh::DeviceDesc desc;
    if (OK != CheetAh::GetDeviceDesc(Name, &desc))
    {
        return MODS_FALSE;
    }

    if (OK != CheetAh::SocPtr()->GetAperture(desc.devIndex, ppLinAddr))
    {
        return MODS_FALSE;
    }

    *pSize = static_cast<UINT32>(desc.apertureSize);
    return MODS_TRUE;
}

///-----------------------------------------------------------------------------
// Low-level memory allocator
//
MODS_EXPORT size_t ModsDrvGetPageSize(void)
{
    return Platform::GetPageSize();
}

MODS_EXPORT void * ModsDrvAllocPagesForPciDev
(
    size_t    numBytes,
    MODS_BOOL contiguous,
    UINT32    addressBits,
    UINT32    pageAttrib,
    UINT32    domain,
    UINT32    bus,
    UINT32    device,
    UINT32    function
)
{
    void * desc;
    RC rc = Platform::AllocPages(numBytes,
                                 &desc,
                                 contiguous != MODS_FALSE,
                                 addressBits,
                                 (Memory::Attrib)pageAttrib,
                                 domain,
                                 bus,
                                 device,
                                 function);
    if (rc != OK)
    {
        ModsDrvPrintf(Tee::PriError,
                      "ModsDrvAllocPages failed: %s\n", rc.Message());
        return nullptr;
    }
    return desc;
}

MODS_EXPORT void * ModsDrvAllocPages
(
    size_t NumBytes,
    MODS_BOOL Contiguous,
    UINT32 AddressBits,
    UINT32 PageAttrib,
    UINT32 Protect,
    UINT32 GpuInst
)
{
    void * Desc;
    RC rc = Platform::AllocPages(NumBytes, &Desc, Contiguous != MODS_FALSE,
                                 AddressBits,
                                 (Memory::Attrib)PageAttrib, GpuInst,
                                 (Memory::Protect)Protect);
    if (rc != OK)
    {
        ModsDrvPrintf(Tee::PriHigh,
                      "ModsDrvAllocPages failed: %s\n", rc.Message());
        return nullptr;
    }
    return Desc;
}

MODS_EXPORT void * ModsDrvAllocPagesAligned
(
    size_t NumBytes,
    UINT32 PhysAlign,
    UINT32 AddressBits,
    UINT32 PageAttrib,
    UINT32 Protect,
    UINT32 GpuInst
)
{
    void * Desc;
    RC rc = Platform::AllocPagesAligned(NumBytes, &Desc, PhysAlign, AddressBits,
                                        (Memory::Attrib)PageAttrib, GpuInst);
    if (rc != OK)
    {
        ModsDrvPrintf(Tee::PriHigh,
               "ModsDrvAllocPagesAligned failed: %s\n",
               rc.Message());
        return NULL;
    }
    return Desc;
}

MODS_EXPORT void ModsDrvFreePages(void *Descriptor)
{
    Platform::FreePages(Descriptor);
}

MODS_EXPORT void * ModsDrvMapPages
(
    void *Descriptor,
    size_t Offset,
    size_t Size,
    UINT32 Protect
)
{
    void *vAddr;
    RC rc = Platform::MapPages(&vAddr, Descriptor, Offset, Size,
                               (Memory::Protect)Protect);
    if (rc != OK)
    {
        ModsDrvPrintf(Tee::PriHigh,
               "ModsDrvMapPages failed: %s\n", rc.Message());
        return NULL;
    }
    return vAddr;
}

MODS_EXPORT void ModsDrvUnMapPages(void * VirtualAddress)
{
    Platform::UnMapPages(VirtualAddress);
}

MODS_EXPORT UINT64 ModsDrvGetPhysicalAddress(void *Descriptor, size_t Offset)
{
    return Platform::GetPhysicalAddress(Descriptor, Offset);
}

MODS_EXPORT MODS_RC ModsDrvSetDmaMask
(
    UINT32 domain,
    UINT32 bus,
    UINT32 device,
    UINT32 func,
    UINT32 numBits
)
{
    return Platform::SetDmaMask(domain, bus, device, func, numBits);
}

MODS_EXPORT MODS_RC ModsDrvDmaMapMemory
(
    UINT32 domain,
    UINT32 bus,
    UINT32 device,
    UINT32 func,
    void  *descriptor
)
{
    return Platform::DmaMapMemory(domain, bus, device, func, descriptor);
}

MODS_EXPORT MODS_RC ModsDrvDmaUnmapMemory
(
    UINT32 domain,
    UINT32 bus,
    UINT32 device,
    UINT32 func,
    void  *descriptor
)
{
    return Platform::DmaUnmapMemory(domain, bus, device, func, descriptor);
}

MODS_EXPORT UINT64 ModsDrvGetMappedPhysicalAddress
(
    UINT32 domain,
    UINT32 bus,
    UINT32 device,
    UINT32 func,
    void  *descriptor,
    size_t offset
)
{
    return Platform::GetMappedPhysicalAddress(domain, bus, device, func, descriptor, offset);
}

MODS_EXPORT void ModsDrvReservePages(size_t NumBytes, UINT32 PageAttrib)
{
    Platform::ReservePages(NumBytes, (Memory::Attrib)PageAttrib);
}

MODS_EXPORT void * ModsDrvMapDeviceMemory
(
    UINT64 PhysAddr,
    size_t NumBytes,
    UINT32 PageAttrib,
    UINT32 Protect
)
{
    void * vAddr;
    RC rc = Platform::MapDeviceMemory(&vAddr, PhysAddr, NumBytes,
        (Memory::Attrib)PageAttrib, (Memory::Protect)Protect);
    if ((rc != OK) && (rc != RC::USER_ABORTED_SCRIPT))
    {
        ModsDrvPrintf(Tee::PriHigh,
               "ModsDrvMapDeviceMemory failed: %s\n",
               rc.Message());
        return 0;
    }
    return vAddr;
}

MODS_EXPORT void ModsDrvUnMapDeviceMemory(void *VirtualAddr)
{
    Platform::UnMapDeviceMemory(VirtualAddr);
}

MODS_EXPORT MODS_RC ModsDrvSetMemRange
(
    UINT64 PhysAddr,
    size_t NumBytes,
    UINT32 PageAttr
)
{
    if (0 == PageAttr)
        PageAttr = Memory::UC;

    RC rc = Platform::SetMemRange(PhysAddr, NumBytes, (Memory::Attrib)PageAttr);
    if ((rc != OK) && (rc != RC::USER_ABORTED_SCRIPT))
    {
        ModsDrvPrintf(Tee::PriHigh,
               "ModsDrvSetMemRange 0x%x_%08x, size %d, type %d failed: %s\n",
               (UINT32)(PhysAddr>>32), (UINT32)PhysAddr, (UINT32)NumBytes,
               PageAttr, rc.Message());
        return rc;
    }
    return OK;
}

MODS_EXPORT void ModsDrvUnSetMemRange(UINT64 PhysAddr, size_t NumBytes)
{
    Platform::UnSetMemRange(PhysAddr, NumBytes);
}

MODS_EXPORT MODS_BOOL ModsDrvSyncForDevice(UINT32 domain,
                                           UINT32 bus,
                                           UINT32 device,
                                           UINT32 func,
                                           void  *descriptor,
                                           UINT32 direction)
{
    // For now this is a stub, see core/include/modsdrv.h for explanation
    return MODS_TRUE;
}

MODS_EXPORT MODS_BOOL ModsDrvSyncForCpu(UINT32 domain,
                                        UINT32 bus,
                                        UINT32 device,
                                        UINT32 func,
                                        void  *descriptor,
                                        UINT32 direction)
{
    // For now this is a stub, see core/include/modsdrv.h for explanation
    return MODS_TRUE;
}

MODS_EXPORT MODS_BOOL ModsDrvGetCarveout(UINT64* pPhysAddr, UINT64* pSize)
{
    return (OK == Platform::GetCarveout(pPhysAddr, pSize))
        ? MODS_TRUE : MODS_FALSE;
}

MODS_EXPORT MODS_BOOL ModsDrvGetVPR(UINT64* pPhysAddr, UINT64* pSize)
{
    return (OK == Platform::GetVPR(pPhysAddr, pSize))
        ? MODS_TRUE : MODS_FALSE;
}

MODS_EXPORT MODS_BOOL ModsDrvGetGenCarveout(UINT64 *pPhysAddr, UINT64 *pSize, UINT32 id, UINT64 align)
{
    return (OK == Platform::GetGenCarveout(pPhysAddr, pSize, id, align))
        ? MODS_TRUE : MODS_FALSE;
}

MODS_EXPORT MODS_BOOL ModsDrvCallACPIGeneric(UINT32 GpuInst,
                                             UINT32 Function,
                                             void *Param1,
                                             void *Param2,
                                             void *Param3)
{
    if (Platform::CallACPIGeneric(GpuInst,
                                  Function,
                                  Param1,Param2,
                                  Param3) == OK)
    {
        return MODS_TRUE;
    }

    return MODS_FALSE;
}

MODS_EXPORT MODS_BOOL ModsDrvLWIFMethod
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
    if (Platform::LWIFMethod(Function, SubFunction, InParams,
        InParamSize, OutStatus, OutData, OutDataSize) == OK)
    {
        return MODS_TRUE;
    }

    return MODS_FALSE;
}

//------------------------------------------------------------------------------

MODS_EXPORT MODS_BOOL ModsDrvCpuIsCPUIDSupported(void)
{
#if defined(__arm__)
    if (Platform::IsTegra())
    {
        Xp::BoardInfo boardInfo;
        RC rc = Xp::GetBoardInfo(&boardInfo);
        return (rc == OK) ? MODS_TRUE : MODS_FALSE;
    }
#endif
    bool ret = Cpu::IsCPUIDSupported();
    return ret ? MODS_TRUE : MODS_FALSE;
}

MODS_EXPORT void ModsDrvCpuCPUID
(
    UINT32 Op,
    UINT32 SubOp,
    UINT32 *pEAX,
    UINT32 *pEBX,
    UINT32 *pECX,
    UINT32 *pEDX
)
{
#if defined(__arm__)
    if (Platform::IsTegra())
    {
        Xp::BoardInfo boardInfo;
        if (Xp::GetBoardInfo(&boardInfo) == OK)
        {
            *pEAX = boardInfo.cpuId;
            *pEBX = 0;
            *pECX = 0;
            *pEDX = 0;
            return;
        }
    }
#endif
    Cpu::CPUID(Op, SubOp, pEAX, pEBX, pECX, pEDX);
}

///-----------------------------------------------------------------------------
// Registry access functions
//
MODS_EXPORT MODS_BOOL ModsDrvReadRegistryDword
(
    const char * RegDevNode,
    const char * RegParmStr,
    UINT32     * pData
)
{
    MASSERT(RegDevNode != 0);
    MASSERT(RegParmStr != 0);
    MASSERT(pData      != 0);

    RC rc = Registry::Read(RegDevNode, RegParmStr, pData);

    // Ignore Ctrl-C errors here, so resman can clean up afterwards properly.
    if((OK == rc) || (rc == RC::USER_ABORTED_SCRIPT))
    {
        return MODS_TRUE;
    }

    return MODS_FALSE;
}

///-----------------------------------------------------------------------------
// Registry access functions
//
MODS_EXPORT MODS_BOOL ModsDrvGpuReadRegistryDword
(
    UINT32       GpuInstance,
    const char * RegDevNode,
    const char * RegParmStr,
    UINT32     * pData
)
{
    MASSERT(RegDevNode != 0);
    MASSERT(RegParmStr != 0);
    MASSERT(pData      != 0);
    string gpuRegString = Utility::StrPrintf("GPU%u_%s", GpuInstance, RegParmStr);

    return ModsDrvReadRegistryDword(RegDevNode, gpuRegString.c_str(), pData);
}

MODS_EXPORT MODS_BOOL ModsDrvReadRegistryString
(
    const char * RegDevNode,
    const char * RegParmStr,
    char       * pData,
    UINT32     * pLength
)
{
    MASSERT(RegDevNode != 0);
    MASSERT(RegParmStr != 0);
    MASSERT(pLength    != 0);

    string Data;
    RC rc = Registry::Read(RegDevNode, RegParmStr, &Data);
    if (OK == rc || RC::USER_ABORTED_SCRIPT == rc)
    {
        if (NULL == pData)
        {
            // Just return the length, so the user can try again with a
            // correctly-sized buffer for us to fill.
        }
        else
        {
            if (*pLength < UINT32(Data.size()))
            {
                MASSERT(!"Registry string too big");
                return MODS_FALSE;
            }
            // If *pLength is exactly == Data.size(), the resulting string
            // will not be null-terminated.  Caveat emptor.
            strncpy(pData, Data.c_str(), *pLength);
        }
        *pLength = (UINT32) Data.size();
        return MODS_TRUE;
    }

    return MODS_FALSE;
}

MODS_EXPORT MODS_BOOL ModsDrvGpuReadRegistryString
(
    UINT32       GpuInstance,
    const char * RegDevNode,
    const char * RegParmStr,
    char       * pData,
    UINT32     * pLength
)
{
    MASSERT(RegDevNode != 0);
    MASSERT(RegParmStr != 0);
    MASSERT(pLength      != 0);
    string gpuRegString = Utility::StrPrintf("GPU%u_%s", GpuInstance, RegParmStr);

    return ModsDrvReadRegistryString(RegDevNode, gpuRegString.c_str(), pData, pLength);
}

MODS_EXPORT MODS_BOOL ModsDrvReadRegistryBinary
(
    const char * RegDevNode,
    const char * RegParmStr,
    UINT08     * pData,
    UINT32     * pLength
)
{
    MASSERT(RegDevNode != 0);
    MASSERT(RegParmStr != 0);
    MASSERT(pData      != 0);
    MASSERT(pLength    != 0);

    vector<UINT08> Data;
    RC rc = Registry::Read(RegDevNode, RegParmStr, &Data);
    if (OK == rc || RC::USER_ABORTED_SCRIPT == rc)
    {
        for (vector<UINT08>::size_type i = 0; i < Data.size(); ++i)
        {
            pData[i] = Data[i];
        }
        *pLength = (UINT32) Data.size();
        return MODS_TRUE;
    }

    return MODS_FALSE;
}

MODS_EXPORT MODS_BOOL ModsDrvGpuReadRegistryBinary
(
    UINT32       GpuInstance,
    const char * RegDevNode,
    const char * RegParmStr,
    UINT08     * pData,
    UINT32     * pLength
)
{
    MASSERT(RegDevNode != 0);
    MASSERT(RegParmStr != 0);
    MASSERT(pData      != 0);
    MASSERT(pLength      != 0);
    string gpuRegString = Utility::StrPrintf("GPU%u_%s", GpuInstance, RegParmStr);

    return ModsDrvReadRegistryBinary(RegDevNode, gpuRegString.c_str(), pData, pLength);
}

MODS_EXPORT MODS_RC ModsDrvPackRegistry
(
    void  * pRegTable,
    LwU32 * pSize
)
{
    return Registry::PackRegistryEntries("ResourceManager", (Registry::PACKED_REGISTRY_TABLE*)pRegTable, pSize);
}

///-----------------------------------------------------------------------------
// Time functions
//
MODS_EXPORT void ModsDrvPause(void)
{
    FLOAT64 maxPollHz = Tasker::GetMaxHwPollHz();

    if (maxPollHz > 0.0)
    {
        // User specified -poll_hw_hz, delay returning to RM or GL long enough
        // to keep the maximum HW register polling frequency below the spec.
        UINT64 tmp = Platform::GetTimeNS() + UINT64(1e9/maxPollHz + 0.5);
        do
        {
            Platform::Pause();
        }
        while (tmp > Platform::GetTimeNS());
    }
    else
    {
        Platform::Pause();
    }
}

MODS_EXPORT void ModsDrvDelayUS(UINT32 Microseconds)
{
    // Don't do a pointless host delay when running on simulation.
    // It would probably be better to call Platform::Delay at some point,
    // but this would slow down simulation runs significantly unless current
    // RM delays were greatly reduced.
    if (Platform::GetSimulationMode() == Platform::Hardware)
        Utility::Delay(Microseconds);
}

MODS_EXPORT void ModsDrvDelayNS(UINT32 Nanoseconds)
{
    // Don't do a pointless host delay when running on simulation.
    // It would probably be better to call Platform::Delay at some point,
    // but this would slow down simulation runs significantly unless current
    // RM delays were greatly reduced.
    if (Platform::GetSimulationMode() == Platform::Hardware)
        Utility::DelayNS(Nanoseconds);
}

MODS_EXPORT void ModsDrvSleep(UINT32 Milliseconds)
{
    Tasker::Sleep(Milliseconds);
}

MODS_EXPORT UINT64 ModsDrvGetTimeNS(void)
{
    return Platform::GetTimeNS();
}

MODS_EXPORT UINT64 ModsDrvGetTimeUS(void)
{
    return Platform::GetTimeUS();
}

MODS_EXPORT UINT64 ModsDrvGetTimeMS(void)
{
    return Platform::GetTimeMS();
}

MODS_EXPORT void ModsDrvGetNTPTime(UINT32 *secs, UINT32 *msecs)
{
    if (secs && msecs)
    {
        Xp::GetNTPTime(*secs, *msecs);
    }
}

///-----------------------------------------------------------------------------
// Interrupt functions
//
MODS_EXPORT int ModsDrvGetLwrrentIrql(void)
{
    return Platform::GetLwrrentIrql();
}

MODS_EXPORT int ModsDrvRaiseIrql(int NewIrql)
{
    return Platform::RaiseIrql((Platform::Irql)NewIrql);
}

MODS_EXPORT void ModsDrvLowerIrql(int NewIrql)
{
    Platform::LowerIrql((Platform::Irql)NewIrql);
}

MODS_EXPORT MODS_BOOL ModsDrvHookIRQ(UINT32 irq, ISR func, void *pCookie)
{
    RC rc = Platform::HookIRQ(irq, func, pCookie);
    return (rc == OK) ? MODS_TRUE : MODS_FALSE;
}

MODS_EXPORT MODS_BOOL ModsDrvUnhookIRQ(UINT32 irq, ISR func, void *pCookie)
{
    RC rc = Platform::UnhookIRQ(irq, func, pCookie);
    return (rc == OK) ? MODS_TRUE : MODS_FALSE;
}

MODS_EXPORT MODS_BOOL ModsDrvHookInt
(
    const IrqParams* pIrqInfo,
    ISR func,
    void* pCookie
)
{
    RC rc = Platform::HookInt(*pIrqInfo, func, pCookie);
    return (rc == OK) ? MODS_TRUE : MODS_FALSE;
}

MODS_EXPORT MODS_BOOL ModsDrvUnhookInt
(
    const IrqParams* pIrqInfo,
    ISR func,
    void* pCookie
)
{
    RC rc = Platform::UnhookInt(*pIrqInfo, func, pCookie);
    return (rc == OK) ? MODS_TRUE : MODS_FALSE;
}

///-----------------------------------------------------------------------------
// Read and write memory that may live inside a simulator
//
MODS_EXPORT UINT08 ModsDrvMemRd08(const volatile void *Address)
{
    return MEM_RD08(Address);
}

MODS_EXPORT UINT16 ModsDrvMemRd16(const volatile void *Address)
{
    return MEM_RD16(Address);
}

MODS_EXPORT UINT32 ModsDrvMemRd32(const volatile void *Address)
{
    return MEM_RD32(Address);
}

MODS_EXPORT UINT64 ModsDrvMemRd64(const volatile void *Address)
{
    return MEM_RD64(Address);
}

MODS_EXPORT void ModsDrvMemWr08(volatile void *Address, UINT08 Data)
{
    MEM_WR08(Address, Data);
}

MODS_EXPORT void ModsDrvMemWr16(volatile void *Address, UINT16 Data)
{
    MEM_WR16(Address, Data);
}

MODS_EXPORT void ModsDrvMemWr32(volatile void *Address, UINT32 Data)
{
    MEM_WR32(Address, Data);
}

MODS_EXPORT void ModsDrvMemWr64(volatile void *Address, UINT64 Data)
{
    MEM_WR64(Address, Data);
}

MODS_EXPORT void ModsDrvMemCopy
(
    volatile void *Dst,
    const volatile void *Src,
    size_t Count
)
{
    Platform::MemCopy(Dst, Src, Count);
}

MODS_EXPORT void ModsDrvMemSet(volatile void *Dst, UINT08 Data, size_t Count)
{
    Platform::MemSet(Dst, Data, Count);
}

///-----------------------------------------------------------------------------
// Simulator functions
//
MODS_EXPORT MODS_BOOL ModsDrvSimChiplibLoaded(void)
{
    return Platform::IsChiplibLoaded() ? MODS_TRUE : MODS_FALSE;
}

MODS_EXPORT void ModsDrvClockSimulator(void)
{
    Platform::ClockSimulator(3);
}

MODS_EXPORT UINT64 ModsDrvGetSimulatorTime(void)
{
    return Platform::GetSimulatorTime();
}

MODS_EXPORT SimMode ModsDrvGetSimulationMode(void)
{
    return (SimMode)Platform::GetSimulationMode();
}

MODS_EXPORT MODS_BOOL ModsDrvExtMemAllocSupported(void)
{
    return Platform::ExtMemAllocSupported() ? MODS_TRUE : MODS_FALSE;
}

MODS_EXPORT int ModsDrvSimEscapeWrite
(
    const char *Path,
    UINT32 Index,
    UINT32 Size,
    UINT32 Value
)
{
    return Platform::EscapeWrite(Path, Index, Size, Value);
}

MODS_EXPORT int ModsDrvSimEscapeRead
(
    const char *Path,
    UINT32 Index,
    UINT32 Size,
    UINT32 *Value
)
{
    return Platform::EscapeRead(Path, Index, Size, Value);
}

MODS_EXPORT int ModsDrvSimGpuEscapeWrite
(
    UINT32 GpuId,
    const char *Path,
    UINT32 Index,
    UINT32 Size,
    UINT32 Value
)
{
    return Platform::GpuEscapeWrite(GpuId, Path, Index, Size, Value);
}

MODS_EXPORT int ModsDrvSimGpuEscapeRead
(
    UINT32 GpuId,
    const char *Path,
    UINT32 Index,
    UINT32 Size,
    UINT32 *Value
)
{
    return Platform::GpuEscapeRead(GpuId, Path, Index, Size, Value);
}

MODS_EXPORT int ModsDrvSimGpuEscapeWriteBuffer
(
    UINT32 GpuId,
    const char *Path,
    UINT32 Index,
    size_t Size,
    const void *Buf
)
{
    return Platform::GpuEscapeWriteBuffer(GpuId, Path, Index, Size, Buf);
}

MODS_EXPORT int ModsDrvSimGpuEscapeReadBuffer
(
    UINT32 GpuId,
    const char *Path,
    UINT32 Index,
    size_t Size,
    void *Buf
)
{
    return Platform::GpuEscapeReadBuffer(GpuId, Path, Index, Size, Buf);
}

///-----------------------------------------------------------------------------
// Amodel functions
//
MODS_EXPORT void ModsDrvAmodAllocChannelDma
(
    UINT32 ChID,
    UINT32 Class,
    UINT32 CtxDma,
    UINT32 ErrorNotifierCtxDma
)
{
    Platform::AmodAllocChannelDma(ChID, Class, CtxDma, ErrorNotifierCtxDma);
}

MODS_EXPORT void ModsDrvAmodAllocChannelGpFifo
(
    UINT32 ChID,
    UINT32 Class,
    UINT32 CtxDma,
    UINT64 GpFifoOffset,
    UINT32 GpFifoEntries,
    UINT32 ErrorNotifierCtxDma
)
{
    Platform::AmodAllocChannelGpFifo(ChID, Class, CtxDma, GpFifoOffset,
                                     GpFifoEntries, ErrorNotifierCtxDma);
}

MODS_EXPORT void ModsDrvAmodFreeChannel(UINT32 ChID)
{
    Platform::AmodFreeChannel(ChID);
}

MODS_EXPORT void ModsDrvAmodAllocContextDma
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
    Platform::AmodAllocContextDma(ChID, Handle, Class, Target, Limit, Base,
                                  Protect);
}

MODS_EXPORT void ModsDrvAmodAllocObject
(
    UINT32 ChID,
    UINT32 Handle,
    UINT32 Class
)
{
    Platform::AmodAllocObject(ChID, Handle, Class);
}

MODS_EXPORT void ModsDrvAmodFreeObject(UINT32 ChID, UINT32 Handle)
{
    Platform::AmodFreeObject(ChID, Handle);
}

///-----------------------------------------------------------------------------
// GPU-specific functions
//
MODS_EXPORT UINT32 ModsDrvGpuRegWriteExcluded(UINT32 GpuInst, UINT32 Offset)
{
    return GpuPtr()->GetRegWriteMask(GpuInst, Offset);  //Callwlate Bitmask
}

MODS_EXPORT UINT32 ModsDrvRegRd32(const volatile void* address)
{
#if defined(INCLUDE_GPU) && !defined(SIM_BUILD)
    if (Platform::IsVirtFunMode())
    {
        const PHYSADDR   physAddr =
            Platform::VirtualToPhysical(const_cast<volatile void*>(address));
        VmiopElwManager* pMgr     = VmiopElwManager::Instance();
        VmiopElw*        pVmiopElw;
        UINT32           regAddr;
        UINT32           data;
        switch (pMgr->FindVfEmuRegister(physAddr, &pVmiopElw, &regAddr))
        {
            case OK:
                pVmiopElw->CallPluginRegRead(regAddr, 4, &data);
                return data;
            case RC::ILWALID_REGISTER_NUMBER:
                return 0;
            default:
                break;
        }
    }
#endif
    return MEM_RD32(address);
}

MODS_EXPORT void ModsDrvRegWr32(volatile void* address, UINT32 data)
{
#if defined(INCLUDE_GPU) && !defined(SIM_BUILD)
    if (Platform::IsVirtFunMode())
    {
        const PHYSADDR   physAddr = Platform::VirtualToPhysical(address);
        VmiopElwManager* pMgr     = VmiopElwManager::Instance();
        VmiopElw*        pVmiopElw;
        UINT32           regAddr;
        switch (pMgr->FindVfEmuRegister(physAddr, &pVmiopElw, &regAddr))
        {
            case OK:
                pVmiopElw->CallPluginRegWrite(regAddr, 4, &data);
                return;
            case RC::ILWALID_REGISTER_NUMBER:
                return;
            default:
                break;
        }
    }
#endif
    MEM_WR32(address, data);
}

MODS_EXPORT void ModsDrvLogRegRd
(
    UINT32 GpuInst,
    UINT32 Offset,
    UINT32 Data,
    UINT32 Bits
)
{
#if defined(INCLUDE_GPU)
    GpuSubdevice *pSubdev = nullptr;
    GpuDevMgr* const pGpuDevMgr =
        static_cast<GpuDevMgr*>(DevMgrMgr::d_GraphDevMgr);
    if (pGpuDevMgr)
    {
        pSubdev = pGpuDevMgr->GetSubdeviceByGpuInst(GpuInst);
    }
    GpuSubdevice::sLogRegRead(pSubdev, GpuInst, Offset, Data, Bits);
#endif
    return;
}

MODS_EXPORT void ModsDrvLogRegWr
(
    UINT32 GpuInst,
    UINT32 Offset,
    UINT32 Data,
    UINT32 Bits
)
{
#if defined(INCLUDE_GPU)
    GpuSubdevice *pSubdev = nullptr;
    GpuDevMgr* const pGpuDevMgr =
        static_cast<GpuDevMgr*>(DevMgrMgr::d_GraphDevMgr);
    if (pGpuDevMgr)
    {
        pSubdev = pGpuDevMgr->GetSubdeviceByGpuInst(GpuInst);
    }
    GpuSubdevice::sLogRegWrite(pSubdev, GpuInst, Offset, Data, Bits);
#endif
    return;
}

MODS_EXPORT MODS_RC ModsDrvGpuPreVBIOSSetup(UINT32 GpuInstance)
{
    return GpuPtr()->PreVBIOSSetup(GpuInstance);
}

MODS_EXPORT MODS_RC ModsDrvGpuPostVBIOSSetup(UINT32 GpuInstance)
{
    return GpuPtr()->PostVBIOSSetup(GpuInstance);
}

MODS_EXPORT void * ModsDrvGetVbiosFromLws(UINT32 GpuInstance)
{
    return Xp::GetVbiosFromLws(GpuInstance);
}

MODS_EXPORT void ModsDrvAddGpuSubdevice(const void* pPciDevInfo)
{
#if defined(INCLUDE_GPU)
    GpuDevMgr* const pGpuDevMgr =
        static_cast<GpuDevMgr*>(DevMgrMgr::d_GraphDevMgr);
    if (pGpuDevMgr)
    {
        pGpuDevMgr->AddGpuSubdevice(
                *static_cast<const PciDevInfo*>(pPciDevInfo));
    }
#endif
}

MODS_EXPORT UINT32 ModsDrvGpuDisplaySelected(void)
{
    ModsDrvPrintf(Tee::PriError,
                  "ModsDrvGpuDisplaySelected is deprecated. Returning 0.\n");
    return 0;
}

MODS_EXPORT MODS_BOOL ModsDrvGpuGrIntrHook
(
    UINT32 GrIdx,
    UINT32 GrIntr,
    UINT32 ClassError,
    UINT32 Addr,
    UINT32 DataLo
)
{
    bool ret = GpuPtr()->GrIntrHook(GrIdx, GrIntr, ClassError, Addr, DataLo);
    return ret ? MODS_TRUE : MODS_FALSE;
}

MODS_EXPORT MODS_BOOL ModsDrvGpuDispIntrHook(UINT32 Intr0, UINT32 Intr1)
{
    bool ret = GpuPtr()->DispIntrHook(Intr0, Intr1);
    return ret ? MODS_TRUE : MODS_FALSE;
}

MODS_EXPORT MODS_BOOL ModsDrvGpuRcCheckCallback(UINT32 GpuInstance)
{
    bool ret = GpuPtr()->RcCheckCallback(GpuInstance);
    return ret ? MODS_TRUE : MODS_FALSE;
}

MODS_EXPORT MODS_BOOL ModsDrvGpuRcCallback
(
    UINT32 GpuInstance,
    UINT32 hClient,
    UINT32 hDevice,
    UINT32 hObject,
    UINT32 errorLevel,
    UINT32 errorType,
    void *pData,
    void *pRecoveryCallback
)
{
    bool ret = GpuPtr()->RcCallback(GpuInstance, hClient, hDevice, hObject,
                                    errorLevel, errorType, pData,
                                    pRecoveryCallback);
    return ret ? MODS_TRUE : MODS_FALSE;
}

MODS_EXPORT void ModsDrvGpuStopEngineScheduling
(
    UINT32 GpuInstance,
    UINT32 EngineId,
    MODS_BOOL Stop
)
{
    GpuPtr()->StopEngineScheduling(GpuInstance, EngineId, Stop == MODS_TRUE);
}

MODS_EXPORT MODS_BOOL ModsDrvGpuCheckEngineScheduling
(
    UINT32 GpuInstance,
    UINT32 EngineId
)
{
    bool status = GpuPtr()->CheckEngineScheduling(GpuInstance, EngineId);
    return status ? MODS_TRUE : MODS_FALSE;
}

//------------------------------------------------------------------------------
MODS_EXPORT MODS_BOOL ModsDrvHandleRmPolicyManagerEvent
(
    UINT32 GpuInst,
    UINT32 EventId
)
{
    if (OK == GpuPtr()->HandleRmPolicyManagerEvent(GpuInst, EventId))
        return MODS_TRUE;

    return MODS_FALSE;
}

//------------------------------------------------------------------------------
MODS_EXPORT MODS_BOOL ModsDrvClockGetHandle
(
    ModsClockDomain clockDomain,
    UINT32 clockIndex,
    UINT64 *pHandle
)
{
    const RC rc = CheetAh::GetClockHandle(clockDomain, clockIndex, pHandle);
    return (OK == rc) ? MODS_TRUE : MODS_FALSE;
}

//------------------------------------------------------------------------------
MODS_EXPORT MODS_BOOL ModsDrvClockEnable
(
    UINT64 clockHandle,
    MODS_BOOL enable
)
{
    Xp::ClockManager* const pClockMgr = Xp::GetClockManager();
    if (!pClockMgr)
    {
        return MODS_FALSE;
    }
    const RC rc = enable
                    ? pClockMgr->EnableClock(clockHandle)
                    : pClockMgr->DisableClock(clockHandle);
    return (OK == rc) ? MODS_TRUE : MODS_FALSE;
}

//------------------------------------------------------------------------------
MODS_EXPORT MODS_BOOL ModsDrvClockSetRate
(
    UINT64 clockHandle,
    UINT64 rate
)
{
    Xp::ClockManager* const pClockMgr = Xp::GetClockManager();
    if (!pClockMgr)
    {
        return MODS_FALSE;
    }
    const RC rc = pClockMgr->SetClockRate(clockHandle, rate);
    return (OK == rc) ? MODS_TRUE : MODS_FALSE;
}

//------------------------------------------------------------------------------
MODS_EXPORT MODS_BOOL ModsDrvClockGetRate
(
    UINT64 clockHandle,
    UINT64 *pRate
)
{
    Xp::ClockManager* const pClockMgr = Xp::GetClockManager();
    if (!pClockMgr)
    {
        return MODS_FALSE;
    }
    const RC rc = pClockMgr->GetClockRate(clockHandle, pRate);
    return (OK == rc) ? MODS_TRUE : MODS_FALSE;
}

//------------------------------------------------------------------------------
MODS_EXPORT MODS_BOOL ModsDrvClockSetParent
(
    UINT64 clockHandle,
    UINT64 parentClockHandle
)
{
    Xp::ClockManager* const pClockMgr = Xp::GetClockManager();
    if (!pClockMgr)
    {
        return MODS_FALSE;
    }
    const RC rc = pClockMgr->SetClockParent(clockHandle, parentClockHandle);
    return (OK == rc) ? MODS_TRUE : MODS_FALSE;
}

//------------------------------------------------------------------------------
MODS_EXPORT MODS_BOOL ModsDrvClockGetParent
(
    UINT64 clockHandle,
    UINT64 *pParentClockHandle
)
{
    Xp::ClockManager* const pClockMgr = Xp::GetClockManager();
    if (!pClockMgr)
    {
        return MODS_FALSE;
    }
    const RC rc = pClockMgr->GetClockParent(clockHandle, pParentClockHandle);
    return (OK == rc) ? MODS_TRUE : MODS_FALSE;
}

//------------------------------------------------------------------------------
MODS_EXPORT MODS_BOOL ModsDrvClockReset
(
    UINT64 clockHandle,
    MODS_BOOL assert
)
{
    Xp::ClockManager* const pClockMgr = Xp::GetClockManager();
    if (!pClockMgr)
    {
        return MODS_FALSE;
    }
    const RC rc = pClockMgr->Reset(clockHandle, assert == MODS_TRUE);
    return (OK == rc) ? MODS_TRUE : MODS_FALSE;
}

//------------------------------------------------------------------------------
MODS_EXPORT MODS_BOOL ModsDrvGpioEnable
(
    UINT32 bit,
    MODS_BOOL enable
)
{
    if (!Platform::IsTegra())
    {
        return MODS_FALSE;
    }

    RC rc;
    if (enable == MODS_TRUE)
    {
        rc = SysUtil::Gpio::Enable(bit);
    }
    else
    {
        rc = SysUtil::Gpio::Disable(bit);
    }
    return (OK == rc) ? MODS_TRUE : MODS_FALSE;
}

//------------------------------------------------------------------------------
MODS_EXPORT MODS_BOOL ModsDrvGpioRead
(
    UINT32 bit,
    UINT08* pValue
)
{
    if (!Platform::IsTegra())
    {
        return MODS_FALSE;
    }

    UINT32 v = 0;
    const RC rc = SysUtil::Gpio::Read(bit, &v);
    if (OK == rc)
    {
        *pValue = static_cast<UINT08>(v);
    }
    return (OK == rc) ? MODS_TRUE : MODS_FALSE;
}

//------------------------------------------------------------------------------
MODS_EXPORT MODS_BOOL ModsDrvGpioWrite
(
    UINT32 bit,
    UINT08 value
)
{
    if (!Platform::IsTegra())
    {
        return MODS_FALSE;
    }

    const RC rc = SysUtil::Gpio::Write(bit, value);
    return (OK == rc) ? MODS_TRUE : MODS_FALSE;
}

//------------------------------------------------------------------------------
MODS_EXPORT MODS_BOOL ModsDrvGpioSetDirection
(
    UINT32 bit,
    ModsGpioDir dir
)
{
    if (!Platform::IsTegra())
    {
        return MODS_FALSE;
    }

    RC rc;
    if (dir == MODS_GPIO_DIR_INPUT)
    {
        rc = SysUtil::Gpio::DirectionInput(bit);
    }
    else
    {
        rc = SysUtil::Gpio::DirectionOutput(bit, 0);
    }
    return (OK == rc) ? MODS_TRUE : MODS_FALSE;
}

//------------------------------------------------------------------------------
MODS_EXPORT MODS_BOOL ModsDrvI2CRead
(
    UINT32 port,
    UINT08 address,
    const void* pOutputBuffer,
    UINT32 outputSize,
    void* pInputBuffer,
    UINT32 inputSize
)
{
    return MODS_FALSE;
}

//------------------------------------------------------------------------------
MODS_EXPORT MODS_BOOL ModsDrvI2CWrite
(
    UINT32 port,
    UINT08 address,
    const void* pOutputBuffer0,
    UINT32 outputSize0,
    const void* pOutputBuffer1,
    UINT32 outputSize1
)
{
    return MODS_FALSE;
}

MODS_EXPORT MODS_RC ModsDrvTegraSetGpuVolt
(
    UINT32 requestedVoltageInMicroVolts,
    UINT32* pActualVoltageInMicroVolts
)
{
    MASSERT(pActualVoltageInMicroVolts);
#if defined(INCLUDE_GPU)
    return CheetAh::SocPtr()->SetGpuVolt(requestedVoltageInMicroVolts,
                                       pActualVoltageInMicroVolts);
#else
    return RC::UNSUPPORTED_FUNCTION;
#endif
}

MODS_EXPORT MODS_RC ModsDrvTegraGetGpuVolt
(
    UINT32 * pVoltageInMicroVolts
)
{
    MASSERT(pVoltageInMicroVolts);
#if defined(INCLUDE_GPU)
    return CheetAh::SocPtr()->GetGpuVolt(pVoltageInMicroVolts);
#else
    return RC::UNSUPPORTED_FUNCTION;
#endif
}

MODS_EXPORT MODS_RC ModsDrvTegraGetGpuVoltInfo
(
    MODS_TEGRA_GPU_VOLT_INFO * pVoltageInfo
)
{
    MASSERT(pVoltageInfo);
#if defined(INCLUDE_GPU)
    return CheetAh::SocPtr()->GetGpuVoltInfo(
            &pVoltageInfo->milwoltageuV,
            &pVoltageInfo->maxVoltageuV,
            &pVoltageInfo->stepVoltageuV);
#else
    return RC::UNSUPPORTED_FUNCTION;
#endif
}

//! Retrieves chip info on T124 and up.
MODS_EXPORT MODS_RC ModsDrvTegraGetChipInfo
(
    UINT32 * pGpuSpeedoHv,
    UINT32 * pGpuSpeedoLv,
    UINT32 * pGpuIddq,
    UINT32 * pChipSkuId
)
{
#if defined(INCLUDE_GPU)
    return CheetAh::SocPtr()->GetGpuChipInfo(pGpuSpeedoHv, pGpuSpeedoLv, pGpuIddq, pChipSkuId);
#else
    return RC::UNSUPPORTED_FUNCTION;
#endif
}

MODS_EXPORT UINT32 ModsDrvGetFakeProcessID()
{
#if defined(INCLUDE_GPU)
    return RMFakeProcess::GetFakeProcessID();
#else
    return 0;
#endif
}

MODS_EXPORT MODS_RC ModsDrvGetFbConsoleInfo
(
    PHYSADDR * pBaseAddress,
    UINT64   * pSize
)
{
    return Xp::GetFbConsoleInfo(pBaseAddress, pSize);
}

MODS_EXPORT MODS_RC ModsDrvSetupDmaBase
(
    UINT32                  domain,
    UINT32                  bus,
    UINT32                  device,
    UINT32                  func,
    ModsDrvPpcTceBypassMode mode,
    UINT64                  devDmaMask,
    PHYSADDR *              pDmaBase
)
{
    MASSERT(pDmaBase);

    Platform::PpcTceBypassMode bypassMode;
    switch (mode)
    {
        case MODSDRV_PPC_TCE_BYPASS_DEFAULT:
            bypassMode = Platform::PPC_TCE_BYPASS_DEFAULT;
            break;
        case MODSDRV_PPC_TCE_BYPASS_FORCE_ON:
            bypassMode = Platform::PPC_TCE_BYPASS_FORCE_ON;
            break;
        case MODSDRV_PPC_TCE_BYPASS_FORCE_OFF:
            bypassMode = Platform::PPC_TCE_BYPASS_FORCE_OFF;
            break;
        default:
            ModsDrvPrintf(Tee::PriHigh, "Error: Unknown bypass mode %d\n", mode);
            return RC::SOFTWARE_ERROR;
    }

    return Platform::SetupDmaBase(domain, bus, device, func, bypassMode, devDmaMask, pDmaBase);
}

//! Get the DMA base address (0 for everything but PPC)
MODS_EXPORT PHYSADDR ModsDrvGetGpuDmaBaseAddress(UINT32 gpuInstance)
{
    PHYSADDR dmaBaseAddress = static_cast<PHYSADDR>(0);
#if defined(INCLUDE_GPU)
    GpuDevMgr* const pGpuDevMgr =
        static_cast<GpuDevMgr*>(DevMgrMgr::d_GraphDevMgr);
    if (pGpuDevMgr)
    {
        GpuSubdevice *pSubdev = pGpuDevMgr->GetSubdeviceByGpuInst(gpuInstance);
        dmaBaseAddress = pSubdev->GetDmaBaseAddress();
    }
#endif
    return dmaBaseAddress;
}

MODS_EXPORT MODS_RC ModsDrvGetPpcTceBypassMode(UINT32 gpuInstance, ModsDrvPpcTceBypassMode *pBypassMode)
{
    RC rc;
    MASSERT(pBypassMode);
    *pBypassMode = MODSDRV_PPC_TCE_BYPASS_DEFAULT;
#if defined(INCLUDE_GPU)
    GpuDevMgr* const pGpuDevMgr =
        static_cast<GpuDevMgr*>(DevMgrMgr::d_GraphDevMgr);
    if (pGpuDevMgr)
    {
        GpuSubdevice *pSubdev = pGpuDevMgr->GetSubdeviceByGpuInst(gpuInstance);

        // If the property doesnt exist then it is not relevant
        if (!pSubdev->HasNamedProperty("PpcTceBypassMode"))
            return rc;

        GpuSubdevice::NamedProperty *pPpcTceBypassMode;
        UINT32 nPpcTceBypassMode = 0;
        CHECK_RC(pSubdev->GetNamedProperty("PpcTceBypassMode", &pPpcTceBypassMode));
        CHECK_RC(pPpcTceBypassMode->Get(&nPpcTceBypassMode));

        *pBypassMode = static_cast<ModsDrvPpcTceBypassMode>(nPpcTceBypassMode);
    }
#endif
    return rc;
}
MODS_EXPORT MODS_RC ModsDrvGetAtsTargetAddressRange
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
    return Platform::GetAtsTargetAddressRange(gpuInst, pAddr, pAddrGuest,
                                              pAddrWidth, pMask, pMaskWidth,
                                              pGranularity, bIsPeer, peerIndex);
}

MODS_EXPORT MODS_RC ModsDrvGetLwLinkLineRate
(
    UINT32 domain,
    UINT32 bus,
    UINT32 device,
    UINT32 func,
    UINT32 *pSpeed
)
{
    return Platform::GetLwLinkLineRate(domain, bus, device, func, pSpeed);
}

MODS_EXPORT MODS_RC ModsDrvGetForcedLwLinkConnections
(
    UINT32 gpuInst,
    UINT32 numLinks,
    UINT32 *pLinkArray
)
{
    return Platform::GetForcedLwLinkConnections(gpuInst, numLinks, pLinkArray);
}

MODS_EXPORT MODS_RC ModsDrvGetForcedC2CConnections
(
    UINT32 gpuInst,
    UINT32 numLinks,
    UINT32 *pLinkArray
)
{
    return Platform::GetForcedC2CConnections(gpuInst, numLinks, pLinkArray);
}

namespace
{
    UINT32 ModsDrvProtectToMemoryProtect(UINT32 protect)
    {
        UINT32 modsProtect = Memory::ProtectDefault;
        if (protect & PROTECT_READABLE)
        {
            modsProtect |= Memory::Readable;
            protect &= ~PROTECT_READABLE;
        }
        if (protect & PROTECT_WRITEABLE)
        {
            modsProtect |= Memory::Writeable;
            protect &= ~PROTECT_WRITEABLE;
        }
        if (protect & PROTECT_EXELWTABLE)
        {
            modsProtect |= Memory::Exelwtable;
            protect &= ~PROTECT_EXELWTABLE;
        }
        if (protect)
        {
            ModsDrvPrintf(Tee::PriHigh,
                   "ModsDrvProtectToMemoryProtect : Unknown protect bits 0x%08x\n", protect);
            return Memory::ProtectIlwalid;
        }
        return modsProtect;
    }
}

MODS_EXPORT MODS_RC ModsDrvVirtualAlloc(void **ppReturnedAddr, void *pBase, size_t size, UINT32 protect, UINT32 vaFlags)
{
    UINT32 modsProtect = ModsDrvProtectToMemoryProtect(protect);

    if (modsProtect == Memory::ProtectIlwalid)
        return RC::BAD_PARAMETER;

    UINT32 modsVaFlags   = Memory::VirualAllocDefault;
    if (vaFlags & MODSDRV_VAF_SHARED)
    {
        modsVaFlags |= Memory::Shared;
        vaFlags &= ~MODSDRV_VAF_SHARED;
    }
    if (vaFlags & MODSDRV_VAF_ANONYMOUS)
    {
        modsVaFlags |= Memory::Anonymous;
        vaFlags &= ~MODSDRV_VAF_ANONYMOUS;
    }
    if (vaFlags & MODSDRV_VAF_FIXED)
    {
        modsVaFlags |= Memory::Fixed;
        vaFlags &= ~MODSDRV_VAF_FIXED;
    }
    if (vaFlags & MODSDRV_VAF_PRIVATE)
    {
        modsVaFlags |= Memory::Private;
        vaFlags &= ~MODSDRV_VAF_PRIVATE;
    }
    if (vaFlags)
    {
        ModsDrvPrintf(Tee::PriHigh,
               "ModsDrvVirtualAlloc failed: Unknown virtual allocation bits 0x%08x\n", vaFlags);
        return RC::BAD_PARAMETER;
    }
    return Platform::VirtualAlloc(ppReturnedAddr, pBase, size, modsProtect, modsVaFlags);
}

MODS_EXPORT void *ModsDrvVirtualFindFreeVaInRange(size_t size, void *pBase, void *pLimit, size_t align)
{
    return Platform::VirtualFindFreeVaInRange(size, pBase, pLimit, align);
}

MODS_EXPORT MODS_RC ModsDrvVirtualFree(void *pAddr, size_t size, ModsDrvVirtualFreeType freeType)
{
    Memory::VirtualFreeMethod vfMethod = Memory::VirtualFreeDefault;
    switch (freeType)
    {
        case MODSDRV_VFT_DECOMMIT:
            vfMethod = Memory::Decommit;
            break;
        case MODSDRV_VFT_RELEASE:
            vfMethod = Memory::Release;
            break;
        default:
            ModsDrvPrintf(Tee::PriHigh,
                   "ModsDrvVirtualFree failed: Unknown freeType %d\n", freeType);
            return RC::BAD_PARAMETER;
    }
    return Platform::VirtualFree(pAddr, size, vfMethod);
}

MODS_EXPORT MODS_RC ModsDrvVirtualProtect(void *pAddr, size_t size, UINT32 protect)
{
    UINT32 modsProtect = ModsDrvProtectToMemoryProtect(protect);
    if (modsProtect == Memory::ProtectIlwalid)
        return RC::BAD_PARAMETER;
    return Platform::VirtualProtect(pAddr, size, modsProtect);
}

MODS_EXPORT MODS_RC ModsDrvVirtualMadvise(void *pAddr, size_t size, ModsDrvMadviseType advice)
{
    Memory::VirtualAdvice modsAdvice = Memory::VirtualAdviceDefault;
    switch (advice)
    {
        case MODSDRV_MT_NORMAL:
            modsAdvice = Memory::Normal;
            break;
        case MODSDRV_MT_FORK:
            modsAdvice = Memory::Fork;
            break;
        case MODSDRV_MT_DONTFORK:
            modsAdvice = Memory::DontFork;
            break;
        default:
            ModsDrvPrintf(Tee::PriHigh,
                   "ModsDrvMadvise failed: Unknown advice %d\n", advice);
            return RC::BAD_PARAMETER;
    }
    return Platform::VirtualMadvise(pAddr, size, modsAdvice);
}

MODS_EXPORT MODS_BOOL ModsDrvIsLwSwitchPresent(void)
{
#if defined(INCLUDE_GPU)
    TestDeviceMgr* pTestDeviceMgr = static_cast<TestDeviceMgr*>(DevMgrMgr::d_TestDeviceMgr);
    if (pTestDeviceMgr)
    {
        if (pTestDeviceMgr->NumDevicesType(TestDevice::TYPE_LWIDIA_LWSWITCH) > 0)
            return MODS_TRUE;
    }
#endif
    return MODS_FALSE;
}

MODS_EXPORT MODS_RC ModsDrvSetLwLinkSysmemTrained
(
    UINT32    domain,
    UINT32    bus,
    UINT32    device,
    UINT32    func,
    MODS_BOOL trained
)
{
#if defined(INCLUDE_GPU)
    return Platform::SetLwLinkSysmemTrained(domain, bus, device, func, trained != MODS_FALSE);
#else
    return RC::UNSUPPORTED_FUNCTION;
#endif
}

MODS_EXPORT MODS_BOOL ModsDrvAllocSharedSysmem
(
    const char *name,
    UINT64      id,
    size_t      size,
    MODS_BOOL   readOnly,
    void      **ppAddr
)
{
    RC rc = SharedSysmem::Alloc(name, id, size, readOnly != MODS_FALSE, ppAddr);
    return (rc == OK) ? MODS_TRUE : MODS_FALSE;
}

MODS_EXPORT MODS_BOOL ModsDrvImportSharedSysmem
(
    UINT32      pid,
    const char *name,
    UINT64      id,
    MODS_BOOL   readOnly,
    void      **ppAddr,
    size_t     *pSize
)
{
    RC rc = SharedSysmem::Import(pid, name, id, readOnly != MODS_FALSE,
                                 ppAddr, pSize);
    return (rc == OK) ? MODS_TRUE : MODS_FALSE;
}

MODS_EXPORT MODS_BOOL ModsDrvFreeSharedSysmem(void *pAddr)
{
    return (SharedSysmem::Free(pAddr) == OK) ? MODS_TRUE : MODS_FALSE;
}

MODS_EXPORT MODS_BOOL ModsDrvFindSharedSysmem
(
    const char *cmp,
    UINT32      pid,
    char       *name,
    size_t     *pNameSize,
    UINT64     *pId
)
{
    string strName(name, *pNameSize);
    strName = strName.c_str();
    RC rc = SharedSysmem::Find(cmp, pid, &strName, pId);
    if (rc != OK)
        return MODS_FALSE;
    strncpy(name, strName.c_str(), *pNameSize);
    *pNameSize = strName.size();
    return MODS_TRUE;
}

MODS_EXPORT MODS_BOOL ModsDrvAllocPagesIlwPR
(
    size_t numBytes,
    UINT64 *physAddr,
    void **sysMemDesc,
    UINT64 *sysMemOffset
)
{
#if defined(INCLUDE_GPU)
    GpuDevMgr* const pGpuDevMgr =
        static_cast<GpuDevMgr*>(DevMgrMgr::d_GraphDevMgr);

    if (pGpuDevMgr)
    {
        GpuDevice *gpuDevice = pGpuDevMgr->GetFirstGpuDevice();
        MASSERT(gpuDevice);

        ProtectedRegionManager *protMan;
        if (OK == gpuDevice->GetProtectedRegionManager(&protMan))
        {
            MASSERT(protMan);

            if (OK == protMan->ReserveVprMem(
                    numBytes,
                    physAddr,
                    sysMemDesc,
                    sysMemOffset))
            {
                return MODS_TRUE;
            }
        }
    }
#endif
    return MODS_FALSE;
}

#if defined(INCLUDE_MDIAG) && defined(SIM_BUILD)
void* MdiagTestApiGetContext();
#endif

MODS_EXPORT void* ModsDrvTestApiGetContext()
{
#if defined(INCLUDE_MDIAG) && defined(SIM_BUILD)
    return MdiagTestApiGetContext();
#else
    return nullptr;
#endif
}

MODS_EXPORT MODS_RC ModsDrvReadPFPciConfigIlwF
(
    UINT32 addr,
    UINT32 *data
)
{
    // Clear before use
    *data = 0;

#if defined(INCLUDE_GPU)
    if (Platform::IsVirtFunMode())
    {
        VmiopElwManager *pMgr      = VmiopElwManager::Instance();
        UINT32          pfDomain   = ~0;
        UINT32          pfBus      = ~0;
        UINT32          pfDevice   = ~0;
        UINT32          pfFunction = ~0;
        RC              rc;

        MASSERT(pMgr);

        // Get the BDF for the PF (gfid = 0).
        CHECK_RC(pMgr->GetDomainBusDevFun(0, &pfDomain, &pfBus, &pfDevice,
                                          &pfFunction));
        ModsDrvPrintf(Tee::PriLow,
                      "PF domain:0x%x bus:0x%x device:0x%x func:0x%x\n",
                       pfDomain, pfBus, pfDevice, pfFunction);

        CHECK_RC(Platform::PciRead32(pfDomain, pfBus, pfDevice, pfFunction,
                                      addr, data));

        return RC::OK;
    }
#endif

    return RC::UNSUPPORTED_FUNCTION;
}

MODS_EXPORT MODS_RC ModsDrvReadTarFile
(
    const char* tarfilename,
    const char* filename,
    UINT32* pSize,
    char* pData
)
{
    RC rc;

    CHECK_RC(Utility::ReadTarFile(tarfilename, filename, pSize, pData));

    return RC::OK;
}


MODS_EXPORT ModsOperatingSystem ModsDrvGetOperatingSystem()
{
    return static_cast<ModsOperatingSystem>(Xp::GetOperatingSystem());
}
