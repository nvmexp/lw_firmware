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
//! \file rmt_lwdispchannelexception.cpp
//! \brief the test inits LwDisplay HW and allocates a core and window channel.
//!

#include "lwos.h"
#include "lwRmApi.h"
#include "gpu/tests/rmtest.h"
#include "core/include/script.h"
#include "core/include/jscript.h"
#include "gpu/display/lwdisplay/lw_disp.h"
#include "gpu/include/dispchan.h"
#include "core/include/platform.h"
#include "core/include/utility.h"
#include "ctrl/ctrl5070.h"
#include "ctrl/ctrlc370.h"
#include "class/clc370.h"
#include "class/clc37d.h"
#include "class/clc37e.h"

using namespace std;

class LwDispExceptTest : public RmTest
{
public:
    LwDispExceptTest();
    virtual ~LwDispExceptTest();

    virtual string IsTestSupported();

    virtual RC Setup();
    virtual RC Run();
    virtual RC Cleanup();

private:
    Display *m_pDisplay;
    LwRm::Handle m_hClient;
};

LwDispExceptTest::LwDispExceptTest()
{
    m_pDisplay = GetDisplay();
    m_hClient  = 0;

    SetName("LwDispExceptTest");
}

//! \brief LwDispExceptTest destructor
//!
//! does  nothing
//!
//! \sa Cleanup
//------------------------------------------------------------------------------
LwDispExceptTest::~LwDispExceptTest()
{
}

//! \brief IsSupported
//!
//! Checks if the test can be run in the current environment.
//!
//! \return True if the test can be run in the current environment,
//!         false otherwise
//------------------------------------------------------------------------------
string LwDispExceptTest::IsTestSupported()
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
RC LwDispExceptTest::Setup()
{
    RC rc = OK;

    // Ask our base class(es) to setup internal state based on JS settings
    CHECK_RC(InitFromJs());

    return rc;
}

#ifndef MAX_WINDOWS
#define MAX_WINDOWS 32
#endif

