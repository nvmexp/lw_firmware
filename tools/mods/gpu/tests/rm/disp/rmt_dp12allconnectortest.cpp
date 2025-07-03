/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2012-2012,2019 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

//!
//! \file rmt_dp12allconnectortest.cpp
//! \brief Tests DP 1.2 SST/MST functionality on all connectors
//!        Does modesets on all connected connectors
//!        Also fakes topologies on all non-connected connectors
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

#include <algorithm>
using namespace std;

typedef struct connectorIdStruct
{
    DisplayIDs              connectedDisplayIds;
    DisplayIDs              nonConnectedDisplayIds;
}DP_CONN_IDS;

class DP12AllConnector : public RmTest
{
public:
    DP12AllConnector();
    virtual ~DP12AllConnector();

    virtual string IsTestSupported();

    virtual RC Setup();
    virtual RC Run();
    virtual RC Cleanup();

    SETGET_PROP(manualVerif,bool);
    SETGET_PROP(numSimulatedSinks, UINT32)

    RC GetConnectorsAndRun();
    RC ModesetWrapper(DisplayIDs dpConnIds,
                      bool branchDevice,
                      vector<list<DisplayPort::Group*> > *groupsList,
                      vector<DisplayIDs> *monitorIdsList,
                      vector<DPConnector*> *pConnectorList);
    RC DetachAndFree(DisplayIDs dpConnIds,
                     vector<DisplayIDs> *monitorIdsList,
                     vector<list<DisplayPort::Group*> > *groupsList,
                     vector<DPConnector*> *connectorList);
    DisplayIDs GetSupportedConnectorsAndSinks(DisplayIDs dpConnIds,
                                              UINT32 realDevicesConnected);

    RC VerifyImage(DisplayID displayID,
        LwU32 width,
        LwU32 height,
        LwU32 depth,
        LwU32 refreshRate,
        Surface2D* CoreImage);

private:
    DPDisplay*      m_pDisplay;
    bool            m_manualVerif;
    UINT32          m_numSimulatedSinks;
    vector<UINT32>  m_perConnSimulatedSinks;
    LwU32           m_headIterator;
};

//! \brief DP12AllConnector constructor
//!
//! Does SetName which has to be done inside every test's constructor
//!
//------------------------------------------------------------------------------
DP12AllConnector::DP12AllConnector()
{
    SetName("DP12AllConnector");
    m_pDisplay = nullptr;
    m_manualVerif = false;
    m_numSimulatedSinks = 20; //Setting default to dummy value greater than number of heads
    m_perConnSimulatedSinks.clear();
    m_headIterator = 0;
}

//! \brief CycleDisp destructor
//!
//! does  nothing
//!
//! \sa Cleanup
//------------------------------------------------------------------------------
DP12AllConnector::~DP12AllConnector()
{

}

//! \brief IsTestSupported()
//!
//! Check whether the hardware supports EvoDisplay.
//!
//! \return True if the test can be run in the current environment,
//!         false otherwise
//------------------------------------------------------------------------------
string DP12AllConnector::IsTestSupported()
{
    // Must support raster extension
    Display *pDisplay = GetDisplay();
    if (pDisplay->GetDisplayClassFamily() == Display::EVO )
        return RUN_RMTEST_TRUE;
    return "EVO Display class is not supported on current platform";
}

//! \setup Initialise internal state from JS
//!
//! Initial state has to be setup based on the JS setting. This function
// does the same.
//------------------------------------------------------------------------------
RC DP12AllConnector::Setup()
{
    RC rc;
    CHECK_RC(InitFromJs());
    return rc;
}

//! \brief Run the test
//------------------------------------------------------------------------------
RC DP12AllConnector::Run()
{
    RC rc = RC::SOFTWARE_ERROR;
    ErrorLogger::StartingTest();
    ErrorLogger::IgnoreErrorsForThisTest();

    m_pDisplay = new DPDisplay(GetDisplay());

    CHECK_RC(GetConnectorsAndRun());

    ErrorLogger::TestCompleted();
    return rc;
}

