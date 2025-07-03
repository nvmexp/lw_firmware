/*
* LWIDIA_COPYRIGHT_BEGIN
*
* Copyright 2006-2011,2013-2014,2016-2017,2019 by LWPU Corporation.  All rights reserved.  All
* information contained herein is proprietary and confidential to LWPU
* Corporation.  Any use, reproduction, or disclosure without the written
* permission of LWPU Corporation is prohibited.
*
* LWIDIA_COPYRIGHT_END
*/

//!
//!
//! \file rmt_vpllspreadtest.cpp
//! \brief Test to verify the VPLL spread spectrum
//!

#include "gpu/tests/rmtest.h"
#include "core/include/script.h"
#include "core/include/jscript.h"
#include "core/include/display.h"
#include "core/include/platform.h"
#include "core/include/utility.h"
#include "gpu/utility/rmclkutil.h"
#include "gpu/include/gpusbdev.h"
#include "core/include/utility.h"
#include "lwRmApi.h"
#include "core/include/modsedid.h"
#include "core/include/xp.h"
#include "gpu/tests/rm/utility/dtiutils.h"
#include "gpu/display/evo_disp.h"
#include "core/include/memcheck.h"

typedef struct
{
    LwU32 *widths;
    LwU32 *heights;
    LwU32 *refreshRates;
    LwU32 totalResCount;
}edidResolutions;

class VpllSpreadTest: public RmTest
{
public:
    VpllSpreadTest();
    virtual ~VpllSpreadTest();
    virtual string IsTestSupported();
    virtual RC Setup();
    virtual RC Run();
    virtual RC Cleanup();
    RC DoModeSetAndAnalyzePclk(DisplayID, FLOAT32&, LwU32 &, LwU32, LwU32, LwU32);
    RC GetDisplayResolutions(DisplayID, edidResolutions &);

private:
    Display           *m_pDisplay;
    DisplayID         selectedLVDS;
    DisplayID         nonLVDS;
    string            m_protocol;
    string            m_fakeNonLvdsProtocol;
    edidResolutions   lvdsRes;
    edidResolutions   nonlvdsRes;
    GpuSubdevice      *m_pSubdevice;
    LwRm::Handle      m_hBasicSubDev;
    LwU32             m_depth;
};

/*
*Constructor.. Sets the name of the test
*/
VpllSpreadTest::VpllSpreadTest()
{
    SetName("VpllSpreadTest");
    m_pDisplay            = nullptr;
    m_protocol            = "LVDS_LWSTOM";
    m_fakeNonLvdsProtocol = "CRT";
    m_pSubdevice          = nullptr;
    m_hBasicSubDev        = 0;
    m_depth               = 32;
    memset((void *)&lvdsRes, 0, sizeof(edidResolutions));
    memset((void *)&nonlvdsRes, 0, sizeof(edidResolutions));
}

VpllSpreadTest::~VpllSpreadTest()
{

}

/*
 * Test must be run with two panels. LVDS & (CRT | DVI | DP)
 * Checks if vbios has enabled the spread using Rmcontrol call
 */
