/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2011-2015,2019 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

//!
//! \file rmt_dp12mst.cpp
//! \brief Tests DP 1.2 MST functionality
//!
#include "gpu/tests/rmtest.h"
#include "gpu/include/gpudev.h"
#include "gpu/include/gpusbdev.h"
#include "core/include/utility.h"
#include "core/include/modsedid.h"
#include "gpu/display/evo_disp.h"
#include "gpu/display/evo_dp.h"
#include "gpu/display/evo_chns.h"
#include "gpu/tests/rm/utility/dtiutils.h"
#include "core/include/memcheck.h"
using namespace DTIUtils;
using namespace DisplayPort;

class DP2MstTest : public RmTest
{
public:
    DP2MstTest();
    virtual ~DP2MstTest();
    virtual string IsTestSupported();

    virtual RC Setup();
    virtual RC Run();
    virtual RC Cleanup();

    RC MSTAllocation();
    SETGET_PROP(manualVerif,bool);
    SETGET_PROP(branchDevice,bool);

    RC VerifyImage(DisplayID displayID,
                LwU32 width,
                LwU32 height,
                LwU32 depth,
                LwU32 refreshRate,
                Surface2D* CoreImage);

private:
    DPDisplay*      m_pDisplay;
    bool            m_manualVerif;
    bool            m_branchDevice;
    DPConnector*    m_pDpConnector;
};

//! \brief Constructor for HotPlugTest
//!
//! Set a name for the test
//!
//! \sa Setup
//------------------------------------------------------------------------------
DP2MstTest::DP2MstTest() :
                m_pDisplay(NULL),
                m_manualVerif(true),
                m_branchDevice(false),
                m_pDpConnector(NULL)
{
    SetName("DP2MstTest");
    ErrorLogger::StartingTest();
    ErrorLogger::IgnoreErrorsForThisTest();
}

//! \brief Destructor for HotPlugTest
//!
//! Does nothing
//!
//! \sa Cleanup
//------------------------------------------------------------------------------
DP2MstTest::~DP2MstTest()
{
}

//! \brief IsTestSupported()
//!
//! Check whether the hardware supports EvoDisplay.
//!
//! \return True if the test can be run in the current environment,
//!         false otherwise
//------------------------------------------------------------------------------
string DP2MstTest::IsTestSupported()
{
    // Must support raster extension
    Display *pDisplay = GetDisplay();
    if (pDisplay->GetDisplayClassFamily() == Display::EVO )
        return RUN_RMTEST_TRUE;
    return "EVO Display class is not supported on current platform";
}

//! \brief Setup
//!
//! Just JS settings
//
//! \return RC -> OK if everything is allocated,
//------------------------------------------------------------------------------
RC DP2MstTest::Setup()
{
    RC rc;
    CHECK_RC(InitFromJs());
    return rc;
}

//! \brief Runs the test
//!
//! Central point where all tests are run.
//!
//! \return Always OK since we have only one test and it's a visual test.
//------------------------------------------------------------------------------
RC DP2MstTest::Run()
{
    RC rc;
    m_pDisplay = new DPDisplay(GetDisplay());
    CHECK_RC(MSTAllocation());
    ErrorLogger::TestCompleted();
    return rc;
}

//! \brief Cleanup
//!
//! Does nothing
//------------------------------------------------------------------------------
RC DP2MstTest::Cleanup()
{
    RC rc;
    return rc;
}

//------------------------------------------------------------------------------