//! \brief Get detected/connected connectors, and run mode-specific function
//------------------------------------------------------------------------------
RC DP12AllConnector::GetConnectorsAndRun()
{
    RC rc;
    DisplayIDs detectedConnIds, allConnIds;
    DisplayIDs detectedDpConnIds, allDpConnIds, nonConnectedDpConnIds;
    DP_CONN_IDS DpConnectorIds;

    vector<DisplayIDs>                  monitorIdsList;
    vector<list<DisplayPort::Group*> >  groupsList;
    vector<DPConnector*>                connectorList;

    vector<DisplayIDs>                  nonConnMonitorIdsList;
    vector<list<DisplayPort::Group*> >  nonConnGroupsList;
    vector<DPConnector*>                nonConnConnectorList;

    CHECK_RC(m_pDisplay->getDisplay()->InitializeDisplayHW(Display::ALLOC_DISP_CHANNELS));
    m_pDisplay->getDisplay()->SetUseDPLibrary(true);

    //Get all connectors, and the detected ones.
    CHECK_RC(m_pDisplay->getDisplay()->GetDetected(&detectedConnIds));
    CHECK_RC(m_pDisplay->getDisplay()->GetConnectors(&allConnIds));

    // Set MST Mode as this test is meant for MST mode
    m_pDisplay->setDpMode(MST);

    //from among these, get all dp connectors
    for (LwU32 i = 0; i < (LwU32)detectedConnIds.size(); i++)
    {
        if (DTIUtils::VerifUtils::IsSupportedProtocol(m_pDisplay->getDisplay(), detectedConnIds[i], "DP,DP_A,DP_B"))
            detectedDpConnIds.push_back(detectedConnIds[i]);
    }
    for (LwU32 i = 0; i < (LwU32)allConnIds.size(); i++)
    {
        if (DTIUtils::VerifUtils::IsSupportedProtocol(m_pDisplay->getDisplay(), allConnIds[i], "DP,DP_A,DP_B"))
            allDpConnIds.push_back(allConnIds[i]);
    }

    if (allDpConnIds.size() == 0)
    {
        Printf(Tee::PriHigh, "DP12AllConnector: ERROR: No DP connectors found\n");
        return RC::TEST_CANNOT_RUN;
    }

    //Find the non-connected dp connectors
    DisplayIDs::iterator found;
    for (LwU32 i = 0; i < (LwU32)allDpConnIds.size(); i++)
    {
        found = std::find(detectedDpConnIds.begin(), detectedDpConnIds.end(), allDpConnIds[i]);
        if(found == detectedDpConnIds.end())
        {
            //Not in connected dp connectors. Push
            nonConnectedDpConnIds.push_back(allDpConnIds[i]);
        }
    }

    //Now we have all the connected connectors and non-connected connectors.
    //Lets do modesets on them!

    if (!nonConnectedDpConnIds.empty())
    {
        //...Starting with faking branches on the non-connected ones.
        DisplayIDs supportedDisplayIDs = GetSupportedConnectorsAndSinks(
            nonConnectedDpConnIds, static_cast<UINT32>(detectedDpConnIds.size()));
        //MST modesets on non-connected connectors
        CHECK_RC(ModesetWrapper(supportedDisplayIDs,
                                false, //simulated device
                                &nonConnGroupsList,
                                &nonConnMonitorIdsList,
                                &nonConnConnectorList
                                ));
    }

    //Modesets on connected connectors
    if (!detectedDpConnIds.empty())
    {
        CHECK_RC(ModesetWrapper(detectedDpConnIds,
                                true, //real device
                                &groupsList,
                                &monitorIdsList,
                                &connectorList
                                ));
    }

    //Free non-connected connectors
    if (!nonConnectedDpConnIds.empty())
    {
        CHECK_RC(DetachAndFree(nonConnectedDpConnIds,
                               &nonConnMonitorIdsList,
                               &nonConnGroupsList,
                               &nonConnConnectorList
                               ));
    }

    //Free connected connectors
    if (!detectedDpConnIds.empty())
    {
        CHECK_RC(DetachAndFree(detectedDpConnIds,
                               &monitorIdsList,
                               &groupsList,
                               &connectorList));
    }

    return rc;
}

