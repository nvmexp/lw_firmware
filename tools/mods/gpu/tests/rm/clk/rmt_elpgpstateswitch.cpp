/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2006-2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

//!
//! \file rmt_elpgpstateswitch.cpp
//! \brief Test ELPG_ON/OFF while p-state switching is active
//!
//! TODORMT - Write a more detailed file comment

#include <cmath>
#include "gpu/tests/rmtest.h"
#include "gpu/include/gpusbdev.h"
#include "core/include/modsdrv.h"
#include "core/include/script.h"
#include "core/include/jscript.h"
#include "gpu/include/gpusbdev.h"
#include "rmpmucmdif.h"
#include "core/include/tasker.h"
#include "core/include/utility.h"
#include "gpu/utility/rmclkutil.h"
#include "class/cl85b6.h"
#include "class/cl902d.h"
#include "class/cl90b1.h"
#include "ctrl/ctrl2080.h"
#include "ctrl/ctrl208f.h"
#include "core/include/memcheck.h"

#define FREQ_TOLERANCE_100X 10
#define GR_ENGINE                      RM_PMU_LPWR_CTRL_ID_GR_PG
#define VID_ENGINE                     RM_PMU_LPWR_CTRL_ID_GR_PASSIVE  // TODO-djamadar : Remove VID_ENGINE
#define VIC_ENGINE                     RM_PMU_LPWR_CTRL_ID_GR_RG       // TODO-aranjan  : Remove VIC_ENGINE

class ElpgPStateSwitchTest : public RmTest
{
public:
    ElpgPStateSwitchTest();
    virtual ~ElpgPStateSwitchTest();

    virtual string IsTestSupported();

    virtual RC Setup();
    virtual RC Run();
    virtual RC Cleanup();

private:
    RmtClockUtil m_ClkUtil;
    LwRm::Handle m_hBasicClient;
    LwRm::Handle m_hBasicSubDev;
    LwRm::Handle m_hSubDevDiag;

    bool             m_NoGr;
    bool             m_NoVideo;
    bool             m_NoVic;
    vector<UINT32> m_vEngineList;
    vector<UINT32> m_vPStateMasks;

    // P-States parameter structures for RM ctrl cmds
    LW2080_CTRL_PERF_GET_PSTATES_INFO_PARAMS m_getAllPStatesInfoParams;
    LW2080_CTRL_PERF_GET_PSTATE_INFO_PARAMS m_getPStateInfoParams;
    LW2080_CTRL_PERF_GET_LWRRENT_PSTATE_PARAMS m_getLwrrentPStateParams;
    LW2080_CTRL_PERF_SET_FORCE_PSTATE_PARAMS m_setForcePStateParams;

    // CLK Domains variables
    LW2080_CTRL_CLK_GET_DOMAINS_PARAMS m_getDomainsParams;
    LW2080_CTRL_CLK_GET_INFO_PARAMS m_getInfoParams;
    LW2080_CTRL_CLK_INFO *m_pClkInfos;
    LW2080_CTRL_CLK_INFO *m_pSingleClkInfo;
    UINT32 m_numClkInfos;

    //
    // Contains the ideal clk frequencies per p-state
    // Primary key is p-state mask, secondary key is clk domain mask
    //
    map<LwU32, map<LwU32, LwU32> > m_mapPStateClkInfo;

    //
    // Setup and Test functions
    //
    RC SetupElpgPStateSwitchTest();
    RC BasicElpgPStateSwitchTest();
    RC GetPStateInfo(LwU32 pstate);
    RC VerifyPStateSwitch(LwU32 pstate);
    RC VerifyPStateClockFreq(LwU32 pstate);

    // Helper functions
    RC QueryPowerGatingParameter(UINT32 param, UINT32 paramExt, UINT32 *paramVal,
                                 bool set);
    RC PrintAllClockFreq();
    UINT32 GetLogBase2(LwU32 integer);
    RC GetClockState();
    RC AllowPStateChangeInElpgSequence(LwBool flag);
    RC AllowElpgOnDuringPstateChange(LwBool flag);

};

