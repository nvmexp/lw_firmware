/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2009-2022 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#include "lwdawrap.h"
#include "gpu/include/gpudev.h"
#include "core/include/massert.h"
#include "core/include/utility.h"
#include "core/include/zlib.h"
#include "core/include/abort.h"
#include "core/include/jscript.h"
#include "core/include/platform.h"
#include "core/include/fileholder.h"
#include "core/include/regexp.h"
#include "core/include/xp.h"
#include "core/include/version.h"
#include "gpu/utility/surf2d.h"
#include <algorithm>
#include <functional>

typedef UINT64 U64;

#ifndef USE_LWDA_SYSTEM_LIB
#include "../mods/inc/lwdamods.h"
#include "lwda_etbl/tools_rm.h"
#include "lwda_etbl/cluster_prototype.h"
#else
#include "gpu/pcie/pcieimpl.h"
#define LW_INIT_UUID
#include "lwBlockLinear.h"
#include "lwda_etbl/tools_device.h"
#include "lwda_etbl/video.h"
#endif

#ifdef INCLUDE_LWDART
#include "lwda_runtime_api.h"
#endif

namespace Lwca {
    // Keep track of the number of times we have tried to initialize
    // the LWCA runtime and LWCA driver APIs
    Tasker::Mutex s_pRefCountMutex =
        Tasker::CreateMutex("LwdartMods", Tasker::mtxUnchecked);
    struct CtxInitData
    {
        UINT64 activeRefCount; // Active reference count for the primary context on a device
        bool   bLwdaRuntimeInitialized;
    };
    map<LWdevice, CtxInitData> s_CtxInitData;

    // Set flags for the primary context if not yet set
    constexpr UINT32 g_ContextFlags = LW_CTX_SCHED_BLOCKING_SYNC;
    RC PrimaryCtxSetFlags(Device dev)
    {
        // Set flags for the primary context. If the primary context was already created
        // that means we are running conlwrrent LWCA tests, so ignore the failure.
        LWresult err = lwDevicePrimaryCtxSetFlags(dev.GetHandle(), g_ContextFlags);
        if (err != LWDA_SUCCESS && err != LWDA_ERROR_PRIMARY_CONTEXT_ACTIVE)
        {
            PrintResult("lwDevicePrimaryCtxSetFlags", err);
            return RC::LWDA_CONTEXT_CREATE_FAILED;
        }
        return RC::OK;
    }

    RC FreeLwdaRuntimeInternal(const Device& dev)
    {
#ifdef INCLUDE_LWDART
        Tasker::MutexHolder lock(s_pRefCountMutex);
        // Only free the device if there are no outstanding uses of the primary context
        // (In BOTH the LWCA runtime API and the normal LWCA driver API)
        if (s_CtxInitData[dev.GetHandle()].bLwdaRuntimeInitialized &&
            !s_CtxInitData[dev.GetHandle()].activeRefCount)
        {
            s_CtxInitData[dev.GetHandle()].bLwdaRuntimeInitialized = false;

            // Set LWCA runtime device
            lwdaError_t result = lwdaSetDevice(dev.GetHandle());
            if (result != lwdaSuccess)
            {
                Printf(Tee::PriError,
                       "Unable to set LWCA device: %s\n", lwdaGetErrorString(result));
                return RC::LWDA_ERROR;
            }

            // Reset LWCA runtime device (no other tests should be using it at this point)
            result = lwdaDeviceReset();
            if (result != lwdaSuccess)
            {
                Printf(Tee::PriError,
                       "lwdaDeviceReset FAILURE: %s\n", lwdaGetErrorString(result));
                return RC::LWDA_ERROR;
            }
        }
#endif
        return RC::OK;
    }

#ifdef INCLUDE_LWDART
    // Use this function to init the LWCA runtime, to ensure proper
    // interoperability between the LWCA runtime and LWCA driver APIs
    RC InitLwdaRuntime(const Device& dev)
    {
        RC rc;
        Tasker::MutexHolder lock(s_pRefCountMutex);

        // Always increment reference counter
        // It's fine if doesn't init properly, as Lwca::FreeLwdart works regardless
        s_CtxInitData[dev.GetHandle()].activeRefCount++;
        s_CtxInitData[dev.GetHandle()].bLwdaRuntimeInitialized = true;

        // Set primary context flags if needed
        CHECK_RC(PrimaryCtxSetFlags(dev));

        // Set LWCA runtime device
        lwdaError_t result = lwdaSetDevice(dev.GetHandle());
        if (result != lwdaSuccess)
        {
            Printf(Tee::PriError,
                   "Unable to set LWCA device: %s\n", lwdaGetErrorString(result));
            return RC::LWDA_ERROR;
        }

        // Force initialize LWCA runtime if not already initialized
        result = lwdaFree(0);
        if (result != lwdaSuccess)
        {
            Printf(Tee::PriError,
                   "Unable to initialize LWCA Runtime: %s\n", lwdaGetErrorString(result));
            return RC::LWDA_ERROR;
        }

        return rc;
    }

    // Use this function to teardown the LWCA runtime, to ensure proper
    // interoperability between the LWCA runtime and LWCA driver APIs
    RC FreeLwdaRuntime(const Device& dev)
    {
        Tasker::MutexHolder lock(s_pRefCountMutex);

        if (s_CtxInitData[dev.GetHandle()].bLwdaRuntimeInitialized)
        {
            s_CtxInitData[dev.GetHandle()].activeRefCount--;
            return FreeLwdaRuntimeInternal(dev);
        }
        return RC::OK;
    }
#endif

    const string GetLwdaErrorDesc(LWresult result)
    {
        const char* pNameStr = nullptr;
        const char* pDescStr = nullptr;
        if (lwGetErrorName(result, &pNameStr) != LWDA_SUCCESS)
        {
            return "Unrecognized LWCA error code";
        }
        if (lwGetErrorString(result, &pDescStr) != LWDA_SUCCESS)
        {
            return "Unrecognized LWCA error code";
        }

        return string(pNameStr) + ": " + pDescStr;
    }

    LWresult PrintResult(const char* function, LWresult result)
    {
        if (result != LWDA_SUCCESS)
        {
            // Strip function arguments from printout
            string funname(function);
            const size_t paren = funname.find('(');
            if (paren != string::npos)
            {
                funname.resize(paren);
            }
            Printf(Tee::PriError, "%s returned error %d (%s)\n",
                funname.c_str(), static_cast<int>(result),
                GetLwdaErrorDesc(result).c_str());
        }
        return result;
    }

    RC LwdaErrorToRC(LWresult result)
    {
        switch (result)
        {
            case LWDA_SUCCESS:
                return OK;
            case LWDA_ERROR_OUT_OF_MEMORY:
                return RC::CANNOT_ALLOCATE_MEMORY;
            case LWDA_ERROR_FILE_NOT_FOUND:
                return RC::FILE_DOES_NOT_EXIST;
            case LWDA_ERROR_LAUNCH_TIMEOUT:
                return RC::TIMEOUT_ERROR;
            case LWDA_ERROR_NO_BINARY_FOR_GPU:
                return RC::LWDA_NO_BINARY;
            default:
                return RC::LWDA_ERROR;
        }
    }
}

namespace {

    const char lwdaBin[] = "lwca.bin";
    const char lwdaInternalBin[] = "lwca.INTERNAL.bin";
    const char lwbinExt[] = ".lwbin";

    bool g_LwdaInSys = false;

    Lwca::Stream g_IlwalidStream;

    class MakeCtxLwrrent
    {
    public:
        MakeCtxLwrrent
        (
            const Lwca::Instance* pInst,
            LWcontext             ctx
        ) : m_bCtxSet(false)
        {
            LWcontext lwrCtx = 0;
            lwCtxGetLwrrent(&lwrCtx);
            if (lwrCtx != ctx)
            {
                const LWresult err = LW_RUN(lwCtxPushLwrrent(ctx));
                if (err == LWDA_SUCCESS)
                    m_bCtxSet = true;
            }
        }

        MakeCtxLwrrent
        (
            const Lwca::Instance* pInst,
            Lwca::Device          dev
        ) : MakeCtxLwrrent(pInst, pInst->GetHandle(dev)) { }

        ~MakeCtxLwrrent()
        {
            if (m_bCtxSet)
            {
                LWcontext oldCtx;
                LW_RUN(lwCtxPopLwrrent(&oldCtx));
            }
        }

        MakeCtxLwrrent(const MakeCtxLwrrent&)            = delete;
        MakeCtxLwrrent& operator=(const MakeCtxLwrrent&) = delete;

    private:
        bool m_bCtxSet;
    };
} // anonymous namespace

Lwca::Stream& Lwca::Stream::operator=(Stream&& stream)
{
    Free();
    m_Instance          = stream.m_Instance;
    m_Dev               = stream.m_Dev;
    m_Stream            = stream.m_Stream;
    stream.m_Stream     = 0;
    stream.m_Dev        = Device();
    stream.m_Instance   = nullptr;
    return *this;
}

RC Lwca::Stream::InitCheck() const
{
    if (!m_Stream)
    {
        Printf(Tee::PriError, "LWstream not initialized\n");
        return RC::SOFTWARE_ERROR;
    }
    return OK;
}

void Lwca::Stream::Free()
{
    if (m_Stream)
    {
        MakeCtxLwrrent setCtx(m_Instance, m_Dev);
        LW_RUN(lwStreamDestroy(m_Stream));
        m_Instance = 0;
        m_Stream = 0;
    }
}

RC Lwca::Stream::Synchronize() const
{
    RC rc;
    CHECK_RC(InitCheck());
    MakeCtxLwrrent setCtx(m_Instance, m_Dev);
    const LWresult err = LW_RUN(lwStreamSynchronize(m_Stream));
    return LwdaErrorToRC(err);
}

RC Lwca::Stream::Synchronize(unsigned timeout_sec) const
{
    RC rc;
    CHECK_RC(InitCheck());
    Event event(CreateEvent());
    CHECK_RC(event.Record());
    return event.Synchronize(timeout_sec);
}

Lwca::Event Lwca::Stream::CreateEvent() const
{
    MASSERT(m_Stream);
    if (!m_Stream)
    {
        Printf(Tee::PriError, "LWstream not initialized\n");
        return Event();
    }

    MakeCtxLwrrent setCtx(m_Instance, m_Dev);

    LWevent event;
    LWresult err = LW_RUN(lwEventCreate(&event, LW_EVENT_DEFAULT));
    if (err != LWDA_SUCCESS)
    {
        return Event();
    }

    return Event(m_Instance, m_Dev, m_Stream, event);
}

Lwca::Instance& Lwca::Instance::operator=(Instance&& instance)
{
    Free();
    m_Init              = instance.m_Init;
    m_AutoSync          = instance.m_AutoSync;
    m_DeviceInfo        = move(instance.m_DeviceInfo);
    m_Lwbins            = move(instance.m_Lwbins);
    m_InternalLwbins    = move(instance.m_InternalLwbins);
    m_LwbinsInitialized = instance.m_LwbinsInitialized;
    m_UseDefaultContext = instance.m_UseDefaultContext;
    m_NumSecondaryCtx   = instance.m_NumSecondaryCtx;
    m_ActiveContextIdx  = instance.m_ActiveContextIdx;
    instance.m_Init     = false;
    return *this;
}

