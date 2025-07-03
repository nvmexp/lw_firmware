/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2011-2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#include "core/include/display.h"
#include "core/include/platform.h"
#include "core/include/utility.h"
#include "core/include/version.h"
#include "core/include/jsonparser.h"
#include "device/interface/pcie.h"
#include "gpu/fuse/fuse.h"       // For verification of the fuses
#include "gpu/include/dmawrap.h" // For verification using Copy engine
#include "gpu/include/gpusbdev.h"
#include "gpu/utility/surf2d.h"
#include "gpu/js/fpk_comm.h"     // For accessing common JS pickers
#include "gpu/perf/pmusub.h"
#include "gpu/tests/gputest.h"
#include "hwref/maxwell/gm108/dev_gc6_island.h"

#include <algorithm>
#include <cmath>
#include <unordered_map>

#ifdef LINUX_MFG
#include <unistd.h>
#endif


/*******************************************************************************
    This test is designed to test out the GC6 capabilities of the GPU.
    GC6 is a term used to identify that the GPU has gone into extremly low power
    consumption mode. This is done by putting the framebuffer into self-refresh
    mode and powering down the GPU.
    GC6 has two styles of implementation Kepler(GC6K) and Maxwell(GC6M).

    GC6K: This style ilwolves using external cirlwitry to put the FB into self-
    refresh and power down the GPU. This style only has a single type of wakeup
    mechanism called "gpu_event". The gpu_event is managed by either the SmbEC
    or the ACPI driver.

    GC6M: This style ilwolves using the GPU's power islands to put the FB into
    self-refresh mode and powering down the GPU. No external cirlwitry is needed
    to accomplish this task. In addition there are several differnt mechanisms
    for causing the GPU to self wakeup, they are gpu_event, i2c, timer,
    hotplug_detect, and hotplug_detect_irq. The control of these wakeup events
    is actually managed by either the SmbEC or the ACPI driver.
    When using the SmbEC all of the wakeup events can be generated, however when
    using the ACPI driver only the gpu_event is generated.

    The goal of this test is to put the GPU into GC6 mode for a random period of
    time, then generate a wakeup event to bring it out of GC6 and confirm that the
    FB is still intact. The following configurations are supported:
    Style   Driver      wakeup events tested    Comments
    -----   --------    --------------------    --------------------------------
    GC6K    SmbEC       gpu_event               production Eboards
    GC6K    ACPI        gpu_event               Customer platforms, with CS bds
    GC6M    SmbEC       gpu_event, i2c, timer,  production Eboards
                        hpd_plug, hpd_unplug,   not tested in mfg
                        hpd_irq                 not tested in mfg
    GC6M    ACPI        gpu_event               Customer platforms, with CS bds.

*******************************************************************************/
class GpuGcx : public GpuTest
{
public:
    GpuGcx();
    virtual ~GpuGcx() { }

    virtual void PrintJsProperties(Tee::Priority pri);
    virtual bool IsSupported();
    virtual RC Setup();
    virtual RC Run();
    virtual RC Cleanup();

    // Common GC5/GC6 properties
    SETGET_PROP(VerifyFB, bool);
    SETGET_PROP(PrintStats, bool);
    SETGET_PROP(PrintSummary, bool);
    SETGET_PROP(PrintMiscompares, bool);
    SETGET_PROP(PrintFuseStats, bool);
    SETGET_PROP(PrintAddrDecode, bool);
    SETGET_PROP(EventMask, UINT32);
    SETGET_PROP(TestAlgorithm, UINT32);

    // GC6 specific property
    SETGET_PROP(UseOptimus, bool);
    SETGET_PROP(DelayAfterLinkStatusMs, UINT32);
    SETGET_PROP(DelayAfterL2EntryMs, UINT32);
    SETGET_PROP(DelayAfterL2ExitMs, UINT32);
    SETGET_PROP(PollLinkStatusMs, UINT32);
    SETGET_PROP(UseJTMgr, bool);
    SETGET_PROP(ECEnterCmdToStatDelayMs, UINT32);
    SETGET_PROP(ECExitCmdToStatDelayMs, UINT32);
    SETGET_PROP(SkipSwFuseCheck, bool);
    SETGET_PROP(SkipFuseCheck, bool);
    SETGET_PROP(EnableLoadTimeCalc, bool);
    SETGET_PROP(LoadTimeInitEveryRun, bool);
    SETGET_PROP(EnableLoadTimeComparison, bool);
    SETGET_PROP(UseD3Hot, bool);
    SETGET_PROP(PciConfigSpaceRestoreDelayMs, UINT32);
    // GC5 specific property
    SETGET_PROP(WakeupGracePeriodMs, UINT32);
    SETGET_PROP(MaxAbortsPercent, float);
    SETGET_PROP(GC5Mode, UINT32);
    SETGET_PROP(ForcePstate, UINT32);
    SETGET_PROP(UseNativeACPI, bool);
    SETGET_PROP(PlatformType, string);

private:
    struct bootStraps
    {
        UINT32 boot0;
        UINT32 boot3;
        UINT32 boot6;
    };
    struct fuseInfo
    {
        UINT32 offset;  // register offset to read
        UINT32 exp;     // expected fuse value, read at start of the test
        UINT32 act;     // actual fuse value, read after each GC6 cycle (latched on failure)
        UINT32 failLoop;// loop number on first failure
        UINT32 passCnt; // number of times this fuse passed comparison check
        UINT32 failCnt; // number of times this fuse failed the comparison check
        fuseInfo(): offset(~0), exp(0), act(0), failLoop(~0), passCnt(0), failCnt(0) {}
        fuseInfo( fuseInfo *rhs)
        {
            offset = rhs->offset;
            exp = rhs->exp;
            act = rhs->act;
            failLoop = rhs->failLoop;
            passCnt = rhs->passCnt;
            failCnt = rhs->failCnt;
        }
        fuseInfo(UINT32 o, UINT32 e, UINT32 a, UINT32 l, UINT32 p, UINT32 f):
            offset(o), exp(e), act(a), failLoop(l), passCnt(p), failCnt(f){}
    };
    RC      AllocSurface();
    RC      CheckStats();
    RC      CollectAndPrintDebugInfo(Tee::Priority pri);
    RC      CompareBootStraps();
    RC      CompareFuseRecords(bool bReadFuse, bool bPrintBadRecords);
    RC      CompareEngineLoadTimeBaseline();
    UINT32  GetEventId(UINT32 index);
    UINT32  GetEventIndex(UINT32 wakeupEventId);
    RC      GetFuseRecords();
    RC      GetEngineLoadTimeBaseline();
    RC      InitializeFb();
    RC      InnerLoop();
    RC      PickLoopParams();
    void    Print(Tee::Priority pri);
    void    PrintFuseStats(Tee::Priority pri);

    RC      SetDefaultsForPicker(UINT32 Index);
    RC      VerifyFb();
    RC      VerifySurface(Surface2D *pSrc, Surface2D *pDst, Surface2D *pFbSurf);
    RC      VerifyWakeupReason();

    enum surfIds {
        surfGld
        ,surfFb
        ,surfSys
        ,surfNUM
    };
    struct wakeupStats
    {
        UINT32       minUs;
        UINT32       maxUs;
        unsigned int exits;
        unsigned int aborts;
        unsigned int miscompares;
        unsigned int diEntries;  // used only for DI_OS & DI_SSC
        unsigned int diExits;    // used only for DI_OS & DI_SSC
        unsigned int diAttempts; // used only for DI_OS & DI_SSC
        unsigned long expectedEntryTimeUs;
        unsigned long actualEntryTimeUs;
        wakeupStats(): minUs(~0), maxUs(0), exits(0), aborts(0), miscompares(0),
            diEntries(0), diExits(0), diAttempts(0), expectedEntryTimeUs(0L),
            actualEntryTimeUs(0L) {}
    };
    LW2080_CTRL_GPU_READ_DEEP_IDLE_STATISTICS_PARAMS m_PreDIStats;  //WAR1391576
    LW2080_CTRL_GPU_READ_DEEP_IDLE_STATISTICS_PARAMS m_LwrDIStats;  //WAR1391576
    LW2080_CTRL_GPU_READ_DEEP_IDLE_STATISTICS_PARAMS m_ExitDeepIdleStats;

    vector<pair<UINT32, UINT32> > m_BootStraps;
    FancyPicker::FpContext *m_pFpCtx;
    FancyPickerArray *      m_pPickers;

    // Variables to hold the JS properites
    UINT32          m_DelayAfterLinkStatusMs;
    UINT32          m_DelayAfterL2EntryMs = 10;
    UINT32          m_DelayAfterL2ExitMs = 10;
    UINT32          m_ECEnterCmdToStatDelayMs;
    UINT32          m_ECExitCmdToStatDelayMs;
    UINT32          m_EventMask;     // Bitmask of wakeup events to use
    UINT32          m_TestAlgorithm; // default, GC5Test or GC6Test
    UINT32          m_ForcePstate;   // if >0 then force this pstate
    float           m_MaxAbortsPercent;   // Percentage of the loops that are allowed to abort.
    UINT32          m_GC5Mode;  //DI-OS or DI-SSC
    UINT32          m_PollLinkStatusMs;
    bool            m_PrintMiscompares;
    bool            m_PrintStats;
    bool            m_PrintFuseStats;
    bool            m_PrintSummary;
    bool            m_PrintAddrDecode;
    bool            m_SkipSwFuseCheck;
    bool            m_SkipFuseCheck;
    bool            m_UseJTMgr;    // Use JTMgr to control either the SMBUS or ACPI driver.
    bool            m_UseOptimus;  // Use Optimus ACPI protocol.
    bool            m_VerifyFB;    // Verify the Framebuffer after each GC6 cycle.
    bool            m_EnableLoadTimeCalc;
    bool            m_LoadTimeInitEveryRun;
    bool            m_EnableLoadTimeComparison;
    bool            m_ShowWakeupResults;
    bool            m_WakeupMismatch;
    UINT32          m_WakeupGracePeriodMs;
    bool            m_UseNativeACPI = false;
    bool            m_UseD3Hot = false;
    UINT32          m_PciConfigSpaceRestoreDelayMs = 0;
    string          m_PlatformType = "CFL";

    // internal stats
    UINT64          m_EnterTimeMs;   // Total round trip time to enter GC5/GC6
    UINT64          m_ExitTimeMs;    // Total round trip time to exit GC5/GC6
    // index 0-4 = GC6 events, 5-9 = GC5 events
    wakeupStats m_Stats[GC_CMN_WAKEUP_EVENT_NUM_EVENTS];
    // average engine load time
    unordered_map<string, vector<UINT64>> m_EngineLoadTime;
    unordered_map<string, UINT64> m_EngineLoadTimeAvg;
    unordered_map<string, UINT64> m_EngineLoadTimeStd;
    unordered_map<string, double> m_EngineLoadTimeBaselineAvg;
    unordered_map<string, double> m_EngineLoadTimeBaselineStd;

    // Internal state variable
    UINT32          m_Caps;         // Gc6 Capabilities
    UINT32          m_DispDepth;
    UINT32          m_DispHeight;
    UINT32          m_DispRefresh;
    UINT32          m_DispWidth;
    UINT32          m_DmaMethod;
    DmaWrapper      m_DmaWrap;
    UINT32          m_EcFsmMode;    // GC6K or GC6M mode
    UINT32          m_Gc5Pstate;    // what pstate to use for this GC5 cycle
    UINT32          m_Gc6Pstate;    // what pstate is used for GC6 cycles
    UINT32          m_PwrDnPostDlyUsec;
    UINT32          m_PwrUpPostDlyUsec;
    Surface2D       m_Surf[surfNUM];// Surfaces used to verify GPU functionality
    bool            m_SurfAllocated;
    FLOAT64         m_TimeoutMs;
    Tee::Priority   m_VerbosePrint;
    UINT32          m_WakeupEvent;  // What type of wakeup event to use
    LW2080_CTRL_GCX_GET_WAKEUP_REASON_PARAMS m_WakeupParams;
    PStateOwner     m_PStateOwner;      //!< need to claim the pstates when doing GCx
    map<string, fuseInfo> m_FuseInfo;
    UINT08          m_InitialThermReading; //To store therm sensor reading before entering gc6

