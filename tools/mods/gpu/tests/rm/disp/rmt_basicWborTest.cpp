/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2014-2015,2019 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

//!
//!
//! \file rmt_basicWborTest.cpp
//! \brief Sample test to demonstrate use of SetMode() API.
//!.\It does a modeset on all the detected displays. After each modeset
//!.\completion, the test detaches that display and performs the same
//! \sequence on the other detected displays.
//!

#include "gpu/tests/rmtest.h"
#include "core/include/script.h"
#include "core/include/jscript.h"
#include "gpu/include/dispchan.h"
#include "gpu/display/evo_disp.h"
#include "gpu/display/evo_chns.h"
#include "core/include/platform.h"
#include "core/include/utility.h"
#include "lwRmApi.h"
#include "lwmisc.h"
#include "core/include/xp.h"

#include "class/cl9878.h"   // LW9878_WRITEBACK_CHANNEL_DMA

#include "ctrl/ctrl0073.h" // For LW0073_CTRL_CMD_SPECIFIC_GET_WBOR_PRESENT

#include "core/include/memcheck.h"

class BasicWborTest : public RmTest
{
public:
    BasicWborTest();
    virtual ~BasicWborTest();

    virtual string IsTestSupported();

    virtual RC Setup();
    virtual RC Run();

    virtual RC Cleanup();

private:
    Display* m_pDisplay;

};

//! \brief BasicWborTest constructor
//!
//! Doesn't do much except SetName.
//!
//! \sa Setup
//------------------------------------------------------------------------------
BasicWborTest::BasicWborTest()
: m_pDisplay(nullptr)
{
    SetName("BasicWborTest");
}

//! \brief BasicWborTest destructor
//!
//! Doesn't do much.
//!
//! \sa Cleanup
//------------------------------------------------------------------------------
BasicWborTest::~BasicWborTest()
{

}

//! \brief IsTestSupported
//!
//! Checks if the test can be run in the current environment.
//!
//! \return True if the test can be run in the current environment,
//!         false otherwise
//------------------------------------------------------------------------------
string BasicWborTest::IsTestSupported()
{
    Display        *pDisplay = GetDisplay();
    const UINT32    SupportedClasses[] =
    {
        LW9878_WRITEBACK_CHANNEL_DMA,
    };
    const UINT32    numSupportedClasses = sizeof(SupportedClasses) / sizeof(SupportedClasses[0]);

    if(pDisplay->GetDisplayClassFamily() != Display::EVO)
    {
        return "EVO Display class is not supported on current platform";
    }

    for(UINT32 i=0; i<numSupportedClasses; i++)
    {
        if(IsClassSupported(SupportedClasses[i]))
            return RUN_RMTEST_TRUE;
    }

    return "Writeback class not supported";
}

//! \brief Just doing Init from Js, nothing else.
//!
//! \return if InitFromJs passed, else corresponding rc
//------------------------------------------------------------------------------
RC BasicWborTest::Setup()
{
    RC rc;

    // Ask our base class(es) to setup internal state based on JS settings
    CHECK_RC(InitFromJs());

    m_pDisplay = GetDisplay();

    return rc;
}

