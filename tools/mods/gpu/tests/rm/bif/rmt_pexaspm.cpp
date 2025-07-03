/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2012-2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */


//!
//! \file rmt_pexaspm.cpp
//! \brief RM-Test to perform a ASPM check.
//! This test is primarily written to replace PEX_ASPM manual tests.
//! 1. Use either PEX_ASPM control register or Pstate switch to change ASPM settings.
//! 2. Verify ASPM setting changes by check correspounding counter register.
//!
//! note: This test are testing L0s&L1 and deepl1 separately, based on deepl1 flag passed in command line.
//!

#include "core/include/memcheck.h"
#include "device/interface/pcie.h"
#include "rmt_pextests.h"

#include <map>
#include <set>
#include <string> // Only use <> for built-in C++ stuff
#include <vector>

#define L0S_AND_L1                  3
#define COUNTER_MAX                 0xffff
#define SLEEP_INTERVAL_MS           50        // sleep interval for checking pstate switch

#define DEEPL1_SETTING(val)         DRF_VAL(2080_CTRL, _PERF_PSTATE20_FLAGS, _PCIECAPS_DEEPL1, val)
#define L0S_L1_SETTING(val)         DRF_VAL(2080_CTRL, _PERF_PSTATE20_FLAGS, _PCIECAPS_L0S_L1, val)
#define L0S_ENABLE(val)             FLD_TEST_DRF(2080_CTRL, _PERF_PSTATE20_FLAGS, _PCIECAPS_L0S, _ENABLE, val)
#define L1_ENABLE(val)              FLD_TEST_DRF(2080_CTRL, _PERF_PSTATE20_FLAGS, _PCIECAPS_L1, _ENABLE, val)
#define DEEPL1_ENABLE(val)          FLD_TEST_DRF(2080_CTRL, _PERF_PSTATE20_FLAGS, _PCIECAPS_DEEPL1, _ENABLE, val)
// Sync with  objcl.h
#define CL_PCIE_LINK_CAP_ASPM_L1_BIT BIT(11)
// Sync with ctrl2080perf.h
#define LW2080_CTRL_PERF_PSTATE20_FLAGS_PCIECAPS_L0S_L1 2:1

class PEX_ASPMTest : public RmTest
{
public:
    PEX_ASPMTest();
    virtual ~PEX_ASPMTest();

    virtual string IsTestSupported();

    virtual RC Setup();
    virtual RC Run();
    virtual RC Cleanup();

    SETGET_PROP(dynamicTest, UINT32);
    SETGET_PROP(deepl1, UINT32);
    SETGET_PROP(sleepIntervalMS, UINT32);

private:
    GpuSubdevice  *m_pSubdevice;
    Perf          *m_pPerf;
    LwBool         m_dynamicTest;
    LwBool         m_deepl1;
    LwU32          m_sleepIntervalMS;
    LwU32          m_linkCtrlReg;
    LwU32          m_CYAMaskReg;
    LwU32          m_blkcgReg;
    LwU32          m_azaliaLinkCtrlReg;
    LwU32          m_azaliaCYAMaskReg;
    LwU32          m_savedASPMSetting;

    vector<PstateCapInfo> m_pstatesCapInfo;
    vector<PstateCapInfo> m_workingSet;

    RC CheckASPMByRegister();
    RC CheckASPMByPstateSwitch();
    RC CheckRootPortASPMSetting();
    RC CheckRMRegistrySetting();
    RC SaveOffPstateASPMSetting();
    RC RestorePstateASPMSetting();

    RC ChangeASPMSetting(LwU32 pstate, LwU32 aspmSetting);
    RC VerifyASPMStatus(LwU32 capInfo);
    RC CheckCounter(LwBool enable, LwU16 prevCount, LwU16 lwrrCount);
    RC EnableASPMRegisters();
    RC RestoreASPMRegisters();
};

//! \brief Conscructor of PEX_ASPMTest
//------------------------------------------------------------------------------
PEX_ASPMTest::PEX_ASPMTest() :
    m_pSubdevice(NULL),
    m_pPerf(NULL),
    m_dynamicTest(false),
    m_deepl1(false),
    m_sleepIntervalMS(0),
    m_linkCtrlReg(0),
    m_CYAMaskReg(0),
    m_blkcgReg(0),
    m_azaliaLinkCtrlReg(0),
    m_azaliaCYAMaskReg(0),
    m_savedASPMSetting(0)
{
    SetName("PEX_ASPMTest");
}

//! \brief Destructor of PEX_ASPMTest
//!
//!     Destructor of PEX_ASPMTest
//!     Restore L0s and L1 public registers.
//!
//! \sa Cleanup
//------------------------------------------------------------------------------
PEX_ASPMTest::~PEX_ASPMTest()
{

}

