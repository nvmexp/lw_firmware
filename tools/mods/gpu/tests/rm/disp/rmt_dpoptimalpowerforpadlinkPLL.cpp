/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2013-2013,2019 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

//!
//!
//! \file rmt_OptimalPowerForPadlinkPLL.cpp
//! \brief an optimal power comsumption feature for padlink PLL
//! with different cases like normal DP and dual SST
//!
#include "gpu/tests/rmtest.h"
#include "core/include/script.h"
#include "core/include/jscript.h"
#include "core/include/platform.h"
#include "lwRmApi.h"
#include "lwmisc.h"
#include "gpu/include/gpudev.h"
#include "gpu/include/gpusbdev.h"
#include "core/include/display.h"
#include "core/include/platform.h"
#include "gpu/display/evo_disp.h"
#include "gpu/display/evo_dp.h"
#include "gpu/display/evo_chns.h"
#include "disp/v02_05/dev_disp.h"
#include "maxwell/gm200/dev_trim.h"
#include "core/include/memcheck.h"
#include "gpu/tests/rm/utility/dtiutils.h"
#include "class/cl0073.h"  // LW04_DISPLAY_COMMON
#include "ctrl/ctrl0000.h"
#include "ctrl/ctrl0073.h"
#include "ctrl/ctrl0073/ctrl0073dfp.h"
#include <algorithm>
#include <vector>

using namespace std;

#define IsGM20XorBetter(p)   ((p >= Gpu::GM200))

#define MAX_PADLINKS 7

typedef enum
{
    padlink_A = 0,
    padlink_B = 1,
    padlink_C = 2,
    padlink_D = 3,
    padlink_E = 4,
    padlink_F = 5,
    padlink_G = 6,
    padlink_ilwalid
} DFPPADLINK;

class OptimalPowerForPadlinkPLL : public RmTest
{
public:
    OptimalPowerForPadlinkPLL();
    virtual ~OptimalPowerForPadlinkPLL();

    virtual string IsTestSupported();

    virtual RC Setup();
    virtual RC Run();
    virtual RC Cleanup();

    RC testConnectedDPDisplays();

    void detectConnectedDPPanels();
    bool getPowerStateOfPadlinkPLL(UINT32 padlink);
    void printPadlink(UINT32 padlink);
    void printPowerStateOfPadlinks();

    SETGET_PROP(normalDP, bool);
    SETGET_PROP(dualSST, bool);

private:
    GpuSubdevice *m_pSubdev;
    DPDisplay *m_pDisplay;
    DisplayIDs m_detected;
    DisplayIDs workingSet;
    bool m_normalDP;
    bool m_dualSST;
};

//! \brief OptimalPowerForPadlinkPLL constructor
//!
//! Sets the name of the test and default values for cmd line parameters
//!
//------------------------------------------------------------------------------
OptimalPowerForPadlinkPLL::OptimalPowerForPadlinkPLL()
{
    SetName("OptimalPowerForPadlinkPLL");
    m_pSubdev = nullptr;
    m_pDisplay = nullptr;
    m_normalDP = false;
    m_dualSST = false;
}

//! \brief OptimalPowerForPadlinkPLL destructor
//!
//! does  nothing
//!
//! \as Cleanup
//------------------------------------------------------------------------------
OptimalPowerForPadlinkPLL::~OptimalPowerForPadlinkPLL()
{
}

//! \brief IsSupported
//!
//! Checks if the test can be run in the current environment.
//!
//! \return true if the test can be run in the current environment,
//!         false otherwise
//------------------------------------------------------------------------------
string OptimalPowerForPadlinkPLL::IsTestSupported()
{
    Gpu::LwDeviceId chipName = GetBoundGpuSubdevice()->DeviceId();

    if (IsClassSupported(LW04_DISPLAY_COMMON) && IsGM20XorBetter(chipName))
    {
        return RUN_RMTEST_TRUE;
    }

    return "Optimal power for Padlink CML buffers feature is not supported on pre-GM20X chips";
}

