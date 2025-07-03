/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2013-2013 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

//!
//!
//! \file rmt_CrossbarTest.cpp
//! \brief a crossbar sor assignment test for testing the different
//! cases of SOR assignments. User will have to use flag onlyConnectedDisplays
//! if the test has to be run on connected displays else the displays are faked.
//! Also, There are 5 tescases. User will have to update the testcase number
//! which ranges from 1-5 to select which testcase has to be run. Testcase details
//! are written as comments in the start of each testcase.
//!

#include "gpu/tests/rmtest.h"
#include "core/include/script.h"
#include "core/include/jscript.h"
#include "core/include/platform.h"
#include "lwRmApi.h"
#include "lwmisc.h"
#include "gpu/include/gpudev.h"
#include "gpu/include/gpusbdev.h"
#include "core/include/display.h"
#include "core/include/platform.h"
#include "core/include/utility.h"
#include "core/include/memcheck.h"
#include "gpu/tests/rm/utility/dtiutils.h"
#include "disp/v02_05/dev_disp.h"
#include "class/cl0073.h"  // LW04_DISPLAY_COMMON
#include "ctrl/ctrl0000.h"
#include "ctrl/ctrl0073.h"
#include "ctrl/ctrl0073/ctrl0073dfp.h"
#include <algorithm>
#include <vector>

using namespace std;

#define MAX_PADLINKS 7

class CrossbarTest : public RmTest
{
public:
    CrossbarTest();
    virtual ~CrossbarTest();

    virtual string IsTestSupported();

    virtual RC Setup();
    virtual RC Run();
    virtual RC Cleanup();

    Surface2D m_DisplayImage;

    // test cases
    RC DFPAssignment();
    RC DPAssignment();
    RC LVDSAssignment();
    RC NonLVDSAssignment();
    RC MasterSlaveAssignment();
    RC DualLinkAssignment();

    RC AttachDisplay(DisplayID displayid);
    RC DetachDisplay(DisplayID displayid);

    SETGET_PROP(testcase, UINT32);
    SETGET_PROP(onlyConnectedDisplays, bool);
    SETGET_PROP(protocol, string);

private:
    // Test cases
    #define LW_TEST_CASE_DFP              (0x00000001)
    #define LW_TEST_CASE_DP               (0x00000002)
    #define LW_TEST_CASE_LVDS             (0x00000003)
    #define LW_TEST_CASE_MASTER_SLAVE     (0x00000004)
    #define LW_TEST_CASE_DUAL_LINK        (0x00000005)

    UINT32 m_testcase;           //!< Test case to execute
    bool   m_onlyConnectedDisplays;
    string m_protocol;
    Display *m_pDisplay;
    DisplayIDs m_detected;
    DisplayIDs fakedDisplays;
};

//! \brief CrossbarTest constructor
//!
//! Sets the name of the test
//!
//------------------------------------------------------------------------------
CrossbarTest::CrossbarTest()
{
    m_pDisplay = GetDisplay();

    SetName("CrossbarTest");
    m_testcase = 1;
    m_onlyConnectedDisplays = false;
    m_protocol = "LVDS_LWSTOM,SINGLE_TMDS_A,SINGLE_TMDS_B,DUAL_TMDS,DP_A,DP_B,EXT_TMDS_ENC";
}

//! \brief CrossbarTest destructor
//!
//! does  nothing
//!
//! \sa Cleanup
//------------------------------------------------------------------------------
CrossbarTest::~CrossbarTest()
{
}

//! \brief IsSupported
//!
//! Checks if the test can be run in the current environment.
//!
//! \return True if the test can be run in the current environment,
//!         false otherwise
//------------------------------------------------------------------------------
string CrossbarTest::IsTestSupported()
{
    if (m_pDisplay->GetOwningDisplaySubdevice()->HasFeature(Device::GPUSUB_SUPPORTS_SOR_XBAR))
        return RUN_RMTEST_TRUE;
    return "XBAR is not supported on the given GPU. This test shouldn't be run on this GPU!";
}

