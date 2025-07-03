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
//! \file rmt_dp2test.cpp
//! \brief dp2 functionality test
//!

#include "gpu/tests/rmtest.h"
#include "gpu/include/gpudev.h"
#include "gpu/include/gpusbdev.h"
#include "core/include/utility.h"
#include "gpu/display/evo_disp.h"
#include "gpu/display/evo_dp.h"
#include "gpu/display/evo_chns.h"
#include "gpu/tests/rm/utility/dtiutils.h"
#include "core/include/memcheck.h"

using namespace DTIUtils;
using namespace DisplayPort;

class Dp2Test : public RmTest
{
public:
    Dp2Test();
    virtual ~Dp2Test();

    virtual string IsTestSupported();

    virtual RC Setup();
    virtual RC Run();
    virtual RC Cleanup();

    SETGET_PROP(manualVerif,bool);
    SETGET_PROP(width, UINT32);
    SETGET_PROP(height, UINT32);

    RC VerifyImage(DisplayID displayID,
        LwU32 width,
        LwU32 height,
        LwU32 depth,
        LwU32 refreshRate,
        Surface2D* CoreImage);

private:
    DPDisplay* m_pDisplay;
    bool m_manualVerif;
    UINT32 m_width;
    UINT32 m_height;
};

//! \brief Dp2Test constructor
//! Does SetName which has to be done inside ever test's constructor
//------------------------------------------------------------------------------
Dp2Test::Dp2Test()
:m_pDisplay(NULL),
m_manualVerif(false)
{
    SetName("Dp2Test");
    m_width = 640;
    m_height = 480;
}

//! \brief Dp2Test destructor
//! does  nothing
//! \sa Cleanup
//------------------------------------------------------------------------------
Dp2Test::~Dp2Test()
{

}

RC Dp2Test::Setup()
{
    RC rc;

    CHECK_RC(InitFromJs());
    return rc;
}

string Dp2Test::IsTestSupported()
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
RC Dp2Test::Run()
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

    vector<Surface2D>           Images(dpConnIds.size());
    vector<DisplayIDs>          monitorIdVec(dpConnIds.size());
    vector<DPConnector*>        pConnector(dpConnIds.size());
    vector<DisplayPort::Group*> pDpGroup(dpConnIds.size());

    //
    // Set correct mode in evoDisp as this is required for displayID allocation.
    // in MST mode we do dynamic display ID while in SST mode we use connectorID
    // Here "0" is SST Mode & "1"  is MST Mode.
    //
    m_pDisplay->setDpMode(SST);

    coreImage = ImageUtils::SelectImage(width, height);

    for (LwU32 i = 0; i < (LwU32)dpConnIds.size(); i++)
    {
        pDpGroup[i] = NULL;

        //get all detected monitors in respective connectors
        CHECK_RC(m_pDisplay->EnumAllocAndGetDPDisplays(dpConnIds[i], &(pConnector[i]), &(monitorIdVec[i]), true));

        // Send NULL modeset on first connector only
        if (!i)
        {
            // Detach VBIOS screen
            pConnector[i]->notifyDetachBegin(NULL);
            m_pDisplay->getDisplay()->GetEvoDisplay()->ForceDetachHeads();
            pConnector[i]->notifyDetachEnd();
        }

        pDpGroup[i] = pConnector[i]->CreateGroup(monitorIdVec[i]);
        if (pDpGroup[i] == NULL)
        {
            Printf(Tee::PriHigh, "\n ERROR: Fail to create group\n");
            return RC::SOFTWARE_ERROR;
        }

        CHECK_RC(m_pDisplay->getDisplay()->SetupChannelImage(monitorIdVec[i].front(),
                width,
                height,
                32,
                Display::CORE_CHANNEL_ID,
                &Images[i],
                coreImage.GetImageName(),
                0));

        CHECK_RC(m_pDisplay->SetMode(dpConnIds[i], monitorIdVec[i].front(),
                pDpGroup[i],
                width,
                height,
                32,
                refreshRate));

        Printf(Tee::PriHigh, "%s : ModSet Done at connector ID 0x%x \n", __FUNCTION__, dpConnIds[i].Get());

        CHECK_RC(VerifyImage(monitorIdVec[i].front(), width, height, 32, refreshRate, &Images[i]));
    }

    // Switch displays one by one as we want to first see modeset on all possibe displays
    for (LwU32 i = 0; i < (LwU32)dpConnIds.size(); i++)
    {
        CHECK_RC(m_pDisplay->DetachDisplay(dpConnIds[i], monitorIdVec[i].front(), pDpGroup[i]));

        CHECK_RC(m_pDisplay->FreeDPConnectorDisplays(dpConnIds[i]));

        if (Images[i].GetMemHandle() != 0)
            Images[i].Free();
    }

    return rc;
}

RC Dp2Test::VerifyImage(DisplayID displayID, LwU32 width, LwU32 height, LwU32 depth, LwU32 refreshRate, Surface2D* CoreImage)
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

        sprintf(crcFileName, "Dp2Test_%s_%dx%d.xml",lwrProtocol.c_str(), (int) width, (int) height);

        CHECK_RC(DTIUtils::VerifUtils::autoVerification(GetDisplay(),
            GetBoundGpuDevice(),
            displayID,
            width,
            height,
            depth,
            string("DTI/Golden/Dp2Test/"),
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

//! \brief Cleanup, does nothing for this test
//------------------------------------------------------------------------------
RC Dp2Test::Cleanup()
{
    RC rc = OK;
    return  rc;
}

//------------------------------------------------------------------------------
// JS Linkage
//------------------------------------------------------------------------------

//! \brief Linkage to JavaScript
//!
//! Setup the JS hierarchy to match the C++ one.
//------------------------------------------------------------------------------
JS_CLASS_INHERIT(Dp2Test, RmTest,
                 "Simple test for dp1.2");
CLASS_PROP_READWRITE(Dp2Test,manualVerif,bool,
                     "do manual verification, default = 0");
CLASS_PROP_READWRITE(Dp2Test,width,UINT32,
                     "Desired width of Resolution");
CLASS_PROP_READWRITE(Dp2Test,height,UINT32,
                     "Desired height of Resolution");

