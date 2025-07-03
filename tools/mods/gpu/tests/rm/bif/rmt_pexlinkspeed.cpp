/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2012-2019 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

//!
//! \file rmt_pexlinkspeed.cpp
//! \brief RM-Test to perform a pcie link speed test.
//! This test is primarily written to replace pcie link speed  manual tests.
//!  1. Permuate all possible link speed switch.
//!  2. Set link speed through RM control funciton.
//!  3. Verify whether link speed has changed correspondingly.
//!
#include "rmt_pextests.h"
#include <map>
#include <set>
#include <string> // Only use <> for built-in C++ stuff
#include <vector>

#include "core/include/memcheck.h"

class PEX_LinkSpeedTest : public RmTest
{
public:
    PEX_LinkSpeedTest();
    virtual ~PEX_LinkSpeedTest();

    virtual string IsTestSupported();

    virtual RC Setup();
    virtual RC Run();
    virtual RC Cleanup();

    SETGET_PROP(gen3, UINT32);
    SETGET_PROP(gen4, UINT32);

private:
    GpuSubdevice  *m_pSubdevice;
    Perf          *m_pPerf;
    LwBool         m_gen3;
    LwBool         m_gen4;

    vector<PstateCapInfo> m_pstatesCapInfo;
    vector<PstateCapInfo> m_workingSet;

    RC CheckLinkSpeedByPstateSwitch();
    RC CheckLinkSpeedByRMAPI();
    RC CheckPCIELinkSpeedSetting();

    RC SetPCIELinkSpeed(LwU32 speed);
    RC VerifyLinkSpeedStatus(LwU32 speed);
};

//! \brief Conscructor of PEX_LinkSpeedTest
//!
//------------------------------------------------------------------------------
PEX_LinkSpeedTest::PEX_LinkSpeedTest() :
    m_pSubdevice(NULL),
    m_pPerf(NULL),
    m_gen3(false),
    m_gen4(false)
{
    SetName("PEX_LinkSpeedTest");
}

//! \brief Cleanup function
//!
//!     Destructor of PEX_LinkSpeedTest
//!
//! \sa Cleanup
//------------------------------------------------------------------------------
PEX_LinkSpeedTest::~PEX_LinkSpeedTest()
{

}

//! \brief IsTestSupported Function
//!
//!     1. Check where there chip family supported.
//!     2. Chech whether root port support GEN2/GEN3.
//!     2. Check whether VBIOS has supported pstates setting.
//!
//! \return RC::OK if the test can be run in the current environment,
//!         skip reason otherwise
//------------------------------------------------------------------------------
string PEX_LinkSpeedTest::IsTestSupported()
{
    if (!m_pSubdevice)
        m_pSubdevice = GetBoundGpuSubdevice();
    if (!m_pPerf)
        m_pPerf = m_pSubdevice->GetPerf();

    if(Platform::GetSimulationMode() == Platform::Fmodel ||
       Platform::GetSimulationMode() == Platform::RTL)
    {
        return "PEX_LinkSpeedTest does not support Fmodel and RTL";
    }

    if ( OK != CheckPCIELinkSpeedSetting())
        return "PCIe Link Speed Changing is not supported";

    return RUN_RMTEST_TRUE;
}

//! \brief Setup Function
//!
//! \return Should always return RM::OK
//------------------------------------------------------------------------------
RC PEX_LinkSpeedTest::Setup()
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
//!     Start actual test function CheckLinkSpeedByPstateSwitch
//!
//! \return OK if the tests passed, specific RC if failed
//------------------------------------------------------------------------------
RC PEX_LinkSpeedTest::Run()
{
    RC rc;

    CHECK_RC(CheckLinkSpeedByRMAPI());

    return rc;
}

//! \brief Cleanup function
//!     cleanup before descructor
//------------------------------------------------------------------------------
RC PEX_LinkSpeedTest::Cleanup()
{
    return OK;
}