string VpllSpreadTest::IsTestSupported()
{
    RC rc;
    LwRmPtr pLwRm;
    LwU32 i = 0;
    DisplayIDs supportedDisplays;

    //LwRmPtr pLwRm;
    DisplayIDs Detected;
    m_pSubdevice = GetBoundGpuSubdevice();
    m_hBasicSubDev = pLwRm->GetSubdeviceHandle(m_pSubdevice);

    CHECK_RC_CLEANUP(rc);

    m_pDisplay = GetDisplay();
    CHECK_RC_CLEANUP(m_pDisplay->InitializeDisplayHW(Display::ALLOC_DISP_CHANNELS));

    // get all the connected displays
    CHECK_RC_CLEANUP(m_pDisplay->GetDetected(&Detected));
    Printf(Tee::PriHigh, "All Detected displays = \n");
    m_pDisplay->PrintDisplayIds(Tee::PriHigh, Detected);

    //Only the protocol passed by the user must be used
    for (i=0; i < Detected.size(); i++)
    {
        if(DTIUtils::VerifUtils::IsSupportedProtocol(m_pDisplay, Detected[i], m_protocol))
        {
            LW0073_CTRL_DFP_GET_SPREAD_SPECTRUM_PARAMS spreadParams;
            spreadParams.displayId = Detected[i];
            CHECK_RC_CLEANUP(m_pDisplay->RmControl(LW0073_CTRL_CMD_DFP_GET_SPREAD_SPECTRUM,
                                                   &spreadParams, sizeof (spreadParams)));
            if(spreadParams.enabled == LW_TRUE)
            {
                selectedLVDS = Detected[i];
            }
        }
        else
        {
            nonLVDS = Detected[i];
        }
    }

    // Lets find if any fake device is needed.
    if (!selectedLVDS || !nonLVDS)
    {
        CHECK_RC_CLEANUP(m_pDisplay->GetConnectors(&supportedDisplays));
        Printf(Tee::PriHigh, "All supported displays = \n");
        m_pDisplay->PrintDisplayIds(Tee::PriHigh, supportedDisplays);

        i = static_cast<LwU32>(supportedDisplays.size());
        while ((!selectedLVDS || !nonLVDS) && i--)
        {
            Display::DisplayType thisDisplayType;
            CHECK_RC_CLEANUP(m_pDisplay->GetDisplayType(supportedDisplays[i], &thisDisplayType));
            if (!m_pDisplay->IsDispAvailInList(supportedDisplays[i], Detected))
            {
                if((DTIUtils::VerifUtils::IsSupportedProtocol(m_pDisplay, supportedDisplays[i], m_protocol)) &&
                   !selectedLVDS)
                {
                    LW0073_CTRL_DFP_GET_SPREAD_SPECTRUM_PARAMS spreadParams;
                    spreadParams.displayId = supportedDisplays[i];
                    CHECK_RC_CLEANUP(m_pDisplay->RmControl(LW0073_CTRL_CMD_DFP_GET_SPREAD_SPECTRUM,
                                                           &spreadParams, sizeof (spreadParams)));
                    if(spreadParams.enabled == LW_TRUE)
                    {
                        selectedLVDS = supportedDisplays[i];
                        CHECK_RC_CLEANUP(DTIUtils::EDIDUtils::SetLwstomEdid(m_pDisplay, selectedLVDS, false));
                        Printf(Tee::PriHigh, "Faking LVDS display\n");
                    }
                }
                else if (!nonLVDS)
                {
                    if(DTIUtils::VerifUtils::IsSupportedProtocol(m_pDisplay, supportedDisplays[i], m_fakeNonLvdsProtocol))
                    {
                        nonLVDS = supportedDisplays[i];
                        CHECK_RC_CLEANUP(DTIUtils::EDIDUtils::SetLwstomEdid(m_pDisplay, nonLVDS, false));
                        Printf(Tee::PriHigh, "Faking non-LVDS CRT display\n");
                    }
                }

            }
        }
    }
    if (!selectedLVDS || !nonLVDS)
    {
        Printf(Tee::PriHigh, "Required displays not found\n");
        return "Required displays not found";
    }
    return RUN_RMTEST_TRUE;
Cleanup:
    if (rc == OK)
        return RUN_RMTEST_TRUE;
    return rc.Message();
}

// Callwlates mean deviation for a set of integers..
FLOAT32 FindMeanDeviation(vector<LwU32> &allInts)
{
    LwU32 ind  = 0;
    LwU32 sum  = 0;
    LwU32 avg  = 0;
    LwU32 msum = 0;
    while(ind < allInts.size())
    {
        sum += allInts[ind];
        ++ind;
    }
    avg = sum / static_cast<LwU32>(allInts.size());
    ind = 0;
    while(ind < allInts.size())
    {
        msum += abs((LwS32)(allInts[ind] - avg));
        ++ind;
    }
    return ((FLOAT32)msum)/allInts.size();
}

/*
 * Does a modeset for a display and measures PCLK for 'x' number of times.
 * Finally callwlates the mean deviation of all those clock measurements taken.
 */

