/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2015-2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#pragma once

#ifndef INCLUDED_LWLDT_TEST_MODE_H
#define INCLUDED_LWLDT_TEST_MODE_H

#include <atomic>

#include "core/include/fpicker.h"
#include "core/include/lwrm.h"
#include "core/include/rc.h"
#include "core/include/tasker.h"
#include "core/include/tee.h"
#include "core/include/types.h"
#include "device/interface/lwlink/lwllatencybincounters.h"
#include "device/interface/lwlink/lwlpowerstate.h"
#include "device/interface/lwlink/lwlpowerstatecounters.h"
#include "device/interface/lwlink/lwlsleepstate.h"
#include "gpu/lwlink/lwlinkimpl.h"
#include "gpu/perf/pwrwait.h"
#include "gpu/utility/lwlink/lwlinkdev.h"
#include "lwldt_alloc_mgr.h"
#include "lwldt_priv.h"

#include <vector>
#include <string>
#include <cstring>
#include <memory>

class GpuTestConfiguration;

namespace LwLinkDataTestHelper
{
    class TestRoute;
    typedef shared_ptr<TestRoute> TestRoutePtr;

    //--------------------------------------------------------------------------
    //! \brief Class which encapsulates a test mode for the LwLink bandwidth
    //! stress test.  A test mode consists of a set of LwLink routes and
    //! the transfer directions being tested (In, Out, or In/Out).  All LwLink
    //! routes have an endpoint on the GpuSubdevice under test (referred to
    //! as "dut"). When transfer direction is referenced it is always with
    //! respect do the "dut" (e.g. "In" means data is flowing into the "dut")
    //!
    //! Within this class "local" means a resource/class/etc. referencing the
    //! "dut".  "remote" means a resource/class/etc. other than the "dut"
    //!
    //! A test mode also provides functionality for verifying the integrity of
    //! the transferred data as well as the bandwidth achieved
    class TestMode
    {
    public:
        enum
        {
             PAUSE_BEFORE_COPIES = (1 << 0)
            ,PAUSE_AFTER_COPIES = (1 << 1)
            ,PAUSE_NONE = 0
            ,ALL_COMPONENTS = ~0U
        };
        TestMode
        (
            GpuTestConfiguration *pTestConfig
           ,AllocMgrPtr pAllocMgr
           ,UINT32 numErrorsToPrint
           ,UINT32 copyLoopCount
           ,FLOAT64 bwThresholdPct
           ,bool bShowBandwidthStats
           ,UINT32 pauseMask
           ,Tee::Priority pri
        );
        struct BandwidthInfo
        {
            INT32   srcDevId;
            INT32   remoteDevId;
            UINT64  srcLinkMask;
            UINT64  remoteLinkMask;
            LwLink::TransferType transferType;
            FLOAT32 bandwidth;
            FLOAT32 threshold;
            bool    tooLow;
        };

        struct LPCounter
        {
            TestDevicePtr pDev;
            UINT32 linkId = 0;
            UINT32 count = 0;
            bool overflow = false;

            LPCounter() = default;
            LPCounter(TestDevicePtr pDev, UINT32 linkId, UINT32 count)
                : pDev(pDev), linkId(linkId), count(count) {}
            bool operator<(const LPCounter& rhs) const
            {
                return pDev->GetId() < rhs.pDev->GetId() || linkId < rhs.linkId;
            }
        };

        virtual ~TestMode();
        virtual RC AcquireResources();
        RC CheckBandwidth(vector<BandwidthInfo>* pBwFailureInfos);
        RC CheckLPCount(UINT32 tol, FLOAT64 pctTol);
        RC CheckLinkState();
        RC CheckSurfaces();
        RC DoOneCopy(FLOAT64 timeoutMs);

