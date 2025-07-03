/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2009-2020 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

//! \file rmt_fermipstateswitch.cpp
//! \RM-Test to regress the path for P-state/vP-States switches. Support
//! for this test has been deprecated from GP107+ by the CLK folks. RM will
//! either have to port the api calls to universalclocktest which will be
//! maintained by the CLKs team or take ownership for the test.
//!

#ifndef INCLUDED_DTIUTILS_H
#include "gpu/tests/rm/utility/dtiutils.h"
#endif

#include "rmt_fermipstateswitch.h"
#include "gpu/tests/rm/utility/clk/clkbase.h"
#include "core/include/mrmtestinfra.h"
#include "core/include/platform.h"
#include "core/include/memcheck.h"

//! \brief FermiPStateSwitchTest constructor.
//!416667
//! Does not do much - functionality mostly lies in Setup()
//! \sa Setup
//-----------------------------------------------------------------------------
FermiPStateSwitchTest::FermiPStateSwitchTest()
{
    SetName("FermiPStateSwitchTest");
    m_pClkInfos = NULL;
    m_pSingleClkInfo = NULL;
    m_3dAlloc.SetOldestSupportedClass(FERMI_A);
    m_ThermalSlowdownState = LW2080_CTRL_THERMAL_SLOWDOWN_ENABLED;
}

vector<LwU32> FermiPStateSwitchTest::getVPStateMasks()
{
    return m_vPStateMasks;
}

vector<LwU32> FermiPStateSwitchTest::getIMPVirtualPStates()
{
    return m_vIMPVirtualPStates;
}
//! \brief FermiPStateSwitchTest destructor.
//!
//! Does not do much - functionality mostly lies in Cleanup()
//! \sa Cleanup
//-----------------------------------------------------------------------------
FermiPStateSwitchTest::~FermiPStateSwitchTest()
{
}

//! \brief IsSupported Function.
//!
//! \sa Setup
//-----------------------------------------------------------------------------
string FermiPStateSwitchTest::IsTestSupported()
{
    Gpu::LwDeviceId chipName = GetBoundGpuSubdevice()->DeviceId();

    if (!m_3dAlloc.IsSupported(GetBoundGpuDevice()))
    {
        return "P-State Switch Test is only supported for Fermi+ ";
    }

    if (IsGP107orBetter(chipName))
    {
        return "Support for this test has been deprecated from GP107+ in favor of -testname UniversalClockTest -ctp pstateswitch";
    }

    return RUN_RMTEST_TRUE;
}

//! \brief Setup Function.
//!
//! This setup function allocates the required memory, via calls to the sub
//! functions to do appropriate allocations.
//!
//! \return OK if the allocations success, specific RC if failed
//! \sa SetupFermiPStateSwitchTest
//! \sa Run
//-----------------------------------------------------------------------------
RC FermiPStateSwitchTest::Setup()
{
    RC rc;

    m_pSubdevice = GetBoundGpuSubdevice();
    m_pPerf      = m_pSubdevice->GetPerf();

    // Ask our base class(es) to setup internal state based on JS settings
    CHECK_RC(InitFromJs());
    CHECK_RC(SetupFermiPStateSwitchTest());

#if LW_WIN32_HW
    CHECK_RC(LwAPISetupPStateSwitchTest());
#endif

    return rc;
}

//! \brief Run Function.
//!
//! Run function is the primary function which exelwtes the primary sub-test i.e.
//! BasicFermiPStateSwitchTest and optionally, RandomPStateTransitionsTest.
//!
//! \return OK if the tests passed, specific RC if failed
//! \sa BasicFermiPStateSwitchTest
//-----------------------------------------------------------------------------
RC FermiPStateSwitchTest::Run()
{
    RC rc;

#ifdef INCLUDE_MDIAG
    UINT32 i,numLoops = 1;
    bool TestsRunOnMRMI = MrmCppInfra::TestsRunningOnMRMI();
    if (TestsRunOnMRMI)
    {
        CHECK_RC(MrmCppInfra::SetupDoneWaitingForOthers("RM"));
        numLoops = MrmCppInfra::GetTestLoopsArg();
        if (!numLoops) numLoops = 1;
    }
#endif

    //
    // To guarantee valid test results, first clear any pstate forces that may
    // have been left by other programs.
    //
    // Note: The RM Ctrl call for forced pstate clear does a pstate switch
    // internally to perfGetFastestLevel() value to restore clocks, in case
    // tools have changed any clocks directly. ClearPerfLimits() clears
    // perf limits and is a part of ClearForcedPState(). We need the perf
    // limits to be cleared so that the vP-State transitions are not affected.
    //
    GetBoundGpuSubdevice()->GetPerf()->ClearForcedPState();

#ifdef INCLUDE_MDIAG
    for (i=0; i<numLoops; i++)
    {
#endif
        CHECK_RC(BasicFermiPStateSwitchTest());
        if (m_randomTransitionsCount > 0)
        {
            CHECK_RC(RandomPStateTransitionsTest(m_randomTransitionsCount,
                                                 m_randomPStateSwitchIntervalMS));

        }
#if LW_WIN32_HW
        if (!GetBoundGpuSubdevice()->GetPerf()->IsPState30Supported())
        {
            CHECK_RC(LwAPIPStateSwitchTest());
        }
#endif

#ifdef INCLUDE_MDIAG
    }
    if (TestsRunOnMRMI)
    {
        CHECK_RC(MrmCppInfra::TestDoneWaitingForOthers("RM"));
    }
#endif

    return rc;
}

//! \brief Cleanup Function.
//!
//! Cleanup all the allocated variables in the setup. Cleanup can be called
//! even in errorneous condition, in such case all the allocations should be
//! freed, this function takes care of such freeing up.
//!
//! \sa Setup
//-----------------------------------------------------------------------------
RC FermiPStateSwitchTest::Cleanup()
{
    LwRmPtr pLwRm;
    LW2080_CTRL_THERMAL_SYSTEM_EXELWTE_V2_PARAMS thermalCommands = {0};
    RC rc = OK;

    // Restore the thermal slowdown mode
    thermalCommands.clientAPIVersion          = THERMAL_SYSTEM_API_VER;
    thermalCommands.clientAPIRevision         = THERMAL_SYSTEM_API_REV;
    thermalCommands.clientInstructionSizeOf   = sizeof(LW2080_CTRL_THERMAL_SYSTEM_INSTRUCTION);
    thermalCommands.instructionListSize       = 1;
    thermalCommands.instructionList[0].opcode = LW2080_CTRL_THERMAL_SYSTEM_SET_SLOWDOWN_STATE_OPCODE;
    thermalCommands.instructionList[0].operands.setThermalSlowdownState.slowdownState
                                            = m_ThermalSlowdownState;
    rc = pLwRm->ControlBySubdevice(m_pSubdevice, LW2080_CTRL_CMD_THERMAL_SYSTEM_EXELWTE_V2,
        &thermalCommands, sizeof (thermalCommands));

    // We're okay if it's not supported.
    if ((rc != OK || thermalCommands.successfulInstructions != 1) &&
         rc != RC::LWRM_NOT_SUPPORTED)
    {
        Printf(Tee::PriHigh,
               "%d: Restoring thermal slowdown failed. RC=%u  m_ThermalSlowdownState=0x%04x\n",
                    __LINE__, rc.Get(), m_ThermalSlowdownState);
    }

    // Free the client handle
    if (m_hClient)
        LwRmFree(m_hClient, m_hClient, m_hClient);

    delete [] m_pClkInfos;
    delete [] m_pSingleClkInfo;

#if LW_WIN32_HW
    delete [] m_pPerfPstates;
#endif
    return rc;
}

//-----------------------------------------------------------------------------
// Private Member Functions
//-----------------------------------------------------------------------------

//! \brief SetupFermiPStateSwitchTest Function - for basic test setup
//!
//! \return OK if the allocations success, specific RC if failed
//! \sa Setup
//-----------------------------------------------------------------------------
RC FermiPStateSwitchTest::SetupFermiPStateSwitchTest()
{
    RC rc;
    LwRmPtr pLwRm;
    LW2080_CTRL_THERMAL_SYSTEM_EXELWTE_V2_PARAMS thermalCommands =  {0};
    INT32 status = 0;

    m_hBasicSubDev = pLwRm->GetSubdeviceHandle(m_pSubdevice);
    m_hBasicClient = pLwRm->GetClientHandle();
    m_hClient = 0;
    status = (UINT32)LwRmAllocRoot((LwU32*)&m_hClient);
    rc = RmApiStatusToModsRC(status);
    CHECK_RC(rc);

    // PState transitions are supported in PS30 too
    CHECK_RC(GetSupportedPStatesInfo(m_hBasicSubDev, m_getAllPStatesInfoParams, m_vPStateMasks));

    for (UINT32 i = 0; i < m_vPStateMasks.size(); ++i)
    {
        // Get the clk freq settings of each p-state for verification
        CHECK_RC(GetPStateInfo(m_vPStateMasks[i]));

        map<LwU32, LwU32> mapClkDomainAndFreq;
        for (UINT32 j = 0; j < m_getPStateInfoParams.perfClkDomInfoListSize; ++j)
        {
            LW2080_CTRL_PERF_CLK_DOM_INFO *pClkInfoList =
                (LW2080_CTRL_PERF_CLK_DOM_INFO*)(m_getPStateInfoParams.perfClkDomInfoList);

            LW2080_CTRL_PERF_CLK_DOM_INFO ClkInfo = pClkInfoList[j];
            mapClkDomainAndFreq.insert(pair<LwU32, LwU32> (ClkInfo.domain, ClkInfo.freq));
        }

        m_mapPStateClkInfo.insert(pair<LwU32, map<LwU32, LwU32> >
            (m_vPStateMasks[i], mapClkDomainAndFreq));
    }

    if (GetBoundGpuSubdevice()->GetPerf()->IsPState30Supported())
    {
        CHECK_RC(GetSupportedIMPVPStatesInfo(m_hBasicSubDev,
                                             &m_getAllVirtualPStatesInfo,
                                             &m_vVirtualPStates,
                                             &m_vIMPVirtualPStates));
        // Get All the board specific clk info, specifically for the clk domain ordering on the board.
        CHECK_RC(pLwRm->Control(m_hBasicSubDev,
            LW2080_CTRL_CMD_CLK_CLK_DOMAINS_GET_INFO,
            &m_getBoardClkDomains, sizeof (m_getBoardClkDomains)));

        // Creating a map between the clock domain and its index in the clock entries.
        UINT32 i;
        LW2080_CTRL_BOARDOBJGRP_MASK_E32_FOR_EACH_INDEX(i, &m_getBoardClkDomains.super.objMask.super)
            LW2080_CTRL_CLK_CLK_DOMAIN_INFO *boardClkDomainInfo = &m_getBoardClkDomains.domains[i];
            m_mapClkDomainBoardMap.insert(pair<LwU32, LwU32> (boardClkDomainInfo->domain, i));
        LW2080_CTRL_BOARDOBJGRP_MASK_FOR_EACH_INDEX_END
    }

    // Get all clock domains defined
    m_getDomainsParams.clkDomainsType = LW2080_CTRL_CLK_DOMAINS_TYPE_PROGRAMMABALE_ONLY;

    CHECK_RC(pLwRm->Control(m_hBasicSubDev,
        LW2080_CTRL_CMD_CLK_GET_DOMAINS,
        &m_getDomainsParams, sizeof (m_getDomainsParams)));

    m_numClkInfos = Utility::CountBits(m_getDomainsParams.clkDomains);

    // Allocate memory for single clk info
    m_pSingleClkInfo = new
            LW2080_CTRL_CLK_INFO[sizeof(LW2080_CTRL_CLK_INFO)];

    if (m_pSingleClkInfo == NULL)
    {
        Printf(Tee::PriHigh,
               "%d: Single Clock Info allocation failed, \n", __LINE__);
        rc = RC::CANNOT_ALLOCATE_MEMORY;
        return rc;
    }

    memset((void *)m_pSingleClkInfo,
            0,
            sizeof (LW2080_CTRL_CLK_INFO));

    // Allocate memory for all clks info
    m_pClkInfos = new
            LW2080_CTRL_CLK_INFO[m_numClkInfos * sizeof(LW2080_CTRL_CLK_INFO)];

    if (m_pClkInfos == NULL)
    {
        Printf(Tee::PriHigh,
               "%d: All Clocks Info allocation failed, \n", __LINE__);
        rc = RC::CANNOT_ALLOCATE_MEMORY;
        return rc;
    }

    memset((void *)m_pClkInfos,
            0,
            m_numClkInfos * sizeof (LW2080_CTRL_CLK_INFO));

    // fill in clock info state
    for (UINT32 iLoopVar = 0, tempVal = 0; iLoopVar < 32; ++iLoopVar)
    {
        if (((1 << iLoopVar) & m_getDomainsParams.clkDomains) == 0)
            continue;

        m_pClkInfos[tempVal++].clkDomain = (1 << iLoopVar);
    }

    // Get and disable the thermal slowdown mode
    thermalCommands.clientAPIVersion          = THERMAL_SYSTEM_API_VER;
    thermalCommands.clientAPIRevision         = THERMAL_SYSTEM_API_REV;
    thermalCommands.clientInstructionSizeOf   = sizeof(LW2080_CTRL_THERMAL_SYSTEM_INSTRUCTION);
    thermalCommands.instructionListSize       = 2;
    thermalCommands.instructionList[0].opcode = LW2080_CTRL_THERMAL_SYSTEM_GET_SLOWDOWN_STATE_OPCODE;
    thermalCommands.instructionList[1].opcode = LW2080_CTRL_THERMAL_SYSTEM_SET_SLOWDOWN_STATE_OPCODE;
    thermalCommands.instructionList[1].operands.setThermalSlowdownState.slowdownState
                                            = LW2080_CTRL_THERMAL_SLOWDOWN_DISABLED_ALL;
    rc = pLwRm->ControlBySubdevice(m_pSubdevice, LW2080_CTRL_CMD_THERMAL_SYSTEM_EXELWTE_V2,
        &thermalCommands, sizeof (thermalCommands));

    // We're okay if it's not supported.
    if ((rc != OK || thermalCommands.successfulInstructions != 2) &&
         rc != RC::LWRM_NOT_SUPPORTED)
    {
        Printf(Tee::PriHigh,
               "%d: Getting or disabling thermal slowdown failed. RC=%u\n", __LINE__, rc.Get());
    }

    // Save the state for later restore.
    else
        m_ThermalSlowdownState = thermalCommands.instructionList[0].operands.getThermalSlowdownState.slowdownState;

    // Done
    return rc;
}

