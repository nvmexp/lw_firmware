/*
* LWIDIA_COPYRIGHT_BEGIN
*
* Copyright 2013-2015,2019 by LWPU Corporation.  All rights reserved.  All
* information contained herein is proprietary and confidential to LWPU
* Corporation.  Any use, reproduction, or disclosure without the written
* permission of LWPU Corporation is prohibited.
*
* LWIDIA_COPYRIGHT_END
*/

//!
//! \file rmt_Dp2SST.cpp
//! \brief dp2SST functionality test
//! This Test does the Dp2SST modeset and HDCP Upstream Authentication check of Dp2SST monitor.
// !

#include "gpu/tests/rmtest.h"
#include "gpu/include/gpudev.h"
#include "gpu/include/gpusbdev.h"
#include "core/include/utility.h"
#include "gpu/display/evo_disp.h"
#include "gpu/display/evo_dp.h"
#include "gpu/display/evo_chns.h"
#include "gpu/tests/rm/utility/dtiutils.h"
#include "gpu/tests/rm/utility/hdcputils.h"
#include "core/include/memcheck.h"

using namespace DTIUtils;
using namespace DisplayPort;

class Dp2SST : public RmTest
{
public:
    Dp2SST();
    virtual ~Dp2SST();

    virtual string IsTestSupported();

    virtual RC Setup();
    virtual RC Run();
    virtual RC Cleanup();

    SETGET_PROP(manualVerif,bool);
    SETGET_PROP(width, UINT32);
    SETGET_PROP(height, UINT32);
    SETGET_PROP(check_hdcp, bool);
    SETGET_PROP(check_hdcp22, bool);

    RC VerifyImage(DisplayID displayID,
        LwU32 width,
        LwU32 height,
        LwU32 depth,
        LwU32 refreshRate,
        Surface2D* CoreImage);

    RC VerifyHDCP(DisplayID Display);

private:
    DPDisplay* m_pDisplay;
    bool m_manualVerif;
    bool m_check_hdcp;
    bool m_check_hdcp22;
    UINT32 m_width;
    UINT32 m_height;
    Surface2D CoreImage;
};

//! \brief Dp2SST constructor
//! Does SetName which has to be done inside ever test's constructor
//------------------------------------------------------------------------------
Dp2SST::Dp2SST()
:m_pDisplay(NULL),
m_manualVerif(false)
{
    SetName("Dp2SST");
    m_check_hdcp = false;
    m_check_hdcp22 = false;
    m_width = 800;
    m_height = 600;
}

//! \brief Dp2SST destructor
//! does  nothing
//! \sa Cleanup
//------------------------------------------------------------------------------
Dp2SST::~Dp2SST()
{

}

RC Dp2SST::Setup()
{
    RC rc;
    CHECK_RC(InitFromJs());
    return rc;
}

string Dp2SST::IsTestSupported()
{
    Display *pDisplay = GetDisplay();
    if (pDisplay->GetDisplayClassFamily() == Display::EVO )
        return RUN_RMTEST_TRUE;
    return "EVO Display class is not supported on current platform";
}