//! \brief IsTestSupported Function
//!
//! Check whether this test is supported.
//!
//!     1. Check where there chip family supported.
//!     2. If this dynamic test flag has been set, check whether VBIOS is valid.
//!
//! \return RC::OK if the test can be run in the current environment,
//!         skip reason otherwise
//------------------------------------------------------------------------------
string PEX_ASPMTest::IsTestSupported()
{
    if (!m_pSubdevice)
        m_pSubdevice = GetBoundGpuSubdevice();
    if (!m_pPerf)
        m_pPerf = m_pSubdevice->GetPerf();

    if (Platform::GetSimulationMode() == Platform::Fmodel ||
        Platform::GetSimulationMode() == Platform::RTL)
    {
        return "PEX_ASPMTest does not support Fmodel and RTL";
    }

    //
    // Root port ASPM setting should not be checked on emulation, 
    // as BR03 bridge that connects to rootport doesn't support ASPM.
    // Check bug id: 200551864 for details
    //
    if (!GetBoundGpuSubdevice()->IsEmulation())
    {
       if (OK != CheckRootPortASPMSetting())
            return "ASPM is not supported on by root port!";
    }

    // If it's dynamic pex aspm test, we need to check vbios information.
    if (m_deepl1 || m_dynamicTest)
    {
        if (OK != CheckRMRegistrySetting())
            return "ASPM is not supported on current setting!";

        if (OK != PEXTests::FectchPstatesCapInfo(m_pSubdevice, m_pstatesCapInfo))
            return "Dynamic P-states is not supported or not enabled.";
    }
    return RUN_RMTEST_TRUE;
}

//! \brief Setup Function
//!
//!     Enable L0s and L1 public registers.
//!
//! \return Should always return RM::OK
//------------------------------------------------------------------------------
RC PEX_ASPMTest::Setup()
{
    RC rc;
    if (!m_pSubdevice)
        m_pSubdevice = GetBoundGpuSubdevice();
    if (!m_pPerf)
        m_pPerf = m_pSubdevice->GetPerf();

    CHECK_RC(EnableASPMRegisters());
    SaveOffPstateASPMSetting();
    // Ask our base class(es) to setup internal state based on JS settings
    CHECK_RC(InitFromJs());
    return rc;
}

//! \brief Run Function
//!
//! 1. If dynamic flag and deepl1 flag is set, check L0s and L1 by PCIE control register
//! 2. If not,  check L0s and L1 by switching pstate in workingset
//!
//! \return OK if the tests passed, specific RC if failed
//------------------------------------------------------------------------------
RC PEX_ASPMTest::Run()
{
    RC rc;
    if (!m_dynamicTest && !m_deepl1)
    {
        // Change L0s and L1 settigns by setting registers
        CHECK_RC(CheckASPMByRegister());
    }
    else
    {
        // Change L0s and L1 settigns by switching pstate.
        CHECK_RC(CheckASPMByPstateSwitch());
    }
    return rc;
}

//! \brief Cleanup function
//!     Restore L0s and L1 public registers.
//!     cleanup before descructor
//------------------------------------------------------------------------------
RC PEX_ASPMTest::Cleanup()
{
    StickyRC firstRc;
    // Restore L0s and L1 public registers.
    firstRc = RestoreASPMRegisters();
    firstRc = RestorePstateASPMSetting();
    return firstRc;
}

//------------------------------------------------------------------------------
// Private Member Functions
//------------------------------------------------------------------------------
//!\brief  CheckASPMByRegister funciton
//!
//!  1. For each possible cap flag set ASPM register correspoundingly.
//!  2. Verify counter changes.
//!
//! \return OK if the tests passed, specific RC if failed
//------------------------------------------------------------------------------
RC PEX_ASPMTest::CheckASPMByRegister()
{
    RC rc;
    LwU32 i;
    LwU32 tempCap;
    LwU32 regVal = 0;
    vector<LwU32> workingSet;
    const Pcie* pPcie = m_pSubdevice->GetInterface<Pcie>();
    // Set up working set.
    workingSet.push_back(
        LW_XVE_LINK_CONTROL_STATUS_ACTIVE_STATE_LINK_PM_CONTROL_L1_DISABLE_L0S_DISABLE);
    workingSet.push_back(
        LW_XVE_LINK_CONTROL_STATUS_ACTIVE_STATE_LINK_PM_CONTROL_L1_DISABLE_L0S_ENABLE);
    workingSet.push_back(
        LW_XVE_LINK_CONTROL_STATUS_ACTIVE_STATE_LINK_PM_CONTROL_L1_ENABLE_L0S_DISABLE);
    workingSet.push_back(
        LW_XVE_LINK_CONTROL_STATUS_ACTIVE_STATE_LINK_PM_CONTROL_L1_ENABLE_L0S_ENABLE);

    for (i = 0; i < workingSet.size(); i++)
    {
        // Change ASPM setting.
        CHECK_RC(Platform::PciRead32(pPcie->DomainNumber(), pPcie->BusNumber(),
                                     pPcie->DeviceNumber(),
                                     pPcie->FunctionNumber(),
                                     LW_XVE_LINK_CONTROL_STATUS, &regVal));
        regVal = FLD_SET_DRF_NUM(_XVE, _LINK_CONTROL_STATUS, _ACTIVE_STATE_LINK_PM_CONTROL,
                                 workingSet[i], regVal);
        CHECK_RC(Platform::PciWrite32(pPcie->DomainNumber(), pPcie->BusNumber(),
                                      pPcie->DeviceNumber(),
                                      pPcie->FunctionNumber(),
                                      LW_XVE_LINK_CONTROL_STATUS, regVal));

        // Verify ASPM changes.
        tempCap = FLD_SET_DRF_NUM(2080, _CTRL_PERF_PSTATE20_FLAGS,
                                  _PCIECAPS_L0S_L1, workingSet[i], 0);
        Printf(Tee::PriLow, "Changing to ASPM setting, L0s: %s, L1: %s.\r\n",
               L0S_ENABLE(tempCap)? "ENABLE" : "DISABLE",
               L1_ENABLE(tempCap)? "ENABLE" : "DISABLE");

        CHECK_RC_MSG(VerifyASPMStatus(tempCap), "L0s and L1 counter verification failed");
    }
    return rc;
}