RC Lwca::Instance::Init()
{
    if (!m_Init)
    {
        LWresult err = LW_RUN(lwInit(0));
        if (err != LWDA_SUCCESS)
        {
            return RC::LWDA_INIT_FAILED;
        }
    }

    if (!m_LwbinsInitialized)
    {
        string fullPath = Utility::DefaultFindFile(lwdaBin, true);
        if (!m_Lwbins.ReadFromFile(fullPath, true))
        {
            Printf(Tee::PriError, "Failed to load %s\n", lwdaBin);
            return RC::FILE_DOES_NOT_EXIST;
        }

        fullPath = Utility::DefaultFindFile(lwdaInternalBin, true);

        if ((Utility::GetSelwrityUnlockLevel() >= Utility::SUL_LWIDIA_NETWORK) &&
            (Xp::DoesFileExist(fullPath)) && (!m_InternalLwbins.ReadFromFile(fullPath, true)))
        {
            Printf(Tee::PriError, "Failed to load %s\n", lwdaInternalBin);
            return RC::FILE_DOES_NOT_EXIST;
        }

        m_LwbinsInitialized = true;
    }
    m_Init = true;
    return OK;
}

//------------------------------------------------------------------------------
// Set the sub-context state for the LWCA layer. This API should be called after
// Lwca::Instance::Init() and before Lwca::Instance::InitContext()
// Acceptable values for state are: 0=disable sub-context, 1=enable sub-context
// Note:
// Only dynamic (not static) partitioning of sub-contexts is supported right now.
RC Lwca::Instance::SetSubcontextState(Device dev, UINT32 state)
{
#ifndef USE_LWDA_SYSTEM_LIB
    const Lwca::Capability cap = dev.GetCapability();
    if (cap >= Lwca::Capability::SM_70)
    {
        UINT32 subctxState = lwMODSGetSubcontextState();
        if (subctxState != state)
        {
            lwMODSSetSubcontextState(state);
        }
        subctxState = lwMODSGetSubcontextState();
        MASSERT(subctxState == state);
    }
    else
    {
        return RC::UNSUPPORTED_FUNCTION;
    }
    return OK;
#else
    return RC::UNSUPPORTED_FUNCTION;
#endif
}

//-----------------------------------------------------------------------------
RC Lwca::Instance::GetSubcontextState(Device dev, UINT32 *pState)
{
#ifndef USE_LWDA_SYSTEM_LIB
    const Lwca::Capability cap = dev.GetCapability();
    if (cap >= Lwca::Capability::SM_70)
    {
        *pState = lwMODSGetSubcontextState();
    }
    else
    {
        return RC::UNSUPPORTED_FUNCTION;
    }
    return OK;
#else
    return RC::UNSUPPORTED_FUNCTION;
#endif
}

void Lwca::Instance::Free()
{
    // Need to manually free each device info because the PerDevInfo eventually winds
    // up iterating through m_DeviceInfo itself during destruction
    for (auto & lwrDevInfo : m_DeviceInfo)
    {
        lwrDevInfo.second.Free();
    }
    m_DeviceInfo.clear();

    // Ideally we should call lwiFinalize here, but:
    // 1) We can't call lwiFinalize() here because we create a race
    //    condition see bugs 444597 & 439077. Instead we will call it in
    //    GpuDevMgr::ShutdownAll().
    // 2) We shouldn't actually clean it up because two LWCA tests
    //    could be running simultaneously and lwiFinalize() would have
    //    catastrophic effects on the test which would finish second.
    // 3) Init is typically called in BindGpuDevice while Free is typically
    //    called in Cleanup. But a test can be run multiple times after
    //    the device is bound, hence we should not uninitialize the LWCA
    //    instance in Cleanup.
}

RC Lwca::Instance::InitCheck(Device dev) const
{
    RC rc;
    CHECK_RC(dev.InitCheck());
    if (!m_Init || (m_DeviceInfo.find(dev) == m_DeviceInfo.end()))
    {
        MASSERT(!"Context not initialized\n");
        return RC::SOFTWARE_ERROR;
    }
    return OK;
}

bool Lwca::Instance::IsContextInitialized(Device dev) const
{
    MASSERT(dev.IsValid());
    return m_Init && (m_DeviceInfo.find(dev) != m_DeviceInfo.end());
}

bool Lwca::Instance::AssertValid(Device dev) const
{
    MASSERT(dev.IsValid());
    if (!dev.IsValid())
    {
        Printf(Tee::PriError, "Context not initialized\n");
        return false;
    }

    MASSERT(m_DeviceInfo.find(dev) != m_DeviceInfo.end());
    if (m_DeviceInfo.find(dev) == m_DeviceInfo.end())
    {
        Printf(Tee::PriError, "Context not initialized\n");
        return false;
    }

    return true;
}

Lwca::Device Lwca::Instance::GetDevice(int ordinal) const
{
    if (!m_Init)
    {
        Printf(Tee::PriWarn, "LWCA not initialized\n");
        return Device();
    }

    LWdevice dev = Device::IlwalidId;
    if (LW_RUN(lwDeviceGet(&dev, ordinal)) != LWDA_SUCCESS)
    {
        Printf(Tee::PriWarn, "Failed to get device at ordinal %d.\n",
               ordinal);
    }

    return Device(dev);
}

RC Lwca::Instance::FindDevice(GpuDevice& gpuDev, Device* retDev) const
{
    if (!m_Init)
    {
        Printf(Tee::PriError, "LWCA not initialized\n");
        return RC::SOFTWARE_ERROR;
    }

    const UINT32 gpuInst = gpuDev.GetSubdevice(0)->GetGpuInst();

    int count = 0;
    if (LW_RUN(lwDeviceGetCount(&count)) != LWDA_SUCCESS)
    {
        return RC::LWDA_DEVICE_GET_FAILED;
    }

    if (gpuDev.GetSubdevice(0)->IsSOC())
    {
        // Go through list of devices using LWCA driver API and return the first detected iGPU
        // (Assume that the first detected iGPU is also the only iGPU)
        for (int ordinal=0; ordinal < count; ordinal++)
        {
            LWdevice dev;
            int isIntegrated;
            if (LW_RUN(lwDeviceGet(&dev, ordinal)) == LWDA_SUCCESS
             && LW_RUN(lwDeviceGetAttribute(&isIntegrated, LW_DEVICE_ATTRIBUTE_INTEGRATED, dev)) == LWDA_SUCCESS
             && isIntegrated)
            {
                *retDev = Device(dev);
                Printf(Tee::PriLow, "Found LWCA device \"%s\" with GPU instance %u\n",
                    retDev->GetName().c_str(), gpuInst);
                return RC::OK;
            }
        }
    }
    else
    {
        UINT16 gpuPciDom = ~0;
        UINT16 gpuPciBus = ~0;
        UINT16 gpuPciDev = ~0;
        gpuDev.GetSubdevice(0)->GetId().GetPciDBDF(&gpuPciDom, &gpuPciBus, &gpuPciDev);

        for (int ordinal=0; ordinal < count; ordinal++)
        {
            LWdevice dev;
            if (LW_RUN(lwDeviceGet(&dev, ordinal)) != LWDA_SUCCESS)
            {
                continue;
            }

            // For now, disregarding PCI function works for SR-IOV setups LWCA driver API in
            // subordinate MODS processes show up with PCI function 0. There may be some issues
            // later down the line with SMC or SMC/SR-IOV setups.
            // See https://p4sw-swarm.lwpu.com/reviews/28873929 for the dislwssion.
            int lwdaBus;
            int lwdaDev;
            int lwdaDom;
            if (LW_RUN(lwDeviceGetAttribute(&lwdaDom, LW_DEVICE_ATTRIBUTE_PCI_DOMAIN_ID, dev)) != LWDA_SUCCESS
             || LW_RUN(lwDeviceGetAttribute(&lwdaBus, LW_DEVICE_ATTRIBUTE_PCI_BUS_ID,    dev)) != LWDA_SUCCESS
             || LW_RUN(lwDeviceGetAttribute(&lwdaDev, LW_DEVICE_ATTRIBUTE_PCI_DEVICE_ID, dev)) != LWDA_SUCCESS)
            {
                continue;
            }

            if (lwdaDom == gpuPciDom && lwdaBus == gpuPciBus && lwdaDev == gpuPciDev)
            {
                *retDev = Device(dev);
                Printf(Tee::PriLow, "Found LWCA device \"%s\" with GPU instance %u\n",
                    retDev->GetName().c_str(), gpuInst);
                return RC::OK;
            }
        }
    }

    return RC::LWDA_DEVICE_GET_FAILED;
}

unsigned Lwca::Instance::GetDeviceCount() const
{
    if (!m_Init)
    {
        Printf(Tee::PriWarn, "LWCA not initialized\n");
        return 0;
    }

    int count;
    if (LW_RUN(lwDeviceGetCount(&count)) != LWDA_SUCCESS)
    {
        return 0;
    }

    return count;
}

RC Lwca::Instance::GetMemInfo(Device dev, size_t *pFreeMem, size_t *pTotalMem) const
{
    RC rc;
    CHECK_RC(InitCheck());

    MakeCtxLwrrent setCtx(this, dev);
    const LWresult err = LW_RUN(lwMemGetInfo(pFreeMem, pTotalMem));
    return LwdaErrorToRC(err);
}