//! \brief Get all supported connectors and sinks.
//------------------------------------------------------------------------------
DisplayIDs DP12AllConnector::GetSupportedConnectorsAndSinks(DisplayIDs dpConnIds, UINT32 realDevicesConnected)
{
    UINT32 numHeads = m_pDisplay->getDisplay()->GetEvoDisplay()->GetNumHeads();
    numHeads -= realDevicesConnected;
    //Compensate for real devices connected
    //Assumption is that one head is used per connected connector
    //Diffilwlt to know in advance how many actually used

    if(m_numSimulatedSinks > numHeads)
    {
        //Number of sinks are limited by the number of heads.
        Printf(Tee::PriHigh, "DP12AllConnector: Limiting number of simulated sinks by number of heads: %d\n", numHeads);
        m_numSimulatedSinks = numHeads;
    }
    DisplayIDs supportedDpConnIds;
    DisplayIDs::iterator connIt = dpConnIds.begin();
    while(connIt != dpConnIds.end() && m_numSimulatedSinks != 0)
    {
        //Last connector -> assign all remaining sinks
        if((connIt+1) == dpConnIds.end())
        {
            m_perConnSimulatedSinks.push_back(m_numSimulatedSinks);
            m_numSimulatedSinks = 0;
        }
        //Assign 1 sink
        else
        {
            m_perConnSimulatedSinks.push_back(1);
            m_numSimulatedSinks--;
        }
        supportedDpConnIds.push_back(*connIt);
        connIt++;
    }
    return supportedDpConnIds;
}

//! \brief Do a modeset on all connectors specified by dpConnIds
//! \return the connector list created, and monitor Id and group list detected
//------------------------------------------------------------------------------
RC DP12AllConnector::ModesetWrapper(DisplayIDs dpConnIds,
                                    bool branchDevice,
                                    vector<list<DisplayPort::Group*> > *groupsList,
                                    vector<DisplayIDs> *monitorIdsList,
                                    vector<DPConnector*> *pConnectorList)
{
    RC                          rc;
    DisplayIDs                  monitorIds;
    DPConnector                 *pConnector;

    if (!branchDevice)
    {
        if (m_perConnSimulatedSinks.size() < dpConnIds.size())
        {
            Printf(Tee::PriHigh, "DP12AllConnector: ERROR: Not enough simulated sinks in vector to support connectors\n");
            return RC::TEST_CANNOT_RUN;
        }
    }

    for (LwU32 i = 0; i < (LwU32)dpConnIds.size(); i++)
    {
        pConnector = NULL;
        monitorIds.clear();
        CHECK_RC(m_pDisplay->EnumAllocAndGetDPDisplays(dpConnIds[i], &pConnector, &monitorIds, branchDevice, branchDevice ? 1 : m_perConnSimulatedSinks[i]));

        if (pConnector == NULL)
        {
            Printf(Tee::PriHigh, "DP12AllConnector: ERROR: Could not create connector\n");
            return RC::TEST_CANNOT_RUN;
        }
        if (monitorIds.size() == 0)
        {
            Printf(Tee::PriHigh, "DP12AllConnector: ERROR: No displays found\n");
            return RC::TEST_CANNOT_RUN;
        }

        //Keep copy of monitors for all connectors. Used later for detach.
        pConnectorList->push_back(pConnector);
        monitorIdsList->push_back(monitorIds);

        // Create Groups
        list<DisplayPort::Group*> groups(monitorIds.size());
        list<DisplayPort::Group *>::iterator groupItem;
        DisplayIDs::iterator dispItem;
        LwU32 it;
        for (dispItem = monitorIds.begin(), it=0, groupItem = groups.begin();
             dispItem != monitorIds.end();
             it++, groupItem++, dispItem++)
        {
            (*groupItem) = pConnector->CreateGroup(DisplayIDs(1,dispItem->Get()));
        }
        groupsList->push_back(groups);

        vector<Surface2D>   CoreImage(monitorIds.size());
        vector<ImageUtils>  imgArr(monitorIds.size());
        vector<unsigned>    width(monitorIds.size(),1024);
        vector<unsigned>    height(monitorIds.size(),768);
        vector<unsigned>    depth(monitorIds.size(),32);
        vector<unsigned>    refreshRate(monitorIds.size(),60);
        vector<unsigned>    nullParam(monitorIds.size(),0);

        // Do compound query
        if (!(pConnector->IsModePossibleMst(monitorIds,
                                              &groups,
                                              width,
                                              height,
                                              refreshRate,
                                              depth,
                                              nullParam,
                                              nullParam)))
        {
            Printf(Tee::PriHigh, "DP12AllConnector: ERROR: IMP check failed.\n");
            return RC::TEST_CANNOT_RUN;
        }

        for (dispItem=monitorIds.begin(), it=0, groupItem = groups.begin();
              dispItem != monitorIds.end(); it++, groupItem++, dispItem++)
        {
            // Modeset on monitor/group
            imgArr[it] = ImageUtils::SelectImage(width[it], height[it]);
            CHECK_RC(m_pDisplay->getDisplay()->SetupChannelImage(*dispItem,
                                                                 width[it],
                                                                 height[it],
                                                                 depth[it],
                                                                 Display::CORE_CHANNEL_ID,
                                                                 &CoreImage[it],
                                                                 imgArr[it].GetImageName(),
                                                                 0,
                                                                 it)); //Ask RM Config to allocate

            // Do setmode
            CHECK_RC(m_pDisplay->SetMode(dpConnIds[i],
                                         *dispItem,
                                         *groupItem,
                                         width[it],
                                         height[it],
                                         depth[it],
                                         refreshRate[it],
                                         m_headIterator++
                                         ));
        }

        for (groupItem = groups.begin(), dispItem=monitorIds.begin(), it =0;
              groupItem != groups.end(); groupItem++, dispItem++, it++)
        {
            CHECK_RC(VerifyImage(*dispItem, width[it], height[it], depth[it], refreshRate[it], &CoreImage[it]));
            Printf(Tee::PriHigh, "DP12AllConnector: - DisplayID: 0x%x Verified\n", (UINT32) *dispItem);
            if (CoreImage[it].GetMemHandle() != 0)
                CoreImage[it].Free();
        }
    }

    return rc;
}