//! \brief CheckASPMByPstateSwitch function
//!
//! Change pstate and check correspounding cournter register to veryfy ASPM settings.
//! 1. Set to NO_ASPM pstate and record counter value.
//! 2. Change to L0s pstate. Check whether L0s counter increased or not.
//! 3. Change to L1 Pstate. Check whether L1 counter increased or not.
//!
//! \return OK if the tests passed, specific RC if failed
//------------------------------------------------------------------------------
RC PEX_ASPMTest::CheckASPMByPstateSwitch()
{
    RC rc;
    LwU32 i, regVal, aspmSetting;
    vector <LwU32> aspmSettings;

    // Populate working set with different ASPM settings
    if (m_deepl1)
    {
        aspmSetting = 0;
        aspmSettings.push_back(aspmSetting);
        aspmSetting = FLD_SET_DRF(2080, _CTRL_PERF_PSTATE20_FLAGS, _PCIECAPS_L0S, _ENABLE,
                                  aspmSetting);
        aspmSetting = FLD_SET_DRF(2080, _CTRL_PERF_PSTATE20_FLAGS, _PCIECAPS_L1, _ENABLE,
                                  aspmSetting);
        aspmSetting = FLD_SET_DRF(2080, _CTRL_PERF_PSTATE20_FLAGS, _PCIECAPS_DEEPL1, _ENABLE,
                                  aspmSetting);
        aspmSettings.push_back(aspmSetting);
    }
    else
    {
        // L0s enable && L1 disableS
        aspmSetting = 0;
        aspmSettings.push_back(aspmSetting);
        aspmSetting = FLD_SET_DRF(2080, _CTRL_PERF_PSTATE20_FLAGS, _PCIECAPS_L0S, _ENABLE,
                                  aspmSetting);
        aspmSettings.push_back(aspmSetting);
        aspmSetting = 0;
        aspmSetting = FLD_SET_DRF(2080, _CTRL_PERF_PSTATE20_FLAGS, _PCIECAPS_L1, _ENABLE,
                                  aspmSetting);
        aspmSettings.push_back(aspmSetting);
        aspmSetting = 0;
        aspmSetting = FLD_SET_DRF(2080, _CTRL_PERF_PSTATE20_FLAGS, _PCIECAPS_L0S, _ENABLE,
                                  aspmSetting);
        aspmSetting = FLD_SET_DRF(2080, _CTRL_PERF_PSTATE20_FLAGS, _PCIECAPS_L1, _ENABLE,
                                  aspmSetting);
        aspmSettings.push_back(aspmSetting);
    }

    for (i = 0; i < aspmSettings.size(); i++)
    {
        Printf(Tee::PriLow, "Changing to ASPM:, L0s: %s, L1: %s, Deepl1: %s\r\n",
               L0S_ENABLE(aspmSettings[i])? "ENABLE" : "DISABLE",
               L1_ENABLE(aspmSettings[i])? "ENABLE" : "DISABLE",
               DEEPL1_ENABLE(aspmSettings[i])? "ENABLE" : "DISABLE");

        // Modify P0 ASPM setting.
        CHECK_RC(ChangeASPMSetting(LW2080_CTRL_PERF_PSTATES_P0,
                                   aspmSettings[i]));

        regVal = m_pSubdevice->RegRd32(DEVICE_BASE(LW_PCFG) | LW_XVE_PRIV_XV_0);
        Printf(Tee::PriLow, "Current CYA settings: %x\r\n", regVal);

        CHECK_RC_MSG(VerifyASPMStatus(aspmSettings[i]),
                     "ASPM counters verification failed\r\n");
    }

    return rc;
}

//! \brief CheckRootPortASPMSetting function.
//!      Check whether downstream port to GPU supports L0s and L1.
//! \return OK if root port support L0s and L1.
RC PEX_ASPMTest::CheckRootPortASPMSetting()
{
    RC rc;
    LwRmPtr pLwRm;
    LwRm::Handle hSubdevice = pLwRm->GetSubdeviceHandle(m_pSubdevice);

    LW2080_CTRL_BUS_INFO                  busInfo = {0};
    LW2080_CTRL_BUS_GET_INFO_PARAMS       getBusInfoPara = {0};

    busInfo.index = LW2080_CTRL_BUS_INFO_INDEX_PCIE_DOWNSTREAM_LINK_CAPS;
    getBusInfoPara.busInfoListSize = 1;
    getBusInfoPara.busInfoList = (LwP64)&busInfo;

    CHECK_RC(pLwRm->Control(hSubdevice,
             LW2080_CTRL_CMD_BUS_GET_INFO,
             &getBusInfoPara, sizeof(getBusInfoPara)));

    // Check root port config.
    Printf(Tee::PriLow, "Downstream port Link Cap settings: %x\r\n" ,busInfo.data);
    if (!(CL_PCIE_LINK_CAP_ASPM_L1_BIT & busInfo.data))
    {
        Printf(Tee::PriHigh, "Downstream/Root port does not support L1\r\n");
        rc = RC::SOFTWARE_ERROR;
    }

    return rc;
}