RC Lwca::Instance::InitContext(Device dev, int numSecondaryCtx, int activeContextIdx)
{
    RC rc;

    CHECK_RC(dev.InitCheck());

    // If the device is valid, then the instance must have been initialized.
    MASSERT(m_Init);

    if (m_DeviceInfo.find(dev) != m_DeviceInfo.end())
    {
        Printf(Tee::PriError, "Unable to initialize LWCA context twice.\n");
        return RC::SOFTWARE_ERROR;
    }

    const Lwca::Capability cap = dev.GetCapability();

    if (numSecondaryCtx)
    {
        //TODO: We get LWDA_OUT_OF_MEMORY errors on emulation when trying to create more than
        //      2 secondary contexts. I need to see if this is still and issue on real silicon
        //      that uses a different memory management system.
        if ((cap >= Lwca::Capability::SM_70) && (numSecondaryCtx < 64))
        {
            m_NumSecondaryCtx = numSecondaryCtx;
        }
        else
        {
            Printf(Tee::PriError, "Creating %d secondary contexts is not supported.\n",
                   numSecondaryCtx);
            MASSERT(!"Too many contexts specified.");
            return RC::SOFTWARE_ERROR;
        }
    }
    if (activeContextIdx > numSecondaryCtx )
    {
        Printf(Tee::PriError, "Invalid activeContexIdx of %d when numSecondaryCtx = %d\n",
               activeContextIdx, numSecondaryCtx);
        return RC::SOFTWARE_ERROR;
    }

    // Force sysmem
    if (g_LwdaInSys)
    {
#ifndef USE_LWDA_SYSTEM_LIB
        Printf(Tee::PriLow, "Forcing LWCA into sysmem.\n");
        LW_RUN(lwMODSForceSysmem(dev.GetHandle()));
#else
        Printf(Tee::PriError, "Unable to force LWCA into sysmem when using non-MODS LWCA library.\n");
        return RC::UNSUPPORTED_FUNCTION;
#endif
    }

    CHECK_RC(Lwca::PrimaryCtxSetFlags(dev));

    // Create/Set primary context
    //
    // Increment the reference count so that tests using the LWCA runtime
    // can teardown properly
    LWresult err;
    LWcontext primaryCtx = 0;
    {
        Tasker::MutexHolder lock(s_pRefCountMutex);
        err = lwDevicePrimaryCtxRetain(&primaryCtx, dev.GetHandle());
        if (err == LWDA_ERROR_PRIMARY_CONTEXT_ACTIVE)
            err = LWDA_SUCCESS;
        if (err != LWDA_SUCCESS)
        {
            PrintResult("lwDevicePrimaryCtxRetain", err);
            return RC::LWDA_CONTEXT_CREATE_FAILED;
        }
        s_CtxInitData[dev.GetHandle()].activeRefCount++;
    }

    // Primary context needs to be pushed before the stream can be created.
    err = LW_RUN(lwCtxPushLwrrent(primaryCtx));
    if (err != LWDA_SUCCESS)
    {
        return RC::LWDA_CONTEXT_CREATE_FAILED;
    }
    {
        DEFER //make sure this gets called for all conditions
        {
            lwCtxPopLwrrent(&primaryCtx);
        };

        m_ActiveContextIdx = 0;

        // The destructor of PerDevInfo reverts the effects of lwDevicePrimaryCtxRetain()
        PerDevInfo devInfo(dev, this);
        devInfo.m_Contexts.push_back(primaryCtx);

        // Calling CreateStreamInternal() below requires the device to be present in the map
        m_DeviceInfo.insert(make_pair(dev, move(devInfo)));

        const auto pDevInfo = m_DeviceInfo.find(dev);
        MASSERT(pDevInfo != m_DeviceInfo.end());

        // Create default stream
        Stream stream = CreateStreamInternal(dev);
        if (!stream.IsValid())
        {
            rc = stream.InitCheck();
            m_DeviceInfo.erase(dev);
            return rc;
        }
        pDevInfo->second.m_Streams.push_back(move(stream));

    }

    const auto pDevInfo = m_DeviceInfo.find(dev);
    MASSERT(pDevInfo != m_DeviceInfo.end());

    for (int ii = 1; ii <= m_NumSecondaryCtx; ii++)
    {
        LWcontext secondaryCtx = 0;
        err = LW_RUN(lwCtxCreate(&secondaryCtx, Lwca::g_ContextFlags, dev.GetHandle()));
        if (err == LWDA_SUCCESS)
        {
            DEFER //this needs to get called in all cases
            {
                LW_RUN(lwCtxPopLwrrent(&secondaryCtx));
            };
            m_ActiveContextIdx = ii;

            pDevInfo->second.m_Contexts.push_back(secondaryCtx);
            // Create secondary stream
            pDevInfo->second.m_Streams.push_back(CreateStreamInternal(dev));
            if (!pDevInfo->second.m_Streams[ii].IsValid())
            {
                rc = pDevInfo->second.m_Streams[ii].InitCheck();
                break;
            }
        }
        else
        {
            rc = LwdaErrorToRC(err);
            break;
        }
    }

    if (err != LWDA_SUCCESS || rc != OK)
    {
        // we were unable to create the proper number of secondary contexts
        // try to put the LWCA layer in a good state for teardown
        m_ActiveContextIdx = 0;
    }
    else
    {
        m_ActiveContextIdx = activeContextIdx;
    }

    return rc;
}

RC Lwca::Instance::InitContext(GpuDevice* pGpuDev, int numSecondaryCtx, int activeContext)
{
    RC rc;
    CHECK_RC(Init());
    Device dev;
    CHECK_RC(FindDevice(*pGpuDev, &dev));
    CHECK_RC(InitContext(dev, numSecondaryCtx, activeContext));
    return OK;
}

RC Lwca::Instance::SwitchContext(const Device& dev, int ctxIdx)
{
    if (m_ActiveContextIdx == ctxIdx)
        return OK;

    map<Device, PerDevInfo>::const_iterator pDevInfo = m_DeviceInfo.find(dev);
    if (pDevInfo == m_DeviceInfo.end())
    {
        Printf(Tee::PriError, "LWCA device not found!\n");
        MASSERT(!"LWCA device not found!");
        return RC::SOFTWARE_ERROR;
    }

    if (ctxIdx >= static_cast<int>(pDevInfo->second.m_Contexts.size()))
    {
        Printf(Tee::PriError, "LWCA context %d not found!\n", ctxIdx);
        MASSERT(!"LWCA context not found!");
        return RC::SOFTWARE_ERROR;
    }

    m_ActiveContextIdx = ctxIdx;

    return OK;
}

Lwca::Instance::PerDevInfo::~PerDevInfo()
{
    Free();
}

void Lwca::Instance::PerDevInfo::Free()
{
    if (m_Dev.IsValid())
    {
        LWcontext oldCtx;
        m_Tools.Unload();
        for (size_t ii = m_Contexts.size()-1; ii > 0; ii--)
        {
            lwCtxPushLwrrent(m_Contexts[ii]);
            m_Streams[ii].Free();
            lwCtxPopLwrrent(&oldCtx);
            LW_RUN(lwCtxDestroy(m_Contexts[ii]));
        }
        m_Inst->m_ActiveContextIdx = 0;
        lwCtxPushLwrrent(m_Contexts[0]);
        m_Streams[0].Free();
        lwCtxPopLwrrent(&oldCtx);

        // Release the context and decrement the refcount so that
        // tests using the LWCA runtime can teardown properly
        {
            Tasker::MutexHolder lock(s_pRefCountMutex);
            s_CtxInitData[m_Dev.GetHandle()].activeRefCount--;
            LW_RUN(lwDevicePrimaryCtxRelease(m_Dev.GetHandle()));
            FreeLwdaRuntimeInternal(m_Dev);
        }
        m_Dev.Free();
    }
}

Lwca::Device Lwca::Instance::GetLwrrentDevice() const
{
    if (m_DeviceInfo.empty())
    {
        Printf(Tee::PriError, "Context not initialized\n");
        return Device();
    }

    if (m_DeviceInfo.size() > 1)
    {
        Printf(Tee::PriWarn, "Current device ambiguous with multiple devices\n");
        return Device();
    }

    return m_DeviceInfo.begin()->first;
}

RC Lwca::Instance::LoadModule(Device dev, const char* name, Module* pModule) const
{
    MASSERT(pModule);
    RC rc;

    if (!AssertValid(dev))
    {
        Printf(Tee::PriError, "Invalid LWCA device\n");
        return RC::SOFTWARE_ERROR;
    }

    MASSERT(name);
    MASSERT(name[0]);

    Printf(Tee::PriLow, "Loading LWCA module %s\n", name);

    vector<char> lwbinBuffer;

    string nameStr(name);
    if (nameStr.substr(nameStr.size() - (sizeof(lwbinExt)-1)) == lwbinExt)
    {
        // Allow override in MODS runspace
        const string fullFileName = Utility::DefaultFindFile(name, true);
        if ((Utility::GetSelwrityUnlockLevel() >= Utility::SUL_LWIDIA_NETWORK) &&
            Xp::DoesFileExist(fullFileName))
        {
            Printf(Tee::PriWarn, "Using override for %s from MODS runspace!\n", name);
            FileHolder file;
            if (file.Open(fullFileName, "rb") != RC::OK)
            {
                Printf(Tee::PriError, "Could not find or open %s\n", name);
                return RC::LWDA_NO_BINARY;
            }
            fseek(file.GetFile(), 0, SEEK_END);
            const long len = ftell(file.GetFile());
            if (len <= 0)
            {
                Printf(Tee::PriError, "%s is empty\n", name);
                return RC::LWDA_NO_BINARY;
            }
            lwbinBuffer.resize(len);
            fseek(file.GetFile(), 0, SEEK_SET);
            const long numRead = static_cast<long>(fread(&lwbinBuffer[0], 1, len, file.GetFile()));
            if (numRead != len)
            {
                return RC::FILE_READ_ERROR;
            }
        }
        else
        {
            const TarFile* const pFile = GetLwbin(name);
            if (!pFile)
            {
                Printf(Tee::PriError, "Could not find or open %s\n", name);
                return RC::LWDA_NO_BINARY;
            }
            const unsigned size = pFile->GetSize();
            if (size == 0)
            {
                Printf(Tee::PriError, "%s is empty\n", name);
                return RC::LWDA_NO_BINARY;
            }
            lwbinBuffer.resize(size);
            pFile->Seek(0);
            const unsigned numRead = pFile->Read(&lwbinBuffer[0], size);
            if (numRead != size)
            {
                return RC::FILE_READ_ERROR;
            }
        }
    }
    else
    {
        MASSERT(!"Only Uncompressed modules with .lwbin extension are supported!");
        return RC::SOFTWARE_ERROR;
    }

    CHECK_RC(Utility::DumpProgram(".lwbin", &lwbinBuffer[0], lwbinBuffer.size()));

    lwbinBuffer.push_back(0);

    MakeCtxLwrrent setCtx(this, dev);

    LWmodule module;
    LWresult err = LW_RUN(lwModuleLoadData(&module, &lwbinBuffer[0]));
    if (err != LWDA_SUCCESS)
    {
        Printf(Tee::PriError, "LWCA failed to load module\n");
        return LwdaErrorToRC(err);
    }

    *pModule = Module(this, dev, module);
    return rc;
}

bool Lwca::Instance::IsLwdaModuleSupported
(
    const char* nameBegin
)const
{
    return IsLwdaModuleSupported(GetLwrrentDevice(), nameBegin);
}

bool Lwca::Instance::IsLwdaModuleSupported
(
    Device dev,
    const char* nameBegin
) const
{
    if (!AssertValid(dev))
    {
        return false;
    }
    const Lwca::Capability cap = dev.GetCapability();

    vector<string> names;
    m_Lwbins.FindFileNames(nameBegin, lwbinExt, &names);

    vector<string> internalNames;
    m_InternalLwbins.FindFileNames(nameBegin, lwbinExt, &internalNames);
    names.insert(names.begin(), internalNames.begin(), internalNames.end());

    sort(names.begin(), names.end(), greater<string>());

    vector<string>::const_iterator it = names.begin();

    for (; it != names.end(); ++it)
    {
        const string& name = *it;
        const size_t dotPos = name.size() - (sizeof(lwbinExt)-1);
        MASSERT(name.size() > dotPos && dotPos > 2 && name[dotPos] == '.');
        const int major = name[dotPos-2] - '0';
        const int minor = name[dotPos-1] - '0';
        if (cap.MajorVersion() == major &&
            cap.MinorVersion() >= minor)
        {
            return true;
        }
    }
    return false;
}

RC Lwca::Instance::LoadNewestSupportedModule
(
    Device dev,
    const char* nameBegin,
    Lwca::Module* pModule
) const
{
    MASSERT(pModule);

    if (!AssertValid(dev))
    {
        Printf(Tee::PriError, "Invalid LWCA device\n");
        return RC::SOFTWARE_ERROR;
    }

    const Lwca::Capability cap = dev.GetCapability();

    vector<string> names;
    RegExp re;
    if (OK != re.Set(string(nameBegin) + "[0-9]{2}" + string(lwbinExt), RegExp::FULL))
    {
        Printf(Tee::PriError, "Unsupported module name format\n");
        return RC::SOFTWARE_ERROR;
    }
    m_Lwbins.FindFileNames(re, &names);

    vector<string> internalNames;
    m_InternalLwbins.FindFileNames(re, &internalNames);
    names.insert(names.begin(), internalNames.begin(), internalNames.end());

    sort(names.begin(), names.end(), greater<string>());

    vector<string>::const_iterator it = names.begin();
    for (; it != names.end(); ++it)
    {
        const string& name = *it;
        const size_t dotPos = name.size() - (sizeof(lwbinExt)-1);
        MASSERT(name.size() > dotPos && dotPos > 2 && name[dotPos] == '.');
        const int major = name[dotPos-2] - '0';
        const int minor = name[dotPos-1] - '0';
        if (cap.MajorVersion() == major &&
            cap.MinorVersion() >= minor)
        {
            Printf(Tee::PriDebug, "Loading lwbin: %s\n", name.c_str());
            return LoadModule(dev, name.c_str(), pModule);
        }
    }

    Printf(Tee::PriError,
           "Missing module \"%s%d%d.lwbin\"\n",
           nameBegin, cap.MajorVersion(), cap.MinorVersion());
    return RC::UNSUPPORTED_DEVICE;
}