        bool GetAddCpuTraffic() { return m_bAddCpuTraffic; }
        UINT32 GetNumCpuTrafficThreads() { return m_NumCpuTrafficThreads; }
        UINT32 GetCopyLoopCount() { return m_CopyLoopCount; }
        UINT32 GetIdleAfterCopyMs() const { return m_IdleAfterCopyMs; }
        bool GetInsertIdleAfterCopy() const { return m_InsertIdleAfterCopy; }
        bool GetInsertL2AfterCopy() const { return m_InsertL2AfterCopy; }
        double GetL2IntervalMs() const { return m_L2IntervalMs; }
        bool GetLoopbackInput() { return m_bLoopbackInput; }
        bool GetMakeLowPowerCopies() const { return m_MakeLowPowerCopies; }
        UINT32 GetNumErrorsToPrint() { return m_NumErrorsToPrint; }
        UINT32 GetNumRoutes() { return static_cast<UINT32>(m_TestData.size()); }
        bool GetPowerStateToggle() const { return m_PowerStateToggle; }
        bool GetShowLowPowerCheck() const { return m_ShowLowPowerCheck; }
        FLOAT64 GetPSToggleIntervalMs() const { return m_PSToggleIntervalUs / 1000.0; }
        UINT32 GetDoOneCopyCount() const { return m_DoOneCopyCount; }
        UINT32 GetSwLPEntryCount() const { return m_SwLPEntryCount; }
        UINT32 GetMinLPCount() const { return m_MinLPCount; }
        TransferDir GetTransferDir() { return m_TransferDir; }
        RC UpdateLPEntryOrExitCount(bool entry, list<LPCounter>& counters);
        bool GetShowCopyProgress() { return m_bShowCopyProgress; }
        bool GetSysmemLoopback() { return m_bSysmemLoopback; }
        TransferDir GetSysmemTransferDir() const { return m_SysmemTransferDir; }
        UINT32 GetTransferTypes() { return m_TransferTypes; }
        bool GetThermalThrottling() { return m_ThermalThrottling; }
        UINT32 GetThermalThrottlingOnCount() { return m_ThermalThrottlingOnCount; }
        UINT32 GetThermalThrottlingOffCount() { return m_ThermalThrottlingOffCount; }
        bool GetThermalSlowdown() { return m_ThermalSlowdown; }
        UINT32 GetThermalSlowdownPeriodUs() { return m_ThermalSlowdownPeriodUs; }

        bool HasLoopback();
        virtual RC Initialize(const vector<TestRoutePtr> &routes, UINT32 routeMask);
        virtual bool OnlyLoopback();
        virtual RC ReleaseResources();
        RC RequestPowerState(LwLinkPowerState::SubLinkPowerState state, bool bWaitState);
        RC RequestSleepState(LwLinkSleepState::SleepState state, bool waitForState);
        RC ClearLPCounts();

        void SetCopyLoopCount(UINT32 val) { m_CopyLoopCount = val; }
        void SetIdleAfterCopyMs(UINT32 val) { m_IdleAfterCopyMs = val; }
        void SetInsertIdleAfterCopy(bool val) { m_InsertIdleAfterCopy = val; }
        void SetInsertL2AfterCopy(bool insertL2AfterCopy) { m_InsertL2AfterCopy = insertL2AfterCopy; } //$
        void SetL2IntervalMs(double l2IntervalMs) { m_L2IntervalMs = l2IntervalMs; }
        void SetGCxBubble(unique_ptr<GCxBubble> gcxBubble) { m_GCxBubble = move(gcxBubble); }
        void SetMakeLowPowerCopies(bool val) { m_MakeLowPowerCopies = val; }
        void SetDoOneCopyCount(UINT32 val) { m_DoOneCopyCount = val; }
        void SetSwLPEntryCount(UINT32 val) { m_SwLPEntryCount = val; }
        void SetMinLPCount(UINT32 val) { m_MinLPCount = val; }
        void SetTransferDir(TransferDir td) { m_TransferDir = td; }
        void SetSysmemTransferDir(TransferDir td) { m_SysmemTransferDir = td; }
        void SetShowCopyProgress(bool bShowCopyProgress) { m_bShowCopyProgress = bShowCopyProgress; } //$
        void SetSysmemLoopback(bool bSysmemLoopback) { m_bSysmemLoopback = bSysmemLoopback; }
        void SetLoopbackInput(bool bLoopbackInput) { m_bLoopbackInput = bLoopbackInput; }
        void SetFailOnSysmemBw(bool bFailOnSysmemBw) { m_bFailOnSysmemBw = bFailOnSysmemBw; }
        void SetAddCpuTraffic(bool bAddCpuTraffic) { m_bAddCpuTraffic = bAddCpuTraffic; }
        void SetNumCpuTrafficThreads(UINT32 numThreads) { m_NumCpuTrafficThreads = numThreads; }
        void SetPowerStateToggle(bool val) { m_PowerStateToggle = val; }
        void SetShowLowPowerCheck(bool val) { m_ShowLowPowerCheck = val; }
        void SetPSToggleIntervalMs(FLOAT64 val) { m_PSToggleIntervalUs = static_cast<UINT32>(1000 * val); }
        void SetThermalThrottling(bool val) { m_ThermalThrottling = val; }
        void SetThermalThrottlingOnCount(UINT32 val) { m_ThermalThrottlingOnCount = val; }
        void SetThermalThrottlingOffCount(UINT32 val) { m_ThermalThrottlingOffCount = val; }
        void SetThermalSlowdown(bool val) { m_ThermalSlowdown = val; }
        void SetThermalSlowdownPeriodUs(UINT32 val) { m_ThermalSlowdownPeriodUs = val; }
        void SetLatencyBins(UINT32 low, UINT32 mid, UINT32 high);
        virtual RC ShowPortStats();
        virtual RC CheckPortStats();
        virtual RC StartPortStats();
        virtual RC StopPortStats();