//! \brief Run the test
//!
//! It will Init LwDisplay HW and allocate a core and window channel.
//------------------------------------------------------------------------------
RC LwDispExceptTest::Run()
{
    RC rc = OK;
    LwDisplay *pLwDisplay = m_pDisplay->GetLwDisplay();
    UINT32 winInst = 0;
    UINT32 nUsableWindows = 0;
    LwDispChnContext *pCoreDispContext = NULL;
    vector <LwDispChnContext *>pWindowDispContext;
    vector <WindowIndex>hWin;
    LwU32 expectedExceptionCnt[MAX_WINDOWS+1]={0,};
    LW5070_CTRL_CMD_SET_EXCEPTION_RESTART_MODE_PARAMS setExceptParams = {0,};
    LW5070_CTRL_CMD_GET_EXCEPTION_RESTART_MODE_PARAMS getExceptParams = {0,};
    LwRmPtr pLwRm;
    bool   bTestFailed = false;

    getExceptParams.base.subdeviceIndex = setExceptParams.base.subdeviceIndex = m_pDisplay->GetOwningDisplaySubdevice()->GetSubdeviceInst();
    getExceptParams.reason = setExceptParams.reason = LW5070_CTRL_CMD_GET_EXCEPTION_RESTART_MODE_REASON_ILWALID_ARG;
    getExceptParams.restartMode = setExceptParams.restartMode = LW5070_CTRL_CMD_GET_EXCEPTION_RESTART_MODE_RESTART_MODE_SKIP;
    getExceptParams.assert = setExceptParams.assert = LW5070_CTRL_CMD_GET_EXCEPTION_RESTART_MODE_ASSERT_NO;
    getExceptParams.exceptArg = setExceptParams.exceptArg = LW5070_CTRL_CMD_GET_EXCEPTION_RESTART_MODE_USE_EXCEPT_ARG_NO;
    getExceptParams.channel = setExceptParams.channel = 0;

    memset(expectedExceptionCnt, 0, sizeof(expectedExceptionCnt));
    // 1) Initialize display HW
    CHECK_RC(pLwDisplay->InitializeDisplayHW(Display::ALLOC_DISP_CHANNELS));

    // 2) Get core channel context. You'll need to typecast it before using.
    CHECK_RC(pLwDisplay->GetLwDisplayChnContext(Display::CORE_CHANNEL_ID,
                                                &pCoreDispContext,
                                                0,
                                                Display::ILWALID_HEAD));

    LwRm::Handle displayHandle;
    m_hClient = pLwRm->GetClientHandle();

    CHECK_RC(pLwDisplay->GetLwDispObject(&displayHandle));

    CHECK_RC(LwRmControl(m_hClient,
                         displayHandle,
                         LWC370_CTRL_CMD_SET_EXCEPTION_RESTART_MODE,
                         &setExceptParams,
                         sizeof(setExceptParams)));

    // 3) Get usable windows on this chip.
    nUsableWindows = pLwDisplay->GetUsableWindowsMask();
    NUMSETBITS_32(nUsableWindows);

    // 4) Resize the arrays.
    pWindowDispContext.resize(nUsableWindows);
    hWin.resize(nUsableWindows);

    //
    // 5) Allocate window/windowImm channels and get window contexts.
    // Contexts will need to be typecasted before using.
    // Core channel is already allocated
    //
    for (winInst = 0; winInst < nUsableWindows; winInst++)
    {
        CHECK_RC(pLwDisplay->AllocateWinAndWinImm(&hWin[winInst], winInst>>1, winInst));
        CHECK_RC((pLwDisplay->GetLwDisplayChnContext(Display::WINDOW_CHANNEL_ID,
                                                    &pWindowDispContext[winInst],
                                                    winInst,
                                                    Display::ILWALID_HEAD)));
        setExceptParams.channel          = winInst+1;
        setExceptParams.manualRestart    = LW5070_CTRL_CMD_SET_EXCEPTION_RESTART_MODE_MANUAL_RESTART_NO;

        CHECK_RC(LwRmControl(m_hClient,
                         displayHandle,
                         LWC370_CTRL_CMD_SET_EXCEPTION_RESTART_MODE,
                         &setExceptParams,
                         sizeof(setExceptParams)));
    }

    // Send methods with invalid arguments for core and all window channels.
    for (winInst = 0; winInst < nUsableWindows+1; winInst++)
    {
        LwDispChnContext *pChCtx = NULL;
        DisplayChannel   *pCh    = NULL;
        LwU32             method1 = 8; // Reserved00 offset in window and core channels
        LwU32             value1  = 0;
        if (winInst == 0)
        {
        method1 = LWC37D_SOR_SET_CONTROL(0);
        value1  = 0xFFFFFFFF;
            pChCtx = pCoreDispContext;
        }
        else
        {
        method1 = LWC37E_SET_STORAGE;
        value1  = 0xFFFFFFFF;
            pChCtx = pWindowDispContext[(winInst-1)];
        }

        pCh = pChCtx->GetChannel();
        pCh->Write(method1, value1);
        pCh->Flush();

        expectedExceptionCnt[winInst]++;
    }

    //
    // wait for all get==put before checking for exceptions
    // otherwise we might end-up checking the channel status
    // before method processing starts. Also this here would parallelize
    // waits for multiple channels
    //
    for (winInst = 0; winInst < nUsableWindows+1; winInst++)
    {
        LwDispChnContext *pChCtx = NULL;
        DisplayChannel   *pCh    = NULL;
        std::string       chan="";
        if (winInst == 0)
        {
            pChCtx = pCoreDispContext;
        chan = "core";
        }
        else
        {
            pChCtx = pWindowDispContext[(winInst-1)];
            chan = "window";
        }

        pCh = pChCtx->GetChannel();
        DmaDisplayChannelC3 *pDmaCh = static_cast<DmaDisplayChannelC3*>(pCh);
        while(1)
        {
            if (pDmaCh->GetGet(chan) == pDmaCh->GetPut(chan))
            break;
        }
    }

    // Wait until the channel has consumed the methods and any exceptions are handled by RM
    int idleStateRetryCnt = 0;
    LWC370_CTRL_CMD_GET_CHANNEL_INFO_PARAMS getChInfo = {0,};
    for (winInst = 0; winInst < nUsableWindows+1 && bTestFailed == false; winInst++)
    {
        idleStateRetryCnt = 0;
        if (winInst == 0)
        {
            getChInfo.channelClass = LWC37D_CORE_CHANNEL_DMA;
            getChInfo.channelInstance = 0;
        }
        else
        {
            getChInfo.channelClass = LWC37E_WINDOW_CHANNEL_DMA;
            getChInfo.channelInstance = winInst-1;
        }
        while (1)
        {
            CHECK_RC(LwRmControl(m_hClient,
                                 displayHandle,
                                 LWC370_CTRL_CMD_GET_CHANNEL_INFO,
                                 &getChInfo,
                                 sizeof(getChInfo)));

            if (getChInfo.channelState == LWC370_CTRL_GET_CHANNEL_INFO_STATE_IDLE)
            {
                getExceptParams.channel = winInst;
                getExceptParams.exceptCnt = 0;
                CHECK_RC(LwRmControl(m_hClient,
                     displayHandle,
                     LWC370_CTRL_CMD_GET_EXCEPTION_RESTART_MODE,
                     &getExceptParams,
                     sizeof(getExceptParams)));

                if (getExceptParams.exceptCnt == expectedExceptionCnt[winInst])
                   break;

                idleStateRetryCnt++;
                if (idleStateRetryCnt == 5)
                {
                     bTestFailed = true;
                     break;
                }
            }
            else if (getChInfo.channelState == LWC370_CTRL_GET_CHANNEL_INFO_STATE_BUSY)
            {
                continue;
            }
            else
            {
                // unexpected channel state
                return RC::SOFTWARE_ERROR;
            }
        } // while(1)
    }

    // 6) Delete all the allocated windows.
    for (winInst = 0; winInst < nUsableWindows; winInst++)
    {
        CHECK_RC(pLwDisplay->DestroyWinAndWinImm(hWin[winInst]));
    }

    if (bTestFailed && rc == OK)
        rc = RC::SOFTWARE_ERROR;

    return rc;
}

//! \brief Cleanup, does nothing for this simple test
//------------------------------------------------------------------------------
RC LwDispExceptTest::Cleanup()
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
JS_CLASS_INHERIT(LwDispExceptTest, RmTest,
    "Simple test to verify if exception interrupts and RM handling of exceptions is working for lwdisplay.");