//! \brief BasicFermiPStateSwitchTest function.
//!
//! Makes all possible transitions between defined P-States, and
//! between defined IMP vP-States (if supported), exactly once
//!
//! \return OK if the tests passed, specific RC if failed
//------------------------------------------------------------------------------
RC FermiPStateSwitchTest::BasicFermiPStateSwitchTest()
{
    RC rc;

    Printf(Tee::PriHigh, "\n In BasicFermiPStateSwitchTest\n");
    CHECK_RC(PrintAllPStatesInfo());

    if (m_vPStateMasks.size() != 0)
    {
        Printf(Tee::PriHigh, "Starting all possible P-State transitions:\n");
    }
    else
    {
        Printf(Tee::PriHigh,
               "No P-States defined, so no P-State transitions possible\n");
        return rc;
    }

    // Initialize to the lowest p-state
    CHECK_RC(ProgramAndVerifyPStates(m_vPStateMasks[0]));

    //
    // Start the p-state switching, making sure that each possible transition
    // between p-states is covered exactly once
    //
    for (UINT32 i = 0; i < m_vPStateMasks.size()-1; ++i)
    {
        for (UINT32 j = (UINT32)m_vPStateMasks.size()-1; j >= i + 1 && i != j; --j)
        {
            CHECK_RC(ProgramAndVerifyPStates(m_vPStateMasks[j]));
            if (j != (i + 1))
            {
                CHECK_RC(ProgramAndVerifyPStates(m_vPStateMasks[i]));
            }
        }
    }

    for (INT32 i = (INT32)m_vPStateMasks.size()-2; i >= 0; i--)
    {
        CHECK_RC(ProgramAndVerifyPStates(m_vPStateMasks[i]));
    }

    if (GetBoundGpuSubdevice()->GetPerf()->IsPState30Supported())
    {
        PrintAllVirtualPStatesInfo();

        // Lwrrently iterating only over the IMP vP-States, higher priority Bringup GP104.
        if (m_vIMPVirtualPStates.size() != 0)
        {
            Printf(Tee::PriHigh, "Start the vP-State transitions:\n");
        }
        else
        {
            Printf(Tee::PriHigh, "No vP-States in the current board.\n");
            return rc;
        }

        // We try all possible combinations of vP-State transitions only once.
        for (UINT32 i = 0; i < m_vIMPVirtualPStates.size(); i++)
        {
            for (UINT32 j = (UINT32)m_vIMPVirtualPStates.size() - 1; j > i; j--)
            {
                CHECK_RC(ProgramAndVerifyVirtualPStates(m_vIMPVirtualPStates[j]));
                CHECK_RC(ProgramAndVerifyVirtualPStates(m_vIMPVirtualPStates[i]));
            }
        }
        CHECK_RC(ProgramAndVerifyVirtualPStates(m_vIMPVirtualPStates[m_vIMPVirtualPStates.size() - 1]));

        CHECK_RC(ProgramPState(LW2080_CTRL_PERF_PSTATES_CLEAR_FORCED));
    }

    Printf(Tee::PriHigh, "\n BasicFermiPStateSwitchTest done \n");

    return rc;
}

#if LW_WIN32_HW

//! \brief LwAPISetupPStateSwitchTest Function - basic test setup for LwAPI
//!
//! \return OK if the allocations success, specific RC if failed
//! \sa Setup
//-----------------------------------------------------------------------------
RC FermiPStateSwitchTest::LwAPISetupPStateSwitchTest()
{
    LwAPI_Status lwapiStatus;
    RC rc;

    // Do the reqd setup for LwAPI calls
    lwapiStatus = LwAPI_Initialize();
    if (lwapiStatus != LWAPI_OK)
    {
        Printf(Tee::PriHigh, "%d: LwAPI_Initialize failed\n", __LINE__);
        return RC::SOFTWARE_ERROR;
    }

    memset(m_hPhysicalGpu_a, 0, sizeof(m_hPhysicalGpu_a));
    memset(&clkInfo, 0, sizeof(clkInfo));

    lwapiStatus = LwAPI_EnumPhysicalGPUsInternal(m_hPhysicalGpu_a,
                                                 &m_physicalGpuCount);

    if (lwapiStatus != LWAPI_OK)
    {
        Printf(Tee::PriHigh, "%d: LwAPI_EnumPhysicalGPUs failed\n", __LINE__);
        return RC::SOFTWARE_ERROR;
    }

    // Allocate m_pPerfPstates memory
    m_pPerfPstates = new LW_GPU_PERF_PSTATES[ m_physicalGpuCount *
                                           sizeof(LW_GPU_PERF_PSTATES)];
    memset((void*)m_pPerfPstates,
            0,
            m_physicalGpuCount* sizeof(LW_GPU_PERF_PSTATES));

    // Get Pstates info
    for (LwU32 gpuIndex = 0; gpuIndex < m_physicalGpuCount; ++gpuIndex)
    {
        vector<LW_GPU_PERF_PSTATE_ID> tempPStateMasks;
        m_pPerfPstates[gpuIndex].version = LW_GPU_PERF_PSTATES_VER;

        lwapiStatus = LwAPI_GPU_GetPstates(m_hPhysicalGpu_a[gpuIndex],
                                           &m_pPerfPstates[gpuIndex]);
        if (lwapiStatus != LWAPI_OK)
        {
            Printf(Tee::PriHigh, "%d: LwAPI_GPU_GetPstates failed\n", __LINE__);
            return RC::SOFTWARE_ERROR;
        }

        // Store the pstate IDs defined per gpu
        for (LwU32 pstateIndex = 0;
             pstateIndex < m_pPerfPstates[gpuIndex].numPstates;
             ++pstateIndex)
        {
            tempPStateMasks.push_back
                (m_pPerfPstates[gpuIndex].pstates[pstateIndex].pstateId);
        }

        m_LwAPIPStateMasks.push_back(tempPStateMasks);
    }

    // Associate the clk ids with their domain names
    m_LwAPIClkDomainNames.insert (pair<LW_GPU_CLOCK_DOMAIN_ID, string>
                                  (LWAPI_GPU_CLOCK_DOMAIN_M, "MCLK"));

    m_LwAPIClkDomainNames.insert (pair<LW_GPU_CLOCK_DOMAIN_ID, string>
                                  (LWAPI_GPU_CLOCK_DOMAIN_HOST, "HOSTCLK"));

    m_LwAPIClkDomainNames.insert (pair<LW_GPU_CLOCK_DOMAIN_ID, string>
                                  (LWAPI_GPU_CLOCK_DOMAIN_DISP, "DISPCLK"));

    m_LwAPIClkDomainNames.insert (pair<LW_GPU_CLOCK_DOMAIN_ID, string>
                                  (LWAPI_GPU_CLOCK_DOMAIN_PCLK0, "PCLK0"));

    m_LwAPIClkDomainNames.insert (pair<LW_GPU_CLOCK_DOMAIN_ID, string>
                                  (LWAPI_GPU_CLOCK_DOMAIN_PCLK1, "PCLK1"));

    m_LwAPIClkDomainNames.insert (pair<LW_GPU_CLOCK_DOMAIN_ID, string>
                                  (LWAPI_GPU_CLOCK_DOMAIN_XCLK, "XCLK"));

    m_LwAPIClkDomainNames.insert (pair<LW_GPU_CLOCK_DOMAIN_ID, string>
                                  (LWAPI_GPU_CLOCK_DOMAIN_LTC2, "LTC2CLK"));

    m_LwAPIClkDomainNames.insert (pair<LW_GPU_CLOCK_DOMAIN_ID, string>
                                  (LWAPI_GPU_CLOCK_DOMAIN_GPC2, "GPC2CLK"));

    m_LwAPIClkDomainNames.insert (pair<LW_GPU_CLOCK_DOMAIN_ID, string>
                                  (LWAPI_GPU_CLOCK_DOMAIN_XBAR2, "XBAR2CLK"));

    m_LwAPIClkDomainNames.insert (pair<LW_GPU_CLOCK_DOMAIN_ID, string>
                                  (LWAPI_GPU_CLOCK_DOMAIN_SYS2, "SYS2CLK"));

    m_LwAPIClkDomainNames.insert (pair<LW_GPU_CLOCK_DOMAIN_ID, string>
                                  (LWAPI_GPU_CLOCK_DOMAIN_HUB2, "HUB2CLK"));

    m_LwAPIClkDomainNames.insert (pair<LW_GPU_CLOCK_DOMAIN_ID, string>
                                  (LWAPI_GPU_CLOCK_DOMAIN_LEG, "LEGCLK"));

    m_LwAPIClkDomainNames.insert (pair<LW_GPU_CLOCK_DOMAIN_ID, string>
                                  (LWAPI_GPU_CLOCK_DOMAIN_PWR, "PWRCLK"));

    m_LwAPIClkDomainNames.insert (pair<LW_GPU_CLOCK_DOMAIN_ID, string>
                                  (LWAPI_GPU_CLOCK_DOMAIN_MSD, "MSDCLK"));

    m_LwAPIClkDomainNames.insert (pair<LW_GPU_CLOCK_DOMAIN_ID, string>
                                  (LWAPI_GPU_CLOCK_DOMAIN_UTILS, "UTILSCLK"));

    return rc;
}

//! \brief LwAPIPStateSwitchTest function.
//!
//! Makes all possible transitions between defined P-States exactly once,
//! using LwAPI function calls
//!
//! \return OK if the tests passed, specific RC if failed
//------------------------------------------------------------------------------
RC FermiPStateSwitchTest::LwAPIPStateSwitchTest()
{
    RC rc;
    LW_GPU_PERF_PSTATE_FALLBACK fallback
        = LWAPI_GPU_PERF_PSTATE_FALLBACK_RETURN_ERROR;

    Printf(Tee::PriHigh, "\n In LwAPIPStateSwitchTest\n");
    CHECK_RC(LwAPIPrintAllPStatesInfo());

    for (LwU32 i = 0; i < m_physicalGpuCount; ++i)
    {
        Printf(Tee::PriHigh, "GPU %08x:\n", m_hPhysicalGpu_a[i]);

        if (m_LwAPIPStateMasks[i].size() != 0)
        {
            Printf(Tee::PriHigh, "Starting all possible P-State transitions:\n");
        }
        else
        {
            Printf(Tee::PriHigh,
                   "No P-States defined, so no P-State transitions possible\n");
            continue;
        }

        // Initialize to the lowest p-state
        CHECK_RC(LwAPIProgramAndVerifyPStates(m_hPhysicalGpu_a[i],
                                              m_LwAPIPStateMasks[i][0],
                                              fallback));

        for (LwU32 j = 0; j < (LwU32)(m_LwAPIPStateMasks[i].size()-1); ++j)
        {
            for (LwU32 k = (LwU32)(m_LwAPIPStateMasks[i].size()-1);
                 k >= j + 1 && k != j; --k)
            {
                CHECK_RC(LwAPIProgramAndVerifyPStates(m_hPhysicalGpu_a[i],
                                                      m_LwAPIPStateMasks[i][k],
                                                      fallback));
                if (k != j + 1)
                {
                    CHECK_RC(LwAPIProgramAndVerifyPStates(m_hPhysicalGpu_a[i],
                                                          m_LwAPIPStateMasks[i][j],
                                                          fallback));
                }
            }
        }

        for (INT32 j = (INT32)m_LwAPIPStateMasks[i].size()-2; j >= 0; j--)
        {
            CHECK_RC(LwAPIProgramAndVerifyPStates(m_hPhysicalGpu_a[i],
                                                  m_LwAPIPStateMasks[i][j],
                                                  fallback));
        }

        Printf(Tee::PriHigh, "\n LwAPIPStateSwitchTest done\n");

        return rc;
    }

    return rc;
}