        virtual RC DumpSurfaces(string fnamePrefix) = 0;
        virtual void Print(Tee::Priority pri, string prefix) = 0;

        template <typename TxFun, typename RxFun>
        RC ForEachLink(TxFun &&tx, RxFun &&rx)
        {
            RC rc;

            for (auto &td : m_TestData)
            {
                CHECK_RC(td.second.pDutInTestData->ForEachLink(
                    *td.first, forward<TxFun>(tx), forward<RxFun>(rx)
                ));
                CHECK_RC(td.second.pDutOutTestData->ForEachLink(
                    *td.first, forward<TxFun>(tx), forward<RxFun>(rx)
                ));
            }

            return rc;
        }

        template <typename Fun>
        RC ForEachDevice(Fun &&f)
        {
            RC rc;

            for (auto &td : m_TestData)
            {
                CHECK_RC(td.second.pDutInTestData->ForEachDevice(*td.first, forward<Fun>(f)));
                CHECK_RC(td.second.pDutOutTestData->ForEachDevice(*td.first, forward<Fun>(f)));
            }

            return rc;
        }

        template <typename Fun>
        RC ForEachPathEntry(Fun &&f)
        {
            RC rc;

            for (auto &td : m_TestData)
            {
                CHECK_RC(td.second.pDutInTestData->ForEachPathEntry(*td.first, forward<Fun>(f)));
                CHECK_RC(td.second.pDutOutTestData->ForEachPathEntry(*td.first, forward<Fun>(f)));
            }

            return rc;
        }

        static void RunCpuTrafficThread(void* args);
    protected:
        //! Structure representing a test of a particular direction on a route.
        class TestDirectionData
        {
        public:
            struct BgCopyData
            {
                volatile INT32     running     = 0;                   //!< This test direction is lwrrently being          //$
                                                                      //!< copied in the background                        //$
                Tasker::ThreadID   threadID    = Tasker::NULL_THREAD; //!< The threadId of the background copying thread   //$
                Tasker::ThreadFunc copyFunc;                          //!< The function to do the copying                  //$
                vector<UINT32>     localCpus;                         //!< The CPUs local the GPU running copyFunc         //$
                Surface2DPtr       pSrcSurf;                          //!< Pointer to source surface                       //$
                Surface2DPtr       pDstSurf;                          //!< Pointer to destination surface                  //$
                void*              pSrcAddr    = nullptr;             //!< Source surface address
                void*              pDstAddr    = nullptr;             //!< Destination surface address                     //$
                UINT64             totalBytes  = 0LLU;                //!< Total bytes copied so far in this direction     //$
                UINT64             totalTimeNs = 0LLU;                //!< Total time in Ns for all copies                 //$
            };
            TestDirectionData(AllocMgrPtr pAllocMgr) : m_pAllocMgr(pAllocMgr) { }
            virtual ~TestDirectionData()  { m_BgCopies.clear(); }

            void AddCopyBytes(UINT64 bytes) { m_TotalBytes += bytes; }
            void AddTimeNs(UINT64 timeNs) { m_TotalTimeNs += timeNs; }
            bool IsHwLocal() const { return m_bHwLocal; }
            bool IsInput() const { return m_bDataIn; }
            virtual bool IsInUse() const { return !m_BgCopies.empty(); }

