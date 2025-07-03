/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2015-2019,2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#pragma once

#ifndef INCLUDED_LWLBWS_TEST_MODE_H
#define INCLUDED_LWLBWS_TEST_MODE_H

#include "core/include/lwrm.h"
#include "core/include/rc.h"
#include "core/include/tasker.h"
#include "core/include/tee.h"
#include "core/include/types.h"
#include "device/interface/lwlink/lwlpowerstate.h"
#include "device/interface/lwlink/lwlpowerstatecounters.h"
#include "gpu/include/dmawrap.h"
#include "gpu/tests/lwlink/lwldatatest/lwldt_priv.h"
#include "gpu/tests/lwlink/lwldatatest/lwldt_test_mode.h"
#include "gpu/utility/lwsurf.h"
#include "gpu/utility/lwlink/lwlinkdev.h"
#include "gpu/utility/surf2d.h"
#include "lwlbws_alloc_mgr.h"

#include <atomic>
#include <cstring>
#include <string>
#include <vector>

class GpuTestConfiguration;
class Channel;

namespace LwLinkDataTestHelper
{
    class TestRoute;
    class TestRouteCe2d;

    //--------------------------------------------------------------------------
    //! \brief Class which encasulates a test mode for the LwLink bandwidth
    //! stress test.  A test mode consists of a set of LwLink routes and
    //! the transfer directions being tested (In, Out, or In/Out).  All LwLink
    //! routes have an endpoint on the GpuSubdevice under test (referred to
    //! as "dut"). When transfer direction is referenced it is always with
    //! respect do the "dut" (e.g. "In" means data is flowing into the "dut")
    //!
    //! Within this class "local" means a resource/class/etc. referencing the
    //! "dut".  "remote" means a resource/class/etc. other than the "dut"
    //!
    //! Note : This test assumes that copy engines are in a 1:1 LCE->PCE mapping
    //!
    //! Each route for the test mode will be assigned the number of copy
    //! engines required to saturate the raw bandwidth of all the LwLinks in the
    //! routes in all directions being tested
    //!
    //! Each test direction of each route has a source/destination pair of
    //! surfaces as well as a list of copies that will be performed.  There is
    //! one copy for each copy engine assigned to the (route, direction)
    //! tuple.  The source and destination surfaces are divided into a number of
    //! regions equal to the number of copy engines.  Each region is further
    //! subdivided into 2 sub-regions and the copies switch their source for the
    //! sub-region each time a copy is performed to keep the data changing:
    //!
    //!            SRC                                           DST
    //!    __________________                            __________________
    //!   | Region 0         |                          | Region 0         |
    //!   |   Subregion 0    |                          |   Subregion 0    |
    //!   |    ----------    |                          |    ----------    |
    //!   |   Subregion 1    |                          |   Subregion 1    |
    //!   |------------------|                          |------------------|
    //!   | Region 1         |                          | Region 1         |
    //!   |   Subregion 0    |                          |   Subregion 0    |
    //!   |    ----------    |                          |    ----------    |
    //!   |   Subregion 1    |                          |   Subregion 1    |
    //!   |------------------|                          |------------------|
    //!   |         .        |                          |         .        |
    //!   |         .        |                          |         .        |
    //!   |         .        |                          |         .        |
    //!   |------------------|                          |------------------|
    //!   | Region N         |                          | Region N         |
    //!   |   Subregion 0    |                          |   Subregion 0    |
    //!   |    ----------    |                          |    ----------    |
    //!   |   Subregion 1    |                          |   Subregion 1    |
    //!   |__________________|                          |__________________|
    //!
    //! For example : The first time data is copied, CE0 copies source region
    //! (0, 0) to destination region (0, 0) and source region (0, 1) to destination
    //! region (0, 1).  The second time data is copied, CE0 copies source region
    //! (0, 1) to destination region (0, 0)  and source region (0, 0) to
    //! destination region (0, 1)
    //!
    //! A test mode also provides functionality for verifying the integrity of
    //! the transferred data as well as the bandwidth achieved
    class TestModeCe2d : public TestMode
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
            TestModeCe2d
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
            virtual ~TestModeCe2d();

