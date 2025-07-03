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
//! \file rmt_dp12lanebwcounttest.cpp
//! \brief Tests DP 1.2 IMP functionality
//! Run with the following command-line options:
//! -branchDevice if there is a real device attached
//! If not, specify sinks using -numSimulatedSinks (default: 2)
//! use -minLaneCount, -minLinkRate, -resWidth, -resHeight to specify
//! minimum limits for lane-bandwidth parameters (especially useful for
//! branch simulator since faked EDIDs support many, many resolutions)
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

#define MAX_LANE_COUNT 3
#define MAX_BW_COUNT 4

const UINT32 linkRateSelection[] = {162000000,
                                    270000000,
                                    540000000,
                                    810000000};
const UINT32 laneCountSelection[] = {1,
                                     2,
                                     4};

typedef vector<Display::Mode> Vmodes; //Vector of modes
typedef vector<Vmodes> Vvmodes; //Vector of vector of modes

class DP12LaneBandwidthCount : public RmTest
{
public:
    DP12LaneBandwidthCount();
    virtual ~DP12LaneBandwidthCount();
    virtual string IsTestSupported();

    virtual RC Setup();
    virtual RC Run();
    virtual RC Cleanup();

    RC BandwidthCount();

    SETGET_PROP(manualVerif,bool);
    SETGET_PROP(branchDevice,bool);
    SETGET_PROP(numSimulatedSinks,UINT32);
    SETGET_PROP(resWidth,UINT32);
    SETGET_PROP(resHeight,UINT32);
    SETGET_PROP(minLinkRate,UINT32);
    SETGET_PROP(minLaneCount,UINT32);
    SETGET_PROP(mstMode,bool);

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
    UINT32          m_numSimulatedSinks;
    UINT32          m_resWidth;
    UINT32          m_resHeight;
    UINT32          m_minLinkRate;
    UINT32          m_minLaneCount;
    bool            m_mstMode;
    DPConnector*    m_pDpConnector;

    void MakeModeCombinations(Vvmodes& finalResult, //final result
                              Vmodes& lwrrentResult, //current result
                              Vvmodes::iterator me,
                              Vvmodes::iterator end);
    void PrintModeCombinations(Vvmodes AlignedModeCombinations);
    UINT32 PickLaneCountLimit();
    UINT32 PickLinkRateLimit();
};

//! \brief Constructor for DP12LaneBandwidthCount
//!
//! Set a name for the test
//!
//! \sa Setup
//------------------------------------------------------------------------------
DP12LaneBandwidthCount::DP12LaneBandwidthCount() :
                m_pDisplay(NULL),
                m_manualVerif(false),
                m_branchDevice(false),
                m_numSimulatedSinks(2),
                m_resWidth(640),
                m_resHeight(480),
                m_minLinkRate(162000000),
                m_minLaneCount(1),
                m_mstMode(false),
                m_pDpConnector(NULL)
{
    SetName("DP12LaneBandwidthCount");
    ErrorLogger::StartingTest();
    ErrorLogger::IgnoreErrorsForThisTest();
}

//! \brief Destructor for HotPlugTest
//!
//! Does nothing
//!
//! \sa Cleanup
//------------------------------------------------------------------------------
DP12LaneBandwidthCount::~DP12LaneBandwidthCount()
{
}

//! \brief IsTestSupported()
//!
//! Check whether the hardware supports EvoDisplay.
//!
//! \return True if the test can be run in the current environment,
//!         false otherwise
//------------------------------------------------------------------------------
string DP12LaneBandwidthCount::IsTestSupported()
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
RC DP12LaneBandwidthCount::Setup()
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
RC DP12LaneBandwidthCount::Run()
{
    RC rc;
    m_pDisplay = new DPDisplay(GetDisplay());
    CHECK_RC(BandwidthCount());
    ErrorLogger::TestCompleted();
    return rc;
}

//! \brief Cleanup
//!
//! Does nothing
//------------------------------------------------------------------------------
RC DP12LaneBandwidthCount::Cleanup()
{
    RC rc;
    return rc;
}

