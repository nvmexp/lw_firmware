/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2017-2019,2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#pragma once

#ifndef INCLUDED_LWLWL_TEST_MODE_H
#define INCLUDED_LWLWL_TEST_MODE_H

#include "core/include/rc.h"
#include "core/include/tee.h"
#include "core/include/types.h"
#include "gpu/tests/lwca/lwlwl/lwlwl_alloc_mgr.h"
#include "gpu/tests/lwca/lwlink/lwlink.h"
#include "gpu/tests/lwlink/lwldatatest/lwldt_test_mode.h"
#include <vector>

class GpuTestConfiguration;

namespace LwLinkDataTestHelper
{
    class TestRoute;
    class TestRouteLwda;

    //--------------------------------------------------------------------------
    //! \brief Class which encasulates a test mode for the LWCA based lwlink
    //! data transfer testing.  A test mode consists of a set of LwLink routes
    //! and the transfer directions being tested (In, Out, or In/Out).  Routes
    //! will always have a "local" end and "remote" end.  The "local" end of
    //! a route will always be a GpuSubdevice and the "local" GpuSubdevice may
    //! also be referred to as the device under test ("dut").  When transfer
    //! direction is referenced it is always with respect do the "dut" (e.g.
    //! "In" means data is flowing into the "dut")
    //!
    //! Each route for the test mode will be assigned the number of SMs required
    //! to saturate the raw bandwidth of all the LwLinks in the routes in all
    //! directions being tested based on the configuration of the kernel being
    //! used
    //!
    //! For the GUPS related kernel:
    //!    Each test direction of each route will have a destination surface
    //!    that will either be read from or written to depending on the
    //!    direction of traffic under test.
    //!
    //!    The GUPS kernel may be configured for either random or linear access
    //!    as well as different transfer widths.
    //!
    //!    The purpose of the GUPS test is to generate specific size
    //!    lwlink packets based on the supported transfer widths (4, 8, 16,
    //!    and 128 bytes).  The kernel itself reads or writes from random
    //!    locations of the destination surface (writes are with random data).
    //!
    //!    No data verification is done in the GUPS test, however bandwidth
    //!    verification is performed
    //!
    //! For the CE2D emulation kernel (not yet implemented):
    //!    This kernel will (when implemented) generate similar traffic to the
    //!    LwLinkBwStress test that uses the CE or 2D engines
    //!
    class TestModeLwda : public TestMode
    {
        public:
            enum P2PAllocMode
            {
                 P2P_ALLOC_ALL_LOCAL
                ,P2P_ALLOC_ALL_REMOTE
                ,P2P_ALLOC_IN_LOCAL
                ,P2P_ALLOC_OUT_LOCAL
                ,P2P_ALLOC_DEFAULT
            };
            enum SubtestType
            {
                ST_GUPS
               ,ST_CE2D
            };
            enum TransferWidth
            {
                TW_4BYTES = 4
               ,TW_8BYTES = 8
               ,TW_16BYTES = 16
               ,TW_128BYTES = 128
            };

            typedef enum LwLinkKernel::MEM_INDEX_TYPE GupsMemIndexType;

            TestModeLwda
            (
                Lwca::Instance *pLwda
               ,GpuTestConfiguration *pTestConfig
               ,AllocMgrPtr pAllocMgr
               ,UINT32 numErrorsToPrint
               ,UINT32 copyLoopCount
               ,FLOAT64 bwThresholdPct
               ,bool bShowBandwidthStats
               ,UINT32 pauseMask
               ,Tee::Priority pri
            );
            virtual ~TestModeLwda();

            virtual RC AcquireResources();
            RC AssignSms
            (
                UINT32 minSms
               ,UINT32 maxSms
               ,bool  *pbSmLimitsExceeded
            );
            RC ConfigureGups
            (
                UINT32 unroll
               ,bool bAtomicWrite
               ,GupsMemIndexType gupsMemIdx
               ,UINT32 memSize
            );
            virtual RC DumpSurfaces(string fnamePrefix);
            UINT32 GetMaxSms();
            virtual void Print(Tee::Priority pri, string prefix);
            virtual RC ReleaseResources();
            void SetCe2dMemorySizeFactor(UINT32 val) { m_Ce2dMemorySizeFactor = val; }
            void SetP2PAllocMode(P2PAllocMode p2pAllocMode) { m_P2PAllocMode = p2pAllocMode; }
            void SetRuntimeSec(UINT32 val) { m_RuntimeSec = val; }
            void SetSubtestType(SubtestType val) { m_SubtestType = val; }
            void SetTransferWidth(TransferWidth val) { m_TransferWidth = val; }
            void SetWarmupSec(UINT32 val) { m_WarmupSec = val; }
        private:

            //! Structure representing a test of a particular direction on a route.
            class TestDirectionDataLwda : public TestDirectionData
            {
                public:
                    TestDirectionDataLwda(AllocMgrPtr pAllocMgr);
                    virtual ~TestDirectionDataLwda() { }