    // speedup variables
    Display *       m_pDisplay;
    GCxImpl *       m_pGCx;        // Implementor to execute the GC5/GC6 cycles.
    GpuSubdevice *  m_pSubdev;
    GpuTestConfiguration* m_pTestCfg;
    Perf::PerfPoint m_PerfPt;

};

JS_CLASS_INHERIT(GpuGcx, GpuTest, "GPU GCx test");

CLASS_PROP_READWRITE(GpuGcx, EnableLoadTimeComparison, bool,
                     "Compare load time of each engine to baseline, and emit error when load time is too high (default is false)");
CLASS_PROP_READWRITE(GpuGcx, LoadTimeInitEveryRun, bool,
                     "Whether to init records of load time in previous Run() invocation (default is false)");
CLASS_PROP_READWRITE(GpuGcx, EnableLoadTimeCalc, bool,
                     "Record load time of each engine (default is false)");
CLASS_PROP_READWRITE(GpuGcx, VerifyFB, bool,
                     "Verify the Framebuffer (default is true)");
CLASS_PROP_READWRITE(GpuGcx, UseOptimus, bool,
                     "Use Optimus to reset the GPU, if available (default is true)");
CLASS_PROP_READWRITE(GpuGcx, UseJTMgr, bool,
                     "Use JTMgr to reset the GPU, if available (default is true)");
CLASS_PROP_READWRITE(GpuGcx, PrintStats, bool,
                     "Print timing stats for each loop. (default is false)");
CLASS_PROP_READWRITE(GpuGcx, PrintFuseStats, bool,
                     "Print fuse stats at the end of the test. (default is false)");
CLASS_PROP_READWRITE(GpuGcx, PrintSummary, bool,
                     "Print timing summary at end of the test. (default is false)");
CLASS_PROP_READWRITE(GpuGcx, PrintMiscompares, bool,
                     "Print cycle stats on loops that miscompare. (default is false)");
CLASS_PROP_READWRITE(GpuGcx, PrintAddrDecode, bool,
                     "Print the address decode for pixel miscompares. (default is true)");
CLASS_PROP_READWRITE(GpuGcx, DelayAfterLinkStatusMs, UINT32,
                     "Delay after PEX link status is set, in ms (default is 0)");
CLASS_PROP_READWRITE(GpuGcx, PciConfigSpaceRestoreDelayMs, UINT32,
                     "Delay before accession PCI config space, in ms (default is 50)");
CLASS_PROP_READWRITE(GpuGcx, DelayAfterL2EntryMs, UINT32,
                     "Delay after link goes to L2, in ms (default is 10)");
CLASS_PROP_READWRITE(GpuGcx, DelayAfterL2ExitMs, UINT32,
                     "Delay after link come out of L2, in ms (default is 10)");
CLASS_PROP_READWRITE(GpuGcx, PollLinkStatusMs, UINT32,
                     "How long to poll for PEX link status to change, in ms (default is 100)");
CLASS_PROP_READWRITE(GpuGcx, ECEnterCmdToStatDelayMs, UINT32,
                     "EnterGCx delay after x msec (default = 0) after SMBus cmd before reading status.");
CLASS_PROP_READWRITE(GpuGcx, ECExitCmdToStatDelayMs, UINT32,
                     "ExitGCx delay after x msec (default = 0) after SMBus cmd before reading status.");
CLASS_PROP_READWRITE(GpuGcx, EventMask, UINT32,
                    "Bitmap of wakeup events to use.");
CLASS_PROP_READWRITE(GpuGcx, TestAlgorithm, UINT32,
                    "Type of test algorithm to use 0=default(gc5 and gc6), "
                    "0x1=GC6Test, 0x2=GC5Test, 0x4=RTD3, 0x8=FastGC6, "
                    "mask for mix cycles");
CLASS_PROP_READWRITE(GpuGcx, ForcePstate, INT32,
                    "Force to pstate x.intersect (default is randomly pick valid pstates)");
CLASS_PROP_READWRITE(GpuGcx, SkipSwFuseCheck, bool,
                    "Skip SW Fuse enable check (default = false).");
CLASS_PROP_READWRITE(GpuGcx, SkipFuseCheck, bool,
                    "Skip Fuse retention check (default = false).");
CLASS_PROP_READWRITE(GpuGcx, MaxAbortsPercent, float,
                    "Maximum percentage of GC5 entry aborts (default=10.0) before test failure");
CLASS_PROP_READWRITE(GpuGcx, WakeupGracePeriodMs, UINT32,
                     "Additional wait time after wakeup event should have oclwred. (default=100ms)");
CLASS_PROP_READWRITE(GpuGcx, GC5Mode, UINT32,
                     "DeepIdle mode to test. 1=DI-OS or 2=DI-SSC. (default=DI-OS)");
CLASS_PROP_READWRITE(GpuGcx, UseNativeACPI, bool,
                    "Use Native ACPI ON/OFF method for power cycle (default = false).");
CLASS_PROP_READWRITE(GpuGcx, UseD3Hot, bool,
                    "Use D3 Hot method (default = false).");
CLASS_PROP_READWRITE(GpuGcx, PlatformType, string,
                    "Platform type to use root port low power offset (default = CFL).");

GpuGcx::GpuGcx()
:
 m_DelayAfterLinkStatusMs(0)
,m_ECEnterCmdToStatDelayMs(0)
,m_ECExitCmdToStatDelayMs(0)
,m_EventMask(GC_CMN_WAKEUP_EVENT_gc6all_mfg |
    GC_CMN_WAKEUP_EVENT_gc5all | GC_CMN_WAKEUP_EVENT_rtd3all) // see fpk_comm.h
,m_TestAlgorithm(GCX_TEST_ALGORITHM_default)
,m_ForcePstate(Perf::ILWALID_PSTATE)
,m_MaxAbortsPercent(10.0)
,m_GC5Mode(GCxImpl::gc5DI_OS)
,m_PollLinkStatusMs(500)
,m_PrintMiscompares(false)
,m_PrintStats(false)
,m_PrintFuseStats(false)
,m_PrintSummary(false)
,m_PrintAddrDecode(true)
,m_SkipSwFuseCheck(false)
,m_SkipFuseCheck(false)
,m_UseJTMgr(true)
,m_UseOptimus(true)
,m_VerifyFB(true)
,m_EnableLoadTimeCalc(false)
,m_LoadTimeInitEveryRun(false)
,m_EnableLoadTimeComparison(false)
,m_ShowWakeupResults(false)
,m_WakeupMismatch(false)
,m_WakeupGracePeriodMs(5000)
,m_EnterTimeMs(0)
,m_ExitTimeMs(0)
,m_Caps(0)
,m_DispDepth(0)
,m_DispHeight(0)
,m_DispRefresh(0)
,m_DispWidth(0)
,m_DmaMethod(DmaWrapper::COPY_ENGINE)
,m_EcFsmMode(LW_EC_INFO_FW_CFG_FSM_MODE_GC6K)
,m_Gc5Pstate(0)
,m_Gc6Pstate(0)
,m_PwrDnPostDlyUsec(0)
,m_PwrUpPostDlyUsec(0)
,m_SurfAllocated(false)
,m_TimeoutMs(0)
,m_VerbosePrint(Tee::PriLow)
,m_WakeupEvent(GC_CMN_WAKEUP_EVENT_none)
,m_InitialThermReading(0)
,m_pDisplay(nullptr)
,m_pGCx(nullptr)
,m_pSubdev(nullptr)
,m_pTestCfg(nullptr)
{
    m_pFpCtx = GetFpContext();
    CreateFancyPickerArray(GCX_NUM_PICKERS);
    m_pPickers = GetFancyPickerArray();
    memset(&m_WakeupParams, 0, sizeof(m_WakeupParams));
    memset(&m_ExitDeepIdleStats, 0, sizeof(m_ExitDeepIdleStats));

    //WAR1391576
    memset(&m_LwrDIStats, 0, sizeof(m_LwrDIStats));
    memset(&m_PreDIStats, 0, sizeof(m_PreDIStats));
}

//--------------------------------------------------------------------------------------------------
void GpuGcx::PrintJsProperties(Tee::Priority pri)
{
    GpuTest::PrintJsProperties(pri);
    const char *tf[2] = { "false", "true" };
    const char *gc5Mode[] = {"unknown", "DI_OS", "DI_SSC"};
    Printf(pri, "GpuGcx Js Properties:\n");
    Printf(pri, "\t%-32s 0x%x\n",   "TestAlgorithm:",            m_TestAlgorithm);
    Printf(pri, "\t%-32s %s\n",     "VerifyFB:",                 tf[m_VerifyFB]);
    Printf(pri, "\t%-32s %s\n",     "PrintStats:",               tf[m_PrintStats]);
    Printf(pri, "\n%-32s %s\n",     "PrintFuseStats:",           tf[m_PrintFuseStats]);
    Printf(pri, "\t%-32s %s\n",     "PrintSummary:",             tf[m_PrintSummary]);
    Printf(pri, "\t%-32s %s\n",     "PrintMiscompares:",         tf[m_PrintMiscompares]);
    Printf(pri, "\t%-32s %s\n",     "PrintAddrDecode:",          tf[m_PrintAddrDecode]);
    Printf(pri, "\t%-32s %s\n",     "UseOptimus:",               tf[m_UseOptimus]);
    Printf(pri, "\t%-32s %s\n",     "UseJTMgr:",                 tf[m_UseJTMgr]);
    Printf(pri, "\t%-32s %ums\n",   "DelayAfterLinkStatusMs:",   m_DelayAfterLinkStatusMs);
    Printf(pri, "\t%-32s %ums\n",   "DelayAfterL2EntryMs:",      m_DelayAfterL2EntryMs);
    Printf(pri, "\t%-32s %ums\n",   "DelayAfterL2ExitMs:",       m_DelayAfterL2ExitMs);
    Printf(pri, "\t%-32s %ums\n",   "PollLinkStatusMs:",         m_PollLinkStatusMs);
    Printf(pri, "\t%-32s %ums\n",   "ECEnterCmdToStatDelayMs:",  m_ECEnterCmdToStatDelayMs);
    Printf(pri, "\t%-32s %ums\n",   "ECExitCmdToStatDelayMs:",   m_ECExitCmdToStatDelayMs);
    Printf(pri, "\t%-32s 0x%x\n",   "EventMask:",                m_EventMask);
    Printf(pri, "\t%-32s %d\n",     "ForcePstate:",              m_ForcePstate);
    Printf(pri, "\t%-32s %s\n",     "SkipSwFuseCheck:",          tf[m_SkipSwFuseCheck]);
    Printf(pri, "\t%-32s %s\n",     "SkipFuseCheck:",            tf[m_SkipFuseCheck]);
    Printf(pri, "\t%-32s %s\n",     "EnableLoadTimeCalc:",       tf[m_EnableLoadTimeCalc]);
    Printf(pri, "\t%-32s %s\n",     "LoadTimeInitEveryRun:",     tf[m_LoadTimeInitEveryRun]);
    Printf(pri, "\t%-32s %s\n",     "EnableLoadTimeComparison:", tf[m_EnableLoadTimeComparison]);
    Printf(pri, "\t%-32s %d\n",     "WakeupGracePeriodMs:",      m_WakeupGracePeriodMs);
    Printf(pri, "\t%-32s %3.2f\n",  "MaxAbortsPercent:",         m_MaxAbortsPercent);
    Printf(pri, "\t%-32s %s\n",     "GC5Mode:",                  gc5Mode[m_GC5Mode]);
    Printf(pri, "\t%-32s %s\n",     "UseNativeACPI:",            tf[m_UseNativeACPI]);
}