//! \setup Initialise internal state from JS
//!
//! Initial state setup based on the JS setting
//------------------------------------------------------------------------------
RC CrossbarTest::Setup()
{
    RC rc = OK;
    DisplayIDs supported;
    DisplayIDs displaysToFake;

    // Ask our base class(es) to setup internal state based on JS settings
    CHECK_RC(InitFromJs());

    CHECK_RC(m_pDisplay->InitializeDisplayHW(Display::ALLOC_DISP_CHANNELS));

    CHECK_RC(m_pDisplay->GetDetected(&m_detected));
    CHECK_RC(m_pDisplay->GetConnectors(&supported));

    for(UINT32 loopCount = 0; loopCount < supported.size(); loopCount++)
    {
        if (!m_pDisplay->IsDispAvailInList(supported[loopCount], m_detected) &&
            DTIUtils::VerifUtils::IsSupportedProtocol(m_pDisplay, supported[loopCount], m_protocol))
        {
            displaysToFake.push_back(supported[loopCount]);
        }
    }

    for(UINT32 loopCount = 0; loopCount < displaysToFake.size(); loopCount++)
    {
        CHECK_RC(DTIUtils::EDIDUtils::SetLwstomEdid(m_pDisplay,displaysToFake[loopCount], false));
        fakedDisplays.push_back(displaysToFake[loopCount]);
    }

    return rc;
}

//! \brief Run the test
//!
//! Each of the different test cases can be ilwoked using a testcase parameter.
//! After each test. The subdevice instance needs to be shut down and restarted
//! so previous stale data is cleared.
//------------------------------------------------------------------------------
RC CrossbarTest::Run()
{
    RC rc = OK;

    switch (m_testcase)
    {
        case LW_TEST_CASE_DFP:
            rc = DFPAssignment();
            break;
        case LW_TEST_CASE_DP:
            rc = DPAssignment();
            break;
        case LW_TEST_CASE_LVDS:
            rc = LVDSAssignment();
            break;
        case LW_TEST_CASE_MASTER_SLAVE:
            rc = MasterSlaveAssignment();
            break;
        case LW_TEST_CASE_DUAL_LINK:
            rc = DualLinkAssignment();
            break;
        default:
            Printf(Tee::PriHigh, "CrossbarTest: Specify a correct testcase! \n");
            rc = RC::SOFTWARE_ERROR;
    }

    return rc;
}

