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
//! \file rmt_pexaslm.cpp
//! This test is primarily written to replace PEX_ASLM manual tests.
//! 1. Use either PEX_ASLM control register or Pstate switch to change ASLM setting.
//! 2. Verify ASLM setting changes by check correspounding counter register.
//!

#include "core/include/jscript.h"
#include "core/include/memcheck.h"
#include "core/include/script.h"
#include "device/interface/pcie.h"
#include "gpu/tests/rmtest.h"
#include "rmt_pextests.h"

#include <map>
#include <set>
#include <string> // Only use <> for built-in C++ stuff
#include <vector>

#define LINK_WIDTH(val) DRF_VAL(2080_CTRL, _GET_PSTATE_FLAG, _PCIELINKWIDTH, val)
#define REQUESTED_PSTATE_NUM 4

class PEX_ASLMTest : public RmTest
{

public:
    PEX_ASLMTest();
    virtual ~PEX_ASLMTest();

    virtual string IsTestSupported();

    virtual RC Setup();
    virtual RC Run();
    virtual RC Cleanup();

    SETGET_PROP(dynamicTest, UINT32);

private:
    LwBool         m_dynamicTest;
    GpuSubdevice  *m_pSubdevice;
    Perf          *m_pPerf;

    vector<PstateCapInfo> m_pstatesCapInfo;
    vector<PstateCapInfo> m_workingSet;

    RC CheckASLMByRegister();
    RC CheckASLMByPstateSwitch();
    RC ChangeLinkWidthByReg(LwU32 targetWidth);
    RC CheckPCIELinkWidthSetting();
    RC VerifyLwrrentLinkWidth(LwU32 targetWidth);

    LwBool CheckPstateSettings();
    LwU32 ColwertLinkWidth(LwU32 capLinkWidth);
};

//! \brief Conscructor of PEX_ASLMTest
//------------------------------------------------------------------------------
PEX_ASLMTest::PEX_ASLMTest() :
    m_dynamicTest(false),
    m_pSubdevice(NULL),
    m_pPerf(NULL)
{
    SetName("PEX_ASLMTest");
}

//! \brief Destructor of PEX_ASLMTest
//!
//!     Destructor of PEX_ASLMTest
//!
//! \sa Cleanup
//------------------------------------------------------------------------------
PEX_ASLMTest::~PEX_ASLMTest()
{

}

//! \brief IsTestSupported Function
//!
//! Check whether this test is supported.
//!
//!     1. Check where there chip family supported.
//!     2. If this dynamic test flag has been set, check whether VBIOS is valid.
//!
//! \return RUN_RMTEST_TRUE if the test can be run in the current environment,
//!         skip reason otherwise
//------------------------------------------------------------------------------
string PEX_ASLMTest::IsTestSupported()
{
    if (!m_pSubdevice)
        m_pSubdevice = GetBoundGpuSubdevice();
    if (!m_pPerf)
        m_pPerf = m_pSubdevice->GetPerf();

    if(Platform::GetSimulationMode() == Platform::Fmodel ||
       Platform::GetSimulationMode() == Platform::RTL)
    {
        return "PEX_ASLMTest does not support Fmodel and RTL";
    }

    // If it's dynamic pex aslm test, we need to check vbios information.
    if (m_dynamicTest)
    {
        if (OK != CheckPCIELinkWidthSetting())
            return "ASLM is not supported on current settings!";

        PEXTests::FectchPstatesCapInfo( m_pSubdevice, m_pstatesCapInfo);
        if (!CheckPstateSettings())
           return "Need VBIOS with four p-states that are different only by L0s and L1 settings";
    }

    return RUN_RMTEST_TRUE;
}

//! \brief Setup Function
//!
//! \return Should always return RM::OK
//------------------------------------------------------------------------------
RC PEX_ASLMTest::Setup()
{
    RC rc;
    if (!m_pSubdevice)
        m_pSubdevice = GetBoundGpuSubdevice();
    if (!m_pPerf)
        m_pPerf = m_pSubdevice->GetPerf();

    // Ask our base class(es) to setup internal state based on JS settings
    CHECK_RC(InitFromJs());

    return rc;
}