            UINT32 GetMaxDataBwKiBps() const { return m_MaxDataBwKiBps; }
            UINT64 GetTotalBytes() const { return m_TotalBytes; }
            UINT64 GetTotalTimeNs() const { return m_TotalTimeNs; }
            FLOAT64 GetPacketEfficiency() const { return m_PacketEfficiency; }
            LwLinkImpl::HwType GetTransferHw() const { return m_TransferHw; }
            bool GetUseDmaInit() { return m_pAllocMgr->GetUseDmaInit(); }
            UINT32 GetXbarBwKiBps() { return m_XbarBwKiBps; }

            virtual RC Release(bool *pbAnyReleased);
            virtual void Reset();

            void SetAddCpuTraffic(bool val) { m_bAddCpuTraffic = val; }
            void SetMaxDataBwKiBps(UINT32 bw) { m_MaxDataBwKiBps = bw; }
            void SetNumCpuTrafficThreads(UINT32 val) { m_NumCpuTrafficThreads = val; }
            void SetHwLocal(bool bLocal) { m_bHwLocal = bLocal; }
            void SetInput(bool bInput) { m_bDataIn = bInput; }
            void SetSysmemLoopback(bool val) { m_bSysmemLoopback = val; }
            void SetTotalBytes(UINT64 bytes) { m_TotalBytes = bytes; }
            void SetTotalTimeNs(UINT64 ttns) { m_TotalTimeNs = ttns; }
            void SetPacketEfficiency(FLOAT64 eff) { m_PacketEfficiency = eff; }
            void SetTransferHw(LwLinkImpl::HwType thw) { m_TransferHw = thw; }
            virtual RC SetupTestSurfaces(GpuTestConfiguration *pTestConfig);
            void SetXbarBwKiBps(UINT32 xbbw) { m_XbarBwKiBps = xbbw; }
            virtual RC StartCopies();
            virtual RC StopCopies();

            virtual bool AreBytesCopied() const = 0;
            bool GetAddCpuTraffic() { return m_bAddCpuTraffic; }
            UINT32 GetNumCpuTrafficThreads() { return m_NumCpuTrafficThreads; }
            virtual UINT64 GetBytesPerLoop() const = 0;
            virtual string GetBandwidthFailString() const = 0;
            bool GetSysmemLoopback() { return m_bSysmemLoopback; }
            virtual UINT32 GetTransferWidth() const = 0;
            virtual RC UpdateCopyStats(UINT32 numCopies) = 0;

        private:
            // This overloaded function will be called from ForEachLink if Fun
            // can be called with 4 arguments. Otherwise the declspec in the
            // last argument won't have sense and the version below will be
            // discarded by SFINAE.
            template <typename Fun>
            static
            RC IlwokeLinkFun
            (
                Fun &&f
              , TestDevicePtr orgDev
              , UINT32 orgLinkId
              , TestDevicePtr remDev
              , UINT32 remLinkId
              , decltype(declval<Fun>()(orgDev, orgLinkId, remDev, remLinkId)) * = nullptr
            )
            {
                return forward<Fun>(f)(orgDev, orgLinkId, remDev, remLinkId);
            }

            // This overloaded function will be called from ForEachLink if Fun
            // can be called with 2 arguments. Otherwise the declspec in the
            // last argument won't have sense and the version below will be
            // discarded by SFINAE.
            template <typename Fun>
            static
            RC IlwokeLinkFun
            (
                Fun &&f
              , TestDevicePtr orgDev
              , UINT32 orgLinkId
              , TestDevicePtr remDev
              , UINT32 remLinkId
              , decltype(declval<Fun>()(orgDev, orgLinkId)) * = nullptr
            )
            {
                return forward<Fun>(f)(orgDev, orgLinkId);
            }

            // This overloaded function will be called if the user decided to
            // skip ilwoking a function for this sublink and passed nullptr.
            static
            RC IlwokeLinkFun
            (
                nullptr_t nullp
              , TestDevicePtr orgDev
              , UINT32 orgLinkId
              , TestDevicePtr remDev
              , UINT32 remLinkId
              , void * = nullptr
            )
            {
                return OK;
            }