//! \brief DFP SOR assignment test
//!
//! This test requires more than 4 DFP displays. The test walks through
//! the displays, performs the crossbar SOR assignments, and checks
//! the assignment table for proper assignment. The test also checks for
//! the correct error reporting when an assignment is attempted for a 5th
//! DFP display.
//------------------------------------------------------------------------------
RC CrossbarTest::DFPAssignment()
{
    DisplayIDs  workingSet;
    UINT32      i, j, padlinksUsed = 0, padlink = 0, flag = 0, numOfDisplays = 0;
    RC          rc = OK;

    LW0073_CTRL_DFP_ASSIGN_SOR_PARAMS assignSorParams = {0};
    LW0073_CTRL_DFP_GET_PADLINK_MASK_PARAMS padlinkMaskParams = {0};

    Printf(Tee::PriHigh, "CrossbarTest: DFP testcase\n");

    if(!m_onlyConnectedDisplays)
    {
        for(i = 0; i < fakedDisplays.size(); i++)
        {
            workingSet.push_back(fakedDisplays[i]);
        }
    }
    else
    {
        for (i = 0; i < m_detected.size(); i++)
        {
            if (DTIUtils::VerifUtils::IsSupportedProtocol(m_pDisplay, m_detected[i], "CRT,LVDS_LWSTOM"))
            {
                continue;
            }
            workingSet.push_back(m_detected[i]);
        }
    }

    // working set now just has TMDS, DP, LVDS display entries
    if (workingSet.size() < (LW0073_CTRL_CMD_DFP_ASSIGN_SOR_MAX_SORS + 1))
    {
        Printf(Tee::PriHigh, "CrossbarTest: Need more than %d connected displays to complete this test\n",
            LW0073_CTRL_CMD_DFP_ASSIGN_SOR_MAX_SORS);
        CHECK_RC(RC::UNSUPPORTED_SYSTEM_CONFIG);
    }

    // iterate through the DFP displays
    for (i = 0; workingSet.size(); i++)
    {
        //
        // Check if a display on the same padlink has already been done
        // a modeset on or not. If yes, then we skip this display on
        // the same padlink as the other one.
        //
        padlinkMaskParams.subDeviceInstance = 0;
        padlinkMaskParams.displayId = workingSet[i];
        rc = m_pDisplay->RmControl(
                    LW0073_CTRL_CMD_DFP_GET_PADLINK_MASK,
                    &padlinkMaskParams, sizeof (padlinkMaskParams));

        if (rc == OK)
        {
            for (padlink = 0; padlink < MAX_PADLINKS; padlink ++)
            {
                if (BIT(padlink) & padlinkMaskParams.padlinkMask)
                {
                    if (BIT(padlink) & padlinksUsed)
                    {
                        flag = 1;
                        break;
                    }
                    padlinksUsed |= BIT(padlink);
                }
            }
        }

        if (flag == 1)
        {
            flag = 0;
            continue;
        }

        // Assign SOR for display
        assignSorParams.subDeviceInstance = 0;
        assignSorParams.displayId = workingSet[i];
        assignSorParams.sorExcludeMask = 0;
        assignSorParams.slaveDisplayId = 0;

        rc = m_pDisplay->RmControl(
                    LW0073_CTRL_CMD_DFP_ASSIGN_SOR,
                    &assignSorParams, sizeof (assignSorParams));

        numOfDisplays++;

        if (numOfDisplays <= LW0073_CTRL_CMD_DFP_ASSIGN_SOR_MAX_SORS)
        {
            if (OK != rc)
            {
                Printf(Tee::PriHigh,
                        "CrossbarTest: assign SOR failed: 0x%x\n", rc.Get());
                CHECK_RC(rc);
            }
        }
        else
        {
            if (OK == rc)
            {
                Printf(Tee::PriHigh,
                        "CrossbarTest: Displays exceeding crossbar capacity not detected\n");
                rc = RC::SOFTWARE_ERROR;
                CHECK_RC(rc);
            }
            else
            {
                Printf(Tee::PriHigh,
                        "CrossbarTest: This test case was properly tested..!\n");
                rc = OK;
                break;
            }
        }

        // Perform a modeset
        CHECK_RC(AttachDisplay(workingSet[i]));

        for (j = 0; j < LW0073_CTRL_CMD_DFP_ASSIGN_SOR_MAX_SORS; j++)
        {
            if (((workingSet[i] & assignSorParams.sorAssignListWithTag[j].displayMask) == workingSet[i]))
            {
                Printf(Tee::PriHigh, " displayId = 0x%08x is driven by SOR %d\n", (UINT32)workingSet[i], j);
                break;
            }
        }
    }

    return rc;
}

