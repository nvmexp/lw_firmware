/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2009-2016 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

//! \file rmt_fermipstateswitch.cpp
//! \RM-Test to regress the path for P-state switches. The focus of the test
//! is on fermi now, but would be supported later for earlier archs as well.
//!

#include <cmath>
#include "gpu/tests/rmtest.h"
#include "core/include/jscript.h"
#include "gpu/tests/gputestc.h"
#include "gpu/utility/rmclkutil.h"
#include "gpu/include/gpusbdev.h"
#include "core/include/utility.h"
#include "lwRmApi.h"
#include "class/cl9097.h" // FERMI_A
#include "ctrl/ctrl2080.h"
#include "ctrl/ctrl2080/ctrl2080perf.h"
#include "ctrl/ctrl2080/ctrl2080clk.h"
#include "gpu/include/gralloc.h"

// #define DEBUG_CHECK_RC 1
#define FREQ_TOLERANCE_100X 2

#define FREQ_TOLERANCE_NAFLL 5

#define DEFAULT_MIN_IMP_VPSTATE 256

// Use LwAPI only for winmods, disabled for the rest
#if LWOS_IS_WINDOWS && !defined(SIM_BUILD) && !defined(WIN_MFG)
    #define LW_WIN32_HW 1
#else
    #define LW_WIN32_HW 0
#endif

#if LW_WIN32_HW

#include <lwapi.h>
#include "lwRmReg.h"
#include <windows.h>

#endif

class FermiPStateSwitchTest: public RmTest
{
public:
    FermiPStateSwitchTest();
    virtual ~FermiPStateSwitchTest();
    virtual string  IsTestSupported();
    virtual RC Setup();
    virtual RC Run();
    virtual RC Cleanup();

    SETGET_PROP(randomTransitionsCount, UINT32);
    SETGET_PROP(randomPStateSwitchIntervalMS, UINT32);
    vector<LwU32> getVPStateMasks();
    vector<LwU32> getIMPVirtualPStates();

private:
    RmtClockUtil m_ClkUtil;
    LwRm::Handle m_hBasicClient;
    LwRm::Handle m_hBasicSubDev;
    LwRm::Handle m_hClient;
    GpuSubdevice  *m_pSubdevice;
    Perf          *m_pPerf;
    ThreeDAlloc    m_3dAlloc;

    //! Command line switch for enabling random transitions test
    UINT32 m_randomTransitionsCount;

    UINT32 m_randomPStateSwitchIntervalMS;

    //! P-States parameter structures for RM ctrl cmds
    LW2080_CTRL_PERF_GET_PSTATES_INFO_PARAMS m_getAllPStatesInfoParams;
    LW2080_CTRL_PERF_GET_PSTATE_INFO_PARAMS m_getPStateInfoParams;

    //! TODO
    //! Using an array for general vP-States and the IMP vP-States
    //! would be easier to debug.
    //
    vector<LwU32> m_vVirtualPStates;
    vector<LwU32> m_vIMPVirtualPStates;
    vector<LwU32> m_vPStateMasks;

    //! vP-state RM ctrl cmds ....
    LW2080_CTRL_PERF_VPSTATES_INFO m_getAllVirtualPStatesInfo;

    //! CLK Domains variables
    LW2080_CTRL_CLK_GET_DOMAINS_PARAMS m_getDomainsParams;
    LW2080_CTRL_CLK_GET_INFO_PARAMS m_getInfoParams;
    LW2080_CTRL_CLK_MEASURE_FREQ_PARAMS m_clkCounterFreqParams;
    LW2080_CTRL_CLK_INFO *m_pClkInfos;
    LW2080_CTRL_CLK_INFO *m_pSingleClkInfo;
    UINT32 m_numClkInfos;

    //! CLK Domain variable specific to vP-states
    LW2080_CTRL_CLK_CLK_DOMAINS_INFO m_getBoardClkDomains;
    //! map between the clk domains and the board specific numbering
    map<LwU32, LwU32> m_mapClkDomainBoardMap;

    //
    //! Contains the ideal clk frequencies per p-state
    //! Primary key is p-state mask, secondary key is clk domain mask
    //
    map<LwU32, map<LwU32, LwU32> > m_mapPStateClkInfo;

