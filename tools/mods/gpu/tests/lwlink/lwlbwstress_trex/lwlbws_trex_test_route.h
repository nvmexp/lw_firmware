/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2019,2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#pragma once

#include "gpu/tests/lwlink/lwldatatest/lwldt_test_route.h"

namespace LwLinkDataTestHelper
{
    //--------------------------------------------------------------------------
    //! \brief Class which encapsulates test route for LwLink bandwidth
    //! stress testing.  A route (just like an LwLinkRoute) represents
    //! multiple LwLinks where the source and destination device is the same.
    //!
    //! This class is mainly an informational repository and contains no actual
    //! copy functionality
    class TestRouteTrex : public TestRoute
    {
        public:
            TestRouteTrex
            (
                 TestDevicePtr  pLocDev
                ,LwLinkRoutePtr pRoute
                ,Tee::Priority  pri
            ) : TestRoute(pLocDev, pRoute, pri) {}
            virtual ~TestRouteTrex() {}

            RC CheckLinkState() override;
    };
};