//! \brief DP SOR assignment test
//!
//! This test requires 4 DFP (non-LVDS)displays. The test walks through the displays
//! and performs the crossbar SOR assignments by forcing SORs in an increasing order.
//! These SORs are stored in an array and then a re-assignment is performed on these
//! displays followed by a modeset. The test makes sure that the SORs allocated
//! in the second call are same as those that were previously allocated.
//------------------------------------------------------------------------------
RC CrossbarTest::DPAssignment()
{
    DisplayIDs  workingSet;
    UINT32      i, sorIdx = 0, flag = 0, padlinksUsed = 0, padlink = 0, numOfDisplays = 0;
    RC          rc = OK;
    UINT32      sorAssignListOld[LW0073_CTRL_CMD_DFP_ASSIGN_SOR_MAX_SORS] = {0};
    UINT32      sorAssignListNew[LW0073_CTRL_CMD_DFP_ASSIGN_SOR_MAX_SORS] = {0};
    LW0073_CTRL_DFP_ASSIGN_SOR_PARAMS assignSorParams = {0};
    LW0073_CTRL_DFP_GET_PADLINK_MASK_PARAMS padlinkMaskParams = {0};

    Printf(Tee::PriHigh, "CrossbarTest: DP testcase\n");

    if(!m_onlyConnectedDisplays)
    {
        for(i = 0; i < fakedDisplays.size(); i++)
        {
            workingSet.push_back(fakedDisplays[i]);
        }
    }
    else
    {
        for (i = 0; i < m_detected.size(); i++)
        {
            if (DTIUtils::VerifUtils::IsSupportedProtocol(m_pDisplay, m_detected[i], "CRT,LVDS_LWSTOM"))
            {
                continue;
            }
            workingSet.push_back(m_detected[i]);
        }
    }

    if (workingSet.size() < LW0073_CTRL_CMD_DFP_ASSIGN_SOR_MAX_SORS)
    {
        Printf(Tee::PriHigh, "CrossbarTest: Need at least %d connected displays to complete this test\n",
            LW0073_CTRL_CMD_DFP_ASSIGN_SOR_MAX_SORS);
        CHECK_RC(RC::UNSUPPORTED_SYSTEM_CONFIG);
    }

    // run the test on DFP displays
    for (i = 0; i < workingSet.size(); i++)
    {
        //
        // Check if a display on the same padlink has already been done
        // a modeset on or not. If yes, then we skip this display on
        // the same padlink as the other one.
        //
        padlinkMaskParams.subDeviceInstance = 0;
        padlinkMaskParams.displayId = workingSet[i];
        rc = m_pDisplay->RmControl(
                    LW0073_CTRL_CMD_DFP_GET_PADLINK_MASK,
                    &padlinkMaskParams, sizeof (padlinkMaskParams));

        if (rc == OK)
        {
            for (padlink = 0; padlink < 6; padlink ++)
            {
                if (BIT(padlink) & padlinkMaskParams.padlinkMask)
                {
                    if (BIT(padlink) & padlinksUsed)
                    {
                        flag = 1;
                        break;
                    }
                    padlinksUsed |= BIT(padlink);
                }
            }
        }

        if (flag == 1)
        {
            flag = 0;
            continue;
        }

        numOfDisplays++;
        if (numOfDisplays <= LW0073_CTRL_CMD_DFP_ASSIGN_SOR_MAX_SORS)
        {
            if (OK != rc)
            {
                Printf(Tee::PriHigh,
                        "CrossbarTest: assign SOR failed: 0x%x\n", rc.Get());
                CHECK_RC(rc);
            }
        }
        else
        {
            break;
        }

        // Assign SOR for display
        assignSorParams.subDeviceInstance = 0;
        assignSorParams.displayId = workingSet[i];
        assignSorParams.sorExcludeMask = (0xf)^(BIT(numOfDisplays-1));
        assignSorParams.slaveDisplayId = 0;

        rc = m_pDisplay->RmControl(
                    LW0073_CTRL_CMD_DFP_ASSIGN_SOR,
                    &assignSorParams, sizeof (assignSorParams));

        if (OK != rc)
        {
            Printf(Tee::PriHigh,
                    "CrossbarTest: assign SOR failed: 0x%x\n", rc.Get());
            CHECK_RC(rc);
        }

        if (((workingSet[i] & assignSorParams.sorAssignListWithTag[numOfDisplays-1].displayMask) == workingSet[i]))
        {
            Printf(Tee::PriHigh, " displayId = 0x%08x is driven by SOR %d\n", (UINT32)workingSet[i], numOfDisplays-1);
            sorAssignListOld[sorIdx++] = numOfDisplays-1;
        }
        else
        {
            Printf(Tee::PriHigh, " displayId = 0x%08x could not be allocated SOR %d, something wrong!\n", (UINT32)workingSet[i], numOfDisplays-1);
            rc = RC::SOFTWARE_ERROR;
            break;
        }
    }

    sorIdx = 0;
    flag = 0;
    padlinksUsed = 0;
    numOfDisplays = 0;

    // iterate through the displays and check that re-assignment remains the same
    for (i = 0; i < workingSet.size(); i++)
    {
        //
        // Check if a display on the same padlink has already been done
        // a modeset on or not. If yes, then we skip this display on
        // the same padlink as the other one.
        //
        padlinkMaskParams.subDeviceInstance = 0;
        padlinkMaskParams.displayId = workingSet[i];
        rc = m_pDisplay->RmControl(
                    LW0073_CTRL_CMD_DFP_GET_PADLINK_MASK,
                    &padlinkMaskParams, sizeof (padlinkMaskParams));

        if (rc == OK)
        {
            for (padlink = 0; padlink < 6; padlink ++)
            {
                if (BIT(padlink) & padlinkMaskParams.padlinkMask)
                {
                    if (BIT(padlink) & padlinksUsed)
                    {
                        flag = 1;
                        break;
                    }
                    padlinksUsed |= BIT(padlink);
                }
            }
        }

        if (flag == 1)
        {
            flag = 0;
            continue;
        }

        // Assign SOR for display
        assignSorParams.subDeviceInstance = 0;
        assignSorParams.displayId = workingSet[i];
        assignSorParams.sorExcludeMask = 0;
        assignSorParams.slaveDisplayId = 0;

        rc = m_pDisplay->RmControl(
                LW0073_CTRL_CMD_DFP_ASSIGN_SOR,
                &assignSorParams, sizeof (assignSorParams));

        numOfDisplays++;

        if (numOfDisplays <= LW0073_CTRL_CMD_DFP_ASSIGN_SOR_MAX_SORS)
        {
            if (OK != rc)
            {
                Printf(Tee::PriHigh,
                        "CrossbarTest: assign SOR failed: 0x%x\n", rc.Get());
                CHECK_RC(rc);
            }
        }
        else
        {
            rc = OK;
            break;
        }

        // Perform a modeset
        CHECK_RC(AttachDisplay(workingSet[i]));

        if ((workingSet[i] & assignSorParams.sorAssignListWithTag[numOfDisplays-1].displayMask) != workingSet[i])
        {
            Printf(Tee::PriHigh, " displayId = 0x%08x is not alloacted the SOR it was previously allocated\n", (UINT32)workingSet[i]);
            rc = RC::SOFTWARE_ERROR;
            break;
        }
        else
        {
            Printf(Tee::PriHigh, " displayId = 0x%08x is driven by SOR %d\n", (UINT32)workingSet[i], numOfDisplays-1);
            sorAssignListNew[sorIdx++] = numOfDisplays-1;
        }
    }

    // check if same SORs have been allocated or not.
    for (i = 0; i < LW0073_CTRL_CMD_DFP_ASSIGN_SOR_MAX_SORS; i++)
    {
        if (sorAssignListOld[i] != sorAssignListNew[i])
        {
            Printf(Tee::PriHigh,
                "CrossbarTest: Different SORs were allocated everytime, something wrong by RM..!\n");
            CHECK_RC(RC::SOFTWARE_ERROR);
        }
    }

    return rc;
}

