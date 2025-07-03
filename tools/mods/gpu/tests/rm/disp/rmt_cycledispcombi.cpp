/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2006-2019 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

//!
//!
//! \brief a test to cycle through all possible valid combinations of
//! connected displays
//!

#include "gpu/tests/rmtest.h"
#include "core/include/script.h"
#include "core/include/jscript.h"
#include <time.h>
#include "core/include/platform.h"
#include "core/include/xp.h"
#include "gpu/tests/rm/utility/dtiutils.h"
#include "core/include/memcheck.h"

class CycleDispCombi : public RmTest
{
public:
    CycleDispCombi();
    virtual ~CycleDispCombi();

    virtual string IsTestSupported();

    virtual RC Setup();
    virtual RC Run();
    virtual RC Cleanup();

    RC FakeAllSupportedDisplays();

    bool combinationGenerator(vector<UINT32>& validCombi,
                            UINT32 tiralCombi,
                            UINT32 numOfHeads,
                            UINT32 numOfDisps);

    SETGET_PROP(onlyConnectedDisplays,bool);
    SETGET_PROP(manualVerif,bool);
    SETGET_PROP(generate_gold, bool);
    SETGET_PROP(protocol, string);   //Config protocol through cmdline
    SETGET_PROP(pixelclock, UINT32); //Config pixel clock through cmdline
    SETGET_PROP(rastersize, string); //Config raster size through cmdline

private:
    bool          m_onlyConnectedDisplays;
    bool          m_manualVerif;
    bool          m_generate_gold;
    string        m_protocol;
    UINT32        m_pixelclock;
    string        m_rastersize;
    DisplayIDs    workingSet;
    string        m_goldenCrcDir;
};

//! \brief CycleDispCombi constructor
//! Does SetName which has to be done inside ever test's constructor
//------------------------------------------------------------------------------
CycleDispCombi::CycleDispCombi()
{
    SetName("CycleDispCombi");
    m_onlyConnectedDisplays = false;
    m_manualVerif = false;
    m_generate_gold = false;
    m_protocol = "CRT,TV,LVDS_LWSTOM,SINGLE_TMDS_A,SINGLE_TMDS_B,DUAL_TMDS,DP_A,DP_B,EXT_TMDS_ENC,WRBK";
    m_pixelclock = 0;
    m_rastersize = "";
    m_goldenCrcDir = "DTI/Golden/CycleDispCombiTest/";
}

//! \brief CycleDispCombi destructor
//! does  nothing
//! \sa Cleanup
//------------------------------------------------------------------------------
CycleDispCombi::~CycleDispCombi()
{

}

//! \brief IsSupported
//! Checks if the test can be run in the current environment.
//! \return True if the test can be run in the current environment,
//!         false otherwise
//! The test has to run of GF11X chips but can run on all elwironments
//-----------------------------------------------------------------------------
string CycleDispCombi::IsTestSupported()
{
    return RUN_RMTEST_TRUE;
}

//! \setup Initialise internal state from JS
//! Initial state has to be setup based on the JS setting. This function
//! does the same.
//------------------------------------------------------------------------------
RC CycleDispCombi::Setup()
{
    RC rc;

    CHECK_RC(InitFromJs());

    //manual Verif must be used
    //1) When OnlyConnectedDisplays are used
    //2) And only on Hw Platforms
    if (m_manualVerif && m_onlyConnectedDisplays &&
        Platform::GetSimulationMode() != Platform::Hardware)
    {
        m_manualVerif = false;
    }

    return rc;
}

RC CycleDispCombi::FakeAllSupportedDisplays()
{

    RC rc;
    Display *pDisplay = GetDisplay();
    //get Detected displays
    DisplayIDs Detected;
    CHECK_RC(pDisplay->GetDetected(&Detected));

    // get all the Supported  displays
    DisplayIDs supported;
    CHECK_RC(pDisplay->GetConnectors(&supported));

    DisplayIDs displaysToFake;
    for(UINT32 loopCount = 0; loopCount < supported.size(); loopCount++)
    {
        if (!pDisplay->IsDispAvailInList(supported[loopCount], Detected) &&
            DTIUtils::VerifUtils::IsSupportedProtocol(pDisplay, supported[loopCount], m_protocol))
        {
            displaysToFake.push_back(supported[loopCount]);
        }
    }

    for(UINT32 loopCount = 0; loopCount < displaysToFake.size(); loopCount++)
    {
        CHECK_RC(DTIUtils::EDIDUtils::SetLwstomEdid(pDisplay, displaysToFake[loopCount], false));

        workingSet.push_back(displaysToFake[loopCount]);
    }

    return rc;
}

