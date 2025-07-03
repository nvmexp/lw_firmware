/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2006-2009,2017,2019 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

//!
//!
//! \file rmt_CycleDisp.cpp
//! \brief CycleDisp test to cycle through all connected
//! displays on all heads.
//!

#include "gpu/tests/rmtest.h"
#include "core/include/script.h"
#include "core/include/jscript.h"
#include "core/include/display.h"
#include "core/include/tasker.h"
#include <time.h>
#include "core/include/platform.h"
#include "core/include/xp.h"
#include "gpu/tests/rm/utility/dtiutils.h"
#include "core/include/memcheck.h"

#define OBJ_MAX_SORS    4

class CycleDisp : public RmTest
{
public:
    CycleDisp();
    virtual ~CycleDisp();

    virtual string IsTestSupported();

    virtual RC Setup();
    virtual RC Run();
    virtual RC Cleanup();
    RC VerifyDisp(DisplayID SetModeDisplay,
                    UINT32 width,
                    UINT32 height,
                    UINT32 depth,
                    UINT32 refreshRate,
                    UINT32 thisHead);

    RC RunAllHeads(DisplayID SetModeDisplay,
        UINT32    width,
        UINT32    height,
        UINT32    depth,
        UINT32    refreshRate);

    SETGET_PROP(onlyConnectedDisplays, bool);
    SETGET_PROP(manualVerif, bool);
    SETGET_PROP(generate_gold, bool);
    SETGET_PROP(protocol, string);   //Config protocol through cmdline
    SETGET_PROP(pixelclock, UINT32); //Config pixel clock through cmdline
    SETGET_PROP(rastersize, string); //Config raster size through cmdline
    SETGET_PROP(WithAllHeads,bool);  //Config WithAllHeads through cmdline
    SETGET_PROP(withAllSors,bool);   //Config WithAllSors through cmdline
    SETGET_PROP(withAllWbors,bool);  //Config WithAllWbors through cmdline

private:
    Display     *pDisplay;
    bool        m_onlyConnectedDisplays;
    bool        m_manualVerif;
    bool        m_generate_gold;
    string      m_protocol;
    UINT32      m_pixelclock;
    string      m_rastersize;
    bool        m_WithAllHeads;
    bool        m_withAllSors;
    bool        m_withAllWbors;
    string      m_goldenCrcDir;
};

//! \brief CycleDisp constructor
//!
//! Does SetName which has to be done inside ever test's constructor
//!
//------------------------------------------------------------------------------
CycleDisp::CycleDisp()
{
    SetName("CycleDisp");
    pDisplay = nullptr;
    m_onlyConnectedDisplays = false;
    m_manualVerif = false;
    m_generate_gold = false;
    m_protocol = "CRT,TV,LVDS_LWSTOM,SINGLE_TMDS_A,SINGLE_TMDS_B,DUAL_TMDS,DP_A,DP_B,EXT_TMDS_ENC,WRBK";
    m_pixelclock = 0;
    m_rastersize = "";
    m_WithAllHeads = false;
    m_withAllSors = false;
    m_withAllWbors = false;
    m_goldenCrcDir = "DTI/Golden/CycleDispTest/";
}

//! \brief CycleDisp destructor
//!
//! does  nothing
//!
//! \sa Cleanup
//------------------------------------------------------------------------------
CycleDisp::~CycleDisp()
{

}

//! \brief IsSupported
//!
//! Checks if the test can be run in the current environment.
//!
//! \return True if the test can be run in the current environment,
//!         false otherwise
//! The test has to run of GF11X chips but can run on all elwironments
//------------------------------------------------------------------------------
string CycleDisp::IsTestSupported()
{
    return RUN_RMTEST_TRUE;
}

