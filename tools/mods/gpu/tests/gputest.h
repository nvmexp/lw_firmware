/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2005-2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#pragma once

#include "core/include/golden.h"
#include "core/include/statistics.h"
#include "core/tests/modstest.h"
#include "core/include/golden.h"
#include "gpu/include/gpusbdev.h"
#include "gpu/include/gpugoldensurfaces.h"
#include "gpu/perf/perfsub.h"
#include "gpu/perf/pmusub.h"
#include "gpu/tests/gputestc.h"
#include <memory>
#include "core/include/memerror.h"
#include <regex>

class TestDevice;
typedef shared_ptr<TestDevice> TestDevicePtr;
class JsTestDevice;
class JsGpuSubdevice;
class JsGpuDevice;
class TpcEccError;

extern SObject GpuTest_Object;

class GpuTest: public ModsTest
{
public:
    GpuTest();
    virtual ~GpuTest();

    virtual bool IsSupported();

    virtual RC Setup();
    virtual RC Run() = 0;
    virtual RC Cleanup();
    virtual RC EndLoop() { return EndLoop(NoLoopId); }
    virtual RC EndLoop(UINT32 loop, UINT64 mleProgress, RC rcFromRun);
    virtual RC EndLoop(UINT32 loop);
    virtual RC EndLoopMLE(UINT64 mleProgress);
    virtual bool IsTestType(TestType tt);

    RC AllocDisplayAndSurface(bool useBlockLinear);
    RC AllocDisplay();

    virtual bool RequiresSoftwareTreeValidation() const { return true; }
    //! Set up an extra objects to be objects of an GpuTest.
    virtual RC AddExtraObjects(JSContext *cx, JSObject *obj);

    virtual RC InitFromJs();
    virtual void PrintJsProperties(Tee::Priority pri);
    virtual void PropertyHelp(JSContext *pCtx, regex *pRegex);
    virtual RC SetupJsonItems();

    //! True if this test exercises all subdevices at once, false if this
    //! test should be run once per subdevice to get full coverage.
    virtual bool GetTestsAllSubdevices() { return false; }

    virtual void BindRmClient(LwRm *pLwRm);

    virtual RC BindTestDevice(TestDevicePtr pTestDevice);
    virtual RC BindTestDevice(TestDevicePtr pTestDevice, JSContext *cx, JSObject *obj);

    LwRm *GetBoundRmClient();
    GpuDevice *GetBoundGpuDevice();
    GpuSubdevice *GetBoundGpuSubdevice();

    TestDevicePtr GetBoundTestDevice() { return m_pTestDevice; }

    DisplayMgr::TestContext * GetDisplayMgrTestContext() const;
    Display * GetDisplay();
    DisplayMgr::Requirements GetDispMgrReqs();
    RC GetDisplayPitch(UINT32 *pitch);
    RC AdjustPitchForScanout(UINT32* pPitch);
    UINT32 GetPrimaryDisplay();

    UINT32 GetDeviceId();

    RC CheckSetupCompleted();

    RC HookupPerfCallbacks();

    double GetBps() const { return m_BytesReadOrWrittenPerSecond; }
    void SetDisableWatchdog(bool disable);
    bool GetDisableWatchdog();

    RC SetForcedElpgMask(UINT32 Mask);
    UINT32 GetForcedElpgMask() const;
    RC FireIsmExpCallback(string subtestName);

    SETGET_PROP(DisableRcWatchdog, bool);
    SETGET_PROP(PwrSampleIntervalMs, UINT32);
    SETGET_PROP(TargetLwSwitch, bool);
    SETGET_PROP(IssueRateOverride, string);
    SETGET_PROP(CheckPickers, bool);

    // Need this to be public for use with CleanupOnReturn.
    RC StopPowerThread();
    void GpuHung(bool doExit);
    MemError &GetMemError(UINT32 errObj);
    vector<unique_ptr<MemError>> &GetMemErrors() { return m_ErrorObjs; }
    
    static UINT64 GetErrorCheckPeriodMS() { return s_ErrorCheckPeriodMS; };
    static RC SetErrorCheckPeriodMS(UINT64 period) 
    { 
        s_ErrorCheckPeriodMS = period;
        return RC::OK;
    };
protected:
     
    vector<unique_ptr<MemError>> m_ErrorObjs; 
    bool m_DidResolveMemErrorResult = false;
    