//! \brief LwAPIProgramAndVerifyPStates function.
//!
//! This function Programs and Verifies the gpu to any p-state forcibly using
//! LwAPI function calls
//!
//! \param hPhysicalGpu The GPU handle
//! \param pstate Mask of the target p-state
//! \param fallback Specifies what happens when setting to target p-state fails
//! \return OK if the verification passed, specific RC if failed
//------------------------------------------------------------------------------
RC FermiPStateSwitchTest::LwAPIProgramAndVerifyPStates
(
    LwPhysicalGpuHandle hPhysicalGpu,
    LW_GPU_PERF_PSTATE_ID pstate,
    LW_GPU_PERF_PSTATE_FALLBACK fallback
)
{
    RC rc;
    LwAPI_Status lwapiStatus;
    Printf(Tee::PriHigh, "\nForce switch to P-State P%d:\n", pstate);

    // Print the clk freqs before transition
    Printf(Tee::PriHigh, "Clk frequencies before P-State transition:\n");
    CHECK_RC(LwAPIPrintAllClockFreq());

    lwapiStatus = LwAPI_GPU_SetForcePstate(hPhysicalGpu, pstate, fallback);
    if (lwapiStatus != LWAPI_OK)
    {
        Printf(Tee::PriHigh, "%d: LwAPI_GPU_SetForcePstate failed for"
                            "GPU %08x, Pstate %d, fallback %d:",
                            __LINE__, hPhysicalGpu, pstate, fallback);

        LwAPIPrintErrorMsg(lwapiStatus);
        return RC::SOFTWARE_ERROR;
    }

    // Print the clk freqs after transition
    Printf(Tee::PriHigh, "Clk frequencies after P-State transition:\n");
    CHECK_RC(LwAPIPrintAllClockFreq());

    // Verify the freq of the clks
    CHECK_RC(LwAPIVerifyPStateClockFreq(hPhysicalGpu, pstate));

    // Verify whether the switch to the desired p-state actually took place
    LW_GPU_PERF_PSTATE_ID lwrrPstate;
    lwapiStatus = LwAPI_GPU_GetLwrrentPstate(hPhysicalGpu, &lwrrPstate);
    if (lwapiStatus != LWAPI_OK)
    {
        Printf(Tee::PriHigh, "%d: LwAPI_GPU_GetLwrrentPstate failed for"
                            "GPU %08x:", __LINE__, hPhysicalGpu);

        LwAPIPrintErrorMsg(lwapiStatus);
        return RC::SOFTWARE_ERROR;
    }

    if (lwrrPstate != pstate)
    {
        Printf(Tee::PriHigh, "Setting to P-State P%d failed !!\n",
               pstate);

        Printf(Tee::PriHigh, "Target P-State : P%d\n"
                             "Achieved P-State : P%d\n",
                             pstate, lwrrPstate);

        if (LWAPI_GPU_PERF_PSTATE_FALLBACK_RETURN_ERROR == fallback)
        {
            return RC::CANNOT_SET_STATE;
        }

    }

    return rc;
}

//! \brief LwAPIVerifyPStateClockFreq function.
//!
//! Verifies the current clock frequencies against the clock frequencies for
//! the supplied p-state - the idea is to ensure that the p-state transition
//! has been able to change all the clock frequencies correctly using LwAPI
//! calls.
//!
//! \param hPhysicalGpu The GPU handle
//! \param pstate The P-state ID for which details are to be printed
//! \return OK if the verification passed, specific RC ERROR if failed
//------------------------------------------------------------------------------
RC FermiPStateSwitchTest::LwAPIVerifyPStateClockFreq
(
    LwPhysicalGpuHandle hPhysicalGpu,
    LW_GPU_PERF_PSTATE_ID pstate
)
{
    RC rc;

    // hook up clk info table
    CHECK_RC(LwAPIGetClockState(hPhysicalGpu));

    for (LwU32 i = 0; i < m_physicalGpuCount; ++i)
    {
        if (hPhysicalGpu != m_hPhysicalGpu_a[i])
        {
            continue;
        }
        for (LwU32 j = 0; j < m_pPerfPstates[i].numPstates; ++j)
        {
            if (pstate != m_pPerfPstates[i].pstates[j].pstateId)
            {
                continue;
            }
            Printf(Tee::PriHigh, "P%d:\n", pstate);
            for (LwU32 k = 0; k < m_pPerfPstates[i].numClocks; ++k)
            {
                LW_GPU_CLOCK_DOMAIN_ID domainID
                    = m_pPerfPstates[i].pstates[j].clocks[k].domainId;

                LwU32 expectedFreq = m_pPerfPstates[i].pstates[j].clocks[k].freq;
                LwU32 actualFreq;
                if (clkInfo.version == LW_GPU_CLOCK_INFO_VER)
                {
                    actualFreq = clkInfo.extendedDomain[domainID].effectiveFrequency;
                }
                else
                {
                    actualFreq = clkInfo.domain[domainID].frequency;
                }

                double freqDiff = (double)actualFreq - (double)expectedFreq;
                Printf(Tee::PriHigh, "Clk Name : %13s, ",
                       m_LwAPIClkDomainNames[domainID].c_str());
                Printf(Tee::PriHigh, "ID : %2d, ", pstate);
                Printf(Tee::PriHigh, "Expected Freq : %7d KHz, ",
                       expectedFreq);
                Printf(Tee::PriHigh, "Actual Freq : %7d KHz, ",
                       actualFreq);
                Printf(Tee::PriHigh, "Difference : %+5.1f %%\n",
                       (freqDiff * 100.0 / expectedFreq));

                // Clk freq must match EXACTLY
                MASSERT(abs(freqDiff * 100.0 / expectedFreq) <= FREQ_TOLERANCE_100X);
            }
        }
    }

    return rc;
}

//! \brief LwAPIPrintAllPStatesInfo function.
//!
//! Prints the p-states info in a formatted manner, assuming that p-states have
//! been queried before and stored in m_pPerfPstates using LwAPI calls
//!
//! \return OK if the verification passed, specific RC if failed
//------------------------------------------------------------------------------
RC FermiPStateSwitchTest::LwAPIPrintAllPStatesInfo()
{
    RC rc;
    for (LwU32 gpuIndex = 0; gpuIndex < m_physicalGpuCount; ++gpuIndex)
    {
        Printf(Tee::PriHigh, "\nGPU - %08x\n", m_hPhysicalGpu_a[gpuIndex]);
        Printf(Tee::PriHigh, "P-State details:\n");

        for (LwU32 pstatesIndex = 0; pstatesIndex < m_pPerfPstates[gpuIndex].numPstates;
             ++pstatesIndex)
        {
            Printf(Tee::PriHigh, "\nP%d:\n",
                   m_pPerfPstates[gpuIndex].pstates[pstatesIndex].pstateId);

            Printf(Tee::PriHigh, "Clock details:\n");
            for (LwU32 clocksIndex = 0; clocksIndex < m_pPerfPstates[gpuIndex].numClocks;
                 ++clocksIndex)
            {
                LW_GPU_CLOCK_DOMAIN_ID domainID
                    = m_pPerfPstates[gpuIndex].pstates[pstatesIndex].clocks[clocksIndex].domainId;

                if (m_LwAPIClkDomainNames.find(domainID) == m_LwAPIClkDomainNames.end())
                {
                    Printf(Tee::PriHigh, "Clock domain %d undefined!!\n", domainID);
                    continue;
                }

                Printf(Tee::PriHigh, " Name : %13s, Index : %2d, Freq : %7d KHz\n",
                    m_LwAPIClkDomainNames[domainID].c_str(),
                    m_pPerfPstates[gpuIndex].pstates[pstatesIndex].clocks[clocksIndex].domainId,
                    m_pPerfPstates[gpuIndex].pstates[pstatesIndex].clocks[clocksIndex].freq);
            }

            Printf(Tee::PriHigh, "Voltage details:\n");
            for (LwU32 voltagesIndex=0; voltagesIndex < m_pPerfPstates[gpuIndex].numVoltages;
                ++voltagesIndex)
            {
                switch(m_pPerfPstates[gpuIndex].pstates[pstatesIndex].voltages[voltagesIndex].domainId)
                {
                    case LWAPI_GPU_PERF_VOLTAGE_DOMAIN_CORE:
                        Printf(Tee::PriHigh, "CORE\n");
                        break;
                    case LWAPI_GPU_PERF_VOLTAGE_DOMAIN_FB:
                        Printf(Tee::PriHigh, "FB\n");
                        break;
                    default:
                        Printf(Tee::PriHigh, "Voltage domain %d not defined\n",
                            m_pPerfPstates[gpuIndex].pstates[pstatesIndex].voltages[voltagesIndex].domainId);
                }

                Printf(Tee::PriHigh, "Voltage : %5d mV, flags %x\n",
                    m_pPerfPstates[gpuIndex].pstates[pstatesIndex].voltages[voltagesIndex].mvolt,
                    m_pPerfPstates[gpuIndex].pstates[pstatesIndex].voltages[voltagesIndex].flags);
            }
        }
    }

    return rc;
}

//! \brief LwAPIPrintAllClockFreq function.
//!
//! Prints the details of all programmable clks using LwAPI
//!
//! \return OK if the verification passed, specific RC ERROR if failed
//------------------------------------------------------------------------------
RC FermiPStateSwitchTest::LwAPIPrintAllClockFreq()
{
    RC rc;
    LwU32 i, j;

    for (i = 0; i < m_physicalGpuCount; i++)
    {
        Printf(Tee::PriHigh, "GPU %08x:\n", m_hPhysicalGpu_a[i]);
        CHECK_RC(LwAPIGetClockState(m_hPhysicalGpu_a[i]));

        for (j = 0; j < LWAPI_MAX_GPU_CLOCKS; j++)
        {
            if (!(clkInfo.domain[j].bIsPresent))
            {
                continue;
            }

            if (m_LwAPIClkDomainNames.find(static_cast<LW_GPU_CLOCK_DOMAIN_ID>(j)) ==
                m_LwAPIClkDomainNames.end())
            {
                Printf(Tee::PriHigh, "Name : Not defined, ");
            }
            else
            {
                Printf(Tee::PriHigh, "Name : %13s, ",
                       m_LwAPIClkDomainNames[static_cast<LW_GPU_CLOCK_DOMAIN_ID>(j)].c_str());
            }

            Printf(Tee::PriHigh, "ID : %3d, Freq : %7d",
                   j, clkInfo.domain[j].frequency);

            if (clkInfo.version == LW_GPU_CLOCK_INFO_VER)
            {
                Printf(Tee::PriHigh, ", Actual Freq : %7d\n",
                       clkInfo.extendedDomain[j].effectiveFrequency);
            }
            else
            {
                Printf(Tee::PriHigh, "\n");
            }
        }
    }

    return rc;
}

//! \brief LwAPIGetClockState function.
//!
//! This function is used to read the current clock state for all the clocks
//! that are programmable and stores the read data in clkInfo using LwAPI .
//!
//! \param hPhysicalGpu The GPU handle
//! \return OK if clock domains are read properly, specific RC ERROR if failed.
//------------------------------------------------------------------------------
RC FermiPStateSwitchTest::LwAPIGetClockState(LwPhysicalGpuHandle hPhysicalGpu)
{
    RC rc;
    LwAPI_Status lwapiStatus;
    LwAPI_ShortString szErrDesc;

    clkInfo.version = LW_GPU_CLOCK_INFO_VER;
    lwapiStatus = LwAPI_GPU_GetAllClocks(hPhysicalGpu, &clkInfo);

    if (lwapiStatus == LWAPI_INCOMPATIBLE_STRUCT_VERSION)
    {
        Printf(Tee::PriHigh, "%d: LwAPI_GPU_GetAllClocks failed "
                             ": Incomapitble structure version %d:\n",
                             __LINE__, LW_GPU_CLOCK_INFO_VER);

        clkInfo.version = LW_GPU_CLOCK_INFO_VER_1;
        lwapiStatus = LwAPI_GPU_GetAllClocks(hPhysicalGpu, &clkInfo);
    }

    if (lwapiStatus != LWAPI_OK)
    {
        LwAPI_GetErrorMessage(lwapiStatus, szErrDesc);
        Printf(Tee::PriHigh, "%d: LwAPI_GPU_GetAllClocks for GPU[%08x] failed: %s",
               __LINE__, szErrDesc);
        return RC::SOFTWARE_ERROR;
    }

    return rc;
}

//! \brief LwAPIPrintErrorMsg function.
//!
//! This function is used to print the error message corresponding to LwAPI
//! status supplied to it.
//----------------------------------------------------------------------------
void FermiPStateSwitchTest::LwAPIPrintErrorMsg(LwAPI_Status status)
{
    LwAPI_ShortString szErrDesc;
    LwAPI_GetErrorMessage(status, szErrDesc);
    Printf(Tee::PriHigh, "%s\n", szErrDesc);
}
#endif

