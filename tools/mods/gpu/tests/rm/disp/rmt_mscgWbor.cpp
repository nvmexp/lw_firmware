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
//! \file rmt_MscgWbor.cpp
//! \brief MscgWbor test to enter into MSCG state
//! displays on all heads.
//!

#include "gpu/tests/rmtest.h"
#include "core/include/script.h"
#include "core/include/jscript.h"
#include "core/include/display.h"
#include "gpu/display/evo_disp.h"
#include "core/include/tasker.h"
#include <time.h>
#include "core/include/platform.h"
#include "core/include/xp.h"
#include "gpu/tests/rm/utility/dtiutils.h"
#include "pascal/gp106/dev_pwr_pri.h"
#include "lwtiming.h"
#include "gpu/display/modeset_utils.h"
#include "core/include/memcheck.h"

#include <sstream>

class MscgWbor : public RmTest
{
public:
    MscgWbor();
    virtual ~MscgWbor();

    virtual string IsTestSupported();

    virtual RC Setup();
    virtual RC Run();
    virtual RC Cleanup();

    SETGET_PROP(Bug1699931, bool);

private:
    Display       *pDisplay;
    GpuSubdevice  *m_Parent;
    EvoDisplay    *pEvoDisp;
    DisplayIDs    workingSet;
    string        m_protocol;
    bool          m_Bug1699931;

    RC DumpFrame(
        Surface2D& surface,
        UINT32 headNum,
        UINT32 idx,
        UINT32 surfaceNum
    );
};

RC MscgWbor::DumpFrame
(
    Surface2D& surface,
    UINT32 headNum,
    UINT32 idx,
    UINT32 surfaceNum
)
{
    RC rc;
    std::ostringstream os;

    os << "wrbkOnHead" << headNum << "_frame" << idx
       << "_surface" << surfaceNum;

    os << ".png";
    CHECK_RC(surface.WritePng(os.str().c_str(),
        GetBoundGpuSubdevice()->GetSubdeviceInst()));

    return OK;
}

//! \brief MscgWbor constructor
//!
//! Does SetName which has to be done inside ever test's constructor
//!
//------------------------------------------------------------------------------
MscgWbor::MscgWbor()
{
    SetName("MscgWbor");
    pDisplay = nullptr;
    m_Parent = nullptr;
    pEvoDisp = nullptr;
    m_protocol = "WRBK";
    m_Bug1699931 = false;
}

//! \brief MscgWbor destructor
//!
//! does  nothing
//!
//! \sa Cleanup
//------------------------------------------------------------------------------
MscgWbor::~MscgWbor()
{

}

//! \brief IsSupported
//!
//! Checks if the test can be run in the current environment.
//!
//! \return True if the test can be run in the current environment,
//!         false otherwise
//------------------------------------------------------------------------------
string MscgWbor::IsTestSupported()
{
    RC rc;
    UINT32      wrbkClassId;

    pEvoDisp = GetDisplay()->GetEvoDisplay();

    if (!pEvoDisp)
    {
        return "EVO Display is required to run this test";
    }

     // Display must support a writeback class
    CHECK_RC_CLEANUP(pEvoDisp->GetClassAllocID(&wrbkClassId, Display::WRITEBACK_CHANNEL_ID));
    if (wrbkClassId == 0 ||
        !IsClassSupported(wrbkClassId))
    {
        return "Current platform does not support display writeback class";
    }

Cleanup:
    if (rc == OK)
        return RUN_RMTEST_TRUE;
    else
        return rc.Message();
}

//! \setup Initialise internal state from JS
//!
//! Initial state has to be setup based on the JS setting. This function
// does the same.
//------------------------------------------------------------------------------
RC MscgWbor::Setup()
{
    RC          rc;
    DisplayIDs  Detected;
    UINT32      numDisps;

    CHECK_RC(InitFromJs());

    pDisplay = GetDisplay();
    m_Parent = GetBoundGpuSubdevice();

    CHECK_RC(pDisplay->InitializeDisplayHW(Display::ALLOC_DISP_CHANNELS));

    // get all the connected displays
    CHECK_RC(pDisplay->GetDetected(&Detected));
    Printf(Tee::PriHigh, "All Detected displays = \n");
    pDisplay->PrintDisplayIds(Tee::PriHigh, Detected);

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

    //get the no of displays to work with
    numDisps = static_cast<UINT32>(workingSet.size());

    if (numDisps == 0)
    {
        Printf(Tee::PriHigh, "No displays detected, test aborting...\n");
        return RC::SOFTWARE_ERROR;
    }

    return rc;
}