//--------------------------------------------------------------------------------------------------
bool GpuGcx::IsSupported()
{
    m_pSubdev = GetBoundGpuSubdevice();
    m_pGCx = m_pSubdev->GetGCxImpl();
    if (m_pGCx == nullptr)
    {
        return false;
    }

    m_pGCx->SetBoundRmClient(GetBoundRmClient());
    m_pGCx->SetVerbosePrintPriority(GetVerbosePrintPri());
    m_pGCx->SetGc5Mode(static_cast<GCxImpl::gc5Mode>(m_GC5Mode));
    GetBoundGpuSubdevice()->GetPerf()->GetLwrrentPerfPoint(&m_PerfPt);
    if (m_TestAlgorithm == GCX_TEST_ALGORITHM_default)
    {
        if (m_pSubdev->HasFeature(Device::GPUSUB_SUPPORTS_GC5) &&
            !m_pGCx->IsGc5Supported(Perf::ILWALID_PSTATE))
        {
            return false;
        }
        if (m_pSubdev->HasFeature(Device::GPUSUB_SUPPORTS_RTD3) &&
            !m_pGCx->IsRTD3Supported(m_pGCx->GetGc6Pstate(), m_UseNativeACPI))
        {
            return false;
        }
        if (m_pSubdev->HasFeature(Device::GPUSUB_SUPPORTS_LEGACY_GC6) &&
            !m_pGCx->IsGc6Supported(m_pGCx->GetGc6Pstate()))
        {
            return false;
        }
    }
    if (m_TestAlgorithm & GCX_TEST_ALGORITHM_gc5)
    {
        UINT32 pstate = m_ForcePstate;
        if (pstate == Perf::ILWALID_PSTATE)
            m_pSubdev->GetPerf()->GetLwrrentPState(&pstate);
        if (!m_pGCx->IsGc5Supported(pstate))
        {
            return false;
        }
    }
    if (m_TestAlgorithm & GCX_TEST_ALGORITHM_gc6)
    {
        if (!m_pSubdev->HasFeature(Device::GPUSUB_SUPPORTS_LEGACY_GC6) ||
            !m_pGCx->IsGc6Supported(m_pGCx->GetGc6Pstate()))
        {
            return false;
        }
    }
    if (m_TestAlgorithm & GCX_TEST_ALGORITHM_rtd3)
    {
        if (!m_pGCx->IsRTD3Supported(m_pGCx->GetGc6Pstate(), m_UseNativeACPI))
        {
            return false;
        }
    }
    if (m_TestAlgorithm & GCX_TEST_ALGORITHM_fgc6)
    {
        if (!m_pGCx->IsFGc6Supported(m_pGCx->GetGc6Pstate()))
        {
            return false;
        }
    }
    if (m_UseD3Hot && m_TestAlgorithm != GCX_TEST_ALGORITHM_rtd3)
    {
        return false;
    }
    return true;
}

//--------------------------------------------------------------------------------------------------
RC GpuGcx::Setup()
{
    RC rc;

    m_pSubdev = GetBoundGpuSubdevice();
    m_pGCx = m_pSubdev->GetGCxImpl();
    Perf *pPerf = m_pSubdev->GetPerf();
    m_pTestCfg  = GetTestConfiguration();
    m_TimeoutMs = m_pTestCfg->TimeoutMs();

    if (m_pGCx == nullptr)
    {
        Printf(Tee::PriError, "Missing implementation of GCxImpl object!\n");
        return RC::SOFTWARE_ERROR;
    }

    CHECK_RC(pPerf->GetLwrrentPerfPoint(&m_PerfPt));
    CHECK_RC(GpuTest::Setup());
    // can't do this until after GpuTest::Setup();
    m_VerbosePrint = m_pTestCfg->Verbose() ? Tee::PriNormal : Tee::PriLow;
    CHECK_RC(m_PStateOwner.ClaimPStates(m_pSubdev));
    CHECK_RC(GpuTest::AllocDisplay()); // Allocate display so we can turn it off
    CHECK_RC(AllocSurface());

    m_pDisplay = GetDisplay();
    if (m_pDisplay->GetDisplayClassFamily() != Display::NULLDISP)
    {
        // Display must be disabled for GC5(DI-OS or DI-SSC), or GC6
        // For LwDisplay it will be disabled by DeactivateCompostion
        if (m_pDisplay->GetDisplayClassFamily() == Display::EVO)
        {
            CHECK_RC(m_pDisplay->GetMode(m_pDisplay->Selected(),
                &m_DispWidth, &m_DispHeight, &m_DispDepth, &m_DispRefresh));
            m_pDisplay->Disable();
        }

        if (m_pSubdev->HasBug(200029713))
        {
            // If the display clock is being sourced from the DispPLL instead of the
            // OneSrcClk BypassPLL. Gc5 will not enter if the DispPLL is being used.
            // So reduce the clock and force it to the BypassPLL.
            // Note: when we restore the perf point the original clock value should get restored.
            Gpu::ClkSrc clkSrc;
            CHECK_RC(m_pSubdev->GetClock(Gpu::ClkDisp, 0, 0, 0, &clkSrc));
            if (clkSrc == Gpu::ClkSrcDISPPLL)
            {
                Perf::ClkRange clkRange;
                CHECK_RC(m_pSubdev->GetPerf()->GetClockRange(
                                m_PerfPt.PStateNum, Gpu::ClkDisp, &clkRange));
                Perf::PerfPoint perfPt = m_PerfPt;
                if (perfPt.Clks.find(Gpu::ClkDisp) != perfPt.Clks.end())
                {
                    perfPt.Clks[Gpu::ClkDisp].FreqHz = clkRange.MinKHz*1000;
                    CHECK_RC(pPerf->SetPerfPoint(perfPt));
                }
            }
        }
    }

    // Initialize the GCx implementor for GC5 & GC6 features
    m_pGCx->SetVerbosePrintPriority(GetVerbosePrintPri());
    m_pGCx->SetSkipSwFuseCheck(m_SkipSwFuseCheck);
    m_pGCx->SetWakeupGracePeriodMs(m_WakeupGracePeriodMs);
    m_pGCx->SetBoundRmClient(GetBoundRmClient());
    m_pGCx->SetUseNativeACPI(m_UseNativeACPI);
    m_pGCx->SetPciConfigSpaceRestoreDelay(m_PciConfigSpaceRestoreDelayMs);
    m_pGCx->SetPlatformType(m_PlatformType);
    m_pGCx->SetEnableLoadTimeCalc(m_EnableLoadTimeCalc);
    CHECK_RC(m_pGCx->SetUseD3Hot(m_UseD3Hot));

    if (m_TestAlgorithm == GCX_TEST_ALGORITHM_default)
    {
        if (m_pSubdev->HasFeature(Device::GPUSUB_SUPPORTS_GC5))
        {
            CHECK_RC(m_pGCx->InitGc5());
        }
        CHECK_RC(m_pGCx->InitGc6(m_TimeoutMs));
        m_Gc6Pstate = m_pGCx->GetGc6Pstate();
    }
    else
    {
        if (m_TestAlgorithm & GCX_TEST_ALGORITHM_gc5)
        {
            CHECK_RC(m_pGCx->InitGc5());
        }
        if (m_TestAlgorithm & GCX_TEST_ALGORITHM_gc6 ||
            m_TestAlgorithm & GCX_TEST_ALGORITHM_rtd3 ||
            m_TestAlgorithm & GCX_TEST_ALGORITHM_fgc6)
        {
            CHECK_RC(m_pGCx->InitGc6(m_TimeoutMs));
            m_Gc6Pstate = m_pGCx->GetGc6Pstate();
        }
    }

    if (m_UseJTMgr)
    {
        Xp::JTMgr::DebugOpts dbgOpts =
        {
            m_ECEnterCmdToStatDelayMs,
            m_ECExitCmdToStatDelayMs,
            m_DelayAfterLinkStatusMs,
            m_DelayAfterL2EntryMs,
            m_DelayAfterL2ExitMs,
            m_PollLinkStatusMs,
            m_VerbosePrint
        };
        m_pGCx->SetUseJTMgr(m_UseJTMgr);
        m_pGCx->SetDebugOptions(dbgOpts);
        m_Caps = m_pGCx->GetGc6Caps();
        m_EcFsmMode = m_pGCx->GetGc6FsmMode();
    }

    return rc;
}

//------------------------------------------------------------------------------
RC GpuGcx::Cleanup()
{
    StickyRC rc;

    rc = m_pGCx->SetUseD3Hot(false);
    m_pGCx->SetUseNativeACPI(false);
    m_pGCx->Shutdown(); // shutdown and release any resources used by this implementor

    if (m_SurfAllocated)
    {
        m_DmaWrap.Wait(m_pTestCfg->TimeoutMs());
        m_DmaWrap.Cleanup();
        m_Surf[surfGld].Free();
        m_Surf[surfFb].Free();
        m_Surf[surfSys].Free();
    }

    if ((m_DispWidth != 0) && (m_DispHeight != 0) &&
        (m_DispDepth != 0) && (m_DispRefresh != 0) &&
        (m_pDisplay->GetDisplayClassFamily() != Display::NULLDISP))
    {
        rc = m_pDisplay->SetMode(m_DispWidth, m_DispHeight,
                                 m_DispDepth, m_DispRefresh);
    }

    Perf  *pPerf = m_pSubdev->GetPerf();
    Printf(Tee::PriNormal, "Restoring PerfPt %s\n",
        m_PerfPt.name(pPerf->IsPState30Supported()).c_str());
    rc = pPerf->SetPerfPoint(m_PerfPt);

    m_PStateOwner.ReleasePStates();

    rc = GpuTest::Cleanup();

    return rc;
}

//------------------------------------------------------------------------------
RC GpuGcx::SetDefaultsForPicker(UINT32 Index)
{
    FancyPickerArray * pPickers = GetFancyPickerArray();
    FancyPicker &fp = (*pPickers)[Index];
    RC rc = OK;
    //TODO: Need to come up with real timming values.
    // This diagram is somewhat outdated but still useful.
    // power down sequence                   power up sequence
    // ~fb_clmp:_____1                                             9__________10
    //               |__________________~    ______________________|
    // ~pex_rst:__________2                                    8_______________
    //                    |_____________~    __________________|
    // pwr_good:________________                        7______________________
    //                          |_______~    ___________|
    // pwr_en  :________________3                6_____________________________
    //                          |______4~    5___|
    //
    // Refer to the timming chart above for the proper delays
    switch (Index)
    {
        case GCX_PWR_DN_POST_DELAY  : // delay from 3 -> 4
            fp.ConfigRandom();
            fp.AddRandRange(25, 0, 10000);    // 0-10 msec
            fp.CompileRandom();
            break;

        case GCX_PWR_UP_POST_DELAY : // delay from 9 -> 10
            fp.ConfigRandom();
            fp.AddRandRange(25, 1000, 20000);    // 1-20 msec, maybe longer
            fp.CompileRandom();
            break;

        case GCX_PWR_DELAY_MAX   :
            fp.ConfigConst(0xffffffff); // delay for a given set of loops.
            break;

        case GC_CMN_WAKEUP_EVENT :
            MASSERT(m_EventMask);
            fp.ConfigRandom();
            if (m_EventMask & GC_CMN_WAKEUP_EVENT_gc6gpu)
                fp.AddRandItem(10, GC_CMN_WAKEUP_EVENT_gc6gpu);

            if (m_EventMask & GC_CMN_WAKEUP_EVENT_gc6i2c)
                fp.AddRandItem( 5, GC_CMN_WAKEUP_EVENT_gc6i2c);

            if (m_EventMask & GC_CMN_WAKEUP_EVENT_gc6timer)
                fp.AddRandItem( 5, GC_CMN_WAKEUP_EVENT_gc6timer);

            // These events require special blue wire modifications to the board
            // So don't enable them by default.
            // see B1321624
            if (m_EventMask & GC_CMN_WAKEUP_EVENT_gc6hotplug)
                fp.AddRandItem( 5, GC_CMN_WAKEUP_EVENT_gc6hotplug);

            if (m_EventMask & GC_CMN_WAKEUP_EVENT_gc6hotunplug)
                fp.AddRandItem( 5, GC_CMN_WAKEUP_EVENT_gc6hotunplug);

            if (m_EventMask & GC_CMN_WAKEUP_EVENT_gc6hotplugirq)
                fp.AddRandItem( 5, GC_CMN_WAKEUP_EVENT_gc6hotplugirq);

            if (m_EventMask & GC_CMN_WAKEUP_EVENT_gc6i2c_bypass)
                fp.AddRandItem( 5, GC_CMN_WAKEUP_EVENT_gc6i2c_bypass);

            // force a wakeup by reading a GPU register.
            if (m_EventMask & GC_CMN_WAKEUP_EVENT_gc5pex)
                fp.AddRandItem(10, GC_CMN_WAKEUP_EVENT_gc5pex);

            // force a wakeup using the RM Ctrl API.
            if (m_EventMask & GC_CMN_WAKEUP_EVENT_gc5rm)
                fp.AddRandItem( 5, GC_CMN_WAKEUP_EVENT_gc5rm);

            // force a wakeup by issuing an I2C cmd
            if (m_EventMask & GC_CMN_WAKEUP_EVENT_gc5i2c)
                fp.AddRandItem( 10, GC_CMN_WAKEUP_EVENT_gc5i2c);

            // use SCI's internal timer
            if (m_EventMask & GC_CMN_WAKEUP_EVENT_gc5timer)
                fp.AddRandItem( 5, GC_CMN_WAKEUP_EVENT_gc5timer);

            // use LW2080_CTRL_CMD_TIMER_SCHEDULE to schedule a GPU TIMER event.
            if (m_EventMask & GC_CMN_WAKEUP_EVENT_gc5alarm)
                fp.AddRandItem( 5, GC_CMN_WAKEUP_EVENT_gc5alarm);

            fp.CompileRandom();
            break;

        case GCX_WAKEUP_EVENT_TIMER_VAL: // in usecs
            fp.ConfigRandom();
            fp.AddRandRange(25, 100, 0xffffff);
            fp.CompileRandom();
            break;

        case GCX_WAKEUP_EVENT_ALARM_TIMER_VAL: // in usec
            fp.ConfigRandom();
            fp.AddRandRange(25, 100, 100000); // 100us - 100ms
            fp.CompileRandom();
            break;

        case GCX_GC5_PSTATE:
            fp.ConfigList(FancyPicker::WRAP);
            fp.AddListItem(5, 15);  // pstate 5 10 times
            fp.AddListItem(8, 5);  // pstate 8 10 times
            break;

        default:
            MASSERT(!"Your new picker doesn't have a default configuration");
            rc = RC::SOFTWARE_ERROR;
            break;
    }
    return rc;
}