//! \brief ElpgPStateSwitchTest constructor
//!
//! Does not do much - functionality mostly lies in Setup()
//!
//! \sa Setup
//------------------------------------------------------------------------------
ElpgPStateSwitchTest::ElpgPStateSwitchTest()
{
    SetName("ElpgPStateSwitchTest");
    m_NoGr      = false;
    m_NoVideo   = false;
    m_NoVic     = false;
    m_pClkInfos = NULL;
    m_pSingleClkInfo = NULL;
}

//! \brief ElpgPStateSwitchTest destructor.
//!
//! Does not do much - functionality mostly lies in Cleanup()
//!
//! \sa Cleanup
//------------------------------------------------------------------------------
ElpgPStateSwitchTest::~ElpgPStateSwitchTest()
{

}

//! \brief Will return true only on GF10X/GF10Y chips
//!
//! \return True if the test can be run in the current environment,
//!         false otherwise
//------------------------------------------------------------------------------
string ElpgPStateSwitchTest::IsTestSupported()
{
    RC     rc;
    UINT32 noOfEngs = 0;
    UINT32 powerGatingParameterEnabled;
    bool   isGF10x;

    isGF10x = ((IsClassSupported(FERMI_TWOD_A))&&
               (IsClassSupported(GF100_MSVLD))&&
               (IsClassSupported(GT212_SUBDEVICE_PMU)));

    if(isGF10x)
    {
        rc = QueryPowerGatingParameter
                 (LW2080_CTRL_MC_POWERGATING_PARAMETER_NUM_POWERGATEABLE_ENGINES,
                  0, &noOfEngs, false);

        if (rc != OK)
        {
            return "Error while querying number of powergateable engines";

        }
    }
    else
    {
        return "Test supported only for GF10X/GF10Y";
    }

    for(UINT32 eng = 0; eng < noOfEngs; eng++)
    {
        switch(eng)
        {
            case GR_ENGINE:
                if (!m_NoGr)
                {
                    rc = QueryPowerGatingParameter(LW2080_CTRL_MC_POWERGATING_PARAMETER_ENABLED,
                                       GR_ENGINE, &powerGatingParameterEnabled, false);
                    if (rc == RC::LWRM_NOT_SUPPORTED)
                    {
                        return "Elpg Is Not Enabled on GR Engine";
                    }
                    else
                    {
                        m_NoGr = !(powerGatingParameterEnabled);
                        if(!m_NoGr)
                        {
                            m_vEngineList.push_back(GR_ENGINE);
                        }
                    }
                }
                break;

            case VID_ENGINE:
                if (!m_NoVideo)
                {
                    rc = QueryPowerGatingParameter(LW2080_CTRL_MC_POWERGATING_PARAMETER_ENABLED,
                                    VID_ENGINE, &powerGatingParameterEnabled, false);
                    if (rc == RC::LWRM_NOT_SUPPORTED)
                    {
                        return "Elpg Is Not Enabled on Video Engine";
                    }
                    else
                    {
                        m_NoVideo = !(powerGatingParameterEnabled);
                        if(!m_NoVideo)
                        {
                            m_vEngineList.push_back(VID_ENGINE);
                        }
                    }
                }
                break;

            case VIC_ENGINE:
                if (!m_NoVic)
                {
                    rc = QueryPowerGatingParameter(LW2080_CTRL_MC_POWERGATING_PARAMETER_ENABLED,
                                    VIC_ENGINE, &powerGatingParameterEnabled, false);
                    if (rc == RC::LWRM_NOT_SUPPORTED)
                    {
                        return "Elpg Is Not Enabled on Vic Engine";
                    }
                    else
                    {
                        m_NoVic = !(powerGatingParameterEnabled);
                        if(!m_NoVic)
                        {
                            m_vEngineList.push_back(VIC_ENGINE);
                        }
                    }
                }
                break;
             default:
                return ("Engine not supported in this test");
        }
    }

    return RUN_RMTEST_TRUE;
}

//! \brief Setup Function.
//!
//! This setup function allocates the required memory, via calls to the sub
//! functions to do appropriate allocations.
//!
//! \return OK if the allocations success, specific RC if failed
//------------------------------------------------------------------------------
RC ElpgPStateSwitchTest::Setup()
{
    RC rc;

    // Ask our base class(es) to setup internal state based on JS settings
    CHECK_RC(InitFromJs());
    CHECK_RC(SetupElpgPStateSwitchTest());

    return rc;
}