                    RC Acquire
                    (
                        TransferType tt
                       ,Lwca::HostMemory *pCopyTriggers
                       ,GpuDevice *pLocDev
                       ,Lwca::Device locLwdaDev
                       ,Lwca::Device remLwdaDev
                       ,GpuTestConfiguration *pTestConfig
                       ,UINT64 memorySize
                    );
                    virtual bool AreBytesCopied() const { return m_SubtestType != ST_GUPS; }
                    RC CheckDestinationSurface
                    (
                        UINT32 numErrorsToPrint,
                        const string &failStrBase
                    );
                    void ConfigureGups
                    (
                        UINT32 unroll
                       ,bool bAtomicWrite
                       ,GupsMemIndexType gupsMemIdx
                    );
                    virtual RC DumpMemory(string fnameBase) const;
                    virtual string GetBandwidthFailString() const;
                    virtual UINT64 GetBytesPerLoop() const;
                    UINT32 GetSmCount() { return m_SmCount; }
                    string GetSmString() const;
                    UINT32 GetThreadsPerSm() { return m_ThreadsPerSm; }
                    virtual UINT32 GetTransferWidth() const;
                    virtual bool IsInUse() const;
                    virtual RC Release(bool *pbAnyReleased);
                    virtual void Reset();
                    void SetSmCount(UINT32 sms) { m_SmCount = sms; }
                    void SetSubtestType(SubtestType st) { m_SubtestType = st; }
                    void SetThreadsPerSm(UINT32 tpsm) { m_ThreadsPerSm = tpsm; }
                    void SetTransferWidth(TransferWidth tw) { m_TransferWidth = tw; }
                    virtual RC SetupTestSurfaces(GpuTestConfiguration *pTestConfig);
                    virtual RC StartCopies();
                    RC Synchronize();
                    virtual RC UpdateCopyStats(UINT32 numCopies);

                private:
                    AllocMgrLwda *               m_pAllocMgr       = nullptr;
                    AllocMgrLwda::LwdaMemory   * m_pSrcMem         = nullptr;
                    AllocMgrLwda::LwdaMemory   * m_pDstMem         = nullptr;
                    AllocMgrLwda::LwdaMemory   * m_pCopyStatsMem   = nullptr;
                    AllocMgrLwda::LwdaFunction * m_pFunction       = nullptr;
                    UINT32                       m_SmCount         = 0;
                    UINT32                       m_ThreadsPerSm    = 0;
                    TransferWidth                m_TransferWidth   = TW_16BYTES;
                    UINT64                       m_DevCopyTriggers = 0ULL;
                    void *                       m_pCopyTriggers   = nullptr;
                    LwLinkKernel::GupsInput      m_GupsConfig;
                    SubtestType                  m_SubtestType     = ST_GUPS;
            };

            struct SmInfo
            {
                UINT32 assignedSms  = 0;
                UINT32 threadsPerSm = 0;
                UINT32 maxSms       = 0;
            };

            Lwca::Instance *       m_pLwda                = nullptr;
            Lwca::HostMemory       m_CopyTriggers;
            P2PAllocMode           m_P2PAllocMode         = P2P_ALLOC_DEFAULT;
            UINT32                 m_Ce2dMemorySizeFactor = 10;
            TransferWidth          m_TransferWidth        = TW_16BYTES;
            SubtestType            m_SubtestType          = ST_GUPS;
            UINT32                 m_RuntimeSec           = 0;
            UINT32                 m_WarmupSec            = 0;

            UINT32                 m_GupsUnroll           = 8;
            GupsMemIndexType       m_GupsMemIdx           = LwLinkKernel::RANDOM;
            bool                   m_bGupsAtomicWrite     = false;
            UINT32                 m_GupsMemorySize       = 0;

            virtual RC CheckDestinationSurface
            (
                TestRoutePtr pRoute
               ,TestDirectionDataPtr pTestDir
            );
            virtual TestDirectionDataPtr CreateTestDirectionData();
            RC GetBandwidthPerSmKiBps
            (
                 GpuSubdevice *pSubdev
                ,UINT32 *pReadBwPerSmKiBps
                ,UINT32 *pWriteBwPerSmKiBps
            );
            UINT64 GetMemorySize(TestRoutePtr pRoute, GpuTestConfiguration *pTestConfig);
            virtual UINT64 GetMinimumBytesPerCopy(UINT32 maxBwKiBps) { return 0; }
            RC GetSmInfo(TestRouteLwda *pTrLwda, Lwca::Device dev, SmInfo *pSmInfo);
            virtual RC SetupCopies(TestDirectionDataPtr pTestDir);
            virtual RC TriggerAllCopies();
            virtual RC WaitForCopies(FLOAT64 timeoutMs);
    };
};

#endif // INCLUDED_LWLWL_TEST_MODE_H