//! \brief RandomPStateTransitionsTest function.
//!
//! Makes random transitions between defined/possible P-States
//! and possible vP-States [Lwrrently only the IMP vP-States]
//! This test ends when both the loop iteration count expires, and the time limit expires.
//! Not changing the name of the function to avoid any issues with test dependent
//! on this test as an API.
//!
//! \param nIterations : Number of random iterations to be made
//! \param timeIntervalMS : Time interval (ms) between successive PState transitions
//! \param impMinPerfLevelPossible [optional] : MinPerfLevel where Display modeset is possible.
//! \param bAssertOnMismatch [optional] : Assert if any clock is mismatch when set to TRUE.
//! \param pMismatchedClks [optional] : Vector to store mismatch clocks.
//! \param freqTolerance100X [optional] : Frequency tolerance clocks difference allowed.
//! \param durationSecs [optional] : Durations for running random PState transitions.
//! \param bCheckUnderflow [optional] : Check if display is underflow after each PState
//!                                     transition when set to TRUE.
//! \param bIgnoreClockDiff [optional] : Ignore differences in clock programming (Greater
//!                                      than the tolerance level)
//!
//! \return OK if the tests passed, specific RC if failed
//------------------------------------------------------------------------------
RC FermiPStateSwitchTest::RandomPStateTransitionsTest
(
    UINT32 nIterations,
    UINT32 timeIntervalMS,
    UINT32 impMinPerfLevelPossible,
    bool bAssertOnMismatch,
    vector<LW2080_CTRL_CLK_EXTENDED_INFO> *pMismatchedClks,
    LwU32 freqTolerance100X,
    UINT32 durationSecs,
    bool bCheckUnderflow,
    bool bIgnoreClockDiff
)
{
    RC rc;
    UINT32 nLwrrentPerfLevelIndex;
    UINT32 nTargetPerfLevelIndex;
    UINT32 nIMPPerfLevels;
    UINT32 iteration;
    Display *pDisplay = GetDisplay();
    UINT32 numHeads = pDisplay->GetHeadCount();
    UINT32 underflowHead;
    UINT64 startTime;
    UINT64 elapseTimeSec;
    bool bPS30Supported = GetBoundGpuSubdevice()->GetPerf()->IsPState30Supported();
    Printf(Tee::PriHigh, "\n In RandomPStateTransitionsTest \n");
    if (!bPS30Supported)
    {
        UINT32 nNumPStates = (UINT32)m_vPStateMasks.size();
        if (nNumPStates == 0)
        {
            Printf(Tee::PriHigh, "No P-States defined, "
                   "so no random transitions possible\n");
            return rc;
        }
        else if (nNumPStates == 1)
        {
            Printf(Tee::PriHigh, "Only 1 P-State defined - "
                   "no point in having random transitions\n");
            return rc;
        }

        for (nIMPPerfLevels = 0;
             (nIMPPerfLevels < nNumPStates) &&
             (m_vPStateMasks[nIMPPerfLevels] <= impMinPerfLevelPossible);
             nIMPPerfLevels ++);
        if (nIMPPerfLevels <= 1)
        {
            Printf(Tee::PriHigh, "Only 1 possible P-State can be set - "
                   "no point in having random transitions\n");
            return rc;
        }
        // set the current index to impMinPState before entering random PState loop
        nLwrrentPerfLevelIndex = (nIMPPerfLevels - 1);

    }
    else
    {
        if (impMinPerfLevelPossible == DEFAULT_MIN_IMP_VPSTATE)
        {
            impMinPerfLevelPossible = 0;
            Printf(Tee::PriHigh,"No min possible IMP vP-State mentioned;"
                   " setting the default value 0\n");
        }
        //
        // Iterate over all the IMP vP-States...
        // Check impMinPerfLevelPossible against the number of IMP vP-states available
        //
        if (m_vIMPVirtualPStates.size() <= impMinPerfLevelPossible)
        {
            Printf(Tee::PriHigh, "No. of supported IMP vP-States(%d) are less "
                   "than the min state(%d) random test not possible\n",
                   (UINT32)m_vIMPVirtualPStates.size(),
                   impMinPerfLevelPossible);
            return rc;
        }
        if (impMinPerfLevelPossible == m_vIMPVirtualPStates.size() - 1)
        {
            Printf(Tee::PriHigh, "Only one vP-State supported is possible,"
                   " no point in running the test\n");
            return rc;
        }
        nIMPPerfLevels = static_cast<UINT32>(m_vIMPVirtualPStates.size() - impMinPerfLevelPossible);
        nLwrrentPerfLevelIndex = impMinPerfLevelPossible;
        // The plan is to iterate through the range
        // [impMinPerfLevelPossible,m_vIMPVirtualPStates.size()-1].
    }

    nTargetPerfLevelIndex = nLwrrentPerfLevelIndex;
    iteration = 0;
    startTime = Platform::GetTimeMS();
    elapseTimeSec = 0;

    Printf(Tee::PriHigh, "Starting Random Perf Level transitions :min iterations = %u,"
           " min time = %u sec.\n", nIterations, durationSecs);

    do
    {
        if (!bPS30Supported)
        {
            // Get new target P-State
            do
            {
                nTargetPerfLevelIndex = rand() % nIMPPerfLevels;
            }
            while (nTargetPerfLevelIndex == nLwrrentPerfLevelIndex);

            // Make the transition
            CHECK_RC(ProgramAndVerifyPStates(m_vPStateMasks[nTargetPerfLevelIndex],
                                             bAssertOnMismatch,
                                             pMismatchedClks,
                                             freqTolerance100X,
                                             false));
        }
        else
        {
            // Get new target IMP vP-State and set it.
            while (nLwrrentPerfLevelIndex == nTargetPerfLevelIndex)
            {
                nTargetPerfLevelIndex =
                    impMinPerfLevelPossible + rand() % nIMPPerfLevels;
            }
            rc = ProgramAndVerifyIMPVPStates(nTargetPerfLevelIndex,
                                             bAssertOnMismatch,
                                             pMismatchedClks,
                                             freqTolerance100X,
                                             false);

            if (rc != OK)
            {
                if (!(bIgnoreClockDiff &&
                      (rc == RC::DATA_MISMATCH || rc == RC::CLOCKS_TOO_LOW)))
                {
                    Printf(Tee::PriHigh, "ERROR: %s: Programing vPState "
                           "fails with RC =%d", __FUNCTION__, (UINT32)rc);
                    return rc;
                }
            }
            rc.Clear();
        }

        if (bCheckUnderflow)
        {
            if ((rc = DTIUtils::VerifUtils::checkDispUnderflow(pDisplay, numHeads, &underflowHead)) != OK)
            {
                if (rc == RC::LWRM_ERROR)
                {
                    string IsPS30 = bPS30Supported ? "PStates 3.0" : "!PStates 3.0";
                    Printf(Tee::PriHigh, "ERROR: %s: %s: Display underflow was detected on head %u after switching from state %u to %u\n",
                           __FUNCTION__,
                           IsPS30.c_str(),
                           underflowHead,
                           m_vPStateMasks[nLwrrentPerfLevelIndex],
                           m_vPStateMasks[nTargetPerfLevelIndex]);
                }
                else
                {
                    Printf(Tee::PriHigh, "ERROR: %s: checkDispUnderflow failed with RC = %d\n",
                           __FUNCTION__, (UINT32)rc);
                }
                return rc;
            }
        }

        // sleep for given time interval from cmd line (default 47 ms) and give chance for other threads to run
        if (GetBoundGpuSubdevice()->IsEmulation())
        {
            GpuUtility::SleepOnPTimer(timeIntervalMS, GetBoundGpuSubdevice());
        }
        else
        {
            Tasker::Sleep(timeIntervalMS);
        }

        nLwrrentPerfLevelIndex = nTargetPerfLevelIndex;
        ++iteration;
        elapseTimeSec = (Platform::GetTimeMS() - startTime) / 1000;
    }
    while (iteration < nIterations || (UINT32)elapseTimeSec < durationSecs);

    CHECK_RC(ProgramPState(LW2080_CTRL_PERF_PSTATES_CLEAR_FORCED));

    Printf(Tee::PriHigh, "\n RandomPStateTransitionsTest done \n");
    return rc;
}

//! \brief ProgramPState function.
//!
//! This function calls an RM API to force the system to a specified p-state.
//!
//! \param targetPstateMask: LW2080_CTRL_PERF_PSTATES_xxx spec for the pstate
//!                          we need to switch to; if set to
//!                          LW2080_CTRL_PERF_PSTATES_CLEAR_FORCED, then any
//!                          previously applied pstate forces are removed.
//!
//! \return OK if the verification passed, specific RC if failed
//------------------------------------------------------------------------------
RC FermiPStateSwitchTest::ProgramPState(UINT32 targetPstateMask)
{
    LW2080_CTRL_PERF_LIMIT_SET_STATUS limits[2] = { {0}, {0} };
    LW2080_CTRL_PERF_LIMITS_SET_STATUS_PARAMS setLimitsParams = {0};

    //
    // Set up limits to lock RM to a particular pstate.  Gpc2clk will still be
    // free to float as RM chooses.  If dispclk is decoupled, dispclk will also
    // be set according to RM's preference.
    //
    // Note: we use the LW2080_CTRL_CMD_PERF_LIMITS_SET_STATUS API here,
    // instead of calling Perf::ForcePState, because Perf::ForcePState uses
    // LW2080_CTRL_CMD_PERF_SET_FORCE_PSTATE_EX, and that API forces dispclk
    // decoupling to be disabled (i.e., it forces dispclk to switch to the
    // frequency specified by the pstate).  If dispclk decoupling was enabled,
    // the forcing it to be disabled can cause dispclk to be lower than what
    // IMP requires, which can lead to underflows.
    //
    limits[0].limitId = LW2080_CTRL_PERF_LIMIT_CLIENT_LOW_LOOSE_0_MIN;
    limits[1].limitId = LW2080_CTRL_PERF_LIMIT_CLIENT_LOW_LOOSE_0_MAX;

    if (LW2080_CTRL_PERF_PSTATES_CLEAR_FORCED == targetPstateMask)
    {
        limits[0].input.type = LW2080_CTRL_PERF_LIMIT_INPUT_TYPE_DISABLED;
    }
    else
    {
        limits[0].input.type = LW2080_CTRL_PERF_LIMIT_INPUT_TYPE_PSTATE;
    }
    limits[1].input.type = limits[0].input.type;

    limits[0].input.data.pstate.pstateId = targetPstateMask;
    limits[1].input.data.pstate.pstateId = targetPstateMask;

    setLimitsParams.numLimits = sizeof(limits) / sizeof(limits[0]);
    setLimitsParams.pLimits   = LW_PTR_TO_LwP64(&limits[0]);

    return LwRmPtr()->ControlBySubdevice(m_pSubdevice,
                                         LW2080_CTRL_CMD_PERF_LIMITS_SET_STATUS,
                                         &setLimitsParams,
                                         sizeof(setLimitsParams));
}

//! \brief Program a vP-State
//!
//! This function calls an RM API to force the system to a specific vP-state.
//!
//! \param targetVirtualPState[in]: The vP-state number which must be forced.
//!
//! \return OK if the verification passed, specific RC if failed
//------------------------------------------------------------------------------
RC FermiPStateSwitchTest::ProgramVirtualPState(UINT32 targetVirtualPState)
{
    LW2080_CTRL_PERF_LIMIT_SET_STATUS limits[2] = { {0}, {0} };
    LW2080_CTRL_PERF_LIMITS_SET_STATUS_PARAMS setLimitsParams = {0};

    limits[0].limitId = LW2080_CTRL_PERF_LIMIT_CLIENT_LOW_LOOSE_0_MIN;
    limits[1].limitId = LW2080_CTRL_PERF_LIMIT_CLIENT_LOW_LOOSE_0_MAX;
    //
    // The above limits are lower in priority when compared to the limit "FORCED". If the behaviour
    // is not as expected, please check the perf limits set (with the help of LwPStateViewer).
    //
    limits[0].input.type = limits[1].input.type = LW2080_CTRL_PERF_LIMIT_INPUT_TYPE_VPSTATE;

    limits[0].input.data.vpstate.vpstate = targetVirtualPState;
    limits[1].input.data.vpstate.vpstate = targetVirtualPState;

    setLimitsParams.numLimits = sizeof(limits) / sizeof(limits[0]);
    setLimitsParams.pLimits   = LW_PTR_TO_LwP64(&limits[0]);

    return LwRmPtr()->ControlBySubdevice(m_pSubdevice,
                                         LW2080_CTRL_CMD_PERF_LIMITS_SET_STATUS,
                                         &setLimitsParams,
                                         sizeof(setLimitsParams));
}

// Helper functions for p-state switch test.