//! \setup Initialise internal state from JS
//!
//! Initial state has to be setup based on the JS setting. This function
// does the same.
//------------------------------------------------------------------------------
RC CycleDisp::Setup()
{
    RC rc;

    CHECK_RC(InitFromJs());

    pDisplay = GetDisplay();

    // manual Verif must be used
    //1) When OnlyConnectedDisplays are used
    //2) And only on Hw Platforms
    if (m_manualVerif && (!m_onlyConnectedDisplays ||
        Platform::GetSimulationMode() != Platform::Hardware))
    {
        m_manualVerif = false;
    }

    return rc;
}

//! \brief Run the test
//!
//! Call the SetMode() API with some default values
//!
//! \return OK if the SetMode call succeeded, else corresponding rc
//------------------------------------------------------------------------------
RC CycleDisp::Run()
{
    RC rc;
    bool isXbarSupported =  FALSE;

    CHECK_RC(pDisplay->InitializeDisplayHW(Display::ALLOC_DISP_CHANNELS));

    if (pDisplay->GetOwningDisplaySubdevice()->HasFeature(Device::GPUSUB_SUPPORTS_SOR_XBAR))
    {
        isXbarSupported = TRUE;
    }

    // get all the connected displays
    DisplayIDs Detected;
    CHECK_RC(pDisplay->GetDetected(&Detected));
    Printf(Tee::PriHigh, "All Detected displays = \n");
    pDisplay->PrintDisplayIds(Tee::PriHigh, Detected);

    DisplayIDs workingSet;

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
            if (! (DTIUtils::EDIDUtils::IsValidEdid(pDisplay, Detected[i]) &&
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

    Printf(Tee::PriHigh, "\n Detected displays with protocol %s \n",m_protocol.c_str());
    pDisplay->PrintDisplayIds(Tee::PriHigh, workingSet);

    //If !m_onlyConnectedDisplays then consider fake displays too
    if(!m_onlyConnectedDisplays)
    {
        //get all the available displays
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
            CHECK_RC(DTIUtils::EDIDUtils::SetLwstomEdid(pDisplay,displaysToFake[loopCount], false));
            workingSet.push_back(displaysToFake[loopCount]);
        }

    }

    //get the no of displays to work with
    UINT32 numDisps = static_cast<UINT32>(workingSet.size());

    if (numDisps == 0)
    {
        Printf(Tee::PriHigh, "No displays detected, test aborting... \n");
        return RC::SOFTWARE_ERROR;
    }

    UINT32 width;
    UINT32 height;
    UINT32 depth;
    UINT32 refreshRate;
    bool   useSmallRaster = false;

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
        depth    = 32;
        refreshRate  = 60;
    }
    else
    {
        width    = 640;
        height   = 480;
        depth    = 32;
        refreshRate = 60;
    }

    UINT32 numSors = OBJ_MAX_SORS;
    UINT32 sorNum  = 0;
    UINT32 sorExcludeMask = 0;
    vector<UINT32>sorList;
    bool   bFirstWbor = true;

    for(UINT32 disp = 0 ; disp < numDisps ; disp++)
    {
        DisplayID SetModeDisplay = workingSet[disp];

        if (DTIUtils::VerifUtils::IsSupportedProtocol(pDisplay, SetModeDisplay, "WRBK"))
        {
            if (!m_withAllWbors && !bFirstWbor)
                continue;

            // We just need to try this displayID on all heads
            // For wrbk CRC
            CHECK_RC(RunAllHeads(SetModeDisplay, width, height, depth, refreshRate));
            bFirstWbor = false;
        }
        else
        {
            sorNum  = 0;
            while(sorNum < numSors)
            {
                if (!(DTIUtils::VerifUtils::IsSupportedProtocol(pDisplay, SetModeDisplay, "CRT")))
                {
                    if (isXbarSupported)
                    {
                        sorExcludeMask = (0xf)^(BIT(sorNum));

                        if ((m_protocol.compare("EDP") == 0) || (m_protocol.compare("eDP") == 0))
                        {
                            //
                            // On eDP, GPIO pin in the vbios decides which SOR will be used for
                            // running the sequencer. Also, The ASSR bit that is set in BeforeLinkTraining
                            // on an eDP panel is per-SOR and a sticky bit. Every SOR that you use eDP
                            // on you'd have to set this bit, and once that bit is set you cannot use
                            // an external panel on it. So SOR number is fixed for eDP. Don't pass SOR exclude
                            // mask for eDP
                            //
                            sorExcludeMask = 0;
                        }

                        CHECK_RC(pDisplay->AssignSOR(DisplayIDs(1, SetModeDisplay),
                            sorExcludeMask, sorList, Display::AssignSorSetting::ONE_HEAD_MODE));

                        for (UINT32 i = 0; i < OBJ_MAX_SORS; i++)
                        {
                            if ((SetModeDisplay & sorList[i]) == SetModeDisplay)
                            {
                                Printf(Tee::PriHigh, "Using SOR = %d\n", sorNum);
                                break;
                            }
                        }
                    }
                }

                CHECK_RC(RunAllHeads(SetModeDisplay, width, height, depth, refreshRate));

                //
                // You should not try with different SORs if the display is LVDS (as LVDS is supported only on SOR0)
                // Also, if the given display is a CRT then no point of verifying on various SORs, it'll make
                // use of DAC. On eDP, GPIO pin in the vbios decides which SOR will be used for running the sequencer.
                // So SOR number is fixed for eDP.
                //
                if ((m_withAllSors) &&
                    (!(DTIUtils::VerifUtils::IsSupportedProtocol(pDisplay, SetModeDisplay, "CRT,LVDS_LWSTOM,EDP"))))
                {
                    sorNum ++;
                }
                else
                {
                     break;
                }
            }
        }
    }

    return rc;
}

