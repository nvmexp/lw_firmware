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

/**
 * @file    gpu.cpp
 * @brief   Implement class Gpu.
 */

#include <list>

// mods/core
#include "core/include/channel.h"
#include "core/include/chiplibtracecapture.h"
#include "core/include/cmdline.h"
#include "core/include/cpu.h"
#include "core/include/display.h"
#include "core/include/fileholder.h"
#include "core/include/framebuf.h"
#include "core/include/globals.h"
#include "core/include/gpu.h"
#include "core/include/jscript.h"
#include "core/include/mgrmgr.h"
#include "core/include/lwrm.h"
#include "core/include/pci.h"
#include "core/include/platform.h"
#include "core/include/refparse.h"
#include "core/include/registry.h"
#include "core/include/simclk.h"
#include "core/include/tasker.h"
#include "core/include/tee.h"
#include "core/include/utility.h"
#include "core/include/version.h"
#include "core/include/xp.h"
#include "device/interface/pcie.h"

// mods/gpu
#include "gpu/include/floorsweepimpl.h"
#include "gpu/include/gpumgr.h"
#include "gpu/include/gpusbdev.h"
#include "gpu/include/pcicfg.h"
#include "gpu/include/testdevicemgr.h"
#include "gpu/include/vgpu.h"
#include "gpu/interface/vbios_preos_pbi.h"
#include "gpu/js_gpu.h"
#include "gpu/js_gpusb.h"
#include "gpu/reghal/reghaltable.h"
#include "gpu/utility/bglogger.h"
#include "gpu/utility/chanwmgr.h"
#include "gpu/utility/gpuutils.h"
#include "gpu/utility/hbmdev.h"
#include "gpu/utility/hulkprocessing/hulklicenseverifier.h"
#include "gpu/utility/lwlink/lwlinkdevif/lwl_topology_mgr.h"
#include "gpu/utility/pcie/pexdev.h"
#include "gpu/utility/rchelper.h"
#include "gpu/utility/runlist.h"
#include "gpu/utility/surf2d.h"
#include "gpu/utility/tpceccerror.h"
#include "gpu/utility/tsg.h"
#include "gpu/vmiop/vmiopelwmgr.h"
#if LWCFG(GLOBAL_GPU_IMPL_G000)
#include "gpu/amodgpu.h"
#endif
#include "gpu/gm10xgpu.h"
#include "gpu/gp10xgpu.h"
#if LWCFG(GLOBAL_ARCH_VOLTA)
#include "gpu/voltagpu.h"
#endif
#if LWCFG(GLOBAL_ARCH_TURING)
#include "gpu/tu100gpu.h"
#endif
#if LWCFG(GLOBAL_ARCH_AMPERE)
#include "gpu/amperegpu.h"
#endif
#if LWCFG(GLOBAL_ARCH_HOPPER)
#include "gpu/hoppergpu.h"
#endif
#if LWCFG(GLOBAL_ARCH_ADA)
#include "gpu/adagpu.h"
#endif

// mods/opengl
#ifdef INCLUDE_OGL
#include "opengl/modsgl.h"
#endif

// mods/cheetah
#include "cheetah/include/tegchip.h"
#include "cheetah/include/tegraaperturemap.h"
#include "cheetah/include/tegrasocdevice.h"
#include "cheetah/include/tegrasocdeviceptr.h"
#if defined(TEGRA_MODS)
#include "cheetah/include/tegrarm.h"
#else
class TegraRm { }; // Stub for unique_ptr destructor
#endif

// mods/azalia
#ifdef INCLUDE_AZALIA
#include "device/azalia/azactrlmgr.h"
#include "device/azalia/azactrl.h"
#endif

// mdiag
#if defined(INCLUDE_MDIAG)
#include "mdiag/advschd/policymn.h"
#endif

// LWPU SDK
#include "class/cl50a0.h" // LW50_MEMORY_VIRTUAL
#include "ctrl/ctrl2080/ctrl2080gpu.h"
#include "Lwcm.h"

// drivers/resman
#include "lwRmReg.h"
#include "lw_ref.h" // for LW_PMC_BOOT_0 and LW_PMC_BOOT_1

// arch/lwalloc
#include "gpuarch.h" // LWGPU_ARCHITECTURE_SERIES*
#include "lwcmrsvd.h"
#include "rm.h"

#if defined(TEGRA_MODS)
#include <stdlib.h>
#endif

#define RM_INIT_SECTION 0x80000000

// Declare the one global GPU object that GpuPtr refers to
Gpu GpuPtr::s_Gpu;

bool                Gpu::s_IsInitialized    = false;
bool                Gpu::s_RmInitCompleted  = false;
bool                Gpu::s_SortPciAscending = true;
const PHYSADDR      Gpu::s_IlwalidPhysAddr  = (PHYSADDR)-1;

namespace
{
    Tasker::ThreadID s_OneHzCallbackThreadID   = Tasker::NULL_THREAD;
    volatile INT32   s_ExitOneHzCallbackThread = 0;
    bool             s_LwRmInitCompleted       = false;
    bool             s_GpuDevMgrHasDevices     = false;
    void             *s_OneHzMutex = nullptr;

    void OneHzCallbackThread(void*)
    {
        const UINT64 targetSleepTimeUs = 1000*1000; // 1s target sleep time
        const UINT64 maxSleepIntervalUs = 100*1000; // 100ms max sleep interval

        GpuDevMgr* const pGpuMgr = DevMgrMgr::d_GraphDevMgr;
        Tasker::DetachThread detach;
        while (!Cpu::AtomicRead(&s_ExitOneHzCallbackThread))
        {
            {
                Tasker::MutexHolder lock(s_OneHzMutex);
                pGpuMgr->Run1HzCallbacks();
            }
            UINT64 start = Xp::GetWallTimeUS();

            // Gpu::Shutdown joins this thread, which could result in a long
            // wait if we just pinged RM. Therefore, instead of sleeping for 1s
            // outright, split the sleep into steps and check for the exit flag
            // between each of the sleeps. That reduces the worst-case scenario
            // wait from 1s down to maxSleepIntervalUs
            UINT64 sleepTimeUs = maxSleepIntervalUs;
            while (!Cpu::AtomicRead(&s_ExitOneHzCallbackThread) && sleepTimeUs)
            {
                // We need to round up the sleepTimeUs so that we aren't
                // undershooting the target due to round-off error
                Tasker::Sleep(static_cast<FLOAT64>((sleepTimeUs + 999) / 1000));

                UINT64 timeSinceCB = Xp::GetWallTimeUS() - start;
                if (timeSinceCB >= targetSleepTimeUs)
                {
                    break;
                }

                sleepTimeUs = min((targetSleepTimeUs - timeSinceCB),
                                  maxSleepIntervalUs);
            }
        }
    }
}

string Gpu::s_ThreadMonitor("thread_monitor");
string Gpu::s_ThreadShutdown("thread_results");

//------------------------------------------------------------------------------
// Creators
//
#define DEFINE_NEW_GPU(DevIdStart, DevIdEnd, ChipId, GpuId, Constant,     \
                       Family, SubdeviceType, ...)                        \
    extern GpuSubdevice *Create##SubdeviceType(Gpu::LwDeviceId,           \
                                               const PciDevInfo *);
#define DEFINE_DUP_GPU(...) // don't care
#define DEFINE_OBS_GPU(...) // don't care
#include "gpu/include/gpulist.h"
#undef DEFINE_NEW_GPU
#undef DEFINE_DUP_GPU
#undef DEFINE_OBS_GPU

static Gpu::Chip s_Chips[] =
{
#define DEFINE_NEW_GPU(DevIdStart, DevIdEnd, ChipId, GpuId, Constant,   \
                       Family, SubdeviceType, ...)                      \
    { ChipId, DevIdStart, DevIdEnd, Gpu::GpuId, false, Create##SubdeviceType },
#define DEFINE_DUP_GPU(DevIdStart, DevIdEnd, ChipId, GpuId)             \
    { ChipId, DevIdStart, DevIdEnd, Gpu::GpuId, false, NULL },
#define DEFINE_OBS_GPU(DevIdStart, DevIdEnd, ChipId, GpuId, Constant, Family) \
    { ChipId, DevIdStart, DevIdEnd, Constant, true, NULL },
#include "gpu/include/gpulist.h"
#undef DEFINE_NEW_GPU
#undef DEFINE_DUP_GPU
#undef DEFINE_OBS_GPU
};

// Find the Chip for specified device ID. If not found return 0.
static const Gpu::Chip * FindChip
(
    UINT32 ChipId,
    UINT32 PciDeviceId
)
{
    Gpu::Chip *pChip = NULL; // Return value
    size_t i;           // Loop variable

    // Find the chip that matches ChipId
    //
    for (i = 0; i < NUMELEMS(s_Chips); ++i)
    {
        if (s_Chips[i].ChipId == ChipId)
        {
            pChip = &s_Chips[i];
            break;
        }
    }

    if (pChip == NULL)
    {
        // If no direct Chip match, find the chip that matches DeviceId
        //
        // Note that this really shouldn't happen unless there are test GPUs
        // defined that do not have a valid ARCH value.  Any actual released
        // GPU should be found based on the actual ChipId

        Printf(Tee::PriLow,
               "FindChip : Unable to find ChipId 0x%08x, searching by "
               "PciDeviceId\n", ChipId);
        for (i = 0; i < NUMELEMS(s_Chips); ++i)
        {
            if ((s_Chips[i].DevIdStart <= PciDeviceId) &&
                (s_Chips[i].DevIdEnd >= PciDeviceId))
            {
                pChip = &s_Chips[i];
                break;
            }
        }
    }

    // If we found the chip, but it's a duplicate created by
    // DEFINE_DUP_GPU, then copy the missing info from the original
    // GPU.
    //
    if (pChip && !pChip->IsObsolete && !pChip->CreateGpuSubdevice)
    {
        for (i = 0; i < NUMELEMS(s_Chips); ++i)
        {
            if ((s_Chips[i].LwId == pChip->LwId) &&
                (s_Chips[i].CreateGpuSubdevice != NULL))
            {
                pChip->CreateGpuSubdevice = s_Chips[i].CreateGpuSubdevice;
                break;
            }
        }
        MASSERT(pChip->CreateGpuSubdevice != NULL);
    }

    return pChip;
}

struct IdToNameAndFamily
{
    Gpu::LwDeviceId id;
    const char * name;
    GpuDevice::GpuFamily family;
    const char *familyName;
};