//------------------------------------------------------------------------------
RC GpuGcx::AllocSurface()
{
    RC rc;
    // When being run as a callback, don't do any surface allocation
    if (!m_VerifyFB)
        return rc;

    for (int i=0; i<surfNUM; i++)
    {
        switch ( i ) {
            case surfSys: m_Surf[i].SetLocation(Memory::Coherent); break;
            case surfGld: m_Surf[i].SetLocation(Memory::Coherent); break;
            case surfFb : m_Surf[i].SetLocation(Memory::Fb); break;
        }
        m_Surf[i].SetWidth(m_pTestCfg->SurfaceWidth());
        m_Surf[i].SetHeight(m_pTestCfg->SurfaceHeight());
        m_Surf[i].SetColorFormat(ColorUtils::A8R8G8B8);
        m_Surf[i].SetProtect(Memory::ReadWrite);
        m_Surf[i].SetLayout(Surface::Pitch);
        CHECK_RC(m_Surf[i].Alloc(m_pSubdev->GetParentDevice()));
        CHECK_RC(m_Surf[i].Map(m_pSubdev->GetSubdeviceInst()));
    }
    m_SurfAllocated = true;

    // allocate DMA objects. Only do this if there are surfaces to work with
    DmaCopyAlloc alloc;
    if ((DmaWrapper::COPY_ENGINE == m_DmaMethod) &&
        !(alloc.IsSupported(m_pSubdev->GetParentDevice())))
    {
        Printf(Tee::PriLow, "Copy Engine not supported. Using TwoD.\n");
        m_DmaMethod = DmaWrapper::TWOD;
    }

    m_DmaWrap = DmaWrapper();
    CHECK_RC(m_DmaWrap.Initialize(m_pTestCfg,
                                  m_pTestCfg->NotifierLocation(),
                                  (DmaWrapper::PREF_TRANS_METH)m_DmaMethod));
    return rc;
}

//------------------------------------------------------------------------------
RC GpuGcx::Run()
{
    StickyRC rc;
    if (m_TestAlgorithm == GCX_TEST_ALGORITHM_gc5)
    {
        Printf(GetVerbosePrintPri(), "GpuGcx test emulating GC5 Test with %s cycles.\n",
            m_GC5Mode == GCxImpl::gc5DI_OS ? "DI_OS" : "DI_SSC");
    }
    else if (m_TestAlgorithm == GCX_TEST_ALGORITHM_gc6)
    {
        Printf(GetVerbosePrintPri(), "GpuGcx test emulating GC6 Test.\n");
    }
    // Initialize the contents of the FB memory
    // TODO:maybe we should change the contents on each loop
    CHECK_RC(InitializeFb());
    CHECK_RC(m_pSubdev->GetStraps(&m_BootStraps));
    CHECK_RC(GetFuseRecords());
    CHECK_RC(GetEngineLoadTimeBaseline());

    // Initialize object that store load time
    if (m_LoadTimeInitEveryRun) 
    {
        m_EngineLoadTime.clear();
        m_EngineLoadTimeAvg.clear();
        m_EngineLoadTimeStd.clear();
    }

    for (m_pFpCtx->LoopNum = 0; m_pFpCtx->LoopNum < m_pTestCfg->Loops();
         m_pFpCtx->LoopNum++)
    {
        rc = InnerLoop();
        if (OK != rc)
        {
            Printf(Tee::PriError, "GpuGcx returned rc:%d\n", rc.Get());
            break;
        }
    }
    CHECK_RC(CheckStats());
    return rc;
}

//------------------------------------------------------------------------------
RC GpuGcx::InnerLoop()
{
    StickyRC rc;

    CHECK_RC(PickLoopParams());

    // Enter GC5 or GC6 cycle
    if (m_WakeupEvent & GC_CMN_WAKEUP_EVENT_gc6all)
    {
        if (m_WakeupEvent & GC_CMN_WAKEUP_EVENT_gc6i2c_bypass)
        {
            m_pGCx->ReadThermSensor(&m_InitialThermReading, 10);
            CHECK_RC(m_pGCx->EnableFakeThermSensor(true));
        }

        CHECK_RC(m_pGCx->DoEnterGc6(m_WakeupEvent, m_PwrDnPostDlyUsec));
    }
    else if (m_WakeupEvent & GC_CMN_WAKEUP_EVENT_gc5all)
    {
        CHECK_RC(m_pGCx->DoEnterGc5(m_Gc5Pstate, m_WakeupEvent,
                                    m_PwrDnPostDlyUsec));
    }
    else
    {
        return RC::SOFTWARE_ERROR;
    }

    m_EnterTimeMs = m_pGCx->GetEnterTimeMs();

    const bool bRestorePstate = true;
    // Exit GC5 or GC6 cycle
    if (m_WakeupEvent & GC_CMN_WAKEUP_EVENT_gc6all)
    {
        CHECK_RC(m_pGCx->DoExitGc6(m_PwrUpPostDlyUsec, bRestorePstate));

        if (m_WakeupEvent & GC_CMN_WAKEUP_EVENT_gc6i2c_bypass)
            CHECK_RC(m_pGCx->EnableFakeThermSensor(false));

        CHECK_RC(CompareBootStraps());

        if (m_EnableLoadTimeCalc) 
        {
            // store each round's engine load time
            LW2080_CTRL_GPU_GET_ENGINE_LOAD_TIMES_PARAMS params  = m_pGCx->GetEngineLoadTimeParams();
            LW2080_CTRL_GPU_GET_ID_NAME_MAPPING_PARAMS   mapping = m_pGCx->GetEngineIdNameMappingParams();
            for (UINT32 i = 0; i < params.engineCount; i++)
            {
                if (!params.engineIsInit[i])
                {
                    continue;
                }
                
                auto retrieveLoadTime = [&](UINT32 id) 
                {
                    for (UINT32 j = 0; j < mapping.engineCount; j++)
                    {
                        if (mapping.engineID[j] == id)
                        {
                            return string(mapping.engineName[j]); 
                        }
                    }
                    MASSERT(!"Engine ID not found!");
                    return string("");
                };

                m_EngineLoadTime[retrieveLoadTime(params.engineList[i])].push_back(params.engineStateLoadTime[i]);
            }
        }

        // Log any fuse retention failure but don't return on failure yet. We will check again at
        // the end of the test.
        const bool bReadFuse = true, bPrintBadRecords = false;
        CompareFuseRecords(bReadFuse, bPrintBadRecords);
    }
    else if (m_WakeupEvent & GC_CMN_WAKEUP_EVENT_gc5all)
    {
        CHECK_RC(m_pGCx->DoExitGc5(m_PwrUpPostDlyUsec, bRestorePstate));
        CHECK_RC(m_pGCx->GetDeepIdleStats(&m_ExitDeepIdleStats));
    }
    else
    {
        Printf(Tee::PriError, "Error Unknown wakeup event:0x%x\n",
               m_WakeupEvent);
        return RC::SOFTWARE_ERROR;
    }

    m_ExitTimeMs = m_pGCx->GetExitTimeMs();

    if (OK == (rc = VerifyWakeupReason()))
    {
        // Verify the FB is not corrupted
        rc = VerifyFb();
    }

    Print(Tee::PriNormal);
    return rc;
}

