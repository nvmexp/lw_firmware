/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2011-2013,2016-2017,2019 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

//!
//! \file rmt_dpflush.cpp
//! \brief Tests DP Flush Mode
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

#define MAX_LANE_COUNT 1
#define MAX_BW_COUNT 1
#define MAX_RESOLUTIONS 5

//const LwU32 linkRateSelection[] = {RBR, HBR, HBR2};
//const LwU32 laneCountSelection[] = {1, 2,4};

const LwU32 linkRateSelection[] = {HBR};
const LwU32 laneCountSelection[] = {2};

typedef struct DPFlushResolutions_s
{
    LwU32 hres;
    LwU32 vres;
} DPFlushResolutions_t;

const DPFlushResolutions_t DPFlushRes[] =
    {
        {1920, 1080},
        {1600, 900},
        {1280, 1024},
        {1280, 720},
        {800, 600},
    };

class DPFlushTest : public RmTest
{
public:
    DPFlushTest();
    virtual ~DPFlushTest();
    virtual string IsTestSupported();

    virtual RC Setup();
    virtual RC Run();
    virtual RC Cleanup();

    RC MSTAllocation();
    SETGET_PROP(branchDevice,bool);
    SETGET_PROP(mstMode,bool);

private:
    DPDisplay*      m_pDisplay;
    bool            m_branchDevice;
    bool            m_mstMode;
    DPConnector*    m_pDpConnector;
};

//! \brief Constructor for HotPlugTest
//!
//! Set a name for the test
//!
//! \sa Setup
//------------------------------------------------------------------------------
DPFlushTest::DPFlushTest() :
                m_pDisplay(NULL),
                m_branchDevice(false),
                m_mstMode(false),
                m_pDpConnector(NULL)
{
    SetName("DPFlushTest");
    ErrorLogger::StartingTest();
    ErrorLogger::IgnoreErrorsForThisTest();
}

//! \brief Destructor for DP Flush Test
//!
//! Does nothing
//!
//! \sa Cleanup
//------------------------------------------------------------------------------
DPFlushTest::~DPFlushTest()
{
}

//! \brief IsTestSupported()
//!
//! Check whether the hardware supports EvoDisplay
//!
//! \return True if the test can be run in the current environment,
//!         false otherwise
//------------------------------------------------------------------------------
string DPFlushTest::IsTestSupported()
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
RC DPFlushTest::Setup()
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
RC DPFlushTest::Run()
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
RC DPFlushTest::Cleanup()
{
    RC rc;
    return rc;
}

//------------------------------------------------------------------------------