//! \brief Run the test
//!
//! It tries a SST DP modeset using DP library
//------------------------------------------------------------------------------
RC Dp2SST::Run()
{
    RC rc;
    UINT32        width;
    UINT32        height;
    UINT32        refreshRate;

    width = m_width;
    height = m_height;

    refreshRate = 60;

    DisplayIDs              connectors;
    DisplayIDs              nonDPConnIds;
    DisplayIDs              dpConnIds;
    ImageUtils              coreImage;

    CHECK_RC(ErrorLogger::StartingTest());
    ErrorLogger::IgnoreErrorsForThisTest();

    m_pDisplay = new DPDisplay(GetDisplay());
    CHECK_RC(m_pDisplay->getDisplay()->InitializeDisplayHW(Display::ALLOC_DISP_CHANNELS));

    m_pDisplay->getDisplay()->SetUseDPLibrary(true);

    //get all connected device
    CHECK_RC(m_pDisplay->getDisplay()->GetDetected(&connectors));

    //get all dp connectors
    for (LwU32 i = 0; i < (LwU32)connectors.size(); i++)
    {
        if (DTIUtils::VerifUtils::IsSupportedProtocol(m_pDisplay->getDisplay(), connectors[i], "DP,DP_A,DP_B"))
        {
            dpConnIds.push_back(connectors[i]);
        }
        else
        {
            nonDPConnIds.push_back(connectors[i]);
        }
    }

    if (dpConnIds.empty())
    {
        Printf(Tee::PriHigh, "Error: No DP supported connectors found\n");
        return RC::TEST_CANNOT_RUN;
    }

    constexpr bool FakeMonitors = false;
    if (FakeMonitors)
    {
        // fake the DPDisplayIDS to have SST using DP library.
        DisplayIDs supported;
        DisplayIDs displaysToFake;
        CHECK_RC(m_pDisplay->getDisplay()->GetConnectors(&supported));

        for(UINT32 loopCount = 0; loopCount < supported.size(); loopCount++)
        {
            if (!m_pDisplay->getDisplay()->IsDispAvailInList(supported[loopCount], connectors) &&
                DTIUtils::VerifUtils::IsSupportedProtocol(m_pDisplay->getDisplay(), supported[loopCount], "DP,DP_A,DP_B"))
            {
                displaysToFake.push_back(supported[loopCount]);
            }
        }

        string edidFile = "./edids/edid_DP_HP.txt";

        for(UINT32 loopCount = 0; loopCount < displaysToFake.size(); loopCount++)
        {
            CHECK_RC(DTIUtils::EDIDUtils::SetLwstomEdid(m_pDisplay->getDisplay(),displaysToFake[loopCount], false, edidFile));
            dpConnIds.push_back(displaysToFake[loopCount]);
        }
    }

    DisplayIDs  dp2sstConnIds;

    if(dpConnIds.size() > 1)
    {
        dp2sstConnIds.push_back(dpConnIds[0]);
        dp2sstConnIds.push_back(dpConnIds[1]);
    }
    else if((dpConnIds.size() == 1) || dpConnIds.empty())
    {
       return RC::TEST_CANNOT_RUN;
    }

    vector<Surface2D>           Images(dp2sstConnIds.size());

    vector<DPConnector*>        pConnector(dp2sstConnIds.size());
    vector<DisplayPort::Group*> pDpGroups(dp2sstConnIds.size());

    //
    // Set correct mode in evoDisp as this is required for displayID allocation.
    // in MST mode we do dynamic display ID while in SST mode we use connectorID
    // Here "0" is SST Mode & "1"  is MST Mode.
    //
    m_pDisplay->setDpMode(SST);

    coreImage = ImageUtils::SelectImage(width, height);

    vector<DisplayIDs>monitorIds;

    // pick 2 from the available Dp connectros.
    for (LwU32 i = 0; i < (LwU32)dp2sstConnIds.size(); i++)
    {
        pDpGroups[i] = NULL;

        DisplayIDs monIdsPerCnctr;
        //get all detected monitors in respective connectors
        CHECK_RC(m_pDisplay->EnumAllocAndGetDPDisplays(dp2sstConnIds[i], &(pConnector[i]), &monIdsPerCnctr, true));
        pDpGroups[i] = pConnector[i]->CreateGroup(monIdsPerCnctr);
        if (pDpGroups[i] == NULL)
        {
            Printf(Tee::PriHigh, "\n ERROR: Fail to create group\n");
            return RC::SOFTWARE_ERROR;
        }
        monitorIds.push_back(monIdsPerCnctr);
    }

    DisplayIDs dpSSTMonIds;
    for (UINT32 i = 0; i < monitorIds.size(); i++)
    {
        for (UINT32 j= 0; j < monitorIds[i].size(); j++)
        {
            dpSSTMonIds.push_back(monitorIds[i][j]);
        }
    }

    CHECK_RC(m_pDisplay->getDisplay()->SetupChannelImage(dpSSTMonIds[0],
        width,
        height,
        32,
        Display::CORE_CHANNEL_ID,
        &Images[0],
        coreImage.GetImageName(),
        0,
        0 )); //###############HEAD  HARD CODE IT ###############

    bool doModeSeton2sst = true;

    if (doModeSeton2sst)
    {
        CHECK_RC(pConnector[0]->ConfigureSingleHeadMultiStreamSST(dpSSTMonIds,
            pDpGroups,
            width,
            height,
            32,
            true));

        CHECK_RC(m_pDisplay->SetModeSingleHeadMultiStreamSST(dp2sstConnIds, dpSSTMonIds,
            pDpGroups,
            width,
            height,
            32,
            refreshRate,
            0));

        CHECK_RC(VerifyImage(dpSSTMonIds[0], width, height, 32, refreshRate, &Images[0]));

        if (m_check_hdcp)
        {
            CHECK_RC(VerifyHDCP(dpSSTMonIds[0]));
        }

        if (m_check_hdcp22)
        {
            //
            // TODO need to add code to verify bcaps for HDCP22.
            // TODO need to add code to verify Cs for 31 bit.
            // windows lwHdcpApi already does supports this..
            // for now calling verify hdcp which does verification of hdcp2.2 as well
            //.
            CHECK_RC(VerifyHDCP(dpSSTMonIds[0]));
        }

        //
        // this has to be get detach both the links. notify detach end has to be sent twice
        // inorder to deatch both the links.
        //
        CHECK_RC(m_pDisplay->DetachSingleHeadMultiStreamSST(dp2sstConnIds,
            dpSSTMonIds,
            pDpGroups));
    }
    else
    {
        CHECK_RC(m_pDisplay->SetMode(dpConnIds[0], dpSSTMonIds[0],
            pDpGroups[0],
            width,
            height,
            32,
            refreshRate,
            0));

        CHECK_RC(VerifyImage(dpSSTMonIds[0], width, height, 32, refreshRate, &Images[0]));

        CHECK_RC(m_pDisplay->DetachDisplay(dpConnIds[0],
                                           dpSSTMonIds[0],
                                           pDpGroups[0]));

    }

    return rc;
}