//------------------------------------------------------------------------------
// Verify the actual wakeup reason against the wakeup event.
RC GpuGcx::VerifyWakeupReason()
{
    RC rc;
    bool bShowResults = m_PrintStats;
    bool bVerifyWakeup = true;
    UINT32 mask = 0;
    UINT32 eventIdx = GetEventIndex(m_WakeupEvent);
    if (eventIdx >= GC_CMN_WAKEUP_EVENT_NUM_EVENTS)
    {
        Printf(Tee::PriError,
               "GpuGcx::VerifyWakeupReason(), Invalid m_WakeupEvent detected!");
        return RC::SOFTWARE_ERROR;
    }
    m_pGCx->GetWakeupParams(&m_WakeupParams);
    m_ShowWakeupResults = false;
    m_WakeupMismatch = false;

    if (m_WakeupEvent & GC_CMN_WAKEUP_EVENT_gc5all)
    {
        m_Stats[eventIdx].diEntries         += m_ExitDeepIdleStats.entries;
        m_Stats[eventIdx].diExits           += m_ExitDeepIdleStats.exits;
        m_Stats[eventIdx].diAttempts        += m_ExitDeepIdleStats.attempts;
        m_Stats[eventIdx].actualEntryTimeUs += m_ExitDeepIdleStats.time;
        m_Stats[eventIdx].expectedEntryTimeUs += m_PwrDnPostDlyUsec;
        if (m_GC5Mode == GCxImpl::gc5DI_OS)
        {
            // we don't really know what type of entries/exits oclwred but we do know how many
            if (m_ExitDeepIdleStats.entries == 0)
                m_Stats[eventIdx].aborts++;
            else
                m_Stats[eventIdx].exits++;

            return OK;
        }

        if (m_WakeupParams.gc5ExitType ==
            LW2080_CTRL_GCX_GET_WAKEUP_REASON_GC5_SSC_ABORT)
        {
            // Lwrrently there is only 1 abort that would be acceptable
            if ( (m_WakeupEvent == GC_CMN_WAKEUP_EVENT_gc5alarm) &&
                 (FLD_TEST_DRF(2080_CTRL,
                               _GCX_GET_WAKEUP_REASON_GC5,
                               _SSC_ABORT_SPELWLATIVE_PTIMER_ALARM,
                               _YES,
                               m_WakeupParams.gc5AbortCode)))
            {
                m_Stats[eventIdx].exits++;
            }
            else
            {
                rc = RC::HW_STATUS_ERROR;
                bShowResults |= m_PrintMiscompares;
                bVerifyWakeup = false;
                m_Stats[eventIdx].aborts++;
            }
        }
        else
        {
            m_Stats[eventIdx].exits++;
        }
    }
    // Verify the wakeup reason is what is expected.
    if (bVerifyWakeup)
    {
        m_Stats[eventIdx].maxUs = max(m_Stats[eventIdx].maxUs,
                                      m_PwrDnPostDlyUsec);
        m_Stats[eventIdx].minUs = min(m_Stats[eventIdx].minUs,
                                      m_PwrDnPostDlyUsec);
        Xp::JTMgr::CycleStats stats;
        m_pGCx->GetGc6CycleStats(&stats);
        switch (m_WakeupEvent)
        {
            // Falling to the gc6gpu event is intentional to verify if
            // GPU actually wake up
            case GC_CMN_WAKEUP_EVENT_gc6i2c_bypass:
                if (stats.exitWakeupEventStatus != OK)
                {
                    rc = stats.exitWakeupEventStatus;
                    bShowResults |= m_PrintMiscompares;
                    if (rc == RC::SOFTWARE_ERROR)
                        m_Stats[eventIdx].miscompares++;
                    else
                    {
                        m_Stats[eventIdx].aborts++;
                        Printf(Tee::PriError, "Fakethermsensor failed the I2C transaction\n");
                    }
                    break;
                }
                else
                {
                    // Verify if the thermal sensor is coming up correctly
                    // Not actually failing for now as it is just for analysis

                    UINT08 temp[5] = {0}; //To read thermsensor 5 times
                    UINT08 sum = 0;
                    for (UINT32 i = 0; i < 5; i++)
                    {
                        m_pGCx->ReadThermSensor(temp + i, 10);
                        sum += temp[i];
                    }

                    UINT08 max = *max_element(temp, temp + 5);
                    UINT08 min = *min_element(temp, temp + 5);

                    //If we are testing at 0C, don't fail
                    if (m_InitialThermReading == 0 && sum == 0)
                    {
                        Printf(m_VerbosePrint, "Skip verifying thermal sensor as we are testing "
                                "at 0C temperature\n");
                    }
                    //If fakethermalsensor is activated for a very long time (lwrrently 20 sec),
                    //temperature might always report back 0, don't fail here
                    else if (m_PwrDnPostDlyUsec > 20000000 && sum == 0)
                    {
                        Printf(m_VerbosePrint, "Fakethermsensor was activated for a very long "
                                "time, temperature is always coming up 0\n");
                    }
                    // Failing case when thermal reading increased after this gc6 event
                    else if (temp[0] > m_InitialThermReading)
                    {
                        Printf(m_VerbosePrint, "All parts should be cooler when power is removed under "
                                "any thermal environment\n");
                    }
                    //Fail if the temperature reading are not bounded within 5 degrees
                    else if (abs(max - min) > 5)
                    {
                        Printf(m_VerbosePrint, "Thermal sensor is not coming up correctly, "
                                "as temperature is not bounded within 5 degrees\n");
                    }
                } //intentional fall-through

            case GC_CMN_WAKEUP_EVENT_gc6gpu:
                m_Stats[eventIdx].exits++;
                if (!FLD_TEST_DRF(_PGC6, _SCI_SW_INTR0, _STATUS_LWRR_GPU_EVENT,
                    _PENDING, m_WakeupParams.sciIntr0))
                {
                    rc = RC::HW_STATUS_ERROR;
                    bShowResults |= m_PrintMiscompares;
                    m_Stats[eventIdx].miscompares++;
                }
                break;

            case GC_CMN_WAKEUP_EVENT_gc6i2c:
                m_Stats[eventIdx].exits++;
                if (!FLD_TEST_DRF(_PGC6, _SCI_SW_INTR0, _STATUS_LWRR_I2CS,
                                  _PENDING, m_WakeupParams.sciIntr0))
                {
                    rc = RC::HW_STATUS_ERROR;
                    bShowResults |= m_PrintMiscompares;
                    m_Stats[eventIdx].miscompares++;
                    break;
                }

                // Did the I2C read actually return correct data? This verifies if
                // the replay mechanism actually functioned properly.
                if (stats.exitWakeupEventStatus != OK)
                {
                    Printf(Tee::PriNormal,
                        "I2C replay mechanism is faulty.Please update your VBIOS\n");
                    return stats.exitWakeupEventStatus;
                }
                break;

            case GC_CMN_WAKEUP_EVENT_gc6timer:
                m_Stats[eventIdx].exits++;
                if (!FLD_TEST_DRF(_PGC6, _SCI_SW_INTR0, _STATUS_LWRR_WAKE_TIMER,
                                  _PENDING, m_WakeupParams.sciIntr0))
                {
                    rc = RC::HW_STATUS_ERROR;
                    bShowResults |= m_PrintMiscompares;
                    m_Stats[eventIdx].miscompares++;
                }
                break;

            // Nothing to be done for CPU wake
            case GC_CMN_WAKEUP_EVENT_gc6rtd3_cpu:
                m_Stats[eventIdx].exits++;
                break;

            case GC_CMN_WAKEUP_EVENT_gc6rtd3_gputimer:
                m_Stats[eventIdx].exits++; 
                if (stats.exitGpuWakeStatus == 0)                
                {
                    rc = RC::HW_STATUS_ERROR;
                    bShowResults |= m_PrintMiscompares;
                    m_Stats[eventIdx].miscompares++;
                }
                break;

            case GC_CMN_WAKEUP_EVENT_gc6hotplug:
            case GC_CMN_WAKEUP_EVENT_gc6hotunplug:
                m_Stats[eventIdx].exits++;
                // create a mask of all the pending steady state HotPlugDetect(HPD) interrupts
                mask =
                   (DRF_DEF(_PGC6, _SCI_SW_INTR1, _STATUS_LWRR_HPD_0, _PENDING) |
                    DRF_DEF(_PGC6, _SCI_SW_INTR1, _STATUS_LWRR_HPD_1, _PENDING) |
                    DRF_DEF(_PGC6, _SCI_SW_INTR1, _STATUS_LWRR_HPD_2, _PENDING) |
                    DRF_DEF(_PGC6, _SCI_SW_INTR1, _STATUS_LWRR_HPD_3, _PENDING) |
                    DRF_DEF(_PGC6, _SCI_SW_INTR1, _STATUS_LWRR_HPD_4, _PENDING) |
                    DRF_DEF(_PGC6, _SCI_SW_INTR1, _STATUS_LWRR_HPD_5, _PENDING));

                if ((m_WakeupParams.sciIntr1 & mask) == 0)
                {
                    rc = RC::HW_STATUS_ERROR;
                    bShowResults |= m_PrintMiscompares;
                    m_Stats[eventIdx].miscompares++;
                }
                break;

            case GC_CMN_WAKEUP_EVENT_gc6hotplugirq:
                m_Stats[eventIdx].exits++;
                // create a mask of all the pending pulsing HotPlugDetect(HPD) interrupts
                mask =
                   (DRF_DEF(_PGC6, _SCI_SW_INTR0, _STATUS_LWRR_HPD_IRQ_0, _PENDING) |
                    DRF_DEF(_PGC6, _SCI_SW_INTR0, _STATUS_LWRR_HPD_IRQ_1, _PENDING) |
                    DRF_DEF(_PGC6, _SCI_SW_INTR0, _STATUS_LWRR_HPD_IRQ_2, _PENDING) |
                    DRF_DEF(_PGC6, _SCI_SW_INTR0, _STATUS_LWRR_HPD_IRQ_3, _PENDING) |
                    DRF_DEF(_PGC6, _SCI_SW_INTR0, _STATUS_LWRR_HPD_IRQ_4, _PENDING) |
                    DRF_DEF(_PGC6, _SCI_SW_INTR0, _STATUS_LWRR_HPD_IRQ_5, _PENDING));

                if ((m_WakeupParams.sciIntr0 & mask) == 0)
                {
                    rc = RC::HW_STATUS_ERROR;
                    bShowResults |= m_PrintMiscompares;
                    m_Stats[eventIdx].miscompares++;
                }
                break;

            case GC_CMN_WAKEUP_EVENT_gc5rm:
            case GC_CMN_WAKEUP_EVENT_gc5pex:
                if (!FLD_TEST_DRF(_PGC6, _SCI_SW_INTR0, _STATUS_LWRR_BSI_EVENT,
                                  _PENDING, m_WakeupParams.sciIntr0))
                {
                    rc = RC::HW_STATUS_ERROR;
                    bShowResults |= m_PrintMiscompares;
                    m_Stats[eventIdx].miscompares++;
                }
                break;

            case GC_CMN_WAKEUP_EVENT_gc5i2c:
                if (!FLD_TEST_DRF(_PGC6, _SCI_SW_INTR0, _STATUS_LWRR_I2CS,
                                  _PENDING, m_WakeupParams.sciIntr0))
                {
                    rc = RC::HW_STATUS_ERROR;
                    bShowResults |= m_PrintMiscompares;
                    m_Stats[eventIdx].miscompares++;
                }
                break;

            case GC_CMN_WAKEUP_EVENT_gc5timer:
            case GC_CMN_WAKEUP_EVENT_gc5alarm:
                if (!FLD_TEST_DRF(_PGC6, _SCI_SW_INTR0, _STATUS_LWRR_WAKE_TIMER,
                                  _PENDING, m_WakeupParams.sciIntr0))
                {
                    rc = RC::HW_STATUS_ERROR;
                    bShowResults |= m_PrintMiscompares;
                    m_Stats[eventIdx].miscompares++;
                }
                break;

            default:
                MASSERT(!"GpuGcx::Unknown event type.");
                break;

        }
    }
    // It's confusing to see the error results before the "loop:%d pstate:%d" marker.
    // So set some flags and process the information later.
    m_ShowWakeupResults = bShowResults;
    if (bShowResults)
    {
        m_WakeupMismatch = (rc != OK);
    }
    // For now issue a warning if the wakeup reason doesn't match the expected wakeup reason.
    // For GC5 we dont control the external environment as tightly as we do for GC6.
    // -RM does not pause the 1Hz thread
    // -MODS does not pause the backgroud loggers
    // -MODS does not pause the pmu thread
    // -MODS does not mask the interrupt polling
    // So there is a likelyhood of getting some PCI-E traffic directed to the GPU. When that happens
    // the wakeup reason will be a BSI event and we will log it as a miscompare.
    return OK;
}

