/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2012-2020 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

//! \file rmt_UniversalClockTest.cpp
//! \Universal RM-Test to exercise all testcases related to clocks.
//! The test will first parse the CTP file to get the clock test data
//! & then it will run all test on provided data set
//!

#include <math.h>
#include <ctype.h>
#include <map>
#include <string>
#include "gpu/tests/rmtest.h"
#include "core/include/jscript.h"
#include "core/include/platform.h"
#include "gpu/tests/rm/utility/clk/clkbase.h"
#include "lwRmApi.h"
#include "core/include/lwrm.h"
#include "core/include/utility.h"
#include "core/include/gpu.h"
#include "gpu/include/gpusbdev.h"
#include "ctrl/ctrl2080.h"
#include "ctrl/ctrl2080/ctrl2080perf.h"
#include "ctrl/ctrl2080/ctrl2080clk.h"
#include "ctrl/ctrl2080/ctrl2080fb.h"
#include "ctrl/ctrl2080/ctrl2080bus.h"
#include "ctrl/ctrl2080/ctrl2080volt.h"
#include "ctrl/ctrl2080/ctrl2080boardobj.h"

#include "gpu/tests/rm/clk/uct/uctexception.h"
#include "gpu/tests/rm/clk/uct/uctfilereader.h"
#include "gpu/tests/rm/clk/uct/uctresult.h"
#include "gpu/tests/rm/clk/uct/uctchip.h"
#include "gpu/tests/rm/clk/uct/uctvflwrve.h"
#include "gpu/tests/rm/clk/uct/ucttrialspec.h"

#include "core/include/memcheck.h"

//
// We use P0 for programming pseudo-p-states because the RMAPI PERF_SET_PSTATE_INFO
// supports overclocking only on P0.
//
#define PSEUDOPSTATE_INDEX 0
#define CLK_IDX_ILWALID    (0xFFFFFFFF)

using namespace uct;

// Global variable for vflwrve clkDomains
LW2080_CTRL_CLK_CLK_DOMAINS_INFO clkDomainsInfo;

// Global variable for index of master clkDomain,if specified in vflwrve test.
UINT32 clkDomailwfLwrveIndex;

// Global variable for whether p-state is supported or not
bool m_PstateSupported;

//Global variable for storing programmable domains info
LW2080_CTRL_CLK_CLK_PROGS_INFO clkProgsInfo;

// Global variable for VF relations table info
LW2080_CTRL_CLK_CLK_VF_RELS_INFO clkVfRelsInfo;

// Global variable for clock enumeration table info
LW2080_CTRL_CLK_CLK_ENUMS_INFO clkEnumsInfo;

// Global variable for volt rails dynamic data
LW2080_CTRL_VOLT_VOLT_RAILS_STATUS_PARAMS voltRailsStatus;

// Structures to store target volt change requests
typedef struct CLK_UCT_VOLT_INFO
{
    UINT32     voltDomain;
    UINT32     voltUv;
}CLK_UCT_VOLT_INFO;

typedef struct CLK_UCT_VOLT_LIST
{
    CLK_UCT_VOLT_INFO     voltInfo[LW2080_CTRL_VOLT_VOLT_DOMAIN_MAX_ENTRIES];
}CLK_UCT_VOLT_LIST;

//!
//! \Class UniversalClockTest
//!
//! Implements the universal clock test as indicated by wiki
//! https://wiki.lwpu.com/engwiki/index.php/Resman/Resman_Components/Clocks/UniversalClockTest
//!

class UniversalClockTest : public RmTest
{
public:
    UniversalClockTest();
    virtual ~UniversalClockTest();

    virtual string IsTestSupported();

    virtual RC Setup();
    virtual RC Run();
    virtual RC Cleanup();

    SETGET_PROP(EntryCtpFilename, string);
    SETGET_PROP(Exclude, string);
    SETGET_PROP(DryRun, bool);
    SETGET_PROP(ExitOnFail, bool);

private:
    //! \brief          GPU subdevice handle for calling RM API
    GpuSubdevice       *m_pSubdevice;

    //! \brief          Client handle for calling RM API
    LwRm::Handle        m_hClient;

    //!
    //! \brief          Clock Test Profile (ctp) Filename
    //!
    //! \details        'rmtest.js' passes this argument from the -ctp
    //!                 command-line parameter.
    //!
    string              m_EntryCtpFilename;

    //!
    //! \brief          Clock Domains to Exclude
    //!
    //! \details        'rmtest.js' passes this argument from the -exclude_clock
    //!                 command-line parameter.
    //!
    string              m_Exclude;

    //! Cache RmApiField in variable for further use.
    TrialSpec::RmapiField::Mode m_rmapiFieldMode;

    //!
    //! \brief          Dry Run Flag
    //!
    //! \details        'rmtest.js' sets this member to 'true' if the -dryrun
    //!                 command-line option was used.  It is also set to true
    //!                 if a 'dryrun' directive was sourced from a ctp file.
    //!
    //!                 When this flag is set, no trials are run.  However, the
    //!                 ctp files are source and checked for syntax errors.
    //!
    bool                m_DryRun;

    //!
    //! \brief          Exit on First Failure flag
    //!
    //! \details        'rmtest.js' sets this member to 'true' if the -exitOnFail
    //!                 command-line option was used.
    bool                m_ExitOnFail;

    //! \brief          Store the status from the IsTestSupported and Setup calls
    ExceptionList       m_initStatus;

    //! \brief          VBIOS P-States
    VbiosPStateArray    m_VbiosPStateMap;

    //! \brief          Trial specifications to Run
    TrialSpec::List     m_TrialList;

    //!
    //! \brief          Used by GET_PSTATES_INFO
    //!
    //! \details        This member is a global copy because other API calls
    //!                 rely on information set here.
    //!
    //! \todo           Make this data structure local.
    //!
    LW2080_CTRL_PERF_GET_PSTATES_INFO_PARAMS m_PStatesInfoParams;

    //!
    //! \brief          FMODEL Flag
    //!
    //! \details        Denotes whether or not clock frequencies will be measured with HW clock counters.
    //!
    bool                m_ProgrammingSupported;

    //!
    //! \brief          Has UniversalClockTest::ModifyPState been called?
    //! \see            UniversalClockTest::Cleanup
    //! \see            UniversalClockTest::ModifyPState
    //!
    //! \details        Used by Cleanup to determine if the p-state should be restored.
    //!
    bool                m_ModifyPState;

    //! \brief          Cached all static information for all the supported NAFLL devices
    LW2080_CTRL_CLK_NAFLL_DEVICES_INFO_PARAMS    m_ClkNafllDevicesInfoParams;

    //! \brief          Cached LWVDD index used across in UCT code
    UINT32              m_LwvddIdx;

    //!
    //! \brief          Cached MSVDD index used across in UCT code
    //!
    //! \note           Applicable only for PS 4.0 and later.
    //!
    UINT32              m_MsvddIdx;

    //! \brief          Cached supported voltRails mask
    UINT32              m_VoltRailsMask;

    ExceptionList ReadEntryCtpFile();
    ExceptionList CheckFeatures();
    ExceptionList GetSupportedPStates();
    Exception GetPStateInfo(VbiosPState::Index pstateIndex, VbiosPState &pstate);
    Exception SwitchToPState(VbiosPState::Index nPState = PSEUDOPSTATE_INDEX);
    Exception VerifyPStateSwitch(VbiosPState::Index nPState = PSEUDOPSTATE_INDEX);
    ExceptionList VerifyModifyPState(const FullPState &targetPState);
    Exception ModifyPState(const FullPState &pstate);
    Exception SetVfPoints(const FullPState &pstate);
    UINT32 GetNafllDvcoMinReachedClkDomMask();
    Exception CheckMinFrequency(Result &result);
    Exception SetClocks(const FullPState &pstate);
    Exception SetInfos(const FullPState &pstate);
    Exception GetFreqDomainStatus();
    Exception SetChSeq(const FullPState &pstate);
    Exception ConfigClocks(Result &result);
    Exception GetMeasuredFreq(FreqDomain freqDomain, UINT32 *freqKHz);
    ExceptionList GetClockProgramming(Result &result, bool bCheckMeasured);
    UINT32 clkDomainToNafllIdMask(UINT32 clkDomain);
    void GetVoltRailList(LW2080_CTRL_PERF_CHANGE_SEQ_CHANGE_INPUT *pChangeInput, CLK_UCT_VOLT_LIST *pVoltList);
    UINT32 GetClockHalIdxFromDomain(UINT32 clkDomain);
};

//!
//! \brief      UniversalClockTest constructor.
//! \see        Setup
//!
//! \details    Functionality mostly lies in Setup().
//!
UniversalClockTest::UniversalClockTest()
{
    SetName("UniversalClockTest");

    // Get device handle
    m_hClient = 0;

    m_DryRun = false;
    m_ExitOnFail = false;
    m_ProgrammingSupported = true;
    m_ModifyPState = false;
}

//!
//! \brief      UniversalClockTest destructor
//! \see        Cleanup
//!
//! \details    Does nothing.  Functionality lies in Cleanup().
//!
UniversalClockTest::~UniversalClockTest()
{
}

//!
//! \brief      Is this test supported?
//!
//! \retval     RUN_RMTEST_TRUE     Yes, it is supported.
//! \return     Reason why the test is not supported.
//!
//! \todo       Read and parse the CTP file from this function instead of Setup
//!             so that we can consider the 'disable', 'enable', and 'ramtype'
//!             fields in the CTP file.
//!
string UniversalClockTest::IsTestSupported()
{
    RC rc;
    LwRmPtr pLwRm;

    m_pSubdevice = GetBoundGpuSubdevice();
    Gpu::LwDeviceId chip = m_pSubdevice->DeviceId();

    if (Platform::GetSimulationMode() == Platform::Fmodel)
    {
        if (!IsGM20XorBetter(chip))
        {
            // For the FMODEL platform, UCT has only been regularly tested on GM20X_and_later.
            return "Universal Clock Test is not supported for F-Model due to incomplete register modeling.";
        }
        else
        {
            // For the FMODEL platform, UCT should not try to measure clock frequencies using HW clock counters.
            m_ProgrammingSupported = false;
        }
    }

    if (!IsKEPLERorBetter(chip))
    {
        return "Universal Clock Test is supported only for Kepler+";
    }

    //
    // NOTE - GetSupportedPStates makes use of voltage indices and clkDomainsInfo,
    // hence, the sequence here of initializing the voltage domain indices first,
    // calling CLK_DOMAINS_GET_INFO next and then calling GetSupportedPStates needs
    // to stay as-is.
    //

    // Initialize voltage domain indices
    m_LwvddIdx = BIT_IDX_32(CLK_UCT_VOLT_DOMAIN_LOGIC);
    m_MsvddIdx = BIT_IDX_32(CLK_UCT_VOLT_DOMAIN_MSVDD);

    // Cache clock domains information which is later used for constructing vflwrve based test profile.
    rc = pLwRm->ControlBySubdevice(m_pSubdevice, LW2080_CTRL_CMD_CLK_CLK_DOMAINS_GET_INFO,
                &clkDomainsInfo, sizeof(clkDomainsInfo));
    if(rc!=OK)
    {
        return "Error in LW2080_CTRL_CMD_CLK_CLK_DOMAINS_GET_INFO in function IsTestSupported()";
    }

    // Get vbios pstates into m_VbiosPStateMap.
    m_initStatus.push(GetSupportedPStates());

    //
    // Source the CTP files starting with the one specified on the command line.
    // If there is an error, 'dryrun' is specified, or there are no trials defined,
    // status contains one or more exceptions indicating this.
    // Otherwise m_TrialList contains the trials to be exelwted.
    //
    Printf(Tee::PriHigh, "\n======== Sourcing CTP (Clock Test Profile) files... (%s)\n\n",
        rmt::String::function(__PRETTY_FUNCTION__).c_str());
    Printf(Tee::PriHigh, "%s: Current working directory\n",
        rmt::String::getcwd().defaultTo("[unknown]").c_str());

    // Cache programmable clock domains which is later used for constructing vflwrve based test profile.
    rc = pLwRm->ControlBySubdevice(
             m_pSubdevice,
             LW2080_CTRL_CMD_CLK_CLK_PROGS_GET_INFO,
             &clkProgsInfo,
             sizeof(clkProgsInfo));
    if(rc!=OK)
    {
        return "Error in LW2080_CTRL_CMD_CLK_CLK_PROGS_GET_INFO in function IsTestSupported()";
    }

    // Cache VF relation table info which is later used for constructing vflwrve based test profile.
    rc = pLwRm->ControlBySubdevice(
             m_pSubdevice,
             LW2080_CTRL_CMD_CLK_CLK_VF_RELS_GET_INFO,
             &clkVfRelsInfo,
             sizeof(clkVfRelsInfo));
    if(rc!=OK)
    {
        return "Error in LW2080_CTRL_CMD_CLK_CLK_VF_RELS_GET_INFO in function IsTestSupported()";
    }

    // Cache enumeration table info which is later used for constructing vflwrve based test profile.
    rc = pLwRm->ControlBySubdevice(
             m_pSubdevice,
             LW2080_CTRL_CMD_CLK_CLK_ENUMS_GET_INFO,
             &clkEnumsInfo,
             sizeof(clkEnumsInfo));
    if(rc!=OK)
    {
        return "Error in LW2080_CTRL_CMD_CLK_CLK_ENUMS_GET_INFO in function IsTestSupported()";
    }

    //
    // Set the mask of volt rails to get the dynamic data for.
    // We cache m_VoltRailsMask in @ref GetSupportedPStates where we get the
    // mask using @ref LW2080_CTRL_CMD_VOLT_VOLT_RAILS_GET_INFO.
    //
    voltRailsStatus.super.objMask.super.pData[0] = m_VoltRailsMask;

    // Cache volt rails dynamic data  which is later used for constructing vflwrve based test profile.
    rc = pLwRm->ControlBySubdevice(
             m_pSubdevice,
             LW2080_CTRL_CMD_VOLT_VOLT_RAILS_GET_STATUS,
             &voltRailsStatus,
             sizeof(voltRailsStatus));
    if(rc!=OK)
    {
        return "Error in LW2080_CTRL_CMD_VOLT_VOLT_RAILS_GET_STATUS in function IsTestSupported()";
    }

    m_initStatus.push(ReadEntryCtpFile());

    // If the parsing was ok but there are no trials, the test is skipped.
    // Otherwise Setup will dump the exceptions and fail the test
    if (m_initStatus.isOkay() && m_TrialList.empty())
    {
        return "There are no trials specified";
    }

    Printf(Tee::PriHigh, "Universal Clock Test: https://wiki.lwpu.com/engwiki/index.php/Resman/Resman_Components/Clocks/UniversalClockTest\n");

    return RUN_RMTEST_TRUE;
}