//! \brief Run Function
//!
//! 1. If dynamic flag is set, change PCIE link width by changing pstate.
//! 2. If not, change PCIE link width by writign to PCIE control register
//!
//! \return OK if the tests passed, specific RC if failed
//------------------------------------------------------------------------------
RC PEX_ASLMTest::Run()
{
    RC rc;
    if (!m_dynamicTest)
    {
        // Change PCIE link width by setting registers
        CHECK_RC(CheckASLMByRegister());
    }
    else
    {
        // Change PCIE link width by switching pstate.
        CHECK_RC(CheckASLMByPstateSwitch());

    }
    return rc;
}

//! \brief Cleanup function
//------------------------------------------------------------------------------
RC PEX_ASLMTest::Cleanup()
{
    return OK;
}

//------------------------------------------------------------------------------
// Private Member Functions
//------------------------------------------------------------------------------
//!\brief CheckASLMByRegister function
//!
//!  1. For each possible cap flag, change ASLM registers correspoundingly.
//!  2. Verify link width changes.
//!
//! \return OK if the tests passed, specific RC if failed
//------------------------------------------------------------------------------
RC PEX_ASLMTest::CheckASLMByRegister()
{
    RC rc;
    size_t indexA;
    size_t indexB;
    vector<LwU32> targetWidths;

    // Populate link width change plan.
    targetWidths.push_back(LW_XVE_LINK_CONTROL_STATUS_NEGOTIATED_LINK_WIDTH_X1);
    targetWidths.push_back(LW_XVE_LINK_CONTROL_STATUS_NEGOTIATED_LINK_WIDTH_X2);
    targetWidths.push_back(LW_XVE_LINK_CONTROL_STATUS_NEGOTIATED_LINK_WIDTH_X4);
    targetWidths.push_back(LW_XVE_LINK_CONTROL_STATUS_NEGOTIATED_LINK_WIDTH_X8);
    targetWidths.push_back(LW_XVE_LINK_CONTROL_STATUS_NEGOTIATED_LINK_WIDTH_X16);

    // Permute all possiblel link width changes.
    CHECK_RC(ChangeLinkWidthByReg(targetWidths[0]));
    for (indexA = 0; indexA < targetWidths.size() - 1; indexA++)
    {
        for (indexB = targetWidths.size() - 1; indexB > indexA ; indexB--)
        {
            CHECK_RC(ChangeLinkWidthByReg(targetWidths[indexB]));
            // Check change result.
            CHECK_RC_MSG(VerifyLwrrentLinkWidth(targetWidths[indexB]),
                         "ASLM change verification failed\r\n");

            // Change back.
            CHECK_RC(ChangeLinkWidthByReg(targetWidths[indexA]));
            // CHeck change result.
            CHECK_RC_MSG(VerifyLwrrentLinkWidth(targetWidths[indexA]),
                         "ASLM change verification failed\r\n");
        }
    }

    return rc;
}