RC DPFlushTest::MSTAllocation()
{
    RC                                  rc;
    DisplayIDs                          connectors;
    DisplayIDs                          dpConnectors;
    DisplayIDs                          displayIDs;
    ListDevice                          deviceList;
    Display *                           pDisplay;

    // Get Display

    pDisplay = m_pDisplay->getDisplay();

    // Set to use library

    pDisplay->SetUseDPLibrary(true);

    // Initialize display hardware
    CHECK_RC(pDisplay->InitializeDisplayHW(Display::ALLOC_DISP_CHANNELS));

    //
    // Get all connected devices in case of real displays.
    // Check "m_branchDevice" flag to determine actual or fake display.
    // !m_branchDevice then consider fake display's
    //
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
        Printf(Tee::PriHigh, "Error: No DP supported connectors found\n");
        return RC::TEST_CANNOT_RUN;
    }

    //
    // Set correct mode in evoDisp as this is required for displayID allocation.
    // in MST mode we do dynamic display ID while in SST mode we use connectorID
    // Here "0" is SST Mode & "1"  is MST Mode.
    //
    m_pDisplay->setDpMode((DpMode)m_mstMode);

    //Get all monitors connected to first Connector
    CHECK_RC(m_pDisplay->EnumAllocAndGetDPDisplays(dpConnectors[0], &m_pDpConnector, &displayIDs, (m_branchDevice || !m_mstMode)));

    Printf(Tee::PriHigh, "All DP displays %d = \n",(int) displayIDs.size());
    m_pDisplay->getDisplay()->PrintDisplayIds(Tee::PriHigh, displayIDs);

    if (displayIDs.size() == 0) return RC::TEST_CANNOT_RUN;

    if (m_pDpConnector == NULL) return RC::SOFTWARE_ERROR;

    // Detach VBIOS screen
    m_pDpConnector->notifyDetachBegin(NULL);
    m_pDisplay->getDisplay()->GetEvoDisplay()->ForceDetachHeads();
    m_pDpConnector->notifyDetachEnd();

    // Start modeset and compound query
    vector<Surface2D> CoreImage(displayIDs.size());
    vector<ImageUtils> imgArr(displayIDs.size());

    vector<unsigned> width(displayIDs.size(),DPFlushRes[0].hres);
    vector<unsigned> height(displayIDs.size(),DPFlushRes[0].vres);
    vector<unsigned> depth(displayIDs.size(),24);
    vector<unsigned> refreshRate(displayIDs.size(),60);
    vector<unsigned> nullParam(displayIDs.size(),0);

    list<DisplayPort::Group*> groups (displayIDs.size());

    list<DisplayPort::Group *>::iterator groupItem;
    DisplayIDs::iterator dispItem;
    LwU32 it;

    // Create Groups
    for (dispItem=displayIDs.begin(), it=0, groupItem = groups.begin();
        dispItem != displayIDs.end(); it++, groupItem++, dispItem++)
    {
        (*groupItem) = m_pDpConnector->CreateGroup(DisplayIDs(1,dispItem->Get()));
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

    for (dispItem=displayIDs.begin(), it=0, groupItem = groups.begin();
        dispItem != displayIDs.end(); it++, groupItem++, dispItem++)
    {
        // Modeset on monitor/group
        imgArr[it] = ImageUtils::SelectImage(width[it], height[it]);
        CHECK_RC(m_pDisplay->getDisplay()->SetupChannelImage(*dispItem,
                                           width[it],
                                           height[it],
                                           32,
                                           Display::CORE_CHANNEL_ID,
                                           &CoreImage[it],
                                           imgArr[it].GetImageName(),
                                           0,
                                           it)); //Ask RM Config to allocate

        // Do setmode
        CHECK_RC(m_pDisplay->SetMode(dpConnectors[0],
                                    *dispItem,
                                    *groupItem,
                                    width[it],
                                    height[it],
                                    depth[it],
                                    refreshRate[it],
                                    it));
    }
    // All monitors at 1080p
    // Init resolution
    int ratecnt, lanecnt, rescnt;
    bool bForceRgDiv  = false;

    //
    // We force RGDiv only in MST mode. In SST mode if we fail we linkTrain at max
    // linkConfig - https://wiki.lwpu.com/gpuhwdept/index.php/GPU_Display/DP_Multistream#Flush_mode_Restrictions
    //
    if (m_mstMode)
    {
        bForceRgDiv = true;
    }

    for (ratecnt =0; ratecnt < MAX_BW_COUNT; ratecnt ++)
    {
        if (linkRateSelection[ratecnt] > m_pDpConnector->maxLinkRateSupported())
            continue;

        // Set Link Configuration parametters
        for (lanecnt = 0; lanecnt < MAX_LANE_COUNT; lanecnt ++)
        {
            // Reset previous LinkConfig
            m_pDpConnector->SetLinkConfigPolicy(DP_LINK_CONFIG_POLICY_DEFAULT, linkRateSelection[ratecnt],
                                    laneCountSelection[lanecnt], !bForceRgDiv, bForceRgDiv, m_mstMode);

            // Apply Preferred linkConfig Link Config
            m_pDpConnector->SetLinkConfigPolicy(DP_LINK_CONFIG_POLICY_PREFERRED, linkRateSelection[ratecnt],
                                    laneCountSelection[lanecnt], !bForceRgDiv, bForceRgDiv, m_mstMode);

            // Detach Screens
            for (groupItem = groups.begin(), dispItem=displayIDs.begin(), it =0;
                groupItem != groups.end(); groupItem++, dispItem++, it++)
            {
                m_pDpConnector->DetachDisplay(DisplayIDs(1,*dispItem), *groupItem);
                if (CoreImage[it].GetMemHandle() != 0)
                    CoreImage[it].Free();

            }
            // find resolution that fits
            for (rescnt = 0; rescnt < MAX_RESOLUTIONS; rescnt ++)
            {
                for (it = 0; it < displayIDs.size(); it++)
                {
                    // Set first monitor resolution
                    width[it] = DPFlushRes[rescnt].hres;
                    height[it] = DPFlushRes[rescnt].vres;
                }
                // Check if it fits
                if (m_pDpConnector->IsModePossibleMst(displayIDs,
                                      &groups,
                                      width,
                                      height,
                                      refreshRate,
                                      depth,
                                      nullParam,
                                      nullParam))
                    break;
            }

            if (rescnt == MAX_RESOLUTIONS)
            {
                // we could not find a supported resolution for the given bandwidth
                goto reset;
            }

            // Found working configuration
            Printf(Tee::PriHigh, "Selected [%ux%u] BW: %u LCNT %u.\n",DPFlushRes[rescnt].hres,
                                                                      DPFlushRes[rescnt].vres,
                                                                      linkRateSelection[ratecnt],
                                                                      laneCountSelection[lanecnt]);

            // At least One of the monitors would be in RG_DIV Now - Doing Modeset
            for (dispItem=displayIDs.begin(), it=0, groupItem = groups.begin();
                dispItem != displayIDs.end(); it++, groupItem++, dispItem++)
            {
                // Modeset on monitor/group
                imgArr[it] = ImageUtils::SelectImage(width[it], height[it]);

                // Free Channel if allocated
                if (CoreImage[it].GetMemHandle() != 0)
                    CoreImage[it].Free();
                CHECK_RC(m_pDisplay->getDisplay()->SetupChannelImage(*dispItem,
                                           width[it],
                                           height[it],
                                           32,
                                           Display::CORE_CHANNEL_ID,
                                           &CoreImage[it],
                                           imgArr[it].GetImageName(),
                                           0,
                                           it)); //Ask RM Config to allocate

                // Do setmode
                CHECK_RC(m_pDisplay->SetMode(dpConnectors[0],
                                            *dispItem,
                                            *groupItem,
                                            width[it],
                                            height[it],
                                            depth[it],
                                            refreshRate[it],
                                            it));
            }
            // Detach Screens
            for (groupItem = groups.begin(), dispItem=displayIDs.begin(), it =0;
                groupItem != groups.end(); groupItem++, dispItem++, it++)
            {
                m_pDpConnector->DetachDisplay(DisplayIDs(1,*dispItem), *groupItem);
                if (CoreImage[it].GetMemHandle() != 0)
                    CoreImage[it].Free();
            }

reset:
            // Reset previous LinkConfig
            m_pDpConnector->SetLinkConfigPolicy(DP_LINK_CONFIG_POLICY_DEFAULT, linkRateSelection[ratecnt],
                                    laneCountSelection[lanecnt], !bForceRgDiv, bForceRgDiv, m_mstMode);

            // Set back to 1080p
            for (it = 0; it < displayIDs.size(); it++)
            {
                width[it] = DPFlushRes[0].hres;
                height[it] = DPFlushRes[0].vres;
            }

            // CompoundQuery
            if (!m_pDpConnector->IsModePossibleMst(displayIDs,
                                                   &groups,
                                                   width,
                                                   height,
                                                   refreshRate,
                                                   depth,
                                                   nullParam,
                                                   nullParam))
            {
                Printf(Tee::PriHigh, "Topology Not Supported.\n");
                return RC::TEST_CANNOT_RUN;
            }

            // ModeSet
            for (dispItem=displayIDs.begin(), it=0, groupItem = groups.begin();
                dispItem != displayIDs.end(); it++, groupItem++, dispItem++)
            {
                // Modeset on monitor/group
                imgArr[it] = ImageUtils::SelectImage(width[it], height[it]);

                CHECK_RC(m_pDisplay->getDisplay()->SetupChannelImage(*dispItem,
                                           width[it],
                                           height[it],
                                           32,
                                           Display::CORE_CHANNEL_ID,
                                           &CoreImage[it],
                                           imgArr[it].GetImageName(),
                                           0,
                                           it)); //Ask RM Config to allocate

                // Do setmode
                CHECK_RC(m_pDisplay->SetMode(dpConnectors[0],
                                            *dispItem,
                                            *groupItem,
                                            width[it],
                                            height[it],
                                            depth[it],
                                            refreshRate[it],
                                            it));
            }

        }
    }

    // Detach Screens
    for (groupItem = groups.begin(), dispItem=displayIDs.begin(), it =0;
        groupItem != groups.end(); groupItem++, dispItem++, it++)
    {
        m_pDpConnector->DetachDisplay(DisplayIDs(1,*dispItem), *groupItem);
        if (CoreImage[it].GetMemHandle() != 0)
         CoreImage[it].Free();
    }
            m_pDisplay->getDisplay()->GetEvoDisplay()->DetachAllDisplays();

    m_pDisplay->FreeDPConnectorDisplays(dpConnectors[0]);
    return rc;
}

//------------------------------------------------------------------------------
// JS Linkage
//------------------------------------------------------------------------------

//! \brief Linkage to JavaScript
//!
//! Setup the JS hierarchy to match the C++ one.
//------------------------------------------------------------------------------
JS_CLASS_INHERIT(DPFlushTest, RmTest, "DP1.2 Flush Mode Test\n");
CLASS_PROP_READWRITE(DPFlushTest,branchDevice,bool,
                     "Actual Branch Device present default = 0");
CLASS_PROP_READWRITE(DPFlushTest,mstMode,bool,
                     "Running test in MST Mode default is SST Mode = 0");