//------------------------------------------------------------------------------
// Pick differet timing parameters and surface data for this loop.
RC GpuGcx::PickLoopParams()
{
    RC rc;

    //GC6K and GC6M with the ACPI driver installed only supports gpu events.
    //GC6M with SmbEC supports all wakeup events.
    //Default to GC6K or GC6M with ACPI driver.
    m_WakeupEvent = (*m_pPickers)[GC_CMN_WAKEUP_EVENT].Pick();
    // only a single bit can be set.
    MASSERT((m_WakeupEvent & (m_WakeupEvent-1)) == 0);

    bool fakeThermEnable = false, fakeThermSupported = false;

    // GC6K or ACPI case only supports Gpu events for GC6 or cpu event for RTD3
    if ((FLD_TEST_DRF(_EC_INFO, _FW_CFG, _FSM_MODE, _GC6K, m_EcFsmMode) ||
        FLD_TEST_DRF(_EC_INFO, _FW_CFG, _FSM_MODE, _BOOTLOADER, m_EcFsmMode)) &&
        (m_WakeupEvent & GC_CMN_WAKEUP_EVENT_gc6all))
    {
        if (m_WakeupEvent & GC_CMN_WAKEUP_EVENT_rtd3all)
        {
            m_WakeupEvent = GC_CMN_WAKEUP_EVENT_gc6rtd3_cpu;
        }
        else
        {
            m_WakeupEvent = GC_CMN_WAKEUP_EVENT_gc6gpu;
        }
    }
    else if ((m_WakeupEvent == GC_CMN_WAKEUP_EVENT_gc6hotplug) &&
        FLD_TEST_DRF(_EC_INFO, _CAP_1, _PWRUP_T1_SUPPORT, _FALSE, m_Caps >> 8))
    {
        m_WakeupEvent = GC_CMN_WAKEUP_EVENT_gc6gpu; // fallback to safe value
    }
    else if ((m_WakeupEvent == GC_CMN_WAKEUP_EVENT_gc6hotplugirq) &&
        (FLD_TEST_DRF(_EC_INFO, _CAP_1, _PWRUP_T2_SUPPORT, _FALSE, m_Caps >> 8) ||
         FLD_TEST_DRF(_EC_INFO, _CAP_1, _HPD_MODIFY_SUPPORT, _FALSE, m_Caps >> 8)))
    {
        m_WakeupEvent = GC_CMN_WAKEUP_EVENT_gc6gpu; // fallback to safe value
    }
    else if ((m_WakeupEvent == GC_CMN_WAKEUP_EVENT_gc6hotunplug) &&
            FLD_TEST_DRF(_EC_INFO, _CAP_1, _HPD_MODIFY_SUPPORT,
                         _FALSE, m_Caps >> 8))
    {
        m_WakeupEvent = GC_CMN_WAKEUP_EVENT_gc6gpu; // fallback to safe value
    }
    else if (m_WakeupEvent == GC_CMN_WAKEUP_EVENT_gc6i2c)
    {
        // Explicitly disable the fake thermal sensor for the i2c event
        rc = m_pGCx->IsFakeThermSensorEnabled(&fakeThermEnable, &fakeThermSupported);
        if (rc == OK && fakeThermSupported && fakeThermEnable)
            CHECK_RC(m_pGCx->EnableFakeThermSensor(false));
    }
    else if ((m_WakeupEvent == GC_CMN_WAKEUP_EVENT_gc6i2c_bypass) &&
             ((OK != m_pGCx->IsFakeThermSensorEnabled(&fakeThermEnable,
                                                      &fakeThermSupported)) ||
              !fakeThermSupported))
    {
        m_WakeupEvent = GC_CMN_WAKEUP_EVENT_gc6gpu; // fallback to safe value
    }
    else if (m_WakeupEvent == GC_CMN_WAKEUP_EVENT_gc5i2c &&
             !m_pGCx->IsThermSensorFound())
    {
        // fallback to most common case if we could find the therm sensor.
        m_WakeupEvent = GC_CMN_WAKEUP_EVENT_gc5pex;
    }
    else if ((m_WakeupEvent & GC_CMN_WAKEUP_EVENT_gc5all) &&
             (m_GC5Mode == GCxImpl::gc5DI_OS) &&
             ((m_WakeupEvent != GC_CMN_WAKEUP_EVENT_gc5i2c) &&
              (m_WakeupEvent != GC_CMN_WAKEUP_EVENT_gc5pex)))
    {
        // We have a GC5 event that isn't valid for DI_OS.
        // fallback to safe wakeup event
        m_WakeupEvent = GC_CMN_WAKEUP_EVENT_gc5pex;
    }

    // Pick some timing values for power up/down

    UINT32 max_delay    = (UINT32)(*m_pPickers)[GCX_PWR_DELAY_MAX].Pick();
    m_PwrDnPostDlyUsec = min((*m_pPickers)[GCX_PWR_DN_POST_DELAY].Pick(), max_delay);
    m_PwrUpPostDlyUsec = min((*m_pPickers)[GCX_PWR_UP_POST_DELAY].Pick(), max_delay);

    // We are using the SCI's internal timer to automatically wake up the GPU.
    // this timer has 24bits of range, and starts running at the start of GC5/GC6
    // entry. Care should be taken when using very small values because you
    // could power down the GPU and have it power back up while you are polling
    // the EC for GPU status.
    // Also this internal timer has a maximum value of 16.67s which is much
    // smaller than our general postDlyUsec range so guard against that.
    if ((m_WakeupEvent == GC_CMN_WAKEUP_EVENT_gc6timer) ||
        (m_WakeupEvent == GC_CMN_WAKEUP_EVENT_gc6rtd3_gputimer) ||
        (m_WakeupEvent == GC_CMN_WAKEUP_EVENT_gc5timer))
    {
        max_delay = min(max_delay, (UINT32)0xffffff);
        m_PwrDnPostDlyUsec =
            min((*m_pPickers)[GCX_WAKEUP_EVENT_TIMER_VAL].Pick(), max_delay);
    }
    // We are using the PTIMER alarm to wakeup the GPU. With this alarm the
    // timer value must be restricted to  1us < alarm < 100ms. In addition we
    // need to allow enough time for the alarm timer to be programmed and the
    // GPU to go into GC5 mode. So we will use the following restriction:
    // 100us < alarm < 100ms
    else if (m_WakeupEvent == GC_CMN_WAKEUP_EVENT_gc5alarm)
    {
        max_delay = min(max_delay, (UINT32)100000);
        m_PwrDnPostDlyUsec =
            min((*m_pPickers)[GCX_WAKEUP_EVENT_ALARM_TIMER_VAL].Pick(), max_delay);
    }

    if (m_WakeupEvent & GC_CMN_WAKEUP_EVENT_gc5all)
    {
        m_Gc5Pstate = (*m_pPickers)[GCX_GC5_PSTATE].Pick();
        if (m_ForcePstate != Perf::ILWALID_PSTATE)
        {
            m_Gc5Pstate = m_ForcePstate;
        }
        // The GC5 test did not switch pstates unless you use -ForcePstate on the cmd line
        else if (!m_pGCx->IsValidGc5Pstate(m_Gc5Pstate))
        {
            m_Gc5Pstate = m_PerfPt.PStateNum;
        }
    }
    return rc;
}

//------------------------------------------------------------------------------
// Initialize the contents of the Framebuffer with unique data.
RC GpuGcx::InitializeFb()
{
    RC rc;
    // when being run as a callback don't mess with the FB
    if (!m_VerifyFB)
        return rc;

    const UINT32 SubdevInst = m_pSubdev->GetSubdeviceInst();
    // TODO:
    // Pick a different pattern for each loop and copy it to the framebuffer
    CHECK_RC(Memory::FillAddress(m_Surf[surfGld].GetAddress(),
                                 m_Surf[surfGld].GetWidth(),
                                 m_Surf[surfGld].GetHeight(),
                                 m_Surf[surfGld].GetBitsPerPixel(),
                                 m_Surf[surfGld].GetPitch()));

    m_DmaWrap.SetSurfaces(&m_Surf[surfGld], &m_Surf[surfFb]);
    CHECK_RC(m_DmaWrap.Copy(
        0,                        // Starting src X, in bytes not pixels.
        0,                        // Starting src Y, in lines.
        m_Surf[surfGld].GetPitch(),  // Width of copied rect, in bytes.
        m_Surf[surfGld].GetHeight(), // Height of copied rect, in bytes
        0,                        // Starting dst X, in bytes not pixels.
        0,                        // Starting dst Y, in lines.
        m_TimeoutMs,
        SubdevInst));

    return OK;
}

//------------------------------------------------------------------------------
// Verify the Framebuffer contents have not changed since the last GpuReset.
RC GpuGcx::VerifyFb()
{
    RC rc;

    // When being used as a callback don't mess with the FB
    if (!m_VerifyFB)
        return rc;

    // Copy the memory from the FB to SYS and then compare against the golden
    // surface.

    const UINT32 SubdevInst = m_pSubdev->GetSubdeviceInst();
    m_DmaWrap.SetSurfaces(&m_Surf[surfFb], &m_Surf[surfSys]);
    CHECK_RC(m_DmaWrap.Copy(
        0,                        // Starting src X, in bytes not pixels.
        0,                        // Starting src Y, in lines.
        m_Surf[surfSys].GetPitch(),  // Width of copied rect, in bytes.
        m_Surf[surfSys].GetHeight(), // Height of copied rect, in bytes
        0,                        // Starting dst X, in bytes not pixels.
        0,                        // Starting dst Y, in lines.
        m_TimeoutMs,
        SubdevInst));

    CHECK_RC(VerifySurface(&m_Surf[surfGld], &m_Surf[surfSys], &m_Surf[surfFb]));

    return OK;
}
//------------------------------------------------------------------------------
// Verify the data in both Surface2D objects are the same
RC GpuGcx::VerifySurface(Surface2D *pSrc, Surface2D *pDst, Surface2D *pFbSurf)
{
    StickyRC rc;
    MASSERT( pSrc->GetBytesPerPixel() == sizeof(UINT32));

    FrameBuffer* pFb = GetBoundGpuSubdevice()->GetFB();
    const UINT32 numFbios = pFb->GetFbioCount();
    const UINT32 bitsPerPart = 64;

    vector<UINT32> errCount;

    // No need to allocate space if there's nothing to print
    if (m_PrintAddrDecode)
    {
        errCount.resize(numFbios * bitsPerPart, 0);
    }

    for (UINT32 row = 0; row < pSrc->GetHeight(); row++)
    {
        // read in one line at a time, this works because the test only supports
        // pitch linear surfaces
        UINT32 Pixels = pSrc->GetWidth();
        vector<UINT32> SrcPix(Pixels);
        vector<UINT32> DstPix(Pixels);
        UINT64 srcStartOffset = pSrc->GetPixelOffset(0, row);
        UINT64 dstStartOffset = pDst->GetPixelOffset(0, row);
        Platform::VirtualRd((UINT08*)pDst->GetAddress() + dstStartOffset,
                            &DstPix[0],
                            Pixels*sizeof(UINT32));
        Platform::VirtualRd((UINT08*)pSrc->GetAddress() + srcStartOffset,
                            &SrcPix[0],
                            Pixels*sizeof(UINT32));

        for (UINT32 j = 0; j < Pixels; j++)
        {
            if (SrcPix[j] != DstPix[j])
            {
                Printf(Tee::PriNormal,
                       "Fb miscompared at X:%d Y:%d expected=0x%x got=0x%x",
                       j, row, SrcPix[j], DstPix[j]);

                if (m_PrintAddrDecode)
                {
                    FbDecode decode;
                    UINT64 offset = pFbSurf->GetPixelOffset(j, row) +
                        pFbSurf->GetCtxDmaOffsetGpu();

                    rc = pFb->DecodeAddress
                    (
                        &decode,
                        offset,
                        pFbSurf->GetPteKind(),
                        (UINT32) (Platform::GetPageSize() / 1024) // size in KB
                    );
                    Printf(Tee::PriNormal, " %s",
                           pFb->GetDecodeString(decode).c_str());

                    UINT32 diff = SrcPix[j] ^ DstPix[j];
                    for (UINT32 i = 0; i < 32; i++)
                    {
                        if (((1 << i) & diff) != 0)
                        {
                            // +32 as subPart 1 is a partition's upper 32 bits
                            errCount[decode.virtualFbio * bitsPerPart + i +
                                   ((decode.subpartition > 0) ? 32 : 0)]++;
                        }
                    }
                }
                Printf(Tee::PriNormal, "\n");

                rc = RC::MEM_TO_MEM_RESULT_NOT_MATCH;
            }
        }
    }

    if (m_PrintAddrDecode && rc == RC::MEM_TO_MEM_RESULT_NOT_MATCH)
    {
        Printf(Tee::PriNormal, "\nFailure Summary:\n");
        Printf(Tee::PriNormal, "Failing Bit   Num Miscompares\n");
        Printf(Tee::PriNormal, "-----------   ---------------\n");
        for (UINT32 virtualFbio = 0; virtualFbio < numFbios; virtualFbio++)
        {
            for (UINT32 bit = 0; bit < bitsPerPart; bit++)
            {
                if (errCount[virtualFbio * bitsPerPart + bit] > 0)
                {
                    Printf(Tee::PriNormal, "%c%-10u   %15u\n",
                            pFb->VirtualFbioToLetter(virtualFbio),
                            bit, errCount[virtualFbio * bitsPerPart + bit]);
                }
            }
        }
        Printf(Tee::PriNormal, "\n");
    }

    return rc;
}

//--------------------------------------------------------------------------------------------------
// Return the proper eventId for the index into the m_Stats[]
UINT32 GpuGcx::GetEventId(UINT32 index)
{
    UINT32 eventId = 0;

    if (index < GC_CMN_WAKEUP_EVENT_NUM_EVENTS_gc6)
        eventId = 1<<index;
    else if (index < NUMELEMS(m_Stats))
        eventId = 1 << (16 - (GC_CMN_WAKEUP_EVENT_NUM_EVENTS_gc6-index));
    else
        MASSERT(!"Bad index value into m_Stats!");

    return eventId;
}

