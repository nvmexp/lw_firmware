/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2014-2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

/**
 * @brief Implement algorithms for enter/exit of GC5 & GC6 power
 * saving sequences
 * This is the class interface for the GCx implementor class
 * starting on GF117 GPUs.
 */
#ifndef INCLUDED_GCXIMPL_H
#define INCLUDED_GCXIMPL_H
#include "core/include/rc.h"
#include "lwtypes.h"
#include "core/include/lwrm.h"
#include "gpu/utility/surf2d.h"
#include "core/include/xp.h"
#include "jt.h"
#include "gpu/include/pcicfg.h"
#include "gpu/perf/perfsub.h"
#include "gpu/utility/pcie/pexdev.h"
// For verifying the wakeup reason
#include "ctrl/ctrl2080/ctrl2080power.h"
#include "gpu/gc6/ec_info.h"
// For retrieving each engines load time
#include "ctrl/ctrl2080/ctrl2080gpu.h"

class GpuSubdevice;
class SmbPort;

using GC6EntryParams = LW2080_CTRL_GC6_ENTRY_PARAMS;
using GC6ExitParams  = LW2080_CTRL_GC6_EXIT_PARAMS;

class GCxImpl
{
public:
    enum gc5Mode
    {
        gc5DI_OS = 1, //!<  DeepIdle_OportunisticSleep
        gc5DI_SSC = 2 //!<  DeepIdle_SoftwareSequenceControlled
    };
    enum decodeStrategy //!< what stragegy to use when decoding wakeup results
    {
        decodeGc5 = 1,  //!<  decode gc5ExitType,gc5AbortCode,sciIntr0,sciIntr1
        decodeGc6 = 2   //!<  just decode sciInt0/sciIntr1
    };

                GCxImpl(GpuSubdevice *pSubdev);
    virtual     ~GCxImpl();

    RC          DoEnterGc5
                (
                    UINT32 pstate,      //!<  what pstate to use for this cycle
                    UINT32 wakeupEvent, //!<  the type of wakeup event that will be used to exit GC5
                    UINT32 enterDelayUsec //!< how long to wait after entering GC5 before returning to the caller
                );
    RC          DoEnterGc6
                (
                    UINT32 wakeupEvent, //!< the type of wakeup event that will be used to exit GC6
                    UINT32 enterDelayUsec //!< delay after entering GC6 before returning to the caller
                );
    RC          DoExitGc5(
                    UINT32 exitDelayUsec,   //!< delay after exiting GC6 before returning
                    bool bRestorePstate     //!< if true restore to the pstate prior to entry.
                );
    RC          DoExitGc6(
                    UINT32 exitDelayUsec,   //!< delay after exiting GC6 before returning
                    bool bRestorePstate     //!< if true restore to the pstate prior to entry.
                );
    string      Gc5ExitTypeToString(UINT32 exitType);
    UINT32      GetEnterTimeMs() {return static_cast<UINT32>(m_EnterTimeMs);}
    RC          GetDeepIdleStats(LW2080_CTRL_GPU_READ_DEEP_IDLE_STATISTICS_PARAMS *pStats);
    RC          GetDebugOptions(Xp::JTMgr::DebugOpts * pDbgOpts);
    UINT32      GetExitTimeMs() {return static_cast<UINT32>(m_ExitTimeMs);}
    LW2080_CTRL_GPU_GET_ENGINE_LOAD_TIMES_PARAMS GetEngineLoadTimeParams() { return m_EngineLoadTimeParams; }
    LW2080_CTRL_GPU_GET_ID_NAME_MAPPING_PARAMS GetEngineIdNameMappingParams() { return m_EngineIdNameMappingParams; }
    UINT32      GetGc6Caps();
    UINT32      GetGc6FsmMode();
    void        GetGc6CycleStats(Xp::JTMgr::CycleStats *pStats);
    UINT32      GetGc6Pstate();
    void        GetWakeupParams(LW2080_CTRL_GCX_GET_WAKEUP_REASON_PARAMS *pParms);
    UINT32      GetWakeupGracePeriodMs() {return m_WakeupGracePeriodMs;}
    RC          InitGc5();  //!< Initialize GC5 internals
    RC          InitGc6(FLOAT64 timeoutMs); //!< how long to wait for the config space to be ready.
    bool        IsGc5Supported(UINT32 pstate);
    bool        IsGc6Supported(UINT32 pstate);
    bool        IsRTD3Supported(UINT32 pstate, bool bNativeACPI);
    bool        IsRTD3GCOffSupported(UINT32 pstate);
    bool        IsFGc6Supported(UINT32 pstate);
    bool        IsGc6JTACPISupported() const { return (m_pJT->IsJTACPISupported()); }
    bool        IsGc6SMBusSupported() const { return (m_pJT->IsSMBusSupported()); }
    bool        IsThermSensorFound();
    void        SetBoundRmClient(LwRm * pBoundRm){m_pLwRm = pBoundRm;}
    void        SetDebugOptions(Xp::JTMgr::DebugOpts dbgOpts);
    void        SetGc5Mode(gc5Mode mode){m_Gc5Mode = mode;} //!< DI-OS or DI-SSC
    void        SetSkipSwFuseCheck(bool bCheck){m_SkipSwFuseCheck = bCheck;}
    void        SetEnableLoadTimeCalc(bool bEnable){m_EnableLoadTimeCalc = bEnable;}
    void        SetUseJTMgr(bool bUse){m_UseJTMgr = bUse;}
    void        SetUseOptimus(bool bUse){m_UseOptimus = bUse;}
    void        SetVerbosePrintPriority(Tee::Priority pri) {m_VerbosePri = pri;}
    void        SetWakeupGracePeriodMs(UINT32 period) {m_WakeupGracePeriodMs = period;}
    void        EnableGC6ExitPostProcessing(bool req){ m_Gc6ExitRequirePostProcessing = req; }
    void        SetUseRTD3(bool bUse) { m_UseRTD3 = bUse; }
    void        SetUseFastGC6(bool bUse) { m_UseFastGC6 = bUse; }
    void        SetUseLWSR(bool bUse) { m_UseLWSR = bUse; }
    void        SetUseNativeACPI(bool bUse) { m_UseNativeACPI = bUse; }
    void        SetPciConfigSpaceRestoreDelay(UINT32 val) { m_PciConfigSpaceRestoreDelayMs = val; }
    void        SetPlatformType(string type) { m_PlatformType = type; }
    RC          SetUseD3Hot(bool bUse);
    RC          Shutdown(); //!< Must be called to free up the resources before RM has been shutdown.