//! \brief LVDS SOR assignment test
//!
//! This test requires 1 LVDS and 1 or more other DFP displays. The test walks
//! through the non-LVDS displays and performs the crossbar SOR assignments and modesets.
//! Shutdown modeset is then done on the DFP(non-LVDS) display to which SOR0 is allocated
//! Then LVDS display is allocated SOR0 and checked for success of the ctrl call.
//! The ctrl call is called for LVDS such that SOR0 is deleted from the allocation list.
//! It's checked that the ctrl call fails for this case.
//------------------------------------------------------------------------------
RC CrossbarTest::LVDSAssignment()
{
    DisplayIDs  workingSet;
    DisplayID   displayOnSor0 = 0;
    UINT32      i, j, numOfDisplays = 0;
    RC          rc = OK;
    LW0073_CTRL_DFP_ASSIGN_SOR_PARAMS assignSorParams = {0};

    Printf(Tee::PriHigh, "CrossbarTest: LVDS testcase\n");

    if(!m_onlyConnectedDisplays)
    {
        for(i = 0; i < fakedDisplays.size(); i++)
        {
            workingSet.push_back(fakedDisplays[i]);
        }
    }
    else
    {
        for (i = 0; i < m_detected.size(); i++)
        {
            if (DTIUtils::VerifUtils::IsSupportedProtocol(m_pDisplay, m_detected[i], "CRT,LVDS_LWSTOM"))
            {
                continue;
            }
            workingSet.push_back(m_detected[i]);
        }
    }

    // iterate through the detected non-LVDS displays
    for (i = 0; i < workingSet.size(); i++)
    {
        if (DTIUtils::VerifUtils::IsSupportedProtocol(m_pDisplay, workingSet[i], "LVDS_LWSTOM"))
        {
            continue;
        }

        numOfDisplays++;

        if (numOfDisplays > LW0073_CTRL_CMD_DFP_ASSIGN_SOR_MAX_SORS)
            break;

        // Assign SOR for display
        assignSorParams.subDeviceInstance = 0;
        assignSorParams.displayId = workingSet[i];
        assignSorParams.sorExcludeMask = 0;
        assignSorParams.slaveDisplayId = 0;

        rc = m_pDisplay->RmControl(
                LW0073_CTRL_CMD_DFP_ASSIGN_SOR,
                &assignSorParams, sizeof (assignSorParams));

        if (OK != rc)
        {
            Printf(Tee::PriHigh,
                    "CrossbarTest: assign SOR failed: 0x%x\n", rc.Get());
            CHECK_RC(rc);
        }

        // Perform a modeset
        CHECK_RC(AttachDisplay(workingSet[i]));

        for (j = 0; j < LW0073_CTRL_CMD_DFP_ASSIGN_SOR_MAX_SORS; j++)
        {
            if (((workingSet[i] & assignSorParams.sorAssignListWithTag[j].displayMask) == workingSet[i]))
            {
                Printf(Tee::PriHigh, " displayId = 0x%08x is driven by SOR %d\n", (UINT32)workingSet[i], j);
                if (j == 0)
                {
                    displayOnSor0 = workingSet[i];
                }
                break;
            }
        }

         if (displayOnSor0 != 0)
             break;
    }

    // if a DFP was allocated SOR0 then we detach it to make availability for LVDS
    if (displayOnSor0 != 0)
    {
        CHECK_RC(DetachDisplay(displayOnSor0));
    }

    // Search for the LVDS display
    for (i = 0; i < workingSet.size(); i++)
    {
        if (DTIUtils::VerifUtils::IsSupportedProtocol(m_pDisplay, workingSet[i], "LVDS_LWSTOM"))
        {
            // Assign SOR for display
            assignSorParams.subDeviceInstance = 0;
            assignSorParams.displayId = workingSet[i];
            assignSorParams.sorExcludeMask = 0;
            assignSorParams.slaveDisplayId = 0;

            rc = m_pDisplay->RmControl(
                    LW0073_CTRL_CMD_DFP_ASSIGN_SOR,
                    &assignSorParams, sizeof (assignSorParams));

            if (OK != rc)
            {
                Printf(Tee::PriHigh,
                        "CrossbarTest: assign SOR failed: 0x%x\n", rc.Get());
                CHECK_RC(rc);
            }

            // Perform a modeset
            CHECK_RC(AttachDisplay(workingSet[i]));

            if (((workingSet[i] & assignSorParams.sorAssignListWithTag[0].displayMask) != workingSet[i]))
            {
                Printf(Tee::PriHigh,
                        "CrossbarTest: SOR assignment error, incorrect assignment for LVDS display\n");
                CHECK_RC(RC::SOFTWARE_ERROR);
            }

            CHECK_RC(DetachDisplay(workingSet[i]));

            // Re-assign sor for LVDS display with SOR0 excluded
            assignSorParams.subDeviceInstance = 0;
            assignSorParams.displayId = workingSet[i];
            assignSorParams.sorExcludeMask = 0x1;
            assignSorParams.slaveDisplayId = 0;

            rc = m_pDisplay->RmControl(
                    LW0073_CTRL_CMD_DFP_ASSIGN_SOR,
                    &assignSorParams, sizeof (assignSorParams));

            if (rc == OK)
            {
                Printf(Tee::PriHigh,
                        "CrossbarTest: Exclusive LVDS re-assignment incorrectly granted\n");
                CHECK_RC(RC::SOFTWARE_ERROR);
            }
            else
            {
                Printf(Tee::PriHigh,
                        "CrossbarTest: LVDSAssignment test case verified properly.\n");
                rc = OK;
            }

            break;
        }
    }

    return rc;
}