        public:
            // Comments to the way we iterate devices in ForEachLink below. The
            // way the data flows between devices depends on two variables:
            // direction and the type of packets inside a transaction. For the
            // purpose of MODS there are two important types of packets: REQUEST
            // and RESPONSE. A REQUEST is a packet that is used to "request"
            // work at the destination. A RESPONSE is returned to a request. A
            // device can be either written or read using either REQUEST or
            // RESPONSE. This gives us 4 different combinations of routing of
            // the traffic. A diagram below shows these combinations:
            // +------+  <---  +------+ Data in, RESPONSE. GPU0 sends a request
            // |    rx+--------+tx    | for data from GPU1, using a copy engine
            // | GPU0 |        | GPU1 | (CE) on its side. GPU1 responds. Data is
            // |    tx+--------+rx    | written to GPU0 using a RESPONSE route
            // +------+        +------+ from GPU1 to GPU0.
            // |  CE  |                 IsHwLocal() == true, IsInput() == true
            // +------+
            //
            // +------+  <---  +------+ Data in, REQUEST. GPU1 sends a request
            // |    rx+--------+tx    | to write data to GPU0. Data is written
            // | GPU0 |        | GPU1 | to GPU0 using a REQUEST route.
            // |    tx+--------+rx    |
            // +------+        +------+
            //                 |  CE  | IsHwLocal() == false, IsInput() == true
            //                 +------+
            //
            // +------+        +------+ Data out, RESPONSE. GPU1 sends a request
            // |    rx+--------+tx    | for data from GPU0, using a copy engine
            // | GPU0 |        | GPU1 | (CE) on its side. GPU0 responds. Data is
            // |    tx+--------+rx    | written to GPU1 using a RESPONSE route
            // +------+  --->  +------+ from GPU0 to GPU1.
            //                 |  CE  | IsHwLocal() == false, IsInput() == false
            //                 +------+
            //
            // +------+        +------+ Data out, REQUEST. GPU0 sends a request
            // |    rx+--------+tx    | to write data to GPU1. Data is written
            // | GPU0 |        | GPU1 | to GPU1 using a REQUEST route.
            // |    tx+--------+rx    |
            // +------+  --->  +------+
            // |  CE  |                 IsHwLocal() == true, IsInput() == false
            // +------+
            template <typename Route, typename TxFun, typename RxFun>
            RC ForEachLink(Route &tr, TxFun &&tx, RxFun &&rx)
            {
                RC rc;
                if (!IsInUse())
                    return rc;

                // Who initiates traffic using REQUEST/RESPONSE? According to
                // the diagram above, if data flows in, it's the remote device.
                // Note that this doesn't correspond to the location of the CE,
                // because in the case of RESPONSE the response is from the
                // other side of the CE.
                TestDevicePtr origin = IsInput() ? tr.GetRemoteLwLinkDev() : tr.GetLocalLwLinkDev();

                // According to the diagram above RESPONSE is when it is either
                // input and the CE is at the local end or it is output and the
                // CE is at the remote end.
                auto dataType = IsInput() == IsHwLocal() ? LwLink::DT_RESPONSE :
                                                           LwLink::DT_REQUEST;

                auto pathIt = tr.PathsBegin(origin, dataType);
                auto pathEnd = tr.PathsEnd(origin, dataType);
                for (; pathEnd != pathIt; ++pathIt)
                {
                    auto peIt = pathIt->PathEntryBegin(origin);
                    auto peEnd = LwLinkPath::PathEntryIterator();
                    for (; peEnd != peIt; ++peIt)
                    {
                        auto con = peIt->pCon;
                        // the path iterator always makes sure that srcDev is
                        // the transmitter
                        auto srcDev = peIt->pSrcDev;
                        // and the remote device is the receiver
                        auto remoteDev = con->GetRemoteDevice(srcDev);
                        for (const auto &eid : con->GetLinks(srcDev))
                        {
                            // If bRemote is true it means that both ends of the
                            // connection are on the same device and data is flowing
                            // from the second endpoint to the first. Usually we
                            // always get the transmitter's link id in the first
                            // member of the pair that GetLinks returns. It is
                            // possible, because GetLinks has an extra argument
                            // of the source device and link ids are arranged
                            // accordingly. But in the case when both devices
                            // are the same, we need an extra flag bRemote that
                            // indicates how link ids are arranged. If it is
                            // true the first link is the receiver, the second is
                            // the transmitter.
                            auto txLinkId = peIt->bRemote ? eid.second : eid.first;
                            auto rxLinkId = peIt->bRemote ? eid.first : eid.second;

                            CHECK_RC(IlwokeLinkFun(
                                forward<TxFun>(tx),
                                srcDev, txLinkId, remoteDev, rxLinkId
                            ));
                            CHECK_RC(IlwokeLinkFun(
                                forward<RxFun>(rx),
                                remoteDev, rxLinkId, srcDev, txLinkId
                            ));
                            // Loopout links in the connection need both RX/TX to be called
                            // on both ends
                            if ((eid.second != eid.first) && (srcDev == remoteDev))
                            {
                                CHECK_RC(IlwokeLinkFun(
                                    forward<TxFun>(tx),
                                    srcDev, rxLinkId, remoteDev, txLinkId
                                ));
                                CHECK_RC(IlwokeLinkFun(
                                    forward<RxFun>(rx),
                                    remoteDev, txLinkId, srcDev, rxLinkId
                                ));
                            }
                        }
                    }
                }
                return rc;
            }

