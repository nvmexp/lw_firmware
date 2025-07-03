/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2015-2016, 2019 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

//!
//!
//! \file rmt_ChannelAllocation.cpp
//! \brief the test inits LwDisplay HW and allocates a core and window channel.
//!

#include "gpu/tests/rmtest.h"
#include "core/include/script.h"
#include "core/include/jscript.h"
#include "gpu/display/lwdisplay/lw_disp.h"
#include "core/include/platform.h"
#include "core/include/utility.h"

using namespace std;

class ChannelAllocationTest : public RmTest
{
public:
    ChannelAllocationTest();
    virtual ~ChannelAllocationTest();

    virtual string IsTestSupported();

    virtual RC Setup();
    virtual RC Run();
    virtual RC Cleanup();

private:
    Display *m_pDisplay;
};

ChannelAllocationTest::ChannelAllocationTest()
{
    m_pDisplay = GetDisplay();

    SetName("ChannelAllocationTest");
}

//! \brief ChannelAllocationTest destructor
//!
//! does  nothing
//!
//! \sa Cleanup
//------------------------------------------------------------------------------
ChannelAllocationTest::~ChannelAllocationTest()
{
}

//! \brief IsSupported
//!
//! Checks if the test can be run in the current environment.
//!
//! \return True if the test can be run in the current environment,
//!         false otherwise
//------------------------------------------------------------------------------
string ChannelAllocationTest::IsTestSupported()
{
    if (m_pDisplay->GetDisplayClassFamily() != Display::LWDISPLAY)
    {
        return "The test is supported only on LwDisplay class!";
    }

    return RUN_RMTEST_TRUE;
}

//! \setup Initialise internal state from JS
//!
//! Initial state setup based on the JS setting
//------------------------------------------------------------------------------
RC ChannelAllocationTest::Setup()
{
    RC rc = OK;

    // Ask our base class(es) to setup internal state based on JS settings
    CHECK_RC(InitFromJs());

    return rc;
}

//! \brief Run the test
//!
//! It will Init LwDisplay HW and allocate a core and window channel.
//------------------------------------------------------------------------------
RC ChannelAllocationTest::Run()
{
    RC rc = OK;
    LwDisplay *pLwDisplay = m_pDisplay->GetLwDisplay();
    UINT32 winInst = 0;
    UINT32 nUsableWindows = 0;
    LwDispChnContext *pCoreDispContext = NULL;
    vector <LwDispChnContext *>pWindowDispContext;
    vector <WindowIndex>hWin;

    // 1) Initialize display HW
    CHECK_RC(pLwDisplay->InitializeDisplayHW(Display::ALLOC_DISP_CHANNELS));

    // 2) Get core channel context. You'll need to typecast it before using.
    CHECK_RC(pLwDisplay->GetLwDisplayChnContext(Display::CORE_CHANNEL_ID,
                                                &pCoreDispContext,
                                                0,
                                                Display::ILWALID_HEAD));

    // 3) Get usable windows on this chip.
    nUsableWindows = pLwDisplay->GetUsableWindowsMask();
    NUMSETBITS_32(nUsableWindows);

    // 4) Resize the arrays.
    pWindowDispContext.resize(nUsableWindows);
    hWin.resize(nUsableWindows);

    //
    // allocate all the possible windows and delete them.
    // for now we are not supporting the case of window floorsweeping
    //

    //
    // 5) Allocate window/windowImm channels and get window contexts.
    // Contexts will need to be typecasted before using.
    //
    for (winInst = 0; winInst < nUsableWindows; winInst++)
    {
        CHECK_RC(pLwDisplay->AllocateWinAndWinImm(&hWin[winInst], winInst>>1, winInst));
        CHECK_RC((pLwDisplay->GetLwDisplayChnContext(Display::WINDOW_CHANNEL_ID,
                                                    &pWindowDispContext[winInst],
                                                    winInst,
                                                    Display::ILWALID_HEAD)));
    }

    // 6) Delete all the allocated windows.
    for (winInst = 0; winInst < nUsableWindows; winInst++)
    {
        CHECK_RC(pLwDisplay->DestroyWinAndWinImm(hWin[winInst]));
    }

    return rc;
}

//! \brief Cleanup, does nothing for this simple test
//------------------------------------------------------------------------------
RC ChannelAllocationTest::Cleanup()
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
JS_CLASS_INHERIT(ChannelAllocationTest, RmTest,
    "Simple test to check verify Init of Display HW and channel allocation.");