//! \brief Run the test
//!
//! Call the SetMode() API with some default values
//!
//! \return OK if the SetMode call succeeded, else corresponding rc
//------------------------------------------------------------------------------
RC MscgWbor::Run()
{
    RC                   rc;
    UINT32               width  = 640;
    UINT32               height = 480;
    UINT32               depth  = 32;
    UINT32               refreshRate = 60;
    UINT32               thisHead = Display::ILWALID_HEAD;
    UINT32               numDisps = static_cast<UINT32>(workingSet.size());
    UINT32               descId;
    Surface2D            CoreImage;
    Surface2D            OutputSurface;
    EvoCRCSettings       crcSettings;
    LwRmPtr              pLwRm;
    LW2080_CTRL_MC_QUERY_POWERGATING_PARAMETER_PARAMS QueryParams = {0};
    LW2080_CTRL_POWERGATING_PARAMETER PgParams;
    LwU32 engineState     = 0;
    LwU32                origGatingCount = 0;
    LwU32                newGatingCount = 0;

    PgParams.parameter   = LW2080_CTRL_MC_POWERGATING_PARAMETER_GATINGCOUNT;
    PgParams.parameterExtended = LW2080_CTRL_MC_POWERGATING_ENGINE_ID_MS;
    QueryParams.listSize = 1;
    QueryParams.parameterList = (LwP64)&PgParams;

    //For any given resolution select image of that resolution
    //else select default image
    DTIUtils::ImageUtils imgArr = DTIUtils::ImageUtils::SelectImage(width, height);

    // Fill in the CRC settings we want
    crcSettings.ControlChannel    = EvoCRCSettings::CORE;
    crcSettings.CaptureAllCRCs    = true;
    // Head with WBOR has loadv controlled by Wrbk.Udt+GetFrame so we
    // should not allow DTI to poll for completion notifier by default
    // as it may never return
    crcSettings.PollCRCCompletion = false;

    for(UINT32 disp = 0 ; disp < numDisps ; disp++)
    {
        DisplayID SetModeDisplay = workingSet[disp];

        // We just need to try this displayID on all heads
        // For wrbk CRC
        if ((rc = GetDisplay()->GetHead(SetModeDisplay, &thisHead)) == RC::ILWALID_HEAD)
        {
            Printf(Tee::PriHigh, "MscgWbor: Failed to Get Head.\n ");
            return rc;
        }

        LWT_TIMING timing;
        UINT32 flags = 0;

        memset(&timing, 0 , sizeof(LWT_TIMING));
        CHECK_RC(ModesetUtils::CallwlateLWTTiming(
                    pEvoDisp,
                    SetModeDisplay,
                    width,
                    height,
                    refreshRate,
                    Display::CVT_RB,
                    &timing,
                    flags));

        if(m_Bug1699931)
        {
            // Bug 1699931: If the chip is affected by this bug, then this test will be run with no overscan
            timing.HBorder = 0;
            timing.VBorder = 0;
        }
        else
        {
            // Otherwise this test should setup overscan all around
            timing.HBorder = 2;
            timing.VBorder = 1;
        }

        EvoRasterSettings rs(&timing);

        CHECK_RC(pEvoDisp->SetupEvoLwstomTimings(SetModeDisplay, width, height,
            refreshRate, Display::MANUAL, &rs, thisHead, 0, nullptr));

        CHECK_RC(pDisplay->SetupChannelImage(SetModeDisplay,
                                            width,
                                            height,
                                            depth,
                                            Display::CORE_CHANNEL_ID,
                                            &CoreImage,
                                            imgArr.GetImageName(),
                                            0,/*color*/
                                            thisHead));

        CHECK_RC(pDisplay->SetupChannelImage(SetModeDisplay,
                                            width+80, height+60,   // We setup the wrbk surface to be slightly later than the core image
                                            depth,
                                            Display::WRITEBACK_CHANNEL_ID,
                                            &OutputSurface,
                                            "",
                                            0xFFFF0000,
                                            thisHead,
                                            0, 0,
                                            Surface2D::Pitch,
                                            ColorUtils::A8R8G8B8));

        CHECK_RC(pDisplay->SetImage(SetModeDisplay, &OutputSurface,
            Display::WRITEBACK_CHANNEL_ID));

        CHECK_RC(pDisplay->SetMode(SetModeDisplay,
                                  width,
                                  height,
                                  depth,
                                  refreshRate,
                                  thisHead));

        // Get initial gating count just after modeset
        PgParams.parameter   = LW2080_CTRL_MC_POWERGATING_PARAMETER_GATINGCOUNT;
        PgParams.parameterExtended = LW2080_CTRL_MC_POWERGATING_ENGINE_ID_MS;

        rc = pLwRm->ControlBySubdevice(GetBoundGpuSubdevice(),
                                       LW2080_CTRL_CMD_MC_QUERY_POWERGATING_PARAMETER,
                                       &QueryParams,
                                       sizeof(QueryParams));

        origGatingCount = PgParams.parameterValue;
        Printf(Tee::PriHigh,"Starting gating count (after WBOR attached): %d\n", origGatingCount);

        // Start the CRC
        CHECK_RC(pDisplay->SetupCrcBuffer(SetModeDisplay, &crcSettings));
        CHECK_RC(pDisplay->StartRunningCrc(SetModeDisplay, &crcSettings));

        // Loadv to promote the CRC setting to Active
        // These require the system to be out of MSCG
        CHECK_RC(pEvoDisp->GetWritebackFrame(thisHead, false, &descId));
        CHECK_RC(pEvoDisp->PollWritebackFrame(thisHead, descId, true));

        // Dump the frame
        CHECK_RC(DumpFrame(OutputSurface, thisHead, 0, 0));

        // Enter MSCG again here
        do
        {
            Tasker::Sleep(1000);
            Printf(Tee::PriHigh,"Parsing engine state.\n");
            PgParams.parameter         = LW2080_CTRL_MC_POWERGATING_PARAMETER_PWR_STATE_ENGINE;
            PgParams.parameterExtended = LW2080_CTRL_MC_POWERGATING_ENGINE_ID_MS;
            QueryParams.listSize = 1;
            QueryParams.parameterList = (LwP64)&PgParams;

            rc = pLwRm->ControlBySubdevice(GetBoundGpuSubdevice(),
                                    LW2080_CTRL_CMD_MC_QUERY_POWERGATING_PARAMETER,
                                    &QueryParams,
                                    sizeof(QueryParams));

            engineState = PgParams.parameterValue;

            if(engineState == LW2080_CTRL_MC_POWERGATING_PARAMETER_PWR_STATE_ENGINE_PWR_ON)
                  Printf(Tee::PriHigh, "The MS engine is still not down.");
        } while(engineState != LW2080_CTRL_MC_POWERGATING_PARAMETER_PWR_STATE_ENGINE_PWR_OFF);

        // One more Frame, To Come out of MSCG
        CHECK_RC(pEvoDisp->GetWritebackFrame(thisHead, false, &descId));
        CHECK_RC(pEvoDisp->PollWritebackFrame(thisHead, descId, true));

        // Read MSCG gating count again after the whole enter -> exit completes
        PgParams.parameter   = LW2080_CTRL_MC_POWERGATING_PARAMETER_GATINGCOUNT;
        PgParams.parameterExtended = LW2080_CTRL_MC_POWERGATING_ENGINE_ID_MS;

        rc = pLwRm->ControlBySubdevice(GetBoundGpuSubdevice(),
                                       LW2080_CTRL_CMD_MC_QUERY_POWERGATING_PARAMETER,
                                       &QueryParams,
                                       sizeof(QueryParams));

        newGatingCount = PgParams.parameterValue;

        Printf(Tee::PriHigh,"Start gating count (after WBOR is attached): %d\nEnding gating count (after 2 frames with delay between frame): %d\n", origGatingCount, newGatingCount);
        if (newGatingCount <= origGatingCount)
        {
            Printf(Tee::PriHigh, "Failed to perform at least 1 MSCG gating cycle\n");

            // TODO: This return is destructive, we have not cleaned up the DWB head
            return RC::SOFTWARE_ERROR;
        }
        else
        {
            Printf(Tee::PriHigh, "Able to observe %d > 0 cycles between WBOR GetFrames, MSCG passed\n", (newGatingCount - origGatingCount));
        }

        // One more Frame, To Come out of MSCG
        CHECK_RC(pEvoDisp->GetWritebackFrame(thisHead, false, &descId));
        CHECK_RC(pEvoDisp->PollWritebackFrame(thisHead, descId, true));

        // Dump the frame
        CHECK_RC(DumpFrame(OutputSurface, thisHead, 1, (UINT32)0));

        // Stop running CRC
        CHECK_RC(pDisplay->StopRunningCrc(SetModeDisplay, &crcSettings));

        CHECK_RC(pDisplay->DetachDisplay(DisplayIDs(1, SetModeDisplay)));

        // Poll for CRC complete
        CHECK_RC(pEvoDisp->PollCrcCompletion(&crcSettings));

        CrcComparison crcComp;
        VerifyCrcTree crcVerifSetting;

        // In WBOR:
        //  1). Present interval is alway met
        //  2). First frame may cause primary overflow
        //  3). We do not care about the platform (CRC should be the same)
        //  4). CRC is the same across chips
        // For more details of why these are required, see WborMultiPassRigorousTest
        crcVerifSetting.crcNotifierField.crcField.bCompEvenIfPresentIntervalMet = true;
        crcVerifSetting.crcNotifierField.bIgnorePrimaryOverflow = true;
        crcVerifSetting.crcHeaderField.bSkipPlatform = true;
        crcVerifSetting.crcHeaderField.bSkipChip = true;

        CHECK_RC(crcComp.DumpCrcToXml(GetBoundGpuDevice(), "dwb_mscg.xml", &crcSettings, true, true));
        CHECK_RC(crcComp.CompareCrcFiles("dwb_mscg.xml", "./DTI/Golden/WborMultiPassRigorous/mscgWbor_640x480.xml",
            nullptr, &crcVerifSetting));
    }

    return rc;
}

//! \brief Cleanup, does nothing for this test
//------------------------------------------------------------------------------
RC MscgWbor::Cleanup()
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
JS_CLASS_INHERIT(MscgWbor, RmTest,
    "Test to make WBOR to enter into MSCG");

CLASS_PROP_READWRITE(MscgWbor, Bug1699931, bool, "Indicates if Bug 1699931 affects this run");

