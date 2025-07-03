/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2017-2021 by LWPU Corporation. All rights reserved. All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */
#pragma once

#ifndef __INCLUDED_LWLDATATEST_H__
#define __INCLUDED_LWLDATATEST_H__

#include "core/include/jscript.h"
#include "core/include/jscallbacks.h"

#include "gpu/tests/lwlink/lwlinktest.h"
#include "gpu/tests/lwlink/lwldatatest/lwldt_alloc_mgr.h"
#include "gpu/tests/lwlink/lwldatatest/lwldt_test_route.h"
#include "gpu/tests/lwlink/lwldatatest/lwldt_test_mode.h"

#include <vector>
#include <set>
#include <tuple>

class GpuTestConfiguration;
class Goldelwalues;
class GpuSubdevice;

namespace MfgTest
{
    //--------------------------------------------------------------------------
    //! \brief Base class for a LwLink test that performs data transfers.  The
    //! default configuration of data based tests is that they will attempt to
    //! saturate all LwLinks connected to the DUT in all directions simultaneously.
    //! Test arguments are provided that can expand that with the ability to test
    //! all combinations for routes, directions, and transfer types. Each
    //! derived data transfer test may transfer data in different ways using
    //! different engines
    //!
    class LwLinkDataTest : public LwLinkTest
    {
        // If you create a descendant of this class, be aware that if
        // UseDataFlowObj is true, LwLinkDataTest sets test routes in a way that
        // the "local" device inside the TestRoute instance is the one that
        // generates the traffic. It's up to your descendant class to setup the
        // copy engines at the correspondent "local" end of the TestRoute.
    public:
        struct DataFlowProps
        {
            static constexpr const char *const REQUEST = ENCJSENT("request");
            static constexpr const char *const RESPONSE = ENCJSENT("response");
            static constexpr const char *const SOURCE = ENCJSENT("source");
            static constexpr const char *const SINK = ENCJSENT("sink");
            static constexpr const char *const NODE = ENCJSENT("node");
            static constexpr const char *const ADDR_REGION = ENCJSENT("addr_region");
        };

        enum
        {
            ALL_ROUTES = ~0U //!< Specify all routes
           ,ALL_LINKS = ~0U //!< Specify all links
        };

        enum LoopbackTestType
        {
            LBT_READ       = (1 << 0)
           ,LBT_WRITE      = (1 << 1)
           ,LBT_READ_WRITE = (LBT_READ | LBT_WRITE)
        };

        LwLinkDataTest();
        virtual ~LwLinkDataTest();
        virtual bool IsSupported();
        virtual RC Setup();
        virtual RC Cleanup();

        virtual RC RunTest();

        void PrintJsProperties(Tee::Priority pri);

        SETGET_PROP(NumErrorsToPrint,         UINT32);
        SETGET_PROP(BwThresholdPct,           FLOAT64);
        SETGET_PROP(TransferDirMask,          UINT32);
        SETGET_PROP(TransferTypeMask,         UINT32);
        SETGET_PROP(MaxSimulRoutes,           UINT32);
        SETGET_PROP(MinSimulRoutes,           UINT32);
        SETGET_PROP(LwLinkIdMask,             UINT32);
        SETGET_PROP(ShowTestModes,            bool);
        SETGET_PROP(ShowBandwidthData,        bool);
        SETGET_PROP(CopyLoopCount,            UINT32);
        SETGET_PROP(DumpSurfaceOnMiscompare,  bool);
        SETGET_PROP(SkipBandwidthCheck,       bool);
        SETGET_PROP(SkipSurfaceCheck,         bool);
        SETGET_PROP(SysmemLoopback,           bool);
        SETGET_PROP(PauseTestMask,            UINT32);
        SETGET_PROP(PerModeRuntimeSec,        UINT32);
        SETGET_PROP(RuntimeMs,                UINT32);
        SETGET_PROP(TestAllGpus,              bool);
        SETGET_PROP(ShowTestProgress,         bool);
        SETGET_PROP(SkipLinkStateCheck,       bool);
        SETGET_PROP(FailOnSysmemBw,           bool);
        SETGET_PROP(AddCpuTraffic,            bool);
        SETGET_PROP(NumCpuTrafficThreads,     UINT32);
        SETGET_PROP(TogglePowerState,         bool);
        SETGET_PROP(TogglePowerStateIntMs,    FLOAT64);
        SETGET_PROP(LPCountCheckTol,          UINT32);
        SETGET_PROP(LPCountCheckTolPct,       FLOAT64);
        SETGET_PROP(SkipLPCountCheck,         bool);
        SETGET_PROP(MinLPCount,               UINT32);
        SETGET_PROP(LowPowerBandwidthCheck,   bool);
        SETGET_PROP(CheckHwPowerStateSwitch,  bool);
        SETGET_PROP(ShowLowPowerCheck,        bool);
        SETGET_PROP(IdleAfterCopyMs,          UINT32);
        SETGET_PROP(ThermalThrottling,        bool);
        SETGET_PROP(ThermalThrottlingOnCount, UINT32);
        SETGET_PROP(ThermalThrottlingOffCount,UINT32);
        SETGET_PROP(ThermalSlowdown,          bool);
        SETGET_PROP(ThermalSlowdownPeriodUs,  UINT32);
        SETGET_PROP(LoopbackTestType,         UINT32);
        SETGET_PROP(DataFlow,                 JSObject *);
        SETGET_PROP(DataFlowFile,             string);
        SETGET_PROP(UseDataFlowObj,           bool);
        SETGET_PROP(UseOptimal,               bool);
        SETGET_PROP(ShowPortStats,            bool);
        SETGET_PROP(SkipPortStatsCheck,       bool);
        SETGET_PROP(LowLatencyThresholdNs,    UINT32);
        SETGET_PROP(MidLatencyThresholdNs,    UINT32);
        SETGET_PROP(HighLatencyThresholdNs,   UINT32);
        SETGET_PROP(IsBgTest,                 bool);
        SETGET_PROP(KeepRunning,              bool);
        SETGET_PROP(L2AfterCopy,              bool);
        SETGET_PROP(L2DurationMs,             double);
        SETGET_PROP(GCxInL2,                  UINT32);
        SETGET_PROP(EnableUphyLogging,        bool);
    protected:
        // Subclasses must provide a way to create their test specific helpers and
        // assign test specific hardware
        virtual RC AssignTestTransferHw(LwLinkDataTestHelper::TestModePtr pTm) = 0;
        virtual LwLinkDataTestHelper::AllocMgrPtr  CreateAllocMgr() = 0;
        virtual LwLinkDataTestHelper::TestRoutePtr CreateTestRoute
        (
            TestDevicePtr  pLwLinkDev,
            LwLinkRoutePtr pRoute
        ) = 0;
        virtual LwLinkDataTestHelper::TestModePtr  CreateTestMode() = 0;
        virtual RC InitializeAllocMgr() = 0;

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