//!
//! \brief This function check/setup features required by the test and does
//!        initial sanity check based on what is parsed from ctp groups
//!
//! \return OK if everything went fine.
//------------------------------------------------------------------------------
RC UniversalClockTest::Setup()
{
    RC rc;
    LwRmPtr pLwRm;

    // Ask our base class(es) to setup internal state based on JS settings
    CHECK_RC(InitFromJs());

    // See if we have a -dryrun flag on the command line.
    if (m_DryRun)
        Printf(Tee::PriHigh, "\n== -dryrun command-line option prevents trials from being exelwted.  (%s)\n\n",
            rmt::String::function(__PRETTY_FUNCTION__).c_str());

    // Allocate the RM client.
    CHECK_RC(RmApiStatusToModsRC(LwRmAllocRoot((LwU32*) &m_hClient)));

    // Flag that there has been a 'dryrun' field in a CTP file.
    if (m_initStatus.mostSerious() == Exception::DryRun)
        m_DryRun = true;

    // Check if we need Vbiosp0 and if so does it exist
    TrialSpec::List::const_iterator pTrial;
    bool bRequireP0 = false;

    for (pTrial = ((const TrialSpec::List &) m_TrialList).begin();
         pTrial!= ((const TrialSpec::List &) m_TrialList).end(); ++pTrial)
    {
        if(pTrial->rmapi.mode == TrialSpec::RmapiField::Perf)
            bRequireP0 = true;
    }

    if (bRequireP0 && !m_VbiosPStateMap.isValid(0))
        m_initStatus.push(Exception("VBIOS P0 p-state is not defined but this trial requires it.\n",
            __PRETTY_FUNCTION__, Exception::Fatal));

    // Make sure this GPU, VBIOS, etc. has what we need to run the trials.
    if (!m_initStatus.isSerious())
        m_initStatus.push(CheckFeatures());

    memset(&m_ClkNafllDevicesInfoParams, 0, sizeof(m_ClkNafllDevicesInfoParams));

    rc = RmApiStatusToModsRC(pLwRm->ControlBySubdevice(
                m_pSubdevice,
                LW2080_CTRL_CMD_CLK_NAFLL_DEVICES_GET_INFO,
               &m_ClkNafllDevicesInfoParams,
                sizeof(m_ClkNafllDevicesInfoParams)));

    if (rc != OK)
    {
        Printf(Tee::PriHigh, "\n======== NAFLL not supported. (%s)\n",
            rmt::String::function(__PRETTY_FUNCTION__).c_str());
    }

    // Dump out any errors from the initialization steps (IsTestSupported + Setup)
    Printf(Tee::PriHigh, "\n======== Done sourcing CTP (Clock Test Profile) files. (%s)\n",
        rmt::String::function(__PRETTY_FUNCTION__).c_str());
    Printf(Tee::PriHigh, "%s\n\n", m_initStatus.string().c_str());

    // Indicate the error (if any) in the result code.
    return m_initStatus.modsRC();
}

//!
//! \brief      Run the Test
//!
//! \details    This calls the setting and verifying the PStates specified
//!             number of times.  Everytime a PState is set according to the
//!             test data provided and then the action is verfied.
//!
//! \retval     OK if test passed
//! \return     Specific RC if failed
//!
RC UniversalClockTest::Run()
{
    TrialSpec::List::const_iterator pTrial;
    RC mostSeriousRC;
    RC rc;
    Exception::Level mostSeriousLevel = Exception::Null;
    LwRmPtr pLwRm;
    Result r;

    // Iterate over every trial spec read from the Clock Test Profile (CTP) file(s).
    for (pTrial = ((const TrialSpec::List &) m_TrialList).begin();
         pTrial!= ((const TrialSpec::List &) m_TrialList).end(); ++pTrial)
    {
        Printf(Tee::PriHigh, "\n\n======== Starting trial specification %s  (%s)\n",
            rmt::String::function(__PRETTY_FUNCTION__).c_str(),
            pTrial->name.c_str());

        Printf(Tee::PriHigh, "%s\n", pTrial->debugString().c_str());

        // Iterate through the p-states defined in this trial.
        for (TrialSpec::Iterator pPStateRef = &*pTrial; pPStateRef; ++pPStateRef)
        {
            Result result;
            ExceptionList pstateStatus;     // For this iteration only
            UINT32 clkDomainDvcoMinMask;

            Printf(Tee::PriHigh, "\n\n==== Starting iteration  index=%llu  remaining=%llu  (%s)\n",
                pPStateRef.iterationIndex(),
                pPStateRef.iterationRemaining(),
                rmt::String::function(__PRETTY_FUNCTION__).c_str());

            // Get the generated fully-defined p-state.
            pstateStatus.push(pPStateRef.fullPState(result.target));
            Printf(Tee::PriHigh, "== About to program each domain:  (Khz/uV)\n%s\n",
                result.reportString(true).c_str());

            //
            // If this is in dry run mode then we only want to print the pstate generated.
            // Otherwwise, we now have a pstate that needs to be tested.
            //
            if(!m_DryRun && pstateStatus.isOkay())
            {
                bool bConfigClock = false;
                bool bCheckSource = true;
                m_rmapiFieldMode = pTrial->rmapi.mode;
                switch (pTrial->rmapi.mode)
                {

                // Use the perf (p-state) interface
                case TrialSpec::RmapiField::Perf:
                    bCheckSource = false;

                    if (result.target.bVfLwrveBlock)
                    {
                        pstateStatus.push(CheckMinFrequency(result));

                        pstateStatus.push(SetVfPoints(result.target));

                        pstateStatus.push(SwitchToPState(result.target.base));

                        pstateStatus.push(VerifyPStateSwitch(result.target.base));
                        break;
                    }
                    else
                    {
                        // The p-state reference specifies a pseudo-p-state.
                        if (!result.target.matches(m_VbiosPStateMap))
                        {
                            //
                            // Modify P0 to match the fully-defined p-state to test.
                            // We always use P0 because RMAPI does not permit others to be used.
                            //
                            pstateStatus.push(ModifyPState(result.target));

                            // Verify that P0 was properly modified.
                            pstateStatus.push(VerifyModifyPState(result.target));

                            // Switch to the modified P0.
                            pstateStatus.push(SwitchToPState());

                            // Verify p-state switch
                            pstateStatus.push(VerifyPStateSwitch());
                        }

                        // The p-state reference specifies a VBIOS p-state.
                        else
                        {
                            // Since we use P0 for the other mode, restore it.
                            if (result.target.base == PSEUDOPSTATE_INDEX)
                                pstateStatus.push(ModifyPState(m_VbiosPStateMap[PSEUDOPSTATE_INDEX]));

                            // Switch to the specified p-state.
                            pstateStatus.push(SwitchToPState(result.target.base));

                            // Verify p-state switch
                            pstateStatus.push(VerifyPStateSwitch(result.target.base));
                        }
                    }
                    break;

                // Use the clocks interface
                case TrialSpec::RmapiField::Clk:
                    // Program the clocks directly.
                    pstateStatus.push(SetClocks(result.target));
                    break;

                // Use the pmu interface
                case TrialSpec::RmapiField::Pmu:
                    // Program the clocks directly.
                    pstateStatus.push(SetInfos(result.target));
                    break;

                // Use the VF Injection interface
                case TrialSpec::RmapiField::ChSeq:

                    if (result.target.bVfLwrveBlock)
                    {
                        pstateStatus.push(CheckMinFrequency(result));
                        // Setting LWVDD volt to min required voltage by all the clkDomains
                        if (result.target.volt[m_LwvddIdx] < m_VbiosPStateMap.initPState.volt[m_LwvddIdx])
                        {
                            result.target.volt[m_LwvddIdx] = m_VbiosPStateMap.initPState.volt[m_LwvddIdx];
                        }

                        //
                        // If PS 4.0 is supported, also set MSVDD volt to min required voltage by
                        // all the clkDomains.
                        //
                        if (clkDomainsInfo.version == LW2080_CTRL_CLK_CLK_DOMAIN_VERSION_40)
                        {
                            if (result.target.volt[m_MsvddIdx] < m_VbiosPStateMap.initPState.volt[m_MsvddIdx])
                            {
                                result.target.volt[m_MsvddIdx] = m_VbiosPStateMap.initPState.volt[m_MsvddIdx];
                            }
                        }
                    }

                    // Program via the PMU's change sequencer.
                    pstateStatus.push(SetChSeq(result.target));

                    // Get the status of each frequency domain.
                    pstateStatus.push(GetFreqDomainStatus());
                    break;

                // Use the clock config interface
                case TrialSpec::RmapiField::Config:
                    // Config the clocks
                    pstateStatus.push(ConfigClocks(result));
                    bConfigClock = true;
                    break;
                }

                if (pTrial->rmapi.mode != TrialSpec::RmapiField::Config)
                {
                    // Verify clocks correctly programmed and measured
                    pstateStatus.push(GetClockProgramming(result, m_ProgrammingSupported));
                }

                // Stores the mask of all the NAFLL clkDomains whose DVCO min have reached.
                clkDomainDvcoMinMask = GetNafllDvcoMinReachedClkDomMask();

                // Check to see if current clock programming matches that indicated by result.target.
                pstateStatus.push(result.checkResult(pTrial->tolerance.setting,
                            m_ProgrammingSupported && (!bConfigClock),
                            bCheckSource, clkDomainDvcoMinMask));

                //No errors.
                if (pstateStatus.isOkay())
                {
                    Printf(Tee::PriHigh, "%sVerified \"%s\" from \"%s\" trial.  Okay.  (%s)\n",
                        result.target.start.locationString().c_str(),
                        result.target.name.c_str(), pTrial->name.c_str(),
                        rmt::String::function(__PRETTY_FUNCTION__).c_str());
                }
            }

            // Report results unless this was just a dry run.
            if (!m_DryRun)
                Printf(Tee::PriHigh, "== State of each domain after programming:  (Khz/uV)\n%s\n",
                    result.reportString(false).c_str());

            // One or more exceptions.
            if (!pstateStatus.isOkay())
            {
                Printf(Tee::PriHigh, "%sFailure %s \"%s\" from \"%s\" trial.  (%s)\n",
                    result.target.start.locationString().c_str(),
                    (m_DryRun? "evaluating" : "testing"),
                    result.target.name.c_str(), pTrial->name.c_str(),
                    rmt::String::function(__PRETTY_FUNCTION__).c_str());
                Printf(Tee::PriHigh, "%s\n",
                    pstateStatus.string().c_str());

                // This is more serious than any prior exception.
                if (pstateStatus.isSerious(mostSeriousLevel))
                {
                    mostSeriousLevel = pstateStatus.mostSerious();
                    mostSeriousRC    = pstateStatus.modsRC();
                }

                // Return the failure if user indicated they do not want the test to continue
                if (m_ExitOnFail)
                {
                    Printf(Tee::PriHigh, "Returning on first failure (-exitOnFail specified)\n");
                    return mostSeriousRC;
                }
            }

            // Communicate to the organic life-form.
            Printf(Tee::PriHigh, "\n\n==== Finished p-state reference iteration  index=%llu  remaining=%llu  (%s)\n",
                pPStateRef.iterationIndex(),
                pPStateRef.iterationRemaining(),
                rmt::String::function(__PRETTY_FUNCTION__).c_str());
        }

        // Communicate to the organic life-form.
        Printf(Tee::PriHigh, "\n\n======== Finished trial specification %s  (%s)\n",
            rmt::String::function(__PRETTY_FUNCTION__).c_str(),
            pTrial->name.c_str());
    }

    // Speak on the way out.
    Printf(Tee::PriHigh, "\n======== Done.  RC=%u  (%s)\n\n", (unsigned) mostSeriousRC,
        rmt::String::function(__PRETTY_FUNCTION__).c_str());

    // Both are okay or neither is okay.
    MASSERT((mostSeriousLevel == Exception::Null) == (mostSeriousRC == OK));

    // Done.
    return mostSeriousRC;
}