//! \brief ProgramAndVerifyPStates function.
//!
//! This function Programs and Verifies the gpu to a given p-state forcibly.
//!
//! \param nTargetPstate Mask of the target p-state
//! \param bAssertOnMismatch [optional] : Assert if any clock is mismatch when set to TRUE.
//! \param pMismatchedClks [optional] : Vector to store mismatch clocks.
//! \param freqTolerance100X [optional] : Frequency tolerance clocks difference allowed.
//! \param bOutputClkInfo [optional] : Output clock info of PState if set to TRUE
//!
//! \return OK if the verification passed, specific RC if failed
//------------------------------------------------------------------------------
RC FermiPStateSwitchTest::ProgramAndVerifyPStates(LwU32 nTargetPstate,
                                                  bool bAssertOnMismatch,
                                                  vector<LW2080_CTRL_CLK_EXTENDED_INFO> *pMismatchedClks,
                                                  LwU32 freqTolerance100X,
                                                  bool bOutputClkInfo)
{
    RC rc;
    LwU32 nIdealClkFreq;
    map<LwU32, map<LwU32, LwU32> >::iterator itPStateInfo;
    map<LwU32, LwU32>::iterator itPStateClkInfo;
    UINT32 lwrrPState;

    Printf(Tee::PriHigh, "******Force switch to P-State P%d (Mask 0x%08x)******\n",
           GetLogBase2(nTargetPstate), (UINT32)(nTargetPstate));

    if (nTargetPstate < LW2080_CTRL_PERF_PSTATES_P0 ||
        nTargetPstate > LW2080_CTRL_PERF_PSTATES_MAX)
    {
        Printf(Tee::PriHigh, "Unknown target P-State P%d (Mask 0x%08x)\n",
               GetLogBase2(nTargetPstate), (UINT32)(nTargetPstate));
        return RC::BAD_PARAMETER;
    }

    if (bOutputClkInfo)
    {
        // Print the clk freqs before transition
        Printf(Tee::PriHigh, "Clk frequencies before transition to P%d:\n",
               GetLogBase2(nTargetPstate));
        CHECK_RC(PrintAllClockFreq());

        // Print the target clk freqs as well
        Printf(Tee::PriHigh,"\nTarget Clk Freq for P-State P%d:\n",
                GetLogBase2(nTargetPstate));

        itPStateInfo = m_mapPStateClkInfo.find(nTargetPstate);

        if (itPStateInfo == m_mapPStateClkInfo.end())
        {
            Printf(Tee::PriHigh, "%d: Undefined P-State mask %08x\n",
                   __LINE__, (UINT32)nTargetPstate);
            return RC::BAD_PARAMETER;
        }

        map<LwU32, LwU32> mapPStateClkInfo = itPStateInfo->second;
        for (itPStateClkInfo = mapPStateClkInfo.begin();
            itPStateClkInfo != mapPStateClkInfo.end();
            ++itPStateClkInfo)
        {
            nIdealClkFreq = itPStateClkInfo->second;

            Printf(Tee::PriHigh, "Clk Name : %13s,    ",
                   m_ClkUtil.GetClkDomainName(itPStateClkInfo->first));

            Printf(Tee::PriHigh, "Target Freq : %7d KHz\n",
                   (UINT32)(nIdealClkFreq));
        }
    }

    rc = ProgramPState(nTargetPstate);
    if (rc != OK)
    {
        Printf(Tee::PriHigh, "%d: Forced transition to P-State P%d (Mask 0x%08x) failed\n",
               __LINE__, GetLogBase2(nTargetPstate), (UINT32)(nTargetPstate));
        return rc;
    }

    if (bOutputClkInfo)
    {
        // Print the clk freqs after transition
        Printf(Tee::PriHigh, "Clk frequencies after transition to P%d:\n",
               GetLogBase2(nTargetPstate));
        CHECK_RC(PrintAllClockFreq());
    }

    // Verify the freq of the clks
    CHECK_RC(VerifyPStateClockFreq(nTargetPstate,
                                   bAssertOnMismatch,
                                   pMismatchedClks,
                                   freqTolerance100X));

    // Verify whether the switch to the desired p-state actually took place
    CHECK_RC(m_pPerf->GetLwrrentPState(&lwrrPState));

    if (lwrrPState != GetLogBase2(nTargetPstate))
    {
        Printf(Tee::PriHigh, "Setting to P-State P%d (Mask 0x%08x) failed !!\n",
               GetLogBase2(nTargetPstate), (UINT32)(nTargetPstate));

        Printf(Tee::PriHigh, "Target P-State : P%d (Mask 0x%08x)\n"
                             "Achieved P-State : P%d (Mask 0x%08x)\n",
                             GetLogBase2(nTargetPstate), (UINT32)(nTargetPstate),
                             lwrrPState, BIT(lwrrPState));

        return RC::CANNOT_SET_STATE;
    }
    else
    {
        Printf(Tee::PriHigh, "Setting to P-State P%d (Mask 0x%08x) successful !!\n",
               GetLogBase2(nTargetPstate), (UINT32)(nTargetPstate));
    }

    return rc;
}

// Helper functions for vP-State switch test.

//! \brief Helper function to set a vP-State
//!
//! \param nTargetVirtualPState[in]: the target vP-State
//! \param bAssertOnMismatch [optional]: Assert if any clock is mismatch when set to TRUE.
//! \param pMismatchedClks [optional]: Vector to store mismatch clocks.
//! \param freqTolerance100X [optional]: Frequency tolerance clocks difference allowed.
//! \param bOutputClkInfo [optional]: Output clock info of vP-State if set to TRUE.
//!
//! \return OK if the verification passed, specific RC if failed
//-----------------------------------------------------------------------------
RC FermiPStateSwitchTest::ProgramAndVerifyVirtualPStates
(
    LwU32 nTargetVirtualPState,
    bool bAssertOnMismatch,
    vector<LW2080_CTRL_CLK_EXTENDED_INFO> *pMismatchedClks,
    LwU32 freqTolerance100X,
    bool bOutputClkInfo
)
{
    RC rc;
    //
    // Check the vP-state among the list of supported vP-States.
    // Print the current clock frequencies and target frequencies
    // Force the vP-State..
    //
    vector<LwU32>::iterator itVirtualPState;
    map<LwU32, LwU32>::iterator itClkDomainMapInfo;
    for (itVirtualPState = m_vVirtualPStates.begin();
         itVirtualPState != m_vVirtualPStates.end();
         itVirtualPState++)
    {
        if (nTargetVirtualPState == *itVirtualPState)
        {
            break;
        }
    }
    if (itVirtualPState == m_vVirtualPStates.end())
    {
        Printf(Tee::PriHigh,"The Virtual State %d is not supported.\n",
               nTargetVirtualPState);
        return RC::BAD_PARAMETER;
    }

    if (bOutputClkInfo)
    {
        // Print the clk freqs before transition
        Printf(Tee::PriHigh, "Clk frequencies before transition to vP-State %d:\n",
               nTargetVirtualPState);
        CHECK_RC(PrintAllClockFreq());
        // Print the Target Frequencies
        Printf(Tee::PriHigh, "Target Clk frequencies for vP-State %d:\n",
               nTargetVirtualPState);
        // Use PerfPoints
        LW2080_CTRL_PERF_VPSTATE_INFO *pVpstateInfo =
            &m_getAllVirtualPStatesInfo.vpstates[nTargetVirtualPState];
        switch (pVpstateInfo->type)
        {
            case LW2080_CTRL_PERF_VPSTATE_TYPE_2X:
            {
                Printf(Tee::PriHigh, "Are we supposed to come here in a PState30 system?\n");
                break;
            }
            case LW2080_CTRL_PERF_VPSTATE_TYPE_3X:
            {
                // Iterate throught the clock entries and print the target and minEff frequencies
                for (itClkDomainMapInfo = m_mapClkDomainBoardMap.begin();
                     itClkDomainMapInfo != m_mapClkDomainBoardMap.end();
                     itClkDomainMapInfo++)
                {
                    Printf(Tee::PriHigh, "Domain : %s\n",
                           m_ClkUtil.GetClkDomainName(itClkDomainMapInfo->first));
                    Printf(Tee::PriHigh, "    Target Freq (MHz) : %d\n",
                        pVpstateInfo->data.v3x.clocks[itClkDomainMapInfo->second].targetFreqMHz);
                    Printf(Tee::PriHigh, "    MinEff Freq (MHz) : %d\n",
                        pVpstateInfo->data.v3x.clocks[itClkDomainMapInfo->second].minEffFreqMHz);
                }
                break;
            }
        }
    }
    Printf(Tee::PriHigh, "Forcing vP-State %d\n", nTargetVirtualPState);
    // Program vP-State
    rc = ProgramVirtualPState(nTargetVirtualPState);
    if (rc != OK)
    {
        Printf(Tee::PriHigh,
               "ERROR: %s: %d: Forced Transition to vP-State %d failed\n",
               __FUNCTION__, __LINE__, nTargetVirtualPState);
        return rc;
    }
    // Verify the clks after setting the vP-State
    CHECK_RC(VerifyVirtualPStateClockFreq(nTargetVirtualPState));
    //
    // TODO: Need to verify the vP-State directly using LW2080_CTRL_CMD_PERF_VPS_GET_INFO.
    // The above ctrl call is lwrrently not supported by LWRM.
    //

    return rc;
}

//! \brief Helper function to set an IMP vP-State.
//!
//! \param nTargetVirtualPState[in]: the target vP-State
//! \param bAssertOnMismatch [optional]: Assert if any clock is mismatch when set to TRUE.
//! \param pMismatchedClks [optional]: Vector to store mismatch clocks.
//! \param freqTolerance100X [optional]: Frequency tolerance clocks difference allowed.
//! \param bOutputClkInfo [optional]: Output clock info of vP-State if set to TRUE.
//!
//! \return OK if the verification passed, specific RC if failed
//-----------------------------------------------------------------------------
RC FermiPStateSwitchTest::ProgramAndVerifyIMPVPStates
(
    LwU32 nTargetVirtualPState,
    bool bAssertOnMismatch,
    vector<LW2080_CTRL_CLK_EXTENDED_INFO> *pMismatchedClks,
    LwU32 freqTolerance100X,
    bool bOutputClkInfo
)
{
    if (nTargetVirtualPState < m_vIMPVirtualPStates.size())
    {
        return ProgramAndVerifyVirtualPStates(m_vIMPVirtualPStates[nTargetVirtualPState],
                                              bAssertOnMismatch,
                                              pMismatchedClks,
                                              freqTolerance100X,
                                              bOutputClkInfo);
    }
    else
    {
        return RC::BAD_PARAMETER;
    }
}

//! \brief PrintAllPStatesInfo function.
//!
//! Prints the p-states info in a formatted manner, assuming that p-states have
//! been queried before and stored in m_getAllPStatesInfoParams
//!
//! \return OK if the verification passed, specific RC if failed
//------------------------------------------------------------------------------
RC FermiPStateSwitchTest::PrintAllPStatesInfo()
{
    RC rc;
    LwU32 nMaxPState = GetLogBase2(LW2080_CTRL_PERF_PSTATES_MAX);

    Printf(Tee::PriHigh, "P-States Mask = 0x%08x\n"
                         "Supported Clock Domains = 0x%08x\n"
                         "Supported Voltage Domains = 0x%08x\n",
                         (unsigned int)(m_getAllPStatesInfoParams.pstates),
                         (unsigned int)(m_getAllPStatesInfoParams.perfClkDomains),
                         (unsigned int)(m_getAllPStatesInfoParams.perfVoltageDomains));

    Printf(Tee::PriHigh, "Max P-State = P%u (Mask 0x%08x)\n",
           nMaxPState, (UINT32)(LW2080_CTRL_PERF_PSTATES_MAX));

    // Print the p-states defined
    Printf(Tee::PriHigh, "List of P-States defined:\n");
    for (UINT32 i = 0; i <= nMaxPState; ++i)
    {
        if (m_getAllPStatesInfoParams.pstates & (1 << i))
        {
            Printf(Tee::PriHigh, "\nP%d details:-\n", i);
            CHECK_RC(PrintPStateInfo((1 << i)));
        }
    }

    // Print the clock domains supported
    Printf(Tee::PriHigh, "\nList of clk domains supported by chip:\n");
    m_ClkUtil.DumpDomains(m_getAllPStatesInfoParams.perfClkDomains);

    // Print the voltage domains supported
    Printf(Tee::PriHigh, "\nList of voltage domains supported by chip:\n");
    if (m_getAllPStatesInfoParams.perfVoltageDomains &
        LW2080_CTRL_PERF_VOLTAGE_DOMAINS_CORE)
    {
        Printf(Tee::PriHigh, "CORE\n");
    }
    if (m_getAllPStatesInfoParams.perfVoltageDomains &
        LW2080_CTRL_PERF_VOLTAGE_DOMAINS_FB)
    {
        Printf(Tee::PriHigh, "FB\n");
    }

    return rc;
}