        LwLinkDataTestHelper::AllocMgrPtr GetAllocMgr() { return m_pAllocMgr; }
        const set<TestDevicePtr> & GetTestedDevs() const { return m_TestedDevs; }
        const vector<LwLinkDataTestHelper::TestRoutePtr> & GetTestRoutes() const
            { return m_TestRoutes; }

    private:
        //! Generic Test data
        GpuTestConfiguration *m_pTestConfig;
        Goldelwalues *        m_pGolden;

        //! List of routes that will be excersized by the test
        vector<LwLinkDataTestHelper::TestRoutePtr> m_TestRoutes;

        //! List of test modes that will be run
        vector<LwLinkDataTestHelper::TestModePtr> m_TestModes;

        //! Set of GpuSubdevices that are being tested
        set<TestDevicePtr> m_TestedDevs;
        set<TestDevicePtr> m_CounterReservedDevs;

        //!< Allocation manager for managing surface/CE allocations
        LwLinkDataTestHelper::AllocMgrPtr m_pAllocMgr;

        vector<LwLinkPowerState::Owner> m_PowerStateOwners;

        //! JS variables (see JS interfaces for description)
        UINT32  m_NumErrorsToPrint;
        FLOAT64 m_BwThresholdPct;
        UINT32  m_TransferDirMask;
        UINT32  m_TransferTypeMask;
        UINT32  m_MaxSimulRoutes;
        UINT32  m_MinSimulRoutes;
        UINT32  m_LwLinkIdMask;
        bool    m_ShowTestModes;
        bool    m_ShowBandwidthData;
        UINT32  m_CopyLoopCount;
        bool    m_DumpSurfaceOnMiscompare;
        bool    m_SkipBandwidthCheck;
        bool    m_SkipSurfaceCheck;
        bool    m_SysmemLoopback;
        UINT32  m_PauseTestMask;
        UINT32  m_PerModeRuntimeSec;
        UINT32  m_RuntimeMs;
        bool    m_TestAllGpus;
        bool    m_ShowTestProgress;
        bool    m_SkipLinkStateCheck;
        bool    m_FailOnSysmemBw;
        bool    m_AddCpuTraffic;
        UINT32  m_NumCpuTrafficThreads;
        bool    m_TogglePowerState;
        FLOAT64 m_TogglePowerStateIntMs;
        UINT32  m_LPCountCheckTol;
        FLOAT64 m_LPCountCheckTolPct;
        bool    m_SkipLPCountCheck;
        UINT32  m_MinLPCount;
        bool    m_LowPowerBandwidthCheck;
        bool    m_CheckHwPowerStateSwitch;
        bool    m_ShowLowPowerCheck;
        UINT32  m_IdleAfterCopyMs;
        bool    m_ThermalThrottling;
        UINT32  m_ThermalThrottlingOnCount;
        UINT32  m_ThermalThrottlingOffCount;
        bool    m_ThermalSlowdown;
        UINT32  m_ThermalSlowdownPeriodUs;
        UINT32  m_LoopbackTestType;
        bool    m_ShowPortStats;
        bool    m_SkipPortStatsCheck;
        UINT32  m_LowLatencyThresholdNs;
        UINT32  m_MidLatencyThresholdNs;
        UINT32  m_HighLatencyThresholdNs;
        bool    m_IsBgTest;
        bool    m_KeepRunning;
        bool    m_L2AfterCopy = false;
        double  m_L2DurationMs = 100;
        UINT32  m_GCxInL2 = GCxPwrWait::GCxModeDisabled;
        UINT32  m_EnableUphyLogging = false;