//! \brief CheckRMRegistrySetting function.
//!      Check RM resgistry setting of L0s and L1.
//! \return OK if L0s and L1 are supported, LWRM_NOT_SUPPORTED otherwise.
RC PEX_ASPMTest::CheckRMRegistrySetting()
{
    RC rc;
    LwRmPtr pLwRm;
    LwRm::Handle hSubdeviceDiag = m_pSubdevice->GetSubdeviceDiag();

    LW208F_CTRL_BIF_INFO_PARAMS  getL0Prop = {0};
    LW208F_CTRL_BIF_INFO_PARAMS  getL1Prop = {0};

    // get the current pstate number
    getL0Prop.index = LW208F_CTRL_BIF_INFO_INDEX_L0S_ENABLED;
    getL1Prop.index = LW208F_CTRL_BIF_INFO_INDEX_L1_ENABLED;

    CHECK_RC(pLwRm->Control(hSubdeviceDiag,
             LW208F_CTRL_CMD_BIF_INFO,
             &getL0Prop, sizeof(getL0Prop)));
    CHECK_RC(pLwRm->Control(hSubdeviceDiag,
             LW208F_CTRL_CMD_BIF_INFO,
             &getL1Prop, sizeof(getL1Prop)));

    // Check bif property.
    if (getL0Prop.data || getL1Prop.data)
    {
        Printf(Tee::PriHigh, "ASPM is diabled by OS!\r\n");
        Printf(Tee::PriHigh, "L0s: %s, L1: %s\r\n",
               getL0Prop.data ? "NOTSUPPORTED" : "SUPPORTED",
               getL1Prop.data ? "NOTSUPPORTED" : "SUPPORTED");
        rc = RC::LWRM_NOT_SUPPORTED;
    }

    return rc;
}

//! \brief ChangeASPMSetting function.
//!        Change given pstate ASPM(including deepl1) setting.
//! \return OK, specific RC code otherwise
RC PEX_ASPMTest::ChangeASPMSetting(LwU32 pstate, LwU32 aspmSetting)
{
    RC rc;
    LwRmPtr pLwRm;
    LwRm::Handle hSubdevice = pLwRm->GetSubdeviceHandle(m_pSubdevice);
    LW2080_CTRL_PERF_PSTATE20 pstateInfo = {0};
    LW2080_CTRL_PERF_SET_PSTATES20_DATA_PARAMS setPstateParams = {0};

    aspmSetting = FLD_SET_DRF(2080, _CTRL_PERF_PSTATE20_FLAGS,
                              _PCIECAPS_SET_ASPM, _ENABLE, aspmSetting);
    pstateInfo.pstateID = pstate;
    pstateInfo.flags |= aspmSetting;

    setPstateParams.numPstates = 1;
    setPstateParams.pstate[0] = pstateInfo;

    CHECK_RC(pLwRm->Control(hSubdevice,
             LW2080_CTRL_CMD_PERF_SET_PSTATES20_DATA,
             &setPstateParams, sizeof(setPstateParams)));

    return rc;
}

