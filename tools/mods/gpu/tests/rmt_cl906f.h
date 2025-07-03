/*
* LWIDIA_COPYRIGHT_BEGIN
*
* Copyright 2007-2008 by LWPU Corporation. All rights reserved.
* All information contained herein is proprietary and confidential
* to LWPU Corporation. Any use, reproduction, or disclosure without
* the written permission of LWPU Corporation is prohibited.
*
* LWIDIA_COPYRIGHT_END
*/

//! \file rmt_cl906f.h
//! \brief Fermi Host Non-Stall Interrupt test
//!
//! This test sends the LW906F_NON_STALL_INTERRUPT method
//! to cause an interrupt to be sent to RM

#ifndef INCLUDED_CLASS906FTEST_H
#define INCLUDED_CLASS906FTEST_H

#include "rmtest.h"
#include "core/include/channel.h"
#include "core/include/tasker.h"
#include "gpu/include/notifier.h"

#define CLASS906FTEST_CHANNELS_COUNT              (5)

class Class906fTest : public RmTest
{
public:
    Class906fTest();
    virtual ~Class906fTest();
    virtual string IsTestSupported();
    virtual RC Setup();
    virtual RC Run();
    virtual RC Cleanup();

    SETGET_PROP(MethodPerCHID, UINT32);

private:
    Channel        *m_pCh[CLASS906FTEST_CHANNELS_COUNT];
    LwRm::Handle    m_hCh[CLASS906FTEST_CHANNELS_COUNT];
    Notifier        m_notifier[CLASS906FTEST_CHANNELS_COUNT];
    UINT32          m_MethodPerCHID;
    RC tamperChannelProperties(UINT32);
    RC WaitStream(UINT32);
    RC WriteStream(UINT32, bool);
    RC WriteHostSoftwareMethod(UINT32);
    RC RunRegularStream();
    RC RunWithMidStreamTampering();
    RC RunAllStreamsThenWait();
    RC RunMultiEvents();
    RC RunWithGraphics();
    RC RunWithHostSoftwareMethods();

    LwBool          m_bNeedWfi;

// Quick and dirty hack to allow this test to be a background test.
// We need this for the FermiClockxxx tests
// runControl controls all instances of this test.
public:
    enum RunControl
    {   StopRunning, ContinueRunning, FirstTimeRunning  };
    static RunControl runControl;

};

#endif