RC Lwca::Instance::AllocDeviceMem(UINT64 size, Lwca::DeviceMemory* pOut) const
{
    MASSERT(pOut);
    return AllocDeviceMem(GetLwrrentDevice(), size, pOut);
}

RC Lwca::Instance::AllocDeviceMem
(
    Device dev,
    UINT64 size,
    Lwca::DeviceMemory* pOut
) const
{
    MASSERT(pOut);

    if (!AssertValid(dev))
    {
        Printf(Tee::PriError, "Invalid LWCA device\n");
        return RC::SOFTWARE_ERROR;
    }

    MASSERT(size > 0);
    if (size == 0)
    {
        Printf(Tee::PriError, "Allocation size must be greater than 0\n");
        return RC::ILWALID_ARGUMENT;
    }

    MakeCtxLwrrent setCtx(this, dev);

    LWdeviceptr ptr;
    LWresult err = LW_RUN(lwMemAlloc(&ptr, size));
    if (err != LWDA_SUCCESS)
    {
        Printf(Tee::PriError, "Failed to allocate device memory\n");

        size_t freeMem;
        size_t totalMem;
        if (GetMemInfo(dev, &freeMem, &totalMem) != OK)
        {
            Printf(Tee::PriError, "Failed to get device memory information\n");
        }
        else
        {
            Printf(Tee::PriError, "Memory Information:\n"
                   "\tRequested:\t%llu bytes\n"
                   "\tAvailable:\t%zu bytes\n"
                   "\tTotal memory:\t%zu bytes\n",
                   size, freeMem, totalMem);
        }

        return LwdaErrorToRC(err);
    }

    *pOut = DeviceMemory(this, dev, ptr, size, 0);
    return OK;
}

Lwca::Array Lwca::Instance::AllocArray
(
    Device         dev,
    unsigned       width,
    unsigned       height,
    unsigned       depth,
    LWarray_format format,
    unsigned       channels,
    unsigned       flags
) const
{
    if (!AssertValid(dev))
    {
        return Array();
    }

    MASSERT(width);
    MASSERT(channels);
    MASSERT(channels <= 4);

    if ((width == 0) ||
        (channels == 0) ||
        (channels > 4))
    {
        return Array();
    }

    unsigned bytesPerTexel = 0;

    switch (format)
    {
        case LW_AD_FORMAT_UNSIGNED_INT8:
        case LW_AD_FORMAT_SIGNED_INT8:
            bytesPerTexel = channels;
            break;

        case LW_AD_FORMAT_UNSIGNED_INT16:
        case LW_AD_FORMAT_SIGNED_INT16:
        case LW_AD_FORMAT_HALF:
            bytesPerTexel = 2 * channels;
            break;

        case LW_AD_FORMAT_UNSIGNED_INT32:
        case LW_AD_FORMAT_SIGNED_INT32:
        case LW_AD_FORMAT_FLOAT:
            bytesPerTexel = 4 * channels;
            break;

        default:
            MASSERT(!"Unrecognized array format");
            return Array();
    }

    LWDA_ARRAY3D_DESCRIPTOR desc;
    desc.Width       = width;
    desc.Format      = format;
    desc.NumChannels = channels;
    desc.Flags       = flags;

    // 1D layered arrays have height equal to zero
    if (flags == LWDA_ARRAY3D_LAYERED && depth == 0)
    {
        desc.Height = depth;
        desc.Depth  = height;
    }
    else
    {
        desc.Height = height;
        desc.Depth  = depth;
    }

    MakeCtxLwrrent setCtx(this, dev);

    LWarray array;
    if (LW_RUN(lwArray3DCreate(&array, &desc)) != LWDA_SUCCESS)
    {
        return Array();
    }

    return Array(this, dev, array, width, height, depth, bytesPerTexel, flags);
}

RC Lwca::Instance::AllocHostMem(Device dev,
                                size_t size,
                                UINT32 flags,
                                Lwca::HostMemory* pOut) const
{
    MASSERT(pOut);

    if (!AssertValid(dev))
    {
        Printf(Tee::PriError, "Invalid LWCA device\n");
        return RC::SOFTWARE_ERROR;
    }

    MASSERT(size > 0);
    if (size == 0)
    {
        Printf(Tee::PriError, "Allocation size must be greater than 0\n");
        return RC::ILWALID_ARGUMENT;
    }

    void* hostPtr = nullptr;
    MakeCtxLwrrent setCtx(this, dev);
    LWresult err = LW_RUN(lwMemHostAlloc(&hostPtr, size, flags));
    if (err != LWDA_SUCCESS)
    {
        Printf(Tee::PriError, "Failed to allocate host memory\n");
        return LwdaErrorToRC(err);
    }

    *pOut = HostMemory(this, dev, hostPtr, size);
    return OK;
}

RC Lwca::Instance::AllocSurfaceClientMem
(
    UINT32              sizeBytes,
    Memory::Location    loc,
    GpuSubdevice*       pGpuSubdev,
    Surface2D*          pSurf,
    ClientMemory*       pClientMem
) const
{
    MASSERT(pSurf);
    MASSERT(pClientMem);
    MASSERT(pGpuSubdev);

    RC rc;
    pSurf->SetWidth(sizeBytes / sizeof(UINT32));
    pSurf->SetHeight(1);
    pSurf->SetColorFormat(ColorUtils::VOID32);
    pSurf->SetLocation(loc);
    pSurf->SetLayout(Surface2D::Layout::Pitch);
    LwRmPtr pLwRm;
    CHECK_RC(pSurf->Alloc(pGpuSubdev->GetParentDevice(), pLwRm.Get()));
    CHECK_RC(pSurf->Map(pGpuSubdev->GetSubdeviceInst(), pLwRm.Get()));
    *pClientMem = MapClientMem(pSurf->GetMemHandle(), pGpuSubdev->GetSubdeviceInst(),
                               sizeBytes, pGpuSubdev->GetParentDevice(), loc);
    return rc;
}

Lwca::ClientMemory Lwca::Instance::MapClientMem
(
    UINT32           memHandle,
    UINT64           offset,
    UINT64           size,
    GpuDevice*       pGpuDevice,
    Memory::Location loc
) const
{
    return MapClientMem(GetLwrrentDevice(), memHandle, offset, size, pGpuDevice, loc);
}

Lwca::ClientMemory Lwca::Instance::MapClientMem
(
    Device           dev,
    UINT32           memHandle,
    UINT64           offset,
    UINT64           size,
    GpuDevice*       pGpuDevice,
    Memory::Location loc
) const
{
#ifndef USE_LWDA_SYSTEM_LIB
    if (!AssertValid(dev))
    {
        return ClientMemory();
    }

    MASSERT(size > 0);
    if (size == 0)
    {
        return ClientMemory();
    }

    MakeCtxLwrrent setCtx(this, dev);

    LwRmPtr  pLwRm;
    UINT64   ptr;
    LWresult result = LWDA_ERROR_UNKNOWN;
    switch (loc)
    {
        case Memory::Fb:
            result = LW_RUN(lwMODSAllocResourceMemory64(
                        &ptr,
                        pLwRm->GetClientHandle(),
                        pLwRm->GetDeviceHandle(pGpuDevice),
                        memHandle,
                        offset,
                        size));
            break;

        case Memory::Coherent:
        case Memory::NonCoherent:
            result = LW_RUN(lwMODSAllocSystemMemory64(
                        &ptr,
                        pLwRm->GetClientHandle(),
                        pLwRm->GetDeviceHandle(pGpuDevice),
                        memHandle,
                        offset,
                        size));
            break;

        default:
            MASSERT(!"Unsupported memory location");
            break;
    }

    if (result != LWDA_SUCCESS)
    {
        return ClientMemory();
    }

    return ClientMemory(this, dev, ptr, size);
#else
    return ClientMemory();
#endif
}

Lwca::Stream Lwca::Instance::CreateStream(Device dev) const
{
    if (!AssertValid(dev))
    {
        return Stream();
    }

    MakeCtxLwrrent setCtx(this, dev);

    return CreateStreamInternal(dev);
}

Lwca::Stream Lwca::Instance::CreateStreamInternal(Device dev) const
{
    LWstream stream;
    if (LW_RUN(lwStreamCreate(&stream, LW_STREAM_NON_BLOCKING)) != LWDA_SUCCESS)
    {
        return Stream();
    }

    return Stream(this, dev, stream);
}

const Lwca::Stream& Lwca::Instance::GetStream(Device dev) const
{
    if (!AssertValid(dev))
    {
        return g_IlwalidStream;
    }
    return m_DeviceInfo.find(dev)->second.m_Streams[m_ActiveContextIdx];
}

RC Lwca::Instance::SynchronizeContext() const
{
    return SynchronizeContext(GetLwrrentDevice());
}

RC Lwca::Instance::SynchronizeContext(Device dev) const
{
    RC rc;
    CHECK_RC(InitCheck(dev));
    MakeCtxLwrrent setCtx(this, dev);
    const LWresult err = LW_RUN(lwCtxSynchronize());
    return LwdaErrorToRC(err);
}

RC Lwca::Instance::Synchronize() const
{
    return Synchronize(GetLwrrentDevice());
}

RC Lwca::Instance::Synchronize(Device dev) const
{
    RC rc;
    CHECK_RC(InitCheck(dev));
    MakeCtxLwrrent setCtx(this, dev);
    const LWresult err = LW_RUN(lwStreamSynchronize(GetStream(dev).GetHandle()));
    return LwdaErrorToRC(err);
}

RC Lwca::Instance::Synchronize(unsigned timeout_sec) const
{
    return Synchronize(GetLwrrentDevice(), timeout_sec);
}

RC Lwca::Instance::Synchronize(Device dev, unsigned timeout_sec) const
{
    RC rc;
    CHECK_RC(InitCheck(dev));
    const Event event(CreateEvent(dev));
    CHECK_RC(event.Record());
    return event.Synchronize(timeout_sec);
}

namespace
{
    struct SynchronizePollArgs
    {
        const Lwca::Instance* pInstance;
        Lwca::Device          dev;
        bool                  synchronizeDone;
    };
    void SynchronizeThread(void* args)
    {
        Tasker::Yield();
        SynchronizePollArgs* pollArgs = static_cast<SynchronizePollArgs*>(args);
        MakeCtxLwrrent setCtx(pollArgs->pInstance, pollArgs->dev);
        lwCtxSynchronize();
        pollArgs->synchronizeDone = true;
    }
    bool SynchronizePollThread(void* args)
    {
        SynchronizePollArgs* pollArgs = static_cast<SynchronizePollArgs*>(args);
        return pollArgs->synchronizeDone;
    }
}

RC Lwca::Instance::SynchronizePoll(unsigned timeoutMs) const
{
    return SynchronizePoll(GetLwrrentDevice(), timeoutMs);
}