RC Dp2SST::VerifyImage(DisplayID displayID, LwU32 width, LwU32 height, LwU32 depth, LwU32 refreshRate, Surface2D* CoreImage)
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

        sprintf(crcFileName, "Dp2SST_%s_%dx%d.xml",lwrProtocol.c_str(), (int) width, (int) height);

        CHECK_RC(DTIUtils::VerifUtils::autoVerification(GetDisplay(),
            GetBoundGpuDevice(),
            displayID,
            width,
            height,
            depth,
            string("DTI/Golden/Dp2SST/"),
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

RC Dp2SST::VerifyHDCP(DisplayID Display)
{
    RC rc;
    HDCPUtils hdcpUtils;
    bool hdcpEnabled = false;

    CHECK_RC(hdcpUtils.Init(m_pDisplay->getDisplay(), Display));

    CHECK_RC(hdcpUtils.PerformJob(HDCPUtils::RENEGOTIATE_LINK));

    CHECK_RC(m_pDisplay->getDisplay()->GetHDCPInfo(Display, &hdcpEnabled, NULL,
                NULL, NULL, NULL, true));

    if (hdcpEnabled)
    {
        CHECK_RC(hdcpUtils.PerformJob(HDCPUtils::READ_LINK_STATUS));
    }

    return rc;
}

//! \brief Cleanup, does nothing for this test
//------------------------------------------------------------------------------
RC Dp2SST::Cleanup()
{
    RC rc = OK;

    if (CoreImage.GetMemHandle() != 0)
        CoreImage.Free();

    return  rc;
}

//------------------------------------------------------------------------------
// JS Linkage
//------------------------------------------------------------------------------

//! \brief Linkage to JavaScript
//!
//! Setup the JS hierarchy to match the C++ one.
//------------------------------------------------------------------------------
JS_CLASS_INHERIT(Dp2SST, RmTest,
                 "Simple test for dp1.2");
CLASS_PROP_READWRITE(Dp2SST,manualVerif,bool,
                     "do manual verification, default = 0");
CLASS_PROP_READWRITE(Dp2SST,width,UINT32,
                     "Desired width of Resolution");
CLASS_PROP_READWRITE(Dp2SST,height,UINT32,
                     "Desired height of Resolution");
CLASS_PROP_READWRITE(Dp2SST,check_hdcp,bool,
                    "verifies hdcp1.x on given monitor");
CLASS_PROP_READWRITE(Dp2SST,check_hdcp22,bool,
                    "verifies hdcp2.2 on given monitor");