//!\brief ChangeLinkWidthByReg funciton
//!
//!   Permuate all possible pstate switch.
//!   Switch pstate and verify whether link width has changed correspondingly
//!
//! \return OK if the tests passed, specific RC if failed
//------------------------------------------------------------------------------
RC PEX_ASLMTest::CheckASLMByPstateSwitch()
{
    RC rc;
    size_t indexA;
    size_t indexB;

    Printf(Tee::PriLow, "Changing to %d pstate, Link Width: %d,\r\n",
           m_workingSet[0].pstate, ColwertLinkWidth(m_workingSet[0].capFlag));
    CHECK_RC(PEXTests::ChangePstate(m_workingSet[0].pstate, m_pSubdevice, m_pPerf));
    for (indexA = 0; indexA < m_workingSet.size() - 1; indexA++)
    {
        for (indexB = m_workingSet.size() - 1; indexB > indexA ; indexB--)
        {
            // Switch indexb pstate.
            Printf(Tee::PriLow, "Changing to %d pstate, Link Width: %d,\r\n",
                   DTIUtils::Mislwtils::getLogBase2(m_workingSet[indexB].pstate),
                   ColwertLinkWidth(m_workingSet[indexB].capFlag));
            CHECK_RC(PEXTests::ChangePstate(m_workingSet[indexB].pstate, m_pSubdevice, m_pPerf));
            CHECK_RC_MSG(VerifyLwrrentLinkWidth(ColwertLinkWidth(m_workingSet[indexB].capFlag)),
                         "Link width change verification failed\r\n");

            // Switch indexA pstate.
            Printf(Tee::PriLow, "Changing to %d pstate, Link Width: %d,\r\n",
                   DTIUtils::Mislwtils::getLogBase2(m_workingSet[indexA].pstate),
                   ColwertLinkWidth(m_workingSet[indexA].capFlag));
            CHECK_RC(PEXTests::ChangePstate(m_workingSet[indexA].pstate, m_pSubdevice, m_pPerf));
            CHECK_RC_MSG(VerifyLwrrentLinkWidth(ColwertLinkWidth(m_workingSet[indexA].capFlag)),
                         "Link width change verification failed\r\n");
        }
    }

    return rc;
}

//!\brief  CheckPstateSettings funciton
//!   Look for 4 pstates that suport pcie link width x1 x2 x4 x8 and x16 separatly.
//!
//! \return True if found needed pstates
//------------------------------------------------------------------------------
LwBool PEX_ASLMTest::CheckPstateSettings()
{
    LwU32 i;
    set<LwU32> pstateSet;
    pair<set<LwU32>::iterator,bool> ret;
    // Find 4 pstats with differnt link width.
    if (m_pstatesCapInfo.size() < REQUESTED_PSTATE_NUM)
        return false;

    for ( i = 0; i < m_pstatesCapInfo.size(); i++)
    {
        if (LINK_WIDTH(m_pstatesCapInfo[i].capFlag)
            == LW2080_CTRL_GET_PSTATE_FLAG_PCIELINKWIDTH_UNDEFINED)
            continue;

        ret = pstateSet.insert(LINK_WIDTH(m_pstatesCapInfo[i].capFlag));
        if (ret.second)
            m_workingSet.push_back(m_pstatesCapInfo[i]);
    }

    return m_workingSet.size() == REQUESTED_PSTATE_NUM;
}