//! \brief VerifyASPMStatus function.
//!
//! Check L0s, L1, deepL1 status coutners againest L0s, L1 and DEEP L1 settings
//!
//! \return OK, specific RC code otherwise
RC PEX_ASPMTest::VerifyASPMStatus(LwU32 capFlag)
{
    RC rc;
    LwU32 l0sIndex    = 0;
    LwU32 l1Index     = 0;
    LwU32 deepl1Index = 0;

    LwRmPtr pLwRm;
    LwRm::Handle hSubdevice = pLwRm->GetSubdeviceHandle(m_pSubdevice);
    LW2080_CTRL_BUS_GET_PEX_COUNTERS_PARAMS   preCountersParams = {0};
    LW2080_CTRL_BUS_GET_PEX_COUNTERS_PARAMS   lwrrCountersParams = {0};
    LW2080_CTRL_BUS_CLEAR_PEX_COUNTERS_PARAMS clearCountersParams = {0};

    if (m_deepl1)
        // L1 and Deepl1 should be enabled and disabled together
        MASSERT(DEEPL1_ENABLE(capFlag) == L1_ENABLE(capFlag));

    // Prepare counter crl call parameters.
    preCountersParams.pexCounterMask = 0;
    preCountersParams.pexCounterMask |= LW2080_CTRL_BUS_PEX_COUNTER_GPU_XMIT_L0S_ENTRY_COUNT;
    preCountersParams.pexCounterMask |= LW2080_CTRL_BUS_PEX_COUNTER_L1_ENTRY_COUNT;
    preCountersParams.pexCounterMask |= LW2080_CTRL_BUS_PEX_COUNTER_DEEP_L1_ENTRY_COUNT;

    lwrrCountersParams.pexCounterMask = preCountersParams.pexCounterMask;
    clearCountersParams.pexCounterMask = preCountersParams.pexCounterMask;

    // Callwlate counter Index
    l0sIndex = Utility::BitScanForward(LW2080_CTRL_BUS_PEX_COUNTER_GPU_XMIT_L0S_ENTRY_COUNT);
    l1Index = Utility::BitScanForward(LW2080_CTRL_BUS_PEX_COUNTER_L1_ENTRY_COUNT);
    deepl1Index = Utility::BitScanForward(LW2080_CTRL_BUS_PEX_COUNTER_DEEP_L1_ENTRY_COUNT);

    // Get pex L0s L1 and Deepl1 counters.
    CHECK_RC(pLwRm->Control(hSubdevice,
                        LW2080_CTRL_CMD_BUS_GET_PEX_COUNTERS,
                        &preCountersParams, sizeof(preCountersParams)));

    // Make sure current value is not 0xFFFF
    if (preCountersParams.pexCounters[l0sIndex] == COUNTER_MAX ||
        preCountersParams.pexCounters[l1Index] == COUNTER_MAX ||
        preCountersParams.pexCounters[deepl1Index] == COUNTER_MAX)
    {
        Printf(Tee::PriLow, "Reset counter registers\r\n");
        CHECK_RC(pLwRm->Control(hSubdevice,
                        LW2080_CTRL_CMD_BUS_CLEAR_PEX_COUNTERS,
                        &clearCountersParams, sizeof(clearCountersParams)));

        // reread counters.
        CHECK_RC(pLwRm->Control(hSubdevice,
                        LW2080_CTRL_CMD_BUS_GET_PEX_COUNTERS,
                        &preCountersParams, sizeof(preCountersParams)));

        MASSERT(preCountersParams.pexCounters[l0sIndex] != COUNTER_MAX &&
                preCountersParams.pexCounters[l1Index] != COUNTER_MAX &&
                preCountersParams.pexCounters[deepl1Index] != COUNTER_MAX);
    }

    Tasker::Sleep(m_sleepIntervalMS);

    CHECK_RC(pLwRm->Control(hSubdevice,
                            LW2080_CTRL_CMD_BUS_GET_PEX_COUNTERS,
                            &lwrrCountersParams, sizeof(lwrrCountersParams)));

    if (m_deepl1)
    {
        Printf(Tee::PriLow, "Checking Deepl1 counter. \r\n");
        CHECK_RC(CheckCounter(DEEPL1_ENABLE(capFlag),
                              preCountersParams.pexCounters[deepl1Index],
                              lwrrCountersParams.pexCounters[deepl1Index]));
    }
    else
    {
        Printf(Tee::PriLow, "Checking L0s counter. \r\n");
        CHECK_RC(CheckCounter(L0S_ENABLE(capFlag),
                              preCountersParams.pexCounters[l0sIndex],
                              lwrrCountersParams.pexCounters[l0sIndex]));
    }

    Printf(Tee::PriLow, "Checking L1 counter. \r\n");
    CHECK_RC(CheckCounter(L1_ENABLE(capFlag),
                          preCountersParams.pexCounters[l1Index],
                          lwrrCountersParams.pexCounters[l1Index]));

    return rc;
}

//! \brief Verify that counter are changing as expected.
//! \return OK, specific RC code otherwise
RC PEX_ASPMTest::CheckCounter(LwBool enable, LwU16 prevCount, LwU16 lwrrCount)
{
    RC rc;
    if (enable && lwrrCount <= prevCount)
    {
        Printf(Tee::PriHigh, "ASPM has been enabled but counter is not increasing! \r\n");
        Printf(Tee::PriHigh, "Prev Count: %04x Lwrr Count:%04x \r\n", prevCount, lwrrCount);

        rc = RC::SOFTWARE_ERROR;
    }
    else if ( !enable && prevCount != lwrrCount)
    {
        Printf(Tee::PriHigh, "ASPM has not been enabled but counter is changing! \r\n");
        Printf(Tee::PriHigh, "Prev Count: %04x Lwrr Count:%04x \r\n", prevCount, lwrrCount);

        rc = RC::SOFTWARE_ERROR;
    }
    return rc;
}