//! \brief Print vraious vP-State Info
//!
//! Prints the v-pstates info in a formatted manner, assuming that v-pstates have
//! been queried before and stored in m_getAllVirtualPStatesInfo
//!
//------------------------------------------------------------------------------
void FermiPStateSwitchTest::PrintAllVirtualPStatesInfo()
{
    map<LwU32, LwU32>::iterator itClkDomainsBoardInfo;
    Printf(Tee::PriHigh, "---------------Clk Domain Numbering on the board---------------\n");
    for (itClkDomainsBoardInfo = m_mapClkDomainBoardMap.begin();
         itClkDomainsBoardInfo != m_mapClkDomainBoardMap.end();
         itClkDomainsBoardInfo++)
    {
         Printf(Tee::PriHigh, "    %13s : %d\n",
                m_ClkUtil.GetClkDomainName(itClkDomainsBoardInfo->first),
                itClkDomainsBoardInfo->second);
    }

    Printf(Tee::PriHigh, "--------------Virtual PState Data--------------\n");
    static const string virtualPStateIndexName[] = {"D2,                        ",
                                                    "D3,                        ",
                                                    "D4,                        ",
                                                    "D5,                        ",
                                                    "OVERLWR,                   ",
                                                    "VRHOT,                     ",
                                                    "MAXBATT,                   ",
                                                    "MAXSLI,                    ",
                                                    "MAXTHERMSUS,               ",
                                                    "SLOWDOWNPWR,               ",
                                                    "MIDPOINT,                  ",
                                                    "FLAGS,                     ",
                                                    "INFLECTION0,               ",
                                                    "FULLDEFLECT,               ",
                                                    "IMPFIRST,                  ",
                                                    "IMPLAST,                   ",
                                                    "RATEDTDP,                  ",
                                                    "BOOST,                     ",
                                                    "TURBOBOOST,                ",
                                                    "MAX_LWSTOMER_BOOST,        ",
                                                    "INFLECTION1,               ",
                                                    "INFLECTION2,               ",
                                                    "WHISPER_MODE,              ",
                                                    "DLPPM_1X_ESTIMATION_MINIMUM",
                                                    "DLPPC_1X_SEARCH_MINIMUM    "};
    Printf(Tee::PriHigh, "All about the Special vP-States................\n");
    ct_assert(LW2080_CTRL_PERF_VPSTATES_IDX_NUM_INDEXES ==
        sizeof(virtualPStateIndexName)/sizeof(virtualPStateIndexName[0]));
    for (UINT32 i = 0; i < LW2080_CTRL_PERF_VPSTATES_IDX_NUM_INDEXES; i++)
    {
        Printf(Tee::PriHigh,"    %d : %s\n",
               m_getAllVirtualPStatesInfo.vpstateIdx[i],
               virtualPStateIndexName[i].c_str());
    }

    // These would be the clk Domains enabled
    Printf(Tee::PriHigh,"No. of domain groups supported on the board = %d\n",
           m_getAllVirtualPStatesInfo.nDomainGroups);

    // We iterate over all the vpstates which are available in the .super.objMask.super
    Printf(Tee::PriHigh,"Clock Info for vP-States:\n");
    UINT32 i;
    LW2080_CTRL_BOARDOBJGRP_MASK_E255_FOR_EACH_INDEX(i, &m_getAllVirtualPStatesInfo.super.objMask.super)
        LW2080_CTRL_PERF_VPSTATE_INFO *pVpstateInfo =
            &m_getAllVirtualPStatesInfo.vpstates[i];
        Printf(Tee::PriHigh, "vP-State %d:\n", i);
        switch (pVpstateInfo->type)
        {
            case LW2080_CTRL_PERF_VPSTATE_TYPE_2X:
            {
                // Shouldn't come here on a PS30 System
                Printf(Tee::PriHigh, "vP-state number = %d\n",
                       (unsigned int)(pVpstateInfo->data.v2x.vPstateNum));
                Printf(Tee::PriHigh,
                       "Maximum Required power for the vP-state = %d\n",
                       (unsigned int)(pVpstateInfo->data.v2x.reqPower10mW));
                Printf(Tee::PriHigh,
                       "Maximum Required power for the vP-state when shutdown is on = %d\n",
                       (unsigned int)(pVpstateInfo->data.v2x.reqSlowdownPower10mW));
                // Value set for each domain group
                for (UINT32 j = 0; j < LW2080_CTRL_PERF_VPSTATE_2X_DOMAIN_GROUP_NUM; j++)
                {
                    Printf(Tee::PriHigh, "            value[%u]: %u\n",
                           j, pVpstateInfo->data.v2x.value[j]);
                }
                break;
            }
            case LW2080_CTRL_PERF_VPSTATE_TYPE_3X:
            {
                Printf(Tee::PriHigh, "    pstateIdx: %d\n",
                       pVpstateInfo->data.v3x.pstateIdx);
                // Iterate the Clock Domains
                for (itClkDomainsBoardInfo = m_mapClkDomainBoardMap.begin();
                     itClkDomainsBoardInfo != m_mapClkDomainBoardMap.end();
                     itClkDomainsBoardInfo++)
                {
                    Printf(Tee::PriHigh, "    %s\n",
                           m_ClkUtil.GetClkDomainName(itClkDomainsBoardInfo->first));
                    Printf(Tee::PriHigh, "        targetFreqMHz: %d\n",
                        pVpstateInfo->data.v3x.clocks[itClkDomainsBoardInfo->second].targetFreqMHz);
                    Printf(Tee::PriHigh, "        minEffFreqMHz: %d\n",
                        pVpstateInfo->data.v3x.clocks[itClkDomainsBoardInfo->second].minEffFreqMHz);
                }
                break;
            }
        }
    LW2080_CTRL_BOARDOBJGRP_MASK_FOR_EACH_INDEX_END;

}

//! \brief GetSupportedPStatesInfo function.
//!
//! Gets the details of the p-state supported on the subdevice provided as argument
//!
//! \param hSubdevice[in]       : The handle to subDevice for whose pstates is to be retrieved
//! \param PStateMasks[out]     : The vector to be filled with the supported Pstate masks
//! \return OK if the pstate retrieval passed, specific RC if failed
//------------------------------------------------------------------------------
RC FermiPStateSwitchTest::GetSupportedPStatesInfo(LwRm::Handle hSubdevice,
                                                  LW2080_CTRL_PERF_GET_PSTATES_INFO_PARAMS &PStatesInfoParams,
                                                  vector<LwU32> &PStateMasks)
{
    RC rc;
    LwRmPtr pLwRm;

    // Get P-States info
    rc = pLwRm->Control(hSubdevice,
                            LW2080_CTRL_CMD_PERF_GET_PSTATES_INFO,
                            &PStatesInfoParams,
                            sizeof(LW2080_CTRL_PERF_GET_PSTATES_INFO_PARAMS));
    if (rc != OK)
    {
        Printf(Tee::PriHigh, "%s: LW2080_CTRL_CMD_PERF_GET_PSTATES_INFO Ctrl cmd failed with RC = %d\n",
                             __FUNCTION__, (UINT32)rc);
        return rc;
    }

    PStateMasks.clear();
    LwU32 nMaxPState = GetLogBase2(LW2080_CTRL_PERF_PSTATES_MAX);
    for (UINT32 i = 0; i <= nMaxPState; ++i)
    {
        if (PStatesInfoParams.pstates & (1 << i))
        {
            LwU32 nPStateMask = 1 << i;
            PStateMasks.push_back(nPStateMask);
        }
    }
    return rc;
}

//! \brief Get Various IMP vP-State related data.
//!
//!Gets the details of the vpstates supported on the board
//!
//! \param hSubdevice[in]: The handle to subDevice for whose pstates is to be retrieved
//! \param pVirtualPStatesInfo[out]: Will provide the vpstate info needed.
//! \param pVirtualPStates[out]: Will provide the vpstate indices
//! \param pIMPVirtualPStates[out]: Will be filled with vpstates indices populated in the vpstate entries
//!
//! \return OK if the pstate retrieval passed, specific RC if failed
//----------------------------------------------------------------------------
RC FermiPStateSwitchTest::GetSupportedIMPVPStatesInfo
(
    LwRm::Handle hSubdevice,
    LW2080_CTRL_PERF_VPSTATES_INFO *pVirtualPStatesInfo,
    vector<LwU32> *pVirtualPStates,
    vector<LwU32> *pIMPVirtualPStates
)
{
    RC rc;
    LwRmPtr pLwRm;
    UINT32 IMPfirst;
    UINT32 IMPlast;
    rc = pLwRm->Control(hSubdevice,
                        LW2080_CTRL_CMD_PERF_VPSTATES_GET_INFO,
                        pVirtualPStatesInfo,
                        sizeof(LW2080_CTRL_PERF_VPSTATES_INFO));
    if (rc != OK)
    {
        Printf(Tee::PriHigh,
               "%s: LW2080_CTRL_CMD_PERF_GET_VPSTATES_INFO Ctrl cmd failed with RC = %d\n",
               __FUNCTION__, (UINT32)rc);
        return rc;
    }
    // We iterate over all the vpstates which are available in the .super.objMask.super
    UINT32 i;
    LW2080_CTRL_BOARDOBJGRP_MASK_E255_FOR_EACH_INDEX(i, &pVirtualPStatesInfo->super.objMask.super)
        LW2080_CTRL_PERF_VPSTATE_INFO *pVpstateInfo =
            &pVirtualPStatesInfo->vpstates[i];
        switch (pVpstateInfo->type)
        {
            case LW2080_CTRL_PERF_VPSTATE_TYPE_2X:
            {
                Printf(Tee::PriHigh,
                       "Are we supposed to come here in a PState30 system?\n");
                break;
            }
            case LW2080_CTRL_PERF_VPSTATE_TYPE_3X:
            {
                // Adding all the vP-State entries to a vector
                pVirtualPStates->push_back(i);
                break;
            }
        }
    LW2080_CTRL_BOARDOBJGRP_MASK_FOR_EACH_INDEX_END;
    // Store the IMPfirst and IMPlast indices
    IMPfirst = pVirtualPStatesInfo->vpstateIdx[LW2080_CTRL_PERF_VPSTATES_IDX_IMPFIRST];
    IMPlast = pVirtualPStatesInfo->vpstateIdx[LW2080_CTRL_PERF_VPSTATES_IDX_IMPLAST];
    //
    // For colwenience, storing all the IMP vP-States in a vector, the ones
    // flagged "skipped" are skipped. No exiting if m_vIMPVirtualPStates is
    // empty to support general vP-State transitions.
    //
    if (IMPfirst > IMPlast ||
        pVirtualPStates->operator[](pVirtualPStates->size() - 1) < IMPlast)
    {
        Printf(Tee::PriHigh, "No of vP-States: %d and Last vP-State: %d\n",
               (UINT32)pVirtualPStates->size(),
               (UINT32)pVirtualPStates->operator[](pVirtualPStates->size() - 1));
        Printf(Tee::PriHigh, "Invalid IMP vP-State Indices IMPfirst: %d"
               " IMPlast: %d and no. of vP-States\n", IMPfirst, IMPlast);
        return RC::LWRM_ERROR;
    }
    // Get the first general vP-State equal to the IMP vP-State
    UINT32 j = 0;
    while (pVirtualPStates->operator[](j) < IMPfirst)
    {
        j++;
    }
    for (UINT32 i = IMPfirst; i <= IMPlast; i++)
    {
        if (i == pVirtualPStates->operator[](j))
        {
            pIMPVirtualPStates->push_back(pVirtualPStates->operator[](j++));
        }
    }
    return rc;
}