RC Lwca::Instance::SynchronizePoll(Device dev, unsigned timeoutMs) const
{
    RC rc;
    CHECK_RC(InitCheck(dev));

    SynchronizePollArgs pollArgs = {0};
    pollArgs.pInstance       = this;
    pollArgs.dev             = dev;
    pollArgs.synchronizeDone = false;

    Tasker::ThreadID syncThread = Tasker::CreateThread(SynchronizeThread,
                                                       &pollArgs,
                                                       Tasker::MIN_STACK_SIZE,
                                                       "SyncThread");
    if (OK == (rc = POLLWRAP_HW(SynchronizePollThread, &pollArgs, timeoutMs)))
        Tasker::Join(syncThread, timeoutMs);
    // If the pollthread timed out, then it isn't safe to join the thread
    // because it might hang
    return rc;
}

RC Lwca::Instance::GetComputeEngineChannel(UINT32 *phClient,
                                           UINT32 *phChannel)
{
    return GetComputeEngineChannel(GetLwrrentDevice(), phClient, phChannel);
}

RC Lwca::Instance::GetComputeEngineChannel(Device dev,
                                           UINT32 *phClient,
                                           UINT32 *phChannel)
{
#ifndef USE_LWDA_SYSTEM_LIB
    MASSERT(phClient);
    MASSERT(phChannel);

    *phClient  = 0;
    *phChannel = 0;

    const LWetblToolsRm *pLwRMInfo = NULL;

    RC rc;
    CHECK_RC(InitCheck(dev));
    MakeCtxLwrrent setCtx(this, dev);
    LWresult err = LW_RUN(lwGetExportTable((const void **) &pLwRMInfo,
                                           (const LWuuid*)&LW_ETID_ToolsRm));
    CHECK_RC(LwdaErrorToRC(err));

    LwU32 RmClient;
    LwU32 RmChannel;
    err = LW_RUN(pLwRMInfo->GetComputeEngineChannel(&RmClient,
                                                    &RmChannel,
                                                    GetHandle(dev)));
    CHECK_RC(LwdaErrorToRC(err));

    *phClient = RmClient;
    *phChannel = RmChannel;
    return OK;
#else
    return RC::UNSUPPORTED_FUNCTION;
#endif
}

RC Lwca::Instance::GetComputeEngineChannelGroup(UINT32 *phChannel)
{
#ifndef USE_LWDA_SYSTEM_LIB
    Device dev = GetLwrrentDevice();
    MASSERT(phChannel);
    *phChannel = 0;
    const LWetblToolsRm *pLwRMInfo = NULL;

    RC rc;
    CHECK_RC(InitCheck(dev));
    MakeCtxLwrrent setCtx(this, dev);
    LWresult err = LW_RUN(lwGetExportTable((const void **) &pLwRMInfo,
                                           (const LWuuid*)&LW_ETID_ToolsRm));
    CHECK_RC(LwdaErrorToRC(err));

    LwU32 RmChannel;
    err = LW_RUN(pLwRMInfo->GetComputeEngineChannelGroup(&RmChannel,
                                                         GetHandle(dev)));
    CHECK_RC(LwdaErrorToRC(err));

    *phChannel = RmChannel;
    return OK;
#else
    return RC::UNSUPPORTED_FUNCTION;
#endif
}

RC Lwca::Instance::GetContextHandles(LWtoolsContextHandlesRm *pContextHandlesRm)
{
    return GetContextHandles(GetLwrrentDevice(), pContextHandlesRm);
}

RC Lwca::Instance::GetContextHandles
(
    Device dev,
    LWtoolsContextHandlesRm *pContextHandlesRm
)
{
#ifndef USE_LWDA_SYSTEM_LIB
    MASSERT(pContextHandlesRm);
    const LWetblToolsRm *pLwRMInfo = NULL;

    RC rc;
    CHECK_RC(InitCheck(dev));
    MakeCtxLwrrent setCtx(this, dev);
    LWresult err = LW_RUN(lwGetExportTable((const void **) &pLwRMInfo,
                                           (const LWuuid*)&LW_ETID_ToolsRm));
    CHECK_RC(LwdaErrorToRC(err));

    err = LW_RUN(pLwRMInfo->GetContextHandles(pContextHandlesRm, GetHandle(dev)));
    return LwdaErrorToRC(err);
#else
    return RC::UNSUPPORTED_FUNCTION;
#endif
}

const TarFile* Lwca::Instance::GetLwbin(const string& name) const
{
    const TarFile *pTarFile = m_InternalLwbins.Find(name);
    if (pTarFile == nullptr)
        pTarFile = m_Lwbins.Find(name);
    return pTarFile;
}

LWcontext Lwca::Instance::GetHandle(Device dev) const
{
    if (!AssertValid(dev))
    {
        // NULL is considered "no context" for LWCA
        return 0;
    }
    return m_DeviceInfo.find(dev)->second.m_Contexts[m_ActiveContextIdx];
}

RC Lwca::Instance::EnablePeerAccess(Device dev, Device peerDev) const
{
    RC rc;
    CHECK_RC(InitCheck(dev));
    CHECK_RC(InitCheck(peerDev));

    int canAccessPeer = 0;
    MakeCtxLwrrent setCtx(this, dev);

    LWresult err = LW_RUN(lwDeviceCanAccessPeer(&canAccessPeer,
                                                dev.GetHandle(),
                                                peerDev.GetHandle()));
    CHECK_RC(LwdaErrorToRC(err));

    if (!canAccessPeer)
    {
        Printf(Tee::PriError, "%s: Incapable of accessing peer device \n", __FUNCTION__);
        return RC::SOFTWARE_ERROR;
    }

    err = LW_RUN(lwCtxEnablePeerAccess(GetHandle(peerDev), 0));
    return LwdaErrorToRC(err);
}

RC Lwca::Instance::DisablePeerAccess(Device dev, Device peerDev) const
{
    RC rc;
    CHECK_RC(InitCheck(dev));
    CHECK_RC(InitCheck(peerDev));

    MakeCtxLwrrent setCtx(this, dev);
    LWresult err = lwCtxDisablePeerAccess(GetHandle(peerDev));
    if (err == LWDA_ERROR_PEER_ACCESS_NOT_ENABLED)
    {
        err = LWDA_SUCCESS;
    }
    else
    {
        PrintResult("lwCtxDisablePeerAccess", err);
    }
    return LwdaErrorToRC(err);
}

RC Lwca::Instance::EnableLoopbackAccess(Device dev) const
{
#ifndef USE_LWDA_SYSTEM_LIB
    RC rc;
    CHECK_RC(InitCheck(dev));

    auto & perDevInfo = m_DeviceInfo.find(dev)->second;
    if (perDevInfo.m_LoopbackRefCount)
        return RC::OK;

    MakeCtxLwrrent setCtx(this, dev);

    const LWresult err = LW_RUN(lwMODSDeviceEnableLoopback(dev.GetHandle()));
    rc = LwdaErrorToRC(err);
    if (rc == RC::OK)
        perDevInfo.m_LoopbackRefCount++;
    return rc;
#else
    return RC::UNSUPPORTED_FUNCTION;
#endif
}

RC Lwca::Instance::DisableLoopbackAccess(Device dev) const
{
#ifndef USE_LWDA_SYSTEM_LIB
    RC rc;
    CHECK_RC(InitCheck(dev));

    auto & perDevInfo = m_DeviceInfo.find(dev)->second;
    if (perDevInfo.m_LoopbackRefCount == 0)
        return RC::OK;

    perDevInfo.m_LoopbackRefCount--;
    if (perDevInfo.m_LoopbackRefCount != 0)
        return RC::OK;

    MakeCtxLwrrent setCtx(this, dev);

    const LWresult err = LW_RUN(lwMODSDeviceDisableLoopback(dev.GetHandle()));
    rc = LwdaErrorToRC(err);
    if (rc != RC::OK)
        perDevInfo.m_LoopbackRefCount = 1;
    return rc;
#else
    return RC::UNSUPPORTED_FUNCTION;
#endif
}

RC Lwca::Device::InitCheck() const
{
    if (m_Dev == IlwalidId)
    {
        Printf(Tee::PriError, "LWdevice not initialized\n");
        return RC::SOFTWARE_ERROR;
    }
    return OK;
}

string Lwca::Device::GetName() const
{
    if (m_Dev == IlwalidId)
    {
        Printf(Tee::PriWarn, "LWdevice not initialized\n");
        return "";
    }

    char name[MaxNameLength] = { };
    const LWresult result = lwDeviceGetName(name, MaxNameLength, m_Dev);
    if (result == LWDA_ERROR_UNKNOWN)
    {
        return "Unknown";
    }
    else
    {
        PrintResult("lwDeviceGetName", result);
    }

    return string(name);
}

Lwca::Capability Lwca::Device::GetCapability() const
{
    if (m_Dev == IlwalidId)
    {
        Printf(Tee::PriWarn, "LWdevice not initialized\n");
        return Capability(0, 0);
    }

    int maj, min;
    const LWresult errMaj = LW_RUN(lwDeviceGetAttribute(&maj, LW_DEVICE_ATTRIBUTE_COMPUTE_CAPABILITY_MAJOR, m_Dev));
    const LWresult errMin = LW_RUN(lwDeviceGetAttribute(&min, LW_DEVICE_ATTRIBUTE_COMPUTE_CAPABILITY_MINOR, m_Dev));
    if (errMaj != LWDA_SUCCESS || errMin != LWDA_SUCCESS)
    {
        return Capability(0, 0);
    }

    Printf(Tee::PriDebug, "Compute Capability: %d.%d\n", maj, min);

    return Capability(maj, min);
}

int Lwca::Device::GetAttribute(LWdevice_attribute attr) const
{
    if (m_Dev == IlwalidId)
    {
        Printf(Tee::PriWarn, "LWdevice not initialized\n");
        return 0;
    }

    int value;
    if (LW_RUN(lwDeviceGetAttribute(&value, attr, m_Dev)) != LWDA_SUCCESS)
    {
        return 0;
    }

    return value;
}

RC Lwca::Device::GetProperties(LWdevprop* prop) const
{
    RC rc;
    CHECK_RC(InitCheck());
    const LWresult err = LW_RUN(lwDeviceGetProperties(prop, m_Dev));
    return LwdaErrorToRC(err);
}

RC Lwca::Device::GetPropertiesSilent(LWdevprop* prop) const
{
    RC rc;
    CHECK_RC(InitCheck());
    const LWresult err = lwDeviceGetProperties(prop, m_Dev);
    return LwdaErrorToRC(err);
}

UINT32 Lwca::Device::GetShaderCount() const
{
    return GetAttribute(LW_DEVICE_ATTRIBUTE_MULTIPROCESSOR_COUNT);
}

Lwca::Event& Lwca::Event::operator=(Event&& event)
{
    Free();
    m_Instance    = event.m_Instance;
    m_Stream      = event.m_Stream;
    m_Dev         = event.m_Dev;
    m_Event       = event.m_Event;
    event.m_Event = 0;
    return *this;
}

RC Lwca::Event::InitCheck() const
{
    if (!m_Event)
    {
        Printf(Tee::PriError, "LWevent not initialized\n");
        return RC::SOFTWARE_ERROR;
    }
    return OK;
}

void Lwca::Event::Free()
{
    if (m_Event)
    {
        MakeCtxLwrrent setCtx(m_Instance, m_Dev);
        LW_RUN(lwEventDestroy(m_Event));
        m_Event = 0;
    }
}