//------------------------------------------------------------------------------
// Return the proper index value into m_Stats[] for the given wakeupEvent
UINT32 GpuGcx::GetEventIndex(UINT32 wakeupEventId)
{
    UINT32 idx = 0;
    // GC6 events correlate to indices 0..GC_CMN_WAKEUP_EVENT_NUM_EVENTS_gc6-1
    // GC5 events correlate to indices GC_CMN_WAKEUP_EVENT_NUM_EVENTS_gc6.. GC_CMN_WAKEUP_EVENT_NUM_EVENTS-1
    if (wakeupEventId & GC_CMN_WAKEUP_EVENT_gc6all)
    {
        idx = (UINT32)Utility::BitScanForward(wakeupEventId & GC_CMN_WAKEUP_EVENT_gc6all);
    }
    else if (wakeupEventId & GC_CMN_WAKEUP_EVENT_gc5all)
    {
        idx = (UINT32)Utility::BitScanForward((wakeupEventId & GC_CMN_WAKEUP_EVENT_gc5all) >> 16);
        idx += GC_CMN_WAKEUP_EVENT_NUM_EVENTS_gc6;
    }
    else
    {
        MASSERT(!"Invalid wakeupEventId!");
    }
    return idx;
}
//------------------------------------------------------------------------------
RC GpuGcx::CheckStats()
{
    StickyRC rc;
    float totalAborts = 0.0;
    float gc5Loops = 0.0;

    // m_Stats[0..GC_CMN_WAKEUP_EVENT_NUM_EVENTS_gc6-1] = GC6
    // m_Stats[GC_CMN_WAKEUP_EVENT_NUM_EVENTS_gc6..GC_CMN_WAKEUP_EVENT_NUM_EVENTS-1] = GC5
    for (unsigned i = GC_CMN_WAKEUP_EVENT_NUM_EVENTS_gc6;
         i < NUMELEMS(m_Stats); i++)
    {
        totalAborts += (float)m_Stats[i].aborts;
        gc5Loops += (m_Stats[i].aborts + m_Stats[i].exits);
    }
    if (totalAborts/gc5Loops*100.0 > m_MaxAbortsPercent)
        rc = RC::HW_STATUS_ERROR;

    // callwlate average and std of each engine's load time
    if (m_EnableLoadTimeCalc)
    {
        for (auto it : m_EngineLoadTime)
        {
            UINT64 avgLoadTimeMs = 0;

            for (size_t i = 0; i < it.second.size(); i++)
            {
                avgLoadTimeMs += it.second[i] / 1000;
            }

            m_EngineLoadTimeAvg[it.first] = avgLoadTimeMs / it.second.size();

            Printf(Tee::PriNormal, "Engine %s load time mean = %llu (%lu data points).\n", 
                it.first.c_str(), m_EngineLoadTimeAvg[it.first], it.second.size());
        }

        for (auto it : m_EngineLoadTime)
        {
            UINT64 stdLoadTimeMs = 0;
            UINT64 avgLoadTimeMs = m_EngineLoadTimeAvg[it.first];

            for (size_t i = 0; i < it.second.size(); i++)
            {
                UINT64 loadTimeMs = it.second[i] / 1000;
                UINT64 squaredDiff = (loadTimeMs >= avgLoadTimeMs ? loadTimeMs - avgLoadTimeMs : avgLoadTimeMs - loadTimeMs);
                squaredDiff *= squaredDiff;
                stdLoadTimeMs += squaredDiff;
            }

            m_EngineLoadTimeStd[it.first] = static_cast<UINT64>(sqrt(stdLoadTimeMs / it.second.size()));

            Printf(Tee::PriNormal, "Engine %s load time std = %llu (%lu data points).\n", 
                it.first.c_str(), m_EngineLoadTimeStd[it.first], it.second.size());
        }
    }
    CHECK_RC(GetEngineLoadTimeBaseline());
    CHECK_RC(CompareEngineLoadTimeBaseline());

    if ((rc != OK) || m_PrintStats || m_PrintSummary)
    {
        Printf(Tee::PriNormal, "\n");
        UINT64 actEntryTime = 0L;
        UINT64 expEntryTime = 0L;
        for (unsigned i = 0; i < NUMELEMS(m_Stats); i++)
        {
            if (m_Stats[i].aborts + m_Stats[i].exits + m_Stats[i].miscompares > 0)
            {
                if (i >= GC_CMN_WAKEUP_EVENT_NUM_EVENTS_gc6)
                {
                    actEntryTime += m_Stats[i].actualEntryTimeUs;
                    expEntryTime += m_Stats[i].expectedEntryTimeUs;
                    float residency = 0.0f;
                    if (m_Stats[i].expectedEntryTimeUs > 0)
                    {
                        residency = static_cast<float>(m_Stats[i].actualEntryTimeUs) /
                                    static_cast<float>(m_Stats[i].expectedEntryTimeUs);
                    }
                    Printf(Tee::PriNormal,
                        "WakeupEvent:%12s cycles:%03d DI_%s(entries:%03d exits:%03d attempts:%03d miscompares:%03d aborts:%03d time:%ldus) Residency:%3.2f%%\n",
                        m_pGCx->WakeupEventToString(GetEventId(i)).c_str(),
                        m_Stats[i].exits,
                        m_GC5Mode == GCxImpl::gc5DI_OS ? "OS" : "SSC",
                        m_Stats[i].diEntries,
                        m_Stats[i].diExits,
                        m_Stats[i].diAttempts,
                        m_Stats[i].miscompares,
                        m_Stats[i].aborts,
                        m_Stats[i].actualEntryTimeUs,
                        residency*100.0);
                }
                else
                {
                    Printf(Tee::PriNormal,
                           "WakeupEvent:%12s exits:%03d miscompares:%03d aborts:%03d  minUs:%08d maxUs:%08d\n",
                        m_pGCx->WakeupEventToString(GetEventId(i)).c_str(),
                        m_Stats[i].exits,
                        m_Stats[i].miscompares,
                        m_Stats[i].aborts,
                        m_Stats[i].minUs,
                        m_Stats[i].maxUs);

                    if (GetEventId(i) == GC_CMN_WAKEUP_EVENT_gc6i2c_bypass &&
                            m_Stats[i].miscompares > 0)
                    {
                        Printf(Tee::PriNormal, "WakupEvent gc6i2c bypass failed\n");
                        rc = RC::SOFTWARE_ERROR;
                    }
                }
            }
        }

        if (gc5Loops > 0)
        {
            Printf(Tee::PriNormal, "GC5 Entry ratio: %3.2f%%\n",
                (gc5Loops - totalAborts)/gc5Loops*100.0);

            if (expEntryTime != 0)
            {
                Printf(Tee::PriNormal, "GC5 Residency: %3.2f%%\n",
                       (100.0 * actEntryTime) / expEntryTime);
            }
            else
            {
                Printf(Tee::PriNormal, "GC5 Residency: n/a\n");
            }
        }
    }

    const bool bReadFuse = false, bPrintBadRecords = true;
    CHECK_RC(CompareFuseRecords(bReadFuse, bPrintBadRecords));
    PrintFuseStats(Tee::PriNormal);
    return rc;
}

//------------------------------------------------------------------------------
// Print the current stats.
void GpuGcx::Print(Tee::Priority pri)
{
    UINT32 pstate = (m_WakeupEvent & GC_CMN_WAKEUP_EVENT_gc6all) ? m_Gc6Pstate : m_Gc5Pstate;
    if (m_ShowWakeupResults || m_PrintStats)
    {
        Printf(pri, "Loop:%d pstate:%d\n", m_pFpCtx->LoopNum, pstate);
    }

    if (m_ShowWakeupResults)
    {
        if (m_WakeupMismatch)
        {
            Printf(Tee::PriNormal, "Wakeup reason:%s doesn't match wakeup event:%s.\n",
                (m_WakeupEvent & GC_CMN_WAKEUP_EVENT_gc6all) ?
                    m_pGCx->WakeupReasonToString(m_WakeupParams, GCxImpl::decodeGc6).c_str():
                    m_pGCx->WakeupReasonToString(m_WakeupParams, GCxImpl::decodeGc5).c_str(),
                m_pGCx->WakeupEventToString(m_WakeupEvent).c_str());
        }
        Printf(Tee::PriNormal, "Wakeup reason params:\n"
                "\tselectPowerState:%d statId:%d\n" // deepL1Type:%d\n"
                "\tgc5ExitType:%d gc5AbortCode:%d\n"
                "\tsciIntrStatus0:0x%x sciIntrStatus1:0x%x\n"
                "\tpmcIntr0:0x%x pmcIntr1:0x%x\n",
                m_WakeupParams.selectPowerState, m_WakeupParams.statId,
                //m_WakeupParams.deepL1Type,
                m_WakeupParams.gc5ExitType, m_WakeupParams.gc5AbortCode,
                m_WakeupParams.sciIntr0, m_WakeupParams.sciIntr1,
                m_WakeupParams.pmcIntr0, m_WakeupParams.pmcIntr1);
    }
    if (m_PrintStats && m_UseJTMgr && (m_WakeupEvent & GC_CMN_WAKEUP_EVENT_gc6all))
    {
        Xp::JTMgr::CycleStats stats;
        m_pGCx->GetGc6CycleStats(&stats);
        Printf(pri, "GC6 cycle stats\n");
        if (m_pGCx->IsGc6SMBusSupported())
        {
            Printf(pri, "SmbEC EntryMs:%4lld  LinkDnMs:%4lld PostDlyUsec:%9d EntryStatus:%d ECPwrStatus:0x%x RmEntryMs:%4lld \n",
                stats.entryTimeMs, stats.enterLinkDnMs,  m_PwrDnPostDlyUsec,
                stats.entryStatus, stats.entryPwrStatus, m_EnterTimeMs);

            Printf(pri, "SmbEC ExitMs :%4lld  LinkUpMs:%4lld PostDlyUsec:%9d ExitStatus :%d ECPwrStatus:0x%x RmExitMs :%4lld wakeupEvent:%d(%s)\n",
                stats.exitTimeMs, stats.exitLinkUpMs, m_PwrUpPostDlyUsec,
                stats.exitStatus, stats.exitPwrStatus, m_ExitTimeMs,
                stats.exitWakeupEvent, m_pGCx->WakeupEventToString(m_WakeupEvent).c_str());
        }
        else
        {
            const char* acpiMethod = m_pGCx->IsGc6JTACPISupported() ? "JT ACPI" : "Native ACPI";
            Printf(pri, "%s EntryMs:%4lld PostDlyUsec:%9d RmEntryMs:%4lld \n",
                acpiMethod, stats.entryTimeMs, m_PwrDnPostDlyUsec, m_EnterTimeMs);
            Printf(pri, "%s ExitMs :%4lld PostDlyUsec:%9d RmExitMs :%4lld wakeupEvent:%d(%s)\n",
                acpiMethod, stats.exitTimeMs, m_PwrUpPostDlyUsec, m_ExitTimeMs,
                m_WakeupEvent, m_pGCx->WakeupEventToString(m_WakeupEvent).c_str());
        }
    }
    if (m_PrintStats && (m_WakeupEvent & GC_CMN_WAKEUP_EVENT_gc5all))
    {
        Printf(pri, "GC5 cycle stats\n");
        Printf(pri, "EntryMs:%.4lld PostDlyUsec:%.9d WakeupEvent :0x%x(%s)\n"
            ,m_EnterTimeMs
            ,m_PwrDnPostDlyUsec
            ,m_WakeupEvent
            ,m_pGCx->WakeupEventToString(m_WakeupEvent).c_str());
        float residency = 0.0f;
        if (m_PwrDnPostDlyUsec > 0)
        {
            residency = m_ExitDeepIdleStats.time * 100.0f / m_PwrDnPostDlyUsec;
        }
        if (m_GC5Mode == GCxImpl::gc5DI_SSC)
        {
            Printf(pri, "ExitMs :%.4lld PostDlyUsec:%.9d WakeupReason:%d(%s) AbortCode:0x%x DI-SSC(entries:%d exits:%d attempts:%d time:%dus)Residency:%3.2f%%\n"
                ,m_ExitTimeMs
                ,m_PwrUpPostDlyUsec
                ,m_WakeupParams.gc5ExitType
                ,m_pGCx->Gc5ExitTypeToString(m_WakeupParams.gc5ExitType).c_str()
                ,m_WakeupParams.gc5AbortCode
                ,m_ExitDeepIdleStats.entries
                ,m_ExitDeepIdleStats.exits
                ,m_ExitDeepIdleStats.attempts
                ,m_ExitDeepIdleStats.time
                ,residency
                );
        }
        else if (m_GC5Mode == GCxImpl::gc5DI_OS)
        {
            Printf(pri, "ExitMs :%.4lld PostDlyUsec:%.9d DI-OS(entries:%d exits:%d attempts:%d time:%d) Residency:%3.2f%%\n"
                ,m_ExitTimeMs
                ,m_PwrUpPostDlyUsec
                ,m_ExitDeepIdleStats.entries
                ,m_ExitDeepIdleStats.exits
                ,m_ExitDeepIdleStats.attempts
                ,m_ExitDeepIdleStats.time
                ,residency);
        }
    }

    if (m_PrintStats)
    {
        CollectAndPrintDebugInfo(pri);
    }
}