RC VpllSpreadTest::DoModeSetAndAnalyzePclk(DisplayID display, FLOAT32 &Mean, LwU32 &maxClk,
                                                   LwU32 width, LwU32 height, LwU32 rRate)
{
    RC rc;
    LwRmPtr pLwRm;
    LwU32 Head;
    LwU32 loop = 10;
    LW2080_CTRL_CLK_MEASURE_FREQ_PARAMS m_clkCounterFreqParams;
    vector<LwU32> clocks;

    if ((rc = m_pDisplay->GetHead(display, (UINT32*)&Head)) == RC::ILWALID_HEAD)
    {
        Printf(Tee::PriHigh, "VpllSpreadTest: Failed to Get Head.\n ");
        return rc;
    }

    rc = m_pDisplay->SetMode(display, width, height, m_depth, rRate);
    if (OK != rc)
    {
        if ( rc == RC::DISPLAY_NO_FREE_HEAD)
        {
            Printf(Tee::PriHigh, "SetMode call failed DISPLAY_NO_FREE_HEAD, considering OK\n");
            rc = OK;
        }
        else
        {
            Printf(Tee::PriHigh, "SetMode call failed\n");
        }
        return rc;
    }
    Printf(Tee::PriHigh,"Head found for display %u \n", Head);
    m_clkCounterFreqParams.clkDomain = LW2080_CTRL_CLK_DOMAIN_PCLK(Head);
    while(loop--)
    {

        CHECK_RC(pLwRm->Control(m_hBasicSubDev,
                            LW2080_CTRL_CMD_CLK_MEASURE_FREQ,
                            &m_clkCounterFreqParams,
                            sizeof (m_clkCounterFreqParams)));

        clocks.push_back(m_clkCounterFreqParams.freqKHz);
        if(m_clkCounterFreqParams.freqKHz > maxClk)
            maxClk = m_clkCounterFreqParams.freqKHz;

        Printf(Tee::PriHigh, "PCLK FREQ: %x\n",m_clkCounterFreqParams.freqKHz);
    }
    Mean = FindMeanDeviation(clocks);
    Printf(Tee::PriHigh, "Mean Deviation: %f\n",Mean);
    Printf(Tee::PriHigh,"\n\n");

    return OK;
}

/*
 * Loop this 'y' number of times:
 *    Detach lvds panel, do modeset on non-lvds panel and measure clocks for 'x' number of times.
 *    Callwlate Mean deviation.
 *    Detach non-lvds panel, do modeset on lvds panel and measure clocks for 'x' number times.
 *    Callwlate Mean deviation.
 * Now we have 'y' mean deviation values for LVDS PClk and non-LVDS PClk.
 * Callwlate the mean deviation average for each set of 'y' values.
 * If mean deviation average for non-LVDS is 0 and for LVDS is non-zero, the test is PASS.
 * If mean deviation average for non-LVDS is non-zero and if LVDS mean deviation average is atleast 3(factor) times
 *    greater than non-LVDS mean average, than the test is marked as PASS.
 * Factor is embirical, got it from Dhawal.
 */