static IdToNameAndFamily s_IdToNameAndFamily[] =
{
#define DEFINE_NEW_GPU(DIDStart, DIDEnd, ChipId, LwId, Constant, Family, ...) \
    { Gpu::LwId, #LwId, GpuDevice::Family, #Family },
#define DEFINE_DUP_GPU(...) // don't care
#define DEFINE_OBS_GPU(DIDStart, DIDEnd, ChipId, LwId, Constant, Family) \
    { (Gpu::LwDeviceId)Constant, #LwId, GpuDevice::Family, #Family },
#include "gpu/include/gpulist.h"
#undef DEFINE_NEW_GPU
#undef DEFINE_DUP_GPU
#undef DEFINE_OBS_GPU
};

static const char * s_ClkSrcToString[Gpu::ClkSrc_NUM] =
{
    ENCJSENT("DEFAULT"),
    ENCJSENT("XTAL"),
    ENCJSENT("EXTREF"),
    ENCJSENT("QUALEXTREF"),
    ENCJSENT("EXTSPREAD"),
    ENCJSENT("SPPLL0"),
    ENCJSENT("SPPLL1"),
    ENCJSENT("MPLL"),
    ENCJSENT("XCLK"),
    ENCJSENT("XCLK3XDIV2"),
    ENCJSENT("HOSTCLK"),
    ENCJSENT("PEXREFCLK"),
    ENCJSENT("VPLL0"),
    ENCJSENT("VPLL1"),
    ENCJSENT("VPLL2"),
    ENCJSENT("VPLL3"),
    ENCJSENT("XCLKGEN2"),
    ENCJSENT("SYSPLL"),
    ENCJSENT("GPCPLL"),
    ENCJSENT("LTCPLL"),
    ENCJSENT("XBARPLL"),
    ENCJSENT("REFMPLL"),
    ENCJSENT("TestClk"),
    ENCJSENT("XTAL4X"),
    ENCJSENT("PWRCLK"),
    ENCJSENT("HDACLK"),
    ENCJSENT("XCLK500"),
    ENCJSENT("XCLKGEN3"),
    ENCJSENT("HBMPLL"),
    ENCJSENT("NAFLL"),
    ENCJSENT("Supported"),
};

//------------------------------------------------------------------------------
// GPU monitor thread.
//
Tasker::ThreadID s_GpuMonitorThreadID = Tasker::NULL_THREAD;
Tasker::EventID s_GpuMonitorEvent;
bool s_ContinueGpuMonitorThread = true;

namespace
{
    void GpuMonitorThread
    (
        void * Args
    )
    {
        GpuDevice *pDev = 0;
        UINT32 idx;
        UINT32 numGpusFound = 0;
        const Gpu::MonitorArgs *pGpuMonArgs = static_cast<Gpu::MonitorArgs *>(Args);
        const FLOAT64 MonitorIntervalMs = pGpuMonArgs->monitorIntervalMs;
        const Gpu::MonitorFilter monitorFilter =
            static_cast<Gpu::MonitorFilter>(pGpuMonArgs->monitorFilter);

        Platform::PreGpuMonitorThread();

        for (;;)
        {
            Tasker::WaitOnEvent(s_GpuMonitorEvent, MonitorIntervalMs);
            if (!s_ContinueGpuMonitorThread)
                break;

            ChiplibOpScope newScope("GPU Status Monitor", NON_IRQ,
                                     ChiplibOpScope::SCOPE_COMMON, NULL);

            pDev = DevMgrMgr::d_GraphDevMgr->GetFirstGpuDevice();
            while (pDev)
            {
                numGpusFound++;
                // Print the monitor info for each subdevice
                for (idx = 0; idx < pDev->GetNumSubdevices(); ++idx)
                {
                    Printf(Tee::PriNormal, "Dev %d.%d: ",
                           pDev->GetDeviceInst(), idx);
                    pDev->GetSubdevice(idx)->PrintMonitorInfo(monitorFilter);
                    pDev->GetSubdevice(idx)->PrintMonitorRegisters();
                }
                pDev = DevMgrMgr::d_GraphDevMgr->GetNextGpuDevice(pDev);
            }
            if (!numGpusFound)
            {
                Printf(Tee::PriNormal, "Error getting at least one GPU Device in "
                                       "GpuMonitorThread.\n");
                break;
            }
        }
    }
}

//------------------------------------------------------------------------------
// JS thread.
//
Tasker::ThreadID s_GpuThreadThreadID = Tasker::NULL_THREAD;
Tasker::EventID s_GpuThreadEvent;
bool s_ContinueGpuJSThread = true;

static void JSMonitorThread
(
    void * Args
)
{
    GpuPtr pGpu;
    FLOAT64 MonitorIntervalMs = *(FLOAT64 *)Args;

    JavaScriptPtr pJs;

    for (;;)
    {
        Tasker::WaitOnEvent(s_GpuThreadEvent, MonitorIntervalMs);
        if (!s_ContinueGpuJSThread)
            break;

        if (OK != pJs->CallMethod(pJs->GetGlobalObject(),
                                  pGpu->ThreadMonitor()))
            Printf(Tee::PriError, "%s() failed\n",
                   pGpu->ThreadMonitor().c_str());
    }
}

UINT32 Gpu::GetSubdeviceStatus
(
    const PciDevInfo& pciDevInfo,
    const Chip** ppChip,
    const set<string>& GpusUnderTest
)
{
    UINT32 status = gpuStatusValid;

    if (LWGPU_ARCHITECTURE_SERIES_TEGRA ==
             DRF_VAL(GPU, _ARCHITECTURE, _SERIES, pciDevInfo.ChipId))
    {
        // If filtering by PCI device ID and this is an SOC device and SOC
        // devices have not also been explicitly allowed, ignore the SOC
        if (!m_AllowedPciDevices.empty() && !m_OnlySocDevice)
            status |= gpuStatusIgnoredPciDev;
    }
    // Ignore devices not specified on the enumerated list (if the list is not empty)
    else if (!IsPciDeviceInAllowedList(pciDevInfo.PciDomain,
                                       pciDevInfo.PciBus,
                                       pciDevInfo.PciDevice,
                                       pciDevInfo.PciFunction))
    {
        status |= gpuStatusIgnoredPciDev;
    }
    else if (m_OnlySocDevice)
    {
        // If no PCI devices have been filtered but only SOC devices are
        // requested ignore any PCI device
        status |= gpuStatusIgnoredPciDev;
    }

    if (!m_AllowedDevIds.empty())
    {
        if (m_AllowedDevIds.find(pciDevInfo.PciDeviceId) == m_AllowedDevIds.end())
        {
            status |= gpuStatusIgnoredPciDev;
        }
    }

    if (pciDevInfo.PciDeviceId == 0xFFFF)
    {
        const UINT32 pmcBoot0 =
            MEM_RD32((UINT08*)pciDevInfo.LinLwBase + LW_PMC_BOOT_0);
        const UINT32 pmcBoot1 =
            MEM_RD32((UINT08*)pciDevInfo.LinLwBase + LW_PMC_BOOT_1);
        if ((pmcBoot0 == ~0U) && (pmcBoot1 == ~0U))
        {
            status |= gpuStatusFellOffTheBus;
        }
    }

    const Gpu::Chip* pChip = FindChip(pciDevInfo.ChipId,
                                      pciDevInfo.PciDeviceId);
    if (ppChip)
    {
        *ppChip = pChip;
    }
    if (!pChip)
    {
        return status | gpuStatusUnknown;
    }
    else if (pChip->IsObsolete)
    {
        status |= gpuStatusObsolete;
    }

    Gpu::LwDeviceId devId = (Gpu::LwDeviceId)pChip->LwId;

    if (IsIgnoredFamily(devId))
        status |= gpuStatusIgnoredGpuClass;

    string extGpuName = Gpu::DeviceIdToExternalString(devId);
    string intGpuName = Gpu::DeviceIdToInternalString(devId);
    if (!GpusUnderTest.empty())
    {
        // Findout if user restricted the GPUs to be tested
        if (GpusUnderTest.find(extGpuName) == GpusUnderTest.end())
        {
            Printf(Tee::PriLow,
                    "Gpu [%s]: will not be tested\n", intGpuName.c_str());
            status |= gpuStatusIgnoredGpuClass;
        }
    }

    return status;
}

//------------------------------------------------------------------------------
namespace // anonymous namespace
{
    RC FamilyNameLookup
    (
        const string & family,
        GpuDevice::GpuFamily * pFamily
    )
    {
        GpuDevice::GpuFamily f = GpuDevice::None;

        for (size_t i = 0; i < NUMELEMS(s_IdToNameAndFamily); i++)
        {
            const string thisName = Utility::RestoreString(s_IdToNameAndFamily[i].familyName);
            if (0 == stricmp(thisName.c_str(), family.c_str()))
            {
                f = s_IdToNameAndFamily[i].family;
                break;
            }
        }
        if (GpuDevice::None == f)
        {
            Printf(Tee::PriError, "Unexpected gpu family name \"%s\"\n",
                   family.c_str());
            return RC::BAD_COMMAND_LINE_ARGUMENT;
        }
        *pFamily = f;
        return OK;
    }
};  // anonymous namespace

vector<bool> Gpu::s_IgnoredFamilies(GpuDevice::NUM_GpuFamily, false);

//------------------------------------------------------------------------------
bool Gpu::IsIgnoredFamily(Gpu::LwDeviceId id)
{
    for (size_t i = 0; i < NUMELEMS(s_IdToNameAndFamily); i++)
    {
        if (s_IdToNameAndFamily[i].id == id)
            return s_IgnoredFamilies[s_IdToNameAndFamily[i].family];
    }
    return false;
}

//------------------------------------------------------------------------------
RC Gpu::IgnoreFamily(const string & family)
{
    GpuDevice::GpuFamily f;
    RC rc;
    CHECK_RC(FamilyNameLookup(family, &f));
    if (s_IgnoredFamilies[f]) // Is the family already ignored?
    {
        return OK;
    }
    if (GpuPtr::IsInstalled() && GpuPtr()->IsInitialized())
        return RC::SOFTWARE_ERROR; // Too late to set this flag!
    s_IgnoredFamilies[f] = true;

    return OK;
}

//------------------------------------------------------------------------------
RC Gpu::OnlyFamily(const string & family)
{
    GpuDevice::GpuFamily f;
    RC rc;
    if (GpuPtr::IsInstalled() && GpuPtr()->IsInitialized())
        return RC::SOFTWARE_ERROR; // Too late to set this flag!
    CHECK_RC(FamilyNameLookup(family, &f));
    s_IgnoredFamilies.clear();
    s_IgnoredFamilies.resize(GpuDevice::NUM_GpuFamily, true);
    s_IgnoredFamilies[f] = false;
    return OK;
}

map<UINT32, UINT32> Gpu::s_RemapRegs;
//------------------------------------------------------------------------------
/*static */
UINT32 Gpu::GetRemappedReg(UINT32 offset)
{
    // find() can be expensive, so only call it once. However I don't envision this
    // map to get very large, < 10 registers.
    auto itr = s_RemapRegs.find(offset);
    if (itr != s_RemapRegs.end())
    {
        return itr->second;
    }
    return offset;
}

//------------------------------------------------------------------------------
/*static*/
RC Gpu::RemapReg(UINT32 fromAddr, UINT32 toAddr)
{
    //Don't allow multiple indirection
    if (s_RemapRegs.find(toAddr) == s_RemapRegs.end())
    {
        s_RemapRegs[fromAddr] = toAddr;
        return RC::OK;
    }
    return RC::SOFTWARE_ERROR;
}

//------------------------------------------------------------------------------
Gpu::Gpu()
{
}

//------------------------------------------------------------------------------
Gpu::~Gpu()
{
    if (Platform::IsForcedTermination())
        return;
    // we should already be shut down
    MASSERT (!s_IsInitialized);
}

void *Gpu::GetOneHzMutex()
{
    return s_OneHzMutex;
}

//------------------------------------------------------------------------------
RC Gpu::ShutDown(ShutDownMode Mode)
{
    PROFILE_ZONE(GENERIC)

    StickyRC firstRc;

    GpuDevMgr* const pGpuDevMgr = DevMgrMgr::d_GraphDevMgr;

    ChiplibOpScope newScope("GPU ShutDown", NON_IRQ,
                            ChiplibOpScope::SCOPE_COMMON, NULL);

    Printf(Tee::PriLow, "Beginning GPU shut down\n");

    if (s_IsInitialized && Mode == ShutDownMode::QuickAndDirty)
    {
        Printf(Tee::PriNormal,
            "Quick and Dirty GPU shut down requested, skipping all steps.\n");

        for (auto pGpuSubdevice : *pGpuDevMgr)
        {
            Printf(Tee::PriNormal,
                "Do end of sim check on Gpu%d.\n", pGpuSubdevice->GetGpuId());
            firstRc = pGpuSubdevice->DoEndOfSimCheck(Mode);
        }

        // Unhook the interrupts.
        if (pGpuDevMgr)
        {
            pGpuDevMgr->UnhookInterrupts();
        }

        s_LwRmInitCompleted   = false;
        s_GpuDevMgrHasDevices = false;
        s_IsInitialized       = false;
        return firstRc;
    }

    // Shutdown all background loggers before shutting down GPUs
    firstRc = BgLogger::Shutdown();
    TpcEccError::Shutdown();

    if (s_IsInitialized)
    {
        // Kill the 1Hz thread
        if (!Cpu::AtomicRead(&s_ExitOneHzCallbackThread))
        {
            Cpu::AtomicWrite(&s_ExitOneHzCallbackThread, 1);
            if (s_OneHzCallbackThreadID != Tasker::NULL_THREAD)
            {
                const RC rc = Tasker::Join(s_OneHzCallbackThreadID, 2000);
                if (OK != rc)
                {
                    Printf(Tee::PriWarn,
                           "Unable to stop the 1Hz callback thread\n");
                }
                Tasker::FreeMutex(s_OneHzMutex);
            }
        }

        ChannelWrapperMgr::ShutDown();

        if (s_GpuMonitorThreadID != Tasker::NULL_THREAD)
        {
            s_ContinueGpuMonitorThread = false;
            Tasker::SetEvent(s_GpuMonitorEvent);
            firstRc = Tasker::Join(s_GpuMonitorThreadID);
            Tasker::FreeEvent(s_GpuMonitorEvent);
            s_GpuMonitorThreadID = Tasker::NULL_THREAD;
            s_GpuMonitorEvent = NULL;
        }

        if (s_GpuThreadThreadID != Tasker::NULL_THREAD)
        {
            s_ContinueGpuJSThread = false;
            Tasker::SetEvent(s_GpuThreadEvent);
            firstRc = Tasker::Join(s_GpuThreadThreadID);
            Tasker::FreeEvent(s_GpuThreadEvent);
            s_GpuThreadThreadID = Tasker::NULL_THREAD;
            s_GpuThreadEvent = NULL;
        }

        JsArray Args;
        JavaScriptPtr pJs;
        UINT32 jsRetVal = RC::SOFTWARE_ERROR;
        jsval retValJs  = JSVAL_NULL;
        firstRc = pJs->CallMethod(pJs->GetGlobalObject(),
                                  "HandleGpuTeardown",
                                  Args,
                                  &retValJs);
        firstRc = pJs->FromJsval(retValJs, &jsRetVal);
        firstRc = jsRetVal;

        JSSurf2DTracker::CollectGarbage();
    }

    // This shutdown may call into RM to perform shutdown, therefore it must be
    // done before RM shutdown
    firstRc = LwLinkDevIf::TopologyManager::Shutdown();
    if (DevMgrMgr::d_TestDeviceMgr)
    {
        ((TestDeviceMgr*)DevMgrMgr::d_TestDeviceMgr)->ShutdownAll();
    }

    if (s_GpuDevMgrHasDevices)
    {
        // bad code design but shutdown gets called twice.  argh!
        if (pGpuDevMgr)
        {
            DevMgrMgr::d_GraphDevMgr->OnExit();

            // we're about to destroy the RM so we have to unlink here
            // on exit, we've nuked the JS engine, so this would have
            // happened through an OnExit call to the GpuDevMgr first
            // and will be a noop here
            firstRc = pGpuDevMgr->ShutdownAll();
            pGpuDevMgr->UnlinkAll();
            pGpuDevMgr->FreeDeviceObjects();
            // leave the device manager intact with GpuSubdevice info
        }
        s_GpuDevMgrHasDevices = false;
    }

    bool unhookInt = true;
    if (s_LwRmInitCompleted && !m_SkipDevInit)
    {
        s_LwRmInitCompleted = false;

        // Destroy all remaining client resources allocated from the
        // RM (by destroying all remaining RM clients).  The RM will
        // crash if we do this after RmDestroyDevice. -- ?? did this
        // comment intend to say RmDestroyGpu?
        for (UINT32 client = 0;
             client < LwRmPtr::GetValidClientCount(); client++)
        {
            firstRc = LwRmPtr(client)->FreeClient();
        }

        // Destroy devices in RM
        if (pGpuDevMgr)
        {
            if (!m_SocDevices.empty())
            {
                if (!RmDestroySoc(&m_SocInitInfo))
                {
                    Printf(Tee::PriNormal, "RmDestroySoc failed!\n");
                }
                memset(&m_SocInitInfo, 0, sizeof(m_SocInitInfo));
            }

            pGpuDevMgr->UnhookInterrupts();
            unhookInt = false;

            pGpuDevMgr->DestroySubdevicesInRm();

            for (auto pSubdev : *pGpuDevMgr)
            {
                Xp::ResumeConsole(pSubdev);
            }
        }
    }

#if defined(TEGRA_MODS)
    // Shut down CheetAh RM
    if (m_pTegraRm.get())
    {
        firstRc = m_pTegraRm->ShutDown();
        m_pTegraRm.reset(nullptr);
    }
#endif

    if (Platform::IsVirtFunMode() || Platform::IsPhysFunMode())
    {
        VmiopElwManager::Instance()->ShutDown();
    }

    if (pGpuDevMgr && unhookInt)
    {
        pGpuDevMgr->UnhookInterrupts();
    }

    // Free RM internal data structures (pGpu, pDev) needed by RmUnhookIsr.
    if (!RmDestroy())
    {
        Printf(Tee::PriNormal, "RmDestroy failed\n");
        firstRc = RC::SOFTWARE_ERROR;
    }

    // Destroy HBM Devices before GpuSubdevices (they get destroyed in pGpuDevMgr->FreeDevices())
    // since destroying HBM requires writing to GPU registers
    if (DevMgrMgr::d_HBMDevMgr)
    {
        ((HBMDevMgr*)DevMgrMgr::d_HBMDevMgr)->ShutdownAll();
        delete DevMgrMgr::d_HBMDevMgr;
        DevMgrMgr::d_HBMDevMgr = 0;
    }

    if (pGpuDevMgr)
    {
        pGpuDevMgr->PostRmShutdown();

        // Free mappings made by FindDevices.
        pGpuDevMgr->FreeDevices();
    }

    if (DevMgrMgr::d_PexDevMgr)
    {
        ((PexDevMgr*)DevMgrMgr::d_PexDevMgr)->ShutdownAll();
        delete DevMgrMgr::d_PexDevMgr;
        DevMgrMgr::d_PexDevMgr = 0;
    }

    if (DevMgrMgr::d_TestDeviceMgr)
    {
        // OnExit completely shuts down all LwLink libraries including those necessary for
        // RM so it must be called after RM shutdown
        ((TestDeviceMgr*)DevMgrMgr::d_TestDeviceMgr)->OnExit();
        delete DevMgrMgr::d_TestDeviceMgr;
        DevMgrMgr::d_TestDeviceMgr = 0;
    }

    if (!m_SocDevices.empty())
    {
        firstRc = CheetAh::SocPtr()->ShutDownGPUClocks();
        m_SocDevices.clear();
    }

    Vgpu::Shutdown();

    if (pGpuDevMgr)
    {
        // now we can destroy the gpu device manager
        delete pGpuDevMgr;
        DevMgrMgr::d_GraphDevMgr = nullptr;
    }

#if defined(TEGRA_MODS)
    // Re-enable register firewall
    if (CheetAh::IsInitialized())
    {
        firstRc = CheetAh::SocPtr()->ClaimGpuRegFirewallOwnership(false);
    }
#endif

    s_IsInitialized = false;
    s_RmInitCompleted = false;

    Printf(Tee::PriLow, "Completed GPU shut down\n");
    return firstRc;
}

RC Gpu::DramclkDevInitWARkHz(UINT32 freqkHz)
{
    RC rc;

    for (auto pSubdev : *DevMgrMgr::d_GraphDevMgr)
    {
        // Do not remove the HasBug(200167665) check until *after* GV100 is in
        // production - even if the SetHasBug has been removed. MED team may want
        // to do experiments with FB falcon disabled.
        if (pSubdev->HasBug(1730212) || pSubdev->HasBug(200167665))
        {
            CHECK_RC(pSubdev->DramclkDevInitWARkHz(freqkHz));
        }
    }

    return rc;
}

RC Gpu::DisableEcc()
{
    RC rc;

    for (auto pSubdev : *DevMgrMgr::d_GraphDevMgr)
    {
        CHECK_RC(pSubdev->DisableEcc());
    }

    return rc;
}

RC Gpu::EnableEcc()
{
    RC rc;

    for (auto pSubdev : *DevMgrMgr::d_GraphDevMgr)
    {
        CHECK_RC(pSubdev->EnableEcc());
    }

    return rc;
}

RC Gpu::SkipInfoRomRowRemapping()
{
    RC rc;

    for (auto pSubdev : *DevMgrMgr::d_GraphDevMgr)
    {
        CHECK_RC(pSubdev->SkipInfoRomRowRemapping());
    }

    return rc;
}

RC Gpu::DisableRowRemapper()
{
    RC rc;

    for (auto pSubdev : *DevMgrMgr::d_GraphDevMgr)
    {
        CHECK_RC(pSubdev->DisableRowRemapper());
    }

    return rc;
}

string Gpu::ToString(ResetMode mode)
{
    switch (mode)
    {
        case ResetMode::NoReset:  return "NoReset";
        case ResetMode::HotReset: return "HotReset";
        case ResetMode::SwReset:  return "SwReset";
        case ResetMode::FundamentalReset:  return "FundamentalReset";
        case ResetMode::ResetDevice:  return "ResetDevice";
        case ResetMode::FLReset:  return "FLReset";

        default:
            MASSERT(!"Unknown ResetMode");
            return "Unknown";
    }
}

// Does a secondary bus reset on all testdevices
RC Gpu::Reset(ResetMode mode)
{
    PROFILE_ZONE(GENERIC)

    if (mode == ResetMode::NoReset)
        return OK;

    RC rc;
    TestDeviceMgr* pTestDeviceMgr = static_cast<TestDeviceMgr*>(DevMgrMgr::d_TestDeviceMgr);

    for (UINT32 devInst = 0; devInst < pTestDeviceMgr->NumDevices(); devInst++)
    {
        TestDevicePtr pTestDevice;
        CHECK_RC(pTestDeviceMgr->GetDevice(devInst, &pTestDevice));

        if (pTestDevice->IsModsInternalPlaceholder())
            continue;

        switch (pTestDevice->GetType())
        {
            case TestDevice::TYPE_LWIDIA_GPU:
            {
                auto pSubdev = pTestDevice->GetInterface<GpuSubdevice>();
                if (pSubdev && pSubdev->IsEmOrSim())
                    continue;
                break;
            }
            case TestDevice::TYPE_LWIDIA_LWSWITCH:
            case TestDevice::TYPE_IBM_NPU:
                break;
            default:
                continue; //skip
        }

        switch (mode)
        {
            case ResetMode::HotReset:
                CHECK_RC(pTestDevice->HotReset(TestDevice::FundamentalResetOptions::Disable));
                break;
            case ResetMode::SwReset:
            {
                auto pSubdev = pTestDevice->GetInterface<GpuSubdevice>();
                if (pSubdev)
                    CHECK_RC(pSubdev->GetFsImpl()->SwReset(true));
                break;
            }
            case ResetMode::FundamentalReset:
            {
                CHECK_RC(pTestDevice->HotReset(TestDevice::FundamentalResetOptions::Enable));
                break;
            }
            case ResetMode::ResetDevice:
            {
                CHECK_RC(pTestDevice->HotReset(GpuSubdevice::FundamentalResetOptions::Default));
                break;
            }
            case ResetMode::FLReset:
            {
                CHECK_RC(pTestDevice->FLReset());
                break;
            }
            default:
                Printf(Tee::PriError, "Unknown reset mode : %d\n", static_cast<UINT32>(mode));
                return RC::BAD_PARAMETER;
        }
    }

    // This newline makes the log prettier
    Printf(Tee::PriNormal, "\n");

    return rc;
}

/*!
 * This function does various escape writes to configure the simulation as
 * desired.  Strap configuration has been moved to PostBar0Setup.
 */
void Gpu::ConfigureSimulation(void)
{
    JavaScriptPtr pJs;
    bool skipConfigSim;

    pJs->GetProperty(Gpu_Object, Gpu_SkipConfigSim, &skipConfigSim);
    if (skipConfigSim)
        return;

    if ((Platform::GetSimulationMode() == Platform::Amodel) &&
        ((Xp::GetOperatingSystem() != Xp::OS_WINDA) &&
         (Xp::GetOperatingSystem() != Xp::OS_LINDA)))
    {
        // first amod escape write must set the version
        int rc = Platform::EscapeWrite("ModsVersion", 0, 4, /*version*/ 1);
        if (rc != 0)
        {
            MASSERT(0 && "setting mods version must succeed");
        }
    }
    else if (Platform::GetSimulationMode() == Platform::Fmodel)
    {
        // Amodel/Fmodel FB size must be configured separately (note that in
        // newer fmodels escape writes to STRAP_FB are ignored)

        // Note that m_StrapFb must not be modified here.  If modified here it will affect
        // other places in MODS where m_StrapFb is used where it needs to stay 0.  We should
        // only send 256 down when m_StrapFb is zero in the escape write in this particular
        // function
        //
        // In addition, based on the comment above STRAP_FB is not even used on newer fmodels
        // and only hopper+ support > 32bit strap FB values so skip the write completely if
        // m_StrapFb > _UINT32_MAX
        if (m_StrapFb <= _UINT32_MAX)
        {
            UINT32 strapFb = static_cast<UINT32>(m_StrapFb);
            if (strapFb == 0)
            {
                // If not set from command line, set it to 256M.
                strapFb = 256;
            }

            Platform::EscapeWrite("STRAP_FB", 0, 4, strapFb);
        }
    }

    // Provide a JS callback to let users do additional tweaks
    pJs->CallMethod(pJs->GetGlobalObject(), "ConfigureSimulationCallback");
}

//! This function does various escape writes to configure the simulation as
//! desired and is called after BAR0 is setup, but before setting up BAR1 or
//! BAR2.  In particular, we need to configure the straps, and we need to
//! configure the RAM models on RTL to match up with the PFB_CFG, etc.
//! settings.  Depending, for example, on the fbRamType, the RTL simulation
//! will use a DDR1, DDR2, or DDR3 RAM model.
//!
//! The following things MUST NOT be changed here:
//!     Anything that would affect BAR0
//!     BAR widths (64bit vs. 32bit)
//!     Anything that would affect LW_CONFIG_PCI_LW_1
//!     Anything that would affect LW_CONFIG_PCI_LW_15
//!     Anything that would affect LW_PMC_BOOT_1
//!     Anything that would affect LW_PMC_BOOT_0
//!
//! \return true if this GPU should be used, false to ignore this GPU.
//!     If false, this GPU will not be returned by RmFindDevices().
//!
bool Gpu::PostBar0Setup(const PciDevInfo& pciDevInfo)
{
    JavaScriptPtr pJs;
    bool skipConfigSim;

    pJs->GetProperty(Gpu_Object, Gpu_SkipConfigSim, &skipConfigSim);
    if (skipConfigSim)
        return true;

    const Chip* pChip = nullptr;

    if (GetSubdeviceStatus(pciDevInfo, &pChip, m_GpusUnderTest) != gpuStatusValid)
    {
        return false;
    }

    const LwDeviceId devId = static_cast<LwDeviceId>(pChip->LwId);

    // Note: On PPC, the HWIOMMU is called TCE and it works in a different way
    // than on most other platforms.  The behavior of pci_set_dma_mask() is
    // unusual and only specific masks can be used, otherwise it will fail.
    // So on PPC, we don't set the DMA mask here and we leave it until we
    // set up the TCE bypass via another ioctl.
#ifndef LWCPU_PPC64LE
    {
        RegHalTable regHal;
        regHal.Initialize(static_cast<Device::LwDeviceId>(devId));

        // On Kepler and Maxwell, the size of addressable sysmem by the GPU was
        // 40 bits.  On Fermi it was less, but we don't support Fermi anymore.
        // Unfortunately, hwproject.h doesn't have this value defined for Kepler
        // and Pascal.  All future chips do have it defined.  So we fall back
        // to 40 if we can't read the value from RegHal.  If some future chip is
        // missing this value, it is still safer to fall back to a smaller one.
        const UINT32 numBits =
            regHal.IsSupported(MODS_CHIP_EXTENDED_SYSTEM_PHYSICAL_ADDRESS_BITS)
            ? regHal.LookupAddress(MODS_CHIP_EXTENDED_SYSTEM_PHYSICAL_ADDRESS_BITS)
            : 40U; // Fall back for Kepler and Maxwell

        Printf(Tee::PriLow, "Detected %u sysmem address bits for chip id 0x%x"
               " at %04x:%02x:%02x.%x, IOVA %s\n",
               numBits, pChip->LwId,
               pciDevInfo.PciDomain, pciDevInfo.PciBus,
               pciDevInfo.PciDevice, pciDevInfo.PciFunction,
               (pciDevInfo.InitNeeded & MODSRM_INIT_IOVASPACE) ? "enabled" : "disabled");

        const RC rc = Platform::SetDmaMask(pciDevInfo.PciDomain, pciDevInfo.PciBus,
                                           pciDevInfo.PciDevice, pciDevInfo.PciFunction,
                                           numBits);
        if (rc != OK && rc != RC::UNSUPPORTED_FUNCTION)
        {
            Printf(Tee::PriError, "SetDmaMask returned error for GPU %04x:%02x:%02x.%x\n",
                   pciDevInfo.PciDomain, pciDevInfo.PciBus,
                   pciDevInfo.PciDevice, pciDevInfo.PciFunction);
            return false;
        }
    }
#endif

    UINT32 boot0Strap;
    pJs->GetProperty(Gpu_Object, Gpu_Boot0Strap, &boot0Strap);

    static const UINT32 SOCPciDevId = 0xFA1;
    if ((pciDevInfo.PciDeviceId > 0xFFFFU) ||
        (pciDevInfo.PciDeviceId == SOCPciDevId))
    {
        // Skip straps if this is an SOC device or not on PCI bus
    }
#if LWCFG(GLOBAL_GPU_IMPL_G000)
    else if (devId >= Gpu::AMODEL)
    {
        return AmodGpuSubdevice::SetBoot0Strap(boot0Strap);
    }
#endif
#if LWCFG(GLOBAL_ARCH_HOPPER)
    else if (devId >= Gpu::GH100)
    {
        UINT32 boot3Strap;
        UINT32 gc6Strap;
        pJs->GetProperty(Gpu_Object, Gpu_Boot3Strap, &boot3Strap);
        pJs->GetProperty(Gpu_Object, Gpu_StrapGc6, &gc6Strap);
        return HopperGpuSubdevice::SetBootStraps(devId,
                                                 pciDevInfo,
                                                 boot0Strap,
                                                 boot3Strap,
                                                 m_StrapFb,
                                                 gc6Strap);
    }
#endif
#if LWCFG(GLOBAL_ARCH_ADA)
    else if (devId >= Gpu::AD102 || devId == Gpu::AD102F)
    {
        UINT32 boot3Strap;
        UINT32 gc6Strap;
        pJs->GetProperty(Gpu_Object, Gpu_Boot3Strap, &boot3Strap);
        pJs->GetProperty(Gpu_Object, Gpu_StrapGc6, &gc6Strap);
        return AdaGpuSubdevice::SetBootStraps(devId,
                                                pciDevInfo.LinLwBase,
                                                boot0Strap,
                                                boot3Strap,
                                                m_StrapFb,
                                                gc6Strap);
    }
#endif
#if LWCFG(GLOBAL_ARCH_AMPERE)
    else if (devId >= Gpu::GA100)
    {
        UINT32 boot3Strap;
        UINT32 gc6Strap;
        pJs->GetProperty(Gpu_Object, Gpu_Boot3Strap, &boot3Strap);
        pJs->GetProperty(Gpu_Object, Gpu_StrapGc6, &gc6Strap);
        return AmpereGpuSubdevice::SetBootStraps(devId,
                                                pciDevInfo.LinLwBase,
                                                boot0Strap,
                                                boot3Strap,
                                                m_StrapFb,
                                                gc6Strap);
    }
#endif
#if LWCFG(GLOBAL_ARCH_TURING)
    else if (devId >= Gpu::TU102)
    {
        UINT32 boot3Strap;
        UINT32 gc6Strap;
        pJs->GetProperty(Gpu_Object, Gpu_Boot3Strap, &boot3Strap);
        pJs->GetProperty(Gpu_Object, Gpu_StrapGc6, &gc6Strap);
        return TU100GpuSubdevice::SetBootStraps(devId,
                                                pciDevInfo.LinLwBase,
                                                boot0Strap,
                                                boot3Strap,
                                                m_StrapFb,
                                                gc6Strap);
    }
#endif
#if LWCFG(GLOBAL_ARCH_VOLTA)
    else if (devId >= Gpu::GV100)
    {
        UINT32 boot3Strap;
        UINT32 gc6Strap;
        pJs->GetProperty(Gpu_Object, Gpu_Boot3Strap, &boot3Strap);
        pJs->GetProperty(Gpu_Object, Gpu_StrapGc6, &gc6Strap);
        return VoltaGpuSubdevice::SetBootStraps(devId,
                                                pciDevInfo.LinLwBase,
                                                boot0Strap,
                                                boot3Strap,
                                                m_StrapFb,
                                                gc6Strap);
    }
#endif
#if LWCFG(GLOBAL_ARCH_PASCAL)
    else if (devId >= Gpu::GP100)
    {
        UINT32 boot3Strap;
        UINT32 gc6Strap;
        pJs->GetProperty(Gpu_Object, Gpu_Boot3Strap, &boot3Strap);
        pJs->GetProperty(Gpu_Object, Gpu_StrapGc6, &gc6Strap);
        return GP10xGpuSubdevice::SetBootStraps(devId,
                                                pciDevInfo.LinLwBase,
                                                boot0Strap,
                                                boot3Strap,
                                                m_StrapFb,
                                                gc6Strap);
    }
#endif
#if LWCFG(GLOBAL_ARCH_MAXWELL)
    else if (devId >= Gpu::GM107)
    {   // In Maxwell the registers were re-organized, so we need to use multiple
        // properties to get the same affect as Fermi boot0Strap.
        UINT32 boot3Strap;
        pJs->GetProperty(Gpu_Object, Gpu_Boot3Strap, &boot3Strap);
        return GM10xGpuSubdevice::SetBootStraps(devId,
                                                pciDevInfo.LinLwBase,
                                                boot0Strap,
                                                boot3Strap,
                                                m_StrapFb);
    }
#endif
    else
    {
        MASSERT(!"This branch doesn't support this GPU!");
    }
    return true;
}

//------------------------------------------------------------------------------
RC Gpu::PreVBIOSSetup(UINT32 GpuInstance)
{
    if (Platform::IsVirtFunMode())
    {
        return RC::OK;
    }

    RC rc;
    JavaScriptPtr pJs;
    GpuSubdevice *pGpuSubdevice = DevMgrMgr::d_GraphDevMgr->GetSubdeviceByGpuInst(GpuInstance);

    // Call chip-specific version
    //
    MASSERT(pGpuSubdevice);
    CHECK_RC(pGpuSubdevice->PreVBIOSSetup());

    // Provide a JS callback to let users do additional tweaks,
    // passing the GpuSubdevice as an argument.
    //
    JsArray Args(1);
    JsGpuSubdevice *pJsGpuSubdevice = new JsGpuSubdevice();
    pJsGpuSubdevice->SetGpuSubdevice(pGpuSubdevice);

    JSContext *pContext;
    CHECK_RC(pJs->GetContext(&pContext));
    CHECK_RC(pJsGpuSubdevice->CreateJSObject(pContext,
                                              pJs->GetGlobalObject()));
    CHECK_RC(pJs->ToJsval(pJsGpuSubdevice->GetJSObject(), &Args[0]));
    pJs->CallMethod(pJs->GetGlobalObject(), "PreVBIOSCallback", Args);

    return rc;
}

//------------------------------------------------------------------------------
RC Gpu::PostVBIOSSetup(UINT32 GpuInstance)
{
    if (Platform::IsVirtFunMode())
    {
        return RC::OK;
    }

    RC rc;
    JavaScriptPtr pJs;
    GpuSubdevice *pGpuSubdevice = DevMgrMgr::d_GraphDevMgr->GetSubdeviceByGpuInst(GpuInstance);

    // Call chip-specific version
    //
    MASSERT(pGpuSubdevice);
    CHECK_RC(pGpuSubdevice->PostVBIOSSetup());

    // Provide a JS callback to let users do additional tweaks,
    // passing the GpuSubdevice as an argument.
    //
    JsArray Args(1);
    JsGpuSubdevice *pJsGpuSubdevice = new JsGpuSubdevice();
    pJsGpuSubdevice->SetGpuSubdevice(pGpuSubdevice);
    JSContext *pContext;
    CHECK_RC(pJs->GetContext(&pContext));
    CHECK_RC(pJsGpuSubdevice->CreateJSObject(pContext,
                                              pJs->GetGlobalObject()));
    CHECK_RC(pJs->ToJsval(pJsGpuSubdevice->GetJSObject(), &Args[0]));
    pJs->CallMethod(pJs->GetGlobalObject(), "PostVBIOSCallback", Args);

    return rc;
}

//------------------------------------------------------------------------------
RC Gpu::EnumSOCDevices(UINT32 devType)
{
    vector<CheetAh::DeviceDesc> desc;
    RC rc = CheetAh::GetDeviceDescByType(devType, &desc);

    // Skip unrecognized devices
    if (rc == RC::DEVICE_NOT_FOUND)
    {
        Printf(Tee::PriDebug, "Skipping unrecognized device type %u\n", devType);
        rc.Clear();
    }

    for (UINT32 idev=0; idev < desc.size(); idev++)
    {
        SocDevInfo devInfo = {0};
        CHECK_RC(GetSOCAperture(desc[idev].devIndex,
                    &devInfo.LinApertureBase));
        devInfo.PhysApertureBase = desc[idev].apertureOffset;
        devInfo.ApertureSize     = desc[idev].apertureSize;
        devInfo.DeviceIndex      = desc[idev].devIndex;
        devInfo.DeviceType       = devType;

        UINT32 irq = 0;
        UINT32 validIrqs = 0;
        for (; (irq < desc[idev].ints.size()) && (irq < MAX_SOC_IRQS); irq++)
        {
            if (desc[idev].ints[irq].target != CheetAh::INT_TARGET_AVP)
            {
                CHECK_RC(CheetAh::GetCpuIrq(desc[idev].ints[irq], &devInfo.Irq[validIrqs++]));
            }
        }
        devInfo.NumIrqs = validIrqs;

        m_SocDevices.push_back(devInfo);
    }
    return rc;
}

//------------------------------------------------------------------------------
RC Gpu::EnumAllSOCDevicesForRM(GpuSubdevice* pSubdev)
{
    if (Platform::IsTegra())
    {
        vector<UINT32> devicesToInit;
        RC rc;
        CHECK_RC(pSubdev->GetSOCDeviceTypes(&devicesToInit, GpuSubdevice::SOC_RM));
        for (UINT32 i=0; i < devicesToInit.size(); i++)
        {
            CHECK_RC(EnumSOCDevices(devicesToInit[i]));
        }
    }
    return OK;
}

//------------------------------------------------------------------------------
RC Gpu::GetSOCAperture(UINT32 devIndex, void** ppLinAperture)
{
    if (!Platform::IsTegra())
    {
        return RC::SOFTWARE_ERROR;
    }
    return CheetAh::SocPtr()->GetAperture(devIndex, ppLinAperture);
}

//------------------------------------------------------------------------------
namespace
{
    RC HandleLackOfInterrupts()
    {
        const bool needInterrupts = Xp::GetOperatingSystem() == Xp::OS_LINUX;

        const INT32 pri = needInterrupts ? Tee::PriError : Tee::PriWarn;

        Printf(pri, "No supported interrupt types detected\n");
        if (needInterrupts)
        {
            Printf(pri,
                   "This can be caused by old Linux kernel which lacks support for new hardware\n"
                   "or by incorrect settings in SBIOS Setup.  Please make sure the system is\n"
                   "configured properly and has support for MSI or MSI-X.\n"
                   "This failure can be ignored with -poll_interrupts, but it may result in\n"
                   "test escapes.\n");
        }

        return needInterrupts ? RC::CANNOT_HOOK_INTERRUPT : RC::OK;
    }
}

//------------------------------------------------------------------------------
RC Gpu::Initialize()
{
    PROFILE_ZONE(GENERIC)

    RC rc = OK;
    JavaScriptPtr pJs;
    JSObject * pGlobObj = pJs->GetGlobalObject();

    SimClk::EventWrapper event(SimClk::Event::GPU_INIT);
    ChiplibOpScope newScope("GPU Init", NON_IRQ,
                            ChiplibOpScope::SCOPE_COMMON, NULL);

    static bool s_InitializeFailed = false;

    if (s_InitializeFailed)
    {
        Printf(Tee::PriAlways,
               "Previous initialization failed; can not attempt to initialize again\n");
        return RC::SOFTWARE_ERROR;
    }
    // Set InitializeFailed to true in case we error out before
    // reaching the end

    s_InitializeFailed = true;

    // Create the pex device manager very early so that JS properties can be
    // applied to it in the pre init callback before it is initialized
    DevMgrMgr::d_PexDevMgr = new PexDevMgr();
    PexDevMgr *pPexDevMgr = (PexDevMgr*)DevMgrMgr::d_PexDevMgr;

    {
        string  jsCallbackFunc;
        RC callbackRc = pJs->GetProperty(Gpu_Object, Gpu_PreInitCallback, &jsCallbackFunc);
        JSFunction* pFunction = 0;
        // If the function is not defined, just skip it (and don't exit or crash!)
        if ((callbackRc == RC::OK) &&
            (pJs->GetProperty(pGlobObj, jsCallbackFunc.c_str(), &pFunction) == RC::OK))
        {
            JsArray args;                           // passing in empty args
            jsval retValJs = JSVAL_NULL;
            UINT32 jsRetVal = RC::SOFTWARE_ERROR;
            CHECK_RC(pJs->CallMethod(pGlobObj, jsCallbackFunc, args, &retValJs));
            CHECK_RC(pJs->FromJsval(retValJs, &jsRetVal));
            CHECK_RC(jsRetVal);
        }
    }

    if (!Platform::IsInitialized())
    {
        Printf(Tee::PriAlways,
               "Cannot initialize Gpu until Platform is initialized\n");
        return RC::WAS_NOT_INITIALIZED;
    }

    if (s_IsInitialized)
    {
        Printf(Tee::PriLow, "Gpu is already initialized.\n");
        s_InitializeFailed = false;
        return OK;
    }

    DevMgrMgr::d_GraphDevMgr = new GpuDevMgr();
    GpuDevMgr *gpuDevMgr = DevMgrMgr::d_GraphDevMgr;

    // used to be static data private to this file before simgpu and hwgpu
    // factored out
    s_IsInitialized        = false;
    s_LwRmInitCompleted    = false;
    s_GpuDevMgrHasDevices  = false;

    bool IgnoreMissingExtPower = false;
    bool ResetExceptions = false;

    // Get various JS properties
    CHECK_RC(pJs->GetProperty(Gpu_Object, Gpu_SkipDevInit, &m_SkipDevInit));
    CHECK_RC(pJs->GetProperty(Gpu_Object, Gpu_SkipRmStateInit,
                              &m_SkipRmStateInit));
    CHECK_RC(pJs->GetProperty(Gpu_Object, Gpu_IgnoreExtPower,
                              &IgnoreMissingExtPower));
    CHECK_RC(pJs->GetProperty(Gpu_Object, Gpu_PollInterrupts,
                              &m_PollInterrupts));
    CHECK_RC(pJs->GetProperty(Gpu_Object, Gpu_AutoInterrupts,
                              &m_AutoInterrupts));
    CHECK_RC(pJs->GetProperty(Gpu_Object, Gpu_IntaInterrupts,
                              &m_IntaInterrupts));
    CHECK_RC(pJs->GetProperty(Gpu_Object, Gpu_MsiInterrupts,
                              &m_MsiInterrupts));
    CHECK_RC(pJs->GetProperty(Gpu_Object, Gpu_MsixInterrupts,
                              &m_MsixInterrupts));
    CHECK_RC(pJs->GetProperty(Gpu_Object, Gpu_SkipIntaIntrCheck,
                              &m_SkipIntaIntrCheck));
    CHECK_RC(pJs->GetProperty(Gpu_Object, Gpu_SkipMsiIntrCheck,
                              &m_SkipMsiIntrCheck));
    CHECK_RC(pJs->GetProperty(Gpu_Object, Gpu_SkipMsixIntrCheck,
                              &m_SkipMsixIntrCheck));
    CHECK_RC(pJs->GetProperty(Gpu_Object, Gpu_MonitorIntervalMs,
                              &m_GpuMonitorData.monitorIntervalMs));
    CHECK_RC(pJs->GetProperty(Gpu_Object, Gpu_MonitorFilter,
                              &m_GpuMonitorData.monitorFilter));
    CHECK_RC(pJs->GetProperty(Gpu_Object, Gpu_ThreadIntervalMs,
                              &m_ThreadIntervalMs));
    CHECK_RC(pJs->GetProperty(Gpu_Object, Gpu_ForceRepost,
                              &m_ForceGpuRepost));
    CHECK_RC(pJs->GetProperty(Gpu_Object, Gpu_SkipVbiosPost,
                              &m_SkipVbiosPost));
    CHECK_RC(pJs->GetProperty(Gpu_Object, Gpu_ResetExceptions,
                              &ResetExceptions));
    CHECK_RC(pJs->GetProperty(Gpu_Object, Gpu_SkipRestoreDisplay,
                              &m_SkipRestoreDisplay));
    CHECK_RC(pJs->GetProperty(Gpu_Object, Gpu_IsFusing,
                              &m_IsFusing));
    CHECK_RC(pJs->GetProperty(Gpu_Object, Gpu_ForceNoDevices,
                              &m_ForceNoDevices));
    CHECK_RC(pJs->GetProperty(Gpu_Object, Gpu_HulkDisableFeatures,
                              &m_HulkDisableFeatures));
    CHECK_RC(pJs->GetProperty(Gpu_Object, Gpu_HulkIgnoreErrors,
                              &m_HulkIgnoreErrors));
    CHECK_RC(pJs->GetProperty(Gpu_Object, Gpu_LoadHulkViaMods,
                              &m_LoadHulkViaMods));
    CHECK_RC(pJs->GetProperty(Gpu_Object, Gpu_SkipHulkIfRevoked,
                              &m_SkipHulkIfRevoked));
    CHECK_RC(pJs->GetProperty(Gpu_Object, Gpu_DisableFspSupport,
                              &m_DisableFspSupport));

    REGISTER_RANGE Range;
    CHECK_RC(pJs->GetProperty(pGlobObj, "g_BeginDumpAddr", &(Range.Start)));
    CHECK_RC(pJs->GetProperty(pGlobObj, "g_EndDumpAddr",   &(Range.End)));

    if (Range.Start <= Range.End)
        g_GpuLogAccessRegions.push_back(Range);

    if (Platform::GetSimulationMode() == Platform::RTL)
    {
        // RM init is about to begin
        Platform::EscapeWrite("entering_section", 0, 4, RM_INIT_SECTION);
    }

#if defined(TEGRA_MODS)
    // Ignore register whitelist.  This allows us to access any GPU registers we need.
    CHECK_RC(CheetAh::SocPtr()->ClaimGpuRegFirewallOwnership(true));
#endif

    TpcEccError::Initialize();

    DevMgrMgr::d_TestDeviceMgr = new TestDeviceMgr();
    TestDeviceMgr* pTestDeviceMgr = static_cast<TestDeviceMgr*>(DevMgrMgr::d_TestDeviceMgr);

    // Set straps, etc. in simulator.
    ConfigureSimulation();

    // Initialize SRIOV
    if (Platform::IsVirtFunMode() || Platform::IsPhysFunMode())
    {
        CHECK_RC(VmiopElwManager::Instance()->Init());
    }

    // Disable enumeration of dGPU in CheetAh MODS
    //
    // The liblwrm_gpu.so library will try to enumerate dGPU if it can,
    // with this elw var we can disable this behavior.
    //
    // We are not using Xp::SetElw here because it will overwrite the elw
    // var if it exists, but we want to allow the users to provide their own
    // overrides.
#if defined(TEGRA_MODS)
    setelw("LWRM_GPU_LWGPU_NO_PROBE_DGPU", "1", 0);
#endif

    // Find all GPU chips.
    CHECK_RC(pTestDeviceMgr->FindDevices());
    Printf(Tee::PriDebug, "Found device(s).\n");

    // Do the bridge device enumeration ahead of InitializeAll
    // so we can sort GPU's again based on relation with bridge
    CHECK_RC(pPexDevMgr->FindDevices());
    gpuDevMgr->AssignSubdeviceInstances();

    // prevent the OS from writing to FB
    for (auto pSubdev : *gpuDevMgr)
    {
        Xp::SuspendConsole(pSubdev);
    }

    bool disableEcc = false;
    bool enableEcc = false;
    CHECK_RC(pJs->GetProperty(Gpu_Object, Gpu_DisableEcc, &disableEcc));
    CHECK_RC(pJs->GetProperty(Gpu_Object, Gpu_EnableEcc, &enableEcc));

    bool skipInfoRomRowRemapping = false;
    bool disableRowRemapper = false;
    CHECK_RC(pJs->GetProperty(Gpu_Object, Gpu_SkipInfoRomRowRemapping, &skipInfoRomRowRemapping));
    CHECK_RC(pJs->GetProperty(Gpu_Object, Gpu_DisableRowRemapper, &disableRowRemapper));

    UINT32 dramclkDevinitFreqkHz;
    CHECK_RC(pJs->GetProperty(Gpu_Object, Gpu_DramclkDevInitWARkHz, &dramclkDevinitFreqkHz));

    JsArray fsArray;
    JsArray fsResetArray;
    JsArray fixFsInitMasksArray;
    pJs->GetProperty(pGlobObj, "g_FloorsweepArgs", &fsArray);
    pJs->GetProperty(pGlobObj, "g_FsReset", &fsResetArray);
    pJs->GetProperty(pGlobObj, "g_FixFsInitMasks", &fixFsInitMasksArray);

    for (auto pSubdev : *gpuDevMgr)
    {
        // User is running on emulation with a NETLIST that does not support FSP, so fallback to
        // legacy mechanism.
        if (m_DisableFspSupport)
        {
            pSubdev->ClearHasFeature(Device::Feature::GPUSUB_SUPPORTS_FSP);
        }

        bool bSupportsHoldoff = pSubdev->HasFeature(Device::Feature::GPUSUB_SUPPORTS_GFW_HOLDOFF);

        // ensure the vbios holdoff isn't set before reset
        if (bSupportsHoldoff &&
            !pSubdev->IsSOC() &&
            (Platform::GetSimulationMode() == Platform::Hardware) &&
            Platform::HasClientSideResman() &&
            pSubdev->GetGFWBootHoldoff())
        {
            CHECK_RC(pSubdev->SetGFWBootHoldoff(false));
        }

        // for maxwell/pascal/volta chips, a fundamental reset is needed to enable/disable ecc
        if (bSupportsHoldoff &&
            (disableEcc || enableEcc))
        {
            CHECK_RC(pSubdev->HotReset(GpuSubdevice::FundamentalResetOptions::Enable));
        }
    }

    UINT32 resetMode;
    CHECK_RC(pJs->GetProperty(Gpu_Object, Gpu_Reset, &resetMode));
    CHECK_RC(Reset(static_cast<ResetMode>(resetMode)));

    // make sure we don't leave holdoff bit set on error
    DEFER
    {
        for (auto pSubdev : *gpuDevMgr)
        {
            bool bSupportsHoldoff = pSubdev->HasFeature(Device::Feature::GPUSUB_SUPPORTS_GFW_HOLDOFF);
            if (bSupportsHoldoff &&
                !pSubdev->IsSOC() &&
                (Platform::GetSimulationMode() == Platform::Hardware) &&
                Platform::HasClientSideResman() &&
                pSubdev->GetGFWBootHoldoff())
            {
                pSubdev->SetGFWBootHoldoff(false);
            }
        }
    };


    // when devinit is run from ifr, coordinate with vbios ucode to handle certain arguments
    for (auto pSubdev : *gpuDevMgr)
    {
        // Check to see if SCPM has been ilwoked.
        CHECK_RC(CheckScpm(pSubdev));

        // check if floorsweep args were passed in
        bool needFloorsweeping = false;
        string fsArgs;
        UINT32 devInst = pSubdev->DevInst();
        if ((devInst < fsArray.size()) &&
            (RC::OK == pJs->FromJsval(fsArray[devInst], &fsArgs)) &&
            !fsArgs.empty())
        {
            needFloorsweeping = true;
        }

        bool jsResetFs;
        bool resetFs = false;
        if ((devInst < fsResetArray.size()) &&
            (RC::OK == pJs->FromJsval(fsResetArray[devInst], &jsResetFs)))
        {
            resetFs = jsResetFs;
        }

        bool jsFixFsInitMasks;
        bool fixFsInitMasks = false;
        if ((devInst < fixFsInitMasksArray.size()) &&
            (RC::OK == pJs->FromJsval(fixFsInitMasksArray[devInst], &jsFixFsInitMasks)))
        {
            fixFsInitMasks = jsFixFsInitMasks;
        }

        bool bReqdReset = false;
        bool bHasNonFsOpt = false;
        if (disableEcc || enableEcc || dramclkDevinitFreqkHz || skipInfoRomRowRemapping || disableRowRemapper)
        {
            bHasNonFsOpt = true;
        }
        if (needFloorsweeping || resetFs || fixFsInitMasks || bHasNonFsOpt)
        {
            // In case we use the FSP flow, we need to do the reset after FSing has been applied
            bReqdReset = true;
        }

        // Wait for FSP init to complete after any reset
        if (pSubdev->HasBug(3471757) && (Platform::GetSimulationMode() == Platform::Hardware))
        {
            bool bRequiresBootComplete = !(needFloorsweeping || resetFs || fixFsInitMasks || m_SkipDevInit);
            RC bootPollRc = pSubdev->PollGFWBootComplete(bRequiresBootComplete ?
                                                         Tee::PriNormal : Tee::PriLow);
            if (bootPollRc != RC::OK)
            {
                if (!bRequiresBootComplete)
                {
                    Printf(Tee::PriWarn, "Boot did not complete, may be due to incorrect FS config\n");
                }
                else
                {
                    CHECK_RC(bootPollRc);
                }
            }
        }

        bool bSupportsHoldoff = pSubdev->HasFeature(Device::Feature::GPUSUB_SUPPORTS_GFW_HOLDOFF);
        if (bReqdReset &&
            bSupportsHoldoff &&
            !pSubdev->IsSOC() &&
            (Platform::GetSimulationMode() == Platform::Hardware) &&
            Platform::HasClientSideResman() &&
            pSubdev->IsGFWBootStarted())
        {
            // set the holdoff bit and reset
            CHECK_RC(pSubdev->SetGFWBootHoldoff(true));
            CHECK_RC(pSubdev->GetFsImpl()->SwReset(false));

            // use old sequence if devinit didn't run
            if (!pSubdev->IsGFWBootStarted())
            {
                pSubdev->SetGFWBootHoldoff(false);
            }
        }

        // TODO untested at this location
        // If we want to enable or disable ECC, DisableEcc/EnableEcc must be
        // called before we initialize RM. RM will do a devinit because we
        // have done a hot reset since -disable_ecc is tied to -hot_reset
        if (disableEcc && enableEcc)
        {
            Printf(Tee::PriError, "Cannot use -disable_ecc with -enable_ecc!\n");
            return RC::BAD_COMMAND_LINE_ARGUMENT;
        }

        if (!pSubdev->HasBug(3472315) && bHasNonFsOpt)
        {
            if (disableEcc)
            {
                CHECK_RC(DisableEcc());
            }
            if (enableEcc)
            {
                CHECK_RC(EnableEcc());
            }

            if (disableRowRemapper)
            {
                CHECK_RC(DisableRowRemapper());
            }

            if (skipInfoRomRowRemapping)
            {
                CHECK_RC(SkipInfoRomRowRemapping());
            }

            // TODO untested at this location
            // If the user provides an override frequency for dramclk on GP100,
            // now is the time to apply it
            if (dramclkDevinitFreqkHz)
            {
                CHECK_RC(DramclkDevInitWARkHz(dramclkDevinitFreqkHz));
            }
        }

        // Setup Floorsweeping Args
        PROFILE_NAMED_ZONE(GENERIC, "Run FS callbacks")
        JsArray Params;
        jsval arg1;
        jsval retValJs = JSVAL_NULL;
        UINT32 retVal = RC::SOFTWARE_ERROR;

        // Get a JsGpuSubdevice to send to JavaScript
        JsGpuSubdevice * pJsGpuSubdevice = new JsGpuSubdevice();
        MASSERT(pJsGpuSubdevice);
        pJsGpuSubdevice->SetGpuSubdevice(pSubdev);

        JSContext *pContext;
        CHECK_RC(pJs->GetContext(&pContext));
        CHECK_RC(pJsGpuSubdevice->
                 CreateJSObject(pContext,
                                pJs->GetGlobalObject()));

        CHECK_RC(pJs->ToJsval(pJsGpuSubdevice->GetJSObject(), &arg1));
        Params.push_back(arg1);

        CHECK_RC(pJs->CallMethod(pGlobObj, "ProcessFloorsweepingArgs",
                                 Params, &retValJs));
        CHECK_RC(pJs->FromJsval(retValJs, &retVal));

        if (RC::UNDEFINED_JS_METHOD == retVal)
            retVal = RC::OK;

        CHECK_RC(retVal);

        const bool bSupportsFsp = (pSubdev->HasFeature(Device::Feature::GPUSUB_SUPPORTS_FSP) &&
                                  (Platform::GetSimulationMode() == Platform::Hardware));
        if (bSupportsFsp && bSupportsHoldoff)
        {
            Printf(Tee::PriError,
                   "Can't have both GPUSUB_SUPPORTS_FSP & GPUSUB_SUPPORTS_GFW_HOLDOFF enabled!\n");
            return RC::SOFTWARE_ERROR;
        }
        if (bSupportsFsp)
        {
            // TODO maybe HotReset is wiping away state here...
            // Apply floorsweeping args
            CHECK_RC(PreVBIOSSetup(pSubdev->GetGpuInst()));

            if (bReqdReset)
            {
                if (pSubdev->HasBug(3472315))
                {
                    CHECK_RC(pSubdev->HotReset(TestDevice::FundamentalResetOptions::Disable));
                }
                else
                {
                    CHECK_RC(pSubdev->FLReset());
                }
            }
            if (!m_SkipDevInit)
            {
                // Wait for FSP init to complete
                CHECK_RC(pSubdev->PollGFWBootComplete(Tee::PriNormal));
            }

            // Other FSP options can't be used with FSing due to issues with FLR on some configs
            if (pSubdev->HasBug(3472315) && bHasNonFsOpt)
            {
                // Note that this will only work until FW FSing is not enabled
                // Once that is enabled (and if the FLR issue is not fixed yet), we would need to indicate
                // the MODS FS option even in this section to ensure that VBIOS FSing is not exelwted

                if (disableEcc)
                {
                    CHECK_RC(DisableEcc());
                }
                if (enableEcc)
                {
                    CHECK_RC(EnableEcc());
                }

                if (disableRowRemapper)
                {
                    CHECK_RC(DisableRowRemapper());
                }

                if (skipInfoRomRowRemapping)
                {
                    CHECK_RC(SkipInfoRomRowRemapping());
                }

                if (dramclkDevinitFreqkHz)
                {
                    CHECK_RC(DramclkDevInitWARkHz(dramclkDevinitFreqkHz));
                }

                CHECK_RC(pSubdev->FLReset());

                CHECK_RC(pSubdev->PollGFWBootComplete(Tee::PriNormal));
            }
        }
    }


    if (!Platform::IsVirtFunMode())
    {
        PROFILE_NAMED_ZONE(GENERIC, "InitializeAll")
        CHECK_RC(pTestDeviceMgr->InitializeAll());
    }

    bool checkXenCompatibility = false;
    CHECK_RC(pJs->GetProperty(Gpu_Object, Gpu_CheckXenCompatibility,
                              &checkXenCompatibility));
    if (checkXenCompatibility)
    {
        for (auto pSubdev : *gpuDevMgr)
        {
            CHECK_RC(pSubdev->CheckXenCompatibility());
        }
    }

    // Enumerate SOC devices
    for (auto pSubdev : *gpuDevMgr)
    {
        if (pSubdev->IsSOC())
        {
#if defined(TEGRA_MODS)
            m_pTegraRm.reset(new TegraRm);
            CHECK_RC(m_pTegraRm->Initialize());
#else
            if (Platform::HasClientSideResman())
            {
                CHECK_RC(EnumAllSOCDevicesForRM(pSubdev));
            }
#endif
            break; // only one SOC GPU supported
        }
    }

    // Perform any GPU MODS specific SOC initializations.
    if (!m_SocDevices.empty())
    {
        MASSERT(Platform::HasClientSideResman());
        CHECK_RC(CheetAh::SocPtr()->InitializeGPUClocks());
    }

    // Reset exceptions
    if (ResetExceptions)
    {
        for (auto pSubdev : *gpuDevMgr)
        {
            pSubdev->ResetExceptionsDuringShutdown(true);
        }
    }

    // Print more information before we attempt to initialize the GPUs.
    if (Platform::HasClientSideResman())
    {
        PROFILE_NAMED_ZONE(GENERIC, "PreCreateGpuDeviceObjects")
        // Enumerate devices before initializing them in RM.
        CHECK_RC(gpuDevMgr->PreCreateGpuDeviceObjects());

        CHECK_RC(Utility::CallOptionalJsMethod("PreGpuInitCallback"));
    }

    // Resolve conflicts between the various -XXX_interrupts and
    // -skip_XXX_intr_check flags.  After this section, exactly one of
    // m_PollInterrupts, m_MsixInterrupts, m_MsiInterrupts, m_IntaInterrupts, and
    // m_AutoInterrupts will be true.
    struct { const char *name; bool *pValue; } intFlags[] =
    {
        { "-poll_interrupts", &m_PollInterrupts },
        { "-msi_interrupts",  &m_MsiInterrupts  },
        { "-msix_interrupts", &m_MsixInterrupts },
        { "-inta_interrupts", &m_IntaInterrupts },
        { "-auto_interrupts", &m_AutoInterrupts }
    };
    for (size_t ii = 0; ii < sizeof(intFlags)/sizeof(intFlags[0]); ++ii)
    {
        for (size_t jj = ii+1; jj < sizeof(intFlags)/sizeof(intFlags[0]); ++jj)
        {
            if (*intFlags[ii].pValue && *intFlags[jj].pValue)
            {
                Printf(Tee::PriError, "%s conflicts with %s.\n",
                       intFlags[ii].name, intFlags[jj].name);
                return RC::BAD_COMMAND_LINE_ARGUMENT;
            }
        }
    }

    if (Platform::IsVirtFunMode())
    {
        // on VF interrupt's are handled using MSI-X or polling

        m_SkipIntaIntrCheck = true;
        m_SkipMsiIntrCheck = true;

        if (!m_PollInterrupts)
            m_MsixInterrupts = true;

        Printf(Tee::PriLow, "Skipping legacy interrupts and MSI on VF\n");
    }

    if (!Platform::IRQTypeSupported(MODS_XP_IRQ_TYPE_MSIX))
    {
        m_SkipMsixIntrCheck = true;
        if (m_MsixInterrupts)
        {
            Printf(Tee::PriError,
               "-msi_interrupts specified and platform does not support MSI-X.\n");
            return RC::BAD_COMMAND_LINE_ARGUMENT;
        }

        Printf(Tee::PriLow, "MSI-X not supported on this platform, skipping\n");
    }

    if (m_MsiInterrupts && m_SkipMsiIntrCheck)
    {
        Printf(Tee::PriError,
               "-msi_interrupts conflicts with -skip_msi_intr_check.\n");
        return RC::BAD_COMMAND_LINE_ARGUMENT;
    }
    if (m_MsixInterrupts && m_SkipMsixIntrCheck)
    {
        Printf(Tee::PriError,
               "-msix_interrupts conflicts with -skip_msix_intr_check.\n");
        return RC::BAD_COMMAND_LINE_ARGUMENT;
    }
    if (m_IntaInterrupts && m_SkipIntaIntrCheck)
    {
        Printf(Tee::PriError,
            "-inta_interrupts conflicts with -skip_inta_intr_check.\n");
        return RC::BAD_COMMAND_LINE_ARGUMENT;
    }

    if (!m_PollInterrupts && !m_AutoInterrupts &&
        !m_IntaInterrupts && !m_MsiInterrupts && !m_MsixInterrupts)
    {
        // If the user didn't choose any interrupt-mode flag, pick one.
        bool supportsMsi  = !m_SkipMsiIntrCheck;
        bool supportsMsix = !m_SkipMsixIntrCheck;
        bool supportsInta = !m_SkipIntaIntrCheck;

        for (auto pSubdev : *gpuDevMgr)
        {
            if (!pSubdev->IsSOC())
            {
                if (supportsInta)
                {
                    if (!pSubdev->IsHookIntSupported())
                    {
                        supportsInta = false;
                        Printf(Tee::PriLow, "Legacy interrupts not supported on %s, skipping\n",
                               pSubdev->GetName().c_str());
                    }
                }

                if (supportsMsi)
                {
                    if (!pSubdev->IsHookMsiSupported() ||
                        pSubdev->GetMsiCapOffset() == 0)
                    {
                        supportsMsi = false;
                        Printf(Tee::PriLow, "MSI not supported on %s, skipping\n",
                               pSubdev->GetName().c_str());
                    }
                }

                if (supportsMsix)
                {
                    if (!pSubdev->IsHookMsixSupported() ||
                        pSubdev->GetMsixCapOffset() == 0)
                    {
                        supportsMsix = false;
                        Printf(Tee::PriLow, "MSI-X not supported on %s, skipping\n",
                               pSubdev->GetName().c_str());
                    }
                }
            }
        }

#if defined(SIM_BUILD)
        // on sim builds default needs to be Inta if supported
        // to provide consistent testing accross chips
        if (supportsInta)
            m_IntaInterrupts = true;
        else
            m_PollInterrupts = true;
#else
        if (supportsMsix)
            m_MsixInterrupts = true;
        else if (supportsMsi)
            m_MsiInterrupts = true;
        else if (supportsInta)
            m_IntaInterrupts = true;
        else
            m_PollInterrupts = true;
#endif

        string intrTypes;
        if (supportsInta)
            intrTypes += " legacy";
        else
            m_SkipIntaIntrCheck = true;
        if (supportsMsi)
            intrTypes += " MSI";
        else
            m_SkipMsiIntrCheck = true;
        if (supportsMsix)
            intrTypes += " MSI-X";
        else
            m_SkipMsixIntrCheck = true;
        if (intrTypes.empty())
            CHECK_RC(HandleLackOfInterrupts());
        else
            Printf(Tee::PriLow, "Detected supported interrupt types:%s\n", intrTypes.c_str());
    }
    else
    {
        // Skip unnecessary interrupt checks:
        // -msix_interrupts implies -skip_inta_intr_check, -skip_msi_intr_check
        // -msi_interrupts  implies -skip_inta_intr_check, -skip_msix_intr_check
        // -inta_interrupts implies -skip_msi_intr_check,  -skip_msix_intr_check
        // -poll_interrupts implies -skip_msix_intr_check, skip_msi_intr_check, skip_inta_intr_check
        if (m_MsiInterrupts || m_PollInterrupts)
        {
            m_SkipIntaIntrCheck = true;
            m_SkipMsixIntrCheck = true;
        }
        if (m_MsixInterrupts || m_PollInterrupts)
        {
            m_SkipIntaIntrCheck = true;
            m_SkipMsiIntrCheck = true;
        }
        if (m_IntaInterrupts || m_PollInterrupts)
        {
            m_SkipMsiIntrCheck = true;
            m_SkipMsixIntrCheck = true;
        }
    }

    bool allowAutoMsix = false;
    bool allowAutoMsi = false;
    bool allowAutoInta = false;
    CHECK_RC(ValidateIsr(&allowAutoMsix, &allowAutoMsi, &allowAutoInta));

    if (m_AutoInterrupts && !allowAutoInta && !allowAutoMsi && !allowAutoMsix)
    {
        CHECK_RC(HandleLackOfInterrupts());
    }

    if (m_SkipDevInit)
    {
        // Fake the setup done by CreateGpuDeviceObjects()
        CHECK_RC(gpuDevMgr->CreateGpuDeviceObjectsWithoutResman());
        s_GpuDevMgrHasDevices = true;

        if (m_IsFusing)
        {
            CHECK_RC(gpuDevMgr->InitializeAllWithoutRm());
        }

        // We're done
        // hewu: we need to rename IsInitialized to something.. this would be
        // more accurate if we rename s_IsInitialied to s_DevicesFound
        s_IsInitialized=true;
        s_InitializeFailed = false;
        s_RmInitCompleted = false;
        return OK;
    }

    CHECK_RC(HookIsr(allowAutoMsix, allowAutoMsi, allowAutoInta));

    // Ensure we shut down LwRm correctly
    s_LwRmInitCompleted = true;

    // Alloc the global Clients (Root object).
    for (UINT32 client = 0; client < LwRmPtr::GetValidClientCount(); client++)
    {
        CHECK_RC(LwRmPtr(client)->AllocRoot());
    }

    if (g_FieldDiagMode == FieldDiagMode::FDM_617)
    {
        // In the version of MODS that uses kernel side RM we have to update the
        // registry manually in contrast to the client side RM where the RM just
        // calls a MODS callback to retrieve a registry value. The manual update
        // has to happen just right after allocating the root with
        // LwRmAllocRoot, before any other RM calls have a chance to open
        // /dev/lwpu.
        CHECK_RC(Registry::ForEachEntry("ResourceManager", "",
                 [](auto node, auto param, auto data)->RC
                 {
                    return LwRmPtr()->WriteRegistryDword(node, param, data);
                 }
                 ));
    }

    // Initialize PexDevMgr before creating the JsGpuSubdevice() so when
    // PerGpuInitializeCallback() is exelwted we can setup the ASPM settings
    // before RM tries to initialize the card.


    CHECK_RC(pPexDevMgr->InitializeAll());

    // Run JS callbacks for each GPU
    for (auto pSubdev : *gpuDevMgr)
    {
        if (Platform::HasClientSideResman())
        {
            PROFILE_NAMED_ZONE(GENERIC, "Run JS callbacks")
            JsArray Params;
            jsval arg1;
            jsval retValJs = JSVAL_NULL;
            UINT32 retVal = RC::SOFTWARE_ERROR;

            // Get a JsGpuSubdevice to send to JavaScript
            JsGpuSubdevice * pJsGpuSubdevice = new JsGpuSubdevice();
            MASSERT(pJsGpuSubdevice);
            pJsGpuSubdevice->SetGpuSubdevice(pSubdev);

            JSContext *pContext;
            CHECK_RC(pJs->GetContext(&pContext));
            CHECK_RC(pJsGpuSubdevice->
                     CreateJSObject(pContext,
                                    pJs->GetGlobalObject()));

            CHECK_RC(pJs->ToJsval(pJsGpuSubdevice->GetJSObject(), &arg1));
            Params.push_back(arg1);

            CHECK_RC(pJs->CallMethod(pGlobObj, "PerGpuInitializeCallback",
                                     Params, &retValJs));
            CHECK_RC(pJs->FromJsval(retValJs, &retVal));

            if (RC::UNDEFINED_JS_METHOD == retVal)
                retVal = RC::OK;

            CHECK_RC(retVal);
        }
    }

    map<UINT32, string> GpuBiosFileNames = CommandLine::GpuBiosFileNames();

    // Initialize SOC devices in RM, including GPU on the SOC
    if (!m_SocDevices.empty())
    {
        // Get VBIOS and IRQ state of the SOC GPU
        vector<UINT08> socVBiosImage;
        bool irqsHooked = false;
        for (auto pSubdev : *gpuDevMgr)
        {
            if (pSubdev->IsSOC())
            {
                CHECK_RC(pSubdev->GetVBiosImage(&socVBiosImage));
                irqsHooked = pSubdev->InterruptsHooked();
                break; // only one SOC GPU supported
            }
        }

        // Get firmware for the SOC
        vector<UINT08> socFirmwareImage;
        if (GpuBiosFileNames.count(CommandLine::SOC_FIRMWARE))
        {
            const string file = GpuBiosFileNames[CommandLine::SOC_FIRMWARE];
            CHECK_RC(GpuSubdevice::GetSocBiosImage(file, &socFirmwareImage));
            GpuBiosFileNames.erase(CommandLine::SOC_FIRMWARE);
        }

        DEFER
        {
            // Don't dangle temporary image pointers
            m_SocInitInfo.BiosImage     = nullptr;
            m_SocInitInfo.BiosImageSize = 0;
            m_SocInitInfo.FirmwareImage = nullptr;
        };

        // Init SOC in RM
        m_SocInitInfo.BiosImage     = socVBiosImage.empty() ? 0 : socVBiosImage.data();
        m_SocInitInfo.BiosImageSize = static_cast<UINT32>(socVBiosImage.size());
        m_SocInitInfo.FirmwareImage = socFirmwareImage.empty() ? 0 : socFirmwareImage.data();
        m_SocInitInfo.Devices       = &m_SocDevices[0];
        m_SocInitInfo.NumDevices    = static_cast<UINT32>(m_SocDevices.size());
        CHECK_RC(CheetAh::ReadChipIdRegister(&m_SocInitInfo.ChipId));
        if (irqsHooked)
        {
            m_SocInitInfo.InitDone |= MODSRM_INIT_ISR;
        }

        if (RmInitSoc(&m_SocInitInfo) != LW_OK)
        {
            Printf(Tee::PriLow, "Failed to initialize SOC in RM\n");
            return RC::WAS_NOT_INITIALIZED;
        }
    }

    MASSERT(!m_SkipDevInit); // Shouldn't make it this far if skipping devinit

    for (auto pSubdev : *gpuDevMgr)
    {
        PROFILE_NAMED_ZONE(GENERIC, "Wait for GFW boot")

        bool bSupportsHoldoff = pSubdev->HasFeature(Device::Feature::GPUSUB_SUPPORTS_GFW_HOLDOFF);

        // Check if the vbios ucode is waiting to run devinit
        if (!pSubdev->IsSOC() &&
            (Platform::GetSimulationMode() == Platform::Hardware) &&
            Platform::HasClientSideResman() &&
            pSubdev->IsGFWBootStarted())
        {
            if (bSupportsHoldoff && pSubdev->GetGFWBootHoldoff())
            {
                CHECK_RC(PreVBIOSSetup(pSubdev->GetGpuInst()));

                // Release holdoff, letting the vbios ucode run devinit
                CHECK_RC(pSubdev->SetGFWBootHoldoff(false));

                CHECK_RC(pSubdev->PollGFWBootComplete(Tee::PriNormal));
            }
        }
    }

    UINT32 hbmDeviceInitMethod = 0;
    CHECK_RC(pJs->GetProperty(Gpu_Object, Gpu_HbmDeviceInitMethod, &hbmDeviceInitMethod));

    // Collect DRAM information from the VBIOS preOS PBI before RM is
    // initialized, if required. Used to work around elevated PLMs on the
    // interface to the DRAM (HBM/GDDR).
    //
    // NOTE: The preOS PBI interface is available after VBIOS IFR and runs as a
    // task on the PMU. Once RM loads, it resets the PMU and interferes with the
    // preOS PBI task. The task will potentially return an unknown error status
    // if used. It will have unexpected behaviour.
    vector<GpuDramDevIds> allGpusDramDevIds;
    if (!Platform::IsVirtFunMode() && !Platform::IsTegra() &&
        !m_SkipDevInit && // Needs devinit for the VBIOS preOS interface to run
        hbmDeviceInitMethod != Gpu::DoNotInitHbm)
    {
        allGpusDramDevIds.reserve(gpuDevMgr->NumGpus());

        for (auto pSubdev : *gpuDevMgr)
        {
            PROFILE_NAMED_ZONE(GENERIC, "Get DRAM info")

            allGpusDramDevIds.emplace_back(pSubdev);
            GpuDramDevIds& gpuDramDevIds = allGpusDramDevIds.back();

            // NOTE: The GPU family is not yet available from
            // GpuDevice::GetFamily.  We have to determine the family from the
            // device ID. This leaves out Direct Amodel since it doesn't have a
            // device ID set. Does not matter since there is no HBM to read on
            // Directed Amodel.
            using GpuFamily = GpuDevice::GpuFamily;

            GpuDevice* pParentDev = pSubdev->GetParentDevice();
            if (!pParentDev)
            {
                // Parent has not been assigned yet.
                // NOTE: Unclear what causes this situation to occur. Happens
                // when using a kernel mode RM setups.
                Printf(Tee::PriWarn, "GPU subdevice's parent device has not "
                       "been set. Unable to determine GPU family during early "
                       "initialization.\n");
                MASSERT(!gpuDramDevIds.hasData);
                continue;
            }

            const GpuFamily gpuFamily = pParentDev->GetFamilyFromDeviceId();
            if (gpuFamily < GpuFamily::Ampere)
            {
                // Skip checking the VBIOS preOS PBI interface.
                MASSERT(!gpuDramDevIds.hasData);
                continue;
            }

            rc = VbiosPreOsPbi::GetDramInformation(pSubdev,
                                                   &gpuDramDevIds.dramDevIds);
            if (rc != RC::OK)
            {
                Printf(Tee::PriLow, "Failed to get DRAM information from "
                       "VBIOS preOS PBI. Will attempt to read from the "
                       "DRAM.\n");
                rc.Clear();
                MASSERT(!gpuDramDevIds.hasData);
                continue;
            }

            gpuDramDevIds.hasData = true;
        }
    }

    // Apply poison overrides (Needs to be run after devinit, and before RM init)
    JsArray poisonEnArray;
    CHECK_RC(pJs->GetProperty(pGlobObj, "g_EnablePoison", &poisonEnArray));
    for (auto pSubdev : *gpuDevMgr)
    {
        if (pSubdev->IsSOC())
            continue;

        UINT32 devInst    = pSubdev->DevInst();
        bool enablePoison = false;
        if ((devInst < poisonEnArray.size()) &&
            (RC::OK == pJs->FromJsval(poisonEnArray[devInst], &enablePoison)))
        {
            CHECK_RC(pSubdev->OverridePoisolwal(enablePoison));
        }

        // SM Internal Error Containment Override
        bool enSmInternalErrorContainment;
        if (pJs->GetProperty(pGlobObj, "g_EnSmInternalErrorContainment", &enSmInternalErrorContainment) == RC::OK)
        {
            CHECK_RC(pSubdev->OverrideSmInternalErrorContainment(enSmInternalErrorContainment));
        }

        // GCC and CTXSW Poison Error Containment Override
        bool enGccCtxswPoisonContainment;
        if (pJs->GetProperty(pGlobObj, "g_EnGccCtxswPoisonContainment", &enGccCtxswPoisonContainment) == RC::OK)
        {
            CHECK_RC(pSubdev->OverrideGccCtxswPoisonContainment(enGccCtxswPoisonContainment));
        }
    }

    // Load HULK (with via MODS, or pass args to RM if to be loaded by RM)
    CHECK_RC(LoadHulkLicence());
    bool bStopInitAfterHulk = false;
    CHECK_RC(pJs->GetProperty(Gpu_Object, Gpu_StopInitAfterHulk, &bStopInitAfterHulk));
    if (bStopInitAfterHulk)
    {
        // Fake the setup done by CreateGpuDeviceObjects()
        CHECK_RC(gpuDevMgr->CreateGpuDeviceObjectsWithoutResman());
        s_GpuDevMgrHasDevices = true;
        s_IsInitialized       = true;
        s_InitializeFailed    = false;
        s_RmInitCompleted     = false;
        return RC::OK;
    }

    // Initialize non-SOC GPU devices in RM.
    // This init has to happen after SOC has been initialized
    // in case there are external PCI GPUs on CheetAh.
    // Also discard VBIOSes which were used by any GPUs.
    for (auto pSubdev : *gpuDevMgr)
    {
        if (!pSubdev->IsSOC()) // SOC GPUs already initialized by RmInitSoc
        {
            // If VBIOS supports the IFR-driven Miata boot flow, then wait for it to finish
            if (Platform::GetSimulationMode() == Platform::Hardware &&
                pSubdev->HasFeature(Device::Feature::GPUSUB_SUPPORTS_GFW_HOLDOFF) &&
                Platform::HasClientSideResman() &&
                pSubdev->IsGFWBootStarted())
            {
                CHECK_RC(pSubdev->PollGFWBootComplete(Tee::PriNormal));
            }

            // Check certain RM registries before RM init
            CHECK_RC(pSubdev->ValidatePreInitRmRegistries());

            // Now tell the resman to init the card.
            // Broken for SLI, but this code block is a no-op on WinXP anyway
            //
            CHECK_RC(pSubdev->InitGpuInRm(m_SkipRmStateInit,
                                          m_ForceGpuRepost,
                                          m_SkipVbiosPost,
                                          false));
            CHECK_RC(pSubdev->PostRmInit());
        }

        // Cross this GPU off our local copy of VBIOS filenames,
        // to check for extraneous VBIOSes on the command line.
        //
        GpuBiosFileNames.erase(pSubdev->GetGpuInst());
    }

    // We removed entries from the local GpuBiosFileNames variable as
    // they were used.  If there are any left over, then the caller
    // passed too many -gpubios arguments.
    if (!GpuBiosFileNames.empty())
    {
        for (map<UINT32, string>::iterator iter = GpuBiosFileNames.begin();
             iter != GpuBiosFileNames.end(); ++iter)
        {
            Printf(Tee::PriAlways,
                   "Error: A -gpubios argument was given for nonexistent"
                   " device %d.\n",
                   iter->first);
        }
        return RC::BAD_COMMAND_LINE_ARGUMENT;
    }

    // Dump the registers according to the -dump_regs
    // commandline options
    JsArray devInstArray;
    if (OK == pJs->GetProperty(pGlobObj, "g_DumpRegs", &devInstArray))
    {
        for (GpuDevice *pDev = gpuDevMgr->GetFirstGpuDevice();
             pDev != NULL; pDev = gpuDevMgr->GetNextGpuDevice(pDev))
        {
            const UINT32 devInst = pDev->GetDeviceInst();
            if (devInst >= devInstArray.size())
            {
                continue;
            }
            JsArray rangeArray;
            // if we use only -dev 1 in the mods command line option,
            // g_DumpRegs[0] will be 'undefined', so we need to skip this.
            if (OK != pJs->FromJsval(devInstArray[devInst], &rangeArray))
            {
                continue;
            }

            for (UINT32  i=0; i<pDev->GetNumSubdevices(); i++)
            {
                Printf(Tee::PriNormal,
                       "Dumping Registers for Device:%u,Subdevice:%u\n",
                       devInst, i);
                for (UINT32 j=0; j <rangeArray.size(); j++)
                {
                    JSObject * pRangeObject;
                    UINT32 startAddr;
                    UINT32 endAddr;
                    CHECK_RC(pJs->FromJsval(rangeArray[j], &pRangeObject));
                    CHECK_RC(pJs->GetProperty(pRangeObject, "intStartAddr", &startAddr));
                    CHECK_RC(pJs->GetProperty(pRangeObject, "intEndAddr", &endAddr));
                    if (startAddr <= endAddr)
                    {
                        pDev->GetSubdevice(i)->DumpRegs(startAddr, endAddr);
                    }
                }
            }
        }
    }

    // Show the available devices.
    Printf(Tee::PriLow, "Found GPUs:\n");
    for (auto pSubdev : *gpuDevMgr)
    {
        auto pGpuPcie = pSubdev->GetInterface<Pcie>();
        Printf(Tee::PriLow,
                "   DevInst %u  %04x:%02x:%02x.%x  DevId:%04x  ChipId:%x  GpuInst %d  %s\n",
                pSubdev->DevInst(),
                pGpuPcie->DomainNumber(),
                pGpuPcie->BusNumber(),
                pGpuPcie->DeviceNumber(),
                pGpuPcie->FunctionNumber(),
                pGpuPcie->DeviceId(),
                pSubdev->ChipArchId(),
                pSubdev->GetGpuInst(),
                pSubdev->IsPrimary() ? "(vga)" : ""
                );
    }

    // Allocate and Initialize HBM device
    if (!Platform::IsVirtFunMode() &&
        hbmDeviceInitMethod != Gpu::DoNotInitHbm)
    {
        DevMgrMgr::d_HBMDevMgr = new HBMDevMgr();
        HBMDevMgr *pHBMDevMgr = (HBMDevMgr*)DevMgrMgr::d_HBMDevMgr;
        CHECK_RC(pHBMDevMgr->FindDevices());
        pHBMDevMgr->SetInitMethod(static_cast<HbmDeviceInitMethod>(hbmDeviceInitMethod));

        // Internally in the HBM Initialization the HBM info gets stored in their associated GPU
        // subdevice since we may teardown the HBM devices before RM inits

        RC dramInitRc;
        if (allGpusDramDevIds.empty())
        {
            dramInitRc = pHBMDevMgr->InitializeAll();
        }
        else
        {
            dramInitRc = pHBMDevMgr->InitializeAll(allGpusDramDevIds);
        }

        bool ignoreConfigCheck = 0;
        CHECK_RC(pJs->GetProperty(Gpu_Object, Gpu_HbmCfgCheckIgnore, &ignoreConfigCheck));
        if (dramInitRc == RC::UNSUPPORTED_DEVICE && ignoreConfigCheck)
        {
            Printf(Tee::PriWarn, "Ignore unsupported HBM config since"
                                "-fb_hbm_config_check_ignore was specified\n");
        }
        else if (dramInitRc != RC::PRIV_LEVEL_VIOLATION)
        {
            CHECK_RC(dramInitRc);
        }

        // If we're initializing HBM temporarily, destroy the device now to avoid any HBM device
        // contention with RM
        if (hbmDeviceInitMethod == Gpu::InitHbmTempDefault)
        {
            ((HBMDevMgr*)DevMgrMgr::d_HBMDevMgr)->ShutdownAll();
            delete DevMgrMgr::d_HBMDevMgr;
            DevMgrMgr::d_HBMDevMgr = nullptr;
        }
    }

    // Have the gpu device manager find the devices
    if (!m_SkipRmStateInit)
    {
        CHECK_RC(gpuDevMgr->CreateGpuDeviceObjects());
        s_GpuDevMgrHasDevices = true;
    }
    else
    {
        CHECK_RC(gpuDevMgr->CreateGpuDeviceObjectsWithoutResman());
        s_GpuDevMgrHasDevices = true;
        CHECK_RC(gpuDevMgr->InitializeAllWithoutRm());

        // We're done
        s_IsInitialized    = true;
        s_InitializeFailed = false;
        return OK;
    }

    // Initialize (and allocate) all of the devices
    rc = gpuDevMgr->InitializeAll();
    if (OK != rc)
    {
        SetCriticalEvent(Utility::CE_FATAL_ERROR);
        return rc;
    }

    s_RmInitCompleted = true;

    // Initialize all devices after RM (i.e. this will
    // initialize any devices that depend upon RM - devices that do not depend
    // upon RM have already been found/initialized)
    if (!Platform::IsVirtFunMode())
    {
        CHECK_RC(pTestDeviceMgr->InitializeAll());

        CHECK_RC(LwLinkDevIf::TopologyManager::Initialize());
        CHECK_RC(LwLinkDevIf::TopologyManager::Setup());
        CHECK_RC(LwLinkDevIf::TopologyManager::Print(Tee::PriLow));
    }

    // Start the 1Hz callback thread
    if (Platform::HasClientSideResman()
        && (Xp::GetOperatingSystem() != Xp::OS_LINUXSIM)
        && (Xp::GetOperatingSystem() != Xp::OS_WINSIM))
    {
        s_OneHzMutex = Tasker::AllocMutex("Gpu::s_OneHzMutex", Tasker::mtxUnchecked);
        s_OneHzCallbackThreadID = Tasker::CreateThread(OneHzCallbackThread,
            0, 0, "RMCallback1Hz");
        if (s_OneHzCallbackThreadID == Tasker::NULL_THREAD)
        {
            Printf(Tee::PriWarn, "Unable to create 1Hz callback thread\n");
        }
    }

    bool preserveFloorsweeping = false;
    CHECK_RC(pJs->GetProperty(pGlobObj, "g_PreserveFloorsweeping", &preserveFloorsweeping));

    bool printPreservedFs = false;
    CHECK_RC(pJs->GetProperty(pGlobObj, "g_PrintPreservedFs", &printPreservedFs));

    bool generateSweepJson = GpuPtr()->GetGenerateSweepJsonArg();

    if (Platform::HasClientSideResman())
    {
        if (preserveFloorsweeping || printPreservedFs)
        {
            for (auto pSubdev : *gpuDevMgr)
            {
                if (preserveFloorsweeping)
                {
                    CHECK_RC(pSubdev->GetFsImpl()->PreserveFloorsweepingSettings());
                }
                CHECK_RC(pSubdev->GetFsImpl()->PrintPreservedFloorsweeping());
            }
        }
        if (generateSweepJson)
        {
            for (auto pSubdev : *gpuDevMgr)
            {
                CHECK_RC(pSubdev->GetFsImpl()->GenerateSweepJson());
            }
        }
    }

    // Print more information after RM knows the GPUs and GPU device manager is initialized.
    if (Platform::HasClientSideResman())
    {
        CHECK_RC(Utility::CallOptionalJsMethod("PostGpuEnumCallback"));
    }

    s_IsInitialized = true;
    s_InitializeFailed = false;
    Printf(Tee::PriDebug, "Completed PC Gpu initialization.\n");

    // Reuse escapewrite to notify RM init is done for fmodel
    // This is a temporary hack before we get a better idea on fermi floorsweeping.
    if ((Platform::GetSimulationMode() == Platform::RTL) ||
        (Platform::GetSimulationMode() == Platform::Fmodel))
    {
        // RM init is done
        Platform::EscapeWrite("leaving_section", 0, 4, RM_INIT_SECTION);
    }

    // Additional checks:
    LwRmPtr pLwRm;

    if (pTestDeviceMgr->GetFirstDevice()->IsEmulation() ||
        Platform::GetSimulationMode() != Platform::Hardware)
    {
        Printf(Tee::PriNormal, "Running on simulation/emulation.\n");
    }

    // We have to hook the power connector event on all GPUs
    for (auto pGpuSubdevice : *gpuDevMgr)
    {
        UINT32 GpuInstance = pGpuSubdevice->GetGpuInst();
        LwRm::Handle hRmSubdev = pLwRm->GetSubdeviceHandle(pGpuSubdevice);
        MASSERT(hRmSubdev);

        LW2080_CTRL_PERF_GET_POWER_CONNECTOR_PARAMS pwrParams = {0};

        rc = pLwRm->Control(hRmSubdev,
                     LW2080_CTRL_CMD_PERF_GET_POWER_CONNECTOR_STATUS,
                     &pwrParams, sizeof(pwrParams));
        if (RC::LWRM_NOT_SUPPORTED == rc)
        {
            rc.Clear();
            continue;
        }
        CHECK_RC(rc);
        if (DRF_VAL(2080, _CTRL_PERF_POWER_CONNECTOR, _STATUS_AUX,
                    pwrParams.connectorsPresent) &&
            (DRF_VAL(2080, _CTRL_PERF_POWER_CONNECTOR, _STATUS_AUX,
                     pwrParams.lwrrStatus)
             == LW2080_CTRL_PERF_POWER_CONNECTOR_STATUS_AUX_DISCONN) &&
            !IgnoreMissingExtPower)
        {
            Printf(Tee::PriError,
                   "External power is not connected on device %d.\n",
                   GpuInstance);
            return RC::AUXILIARY_POWER_UNCONNECTED;
        }
    }

    for (auto pGpuSubdevice : *gpuDevMgr)
    {
        Printf(Tee::PriLow, "Checking pre-init register state\n");
        CHECK_RC(pGpuSubdevice->ValidatePreInitRegisters());

        // Fake any simulated EDIDs now
        pGpuSubdevice->GetParentDevice()->SetupSimulatedEdids();
    }

    if (0 != m_GpuMonitorData.monitorIntervalMs)
    {
        if (Platform::IsPhysFunMode() || Platform::IsDefaultMode())
        {
            s_GpuMonitorEvent = Tasker::AllocEvent("Monitor");
            s_GpuMonitorThreadID = Tasker::CreateThread(GpuMonitorThread,
                &m_GpuMonitorData, 0, "GPU monitor");
        }
        else
        {
            Printf(Tee::PriWarn, "-monitor_ms argument is ignored in VF mods\n");
        }
    }

    if (m_ThreadIntervalMs != 0)
    {
        s_GpuThreadEvent    = Tasker::AllocEvent("JSThread");
        s_GpuThreadThreadID = Tasker::CreateThread(JSMonitorThread,
            &m_ThreadIntervalMs, 0, "JS thread");
    }

    if ((Platform::GetSimulationMode() != Platform::Amodel) &&
        (Platform::IsPhysFunMode() || Platform::IsDefaultMode()))
    {
        // FIXME should we get 'HandleGpuPostInitArgs' to run successfully on amodel?
        string  jsCallbackFunc;
        RC callbackRc = pJs->GetProperty(Gpu_Object, Gpu_PostInitCallback, &jsCallbackFunc);
        JSFunction* pFunction = 0;
        // If the function is not defined, just skip it (and don't exit or crash!)
        if ((callbackRc == RC::OK) &&
            (pJs->GetProperty(pGlobObj, jsCallbackFunc.c_str(), &pFunction) == RC::OK))
        {
            JsArray args;                           // passing in empty args
            jsval retValJs = JSVAL_NULL;
            UINT32 jsRetVal = RC::SOFTWARE_ERROR;
            CHECK_RC(pJs->CallMethod(pGlobObj, jsCallbackFunc, args, &retValJs));
            CHECK_RC(pJs->FromJsval(retValJs, &jsRetVal));
            CHECK_RC(jsRetVal);
        }
    }
    // checking whether there is a correct response from -enable_ecc/-disable_ecc flag
    for (auto pSubdev : *DevMgrMgr::d_GraphDevMgr)
    {
        bool isEccSupported = false;
        UINT32 eccMask = 0;
        bool eccCheck = (pSubdev->GetEccEnabled(&isEccSupported, &eccMask) == RC::OK) &&
                        isEccSupported && Ecc::IsUnitEnabled(Ecc::Unit::DRAM, eccMask);
        if (enableEcc && (!isEccSupported))
        {
            Printf(Tee::PriWarn, "ECC is unsupported on this chip, -enable_ecc failed!\n");
        }
        else if (disableEcc && isEccSupported)
        {
            Printf(Tee::PriWarn, "ECC is still supported on this chip, -disable_ecc failed!\n");
        }
        else if (enableEcc && (!eccCheck))
        {
            Printf(Tee::PriWarn, "-enable_ecc has failed to enable ECC!\n");
        }
        else if (disableEcc && eccCheck)
        {
            Printf(Tee::PriWarn, "-disable_ecc has failed to disable ECC!\n");
        }
    }

    CHECK_RC(VerifyHulkLicense());

#if defined(SIM_BUILD) && defined(LW_WINDOWS) && !defined(LW_64_BITS) && !defined(SERVER_MODE)
    for (auto pSubdev : *gpuDevMgr)
    {
        if (!pSubdev->HasBug(1706402))
        {
            Printf(Tee::PriError,
                   "32-bit winsim mods is not supported on %s."
                   "  Please upgrade to 64-bit.\n",
                   Gpu::DeviceIdToString(pSubdev->DeviceId()));
            return RC::UNSUPPORTED_DEVICE;
        }
    }
#endif
    return OK;
}

//-------------------------------------------------------------------------------------------------
// Read the Security Canary Path Monitor and if the status is non-zero report the faulting sensors
// and return HW_ERROR.
RC Gpu::CheckScpm(GpuSubdevice * pSubdev)
{
    RC rc;
    UINT32 status = 0;
    if (!Platform::IsDefaultMode() || (Platform::GetSimulationMode() == Platform::RTL))
    {
        return RC::OK;
    }

    if (!pSubdev)
    {
        return RC::BAD_PARAMETER;
    }
    auto pGpuPcie = pSubdev->GetInterface<Pcie>();
    rc = pGpuPcie->GetScpmStatus(&status);
    if (rc == RC::OK)
    {
        if (status)
        {
            Printf(Tee::PriError,
                   "SCPM faults:0x%x detected!"
                   " Chip may be in lockdown mode where a cold-reset is required!\n", status);
            Printf(Tee::PriError, "Faulting Sensors:%s\n",
                   pGpuPcie->DecodeScpmStatus(status).c_str());
            return RC::HW_ERROR;
        }
        return (rc);
    }
    if (rc == RC::UNSUPPORTED_FUNCTION)
    {
        rc.Clear();
    }
    return rc;
}

RC Gpu::ValidateIsr(bool *allowAutoMsix, bool *allowAutoMsi, bool *allowAutoInta)
{
    RC rc;
    GpuDevMgr *gpuDevMgr = (GpuDevMgr*)DevMgrMgr::d_GraphDevMgr;

    // Verify that HW interrupts work
    *allowAutoMsix = !m_SkipMsixIntrCheck; // -auto_interrupts can use MSI-X
    *allowAutoMsi  = !m_SkipMsiIntrCheck;  // -auto_interrupts can use MSI
    *allowAutoInta = !m_SkipIntaIntrCheck; // -auto_interrupts can use INTA
    for (auto pSubdev : *gpuDevMgr)
    {
        // 1) Always validate interrupts on SOC GPUs (e.g. GK20A)
        // 2) Automatically determine which mechanism works with -auto_interrupt
        // 3) Skip INTA validation with -skip_inta_intr_check
        // 4) Skip MSI validation with -skip_msi_intr_check
        if (pSubdev->IsSOC() || !m_SkipIntaIntrCheck || !m_SkipMsiIntrCheck|| !m_SkipMsixIntrCheck)
        {
            if (m_AutoInterrupts && !pSubdev->IsSOC())
            {
                if (*allowAutoInta &&
                    Xp::ValidateGpuInterrupt(pSubdev, Xp::IntaIntrCheck) != RC::OK)
                {
                    *allowAutoInta = false;
                }
                if (*allowAutoMsi &&
                    Xp::ValidateGpuInterrupt(pSubdev, Xp::MsiIntrCheck) != RC::OK)
                {
                    *allowAutoMsi = false;
                }
                if (*allowAutoMsix &&
                    Xp::ValidateGpuInterrupt(pSubdev, Xp::MsixIntrCheck) != RC::OK)
                {
                    *allowAutoMsix = false;
                }
            }
            else
            {
                UINT32 which = Xp::NoIntrCheck;
                if (!m_SkipIntaIntrCheck)
                {
                    which |= Xp::IntaIntrCheck;
                }
                if (!m_SkipMsiIntrCheck)
                {
                    which |= Xp::MsiIntrCheck;
                }
                if (!m_SkipMsixIntrCheck)
                {
                    which |= Xp::MsixIntrCheck;
                }
                CHECK_RC(Xp::ValidateGpuInterrupt(pSubdev, which));
            }
        }
    }

    return rc;
}

RC Gpu::HookIsr(bool allowAutoMsix, bool allowAutoMsi, bool allowAutoInta)
{
    RC rc;
    GpuDevMgr *gpuDevMgr = (GpuDevMgr*)DevMgrMgr::d_GraphDevMgr;

    // Hook polling or hardware ISR.
    if (m_AutoInterrupts)
    {
        if (allowAutoMsix)
        {
            m_IrqType = MODS_XP_IRQ_TYPE_MSIX;
        }
        else if (allowAutoMsi)
        {
            m_IrqType = MODS_XP_IRQ_TYPE_MSI;
        }
        else if (allowAutoInta)
        {
            m_IrqType = MODS_XP_IRQ_TYPE_INT;
        }
        else
        {
            m_PollInterrupts = true;
        }
    }
    else
    {
        if (m_MsixInterrupts)
            m_IrqType = MODS_XP_IRQ_TYPE_MSIX;
        else if (m_MsiInterrupts)
            m_IrqType = MODS_XP_IRQ_TYPE_MSI;
        else if (m_IntaInterrupts)
            m_IrqType = MODS_XP_IRQ_TYPE_INT;
    }

    if (Platform::IsVirtFunMode()) // TODO: Stop ignoring error for VirtFunMode
        gpuDevMgr->HookInterrupts(m_PollInterrupts, m_IrqType);
    else
        CHECK_RC(gpuDevMgr->HookInterrupts(m_PollInterrupts, m_IrqType));

    return rc;
}

RC Gpu::ValidateAndHookIsr()
{
    RC rc;
    bool allowAutoMsix = false;
    bool allowAutoMsi = false;
    bool allowAutoInta = false;

    CHECK_RC(ValidateIsr(&allowAutoMsix, &allowAutoMsi, &allowAutoInta));
    CHECK_RC(HookIsr(allowAutoMsix, allowAutoMsi, allowAutoInta));

    return rc;
}

//------------------------------------------------------------------------------
bool Gpu::IsInitialized() const
{
    return s_IsInitialized;
}

bool Gpu::IsRmInitCompleted() const
{
    return s_RmInitCompleted;
}

bool Gpu::IsMgrInit() const
{
    return DevMgrMgr::d_GraphDevMgr && DevMgrMgr::d_GraphDevMgr->HasFoundDevices();
}

bool Gpu::IsInitSkipped() const
{
    return (m_SkipDevInit || m_SkipRmStateInit);
}

//------------------------------------------------------------------------------
bool Gpu::IsPciDeviceInAllowedList(UINT32 domain, UINT32 bus, UINT32 device, UINT32 function) const
{
    // This list is populated with the -only_pci_dev arg. Therefore, if this arg
    // is not used, the list will be empty, and all PCI devices are 'allowed'.
    // In that case, we should return true.
    bool ret = true;

    if (!m_AllowedPciDevices.empty())
    {
        const UINT64 id = (((static_cast<UINT64>(domain   & 0xFFFF)) << 48) |
                           ((static_cast<UINT64>(bus      & 0xFFFF)) << 32) |
                           ((static_cast<UINT64>(device   & 0xFFFF)) << 16) |
                            (static_cast<UINT64>(function & 0xFFFF)));
        if (m_AllowedPciDevices.find(id) == m_AllowedPciDevices.end())
        {
            ret = false;
        }
    }

    return ret;
}

//------------------------------------------------------------------------------
void Gpu::SetHasBugOverride(UINT32 GpuInst, UINT32 BugNum, bool HasBug)
{
    if (GpuInst >= LW0080_MAX_DEVICES)
    {
        Printf(Tee::PriNormal, "DevInst:%d to large - max of %d\n", GpuInst, LW0080_MAX_DEVICES);
        return;
    }

    if (0 == m_HasBugOverride.size())
    {
        m_HasBugOverride.resize(LW0080_MAX_DEVICES);
    }
    m_HasBugOverride[GpuInst][BugNum] = HasBug;
}
//------------------------------------------------------------------------------
bool Gpu::GetHasBugOverride(UINT32 GpuInst, UINT32 BugNum, bool *pHasBug)
{
    // don't have override
    if (0 == m_HasBugOverride.size())
    {
        return false;
    }

    // don't have override for that bug
    if (m_HasBugOverride[GpuInst].find(BugNum) == m_HasBugOverride[GpuInst].end())
    {
        return false;
    }

    MASSERT(pHasBug);
    *pHasBug = m_HasBugOverride[GpuInst][BugNum];
    return true;
}


//------------------------------------------------------------------------------
/* static */ string Gpu::DeviceIdToString(Gpu::LwDeviceId ndi)
{
#if LWCFG(GLOBAL_FAMILY_T23X)
    if (ndi == Gpu::T234)
    {
        if (CheetAh::SocPtr()->GetMajorRev() == 9)
            return "T239";
        else
            return "T234";
    }
#endif
    return DeviceIdToInternalString(ndi);
}

//------------------------------------------------------------------------------
/* static */ string Gpu::DeviceIdToInternalString(Gpu::LwDeviceId ndi)
{
    // Certain internal scripts, like floorsweep.egg, will also be redacted and so
    // are relying on matching the redacted string that's printed to the log.
    JavaScriptPtr pJs;
    bool devidFsWar = false;
    pJs->GetProperty(pJs->GetGlobalObject(), "g_DevidFsWar", &devidFsWar);
    if (devidFsWar)
        return DeviceIdToExternalString(ndi);

    return Utility::RestoreString(DeviceIdToExternalString(ndi));
}

//------------------------------------------------------------------------------
// This version will always return the redacted version of the name, regardless of the
// environment.
//
/* static */ string Gpu::DeviceIdToExternalString(Gpu::LwDeviceId ndi)
{
    if ((Xp::GetOperatingSystem() == Xp::OS_WINDA) ||
        (Xp::GetOperatingSystem() == Xp::OS_LINDA))
    {
        return Utility::ToUpperCase(Xp::GetElw("OGL_ChipName"));
    }

    for (size_t i = 0; i < NUMELEMS(s_IdToNameAndFamily); i++)
    {
        if (ndi == s_IdToNameAndFamily[i].id)
        {
            return s_IdToNameAndFamily[i].name;
        }
    }

    return "";
}

//------------------------------------------------------------------------------
/* static */ UINT32 Gpu::DeviceIdToFamily(Gpu::LwDeviceId ndi)
{
    for (size_t i = 0; i < NUMELEMS(s_IdToNameAndFamily); i++)
    {
        if (ndi == s_IdToNameAndFamily[i].id)
        {
            return static_cast<UINT32>(s_IdToNameAndFamily[i].family);
        }
    }

    return static_cast<UINT32>(GpuDevice::None);
}

//------------------------------------------------------------------------------
const char * Gpu::ClkSrcToString(Gpu::ClkSrc cs)
{
    if ((cs >= 0) && (cs < Gpu::ClkSrc_NUM))
        return s_ClkSrcToString[cs];

    return "";
}

//-----------------------------------------------------------------------------
//! If this flag is set, then clients are encouraged to use the GPU
//! virtual-address mechanism whenever possible.  See
//! Gpu::SupportsMapVirtualDma() and Gpu::SetUseVirtualDma().
//!
//! This flag is normally set via command-line option.  It is expected
//! to become the standard behavior in the future, but is lwrrently
//! disabled by default because the virtual-memory allocation
//! mechanism only allocates on a page boundary.  Forcing all surfaces
//! to be page-aligned means that we aren't testing some interesting
//! corner cases.  Once the virtual-memory mechanism is fixed, this
//! flag should disappear; see bug 239203.
//!
bool Gpu::GetUseVirtualDma() const
{
    return m_UseVirtualDma;
}

//-----------------------------------------------------------------------------
//! Sets the flag indicating whether clients should use the
//! virtual-address mechanism whenever possible.  See
//! Gpu::SupportsMapVirtualDma() and Gpu::GetUseVirtualDma().
//!
RC Gpu::SetUseVirtualDma(bool Value)
{
    RC rc;
    m_UseVirtualDma = Value;
    return rc;
}

UINT08 Gpu::RegRd08(UINT32 Offset)
{
    return DevMgrMgr::d_GraphDevMgr->GetFirstGpu()->RegRd08(Offset);
}

UINT16 Gpu::RegRd16(UINT32 Offset)
{
    return DevMgrMgr::d_GraphDevMgr->GetFirstGpu()->RegRd16(Offset);
}

UINT32 Gpu::RegRd32(UINT32 Offset)
{
    return DevMgrMgr::d_GraphDevMgr->GetFirstGpu()->RegRd32(Offset);
}

void Gpu::RegWr08(UINT32 Offset, UINT08 Data)
{
    DevMgrMgr::d_GraphDevMgr->GetFirstGpu()->RegWr08(Offset, Data);
}

void Gpu::RegWr16(UINT32 Offset, UINT16 Data)
{
    DevMgrMgr::d_GraphDevMgr->GetFirstGpu()->RegWr16(Offset, Data);
}

void Gpu::RegWr32(UINT32 Offset, UINT32 Data)
{
    DevMgrMgr::d_GraphDevMgr->GetFirstGpu()->RegWr32(Offset, Data);
}

//-----------------------------------------------------------------------------
//! \brief Read a Soc register
//!
//! \param DevType : SOC device type to read
//! \param Index   : SOC device index to read (i.e. if there are multiple
//!                  devices of DevType, the index of the device to access)
//! \param Offset  : Offset of the register from the start of the device block
//!                  to read
//! \param pValue  : Pointer to returned register value
//!
//! \return OK if successful, not OK otherwise
//!
RC Gpu::SocRegRd32 (UINT32 DevType, UINT32 Index, UINT32 Offset, UINT32 *pValue)
{
    if (!Platform::IsTegra())
    {
        return RC::SOFTWARE_ERROR;
    }
    MASSERT(pValue);
    RC rc;
    vector<CheetAh::DeviceDesc> desc;
    CHECK_RC(CheetAh::GetDeviceDescByType(DevType, &desc));
    if ((UINT32)Index >= desc.size())
    {
        Printf(Tee::PriError,
               "SocRegRd32 : Unable to find device %u, index %u\n",
               DevType, Index);
        return RC::SOFTWARE_ERROR;
    }
    return SocRegRd32(desc[Index].devIndex, Offset, pValue);
}

//-----------------------------------------------------------------------------
//! \brief Read a Soc register
//!
//! \param DevIndex : Device index from CheetAh::DeviceDesc structure for the
//!                   reading the register
//! \param Offset   : Offset of the register from the start of the device block
//!                   to read
//! \param pValue   : Pointer to returned register value
//!
//! \return OK if successful, not OK otherwise
//!
RC Gpu::SocRegRd32(UINT32 DevIndex, UINT32 Offset, UINT32 *pValue)
{
    if (!Platform::IsTegra())
    {
        return RC::SOFTWARE_ERROR;
    }

    MASSERT(pValue);
    RC    rc;
    void *pLinAperture;
    CHECK_RC(CheetAh::SocPtr()->GetAperture(DevIndex, &pLinAperture));

    *pValue = MEM_RD32((char *)pLinAperture + Offset);
    return rc;
}

//-----------------------------------------------------------------------------
//! \brief Read a Soc register
//!
//! \param PhysAddr : Register physical address
//! \param pValue   : Pointer to returned register value
//!
//! \return OK if successful, not OK otherwise
//!
RC Gpu::SocRegRd32(PHYSADDR PhysAddr, UINT32 *pValue)
{
    if (!Platform::IsTegra())
    {
        return RC::SOFTWARE_ERROR;
    }

    RC rc;
    CheetAh::DeviceDesc devDesc;
    CHECK_RC(CheetAh::SocPtr()->GetDeviceDescByPhysAddr(PhysAddr, &devDesc));
    return SocRegRd32(devDesc.devIndex,
                      static_cast<UINT32>(PhysAddr - devDesc.apertureOffset),
                      pValue);
}

//-----------------------------------------------------------------------------
//! \brief Write a Soc register
//!
//! \param DevType : SOC device type to write
//! \param Index   : SOC device index to write (i.e. if there are multiple
//!                  devices of DevType, the index of the device to access)
//! \param Offset  : Offset of the register from the start of the device block
//!                  to write
//! \param Data    : Value to write into the register
//!
//! \return OK if successful, not OK otherwise
//!
RC Gpu::SocRegWr32(UINT32 DevType, UINT32 Index, UINT32 Offset, UINT32 Data)
{
    if (!Platform::IsTegra())
    {
        return RC::SOFTWARE_ERROR;
    }

    RC rc;
    vector<CheetAh::DeviceDesc> desc;
    CHECK_RC(CheetAh::GetDeviceDescByType(DevType, &desc));
    if ((UINT32)Index >= desc.size())
    {
        Printf(Tee::PriError,
               "SocRegWr32 : Unable to find device %u, index %u\n",
               DevType, Index);
        return RC::SOFTWARE_ERROR;
    }
    return SocRegWr32(desc[Index].devIndex, Offset, Data);
}

//-----------------------------------------------------------------------------
//! \brief Write a Soc register
//!
//! \param DevIndex : Device index from CheetAh::DeviceDesc structure for the
//!                   writing the register
//! \param Offset   : Offset of the register from the start of the device block
//!                   to write
//! \param Data     : Value to write into the register
//!
//! \return OK if successful, not OK otherwise
//!
RC Gpu::SocRegWr32(UINT32 DevIndex, UINT32 Offset, UINT32 Data)
{
    if (!Platform::IsTegra())
    {
        return RC::SOFTWARE_ERROR;
    }

    RC    rc;
    void *pLinAperture;
    CHECK_RC(CheetAh::SocPtr()->GetAperture(DevIndex, &pLinAperture));
    MEM_WR32((char *)pLinAperture + Offset, Data);
    return rc;
}

//-----------------------------------------------------------------------------
//! \brief Write a Soc register
//!
//! \param PhysAddr : Register physical address
//! \param Data     : Value to write into the register
//!
//! \return OK if successful, not OK otherwise
//!
RC Gpu::SocRegWr32(PHYSADDR PhysAddr, UINT32 Data)
{
    if (!Platform::IsTegra())
    {
        return RC::SOFTWARE_ERROR;
    }

    RC rc;
    CheetAh::DeviceDesc devDesc;
    CHECK_RC(CheetAh::SocPtr()->GetDeviceDescByPhysAddr(PhysAddr, &devDesc));
    return SocRegWr32(devDesc.devIndex,
                      static_cast<UINT32>(PhysAddr - devDesc.apertureOffset),
                      Data);
}

bool Gpu::RegWriteExcluded(UINT32 Offset)
{
    // Check to see if this address is one of the ones we want to avoid
    // writing to.
    vector<REGISTER_RANGE>::iterator rangeIter = g_GpuWriteExclusionRegions.begin();

    while (rangeIter != g_GpuWriteExclusionRegions.end())
    {
        if ((Offset >= rangeIter->Start) &&
            (Offset <= rangeIter->End))
        {
            return true;
        }

        rangeIter ++;
    }

    return false;
}
//-----------------------------------------------------------------------------

RegMaskRange::RegMaskRange()
{
    m_GpuInst  = 0;
    m_Startreg = 0;
    m_Endreg   = 0;
    m_Bitmask  = 0xFFFFFFFF;
}
RegMaskRange::RegMaskRange(UINT32 GpuInst, UINT32 addr1, UINT32 addr2, UINT32 msk)
{
    m_GpuInst = GpuInst;
    m_Startreg = addr1;
    m_Endreg = addr2;
    m_Bitmask = msk;
    return;
}

void RegMaskRange::SetValues(UINT32 GpuInst, UINT32 addr1, UINT32 addr2, UINT32 msk)
{
    m_GpuInst = GpuInst;
    m_Startreg = addr1;
    m_Endreg = addr2;
    m_Bitmask = msk;
    return;
}

vector<RegMaskRange> g_RegWrMask;

UINT32 Gpu::GetRegWriteMask(UINT32 GpuInst, UINT32 Offset) const
{
    vector<RegMaskRange>::const_iterator rangeIter = g_RegWrMask.begin();
    //Find the Offset in the array of register mask ranges and return the
    //Mask. If Offset is not present return FFFFFFFF.
    while (rangeIter != g_RegWrMask.end())
    {
        if (rangeIter->IsInRange(GpuInst, Offset))
        {
            return rangeIter->GetMask();
        }
        rangeIter++;
    }
    return 0xFFFFFFFF;
}

void Gpu::StopVpmCapture()
{
    // Stop VPM for all subdevices
    MASSERT(DevMgrMgr::d_GraphDevMgr);

    for (auto pSubdev : *DevMgrMgr::d_GraphDevMgr)
    {
        pSubdev->StopVpmCapture();
    }
}

//! All Interrupt hooks are deprecated.  Please do not add any additional
//! code that uses them.  Please do not add any more functions that install
//! callbacks with the Resource Manager, that approach does not work on
//! real operating systems where the RM lives in the kernel
//------------------------------------------------------------------------------
bool Gpu::GrIntrHook(UINT32 GrIdx, UINT32 GrIntr, UINT32 ClassError,
                     UINT32 Addr, UINT32 DataLo)
{
    if (m_GrIntrHook)
    {
        return m_GrIntrHook(GrIdx, GrIntr, ClassError, Addr, DataLo, m_GrIntrHookData);
    }
    return false;
}

//! All Interrupt hooks are deprecated.  Please do not add any additional
//! code that uses them.  Please do not add any more functions that install
//! callbacks with the Resource Manager, that approach does not work on
//! real operating systems where the RM lives in the kernel
//------------------------------------------------------------------------------
bool Gpu::DispIntrHook(UINT32 Intr0, UINT32 Intr1)
{
    if (m_DispIntrHook)
    {
        return m_DispIntrHook(Intr0, Intr1, m_DispIntrHookData);
    }
    return false;
}

//! All Interrupt hooks are deprecated.  Please do not add any additional
//! code that uses them.  Please do not add any more functions that install
//! callbacks with the Resource Manager, that approach does not work on
//! real operating systems where the RM lives in the kernel
//------------------------------------------------------------------------------
void Gpu::RegisterGrIntrHook(GrIntrHookProc IntrHook, void *CallbackData)
{
    m_GrIntrHook = IntrHook;
    m_GrIntrHookData = CallbackData;
}

//! All Interrupt hooks are deprecated.  Please do not add any additional
//! code that uses them.  Please do not add any more functions that install
//! callbacks with the Resource Manager, that approach does not work on
//! real operating systems where the RM lives in the kernel
//------------------------------------------------------------------------------
void Gpu::RegisterDispIntrHook(DispIntrHookProc IntrHook, void *CallbackData)
{
    m_DispIntrHook = IntrHook;
    m_DispIntrHookData = CallbackData;
}

//------------------------------------------------------------------------------
RC Gpu::HandleRmPolicyManagerEvent(UINT32 GpuInst, UINT32 EventId)
{
#if defined(INCLUDE_MDIAG)
    GpuSubdevice *pSubdev = DevMgrMgr::d_GraphDevMgr->GetSubdeviceByGpuInst(GpuInst);

    if (!PolicyManager::HasInstance() ||
        !PolicyManager::Instance()->IsInitialized())
    {
        return OK;
    }
    return PolicyManager::Instance()->HandleRmEvent(pSubdev, EventId);
#else
    return OK;
#endif
}
//------------------------------------------------------------------------------

//! Called from RM ISR on CLIENT_SIDE_RESMAN platforms when a
//! robust-channel error oclwrs.  Return true if we're using Vista
//! two-stage error recovery, or false to use legacy recovery.
//------------------------------------------------------------------------------
bool Gpu::RcCheckCallback(UINT32 GpuInstance)
{
    GpuDevice *pGpuDevice = DevMgrMgr::d_GraphDevMgr->
                            GetSubdeviceByGpuInst(GpuInstance)->
                            GetParentDevice();

    MASSERT(pGpuDevice);
    return pGpuDevice->GetUseRobustChannelCallback();
}

//! Called from RM ISR on CLIENT_SIDE_RESMAN platforms if
//! RcCheckCallback returned true.  Registers the error with the
//! client, and passes a routine that the client should call after the
//! ISR to finish the recovery process.
//------------------------------------------------------------------------------
bool Gpu::RcCallback
(
    UINT32 GpuInstance,
    LwRm::Handle hClient,
    LwRm::Handle hDevice,
    LwRm::Handle hObject,
    UINT32 errorLevel,
    UINT32 errorType,
    void *pData,
    void *pRecoveryCallback
)
{
    Tsg *pTsg = Tsg::GetTsgFromHandle(hClient, hObject);
    if (pTsg)
    {
        MASSERT(pTsg->GetGpuDevice()->GetUseRobustChannelCallback());
        if (OK == pTsg->RobustChannelCallback(errorLevel, errorType,
                                              pData, pRecoveryCallback))
        {
            return true;
        }
    }
    else
    {
        for (UINT32 ii = 0; ii < LwRmPtr::GetValidClientCount(); ++ii)
        {
            LwRmPtr pLwRm(ii);
            Channel *pCh = pLwRm->GetChannel(hObject);
            if (pLwRm->GetClientHandle() == hClient && pCh != NULL)
            {
                MASSERT(pCh->GetGpuDevice()->GetUseRobustChannelCallback());
                if (OK == pCh->RobustChannelCallback(errorLevel, errorType,
                                                     pData, pRecoveryCallback))
                {
                    return true;
                }
            }
        }
    }
    return false;
}

//! Called from the RM to temporarily stop RL_PUT from being
//! incremented in a runlist.
//!
//! \param GpuInstance The GPU to block.
//! \param Engine The engine to block.
//! \param Stop true to block the runlist, false to unblock it.
//-----------------------------------------------------------------------------
void Gpu::StopEngineScheduling
(
    UINT32 GpuInstance,
    UINT32 EngineId,
    bool Stop
)
{
    GpuDevice *pGpuDevice = DevMgrMgr::d_GraphDevMgr->
                            GetSubdeviceByGpuInst(GpuInstance)->
                            GetParentDevice();
    Runlist *pRunlist = NULL;

    // Get a pointer to the runlist
    //
    if (pGpuDevice->GetRunlist(EngineId, &*LwRmPtr(), false, &pRunlist) != OK)
    {
        MASSERT(!"Could not find runlist");
    }

    // Block or unblock any threads that increment RL_PUT.
    //
    pRunlist->BlockOnFlush(Stop);
}

//! Called from the RM to check whether the runlist is empty.  Used to
//! poll after calling StopEngineScheduling().
//!
//! \param GpuInstance The GPU to block.
//! \param Engine The engine to block.
//! \param return true if the runlist is empty, false if not
//-----------------------------------------------------------------------------
bool Gpu::CheckEngineScheduling
(
    UINT32 GpuInstance,
    UINT32 EngineId
)
{
    GpuDevice *pGpuDevice = DevMgrMgr::d_GraphDevMgr->
                            GetSubdeviceByGpuInst(GpuInstance)->
                            GetParentDevice();
    Runlist *pRunlist = NULL;

    // Get a pointer to the runlist
    //
    if (pGpuDevice->GetRunlist(EngineId, &*LwRmPtr(), false, &pRunlist) != OK)
    {
        MASSERT(!"Could not find runlist");
        return false;
    }

    // Return true if the runlist is idle.
    //
    return !pRunlist->IsAllocated() || pRunlist->IsBlockedAndFlushed();
}

//-----------------------------------------------------------------------------
RC Gpu::ThreadFile(const string& FileName)
{
    RC rc;
    JavaScriptPtr pJavaScript;

    CHECK_RC(pJavaScript->Import(FileName.c_str()));
    return OK;
}

/**
 *------------------------------------------------------------------------------
 * @function(Gpu::ReadCrtc)
 *
 * Read a vga crtc register.
 *------------------------------------------------------------------------------
 */


RC Gpu::SetVPLLLockDelay(UINT32 delay)
{
    LwRmPtr pLwRm;

    LW_CFGEX_RESERVED_PROPERTY Property;

    LwU32 In[1]  = {delay};

    Property.Property = PROPERTY_SET_VPLL_LOCK_DELAY;
    Property.In       = LW_PTR_TO_LwP64(In);
    Property.Out      = LW_PTR_TO_LwP64(NULL);

    GpuSubdevice* pSubdev = DevMgrMgr::d_GraphDevMgr->GetFirstGpu();
    return pLwRm->ConfigSetEx(LW_CFGEX_RESERVED, &Property, sizeof(Property), pSubdev);
}

RC Gpu::GetVPLLLockDelay(UINT32* delay)
{
    LwRmPtr pLwRm;
    LwU32 Out[1] = {0};
    LW_CFGEX_RESERVED_PROPERTY Property;

    Property.Property = PROPERTY_GET_VPLL_LOCK_DELAY;
    Property.In       = LW_PTR_TO_LwP64(NULL);
    Property.Out      = LW_PTR_TO_LwP64(Out);

    GpuSubdevice* pSubdev = DevMgrMgr::d_GraphDevMgr->GetFirstGpu();
    if (OK != pLwRm->ConfigGetEx(LW_CFGEX_RESERVED, &Property,
                                 sizeof(Property), pSubdev))
        return 0xFF;

    *delay = Out[0];

    return OK;
}

//------------------------------------------------------------------------------
RC Gpu::SetRequireChannelEngineId(bool bRequire)
{
    m_bRequireChannelEngineId = bRequire;
    return RC::OK;
}

//------------------------------------------------------------------------------
bool Gpu::GetRequireChannelEngineId() const
{
    return m_bRequireChannelEngineId;
}

// Debugger hook functions: these functions should never be called from any
// other code, but they are useful to call when you are stopped in the debugger
// and want to examine registers/FB.
UINT08 GpuSubRegRd08(UINT32 subdev, UINT32 Address)
{
    GpuSubdevice *pSubdev = DevMgrMgr::d_GraphDevMgr->GetSubdeviceByGpuInst(subdev);
    MASSERT(pSubdev);
    return pSubdev->RegRd08(Address);
}

UINT16 GpuSubRegRd16(UINT32 subdev, UINT32 Address)
{
    GpuSubdevice *pSubdev = DevMgrMgr::d_GraphDevMgr->GetSubdeviceByGpuInst(subdev);
    MASSERT(pSubdev);
    return pSubdev->RegRd16(Address);
}

UINT32 GpuSubRegRd32(UINT32 subdev, UINT32 Address)
{
    GpuSubdevice *pSubdev = DevMgrMgr::d_GraphDevMgr->GetSubdeviceByGpuInst(subdev);
    MASSERT(pSubdev);
    return pSubdev->RegRd32(Address);
}

void GpuSubRegWr08(UINT32 subdev, UINT32 Address, UINT08 Data)
{
    GpuSubdevice *pSubdev = DevMgrMgr::d_GraphDevMgr->GetSubdeviceByGpuInst(subdev);
    MASSERT(pSubdev);
    pSubdev->RegWr08(Address, Data);
}

void GpuSubRegWr16(UINT32 subdev, UINT32 Address, UINT16 Data)
{
    GpuSubdevice *pSubdev = DevMgrMgr::d_GraphDevMgr->GetSubdeviceByGpuInst(subdev);
    MASSERT(pSubdev);
    pSubdev->RegWr16(Address, Data);
}

void GpuSubRegWr32(UINT32 subdev, UINT32 Address, UINT32 Data)
{
    GpuSubdevice *pSubdev = DevMgrMgr::d_GraphDevMgr->GetSubdeviceByGpuInst(subdev);
    MASSERT(pSubdev);
    pSubdev->RegWr32(Address, Data);
}

//------------------------------------------------------------------------------
RC Gpu::SetHulkCert(const vector<UINT08> &hulk)
{
    m_HulkCert.clear();
    m_HulkCert = hulk;

    return RC::OK;
}

RC Gpu::LoadHulkLicence()
{
    PROFILE_ZONE(GENERIC)

    RC rc;
    size_t hulkSize = m_HulkCert.size();

    // No HULK to process
    if (hulkSize == 0)
    {
        return RC::OK;
    }

    TestDeviceMgr* pTestDeviceMgr = static_cast<TestDeviceMgr*>(DevMgrMgr::d_TestDeviceMgr);
    if (m_LoadHulkViaMods)
    {
        size_t size = CEIL_DIV(hulkSize, 4);
        vector<UINT32> hulkCert(size);
        memcpy(hulkCert.data(), m_HulkCert.data(), hulkSize);

        for (UINT32 deviceInstance = 0; deviceInstance < pTestDeviceMgr->NumDevices(); deviceInstance++)
        {
            TestDevicePtr pTestDevice;
            CHECK_RC(pTestDeviceMgr->GetDevice(deviceInstance, &pTestDevice));

            bool bDevidHulkRevoked = false;
            if ((pTestDevice->GetIsDevidHulkRevoked(&bDevidHulkRevoked) == RC::OK) &&
                 bDevidHulkRevoked)
            {
                if (m_SkipHulkIfRevoked)
                {
                    Printf(Tee::PriLow, "Devid HULK revoked, skipping HULK loading\n");
                    continue;
                }
                else
                {
                    Printf(Tee::PriWarn, "Devid HULKs revoked on the device\n");
                }
            }
            rc = pTestDevice->LoadHulkLicence(hulkCert, m_HulkIgnoreErrors);
            if (rc == RC::UNSUPPORTED_FUNCTION)
            {
                Printf(Tee::PriDebug, "Hulk loading not supported on this device\n");
                rc.Clear();
            }
            else if (rc != RC::OK)
            {
                return rc;
            }
        }
    }
    else
    {
        for (UINT32 deviceInstance = 0; deviceInstance < pTestDeviceMgr->NumDevices(); deviceInstance++)
        {
            TestDevicePtr pTestDevice;
            CHECK_RC(pTestDeviceMgr->GetDevice(deviceInstance, &pTestDevice));

            bool bDevidHulkRevoked = false;
            if ((pTestDevice->GetIsDevidHulkRevoked(&bDevidHulkRevoked) == RC::OK) &&
                bDevidHulkRevoked)
            {
                if (m_SkipHulkIfRevoked)
                {
                    // Since we load HULK in RM through regkeys, do all-or-nothing
                    Printf(Tee::PriLow, "Devid HULK revoked, skipping HULK loading\n");
                    return RC::OK;
                }
                else
                {
                    Printf(Tee::PriWarn, "Devid HULKs revoked on the device\n");
                }
            }
        }

        // HULK will be loaded by RM
        CHECK_RC(Registry::Write("ResourceManager", "RMHulkCert", m_HulkCert));
        CHECK_RC(Registry::Write("ResourceManager", "RMHulkCertSize", static_cast<UINT32>(hulkSize)));
        CHECK_RC(Registry::Write("ResourceManager", "RmHulkDisableFeatures", m_HulkDisableFeatures));
        CHECK_RC(Registry::Write("ResourceManager", "RmIgnoreHulkErrors", m_HulkIgnoreErrors));
    }

    return rc;
}

//------------------------------------------------------------------------------
RC Gpu::SetUsePlatformFLR(bool bUsePlatformFLR)
{
    m_bUsePlatformFLR = bUsePlatformFLR;
    return RC::OK;
}

RC Gpu::SetGenerateSweepJsonArg(bool bGenerateSweepJsonArg)
{
    m_bGenerateSweepJsonArg = bGenerateSweepJsonArg;
    return RC::OK;
}

//------------------------------------------------------------------------------
RC Gpu::SetHulkXmlCertArg(string hulkXmlCert)
{
    m_HulkXmlCert = hulkXmlCert;
    return RC::OK;
}

RC Gpu::VerifyHulkLicense()
{
    RC rc;
    string hulkXmlCert = GpuPtr()->GetHulkXmlCertArg();
    if (hulkXmlCert.empty())
    {
        return rc; // No HULK XML Cert to verify
    }
    if (m_HulkCert.empty())
    {
        Printf(Tee::PriError,
                "HULK License verification requires -hulk_cert command line argument\n");
        return RC::BAD_COMMAND_LINE_ARGUMENT;
    }

    for (auto pTestDevice : *DevMgrMgr::d_TestDeviceMgr)
    {
        rc = HulkLicenseVerifier::VerifyHulkLicense(pTestDevice, hulkXmlCert);
    }
    return rc;
}