    //! State of thermal slowdown before the test began
    LW2080_CTRL_THERMAL_SLOWDOWN_STATE m_ThermalSlowdownState;

public:
    //
    //! Setup and Test functions
    //
    RC SetupFermiPStateSwitchTest();
    RC BasicFermiPStateSwitchTest();
    RC RandomPStateTransitionsTest(UINT32 iterations,
                                   UINT32 timeInterval,
                                   UINT32 impMinPStatePossible = LW2080_CTRL_PERF_PSTATES_MAX,
                                   bool bAssertOnMismatch = true,
                                   vector<LW2080_CTRL_CLK_EXTENDED_INFO> *pMismatchedClks = NULL,
                                   LwU32 freqTolerance100X = FREQ_TOLERANCE_100X,
                                   UINT32 durationSecs = 0,
                                   bool bCheckUnderflow = false,
                                   bool bIgnoreClockDiff = false);
    RC ProgramAndVerifyPStates(LwU32 pstate,
                               bool bAssertOnMismatch = true,
                               vector<LW2080_CTRL_CLK_EXTENDED_INFO> *pMismatchedClks = NULL,
                               LwU32 freqTolerance100X = FREQ_TOLERANCE_100X,
                               bool bOutputClkInfo = true);
    RC ProgramAndVerifyVirtualPStates(LwU32 virtualPState,
                                      bool bAssertOnMismatch = true,
                                      vector<LW2080_CTRL_CLK_EXTENDED_INFO> *pMismatchedClks = NULL,
                                      LwU32 freqTolerance100X = FREQ_TOLERANCE_NAFLL,
                                      bool bOutputClkInfo = true);
    RC ProgramAndVerifyIMPVPStates(LwU32 nTargetVirtualPState,
                                   bool bAssertOnMismatch = true,
                                   vector<LW2080_CTRL_CLK_EXTENDED_INFO> *pMismatchedClks = NULL,
                                   LwU32 freqTolerance100X = FREQ_TOLERANCE_NAFLL,
                                   bool bOutputClkInfo = true);
    RC ProgramPState(UINT32 targetPstateMask);
    RC ProgramVirtualPState(UINT32 targetVirtualPstate);
    RC GetPStateInfo(LwU32 pstate);
    RC GetSupportedPStatesInfo(LwRm::Handle hSubdevice,
                               LW2080_CTRL_PERF_GET_PSTATES_INFO_PARAMS &PStatesInfoParams,
                               vector<LwU32> &PStateMasks);
    RC GetSupportedIMPVPStatesInfo(LwRm::Handle hSubdevice,
                                   LW2080_CTRL_PERF_VPSTATES_INFO *pVirtualPStatesInfo,
                                   vector<LwU32> *pVirtualPStates,
                                   vector<LwU32> *pIMPVirtualPStates);
    RC VerifyPStateClockFreq(LwU32 pstate,
                             bool bAssertOnMismatch = true,
                             vector<LW2080_CTRL_CLK_EXTENDED_INFO> *pMismatchedClks = NULL,
                             LwU32 freqTolerance100X = FREQ_TOLERANCE_100X);

    RC VerifyVirtualPStateClockFreq(LwU32 lwirtualPState,
                                    bool bAssertOnMismatch = false,
                                    vector<LW2080_CTRL_CLK_EXTENDED_INFO> *pMismatchedClks = NULL,
                                    LwU32 freqTolerance100X = FREQ_TOLERANCE_NAFLL);

    //! Helper functions
    RC PrintAllPStatesInfo();
    void PrintAllVirtualPStatesInfo();
    RC PrintPStateInfo(LwU32 pstate);
    RC PrintAllClockFreq();
    RC GetClockState();
    UINT32 GetLogBase2(LwU32 integer);

private:

#if LW_WIN32_HW
    //! P-States parameter structures for LwAPI calls
    LwPhysicalGpuHandle m_hPhysicalGpu_a[LWAPI_MAX_PHYSICAL_GPUS];
    LW_GPU_PERF_PSTATES* m_pPerfPstates;
    LwU32 m_physicalGpuCount;

    //! Clk domains variables
    LW_GPU_CLOCK_INFO clkInfo;

    //! Contains clock domain names
    map<LW_GPU_CLOCK_DOMAIN_ID, string> m_LwAPIClkDomainNames;

    //! Contains list of defined P-States per GPU
    vector< vector<LW_GPU_PERF_PSTATE_ID> > m_LwAPIPStateMasks;

    //! PState transitions using LwAPI
    RC LwAPISetupPStateSwitchTest();
    RC LwAPIPStateSwitchTest();

    RC LwAPIProgramAndVerifyPStates(LwPhysicalGpuHandle hPhysicalGpu,
                                    LW_GPU_PERF_PSTATE_ID pstate,
                                    LW_GPU_PERF_PSTATE_FALLBACK fallback);

    RC LwAPIVerifyPStateClockFreq(LwPhysicalGpuHandle hPhysicalGpu,
                                  LW_GPU_PERF_PSTATE_ID pstate);
    RC LwAPIPrintAllPStatesInfo();
    RC LwAPIGetClockState(LwPhysicalGpuHandle hPhysicalGpu);
    RC LwAPIPrintAllClockFreq();
    void LwAPIPrintErrorMsg(LwAPI_Status status);
#endif
};