    string      WakeupEventToString(UINT32 wakeupEvent);
    string      WakeupReasonToString
                (
                    const LW2080_CTRL_GCX_GET_WAKEUP_REASON_PARAMS &parms
                    ,decodeStrategy strategy
                );
    RC          EnableFakeThermSensor(bool bEnable);
    RC          IsFakeThermSensorEnabled(bool *pbEnable, bool *pbSupported);
    RC          ReadThermSensor(UINT08 *pValue, UINT32 retries);
    bool        IsValidGc5Pstate(UINT32 pstate);
private:
    RC          ActivateGc5( bool bActivate);
    RC          CheckCfgSpaceNotReady();
    RC          CheckCfgSpaceReady();
    RC          EnterGc5
                (
                    bool bUseHwTimer,       //!< if true use the hardware time to wakeup GPU
                    UINT32 timeToWakeUs,    //!< how long to wait before wakeup event
                    bool bAutoSetupHwTimer  //!< if wakeup event = GCX_WAKEUP_EVENT_alarm this must be true.
                );
    RC          EnterGc6
                (
                    GC6EntryParams &entry,
                    UINT32 enterDelayUsec //!< how long to wait after entering GC6 before returning to the caller
                );
    RC          ExitGc6
                (
                    GC6ExitParams &exit,
                    UINT32 exitDelayUsec //!< delay after exiting GC6 before returning
                );
    RC          ExitGc5();
    RC          GetDeepIdleStats
                (
                    UINT32 reset,       //!< non-zero value = reset the stats after update.
                    LW2080_CTRL_GPU_READ_DEEP_IDLE_STATISTICS_PARAMS *pStats //!<
                );
    UINT32      GetLwrPstate();
    UINT64      GetPTimer();
    void        GpuReadyMemNotifyHandler();
    RC          InitializeSmbusDevice();
    static void PtimerAlarmHandler(void *pArg, void *pData, LwU32 hEvent, LwU32 data, LwU32 status);
    static bool PollPtimerAlarmFired(void *pArg);
    RC          RestoreCfgSpace();
    RC          SaveCfgSpace();
    RC          SetGpuRdyEventMemNotifier();
    RC          ValidateWakeupEvent(UINT32 wakeupEvent);
    RC          VerifyASPM();        //!< return OK if L1 ASPM substate is supported
    bool        IsGc6SupportedCommon(UINT32 pstate, UINT32 powerFeature, bool bNativeACPI);
    RC          FillGC6EntryParams(GC6EntryParams &entryParams, UINT32 enterDelayUsec);
    RC          FillGC6ExitParams(GC6ExitParams &exitParams);
    RC          GetRtd3AuxRailSwitchSupport();

    GpuSubdevice *  m_pSubdev;          //!< pointer to our current GpuSubdevice
    LwRm *          m_pLwRm;
    UINT32          m_WakeupEvent;       //!< Current wakeup event to process
    UINT32          m_WakeupReason;      //!< The reason for the last wakeup event.
    UINT32          m_WakeupGracePeriodMs;
    LW2080_CTRL_GCX_GET_WAKEUP_REASON_PARAMS m_WakeupParams;

