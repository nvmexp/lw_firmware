/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2006-2022 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#include "gpu/include/gpusbdev.h"

#include "ampere/ga102/dev_master.h" // LW_PMC_BOOT_0*
#include "ampere/ga102/dev_lw_xve.h" // LW_XVE_SUBSYSTEM_ALIAS*
#include "class/cl0000.h"  // LW01_NULL_OBJECT
#include "class/cl0073.h"
#include "class/cl208f.h" // LW20_SUBDEVICE_DIAG
#include "class/cl9096.h" // GF100_ZBC_CLEAR
#include "class/cl907f.h" // GF100_REMAPPER
#include "class/cl90e7.h" // GF100_SUBDEVICE_INFOROM
#include "class/cla0e1.h" // GK110_SUBDEVICE_FB
#include "class/clb06f.h" // MAXWELL_CHANNEL_GPFIFO_A
#include "class/clc06f.h" // PASCAL_CHANNEL_GPFIFO_A
#include "class/clc36f.h" // VOLTA_CHANNEL_GPFIFO_A
#include "class/clc56f.h" // AMPERE_CHANNEL_GPFIFO_A
#include "class/clc637.h"
#include "core/include/cmdline.h"
#include "core/include/cnslmgr.h"
#include "core/include/console.h"
#include "core/include/evntthrd.h"
#include "core/include/fileholder.h"
#include "core/include/filesink.h"
#include "core/include/gpu.h"
#include "core/include/ism.h"
#include "core/include/jscript.h"
#include "core/include/mgrmgr.h"
#include "core/include/mle_protobuf.h"
#include "core/include/modsdrv.h"
#include "core/include/lwrm.h"
#include "core/include/platform.h"
#include "core/include/rc.h"
#include "core/include/refparse.h"
#include "core/include/registry.h"
#include "core/include/tee.h"
#include "core/include/utility.h"
#include "core/include/xp.h"
#include "ctrl/ctrl0000.h"
#include "ctrl/ctrl0000/ctrl0000gpu.h" // SLI video bridge detection
#include "ctrl/ctrl0073.h"  // for LW0073_CTRL_SYSTEM_GET_NUM_HEADS_PARAMS
#include "ctrl/ctrl2080.h"
#include "ctrl/ctrl2080/ctrl2080fuse.h" // FUB
#include "ctrl/ctrl2080/ctrl2080mc.h" // for PMU queries
#include "ctrl/ctrl208f/ctrl208fgpu.h" // JTAG
#include "ctrl/ctrl90e7.h"
#include "ctrl/ctrla0e1.h"
#include "device/interface/c2c.h"
#include "device/interface/gpio.h"
#include "device/interface/i2c.h"
#include "device/interface/lwlink.h"
#include "device/interface/pcie.h"
#include "device/interface/portpolicyctrl.h"
#include "device/interface/xusbhostctrl.h"
#include "fermi/gf100/dev_bus.h"
#include "fermi/gf100/dev_graphics_nobundle.h"
#include "fermi/gf100/dev_mmu.h"
#include "fermi/gf100/dev_mspdec_pri.h"
#include "fermi/gf100/dev_msppp_pri.h"
#include "fermi/gf100/dev_msvld_pri.h"
#include "gpu/framebuf/gpufb.h"
#include "gpu/framebuf/nofb.h"
#include "gpu/framebuf/vmfb.h"
#include "gpu/fuse/fuse.h"
#include "gpu/fuse/gf100ecid.h"
#include "gpu/include/boarddef.h"
#include "gpu/include/falconecc.h"
#include "gpu/include/pcibusecc.h"
#include "gpu/include/floorsweepimpl.h"
#include "gpu/include/gpudev.h"
#include "gpu/include/gpuism.h"
#include "gpu/include/gpupm.h"
#include "gpu/include/hbmimpl.h"
#include "gpu/include/hshubecc.h"
#include "gpu/include/pcicfg.h"
#include "gpu/include/testdevicemgr.h"
#include "gpu/include/userdalloc.h"
#include "gpu/pcie/pcicfggpu.h"
#include "gpu/perf/avfssub.h"
#include "gpu/perf/clockthrottle.h"
#include "gpu/perf/perfsub.h"
#include "gpu/perf/perfsub_20.h"
#include "gpu/perf/perfsub_30.h"
#include "gpu/perf/perfsub_40.h"
#include "gpu/perf/perfsub_android.h"
#include "gpu/perf/perfsub_socclientrm.h"
#include "gpu/perf/perfutil.h"
#include "gpu/perf/pmusub.h"
#include "gpu/perf/pwrsub.h"
#include "gpu/perf/thermsub.h"
#include "gpu/reghal/smcreghal.h"
#include "gpu/repair/row_remapper/row_remapper.h"
#include "gpu/tempctrl/hwaccess/ipmitempctrl.h"
#include "gpu/utility/bglogger.h"
#include "gpu/utility/ecccount.h"
#include "gpu/utility/edccount.h"
#include "gpu/utility/gpuutils.h"
#include "gpu/utility/modscnsl.h"
#include "gpu/utility/pcie/pexdev.h"
#include "gpu/utility/scopereg.h"
#include "gpu/utility/sec2rtossub.h"
#include "gpu/utility/smlwtil.h"
#include "gpu/utility/subdevfb.h"
#include "gpu/utility/subdevgr.h"
#include "gpu/utility/vbios.h"
#include "gpu/vmiop/vmiopelw.h" // WAR(MODSAMPERE-1222):
#include "gpu/vmiop/vmiopelwmgr.h"
#include "gpu/utility/fsp.h"
#include "js_gpu.h"
#include "js_gpusb.h"
#include "Lwcm.h"
#include "lwcmrsvd.h"
#include "lwos.h"
#include "rm.h"
#include "t12x/t124/project_relocation_table.h" // LW_DEVID_GPU
#include "cheetah/include/tegchip.h"
#include "cheetah/include/tegrasocdevice.h"
#include "cheetah/include/tegrasocdeviceptr.h"
#include "vbiosdcb4x.h"

#ifdef INCLUDE_AZALIA
#include "device/azalia/azactrlmgr.h"
#include "device/azalia/azactrl.h"
#include "fermi/gf100/dev_lw_xve1.h"
#endif

#ifdef TEGRA_MODS
#include "rmapi_tegra.h"
#endif

#include <algorithm>
#include <array>
#include <numeric>
#include <ctype.h>

// extern all the implementer creation functions for the GpuSubdevice functional
// implementers
#define DEFINE_NEW_GPU(DevIdStart, DevIdEnd, ChipId, GpuId, Constant,     \
                       Family, SubdeviceType, FrameBufferType, IsmType,   \
                       FuseType, FloorsweepType, GCxType,                 \
                       HBMType, HwrefDir, DispHwrefDir, ...)              \
    extern GpuIsm *Create##IsmType(GpuSubdevice *);                       \
    extern unique_ptr<Fuse> Create##FuseType(TestDevice*);                \
    extern FloorsweepImpl *Create##FloorsweepType(GpuSubdevice *);        \
    extern GCxImpl *Create##GCxType(GpuSubdevice*);                       \
    extern HBMImpl *Create##HBMType(GpuSubdevice*);
#define DEFINE_DUP_GPU(...) // don't care
#define DEFINE_OBS_GPU(...) // don't care
#include "gpu/include/gpulist.h"
#undef DEFINE_NEW_GPU
#undef DEFINE_DUP_GPU
#undef DEFINE_OBS_GPU

// Used by chiplib team to log devinit events for performace analysis
#define DEVINIT_SECTION 0x80000004

namespace
{
    // Structure describing the creation functions for GPU implementers
    struct GpusubImpls
    {
        Gpu::LwDeviceId gpuId;
        FrameBuffer*    (*CreateFb)(GpuSubdevice*, LwRm*);
        GpuIsm*         (*CreateIsm)(GpuSubdevice*);
        unique_ptr<Fuse>(*CreateFuse)(TestDevice*);
        FloorsweepImpl* (*CreateFloorsweep)(GpuSubdevice*);
        GCxImpl*        (*CreateGCx)(GpuSubdevice*);
        HBMImpl*        (*CreateHBM)(GpuSubdevice*);
    };

    GpusubImpls s_GpusubImpls[] =
    {
#define DEFINE_NEW_GPU(DevIdStart, DevIdEnd, ChipId, GpuId, Constant,   \
                       Family, SubdeviceType, FrameBufferType, IsmType, \
                       FuseType, FloorsweepType, GCxType,               \
                       HBMType, HwrefDir, DispHwrefDir, ...)            \
    { Gpu::GpuId, FrameBufferType, Create##IsmType,                     \
      Create##FuseType, Create##FloorsweepType,                         \
      Create##GCxType, Create##HBMType },
#define DEFINE_DUP_GPU(...) // don't care
#define DEFINE_OBS_GPU(...) // don't care
#include "gpu/include/gpulist.h"
#undef DEFINE_NEW_GPU
#undef DEFINE_DUP_GPU
#undef DEFINE_OBS_GPU
    };

    // Find the implementer function structure for a particular GPU
    GpusubImpls *FindGpusubImpls(GpuSubdevice *pSubdev)
    {
        GpusubImpls *pImpls = nullptr;

        for (UINT32 i = 0; i < NUMELEMS(s_GpusubImpls); ++i)
        {
            if (s_GpusubImpls[i].gpuId == pSubdev->DeviceId())
            {
                pImpls = &s_GpusubImpls[i];
                break;
            }
        }
        return pImpls;
    }

    bool PollKFusesStateDone(void *pParam)
    {
        MASSERT(pParam);
        GpuSubdevice* const pSubdev = (GpuSubdevice*)pParam;
        const UINT64 KfuseIlwalidVal = 0xbadf1002;

        UINT32 State = pSubdev->Regs().Read32(MODS_KFUSE_STATE);
        // HW really needs to update the KFuse manual
        // LW_KFUSE_STATE_DONE = bit 16
        bool retval = (State == KfuseIlwalidVal) ?
            (false) : pSubdev->Regs().TestField(State, MODS_KFUSE_STATE_DONE, 1);
        return retval;
    }

    //-----------------------------------------------------------------------------
    bool PollFbFlush(void * pArgs)
    {
        GpuSubdevice* pSubdev = (GpuSubdevice*)pArgs;
        if (pSubdev->Regs().Read32(MODS_UFLUSH_FB_FLUSH) == 0)
        {
            return true;
        }
        return false;
    }

    struct ChannelsIdlePollArgs
    {
        GpuSubdevice *pSubdev;
        LwRm* pLwRm;
    };

    bool PollAllChannelsIdleWrapper(void * pArgs)
    {
        ChannelsIdlePollArgs *args = static_cast<ChannelsIdlePollArgs *>(pArgs);
        return args->pSubdev->PollAllChannelsIdle(args->pLwRm);
    }

    struct IlwalidateCompbitCachePollArgs
    {
        GpuSubdevice *gpuSubdevice;
        UINT32 ltc;
        UINT32 slice;
    };

    bool PollCompbitCacheSliceIlwalidated(void *pvArgs)
    {
        IlwalidateCompbitCachePollArgs *args =
            static_cast<IlwalidateCompbitCachePollArgs *>(pvArgs);

        const UINT32 addr = args->gpuSubdevice->Regs().LookupAddress(MODS_PLTCG_LTC0_LTS0_CBC_CTRL_1)
                         + (args->ltc * args->gpuSubdevice->Regs().LookupAddress(MODS_LTC_PRI_STRIDE))
                         + (args->slice * args->gpuSubdevice->Regs().LookupAddress(MODS_LTS_PRI_STRIDE));

        UINT32 value = args->gpuSubdevice->RegRd32(addr);
        return args->gpuSubdevice->Regs().TestField(value,
            MODS_PLTCG_LTC0_LTS0_CBC_CTRL_1_ILWALIDATE_INACTIVE);
    }

    bool ErrorLogFilter(const char *errMsg)
    {
        const char * patterns[] =
        {
             "LW_PGRAPH_INTR:0x*"
            ,"LW_PGRAPH_EXCEPTION:0x*"
            ,"LW_PGRAPH_PRI_BE?_CROP_HWW_ESR:0x*"
            ,"LW_PGRAPH_PRI_BE?_CROP_HWW_CRD_DEAD_TAGS\n"
        };
        for (size_t i = 0; i < NUMELEMS(patterns); i++)
        {
            if (Utility::MatchWildCard(errMsg, patterns[i]))
                return false;
        }
        return true; // log this error, it's not one of the filtered items.
    }

    #define MME_WRITE_TIMEOUT 1000
    #define ILWALID_IRQ_NUM 0xFF

#if !defined(MACOSX_MFG)
    RC PciRead16(const Pcie* pPcie, INT32 address, UINT16* pData)
    {
        return Platform::PciRead16(pPcie->DomainNumber(), pPcie->BusNumber(),
                pPcie->DeviceNumber(), pPcie->FunctionNumber(), address, pData);
    }

    RC PciWrite16(const Pcie* pPcie, INT32 address, UINT16 data)
    {
        return Platform::PciWrite16(pPcie->DomainNumber(), pPcie->BusNumber(),
                pPcie->DeviceNumber(), pPcie->FunctionNumber(), address, data);
    }
    RC PciRead32(const Pcie* pPcie, INT32 address, UINT32* pData)
    {
        return Platform::PciRead32(pPcie->DomainNumber(), pPcie->BusNumber(),
                pPcie->DeviceNumber(), pPcie->FunctionNumber(), address, pData);
    }

    RC PciWrite32(const Pcie* pPcie, INT32 address, UINT32 data)
    {
        return Platform::PciWrite32(pPcie->DomainNumber(), pPcie->BusNumber(),
                pPcie->DeviceNumber(), pPcie->FunctionNumber(), address, data);
    }
#endif
}

// Create the storage for manuals
map<Gpu::LwDeviceId, RefManual> GpuSubdevice::m_ManualMap;
namespace
{
    RefManual s_EmptyRefManual;
    constexpr UINT32 ILWALID_LOC_ID = 0xffffffff;
};

//! Create an empty GpuSubdevice.  This doesn't do any initialization.
GpuSubdevice::GpuSubdevice
(
    const char *name,
    Gpu::LwDeviceId deviceId,
    const PciDevInfo *pPciDevInfo
) : TestDevice(Id(pPciDevInfo->PciDomain,
                  pPciDevInfo->PciBus,
                  pPciDevInfo->PciDevice,
                  pPciDevInfo->PciFunction),
               static_cast<Device::LwDeviceId>(deviceId)),
    m_pPciDevInfo(new PciDevInfo(*pPciDevInfo)),
    m_HookedIntr(HOOKED_NONE),
    m_VerboseJtag(false),
    m_SavedBankSwizzle(false),
    m_OrigBankSwizzle(0),
    m_DeviceId(deviceId),
    m_Initialized(false),
    m_Parent(0),
    m_Subdevice(0),
    m_GpuLocId(ILWALID_LOC_ID),
    m_SubdeviceDiag(0),
    m_hZbc(0),
    m_GpuId(0),
    m_Fb(0),
    m_pThermal(0),
    m_pPerf(0),
    m_pPower(0),
    m_pAvfs(nullptr),
    m_pVolt3x(nullptr),
    m_pTempCtrl(),
    m_StandByIntr(HOOKED_NONE),
    m_InStandBy(false),
    m_InGc6(false),
    m_PausePollInterrupts(false),
    m_PerfWasOwned(false),
    m_ValidationMutex(Tasker::CreateMutex("Interrupt validation", Tasker::mtxUnchecked)),
    m_ValidatedTree(~0U),
    m_FbHeapSize(0),
    m_L2CacheOnlyMode(false),
    m_BusInfoLoaded(false),
    m_GpuInfoLoaded(false),
    m_ReadSkuInfoFromBios(false),
    m_ReadBoardInfoFromBoardDB(false),
    m_BoardName("unknown board"),
    m_BoardDefIndex(0),
    m_RestoreRegTableDone(false),
    m_ResetExceptions(false),
    m_pPmu(nullptr),
    m_pSec2Rtos(nullptr),
    m_bCheckPowerGating(true),
    m_CheckPrivProtection(true),
    m_pGetBar1OffsetMutex(Tasker::AllocMutex(
                              "GpuSubdevice::m_pGetBar1OffsetMutex",
                              Tasker::mtxUnchecked)),
    m_pSmcEnabledMutex(Tasker::AllocMutex(
                              "GpuSubdevice::m_pSmcEnabledMutex",
                              Tasker::mtxUnchecked)),
    m_pClockThrottle(0),
    m_ClockThrottleReserved(false),
    m_pEccErrCounter(nullptr),
    m_pEdcErrCounter(nullptr),
    m_EccInfoRomReportingDisabled(false),
    m_pPerfmon(nullptr),
    m_IsEmulation(false),
    m_EmulationRev(0),
    m_IsDFPGA(false),
    m_HasUsableAzalia(true),
    m_EosAssert(true),
    m_IsEarlyInitComplete(false),
    m_IsIOSpaceEnabled(pPciDevInfo->SbiosBoot),
    m_pNotifierMem(nullptr)
{
    SetName(name);
    Reset(); // initialize variables first time

    // On Android, update SOC GPU info
    if (Platform::UsesLwgpuDriver() && !IsFakeTegraGpu())
    {
        vector<CheetAh::DeviceDesc> desc;
        RC rc = CheetAh::GetDeviceDescByType(LW_DEVID_GPU, &desc);
        if (rc != OK || desc.size() != 1)
        {
            Printf(Tee::PriError, "Unsuppoerted GPU\n");
            MASSERT(0);
            return;
        }
        const UINT32 bar0Size = 16*1024*1024;
        MASSERT(bar0Size < desc[0].apertureSize);
        m_pPciDevInfo->SocDeviceIndex = desc[0].devIndex;
        m_pPciDevInfo->PhysLwBase = desc[0].apertureOffset;
        m_pPciDevInfo->PhysFbBase = desc[0].apertureOffset + bar0Size;
        m_pPciDevInfo->LwApertureSize = bar0Size;
        m_pPciDevInfo->FbApertureSize = desc[0].apertureSize - bar0Size;
        m_pPciDevInfo->DmaBaseAddress = static_cast<PHYSADDR>(0);
        rc = CheetAh::SocPtr()->GetAperture(desc[0].devIndex,
                                          &m_pPciDevInfo->LinLwBase);
        if (rc != OK)
        {
            Printf(Tee::PriError, "Unable to obtain access to GPU aperture\n");
            MASSERT(0);
            return;
        }
    }

    GpusubImpls *pImpls = FindGpusubImpls(this);
    MASSERT(pImpls);
    // If no implementers were found for this GPU, then leave ISM and PCIE
    // implementers NULL.  Note, no implementers will cause a failure during
    // AllocClientStuff when a frame buffer impementation is instantiated
    if (pImpls)
    {
        m_pISM.reset(pImpls->CreateIsm(this));
        m_pFsImpl.reset(pImpls->CreateFloorsweep(this));
        m_pGCxImpl.reset(pImpls->CreateGCx(this));
        m_pHBMImpl.reset(pImpls->CreateHBM(this));
    }

    // Floorsweep object is required (even if its a NULL
    // implementation that does nothing)
    MASSERT(m_pFsImpl.get());

    if (Platform::IsPPC())
    {
        ADD_NAMED_PROPERTY(PpcTceBypassMode, UINT32, MODSDRV_PPC_TCE_BYPASS_DEFAULT);
    }

    ADD_NAMED_PROPERTY(MevpEnableMask, UINT32, LW_PBUS_FS_MEVP_DEFAULT);
    ADD_NAMED_PROPERTY(TpcEnableMask, UINT32, 0);
    ADD_NAMED_PROPERTY(SmEnableMask, UINT32, 0);

    ADD_NAMED_PROPERTY(FbMs01Mode, bool, false);
    ADD_NAMED_PROPERTY(GpcEnableMask, UINT32, 0);
    ADD_NAMED_PROPERTY(NoFsRestore, bool, false);
    ADD_NAMED_PROPERTY(FermiFsArgs, string, "");
    ADD_NAMED_PROPERTY(FsReset, bool, false);
    ADD_NAMED_PROPERTY(AdjustFsArgs, bool, false);
    ADD_NAMED_PROPERTY(AddLwrFsMask,  bool, false);
    ADD_NAMED_PROPERTY(AdjustFsL2Noc, bool, false);
    ADD_NAMED_PROPERTY(FixFsInitMasks, bool, false);
    ADD_NAMED_PROPERTY(FbGddr5x16Mode, bool, false);
    ADD_NAMED_PROPERTY(DumpMicronGDDR6Info, bool, false);
    ADD_NAMED_PROPERTY(FbBankSwizzle, UINT32, 0);
    ADD_NAMED_PROPERTY(FermiGpcTileMap, string, "");
    ADD_NAMED_PROPERTY(PreGpuInitScript, string, "");
    ADD_NAMED_PROPERTY(IsFbBroken, bool, false);
    ADD_NAMED_PROPERTY(UseFakeBar1, bool, false);
    ADD_NAMED_PROPERTY(SmcPartitions, string, "");
    ADD_NAMED_PROPERTY(SmcPartIdx, UINT32, ~0u);
    ADD_NAMED_PROPERTY(SmcEngineIdx, UINT32, ~0u);
    ADD_NAMED_PROPERTY(ActiveSwizzId, UINT32, ~0u);
    ADD_NAMED_PROPERTY(ActiveSmcEngine, UINT32, ~0u);

    bool FilterBug599380Exceptions = false;
    JavaScriptPtr()->GetProperty(Gpu_Object,
            Gpu_FilterBug599380Exceptions, &FilterBug599380Exceptions);
    if (FilterBug599380Exceptions)
    {
       ErrorLogger::InstallErrorLogFilter(ErrorLogFilter);
    }

    // Used to clear pending interrupts
    IntRegClear();
    IntRegAdd(MODS_PMC_INTR_PFIFO,     0, MODS_PFIFO_INTR_EN_x,     0, 0x0);
    IntRegAdd(MODS_PMC_INTR_PFIFO,     0, MODS_PFIFO_INTR_EN_x,     1, 0x0);
    IntRegAdd(MODS_PMC_INTR_PGRAPH,    0, MODS_PGRAPH_INTR_EN,      0, 0x0);
    IntRegAdd(MODS_PMC_INTR_THERMAL,   0, MODS_THERM_INTR_EN_0,     0, 0x0);
    IntRegAdd(MODS_PMC_INTR_PTIMER,    0, MODS_PTIMER_INTR_EN_0,    0, 0x0);
    IntRegAdd(MODS_PMC_INTR_PBUS,      0, MODS_PBUS_INTR_EN_x,      0, 0x0);
    IntRegAdd(MODS_PMC_INTR_PBUS,      0, MODS_PBUS_INTR_EN_x,      1, 0x0);
    IntRegAdd(MODS_PMC_INTR_PMU,       0, MODS_PPWR_FALCON_IRQSCLR, 0, ~0x0);

    // Fermi & Kepler versions of the above registers
    IntRegAdd(MODS_PMC_INTR_x_PFIFO,   0, MODS_PFIFO_INTR_EN_x,     0, 0x0);
    IntRegAdd(MODS_PMC_INTR_x_PFIFO,   0, MODS_PFIFO_INTR_EN_x,     1, 0x0);
    IntRegAdd(MODS_PMC_INTR_x_PGRAPH,  0, MODS_PGRAPH_INTR_EN,      0, 0x0);
    IntRegAdd(MODS_PMC_INTR_x_THERMAL, 0, MODS_THERM_INTR_EN_0,     0, 0x0);
    IntRegAdd(MODS_PMC_INTR_x_PTIMER,  0, MODS_PTIMER_INTR_EN_0,    0, 0x0);
    IntRegAdd(MODS_PMC_INTR_x_PBUS,    0, MODS_PBUS_INTR_EN_x,      0, 0x0);
    IntRegAdd(MODS_PMC_INTR_x_PBUS,    0, MODS_PBUS_INTR_EN_x,      1, 0x0);
    IntRegAdd(MODS_PMC_INTR_x_PMU,     0, MODS_PPWR_FALCON_IRQSCLR, 0, ~0x0);

    if (Regs().IsSupported(MODS_PMC_INTR_PRIV_RING) &&
        Regs().IsSupported(MODS_PPRIV_MASTER_RING_COMMAND_CMD_ACK_INTERRUPT))
    {
        IntRegAdd(MODS_PMC_INTR, 0, Regs().LookupMask(MODS_PMC_INTR_PRIV_RING),
                  MODS_PPRIV_MASTER_RING_COMMAND, 0,
                  Regs().LookupMask(MODS_PPRIV_MASTER_RING_COMMAND_CMD),
                  Regs().SetField(
                          MODS_PPRIV_MASTER_RING_COMMAND_CMD_ACK_INTERRUPT));
    }
    else if (Regs().IsSupported(MODS_PMC_INTR_x_PRIV_RING) &&
             Regs().IsSupported(MODS_PPRIV_MASTER_RING_COMMAND_CMD_ACK_INTERRUPT))
    {
        IntRegAdd(MODS_PMC_INTR_x, 0,
                  Regs().LookupMask(MODS_PMC_INTR_x_PRIV_RING),
                  MODS_PPRIV_MASTER_RING_COMMAND, 0,
                  Regs().LookupMask(MODS_PPRIV_MASTER_RING_COMMAND_CMD),
                  Regs().SetField(
                          MODS_PPRIV_MASTER_RING_COMMAND_CMD_ACK_INTERRUPT));
    }
}

// Protected contructor exclusively for unit testing
GpuSubdevice::GpuSubdevice
(
    Gpu::LwDeviceId deviceId,
    const PciDevInfo *pPciDevInfo
) : TestDevice(Id(pPciDevInfo->PciDomain,
                  pPciDevInfo->PciBus,
                  pPciDevInfo->PciDevice,
                  pPciDevInfo->PciFunction),
                  static_cast<Device::LwDeviceId>(deviceId))
  , m_DeviceId(deviceId)
  , m_Initialized(false)
  , m_pGetBar1OffsetMutex(nullptr)
  , m_pSmcEnabledMutex(nullptr)
  , m_pPerfmon(nullptr)
{
    Reset();
}

//! The destructor should call shutdown in the case it hasn't been done
//! explicitly.
GpuSubdevice::~GpuSubdevice()
{
    if (Platform::HasClientSideResman())
    {
        // Until Early Initialization is complete, clean up responsibility is
        // on the procedure which attempted to create the sub device
        if (m_IsEarlyInitComplete && m_pPciDevInfo)
        {
            // Unmap BAR0
            if (m_pPciDevInfo->InitDone & MODSRM_INIT_BAR0)
            {
                Platform::UnMapDeviceMemory(m_pPciDevInfo->LinLwBase);
            }

            // Restore original BAR1 mapping
            if (m_pPciDevInfo->InitDone & MODSRM_INIT_BAR1WC)
            {
                Platform::UnSetMemRange(m_pPciDevInfo->PhysFbBase,
                                        m_pPciDevInfo->FbApertureSize);
            }
        }
    }

    MASSERT(!m_Initialized);

    if (m_pGetBar1OffsetMutex)
    {
       Tasker::FreeMutex(m_pGetBar1OffsetMutex);
    }

    if (m_pSmcEnabledMutex)
    {
        Tasker::FreeMutex(m_pSmcEnabledMutex);
    }

    // If PMU was created, shut it down
    if (m_pPerfmon)
    {
        m_pPerfmon->ShutDown();
        delete m_pPerfmon;
        m_pPerfmon = nullptr;
    }
}

void GpuSubdevice::ReassignPciDev(const PciDevInfo& pciDevInfo)
{
    m_pPciDevInfo.reset(new PciDevInfo(pciDevInfo));
}

bool GpuSubdevice::IsSamePciAddress
(
    UINT32 domain,
    UINT32 bus,
    UINT32 device,
    UINT32 function
) const
{
    return (m_pPciDevInfo->PciDomain == domain &&
            m_pPciDevInfo->PciBus == bus &&
            m_pPciDevInfo->PciDevice == device &&
            m_pPciDevInfo->PciFunction == function);
}

namespace
{
    string GetGpuNameFromJs(Pcie* pPcie)
    {
        if (!pPcie)
        {
            Printf(Tee::PriDebug, "No PCIE object for GPU subdevice\n");
            return "";
        }

        JavaScriptPtr pJs;
        JsArray names;
        if (OK != pJs->GetProperty(pJs->GetGlobalObject(), "g_GpuNames", &names))
        {
            Printf(Tee::PriDebug, "Failed to retrieve g_GpuNames from JS\n");
            return "";
        }

        for (const auto& nameArray : names)
        {
            JsArray elems;
            if (OK != pJs->FromJsval(nameArray, &elems))
            {
                Printf(Tee::PriDebug, "Failed to retrieve GPU name from g_GpuNames\n");
                return "";
            }

            if (elems.size() != 5)
            {
                Printf(Tee::PriDebug, "Invalid contents of g_GpuNames\n");
                return "";
            }

            UINT32 pos[4] = { };
            string name;
            for (UINT32 i = 0; i < 4; i++)
            {
                if (OK != pJs->FromJsval(elems[i], &pos[i]))
                {
                    Printf(Tee::PriDebug, "Invalid contents of g_GpuNames\n");
                    return "";
                }
            }
            if (OK != pJs->FromJsval(elems[4], &name))
            {
                Printf(Tee::PriDebug, "Invalid contents of g_GpuNames\n");
                return "";
            }

            if (pPcie->DomainNumber()   == pos[0] &&
                pPcie->BusNumber()      == pos[1] &&
                pPcie->DeviceNumber()   == pos[2] &&
                pPcie->FunctionNumber() == pos[3])
            {
                Printf(Tee::PriLow, "Setting GPU name for %x:%02x.%02x.%x = %s\n",
                       pos[0], pos[1], pos[2], pos[3], name.c_str());
                return name;
            }
        }
        Printf(Tee::PriDebug, "GPU name not set\n");
        return "";
    }
}

//-----------------------------------------------------------------------------
string GpuSubdevice::GetClassName() const
{
    return Device::GetName();
}

//------------------------------------------------------------------------------
/* virtual */ RC GpuSubdevice::GetChipPrivateRevision(UINT32* pPrivRev)
{
    MASSERT(pPrivRev);
    RC rc;
    *pPrivRev = 0;

    if (Utility::GetSelwrityUnlockLevel() < Utility::SUL_LWIDIA_NETWORK)
        return OK;

    if (GpuPtr()->IsRmInitCompleted() && !IsSOC() && Platform::HasClientSideResman())
    {
        UINT32 revision = 0;
        LW2080_CTRL_CMD_GPU_GET_ENGR_REV_INFO_PARAMS params = {0};
        LwRmPtr pLwRm;
        if (OK != (rc = pLwRm->ControlBySubdevice(
                                this, LW2080_CTRL_CMD_GPU_GET_ENGR_REV_INFO,
                                &params, sizeof(params))))
        {
            Printf(Tee::PriError,
                   "Cannot get private revision info. rc = %d\n",
                   rc.Get());
            return rc;
        }

        revision = params.majorRev;
        revision = (revision << 8)|((UINT08)params.minorRev);
        revision = (revision << 8)|((UINT08)params.minorExtRev);

        *pPrivRev = revision;
    }
    else
    {
        // Read out the dev_master registers out directly
        *pPrivRev = PrivateRevisionWithoutRm();
    }

    return OK;
}

//------------------------------------------------------------------------------
/* virtual */ RC GpuSubdevice::GetChipRevision(UINT32* pRev)
{
    MASSERT(pRev);
    RC rc;
    *pRev = 0;

    if (GpuPtr()->IsRmInitCompleted())
    {
        LW2080_CTRL_MC_GET_ARCH_INFO_PARAMS params = {0};
        if (OK != (rc = LwRmPtr()->ControlBySubdevice(
                        this, LW2080_CTRL_CMD_MC_GET_ARCH_INFO,
                        &params, sizeof(params))))
        {
            Printf(Tee::PriError,
                   "RmControl failed getting revision information.\n");
            return rc;
        }

        *pRev = params.revision;
    }
    else
    {
        // Read out the dev_master registers out directly
        *pRev = RevisionWithoutRm();
    }

    return OK;
}

//------------------------------------------------------------------------------
/* virtual */ RC GpuSubdevice::GetChipSubRevision(UINT32* pRev)
{
    MASSERT(pRev);
    RC rc;
    *pRev = 0;

    // 0 = no subrevision, 1 = P, 2 = Q, ...
    CHECK_RC(Regs().Read32Priv(MODS_FUSE_OPT_SUBREVISION, pRev));
    return RC::OK;
}

//------------------------------------------------------------------------------
/* virtual */ RC GpuSubdevice::GetChipSkuModifier(string *pStr)
{
    MASSERT(pStr);
    RC rc;

    *pStr = "";
    CHECK_RC(GetSkuInfoFromBios());
    *pStr = m_ChipSkuModifier;

    return OK;
}

//------------------------------------------------------------------------------
/* virtual */ RC GpuSubdevice::GetChipSkuNumber(string *pStr)
{
    MASSERT(pStr);
    RC rc;
    *pStr = "";

    if (IsSOC())
    {
        UINT32 chipSkuId;
        CHECK_RC(CheetAh::SocPtr()->GetGpuChipInfo(nullptr, nullptr,
                                                 nullptr, &chipSkuId));
        *pStr = Utility::StrPrintf("%u", chipSkuId);
    }
    else
    {
        CHECK_RC(GetSkuInfoFromBios());
        *pStr = m_ChipSkuId;
    }

    return OK;
}

//------------------------------------------------------------------------------
/* virtual */ RC GpuSubdevice::GetChipTemps(vector<FLOAT32> *pTempsC)
{
    MASSERT(pTempsC);
    RC rc;
    Thermal *pTherm = GetThermal();
    bool bPrimaryPresent = false;
    CHECK_RC(pTherm->GetPrimaryPresent(&bPrimaryPresent));
    if (bPrimaryPresent)
    {
        FLOAT32 temp;
        CHECK_RC(pTherm->GetChipTempViaPrimary(&temp));
        pTempsC->push_back(temp);
        return rc;
    }
    return RC::UNSUPPORTED_FUNCTION;
}

//-----------------------------------------------------------------------------
RC GpuSubdevice::GetDeviceErrorList(vector<DeviceError>* pErrorList)
{
    MASSERT(pErrorList);
    pErrorList->clear();
    return OK;
}

//-----------------------------------------------------------------------------
RC GpuSubdevice::GetEcidString(string* pStr)
{
    MASSERT(pStr);
    // If unable to get the ECID just use the name as the ECID (this happens on sim platforms)
    if (OK != GetEcid(pStr))
        *pStr = GetName();
    return OK;
}

//------------------------------------------------------------------------------
/* virtual */ RC GpuSubdevice::GetFoundry(ChipFoundry *pFoundry)
{
    MASSERT(pFoundry);
    *pFoundry = CF_UNKNOWN;

    switch (Foundry())
    {
        case LW2080_CTRL_GPU_INFO_FOUNDRY_TSMC: *pFoundry = CF_TSMC; break;
        case LW2080_CTRL_GPU_INFO_FOUNDRY_UMC: *pFoundry = CF_UMC; break;
        case LW2080_CTRL_GPU_INFO_FOUNDRY_IBM: *pFoundry = CF_IBM; break;
        case LW2080_CTRL_GPU_INFO_FOUNDRY_SMIC: *pFoundry = CF_SMIC; break;
        case LW2080_CTRL_GPU_INFO_FOUNDRY_CHARTERED: *pFoundry = CF_CHARTERED; break;
        case LW2080_CTRL_GPU_INFO_FOUNDRY_TOSHIBA: *pFoundry = CF_TOSHIBA; break;
        case LW2080_CTRL_GPU_INFO_FOUNDRY_SONY: *pFoundry = CF_SONY; break;
        case LW2080_CTRL_GPU_INFO_FOUNDRY_SAMSUNG: *pFoundry = CF_SAMSUNG; break;
        default:  *pFoundry = CF_UNKNOWN; break;
    }
    return OK;
}

//------------------------------------------------------------------------------
/* virtual */ string GpuSubdevice::GetName() const
{
    if (IsSOC())
    {
        return Gpu::DeviceIdToString(DeviceId());
    }

    string topologyStr;
    auto pLwLink = GetInterface<LwLink>();
    if (pLwLink && pLwLink->GetTopologyId() != ~0U)
        topologyStr = Utility::StrPrintf("(%u) ", pLwLink->GetTopologyId());

    const UINT32 gpuId = (m_GpuLocId != ILWALID_LOC_ID) ? m_GpuLocId : GetGpuInst();

    string domain;
    auto pPcie = GetInterface<Pcie>();
    if (pPcie->DomainNumber())
        domain = Utility::StrPrintf("%04x:", pPcie->DomainNumber());

    return Utility::StrPrintf("GPU %u %s[%s%02x:%02x.%x]",
                              gpuId,
                              topologyStr.c_str(),
                              domain.c_str(),
                              pPcie->BusNumber(),
                              pPcie->DeviceNumber(),
                              pPcie->FunctionNumber());
}

//-----------------------------------------------------------------------------
RC GpuSubdevice::GetTestPhase(UINT32* pPhase)
{
    MASSERT(pPhase);
    RC rc;
    *pPhase = 0;

    // bug 875326: exit code format:
    // S VVV PP TTT EEE
    // S   = static / perf jump/ perf sweep
    // VVV = VF Point (PS2.0) or locationStr and intersect info (PS3.0)
    // PP  = PState
    // TTT = Test Num
    // EEE = error number

    Perf* pPerf = GetPerf();
    if (!pPerf)
        return OK;

    UINT32 numPstates = 0;
    CHECK_RC(pPerf->GetNumPStates(&numPstates));
    if (numPstates == 0)
        return OK;

    UINT32 errCodeDigits = 0;
    CHECK_RC(GetPerf()->GetPStateErrorCodeDigits(&errCodeDigits));
    UINT32 lwrrPstate = 0;
    CHECK_RC(GetPerf()->GetLwrrentPState(&lwrrPstate));
    *pPhase = errCodeDigits * 100 + lwrrPstate;

    return OK;
}

//-----------------------------------------------------------------------------
void GpuSubdevice::Print(Tee::Priority pri, UINT32 code, string prefix) const
{
    Printf(pri, code, "%s%s\n", prefix.c_str(), GetName().c_str());
}

//-----------------------------------------------------------------------------
TestDevicePtr GpuSubdevice::GetTestDevice()
{
    TestDeviceMgr* pTestDeviceMgr = static_cast<TestDeviceMgr*>(DevMgrMgr::d_TestDeviceMgr);
    MASSERT(pTestDeviceMgr);
    TestDevicePtr pTestDevice;
    pTestDeviceMgr->GetDevice(DevInst(), &pTestDevice);
    return pTestDevice;
}

//-----------------------------------------------------------------------------
RC GpuSubdevice::GetFlaCapable(bool* bCapable) const
{
    MASSERT(bCapable);
    RC rc;

    *bCapable = false;

    LW2080_CTRL_GPU_GET_INFO_V2_PARAMS params = {};
    LW2080_CTRL_GPU_INFO& info = params.gpuInfoList[0];

    params.gpuInfoListSize = 1;
    info.index = LW2080_CTRL_GPU_INFO_INDEX_GPU_FLA_CAPABILITY;
    CHECK_RC(LwRmPtr()->ControlBySubdevice(this,
                                           LW2080_CTRL_CMD_GPU_GET_INFO_V2,
                                           &params, sizeof(params)));

    *bCapable = static_cast<bool>(info.data);

    return rc;
}

//--------------------------------------------------------------------
// This routine gets called very early during the initialization
// process. It is after the constructor but way before RM has been
// initialized. The only guarentee here is that the BARs have been
// properly setup and construction has completed. So you can use
// polymorphism to perform some of the device specific initialization,
// but you can't use RM control calls.
RC GpuSubdevice::EarlyInitialization()
{
    RC rc;

    CHECK_RC(TestDevice::Initialize());

    // PCIE Implementation is required
    MASSERT(SupportsInterface<Pcie>());
    if (SupportsInterface<Pcie>())
    {
        CHECK_RC(GetInterface<Pcie>()->Initialize());

        // NOTE: Ilwoke callback as soon as Pcie Init is done so that m_GpuLocId gets populated
        // before GetName is used

        // Provide a JS callback to let users do additional tweaks,
        // passing the GpuSubdevice as an argument.
        JsArray Params;
        JavaScriptPtr pJs;
        jsval arg1;
        jsval retValJs = JSVAL_NULL;
        UINT32 callbackRc = RC::SOFTWARE_ERROR;

        JsGpuSubdevice *pJsGpuSubdevice = new JsGpuSubdevice();
        pJsGpuSubdevice->SetGpuSubdevice(this);

        JSContext *pContext;
        CHECK_RC(pJs->GetContext(&pContext));
        CHECK_RC(pJsGpuSubdevice->CreateJSObject(pContext,
                                                 pJs->GetGlobalObject()));
        CHECK_RC(pJs->ToJsval(pJsGpuSubdevice->GetJSObject(), &arg1));
        Params.push_back(arg1);

        CHECK_RC(pJs->CallMethod(pJs->GetGlobalObject(),
                 "EarlyInitCallback", Params, &retValJs));

        CHECK_RC(pJs->FromJsval(retValJs, &callbackRc));

        if (RC::UNDEFINED_JS_METHOD == callbackRc)
            callbackRc = RC::OK;
        CHECK_RC(callbackRc);
    }

    // Initialize XUSB Host Conroller
    if (SupportsInterface<XusbHostCtrl>())
    {
        CHECK_RC(GetInterface<XusbHostCtrl>()->Initialize());
    }
    // Initialize Port Policy Controller
    if (SupportsInterface<PortPolicyCtrl>())
    {
        CHECK_RC(GetInterface<PortPolicyCtrl>()->Initialize());
    }

    if (SupportsInterface<I2c>())
    {
        CHECK_RC(GetInterface<I2c>()->Initialize());
    }

    CreatePteKindTable();
    SetupBugs();
    SetupFeatures();
    m_EmulationRev = GetEmulationRev();

    m_GpuName = GetGpuNameFromJs(GetInterface<Pcie>());

    // Fix for 1769647 opened up a latent issue in our code which caused
    // PVS MODS InstInSys to fail (GetCachedFuseReg fails to read within
    // timeout duration). Issue was not caught before because error code
    // returned from EarlyInitialization was not being processed. This issue
    // is being fixed as part of 1783723. In the interim, to avoid PVS/DVS
    // failure, added the following condition.
    // TODO remove following statement as part of fix for 1783723
    m_IsEarlyInitComplete = true;

#if defined(INCLUDE_DGPU)
    if (HasBug(1722075) && !IsEmOrSim() && Platform::HasClientSideResman())
    {
        if (!m_pFuse)
        {
            const GpusubImpls* pImpls = FindGpusubImpls(this);
            if (pImpls == nullptr)
            {
                return RC::SOFTWARE_ERROR;
            }
            m_pFuse = pImpls->CreateFuse(this);
        }

        JavaScriptPtr pJs;
        bool skipPreCache;
        CHECK_RC(pJs->GetProperty(pJs->GetGlobalObject(),
            "g_SkipFusePreCache", &skipPreCache));

        if (!skipPreCache)
        {
            vector<UINT32> temp;
            CHECK_RC(m_pFuse->GetCachedFuseReg(&temp));
        }
    }
#endif

    // Add an entry to the ManualMap here since GetRefManual() is a contant
    // function and it cannot be added there.  If the file does not exist, do
    // not create or add it.  A dummy unparsed ref manual will be returned
    if (m_ManualMap.find(DeviceId()) == m_ManualMap.end())
    {
        // Need to check for both normal and .gz file names
        const string filename = GetRefManualFile();
        if (filename == "" ||
            (!Xp::DoesFileExist(Utility::DefaultFindFile(filename, true)) &&
             !Xp::DoesFileExist(Utility::DefaultFindFile(filename + ".gz", true))))
        {
            const bool isSim = Xp::GetOperatingSystem() == Xp::OS_LINUXSIM ||
                               Xp::GetOperatingSystem() == Xp::OS_WINSIM;
            const INT32 pri = isSim ? Tee::PriWarn : Tee::PriLow;
            Printf(pri, "Register manual file %s not found\n", filename.c_str());
            rc = RC::FILE_DOES_NOT_EXIST;
        }
        else
        {
            m_ManualMap[DeviceId()] = RefManual();
        }
    }

    m_IsEarlyInitComplete = true;
    return rc;
}

RC GpuSubdevice::GetSocBiosImage
(
    const string& filename,
    vector<UINT08>* pBiosImage
)
{
    RC rc;
    CHECK_RC(GpuSubdevice::LoadImageFromFile(filename, pBiosImage));
    if ((*pBiosImage)[0] != 0x55 || (*pBiosImage)[1] != 0xAA)
    {
        Printf(Tee::PriWarn, "BIOS image contains incorrect signature\n");
    }
    return rc;
}

RC GpuSubdevice::LoadImageFromFile
(
    const string& filename,
    vector<UINT08>* pImage
)
{
    FileHolder file;
    RC rc = file.Open(filename.c_str(), "rb");
    if (rc != RC::OK)
    {
        Printf(Tee::PriError, "Could not open image file '%s'\n",
                filename.c_str());
        return rc;
    }

    const UINT32 READ_SIZE = 0x4000;
    do
    {
        pImage->resize(pImage->size() + READ_SIZE, 0);
    }
    while (READ_SIZE == fread(&(*pImage)[pImage->size() - READ_SIZE],
                              1, READ_SIZE, file.GetFile()));

    return RC::OK;
}

RC GpuSubdevice::ValidateBiosImage
(
    const string& filename,
    vector<UINT08>* pBiosImage
) const
{
    RC rc;
    CHECK_RC(GpuSubdevice::LoadImageFromFile(filename, pBiosImage));
    if (((*pBiosImage)[0] != 0x55 || (*pBiosImage)[1] != 0xAA) &&
        ((*pBiosImage)[0] != 'N' || (*pBiosImage)[1] != 'V' || (*pBiosImage)[2] != 'G' || (*pBiosImage)[3] != 'I'))
    {
        Printf(Tee::PriWarn, "BIOS image contains incorrect signature\n");
    }
    return rc;
}

RC GpuSubdevice::GetVBiosImage(vector<UINT08>* pBiosImage) const
{
    RC rc;
    map<UINT32, string> GpuBiosFileNames = CommandLine::GpuBiosFileNames();
    if (GpuBiosFileNames.count(GetGpuInst()))
    {
        const string filename = GpuBiosFileNames[GetGpuInst()];
        CHECK_RC(ValidateBiosImage(filename, pBiosImage));
    }

    // Apply ReverseLVDSTMDS setting to VBIOS image

    bool reverseLVDSTMDS = false;
    JavaScriptPtr pJs;
    CHECK_RC(pJs->GetProperty(Gpu_Object, Gpu_ReverseLVDSTMDS,
                              &reverseLVDSTMDS));
    if (reverseLVDSTMDS)
    {
        if (!pBiosImage->empty())
        {
            Printf(Tee::PriAlways,
                   "Error: DCB overrides require one VBIOS file per"
                   " gpu (no \"-gpubios\" argument for device %d).\n",
                   GetGpuInst());
            return RC::BAD_COMMAND_LINE_ARGUMENT;
        }
        CHECK_RC(VBIOSReverseLVDSTMDS(&(*pBiosImage)[0]));
    }

    return OK;
}
RC GpuSubdevice::GetNetlistImage(vector<UINT08>* pNetlistImage) const
{
    RC rc;
    const string filename = CommandLine::GpuNetlistName();
    if (filename.size())
    {
        FileHolder file;
        rc = file.Open(filename.c_str(), "rb");
        if (rc != RC::OK)
        {
            Printf(Tee::PriError, "Could not open Netlist file '%s'\n",
                   filename.c_str());
            return rc;
        }

        const UINT32 READ_SIZE = 0x4000;
        do
        {
            pNetlistImage->resize(pNetlistImage->size() + READ_SIZE, 0);
        }
        while (fread(&(*pNetlistImage)[pNetlistImage->size() - READ_SIZE],
                     1, READ_SIZE, file.GetFile()) == READ_SIZE);
    }
    return OK;
}

RC GpuSubdevice::GetVBiosImageHash(vector<UINT08>* pBiosImageHash) const
{
    RC rc;

    if (IsSOC() || IsEmOrSim() ||
        (!HasFeature(GPUSUB_SUPPORTS_VBIOS_HASH) && !HasFeature(GPUSUB_SUPPORTS_VBIOS_GUID)))
    {
        return RC::LWRM_NOT_SUPPORTED;
    }

    MASSERT(pBiosImageHash);

    if (HasFeature(GPUSUB_SUPPORTS_VBIOS_GUID))
    {

        LW2080_CTRL_BIOS_GET_BUILD_GUID_PARAMS vbiosBuildGUIDParams;
        memset(&vbiosBuildGUIDParams, 0, sizeof(vbiosBuildGUIDParams));

        CHECK_RC(LwRmPtr()->ControlBySubdevice(this,
                                               LW2080_CTRL_CMD_BIOS_GET_BUILD_GUID,
                                               &vbiosBuildGUIDParams,
                                               sizeof(vbiosBuildGUIDParams)));

        pBiosImageHash->resize(sizeof(vbiosBuildGUIDParams.buildGuid));
        memcpy(pBiosImageHash->data(), vbiosBuildGUIDParams.buildGuid,
               sizeof(vbiosBuildGUIDParams.buildGuid));
    }
    else
    {
        LW2080_CTRL_BIOS_GET_IMAGE_HASH_PARAMS vbiosImageHashParams;
        memset(&vbiosImageHashParams, 0, sizeof(vbiosImageHashParams));

        CHECK_RC(LwRmPtr()->ControlBySubdevice(this,
                                               LW2080_CTRL_CMD_BIOS_GET_IMAGE_HASH,
                                               &vbiosImageHashParams,
                                               sizeof(vbiosImageHashParams)));

        pBiosImageHash->resize(sizeof(vbiosImageHashParams.imageHash));
        memcpy(pBiosImageHash->data(), vbiosImageHashParams.imageHash,
               sizeof(vbiosImageHashParams.imageHash));

    }

    return OK;
}
//--------------------------------------------------------------------
//! \brief Tell the RM to initialize the GPU
//!
//! \param skipRmStateInit Skips most of the RM initialization after
//!     the VBIOS init.  Used by -skip_rm_state_init.
//! \param forceRepost Force the VBIOS to post, even if it was already
//!     posted by SBIOS
//! \param hotSbrRecovery Set true if we're re-initializing the GPU
//!     after a hot secondary bus reset
//!
RC GpuSubdevice::InitGpuInRm
(
    bool          skipRmStateInit,
    bool          forceRepost,
    bool          skipVbiosPost,
    bool          hotSbrRecovery
)
{
    PROFILE_ZONE(GENERIC)

    JavaScriptPtr pJs;
    LW_STATUS status;
    RC rc;

    if ((m_pPciDevInfo->InitDone & MODSRM_INIT_HAL) != 0)
    {
        Printf(Tee::PriNormal, "Attempt to initialize the GPU twice\n");
        return RC::SOFTWARE_ERROR;
    }

    // Get the VBIOS and GSP images
    //
    vector<UINT08> biosImage;
    vector<UINT08> gspImage;
    vector<UINT08> gspLoggingImage;
    if (!Platform::IsVirtFunMode())
    {
        CHECK_RC(GetVBiosImage(&biosImage));

        if (m_GspFwImg != "")
        {
            CHECK_RC(LoadImageFromFile(m_GspFwImg, &gspImage));
        }

        if (m_GspLogFwImg != "")
        {
            CHECK_RC(LoadImageFromFile(m_GspLogFwImg, &gspLoggingImage));
        }
    }

    // Get the Netlist image
    //
    vector<UINT08> netlistImage;
    CHECK_RC(GetNetlistImage(&netlistImage));

    // set the EOS
    pJs->GetProperty(Gpu_Object, Gpu_EndOfSimAssert, &m_EosAssert);

    SCOPED_DEV_INST(this);

    // Call the RM init function
    //
    skipVbiosPost |= (Platform::GetSimulationMode() == Platform::Amodel &&
                          biosImage.empty());
    status = RmInitGpu(m_pPciDevInfo.get(),
                   biosImage.empty() ? NULL : &biosImage[0],
                   static_cast<UINT32>(biosImage.size()),
                   gspImage.empty() ? NULL : &gspImage[0],
                   static_cast<UINT32>(gspImage.size()),
                   gspLoggingImage.empty() ? NULL : &gspLoggingImage[0],
                   static_cast<UINT32>(gspLoggingImage.size()),
                   netlistImage.empty() ? NULL : &netlistImage[0],
                   static_cast<UINT32>(netlistImage.size()),
                   skipRmStateInit,
                   skipVbiosPost,
                   forceRepost,
                   hotSbrRecovery);
    if (status != LW_OK)
    {
        rc = RmApiStatusToModsRC(status);
        // map generic errors to NOT_INITIALIZED
        if (rc == RC::LWRM_ERROR)
        {
            rc.Clear();
            rc = RC::WAS_NOT_INITIALIZED;
        }
    }

    return rc;
}

string GpuSubdevice::GetBootInfoString() const
{
    string bootInfo;

    if (IsSOC() || !Platform::HasClientSideResman())
    {
        return bootInfo;
    }

    // Get system console location from the OS.
    // GetFbConsoleInfo returns addresses only if it detected EFI console.
    PHYSADDR consoleStart = 0;
    UINT64   consoleSize  = 0;
    const RC fbConRc = Xp::GetFbConsoleInfo(&consoleStart, &consoleSize);
    if ((fbConRc == RC::OK) && consoleStart)
    {
        // Check whether the EFI console is in BAR1 (proper UEFI a.k.a. GOP)
        const PHYSADDR bar1Start = GetPhysFbBase();
        const PHYSADDR bar1End   = bar1Start + FbApertureSize();
        const bool     isGop     = (consoleStart >= bar1Start) && (consoleStart < bar1End);

        // Check whether the EFI console is in BAR2 (CSM a.k.a. compatibility mode)
        const PHYSADDR bar2Start = GetPhysInstBase();
        const PHYSADDR bar2End   = bar2Start + InstApertureSize();
        const bool     isCsm     = (consoleStart >= bar2Start) && (consoleStart < bar2End);

        if (isGop)
        {
            bootInfo = "UEFI GOP";
            if (IsPrimary())
            {
                bootInfo += " (WARNING: Primary VGA flag is set)";
            }
            if (!IsUefiBoot())
            {
                bootInfo += " (WARNING: UEFI boot flag is not set)";
            }
        }
        else if (isCsm)
        {
            bootInfo = "UEFI CSM";
            if (!IsPrimary())
            {
                bootInfo += " (WARNING: Primary VGA flag is not set)";
            }
            if (IsUefiBoot())
            {
                bootInfo += " (WARNING: UEFI boot flag is set)";
            }
        }
        else
        {
            // If we ended up here, it means the system was booted in UEFI mode,
            // but the console is not on this GPU (another GPU is the boot GPU).
            bootInfo = "Secondary";
            if (IsPrimary())
            {
                bootInfo += " (WARNING: Primary VGA flag is set)";
            }
            if (IsUefiBoot())
            {
                bootInfo += " (WARNING: UEFI boot flag is set)";
            }
            if (m_IsIOSpaceEnabled)
            {
                bootInfo += " (WARNING: I/O space is enabled)";
            }
        }
    }
    else if (IsPrimary())
    {
        bootInfo = "Primary VGA";
        if (IsUefiBoot())
        {
            bootInfo += " (WARNING: UEFI boot flag is set)";
        }
    }
    else if (IsUefiBoot())
    {
        bootInfo = "UEFI";
        if (fbConRc != RC::UNSUPPORTED_FUNCTION)
        {
            bootInfo += " (WARNING: system console not detected)";
        }
    }
    else
    {
        bootInfo = "Secondary";
        if (m_IsIOSpaceEnabled)
        {
            bootInfo += " (WARNING: I/O space is enabled)";
        }
    }

    return bootInfo;
}

void GpuSubdevice::DestroyInRm()
{
    SCOPED_DEV_INST(this);

    if (!RmDestroyGpu(m_pPciDevInfo.get()))
    {
        Printf(Tee::PriNormal, "RmDestroyGpu failed\n");
    }
}

//! Reset variables that would normally go in the constructor
void GpuSubdevice::Reset()
{
    m_RomVersion.erase();
    m_InfoRomVersion.erase();
    m_RomType = ~0;
    m_RomTimestamp = ~0;
    m_RomExpiration = ~0;
    m_RomSelwrityStatus = ~0;
    if (m_pFsImpl.get())
        m_pFsImpl->Reset();
}

//! Implementation specific shutdown sequence such as register writes
RC GpuSubdevice::Shutdown()
{
    RC rc;

    // Do a final dump of blacklisted pages on -blacklist_pages_on_error
    JavaScriptPtr pJs;
    bool blacklisting = false;
    if ((pJs->GetProperty(pJs->GetGlobalObject(), "g_BlacklistPagesOnEccError",
                          &blacklisting) == OK) &&
        blacklisting)
    {
        Blacklist blacklistedPages;
        CHECK_RC(blacklistedPages.LoadIfSupported(this));
        blacklistedPages.Subtract(m_BlacklistOnEntry);
        if (blacklistedPages.GetSize() > 0)
        {
            Printf(Tee::PriNormal, "New Blacklisted Pages:\n");
            CHECK_RC(blacklistedPages.Print(Tee::PriNormal));
            CHECK_RC(blacklistedPages.WriteJson("new_blacklisted_page"));
        }
    }

    if (m_ResetExceptions)
    {
        // Call RM interface to force reset exceptions
        if (Platform::GetSimulationMode() == Platform::Amodel)
        {
            Printf(Tee::PriWarn,
                "reset exceptions on amodel is not supported - ignoring\n");
        }
        else
        {
            // Lwrrently only GR engine is supported by RM;
            // Please reference lw2080CtrlCmdGpuResetExceptions() for details
            LwRmPtr pLwRm;
            LW2080_CTRL_GPU_RESET_EXCEPTIONS_PARAMS params = {0};
            params.targetEngine = LW2080_CTRL_GPU_RESET_EXCEPTIONS_ENGINE_GR;

            rc = pLwRm->Control(pLwRm->GetSubdeviceHandle(this),
                                LW2080_CTRL_CMD_GPU_RESET_EXCEPTIONS,
                                (void*)&params,
                                sizeof(params));
            if (OK != rc)
            {
                Printf(Tee::PriError, "Call RM to reset exceptions failed!\n");
                return rc;
            }
        }
    }

    // override this function to bang registers on shutdown
    if ((m_PrivRegTable.begin() != m_PrivRegTable.end()) &&
        !GetParentDevice()->GetResetInProgress())
    {
        for (RegTable::const_iterator iter = m_PrivRegTable.begin();
             iter != m_PrivRegTable.end(); ++iter)
        {
            RegWr32(iter->first, iter->second);
        }
        m_PrivRegTable.clear();
        m_RestoreRegTableDone = true;
    }

    //This restore conf registers through config cycles
    if ((m_ConfRegTable.begin() != m_ConfRegTable.end()) &&
        !GetParentDevice()->GetResetInProgress())
    {
        const auto*   pPcie = GetInterface<Pcie>();
        for (RegTable::const_iterator iter = m_ConfRegTable.begin();
             iter != m_ConfRegTable.end(); ++iter)
        {
            PciWrite32(pPcie, iter->first, iter->second);
        }
        if (!m_ConfRegTable.empty())
        {
            UINT32 flushOutstanigWrite;
            PciRead32(pPcie, 0x0, &flushOutstanigWrite);
            Printf(Tee::PriNormal, "Preparing EndOfSim check. Reading "
            "Performing Pciread32 to sync config space access.\n");
        }
        m_ConfRegTable.clear();
    }

    while (!m_eventHooks.empty())
    {
        UnhookResmanEvent(m_eventHooks.begin()->first);
    }

    CHECK_RC(DoEndOfSimCheck(Gpu::ShutDownMode::Normal));
    return OK;
}

//--------------------------------------------------------------------
/*static*/ string GpuSubdevice::ResetTypeToString(GpuSubdevice::ResetType type)
{
    switch (type)
    {
        case GpuSubdevice::ResetType::Hot          : return "hot";
        case GpuSubdevice::ResetType::Fundamental  : return "fundamental";
        case GpuSubdevice::ResetType::FlrMods      : return "flr (MODS)";
        case GpuSubdevice::ResetType::FlrPlatform  : return "flr (Platform)";
    }
    return "unknown";
}

RC GpuSubdevice::FLReset()
{
    RC rc;

    ResetInfo resetInfo;
    resetInfo.resetType       = GpuPtr()->GetUsePlatformFLR() ? ResetType::FlrPlatform :
                                                                ResetType::FlrMods;
    resetInfo.pciCfgMask      = static_cast<UINT32>(ResetPciFunction::Gpu);
    CHECK_RC(ResetImpl(resetInfo));

    return rc;
}

RC GpuSubdevice::HotReset(FundamentalResetOptions funResetOption)
{
    RC rc;
    ResetInfo resetInfo;

    // determine whether fundamental and hot reset are coupled
    bool fundamentalResetCoupled = false;
    if (Regs().IsSupported(MODS_XTL_EP_PRI_RESET_ASSERT_FUNDAMENTAL_RESET_ON_HOT_RESET))
    {
        fundamentalResetCoupled = Regs().Test32(
            MODS_XTL_EP_PRI_RESET_ASSERT_FUNDAMENTAL_RESET_ON_HOT_RESET_RESET);
    }

    // determine whether we want to trigger a fundamental reset
    bool fundamentalReset = false;
    switch (funResetOption)
    {
        case FundamentalResetOptions::Default:
            fundamentalReset = fundamentalResetCoupled;
            break;
        case FundamentalResetOptions::Enable:
            fundamentalReset = true;
            break;
        case FundamentalResetOptions::Disable:
            fundamentalReset = false;
            break;
        default:
            Printf(Tee::PriError, "Unknown fundamental reset option: %d\n",
                   static_cast<UINT32>(funResetOption));
            return RC::ILWALID_ARGUMENT;
    }
    resetInfo.resetType = fundamentalReset ? ResetType::Fundamental : ResetType::Hot;

    if (fundamentalReset != fundamentalResetCoupled)
    {
        if (!Regs().IsSupported(
                MODS_XTL_EP_PRI_RESET_ASSERT_FUNDAMENTAL_RESET_ON_HOT_RESET))
        {
            Printf(Tee::PriError, "Can't trigger %s reset.\n",
                                   ResetTypeToString(resetInfo.resetType).c_str());
            return RC::UNSUPPORTED_FUNCTION;
        }

        if (!Regs().HasRWAccess(MODS_XTL_EP_PRI_RESET))
        {
            Printf(Tee::PriError, "Can't trigger %s reset due to priv mask.\n",
                   ResetTypeToString(resetInfo.resetType).c_str());
            return RC::UNSUPPORTED_FUNCTION;
        }

        resetInfo.modifyResetCoupling = true;
    }

    resetInfo.pciCfgMask      = static_cast<UINT32>(ResetPciFunction::Gpu);
#ifdef INCLUDE_AZALIA
    resetInfo.pciCfgMask     |= static_cast<UINT32>(ResetPciFunction::Azalia);
#endif
    if (HasFeature(GPUSUB_SUPPORTS_USB) && SupportsInterface<XusbHostCtrl>())
    {
        resetInfo.pciCfgMask |= static_cast<UINT32>(ResetPciFunction::Usb);
    }
    if (HasFeature(GPUSUB_SUPPORTS_USB) && SupportsInterface<PortPolicyCtrl>())
    {
        resetInfo.pciCfgMask |= static_cast<UINT32>(ResetPciFunction::Ppc);
    }

    CHECK_RC(ResetImpl(resetInfo));
    return rc;
}

RC GpuSubdevice::ResetImpl(const ResetInfo &resetInfo)
{
    RC rc;

    Printf(Tee::PriNormal, "Performing a %s reset on %s\n",
                            ResetTypeToString(resetInfo.resetType).c_str(),
                            GetName().c_str());

    if (resetInfo.resetType == ResetType::FlrPlatform)
    {
        CHECK_RC(Platform::PciResetFunction(m_pPciDevInfo->PciDomain,
                                            m_pPciDevInfo->PciBus,
                                            m_pPciDevInfo->PciDevice,
                                            m_pPciDevInfo->PciFunction));
        GFWWaitPostReset();
        Printf(Tee::PriNormal, "%s reset succeeded\n",
                                ResetTypeToString(resetInfo.resetType).c_str());
        return rc;
    }

    vector<unique_ptr<PciCfgSpace>> cfgSpaceList;
    PciCfgSpace* pPciCfgSpaceGPU = nullptr;
    PciCfgSpace* pPciCfgSpaceUSB = nullptr;
    PciCfgSpace* pPciCfgSpacePPC = nullptr;

    const bool supportsGfwHoldoff = HasFeature(GPUSUB_SUPPORTS_GFW_HOLDOFF);
    const bool gfwBootHoldoff = supportsGfwHoldoff ? GetGFWBootHoldoff() : false;

    PexDevice* bridge = nullptr;
    UINT32 parentPort;
    CHECK_RC(GetInterface<Pcie>()->GetUpStreamInfo(&bridge, &parentPort));

    if ((bridge == nullptr) && (resetInfo.resetType != ResetType::FlrMods))
    {
        Printf(Tee::PriError, "Cannot find bridge device, %s reset not supported\n",
               ResetTypeToString(resetInfo.resetType).c_str());
        return RC::UNSUPPORTED_HARDWARE_FEATURE;
    }

#ifdef INCLUDE_AZALIA
    PciCfgSpace* pPciCfgSpaceAzalia = nullptr;

    // Uninitialize Azalia device
    AzaliaController* const pAzalia = GetAzaliaController();
    if (resetInfo.pciCfgMask & static_cast<UINT32>(ResetPciFunction::Azalia))
    {
        if (pAzalia)
        {
            CHECK_RC(pAzalia->Uninit());
        }
    }
#endif

    cfgSpaceList.push_back(AllocPciCfgSpace());
    cfgSpaceList.back()->Initialize(m_pPciDevInfo->PciDomain,
                                  m_pPciDevInfo->PciBus,
                                  m_pPciDevInfo->PciDevice,
                                  m_pPciDevInfo->PciFunction);
    pPciCfgSpaceGPU = cfgSpaceList.back().get();

#ifdef INCLUDE_AZALIA
    if (pAzalia &&
        resetInfo.pciCfgMask & static_cast<UINT32>(ResetPciFunction::Azalia))
    {
        SysUtil::PciInfo azaPciInfo = {0};
        CHECK_RC(pAzalia->GetPciInfo(&azaPciInfo));

        cfgSpaceList.push_back(AllocPciCfgSpace());
        CHECK_RC(cfgSpaceList.back()->Initialize(azaPciInfo.DomainNumber,
                                               azaPciInfo.BusNumber,
                                               azaPciInfo.DeviceNumber,
                                               azaPciInfo.FunctionNumber));
        pPciCfgSpaceAzalia = cfgSpaceList.back().get();
    }
#endif

    string usbDriverName;
    string ppcDriverName;

    if (resetInfo.pciCfgMask & static_cast<UINT32>(ResetPciFunction::Usb))
    {
        UINT32 domain, bus, device, function;
        CHECK_RC(GetInterface<XusbHostCtrl>()->GetPciDBDF(&domain, &bus, &device, &function));

        cfgSpaceList.push_back(AllocPciCfgSpace());
        CHECK_RC(cfgSpaceList.back()->Initialize(domain, bus, device, function));

        pPciCfgSpaceUSB = cfgSpaceList.back().get();
        rc = Xp::DisablePciDeviceDriver(domain, bus, device, function, &usbDriverName);
        if (OK != rc)
        {
            Printf(Tee::PriError, "Unable to disable USB driver for %04x:%02x:%02x.%02x\n",
                   domain, bus, device, function);
            return rc;
        }
    }

    DEFER
    {
        if (pPciCfgSpaceUSB && !usbDriverName.empty())
        {
            Xp::EnablePciDeviceDriver(pPciCfgSpaceUSB->PciDomainNumber(),
                                pPciCfgSpaceUSB->PciBusNumber(),
                                pPciCfgSpaceUSB->PciDeviceNumber(),
                                pPciCfgSpaceUSB->PciFunctionNumber(),
                                usbDriverName);
        }
    };

    if (resetInfo.pciCfgMask & static_cast<UINT32>(ResetPciFunction::Ppc))
    {
        UINT32 domain, bus, device, function;
        CHECK_RC(GetInterface<PortPolicyCtrl>()->GetPciDBDF(&domain, &bus, &device, &function));

        cfgSpaceList.push_back(AllocPciCfgSpace());
        CHECK_RC(cfgSpaceList.back()->Initialize(domain, bus, device, function));

        pPciCfgSpacePPC = cfgSpaceList.back().get();
        rc = Xp::DisablePciDeviceDriver(domain, bus, device, function, &ppcDriverName);
        if (OK != rc)
        {
            Printf(Tee::PriError, "Unable to disable PPC driver for %04x:%02x:%02x.%02x\n",
                   domain, bus, device, function);
            return rc;
        }

    }

    DEFER
    {
        if (pPciCfgSpacePPC && !ppcDriverName.empty())
        {
            Xp::EnablePciDeviceDriver(pPciCfgSpacePPC->PciDomainNumber(),
                                pPciCfgSpacePPC->PciBusNumber(),
                                pPciCfgSpacePPC->PciDeviceNumber(),
                                pPciCfgSpacePPC->PciFunctionNumber(),
                                ppcDriverName);
        }
    };

    Printf(Tee::PriDebug, "Saving config space...\n");
    for (auto cfgSpaceIter = cfgSpaceList.begin();
         cfgSpaceIter != cfgSpaceList.end();
         ++cfgSpaceIter)
    {
        CHECK_RC((*cfgSpaceIter)->Save());
    }

    // Optionally override whether fundamental reset gets triggered. Doing this after pci cfg space
    // is saved so that the original value is restored.
    if (resetInfo.modifyResetCoupling)
    {
        Regs().Write32(resetInfo.resetType == ResetType::Fundamental ?
                       MODS_XTL_EP_PRI_RESET_ASSERT_FUNDAMENTAL_RESET_ON_HOT_RESET_RESET :
                       MODS_XTL_EP_PRI_RESET_ASSERT_FUNDAMENTAL_RESET_ON_HOT_RESET_NORESET);
    }

    if (supportsGfwHoldoff && !gfwBootHoldoff)
    {
        // If VBIOS supports the IFR-driven Miata boot flow, then wait for it to finish
        // (To prevent any race b/w current VBIOS exelwtion and the sequence triggered at reset)
        Printf(Tee::PriDebug, "Waiting for previous GFW boot to complete\n");
        if (PollGFWBootComplete(Tee::PriLow) == RC::OK)
        {
            Printf(Tee::PriDebug, "Previous GFW boot completed\n");
        }
    }

    bool gpuLtrEnable = false;
    bool hotPlugEnabled = false;
    if (resetInfo.resetType != ResetType::FlrMods)
    {
        rc = GetInterface<Pcie>()->GetLTREnabled(&gpuLtrEnable);
        if ((rc == RC::UNSUPPORTED_HARDWARE_FEATURE) || (rc == RC::UNSUPPORTED_FUNCTION))
            rc.Clear();
        else if (rc != OK)
            return rc;

        // disable pcie hotplug interrupts. otherwise the OS could get notified and interfere
        CHECK_RC(bridge->GetDownstreamPortHotplugEnabled(parentPort, &hotPlugEnabled));
        if (hotPlugEnabled)
        {
            CHECK_RC(bridge->SetDownstreamPortHotplugEnabled(parentPort, false));
        }
    }

    DEFER
    {
        if (hotPlugEnabled && (resetInfo.resetType != ResetType::FlrMods))
        {
            bridge->SetDownstreamPortHotplugEnabled(parentPort, true);
        }
    };

    // Actual reset trigger
    switch (resetInfo.resetType)
    {
        case ResetType::FlrMods:
            CHECK_RC(Pci::TriggerFLReset(m_pPciDevInfo->PciDomain,
                                         m_pPciDevInfo->PciBus,
                                         m_pPciDevInfo->PciDevice,
                                         m_pPciDevInfo->PciFunction));
            break;
        case ResetType::Hot:
        case ResetType::Fundamental:
            bridge->ResetDownstreamPort(parentPort);
            break;
        default:
            Printf(Tee::PriError, "Reset Impl: unsupported reset impl type");
            MASSERT(!"Reset Impl: unsupported reset impl type");
            return RC::SOFTWARE_ERROR;
    }

    // Sleep for some time before restoring the PCI config space. WAR for Bug 3158926.
    // The PCI spec also mentions a wait of ~100ms before any access of the config space
    Tasker::Sleep(200);

    CHECK_RC(pPciCfgSpaceGPU->CheckCfgSpaceReady(Tasker::GetDefaultTimeoutMs()));

    // the reset will disable LTR on the upstream device. enable if gpu had it enabled.
    if (gpuLtrEnable)
    {
        rc = bridge->SetDownstreamPortLTR(parentPort, true);
        if (rc == RC::UNSUPPORTED_HARDWARE_FEATURE)
            rc.Clear();
        else if (rc != RC::OK)
            return rc;
    }

    bool delayWarPerformed = false;
    for (auto cfgSpaceIter = cfgSpaceList.begin();
         cfgSpaceIter != cfgSpaceList.end();
         ++cfgSpaceIter)
    {
        PciCfgSpace* pCfgSpace = cfgSpaceIter->get();
        vector<UINT32> ssidRegisters;

        if (pCfgSpace == pPciCfgSpaceGPU)
        {
            ssidRegisters.push_back(LW_XVE_SUBSYSTEM_ALIAS);

            // xusb alias register is in the gpu's cfg space
            if (Regs().IsSupported(MODS_XVE_SUBSYSTEM_ALIAS_XUSB))
                ssidRegisters.push_back(Regs().LookupAddress(
                    MODS_XVE_SUBSYSTEM_ALIAS_XUSB));

            // ppc alias register is in the gpu's cfg space
            if (Regs().IsSupported(MODS_XVE_SUBSYSTEM_ALIAS_PPC))
                ssidRegisters.push_back(Regs().LookupAddress(
                    MODS_XVE_SUBSYSTEM_ALIAS_PPC));

        }
#ifdef INCLUDE_AZALIA
        else if (pCfgSpace == pPciCfgSpaceAzalia)
        {
            ssidRegisters.push_back(LW_XVE1_SUBSYSTEM_ALIAS);
        }
#endif

        for (auto address : ssidRegisters)
        {
            // If the subsystem VID and subsystem ID are non-zero we want to skip
            // restoring the register and instead used the value filled in by
            // the IFR. See Bug 1752852
            UINT32 value;
            CHECK_RC(Platform::PciRead32(pCfgSpace->PciDomainNumber(),
                                         pCfgSpace->PciBusNumber(),
                                         pCfgSpace->PciDeviceNumber(),
                                         pCfgSpace->PciFunctionNumber(),
                                         address, &value));
            if (value)
            {
                pCfgSpace->SkipRegRestore(address);
            }
        }

        // WAR - MMU fault with hot_reset on primary
        // Add a delay before restoring config space
        if (IsPrimary() && HasBug(200629532) && !delayWarPerformed)
        {
            Tasker::Sleep(800);
            delayWarPerformed = true;
        }

        CHECK_RC(pCfgSpace->Restore());
    }

    GFWWaitPostReset();

    Printf(Tee::PriNormal, "%s reset succeeded\n",
                            ResetTypeToString(resetInfo.resetType).c_str());
    return rc;
}

void GpuSubdevice::GFWWaitPostReset()
{
    const bool supportsGfwHoldoff = HasFeature(GPUSUB_SUPPORTS_GFW_HOLDOFF);
    const bool gfwBootHoldoff = supportsGfwHoldoff ? GetGFWBootHoldoff() : false;
    if (supportsGfwHoldoff)
    {
        if (gfwBootHoldoff)
        {
            // Sleep long enough for GFW boot to reach the floorsweeping stage
            // See https://confluence.lwpu.com/x/86YRBg and
            //     https://confluence.lwpu.com/x/GSxEIg
            Tasker::Sleep(200);
        }
        else
        {
            // If VBIOS supports the IFR-driven Miata boot flow, then wait for it to finish
            Printf(Tee::PriDebug, "Waiting for GFW boot to complete\n");
            if (PollGFWBootComplete(Tee::PriLow) == RC::OK)
            {
                Printf(Tee::PriDebug, "GFW boot completed\n");
            }
        }
    }
}

//--------------------------------------------------------------------
//! Reset fuses, registers to the state they were before MODS started
void GpuSubdevice::PostRmShutdown()
{
    GetFsImpl()->PostRmShutdown(false);

    RestoreEarlyRegs();

    if (m_SavedBankSwizzle)
        Regs().Write32(MODS_PFB_FBPA_BANK_SWIZZLE, m_OrigBankSwizzle);

    // Handle hacky arg to allow us to figure out what Micron chips we have on our board.
    // Calls HotReset afterwards to reset the broken chip state.
    if (GetNamedPropertyValue("DumpMicronGDDR6Info"))
    {
        DumpMicronGDDR6Info();
    }
}

const FbRanges& GpuSubdevice::GetCriticalFbRanges()
{
    return m_CriticalFbRanges;
}

void GpuSubdevice::AddCriticalFbRange(UINT64 start, UINT64 length)
{
    FbRange range;
    range.start = start;
    range.size = length;
    m_CriticalFbRanges.push_back(range);
}

//! Set the Gpu SubdeviceInstance
RC GpuSubdevice::SetSubdeviceInst(UINT32 subdevInst)
{
    if (subdevInst >= LW2080_MAX_SUBDEVICES)
    {
        Printf(Tee::PriError,
               "Subdevice ID is greater than max subdevice ID.\n");
        return RC::ILWALID_DEVICE_ID;
    }

    m_Subdevice = subdevInst;

    return OK;
}

//! Set the parent GpuDevice
RC GpuSubdevice::SetParentDevice(GpuDevice *pParent)
{
    MASSERT(pParent);

    if (!pParent)
        return RC::ILWALID_ARGUMENT;

    m_Parent = pParent;

    return OK;
}

//! \brief Return a GF100_ZBC_CLEAR object for the subdev, or 0 if not supported
//!
//! ZBC objects are used to maintain the ZBC table for clearing
//! compressed surfaces; see //hw/doc/gpu/fermi/fermi/design/
//! Functional_Descriptions/FermiFBCompression_FD.doc.  Only one ZBC
//! object can be allocated per subdevice, and freeing it clears the
//! ZBC table and potentially corrupts any surface that used ZBC, so
//! we need to avoid freeing ZBC objects once we've allocated them.
//! This method lazily allocates a ZBC object when it's first called.
//!
LwRm::Handle GpuSubdevice::GetZbc()
{
    if (m_hZbc == 0)
    {
        LwRm::Handle hZbc = 0;
        LwRmPtr pLwRm;
        Tasker::MutexHolder mutexHolder(pLwRm->GetMutex());
        if (m_hZbc == 0)
        {
            if (pLwRm->Alloc(pLwRm->GetSubdeviceHandle(this),
                             &hZbc, GF100_ZBC_CLEAR, nullptr) == OK)
            {
                m_hZbc = hZbc;
            }
        }
    }
    return m_hZbc;
}

//! Return Azalia device which is on this GPU
AzaliaController* GpuSubdevice::GetAzaliaController() const
{
#ifndef INCLUDE_AZALIA
    return nullptr;
#else
    if (!m_HasUsableAzalia)
        return nullptr;

    if (IsEmulation())
        return nullptr;

    if (GetSkipAzaliaInit())
    {
        return nullptr;
    }

    AzaliaControllerManager* const pAzaliaMgr = AzaliaControllerMgr::Mgr();
    if (!pAzaliaMgr)
    {
        return nullptr;
    }

    UINT32 numControllers = 0;
    if (OK != pAzaliaMgr->GetNumControllers(&numControllers))
    {
        return nullptr;
    }

    for (UINT32 i=0; i < numControllers; i++)
    {
        Controller *pCtrl = 0;
        SysUtil::PciInfo pciInfo = {0};
        if ((OK == pAzaliaMgr->Get(i, &pCtrl)) &&
            (OK == pCtrl->GetPciInfo(&pciInfo)) &&
            (pciInfo.DomainNumber == GetInterface<Pcie>()->DomainNumber()) &&
            (pciInfo.BusNumber == GetInterface<Pcie>()->BusNumber()) &&
            (pciInfo.DeviceNumber == GetInterface<Pcie>()->DeviceNumber()))
        {
            return static_cast<AzaliaController*>(pCtrl);
        }
    }
    return nullptr;
#endif
}

//! \brief Initialize the Gpu Subdevice
//!
//! The Gpu Subdevice should be initialized by passing the parent and
//! subdevice ID.  The subdevice will then initialize itself through the RM.
RC GpuSubdevice::Initialize(GpuDevice *pParent, UINT32 subdevInst)
{
    RC rc = OK;

    CHECK_RC(SetSubdeviceInst(subdevInst));
    CHECK_RC(SetParentDevice(pParent));
    CHECK_RC(Initialize());

    // Setup features and bugs for the GpuSubdevice that require the system
    // to be initialized  Note that these functions are virtual so this will
    // actually call the function within the derived GpuSubdevice class.
    SetupBugs();
    SetupFeatures();

    // Echo necessary bugs from the RM database into the MODS database
    CHECK_RC(SetupRmBugs());

    CHECK_RC(SetupOglBugs());

    // Print the list of bugs and features
    PrintBugs();
    PrintFeatures();
    CHECK_RC(m_BlacklistOnEntry.LoadIfSupported(this));

    return rc;
}

RC GpuSubdevice::InitializeWithoutRm(GpuDevice *pParent, UINT32 subdevInst)
{
    RC rc;
    CHECK_RC(SetSubdeviceInst(subdevInst));
    CHECK_RC(SetParentDevice(pParent));

#if defined(INCLUDE_DGPU)
    // add more here

    MASSERT(m_Fb == nullptr);
    const GpusubImpls *pImpls = FindGpusubImpls(this);
    if (!pImpls)
        return RC::SOFTWARE_ERROR;
    m_Fb = ((IsFbBroken() || GpuPtr()->IsFusing()) ?
             new NoFrameBuffer(this, nullptr) :
             pImpls->CreateFb(this, nullptr));
    if (m_Fb == nullptr)
        return RC::DID_NOT_INSTALL_SINGLETON;
    CHECK_RC(m_Fb->Initialize());

    if (!m_pFuse)
    {
        m_pFuse = pImpls->CreateFuse(this);
    }
#endif

    // Create GPU specific PMU object for secure fuseblow
    if (!m_pPmu)
    {
        m_pPmu = PMU::Factory(this);
    }

    SetupBugs();
    SetupFeatures();
    PrintBugs();
    PrintFeatures();
    return rc;
}

//! \brief Initialize the Gpu Subdevice
//!
RC GpuSubdevice::Initialize(Trigger trigger)
{
    StickyRC rc;
    LwRmPtr   pLwRm;
    // While entering standby, subdev ctxts are not released in RM.Don't reallocate
    // them during resume
    if (Trigger::STANDBY_RESUME != trigger)
    {
        //RM Subdev handles should not be valid. Otherwise there may be a resource leak
        MASSERT(!m_IsLwRmSubdevHandlesValid);
        if (m_IsLwRmSubdevHandlesValid)
        {
            Printf(Tee::PriError, "Subdev handles should not be valid. Possible resource leak\n");
            return RC::SOFTWARE_ERROR;
        }

        for (UINT32 client = 0; client < LwRmPtr::GetValidClientCount(); client++)
        {
            CHECK_RC(LwRmPtr(client)->AllocSubDevice(m_Parent->GetDeviceInst(),
                                                     m_Subdevice));
            // Acquire mutex for enabling/disabling SMC to ensure that SMC is
            // not enabled/disabled until SMC sensitive regs are read in a
            // monitor iteration
            Tasker::MutexHolder mh(GetSmcEnabledMutex());

            // Configure SMC Partitions if requested
            if (UsingMfgModsSMC())
            {
                // Create and configure SMC partitions
                CHECK_RC(PartitionSmc(client));
            }

            // Init SMC resources if SMC is enabled
            CHECK_RC(InitSmcResources(client));
        }
        m_IsLwRmSubdevHandlesValid = true;
    }

    if (!m_IsLwRmSubdevHandlesValid)
    {
        Printf(Tee::PriError, "Subdevice ctxt needs to be allocated in RM\n");
        return RC::SOFTWARE_ERROR;
    }

    CHECK_RC(SetupUserdWriteback(Regs()));

    // Allocate SubdeviceDiag handle
    // Not available on Android
    if (!Platform::UsesLwgpuDriver())
    {
        CHECK_RC(pLwRm->Alloc(pLwRm->GetSubdeviceHandle(this),
                              &m_SubdeviceDiag,
                              LW20_SUBDEVICE_DIAG,
                              nullptr));
    }

    m_Initialized = true;
    Reset(); // reset implementation details

    GpusubImpls *pImpls = FindGpusubImpls(this);
    if (nullptr == pImpls)
    {
        Printf(Tee::PriError, "Could not find implemenation creators for %s.\n",
               Gpu::DeviceIdToString(DeviceId()).c_str());
        return RC::WAS_NOT_INITIALIZED;
    }

    // Determine whether this is real or emulated GPU
    LW2080_CTRL_GPU_GET_SIMULATION_INFO_PARAMS params = {0};
    rc = LwRmPtr()->ControlBySubdevice(this,
            LW2080_CTRL_CMD_GPU_GET_SIMULATION_INFO, &params, sizeof(params));
    if (OK == rc)
    {
        switch (params.type)
        {
            default:
                MASSERT(!"Unknown SIMULATION_INFO_TYPE");
                // fall through

            case LW2080_CTRL_GPU_GET_SIMULATION_INFO_TYPE_NONE:
            case LW2080_CTRL_GPU_GET_SIMULATION_INFO_TYPE_MODS_AMODEL:
            case LW2080_CTRL_GPU_GET_SIMULATION_INFO_TYPE_LIVE_AMODEL:
            case LW2080_CTRL_GPU_GET_SIMULATION_INFO_TYPE_FMODEL:
            case LW2080_CTRL_GPU_GET_SIMULATION_INFO_TYPE_RTL:
            case LW2080_CTRL_GPU_GET_SIMULATION_INFO_TYPE_UNKNOWN:
                m_IsDFPGA = false;
                m_IsEmulation = false;
                break;

            case LW2080_CTRL_GPU_GET_SIMULATION_INFO_TYPE_EMU:
            case LW2080_CTRL_GPU_GET_SIMULATION_INFO_TYPE_EMU_LOW_POWER:
                m_IsDFPGA = false;
                m_IsEmulation = true;
                break;

            case LW2080_CTRL_GPU_GET_SIMULATION_INFO_TYPE_DFPGA:
                m_IsDFPGA = true;
                m_IsEmulation = true;
                break;
            case LW2080_CTRL_GPU_GET_SIMULATION_INFO_TYPE_DFPGA_RTL:
            case LW2080_CTRL_GPU_GET_SIMULATION_INFO_TYPE_DFPGA_FMODEL:
                // These are sim builds of the DFPGA netlists (no graphics engine)
                m_IsDFPGA = true;
                m_IsEmulation = false;
                break;
        }
    }

    if (!IsSOC() && !Platform::HasClientSideResman() && m_IsEmulation)
    {
        Printf(Tee::PriError, "Running on emulation with kernel mode RM is not supported!\n");
        return RC::UNSUPPORTED_SYSTEM_CONFIG;
    }

    if (Platform::IsPhysFunMode() || Platform::IsDefaultMode())
    {
        // Create suitable framebuffer object
        MASSERT(nullptr == m_Fb);  // ShutdownSubdevice should have been run.
        if (IsFbBroken())
            m_Fb = new NoFrameBuffer(this, pLwRm.Get());
        else
            m_Fb = pImpls->CreateFb(this, pLwRm.Get());
        if (nullptr == m_Fb)
            return RC::DID_NOT_INSTALL_SINGLETON;
        Printf(Tee::PriDebug, "FrameBuffer[%d] = %s\n",
            m_Subdevice, m_Fb->GetName());
        rc = m_Fb->Initialize();

        if (!m_pPerf)
        {
            if (IsSOC())
            {
                CHECK_RC(InitializePerfSoc());
            }
            else
            {
                CHECK_RC(InitializePerfGPU());
            }
        }
        MASSERT(m_pPerf);

        if (!m_pPower)
        {
            m_pPower = new Power(this);
        }
        MASSERT(m_pPower);

    #if defined(INCLUDE_DGPU)
        if (!m_pFuse)
        {
            m_pFuse = pImpls->CreateFuse(this);
        }

        if (!(m_pAvfs || IsSOC()))
        {
            m_pAvfs = new Avfs(this);
            MASSERT(m_pAvfs);
        }

        if (!(m_pVolt3x || IsSOC()))
        {
            m_pVolt3x = new Volt3x(this);
            MASSERT(m_pVolt3x);
        }

    #endif

        if (!m_pSubdevGr.get())
        {
            m_pSubdevGr.reset(SubdeviceGraphics::Alloc(this));
        }

        if (!m_pSubdevFb.get())
        {
            m_pSubdevFb.reset(SubdeviceFb::Alloc(this));
        }

        if (!m_pFalconEcc.get())
        {
            if (Platform::UsesLwgpuDriver())
            {
                m_pFalconEcc = make_unique<TegraFalconEcc>(this);
            }
            else
            {
                m_pFalconEcc = make_unique<RmFalconEcc>(this);
            }
        }

        if (!m_pHsHubEcc.get())
        {
            m_pHsHubEcc = make_unique<HsHubEcc>(this);
        }

        if (!m_pPciBusEcc.get())
        {
            m_pPciBusEcc = make_unique<PciBusEcc>(this);
        }

        if (!m_pThermal)
        {
            m_pThermal = new Thermal(this);
        }
        MASSERT(m_pThermal);
        rc = m_pThermal->Initialize();
    }
    else
    {
        m_Fb = new VmFrameBuffer(this, pLwRm.Get());
        rc = m_Fb->Initialize();
    }
    // Remember about Azalia here so that during power off/on scenarios
    //  registers don't need to be read to determine the answer:
    m_HasUsableAzalia = HasFeature(GPUSUB_HAS_AZALIA_CONTROLLER) &&
        IsFeatureEnabled(GPUSUB_HAS_AZALIA_CONTROLLER);

    memset(&m_FbRegionInfo, 0, sizeof(m_FbRegionInfo));

    RC tempRC = LwRmPtr()->ControlBySubdevice(this,
        LW2080_CTRL_CMD_FB_GET_FB_REGION_INFO, &m_FbRegionInfo,
        sizeof(m_FbRegionInfo));

    if (OK == tempRC)
    {
        Printf(Tee::PriDebug, "FB region count = %u\n",
               static_cast<unsigned>(m_FbRegionInfo.numFBRegions));

        for (UINT32 i = 0; i < m_FbRegionInfo.numFBRegions; ++i)
        {
            Printf(Tee::PriDebug, "FB region %u minAddress = %llx\n", i,
                    m_FbRegionInfo.fbRegion[i].base);
            Printf(Tee::PriDebug, "FB region %u maxAddress = %llx\n", i,
                    m_FbRegionInfo.fbRegion[i].limit);
            Printf(Tee::PriDebug, "FB region %u reserved = %llx\n", i,
                    m_FbRegionInfo.fbRegion[i].reserved);
            Printf(Tee::PriDebug, "FB region %u performance = %u\n", i,
                static_cast<unsigned>(m_FbRegionInfo.fbRegion[i].performance));
            Printf(Tee::PriDebug, "FB region %u supportISO = %s\n", i,
                    m_FbRegionInfo.fbRegion[i].supportISO ? "true" : "false");
            Printf(Tee::PriDebug, "FB region %u supportCompressed = %s\n", i,
                    m_FbRegionInfo.fbRegion[i].supportCompressed ? "true"
                                                                 : "false");
            Printf(Tee::PriDebug, "FB region %u bProtected = %s\n", i,
                    m_FbRegionInfo.fbRegion[i].bProtected ? "true" : "false");
        }
    }
    else if (RC::LWRM_NOT_SUPPORTED != tempRC)
    {
        rc = tempRC;

        Printf(Tee::PriDebug, "No FB region info\n");
    }

    // Set voltage offset requested by the SOC
    //
    if (IsSOC() && Platform::HasClientSideResman())
    {
        INT32 voltageOffsetMv;
        rc = CheetAh::SocPtr()->GetGpuVoltageOffsetMv(&voltageOffsetMv);
        if (rc == OK)
            CHECK_RC(m_pPerf->SetDefaultVoltTuningOffsetMv(voltageOffsetMv));
        else if (rc == RC::UNSUPPORTED_FUNCTION)
            rc.Clear();
        else
            CHECK_RC(rc);
    }

    // Ensure that the GPU is initialized
    if (IsSOC() && Platform::UsesLwgpuDriver() && ! IsFakeTegraGpu())
    {
        CHECK_RC(InitializeGPU());
    }

    // Cache whether user has requested for Emulation Mem Triggers to happen
    if (IsEmulation())
    {
        JavaScriptPtr pJs;

        // Using a temp RC since the rc above is Sticky
        RC tmpRc = pJs->GetProperty(pJs->GetGlobalObject(), "g_EmuMemTrigger", &m_DoEmuMemTrigger);
        if (tmpRc != OK)
        {
            // In case of an error, we'll only report it as a warning; it's possible that external
            // JS scripts won't have the g_EmuMemTrigger defined, which is fine if they don't care
            // to use this trigger
            Printf(Tee::PriWarn, "g_EmuMemTrigger variable is not defined\n");
        }
    }

    // Initialize Port Policy Controller's Ftb interface
    if (SupportsInterface<PortPolicyCtrl>())
    {
        CHECK_RC(GetInterface<PortPolicyCtrl>()->InitializeFtb());
    }

    // When running MODS with SRIOV, save current GpuSubdevice pointer
    // in VmiopElwManager for future reference. VmiopElwManager doesn't
    // have another way to query it when needed.
    // e.g. VmiopElwPhysFun::CallPluginPfRegRead() needs it to read registers
    //      using Reghal
    if (Platform::IsPhysFunMode() || Platform::IsVirtFunMode())
    {
        CHECK_RC(VmiopElwManager::Instance()->SetGpuSubdevInstance(this));
    }

#if !defined(TEGRA_MODS) && defined(DEBUG)
    bool skipEcidCheck;
    JavaScriptPtr pJs;
    pJs->GetProperty(pJs->GetGlobalObject(), "g_SkipEcidCheck", &skipEcidCheck);

    if (!skipEcidCheck && !Platform::IsVirtFunMode())
    {
    // Check that Fused and RM control ECIDs are identical
        if (!AreEcidsIdentical())
        {
            Printf(Tee::PriError, "Fused and RM control ECIDs are different\n");

            return RC::SOFTWARE_ERROR;
        }
    }
#endif

    return rc;
}

RC GpuSubdevice::Initialize()
{
    return Initialize(Trigger::INIT_SHUTDOWN);
}

RC GpuSubdevice::InitializeGPU()
{
    Printf(Tee::PriDebug, "Initializing the GPU\n");

    RC rc;
    LwRmPtr pLwRm;

    const UINT32 socGpFifoClasses[] =
    {
        AMPERE_CHANNEL_GPFIFO_A,  // T234 (GA10B)
        VOLTA_CHANNEL_GPFIFO_A    // T194 (GV11B)
    };

    UINT32 channelClass = 0;
    CHECK_RC(pLwRm->GetFirstSupportedClass(static_cast<UINT32>(NUMELEMS(socGpFifoClasses)),
                                           &socGpFifoClasses[0],
                                           &channelClass,
                                           m_Parent));

    LW_CHANNELGPFIFO_ALLOCATION_PARAMETERS channelParams = { };
    UINT32 channelHandle = 0;
    CHECK_RC(pLwRm->Alloc(pLwRm->GetDeviceHandle(m_Parent),
                          &channelHandle,
                          channelClass,
                          &channelParams));
    DEFER
    {
        pLwRm->Free(channelHandle);
    };

    // Retrieve USERD pointer
    void* channelAddr = nullptr;
    CHECK_RC(pLwRm->MapMemory(channelHandle,
                              0,
                              0x40, // dummy size, ignored by rmapi_tegra
                              &channelAddr,
                              0,
                              this));
    DEFER
    {
        pLwRm->UnmapMemory(channelHandle, channelAddr, 0, this);
    };

    // rmapi_tegra allocates channels lazily.  This call actually
    // instantiates the channel inside rmapi_tegra.
    // TODO In the future allocate a pushbuffer surface and ilwoke
    // LwRmTegraKickoff to send a NOP to the GPU in the pushbuffer.
#ifdef TEGRA_MODS
    if (LW_OK != LwRmTegraChannelSetTimeout(channelAddr, 33000U))
    {
        Printf(Tee::PriError, "LWRM: *** Failed to initialize the GPU\n");
        return RC::LWRM_ERROR;
    }
#endif

    return OK;
}

//! Shutdown the GpuSubdevice
//! If shutdown is triggered as part of standby procedure, retain subdevice ctxt in RM
//! Otherwise free it
void GpuSubdevice::ShutdownSubdevice(Trigger trigger)
{
    LwRmPtr pLwRm;
    Shutdown(); // call any implementation-specific shutdown semantics first

    delete m_Fb;
    m_Fb = nullptr;

    if (m_pThermal)
    {
        m_pThermal->Cleanup();
    }
    if (Trigger::INIT_SHUTDOWN == trigger)
    {
        delete m_pThermal;
        m_pThermal = nullptr;
    }
    if (m_pPerf)
    {
        m_PerfWasOwned = m_pPerf->IsOwned();
        m_pPerf->Cleanup();
        delete m_pPerf;
        m_pPerf = nullptr;
    }
    if (m_pPower)
    {
        delete m_pPower;
        m_pPower = nullptr;
    }
#if defined(INCLUDE_DGPU)
    if (m_pAvfs)
    {
        m_pAvfs->Cleanup();
        delete m_pAvfs;
        m_pAvfs = nullptr;
    }
    if (m_pVolt3x)
    {
        m_pVolt3x->Cleanup();
        delete m_pVolt3x;
        m_pVolt3x = nullptr;
    }
#endif

    if (!m_pTempCtrl.empty())
    {
        m_pTempCtrl.clear();
    }

    // If m_pPmu is still non-NULL after FreeClientStuff(...),
    // it must been created without RM initialized in InitializeWithoutRm(...)
    if (m_pPmu)
    {
        delete m_pPmu;
        m_pPmu = nullptr;
    }

    m_pSubdevGr.reset();
    m_pSubdevFb.reset();
    m_pFalconEcc.reset();
    m_pHsHubEcc.reset();
    m_pPciBusEcc.reset();

    if (m_SubdeviceDiag)
    {
        pLwRm->Free(m_SubdeviceDiag);
        m_SubdeviceDiag = 0;
    }

    if (m_hZbc)
    {
        pLwRm->Free(m_hZbc);
        m_hZbc = 0;
    }

    // Retain subdevice handles as part of standby procedure. According to new RM
    // object management model, when we release sub device handle, any channel mappings
    // associated with that subdevice will be released too. We want to retain channels across
    // standby/reset/resume cycle. Hence don't release them for those cases.
    if (Trigger::STANDBY_RESUME != trigger)
    {
        if (m_Initialized)
        {
            MASSERT(m_IsLwRmSubdevHandlesValid);
            for (auto& partitionHandle : m_SmcResource.clientPartitionHandle)
            {
                if (partitionHandle.second != 0)
                {
                    LwRmPtr(partitionHandle.first)->Free(partitionHandle.second);
                }
            }
            m_SmcResource.clientPartitionHandle.clear();

            if (!Platform::IsVirtFunMode() && (m_PartInfo.size() > 0))
            {
                // Destroy SMC partitions for bug 200550862
                LW2080_CTRL_GPU_SET_PARTITIONS_PARAMS params = {};
                params.partitionCount = static_cast<UINT32>(m_PartInfo.size());

                for (UINT32 i = 0; i < m_PartInfo.size(); i++)
                {
                    params.partitionInfo[i].bValid = false;
                    params.partitionInfo[i].swizzId = m_PartInfo[i].swizzId;
                }
                RC rc = LwRmPtr()->ControlBySubdevice(this,
                                                  LW2080_CTRL_CMD_GPU_SET_PARTITIONS,
                                                  &params,
                                                  sizeof(params));
                if (rc != RC::OK)
                {
                    Printf(Tee::PriError, "SMC partition destroy Failed\n");
                    MASSERT(0);
                }
            }

            for (UINT32 client = 0;
                 client < LwRmPtr::GetValidClientCount(); client++)
            {
                LwRmPtr(client)->FreeSubDevice(m_Parent->GetDeviceInst(),
                                               m_Subdevice);
            }
        }
        m_IsLwRmSubdevHandlesValid = false;
    }

    m_Initialized = false;
    // don't ilwalidate.  We just shut down, we have not cleared,
    // destroyed, or otherwise reset the state
}

void GpuSubdevice::ShutdownSubdevice()
{
    ShutdownSubdevice(Trigger::INIT_SHUTDOWN);
}

RC GpuSubdevice::AllocClientStuff(UINT32 resetControlsMask)
{
    RC rc;
    JavaScriptPtr pJs;

    // SMC dynamic partitioning + dynamic TPC FS tests change TPC count on the
    // chip based on the partition requested. The change in TPC count is
    // reflected after issuing a reset RM ctrl call. Resetting it here so that
    // GetTpcCount() makes the RM ctrl call to get the updated TPC count.
    m_TpcCount = 0;

    MASSERT(!m_pClockThrottle);
    MASSERT(!m_ClockThrottleReserved);

    if (!GetPhysLwBase() && !IsSOC())
    {
        LW2080_CTRL_BUS_GET_PCI_BAR_INFO_PARAMS params = {0};
        RC rc;
        CHECK_RC(LwRmPtr()->ControlBySubdevice(this,
                                          LW2080_CTRL_CMD_BUS_GET_PCI_BAR_INFO,
                                          &params,
                                          sizeof(params)));
        m_pPciDevInfo->PhysLwBase = params.pciBarInfo[0].barOffset;
        m_pPciDevInfo->PhysFbBase = params.pciBarInfo[1].barOffset;
        m_pPciDevInfo->PhysInstBase = params.pciBarInfo[2].barOffset;
        m_pPciDevInfo->LwApertureSize = static_cast<UINT32>(params.pciBarInfo[0].barSizeBytes);
        m_pPciDevInfo->FbApertureSize = params.pciBarInfo[1].barSizeBytes;
        m_pPciDevInfo->InstApertureSize = params.pciBarInfo[2].barSizeBytes;
    }

    m_pClockThrottle = ClockThrottle::Factory(this);

    MASSERT(m_pClockThrottle);

    MASSERT(!m_pPmu);

    // Don't do this on CheetAh
    if (!Platform::UsesLwgpuDriver())
    {
        m_pPmu = PMU::Factory(this);

        rc = m_pPmu->Initialize();
        if (rc != OK)
        {
            m_pPmu->Shutdown(false);
            delete m_pPmu;
            m_pPmu = nullptr;

            if (rc == RC::UNSUPPORTED_FUNCTION || rc == RC::LWRM_NOT_SUPPORTED)
            {
                rc.Clear(); // Suppress this error.
            }
            else
            {
                return rc;
            }
        }

        // SEC2 RTOS initialization
        m_pSec2Rtos = new Sec2Rtos(this);

        rc = m_pSec2Rtos->Initialize();
        if (rc != OK)
        {
            m_pSec2Rtos->Shutdown();
            delete m_pSec2Rtos;
            m_pSec2Rtos = nullptr;

            if (rc == RC::UNSUPPORTED_FUNCTION || rc == RC::LWRM_NOT_SUPPORTED)
            {
                rc.Clear(); // Suppress this error.
            }
            else
            {
                return rc;
            }
        }
    }

    // When toggling MIG in InfoRom, we need to disable ecc error counter since
    // RM calls to get ecc error count will fail.
    // Don't worry about EDC since it's not on HBM devices.
    if (!m_pEccErrCounter &&
        !m_SkipEccErrCtrInit &&
        (Platform::IsPhysFunMode() || Platform::IsDefaultMode()))
    {
        m_pEccErrCounter = new EccErrCounter(this);
        CHECK_RC(m_pEccErrCounter->Initialize());
    }

    bool checkEdcErrors = false;
    if (Platform::IsPhysFunMode() || Platform::IsDefaultMode())
        CHECK_RC(pJs->GetProperty(Gpu_Object, Gpu_CheckEdcErrors, &checkEdcErrors));

    bool isEdcEnabled;
    UINT32 edcEnabledMask;
    CHECK_RC(GetEdcEnabled(&isEdcEnabled, &edcEnabledMask));
    if (!m_pEdcErrCounter && checkEdcErrors && isEdcEnabled)
    {
        m_pEdcErrCounter = new EdcErrCounter(this);
        CHECK_RC(m_pEdcErrCounter->Initialize());
    }

    bool resume = (resetControlsMask & ResetControls::DoResume) ? true : false;
    bool bSkipErrCountersResume =
        (resetControlsMask & ResetControls::DoZpiReset) ? true : false;
    if (m_pThermal && !resume && !bSkipErrCountersResume)
    {
        CHECK_RC(m_pThermal->InitOverTempErrCounter());
    }

    m_pPerfmon = CreatePerfmon();

    if (!IsSOC() && !resume)
    {
        auto pLwLink = GetInterface<LwLink>();
        if (pLwLink)
        {
            CHECK_RC(pLwLink->Initialize({ }));
        }
        auto pC2C = GetInterface<C2C>();
        if (pC2C)
        {
            CHECK_RC(pC2C->Initialize());
        }
    }
    return rc;
}

RC GpuSubdevice::FreeClientStuff(UINT32 resetControlsMask)
{
    StickyRC firstRc;
    bool standBy = (resetControlsMask & ResetControls::DoStandby) ? true : false;
    bool bGpuLost = (resetControlsMask & ResetControls::GpuIsLost) ? true : false;
    bool bSkipErrCountersShutdown =
        (resetControlsMask & ResetControls::DoZpiReset) ? true : false;

    if (!IsSOC() && !standBy)
    {
        LwLink* pLwLink = GetInterface<LwLink>();
        if (pLwLink)
        {
            firstRc = pLwLink->Shutdown();
        }

        Gpio* pGpio = GetInterface<Gpio>();
        if (pGpio)
        {
            firstRc = pGpio->Shutdown();
        }
    }

    // If MODS is already in the process of shutting down, ignore any RM reports
    // the the GPU has been lost if we expect the GPU to have been lost (since MODS
    // may be shutting down for that very reason)
    auto IgnoreGpuLost = [&] (RC rc)
    {
        if (!standBy && bGpuLost && (rc != RC::LWRM_GPU_IS_LOST))
            firstRc = rc;
    };

    if (m_pHBMImpl && !standBy)
    {
        IgnoreGpuLost(m_pHBMImpl->ShutDown());
    }

    if (!(standBy || bSkipErrCountersShutdown))
    {
        if (m_pThermal)
        {
            IgnoreGpuLost(m_pThermal->ShutdownOverTempErrCounter());
            IgnoreGpuLost(m_pThermal->ShutdownOverTempCounter());
        }

        if (m_pEdcErrCounter)
        {
            IgnoreGpuLost(m_pEdcErrCounter->ShutDown(true));
            delete m_pEdcErrCounter;
            m_pEdcErrCounter = nullptr;
        }

        if (m_pEccErrCounter)
        {
            IgnoreGpuLost(m_pEccErrCounter->ShutDown(true));
            delete m_pEccErrCounter;
            m_pEccErrCounter = nullptr;
        }
    }

    // If PMU was created, shut it down
    if (m_pPmu)
    {
        m_pPmu->Shutdown(GetEosAssert());
        delete m_pPmu;
        m_pPmu = nullptr;
    }

    // If SEC2 RTOS was created, shut it down
    if (m_pSec2Rtos)
    {
        m_pSec2Rtos->Shutdown();
        delete m_pSec2Rtos;
        m_pSec2Rtos = nullptr;
    }

    if (m_pClockThrottle)
    {
        delete m_pClockThrottle;
        m_pClockThrottle = 0;
        m_ClockThrottleReserved = false;
    }

    if (m_pPerfmon)
    {
        m_pPerfmon->ShutDown();
        delete m_pPerfmon;
        m_pPerfmon = nullptr;
    }

    // free notifier surface
    if (m_pNotifierSurface.get() && !standBy)
    {
        LwRmPtr pLwRm;
        LW2080_CTRL_EVENT_SET_MEMORY_NOTIFIES_PARAMS memNotifiesParams = { 0 };
        memNotifiesParams.hMemory = LW01_NULL_OBJECT;
        IgnoreGpuLost(pLwRm->Control(pLwRm->GetSubdeviceHandle(this),
                          LW2080_CTRL_CMD_EVENT_SET_MEMORY_NOTIFIES,
                          &memNotifiesParams, sizeof(memNotifiesParams)));

        m_pNotifierSurface->Unmap();
        m_pNotifierMem = nullptr;
        m_pNotifierSurface->Free();
        m_pNotifierSurface.reset();
    }

    return firstRc;
}

//------------------------------------------------------------------------------
RC GpuSubdevice::SetBackdoorGpuPowerGc6(bool pwrOn)
{
    return OK;
}

//------------------------------------------------------------------------------
RC GpuSubdevice::SetBackdoorGpuLinkL2(bool isL2)
{
    return OK;
}

//------------------------------------------------------------------------------
RC GpuSubdevice::SetBackdoorGpuAssertPexRst(bool isAsserted)
{
    return OK;
}

//------------------------------------------------------------------------------
// Put the GPU into GC6 mode. In this mode all of the power rails have been
// turned off with the exception of the FB. No access to the GPU should be done
// until ExitGc6() has been called.
// It is up to the caller to insure this requirement.
// Prior to calling this API the caller should save of the Azalia config space.
// The PCI config space is managed internally by RM.
RC GpuSubdevice::EnterGc6(GC6EntryParams &entryParams)
{
    RC rc;
    LwRmPtr pLwRm;

    // There are other threads that could access the GPU (PMU event, disp,
    // mods console,etc)

    // Trying to pause the mods console when queued prints are not enabled will
    // cause a deadlock condition.
    // Disable the current console before GPU is shutdown
    CHECK_RC(ConsoleManager::Instance()->PrepareForSuspend(GetParentDevice()));

    // We need to cover the case where an interrupt has been generated and an
    // event has been created but not processed before we mask off the interrupt
    // processing.
    BgLogger::PauseLogging();
    m_pPmu->Pause(true);
    Tasker::PollHw(Tasker::GetDefaultTimeoutMs(), [this]()->bool
    {
        return m_pPmu->IsPaused();
    }, MODS_FUNCTION);
    m_InGc6 = true;

    Tasker::DetachThread detach;

    LW2080_CTRL_GC6_ENTRY_PARAMS gc6EntryParams = entryParams;

    CHECK_RC(pLwRm->Control(pLwRm->GetSubdeviceHandle(this),
                (UINT32)LW2080_CTRL_CMD_GC6_ENTRY,
                &gc6EntryParams,
                sizeof (gc6EntryParams)));

    return rc;
}

//------------------------------------------------------------------------------
// Take the Gpu out of GC6 mode. If successful all of the power rails will be
// restored and the GpuInstance data will be intact.
// After this call the caller should restore the Azalia config space.
RC GpuSubdevice::ExitGc6(GC6ExitParams &exitParams)
{
    {
        RC rc;
        LwRmPtr pLwRm;
        LW2080_CTRL_GC6_EXIT_PARAMS gc6ExitParams = exitParams;

        Tasker::DetachThread detach;
        CHECK_RC(pLwRm->Control(pLwRm->GetSubdeviceHandle(this),
                    (UINT32)LW2080_CTRL_CMD_GC6_EXIT,
                    &gc6ExitParams,
                    sizeof (gc6ExitParams)));
    }

    m_InGc6 = false;

    // Re-enable the current console after GPU resumes, if applicable
    ConsoleManager::Instance()->ResumeFromSuspend(GetParentDevice());

    BgLogger::UnpauseLogging();

    m_pPmu->Pause(false);
    // Note: It is unclear why we allow other threads to fiddle with the PMU
    // during GCx cycles.  This can potentially lead to races.
    Tasker::PollHw(Tasker::GetDefaultTimeoutMs(), [this]()->bool
    {
        return !m_pPmu->IsPaused();
    }, MODS_FUNCTION);

    return RC::OK;
}

RC GpuSubdevice::StandBy()
{
    MASSERT(!m_InStandBy);

    if (GetParentDevice()->GetNumSubdevices() > 1)
    {
        Printf(Tee::PriLow,
            "Unable to put the GPU in standby when it's linked in SLI mode\n");
        return RC::SLI_GPU_LINKED;
    }

    RC rc;
    if (m_pPerf->IsLocked())
    {
        CHECK_RC(m_pPerf->GetLwrrentPerfPoint(&m_LwrPerfPoint));
    }

    // this is a hack: Lwrrently MODS is detecting the 'nominal' point
    // in bios by reading the "boot setting". This boot clock is
    // stored in the Perf object.  In a suspend/resume, we lwrrently
    // delete and recreate the Perf object.  The problem is that if we
    // were to change a PerfPoint, MODS would create a nominal point
    // at the last PerfPoint, and the last PerfPoint would not
    // necessarily the "boot setting" - and we would incorrectly
    // create the preset points in the Perf object
    if (!m_pPerf->IsPState30Supported())
    {
        CHECK_RC(m_pPerf->RestorePStateTable(Perf::ILWALID_PSTATE,
                                             Perf::AllDomains));
    }

    CHECK_RC(FreeClientStuff(ResetControls::DoStandby));

    CHECK_RC(GetParentDevice()->GetUserdManager()->Free());

    // Disable the current console before GPU suspend, if it is being used
    CHECK_RC(ConsoleManager::Instance()->PrepareForSuspend(GetParentDevice()));

    GetParentDevice()->GetDisplay()->FreeGpuResources(
            GetParentDevice()->GetDeviceInst());
    GetParentDevice()->GetDisplay()->FreeRmResources(
            GetParentDevice()->GetDeviceInst());

    ShutdownSubdevice(Trigger::STANDBY_RESUME);

    // This causes HasPendingInterrupt to return false, effectively pausing the
    // PollingInterruptThread from handling any new interrupts.
    m_InStandBy = true;

    // Pause the thread which is collecting PCIE errors
    m_pPausePexErrThread.reset(new PexDevMgr::PauseErrorCollectorThread(
                (PexDevMgr*)DevMgrMgr::d_PexDevMgr));

    m_StandByIntr = m_HookedIntr;
    CHECK_RC(UnhookISR());

    if (!RmSetPowerState(GetGpuInst(), LW_POWER_ADAPTER_STATE_S3))
    {
        Printf(Tee::PriError, "Unable to shut down the GPU\n");
        return RC::SOFTWARE_ERROR;
    }
    return OK;
}

RC GpuSubdevice::Resume()
{
    MASSERT(m_InStandBy);

    if (!RmSetPowerState(GetGpuInst(), LW_POWER_ADAPTER_STATE_S0))
    {
        return RC::WAS_NOT_INITIALIZED;
    }

    m_pPausePexErrThread.reset(nullptr);

    RC rc;

    // This effectively unpauses the PollingInterruptThread so it can resume
    // handling interrupts
    m_InStandBy = false;

    if (m_StandByIntr != HOOKED_NONE)
    {
        // Make sure all interrupts are disabled
        // Interrupts are re-enabled by RM in osReleaseSema()
        for (UINT32 i = 0; i < GetNumIntrTrees(); i++)
        {
            SetIntrEnable(i, intrDisabled);
        }

        CHECK_RC(HookInterruptsByType(m_StandByIntr));
        EnableInterruptRouting();
    }

    CHECK_RC(Initialize(Trigger::STANDBY_RESUME));
    JsGpuSubdevice::Resume(this);

    Display *pDisplay = GetParentDevice()->GetDisplay();
    if (pDisplay)
    {
        // This is needed to reinitialize DisplayPortMgr:
        if (pDisplay->GetDisplayClassFamily() == Display::LWDISPLAY)
            CHECK_RC(pDisplay->GetDisplayMgr()->Activate());
        else
            CHECK_RC(pDisplay->AllocGpuResources());
    }

    CHECK_RC(GetParentDevice()->GetUserdManager()->Alloc());

    CHECK_RC(AllocClientStuff(ResetControls::DoResume));

    if (Perf::ILWALID_PSTATE != m_LwrPerfPoint.PStateNum && GetPerf()->HasPStates())
    {
        CHECK_RC(m_pPerf->SetPerfPoint(m_LwrPerfPoint));
    }

    // Re-enable the current console after GPU resumes, if applicable
    CHECK_RC(ConsoleManager::Instance()->ResumeFromSuspend(GetParentDevice()));

    return OK;
}

bool GpuSubdevice::HasIrq(UINT32 irq) const
{
    for (UINT32 i = 0; i < m_Irqs.size(); i++)
    {
        if (m_Irqs[i] == irq)
        {
            return true;
        }
    }
    return false;
}

bool GpuSubdevice::IsHookMsiSupported() const
{
    return (!IsSOC() &&
            GetMsiCapOffset() != 0 &&
            Platform::IRQTypeSupported(MODS_XP_IRQ_TYPE_MSI));
}

bool GpuSubdevice::IsHookMsixSupported() const
{
    return (!IsSOC() &&
            GetMsixCapOffset() != 0 &&
            Platform::IRQTypeSupported(MODS_XP_IRQ_TYPE_MSIX));
}

bool GpuSubdevice::IsHookIntSupported() const
{
    return !IsSOC() && Platform::IRQTypeSupported(MODS_XP_IRQ_TYPE_INT)
#if !defined(SIM_BUILD)
        && (GetIrq() != 0)
#endif
        ;
}

bool GpuSubdevice::IsHookIrqSupported() const
{
#if defined(LINUX_MFG)
    return IsSOC();
#elif defined(MACOSX_MFG)
    return false;
#else
    return true;
#endif
}

//! Colwert IRQ type into ascii string
/* static */ const char *GpuSubdevice::GetIRQTypeName(UINT32 irqType)
{
    switch (irqType)
    {
        case MODS_XP_IRQ_TYPE_INT:
            return "INTx";
        case MODS_XP_IRQ_TYPE_MSI:
            return "MSI";
        case MODS_XP_IRQ_TYPE_CPU:
            return "CPU";
        case MODS_XP_IRQ_TYPE_MSIX:
            return "MSI-X";
        default:
            return "unknown";
    }
}

UINT32 GpuSubdevice::GetNumIRQs(UINT32 irqType)
{
    switch (irqType)
    {
        case MODS_XP_IRQ_TYPE_INT:
            return 1;
        case MODS_XP_IRQ_TYPE_MSI:
            return IsHookMsiSupported() ? 1 : 0;
        case MODS_XP_IRQ_TYPE_MSIX:
            return (IsHookMsixSupported()
                    ? Regs().GetField(m_pPciDevInfo->MsixCapCtrl,
                                      MODS_EP_PCFG_GPU_MSIX_CAP_HEADER_TABLE_SIZE) + 1
                    : 0);
        default:
            Printf(Tee::PriWarn, "Unknown interrupt type %d\n", irqType);
    }

    return 0;
}

RC GpuSubdevice::HookMSI()
{
    RC rc;

    if (!IsHookMsiSupported())
    {
        Printf(Tee::PriError, "System Doesn't Support MSI\n");
        return RC::CANNOT_HOOK_INTERRUPT;
    }

    CHECK_RC(AllocateIRQs(MODS_XP_IRQ_TYPE_MSI));
    MASSERT(m_Irqs.size() == 1);

    IrqParams irqInfo;
    LoadIrqInfo(&irqInfo, MODS_XP_IRQ_TYPE_MSI, m_Irqs[0], 0);
    CHECK_RC(Platform::HookInt(irqInfo, nullptr, nullptr));

#if !defined(MACOSX_MFG)
    // Enable MSI. This is not needed in Mac, kernel does it.
    const auto* pPcie = GetInterface<Pcie>();
    UINT16      msiCtrl;
    CHECK_RC(PciRead16(pPcie, GetMsiCapOffset() + LW_PCI_CAP_MSI_CTRL, &msiCtrl));
    msiCtrl = FLD_SET_DRF(_PCI, _CAP_MSI_CTRL, _ENABLE, _TRUE, msiCtrl);
    CHECK_RC(PciWrite16(pPcie, GetMsiCapOffset() + LW_PCI_CAP_MSI_CTRL, msiCtrl));
#endif

    return OK;
}

RC GpuSubdevice::UnhookMSI()
{
    RC rc;
#if !defined(MACOSX_MFG)
    // Disable MSI. This is not needed in Mac, kernel does it.
    const auto* pPcie = GetInterface<Pcie>();
    UINT16      msiCtrl;
    CHECK_RC(PciRead16(pPcie, GetMsiCapOffset() + LW_PCI_CAP_MSI_CTRL, &msiCtrl));
    msiCtrl = FLD_SET_DRF(_PCI, _CAP_MSI_CTRL, _ENABLE, _FALSE, msiCtrl);
    CHECK_RC(PciWrite16(pPcie, GetMsiCapOffset() + LW_PCI_CAP_MSI_CTRL, msiCtrl));
#endif

    IrqParams irqInfo;
    LoadIrqInfo(&irqInfo, MODS_XP_IRQ_TYPE_MSI, m_Irqs[0], 0);
    CHECK_RC(Platform::UnhookInt(irqInfo, nullptr, nullptr));
    return OK;
}

RC GpuSubdevice::HookMSIX()
{
    RC rc;

    if (!IsHookMsixSupported())
    {
        Printf(Tee::PriError, "System Doesn't Support MSI-X\n");
        return RC::CANNOT_HOOK_INTERRUPT;
    }

    CHECK_RC(AllocateIRQs(MODS_XP_IRQ_TYPE_MSIX));
    MASSERT(m_Irqs.size() > 0);

    IrqParams irqInfo;
    LoadIrqInfo(&irqInfo, MODS_XP_IRQ_TYPE_MSIX, 0, 0);
    for (UINT32 irq: m_Irqs)
    {
        irqInfo.irqNumber = irq;
        CHECK_RC(Platform::HookInt(irqInfo, nullptr, nullptr));
    }

#if !defined(MACOSX_MFG)
    // Enable MSI-X.  We don't know whether this is needed on MacOS or not.
    const UINT32 msixTable = Regs().Read32(MODS_EP_PCFG_GPU_MSIX_CAP_TABLE);
    MASSERT(Regs().TestField(msixTable,
                             MODS_EP_PCFG_GPU_MSIX_CAP_TABLE_BIR_BAR0));
    for (UINT32 ii = 0; ii < m_Irqs.size(); ++ii)
    {
        const UINT32 tableCtl = msixTable + LW_PCI_MSIX_TABLE_CONTROL(ii);
        RegWr32(tableCtl, FLD_SET_DRF(_PCI, _MSIX_TABLE_CONTROL,
                                      _MASK, _ENABLED, RegRd32(tableCtl)));
    }
    Regs().Write32(MODS_EP_PCFG_GPU_MSIX_CAP_HEADER_ENABLE_ENABLED);
#endif
    return OK;
}

RC GpuSubdevice::UnhookMSIX()
{
    RC rc;

#if !defined(MACOSX_MFG)
    // Disable MSI-X.  We don't know whether this is needed on MacOS or not.
    const UINT32 msixTable = Regs().Read32(MODS_EP_PCFG_GPU_MSIX_CAP_TABLE);
    if (Regs().TestField(msixTable, MODS_EP_PCFG_GPU_MSIX_CAP_TABLE_BIR_BAR0))
    {
        Regs().Write32(MODS_EP_PCFG_GPU_MSIX_CAP_HEADER_ENABLE_DISABLED);
        for (UINT32 ii = 0; ii < m_Irqs.size(); ++ii)
        {
            const UINT32 tableCtl = msixTable + LW_PCI_MSIX_TABLE_CONTROL(ii);
            RegWr32(tableCtl, FLD_SET_DRF(_PCI, _MSIX_TABLE_CONTROL,
                                          _MASK, _DISABLED, RegRd32(tableCtl)));
        }
    }
#endif

    IrqParams irqInfo;
    LoadIrqInfo(&irqInfo, MODS_XP_IRQ_TYPE_MSIX, 0, 0);
    for (UINT32 irq: m_Irqs)
    {
        irqInfo.irqNumber = irq;
        Platform::UnhookInt(irqInfo, nullptr, nullptr);
    }
    return OK;
}

void GpuSubdevice::LoadIrqInfo
(
    IrqParams* pIrqInfo,
    UINT08    irqType,
    UINT32    irqNumber,
    UINT32    maskInfoCount
)
{
    const RegHal& regs  = Regs();
    const auto*   pPcie = GetInterface<Pcie>();

    memset(pIrqInfo, 0, sizeof(*pIrqInfo));
    pIrqInfo->irqNumber       = irqNumber;
    pIrqInfo->pciDev.domain   = pPcie->DomainNumber();
    pIrqInfo->pciDev.bus      = pPcie->BusNumber();
    pIrqInfo->pciDev.device   = pPcie->DeviceNumber();
    pIrqInfo->pciDev.function = pPcie->FunctionNumber();
    pIrqInfo->irqType         = irqType;
    pIrqInfo->maskInfoCount   = maskInfoCount;

    if (maskInfoCount)
    {
        pIrqInfo->barAddr     = GetPhysLwBase();
        pIrqInfo->barSize     = LwApertureSize();
    }
    for (UINT32 ii = 0; ii < maskInfoCount; ii++)
    {
        auto pMask = &pIrqInfo->maskInfoList[ii];
        if (regs.IsSupported(MODS_PMC_INTR, ii) &&
            regs.IsSupported(MODS_PMC_INTR_EN, ii))
        {
            pMask->irqPendingOffset = regs.LookupAddress(MODS_PMC_INTR, ii);
            pMask->irqEnabledOffset = regs.LookupAddress(MODS_PMC_INTR_EN, ii);
            pMask->irqEnableOffset  = regs.LookupAddress(MODS_PMC_INTR_EN, ii);
            pMask->irqDisableOffset = regs.LookupAddress(MODS_PMC_INTR_EN, ii);
        }
        else if (regs.IsSupported(MODS_PMC_INTR_x, ii) &&
                 regs.IsSupported(MODS_PMC_INTR_EN_x, ii))
        {
            pMask->irqPendingOffset = regs.LookupAddress(MODS_PMC_INTR_x, ii);
            pMask->irqEnabledOffset = regs.LookupAddress(MODS_PMC_INTR_EN_x, ii);
            pMask->irqEnableOffset  = regs.LookupAddress(MODS_PMC_INTR_EN_x, ii);
            pMask->irqDisableOffset = regs.LookupAddress(MODS_PMC_INTR_EN_x, ii);
        }
        pMask->andMask          = 0;
        pMask->orMask           = 0;
        pMask->maskType         = MODS_XP_MASK_TYPE_IRQ_DISABLE;
    }
}

RC GpuSubdevice::AllocateIRQs(UINT32 irqType)
{
    const UINT32 lwecs = GetNumIRQs(irqType);
    const auto*  pPcie = GetInterface<Pcie>();
    MASSERT(lwecs > 0);

    m_Irqs.resize(lwecs);
    return Platform::AllocateIRQs(pPcie->DomainNumber(),
                                  pPcie->BusNumber(),
                                  pPcie->DeviceNumber(),
                                  pPcie->FunctionNumber(),
                                  lwecs, m_Irqs.data(), irqType);
}

RC GpuSubdevice::HookINTA()
{
    RC rc;

    if (IsSOC())
    {
        // On SOC, enumerate all IRQs for devices handled by RM
        if (m_Irqs.empty())
        {
            CHECK_RC(GetSOCInterrupts(&m_Irqs));
        }
    }
    else
    {
        m_Irqs.resize(1);
        m_Irqs[0] = m_pPciDevInfo->Irq;
    }

    if (IsHookIntSupported())
    {
        IrqParams irqInfo;
        LoadIrqInfo(&irqInfo, MODS_XP_IRQ_TYPE_INT, GetIrq(), GetNumIntrTrees());
        CHECK_RC(Platform::HookInt(irqInfo, nullptr, nullptr));
    }
    else if (IsHookIrqSupported())
    {
        if (m_Irqs.empty())
        {
            Printf(Tee::PriError, "IRQ was not assigned to GPU\n");
            return RC::CANNOT_HOOK_INTERRUPT;
        }
        string hookedIrqsMsg = "Hooked CPU IRQs:";
        for (UINT32 i=0; i < m_Irqs.size(); i++)
        {
            rc = Platform::HookIRQ(m_Irqs[i], nullptr, nullptr);
            if (OK != rc)
            {
                UnhookINTA();
                return rc;
            }
            hookedIrqsMsg += Utility::StrPrintf(" %u", m_Irqs[i]);
        }
        Printf(Tee::PriLow, "%s\n", hookedIrqsMsg.c_str());
    }

    return OK;
}

RC GpuSubdevice::UnhookINTA()
{
    RC rc;
    if (IsHookIntSupported())
    {
        IrqParams irqInfo;
        LoadIrqInfo(&irqInfo, MODS_XP_IRQ_TYPE_INT, GetIrq(), 0);
        CHECK_RC(Platform::UnhookInt(irqInfo, nullptr, nullptr));
    }
    else if (IsHookIrqSupported())
    {
        StickyRC firstRc;
        for (UINT32 irq: m_Irqs)
        {
            firstRc = Platform::UnhookIRQ(irq, nullptr, nullptr);
        }
        CHECK_RC(firstRc);
    }
    return OK;
}

// Thin wrapper around HookInterrupts() that takes a HookedIntrType arg
RC GpuSubdevice::HookInterruptsByType(HookedIntrType intrType)
{
    switch (intrType)
    {
        case HOOKED_NONE:
            return OK;
        case HOOKED_INTA:
            return HookInterrupts(MODS_XP_IRQ_TYPE_INT);
        case HOOKED_MSI:
            return HookInterrupts(MODS_XP_IRQ_TYPE_MSI);
        case HOOKED_MSIX:
            return HookInterrupts(MODS_XP_IRQ_TYPE_MSIX);
    }
    MASSERT(!"Unknown intrType");
    return RC::SOFTWARE_ERROR;
}

RC GpuSubdevice::HookInterrupts(UINT32 irqType)
{
    RC rc;

    if (InterruptsHooked())
    {
        MASSERT(!"Called HookInterrupts with interrupts already hooked");
        return RC::SOFTWARE_ERROR;
    }

    if (!Platform::HasClientSideResman())
    {
        Printf(Tee::PriDebug,
               "Skipping hooking interrupts with kernel side RM\n");
        return OK;
    }

    switch (irqType)
    {
        case MODS_XP_IRQ_TYPE_INT:
            CHECK_RC(HookINTA());
            m_HookedIntr = HOOKED_INTA;
            break;
        case MODS_XP_IRQ_TYPE_MSI:
            CHECK_RC(HookMSI());
            m_HookedIntr = HOOKED_MSI;
            break;
        case MODS_XP_IRQ_TYPE_MSIX:
            CHECK_RC(HookMSIX());
            m_HookedIntr = HOOKED_MSIX;
            break;
        default:
            Printf(Tee::PriError, "Unknown interrupt type %d\n", irqType);
            return RC::UNSUPPORTED_FUNCTION;
    }

    m_pPciDevInfo->InitDone |= MODSRM_INIT_ISR;

    Printf(Tee::PriDebug, "Hooked hardware GPU %s for GPU %u\n",
           GetIRQTypeName(irqType), GetGpuInst());
#if defined(SIM_BUILD)
    Printf(Tee::PriNormal, "Using interrupt type %s (%d)\n",
           GetIRQTypeName(irqType), irqType);
#endif

    return OK;
}

RC GpuSubdevice::UnhookISR()
{
    if (m_HookedIntr == HOOKED_NONE)
    {
        return OK;
    }

    RC rc;

    auto pPcie = GetInterface<Pcie>();

    // Disable HW interrupts
    if ((m_pPciDevInfo->InitDone & MODSRM_INIT_PGPU) != 0)
    {
        Printf(Tee::PriLow, "Disabling interrupts in RM\n");
        RmDisableInterrupts(GetGpuInst());
    }
    for (UINT32 i = 0; i < GetNumIntrTrees(); i++)
    {
        Printf(Tee::PriLow, "Disabling interrupt tree %u\n", i);
        SetIntrEnable(i, intrDisabled);
    }

    // Unhook interrupts
    switch (m_HookedIntr)
    {
        case HOOKED_NONE:
            break;
        case HOOKED_INTA:
            CHECK_RC(UnhookINTA());
            break;
        case HOOKED_MSI:
            CHECK_RC(UnhookMSI());
            break;
        case HOOKED_MSIX:
            CHECK_RC(UnhookMSIX());
            break;
    }

    Platform::FreeIRQs(pPcie->DomainNumber(), pPcie->BusNumber(),
                       pPcie->DeviceNumber(), pPcie->FunctionNumber());
    m_pPciDevInfo->InitDone &= ~MODSRM_INIT_ISR;
    m_HookedIntr = HOOKED_NONE;
    return OK;
}

//-----------------------------------------------------------------------------------------------
void GpuSubdevice::RearmMSI()
{
    if (HasBug(998963))
    {
        // From https://p4viewer.lwpu.com/get/hw/doc/gpu/maxwell/gm204/gen_manuals/dev_lw_xve.ref
        // "CPU should issue a write (value doesn't matter) to
        // LW_XVE_MSI_CTRL_CAP_ID (7;0) with byte enable write_be_= 4'b1110."
        // Bug 998963: PCI config cycles wont re-arm the MSI interrupts.
        const ModsGpuRegAddress capReg = MODS_EP_PCFG_GPU_MSI_64_HEADER;
        const RegHalDomain domain = Regs().LookupDomain(capReg);
        const UINT32 addr = (Regs().LookupAddress(capReg) +
                             GetDomainBase(0, domain, LwRmPtr().Get()));
        const UINT08 EOI = 0xFF;
        RegWr08(addr, EOI);
    }
}

void GpuSubdevice::RearmMSIX()
{
    if (HasBug(2897081))
    {
        const UINT08 EOI = 0xFF;
        auto pPcie = GetInterface<Pcie>();
        Platform::PciWrite08(pPcie->DomainNumber(),
                pPcie->BusNumber(), pPcie->DeviceNumber(),
                pPcie->FunctionNumber(),
                GetMsixCapOffset(),
                EOI);
    }
}

UINT32 GpuSubdevice::GetIntrStatus(unsigned intrTree) const
{
    MASSERT(intrTree < GetNumIntrTrees());
    const bool postKepler = Regs().IsSupported(MODS_PMC_INTR, intrTree);
    return Regs().Read32(postKepler ? MODS_PMC_INTR : MODS_PMC_INTR_x,
                         intrTree);
}

bool GpuSubdevice::GetSWIntrStatus(unsigned intrTree) const
{
    MASSERT(intrTree < GetNumIntrTrees());
    const bool postKepler = Regs().IsSupported(MODS_PMC_INTR, intrTree);
    return Regs().Read32(postKepler
                         ? MODS_PMC_INTR_SOFTWARE
                         : MODS_PMC_INTR_x_SOFTWARE,
                         intrTree) != 0;
}

namespace
{
    UINT32 GetIrqTimeout()
    {
        static UINT32 irqTimeout = 10000; // Default to 10 seconds
        static bool initialized = false;

        if (initialized)
        {
            return irqTimeout;
        }

        // On Silicon the interrupt should come really quickly,
        // so set 100ms timeout.
        if (Platform::GetSimulationMode() == Platform::Hardware)
        {
            // This is poor man's way of distinguishing between Silicon
            // and emulation. We do this since m_IsEmulation has
            // not been initialized yet.
            UINT32 rmTimeoutMs = 0;
            Registry::Read("ResourceManager", "RmDefaultTimeout", &rmTimeoutMs);
            if (rmTimeoutMs < 60000)
            {
                irqTimeout = 100;
            }
        }

        initialized = true;
        return irqTimeout;
    }
}

void GpuSubdevice::SetSWIntrPending
(
    unsigned intrTree,
    bool pending
)
{
    MASSERT(intrTree < GetNumIntrTrees());
    UINT32 value = 0;
    const bool postKepler = Regs().IsSupported(MODS_PMC_INTR);
    const ModsGpuRegAddress modsPmcIntr = (postKepler
                                           ? MODS_PMC_INTR
                                           : MODS_PMC_INTR_x);
    if (pending)
    {
        Regs().SetField(&value, (postKepler
                                 ? MODS_PMC_INTR_SOFTWARE_PENDING
                                 : MODS_PMC_INTR_x_SOFTWARE_PENDING));
    }
    Regs().Write32(modsPmcIntr, intrTree, value);

    // Re-read the register to make sure the write went through (esp. on CheetAh)
    Regs().Read32(modsPmcIntr, intrTree);
}

void GpuSubdevice::SetIntrEnable
(
    unsigned intrTree,
    IntrEnable state
)
{
    MASSERT(intrTree < GetNumIntrTrees());
    const bool postKepler = Regs().IsSupported(MODS_PMC_INTR);
    switch (state)
    {
        case intrDisabled:
            Regs().Write32(postKepler
                           ? MODS_PMC_INTR_EN_INTA_DISABLED
                           : MODS_PMC_INTR_EN_x_INTA_DISABLED,
                           intrTree);
            break;

        case intrHardware:
            Regs().Write32(postKepler
                           ? MODS_PMC_INTR_EN_INTA_HARDWARE
                           : MODS_PMC_INTR_EN_x_INTA_HARDWARE,
                           intrTree);
            break;

        case intrSoftware:
            Regs().Write32(postKepler
                           ? MODS_PMC_INTR_EN_INTA_SOFTWARE
                           : MODS_PMC_INTR_EN_x_INTA_SOFTWARE,
                           intrTree);
            break;

        default:
            MASSERT(!"Invalid state");
            break;
    }
}

/* virtual */ unsigned GpuSubdevice::GetNumIntrTrees() const
{
    return 2;
}

RC GpuSubdevice::ServiceInterrupt(UINT32 irq)
{
    RC rc;
    if (m_pValidationEvent.load())
    {
        Tasker::MutexHolder mh(m_ValidationMutex);

        // Check if the GPU really has a pending interrupt
        if (!GetIntrStatus(m_ValidatedTree))
        {
            // This is common on CheetAh
            Printf(Tee::PriLow, "Spurious interrupt detected\n");
            return OK;
        }

        // Disable software interrupt
        SetSWIntrPending(m_ValidatedTree, false);

        // Rearm MSI without RM
        if (m_HookedIntr == HOOKED_MSI)
        {
            RearmMSI();
        }
        else if (m_HookedIntr == HOOKED_MSIX)
        {
            RearmMSIX();
        }

        // Notify the thread which is validating interrupts that the interrupt
        // has been received
        if (m_pValidationEvent.load())
            Tasker::SetEvent(m_pValidationEvent.load());
    }
    else if (IsSOC())
    {
        rc = RmIsrSoc(GetGpuInst(), irq) ? OK : RC::UNEXPECTED_INTERRUPTS;
    }
    else
    {
        rc = RmIsr(GetGpuInst()) ? OK : RC::UNEXPECTED_INTERRUPTS;
        if (m_HookedIntr == HOOKED_MSI || m_HookedIntr == HOOKED_MSIX)
        {
            RmRearmMsi(GetGpuInst());
        }
    }
    return rc;
}

RC GpuSubdevice::ValidateInterrupts(UINT32 interruptMask)
{
    if (!Platform::HasClientSideResman())
    {
        Printf(Tee::PriDebug,
               "Skipping interrupt validation with kernel side RM\n");
        return OK;
    }

    SCOPED_DEV_INST(this);

    if (IsSOC())
    {
        return ValidateSOCInterrupts();
    }

    auto pPcie = GetInterface<Pcie>();

    Printf(Tee::PriDebug,
           "ValidateInterrupts for GPU %u - PCI device %04x:%x:%02x.%x\n",
            GetGpuInst(), pPcie->DomainNumber(), pPcie->BusNumber(),
            pPcie->DeviceNumber(), pPcie->FunctionNumber());

    // Make sure PMU is enabled for this GPU
    EnableInterruptRouting();

    // Make sure all interrupts are disabled
    for (UINT32 i=0; i < GetNumIntrTrees(); i++)
    {
        SetIntrEnable(i, intrDisabled);
    }

    // Clear any stuck interrupts
    RC rc;
    CHECK_RC(ClearStuckInterrupts());

    // Validate INTA interrupts
    if (interruptMask & HOOKED_INTA)
    {
        // Make sure the interrupt line has been assigned
        if (m_pPciDevInfo->Irq == 0)
        {
            if (!IsSOC())
            {
#if defined(LINUX_MFG)
                Printf(Tee::PriLow,
                        "This system does not support legacy interrupts "
                        "for GPU %u - PCI device %04x:%x:%02x.%x\n",
                        GetGpuInst(), pPcie->DomainNumber(), pPcie->BusNumber(),
                        pPcie->DeviceNumber(), pPcie->FunctionNumber());
#else
                Printf(Tee::PriNormal,
                        "The GPU was assigned IRQ %u. This is not valid.\n",
                        m_pPciDevInfo->Irq);
                return RC::ILWALID_IRQ;
#endif
            }
            else
            {
                MASSERT(!"GPU interrupts missing in project relocation table!");
                Printf(Tee::PriNormal,
                        "GPU does not have any interrupts.\n");
                return RC::SOFTWARE_ERROR;
            }
        }

        CHECK_RC(ValidateInterrupt(HOOKED_INTA, 0));

        Printf(Tee::PriLow, "Legacy interrupts verified OK\n");
    }
    else
    {
        Printf(Tee::PriLow, "Skipping legacy interrupts verification\n");
    }

    // Validate MSI interrupts, not available on SOC GPUs
    if (!IsSOC() && (interruptMask & HOOKED_MSI))
    {
        // Check if MSI is supported at all
        if (!GetMsiCapOffset())
        {
            Printf(Tee::PriNormal, "This system does not support MSI\n");
            return RC::CANNOT_HOOK_INTERRUPT;
        }

        // Rearm MSI in case it wasn't rearmed in the previous MODS run
        // or is stuck in some other way.
        RearmMSI();

        CHECK_RC(ValidateInterrupt(HOOKED_MSI, 0));

        Printf(Tee::PriLow, "MSI verified OK\n");
    }
    else
    {
        Printf(Tee::PriLow, "Skipping MSI verification\n");
    }

    // Validate MSI-X interrupts, not available on SOC GPUs
    if (!IsSOC() && (interruptMask & HOOKED_MSIX))
    {
        // Check if MSI-X is supported at all
        if (!GetMsixCapOffset())
        {
            Printf(Tee::PriNormal, "This system does not support MSI-X\n");
            return RC::CANNOT_HOOK_INTERRUPT;
        }

        // Rearm MSI in case it wasn't rearmed in the previous MODS run
        // or is stuck in some other way.
        RearmMSIX();

        CHECK_RC(ValidateInterrupt(HOOKED_MSIX, 0));

        Printf(Tee::PriLow, "MSI-X verified OK\n");
    }
    else
    {
        Printf(Tee::PriLow, "Skipping MSI-X verification\n");
    }
    return OK;
}

RC GpuSubdevice::ValidateSOCInterrupts()
{
    MASSERT(m_Irqs.empty());

    CheetAh::DeviceDesc deviceDesc;
    RC rc;
    CHECK_RC(GetDeviceDesc(GetSocDeviceIndex(), &deviceDesc));

    // Aurora GPU does not have interrupts
    if (deviceDesc.ints.empty())
    {
        Printf(Tee::PriDebug, "SOC GPU %u does not have any interrupts, "
                "skipping validation\n", GetGpuInst());
        return OK;
    }

    Printf(Tee::PriDebug, "ValidateInterrupts for SOC GPU %u\n", GetGpuInst());

    // Lwrrently we assume an SOC GPU has 2 interrupts
    if (deviceDesc.ints.size() == 3) // Ignore GPMU interrupt on GP10B
    {
        deviceDesc.ints.resize(2);
    }
    MASSERT(deviceDesc.ints.size() == 2);

    // Make sure PMU is enabled for this GPU
    EnableInterruptRouting();

    // Make sure all interrupts are disabled
    for (UINT32 i = 0; i < GetNumIntrTrees(); i++)
    {
        SetIntrEnable(i, intrDisabled);
    }

    if (HasPendingInterrupt())
    {
        Printf(Tee::PriError, "GPU %u's interrupts are stuck\n", GetGpuInst());
        return RC::IRQ_STUCK_ASSERTED;
    }

    for (UINT32 i=0; i < deviceDesc.ints.size(); i++)
    {
        if (deviceDesc.ints[i].target != CheetAh::INT_TARGET_AVP)
        {
            UINT32 cpuIrq = ~0U;
            CHECK_RC(CheetAh::GetCpuIrq(deviceDesc.ints[i], &cpuIrq));
            m_Irqs.push_back(cpuIrq);
            DEFER
            {
                this->m_Irqs.clear();
            };
            CHECK_RC(ValidateInterrupt(HOOKED_INTA, i));
        }
    }
    return rc;
}

RC GpuSubdevice::ValidateInterrupt(HookedIntrType intrType, unsigned intrTree)
{
    // Hook interrupt
    StickyRC rc;

    CHECK_RC(HookInterruptsByType(intrType));

    DEFER
    {
        this->UnhookISR();
    };

    {
        Tasker::MutexHolder mh(m_ValidationMutex);
        m_ValidatedTree = intrTree;
        m_pValidationEvent.store(Tasker::AllocEvent("Validated interrupt", false));
        MASSERT(m_pValidationEvent.load());
    }
    DEFER
    {
        Tasker::MutexHolder mh(this->m_ValidationMutex);
        Tasker::FreeEvent(this->m_pValidationEvent.load());
        this->m_pValidationEvent.store(nullptr);
    };

    const UINT32 numInterruptsToCheck = 10;
    const UINT32 irqTimeout = GetIrqTimeout();
    auto pPcie = GetInterface<Pcie>();

    UINT32 intCount = 0;
    for (UINT32 i=0; i < numInterruptsToCheck; i++)
    {
        // Enable SW interrupt
        SetIntrEnable(intrTree, intrSoftware);

        // Trigger interrupt in HW
        SetSWIntrPending(intrTree, true);

        // Wait for the interrupt
        const bool irqReceived = (OK == Tasker::WaitOnEvent(m_pValidationEvent.load(),
                                                            irqTimeout));

        // If the interrupt triggered - we're happy
        if (irqReceived)
        {
            intCount++;
        }

        // If the software interrupt is still pending, something went wrong.
        if (GetSWIntrStatus(intrTree))
        {
            // Only print this string the first time this event happens...let's
            // not spew the user
            if (OK == rc)
            {
                if (m_HookedIntr == HOOKED_INTA)
                {
                    Printf(Tee::PriNormal,
                            "Timed out waiting for IRQ %u (iteration %u)\n",
                            m_Irqs[0],
                            i);
                }
                else if (m_HookedIntr == HOOKED_MSI)
                {
                    Printf(Tee::PriNormal,
                           "Timed out waiting for MSI for PCI device"
                           " %04x:%x:%02x.%x (iteration %u)\n",
                           pPcie->DomainNumber(), pPcie->BusNumber(),
                           pPcie->DeviceNumber(), pPcie->FunctionNumber(),
                           i);
                }
                else if (m_HookedIntr == HOOKED_MSIX)
                {
                    Printf(Tee::PriNormal,
                           "Timed out waiting for MSI-X for PCI device"
                           " %04x:%x:%02x.%x (iteration %u)\n",
                           pPcie->DomainNumber(), pPcie->BusNumber(),
                           pPcie->DeviceNumber(), pPcie->FunctionNumber(),
                           i);
                }
            }
            rc = RC::IRQ_CANNOT_ASSERT;
        }

        // Make sure the interrupt gets shut off
        SetSWIntrPending(intrTree, false);
    }

    // Wait for any pending interrupts to clear out.
    // Needed on CheetAh due to some sort of race resulting from slow
    // interrupt propagation.
    if (IsSOC())
    {
        // We probably need under 100us to make sure there is no
        // spurious pending interrupt, but let's do 1ms just to be on
        // the safe side.
        Tasker::Sleep(1);
    }

    if (intCount != numInterruptsToCheck)
    {
        if (m_HookedIntr == HOOKED_INTA)
        {
            Printf(Tee::PriNormal,
                   "Interrupt mechanism for IRQ %u is not working.\n"
                   "%u out of %u interrupts got through.\n",
                   m_Irqs[0],
                   intCount,
                   numInterruptsToCheck);
        }
        else if (m_HookedIntr == HOOKED_MSI || m_HookedIntr == HOOKED_MSIX)
        {
            const auto type = m_HookedIntr == HOOKED_MSI ? "MSI" : "MSI-X";
            Printf(Tee::PriNormal,
                   "%s mechanism for PCI device %04x:%x:%02x.%x is not working.\n"
                   "%u out of %u interrupts got through.\n",
                   type,
                   pPcie->DomainNumber(), pPcie->BusNumber(),
                   pPcie->DeviceNumber(), pPcie->FunctionNumber(),
                   intCount,
                   numInterruptsToCheck);
        }

        rc = RC::IRQ_CANNOT_ASSERT;
    }

    // Disable interrupts
    SetSWIntrPending(intrTree, false);
    SetIntrEnable(intrTree, intrDisabled);

    return rc;
}

void GpuSubdevice::ResetExceptionsDuringShutdown(bool bEnable)
{
    m_ResetExceptions = bEnable;
}

//! Load up the bus info for this subdevice
RC GpuSubdevice::LoadBusInfo()
{
    if (m_BusInfoLoaded)
        return OK;

    RC rc = OK;

    LW2080_CTRL_BUS_GET_INFO_PARAMS params = {0};

    params.busInfoListSize = sizeof (m_BusInfo) / sizeof (LW2080_CTRL_BUS_INFO);
    params.busInfoList = LW_PTR_TO_LwP64(&m_BusInfo);
    m_BusInfo.caps.index = LW2080_CTRL_BUS_INFO_INDEX_CAPS;
    m_BusInfo.bustype.index = LW2080_CTRL_BUS_INFO_INDEX_TYPE;
    m_BusInfo.noncoherent.index =
        LW2080_CTRL_BUS_INFO_INDEX_NONCOHERENT_DMA_FLAGS;
    m_BusInfo.coherent.index = LW2080_CTRL_BUS_INFO_INDEX_COHERENT_DMA_FLAGS;
    m_BusInfo.gpugartsize.index = LW2080_CTRL_BUS_INFO_INDEX_GPU_GART_SIZE;
    m_BusInfo.gpugartsizehi.index = LW2080_CTRL_BUS_INFO_INDEX_GPU_GART_SIZE_HI;
    m_BusInfo.gpugartflags.index = LW2080_CTRL_BUS_INFO_INDEX_GPU_GART_FLAGS;

    rc = LwRmPtr()->ControlBySubdevice(this,
                                       LW2080_CTRL_CMD_BUS_GET_INFO,
                                       &params, sizeof(params));

    if (rc != OK)
    {
        Printf(Tee::PriError,
               "ERROR LOADING BUS INFO! RmControl call failed! RC = %d\n",
               (UINT32)rc);
        memset(&m_BusInfo, 0, sizeof (m_BusInfo));
#ifndef SEQ_FAIL
        MASSERT(!"Failed to load bus info through RmControl");
#endif
        return rc;
    }

    m_BusInfoLoaded = true;
    return OK;
}

bool GpuSubdevice::HasVbiosModsFlagA()
{
    UINT32 data = 0;
    RC rc = GetBiosInfo(LW2080_CTRL_BIOS_INFO_INDEX_STRICT_SKU_FLAG, &data);
    if (OK != rc)
    {
        Printf(Tee::PriLow,
               "LwRm::Control failed in GpuSubdevice::HasVbiosModsFlagA\n");
        data = 0;
    }
    return (0 != data);
}

bool GpuSubdevice::HasVbiosModsFlagB()
{
    // Flag A chips also get Flag B set, but we don't want this
    // check to include Flag A chips
    if (HasVbiosModsFlagA())
    {
        return false;
    }

    // CheetAh does not support LW2080_CTRL_CMD_GR_GET_CAPS_V2 as of now
    // Use old control call for it
    if (IsSOC() && Platform::UsesLwgpuDriver())
    {
        LW0080_CTRL_GR_GET_CAPS_PARAMS params = { 0 };

        if (OK != LwRmPtr()->ControlBySubdevice(this, LW0080_CTRL_CMD_GR_GET_CAPS,
                                                &params, sizeof(params)))
        {
            Printf(Tee::PriWarn, "RmControl failed getting VBIOS MODS Flag B!\n");
            return false;
        }

        return LW0080_CTRL_GR_GET_CAP(reinterpret_cast<LwU8*>(params.capsTbl),
                                      LW0080_CTRL_GR_CAPS_QUADRO_GENERIC) != 0;
    }
    else
    {
        LW2080_CTRL_GR_GET_CAPS_V2_PARAMS params = { };

        if (IsSMCEnabled())
        {
            SetGrRouteInfo(&(params.grRouteInfo), GetSmcEngineIdx());
        }
        if (OK != LwRmPtr()->ControlBySubdevice(this, LW2080_CTRL_CMD_GR_GET_CAPS_V2,
                                                &params, sizeof(params)))
        {
            Printf(Tee::PriWarn, "RmControl failed getting VBIOS MODS Flag B!\n");
            return false;
        }

        return LW0080_CTRL_GR_GET_CAP(params.capsTbl,
                                     LW0080_CTRL_GR_CAPS_QUADRO_GENERIC) != 0;
    }
}

bool GpuSubdevice::IsMXMBoard()
{
    return (0 != MxmVersion());
}

UINT32 GpuSubdevice::MxmVersion()
{
    UINT32 data = 0;
    RC rc = GetBiosInfo(LW2080_CTRL_BIOS_INFO_INDEX_MXM_VERSION, &data);
    if (OK != rc)
    {
        Printf(Tee::PriLow,
               "LwRm::Control failed in GpuSubdevice::MxmVersion\n");
        data = 0;
    }
    return data;
}

UINT32 GpuSubdevice::VbiosRevision()
{
    UINT32 data = 0;
    RC rc = GetBiosInfo(LW2080_CTRL_BIOS_INFO_INDEX_REVISION, &data);
    if (OK != rc)
    {
        Printf(Tee::PriLow,
               "LwRm::Control failed in GpuSubdevice::VbiosRevision\n");
        data = 0;
    }
    return data;
}

UINT32 GpuSubdevice::VbiosOEMRevision()
{
    UINT32 data = 0;
    RC rc = GetBiosInfo(LW2080_CTRL_BIOS_INFO_INDEX_OEM_REVISION, &data);
    if (OK != rc)
    {
        Printf(Tee::PriLow,
               "LwRm::Control failed in GpuSubdevice::VbiosOEMRevision\n");
        data = 0;
    }
    return data;
}

RC GpuSubdevice::GetBiosInfo(UINT32 infoIndex, UINT32 *pVersion)
{
    LW2080_CTRL_BIOS_INFO biosInfo[1];
    biosInfo[0].index = infoIndex;
    biosInfo[0].data = 0;

    LW2080_CTRL_BIOS_GET_INFO_PARAMS biosInfoParams = {0};
    biosInfoParams.biosInfoListSize = 1;
    biosInfoParams.biosInfoList = LW_PTR_TO_LwP64(&biosInfo);
    RC rc;
    CHECK_RC(LwRmPtr()->ControlBySubdevice(this,
                                           LW2080_CTRL_CMD_BIOS_GET_INFO,
                                           &biosInfoParams,
                                           sizeof(biosInfoParams)));
    MASSERT(pVersion);
    *pVersion = biosInfo[0].data;
    return rc;
}

string GpuSubdevice::RomTypeToString(UINT32 type)
{
    switch (type)
    {
        case LW2080_CTRL_BIOS_INFO_SELWRITY_TYPE_LWIDIA_RELEASE:
            return "LWPU Authenticated";
        case LW2080_CTRL_BIOS_INFO_SELWRITY_TYPE_PARTNER:
            return "Partner Production";
        case LW2080_CTRL_BIOS_INFO_SELWRITY_TYPE_ILWALID:
            return "Invalid";
        case LW2080_CTRL_BIOS_INFO_SELWRITY_TYPE_UNSIGNED:
            return "Unsigned";
        case LW2080_CTRL_BIOS_INFO_SELWRITY_TYPE_LWIDIA_DEBUG:
            return "LWPU Debug";
        case LW2080_CTRL_BIOS_INFO_SELWRITY_TYPE_PARTNER_DEBUG:
            return "Partner Debug";
        case LW2080_CTRL_BIOS_INFO_SELWRITY_TYPE_LWIDIA_AE_DEBUG:
            return "LWPU AE Debug";
        default:
            MASSERT(!"Unknown VBIOS type");
            break;
    }
    return "***Unknown***";
}

//! Check for whether or not a HULK license is flashed on the InfoROM
RC GpuSubdevice::GetIsHulkFlashed(bool* pHulkFlashed)
{
    return RC::UNSUPPORTED_FUNCTION;
}

RC GpuSubdevice::EndOfLoopCheck(bool captureReference)
{
    StickyRC combinedRC;

    combinedRC = TestDevice::EndOfLoopCheck(captureReference);

    LW2080_CTRL_FB_INFO fbInfo = { LW2080_CTRL_FB_INFO_INDEX_HEAP_FREE, 0 };
    LW2080_CTRL_FB_GET_INFO_PARAMS params = { 1, LW_PTR_TO_LwP64(&fbInfo) };
    RC fbRC = LwRmPtr()->ControlBySubdevice(this,
        LW2080_CTRL_CMD_FB_GET_INFO, &params, sizeof(params));

    combinedRC = fbRC;

    if (fbRC == RC::OK)
    {
        Printf(Tee::PriLow,
            "    FbHeapFreeKB    = %u\n",
            fbInfo.data);

        if (captureReference)
        {
            m_FbHeapFreeKBReference = fbInfo.data;
        }
        else
        {
            if (fbInfo.data < m_FbHeapFreeKBReference)
            {
                Printf(Tee::PriError,
                    "FbHeapFreeKB = %u < %u\n",
                    fbInfo.data, m_FbHeapFreeKBReference);
                combinedRC = RC::MEMORY_LEAK;
            }
        }
    }

    return combinedRC;
}

//! Check the bus info for the bus type
UINT32 GpuSubdevice::BusType()
{
    if (!m_BusInfoLoaded)
        LoadBusInfo();
    if (!m_BusInfoLoaded)
        return 0; // I'll be explicit

    return m_BusInfo.bustype.data;
}

//! Check the bus info if we support NC
bool GpuSubdevice::SupportsNonCoherent()
{
    if (!m_BusInfoLoaded)
        LoadBusInfo();
    if (!m_BusInfoLoaded)
        return false;

    return m_BusInfo.noncoherent.data != 0;
}

//! Check the bus info for the GPU GART size
UINT64 GpuSubdevice::GpuGartSize()
{
    if (!m_BusInfoLoaded)
        LoadBusInfo();
    if (!m_BusInfoLoaded)
        return 0;

    return m_BusInfo.gpugartsize.data | ((LwU64)m_BusInfo.gpugartsizehi.data << 32);
}

//! Return the GPU GART flags
UINT32 GpuSubdevice::GpuGartFlags()
{
    if (!m_BusInfoLoaded)
        LoadBusInfo();
    if (!m_BusInfoLoaded)
        return 0;

    return m_BusInfo.gpugartflags.data;
}

//! Return the noncoherent flags
UINT32 GpuSubdevice::NCFlags()
{
    if (!m_BusInfoLoaded)
        LoadBusInfo();
    if (!m_BusInfoLoaded)
        return 0;

    return m_BusInfo.noncoherent.data;
}

//! Return the coherent flags
UINT32 GpuSubdevice::CFlags()
{
    if (!m_BusInfoLoaded)
        LoadBusInfo();
    if (!m_BusInfoLoaded)
        return 0;

    return m_BusInfo.coherent.data;
}

//! Load up the GPU info for this subdevice
RC GpuSubdevice::LoadGpuInfo()
{
    if (m_GpuInfoLoaded)
        return OK;
    RC rc;
    memset(&m_GpuInfo, 0, sizeof (m_GpuInfo));
    m_GpuInfo.ecidLo32.index = LW2080_CTRL_GPU_INFO_INDEX_ECID_LO32;
    m_GpuInfo.ecidHi32.index = LW2080_CTRL_GPU_INFO_INDEX_ECID_HI32;
    m_GpuInfo.extended.index = LW2080_CTRL_GPU_INFO_INDEX_ECID_EXTENDED;
    m_GpuInfo.extMinorRevision.index =
                                LW2080_CTRL_GPU_INFO_INDEX_MINOR_REVISION_EXT;
    m_GpuInfo.netlistRevision0.index = LW2080_CTRL_GPU_INFO_INDEX_NETLIST_REV0;
    m_GpuInfo.netlistRevision1.index = LW2080_CTRL_GPU_INFO_INDEX_NETLIST_REV1;
    m_GpuInfo.sampleType.index = LW2080_CTRL_GPU_INFO_INDEX_SAMPLE;
    m_GpuInfo.hwQualType.index = LW2080_CTRL_GPU_INFO_INDEX_HW_QUAL_TYPE;

    UINT32 gpuInfoSize = sizeof (m_GpuInfo) / sizeof (LW2080_CTRL_GPU_INFO);
    for (UINT32 i = 0; i < gpuInfoSize; ++i)
    {
        LW2080_CTRL_GPU_INFO *pGpuInfo = reinterpret_cast<LW2080_CTRL_GPU_INFO *>(&m_GpuInfo) + i;
        LW2080_CTRL_GPU_GET_INFO_V2_PARAMS params = {0};
        params.gpuInfoListSize = 1;
        params.gpuInfoList[0].index = pGpuInfo->index;
        RC subRc = LwRmPtr()->ControlBySubdevice(
                this, LW2080_CTRL_CMD_GPU_GET_INFO_V2, &params, sizeof(params));
        if (subRc == OK)
        {
            *((bool *)(&m_GpuInfoValid) + i) = true;
            pGpuInfo->data = params.gpuInfoList[0].data;
        }
        else
        {
            *((bool *)(&m_GpuInfoValid) + i) = false;
            rc = subRc;
        }
    }

    if (rc != OK)
    {
        Printf(Tee::PriLow,
               "Some RmControl call failed during getting Gpu info! RC = %d\n",
               (UINT32)rc);
    }

    m_GpuInfoLoaded = true;
    return rc;
}

//! \brief Return the Chip ID (ECID)
//!
//! See https://gpuhwtesla/index.php/Tesla_G80#ECID_Encoding and bug
//! 304974 for ECID documentation.
//!
RC GpuSubdevice::ChipId(vector<UINT32>* pEcids)
{
    MASSERT(pEcids);
    pEcids->clear();

    if (!m_GpuInfoLoaded)
        LoadGpuInfo();

    if (!m_GpuInfoValid.ecidLo32Valid ||
        !m_GpuInfoValid.ecidHi32Valid ||
        !m_GpuInfoValid.extendedValid)
    {
        pEcids->push_back(0);
    }
    else
    {
        pEcids->push_back(m_GpuInfo.extended.data);
        pEcids->push_back(m_GpuInfo.ecidHi32.data);
        pEcids->push_back(m_GpuInfo.ecidLo32.data);
    }
    return OK;
}

UINT32 GpuSubdevice::GetPLLCfg(PLLType pll) const
{
    switch (pll)
    {
        case SP_PLL0:
            return Regs().Read32(MODS_PVTRIM_SYS_SPPLL0_CFG);
        case SP_PLL1:
            return Regs().Read32(MODS_PVTRIM_SYS_SPPLL1_CFG);
        case SYS_PLL:
            return Regs().Read32(MODS_PTRIM_SYS_SYSPLL_CFG);
        case GPC_PLL:
            return Regs().Read32(MODS_PTRIM_SYS_GPCPLL_CFG);
        case LTC_PLL:
            return Regs().Read32(MODS_PTRIM_SYS_LTCPLL_CFG);
        case XBAR_PLL:
            return Regs().Read32(MODS_PTRIM_SYS_XBARPLL_CFG);
        default:
            MASSERT(!"Invalid PLL type");
    }

    return 0;
}

void GpuSubdevice::SetPLLCfg(PLLType pll, UINT32 cfg)
{
    switch (pll)
    {
        case SP_PLL0:
            Regs().Write32(MODS_PVTRIM_SYS_SPPLL0_CFG, cfg);
            break;
        case SP_PLL1:
            Regs().Write32(MODS_PVTRIM_SYS_SPPLL1_CFG, cfg);
            break;
        case SYS_PLL:
            Regs().Write32(MODS_PTRIM_SYS_SYSPLL_CFG, cfg);
            break;
        case GPC_PLL:
            Regs().Write32(MODS_PTRIM_SYS_GPCPLL_CFG, cfg);
            break;
        case LTC_PLL:
            Regs().Write32(MODS_PTRIM_SYS_LTCPLL_CFG, cfg);
            break;
        case XBAR_PLL:
            Regs().Write32(MODS_PTRIM_SYS_XBARPLL_CFG, cfg);
            break;
        default:
            MASSERT(!"Invalid PLL type");
    }
}

void GpuSubdevice::SetPLLLocked(PLLType pll)
{
    UINT32 Reg = GetPLLCfg(pll);

    switch (pll)
    {
        case SP_PLL0:
            Regs().SetField(&Reg, MODS_PVTRIM_SYS_SPPLL0_CFG_ENB_LCKDET_POWER_ON);
            break;
        case SP_PLL1:
            Regs().SetField(&Reg, MODS_PVTRIM_SYS_SPPLL1_CFG_ENB_LCKDET_POWER_ON);
            break;
        case SYS_PLL:
            Regs().SetField(&Reg, MODS_PTRIM_SYS_SYSPLL_CFG_ENB_LCKDET_POWER_ON);
            break;
        case GPC_PLL:
            Regs().SetField(&Reg, MODS_PTRIM_SYS_GPCPLL_CFG_ENB_LCKDET_POWER_ON);
            break;
        case LTC_PLL:
            Regs().SetField(&Reg, MODS_PTRIM_SYS_LTCPLL_CFG_ENB_LCKDET_POWER_ON);
            break;
        case XBAR_PLL:
            Regs().SetField(&Reg, MODS_PTRIM_SYS_XBARPLL_CFG_ENB_LCKDET_POWER_ON);
            break;
        default:
            MASSERT(!"Invalid PLL type.");
    }

    SetPLLCfg(pll, Reg);
}

bool GpuSubdevice::IsPLLLocked(PLLType pll) const
{
    UINT32 Reg = GetPLLCfg(pll);

    switch (pll)
    {
        case SP_PLL0:
            return Regs().TestField(Reg, MODS_PVTRIM_SYS_SPPLL0_CFG_PLL_LOCK_TRUE);
            break;
        case SP_PLL1:
            return Regs().TestField(Reg, MODS_PVTRIM_SYS_SPPLL1_CFG_PLL_LOCK_TRUE);
            break;
        case SYS_PLL:
            return Regs().TestField(Reg, MODS_PTRIM_SYS_SYSPLL_CFG_PLL_LOCK_TRUE);
            break;
        case GPC_PLL:
            return Regs().TestField(Reg, MODS_PTRIM_SYS_GPCPLL_CFG_PLL_LOCK_TRUE);
            break;
        case LTC_PLL:
            return Regs().TestField(Reg, MODS_PTRIM_SYS_LTCPLL_CFG_PLL_LOCK_TRUE);
            break;
        case XBAR_PLL:
            return Regs().TestField(Reg, MODS_PTRIM_SYS_XBARPLL_CFG_PLL_LOCK_TRUE);
            break;
        default:
            MASSERT(!"Invalid PLL type.");
    }

    return false;
}

bool GpuSubdevice::IsPLLEnabled(PLLType pll) const
{
    switch (pll)
    {
        case SP_PLL0:
            return Regs().Test32(MODS_PVTRIM_SYS_SPPLL0_CFG_ENABLE_YES);
            break;
        case SP_PLL1:
            return Regs().Test32(MODS_PVTRIM_SYS_SPPLL1_CFG_ENABLE_YES);
            break;
        case SYS_PLL:
            return Regs().Test32(MODS_PTRIM_SYS_SYSPLL_CFG_ENABLE_YES);
            break;
        case GPC_PLL:
            return Regs().Test32(MODS_PTRIM_SYS_GPCPLL_CFG_ENABLE_YES);
            break;
        case LTC_PLL:
            return Regs().Test32(MODS_PTRIM_SYS_LTCPLL_CFG_ENABLE_YES);
            break;
        case XBAR_PLL:
            return Regs().Test32(MODS_PTRIM_SYS_XBARPLL_CFG_ENABLE_YES);
            break;
        default:
            MASSERT(!"Invalid PLL type.");
    }

    return false;
}

//! Get list of PLLs supported by Get/SetPLLCfg(), Set/IsPLLLocked(),
//! and IsPLLEnabled()
//!
/* virtual */ void GpuSubdevice::GetPLLList(vector<PLLType> *pPllList) const
{
    static const PLLType MyPllList[] = { SP_PLL0, SP_PLL1, SYS_PLL, GPC_PLL, LTC_PLL, XBAR_PLL };

    MASSERT(pPllList != NULL);
    pPllList->resize(NUMELEMS(MyPllList));
    copy(&MyPllList[0], &MyPllList[NUMELEMS(MyPllList)], pPllList->begin());
}

//! Colwert PLLType into ascii string
/* static */ const char *GpuSubdevice::GetPLLName(PLLType pll)
{
    switch (pll)
    {
        case LW_PLL:
            return "LW_PLL";
        case H_PLL:
            return "H_PLL";
        case SP_PLL0:
            return "SP_PLL0";
        case SP_PLL1:
            return "SP_PLL1";
        case SYS_PLL:
            return "SYS_PLL";
        case GPC_PLL:
            return "GPC_PLL";
        case LTC_PLL:
            return "LTC_PLL";
        case XBAR_PLL:
            return "XBAR_PLL";
        default:
            MASSERT(!"Invalid PLL type.");
    }
    return "UNKNOWN_PLL";
}

RC GpuSubdevice::CalibratePLL(Gpu::ClkSrc clkSrc)
{
    RC rc;
    LwRmPtr pLwRm;
    LW2080_CTRL_CLK_NOISE_AWARE_PLL_CALIBRATION_PARAMS calibrationParams;

    memset(&calibrationParams, 0, sizeof(calibrationParams));

    calibrationParams.clkSrc = ClkSrcToCtrl2080ClkSrc(clkSrc);

    CHECK_RC(pLwRm->Control(pLwRm->GetSubdeviceHandle(this),
                            LW2080_CTRL_CMD_CLK_NOISE_AWARE_PLL_CALIBRATION,
                            &calibrationParams, sizeof(calibrationParams)));

    return rc;
}

string GpuSubdevice::GetUUIDString(bool bRemoveGPUPrefix)
{
    RC rc;
    string strUuid;
    LW0000_CTRL_GPU_GET_UUID_FROM_GPU_ID_PARAMS params = { };
    params.gpuId = GetGpuId();
    params.flags = LW0000_CTRL_CMD_GPU_GET_UUID_FROM_GPU_ID_FLAGS_FORMAT_ASCII;

    rc = LwRmPtr()->Control(LwRmPtr()->GetClientHandle(),
                            LW0000_CTRL_CMD_GPU_GET_UUID_FROM_GPU_ID,
                            &params,
                            sizeof(params));
    if (rc == RC::OK && params.uuidStrLen)
    {
        strUuid = (const char *)&params.gpuUuid[0];
    }
    if (bRemoveGPUPrefix && (strUuid.find("GPU-") == 0))
    {
        strUuid.erase(0, 4);
    }
    return (strUuid);
}

//! \brief Return the Chip ID (ECID) as a one-line human-readable string
//!
string GpuSubdevice::ChipIdCooked()
{
    struct ChipInfo
    {
        LW2080_CTRL_GPU_INFO LotCode;
        LW2080_CTRL_GPU_INFO FabCodeFab;
        LW2080_CTRL_GPU_INFO WaferID;
        LW2080_CTRL_GPU_INFO XCoord;
        LW2080_CTRL_GPU_INFO YCoord;
        LW2080_CTRL_GPU_INFO VendorID;
    } LwrChipInfo = { };

    LW2080_CTRL_GPU_GET_INFO_V2_PARAMS Params = { };
    RC rc;
    Params.gpuInfoListSize =
        sizeof (LwrChipInfo) / sizeof (LW2080_CTRL_GPU_INFO);

    LwrChipInfo.FabCodeFab.index = LW2080_CTRL_GPU_INFO_INDEX_FAB_CODE_FAB;
    LwrChipInfo.LotCode.index = LW2080_CTRL_GPU_INFO_INDEX_LOT_CODE;
    LwrChipInfo.WaferID.index = LW2080_CTRL_GPU_INFO_INDEX_WAFER_ID;
    LwrChipInfo.XCoord.index = LW2080_CTRL_GPU_INFO_INDEX_X_COORD;
    LwrChipInfo.YCoord.index = LW2080_CTRL_GPU_INFO_INDEX_Y_COORD;
    LwrChipInfo.VendorID.index = LW2080_CTRL_GPU_INFO_INDEX_VENDOR_CODE;

    LW2080_CTRL_GPU_INFO *pChipInfo = (LW2080_CTRL_GPU_INFO*)&LwrChipInfo;
    for (UINT32 i = 0; i < Params.gpuInfoListSize; i++)
    {
        Params.gpuInfoList[i].index = pChipInfo[i].index;
    }

    rc = LwRmPtr()->ControlBySubdevice(this,
                                       LW2080_CTRL_CMD_GPU_GET_INFO_V2,
                                       &Params,
                                       sizeof(Params));

    if (rc != OK)
    {
        Printf(Tee::PriError,
               "ERROR LOADING GPU INFO! RmControl call failed! RC = %d\n",
               (UINT32)rc);
        memset(&LwrChipInfo, 0, sizeof (LwrChipInfo));
#ifndef SEQ_FAIL
        MASSERT(!"Failed to load GPU info through RmControl");
#endif
        return "";
    }

    for (UINT32 i = 0; i < Params.gpuInfoListSize; i++)
    {
        pChipInfo[i].data = Params.gpuInfoList[i].data;
    }

    // Return the string
    char LotCodeCooked[16] = "*****";
    const char LotCode2Char[] = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ";

    for (UINT32 CharIdx = 0; CharIdx < 5; CharIdx++)
        LotCodeCooked[4-CharIdx] =
            LotCode2Char[
                (LwrChipInfo.LotCode.data >> 6*CharIdx) & 0x3F];
    LotCodeCooked[5] = '\0';

    return Utility::StrPrintf(
            "%c%s-%02d_x%02d_y%02d",
            LotCode2Char[LwrChipInfo.FabCodeFab.data & 0x3F],
            LotCodeCooked,
            (int)(LwrChipInfo.WaferID.data),
            (int)(LwrChipInfo.XCoord.data),
            (int)(LwrChipInfo.YCoord.data));
}

// Return string of the form "Gpu(dev,sbdev,0xdomain:0xbus:0xdev.0xfunc)"
string GpuSubdevice::GpuIdentStr()
{
    if (!m_GpuName.empty())
    {
        return m_GpuName;
    }
    else if (IsSOC())
    {
        return "GPU (SOC)";
    }
    else
    {
        auto pPcie = GetInterface<Pcie>();
        if (pPcie)
        {
            string domain;
            if (pPcie->DomainNumber())
                domain = Utility::StrPrintf("%04x:", pPcie->DomainNumber());
            return Utility::StrPrintf("GPU %u [%s%02x:%02x.%x]",
                                      m_pPciDevInfo->GpuInstance,
                                      domain.c_str(),
                                      pPcie->BusNumber(),
                                      pPcie->DeviceNumber(),
                                      pPcie->FunctionNumber());
        }
        else
        {
            string domain;
            if (m_pPciDevInfo->PciDomain)
                domain = Utility::StrPrintf("%04x:", m_pPciDevInfo->PciDomain);
            return Utility::StrPrintf("GPU %u [%s%02x:%02x.%x]",
                                      m_pPciDevInfo->GpuInstance,
                                      domain.c_str(),
                                      m_pPciDevInfo->PciBus,
                                      m_pPciDevInfo->PciDevice,
                                      m_pPciDevInfo->PciFunction);
        }
    }
}

UINT32 GpuSubdevice::SampleType()
{
    if (!m_GpuInfoLoaded)
    {
        LoadGpuInfo();
    }
    if (!m_GpuInfoValid.sampleTypeValid)
    {
        return 0;
    }
    return m_GpuInfo.sampleType.data;
}

//! \brief Return HWQual type
UINT32 GpuSubdevice::HQualType()
{
    if (!m_GpuInfoLoaded)
    {
        LoadGpuInfo();
    }
    if (!m_GpuInfoValid.hwQualTypeValid)
    {
        return 0;
    }
    return m_GpuInfo.hwQualType.data;
}

bool GpuSubdevice::IsEcidLo32Valid()
{
    if (!m_GpuInfoLoaded)
    {
        LoadGpuInfo();
    }
    return m_GpuInfoValid.ecidLo32Valid;
}

bool GpuSubdevice::IsEcidHi32Valid()
{
    if (!m_GpuInfoLoaded)
    {
        LoadGpuInfo();
    }
    return m_GpuInfoValid.ecidHi32Valid;
}

bool GpuSubdevice::IsExtendedValid()
{
    if (!m_GpuInfoLoaded)
    {
        LoadGpuInfo();
    }
    return m_GpuInfoValid.extendedValid;
}

bool GpuSubdevice::IsExtMinorRevisiolwalid()
{
    if (!m_GpuInfoLoaded)
    {
        LoadGpuInfo();
    }
    return m_GpuInfoValid.extMinorRevisiolwalid;
}

bool GpuSubdevice::IsNetlistRevision0Valid()
{
    if (!m_GpuInfoLoaded)
    {
        LoadGpuInfo();
    }
    return m_GpuInfoValid.netlistRevision0Valid;
}

bool GpuSubdevice::IsNetlistRevision1Valid()
{
    if (!m_GpuInfoLoaded)
    {
        LoadGpuInfo();
    }
    return m_GpuInfoValid.netlistRevision1Valid;
}

bool GpuSubdevice::IsSampleTypeValid()
{
    if (!m_GpuInfoLoaded)
    {
        LoadGpuInfo();
    }
    return m_GpuInfoValid.sampleTypeValid;
}

bool GpuSubdevice::IsHwQualTypeValid()
{
    if (!m_GpuInfoLoaded)
    {
        LoadGpuInfo();
    }
    return m_GpuInfoValid.hwQualTypeValid;
}

//-----------------------------------------------------------------------------
/*virtual*/
UINT32 GpuSubdevice::GetEmulationRev()
{
    // Kernel mode RM can only read the emulation revision after initialization
    // of subdevice handles, until then return 0
    if (Platform::UsesLwgpuDriver() ||
        (!Platform::HasClientSideResman() && !m_IsLwRmSubdevHandlesValid) ||
        !Regs().IsSupported(MODS_PBUS_EMULATION_REV0) ||
        IsSOC())
    {
        return 0;
    }

    return Regs().Read32(MODS_PBUS_EMULATION_REV0);
}

//----------------------------------------------------------------------------
bool GpuSubdevice::IsEmulation() const
{
    // During RMInit there are a few calls to get the supported JT
    // function capabilities. The problem there is that we have to
    // pass back different values depending on if we are running under
    // silicon or emulation. However the GpuSubdevice has not been
    // fully initialized yet so we can't read from it to get that
    // information. RM will try to read the LW_PBUS_EMULATION_REV0 to
    // determine if we are running under emulation so fall back to
    // that approach. see bug http://lwbugs/1417727
    if (!IsInitialized())
    {
        return (m_EmulationRev != 0);
    }
    return m_IsEmulation;
}

//----------------------------------------------------------------------------
bool GpuSubdevice::IsEmOrSim() const
{
    return IsEmulation() ||
           (Platform::GetSimulationMode() != Platform::Hardware);
}

//----------------------------------------------------------------------------
bool GpuSubdevice::IsDFPGA() const
{
    return m_IsDFPGA;
}

//----------------------------------------------------------------------------
UINT32 GpuSubdevice::GetCoreCount() const
{
    return GetGrInfoTotal(LW2080_CTRL_GR_INFO_INDEX_GPU_CORE_COUNT);
}

UINT32 GpuSubdevice::GetRTCoreCount() const
{
    return GetGrInfoTotal(LW2080_CTRL_GR_INFO_INDEX_RT_CORE_COUNT);
}

UINT32 GpuSubdevice::GetGrInfoTotal(UINT32 rmInfoIndex) const
{
    LW2080_CTRL_GR_GET_INFO_PARAMS params = { };
    LW2080_CTRL_GR_INFO grInfo = { };
    grInfo.index = rmInfoIndex;
    params.grInfoListSize = 1;
    params.grInfoList = LW_PTR_TO_LwP64(&grInfo);
    UINT32 count = 0;

    if (IsSMCEnabled())
    {
        for (auto const & smcEngineId : GetSmcEngineIds())
        {
            params.grRouteInfo = { };
            SetGrRouteInfo(&(params.grRouteInfo), smcEngineId);
            if (LwRmPtr()->ControlBySubdevice(this,
                                              LW2080_CTRL_CMD_GR_GET_INFO,
                                              &params,
                                              sizeof(params)) != RC::OK)
            {
                Printf(Tee::PriWarn, "RmControl failed getting count"
                       " for swizzId %d and smcEngineId %d\n", GetSwizzId(), smcEngineId);
            }
            else
            {
                count += grInfo.data;
            }
        }
    }
    else
    {
        if (LwRmPtr()->ControlBySubdevice(this,
                                          LW2080_CTRL_CMD_GR_GET_INFO,
                                          &params,
                                          sizeof(params)) != RC::OK)
        {
            Printf(Tee::PriWarn, "RmControl failed getting count.\n");
        }
        else
        {
            count = grInfo.data;
        }
    }

    return count;
}

//-----------------------------------------------------------------------------
bool GpuSubdevice::GetPteKindFromName(const char *name, UINT32 *pPteKind)
{
    // Scan the table for a PTE kind that matches the specified name
    PteKindTable::const_iterator end = m_PteKindTable.end();
    for (PteKindTable::const_iterator kind = m_PteKindTable.begin();
         kind != end; ++kind)
    {
        if (kind->Name == name)
        {
            *pPteKind = kind->PteKind;
            return true;
        }
    }
    return false;
}

//-----------------------------------------------------------------------------
const char *GpuSubdevice::GetPteKindName(UINT32 pteKind)
{
    // Scan the table for this PTE kind
    PteKindTable::const_iterator end = m_PteKindTable.end();
    for (PteKindTable::const_iterator pKind = m_PteKindTable.begin();
         pKind != end; ++pKind)
    {
        if (pKind->PteKind == pteKind)
        {
            return pKind->Name.c_str();
        }
    }

    return 0;
}

RC GpuSubdevice::SetMonitorRegisters(const string &registers)
{
    RC rc;

    vector<string> registerTokens;
    CHECK_RC(Utility::Tokenizer(registers, "|; \r\n", &registerTokens));
    const size_t numRegisters = registerTokens.size();

    const RefManual *pRefManual = GetRefManual();

    for (UINT32 registerIdx = 0; registerIdx < numRegisters; registerIdx++)
    {
        MonitorRegister monitorRegister = {0};

        if (!isdigit(registerTokens[registerIdx][0]))
        {
            if (pRefManual->GetNumRegisters() == 0)
            {
                Printf(Tee::PriError, "MODS has no manuals for %s.\n",
                    Gpu::DeviceIdToString(DeviceId()).c_str());
                return RC::FILE_UNKNOWN_ERROR;
            }

            vector<string> arrayTokens;
            CHECK_RC(Utility::Tokenizer(registerTokens[registerIdx], "(,)",
                &arrayTokens));

            if (arrayTokens.size() < 1)
            {
                Printf(Tee::PriError, "Error extracting register name.\n");
                return RC::SOFTWARE_ERROR;
            }

            monitorRegister.pRMR =
                pRefManual->FindRegister(arrayTokens[0].c_str());
            if (!monitorRegister.pRMR)
            {
                Printf(Tee::PriError, "Register %s not found in manuals.\n",
                       arrayTokens[0].c_str());
                return RC::ILWALID_INPUT;
            }
            if ( (int)(arrayTokens.size() - 1) !=
                monitorRegister.pRMR->GetDimensionality())
            {
                Printf(Tee::PriError,
                       "Wrong number of array indexes in \"%s\"\n",
                       registerTokens[registerIdx].c_str());
                return RC::ILWALID_INPUT;
            }
            if (arrayTokens.size() >= 2)
            {
                monitorRegister.i = strtoul(arrayTokens[1].c_str(), nullptr, 0);
                if (monitorRegister.i >=
                    (UINT32)monitorRegister.pRMR->GetArraySizeI())
                {
                    Printf(Tee::PriError,
                           "index \"i\" of register %s is out of bounds\n",
                           arrayTokens[0].c_str());
                    return RC::ILWALID_INPUT;
                }
            }
            if (arrayTokens.size() >= 3)
            {
                monitorRegister.j = strtoul(arrayTokens[2].c_str(), nullptr, 0);
                if (monitorRegister.j >=
                    (UINT32)monitorRegister.pRMR->GetArraySizeJ())
                {
                    Printf(Tee::PriError,
                           "index \"j\" of register %s is out of bounds\n",
                           arrayTokens[0].c_str());
                    return RC::ILWALID_INPUT;
                }
            }
            switch (monitorRegister.pRMR->GetDimensionality())
            {
                case 2:
                    monitorRegister.Offset = monitorRegister.pRMR->GetOffset(
                        monitorRegister.i, monitorRegister.j);
                    break;
                case 1:
                    monitorRegister.Offset =
                        monitorRegister.pRMR->GetOffset(monitorRegister.i);
                    break;
                case 0:
                default:
                    monitorRegister.Offset = monitorRegister.pRMR->GetOffset();
                    break;
            }
        }
        else
        {
            monitorRegister.Offset =
                strtoul(registerTokens[registerIdx].c_str(), nullptr, 0);
        }
        m_MonitorRegisters.push_back(monitorRegister);
    }

    return rc;
}

void GpuSubdevice::PrintMonitorRegisters() const
{
    const size_t numMonitorRegisters = m_MonitorRegisters.size();

    for (size_t regIdx = 0; regIdx < numMonitorRegisters; regIdx++)
    {
        const UINT32 offset = m_MonitorRegisters[regIdx].Offset;
        const UINT32 regValue = RegRd32(offset);
        const RefManualRegister *pRMR = m_MonitorRegisters[regIdx].pRMR;

        if (!pRMR)
        {
            Printf(Tee::PriNormal, "  0x%08x = 0x%08x\n", offset, regValue);
        }
        else
        {
            string regName = pRMR->GetName();
            switch (pRMR->GetDimensionality())
            {
                case 2:
                    regName += Utility::StrPrintf("(%u,%u)",
                        m_MonitorRegisters[regIdx].i,
                        m_MonitorRegisters[regIdx].j);
                    break;
                case 1:
                    regName += Utility::StrPrintf("(%u)",
                        m_MonitorRegisters[regIdx].i);
                    break;
            }

            Printf(Tee::PriNormal, "  0x%08x [%s] = 0x%08x\n",
                offset, regName.c_str(), regValue);
            const size_t regNameSize = pRMR->GetName().size();
            for (int fieldIdx = 0; fieldIdx < pRMR->GetNumFields(); fieldIdx++)
            {
                const RefManualRegisterField *pRMRF = pRMR->GetField(fieldIdx);
                const UINT32 fieldValue =
                    (regValue & pRMRF->GetMask()) >> pRMRF->GetLowBit();
                string valueName;
                const size_t fieldNameSize = pRMRF->GetName().size();
                if (fieldValue < (UINT32)pRMRF->GetNumValues())
                {
                    const RefManualFieldValue * pRMFV =
                        pRMRF->GetValue(fieldValue);
                    if (pRMFV)
                    {
                        valueName = " [";
                        valueName += &pRMFV->Name.c_str()[fieldNameSize];
                        valueName += "]";
                    }
                }
                Printf(Tee::PriNormal, "    %s = 0x%x%s\n",
                    &pRMRF->GetName().c_str()[regNameSize], fieldValue,
                    valueName.c_str());
            }
        }
    }
}

UINT32 GpuSubdevice::GetEngineStatus(Engine engineName)
{
    switch (engineName)
    {
        case GpuSubdevice::GR_ENGINE:
            return Regs().Read32(MODS_PGRAPH_STATUS);
        case GpuSubdevice::MSPDEC_ENGINE:
            return Regs().Read32(MODS_PMSPDEC_FALCON_IDLESTATE);
        case GpuSubdevice::MSPPP_ENGINE:
            return Regs().Read32(MODS_PMSPPP_FALCON_IDLESTATE);
        case GpuSubdevice::MSVLD_ENGINE:
            return Regs().Read32(MODS_PMSVLD_FALCON_IDLESTATE);
        case GpuSubdevice::PCE0_ENGINE:
            return Regs().Read32(MODS_PCE0_FALCON_IDLESTATE);
        case GpuSubdevice::PCE1_ENGINE:
            return Regs().Read32(MODS_PCE1_FALCON_IDLESTATE);
        default:
            Printf(Tee::PriNormal, "engine not supported on current chip.\n");
            return 0;
            break;
    }
}

//! Return the device name
string GpuSubdevice::DeviceName()
{
    LW2080_CTRL_GPU_GET_NAME_STRING_PARAMS params = {0};
    params.gpuNameStringFlags = FLD_SET_DRF(2080, _CTRL_GPU,
                                            _GET_NAME_STRING_FLAGS_TYPE,
                                            _ASCII, params.gpuNameStringFlags);
    if (OK !=
        LwRmPtr()->ControlBySubdevice(this,
                                      LW2080_CTRL_CMD_GPU_GET_NAME_STRING,
                                      &params, sizeof(params)))
    {
        return "Unknown";
    }

    return (char *)params.gpuNameString.ascii;
}

//! Return the device short name
string GpuSubdevice::DeviceShortName()
{
    LW2080_CTRL_GPU_GET_SHORT_NAME_STRING_PARAMS params = {{0}};
    if (OK !=
        LwRmPtr()->ControlBySubdevice(this,
                                      LW2080_CTRL_CMD_GPU_GET_SHORT_NAME_STRING,
                                      &params, sizeof(params)))
    {
        return "Unknown";
    }

    return (char *)params.gpuShortNameString;
}

//! Return the architecture number
UINT32 GpuSubdevice::Architecture()
{
    LW2080_CTRL_MC_GET_ARCH_INFO_PARAMS params = {0};
    if (OK != LwRmPtr()->ControlBySubdevice(this,
                                            LW2080_CTRL_CMD_MC_GET_ARCH_INFO,
                                            &params, sizeof(params)))
    {
        Printf(Tee::PriWarn,
               "RmControl failed getting architecture information.\n");
        return 0;
    }

    return params.architecture;
}

//! Return the implementation number
UINT32 GpuSubdevice::Implementation()
{
    LW2080_CTRL_MC_GET_ARCH_INFO_PARAMS params = {0};
    if (OK != LwRmPtr()->ControlBySubdevice(this,
                                            LW2080_CTRL_CMD_MC_GET_ARCH_INFO,
                                            &params, sizeof(params)))
    {
        Printf(Tee::PriWarn,
               "RmControl failed getting implementation information.\n");
        return 0;
    }

    return params.implementation;
}

UINT32 GpuSubdevice::ExtMinorRevision()
{
    if (!m_GpuInfoLoaded)
    {
        LoadGpuInfo();
    }
    if (!m_GpuInfoValid.extMinorRevisiolwalid)
    {
        return 0;
    }
    return m_GpuInfo.extMinorRevision.data;
}

UINT32 GpuSubdevice::NetlistRevision0()
{
    if (!m_GpuInfoLoaded)
    {
        LoadGpuInfo();
    }
    if (!m_GpuInfoValid.netlistRevision0Valid)
    {
        return 0;
    }
    return m_GpuInfo.netlistRevision0.data;
}

UINT32 GpuSubdevice::NetlistRevision1()
{
    if (!m_GpuInfoLoaded)
    {
        LoadGpuInfo();
    }
    if (!m_GpuInfoValid.netlistRevision1Valid)
    {
        return 0;
    }
    return m_GpuInfo.netlistRevision1.data;
}

//! Set the size of the usable region of the FB
void GpuSubdevice::SetFbHeapSize(UINT64 val)
{
    if (0 == m_FbHeapSize)
    {
        m_FbHeapSize = val;
    }
}

//! Set L2 Cache Only Mode
void GpuSubdevice::SetL2CacheOnlyMode(bool val)
{
    m_L2CacheOnlyMode = val;
}

void GpuSubdevice::AckDirectRegAccess(UINT32 reg) const
{
    // This check is only with GPU driver in the kernel.
    // With RM, we don't need to do this, because RM doesn't control GPU RG, MODS does.
    if (Platform::UsesLwgpuDriver())
    {
        Printf(Tee::PriError,
               "Direct reg access 0x%08x may hang the CPU\n", reg);
        MASSERT(!"Direct register access");
    }
}

//! Read an 8-bit register value
UINT08 GpuSubdevice::RegRd08(UINT32 offset) const
{
    offset = Gpu::GetRemappedReg(offset);
    MASSERT(!IsPerSMCEngineReg(offset));
    if (m_pPciDevInfo->LinLwBase)
    {
        AckDirectRegAccess(offset);
        UINT08 value = MEM_RD08((char *)m_pPciDevInfo->LinLwBase + offset);
        sLogRegRead(this, GetGpuInst(), offset, value, 8);
        return value;
    }
#ifdef NDEBUG
    if (Tee::GetFileSink()->IsEncrypted())
#endif
    {
        Printf(Tee::PriError, "Failed to read register 0x%08x\n", offset);
    }
    MASSERT(!"RegRd08 failed to read register");
    return 0;
}

//! Read a 16-bit register value
UINT16 GpuSubdevice::RegRd16(UINT32 offset) const
{
    offset = Gpu::GetRemappedReg(offset);
    MASSERT(!IsPerSMCEngineReg(offset));
    if (m_pPciDevInfo->LinLwBase)
    {
        AckDirectRegAccess(offset);
        UINT16 value = MEM_RD16((char *)m_pPciDevInfo->LinLwBase + offset);
        sLogRegRead(this, GetGpuInst(), offset, value, 16);
        return value;
    }
#ifdef NDEBUG
    if (Tee::GetFileSink()->IsEncrypted())
#endif
    {
        Printf(Tee::PriError, "Failed to read register 0x%08x\n", offset);
    }
    MASSERT(!"RegRd16 failed to read register");
    return 0;
}


UINT32 GpuSubdevice::RegOpRd32(UINT32 offset, LwRm * pLwRm) const
{
    offset = Gpu::GetRemappedReg(offset);
    if (!pLwRm)
    {
        pLwRm = LwRmPtr().Get();
    }

    LW2080_CTRL_GPU_REG_OP regReadOp = { 0 };
    LW2080_CTRL_GPU_EXEC_REG_OPS_PARAMS params;
    RC rc;

    memset(&params, 0, sizeof(LW2080_CTRL_GPU_EXEC_REG_OPS_PARAMS));
    regReadOp.regOp = LW2080_CTRL_GPU_REG_OP_READ_32;
    regReadOp.regType = LW2080_CTRL_GPU_REG_OP_TYPE_GLOBAL;
    regReadOp.regOffset = offset;
    regReadOp.regValueHi = 0;
    regReadOp.regValueLo = 0;
    regReadOp.regAndNMaskHi = 0;
    regReadOp.regAndNMaskLo = ~0;

    params.regOpCount = 1;
    params.regOps = LW_PTR_TO_LwP64(&regReadOp);

    rc = pLwRm->ControlBySubdevice(
            this,
            LW2080_CTRL_CMD_GPU_EXEC_REG_OPS,
            &params,
            sizeof(params));

    if ((rc != OK) ||
            (regReadOp.regStatus !=
             LW2080_CTRL_GPU_REG_OP_STATUS_SUCCESS))
    {
#ifdef NDEBUG
        if (Tee::GetFileSink()->IsEncrypted())
#endif
        {
            Printf(Tee::PriError,
                    "GpuSubdevice::RegRd32 : failed to read register 0x%08x.\n", offset);
            if (rc != OK)
            {
                Printf(Tee::PriError, "GpuSubdevice::RegRd32 : %s\n",
                        rc.Message());
            }
            else
            {
                Printf(Tee::PriError,
                        "GpuSubdevice::RegRd32 : Reg Op Status = %d\n",
                        regReadOp.regStatus);
            }
        }
#if defined(DEBUG)
        if ((Xp::GetOperatingSystem() != Xp::OS_LINDA) &&
            (Xp::GetOperatingSystem() != Xp::OS_WINDA))
        {
            MASSERT(!"Generic assertion failure<refer to previous message>.");
        }
#endif
    }

    return regReadOp.regValueLo;
}

//! Read an 32-bit register value
UINT32 GpuSubdevice::RegRd32(UINT32 offset) const
{
    offset = Gpu::GetRemappedReg(offset);
    MASSERT(!IsPerSMCEngineReg(offset));
    bool bUseRegOps = m_bUseRegOps ? m_IsLwRmSubdevHandlesValid : false;

    if ((m_bCheckPowerGating && RegPowerGated(offset)) ||
        RegDecodeTrapped(offset))
    {
        bUseRegOps = m_Initialized;
        if (!m_Initialized)
        {
            MASSERT(!RegDecodeTrapped(offset));
            Printf(RegDecodeTrapped(offset) ? Tee::PriError : Tee::PriLow,
                   "GpuSubdevice::RegRd32 : Accessing %s register "
                   "(0x%08x) before initialization\n",
                   RegDecodeTrapped(offset) ? "trapped" : "power gated",
                   offset);
        }
    }

    UINT32 val = 0;
    if (bUseRegOps ||
        (!Platform::HasClientSideResman() &&
         !Platform::UsesLwgpuDriver() &&
         !IsSOC() &&
         m_IsLwRmSubdevHandlesValid))
    {
        val = RegOpRd32(offset);

    }
    else if (m_pPciDevInfo->LinLwBase)
    {
        AckDirectRegAccess(offset);
        val = RegRd32PGSafe(offset);
    }
    else
    {
#ifdef NDEBUG
        if (Tee::GetFileSink()->IsEncrypted())
#endif
        {
            Printf(Tee::PriError, "Failed to read register 0x%08x\n", offset);
        }
        MASSERT(!"RegRd32 failed to read register");
        return 0;
    }
    sLogRegRead(this, GetGpuInst(), offset, val, 32);
    return val;
}

//------------------------------------------------------------------------------
UINT32 GpuSubdevice::RegRd32
(
    UINT32        domIdx,
    RegHalDomain  domain,
    LwRm        * pLwRm,
    UINT32        offset
)
{
    offset = Gpu::GetRemappedReg(offset);
    if (domain == RegHalDomain::PCIE ||
        (domain == RegHalDomain::XVE && Platform::IsVirtFunMode()))
    {
        auto   pPcie = GetInterface<Pcie>();
        UINT32 data  = 0;
        Platform::PciRead32(pPcie->DomainNumber(),
                            pPcie->BusNumber(),
                            pPcie->DeviceNumber(),
                            pPcie->FunctionNumber(),
                            offset,
                            &data);
        return data;
    }
    const UINT32 domainBase = GetDomainBase(domIdx, domain, pLwRm);
    return RegRd32(domainBase + offset);
}

//-----------------------------------------------------------------------------
UINT64 GpuSubdevice::RegRd64(UINT32 offset) const
{
    MASSERT(!"RegRd64 not supported in GpuSubdevice\n");
    return 0;
}

//------------------------------------------------------------------------------
UINT64 GpuSubdevice::RegRd64
(
    UINT32        domIdx,
    RegHalDomain  domain,
    LwRm        * pLwRm,
    UINT32        offset
)
{
    MASSERT(!"RegRd64 not supported in GpuSubdevice\n");
    return 0;
}

UINT32 GpuSubdevice::RegRd32PGSafe(UINT32 offset) const
{
    offset = Gpu::GetRemappedReg(offset);
    MASSERT(!IsPerSMCEngineReg(offset));
    const UINT32 value = MEM_RD32((char *)m_pPciDevInfo->LinLwBase + offset);
    return value;
}

void GpuSubdevice::RegWr32PGSafe(UINT32 offset, UINT32 data)
{
    offset = Gpu::GetRemappedReg(offset);
    MASSERT(!IsPerSMCEngineReg(offset));
    MEM_WR32(static_cast<char*>(m_pPciDevInfo->LinLwBase) + offset, data);
}

//! Read a 32-bit value from a context switched register on the specified
//! context
RC GpuSubdevice::CtxRegRd32(LwRm::Handle hCtx, UINT32 offset, UINT32 *pData, LwRm* pLwRm)
{
    offset = Gpu::GetRemappedReg(offset);
    LW2080_CTRL_GPU_REG_OP regReadOp = { 0 };
    LW2080_CTRL_GPU_EXEC_REG_OPS_PARAMS params;
    RC rc;
    pLwRm = pLwRm ? pLwRm : LwRmPtr().Get();

    memset(&params, 0, sizeof(LW2080_CTRL_GPU_EXEC_REG_OPS_PARAMS));
    regReadOp.regOp = LW2080_CTRL_GPU_REG_OP_READ_32;
    regReadOp.regType = LW2080_CTRL_GPU_REG_OP_TYPE_GR_CTX;
    regReadOp.regOffset = offset;
    regReadOp.regValueHi = 0;
    regReadOp.regValueLo = 0;
    regReadOp.regAndNMaskHi = 0;
    regReadOp.regAndNMaskLo = ~0;

    params.regOpCount = 1;
    params.regOps = LW_PTR_TO_LwP64(&regReadOp);
    params.hClientTarget = pLwRm->GetClientHandle();
    params.hChannelTarget = hCtx;

    rc = pLwRm->ControlBySubdevice(
                        this,
                        LW2080_CTRL_CMD_GPU_EXEC_REG_OPS,
                        &params,
                        sizeof(params));

    if (regReadOp.regStatus != LW2080_CTRL_GPU_REG_OP_STATUS_SUCCESS)
    {
        Printf(Tee::PriError,
               "GpuSubdevice::CtxRegRd32 0x%08x: Reg Op Status = %d\n",
               offset, regReadOp.regStatus);
        if (rc == OK)
            rc = RC::LWRM_ILWALID_PARAM_STRUCT;
    }
    else
    {
        *pData = regReadOp.regValueLo;
    }

    return rc;
}

/* static */ void GpuSubdevice::sLogReg
(
    const GpuSubdevice *pSubdev,
    UINT32 gpuInst,
    UINT32 offset,
    UINT32 data,
    UINT32 bits,
    const string &regPrefix
)
{
    const RefManual *pRefManual = nullptr;
    for (vector<REGISTER_RANGE>::iterator
         rangeIter = g_GpuLogAccessRegions.begin();
         rangeIter != g_GpuLogAccessRegions.end();
         ++rangeIter)
    {
        if ((offset >= rangeIter->Start) && (offset <= rangeIter->End))
        {
            if (pSubdev && !pRefManual)
                pRefManual = pSubdev->GetRefManual();

            string regStr = regPrefix + " ";
            if (pRefManual && pRefManual->FindRegister(offset))
            {
                const RefManualRegister *pReg = pRefManual->FindRegister(offset);
                regStr += Utility::StrPrintf("%s (%06x) ", pReg->GetName().c_str(), offset);
            }
            else
            {
                regStr += Utility::StrPrintf("%06x ", offset);
            }
            switch (bits)
            {
                case 8:
                    regStr += Utility::StrPrintf("%02x\n", data);
                    break;
                case 16:
                    regStr += Utility::StrPrintf("%04x\n", data);
                    break;
                case 32:
                    regStr += Utility::StrPrintf("%08x\n", data);
                    break;
                default:
                    // If in future we add a different size, add a new case here
                    MASSERT(!"Invalid value of argument 'bits'.");
            }

            if (!Tasker::CanThreadYield())
                ModsDrvIsrPrintString(Tee::PriNormal, regStr.c_str());
            else
                Printf(Tee::PriNormal, "%s", regStr.c_str());
            return;
        }
    }
}

//------------------------------------------------------------------------------
/*static*/ void GpuSubdevice::sLogRegRead
(
    const GpuSubdevice *pSubdev,
    UINT32 gpuInst,
    UINT32 offset,
    UINT32 data,
    UINT32 bits
)
{
    // create a static string to prevent 1000s of Pool::Alloc calls
    static const string prefix("R+");
    sLogReg(pSubdev, gpuInst, offset, data, bits, prefix);
}

//------------------------------------------------------------------------------
/*static*/ void GpuSubdevice::sLogRegWrite
(
    const GpuSubdevice *pSubdev,
    UINT32 gpuInst,
    UINT32 offset,
    UINT32 data,
    UINT32 bits
)
{
    // create a static string to prevent 1000s of Pool::Alloc calls
    static const string prefix("W+");
    sLogReg(pSubdev, gpuInst, offset, data, bits, prefix);
}

//------------------------------------------------------------------------------
// Write a 8-bit value to a register with acknowledge
void GpuSubdevice::RegWrSync08(UINT32 offset, UINT08 data)
{
    offset = Gpu::GetRemappedReg(offset);
    MASSERT(!IsPerSMCEngineReg(offset));
    RegWr08(offset, data);
    if (Platform::Hardware != Platform::GetSimulationMode())
    {
        //see http://lwbugs/1043132
        RC rc = PollRegValue(offset, data, 0xFF, 1000);
        if (OK != rc)
        {
            Printf(Tee::PriLow,
                   "GpuSubdevice::RegWrSync08(%02x, %02x)"
                   " timed out on readback.\n",
                   offset, static_cast<UINT32>(data));
        }
    }
}

//! Write an 8-bit value to a register
void GpuSubdevice::RegWr08(UINT32 offset, UINT08 data)
{
    offset = Gpu::GetRemappedReg(offset);
    MASSERT(!IsPerSMCEngineReg(offset));
    const UINT32 wordMask = GpuPtr()->GetRegWriteMask(GetGpuInst(), offset);

    //Get exact correct mask to apply considering alignment of Offset byte
    UINT32 byteoffset = offset & 3;
    const UINT08 msk = UINT08(wordMask >> (8*byteoffset));
    if (!msk)
    {
        Printf(Tee::PriDebug,
               "GpuSubdevice::RegWr08(%08x, %02x) excluded.\n",
               offset, static_cast<UINT32>(data));
        return; // Bail without writing the register
    }

    if (msk != (UINT08)~0)
    {
        UINT08 preWriteVal = RegRd08(offset);
        data &= ((data & msk)|(preWriteVal & msk)); //Masking write data
    }

    if (m_pPciDevInfo->LinLwBase)
    {
        AckDirectRegAccess(offset);
        MEM_WR08(static_cast<char*>(m_pPciDevInfo->LinLwBase) + offset, data);
        sLogRegWrite(this, GetGpuInst(), offset, data, 8);
    }
    else
    {
#ifdef NDEBUG
        if (Tee::GetFileSink()->IsEncrypted())
#endif
        {
            Printf(Tee::PriError, "Failed to write register 0x%08x\n", offset);
        }
        MASSERT(!"RegWr08 failed to write register");
    }
}

//------------------------------------------------------------------------------
// Write a 16-bit value to a register with acknowledge
void GpuSubdevice::RegWrSync16(UINT32 offset, UINT16 data)
{
    offset = Gpu::GetRemappedReg(offset);
    MASSERT(!IsPerSMCEngineReg(offset));
    RegWr16(offset, data);
    if (Platform::Hardware != Platform::GetSimulationMode())
    {
        //see http://lwbugs/1043132
        RC rc = PollRegValue(offset, data, 0xFFFF, 1000);
        if (OK != rc)
        {
            Printf(Tee::PriLow,
                   "GpuSubdevice::RegWrSync16(%04x, %04x)"
                   " timed out on readback.\n",
                   offset, static_cast<UINT32>(data));
        }
    }
}

//! Write a 16-bit value to a register
void GpuSubdevice::RegWr16(UINT32 offset, UINT16 data)
{
    offset = Gpu::GetRemappedReg(offset);
    MASSERT(!IsPerSMCEngineReg(offset));
    const UINT32 wordMask = GpuPtr()->GetRegWriteMask(GetGpuInst(), offset);

    //Get the correct mask to apply considering alignment of offset byte
    const UINT32 halfwordoffset = offset & 3;
    const UINT16 msk = UINT16(wordMask >> (8*halfwordoffset));
    if (!msk)         //If all the bits are masked
    {
        Printf(Tee::PriDebug,
               "GpuSubdevice::RegWr16(%08x, %04x) excluded.\n",
               offset, static_cast<UINT32>(data));
        return; // Bail without writing the register
    }

    if (msk != (UINT16)~0)  //If there are some bits to be masked
    {
        //Pre write value of the offset being written
        UINT16 rd_bck = RegRd16(offset);
        data = (data & msk)|(rd_bck & ~(msk));
    }

    if (m_pPciDevInfo->LinLwBase)
    {
        AckDirectRegAccess(offset);
        MEM_WR16(static_cast<char*>(m_pPciDevInfo->LinLwBase) + offset, data);
        sLogRegWrite(this, GetGpuInst(), offset, data, 16);
    }
    else
    {
#ifdef NDEBUG
        if (Tee::GetFileSink()->IsEncrypted())
#endif
        {
            Printf(Tee::PriError, "Failed to write register 0x%08x\n", offset);
        }
        MASSERT(!"RegWr16 failed to write register");
    }
}

//------------------------------------------------------------------------------
// Write a 32-bit value to a register with acknowledge
void GpuSubdevice::RegWrSync32(UINT32 offset, UINT32 data)
{
    offset = Gpu::GetRemappedReg(offset);
    MASSERT(!IsPerSMCEngineReg(offset));
    RegWr32(offset, data);
    if (Platform::Hardware != Platform::GetSimulationMode())
    {
        //see http://lwbugs/1043132
        RC rc = PollRegValue(offset, data, ~0, 1000);
        if (OK != rc)
        {
            Printf(Tee::PriLow,
                   "GpuSubdevice::RegWrSync32(%08x, %08x)"
                   " timed out on readback.\n",
                   offset, data);
        }
    }
}

//------------------------------------------------------------------------------
void GpuSubdevice::RegWrSync32
(
    UINT32        domIdx,
    RegHalDomain  domain,
    LwRm        * pLwRm,
    UINT32        offset,
    UINT32        data
)
{
    offset = Gpu::GetRemappedReg(offset);
    if (domain == RegHalDomain::PCIE ||
        (domain == RegHalDomain::XVE && Platform::IsVirtFunMode()))
    {
        auto pPcie = GetInterface<Pcie>();
        Platform::PciWrite32(pPcie->DomainNumber(),
                             pPcie->BusNumber(),
                             pPcie->DeviceNumber(),
                             pPcie->FunctionNumber(),
                             offset,
                             data);
        return;
    }
    const UINT32 domainBase = GetDomainBase(domIdx, domain, pLwRm);
    RegWrSync32(domainBase + offset, data);
}

//------------------------------------------------------------------------------
void GpuSubdevice::RegWrSync64(UINT32 offset, UINT64 data)
{
    MASSERT(!"RegWrSync64 not supported in GpuSubdevice\n");
}

//------------------------------------------------------------------------------
void GpuSubdevice::RegWrSync64
(
    UINT32        domIdx,
    RegHalDomain  domain,
    LwRm        * pLwRm,
    UINT32        offset,
    UINT64        data
)
{
    MASSERT(!"RegWrSync64 not supported in GpuSubdevice\n");
}

void GpuSubdevice::RegOpWr32(UINT32 offset, UINT32 data, LwRm * pLwRm)
{
    offset = Gpu::GetRemappedReg(offset);
    if (!pLwRm)
    {
        pLwRm = LwRmPtr().Get();
    }

    LW2080_CTRL_GPU_REG_OP regWriteOp = { 0 };
    LW2080_CTRL_GPU_EXEC_REG_OPS_PARAMS params;
    RC rc;

    memset(&params, 0, sizeof(LW2080_CTRL_GPU_EXEC_REG_OPS_PARAMS));
    regWriteOp.regOp = LW2080_CTRL_GPU_REG_OP_WRITE_32;
    regWriteOp.regType = LW2080_CTRL_GPU_REG_OP_TYPE_GLOBAL;
    regWriteOp.regOffset = offset;
    regWriteOp.regValueHi = 0;
    regWriteOp.regValueLo = data;
    regWriteOp.regAndNMaskHi = 0;
    regWriteOp.regAndNMaskLo = ~0;

    params.regOpCount = 1;
    params.regOps = LW_PTR_TO_LwP64(&regWriteOp);

    rc = pLwRm->ControlBySubdevice(
            this,
            LW2080_CTRL_CMD_GPU_EXEC_REG_OPS,
            &params,
            sizeof(params));

    if ((rc != OK) ||
            (regWriteOp.regStatus !=
             LW2080_CTRL_GPU_REG_OP_STATUS_SUCCESS))
    {
#ifdef NDEBUG
        if (Tee::GetFileSink()->IsEncrypted())
#endif
        {
            Printf(Tee::PriError,
                    "GpuSubdevice::RegWr32 : failed to write register 0x%08x.\n", offset);
            if (rc != OK)
            {
                Printf(Tee::PriError, "GpuSubdevice::RegWr32 : %s\n",
                        rc.Message());
            }
            else
            {
                Printf(Tee::PriError,
                        "GpuSubdevice::RegWr32 : Reg Op Status = %d\n",
                        regWriteOp.regStatus);
            }
        }
#if defined(DEBUG)
        if ((Xp::GetOperatingSystem() != Xp::OS_LINDA) &&
            (Xp::GetOperatingSystem() != Xp::OS_WINDA))
        {
            MASSERT(!"Generic assertion failure<refer to previous message>.");
        }
#endif
    }
    return;
}

//! Write a 32-bit value to a register
void GpuSubdevice::RegWr32(UINT32 offset, UINT32 data)
{
    offset = Gpu::GetRemappedReg(offset);
    MASSERT(!IsPerSMCEngineReg(offset));
    UINT32 msk = GpuPtr()->GetRegWriteMask(GetGpuInst(), offset);

    if (!msk)
    {
        Printf(Tee::PriDebug,
               "GpuSubdevice::RegWr32(%08x, %08x) excluded.\n",
               offset, static_cast<UINT32>(data));
        return; // Bail without writing the register
    }

    if (msk != (UINT32) ~0)
    {
        //Considering pre write value at the offset
        UINT32 rd_bck = RegRd32(offset);
        data = (data & msk)|(rd_bck & ~(msk));
    }

    bool bUseRegOps = m_bUseRegOps ? m_IsLwRmSubdevHandlesValid : false;

    if ((m_bCheckPowerGating && RegPowerGated(offset)) ||
        (m_CheckPrivProtection && m_Initialized && RegPrivProtected(offset)) ||
        RegDecodeTrapped(offset))
    {
        bUseRegOps = m_Initialized;
        if (!m_Initialized)
        {
            MASSERT(!RegDecodeTrapped(offset));
            Printf(RegDecodeTrapped(offset) ? Tee::PriError : Tee::PriLow,
                   "GpuSubdevice::RegWr32 : Accessing %s register "
                   "(0x%08x) before initialization\n",
                   RegDecodeTrapped(offset) ? "trapped" : "power gated",
                   offset);
        }
    }

    if (bUseRegOps ||
        (!Platform::HasClientSideResman() &&
         !Platform::UsesLwgpuDriver() &&
         !IsSOC() &&
         m_IsLwRmSubdevHandlesValid))
    {
        RegOpWr32(offset, data);
    }
    else if (m_pPciDevInfo->LinLwBase)
    {
        AckDirectRegAccess(offset);
        RegWr32PGSafe(offset, data);
    }
    else
    {
#ifdef NDEBUG
        if (Tee::GetFileSink()->IsEncrypted())
#endif
        {
            Printf(Tee::PriError, "Failed to write register 0x%08x\n", offset);
        }
        MASSERT(!"RegWr32 failed to write register");
        return;
    }

    sLogRegWrite(this, GetGpuInst(), offset, data, 32);

#ifdef RECORD_REG_WR
    Platform::DisableInterrupts();
    Printf(Tee::FileOnly, "REG_WR32(%#010x,%#010x)\n", offset, data);
    Platform::EnableInterrupts();
#endif
}

//------------------------------------------------------------------------------
void GpuSubdevice::RegWr32
(
    UINT32        domIdx,
    RegHalDomain  domain,
    LwRm        * pLwRm,
    UINT32        offset,
    UINT32        data
)
{
    offset = Gpu::GetRemappedReg(offset);
    if (domain == RegHalDomain::PCIE ||
        (domain == RegHalDomain::XVE && Platform::IsVirtFunMode()))
    {
        auto pPcie = GetInterface<Pcie>();
        Platform::PciWrite32(pPcie->DomainNumber(),
                             pPcie->BusNumber(),
                             pPcie->DeviceNumber(),
                             pPcie->FunctionNumber(),
                             offset,
                             data);
        return;
    }

    const UINT32 domainBase = GetDomainBase(domIdx, domain, pLwRm);
    RegWr32(domainBase + offset, data);
}

//-----------------------------------------------------------------------------
void GpuSubdevice::RegWr64(UINT32 offset, UINT64 data)
{
    MASSERT(!"RegWr64 not supported in GpuSubdevice\n");
}

//------------------------------------------------------------------------------
void GpuSubdevice::RegWr64
(
    UINT32        domIdx,
    RegHalDomain  domain,
    LwRm        * pLwRm,
    UINT32        offset,
    UINT64        data
)
{
    MASSERT(!"RegWr64 not supported in GpuSubdevice\n");
}

//------------------------------------------------------------------------------
void GpuSubdevice::RegWrBroadcast32
(
    UINT32        domIdx,
    RegHalDomain  domain,
    LwRm        * pLwRm,
    UINT32        offset,
    UINT32        data
)
{
    MASSERT(!"RegWrBroadcast32 not supported in GpuSubdevice\n");
}

//------------------------------------------------------------------------------
void GpuSubdevice::RegWrBroadcast64
(
    UINT32        domIdx,
    RegHalDomain  domain,
    LwRm        * pLwRm,
    UINT32        offset,
    UINT64        data
)
{
    MASSERT(!"RegWrBroadcast64 not supported in GpuSubdevice\n");
}

RC GpuSubdevice::UnlockHost2Jtag(const vector<UINT32> &secCfgMask,
                                 const vector<UINT32> &selwreKeys)
{
    // derived classes must implement this secure sequence.
    return RC::UNSUPPORTED_FUNCTION;
}

RC GpuSubdevice::ReadHost2Jtag(UINT32 chiplet,
                               UINT32 instruction,
                               UINT32 chainLength,
                               vector<UINT32> *pResult)
{
    RC rc;
    MASSERT(pResult);
    pResult->resize(chainLength / 32 + 1);

    if (IsSOC())
    {
        CHECK_RC(CheetAh::SocPtr()->ReadHost2Jtag(chiplet,
                                                instruction,
                                                chainLength,
                                                pResult));
    }
    else
    {
        LwRmPtr pLwRm;
        LwRm::Handle hSubDevDiag;
        hSubDevDiag = GetSubdeviceDiag();

        LW208F_CTRL_GPU_JTAG_CHAIN_PARAMS params = {0};
        params.chainLen   = chainLength;
        params.chipletSel = chiplet;
        params.instrId    = instruction;
        params.data       = LW_PTR_TO_LwP64(&((*pResult)[0]));
        params.dataArrayLen = (LwU32)pResult->size();
        rc = pLwRm->Control(hSubDevDiag,
                            LW208F_CTRL_CMD_GPU_GET_JTAG_CHAIN,
                            &params,
                            sizeof(params));
    }

    if (m_VerboseJtag)
    {
        Printf(Tee::PriNormal,
               "ReadHost2Jtag: Chiplet:0x%x Instr:0x%x"
               " Len:%d Data(maxBit-minBit):",
               chiplet, instruction, chainLength);
        for (size_t i = pResult->size(); i > 0; i--)
            Printf(Tee::PriNormal, "0x%08x ", (*pResult)[i-1]);
        Printf(Tee::PriNormal, "\n");
    }

    return rc;
}

RC GpuSubdevice::WriteHost2Jtag(UINT32 chiplet,
                                UINT32 instruction,
                                UINT32 chainLength,
                                const vector<UINT32> &inputData)
{
    RC rc;

    if (inputData.size() < (chainLength/32))
    {
        Printf(Tee::PriNormal, "input vector and chain length mismatch\n");
        return RC::BAD_PARAMETER;
    }

    if (m_VerboseJtag)
    {
        Printf(Tee::PriNormal,
               "WriteHost2Jtag: Chiplet:0x%x Instr:0x%x"
               " Len:%d Data(maxBit-minBit):",
               chiplet, instruction, chainLength);
        for (size_t i = inputData.size(); i > 0; i--)
            Printf(Tee::PriNormal, "0x%08x ", inputData[i-1]);
        Printf(Tee::PriNormal, "\n");
    }

    if (IsSOC())
    {
        CHECK_RC(CheetAh::SocPtr()->WriteHost2Jtag(chiplet,
                                                 instruction,
                                                 chainLength,
                                                 inputData));
    }
    else
    {

        LwRmPtr pLwRm;
        LwRm::Handle hSubDevDiag;
        hSubDevDiag = GetSubdeviceDiag();
        LW208F_CTRL_GPU_JTAG_CHAIN_PARAMS params = {0};
        params.chainLen   = chainLength;
        params.chipletSel = chiplet;
        params.instrId    = instruction;
        params.data       = LW_PTR_TO_LwP64(&inputData[0]);
        params.dataArrayLen = (LwU32)inputData.size();
        rc = pLwRm->Control(hSubDevDiag,
                            LW208F_CTRL_CMD_GPU_SET_JTAG_CHAIN,
                            &params,
                            sizeof(params));
    }
    return rc;
}

/*virtual*/
RC GpuSubdevice::ReadHost2Jtag(
    vector<JtagCluster> &jtagClusters   // vector of {chipletId,instrId} tuples
    ,UINT32 chainLen                    // number of bits in the chain
    ,vector<UINT32> *pResult            // location to store the data
    ,UINT32 capDis                      // sets the LW_PJTAG_ACCESS_CONFIG_CAPT_DIS bit, default=0
    ,UINT32 updtDis)                    // sets the LW_PJTAG_ACCESS_CONFIG_UPDT_DIS bit, default=0
{
    return RC::UNSUPPORTED_FUNCTION;
}

/*virtual*/
RC GpuSubdevice::WriteHost2Jtag(
     vector<JtagCluster> &jtagClusters   // vector of {chipletId,instrId} tuples
     ,UINT32 chainLen                    // number of bits in the chain
     ,const vector<UINT32> &inputData     // location to store the data
     ,UINT32 capDis                      // sets the LW_PJTAG_ACCESS_CONFIG_CAPT_DIS bit, default=0
     ,UINT32 updtDis)                    // sets the LW_PJTAG_ACCESS_CONFIG_UPDT_DIS bit, default=0
{
    return RC::UNSUPPORTED_FUNCTION;
}

//! Write a 32-bit value to a context switched register on the specified
//! context.
RC GpuSubdevice::CtxRegWr32(LwRm::Handle hCtx, UINT32 offset, UINT32 data, LwRm* pLwRm)
{
    LW2080_CTRL_GPU_REG_OP regWriteOp = { 0 };
    LW2080_CTRL_GPU_EXEC_REG_OPS_PARAMS params;
    RC rc;
    pLwRm = pLwRm ? pLwRm : LwRmPtr().Get();

    memset(&params, 0, sizeof(LW2080_CTRL_GPU_EXEC_REG_OPS_PARAMS));
    regWriteOp.regOp = LW2080_CTRL_GPU_REG_OP_WRITE_32;
    regWriteOp.regType = LW2080_CTRL_GPU_REG_OP_TYPE_GR_CTX;
    regWriteOp.regOffset = offset;
    regWriteOp.regValueHi = 0;
    regWriteOp.regValueLo = data;
    regWriteOp.regAndNMaskHi = 0;
    regWriteOp.regAndNMaskLo = ~0;

    params.regOpCount = 1;
    params.regOps = LW_PTR_TO_LwP64(&regWriteOp);
    params.hClientTarget = pLwRm->GetClientHandle();
    params.hChannelTarget = hCtx;

    rc = pLwRm->ControlBySubdevice(
                        this,
                        LW2080_CTRL_CMD_GPU_EXEC_REG_OPS,
                        &params,
                        sizeof(params));

    if (regWriteOp.regStatus != LW2080_CTRL_GPU_REG_OP_STATUS_SUCCESS)
    {
        Printf(Tee::PriError,
               "GpuSubdevice::CtxRegWr32 0x%08x: Reg Op Status = %d\n",
               offset, regWriteOp.regStatus);
        if (rc == OK)
            rc = RC::LWRM_ILWALID_PARAM_STRUCT;
    }

    return rc;
}

bool GpuSubdevice::EnablePowerGatingTestDuringRegAccess(bool bEnable)
{
    bool bReturn = m_bCheckPowerGating;
    m_bCheckPowerGating = bEnable;
    return bReturn;
}

bool GpuSubdevice::EnablePrivProtectedRegisters(bool enable)
{
    bool returlwalue = m_CheckPrivProtection;
    m_CheckPrivProtection = enable;
    return returlwalue;
}

//! Read a vga crtc register.
UINT08 GpuSubdevice::ReadCrtc(UINT08 index, INT32  head /* = 0 */)
{
    return ReadVGAReg(index, head, PROPERTY_CR_RD32);
}

//! Write a vga crtc register.
RC GpuSubdevice::WriteCrtc(UINT08 index, UINT08 data, INT32  head /* = 0 */)
{
    return WriteVGAReg(index, data, head, PROPERTY_CR_WR32);
}

//! Read a vga sequencer register.
UINT08 GpuSubdevice::ReadSr(UINT08 index, INT32 head /* = 0 */)
{
    return ReadVGAReg(index, head, PROPERTY_SR_RD32);
}

//! Write a vga sequencer register.
RC GpuSubdevice::WriteSr(UINT08 index, UINT08 data, INT32 head /* = 0 */)
{
    return WriteVGAReg(index, data, head, PROPERTY_SR_WR32);
}

//! Read a vga attribute register.
UINT08 GpuSubdevice::ReadAr(UINT08 index, INT32 head /* = 0 */)
{
    return ReadVGAReg(index, head, PROPERTY_AR_RD32);
}

//! Write a vga attribute register.
RC GpuSubdevice::WriteAr(UINT08 index, UINT08 data, INT32 head /* = 0 */)
{
    return WriteVGAReg(index, data, head, PROPERTY_AR_WR32);
}

//! Read a vga graphics controller register.
UINT08 GpuSubdevice::ReadGr(UINT08 index, INT32 head /* = 0 */)
{
    return ReadVGAReg(index, head, PROPERTY_GR_RD32);
}

//! Write a vga graphics controller register.
RC GpuSubdevice::WriteGr(UINT08 index, UINT08 data, INT32 head /* = 0 */)
{
    return WriteVGAReg(index, data, head, PROPERTY_GR_WR32);
}

//! This function writes a VGA crtc/sequencer/attribute/graphics controller reg
RC GpuSubdevice::WriteVGAReg
(
    UINT08 index,
    UINT08 data,
    INT32 head,
    UINT32 prop
)
{
    MASSERT((0 == head) || (1 == head));

    LwRmPtr pLwRm;
    LwU32 in[3] = {static_cast<LwU32>(head), index, data};
    LW_CFGEX_RESERVED_PROPERTY property;

    property.Property = prop;
    property.In       = LW_PTR_TO_LwP64(in);
    property.Out      = LW_PTR_TO_LwP64(0);

    return pLwRm->ConfigSetEx(LW_CFGEX_RESERVED, &property, sizeof(property),
                              this);
}

//! This function reads a VGA crtc/sequencer/attribute/graphics controller reg
RC GpuSubdevice::ReadVGAReg(UINT08 index, INT32 head, UINT32 prop)
{
    MASSERT((0 == head) || (1 == head));

    LwRmPtr pLwRm;
    LwU32 in[2]  = {static_cast<LwU32>(head), index};
    LwU32 out[1] = {0};
    LW_CFGEX_RESERVED_PROPERTY property;

    property.Property = prop;
    property.In       = LW_PTR_TO_LwP64(in);
    property.Out      = LW_PTR_TO_LwP64(out);

    if (OK != pLwRm->ConfigGetEx(LW_CFGEX_RESERVED,
                                 &property, sizeof(property), this))
    {
        return 0xFF;
    }

    return static_cast<UINT08>(out[0]);
}

/* virtual */
UINT32 GpuSubdevice::GetSpeedoCount()
{
    return GetFB()->GetFbioCount() + GetGpcCount() + GetSysPartitionCount();
}

/* virtual */
UINT32 GpuSubdevice::GetSysPartitionCount()
{
    //Sets the number of LW_PSPEEDO_SYS_C?_CTRL registers for different gpus.
    return 1;
}


//------------------------------------------------------------------------------
/*virtual*/
RC GpuSubdevice::ReadSpeedometers
(
    vector<UINT32>* pSpeedos    //!< (out) measured ring cycles
    ,vector<IsmInfo>* pInfo//!< (out) uniquely identifies each ISM.
    ,PART_TYPES SpeedoType      //!< (in) what type of speedo to read
    ,vector<IsmInfo>* pSettings //!< (in) how to configure each ISM
    ,UINT32 DurationClkCycles   //!< (in)  sample len in cl cycles.
)
{
    UINT32 simMode = Platform::GetSimulationMode();
    if ( IsEmulation() || ( simMode == Platform::Fmodel) || (simMode == Platform::Amodel))
    {
        // On emulation we never have a netlist to represent the entire GPU. Since
        // the ISM chains span across hardware blocks, not having all of the blocks
        // will actually break (cut off) part of a chain.So don't waist any time
        // trying to read broken chains.
        // For FModel & Amodel I don't think support has ever been added. However RTL does
        // have support and they want to start using it.
        pSpeedos->clear();
        switch (SpeedoType)
        {
            case BIN:
            case AGING:
            case BIN_AGING:
            case COMP:
            case METAL:
            case MINI:
            case TSOSC:
            case VNSENSE:
            case NMEAS:
            case AGING2:
            case HOLD:
            case IMON:
                if (m_pISM.get())
                {
                    pSpeedos->resize(m_pISM->GetNumISMs(SpeedoType), 0);
                }
                else
                    return RC::UNSUPPORTED_FUNCTION;
                break;

            default:    // CPM, CTRL, ISINK, ALL
                Printf(Tee::PriNormal, "SpeedoType:%s not supported in ReadSpeedometers().\n",
                       IsmSpeedo::SpeedoPartTypeToString(SpeedoType));
                return RC::UNSUPPORTED_FUNCTION;
        }
        return OK;
    }
    if (m_pISM.get()) // SpeedoType = BIN/COMP/METAL/MINI/TSOSC/VNSENSE/AGING
    {
        return m_pISM->ReadISMSpeedometers(pSpeedos, pInfo,
                        SpeedoType, pSettings, DurationClkCycles);
    }
    else
        return RC::UNSUPPORTED_FUNCTION;
}

RC GpuSubdevice::SetIntThermCalibration(UINT32 valueA, UINT32 valueB)
{
    RegHal &regs = Regs();
    UINT32 LwFuseOptIntTsA = regs.Read32(MODS_FUSE_OPT_INT_TS_A);
    UINT32 LwFuseOptIntTsB = regs.Read32(MODS_FUSE_OPT_INT_TS_B);
    if ((LwFuseOptIntTsA != 0) && (LwFuseOptIntTsB != 0))
    {
        Printf(Tee::PriNormal, "Warning: Thermal Fuses already calibrated. Will use fuse values\n");
        Printf(Tee::PriNormal, "LW_FUSE_OPT_INT_TS_A = 0x%x, "
                               "LW_FUSE_OPT_INT_TS_B = 0x%x\n",
                                LwFuseOptIntTsA, LwFuseOptIntTsB);
        return OK;
    }

    if ((valueA > 0xFFFF) || (valueB > 0xFFFF))
    {
        return RC::BAD_PARAMETER;
    }

    // set int sensor's temperature equation to be obtained from SW
    UINT32 LwThermSensor1 = regs.Read32(MODS_THERM_SENSOR_1);
    regs.SetField(&LwThermSensor1, MODS_THERM_SENSOR_1_SELECT_SW_A_ON);
    regs.SetField(&LwThermSensor1, MODS_THERM_SENSOR_1_SELECT_SW_B_ON);

    regs.Write32(MODS_THERM_SENSOR_1, LwThermSensor1);

    UINT32 LwThermSensor2 = regs.Read32(MODS_THERM_SENSOR_2);

    regs.SetField(&LwThermSensor2, MODS_THERM_SENSOR_2_SW_A, valueA);
    regs.SetField(&LwThermSensor2, MODS_THERM_SENSOR_2_SW_B, valueB);

    regs.Write32(MODS_THERM_SENSOR_2, LwThermSensor2);

    return OK;
}

//-----------------------------------------------------------------------------
RC GpuSubdevice::SetOverTempRange(OverTempCounter::TempType overTempType, INT32 min, INT32 max)
{
    Thermal* pTherm = GetThermal();
    if (!pTherm)
        return RC::UNSUPPORTED_FUNCTION;

    return pTherm->SetOverTempRange(overTempType, min, max);
}

//-----------------------------------------------------------------------------
RC GpuSubdevice::GetOverTempCounter(UINT32 *pCount)
{
    Thermal* pTherm = GetThermal();
    if (!pTherm)
        return RC::UNSUPPORTED_FUNCTION;

    return pTherm->GetOverTempCounter(pCount);
}

//-----------------------------------------------------------------------------
RC GpuSubdevice::GetExpectedOverTempCounter(UINT32 *pCount)
{
    Thermal* pTherm = GetThermal();
    if (!pTherm)
        return RC::UNSUPPORTED_FUNCTION;

    return pTherm->GetExpectedOverTempCounter(pCount);
}

namespace
{

struct ClkSrcEntry
{
    UINT32 ctrl2080Bit;
    Gpu::ClkSrc clkSrc;
};

constexpr std::array<ClkSrcEntry, 21> s_ClkSrcs =
{
    {
        { LW2080_CTRL_CLK_SOURCE_DEFAULT,    Gpu::ClkSrcDefault    }
       ,{ LW2080_CTRL_CLK_SOURCE_MPLL,       Gpu::ClkSrcMPLL       }
       ,{ LW2080_CTRL_CLK_SOURCE_VPLL0,      Gpu::ClkSrcVPLL0      }
       ,{ LW2080_CTRL_CLK_SOURCE_VPLL1,      Gpu::ClkSrcVPLL1      }
       ,{ LW2080_CTRL_CLK_SOURCE_SPPLL0,     Gpu::ClkSrcSPPLL0     }
       ,{ LW2080_CTRL_CLK_SOURCE_SPPLL1,     Gpu::ClkSrcSPPLL1     }
       ,{ LW2080_CTRL_CLK_SOURCE_XCLK,       Gpu::ClkSrcXCLK       }
       ,{ LW2080_CTRL_CLK_SOURCE_PEXREFCLK,  Gpu::ClkSrcPEXREFCLK  }
       ,{ LW2080_CTRL_CLK_SOURCE_XTAL,       Gpu::ClkSrcXTAL       }
       ,{ LW2080_CTRL_CLK_SOURCE_3XXCLKDIV2, Gpu::ClkSrcXCLK3XDIV2 }
       ,{ LW2080_CTRL_CLK_SOURCE_GPCPLL,     Gpu::ClkSrcGPCPLL     }
       ,{ LW2080_CTRL_CLK_SOURCE_LTCPLL,     Gpu::ClkSrcLTCPLL     }
       ,{ LW2080_CTRL_CLK_SOURCE_XBARPLL,    Gpu::ClkSrcXBARPLL    }
       ,{ LW2080_CTRL_CLK_SOURCE_SYSPLL,     Gpu::ClkSrcSYSPLL     }
       ,{ LW2080_CTRL_CLK_SOURCE_XTAL4X,     Gpu::ClkSrcXTAL4X     }
       ,{ LW2080_CTRL_CLK_SOURCE_REFMPLL,    Gpu::ClkSrcREFMPLL    }
       ,{ LW2080_CTRL_CLK_SOURCE_HOSTCLK,    Gpu::ClkSrcHOSTCLK    }
       ,{ LW2080_CTRL_CLK_SOURCE_XCLK500,    Gpu::ClkSrcXCLK500    }
       ,{ LW2080_CTRL_CLK_SOURCE_XCLKGEN3,   Gpu::ClkSrcXCLKGEN3   }
       ,{ LW2080_CTRL_CLK_SOURCE_DISPPLL,    Gpu::ClkSrcDISPPLL    }
       ,{ LW2080_CTRL_CLK_SOURCE_HBMPLL,     Gpu::ClkSrcHBMPLL     }
    }
};

}

Gpu::ClkSrc GpuSubdevice::Ctrl2080ClkSrcToClkSrc(UINT32 i)
{
    for (const auto& srcEntry : s_ClkSrcs)
    {
        if (srcEntry.ctrl2080Bit == i)
            return srcEntry.clkSrc;
    }
    return Gpu::ClkSrcDefault;
}

UINT32 GpuSubdevice::ClkSrcToCtrl2080ClkSrc(Gpu::ClkSrc src)
{
    for (const auto& srcEntry : s_ClkSrcs)
    {
        if (srcEntry.clkSrc == src)
            return srcEntry.ctrl2080Bit;
    }
    return LW2080_CTRL_CLK_SOURCE_DEFAULT;
}

bool GpuSubdevice::CanClkDomainBeProgrammed(Gpu::ClkDomain cd) const
{
    return RmHasClkDomain(cd, CLK_DOMAIN_WRITABLE);
}

bool GpuSubdevice::HasDomain (Gpu::ClkDomain cd) const
{
    return RmHasClkDomain(cd, CLK_DOMAIN_READABLE);
}

bool GpuSubdevice::RmHasClkDomain
(
    Gpu::ClkDomain cd,
    ClkDomainProp domainProp
) const
{
    if (Platform::IsVirtFunMode())
    {
        return false;
    }

    // On CheetAh we have a special way to control the GPU clock
    if (Platform::UsesLwgpuDriver())
    {
        return (cd == Gpu::ClkGpc2) || (cd == Gpu::ClkGpc);
    }

    LwRmPtr pLwRm;

    LW2080_CTRL_CLK_CLK_DOMAINS_INFO clkDomainsInfo = {};
    if (OK != LwRmPtr()->ControlBySubdevice(this,
                                            LW2080_CTRL_CMD_CLK_CLK_DOMAINS_GET_INFO,
                                            &clkDomainsInfo,
                                            sizeof(clkDomainsInfo)))
    {
        return false;
    }

    UINT32 clkBit = PerfUtil::GpuClkDomainToCtrl2080Bit(cd);
    Gpu::ClkDomain opDom = PerfUtil::ColwertToOppositeClockDomain(cd);
    UINT32 opClkBit = PerfUtil::GpuClkDomainToCtrl2080Bit(opDom);

    if (domainProp == CLK_DOMAIN_WRITABLE)
    {
        if (clkDomainsInfo.programmableDomains & (clkBit | opClkBit))
            return true;
    }
    else if (domainProp == CLK_DOMAIN_READABLE)
    {
        if (clkDomainsInfo.vbiosDomains &
            clkDomainsInfo.readableDomains &
            (clkBit | opClkBit))
            return true;
    }
    else
    {
        MASSERT(!"Unknown clock domain type\n");
    }

    return false;
}

//! Set the clocks that RM deem settable
RC GpuSubdevice::SetClock(Gpu::ClkDomain clkDomain, UINT64 targetHz)
{
    Perf* pPerf = GetPerf();
    if (!pPerf)
    {
        Printf(Tee::PriError, "Perf disabled - cannot set clock\n");
        return RC::SOFTWARE_ERROR;
    }

    RC rc;

    // Get the current supported Pstate
    UINT32 pstateNum = 0;
    CHECK_RC(pPerf->GetLwrrentPState(&pstateNum));
    return SetClock(clkDomain, targetHz, pstateNum, 0);
}

//--------------------------------------------------------------------
//! \brief Set the clocks that RM deem settable
//!
//! \param clkDomain Which clock to set
//! \param targetHz Desired clock frequency
//! \param PStateNum Which p-state to set the clock in, or
//!     Perf::ILWALID_PSTATE to use the current p-state
//! \param flags Flags from the Perf::ClkFlags enum, or-ed together
//!
/* virtual */ RC GpuSubdevice::SetClock
(
    Gpu::ClkDomain clkDomain,
    UINT64 targetHz,
    UINT32 pstateNum,
    UINT32 flags
)
{
    UINT32 pmuBootMode = 0;
    if (OK == Registry::Read("ResourceManager", "RmPmuBootstrapMode", &pmuBootMode))
    {
        if (pmuBootMode != 0)
        {
            Printf(Tee::PriDebug, "PMU not bootstraped - cannot set clock\n");
            return OK;
        }
    }

    RC rc;
    LwRmPtr pLwRm;
    Perf* pPerf = GetPerf();
    if (!pPerf)
    {
        Printf(Tee::PriError, "Perf disabled - cannot set clock\n");
        return RC::SOFTWARE_ERROR;
    }

    // Make sure it's a valid RM clock domain
    FLOAT32 freqRatio = 1.0f;
    if (!pPerf->IsRmClockDomain(clkDomain))
    {
        freqRatio = PerfUtil::GetModsToRmFreqRatio(clkDomain);
        clkDomain = PerfUtil::ColwertToOppositeClockDomain(clkDomain);
    }
    targetHz = static_cast<UINT64>(targetHz * freqRatio);

    if (!pPerf->CanModifyThisPState(pstateNum))
    {
        Printf(Tee::PriError, "Cannot modify pstate %d\n",
               static_cast<int>(pstateNum));
        return RC::UNSUPPORTED_FUNCTION;
    }

    if (pPerf->CanSetClockDomailwiaPState(clkDomain) &&
        pstateNum != Perf::ILWALID_PSTATE)
    {
        // In PStates 3.0, to set a new clock, we just need to add
        // an entry to the Clks array for the current PerfPoint or
        // modify the existing entry for that domain and then
        // call Perf::SetPerfPoint() to go to the new frequency.
        if (pPerf->IsPState30Supported() || pPerf->IsPState35Supported())
        {
            Perf::PerfPoint pt;
            CHECK_RC(pPerf->GetLwrrentPerfPoint(&pt));

            // Make sure we actually switch the PState
            pt.PStateNum = pstateNum;

            Perf::ClkSetting clk =
                Perf::ClkSetting(clkDomain, targetHz, flags, false);
            pt.Clks[clkDomain] = clk;

            CHECK_RC(pPerf->SetPerfPoint(pt));
        }
        else
        {
            // In PStates 2.0, update RM's PState tables with clock override
            CHECK_RC(pPerf->SetClockDomailwiaPState(clkDomain,
                                                    targetHz,
                                                    flags));
        }
    }
    else
    {
        const UINT64 targetkHz = clkDomain == Gpu::ClkPexGen ? targetHz : targetHz / 1000;
        CHECK_RC(pPerf->ForceClock(clkDomain, targetkHz, flags));
    }

    return rc;
}

// !Read a GPU clock domain.
/* virtual */ RC GpuSubdevice::GetClock
(
    Gpu::ClkDomain clkToGet,
    UINT64 * pActualPllHz,
    UINT64 * pTargetPllHz,
    UINT64 * pSlowedHz,
    Gpu::ClkSrc * pSrc
)
{
    if (Platform::UsesLwgpuDriver())
    {
        UINT64 freqHz = 0;
        if (clkToGet == Gpu::ClkGpc2 || clkToGet == Gpu::ClkGpc)
        {
            RC rc;
            CHECK_RC(m_pPerf->GetClockAtPState(0, clkToGet, &freqHz));
        }
        else if (clkToGet == Gpu::ClkM)
        {
            freqHz = CheetAh::SocPtr()->GetRAMFreqHz();
        }
        if (pActualPllHz)
            *pActualPllHz = freqHz;
        if (pTargetPllHz)
            *pTargetPllHz = freqHz;
        if (pSlowedHz)
            *pSlowedHz = freqHz;
        if (pSrc)
            *pSrc = Ctrl2080ClkSrcToClkSrc(0);
        return OK;
    }

    RC rc = OK;
    LwRmPtr pLwRm;
    Perf* pPerf = GetPerf();
    const Avfs* pAvfs = GetAvfs();
    UINT32 pmuBootMode = 0;
    if (OK == Registry::Read("ResourceManager", "RmPmuBootstrapMode", &pmuBootMode))
    {
        if (pmuBootMode != 0)
        {
            Printf(Tee::PriDebug, "PMU not bootstraped - cannot get clock\n");
            if (pActualPllHz != NULL)
                *pActualPllHz = 1;
            if (pTargetPllHz != NULL)
                *pTargetPllHz = 1;
            if (pSlowedHz != NULL)
                *pSlowedHz = 1;
            return OK;
        }
    }

    // Make sure it's a valid RM clock domain
    FLOAT32 freqRatio = 1.0f;
    if (pPerf && !pPerf->IsRmClockDomain(clkToGet))
    {
        freqRatio = PerfUtil::GetRmToModsFreqRatio(clkToGet);
        clkToGet = PerfUtil::ColwertToOppositeClockDomain(clkToGet);
    }

    LW2080_CTRL_CLK_GET_EXTENDED_INFO_PARAMS ctrlParams = {0};

    ctrlParams.flags = 0;
    ctrlParams.numClkInfos = 1;
    ctrlParams.clkInfos[0].clkInfo.clkDomain =
        PerfUtil::GpuClkDomainToCtrl2080Bit(clkToGet);

    if (HasDomain(clkToGet))
    {
        rc = pLwRm->Control(
            pLwRm->GetSubdeviceHandle(this),
            LW2080_CTRL_CMD_CLK_GET_EXTENDED_INFO,
            &ctrlParams,
            sizeof(ctrlParams));
    }
    // Else, silently fail -- CtrlInfo is all zeros.

    bool isClkPexGen = (clkToGet == Gpu::ClkPexGen) ? true : false;
    if (pActualPllHz)
    {
        *pActualPllHz = ctrlParams.clkInfos[0].clkInfo.actualFreq *
            (isClkPexGen ? 1ULL : 1000ULL);
        *pActualPllHz = static_cast<UINT64>(*pActualPllHz * freqRatio);
    }

    if (pTargetPllHz)
    {
        *pTargetPllHz = ctrlParams.clkInfos[0].clkInfo.targetFreq *
            (isClkPexGen ? 1ULL : 1000ULL);
        *pTargetPllHz = static_cast<UINT64>(*pTargetPllHz * freqRatio);
    }

    if (pSlowedHz)
    {
        *pSlowedHz = ctrlParams.clkInfos[0].effectiveFreq *
            (isClkPexGen ? 1ULL : 1000ULL);
        *pSlowedHz = static_cast<UINT64>(*pSlowedHz * freqRatio);

        // For PStates 3.0, use the clock counters to get the effective frequency
        // Report the average frequency based on all partitions
        if (pPerf && pPerf->IsPState30Supported())
        {
            bool canMeasure = false;

            if (pAvfs && pAvfs->IsNafllClkDomain(clkToGet))
            {
                canMeasure = true;
            }
            else
            {
                LW2080_CTRL_CLK_CLK_DOMAINS_INFO clkDomainsInfo = {};
                CHECK_RC(LwRmPtr()->ControlBySubdevice(this,
                                                       LW2080_CTRL_CMD_CLK_CLK_DOMAINS_GET_INFO,
                                                       &clkDomainsInfo,
                                                       sizeof(clkDomainsInfo)));
                if (clkDomainsInfo.clocksHAL >= LW2080_CTRL_CLK_CLK_DOMAIN_HAL_GV100)
                {
                    canMeasure = true;
                }
            }

            if (canMeasure)
            {
                vector<UINT64> effectiveFreqs;
                CHECK_RC(pPerf->MeasureClockPartitions(
                    clkToGet, pPerf->GetClockPartitionMask(clkToGet), &effectiveFreqs));
                *pSlowedHz = 0;
                for (UINT32 ii = 0; ii < effectiveFreqs.size(); ii++)
                {
                    *pSlowedHz += static_cast<UINT64>(effectiveFreqs[ii] * freqRatio);
                }
                if (!effectiveFreqs.empty())
                {
                    *pSlowedHz /= effectiveFreqs.size();
                }
                else
                {
                    Printf(Tee::PriError,
                           "Could not measure effective frequency for %s domain\n",
                           PerfUtil::ClkDomainToStr(clkToGet));
                    return RC::SOFTWARE_ERROR;
                }
            }
        }
    }

    if (pSrc)
    {
        if (pAvfs && pAvfs->IsNafllClkDomain(clkToGet))
        {
            *pSrc = Gpu::ClkSrcNAFLL;
        }
        else
        {
            *pSrc = Ctrl2080ClkSrcToClkSrc(ctrlParams.clkInfos[0].clkInfo.clkSource);
        }
    }

    return rc;
}

// Return the clock frequency for the specified clock source. Note this
// clock may not be one of the clock domains returned by the RM.
// Note: This API is deprecated and will not work for all clock sources starting with Volta.
// Use GetClockDomainFreqKHz() instead.
RC GpuSubdevice::GetClockSourceFreqKHz
(
    UINT32 clk2080ClkSource,
    LwU32 *pClkSrcFreqKHz
)
{
    RC rc;
    LwRmPtr pLwRm;
    LW2080_CTRL_CLK_GET_SRC_FREQ_KHZ_V2_PARAMS getSrcFreqParams = { };
    LW2080_CTRL_CLK_INFO *clkInfo = getSrcFreqParams.clkInfoList;

    clkInfo[0].clkSource = clk2080ClkSource;
    getSrcFreqParams.clkInfoListSize = 1;

    // get New clocks
    CHECK_RC(pLwRm->ControlBySubdevice(this,
                LW2080_CTRL_CMD_CLK_GET_SRC_FREQ_KHZ_V2,
                &getSrcFreqParams,
                sizeof (getSrcFreqParams)));

    *pClkSrcFreqKHz = clkInfo[0].actualFreq;
    return OK;
}

//-------------------------------------------------------------------------------------------------
// Return the clock frequency for the specified clock domain.
RC GpuSubdevice::GetClockDomainFreqKHz
(
    UINT32 clk2080ClkDomain,
    LwU32 *pClkDomainFreqKHz
)
{
    RC rc;
    LwRmPtr pLwRm;
    LW2080_CTRL_CLK_GET_INFO_V2_PARAMS getClkInfoParams = {};
    getClkInfoParams.clkInfoListSize = 1;
    getClkInfoParams.clkInfoList[0].clkDomain = clk2080ClkDomain;

    // get New clocks
    CHECK_RC(pLwRm->ControlBySubdevice(this,
                LW2080_CTRL_CMD_CLK_GET_INFO_V2,
                &getClkInfoParams,
                sizeof (getClkInfoParams)));
    *pClkDomainFreqKHz = getClkInfoParams.clkInfoList[0].actualFreq;
    return rc;
}

// !Returns the active PLL count for the given clock domain
RC GpuSubdevice::GetPllCount
(
    Gpu::ClkDomain clkToGet,
    UINT32 *pCount
)
{
    if (!HasDomain(clkToGet) || !pCount)
    {
        return RC::BAD_PARAMETER;
    }
    RC rc;
    LwRmPtr pLwRm;

    LW2080_CTRL_CLK_GET_PLL_COUNT_PARAMS params = {0};

    params.clkDomain = PerfUtil::GpuClkDomainToCtrl2080Bit(clkToGet);

    CHECK_RC(pLwRm->ControlBySubdevice(this,
        LW2080_CTRL_CMD_CLK_GET_PLL_COUNT,
        &params,
        sizeof(params)));

    *pCount = params.pllCount;

    return OK;
}

//------------------------------------------------------------------------------
//! \brief Set the frequency of the SysPll (*NOT* the SysClk)
//!
//! This is needed to shmoo clocks, since the SysPll is not usually
//! changed by other means.
//!
//! NOTE : Clocks routed to the SYSPLL prior to changing the value
//! SHOULD be rerouted back to the SYSPLL when this API finishes. However
//! if the desired SYSPLL frequency is close to any valid bypass
//! clock frequency, then the clocks could actually remain on the
//! bypass path if, due to rounding, the bypass path is actually
//! closer to the target frequency
//!
//! \param targetHz Target frequency in Hz for the SysPll
//!
//! \return OK if successful, not OK otherwise
RC GpuSubdevice::SetSysPll(UINT64 targetHz)
{
    LwRmPtr pLwRm;
    UINT32  clk;
    RC      rc;
    LW2080_CTRL_CLK_INFO clkInfo = { };
    LW2080_CTRL_CLK_SET_PLL_PARAMS pllParams = { };
    LW2080_CTRL_CLK_GET_DOMAINS_PARAMS domainParams = { };
    LW2080_CTRL_CLK_GET_EXTENDED_INFO_PARAMS clkInfoParams = { };

    // Step one : get the extended clock information for all programmable clocks
    domainParams.clkDomainsType = LW2080_CTRL_CLK_DOMAINS_TYPE_PROGRAMMABALE_ONLY;
    CHECK_RC(pLwRm->ControlBySubdevice(this,
                                       LW2080_CTRL_CMD_CLK_GET_DOMAINS,
                                       &domainParams,
                                       sizeof(domainParams)));

    clkInfoParams.flags = 0;
    clkInfoParams.numClkInfos = Utility::CountBits(domainParams.clkDomains);

    memset(&(clkInfoParams.clkInfos[0]), 0,
           clkInfoParams.numClkInfos * sizeof(LW2080_CTRL_CLK_EXTENDED_INFO));

    UINT32 lwrIndex = 0;
    for (clk = 0; clk < 32; clk++)
    {
        if (domainParams.clkDomains & (1 << clk))
        {
            clkInfoParams.clkInfos[lwrIndex++].clkInfo.clkDomain = (1 << clk);
        }
    }
    CHECK_RC(pLwRm->Control(pLwRm->GetSubdeviceHandle(this),
                            LW2080_CTRL_CMD_CLK_GET_EXTENDED_INFO,
                            &clkInfoParams,
                            sizeof(clkInfoParams)));

    // Step 2: Determine which of the programable clocks are lwrrently attached
    // to the SYSPLL and change their frequency to a value that will put them
    // on the bypass path
    vector<LW2080_CTRL_CLK_INFO> sysPllClocks;
    for (clk = 0; clk < clkInfoParams.numClkInfos; clk++)
    {
        if (clkInfoParams.clkInfos[clk].clkInfo.clkSource ==
            LW2080_CTRL_CLK_SOURCE_SYSPLL)
        {
            // Lwrrently there is no other way than a hardcoded number to
            // determine what frequency to use for the bypass path.  Once
            // PStates are implemented, we can find a PState where the clock
            // is on the bypass path and then use that frequency
            clkInfo.targetFreq = 405000;

            clkInfo.clkDomain = clkInfoParams.clkInfos[clk].clkInfo.clkDomain;
            sysPllClocks.push_back(clkInfo);
        }
    }

    // If there were clocks on the SYSPLL, set their frequency so that they are
    // now on the bypass path
    if (sysPllClocks.size())
    {
        LW2080_CTRL_CLK_SET_INFO_PARAMS setInfoParams = { };

        setInfoParams.flags = LW2080_CTRL_CLK_SET_INFO_FLAGS_WHEN_IMMEDIATE;
        setInfoParams.clkInfoList = LW_PTR_TO_LwP64(&sysPllClocks[0]);
        setInfoParams.clkInfoListSize = (LwU32)sysPllClocks.size();

        CHECK_RC(pLwRm->Control(
                 pLwRm->GetSubdeviceHandle(this),
                 LW2080_CTRL_CMD_CLK_SET_INFO,
                 &setInfoParams,
                 sizeof(setInfoParams)));
    }

    // Change the SYSPLL frequency
    clkInfo.clkDomain = sysPllClocks.size() ?
                                  sysPllClocks[0].clkDomain :
                                  LW2080_CTRL_CLK_DOMAIN_SYS2CLK;

    UINT64 targetFreq64 = (targetHz + 500) / 1000;
    clkInfo.targetFreq = UNSIGNED_CAST(LwU32, targetFreq64);
    clkInfo.clkSource = LW2080_CTRL_CLK_SOURCE_SYSPLL;
    pllParams.clkInfoListSize = 1;
    pllParams.clkInfoList = LW_PTR_TO_LwP64(&clkInfo);
    CHECK_RC(pLwRm->Control(pLwRm->GetSubdeviceHandle(this),
                            LW2080_CTRL_CMD_CLK_SET_PLL,
                            &pllParams,
                            sizeof(pllParams)));

    // Change the clock values to the frequency that the SYSPLL was just
    // set to.  This SHOULD reroute the clocks back to the SYSPLL.  Note
    // that if the desired SYSPLL frequency is close to any valid bypass
    // clock frequency, then the clocks could actually remain on the
    // bypass path if, due to rounding, the bypass path is actually
    // closer to the target frequency
    if (sysPllClocks.size())
    {
        LW2080_CTRL_CLK_SET_INFO_PARAMS setInfoParams = { };

        for (clk = 0; clk < sysPllClocks.size(); clk++)
        {
            sysPllClocks[clk].targetFreq = static_cast<LwU32>(targetFreq64);
        }
        setInfoParams.flags = LW2080_CTRL_CLK_SET_INFO_FLAGS_WHEN_IMMEDIATE;
        setInfoParams.clkInfoList = LW_PTR_TO_LwP64(&sysPllClocks[0]);
        setInfoParams.clkInfoListSize = (LwU32)sysPllClocks.size();

        CHECK_RC(pLwRm->Control(
                 pLwRm->GetSubdeviceHandle(this),
                 LW2080_CTRL_CMD_CLK_SET_INFO,
                 &setInfoParams,
                 sizeof(setInfoParams)));

        // Now validate that the clocks that were on SYSPLL are back onto SYSPLL
        // when the function exits.  If any of them are not on SYSPLL, print a
        // warning message
        memset(&(clkInfoParams.clkInfos[0]), 0,
               clkInfoParams.numClkInfos * sizeof(LW2080_CTRL_CLK_EXTENDED_INFO));

        clkInfoParams.numClkInfos = (LwU32)sysPllClocks.size();
        for (clk = 0; clk < sysPllClocks.size(); clk++)
        {
            clkInfoParams.clkInfos[clk].clkInfo.clkDomain =
                sysPllClocks[clk].clkDomain;
        }
        CHECK_RC(pLwRm->Control(pLwRm->GetSubdeviceHandle(this),
                                LW2080_CTRL_CMD_CLK_GET_EXTENDED_INFO,
                                &clkInfoParams,
                                sizeof(clkInfoParams)));

        bool bClockFound = false;
        for (clk = 0; clk < sysPllClocks.size(); clk++)
        {
            if (clkInfoParams.clkInfos[clk].clkInfo.clkSource !=
                LW2080_CTRL_CLK_SOURCE_SYSPLL)
            {
                Gpu::ClkDomain clkDom = PerfUtil::ClkCtrl2080BitToGpuClkDomain(
                        sysPllClocks[clk].clkDomain);
                Printf(Tee::PriWarn, "%s is no longer routed to SYSPLL\n",
                       PerfUtil::ClkDomainToStr(clkDom));
                bClockFound = true;
            }
        }
        if (bClockFound)
        {
            Printf(Tee::PriWarn,
                   "SYSPLL frequency too close to bypass clock.\n"
                   "          Change SYSPLL frequency and call SetClock to move"
                   " clocks back to SYSPLL\n");
        }
    }

    return rc;
}

/* virtual */
RC GpuSubdevice::GetWPRInfo(UINT32 regionID,
                            UINT32 *pReadMask,
                            UINT32 *pWriteMask,
                            UINT64 *pStartAddr,
                            UINT64 *pEndAddr)
{
    // The first supported GPU is GM20x
    return RC::UNSUPPORTED_FUNCTION;
}

/* virtual */
RC GpuSubdevice::GetFalconStatus(UINT32 falconId, UINT32 *pLevel)
{
    return RC::UNSUPPORTED_FUNCTION;
}

//! \brief Set a handler to call whenever an event oclwrs in the resman
//!
//! Colwenience method to specify a handler function to call whenever
//! a subdevice event oclwrs in the resman.  This method assumes that
//! only one handler (at most) is hooked to each event; specifying a
//! new handler simply replaces the old one.
//! \sa UnhookResmanEvent()
//!
//! \param Index Tells which event we're responding to
//!     (e.g. LW2080_NOTIFIERS_HOTPLUG).
//! \param Handler The function to call when the event oclwrs
//! \param Action Event notification action
//! \param notifierMemAction Setup notifier memory or not
RC GpuSubdevice::HookResmanEvent
(
    UINT32 index,
    void   (*Handler)(void*),
    void*  pArgs,
    UINT32 action,
    NotifierMemAction notifierMemAction
)
{
    RC rc;
    LwRmPtr pLwRm;

    // Create the EventHook object for this event if it doesn't
    // already exist.
    //
    if (m_eventHooks.count(index) == 0)
    {
        LwRm::Handle  hSubdevice = pLwRm->GetSubdeviceHandle(this);
        EventThread  *pEventThread = new EventThread();
        LwRm::Handle  hEvent = 0;

#if defined(TEGRA_MODS)
        if (CheetAh::SocPtr()->HasFeature(Device::SOC_SUPPORTS_DISPLAY_WITH_FE))
        {
            void* const pOsEvent = pEventThread->GetOsEvent(
                    pLwRm->GetDispClientHandle(),
                    pLwRm->GetDeviceHandle(GetParentDevice()));
            if (pOsEvent)
            {
                rc = pLwRm->AllocDispEvent(hSubdevice, &hEvent, LW01_EVENT_OS_EVENT,
                                           index, pOsEvent);
            }
        }
        else
#endif
        {
            void* const pOsEvent = pEventThread->GetOsEvent(
                    pLwRm->GetClientHandle(),
                    pLwRm->GetDeviceHandle(GetParentDevice()));
            if (pOsEvent)
            {
                rc = pLwRm->AllocEvent(hSubdevice, &hEvent, LW01_EVENT_OS_EVENT,
                                       index, pOsEvent);
            }
        }

        if (rc != OK)
        {
            delete pEventThread;
            return rc;
        }
        m_eventHooks[index].hEvent = hEvent;
        m_eventHooks[index].pEventThread = pEventThread;
    }

    // Set up notifier memory if it has not been set up
    //
    if (NOTIFIER_MEMORY_ENABLED == notifierMemAction)
    {
        if (!m_pNotifierSurface.get())
        {
            m_pNotifierSurface.reset(new Surface2D);
            m_pNotifierSurface->SetName("_ResmanEventNotifier");

            UINT32 eventMemSize =
                sizeof(LwNotification) * LW2080_NOTIFIERS_MAXCOUNT;
            eventMemSize = ALIGN_UP(eventMemSize,
                ColorUtils::PixelBytes(ColorUtils::VOID32));
            m_pNotifierSurface->SetWidth(eventMemSize /
                ColorUtils::PixelBytes(ColorUtils::VOID32));
            m_pNotifierSurface->SetPitch(eventMemSize);
            m_pNotifierSurface->SetHeight(1);
            m_pNotifierSurface->SetColorFormat(ColorUtils::VOID32);
            m_pNotifierSurface->SetLocation(Memory::Coherent);
            m_pNotifierSurface->SetAddressModel(Memory::Paging);
            m_pNotifierSurface->SetKernelMapping(true);

            CHECK_RC(m_pNotifierSurface->Alloc(GetParentDevice()));
            CHECK_RC(m_pNotifierSurface->Fill(0, GetSubdeviceInst()));
            CHECK_RC(m_pNotifierSurface->Map(GetSubdeviceInst()));
            m_pNotifierMem = (LwNotification *)m_pNotifierSurface->GetAddress();

            LW2080_CTRL_EVENT_SET_MEMORY_NOTIFIES_PARAMS memNotifiesParams
                = {0};
            memNotifiesParams.hMemory = m_pNotifierSurface->GetMemHandle();
            CHECK_RC(pLwRm->Control(pLwRm->GetSubdeviceHandle(this),
                LW2080_CTRL_CMD_EVENT_SET_MEMORY_NOTIFIES,
                &memNotifiesParams, sizeof(memNotifiesParams)));
        }

        MEM_WR16(&m_pNotifierMem[index].status,
                 LW2080_EVENT_MEMORY_NOTIFIES_STATUS_PENDING);
    }

    // Set up the new action & handler
    //
    LW2080_CTRL_EVENT_SET_NOTIFICATION_PARAMS params = {0};
    params.event  = index;
    params.action = action;
    CHECK_RC(pLwRm->ControlBySubdevice(this,
                                       LW2080_CTRL_CMD_EVENT_SET_NOTIFICATION,
                                       &params, sizeof(params)));

    CHECK_RC(m_eventHooks[index].pEventThread->SetHandler(Handler, pArgs));
    return rc;
}

//! Unhook a handler created by HookResmanEvent(), and free the
//! resources that were created by HookResmanEvent().
//!
//! \param Index Same as the Index parameter passed to HookResmanEvent()

RC GpuSubdevice::UnhookResmanEvent(UINT32 index)
{
    LwRmPtr pLwRm;
    RC rc;

    if (m_eventHooks.count(index) != 0)
    {
        // Clear the EventHook entry first, to make sure we won't
        // re-call this function if it aborts
        //
        EventHook eventHook = m_eventHooks[index];
        m_eventHooks.erase(index);

        // Tell the resman to disable this event notification
        //
        LW2080_CTRL_EVENT_SET_NOTIFICATION_PARAMS params = {0};
        params.event  = index;
        params.action = LW2080_CTRL_EVENT_SET_NOTIFICATION_ACTION_DISABLE;
        CHECK_RC(pLwRm->ControlBySubdevice(
                     this,
                     LW2080_CTRL_CMD_EVENT_SET_NOTIFICATION,
                     &params, sizeof(params)));

        // Halt the event-handling thread and clean up the resources
        //
        pLwRm->Free(eventHook.hEvent);
        CHECK_RC(eventHook.pEventThread->SetHandler(nullptr));
        delete eventHook.pEventThread;
    }

    return rc;
}

//! Read event data in notifier memory;
//! RM need to copy event data to the notifier memory.
//!
//! \param Index Tells which event we're responding to
RC GpuSubdevice::GetResmanEventData
(
    UINT32 index,
    LwNotification* pNotifierData
) const
{
    RC rc;

    if (m_pNotifierMem == nullptr)
    {
        return RC::SOFTWARE_ERROR;
    }

    LwV16 status = MEM_RD16(&m_pNotifierMem[index].status);
    if (status == LW2080_EVENT_MEMORY_NOTIFIES_STATUS_PENDING)
    {
        return RC::CANNOT_GET_ELEMENT;
    }

    // read data
    pNotifierData->info32 = MEM_RD32(&m_pNotifierMem[index].info32);
    pNotifierData->info16 = MEM_RD16(&m_pNotifierMem[index].info16);
    pNotifierData->status = status;
    pNotifierData->timeStamp.nanoseconds[0] =
        MEM_RD32(&m_pNotifierMem[index].timeStamp.nanoseconds[0]);
    pNotifierData->timeStamp.nanoseconds[1] =
        MEM_RD32(&m_pNotifierMem[index].timeStamp.nanoseconds[1]);

    // clear status
    MEM_WR16(&m_pNotifierMem[index].status,
             LW2080_EVENT_MEMORY_NOTIFIES_STATUS_PENDING);

    return rc;
}

//! \brief Whether the event has been hooked
//!
bool GpuSubdevice::IsResmanEventHooked(UINT32 index) const
{
    return m_eventHooks.count(index) != 0;
}

//! Get the board ID
UINT32 GpuSubdevice::BoardId()
{
    UINT32 boardId = 0;

    if (OK == LwRmPtr()->ConfigGet(LW_CFG_BOARD_ID, &boardId, this))
        return boardId;

    // This cfg-get failure is normal on older boards, i.e. lw18.
    return 0;
}
//------------------------------------------------------------------------------
RC GpuSubdevice::GetSkuInfoFromBios()
{
    if (m_ReadSkuInfoFromBios)
        return OK;

    RC rc;
    LW2080_CTRL_BIOS_GET_SKU_INFO_PARAMS params;
    memset(&params, 0, sizeof(params));
    CHECK_RC(LwRmPtr()->ControlBySubdevice(this,
                                           LW2080_CTRL_CMD_BIOS_GET_SKU_INFO,
                                           &params, sizeof(params)));
    m_ChipSkuId       = params.chipSKU;
    m_ChipSkuModifier = params.chipSKUMod;
    m_BoardNum        = params.project;
    m_BoardSkuNum     = params.projectSKU;
    m_BoardSkuMod     = params.projectSKUMod;
    m_BusinessCycle   = params.businessCycle;
    m_CDPN            = params.CDP;

    m_ReadSkuInfoFromBios = true;
    return rc;
}

//------------------------------------------------------------------------------
RC GpuSubdevice::GetBoardInfoFromBoardDB
(
    string *pBoardName,
    UINT32 *pBoardDefIndex,
    bool ignoreCache
)
{
    MASSERT(pBoardName && pBoardDefIndex);
    if (m_ReadBoardInfoFromBoardDB && !ignoreCache)
    {
        *pBoardName = m_BoardName;
        *pBoardDefIndex = m_BoardDefIndex;
        return OK;
    }

    RC rc;
    string boardName("unknown board");
    UINT32 boardDefIndex = 0;
    vector<BoardDB::BoardEntry> matchedBoardEntries;

    //First look for PERFECT physical match
    Printf(Tee::PriLow, "Looking for specific matches...\n");
    const BoardDB & boards = BoardDB::Get();
    CHECK_RC(boards.GetPhysicalMatch(this,
                                     BoardDB::PERFECT,
                                     BoardDB::REGEX_THAT,
                                     &matchedBoardEntries));

    if (matchedBoardEntries.size() == 0)
    {
        //Try look for SVID_GENERIC physical match
        Printf(Tee::PriLow, "Looking for SVID specific matches...\n");
        CHECK_RC(boards.GetPhysicalMatch(this,
                                         BoardDB::SVID_GENERIC,
                                         BoardDB::REGEX_THAT,
                                         &matchedBoardEntries));

        //Try look for GENERIC physical match
        Printf(Tee::PriLow, "Looking for generic matches...\n");
        if (matchedBoardEntries.size() == 0)
        {
            //Try look for SVID_GENERIC physical match
            CHECK_RC(boards.GetPhysicalMatch(this,
                                             BoardDB::GENERIC,
                                             BoardDB::REGEX_THAT,
                                             &matchedBoardEntries));
        }
    }
    if (matchedBoardEntries.size() == 1)
    {
        boardName = matchedBoardEntries[0].m_BoardName;
        boardDefIndex = matchedBoardEntries[0].m_BoardDefIndex;

    }
    else if (matchedBoardEntries.size() > 1)
    {
        Printf(Tee::PriNormal, "Duplicate board descriptions:\n");
        for (UINT32 i = 0; i < matchedBoardEntries.size(); i++)
        {
            Printf(Tee::PriNormal, "    %s[%d]\n",
                matchedBoardEntries[i].m_BoardName.c_str(),
                matchedBoardEntries[i].m_BoardDefIndex);

        }
        return RC::DUPLICATE_BOARD_DESCRIPTION;
    }
    else
    {
        //If get to this point, no match
        if (Platform::GetSimulationMode() == Platform::Fmodel)
        {
            // Useful for modstest.js on fmodel
            boardName = Gpu::DeviceIdToString(DeviceId());
            boardName += "-SIM";
        }
        else
        {
            boardName = "unknown board";
        }
    }

    *pBoardName = boardName;
    *pBoardDefIndex = boardDefIndex;
    if (!ignoreCache)
    {
        m_BoardName     = boardName;
        m_BoardDefIndex = boardDefIndex;
        m_ReadBoardInfoFromBoardDB = true;
    }

    return OK;
}

//------------------------------------------------------------------------------
string GpuSubdevice::GetBoardNumber()
{
    if (OK != GetSkuInfoFromBios())
        return "";
    return m_BoardNum;
}
string GpuSubdevice::GetBoardSkuNumber()
{
    if (OK != GetSkuInfoFromBios())
        return "";
    return m_BoardSkuNum;
}
string GpuSubdevice::GetBoardSkuMod()
{
    if (OK != GetSkuInfoFromBios())
        return "";
    return m_BoardSkuMod;
}
string GpuSubdevice::GetCDPN()
{
    if (OK != GetSkuInfoFromBios())
        return "";
    return m_CDPN;
}
UINT32 GpuSubdevice::GetBusinessCycle()
{
    if (OK != GetSkuInfoFromBios())
    {
        Printf(Tee::PriError, "cannot retrieve info from BIOS\n");
        return 0;
    }
    return m_BusinessCycle;
}

//------------------------------------------------------------------------------
UINT32 GpuSubdevice::RopCount() const
{
    if (!IsInitialized())
    {
        return 0;
    }

    // GM20x (and followon chips) sweep LTC and ROP together.
    return GetFB()->GetLtcCount();
}

//------------------------------------------------------------------------------
UINT32 GpuSubdevice::RopPixelsPerClock() const
{
    return 8;
}

//------------------------------------------------------------------------------
UINT32 GpuSubdevice::ShaderCount() const
{
    // SM floorsweeping is not supported, if it is then this becomes more complex
    // (iterate through each TPC and sum the shaders)
    return GetTpcCount() * GetMaxSmPerTpcCount();
}

//------------------------------------------------------------------------------
UINT32 GpuSubdevice::VpeCount() const
{
    if (!IsInitialized())
    {
        return 0;
    }

    // G80+ uses TPCs instead of VPEs, return the number of TPCs here
    return GetTpcCount();
}

//------------------------------------------------------------------------------
UINT32 GpuSubdevice::GetFrameBufferUnitCount() const
{
    return Utility::CountBits(m_pFsImpl->FbMask());
}

//------------------------------------------------------------------------------
UINT32 GpuSubdevice::GetFrameBufferIOCount() const
{
    return Utility::CountBits(m_pFsImpl->FbioMask());
}

//------------------------------------------------------------------------------
UINT32 GpuSubdevice::GetGpcCount() const
{
    return Utility::CountBits(m_pFsImpl->GpcMask());
}

//------------------------------------------------------------------------------
UINT32 GpuSubdevice::GetMaxGpcCount() const
{
    if (!m_MaxGpcCount)
    {
        if (!Platform::IsVirtFunMode())
        {
            m_MaxGpcCount = (Regs().IsSupported(MODS_PTOP_SCAL_NUM_GPCS) ?
                             Regs().Read32(MODS_PTOP_SCAL_NUM_GPCS) : 0);
        }
        else
        {
            RC rc = ReadPfRegFromVf(MODS_PTOP_SCAL_NUM_GPCS, &m_MaxGpcCount);
            rc.Clear(); // Ignore the error code
        }
    }

    if (!m_MaxGpcCount)
    {
        Printf(Tee::PriWarn, "Max GPC count is 0\n");
    }

    return m_MaxGpcCount;
}

//------------------------------------------------------------------------------
RC GpuSubdevice::SaveFBConfig(FBConfigData* pData)
{
    pData->readFtCfg    = Regs().Read32(MODS_PFB_FBPA_READ_FT_CFG);
    pData->readBankQCfg = Regs().Read32(MODS_PFB_FBPA_2_READ_BANKQ_CFG);
    return OK;
}

//------------------------------------------------------------------------------
RC GpuSubdevice::RestoreFBConfig(const FBConfigData& data)
{
    Regs().Write32(MODS_PFB_FBPA_READ_FT_CFG,      data.readFtCfg);
    Regs().Write32(MODS_PFB_FBPA_2_READ_BANKQ_CFG, data.readBankQCfg);
    return OK;
}

//------------------------------------------------------------------------------
RC GpuSubdevice::EnableFBRowDebugMode()
{
    // Disable row coalescing and enable row activation for every access
    Regs().Write32(MODS_PFB_FBPA_READ_FT_CFG_MAX_CHAIN, 1);
    Regs().Write32(MODS_PFB_FBPA_2_READ_BANKQ_CFG_BANK_HIT_DISABLED);

    return OK;
}

//------------------------------------------------------------------------------
UINT32 GpuSubdevice::GetMaxPceCount() const
{
    return 0;
}

//------------------------------------------------------------------------------
UINT32 GpuSubdevice::GetMaxGrceCount() const
{
    return 0;
}

//------------------------------------------------------------------------------
vector<UINT32> GpuSubdevice::GetActiveSmcVirtualGpcs() const
{
    MASSERT(IsSMCEnabled());
    MASSERT(UsingMfgModsSMC()); // Relies on GpuSubdevice owning the SMC topology

    // NOTE: GPCs are assigned to partitions or are unpartitioned. Active GPCs
    // are those in a partition that are assigned to an SMC engine.
    //
    // Partitioning and GPC engine assignment cannot change after GPU
    // initialization.

    vector<UINT32> virtGpcs;
    virtGpcs.reserve(GetGpcCount());

    for (const auto& smcTopoIt : m_SmcTopology)
    {
        const SmcPartTopo& partitionTopo = smcTopoIt.second;
        for (const SmcEngineTopo& engineTopo : partitionTopo.smcEngines)
        {
            for (const SmcGpcTopo& gpcTopo : engineTopo.gpcs)
            {
                virtGpcs.push_back(gpcTopo.virtGpc);
            }
        }
    }

    return virtGpcs;
}

//------------------------------------------------------------------------------
RC GpuSubdevice::GetPceLceMapping(LW2080_CTRL_CE_SET_PCE_LCE_CONFIG_PARAMS* pParams)
{
    return RC::UNSUPPORTED_FUNCTION;
}

//------------------------------------------------------------------------------
UINT32 GpuSubdevice::GetTexCountPerTpc() const
{
    // See decoder ring, or the number of pipes in
    // LW_PGRAPH_PRI_GPC0_TPC0_TEX_TRM_DBG_ROUTING
    return 2;
}

//------------------------------------------------------------------------------
UINT32 GpuSubdevice::GetTpcCount() const
{
    if (!m_TpcCount)
    {
        LW2080_CTRL_GR_INFO grInfo[1] = {};
        grInfo[0].index = LW2080_CTRL_GR_INFO_INDEX_SHADER_PIPE_SUB_COUNT;

        LW2080_CTRL_GR_GET_INFO_PARAMS params = {};
        params.grInfoListSize = static_cast<LwU32>(NUMELEMS(grInfo));
        params.grInfoList = LW_PTR_TO_LwP64(&grInfo[0]);

        if (IsSMCEnabled())
        {
// Mac does not have the grRouteInfo params for LW2080_CTRL_GR_GET_INFO_PARAMS
#if !defined(MACOSX_MFG)
            SetGrRouteInfo(&(params.grRouteInfo), GetSmcEngineIdx());
#endif
        }
        if (RC::OK != LwRmPtr()->ControlBySubdevice(this,
                                                    LW2080_CTRL_CMD_GR_GET_INFO,
                                                    &params,
                                                    sizeof(params)))
        {
            Printf(Tee::PriWarn, "TPC Count is 0\n");
            return 0;
        }
        m_TpcCount = grInfo[0].data;
    }

    return m_TpcCount;
}

//------------------------------------------------------------------------------
UINT32 GpuSubdevice::GetTpcCountOnGpc(UINT32 virtualGpcNum) const
{
    UINT32 hwGpcNum;
    if (OK == VirtualGpcToHwGpc(virtualGpcNum, &hwGpcNum))
    {
        return Utility::CountBits(GetFsImpl()->TpcMask(hwGpcNum));
    }
    else
    {
        MASSERT(!"Bad GPC num passed to GpuSubdevice::GetTpcCountOnGpc");
        return 0;
    }
}

//------------------------------------------------------------------------------
UINT32 GpuSubdevice::GetPesCountOnGpc(UINT32 virtualGpcNum) const
{
    UINT32 hwGpcNum;
    if (RC::OK == VirtualGpcToHwGpc(virtualGpcNum, &hwGpcNum))
    {
        return Utility::CountBits(GetFsImpl()->PesMask(hwGpcNum));
    }
    else
    {
        MASSERT(!"Bad GPC num passed to GpuSubdevice::GetPesCountOnGpc");
        return 0;
    }
}

//------------------------------------------------------------------------------
UINT32 GpuSubdevice::GetMaxTpcCount() const
{
    if (!m_MaxTpcCount)
    {
        if (!Platform::IsVirtFunMode())
        {
            m_MaxTpcCount = (Regs().IsSupported(MODS_PTOP_SCAL_NUM_TPC_PER_GPC) ?
                             Regs().Read32(MODS_PTOP_SCAL_NUM_TPC_PER_GPC) : 0);
        }
        else
        {
            RC rc = ReadPfRegFromVf(MODS_PTOP_SCAL_NUM_TPC_PER_GPC, &m_MaxTpcCount);
            rc.Clear(); // Ignore the error code
        }
    }

    if (!m_MaxTpcCount)
    {
        Printf(Tee::PriWarn, "Max TPC count is 0\n");
    }

    return m_MaxTpcCount;
}

UINT32 GpuSubdevice::GetMaxPesCount() const
{
    if (Regs().IsSupported(MODS_PTOP_SCAL_NUM_PES_PER_GPC))
    {
        return Regs().Read32(MODS_PTOP_SCAL_NUM_PES_PER_GPC);
    }
    return 0;
}

UINT32 GpuSubdevice::GetMaxZlwllCount() const
{
    // For fermi and kepler, return # of tpcs so zlwlls == tpcs.  Later chips
    // decouple these, so we'll use the actual register to get the number of
    // zlwlls.
    return Regs().Read32(MODS_PTOP_SCAL_NUM_TPC_PER_GPC);
}

UINT32 GpuSubdevice::GetMaxSmPerTpcCount() const
{
    if (!m_MaxSmPerTpcCount)
    {
        LW2080_CTRL_GR_INFO grInfo[1] = {};
        grInfo[0].index = LW2080_CTRL_GR_INFO_INDEX_LITTER_NUM_SM_PER_TPC;

        LW2080_CTRL_GR_GET_INFO_PARAMS params = {};
        params.grInfoListSize = static_cast<LwU32>(NUMELEMS(grInfo));
        params.grInfoList = LW_PTR_TO_LwP64(&grInfo[0]);

        if (IsSMCEnabled())
        {
// Mac does not have the grRouteInfo params for LW2080_CTRL_GR_GET_INFO_PARAMS
#if !defined(MACOSX_MFG)
            SetGrRouteInfo(&(params.grRouteInfo), GetSmcEngineIdx());
#endif
        }
        if (RC::OK != LwRmPtr()->ControlBySubdevice(this,
                                                    LW2080_CTRL_CMD_GR_GET_INFO,
                                                    &params,
                                                    sizeof(params)))
        {
            Printf(Tee::PriWarn, "Max SMs per TPC Count is 0\n");
            return 0;
        }
        m_MaxSmPerTpcCount = grInfo[0].data;
    }

    return m_MaxSmPerTpcCount;
}

UINT32 GpuSubdevice::GetMaxFbpCount() const
{
    return Regs().Read32(MODS_PTOP_SCAL_NUM_FBPS);
}

UINT32 GpuSubdevice::GetMaxLwencCount() const
{
    return (Regs().IsSupported(MODS_PTOP_SCAL_NUM_LWENCS) ?
            Regs().Read32(MODS_PTOP_SCAL_NUM_LWENCS) : 0);
}

UINT32 GpuSubdevice::GetMaxLwdecCount() const
{
    return (Regs().IsSupported(MODS_PTOP_SCAL_NUM_LWDECS) ?
            Regs().Read32(MODS_PTOP_SCAL_NUM_LWDECS) : 0);
}

UINT32 GpuSubdevice::GetMaxLwjpgCount() const
{
    return (Regs().IsSupported(MODS_PTOP_SCAL_NUM_LWJPGS) ?
            Regs().Read32(MODS_PTOP_SCAL_NUM_LWJPGS) : 0);
}

UINT32 GpuSubdevice::GetMaxL2Count() const
{
    // We're actually returning the number per FBP
    return (Regs().IsSupported(MODS_PTOP_SCAL_NUM_LTC_PER_FBP) ?
            Regs().Read32(MODS_PTOP_SCAL_NUM_LTC_PER_FBP) : 0);
}

UINT32 GpuSubdevice::GetMaxOfaCount() const
{
    return 0;
}

UINT32 GpuSubdevice::GetMaxSyspipeCount() const
{
    return 0;
}

UINT32 GpuSubdevice::GetMaxLwlinkCount() const
{
    return 0;
}

UINT32 GpuSubdevice::GetMaxPerlinkCount() const
{
    return 0;
}

UINT32 GpuSubdevice::GetMaxHsHubCount() const
{
    // This RM control call is not supported on amodel and iGpu
    if (Platform::GetSimulationMode() != Platform::Amodel && !IsSOC())
    {
        LwRmPtr pLwRm;
        RC rc;
        LW208F_CTRL_MMU_GET_NUM_HSHUBMMUS_PARAMS params = {};
        const LwRm::Handle hSubDevDiag = GpuSubdevice::GetSubdeviceDiag();

        rc = pLwRm->Control(hSubDevDiag,
                       (UINT32)LW208F_CTRL_CMD_MMU_GET_NUM_HSHUBMMUS,
                       &params,
                       sizeof(params));
        if (OK == rc)
        {
            return params.numHshubmmus;
        }
    }
    // Order matters here, if the size register is present use it, if not present
    // but an HSHUB register exists it must be on a chip with only 1 HSHUB
    if (Regs().IsSupported(MODS_PFB_HSHUB__SIZE_1))
        return Regs().LookupAddress(MODS_PFB_HSHUB__SIZE_1);
    if (Regs().IsSupported(MODS_PFB_HSHUB_IG_CE2HSH_CFG))
        return 1;
    return 0;
}

/* virtual */ UINT32 GpuSubdevice::GetMaxShaderCount() const
{
    return (GetMaxTpcCount() * GetMaxGpcCount() * GetMaxSmPerTpcCount());
}

double GpuSubdevice::GetArchEfficiency(Instr type)
{
    return 0.0;
}

bool GpuSubdevice::GetFbMs01Mode()
{
    NamedProperty *pFbMs01ModeProp;
    bool           FbMs01Mode = false;
    if ((OK != GetNamedProperty("FbMs01Mode", &pFbMs01ModeProp)) ||
        (OK != pFbMs01ModeProp->Get(&FbMs01Mode)))
    {
        Printf(Tee::PriDebug, "FbMs01Mode is not valid!!\n");
    }
    return FbMs01Mode;
}

bool GpuSubdevice::GetFbGddr5x16Mode()
{
    NamedProperty *pFbGddr5x16ModeProp;
    bool           FbGddr5x16Mode = false;
    if ((OK != GetNamedProperty("FbGddr5x16Mode", &pFbGddr5x16ModeProp)) ||
        (OK != pFbGddr5x16ModeProp->Get(&FbGddr5x16Mode)))
    {
        Printf(Tee::PriDebug, "FbGddr5x16Mode is not valid!!\n");
    }
    return FbGddr5x16Mode;
}

UINT32 GpuSubdevice::GetFbBankSwizzle()
{
    NamedProperty *pProp;
    UINT32  FbBankSwizzle = 0;
    if ((OK != GetNamedProperty("FbBankSwizzle", &pProp)) ||
        (OK != pProp->Get(&FbBankSwizzle)))
    {
        Printf(Tee::PriDebug, "FbBankSwizzle is not valid!!\n");
    }
    return FbBankSwizzle;
}

bool GpuSubdevice::GetFbBankSwizzleIsSet()
{
    NamedProperty *pProp;
    if (OK != GetNamedProperty("FbBankSwizzle", &pProp))
        return false;
    bool isSet = pProp->IsSet();
    return isSet;
}

string GpuSubdevice::GetFermiGpcTileMap()
{
    NamedProperty *pProp;
    string FermiGpcTileMap;
    if ((OK != GetNamedProperty("FermiGpcTileMap", &pProp)) ||
        (OK != pProp->Get(&FermiGpcTileMap)))
    {
        Printf(Tee::PriDebug, "FermiGpcTileMap is not valid!!\n");
    }
    return FermiGpcTileMap;
}

RC GpuSubdevice::ExelwtePreGpuInitScript()
{
    RC rc;
    NamedProperty *pProp;
    string PreGpuInitScript;
    if ((OK != GetNamedProperty("PreGpuInitScript", &pProp)) ||
        (OK != pProp->Get(&PreGpuInitScript)))
    {
        Printf(Tee::PriDebug, "PreGpuInitScript is not present.\n");
    }
    if (!PreGpuInitScript.empty())
    {
        // RM init sends escape writes before and after Devinit. While using -pre_gpu_init_script,
        // RM init does not send these espace writes. Triggering escape writes here to notify RTL
        // about devinit. For more info refer BUG 3240231

        if (Platform::GetSimulationMode() == Platform::RTL)
        {
            Platform::EscapeWrite("entering_section", 0, 4, DEVINIT_SECTION);
        }

        DEFER
        {
            if (Platform::GetSimulationMode() == Platform::RTL)
            {
                Platform::EscapeWrite("leaving_section", 0, 4, DEVINIT_SECTION);
            }
        };

        string fullPath = Utility::DefaultFindFile(PreGpuInitScript, true);

        string fileData;

        if (OK != Xp::InteractiveFileRead(fullPath.c_str(), &fileData))
        {
            return RC::FILE_UNKNOWN_ERROR;
        }

        if (fileData.empty())
        {
            Printf(Tee::PriError, "File is empty: \"%s\"! \n", fullPath.c_str());
            return RC::BAD_FORMAT;
        }

        std::vector<std::string> lineTokens;
        std::vector<std::string> tokens;
        Utility::Tokenizer(fileData, "\n", &lineTokens);

        if (lineTokens.size())
        {
            const size_t numLines = lineTokens.size();

            for (size_t lineIndex = 0; lineIndex < numLines; lineIndex++)
            {
                Utility::Tokenizer(lineTokens[lineIndex], ":", &tokens);

                if (tokens.size() == 0)
                    continue;

                if (tokens[0] == "W")
                {
                    if (tokens.size() >= 2 && tokens[1] == "0")
                    {
                        if (tokens.size() != 4)
                        {
                            Printf(Tee::PriError, "File format incorrect in file \"%s\"! \n", fullPath.c_str());
                            return RC::ILWALID_INPUT;
                        }

                        if (tokens[2].empty())
                        {
                            Printf(Tee::PriError,
                                   "%s: ERROR: invalid register offset (line %zu).\n",
                                   __FUNCTION__, lineIndex);
                            return RC::ILWALID_INPUT;
                        }
                        UINT32 regOffset = Utility::ColwertStrToHex(tokens[2].c_str());
                        UINT32 regData = Utility::ColwertStrToHex(tokens[3].c_str());

                        RegWr32(regOffset,regData);
                    }
                }
                if (tokens[0] == "p")
                {
                    if (tokens.size() >= 2 && tokens[1] == "0")
                    {
                        if (tokens.size() != 5)
                        {
                            Printf(Tee::PriError, "File format incorrect in file \"%s\"! \n", fullPath.c_str());
                            return RC::ILWALID_INPUT;
                        }

                        if (tokens[2].empty())
                        {
                            Printf(Tee::PriError,
                                   "%s: ERROR: invalid register offset (line %zu).\n",
                                   __FUNCTION__, lineIndex);
                            return RC::ILWALID_INPUT;
                        }

                        UINT32 regOffset = Utility::ColwertStrToHex(tokens[2].c_str());
                        UINT32 regValue = Utility::ColwertStrToHex(tokens[3].c_str());
                        UINT32 maskData = Utility::ColwertStrToHex(tokens[4].c_str());

                        rc = PollRegValue(regOffset, regValue, maskData, Tasker::GetDefaultTimeoutMs());

                        if (OK != rc)
                        {
                            Printf(Tee::PriError,
                                "GpuSubdevice::ExelwtePreGpuInitScript: Reg poll timed out reg: %d\n", regOffset);
                            return rc;
                        }
                    }
                }
            }
        }
    }
    return rc;
}
RC GpuSubdevice::GetMemoryBandwidthBps(UINT64 *pBps)
{
    MASSERT(pBps);

    const auto* pFb = GetFB();
    if (pFb->IsFbBroken())
    {
        Printf(Tee::PriLow, "Cannot get memory bandwidth with broken FB\n");
        return RC::UNSUPPORTED_DEVICE;
    }

    const UINT64 busWidth = pFb->GetBusWidth();

    if (!busWidth)
    {
        Printf(Tee::PriLow, "Cannot callwlate memory bandwidth, bus width is 0\n");
        return RC::UNSUPPORTED_DEVICE;
    }

    UINT64 freqHz = 0;

    if (IsSOC())
    {
        freqHz = CheetAh::SocPtr()->GetRAMFreqHz();
    }
    else
    {
        RC rc;
        CHECK_RC(GetClock(Gpu::ClkM, &freqHz, nullptr, nullptr, nullptr));
    }

    if (!freqHz)
    {
        Printf(Tee::PriLow, "Cannot callwlate memory bandwidth, memory freq is 0\n");
        return RC::UNSUPPORTED_DEVICE;
    }

    // The ClkM frequency is a multiple of WCK, depending on which GDDR mode
    // the GPU is in. Callwlating the bitRate is a function of WCK * DataRate
    // (where DataRate is # clocks/cycle and depends on the GDDR mode), but
    // since ClkM is already adjusted for the GDDR mode, we can callwlate
    // bitRate with: 2 * ClkM
    const UINT64 bitRate = 2U * freqHz;

    *pBps = static_cast<UINT64>((busWidth / 8.0) * bitRate);
    return RC::OK;
}

RC GpuSubdevice::GetMaxL2BandwidthBps(UINT64 l2ClkHz, UINT64 *pReadBps, UINT64 *pWriteBps)
{
    RC rc;
    MASSERT(pReadBps);
    MASSERT(pWriteBps);

    if (IsSOC())
    {
        return RC::UNSUPPORTED_FUNCTION;
    }

    if (l2ClkHz == 0ULL)
    {
        Printf(Tee::PriLow, "Cannot callwlate L2 bandwidth, L2 freq is 0\n");
        return RC::UNSUPPORTED_DEVICE;
    }

    const UINT32 dataBusSizeBytes = 32;
    const UINT32 numActiveL2Slices = GetFB()->GetL2SliceCount();
    const UINT64 portBps = static_cast<UINT64>(numActiveL2Slices * dataBusSizeBytes * l2ClkHz);
    UINT32 numWritePorts = 1;
    UINT32 numReadPorts  = 1;
    if (Regs().IsSupported(MODS_LITTER_LTC_SECONDARY_PORT) &&
        Regs().LookupAddress(MODS_LITTER_LTC_SECONDARY_PORT) == 1)
    {
        // Number of read ports are usually x2 for 100 class chips
        // and x1 for the others
        numReadPorts = 2;
    }
    *pReadBps  = portBps * numReadPorts;
    *pWriteBps = portBps * numWritePorts;

    return rc;
}

UINT32 GpuSubdevice::GetMaxCeCount() const
{
    return 0;
}

RC GpuSubdevice::GetMaxCeBandwidthKiBps(UINT32 *pBwKiBps)
{
    MASSERT(pBwKiBps);
    *pBwKiBps = 0;
    return RC::UNSUPPORTED_FUNCTION;
}

/* virtual */ RC GpuSubdevice::GetMaxL1CacheSize(UINT32* pSizeBytes) const
{
    MASSERT(pSizeBytes);
    RC rc;

    UINT32 sizeBytes = 0;
    CHECK_RC(GetMaxL1CacheSizePerSM(&sizeBytes));
    sizeBytes *= ShaderCount();

    *pSizeBytes = sizeBytes;
    return rc;
}

/* virtual */ RC GpuSubdevice::GetMaxL1CacheSizePerSM(UINT32* pSizeBytes) const
{
    // Not all architectures support L1 caching of global (LDG) instructions
    // Others require special flags to support L1, so by default we'll just return unsupported
    Printf(Tee::PriError, "L1 cache size is not defined for this chip!\n");
    return RC::UNSUPPORTED_FUNCTION;
}

double GpuSubdevice::GetIssueRate(Instr type)
{
    return 1.0;
}

RC GpuSubdevice::OverrideIssueRate(const string& overrideStr, IssueRateOverrideSettings* pOrigSettings)
{
    // Check for priv access / SW support of current GPU
    RC rc;
    CHECK_RC(CheckIssueRateOverride());

    // Back up old override state if requested
    if (pOrigSettings)
    {
        CHECK_RC(GetIssueRateOverride(pOrigSettings));
    }

    // Override state
    vector<pair<string, UINT32>> overrides;
    CHECK_RC(ParseIssueRateOverride(overrideStr, &overrides));
    for (const auto& entry : overrides)
    {
        const string& unit    = entry.first;
        const UINT32 throttle = entry.second;
        CHECK_RC(IssueRateOverride(unit, throttle));
    }
    return rc;
}

RC GpuSubdevice::RestoreIssueRate(const IssueRateOverrideSettings& origSettings)
{
    // Check for priv access / SW support of current GPU
    RC rc;
    CHECK_RC(CheckIssueRateOverride());

    // Restore old override state
    CHECK_RC(IssueRateOverride(origSettings));
    return rc;
}

/* virtual */ RC GpuSubdevice::ParseIssueRateOverride(const string& overrideStr, vector<pair<string, UINT32>>* pOverrides)
{
    RC rc;
    const string cleanStr = Utility::ToUpperCase(Utility::RemoveSpaces(overrideStr));

    vector<string> tokens;
    Utility::Tokenizer(cleanStr, ",", &tokens);
    if (tokens.size() % 2)
    {
        Printf(Tee::PriError, "IssueRateOverride has an odd number of elements\n");
        return RC::BAD_COMMAND_LINE_ARGUMENT;
    }
    for (UINT32 i = 0; i < tokens.size(); i += 2)
    {
        const string& unit = tokens[i];
        UINT32 throttle = 0;
        CHECK_RC(Utility::StringToUINT32(tokens[i + 1], &throttle, 0));
        pOverrides->emplace_back(std::move(unit), throttle);
    }
    return rc;
}

/* virtual */ RC GpuSubdevice::CheckIssueRateOverride()
{
    Printf(Tee::PriError, "Issue-rate override is not implemented for this GPU\n");
    return RC::UNSUPPORTED_FUNCTION;
}

/* virtual */ RC GpuSubdevice::GetIssueRateOverride(IssueRateOverrideSettings *pOverrides)
{
    Printf(Tee::PriError, "Issue-rate override is not implemented for this GPU\n");
    return RC::UNSUPPORTED_FUNCTION;
}

/* virtual */ RC GpuSubdevice::IssueRateOverride(const IssueRateOverrideSettings& overrides)
{
    Printf(Tee::PriError, "Issue-rate override is not implemented for this GPU\n");
    return RC::UNSUPPORTED_FUNCTION;
}

/* virtual */ RC GpuSubdevice::IssueRateOverride(const string& unit, UINT32 throttle)
{
    Printf(Tee::PriError, "Issue-rate override is not implemented for this GPU\n");
    return RC::UNSUPPORTED_FUNCTION;
}

UINT08 GpuSubdevice::GetSmbusAddress() const
{
    const RegHal &regs = Regs();
    const auto arpStatus = regs.Read32(MODS_THERM_ARP_STATUS);
    const auto i2cAddr = regs.Read32(MODS_THERM_I2C_ADDR);

    // Get address from SMBus address resolution protocol if enabled
    if (regs.Read32(MODS_THERM_ARP_CFG_ENABLE) &&
        regs.GetField(arpStatus, MODS_THERM_ARP_STATUS_AV_FLAG))
    {
        return static_cast<UINT08>(regs.GetField(arpStatus, MODS_THERM_ARP_STATUS_ARP_ADR));
    }

    if (regs.Test32(MODS_PEXTDEV_BOOT_3_STRAP_1_SMB_ALT_ADDR_ENABLED))
    {
        return static_cast<UINT08>(regs.GetField(i2cAddr, MODS_THERM_I2C_ADDR_1) / 2);
    }

    switch (regs.Read32(MODS_FUSE_OPT_I2CS_ADDR_SEL_DATA))
    {
        case 0:
            return static_cast<UINT08>(regs.GetField(i2cAddr, MODS_THERM_I2C_ADDR_0) / 2);
        case 1:
            return static_cast<UINT08>(regs.GetField(i2cAddr, MODS_THERM_I2C_ADDR_1) / 2);
        case 2:
            return static_cast<UINT08>(regs.GetField(i2cAddr, MODS_THERM_I2C_ADDR_2) / 2);
        case 3:
            return static_cast<UINT08>(regs.GetField(i2cAddr, MODS_THERM_I2C_ADDR_3) / 2);
        default:
            Printf(Tee::PriError, "Smbus address not found\n");
            MASSERT(0);
    }
    return 0;
}

RC GpuSubdevice::DramclkDevInitWARkHz(UINT32 freqkHz)
{
    Printf(Tee::PriError, "Dramclk devinit WAR is not supported\n");
    return RC::UNSUPPORTED_FUNCTION;
}

RC GpuSubdevice::DisableEcc()
{
    Printf(Tee::PriWarn, "ECC is unsupported on this chip, -disable_ecc failed!\n");
    return RC::UNSUPPORTED_FUNCTION;
}

RC GpuSubdevice::EnableEcc()
{
    // WAR 1742224
    // The field diag uses -enable_ecc unconditionally, so this
    // method cannot return an invalid RC for being unsupported
    Printf(Tee::PriWarn, "-enable_ecc is unsupported on this chip!\n");
    return OK;
}

RC GpuSubdevice::DisableDbi()
{
    Printf(Tee::PriError, "-disable_dbi is unsupported on this chip!\n");
    return RC::UNSUPPORTED_FUNCTION;
}

RC GpuSubdevice::EnableDbi()
{
    Printf(Tee::PriError, "-enable_dbi is unsupported on this chip!\n");
    return RC::UNSUPPORTED_FUNCTION;
}

RC GpuSubdevice::SkipInfoRomRowRemapping()
{
    Printf(Tee::PriError, "-skip_inforom_row_remapping is unsupported on this chip!\n");
    return RC::UNSUPPORTED_FUNCTION;
}

RC GpuSubdevice::DisableRowRemapper()
{
    Printf(Tee::PriError, "-disable_row_remapper is unsupported on this chip!\n");
    return RC::UNSUPPORTED_FUNCTION;
}

bool GpuSubdevice::GetL2EccCheckbitsEnabled() const
{
    return Regs().Test32(MODS_PLTCG_LTCS_LTSS_DSTG_CFG0_CHECKBIT_WR_SUPPRESS_DISABLED);
}
RC GpuSubdevice::SetL2EccCheckbitsEnabled(bool enable)
{
    UINT32 value = Regs().Read32(MODS_PLTCG_LTCS_LTSS_DSTG_CFG0);
    Regs().SetField(&value,
                    enable ? MODS_PLTCG_LTCS_LTSS_DSTG_CFG0_CHECKBIT_WR_SUPPRESS_DISABLED
                           : MODS_PLTCG_LTCS_LTSS_DSTG_CFG0_CHECKBIT_WR_SUPPRESS_ENABLED);
    Regs().Write32(MODS_PLTCG_LTCS_LTSS_DSTG_CFG0, value);

    // WAR for bug 649430: read the registers back, in order to give
    // the register-write time to percolate through the ECC subsystem
    const UINT32 numLTCs      = GetFB()->GetLtcCount();
    for (UINT32 ltcid = 0; ltcid < numLTCs; ltcid++)
    {
        const UINT32 slicesPerLTC = GetFB()->GetSlicesPerLtc(ltcid);
        for (UINT32 slice = 0; slice < slicesPerLTC; slice++)
        {
            RegRd32(Regs().LookupAddress(MODS_PLTCG_LTC0_LTS0_DSTG_CFG0)
                        + ltcid * Regs().LookupAddress(MODS_LTC_PRI_STRIDE)
                        + slice * Regs().LookupAddress(MODS_LTS_PRI_STRIDE));
        }
    }
    return OK;
}
bool GpuSubdevice::GetL1EccEnabled() const
{
    return Regs().Test32(MODS_PGRAPH_PRI_GPCS_TPCS_L1C_ECC_CSR_ENABLE_EN);
}
RC GpuSubdevice::SetL1EccEnabled(bool enable)
{
    UINT32 gpcCount = GetGpcCount();
    for (UINT32 gpc = 0; gpc < gpcCount; ++gpc)
    {
        UINT32 tpcCount = GetTpcCountOnGpc(gpc);
        for (UINT32 tpc = 0; tpc < tpcCount; ++tpc)
        {
            const UINT32 reg =
                Regs().LookupAddress(MODS_PGRAPH_PRI_GPC0_TPC0_L1C_ECC_CSR)
              + (gpc * Regs().LookupAddress(MODS_GPC_PRI_STRIDE))
              + (tpc * Regs().LookupAddress(MODS_TPC_IN_GPC_STRIDE));
            UINT32 value = RegRd32(reg);

            if (enable)
            {
                Regs().SetField(&value, MODS_PGRAPH_PRI_GPCS_TPCS_L1C_ECC_CSR_ENABLE_EN);
            }
            else
            {
                Regs().SetField(&value, MODS_PGRAPH_PRI_GPCS_TPCS_L1C_ECC_CSR_ENABLE_DIS);
            }

            RegWr32(reg, value);
        }
    }

    return OK;
}
RC GpuSubdevice::ForceL1EccCheckbits(UINT32 data)
{
    return RC::UNSUPPORTED_FUNCTION;
}
RC GpuSubdevice::IlwalidateCCache()
{
    Regs().Write32(MODS_PGRAPH_PRI_GPCS_GCC_DBG_ILWALIDATE_TASK);
    Regs().Write32(MODS_PGRAPH_PRI_GPCS_TPCS_SM_CACHE_CONTROL_ILWALIDATE_IDX_CCACHE_TASK);
    return OK;
}
RC GpuSubdevice::SetSMDebuggerMode(bool enable)
{
    if (enable)
    {
        Regs().Write32(MODS_PGRAPH_PRI_GPCS_TPCS_SM_DBGR_CONTROL0_DEBUGGER_MODE_ON);
    }
    else
    {
        Regs().Write32(MODS_PGRAPH_PRI_GPCS_TPCS_SM_DBGR_CONTROL0_DEBUGGER_MODE_OFF);
    }
    return OK;
}

RC GpuSubdevice::GetSmEccEnable(bool * pEnable, bool * pOverride) const
{
    if (pEnable)
    {
        *pEnable =  Regs().Test32(MODS_PGRAPH_PRI_GPCS_TPCS_SM_LRF_ECC_CONTROL_MASTER_EN_ENABLE);
    }

    return OK;
}
RC GpuSubdevice::SetSmEccEnable(bool bEnable, bool bOverride)
{
    UINT32 gpcCount = GetGpcCount();
    for (UINT32 gpc = 0; gpc < gpcCount; ++gpc)
    {
        UINT32 tpcCount = GetTpcCountOnGpc(gpc);
        for (UINT32 tpc = 0; tpc < tpcCount; ++tpc)
        {
            const UINT32 reg =
                Regs().LookupAddress(MODS_PGRAPH_PRI_GPC0_TPC0_SM_LRF_ECC_CONTROL)
              + (gpc * Regs().LookupAddress(MODS_GPC_PRI_STRIDE))
              + (tpc * Regs().LookupAddress(MODS_TPC_IN_GPC_STRIDE));
            UINT32 value = RegRd32(reg);

            if (bEnable)
            {
                Regs().SetField(&value, MODS_PGRAPH_PRI_GPC0_TPC0_SM_LRF_ECC_CONTROL_MASTER_EN_ENABLE);
            }
            else
            {
                Regs().SetField(&value, MODS_PGRAPH_PRI_GPC0_TPC0_SM_LRF_ECC_CONTROL_MASTER_EN_DISABLE);
            }

            RegWr32(reg, value);
        }
    }

    return OK;
}

RC GpuSubdevice::GetSHMEccEnable(bool * pEnable, bool * pOverride) const
{
    return RC::UNSUPPORTED_FUNCTION;
}
RC GpuSubdevice::SetSHMEccEnable( bool bEnable, bool bOverride)
{
    return RC::UNSUPPORTED_FUNCTION;
}

RC GpuSubdevice::GetTexEccEnable(bool * pEnable, bool * pOverride) const
{
    return RC::UNSUPPORTED_FUNCTION;;
}

RC GpuSubdevice::SetTexEccEnable( bool bEnable, bool bOverride)
{
    return RC::UNSUPPORTED_FUNCTION;
}

RC GpuSubdevice::SetEccOverrideEnable(LW2080_ECC_UNITS eclwnit, bool bEnabled, bool bOverride)
{
    return RC::UNSUPPORTED_FUNCTION;
}

RC GpuSubdevice::GetEccEnabled(bool *pbSupported, UINT32 *pEnabledMask) const
{
    RC rc;
    LW2080_CTRL_GPU_QUERY_ECC_STATUS_PARAMS params;

    MASSERT(pbSupported);
    MASSERT(pEnabledMask);

    *pbSupported = false;
    *pEnabledMask = 0;

    // Query the RM for the ECC enabled counts, and exit on error.
    //
    // Note that an LWRM_NOT_SUPPORTED is not considered a real error,
    // since that's expected for GPUs that don't have ECC.  In that
    // case, we return OK and return not supported with the enable mask
    // set to zero
    //
    memset(&params, 0, sizeof(params));
    if (IsSMCEnabled())
    {
        SetGrRouteInfo(&(params.grRouteInfo), GetSmcEngineIdx());
    }
    rc = LwRmPtr()->ControlBySubdevice(this,
                                       LW2080_CTRL_CMD_GPU_QUERY_ECC_STATUS,
                                       &params, sizeof(params));
    if (rc != OK)
    {
        if (rc == RC::LWRM_NOT_SUPPORTED)
            rc.Clear();  // Suppress this error.
        return rc;
    }

    for (UINT32 unit = 0; unit < LW2080_CTRL_GPU_ECC_UNIT_COUNT; unit++)
    {
        if (params.units[unit].enabled)
            *pEnabledMask |= Ecc::ToEclwnitBitFromRmGpuEclwnit(unit);
    }

    // There should be a non-zero enable mask to consider ECC supported
    if (*pEnabledMask)
        *pbSupported = true;

    return rc;
}

RC GpuSubdevice::GetEdcEnabled(bool *pbSupported,
                               UINT32 *pEnabledMask)
{
    RC rc;

    MASSERT(pbSupported);
    MASSERT(pEnabledMask);

    *pbSupported = false;
    *pEnabledMask = 0;

    if (!HasFeature(GPUSUB_SUPPORTS_EDC))
    {
        *pbSupported = false;
        return OK;
    }

    *pbSupported = true;

    UINT32 pfbCfg0 = 0;
    if (Regs().IsSupported(MODS_PFB_FBPA_CFG0))
    {
        if (HasBug(2355623))
        {
            pfbCfg0 = Regs().Read32(MODS_PFB_FBPA_CFG0);
        }
        else
        {
            CHECK_RC(Regs().Read32Priv(MODS_PFB_FBPA_CFG0, &pfbCfg0));
        }
    }

    if (Regs().TestField(pfbCfg0, MODS_PFB_FBPA_CFG0_RDEDC_ENABLED))
    {
        *pEnabledMask |= 0x00000001; // 1 << EDC_UNIT_RD
    }

    if (Regs().TestField(pfbCfg0, MODS_PFB_FBPA_CFG0_WREDC_ENABLED))
    {
        *pEnabledMask |= 0x00000002; // 1 << EDC_UNIT_WR
    }

    if (Regs().TestField(pfbCfg0, MODS_PFB_FBPA_CFG0_REPLAY_ENABLED))
    {
        *pEnabledMask |= 0x00000004; // 1 << EDC_UNIT_REPLAY
    }

    return rc;
}

//------------------------------------------------------------------------------
RC GpuSubdevice::SmidToHwGpcHwTpc
(
    UINT32 smid,
    UINT32 *pHwGpcNum,
    UINT32 *pHwTpcNum
) const
{
    RC rc;

    if (!Regs().IsSupported(MODS_SCAL_LITTER_NUM_SM_PER_TPC) ||
        !Regs().IsSupported(MODS_PGRAPH_PRI_CWD_GPC_TPC_ID))
    {
        Printf(Tee::PriError, "smid to HW GPC/TPC mapping not supported\n");
        return RC::UNSUPPORTED_FUNCTION;
    }
    if (Platform::IsVirtFunMode() && !IsSMCEnabled())
    {
        // Virtual function calls rely on MODS_PRGRAPH_PRI_CWD_GPC_TPC_ID being routed to the
        // physical function by SmcRegHal::ReadPgraphRegister
        Printf(Tee::PriError, "smid to HW GPC/TPC mapping not supported on VFs without SMC\n");
        return RC::UNSUPPORTED_FUNCTION;
    }

    // Don't use GetMaxSmPerTpcCount() since we want the number of physical, not virt SMs.
    // For GP100 arch GEMM kernels report the physical SM id instead of the virt LWCA-SM id.
    UINT32 smPerTpc = Regs().LookupAddress(MODS_SCAL_LITTER_NUM_SM_PER_TPC);
    UINT32 smLookup = smid / smPerTpc;

    // We use the smid to index into a register array, which has a sequential smid layout.
    // Each register contains 4 TPC/GPC pairs, with 4 bits per field.
    UINT32 i = smLookup / 4;
    UINT32 j = smLookup % 4;
    UINT32 val = Regs().Read32(MODS_PGRAPH_PRI_CWD_GPC_TPC_ID, i);
    UINT32 logicalTpc = (val >> (j * 8)) & 0xF;
    UINT32 localGpc = (val >> (j * 8 + 4)) & 0xF;

    UINT32 hwGpc = 0;
    UINT32 hwTpc = 0;

    if (IsSMCEnabled())
    {
        CHECK_RC(LocalGpcToHwGpc(GetPartIdx(), GetSmcEngineIdx(), localGpc, &hwGpc));
    }
    else
    {
        CHECK_RC(VirtualGpcToHwGpc(localGpc, &hwGpc));
    }
    CHECK_RC(VirtualTpcToHwTpc(hwGpc, logicalTpc, &hwTpc));
    *pHwGpcNum = hwGpc;
    *pHwTpcNum = hwTpc;

    return rc;
}

//------------------------------------------------------------------------------
RC GpuSubdevice::LocalGpcToHwGpc
(
    UINT32 partIdx,
    UINT32 smcEngineIdx,
    UINT32 localGpcNum,
    UINT32 *pHwGpcNum
) const
{
    MASSERT(IsSMCEnabled());
    RC rc;

    if (Platform::IsVirtFunMode())
    {
        VmiopElw* pVmiopElw = nullptr;
        CHECK_RC(GetVmiopElw(&pVmiopElw));
        return pVmiopElw->CallPluginLocalGpcToHwGpc(smcEngineIdx, localGpcNum, pHwGpcNum);
    }
    else
    {
        MASSERT(m_SmcTopology.count(partIdx));
        MASSERT(smcEngineIdx < m_SmcTopology[partIdx].smcEngines.size());
        MASSERT(localGpcNum  < m_SmcTopology[partIdx].smcEngines[smcEngineIdx].gpcs.size());
        *pHwGpcNum = m_SmcTopology[partIdx].smcEngines[smcEngineIdx].gpcs[localGpcNum].physGpc;

        Printf(Tee::PriDebug, "LocalGPC: %d VirtGPC: %d PhysGPC: %d\n"
               ,m_SmcTopology[partIdx].smcEngines[smcEngineIdx].gpcs[localGpcNum].localGpc
               ,m_SmcTopology[partIdx].smcEngines[smcEngineIdx].gpcs[localGpcNum].virtGpc
               ,m_SmcTopology[partIdx].smcEngines[smcEngineIdx].gpcs[localGpcNum].physGpc
        );
    }
    return rc;
}

//------------------------------------------------------------------------------
RC GpuSubdevice::VirtualGpcToHwGpc
(
    UINT32 virtualGpcNum,
    UINT32 *pHwGpcNum
) const
{
    MASSERT(!Platform::IsVirtFunMode());

    // Check that virtual GPC exists
    if (virtualGpcNum >= GetGpcCount())
    {
        return RC::SOFTWARE_ERROR;
    }

    if (HasFeature(GPUSUB_GPC_REENUMERATION))
    {
        *pHwGpcNum =
            Regs().Read32(MODS_PSMCARB_SMC_PARTITION_GPC_MAP_PHYSICAL_GPC_ID, virtualGpcNum);
    }
    else
    {
        // Colwert the GPC number to a HW GPC number
        UINT32 gpcMask = GetFsImpl()->GpcMask();
        UINT32 gpcCount = 0;
        UINT32 startBit = 0;
        do
        {
            *pHwGpcNum = Utility::BitScanForward(gpcMask, startBit);
            startBit = *pHwGpcNum + 1;
            gpcCount++;
        } while (gpcCount != (virtualGpcNum + 1));
    }
    return OK;
}

//------------------------------------------------------------------------------
RC GpuSubdevice::HwGpcToVirtualGpc
(
    UINT32 hwGpcNum,
    UINT32 *pVirtualGpcNum
) const
{
    // Check that HW GPC exists
    UINT32 gpcMask = GetFsImpl()->GpcMask();
    UINT32 gpcBit = 1 << hwGpcNum;
    if ((gpcMask & gpcBit) == 0)
    {
        return RC::SOFTWARE_ERROR;
    }

    if (HasFeature(GPUSUB_GPC_REENUMERATION))
    {
        const RegHal& regs = Regs();
        const UINT32 gpcCount = GetGpcCount();
        UINT32 virtGpc;
        for (virtGpc = 0; virtGpc < gpcCount; virtGpc++)
        {
            UINT32 hwGpc =
                regs.Read32(MODS_PSMCARB_SMC_PARTITION_GPC_MAP_PHYSICAL_GPC_ID, virtGpc);
            if (hwGpc == hwGpcNum)
            {
                break;
            }
        }
        if (virtGpc >= gpcCount)
        {
            Printf(Tee::PriError, "HW GPC was not in the Virtual GPC -> HW GPC mapping!\n");
            return RC::SOFTWARE_ERROR;
        }
        *pVirtualGpcNum = virtGpc;
    }
    else
    {
        // Virtual GpcNum starts from zero
        *pVirtualGpcNum = (UINT32)Utility::CountBits(gpcMask & (gpcBit - 1));
    }
    return OK;
}

RC GpuSubdevice::HwGpcToVirtualGpcMask
(
    UINT32 HwGpcMask,
    UINT32 *pVirtualGpcMask
) const
{
    UINT32 virtGpcNum;
    *pVirtualGpcMask = 0;
    INT32 hwGPCNum = Utility::BitScanForward(HwGpcMask, 0);
    while (hwGPCNum != -1)
    {
        RC rcH2V = HwGpcToVirtualGpc(hwGPCNum, &virtGpcNum);
        if (rcH2V != RC::OK)
        {
            Printf(Tee::PriError,
                "GPC%d not enabled for hwGpcMask=0x%02x\n",
                hwGPCNum, HwGpcMask);
            return RC::ILWALID_ARGUMENT;
        }
        *pVirtualGpcMask |= 1ULL << virtGpcNum;
        hwGPCNum = Utility::BitScanForward(HwGpcMask, hwGPCNum + 1);
    }

    return RC::OK;
}

//------------------------------------------------------------------------------
/* virtual */ RC GpuSubdevice::VirtualTpcToHwTpc
(
    UINT32 hwGpcNum,
    UINT32 virtualTpcNum,
    UINT32 *pHwTpcNum
) const
{
    MASSERT(pHwTpcNum);
    RC rc;

    UINT32 tpcMask = 0;
    if (Platform::IsVirtFunMode())
    {
        VmiopElw* pVmiopElw = nullptr;
        CHECK_RC(GetVmiopElw(&pVmiopElw));
        CHECK_RC(pVmiopElw->CallPluginGetTpcMask(hwGpcNum, &tpcMask));
    }
    else
    {
        tpcMask = GetFsImpl()->TpcMask(hwGpcNum);
    }
    INT32 hwTpcNum = Utility::FindNthSetBit(tpcMask, (INT32)virtualTpcNum);
    if (hwTpcNum < 0)
        return RC::SOFTWARE_ERROR;
    *pHwTpcNum = (UINT32)hwTpcNum;
    return rc;
}

//------------------------------------------------------------------------------
/* virtual */ RC GpuSubdevice::HwTpcToVirtualTpc
(
    UINT32 hwGpcNum,
    UINT32 hwTpcNum,
    UINT32 *pVirtualTpcNum
) const
{
    UINT32 tpcMask = GetFsImpl()->TpcMask(hwGpcNum);
    UINT32 tpcBit = 1 << hwTpcNum;

    if ((tpcMask & tpcBit) == 0)
    {
        return RC::SOFTWARE_ERROR;
    }

    // virtual TpcNum starts from zero
    *pVirtualTpcNum = (UINT32)Utility::CountBits(tpcMask & (tpcBit - 1));
    return OK;
}

//------------------------------------------------------------------------------
/* virtual */ RC GpuSubdevice::GetSOCDeviceTypes
(
    vector<UINT32>* pDeviceTypes,
    SOCDevFilter filter
)
{
    return RC::DEVICE_NOT_FOUND;
}

//------------------------------------------------------------------------------
RC GpuSubdevice::GetSOCInterrupts(vector<UINT32>* pInterrupts)
{
    MASSERT(pInterrupts);
    pInterrupts->clear();

    vector<UINT32> devTypes;
    RC rc;
    CHECK_RC(GetSOCDeviceTypes(&devTypes, SOC_INTERRUPT));

    for (UINT32 i=0; i < devTypes.size(); i++)
    {
        vector<CheetAh::DeviceDesc> deviceDescs;
        CHECK_RC(CheetAh::GetDeviceDescByType(devTypes[i], &deviceDescs));
        for (UINT32 j=0; j < deviceDescs.size(); j++)
        {
            for (UINT32 k=0; k < deviceDescs[j].ints.size(); k++)
            {
                if (deviceDescs[j].ints[k].target != CheetAh::INT_TARGET_AVP)
                {
                    UINT32 cpuIrq = ~0U;
                    CHECK_RC(CheetAh::GetCpuIrq(deviceDescs[j].ints[k], &cpuIrq));
                    pInterrupts->push_back(cpuIrq);
                }
            }
        }
    }
    return OK;
}

//------------------------------------------------------------------------------
UINT32 GpuSubdevice::Foundry()
{
    // Foundry info is not available from RM, so read it directly from PMC_BOOT_0
    UINT32 pmcBoot0 = RegRd32(LW_PMC_BOOT_0);

    // Make sure the read was successful
    MASSERT((pmcBoot0 != 0xFFFFFFFF) && ((pmcBoot0 & 0xFFFF0000) != 0xBADF0000));

    // Colwert to constants expected by the callers
    UINT32 foundryId = DRF_VAL(_PMC, _BOOT_0, _FOUNDRY, pmcBoot0);
    switch (foundryId)
    {
        case LW_PMC_BOOT_0_FOUNDRY_TSMC:
            return LW2080_CTRL_GPU_INFO_FOUNDRY_TSMC;
        case LW_PMC_BOOT_0_FOUNDRY_UMC:
            return LW2080_CTRL_GPU_INFO_FOUNDRY_UMC;
        case LW_PMC_BOOT_0_FOUNDRY_IBM:
            return LW2080_CTRL_GPU_INFO_FOUNDRY_IBM;
        case LW_PMC_BOOT_0_FOUNDRY_SMIC:
            return LW2080_CTRL_GPU_INFO_FOUNDRY_SMIC;
        case LW_PMC_BOOT_0_FOUNDRY_CHARTERED:
            return LW2080_CTRL_GPU_INFO_FOUNDRY_CHARTERED;
        case LW_PMC_BOOT_0_FOUNDRY_SONY:
            return LW2080_CTRL_GPU_INFO_FOUNDRY_SONY;
        case LW_PMC_BOOT_0_FOUNDRY_TOSHIBA:
            return LW2080_CTRL_GPU_INFO_FOUNDRY_TOSHIBA;
        case LW_PMC_BOOT_0_FOUNDRY_SEC:
            return LW2080_CTRL_GPU_INFO_FOUNDRY_SAMSUNG;
        default:
            Printf(Tee::PriError, "Unknown Foundry ID (%d).\n", foundryId);
            break;
    }
    MASSERT(!"Unknown foundry ID");
    return Gpu::IlwalidFoundry;
}

UINT32 GpuSubdevice::MemoryIndex()
{
    if (!FrameBuffer::IsHbm(this) ||
        !Regs().IsSupported(MODS_THERM_SELWRE_WR_SCRATCH_3_MEM_TABLE_INDEX))
    {
        return 0xFFFFFFFF;
    }
    return Regs().Read32(MODS_THERM_SELWRE_WR_SCRATCH_3_MEM_TABLE_INDEX);
}

//------------------------------------------------------------------------------
UINT32 GpuSubdevice::MemoryStrap()
{
    return Regs().Read32(MODS_PEXTDEV_BOOT_0_STRAP_RAMCFG);
}

//------------------------------------------------------------------------------
UINT32 GpuSubdevice::RamConfigStrap()
{
    return Regs().Read32(MODS_PEXTDEV_BOOT_0_STRAP_RAMCFG);
}
//------------------------------------------------------------------------------
UINT32 GpuSubdevice::TvEncoderStrap()
{
    return Regs().Read32(MODS_PEXTDEV_BOOT_0_STRAP_TVMODE);
}
//------------------------------------------------------------------------------
UINT32 GpuSubdevice::Io3GioConfigPadStrap()
{
    return Regs().Read32(MODS_PEXTDEV_BOOT_0_STRAP_3GIO_PADCFG_LUT_ADR);
}
//------------------------------------------------------------------------------
UINT32 GpuSubdevice::UserStrapVal()
{
    return Regs().Read32(MODS_PEXTDEV_BOOT_0_STRAP_USER);
}

/* virtual */
RC GpuSubdevice::SetZlwllSize( UINT32 zlwllRegion,
                               UINT32 displayWidth,
                               UINT32 displayHeight)
{
    UINT32 zlwll_size = GetZlwllPixelsCovered();
    UINT32 zlwll_width = 0;
    UINT32 zlwll_height = 0;

    const UINT32 zlwllWidthMult = Regs().LookupValue(MODS_PGRAPH_PRI_GPC0_ZLWLL_ZCSIZE_WIDTH__MULTIPLE);
    const UINT32 zlwllHeightMult = Regs().LookupValue(MODS_PGRAPH_PRI_GPC0_ZLWLL_ZCSIZE_HEIGHT__MULTIPLE);

    zlwll_size /= zlwllWidthMult;
    zlwll_size /= zlwllHeightMult;

    zlwll_width = displayWidth +
                  (zlwllWidthMult - 1);
    zlwll_width /= zlwllWidthMult;

    zlwll_height = displayHeight +
                   (zlwllHeightMult - 1);
    zlwll_height /= zlwllHeightMult;

    if ((zlwll_width * zlwll_height)  > zlwll_size)
    {
        zlwll_height = zlwll_size / zlwll_width;

        // Ensure that zlwll_width and zlwll_height will fit in the appropriate
        // registers
        while (zlwll_height & ~Regs().LookupFieldValueMask(MODS_PGRAPH_PRI_GPC0_ZLWLL_ZCSIZE_HEIGHT))
        {
            zlwll_height = zlwll_height >> 1;
            zlwll_width = zlwll_width << 1;
        }
        while (zlwll_width & ~Regs().LookupFieldValueMask(MODS_PGRAPH_PRI_GPC0_ZLWLL_ZCSIZE_WIDTH))
        {
            zlwll_width = zlwll_width >> 1;
            zlwll_height = zlwll_height << 1;
        }

        // If the zlwll_height or width are out of bounds at this point,
        // return an error
        if ((zlwll_height & Regs().LookupFieldValueMask(MODS_PGRAPH_PRI_GPC0_ZLWLL_ZCSIZE_HEIGHT)) ||
            (zlwll_width & Regs().LookupFieldValueMask(MODS_PGRAPH_PRI_GPC0_ZLWLL_ZCSIZE_WIDTH)))
        {
            Printf(Tee::PriError,
                   "Cannot allocate zlwll size %dx%d for display size %dx%d\n",
                   zlwll_width * zlwllWidthMult,
                   zlwll_height * zlwllHeightMult,
                   displayWidth,
                   displayHeight);
            return RC::CANNOT_ALLOCATE_MEMORY;
        }
        Printf(Tee::PriWarn, "Reducing size of ZLWLL RAM to %dx%d\n",
                zlwll_width * zlwllWidthMult,
                zlwll_height * zlwllHeightMult);
    }

    if (zlwll_height || zlwll_width)
    {
        UINT32 zcsize = Regs().Read32(MODS_PGRAPH_PRI_GPC0_ZLWLL_ZCSIZE, zlwllRegion);
        Regs().SetField(&zcsize, MODS_PGRAPH_PRI_GPC0_ZLWLL_ZCSIZE_WIDTH, zlwll_width);
        Regs().SetField(&zcsize, MODS_PGRAPH_PRI_GPC0_ZLWLL_ZCSIZE_HEIGHT, zlwll_height);
        Regs().Write32(MODS_PGRAPH_PRI_GPC0_ZLWLL_ZCSIZE, zlwllRegion, zcsize);
    }

    return OK;
}

/* virtual */
bool GpuSubdevice::HasFeature(Feature feature) const
{
    MASSERT(IS_GPUSUB_FEATURE(feature));

    return Device::HasFeature(feature);
}

/* virtual */
void GpuSubdevice::SetHasFeature(Feature feature)
{
    MASSERT(IS_GPUSUB_FEATURE(feature));

    Device::SetHasFeature(feature);
}

/* virtual */
void GpuSubdevice::ClearHasFeature(Feature feature)
{
    MASSERT(IS_GPUSUB_FEATURE(feature));

    Device::ClearHasFeature(feature);
}

/* virtual */
bool GpuSubdevice::IsFeatureEnabled(Feature feature)
{
    MASSERT(HasFeature(feature));

    switch (feature)
    {
        case GPUSUB_HAS_KFUSES:
        case GPUSUB_HAS_AZALIA_CONTROLLER:
        {
            bool featureEnabled = true;

            // Display engine can be fused out
            {
                unique_ptr<ScopedRegister> debug1;
                if (Regs().IsSupported(MODS_PTOP_DEBUG_1_FUSES_VISIBLE))
                {
                    debug1 = make_unique<ScopedRegister>(this, Regs().LookupAddress(MODS_PTOP_DEBUG_1));
                    Regs().Write32(MODS_PTOP_DEBUG_1_FUSES_VISIBLE_ENABLED);
                }
                if (!Regs().Test32(MODS_FUSE_OPT_DISPLAY_DISABLE_DATA, 0))
                {
                    featureEnabled = false;
                }
            }

            // Azalia can be disabled by LW_XVE_PRIV_XV_1_CYA_XVE_MULTIFUNC
            if (feature == GPUSUB_HAS_AZALIA_CONTROLLER)
            {
                if (Regs().Test32(MODS_XVE_PRIV_XV_1_CYA_XVE_MULTIFUNC_DISABLE))
                {
                    featureEnabled = false;
                }
            }

            if (feature == GPUSUB_HAS_AZALIA_CONTROLLER)
            {
                MASSERT(featureEnabled || !IsAzaliaMandatory());
            }
            return featureEnabled;
        }

        // -----  GF100GpuSubdevice::IsFeatureEnabled  -----
        case GPUSUB_STRAPS_PEX_PLL_EN_TERM100:
            // NOTE: the enable bit is 1 for G92+
            return Regs().Test32(MODS_PEXTDEV_BOOT_0_STRAP_PEX_PLL_EN_TERM100_EN);

        // -----  G92GpuSubdevice::IsFeatureEnabled  -----
        case GPUSUB_STRAPS_SUB_VENDOR:
            return Regs().Test32(MODS_PEXTDEV_BOOT_0_STRAP_SUB_VENDOR_BIOS);

        case GPUSUB_STRAPS_SLOT_CLK_CFG:
            return Regs().Test32(MODS_PEXTDEV_BOOT_3_STRAP_1_SLOT_CLK_CFG_ENABLE);

        case GPUSUB_SUPPORTS_AZALIA_LOOPBACK:
            return IsFeatureEnabled(GPUSUB_HAS_AZALIA_CONTROLLER);

        default:
            Printf(Tee::PriNormal,
                   "IsFeatureEnabled not supported for feature %d\n",
                   feature);
            return false;
    }
}

string GpuSubdevice::GetRefManualFile() const
{
    string filename = GpuPtr()->DeviceIdToString(DeviceId());

    // Letters 'F' and 'S' are removed before returning manual name as they are special chips and use the
    // base chip's manual to look up registers. Since GA10F is a regular chip and has it's own manual,
    // the letter 'F' should not be removed for the chip GA10F
    if (!filename.empty() && (filename != "GA10F") &&
        ((filename.at(filename.size() - 1) == 'S') ||
         (filename.at(filename.size() - 1) == 'F')))
    {
        filename.resize(filename.size() - 1);
    }
    filename += "_ref.txt";

    transform(filename.begin(), filename.end(), filename.begin(), ::tolower);
    return filename;
}

bool GpuSubdevice::HasBug(UINT32 bugNum) const
{
    bool overridenHasBug;
    if (GpuPtr()->GetHasBugOverride(GetGpuInst(), bugNum, &overridenHasBug))
    {
        return overridenHasBug;
    }
    return Device::HasBug(bugNum);
}

// GPIO functions
bool GpuSubdevice::GpioIsOutput(UINT32 whichGpio)
{
    if (GpuPtr()->IsRmInitCompleted())
    {
        // ancient API...
        LwU32 in[1]  = {whichGpio};
        LwU32 out[1] = {0};
        LW_CFGEX_RESERVED_PROPERTY property;

        property.Property = PROPERTY_GPIO_GET_DIRECTION;
        property.In       = LW_PTR_TO_LwP64(in);
        property.Out      = LW_PTR_TO_LwP64(out);

        if (OK != LwRmPtr()->ConfigGetEx(LW_CFGEX_RESERVED,
                                         &property,
                                         sizeof(property),
                                         this))
        {
            Printf(Tee::PriNormal,
                   "Error reading output enabled on GPIO %d\n",
                   whichGpio);
            return false;
        }

        return (out[0] == LW_CFGEX_RESERVED_PROPERTY_GPIO_DIRECTION_OUT);
    }
    else
    {
       return Regs().Test32(MODS_PMGR_GPIO_OUTPUT_CNTL_IO_OUT_EN_YES, whichGpio);
    }
}

void GpuSubdevice::GpioSetOutput(UINT32 whichGpio, bool level)
{
    RC rc;

    if (GpuPtr()->IsRmInitCompleted())
    {
        // First we need to set the direction to Output
        LwU32 in[2]  = {whichGpio, LW_CFGEX_RESERVED_PROPERTY_GPIO_DIRECTION_OUT};
        LW_CFGEX_RESERVED_PROPERTY property;

        property.Property = PROPERTY_GPIO_SET_DIRECTION;
        property.In       = LW_PTR_TO_LwP64(in);
        property.Out      = LW_PTR_TO_LwP64(0);

        if (OK != (rc = LwRmPtr()->ConfigSetEx(LW_CFGEX_RESERVED,
                                               &property,
                                               sizeof(property),
                                               this)))
        {
            Printf(Tee::PriNormal,
                   "Error setting direction on GPIO %d. Rc = %d\n",
                   whichGpio, UINT32(rc));
        }

        // Now actually write the value to the output pin
        in[1]  = (UINT32)level;

        property.Property = PROPERTY_GPIO_WRITE_OUT;

        if (OK != (rc = LwRmPtr()->ConfigSetEx(LW_CFGEX_RESERVED,
                                               &property,
                                               sizeof(property),
                                              this)))
        {
            Printf(Tee::PriNormal,
                   "Error setting value %d on GPIO %d. Rc = %d\n",
                   level, whichGpio, rc.Get());
        }
    }
    else
    {
        // First set the direction to output
        Regs().Write32(MODS_PMGR_GPIO_OUTPUT_CNTL_IO_OUT_EN_YES, whichGpio);
        // Then set the output
        Regs().Write32(MODS_PMGR_GPIO_OUTPUT_CNTL_IO_OUTPUT, whichGpio, level ? 1 : 0);
        // Trigger the GPIO change
        Regs().Write32(MODS_PMGR_GPIO_OUTPUT_CNTL_TRIGGER_UPDATE_TRIGGER);
    }
}

bool GpuSubdevice::GpioGetOutput(UINT32 whichGpio)
{
    if (!GpioIsOutput(whichGpio))
    {
        Printf(Tee::PriLow, "GPIO %d is not enabled for output.\n", whichGpio);
        return false;
    }

    if (GpuPtr()->IsRmInitCompleted())
    {
        // ancient API...
        LwU32 in[1]  = {whichGpio};
        LwU32 out[1] = {0};
        LW_CFGEX_RESERVED_PROPERTY property;

        property.Property = PROPERTY_GPIO_READ_OUT;
        property.In       = LW_PTR_TO_LwP64(in);
        property.Out      = LW_PTR_TO_LwP64(out);

        if (OK != LwRmPtr()->ConfigGetEx(LW_CFGEX_RESERVED,
                                         &property,
                                         sizeof(property),
                                         this))
        {
            Printf(Tee::PriNormal,
                   "Error reading output enabled on GPIO %d.\n",
                   whichGpio);
            return false;
        }

        return (out[0] != 0);
    }
    else
    {
       return Regs().Read32(MODS_PMGR_GPIO_OUTPUT_CNTL_IO_OUTPUT, whichGpio) != 0;
    }
}

bool GpuSubdevice::GpioGetInput(UINT32 whichGpio)
{
    if (GpuPtr()->IsRmInitCompleted())
    {
        // ancient API...
        LwU32 in[1]  = {whichGpio};
        LwU32 out[1] = {0};
        LW_CFGEX_RESERVED_PROPERTY property;

        property.Property = PROPERTY_GPIO_READ_IN;
        property.In       = LW_PTR_TO_LwP64(in);
        property.Out      = LW_PTR_TO_LwP64(out);

        if (OK != LwRmPtr()->ConfigGetEx(LW_CFGEX_RESERVED,
                                        &property,
                                        sizeof(property),
                                        this))
        {
            Printf(Tee::PriNormal, "Error reading input on GPIO %d.\n", whichGpio);
            return false;
        }

        return (out[0] != 0);
    }
    else
    {
       return Regs().Read32(MODS_PMGR_GPIO_OUTPUT_CNTL_IO_INPUT, whichGpio) != 0;
    }
}

RC GpuSubdevice::SetGpioDirection(UINT32 gpioNum, GpioDirection direction)
{
    UINT32 numGpios = Regs().LookupAddress(MODS_PMGR_GPIO_OUTPUT_CNTL__SIZE_1);
    if (gpioNum > numGpios)
    {
        Printf(Tee::PriError, "The chip only has %u GPIOs.\n", numGpios);
        return RC::BAD_PARAMETER;
    }

    Regs().Write32(MODS_PMGR_GPIO_OUTPUT_CNTL_IO_OUT_EN, gpioNum, direction == OUTPUT ? 1 : 0);
    return RC::OK;
}

RC GpuSubdevice::SetDebugState(string name, UINT32 value)
{
    Printf(Tee::PriError, "No debug states implemented\n");
    return RC::BAD_PARAMETER;
}

//-----------------------------------------------------------------------------
//! \brief Setup MODS bugs from the RM bug database
//
//! Only bugs from the RM database should be setup here.  All other MODS
//! specific bugs database entries should be setup in the appropriate subdevice
//! SetupBugs function
RC GpuSubdevice::SetupRmBugs()
{
    RC      rc = OK;
    LwRmPtr pLwRm;

    if (!IsDFPGA()) // Bug 200023889
    {
        // CheetAh does not support LW2080_CTRL_CMD_GR_GET_CAPS_V2 as of now
        // Use old control call for it
        if (IsSOC() && Platform::UsesLwgpuDriver())
        {
            UINT08  capsGr[LW0080_CTRL_GR_CAPS_TBL_SIZE] = { };
            LW0080_CTRL_GR_GET_CAPS_PARAMS paramsGr = { };

            paramsGr.capsTbl = LW_PTR_TO_LwP64(capsGr);
            paramsGr.capsTblSize = LW0080_CTRL_GR_CAPS_TBL_SIZE;

            CHECK_RC(pLwRm->Control(pLwRm->GetDeviceHandle(m_Parent),
                                    LW0080_CTRL_CMD_GR_GET_CAPS,
                                    &paramsGr,
                                    sizeof (paramsGr)));

            if (LW0080_CTRL_GR_GET_CAP(capsGr,
                                       LW0080_CTRL_GR_CAPS_M2M_LINE_COUNT_BUG_232480))
            {
                SetHasBug(232480);
            }
        }
        else
        {
            LW2080_CTRL_GR_GET_CAPS_V2_PARAMS paramsGr = { };

            if (IsSMCEnabled())
            {
                SetGrRouteInfo(&(paramsGr.grRouteInfo), GetSmcEngineIdx());
            }
            CHECK_RC(pLwRm->Control(pLwRm->GetSubdeviceHandle(this),
                                    LW2080_CTRL_CMD_GR_GET_CAPS_V2,
                                    &paramsGr,
                                    sizeof (paramsGr)));

            if (LW0080_CTRL_GR_GET_CAP(paramsGr.capsTbl,
                                       LW0080_CTRL_GR_CAPS_M2M_LINE_COUNT_BUG_232480))
            {
                SetHasBug(232480);
            }
        }
    }

    UINT08  capsFb[LW0080_CTRL_FB_CAPS_TBL_SIZE] = { };
    LW0080_CTRL_FB_GET_CAPS_PARAMS paramsFb = { };

    paramsFb.capsTbl = LW_PTR_TO_LwP64(capsFb);
    paramsFb.capsTblSize = LW0080_CTRL_FB_CAPS_TBL_SIZE;

    CHECK_RC(pLwRm->Control(pLwRm->GetDeviceHandle(m_Parent),
                            LW0080_CTRL_CMD_FB_GET_CAPS,
                            &paramsFb,
                            sizeof (paramsFb)));

    if (LW0080_CTRL_FB_GET_CAP(capsFb,
                               LW0080_CTRL_FB_CAPS_ISO_FETCH_ALIGN_BUG_561630))
    {
        SetHasBug(561630);
    }

    // RM does not always update the param.flag field on LPWR control calls
    SetHasBug(2225586);

    return rc;
}

//-----------------------------------------------------------------------------
//! \brief Setup MODS bugs from OpenGL driver bugs
RC GpuSubdevice::SetupOglBugs()
{
    // TexCoord x or y are not guaranteed to be present when needed by
    // PointSprites with SetPointSpriteSelect.RMode = FROM_S or FROM_R modes.
    SetHasBug(394858);

    // SPIRV Meshlet shaders don't work when accessing light arrays using for loops
    SetHasBug(2541586);
    return RC::OK;
}

//-----------------------------------------------------------------------------
//! \brief Setup global features
void GpuSubdevice::SetupFeatures()
{
    if (m_Initialized)
    {
        // Only Rm queried features should be setup here
        if (FLD_TEST_DRF(2080, _CTRL_BUS_INFO, _GPU_GART_FLAGS_UNIFIED,
                         _TRUE, GpuGartFlags()))
        {
            SetHasFeature(GPUSUB_HAS_UNIFIED_GART_FLAGS);
        }

        if (m_BusInfo.caps.data & LW2080_CTRL_BUS_INFO_CAPS_CHIP_INTEGRATED)
        {
            SetHasFeature(GPUSUB_CHIP_INTEGRATED);
        }

        if (FLD_TEST_DRF(2080, _CTRL_BUS_INFO, _COHERENT_DMA_FLAGS_GPUGART,
                         _TRUE, CFlags()))
        {
            SetHasFeature(GPUSUB_HAS_COHERENT_SYSMEM);
        }

        if (UsableFbHeapSize(useNonProtected) > 0)
        {
            SetHasFeature(GPUSUB_HAS_FB_HEAP);
        }

        if (UsableFbHeapSize(useProtected) > 0)
        {
            SetHasFeature(GPUSUB_HAS_VIDEO_PROTECTED_REGION);
        }

        // Setup features that require the system to be properly initialized
        // (i.e. LwRm can be accessed, all GpuSubdevice data setup, registers
        // can be accessed, etc.)
        if (IsDFPGA())
        {
            // The GF110F FPGA doesn't support paging, because the only
            // engine is the display engine.  For now, we'll work around
            // this by pretending that GF110F supports segmentation, so we
            // don't get RM errors we alloc Surface2D::CtxDmaGpu.

            SetHasFeature(GPUSUB_SUPPORTS_GPU_SEGMENTATION);

            ClearHasFeature(GPUSUB_SUPPORTS_PAGING);

            // The GF110F/2/3 FPGA doesn't support block linear allocations.
            ClearHasFeature(GPUSUB_HAS_BLOCK_LINEAR);
        }

#if LWCFG(GLOBAL_GPU_IMPL_G000)
        if (DeviceId() != Gpu::AMODEL)
#endif
        {
            if (UsableFbHeapSize(useNonProtected) +
                UsableFbHeapSize(useProtected) > 0 &&
                !IsFbBroken() &&
                !IsSOC() &&
                Platform::GetSimulationMode() != Platform::RTL)
            {
                SetHasFeature(GPUSUB_SUPPORTS_EDC);
            }
        }

        // GF100 to GV100 have a BAR1 remapper
        if (LwRmPtr()->IsClassSupported(GF100_REMAPPER, GetParentDevice()))
            SetHasFeature(GPUSUB_HAS_BAR1_REMAPPER);
    }
    else
    {
#if !defined(PPC64LE)
        // PPC64LE must use cached sysmem
        SetHasFeature(GPUSUB_HAS_NONCOHERENT_SYSMEM);
#endif
        SetHasFeature(GPUSUB_HAS_SYSMEM);
        SetHasFeature(GPUSUB_HAS_32BPP_Z);
        SetHasFeature(GPUSUB_SUPPORTS_FSAA);
        SetHasFeature(GPUSUB_SUPPORTS_FSAA8X);        // Setup features that are independant of system state

#if LWCFG(GLOBAL_GPU_IMPL_G000)
        if (DeviceId() != Gpu::AMODEL)
#endif
        {
            SetHasFeature(GPUSUB_CAN_HAVE_PSTATE20);

            // -----  LW50GpuSubdevice::SetupFeatures  -----
            // G80 and above support block linear
            SetHasFeature(GPUSUB_HAS_BLOCK_LINEAR);

            // G80 and above can select undetected displays
            SetHasFeature(GPUSUB_ALLOW_UNDETECTED_DISPLAYS);

            // GPUs G80 and greater support paging
            SetHasFeature(GPUSUB_SUPPORTS_PAGING);

            // GPUs G80 and greater have GPFIFO channel
            SetHasFeature(GPUSUB_HAS_GPFIFO_CHANNEL);

            // GPUs G80 and greater have ISO displays
            SetHasFeature(GPUSUB_HAS_ISO_DISPLAY);

            // GPUs LW40 and above support for clamping texels with > 32bits
            SetHasFeature(GPUSUB_SUPPORTS_CLAMP_GT_32BPT_TEXTURES);

            // GPUs above LW30 support I2C
            SetHasFeature(GPUSUB_HAS_I2C);

            // GPUs LW50 and above support quick suspend/resume rmtest feature
            SetHasFeature(GPUSUB_HAS_QUICK_SUSPEND_RESUME);

            // G80+ support hybrid p2p mapping in RM.
            SetHasFeature(GPUSUB_SUPPORTS_RM_HYBRID_P2P);

            // Clock pulsing thermal-slowdown feature.
            SetHasFeature(GPUSUB_HAS_CLOCK_THROTTLE);

            // -----  G82GpuSubdevice::SetupFeatures  -----
            // G82+ support a MODS console
            SetHasFeature(GPUSUB_SUPPORTS_MODS_CONSOLE);

            // -----  G92GpuSubdevice::SetupFeatures  -----
            // G92+ have these straps
            SetHasFeature(GPUSUB_STRAPS_SUB_VENDOR);
            SetHasFeature(GPUSUB_STRAPS_PEX_PLL_EN_TERM100);
            SetHasFeature(GPUSUB_STRAPS_SLOT_CLK_CFG);

            // G92+ Can check for FB calibration errors
            SetHasFeature(GPUSUB_CAN_CHECK_FB_CALIBRATION);

            SetHasFeature(GPUSUB_HAS_BOARDS_JS_ENTRIES);

            // -----  GT200GpuSubdevice::SetupFeatures  -----
            // All tesla2 gpus support 16 and 24-bit depth surfaces.
            SetHasFeature(GPUSUB_HAS_16BPP_Z);
            SetHasFeature(GPUSUB_HAS_24BPP_Z);

            // GT200 and above support C24 Scanout
            SetHasFeature(GPUSUB_HAS_C24_SCANOUT);

            // GT200 and above have compression tags
            SetHasFeature(GPUSUB_HAS_COMPRESSION_TAGS);

            // GT200 and above can support compression (other than GT206)
            SetHasFeature(GPUSUB_HAS_COMPRESSION);

            // G94 and above have I2C Control
            SetHasFeature(GPUSUB_HAS_LW2080_CTRL_I2C);

            // G94 and above have Hybrid/Aux Pads
            SetHasFeature(GPUSUB_SUPPORTS_HYBRID_PADS);

            // -----  GT212GpuSubdevice::SetupFeatures()  -----
            SetHasFeature(GPUSUB_HAS_KFUSES);

            // GT212 and beyond have a texture coalescer
            SetHasFeature(GPUSUB_HAS_TEXTURE_COALESCER);

            // GT21X gpus contain azalia controllers.
            SetHasFeature(GPUSUB_HAS_AZALIA_CONTROLLER);
            SetHasFeature(GPUSUB_SUPPORTS_AZALIA_LOOPBACK);

            SetHasFeature(GPUSUB_HAS_CLOCK_THROTTLE);

            SetHasFeature(GPUSUB_HAS_ELPG);

            SetHasFeature(GPUSUB_HAS_PDISP_RG_RASTER_EXTEND);

            // Gpu supports Get/SetPLLCfg(), Set/IsPLLLocked(), IsPLLEnabled()
            SetHasFeature(GPUSUB_SUPPORTS_PLL_QUERIES);

            // GF100 and  beyond have a method macro expander
            SetHasFeature(GPUSUB_HAS_MME);

            // check for ASPMs with PEX tests (started in GT215)
            SetHasFeature(GPUSUB_SUPPORTS_ASPM_OPTION_IN_PEXTESTS);

            // Gpu supports tessellation shaders (Fermi+)
            SetHasFeature(GPUSUB_SUPPORTS_TESSELLATION);

            SetHasFeature(GPUSUB_SUPPORTS_ECC);

            // Gpu supports L2 mode changing in the middle of a MODS run
            SetHasFeature(GPUSUB_SUPPORTS_RUNTIME_L2_MODE_CHANGE);

            // Gpu TSEC engine supports the GFE application
            SetHasFeature(GPUSUB_TSEC_SUPPORTS_GFE_APPLICATION);

            // Gpu LwLink supports HS->SAFE-HS cycling
            SetHasFeature(GPUSUB_SUPPORTS_LWLINK_HS_SAFE_CYCLING);

            SetHasFeature(GPUSUB_HAS_ONBOARD_PWR_SENSORS);

            // SCAL_NUM_CES is the number of logical CEs, CE_PCE2LCE_CONFIG__SIZE_1
            // is the number of physical. If CE_PCE2LCE_CONFIG__SIZE_1 then LCEs are
            // not present in the chip and therefore the mapping would always be
            // considered 1:1
            //
            // MODS cannot control one-to-one with kernel mode RM so dont try and enable
            // one-to-one mode
            if (IsSOC() ||
                (Platform::HasClientSideResman() &&
                 (!Regs().IsSupported(MODS_CE_PCE2LCE_CONFIG__SIZE_1) ||
                  !Regs().IsSupported(MODS_PTOP_SCAL_NUM_CES) ||
                  (Regs().Read32(MODS_PTOP_SCAL_NUM_CES) >=
                   Regs().LookupAddress(MODS_CE_PCE2LCE_CONFIG__SIZE_1)))))
            {
                SetHasFeature(GPUSUB_SUPPORTS_LCE_PCE_ONE_TO_ONE);
            }

            if (Regs().IsSupported(MODS_XVE_RESET_CTRL_GPU_RESET))
            {
                SetHasFeature(GPUSUB_HAS_GPU_RESET);
            }

            if (Regs().IsSupported(MODS_XP_L1P_ENTRY_COUNT))
            {
                SetHasFeature(GPUSUB_SUPPORTS_ASPM_L1P);
            }
        }
    }
}

//-----------------------------------------------------------------------------
//! \brief Setup global features
bool GpuSubdevice::Is32bitChannel()
{
    MASSERT(!"Not implemented\n");
    return false;
}

RC GpuSubdevice::GetPmu(PMU **ppPmu)
{
    // Validate that this GPU has a PMU
    if (!m_pPmu)
        return RC::UNSUPPORTED_FUNCTION;
    *ppPmu = m_pPmu;

    return OK;
}

//------------------------------------------------------------------------------
//! \brief Get SEC2 for this platform
RC GpuSubdevice::GetSec2Rtos(Sec2Rtos **ppSec2)
{
    // Validate that this GPU has a SEC2
    if (!m_pSec2Rtos)
        return RC::UNSUPPORTED_FUNCTION;

    *ppSec2 = m_pSec2Rtos;

    return OK;
}

//--------------------------------------------------------------------
//! \brief Return true if any interrupt bits are asserted
//!
bool GpuSubdevice::HasPendingInterrupt()
{
    if (m_InStandBy || m_InGc6 || m_PausePollInterrupts)
    {
        return false;
    }

    const unsigned numIntrTrees = GetNumIntrTrees();
    for (unsigned ii = 0; ii < numIntrTrees; ++ii)
    {
        if (GetIntrStatus(ii) != 0)
        {
            return true;
        }
    }
    return false;
}

RC GpuSubdevice::ClearStuckInterrupts()
{
    RC rc;

    // There shouldn't be any interrupts asserted.  If there are, let's clear
    // them out.
    if (HasPendingInterrupt())
    {
        for (const auto& entry: m_InterruptMap)
        {
            if (Regs().Read32(entry.topAddr, entry.topIndex) & entry.topMask)
            {
                const char *topName  = Regs().ColwertToString(entry.topAddr);
                const char *leafName = Regs().ColwertToString(entry.leafAddr);
                if (topName)
                {
                    Printf(Tee::PriNormal, "%s(%d) bit 0x%08x high.\n",
                           topName, entry.topIndex, entry.topMask);
                }

                UINT32 leafValue = entry.leafValue;
                if (entry.leafMask != 0xffffffff)
                {
                    leafValue |=
                        Regs().Read32(entry.leafAddr, entry.leafIndex) &
                        ~entry.leafMask;
                }
                if (leafName)
                {
                    Printf(Tee::PriNormal,
                        "Trying to clear interrupt by writing 0x%x to %s(%d)\n",
                        leafValue, leafName, entry.leafIndex);
                }
                Regs().Write32(entry.leafAddr, entry.leafIndex, leafValue);
                // Pause to allow the updated interrupt enable bit to affect
                // the corresponding interrupt status bit.
                Tasker::Sleep(0);
            }
        }

        // Check to see if we succcessfully cleared all interrupt bits
        //
        const unsigned numIntrTrees = GetNumIntrTrees();
        for (unsigned ii = 0; ii < numIntrTrees; ++ii)
        {
            UINT32 intStatus = GetIntrStatus(ii);
            if (intStatus != 0)
            {
                Printf(Tee::PriError,
                    "Couldn't clear GPU's interrupt state; int%d == 0x%08x.\n",
                    ii, intStatus);
                rc = RC::IRQ_STUCK_ASSERTED;
            }
        }
        if (rc != OK)
        {
            return rc;
        }

        // Print message if we successfully cleared all interrupt bits
        //
        Printf(Tee::PriNormal,
               "Successfully cleared GPU's interrupt state.\n");
    }

    return rc;
}

void GpuSubdevice::IntRegClear()
{
    m_InterruptMap.clear();
}

//--------------------------------------------------------------------
//! \brief Map the interrupt bits to the registers that control them
//!
//! Simplified version of IntRegAdd which clears a named
//! register field, and always writes the whole leaf register
//! (i.e. leafMask = 0xffffffff).  This is the 90% case in
//! pre-Turing chips.
//!
void GpuSubdevice::IntRegAdd
(
    ModsGpuRegField   topField,
    UINT32            topIndex,
    ModsGpuRegAddress leafAddr,
    UINT32            leafIndex,
    UINT32            leafValue
)
{
    if (Regs().IsSupported(topField))
    {
        IntRegAdd(Regs().ColwertToAddress(topField), topIndex,
                  Regs().LookupMask(topField), leafAddr, leafIndex,
                  0xffffffffU, leafValue);
    }
}

//--------------------------------------------------------------------
//! \brief Map the interrupt bits to the registers that control them
//!
//! This method is called in the constructor to map the
//! interrupt-status bits to one or more registers.  The mapping is
//! used by ClearStuckInterrupts(), on the theory that writing a value
//! to the associated register should clear the interrupt-status bit.
//!
//! Do nothing if the registers do not exist on this GPU.
//!
//! \param topAddr The interrupt register that has bit(s) to clear
//! \param topIndex Index of topAddr register array, or 0 if topAddr
//!     is not a register array
//! \param topMask The bit(s) to clear in topAddr
//! \param leafAddr The interrupt register that controls the bit(s) in
//!     topAddr
//! \param leafIndex Index of leafAddr register array
//! \param leafMask Mask to write to leafAddr to clear the bit(s)
//! \param leafValue Value to write to leafAddr to clear the bit(s)
//!
void GpuSubdevice::IntRegAdd
(
    ModsGpuRegAddress topAddr,
    UINT32            topIndex,
    UINT32            topMask,
    ModsGpuRegAddress leafAddr,
    UINT32            leafIndex,
    UINT32            leafMask,
    UINT32            leafValue
)
{
    MASSERT(topMask != 0);
    MASSERT(leafMask != 0);
    MASSERT((leafValue & ~leafMask) == 0);

    if (Regs().IsSupported(topAddr, topIndex) &&
        Regs().IsSupported(leafAddr, leafIndex))
    {
        InterruptMapEntry newEntry = {};
        newEntry.topAddr   = topAddr;
        newEntry.topIndex  = topIndex;
        newEntry.topMask   = topMask;
        newEntry.leafAddr  = leafAddr;
        newEntry.leafIndex = leafIndex;
        newEntry.leafMask  = leafMask;
        newEntry.leafValue = leafValue;
        m_InterruptMap.push_back(newEntry);
    }
}

//------------------------------------------------------------------------------
RefManual *GpuSubdevice::GetRefManual() const
{
    const auto iter = m_ManualMap.find(DeviceId());

    // Checking for the existence of the manual file is done in
    // EarlyInitialization.  If one was not created there then there is no
    // reason to even attempt to parse the map, instead it is necessary to
    // return a blank unparsed manual (API users rely on never receiving a
    // null pointer from this function)
    if (iter == m_ManualMap.end())
        return &s_EmptyRefManual;

    // Lazy parse the map only if it was not previously parsed and only if it
    // was actually requested.  Parsing the map can take ~25s and that
    // effectively doubles some arch test lengths on emulation
    if (!iter->second.ParseTried())
    {
        string filename = GetRefManualFile();
        if (iter->second.Parse(filename.c_str(),
            IsEmulation() || Platform::ManualCachingEnabled()) != OK)
            Printf(Tee::PriWarn, "Could not parse %s\n", filename.c_str());
    }
    return &iter->second;
}
void GpuSubdevice::SetRomVersion(string romVersion)
{
    m_RomVersion = romVersion;
}

//------------------------------------------------------------------------------
RC GpuSubdevice::FbFlush(FLOAT64 timeOutMs)
{
    // write arbitrary number to LW_UFLUSH_FB_FLUSH
    Regs().Write32(MODS_UFLUSH_FB_FLUSH, 3);
    return POLLWRAP_HW(&PollFbFlush, this, timeOutMs);
}

const string& GpuSubdevice::GetRomVersion()
{
    // Check to see if the rom version is already cached in m_RomVersion.
    // If not, then retrieve it from BIOS and cache it.

    if (m_RomVersion.empty())
    {
        LW2080_CTRL_BIOS_INFO biosInfo[2] = { };
        biosInfo[0].index = LW2080_CTRL_BIOS_INFO_INDEX_REVISION;
        biosInfo[1].index = LW2080_CTRL_BIOS_INFO_INDEX_OEM_REVISION;
        LW2080_CTRL_BIOS_GET_INFO_PARAMS params = { };
        params.biosInfoListSize = 2;
        params.biosInfoList = LW_PTR_TO_LwP64(biosInfo);
        UINT32 biosRevision = 0, biosOemRevision = 0;
        if (OK == LwRmPtr()->ControlBySubdevice(
                                this,
                                LW2080_CTRL_CMD_BIOS_GET_INFO,
                                &params, sizeof(params)))
        {
            biosRevision = biosInfo[0].data;
            biosOemRevision = biosInfo[1].data;
        }

        // Cache the BIOS version for this device
        if (IsSOC() && biosRevision == 0 && biosOemRevision == 0)
        {
            m_RomVersion = "";
        }
        else
        {
            m_RomVersion = Utility::StrPrintf("%x.%02x.%02x.%02x.%02x",
                (biosRevision >> 24) & 0xFF,
                (biosRevision >> 16) & 0xFF,
                (biosRevision >>  8) & 0xFF,
                (biosRevision >>  0) & 0xFF,
                biosOemRevision & 0xFF);
        }
    }

    // Return the cached rom version

    return m_RomVersion;
}

//-----------------------------------------------------------------------------
RC GpuSubdevice::GetRomVersion(string* pVersion)
{
    MASSERT(pVersion);

    *pVersion = GetRomVersion();

    return RC::OK;
}

//Update the internal strings with the VBIOS Project Info metadata. If the call fails or the data
//is invalid don't update the string(s).
RC GpuSubdevice::GetRomProjectInfo()
{
    if ((Platform::GetSimulationMode() == Platform::Amodel) ||
        (Platform::GetSimulationMode() == Platform::RTL) ||
        IsSOC())
    {
        return RC::UNSUPPORTED_FUNCTION;
    }
    RC rc;
    LW2080_CTRL_BIOS_GET_PROJECT_INFO_PARAMS params = { };
    CHECK_RC(LwRmPtr()->ControlBySubdevice(
                        this,
                        LW2080_CTRL_CMD_BIOS_GET_PROJECT_INFO,
                        &params,
                        sizeof(params)));
    if (params.partner[0] != '\0')
    {
        m_RomPartner = (char *)params.partner;
    }
    if (params.projectId[0] != '\0')
    {
        m_RomProjectId = (char *)params.projectId;
    }
    return rc;
}

//Return the string containing the VBIOS Project Info metadata. If the call fails just return
//the empty string.
const string& GpuSubdevice::GetRomProjectId()
{
    if (m_RomProjectId.empty())
    {
        GetRomProjectInfo();
    }
    return m_RomProjectId;
}

//Return the string containing the VBIOS Project Info metadata. If the call fails just return
//the empty string.
const string& GpuSubdevice::GetRomPartner()
{
    if (m_RomPartner.empty())
    {
        GetRomProjectInfo();
    }
    return m_RomPartner;
}

string GpuSubdevice::GetRomType()
{
    RC rc;

    if (IsSOC())
        return "";

    if (m_RomType == (UINT32)~0)
    {
        UINT32 romType;

        // GetBiosInfo will return 0 for a value if the RM control call fails
        // which is not the behavior we want here
        rc = GetBiosInfo(LW2080_CTRL_BIOS_INFO_INDEX_VBIOS_SELWRITY_TYPE,
                         &romType);
        if (OK == rc)
            m_RomType = romType;
    }

    if (rc == OK)
    {
        return RomTypeToString(m_RomType);
    }

    return "";
}

UINT32 GpuSubdevice::GetRomTypeEnum()
{
    RC rc;
    if (m_RomType == (UINT32)~0)
    {
        UINT32 romType;

        // GetBiosInfo will return 0 for a value if the RM control call fails
        // which is not the behavior we want here
        rc = GetBiosInfo(LW2080_CTRL_BIOS_INFO_INDEX_VBIOS_SELWRITY_TYPE,
                         &romType);
        if (OK == rc)
            m_RomType = romType;
    }

    return m_RomType;
}

UINT32 GpuSubdevice::GetRomTimestamp()
{
    if (m_RomTimestamp == (UINT32)~0)
    {
        UINT32 romTimestamp;
        RC rc;

        // GetBiosInfo will return 0 for a value if the RM control call fails
        // which is not the behavior we want here
        rc = GetBiosInfo(LW2080_CTRL_BIOS_INFO_INDEX_VBIOS_SELWRITY_CREATION,
                         &romTimestamp);
        if ((OK == rc) && romTimestamp)
            m_RomTimestamp = romTimestamp;
    }
    return m_RomTimestamp;
}

UINT32 GpuSubdevice::GetRomExpiration()
{
    if (m_RomExpiration == (UINT32)~0)
    {
        UINT32 romExpiration;
        RC rc;

        // GetBiosInfo will return 0 for a value if the RM control call fails
        // which is not the behavior we want here
        rc = GetBiosInfo(LW2080_CTRL_BIOS_INFO_INDEX_VBIOS_SELWRITY_EXPIRATION,
                         &romExpiration);
        if ((OK == rc) && romExpiration)
            m_RomExpiration = romExpiration;
    }
    return m_RomExpiration;
}

UINT32 GpuSubdevice::GetRomSelwrityStatus()
{
    RC rc;
    if (m_RomSelwrityStatus == (UINT32)~0)
    {
        UINT32 selwrityStatus;

        // GetBiosInfo will return 0 for a value if the RM control call fails
        // which is not the behavior we want here
        rc = GetBiosInfo(LW2080_CTRL_BIOS_INFO_INDEX_VBIOS_STATUS,
                         &selwrityStatus);
        if (OK == rc)
            m_RomSelwrityStatus = selwrityStatus;
    }

    return m_RomSelwrityStatus;
}

string GpuSubdevice::GetRomOemVendor()
{
    LW2080_CTRL_BIOS_GET_OEM_INFO_PARAMS oemBiosInfo;
    memset(&oemBiosInfo, 0, sizeof(LW2080_CTRL_BIOS_GET_OEM_INFO_PARAMS));
    if (OK == LwRmPtr()->ControlBySubdevice(this,
                                            LW2080_CTRL_CMD_BIOS_GET_OEM_INFO,
                                            &oemBiosInfo,
                                            sizeof(oemBiosInfo)))
    {
        if (oemBiosInfo.selwreVendorName[0] != '\0')
        {
            return (char *)oemBiosInfo.selwreVendorName;
        }
        else if (oemBiosInfo.vendorName[0] != '\0')
        {
            return (char *)oemBiosInfo.vendorName;
        }
    }

    return "";
}

RC GpuSubdevice::GetEmulationInfo
(
    LW208F_CTRL_GPU_GET_EMULATION_INFO_PARAMS * pEmulationInfo
)
{
    StickyRC rc;
    MASSERT(pEmulationInfo);
    LwRm::Handle hSubDevDiag;
    hSubDevDiag = GetSubdeviceDiag();
    memset(pEmulationInfo, 0, sizeof(*pEmulationInfo));
    if (IsSMCEnabled())
    {
        SetGrRouteInfo(&(pEmulationInfo->grRouteInfo), GetSmcEngineIdx());
    }
    CHECK_RC(LwRmPtr()->Control(hSubDevDiag,
                                LW208F_CTRL_CMD_GPU_GET_EMULATION_INFO,
                                pEmulationInfo, sizeof(*pEmulationInfo)));
    return rc;
}

RC GpuSubdevice::ValidateVbios
(
    bool         bIgnoreFailure,
    VbiosStatus *pStatus,
    bool        *pbIgnored
)
{
    if (IsSOC())
    {
        return OK;
    }

    StickyRC rc;
    LW2080_CTRL_BIOS_GET_INFO_PARAMS vbiosParams;
    LW2080_CTRL_BIOS_INFO vbiosInfo[3];

    MASSERT(pStatus);
    MASSERT(pbIgnored);
    *pStatus   = VBIOS_ERROR; // Set to "error" in case the caller won't
                              // check rc and the RM ctrl call below fails
    *pbIgnored = false;

    memset(vbiosInfo, 0, sizeof(vbiosInfo));
    vbiosInfo[0].index = LW2080_CTRL_BIOS_INFO_INDEX_VBIOS_SELWRITY_TYPE;
    vbiosInfo[1].index = LW2080_CTRL_BIOS_INFO_INDEX_VBIOS_STATUS;
    vbiosInfo[2].index = LW2080_CTRL_BIOS_INFO_INDEX_VBIOS_SELWRITY_EXPIRATION;
    vbiosParams.biosInfoListSize = 3;
    vbiosParams.biosInfoList = LW_PTR_TO_LwP64(vbiosInfo);

    CHECK_RC(LwRmPtr()->ControlBySubdevice(this,
                                           LW2080_CTRL_CMD_BIOS_GET_INFO,
                                           &vbiosParams,
                                           sizeof(vbiosParams)));

    // Cache the information for later use
    m_RomType = vbiosInfo[0].data;
    m_RomSelwrityStatus = vbiosInfo[1].data;
    if (vbiosInfo[2].data)
        m_RomExpiration = vbiosInfo[2].data;

    const bool bBypassPresent =
        (Utility::GetSelwrityUnlockLevel() == Utility::SUL_LWIDIA_NETWORK_BYPASS) ||
        (Utility::GetSelwrityUnlockLevel() == Utility::SUL_BYPASS_UNEXPIRED);

    switch (m_RomSelwrityStatus)
    {
        case LW2080_CTRL_BIOS_INFO_STATUS_EXPIRED:
        case LW2080_CTRL_BIOS_INFO_STATUS_DEVID_MISMATCH:
            if (!bIgnoreFailure)
                break;

            // else fall-through
                // no break

        case LW2080_CTRL_BIOS_INFO_STATUS_OK:
            *pStatus = VBIOS_OK;

            // RM is OK with this vbios.  Mods is pickier, let's check further.
            switch (m_RomType)
            {
                case LW2080_CTRL_BIOS_INFO_SELWRITY_TYPE_LWIDIA_DEBUG:
                {
                    bool bDisallowVbiosSelwrityBypass = false;
                    JavaScriptPtr pJs;

                    // No need to check return value, if it fails the variable
                    // will not be changed.
                    (void)pJs->GetProperty(pJs->GetGlobalObject(),
                                           "g_DisallowVbiosSelwrityBypass",
                                           &bDisallowVbiosSelwrityBypass);

                    // Allowed with engineering packages when bypass.js is used.
                    if (!bDisallowVbiosSelwrityBypass && bBypassPresent)
                    {
                        *pStatus   = VBIOS_WARNING;
                        *pbIgnored = true;
                        Printf(Tee::PriWarn, "Unexpired LWD VBIOS Bypassed\n");
                    }
                    else
                    {
                        *pStatus = VBIOS_ERROR;
                        *pbIgnored = bIgnoreFailure;
                    }
                    break;
                }

                case LW2080_CTRL_BIOS_INFO_SELWRITY_TYPE_PARTNER_DEBUG:
                case LW2080_CTRL_BIOS_INFO_SELWRITY_TYPE_LWIDIA_AE_DEBUG:
                    // Mods allows these in engineering packages only.
                    *pStatus   = VBIOS_WARNING;
                    *pbIgnored = bIgnoreFailure;
                    break;

                case LW2080_CTRL_BIOS_INFO_SELWRITY_TYPE_LWIDIA_RELEASE:
                case LW2080_CTRL_BIOS_INFO_SELWRITY_TYPE_PARTNER:
                case LW2080_CTRL_BIOS_INFO_SELWRITY_TYPE_UNSIGNED:
                    // These types are ok in all packages
                    break;

                default:
                    // Any new types of signing should be an error until a policy
                    // decision is made for how MODS should handle them
                    *pStatus   = VBIOS_ERROR;
                    *pbIgnored = bIgnoreFailure;
                    break;
            }
            break;

        default:
            if (bBypassPresent)
            {
                Printf(Tee::PriWarn,
                       "VBIOS Status Error=%u Bypassed\n",
                       m_RomSelwrityStatus);
                *pbIgnored = bIgnoreFailure;
                break;
            }
            return RC::VBIOS_CERT_ERROR;
    }

    if (!bIgnoreFailure && (*pStatus == VBIOS_ERROR))
        return RC::VBIOS_CERT_ERROR;
    else
        return OK;
}

Memory::Location GpuSubdevice::FixOptimalLocation(Memory::Location loc)
{
    if (loc == Memory::Optimal)
        return ((FbHeapSize() != 0) || m_L2CacheOnlyMode) ? Memory::Fb : Memory::NonCoherent;
    return loc;
}

RC GpuSubdevice::CheckXenCompatibility() const
{
    bool passthroughCompatible = true;
    if ((m_pPciDevInfo->PhysLwBase + m_pPciDevInfo->LwApertureSize) >
        0x3FFFFFFFFFFLL)
    {
        passthroughCompatible = false;
    }

    if ((m_pPciDevInfo->PhysFbBase + m_pPciDevInfo->FbApertureSize) >
        0x3FFFFFFFFFFLL)
    {
        passthroughCompatible = false;
    }

    if ((m_pPciDevInfo->PhysInstBase + m_pPciDevInfo->InstApertureSize) >
        0x3FFFFFFFFFFLL)
    {
        passthroughCompatible = false;
    }

    if (!passthroughCompatible)
    {
        Printf(Tee::PriWarn,
            "GPU on PCI %04x:%02x:%02x.%x"
            " is not setup correctly for passthrough mode\n",
            m_pPciDevInfo->PciDomain,
            m_pPciDevInfo->PciBus,
            m_pPciDevInfo->PciDevice,
            m_pPciDevInfo->PciFunction);
    }

    bool vGPUCompatible = true;
    if ((m_pPciDevInfo->PhysFbBase + m_pPciDevInfo->FbApertureSize) >
        0xFFFFFFFF)
    {
        vGPUCompatible = false;
    }

    if ((m_pPciDevInfo->PhysInstBase + m_pPciDevInfo->InstApertureSize) >
        0xFFFFFFFF)
    {
        vGPUCompatible = false;
    }

    if (!vGPUCompatible)
    {
        Printf(Tee::PriError,
               "GPU on PCI %04x:%02x:%02x.%x"
               " is not setup correctly for vGPU mode\n",
               m_pPciDevInfo->PciDomain,
               m_pPciDevInfo->PciBus,
               m_pPciDevInfo->PciDevice,
               m_pPciDevInfo->PciFunction);
    }

    if (!passthroughCompatible || !vGPUCompatible)
    {
        Printf(Tee::PriNormal, "   BAR0 base = 0x%016llx, size = 0x%08x\n",
            m_pPciDevInfo->PhysLwBase, m_pPciDevInfo->LwApertureSize);
        Printf(Tee::PriNormal, "   BAR1 base = 0x%016llx, size = 0x%08llx\n",
            m_pPciDevInfo->PhysFbBase, m_pPciDevInfo->FbApertureSize);
        Printf(Tee::PriNormal, "   BAR2 base = 0x%016llx, size = 0x%08llx\n",
            m_pPciDevInfo->PhysInstBase, m_pPciDevInfo->InstApertureSize);
    }

    if (!vGPUCompatible)
        return RC::ILWALID_DEVICE_BAR;

    return OK;
}

//-----------------------------------------------------------------------------
bool GpuSubdevice::CanL2BeDisabled()
{
    if (HasBug(1735434) && IsEccFuseEnabled())
    {
        // L2 cant be disabled if ECC fuse is blown
        return false;
    }
    else
    {
        RC rc;
        bool bSupported = false;
        UINT32 enableMask = 0;
        rc = GetEccEnabled(&bSupported, &enableMask);
        return !(rc == RC::OK && bSupported && Ecc::IsUnitEnabled(Ecc::Unit::DRAM, enableMask));
    }
}

//-----------------------------------------------------------------------------
RC GpuSubdevice::SetL2Mode(L2Mode mode)
{
    RC rc;
    if (GetFB()->GetL2CacheSize() == 0)
    {
        // no L2
        return OK;
    }

    if (!CanL2BeDisabled() && mode == L2_DISABLED)
    {
        Printf(Tee::PriWarn,
               "FB ECC requires L2 enabled. L2_BYPASS request is ignored.\n");
        return OK;
    }

    const LwRm::Handle hSubDevDiag = GpuSubdevice::GetSubdeviceDiag();
    if (hSubDevDiag == 0)
    {
        return OK;
    }

    LwRmPtr pLwRm;

    // Before we can change the cache control mode we MUST ilwalidate & clean the
    // existing cache or undefined behavior can result. see B1705999
    IlwalidateL2Cache(L2_ILWALIDATE_CLEAN);

    LW208F_CTRL_FB_CTRL_GPU_CACHE_PARAMS params = {0};
    switch (mode)
    {
        case L2_DEFAULT:
            params.bypassMode =
                LW208F_CTRL_FB_CTRL_GPU_CACHE_BYPASS_MODE_DEFAULT;
            break;
        case L2_DISABLED:
            params.bypassMode =
                LW208F_CTRL_FB_CTRL_GPU_CACHE_BYPASS_MODE_ENABLED;
            break;
        case L2_ENABLED:
            params.bypassMode =
                LW208F_CTRL_FB_CTRL_GPU_CACHE_BYPASS_MODE_DISABLED;
            break;
        default:
            return RC::BAD_PARAMETER;
    }

    rc = pLwRm->Control(hSubDevDiag,
                        LW208F_CTRL_CMD_FB_CTRL_GPU_CACHE,
                        &params,
                        sizeof(params));
    return rc;
}
//-----------------------------------------------------------------------------
RC GpuSubdevice::GetL2Mode(L2Mode *pMode) const
{
    MASSERT(pMode);
    if (GetFB()->GetL2CacheSize() == 0)
    {
        // no L2
        *pMode = L2_DEFAULT;
        return OK;
    }
    RC rc;
    LwRmPtr pLwRm;
    LW2080_CTRL_FB_GET_GPU_CACHE_INFO_PARAMS params = {0};
    CHECK_RC(pLwRm->ControlBySubdevice(this,
                                       LW2080_CTRL_CMD_FB_GET_GPU_CACHE_INFO,
                                       &params,
                                       sizeof(params)));
    switch (params.bypassMode)
    {
        case LW2080_CTRL_FB_GET_GPU_CACHE_INFO_BYPASS_MODE_DISABLED:
            *pMode = L2_ENABLED;
            break;
        case LW2080_CTRL_FB_GET_GPU_CACHE_INFO_BYPASS_MODE_ENABLED:
            *pMode = L2_DISABLED;
            break;
        default:
            return RC::SOFTWARE_ERROR;
    }

    return OK;
}
//-----------------------------------------------------------------------------
//! \brief Ilwalidate the L2 cache
/* virtual */ RC GpuSubdevice::IlwalidateL2Cache(UINT32 flags)
{
    RC rc;
    LwRmPtr pLwRm;
    LW2080_CTRL_FB_FLUSH_GPU_CACHE_PARAMS flushParams = { };

    flushParams.flags = DRF_DEF(2080, _CTRL_FB_FLUSH_GPU_CACHE_FLAGS, _APERTURE, _VIDEO_MEMORY) |
                        DRF_DEF(2080, _CTRL_FB_FLUSH_GPU_CACHE_FLAGS, _ILWALIDATE, _YES) |
                        DRF_DEF(2080, _CTRL_FB_FLUSH_GPU_CACHE_FLAGS, _FLUSH_MODE, _FULL_CACHE) |
                        DRF_DEF(2080, _CTRL_FB_FLUSH_GPU_CACHE_FLAGS, _FB_FLUSH, _NO);

    if (flags & L2_ILWALIDATE_CLEAN)
    {
        flushParams.flags = FLD_SET_DRF(2080, _CTRL_FB_FLUSH_GPU_CACHE_FLAGS, _WRITE_BACK, _YES,
                                        flushParams.flags);
    }

    if (flags & L2_FB_FLUSH)
    {
        flushParams.flags = FLD_SET_DRF(2080, _CTRL_FB_FLUSH_GPU_CACHE_FLAGS, _FB_FLUSH, _YES,
                                        flushParams.flags);
    }

    CHECK_RC(pLwRm->Control(pLwRm->GetSubdeviceHandle(this),
                            LW2080_CTRL_CMD_FB_FLUSH_GPU_CACHE,
                            &flushParams, sizeof(flushParams)));

    return rc;
}
//-----------------------------------------------------------------------------
//! \brief Check if the given address is BAR0 address
bool GpuSubdevice::IsBar0VirtAddr(const volatile void* addr) const
{
    const volatile char *pBar0 =
        reinterpret_cast<const volatile char*>(m_pPciDevInfo->LinLwBase);
    const volatile char *pMem = reinterpret_cast<const volatile char*>(addr);

    return (pBar0 <= pMem) && (pMem < pBar0 + LwApertureSize());
}

//-----------------------------------------------------------------------------
//! \brief Check if the given address is BAR1 address, and optionally,
//!        return the corresponding GPU virutal address
bool GpuSubdevice::IsBar1VirtAddr
(
    const volatile void* addr,
    UINT64* pGpuVirtAddr
) const
{
    Tasker::MutexHolder mutexHolder(m_pGetBar1OffsetMutex);
    LwRmPtr pLwRm;
    LW2080_CTRL_FB_GET_BAR1_OFFSET_PARAMS params = {0};

    // Re-entrance check: the following RM control call might cause this
    // function ilwoked relwrsively. Add some logic to stop it (very hacky).
    static bool first_time = true;
    if (!first_time)
    {
        return false;
    }

    if (Platform::GetLwrrentIrql() != Platform::NormalIrql)
    {
        return false;
    }

    first_time = false;
    params.cpuVirtAddress = LW_PTR_TO_LwP64(addr);

    if (pLwRm->Control(pLwRm->GetSubdeviceHandle(this),
                       LW2080_CTRL_CMD_FB_GET_BAR1_OFFSET,
                       &params, sizeof(params)) == OK)
    {
        if (pGpuVirtAddr)
        {
            *pGpuVirtAddr = params.gpuVirtAddress;
        }
        first_time = true;
        return true;
    }
    first_time = true;
    return false;
}

RC GpuSubdevice::DetectVbridge(bool *pResult)
{
    RC rc;

    MASSERT(pResult);

    LW0000_CTRL_GPU_GET_SLI_CONFIG_INFO_PARAMS sliInfoParams;
    CHECK_RC(LwRmPtr()->Control(LwRmPtr()->GetClientHandle(),
                                LW0000_CTRL_CMD_GPU_GET_SLI_CONFIG_INFO,
                                &sliInfoParams,
                                sizeof(sliInfoParams)));
    *pResult = FLD_TEST_DRF(0000, _CTRL_GPU_SLI_INFO, _VIDLINK, _PRESENT,
                            sliInfoParams.sliConfig.sliInfo);

    return OK;
}

/* virtual */ int GpuSubdevice::EscapeWrite
(
    const char *path,
    UINT32 index,
    UINT32 size,
    UINT32 value
)
{
    return Platform::GpuEscapeWrite(m_GpuId, path, index, size, value);
}

/* virtual */ int GpuSubdevice::EscapeRead
(
    const char *path,
    UINT32 index,
    UINT32 size,
    UINT32 *pValue
)
{
    return Platform::GpuEscapeRead(m_GpuId, path, index, size, pValue);
}

/* virtual */ int GpuSubdevice::EscapeWriteBuffer
(
    const char *path,
    UINT32 index,
    size_t size,
    const void *pBuf
)
{
    return Platform::GpuEscapeWriteBuffer(m_GpuId, path, index, size, pBuf);
}

/* virtual */ int GpuSubdevice::EscapeReadBuffer
(
    const char *path,
    UINT32 index,
    size_t size,
    void *pBuf
) const
{
    return Platform::GpuEscapeReadBuffer(m_GpuId, path, index, size, pBuf);
}

//------------------------------------------------------------------------------
//! \brief Retrieve the number of FB calibration errors
//!
//! \param Reset - Resets the FB calibration error count to zero
RC GpuSubdevice::GetFBCalibrationCount
(
    FbCalibrationLockCount* pCount,
    bool reset
)
{
    RC rc;
    LwRmPtr pLwRm;

    MASSERT(pCount);

    memset(pCount, 0, sizeof(*pCount));

    pCount->flags = (reset ? LW2080_CTRL_CMD_FB_GET_CAL_FLAG_RESET :
                     LW2080_CTRL_CMD_FB_GET_CAL_FLAG_NONE);

    CHECK_RC(pLwRm->Control(
                    pLwRm->GetSubdeviceHandle(this),
                    LW2080_CTRL_CMD_FB_GET_CALIBRATION_LOCK_FAILED,
                    pCount,
                    sizeof(*pCount)));

    return rc;
}

/* static */ bool GpuSubdevice::SetBoot0Strap
(
    Gpu::LwDeviceId deviceId,
    void * linLwBase,
    UINT32 boot0Strap
)
{
    // This routine is called before the GpuSubdevice is created, so we can't use the
    // regular ModsRegHal interface to actually set the registers, however by creating
    // a local reghaltable it can be used to query offsets and fields
    RegHalTable regs;
    regs.Initialize(Device::LwDeviceId(deviceId));

    UINT32 lwrBoot0Strap = 0;
    INT32  openGL;
    JavaScriptPtr pJs;

    // Set Lwdqro strap?
    pJs->GetProperty(Gpu_Object, Gpu_StrapOpenGL, &openGL);
    lwrBoot0Strap = MEM_RD32((char *)linLwBase + regs.LookupAddress(MODS_PEXTDEV_BOOT_0));

    if (boot0Strap == 0xffffffff) // option '-boot_0_strap' not specified
    {
        INT32  strapRamcfg;
        UINT64 strapFb = GpuPtr()->GetStrapFb();

        // Default straps
        boot0Strap = lwrBoot0Strap;

        if ((strapFb != 0) ||
            (Platform::GetSimulationMode() != Platform::Hardware))
        {
            // We cannot change strap on emulator as it depends on the configuration.
            // So only change it when option -strap_fb presents
            switch (strapFb)
            {
                case 64:
                    regs.SetField(&boot0Strap, MODS_PEXTDEV_BOOT_0_STRAP_FB_64M);
                    break;
                case 128:
                    regs.SetField(&boot0Strap, MODS_PEXTDEV_BOOT_0_STRAP_FB_128M);
                    break;
                case 0:         // set to 256M if no specified from command line
                case 256:
                    regs.SetField(&boot0Strap, MODS_PEXTDEV_BOOT_0_STRAP_FB_256M);
                    break;
                case 512:
                    regs.SetField(&boot0Strap, MODS_PEXTDEV_BOOT_0_STRAP_FB_512M);
                    break;
                case 1024:
                    regs.SetField(&boot0Strap, MODS_PEXTDEV_BOOT_0_STRAP_FB_1G);
                    break;
                case 2048:
                    regs.SetField(&boot0Strap, MODS_PEXTDEV_BOOT_0_STRAP_FB_2G);
                    break;
                case 4096:
                    regs.SetField(&boot0Strap, MODS_PEXTDEV_BOOT_0_STRAP_FB_4G);
                    break;
                default:
                    // Note Odd "STRAP_FB" sizes are allowed on Amodel, since
                    // "-strap_fb" controls not only the BAR1 window but also the
                    // actual FB size there on Amodel.  However the window
                    // is limited to the values in LW_PEXTDEV_BOOT_0.
                    if (Platform::GetSimulationMode() != Platform::Amodel)
                    {
                        Printf(Tee::PriError,
                               "Invalid STRAP_FB setting : %llu\n", strapFb);
                        MASSERT(!"Generic assertion failure<refer to previous message>.");
                    }
                    regs.SetField(&boot0Strap, MODS_PEXTDEV_BOOT_0_STRAP_FB_512M);
                    break;
            }
        }
        pJs->GetProperty(Gpu_Object, Gpu_StrapRamcfg, &strapRamcfg);

        if ((strapRamcfg != -1) ||
            (Platform::GetSimulationMode() != Platform::Hardware))
        {
            if (strapRamcfg == -1)
                strapRamcfg = 0;
            regs.SetField(&boot0Strap, MODS_PEXTDEV_BOOT_0_STRAP_RAMCFG, strapRamcfg);
        }

#define QUADRO_PCI_DEVID_MIN   24U

        if ((openGL != -1) ||
            (Platform::GetSimulationMode() != Platform::Hardware))
        {
            if (openGL == -1)
                openGL = 0;
            if (openGL != 0)
            {
                Printf(Tee::PriLow, "Strap as Lwdqro\n");
                if (regs.GetField(boot0Strap,
                                  MODS_PEXTDEV_BOOT_0_STRAP_PCI_DEVID) < QUADRO_PCI_DEVID_MIN)
                {
                    regs.SetField(&boot0Strap,
                                  MODS_PEXTDEV_BOOT_0_STRAP_PCI_DEVID,
                                  QUADRO_PCI_DEVID_MIN);
                }
            }
            else
            {
                Printf(Tee::PriLow, "Strap as VdChip\n");
                if (regs.GetField(boot0Strap,
                                  MODS_PEXTDEV_BOOT_0_STRAP_PCI_DEVID) >= QUADRO_PCI_DEVID_MIN)
                {
                    regs.SetField(&boot0Strap, MODS_PEXTDEV_BOOT_0_STRAP_PCI_DEVID_INIT);
                }
            }
        }
    }
    else
    {
        Printf(Tee::PriNormal, "Boot0Strap set to 0x%x\n", boot0Strap);
    }

    if (Platform::GetSimulationMode() == Platform::RTL)
    {
        Platform::EscapeWrite("LW_PEXTDEV_BOOT_0", regs.LookupAddress(MODS_PEXTDEV_BOOT_0), 4,
                              boot0Strap);
    }

    if ((Platform::GetSimulationMode() == Platform::Hardware) &&
        (regs.GetField(boot0Strap, MODS_PEXTDEV_BOOT_0_STRAP_FB) >
         regs.GetField(lwrBoot0Strap, MODS_PEXTDEV_BOOT_0_STRAP_FB)))
    {
        Printf(Tee::PriError, "Cannot expand BAR1 on Emulation/Silicon!\n");
        return false;
    }

    if (boot0Strap != lwrBoot0Strap)
    {
        regs.SetField(&boot0Strap, MODS_PEXTDEV_BOOT_0_STRAP_OVERWRITE_ENABLED);
        MEM_WR32((char *)linLwBase + regs.LookupAddress(MODS_PEXTDEV_BOOT_0), boot0Strap);
    }

    // Lwdqro mode cannot be programmed/enabled on silicon.
    // Double check to make sure test does not request enabling this
    // mode on a vdchip board.
    if (openGL == 1)
    {
        UINT32 ptop_debug = MEM_RD32((char *) linLwBase + regs.LookupAddress(MODS_PTOP_DEBUG_1));
        Printf(Tee::PriError, "Cannot put chip in Lwdqro mode!\n");
        return regs.TestField(ptop_debug, MODS_PTOP_DEBUG_1_OPENGL_ON);
    }
    return true;
}

/* virtual */
bool GpuSubdevice::RegPowerGated(UINT32 offset) const
{
    if (((offset >= DRF_BASE(LW_PGRAPH)) && (offset <= DRF_EXTENT(LW_PGRAPH))) ||
        ((offset >= DRF_BASE(LW_PMSPDEC)) && (offset <= DRF_EXTENT(LW_PMSPDEC))) ||
        ((offset >= DRF_BASE(LW_PMSVLD)) && (offset <= DRF_EXTENT(LW_PMSVLD))) ||
        ((offset >= DRF_BASE(LW_PMSPPP)) && (offset <= DRF_EXTENT(LW_PMSPPP))))
    {
        return (HasFeature(GPUSUB_SUPPORTS_TPC_PG) || HasFeature(GPUSUB_HAS_ELPG));
    }

    return false;
}

/* virtual */
bool GpuSubdevice::RegPrivProtected(UINT32 offset) const
{
    return false;
}

/* virtual */
bool GpuSubdevice::RegDecodeTrapped(UINT32 offset) const
{
    return false;
}

RC GpuSubdevice::SetVerboseJtag(bool enable)
{
    m_VerboseJtag = enable;
    return OK;
}

bool GpuSubdevice::GetVerboseJtag()
{
    return m_VerboseJtag;
}

RC GpuSubdevice::SetGpuLocId(UINT32 locId)
{
    m_GpuLocId = locId;
    return RC::OK;
}

//-------------------------------------------------------------------------------
RC GpuSubdevice::ReserveClockThrottle(ClockThrottle ** pClockThrottle)
{
    if (!m_pClockThrottle)
        return RC::SOFTWARE_ERROR;
    if (m_ClockThrottleReserved)
        return RC::RESOURCE_IN_USE;

    m_ClockThrottleReserved = true;
    *pClockThrottle = m_pClockThrottle;

    return OK;
}

RC GpuSubdevice::ReleaseClockThrottle(const ClockThrottle * pClockThrottle)
{
    MASSERT(m_pClockThrottle);
    MASSERT(m_pClockThrottle == pClockThrottle);
    MASSERT(m_ClockThrottleReserved);

    RC rc;

    if (m_pClockThrottle)
    {
        // Just in case the test failed to release.  Extra Release is harmless.
        rc = m_pClockThrottle->Release();
    }
    else
    {
        rc = RC::SOFTWARE_ERROR;
    }

    m_ClockThrottleReserved = false;
    return rc;
}

//------------------------------------------------------------------------------
//! \brief Get the list of named properties that is valid for this particular
//!        GPU
void GpuSubdevice::GetNamedPropertyList(set<string> *pNamedProperties)
{
    map<string, NamedProperty>::const_iterator pNamedProperty;

    for (pNamedProperty = m_NamedProperties.begin();
         pNamedProperty != m_NamedProperties.end(); pNamedProperty++)
    {
        pNamedProperties->insert(pNamedProperty->first);
    }
}

//------------------------------------------------------------------------------
//! \brief Get a pointer to the specified named property
RC GpuSubdevice::GetNamedProperty(string name, NamedProperty **pProperty)
{
    if (m_NamedProperties.count(name) == 0)
    {
        Printf(Tee::PriLow, "Unknown init property : %s\n", name.c_str());
        return RC::SOFTWARE_ERROR;
    }
    *pProperty = &m_NamedProperties.at(name);
    return RC::OK;
}

//------------------------------------------------------------------------------
//! \brief Get a pointer to the specified named property
RC GpuSubdevice::GetNamedProperty(string name, const NamedProperty **pProperty) const
{
    if (m_NamedProperties.count(name) == 0)
    {
        Printf(Tee::PriLow, "Unknown init property : %s\n", name.c_str());
        return RC::SOFTWARE_ERROR;
    }
    *pProperty = &m_NamedProperties.at(name);
    return RC::OK;
}

//------------------------------------------------------------------------------
//! \brief Get value of the specified named property
bool GpuSubdevice::GetNamedPropertyValue(const string& name) const
{
    bool value = false;
    const NamedProperty *pProp;
    if (RC::OK == GetNamedProperty(name, &pProp))
    {
        if (RC::OK != pProp->Get(&value))
        {
            Printf(Tee::PriLow, "Gpu:Initialize:: Failed to read %s property.\n", name.c_str());
            value = false;
        }
    }
    return value;
}

//------------------------------------------------------------------------------
//! \brief Check if the named property exists
bool GpuSubdevice::HasNamedProperty(string name) const
{
    return m_NamedProperties.count(name) != 0;
}

//------------------------------------------------------------------------------
//! \brief Add a named property to the list
void GpuSubdevice::AddNamedProperty
(
    string name,
    const NamedProperty &namedProperty
)
{
    if (m_NamedProperties.count(name) != 0)
    {
        Printf(Tee::PriWarn, "NamedProperty %s already exists, skipping!\n",
               name.c_str());
        return;
    }

    m_NamedProperties[name] = namedProperty;
}

// Default constructor needed by map<>
GpuSubdevice::NamedProperty::NamedProperty()
{
    m_Type = Invalid;
    m_Data.clear();
    m_IsSet = false;
}
GpuSubdevice::NamedProperty::NamedProperty(bool bDefault)
{
    m_Type = Bool;
    m_Data.push_back(static_cast<UINT32>(bDefault));
    m_IsSet = false;
}
GpuSubdevice::NamedProperty::NamedProperty(UINT32 uDefault)
{
    m_Type = Uint32;
    m_Data.push_back(uDefault);
    m_IsSet = false;
}
GpuSubdevice::NamedProperty::NamedProperty(UINT32 size, UINT32 uDefault)
{
    m_Type = Array;
    m_Data.resize(size, uDefault);
    m_IsSet = false;
}
GpuSubdevice::NamedProperty::NamedProperty(const string &sDefault)
    : m_sData(sDefault)
{
    m_Type = String;
    m_IsSet = false;
}
UINT32 GpuSubdevice::NamedProperty::GetArraySize()
{
    if (m_Type != Array)
        return 0;

    return static_cast<UINT32>(m_Data.size());
}
RC GpuSubdevice::NamedProperty::Set(bool value)
{
    RC rc;
    CHECK_RC(ValidateType(Bool, __FUNCTION__));
    m_Data[0] = value ? 1 : 0;
    m_IsSet = true;
    return rc;
}
RC GpuSubdevice::NamedProperty::Set(UINT32 value)
{
    RC rc;
    CHECK_RC(ValidateType(Uint32, __FUNCTION__));
    m_Data[0] = value;
    m_IsSet = true;
    return rc;
}
RC GpuSubdevice::NamedProperty::Set(const string &value)
{
    RC rc;
    CHECK_RC(ValidateType(String, __FUNCTION__));
    m_sData = value;
    m_IsSet = true;
    return rc;
}
RC GpuSubdevice::NamedProperty::Set(const vector<UINT32> &values)
{
    RC rc;
    CHECK_RC(ValidateType(Array, __FUNCTION__));
    if (values.size() > m_Data.size())
    {
        Printf(Tee::PriError, "NamedProperty array too large\n");
        return RC::SOFTWARE_ERROR;
    }
    for (UINT32 i = 0; i < values.size(); i++)
    {
        m_Data[i] = values[i];
    }
    m_IsSet = true;
    return rc;
}
RC GpuSubdevice::NamedProperty::Get(bool *pValue) const
{
    RC rc;
    CHECK_RC(ValidateType(Bool, __FUNCTION__));
    *pValue = m_Data[0] ? true : false;
    return rc;
}
RC GpuSubdevice::NamedProperty::Get(UINT32 *pValue) const
{
    RC rc;
    CHECK_RC(ValidateType(Uint32, __FUNCTION__));
    *pValue = m_Data[0];
    return rc;
}
RC GpuSubdevice::NamedProperty::Get(string *pValue) const
{
    RC rc;
    CHECK_RC(ValidateType(String, __FUNCTION__));
    *pValue = m_sData;
    return rc;
}
RC GpuSubdevice::NamedProperty::Get(vector<UINT32> *pValues) const
{
    RC rc;
    CHECK_RC(ValidateType(Array, __FUNCTION__));
    pValues->assign(m_Data.begin(), m_Data.end());
    return rc;
}
RC GpuSubdevice::NamedProperty::ValidateType
(
    NamedPropertyType type,
    string function
) const
{
    if (m_Type != type)
    {
        Printf(Tee::PriError,
               "%s type mismatch, requested type %d is not actual %d\n",
               function.c_str(), type, m_Type);
        return RC::SOFTWARE_ERROR;
    }
    return OK;
}

//------------------------------------------------------------------------------
/* virtual */ void GpuSubdevice::GetMmeToMem2MemRegInfo
(
    bool bEnable,
    UINT32 *pOffset,
    UINT32 *pMask,
    UINT32 *pData
)
{
    MASSERT(pOffset);
    MASSERT(pMask);
    MASSERT(pData);

    *pOffset = Regs().LookupAddress(MODS_PGRAPH_PRI_MME_CONFIG);
    *pMask = Regs().LookupMask(MODS_PGRAPH_PRI_MME_CONFIG_MME_TO_M2M_INLINE);
    if (bEnable)
    {
        *pData = Regs().SetField(MODS_PGRAPH_PRI_MME_CONFIG_MME_TO_M2M_INLINE_ENABLED);
    }
    else
    {
        *pData = Regs().SetField(MODS_PGRAPH_PRI_MME_CONFIG_MME_TO_M2M_INLINE_DISABLED);
    }
}

//------------------------------------------------------------------------------
//!
//! \brief Size of MME Instruction RAM in 32-bit chunks.
//!
/* virtual */ UINT32 GpuSubdevice::GetMmeInstructionRamSize()
{
    // Number of possible 32-bit MME instructions
    return 2048;
}

//------------------------------------------------------------------------------
//!
//! \brief Size of the MME Start Address RAM in 32-bit chunks.
//!
/* virtual */ UINT32 GpuSubdevice::GetMmeStartAddrRamSize()
{
    // Number of possible 32-bit MME Start Addresses
    return 128;
}

//------------------------------------------------------------------------------
//!
//! \brief Size of the MME Data RAM in 32-bit chunks.
//!
/* virtual */ UINT32 GpuSubdevice::GetMmeDataRamSize()
{
    return 0;
}

//------------------------------------------------------------------------------
//!
//! \brief Size of the MME Shadow RAM in 32-bit chunks.
//!
/* virtual */ UINT32 GpuSubdevice::GetMmeShadowRamSize()
{
    // Number of possible 32-bit MME Shadowed Methods
    return 2048;
}

//------------------------------------------------------------------------------
/* virtual */ UINT32 GpuSubdevice::GetMmeVersion()
{
    // This will be used by MMESimulator to pick the macros in order
    // to get the value of method released by MME
    return 1;
}

//------------------------------------------------------------------------------
/* virtual */ RC GpuSubdevice::MmeShadowRamWrite(const MmeMethod method,
                                                 UINT32 data)
{
    const MmeMethods mmeMethods = GetMmeShadowMethods();

    if (!mmeMethods.count(method))
    {
        Printf(Tee::PriError,
               "Unknown MME shadow method Class 0x%04x, Method 0x%04x\n",
               method.ShadowClass, method.ShadowMethod);
        return RC::SOFTWARE_ERROR;
    }

    Regs().Write32(MODS_PGRAPH_PRI_MME_SHADOW_RAM_DATA, data);

    UINT32 val = 0;
    Regs().SetField(&val, MODS_PGRAPH_PRI_MME_SHADOW_RAM_INDEX_LWCLASS, method.ShadowClass);
    Regs().SetField(&val, MODS_PGRAPH_PRI_MME_SHADOW_RAM_INDEX_METHOD_ADDR, method.ShadowMethod);
    Regs().SetField(&val, MODS_PGRAPH_PRI_MME_SHADOW_RAM_INDEX_WRITE_TRIGGER);
    Regs().Write32(MODS_PGRAPH_PRI_MME_SHADOW_RAM_INDEX, val);

    return POLLWRAP_HW(PollMmeWriteComplete, this, MME_WRITE_TIMEOUT);
}

//------------------------------------------------------------------------------
/* virtual */ RC GpuSubdevice::MmeShadowRamRead(const MmeMethod method,
                                                UINT32 *pData)
{
    const MmeMethods mmeMethods = GetMmeShadowMethods();
    RC rc;

    *pData = 0;
    if (!mmeMethods.count(method))
    {
        Printf(Tee::PriError,
               "Unknown MME shadow method Class 0x%04x, method 0x%04x\n",
               method.ShadowClass, method.ShadowMethod);
        return RC::SOFTWARE_ERROR;
    }

    UINT32 val = 0;
    Regs().SetField(&val, MODS_PGRAPH_PRI_MME_SHADOW_RAM_INDEX_LWCLASS, method.ShadowClass);
    Regs().SetField(&val, MODS_PGRAPH_PRI_MME_SHADOW_RAM_INDEX_METHOD_ADDR, method.ShadowMethod);
    Regs().SetField(&val, MODS_PGRAPH_PRI_MME_SHADOW_RAM_INDEX_READ_TRIGGER);
    Regs().Write32(MODS_PGRAPH_PRI_MME_SHADOW_RAM_INDEX, val);

    CHECK_RC(POLLWRAP_HW(PollMmeReadComplete, this, MME_WRITE_TIMEOUT));

    *pData = Regs().Read32(MODS_PGRAPH_PRI_MME_SHADOW_RAM_DATA);
    return rc;
}

//------------------------------------------------------------------------------
/* virtual */ RC GpuSubdevice::LoadMmeShadowData()
{
    return RC::UNSUPPORTED_FUNCTION;
}

//------------------------------------------------------------------------------
/* virtual */ GpuSubdevice::MmeMethods GpuSubdevice::GetMmeShadowMethods()
{
    if (m_MmeShadowMethods.size() == 0)
    {
        LoadMmeShadowData();
    }
    return m_MmeShadowMethods;
}

//------------------------------------------------------------------------------
void GpuSubdevice::AddMmeShadowMethod(UINT32 shadowClass,
                                      UINT32 shadowMethod,
                                      bool bShared)
{
    MmeMethod method;
    method.ShadowClass = shadowClass;
    method.ShadowMethod = shadowMethod;
    m_MmeShadowMethods[method] = bShared;
}

//------------------------------------------------------------------------------
bool GpuSubdevice::MmeMethod::operator<
(
    const GpuSubdevice::MmeMethod &rhs
) const
{
    if (ShadowClass != rhs.ShadowClass)
        return (ShadowClass < rhs.ShadowClass);

    return (ShadowMethod < rhs.ShadowMethod);
}

void GpuSubdevice::SetEarlyReg(string reg, UINT32 value)
{
    UINT32 regNum;
    if (isdigit(reg[0]))
    {
        regNum = strtoul(reg.c_str(), nullptr, 16);
    }
    else
    {
        if (GetRefManual() && GetRefManual()->FindRegister(reg.c_str()))
        {
            regNum = GetRefManual()->FindRegister(reg.c_str())->GetOffset();
        }
        else
        {
            Printf(Tee::PriWarn,
                   "Ref manual not present register not found on %s\n"
                   "Skipping early register write of %s\n",
                   GetName().c_str(),
                   reg.c_str());
            return;
        }
    }
    m_EarlyRegsMap[regNum] = value;
}

void GpuSubdevice::ApplyEarlyRegsChanges(bool inPreVBIOSSetup)
{
    if (!inPreVBIOSSetup)
        return;

    map<UINT32, UINT32>::const_iterator cursor = m_EarlyRegsMap.begin();
    for (; cursor != m_EarlyRegsMap.end(); cursor++)
    {
        Printf(Tee::PriNormal, "Warning: Writing 0x%08x register with value "
            "0x%08x early - but it can be still overwritten by "
            "VBIOS/RM/MODS later.\n", cursor->first, cursor->second);

        m_RestoreEarlyRegsMap[cursor->first] = RegRd32(cursor->first);
        RegWr32(cursor->first, cursor->second);
    }
}

void GpuSubdevice::RestoreEarlyRegs()
{
    map<UINT32, UINT32>::const_iterator cursor = m_RestoreEarlyRegsMap.begin();
    while (cursor != m_RestoreEarlyRegsMap.end())
    {
        RegWr32(cursor->first, cursor->second);
        cursor++;
    }
}

RC GpuSubdevice::GetRamSvop(SvopValues *pSvop)
{
    RC rc;
    LwRmPtr pLwRm;
    LwRm::Handle hSubDevDiag;

    MASSERT(pSvop);

    hSubDevDiag = GetSubdeviceDiag();
    rc = pLwRm->Control(hSubDevDiag,
                        LW208F_CTRL_CMD_GPU_GET_RAM_SVOP_VALUES,
                        pSvop,
                        sizeof(SvopValues));
    return rc;
}

RC GpuSubdevice::SetRamSvop(SvopValues *pSvop)
{
    RC rc;
    LwRmPtr pLwRm;
    LwRm::Handle hSubDevDiag;

    MASSERT(pSvop);

    hSubDevDiag = GetSubdeviceDiag();
    rc = pLwRm->Control(hSubDevDiag,
                        LW208F_CTRL_CMD_GPU_SET_RAM_SVOP_VALUES,
                        pSvop,
                        sizeof(SvopValues));
    return rc;
}

//------------------------------------------------------------------------------
RC GpuSubdevice::DisableEccInfoRomReporting()
{
    if (!HasFeature(GPUSUB_SUPPORTS_ECC))
    {
        return OK;
    }

    LwRmPtr pLwRm;
    LW2080_CTRL_GPU_QUERY_AGGREGATE_ECC_STATUS_PARAMS params;
    memset(&params, 0, sizeof(params));
    if (OK == pLwRm->ControlBySubdevice(this,
            LW2080_CTRL_CMD_GPU_QUERY_AGGREGATE_ECC_STATUS,
            &params, sizeof(params)))
    {
        if (params.sbe.count || params.dbe.count)
        {
            Printf(Tee::PriLow,
                    "GPU %u.%u has pre-existing ECC errors: "
                    "SBE=%llu, DBE=%llu\n",
                    GetParentDevice()->GetDeviceInst(),
                    GetSubdeviceInst(),
                    params.sbe.count, params.dbe.count);
        }
    }

    RC rc;
    LwRm::Handle hSubDevDiag;
    hSubDevDiag = GetSubdeviceDiag();

    rc = pLwRm->Control(hSubDevDiag,
            LW208F_CTRL_CMD_GPU_DISABLE_ECC_INFOROM_REPORTING,
            0, 0);

    if (RC::LWRM_NOT_SUPPORTED == rc)
    {
        m_EccInfoRomReportingDisabled = true;
        Printf(Tee::PriDebug,
               "ECC not supported, so we can't disable ECC reporting\n");
        return OK;
    }
    if (OK != rc)
    {
        Printf(Tee::PriError, "Unable to disable ECC reporting!\n");
    }
    else
    {
        m_EccInfoRomReportingDisabled = true;
        Printf(Tee::PriDebug, "ECC reporting disabled\n");
    }
    return rc;
}

//------------------------------------------------------------------------------
bool GpuSubdevice::IsEccInfoRomReportingDisabled() const
{
    return m_EccInfoRomReportingDisabled;
}

//------------------------------------------------------------------------------
RC GpuSubdevice::GetAllEccErrors(UINT32 *pCorr, UINT32 *pUncorr)
{
    MASSERT(pCorr);
    MASSERT(pUncorr);

    *pCorr = 0;
    *pUncorr = 0;

    if (!HasFeature(GPUSUB_SUPPORTS_ECC))
    {
        return OK;
    }

    RC rc;
    // TODO(stewarts): legacy API. Must update to ctrl2080ecc.h
    LW2080_CTRL_GPU_QUERY_AGGREGATE_ECC_STATUS_PARAMS aggParams;
    memset(&aggParams, 0, sizeof(aggParams));
    rc = LwRmPtr()->ControlBySubdevice(this,
                                LW2080_CTRL_CMD_GPU_QUERY_AGGREGATE_ECC_STATUS,
                                &aggParams, sizeof(aggParams));

    if (rc == RC::LWRM_NOT_SUPPORTED)
        rc.Clear();  // Suppress this error.
    CHECK_RC(rc);

    *pCorr += static_cast<UINT32>(aggParams.sbe.count);
    *pUncorr += static_cast<UINT32>(aggParams.dbe.count);

    LW2080_CTRL_GPU_QUERY_ECC_STATUS_PARAMS lwrParams;
    memset(&lwrParams, 0, sizeof(lwrParams));
    lwrParams.flags = DRF_DEF(2080_CTRL_GPU_QUERY_ECC_STATUS,
                              _FLAGS, _TYPE, _RAW);
    if (IsSMCEnabled())
    {
        SetGrRouteInfo(&(lwrParams.grRouteInfo), GetSmcEngineIdx());
    }
    rc = LwRmPtr()->ControlBySubdevice(this,
                                       LW2080_CTRL_CMD_GPU_QUERY_ECC_STATUS,
                                       &lwrParams, sizeof(lwrParams));

    if (rc == RC::LWRM_NOT_SUPPORTED)
        rc.Clear();  // Suppress this error.
    CHECK_RC(rc);

    for (UINT32 unit = 0; unit < LW2080_CTRL_GPU_ECC_UNIT_COUNT; unit++)
    {
        if (lwrParams.units[unit].enabled)
        {
            *pCorr += static_cast<UINT32>(lwrParams.units[unit].sbe.count);
            *pUncorr += static_cast<UINT32>(lwrParams.units[unit].dbe.count);
        }
    }

    return rc;
}

//------------------------------------------------------------------------------
GpuPerfmon * GpuSubdevice::CreatePerfmon()
{
    Printf(Tee::PriError, "GpuSubdevice::CreatePerfmon() is not supported!\n");
    return nullptr;
}

//------------------------------------------------------------------------------
RC GpuSubdevice::GetPerfmon(GpuPerfmon **ppPerfmon)
{
    // Validate that this GPU has a Perfmon
    if (m_pPerfmon == nullptr)
        return RC::UNSUPPORTED_FUNCTION;
    *ppPerfmon = m_pPerfmon;

    return OK;
}

//------------------------------------------------------------------------------
RC GpuSubdevice::GetISM(GpuIsm **ppISM)
{
    if (m_pISM.get() == nullptr)
    {
        return RC::UNSUPPORTED_FUNCTION;
    }
    *ppISM = m_pISM.get();
    return OK;
}

//-----------------------------------------------------------------------------
Ism* GpuSubdevice::GetIsm()
{
    return dynamic_cast<Ism*>(m_pISM.get());
}

//------------------------------------------------------------------------------
/* virtual */ unique_ptr<PciCfgSpace> GpuSubdevice::AllocPciCfgSpace() const
{
    return make_unique<PciCfgSpaceGpu>(this);
}

//------------------------------------------------------------------------------
bool GpuSubdevice::IsQuadroEnabled()
{
    unique_ptr<ScopedRegister> debug1;
    if (Regs().IsSupported(MODS_PTOP_DEBUG_1_FUSES_VISIBLE))
    {
        debug1 = make_unique<ScopedRegister>(this, Regs().LookupAddress(MODS_PTOP_DEBUG_1));
        Regs().Write32(MODS_PTOP_DEBUG_1_FUSES_VISIBLE_ENABLED);
    }

    bool fuseEnabled;

    if (Platform::GetSimulationMode() == Platform::Hardware)
    {
        fuseEnabled = !Regs().Test32(MODS_FUSE_OPT_OPENGL_EN_DATA_INIT);
    }
    else
    {
        fuseEnabled = true;
    }

    if (fuseEnabled)
    {
        return Regs().Test32(MODS_PTOP_DEBUG_1_OPENGL_ON);
    }

    return false;
}

bool GpuSubdevice::IsMsdecEnabled()
{
    unique_ptr<ScopedRegister> debug1;
    if (Regs().IsSupported(MODS_PTOP_DEBUG_1_FUSES_VISIBLE))
    {
        debug1 = make_unique<ScopedRegister>(this, Regs().LookupAddress(MODS_PTOP_DEBUG_1));
        Regs().Write32(MODS_PTOP_DEBUG_1_FUSES_VISIBLE_ENABLED);
    }

    if ((Regs().Test32(MODS_FUSE_OPT_MSDEC_DISABLE_DATA_INIT)) ||
        (Platform::GetSimulationMode() != Platform::Hardware))
    {
        return true;
    }

    return false;
}

bool GpuSubdevice::IsEccFuseEnabled()
{
    unique_ptr<ScopedRegister> debug1;
    if (Regs().IsSupported(MODS_PTOP_DEBUG_1_FUSES_VISIBLE))
    {
        debug1 = make_unique<ScopedRegister>(this, Regs().LookupAddress(MODS_PTOP_DEBUG_1));
        Regs().Write32(MODS_PTOP_DEBUG_1_FUSES_VISIBLE_ENABLED);
    }

    bool fuseEnabled;

    if ((Platform::GetSimulationMode() == Platform::RTL) ||
        (Platform::GetSimulationMode() == Platform::Hardware))
    {
        fuseEnabled = !Regs().Test32(MODS_FUSE_OPT_ECC_EN_DATA_INIT);
    }
    else
    {
        fuseEnabled = true;
    }

    return fuseEnabled;
}

bool GpuSubdevice::Is2XTexFuseEnabled()
{
    MASSERT(!"Is2XTexFuseEnabled() not supported for this devce");
    return false;
}

RC GpuSubdevice::GetInfoRomObjectVersion
(
    const char * objName,
    UINT32 *     pVersion,
    UINT32 *     pSubVersion
) const
{
    if (IsSOC())
    {
        return RC::LWRM_NOT_SUPPORTED;
    }

    MASSERT(pVersion);
    MASSERT(pSubVersion);
    RC rc;

    *pVersion = 0;
    *pSubVersion = 0;

    LW2080_CTRL_GPU_GET_INFOROM_OBJECT_VERSION_PARAMS verParams = { };
    const size_t objNameLen = min(strlen(objName), sizeof(verParams.objectType));
    memcpy(verParams.objectType, objName, objNameLen);
    rc = LwRmPtr()->ControlBySubdevice(this,
                                LW2080_CTRL_CMD_GPU_GET_INFOROM_OBJECT_VERSION,
                                &verParams,
                                sizeof(verParams));
    if (OK == rc)
    {
        *pVersion = static_cast<UINT32>(verParams.version);
        *pSubVersion = static_cast<UINT32>(verParams.subversion);
    }
    else if ((rc == RC::LWRM_ILWALID_OBJECT_ERROR) ||
             (rc == RC::LWRM_OBJECT_NOT_FOUND))
    {
        rc.Clear();
    }
    return rc;
}

RC GpuSubdevice::CheckEccErr(Ecc::Unit eclwnit, Ecc::ErrorType errType) const
{
    RC rc;
    UINT64 corr = 0;
    UINT64 uncorr = 0;

    Ecc::DetailedErrCounts detailedCounts;
    rc = GetAggregateEccInfo(eclwnit, &detailedCounts);

    if (rc != RC::OK)
    {
        if ((RC::LWRM_NOT_SUPPORTED == rc) ||
            (RC::UNSUPPORTED_FUNCTION == rc))
        {
            rc.Clear();
        }
        return rc;
    }

    corr = Utility::Accumulate(detailedCounts.eccCounts, &Ecc::ErrCounts::corr);
    uncorr = Utility::Accumulate(detailedCounts.eccCounts, &Ecc::ErrCounts::uncorr);

    if (((errType == Ecc::ErrorType::CORR) && (corr > 0)) ||
        ((errType == Ecc::ErrorType::UNCORR) && (uncorr > 0)))
    {
        return GetRC(eclwnit, errType);
    }

    return RC::OK;
}

RC GpuSubdevice::CheckInfoRomForEccErr(Ecc::Unit eclwnit, Ecc::ErrorType errType) const
{
    RC rc;
    LwRmPtr pLwRm;

    CHECK_RC(CheckInfoRom());

    UINT32  version, subVersion;
    CHECK_RC(GetInfoRomObjectVersion(LW2080_CTRL_GPU_INFOROM_OBJECT_NAME_ECC,
                                      &version, &subVersion));
    if ((version != 0) || (subVersion != 0))
    {
        // if we have entered here, we must support at least one
        MASSERT (GetSubdeviceGraphics() || GetSubdeviceFb() || GetFalconEcc());

        if (eclwnit != Ecc::Unit::MAX)
        {
            return CheckEccErr(eclwnit, errType);
        }

        for (Ecc::Unit lwrUnit = Ecc::Unit::FIRST; lwrUnit < Ecc::Unit::MAX;
              lwrUnit = Ecc::GetNextUnit(lwrUnit))
        {
            CHECK_RC(CheckEccErr(lwrUnit, errType));
        }
    }
    return rc;
}

namespace
{
    const string s_FbHeader  = "    Part   Subpartition             SBE                    DBE";
    const string s_LtcHeader = "    Part   Slice            Correctable          Uncorrectable";
    const string s_GpcHeader = "     GPC     TPC            Correctable          Uncorrectable";
    const string s_HsHubHeader = "   HSHUB   Block            Correctable          Uncorrectable";

    const map<Ecc::Unit, string> s_EclwnitHeaders =
    {
        { Ecc::Unit::LRF,             "Register File Error Counts       "  },
        { Ecc::Unit::CBU,             "CBU Error Counts                 "  },
        { Ecc::Unit::L1,              "L1 Cache Error Counts            "  },
        { Ecc::Unit::L1DATA,          "L1 Data Cache Error Counts       "  },
        { Ecc::Unit::L1TAG,           "L1 Tag Cache Error Counts        "  },
        { Ecc::Unit::SHM,             "SHM Cache Error Counts           "  },
        { Ecc::Unit::TEX,             "Texture Cache Retry Counts       "  },
        { Ecc::Unit::L2,              "L2 Cache Error Counts            "  },
        { Ecc::Unit::DRAM,            "FB Error Counts                  "  },
        { Ecc::Unit::SM_ICACHE,       "SM Instruction Cache Error Counts"  },
        { Ecc::Unit::GCC_L15,         "GCC L15 Error Counts             "  },
        { Ecc::Unit::GPCMMU,          "GPC MMU Error Counts             "  },
        { Ecc::Unit::HUBMMU_L2TLB,    "HUB MMU L2 TLB Error Counts      "  },
        { Ecc::Unit::HUBMMU_HUBTLB,   "HUB MMU HUB TLB Error Counts     "  },
        { Ecc::Unit::HUBMMU_FILLUNIT, "HUB MMU Fill Unit Error Counts   "  },
        { Ecc::Unit::GPCCS,           "GPCCS Error Counts               "  },
        { Ecc::Unit::FECS,            "FECS Error Counts                "  },
        { Ecc::Unit::PMU,             "PMU Error Counts                 "  },
        { Ecc::Unit::SM_RAMS,         "SM RAMS Error Counts             "  },
        { Ecc::Unit::HSHUB,           "HSHUB Error Counts               "  },
        { Ecc::Unit::PCIE_REORDER,    "PCIE Reorder Error Counts        "  },
        { Ecc::Unit::PCIE_P2PREQ,     "PCIE P2P Error Counts            "  }
    };
}

RC GpuSubdevice::GetAggregateEccInfo(Ecc::Unit eclwnit, Ecc::DetailedErrCounts * pEccData) const
{
    SubdeviceGraphics *pSubdeviceGraphics = GetSubdeviceGraphics();
    SubdeviceFb *pSubdeviceFb = GetSubdeviceFb();
    FalconEcc *pFalconEcc = GetFalconEcc();
    HsHubEcc *pHsHubEcc = GetHsHubEcc();
    PciBusEcc *pPciBusEcc = GetPciBusEcc();
    const Ecc::UnitType unitType = Ecc::GetUnitType(eclwnit);

    if ((((unitType == Ecc::UnitType::FB) || (unitType == Ecc::UnitType::L2)) &&
         (pSubdeviceFb == nullptr)) ||
        ((unitType == Ecc::UnitType::GPC) && (pSubdeviceGraphics == nullptr)) ||
        ((unitType == Ecc::UnitType::FALCON) && (pFalconEcc == nullptr)) ||
        ((unitType == Ecc::UnitType::PCIE) && (pPciBusEcc == nullptr)))
    {
        return RC::UNSUPPORTED_FUNCTION;
    }

    RC rc;
    switch (eclwnit)
    {
        case Ecc::Unit::LRF:
            return pSubdeviceGraphics->GetRfDetailedAggregateEccCounts(pEccData);
        case Ecc::Unit::CBU:
            return pSubdeviceGraphics->GetCbuDetailedAggregateEccCounts(pEccData);
        case Ecc::Unit::L1:
            return pSubdeviceGraphics->GetL1cDetailedAggregateEccCounts(pEccData);
        case Ecc::Unit::L1DATA:
            return pSubdeviceGraphics->GetL1DataDetailedAggregateEccCounts(pEccData);
        case Ecc::Unit::L1TAG:
            return pSubdeviceGraphics->GetL1TagDetailedAggregateEccCounts(pEccData);
        case Ecc::Unit::SHM:
            return pSubdeviceGraphics->GetSHMDetailedAggregateEccCounts(pEccData);
        case Ecc::Unit::TEX:
            {
                SubdeviceGraphics::GetTexDetailedEccCountsParams texParams;
                CHECK_RC(pSubdeviceGraphics->GetTexDetailedAggregateEccCounts(&texParams));
                pEccData->eccCounts.resize(texParams.eccCounts.size());
                for (size_t gpc = 0; gpc < texParams.eccCounts.size(); ++gpc)
                {
                    pEccData->eccCounts[gpc].resize(texParams.eccCounts[gpc].size());
                    for (size_t tpc = 0; tpc < texParams.eccCounts[gpc].size(); ++tpc)
                    {
                        Ecc::ErrCounts eccCounts = {};
                        for (size_t tex = 0; tex < texParams.eccCounts[gpc][tpc].size(); ++tex)
                        {
                            eccCounts += texParams.eccCounts[gpc][tpc][tex];
                        }

                        pEccData->eccCounts[gpc][tpc] = eccCounts;
                    }
                }
            }
            break;
        case Ecc::Unit::L2:
            return pSubdeviceFb->GetLtcDetailedAggregateEccCounts(pEccData);
        case Ecc::Unit::DRAM:
            return pSubdeviceFb->GetDramDetailedAggregateEccCounts(pEccData);
        case Ecc::Unit::SM_ICACHE:
            return pSubdeviceGraphics->GetSmIcacheDetailedAggregateEccCounts(pEccData);
        case Ecc::Unit::GCC_L15:
            return pSubdeviceGraphics->GetGccL15DetailedAggregateEccCounts(pEccData);
        case Ecc::Unit::GPCMMU:
            return pSubdeviceGraphics->GetGpcMmuDetailedAggregateEccCounts(pEccData);
        case Ecc::Unit::HUBMMU_L2TLB:
            return pSubdeviceGraphics->GetHubMmuL2TlbDetailedAggregateEccCounts(pEccData);
        case Ecc::Unit::HUBMMU_HUBTLB:
            return pSubdeviceGraphics->GetHubMmuHubTlbDetailedAggregateEccCounts(pEccData);
        case Ecc::Unit::HUBMMU_FILLUNIT:
            return pSubdeviceGraphics->GetHubMmuFillUnitDetailedAggregateEccCounts(pEccData);
        case Ecc::Unit::GPCCS:
            return pSubdeviceGraphics->GetGpccsDetailedAggregateEccCounts(pEccData);
        case Ecc::Unit::FECS:
            return pSubdeviceGraphics->GetFecsDetailedAggregateEccCounts(pEccData);
        case Ecc::Unit::PMU:
            {
                Ecc::ErrCounts errCounts;
                pEccData->eccCounts.resize(1);
                pEccData->eccCounts[0].resize(1);
                rc = pFalconEcc->GetPmuDetailedAggregateEccCounts(&errCounts);
                pEccData->eccCounts[0][0] = errCounts;
            }
            break;
        case Ecc::Unit::SM_RAMS:
            return pSubdeviceGraphics->GetSmRamsDetailedAggregateEccCounts(pEccData);
        case Ecc::Unit::HSHUB:
            return pHsHubEcc->GetHsHubDetailedAggregateEccCounts(pEccData);
        case Ecc::Unit::PCIE_REORDER:
            {
                Ecc::ErrCounts errCounts;
                pEccData->eccCounts.resize(1);
                pEccData->eccCounts[0].resize(1);
                rc = pPciBusEcc->GetDetailedAggregateReorderEccCounts(&errCounts);
                pEccData->eccCounts[0][0] = errCounts;
            }
            break;
        case Ecc::Unit::PCIE_P2PREQ:
            {
                Ecc::ErrCounts errCounts;
                pEccData->eccCounts.resize(1);
                pEccData->eccCounts[0].resize(1);
                rc = pPciBusEcc->GetDetailedAggregateP2PEccCounts(&errCounts);
                pEccData->eccCounts[0][0] = errCounts;
            }
            break;
        default:
            MASSERT(!"Unknown ECC Unit");
            return RC::SOFTWARE_ERROR;
    }
    return rc;
}

RC GpuSubdevice::PrintAggregateEccInfo(Ecc::Unit eclwnit) const
{
    Ecc::DetailedErrCounts detailedCounts;

    auto unitHeader = s_EclwnitHeaders.find(eclwnit);
    if (unitHeader == s_EclwnitHeaders.end())
    {
        MASSERT(!"Unknown ECC unit");
        Printf(Tee::PriError, "%s : Unknown ECC unit %u\n",
               __FUNCTION__, static_cast<UINT32>(eclwnit));
        return RC::SOFTWARE_ERROR;
    }

    RC rc = GetAggregateEccInfo(eclwnit, &detailedCounts);
    if (rc != RC::OK)
    {
        if ((RC::LWRM_NOT_SUPPORTED == rc) ||
            (RC::UNSUPPORTED_FUNCTION == rc))
        {
            rc.Clear();
            Printf(Tee::PriNormal,
                   "  %s : Not Supported!\n", unitHeader->second.c_str());
        }
        return rc;
    }
    else
    {
        const Ecc::UnitType unitType = Ecc::GetUnitType(eclwnit);
        if (unitType == Ecc::UnitType::FALCON)
        {
            Printf(Tee::PriNormal, "  %s :\n", unitHeader->second.c_str());
            Printf(Tee::PriNormal, "    Correctable   : %20lld\n",
                   detailedCounts.eccCounts[0][0].corr);
            Printf(Tee::PriNormal, "    Uncorrectable : %20lld\n",
                   detailedCounts.eccCounts[0][0].uncorr);

        }
        else
        {
            const string * pHeader;
            if (eclwnit == Ecc::Unit::DRAM)
                pHeader = &s_FbHeader;
            else if (eclwnit == Ecc::Unit::L2)
                pHeader = &s_LtcHeader;
            else if (eclwnit == Ecc::Unit::HSHUB)
                pHeader = &s_HsHubHeader;
            else
            {
                MASSERT(unitType == Ecc::UnitType::GPC);
                pHeader = &s_GpcHeader;
            }

            Printf(Tee::PriNormal, "  %s :\n", unitHeader->second.c_str());
            Printf(Tee::PriNormal,
                   "%s\n"
                   "    ----------------------------------------------------------\n",
                   pHeader->c_str());
            for (UINT32 idx0 = 0; idx0 < detailedCounts.eccCounts.size(); idx0++)
            {
                for (uint32 idx1 = 0;
                     idx1 < detailedCounts.eccCounts[idx0].size(); idx1++)
                {
                    const string idx1str =
                        (eclwnit == Ecc::Unit::HSHUB) ? Ecc::HsHubSublocationString(idx1) :
                                                        Utility::StrPrintf("%u", idx1);
                    Printf(Tee::PriNormal,
                           "    %4d   %5s   %20lld   %20lld\n",
                           idx0, idx1str.c_str(),
                           detailedCounts.eccCounts[idx0][idx1].corr,
                           detailedCounts.eccCounts[idx0][idx1].uncorr);
                }
            }
        }

    }
    return rc;
}


RC GpuSubdevice::DumpInfoRom(JsonItem *pJsi) const
{
    SCOPED_DEV_INST(this);

    RC rc;
    LwRmPtr pLwRm;

    MASSERT(pJsi);

    pJsi->SetCategory(JsonItem::JSONLOG_INFOROM);
    pJsi->SetJsonLimitedAllowAllFields(true);

    rc = CheckInfoRom();
    if ((rc == RC::UNSUPPORTED_FUNCTION) ||
        (rc == RC::LWRM_NOT_SUPPORTED))
    {
        return OK;
    }

    UINT32 version, subVersion;
    CHECK_RC(GetInfoRomObjectVersion(LW2080_CTRL_GPU_INFOROM_OBJECT_NAME_OBD,
                                     &version,
                                     &subVersion));

    Printf(Tee::PriNormal, "InfoRom Data (GPU %d:%d): \n",
           GetParentDevice()->GetDeviceInst(),
           GetSubdeviceInst());

    if ((version != 0) || (subVersion != 0))
    {
        LW2080_CTRL_GPU_GET_OEM_BOARD_INFO_PARAMS odbParams = { 0 };
        CHECK_RC(pLwRm->ControlBySubdevice(this,
                                        LW2080_CTRL_CMD_GPU_GET_OEM_BOARD_INFO,
                                        &odbParams,
                                        sizeof(odbParams)));

        pJsi->SetTag("inforom");

        const string modsHeaderTag = "ModsHeader";

        string printStr = Utility::StrPrintf("%u.%u", version, subVersion);
        pJsi->SetField("object", printStr.c_str());
        Printf(Tee::PriNormal, "  Object           : OBD (v%s)\n", printStr.c_str());
        printStr = Utility::StrPrintf("Object:OBD (v%s)", printStr.c_str());
        Mle::Print(Mle::Entry::tagged_str)
            .tag(modsHeaderTag)
            .key("Object")
            .value(Utility::StrPrintf("OBD (v%s)", printStr.c_str()));

        printStr = Utility::StrPrintf("%04x/%02x/%02x",
               static_cast<unsigned>(odbParams.buildDate >> 16),
               static_cast<unsigned>((odbParams.buildDate >> 8) & 0xFF),
               static_cast<unsigned>(odbParams.buildDate  & 0xFF));
        pJsi->SetField("build_date", printStr.c_str());
        Printf(Tee::PriNormal, "  Build Date       : %s\n", printStr.c_str());
        Mle::Print(Mle::Entry::tagged_str)
            .tag(modsHeaderTag)
            .key("Build Date")
            .value(printStr);

        // These assign call can mess up strings when copied as-is (leaving extra
        // null characters); always use StrPrintf to copy it into another string
        printStr.assign((const char *)odbParams.marketingName,
                        LW2080_GPU_MAX_MARKETING_NAME_LENGTH);
        pJsi->SetField("marketing_name", printStr.c_str());
        Printf(Tee::PriNormal, "  Marketing Name   : %s\n", printStr.c_str());
        Mle::Print(Mle::Entry::tagged_str)
            .tag(modsHeaderTag)
            .key("Marketing Name")
            .value(printStr);

        printStr.assign((const char *)odbParams.serialNumber,
                        LW2080_GPU_MAX_SERIAL_NUMBER_LENGTH);
        pJsi->SetField("serial_number", printStr.c_str());
        Printf(Tee::PriNormal, "  Serial Number    : %s\n", printStr.c_str());
        Mle::Print(Mle::Entry::tagged_str)
            .tag(modsHeaderTag)
            .key("Serial Number")
            .value(printStr);

        if (isprint(odbParams.memoryManufacturer))
        {
            printStr.assign(1, odbParams.memoryManufacturer);
            pJsi->SetField("memory_manufacturer", printStr.c_str());
            Printf(Tee::PriNormal, "  Memory Man.      : %s\n",
                   printStr.c_str());
            Mle::Print(Mle::Entry::tagged_str)
                .tag(modsHeaderTag)
                .key("Memory Man.")
                .value(printStr);
        }

        printStr.assign((const char *)odbParams.memoryPartID,
                        LW2080_GPU_MAX_MEMORY_PART_ID_LENGTH);
        pJsi->SetField("memory_part_id", printStr.c_str());
        Printf(Tee::PriNormal, "  Memory Part ID   : %s\n", printStr.c_str());
        Mle::Print(Mle::Entry::tagged_str)
            .tag(modsHeaderTag)
            .key("Memory Part ID")
            .value(printStr);

        printStr.assign((const char *)odbParams.memoryDateCode,
                        LW2080_GPU_MAX_MEMORY_DATE_CODE_LENGTH);
        pJsi->SetField("memory_date_code", printStr.c_str());
        Printf(Tee::PriNormal, "  Memory Date Code : %s\n", printStr.c_str());
        Mle::Print(Mle::Entry::tagged_str)
            .tag(modsHeaderTag)
            .key("Memory Date Code")
            .value(printStr);

        printStr.assign((const char *)odbParams.productPartNumber,
                        LW2080_GPU_MAX_PRODUCT_PART_NUMBER_LENGTH);
        pJsi->SetField("product_part_num", printStr.c_str());
        Printf(Tee::PriNormal, "  Product Part Num : %s\n", printStr.c_str());
        Mle::Print(Mle::Entry::tagged_str)
            .tag(modsHeaderTag)
            .key("Product Part Num")
            .value(printStr);

        printStr.assign((const char *)odbParams.board699PartNumber,
                        LW2080_GPU_MAX_PRODUCT_PART_NUMBER_LENGTH);
        pJsi->SetField("699_prod_part_num", printStr.c_str());
        Printf(Tee::PriNormal, " 699 Prod Part Num : %s\n", printStr.c_str());
        Mle::Print(Mle::Entry::tagged_str)
            .tag(modsHeaderTag)
            .key("699 Prod Part Num")
            .value(printStr);

        pJsi->WriteToLogfile();
    }
    else
    {
        Printf(Tee::PriNormal,
               "  Object           : OBD - Not found!\n");
    }

    CHECK_RC(GetInfoRomObjectVersion(LW2080_CTRL_GPU_INFOROM_OBJECT_NAME_PWR,
                                     &version,
                                     &subVersion));
    if ((version != 0) || (subVersion != 0))
    {
        Printf(Tee::PriNormal, "\n  Object           : PWR (v%d.%d)\n",
               version,
               subVersion);

        LW2080_CTRL_PMGR_PWR_MONITOR_GET_INFO_PARAMS monParams = { 0 };
        CHECK_RC(pLwRm->ControlBySubdevice(this,
                                     LW2080_CTRL_CMD_PMGR_PWR_MONITOR_GET_INFO,
                                     &monParams,
                                     sizeof(monParams)));
        Printf(Tee::PriNormal, "  Power Monitor    : %s\n",
               monParams.bSupported ? "Enabled" : "Disabled");
        if (monParams.bSupported)
        {
            Printf(Tee::PriNormal, "    Sample Rate (ms) : %d\n",
                   monParams.samplingPeriodms);
            Printf(Tee::PriNormal, "    Sample Count     : %d\n",
                   monParams.sampleCount);
            if (monParams.channelMask)
            {
                Printf(Tee::PriNormal,
                       "    Channel   Sensor Mask   Pwr Offset (mw)   "
                       "Pwr Limit (mw)\n");
                Printf(Tee::PriNormal,
                       "    ------------------------------------------"
                       "--------------\n");
                for (UINT32 i = 0;
                      i < LW2080_CTRL_PMGR_PWR_CHANNEL_MAX_CHANNELS; i++)
                {
                    if (!((1 << i) & monParams.channelMask))
                        continue;

                    //
                    // INFOROM support of power capping is tied to
                    // PWR_CHANNEL_TYPE_1X which is deprecated after Fermi,
                    // so returning N/A.
                    //
                    Printf(Tee::PriNormal,
                           "       N/A        N/A           N/A         N/A\n");
                }

            }
        }

        string capAlg = "Disabled";
        Printf(Tee::PriNormal, "  Power Capping    : %s\n", capAlg.c_str());
    }
    else
    {
        Printf(Tee::PriNormal,
               "\n  Object           : PWR - Not found!\n");
    }

    CHECK_RC(GetInfoRomObjectVersion(LW2080_CTRL_GPU_INFOROM_OBJECT_NAME_ECC,
                                     &version,
                                     &subVersion));
    if ((version != 0) || (subVersion != 0))
    {
        Printf(Tee::PriNormal, "\n  Object           : ECC (v%d.%d)\n",
               version,
               subVersion);

        // if we have entered here, we must support atleast one
        MASSERT (GetSubdeviceGraphics() || GetSubdeviceFb() || GetFalconEcc() || GetPciBusEcc());

        for (Ecc::Unit lwrUnit = Ecc::Unit::FIRST; lwrUnit < Ecc::Unit::MAX;
              lwrUnit = Ecc::GetNextUnit(lwrUnit))
        {
            CHECK_RC(PrintAggregateEccInfo(lwrUnit));
        }
    }
    else
    {
        Printf(Tee::PriNormal,
               "\n  Object           : ECC - Not found!\n");
    }

    rc = DumpEccAddress();
    if (rc != OK)
    {
        if (RC::LWRM_NOT_SUPPORTED == rc ||
            RC::UNSUPPORTED_FUNCTION == rc)
        {
            rc.Clear();
            Printf(Tee::PriNormal,
                   "  ECC Address Information     : Not Supported!\n");
        }
        else
            return rc;
    }

    CHECK_RC(GetInfoRomObjectVersion(LW2080_CTRL_GPU_INFOROM_OBJECT_NAME_LWL,
                                     &version,
                                     &subVersion));
    if ((version != 0) || (subVersion != 0))
    {
        Printf(Tee::PriNormal, "\n  Object           : LWL (v%d.%d)\n",
               version,
               subVersion);
        CHECK_RC(DumpLWLInfoRomData(Tee::PriNormal));
    }
    else
    {
        Printf(Tee::PriNormal,
               "\n  Object           : LWL - Not found!\n");
    }

    CHECK_RC(GetInfoRomObjectVersion(LW2080_CTRL_GPU_INFOROM_OBJECT_NAME_PBL,
                                     &version,
                                     &subVersion));
    if ((version != 0) || (subVersion != 0))
    {
        Printf(Tee::PriNormal, "\n  Object           : PBL (v%d.%d)\n",
               version,
               subVersion);
        Blacklist blacklist;
        CHECK_RC(blacklist.Load(this));
        CHECK_RC(blacklist.Print(Tee::PriNormal));
    }
    else
    {
        Printf(Tee::PriNormal,
               "\n  Object           : PBL - Not found!\n");
    }

    CHECK_RC(GetInfoRomObjectVersion("RRL",
                                     &version,
                                     &subVersion));
    if ((version != 0) || (subVersion != 0))
    {
        Printf(Tee::PriNormal, "\n  Object           : RRL (v%d.%d)\n",
               version,
               subVersion);
        RowRemapper remapper(this, LwRmPtr().Get());
        remapper.PrintInfoRomRemappedRows("  ");
    }

    UINT32 mode;
    CHECK_RC(GetGOMMode(&mode));

    switch (mode)
    {
        case LW0080_CTRL_GPU_SET_OPERATION_MODE_NEW_OPERATION_MODE_CONFIGURATION_A:
            Printf(Tee::PriNormal, "GPU Operation Mode : All on\n");
            break;
        case LW0080_CTRL_GPU_SET_OPERATION_MODE_NEW_OPERATION_MODE_CONFIGURATION_B:
            Printf(Tee::PriNormal, "GPU Operation Mode : Low tex\n");
            break;
        case LW0080_CTRL_GPU_SET_OPERATION_MODE_NEW_OPERATION_MODE_CONFIGURATION_C:
            Printf(Tee::PriNormal, "GPU Operation Mode : Low tex and low DP\n");
            break;
        case LW0080_CTRL_GPU_SET_OPERATION_MODE_NEW_OPERATION_MODE_CONFIGURATION_D:
            Printf(Tee::PriNormal, "GPU Operation Mode : Compute\n");
            break;
        case LW0080_CTRL_GPU_SET_OPERATION_MODE_NEW_OPERATION_MODE_CONFIGURATION_E:
            Printf(Tee::PriNormal, "GPU Operation Mode : Low DP\n");
            break;
        default:
            Printf(Tee::PriNormal, "GPU Operation Mode : N/A\n");
            break;
    }

    rc = DumpBBXInfoRomData(Tee::PriNormal);
    if (rc != OK)
    {
        if (RC::LWRM_NOT_SUPPORTED == rc ||
            RC::UNSUPPORTED_FUNCTION == rc)
        {
            rc.Clear();
            Printf(Tee::PriNormal,
                   "  BBX Data                    : Not Supported!\n");
        }
        else
            return rc;
    }

    return rc;
}

RC GpuSubdevice::DumpEccAddress() const
{
    RC rc;
    LwRmPtr pLwRm;
    LwRm::Handle clsA0E1Handle;
    const FrameBuffer *pFb = GetFB();
    const UINT32 columnDivisor = pFb->GetBurstSize() / pFb->GetAmapColumnSize();
    const UINT32 columnMultiplier = pFb->GetPseudoChannelsPerChannel();
    Tee::Priority pri = Tee::PriNormal;

    CHECK_RC(LwRmPtr()->Alloc(LwRmPtr()->GetSubdeviceHandle(this),
                &clsA0E1Handle,
                GK110_SUBDEVICE_FB,
                nullptr));

    DEFER
    {
        if (clsA0E1Handle)
        {
            pLwRm->Free(clsA0E1Handle);
        }
    };

    LWA0E1_CTRL_FB_GET_AGGREGATE_DRAM_ECC_ADDRESSES_PARAMS eccParams = {0};

    CHECK_RC(pLwRm->Control(clsA0E1Handle,
                LWA0E1_CTRL_CMD_FB_GET_AGGREGATE_DRAM_ECC_ADDRESSES,
                &eccParams,
                sizeof(eccParams)));

    if (eccParams.addressCount > LWA0E1_CTRL_FB_AGGREGATE_DRAM_ECC_ADDRESS_ENTRIES)
    {
        return RC::SOFTWARE_ERROR;
    }

    Printf(pri, "\nECC addresses: \n");
    if (eccParams.flags)
    {
        Printf(Tee::PriWarn, "Number of aggregate ECC addresses with error are more"
                " than maximum entries that can be returned\n");
    }

    Printf(pri, "Total count: %u\n", eccParams.addressCount);
    if (!eccParams.addressCount)
        return OK;

    Printf(pri,
            "    Type  Part  Subp      Col       Row      Bank    ExtBank\n"
            "    ---------------------------------------------------------\n");

    for (UINT32 idx = 0; idx < eccParams.addressCount; ++idx)
    {
        LWA0E1_CTRL_FB_AGGREGATE_DRAM_ECC_ADDRESS adr = eccParams.address[idx];
        string type;
        if (FLD_TEST_DRF(A0E1_CTRL_FB, _AGGREGATE_DRAM_ECC_ADDRESS_0, _TYPE, _NONE, adr.address0))
            type = "NONE";
        else if (FLD_TEST_DRF(A0E1_CTRL_FB, _AGGREGATE_DRAM_ECC_ADDRESS_0, _TYPE, _SBE, adr.address0))
            type = "SBE";
        else if (FLD_TEST_DRF(A0E1_CTRL_FB, _AGGREGATE_DRAM_ECC_ADDRESS_0, _TYPE, _DBE, adr.address0))
            type = "DBE";
        Printf(pri, "    %4s %4u %5u %#10x %#9x %#8x %6u\n",
                type.c_str(),
                DRF_VAL(A0E1_CTRL_FB, _AGGREGATE_DRAM_ECC_ADDRESS_0, _PARTITION, adr.address0),
                DRF_VAL(A0E1_CTRL_FB, _AGGREGATE_DRAM_ECC_ADDRESS_0, _SUBPARTITION, adr.address0),
                (DRF_VAL(A0E1_CTRL_FB, _AGGREGATE_DRAM_ECC_ADDRESS_0, _COLUMN,
                         adr.address0) / columnDivisor) * columnMultiplier,
                DRF_VAL(A0E1_CTRL_FB, _AGGREGATE_DRAM_ECC_ADDRESS_1, _ROW, adr.address1),
                DRF_VAL(A0E1_CTRL_FB, _AGGREGATE_DRAM_ECC_ADDRESS_1, _BANK, adr.address1),
                DRF_VAL(A0E1_CTRL_FB, _AGGREGATE_DRAM_ECC_ADDRESS_1, _EXTBANK, adr.address1));
    }

    Printf(pri,
            "    ---------------------------------------------------------\n");
    return rc;
}

RC GpuSubdevice::DumpLWLInfoRomData(Tee::Priority pri) const
{
    RC rc;

    auto pLwLink = GetInterface<LwLink>();

    // The inforom could have an lwlink block, but if MODS is running with kernel
    // mode RM, then pLwLink will be null.
    if (pLwLink == nullptr)
        return RC::OK;

    // Fatal Error Counts
    Printf(pri, "  LWLink Fatal Error Counts:\n");
    Printf(pri,
           "    Link ID         Error Type                        Count\n"
           "    -------------------------------------------------------\n");
    for (UINT32 linkId = 0; linkId < pLwLink->GetMaxLinks(); linkId ++)
    {
        if (!pLwLink->IsLinkValid(linkId))
            continue;

        LW2080_CTRL_LWLINK_GET_LINK_FATAL_ERROR_COUNTS_PARAMS lwlFatalErrorParams = { };
        lwlFatalErrorParams.linkId = linkId;

        rc = LwRmPtr()->ControlBySubdevice(this,
                 LW2080_CTRL_CMD_LWLINK_GET_LINK_FATAL_ERROR_COUNTS,
                 &lwlFatalErrorParams,
                 sizeof(lwlFatalErrorParams));

        if (rc == RC::LWRM_NOT_SUPPORTED)
        {
            Printf(pri, "    NOT SUPPORTED\n");
            rc.Clear();
            break;
        }
        CHECK_RC(rc);

        for (UINT32 errorType = 0; errorType < LW2080_CTRL_LWLINK_NUM_FATAL_ERROR_TYPES; errorType++)
        {
            if (!(LW2080_CTRL_LWLINK_IS_FATAL_ERROR_COUNT_VALID(errorType,
                                                                lwlFatalErrorParams.supportedCounts)))
            {
                continue;
            }

            UINT32 errCount = lwlFatalErrorParams.fatalErrorCounts[errorType];
            if (errCount == 0)
                continue;

            string errorTypeStr;
            switch (errorType)
            {
                case LW2080_CTRL_LWLINK_FATAL_ERROR_TYPE_TL_RX_DL_DATA_PARITY:
                    errorTypeStr = "Rx DL Data Parity";
                    break;
                case LW2080_CTRL_LWLINK_FATAL_ERROR_TYPE_TL_RX_DL_CTRL_PARITY:
                    errorTypeStr = "Rx DL Ctrl Parity";
                    break;
                case LW2080_CTRL_LWLINK_FATAL_ERROR_TYPE_TL_RX_PROTOCOL:
                    errorTypeStr = "Rx Protocol";
                    break;
                case LW2080_CTRL_LWLINK_FATAL_ERROR_TYPE_TL_RX_OVERFLOW:
                    errorTypeStr = "Rx Overflow";
                    break;
                case LW2080_CTRL_LWLINK_FATAL_ERROR_TYPE_TL_RX_RAM_DATA_PARITY:
                    errorTypeStr = "Rx RAM Data Parity";
                    break;
                case LW2080_CTRL_LWLINK_FATAL_ERROR_TYPE_TL_RX_RAM_HDR_PARITY:
                    errorTypeStr = "Rx RAM HDR Parity";
                    break;
                case LW2080_CTRL_LWLINK_FATAL_ERROR_TYPE_TL_RX_RESP:
                    errorTypeStr = "Rx Response";
                    break;
                case LW2080_CTRL_LWLINK_FATAL_ERROR_TYPE_TL_RX_POISON:
                    errorTypeStr = "Rx Poison";
                    break;
                case LW2080_CTRL_LWLINK_FATAL_ERROR_TYPE_TL_TX_RAM_DATA_PARITY:
                    errorTypeStr = "Tx RAM Data Parity";
                    break;
                case LW2080_CTRL_LWLINK_FATAL_ERROR_TYPE_TL_TX_RAM_HDR_PARITY:
                    errorTypeStr = "Tx RAM HDR Parity";
                    break;
                case LW2080_CTRL_LWLINK_FATAL_ERROR_TYPE_TL_TX_CREDIT:
                    errorTypeStr = "Tx Credit";
                    break;
                case LW2080_CTRL_LWLINK_FATAL_ERROR_TYPE_TL_DL_FLOW_CTRL_PARITY:
                    errorTypeStr = "DL Flow Ctrl Parity";
                    break;
                case LW2080_CTRL_LWLINK_FATAL_ERROR_TYPE_TL_DL_HDR_PARITY:
                    errorTypeStr = "DL HDR Parity";
                    break;
                case LW2080_CTRL_LWLINK_FATAL_ERROR_TYPE_DL_TX_RECOVERY_LONG:
                    errorTypeStr = "Tx Recovery Long";
                    break;
                case LW2080_CTRL_LWLINK_FATAL_ERROR_TYPE_DL_TX_FAULT_RAM:
                    errorTypeStr = "Tx Fault RAM";
                    break;
                case LW2080_CTRL_LWLINK_FATAL_ERROR_TYPE_DL_TX_FAULT_INTERFACE:
                    errorTypeStr = "Tx Fault Interface";
                    break;
                case LW2080_CTRL_LWLINK_FATAL_ERROR_TYPE_DL_TX_FAULT_SUBLINK_CHANGE:
                    errorTypeStr = "Tx Fault Sublink Change";
                    break;
                case LW2080_CTRL_LWLINK_FATAL_ERROR_TYPE_DL_RX_FAULT_SUBLINK_CHANGE:
                    errorTypeStr = "Rx Fault Sublink Change";
                    break;
                case LW2080_CTRL_LWLINK_FATAL_ERROR_TYPE_DL_RX_FAULT_DL_PROTOCOL:
                    errorTypeStr = "Rx Fault DL Protocol";
                    break;
                case LW2080_CTRL_LWLINK_FATAL_ERROR_TYPE_DL_LTSSM_FAULT:
                    errorTypeStr = "LTSSM Fault";
                    break;
                case LW2080_CTRL_LWLINK_FATAL_ERROR_TYPE_TLC_RX_DL_HDR_PARITY:
                    errorTypeStr = "Rx DL HDR Parity";
                    break;
                case LW2080_CTRL_LWLINK_FATAL_ERROR_TYPE_TLC_RX_ILWALID_AE_FLIT_RCVD:
                    errorTypeStr = "Rx Invalid AE Flit Received";
                    break;
                case LW2080_CTRL_LWLINK_FATAL_ERROR_TYPE_TLC_RX_ILWALID_BE_FLIT_RCVD:
                    errorTypeStr = "Rx Invalid BE Flit Received";
                    break;
                case LW2080_CTRL_LWLINK_FATAL_ERROR_TYPE_TLC_RX_ILWALID_ADDR_ALIGN:
                    errorTypeStr = "Rx Invalid Address Alignment";
                    break;
                case LW2080_CTRL_LWLINK_FATAL_ERROR_TYPE_TLC_RX_PKT_LEN:
                    errorTypeStr = "Rx Packet Length";
                    break;
                case LW2080_CTRL_LWLINK_FATAL_ERROR_TYPE_TLC_RX_RSVD_CMD_ENC:
                    errorTypeStr = "Rx RSVD Command Enc";
                    break;
                case LW2080_CTRL_LWLINK_FATAL_ERROR_TYPE_TLC_RX_RSVD_DAT_LEN_ENC:
                    errorTypeStr = "Rx RSVD Data Length Enc";
                    break;
                case LW2080_CTRL_LWLINK_FATAL_ERROR_TYPE_TLC_RX_RSVD_ADDR_TYPE:
                    errorTypeStr = "Rx RSVD Address Type";
                    break;
                case LW2080_CTRL_LWLINK_FATAL_ERROR_TYPE_TLC_RX_RSVD_RSP_STATUS:
                    errorTypeStr = "Rx RSVD RSP Status";
                    break;
                case LW2080_CTRL_LWLINK_FATAL_ERROR_TYPE_TLC_RX_RSVD_PKT_STATUS:
                    errorTypeStr = "Rx RSVD Packet Status";
                    break;
                case LW2080_CTRL_LWLINK_FATAL_ERROR_TYPE_TLC_RX_RSVD_CACHE_ATTR_ENC_IN_PROBE_REQ:
                    errorTypeStr = "Rx RSVD Cache Attribute Enc In Probe Request";
                    break;
                case LW2080_CTRL_LWLINK_FATAL_ERROR_TYPE_TLC_RX_RSVD_CACHE_ATTR_ENC_IN_PROBE_RESP:
                    errorTypeStr = "Rx RSVD Cache Attribute Enc in Probe Response";
                    break;
                case LW2080_CTRL_LWLINK_FATAL_ERROR_TYPE_TLC_RX_DAT_LEN_GT_ATOMIC_REQ_MAX_SIZE:
                    errorTypeStr = "Rx DAT Length GT Atomic Request Max Size";
                    break;
                case LW2080_CTRL_LWLINK_FATAL_ERROR_TYPE_TLC_RX_DAT_LEN_GT_RMW_REQ_MAX_SIZE:
                    errorTypeStr = "Rx DAT Length GT RMW Request Max Size";
                    break;
                case LW2080_CTRL_LWLINK_FATAL_ERROR_TYPE_TLC_RX_DAT_LEN_LT_ATR_RESP_MIN_SIZE:
                    errorTypeStr = "Rx DAT Length LT ATR Response Min Size";
                    break;
                case LW2080_CTRL_LWLINK_FATAL_ERROR_TYPE_TLC_RX_ILWALID_PO_FOR_CACHE_ATTR:
                    errorTypeStr = "Rx Invalid PO for Cache Attribute";
                    break;
                case LW2080_CTRL_LWLINK_FATAL_ERROR_TYPE_TLC_RX_ILWALID_COMPRESSED_RESP:
                    errorTypeStr = "Rx Invalid Compressed Response";
                    break;
                case LW2080_CTRL_LWLINK_FATAL_ERROR_TYPE_TLC_RX_RESP_STATUS_TARGET:
                    errorTypeStr = "Rx Response Status Target";
                    break;
                case LW2080_CTRL_LWLINK_FATAL_ERROR_TYPE_TLC_RX_RESP_STATUS_UNSUPPORTED_REQUEST:
                    errorTypeStr = "Rx Response Status Unsupported Request";
                    break;
                case LW2080_CTRL_LWLINK_FATAL_ERROR_TYPE_TLC_RX_HDR_OVERFLOW:
                    errorTypeStr = "Rx HDR Overflow";
                    break;
                case LW2080_CTRL_LWLINK_FATAL_ERROR_TYPE_TLC_RX_DATA_OVERFLOW:
                    errorTypeStr = "Rx Data Overflow";
                    break;
                case LW2080_CTRL_LWLINK_FATAL_ERROR_TYPE_TLC_RX_STOMPED_PKT_RCVD:
                    errorTypeStr = "Rx Stomped Packet Received";
                    break;
                case LW2080_CTRL_LWLINK_FATAL_ERROR_TYPE_TLC_RX_CORRECTABLE_INTERNAL:
                    errorTypeStr = "Rx Correctable Internal";
                    break;
                case LW2080_CTRL_LWLINK_FATAL_ERROR_TYPE_TLC_RX_UNSUPPORTED_VC_OVERFLOW:
                    errorTypeStr = "Rx Unsupported VC Overflow";
                    break;
                case LW2080_CTRL_LWLINK_FATAL_ERROR_TYPE_TLC_RX_UNSUPPORTED_LWLINK_CREDIT_RELEASE:
                    errorTypeStr = "Rx Unsupported LwLink Credit Release";
                    break;
                case LW2080_CTRL_LWLINK_FATAL_ERROR_TYPE_TLC_RX_UNSUPPORTED_NCISOC_CREDIT_RELEASE:
                    errorTypeStr = "Rx Unsupported NCISOC Credit Release";
                    break;
                case LW2080_CTRL_LWLINK_FATAL_ERROR_TYPE_TLC_TX_HDR_CREDIT_OVERFLOW:
                    errorTypeStr = "Tx HDR Credit Overflow";
                    break;
                case LW2080_CTRL_LWLINK_FATAL_ERROR_TYPE_TLC_TX_DATA_CREDIT_OVERFLOW:
                    errorTypeStr = "Tx Data Credit Overflow";
                    break;
                case LW2080_CTRL_LWLINK_FATAL_ERROR_TYPE_TLC_TX_DL_REPLAY_CREDIT_OVERFLOW:
                    errorTypeStr = "Tx DL Replay Credit Overflow";
                    break;
                case LW2080_CTRL_LWLINK_FATAL_ERROR_TYPE_TLC_TX_UNSUPPORTED_VC_OVERFLOW:
                    errorTypeStr = "Tx Unsupported VC Overflow";
                    break;
                case LW2080_CTRL_LWLINK_FATAL_ERROR_TYPE_TLC_TX_STOMPED_PKT_SENT:
                    errorTypeStr = "Tx Stomped Packet Sent";
                    break;
                case LW2080_CTRL_LWLINK_FATAL_ERROR_TYPE_TLC_TX_DATA_POISONED_PKT_SENT:
                    errorTypeStr = "Tx Data Poisoned Packet Sent";
                    break;
                case LW2080_CTRL_LWLINK_FATAL_ERROR_TYPE_TLC_TX_RESP_STATUS_TARGET:
                    errorTypeStr = "Tx Response Status Target";
                    break;
                case LW2080_CTRL_LWLINK_FATAL_ERROR_TYPE_TLC_TX_RESP_STATUS_UNSUPPORTED_REQUEST:
                    errorTypeStr = "Tx Response Status Unsupported Request";
                    break;
                case LW2080_CTRL_LWLINK_FATAL_ERROR_TYPE_MIF_RX_RAM_DATA_PARITY:
                    errorTypeStr = "Mif Rx RAM Data Parity";
                    break;
                case LW2080_CTRL_LWLINK_FATAL_ERROR_TYPE_MIF_RX_RAM_HDR_PARITY:
                    errorTypeStr = "Mif Rx RAM HDR Parity";
                    break;
                case LW2080_CTRL_LWLINK_FATAL_ERROR_TYPE_MIF_TX_RAM_DATA_PARITY:
                    errorTypeStr = "Mif Tx RAM Data Parity";
                    break;
                case LW2080_CTRL_LWLINK_FATAL_ERROR_TYPE_MIF_TX_RAM_HDR_PARITY:
                    errorTypeStr = "Mif Tx RAM HDR Parity";
                    break;
                default:
                    errorTypeStr = Utility::StrPrintf("%u", errorType);
                    Printf(Tee::PriWarn, "DumpLWLInfoRomData: fatal error type unknown\n");
                    break;
            }

            Printf(pri,
                   "    %2u              %-25s    %10u\n",
                   linkId,
                   errorTypeStr.c_str(),
                   errCount);
        }
    }

    // Nonfatal Error Counts
    Printf(pri, "  LWLink Nonfatal Error Rates (Past Days):\n");
    Printf(pri,
           "    Link ID        Day   Max Errors Per Minute    Timestamp\n"
           "    -------------------------------------------------------\n");
    for (UINT32 linkId = 0; linkId < pLwLink->GetMaxLinks(); linkId ++)
    {
        if (!pLwLink->IsLinkValid(linkId))
            continue;

        LW2080_CTRL_LWLINK_GET_LINK_NONFATAL_ERROR_RATES_PARAMS lwlNonfatalErrorParams = {};
        lwlNonfatalErrorParams.linkId = linkId;

        rc = LwRmPtr()->ControlBySubdevice(this,
                 LW2080_CTRL_CMD_LWLINK_GET_LINK_NONFATAL_ERROR_RATES,
                 &lwlNonfatalErrorParams,
                 sizeof(lwlNonfatalErrorParams));

        if (rc == RC::LWRM_NOT_SUPPORTED)
        {
            Printf(pri, "    NOT SUPPORTED\n");
            rc.Clear();
            break;
        }
        CHECK_RC(rc);

        UINT32 numDays = lwlNonfatalErrorParams.numDailyMaxNonfatalErrorRates;
        for (UINT32 i = 0; i < numDays; i++)
        {
            Printf(pri,
                   "    %2u      %10u              %10u   %10u\n",
                   linkId,
                   i,
                   lwlNonfatalErrorParams.dailyMaxNonfatalErrorRates[i].errorsPerMinute,
                   lwlNonfatalErrorParams.dailyMaxNonfatalErrorRates[i].timestamp);
        }
    }
    Printf(pri, "  LWLink Nonfatal Error Rates (Past Months):\n");
    Printf(pri,
           "    Link ID      Month   Max Errors Per Minute    Timestamp\n"
           "    -------------------------------------------------------\n");
    for (UINT32 linkId = 0; linkId < pLwLink->GetMaxLinks(); linkId ++)
    {
        if (!pLwLink->IsLinkValid(linkId))
            continue;

        LW2080_CTRL_LWLINK_GET_LINK_NONFATAL_ERROR_RATES_PARAMS lwlNonfatalErrorParams = {};
        lwlNonfatalErrorParams.linkId = linkId;

        rc = LwRmPtr()->ControlBySubdevice(this,
                 LW2080_CTRL_CMD_LWLINK_GET_LINK_NONFATAL_ERROR_RATES,
                 &lwlNonfatalErrorParams,
                 sizeof(lwlNonfatalErrorParams));

        if (rc == RC::LWRM_NOT_SUPPORTED)
        {
            Printf(pri, "    NOT SUPPORTED\n");
            rc.Clear();
            break;
        }
        CHECK_RC(rc);

        UINT32 numMonths = lwlNonfatalErrorParams.numDailyMaxNonfatalErrorRates;
        for (UINT32 i = 0; i < numMonths; i++)
        {
            Printf(pri,
                   "    %2d      %10d              %10d   %10d\n",
                   linkId,
                   i,
                   lwlNonfatalErrorParams.monthlyMaxNonfatalErrorRates[i].errorsPerMinute,
                   lwlNonfatalErrorParams.monthlyMaxNonfatalErrorRates[i].timestamp);
        }
    }

    // Last Error Remote Type
    Printf(pri, "  LWLink Last Error Remote Type:\n");
    Printf(pri,
           "    Link ID                                     Remote Type\n"
           "    -------------------------------------------------------\n");
    for (UINT32 linkId = 0; linkId < pLwLink->GetMaxLinks(); linkId ++)
    {
        if (!pLwLink->IsLinkValid(linkId))
            continue;

        LW2080_CTRL_LWLINK_GET_LINK_LAST_ERROR_REMOTE_TYPE_PARAMS lwlLastErrorRemoteTypeParams = {};
        lwlLastErrorRemoteTypeParams.linkId = linkId;

        rc = LwRmPtr()->ControlBySubdevice(this,
                LW2080_CTRL_CMD_LWLINK_GET_LINK_LAST_ERROR_REMOTE_TYPE,
                &lwlLastErrorRemoteTypeParams,
                sizeof(lwlLastErrorRemoteTypeParams));

        if (rc == RC::LWRM_NOT_SUPPORTED)
        {
            Printf(pri, "    NOT SUPPORTED\n");
            rc.Clear();
            break;
        }
        CHECK_RC(rc);

        string remoteTypeStr;
        switch (lwlLastErrorRemoteTypeParams.remoteType)
        {
            case LW2080_CTRL_LWLINK_DEVICE_INFO_DEVICE_TYPE_EBRIDGE:
                remoteTypeStr = "EBridge";
                break;
            case LW2080_CTRL_LWLINK_DEVICE_INFO_DEVICE_TYPE_NPU:
                remoteTypeStr = "NPU";
                break;
            case LW2080_CTRL_LWLINK_DEVICE_INFO_DEVICE_TYPE_GPU:
                remoteTypeStr = "GPU";
                break;
            case LW2080_CTRL_LWLINK_DEVICE_INFO_DEVICE_TYPE_SWITCH:
                remoteTypeStr = "LwSwitch";
                break;
            case LW2080_CTRL_LWLINK_DEVICE_INFO_DEVICE_TYPE_NONE:
                remoteTypeStr = "None";
                break;
            default:
                remoteTypeStr = Utility::StrPrintf("%u", lwlLastErrorRemoteTypeParams.remoteType);
                Printf(Tee::PriWarn, "DumpLWLInfoRomData: error remote type unknown\n");
                break;
        }
        Printf(pri,
               "    %2u                                 %20s\n",
               linkId,
               remoteTypeStr.c_str());
    }
    return rc;
}

RC GpuSubdevice::DumpBBXInfoRomData(Tee::Priority pri) const
{
    //Print BBX Inforom Data
    RC rc;
    LwRmPtr pLwRm;
    LwRm::Handle cls90E7Handle;

    rc = LwRmPtr()->Alloc(LwRmPtr()->GetSubdeviceHandle(this),
            &cls90E7Handle,
            GF100_SUBDEVICE_INFOROM,
            nullptr);

    if (!cls90E7Handle)
    {
        return RC::LWRM_ILWALID_OBJECT_ERROR;
    }

    DEFER
    {
        if (cls90E7Handle)
        {
            pLwRm->Free(cls90E7Handle);
        }
    };

    LW90E7_CTRL_BBX_GET_FIELD_DIAG_DATA_PARAMS bbxFddParams = {0};
    rc = pLwRm->Control(cls90E7Handle,
                LW90E7_CTRL_CMD_BBX_GET_FIELD_DIAG_DATA,
                &bbxFddParams,
                sizeof(bbxFddParams));

    if (rc == RC::UNSUPPORTED_FUNCTION || rc == RC::LWRM_NOT_SUPPORTED)
    {
        Printf(pri, "        **************************************************\n"
                    "                 Field diagnostic data\n"
                    "        **************************************************\n"
                    "        Not Supported\n");
        rc.Clear();
    }
    else
    {
        CHECK_RC(rc);
        Printf(pri, "\nInfoRom BBX Data (GPU %d:%d): \n",
                GetParentDevice()->GetDeviceInst(),
                GetSubdeviceInst());

#define GmTime(x) Utility::ColwertEpochToGMT(x).c_str()

        Printf(pri, "        **************************************************\n"
                    "                 Field diagnostic data\n"
                    "        **************************************************\n");
        Printf(pri, "        Fieldiag result       : %llu\n",
               bbxFddParams.fieldDiagResult);
        Printf(pri, "        Fieldiag version      : %u\n",
               static_cast<unsigned>(bbxFddParams.fieldDiagVersion));
        Printf(pri, "        Fieldiag timestamp    : %s\n",
               GmTime(bbxFddParams.fieldDiagTimestamp));
    }

    LW90E7_CTRL_BBX_GET_TIME_DATA_PARAMS bbxTdParams = {0};
    rc = pLwRm->Control(cls90E7Handle,
                LW90E7_CTRL_CMD_BBX_GET_TIME_DATA,
                &bbxTdParams,
                sizeof(bbxTdParams));
    if (rc == RC::UNSUPPORTED_FUNCTION || rc == RC::LWRM_NOT_SUPPORTED)
    {
        Printf(pri, "\n        **************************************************\n"
                    "                 BBX recorded timing data (in EPOCH)\n"
                    "        **************************************************\n"
                    "        Not Supported\n");
        rc.Clear();
    }
    else
    {
        CHECK_RC(rc);
        Printf(pri, "\n        **************************************************\n"
                    "               BBX recorded timing data (in EPOCH)\n"
                    "        **************************************************\n");
        Printf(pri, "        First Time BBX was updated             : %s\n",
                    GmTime(bbxTdParams.timeStart));
        Printf(pri, "        Last Time BBX was updated              : %s\n",
                    GmTime(bbxTdParams.timeEnd));
        Printf(pri, "        Total time GPU was running(in sec)     : %u\n",
               static_cast<unsigned>(bbxTdParams.timeRun));

        if (bbxTdParams.time24Hours != 0)
        {
            Printf(pri, "        Time when 24 hours of runtime reached  : %s\n",
                        GmTime(bbxTdParams.time24Hours));
        }
        if (bbxTdParams.time100Hours != 0)
        {
            Printf(pri, "        Time when 100 hours of runtime reached : %s\n",
                        GmTime(bbxTdParams.time100Hours));
        }
    }

    LW90E7_CTRL_BBX_GET_PWR_DATA_PARAMS bbxPdParams = {0};
    rc = pLwRm->Control(cls90E7Handle,
                LW90E7_CTRL_CMD_BBX_GET_PWR_DATA,
                &bbxPdParams,
                sizeof(bbxPdParams));
    if (rc == RC::UNSUPPORTED_FUNCTION || rc == RC::LWRM_NOT_SUPPORTED)
    {
        Printf(pri, "\n        **************************************************\n"
                    "                 BBX recorded power data\n"
                    "        **************************************************\n"
                    "        Not Supported\n");
        rc.Clear();
    }
    else
    {
        CHECK_RC(rc);
        Printf(pri, "\n        **************************************************\n"
                    "                 BBX recorded power data\n"
                    "        **************************************************\n");
        Printf(pri, "        Number of times PCIe external power has been disconnected: %u\n",
                    static_cast<unsigned>(bbxPdParams.pwrExternalDisconnectCount));
        Printf(pri, "        Number of times power is drawn over the then power limit : %u\n",
                    static_cast<unsigned>(bbxPdParams.pwrLimitCrossCount));
        if (bbxPdParams.pwrMaxDayCount)
        {
            Printf(pri, "        Maximum power drawn per day, for last %u days:\n",
                        static_cast<unsigned>(bbxPdParams.pwrMaxDayCount));
            Printf(pri, "        ------------------------------------------------------------------\n");
            Printf(pri, "        Power drawn(mW)       Power limit(mW)       Timestamp(EPOCH)\n");
            Printf(pri, "        ------------------------------------------------------------------\n");
            MASSERT(bbxPdParams.pwrMaxDayCount <= LW90E7_CTRL_BBX_PWR_ENTRIES);
            for (UINT32 idx = 0; idx < bbxPdParams.pwrMaxDayCount; idx++)
            {
                LW90E7_CTRL_BBX_PWR_ENTRY bbxPwrEntry = bbxPdParams.pwrMaxDay[idx];
                Printf(pri, "%18u%22u%33s\n",
                       static_cast<unsigned>(bbxPwrEntry.value),
                       static_cast<unsigned>(bbxPwrEntry.limit),
                       GmTime(bbxPwrEntry.timestamp));
            }
            Printf(pri, "        ------------------------------------------------------------------\n\n");
        }

        if (bbxPdParams.pwrMaxMonthCount)
        {
            Printf(pri, "        Maximum power drawn per month, for last %u months:\n",
                        static_cast<unsigned>(bbxPdParams.pwrMaxMonthCount));
            Printf(pri, "        ------------------------------------------------------------------\n");
            Printf(pri, "        Power drawn(mW)       Power limit(mW)       Timestamp(EPOCH)\n");
            Printf(pri, "        ------------------------------------------------------------------\n");
            MASSERT(bbxPdParams.pwrMaxMonthCount <= LW90E7_CTRL_BBX_PWR_ENTRIES);
            for (UINT32 idx = 0; idx < bbxPdParams.pwrMaxMonthCount; idx++)
            {
                LW90E7_CTRL_BBX_PWR_ENTRY bbxPwrEntry = bbxPdParams.pwrMaxMonth[idx];
                Printf(pri, "%18u%22u%33s\n",
                       static_cast<unsigned>(bbxPwrEntry.value),
                       static_cast<unsigned>(bbxPwrEntry.limit),
                       GmTime(bbxPwrEntry.timestamp));
            }
            Printf(pri, "        ------------------------------------------------------------------\n\n");
        }

        if (bbxPdParams.pwrHistogramCount)
        {
            MASSERT(bbxPdParams.pwrHistogramCount <= LW90E7_CTRL_BBX_PWR_HISTOGRAM_ENTRIES);
            Printf(pri, "\n        Power consumption histogram for time spent in various ranges\n");
            Printf(pri, "        ----------------------------------------------------------------\n");
            Printf(pri, "          Power consumption range(mW)             frequency\n");
            Printf(pri, "        ----------------------------------------------------------------\n");
            for (UINT32 idx = 0; idx < bbxPdParams.pwrHistogramCount; idx++)
            {
                LW90E7_CTRL_BBX_PWR_HISTOGRAM bbxPwrHist = bbxPdParams.pwrHistogram[idx];
                Printf(pri, "%18u   - %8u%26u\n",
                       static_cast<unsigned>(bbxPwrHist.min),
                       static_cast<unsigned>(bbxPwrHist.max),
                       static_cast<unsigned>(bbxPwrHist.frequency));
            }
            Printf(pri, "        ----------------------------------------------------------------\n\n");
        }

        Printf(pri, "\n        Moving average of power consumption per hour, for last %u hours\n",
                    LW90E7_CTRL_BBX_PWR_AVERAGE_HOUR_ENTRIES);
        Printf(pri, "        --------------------------------\n");
        Printf(pri, "        Hour        Power Consumption(W)\n");
        Printf(pri, "        --------------------------------\n");
        for (UINT32 idx = 0; idx < LW90E7_CTRL_BBX_PWR_AVERAGE_HOUR_ENTRIES; idx++)
        {
            Printf(pri, "%11u%22u\n", idx + 1,
                   static_cast<unsigned>(bbxPdParams.pwrAverageHour[idx]));
        }
        Printf(pri, "        ---------------------------------\n\n");

        Printf(pri, "        Moving average of power consumption per day, for last %u days\n",
                    LW90E7_CTRL_BBX_PWR_AVERAGE_DAY_ENTRIES);
        Printf(pri, "        --------------------------------\n");
        Printf(pri, "        Day        Power Consumption(W)\n");
        Printf(pri, "        --------------------------------\n");
        for (UINT32 idx = 0; idx < LW90E7_CTRL_BBX_PWR_AVERAGE_DAY_ENTRIES; idx++)
        {
            Printf(pri, "%10u%22u\n", idx + 1,
                   static_cast<unsigned>(bbxPdParams.pwrAverageDay[idx]));
        }
        Printf(pri, "        ---------------------------------\n\n");

        Printf(pri, "        Moving average of power consumption per month, for last %u months\n",
                    LW90E7_CTRL_BBX_PWR_AVERAGE_MONTH_ENTRIES);
        Printf(pri, "        ---------------------------------\n");
        Printf(pri, "        Month        Power Consumption(W)\n");
        Printf(pri, "        ---------------------------------\n");
        for (UINT32 idx = 0; idx < LW90E7_CTRL_BBX_PWR_AVERAGE_MONTH_ENTRIES; idx++)
        {
            Printf(pri, "%10u%24u\n", idx + 1,
                   static_cast<unsigned>(bbxPdParams.pwrAverageMonth[idx]));
        }
        Printf(pri, "        ---------------------------------\n\n");
    }

    LW90E7_CTRL_BBX_GET_PWR_SAMPLES_PARAMS bbxPwrSampParams = {0};
    rc = pLwRm->Control(cls90E7Handle,
                LW90E7_CTRL_CMD_BBX_GET_PWR_SAMPLES,
                &bbxPwrSampParams,
                sizeof(bbxPwrSampParams));

    if (rc == RC::UNSUPPORTED_FUNCTION || rc == RC::LWRM_NOT_SUPPORTED)
    {
        Printf(pri, "\n        **************************************************\n"
                    "                 Power Samples\n"
                    "        **************************************************\n"
                    "        Not Supported\n");
        rc.Clear();
    }
    else
    {
        CHECK_RC(rc);
        Printf(pri, "\n        **************************************************\n"
                    "                 Power Samples\n"
                    "        **************************************************\n");

        Printf(pri, "        Power sample interval (in sec)     : %u\n",
               static_cast<unsigned>(bbxPwrSampParams.pwrSampleInterval));
        Printf(pri, "        -----------------------------------------------------------------\n");
        Printf(pri,
               "        Power sample reading (in W) in row major order, sample count: %u\n",
               static_cast<unsigned>(bbxPwrSampParams.pwrSampleCount));
        Printf(pri, "        -----------------------------------------------------------------");
        for (UINT32 idx = 0; idx < bbxPwrSampParams.pwrSampleCount; idx++)
        {
            if (idx % 6 == 0)
                Printf(pri, "\n");
            Printf(pri, "%10u ",
                   static_cast<unsigned>(bbxPwrSampParams.pwrSample[idx]));
        }
        Printf(pri, "\n        -----------------------------------------------------------------\n");
    }

    LW90E7_CTRL_BBX_GET_TEMP_DATA_PARAMS bbxTempDataParams = {0};
    rc = pLwRm->Control(cls90E7Handle,
                LW90E7_CTRL_CMD_BBX_GET_TEMP_DATA,
                &bbxTempDataParams,
                sizeof(bbxTempDataParams));

    if (rc == RC::UNSUPPORTED_FUNCTION || rc == RC::LWRM_NOT_SUPPORTED)
    {
        Printf(pri, "        **************************************************\n"
                    "                 BBX recorded temperature data\n"
                    "        **************************************************\n"
                    "        Not Supported\n");
        rc.Clear();
    }
    else
    {
        CHECK_RC(rc);
        Printf(pri, "        **************************************************\n"
                    "                 BBX recorded temperature data\n"
                    "        **************************************************\n");
        Printf(pri, "        Total sum of GPU temperature change in its lifetime in "
                    "0.1C granularity: %u.%uC\n\n",
                    static_cast<unsigned>(bbxTempDataParams.tempSumDelta / 10),
                    static_cast<unsigned>(bbxTempDataParams.tempSumDelta % 10));

        LW90E7_CTRL_BBX_TEMP_HISTOGRAM bbxTempHist =
            bbxTempDataParams.tempHistogramThreshold;
        if (bbxTempHist.count)
        {
            Printf(pri, "        Histogram of GPU temperature crossing various thresolds\n"
                        "        Step change : %d (in Celsius), temperature threshold\n"
                        "        frequency : Number of times temperature crossed it\n",
                        bbxTempHist.step);
            MASSERT(bbxTempHist.count <= LW90E7_CTRL_BBX_TEMP_HISTOGRAM_ENTRIES);
            Printf(pri, "        -------------------------------\n");
            Printf(pri, "           Temp            Frequency\n");
            Printf(pri, "        -------------------------------\n");
            UINT32 freq = 0;
            for (UINT32 idx = 0; idx < bbxTempHist.count; idx++)
            {
                freq = bbxTempHist.frequency[idx];
                Printf(pri, "%12u%25u\n",
                        bbxTempHist.base + bbxTempHist.step * idx, freq);
            }
            Printf(pri, "        -------------------------------\n\n");
        }

        bbxTempHist = bbxTempDataParams.tempHistogramTime;
        if (bbxTempHist.count)
        {
            Printf(pri, "        Histogram of time, GPU was in various temperature ranges\n"
                        "        Step change : %d (in Celsius), upper bound of that range\n"
                        "        frequency : Amount of time (in sec) temp. remained in this range\n",
                        bbxTempHist.step);
            MASSERT(bbxTempHist.count <= LW90E7_CTRL_BBX_TEMP_HISTOGRAM_ENTRIES);
            Printf(pri, "        -------------------------------\n");
            Printf(pri, "        Temp                Frequency\n");
            Printf(pri, "        -------------------------------\n");
            UINT32 freq = 0;
            for (UINT32 idx = 0; idx < bbxTempHist.count; idx++)
            {
                freq = bbxTempHist.frequency[idx];
                string str;
                if (idx == 0)
                {
                    str = Utility::StrPrintf("< %u  ",
                                             bbxTempHist.base +
                                             bbxTempHist.step * idx);
                }
                else
                {
                    str = Utility::StrPrintf("%3u - %3u",
                                             bbxTempHist.base +
                                             bbxTempHist.step * (idx - 1),
                                             bbxTempHist.base +
                                             bbxTempHist.step * idx - 1);
                }
                Printf(pri, "%17s%15u\n", str.c_str(), freq);
            }
            Printf(pri, "        -------------------------------\n\n");
        }

        if (bbxTempDataParams.tempHourlyMaxSampleCount &&
            (bbxTempDataParams.tempHourlyMaxSample[0].timestamp != 0))
        {
            MASSERT(bbxTempDataParams.tempHourlyMaxSampleCount <=
                    LW90E7_CTRL_BBX_TEMP_HOURLY_MAX_ENTRIES);
            UINT32 validEntries = 0;
            for (; validEntries < bbxTempDataParams.tempHourlyMaxSampleCount; validEntries++)
            {
                if (bbxTempDataParams.tempHourlyMaxSample[validEntries].timestamp == 0)
                    break;
            }

            Printf(pri, "        Maximum GPU temp per hour, for last %u hours:\n", validEntries);
            Printf(pri, "        --------------------------------------------------------------\n");
            Printf(pri, "        Hour      GPU Temperature(Celsius)       Timestamp(EPOCH)\n");
            Printf(pri, "        --------------------------------------------------------------\n");
            for (UINT32 idx = 0; idx < validEntries; idx++)
            {
                LW90E7_CTRL_BBX_TEMP_ENTRY bbxTempEntry =
                                            bbxTempDataParams.tempHourlyMaxSample[idx];

                if (bbxTempEntry.timestamp == 0)
                    break;

                Printf(pri, "%12u%20.8f%37s\n", idx + 1,
                        Utility::ColwertFXPToFloat(bbxTempEntry.value, 24, 8),
                        GmTime(bbxTempEntry.timestamp));
            }
            Printf(pri, "        --------------------------------------------------------------\n\n");
        }

        if (bbxTempDataParams.tempMaxDayCount)
        {
            MASSERT(bbxTempDataParams.tempMaxDayCount <=
                    LW90E7_CTRL_BBX_TEMP_ENTRIES);
            Printf(pri, "        Maximum GPU temp per day, for last %u days:\n",
                   static_cast<unsigned>(bbxTempDataParams.tempMaxDayCount));
            Printf(pri, "        --------------------------------------------------------------\n");
            Printf(pri, "        Day      GPU Temperature(Celsius)       Timestamp(EPOCH)\n");
            Printf(pri, "        --------------------------------------------------------------\n");
            for (UINT32 idx = 0; idx < bbxTempDataParams.tempMaxDayCount; idx++)
            {
                LW90E7_CTRL_BBX_TEMP_ENTRY bbxTempEntry =
                                            bbxTempDataParams.tempMaxDay[idx];
                Printf(pri, "%10u%22.8f%37s\n", idx + 1,
                        Utility::ColwertFXPToFloat(bbxTempEntry.value, 24, 8),
                        GmTime(bbxTempEntry.timestamp));
            }
            Printf(pri, "        --------------------------------------------------------------\n\n");
        }

        if (bbxTempDataParams.tempMinDayCount)
        {
            MASSERT(bbxTempDataParams.tempMinDayCount <=
                    LW90E7_CTRL_BBX_TEMP_ENTRIES);
            Printf(pri, "        Minimum GPU temp per day, for last %u days:\n",
                   static_cast<unsigned>(bbxTempDataParams.tempMinDayCount));
            Printf(pri, "        --------------------------------------------------------------\n");
            Printf(pri, "        Day      GPU Temperature(Celsius)       Timestamp(EPOCH)\n");
            Printf(pri, "        --------------------------------------------------------------\n");
            for (UINT32 idx = 0; idx < bbxTempDataParams.tempMinDayCount; idx++)
            {
                LW90E7_CTRL_BBX_TEMP_ENTRY bbxTempEntry =
                                            bbxTempDataParams.tempMinDay[idx];
                Printf(pri, "%10u%22.8f%37s\n", idx + 1,
                        Utility::ColwertFXPToFloat(bbxTempEntry.value, 24, 8),
                        GmTime(bbxTempEntry.timestamp));
            }
            Printf(pri, "        --------------------------------------------------------------\n\n");
        }

        if (bbxTempDataParams.tempMaxWeekCount)
        {
            MASSERT(bbxTempDataParams.tempMaxWeekCount <= LW90E7_CTRL_BBX_TEMP_ENTRIES);
            Printf(pri, "        Maximum GPU temp per week, for last %u weeks:\n",
                   static_cast<unsigned>(bbxTempDataParams.tempMaxWeekCount));
            Printf(pri, "        --------------------------------------------------------------\n");
            Printf(pri, "        Week      GPU Temperature(Celsius)       Timestamp(EPOCH)\n");
            Printf(pri, "        --------------------------------------------------------------\n");
            for (UINT32 idx = 0; idx < bbxTempDataParams.tempMaxWeekCount; idx++)
            {
                LW90E7_CTRL_BBX_TEMP_ENTRY bbxTempEntry =
                                            bbxTempDataParams.tempMaxWeek[idx];
                Printf(pri, "%10u%22.8f%37s\n", idx + 1,
                        Utility::ColwertFXPToFloat(bbxTempEntry.value, 24, 8),
                        GmTime(bbxTempEntry.timestamp));
            }
            Printf(pri, "        --------------------------------------------------------------\n\n");
        }

        if (bbxTempDataParams.tempMinWeekCount)
        {
            MASSERT(bbxTempDataParams.tempMinWeekCount <=
                    LW90E7_CTRL_BBX_TEMP_ENTRIES);
            Printf(pri, "        Minimum GPU temp per week, for last %u weeks:\n",
                   static_cast<unsigned>(bbxTempDataParams.tempMinWeekCount));
            Printf(pri, "        --------------------------------------------------------------\n");
            Printf(pri, "        Week      GPU Temperature(Celsius)       Timestamp(EPOCH)\n");
            Printf(pri, "        --------------------------------------------------------------\n");
            for (UINT32 idx = 0; idx < bbxTempDataParams.tempMinWeekCount; idx++)
            {
                LW90E7_CTRL_BBX_TEMP_ENTRY bbxTempEntry =
                                            bbxTempDataParams.tempMinWeek[idx];
                Printf(pri, "%10u%22.8f%37s\n", idx + 1,
                        Utility::ColwertFXPToFloat(bbxTempEntry.value, 24, 8),
                        GmTime(bbxTempEntry.timestamp));
            }
            Printf(pri, "        --------------------------------------------------------------\n\n");
        }

        if (bbxTempDataParams.tempMaxMonthCount)
        {
            MASSERT(bbxTempDataParams.tempMaxMonthCount <= LW90E7_CTRL_BBX_TEMP_ENTRIES);
            Printf(pri, "        Maximum GPU temp per month, for last %u months:\n",
                   static_cast<unsigned>(bbxTempDataParams.tempMaxMonthCount));
            Printf(pri, "        --------------------------------------------------------------\n");
            Printf(pri, "        Month      GPU Temperature(Celsius)       Timestamp(EPOCH)\n");
            Printf(pri, "        --------------------------------------------------------------\n");
            for (UINT32 idx = 0; idx < bbxTempDataParams.tempMaxMonthCount; idx++)
            {
                LW90E7_CTRL_BBX_TEMP_ENTRY bbxTempEntry =
                                            bbxTempDataParams.tempMaxMonth[idx];
                Printf(pri, "%10u%24.8f%35s\n", idx + 1,
                        Utility::ColwertFXPToFloat(bbxTempEntry.value, 24, 8),
                        GmTime(bbxTempEntry.timestamp));
            }
            Printf(pri, "        --------------------------------------------------------------\n\n");
        }

        if (bbxTempDataParams.tempMinMonthCount)
        {
            MASSERT(bbxTempDataParams.tempMinMonthCount <=
                    LW90E7_CTRL_BBX_TEMP_ENTRIES);
            Printf(pri, "        Minimum GPU temp per month, for last %u months:\n",
                   static_cast<unsigned>(bbxTempDataParams.tempMinMonthCount));
            Printf(pri, "        --------------------------------------------------------------\n");
            Printf(pri, "        Month      GPU Temperature(Celsius)       Timestamp(EPOCH)\n");
            Printf(pri, "        --------------------------------------------------------------\n");
            for (UINT32 idx = 0; idx < bbxTempDataParams.tempMinMonthCount; idx++)
            {
                LW90E7_CTRL_BBX_TEMP_ENTRY bbxTempEntry =
                                            bbxTempDataParams.tempMinMonth[idx];
                Printf(pri, "%10u%24.8f%35s\n", idx + 1,
                        Utility::ColwertFXPToFloat(bbxTempEntry.value, 24, 8),
                        GmTime(bbxTempEntry.timestamp));
            }
            Printf(pri, "        --------------------------------------------------------------\n\n");
        }

        if (bbxTempDataParams.tempCompressionBufferCount &&
            (bbxTempDataParams.tempCompressionBuffer[0].timestamp != 0))
        {
            // The bucket size for the compression depends on the run time.  It starts hourly and
            // as soon as the available space is full switches to every 2 hours and removes half
            // the entries.  This pattern repeats until the sampling is oclwrring only every
            // 128 hours
            const UINT32 timeRunHours = bbxTdParams.timeRun / 60 / 60;
            UINT32 sampleRateShift = 0;
            if (timeRunHours >= LW90E7_CTRL_BBX_TEMP_COMPRESSION_BUFFER_ENTRIES)
            {
                // Maximum sample rate is 128 so cap the shift at 7
                sampleRateShift =
                    min(Utility::BitScanReverse(timeRunHours /
                                                LW90E7_CTRL_BBX_TEMP_COMPRESSION_BUFFER_ENTRIES),
                        7);
            }

            MASSERT(bbxTempDataParams.tempCompressionBufferCount <=
                    LW90E7_CTRL_BBX_TEMP_COMPRESSION_BUFFER_ENTRIES);
            Printf(pri, "        Maximum GPU temperatures taken every %u hour(s):\n",
                   1U << sampleRateShift);
            Printf(pri, "        --------------------------------------------------------------\n");
            Printf(pri, "            Hour      GPU Temperature(Celsius)      Timestamp(EPOCH)\n");
            Printf(pri, "        --------------------------------------------------------------\n");
            for (UINT32 idx = 0; idx < bbxTempDataParams.tempCompressionBufferCount; idx++)
            {
                LW90E7_CTRL_BBX_TEMP_ENTRY bbxTempEntry =
                                            bbxTempDataParams.tempCompressionBuffer[idx];

                if (bbxTempEntry.timestamp == 0)
                    break;

                Printf(pri, "%15u%22.8f%32s\n", (idx + 1)*(1U << sampleRateShift),
                        Utility::ColwertFXPToFloat(bbxTempEntry.value, 24, 8),
                        GmTime(bbxTempEntry.timestamp));
            }
            Printf(pri, "        --------------------------------------------------------------\n\n");
        }

        if (bbxTempDataParams.tempMaxAllCount)
        {
            MASSERT(bbxTempDataParams.tempMaxAllCount <= LW90E7_CTRL_BBX_TEMP_ENTRIES);
            Printf(pri, "        Maximum %u GPU temperatures:\n",
                   static_cast<unsigned>(bbxTempDataParams.tempMaxAllCount));
            Printf(pri, "        ----------------------------------------------------\n");
            Printf(pri, "        GPU Temperature(Celsius)       Timestamp(EPOCH)\n");
            Printf(pri, "        ----------------------------------------------------\n");
            for (UINT32 idx = 0; idx < bbxTempDataParams.tempMaxAllCount; idx++)
            {
                LW90E7_CTRL_BBX_TEMP_ENTRY bbxTempEntry =
                                            bbxTempDataParams.tempMaxAll[idx];
                Printf(pri, "%23.8f%36s\n",
                        Utility::ColwertFXPToFloat(bbxTempEntry.value, 24, 8),
                        GmTime(bbxTempEntry.timestamp));
            }
            Printf(pri, "        ----------------------------------------------------\n\n");
        }

        if (bbxTempDataParams.tempMinAllCount)
        {
            MASSERT(bbxTempDataParams.tempMinAllCount <=
                    LW90E7_CTRL_BBX_TEMP_ENTRIES);
            Printf(pri, "        Minimum %u GPU temperatures:\n",
                   static_cast<unsigned>(bbxTempDataParams.tempMinAllCount));
            Printf(pri, "        ----------------------------------------------------\n");
            Printf(pri, "        GPU Temperature(Celsius)       Timestamp(EPOCH)\n");
            Printf(pri, "        ----------------------------------------------------\n");
            for (UINT32 idx = 0; idx < bbxTempDataParams.tempMinAllCount; idx++)
            {
                LW90E7_CTRL_BBX_TEMP_ENTRY bbxTempEntry =
                                            bbxTempDataParams.tempMinAll[idx];
                Printf(pri, "%23.8f%36s\n",
                        Utility::ColwertFXPToFloat(bbxTempEntry.value, 24, 8),
                        GmTime(bbxTempEntry.timestamp));
            }
            Printf(pri, "        ----------------------------------------------------\n\n");
        }

        Printf(pri, "        Moving average of GPU temperature per hour, for last %u hours:\n",
                LW90E7_CTRL_BBX_TEMP_AVERAGE_HOUR_ENTRIES);
        Printf(pri, "        --------------------------------\n");
        Printf(pri, "        Hour        Temperature(Celsius)\n");
        Printf(pri, "        --------------------------------\n");
        for (UINT32 idx = 0; idx < LW90E7_CTRL_BBX_TEMP_AVERAGE_HOUR_ENTRIES; idx++)
        {
            FLOAT64 val = Utility::ColwertFXPToFloat(
                                bbxTempDataParams.tempAverageHour[idx], 24, 8);
            Printf(pri, "%11u%22.8f\n", idx + 1, val);
        }
        Printf(pri, "        --------------------------------\n\n");

        Printf(pri, "        Moving average of GPU temperature per day, for last %u days:\n",
                LW90E7_CTRL_BBX_TEMP_AVERAGE_DAY_ENTRIES);
        Printf(pri, "        -------------------------------\n");
        Printf(pri, "        Day        Temperature(Celsius)\n");
        Printf(pri, "        -------------------------------\n");
        for (UINT32 idx = 0; idx < LW90E7_CTRL_BBX_TEMP_AVERAGE_DAY_ENTRIES; idx++)
        {
            FLOAT64 val = Utility::ColwertFXPToFloat(
                                bbxTempDataParams.tempAverageDay[idx], 24, 8);
            Printf(pri, "%10u%22.8f\n", idx + 1, val);
        }
        Printf(pri, "        -------------------------------\n\n");

        Printf(pri, "        Moving average of GPU temperature per month, for last %u month:\n",
                LW90E7_CTRL_BBX_TEMP_AVERAGE_MONTH_ENTRIES);
        Printf(pri, "        ---------------------------------\n");
        Printf(pri, "        Month        Temperature(Celsius)\n");
        Printf(pri, "        ---------------------------------\n");
        for (UINT32 idx = 0;
             idx < LW90E7_CTRL_BBX_TEMP_AVERAGE_MONTH_ENTRIES; idx++)
        {
            FLOAT64 val = Utility::ColwertFXPToFloat(
                                bbxTempDataParams.tempAverageMonth[idx], 24, 8);
            Printf(pri, "%10u%24.8f\n", idx + 1, val);
        }
        Printf(pri, "        ---------------------------------\n\n");
    }

    LW90E7_CTRL_BBX_GET_TEMP_SAMPLES_PARAMS bbxTempSamplesParams = {0};
    rc = pLwRm->Control(cls90E7Handle,
                LW90E7_CTRL_CMD_BBX_GET_TEMP_SAMPLES,
                &bbxTempSamplesParams,
                sizeof(bbxTempSamplesParams));
    if (rc == RC::UNSUPPORTED_FUNCTION || rc == RC::LWRM_NOT_SUPPORTED)
    {
        Printf(pri, "        **************************************************\n"
                    "                 BBX recorded temperature samples\n"
                    "        **************************************************\n"
                    "        Not Supported\n");
        rc.Clear();
    }
    else
    {
        CHECK_RC(rc);
        Printf(pri, "        **************************************************\n"
                    "                 BBX recorded temperature samples\n"
                    "        **************************************************\n");
        Printf(pri, "        Periodic sampling interval in sec: %u\n",
               static_cast<unsigned>(bbxTempSamplesParams.tempSampleInterval));

        if (bbxTempSamplesParams.tempSampleCount)
        {
            MASSERT(bbxTempSamplesParams.tempSampleCount <=
                    LW90E7_CTRL_BBX_TEMP_SAMPLE_ENTRIES);
            Printf(pri, "        ---------------------------------------------------------"
                        "-------------------------------------------------------\n");
            Printf(pri, "          Temperature samples (in Celsius) in row major order, "
                    "Number of Temperature samples: %u\n",
                    static_cast<unsigned>(bbxTempSamplesParams.tempSampleCount));
            Printf(pri, "        ---------------------------------------------------------"
                        "-------------------------------------------------------");
            for (UINT32 idx = 0; idx < bbxTempSamplesParams.tempSampleCount; idx++)
            {
                if (idx % 6 == 0)
                    Printf(pri, "\n");
                FLOAT64 val = Utility::ColwertFXPToFloat(
                                    bbxTempSamplesParams.tempSample[idx], 24, 8);
                Printf(pri, "%20.8f", val);
            }
            Printf(pri, "\n        ---------------------------------------------------------"
                        "-------------------------------------------------------\n");
        }
    }

    LW90E7_CTRL_BBX_GET_SYSTEM_DATA_PARAMS bbxSystemParams = {0};
    rc = pLwRm->Control(cls90E7Handle,
                LW90E7_CTRL_CMD_BBX_GET_SYSTEM_DATA,
                &bbxSystemParams,
                sizeof(bbxSystemParams));
    if (rc == RC::UNSUPPORTED_FUNCTION || rc == RC::LWRM_NOT_SUPPORTED)
    {
        Printf(pri, "\n        **************************************************\n"
                    "                 System data\n"
                    "        **************************************************\n"
                    "        Not Supported\n");
        rc.Clear();
    }
    else
    {
        CHECK_RC(rc);
        Printf(pri, "\n        **************************************************\n"
                    "                 System data\n"
                    "        **************************************************\n");
        MASSERT(bbxSystemParams.driverVersionCount <=
                LW90E7_CTRL_BBX_GET_SYSTEM_DATA_ENTRIES);
        Printf(pri, "        LWPU driver version history\n");
        for (UINT32 idx = 0; idx < bbxSystemParams.driverVersionCount; idx++)
        {
            Printf(pri, "\n        Entry %u\n", idx + 1);
            Printf(pri, "          LWPU driver version       : %llu\n",
                    bbxSystemParams.driver[idx].version.version);
            Printf(pri, "          Timestamp                   : %s\n",
                    GmTime(bbxSystemParams.driver[idx].timestamp));
        }

        MASSERT(bbxSystemParams.vbiosVersionCount <=
                LW90E7_CTRL_BBX_GET_SYSTEM_DATA_ENTRIES);
        Printf(pri, "\n\n        Vbios version history\n");
        for (UINT32 idx = 0; idx < bbxSystemParams.vbiosVersionCount; idx++)
        {
            UINT32 vbiosVer = static_cast<unsigned>(bbxSystemParams.vbios[idx].version.vbios);
            string str = Utility::StrPrintf("%x.%02x.%02x.%02x.%02x",
                    (vbiosVer >> 24) & 0xFF,
                    (vbiosVer >> 16) & 0xFF,
                    (vbiosVer >>  8) & 0xFF,
                    (vbiosVer >>  0) & 0xFF,
                    bbxSystemParams.vbios[idx].version.oem & 0xFF);
            Printf(pri, "\n        Entry %u\n", idx + 1);
            Printf(pri, "          Vbios version               : %s\n", str.c_str());
            Printf(pri, "          Timestamp                   : %s\n",
                    GmTime(bbxSystemParams.vbios[idx].timestamp));
        }

        MASSERT(bbxSystemParams.osVersionCount <=
                LW90E7_CTRL_BBX_GET_SYSTEM_DATA_ENTRIES);
        Printf(pri, "\n\n        OS version history\n");
        for (UINT32 idx = 0; idx < bbxSystemParams.osVersionCount; idx++)
        {
            Printf(pri, "\n        Entry %u\n", idx + 1);
            Printf(pri, "          OS type                     : %u\n",
                    bbxSystemParams.os[idx].version.type);
            Printf(pri, "          OS major version number     : %u\n",
                    bbxSystemParams.os[idx].version.major);
            Printf(pri, "          OS minor version number     : %u\n",
                    bbxSystemParams.os[idx].version.minor);
            Printf(pri, "          OS build number             : %u\n",
                    bbxSystemParams.os[idx].version.build);
            Printf(pri, "          Timestamp                   : %s\n",
                    GmTime(bbxSystemParams.os[idx].timestamp));
        }
    }

    LW90E7_CTRL_BBX_GET_PCIE_DATA_PARAMS bbxPcieParams = {0};
    rc = pLwRm->Control(cls90E7Handle,
                LW90E7_CTRL_CMD_BBX_GET_PCIE_DATA,
                &bbxPcieParams,
                sizeof(bbxPcieParams));
    if (rc == RC::UNSUPPORTED_FUNCTION || rc == RC::LWRM_NOT_SUPPORTED)
    {
        Printf(pri, "\n        **************************************************\n"
                    "                 PCIE data\n"
                    "        **************************************************\n"
                    "        Not Supported\n");
        rc.Clear();
    }
    else
    {
        CHECK_RC(rc);
        Printf(pri, "\n        **************************************************\n"
                    "                 PCIE data\n"
                    "        **************************************************\n");
        Printf(pri, "        No. of PCIe Uncorrectable non-fatal errors : %u\n",
                static_cast<unsigned>(bbxPcieParams.pcieNonFatalErrCount));
        Printf(pri, "        No. of PCIe Uncorrectable fatal errors     : %u\n",
                static_cast<unsigned>(bbxPcieParams.pcieFatalErrCount));
        Printf(pri, "        No. of individiual PCIe uncorrectable non-fatal errors\n"
                "        reported by Advanced Error Reporting \n");
        Printf(pri, "        -------------------------------------------------\n");
        Printf(pri, "                Error type                  Count\n");
        Printf(pri, "        -------------------------------------------------\n");
        for (UINT32 idx = 0;
             idx < LW90E7_CTRL_BBX_PCIE_AER_UNCORR_IDX_ENTRIES; idx++)
        {
            string aerError[] = {"RSVD", "DLINK_PROTO_ERR", "SURPRISE_DOWN",
                                "POISONED_TLP", "FC_PROTO_ERR", "CPL_TIMEOUT",
                                "CPL_ABORT", "UNEXP_CPL", "RCVR_OVERFLOW",
                                "MALFORMED_TLP", "ECRC_ERROR","UNSUPPORTED_REQ",
                                "ACS_VIOLATION", "INTERNAL_ERROR", "MC_BLOCKED_TLP",
                                "ATOMIC_OP_EGRESS_BLOCKED", "TLP_PREFIX_BLOCKED",
                                "RESERVED"};
            Printf(pri, "%32s%15u\n", aerError[idx].c_str(),
                   static_cast<unsigned>(bbxPcieParams.pcieAerErrCount[idx]));
        }
        Printf(pri, "        -------------------------------------------------\n");
        Printf(pri, "\n");
        MASSERT(bbxPcieParams.pcieCorrErrRateMaxDayCount <=
                LW90E7_CTRL_BBX_PCIE_CORR_ERR_RATE_ENTRIES);
        MASSERT(bbxPcieParams.pcieCorrErrRateMaxMonthCount <=
                LW90E7_CTRL_BBX_PCIE_CORR_ERR_RATE_ENTRIES);

        Printf(pri,
               "        Maximum PCIe correctable error rate per day for past %u days\n",
               static_cast<unsigned>(bbxPcieParams.pcieCorrErrRateMaxDayCount));
        Printf(pri, "        -------------------------------------------------\n");
        Printf(pri, "        Error rate(Errors/minute)       Timestamp\n");
        Printf(pri, "        -------------------------------------------------\n");

        for (UINT32 idx = 0;
             idx < bbxPcieParams.pcieCorrErrRateMaxDayCount; idx++)
        {
            Printf(pri, "%12u%45s\n",
                   static_cast<unsigned>(bbxPcieParams.pcieCorrErrRateMaxDay[idx].value),
                   GmTime(bbxPcieParams.pcieCorrErrRateMaxDay[idx].timestamp));
        }
        Printf(pri, "        -------------------------------------------------\n\n");

        Printf(pri,
               "        Maximum PCIe correctable error rate per day for past %u months\n",
               static_cast<unsigned>(bbxPcieParams.pcieCorrErrRateMaxMonthCount));
        Printf(pri, "        -------------------------------------------------\n");
        Printf(pri, "        Error rate(Errors/minute)       Timestamp\n");
        Printf(pri, "        -------------------------------------------------\n");

        for (UINT32 idx = 0;
             idx < bbxPcieParams.pcieCorrErrRateMaxMonthCount; idx++)
        {
            Printf(pri, "%12u%45s\n",
                   static_cast<unsigned>(bbxPcieParams.pcieCorrErrRateMaxMonth[idx].value),
                   GmTime(bbxPcieParams.pcieCorrErrRateMaxMonth[idx].timestamp));
        }
        Printf(pri, "        -------------------------------------------------\n");
    }

    LW90E7_CTRL_BBX_GET_XID2_DATA_PARAMS bbxXid2Params = {0};
    rc = pLwRm->Control(cls90E7Handle,
                LW90E7_CTRL_CMD_BBX_GET_XID2_DATA,
                &bbxXid2Params,
                sizeof(bbxXid2Params));
    if (rc == RC::UNSUPPORTED_FUNCTION || rc == RC::LWRM_NOT_SUPPORTED)
    {
        Printf(pri, "\n        **************************************************\n"
                    "                 BBX recorded Xid2 data\n"
                    "        **************************************************\n"
                    "        Not Supported\n");
        rc.Clear();
    }
    else
    {
        CHECK_RC(rc);
        Printf(pri, "\n        **************************************************\n"
                    "                 BBX recorded Xid2 data\n"
                    "        **************************************************\n");
        Printf(pri, "        Number of times Xid 13 oclwrred          : %u\n",
               static_cast<unsigned>(bbxXid2Params.xid13Count));
        Printf(pri, "        Number of times Xid 31 oclwrred          : %u\n",
               static_cast<unsigned>(bbxXid2Params.xid31Count));
        Printf(pri, "        Number of times other Xids oclwrred      : %u\n",
               static_cast<unsigned>(bbxXid2Params.xidOtherCount));
        Printf(pri, "        Number of xidEntry entries               : %u\n",
               static_cast<unsigned>(bbxXid2Params.xidFirstEntryCount));

        if (bbxXid2Params.xidFirstEntryCount)
        {
            MASSERT(bbxXid2Params.xidFirstEntryCount <=
                    LW90E7_CTRL_BBX_XID2_ENTRIES);
            Printf(pri, "        Entires for first few Xids oclwrred:\n");
            for (UINT32 idx = 0; idx < bbxXid2Params.xidFirstEntryCount; idx++)
            {
                const LW90E7_CTRL_BBX_XID2_ENTRY &bbxXid2Entry =
                    bbxXid2Params.xidFirstEntry[idx];
                Printf(pri, "          Timestamp (EPOCH) of the entry %u: %s\n",
                        idx + 1, GmTime(bbxXid2Entry.timestamp));
                Printf(pri, "          Xid number: %u\n", bbxXid2Entry.number);
                Printf(pri, "          ECC Enabled: %s\n",
                        bbxXid2Entry.bEccEnabled ? "true" : "false");
                Printf(pri, "          OS Type: %u\n", bbxXid2Entry.os.type);
                Printf(pri, "          OS major version: %u\n", bbxXid2Entry.os.major);
                Printf(pri, "          OS minor version: %u\n", bbxXid2Entry.os.minor);
                Printf(pri, "          OS build number: %u\n", bbxXid2Entry.os.build);
                Printf(pri, "          Lwpu RM driver version: %llu\n\n",
                        bbxXid2Entry.driver.version);
            }
        }

        Printf(pri, "        Number of xidLastEntry entries           : %u\n",
                bbxXid2Params.xidLastEntryCount);

        if (bbxXid2Params.xidLastEntryCount)
        {
            MASSERT(bbxXid2Params.xidLastEntryCount <=
                    LW90E7_CTRL_BBX_XID2_ENTRIES);
            Printf(pri, "        Entires for most recent Xids oclwrred:\n");
            for (UINT32 idx = 0; idx < bbxXid2Params.xidLastEntryCount; idx++)
            {
                const LW90E7_CTRL_BBX_XID2_ENTRY &bbxXid2Entry =
                    bbxXid2Params.xidLastEntry[idx];
                Printf(pri, "          Timestamp (EPOCH) of the entry %u: %s\n",
                        idx + 1, GmTime(bbxXid2Entry.timestamp));
                Printf(pri, "          Xid number: %u\n", bbxXid2Entry.number);
                Printf(pri, "          ECC Enabled: %s\n",
                        bbxXid2Entry.bEccEnabled ? "true" : "false");
                Printf(pri, "          OS Type: %u\n", bbxXid2Entry.os.type);
                Printf(pri, "          OS major version: %u\n",
                       bbxXid2Entry.os.major);
                Printf(pri, "          OS minor version: %u\n",
                       bbxXid2Entry.os.minor);
                Printf(pri, "          OS build number: %u\n",
                       bbxXid2Entry.os.build);
                Printf(pri, "          Lwpu RM driver version: %llu\n\n",
                        bbxXid2Entry.driver.version);
            }
        }

        Printf(pri, "        Number of xidFirstDetailedEntry entries  : %u\n",
               static_cast<unsigned>(bbxXid2Params.xidFirstDetailedEntryCount));

        if (bbxXid2Params.xidFirstDetailedEntryCount)
        {
            MASSERT(bbxXid2Params.xidFirstDetailedEntryCount <=
                    LW90E7_CTRL_BBX_XID2_ENTRIES);
            Printf(pri, "        Detailed entires for first few Xids oclwrred:\n");
            for (UINT32 idx = 0;
                 idx < bbxXid2Params.xidFirstDetailedEntryCount; idx++)
            {
                const LW90E7_CTRL_BBX_XID2_DETAILED_ENTRY &bbxXid2Detail =
                    bbxXid2Params.xidFirstDetailedEntry[idx];
                const LW90E7_CTRL_BBX_XID2_ENTRY &bbxXid2Entry = bbxXid2Detail.xid;
                Printf(pri, "          Timestamp (EPOCH) of the entry %u     : %s\n",
                        idx + 1, GmTime(bbxXid2Entry.timestamp));
                Printf(pri, "          Xid number                           : %u\n",
                            bbxXid2Entry.number);
                Printf(pri, "          ECC Enabled                          : %s\n",
                            bbxXid2Entry.bEccEnabled ? "true" : "false");
                Printf(pri, "          OS Type                              : %u\n",
                            bbxXid2Entry.os.type);
                Printf(pri, "          OS major version                     : %u\n",
                            bbxXid2Entry.os.major);
                Printf(pri, "          OS minor version                     : %u\n",
                            bbxXid2Entry.os.minor);
                Printf(pri, "          OS build number                      : %u\n",
                            bbxXid2Entry.os.build);
                Printf(pri, "          Lwpu RM driver version             : %llu\n",
                            bbxXid2Entry.driver.version);
                Printf(pri, "          Array of data associated with the Xid:");
                for (UINT32 iidx = 0;
                     iidx < LW90E7_CTRL_BBX_XID2_DETAILED_DATA_ENTRIES; iidx++)
                {
                    Printf(pri, " 0x%08x",
                           static_cast<unsigned>(bbxXid2Detail.data[iidx]));
                }
                Printf(pri, "\n\n");
            }
        }

        Printf(pri, "        Number of xidLastDetailedEntry entries   : %u\n",
               static_cast<unsigned>(bbxXid2Params.xidLastDetailedEntryCount));

        if (bbxXid2Params.xidLastDetailedEntryCount)
        {
            MASSERT(bbxXid2Params.xidLastDetailedEntryCount <=
                    LW90E7_CTRL_BBX_XID2_ENTRIES);
            Printf(pri,
                   "        Detailed entires for most recent Xids oclwrred:\n");
            for (UINT32 idx = 0;
                 idx < bbxXid2Params.xidLastDetailedEntryCount; idx++)
            {
                const LW90E7_CTRL_BBX_XID2_DETAILED_ENTRY &bbxXid2Detail =
                    bbxXid2Params.xidLastDetailedEntry[idx];
                const LW90E7_CTRL_BBX_XID2_ENTRY &bbxXid2Entry = bbxXid2Detail.xid;
                Printf(pri, "          Timestamp (EPOCH) of the entry %u: %s\n",
                        idx + 1, GmTime(bbxXid2Entry.timestamp));
                Printf(pri, "          Xid number: %u\n", bbxXid2Entry.number);
                Printf(pri, "          ECC Enabled: %s\n",
                            bbxXid2Entry.bEccEnabled ? "true" : "false");
                Printf(pri, "          OS Type: %u\n", bbxXid2Entry.os.type);
                Printf(pri, "          OS major version: %u\n",
                       bbxXid2Entry.os.major);
                Printf(pri, "          OS minor version: %u\n",
                       bbxXid2Entry.os.minor);
                Printf(pri, "          OS build number: %u\n",
                       bbxXid2Entry.os.build);
                Printf(pri, "          Lwpu RM driver version: %llu\n",
                            bbxXid2Entry.driver.version);
                Printf(pri, "          Array of data associated with the Xid:");
                for (UINT32 iidx = 0;
                     iidx < LW90E7_CTRL_BBX_XID2_DETAILED_DATA_ENTRIES; iidx++)
                {
                    Printf(pri, " %u",
                           static_cast<unsigned>(bbxXid2Detail.data[iidx]));
                }
                Printf(pri, "\n\n");
            }
        }
    }

#undef GmTime

    return rc;
}

RC GpuSubdevice::CheckInfoRomForPBLCapErr(UINT32 pblCap) const
{
    RC rc;
    UINT32 version, subVersion;
    CHECK_RC(CheckInfoRom());
    CHECK_RC(GetInfoRomObjectVersion(LW2080_CTRL_GPU_INFOROM_OBJECT_NAME_PBL,
                                     &version,
                                     &subVersion));

    if ((version != 0) || (subVersion != 0))
    {
        Blacklist blacklist;
        CHECK_RC(blacklist.Load(this));

        UINT32 blacklistSize = blacklist.GetSize();
        if (blacklistSize > pblCap)
        {
            Printf(Tee::PriError, "PBL size %u is greater than allowed capacity %u\n",
                                   blacklistSize, pblCap);
            return RC::RM_RCH_INFOROM_DPR_FAILURE;
        }
    }

    return RC::OK;
}

RC GpuSubdevice::CheckInfoRomForDprTotalErr(UINT32 maxPages, bool onlyDbe) const
{
    RC rc;
    UINT32 version, subVersion;
    CHECK_RC(CheckInfoRom());
    CHECK_RC(GetInfoRomObjectVersion(LW2080_CTRL_GPU_INFOROM_OBJECT_NAME_PBL,
                                     &version,
                                     &subVersion));

    if ((version != 0) || (subVersion != 0))
    {
        Blacklist blacklist;
        vector<Blacklist::AddrInfo> addrInfo;
        CHECK_RC(blacklist.Load(this));
        CHECK_RC(blacklist.GetPages(&addrInfo));

        UINT32 numDpr = 0;
        for (const Blacklist::AddrInfo& info : addrInfo)
        {
            if (info.source == Blacklist::Source::DPR_DBE ||
                (!onlyDbe && info.source == Blacklist::Source::DPR_SBE))
            {
                numDpr++;
            }
        }
        if (numDpr > maxPages)
        {
            Printf(Tee::PriError, "PBL with DPR %u is greater than specified max %u\n",
                                   numDpr, maxPages);
            return RC::RM_RCH_INFOROM_DPR_FAILURE;
        }
    }

    return rc;
}

RC GpuSubdevice::CheckInfoRomForDprDbeRateErr(UINT32 ratePages, UINT32 rateDays) const
{
    RC rc;
    UINT32 version, subVersion;
    CHECK_RC(CheckInfoRom());
    CHECK_RC(GetInfoRomObjectVersion(LW2080_CTRL_GPU_INFOROM_OBJECT_NAME_PBL,
                                     &version,
                                     &subVersion));

    if ((version != 0) || (subVersion != 0))
    {
        Blacklist blacklist;
        vector<Blacklist::AddrInfo> addrInfo;

        CHECK_RC(blacklist.Load(this));
        CHECK_RC(blacklist.GetSortedPages(&addrInfo));

        UINT32 numBlacklistedPages = static_cast<UINT32>(addrInfo.size());
        if (numBlacklistedPages < ratePages)
        {
            return RC::OK;
        }

        const UINT32 secPerDay = 86400;
        const UINT32 rateSec   = rateDays * secPerDay;
        for (UINT32 i = 0; i < numBlacklistedPages; i++)
        {
            if (addrInfo[i].source != Blacklist::Source::DPR_DBE)
            {
                continue;
            }

            UINT32 endTime  = addrInfo[i].timestamp + rateSec;
            UINT32 numPages = 1;
            for (UINT32 j = i + 1; j < numBlacklistedPages; j++)
            {
                if (addrInfo[j].timestamp > endTime)
                {
                    break;
                }

                if (addrInfo[j].source == Blacklist::Source::DPR_DBE)
                {
                    numPages++;
                    if (numPages > ratePages)
                    {
                        Printf(Tee::PriError, "DPR rate exceeds specified max\n");
                        return RC::RM_RCH_INFOROM_DPR_FAILURE;
                    }
                }
            }
        }
    }

    return RC::OK;
}

RC GpuSubdevice::CheckInfoRom() const
{
    if (IsSOC())
    {
        return RC::LWRM_NOT_SUPPORTED;
    }

    RC rc;
    LwRmPtr pLwRm;
    LwRm::Handle hSubDevDiag;

    hSubDevDiag = GetSubdeviceDiag();
    LW208F_CTRL_GPU_VERIFY_INFOROM_PARAMS params = { 0 };
    rc = pLwRm->Control(hSubDevDiag,
                        LW208F_CTRL_CMD_GPU_VERIFY_INFOROM,
                        &params,
                        sizeof(params));
    CHECK_RC(rc);

    switch (params.result)
    {
        case LW208F_CTRL_GPU_INFOROM_VERIFICATION_RESULT_VALID:
            Printf(Tee::PriLow, "InfoROM is valid.\n");
            return OK;
        case LW208F_CTRL_GPU_INFOROM_VERIFICATION_RESULT_ILWALID:
            Printf(Tee::PriLow, "InfoROM is invalid.\n");
            return RC::ILWALID_INFO_ROM;
        default:
            Printf(Tee::PriError, "Unable to verify InfoROM.\n");
            return RC::SOFTWARE_ERROR;
    }
}

void GpuSubdevice::SetInfoRomVersion(string infoRomVersion)
{
    m_InfoRomVersion = infoRomVersion;
}

const string& GpuSubdevice::GetInfoRomVersion()
{
    if (m_InfoRomVersion.empty())
    {
        LW2080_CTRL_GPU_GET_INFOROM_IMAGE_VERSION_PARAMS verParams;
        memset(&verParams, 0, sizeof (verParams));
        if (OK == LwRmPtr()->ControlBySubdevice(
                        this, LW2080_CTRL_CMD_GPU_GET_INFOROM_IMAGE_VERSION,
                        &verParams, sizeof(verParams)))
        {
            Printf(Tee::PriLow,
                   "\nInfoRom Image Version: %s\n", verParams.version);
            m_InfoRomVersion = Utility::StrPrintf("%s", verParams.version);
        }
    }

    // Return the cached/empty InfoROM version
    return m_InfoRomVersion;
}

string GpuSubdevice::GetBoardSerialNumber() const
{
    RC rc;
    LwRmPtr pLwRm;

    string serNum;

    rc = CheckInfoRom();
    if ((rc != RC::UNSUPPORTED_FUNCTION) && (rc != RC::LWRM_NOT_SUPPORTED))
    {
        UINT32 version, subVersion;
        if (OK == GetInfoRomObjectVersion(
                        LW2080_CTRL_GPU_INFOROM_OBJECT_NAME_OBD,
                        &version, &subVersion))
        {
            if ((version != 0) || (subVersion != 0))
            {
                LW2080_CTRL_GPU_GET_OEM_BOARD_INFO_PARAMS odbParams = { 0 };
                if (
                    OK == pLwRm->ControlBySubdevice(
                        this,
                        LW2080_CTRL_CMD_GPU_GET_OEM_BOARD_INFO,
                        &odbParams,
                        sizeof(odbParams)
                    )
                )
                {
                    serNum.assign(reinterpret_cast<const char*>(
                                            &odbParams.serialNumber[0]),
                                  LW2080_GPU_MAX_SERIAL_NUMBER_LENGTH);
                    string::size_type end = serNum.find('\0');
                    if (string::npos != end) serNum.erase(end);
                }
            }
        }
    }

    if (serNum.empty())
    {
        Printf(Tee::PriLow, "\tInforom does not contain Board Serial Number\n");
    }

    return serNum;
}

string GpuSubdevice::GetBoardMarketingName() const
{
    RC rc;
    LwRmPtr pLwRm;

    string marketingName;

    rc = CheckInfoRom();
    if ((rc != RC::UNSUPPORTED_FUNCTION) && (rc != RC::LWRM_NOT_SUPPORTED))
    {
        UINT32 version, subVersion;
        if (RC::OK == GetInfoRomObjectVersion(LW2080_CTRL_GPU_INFOROM_OBJECT_NAME_OBD, &version,
                                              &subVersion))
        {
            if ((version != 0) || (subVersion != 0))
            {
                LW2080_CTRL_GPU_GET_OEM_BOARD_INFO_PARAMS odbParams = {};
                if (RC::OK == pLwRm->ControlBySubdevice(this,
                    LW2080_CTRL_CMD_GPU_GET_OEM_BOARD_INFO, &odbParams, sizeof(odbParams)))
                {

                    marketingName.assign(reinterpret_cast<const char*>(&odbParams.marketingName[0]),
                                         sizeof(odbParams.marketingName));
                    string::size_type end = marketingName.find('\0');
                    if (string::npos != end)
                    {
                        marketingName.erase(end);
                    }
                }
            }
        }
    }

    if (marketingName.empty())
    {
        Printf(Tee::PriLow, "\tInforom does not contain Marketing Name\n");
    }

    return marketingName;
}

string GpuSubdevice::GetBoardProjectSerialNumber() const
{
    RC rc;
    rc = CheckInfoRom();
    string boardSrNo;
    if ((rc != RC::UNSUPPORTED_FUNCTION) && (rc != RC::LWRM_NOT_SUPPORTED))
    {
        UINT32 version, subVersion;
        rc = GetInfoRomObjectVersion(LW2080_CTRL_GPU_INFOROM_OBJECT_NAME_OBD,
                                    &version, &subVersion);
        if ((rc == OK) && (version != 0 || subVersion != 0))
        {
            LW2080_CTRL_GPU_GET_OEM_BOARD_INFO_PARAMS odbParams = { 0 };
            LwRmPtr pLwRm;
            rc = pLwRm->ControlBySubdevice(this,
                    LW2080_CTRL_CMD_GPU_GET_OEM_BOARD_INFO,
                    &odbParams,
                    sizeof(odbParams));
            if (rc == OK)
            {
                string::size_type end;
                char   brdType = odbParams.boardType;
                string brdPartNum((const char *)odbParams.productPartNumber,
                                  LW2080_GPU_MAX_PRODUCT_PART_NUMBER_LENGTH);
                string brdRevision((const char *)odbParams.boardRevision, 3);
                string brdSerNum((const char *)odbParams.serialNumber,
                                 LW2080_GPU_MAX_SERIAL_NUMBER_LENGTH);

                if (!isalpha(brdType) || brdPartNum.empty() ||
                        brdRevision.empty() ||  brdSerNum.empty())
                {
                    Printf(Tee::PriLow,
                          "Inforom does not contain Project Board Serial Number Info\n");
                    return boardSrNo;
                }

                end = brdPartNum.find('\0');
                if (string::npos != end) brdPartNum.erase(end);
                // Pad string with spaces, in case incomplete information
                // has been flashed in the inforom.
                brdPartNum.insert(brdPartNum.end(),
                                  LW2080_GPU_MAX_PRODUCT_PART_NUMBER_LENGTH -
                                  brdPartNum.size(),
                                  ' ');

                end = brdRevision.find('\0');
                if (string::npos != end) brdRevision.erase(end);
                brdRevision.insert(brdRevision.end(),
                                   3 - brdRevision.size(), ' ');

                end = brdSerNum.find('\0');
                if (string::npos != end) brdSerNum.erase(end);

                boardSrNo = brdType;

                //Extract Board Number from ProductPartNumber which
                //is at 5th index of length 4
                boardSrNo.append(brdPartNum, 5, 4);

                boardSrNo += ("-" + brdRevision + "-");

                //Extract Uid from Prod. Ser. No. which is last 4 char
                size_t origLength = brdSerNum.length();
                if (origLength >= 4)
                {
                    boardSrNo.append(brdSerNum, origLength - 4, 4);
                }
                else
                {
                    boardSrNo += brdSerNum;
                }
            }
        }
    }
    return boardSrNo;
}

RC GpuSubdevice::SetFbMinimum ( UINT32 fbMinMB )
{
    // The following ranges are based on fermi limits; see
    // https://wiki.lwpu.com/gpuhwdept/index.php/GPU_Verification_Infrastructure_Group/MODS_Verification_Infrastructure/FbSize
    const UINT32 minFbColumnBits = 8;
    const UINT32 minFbRowBits = 10;
    const UINT32 minBankBits = 2;

    return SetFbMinimumInternal(
            fbMinMB,
            minFbColumnBits,
            minFbRowBits,
            minBankBits);
}

//------------------------------------------------------------------------------
// Callwlate & set Fbbanks, FbColumnBits, & FbRowBits based on the fbMinMB.
// This algorithm assumes the following constraints:
// - Max Column bits = 12
// - Max Row bits = 17
// Callwlate the minimum number of banks, then the minimum number of columns &
// finally the minimum number of rows needed to access fbMinMB.
RC GpuSubdevice::SetFbMinimumInternal
(
    UINT32 fbMinMB,
    const UINT32 minFbColumnBits,
    const UINT32 minFbRowBits,
    const UINT32 minBankBits
)
{
    const UINT32 maxFbRowBits = 17;
    const UINT32 maxFbColBits = 12;
    const UINT32 maxFbRowColBits = maxFbRowBits +  maxFbColBits;

    UINT64 Value = fbMinMB;

    UINT32 numPartitions = GetFrameBufferUnitCount();
    MASSERT(numPartitions);
    Value /= numPartitions;

    UINT32 powerOfTwoValue = 2;
    while (powerOfTwoValue < Value)
    {
        powerOfTwoValue *= 2;
    }
    Value = powerOfTwoValue;

    if (Value > 16*1024)
    {
        Printf(Tee::PriError, "Invalid value (%u) for -fb_minimum; "
            "Acceptable values are <= 16*1024*numFBs.\n", fbMinMB);
        return RC::BAD_COMMAND_LINE_ARGUMENT;
    }

    Value /= 2; // multi-channel
    Value *= 1024*1024;
    Value /= 4; // bytes per unit

    // The following ranges are based on fermi limits; see
    // https://wiki.lwpu.com/gpuhwdept/index.php/GPU_Verification_Infrastructure_Group/MODS_Verification_Infrastructure/FbSize
    JavaScriptPtr pJs;
    UINT32 FbBanks = 1 << minBankBits;
    Value /= FbBanks;
    while (Value > (1 << maxFbRowColBits))
    {
        FbBanks *= 2;
        Value /= 2;
    }
    pJs->SetProperty(Gpu_Object, Gpu_FbBanks, FbBanks);

    UINT32 FbColumnBits = minFbColumnBits;
    Value /= (1ULL << minFbColumnBits);
    while (Value > (1<<maxFbRowBits))
    {
        FbColumnBits += 1;
        Value /= 2;
    }
    pJs->SetProperty(Gpu_Object, Gpu_FbColumnBits, FbColumnBits);

    UINT32 FbRowBits = minFbRowBits;
    Value /= (1ULL << minFbRowBits);
    while (Value > 1)
    {
        FbRowBits += 1;
        Value /= 2;
    }
    pJs->SetProperty(Gpu_Object, Gpu_FbRowBits, FbRowBits);

    return OK;
}

void GpuSubdevice::DumpRegs(UINT32 startAddr, UINT32 endAddr) const
{
    for (UINT32 i=startAddr; i<=endAddr; i+=4)
    {
        const UINT32 val = RegRd32(i);
        Printf(Tee::PriNormal, "[0x%08x]->0x%08x\n", i, val);
    }
}

UINT32 GpuSubdevice::GetFbRegionPerformance(UINT32 regionIndex)
{
    MASSERT(regionIndex < m_FbRegionInfo.numFBRegions);

    return m_FbRegionInfo.fbRegion[regionIndex].performance;
}

void GpuSubdevice::GetFbRegionAddressRange
(
    UINT32 regionIndex,
    UINT64 *minAddress,
    UINT64 *maxAddress
)
{
    MASSERT(regionIndex < m_FbRegionInfo.numFBRegions);

    *minAddress = m_FbRegionInfo.fbRegion[regionIndex].base;
    *maxAddress = m_FbRegionInfo.fbRegion[regionIndex].limit;
}

UINT64 GpuSubdevice::UsableFbHeapSize(UINT32 kinds) const
{
    // If neither protected nor non-protected is specified,
    // assume the caller wanted only non-protected regions.
    if (0 == (kinds & (useProtected | useNonProtected)))
    {
        kinds |= useNonProtected;
    }

    UINT64 total = 0;
    for (UINT32 i=0; i < m_FbRegionInfo.numFBRegions; i++)
    {
        UINT32 flags = 0;
        if (m_FbRegionInfo.fbRegion[i].supportCompressed)
        {
            flags |= useCompressed;
        }
        if (m_FbRegionInfo.fbRegion[i].supportISO)
        {
            flags |= useDisplayable;
        }
        if (m_FbRegionInfo.fbRegion[i].bProtected)
        {
            flags |= useProtected;
        }
        else
        {
            flags |= useNonProtected;
        }
        if ((flags & kinds) == kinds)
        {
            const UINT64 base = m_FbRegionInfo.fbRegion[i].base;
            const UINT64 limit = m_FbRegionInfo.fbRegion[i].limit;
            const UINT64 size = limit - base + 1;
            total += size;
        }
    }
    return total;
}

//------------------------------------------------------------------------------
struct PollRegArgs
{
    const GpuSubdevice* pSubdev;
    UINT32              address;
    UINT32              value;
    UINT32              mask;
};

//------------------------------------------------------------------------------
static bool PollRegValueFunc(void *pArgs)
{
    PollRegArgs *pollArgs = (PollRegArgs*)(pArgs);
    UINT32 address = pollArgs->address;
    UINT32 value   = pollArgs->value;
    UINT32 mask    = pollArgs->mask;

    UINT32 readValue = pollArgs->pSubdev->RegRd32(address);

    return ((value & mask) == (readValue & mask));
}

//------------------------------------------------------------------------------
RC GpuSubdevice::PollRegValue
(
    UINT32 address,
    UINT32 value,
    UINT32 mask,
    FLOAT64 timeoutMs
) const
{
    PollRegArgs pollArgs;
    pollArgs.pSubdev = this;
    pollArgs.address = address;
    pollArgs.value = value;
    pollArgs.mask = mask;
    return POLLWRAP_HW(PollRegValueFunc, &pollArgs, timeoutMs);
}

//------------------------------------------------------------------------------
static bool PollRegValueGreaterOrEqualFunc(void *pArgs)
{
    PollRegArgs *pollArgs = (PollRegArgs*)(pArgs);
    UINT32 address = pollArgs->address;
    UINT32 value   = pollArgs->value;
    UINT32 mask    = pollArgs->mask;
    UINT32 readValue = pollArgs->pSubdev->RegRd32(address);
    bool retval = ((value & mask) >= (readValue & mask)) ? true : false;
    return retval;
}

//------------------------------------------------------------------------------
RC GpuSubdevice::PollRegValueGreaterOrEqual
(
    UINT32 address,
    UINT32 value,
    UINT32 mask,
    FLOAT64 timeoutMs
) const
{
    PollRegArgs pollArgs;
    pollArgs.pSubdev = this;
    pollArgs.address = address;
    pollArgs.value   = value;
    pollArgs.mask    = mask;
    return POLLWRAP_HW(PollRegValueGreaterOrEqualFunc,
            &pollArgs, timeoutMs);
}

//------------------------------------------------------------------------------
struct PollRegHalFieldArgs
{
    UINT32 fieldValue;
    ModsGpuRegField field;
    RegHal::ArrayIndexes_t indexes;
    GpuSubdevice *pSub;
};

//------------------------------------------------------------------------------
static bool PollRegFieldGreaterOrEqualFunc(void *pArgs)
{
    PollRegHalFieldArgs *pollRegHalArgs = (PollRegHalFieldArgs*)(pArgs);

    UINT32 regVal = pollRegHalArgs->pSub->Regs().Read32(
            pollRegHalArgs->field, pollRegHalArgs->indexes);
    return regVal >= pollRegHalArgs->fieldValue;
}

//------------------------------------------------------------------------------
RC GpuSubdevice::PollGpuRegFieldGreaterOrEqual
(
    ModsGpuRegField field,
    UINT32 regIndex1,
    UINT32 regIndex2,
    UINT32 value,
    FLOAT64 timeoutMs
)
{
    PollRegHalFieldArgs pollArgs;
    pollArgs.pSub = this;
    pollArgs.fieldValue = value;
    pollArgs.field = field;
    pollArgs.indexes = { regIndex1, regIndex2 };
    return POLLWRAP_HW(PollRegFieldGreaterOrEqualFunc, &pollArgs, timeoutMs);
}

//------------------------------------------------------------------------------
struct PollRegHalArgs
{
    ModsGpuRegValue gpuRegValue;
    RegHal::ArrayIndexes_t indexes;
    GpuSubdevice *pSub;
};

//------------------------------------------------------------------------------
static bool PollRegHalValueFunc(void *pArgs)
{
    PollRegHalArgs *pPollRegHalArgs = (PollRegHalArgs*)(pArgs);

    return pPollRegHalArgs->pSub->Regs().Test32(pPollRegHalArgs->gpuRegValue,
                                                pPollRegHalArgs->indexes);
}

//------------------------------------------------------------------------------
RC GpuSubdevice::PollGpuRegValue
(
    ModsGpuRegValue value,
    UINT32 regIndex1,
    UINT32 regIndex2,
    FLOAT64 timeoutMs
)
{
    PollRegHalArgs pollArgs;
    pollArgs.pSub = this;
    pollArgs.gpuRegValue = value;
    pollArgs.indexes = { regIndex1, regIndex2 };
    return POLLWRAP_HW(PollRegHalValueFunc, &pollArgs, timeoutMs);
}

//------------------------------------------------------------------------------
RC GpuSubdevice::PollGpuRegValue
(
    ModsGpuRegValue value,
    UINT32 regIndex,
    FLOAT64 timeoutMs
)
{
    return PollGpuRegValue(value, regIndex, 0, timeoutMs);
}

//------------------------------------------------------------------------------
// Noop implementations for GPUs that don't support this.
UINT32 GpuSubdevice::GetCropWtThreshold() //WAR for bug 870748
{
    return Regs().Read32(MODS_PGRAPH_PRI_BES_CROP_DEBUG2_CC_HYBRID_WT_THRESHOLD);
}
void GpuSubdevice::SetCropWtThreshold(UINT32 thresh) //WAR for bug 870748
{
    Regs().Write32(MODS_PGRAPH_PRI_BES_CROP_DEBUG2_CC_HYBRID_WT_THRESHOLD, thresh);
}

/* virtual */ void GpuSubdevice::EnableBar1PhysMemAccess()
{
    Regs().Write32(MODS_PBUS_BAR1_BLOCK_MODE_PHYSICAL);
}

/*virtual*/ unique_ptr<ScopedRegister> GpuSubdevice::EnableScopedBar1PhysMemAccess()
{
    auto origReg = make_unique<ScopedRegister>(this, MODS_PBUS_BAR1_BLOCK);
    Regs().Write32(MODS_PBUS_BAR1_BLOCK_MODE_PHYSICAL);
    return origReg;
}

/* virtual */ unique_ptr<ScopedRegister> GpuSubdevice::ProgramScopedPraminWindow(UINT64 addr, Memory::Location loc)
{
    auto origReg = make_unique<ScopedRegister>(this, MODS_PBUS_BAR0_WINDOW);

    auto newVal = Regs().SetField(MODS_PBUS_BAR0_WINDOW_BASE, static_cast<UINT32>(addr >> 16));

    switch (loc)
    {
        case Memory::Fb:
            Regs().SetField(&newVal, MODS_PBUS_BAR0_WINDOW_TARGET_VID_MEM);
            break;
        case Memory::Coherent:
            Regs().SetField(&newVal, MODS_PBUS_BAR0_WINDOW_TARGET_SYS_MEM_COHERENT);
            break;
        case Memory::NonCoherent:
            Regs().SetField(&newVal, MODS_PBUS_BAR0_WINDOW_TARGET_SYS_MEM_NONCOHERENT);
            break;
        default:
            MASSERT(!"Invalid location!");
    }

    Regs().Write32(MODS_PBUS_BAR0_WINDOW, newVal);

    //    GpuSubdevice::ProgramPraminWindow(addr, loc);
    return origReg;
}

// Sets the CPU affinity for this GPU Subdevice for the current thread so
// that in a NUMA system the thread is on a CPU close to the GPU
RC GpuSubdevice::SetThreadCpuAffinity()
{
    RC rc;
    vector<UINT32> cpuMasks;
    auto pPcie = GetInterface<Pcie>();
    CHECK_RC(Xp::GetDeviceLocalCpus(pPcie->DomainNumber(),
                                    pPcie->BusNumber(),
                                    pPcie->DeviceNumber(),
                                    pPcie->FunctionNumber(),
                                    &cpuMasks));
    CHECK_RC(Tasker::SetCpuAffinity(Tasker::LwrrentThread(), cpuMasks));
    return rc;
}

INT32 GpuSubdevice::GetNumaNode() const
{
    auto pPcie = GetInterface<Pcie>();
    if (!pPcie)
    {
        return -1;
    }
    return Xp::GetDeviceNumaNode(pPcie->DomainNumber(),
                                 pPcie->BusNumber(),
                                 pPcie->DeviceNumber(),
                                 pPcie->FunctionNumber());
}

RC GpuSubdevice::GetGOMMode(UINT32 *pGomMode) const
{
    MASSERT(pGomMode != nullptr);
    RC rc;
    *pGomMode = 0;
    LW2080_CTRL_CMD_GPU_QUERY_OPERATION_MODE_PARAMS params = {0};

    rc = LwRmPtr()->ControlBySubdevice(this,
                    LW2080_CTRL_CMD_GPU_QUERY_OPERATION_MODE,
                    &params,
                    sizeof(params));

    if (rc == RC::LWRM_NOT_SUPPORTED)
    {
        Printf(Tee::PriLow, "Not able to detect GOM setting on this board\n");
        return OK;
    }

    *pGomMode = params.lwrrentOperationMode;
    return rc;
}

bool GpuSubdevice::IsDisplayFuseEnabled()
{
    return Regs().Test32(MODS_PTOP_FS_STATUS_DISPLAY_ENABLE);
}

Display* GpuSubdevice::GetDisplay()
{
    return GetParentDevice()->GetDisplay();
}

//------------------------------------------------------------------------------
bool GpuSubdevice::HasDisplay()
{
    LwRmPtr pLwRm;
    if (pLwRm->IsClassSupported(LW04_DISPLAY_COMMON, GetParentDevice()))
    {
        LW0073_CTRL_SYSTEM_GET_NUM_HEADS_PARAMS Params = {0};
        Params.subDeviceInstance = GetSubdeviceInst();
        RC rc = pLwRm->Control(pLwRm->GetSubdeviceHandle(this),
                LW0073_CTRL_CMD_SYSTEM_GET_NUM_HEADS,
                &Params, sizeof(Params));
        if (OK != rc)
        {
            Printf(Tee::PriLow, "Warning: Invalid RmCall to get "
                    "disp Heads. We can't surely say if display is disabled.Error code is %u\n", rc.Get());
        }
        else if (!Params.numHeads)
        {
            //Display is disabled
            return false;
        }
        else
        {
            return true;
        }
    }
    else
    {
        return false;
    }
    return IsDisplayFuseEnabled();
}

//------------------------------------------------------------------------------
bool GpuSubdevice::HasSORXBar()
{
    LwRmPtr pLwRm;
    if (!pLwRm->IsClassSupported(LW04_DISPLAY_COMMON, GetParentDevice()))
        // Play safe on errors, assume it is there:
        return true;

    // Assume the display class is not allocated yet, so we are unable to use
    // "Display::RmControl":
    LwRm::Handle class073Handle;
    if (OK != pLwRm->Alloc(
        pLwRm->GetDeviceHandle(GetParentDevice()),
        &class073Handle,
        LW04_DISPLAY_COMMON,
        NULL))
        return true;

    LW0073_CTRL_SYSTEM_GET_CAPS_V2_PARAMS systemCaps = {};
    RC rc = pLwRm->Control(class073Handle,
        LW0073_CTRL_CMD_SYSTEM_GET_CAPS_V2,
        &systemCaps, sizeof(systemCaps));

    pLwRm->Free(class073Handle);

    if (OK != rc)
    {
        return true;
    }

    return !!LW0073_CTRL_SYSTEM_GET_CAP(systemCaps.capsTbl,
        LW0073_CTRL_SYSTEM_CAPS_CROSS_BAR_SUPPORTED);
}

bool GpuSubdevice::FaultBufferOverflowed() const
{
    MASSERT(!"Unsupported function");
    return false;
}

//-----------------------------------------------------------------------------
//! \brief Ilwalidate the compression bit cache
/* virtual */ RC GpuSubdevice::IlwalidateCompBitCache(FLOAT64 timeoutMs)
{
    RC rc;
    const UINT32 ltcNum    = GetFB()->GetLtcCount();

    MASSERT(ltcNum > 0);

    Regs().Write32(MODS_PLTCG_LTCS_LTSS_CBC_CTRL_1_ILWALIDATE_ACTIVE);

    // Wait until comptaglines for all LTCs and slices are set
    for (UINT32 ltc = 0; ltc < ltcNum; ++ltc)
    {
        const UINT32 ltcSlices = GetFB()->GetSlicesPerLtc(ltc);
        MASSERT(ltcSlices > 0);

        for (UINT32 slice = 0; slice < ltcSlices; ++slice)
        {
            IlwalidateCompbitCachePollArgs args;
            args.gpuSubdevice = this;
            args.ltc = ltc;
            args.slice = slice;

            CHECK_RC(POLLWRAP_HW(&PollCompbitCacheSliceIlwalidated, &args,
                                 timeoutMs));
        }
    }

    return rc;
}

//-----------------------------------------------------------------------------
//! \brief Get the number of comptag lines per cache line.
/* virtual */ UINT32 GpuSubdevice::GetComptagLinesPerCacheLine()
{
    return Regs().Read32(MODS_PLTCG_LTCS_LTSS_CBC_PARAM_COMPTAGS_PER_CACHE_LINE);
}

//-----------------------------------------------------------------------------
//! \brief Get the compression bit cache line size.
/* virtual */ UINT32 GpuSubdevice::GetCompCacheLineSize()
{
    return 512 << Regs().Read32(MODS_PLTCG_LTCS_LTSS_CBC_PARAM_CACHE_LINE_SIZE);
}

//-----------------------------------------------------------------------------
//! \brief Get the compression bit cache base address
/* virtual */ UINT32 GpuSubdevice::GetCbcBaseAddress()
{
    return Regs().Read32(MODS_PLTCG_LTCS_LTSS_CBC_BASE_ADDRESS);
}

//-----------------------------------------------------------------------------
bool GpuSubdevice::IsFbBroken()
{
    return GetNamedPropertyValue("IsFbBroken");
}

//-----------------------------------------------------------------------------
bool GpuSubdevice::IsFakeBar1Enabled()
{
    return GetNamedPropertyValue("UseFakeBar1");
}

//-----------------------------------------------------------------------------
RC GpuSubdevice::PostRmInit()
{
    RC rc;

    // The JTAG registers aren't available for VF
    if (!IsSOC() && !Platform::IsVirtFunMode())
    {
        // Hulk has 2 modes:
        // 1. If embedded in vbios, it will run during devinit
        // 2. Otherwise RM call to init the device will apply the license
        // For the later case, we need to make this call here in order to apply
        // the jtag changes.(e.g if using -hulk_cert commandline)
        ApplyJtagChanges(GpuSubdevice::PostRMInit);
    }

    if (Platform::IsPhysFunMode())
    {
        auto pGpuPcie = GetInterface<Pcie>();
        MASSERT(pGpuPcie != nullptr);
        CHECK_RC(Platform::PciEnableSriov(
                        pGpuPcie->DomainNumber(), pGpuPcie->BusNumber(),
                        pGpuPcie->DeviceNumber(), pGpuPcie->FunctionNumber(),
                        VmiopElwManager::Instance()->GetVfCount()));
    }

    return OK;
}

/* virtual */ UINT32 GpuSubdevice::GetUserdAlignment() const
{
    return (1 << Regs().LookupAddress(MODS_RAMUSERD_BASE_SHIFT));
}

RC GpuSubdevice::FlushL2ProbeFilterCache()
{
    if (!Regs().IsSupported(MODS_PLTCG_LTCS_LTSS_PRBF_0))
        return RC::UNSUPPORTED_FUNCTION;

    MASSERT(m_Fb);
    MASSERT(m_pFsImpl);
    RC rc;

    Regs().Write32(MODS_PLTCG_LTCS_LTSS_PRBF_0_CLEAN_ACTIVE);

    for (UINT32 lwrLtc = 0; lwrLtc < m_Fb->GetLtcCount(); lwrLtc++)
    {
        const UINT32 ltcSlices = m_Fb->GetSlicesPerLtc(lwrLtc);
        MASSERT(ltcSlices > 0);
        for (UINT32 lwrSlice = 0; lwrSlice < ltcSlices; lwrSlice++)
        {
            MASSERT(Regs().IsSupported(MODS_PLTCG_LTCx_LTSy_PRBF_0,
                                       {lwrLtc, lwrSlice}));
            CHECK_RC(PollGpuRegValue(MODS_PLTCG_LTCx_LTSy_PRBF_0_CLEAN_INACTIVE,
                                     lwrLtc,
                                     lwrSlice,
                                     Tasker::GetDefaultTimeoutMs()));
        }
    }

    return rc;
}

/* virtual */ RC GpuSubdevice::GetVprSize(UINT64 *size, UINT64 *alignment)
{
    *size = 0;
    *alignment = 0;
    return OK;
}

/* virtual */ RC GpuSubdevice::GetAcr1Size(UINT64 *size, UINT64 *alignment)
{
    *size = 0;
    *alignment = 0;
    return OK;
}

/* virtual */ RC GpuSubdevice::GetAcr2Size(UINT64 *size, UINT64 *alignment)
{
    *size = 0;
    *alignment = 0;
    return OK;
}

/* virtual */ RC GpuSubdevice::SetVprRegisters(UINT64 size, UINT64 physAddr)
{
    return RC::UNSUPPORTED_FUNCTION;
}

/* virtual */ RC GpuSubdevice::RestoreVprRegisters()
{
    return SetVprRegisters(0, 0x1FFFFFFFFFULL);
}

/* virtual */ RC GpuSubdevice::ReadVprRegisters(UINT64 *size, UINT64 *physAddr)
{
    return RC::UNSUPPORTED_FUNCTION;
}

/* virtual */ RC GpuSubdevice::SetAcrRegisters
(
    UINT64 acr1Size,
    UINT64 acr1PhysAddr,
    UINT64 acr2Size,
    UINT64 acr2PhysAddr
)
{
    return RC::UNSUPPORTED_FUNCTION;
}

/* virtual */ bool GpuSubdevice::GetHBMTypeInfo(string *pVendor, string *pRevision) const
{
    MASSERT(pVendor);
    MASSERT(pRevision);

    if (m_HbmSitesInfo.empty())
    {
        Printf(Tee::PriDebug, "HBM Type Info not set!\n");
        return false;
    }

    // Grab info from any valid site
    HbmSiteInfo hbmSiteInfo = m_HbmSitesInfo.begin()->second;
    *pVendor = hbmSiteInfo.vendorName;
    *pRevision = hbmSiteInfo.revisionStr;

    return true;
}

/* virtual */ bool GpuSubdevice::GetHBMSiteInfo(const UINT32 siteID, HbmSiteInfo *pHbmSiteInfo)
{
    MASSERT(pHbmSiteInfo);

    auto search = m_HbmSitesInfo.find(siteID);
    if (search != m_HbmSitesInfo.end())
    {
        *pHbmSiteInfo = search->second;
        return true;
    }

    return false;
}

/* virtual */ RC GpuSubdevice::SetHBMSiteInfo(const UINT32 siteID, const HbmSiteInfo& hbmSiteInfo)
{
    RC rc;

    m_HbmSitesInfo[siteID] = hbmSiteInfo;

    return rc;
}

/* virtual */ void GpuSubdevice::SetHBMDeviceIdInfo(const HbmDeviceIdInfo& hbmDeviceIdInfo)
{
    m_HbmDeviceIdInfo = hbmDeviceIdInfo;
    m_IsHbmDeviceIdInfoValid = true;
}

/* virtual */ bool GpuSubdevice::GetHBMDeviceIdInfo(HbmDeviceIdInfo* pHbmDeviceIdInfo)
{
    if (m_IsHbmDeviceIdInfoValid)
    {
        *pHbmDeviceIdInfo = m_HbmDeviceIdInfo;
        return true;
    }
    Printf(Tee::PriDebug, "HBM Device ID Info was not initialized.\n");
    return false;
}

//!
//! \brief Reconfigure the GPU lanes to the HBM.
//!
//! \param laneError Lane to remap.
//!
/* virtual */ RC GpuSubdevice::ReconfigGpuHbmLanes
(
    const LaneError& laneError
)
{
    return ReconfigGpuHbmLanes(laneError, false/*no dword swizzle*/);
}

//!
//! \brief Reconfigure the GPU lanes to the HBM.
//!
//! \param laneError Lane to remap.
//! \param withDwordSwizzle If true, swizzle the DWORD within the subpartition.
//!
RC GpuSubdevice::ReconfigGpuHbmLanes
(
    const LaneError& laneError
    ,bool withDwordSwizzle
)
{
    RC rc;
    RegHal &regs = Regs();

    // Callwlate mask and remap values for remapping
    //
    UINT16 mask;
    UINT16 remapVal;
    CHECK_RC(HBMDevice::CalcLaneMaskAndRemapValue(laneError.laneType,
                                                  laneError.laneBit, &mask, &remapVal));

    // Reprogram the gpu lane configuration
    //
    UINT32 linkRepairRegAddr;
    CHECK_RC(GetHbmLinkRepairRegister(laneError, &linkRepairRegAddr));
    const UINT32 regValue = RegRd32(linkRepairRegAddr);
    UINT32 newRegValue = regValue;

    // Retain the existing remapping in the other byte pair
    if ((regValue & mask) != mask)
    {
        Printf(Tee::PriWarn,
               "Overwriting existing remap value in link repair register. Any "
               "previous remapping associated with the byte pair will be "
               "overridden with the new remap value\n");
    }
    remapVal = (remapVal & mask) | (regValue & ~mask);

    // Set remap value
    regs.SetField(&newRegValue,
                  MODS_PFB_FBPA_0_FBIO_HBM_LINK_REPAIR_CHANNELx_DWORDy_CODE,
                  remapVal);

    // Set the bypass bit
    regs.SetField(&newRegValue,
                  MODS_PFB_FBPA_0_FBIO_HBM_LINK_REPAIR_CHANNELx_DWORDy_BYPASS,
                  1);

    // Write link repair register value
    RegWr32(linkRepairRegAddr, newRegValue);

    Printf(Tee::PriLow, "Reconfigure GPU lanes: %s - linkRepairReg(0x%08x), "
           "remapVal(old(0x%08x), new(0x%08x))\n", laneError.ToString().c_str(),
           linkRepairRegAddr, regValue, newRegValue);

    return rc;
}

//-----------------------------------------------------------------------------
/* virtual */ RC GpuSubdevice::GetHBMSiteMasterFbpaNumber
(
    const UINT32 siteID
    ,UINT32* const pFbpaNum
) const
{
    Printf(Tee::PriError,
           "GetHBMSiteMasterFbpaNumber called without being overridden. "
           "Override in gpu subdevices that support HBM.\n");
    return RC::UNSUPPORTED_FUNCTION;
}

//-----------------------------------------------------------------------------
/* virtual */ RC GpuSubdevice::GetHBMSiteFbps
(
    const UINT32 siteID,
    UINT32* const pFbpNum1,
    UINT32* const pFbpNum2
) const
{
    Printf(Tee::PriError, "HBM FBP info not available for this GPU subdevice\n");
    return RC::UNSUPPORTED_FUNCTION;
}

//-----------------------------------------------------------------------------
//!
//! \brief Colwerts the channel to the GPU HBM Link Repair register channel.
//!
//! Accepts arguments in terms of the FBPA.
//!
//! The MODS_PFB_FBPA_0_FBIO_HBM_LINK_REPAIR_CHANNELx_DWORDy registers (for some
//! x,y,z) refer to a 'channel', which is not the same as the HBM channel.
//!
UINT32 GpuSubdevice::HbmChannelToGpuHbmLinkRepairChannel
(
    UINT32 hbmChannel
) const
{
    return hbmChannel % 2;
}

//-----------------------------------------------------------------------------
//!
//! \brief Get the address of the HBM Link Repair Register corresponding to the
//! given arguments.
//!
//! Some GPUs have DWORD swizzling between the FBPA and the HBM which needs to
//! be accounted for on a per GPU basis.
//!
RC GpuSubdevice::GetHbmLinkRepairRegister
(
    const LaneError& laneError
    ,UINT32* pOutRegAddr
    ,bool withDwordSwizzle
) const
{
    RC rc;

    const FrameBuffer* const pFb = GetFB();
    const UINT32 subpSize = pFb->GetSubpartitionBusWidth();
    const UINT32 dwordSize = 32;
    const UINT32 dwordsPerSubp = subpSize / dwordSize;
    MASSERT(dwordsPerSubp > 0);
    // Get the DWORD
    //
    UINT32 hbmDword = (laneError.laneBit % subpSize) / dwordSize;
    if (withDwordSwizzle)
    {
        hbmDword = (dwordsPerSubp - 1) - hbmDword; // -1 since dwords are 0-indexed
    }

    // Get the HBM Link Repair Register channel
    //
    const UINT32 subp = laneError.laneBit / subpSize;
    const UINT32 hwFbio = pFb->HwFbpaToHwFbio(laneError.hwFbpa.Number());
    UINT32 hbmSite;
    UINT32 hbmChannel;
    CHECK_RC(pFb->FbioSubpToHbmSiteChannel(hwFbio, subp, &hbmSite, &hbmChannel));
    const UINT32 linkChannel = HbmChannelToGpuHbmLinkRepairChannel(hbmChannel);

    // Get the register address
    //
    *pOutRegAddr =
        Regs().LookupAddress(MODS_PFB_FBPA_0_FBIO_HBM_LINK_REPAIR_CHANNELx_DWORDy,
                             { laneError.hwFbpa.Number(), linkChannel, hbmDword });

    return rc;
}

RC GpuSubdevice::EmuMemTrigger()
{
    if (m_DoEmuMemTrigger)
    {
        static bool alreadyTriggered = false;
        if (alreadyTriggered)
        {
            Printf(Tee::PriWarn, "Emulation Memeory Trigger already happened, ignoring...\n");
        }
        else
        {
            Regs().Write32(MODS_PFB_FBPA_ECC_ERR_INJECTION_ADDR, 0, 0x1);
            alreadyTriggered = true;
        }
    }

    return OK;
}

/* virtual */
RC GpuSubdevice::GetKFuseStatus(KFuseStatus *pStatus, FLOAT64 TimeoutMs)
{
    if (!IsFeatureEnabled(GPUSUB_HAS_KFUSES))
        return RC::UNSUPPORTED_HARDWARE_FEATURE;

    MASSERT(pStatus);
    RC rc;

    // Part 1: Kick off the KFuse state machine
    // incomplete KFuse manual:
    // LW_KFUSE_STATE_RESET value (0 / 1) is not clearly defined
    Regs().Write32(MODS_KFUSE_STATE, Regs().SetField(MODS_KFUSE_STATE_RESET, 1));

    // Wait for state machine to finish
    CHECK_RC(POLLWRAP_HW(PollKFusesStateDone, this, TimeoutMs));

    // Part 2: Make sure CRC passes
    pStatus->CrcPass = Regs().Test32(MODS_KFUSE_STATE_CRCPASS, 1);

    UINT32 ErrorCount   = Regs().Read32(MODS_KFUSE_ERRCOUNT);
    pStatus->Error1     = Regs().GetField(ErrorCount, MODS_KFUSE_ERRCOUNT_ERR_1);
    pStatus->Error2     = Regs().GetField(ErrorCount, MODS_KFUSE_ERRCOUNT_ERR_2);
    pStatus->Error3     = Regs().GetField(ErrorCount, MODS_KFUSE_ERRCOUNT_ERR_3);
    pStatus->FatalError = Regs().GetField(ErrorCount, MODS_KFUSE_ERRCOUNT_ERR_FATAL);

    // Part 3: Read the HDCP Keys. Set the key addr to autoincrement mode
    ScopedRegister OrgVal(this, Regs().LookupAddress(MODS_KFUSE_KEYADDR));
    Regs().Write32(MODS_KFUSE_KEYADDR, Regs().SetField(MODS_KFUSE_KEYADDR_AUTOINC, 1));
    const UINT32 NUM_KEYS = 144;
    pStatus->Keys.clear();
    for (UINT32 i = 0; i < NUM_KEYS; i++)
    {
        UINT32 NewKey = Regs().Read32(MODS_KFUSE_KEYS);
        pStatus->Keys.push_back(NewKey);
    }

    return rc;
}

/* virtual */
UINT32 GpuSubdevice::GetZlwllPixelsCovered() const
{
    return Regs().LookupAddress(MODS_PGPC_ZLWLL_PIXELS_COVERED);
}

/* virtual */
UINT32 GpuSubdevice::GetZlwllNumZFar() const
{
    return Regs().LookupAddress(MODS_PGPC_ZLWLL_NUM_ZFAR);
}

/* virtual */
UINT32 GpuSubdevice::GetZlwllNumZNear() const
{
    return Regs().LookupAddress(MODS_PGPC_ZLWLL_NUM_ZNEAR);
}

//-----------------------------------------------------------------------------
//! emulation and RTL virtual performance monitor speedup
void GpuSubdevice::StopVpmCapture()
{
    Regs().Write32(MODS_PERF_PMASYS_CONTROL_STOPVPMCAPTURE_DOIT);

    Printf(Tee::PriNormal,
           "VPM Capture stopped (LW_PERF_PMASYS_CONTROL=0x%x).\n",
           Regs().Read32(MODS_PERF_PMASYS_CONTROL));
}

RC GpuSubdevice::ValidatePreInitRegisters()
{
    RC rc;

    // get the gpc tile map option
    string GpcTileMap = GetFermiGpcTileMap();

    // there are no options specified to setup a gpc tile map
    if (GpcTileMap.empty())
        return rc;

    Printf(Tee::PriDebug, "GpuSubdevice::ReadGpcTileMapArguments"
           " Arguments: %s\n", GpcTileMap.c_str());

    vector<unsigned int> GpcTileMapVector;
    string::size_type copyStart = GpcTileMap.find_first_not_of(':');
    string::size_type copyEnd = GpcTileMap.find_first_of(':', copyStart);
    char *ep = 0;
    unsigned int mapval;
    UINT32 mapEntries = 0;

    while (copyStart != copyEnd)
    {
        if (copyEnd == string::npos)
        {
            const string mapvalString = GpcTileMap.substr(copyStart);
            mapval = std::strtoul(mapvalString.c_str(), &ep, 10);
            mapEntries++;
            GpcTileMapVector.push_back(mapval);
            Printf(Tee::PriDebug, "GpuSubdevice::ReadGpcTileMapArguments"
                   " Map Entry: %d\n", mapval);

            copyStart = copyEnd;
        }
        else
        {
            const string mapvalString = GpcTileMap.substr(copyStart,
                                                          copyEnd - copyStart);
            mapval = std::strtoul(mapvalString.c_str(), &ep, 10);
            mapEntries++;
            GpcTileMapVector.push_back(mapval);
            Printf(Tee::PriDebug,
                   "GpuSubdevice::ReadGpcTileMapArguments"
                   " Map Entry: %d\n", mapval);

            copyStart = GpcTileMap.find_first_not_of(':', copyEnd);
            copyEnd = GpcTileMap.find_first_of(':', copyStart);
        }
    }

    Printf(Tee::PriDebug, "GpuSubdevice::ReadGpcTileMapArguments"
           " Total Map Entries: %u\n", mapEntries);

    // user didn't specify any map entries, just return as no more to do
    if (mapEntries == 0)
        return rc;

    if (mapEntries >= LW2080_CTRL_GR_SET_GPC_TILE_MAP_MAX_VALUES)
    {
        Printf(Tee::PriError,
               "GpuSubdevice::ReadGpcTileMapArguments"
               " Total Map Entries: %u >= control call max = %u\n",
               mapEntries, LW2080_CTRL_GR_SET_GPC_TILE_MAP_MAX_VALUES);
        return RC::BAD_COMMAND_LINE_ARGUMENT;
    }

    vector<unsigned int>::const_iterator uintData;

    LW2080_CTRL_GR_SET_GPC_TILE_MAP_PARAMS gpctilemapParams = { };

    gpctilemapParams.mapValueCount = mapEntries;
    int i;

    for (uintData = GpcTileMapVector.begin(), i = 0;
         uintData != GpcTileMapVector.end();
         ++uintData, ++i)
    {
        gpctilemapParams.mapValues[i] = (UINT08)(*uintData & 0xFF);
        Printf(Tee::PriDebug,
               "GpuSubdevice::ReadGpcTileMapArguments"
               " Packed Map Entry in Vector: 0x%02x\n",
               gpctilemapParams.mapValues[i]);
    }

    if (IsSMCEnabled())
    {
        for (auto const & smcEngineId : GetSmcEngineIds())
        {
            gpctilemapParams.grRouteInfo = { 0 };
            SetGrRouteInfo(&(gpctilemapParams.grRouteInfo), smcEngineId);
            rc = LwRmPtr()->ControlBySubdevice(this,
                                       LW2080_CTRL_CMD_GR_SET_GPC_TILE_MAP,
                                       &gpctilemapParams,
                                       sizeof (gpctilemapParams));
            if (OK != rc)
            {
                Printf(Tee::PriDebug, "GpuSubdevice::ReadGpcTileMapArguments"
                       " Error: LW2080_CTRL_CMD_GR_SET_GPC_TILE_MAP control call failed"
                       " for swizzId %d and smcEngineId %d\n", GetSwizzId(), smcEngineId);
                return rc;
            }
        }
    }
    else
    {
        rc = LwRmPtr()->ControlBySubdevice(this,
                                   LW2080_CTRL_CMD_GR_SET_GPC_TILE_MAP,
                                   &gpctilemapParams,
                                   sizeof (gpctilemapParams));
        if (OK != rc)
        {
            Printf(Tee::PriDebug, "GpuSubdevice::ReadGpcTileMapArguments"
                   " Error: LW2080_CTRL_CMD_GR_SET_GPC_TILE_MAP control call failed\n");
        }
    }

    return rc;
}

//--------------------------------------------------------------------
//! \brief Tell whether Azalia must be enabled on a board
//!
//! This method is used to sanity-check the results of HasFeature()
//! and IsFeatureEnabled() on GPUSUB_HAS_AZALIA_CONTROLLER.
//!
/* virtual */ bool GpuSubdevice::IsAzaliaMandatory() const
{
    //Azalia should not be mandatory if user intentionally disabled it
    if (GetSkipAzaliaInit())
    {
        return false;
    }

    // Azalia is often not supported in simulation, so consider it
    // optional
    //
    if (Platform::GetSimulationMode() != Platform::Hardware)
    {
        return false;
    }

    // There might be no Azalia on emulation as well
    //
    if (IsEmulation())
    {
        return false;
    }

    // SOC GPUs don't have Azalia (Azalia is outside of the GPU)
    //
    if (IsSOC())
    {
        return false;
    }

    // WAR for bug 1322069: Azalia is frequently disabled on Optimus
    // parts, even when the RM says it supports HDMI.  Marking Azalia
    // as optional on Optimus to get around that.  Remove this section
    // if we find a better solution for Optimus.
    //
    auto pGpuPcie = GetInterface<Pcie>();
    unique_ptr<Xp::OptimusMgr> pOptimusMgr(Xp::GetOptimusMgr(
                                                pGpuPcie->DomainNumber(),
                                                pGpuPcie->BusNumber(),
                                                pGpuPcie->DeviceNumber(),
                                                pGpuPcie->FunctionNumber()));
    if (pOptimusMgr.get())
    {
        return false;
    }

    // Otherwise, Azalia is mandatory if the board supports HDMI
    // and/or DisplayPort.
    //
    Display *pDisplay = GetParentDevice()->GetDisplay();

    if (pDisplay == nullptr)
        return false;

    const UINT32 subdeviceInstance = GetSubdeviceInst();
    UINT32 connectors = 0;
    pDisplay->GetConnectors(subdeviceInstance, &connectors);

    UINT32 virtualConnectors = 0;
    pDisplay->GetVirtualDisplays(subdeviceInstance, &virtualConnectors);

    UINT32 actualConnectors = connectors & ~virtualConnectors;

    for (INT32 bit = Utility::BitScanForward(actualConnectors);
         bit >= 0; bit = Utility::BitScanForward(actualConnectors, bit + 1))
    {
        LW0073_CTRL_DFP_GET_INFO_PARAMS dfpInfo = { };
        dfpInfo.subDeviceInstance = subdeviceInstance;
        dfpInfo.displayId = 1 << bit;
        pDisplay->RmControl(LW0073_CTRL_CMD_DFP_GET_INFO,
                            &dfpInfo, sizeof(dfpInfo));
        if (FLD_TEST_DRF(0073, _CTRL_DFP_FLAGS, _HDMI_CAPABLE, _TRUE,
                         dfpInfo.flags))
        {
            return true;
        }
        if (FLD_TEST_DRF(0073, _CTRL_DFP_FLAGS, _SIGNAL, _DISPLAYPORT,
                         dfpInfo.flags) &&
           FLD_TEST_DRF(0073, _CTRL_DFP_FLAGS, _EMBEDDED_DISPLAYPORT, _FALSE,
                        dfpInfo.flags))
        {
            return true;
        }
    }

    return false;
}

RC GpuSubdevice::GetAteIddq(UINT32 *pIddq, UINT32 *pVersion)
{
    unique_ptr<ScopedRegister> debug1;
    if (Regs().IsSupported(MODS_PTOP_DEBUG_1_FUSES_VISIBLE))
    {
        debug1 = make_unique<ScopedRegister>(this, Regs().LookupAddress(MODS_PTOP_DEBUG_1));
        Regs().Write32(MODS_PTOP_DEBUG_1_FUSES_VISIBLE_ENABLED);
    }

    UINT32 IddqData = Regs().Read32(MODS_FUSE_OPT_IDDQ_DATA);
    UINT32 IddqValid   = Regs().Read32(MODS_FUSE_OPT_IDDQ_VALID);
    UINT32 PdelayValid = Regs().Read32(MODS_FUSE_OPT_PDELAY_VALID);

    RC rc;
    MASSERT(pIddq);
    MASSERT(pVersion);
    if ((IddqValid == 0) && (PdelayValid == 0))
    {
        rc = RC::UNSUPPORTED_FUNCTION;
    }
    else if ((IddqValid == 1) && (PdelayValid == 1))
    {
        *pVersion = 1;
    }
    // these are spelwlated... not 100% sure whether ATE will follow this:
    else if ((IddqValid == 1) && (PdelayValid == 0))
    {
        *pVersion = 2;
    }
    else if ((IddqValid == 0) && (PdelayValid == 1))
    {
        *pVersion = 3;
    }
    else
    {
        Printf(Tee::PriNormal, "Unknown Iddq version\n");
        rc = RC::UNSUPPORTED_FUNCTION;
    }

    *pIddq = IddqData * 50;

    return rc;
}

UINT64 GpuSubdevice::FramebufferBarSize() const
{
    UINT32 strapFB = Regs().Read32(MODS_PEXTDEV_BOOT_0_STRAP_FB);
    UINT64 fbBarSize = 64;

    if (strapFB <= Regs().LookupValue(MODS_PEXTDEV_BOOT_0_STRAP_FB_128G))
    {
        return fbBarSize << strapFB;
    }

    MASSERT(!"Invalid STRAP_FB value");
    return 0;
}

//------------------------------------------------------------------------------
/* virtual */
RC GpuSubdevice::GetPdi(UINT64* pPdi)
{
    MASSERT(pPdi);
    RC rc;

    // TU102+
    vector<ModsGpuRegAddress> pdiRegs =
    {
        MODS_FUSE_OPT_PDI_1,
        MODS_FUSE_OPT_PDI_0
    };

    // GP100 - TU000
    vector<ModsGpuRegAddress> pdiPrivRegs =
    {
        MODS_FUSE_OPT_PRIV_PDI_1,
        MODS_FUSE_OPT_PRIV_PDI_0
    };

    vector<vector<ModsGpuRegAddress>> possibleRegs =
    {
        pdiRegs,
        pdiPrivRegs
    };

    for (auto regs: possibleRegs)
    {
        if (Regs().IsSupported(regs[0]) && Regs().IsSupported(regs[1]))
        {
            for (auto reg: regs)
            {
                UINT32 pdiValue;
                CHECK_RC(Regs().Read32Priv(reg, &pdiValue));
                *pPdi = (*pPdi << 32) | pdiValue;
            }
            return OK;
        }
    }

    return RC::UNSUPPORTED_FUNCTION;;
}

//------------------------------------------------------------------------------
/* virtual */
RC GpuSubdevice::GetRawEcid(vector<UINT32>* pEcid, bool bReverseFields)
{
    return GetGF100RawEcid(this, pEcid, bReverseFields);
}

//------------------------------------------------------------------------------
/* virtual */
RC GpuSubdevice::GetEcid(string* pEcid)
{
    return GetGF100CookedEcid(this, pEcid);
}

//-------------------------------------------------------------------------------
/* static */ bool GpuSubdevice::PollMmeWriteComplete(void *pvArgs)
{
    GpuSubdevice *pSubdev = static_cast<GpuSubdevice *>(pvArgs);
    return pSubdev->Regs().Test32(
            MODS_PGRAPH_PRI_MME_SHADOW_RAM_INDEX_WRITE_DONE);
}

//-------------------------------------------------------------------------------
/* static */ bool GpuSubdevice::PollMmeReadComplete(void *pvArgs)
{
    GpuSubdevice *pSubdev = static_cast<GpuSubdevice *>(pvArgs);
    return pSubdev->Regs().Test32(
            MODS_PGRAPH_PRI_MME_SHADOW_RAM_INDEX_READ_DONE);
}

//! \brief Setup/clear bugs for Kepler
//------------------------------------------------------------------------------
void GpuSubdevice::SetupBugsKepler()
{
    if (IsInitialized())
    {
        // Setup bugs that require the system to be properly initialized
        // (i.e. LwRm can be accessed, all GpuSubdevice data setup, registers
        // can be accessed, etc.)

        // Disallow inheriting bugs Fermi->Kepler.
        // Bugs for both should be listed also in SetupBugsFermi.
        MASSERT(GpuDevice::Fermi != GetParentDevice()->GetFamily());
        // Note: use != Fermi so this assertion doesn't trigger on
        // partial-chip sims that lack 3d engines.
        // GetFamily checks which 3d engine is supported.

        // This bug is for the vttgen misc1_pad which has hardcoded values for
        // all but chip gf106.
        SetHasBug(548966);

        // Only check pull-up resistors on first 4 ports
        SetHasBug(997557);

    }
    else
    {
        // Setup bugs that do not require the system to be properly initialized

        // G80 and above should not touch VGA registers on simulation
        // (TODO : This was fixed...where?)
        SetHasBug(143732);

        // Bug 321377 : G82+ cannot set PCLK directly
        SetHasBug(321377);

        //Pri ring gets broken if device in daisy chain gets floorswept.
        //Speedos can't be read in all partitions because of this.
        SetHasBug(598576);

        // Fuse reads are inconsistent. Sometimes get back 0xFFFFFFFF. Need fuse
        // related code to make sure we have consistent reads first
        SetHasBug(622250);

        // In 8+24 VCAA mode one fine-raster transaction carries 1x2 pixels
        // worth of data. So to generate one quad (2X2 pixels) for SM, PROP
        // needs to stitch 2 transaction together.
        // TC might run out of it's resources when it sees the first
        // transaction of the quad, and it may force tile_end and flush the
        // tile with only first half of the quad.
        // That would cause PROP to break quad into 2 transactions with 1x2
        // pixels covered in each and they both will end-up in different
        // subtiles.
        SetHasBug(618730);

        // TXD instructions need inputs for partial derivitives sanitized on
        // non-pixel shaders
        SetHasBug(630560);

        // PCIE correctable errors during link speed toggle
        SetHasBug(361633);

        // Report pass if selfgild fail but crc lead;
        SetHasBug(849226);

        //Bug in RM PMU-DeepIdle programming. Applicable on all kepler gpus
        //This forces mods to read deep idle stats at pstate 0
        //This also forces mods to enable deepl1 interrupt.
        if (!IsSOC() || Platform::HasClientSideResman())
        {
            SetHasBug(1042805);
        }

        // RM changes to Gen1 speed when performing voltage changes
        SetHasBug(849638);

        // Bug 853035: The ISM controller increments the duration by 1.
        // So the duration value passed to it needs to be decremented by 1.
        SetHasBug(853035);

        SetHasBug(998963); // Need to rearm MSI after clearing the interrupt.
    }
}

void GpuSubdevice::PrintFermiMonitorInfo
(
    bool HasPbdmaStatus,
    UINT32 PfifoEngineStatusSize,
    UINT32 PfifoEngineChannelSize,
    UINT32 PbdmaChannelSize
)
{
    RegHal &regs = Regs();
    vector<UINT32> Engines;
    GetParentDevice()->GetEngines(&Engines);
    bool HasCe1 = (Engines.end() != find(Engines.begin(), Engines.end(),
                                         (UINT32)LW2080_ENGINE_TYPE_COPY1));
    UINT32 PbDmaStatus  = HasPbdmaStatus ? regs.Read32(MODS_PFIFO_PBDMA_STATUS) : 0;
    UINT32 PgraphStatus = regs.Read32(MODS_PGRAPH_STATUS);
    UINT32 MsvldStatus  = regs.Read32(MODS_PMSVLD_FALCON_IDLESTATE);
    UINT32 MspdecStatus = regs.Read32(MODS_PMSPDEC_FALCON_IDLESTATE);
    UINT32 MspppStatus  = regs.Read32(MODS_PMSPPP_FALCON_IDLESTATE);
    UINT32 Ce0Status    = regs.Read32(MODS_PCE0_FALCON_IDLESTATE);
    UINT32 Ce1Status    = HasCe1 ? regs.Read32(MODS_PCE1_FALCON_IDLESTATE) : 0;

    Printf(Tee::PriNormal, "Status:");
    if (HasPbdmaStatus)
        Printf(Tee::PriNormal, " PFIFO_PBDMA=0x%08x", PbDmaStatus);
    Printf(Tee::PriNormal, " PGRAPH=0x%08x",      PgraphStatus);
    Printf(Tee::PriNormal, " MSVLD=0x%08x",       MsvldStatus);
    Printf(Tee::PriNormal, " MSPDEC=0x%08x",      MspdecStatus);
    Printf(Tee::PriNormal, " MSPPP=0x%08x",       MspppStatus);
    Printf(Tee::PriNormal, " CE0=0x%08x",         Ce0Status);
    if (HasCe1)
        Printf(Tee::PriNormal, " CE1=0x%08x",         Ce1Status);
    Printf(Tee::PriNormal, "\n");

    Printf(Tee::PriNormal, "LW_PFIFO_ENGINE_STATUS : ");
    for (UINT32 i =0; i < PfifoEngineStatusSize; ++i)
    {
        Printf(Tee::PriNormal, "0x%08x ", Regs().Read32(MODS_PFIFO_ENGINE_STATUS, i));
    }
    Printf(Tee::PriNormal, "\n");

    Printf(Tee::PriNormal, "LW_PFIFO_ENGINE_CHANNEL: ");
    for (UINT32 i =0; i < PfifoEngineChannelSize; ++i)
    {
        Printf(Tee::PriNormal, "0x%08x ", Regs().Read32(MODS_PFIFO_ENGINE_CHANNEL, i));
    }
    Printf(Tee::PriNormal, "\n");

    const UINT32 reg_size = 6;
    UINT32 reg[reg_size];
    char label[16];
    fill(reg, reg + reg_size, 0);
    for (UINT32 i = 0; i < PbdmaChannelSize; ++i)
    {
        sprintf(label, "PPBDMA(%d):", i);
        reg[0] = regs.Read32(MODS_PPBDMA_CHANNEL, i);
        reg[1] = regs.Read32(MODS_PPBDMA_PB_HEADER, i);
        reg[2] = regs.Read32(MODS_PPBDMA_GP_GET, i);
        reg[3] = regs.Read32(MODS_PPBDMA_GP_PUT, i);
        reg[4] = regs.Read32(MODS_PPBDMA_GET, i);
        reg[5] = regs.Read32(MODS_PPBDMA_PUT, i);
        if (0 != std::accumulate(reg, reg + reg_size, 0, logical_or<UINT32>()))
        {
            Printf(Tee::PriNormal,
                   "%10s CHANNEL=0x%08x PB_HEADER=0x%08x"
                   " GP_GET=0x%08x GP_PUT=0x%08x"
                   " GET=0x%08x PUT=0x%08x\n",
                    label, reg[0], reg[1], reg[2], reg[3], reg[4], reg[5]);
            fill(reg, reg + reg_size, 0);
            label[0] = 0;
        }

        reg[0] = regs.Read32(MODS_PPBDMA_CONTROL, i);
        reg[1] = regs.Read32(MODS_PPBDMA_PB_COUNT, i);
        reg[2] = regs.Read32(MODS_PPBDMA_PB_FETCH, i);
        reg[3] = regs.Read32(MODS_PPBDMA_PB_HEADER, i);
        reg[4] = regs.Read32(MODS_PPBDMA_INTR_0, i);
        if (0 != std::accumulate(reg, reg + reg_size, 0, logical_or<UINT32>()))
        {
            Printf(Tee::PriNormal,
                   "%10s CONTROL=0x%08x PB_COUNT=0x%08x PB_FETCH=0x%08x"
                   " PB_HEADER=0x%08x INTR_0=0x%08x\n",
                    label, reg[0], reg[1], reg[2], reg[3], reg[4]);
            fill(reg, reg + reg_size, 0);
            label[0] = 0;
        }

        reg[0] = regs.Read32(MODS_PPBDMA_METHOD0, i);
        reg[1] = regs.Read32(MODS_PPBDMA_DATA0, i);
        reg[2] = regs.Read32(MODS_PPBDMA_METHOD1, i);
        reg[3] = regs.Read32(MODS_PPBDMA_DATA1, i);
        if (0 != std::accumulate(reg, reg + reg_size, 0, logical_or<UINT32>()))
        {
            Printf(Tee::PriNormal,
                   "%10s METHOD0=0x%08x DATA0=0x%08x METHOD1=0x%08x DATA1=0x%08x\n",
                    label, reg[0], reg[1], reg[2], reg[3]);
            fill(reg, reg + reg_size, 0);
            label[0] = 0;
        }
    }
}

/* virtual */
void GpuSubdevice::ApplyMemCfgProperties(bool inPreVBIOSSetup)
{
    ApplyMemCfgProperties_Impl(inPreVBIOSSetup, 0, 0, false);
}

//--------------------------------------------------------------------
// The algorithm for applying MemCfg properties is used on multiple
// GPUs, however on some GPUs the actual register addresses have
// changed. We use pRegs to list the actual register addresses in this
// case.
void GpuSubdevice::ApplyMemCfgProperties_Impl
(
    bool InPreVBIOSSetup,
    UINT32 extraPfbCfg0,
    UINT32 extraPfbCfg0Mask,
    bool ReqFilterOverride
)
{
    JavaScriptPtr pJs;
    UINT32 FbBankSize;
    UINT32 FbBankMapPattern;
    UINT32 FbBanks;
    UINT32 FbBurstLength;
    UINT32 FbColumnBits;
    UINT32 FbExtBank;
    UINT32 FbRowBits;
    UINT32 FbRandomBankSwizzle;
    string FbCmdMapping;

    // If user has given args to mods on the matter the fb paramters can be
    // overridden in some cases.  Primary cards cannot have their POST
    // intercepted.  Hence no "PreVBIOSSetup" applies.  Secondary cards however
    // always call here to run before DevinitInitializeDevice.  In this case
    // we can override what the BIOS straps/tables/scriptery would otherwise have
    // done.
    //
    // PreVBIOSSetup runs just before RM's DevinitInitializeDevice.  If
    // DevinitInitializeDevice isn't going to be called right afterward, then
    // we shouldn't ever get here.  Check that if things get weird.
    //

    // These properties need to be overridden very early, before the RM has
    // initialized, and cannot be changed dynamically later.
    pJs->GetProperty(Gpu_Object, Gpu_FbBankMapPattern, &FbBankMapPattern);
    pJs->GetProperty(Gpu_Object, Gpu_FbBankSize, &FbBankSize);
    pJs->GetProperty(Gpu_Object, Gpu_FbBanks, &FbBanks);
    pJs->GetProperty(Gpu_Object, Gpu_FbBurstLength, &FbBurstLength);
    pJs->GetProperty(Gpu_Object, Gpu_FbColumnBits, &FbColumnBits);
    pJs->GetProperty(Gpu_Object, Gpu_FbExtBank, &FbExtBank);
    pJs->GetProperty(Gpu_Object, Gpu_FbRowBits, &FbRowBits);
    pJs->GetProperty(Gpu_Object, Gpu_FbCmdMapping, &FbCmdMapping);
    pJs->GetProperty(Gpu_Object, Gpu_FbRandomBankSwizzle, &FbRandomBankSwizzle);

    bool bFbOverride = FbBankMapPattern || FbBankSize || FbBanks ||
                       FbBurstLength || FbColumnBits || FbExtBank ||
                       FbRowBits || (FbCmdMapping != "0") || GetFbMs01Mode() ||
                       GetFbGddr5x16Mode() || extraPfbCfg0Mask;

    bool FbBankSwizzleParam = GetFbBankSwizzleIsSet();
    UINT32 bankSwizzle = GetFbBankSwizzle();

    if (FbBankSwizzleParam && !m_SavedBankSwizzle)
    {
        m_OrigBankSwizzle = Regs().Read32(MODS_PFB_FBPA_BANK_SWIZZLE);
        m_SavedBankSwizzle = true;
    }

    // Disable or enable DBI
    //
    if (!InPreVBIOSSetup)
    {
        bool disableDbi = false;
        bool enableDbi = false;
        pJs->GetProperty(Gpu_Object, Gpu_DisableDbi, &disableDbi);
        pJs->GetProperty(Gpu_Object, Gpu_EnableDbi, &enableDbi);
        if (disableDbi)
        {
            MASSERT(!enableDbi);
            DisableDbi();
        }
        else if (enableDbi)
        {
            EnableDbi();
        }
    }

    // On hardware or emulation, these are real DRAMs, and you can't just tweak
    // the settings willy-nilly.  You need to use an appropriate VBIOS instead.
    if (bFbOverride && Platform::GetSimulationMode() == Platform::Hardware)
    {
        Printf(Tee::PriError,
               "Cannot override FB settings on emulation/hardware\n");
        MASSERT(!"Generic assertion failure<refer to previous message>.");
        return;
    }

    // FbBankSwizzle *is* something you can set on real HW, however...
    // so that arg is special-cased below.
    //
    // It is expected that if any of the gpu subdevices call this function with
    // ReqFilterOverride set to true that they have already checked that the
    // parameters that require the filter overrides are valid if running on
    // hardware
    if (!(bFbOverride || FbRandomBankSwizzle || FbBankSwizzleParam || ReqFilterOverride))
    {
        return;
    }

    UINT32 PfbCfg0          = 0;
    UINT32 PfbCfg0Mask      = 0;
    UINT32 PfbCfg1          = 0;
    UINT32 PfbCfg1Mask      = 0;
    UINT32 PfbReadFtCfg     = 0;
    UINT32 PfbReadFtCfgMask = 0;
    UINT32 PfbFbioBcast     = 0;
    UINT32 PfbFbioBcastMask = 0;

    if (!InPreVBIOSSetup)
    {
        PfbCfg0      = Regs().Read32(MODS_PFB_FBPA_CFG0);
        PfbCfg1      = Regs().Read32(MODS_PFB_FBPA_CFG1);
        PfbReadFtCfg = Regs().Read32(MODS_PFB_FBPA_READ_FT_CFG);
        PfbFbioBcast = Regs().Read32(MODS_PFB_FBPA_FBIO_BROADCAST);
    }
    const UINT32 defPfbCfg0      = PfbCfg0;
    const UINT32 defPfbCfg1      = PfbCfg1;
    const UINT32 defPfbReadFtCfg = PfbReadFtCfg;
    const UINT32 defPfbFbioBcast = PfbFbioBcast;

    if (FbCmdMapping != "0")
    {
        if (!ApplyFbCmdMapping(FbCmdMapping, &PfbFbioBcast, &PfbFbioBcastMask))
        {
            Printf(Tee::PriError,
                   "Invalid setting for Gpu.FbCmdMapping: %s\n",
                   FbCmdMapping.c_str());
            MASSERT(!"Generic assertion failure<refer to previous message>.");
        }
    }

    if (FbBankMapPattern != 0)
    {
        // We left the arg scaffolding in because it might come back
        // later, but not actually supported any longer.
        Printf(Tee::PriError,
               "Gpu.FbBankMapPattern not supported on this arch!\n");
        MASSERT(!"Generic assertion failure<refer to previous message>.");
    }

    if (FbBanks != 0)
    {
        switch (FbBanks)
        {
            case 4:
                Regs().SetField(&PfbCfg1, MODS_PFB_FBPA_CFG1_BANK_2);
                break;
            case 8:
                Regs().SetField(&PfbCfg1, MODS_PFB_FBPA_CFG1_BANK_3);
                break;
            case 16:
                Regs().SetField(&PfbCfg1, MODS_PFB_FBPA_CFG1_BANK_4);
                break;
            default:
                Printf(Tee::PriError,
                       "Invalid setting for Gpu.FbBanks: %d\n", FbBanks);
                MASSERT(!"Generic assertion failure<refer to previous message>.");
        }
        PfbCfg1Mask |= Regs().LookupMask(MODS_PFB_FBPA_CFG1_BANK);
    }

    if (FbBurstLength != 0)
    {
        switch (FbBurstLength)
        {
            case 4:
                Regs().SetField(&PfbCfg0, MODS_PFB_FBPA_CFG0_BURST_LENGTH_4);
                break;
            case 8:
                Regs().SetField(&PfbCfg0, MODS_PFB_FBPA_CFG0_BURST_LENGTH_8);
                break;
            default:
                Printf(Tee::PriError,
                       "Invalid setting for Gpu.FbBurstLength: %d\n",
                       FbBurstLength);
                MASSERT(!"Generic assertion failure<refer to previous message>.");
        }
        PfbCfg0Mask |= Regs().LookupMask(MODS_PFB_FBPA_CFG0_BURST_LENGTH);
    }

    if (FbColumnBits != 0)
    {
        if (FbColumnBits < 8)
        {
            Printf(Tee::PriError,
                   "Invalid setting for Gpu.FbColumnBits: %d\n", FbColumnBits);
            MASSERT(!"Generic assertion failure<refer to previous message>.");
        }
        else
        {
            Regs().SetField(&PfbCfg1, MODS_PFB_FBPA_CFG1_COL, FbColumnBits);
            PfbCfg1Mask |= Regs().LookupMask(MODS_PFB_FBPA_CFG1_COL);
            if (FbColumnBits == Regs().LookupValue(MODS_PFB_FBPA_CFG1_COL_8))
            {
                Regs().SetField(&PfbReadFtCfg,
                                MODS_PFB_FBPA_READ_FT_CFG_PAGE_SIZE_1K);
                PfbReadFtCfgMask |= Regs().LookupMask(
                                MODS_PFB_FBPA_READ_FT_CFG_PAGE_SIZE);
            }
        }
    }

    if (FbExtBank != 0)
    {
        switch (FbExtBank)
        {
            case 1:
                Regs().SetField(&PfbCfg0, MODS_PFB_FBPA_CFG0_EXTBANK_0);
                break;
            case 2:
                Regs().SetField(&PfbCfg0, MODS_PFB_FBPA_CFG0_EXTBANK_1);
                break;
            default:
                Printf(Tee::PriError,
                       "Invalid setting for Gpu.FbExtBank: %d.\n", FbExtBank);
                MASSERT(!"Generic assertion failure<refer to previous message>.");
        }
        PfbCfg0Mask |= Regs().LookupMask(MODS_PFB_FBPA_CFG0_EXTBANK);
    }

    if (GetFbGddr5x16Mode())
    {
        Regs().SetField(&PfbCfg0, MODS_PFB_FBPA_CFG0_CLAMSHELL_X16);
        PfbCfg0Mask |= Regs().LookupMask(MODS_PFB_FBPA_CFG0_CLAMSHELL);
    }

    if (FbRowBits != 0)
    {
        Regs().SetField(&PfbCfg1, MODS_PFB_FBPA_CFG1_ROWA, FbRowBits - 8);
        Regs().SetField(&PfbCfg1, MODS_PFB_FBPA_CFG1_ROWB, FbRowBits - 8);
        PfbCfg1Mask |= Regs().LookupMask(MODS_PFB_FBPA_CFG1_ROWA);
        PfbCfg1Mask |= Regs().LookupMask(MODS_PFB_FBPA_CFG1_ROWB);
    }

    if (GetFbMs01Mode())
    {
        Regs().SetField(&PfbFbioBcast,
                        MODS_PFB_FBPA_FBIO_BROADCAST_DDR_MODE_MS01);
        PfbFbioBcastMask |=
            Regs().LookupMask(MODS_PFB_FBPA_FBIO_BROADCAST_DDR_MODE);
    }

    if (extraPfbCfg0Mask)
    {
        PfbCfg0     &= (~extraPfbCfg0Mask);
        PfbCfg0     |= extraPfbCfg0;
        PfbCfg0Mask |= extraPfbCfg0Mask;
    }

    if (FbBankSize)
    {
        // We left the arg scaffolding in because it might come back
        // later, but not actually supported any longer.
        Printf(Tee::PriError, "Gpu.FbBankSize not supported on this arch!\n");
        MASSERT(!"Generic assertion failure<refer to previous message>.");
    }

    if (FbRandomBankSwizzle)
    {
        // We left the arg scaffolding in because it might come back
        // later, but not actually supported any longer.
        Printf(Tee::PriError,
               "Gpu.FbRandomBankSwizzle not supported on this arch!\n");
        MASSERT(!"Generic assertion failure<refer to previous message>.");
    }
#ifdef LW_VERIF_FEATURES
    if (InPreVBIOSSetup)
    {
        // The devinit scripts will be intercepted and changed in RM to not
        // clobber our specific settings on cfg0, 1, bankcfg.  Our job is to
        // communicate which registers, values and masks to use to accomplish that.
        vector<LW_CFGEX_SET_GRCTX_INIT_OVERRIDES_PARAMS> params;
        LW_CFGEX_SET_GRCTX_INIT_OVERRIDES_PARAMS param = { };

        //LwRmPtr pLwRm;
        if (Platform::GetSimulationMode() != Platform::Hardware)
        {
            if (PfbFbioBcastMask)
            {
                param.regAddr = Regs().LookupAddress(MODS_PFB_FBPA_FBIO_BROADCAST);
                param.andMask = ~PfbFbioBcastMask; // mask to leave alone
                param.orMask  = PfbFbioBcast;      // value to send in
                params.push_back(param);
            }
            if (PfbCfg0Mask)
            {
                param.regAddr = Regs().LookupAddress(MODS_PFB_FBPA_CFG0);
                param.andMask = ~PfbCfg0Mask; // mask to leave alone
                param.orMask  = PfbCfg0;      // value to send in
                params.push_back(param);
            }
            if (PfbCfg1Mask)
            {
                param.regAddr = Regs().LookupAddress(MODS_PFB_FBPA_CFG1);
                param.andMask = ~PfbCfg1Mask; // mask to leave alone
                param.orMask  = PfbCfg1;      // value to send in
                params.push_back(param);
            }
            if (PfbReadFtCfgMask)
            {
                param.regAddr = Regs().LookupAddress(MODS_PFB_FBPA_READ_FT_CFG);
                param.andMask = ~PfbReadFtCfgMask; // mask to leave alone
                param.orMask  = PfbReadFtCfg;      // value to send in
                params.push_back(param);
            }
        }

        if (FbBankSwizzleParam)
        {
            param.regAddr  = Regs().LookupAddress(MODS_PFB_FBPA_BANK_SWIZZLE);
            param.andMask  = 0;  // change entire register
            param.orMask = bankSwizzle;
            params.push_back(param);
        }

        // Give newer chips a chance to fiddle with the list.
        FilterMemCfgRegOverrides(&params);

        if (params.size())
        {
            // Pack params into a vector of bytes for registry, yuck.
            const size_t len = params.size() * sizeof(params[0]);
            vector<UINT08> Array(len);
            const UINT08 * pParams = reinterpret_cast<UINT08*>(&params[0]);
            for (size_t i = 0; i < len; i++)
            {
                Array[i] = pParams[i];
            }
            Registry::Write("ResourceManager", "OverrideGpuDevinit", Array);
        }
    }
    else
#endif
    {
        if (Platform::GetSimulationMode() != Platform::Hardware)
        {
            if (defPfbFbioBcast != PfbFbioBcast)
            {
                Regs().Write32(MODS_PFB_FBPA_FBIO_BROADCAST, PfbFbioBcast);
            }
            if (defPfbCfg0 != PfbCfg0)
            {
                Regs().Write32(MODS_PFB_FBPA_CFG0, PfbCfg0);
            }
            if (defPfbCfg1 != PfbCfg1)
            {
                Regs().Write32(MODS_PFB_FBPA_CFG1, PfbCfg1);
            }
            if (defPfbReadFtCfg != PfbReadFtCfg)
            {
                Regs().Write32(MODS_PFB_FBPA_READ_FT_CFG, PfbReadFtCfg);
            }
        }
        if (FbBankSwizzleParam)
        {
            Regs().Write32(MODS_PFB_FBPA_BANK_SWIZZLE, bankSwizzle);
        }
    }
}

#ifdef LW_VERIF_FEATURES
/*virtual*/
void GpuSubdevice::FilterMemCfgRegOverrides
(
    vector<LW_CFGEX_SET_GRCTX_INIT_OVERRIDES_PARAMS>* pOverrides
)
{
    // Does nothing in base class.
    // In gpus with multicast priv-regs schemes, we dup the overrides
    // to also cover the multicast versions of each register.
}

// This API has no dependancies on the GPU's register addresses. The API above should be overloaded
// by GPUs that want to override the multicast regs.
void GpuSubdevice::FilterMemCfgRegOverrides
(
    UINT32 offsetMC0,
    UINT32 offsetMC1,
    UINT32 offsetMC2,
    vector<LW_CFGEX_SET_GRCTX_INIT_OVERRIDES_PARAMS>* pOverrides
)
{
    const size_t len = pOverrides->size();
    for (size_t i = 0; i < len; i++)
    {
        LW_CFGEX_SET_GRCTX_INIT_OVERRIDES_PARAMS param;
        param = (*pOverrides)[i];
        param.regAddr = (*pOverrides)[i].regAddr + offsetMC0;
        pOverrides->push_back(param);
        param.regAddr = (*pOverrides)[i].regAddr + offsetMC1;
        pOverrides->push_back(param);
        param.regAddr = (*pOverrides)[i].regAddr + offsetMC2;
        pOverrides->push_back(param);
    }
}

#endif

/* virtual */
bool GpuSubdevice::ApplyFbCmdMapping
(
    const string &FbCmdMapping,
    UINT32 *pPfbCfgBcast,
    UINT32 *pPfbCfgBcastMask
) const
{
    if (FbCmdMapping == "GENERIC")
        Regs().SetField(pPfbCfgBcast,
                MODS_PFB_FBPA_FBIO_BROADCAST_CMD_MAPPING_GENERIC);
    else if (FbCmdMapping == "GENERIC2")
        Regs().SetField(pPfbCfgBcast,
                MODS_PFB_FBPA_FBIO_BROADCAST_CMD_MAPPING_GENERIC2);
    else if (FbCmdMapping == "GDDR5_170")
        Regs().SetField(pPfbCfgBcast,
                MODS_PFB_FBPA_FBIO_BROADCAST_CMD_MAPPING_GDDR5_170);
    else if (FbCmdMapping == "GDDR5_170_MIRR")
        Regs().SetField(pPfbCfgBcast,
                MODS_PFB_FBPA_FBIO_BROADCAST_CMD_MAPPING_GDDR5_170_MIRR);
    else if (FbCmdMapping == "GDDR3_BGA136")
        Regs().SetField(pPfbCfgBcast,
                MODS_PFB_FBPA_FBIO_BROADCAST_CMD_MAPPING_GDDR3_BGA136);
    else if (FbCmdMapping == "SDDR3_BGA100")
        Regs().SetField(pPfbCfgBcast,
                MODS_PFB_FBPA_FBIO_BROADCAST_CMD_MAPPING_SDDR3_BGA100);
    else if (FbCmdMapping == "GDDR5_GT215_COMP")
        Regs().SetField(pPfbCfgBcast,
                MODS_PFB_FBPA_FBIO_BROADCAST_CMD_MAPPING_GDDR5_GT215_COMP);
    else if (FbCmdMapping == "GDDR3_GT215_COMP")
        Regs().SetField(pPfbCfgBcast,
                MODS_PFB_FBPA_FBIO_BROADCAST_CMD_MAPPING_GDDR3_GT215_COMP);
    else
        return false;

    *pPfbCfgBcastMask |=
        Regs().LookupMask(MODS_PFB_FBPA_FBIO_BROADCAST_CMD_MAPPING);
    return true;
}

RC GpuSubdevice::PreVBIOSSetup()
{
    RC rc;

    CHECK_RC(GetFsImpl()->ApplyFloorsweepingChanges(true));
    ApplyEarlyRegsChanges(true);
    ApplyJtagChanges(PreVbiosInit);
    ApplyMemCfgProperties(true);

    return rc;
}

RC GpuSubdevice::PostVBIOSSetup()
{
    RC rc;

    CHECK_RC(GetFsImpl()->ApplyFloorsweepingChanges(false));
    ApplyEarlyRegsChanges(false);
    CHECK_RC(ExelwtePreGpuInitScript());
    ApplyJtagChanges(PostVbiosInit);
    ApplyMemCfgProperties(false);

    return rc;
}

//! \brief Dump Micron GDDR6 chipid information (verified on TU10x boards)
//!
//! Priv access is required, as well as Micron GDDR6 memory.
//! This function can't go in the Framebuffer class as it is called when m_Fb is uninitialized.
void GpuSubdevice::DumpMicronGDDR6Info()
{
    RegHal &regs = Regs();

    // If we are running on a production fused part with a HULK cert and the ChipID FPF enabled,
    // we can skip the priv access check.
    bool hulkEnabled;
    GetIsHulkFlashed(&hulkEnabled);
    if (hulkEnabled &&
        regs.IsSupported(MODS_FPF_OPT_FIELD_SPARE_FUSES_3_DATA) &&
        regs.Test32(MODS_FPF_OPT_FIELD_SPARE_FUSES_3_DATA, 0x1))
    {
        Printf(Tee::PriLow, "Using HULK license to read ChipId info\n");
    }
    // Otherwise check that we have priv access to the required registers
    else if (!regs.IsSupported(MODS_PFB_FBPA_MEM_PRIV_LEVEL_MASK) ||
             !regs.IsSupported(MODS_PFB_FBPA_FBIO_PRIV_LEVEL_MASK) ||
             !regs.Test32(MODS_PFB_FBPA_MEM_PRIV_LEVEL_MASK_WRITE_PROTECTION_LEVEL0_ENABLE) ||
             !regs.Test32(MODS_PFB_FBPA_FBIO_PRIV_LEVEL_MASK_WRITE_PROTECTION_LEVEL0_ENABLE))
    {
        Printf(Tee::PriWarn, "Insufficient register access for dumping memory info.\n");
        return;
    }

    // Prevent the OS from writing to FB
    Xp::SuspendConsole(this);
    DEFER
    {
        // Reset the GPU upon exiting the function
        HotReset();
        Xp::ResumeConsole(this);
    };

    // Save off initial register state
    const auto cfg0    = regs.Read32(MODS_PFB_FBPA_CFG0);
    const auto pwrCtrl = regs.Read32(MODS_PFB_FBPA_FBIO_PWR_CTRL);
    const auto cfgPwrd = regs.Read32(MODS_PFB_FBPA_FBIO_CFG_PWRD);
    const auto cfg11   = regs.Read32(MODS_PFB_FBPA_FBIO_CFG11);
    const auto mrs     = regs.Read32(MODS_PFB_FBPA_GENERIC_MRS);

    // Disable ACPD
    regs.Write32(MODS_PFB_FBPA_CFG0_DRAM_ACPD_NO_POWERDOWN);

    // Disable Refresh
    regs.Write32(MODS_PFB_FBPA_REFCTRL_VALID_0);
    regs.Write32(MODS_PFB_FBPA_FBIO_PWR_CTRL_DYNAMIC_DQS_RCVR_PWRD_DISABLE);

    auto cfgPwrdNew = cfgPwrd;
    regs.SetField(&cfgPwrdNew, MODS_PFB_FBPA_FBIO_CFG_PWRD_DIFFAMP_PWRD_DISABLE);
    regs.SetField(&cfgPwrdNew, MODS_PFB_FBPA_FBIO_CFG_PWRD_DQ_RX_DIGDLL_PWRD_DISABLE);
    regs.SetField(&cfgPwrdNew, MODS_PFB_FBPA_FBIO_CFG_PWRD_DQ_RX_GDDR5_PWRD_ENABLE);
    regs.SetField(&cfgPwrdNew, MODS_PFB_FBPA_FBIO_CFG_PWRD_RDQS_RX_GDDR5_PWRD_ENABLE);
    regs.Write32(MODS_PFB_FBPA_FBIO_CFG_PWRD, cfgPwrdNew);

    // Arcane entry sequence
    regs.Write32(MODS_PFB_FBPA_GENERIC_MRS, 0x080);
    regs.Write32(MODS_PFB_FBPA_GENERIC_MRS, 0x4ca);
    regs.Write32(MODS_PFB_FBPA_GENERIC_MRS, 0xca3);
    regs.Write32(MODS_PFB_FBPA_GENERIC_MRS, 0x8dc);
    regs.Write32(MODS_PFB_FBPA_GENERIC_MRS, 0x4ca);
    regs.Write32(MODS_PFB_FBPA_GENERIC_MRS, 0x1c0);
    regs.Write32(MODS_PFB_FBPA_GENERIC_MRS, 0x380);
    regs.Write32(MODS_PFB_FBPA_GENERIC_MRS, 0xff9);
    regs.Write32(MODS_PFB_FBPA_GENERIC_MRS, 0xfff);
    regs.Write32(MODS_PFB_FBPA_GENERIC_MRS, 0xfff);
    regs.Write32(MODS_PFB_FBPA_GENERIC_MRS, 0xffd);

    // Increment the pointer to the first bit of ChipId info
    for (UINT32 i = 0; i < 42; i++)
    {
        regs.Write32(MODS_PFB_FBPA_GENERIC_MRS, 0x8c7);
        Tasker::Sleep(0.1);
    }

    auto cfg11New = cfg11;
    regs.SetField(&cfg11New, MODS_PFB_FBPA_FBIO_CFG11_VEND_ID_C0_LOAD_EN, 0x1);
    regs.SetField(&cfg11New, MODS_PFB_FBPA_FBIO_CFG11_VEND_ID_C1_LOAD_EN, 0x1);

    // Can't call GetFB()->GetFbioMask() as m_Fb is uninitialized
    const auto fbioMask = GetFsImpl()->FbioMask();

    // 64 bits worth of information, fetched via 64 register reads
    // 0=0x0
    // 1=0xFFFFFFFF
    map<UINT32, std::array<std::bitset<64>, 2>> bits;
    for (UINT32 i = 0; i < 64; i++)
    {
        regs.Write32(MODS_PFB_FBPA_GENERIC_MRS, 0x8c7);
        Tasker::Sleep(0.1);

        // Enable VID Reading
        regs.Write32(MODS_PFB_FBPA_FBIO_CFG11, cfg11New);

        for (UINT32 hwFbio = 0; hwFbio < regs.Read32(MODS_PTOP_SCAL_NUM_FBPAS); hwFbio++)
        {
            if (fbioMask & (1u << hwFbio))
            {
                // Try reading info even if no chip is attached to the channel
                const auto val0 =
                    RegRd32(regs.LookupAddress(MODS_PFB_FBPA_0_FBIO_VEND_ID_C0) +
                            regs.LookupAddress(MODS_FBPA_PRI_STRIDE) * hwFbio);
                const auto val1 =
                    RegRd32(regs.LookupAddress(MODS_PFB_FBPA_0_FBIO_VEND_ID_C1) +
                            regs.LookupAddress(MODS_FBPA_PRI_STRIDE) * hwFbio);
                UINT32 bitIdx = 0;
                if (i < 22)
                {
                    bitIdx = i * 3;
                }
                else if (i >= 22 && i < 43)
                {
                    bitIdx = (i - 22) * 3 + 2;
                }
                else if (i >= 43 && i < 64)
                {
                    bitIdx = (i - 43) * 3 + 1;
                }
                else
                {
                    MASSERT(!"UNREACHABLE");
                }
                bits[hwFbio][0][bitIdx] = (val0 == 0xFFFFFFFF);
                bits[hwFbio][1][bitIdx] = (val1 == 0xFFFFFFFF);
            }
        }
        regs.Write32(MODS_PFB_FBPA_FBIO_CFG11, cfg11);
    }

    // Arcane exit sequence
    regs.Write32(MODS_PFB_FBPA_GENERIC_MRS, 0x080);
    regs.Write32(MODS_PFB_FBPA_GENERIC_MRS, 0x080);
    regs.Write32(MODS_PFB_FBPA_GENERIC_MRS, 0x080);
    regs.Write32(MODS_PFB_FBPA_GENERIC_MRS, 0x080);

    regs.Write32(MODS_PFB_FBPA_GENERIC_MRS, 0x380);
    regs.Write32(MODS_PFB_FBPA_GENERIC_MRS, 0x080);
    regs.Write32(MODS_PFB_FBPA_GENERIC_MRS, 0x080);
    regs.Write32(MODS_PFB_FBPA_GENERIC_MRS, 0x081);
    regs.Write32(MODS_PFB_FBPA_GENERIC_MRS, 0x080);
    regs.Write32(MODS_PFB_FBPA_GENERIC_MRS, 0x081);
    regs.Write32(MODS_PFB_FBPA_GENERIC_MRS, 0x080);

    // Restore registers just-in-case (even though we reset afterwards)
    regs.Write32(MODS_PFB_FBPA_CFG0,          cfg0);
    regs.Write32(MODS_PFB_FBPA_FBIO_PWR_CTRL, pwrCtrl);
    regs.Write32(MODS_PFB_FBPA_FBIO_CFG_PWRD, cfgPwrd);
    regs.Write32(MODS_PFB_FBPA_REFCTRL_VALID_1);
    regs.Write32(MODS_PFB_FBPA_FBIO_CFG11,    cfg11);
    regs.Write32(MODS_PFB_FBPA_GENERIC_MRS,   mrs);

    // Dump ChipId Info
    const auto& devInst = DevInst();
    for (const auto& fbioBits : bits)
    {
        const auto& hwFbio = fbioBits.first;
        for (UINT32 subp = 0; subp < 2; subp++)
        {
            const auto& bitfield = fbioBits.second[subp];
            const UINT64 fieldNum = bitfield.to_ullong();
            const char hwFbioLetter = 'A' + hwFbio;

            // If the returned data is all 0s or 1s, we queried either
            // a disconnected channel or a chip from an unsupported vendor
            if (fieldNum == 0x0ULL || fieldNum == ~0x0ULL)
            {
                Printf(Tee::PriNormal, "DRAM_%c%d_GPU%d_ChipId: N/A\n",
                    hwFbioLetter, subp, devInst);
                continue;
            }
            Printf(Tee::PriNormal,
                "DRAM_%c%d_GPU%d_ChipId: 0x%016llx\n",
                hwFbioLetter, subp, devInst, fieldNum);

            // All these parity bits are taken from a spreadsheet provided by Micron
            bool parityError = false;
            const bool parityMode = bitfield[43];
            Printf(Tee::PriLow, "Parity mode (bit 43): %d\n", parityMode);
            if (parityMode == 0x1)
            {
                // Compute parity bits for checking
                bool c0 = 0x1, c1 = 0x1, c2 = 0x1;
                for (UINT32 i = 0; i < 16; i++)
                {
                    c0 ^= bitfield[i];
                }
                for (UINT32 i = 16; i < 32; i++)
                {
                    c1 ^= bitfield[i];
                }
                for (UINT32 i = 32; i < 44; i++)
                {
                    c2 ^= bitfield[i];
                }
                c2 ^= bitfield[51] ^ bitfield[52] ^ bitfield[63];

                // Fetch parity bits
                const bool p0 = bitfield[60];
                const bool p1 = bitfield[61];
                const bool p2 = bitfield[62];

                // Compare parity bits
                if (c0 != p0 || c1 != p1 || c2 != p2)
                {
                    parityError = true;
                    Printf(Tee::PriWarn,
                           "Parity mismatch!\n"
                           "Retrieved Parity bits [60-62]: %d%d%d\n"
                           "Computed  Parity bits [60-62]: %d%d%d\n",
                           p0, p1, p2,
                           c0, c1, c2);
                }
            }
            else if (parityMode == 0x0)
            {
                // Compute parity bits for checking
                bool c0 = 0x1, c1 = 0x1, c2 = 0x1, c3 = 0x1;
                for (UINT32 i = 0; i < 15; i++)
                {
                    c0 ^= bitfield[i];
                }
                for (UINT32 i = 15; i < 30; i++)
                {
                    c1 ^= bitfield[i];
                }
                for (UINT32 i = 30; i < 43; i++)
                {
                    c2 ^= bitfield[i];
                }
                c3 ^= bitfield[51] ^ bitfield[52];

                // Fetch parity bits
                const bool p0 = bitfield[60];
                const bool p1 = bitfield[61];
                const bool p2 = bitfield[62];
                const bool p3 = bitfield[63];

                // Compare parity bits
                if (c0 != p0 || c1 != p1 || c2 != p2 || c3 != p3)
                {
                    parityError = true;
                    Printf(Tee::PriWarn,
                           "Parity mismatch!\n"
                           "Retrieved Parity bits [60-63]: %d%d%d%d\n"
                           "Computed  Parity bits [60-63]: %d%d%d%d\n",
                           p0, p1, p2, p3,
                           c0, c1, c2, c3);
                }
            }
            const UINT64 lotIdNum = (fieldNum >> 3) & Utility::Bitmask<UINT64>(20);
            const UINT64 waferNum = (fieldNum >> 23) & Utility::Bitmask<UINT64>(5);
            const UINT64 lotIdHex = ((fieldNum >> 51) & Utility::Bitmask<UINT64>(2)) + 0xB;
            Printf(Tee::PriNormal,
                   "DRAM_%c%d_GPU%d_LotId:  %llu%llX\n",
                   hwFbioLetter, subp, devInst, lotIdNum, lotIdHex);
            Printf(Tee::PriNormal,
                   "DRAM_%c%d_GPU%d_Wafer:  %llu\n",
                   hwFbioLetter, subp, devInst, waferNum);
            Printf(Tee::PriNormal,
                   "DRAM_%c%d_GPU%d_Parity: %s\n",
                   hwFbioLetter, subp, devInst, parityError ? "Bad" : "Good");
        }
    }
}

bool GpuSubdevice::PollJtagForNonzero(void *pVoidPollArgs)
{
    GpuSubdevice* subdev = static_cast<GpuSubdevice*>(pVoidPollArgs);
    return (subdev->Regs().Read32(MODS_PJTAG_ACCESS_CTRL_CTRL_STATUS) != 0);
}

// Allow Jtag read/write access.
void GpuSubdevice::ApplyJtagChanges(UINT32 processFlag)
{
#ifdef LW_VERIF_FEATURES
    if (Platform::GetSimulationMode() == Platform::Hardware)
    {
        if (processFlag != PostVbiosInit)
            return;

        RegHal &regs = Regs();

        // The jtag instruction is a no-op for any chiplets that do not
        // exist, so just call the instruction for all possible chiplets.
        for (UINT32 chiplet_id=0; chiplet_id < GetNumJtagChiplets(); chiplet_id++)
        {
            regs.Write32(MODS_PJTAG_ACCESS_CONFIG, 0);

            UINT32 h2jRegCtrl=0;
            regs.SetField(&h2jRegCtrl, MODS_PJTAG_ACCESS_CTRL_INSTR_ID, 0x39);
            regs.SetField(&h2jRegCtrl, MODS_PJTAG_ACCESS_CTRL_REG_LENGTH, 64-1);
            regs.SetField(&h2jRegCtrl, MODS_PJTAG_ACCESS_CTRL_DWORD_EN, 0);
            regs.SetField(&h2jRegCtrl, MODS_PJTAG_ACCESS_CTRL_CHIPLET_SEL, chiplet_id);
            regs.SetField(&h2jRegCtrl, MODS_PJTAG_ACCESS_CTRL_REQ_CTRL, 1);
            regs.Write32(MODS_PJTAG_ACCESS_CTRL, h2jRegCtrl);

            RC rc = POLLWRAP_HW(PollJtagForNonzero, static_cast<void*>(this), 1000);
            if (OK == rc)
            {
                regs.Write32(MODS_PJTAG_ACCESS_DATA, 0x1B1CCF93);
                regs.Write32(MODS_PJTAG_ACCESS_DATA, 0x7CF48C24);
            }
            regs.Write32(MODS_PJTAG_ACCESS_CTRL, 0);
            // Bug 927067: exit loop if an invalid chiplet is found.
            if (OK != rc)
            {
                break;
            }
        }
    }
#endif // LW_VERIF_FEATURES
}

/* virtual */ RC GpuSubdevice::PrepareEndOfSimCheck
(
    Gpu::ShutDownMode mode
)
{
    RC rc;

    Printf(Tee::PriNormal, "%s: Waiting for all channels to become idle\n", __FUNCTION__);

    ChannelsIdlePollArgs args;
    args.pSubdev = this;
    if (IsSMCEnabled())
    {
        MASSERT(m_GetSmcRegHalsCb);
        vector<SmcRegHal*> smcRegHals;

        // Acquire the mutex for SmcRegHal to ensure they are not freed
        // before SmcRegHal usage is complete here
        Tasker::MutexHolder mh(m_GetSmcRegHalsCb(&smcRegHals));

        for (auto const & lwrRegHal : smcRegHals)
        {
            args.pLwRm = lwrRegHal->GetLwRmPtr();
            CHECK_RC(POLLWRAP_HW(&PollAllChannelsIdleWrapper, &args, Tasker::NO_TIMEOUT));
        }
    }
    else
    {
        args.pLwRm = nullptr;
        CHECK_RC(POLLWRAP_HW(&PollAllChannelsIdleWrapper, &args, Tasker::NO_TIMEOUT));
    }

    if (Gpu::ShutDownMode::QuickAndDirty != mode)
    {
        // Do a UFLUSH to clear all pending transactions
        Printf(Tee::PriNormal, "Preparing EndOfSim check. Flushing all "
            "outstanding requests via UFLUSH_FB_FLUSH_PENDING_ISSUE.\n");
        CHECK_RC(FbFlush(Tasker::NO_TIMEOUT));
    }

    return rc;
}

void GpuSubdevice::EndOfSimDelay() const
{
    Platform::Delay(3);   // Delay 3us before deasserting END_OF_SIM
}

RC GpuSubdevice::InsertEndOfSimFence
(
    UINT32 pprivSysPriFence,
    UINT32 pprivGpcGpc0PriFence,
    UINT32 pprivGpcStride,
    UINT32 ppriveFbpFbp0PrivFence,
    UINT32 pprivFbpStride,
    bool bForceFenceSyncBeforeEos
) const
{
    RC rc;

    // Fence synce will hurt performance, but gm20x+ chips rely on it
    // in order to make sure register poll must be completed before
    // EOS assertion.
    // If DisableFenceSyncBeforeEos is not specified, fence sync will
    // be sent down by default for gm20x+ chips.
    JavaScriptPtr pJs;
    bool bFenceSyncBeforeEos = false;
    bool bDisableFenceSyncBeforeEos = false;
    CHECK_RC(pJs->GetProperty(Gpu_Object,
        Gpu_DisableFenceSyncBeforeEos,
        &bDisableFenceSyncBeforeEos));

    if (bDisableFenceSyncBeforeEos)
    {
        // fence sync is disabled explicitly
        bFenceSyncBeforeEos = false;
    }
    else
    {
        if (!bForceFenceSyncBeforeEos)
        {
            CHECK_RC(pJs->GetProperty(Gpu_Object,
                Gpu_FenceSyncBeforeEos,
                &bFenceSyncBeforeEos));

            if (!bFenceSyncBeforeEos)
            {
                // if fence sync is not enabled explicitly,
                // enable it intelligently if register restore
                // happened in GpuShutDown.
                bFenceSyncBeforeEos = IsRegTableRestored();
            }
        }
        else
        {
            bFenceSyncBeforeEos = bForceFenceSyncBeforeEos;
        }
    }

    if (bFenceSyncBeforeEos)
    {
        //
        // Insert fence to ensure all priv register accesses before
        // eos are done. Returned data can be ignored.
        //

        // sys
        Printf(Tee::PriNormal, "Preparing EndOfSim check. Reading "
            "SYS_PRI_FENCE to sync sys priv register access.\n");
        RegRd32(pprivSysPriFence);

        // gpc
        Printf(Tee::PriNormal, "Preparing EndOfSim check. Reading "
            "GPCx_PRI_FENCE to sync gpc priv register access.\n");
        UINT32 gpcCount = GetGpcCount();
        for (UINT32 gpcIdx = 0; gpcIdx < gpcCount; ++ gpcIdx)
        {
            RegRd32(pprivGpcGpc0PriFence + gpcIdx * pprivGpcStride);
        }

        // fb
        Printf(Tee::PriNormal, "Preparing EndOfSim check. Reading "
            "FBPx_PRI_FENCE to sync fb priv register access.\n");
        UINT32 fbCount = GetFrameBufferUnitCount();
        for (UINT32 fbIdx = 0; fbIdx < fbCount; ++ fbIdx)
        {
            RegRd32(ppriveFbpFbp0PrivFence + fbIdx * pprivFbpStride);
        }
    }

    return rc;
}

RC GpuSubdevice::GetVmiopElw(VmiopElw** ppVmiopElw) const
{
    RC rc;
    MASSERT(Platform::IsVirtFunMode());
    MASSERT(ppVmiopElw);

    *ppVmiopElw = nullptr;

    VmiopElwManager *pMgr = VmiopElwManager::GetInstance();
    if (!pMgr)
    {
        Printf(Tee::PriError, "Vmiop manager not found\n");
        return RC::DEVICE_NOT_FOUND;
    }

    const UINT32 gfid = pMgr->GetGfidAttachedToProcess();
    *ppVmiopElw = pMgr->GetVmiopElw(gfid);
    if (!*ppVmiopElw)
    {
        Printf(Tee::PriError, "Vmiop elw for gfid %u not found\n", gfid);
        return RC::DEVICE_NOT_FOUND;
    }

    return rc;
}

//-----------------------------------------------------------------------------
//! \brief Create the PTE kind table for Fermi
//!
//! This is ordinarily done in the constructor, but adding all the kinds in
//! a single procedure causes the DOS Mods build to hang.  This routine breaks
//! the allocations up into several soubroutines to avoid problems with the
//! DJGPP optimizer.
void GpuSubdevice::CreatePteKindTable()
{
    m_PteKindTable.clear();

    AddPteKinds();
    AddPteKindsFloatDepth();
    AddPteKindsCompressed();
}

//-----------------------------------------------------------------------------
void GpuSubdevice::AddPteKinds()
{
    static const PteKindTableEntry entries[] =
    {
        PTE_KIND_TABLE_ENTRY(PITCH),
        PTE_KIND_TABLE_ENTRY(PITCH_NO_SWIZZLE),
        PTE_KIND_TABLE_ENTRY(Z16),
        PTE_KIND_TABLE_ENTRY(Z16_2C),
        PTE_KIND_TABLE_ENTRY(Z16_MS2_2C),
        PTE_KIND_TABLE_ENTRY(Z16_MS4_2C),
        PTE_KIND_TABLE_ENTRY(Z16_MS8_2C),
        PTE_KIND_TABLE_ENTRY(Z16_MS16_2C),
        PTE_KIND_TABLE_ENTRY(Z16_2Z),
        PTE_KIND_TABLE_ENTRY(Z16_MS2_2Z),
        PTE_KIND_TABLE_ENTRY(Z16_MS4_2Z),
        PTE_KIND_TABLE_ENTRY(Z16_MS8_2Z),
        PTE_KIND_TABLE_ENTRY(Z16_MS16_2Z),
        PTE_KIND_TABLE_ENTRY(Z16_4CZ),
        PTE_KIND_TABLE_ENTRY(Z16_MS2_4CZ),
        PTE_KIND_TABLE_ENTRY(Z16_MS4_4CZ),
        PTE_KIND_TABLE_ENTRY(Z16_MS8_4CZ),
        PTE_KIND_TABLE_ENTRY(S8Z24),
        PTE_KIND_TABLE_ENTRY(S8Z24_1Z),
        PTE_KIND_TABLE_ENTRY(S8Z24_MS2_1Z),
        PTE_KIND_TABLE_ENTRY(S8Z24_MS4_1Z),
        PTE_KIND_TABLE_ENTRY(S8Z24_MS8_1Z),
        PTE_KIND_TABLE_ENTRY(S8Z24_2CZ),
        PTE_KIND_TABLE_ENTRY(S8Z24_MS2_2CZ),
        PTE_KIND_TABLE_ENTRY(S8Z24_MS4_2CZ),
        PTE_KIND_TABLE_ENTRY(S8Z24_MS8_2CZ),
        PTE_KIND_TABLE_ENTRY(S8Z24_MS16_2CZ),
        PTE_KIND_TABLE_ENTRY(S8Z24_2CS),
        PTE_KIND_TABLE_ENTRY(S8Z24_MS2_2CS),
        PTE_KIND_TABLE_ENTRY(S8Z24_MS4_2CS),
        PTE_KIND_TABLE_ENTRY(S8Z24_MS8_2CS),
        PTE_KIND_TABLE_ENTRY(S8Z24_MS16_2CS),
        PTE_KIND_TABLE_ENTRY(S8Z24_4CSZV),
        PTE_KIND_TABLE_ENTRY(S8Z24_MS2_4CSZV),
        PTE_KIND_TABLE_ENTRY(S8Z24_MS4_4CSZV),
        PTE_KIND_TABLE_ENTRY(S8Z24_MS8_4CSZV),
        PTE_KIND_TABLE_ENTRY(V8Z24_MS4_VC12),
        PTE_KIND_TABLE_ENTRY(V8Z24_MS4_VC4),
        PTE_KIND_TABLE_ENTRY(V8Z24_MS8_VC8),
        PTE_KIND_TABLE_ENTRY(V8Z24_MS8_VC24),
        PTE_KIND_TABLE_ENTRY(V8Z24_MS4_VC12_1ZV),
        PTE_KIND_TABLE_ENTRY(V8Z24_MS4_VC4_1ZV),
        PTE_KIND_TABLE_ENTRY(V8Z24_MS8_VC8_1ZV),
        PTE_KIND_TABLE_ENTRY(V8Z24_MS8_VC24_1ZV),
        PTE_KIND_TABLE_ENTRY(V8Z24_MS4_VC12_2CS),
        PTE_KIND_TABLE_ENTRY(V8Z24_MS4_VC4_2CS),
        PTE_KIND_TABLE_ENTRY(V8Z24_MS8_VC8_2CS),
        PTE_KIND_TABLE_ENTRY(V8Z24_MS8_VC24_2CS),
        PTE_KIND_TABLE_ENTRY(V8Z24_MS4_VC12_2CZV),
        PTE_KIND_TABLE_ENTRY(V8Z24_MS4_VC4_2CZV),
        PTE_KIND_TABLE_ENTRY(V8Z24_MS8_VC8_2CZV),
        PTE_KIND_TABLE_ENTRY(V8Z24_MS8_VC24_2CZV),
        PTE_KIND_TABLE_ENTRY(V8Z24_MS4_VC12_2ZV),
        PTE_KIND_TABLE_ENTRY(V8Z24_MS4_VC4_2ZV),
        PTE_KIND_TABLE_ENTRY(V8Z24_MS8_VC8_2ZV),
        PTE_KIND_TABLE_ENTRY(V8Z24_MS8_VC24_2ZV),
        PTE_KIND_TABLE_ENTRY(V8Z24_MS4_VC12_4CSZV),
        PTE_KIND_TABLE_ENTRY(V8Z24_MS4_VC4_4CSZV),
        PTE_KIND_TABLE_ENTRY(V8Z24_MS8_VC8_4CSZV),
        PTE_KIND_TABLE_ENTRY(V8Z24_MS8_VC24_4CSZV),
        PTE_KIND_TABLE_ENTRY(Z24S8),
        PTE_KIND_TABLE_ENTRY(Z24S8_1Z),
        PTE_KIND_TABLE_ENTRY(Z24S8_MS2_1Z),
        PTE_KIND_TABLE_ENTRY(Z24S8_MS4_1Z),
        PTE_KIND_TABLE_ENTRY(Z24S8_MS8_1Z),
        PTE_KIND_TABLE_ENTRY(Z24S8_2CS),
        PTE_KIND_TABLE_ENTRY(Z24S8_MS2_2CS),
        PTE_KIND_TABLE_ENTRY(Z24S8_MS4_2CS),
        PTE_KIND_TABLE_ENTRY(Z24S8_MS8_2CS),
        PTE_KIND_TABLE_ENTRY(Z24S8_MS16_2CS),
        PTE_KIND_TABLE_ENTRY(Z24S8_2CZ),
        PTE_KIND_TABLE_ENTRY(Z24S8_MS2_2CZ),
        PTE_KIND_TABLE_ENTRY(Z24S8_MS4_2CZ),
        PTE_KIND_TABLE_ENTRY(Z24S8_MS8_2CZ),
        PTE_KIND_TABLE_ENTRY(Z24S8_MS16_2CZ),
        PTE_KIND_TABLE_ENTRY(Z24S8_4CSZV),
        PTE_KIND_TABLE_ENTRY(Z24S8_MS2_4CSZV),
        PTE_KIND_TABLE_ENTRY(Z24S8_MS4_4CSZV),
        PTE_KIND_TABLE_ENTRY(Z24S8_MS8_4CSZV),
        PTE_KIND_TABLE_ENTRY(Z24V8_MS4_VC12),
        PTE_KIND_TABLE_ENTRY(Z24V8_MS4_VC4),
        PTE_KIND_TABLE_ENTRY(Z24V8_MS8_VC8),
        PTE_KIND_TABLE_ENTRY(Z24V8_MS8_VC24),
        PTE_KIND_TABLE_ENTRY(Z24V8_MS4_VC12_1ZV),
        PTE_KIND_TABLE_ENTRY(Z24V8_MS4_VC4_1ZV),
        PTE_KIND_TABLE_ENTRY(Z24V8_MS8_VC8_1ZV),
        PTE_KIND_TABLE_ENTRY(Z24V8_MS8_VC24_1ZV),
        PTE_KIND_TABLE_ENTRY(Z24V8_MS4_VC12_2CS),
        PTE_KIND_TABLE_ENTRY(Z24V8_MS4_VC4_2CS),
        PTE_KIND_TABLE_ENTRY(Z24V8_MS8_VC8_2CS),
        PTE_KIND_TABLE_ENTRY(Z24V8_MS8_VC24_2CS),
        PTE_KIND_TABLE_ENTRY(Z24V8_MS4_VC12_2CZV),
        PTE_KIND_TABLE_ENTRY(Z24V8_MS4_VC4_2CZV),
        PTE_KIND_TABLE_ENTRY(Z24V8_MS8_VC8_2CZV),
        PTE_KIND_TABLE_ENTRY(Z24V8_MS8_VC24_2CZV),
        PTE_KIND_TABLE_ENTRY(Z24V8_MS4_VC12_2ZV),
        PTE_KIND_TABLE_ENTRY(Z24V8_MS4_VC4_2ZV),
        PTE_KIND_TABLE_ENTRY(Z24V8_MS8_VC8_2ZV),
        PTE_KIND_TABLE_ENTRY(Z24V8_MS8_VC24_2ZV),
        PTE_KIND_TABLE_ENTRY(Z24V8_MS4_VC12_4CSZV),
        PTE_KIND_TABLE_ENTRY(Z24V8_MS4_VC4_4CSZV),
        PTE_KIND_TABLE_ENTRY(Z24V8_MS8_VC8_4CSZV),
        PTE_KIND_TABLE_ENTRY(Z24V8_MS8_VC24_4CSZV),
        PTE_KIND_TABLE_ENTRY(X8Z24_X16V8S8_MS4_VC12),
        PTE_KIND_TABLE_ENTRY(X8Z24_X16V8S8_MS4_VC4),
        PTE_KIND_TABLE_ENTRY(X8Z24_X16V8S8_MS8_VC8),
        PTE_KIND_TABLE_ENTRY(X8Z24_X16V8S8_MS8_VC24),
        PTE_KIND_TABLE_ENTRY(X8Z24_X16V8S8_MS4_VC12_1CS),
        PTE_KIND_TABLE_ENTRY(X8Z24_X16V8S8_MS4_VC4_1CS),
        PTE_KIND_TABLE_ENTRY(X8Z24_X16V8S8_MS8_VC8_1CS),
        PTE_KIND_TABLE_ENTRY(X8Z24_X16V8S8_MS8_VC24_1CS),
        PTE_KIND_TABLE_ENTRY(X8Z24_X16V8S8_MS4_VC12_1ZV),
        PTE_KIND_TABLE_ENTRY(X8Z24_X16V8S8_MS4_VC4_1ZV),
        PTE_KIND_TABLE_ENTRY(X8Z24_X16V8S8_MS8_VC8_1ZV),
        PTE_KIND_TABLE_ENTRY(X8Z24_X16V8S8_MS8_VC24_1ZV),
        PTE_KIND_TABLE_ENTRY(X8Z24_X16V8S8_MS4_VC12_1CZV),
        PTE_KIND_TABLE_ENTRY(X8Z24_X16V8S8_MS4_VC4_1CZV),
        PTE_KIND_TABLE_ENTRY(X8Z24_X16V8S8_MS8_VC8_1CZV),
        PTE_KIND_TABLE_ENTRY(X8Z24_X16V8S8_MS8_VC24_1CZV),
        PTE_KIND_TABLE_ENTRY(X8Z24_X16V8S8_MS4_VC12_2CS),
        PTE_KIND_TABLE_ENTRY(X8Z24_X16V8S8_MS4_VC4_2CS),
        PTE_KIND_TABLE_ENTRY(X8Z24_X16V8S8_MS8_VC8_2CS),
        PTE_KIND_TABLE_ENTRY(X8Z24_X16V8S8_MS8_VC24_2CS),
        PTE_KIND_TABLE_ENTRY(X8Z24_X16V8S8_MS4_VC12_2CSZV),
        PTE_KIND_TABLE_ENTRY(X8Z24_X16V8S8_MS4_VC4_2CSZV),
        PTE_KIND_TABLE_ENTRY(X8Z24_X16V8S8_MS8_VC8_2CSZV),
        PTE_KIND_TABLE_ENTRY(X8Z24_X16V8S8_MS8_VC24_2CSZV),
        PTE_KIND_TABLE_ENTRY(GENERIC_16BX2)
    };
    for (UINT32 i=0; i < sizeof(entries)/sizeof(entries[0]); i++)
    {
        m_PteKindTable.push_back(entries[i]);
    }
}

//-----------------------------------------------------------------------------
void GpuSubdevice::AddPteKindsFloatDepth()
{
    static const PteKindTableEntry entries[] =
    {
        PTE_KIND_TABLE_ENTRY(ZF32),
        PTE_KIND_TABLE_ENTRY(ZF32_1Z),
        PTE_KIND_TABLE_ENTRY(ZF32_MS2_1Z),
        PTE_KIND_TABLE_ENTRY(ZF32_MS4_1Z),
        PTE_KIND_TABLE_ENTRY(ZF32_MS8_1Z),
        PTE_KIND_TABLE_ENTRY(ZF32_2CS),
        PTE_KIND_TABLE_ENTRY(ZF32_MS2_2CS),
        PTE_KIND_TABLE_ENTRY(ZF32_MS4_2CS),
        PTE_KIND_TABLE_ENTRY(ZF32_MS8_2CS),
        PTE_KIND_TABLE_ENTRY(ZF32_MS16_2CS),
        PTE_KIND_TABLE_ENTRY(ZF32_2CZ),
        PTE_KIND_TABLE_ENTRY(ZF32_MS2_2CZ),
        PTE_KIND_TABLE_ENTRY(ZF32_MS4_2CZ),
        PTE_KIND_TABLE_ENTRY(ZF32_MS8_2CZ),
        PTE_KIND_TABLE_ENTRY(ZF32_MS16_2CZ),
        PTE_KIND_TABLE_ENTRY(ZF32_X16V8S8_MS4_VC12),
        PTE_KIND_TABLE_ENTRY(ZF32_X16V8S8_MS4_VC4),
        PTE_KIND_TABLE_ENTRY(ZF32_X16V8S8_MS8_VC8),
        PTE_KIND_TABLE_ENTRY(ZF32_X16V8S8_MS8_VC24),
        PTE_KIND_TABLE_ENTRY(ZF32_X16V8S8_MS4_VC12_1CS),
        PTE_KIND_TABLE_ENTRY(ZF32_X16V8S8_MS4_VC4_1CS),
        PTE_KIND_TABLE_ENTRY(ZF32_X16V8S8_MS8_VC8_1CS),
        PTE_KIND_TABLE_ENTRY(ZF32_X16V8S8_MS8_VC24_1CS),
        PTE_KIND_TABLE_ENTRY(ZF32_X16V8S8_MS4_VC12_1ZV),
        PTE_KIND_TABLE_ENTRY(ZF32_X16V8S8_MS4_VC4_1ZV),
        PTE_KIND_TABLE_ENTRY(ZF32_X16V8S8_MS8_VC8_1ZV),
        PTE_KIND_TABLE_ENTRY(ZF32_X16V8S8_MS8_VC24_1ZV),
        PTE_KIND_TABLE_ENTRY(ZF32_X16V8S8_MS4_VC12_1CZV),
        PTE_KIND_TABLE_ENTRY(ZF32_X16V8S8_MS4_VC4_1CZV),
        PTE_KIND_TABLE_ENTRY(ZF32_X16V8S8_MS8_VC8_1CZV),
        PTE_KIND_TABLE_ENTRY(ZF32_X16V8S8_MS8_VC24_1CZV),
        PTE_KIND_TABLE_ENTRY(ZF32_X16V8S8_MS4_VC12_2CS),
        PTE_KIND_TABLE_ENTRY(ZF32_X16V8S8_MS4_VC4_2CS),
        PTE_KIND_TABLE_ENTRY(ZF32_X16V8S8_MS8_VC8_2CS),
        PTE_KIND_TABLE_ENTRY(ZF32_X16V8S8_MS8_VC24_2CS),
        PTE_KIND_TABLE_ENTRY(ZF32_X16V8S8_MS4_VC12_2CSZV),
        PTE_KIND_TABLE_ENTRY(ZF32_X16V8S8_MS4_VC4_2CSZV),
        PTE_KIND_TABLE_ENTRY(ZF32_X16V8S8_MS8_VC8_2CSZV),
        PTE_KIND_TABLE_ENTRY(ZF32_X16V8S8_MS8_VC24_2CSZV),
        PTE_KIND_TABLE_ENTRY(ZF32_X24S8),
        PTE_KIND_TABLE_ENTRY(ZF32_X24S8_1CS),
        PTE_KIND_TABLE_ENTRY(ZF32_X24S8_MS2_1CS),
        PTE_KIND_TABLE_ENTRY(ZF32_X24S8_MS4_1CS),
        PTE_KIND_TABLE_ENTRY(ZF32_X24S8_MS8_1CS),
        PTE_KIND_TABLE_ENTRY(ZF32_X24S8_2CSZV),
        PTE_KIND_TABLE_ENTRY(ZF32_X24S8_MS2_2CSZV),
        PTE_KIND_TABLE_ENTRY(ZF32_X24S8_MS4_2CSZV),
        PTE_KIND_TABLE_ENTRY(ZF32_X24S8_MS8_2CSZV),
        PTE_KIND_TABLE_ENTRY(ZF32_X24S8_MS16_2CSZV),
        PTE_KIND_TABLE_ENTRY(ZF32_X24S8_2CS),
        PTE_KIND_TABLE_ENTRY(ZF32_X24S8_MS2_2CS),
        PTE_KIND_TABLE_ENTRY(ZF32_X24S8_MS4_2CS),
        PTE_KIND_TABLE_ENTRY(ZF32_X24S8_MS8_2CS),
        PTE_KIND_TABLE_ENTRY(ZF32_X24S8_MS16_2CS)
    };
    for (UINT32 i=0; i < sizeof(entries)/sizeof(entries[0]); i++)
    {
        m_PteKindTable.push_back(entries[i]);
    }
}

//-----------------------------------------------------------------------------
void GpuSubdevice::AddPteKindsCompressed()
{
    static const PteKindTableEntry entries[] =
    {
        PTE_KIND_TABLE_ENTRY(C32_2C),
        PTE_KIND_TABLE_ENTRY(C32_2CBR),
        PTE_KIND_TABLE_ENTRY(C32_2CBA),
        PTE_KIND_TABLE_ENTRY(C32_2CRA),
        PTE_KIND_TABLE_ENTRY(C32_2BRA),
        PTE_KIND_TABLE_ENTRY(C32_MS2_2C),
        PTE_KIND_TABLE_ENTRY(C32_MS2_2CBR),
        PTE_KIND_TABLE_ENTRY(C32_MS2_2CRA),
        PTE_KIND_TABLE_ENTRY(C32_MS4_2C),
        PTE_KIND_TABLE_ENTRY(C32_MS4_2CBR),
        PTE_KIND_TABLE_ENTRY(C32_MS4_2CBA),
        PTE_KIND_TABLE_ENTRY(C32_MS4_2CRA),
        PTE_KIND_TABLE_ENTRY(C32_MS4_2BRA),
        PTE_KIND_TABLE_ENTRY(C32_MS8_MS16_2C),
        PTE_KIND_TABLE_ENTRY(C32_MS8_MS16_2CRA),
        PTE_KIND_TABLE_ENTRY(C64_2C),
        PTE_KIND_TABLE_ENTRY(C64_2CBR),
        PTE_KIND_TABLE_ENTRY(C64_2CBA),
        PTE_KIND_TABLE_ENTRY(C64_2CRA),
        PTE_KIND_TABLE_ENTRY(C64_2BRA),
        PTE_KIND_TABLE_ENTRY(C64_MS2_2C),
        PTE_KIND_TABLE_ENTRY(C64_MS2_2CBR),
        PTE_KIND_TABLE_ENTRY(C64_MS2_2CRA),
        PTE_KIND_TABLE_ENTRY(C64_MS4_2C),
        PTE_KIND_TABLE_ENTRY(C64_MS4_2CBR),
        PTE_KIND_TABLE_ENTRY(C64_MS4_2CBA),
        PTE_KIND_TABLE_ENTRY(C64_MS4_2CRA),
        PTE_KIND_TABLE_ENTRY(C64_MS4_2BRA),
        PTE_KIND_TABLE_ENTRY(C64_MS8_MS16_2C),
        PTE_KIND_TABLE_ENTRY(C64_MS8_MS16_2CRA),
        PTE_KIND_TABLE_ENTRY(C128_2C),
        PTE_KIND_TABLE_ENTRY(C128_2CR),
        PTE_KIND_TABLE_ENTRY(C128_MS2_2C),
        PTE_KIND_TABLE_ENTRY(C128_MS2_2CR),
        PTE_KIND_TABLE_ENTRY(C128_MS4_2C),
        PTE_KIND_TABLE_ENTRY(C128_MS4_2CR),
        PTE_KIND_TABLE_ENTRY(C128_MS8_MS16_2C),
        PTE_KIND_TABLE_ENTRY(C128_MS8_MS16_2CR),
        PTE_KIND_TABLE_ENTRY(X8C24)
    };
    for (UINT32 i=0; i < sizeof(entries)/sizeof(entries[0]); i++)
    {
        m_PteKindTable.push_back(entries[i]);
    }
}

//-----------------------------------------------------------------------------
// See bug 598305
RC GpuSubdevice::GetAteSpeedo
(
    UINT32 SpeedoNum /*unused*/,
    UINT32 *pSpeedo,
    UINT32 *pVersion
)
{
    unique_ptr<ScopedRegister> debug1;
    if (Regs().IsSupported(MODS_PTOP_DEBUG_1_FUSES_VISIBLE))
    {
        debug1 = make_unique<ScopedRegister>(this, Regs().LookupAddress(MODS_PTOP_DEBUG_1));
        Regs().Write32(MODS_PTOP_DEBUG_1_FUSES_VISIBLE_ENABLED);
    }

    UINT32 SpeedoData = Regs().Read32(MODS_FUSE_OPT_PDELAY_DATA);

    if (SpeedoData == 0)
    {
        return RC::UNSUPPORTED_FUNCTION;
    }
    else
    {
        RC rc;
        MASSERT(pSpeedo && pVersion);
        *pSpeedo = SpeedoData * (HasBug(598305) ? 2 : 1);
        UINT32 DummyIddq;
        CHECK_RC(GetAteIddq(&DummyIddq, pVersion));
    }

    return OK;
}

//------------------------------------------------------------------------------
/* virtual */ RC GpuSubdevice::GetAteSpeedos(vector<UINT32>* pValues, UINT32* pVersion)
{
    MASSERT(pValues);
    MASSERT(pVersion);
    pValues->clear();

    UINT32 speedo = 0;
    UINT32 version = 0;
    bool bAtLeastOneGetSuccessful = false;
    for (UINT32 i = 0; i < NumAteSpeedo(); i++)
    {
        if (OK == GetAteSpeedo(i, &speedo, &version))
        {
            bAtLeastOneGetSuccessful = true;
        }
        else
        {
            speedo = 0;
        }
        pValues->push_back(speedo);
    }
    *pVersion = version;
    if (!bAtLeastOneGetSuccessful)
        return RC::UNSUPPORTED_FUNCTION;
    return OK;
}

//------------------------------------------------------------------------------
/* virtual */ void GpuSubdevice::ResetEdcErrors()
{
    /* Per FBPA reset edc error counts
     * Initialise the value of edc crc rate limit too.
     * This is to measure enable logging of edc errors generated in the
     * following mods test only.
     * This might lead to clearing of edc errors before saturation of edc
     * error limit is reached.
     */
    if ((Regs().IsSupported(MODS_PFB_FBPA_0_CRC_ERROR_CNT)) &&
        (HasBug(2355623) || Regs().HasRWAccess(MODS_PFB_FBPA_0_CRC)))
    {
        for (UINT32 virtFbio = 0; virtFbio < GetFB()->GetFbioCount(); virtFbio++)
        {
            const bool isHalfFbpa = GetFB()->IsHalfFbpaEn(virtFbio);
            const UINT32 hwFbio = GetFB()->VirtualFbioToHwFbio(virtFbio);
            Regs().Write32(MODS_PFB_FBPA_0_CRC_RESET_ERROR_CNT_ENABLE, hwFbio);

            // Other counters share the same format as the total counter
            if (Regs().IsSupported(MODS_PFB_FBPA_0_CRC_RD))
            {
                // This register isn't self-clearing, but it only clears the counters when written to
                Regs().Write32(MODS_PFB_FBPA_0_CRC_RD_RESET_ERROR_CNT_ENABLE, hwFbio);
            }
            if (Regs().IsSupported(MODS_PFB_FBPA_0_CRC_WR))
            {
                // This register isn't self-clearing, but it only clears the counters when written to
                Regs().Write32(MODS_PFB_FBPA_0_CRC_WR_RESET_ERROR_CNT_ENABLE, hwFbio);
            }
            if (!isHalfFbpa &&
                Regs().IsSupported(MODS_PFB_FBPA_0_REPLAY_CYCLES_SUBP0))
            {
                // Toggle registers since they are supposed to be self-clearing, but aren't
                Regs().Write32(MODS_PFB_FBPA_0_REPLAY_CYCLES_SUBP0_RESET_MAX_CNT_ENABLE, hwFbio);
                Regs().Write32(MODS_PFB_FBPA_0_REPLAY_CYCLES_SUBP0_RESET_MAX_CNT_DISABLE, hwFbio);
            }
            if (!isHalfFbpa &&
                Regs().IsSupported(MODS_PFB_FBPA_0_REPLAY_RETRY_SUBP0))
            {
                // Toggle registers since they are supposed to be self-clearing, but aren't
                Regs().Write32(MODS_PFB_FBPA_0_REPLAY_RETRY_SUBP0_RESET_MAX_CNT_ENABLE, hwFbio);
                Regs().Write32(MODS_PFB_FBPA_0_REPLAY_RETRY_SUBP0_RESET_MAX_CNT_DISABLE, hwFbio);
            }
            if (Regs().IsSupported(MODS_PFB_FBPA_0_REPLAY_CYCLES_SUBP1))
            {
                // Toggle registers since they are supposed to be self-clearing, but aren't
                Regs().Write32(MODS_PFB_FBPA_0_REPLAY_CYCLES_SUBP1_RESET_MAX_CNT_ENABLE, hwFbio);
                Regs().Write32(MODS_PFB_FBPA_0_REPLAY_CYCLES_SUBP1_RESET_MAX_CNT_DISABLE, hwFbio);
            }
            if (Regs().IsSupported(MODS_PFB_FBPA_0_REPLAY_RETRY_SUBP1))
            {
                // Toggle registers since they are supposed to be self-clearing, but aren't
                Regs().Write32(MODS_PFB_FBPA_0_REPLAY_RETRY_SUBP1_RESET_MAX_CNT_ENABLE, hwFbio);
                Regs().Write32(MODS_PFB_FBPA_0_REPLAY_RETRY_SUBP1_RESET_MAX_CNT_DISABLE, hwFbio);
            }
            // Clears CRC_MAX_DELTA
            if (Regs().IsSupported(MODS_PFB_FBPA_0_CRC_ERROR_RATE))
            {
                // Self-clearing
                Regs().Write32(MODS_PFB_FBPA_0_CRC_ERROR_RATE_EXCEEDED_CLEAR, hwFbio);
            }
            // It's unclear if the byte counters are reset elsewhere, so explicitly zero them
            if (Regs().IsSupported(MODS_PFB_FBPA_0_CRC_ERROR_COUNT_RD_BYTE))
            {
                for (UINT32 i = 0;
                     i < Regs().LookupAddress(MODS_PFB_FBPA_0_CRC_ERROR_COUNT_RD_BYTE__SIZE_1);
                     i++)
                {
                    Regs().Write32(MODS_PFB_FBPA_0_CRC_ERROR_COUNT_RD_BYTE_ERRCNT_INIT, {hwFbio, i});
                    Regs().Write32(MODS_PFB_FBPA_0_CRC_ERROR_COUNT_WR_BYTE_ERRCNT_INIT, {hwFbio, i});
                }
            }
        }
    }

    JavaScriptPtr pJs;
    JSObject *pGlobalObject = pJs->GetGlobalObject();
    jsval jv;
    if (Regs().IsSupported(MODS_PFB_FBPA_0_CRC_ERROR_RATE_LIMIT) &&
        OK == pJs->GetPropertyJsval(pGlobalObject, "g_EdcRateLimit", &jv) &&
        (JSVAL_IS_NUMBER(jv) || JSVAL_IS_STRING(jv)))
    {
        UINT32 limit;
        const UINT32 maxLimit = Regs().LookupMask(MODS_PFB_FBPA_0_CRC_ERROR_RATE_LIMIT);
        if (OK == pJs->FromJsval(jv, &limit) && limit <= maxLimit)
        {
            for (UINT32 virtFbio = 0; virtFbio < GetFB()->GetFbioCount(); virtFbio++)
            {
                const UINT32 hwFbio = GetFB()->VirtualFbioToHwFbio(virtFbio);
                Regs().Write32(MODS_PFB_FBPA_0_CRC_ERROR_RATE_LIMIT, hwFbio, limit);
            }
        }
        else
        {
            Printf(Tee::PriWarn, "Suggested edc rate limit is invalid\n");
        }
    }
}

// Clear the EDC Max Delta Register (priv register) if needed.
RC GpuSubdevice::ClearEdcMaxDelta(UINT32 virtualFbio)
{
    RC rc;
    const UINT32 hwFbio = GetFB()->VirtualFbioToHwFbio(virtualFbio);

    if (!Regs().IsSupported(MODS_PFB_FBPA_0_CRC_ERROR_CNT))
    {
        return RC::UNSUPPORTED_FUNCTION;
    }

    // Check if running with priv access. If so clear CRC_MAX_DELTA. If not return an error.
    if (Regs().IsSupported(MODS_PFB_FBPA_0_CRC_ERROR_RATE) && Regs().HasRWAccess(MODS_PFB_FBPA_0_CRC_ERROR_RATE))
    {
        // Self-clearing
        Regs().Write32(MODS_PFB_FBPA_0_CRC_ERROR_RATE_EXCEEDED_CLEAR, hwFbio);
    }
    else
    {
        Printf(Tee::PriError, "Do not have priv access, can not clear the Max Delta Regitser\n");
        return RC::PRIV_LEVEL_VIOLATION;
    }
    return RC::OK;
}

//------------------------------------------------------------------------------
/* virtual */ RC GpuSubdevice::GetEdcDebugInfo
(
    UINT32 virtFbio,
    ExtEdcInfo *pResult
)
{
    RC rc;
    const UINT32 hwFbio = GetFB()->VirtualFbioToHwFbio(virtFbio);

    *pResult = {};
    pResult->EdcErrType = static_cast<UINT08>(Regs().Read32(MODS_PFB_FBPA_0_CRC_DEBUG, hwFbio));
    if (Regs().IsSupported(MODS_PFB_FBPA_0_CRC_ERROR_RATE_EXCEEDED_YES))
    {
        pResult->EdcRateExceeded = Regs().Test32(MODS_PFB_FBPA_0_CRC_ERROR_RATE_EXCEEDED_YES, hwFbio);
    }
    if (Regs().IsSupported(MODS_PFB_FBPA_0_CRC_TRIGGER_BYTE_NUM))
    {
        pResult->EdcTrgByteNum = Regs().Read32(MODS_PFB_FBPA_0_CRC_TRIGGER_BYTE_NUM, hwFbio);
    }
    return rc;
}

//------------------------------------------------------------------------------
/* virtual */ RC GpuSubdevice::GetEdcDirectCounts
(
    UINT32 virtFbio,
    bool supported[EdcErrCounter::NUM_EDC_ERROR_RECORDS],
    UINT64 counts[EdcErrCounter::NUM_EDC_ERROR_RECORDS]
)
{
    RC rc;
    const bool isHalfFbpa = GetFB()->IsHalfFbpaEn(virtFbio);
    const UINT32 hwFbio = GetFB()->VirtualFbioToHwFbio(virtFbio);

    if (!Regs().IsSupported(MODS_PFB_FBPA_0_CRC_ERROR_CNT))
    {
        return RC::UNSUPPORTED_FUNCTION;
    }

    supported[EdcErrCounter::TOTAL_EDC_ERRORS_RECORD] = true;
    counts[EdcErrCounter::TOTAL_EDC_ERRORS_RECORD] = Regs().Read32(MODS_PFB_FBPA_0_CRC_ERROR_CNT, hwFbio);
    if (Regs().IsSupported(MODS_PFB_FBPA_0_CRC_RD))
    {
        supported[EdcErrCounter::READ_EDC_ERRORS_RECORD] = true;
        counts[EdcErrCounter::READ_EDC_ERRORS_RECORD] =
            Regs().Read32(MODS_PFB_FBPA_0_CRC_RD_ERROR_CNT, hwFbio);
    }
    if (Regs().IsSupported(MODS_PFB_FBPA_0_CRC_WR))
    {
        supported[EdcErrCounter::WRITE_EDC_ERRORS_RECORD] = true;
        counts[EdcErrCounter::WRITE_EDC_ERRORS_RECORD] =
            Regs().Read32(MODS_PFB_FBPA_0_CRC_WR_ERROR_CNT, hwFbio);
    }
    if (!isHalfFbpa &&
        Regs().IsSupported(MODS_PFB_FBPA_0_REPLAY_CYCLES_SUBP0))
    {
        supported[EdcErrCounter::MAX_REPLAY_CYCLES_SUBP0_EDC_RECORD] = true;
        counts[EdcErrCounter::MAX_REPLAY_CYCLES_SUBP0_EDC_RECORD] =
            Regs().Read32(MODS_PFB_FBPA_0_REPLAY_CYCLES_SUBP0_MAX_CNT, hwFbio);
    }
    if (!isHalfFbpa &&
        Regs().IsSupported(MODS_PFB_FBPA_0_REPLAY_RETRY_SUBP0))
    {
        supported[EdcErrCounter::MAX_REPLAY_RETRY_SUBP0_EDC_RECORD] = true;
        counts[EdcErrCounter::MAX_REPLAY_RETRY_SUBP0_EDC_RECORD] =
            Regs().Read32(MODS_PFB_FBPA_0_REPLAY_RETRY_SUBP0_MAX_CNT, hwFbio);
    }
    if (Regs().IsSupported(MODS_PFB_FBPA_0_REPLAY_CYCLES_SUBP1))
    {
        supported[EdcErrCounter::MAX_REPLAY_CYCLES_SUBP1_EDC_RECORD] = true;
        counts[EdcErrCounter::MAX_REPLAY_CYCLES_SUBP1_EDC_RECORD] =
            Regs().Read32(MODS_PFB_FBPA_0_REPLAY_CYCLES_SUBP1_MAX_CNT, hwFbio);
    }
    if (Regs().IsSupported(MODS_PFB_FBPA_0_REPLAY_RETRY_SUBP1))
    {
        supported[EdcErrCounter::MAX_REPLAY_RETRY_SUBP1_EDC_RECORD] = true;
        counts[EdcErrCounter::MAX_REPLAY_RETRY_SUBP1_EDC_RECORD] =
            Regs().Read32(MODS_PFB_FBPA_0_REPLAY_RETRY_SUBP1_MAX_CNT, hwFbio);
    }
    if (Regs().IsSupported(MODS_PFB_FBPA_0_CRC_ERROR_MAX_DELTA))
    {
        supported[EdcErrCounter::MAX_DELTA_EDC_RECORD] = true;
        counts[EdcErrCounter::MAX_DELTA_EDC_RECORD] =
            Regs().Read32(MODS_PFB_FBPA_0_CRC_ERROR_MAX_DELTA_VALUE, hwFbio);
    }
    if (Regs().IsSupported(MODS_PFB_FBPA_0_CRC_TICK))
    {
        supported[EdcErrCounter::CRC_TICK_EDC_RECORD] = true;
        counts[EdcErrCounter::CRC_TICK_EDC_RECORD] =
            Regs().Read32(MODS_PFB_FBPA_0_CRC_TICK_VALUE, hwFbio);
    }
    if (Regs().IsSupported(MODS_PFB_FBPA_0_CRC_ERROR_RATE_TICK_CLOCK))
    {
        supported[EdcErrCounter::CRC_TICK_CLOCK_EDC_RECORD] = true;
        counts[EdcErrCounter::CRC_TICK_CLOCK_EDC_RECORD] =
            Regs().Read32(MODS_PFB_FBPA_0_CRC_ERROR_RATE_TICK_CLOCK, hwFbio);
    }
    if (Regs().IsSupported(MODS_PFB_FBPA_0_CRC_ERROR_COUNT_RD_BYTE))
    {
        for (UINT32 i = 0;
             i < Regs().LookupAddress(MODS_PFB_FBPA_0_CRC_ERROR_COUNT_RD_BYTE__SIZE_1);
             i++)
        {
            MASSERT(Regs().LookupAddress(MODS_PFB_FBPA_0_CRC_ERROR_COUNT_RD_BYTE__SIZE_1) == 8);
            MASSERT(EdcErrCounter::BYTES_READ_EDC_ERROR_RECORD_BASE + i <=
                    EdcErrCounter::BYTES_READ_EDC_ERROR_RECORD_MAX);
            MASSERT(EdcErrCounter::BYTES_WRITE_EDC_ERROR_RECORD_BASE + i <=
                    EdcErrCounter::BYTES_WRITE_EDC_ERROR_RECORD_MAX);

            // In half-fbpa mode bytes 0-3 correspond to SUBP0, which is floorswept
            if (isHalfFbpa && i < 4)
            {
                continue;
            }
            supported[EdcErrCounter::BYTES_READ_EDC_ERROR_RECORD_BASE + i] = true;
            supported[EdcErrCounter::BYTES_WRITE_EDC_ERROR_RECORD_BASE + i] = true;
            counts[EdcErrCounter::BYTES_READ_EDC_ERROR_RECORD_BASE + i] =
                Regs().Read32(MODS_PFB_FBPA_0_CRC_ERROR_COUNT_RD_BYTE_ERRCNT, {hwFbio, i});
            counts[EdcErrCounter::BYTES_WRITE_EDC_ERROR_RECORD_BASE + i] =
                Regs().Read32(MODS_PFB_FBPA_0_CRC_ERROR_COUNT_WR_BYTE_ERRCNT, {hwFbio, i});
        }
    }

    supported[EdcErrCounter::MAX_REPLAY_RETRY_EDC_RECORD] =
        supported[EdcErrCounter::MAX_REPLAY_RETRY_SUBP0_EDC_RECORD] ||
        supported[EdcErrCounter::MAX_REPLAY_RETRY_SUBP1_EDC_RECORD];

    counts[EdcErrCounter::MAX_REPLAY_RETRY_EDC_RECORD] =
        max(counts[EdcErrCounter::MAX_REPLAY_RETRY_SUBP0_EDC_RECORD],
            counts[EdcErrCounter::MAX_REPLAY_RETRY_SUBP1_EDC_RECORD]);

    return rc;
}

//------------------------------------------------------------------------------
/* virtual */ void GpuSubdevice::ClearEdcDebug(UINT32 virtualFbio)
{
    UINT32 resetVal = 0;
    /* Reset LW_PFB_FBPA_n_CRC_DEBUG */
    const bool isHalfFbpa = GetFB()->IsHalfFbpaEn(virtualFbio);
    if (!isHalfFbpa)
    {
        Regs().SetField(&resetVal, MODS_PFB_FBPA_0_CRC_DEBUG_SUBP0_RD_ERR_CLEAR);
        Regs().SetField(&resetVal, MODS_PFB_FBPA_0_CRC_DEBUG_SUBP0_RD_RPL_CLEAR);
        Regs().SetField(&resetVal, MODS_PFB_FBPA_0_CRC_DEBUG_SUBP0_WR_ERR_CLEAR);
        Regs().SetField(&resetVal, MODS_PFB_FBPA_0_CRC_DEBUG_SUBP0_WR_RPL_CLEAR);
    }
    Regs().SetField(&resetVal, MODS_PFB_FBPA_0_CRC_DEBUG_SUBP1_RD_ERR_CLEAR);
    Regs().SetField(&resetVal, MODS_PFB_FBPA_0_CRC_DEBUG_SUBP1_RD_RPL_CLEAR);
    Regs().SetField(&resetVal, MODS_PFB_FBPA_0_CRC_DEBUG_SUBP1_WR_ERR_CLEAR);
    Regs().SetField(&resetVal, MODS_PFB_FBPA_0_CRC_DEBUG_SUBP1_WR_RPL_CLEAR);

    // Reset all
    const UINT32 hwFbio = GetFB()->VirtualFbioToHwFbio(virtualFbio);
    Regs().Write32(MODS_PFB_FBPA_0_CRC_DEBUG, hwFbio, resetVal);
}

//  Helper routine to read chip revision without RM support
/* virtual */ UINT32 GpuSubdevice::RevisionWithoutRm()
{
        // Read out the dev_master registers out directly
        UINT32 MajorRevision = Regs().Read32(MODS_PMC_BOOT_0_MAJOR_REVISION);
        UINT32 MinorRevision = Regs().Read32(MODS_PMC_BOOT_0_MINOR_REVISION);

        UINT32 majShift, minShift;
        Regs().LookupField(MODS_PMC_BOOT_0_MAJOR_REVISION, nullptr, &majShift);
        Regs().LookupField(MODS_PMC_BOOT_0_MINOR_REVISION, nullptr, &minShift);

        return (MajorRevision << majShift |
                MinorRevision << minShift);
}

//  Helper routine to read engineering chip revision without RM support
/* virtual */ UINT32 GpuSubdevice::PrivateRevisionWithoutRm()
{
       // Read out the dev_master registers out directly
        UINT32 MajorRevision = Regs().Read32(MODS_PMC_BOOT_0_MAJOR_REVISION);
        UINT32 MinorRevision = Regs().Read32(MODS_PMC_BOOT_0_MINOR_REVISION);

        UINT32 MinorExtdRevision =
            Regs().Read32(MODS_PMC_BOOT_2_MINOR_EXTENDED_REVISION);

        return (MajorRevision << 16 |
                MinorRevision << 8  |
                MinorExtdRevision);
}

/* virtual */ RC GpuSubdevice::DoEndOfSimCheck(Gpu::ShutDownMode mode)
{
    RC rc;

#ifdef SIM_BUILD
    if ((Platform::GetSimulationMode() == Platform::RTL) &&
        !Platform::IsVirtFunMode() &&
        GetEosAssert() &&
        !GetParentDevice()->GetResetInProgress())
    {
        if (GpuPtr()->IsInitSkipped())
        {
            Printf(Tee::PriWarn,
                   "Chip is not initialized.  Skip EOS Assertion.\n");
            Platform::EscapeWrite("EndOfSimAssertDisable", 0, 32, 1);
            return rc;

        }
        CHECK_RC(PrepareEndOfSimCheck(mode));

        // Assert an EndOfSim signal before RM free the device/subdevice
        Printf(Tee::PriNormal, "Asserting EndOfSim. There should be no "
            "outstanding credits during this time.\n");
        Platform::EscapeWrite("EndOfSimChecks", 0, 4,
            Regs().SetField(MODS_PTOP_END_OF_SIM_REACHED_ASSERT));
        EndOfSimDelay();   // Delay before deasserting this signal

        Printf(Tee::PriNormal, "Deasserting EndOfSim. Simulation can continue."
               " RM will proceed to shutdown the GPU.\n");
        Platform::EscapeWrite("EndOfSimChecks", 0, 4,
            Regs().SetField(MODS_PTOP_END_OF_SIM_REACHED_DEASSERT));
    }
#endif

    return rc;
}

//-----------------------------------------------------------------------------
// Return a vector of offset, value pairs for all of the strap registers.
/*virtual */ RC GpuSubdevice::GetStraps
(
    vector<pair<UINT32, UINT32>>* pStraps
)
{
    pair <UINT32, UINT32> boot0(Regs().LookupAddress(MODS_PEXTDEV_BOOT_0),
                                Regs().Read32(MODS_PEXTDEV_BOOT_0));
    pair <UINT32, UINT32> boot3(Regs().LookupAddress(MODS_PEXTDEV_BOOT_3),
                                Regs().Read32(MODS_PEXTDEV_BOOT_3));
    pStraps->clear();
    pStraps->push_back(boot0);
    pStraps->push_back(boot3);
    return OK;
}

//-----------------------------------------------------------------------------
/*virtual*/ void GpuSubdevice::EnablePwrBlock()
{
    Regs().Write32(MODS_PMC_ENABLE_PWR_ENABLED);
}

//-----------------------------------------------------------------------------
/*virtual*/ void GpuSubdevice::DisablePwrBlock()
{
    Regs().Write32(MODS_PMC_ENABLE_PWR_DISABLED);
}

//-----------------------------------------------------------------------------
/*virtual*/ bool GpuSubdevice::IsXbarSecondaryPortSupported() const
{
    return Regs().IsSupported(MODS_PLTCG_LTC0_LTS0_DSTG_CFG2_XBAR_PORT1_ENABLE);
}

//-----------------------------------------------------------------------------
/*virtual*/ bool GpuSubdevice::IsXbarSecondaryPortConfigurable() const
{
    return IsXbarSecondaryPortSupported();
}

//-----------------------------------------------------------------------------
/*virtual*/ bool GpuSubdevice::IsPgraphRegister(UINT32 offset) const
{
    return (offset >= DRF_BASE(LW_PGRAPH)) && (offset <= DRF_EXTENT(LW_PGRAPH));
}
//-----------------------------------------------------------------------------
/*virtual*/ bool GpuSubdevice::PollAllChannelsIdle(LwRm *pLwRm)
{
    return Regs().Test32(MODS_PFIFO_SCHED_STATUS_ALL_CHANNELS_IDLE);
}

//-----------------------------------------------------------------------------
/* virtual */ UINT32 GpuSubdevice::GetDomainBase
(
    UINT32       domainIdx,
    RegHalDomain domain,
    LwRm*        pLwRm
)
{
    switch (domain)
    {
        case RegHalDomain::XVE:
            return DEVICE_BASE(LW_PCFG);
        default:
           break;
    }
    return 0;
}

RC GpuSubdevice::AddTempCtrl
(
    UINT32 id,
    string name,
    UINT32 min,
    UINT32 max,
    UINT32 numInst,
    string units,
    string interfaceType,
    JSObject *pInterfaceParams
)
{
    RC rc;

    if (m_pTempCtrl.find(id) != m_pTempCtrl.end())
    {
        Printf(Tee::PriError, "Controller Id %u already exists\n", id);
        return RC::BAD_PARAMETER;
    }

    TempCtrlPtr newCtrl;
    if (interfaceType == "IPMI")
    {
        newCtrl = std::make_shared<IpmiTempCtrl>(id, name, min, max, numInst, units);
    }
    else
    {
        Printf(Tee::PriError, "Interface type %s not supported\n", interfaceType.c_str());
        return RC::BAD_PARAMETER;
    }
    CHECK_RC(newCtrl->InitGetSetParams(pInterfaceParams));
    m_pTempCtrl[id] = newCtrl;

    return OK;
}

TempCtrlPtr GpuSubdevice::GetTempCtrlViaId(UINT32 id)
{
    TempCtrlMap::iterator it;
    it = m_pTempCtrl.find(id);
    if (it == m_pTempCtrl.end())
    {
        return nullptr;
    }
    return it->second;
}

//-----------------------------------------------------------------------------
/* static */ string GpuSubdevice::HwDevTypeToString(GpuSubdevice::HwDevType hwType)
{
    switch (hwType)
    {
        case GpuSubdevice::HwDevType::HDT_GRAPHICS : return "GRAPHICS";
        case GpuSubdevice::HwDevType::HDT_LWDEC    : return "LWDEC";
        case GpuSubdevice::HwDevType::HDT_LWENC    : return "LWENC";
        case GpuSubdevice::HwDevType::HDT_LWJPG    : return "LWJPG";
        case GpuSubdevice::HwDevType::HDT_OFA      : return "OFA";
        case GpuSubdevice::HwDevType::HDT_SEC      : return "SEC";
        case GpuSubdevice::HwDevType::HDT_LCE      : return "LCE";
        case GpuSubdevice::HwDevType::HDT_GSP      : return "GSP";
        case GpuSubdevice::HwDevType::HDT_IOCTRL   : return "IOCTRL";
        case GpuSubdevice::HwDevType::HDT_FLA      : return "FLA";
        case GpuSubdevice::HwDevType::HDT_C2C      : return "C2C";
        case GpuSubdevice::HwDevType::HDT_HSHUB    : return "HSHUB";
        case GpuSubdevice::HwDevType::HDT_FSP      : return "FSP";
        default: break;
    }
    return "UNKNOWN";
}

//-----------------------------------------------------------------------------
/* static */ GpuSubdevice::HwDevType GpuSubdevice::StringToHwDevType(const string& hwTypeStr)
{
    HwDevType hdt = static_cast<HwDevType>(0);
    while (hdt < HwDevType::HDT_ILWALID)
    {
        if (HwDevTypeToString(hdt) == hwTypeStr)
        {
            return hdt;
        }
        hdt = static_cast<HwDevType>(static_cast<int>(hdt) + 1);
    }

    Printf(Tee::PriWarn, "Invalid HwDev type (%s)\n", hwTypeStr.c_str());
    return hdt;
}

//-----------------------------------------------------------------------------
void GpuSubdevice::HwDevInfo::Print(Tee::Priority pri)
{
    Printf(pri, "\n----------------------------------------\n"
                "Device Type      : %s\n"
                "Device Instance  : %u\n"
                "Fault Id         : %u (%s)\n"
                "Reset Id         : %u (%s)\n"
                "Pri Base         : 0x%08x\n"
                "Engine           : %s\n"
                "Runlist ID       : %u\n"
                "Runlist Pri Base : 0x%08x\n"
                "HW Engine ID     : 0x%x\n"
                "----------------------------------------\n",
           GpuSubdevice::HwDevTypeToString(hwType).c_str(),
           hwInstance,
           faultId,
           bFaultIdValid ? "Valid" : "Invalid",
           resetId,
           bResetIdValid ? "Valid" : "Invalid",
           priBase,
           bIsEngine ? "Yes" : "No",
           runlistId,
           runlistPriBase,
           hwEngineId);
}

//-----------------------------------------------------------------------------
namespace
{
    struct RegTypeToEnumEntry
    {
        UINT32                  regEnumValue;
        GpuSubdevice::HwDevType enumType;
    };
    static map<GpuDevice::GpuFamily, vector<RegTypeToEnumEntry>> s_PerFamilyRegDevTypeToEnum;
}

//------------------------------------------------------------------------------
void GpuSubdevice::InitHwDevInfo()
{
    RegHal& regs = GpuSubdevice::Regs();

    if (!regs.IsSupported(MODS_PTOP_DEVICE_INFO_CFG) || !regs.IsSupported(MODS_PTOP_DEVICE_INFO2))
    {
        // The register may not be supported (most likely cause is running in VF mods)
        Printf(Tee::PriLow, "Unable to get hardware device info!");
        return;
    }

    const UINT32 numRows       = regs.Read32(MODS_PTOP_DEVICE_INFO_CFG_NUM_ROWS);
    const UINT32 maxRowsPerDev = regs.Read32(MODS_PTOP_DEVICE_INFO_CFG_MAX_ROWS_PER_DEVICE);
    vector<UINT32> rowData(maxRowsPerDev, 0);
    bool bInChain = false;
    UINT32 lwrRowIndex = 0;
    for (UINT32 lwrRow = 0; lwrRow < numRows; lwrRow++)
    {
        const UINT32 devInfo = regs.Read32(MODS_PTOP_DEVICE_INFO2, lwrRow);

        if (!bInChain && regs.TestField(devInfo, MODS_PTOP_DEVICE_INFO2_ROW_VALUE_ILWALID))
            continue;

        MASSERT(lwrRowIndex < maxRowsPerDev);
        rowData[lwrRowIndex++] = devInfo;

        bInChain = regs.TestField(devInfo, MODS_PTOP_DEVICE_INFO2_ROW_CHAIN_MORE);

        // When in chain transitions from true to false we just read the last row
        // of the device info for the current device
        if (!bInChain)
        {
            HwDevInfo newDev;
            UINT32 hiBit, loBit, lwrValue;

            regs.LookupField(MODS_PTOP_DEVICE_INFO2_DEV_TYPE_ENUM, &hiBit, &loBit);
            newDev.hwType = RegDevTypeToEnum(Utility::ExtractBitsFromU32Vector<UINT32>(rowData,
                                                                                       hiBit,
                                                                                       loBit));
            if (newDev.hwType == HwDevType::HDT_ILWALID)
            {
                lwrRowIndex = 0;
                rowData.assign(rowData.size(), 0);
                continue;
            }

            regs.LookupField(MODS_PTOP_DEVICE_INFO2_DEV_INSTANCE_ID, &hiBit, &loBit);
            newDev.hwInstance =
                Utility::ExtractBitsFromU32Vector<UINT32>(rowData, hiBit, loBit);

            regs.LookupField(MODS_PTOP_DEVICE_INFO2_DEV_FAULT_ID, &hiBit, &loBit);
            lwrValue = Utility::ExtractBitsFromU32Vector<UINT32>(rowData, hiBit, loBit);
            if (lwrValue != regs.LookupValue(MODS_PTOP_DEVICE_INFO2_DEV_FAULT_ID_ILWALID))
            {
                newDev.faultId        = lwrValue;
                newDev.bFaultIdValid  = true;
            }

            regs.LookupField(MODS_PTOP_DEVICE_INFO2_DEV_RESET_ID, &hiBit, &loBit);
            lwrValue = Utility::ExtractBitsFromU32Vector<UINT32>(rowData, hiBit, loBit);
            if (lwrValue != regs.LookupValue(MODS_PTOP_DEVICE_INFO2_DEV_RESET_ID_ILWALID))
            {
                newDev.resetId        = lwrValue;
                newDev.bResetIdValid  = true;
            }

            regs.LookupField(MODS_PTOP_DEVICE_INFO2_DEV_DEVICE_PRI_BASE, &hiBit, &loBit);
            lwrValue = Utility::ExtractBitsFromU32Vector<UINT32>(rowData, hiBit, loBit);
            newDev.priBase = lwrValue << (loBit & 0x1F);

            regs.LookupField(MODS_PTOP_DEVICE_INFO2_DEV_IS_ENGINE, &hiBit, &loBit);
            newDev.bIsEngine =
                Utility::ExtractBitsFromU32Vector<UINT32>(rowData, hiBit, loBit) ==
                    regs.LookupValue(MODS_PTOP_DEVICE_INFO2_DEV_IS_ENGINE_TRUE);

            regs.LookupField(MODS_PTOP_DEVICE_INFO2_DEV_RLENG_ID, &hiBit, &loBit);
            newDev.runlistId =
                Utility::ExtractBitsFromU32Vector<UINT32>(rowData, hiBit, loBit);

            regs.LookupField(MODS_PTOP_DEVICE_INFO2_DEV_RUNLIST_PRI_BASE, &hiBit, &loBit);
            lwrValue = Utility::ExtractBitsFromU32Vector<UINT32>(rowData, hiBit, loBit);
            newDev.runlistPriBase = lwrValue << (loBit & 0x1F);

            if (newDev.bIsEngine)
            {
                const UINT32 offset = newDev.runlistPriBase +
                    regs.LookupAddress(MODS_RUNLIST_ENGINE_STATUS_DEBUG, newDev.runlistId);

                // We dont want to use Regs to read this offset directly because Regs will
                // potentially use RM to read the register which will operate on HW that is
                // virtualized where this value needs to be the non-virtualized
                //
                // In addition the RegHal interface expects that domainIdx for the MODS_RUNLIST
                // space to be the logical engine since that is what is used in all other places
                // in MODS which is not available here
                //
                newDev.hwEngineId =
                    regs.GetField(RegRd32(offset), MODS_RUNLIST_ENGINE_STATUS_DEBUG_ENGINE_ID);
            }

            m_HwDevInfo.push_back(newDev);

            newDev.Print(Tee::PriLow);

            lwrRowIndex = 0;
            rowData.assign(rowData.size(), 0);
        }
    }
}

const vector<GpuSubdevice::HwDevInfo> & GpuSubdevice::GetHwDevInfo()
{
    if (m_HwDevInfo.empty())
        InitHwDevInfo();
    return m_HwDevInfo;
}

void GpuSubdevice::ClearHwDevInfo()
{
    m_HwDevInfo.clear();
}

GpuSubdevice::HwDevType GpuSubdevice::RegDevTypeToEnum(UINT32 regType)
{
    // This needs to be able to be called before the parent device is set, so it
    // is not possible to use GetParentDevice()->GetFamily()
    GpuDevice::GpuFamily family =
        static_cast<GpuDevice::GpuFamily>(Gpu::DeviceIdToFamily(DeviceId()));
    if (family == GpuDevice::None)
        return GpuSubdevice::HwDevType::HDT_ILWALID;

    if (!s_PerFamilyRegDevTypeToEnum.count(family))
    {
        s_PerFamilyRegDevTypeToEnum[family] = vector<RegTypeToEnumEntry>();
        auto & familyRegEnums = s_PerFamilyRegDevTypeToEnum[family];
        const auto & regs = Regs();

#define ADD_HWDEV_ENUM_ENTRY(r, e)                               \
    if (regs.IsSupported(r))                                     \
        familyRegEnums.push_back({ regs.LookupValue(r), e });

        ADD_HWDEV_ENUM_ENTRY(MODS_PTOP_DEVICE_INFO2_DEV_TYPE_ENUM_GRAPHICS, HwDevType::HDT_GRAPHICS);
        ADD_HWDEV_ENUM_ENTRY(MODS_PTOP_DEVICE_INFO2_DEV_TYPE_ENUM_LWDEC,    HwDevType::HDT_LWDEC);
        ADD_HWDEV_ENUM_ENTRY(MODS_PTOP_DEVICE_INFO2_DEV_TYPE_ENUM_LWENC,    HwDevType::HDT_LWENC);
        ADD_HWDEV_ENUM_ENTRY(MODS_PTOP_DEVICE_INFO2_DEV_TYPE_ENUM_LWJPG,    HwDevType::HDT_LWJPG);
        ADD_HWDEV_ENUM_ENTRY(MODS_PTOP_DEVICE_INFO2_DEV_TYPE_ENUM_OFA,      HwDevType::HDT_OFA);
        ADD_HWDEV_ENUM_ENTRY(MODS_PTOP_DEVICE_INFO2_DEV_TYPE_ENUM_SEC,      HwDevType::HDT_SEC);
        ADD_HWDEV_ENUM_ENTRY(MODS_PTOP_DEVICE_INFO2_DEV_TYPE_ENUM_LCE,      HwDevType::HDT_LCE);
        ADD_HWDEV_ENUM_ENTRY(MODS_PTOP_DEVICE_INFO2_DEV_TYPE_ENUM_GSP,      HwDevType::HDT_GSP);
        ADD_HWDEV_ENUM_ENTRY(MODS_PTOP_DEVICE_INFO2_DEV_TYPE_ENUM_IOCTRL,   HwDevType::HDT_IOCTRL);
        ADD_HWDEV_ENUM_ENTRY(MODS_PTOP_DEVICE_INFO2_DEV_TYPE_ENUM_FLA,      HwDevType::HDT_FLA);
        ADD_HWDEV_ENUM_ENTRY(MODS_PTOP_DEVICE_INFO2_DEV_TYPE_ENUM_C2C,      HwDevType::HDT_C2C);
        ADD_HWDEV_ENUM_ENTRY(MODS_PTOP_DEVICE_INFO2_DEV_TYPE_ENUM_HSHUB,    HwDevType::HDT_HSHUB);
        ADD_HWDEV_ENUM_ENTRY(MODS_PTOP_DEVICE_INFO2_DEV_TYPE_ENUM_FSP,      HwDevType::HDT_FSP);
#undef ADD_HWDEV_ENUM_ENTRY
    }

    const auto & familyRegEnums = s_PerFamilyRegDevTypeToEnum[family];
    auto pRegEnumEntry = find_if(familyRegEnums.begin(), familyRegEnums.end(),
                 [&] (const RegTypeToEnumEntry & e) -> bool { return e.regEnumValue == regType; });
    if (pRegEnumEntry != familyRegEnums.end())
        return pRegEnumEntry->enumType;

    Printf(Tee::PriWarn, "Unknown device type found in PTOP - %u\n", regType);
    return GpuSubdevice::HwDevType::HDT_ILWALID;
}

//-----------------------------------------------------------------------------
bool GpuSubdevice::IsGFWBootStarted()
{
    auto &regs = GpuSubdevice::Regs();

    if (!regs.IsSupported(MODS_PGC6_AON_SELWRE_SCRATCH_GROUP_05_0_GFW_BOOT))
        return false;

    if (!regs.Test32(MODS_PGC6_AON_SELWRE_SCRATCH_GROUP_05_PRIV_LEVEL_MASK_READ_PROTECTION_ALL_LEVELS_ENABLED))
        return false;

    return !regs.Test32(MODS_PGC6_AON_SELWRE_SCRATCH_GROUP_05_0_GFW_BOOT_PROGRESS_NOT_STARTED);
}

//-----------------------------------------------------------------------------
RC GpuSubdevice::PollGFWBootComplete(Tee::Priority pri)
{
    RC rc;
    if (HasFeature(Device::Feature::GPUSUB_SUPPORTS_FSP) &&
        (Platform::GetSimulationMode() == Platform::Hardware))
    {
        Fsp * pFsp = GetFspImpl();
        MASSERT(pFsp);
        CHECK_RC(pFsp->PollFspInitialized());
    }
    else
    {
        rc = PollGpuRegValue(
                    MODS_PGC6_AON_SELWRE_SCRATCH_GROUP_05_0_GFW_BOOT_PROGRESS_COMPLETED,
                    0,
                    GetGFWBootTimeoutMs());

        if (rc != RC::OK)
        {
            Printf(Tee::PriWarn, "GFW boot did not complete. May be due to an invalid FS config\n");
            Printf(Tee::PriNormal, "  Boot status = 0x%08x\n",
                                   Regs().Read32(MODS_PGC6_AON_SELWRE_SCRATCH_GROUP_05_0_GFW_BOOT));

            return ReportGFWBootMemoryErrors(pri);
        }
    }
    return rc;
}

//-----------------------------------------------------------------------------
RC GpuSubdevice::ReportGFWBootMemoryErrors(Tee::Priority pri)
{
    UINT32 numFbpas = Regs().Read32(MODS_PTOP_SCAL_NUM_FBPAS);
    // We need access to the TRAINING_CMD and TRAINING_STATUS registers
    // in order to categorize GFW boot failures as memory training issues.
    // Not all SKUs will let us read these registers due to priv security.
    // If we can't read them, print a warning and call it a GFW boot failure.
    if (numFbpas >> 16 == 0xBADF ||
        !Regs().HasReadAccess(MODS_PFB_FBPA_TRAINING_CMD, 0) ||
        !Regs().HasReadAccess(MODS_PFB_FBPA_0_TRAINING_STATUS, 0))
    {
        Printf(Tee::PriWarn, "Cannot read memory training status registers\n");
        return RC::GFW_BOOT_FAILURE;
    }

    if (Regs().HasReadAccess(MODS_PFB_FBPA_FALCON_MONITOR))
    {
        Printf(pri, "  LW_PFB_FBPA_FALCON_MONITOR = 0x%08x\n",
                    Regs().Read32(MODS_PFB_FBPA_FALCON_MONITOR));
    }
    Printf(pri, "  LW_PFB_FBPA_TRAINING_CMD = 0x%08x\n",
                Regs().Read32(MODS_PFB_FBPA_TRAINING_CMD, 0));

    bool bMemoryTrainingFailed = false;
    const UINT32 fbpaMask = m_pFsImpl->FbpaMask();
    for (UINT32 fbpa = 0; fbpa < numFbpas; fbpa++)
    {
        const bool bIsFbpaFs = ((fbpaMask >> fbpa) & 1) == 0;
        if (!Regs().IsSupported(MODS_PFB_FBPA_0_TRAINING_STATUS, fbpa) ||
            bIsFbpaFs)
        {
            continue;
        }
        const UINT32 statusVal = Regs().Read32(MODS_PFB_FBPA_0_TRAINING_STATUS, fbpa);
        Printf(pri, "  LW_PFB_FBPA_%u_TRAINING_STATUS = 0x%08x %s\n",
                    fbpa, statusVal, bIsFbpaFs ? "(FS)" : "");
        if (!Regs().Test32(MODS_PFB_FBPA_0_TRAINING_STATUS_SUBP0_STATUS_FINISHED, fbpa) ||
            !Regs().Test32(MODS_PFB_FBPA_0_TRAINING_STATUS_SUBP1_STATUS_FINISHED, fbpa))
        {
            bMemoryTrainingFailed = true;
        }
    }

    for (UINT32 i = 0; i < Regs().LookupAddress(MODS_PFBFALCON_FIRMWARE_MAILBOX__SIZE_1); i++)
    {
        if (Regs().IsSupported(MODS_PFBFALCON_FIRMWARE_MAILBOX, i) &&
            Regs().HasReadAccess(MODS_PFBFALCON_FIRMWARE_MAILBOX, i))
        {
            Printf(pri, "  LW_PFBFALCON_FIRMWARE_MAILBOX(%u) = 0x%08x\n",
                        i, Regs().Read32(MODS_PFBFALCON_FIRMWARE_MAILBOX, i));
        }
    }

    if (bMemoryTrainingFailed)
    {
        if (Regs().Test32(MODS_PFB_FBPA_TRAINING_CMD_ADR_ENABLED))
        {
            return RC::MEMORY_ADDRESS_TRAINING_FAILURE;
        }
        else if (Regs().Test32(MODS_PFB_FBPA_TRAINING_CMD_RD_ENABLED))
        {
            return RC::MEMORY_DQ_READ_TRAINING_FAILURE;
        }
        else if (Regs().Test32(MODS_PFB_FBPA_TRAINING_CMD_WR_ENABLED))
        {
            return RC::MEMORY_DQ_WRITE_TRAINING_FAILURE;
        }
        else
        {
            Printf(Tee::PriError, "Unknown memory training failure\n");
            return RC::SOFTWARE_ERROR;
        }
    }

    return RC::GFW_BOOT_FAILURE;
}

//-----------------------------------------------------------------------------
RC GpuSubdevice::SetGFWBootHoldoff(bool holdoff)
{
    auto pPcie = GetInterface<Pcie>();

    UINT32 value;
    Platform::PciRead32(pPcie->DomainNumber(),
                        pPcie->BusNumber(),
                        pPcie->DeviceNumber(),
                        pPcie->FunctionNumber(),
                        LW_XVE_MISC_3,
                        &value);

    if (holdoff)
    {
        // tell vbios ucode to wait before running devinit
        value = FLD_SET_DRF_NUM(_XVE, _MISC_3, _SCRATCH, 0, value);
    }
    else
    {
        // tell vbios ucode to run devinit
        value = FLD_SET_DRF(_XVE, _MISC_3, _SCRATCH, _DEFAULT, value);
    }

    Platform::PciWrite32(pPcie->DomainNumber(),
                        pPcie->BusNumber(),
                        pPcie->DeviceNumber(),
                        pPcie->FunctionNumber(),
                        LW_XVE_MISC_3,
                        value);

    return OK;
}

//-----------------------------------------------------------------------------
bool GpuSubdevice::GetGFWBootHoldoff()
{
    auto pPcie = GetInterface<Pcie>();
    auto &regs = GpuSubdevice::Regs();

    if (!regs.IsSupported(MODS_XVE_MISC_3_SCRATCH))
        return false;

    UINT32 value;
    Platform::PciRead32(pPcie->DomainNumber(),
                        pPcie->BusNumber(),
                        pPcie->DeviceNumber(),
                        pPcie->FunctionNumber(),
                        LW_XVE_MISC_3,
                        &value);

    return !FLD_TEST_DRF(_XVE, _MISC_3, _SCRATCH, _DEFAULT, value);
}

//-----------------------------------------------------------------------------
RC GpuSubdevice::GetRgbMlwI2cInfo
(
    RgbMlwType type,
    I2c::I2cDcbDevInfoEntry *mlwI2cEntry
)
{
    RC rc;

    UINT08 i2cType;
    switch (type)
    {
        case RgbMlwGpu:
            i2cType = LW_DCB4X_I2C_DEVICE_TYPE_STM32F051K8U6TR;
            break;
        case RgbMlwSli:
            i2cType = LW_DCB4X_I2C_DEVICE_TYPE_STM32L031G6U6TR;
            break;
        default:
            Printf(Tee::PriNormal, "Incorrect MLW type %u\n", type);
            return RC::BAD_PARAMETER;
    }

    bool mlwFound = false;
    I2c::I2cDcbDevInfo i2cDcbDevInfo;
    rc = GetInterface<I2c>()->GetI2cDcbDevInfo(&i2cDcbDevInfo);
    if (rc == RC::LWRM_ERROR || rc == RC::GENERIC_I2C_ERROR)
    {
        return RC::I2C_DEVICE_NOT_FOUND; // WAR bug 2586472
    }
    CHECK_RC(rc);
    for (UINT32 dev = 0; dev < i2cDcbDevInfo.size(); ++dev)
    {
        if (i2cDcbDevInfo[dev].Type == i2cType)
        {
            *mlwI2cEntry = i2cDcbDevInfo[dev];
            mlwFound = true;
            break;
        }
    }

    if (!mlwFound)
    {
        return RC::I2C_DEVICE_NOT_FOUND;
    }

    return rc;
}

//-----------------------------------------------------------------------------
RC GpuSubdevice::GetRgbMlwFwVersion
(
    RgbMlwType type,
    UINT32 *pMajorVersion,
    UINT32 *pMinorVersion
)
{
    MASSERT(pMajorVersion);
    MASSERT(pMinorVersion);

    RC rc;
    *pMinorVersion = 0;
    *pMajorVersion = 0;

    const UINT32 MINOR_VERSION_REG       = 0x02; // register for MLW version minor
    const UINT32 MAJOR_VERSION_REG       = 0x03; // register for MLW version major

    I2c::I2cDcbDevInfoEntry mlwI2cEntry;
    rc = this->GetRgbMlwI2cInfo(type, &mlwI2cEntry);
    if (rc == RC::I2C_DEVICE_NOT_FOUND)
    {
        Printf(Tee::PriError, "%s RGB MLW not found on I2C bus\n",
            GpuSubdevice::RgbMlwTypeToStr(type));
    }
    CHECK_RC(rc);

    I2c::Dev i2cMlwDev = GetInterface<I2c>()->I2cDev(mlwI2cEntry.I2CLogicalPort, mlwI2cEntry.I2CAddress);
    i2cMlwDev.SetOffsetLength(1);
    i2cMlwDev.SetMessageLength(1);
    CHECK_RC(i2cMlwDev.Read(MINOR_VERSION_REG, pMinorVersion));
    CHECK_RC(i2cMlwDev.Read(MAJOR_VERSION_REG, pMajorVersion));

    return OK;
}

//-----------------------------------------------------------------------------
const char* GpuSubdevice::RgbMlwTypeToStr(RgbMlwType t)
{
    switch (t)
    {
        case RgbMlwGpu:
            return "GPU";
        case RgbMlwSli:
            return "SLI";
        default:
            MASSERT(!"Unknown RgbMlwType");
            return "UNKNOWN";
    }
}

//-----------------------------------------------------------------------------
RC GpuSubdevice::DumpCieData()
{
    for (UINT08 type = 0; type < NumRgbMlwType; type++)
    {
        // Get I2C port and address
        I2c::I2cDcbDevInfoEntry mlwI2cEntry;
        if (this->GetRgbMlwI2cInfo(static_cast<RgbMlwType>(type), &mlwI2cEntry))
        {
            if (OK != DumpMlwCieData(mlwI2cEntry.I2CLogicalPort,
                                     mlwI2cEntry.I2CAddress))
            {
                Printf(Tee::PriWarn, "Failed to dump CIE data for LED MLW at 0x%x\n",
                                      mlwI2cEntry.I2CAddress);
            }
        }
    }

    return OK;
}

//-----------------------------------------------------------------------------
RC GpuSubdevice::DumpMlwCieData(UINT32 port, UINT32 address)
{
    RC rc;

    const UINT32 SET_DATA_BLOCK_REG  = 0x2C;
    const UINT32 GET_DATA_BLOCK_REG  = 0x2D;

    const UINT32 headerSizeHeaderByteNum    = 5;
    const UINT32 dataEntrySizeHeaderByteNum = 6;
    const UINT32 numEntriesHeaderByteNum    = 7;
    const UINT32 setDataBlockNumBytes       = 1;
    const UINT32 getDataBlockNumBytes       = 32;
    const UINT32 serialIdNumBytes           = 28;
    const UINT32 calibDataNumBytes          = 4;
    const UINT32 maxDataEntries             = 6;
    const UINT32 maxCalibDataIdx            = 9;
    const string calibDataLabels[maxCalibDataIdx] =
    {
        "Red X",
        "Red Y",
        "Red Luminance",
        "Green X",
        "Green Y",
        "Green Luminance",
        "Blue X",
        "Blue Y",
        "Blue Luminance",
    };

    UINT32 data;
    UINT32 calibDataIdx;
    vector<UINT08> setDataBlock(setDataBlockNumBytes);
    vector<UINT08> getDataBlock(getDataBlockNumBytes);

    I2c::Dev i2cMlwDev = GetInterface<I2c>()->I2cDev(port, address);
    i2cMlwDev.SetMsbFirst(false);

    Printf(Tee::PriNormal, "Illumination CIE data for MLW at 0x%x -\n",
                            address);

    /* Header format :
     * Signature    - 3:0
     * Version      - 4:4
     * Header Size  - 5:5
     * Entry Size   - 6:6
     * Entry Count  - 7:7
     * Reserved     - 15:8
     */
    setDataBlock[0] = 0;
    CHECK_RC(i2cMlwDev.WriteBlk(SET_DATA_BLOCK_REG,
                                setDataBlockNumBytes,
                                setDataBlock));
    CHECK_RC(i2cMlwDev.ReadBlk(GET_DATA_BLOCK_REG,
                               getDataBlockNumBytes,
                               &getDataBlock));
    UINT32 headerNumBytes    = getDataBlock[headerSizeHeaderByteNum];
    UINT32 dataEntryNumBytes = getDataBlock[dataEntrySizeHeaderByteNum];
    UINT32 numDataEntries    = getDataBlock[numEntriesHeaderByteNum];
    if (numDataEntries > maxDataEntries)
    {
        Printf(Tee::PriError, "CIE Table Header populated incorrectly\n");
        return RC::UNSUPPORTED_SYSTEM_CONFIG;
    }

    UINT32 totalDataNumBytes = numDataEntries * dataEntryNumBytes;
    UINT32 numDataBlocks     = 1 + ((totalDataNumBytes + headerNumBytes -1) /
                                     getDataBlockNumBytes);

    vector<UINT08> calibData;
    calibData.reserve(totalDataNumBytes);
    calibData.insert(calibData.begin(),
                     getDataBlock.begin() + headerNumBytes,
                     getDataBlock.end());
    // Read calibration blocks
    // (Starting with 1 since we have already read 1 data block)
    for (UINT32 block = 1; block < numDataBlocks; block++)
    {
        setDataBlock[0] = block;
        CHECK_RC(i2cMlwDev.WriteBlk(SET_DATA_BLOCK_REG,
                                    setDataBlockNumBytes,
                                    setDataBlock));
        CHECK_RC(i2cMlwDev.ReadBlk(GET_DATA_BLOCK_REG,
                                   getDataBlockNumBytes,
                                   &getDataBlock));
        calibData.insert(calibData.end(),
                         getDataBlock.begin(),
                         getDataBlock.end());
    }

    // Print calibration info
    /* Data Entry format :
     * Serial Number
     * RGB data */
    UINT32 offset = 0;
    for (UINT32 dataEntry = 0; dataEntry < numDataEntries; dataEntry++)
    {
        Printf(Tee::PriNormal, "Entry/Zone %u :\n", dataEntry);
        Printf(Tee::PriNormal, "  %-20s = ", "Serial Number");
        for (UINT32 i = 0; i < serialIdNumBytes; i++)
        {
            Printf(Tee::PriNormal, "%c", calibData[offset++]);
        }
        Printf(Tee::PriNormal, "\n");

        // Print calibration values, each of size "calibDataNumBytes"
        calibDataIdx = 0;
        while (calibDataIdx < maxCalibDataIdx)
        {
            i2cMlwDev.ArrayToInt(&calibData[offset],
                                 &data,
                                 calibDataNumBytes);
            Printf(Tee::PriNormal, "  %-20s = %u\n",
                                    calibDataLabels[calibDataIdx++].c_str(),
                                    data);
            offset += calibDataNumBytes;
        }
    }

    return OK;
}

RC GpuSubdevice::CheckL2Mode(GpuSubdevice::L2Mode Mode)
{
    // Bail out early on CheetAh, because we can't read these registers using CPU
    // on CheetAh and also L2 bypass mode cannot be controlled with the lwgpu driver.
    if (Platform::UsesLwgpuDriver())
    {
        return OK;
    }

    if (Mode == GpuSubdevice::L2_DISABLED)
    {
        if (Regs().Test32(MODS_PLTCG_LTCS_LTSS_TSTG_SET_MGMT_2_L2_BYPASS_MODE_DISABLED))
        {
            Printf(Tee::PriError, "L2 Bypass is not enabled.\n");
            return RC::L2_CACHE_ERROR;
        }
    }
    else if (Mode == GpuSubdevice::L2_ENABLED)
    {
        if (Regs().Test32(MODS_PLTCG_LTCS_LTSS_TSTG_SET_MGMT_2_L2_BYPASS_MODE_ENABLED))
        {
            Printf(Tee::PriError, "L2 Bypass is enabled.\n");
            return RC::L2_CACHE_ERROR;
        }
    }
    return OK;
}

void GpuSubdevice::SetGrRouteInfo
(
    LW2080_CTRL_GR_ROUTE_INFO* grRouteInfo,
    UINT32 smcEngineId
) const
{
    grRouteInfo->flags = FLD_SET_DRF(2080, _CTRL_GR_ROUTE_INFO_FLAGS, _TYPE, _ENGID,
                                     grRouteInfo->flags);
    grRouteInfo->route = FLD_SET_DRF_NUM64(2080, _CTRL_GR_ROUTE_INFO_DATA, _ENGID,
                                           smcEngineId,
                                           grRouteInfo->route);
}

/* virtual */ bool GpuSubdevice::IsPersistentSMCEnabled() const
{
    if (HasFeature(GPUSUB_HAS_SMC_STATUS_BIT))
    {
        MASSERT(!"IsPersistentSMCEnabled must be implemented for a specific GPU.");
    }

    return false;
}

bool GpuSubdevice::UsingMfgModsSMC() const
{
    // Fetch SMC partitioning configuration for the current device
    const NamedProperty* pSmcPartProp;
    if (RC::OK != GetNamedProperty("SmcPartitions", &pSmcPartProp))
    {
        return false;
    }
    string partitionStr;
    if (RC::OK != pSmcPartProp->Get(&partitionStr))
    {
        return false;
    }
    return partitionStr.size() > 0;
}

RC GpuSubdevice::PartitionSmc(UINT32 client)
{
    RC rc;

    // Fetch SMC partitioning configuration for the current device
    NamedProperty* pSmcPartProp;
    string partitionStr;
    CHECK_RC(GetNamedProperty("SmcPartitions", &pSmcPartProp));
    CHECK_RC(pSmcPartProp->Get(&partitionStr));
    MASSERT(partitionStr.size() > 0);

    // Parse SMC configuration
    vector<Smlwtil::Partition> partitionData;
    CHECK_RC(Smlwtil::ParsePartitionsStr(partitionStr, &partitionData));
    if (partitionData.size() > LW2080_CTRL_GPU_MAX_PARTITIONS)
    {
        Printf(Tee::PriError,
               "%zu SMC Partitions requested (Max %d)\n",
               partitionData.size(), LW2080_CTRL_GPU_MAX_PARTITIONS);
        return RC::BAD_COMMAND_LINE_ARGUMENT;
    }

    // Configure which SMC engines to use when using SR-IOV with SMC
    for (UINT32 partIdx = 0; partIdx < partitionData.size(); partIdx++)
    {
        const auto& part = partitionData[partIdx];

        bool sriovFound = false;
        bool foundSmcEngine = false;
        UINT32 smcEngineToUse = 0;
        for (UINT32 smcEngIdx = 0; smcEngIdx < part.smcEngineData.size(); smcEngIdx++)
        {
            // By default we'll use the first SMC engine with GPCs within a partition
            if (!foundSmcEngine && part.smcEngineData[smcEngIdx].gpcCount > 0)
            {
                smcEngineToUse = smcEngIdx;
                foundSmcEngine = true;
            }
            // If we have explicitly selected an SMC engine to run on use that instead
            if (part.smcEngineData[smcEngIdx].useWithSriov)
            {
                if (sriovFound)
                {
                    Printf(Tee::PriError,
                           "Only one SR-IOV instance may be created per partition\n");
                    return RC::BAD_COMMAND_LINE_ARGUMENT;
                }
                smcEngineToUse = smcEngIdx;
                sriovFound = true;
            }
        }
        // Not all partitions have to be used
        if (foundSmcEngine)
        {
            m_SriovSmcEngineIndices[partIdx] = smcEngineToUse;
        }
    }

    // Set Partition Mode
    LW2080_CTRL_GPU_SET_PARTITIONING_MODE_PARAMS partModeParams = {};
    partModeParams.partitioningMode =
        LW2080_CTRL_GPU_SET_PARTITIONING_MODE_REPARTITIONING_MAX_PERF;
    rc = LwRmPtr(client)->Control(LwRmPtr(client)->GetSubdeviceHandle(this),
                                      LW2080_CTRL_CMD_GPU_SET_PARTITIONING_MODE,
                                      &partModeParams,
                                      sizeof(partModeParams));
    if (rc != RC::OK)
    {
        Printf(Tee::PriError, "Setting of SMC partitioning mode failed\n");
        return rc;
    }

    // Partition GPU
    LW2080_CTRL_GPU_SET_PARTITIONS_PARAMS params = {};
    params.partitionCount = static_cast<UINT32>(partitionData.size());
    for (UINT32 i = 0; i < partitionData.size(); i++)
    {
        params.partitionInfo[i].bValid = true;
        params.partitionInfo[i].partitionFlag = partitionData[i].partitionFlag;
    }
    rc = LwRmPtr(client)->Control(LwRmPtr(client)->GetSubdeviceHandle(this),
                                      LW2080_CTRL_CMD_GPU_SET_PARTITIONS,
                                      &params,
                                      sizeof(params));
    if (rc != RC::OK)
    {
        Printf(Tee::PriError, "SMC Partitioning Failed\n");
        return rc;
    }

    // Configure SMC Partitions
    map<UINT32, UINT32> swizzId2PartIdx;
    for (UINT32 partIdx = 0; partIdx < partitionData.size(); partIdx++)
    {
        const auto& part = partitionData[partIdx];

        // Store SwizzId of each PartIdx, since later calls to GET_PARTITIONS
        // might be indexed differently
        const auto swizzId = params.partitionInfo[partIdx].swizzId;
        swizzId2PartIdx[swizzId] = partIdx;

        if (part.smcEngineData.size() > LW2080_CTRL_GPU_MAX_SMC_IDS)
        {
            Printf(Tee::PriError,
                   "%zu SMC Engines requested for partition idx %d (Max %d)\n",
                   part.smcEngineData.size(), partIdx, LW2080_CTRL_GPU_MAX_SMC_IDS);
            return RC::BAD_COMMAND_LINE_ARGUMENT;
        }

        LW2080_CTRL_GPU_CONFIGURE_PARTITION_PARAMS confParams = {};
        confParams.swizzId = swizzId;
        confParams.updateSmcEngMask = 0x0;
        // Handle case where an explicit numbers of GPCs is assigned to the SMC engine
        for (UINT32 smcEngIdx = 0; smcEngIdx < part.smcEngineData.size(); smcEngIdx++)
        {
            const auto& smcEngine = part.smcEngineData[smcEngIdx];

            // Don't configure SMC engines with 0 assigned GPCs, since 0 is already
            // the default.
            if (smcEngine.gpcCount == 0)
            {
                continue;
            }
            if (smcEngine.gpcCount > LW2080_CTRL_GPU_MAX_GPC_PER_SMC)
            {
                Printf(Tee::PriError,
                       "SMC engine cannot be assigned %d GPCs (max %d)\n",
                       smcEngine.gpcCount, LW2080_CTRL_GPU_MAX_GPC_PER_SMC);
                return RC::BAD_COMMAND_LINE_ARGUMENT;
            }
            confParams.gpcCountPerSmcEng[smcEngIdx] = smcEngine.gpcCount;
            confParams.updateSmcEngMask |= (1u << smcEngIdx);
        }
        // Handle case where all GPCs are assigned to the SMC engine
        for (UINT32 smcEngIdx = 0; smcEngIdx < part.smcEngineData.size(); smcEngIdx++)
        {
            const auto& smcEngine = part.smcEngineData[smcEngIdx];
            if (smcEngine.useAllGpcs)
            {
                if (swizzId != 0)
                {
                    Printf(Tee::PriError,
                           "Only swizzId 0 (full partition) can assign an SMC engine to use "
                           "all available GPCs\n");
                    return RC::BAD_COMMAND_LINE_ARGUMENT;
                }
                if (confParams.updateSmcEngMask)
                {
                    Printf(Tee::PriError,
                           "An SMC engine was assigned all available GPCs, "
                           "but GPCs were assigned to other SMC engines\n");
                    return RC::BAD_COMMAND_LINE_ARGUMENT;
                }
                // UseAllGpcs updates only one SMC engine
                confParams.bUseAllGPCs = true;
                confParams.updateSmcEngMask = (1u << smcEngIdx);
                break;
            }
        }
        // Only configure the partition if there are GPCs assigned to the SMC engine
        if (confParams.updateSmcEngMask)
        {
            rc = LwRmPtr(client)->Control(LwRmPtr(client)->GetSubdeviceHandle(this),
                                  LW2080_CTRL_CMD_GPU_CONFIGURE_PARTITION,
                                  &confParams,
                                  sizeof(confParams));
            if (rc != RC::OK)
            {
                Printf(Tee::PriError,
                       "Unable to configure SMC engines for partition idx %d\n", partIdx);
                return rc;
            }
        }
    }

    // Fetch [GPC <-> SMC Engine] mappings for use in creating the SMC topology
    map<tuple<UINT32, UINT32>, SmcGpcTopo> gpcMapping;
    const UINT32 gpcCount = GetGpcCount();
    for (UINT32 virtGpc = 0; virtGpc < gpcCount; virtGpc++)
    {
        const RegHal &regs = Regs();
        UINT32 smcMapReg = regs.Read32(MODS_PSMCARB_SMC_PARTITION_GPC_MAP, virtGpc);

        const UINT32 physGpc =
            regs.GetField(smcMapReg, MODS_PSMCARB_SMC_PARTITION_GPC_MAP_PHYSICAL_GPC_ID);
        const UINT32 uGpuId =
            regs.GetField(smcMapReg, MODS_PSMCARB_SMC_PARTITION_GPC_MAP_UGPU_ID);
        const bool sysPipeValid =
            regs.TestField(smcMapReg, MODS_PSMCARB_SMC_PARTITION_GPC_MAP_VALID_TRUE);
        if (sysPipeValid)
        {
            const UINT32 physSys =
                regs.GetField(smcMapReg, MODS_PSMCARB_SMC_PARTITION_GPC_MAP_SYS_PIPE_ID);
            const UINT32 localGpc =
                regs.GetField(smcMapReg, MODS_PSMCARB_SMC_PARTITION_GPC_MAP_SYS_PIPE_LOCAL_GPC_ID);
            SmcGpcTopo gpcTopo = {};
            gpcTopo.localGpc = localGpc;
            gpcTopo.virtGpc  = virtGpc;
            gpcTopo.physGpc  = physGpc;
            gpcTopo.uGpuId   = uGpuId;
            gpcTopo.numTpcs  = GetTpcCountOnGpc(virtGpc);
            gpcMapping[make_tuple(physSys, localGpc)] = gpcTopo;
        }
    }

    // Configure SMC topology
    //
    // This stores the relationships between physical, virtual, and local
    // GPCs and SMC engines (syspipes)
    //
    LW2080_CTRL_GPU_GET_PARTITIONS_PARAMS getPartParams = {};
    getPartParams.bGetAllPartitionInfo = true;
    CHECK_RC(LwRmPtr(client)->Control(LwRmPtr(client)->GetSubdeviceHandle(this),
                                      LW2080_CTRL_CMD_GPU_GET_PARTITIONS,
                                      &getPartParams,
                                      sizeof(getPartParams)));
    for (UINT32 queryIdx = 0; queryIdx < getPartParams.validPartitionCount; queryIdx++)
    {
        const auto& part = getPartParams.queryPartitionInfo[queryIdx];
        const UINT32 swizzId = part.swizzId;

        // Get PartIdx that corresponds to the current partition data,
        // since RM isn't required to return partitions in the same order that they are created
        MASSERT(swizzId2PartIdx.count(swizzId));
        const UINT32 partIdx = swizzId2PartIdx[swizzId];
        m_PartIdx2QueryIdx[partIdx] = queryIdx;

        // Get localSyspipe -> physSyspipe mapping from RM
        LW2080_CTRL_GPU_GET_PHYS_SYS_PIPE_IDS_PARAMS syspipeParams = {};
        syspipeParams.swizzId = swizzId;
        CHECK_RC(LwRmPtr(client)->Control(
                     LwRmPtr(client)->GetSubdeviceHandle(this),
                     LW2080_CTRL_CMD_GPU_GET_PHYS_SYS_PIPE_IDS,
                     &syspipeParams, sizeof(syspipeParams)));
        MASSERT(syspipeParams.physSysPipeCount == part.grEngCount);

        // Init SMC partition topology
        SmcPartTopo partTopo = {};
        partTopo.swizzId  = part.swizzId;

        // Configure topology of SMC engines within the partition
        for (UINT32 smcEngIdx = 0; smcEngIdx < part.grEngCount; smcEngIdx++)
        {
            const UINT32 physSyspipe = syspipeParams.physSysPipeId[smcEngIdx];
            SmcEngineTopo engineTopo = {};
            engineTopo.localSyspipe = smcEngIdx;
            engineTopo.physSyspipe  = physSyspipe;

            // Configure topology of valid GPCs within the SMC engine
            for (UINT32 localGpc = 0; localGpc < part.gpcsPerGr[smcEngIdx]; localGpc++)
            {
                const auto gpcKey = make_tuple(physSyspipe, localGpc);
                // Commit GPC topology
                MASSERT(gpcMapping.count(gpcKey));
                engineTopo.gpcs.push_back(gpcMapping[gpcKey]);
            }
            // Commit SMC engine topology
            partTopo.smcEngines.push_back(engineTopo);
        }
        // Commit SMC partition topology
        MASSERT(!m_SmcTopology.count(partIdx));
        m_SmcTopology[partIdx] = partTopo;
    }

    return rc;
}

RC GpuSubdevice::GetSmcPartitionInfo(UINT32 partitionId, SmcPartTopo *smcInfo)
{
    if (!Platform::IsVirtFunMode() && IsSMCEnabled() &&
        UsingMfgModsSMC() && (partitionId < m_SmcTopology.size()))
    {
        *smcInfo = m_SmcTopology[partitionId];
        return RC::OK;
    }
    return RC::UNSUPPORTED_FUNCTION;
}

RC GpuSubdevice::InitSmcResources(UINT32 client)
{
    RC rc;
    JavaScriptPtr pJs;

    LW2080_CTRL_GPU_GET_PARTITIONS_PARAMS params = {};
    params.bGetAllPartitionInfo = true;
    rc = LwRmPtr(client)->Control(LwRmPtr(client)->GetSubdeviceHandle(this),
                                  LW2080_CTRL_CMD_GPU_GET_PARTITIONS,
                                  &params,
                                  sizeof(params));
    // Exit early if SMC isn't supported
    if (rc == RC::LWRM_NOT_SUPPORTED)
    {
        Printf(Tee::PriLow, "SMC is not supported by RM. "
                            "Skipping possible SMC resource initialization\n");
        return RC::OK;
    }
    CHECK_RC(rc);

    // Exit early if SMC isn't enabled
    if (params.validPartitionCount == 0)
    {
        Printf(Tee::PriLow, "No valid SMC partitions found. "
                            "Skipping SMC resource initialization\n");
        return RC::OK;
    }

    // If we aren't using -smc_partitions with MFG mods, partition indexing will be
    // equivalent to the indexing returned by LW2080_CTRL_CMD_GPU_GET_PARTITIONS
    if (m_PartIdx2QueryIdx.empty())
    {
        MASSERT(!UsingMfgModsSMC());
        for (UINT32 i = 0; i < params.validPartitionCount; i++)
        {
            m_PartIdx2QueryIdx[i] = i;
        }
    }

    // Init SMC topology and find default SMC engine
    bool foundSmcEngine = false;
    UINT32 firstUsablePartIdx = 0;
    UINT32 firstUsableSmcEngine = 0;
    MASSERT(params.validPartitionCount == m_PartIdx2QueryIdx.size());
    for (const auto& elem : m_PartIdx2QueryIdx)
    {
        const UINT32 partIdx = elem.first;
        const UINT32 queryIdx = elem.second;
        const auto& part = params.queryPartitionInfo[queryIdx];

       // Store partition info
       m_PartInfo.push_back(part);

        // Init topology of SMC engines (syspipes) within the partition
        for (UINT32 smcEngIdx = 0; smcEngIdx < part.grEngCount; smcEngIdx++)
        {
            if (part.gpcsPerGr[smcEngIdx])
            {
                m_UsablePartIdx.emplace_back(partIdx);
                if (!foundSmcEngine)
                {
                    // Set default SMC Engine ID for use by MODS
                    foundSmcEngine = true;
                    firstUsablePartIdx   = partIdx;
                    firstUsableSmcEngine = smcEngIdx;
                }
                break;
            }
        }
    }
    if (!foundSmcEngine)
    {
        Printf(Tee::PriError, "SMC partitioning contains no usable GPCs\n");
        return RC::BAD_COMMAND_LINE_ARGUMENT;
    }

    // If -smc_part_idx isn't in use default to the first SMC partition with a usable SMC engine
    NamedProperty* pSmcPartIdxProp;
    UINT32 smcPartIdx = ~0u;
    CHECK_RC(GetNamedProperty("SmcPartIdx", &pSmcPartIdxProp));
    CHECK_RC(pSmcPartIdxProp->Get(&smcPartIdx));
    if (smcPartIdx == ~0u)
    {
        // Set default SwizzId/PartIdx for use by MODS
        m_SmcResource.partIdx = firstUsablePartIdx;
    }
    else
    {
        if (smcPartIdx >= params.validPartitionCount ||
            !GetPartInfo(smcPartIdx).bValid)
        {
            Printf(Tee::PriError,
                   "-smc_part_idx: Invalid SMC partition index %d selected\n",
                   smcPartIdx);
            return RC::BAD_COMMAND_LINE_ARGUMENT;
        }
        m_SmcResource.partIdx = smcPartIdx;
    }

    // If -smc_engine_idx isn't in use default to the first SMC engine with GPCs
    NamedProperty* pSmcEngIdxProp;
    UINT32 smcEngIdx = ~0u;
    CHECK_RC(GetNamedProperty("SmcEngineIdx", &pSmcEngIdxProp));
    CHECK_RC(pSmcEngIdxProp->Get(&smcEngIdx));
    if (smcEngIdx == ~0u)
    {
        // Set default SMC engine for use by MODS
        m_SmcResource.smcEngineIdx = firstUsableSmcEngine;
    }
    else
    {
        if (smcEngIdx >= LW2080_CTRL_GPU_MAX_SMC_IDS ||
            !GetPartInfo(m_SmcResource.partIdx).gpcsPerGr[smcEngIdx])
        {
            Printf(Tee::PriError,
                   "-smc_engine_idx: Invalid SMC engine index %d selected\n",
                   smcEngIdx);
            return RC::BAD_COMMAND_LINE_ARGUMENT;
        }
        m_SmcResource.smcEngineIdx = smcEngIdx;
    }

    // Set active SMC partition and engine for use by LWCA
    NamedProperty* pSwizzIdProp;
    CHECK_RC(GetNamedProperty("ActiveSwizzId", &pSwizzIdProp));
    CHECK_RC(pSwizzIdProp->Set(GetSwizzId()));

    NamedProperty* pSmcEngineProp;
    CHECK_RC(GetNamedProperty("ActiveSmcEngine", &pSmcEngineProp));
    CHECK_RC(pSmcEngineProp->Set(GetSmcEngineIdx()));

    // 1. Get the swizzId and all SmcEngineIds to be used to send
    // the GrRouteInfo for some control calls
    // 2. Allocate SmcResource object with swizzId
    CHECK_RC(AllocSmcResource(client));

    // Flag SMC as enabled for the rest of the MODS run
    EnableSMC();
    return rc;
}

static SmcRegHal* s_pTmpSmcRegHal;
static void* s_SmcRegHalMutex =
    Tasker::AllocMutex("GpuSubdevice::SmcRegHalMutex", Tasker::mtxUnchecked);
static void* GetSmcRegHals(vector<SmcRegHal*>* pSmcRegHals)
{
    pSmcRegHals->push_back(s_pTmpSmcRegHal);
    return s_SmcRegHalMutex;
}

RC GpuSubdevice::AllocSmcResource(UINT32 client)
{
    RC rc;

    const auto& part = GetPartInfo(GetPartIdx());
    for (UINT32 smcEngineId = 0; smcEngineId < part.grEngCount; smcEngineId++)
    {
        if (part.gpcsPerGr[smcEngineId])
        {
            m_SmcResource.smcEngineIds.push_back(smcEngineId);
        }
    }
    if (m_SmcResource.smcEngineIds.empty())
    {
        Printf(Tee::PriError, "No valid SMC engines for current partIdx %d\n", GetPartIdx());
        return RC::BAD_COMMAND_LINE_ARGUMENT;
    }

    // Allocate Partition's SmcResource to be associated with the subdevice
    LWC637_ALLOCATION_PARAMETERS partitionParams = {};
    LwRm::Handle handle;
    partitionParams.swizzId = GetSwizzId();

    CHECK_RC(LwRmPtr(client)->Alloc(
             LwRmPtr(client)->GetSubdeviceHandle(this),
             &handle,
             AMPERE_SMC_PARTITION_REF,
             &partitionParams));
    m_SmcResource.clientPartitionHandle[client] = handle;

    // Init SMC Reghal
    m_SmcRegHal =
        make_unique<SmcRegHal>(this, LwRmPtr(client).Get(), GetSwizzId(), GetSmcEngineIdx());
    m_SmcRegHal->Initialize();

    Tasker::MutexHolder mh(s_SmcRegHalMutex);
    s_pTmpSmcRegHal = m_SmcRegHal.get();
    RegisterGetSmcRegHalsCB(GetSmcRegHals);

    return rc;
}

RC GpuSubdevice::RunFpfFub()
{
    RC rc;

    if (GpuPtr()->IsRmInitCompleted() && !IsSOC())
    {
        Printf(Tee::PriDebug, "Starting FUB exelwtion\n");
        LW2080_CTRL_CMD_FUSE_EXELWTE_FUB_BINARY_PARAMS params = {0};
        LwRmPtr pLwRm;
        rc = pLwRm->ControlBySubdevice(this,
                                       LW2080_CTRL_CMD_FUSE_EXELWTE_FUB_BINARY,
                                       &params,
                                       sizeof(params));
        if (rc != OK)
        {
            Printf(Tee::PriError, "FUB exelwtion failed with status 0x%x\n",
                                   params.returnStatus);
        }
    }
    else
    {
        return RC::UNSUPPORTED_FUNCTION;
    }

    return rc;
}

RC GpuSubdevice::RegisterGetSmcRegHalsCB(function<void* (vector<SmcRegHal*>*)> f)
{
    if (m_GetSmcRegHalsCb && f)
    {
        Printf(Tee::PriError, "GetSmcRegHals callback already registered.\n");
        return RC::SOFTWARE_ERROR;
    }
    m_GetSmcRegHalsCb = f;
    return OK;
}

RegHal& GpuSubdevice::Regs()
{
    return (m_SmcRegHal) ? *m_SmcRegHal : TestDevice::Regs();
}

const RegHal& GpuSubdevice::Regs() const
{
    return (m_SmcRegHal) ? *m_SmcRegHal : TestDevice::Regs();
}

UINT32 GpuSubdevice::GetNumHbmSites() const
{
    return 0;
}

RC GpuSubdevice::ReadPfRegFromVf(ModsGpuRegAddress regAddr, UINT32 *pRegValue) const
{
    RC rc;
    MASSERT(pRegValue);

    VmiopElw* pVmiopElw = nullptr;
    CHECK_RC(GetVmiopElw(&pVmiopElw));
    CHECK_RC(pVmiopElw->CallPluginPfRegRead(regAddr, pRegValue));

    return rc;
}

RC GpuSubdevice::CheckPfRegSupportedFromVf(ModsGpuRegAddress regAddr, bool* pIsSupported) const
{
    RC rc;
    MASSERT(pIsSupported);
    *pIsSupported = false;

    VmiopElw* pVmiopElw = nullptr;
    CHECK_RC(GetVmiopElw(&pVmiopElw));
    CHECK_RC(pVmiopElw->CallPluginPfRegIsSupported(regAddr, pIsSupported));

    return rc;
}

RC GpuSubdevice::Test32PfRegFromVf
(
    ModsGpuRegField field,
    UINT32 value,
    bool* pResult
) const
{
    return Test32PfRegFromVf(0, field, RegHalTable::NO_INDEXES, value, pResult);
}

RC GpuSubdevice::Test32PfRegFromVf
(
    ModsGpuRegField field,
    UINT32 regIndex,
    UINT32 value,
    bool* pResult
) const
{
    return Test32PfRegFromVf(0, field, RegHal::ArrayIndexes{regIndex}, value, pResult);
}

RC GpuSubdevice::Test32PfRegFromVf
(
    ModsGpuRegField field,
    RegHal::ArrayIndexes regIndexes,
    UINT32 value,
    bool* pResult
) const
{
    RC rc;
    MASSERT(pResult);
    *pResult = false;

    VmiopElw* pVmiopElw = nullptr;
    CHECK_RC(GetVmiopElw(&pVmiopElw));
    CHECK_RC(pVmiopElw->CallPluginPfRegTest32(0, field, regIndexes, value, pResult));

    return rc;
}

RC GpuSubdevice::Test32PfRegFromVf
(
    UINT32 domainIndex,
    ModsGpuRegField field,
    UINT32 value,
    bool* pResult
) const
{
    return Test32PfRegFromVf(domainIndex, field, RegHalTable::NO_INDEXES, value, pResult);
}

RC GpuSubdevice::Test32PfRegFromVf
(
    UINT32 domainIndex,
    ModsGpuRegField field,
    UINT32 regIndex,
    UINT32 value,
    bool* pResult
) const
{
    return Test32PfRegFromVf(domainIndex, field, RegHal::ArrayIndexes{regIndex}, value, pResult);
}

RC GpuSubdevice::Test32PfRegFromVf
(
    UINT32 domainIndex,
    ModsGpuRegField field,
    RegHal::ArrayIndexes regIndexes,
    UINT32 value,
    bool* pResult
) const
{
    RC rc;
    MASSERT(pResult);
    *pResult = false;

    VmiopElw* pVmiopElw = nullptr;
    CHECK_RC(GetVmiopElw(&pVmiopElw));
    CHECK_RC(pVmiopElw->CallPluginPfRegTest32(domainIndex, field, regIndexes, value, pResult));

    return rc;
}

RC GpuSubdevice::Test32PfRegFromVf
(
    ModsGpuRegValue value,
    bool* pResult
) const
{
    return Test32PfRegFromVf(0, value, RegHalTable::NO_INDEXES, pResult);
}

RC GpuSubdevice::Test32PfRegFromVf
(
    ModsGpuRegValue value,
    UINT32 regIndex,
    bool* pResult
) const
{
    return Test32PfRegFromVf(0, value, RegHalTable::ArrayIndexes{regIndex}, pResult);
}

RC GpuSubdevice::Test32PfRegFromVf
(
    ModsGpuRegValue value,
    RegHal::ArrayIndexes regIndexes,
    bool* pResult
) const
{
    RC rc;
    MASSERT(pResult);
    *pResult = false;

    VmiopElw* pVmiopElw = nullptr;
    CHECK_RC(GetVmiopElw(&pVmiopElw));
    CHECK_RC(pVmiopElw->CallPluginPfRegTest32(0, value, regIndexes, pResult));

    return rc;
}

RC GpuSubdevice::Test32PfRegFromVf
(
    UINT32 domainIndex,
    ModsGpuRegValue value,
    bool* pResult
) const
{
    return Test32PfRegFromVf(domainIndex, value, RegHalTable::NO_INDEXES, pResult);
}

RC GpuSubdevice::Test32PfRegFromVf
(
    UINT32 domainIndex,
    ModsGpuRegValue value,
    UINT32 regIndex,
    bool* pResult
) const
{
    return Test32PfRegFromVf(domainIndex, value, RegHalTable::ArrayIndexes{regIndex}, pResult);
}

RC GpuSubdevice::Test32PfRegFromVf
(
    UINT32 domainIndex,
    ModsGpuRegValue value,
    RegHal::ArrayIndexes regIndexes,
    bool* pResult
) const
{
    RC rc;
    MASSERT(pResult);
    *pResult = false;

    VmiopElw* pVmiopElw = nullptr;
    CHECK_RC(GetVmiopElw(&pVmiopElw));
    CHECK_RC(pVmiopElw->CallPluginPfRegTest32(domainIndex, value, regIndexes, pResult));

    return rc;
}

RC GpuSubdevice::CheckHbmIeee1500RegAccess(const HbmSite& hbmSite) const
{
    Printf(Tee::PriError, "%s: Unsupported. Override for a specific GPU.\n", __FUNCTION__);
    return RC::UNSUPPORTED_FUNCTION;
}

RC GpuSubdevice::GetAdcCalFuseVals(AdcCalibrationReg *pFusedRegister) const
{
    Printf(Tee::PriError, "GetAdcCalFuseVals unsupported. "
            "Must be overriden for a specific GPU.\n");
    return RC::UNSUPPORTED_FUNCTION;
}

RC GpuSubdevice::GetAdcProgVals(AdcCalibrationReg *pProgrammedRegister) const
{
    Printf(Tee::PriError, "GetAdcProgVals unsupported. "
            "Must be overriden for a specific GPU.\n");
    return RC::UNSUPPORTED_FUNCTION;
}

RC GpuSubdevice::GetAdcRawCorrectVals
(
    AdcRawCorrectionReg *pCorrectionRegister
) const
{
    Printf(Tee::PriError, "GetAdcRawCorrectVals unsupported. "
            "Must be overriden for a specific GPU.\n");
    return RC::UNSUPPORTED_FUNCTION;
}

RC GpuSubdevice::GetAdcMulSampVals
(
    AdcMulSampReg *pAdcMulSampRegister
) const
{
    Printf(Tee::PriError, "GetAdcMulSampVals unsupported. "
            "Must be overriden for a specific GPU.\n");
    return RC::UNSUPPORTED_FUNCTION;
}

RC GpuSubdevice::GetInfoRomRprMaxDataCount(UINT32* pCount) const
{
    MASSERT(pCount);
    RC rc;

    LW2080_CTRL_GPU_GET_INFOROM_OBJECT_VERSION_PARAMS params = {};
    memcpy(params.objectType,
           LW2080_CTRL_GPU_INFOROM_OBJECT_NAME_RPR,
           LW2080_CTRL_GPU_INFOROM_OBJ_TYPE_LEN);

    CHECK_RC(LwRmPtr()->ControlBySubdevice(this,
                                           LW2080_CTRL_CMD_GPU_GET_INFOROM_OBJECT_VERSION,
                                           &params,
                                           sizeof(params)));

    switch (params.version)
    {
        case 1:  *pCount = 26;
            break;
        default: *pCount = LW90E7_CTRL_RPR_MAX_DATA_COUNT;
            break;
    }

    return rc;
}

bool GpuSubdevice::AreEcidsIdentical() const
{
    struct ChipInfo
    {
        LW2080_CTRL_GPU_INFO VendorCode;
        LW2080_CTRL_GPU_INFO LotCode0;
        LW2080_CTRL_GPU_INFO LotCode1;
        LW2080_CTRL_GPU_INFO FabCode;
        LW2080_CTRL_GPU_INFO WaferID;
        LW2080_CTRL_GPU_INFO XCoord;
        LW2080_CTRL_GPU_INFO YCoord;
    } LwrChipInfo = { };

    LW2080_CTRL_GPU_GET_INFO_V2_PARAMS Params = { };
    Params.gpuInfoListSize =
        sizeof (LwrChipInfo) / sizeof (LW2080_CTRL_GPU_INFO);

    LwrChipInfo.VendorCode.index = LW2080_CTRL_GPU_INFO_INDEX_VENDOR_CODE;
    LwrChipInfo.LotCode0.index = LW2080_CTRL_GPU_INFO_INDEX_LOT_CODE;
    LwrChipInfo.LotCode1.index = LW2080_CTRL_GPU_INFO_INDEX_LOT_CODE_HEAD;
    LwrChipInfo.FabCode.index = LW2080_CTRL_GPU_INFO_INDEX_FAB_CODE_FAB;
    LwrChipInfo.XCoord.index = LW2080_CTRL_GPU_INFO_INDEX_X_COORD;
    LwrChipInfo.YCoord.index = LW2080_CTRL_GPU_INFO_INDEX_Y_COORD;
    LwrChipInfo.WaferID.index = LW2080_CTRL_GPU_INFO_INDEX_WAFER_ID;

    LW2080_CTRL_GPU_INFO *pChipInfo = (LW2080_CTRL_GPU_INFO*)&LwrChipInfo;
    for (UINT32 i = 0; i < Params.gpuInfoListSize; i++)
    {
        Params.gpuInfoList[i].index = pChipInfo[i].index;
    }

    if (RC::OK != LwRmPtr()->ControlBySubdevice(this,
                LW2080_CTRL_CMD_GPU_GET_INFO_V2, &Params, sizeof(Params)))
    {
        Printf(Tee::PriError, "LOADING GPU INFO! RmControl call failed!\n");

        return false;
    }

    for (UINT32 i = 0; i < Params.gpuInfoListSize; i++)
    {
        pChipInfo[i].data = Params.gpuInfoList[i].data;
    }

    const UINT32 fusedVendorCode = Regs().Read32(MODS_FUSE_OPT_VENDOR_CODE_DATA);
    const UINT32 fusedLotCode0 = Regs().Read32(MODS_FUSE_OPT_LOT_CODE_0_DATA);
    const UINT32 fusedLotCode1 = Regs().Read32(MODS_FUSE_OPT_LOT_CODE_1_DATA);
    const UINT32 fusedFabCode = Regs().Read32(MODS_FUSE_OPT_FAB_CODE_DATA);
    const int fusedXCoordinate = Regs().Read32(MODS_FUSE_OPT_X_COORDINATE_DATA);
    const UINT32 fusedYCoordinate = Regs().Read32(MODS_FUSE_OPT_Y_COORDINATE_DATA);
    const UINT32 fusedWaferId = Regs().Read32(MODS_FUSE_OPT_WAFER_ID_DATA);

    const int xCoordinateSignBit =
        Regs().LookupMaskSize(MODS_FUSE_OPT_X_COORDINATE_DATA) - 1;
    const int fusedXCoord = fusedXCoordinate -
        2 * (fusedXCoordinate & (1 << xCoordinateSignBit));
    const int rmXCoord = LwrChipInfo.XCoord.data;


    if (fusedVendorCode != LwrChipInfo.VendorCode.data)
    {
        Printf(Tee::PriError, "Fused VendorCode (0x%08x) and RM VendorCode (0x%08x) mismatch!\n",
                fusedVendorCode, LwrChipInfo.VendorCode.data);
        return false;
    }

    if (fusedLotCode0 != LwrChipInfo.LotCode0.data)
    {
        Printf(Tee::PriError, "Fused LotCod0 (0x%08x) and RM LotCode0 (0x%08x) mismatch!\n",
                fusedLotCode0, LwrChipInfo.LotCode0.data);
        return false;
    }

    if (fusedLotCode1 != LwrChipInfo.LotCode1.data)
    {
        Printf(Tee::PriError, "Fused LotCode1 (0x%08x) and RM LotCode1 (0x%08x) mismatch!\n",
                fusedLotCode1, LwrChipInfo.LotCode1.data);
        return false;
    }

    if (fusedFabCode != LwrChipInfo.FabCode.data)
    {
        Printf(Tee::PriError, "Fused FabCode (0x%08x) and RM FabCode (0x%08x) mismatch!\n",
                fusedFabCode, LwrChipInfo.FabCode.data);
        return false;
    }

    if (fusedXCoord != rmXCoord)
    {
        Printf(Tee::PriError, "Fused XCoordinate (0x%08x) and RM XCoordinate (0x%08x) mismatch!\n",
                fusedXCoord, rmXCoord);
        return false;
    }

    if (fusedYCoordinate != LwrChipInfo.YCoord.data)
    {
        Printf(Tee::PriError, "Fused YCoordinate (0x%08x) and RM YCoordinate (0x%08x) mismatch!\n",
                fusedYCoordinate, LwrChipInfo.YCoord.data);
        return false;
    }

    if (fusedWaferId != LwrChipInfo.WaferID.data)
    {
        Printf(Tee::PriError, "Fused WaferId (0x%08x) and RM WaferId (0x%08x) mismatch!\n",
                fusedYCoordinate, LwrChipInfo.YCoord.data);
        return false;
    }

    // The ECIDs match
    return true;
}
//-----------------------------------------------------------------------------
// Return the number of MMUs per GPC or 0 on failure.
UINT32 GpuSubdevice::GetMmuCountPerGpc(UINT32 virtualGpc) const
{
    RC rc;
    LW2080_CTRL_GPU_GET_NUM_MMUS_PER_GPC_PARAMS params = {};
    params.gpcId = virtualGpc;

    if (IsSMCEnabled())
    {
        SetGrRouteInfo(&(params.grRouteInfo), GetSmcEngineIdx());
    }
    rc = LwRmPtr()->ControlBySubdevice(this,
                                       LW2080_CTRL_CMD_GPU_GET_NUM_MMUS_PER_GPC,
                                       &params,
                                       sizeof(params));
    if (RC::OK == rc)
    {
        return params.count;
    }

    if (rc == RC::LWRM_NOT_SUPPORTED)
    {
        return 0;
    }

    MASSERT(!"LW2080_CTRL_CMD_GPU_GET_NUM_MMUS_PER_GPC failed");
    return 0;
}

RC GpuSubdevice::InitializePerfGPU()
{
    RC rc;

    // Query only for PSTATES_INFO
    // Avoid calling the CLOCKS API
    // RM will always ensure that this returns some value
    LW2080_CTRL_PERF_PSTATES_INFO pstatesInfo = {};
    CHECK_RC(LwRmPtr()->ControlBySubdevice(this,
                                           LW2080_CTRL_CMD_PERF_PSTATES_GET_INFO,
                                           &pstatesInfo,
                                           sizeof(pstatesInfo)));

    if (pstatesInfo.version == LW2080_CTRL_PERF_PSTATE_VERSION_40)
    {
        m_pPerf = new Perf40(this, m_PerfWasOwned);
    }
    else if (pstatesInfo.version == LW2080_CTRL_PERF_PSTATE_VERSION_35 ||
             pstatesInfo.version == LW2080_CTRL_PERF_PSTATE_VERSION_30)
    {
        m_pPerf = new Perf30(this, m_PerfWasOwned);
    }
    else
    {
        // Use PSTATE 2.0 on older boards
        // Note : Forcing to PSTATE 2.0 on a PSTATE3.0 board is not supported
        // TODO : Deprecate -force_ps20_dev_mask
        m_pPerf = new Perf20(this, m_PerfWasOwned);
    }

    MASSERT(m_pPerf);

    return rc;
}

RC GpuSubdevice::InitializePerfSoc()
{

#if defined(TEGRA_MODS)
    m_pPerf = new PerfAndroid(this, m_PerfWasOwned);
#else
    m_pPerf = new PerfSocClientRm(this, m_PerfWasOwned);
#endif

    MASSERT(m_pPerf);

    return RC::OK;
}

INT32 GpuSubdevice::GetFullGraphicsTpcMask(UINT32 hwGpcNum) const
{
    UINT32 maxTpcsOnGpc = GetMaxTpcCount();
    return ((1 << maxTpcsOnGpc) - 1);
}

UINT32 GpuSubdevice::GetGraphicsTpcMaskOnGpc(UINT32 hwGpcNum) const
{
    return GetFsImpl()->TpcMask(hwGpcNum);
}

RC GpuSubdevice::ValidatePreInitRmRegistries() const
{
    // Check if the RM registry RmVgpcSingletons (also set via -vgpc_skyline)
    // is less than the allowed number in LW_LITTER_NUM_SINGLETON_GPCS
    UINT32 singletonGpcCount = 0;
    if ((Regs().IsSupported(MODS_LITTER_NUM_SINGLETON_GPCS)) &&
        (RC::OK == Registry::Read("ResourceManager", "RmVgpcSingletons", &singletonGpcCount)) &&
        (singletonGpcCount > Regs().LookupAddress(MODS_LITTER_NUM_SINGLETON_GPCS)))
    {
        Printf(Tee::PriError,
            "RM registry RmVgpcSingletons is set to %u, which is greater than the maximum allowed by HW- %u\n",
            singletonGpcCount,
            Regs().LookupAddress(MODS_LITTER_NUM_SINGLETON_GPCS));
        return RC::BAD_COMMAND_LINE_ARGUMENT;
    }

    return RC::OK;
}

FLOAT64 GpuSubdevice::GetGFWBootTimeoutMs() const
{
    return std::max(2000.0, Tasker::GetDefaultTimeoutMs());
}

RC GpuSubdevice::GetIsDevidHulkEnabled(bool *pIsEnabled) const
{
    *pIsEnabled = true;
    return RC::OK;
}

RC GpuSubdevice::IsSltFused(bool *pIsFused) const
{
    MASSERT(pIsFused);
    if (Regs().IsSupported(MODS_FUSE_OPT_SLT_DONE))
    {
        *pIsFused = (Regs().Read32(MODS_FUSE_OPT_SLT_DONE) == 1);
    }
    else
    {
        *pIsFused = (Regs().Read32(MODS_FUSE_OPT_SLT_REV) > 0);
    }
    return RC::OK;
}

void GpuSubdevice::AddPriFence()
{
    Regs().Write32(MODS_PPRIV_SYS_PRI_FENCE, 0);
    if ((Regs().IsSupported(MODS_SCAL_LITTER_NUM_SYSB) &&
         Regs().LookupAddress(MODS_SCAL_LITTER_NUM_SYSB) != 0) &&
         Regs().IsSupported(MODS_PPRIV_SYSB_PRI_FENCE))
    {
        Regs().Write32(MODS_PPRIV_SYSB_PRI_FENCE, 0);
    }
    if ((Regs().IsSupported(MODS_SCAL_LITTER_NUM_SYSC) &&
         Regs().LookupAddress(MODS_SCAL_LITTER_NUM_SYSC) != 0) &&
         Regs().IsSupported(MODS_PPRIV_SYSC_PRI_FENCE))
    {
        Regs().Write32(MODS_PPRIV_SYSC_PRI_FENCE, 0);
    }

    const UINT32 numActiveGpcs = Utility::CountBits(m_pFsImpl->GpcMask());
    if (numActiveGpcs != 0 &&
        Regs().IsSupported(MODS_PPRIV_GPC_GPCS_PRI_FENCE))
    {
        Regs().Write32(MODS_PPRIV_GPC_GPCS_PRI_FENCE, 0);
    }

    const UINT32 numActiveFbps = Utility::CountBits(m_pFsImpl->FbpMask());
    if (numActiveFbps != 0 &&
        Regs().IsSupported(MODS_PPRIV_FBP_FBPS_PRI_FENCE))
    {
        Regs().Write32(MODS_PPRIV_FBP_FBPS_PRI_FENCE, 0);
    }
}

RC GpuSubdevice::GetFaultMask(ModsGpuRegValue dcFaultReg,
                                    ModsGpuRegValue lowerThresFaultReg,
                                    ModsGpuRegValue higherThresFaultReg,
                                    ModsGpuRegValue overFlowErrorReg,
                                    UINT32 *pFaultMask)
{
    RC rc;
    *pFaultMask = 0;
    if (Regs().Test32(dcFaultReg))
    {
        *pFaultMask |=  LW2080_CTRL_CLK_DOMAIN_CLK_MON_STATUS_MASK_DC_FAULT;
    }
    if (Regs().Test32(lowerThresFaultReg))
    {
        *pFaultMask |= LW2080_CTRL_CLK_DOMAIN_CLK_MON_STATUS_MASK_LOWER_THRESH_FAULT;
    }
    if (Regs().Test32(higherThresFaultReg))
    {
        *pFaultMask |= LW2080_CTRL_CLK_DOMAIN_CLK_MON_STATUS_MASK_HIGHER_THRESH_FAULT;
    }
    if (Regs().Test32(overFlowErrorReg))
    {
        *pFaultMask |= LW2080_CTRL_CLK_DOMAIN_CLK_MON_STATUS_MASK_OVERFLOW_ERROR;
    }
    return rc;
}

float GpuSubdevice::GetGpuUtilizationFromSensorTopo(const GpuSensorInfo &sensorInfo, UINT32 idx)
{
    RC rc;

    const UINT64 value = sensorInfo.status.topologys[idx].reading;

    // Colwert fixed point 40.24 to floating point
    const UINT64 integer  = static_cast<UINT64>(value) >> 24;
    const UINT32 fraction = static_cast<UINT32>(value) & 0xFF'FF'FFU;
    double utilization = static_cast<double>(integer) +
                    static_cast<double>(fraction) / static_cast<double>(1U << 24);
    const auto& topo = sensorInfo.info.topologys[idx];

    // Remove FB sensor output VBIOS scaling factor
    if (topo.label == LW2080_CTRL_PERF_PERF_CF_TOPOLOGY_LABEL_FB)
    {
        // Fetch index of FB sensor from the topology
        if (topo.type == LW2080_CTRL_PERF_PERF_CF_TOPOLOGY_TYPE_SENSED_BASE)
        {
            const UINT32 sensorIdx =
                topo.data.sensedBase.sensorIdx;
            const auto& sensor =
                sensorInfo.sensors.sensors[sensorIdx];

            // Remove the scaling factor from the FB sensor value
            // Format is of type FXP20.12
            if (sensor.type == LW2080_CTRL_PERF_PERF_CF_SENSOR_TYPE_PMU_FB)
            {
                utilization /= Utility::ColwertFXPToFloat(sensor.data.pmuFb.scale, 20, 12);
            }
        }
    }

    // Scale sensor output for better readability
    switch (topo.unit)
    {
        case LW2080_CTRL_PERF_PERF_CF_TOPOLOGY_UNIT_PERCENTAGE:
            // Colwert fraction to percentage
            utilization *= 100.0;
            break;

        case LW2080_CTRL_PERF_PERF_CF_TOPOLOGY_UNIT_BYTES_PER_NSEC:
            // Colwert B/ns to MB/s
            utilization *= 1'000'000'000.0 / (1024.0 * 1024.0);
            break;

        case LW2080_CTRL_PERF_PERF_CF_TOPOLOGY_UNIT_GHZ:
            // Colwert GHz to MHz
            utilization *= 1000.0;
            break;

        default:
            break;
    }

    return static_cast<float>(utilization);
}

RC GpuSubdevice::SetGpuUtilSensorInformation()
{
    RC rc;
    CHECK_RC(LwRmPtr()->ControlBySubdevice(this,
                                           LW2080_CTRL_CMD_PERF_PERF_CF_SENSORS_GET_INFO,
                                           &m_SensorInfo.sensors,
                                           sizeof(m_SensorInfo.sensors)));

    CHECK_RC(LwRmPtr()->ControlBySubdevice(this,
                                           LW2080_CTRL_CMD_PERF_PERF_CF_TOPOLOGYS_GET_INFO,
                                           &m_SensorInfo.info,
                                           sizeof(m_SensorInfo.info)));

    m_SensorInfo.status.super = m_SensorInfo.info.super;
    CHECK_RC(SetGpuUtilStatusInformation());
    m_SensorInfo.queried = true;
    return rc;
}

RC GpuSubdevice::SetGpuUtilStatusInformation()
{
    RC rc;
    CHECK_RC(LwRmPtr()->ControlBySubdevice(this,
                                           LW2080_CTRL_CMD_PERF_PERF_CF_TOPOLOGYS_GET_STATUS,
                                           &m_SensorInfo.status,
                                           sizeof(m_SensorInfo.status)));
    return rc;
}

GpuSubdevice::GpuSensorInfo GpuSubdevice::GetGpuUtilSensorInformation()
{
    return m_SensorInfo;
}

RC GpuSubdevice::GetBoardBOM(UINT32 *pVal)
{
    MASSERT(pVal);
    *pVal = ~0U;
    return RC::UNSUPPORTED_FUNCTION;
}