//! \brief CheckLinkSpeedByRMAPI function
//!
//!   Permuate all possible link speed switch.
//!   Set link speed through RM control funciton.
//!   and verify whether link speed has changed correspondingly
//!
//! \return OK if the tests passed, specific RC if failed
//------------------------------------------------------------------------------
RC PEX_LinkSpeedTest::CheckLinkSpeedByRMAPI()
{
    RC rc;
    LwU32 i, j;
    vector<int> linkSpeeds;

    // Add link speeds to list
    linkSpeeds.push_back(LW2080_CTRL_BUS_SET_PCIE_SPEED_2500MBPS);

    // gen2 link speed is not supported on emulation, only on silicon
    if (!GetBoundGpuSubdevice()->IsEmulation())
    {
        linkSpeeds.push_back(LW2080_CTRL_BUS_SET_PCIE_SPEED_5000MBPS);
    }

    if (m_gen4 || (m_gen3 && m_gen4))
    {
       linkSpeeds.push_back(LW2080_CTRL_BUS_SET_PCIE_SPEED_8000MBPS);
       linkSpeeds.push_back(LW2080_CTRL_BUS_SET_PCIE_SPEED_16000MBPS);
    }
    else if (m_gen3)
    {
       linkSpeeds.push_back(LW2080_CTRL_BUS_SET_PCIE_SPEED_8000MBPS);
    }

    for ( i = 0; i < linkSpeeds.size(); i++)
    {
        CHECK_RC(SetPCIELinkSpeed(linkSpeeds[i]));
        CHECK_RC_MSG(VerifyLinkSpeedStatus(linkSpeeds[i]),
                     "Link speed change verification failed");
        for ( j = i + 1; j < linkSpeeds.size(); j++)
        {
            CHECK_RC(SetPCIELinkSpeed(linkSpeeds[j]));
            CHECK_RC_MSG(VerifyLinkSpeedStatus(linkSpeeds[j]),
                        "Link speed change verification failed");

            // Set back to link speed i.
            CHECK_RC(SetPCIELinkSpeed(linkSpeeds[i]));
            CHECK_RC_MSG(VerifyLinkSpeedStatus(linkSpeeds[i]),
                     "Link speed change verification failed");
        }
    }
    return rc;
}

//! \brief SetPCIELinkSpeed function.
//!
//!     Set link speed to target speed by RM Control function:
//!         LW2080_CTRL_CMD_BUS_SET_PCIE_SPEED
//!
//! \return OK if RM Control function returned with no error.
//------------------------------------------------------------------------------
RC PEX_LinkSpeedTest::SetPCIELinkSpeed(LwU32 speed)
{
    RC rc;
    LwRmPtr pLwRm;
    LwRm::Handle hSubdevice = pLwRm->GetSubdeviceHandle(m_pSubdevice);
    LW2080_CTRL_BUS_SET_PCIE_SPEED_PARAMS pcieLinkSpeed = {0};

    pcieLinkSpeed.busSpeed = speed;

    // Switch indexb pstate.
    Printf(Tee::PriLow, "Changing to PCIe LinkSpeed: Gen%d\n", speed);
    CHECK_RC(pLwRm->Control(hSubdevice,
                    LW2080_CTRL_CMD_BUS_SET_PCIE_SPEED,
                    &pcieLinkSpeed, sizeof(pcieLinkSpeed)));
    return rc;
}

//! \brief VerifyLinkSpeedStatus function.
//!
//!     Check if the link speed has been set to the target link speed.
//!     The way we are verfiying current pcie link speed is by checking
//!     LW_XVE_PRIV_XV_0 register.
//!
//! \return OK if the pcie link speed change has succeeded.
//------------------------------------------------------------------------------
RC PEX_LinkSpeedTest::VerifyLinkSpeedStatus(LwU32 speed)
{
    RC rc;
    LwRmPtr pLwRm;
    LwRm::Handle hSubdevice = pLwRm->GetSubdeviceHandle(m_pSubdevice);
    LW2080_CTRL_BUS_INFO pcieLinkSpeed = {0};
    LW2080_CTRL_BUS_GET_INFO_PARAMS getBusInfoPara = {0};

    pcieLinkSpeed.index = LW2080_CTRL_BUS_INFO_INDEX_PCIE_GPU_LINK_CTRL_STATUS;
    getBusInfoPara.busInfoListSize = 1;
    getBusInfoPara.busInfoList = (LwP64)&pcieLinkSpeed;

    CHECK_RC(pLwRm->Control(hSubdevice,
                    LW2080_CTRL_CMD_BUS_GET_INFO,
                    &getBusInfoPara, sizeof(getBusInfoPara)));

    if (!FLD_TEST_DRF_NUM(2080, _CTRL_BUS_INFO_PCIE_LINK_CTRL_STATUS, _LINK_SPEED,
                         speed, pcieLinkSpeed.data))
    {
        rc = RC::SOFTWARE_ERROR;
    }

    return rc;
}