//! \setup Initialises the internal state from JS
//!
//! Initial state setup based on the JS setting
//------------------------------------------------------------------------------
RC OptimalPowerForPadlinkPLL::Setup()
{
    RC rc = OK;

    // Ask our base class(es) to setup internal state based on JS settings
    CHECK_RC(InitFromJs());

    // Get underlying GPU subdevice
    m_pSubdev = GetBoundGpuSubdevice();

    CHECK_RC(ErrorLogger::StartingTest());
    ErrorLogger::IgnoreErrorsForThisTest();

    m_pDisplay = new DPDisplay(GetDisplay());
    CHECK_RC(m_pDisplay->getDisplay()->InitializeDisplayHW(Display::ALLOC_DISP_CHANNELS));

    // Enable use of DP library
    m_pDisplay->getDisplay()->SetUseDPLibrary(true);

    return rc;
}

//! \brief Run the test
//!
//! Test the optimal power comsumption for connected DP panels
//------------------------------------------------------------------------------
RC OptimalPowerForPadlinkPLL::Run()
{
    RC rc = OK;
    UINT32 count = 0;

    if (m_normalDP)
        count++;
    if (m_dualSST)
        count++;

    if (count != 1)
    {
        Printf(Tee::PriHigh, "Error: Incorrect command line parameter, Test cannot Run.");
        return RC::TEST_CANNOT_RUN;
    }

    rc = testConnectedDPDisplays();

    return rc;
}