            template <typename Route, typename Fun>
            RC ForEachDevice(Route &tr, Fun &&f)
            {
                RC rc;
                if (!IsInUse())
                    return rc;

                TestDevicePtr origin = IsInput() ? tr.GetRemoteLwLinkDev() : tr.GetLocalLwLinkDev();
                auto dataType = IsInput() == IsHwLocal() ? LwLink::DT_RESPONSE :
                                                           LwLink::DT_REQUEST;
                auto pathIt = tr.PathsBegin(origin, dataType);
                auto pathEnd = tr.PathsEnd(origin, dataType);
                for (; pathEnd != pathIt; ++pathIt)
                {
                    auto peIt = pathIt->PathEntryBegin(origin);
                    auto peEnd = LwLinkPath::PathEntryIterator();
                    for (; peEnd != peIt; ++peIt)
                    {
                        auto con = peIt->pCon;
                        auto srcDev = peIt->pSrcDev;
                        auto remoteDev = con->GetRemoteDevice(srcDev);
                        CHECK_RC(forward<Fun>(f)(srcDev));
                        CHECK_RC(forward<Fun>(f)(remoteDev));
                    }
                }
                return rc;
            }

            template <typename Route, typename Fun>
            RC ForEachPathEntry(Route &tr, Fun &&f)
            {
                RC rc;
                if (!IsInUse())
                    return rc;

                TestDevicePtr origin = IsInput() ? tr.GetRemoteLwLinkDev() : tr.GetLocalLwLinkDev();
                auto dataType = IsInput() == IsHwLocal() ? LwLink::DT_RESPONSE :
                                                           LwLink::DT_REQUEST;
                auto pathIt = tr.PathsBegin(origin, dataType);
                auto pathEnd = tr.PathsEnd(origin, dataType);
                for (; pathEnd != pathIt; ++pathIt)
                {
                    auto peIt = pathIt->PathEntryBegin(origin);
                    auto peEnd = LwLinkPath::PathEntryIterator();
                    for (; peEnd != peIt; ++peIt)
                    {
                        CHECK_RC(forward<Fun>(f)(*peIt));
                    }
                }
                return rc;
            }

        protected:
            RC Acquire
            (
                TransferType tt
               ,GpuDevice *pDev
               ,GpuTestConfiguration *pTestConfig
            );
            RC MapSurface
            (
                Surface2D*    pSurf,
                void**        pOutAddr,
                GpuSubdevice* pSubdev
            );
            void UnmapSurface
            (
                Surface2D* pSurf,
                void**     pAddr
            );

        private:
            AllocMgrPtr        m_pAllocMgr;
            bool               m_bDataIn     = false;    //!< Transfer direction (input or
                                                         //!< output)
            bool               m_bHwLocal    = true;     //!< Using DUT local hardware
            LwLinkImpl::HwType m_TransferHw  = LwLinkImpl::HT_NONE; //!< Type of hardware
                                                                    //!< used for transfers
            UINT64             m_TotalBytes  = 0LLU;     //!< Total bytes copied so far in
                                                         //!< this direction
            UINT64             m_TotalTimeNs = 0LLU;     //!< Total time in Ns for all
                                                         //!< copies
            UINT32             m_MaxDataBwKiBps = 0U;
            UINT32             m_XbarBwKiBps    = 0U;
            FLOAT64            m_PacketEfficiency = 0;
            vector<BgCopyData> m_BgCopies;

