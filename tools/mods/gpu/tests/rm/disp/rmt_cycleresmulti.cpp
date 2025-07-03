/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2006-2009,2018-2019 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

//!
//!
//! \file rmt_cycleresmulti.cpp
//! \brief a test to cycle through all general resolution on
//! all connected displays
//!

#include "gpu/tests/rmtest.h"
#include "core/include/script.h"
#include "core/include/jscript.h"
#include "core/include/rawcnsl.h"
#include "core/include/modsedid.h"
#include "kepler/gk104/dev_fbpa.h"
#include "core/include/platform.h"
#include "core/include/xp.h"
#include "gpu/tests/rm/utility/dtiutils.h"
#include "core/include/memcheck.h"

class CycleResMulti : public RmTest
{
public:
    CycleResMulti();
    virtual ~CycleResMulti();

    virtual string IsTestSupported();

    virtual RC Setup();
    virtual RC Run();
    virtual RC Cleanup();

    RC checkASR(DisplayID Display);

    SETGET_PROP(onlyConnectedDisplays, bool);
    SETGET_PROP(manualVerif, bool);
    SETGET_PROP(generate_gold, bool);
    SETGET_PROP(checkASR, bool);
    SETGET_PROP(protocol, string);   //Config protocol through cmdline
    SETGET_PROP(pixelclock, UINT32); //Config pixel clock through cmdline

private:
    bool m_onlyConnectedDisplays;
    bool m_manualVerif;
    bool m_generate_gold;
    bool m_checkASR;
    string      m_protocol;
    UINT32      m_pixelclock;
    string      m_goldenCrcDir;
};

//! \brief CycleResMulti constructor
//!
//! Sets the name of the test
//!
//------------------------------------------------------------------------------
CycleResMulti::CycleResMulti()
{
    SetName("CycleResMulti");
    m_onlyConnectedDisplays = false;
    m_manualVerif = false;
    m_generate_gold = false;
    m_checkASR   = false;
    m_protocol = "CRT,TV,LVDS_LWSTOM,SINGLE_TMDS_A,SINGLE_TMDS_B,DUAL_TMDS,DP_A,DP_B,EXT_TMDS_ENC,WRBK";
    m_pixelclock = 0;
    m_goldenCrcDir = "DTI/Golden/CycleResMultiTest/";
}

//! \brief CycleResMulti destructor
//!
//! does  nothing
//!
//! \sa Cleanup
//------------------------------------------------------------------------------
CycleResMulti::~CycleResMulti()
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
string CycleResMulti::IsTestSupported()
{
    return RUN_RMTEST_TRUE;
}