//! \brief Run the test
//!
//! it selects various possible valid combination for a mutiple display
//! setup. Does a set mode on these possible combinations.
//------------------------------------------------------------------------------
RC CycleDispCombi::Run()
{
    RC rc;
    DisplayIDs    Detected;
    DisplayIDs    ActiveDisplays;
    UINT32        lwrrCombiDispMask = 0;
    UINT32        Head = Display::ILWALID_HEAD;
    UINT32        width;
    UINT32        height;
    UINT32        depth;
    UINT32        refreshRate;
    bool          useSmallRaster = false;

    LW0073_CTRL_SYSTEM_GET_HEAD_ROUTING_MAP_PARAMS dispHeadMapParams;
    Display *pDisplay = GetDisplay();

    CHECK_RC(pDisplay->InitializeDisplayHW(Display::ALLOC_DISP_CHANNELS));

    //get Detected displays
    CHECK_RC(pDisplay->GetDetected(&Detected));

    //Only the protocol passed by the user must be used
    for (UINT32 i=0; i < Detected.size(); i++)
    {
        if(DTIUtils::VerifUtils::IsSupportedProtocol(pDisplay,
                                                  Detected[i],
                                                  m_protocol))
        {
            //
            // if the edid is not valid for disp id then SetLwstomEdid.
            // This is needed on emulation / simulation / silicon (when kvm used)
            //
            if ( !(DTIUtils::EDIDUtils::IsValidEdid(pDisplay, Detected[i]) &&
                   DTIUtils::EDIDUtils::EdidHasResolutions(pDisplay, Detected[i]))
            )
            {
                if((rc = DTIUtils::EDIDUtils::SetLwstomEdid(pDisplay, Detected[i], true)) != OK)
                {
                    Printf(Tee::PriLow,"SetLwstomEdid failed for displayID = 0x%X \n",
                        (UINT32)Detected[i]);
                    return rc;
                }
            }
            workingSet.push_back(Detected[i]);
        }
    }

    // Fake Display in absence of m_onlyConnectedDisplays cmdline
    if(!m_onlyConnectedDisplays)
    {
        CHECK_RC(FakeAllSupportedDisplays());
    }

    // get the head count, thats the highest no of displays we can run at once
    // consider Head Floor Sweeping
    UINT32 numHeads = Utility::CountBits(pDisplay->GetUsableHeadsMask());

    //get the no of displays to work with
    UINT32 numDisps = static_cast<UINT32>(workingSet.size());

    if (numDisps == 0)
    {
        Printf(Tee::PriHigh, "No displays detected, test aborting... \n");
        return RC::SOFTWARE_ERROR;
    }

    UINT32 allCombinations = static_cast<UINT32>(1<<(numDisps));

    vector<UINT32> validCombi;

    // initialize
    validCombi.resize(numDisps);

    if (m_rastersize.compare("") == 0)
    {
        useSmallRaster = (Platform::GetSimulationMode() != Platform::Hardware);
    }
    else
    {
        useSmallRaster = (m_rastersize.compare("small") == 0);
        if (!useSmallRaster && m_rastersize.compare("large") != 0)
        {
            // Raster size param must be either "small" or "large".
            Printf(Tee::PriHigh, "%s: Bad raster size argument: \"%s\". Treating as \"large\"\n", __FUNCTION__, m_rastersize.c_str());
        }
    }

    if (useSmallRaster)
    {
        // This is for Fmodel and RTL which are slow in producing frames
        width    = 160;
        height   = 120;
        depth  = 32;
    }
    else
    {
        width    = 640;
        height   = 480;
        depth    = 32;
    }
    refreshRate = 60;

    vector<Surface2D> CoreImage(numDisps);

    for (UINT32 trialcombi = 3; trialcombi < allCombinations; trialcombi++)
    {
        // if its a valid combination the try the one
        if (!combinationGenerator(validCombi, trialcombi, numHeads, numDisps))
            continue;

        //Detach all the selected display to get all the heads free for this new combination
        ActiveDisplays.clear();
        pDisplay->Selected(&ActiveDisplays);
        if(ActiveDisplays.size() >= 1)
            CHECK_RC(pDisplay->DetachDisplay(ActiveDisplays));

        for (UINT32 disp = 0; disp < numDisps; disp++)
        {
            if (CoreImage[disp].GetMemHandle() != 0)
                CoreImage[disp].Free();
        }

        DisplayIDs lwrrCombinationDispIDs;
        lwrrCombinationDispIDs.clear();
        //Get the Vector of DisplayID of current combination
        for(UINT32 loopCount = 0; loopCount < numDisps; loopCount++)
        {
            if(validCombi[loopCount])
                lwrrCombinationDispIDs.push_back(workingSet[loopCount]);
        }

        pDisplay->DispIDToDispMask(lwrrCombinationDispIDs, lwrrCombiDispMask);

        // zero out param structs
        memset(&dispHeadMapParams, 0,
               sizeof (LW0073_CTRL_SYSTEM_GET_HEAD_ROUTING_MAP_PARAMS));

        // Get Heads for this new Combination
        // Call the GetHeadRouting
        dispHeadMapParams.subDeviceInstance = 0;
        dispHeadMapParams.displayMask = lwrrCombiDispMask;
        dispHeadMapParams.oldDisplayMask = 0;
        dispHeadMapParams.oldHeadRoutingMap = 0;
        dispHeadMapParams.headRoutingMap = 0;

        CHECK_RC(DTIUtils::Mislwtils::GetHeadRoutingMask(pDisplay, &dispHeadMapParams));

        Printf(Tee::PriHigh, "############################################\n");
        Printf(Tee::PriHigh, "Testing Display Combinations DisplayIDs :\n");

        if (dispHeadMapParams.displayMask == 0)
        {
            Printf(Tee::PriHigh, "CycleDispCombi: Display combination '0x%08x' not supported\n",
                                   lwrrCombiDispMask);
            Printf(Tee::PriHigh, " \n ############################################ \n");
            continue;
        }
        for (UINT32 disp = 0; disp < numDisps; disp ++)
        {
            if(!validCombi[disp])
                continue;
            Printf(Tee::PriHigh,"   0x%08x    ", (UINT32)workingSet[disp]);
        }
        Printf(Tee::PriHigh, " \n ############################################ \n");

        DTIUtils::ImageUtils imgArr = DTIUtils::ImageUtils::SelectImage(width, height);

        for (UINT32 disp = 0; disp < numDisps; disp++)
        {
            if (!validCombi[disp])
                continue;

            if ((rc = DTIUtils::Mislwtils::GetHeadFromRoutingMask(workingSet[disp], dispHeadMapParams, &Head)) != OK)
            {
                Printf(Tee::PriHigh, "CycleDispCombi: GetHeadFromRoutingMask Failed to Get Head.\n ");
                return rc;
            }

            rc = pDisplay->SetupChannelImage(workingSet[disp],
                                                width,
                                                height,
                                                depth,
                                                Display::CORE_CHANNEL_ID,
                                                &CoreImage[disp],
                                                imgArr.GetImageName(),
                                                0,
                                                Head);
            if (rc != OK )
            {
                // combination is not possible so go to next combination
                // not an error here
                if(rc == RC::ILWALID_HEAD)
                {
                    rc.Clear();
                    break;
                }
                Printf(Tee::PriHigh, "CycleDispCombi: SetupChannelImage() Failed. RC = %d \n ", (UINT32)rc);
                return rc;
            }

            if (m_pixelclock)
            {

                EvoCoreChnContext *pCoreDispContext = NULL;
                pCoreDispContext = (EvoCoreChnContext *)pDisplay->GetEvoDisplay()->
                                    GetEvoDisplayChnContext(workingSet[disp], Display::CORE_CHANNEL_ID);

                EvoCoreDisplaySettings &LwstomSettings = pCoreDispContext->DispLwstomSettings;

                Printf(Tee::PriHigh, "In %s() Using PixelClock = %d \n", __FUNCTION__, m_pixelclock);

                LwstomSettings.HeadSettings[Head].PixelClkSettings.PixelClkFreqHz = m_pixelclock;
                LwstomSettings.HeadSettings[Head].PixelClkSettings.Adj1000Div1001 = false;
                LwstomSettings.HeadSettings[Head].PixelClkSettings.ClkMode = EvoPixelClockSettings::CLK_LWSTOM;
                LwstomSettings.HeadSettings[Head].PixelClkSettings.NotDriver = false;
                LwstomSettings.HeadSettings[Head].PixelClkSettings.Dirty = true;
                LwstomSettings.HeadSettings[Head].Dirty = true;
            }

            Printf(Tee::PriHigh,"Setmode on DisplayID = 0x%08x \t Head = %d \n", (UINT32)workingSet[disp], Head);
            CHECK_RC(pDisplay->SetMode(workingSet[disp],
                                        width,
                                        height,
                                        depth,
                                        refreshRate,
                                        Head));
            if(!m_manualVerif)
            {
                //removing autoverification till bug 674331 gets resolved
                // till then using only rc checks
                char        crcFileName[50];
                string      lwrProtocol = "";
                UINT32      displayClass = pDisplay->GetClass();
                string      dispClassName = "";
                CHECK_RC(DTIUtils::Mislwtils:: ColwertDisplayClassToString(displayClass,
                                                     dispClassName));
                string      goldenDir = m_goldenCrcDir + dispClassName + "/";

                if (pDisplay->GetProtocolForDisplayId(workingSet[disp], lwrProtocol) != OK)
                {
                    Printf(Tee::PriHigh, "\n Error in getting Protocol for DisplayId = 0x%08X",(UINT32)workingSet[disp]);
                    return RC::SOFTWARE_ERROR;
                }

                sprintf(crcFileName, "CycleDispCombiTest_%s_%dx%d.xml",lwrProtocol.c_str(), width, height);

                VerifyCrcTree      crcCompFlag;
                crcCompFlag.crcHeaderField.bSkipChip = crcCompFlag.crcHeaderField.bSkipPlatform = true;

                rc = DTIUtils::VerifUtils::autoVerification(GetDisplay(),
                                                  GetBoundGpuDevice(),
                                                  workingSet[disp],
                                                  width,
                                                  height,
                                                  depth,
                                                  goldenDir,
                                                  crcFileName,
                                                  &CoreImage[disp]);

                if (OK != rc)
                {
                    Printf(Tee::PriHigh, "\n %s => AutoVerification failed with RC = %d \n",
                            __FUNCTION__, (UINT32)rc);
                    if (rc == RC::FILE_DOES_NOT_EXIST)
                    {
                        if (!m_generate_gold)
                        {
                            Printf(Tee::PriHigh, "\n %s => CRC File does not exist so autoVerification was not done.\n",
                                __FUNCTION__);
                            Printf(Tee::PriHigh, "\n %s => Please Submit CRC files to perforce //sw/mods/rmtest/DTI/Golden/CycleDispCombiTest/... \n",
                                __FUNCTION__);
                        }
                        else
                        {
                            Printf(Tee::PriHigh, "\n %s Gold file got generated, \
                                                 Please submit the files generated in $MODS_RUNSPACE to perforce path\
                                                 //sw/mods/rmtest/DTI/Golden/CycleResCombi/...", crcFileName);
                            rc = OK;
                        }
                    }
                    else
                    {
                        return rc;
                    }
                }

            }
            else
            {
                rc = DTIUtils::VerifUtils::manualVerification(GetDisplay(),
                                            workingSet[disp],
                                            &CoreImage[disp],
                                            "Is image OK?(y/n)",
                                            Display::Mode(width,height,depth,refreshRate),
                                            DTI_MANUALVERIF_TIMEOUT_MS,
                                            DTI_MANUALVERIF_SLEEP_MS);

                if(rc == RC::LWRM_INSUFFICIENT_RESOURCES)
                {
                    Printf(Tee::PriHigh, "Manual verification failed with insufficient resources ignore it for now  please see bug 736351 \n");
                    rc.Clear();
                }
            }
        }
    }

    return rc;
}