            bool               m_bAddCpuTraffic  = false;  //!< Add cpu traffic
            UINT32             m_NumCpuTrafficThreads = 0; //!< Number of simultaneous cpu traffic threads //$
            bool               m_bSysmemLoopback = false;  //!< Testing system memory with sys<->sys //$
        };
        typedef shared_ptr<TestDirectionData> TestDirectionDataPtr;

        struct TestData
        {
            TestDirectionDataPtr pDutInTestData;
            TestDirectionDataPtr pDutOutTestData;
        };

        bool ContainsLwSwitch(TestRoute &route, TestDirectionData &testDir);
        AllocMgrPtr GetAllocMgr() { return m_pAllocMgr; }
        Tee::Priority GetPrintPri() { return m_PrintPri; }
        UINT64 GetProgressPrintTick() { return m_ProgressPrintTick; }
        GpuTestConfiguration * GetTestConfig() { return m_pTestConfig; }
        map<TestRoutePtr, TestData> & GetTestData() { return m_TestData; }
        virtual void ResetTestData();
        void SetProgressPrintTick(UINT64 tick) { m_ProgressPrintTick = tick; }
        virtual bool AllowThroughputCounterPortStats() const { return true; }
        virtual string GetBandwidthFailStringImpl
        (
            TestRoutePtr pRoute,
            TestDirectionDataPtr pTestDir
        ) { return ""; }

    private:
        GpuTestConfiguration * m_pTestConfig;      //!< Test configuration
        TestDevicePtr          m_pLocLwLinkDev;    //!< Pointer to the LwLink device of the DUT
        TransferDir            m_TransferDir;      //!< Transfer direction of the test mode
        TransferDir            m_SysmemTransferDir;//!< Transfer direction of sysmem links
        UINT32                 m_TransferTypes;    //!< Mask of all transfer types of the test
                                                   //!< mode
        UINT32                 m_NumErrorsToPrint; //!< When errors occur during surface check
                                                   //!< this limits the number of errors to
                                                   //!< print before halting prints (0 = print
                                                   //!< all)
        UINT32                 m_CopyLoopCount;    //!< Number of copies loops performed within
                                                   //!< DoOneCopy
        FLOAT64                m_BwThresholdPct;   //!< The percent of the theoretical
                                                   //!< bandwidth that must be obtained to pass
        bool                   m_bSysmemLoopback;  //!< Testing system memory with sys<->sys
                                                   //!< transfers
        bool                   m_bLoopbackInput;   //!< Direction of transfers for loopback
                                                   //!< routes true = reads through
                                                   //!< lwlink, false = writes
        bool                   m_bShowBandwidthStats;  //!< True to show bandwidth statistics
                                                       //!< regardless of pass/fail
        bool                   m_bFailOnSysmemBw;  //!< True if sysmem bandwidth failures
                                                   //!< should also cause test failures
        bool                   m_bShowCopyProgress;//!< true to show progress for copies
        UINT64                 m_ProgressPrintTick;//!< The last time that a progress indicator
                                                   //!< was printed
        UINT32                 m_PauseMask;        //!< Mask of pause points for user input
        Tee::Priority          m_PrintPri;         //!< Priority to print informational message
        bool                   m_bAddCpuTraffic;   //!< Add CPU generated traffic to run
                                                   //!< simultaneously with GPU transfers
        UINT32                 m_NumCpuTrafficThreads; //!< Number of simultaneous CPU traffic
                                                       //!< threads to run with AddCpuTraffic