//! \brief Optimal power consumption by padlink PLLs test
//!
//! This function scans all connected displays and checks which ones of them are DP.
//! It checks padlink PLL's power state for all connected DP displays at various
//! point of times like just before modeset, just after modeset and just after
//! detaching a display. This function handles all cases like normal DP
//! and dual SST as per cmd line request
//------------------------------------------------------------------------------
RC OptimalPowerForPadlinkPLL::testConnectedDPDisplays()
{
    RC rc = OK;
    UINT32 i = 0;
    LW0073_CTRL_DFP_GET_PADLINK_MASK_PARAMS padlinkMaskParams = {0};
    UINT32 width    = 640;
    UINT32 height   = 480;
    UINT32 RefreshRate  = 60;
    UINT32 LwrrPadLinkMask = 0;
    UINT32 padlink = 0;
    DTIUtils::ImageUtils coreImage;
    DPConnector *pConnector = NULL;
    DisplayPort::Group *pDpGroup = NULL;
    DisplayIDs monitorIds;
    Surface2D Images;

    // Get all connected device
    CHECK_RC(m_pDisplay->getDisplay()->GetDetected(&m_detected));

    // Detect DP panels
    detectConnectedDPPanels();

    if (workingSet.empty())
    {
        Printf(Tee::PriHigh, "Error: Failed to find connected DP panel, Test cannot run");
        return RC::TEST_CANNOT_RUN;
    }

    //
    // Set correct mode in evoDisp as this is required for displayID allocation.
    // in MST mode we do dynamic display ID while in SST mode we use connectorID
    // Here "0" is SST Mode & "1"  is MST Mode.
    //
    m_pDisplay->setDpMode(SST);

    // Print power state of all padlinks
    printPowerStateOfPadlinks();

    coreImage = DTIUtils::ImageUtils::SelectImage(width, height);

    if (m_normalDP)
    {
        // Run the test for all connected DP displays
        for (i = 0; i < workingSet.size(); i++)
        {
            // Get the padlink used by current panel
            padlinkMaskParams.subDeviceInstance = 0;
            padlinkMaskParams.displayId = workingSet[i];

            rc = m_pDisplay->getDisplay()->RmControl(
                    LW0073_CTRL_CMD_DFP_GET_PADLINK_MASK,
                    &padlinkMaskParams, sizeof (padlinkMaskParams));

            LwrrPadLinkMask = padlinkMaskParams.padlinkMask;

            // Print padlink used by current panel
            Printf(Tee::PriHigh, "OptimalPowerForPadlinkPLL : displayId = 0x%08x uses : ", (UINT32)workingSet[i]);

            for (padlink = padlink_A; padlink < MAX_PADLINKS; padlink++)
            {
                if (LwrrPadLinkMask & (UINT32)(1 << padlink))
                {
                    printPadlink(padlink);
                }
            }

            // Get all detected monitors in connector
            CHECK_RC(m_pDisplay->EnumAllocAndGetDPDisplays(workingSet[i], &pConnector, &monitorIds, true));

            // Detach VBIOS screen
            pConnector->notifyDetachBegin(NULL);
            m_pDisplay->getDisplay()->GetEvoDisplay()->ForceDetachHeads();
            pConnector->notifyDetachEnd();

            // Print power state of all padlinks just before modeset
            printPowerStateOfPadlinks();

            //
            // Now check for power states of all padlink PLLs
            // Note that all padlink PLLs should be powered down at this point
            //
            for (padlink = padlink_A; padlink < MAX_PADLINKS; padlink++)
            {
                if (LwrrPadLinkMask & (UINT32)(1 << padlink))
                {
                    if (getPowerStateOfPadlinkPLL(padlink))
                    {
                        Printf(Tee::PriHigh, "ERROR: Padlink is powered up even after detaching VBIOS screen\n");
                        return RC::SOFTWARE_ERROR;
                    }
                }
            }

            pDpGroup = pConnector->CreateGroup(monitorIds);

            if (pDpGroup == NULL)
            {
                Printf(Tee::PriHigh, "ERROR: Failed to create a group\n");
                return RC::SOFTWARE_ERROR;
            }

            Printf(Tee::PriHigh, "OptimalPowerForPadlinkPLL : Starting with a modeset on a panel.\n");

            CHECK_RC(m_pDisplay->getDisplay()->SetupChannelImage(monitorIds[0],
                     width,
                     height,
                     32,
                     Display::CORE_CHANNEL_ID,
                     &Images,
                     coreImage.GetImageName(),
                     0,
                     0)); // Hard code the head to 0

            CHECK_RC(m_pDisplay->SetMode(workingSet[i], monitorIds[0],
                     pDpGroup,
                     width,
                     height,
                     32,
                     RefreshRate));

            // Print power state of all padlinks just after modeset
            printPowerStateOfPadlinks();

            //
            // Now check for power states of padlink PLL(s) used by current display
            // Note that PLL for the padlink used by current displayID should be up.
            //
            for (padlink = padlink_A; padlink < MAX_PADLINKS; padlink++)
            {
                if (LwrrPadLinkMask & (UINT32)(1 << padlink))
                {
                    if (!getPowerStateOfPadlinkPLL(padlink))
                    {
                        Printf(Tee::PriHigh, "ERROR: Padlink is powered down, though it's being used by DP display\n");
                        return RC::SOFTWARE_ERROR;
                    }
                }
            }

            // Detach the display
            CHECK_RC(m_pDisplay->DetachDisplay(workingSet[i], monitorIds[0], pDpGroup));

            if (Images.GetMemHandle() != 0)
                Images.Free();

            // Print power state of all padlinks just after detaching the display
            printPowerStateOfPadlinks();

            //
            // Now check for power states of padlink PLL(s) used by this display
            // Note that PLL for the padlink used by current displayID should be down.
            //
            for (padlink = padlink_A; padlink < MAX_PADLINKS; padlink++)
            {
                if (LwrrPadLinkMask & (UINT32)(1 << padlink))
                {
                    if (getPowerStateOfPadlinkPLL(padlink))
                    {
                        Printf(Tee::PriHigh, "ERROR: Padlink is powered up, though it's not being used by DP display\n");
                        return RC::SOFTWARE_ERROR;
                    }
                }
            }
        }
    }

    if (m_dualSST)
    {
        DisplayIDs  dp2sstConnIds;

        if (workingSet.size() > 1)
        {
            dp2sstConnIds.push_back(workingSet[0]);
            dp2sstConnIds.push_back(workingSet[1]);
        }
        else if((workingSet.size() == 1) || workingSet.empty())
        {
            return RC::TEST_CANNOT_RUN;
        }

        vector<Surface2D>           Images(dp2sstConnIds.size());
        vector<DPConnector*>        pConnector(dp2sstConnIds.size());
        vector<DisplayPort::Group*> pDpGroups(dp2sstConnIds.size());

        vector<DisplayIDs> monitorIds;

        // Pick two from the available DP connectros.
        for (UINT32 i = 0; i < (UINT32)dp2sstConnIds.size(); i++)
        {
            pDpGroups[i] = NULL;
            DisplayIDs monIdsPerCnctr;

            // Get all detected monitors in respective connectors
            CHECK_RC(m_pDisplay->EnumAllocAndGetDPDisplays(dp2sstConnIds[i], &(pConnector[i]), &monIdsPerCnctr, true));

            pDpGroups[i] = pConnector[i]->CreateGroup(monIdsPerCnctr);

            if (pDpGroups[i] == NULL)
            {
                Printf(Tee::PriHigh, "\n ERROR: Failed to create a group\n");
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

        LwrrPadLinkMask = 0;

        // Get the padlinks used by dual SST panel
        for (UINT32 i = 0; i < monitorIds.size(); i++)
        {
            padlinkMaskParams.subDeviceInstance = 0;
            padlinkMaskParams.displayId = dpSSTMonIds[i];

            rc = m_pDisplay->getDisplay()->RmControl(
                    LW0073_CTRL_CMD_DFP_GET_PADLINK_MASK,
                    &padlinkMaskParams, sizeof (padlinkMaskParams));

            LwrrPadLinkMask |= padlinkMaskParams.padlinkMask;
        }

        // Print padlinks used by current panel
        Printf(Tee::PriHigh, "OptimalPowerForPadlinkPLL : dual SST panel with displayId = 0x%08x and displayId = 0x%08x uses : ", (UINT32)dpSSTMonIds[0], (UINT32)dpSSTMonIds[1]);

        for (padlink = padlink_A; padlink < MAX_PADLINKS; padlink++)
        {
            if (LwrrPadLinkMask & (UINT32)(1 << padlink))
            {
                printPadlink(padlink);
            }
        }

        // Detach VBIOS screen
        pConnector[0]->notifyDetachBegin(NULL);
        m_pDisplay->getDisplay()->GetEvoDisplay()->ForceDetachHeads();
        pConnector[0]->notifyDetachEnd();

        // Print power state of all padlinks just before modeset
        printPowerStateOfPadlinks();

        //
        // Now check for power states of all padlink PLLs.
        // Note that all padlink PLLs should be powered down at this point.
        //
        for (padlink = padlink_A; padlink < MAX_PADLINKS; padlink++)
        {
            if (LwrrPadLinkMask & (UINT32)(1 << padlink))
            {
                if (getPowerStateOfPadlinkPLL(padlink))
                {
                    Printf(Tee::PriHigh, "ERROR: Padlink is powered up even after detaching VBIOS screen\n");
                    return RC::SOFTWARE_ERROR;
                }
            }
        }

        Printf(Tee::PriHigh, "OptimalPowerForPadlinkPLL : Starting with a modeset on dual SST panel \n");

        CHECK_RC(m_pDisplay->getDisplay()->SetupChannelImage(dpSSTMonIds[0],
                 width,
                 height,
                 32,
                 Display::CORE_CHANNEL_ID,
                 &Images[0],
                 coreImage.GetImageName(),
                 0,
                 0 )); // Hard code the head to 0

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
                 RefreshRate,
                 0));

        // Print power state of all padlinks just after modeset
        printPowerStateOfPadlinks();

        //
        // Now check for power states of padlink PLLs used by this dual SST display.
        // Note that PLLs for the padlinks used by current panel should be up.
        //
        for (padlink = padlink_A; padlink < MAX_PADLINKS; padlink++)
        {
            if (LwrrPadLinkMask & (UINT32)(1 << padlink))
            {
                if (!getPowerStateOfPadlinkPLL(padlink))
                {
                    Printf(Tee::PriHigh, "ERROR: Padlink is powered down, though it's being used by DP display\n");
                    return RC::SOFTWARE_ERROR;
                }
            }
        }

        //
        // For dual SST case, we need to detach both the links, so notify detach
        // end has to be sent twice in order to detach both the links.
        //
        CHECK_RC(m_pDisplay->DetachSingleHeadMultiStreamSST(dp2sstConnIds,
                 dpSSTMonIds,
                 pDpGroups));

        // Print power state of all padlinks just after detaching the display
        printPowerStateOfPadlinks();

        //
        // Now check for power states of padlink PLLs used by this display
        // Note that PLLs for the padlinks used by current panel should be down.
        //
        for (padlink = padlink_A; padlink < MAX_PADLINKS; padlink++)
        {
            if (LwrrPadLinkMask & (UINT32)(1 << padlink))
            {
                if (getPowerStateOfPadlinkPLL(padlink))
                {
                    Printf(Tee::PriHigh, "ERROR: Padlink is powered up, though it's not being used by DP display\n");
                    return RC::SOFTWARE_ERROR;
                }
            }
        }
    }
    return rc;
}

