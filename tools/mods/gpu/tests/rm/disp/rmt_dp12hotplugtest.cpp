/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2012,2015-2017,2019 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

//!
//! \file rmt_dp12hotplugtest.cpp
//! \brief Tests DP 1.2 hotplugs and topologies using branch simulator
//! -branchDevice if there is a real device attached
//!        Use command-line option numSimulatedSinks to specify number of sinks (>=2)
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

class DP12Hotplug : public RmTest
{
public:
    DP12Hotplug();
    virtual ~DP12Hotplug();

    virtual string IsTestSupported();

    virtual RC Setup();
    virtual RC Run();
    virtual RC Cleanup();

    RC Hotplug();
    RC VerifyTopology(DisplayPort::Device* root, UINT32* detectedDevices);

    SETGET_PROP(numSimulatedSinks, UINT32)
    SETGET_PROP(branchDevice,bool);
    SETGET_PROP(mstMode,bool);

private:
    DPDisplay*      m_pDisplay;
    UINT32          m_numSimulatedSinks;
    DPConnector*    m_pDpConnector;
    bool            m_branchDevice;
    bool            m_mstMode;

};

//! \brief DP12AllConnector constructor
//!
//! Does SetName which has to be done inside every test's constructor
//!
//------------------------------------------------------------------------------
DP12Hotplug::DP12Hotplug()
{
    SetName("DP12Hotplug");
    m_pDisplay = nullptr;
    m_numSimulatedSinks = 2;
    m_pDpConnector = nullptr;
    m_branchDevice = false;
    m_mstMode = false;
}

//! \brief CycleDisp destructor
//!
//! does  nothing
//!
//! \sa Cleanup
//------------------------------------------------------------------------------
DP12Hotplug::~DP12Hotplug()
{

}

//! \brief IsTestSupported()
//!
//! Check whether the hardware supports EvoDisplay.
//!
//! \return True if the test can be run in the current environment,
//!         false otherwise
//------------------------------------------------------------------------------
string DP12Hotplug::IsTestSupported()
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
RC DP12Hotplug::Setup()
{
    RC rc;
    CHECK_RC(InitFromJs());
    return rc;
}

//! \brief Run the test
//!
//!
//!
//! \return
//------------------------------------------------------------------------------
RC DP12Hotplug::Run()
{
    RC rc;
    m_pDisplay = new DPDisplay(GetDisplay());
    CHECK_RC(Hotplug());
    ErrorLogger::TestCompleted();
    return rc;
}

//! \brief Cleanup
//!
//! Does nothing
//------------------------------------------------------------------------------
RC DP12Hotplug::Cleanup()
{
    RC rc;
    return rc;
}