//! \brief      Clean up after test
RC UniversalClockTest::Cleanup()
{
    ExceptionList status;

    Printf(Tee::PriHigh, "\n======== Cleaning  (%s)\n", rmt::String::function(__PRETTY_FUNCTION__).c_str());

    // Restore PSTATE0 to the original vbios pstate 0 if it has been modified.
    if (m_VbiosPStateMap.isValid(PSEUDOPSTATE_INDEX) && m_ModifyPState)
    {
        Printf(Tee::PriHigh, "==== Restoring P0 to VBIOS settings  (%s)\n",
            rmt::String::function(__PRETTY_FUNCTION__).c_str());
        status.push(ModifyPState(m_VbiosPStateMap[PSEUDOPSTATE_INDEX]));
    }

    // Restore to init pstate, the state set by devinit
    // TODO: What if there is more than one RMAPI field?
    Printf(Tee::PriHigh, "==== Restoring to init clocks  (%s)\n",
        rmt::String::function(__PRETTY_FUNCTION__).c_str());

    if (m_rmapiFieldMode == TrialSpec::RmapiField::ChSeq)
    {
        status.push(SetChSeq(m_VbiosPStateMap.initPState));
    }
    else if(m_rmapiFieldMode == TrialSpec::RmapiField::Clk)
    {
        status.push(SetClocks(m_VbiosPStateMap.initPState));
    }
    else if(m_rmapiFieldMode == TrialSpec::RmapiField::Pmu)
    {
        status.push(SetInfos(m_VbiosPStateMap.initPState));
    }

    // free LWRM
    if (m_hClient)
    {
        Printf(Tee::PriHigh, "== Freeing LwRm  (%s)\n", rmt::String::function(__PRETTY_FUNCTION__).c_str());
        LwRmFree(m_hClient, m_hClient, m_hClient);
    }

    // Done
    return status.modsRC();
}

//===========

//!
//! \brief      Program clocks individually using LW2080_CTRL_CMD_CLK_SET_INFO
//!
//! \param[in]  pstate   Clock domains will be set
//!
//! \return     Exception (if any)
//!
Exception UniversalClockTest::SetClocks(const FullPState &pstate)
{
    LwRmPtr pLwRm;
    FreqDomain freqDomain;
    LW2080_CTRL_CLK_SET_INFO_PARAMS clkSetInfoParams;
    rmt::Vector<LW2080_CTRL_CLK_INFO> clkInfos;

    Printf(Tee::PriHigh, "\n== Programming clocks via LW2080_CTRL_CMD_CLK_SET_INFO (%s)\n", rmt::String::function(__PRETTY_FUNCTION__).c_str());

    // Initialize the array for the RMAPI call.
    clkInfos.resize(pstate.freqDomainCount());
    clkInfos.memset();

    //
    // Copy over the freq values and flags.  Since the flags are defined in the
    // FullPState object per the LW2080_CTRL_CLK_INFO_FLAGS values, no translation
    // is needed.
    //
    LW2080_CTRL_CLK_INFO *pClkInfoList = clkInfos.c_array();
    for (freqDomain = 0; freqDomain < FreqDomain_Count; ++freqDomain)
    {
        if (pstate.freq[freqDomain])
        {
            pClkInfoList->clkDomain  = BIT(freqDomain);
            pClkInfoList->targetFreq = pstate.freq[freqDomain];
            pClkInfoList->flags      = pstate.flag[freqDomain];
            pClkInfoList->clkSource  = pstate.source[freqDomain];
            ++pClkInfoList;
        }
    }

    // Do not wait until the next modeset.
    clkSetInfoParams.flags = DRF_DEF(2080, _CTRL_CLK_SET_INFO_FLAGS, _WHEN, _IMMEDIATE);
    clkSetInfoParams.clkInfoListSize = (LwU32) clkInfos.size();
    clkSetInfoParams.clkInfoList = LW_PTR_TO_LwP64(clkInfos.c_array());

    // Call the RMAPI to set clocks.
    return Exception(pLwRm->ControlBySubdevice(m_pSubdevice, LW2080_CTRL_CMD_CLK_SET_INFO,
        &clkSetInfoParams, sizeof(clkSetInfoParams)),
        "LW2080_CTRL_CMD_CLK_SET_INFO", __PRETTY_FUNCTION__);
}

//!
//! \brief      Program clocks individually using LW2080_CTRL_CMD_CLK_SET_INFO_PMU
//!
//! \param[in]  pstate   Clock domains will be set
//!
//! \return     Exception (if any)
//!
Exception UniversalClockTest::SetInfos(const FullPState &pstate)
{
    LwU32      voltDomain;
    LwRmPtr    pLwRm;
    FreqDomain freqDomain;
    LwU32      numDomains = 0;

    LW2080_CTRL_CLK_SET_INFOS_PMU_PARAMS      clkSetInfosPmuParams;
    LW2080_CTRL_CLK_EMU_TRAP_REG_SET_PARAMS  clkEmuTrapRegSetParams;

    Printf(Tee::PriHigh, "\n== Programming clocks via LW2080_CTRL_CMD_CLK_SET_INFO_PMU (%s)\n",
            rmt::String::function(__PRETTY_FUNCTION__).c_str());

    //
    // Copy over the freq values and flags.  Since the flags are defined in the
    // FullPState object per the LW2080_CTRL_CLK_INFO_FLAGS values, no translation
    // is needed.
    //
    memset(&clkSetInfosPmuParams, 0, sizeof(clkSetInfosPmuParams));
    memset(&clkEmuTrapRegSetParams, 0, sizeof(clkEmuTrapRegSetParams));

    for (freqDomain = 0; freqDomain < FreqDomain_Count; ++freqDomain)
    {
        if (pstate.freq[freqDomain] != 0)
        {
            //
            // TODO: Will program domains till LW2080_CTRL_CLK_DOMAIN_XCLK
            // have to add changes to target rest (if required)
            //
            if ((BIT32(freqDomain)) > LW2080_CTRL_CLK_DOMAIN_XCLK)
            {
                continue;
            }

            clkSetInfosPmuParams.domainList.clkDomains[numDomains].clkDomain = BIT32(freqDomain);
            clkSetInfosPmuParams.domainList.clkDomains[numDomains].clkFreqKHz = pstate.freq[freqDomain];
            clkSetInfosPmuParams.domainList.clkDomains[numDomains].source = pstate.source[freqDomain];

            //
            // Set RegimeId for NAFLL sourced domains only
            // TODO: Check for NAFLL sourced Domains should be updated here.
            //
            if ((BIT32(freqDomain) == LW2080_CTRL_CLK_DOMAIN_GPCCLK)  ||
                (BIT32(freqDomain) == LW2080_CTRL_CLK_DOMAIN_XBARCLK) ||
                (BIT32(freqDomain) == LW2080_CTRL_CLK_DOMAIN_SYSCLK)  ||
                (BIT32(freqDomain) == LW2080_CTRL_CLK_DOMAIN_HOSTCLK) ||
                (BIT32(freqDomain) == LW2080_CTRL_CLK_DOMAIN_LWDCLK))
            {
                clkSetInfosPmuParams.domainList.clkDomains[numDomains].regimeId =
                    LW2080_CTRL_CLK_NAFLL_REGIME_ID_FFR;
                clkSetInfosPmuParams.domainList.clkDomains[numDomains].source =
                    LW2080_CTRL_CLK_PROG_1X_SOURCE_NAFLL;
            }
            else
            {
                clkSetInfosPmuParams.domainList.clkDomains[numDomains].source =
                    LW2080_CTRL_CLK_PROG_1X_SOURCE_ILWALID;
            }

            numDomains++;
        }
    }

    // Copy over the volt values
    for (voltDomain = 0; voltDomain < VoltDomain_Count; ++voltDomain)
    {
        if ((pstate.volt[voltDomain]) && (BIT(voltDomain) == LW2080_CTRL_PERF_VOLTAGE_DOMAINS_CORE))
        {
            clkSetInfosPmuParams.railList.rails[voltDomain].railIdx = voltDomain;
            clkSetInfosPmuParams.railList.rails[voltDomain].voltageuV = pstate.volt[voltDomain];
            clkEmuTrapRegSetParams.voltageuV = pstate.volt[voltDomain];
        }
    }

    clkSetInfosPmuParams.railList.numRails = (LwU32) pstate.voltDomainCount();
    clkSetInfosPmuParams.domainList.numDomains = numDomains;

    // Call the RMAPI to set voltage on EMU
    if (GetBoundGpuSubdevice()->IsEmulation())
    {
        LwU32 rc = pLwRm->ControlBySubdevice(
                    m_pSubdevice,
                    LW2080_CTRL_CMD_CLK_EMU_TRAP_REG_SET,
                   &clkEmuTrapRegSetParams,
                    sizeof(clkEmuTrapRegSetParams));
        Printf(Tee::PriHigh, "\n== Programming voltage on EMU returned (%d)\n", rc);
    }

    // Call the RMAPI to set clocks.
    return Exception(pLwRm->ControlBySubdevice(m_pSubdevice, LW2080_CTRL_CMD_CLK_SET_INFOS_PMU,
        &clkSetInfosPmuParams, sizeof(clkSetInfosPmuParams)),
        "LW2080_CTRL_CMD_CLK_SET_INFO_PMU", __PRETTY_FUNCTION__);
}

//!
//! \brief  Get the status of the previous clock switch
//!
//! \return     Exception (if any)
//!
Exception UniversalClockTest::GetFreqDomainStatus()
{
    Exception                                       exception;
    LwRmPtr                                         pLwRm;
    RC                                              rc;
    LW2080_CTRL_CLK_FREQ_DOMAIN_GRP_STATUS_PARAMS   params;

    Printf(Tee::PriHigh, "\n== Geting status via LW2080_CTRL_CMD_CLK_FREQ_DOMAIN_GRP_GET_STATUS (%s)\n", rmt::String::function(__PRETTY_FUNCTION__).c_str());

    // Initialize to zero
    memset(&params, 0, sizeof(params));

    // Call the RMAPI to get the status of frequency domains.
    // There is no need to do anything with the returned data under the asumption
    // that RM exelwtes DBG_PRINTF to dump that data to the log.
    rc = pLwRm->ControlBySubdevice(
                m_pSubdevice,
                LW2080_CTRL_CMD_CLK_FREQ_DOMAIN_GRP_GET_STATUS,
               &params,
                sizeof(params));

    // Done
    return (Exception(rc, "LW2080_CTRL_CMD_CLK_FREQ_DOMAIN_GRP_GET_STATUS", __PRETTY_FUNCTION__));
}