RC VpllSpreadTest::Run()
{
    RC rc                   = OK;
    LwU32  ind              = 0;
    const LwU32  loop       = 10;
    const LwU32  factor     = 3;  //Empirical number to verify spread.. From Dhawal.

    FLOAT32 Mean            = 0;
    FLOAT32 lvdsMeanA       = 0;  //Mean deviation average for lvds panel
    FLOAT32 nlvdsMeanA      = 0;  //Mean deviation average for nonlvds panel
    LwU32   lvdsMaxPclk     = 0;  //Maximum pclk found in all the loops
    LwU32   nonlvdsMaxPclk  = 0;

    vector<FLOAT32> lvdsMean;     //Holds lvds mean deviation values for 'loop' times
    vector<FLOAT32> nonlvdsMean;  //Holds nonlvds mean deviation values

    LwU32 lvdsResCount      = lvdsRes.totalResCount;
    LwU32 nonlvdsResCount   = nonlvdsRes.totalResCount;

    //Set modes on both displays

    CHECK_RC(m_pDisplay->SetMode(selectedLVDS, lvdsRes.widths[0], lvdsRes.heights[0],
                                                      m_depth, lvdsRes.refreshRates[0]));
    CHECK_RC(m_pDisplay->SetMode(nonLVDS, nonlvdsRes.widths[0], nonlvdsRes.heights[0],
                                                      m_depth, nonlvdsRes.refreshRates[0]));

    while(ind < loop)
    {
        //Detach LVDS display and modeset on non-lvds. Measure the clock freq..
        Printf(Tee::PriHigh, "Detaching LVDS panel and measuring clock:\n");
        CHECK_RC(m_pDisplay->DetachDisplay(DisplayIDs(1, selectedLVDS)));
        CHECK_RC(DoModeSetAndAnalyzePclk(nonLVDS, Mean, nonlvdsMaxPclk, nonlvdsRes.widths[nonlvdsResCount-1],
                                                        nonlvdsRes.heights[nonlvdsResCount-1],
                                                        nonlvdsRes.refreshRates[nonlvdsResCount-1]));
        nonlvdsMean.push_back(Mean);
        nlvdsMeanA += Mean;

        //Detach non-LVDS and modeset on lvds. Measure the clock freq
        Printf(Tee::PriHigh, "Detaching non-LVDS panel and measuring clock:\n");
        CHECK_RC(m_pDisplay->DetachDisplay(DisplayIDs(1, nonLVDS)));
        CHECK_RC(DoModeSetAndAnalyzePclk(selectedLVDS, Mean, lvdsMaxPclk, lvdsRes.widths[lvdsResCount-1],
                                                        lvdsRes.heights[lvdsResCount-1],
                                                        lvdsRes.refreshRates[lvdsResCount-1]));
        lvdsMean.push_back(Mean);
        lvdsMeanA += Mean;

        //Set mode again
        CHECK_RC(m_pDisplay->SetMode(selectedLVDS, lvdsRes.widths[0], lvdsRes.heights[0],
                                                        m_depth, lvdsRes.refreshRates[0]));
        CHECK_RC(m_pDisplay->SetMode(nonLVDS, nonlvdsRes.widths[0], nonlvdsRes.heights[0],
                                                        m_depth, nonlvdsRes.refreshRates[0]));
        ++ind;
    }

    lvdsMeanA = lvdsMeanA / loop;
    nlvdsMeanA = nlvdsMeanA / loop;
    Printf(Tee::PriHigh,"*********************************************************************\n");
    Printf(Tee::PriHigh, "   Number of loops for measuring clocks     : %u\n", loop);
    Printf(Tee::PriHigh, "   LVDS panel max pclk noticed(KHz)         : %u\n", lvdsMaxPclk);
    Printf(Tee::PriHigh, "   Non-LVDS panel max pclk noticed(KHz)     : %u\n", nonlvdsMaxPclk);
    Printf(Tee::PriHigh, "   LVDS Mean Deviation Average              : %f\n", lvdsMeanA);
    Printf(Tee::PriHigh, "   LVDS approx. spread percent              : %f\n", (lvdsMeanA/lvdsMaxPclk)*100);
    Printf(Tee::PriHigh, "   Non-LVDS Mean Deviation Average          : %f\n", nlvdsMeanA);
    Printf(Tee::PriHigh,"*********************************************************************\n");

    if(lvdsMeanA < (nlvdsMeanA*factor))
    {
        Printf(Tee::PriHigh, "Error: LVDS mean deviation should be atleast %u more than non-lvds Mean deviation\n",
               (unsigned int) factor);
        return RC::SOFTWARE_ERROR;
    }
    return rc;
}

//Gets all resolutions for a Display
RC VpllSpreadTest::GetDisplayResolutions(DisplayID display, edidResolutions &res)
{
    RC rc;
    LwU32 totalResCount = 0;
    Edid *pEdid = m_pDisplay->GetEdidMember();
    if(!display)
    {
        Printf(Tee::PriHigh, "Selected display not found\n");
        return RC::SOFTWARE_ERROR;
    }
    CHECK_RC(pEdid->GetNumListedResolutions(display, (UINT32*)&totalResCount));
    if(!totalResCount)
    {
        Printf(Tee::PriHigh, "No resolutions found\n");
        return RC::SOFTWARE_ERROR;
    }
    res.widths        = new LwU32[totalResCount];
    res.heights       = new LwU32[totalResCount];
    res.refreshRates  = new LwU32[totalResCount];
    res.totalResCount = totalResCount;

    CHECK_RC(pEdid->GetListedResolutions(display, (UINT32*)res.widths, (UINT32*)res.heights, (UINT32*)res.refreshRates));
    return OK;
}

/*
 * Get all the resolutions for both the LVDS panel and non-lvds panel.
 */
RC VpllSpreadTest::Setup()
{
    RC rc = OK;
    CHECK_RC(GetDisplayResolutions(selectedLVDS, lvdsRes));
    CHECK_RC(GetDisplayResolutions(nonLVDS, nonlvdsRes));
    return rc;
}

RC VpllSpreadTest::Cleanup()
{
    RC rc = OK;
    delete[] lvdsRes.widths;
    delete[] lvdsRes.heights;
    delete[] lvdsRes.refreshRates;
    delete[] nonlvdsRes.widths;
    delete[] nonlvdsRes.heights;
    delete[] nonlvdsRes.refreshRates;
    return rc;
}

JS_CLASS_INHERIT(VpllSpreadTest, RmTest, "Tests the vpll spread.");

