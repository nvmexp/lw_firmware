/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2016-2022 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */
#pragma once

#ifndef __INCLUDED_LWLBWSTRESS_H__
#define __INCLUDED_LWLBWSTRESS_H__

#include "gpu/tests/lwlink/lwldatatest.h"
#include "gpu/tests/lwlink/lwlbwstress/lwlbws_alloc_mgr.h"
#include "gpu/tests/lwlink/lwlbwstress/lwlbws_test_route.h"
#include "gpu/tests/lwlink/lwlbwstress/lwlbws_test_mode.h"

#include <vector>
#include <set>
#include <memory>

class GpuTestConfiguration;
class Goldelwalues;
class GpuSubdevice;
class FancyPickerArray;
class TestDevice;
typedef shared_ptr<TestDevice> TestDevicePtr;

namespace MfgTest
{
    //--------------------------------------------------------------------------
    //! \brief LwLink bandwidth stress test.  The default configuration for this
    //! test attempts to saturate all LwLinks connected to the DUT in all directions
    //! simultaneously.  Test arguments are provided that can expand that with the
    //! ability to test all combinations for routes, directions, and transfer
    //! types. For a description of how data transfers are achieved, please see
    //! the description of LwLinkBwStressHelper::TestMode in lwlbws_test_mode.h
    //!
    //! Note : This test assumes that copy engines are in a 1:1 LCE->PCE mapping
    class LwLinkBwStress : public LwLinkDataTest
    {
    public:
        LwLinkBwStress();
        virtual ~LwLinkBwStress();
        bool IsSupported() override;
        RC Setup() override;

        void PrintJsProperties(Tee::Priority pri);

        SETGET_PROP(SurfSizeFactor,           UINT32);
        SETGET_PROP(SrcSurfMode,              UINT32);
        SETGET_PROP(SrcSurfModePercent,       UINT32);
        SETGET_PROP(Use2D,                    bool);
        SETGET_PROP(CopyByLine,               bool);
        SETGET_PROP(SrcSurfLayout,            UINT32);
        SETGET_PROP(CeTransferWidth,          UINT32);
        SETGET_PROP(PixelComponentMask,       UINT32);
        SETGET_PROP(P2PCeAllocMode,           UINT32);
        SETGET_PROP(TestFullFabric,           bool);
        SETGET_PROP(UseLwdaCrc,               bool);
        SETGET_PROP(FbSurfPageSizeKb,         UINT32);
        SETGET_PROP_LWSTOM(UseFla,            bool);
        SETGET_PROP(FlaPageSizeKb,            UINT32);
        SETGET_PROP(UseLwda,                  bool);
        SETGET_PROP(UseDmaInit,               bool);
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
        bool IsTestModeValid(LwLinkDataTestHelper::TestModePtr pTm) override;
        RC ValidateTestSetup() override;
    private:
        RC InitFromJs() override;

        LwLink::HwType m_Engine = LwLink::HT_CE; //!< Which engine is used to perform copies

        bool    m_bPropP2PSupported = false; //!< true if PROP supports P2P transfers
        bool    m_OnePcePerLce      = false; //!< True if GPU supports 1:1 PCE/LCE configuration
        bool    m_FlaSetFromJs      = false; //!< True if FLA mode was provided explicitly

        //! JS variables (see JS interfaces for description)
        UINT32  m_SurfSizeFactor     = 10;
        UINT32  m_SrcSurfMode        = 0;
        UINT32  m_SrcSurfModePercent = 50;
        bool    m_Use2D              = false;
        bool    m_CopyByLine         = false;
        UINT32  m_SrcSurfLayout      = Surface2D::Pitch;
        UINT32  m_CeTransferWidth    = 0;
        UINT32  m_PixelComponentMask = LwLinkDataTestHelper::TestModeCe2d::ALL_COMPONENTS;
        UINT32  m_P2PCeAllocMode     = LwLinkDataTestHelper::TestModeCe2d::P2P_ALLOC_DEFAULT;
        bool    m_TestFullFabric     = false;
        bool    m_UseLwdaCrc         = true;
        UINT32  m_FbSurfPageSizeKb   = 0;
        bool    m_UseFla             = false;
        UINT32  m_FlaPageSizeKb      = 2048;
        bool    m_UseLwda            = true;
        bool    m_UseDmaInit         = true;

        RC AssignCopyEngines(LwLinkDataTestHelper::TestModeCe2d * pTm);
        RC GetMaxRequiredSemaphores(UINT32 *pReqSemaphores);
        RC GetPropP2PSupported(bool *pbSupported);

        static const UINT32 FULL_FABRIC_RANGE = 0x2000; //8K
    };
};

#endif