//!
//! \brief      Program clocks individually using LW2080_CTRL_CMD_PERF_CHANGE_SEQ_(G|S)ET_CONTROL
//!
//! \param[in]  pstate   Clock domains will be set
//!
//! \return     Exception (if any)
//!
Exception UniversalClockTest::SetChSeq(const FullPState &pstate)
{
    Exception                                exception;
    LwRmPtr                                  pLwRm;
    RC                                       rc;
    LW2080_CTRL_PERF_CHANGE_SEQ_CONTROL      perfChangeSeqControlParams;
    LW2080_CTRL_CLK_EMU_TRAP_REG_SET_PARAMS  clkEmuTrapRegSetParams;
    FreqDomain                               freqDomain;
    VoltDomain                               voltDomain;
    UINT32                                   targetVoltageuV = 0;
    UINT32                                   clkIdx = 0;
    UINT32                                   voltListIdx = 0;
    CLK_UCT_VOLT_LIST                        voltList    = {0};

    Printf(Tee::PriHigh, "\n== Programming clocks via PERF_CHANGE_SEQ_(G|S)ET_CONTROL RMAPI (%s)\n", rmt::String::function(__PRETTY_FUNCTION__).c_str());

    // Initialize to zero
    memset(&perfChangeSeqControlParams, 0, sizeof(perfChangeSeqControlParams));
    memset(&clkEmuTrapRegSetParams, 0, sizeof(clkEmuTrapRegSetParams));

    // Call the RMAPI to get the change sequencer control params
    rc = pLwRm->ControlBySubdevice(
                m_pSubdevice,
                LW2080_CTRL_CMD_PERF_CHANGE_SEQ_GET_CONTROL,
               &perfChangeSeqControlParams,
                sizeof(perfChangeSeqControlParams));

    perfChangeSeqControlParams.bChangeRequested = LW_TRUE;

    // Copy over the frequency values
    for (freqDomain = 0; freqDomain < FreqDomain_Count; ++freqDomain)
    {
        if (pstate.freq[freqDomain])
        {
            clkIdx = GetClockHalIdxFromDomain(BIT(freqDomain));
            MASSERT((clkIdx != CLK_IDX_ILWALID));

            //
            // Assert if client request a change in clock domain that is excluded. Ideally this should be done
            // while building the PSTATE tuple @ref FullPState pstate
            //
            // if ((perfChangeSeqControlParams.clkDomainsExclusionMask.super.pData[0] & BIT(clkIdx)) != 0)
            // {
            //     return (Exception(rc, "Requested exluded clock domain.", __PRETTY_FUNCTION__));
            // }

            // Update the input change struct based on request.
            perfChangeSeqControlParams.changeInput.clkDomainsMask.super.pData[0] |= BIT(clkIdx);
            perfChangeSeqControlParams.changeInput.clk[clkIdx].clkFreqkHz =
                                     pstate.freq[freqDomain];
        }
    }

    // Copy over the volt values
    for (voltDomain = 0; voltDomain < VoltDomain_Count; ++voltDomain)
    {
        if (pstate.volt[voltDomain])
        {
            targetVoltageuV = pstate.volt[voltDomain];

            if (BIT(voltDomain) == CLK_UCT_VOLT_DOMAIN_LOGIC ||
                ((clkDomainsInfo.version == LW2080_CTRL_CLK_CLK_DOMAIN_VERSION_40) &&
                  (BIT(voltDomain) == CLK_UCT_VOLT_DOMAIN_MSVDD)))
            {
                // Emulation only constraint, lwrrently min supported is 560mV
                if (GetBoundGpuSubdevice()->IsEmulation())
                {
                    targetVoltageuV = ((targetVoltageuV <
                                       LW2080_CTRL_CLK_EMU_SUPPORTED_MIN_VOLTAGE_UV) ?
                                       LW2080_CTRL_CLK_EMU_SUPPORTED_MIN_VOLTAGE_UV  :
                                       targetVoltageuV);
                    //
                    // On emulation make sure we set the EMU_TRAP_REG with the highest
                    // targetVolt between LWVDD and MSVDD to be able to support all the
                    // clocks.
                    //
                    if (targetVoltageuV > clkEmuTrapRegSetParams.voltageuV)
                    {
                        clkEmuTrapRegSetParams.voltageuV = targetVoltageuV;
                    }
                }
            }
            voltList.voltInfo[voltListIdx].voltDomain = ClkVoltDomainUctTo2080(BIT(voltDomain));
            voltList.voltInfo[voltListIdx].voltUv     = targetVoltageuV;
            voltListIdx++;
        }
    }

    // Fill in the volt rails list
    GetVoltRailList(&perfChangeSeqControlParams.changeInput, &voltList);

    // Call the RMAPI to set voltage on EMU
    if (GetBoundGpuSubdevice()->IsEmulation())
    {
        rc = pLwRm->ControlBySubdevice(
                    m_pSubdevice,
                    LW2080_CTRL_CMD_CLK_EMU_TRAP_REG_SET,
                   &clkEmuTrapRegSetParams,
                    sizeof(clkEmuTrapRegSetParams));
    }

    // Call the RMAPI to set clocks and voltages
    rc = pLwRm->ControlBySubdevice(
                m_pSubdevice,
                LW2080_CTRL_CMD_PERF_CHANGE_SEQ_SET_CONTROL,
               &perfChangeSeqControlParams,
                sizeof(perfChangeSeqControlParams));

    // Done
    return (Exception(rc, "LW2080_CTRL_CMD_PERF_CHANGE_SEQ_SET_CONTROL", __PRETTY_FUNCTION__));
}


//!
//! \brief      Configure clocks (does not program) individually using LW2080_CTRL_CMD_CLK_CONFIG_INFO_V2
//!
//! \param[in]  pstate   Clock domains will be configured to
//!
//! \return     Exception (if any)
//!
Exception UniversalClockTest::ConfigClocks(Result &result)
{
    RC rc;
    LwRmPtr pLwRm;
    FreqDomain freqDomain;
    FreqDomainBitmap freqDomainSet = result.target.freqDomainSet();
    LW2080_CTRL_CLK_CONFIG_INFO_V2_PARAMS clkConfigInfoParams;
    rmt::Vector<LW2080_CTRL_CLK_INFO> clkInfos;
    rmt::Vector<LW2080_CTRL_CLK_INFO>::const_iterator pClkInfos;
    const FullPState pstate = result.target;

    Printf(Tee::PriHigh, "\n== Configuring clocks via LW2080_CTRL_CMD_CLK_CONFIG_INFO_V2 (%s)\n", rmt::String::function(__PRETTY_FUNCTION__).c_str());

    // Initialize the array for the RMAPI call.
    clkInfos.resize(pstate.freqDomainCount());
    clkInfos.memset();

    //
    // Copy over the freq values and flags.  Since the flags are defined in the
    // FullPState object per the LW2080_CTRL_CLK_INFO_FLAGS values, no translation
    // is needed.
    //
    LW2080_CTRL_CLK_INFO *pClkInfoList = clkInfos.c_array();
    for (freqDomain = 0; freqDomain < FreqDomain_Count; ++freqDomain)
    {
        if (pstate.freq[freqDomain])
        {
            pClkInfoList->clkDomain  = BIT(freqDomain);
            pClkInfoList->targetFreq = pstate.freq[freqDomain];
            pClkInfoList->flags      = pstate.flag[freqDomain];
            pClkInfoList->clkSource  = pstate.source[freqDomain];
            ++pClkInfoList;
        }
    }

    // Do not wait until the next modeset.
    clkConfigInfoParams.flags = 0;
    clkConfigInfoParams.clkInfoListSize = (LwU32)clkInfos.size();
    if (clkConfigInfoParams.clkInfoListSize > LW2080_CTRL_CLK_ARCH_MAX_DOMAINS)
    {
        return Exception(RC::DATA_TOO_LARGE, "clkInfoList", __PRETTY_FUNCTION__);
    }
    memcpy(clkConfigInfoParams.clkInfoList, clkInfos.c_array(),
           sizeof(LW2080_CTRL_CLK_INFO) * clkConfigInfoParams.clkInfoListSize);

    // Call the RMAPI to configure clocks.
    rc = pLwRm->ControlBySubdevice(m_pSubdevice, LW2080_CTRL_CMD_CLK_CONFIG_INFO_V2,
                &clkConfigInfoParams, sizeof(clkConfigInfoParams));
    if (rc == OK)
    {
        memcpy(clkInfos.c_array(), clkConfigInfoParams.clkInfoList,
               sizeof(LW2080_CTRL_CLK_INFO) * clkConfigInfoParams.clkInfoListSize);
        for (pClkInfos = clkInfos.begin(); pClkInfos != clkInfos.end(); ++pClkInfos)
        {
            freqDomain = BIT_IDX_32(pClkInfos->clkDomain);
            MASSERT(freqDomain < FreqDomain_Count);

            // The frequency domain is assumed to be one bit per 32-bit entry.
            MASSERT(BIT32(freqDomain) == pClkInfos->clkDomain);

            MASSERT(result.target.freq[freqDomain]);

            // The RMAPI should not give us the same domain more than once.
            MASSERT(freqDomainSet & BIT32(freqDomain));

            // Flag that we have checked this clock domain.
            freqDomainSet &= ~BIT32(freqDomain);

            result.actual.freq[freqDomain]   = pClkInfos->actualFreq;
            result.actual.source[freqDomain] = pClkInfos->clkSource;
            result.actual.flag[freqDomain]   = pClkInfos->flags;
        }
    }

    return Exception(rc, "LW2080_CTRL_CMD_CLK_CONFIG_INFO_V2", __PRETTY_FUNCTION__);

}

//!
//! \brief      Switch p-state switch using LW2080_CTRL_CMD_PERF_SET_FORCE_PSTATE
//!
//! \param[in]  nPState     the pstate number to switch to (0 <= n <= 15)
//!
//! \return     Exception (if any)
//!
Exception UniversalClockTest::SwitchToPState(VbiosPState::Index nPState)
{
    LwRmPtr pLwRm;

    LW2080_CTRL_PERF_SET_FORCE_PSTATE_PARAMS setForcePStateParams;

    Printf(Tee::PriHigh, "\n== Switching PState to P%u (%s)\n", (unsigned) nPState,
        rmt::String::function(__PRETTY_FUNCTION__).c_str());

    MASSERT(BIT(nPState) <= LW2080_CTRL_PERF_PSTATES_MAX && BIT(nPState) >= LW2080_CTRL_PERF_PSTATES_P0);

    // Set the structure parameters for the RMAPI call
    setForcePStateParams.forcePstate = BIT(nPState);
    setForcePStateParams.fallback = LW2080_CTRL_PERF_PSTATE_FALLBACK_RETURN_ERROR;

    // call API
    return Exception(pLwRm->ControlBySubdevice(m_pSubdevice, LW2080_CTRL_CMD_PERF_SET_FORCE_PSTATE,
        &setForcePStateParams, sizeof(setForcePStateParams)),
        "LW2080_CTRL_CMD_PERF_SET_FORCE_PSTATE", __PRETTY_FUNCTION__);
}

//!
//! \brief      Confirm that the current pstate is of the indicated
//!
//! \param[in]  nPState  the pstate number we should be in (0 <= n <= 15)
//!
//! \return     Exception (if any)
//!
Exception UniversalClockTest::VerifyPStateSwitch(VbiosPState::Index nPState)
{
    RC rc;
    LwRmPtr pLwRm;

    LW2080_CTRL_PERF_GET_LWRRENT_PSTATE_PARAMS getLwrrentPStateParams;

    // Get the current pstate number
    getLwrrentPStateParams.lwrrPstate = LW2080_CTRL_PERF_PSTATES_UNDEFINED;
    rc = pLwRm->ControlBySubdevice(m_pSubdevice,
             LW2080_CTRL_CMD_PERF_GET_LWRRENT_PSTATE,
             &getLwrrentPStateParams, sizeof(getLwrrentPStateParams));

    // Bail if there is an error.
    if (rc != OK)
        return Exception(rc, "LW2080_CTRL_CMD_PERF_GET_LWRRENT_PSTATE", __PRETTY_FUNCTION__);

    // Is this what we are expecting?
    if (getLwrrentPStateParams.lwrrPstate != BIT32(nPState))
        return Exception(RC::CANNOT_SET_STATE, rmt::String("Current p-state is P") +
            rmt::String::dec(BIT_IDX_32(getLwrrentPStateParams.lwrrPstate)) +
            " (" + rmt::String::hex(getLwrrentPStateParams.lwrrPstate, 8) +
            "), but was expected to be P" + rmt::String::dec(nPState),
            __PRETTY_FUNCTION__);

    // All is okay
    return Exception();
}