//! \brief GetPStateInfo function.
//!
//! Gets the details of the p-state supplied if it is defined, else returns
//! error .
//!
//! \param nPState The P-state mask for which details are to be printed
//! \return OK if the verification passed, specific RC if failed
//------------------------------------------------------------------------------
RC FermiPStateSwitchTest::GetPStateInfo(LwU32 nPState)
{
    RC rc;
    LwRmPtr pLwRm;
    UINT32 ClkListIdx = 0;
    UINT32 VoltListIdx = 0;
    static vector<UINT08> vClkDomBuff;
    static vector<UINT08> vVoltDomBuff;

    // Setup the parameters for querying details of p-state
    m_getPStateInfoParams.pstate = nPState;

    m_getPStateInfoParams.perfClkDomInfoListSize =
        Utility::CountBits(m_getAllPStatesInfoParams.perfClkDomains);

    m_getPStateInfoParams.perfVoltDomInfoListSize =
        Utility::CountBits(m_getAllPStatesInfoParams.perfVoltageDomains);

    vClkDomBuff.assign(sizeof(LW2080_CTRL_PERF_CLK_DOM_INFO) *
                              m_getPStateInfoParams.perfClkDomInfoListSize, 0);

    m_getPStateInfoParams.perfClkDomInfoList = LW_PTR_TO_LwP64(&vClkDomBuff[0]);

    // From PState3.0 the voltage domains are decoupled
    if (m_getPStateInfoParams.perfVoltDomInfoListSize != 0)
    {
        vVoltDomBuff.assign(sizeof(LW2080_CTRL_PERF_VOLT_DOM_INFO) *
                            m_getPStateInfoParams.perfVoltDomInfoListSize, 0);
        m_getPStateInfoParams.perfVoltDomInfoList = LW_PTR_TO_LwP64(&vVoltDomBuff[0]);
    }
    else
    {
        m_getPStateInfoParams.perfVoltDomInfoList = LW_PTR_TO_LwP64(NULL);
    }

    // set the Clock / voltage domain in each list
    for (UINT32 BitNum = 0; BitNum < 32; BitNum++)
    {
        UINT32 BitMask = 1<<BitNum;
        if (m_getAllPStatesInfoParams.perfClkDomains & BitMask)
        {
            LW2080_CTRL_PERF_CLK_DOM_INFO *pClkInfoList =
                (LW2080_CTRL_PERF_CLK_DOM_INFO*)(&vClkDomBuff[0]);
            pClkInfoList[ClkListIdx].domain = BitMask;
            ClkListIdx++;
        }
        if (m_getAllPStatesInfoParams.perfVoltageDomains & BitMask)
        {
            LW2080_CTRL_PERF_VOLT_DOM_INFO *pVoltInfoList =
                (LW2080_CTRL_PERF_VOLT_DOM_INFO*)(&vVoltDomBuff[0]);
            pVoltInfoList[VoltListIdx].domain = BitMask;
            VoltListIdx++;
        }
    }

    // Get the details of the p-state
    CHECK_RC(pLwRm->Control(m_hBasicSubDev,
                            LW2080_CTRL_CMD_PERF_GET_PSTATE_INFO,
                            &m_getPStateInfoParams,
                            sizeof(m_getPStateInfoParams)));

    return rc;
}

//! \brief VerifyPStateClockFreq function.
//!
//! Verifies the current clock frequencies against the clock frequencies for
//! the supplied p-state - the idea is to ensure that the p-state transition
//! has been able to change all the clock frequencies correctly
//!
//! \param nPState : The expected P-State against which the current P-State is compared.
//! \param bAssertOnMismatch [optional] : Assert if any clock is mismatch when set to TRUE.
//! \param pMismatchedClks [optional] : Vector to store mismatch clocks.
//! \param freqTolerance100X [optional] : Frequency tolerance clocks difference allowed.
//! \return OK if the verification passed, specific RC ERROR if failed
//------------------------------------------------------------------------------
RC FermiPStateSwitchTest::VerifyPStateClockFreq(LwU32 nPState,
                                                bool bAssertOnMismatch,
                                                vector<LW2080_CTRL_CLK_EXTENDED_INFO> *pMismatchedClks,
                                                LwU32 freqTolerance100X)
{
    RC rc;
    LwRmPtr pLwRm;
    LwU32 nIdealClkFreq;
    LwU32 nActualClkFreq;
    map<LwU32, map<LwU32, LwU32> >::iterator itPStateInfo;
    map<LwU32, LwU32>::iterator itPStateClkInfo;
    bool bClkDiffFound = false;
    LW2080_CTRL_CLK_PSTATES2_INFO clkPstates2Info;
    LW2080_CTRL_CLK_GET_PSTATES2_INFO_PARAMS getPstates2InfoParams;

    Printf(Tee::PriHigh, "\nReading the set frequency"
           " for clocks for current P-State\n");
    // hook up clk info table
    CHECK_RC(GetClockState());

    // Verify the clocks
    itPStateInfo = m_mapPStateClkInfo.find(nPState);

    if (itPStateInfo == m_mapPStateClkInfo.end())
    {
        Printf(Tee::PriHigh, "%d: Undefined P-State mask %08x\n",
               __LINE__, (UINT32)nPState);
        return RC::BAD_PARAMETER;
    }

    map<LwU32, LwU32> mapPStateClkInfo = itPStateInfo->second;

    for (itPStateClkInfo = mapPStateClkInfo.begin();
        itPStateClkInfo != mapPStateClkInfo.end();
        ++itPStateClkInfo)
    {
        nIdealClkFreq = itPStateClkInfo->second;

         // fill in clock info state
        m_pSingleClkInfo[0].clkDomain = itPStateClkInfo->first;
        m_clkCounterFreqParams.clkDomain = itPStateClkInfo->first;
        m_clkCounterFreqParams.freqKHz = 0;

        // hook up clk info table to get the back the recently set values
        m_getInfoParams.flags = 0;
        m_getInfoParams.clkInfoListSize = 1;
        m_getInfoParams.clkInfoList = (LwP64)m_pSingleClkInfo;

        // Get current clock info
        CHECK_RC(pLwRm->Control(m_hBasicSubDev,
                 LW2080_CTRL_CMD_CLK_GET_INFO,
                 &m_getInfoParams,
                 sizeof (m_getInfoParams)));

        // Get current clock pstates 2.0 info
        clkPstates2Info.clkDomain = itPStateClkInfo->first;

        getPstates2InfoParams.flags = 0;
        getPstates2InfoParams.clkInfoListSize = 1;
        getPstates2InfoParams.clkInfoList = (LwP64) &clkPstates2Info;

        CHECK_RC(pLwRm->Control(m_hBasicSubDev,
                 LW2080_CTRL_CMD_CLK_GET_PSTATES2_INFO,
                 &getPstates2InfoParams,
                 sizeof (getPstates2InfoParams)));

        Printf(Tee::PriHigh, "Clk Name : %13s,    ",
               m_ClkUtil.GetClkDomainName(itPStateClkInfo->first));

        Printf(Tee::PriHigh,"Source : %10s,    ",
               m_ClkUtil.GetClkSourceName(m_pSingleClkInfo[0].clkSource));

        Printf(Tee::PriHigh,"Flag : 0x%x,    ",
               (UINT32)m_pSingleClkInfo[0].flags);

        //
        // If the clock is tied to a pstate, then its expected value will be
        // specified by the pstate.  Otherwise, the clock frequency may vary
        // based on other perf criteria, even if the pstate is forced, and
        // there will be no "expected" value.
        //
        switch (DRF_VAL(2080, _CTRL_CLK_PSTATES2_INFO_FLAGS, _USAGE,
                        clkPstates2Info.flags))
        {
            case LW2080_CTRL_CLK_PSTATES2_INFO_FLAGS_USAGE_PSTATE:
                Printf(Tee::PriHigh, "Expected Freq : %7d KHz, ",
                       (UINT32)(nIdealClkFreq));
                break;
            case LW2080_CTRL_CLK_PSTATES2_INFO_FLAGS_USAGE_DECOUPLED:
                Printf(Tee::PriHigh, "Expected Freq : (decoupled), ");
                break;
            case LW2080_CTRL_CLK_PSTATES2_INFO_FLAGS_USAGE_RATIO:
                Printf(Tee::PriHigh, "Expected Freq :     (ratio), ");
                break;
            default:
                Printf(Tee::PriHigh, "Expected Freq :   (unknown), ");
                break;
        }

        Printf(Tee::PriHigh, "Measured Freq (SW) : %7d KHz, ",
               (UINT32)(m_pSingleClkInfo[0].actualFreq));

        nActualClkFreq = m_pSingleClkInfo[0].actualFreq;

        // Get the Clock Counter Frequency for this clock
        if (Platform::GetSimulationMode() != Platform::Fmodel)
        {
            CHECK_RC(pLwRm->Control(m_hBasicSubDev,
                            LW2080_CTRL_CMD_CLK_MEASURE_FREQ,
                            &m_clkCounterFreqParams,
                            sizeof (m_clkCounterFreqParams)));

            Printf(Tee::PriHigh, "Measured Freq (HW) : %7d KHz, ",
                   (int)m_clkCounterFreqParams.freqKHz);

            nActualClkFreq = m_clkCounterFreqParams.freqKHz;
        }

        //
        // If the clock is not a pstate clock, there is no "expected"
        // frequency, so don't bother checking if it matches.
        //
        if (!FLD_TEST_DRF(2080, _CTRL_CLK_PSTATES2_INFO_FLAGS, _USAGE, _PSTATE,
                          clkPstates2Info.flags))
        {
            Printf(Tee::PriHigh, "\n");
            continue;
        }

        double fPercentDiff = ((double)nActualClkFreq - (double)nIdealClkFreq) *
                              100.0 / (double)(nIdealClkFreq);

        Printf(Tee::PriHigh, "Difference : %+5.1f %%\n", fPercentDiff);

        // Clk freq must match EXACTLY
        if (abs(fPercentDiff) > freqTolerance100X)
        {
            bClkDiffFound = true;

            if (bAssertOnMismatch)
                MASSERT(!"Clock freq mismatch is above tolerance level allowable!");

            if (pMismatchedClks)
            {
                LW2080_CTRL_CLK_EXTENDED_INFO clkExtndInfo;
                memset(&clkExtndInfo, 0, sizeof(LW2080_CTRL_CLK_EXTENDED_INFO));

                clkExtndInfo.clkInfo.flags      = m_pSingleClkInfo[0].flags;
                clkExtndInfo.clkInfo.clkDomain  = m_pSingleClkInfo[0].clkDomain;
                clkExtndInfo.clkInfo.actualFreq = m_pSingleClkInfo[0].actualFreq;
                clkExtndInfo.clkInfo.targetFreq = nIdealClkFreq;
                clkExtndInfo.clkInfo.clkSource  = m_pSingleClkInfo[0].clkSource;
                clkExtndInfo.effectiveFreq      = nActualClkFreq;

                pMismatchedClks->push_back(clkExtndInfo);
            }
        }

    }

    if (bClkDiffFound)
        return RC::DATA_MISMATCH;

    return rc;
}

//! \brief Verify the clock frequencies set for the vP-State
//!
//! Verifies the current clock frequencies against the clock frequencies for
//! the supplied vP-state - the idea is to ensure that the vP-state transition
//! has been able to change all the clock frequencies correctly
//!
//! \param lwirtualPState[in] : the expected vP-State against which the current vP-State is compared.
//! \param bAssertOnMismatch [optional] : Assert if any clock is mismatch when set to TRUE.
//! \param pMismatchedClks [optional] : Vector to store mismatch clocks.
//! \param freqTolerance100X [optional] : Frequency tolerance clocks difference allowed.
//!
//! \return OK if the verification passed, specific RC ERROR if failed