//! \brief Master-slave dualSST SOR assignment test
//!
//! This test requires 2 DP displays. It tests the assignment of master/slave
//! displays in a dualSST configuration and checks that the same SOR is allocated
//!.to both these displays.
//------------------------------------------------------------------------------
RC CrossbarTest::MasterSlaveAssignment()
{
    DisplayIDs  workingSet;
    UINT32      i;
    RC          rc = OK;
    LW0073_CTRL_DFP_ASSIGN_SOR_PARAMS assignSorParams = {0};

    Printf(Tee::PriHigh, "CrossbarTest: dualSST master/slave testcase\n");

    if(!m_onlyConnectedDisplays)
    {
        for(i = 0; i < fakedDisplays.size(); i++)
        {
            workingSet.push_back(fakedDisplays[i]);
        }
    }
    else
    {
        for (i = 0; i < m_detected.size(); i++)
        {
            if (DTIUtils::VerifUtils::IsSupportedProtocol(m_pDisplay, m_detected[i], "CRT,LVDS_LWSTOM"))
            {
                continue;
            }
            workingSet.push_back(m_detected[i]);
        }
    }

    if (workingSet.size() < 2)
    {
        Printf(Tee::PriHigh, "CrossbarTest: Insufficient DP displays to try this test case, need minimun 2\n");
        return RC::SOFTWARE_ERROR;
    }

    // Assign SOR for display
    assignSorParams.subDeviceInstance = 0;
    assignSorParams.displayId = workingSet[0];
    assignSorParams.slaveDisplayId = workingSet[1];
    assignSorParams.sorExcludeMask = 0;

    rc = m_pDisplay->RmControl(
            LW0073_CTRL_CMD_DFP_ASSIGN_SOR,
            &assignSorParams, sizeof (assignSorParams));

    if (OK != rc)
    {
        Printf(Tee::PriHigh,
                "CrossbarTest: assign SOR failed: 0x%x\n", rc.Get());
        CHECK_RC(rc);
    }

    for (i = 0; i < LW0073_CTRL_CMD_DFP_ASSIGN_SOR_MAX_SORS; i++)
    {
        if (workingSet[0] & assignSorParams.sorAssignListWithTag[i].displayMask)
        {
            if (workingSet[1] & assignSorParams.sorAssignListWithTag[i].displayMask)
            {
                Printf(Tee::PriHigh,
                        "CrossbarTest: SOR allocate to master-slave pair = %d\n", i);
                break;
            }
            else
            {
                Printf(Tee::PriHigh,
                        "CrossbarTest: Incorrect SOR allocated to master-slave pair. Something went wrong!\n");
                rc = RC::SOFTWARE_ERROR;
            }
        }
    }

    return rc;
}