//!\brief ChangeLinkWidthByReg funciton
//!  Change pcie link width by set control registers.
//!  Link for steps:
//!    https://wiki.lwpu.com/engwiki/index.php/Resman/Resman_Components/PEX/Bringup
//!
//!\param[in] targetWidth  targe PCIE link width
//!
//! \return RM::OK if pcie changed link width
//------------------------------------------------------------------------------
RC PEX_ASLMTest::ChangeLinkWidthByReg(LwU32 targetWidth)
{
    RC rc;
    LwU32 regVal;
    LwU32 regVal2 = 0;
    const Pcie* pPcie = m_pSubdevice->GetInterface<Pcie>();
    // Get current link width.
    CHECK_RC(Platform::PciRead32(pPcie->DomainNumber(), pPcie->BusNumber(),
                                 pPcie->DeviceNumber(), pPcie->FunctionNumber(),
                                 LW_XVE_LINK_CONTROL_STATUS, &regVal2));
    regVal = m_pSubdevice->RegRd32(LW_XP_PL_LINK_CONFIG_0(0));

    switch (DRF_VAL(_XVE, _LINK_CONTROL_STATUS, _LINK_SPEED, regVal2))
    {
        case LW_XVE_LINK_CONTROL_STATUS_LINK_SPEED_8P0:
            regVal = FLD_SET_DRF(_XP, _PL_LINK_CONFIG_0, _MAX_LINK_RATE, _8000_MTPS, regVal);
            break;

        case LW_XVE_LINK_CONTROL_STATUS_LINK_SPEED_5P0:
            regVal = FLD_SET_DRF(_XP, _PL_LINK_CONFIG_0, _MAX_LINK_RATE, _5000_MTPS, regVal);
            break;

        case LW_XVE_LINK_CONTROL_STATUS_LINK_SPEED_2P5:
            regVal = FLD_SET_DRF(_XP, _PL_LINK_CONFIG_0, _MAX_LINK_RATE, _2500_MTPS, regVal);
            break;

        default:
            break;
    }

    switch (targetWidth)
    {
        case LW_XVE_LINK_CONTROL_STATUS_NEGOTIATED_LINK_WIDTH_X1:
            regVal = FLD_SET_DRF(_XP, _PL_LINK_CONFIG_0, _TARGET_TX_WIDTH, _x1, regVal);
            break;

        case LW_XVE_LINK_CONTROL_STATUS_NEGOTIATED_LINK_WIDTH_X2:
            regVal = FLD_SET_DRF(_XP, _PL_LINK_CONFIG_0, _TARGET_TX_WIDTH, _x2, regVal);
            break;

        case LW_XVE_LINK_CONTROL_STATUS_NEGOTIATED_LINK_WIDTH_X4:
            regVal = FLD_SET_DRF(_XP, _PL_LINK_CONFIG_0, _TARGET_TX_WIDTH, _x4, regVal);
            break;

        case LW_XVE_LINK_CONTROL_STATUS_NEGOTIATED_LINK_WIDTH_X8:
            regVal = FLD_SET_DRF(_XP, _PL_LINK_CONFIG_0, _TARGET_TX_WIDTH, _x8, regVal);
            break;

        case LW_XVE_LINK_CONTROL_STATUS_NEGOTIATED_LINK_WIDTH_X16:
            regVal = FLD_SET_DRF(_XP, _PL_LINK_CONFIG_0, _TARGET_TX_WIDTH, _x16, regVal);
            break;

        default:
            rc = RC::SOFTWARE_ERROR;
    }

    m_pSubdevice->RegWr32(LW_XP_PL_LINK_CONFIG_0(0), regVal);

    // trigger link width change.
    regVal = FLD_SET_DRF(_XP, _PL_LINK_CONFIG_0, _LTSSM_DIRECTIVE, _CHANGE_WIDTH, regVal);
    m_pSubdevice->RegWr32(LW_XP_PL_LINK_CONFIG_0(0), regVal);

    Printf(Tee::PriLow, "Changing to PCIE link width %d\r\n", targetWidth);

    return rc;
}

//!\brief ColwertLinkWidth funciton
//!     Colwert pcie link width of LW2080_CTRL_CMD_PERF_GET_PSTATE_INFO to
//!     Target actual link width
//!
//!\param[in] capLinkWidth  PCIE link width returned by LW2080_CTRL_CMD_PERF_GET_PSTATE_INFO
//!
//!\return Targe actual link width
//------------------------------------------------------------------------------
LwU32 PEX_ASLMTest::ColwertLinkWidth(LwU32 capLinkWidth)
{
    LwU32 targetWidth;

    MASSERT(capLinkWidth != LW2080_CTRL_GET_PSTATE_FLAG_PCIELINKWIDTH_UNDEFINED);

    switch (LINK_WIDTH(capLinkWidth))
    {
        case LW2080_CTRL_GET_PSTATE_FLAG_PCIELINKWIDTH_1:
            targetWidth = LW_XVE_LINK_CONTROL_STATUS_NEGOTIATED_LINK_WIDTH_X1;
            break;

        case LW2080_CTRL_GET_PSTATE_FLAG_PCIELINKWIDTH_2:
            targetWidth = LW_XVE_LINK_CONTROL_STATUS_NEGOTIATED_LINK_WIDTH_X2;
            break;

        case LW2080_CTRL_GET_PSTATE_FLAG_PCIELINKWIDTH_4:
            targetWidth = LW_XVE_LINK_CONTROL_STATUS_NEGOTIATED_LINK_WIDTH_X4;
            break;

        case LW2080_CTRL_GET_PSTATE_FLAG_PCIELINKWIDTH_8:
            targetWidth = LW_XVE_LINK_CONTROL_STATUS_NEGOTIATED_LINK_WIDTH_X8;
            break;

        case LW2080_CTRL_GET_PSTATE_FLAG_PCIELINKWIDTH_16:
            targetWidth = LW_XVE_LINK_CONTROL_STATUS_NEGOTIATED_LINK_WIDTH_X16;
            break;

        case LW2080_CTRL_GET_PSTATE_FLAG_PCIELINKWIDTH_UNDEFINED:
        default:
            targetWidth = 0;
    }

    return targetWidth;
}

