/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2019-2022 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#pragma once

#include "gpu/tests/lwlink/lwldatatest.h"

namespace MfgTest
{
    class LwLinkBwStressTrex : public LwLinkDataTest
    {
    public:
        LwLinkBwStressTrex();
        virtual ~LwLinkBwStressTrex();
        bool IsSupported() override;
        RC Setup() override;

        void PrintJsProperties(Tee::Priority pri);

        SETGET_PROP(DataLength, UINT32);
        SETGET_PROP(NumPackets, UINT32);

    protected:
        RC AssignTestTransferHw(LwLinkDataTestHelper::TestModePtr pTm) override;
        LwLinkDataTestHelper::AllocMgrPtr  CreateAllocMgr() override;
        LwLinkDataTestHelper::TestRoutePtr CreateTestRoute
        (
            TestDevicePtr  pLwLinkDev,
            LwLinkRoutePtr pRoute
        ) override;
        LwLinkDataTestHelper::TestModePtr  CreateTestMode() override;
        RC InitializeAllocMgr() override;

        void ConfigureTestMode
        (
            LwLinkDataTestHelper::TestModePtr pTm,
            LwLinkDataTestHelper::TransferDir td,
            LwLinkDataTestHelper::TransferDir sysmemTd,
            bool                              bLoopbackInput
        ) override;
        bool IsRouteValid(LwLinkRoutePtr pRoute) override;
        RC ValidateTestSetup() override;

    private:
        UINT32 m_DataLength = 256;
        UINT32 m_NumPackets = 10;
    };
};