//! \brief EnableASPMRegisters function.
//!
//!  1. For checkbyregister,  turn off L0s and L1 CYA mask.
//!  2. For checkbypstateswitch, enable L0s and L1 in  XVE link control register.
RC PEX_ASPMTest::EnableASPMRegisters()
{
    RC rc;
    LwU32 regVal = 0;
    const Pcie* pPcie = m_pSubdevice->GetInterface<Pcie>();
    // enable LW_XVE_LINK_CONTROL_STATUS_ACTIVE_STATE_LINK_PM_CONTROL
    if (m_dynamicTest || m_deepl1)
    {
        CHECK_RC(Platform::PciRead32(pPcie->DomainNumber(), pPcie->BusNumber(),
                                     pPcie->DeviceNumber(),
                                     pPcie->FunctionNumber(),
                                     LW_XVE_LINK_CONTROL_STATUS, &regVal));
        Printf(Tee::PriLow, "LW_XVE_LINK_CONTROL_STATUS: %x\r\n", regVal);
        m_linkCtrlReg = regVal;
        regVal = FLD_SET_DRF_NUM(_XVE, _LINK_CONTROL_STATUS, _ACTIVE_STATE_LINK_PM_CONTROL,
                                 L0S_AND_L1, regVal);
        CHECK_RC(Platform::PciWrite32(pPcie->DomainNumber(), pPcie->BusNumber(),
                                      pPcie->DeviceNumber(),
                                      pPcie->FunctionNumber(),
                                      LW_XVE_LINK_CONTROL_STATUS, regVal));

        regVal = m_pSubdevice->RegRd32(LW_XP_PEX_PLL);
        Printf(Tee::PriLow, "Current Deepl1 delay: %x\r\n",
               DRF_VAL(_XP, _PEX_PLL, _PWRDN_SEQ_START_XCLK_DLY, regVal));

        regVal = FLD_SET_DRF_NUM(_XP, _PEX_PLL, _PWRDN_SEQ_START_XCLK_DLY, 0x4000, regVal);
        m_pSubdevice->RegWr32(LW_XP_PEX_PLL, regVal);
    }
    else
    {
        // remove L0s and l1 CYA mask
        regVal = m_pSubdevice->RegRd32(DEVICE_BASE(LW_PCFG) | LW_XVE_PRIV_XV_0);
        Printf(Tee::PriLow, "LW_XVE_PRIV_XV_0: %x\r\n", regVal);
        m_CYAMaskReg = regVal;
        regVal = FLD_SET_DRF(_XVE, _PRIV_XV_0, _CYA_L0S_ENABLE, _ENABLED, regVal);
        regVal = FLD_SET_DRF(_XVE, _PRIV_XV_0, _CYA_L1_ENABLE, _ENABLED, regVal);
        m_pSubdevice->RegWr32(DEVICE_BASE(LW_PCFG) | LW_XVE_PRIV_XV_0, regVal);

        // Enable azalia ASPM
        regVal = m_pSubdevice->RegRd32(DEVICE_BASE(LW_PCFG1) | LW_XVE1_LINK_CONTROL_STATUS) ;
        m_azaliaLinkCtrlReg = regVal;
        regVal = FLD_SET_DRF_NUM(_XVE1, _LINK_CONTROL_STATUS, _ACTIVE_STATE_LINK_PM_CONTROL,
                                 L0S_AND_L1, regVal);
        m_pSubdevice->RegWr32(DEVICE_BASE(LW_PCFG1) | LW_XVE1_LINK_CONTROL_STATUS, regVal);
        regVal = m_pSubdevice->RegRd32(DEVICE_BASE(LW_PCFG1) | LW_XVE1_PRIV_XV_0);
        m_azaliaCYAMaskReg = regVal;
        regVal = FLD_SET_DRF_NUM(_XVE1, _PRIV_XV_0, _CYA_L0S_ENABLE, 0x0, regVal);
        regVal = FLD_SET_DRF_NUM(_XVE1, _PRIV_XV_0, _CYA_L1_ENABLE, 0x0, regVal);
        m_pSubdevice->RegWr32(DEVICE_BASE(LW_PCFG1) | LW_XVE1_PRIV_XV_0, regVal);

        //
        // to check xusb and ppc presence
        // Read LW_PCFG_XVE_XUSB_STATUS and _PPC_STATUS
        // Following assignment is only required in cases where explicity fuse is required for
        // disabling XUSB/PPC and reading throught status registers.
        //
        UINT32 xusb_present = (m_pSubdevice->RegRd32(LW_PCFG1_OFFSET + LW_XVE_XUSB_STATUS) & ENABLED_TRUE); 
        UINT32 ppc_present  = (m_pSubdevice->RegRd32(LW_PCFG1_OFFSET + LW_XVE_PPC_STATUS) & ENABLED_TRUE);

        auto pGpuPcie = m_pSubdevice->GetInterface<Pcie>();
        UINT32 pci_domain_num = pGpuPcie->DomainNumber();
        UINT32 pci_bus_num = pGpuPcie->BusNumber();
        UINT32 pci_dev_num = pGpuPcie->DeviceNumber();

        if (xusb_present == ENABLED_TRUE)
        {
            // set LW_XVE_XUSB_LINK_CONTROL_STATUS_ACTIVE_STATE_LINK_PM_CONTROL (bit 0 and 1) to 1'b1
            Platform::PciWrite32(pci_domain_num,
                                 pci_bus_num,
                                 pci_dev_num,
                                 2, // xusb fn num
                                 LW_XVE_LINK_CONTROL_STATUS, //offset is same for all functions
                                 DRF_NUM(_XVE, _LINK_CONTROL_STATUS, _ACTIVE_STATE_LINK_PM_CONTROL , 3));
        }
        if (ppc_present == ENABLED_TRUE)
        {
            // set LW_XVE_PPC_LINK_CONTROL_STATUS_ACTIVE_STATE_LINK_PM_CONTROL (bit 0 and 1) to 1'b1
            Platform::PciWrite32(pci_domain_num,
                                 pci_bus_num,
                                 pci_dev_num,
                                 3, // ppc fn num
                                 LW_XVE_LINK_CONTROL_STATUS, //offset is same for all functions
                                 DRF_NUM(_XVE, _LINK_CONTROL_STATUS, _ACTIVE_STATE_LINK_PM_CONTROL , 3));
        }

        //
        // Enable ASPM for WINIDLE_CYA
        // In kepler/gk104/dev_lw_xp.h , the ASPM_WINIDLE init value is 0x00
        // whereas in turing/tu102/dev_lw_xp.h , the value is 0xFF
        // hence to enable ASPM for WINIDLE_CYA, set _ASPM_WINIDLE_INIT to 0xFF
        //
        LwU32 ALL_GEN_SPEED_ENABLE = 0xFF;
        regVal = m_pSubdevice->RegRd32(LW_XP_DL_CYA_1(0));
        regVal = FLD_SET_DRF_NUM(_XP, _DL_CYA_1, _ASPM_WINIDLE, ALL_GEN_SPEED_ENABLE, regVal);
        m_pSubdevice->RegWr32(LW_XP_DL_CYA_1(0), regVal);

        // Enable  LW_XP_DL_CYA_1_ASPM_TEST 
        regVal = FLD_SET_DRF(_XP, _DL_CYA_1, _ASPM_TEST, _INIT, regVal);
        m_pSubdevice->RegWr32(LW_XP_DL_CYA_1(0), regVal);
    }

    return rc;
}