void DP12LaneBandwidthCount::MakeModeCombinations(Vvmodes& finalResult, //final result
                                Vmodes& lwrrentResult, //current result
                                Vvmodes::iterator me,
                                Vvmodes::iterator end)
{
    if(me == end)
    {
        finalResult.push_back(lwrrentResult);
        return;
    }

    Vmodes& myVmodes = *me;
    for(Vmodes::iterator it = myVmodes.begin(); it != myVmodes.end(); it++)
    {
        lwrrentResult.push_back(*it); //add me
        MakeModeCombinations(finalResult, lwrrentResult, me+1, end);
        lwrrentResult.pop_back();
    }
}

RC DP12LaneBandwidthCount::BandwidthCount()
{
    RC                                  rc;
    DisplayIDs                          connectors;
    DisplayIDs                          dpConnectors;
    DisplayIDs                          displayIDs;
    ListDevice                          deviceList;
    Display *                           pDisplay;
    LwU32                               j;

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
        Printf(Tee::PriHigh, "DP12LaneBandwidthCount: ERROR: No DP supported connectors found\n");
        return RC::TEST_CANNOT_RUN;
    }

    //
    // Set correct mode in evoDisp as this is required for displayID allocation.
    // in MST mode we do dynamic display ID while in SST mode we use connectorID
    // Here "0" is SST Mode & "1"  is MST Mode.
    //
    m_pDisplay->setDpMode((DpMode)m_mstMode);

    //Get all monitors connected to first Connector
    CHECK_RC(m_pDisplay->EnumAllocAndGetDPDisplays(dpConnectors[0], &m_pDpConnector, &displayIDs, (m_branchDevice || !m_mstMode), m_numSimulatedSinks));

    Printf(Tee::PriHigh, "All DP displays %d = \n",(int) displayIDs.size());
    m_pDisplay->getDisplay()->PrintDisplayIds(Tee::PriHigh, displayIDs);

    if (m_pDpConnector == NULL)
    {
        Printf(Tee::PriHigh, "DP12LaneBandwidthCount: ERROR: Could not create connector\n");
        return RC::SOFTWARE_ERROR;
    }

    if (displayIDs.size() == 0)
    {
        Printf(Tee::PriHigh, "DP12LaneBandwidthCount: ERROR: No display Ids found\n");
        return RC::TEST_CANNOT_RUN;
    }

    // Detach VBIOS screen
    m_pDpConnector->notifyDetachBegin(NULL);
    m_pDisplay->getDisplay()->GetEvoDisplay()->ForceDetachHeads();
    m_pDpConnector->notifyDetachEnd();

    list<DisplayPort::Group*> groups(displayIDs.size());
    list<DisplayPort::Group *>::iterator groupItem;
    DisplayIDs::iterator dispItem;
    LwU32 it;
    for (dispItem=displayIDs.begin(), it=0, groupItem = groups.begin();
        dispItem != displayIDs.end(); it++, groupItem++, dispItem++)
    {
        (*groupItem) = m_pDpConnector->CreateGroup(DisplayIDs(1,dispItem->Get()));
    }

    // Reset any preferred setting first
    m_pDpConnector->SetLinkConfigPolicy(DP_LINK_CONFIG_POLICY_DEFAULT, linkRateSelection[0],
                                        laneCountSelection[0], false, false, m_mstMode);

    vector<Surface2D> CoreImage(displayIDs.size());
    vector<ImageUtils> imgArr(displayIDs.size());
    vector<LwU32> width(displayIDs.size());
    vector<LwU32> height(displayIDs.size());
    vector<LwU32> depth(displayIDs.size());
    vector<LwU32> refreshRate(displayIDs.size());
    Vmodes LwrrentResolutionCombination;

    UINT32 lowerLaneCountLimit = PickLaneCountLimit();
    UINT32 lowerLinkRateLimit = PickLinkRateLimit();

    for(UINT32 laneCount = lowerLaneCountLimit; laneCount < MAX_LANE_COUNT; laneCount++)
    {
        for(UINT32 bwCount = lowerLinkRateLimit; bwCount < MAX_BW_COUNT; bwCount++)
        {
            // Linkrate should not be more than maximum
            if (linkRateSelection[bwCount] <= (m_pDpConnector->getMaxLinkConfig().peakRate))
            {
                // Set preferred link config
                m_pDpConnector->SetLinkConfigPolicy(DP_LINK_CONFIG_POLICY_PREFERRED, linkRateSelection[bwCount],
                                                          laneCountSelection[laneCount], false, false, m_mstMode);
                Printf(Tee::PriHigh, "DP12LaneBandwidthCount: Set link config policy to "
                                     "Link Rate = %d, "
                                     "Lane Count = %d\n",
                       linkRateSelection[bwCount],
                       laneCountSelection[laneCount]);

                Vvmodes ModeCombinations;
                for(UINT32 sinkCount = 0; sinkCount < (UINT32)displayIDs.size(); sinkCount++)
                {
                    // Get supported resolutions for all display Devices attached at first connector
                    Vvmodes::iterator itListRes = ModeCombinations.begin();
                    Vmodes PossibleModes;
                    m_pDpConnector->GetPossibleModeList(displayIDs[sinkCount], PossibleModes, m_resWidth, m_resHeight);
                    ModeCombinations.insert((itListRes + sinkCount), PossibleModes);
                }

                Vvmodes AlignedModeCombinations;
                Vmodes modesTemp;
                MakeModeCombinations(AlignedModeCombinations, modesTemp, ModeCombinations.begin(), ModeCombinations.end());

                //Uncomment this to print all the possible mode combinations.
                //PrintModeCombinations(AlignedModeCombinations);

                //Loop through all possible combinations of resolution pairs (pairs = number of displays)
                for (Vvmodes::iterator it = AlignedModeCombinations.begin();
                        it != AlignedModeCombinations.end(); it++)
                {
                    LwrrentResolutionCombination = *(it);

                    //Loop through each resolution in the pair (pair = number of displays)
                    for(Vmodes::iterator modeIt = LwrrentResolutionCombination.begin();
                        modeIt != LwrrentResolutionCombination.end(); modeIt++)
                    {
                        for (dispItem=displayIDs.begin(), j=0, groupItem = groups.begin();
                                dispItem != displayIDs.end(); j++, groupItem++, dispItem++)
                        {
                            width[j] = (*modeIt).width;
                            height[j] = (*modeIt).height;
                            depth[j] = (*modeIt).depth;
                            refreshRate[j] = (*modeIt).refreshRate;

                            imgArr[j] = ImageUtils::SelectImage(width[j], height[j]);

                            CHECK_RC(m_pDisplay->getDisplay()->SetupChannelImage(*dispItem,
                                                                width[j], height[j], depth[j], Display::CORE_CHANNEL_ID,
                                                                &CoreImage[j], imgArr[j].GetImageName(), 0, 0));

                            Printf(Tee::PriHigh, "DP12LaneBandwidthCount: Setting mode on: "
                                                 "width = %d, "
                                                 "height = %d, "
                                                 "depth = %d, "
                                                 "refreshRate = %d\n",
                                   width[j], height[j], depth[j], refreshRate[j]);

                            CHECK_RC(m_pDisplay->SetMode(dpConnectors[0],
                                         *dispItem,
                                         *groupItem,
                                         width[j],
                                         height[j],
                                         depth[j],
                                         refreshRate[j],
                                         j));
                        }
                        //Check image here
                        for (dispItem=displayIDs.begin(), j=0, groupItem = groups.begin();
                            dispItem != displayIDs.end(); j++, groupItem++, dispItem++)
                        {
                            CHECK_RC(VerifyImage(*dispItem, width[j], height[j], depth[j], refreshRate[j], &CoreImage[j]));
                            Printf(Tee::PriHigh, "- DisplayID: 0x%x Verified\n", (UINT32) *dispItem);
                            if (CoreImage[j].GetMemHandle() != 0)
                                CoreImage[j].Free();
                            {
                                // Detach head
                                m_pDpConnector->notifyDetachBegin(*groupItem);
                                m_pDisplay->getDisplay()->GetEvoDisplay()->DetachDisplay(DisplayIDs(1,*dispItem));
                                m_pDpConnector->notifyDetachEnd();
                            }
                        }
                    }
                }
            }
        }
    }

    // Reset preferred setting once done
    m_pDpConnector->SetLinkConfigPolicy(DP_LINK_CONFIG_POLICY_DEFAULT, linkRateSelection[0],
                                        laneCountSelection[0], false, false, true);

    CHECK_RC(m_pDisplay->FreeDPConnectorDisplays(dpConnectors[0]));
    return rc;
}

