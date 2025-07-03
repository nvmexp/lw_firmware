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

#include "gpu/tests/lwlink/lwldatatest/lwldt_alloc_mgr.h"
#include "gpu/tests/lwlink/lwldatatest/lwldt_priv.h"

namespace LwLinkDataTestHelper
{
    //--------------------------------------------------------------------------
    //! \brief Class which manages allocations for the LwLink bandwidth stress
    //! test.  This is useful to avoid constant re-allocation of resources when
    //! test modes are changed since many resources can be reused in between
    //! test modes.  This manager takes a "lazy allocation" approach for
    //! surfaces and channels such that when a resource is "acquired" if one is
    //! not available it creates one and then returns that resource.  All
    //! semaphores are allocated at initialization time
    class AllocMgrTrex : public AllocMgr
    {
        public:
            AllocMgrTrex
            (
                GpuTestConfiguration * pTestConfig,
                Tee::Priority pri,
                bool bFailRelease
            ) : AllocMgr(pTestConfig, pri, bFailRelease) {}
            virtual ~AllocMgrTrex() {}
    };
};