//! \brief Dual-link SOR assignment test
//!
//! This test requires 2 or more DFP displays, one of these is a DP with lane count
//! of 8 or a TMDS with dual-links. Proper SOR assignment and primary/secondary link
//! configuration is checked for.
//------------------------------------------------------------------------------
RC CrossbarTest::DualLinkAssignment()
{
    DisplayIDs  workingSet;
    UINT32  i, j, sorId = 0, displayId = 0, data = 0, sorIdx = 0, linkcount = 0;
    RC      rc = OK;
    LW0073_CTRL_DFP_ASSIGN_SOR_PARAMS assignSorParams = {0};
    DisplayIDs::iterator iter;

    Printf(Tee::PriHigh, "CrossbarTest: dual-link testcase\n");

    if(!m_onlyConnectedDisplays)
    {
        for(i = 0; i < fakedDisplays.size(); i++)
        {
            workingSet.push_back(fakedDisplays[i]);
        }
    }
    else
    {
        for (i = 0; i < m_detected.size(); i++)
        {
            if (DTIUtils::VerifUtils::IsSupportedProtocol(m_pDisplay, m_detected[i], "CRT"))
            {
                continue;
            }
            workingSet.push_back(m_detected[i]);
        }
    }

    // iterate through all detected displays
    for (i = 0, iter = workingSet.begin(); iter < workingSet.end(); i++, iter++)
    {
        displayId = iter->Get();

        if (DTIUtils::VerifUtils::IsSupportedProtocol(m_pDisplay, displayId, "DUAL_TMDS"))
        {
            // Assign SOR for display
            assignSorParams.subDeviceInstance = 0;
            assignSorParams.displayId = displayId;
            assignSorParams.sorExcludeMask = 0;
            assignSorParams.slaveDisplayId = 0;

            rc = m_pDisplay->RmControl(
                    LW0073_CTRL_CMD_DFP_ASSIGN_SOR,
                    &assignSorParams, sizeof (assignSorParams));

            if (OK != rc)
            {
                Printf(Tee::PriHigh,
                        "CrossbarTest: assign SOR failed: 0x%x\n", rc.Get());
                CHECK_RC(rc);
            }

            for (j = 0; j < LW0073_CTRL_CMD_DFP_ASSIGN_SOR_MAX_SORS; j++)
            {
                if (((displayId & assignSorParams.sorAssignListWithTag[j].displayMask) == displayId))
                {
                    Printf(Tee::PriHigh, " displayId = 0x%08x is driven by SOR %d\n", (UINT32)displayId, j);
                    sorId = j;
                    break;
                }
            }

            // Perform a modeset
            CHECK_RC(AttachDisplay(displayId));

            linkcount = 0;

            // iterate through all pad links
            for (j = 0; j < LW_PDISP_CLK_REM_LINK_CTRL__SIZE_1; j++)
            {
                data = GetBoundGpuSubdevice()->RegRd32(LW_PDISP_CLK_REM_LINK_CTRL(j));

                sorIdx = DRF_VAL(_PDISP, _CLK_REM_LINK_CTRL, _FRONTEND, data);

                if ((sorIdx-1) == sorId)
                {
                    if (FLD_TEST_DRF(_PDISP, _CLK_REM_LINK_CTRL, _FRONTEND_SOR, _PRIMARY, data))
                    {
                        linkcount++;
                    }
                    if (FLD_TEST_DRF(_PDISP, _CLK_REM_LINK_CTRL, _FRONTEND_SOR, _SECONDARY, data))
                    {
                        linkcount++;
                    }
                }
            }

            if (linkcount != 2)
            {
                Printf(Tee::PriHigh,
                        "CrossbarTest: display: 0x%x, SOR assignment error, pad links incorrectly configured\n", displayId);
                CHECK_RC(RC::SOFTWARE_ERROR);
            }
        }
    }

    return rc;
}