UINT32 DP12LaneBandwidthCount::PickLaneCountLimit()
{
    if(m_minLaneCount == laneCountSelection[0])
        return 0;
    else if(m_minLaneCount == laneCountSelection[1])
        return 1;
    else if(m_minLaneCount == laneCountSelection[2])
        return 2;
    return 0;
}

UINT32 DP12LaneBandwidthCount::PickLinkRateLimit()
{
    if(m_minLinkRate == linkRateSelection[0])
        return 0;
    else if(m_minLinkRate == linkRateSelection[1])
        return 1;
    else if(m_minLinkRate == linkRateSelection[2])
        return 2;
    else if(m_minLinkRate == linkRateSelection[3])
        return 3;
    return 0;
}

void DP12LaneBandwidthCount::PrintModeCombinations(Vvmodes AlignedModeCombinations)
{
    Printf(Tee::PriHigh, "*********************************Printing Pairs**************************************\n");
    for(Vvmodes::iterator it = AlignedModeCombinations.begin(); it != AlignedModeCombinations.end(); it++)
    {
        Vmodes temp = (*it);
        Printf(Tee::PriHigh, "Pairs\n");
        for(Vmodes::iterator it2 = temp.begin(); it2 != temp.end(); it2++)
        {
            Printf(Tee::PriHigh, "width -> %d\n", (int)(*it2).width);
            Printf(Tee::PriHigh, "height -> %d\n", (int)(*it2).height);
            Printf(Tee::PriHigh, "refresh -> %d\n", (int)(*it2).refreshRate);
        }
    }
}

