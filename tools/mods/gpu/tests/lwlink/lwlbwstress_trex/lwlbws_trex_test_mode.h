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

#include "gpu/tests/lwlink/lwldatatest/lwldt_test_mode.h"
#include "gpu/tests/lwlink/lwldatatest/lwldt_priv.h"

namespace LwLinkDataTestHelper
{
    class TestModeTrex : public TestMode
    {
        public:
            TestModeTrex
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
            virtual ~TestModeTrex() {}

            RC AssignTrex();

            RC AcquireResources() override;
            RC ReleaseResources() override;
            RC DumpSurfaces(string fnamePrefix) override;
            void Print(Tee::Priority pri, string prefix) override;
            RC ShowPortStats() override;
            RC CheckPortStats() override;
            RC StartPortStats() override;
            RC StopPortStats() override;

            void SetDataLength(UINT32 len) { m_DataLen = len; }
            void SetNumPackets(UINT32 i) { m_NumPackets = i; }

        private:

            //! Structure representing a test of a particular direction on a route.
            class TestDirectionDataTrex : public TestDirectionData
            {
                public:
                    TestDirectionDataTrex(AllocMgrPtr pAllocMgr);
                    virtual ~TestDirectionDataTrex() { }

                    RC Initialize(TestDevicePtr pLocDev, TestDevicePtr pRemDev);
                    RC ShowPortStats();
                    RC CheckPortStats();

                    bool AreBytesCopied() const override { return true; }
                    bool AreCopiesComplete();
                    string GetBandwidthFailString() const override { return ""; }
                    UINT64 GetBytesPerLoop() const override;
                    UINT32 GetTransferWidth() const override;
                    set<TestDevicePtr> GetSwitchDevs();
                    bool IsInUse() const override;
                    RC SetupCopies(UINT32 numLoops, UINT32 randomSeed);
                    RC StartCopies() override;
                    RC StopCopies() override;
                    RC UpdateCopyStats(UINT32 numCopies) override;

                    void SetStartTime(UINT64 i) { m_StartTime = i; }
                    void SetEndTime(UINT64 i) { m_EndTime = i; }
                    void SetNumReads(UINT32 i) { m_NumReads = i; }
                    void SetNumWrites(UINT32 i) { m_NumWrites = i; }
                    void SetDataLength(UINT32 i) { m_DataLen = i; }

                private:
                    TestDevicePtr m_pLocDev;
                    TestDevicePtr m_pRemDev;
                    map<UINT32,TestDevicePtr> m_SwitchDevs;
                    FLOAT64 m_TimeoutMs = 0;        //!< Timeout from test configuration //$
                    UINT64  m_StartTime = 0LLU;
                    UINT64  m_EndTime = 0LLU;

                    UINT64 m_TotalPackets = 0LLU;
                    UINT32 m_NumReads = 0;
                    UINT32 m_NumWrites = 0;
                    UINT32 m_DataLen = 256;
            };

            RC CheckDestinationSurface(TestRoutePtr pRoute, TestDirectionDataPtr pTestDir) override;
            TestDirectionDataPtr CreateTestDirectionData() override;
            UINT64 GetMinimumBytesPerCopy(UINT32 maxBwKiBps) override;
            RC SetupCopies(TestDirectionDataPtr pTestDir) override;
            RC TriggerAllCopies() override;
            RC WaitForCopies(FLOAT64 timeoutMs) override;

            set<TestDevicePtr> m_SwitchDevs;
            UINT32 m_DataLen = 256;
            UINT32 m_NumPackets = 10;
    };
};