//! \setup Initialise internal state from JS
//!
//! Initial state has to be setup based on the JS setting. This function
// does the same.
//------------------------------------------------------------------------------
RC CycleResMulti::Setup()
{
    RC rc;

    CHECK_RC(InitFromJs());

    //manual Verif must be used
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
//! It reads the Edid of all the connected displays one by one and then
//! test the listed resolutions in the edid along with the ones where
//! where the BE timings don't match the listed timings at native resolution
//! and requires upscaling
//------------------------------------------------------------------------------
RC CycleResMulti::Run()
{
    RC rc;
    bool bAsrPassedAtleastOnce = false;
    Display *pDisplay = GetDisplay();

    CHECK_RC(pDisplay->InitializeDisplayHW(Display::ALLOC_DISP_CHANNELS));

    DisplayIDs Detected;
    DisplayIDs workingSet;

    // get all the Detected displays
    CHECK_RC(pDisplay->GetDetected(&Detected));

    Printf(Tee::PriHigh, "All Detected displays = \n");
    pDisplay->PrintDisplayIds(Tee::PriHigh, Detected);

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
            if (!(DTIUtils::EDIDUtils::IsValidEdid(pDisplay, Detected[i]) &&
                 DTIUtils::EDIDUtils::EdidHasResolutions(pDisplay, Detected[i]))
               )
            {
                CHECK_RC(DTIUtils::EDIDUtils::SetLwstomEdid(pDisplay, Detected[i], true));
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
            CHECK_RC(DTIUtils::EDIDUtils::SetLwstomEdid(pDisplay, displaysToFake[loopCount], false));//
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

    for( UINT32  disp = 0; disp < numDisps; disp++)
    {
        DisplayID SetModeDisplay = workingSet[disp];

        UINT32   ResCount = 0;

        Display::DisplayType DispType;
        CHECK_RC(pDisplay->GetDisplayType(SetModeDisplay, &DispType));

        Edid *pEdid =pDisplay->GetEdidMember();
        CHECK_RC(pEdid->GetNumListedResolutions(SetModeDisplay,
                                                &ResCount));

        UINT32 * Widths  = new UINT32[ResCount];
        UINT32 * Heights = new UINT32[ResCount];
        UINT32 * RefreshRates = new UINT32[ResCount];

        m_protocol = "WRBK";
        bool bIsWbor = DTIUtils::VerifUtils::IsSupportedProtocol(pDisplay,
                                                  SetModeDisplay,
                                                  m_protocol);

        rc = pEdid->GetListedResolutions(SetModeDisplay,
                                         Widths,
                                         Heights,
                                         RefreshRates);
        if(rc != OK)
        {
            Printf(Tee::PriDebug, "Failed to read EDID while getting Display "
                "Device's  listed resolutions \n");

            delete [] Widths;
            delete [] Heights;
            delete [] RefreshRates;
            return rc;
        }

        vector<Display::Mode> testModes;

        // add the general listed resolutions
        for(UINT32 Count = 0;  Count < ResCount; Count++)
        {
            testModes.push_back(Display::Mode(Widths[Count],
                                     Heights[Count],
                                     32,
                                     RefreshRates[Count]));
        }

        // depth test
        if(testModes.size())
        {
            testModes.push_back(Display::Mode(Widths[0],
                                     Heights[0],
                                     16,
                                     RefreshRates[0]));
        }

        // clean resources used
        delete [] Widths;
        delete [] Heights;
        delete [] RefreshRates;

        Surface2D CoreImage;

        DTIUtils::ImageUtils imgArr;

        for(UINT32 loopCount=0 ; loopCount < testModes.size(); loopCount++)
        {
            UINT32 color = 0;
            UINT32 onHead = Display::ILWALID_HEAD;

            if ((rc = GetDisplay()->GetHead(SetModeDisplay, &onHead)) == RC::ILWALID_HEAD)
            {
                Printf(Tee::PriHigh, "CycleDispMulti: Failed to Get Head.\n ");
                return rc;
            }

            if (m_pixelclock)
            {

                EvoCoreChnContext *pCoreDispContext = NULL;
                pCoreDispContext = (EvoCoreChnContext *)pDisplay->GetEvoDisplay()->
                                    GetEvoDisplayChnContext(SetModeDisplay, Display::CORE_CHANNEL_ID);

                EvoCoreDisplaySettings &LwstomSettings = pCoreDispContext->DispLwstomSettings;

                Printf(Tee::PriHigh, "In %s() Using PixelClock = %d \n", __FUNCTION__, m_pixelclock);

                LwstomSettings.HeadSettings[onHead].PixelClkSettings.PixelClkFreqHz = m_pixelclock;
                LwstomSettings.HeadSettings[onHead].PixelClkSettings.Adj1000Div1001 = false;
                LwstomSettings.HeadSettings[onHead].PixelClkSettings.ClkMode = EvoPixelClockSettings::CLK_LWSTOM;
                LwstomSettings.HeadSettings[onHead].PixelClkSettings.NotDriver = false;
                LwstomSettings.HeadSettings[onHead].PixelClkSettings.Dirty = true;
                LwstomSettings.HeadSettings[onHead].Dirty = true;
            }

            // Use CVT_RB timing formula for displayable resolutions
            if ((testModes[loopCount].width >= 300) &&
                (testModes[loopCount].height >= 200) &&
                (testModes[loopCount].refreshRate >= 10))
            {
                EvoRasterSettings RasterSettings;
                CHECK_RC(pDisplay->SetupEvoLwstomTimings(SetModeDisplay,
                                                  testModes[loopCount].width, //width
                                                  testModes[loopCount].height,  //height
                                                  testModes[loopCount].refreshRate,  //RR
                                                  Display::CVT_RB,
                                                  &RasterSettings,
                                                  onHead,
                                                  0,
                                                  NULL));
            }

            //For any given resolution select image of that resolution
            //else select default image
            imgArr = DTIUtils::ImageUtils::SelectImage(testModes[loopCount].width,testModes[loopCount].height);

            CHECK_RC(pDisplay->SetupChannelImage(SetModeDisplay,
                                                testModes[loopCount].width,
                                                testModes[loopCount].height,
                                                testModes[loopCount].depth,
                                                Display::CORE_CHANNEL_ID,
                                                &CoreImage,
                                                imgArr.GetImageName(),
                                                color,
                                                onHead));

            CHECK_RC(pDisplay->SetMode(SetModeDisplay,
                                               testModes[loopCount].width,
                                               testModes[loopCount].height,
                                               testModes[loopCount].depth,
                                               testModes[loopCount].refreshRate));

            if(!m_manualVerif)
            {
                char        crcFileName[80];
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

                sprintf(crcFileName, "CycleResMultiTest_%s_%dx%dx%d.xml",lwrProtocol.c_str(),
                    testModes[loopCount].width, testModes[loopCount].height,
                    testModes[loopCount].depth);

                VerifyCrcTree      crcCompFlag;
                crcCompFlag.crcHeaderField.bSkipChip = crcCompFlag.crcHeaderField.bSkipPlatform = true;
                rc = DTIUtils::VerifUtils::autoVerification(GetDisplay(),
                                                  GetBoundGpuDevice(),
                                                  SetModeDisplay,
                                                  testModes[loopCount].width,
                                                  testModes[loopCount].height,
                                                  testModes[loopCount].depth,
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
                            Printf(Tee::PriHigh, "\n %s => Please Submit CRC files to perforce //sw/mods/rmtest/DTI/Golden/CycleResMulti/... \n",
                                __FUNCTION__);
                        }
                        else
                        {
                            Printf(Tee::PriHigh, "\n %s Gold file got generated, \
                                                 Please submit the files generated in $MODS_RUNSPACE to perforce path\
                                                 //sw/mods/rmtest/DTI/Golden/CycleResMulti/...", crcFileName);
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
                                            testModes[loopCount],
                                            DTI_MANUALVERIF_TIMEOUT_MS,
                                            DTI_MANUALVERIF_SLEEP_MS);

                if(rc == RC::LWRM_INSUFFICIENT_RESOURCES)
                {
                    Printf(Tee::PriHigh, "Manual verification failed with insufficient resources ignore it for now  please see bug 736351 \n");
                    rc.Clear();
                }
            }

            if(m_checkASR)
            {
                rc = checkASR(SetModeDisplay);
                if (rc == OK)
                {
                    bAsrPassedAtleastOnce = true;
                }
            }

            if(CoreImage.GetMemHandle()!= 0)
                CoreImage.Free();

            // Changing resolution on a WBOR requires a detach/attach cycle
            if(bIsWbor)
                CHECK_RC(pDisplay->DetachDisplay(DisplayIDs(1, SetModeDisplay)));
        }

        if(!bIsWbor)
            CHECK_RC(pDisplay->DetachDisplay(DisplayIDs(1, SetModeDisplay)));

    }

    if (m_checkASR && (!bAsrPassedAtleastOnce))
    {
        Printf(Tee::PriHigh, "checkASR failed for all resolutions\n");
        return RC::SOFTWARE_ERROR;
    }

    if (rc !=OK)
    {
        Printf(Tee::PriHigh , " Test Failed rc = %d " , (UINT32) rc);
    }

   return rc;
}

//! \brief Cleanup, does nothing for this test
//------------------------------------------------------------------------------
RC CycleResMulti::Cleanup()
{
    return OK;
}

//! \brief checks whether ASR is enabled correctly
//------------------------------------------------------------------------------
RC CycleResMulti::checkASR(DisplayID Display)
{
    RC rc;
    UINT32 i, asrNum = 0, asrCycCnt = 0, asrMaxCyc = 0, asrMinCyc = 0,
            asrStallCycCnt = 0, asrStallMaxCyc = 0, asrStallMinCyc = 0;
    bool   error = false;

    // for getting Head
    UINT32 Head = Display::ILWALID_HEAD;
    CHECK_RC(GetDisplay()->GetEvoDisplay()->GetHeadByDisplayID(Display, &Head));

    // Getting Subdevice
    GpuSubdevice *pSubdev =  GetDisplay()->GetOwningDisplaySubdevice();

    for (i = 0; i < 10; ++i)
    {
        // reset the registers
        pSubdev->RegWr32(LW_PFB_FBPA_PM_ASR_NUM(Head), 0);
        pSubdev->RegWr32(LW_PFB_FBPA_PM_ASR_CYCCNT(Head), 0);
        pSubdev->RegWr32(LW_PFB_FBPA_PM_ASR_MAXCYC(Head), 0);
        pSubdev->RegWr32(LW_PFB_FBPA_PM_ASR_MINCYC(Head), 0);
        pSubdev->RegWr32(LW_PFB_FBPA_PM_ASRSTALL_CYCCNT(Head), 0);
        pSubdev->RegWr32(LW_PFB_FBPA_PM_ASRSTALL_MAXCYC(Head), 0);
        pSubdev->RegWr32(LW_PFB_FBPA_PM_ASRSTALL_MINCYC(Head), 0);

        // polling delay
        Printf(Tee::PriHigh, "delay 100ms\n");
        Tasker::Sleep(100);

        asrNum = pSubdev->RegRd32(LW_PFB_FBPA_PM_ASR_NUM(Head));
        Printf(Tee::PriHigh, "LW_PFB_FBPA_PM_ASR_NUM %d\n", asrNum);
        if (asrNum == 0)
        {
            Printf(Tee::PriHigh, "ERROR: LW_PFB_FBPA_PM_ASR_NUM not progressing\n");
            error = true;
        }

        asrCycCnt = pSubdev->RegRd32(LW_PFB_FBPA_PM_ASR_CYCCNT(Head));
        Printf(Tee::PriHigh, "LW_PFB_FBPA_PM_ASR_CYCCNT %d\n", asrCycCnt);
        if (asrCycCnt == 0)
        {
            Printf(Tee::PriHigh, "ERROR: LW_PFB_FBPA_PM_ASR_CYCCNT not progressing\n");
            error = true;
        }

        asrMaxCyc = pSubdev->RegRd32(LW_PFB_FBPA_PM_ASR_MAXCYC(Head));
        Printf(Tee::PriHigh, "LW_PFB_FBPA_PM_ASR_MAXCYC %d\n", asrMaxCyc);
        if (asrMaxCyc == 0)
        {
            Printf(Tee::PriHigh, "ERROR: LW_PFB_FBPA_PM_ASR_MAXCYC not progressing\n");
            error = true;
        }

        asrMinCyc = pSubdev->RegRd32(LW_PFB_FBPA_PM_ASR_MINCYC(Head));
        Printf(Tee::PriHigh, "LW_PFB_FBPA_PM_ASR_MINCYC %d\n", asrMinCyc);
        if (asrMinCyc == 0)
        {
            Printf(Tee::PriHigh, "ERROR: LW_PFB_FBPA_PM_ASR_MINCYC not progressing\n");
            error = true;
        }

        asrStallCycCnt = pSubdev->RegRd32(LW_PFB_FBPA_PM_ASRSTALL_CYCCNT(Head));
        Printf(Tee::PriHigh, "LW_PFB_FBPA_PM_ASRSTALL_CYCCNT %d\n", asrStallCycCnt);
        if (asrStallCycCnt == 0)
        {
            Printf(Tee::PriHigh, "ERROR: LW_PFB_FBPA_PM_ASRSTALL_CYCCNT not progressing\n");
            error = true;
        }

        asrStallMaxCyc = pSubdev->RegRd32(LW_PFB_FBPA_PM_ASRSTALL_MAXCYC(Head));
        Printf(Tee::PriHigh, "LW_PFB_FBPA_PM_ASRSTALL_MAXCYC %d\n", asrStallMaxCyc);
        if (asrStallMaxCyc == 0)
        {
            Printf(Tee::PriHigh, "ERROR: LW_PFB_FBPA_PM_ASRSTALL_MAXCYC not progressing\n");
            error = true;
        }

        asrStallMinCyc = pSubdev->RegRd32(LW_PFB_FBPA_PM_ASRSTALL_MINCYC(Head));
        Printf(Tee::PriHigh, "LW_PFB_FBPA_PM_ASRSTALL_MINCYC %d\n", asrStallMinCyc);
        if (asrStallMinCyc == 0)
        {
            Printf(Tee::PriHigh, "ERROR: LW_PFB_FBPA_PM_ASRSTALL_MINCYC not progressing\n");
            error = true;
        }

    }

    if (error)
    {
        return RC::SOFTWARE_ERROR;
    }

    return OK;
}

//------------------------------------------------------------------------------
// JS Linkage
//------------------------------------------------------------------------------

//! \brief Linkage to JavaScript
//!
//! Setup the JS hierarchy to match the C++ one.
//------------------------------------------------------------------------------
JS_CLASS_INHERIT(CycleResMulti, RmTest, "simple cycle resolution test \n");

CLASS_PROP_READWRITE(CycleResMulti,onlyConnectedDisplays,bool,
                     "run on only connected displays, default = 0");
CLASS_PROP_READWRITE(CycleResMulti,manualVerif, bool,
                     "do manual verification, default = 0");
CLASS_PROP_READWRITE(CycleResMulti,generate_gold, bool,
                     "generate golden images, default = 0");
CLASS_PROP_READWRITE(CycleResMulti,checkASR, bool,
                     "Run ASR check, default = 0");
CLASS_PROP_READWRITE(CycleResMulti, protocol, string,
                     "OR protocol to use for the test");
CLASS_PROP_READWRITE(CycleResMulti, pixelclock, UINT32,
                     "Desired pixel clock in Hz");