            virtual RC AcquireResources();
            RC AssignCesOnePcePerLce();
            RC AssignCopyEngines();
            RC AssignTwod();
            virtual RC DumpSurfaces(string fnamePrefix);
            UINT32 GetMaxCopyEngines();
            virtual void Print(Tee::Priority pri, string prefix);
            virtual RC ReleaseResources();
            void SetCeCopyWidth(UINT32 w) { m_CeCopyWidth = w; }
            void SetPixelComponentMask(UINT32 m) { m_PixelComponentMask = m; }
            void SetSrcSurfaceLayout(Surface2D::Layout l) { m_SrcLayout = l; }
            void SetCopyByLine(bool bCopyByLine) { m_bCopyByLine = bCopyByLine; }
            void SetP2PAllocMode(P2PAllocMode p2pAllocMode) { m_P2PAllocMode = p2pAllocMode; }
            void SetSurfaceSizeFactor(UINT32 val) { m_SurfaceSizeFactor = val; }
            void SetSrcSurfModePercent(UINT32 val) { m_SrcSurfModePercent = val; }
            void SetTestFullFabric(bool val) { m_TestFullFabric = val; }
            void SetUseLwdaCrc(bool val) { m_UseLwdaCrc = val; }
        protected:
            virtual void ResetTestData();
        private:

            //! Structure representing a test of a particular direction on a route.
            class TestDirectionDataCe2d : public TestDirectionData
            {
                public:
                    TestDirectionDataCe2d(AllocMgrPtr pAllocMgr);
                    virtual ~TestDirectionDataCe2d() { m_Copies.clear(); }

                    RC Acquire
                    (
                        Surface2D::Layout srcLayout
                       ,TransferType tt
                       ,const AllocMgrCe2d::SemaphoreData & copyTrigSem
                       ,GpuDevice   *pLocDev
                       ,GpuDevice   *pRemDev
                       ,GpuTestConfiguration *pTestConfig
                       ,UINT32 surfSizeFactor
                    );
                    virtual bool AreBytesCopied() const { return true; }
                    bool AreCopiesComplete() const;
                    RC CheckDestinationSurface
                    (
                        GpuTestConfiguration *pTestConfig,
                        UINT32 pixelComponentMask,
                        UINT32 numErrorsToPrint,
                        const string &failStrBase,
                        const LwSurfRoutines::CrcSize &crcSize,
                        LwSurfRoutines::LwSurf *pGoldCrcData, 
                        LwSurfRoutines::LwSurf *pLwrCrcData 
                    );
                    virtual string GetBandwidthFailString() const { return ""; }
                    virtual UINT64 GetBytesPerLoop() const;
                    string GetCeString() const;
                    bool GetTestFullFabric() { return m_bTestFullFabric; }
                    UINT32 GetTotalPces();
                    virtual UINT32 GetTransferWidth() const;
                    virtual bool IsInUse() const;
                    virtual RC Release(bool *pbAnyReleased);
                    virtual void Reset();
                    void SetCeId(UINT32 copyIdx, UINT32 ceId) { m_Copies[copyIdx].ceId = ceId; }
                    void SetNumCopies(UINT32 numCopies) { m_Copies.resize(numCopies); }
                    void SetPceMask(UINT32 copyIdx, UINT32 pceMask)
                        { m_Copies[copyIdx].pceMask = pceMask; }
                    void SetTestFullFabric(bool val) { m_bTestFullFabric = val; }
                    void SetUseLwdaCrc(bool val) { m_bUseLwdaCrc = val; }
                    RC SetupCopies
                    (
                        UINT32 numLoops
                       ,bool bCopyByLine
                       ,UINT32 pixelComponentMask
                    );
                    virtual RC SetupTestSurfaces(GpuTestConfiguration *pTestConfig, UINT32 surfPercent);
                    virtual RC StartCopies();
                    virtual RC UpdateCopyStats(UINT32 numCopies);
                    RC WritePngs(string fnameBase) const;