//! \brief Extracts all detected DP panels in a working set
//------------------------------------------------------------------------------
void OptimalPowerForPadlinkPLL::detectConnectedDPPanels()
{
    // Get all DP connectors in working set
    for (UINT32 i = 0; i < (UINT32)m_detected.size(); i++)
    {
        if (DTIUtils::VerifUtils::IsSupportedProtocol(m_pDisplay->getDisplay(), m_detected[i], "DP,DP_A,DP_B"))
        {
            workingSet.push_back(m_detected[i]);
        }
    }
}

//! \brief Prints actual padlink name from padlink integer value
//------------------------------------------------------------------------------
void OptimalPowerForPadlinkPLL::printPadlink(UINT32 padlink)
{
    switch (padlink)
    {
        case padlink_A:
        Printf(Tee::PriHigh, "padlinkA ");
        break;
        case padlink_B:
        Printf(Tee::PriHigh, "padlinkB ");
        break;
        case padlink_C:
        Printf(Tee::PriHigh, "padlinkC ");
        break;
        case padlink_D:
        Printf(Tee::PriHigh, "padlinkD ");
        break;
        case padlink_E:
        Printf(Tee::PriHigh, "padlinkE ");
        break;
        case padlink_F:
        Printf(Tee::PriHigh, "padlinkF ");
        break;
        case padlink_G:
        Printf(Tee::PriHigh, "padlinkG ");
        break;
        default:
        Printf(Tee::PriHigh, "Invalid padlink ");
        break;
    }
    return;
}