//! \brief Detach all monitors, groups and connectors specified by function parameters
//------------------------------------------------------------------------------
RC DP12AllConnector::DetachAndFree(DisplayIDs dpConnIds, vector<DisplayIDs> *monitorIdsList, vector<list<DisplayPort::Group*> > *groupsList, vector<DPConnector*> *connectorList)
{
    RC rc;
    vector<DisplayIDs>::iterator lwrrentMonitorIdsIt = monitorIdsList->begin();
    vector<list<DisplayPort::Group*> >::iterator lwrrentGroupsIt = groupsList->begin();
    vector<DPConnector*>::iterator lwrrentConnectorIt = connectorList->begin();
    LwU32 i = 0;
    for (; lwrrentMonitorIdsIt != monitorIdsList->end(); lwrrentMonitorIdsIt++, lwrrentGroupsIt++, lwrrentConnectorIt++, i++)
    {
        list<DisplayPort::Group *>::iterator groupItem;
        DisplayIDs::iterator dispItem;
        LwU32 it;
        for (groupItem = (*lwrrentGroupsIt).begin(), dispItem = (*lwrrentMonitorIdsIt).begin(), it = 0;
             groupItem != (*lwrrentGroupsIt).end();
             groupItem++, dispItem++, it++)
        {
            CHECK_RC((*lwrrentConnectorIt)->DetachDisplay(DisplayIDs(1,*dispItem), *groupItem));
        }
        CHECK_RC(m_pDisplay->FreeDPConnectorDisplays(dpConnIds[i]));
    }
    return rc;
}

RC DP12AllConnector::VerifyImage(DisplayID displayID, LwU32 width, LwU32 height, LwU32 depth, LwU32 refreshRate, Surface2D* CoreImage)
{
    RC rc;

    if (!m_manualVerif)
    {
        char        crcFileName[50];
        string      lwrProtocol = "";

        if (m_pDisplay->getDisplay()->GetProtocolForDisplayId(displayID, lwrProtocol) != OK)
        {
            Printf(Tee::PriHigh, "\nDP12AllConnector: ERROR: Error in getting Protocol for DisplayId = 0x%08X",(UINT32)displayID);
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
RC DP12AllConnector::Cleanup()
{
    return OK;
}

//------------------------------------------------------------------------------
// JS Linkage
//------------------------------------------------------------------------------

//! \brief Linkage to JavaScript
//!
//! Setup the JS hierarchy to match the C++ one.
//------------------------------------------------------------------------------
JS_CLASS_INHERIT(DP12AllConnector, RmTest,
    "Simple test to cycle through the displays");
CLASS_PROP_READWRITE(DP12AllConnector,manualVerif,bool,
                     "do manual verification, default = 0");
CLASS_PROP_READWRITE(DP12AllConnector,numSimulatedSinks,UINT32,
                     "number of simulated sinks, default = 2");