//! \brief RestoreASPMRegisters function.
//!
//!  1. For checkbyregister,  turn off L0s and L1 CYA mask.
//!  2. For checkbypstateswitch, enable L0s and L1 in  XVE link control register.
//!
//! \return OK if counter statuses match the setting.
RC PEX_ASPMTest::RestoreASPMRegisters()
{
    const Pcie* pPcie = m_pSubdevice->GetInterface<Pcie>();
    RC rc;

    if (m_dynamicTest || m_deepl1)
    {
        CHECK_RC(Platform::PciWrite32(pPcie->DomainNumber(), pPcie->BusNumber(),
                                      pPcie->DeviceNumber(),
                                      pPcie->FunctionNumber(),
                                      LW_XVE_LINK_CONTROL_STATUS,
                                      m_linkCtrlReg));
    }
    else
    {
        m_pSubdevice->RegWr32(DEVICE_BASE(LW_PCFG) | LW_XVE_PRIV_XV_0, m_CYAMaskReg);
        m_pSubdevice->RegWr32(DEVICE_BASE(LW_PCFG1) | LW_XVE1_LINK_CONTROL_STATUS, m_azaliaLinkCtrlReg);
        m_pSubdevice->RegWr32(DEVICE_BASE(LW_PCFG1) | LW_XVE1_PRIV_XV_0, m_azaliaCYAMaskReg);
    }

    return rc;
}

//! \brief Save off P-state ASPM settings
//! \return OK, specific RC code otherwise
RC PEX_ASPMTest::SaveOffPstateASPMSetting()
{
    RC rc;

    LwRmPtr pLwRm;
    LwRm::Handle hSubdevice = pLwRm->GetSubdeviceHandle(m_pSubdevice);
    LW2080_CTRL_PERF_GET_PSTATES20_DATA_PARAMS getPstateParams = {0};

    getPstateParams.numPstates = 1;
    getPstateParams.pstate[0].pstateID = LW2080_CTRL_PERF_PSTATES_P0;

    CHECK_RC(pLwRm->Control(hSubdevice,
             LW2080_CTRL_CMD_PERF_GET_PSTATES20_DATA,
             &getPstateParams, sizeof(getPstateParams)));
    m_savedASPMSetting = getPstateParams.pstate[0].flags;

    return rc;
}

//! \brief Restore P-state ASPM settings
//! \return OK, specific RC code otherwise
RC PEX_ASPMTest::RestorePstateASPMSetting()
{
    RC rc;

    CHECK_RC(ChangeASPMSetting(LW2080_CTRL_PERF_PSTATES_P0, m_savedASPMSetting));

    return rc;
}

//------------------------------------------------------------------------------
// Global fucntions
//------------------------------------------------------------------------------
//! \brief ChangePstate function.
//!     Verify pstate has been switched to targe pstate.
//!
//! \param[in] targetPstate  target pstate.
//! \param[in] pSubdevice    pointer to gpu sub device.
//! \param[in] pPerf         pointer to perf object.
//!
//! \return OK if pstate succeed succeed
RC PEXTests::ChangePstate(LwU32 targetPstate, GpuSubdevice *pSubdevice, Perf *pPerf)
{
    RC rc;
    LwRmPtr pLwRm;
    LwRm::Handle hSubdevice = pLwRm->GetSubdeviceHandle(pSubdevice);
    LwU32 fallBack = LW2080_CTRL_PERF_PSTATE_FALLBACK_RETURN_ERROR;
    LW2080_CTRL_PERF_GET_LWRRENT_PSTATE_PARAMS getLwrrentPStateParams = {0};

    // change to NO_ASPM pstate.
    CHECK_RC_MSG(pPerf->ForcePState(
                     DTIUtils::Mislwtils::getLogBase2(targetPstate), fallBack),
                     "Forced pstate transition failed.\r\n");

    GpuUtility::SleepOnPTimer(SLEEP_INTERVAL_MS, pSubdevice);

    // Make sure pstate swtich suceed.
    getLwrrentPStateParams.lwrrPstate = LW2080_CTRL_PERF_PSTATES_UNDEFINED;
    CHECK_RC(pLwRm->Control(hSubdevice,
             LW2080_CTRL_CMD_PERF_GET_LWRRENT_PSTATE,
             &getLwrrentPStateParams, sizeof(getLwrrentPStateParams)));

    if (getLwrrentPStateParams.lwrrPstate != targetPstate)
    {
        Printf(Tee::PriHigh, "%s: Failed to force pstate switch\r\n",__FUNCTION__);
        rc = RC::LWRM_ERROR;
    }

    return rc;
}