//! \brief CheckPCIELinkSpeedSetting function.
//!      Check link speed cap
//!      1. GPU gen cap.
//!      2. System gen cap.
//!      3. Speed Change cap.
//! \return LW_OK if PCIE link speed settings are OK.
//------------------------------------------------------------------------------
RC PEX_LinkSpeedTest::CheckPCIELinkSpeedSetting()
{
    RC rc;
    LwRmPtr pLwRm;
    LwRm::Handle hSubdevice = pLwRm->GetSubdeviceHandle(m_pSubdevice);

    LW2080_CTRL_BUS_INFO pcieLinkGen = {0};
    LW2080_CTRL_BUS_GET_INFO_PARAMS getBusInfoPara = {0};

    pcieLinkGen.index = LW2080_CTRL_BUS_INFO_INDEX_PCIE_GEN_INFO;
    getBusInfoPara.busInfoListSize = 1;
    getBusInfoPara.busInfoList = (LwP64)&pcieLinkGen;

    CHECK_RC(pLwRm->Control(hSubdevice,
                    LW2080_CTRL_CMD_BUS_GET_INFO,
                    &getBusInfoPara, sizeof(getBusInfoPara)));

    // Check if linkspeed change is allowed.
    if (FLD_TEST_DRF(2080, _CTRL_BUS_INFO_PCIE_LINK_CAP, _SPEED_CHANGES,
                     _DISABLED, pcieLinkGen.data))
    {
        Printf(Tee::PriHigh, "PCIE LINK SPEED CHANGE IS NOT SUPPORTED!\n");
        rc = RC::LWRM_ERROR;
    }

    // Check if gen2 speed is supported.
    if ((DRF_VAL(2080, _CTRL_BUS_INFO_PCIE_LINK_CAP, _GEN, pcieLinkGen.data)
         < LW2080_CTRL_BUS_INFO_PCIE_LINK_CAP_GEN_GEN2) ||
        (DRF_VAL(2080, _CTRL_BUS_INFO_PCIE_LINK_CAP, _GPU_GEN, pcieLinkGen.data)
         < LW2080_CTRL_BUS_INFO_PCIE_LINK_CAP_GPU_GEN_GEN2))
    {
        Printf(Tee::PriHigh, "GEN2 PCIE link speed is not supported!\n");
        rc = RC::SOFTWARE_ERROR;
    }
    if (m_gen3 &&
          ((DRF_VAL(2080, _CTRL_BUS_INFO_PCIE_LINK_CAP, _GEN, pcieLinkGen.data)
           < LW2080_CTRL_BUS_INFO_PCIE_LINK_CAP_GEN_GEN3) ||
          (DRF_VAL(2080, _CTRL_BUS_INFO_PCIE_LINK_CAP, _GPU_GEN, pcieLinkGen.data)
           < LW2080_CTRL_BUS_INFO_PCIE_LINK_CAP_GPU_GEN_GEN3)))
    {
        Printf(Tee::PriHigh, "GEN3 PCIE link speed is not supported!\n");
        rc = RC::SOFTWARE_ERROR;
    }
    if (m_gen4 &&
          ((DRF_VAL(2080, _CTRL_BUS_INFO_PCIE_LINK_CAP, _GEN, pcieLinkGen.data)
           < LW2080_CTRL_BUS_INFO_PCIE_LINK_CAP_GEN_GEN4) ||
          (DRF_VAL(2080, _CTRL_BUS_INFO_PCIE_LINK_CAP, _GPU_GEN, pcieLinkGen.data)
           < LW2080_CTRL_BUS_INFO_PCIE_LINK_CAP_GPU_GEN_GEN4)))
    {
        Printf(Tee::PriHigh, "GEN4 PCIE link speed is not supported!\n");
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
JS_CLASS_INHERIT(PEX_LinkSpeedTest, RmTest,
    "PEX_LinkSpeedTest test");

CLASS_PROP_READWRITE(PEX_LinkSpeedTest, gen3, LwBool, "Test for Gen3 or not");
CLASS_PROP_READWRITE(PEX_LinkSpeedTest, gen4, LwBool, "Test for Gen4 or not");
