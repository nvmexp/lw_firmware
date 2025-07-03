/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2015-2017,2019-2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#pragma once

#ifndef INCLUDED_LWLINKTEST_H
#define INCLUDED_LWLINKTEST_H

#include <vector>
#include "gpu/tests/gputest.h"
#include "gpu/utility/lwlink/lwlinkdev.h"

class GpuSubdevice;

//------------------------------------------------------------------------------
//! \brief Base class for any LwLink related test.  TODO : Will provide
//! integrated error counting and checking
//!
class LwLinkTest : public GpuTest
{
public:
    LwLinkTest();
    virtual ~LwLinkTest();
    virtual RC Run();
    virtual RC Setup();
    virtual RC Cleanup();
    virtual bool IsSupported();

    virtual RC RunTest() = 0;
protected:
    RC GetLwLinkRoutes
    (
        TestDevicePtr pTestDevice,
        vector<LwLinkRoutePtr> *pRoutes
    );
    void SetDeferredRc(RC rc) { m_DeferredRc = rc; }
    RC AddUphyLogUntrainedLinksDevice(TestDevicePtr pTestDevice, set<TestDevicePtr> *pUphyLogDevices);
    RC LogUphyData(TestDevicePtr pTestDevice);

private:
    //! Deferred RC when not stopping on error
    StickyRC m_DeferredRc;
};

extern SObject LwLinkTest_Object;

#endif // INCLUDED_LWLINKTEST_H