//!
//! @brief Cleanup(): does nothing for this simple test.
//!
//! @param[in]  NONE
//! @param[out] NONE
//!
//! @returns    OK
//-----------------------------------------------------------------------------
RC CrossbarTest::Cleanup()
{
    return OK;
}

RC CrossbarTest::AttachDisplay(DisplayID displayid)
{
    RC rc;

    rc = m_pDisplay->SetMode(displayid, 640, 480, 32, 60);

    if (rc != OK)
    {
        Printf(Tee::PriHigh, "Modeset failed\n");
    }

    return rc;
}

RC CrossbarTest::DetachDisplay(DisplayID displayid)
{
    RC rc;
    CHECK_RC(m_pDisplay->DetachDisplay(DisplayIDs(1, displayid)));
    return rc;
}

//------------------------------------------------------------------------------
// JS Linkage
//------------------------------------------------------------------------------

//! \brief Linkage to JavaScript
//!
//! Setup the JS hierarchy to match the C++ one.
//------------------------------------------------------------------------------
JS_CLASS_INHERIT(CrossbarTest, RmTest, "Crossbar SOR assignment test\n");

CLASS_PROP_READWRITE(CrossbarTest, testcase, UINT32,
                     "subtest case to run");
CLASS_PROP_READWRITE(CrossbarTest,onlyConnectedDisplays,bool,
                     "run on only connected displays, default = 0");
CLASS_PROP_READWRITE(CrossbarTest, protocol, string,
                     "OR protocol to use for the test");