//!
//! \brief      Modify VBIOS P-State
//!
//! \details    This function modifies the RM definition of P0 in VbioS to the
//!             new pstate given. P0 is used because the API PERF_SET_PSTATE_INFO
//!             supports overclocking only on P0.
//!
//! \param[in]  pstate      Fully-defined p-state specification
//!
//------------------------------------------------------------------------------
Exception UniversalClockTest::ModifyPState(const FullPState &pstate)
{
    FreqDomain freqDomain;
    VoltDomain voltDomain;
    LwRmPtr pLwRm;
    rmt::Vector<LW2080_CTRL_PERF_CLK_DOM_INFO>  vClkDomainBuffer;
    rmt::Vector<LW2080_CTRL_PERF_VOLT_DOM_INFO> vVoltDomainBuffer;
    LW2080_CTRL_PERF_SET_PSTATE_INFO_PARAMS setPStateInfoParams;

    Printf(Tee::PriHigh, "\n== Programming clocks via PERF RMAPI  (%s)\n", rmt::String::function(__PRETTY_FUNCTION__).c_str());

    // Flag that we need to restore the p-state on Cleanup.
    m_ModifyPState = true;

    // Print a warning if we need to set source
    if(pstate.hasSourceConfig())
        Printf(Tee::PriHigh, "Warning: Source cannot be set using PERF RMAPI\n");

    // Prepare structure
    setPStateInfoParams.pstate                  = BIT(PSEUDOPSTATE_INDEX);
    setPStateInfoParams.flags                   = DRF_DEF(2080, _CTRL_SET_PSTATE_INFO_FLAG, _MODE, _INTERNAL_TEST);
    setPStateInfoParams.perfClkDomInfoListSize  = (LwU32) pstate.freqDomainCount();
    setPStateInfoParams.perfVoltDomInfoListSize = (LwU32) pstate.voltDomainCount();
    vClkDomainBuffer.resize(setPStateInfoParams.perfClkDomInfoListSize);
    vClkDomainBuffer.memset();
    vVoltDomainBuffer.resize(setPStateInfoParams.perfVoltDomInfoListSize);
    vVoltDomainBuffer.memset();
    setPStateInfoParams.perfClkDomInfoList      = LW_PTR_TO_LwP64(vClkDomainBuffer.c_array());
    setPStateInfoParams.perfVoltDomInfoList     = LW_PTR_TO_LwP64(vVoltDomainBuffer.c_array());

    LW2080_CTRL_PERF_CLK_DOM_INFO  *pClkInfoList = vClkDomainBuffer.c_array();
    LW2080_CTRL_PERF_VOLT_DOM_INFO *pVoltInfoList = vVoltDomainBuffer.c_array();

    //
    // Copy over the freq values and flags.  Since the flags are defined in the
    // FullPState object per the LW2080_CTRL_CLK_INFO_FLAGS values, we must translate
    // them to LW2080_CTRL_PERF_CLK_DOM_INFO_FLAGS values.
    //
    for (freqDomain = 0; freqDomain < FreqDomain_Count; ++freqDomain)
    {
        if (pstate.freq[freqDomain])
        {
            pClkInfoList->domain = BIT(freqDomain);
            pClkInfoList->freq   = pstate.freq[freqDomain];
            pClkInfoList->flags  = ApiFlagMap::translateClk2Perf(pstate.flag[freqDomain]);
            ++pClkInfoList;
        }
    }

    // Copy over the volt values
    for (voltDomain = 0; voltDomain < VoltDomain_Count; ++voltDomain)
    {
        if (pstate.volt[voltDomain])
        {
            pVoltInfoList->domain = BIT(voltDomain);
            pVoltInfoList->type   = LW2080_CTRL_PERF_VOLT_DOM_INFO_TYPE_LOGICAL;
            pVoltInfoList->data.logical.logicalVoltageuV = pstate.volt[voltDomain];
            ++pVoltInfoList;
        }
    }

    // Modify P0
    return Exception(pLwRm->ControlBySubdevice(m_pSubdevice,
            LW2080_CTRL_CMD_PERF_SET_PSTATE_INFO,
            &setPStateInfoParams, sizeof(setPStateInfoParams)),
            "LW2080_CTRL_CMD_PERF_SET_PSTATE_INFO", __PRETTY_FUNCTION__);
}

//!
//! \brief      Set VBIOS P-State VfPoint
//!
//! \details    This function modifies the RM definition of P0 in Vbios to the
//!             new pstate given and specified vf point. API used is
//!             PERF_LIMITS_SET_STATUS
//!
//! \param[in]  pstate      Fully-defined p-state specification
//!
//------------------------------------------------------------------------------
Exception UniversalClockTest::SetVfPoints(const FullPState &pstate)
{
    LwRmPtr pLwRm;
    Exception ex;
    RC rc;
    LW2080_CTRL_PERF_LIMIT_SET_STATUS limits[4];
    LW2080_CTRL_PERF_LIMITS_SET_STATUS_PARAMS setLimitsParams = {0};

    limits[0].limitId = LW2080_CTRL_PERF_LIMIT_MODS_RULES_STRICT_PSTATE_MIN;
    limits[1].limitId = LW2080_CTRL_PERF_LIMIT_MODS_RULES_STRICT_PSTATE_MAX;

    limits[0].input.type = LW2080_CTRL_PERF_LIMIT_INPUT_TYPE_PSTATE;
    limits[1].input.type = LW2080_CTRL_PERF_LIMIT_INPUT_TYPE_PSTATE;
    limits[2].input.type = LW2080_CTRL_PERF_LIMIT_INPUT_TYPE_FREQ;
    limits[3].input.type = LW2080_CTRL_PERF_LIMIT_INPUT_TYPE_FREQ;

    limits[0].input.data.pstate.pstateId = BIT(pstate.base);
    limits[0].input.data.pstate.point    = LW2080_CTRL_PERF_LIMIT_INPUT_DATA_PSTATE_POINT_NOM;
    limits[1].input.data.pstate.pstateId = BIT(pstate.base);
    limits[1].input.data.pstate.point    = LW2080_CTRL_PERF_LIMIT_INPUT_DATA_PSTATE_POINT_NOM;
    limits[2].input.data.freq.domain = clkDomainsInfo.domains[clkDomailwfLwrveIndex].domain ;
    limits[3].input.data.freq.domain = clkDomainsInfo.domains[clkDomailwfLwrveIndex].domain ;

    setLimitsParams.numLimits = 4;
    setLimitsParams.flags = DRF_DEF(2080, _CTRL_SET_PSTATE_INFO_FLAG, _MODE, _INTERNAL_TEST);

    limits[2].input.data.freq.freqKHz = pstate.freq[BIT_IDX_32(clkDomainsInfo.domains[clkDomailwfLwrveIndex].domain)];
    limits[3].input.data.freq.freqKHz = pstate.freq[BIT_IDX_32(clkDomainsInfo.domains[clkDomailwfLwrveIndex].domain)];

    switch(clkDomainsInfo.domains[clkDomailwfLwrveIndex].domain)
    {
        case LW2080_CTRL_CLK_DOMAIN_GPCCLK :
        case LW2080_CTRL_CLK_DOMAIN_GPC2CLK :
            limits[2].limitId = LW2080_CTRL_PERF_LIMIT_MODS_RULES_STRICT_GPC_MAX;
            limits[3].limitId = LW2080_CTRL_PERF_LIMIT_MODS_RULES_STRICT_GPC_MIN;
            break;

        case LW2080_CTRL_CLK_DOMAIN_DISPCLK :
            limits[2].limitId = LW2080_CTRL_PERF_LIMIT_MODS_RULES_STRICT_DISP_MAX;
            limits[3].limitId = LW2080_CTRL_PERF_LIMIT_MODS_RULES_STRICT_DISP_MIN;
            break;

        case LW2080_CTRL_CLK_DOMAIN_MCLK :
            limits[2].limitId = LW2080_CTRL_PERF_LIMIT_MODS_RULES_STRICT_DRAM_MAX;
            limits[3].limitId = LW2080_CTRL_PERF_LIMIT_MODS_RULES_STRICT_DRAM_MIN;
            break;
    }

    setLimitsParams.pLimits = LW_PTR_TO_LwP64(limits);

    return Exception(pLwRm->ControlBySubdevice(m_pSubdevice,
            LW2080_CTRL_CMD_PERF_LIMITS_SET_STATUS,
            &setLimitsParams, sizeof(setLimitsParams)),
            "LW2080_CTRL_CMD_PERF_LIMITS_SET_STATUS", __PRETTY_FUNCTION__);
}

//!
//! \brief      Check Minimum Frequency range for slave clk domains in vflwrve test.
//!
//! \param[in]  pstate number
//!
//! \return Exception if any
//------------------------------------------------------------------------------
Exception UniversalClockTest::CheckMinFrequency(Result &result)
{
    LwU32 i,slaveId,slaveMask;
    LwRmPtr pLwRm;
    LW2080_CTRL_PERF_PSTATES_INFO pstatesInfo = {};

    RC rc = pLwRm->ControlBySubdevice(
        m_pSubdevice
       ,LW2080_CTRL_CMD_PERF_PSTATES_GET_INFO
       ,&pstatesInfo
       ,sizeof(pstatesInfo));

    if(rc!=OK)
    {
        Printf(Tee::PriHigh, "\n Error in LW2080_CTRL_CMD_PERF_PSTATES_GET_INFO %s\n"
            ,rmt::String::function(__PRETTY_FUNCTION__).c_str());
    }
    LW2080_CTRL_BOARDOBJGRP_MASK_FOR_EACH_INDEX(32,i,&pstatesInfo.super.objMask.super)
    {
        if(BIT_IDX_32(pstatesInfo.pstates[i].pstateID) == result.target.base)
        {
            switch (clkDomainsInfo.domains[clkDomailwfLwrveIndex].super.type)
            {
                case LW2080_CTRL_CLK_CLK_DOMAIN_TYPE_30_PRIMARY:
                {
                    slaveMask = clkDomainsInfo.domains[clkDomailwfLwrveIndex].data.v30Master.master.slaveIdxsMask;
                    break;
                }
                case LW2080_CTRL_CLK_CLK_DOMAIN_TYPE_35_PRIMARY:
                {
                    slaveMask = clkDomainsInfo.domains[clkDomailwfLwrveIndex].data.v35Master.master.slaveIdxsMask;
                    break;
                }
                case LW2080_CTRL_CLK_CLK_DOMAIN_TYPE_40_PROG:
                {
                    LwU8 railIdx;

                    // Initialize slaveMask
                    slaveMask = 0;

                    FOR_EACH_INDEX_IN_MASK(32, railIdx, clkDomainsInfo.domains[clkDomailwfLwrveIndex].data.v40Prog.railMask)
                    {
                        if (clkDomainsInfo.domains[clkDomailwfLwrveIndex].data.v40Prog.railVfItem[railIdx].type ==
                            LW2080_CTRL_CLK_CLK_DOMAIN_40_PROG_RAIL_VF_TYPE_PRIMARY)
                        {
                            slaveMask = clkDomainsInfo.domains[clkDomailwfLwrveIndex].data.v40Prog.railVfItem[railIdx].data.master.slaveDomainsMask.super.pData[0];
                        }
                    }
                    FOR_EACH_INDEX_IN_MASK_END;
                    break;
                }
                default:
                {
                    slaveMask = 0;
                    break;
                }
            }

            FOR_EACH_INDEX_IN_MASK(32,slaveId,slaveMask)
            {
                if (pstatesInfo.pstates[i].data.v3x.clkEntries[slaveId].min.porFreqkHz >
                    result.target.freq[BIT_IDX_32(clkDomainsInfo.domains[slaveId].domain)])
                {
                    Printf(Tee::PriHigh,"\n Requested frequency %d is lower than MinFreqRange for clkDomain %d\n",
                        result.target.freq[BIT_IDX_32(clkDomainsInfo.domains[slaveId].domain)],
                        clkDomainsInfo.domains[slaveId].domain );

                    result.target.freq[BIT_IDX_32(clkDomainsInfo.domains[slaveId].domain)] =
                        pstatesInfo.pstates[i].data.v3x.clkEntries[slaveId].min.porFreqkHz;
                }
            }FOR_EACH_INDEX_IN_MASK_END;
        }
    }LW2080_CTRL_BOARDOBJGRP_MASK_FOR_EACH_INDEX_END;

     return Exception(rc, "LW2080_CTRL_CMD_PERF_PSTATES_GET_INFO", __PRETTY_FUNCTION__);

}