//!\brief VerifyLwrrentLinkWidth funciton
//!     Check if the link width has been set to the target link width.
//!     The way we are verfiying current pcie link width is by checking
//!     LW_XVE_PRIV_XV_0 register.
//!
//!\param[in] targetWidth  targe PCIE link width
//!
//!\return LW_OK if pcie link width change succeeded.
//------------------------------------------------------------------------------
RC PEX_ASLMTest::VerifyLwrrentLinkWidth(LwU32 targetWidth)
{
    RC rc;
    LwRmPtr pLwRm;
    LwRm::Handle hSubdevice = pLwRm->GetSubdeviceHandle(m_pSubdevice);
    LW2080_CTRL_BUS_INFO pcieLinkWidth = {0};
    LW2080_CTRL_BUS_GET_INFO_PARAMS getBusInfoPara = {0};

    MASSERT(targetWidth);

    pcieLinkWidth.index = LW2080_CTRL_BUS_INFO_INDEX_PCIE_GPU_LINK_CTRL_STATUS;
    getBusInfoPara.busInfoListSize = 1;
    getBusInfoPara.busInfoList = (LwP64)&pcieLinkWidth;

    CHECK_RC(pLwRm->Control(hSubdevice,
                    LW2080_CTRL_CMD_BUS_GET_INFO,
                    &getBusInfoPara, sizeof(getBusInfoPara)));

    if (!FLD_TEST_DRF_NUM(2080, _CTRL_BUS_INFO_PCIE_LINK_CTRL_STATUS, _LINK_WIDTH,
                         targetWidth, pcieLinkWidth.data))
    {
        rc = RC::SOFTWARE_ERROR;
    }

    return rc;
}

//! \brief CheckPCIELinkWidthSetting function.
//!      Check if ASLM is supported
//!
//! \return True if root port support link width.
//------------------------------------------------------------------------------
RC PEX_ASLMTest::CheckPCIELinkWidthSetting()
{
    RC rc;
    LwRmPtr pLwRm;
    LwRm::Handle hSubdevice = pLwRm->GetSubdeviceHandle(m_pSubdevice);

    LW2080_CTRL_BUS_INFO pcieLinkWidth = {0};
    LW2080_CTRL_BUS_GET_INFO_PARAMS getBusInfoPara = {0};

    pcieLinkWidth.index = LW2080_CTRL_BUS_INFO_INDEX_PCIE_ASLM_STATUS;
    getBusInfoPara.busInfoListSize = 1;
    getBusInfoPara.busInfoList = (LwP64)&pcieLinkWidth;

    CHECK_RC(pLwRm->Control(hSubdevice,
                    LW2080_CTRL_CMD_BUS_GET_INFO,
                    &getBusInfoPara, sizeof(getBusInfoPara)));

    // Check if pcie ASLM is supported.
    if ( FLD_TEST_DRF(2080, _CTRL_BUS_INFO_PCIE_ASLM_STATUS, _PCIE,
                     _ERROR, pcieLinkWidth.data) ||
        FLD_TEST_DRF(2080, _CTRL_BUS_INFO_PCIE_ASLM_STATUS, _SUPPORTED,
                     _NO, pcieLinkWidth.data))
    {
        Printf(Tee::PriHigh, "ASLM is not supported!");
        rc = RC::SOFTWARE_ERROR;
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
JS_CLASS_INHERIT(PEX_ASLMTest, RmTest,
    "PEX ASLM/PEX ASLM Dynamic test");

CLASS_PROP_READWRITE(PEX_ASLMTest, dynamicTest, LwBool, "Dynamic test");
