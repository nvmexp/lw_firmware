/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2018-2019,2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */
#pragma once

#include "gpu/tests/lwlink/lwldatatest.h"
#include "gpu/tests/lwlink/lwlbwstress_tegra/lwlbwst_alloc_mgr.h"
#include "gpu/tests/lwlink/lwlbwstress_tegra/lwlbwst_test_route.h"
#include "gpu/tests/lwlink/lwlbwstress_tegra/lwlbwst_test_mode.h"

class GpuSubdevice;
class TestDevice;
typedef shared_ptr<TestDevice> TestDevicePtr;

namespace MfgTest
{
    //--------------------------------------------------------------------------
    //! \brief LwLink bandwidth stress test for cheetah.  This test assumes a single
    //! loopback test route is present with no other connections
    class LwLinkBwStressTegra : public LwLinkDataTest
    {
    public:
        LwLinkBwStressTegra();
        virtual ~LwLinkBwStressTegra();
        virtual bool IsSupported();

        void PrintJsProperties(Tee::Priority pri);

        SETGET_PROP(MemorySizeKb,  UINT32);
        SETGET_PROP(MemoryPattern, UINT32);
    protected:
        virtual RC AssignTestTransferHw(LwLinkDataTestHelper::TestModePtr pTm);
        virtual LwLinkDataTestHelper::AllocMgrPtr  CreateAllocMgr();
        virtual LwLinkDataTestHelper::TestRoutePtr CreateTestRoute
        (
            TestDevicePtr  pLwLinkDev,
            LwLinkRoutePtr pRoute
        );
        virtual LwLinkDataTestHelper::TestModePtr  CreateTestMode();
        virtual RC InitializeAllocMgr();

        virtual void ConfigureTestMode
        (
            LwLinkDataTestHelper::TestModePtr pTm,
            LwLinkDataTestHelper::TransferDir td,
            LwLinkDataTestHelper::TransferDir sysmemTd,
            bool                              bLoopbackInput
        );
        virtual bool IsRouteValid(LwLinkRoutePtr pRoute);
        virtual bool IsTestModeValid(LwLinkDataTestHelper::TestModePtr pTm);
        virtual RC ValidateTestSetup();
    private:

        //! JS variables (see JS interfaces for description)
        UINT32  m_MemorySizeKb;
        UINT32  m_MemoryPattern;
    };
};