//!
//! \brief  Check for all the Nafll clock domains whose DVCO Min have reached
//!
//! \return returns a Mask of all the Nafll clock domains whose DVCO min have reached.
//------------------------------------------------------------------------------
UINT32 UniversalClockTest::GetNafllDvcoMinReachedClkDomMask()
{
    LW2080_CTRL_CLK_NAFLL_DEVICES_STATUS_PARAMS clkNafllDevicesStatusParams;
    UINT32 Idx,clkDomainDvcoMinMask = 0;
    LwRmPtr pLwRm;

    // Fill in the nafll mask and query the LUT
    clkNafllDevicesStatusParams.super.objMask =
            m_ClkNafllDevicesInfoParams.super.objMask;
    RC rc = pLwRm->ControlBySubdevice(
            m_pSubdevice,
            LW2080_CTRL_CMD_CLK_NAFLL_DEVICES_GET_STATUS,
            &clkNafllDevicesStatusParams,
            sizeof(clkNafllDevicesStatusParams));

    if (rc != OK)
    {
        m_initStatus.push(Exception(rc,
                "LW2080_CTRL_CMD_CLK_NAFLL_DEVICES_GET_STATUS",
                __PRETTY_FUNCTION__));
    }

    else
    {
        FOR_EACH_INDEX_IN_MASK(32, Idx, clkNafllDevicesStatusParams.super.objMask.super.pData[0])
        {
            if (clkNafllDevicesStatusParams.devices[Idx].bDvcoMinReached)
            {
                clkDomainDvcoMinMask |= m_ClkNafllDevicesInfoParams.devices[Idx].clkDomain;
            }
        }FOR_EACH_INDEX_IN_MASK_END;
    }
    return clkDomainDvcoMinMask;
}

//!
//! \brief Check that the lwrent VBiosP0 information matches target pstate
//!
//! \param[in] pstate   the pstate to be checked against
//!
//------------------------------------------------------------------------------
ExceptionList UniversalClockTest::VerifyModifyPState(const FullPState &targetPState)
{
    VbiosPState programmedPState;
    Exception exception;

    // Print a warning if we need to set source
    if(targetPState.hasSourceConfig())
        Printf(Tee::PriHigh, "Warning: Source was not configurable using PERF RMAPI so this is not checked\n");

    // Get the current pstate at P0
    exception = GetPStateInfo(PSEUDOPSTATE_INDEX, programmedPState);
    if (exception)
        return exception;

    // Compare to see if the two p-states are equal.
    if (!targetPState.matches(programmedPState))
    {
        // Make the name clear in the debug spew.
        programmedPState.name = "<current>";

        // Return exceptions showing the p-state read from resman.
        return PStateException(RC::DATA_MISMATCH, "PState P0 was not properly modified.",
            programmedPState, __PRETTY_FUNCTION__);
    }

    // No errors.
    return Exception();
}

//!
//! \brief      Query for all supported pstates and then fetch the pstates and populate
//!
//! \post       m_VbiosPStateMap contains the VBIOS p-state data + a init pstate
//! \post       m_PStatesInfoParams contains the GET_PSTATES_INFO data.
//!
//! \todo       Consider making m_PStatesInfoParams local to this function.
//!
//! \todo       Consider moving this function into VbiosPStateArray.
//!
//! \return     List of exceptions
//!
ExceptionList UniversalClockTest::GetSupportedPStates()
{
    VbiosPState::Index i;
    Exception exception;
    ExceptionList status;
    LwRmPtr pLwRm;
    LwRm::Handle pRmHandle = pLwRm->GetSubdeviceHandle(m_pSubdevice);
    LW2080_CTRL_VOLT_VOLT_RAILS_INFO_PARAMS  voltRailsInfo;
    UINT32                                   voltIdx = 0;
    RC rc;

    Printf(Tee::PriHigh, "\n======== Getting VBIOS P-States  (%s)\n", rmt::String::function(__PRETTY_FUNCTION__).c_str());

    // found how how many pstates do we have enabled lwrrently
    rc = pLwRm->Control(pRmHandle, LW2080_CTRL_CMD_PERF_GET_PSTATES_INFO,
        &m_PStatesInfoParams, sizeof(LW2080_CTRL_PERF_GET_PSTATES_INFO_PARAMS));

    // If we can't read the VBIOS p-states, we can't continue.
    if (rc != OK)
    {
        status.push(Exception(rc, "LW2080_CTRL_CMD_PERF_GET_PSTATES_INFO",
            __PRETTY_FUNCTION__, Exception::Fatal));
        m_PStatesInfoParams.pstates = 0x00000000;
    }

    // For each possible pstate number
    else for (i = 0; i < VbiosPState::count && !status.isFatal(); ++i)
    {
        // If the pstate is enabled in VBIOS
        if (m_PStatesInfoParams.pstates & BIT(i))
        {
            // Get the actual pstate, store and print it
            exception = GetPStateInfo(i, m_VbiosPStateMap[i]);

            // If there is a problem, delete this p-state from the list
            if (exception)
            {
                status.push(exception);
                m_PStatesInfoParams.pstates &= ~BIT(i);
            }
        }
    }

    if (m_PStatesInfoParams.pstates == 0x00000000)
        m_PstateSupported = false;
    else
        m_PstateSupported = true;

    // Get current clocks and fill in a init P-State
    Result r;
    status.push(GetClockProgramming(r, false));

    // If there was any error then return, since if we cannot read clocks then
    // something is seriously wrong
    if (!status.isOkay())
        status.push(Exception("Unable to read clocks", __PRETTY_FUNCTION__,
            Exception::Fatal));

    // Save the init pstates
    VbiosPState &pstate = m_VbiosPStateMap.initPState;

    pstate.base = VbiosPState::initPStateIndex;
    pstate.name = VbiosPState::initPStateName;
    for (FreqDomain i = 0; i<FreqDomain_Count; ++i)
    {
        pstate.freq[i] = r.actual.freq[i];
        pstate.flag[i] = r.actual.flag[i];
    }

    // This is a PS30 call only
    if (IsPASCALorBetter(m_pSubdevice->DeviceId()))
    {
        // Get the boot voltage for LOGIC rail
        memset(&voltRailsInfo, 0, sizeof(voltRailsInfo));
        rc = pLwRm->ControlBySubdevice(m_pSubdevice,
                                       LW2080_CTRL_CMD_VOLT_VOLT_RAILS_GET_INFO,
                                       &voltRailsInfo,
                                       sizeof(voltRailsInfo));
        if (rc == OK)
        {
            // Cache the supported volt rails mask for use later
            m_VoltRailsMask = voltRailsInfo.super.objMask.super.pData[0];

            FOR_EACH_INDEX_IN_MASK(32, voltIdx, voltRailsInfo.super.objMask.super.pData[0])
            {
                // Get the LOGIC Rail boot voltage
                if (voltRailsInfo.rails[voltIdx].super.type == LW2080_CTRL_VOLT_VOLT_DOMAIN_LOGIC)
                {
                    pstate.volt[m_LwvddIdx] = voltRailsInfo.rails[voltIdx].bootVoltageuV;
                }
                else if ((clkDomainsInfo.version == LW2080_CTRL_CLK_CLK_DOMAIN_VERSION_40) &&
                    (voltRailsInfo.rails[voltIdx].super.type == LW2080_CTRL_VOLT_VOLT_DOMAIN_MSVDD))
                {
                    pstate.volt[m_MsvddIdx] = voltRailsInfo.rails[voltIdx].bootVoltageuV;
                }
            }
            FOR_EACH_INDEX_IN_MASK_END;
        }
        else
        {
           // Hardcode, if volt rail call is not supported
            pstate.volt[m_LwvddIdx] = 800000;
            if (clkDomainsInfo.version == LW2080_CTRL_CLK_CLK_DOMAIN_VERSION_40)
            {
                pstate.volt[m_MsvddIdx] = 800000;
            }
        }
    }

    pstate.bInitPState = true;

    // We must have at least one p-state in the m_VbiosPStateMap
    if (!m_PStatesInfoParams.pstates && !m_VbiosPStateMap.initPState.isValid())
        status.push(Exception("Unable to continue without any p-states",
            __PRETTY_FUNCTION__, Exception::Fatal));

    Printf(Tee::PriHigh, "%s  (%s)\n", m_VbiosPStateMap.debugString().c_str(),
        rmt::String::function(__PRETTY_FUNCTION__).c_str());

    // Done
    return status;
}

//!
//! \brief retrieve pstate information for a particular pstate
//!
//! \pre        m_PStatesInfoParams contains the GET_PSTATES_INFO data.
//!
//! \param[in]  pPStateInfoParams   the information returned by
//!                                 LW2080_CTRL_CMD_PERF_GET_PSTATES_INFO
//! \param[in]  pstateIndex         the pstate to retrieve
//! \param[out] pstate              p-state data
//!
//! \todo       Consider moving this function into VbiosPState.
//!
//! \return     Exception if any
//------------------------------------------------------------------------------
Exception UniversalClockTest::GetPStateInfo(VbiosPState::Index pstateIndex, VbiosPState &pstate)
{
    RC rc;
    LwRmPtr pLwRm;
    rmt::Vector<LW2080_CTRL_PERF_CLK_DOM_INFO>  vClkDomainBuffer;
    rmt::Vector<LW2080_CTRL_PERF_CLK_DOM2_INFO> vClkDomain2Buffer;
    rmt::Vector<LW2080_CTRL_PERF_VOLT_DOM_INFO> vVoltDomainBuffer;
    UINT32 clkIdx = 0;
    UINT32 voltIdx = 0;
    LW2080_CTRL_PERF_CLK_DOM_INFO* pClkInfo;
    LW2080_CTRL_PERF_VOLT_DOM_INFO* pVoltInfo;
    UINT32 i;
    const LwU32 nPState = BIT(pstateIndex);

    // prepare the structures needed by the API
    LW2080_CTRL_PERF_GET_PSTATE2_INFO_PARAMS pstateInfoParams = {0};
    pstateInfoParams.pstate                  = nPState;
    pstateInfoParams.perfClkDomInfoListSize  = Utility::CountBits(m_PStatesInfoParams.perfClkDomains);
    pstateInfoParams.perfVoltDomInfoListSize = Utility::CountBits(m_PStatesInfoParams.perfVoltageDomains);
    vClkDomainBuffer.resize(pstateInfoParams.perfClkDomInfoListSize);
    vClkDomain2Buffer.resize(pstateInfoParams.perfClkDomInfoListSize);
    vVoltDomainBuffer.resize(pstateInfoParams.perfVoltDomInfoListSize);
    pstateInfoParams.perfClkDomInfoList  = LW_PTR_TO_LwP64(vClkDomainBuffer.memset());
    pstateInfoParams.perfClkDom2InfoList = LW_PTR_TO_LwP64(vClkDomain2Buffer.memset());
    pstateInfoParams.perfVoltDomInfoList = LW_PTR_TO_LwP64(vVoltDomainBuffer.memset());

    // set the clk/volt domains in each list
    for (i = 0; i < 32; ++i)
    {
        if (m_PStatesInfoParams.perfClkDomains & BIT(i))
        {
            vClkDomainBuffer[clkIdx].domain  = BIT(i);
            vClkDomain2Buffer[clkIdx].domain = BIT(i);
            clkIdx++;
        }

        if(m_PStatesInfoParams.perfVoltageDomains & BIT(i))
        {
            vVoltDomainBuffer[voltIdx].domain = BIT(i);
            voltIdx++;
        }
    }

    // getting the actual PState
    LwRm::Handle pRmHandle = pLwRm->GetSubdeviceHandle(m_pSubdevice);
    rc = pLwRm->Control(pRmHandle,
                        LW2080_CTRL_CMD_PERF_GET_PSTATE2_INFO,
                        &pstateInfoParams,
                        sizeof(pstateInfoParams));

    // Return on error
    if (rc != OK)
        return Exception(rc, "LW2080_CTRL_CMD_PERF_GET_PSTATE2_INFO", __PRETTY_FUNCTION__);

    // Unpack the pstate information into the PState object
    pstate.base = pstateIndex;
    pstate.name = "vbiosp" + rmt::String::dec(pstateIndex);

    // Unpack frequences and flags
    pClkInfo = vClkDomainBuffer.c_array();
    for(i = 0; i < vClkDomainBuffer.size(); ++i)
    {
        pstate.freq[BIT_IDX_32(pClkInfo->domain)] = pClkInfo->freq;
        pstate.flag[BIT_IDX_32(pClkInfo->domain)] = ApiFlagMap::translatePerf2Clk(pClkInfo->flags);
        ++pClkInfo;
    }

    // Unpack voltages
    pVoltInfo = vVoltDomainBuffer.c_array();
    for(i = 0; i < vVoltDomainBuffer.size(); ++i)
    {
        // Prefer the logical voltage if available
        if (pVoltInfo->type == LW2080_CTRL_PERF_VOLT_DOM_INFO_TYPE_LOGICAL)
            pstate.volt[BIT_IDX_32(pVoltInfo->domain)] = pVoltInfo->data.logical.logicalVoltageuV;

        // Use current voltage as a backup plan even though it reflect the temperature, etc.
        else
            pstate.volt[BIT_IDX_32(pVoltInfo->domain)] = pVoltInfo->lwrrTargetVoltageuV;

        ++pVoltInfo;
    }

    // All good, so return a NULL exception
    return Exception();
}