    UINT64          m_EnterTimeMs;       //!< Total round trip time to enter GC5/GC6
    UINT32          m_EnterDelayUsec;    //!< how long to wait after entry
    LW2080_CTRL_GPU_GET_ENGINE_LOAD_TIMES_PARAMS m_EngineLoadTimeParams;
    LW2080_CTRL_GPU_GET_ID_NAME_MAPPING_PARAMS   m_EngineIdNameMappingParams;

    UINT64          m_ExitTimeMs;        //!< Total round trip time to exit GC5
    UINT32          m_ExitDelayUsec;     //!< how long to wait after exit

    Tee::Priority   m_VerbosePri;       //!< what level to use for verbose printing
    bool            m_Gc5InitDone;      //!< if true GC5 initialization if complete
    gc5Mode         m_Gc5Mode;          //!< Either DI-SSC or DI-OS
    Perf::PerfPoint m_LwrPerfPt;        //!< current pstate before GCx entry.

    // Memory storing notifer responses from RM that contain the GC5 ENTRY/EXIT status
    Surface2D       m_GpuRdyMemNotifier;
    LwNotification *m_pGpuRdyMemNotifier;   //!< Memory that gets written prior to setting the event

    ModsEvent *     m_pEventGpuReady;       //!< Event that triggers the handler
    LwRm::Handle    m_hRmEventGpuReady;

    LWOS10_EVENT_KERNEL_CALLBACK_EX m_tmrCallback;
    LwRm::Handle    m_hRmEventTimer;
    bool            m_PtimerAlarmFired; //!< indicates if we got the PTimer alarm notification

    // Variables needed to initialize and manage the SMB port.
    bool            m_bSmbInitDone;     //!< if true SM Bus initialization is complete
    SmbPort *       m_pSmbPort;         //!< pointer to the SB Bus port device to use
    bool            m_bThermSensorFound;//!< if true the GPUs internal therm sensor was found
    UINT32          m_ThermAddr;    //!< SMBus addr of the internal therm sensor.
    // GC5 specific variables
    LW2080_CTRL_GPU_QUERY_DEEP_IDLE_SUPPORT_PARAMS m_Gc5DeepIdle;
    LW2080_CTRL_GPU_READ_DEEP_IDLE_STATISTICS_PARAMS m_Gc5DeepIdleStats;

    // GC6 specific variables
    bool            m_Gc6InitDone;      //!< If true GC6 initialization is complete
    bool            m_SkipSwFuseCheck;  //!< Override the SW fuse check during initialization
    bool            m_UseJTMgr;         //!< Use JTMgr to control either the SMBUS or ACPI driver.
    bool            m_UseOptimus;       //!< Use Optimus ACPI linux driver.
    bool            m_EnableLoadTimeCalc = false;
    bool            m_Gc6ExitRequirePostProcessing;
    bool            m_UseRTD3 = false;
    bool            m_UseFastGC6 = false;
    bool            m_UseLWSR = false;
    bool            m_UseNativeACPI = false;
    bool            m_UseD3Hot = false;
    UINT32          m_PciConfigSpaceRestoreDelayMs = 0;
    string          m_PlatformType = "CFL";
    bool            m_Rtd3AuxRailSwitchSupported = false;

    // Use the Jefferson Technology Manager to control the SMBus Elwironmental Controller.
    Xp::JTMgr *     m_pJT;
    UINT32          m_EcFsmMode;    //!< GC6K or GC6M mode
    UINT32          m_Gc6Pstate;    //!< The only pstate supported by GC6
    UINT08          m_OrigFsmMode = 0;

    PciCfgSpace* m_pPciCfgSpaceGpu;
    vector<unique_ptr<PciCfgSpace> > m_cfgSpaceList;

    // Pause error polling on GCx entry
    unique_ptr<PexDevMgr::PauseErrorCollectorThread> m_pPausePexErrThread;

    UINT64          m_PTimerEntry;      //!< PTimer value just before calling EnterGC6
    UINT64          m_PTimerExit;       //!< PTimer value after calling ExitGC6.
    FLOAT64         m_CfgRdyTimeoutMs;  //!< How long to wait for the config space to be ready
    bool            m_IsGc5Active;      //!<

    #define DEEP_IDLE_STATS_TIMEOUT 1000
    //! This structure describes the necessary data for waiting for
    //! Deep Idle statistics
    struct PollDeepIdleStruct
    {
        GCxImpl *pGCx;
        LW2080_CTRL_GPU_READ_DEEP_IDLE_STATISTICS_PARAMS *pParams;
        RC   pollRc;
    };
    static bool PollGetDeepIdleStats(void *pvArgs);

};
#endif