RC DP2MstTest::MSTAllocation()
{
    RC                                  rc;
    DisplayIDs                          connectors;
    DisplayIDs                          dpConnectors;
    DisplayIDs                          displayIDs;
    ListDevice                          deviceList;
    Display *                           pDisplay;
    UINT32                              i = 0;

    CHECK_RC(ErrorLogger::StartingTest());
    ErrorLogger::IgnoreErrorsForThisTest();

    // Get Display
    pDisplay = m_pDisplay->getDisplay();

    // Set to use library
    pDisplay->SetUseDPLibrary(true);

    // Initialize display hardware
    CHECK_RC(pDisplay->InitializeDisplayHW(Display::ALLOC_DISP_CHANNELS));

    CHECK_RC(m_pDisplay->getDisplay()->GetDetected(&connectors));

    const string protocols = "DP,DP_A,DP_B";

    //Get all DP supported connectors
    for (i = 0; i < (UINT32)connectors.size(); i++)
    {
        if (DTIUtils::VerifUtils::IsSupportedProtocol(pDisplay, connectors[i], protocols))
        {
            dpConnectors.push_back(connectors[i]);
        }
    }
    if (dpConnectors.empty())
    {
        Printf(Tee::PriHigh, "Error: No DP supported connectors found\n");
        return RC::TEST_CANNOT_RUN;
    }

    m_pDisplay->setDpMode(MST);

    for (i = 0; i < (UINT32)dpConnectors.size(); i++)
    {
        //Get all monitors connected
        CHECK_RC(m_pDisplay->EnumAllocAndGetDPDisplays(dpConnectors[i], &m_pDpConnector, &displayIDs, true));

        if (displayIDs.size() > 1)
        {
            break;
        }
    }

    Printf(Tee::PriHigh, "All DP displays %d = \n",(int) displayIDs.size());
    m_pDisplay->getDisplay()->PrintDisplayIds(Tee::PriHigh, displayIDs);

    if (displayIDs.size() == 0) return RC::TEST_CANNOT_RUN;

    if (m_pDpConnector == NULL) return RC::SOFTWARE_ERROR;

    // Start modeset and compound query
    vector<Surface2D> CoreImage(displayIDs.size());
    vector<ImageUtils> imgArr(displayIDs.size());

    vector<unsigned> width(displayIDs.size(),1920);
    vector<unsigned> height(displayIDs.size(),2160);
    vector<unsigned> depth(displayIDs.size(),24);
    vector<unsigned> refreshRate(displayIDs.size(),60);
    vector<unsigned> nullParam(displayIDs.size(),0);

    // make 1st monitor with a different resolution
    list<DisplayPort::Group*> groups(displayIDs.size());
    list<DisplayPort::Group *>::iterator groupItem;
    DisplayIDs::iterator dispItem;
    LwU32 it;

    // Create Groups
    for (dispItem=displayIDs.begin(), it=0, groupItem = groups.begin();
        dispItem != displayIDs.end(); it++, groupItem++, dispItem++)
    {
        (*groupItem) = m_pDpConnector->CreateGroup(DisplayIDs(1, dispItem->Get()));
    }

    // Do compound query
    if (!(m_pDpConnector->IsModePossibleMst(displayIDs,
                                      &groups,
                                      width,
                                      height,
                                      refreshRate,
                                      depth,
                                      nullParam,
                                      nullParam)))
    {
        Printf(Tee::PriHigh, "Topology Not Supported.\n");
        return RC::TEST_CANNOT_RUN;
    }

    CHECK_RC(m_pDpConnector->ConfigureSingleHeadMST(displayIDs,
                                                    groups,
                                                    width[0],
                                                    height[0],
                                                    refreshRate[0],
                                                    true));

    for (dispItem=displayIDs.begin(), it=0, groupItem = groups.begin();
        dispItem != displayIDs.end(); it++, groupItem++, dispItem++)
    {
        // Modeset on monitor/group
        imgArr[it] = ImageUtils::SelectImage(width[it]*2, height[it]);

        CHECK_RC(m_pDisplay->getDisplay()->SetupChannelImage(*dispItem,
                                           width[it]*2,
                                           height[it],
                                           32,
                                           Display::CORE_CHANNEL_ID,
                                           &CoreImage[it],
                                           imgArr[it].GetImageName(),
                                           0,
                                           it)); //Ask RM Config to allocate
    }

    CHECK_RC(m_pDisplay->SetModeDualMST(dpConnectors[i], displayIDs, groups, width, height, refreshRate, depth, 0));

    for (groupItem = groups.begin(), dispItem=displayIDs.begin(), it =0;
        groupItem != groups.end(); groupItem++, dispItem++, it++)
    {
        CHECK_RC(VerifyImage(*dispItem, width[it]*2, height[it], 32, refreshRate[it], &CoreImage[it]));
        Printf(Tee::PriHigh, "- DisplayID: 0x%x Verified\n", (UINT32) *dispItem);
        if (CoreImage[it].GetMemHandle() != 0)
            CoreImage[it].Free();
    }

    CHECK_RC(m_pDisplay->DetachDisplayDualMST(dpConnectors[i], displayIDs, groups));

    CHECK_RC(m_pDisplay->FreeDPConnectorDisplays(dpConnectors[i]));
    return rc;
}

RC DP2MstTest::VerifyImage(DisplayID displayID, LwU32 width, LwU32 height, LwU32 depth, LwU32 refreshRate, Surface2D* CoreImage)
{
    RC rc;

    if (!m_manualVerif)
    {
        char        crcFileName[50];
        string      lwrProtocol = "";

        if (m_pDisplay->getDisplay()->GetProtocolForDisplayId(displayID, lwrProtocol) != OK)
        {
            Printf(Tee::PriHigh, "\n Error in getting Protocol for DisplayId = 0x%08X",(UINT32)displayID);
            return RC::SOFTWARE_ERROR;
        }

        sprintf(crcFileName, "Dp2Test%s_%dx%d.xml",lwrProtocol.c_str(), (int) width, (int) height);

        CHECK_RC(DTIUtils::VerifUtils::autoVerification(GetDisplay(),
                                          GetBoundGpuDevice(),
                                          displayID,
                                          width,
                                          height,
                                          depth,
                                          string("DTI/Golden/CycleDispCombiTest/"),
                                          crcFileName,
                                          CoreImage));
    }
    else
    {
        rc = DTIUtils::VerifUtils::manualVerification(GetDisplay(),
                                    displayID,
                                    CoreImage,
                                    "Is image OK?(y/n)",
                                    Display::Mode(width, height, depth, refreshRate),
                                    DTI_MANUALVERIF_TIMEOUT_MS,
                                    DTI_MANUALVERIF_SLEEP_MS);

        if (rc == RC::LWRM_INSUFFICIENT_RESOURCES)
        {
            Printf(Tee::PriHigh, "Manual verification failed with insufficient resources ignore it for now  please see bug 736351 \n");
            rc.Clear();
        }
    }

    return rc;
}

//------------------------------------------------------------------------------
// JS Linkage
//------------------------------------------------------------------------------

//! \brief Linkage to JavaScript
//!
//! Setup the JS hierarchy to match the C++ one.
//------------------------------------------------------------------------------
JS_CLASS_INHERIT(DP2MstTest, RmTest, "DP1.2 Lane count & Bandwidth change test \n");
CLASS_PROP_READWRITE(DP2MstTest,branchDevice,bool,
                     "Actual Branch Device present default = 0");
CLASS_PROP_READWRITE(DP2MstTest,manualVerif,bool,
                     "do manual verification, default = 0");