//! \brief FectchPstatesCapInfo function.
//!     Query RM for availabe pstates and get capability information from RM for each
//!     available pstate.
//!
//! \param[in] pSubdevice    pointer to gpu sub device.
//! \param[out] pstatesCapInfo  pstates capability information
//!
//! \return OK if rm control call succeed.
//------------------------------------------------------------------------------
RC PEXTests::FectchPstatesCapInfo(GpuSubdevice *pSubdevice, vector<PstateCapInfo> &pstatesCapInfo)
{
    RC rc;
    LwRmPtr pLwRm;
    LwRm::Handle hSubdevice = pLwRm->GetSubdeviceHandle(pSubdevice);
    LW2080_CTRL_PERF_GET_PSTATES_INFO_PARAMS pstatesInfoParams = {0};
    PstateCapInfo pstateCapInfo;

    // Get P-States info
    rc = pLwRm->Control(hSubdevice,
                        LW2080_CTRL_CMD_PERF_GET_PSTATES_INFO,
                        &pstatesInfoParams, sizeof(pstatesInfoParams));
    if (rc != OK)
    {
        Printf(Tee::PriHigh, "%s: LW2080_CTRL_CMD_PERF_GET_PSTATES_INFO Ctrl cmd "
                             "failed with RC = %d\r\n",
                             __FUNCTION__, (UINT32)rc);
        return rc;
    }

    if (FLD_TEST_DRF(2080_CTRL, _PERF_GET_PSTATES_FLAGS, _DYN_PSTATE_CAPABLE, _OFF,
                      pstatesInfoParams.flags) ||
        FLD_TEST_DRF(2080_CTRL, _PERF_GET_PSTATES_FLAGS, _DYNAMIC_PSTATE, _DISABLE,
                      pstatesInfoParams.flags))
    {
        Printf(Tee::PriHigh, "%s:Dynamic P-states is disabled or not supported\r\n",
                             __FUNCTION__);
        return RC::SOFTWARE_ERROR;
    }

    // Get power info of each available pstates.
    LwU32 nMaxPState = DTIUtils::Mislwtils::getLogBase2(LW2080_CTRL_PERF_PSTATES_MAX);
    for (UINT32 i = 0; i <= nMaxPState; ++i)
    {
        if (pstatesInfoParams.pstates & (1 << i))
        {
            LwU32 nPStateMask = 1 << i;
            FectchPstateCapInfo(nPStateMask, pSubdevice, pstateCapInfo);
            pstatesCapInfo.push_back(pstateCapInfo);
        }
    }
    return rc;
}

//! \brief FectchPstatesCapInfo function.
//!     Query RM and fetch one pstate information
//!
//! \param[in] pstate           the targe pstate
//! \param[in] pSubdevice       pointer to gpu sub device.
//! \param[out] pstateCapInfo   pstate capability information
//!
//! \return OK if rm control call succeed.
//------------------------------------------------------------------------------
RC PEXTests::FectchPstateCapInfo(LwU32 pstate, GpuSubdevice *pSubdevice, PstateCapInfo &pstateCapInfo)
{
    RC rc;
    LwRmPtr pLwRm;
    LwRm::Handle hSubdevice = pLwRm->GetSubdeviceHandle(pSubdevice);
    LW2080_CTRL_PERF_GET_PSTATE_INFO_PARAMS pstateInfoParams = {0};

    // Setup the parameters for querying details of p-state
    pstateInfoParams.pstate = pstate;
    pstateInfoParams.perfClkDomInfoListSize = 0; // -- We do not need get Clock Domain Info
    pstateInfoParams.perfVoltDomInfoListSize = 0; // -- We do not need get Volt Domain Info

    // Get the details of the p-state
    rc = pLwRm->Control(hSubdevice,
                            LW2080_CTRL_CMD_PERF_GET_PSTATE_INFO,
                            &pstateInfoParams,
                            sizeof(pstateInfoParams));
    if (rc != OK)
    {
        Printf(Tee::PriHigh, "%s: LW2080_CTRL_CMD_PERF_GET_PSTATE_INFO Ctrl cmd failed with RC = %d\r\n",
                             __FUNCTION__, (UINT32)rc);
        return rc;
    }

    pstateCapInfo.pstate  = pstateInfoParams.pstate;
    pstateCapInfo.capFlag = pstateInfoParams.flags;

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
JS_CLASS_INHERIT(PEX_ASPMTest, RmTest,
    "PEX_ASPM test");

CLASS_PROP_READWRITE(PEX_ASPMTest, dynamicTest, LwBool, "Dynamic test");
CLASS_PROP_READWRITE(PEX_ASPMTest, deepl1, LwBool, "Deepl1 test");
CLASS_PROP_READWRITE(PEX_ASPMTest, sleepIntervalMS, UINT32, "Sleep interval before checking counters");