//!
//! \brief      Source CTP File(s)
//!
//! \pre        m_pSubdevice and m_VbiosPStateMap must contain proper data.
//!
//! \pre        m_EntryCtpFilename should contain the CTP name unless the
//!             default of "sanity" is intended.
//!
//! \pre        m_Exclude should contain the list of clock domains (if any)
//!             to be excluded from the trials.
//!
//! \post       m_TrialList contains the list of trials with their p-state
//!             references resolved (unless there are serious errors).
//!
//! \post       m_Status contains any applicable exceptions.
//!
//! \note       The CtpFileReader object is local to this function so that
//!             its memory is freed when this function returns.
//!
ExceptionList UniversalClockTest::ReadEntryCtpFile()
{
    CtpFileReader reader;
    ExceptionList status;

    // Read 'sanity.ctp' by default.
    status.push(reader.initialize(rmt::String(m_EntryCtpFilename).defaultTo("sanity"),
                m_pSubdevice, rmt::String(m_Exclude)));

    // Read the Clock Test Profile file.
    status.push(reader.scanFile(m_pSubdevice));

    // Talk
    reader.dumpContent();

    // Link the trial specifications to the partial p-states definitions.
    if (!status.isSerious())
        status.push(reader.resolve(m_VbiosPStateMap));

    // Save off the trial specifications (including the partial p-state definitions).
    if (!status.isSerious())
        m_TrialList = reader.getTrialSpecs();

    return status;
}

//!
//! \brief      Check and/or set features
//!
//! \details    The features in the trial specifications are defined using the
//!             'enable', 'disable', and 'ramtype' fields.
//!
//! \pre        m_TrialList contains the list of trials with their p-state
//!             references resolved (unless there are serious errors).
//!
//! \post       m_Status contains any applicable exceptions.
//!
//! \todo       The logic herein is better placed in the TrialSpec and related classes.
//!
ExceptionList UniversalClockTest::CheckFeatures()
{
    TrialSpec::List::const_iterator pTrial;
    TrialSpec::FeatureField::Vector::const_iterator  pFeature;
    LwRmPtr pLwRm;
    ExceptionList status;
    RC rc;

    // Test the features for every trial that has been defined.
    for (pTrial = ((const TrialSpec::List &) m_TrialList).begin();
         pTrial!= ((const TrialSpec::List &) m_TrialList).end(); ++pTrial)
    {
        for (pFeature = pTrial->featureVector.begin();
             pFeature!= pTrial->featureVector.end(); ++pFeature)
        {
            // Set the thermal slowdown mode
            if (pFeature->isSpecified(TrialSpec::FeatureField::ThermalSlowdown)
                && !m_DryRun)
            {
                LW2080_CTRL_THERMAL_SYSTEM_EXELWTE_V2_PARAMS thermalCommands = {0};

                thermalCommands.clientAPIVersion          = THERMAL_SYSTEM_API_VER;
                thermalCommands.clientAPIRevision         = THERMAL_SYSTEM_API_REV;
                thermalCommands.clientInstructionSizeOf   = sizeof(LW2080_CTRL_THERMAL_SYSTEM_INSTRUCTION);
                thermalCommands.instructionListSize       = 1;
                thermalCommands.instructionList[0].opcode = LW2080_CTRL_THERMAL_SYSTEM_SET_SLOWDOWN_STATE_OPCODE;
                thermalCommands.instructionList[0].operands.setThermalSlowdownState.slowdownState =
                    (pFeature->isEnabled(TrialSpec::FeatureField::ThermalSlowdown)?
                    LW2080_CTRL_THERMAL_SLOWDOWN_ENABLED: LW2080_CTRL_THERMAL_SLOWDOWN_DISABLED_ALL);

                // Ask RM about the feature.
                rc = pLwRm->ControlBySubdevice(m_pSubdevice, LW2080_CTRL_CMD_THERMAL_SYSTEM_EXELWTE_V2,
                    &thermalCommands, sizeof (thermalCommands));

                // We're okay if it's not supported if we asked to disable
                if ((rc != OK || thermalCommands.successfulInstructions != 1) &&
                    (rc != RC::LWRM_NOT_SUPPORTED || (pFeature->isEnabled(TrialSpec::FeatureField::ThermalSlowdown))))
                {
                    status.push(Exception(rc, "LW2080_CTRL_CMD_THERMAL_SYSTEM_EXELWTE_V2",
                        *pFeature, __PRETTY_FUNCTION__, Exception::Unsupported));
                }
            }

            // Set the idle slowdown mode
            if (pFeature->isSpecified(TrialSpec::FeatureField::DeepIdle)
                && !m_DryRun)
            {
                LW2080_CTRL_GPU_SET_DEEP_IDLE_MODE_PARAMS idleParams = {0};
                idleParams.mode = (pFeature->isEnabled(TrialSpec::FeatureField::DeepIdle)?
                    LW2080_CTRL_GPU_SET_DEEP_IDLE_MODE_ON:
                    LW2080_CTRL_GPU_SET_DEEP_IDLE_MODE_OFF);

                // Ask RM about the feature.
                rc = pLwRm->ControlBySubdevice(m_pSubdevice, LW2080_CTRL_CMD_GPU_SET_DEEP_IDLE_MODE,
                    &idleParams, sizeof (idleParams));

                // We're okay if it's not supported if we asked to disable
                if (rc != OK &&
                   (rc != RC::LWRM_NOT_SUPPORTED || (pFeature->isEnabled(TrialSpec::FeatureField::DeepIdle))))
                {
                    status.push(Exception(rc, "LW2080_CTRL_CMD_GPU_SET_DEEP_IDLE_MODE",
                        *pFeature, __PRETTY_FUNCTION__, Exception::Unsupported));
                }
            }

            // Check for broken framebuffer
            if (pFeature->isSpecified(TrialSpec::FeatureField::BrokenFB))
            {
                LW2080_CTRL_FB_GET_INFO_PARAMS fbGetInfoParams;
                LW2080_CTRL_FB_INFO            fbInfo;

                fbGetInfoParams.fbInfoListSize = 1;
                fbGetInfoParams.fbInfoList     = (LwP64) &fbInfo;

                // See if 2T mode is enabled in the VBIOS.
                fbInfo.index = LW2080_CTRL_FB_INFO_INDEX_FB_IS_BROKEN;
                rc = pLwRm->ControlBySubdevice(m_pSubdevice, LW2080_CTRL_CMD_FB_GET_INFO,
                    &fbGetInfoParams, sizeof(fbGetInfoParams));

                // Any error in the RMAPI is a problem.
                if (rc != OK)
                {
                    status.push(Exception(rc, "LW2080_CTRL_CMD_FB_GET_INFO",
                        *pFeature, __PRETTY_FUNCTION__, Exception::Unsupported));
                }

                //
                // VBIOS must match the trial specification.  If this is a dry-run,
                // then there is no reason to escalate the status level.
                //
                else if (pFeature->isDisabled(TrialSpec::FeatureField::BrokenFB) != !fbInfo.data)
                {
                    status.push(Exception(pTrial->name.defaultTo("<anonymous>") +
                        ": Trial requires that the framebuffer is" +
                        (pFeature->isEnabled(TrialSpec::FeatureField::BrokenFB)? "" : " not") +
                        " broken", *pFeature, __PRETTY_FUNCTION__,
                        (m_DryRun? Exception::DryRun: Exception::Unsupported)));
                }
            }

            // Check for SDDR4 2T training mode
            if (pFeature->isSpecified(TrialSpec::FeatureField::Training2T))
            {
                LW2080_CTRL_FB_GET_INFO_PARAMS fbGetInfoParams;
                LW2080_CTRL_FB_INFO            fbInfo;

                fbGetInfoParams.fbInfoListSize = 1;
                fbGetInfoParams.fbInfoList     = (LwP64) &fbInfo;

                // See if 2T mode is enabled in the VBIOS.
                fbInfo.index = LW2080_CTRL_FB_INFO_INDEX_TRAINIG_2T;
                rc = pLwRm->ControlBySubdevice(m_pSubdevice, LW2080_CTRL_CMD_FB_GET_INFO,
                    &fbGetInfoParams, sizeof(fbGetInfoParams));

                // Any error in the RMAPI is a problem.
                if (rc != OK)
                {
                    status.push(Exception(rc, "LW2080_CTRL_CMD_FB_GET_INFO",
                        *pFeature, __PRETTY_FUNCTION__, Exception::Unsupported));
                }

                //
                // VBIOS must match the trial specification.  If this is a dry-run,
                // then there is no reason to escalate the status level.
                //
                else if (pFeature->isDisabled(TrialSpec::FeatureField::Training2T) != !fbInfo.data)
                {
                    status.push(Exception(pTrial->name.defaultTo("<anonymous>") +
                        ": Trial requires that Training 2T mode is "+
                        (pFeature->isEnabled(TrialSpec::FeatureField::Training2T)? "enabled" : "disabled") +
                        " in VBIOS.", *pFeature, __PRETTY_FUNCTION__,
                        (m_DryRun? Exception::DryRun: Exception::Unsupported)));
                }
            }
        }

        // Check for RAM type.
        // TODO: Consider moving this logic to TrialSpec::RamTypeField
        if (!pTrial->ramtype.isAny())
        {
            LW2080_CTRL_FB_GET_INFO_PARAMS fbGetInfoParams;
            LW2080_CTRL_FB_INFO            fbInfo;

            fbGetInfoParams.fbInfoListSize = 1;
            fbGetInfoParams.fbInfoList     = (LwP64) &fbInfo;

            fbInfo.index = LW2080_CTRL_FB_INFO_INDEX_RAM_TYPE;
            rc = pLwRm->ControlBySubdevice(m_pSubdevice, LW2080_CTRL_CMD_FB_GET_INFO,
                &fbGetInfoParams, sizeof(fbGetInfoParams));

            if (rc != OK)
            {
                status.push(Exception(rc, "LW2080_CTRL_CMD_FB_GET_INFO",
                    pTrial->ramtype, __PRETTY_FUNCTION__, Exception::Unsupported));
            }

            //
            // If this RAM type is not supported, we have a problem unless this
            // is a dry-run.  In that case, we report an exception, but do not
            // escalate the status level.
            //
            else if (!(pTrial->ramtype.bitmap & BIT(fbInfo.data)))
            {
                status.push(Exception(pTrial->name.defaultTo("<anonymous>") +
                    ": Trial does not support " + TrialSpec::RamTypeField::typeString(BIT(fbInfo.data)) +
                    ". Supported types: " + pTrial->ramtype.typeString(),
                    pTrial->ramtype, __PRETTY_FUNCTION__,
                    (m_DryRun? Exception::DryRun: Exception::Unsupported)));
            }
        }
    }

    // Done
    return status;
}

//!
//! \brief      Get clock counter frequency for the requested clock domain.
//!             LW2080_CTRL_CMD_CLK_MEASURE_FREQ is called for the pre-volta Gpus
//!             whereas LW2080_CTRL_CMD_CLK_CNTR_MEASURE_AVG_FREQ for volta+ Gpus.
//!
//! \param[in]  freqDomain   Clock domain
//! \param[out] freqKHz      Measured frequency in KHz.
//!
//! \return     Exception if any
//!
Exception UniversalClockTest::GetMeasuredFreq(FreqDomain freqDomain, UINT32 *freqKHz)
{
    LwRmPtr pLwRm;
    RC      rc;

    // Colwert into LW2080 clock domain.
    UINT32 ctrlClkDomain = BIT(freqDomain);

    // Initialize frequency to Zero.
    *freqKHz = 0;

    // Fetch free-running clock counter frequency on Volta+
    if (m_pSubdevice->DeviceId() >= Gpu::GV100)
    {
        LW2080_CTRL_CLK_CNTR_MEASURE_AVG_FREQ_PARAMS clkCntrMeasureAvgFreqParams;

        // Initialize input parameters.
        memset(&clkCntrMeasureAvgFreqParams, 0, sizeof(clkCntrMeasureAvgFreqParams));
        clkCntrMeasureAvgFreqParams.clkDomain   = ctrlClkDomain;
        clkCntrMeasureAvgFreqParams.b32BitCntr  = LW_FALSE;
        clkCntrMeasureAvgFreqParams.numParts    = 1;
        clkCntrMeasureAvgFreqParams.parts[0].partIdx =
            LW2080_CTRL_CLK_DOMAIN_PART_IDX_UNDEFINED;

        // Take first sample
        rc = pLwRm->ControlBySubdevice(m_pSubdevice,
                LW2080_CTRL_CMD_CLK_CNTR_MEASURE_AVG_FREQ,
                &clkCntrMeasureAvgFreqParams,
                sizeof(clkCntrMeasureAvgFreqParams));
        if (rc != OK)
        {
            return Exception(rc, "LW2080_CTRL_CMD_CLK_CNTR_MEASURE_AVG_FREQ",
                __PRETTY_FUNCTION__);
        }

        // Wait 5ms for re-sampling
        Utility::Delay(5000);

        // Take second sample
        rc = pLwRm->ControlBySubdevice(m_pSubdevice,
                LW2080_CTRL_CMD_CLK_CNTR_MEASURE_AVG_FREQ,
                &clkCntrMeasureAvgFreqParams,
                sizeof(clkCntrMeasureAvgFreqParams));
        if (rc != OK)
        {
            return Exception(rc, "LW2080_CTRL_CMD_CLK_CNTR_MEASURE_AVG_FREQ",
                __PRETTY_FUNCTION__);
        }
        else
        {
            *freqKHz = clkCntrMeasureAvgFreqParams.parts[0].freqkHz;
        }
    }
    else
    {
        LW2080_CTRL_CLK_MEASURE_FREQ_PARAMS clkMeasureFreqParams;

        clkMeasureFreqParams.clkDomain = ctrlClkDomain;
        rc = pLwRm->ControlBySubdevice(m_pSubdevice, LW2080_CTRL_CMD_CLK_MEASURE_FREQ,
           &clkMeasureFreqParams, sizeof(clkMeasureFreqParams));

        if (rc != OK)
        {
            return Exception(rc, "LW2080_CTRL_CMD_CLK_MEASURE_FREQ",
                __PRETTY_FUNCTION__);
        }
        else
        {
            *freqKHz = clkMeasureFreqParams.freqKHz;
        }
    }

    // All good, so return a NULL exception
    return Exception();
}