//! \brief Checks power state of padlink PLL
//------------------------------------------------------------------------------
bool OptimalPowerForPadlinkPLL::getPowerStateOfPadlinkPLL(UINT32 padlink)
{
    UINT32 regValue = 0;

    if (padlink < padlink_E)
    {
        regValue = m_pSubdev->RegRd32(LW_PVTRIM_SYS_SPPLL0_CTRL);
        return !FLD_IDX_TEST_DRF(_PVTRIM, _SYS_SPPLL0, _CTRL_DIFFMUX_PD, padlink, _ENABLE, regValue);
    }
    else
    {
        regValue = m_pSubdev->RegRd32(LW_PVTRIM_SYS_SPPLL1_CTRL);
        return !FLD_IDX_TEST_DRF(_PVTRIM, _SYS_SPPLL1, _CTRL_DIFFMUX_PD, (padlink - 4), _ENABLE, regValue);
    }
}

//! \brief Prints power state of padlink PLL
//------------------------------------------------------------------------------
void OptimalPowerForPadlinkPLL::printPowerStateOfPadlinks()
{
    UINT32 padlink = 0;

    for (padlink = padlink_A; padlink < MAX_PADLINKS; padlink++)
    {
        if (getPowerStateOfPadlinkPLL(padlink))
        {
            printPadlink(padlink);
            Printf(Tee::PriHigh, ": ON\n");
        }
        else
        {
            printPadlink(padlink);
            Printf(Tee::PriHigh, ": OFF\n");
        }
    }
}

//!
//! @brief Cleanup(): does nothing for this simple test.
//!
//! @param[in]  NONE
//! @param[out] NONE
//!
//! @returns    OK
//-----------------------------------------------------------------------------
RC OptimalPowerForPadlinkPLL::Cleanup()
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
JS_CLASS_INHERIT(OptimalPowerForPadlinkPLL, RmTest, "Optimal power comsumption for padlink PLL test\n");
CLASS_PROP_READWRITE(OptimalPowerForPadlinkPLL, normalDP, bool, "Normal DP panels, default = 0");
CLASS_PROP_READWRITE(OptimalPowerForPadlinkPLL, dualSST, bool, "Dual SST panels, default = 0");