RC Lwca::Event::Record() const
{
    RC rc;
    CHECK_RC(InitCheck());
    MakeCtxLwrrent setCtx(m_Instance, m_Dev);
    const LWresult err = LW_RUN(lwEventRecord(m_Event, m_Stream));
    return LwdaErrorToRC(err);
}

unsigned Lwca::Event::TimeMsElapsedSince(const Event& event) const
{
    return static_cast<unsigned>(TimeMsElapsedSinceF(event));
}

float Lwca::Event::TimeMsElapsedSinceF(const Event& event) const
{
    MASSERT(m_Event);
    if (!m_Event)
    {
        Printf(Tee::PriError, "LWevent not initialized\n");
        return 0.0F;
    }

    float elapsedMs = 0.0F;
    MakeCtxLwrrent setCtx(m_Instance, m_Dev);
    const LWresult err = lwEventElapsedTime(&elapsedMs, event.m_Event, m_Event);
    if (err == LWDA_SUCCESS)
    {
        return elapsedMs;
    }

    if (err != LWDA_ERROR_NOT_READY)
    {
        PrintResult("lwEventElapsedTime", err);
    }
    return 0.0F;
}

bool Lwca::Event::IsDone() const
{
    MASSERT(m_Event);
    if (!m_Event)
    {
        Printf(Tee::PriError, "LWevent not initialized\n");
        return true;
    }

    MakeCtxLwrrent setCtx(m_Instance, m_Dev);
    const LWresult err = lwEventQuery(m_Event);
    if (err == LWDA_SUCCESS)
    {
        return true;
    }

    if (err != LWDA_ERROR_NOT_READY)
    {
        PrintResult("lwEventQuery", err);
        return true;
    }
    return false;
}

RC Lwca::Event::Synchronize() const
{
    RC rc;
    CHECK_RC(InitCheck());
    MakeCtxLwrrent setCtx(m_Instance, m_Dev);
    const LWresult err = LW_RUN(lwEventSynchronize(m_Event));
    return LwdaErrorToRC(err);
}

namespace
{
    bool EventPoll(void* args)
    {
        const Lwca::Event* const pEvent = static_cast<Lwca::Event*>(args);
        return pEvent->IsDone();
    }
}

RC Lwca::Event::Synchronize(unsigned timeout_sec) const
{
    RC rc;
    CHECK_RC(InitCheck());
    return POLLWRAP_HW(EventPoll, const_cast<Event*>(this), timeout_sec*1000);
}

Lwca::Array& Lwca::Array::operator=(Array&& array)
{
    Free();
    m_Instance      = array.m_Instance;
    m_Dev           = array.m_Dev;
    m_Array         = array.m_Array;
    m_Width         = array.m_Width;
    m_Height        = array.m_Height;
    m_Depth         = array.m_Depth;
    m_BytesPerTexel = array.m_BytesPerTexel;
    m_Flags         = array.m_Flags;
    array.m_Array   = 0;
    return *this;
}

RC Lwca::Array::InitCheck() const
{
    if (!m_Array)
    {
        Printf(Tee::PriError, "LWarray not initialized\n");
        return RC::SOFTWARE_ERROR;
    }
    return OK;
}

void Lwca::Array::Free()
{
    if (m_Array)
    {
        MakeCtxLwrrent setCtx(m_Instance, m_Dev);
        LW_RUN(lwArrayDestroy(m_Array));
        m_Instance = 0;
        m_Array = 0;
        m_Width = 0;
        m_Height = 0;
        m_Depth = 0;
        m_BytesPerTexel = 0;
    }
}

RC Lwca::Array::Set(const void* data, unsigned pitch)
{
    RC rc;
    CHECK_RC(InitCheck());

    return Set(m_Instance->GetStream(m_Dev), data, pitch);
}

RC Lwca::Array::Set(const Stream& stream, const void* data, unsigned pitch)
{
    // Avoid scheduling more work if Ctrl-C was pressed
    RC rc;
    CHECK_RC(Abort::Check());

    CHECK_RC(InitCheck());
    CHECK_RC(stream.InitCheck());

    LWDA_MEMCPY3D copyParam;
    memset(&copyParam, 0, sizeof(copyParam));

    copyParam.srcMemoryType = LW_MEMORYTYPE_HOST;
    copyParam.srcHost = data;
    copyParam.srcPitch = pitch;

    copyParam.dstMemoryType = LW_MEMORYTYPE_ARRAY;
    copyParam.dstArray = m_Array;

    copyParam.WidthInBytes = m_Width * m_BytesPerTexel;

    // lwMemcpy3DAsync does not accept zero as a valid
    // dimension but lwArray3DCreate does so we need to
    // adjust that here
    if (m_Flags == LWDA_ARRAY3D_LAYERED && m_Depth == 0)
    {
        copyParam.Height = 1;
        copyParam.Depth = m_Height;
    }
    else
    {
        copyParam.Height = (m_Height != 0) ? m_Height : 1;
        copyParam.Depth = (m_Depth != 0) ? m_Depth : 1;
    }

    MakeCtxLwrrent setCtx(m_Instance, m_Dev);
    const LWresult err = LW_RUN(lwMemcpy3DAsync(&copyParam, stream.GetHandle()));
    if ((err == LWDA_SUCCESS) && m_Instance->IsAutoSyncEnabled())
    {
        CHECK_RC(stream.Synchronize());
    }
    return LwdaErrorToRC(err);
}

RC Lwca::Texture::InitCheck() const
{
    if (!m_TexRef)
    {
        Printf(Tee::PriError, "LWtexref not initialized\n");
        return RC::SOFTWARE_ERROR;
    }
    return OK;
}

RC Lwca::Texture::SetFlag(UINT32 flag)
{
    RC rc;
    CHECK_RC(InitCheck());
    MakeCtxLwrrent setCtx(m_Instance, m_Dev);
    const LWresult err = LW_RUN(lwTexRefSetFlags(m_TexRef, flag));
    return LwdaErrorToRC(err);
}

RC Lwca::Texture::SetFilterMode(LWfilter_mode mode)
{
    RC rc;
    CHECK_RC(InitCheck());
    MakeCtxLwrrent setCtx(m_Instance, m_Dev);
    const LWresult err = LW_RUN(lwTexRefSetFilterMode(m_TexRef, mode));
    return LwdaErrorToRC(err);
}

RC Lwca::Texture::SetAddressMode(LWaddress_mode mode, int dim)
{
    RC rc;
    CHECK_RC(InitCheck());
    MakeCtxLwrrent setCtx(m_Instance, m_Dev);
    const LWresult err = LW_RUN(lwTexRefSetAddressMode(m_TexRef, dim, mode));
    return LwdaErrorToRC(err);
}

RC Lwca::Texture::BindArray(const Array& a)
{
    RC rc;
    CHECK_RC(InitCheck());
    CHECK_RC(a.InitCheck());
    MakeCtxLwrrent setCtx(m_Instance, m_Dev);
    const LWresult err = LW_RUN(lwTexRefSetArray(
                m_TexRef, a.GetHandle(), LW_TRSA_OVERRIDE_FORMAT));
    return LwdaErrorToRC(err);
}

RC Lwca::Surface::InitCheck() const
{
    if (!m_SurfRef)
    {
        Printf(Tee::PriError, "LWsurfref not initialized\n");
        return RC::SOFTWARE_ERROR;
    }
    return OK;
}

RC Lwca::Surface::BindArray(const Array& a)
{
    RC rc;
    CHECK_RC(InitCheck());
    CHECK_RC(a.InitCheck());
    MakeCtxLwrrent setCtx(m_Instance, m_Dev);
    const LWresult err = LW_RUN(lwSurfRefSetArray(
                m_SurfRef, a.GetHandle(), 0));
    return LwdaErrorToRC(err);
}

RC Lwca::Function::InitCheck() const
{
    if (!m_Function)
    {
        Printf(Tee::PriError, "LWfunction not initialized\n");
        return RC::SOFTWARE_ERROR;
    }
    return OK;
}

void Lwca::Function::SetOptimalGridDim(const Instance& inst)
{
    m_GridWidth = m_Dev.GetShaderCount();
    m_GridHeight = 1;
}

RC Lwca::Function::GetSharedSize(unsigned *pBytes)
{
    RC rc;
    CHECK_RC(InitCheck());

    *pBytes = m_SharedMemSize;
    return rc;
}

RC Lwca::Function::SetSharedSize(unsigned bytes)
{
    RC rc;
    CHECK_RC(InitCheck());

    m_SharedMemSize = bytes;
    return rc;
}

RC Lwca::Function::SetSharedSizeMax()
{
    RC rc;
    CHECK_RC(InitCheck());
    MakeCtxLwrrent setCtx(m_Instance, m_Dev);

    // Allocate max possible shared memory for each block to fully occupy
    const INT32 maxSharedMemPerBlock =
        m_Dev.GetAttribute(LW_DEVICE_ATTRIBUTE_MAX_SHARED_MEMORY_PER_BLOCK_OPTIN);
    const INT32 staticSharedMem = GetAttribute(LW_FUNC_ATTRIBUTE_SHARED_SIZE_BYTES);
    const INT32 dynamicSharedMem = maxSharedMemPerBlock - staticSharedMem;

    CHECK_RC(SetAttribute(LW_FUNC_ATTRIBUTE_PREFERRED_SHARED_MEMORY_CARVEOUT,
                          LW_SHAREDMEM_CARVEOUT_MAX_SHARED));
    CHECK_RC(SetAttribute(LW_FUNC_ATTRIBUTE_MAX_DYNAMIC_SHARED_SIZE_BYTES,
                          dynamicSharedMem));
    CHECK_RC(SetSharedSize(dynamicSharedMem));

    return rc;
}

RC Lwca::Function::BindTexture(const Texture& tex)
{
    RC rc;
    CHECK_RC(InitCheck());
    CHECK_RC(tex.InitCheck());
    MakeCtxLwrrent setCtx(m_Instance, m_Dev);
    const LWresult err = LW_RUN(lwParamSetTexRef(m_Function, LW_PARAM_TR_DEFAULT, tex.GetHandle()));
    return LwdaErrorToRC(err);
}

int Lwca::Function::GetAttribute(LWfunction_attribute attr) const
{
    if (!m_Function)
    {
        Printf(Tee::PriWarn, "LWfunction not initialized\n");
        return 0;
    }

    int value;
    MakeCtxLwrrent setCtx(m_Instance, m_Dev);
    if (LW_RUN(lwFuncGetAttribute(&value, attr, m_Function)) != LWDA_SUCCESS)
    {
        return 0;
    }

    return value;
}

RC Lwca::Function::SetAttribute(LWfunction_attribute attr, int value)
{
    RC rc;
    CHECK_RC(InitCheck());

    MakeCtxLwrrent setCtx(m_Instance, m_Dev);
    LWresult err = LW_RUN(lwFuncSetAttribute(m_Function, attr, value));
    return LwdaErrorToRC(err);
}