//--------------------------------------------------------------------------------------------------
// Try to collect additional information to help debug why we are not getting into GC5/GC6.
//
RC GpuGcx::CollectAndPrintDebugInfo(Tee::Priority pri)
{
    RC rc;

    // Read the deep L1 entry counts
    auto pPcie = m_pSubdev->GetInterface<Pcie>();
    if (!pPcie)
    {
        Printf(Tee::PriError, "No PCIE sw module detected!\n");
        return RC::SOFTWARE_ERROR;
    }

    Pcie::AspmEntryCounters counts;
    CHECK_RC(pPcie->GetAspmEntryCounts(&counts));
    Printf(pri, "AspmEntryCounts:(XmitL0SEntry:%.5d UpstreamL0SEntry:%.5d "
        "L1Entry:%.5d L1PEntry:%.5d DeepL1Entry:%.5d)\n",
        counts.XmitL0SEntry, counts.UpstreamL0SEntry,
        counts.L1Entry, counts.L1PEntry, counts.DeepL1Entry);

    CHECK_RC(pPcie->ResetAspmEntryCounters());

    if (m_WakeupEvent & GC_CMN_WAKEUP_EVENT_rtd3all)
    {
        // Below controls are not supported for RTD3 GC6
        return rc;
    }

    const LW2080_CTRL_POWERGATING_PARAMETER  pgCount =
    {
        LW2080_CTRL_MC_POWERGATING_PARAMETER_INGATINGCOUNT,
        PMU::ELPG_MS_ENGINE,
        0
    };
    vector<LW2080_CTRL_POWERGATING_PARAMETER> getIngatingCount(1, pgCount );
    PMU *pPmu = NULL;
    CHECK_RC(m_pSubdev->GetPmu(&pPmu));
    if (!pPmu)
    {
        Printf(Tee::PriError, "No PMU detected!\n");
        return RC::SOFTWARE_ERROR;
    }
    CHECK_RC(pPmu->GetPowerGatingParameters(&getIngatingCount));

    Printf(pri, "MSCG ingating count: %.5d\n", getIngatingCount[0].parameterValue);

    return rc;
}

//--------------------------------------------------------------------------------------------------
// Compare the initial boot strap values against the current.
RC GpuGcx::CompareBootStraps()
{
    StickyRC rc;
    vector<pair<UINT32, UINT32> > straps;
    CHECK_RC(m_pSubdev->GetStraps(&straps));

    if (straps != m_BootStraps)
    {
        Printf(Tee::PriNormal, "GPU straps miscompare!\n");
        rc = RC::BOOT_STRAP_ERROR;
        if (Utility::GetSelwrityUnlockLevel() >= Utility::SUL_LWIDIA_NETWORK)
        {
            Printf(Tee::PriNormal, "Straps Info:\n");
            for (size_t ii = 0; ii < straps.size(); ii++)
            {
                Printf(Tee::PriNormal, "Register:0x%x expected:0x%x actual:0x%x\n",
                       straps[ii].first, m_BootStraps[ii].second, straps[ii].second);
            }
        }
    }

    return rc;
}


//--------------------------------------------------------------------------------------------------
RC GpuGcx::GetFuseRecords()
{
    StickyRC rc;
    if (m_SkipFuseCheck)
    {
        return OK;
    }
    Fuse *pFuse = m_pSubdev->GetFuse();
    MASSERT(pFuse);

    string XmlFilename = pFuse->GetFuseFilename();
    const FuseUtil::FuseDefMap *pFuseDefMap;
    const FuseUtil::SkuList *pSkuList;
    const FuseUtil::MiscInfo *pMiscInfo;
    unique_ptr<FuseParser> pParser(FuseParser::CreateFuseParser(XmlFilename));
    rc = pParser->ParseFile(XmlFilename, &pFuseDefMap, &pSkuList, &pMiscInfo);
    
    if (rc == RC::FILE_DOES_NOT_EXIST)
    {
        Printf(Tee::PriError, "Fuse record file doesn't exist, add "
            "'-testarg 347 SkipFuseCheck true' to skip fuse verification\n");
    }

    CHECK_RC(rc);

    map<string, FuseUtil::FuseDef>::const_iterator iter = pFuseDefMap->begin();
    for (; iter != pFuseDefMap->end(); iter++)
    {
        if (iter->second.PrimaryOffset.Offset != FuseUtil::UNKNOWN_OFFSET)
        {
            fuseInfo NewFuse(iter->second.PrimaryOffset.Offset,
                        m_pSubdev->RegRd32(iter->second.PrimaryOffset.Offset),
                        m_pSubdev->RegRd32(iter->second.PrimaryOffset.Offset),
                        ~0, 0, 0);
            m_FuseInfo[iter->first] = NewFuse;
        }
    }

    return rc;
}

//--------------------------------------------------------------------------------------------------
// Compare all of the fuse records and print an error message for each failing fuse.
// Don't return failure if a miscompare oclwrs, we don't want to stop the testing
// If bReadFuse is true then read the fuses, compare against m_FuseInfo, and update the failure info.
// If bPrintBadRecords is true print a failure message for each bad fuse.
RC GpuGcx::CompareFuseRecords(bool bReadFuse, bool bPrintBadRecords)
{
    StickyRC rc;
    if (m_SkipFuseCheck)
    {
        return OK;
    }
    map<string, fuseInfo>::iterator iter;
    if (bReadFuse)
    {
        for (iter = m_FuseInfo.begin(); iter != m_FuseInfo.end(); iter++)
        {
            UINT32 val = m_pSubdev->RegRd32(iter->second.offset);
            if (val != iter->second.exp)
            {   // miscompare, log 1st failure info.
                if (iter->second.failLoop == (UINT32)~0)
                {
                    iter->second.failLoop = m_pFpCtx->LoopNum;
                    iter->second.act = val;
                }
                iter->second.failCnt++;
                rc = RC::FUSE_VALUE_OUT_OF_RANGE;
            }
            else
            {
                iter->second.passCnt++;
            }
        }
    }

    if (bPrintBadRecords)
    {
        for (iter = m_FuseInfo.begin(); iter != m_FuseInfo.end(); iter++)
        {
            if (iter->second.failCnt)
            {
                if (Utility::GetSelwrityUnlockLevel() >= Utility::SUL_LWIDIA_NETWORK)
                {
                    Printf(Tee::PriNormal, "Fuse:%s miscompared %x times, compared %d times. ",
                        iter->first.c_str(), iter->second.failCnt, iter->second.passCnt);
                    Printf(Tee::PriNormal, "First miscompare on loop:%d expected:0x%.8x actual:0x%.8x\n",
                        iter->second.failLoop, iter->second.exp, iter->second.act);
                }
                rc = RC::FUSE_VALUE_OUT_OF_RANGE;
            }
        }
    }
    return rc;
}

//--------------------------------------------------------------------------------------------------
// Print out the fuse statistics.
void GpuGcx::PrintFuseStats(Tee::Priority pri)
{
    if (m_SkipFuseCheck ||
        (Utility::GetSelwrityUnlockLevel() < Utility::SUL_LWIDIA_NETWORK) ||
        !m_PrintFuseStats)
    {
        return;
    }
    map<string, fuseInfo>::const_iterator iter = m_FuseInfo.begin();
    for (; iter != m_FuseInfo.end(); iter++)
    {
        Printf(pri, "Fuse:%-40s | offset:0x:%.8x | expected:0x%.8x | actual:0x%.8x | passCnt:%.4d | failCnt:%.4d | failLoop:%.4d |\n",
            iter->first.c_str(),
            iter->second.offset,
            iter->second.exp,
            iter->second.act,
            iter->second.passCnt,
            iter->second.failCnt,
            iter->second.failLoop);
    }
}

RC GpuGcx::GetEngineLoadTimeBaseline()
{
    StickyRC rc;

    if (!m_EnableLoadTimeCalc || !m_EnableLoadTimeComparison)
    {
        return OK;
    }
    
    rapidjson::Document document;
    string filename;
#ifdef LINUX_MFG
    filename.resize(100);
    gethostname(&filename[0], (int)filename.size());
    filename.resize(strlen(&filename[0]));
    filename += "_rmperf_t147.js";
#else
    filename = "windows_rmperf_t147.js";
#endif
    rc = JsonParser::ParseJson(filename, &document);

    if (rc == RC::FILE_DOES_NOT_EXIST)
    {
        Printf(Tee::PriError, "Per engine load time baseline file \"%s\" doesn't exist, add "
            "'-testarg 347 EnableLoadTimeCalc false' to skip per engine load time baseline comparison\n", filename.c_str());
    }


    CHECK_RC(rc);

    for (auto iter = document.MemberBegin(); iter != document.MemberEnd(); iter++)
    {
        m_EngineLoadTimeBaselineAvg[iter->name.GetString()] = iter->value["mean"].GetDouble();
        m_EngineLoadTimeBaselineStd[iter->name.GetString()] = iter->value["std"].GetDouble();
    }

    return rc;
}

RC GpuGcx::CompareEngineLoadTimeBaseline()
{
    StickyRC rc;

    if (!m_EnableLoadTimeCalc || !m_EnableLoadTimeComparison)
    {
        return OK;
    }

    // check for removed engine
    for (auto iter : m_EngineLoadTimeBaselineAvg)
    {
        if (m_EngineLoadTimeAvg.count(iter.first) == 0)
        {
            Printf(Tee::PriError, "Engine %s exist in baseline but removed in this test. Please update the baseline.\n", iter.first.c_str());
            rc = RC::UNEXPECTED_RESULT;
        }
    }

    // check for new engine
    for (auto iter : m_EngineLoadTimeAvg)
    {
        if (m_EngineLoadTimeBaselineAvg.count(iter.first) == 0)
        {
            Printf(Tee::PriError, "Engine %s exist in this test but removed in baseline. Please update the baseline.\n", iter.first.c_str());
            rc = RC::UNEXPECTED_RESULT;
        }
    }

    CHECK_RC(rc);

    for (auto iter = m_EngineLoadTimeAvg.begin(); iter != m_EngineLoadTimeAvg.end(); iter++)
    {
        if (m_EngineLoadTimeBaselineAvg.count(iter->first) == 0) continue;

        double baselineAvg = m_EngineLoadTimeBaselineAvg[iter->first];
        double baselineStd = m_EngineLoadTimeBaselineStd[iter->first];
        double highThreshold = baselineAvg + baselineStd * 3;

        double measuredAvg = static_cast<double>(iter->second);

        if (measuredAvg > highThreshold)
        {
            Printf(Tee::PriError, "Engine %s load time = %lf, exceed threshold %lf\n", iter->first.c_str(), measuredAvg, highThreshold);
            rc = RC::EXCEEDED_EXPECTED_THRESHOLD;
        }
    }

    return rc;
}