//!
//! \brief      Get clock information and write it into result.
//!             Upon return, 'result.actual' and 'result.measured'
//!             have been set with clock information.
//!
//!             If result.target is not set, result.actual is populated
//!             with all programmable clock domains
//!
//! \todo       Voltage
//!
//! \param[in\out] result   Target P-state to check current programming against
//!
//! \return     Exception if any
//!
ExceptionList UniversalClockTest::GetClockProgramming(Result &result, bool bCheckMeasured)
{
    LwRmPtr pLwRm;
    rmt::Vector<LW2080_CTRL_CLK_INFO> clkInfos;
    rmt::Vector<LW2080_CTRL_CLK_INFO>::const_iterator pClkInfos;
    LW2080_CTRL_CLK_GET_INFO_PARAMS clkGetInfoParams;
    FreqDomainBitmap freqDomainSet;
    FreqDomain freqDomain;
    RC rc;
    Exception exception;
    ExceptionList status;
    FreqDomainBitmap d;
    UINT32 size = 0;
    UINT32 i;
    // Flag indicating if the result object has a target fullpstate
    bool bHasTarget = result.target.isValid();

    Printf(Tee::PriHigh,
        "\n== Getting Clock Information  (%s)\n",
        rmt::String::function(__PRETTY_FUNCTION__).c_str());

    // If there is no target full pstate, we return all programmable domains
    if (!bHasTarget)
    {
        // Get all the clock domains that are programmable
        LW2080_CTRL_CLK_GET_DOMAINS_PARAMS clkGetDomainParams;
        memset(&clkGetDomainParams, 0, sizeof(clkGetDomainParams));

        clkGetDomainParams.clkDomainsType = LW2080_CTRL_CLK_DOMAINS_TYPE_PROGRAMMABALE_ONLY;

        status.push(Exception(pLwRm->ControlBySubdevice(m_pSubdevice,
           LW2080_CTRL_CMD_CLK_GET_DOMAINS, &clkGetDomainParams,
           sizeof(clkGetDomainParams)),
           "LW2080_CTRL_CMD_CLK_GET_DOMAINS", __PRETTY_FUNCTION__));

        freqDomainSet = clkGetDomainParams.clkDomains;
    }
    else
    {
        freqDomainSet = result.target.freqDomainSet();
    }

    // Number of frequency domains
    size = freqDomainSet;
    NUMSETBITS_32(size);

    // Initialize the parameter block.
    clkInfos.resize(size);
    clkInfos.memset();

    // Setup the CLK_INFO struct to the correct sizes and domains
    clkGetInfoParams.flags              = 0;
    clkGetInfoParams.clkInfoListSize    = size;
    clkGetInfoParams.clkInfoList        = LW_PTR_TO_LwP64(clkInfos.c_array());

    // Initialize each param object with the domain it represents.
    i = 0;
    while (freqDomainSet)
    {
        d = LOWESTBIT(freqDomainSet);
        clkInfos[i++].clkDomain = d;
        freqDomainSet &= ~d;
    }
    MASSERT(i == size);

    // Restor freqDomainSet from the target full pstate if we had one set
    if(bHasTarget)
        freqDomainSet = result.target.freqDomainSet();

    //
    // Use LW2080_CTRL_CMD_CLK_GET_INFO to get the current programming
    // On error, include trial spec location (if any) in the exception.
    //
    exception = Exception(pLwRm->ControlBySubdevice(m_pSubdevice, LW2080_CTRL_CMD_CLK_GET_INFO,
        &clkGetInfoParams, sizeof(clkGetInfoParams)),
        "LW2080_CTRL_CMD_CLK_GET_INFO", __PRETTY_FUNCTION__);
    if (exception)
        status.push(exception);

    // Verify clock programming information match that of the PState.
    else for (pClkInfos = clkInfos.begin(); pClkInfos != clkInfos.end(); ++pClkInfos)
    {
        // Get the domain described by this entry.
        freqDomain = BIT_IDX_32(pClkInfos->clkDomain);
        MASSERT(freqDomain < FreqDomain_Count);

        // The frequency domain is assumed to be one bit per 32-bit entry.
        MASSERT(BIT32(freqDomain) == pClkInfos->clkDomain);

        // Additional checks should be performed if we had a target
        if(bHasTarget)
        {
            // The RMAPI should not give us a domain we did not request.
            MASSERT(result.target.freq[freqDomain]);

            // The RMAPI should not give us the same domain more than once.
            MASSERT(freqDomainSet & BIT32(freqDomain));

            // Flag that we have checked this clock domain.
            freqDomainSet &= ~BIT32(freqDomain);
        }

        // Move the data into the result structure
        result.actual.freq[freqDomain]   = pClkInfos->actualFreq;
        result.actual.source[freqDomain] = pClkInfos->clkSource;
        result.actual.flag[freqDomain]   = pClkInfos->flags;

        //Notes: The check originally done here is moved to uct::Result::checkResult()

        //
        // TODO_CLK2: Add logic so that, for chips (or domains) on Clocks 2.x,
        // we make sure pClkInfos->clkSource matches 'result.target'.  We would
        // have to add a 'source' field to the various p-state classes.
        // You can tell that RM uses Clocks 2.x if pClkInfos->clkSource is nonzero.
        // To be more precise, it Clocks 2.x reports zero for 'source' (or 'flags'),
        // then it means that the clkTypicalPath array is not proper.
        //
        if (bCheckMeasured && bHasTarget)
        {
            status.push(GetMeasuredFreq(freqDomain, &result.measured.freq[freqDomain]));
        }
    }

    return status;
}

//!
//! \brief      Colwert the given LW2080 clock domain to the equivalent
//!             LW2080 NAFLL id mask.
//!
//! \param[in]  clkDomain   @ref LW2080_CTRL_CLK_xyz
//!
//! \return     Corresponding NAFLL id mask @ref LW2080_CTRL_CLK_NAFLL_ID_xyz
//!
UINT32 UniversalClockTest::clkDomainToNafllIdMask(UINT32 clkDomain)
{
    UINT32 nafllIdx    = UINT_MAX;
    UINT32 nafllIdMask = LW2080_CTRL_CLK_NAFLL_MASK_UNDEFINED;

    FOR_EACH_INDEX_IN_MASK(32, nafllIdx, m_ClkNafllDevicesInfoParams.super.objMask.super.pData[0])
    {
        if (clkDomain == m_ClkNafllDevicesInfoParams.devices[nafllIdx].clkDomain)
        {
            nafllIdMask |= BIT(nafllIdx);
        }
    } FOR_EACH_INDEX_IN_MASK_END;

    return nafllIdMask;
}

//!
//! \brief      Get the volt rails info and fill in the given volt list. In case
//!             the volt rails call is not supported just hardcode the values.
//!
//! \param[in]  pVoltRailList Volt rail list to fill
//! \param[in]  voltageuV     voltage in uV
//!
void UniversalClockTest::GetVoltRailList
(
    LW2080_CTRL_PERF_CHANGE_SEQ_CHANGE_INPUT *pChangeInput,
    CLK_UCT_VOLT_LIST                        *pVoltList
)
{
    LwRmPtr                                  pLwRm;
    LW2080_CTRL_VOLT_VOLT_RAILS_INFO_PARAMS  voltRailsInfo;
    UINT32                                   voltIdx = 0;
    RC                                       rc;
    UINT32                                   uctVoltIdx;

    // Get the voltage rail info
    memset(&voltRailsInfo, 0, sizeof(voltRailsInfo));
    rc = pLwRm->ControlBySubdevice(m_pSubdevice,
                LW2080_CTRL_CMD_VOLT_VOLT_RAILS_GET_INFO,
               &voltRailsInfo,
                sizeof(voltRailsInfo));
    if (rc != OK)
    {
        Printf(Tee::PriHigh, "\nWarning: LW2080_CTRL_CMD_VOLT_VOLT_RAILS_GET_INFO not supported on this  Platform/Chip. Hardcoding both rails\n\n");

        // Hardcode the voltList when volt rails call not supported
        pChangeInput->voltRailsMask.super.pData[0]     = 3;         //Bits 0 & 1 both set
        pChangeInput->volt[0].voltageuV                = pVoltList->voltInfo[0].voltUv;
        pChangeInput->volt[0].voltageMinNoiseUnawareuV = pVoltList->voltInfo[0].voltUv;
        pChangeInput->volt[1].voltageuV                = pVoltList->voltInfo[0].voltUv;
        pChangeInput->volt[1].voltageMinNoiseUnawareuV = pVoltList->voltInfo[0].voltUv;
    }
    else
    {
        // Set the same voltage for all available voltage rails
        FOR_EACH_INDEX_IN_MASK(32, voltIdx, voltRailsInfo.super.objMask.super.pData[0])
        {
            for (uctVoltIdx=0; uctVoltIdx < LW2080_CTRL_VOLT_VOLT_DOMAIN_MAX_ENTRIES; uctVoltIdx++)
            {
                //
                // Add the voltage request to change sequencer input only if it's
                // set in the target volt list.
                //
                if (pVoltList->voltInfo[uctVoltIdx].voltDomain == voltRailsInfo.rails[voltIdx].super.type)
                {
                    pChangeInput->volt[voltIdx].voltageuV                 = pVoltList->voltInfo[uctVoltIdx].voltUv;
                    pChangeInput->volt[voltIdx].voltageMinNoiseUnawareuV  = pVoltList->voltInfo[uctVoltIdx].voltUv;
                    pChangeInput->voltRailsMask.super.pData[0]           |= BIT(voltIdx);
                    break;
                }
            }
        }
        FOR_EACH_INDEX_IN_MASK_END;
    }

    return;
}

UINT32 UniversalClockTest::GetClockHalIdxFromDomain
(
    UINT32 clkDomain
)
{
    UINT32 clkIdx;
    FOR_EACH_INDEX_IN_MASK(32, clkIdx, clkDomainsInfo.super.objMask.super.pData[0])
    {
        if (clkDomainsInfo.domains[clkIdx].domain == clkDomain)
        {
            return clkIdx;
        }
    }
    FOR_EACH_INDEX_IN_MASK_END;
    return CLK_IDX_ILWALID;
}

//------------------------------------------------------------------------------
// JS Linkage
//------------------------------------------------------------------------------

//! \brief Linkage to JavaScript
//!
//! Setup the JS hierarchy to match the C++ one.  Don't worry about the details
//! here, you probably don't care.  :-)
//------------------------------------------------------------------------------
JS_CLASS_INHERIT(UniversalClockTest, RmTest,
    " UniversalClockTest-Runs tests based on specifications in a Clock Test Profile covering basic clock testing");

CLASS_PROP_READWRITE(UniversalClockTest, EntryCtpFilename, string, "Clock Test Profile (ctp) filename");
CLASS_PROP_READWRITE(UniversalClockTest, Exclude, string, "Clock domains to exclude from trials");
CLASS_PROP_READWRITE(UniversalClockTest, DryRun, bool, "Do not run any trials but check the syntax of Clock Test Profile (ctp) files.");
CLASS_PROP_READWRITE(UniversalClockTest, ExitOnFail, bool, "Exit on first failure instead of continuing the trial");