RC Lwca::Function::SetClusterDim(unsigned clusterDimX, unsigned clusterDimY, unsigned clusterDimZ)
{
#if !defined(USE_LWDA_SYSTEM_LIB) && LWCFG(GLOBAL_ARCH_HOPPER)
    RC rc;
    CHECK_RC(InitCheck());
    MakeCtxLwrrent setCtx(m_Instance, m_Dev);

    // Get CGA cluster function table
    const LWetblClusterPrototype* pClusterTable = nullptr;
    CHECK_LW_RC(lwGetExportTable(reinterpret_cast<const void**>(&pClusterTable),
                                 reinterpret_cast<const LWuuid*>(&LW_ETID_ClusterPrototype)));
    MASSERT(pClusterTable);

    // Allow non-portable cluster sizes
    CHECK_LW_RC(pClusterTable->SetFunctionClusterNonPortableSizeSupport(m_Function, 1));

    // Explicitly enforce 1 CTA / SM (this is the default policy)
    CHECK_LW_RC(
        pClusterTable->SetFunctionClusterSchedulingPolicy(m_Function, LW_CLUSTER_SCHEDULING_POLICY_SPREAD)
    );

    // Configure cluster dimensions
    CHECK_LW_RC(
        pClusterTable->SetFunctionClusterDim(m_Function, clusterDimX, clusterDimY, clusterDimZ)
    );
    return rc;
#else
    return RC::UNSUPPORTED_FUNCTION;
#endif
}

RC Lwca::Function::SetMaximumClusterSize(unsigned clusterSize)
{
#if !defined(USE_LWDA_SYSTEM_LIB) && LWCFG(GLOBAL_ARCH_HOPPER)
    RC rc;
    CHECK_RC(InitCheck());
    MakeCtxLwrrent setCtx(m_Instance, m_Dev);

    const LWetblClusterPrototype* pClusterTable = nullptr;
    CHECK_LW_RC(lwGetExportTable(reinterpret_cast<const void**>(&pClusterTable), 
                                 reinterpret_cast<const LWuuid*>(&LW_ETID_ClusterPrototype)));
    MASSERT(pClusterTable);
    pClusterTable->OverrideMaximumPerDeviceClusterSize(m_Dev.GetHandle(), clusterSize);
    return rc;
#else
    return RC::UNSUPPORTED_FUNCTION;
#endif
}

RC Lwca::Function::LaunchKernel(const Stream& stream, void** kernelParams, void** extraParams) const
{
    // Avoid scheduling more work if Ctrl-C was pressed
    RC rc;
    CHECK_RC(Abort::Check());

    CHECK_RC(InitCheck());
    CHECK_RC(stream.InitCheck());

    MakeCtxLwrrent setCtx(m_Instance, m_Dev);
    LWresult err = LW_RUN(lwLaunchKernel(
                    m_Function,
                    m_GridWidth, m_GridHeight, 1,
                    m_BlockWidth, m_BlockHeight, m_BlockDepth,
                    m_SharedMemSize,
                    stream.GetHandle(),
                    kernelParams,
                    extraParams));

    if ((err == LWDA_SUCCESS) && m_Instance->IsAutoSyncEnabled())
    {
        CHECK_RC(stream.Synchronize());
    }
    return LwdaErrorToRC(err);
}

RC Lwca::Function::LaunchCooperativeKernel(const Stream& stream, void** kernelParams) const
{
    // Avoid scheduling more work if Ctrl-C was pressed
    RC rc;
    CHECK_RC(Abort::Check());

    CHECK_RC(InitCheck());
    CHECK_RC(stream.InitCheck());

    MakeCtxLwrrent setCtx(m_Instance, m_Dev);
    LWresult err = LW_RUN(lwLaunchCooperativeKernel(
                    m_Function,
                    m_GridWidth, m_GridHeight, 1,
                    m_BlockWidth, m_BlockHeight, m_BlockDepth,
                    m_SharedMemSize,
                    stream.GetHandle(),
                    kernelParams));

    if ((err == LWDA_SUCCESS) && m_Instance->IsAutoSyncEnabled())
    {
        CHECK_RC(stream.Synchronize());
    }
    return LwdaErrorToRC(err);
}

RC Lwca::Function::GetMaxActiveBlocksPerSM(UINT32* pNumBlocks,
                                           UINT32 blockSize,
                                           size_t dynamicSharedMemSize)
{
    MASSERT(pNumBlocks);

    MakeCtxLwrrent setCtx(m_Instance, m_Dev);
    *pNumBlocks = 0;
    int numBlocks = 0;
    LWresult err = LW_RUN(lwOclwpancyMaxActiveBlocksPerMultiprocessor(&numBlocks,
                                                                      m_Function,
                                                                      blockSize,
                                                                      dynamicSharedMemSize));
    if (err == LWDA_SUCCESS)
    {
        *pNumBlocks = static_cast<UINT32>(numBlocks);
    }
    return LwdaErrorToRC(err);
}

RC Lwca::Function::GetMaxBlockSize
(
    UINT32 *pMaxBlocSize
   ,UINT32 *pMinGridSize
   ,UINT32 blockSizeLimit
)
{
    int minGridSize = 0;
    int maxBlockSize = 0;
    *pMinGridSize = 0;
    *pMaxBlocSize = 0;
    MakeCtxLwrrent setCtx(m_Instance, m_Dev);
    LWresult err = LW_RUN(lwOclwpancyMaxPotentialBlockSize(&minGridSize,
                                                           &maxBlockSize,
                                                           m_Function,
                                                           nullptr,
                                                           0,
                                                           static_cast<int>(blockSizeLimit)));

    if (err == LWDA_SUCCESS)
    {
        *pMinGridSize = static_cast<UINT32>(minGridSize);
        *pMaxBlocSize = static_cast<UINT32>(maxBlockSize);
    }
    return LwdaErrorToRC(err);
}

RC Lwca::Function::SetL1Distribution(UINT32 Preference)
{
    return SetSharedSize(Preference);
}

const Lwca::Stream& Lwca::Function::GetStream() const
{
    MASSERT(m_Instance);
    return m_Instance->GetStream(m_Dev);
}

void* Lwca::HostMemoryAllocatorBase::Allocate
(
    const Instance* pInst,
    const Device    dev,
    unsigned        size
)
{
    MASSERT(pInst);
    MASSERT(dev.IsValid());
    MASSERT(pInst->InitCheck(dev) == OK);

    // Ideally we would throw bad_allocon an error, but some MODS builds
    // disable exceptions. For now - just return null pointer.
    if ((pInst == nullptr) || (pInst->InitCheck(dev) != OK) || !dev.IsValid())
    {
        //throw bad_alloc();
        return 0;
    }

    MakeCtxLwrrent setCtx(pInst, dev);
    void* hostPtr = nullptr;
    const LWresult err = LW_RUN(lwMemHostAlloc(&hostPtr, size, 0x0));
    if (err != LWDA_SUCCESS)
    {
        return nullptr;
    }
    return hostPtr;
}

void Lwca::HostMemoryAllocatorBase::Free
(
    const Instance* pInst,
    const Device    dev,
    void*           ptr)
{
    if ((pInst == nullptr) || (pInst->InitCheck(dev) != OK) || !dev.IsValid())
        return;

    MakeCtxLwrrent setCtx(pInst, dev);
    LW_RUN(lwMemFreeHost(ptr));
}

Lwca::HostMemory& Lwca::HostMemory::operator=(HostMemory&& hostMem)
{
    Free();
    m_Instance        = hostMem.m_Instance;
    m_Dev             = hostMem.m_Dev;
    m_HostPtr         = hostMem.m_HostPtr;
    m_Size            = hostMem.m_Size;
    hostMem.m_HostPtr = nullptr;
    return *this;
}

UINT64 Lwca::HostMemory::GetDevicePtr(Device dev)
{
    return Lwca::HostMemory::GetDevicePtr(m_Instance, dev);
}

UINT64 Lwca::HostMemory::GetDevicePtr(const Instance* pInst, Device dev)
{
    MakeCtxLwrrent setCtx(pInst, dev);

    LWdeviceptr devPtr;
    if (LW_RUN(lwMemHostGetDevicePointer(&devPtr, GetPtr(), 0)) != LWDA_SUCCESS)
    {
        Printf(Tee::PriWarn, "Failed to get device pointer to host memory.\n");
        return 0ULL;
    }
    return static_cast<UINT64>(devPtr);
}

void Lwca::HostMemory::Free()
{
    if (m_HostPtr)
    {
        MakeCtxLwrrent setCtx(m_Instance, m_Dev);
        LW_RUN(lwMemFreeHost(m_HostPtr));
        m_HostPtr  = 0;
        m_Size     = 0;
        m_Instance = 0;
    }
}

RC Lwca::Global::InitCheck() const
{
    if (!m_Size)
    {
        Printf(Tee::PriError, "LWCA global not initialized\n");
        return RC::SOFTWARE_ERROR;
    }
    return OK;
}

RC Lwca::Global::Set
(
    const Stream& stream,
    const void* buf,
    size_t offset,
    size_t size
)
{
    // Avoid scheduling more work if Ctrl-C was pressed
    RC rc;
    CHECK_RC(Abort::Check());

    CHECK_RC(InitCheck());
    if (size == 0)
    {
        Printf(Tee::PriWarn, "Copying zero bytes\n");
        return OK;
    }
    if (offset + size > m_Size)
    {
        Printf(Tee::PriError, "Range exceeds allocated memory\n");
        return RC::SOFTWARE_ERROR;
    }

    MakeCtxLwrrent setCtx(m_Instance, m_Dev);
    const LWresult err = LW_RUN(lwMemcpyHtoDAsync(
                static_cast<LWdeviceptr>(m_DevicePtr+offset),
                buf,
                size,
                stream.GetHandle()));
    if (err == LWDA_SUCCESS)
    {
        if (m_Instance->IsAutoSyncEnabled())
        {
            CHECK_RC(stream.Synchronize());
        }
    }
    return LwdaErrorToRC(err);
}

RC Lwca::Global::Get
(
    const Stream& stream,
    void* buf,
    size_t offset,
    size_t size
) const
{
    // Avoid scheduling more work if Ctrl-C was pressed
    RC rc;
    CHECK_RC(Abort::Check());

    CHECK_RC(InitCheck());
    if (size == 0)
    {
        Printf(Tee::PriWarn, "Copying zero bytes\n");
        return OK;
    }
    if (offset + size > m_Size)
    {
        Printf(Tee::PriError, "Range exceeds allocated memory\n");
        return RC::SOFTWARE_ERROR;
    }

    MakeCtxLwrrent setCtx(m_Instance, m_Dev);
    const LWresult err = LW_RUN(lwMemcpyDtoHAsync(
                buf,
                static_cast<LWdeviceptr>(m_DevicePtr+offset),
                size,
                stream.GetHandle()));
    if (err == LWDA_SUCCESS)
    {
        if (m_Instance->IsAutoSyncEnabled())
        {
            CHECK_RC(stream.Synchronize());
        }
    }
    return LwdaErrorToRC(err);
}

RC Lwca::Global::Fill(const Stream& stream, UINT32 Pattern)
{
    // Avoid scheduling more work if Ctrl-C was pressed
    RC rc;
    CHECK_RC(Abort::Check());

    CHECK_RC(InitCheck());
    MakeCtxLwrrent setCtx(m_Instance, m_Dev);
    const LWresult err = LW_RUN(lwMemsetD32Async(static_cast<LWdeviceptr>(m_DevicePtr),
                                                 Pattern,
                                                 m_Size / sizeof(Pattern),
                                                 stream.GetHandle()));
    if (err == LWDA_SUCCESS)
    {
        if (m_Instance->IsAutoSyncEnabled())
        {
            CHECK_RC(stream.Synchronize());
        }
    }
    return LwdaErrorToRC(err);
}

RC Lwca::Global::Set(const void* buf, size_t offset, size_t size)
{
    RC rc;
    CHECK_RC(InitCheck());
    const Stream& stream = m_Instance->GetStream(m_Dev);
    return Set(stream, buf, offset, size);
}