RC DP12LaneBandwidthCount::VerifyImage(DisplayID displayID, LwU32 width, LwU32 height, LwU32 depth, LwU32 refreshRate, Surface2D* CoreImage)
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

//------------------------------------------------------------------------------
// JS Linkage
//------------------------------------------------------------------------------

//! \brief Linkage to JavaScript
//!
//! Setup the JS hierarchy to match the C++ one.
//------------------------------------------------------------------------------
JS_CLASS_INHERIT(DP12LaneBandwidthCount, RmTest, "DP1.2 Lane count & Bandwidth change test \n");
CLASS_PROP_READWRITE(DP12LaneBandwidthCount,branchDevice,bool,
                     "Actual Branch Device present, default = false");
CLASS_PROP_READWRITE(DP12LaneBandwidthCount,numSimulatedSinks,UINT32,
                     "Number of simulated sinks");
CLASS_PROP_READWRITE(DP12LaneBandwidthCount,resWidth,UINT32,
                     "Resolution Width");
CLASS_PROP_READWRITE(DP12LaneBandwidthCount,resHeight,UINT32,
                     "Resolution Width");
CLASS_PROP_READWRITE(DP12LaneBandwidthCount,minLinkRate,UINT32,
                     "Resolution Width");
CLASS_PROP_READWRITE(DP12LaneBandwidthCount,minLaneCount,UINT32,
                     "minimum lane count");
CLASS_PROP_READWRITE(DP12LaneBandwidthCount,manualVerif,bool,
                     "do manual verification, default = 0");
CLASS_PROP_READWRITE(DP12LaneBandwidthCount,mstMode,bool,
                     "Runnung test in MST mode, default is SST Mode");