                private:
                    //! Structure representing a copy region.  See class description
                    //! for information on surface division into regions/subregions
                    struct CopyRegionData
                    {
                        UINT32                  ceId      = 0U;      //!< Id of copy engine used for copies on       //$
                                                                     //!< the region (only used for CE copies)       //$
                        UINT32                  pceMask   = 0U;      //!< Mask of PCEs assigned to the LCE indicated //$
                                                                     //!< in the ceId                                //$
                        Channel *               pCh       = nullptr; //!< Channel used for copies on the region      //$
                        LwRm::Handle            hCh       = 0U;      //!< Channel handle for copies on the region    //$
                        UINT32                  copyClass = 0U;      //!< GPU class being used for the copy          //$
                        AllocMgrCe2d::SemaphoreData startSem;        //!< Semaphore used for timestamping the start  //$
                                                                     //!< of a copy on the region                    //$
                        AllocMgrCe2d::SemaphoreData endSem;          //!< Semaphore used for timestamping the end    //$
                                                                     //!< of a copy on the region                    //$
                        UINT64                  copyCount = 0LLU;    //!< Number of copies performed on the region   //$
                        UINT32                  startY[2] = {};      //!< Starting Y offset for each subregion       //$
                    };

                    AllocMgrCe2d *m_pAllocMgr           = nullptr;
                    bool          m_bIlwertSrcDstSubReg = true; //!< Whether source/destination
                                                                //!< subregions should be ilwerted
                                                                //!< on the next copy
                    Surface2DPtr  m_pSrcSurf;                   //!< Pointer to source surface
                    Surface2DPtr  m_pDstSurf;                   //!< Pointer to destination surface
                    UINT64        m_SrcCtxDmaOffset     = 0LLU; //!< GPU virtual address of the
                                                                //!< start of the source surface
                    UINT64        m_DstCtxDmaOffset     = 0LLU; //!< GPU virtual address of the
                                                                //!< start of the destination
                                                                //!< surface
                    UINT32        m_CopyHeight          = 0U;   //!< Height of the copies
                    UINT64        m_CopyTrigSemCtxOffset = 0LLU;//!< Ctxdma offset of semaphore
                                                                //!< used to trigger the copies
                    vector<CopyRegionData> m_Copies;            //!< List of copy region data for
                                                                //!< this direction

                    FLOAT64       m_TimeoutMs       = 0;        //!< Timeout from test configuration //$
                    bool          m_bTestFullFabric = false;
                    bool          m_bUseLwdaCrc     = false;

                    RC CopyToSysmem
                    (
                        GpuTestConfiguration *pTestConfig
                       ,Surface2DPtr pSrcSurf
                       ,Surface2DPtr pSysSurf
                    );
                    RC InitializeCopyData
                    (
                        TransferType tt
                       ,GpuDevice   *pPeerDev
                    );
                    RC WriteCeMethods
                    (
                         Channel *pCh
                        ,UINT32 ceClass
                        ,Surface2DPtr pSrcSurf
                        ,UINT64 srcOffset64
                        ,UINT32 yOffset
                        ,UINT64 dstOffset64
                        ,UINT64 semOffset64
                        ,UINT32 height
                        ,UINT32 semPayload
                        ,UINT32 pixelComponentMask
                    );
                    RC WriteCopyMethods
                    (
                        Channel *pCh,
                        UINT32 copyClass,
                        Surface2DPtr pSrcSurf,
                        Surface2DPtr pDstSurf,
                        UINT64 srcOffset64,
                        UINT32 srcY,
                        UINT64 dstOffset64,
                        UINT32 dstY,
                        Surface2DPtr pSemSurf,
                        UINT64 semOffset64,
                        UINT32 height,
                        UINT32 semPayload,
                        bool   bCopyByLine,
                        UINT32 pixelComponentMask
                    );
                    RC WriteTwodMethods
                    (
                         Channel *pCh
                        ,Surface2DPtr pSrcSurf
                        ,UINT64 srcOffset64
                        ,UINT32 yOffset
                        ,UINT64 dstOffset64
                        ,UINT64 semOffset64
                        ,UINT32 height
                        ,UINT32 semPayload
                    );
            };