RC Lwca::Global::Get(void* buf, size_t offset, size_t size) const
{
    RC rc;
    CHECK_RC(InitCheck());
    const Stream& stream = m_Instance->GetStream(m_Dev);
    return Get(stream, buf, offset, size);
}

RC Lwca::Global::Set(const void* buf, size_t size)
{
    RC rc;
    CHECK_RC(InitCheck());
    const Stream& stream = m_Instance->GetStream(m_Dev);
    return Set(stream, buf, 0, size);
}

RC Lwca::Global::Get(void* buf, size_t size) const
{
    RC rc;
    CHECK_RC(InitCheck());
    const Stream& stream = m_Instance->GetStream(m_Dev);
    return Get(stream, buf, 0, size);
}

RC Lwca::Global::Fill(UINT32 Pattern)
{
    RC rc;
    CHECK_RC(InitCheck());
    const Stream& stream = m_Instance->GetStream(m_Dev);
    return Fill(stream, Pattern);
}

RC Lwca::Global::Clear()
{
    RC rc;
    CHECK_RC(InitCheck());
    const Stream& stream = m_Instance->GetStream(m_Dev);
    return Fill(stream, 0);
}

RC Lwca::Global::Set(const HostMemory& hostMem)
{
    RC rc;
    CHECK_RC(InitCheck());
    const Stream& stream = m_Instance->GetStream(m_Dev);
    return Set(stream, hostMem.GetPtr(), 0, hostMem.GetSize());
}

RC Lwca::Global::Get(HostMemory* hostMem) const
{
    RC rc;
    CHECK_RC(InitCheck());
    const Stream& stream = m_Instance->GetStream(m_Dev);
    return Get(stream, hostMem->GetPtr(), 0, hostMem->GetSize());
}

RC Lwca::Global::Set(size_t offset, const DeviceMemory &src, size_t srcOffset, size_t size)
{
    // Avoid scheduling more work if Ctrl-C was pressed
    RC rc;
    CHECK_RC(Abort::Check());

    CHECK_RC(InitCheck());
    CHECK_RC(src.InitCheck());

    if (size == 0)
    {
        Printf(Tee::PriWarn, "Copying zero bytes\n");
        return OK;
    }
    if (offset + size > GetSize())
    {
        Printf(Tee::PriError, "Range exceeds allocated memory\n");
        return RC::SOFTWARE_ERROR;
    }

    if (srcOffset + size > src.GetSize())
    {
        Printf(Tee::PriError, "Range exceeds source memory\n");
        return RC::SOFTWARE_ERROR;
    }

    MakeCtxLwrrent setCtx(GetInstance(), GetDevice());

    LWresult err;
    if (GetInstance()->GetHandle(GetDevice()) == src.GetInstance()->GetHandle(src.GetDevice()))
    {
        err = LW_RUN(lwMemcpyDtoDAsync(static_cast<LWdeviceptr>(GetDevicePtr() + offset),
                                       static_cast<LWdeviceptr>(src.GetDevicePtr() + srcOffset),
                                       size,
                                       GetStream().GetHandle()));
    }
    else
    {
        err = LW_RUN(lwMemcpyPeerAsync(static_cast<LWdeviceptr>(GetDevicePtr() + offset),
                                       GetInstance()->GetHandle(GetDevice()),
                                       static_cast<LWdeviceptr>(src.GetDevicePtr() + srcOffset),
                                       src.GetInstance()->GetHandle(src.GetDevice()),
                                       size,
                                       GetStream().GetHandle()));
    }

    if (err == LWDA_SUCCESS)
    {
        if (GetInstance()->IsAutoSyncEnabled())
        {
            CHECK_RC(GetStream().Synchronize());
        }
    }
    return LwdaErrorToRC(err);
}

const Lwca::Stream& Lwca::Global::GetStream() const
{
    MASSERT(m_Instance);
    return m_Instance->GetStream(m_Dev);
}

Lwca::Module& Lwca::Module::operator=(Module&& module)
{
    Unload();
    m_Instance      = module.m_Instance;
    m_Dev           = module.m_Dev;
    m_Module        = module.m_Module;
    module.m_Module = 0;
    return *this;
}

RC Lwca::Module::InitCheck() const
{
    if (!m_Module)
    {
        Printf(Tee::PriError, "LWmodule not initialized\n");
        return RC::SOFTWARE_ERROR;
    }
    return OK;
}

void Lwca::Module::Unload()
{
    if (m_Module)
    {
        MakeCtxLwrrent setCtx(m_Instance, m_Dev);
        LW_RUN(lwModuleUnload(m_Module));
        m_Instance = 0;
        m_Module = 0;
    }
}

Lwca::ModuleEntity Lwca::Module::operator[](const char* name) const
{
    if (!m_Module)
    {
        Printf(Tee::PriWarn, "LWmodule not initialized\n");
        return ModuleEntity(*this, 0);
    }
    return ModuleEntity(*this, name);
}

Lwca::Function Lwca::Module::GetFunction(const char* name) const
{
    if (!m_Module)
    {
        Printf(Tee::PriWarn, "LWmodule not initialized\n");
        return Function();
    }
    LWfunction func = 0;
    MakeCtxLwrrent setCtx(m_Instance, m_Dev);
    if (LW_RUN(lwModuleGetFunction(&func, m_Module, name)) != LWDA_SUCCESS)
    {
        Printf(Tee::PriError, "Function %s not found\n", name);
        return Function();
    }
    return Function(m_Instance, m_Dev, func);
}

Lwca::Function Lwca::Module::GetFunction
(
    const char* name,
    unsigned gridWidth,
    unsigned blockWidth
) const
{
    Function f = GetFunction(name);
    if (f.IsValid())
    {
        f.SetBlockDim(blockWidth);
        f.SetGridDim(gridWidth);
    }
    return f;
}

Lwca::Global Lwca::Module::GetGlobal(const char* name) const
{
    if (!m_Module)
    {
        Printf(Tee::PriWarn, "LWmodule not initialized\n");
        return Global();
    }
    LWdeviceptr ptr = 0;
#if defined(LWDA_FORCE_API_VERSION) && (LWDA_FORCE_API_VERSION < 3020)
    unsigned size = 0;
#else
    size_t size = 0;
#endif
    MakeCtxLwrrent setCtx(m_Instance, m_Dev);
    if (LW_RUN(lwModuleGetGlobal(&ptr, &size, m_Module, name)) != LWDA_SUCCESS)
    {
        return Global();
    }
    return Global(m_Instance, m_Dev, ptr, size);
}

Lwca::Texture Lwca::Module::GetTexture(const char* name) const
{
    if (!m_Module)
    {
        Printf(Tee::PriWarn, "LWmodule not initialized\n");
        return Texture();
    }
    LWtexref texRef = 0;
    MakeCtxLwrrent setCtx(m_Instance, m_Dev);
    if (LW_RUN(lwModuleGetTexRef(&texRef, m_Module, name)) != LWDA_SUCCESS)
    {
        return Texture();
    }
    return Texture(m_Instance, m_Dev, texRef);
}

Lwca::Surface Lwca::Module::GetSurface(const char* name) const
{
    if (!m_Module)
    {
        Printf(Tee::PriWarn, "LWmodule not initialized\n");
        return Surface();
    }
    LWsurfref surfRef = 0;
    MakeCtxLwrrent setCtx(m_Instance, m_Dev);
    if (LW_RUN(lwModuleGetSurfRef(&surfRef, m_Module, name)) != LWDA_SUCCESS)
    {
        return Surface();
    }
    return Surface(m_Instance, m_Dev, surfRef);
}

Lwca::DeviceMemory& Lwca::DeviceMemory::operator=(DeviceMemory&& devMem)
{
    Free();
    this->Global::operator=(devMem);
    m_LoopbackDevicePtr = devMem.m_LoopbackDevicePtr;
    devMem.SetMemory(nullptr, Device(), 0, 0);
    return *this;
}

void Lwca::DeviceMemory::Free()
{
    if (GetSize())
    {
        MakeCtxLwrrent setCtx(GetInstance(), GetDevice());

        #ifndef USE_LWDA_SYSTEM_LIB
        if (m_LoopbackDevicePtr)
            LW_RUN(lwMODSMemUnmapLoopback(static_cast<LWdeviceptr>(m_LoopbackDevicePtr)));
        #endif

        LW_RUN(lwMemFree((LWdeviceptr)GetDevicePtr()));
        SetMemory(0, Device(), 0, 0);
    }
}

RC Lwca::DeviceMemory::MapLoopback()
{
#ifndef USE_LWDA_SYSTEM_LIB
    if (!GetSize())
    {
        Printf(Tee::PriError, "Device memory not initialized\n");
        return RC::SOFTWARE_ERROR;
    }

    if (m_LoopbackDevicePtr)
        return OK;

    MakeCtxLwrrent setCtx(GetInstance(), GetDevice());
    LWdeviceptr ptr = static_cast<LWdeviceptr>(GetDevicePtr());
    const LWresult err = LW_RUN(lwMODSMemMapLoopback(static_cast<LWdeviceptr>(GetDevicePtr()),
                                                     &ptr));
    if (err != LWDA_SUCCESS)
    {
        return LwdaErrorToRC(err);
    }
    m_LoopbackDevicePtr = static_cast<UINT64>(ptr);
    return OK;
#else
    return RC::UNSUPPORTED_FUNCTION;
#endif
}

RC Lwca::DeviceMemory::UnmapLoopback()
{
#ifndef USE_LWDA_SYSTEM_LIB
    if (!GetSize() || !m_LoopbackDevicePtr)
        return OK;

    MakeCtxLwrrent setCtx(GetInstance(), GetDevice());
    const LWresult err = LW_RUN(lwMODSMemUnmapLoopback(
                static_cast<LWdeviceptr>(m_LoopbackDevicePtr)));
    if (err != LWDA_SUCCESS)
    {
        return LwdaErrorToRC(err);
    }
    m_LoopbackDevicePtr = 0;
    return OK;
#else
    return RC::UNSUPPORTED_FUNCTION;
#endif
}

Lwca::ClientMemory& Lwca::ClientMemory::operator=(ClientMemory&& devMem)
{
    Free();
    *static_cast<Global*>(this) = devMem;
    devMem.SetMemory(nullptr, Device(), 0, 0);
    return *this;
}

void Lwca::ClientMemory::Free()
{
#ifndef USE_LWDA_SYSTEM_LIB
    if (GetSize() > 0)
    {
        MakeCtxLwrrent setCtx(GetInstance(), GetDevice());
        LW_RUN(lwMODSFreeResourceMemory64(GetDevicePtr()));
        SetMemory(0, Device(), 0, 0);
    }
#endif
}

P_(Global_Get_LwdaInSys);
P_(Global_Set_LwdaInSys);
static SProperty Global_LwdaInSys
(
    "LwdaInSys",
    0,
    0,
    Global_Get_LwdaInSys,
    Global_Set_LwdaInSys,
    0,
    "Forces LWCA to allocate everything in sysmem."
);

P_(Global_Get_LwdaInSys)
{
    if (OK != JavaScriptPtr()->ToJsval(g_LwdaInSys, pValue))
    {
        *pValue = JSVAL_NULL;
        return JS_FALSE;
    }
    return JS_TRUE;
}

P_(Global_Set_LwdaInSys)
{
    if (OK != JavaScriptPtr()->FromJsval(*pValue, &g_LwdaInSys))
    {
        return JS_FALSE;
    }
    return JS_TRUE;
}
