/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2017,2019,2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */
#pragma once
#include "gpu/tests/lwlink/lwldatatest.h"
#include "gpu/tests/lwca/lwlwl/lwlwl_test_route.h"
#include "gpu/tests/lwca/lwlwl/lwlwl_test_mode.h"
#include "gpu/tests/lwca/lwlink/lwlink.h"
#include <memory>

class TestDevice;
typedef shared_ptr<TestDevice> TestDevicePtr;

namespace MfgTest
{
    //--------------------------------------------------------------------------
    //! \brief Lwca based lwlink data transfer test. The default configuration
    //! for this test attempts to saturate all LwLinks connected to the DUT
    //! in all directions simultaneously.  Test arguments are provided that can
    //! expand that with the ability to test all combinations for routes,
    //! directions, and transfer types.
    class LwdaLwLink : public LwLinkDataTest
    {
    public:
        LwdaLwLink();
        virtual ~LwdaLwLink();
        virtual bool IsSupported();
        virtual RC Setup();
        RC BindTestDevice(TestDevicePtr pTestDevice, JSContext *cx, JSObject *obj) override;
        virtual void PropertyHelp(JSContext *pCtx, regex *pRegex);
        virtual RC Cleanup();

        void PrintJsProperties(Tee::Priority pri);

        SETGET_PROP(Ce2dMemorySizeFactor,     UINT32);
        SETGET_PROP(P2PSmAllocMode,           UINT32);
        SETGET_PROP(MaxPerRtePerDirSms,       UINT32);
        SETGET_PROP(MinPerRtePerDirSms,       UINT32);
        SETGET_PROP(DataTransferWidth,        UINT32);
        SETGET_PROP(SubtestType,              UINT32);
        SETGET_PROP(AutoSmLimits,             bool);
        SETGET_PROP(GupsUnroll,               UINT32);
        SETGET_PROP(GupsAtomicWrite,          bool);
        SETGET_PROP(GupsMemIdxType,           INT32);
        SETGET_PROP(GupsMemorySize,           UINT32);
        SETGET_PROP(WarmupSec,                UINT32);

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
        virtual RC ValidateTestSetup();
    private:
        Lwca::Instance m_Lwda;
        Lwca::Device m_LwdaDevice;

        //! JS variables (see JS interfaces for description)
        UINT32  m_Ce2dMemorySizeFactor = 10;
        UINT32  m_P2PSmAllocMode       = LwLinkDataTestHelper::TestModeLwda::P2P_ALLOC_DEFAULT;
        UINT32  m_MinPerRtePerDirSms   = 0;
        UINT32  m_MaxPerRtePerDirSms   = LwLinkDataTestHelper::TestRouteLwda::ALL_SMS;
        bool    m_AutoSmLimits         = false;
        UINT32  m_DataTransferWidth    = LwLinkDataTestHelper::TestModeLwda::TW_8BYTES;
        UINT32  m_SubtestType          = LwLinkDataTestHelper::TestModeLwda::ST_GUPS;
        UINT32  m_GupsUnroll           = 8;
        UINT32  m_GupsAtomicWrite      = false;
        INT32   m_GupsMemIdxType       = LwLinkKernel::RANDOM;
        INT32   m_GupsMemorySize       = 0;
        UINT32  m_WarmupSec            = 1;

        RC AssignSms(LwLinkDataTestHelper::TestModeLwda * pTm);
    };
};