            Surface2D::Layout      m_SrcLayout            = Surface2D::Pitch;

            //! Perform the copy a single line at a within DoOneCopy time
            bool                   m_bCopyByLine          = false;
            bool                   m_bTwoDTranfers        = false;

            //! Copy engine allocation mode for P2P routes
            P2PAllocMode           m_P2PAllocMode         = P2P_ALLOC_DEFAULT;
            UINT32                 m_CeCopyWidth          = 256;
            UINT32                 m_PixelComponentMask   = ~0U;
            bool                   m_bCopyEnginesAssigned = false;

            //! CEs that are valid for assignment. Each set bit indicates a valid LCE instance
            UINT32                 m_SurfaceSizeFactor    = 10;

            //! Control what ratio of gold surfaces are transfered
            UINT32                 m_SrcSurfModePercent   = 50;

            //! Semaphore used to synchronize data transfer start across all channels
            AllocMgrCe2d::SemaphoreData m_CopyTrigSem;
            UINT32                  m_TotalCopies         = 0;

            //! Create surfaces to test all fabric addresses
            bool                    m_TestFullFabric      = false; 
            bool                    m_UseLwdaCrc          = true;

            bool                    m_OnePcePerLce        = true;

            //! CRC data if using LWCA.  All surfaces should be identical so
            //! only one copy is necessary.  Keep both the gold and current CRC
            //! as member variables so that the data is only allocated once for
            //! the current CRC (not once per check) since excessive allocations
            //! force synchronizations across all GPUs
            LwSurfRoutines::LwSurf  m_LwGoldCrcData;
            LwSurfRoutines::LwSurf  m_LwLwrCrcData;
            LwSurfRoutines::CrcSize m_LwCrcSize;


            RC AssignCeToTestDir
            (
                TestDirectionDataPtr   pTestDir,
                GpuSubdevice *         pSubdev,
                bool                   bLocalHw,
                UINT32                 ceIdx
            );

            void AssignOneToOneCesToTestDir
            (
                TestDirectionDataPtr   pTestDir,
                bool                   bLocalHw,
                UINT32                 firstCe,
                UINT32                 ceCount
            );

            RC AssignForcedLocalCes(map<GpuSubdevice *, UINT32> *pAssignedCeMasks);
            RC AssignOneToOneForcedLocalCes(map<GpuSubdevice *, pair<INT32, UINT32>> *pAvailCes);

            RC AssignP2PCes(map<GpuSubdevice *, UINT32> *pAssignedCeMasks);
            RC AssignOneToOneP2PCes(map<GpuSubdevice *, pair<INT32, UINT32>> *pAvailCes);

            void AutoAdjustCopyLoopcount(TestRouteCe2d *pTrCe2d, TestDirectionDataCe2d *pTdCe2d);
            virtual RC CheckDestinationSurface
            (
                TestRoutePtr pRoute
               ,TestDirectionDataPtr pTestDir
            );
            virtual TestDirectionDataPtr CreateTestDirectionData();
            virtual string GetBandwidthFailStringImpl
            (
                TestRoutePtr pRoute,
                TestDirectionDataPtr pTestDir
            );
            virtual UINT64 GetMinimumBytesPerCopy(UINT32 maxBwKiBps);

            virtual RC SetupCopies(TestDirectionDataPtr pTestDir);
            virtual RC TriggerAllCopies();
            virtual RC WaitForCopies(FLOAT64 timeoutMs);

            static bool PollCopiesComplete(void *pvTestMode);
    };
};

#endif // INCLUDED_LWLBWS_TEST_MODE_H