RC CycleDisp::RunAllHeads
(
    DisplayID SetModeDisplay,
    UINT32    width,
    UINT32    height,
    UINT32    depth,
    UINT32    refreshRate
)
{
    RC rc;
    UINT32 HeadNum  = 0;
    UINT32 numHeads = GetDisplay()->GetHeadCount();

    while (HeadNum < numHeads)
    {
        UINT32 thisHead = Display::ILWALID_HEAD;
        if (m_WithAllHeads)
        {
            thisHead = HeadNum;
        }
        else
        {
            if ((rc = GetDisplay()->GetHead(SetModeDisplay, &thisHead)) == RC::ILWALID_HEAD)
            {
                Printf(Tee::PriHigh, "CycleDispMulti: Failed to Get Head.\n ");
                return rc;
            }
        }

        CHECK_RC(VerifyDisp(SetModeDisplay, width, height, depth, refreshRate, thisHead));

        if (m_WithAllHeads)
        {
            HeadNum ++;
        }
        else
        {
           break;
        }
    }

    return OK;
}

RC CycleDisp::VerifyDisp(DisplayID SetModeDisplay,
                          UINT32 width,
                          UINT32 height,
                          UINT32 depth,
                          UINT32 refreshRate,
                          UINT32 thisHead)
{
    RC rc = OK;
    Surface2D CoreImage;

    if (m_pixelclock)
    {
        EvoCoreChnContext *pCoreDispContext = NULL;
        pCoreDispContext = (EvoCoreChnContext *)pDisplay->GetEvoDisplay()->
                            GetEvoDisplayChnContext(SetModeDisplay, Display::CORE_CHANNEL_ID);

        EvoCoreDisplaySettings &LwstomSettings = pCoreDispContext->DispLwstomSettings;

        Printf(Tee::PriHigh, "In %s() Using PixelClock = %d \n", __FUNCTION__, m_pixelclock);

        LwstomSettings.HeadSettings[thisHead].PixelClkSettings.PixelClkFreqHz = m_pixelclock;
        LwstomSettings.HeadSettings[thisHead].PixelClkSettings.Adj1000Div1001 = false;
        LwstomSettings.HeadSettings[thisHead].PixelClkSettings.ClkMode = EvoPixelClockSettings::CLK_LWSTOM;
        LwstomSettings.HeadSettings[thisHead].PixelClkSettings.NotDriver = false;
        LwstomSettings.HeadSettings[thisHead].PixelClkSettings.Dirty = true;
        LwstomSettings.HeadSettings[thisHead].Dirty = true;
    }

    //For any given resolution select image of that resolution
    //else select default image
    DTIUtils::ImageUtils imgArr = DTIUtils::ImageUtils::SelectImage(width, height);

    CHECK_RC(pDisplay->SetupChannelImage(SetModeDisplay,
                                                width,
                                                height,
                                                depth,
                                                Display::CORE_CHANNEL_ID,
                                                &CoreImage,
                                                imgArr.GetImageName(),
                                                0,/*color*/
                                                thisHead));

    CHECK_RC(pDisplay->SetMode(SetModeDisplay,
                                        width,
                                        height,
                                        depth,
                                        refreshRate,
                                        thisHead));

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

        if (pDisplay->GetProtocolForDisplayId(SetModeDisplay, lwrProtocol) != OK)
        {
            Printf(Tee::PriHigh, "\n Error in getting Protocol for DisplayId = 0x%08X",(UINT32)SetModeDisplay);
            return RC::SOFTWARE_ERROR;
        }

        sprintf(crcFileName, "CycleDispTest_%s_%dx%d.xml",lwrProtocol.c_str(), width, height);

        VerifyCrcTree      crcCompFlag;
        crcCompFlag.crcHeaderField.bSkipChip = crcCompFlag.crcHeaderField.bSkipPlatform = true;
        rc = DTIUtils::VerifUtils::autoVerification(GetDisplay(),
                                    GetBoundGpuDevice(),
                                    SetModeDisplay,
                                    width,
                                    height,
                                    depth,
                                    goldenDir,
                                    crcFileName,
                                    &CoreImage);

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
                     Printf(Tee::PriHigh, "\n %s => Please Submit CRC files to perforce //sw/mods/rmtest/DTI/Golden/CycleDispTest/... \n",
                                    __FUNCTION__);
                }
                else
                {
                    Printf(Tee::PriHigh, "\n %s Gold file got generated, \
                                       Please submit the files generated in $MODS_RUNSPACE to perforce path\
                                       //sw/mods/rmtest/DTI/Golden/CycleDispTest/...", crcFileName);
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
                                                        SetModeDisplay,
                                                        &CoreImage,
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

    CHECK_RC(pDisplay->DetachDisplay(DisplayIDs(1, SetModeDisplay)));

    if(CoreImage.GetMemHandle() != 0)
        CoreImage.Free();

    return rc;
}