//! \brief Run the test
//!
//! Call the SetMode() API with some default values
//!
//! \return OK if the SetMode call succeeded, else corresponding rc
//------------------------------------------------------------------------------
RC BasicWborTest::Run()
{
    RC                      rc = OK;
    DisplayIDs              Displays;
    EvoDisplay             *pEvoDisp;
    EvoDisplayChnContext   *pWrbkChnContext;
    DmaDisplayChannel      *chn;
    string                  chnUserdName("core");
    UINT32                  getVal;
    UINT32                  putVal;

    //
    // Display HW must be Initialized.. this should be called only for once.
    // For WinMODS where display driver is running, this will just be a no-op.
    //
    CHECK_RC(m_pDisplay->InitializeDisplayHW(Display::ALLOC_DISP_CHANNELS));

    CHECK_RC(m_pDisplay->GetDetected(&Displays));

    pEvoDisp = m_pDisplay->GetEvoDisplay();
    if(pEvoDisp == NULL)
    {
        Printf(Tee::PriHigh, "Unable to obtain EvoDisplay\n");
        return RC::SOFTWARE_ERROR;
    }

    // Test control call before we do anything else
    LW0073_CTRL_SPECIFIC_GET_WBOR_PRESENT_MASK_PARAMS params = {0};
    params.subDeviceInstance = 0;

    CHECK_RC(m_pDisplay->RmControl(LW0073_CTRL_CMD_SPECIFIC_GET_WBOR_PRESENT_MASK,
                            &params, sizeof(params)));

    // We should at least have 1 supported WBOR
    MASSERT(params.wborPresentMask != LW0073_CTRL_CMD_SPECIFIC_GET_WBOR_PRESENT_NONE);

    Printf(Tee::PriHigh, "The following WBORs reported present:\n");
    for (UINT32 i = 0; i <= LW0073_CTRL_CMD_SPECIFIC_GET_WBOR_PRESENT_MAX_IDX; ++i)
    {
        if (params.wborPresentMask & (1 << i))
        {
            Printf(Tee::PriHigh, "\tWBOR%d\n", i);
        }
    }

    pWrbkChnContext = pEvoDisp->GetEvoDisplayChnContext(Displays[0], Display::WRITEBACK_CHANNEL_ID);

    // Was the writeback channel allocated?
    if(pWrbkChnContext == NULL)
    {
        Printf(Tee::PriHigh, "Unable to allocate writeback channel\n");
        return RC::SOFTWARE_ERROR;
    }

    // Get the DmaDisplayChannel from DisplayChannel to access
    // DMA channel specific methods
    chn = dynamic_cast<DmaDisplayChannel*>(pWrbkChnContext->GetChannel());
    if(chn == NULL)
    {
        Printf(Tee::PriHigh, "Unable to retrieve writeback channel\n");
        return RC::SOFTWARE_ERROR;
    }

    // Enable channel logging so we see some spew
    chn->EnableLogging(true);

    // Flush the pb at the start
    Printf(Tee::PriHigh, "Writeback Test initial flush\n");
    CHECK_RC(chn->Flush());
    CHECK_RC(chn->WaitForDmaPush(chn->GetDefaultTimeoutMs()));

    Printf(Tee::PriHigh, "GET == %08X, PUT == %08X\n",
           chn->GetGet(chnUserdName), chn->GetPut(chnUserdName));

    // For the basic test we just want to see if we can allocate the channel and
    // write some methods into it. So push in methods, flush and wait for GET == PUT
    Printf(Tee::PriHigh, "Method push\n");
    CHECK_RC(chn->Write(LW9878_SET_FRAME_PERIOD, 250));
    CHECK_RC(chn->Write(LW9878_SET_FRAME_PERIOD, 250));
    CHECK_RC(chn->Write(LW9878_SET_FRAME_PERIOD, 250));
    CHECK_RC(chn->Flush());
    CHECK_RC(chn->WaitForDmaPush(chn->GetDefaultTimeoutMs()));

    // Check that GET == PUT
    getVal = chn->GetGet(chnUserdName);
    putVal = chn->GetPut(chnUserdName);
    Printf(Tee::PriHigh, "GET == %08X, PUT == %08X\n", getVal, putVal);

    if(getVal != putVal)
    {
        Printf(Tee::PriHigh, "Test failed as GET != PUT\n");
        rc = RC::SOFTWARE_ERROR;
    }
    else
    {
        Printf(Tee::PriHigh, "Complete, GET == PUT\n");
    }

    return rc;
}

//! \brief Cleanup, does nothing for this simple test
//------------------------------------------------------------------------------
RC BasicWborTest::Cleanup()
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
JS_CLASS_INHERIT(BasicWborTest, RmTest,
    "Simple test to check that we can allocate a WBOR channel and send methods");