    GpuTestConfiguration *      GetTestConfiguration();
    const GpuTestConfiguration * GetTestConfiguration() const;
    Goldelwalues *              GetGoldelwalues();
    RC                          SetupGoldens(UINT32             DisplayMask,
                                             GoldenSurfaces    *pGoldSurfs,
                                             UINT32             LoopOrDbIndex,
                                             bool               bDbIndex,
                                             bool              *pbEdidFaked);
    virtual RC                  SetGoldenName();
    TestDevicePtr               m_pTestDevice;
    Perf::PerfPoint             m_LastPerf;
    Callbacks::Handle           m_hPreSetupCallback    = 0;
    Callbacks::Handle           m_hPreRunCallback      = 0;
    Callbacks::Handle           m_hPostRunCallback     = 0;
    Callbacks::Handle           m_hPostCleanupCallback = 0;

    RC OnPreTest(const CallbackArguments &args);
    RC OnPreRun(const CallbackArguments &args);
    RC OnPostRun(const CallbackArguments &args);
    RC OnPostTest(const CallbackArguments &args);

    //@{
    //! TODOWJM: All members below should be private.  Can't do it yet because
    //! *many* tests refer to them directly
    GpuTestConfiguration m_TestConfig;
    Goldelwalues         m_Golden;
    //@}

    RC ValidateSoftwareTree();
    void RecordBps (double bytesReadOrWritten, double seconds,
                    Tee::Priority pri = Tee::PriLow);
    void RecordL2Bps
    (
        double bytesRead,
        double bytesWritten,
        UINT64 avgL2FreqHz,
        double seconds,
        Tee::Priority pri
    );

    void SetVerbosePrintPri(Tee::Priority Pri) { m_VerbosePri = Pri; }
    Tee::Priority GetVerbosePrintPri() const { return m_VerbosePri; }
    void VerbosePrintf(const char* format, ...) const;

    RC StartPowerThread();
    RC StartPowerThread(bool detachable);
    void PausePowerThread(bool Pause);
    // Checks that the power thread has actually started
    bool HasPowerThreadStarted() { return m_PwrThreadStarted; }

    // Called by PowerThread after each sample.
    // Override in tests that want to try to react to power (i.e. to hit
    // a target milliwatt value).
    virtual void PowerControlCallback(LwU32 milliWatts) {}

    RC SysmemUsesLwLink(bool* pbSysmemUsesLwLink);

private:
    UINT32 GetSubdevMask();
    RC InnerSetupGoldens
    (
        UINT32             DisplayMask,
        GoldenSurfaces    *pGoldSurfs,
        UINT32             LoopOrDbIndex,
        bool               bDbIndex
    );

    static UINT64 s_ErrorCheckPeriodMS;
    JsTestDevice*             m_JsTestDevice                = nullptr;
    JsGpuSubdevice*           m_JsGpuSubdevice              = nullptr;
    JsGpuDevice*              m_JsGpuDevice                 = nullptr;

    DisplayMgr::TestContext * m_pDisplayMgrTestContext      = nullptr;
    LwRm *                    m_pLwRm                       = nullptr;
    UINT32                    m_FakedEdids                  = 0;
    double                    m_BytesReadOrWrittenPerSecond = 0;
    bool                      m_DisableRcWatchdog           = false;
    bool                      m_CheckPickers                = false;
    UINT32                    m_ForcedElpgMask              = 0;
    UINT32                    m_OrgElpgMask                 = 0;
    bool                      m_ElpgForced                  = false;
    ElpgOwner                 m_ElpgOwner;
    GpuSubdevice::IssueRateOverrideSettings m_OrigSpeedSelect;
    string                    m_IssueRateOverride;
    Tee::Priority             m_VerbosePri                  = Tee::PriLow;

    static void PowerThread(void *pThis);

    Tasker::ThreadID          m_PwrThreadID                 = Tasker::NULL_THREAD;
    Tasker::EventID           m_StopPowerThreadEvent        = nullptr;
    UINT32                    m_PwrSampleIntervalMs         = 500;
    bool                      m_PwrThreadStarted            = false;
    bool                      m_PwrThreadDetachable         = false;

    BasicStatistics<UINT64>   m_TotalPwr;
    vector< BasicStatistics<UINT64> > m_PerChannelPwr;
    UINT64                    m_TimeOfNextSample            = 0;
    bool                      m_PausePwrThread              = false;

    bool                      m_TargetLwSwitch              = false;

    unique_ptr<LwRm::DisableRcWatchdog> m_DisableRcWd;
    vector<unique_ptr<TpcEccError>> m_TpcEccErrorObjs;
    bool                            m_TpcEccErrorsResolved  = false;

    static UINT08             s_DefaultTmdsGoldensEdid[];
    static UINT08             s_DefaultLvdsGoldensEdid[];
    static UINT08             s_DefaultDpGoldensEdid[];
    static constexpr UINT32   NoLoopId = ~0U;
    std::atomic<UINT64>       m_LastErrorCheckTimeMS;
};