        // Data to support testing of a set of source and sink devices specified
        // by a user. The primary use is for maximum traffic solutions found by
        // ptotoutil. The solution lists pairs of source and sink devices for
        // the case of response and request types of traffic. Besides pairs of
        // source and sink devices, there is an additional parameter for request
        // type: address region number. It is lwrrently not in use and is always
        // zero.
        typedef tuple<
            TestDevicePtr // The sink device, i.e. where the data flows to. It is the first one,
                          // because in the response traffic this device reads data. It will be used
                          // in SetupDataFlowRoutes to create a test route.
          , TestDevicePtr // The source device, i.e. where the data is from.
          > RespSrcSinkDevs;
        typedef tuple<
            TestDevicePtr // The source device, i.e. where the data is from.  It is the first one,
                          // because in the request traffic this device writes data. It will be used
                          // in SetupDataFlowRoutes to create a test route.
          , TestDevicePtr // The sink device, i.e. where the data flows to.
          , unsigned int // The address region number. Not used for now.
          > ReqSrcSinkDevs;
        typedef vector<RespSrcSinkDevs> RespDataFlow;
        typedef vector<ReqSrcSinkDevs> ReqDataFlow;
        JSObject    *m_DataFlow = nullptr;     // An object passed from the JavaScript code with
                                               // the list of sources and sinks.
        bool         m_UseDataFlowObj = false; // A flag that the JavaScript code sets if the data
                                               // flow object was set either in the spec file or on
                                               // command line.
        string       m_DataFlowFile;           // The name of a file with JSON definition of the
                                               // traffic flow. Note that MODS parses test arguments
                                               // as JS expressions, therefore the filename has to
                                               // contain quotes after being processed by the
                                               // command line processor. For example: -testarg 246
                                               // DataFlowfile '\"explorer-optimal.json\"'.
        bool         m_UseOptimal = false;     // The test will try to find a JSON file with the
                                               // traffic definition by adding -optimal.json to the
                                               // topology filename without .topo[.bin] extentions.
                                               // The rule for colwerting the topology file name to
                                               // a JSON file name is inside
                                               // `SetupLwLinkDataTestCommon` in gpudecls.js.
        RespDataFlow m_RespDataFlow;           // a list of the response traffic pairs
        ReqDataFlow  m_ReqDataFlow;            // a list of the request traffic pairs
        vector<
            LwLinkDataTestHelper::TestRoutePtr
          > m_RespTestRoutes;                  // test routes for the response traffic
        vector<
            LwLinkDataTestHelper::TestRoutePtr // test routes for the request traffic
          > m_ReqTestRoutes;

        // a helper method that copies the JS data from `m_DataFlow` to
        // `m_RespDataFlow` and `m_ReqDataFlow`
        RC ReadDataFlowFromJSObj();

        // The `LwLinkDataTest` class is constructed by the JavaScript engine
        // while creating the correspondent JS test object. In order to setup
        // `LwLinkDataTest::m_DataFlow` we need a callback to get to the moment
        // when the JS constructor finishes.
        class JSConstructHook
          : public JSHookBase<JSConstructHook, JS_SetCallHookType, JS_SetCallHook>
        {
            typedef JSHookBase<JSConstructHook, JS_SetCallHookType, JS_SetCallHook> Base;

            friend class JSHookAccess;
        public:
            static constexpr bool HasInit = false;
            static constexpr bool HasShutDown = false;

            JSConstructHook(JSRuntime *rt, LwLinkDataTest *This)
              : Base(rt)
              , m_This(This)
            {}

            void *Hook(JSContext *cx, JSStackFrame *fp, JSBool before, JSBool *ok);

        private:
            LwLinkDataTest *m_This;
        };
        JSConstructHook m_JSConstructHook;

        void FilterTestRoutes(vector<LwLinkRoutePtr> *pRoutes);
        RC CleanupTestRoutes();
        RC SetupTestRoutes();
        RC SetupTestRoutesOnGpu
        (
            TestDevicePtr pDev,
            vector<LwLinkRoutePtr> *pRoutes,
            map< TestDevice*, vector<FLOAT32> > *pLinkUsage
        );
        bool AreRouteConnectionsUniform(LwLinkRoutePtr pRoute);

        //! \brief Setup routes using sources and sinks extracted from m_DataFlow
        template <typename SrcSinkList, typename RoutesList, typename OutIterator>
        RC SetupDataFlowRoutes
        (
            const SrcSinkList &srcSinkList,
            const RoutesList &routes,
            OutIterator outIt
        );
        RC SetupDataFlowTestModes();
        RC SetupTestModes();
        void ShowTestMode(size_t modeIdx, Tee::Priority pri, bool *pbTestModeShown);
    };
};

extern SObject LwLinkDataTest_Object;
#endif