RC DP12Hotplug::Hotplug()
{
    RC                                  rc;
    Display *                           pDisplay;
    DisplayIDs                          connectors;
    DisplayIDs                          dpConnectors;
    DisplayIDs                          displayIDs;

    // Get Display
    pDisplay = m_pDisplay->getDisplay();

    // Set to use library
    pDisplay->SetUseDPLibrary(true);

    // Initialize display hardware
    CHECK_RC(pDisplay->InitializeDisplayHW(Display::ALLOC_DISP_CHANNELS));

    // get all the available displays
    // Check "m_branchDevice" flag to determine actual or fake display.
    // !m_branchDevice then consider fake display's
    if(!m_branchDevice && m_mstMode)
    {
        // get all the available displays
        CHECK_RC(m_pDisplay->getDisplay()->GetConnectors(&connectors));
    }
    else
    {
        // get only connected displays
        CHECK_RC(m_pDisplay->getDisplay()->GetDetected(&connectors));
    }

    const string protocols = "DP,DP_A,DP_B";
    //Get all DP supported connectors
    for (UINT32 i = 0; i < (UINT32)connectors.size(); i++)
    {
        if (DTIUtils::VerifUtils::IsSupportedProtocol(pDisplay, connectors[i], protocols))
        {
            dpConnectors.push_back(connectors[i]);
        }
    }
    if (dpConnectors.empty())
    {
        Printf(Tee::PriHigh, "DP12Hotplug: ERROR: No DP supported connectors found\n");
        return RC::TEST_CANNOT_RUN;
    }

    //Force at least two sinks
    if (m_numSimulatedSinks <= 1)
    {
        Printf(Tee::PriHigh, "DP12Hotplug: Forcing simulated sinks = 2 (cannot be < 2)\n");
        m_numSimulatedSinks = 2;
    }

    //
    // Set correct mode in evoDisp as this is required for displayID allocation.
    // in MST mode we do dynamic display ID while in SST mode we use connectorID
    // Here "0" is SST Mode & "1"  is MST Mode.
    //
    m_pDisplay->setDpMode((DpMode)m_mstMode);
    //create a fake topology on the first connector
    CHECK_RC(m_pDisplay->EnumAllocAndGetDPDisplays(dpConnectors[0], &m_pDpConnector, &displayIDs, (m_branchDevice || !m_mstMode), m_numSimulatedSinks));

    if (displayIDs.size() == 0)
    {
        Printf(Tee::PriHigh, "DP12Hotplug: ERROR: No supported Display Ids to run test\n");
        return RC::TEST_CANNOT_RUN;
    }
    if (m_pDpConnector == NULL)
    {
        Printf(Tee::PriHigh, "DP12Hotplug: ERROR: Could not create connector\n");
        return RC::SOFTWARE_ERROR;
    }

    Printf(Tee::PriHigh, "DP12Hotplug: All DP displays %d = \n",(int) displayIDs.size());
    m_pDisplay->getDisplay()->PrintDisplayIds(Tee::PriHigh, displayIDs);

    UINT32 expectedBranches = m_numSimulatedSinks - 2;

    ListDevice detectedDeviceList = m_pDpConnector->GetAllDeviceList();

    UINT32 detectedSinks = 0;
    UINT32 detectedBranches = 0;
    UINT32 detectedDevices = 0;
    DisplayPort::Device* root = NULL;
    for (ListDevice::iterator it = detectedDeviceList.begin(); it != detectedDeviceList.end(); it++)
    {
        if (*it == NULL) //Shouldn't happen
            break;

        if ((*it)->isMultistream() && !(*it)->isVideoSink() && !(*it)->isAudioSink())
            detectedBranches++;
        else
            detectedSinks++;

        if ((*it)->getParent() != NULL)
        {
            if (((*it)->getParent())->getParent() == NULL)
                root = (*it)->getParent();
        }
    }

    if (root == NULL)
    {
        Printf(Tee::PriHigh, "DP12Hotplug: ERROR: Could not find root branch\n");
        return RC::SOFTWARE_ERROR;
    }

    if (detectedSinks != m_numSimulatedSinks || detectedBranches != expectedBranches)
    {
        Printf(Tee::PriHigh, "DP12Hotplug: ERROR: Incorrect number of devices detected\n");
        return RC::SOFTWARE_ERROR;
    }
    else
        Printf(Tee::PriHigh, "DP12Hotplug: Detected List has correct number of sinks and branches.\n");
    //We now know that we got the correct number of devices.

    //Let's traverse down the root port and check that output ports 1 and 2 are the only ones used
    CHECK_RC(VerifyTopology(root, &detectedDevices));
    if(detectedDevices == m_numSimulatedSinks)
        Printf(Tee::PriHigh, "DP12Hotplug: Correct topology detected\n");
    else
    {
        Printf(Tee::PriHigh, "DP12Hotplug: ERROR: Incorrect topology detected\n");
        return RC::SOFTWARE_ERROR;
    }

    CHECK_RC(m_pDisplay->FreeDPConnectorDisplays(dpConnectors[0]));
    return rc;
}

RC DP12Hotplug::VerifyTopology(DisplayPort::Device* root, UINT32* detectedDevices)
{
    RC rc;
    PortMap portmap;
    UINT32 outputMap;
    UINT16 loop = 0;

    portmap = root->getPortMap();
    outputMap = portmap.validMap & (~portmap.inputMap);

    if (!root->isMultistream() || root->isVideoSink() || root->isAudioSink())
    {
        (*detectedDevices)++;
        return OK;
    }

    while (outputMap)
    {
       if (outputMap & (1<<loop))
       {
            if (root->getChild(loop))
            {
                CHECK_RC(VerifyTopology(root->getChild(loop), detectedDevices));
            }
            else
                return RC::SOFTWARE_ERROR;
       }

       outputMap = outputMap & (~(1<<loop));
       loop++;
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
JS_CLASS_INHERIT(DP12Hotplug, RmTest, "DP1.2 Hotplug test \n");
CLASS_PROP_READWRITE(DP12Hotplug,numSimulatedSinks,UINT32,
                     "Number of simulated sinks");
CLASS_PROP_READWRITE(DP12Hotplug,branchDevice,bool,
                     "Actual Branch Device present, default = false");
CLASS_PROP_READWRITE(DP12Hotplug,mstMode,bool,
                     "Runnung test in MST mode, default is SST Mode");