        bool                   m_PowerStateToggle;   //!< start a thread that switches LP/FB
                                                     //!< during transfers
        bool                   m_ShowLowPowerCheck;  //!< record and show the LP/FB switches
                                                     //!< without explicityly causing them
        Tasker::ThreadID       m_PSToggleThreadId;   //!< id of the thread that switches LP/FB
        UINT64                 m_PSToggleStartTime;
        atomic_bool            m_PSToggleThreadCont; //!< conditional to signal the thread to
                                                     //!< stop
        UINT32                 m_PSToggleIntervalUs;
        UINT32                 m_PSToggleTimeout;    //!< timeout on waiting the toggle thread
                                                     //!< to finish
        UINT32                 m_DoOneCopyCount;     //! a counter how many times DoOneCopy
                                                     //! was called
        UINT32                 m_SwLPEntryCount;     //!< the number of times the toggle thread
                                                     //!< switched links to LP
        UINT32                 m_MinLPCount;         //!< overrides the check for the LP entry
                                                     //!< counters, ignored if 0

        bool                   m_MakeLowPowerCopies; //!< switch to LP statically for data
                                                     //!< transfer, check in the end that the
                                                     //!< bandwidth was 1/8 as expected in FB
        bool                   m_InsertIdleAfterCopy;
        bool                   m_InsertL2AfterCopy = false; //!< Power off and on the links between copies
        double                 m_L2IntervalMs = 0;          //!< Time interval for keeping links off in L2
        unique_ptr<GCxBubble>  m_GCxBubble;                 //!< Enter to GCx during L2
        UINT32                 m_IdleAfterCopyMs;
        list<LPCounter>        m_LastHwLPCounters;
        bool                   m_ThermalThrottling;  //!< Execute thermal throttling for data
                                                     //!< transfer, check in the end that the
                                                     //!< bandwidth is reduced
        UINT32                 m_ThermalThrottlingOnCount;  //!< Number of sysclk cycles to
                                                            //!< simulate overtemp
        UINT32                 m_ThermalThrottlingOffCount; //!< Number of sysclk cycles to
                                                            //!< disable overtemp simulation
        bool                   m_ThermalSlowdown;   //!< Execute thermal slowdown for data transfer
        UINT32                 m_ThermalSlowdownPeriodUs; //!< Mimimum number of microseconds to
                                                          //!< remain in thermal slowdown once triggered
        UINT64                 m_ThermalSlowdownStartTime;
        AllocMgrPtr            m_pAllocMgr;         //!< Allocation manager for managing
                                                    //!< resources
        LwLinkLatencyBinCounters::LatencyBins m_LatencyBins;
        map<TestRoutePtr, TestData> m_TestData;     //!< Per route background copy data

        RC CallwlateConnectionThroughput();
        RC CallwlateXbarBandwidth();
        RC CheckBandwidth
        (
            TestRoutePtr pRoute
           ,TestDirectionDataPtr pTestDir
           ,LwLinkImpl::HwType otherDirHw
           ,bool bOtherHwLocal
           ,vector<BandwidthInfo>* pBwFailureInfos
        );
        RC GetConnectionEfficiency
        (
            TestRoutePtr pTestRoute,
            LwLinkConnectionPtr pCon,
            TestDevicePtr pConSrcDev,
            FLOAT64   readDataFlits,
            FLOAT64   writeDataFlits,
            FLOAT64   totalReadFlits,
            FLOAT64   totalWriteFlits,
            FLOAT64 * pPacketEfficiency
        ) const;
        void Pause(UINT32 pausePoint);
        void PowerStateToggleThread();
        RC StartPowerStateToggle(FLOAT64 timeoutMs);
        RC StartThermalThrottling();
        RC StartThermalSlowdownToggle();
        RC StopPowerStateToggle();
        RC StopThermalThrottling();
        RC StopThermalSlowdownToggle();

        virtual RC CheckDestinationSurface
        (
            TestRoutePtr pRoute
           ,TestDirectionDataPtr pTestDir
        ) = 0;
        virtual RC SetupCopies(TestDirectionDataPtr pTestDir) = 0;
        virtual RC TriggerAllCopies() = 0;
        virtual RC WaitForCopies(FLOAT64 timeoutMs) = 0;
        virtual TestDirectionDataPtr CreateTestDirectionData() = 0;
        virtual UINT64 GetMinimumBytesPerCopy(UINT32 maxBwKiBps) = 0;

        static void PowerStateToggleThreadDispatch(void *pThis);
    };
    typedef shared_ptr<TestMode> TestModePtr;
};

#endif // INCLUDED_LWLDT_TEST_MODE_H