//! \brief Cleanup, does nothing for this test
//------------------------------------------------------------------------------
RC CycleDisp::Cleanup()
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
JS_CLASS_INHERIT(CycleDisp, RmTest,
    "Simple test to cycle through the displays");

CLASS_PROP_READWRITE(CycleDisp,onlyConnectedDisplays,bool,
                     "run on only connected displays, default = 0");
CLASS_PROP_READWRITE(CycleDisp,manualVerif,bool,
                     "do manual verification, default = 0");
CLASS_PROP_READWRITE(CycleDisp, generate_gold, bool,
                     "generates gold files, default = 0");
CLASS_PROP_READWRITE(CycleDisp, protocol, string,
                     "OR protocol to use for the test");
CLASS_PROP_READWRITE(CycleDisp, pixelclock, UINT32,
                     "Desired pixel clock in Hz");
CLASS_PROP_READWRITE(CycleDisp, rastersize, string,
                     "Desired raster size (small/large)");
CLASS_PROP_READWRITE(CycleDisp, WithAllHeads, bool,
                     "Cycle through displays with all heads");
CLASS_PROP_READWRITE(CycleDisp, withAllSors, bool,
                     "Cycle through displays on all SORs");
CLASS_PROP_READWRITE(CycleDisp, withAllWbors, bool,
                     "Cycle through displays on all WBORs");