//! \brief Cleanup, does nothing for this test
//------------------------------------------------------------------------------
RC CycleDispCombi::Cleanup()
{
    RC rc = OK;
    // Done with the test Detach all active
    // Displays Connected.
    Display *pDisplay = GetDisplay();
    DisplayIDs SelectedDisplays;
    pDisplay->Selected(&SelectedDisplays);

    if(SelectedDisplays.size() >= 1)
        CHECK_RC(pDisplay->DetachDisplay(SelectedDisplays));

    return  rc;
}

//! \brief combinationGenerator, does
//! takes a integer as input colwerts  it to a combination. Returns true if
//! the combination is valid
//------------------------------------------------------------------------------
bool CycleDispCombi::combinationGenerator(  vector<UINT32>& validCombi,
                                            UINT32 trialCombi,
                                            UINT32 numOfHeads,
                                            UINT32 numOfDisps)
{
    UINT32 limit = 0;

    for(UINT32 loopCount = 0; loopCount < numOfDisps; loopCount++)
    {
        validCombi[loopCount] = (trialCombi & 1);
        if (validCombi[loopCount])
            limit++;
        trialCombi = (trialCombi >> 1);
    }

    if(limit <= numOfHeads && limit > 1)
        return true;
    else
        return false;
}

//------------------------------------------------------------------------------
// JS Linkage
//------------------------------------------------------------------------------

//! \brief Linkage to JavaScript
//!
//! Setup the JS hierarchy to match the C++ one.
//------------------------------------------------------------------------------
JS_CLASS_INHERIT(CycleDispCombi, RmTest,
    "Simple test to cycle through the displays");

CLASS_PROP_READWRITE(CycleDispCombi,onlyConnectedDisplays,bool,
                     "run on only connected displays, default = 0");
CLASS_PROP_READWRITE(CycleDispCombi,manualVerif,bool,
                     "do manual verification, default = 0");
CLASS_PROP_READWRITE(CycleDispCombi,generate_gold, bool,
                     "generate golden images, default = 0");
CLASS_PROP_READWRITE(CycleDispCombi, protocol, string,
                     "OR protocol to use for the test");
CLASS_PROP_READWRITE(CycleDispCombi, pixelclock, UINT32,
                     "Desired pixel clock in Hz");
CLASS_PROP_READWRITE(CycleDispCombi, rastersize, string,
                     "Desired raster size (small/large)");