RC FermiPStateSwitchTest::VerifyVirtualPStateClockFreq
(
    LwU32 lwirtualPState,
    bool bAssertOnMismatch,
    vector<LW2080_CTRL_CLK_EXTENDED_INFO> *pMismatchedClks,
    LwU32 freqTolerance100X
)
{
    RC rc;
    LwRmPtr pLwRm;
    LwU32 nIdealClkFreqMHz;
    LwU32 nMinEffClkFreqMHz;
    LwU32 nActualClkFreqMHz;
    vector<LwU32>::iterator itClkDomainsBoardInfo;
    map<LwU32, LwU32>::iterator itClkDomainMapInfo;
    vector<LwU32>::iterator itVirtualPState;
    double fPercentDiffIdealClk;
    double fPercentDiffMinEffClk;
    LW2080_CTRL_CLK_GET_INFO_PARAMS getInfoParams;
    LW2080_CTRL_CLK_MEASURE_FREQ_PARAMS clkCounterFreqParams;
    LW2080_CTRL_CLK_INFO *pSingleClkInfo;
    bool bClksTooLow = false;
    bool bClkDiffFound = false;

    //Allocate memory for single clk info
    pSingleClkInfo = new
        LW2080_CTRL_CLK_INFO[sizeof(LW2080_CTRL_CLK_INFO)];

    if (pSingleClkInfo == NULL)
    {
        Printf(Tee::PriHigh,
               "%s: Single Clock Info allocation failed.\n",
               __FUNCTION__);
        return RC::CANNOT_ALLOCATE_MEMORY;
    }

    memset((void *)pSingleClkInfo, 0, sizeof(LW2080_CTRL_CLK_INFO));
    // hook up clk info table, will populate m_pClkInfos.
    CHECK_RC(GetClockState());
    Printf(Tee::PriHigh, "Verifying if vP-State %d has been set\n",
           lwirtualPState);
    for (itVirtualPState = m_vVirtualPStates.begin();
         itVirtualPState != m_vVirtualPStates.end();
         itVirtualPState++)
    {
        if (lwirtualPState == *itVirtualPState)
        {
            break;
        }
    }
    if (itVirtualPState == m_vVirtualPStates.end())
    {
        Printf(Tee::PriHigh, "The Virtual State %d is not supported\n",
               lwirtualPState);
    }

    LW2080_CTRL_PERF_VPSTATE_INFO *pVpstateInfo =
        &m_getAllVirtualPStatesInfo.vpstates[lwirtualPState];
    switch (pVpstateInfo->type)
    {
        case LW2080_CTRL_PERF_VPSTATE_TYPE_2X:
        {
            Printf(Tee::PriHigh, "Should not be here in a PS30 State\n");
            break;
        }
        case LW2080_CTRL_PERF_VPSTATE_TYPE_3X:
        {
            //
            // We fetch only programmable CLK domains so we loop over those,
            // although we could have looped over the vpstate clocks instead.
            //

            for (UINT32 j = 0; j < m_numClkInfos; j++)
            {
                const char * const domainName =
                    m_ClkUtil.GetClkDomainName(m_pClkInfos[j].clkDomain);
                Printf(Tee::PriHigh, "Domain: %s\n", domainName);

                itClkDomainMapInfo =
                    m_mapClkDomainBoardMap.find(m_pClkInfos[j].clkDomain);
                if (itClkDomainMapInfo == m_mapClkDomainBoardMap.end())
                {
                    Printf(Tee::PriHigh, "ERROR: Clock Domain %s: returned by "
                           "LW2080_CTRL_CMD_CLK_GET_INFO is missing from "
                           "LW2080_CTRL_CMD_CLK_CLK_DOMAINS_GET_INFO\n",
                           domainName);
                    return RC::DATA_MISMATCH;
                }
                pSingleClkInfo[0].clkDomain = itClkDomainMapInfo->first;
                clkCounterFreqParams.clkDomain = itClkDomainMapInfo->first;
                clkCounterFreqParams.freqKHz = 0;

                // hook up clk info table to get back the recently set values
                getInfoParams.flags = 0;
                getInfoParams.clkInfoListSize = 1;
                getInfoParams.clkInfoList = (LwP64)pSingleClkInfo;

                // Get current clock info
                CHECK_RC(pLwRm->Control(m_hBasicSubDev,
                         LW2080_CTRL_CMD_CLK_GET_INFO,
                         &getInfoParams,
                         sizeof(getInfoParams)));
                Printf(Tee::PriHigh, "Measured Freq (SW): %7d KHz, ",
                       (UINT32)(pSingleClkInfo[0].actualFreq));

                nActualClkFreqMHz = pSingleClkInfo[0].actualFreq / 1000;

                // Get the Clock Counter Frequency for this clock
                if (Platform::GetSimulationMode() != Platform::Fmodel)
                {
                    CHECK_RC(pLwRm->Control(m_hBasicSubDev,
                             LW2080_CTRL_CMD_CLK_MEASURE_FREQ,
                             &clkCounterFreqParams,
                             sizeof(clkCounterFreqParams)));

                    Printf(Tee::PriHigh, "Measured Freq (HW): %7d KHz, ",
                           (int)clkCounterFreqParams.freqKHz);

                    nActualClkFreqMHz = clkCounterFreqParams.freqKHz / 1000;
                }
                nIdealClkFreqMHz =
                    pVpstateInfo->data.v3x.clocks[itClkDomainMapInfo->second].targetFreqMHz;

                nMinEffClkFreqMHz =
                    pVpstateInfo->data.v3x.clocks[itClkDomainMapInfo->second].minEffFreqMHz;

                Printf(Tee::PriHigh,
                       "    targetFreqMHz: (%d MHz) minEffFreqMHz: (%d MHz)\n",
                       nIdealClkFreqMHz, nMinEffClkFreqMHz);

                if (0 == nIdealClkFreqMHz)
                {
                    continue;
                }

                fPercentDiffMinEffClk = ((double)nActualClkFreqMHz - (double)nMinEffClkFreqMHz) *
                                        100.0 / (double)(nMinEffClkFreqMHz);

                Printf(Tee::PriHigh,
                       "    Difference: %+5.1f %% -> Against the minEffFreqMHz\n",
                       fPercentDiffMinEffClk);

                fPercentDiffIdealClk = ((double)nActualClkFreqMHz - (double)nIdealClkFreqMHz) *
                                       100.0 / (double)(nIdealClkFreqMHz);

                Printf(Tee::PriHigh,
                       "    Difference: %+5.1f %% -> Against the targetFreqMHz\n",
                       fPercentDiffIdealClk);
                //
                // With the PStates 3.0, the clock frequencies can vary with the the voltage,
                // an exact match unlikely.
                // If the measured freq is greater than the target freq, then the deviation
                // is with respect to the target freq. (When the freq is greater than
                // the target freq, it is not exactly a deviation as far as IMP is
                // considered. It is just not optimal.) If the frequency lies between the
                // target freq and the minEff freq, it is not considered a deviation. Lastly
                // if the frequency is less than the minEff we consider the deviation with
                // respect to the minEff freq.
                //
                double fPercentDiff = 0.0;
                //
                // (nActualClkFreqMHz <= nIdealClkFreqMHz) &&
                // (nActualClkFreqMHz >= nMinEffClkFreqMHz)
                // will use the default fPercentDiff value.
                //
                if (nActualClkFreqMHz > nIdealClkFreqMHz)
                {
                    fPercentDiff = fPercentDiffIdealClk;
                }
                else if (nActualClkFreqMHz < nMinEffClkFreqMHz)
                {
                    bClksTooLow = true;
                    Printf(Tee::PriHigh,
                           "Clk %s frequency (%d MHz) can't be less than "
                           "minEff Frequency (%d MHz)\n",
                           m_ClkUtil.GetClkDomainName(pSingleClkInfo[0].clkDomain),
                           nActualClkFreqMHz,
                           nMinEffClkFreqMHz);
                }

                if (abs(fPercentDiff) > freqTolerance100X)
                {
                    bClkDiffFound = true;
                    if (bAssertOnMismatch)
                        MASSERT(!"Clock freq mismatch is above tolerance level acceptable!");

                    if (pMismatchedClks)
                    {
                        LW2080_CTRL_CLK_EXTENDED_INFO clkExtndInfo;
                        memset(&clkExtndInfo, 0, sizeof(LW2080_CTRL_CLK_EXTENDED_INFO));

                        clkExtndInfo.clkInfo.flags      = pSingleClkInfo[0].flags;
                        clkExtndInfo.clkInfo.clkDomain  = pSingleClkInfo[0].clkDomain;
                        clkExtndInfo.clkInfo.actualFreq = pSingleClkInfo[0].actualFreq;
                        clkExtndInfo.clkInfo.targetFreq = nIdealClkFreqMHz;
                        clkExtndInfo.clkInfo.clkSource  = pSingleClkInfo[0].clkSource;
                        clkExtndInfo.effectiveFreq      = nActualClkFreqMHz;

                        pMismatchedClks->push_back(clkExtndInfo);
                    }
                }
            }
            break;
        }
    }

    if (bClksTooLow)
    {
        return RC::CLOCKS_TOO_LOW;
    }

    if (bClkDiffFound)
    {
        return RC::DATA_MISMATCH;
    }

    return rc;
}

//! \brief PrintPStateInfo function.
//!
//! Prints the details of the p-state supplied if it is defined, else returns
//! error .
//!
//! \param nPState The P-state flag for which details are to be printed
//! \return OK if the verification passed, specific RC ERROR if failed
//------------------------------------------------------------------------------
RC FermiPStateSwitchTest::PrintPStateInfo(LwU32 nPState)
{
    RC rc;

    // Get the p-states info
    CHECK_RC(GetPStateInfo(nPState));

    // Print the detailed info abt the p-state
    Printf(Tee::PriHigh, "P-state : P%.0f\n", log((double)nPState)/log(2.0));
    Printf(Tee::PriHigh, "Mask : 0x%08x\n", (UINT32)nPState);

    Printf(Tee::PriHigh, "# Clock domains : %u\n",
           m_getPStateInfoParams.perfClkDomInfoListSize);

    Printf(Tee::PriHigh, "Clock domain details :- \n");

    for (UINT32 i = 0; i < m_getPStateInfoParams.perfClkDomInfoListSize; ++i)
    {
        LW2080_CTRL_PERF_CLK_DOM_INFO *pClkInfoList =
            (LW2080_CTRL_PERF_CLK_DOM_INFO*)(m_getPStateInfoParams.perfClkDomInfoList);

        LW2080_CTRL_PERF_CLK_DOM_INFO ClkInfo = pClkInfoList[i];

        Printf(Tee::PriHigh, "  Name : %13s, Mask : 0x%08x, Freq : %7d KHz\n",
               m_ClkUtil.GetClkDomainName(ClkInfo.domain),
               (UINT32)(ClkInfo.domain), (int)(ClkInfo.freq));
    }

    Printf(Tee::PriHigh, "Voltage domain details :- \n");

    for (UINT32 i = 0; i < m_getPStateInfoParams.perfVoltDomInfoListSize; i++)
    {
        LW2080_CTRL_PERF_VOLT_DOM_INFO *pVoltInfoList =
            (LW2080_CTRL_PERF_VOLT_DOM_INFO*)(m_getPStateInfoParams.perfVoltDomInfoList);

        LW2080_CTRL_PERF_VOLT_DOM_INFO VoltInfo = pVoltInfoList[i];

        Printf(Tee::PriHigh, "  Mask : 0x%08x, Volt : %4d mV\n",
               (UINT32)(VoltInfo.domain), (int)(VoltInfo.lwrrTargetVoltageuV / 1000));
    }

    return rc;
}

//! \brief GetClockState function.
//!
//! This function is used to read the current clock state for all the clocks
//! that are programmable and stores the read data in m_pClkInfos.
//!
//! \return OK if clock domains are read properly, specific RC ERROR if failed.
//------------------------------------------------------------------------------
RC FermiPStateSwitchTest::GetClockState()
{
    RC rc;
    LwRmPtr pLwRm;

    // hook up clk info table to get the back the recently set values
    m_getInfoParams.flags = 0;
    m_getInfoParams.clkInfoListSize = m_numClkInfos;
    m_getInfoParams.clkInfoList = (LwP64)m_pClkInfos;

    Printf(Tee::PriHigh, "Reading the set frequency for all the clocks\n");

    // get New clocks
    CHECK_RC(pLwRm->Control(m_hBasicSubDev,
        LW2080_CTRL_CMD_CLK_GET_INFO,
        &m_getInfoParams,
        sizeof (m_getInfoParams)));

    return rc;
}

//! \brief PrintAllClockFreq function.
//!
//! Prints the details of all programmable clks
//!
//! \return OK if the verification passed, specific RC ERROR if failed
//------------------------------------------------------------------------------
RC FermiPStateSwitchTest::PrintAllClockFreq()
{
    RC rc;
    LwRmPtr pLwRm;

    // hook up clk info table
    CHECK_RC(GetClockState());

    Printf(Tee::PriHigh, "\nPresent Clock State\n");

    for (UINT32 iLoopVar = 0; iLoopVar < m_numClkInfos; iLoopVar++)
    {
        const char * const domainName = m_ClkUtil.GetClkDomainName(m_pClkInfos[iLoopVar].clkDomain);
        const char * const sourceName = m_ClkUtil.GetClkSourceName(m_pClkInfos[iLoopVar].clkSource);
        m_clkCounterFreqParams.clkDomain = m_pClkInfos[iLoopVar].clkDomain;
        m_clkCounterFreqParams.freqKHz = 0;

        Printf(Tee::PriHigh, "Domain: %13s,     ", domainName);
        Printf(Tee::PriHigh, "Source: %10s,     ", sourceName);
        Printf(Tee::PriHigh, "Current Freq (SW): %7d KHz,    ", (int)m_pClkInfos[iLoopVar].actualFreq);

        if (Platform::GetSimulationMode() != Platform::Fmodel)
        {
            // Get the Clock Counter Frequency for this clock
            CHECK_RC(pLwRm->Control(m_hBasicSubDev,
                            LW2080_CTRL_CMD_CLK_MEASURE_FREQ,
                            &m_clkCounterFreqParams,
                            sizeof (m_clkCounterFreqParams)));

            Printf(Tee::PriHigh, "Current Freq (HW): %7d KHz,    ", (int)m_clkCounterFreqParams.freqKHz);
        }
        Printf(Tee::PriHigh, "Flag: 0x%x\n", (UINT32)m_pClkInfos[iLoopVar].flags);
    }

    Printf(Tee::PriHigh, "\n");

    return rc;
}

//! \brief GetLogBase2 function.
//!
//! Returns the log to the base 2 of the given number
//!
//! \param nNum Number whose logarithm is to be computed
//! \return Base 2 Logarithm of the given number
//------------------------------------------------------------------------------
UINT32 FermiPStateSwitchTest::GetLogBase2(LwU32 nNum)
{
    return (UINT32)ceil(log((double)nNum)/log(2.0));
}

//-----------------------------------------------------------------------------
// JS Linkage
//-----------------------------------------------------------------------------

//! \brief Linkage to JavaScript
//!
//! Set up a JavaScript class that creates and owns a C++ FermiClockPathTest
//! object.
//-----------------------------------------------------------------------------
JS_CLASS_INHERIT(FermiPStateSwitchTest, RmTest,
                 "Regresses the path for P-State switches.");

CLASS_PROP_READWRITE(FermiPStateSwitchTest, randomTransitionsCount, UINT32,
                     "User provided option for running random P-State transitions");
CLASS_PROP_READWRITE(FermiPStateSwitchTest, randomPStateSwitchIntervalMS, UINT32,
                     "Time interval (ms) between successive PState transitions");