//! \brief Run Function.
//!
//! Run function is the primary function which exelwtes the primary sub-test
//!
//! \return OK if the tests passed, specific RC if failed
//------------------------------------------------------------------------------
RC ElpgPStateSwitchTest::Run()
{
    RC rc;

    CHECK_RC(BasicElpgPStateSwitchTest());
    return rc;
}

//! \brief Cleanup Function.
//!
//! Cleanup all the allocated variables in the setup. Cleanup can be called
//! even in errorneous condition, in such case all the allocations should be
//! freed, this function takes care of such freeing up.
//!
//! \return OK if the deallocations succeed, specific RC if failed
//------------------------------------------------------------------------------
RC ElpgPStateSwitchTest::Cleanup()
{
    delete []m_pSingleClkInfo;
    delete []m_pClkInfos;

    return OK;
}

//------------------------------------------------------------------------------
// Private Member Functions
//------------------------------------------------------------------------------

//! \brief SetupElpgPStateSwitchTest Function - for basic test setup
//!
//! \return OK if the allocations success, specific RC if failed
//! \sa Setup
//-----------------------------------------------------------------------------
RC ElpgPStateSwitchTest::SetupElpgPStateSwitchTest()
{
    RC rc;
    LwRmPtr pLwRm;

    m_hBasicSubDev = pLwRm->GetSubdeviceHandle(GetBoundGpuSubdevice());
    m_hBasicClient = pLwRm->GetClientHandle();
    m_hSubDevDiag = GetBoundGpuSubdevice()->GetSubdeviceDiag();

    // Get P-States info
    CHECK_RC(pLwRm->Control(m_hBasicSubDev,
                            LW2080_CTRL_CMD_PERF_GET_PSTATES_INFO,
                            &m_getAllPStatesInfoParams,
                            sizeof(m_getAllPStatesInfoParams)));

    LwU32 nMaxPState = 0;
    nMaxPState = GetLogBase2(LW2080_CTRL_PERF_PSTATES_MAX);
    for (UINT32 i = 0; i <= nMaxPState; ++i)
    {
        if (m_getAllPStatesInfoParams.pstates & (1 << i))
        {
            LwU32 nPStateMask = 1 << i;
            m_vPStateMasks.push_back(nPStateMask);

            // Get the clk freq settings of each p-state for verification
            CHECK_RC(GetPStateInfo(nPStateMask));

            map<LwU32, LwU32> mapClkDomainAndFreq;
            for (UINT32 j = 0; j < m_getPStateInfoParams.perfClkDomInfoListSize; ++j)
            {
                LW2080_CTRL_PERF_CLK_DOM_INFO * pClkInfoList =
                     (LW2080_CTRL_PERF_CLK_DOM_INFO*)(m_getPStateInfoParams.perfClkDomInfoList);

                LW2080_CTRL_PERF_CLK_DOM_INFO ClkInfo = pClkInfoList[j];
                mapClkDomainAndFreq.insert(pair<LwU32, LwU32> (ClkInfo.domain, ClkInfo.freq));
            }

            m_mapPStateClkInfo.insert(pair<LwU32, map<LwU32, LwU32> >
                                       (nPStateMask, mapClkDomainAndFreq));
        }
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

    return rc;
}

//! \brief BasicElpgPStateSwitchTest function.
//!
//! Makes all possible transitions between defined P-States
//!
//! \return OK if the tests passed, specific RC if failed
//------------------------------------------------------------------------------
RC ElpgPStateSwitchTest::BasicElpgPStateSwitchTest()
{
    RC rc;
    LwRmPtr pLwRm;
    LW2080_CTRL_PERF_SET_FORCE_PSTATE_PARAMS setForcePstateParams = {0};
    LW2080_CTRL_MC_INDUCE_ELPG_PARAMS induceElpgParams = {0};

    Printf(Tee::PriHigh,"***Starting phase 1: Pstate change during ELPG ON event***\n");

    CHECK_RC(AllowPStateChangeInElpgSequence(LW_TRUE));
    for(LwU32 i = 0;i < m_vEngineList.size(); ++i)
    {
        UINT32 initialGatingCount;
        UINT32 finalGatingCount;

        rc = QueryPowerGatingParameter
               (LW2080_CTRL_MC_POWERGATING_PARAMETER_GATINGCOUNT, m_vEngineList[i],
                &initialGatingCount, false);

        if(rc != OK)
        {
            Printf(Tee::PriHigh,"%d: %s: LW2080_CTRL_CMD_MC_QUERY_POWERGATING_PARAMETER call failed\n",
            __LINE__, __FUNCTION__);
            return rc;
        }

        Printf(Tee::PriHigh,"Initial gating count for engine %d = %d\n",
               m_vEngineList[i] , initialGatingCount);

        // Force ELPG event
        induceElpgParams.numEngines = 1;
        induceElpgParams.engineList[0] = m_vEngineList[i];

        Printf(Tee::PriHigh,"Initiating ELPG ON on engine 0x%x\n", m_vEngineList[i]);

        CHECK_RC(pLwRm->Control(m_hBasicSubDev,
                                LW2080_CTRL_CMD_MC_INDUCE_ELPG,
                                &induceElpgParams,
                                sizeof(induceElpgParams)));

        rc = QueryPowerGatingParameter
               (LW2080_CTRL_MC_POWERGATING_PARAMETER_GATINGCOUNT, m_vEngineList[i],
                &finalGatingCount, false);

        if(rc != OK)
        {
            Printf(Tee::PriHigh,"%d: %s: LW2080_CTRL_CMD_MC_QUERY_POWERGATING_PARAMETER call failed\n",
            __LINE__, __FUNCTION__);
            return rc;
        }

        Printf(Tee::PriHigh,"Final gating count for engine %d = %d\n",
               m_vEngineList[i] , finalGatingCount);

        if (finalGatingCount <= initialGatingCount)
        {
            Printf(Tee::PriHigh,"%d: %s: Failed to gate engine 0x%x\n",
                   __LINE__, __FUNCTION__, m_vEngineList[i]);
            return RC::LWRM_ERROR;
        }
    }
    Printf(Tee::PriHigh,"***Completed phase 1***\n");
    CHECK_RC(AllowPStateChangeInElpgSequence(LW_FALSE));

    Printf(Tee::PriHigh,"***Starting phase 2: ELPG ON before programming clocks during p-state change***\n");

    CHECK_RC(AllowElpgOnDuringPstateChange(LW_TRUE));
    for(LwU32 i = 0; i < m_vPStateMasks.size(); ++i)
    {
        setForcePstateParams.forcePstate = m_vPStateMasks[i];
        setForcePstateParams.fallback = LW2080_CTRL_PERF_PSTATE_FALLBACK_RETURN_ERROR;
        CHECK_RC(pLwRm->Control(m_hBasicSubDev,
                                LW2080_CTRL_CMD_PERF_SET_FORCE_PSTATE,
                                &setForcePstateParams,
                                sizeof(setForcePstateParams)));
        CHECK_RC(VerifyPStateSwitch(m_vPStateMasks[i]));
    }
    CHECK_RC(AllowElpgOnDuringPstateChange(LW_FALSE));

    Printf(Tee::PriHigh,"***Completed phase 2***\n");

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
RC ElpgPStateSwitchTest::GetPStateInfo(LwU32 nPState)
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

    vVoltDomBuff.assign(sizeof(LW2080_CTRL_PERF_VOLT_DOM_INFO) *
                               m_getPStateInfoParams.perfVoltDomInfoListSize, 0);

    m_getPStateInfoParams.perfVoltDomInfoList = LW_PTR_TO_LwP64(&vVoltDomBuff[0]);

    // set the Clock / voltage domain in each list
    for (UINT32 BitNum = 0; BitNum < 32; BitNum++)
    {
        UINT32 BitMask = 1<<BitNum;
        if (m_getAllPStatesInfoParams.perfClkDomains & BitMask)
        {
            LW2080_CTRL_PERF_CLK_DOM_INFO * pClkInfoList =
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

//! \brief VerifyPStateSwitch function.
//!
//! \param nTargetPstate Mask of the target p-state
//! \param nFallback Specifies what happens when setting to target p-state fails
//! \return OK if the verification passed, specific RC if failed
//------------------------------------------------------------------------------
RC ElpgPStateSwitchTest::VerifyPStateSwitch(LwU32 nTargetPstate)
{
    RC rc;
    LwRmPtr pLwRm;
    LwU32 nFallback = LW2080_CTRL_PERF_PSTATE_FALLBACK_RETURN_ERROR;

    if (rc != OK)
    {
        Printf(Tee::PriHigh, "%d:Forced trasition to P-State P%d (Mask 0x%08x)failed\n",
               __LINE__, GetLogBase2(nTargetPstate), (UINT32)(nTargetPstate));
        return rc;
    }

    // Print the clk freqs after transition
    Printf(Tee::PriHigh, "Clk frequencies after transition to P%d:\n",
           GetLogBase2(nTargetPstate));
    CHECK_RC(PrintAllClockFreq());

    // Verify the freq of the clks
    CHECK_RC(VerifyPStateClockFreq(nTargetPstate));

    // Verify whether the switch to the desired p-state actually took place
    m_getLwrrentPStateParams.lwrrPstate = LW2080_CTRL_PERF_PSTATES_UNDEFINED;

    CHECK_RC(pLwRm->Control(m_hBasicSubDev,
                            LW2080_CTRL_CMD_PERF_GET_LWRRENT_PSTATE,
                            &m_getLwrrentPStateParams,
                            sizeof(m_getLwrrentPStateParams)));

    if (m_getLwrrentPStateParams.lwrrPstate != nTargetPstate)
    {
        Printf(Tee::PriHigh, "Setting to P-State P%d (Mask 0x%08x) failed !!\n",
               GetLogBase2(nTargetPstate), (UINT32)(nTargetPstate));

        Printf(Tee::PriHigh, "Target P-State : P%d (Mask 0x%08x)\n"
                             "Achieved P-State : P%d (Mask 0x%08x)\n",
                             GetLogBase2(nTargetPstate), (UINT32)(nTargetPstate),
                             GetLogBase2(m_getLwrrentPStateParams.lwrrPstate),
                             (UINT32)(m_getLwrrentPStateParams.lwrrPstate));

        if (LW2080_CTRL_PERF_PSTATE_FALLBACK_RETURN_ERROR == nFallback)
        {
            return RC::CANNOT_SET_STATE;
        }
    }
    else
    {
        Printf(Tee::PriHigh, "Setting to P-State P%d (Mask 0x%08x) successful !!\n",
               GetLogBase2(nTargetPstate), (UINT32)(nTargetPstate));
    }

    return rc;
}

//! \brief GetLogBase2 function.
//!
//! Returns the log to the base 2 of the given number
//!
//! \param nNum Number whose logarithm is to be computed
//! \return Base 2 Logarithm of the given number
//------------------------------------------------------------------------------
UINT32 ElpgPStateSwitchTest::GetLogBase2(LwU32 nNum)
{
    return (UINT32)ceil(log((double)nNum)/log(2.0));
}

//! \brief GetClockState function.
//!
//! This function is used to read the current clock state for all the clocks
//! that are programmable and stores the read data in m_pClkInfos.
//!
//! \return OK if clock domains are read properly, specific RC ERROR if failed.
//------------------------------------------------------------------------------
RC ElpgPStateSwitchTest::GetClockState()
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
RC ElpgPStateSwitchTest::PrintAllClockFreq()
{
    RC rc;

    // hook up clk info table
    CHECK_RC(GetClockState());

    Printf(Tee::PriHigh, "\nPresent Clock State\n");

    for (UINT32 iLoopVar = 0; iLoopVar < m_numClkInfos; iLoopVar++)
    {
        const char * const domainName = m_ClkUtil.GetClkDomainName(m_pClkInfos[iLoopVar].clkDomain);
        const char * const sourceName = m_ClkUtil.GetClkSourceName(m_pClkInfos[iLoopVar].clkSource);

        Printf(Tee::PriHigh, "Domain : %13s,     ", domainName);
        Printf(Tee::PriHigh, "Source : %10s,     ", sourceName);
        Printf(Tee::PriHigh, "Actual Freq : %7d KHz,    ", (int)m_pClkInfos[iLoopVar].actualFreq);
        Printf(Tee::PriHigh, "Flag : 0x%x\n", (UINT32)m_pClkInfos[iLoopVar].flags);
    }

    Printf(Tee::PriHigh, "\n");

    return rc;
}

//! \brief VerifyPStateClockFreq function.
//!
//! Verifies the current clock frequencies against the clock frequencies for
//! the supplied p-state - the idea is to ensure that the p-state transition
//! has been able to change all the clock frequencies correctly
//!
//! \param nPState The P-state flag for which details are to be printed
//! \return OK if the verification passed, specific RC ERROR if failed
//------------------------------------------------------------------------------
RC ElpgPStateSwitchTest::VerifyPStateClockFreq(LwU32 nPState)
{
    RC rc;
    LwRmPtr pLwRm;
    LwU32 nIdealClkFreq;
    LwU32 nActualClkFreq;
    map<LwU32, map<LwU32, LwU32> >::iterator itPStateInfo;
    map<LwU32, LwU32>::iterator itPStateClkInfo;

    Printf(Tee::PriHigh, "\nReading the set frequency for clocks for current P-State\n");
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

        // hook up clk info table to get the back the recently set values
        m_getInfoParams.flags = 0;
        m_getInfoParams.clkInfoListSize = 1;
        m_getInfoParams.clkInfoList = (LwP64)m_pSingleClkInfo;

        // Get current clock info
        CHECK_RC(pLwRm->Control(m_hBasicSubDev,
                 LW2080_CTRL_CMD_CLK_GET_INFO,
                 &m_getInfoParams,
                 sizeof (m_getInfoParams)));
        Printf(Tee::PriHigh, "Clk Name : %13s,    ",
               m_ClkUtil.GetClkDomainName(itPStateClkInfo->first));

        Printf(Tee::PriHigh,"Source : %10s,    ",
               m_ClkUtil.GetClkSourceName(m_pSingleClkInfo[0].clkSource));

        Printf(Tee::PriHigh,"Flag : 0x%x,    ",
               (UINT32)m_pSingleClkInfo[0].flags);

        nActualClkFreq = m_pSingleClkInfo[0].actualFreq;

        Printf(Tee::PriHigh, "Expected Freq : %7d KHz, ",
               (UINT32)(nIdealClkFreq));

        Printf(Tee::PriHigh, "Actual Freq : %7d KHz, ", (UINT32)(nActualClkFreq));

        double fPercentDiff = ((double)nActualClkFreq - (double)nIdealClkFreq) *
                              100.0 / (double)(nIdealClkFreq);

        Printf(Tee::PriHigh, "Difference : %+5.1f %%\n", fPercentDiff);

        // Clk freq must match EXACTLY
        MASSERT(abs(fPercentDiff) <= FREQ_TOLERANCE_100X);
    }

    return rc;
}

//! \brief Allow p-state switch during an induced ELPG event if flag is true
//!
//! \param flag Sets/unsets the pdb property allowing p-state switching inside
//!             ELPG event.
//----------------------------------------------------------------------------
RC ElpgPStateSwitchTest::AllowPStateChangeInElpgSequence(LwBool flag)
{
    RC rc;
    LwU32 status;
    LwRmPtr pLwRm;
    LW208F_CTRL_MC_SET_ALLOW_PSTATE_TRANSITION_PARAMS allowPstateParams = {0};

    // Enable pstate transition within Elpg event
    allowPstateParams.allowPstateTransition = flag;
    status = LwRmControl(pLwRm->GetClientHandle(), m_hSubDevDiag,
                         LW208F_CTRL_CMD_MC_SET_ALLOW_PSTATE_TRANSITION,
                         &allowPstateParams,
                         sizeof(allowPstateParams));

    if (status != LW_OK)
    {
        Printf(Tee::PriHigh, "%d: %s: LW208F_CTRL_CMD_MC_SET_ALLOW_PSTATE_TRANSITION call failed\n",
               __LINE__, __FUNCTION__);
        rc = RmApiStatusToModsRC(status);
        CHECK_RC(rc);
    }

    return rc;
}

//! \brief Allow an ELPG event during p-state switch if flag is true
//!
//! \param flag Sets/unsets the pdb property allowing ELPG ON inside p-state change
//----------------------------------------------------------------------------
RC ElpgPStateSwitchTest::AllowElpgOnDuringPstateChange(LwBool flag)
{
    RC rc;
    LwU32 status;
    LwRmPtr pLwRm;
    LW208F_CTRL_PERF_ALLOW_ELPG_ON_PARAMS allowElpgParams = {0};

    // Enable ELPG ON during p-state change
    allowElpgParams.allowElpgEvent = flag;
    status = LwRmControl(pLwRm->GetClientHandle(), m_hSubDevDiag,
                         LW208F_CTRL_CMD_PERF_ALLOW_ELPG_ON,
                         &allowElpgParams,
                         sizeof(allowElpgParams));

    if (status != LW_OK)
    {
        Printf(Tee::PriHigh, "%d: %s: LW208F_CTRL_CMD_PERF_ALLOW_ELPG_ON call failed\n",
                __LINE__, __FUNCTION__);
        rc = RmApiStatusToModsRC(status);
        CHECK_RC(rc);
    }

    return rc;
}

//! \brief Query for a parameter if set=false, else Set the passed parameter
//!
//! \sa IsSupported, Setup, CleanUp
//----------------------------------------------------------------------------
RC ElpgPStateSwitchTest::QueryPowerGatingParameter(UINT32 param, UINT32 paramExt,
                                                   UINT32 *paramVal,  bool set)
{
    RC           rc;
    LwRmPtr      pLwRm;

    LW2080_CTRL_POWERGATING_PARAMETER powerGatingParam = {0};
    LW2080_CTRL_MC_SET_POWERGATING_PARAMETER_PARAMS setPowerGatingParam = {0};

    powerGatingParam.parameterExtended = paramExt;
    powerGatingParam.parameter = param;

    if(!set)
    {
        LW2080_CTRL_MC_QUERY_POWERGATING_PARAMETER_PARAMS queryPowerGatingParam = {0};

        queryPowerGatingParam.listSize = 1;
        queryPowerGatingParam.parameterList = (LwP64)&powerGatingParam;

        rc = pLwRm->ControlBySubdevice(GetBoundGpuSubdevice(),
                                       LW2080_CTRL_CMD_MC_QUERY_POWERGATING_PARAMETER,
                                       &queryPowerGatingParam,
                                       sizeof(queryPowerGatingParam));

        if (rc == RC::LWRM_NOT_SUPPORTED)
        {
            Printf(Tee::PriHigh,"ElpgPStateSwitchTest::QueryPowerGatingParameter : Query not done\n");
            return rc;
        }
        else
        {
            *paramVal = powerGatingParam.parameterValue;
        }
    }
    else
    {
        powerGatingParam.parameterExtended = paramExt;
        powerGatingParam.parameter = param;
        powerGatingParam.parameterValue = *paramVal;
        setPowerGatingParam.listSize = 1;
        setPowerGatingParam.parameterList = (LwP64)&powerGatingParam;

        rc = pLwRm->Control(m_hBasicSubDev,
                            LW2080_CTRL_CMD_MC_QUERY_POWERGATING_PARAMETER,
                            &setPowerGatingParam,
                            sizeof(setPowerGatingParam));

        if (rc == RC::LWRM_NOT_SUPPORTED)
        {
            Printf(Tee::PriHigh,"ElpgPStateSwitchTest::QueryPowerGatingParameter : Set not done\n");
        }
    }
    return rc;
}

//------------------------------------------------------------------------------
// JS Linkage
//------------------------------------------------------------------------------

//! \brief Linkage to JavaScript
//!
//! Setup the JS hierarchy to match the C++ one.  Don't worry about the details
//! here, you probably don't care.  :-)
//------------------------------------------------------------------------------
JS_CLASS_INHERIT(ElpgPStateSwitchTest, RmTest,
    "TODORMT - Write a good short description here");

