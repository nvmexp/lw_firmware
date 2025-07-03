/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2017-2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#include "gpu/tests/lwca/lwlwl/lwlwl_test_mode.h"
#include "gpu/tests/lwca/lwlwl/lwlwl_test_route.h"
#include "gpu/tests/lwca/lwlwl/lwlwl_alloc_mgr.h"
#include "gpu/tests/lwca/lwlink/lwlink.h"
#include "gpu/tests/gputestc.h"
#include "gpu/include/gpudev.h"
#include "gpu/include/gpusbdev.h"
#include "gpu/lwlink/lwlinkimpl.h"
#include "core/include/fileholder.h"
#include "core/include/framebuf.h"
#include "core/include/utility.h"
#include "core/include/tasker.h"
#include "core/include/xp.h"

using namespace LwLinkDataTestHelper;

namespace
{
    // Default GUPS memory size if it was unable to be callwlated
    const UINT64 DEFAULT_GUPS_TRANSFER_COUNT = 2048 * 64;

    static string GetFunctionName(TestModeLwda::SubtestType st)
    {
        string functionName = "IlwalidFunction";
        switch (st)
        {
            case TestModeLwda::ST_GUPS:
                functionName = "lwlinkgups";
                break;
            case TestModeLwda::ST_CE2D:
                functionName = "lwlinkbwstress";
                break;
            default:
                break;
        }
        return functionName;
    }
}

//------------------------------------------------------------------------------
//! \brief Constructor
//!
TestModeLwda::TestModeLwda
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
) : TestMode(pTestConfig,
             pAllocMgr,
             numErrorsToPrint,
             copyLoopCount,
             bwThresholdPct,
             bShowBandwidthStats,
             pauseMask,
             pri)
   ,m_pLwda(pLwda)
{
}

//------------------------------------------------------------------------------
//! \brief Destructor
//!
TestModeLwda::~TestModeLwda()
{
    ReleaseResources();
}

//------------------------------------------------------------------------------
//! \brief Acquire resources on both the local and any remote devices in order
//! that the test mode can execute all necessary copies.  Resources are acquired
//! from the allocation manager which "lazy allocates"
//!
//! \return OK if successful, not OK if resource acquire failed
/* virtual */ RC TestModeLwda::AcquireResources()
{
    RC rc;

    DEFERNAME(releaseResources) { ReleaseResources(); };

    // Initialize and connect all devices first so that any memory that is allocated
    // will end up being accessible to all devices due to UVA
    for (auto & testMapEntry : GetTestData())
    {
        TestRouteLwda *pTestRouteLwda = static_cast<TestRouteLwda *>(testMapEntry.first.get());
        Lwca::Device locLwdaDev = pTestRouteLwda->GetLocalLwdaDevice();
        Lwca::Device remLwdaDev = pTestRouteLwda->GetRemoteLwdaDevice();

        if (!m_pLwda->IsContextInitialized(locLwdaDev))
        {
            CHECK_RC(m_pLwda->InitContext(locLwdaDev));
        }

        if (pTestRouteLwda->GetTransferType() == TT_LOOPBACK)
        {
            CHECK_RC(m_pLwda->EnableLoopbackAccess(locLwdaDev));
        }
        else if (pTestRouteLwda->GetTransferType() == TT_P2P)
        {
            if (!m_pLwda->IsContextInitialized(remLwdaDev))
            {
                CHECK_RC(m_pLwda->InitContext(remLwdaDev));
            }
            CHECK_RC(m_pLwda->EnablePeerAccess(locLwdaDev, remLwdaDev));
            CHECK_RC(m_pLwda->EnablePeerAccess(remLwdaDev, locLwdaDev));
        }
    }

    for (auto & testMapEntry : GetTestData())
    {
        TestDirectionDataLwda *pTdLwda;
        TestRouteLwda *pTestRouteLwda = static_cast<TestRouteLwda *>(testMapEntry.first.get());

        Printf(GetPrintPri(),
               "%s : Acquiring resources for test route:\n"
               "%s\n",
               __FUNCTION__,
               pTestRouteLwda->GetString("    ").c_str());

        Lwca::Device locLwdaDev = pTestRouteLwda->GetLocalLwdaDevice();
        Lwca::Device remLwdaDev = pTestRouteLwda->GetRemoteLwdaDevice();

        if (!m_CopyTriggers.GetSize())
        {
            CHECK_RC(m_pLwda->AllocHostMem(locLwdaDev, sizeof(LwLinkKernel::CopyTriggers), &m_CopyTriggers));
        }

        pTdLwda = static_cast<TestDirectionDataLwda *>(testMapEntry.second.pDutInTestData.get());

        const UINT64 memorySize = GetMemorySize(testMapEntry.first, GetTestConfig());
        CHECK_RC(pTdLwda->Acquire(pTestRouteLwda->GetTransferType(),
                                  &m_CopyTriggers,
                                  pTestRouteLwda->GetLocalSubdevice()->GetParentDevice(),
                                  locLwdaDev,
                                  remLwdaDev,
                                  GetTestConfig(),
                                  memorySize));
        if (m_SubtestType == ST_GUPS)
            pTdLwda->ConfigureGups(m_GupsUnroll, m_bGupsAtomicWrite, m_GupsMemIdx);
        CHECK_RC(pTdLwda->SetupTestSurfaces(GetTestConfig()));

        pTdLwda = static_cast<TestDirectionDataLwda *>(testMapEntry.second.pDutOutTestData.get());
        CHECK_RC(pTdLwda->Acquire(pTestRouteLwda->GetTransferType(),
                                  &m_CopyTriggers,
                                  pTestRouteLwda->GetLocalSubdevice()->GetParentDevice(),
                                  locLwdaDev,
                                  remLwdaDev,
                                  GetTestConfig(),
                                  memorySize));
        if (m_SubtestType == ST_GUPS)
            pTdLwda->ConfigureGups(m_GupsUnroll, m_bGupsAtomicWrite, m_GupsMemIdx);
        CHECK_RC(pTdLwda->SetupTestSurfaces(GetTestConfig()));
    }

    CHECK_RC(TestMode::AcquireResources());
    releaseResources.Cancel();

    return rc;
}

//------------------------------------------------------------------------------
//! \brief Assign the SMs to each route.  Each route will
//! be assigned the number of SMs required to saturate the raw
//! bandwidth of all the LwLinks in the route in all directions being
//! tested
//!
//! \param minSms             : Minimum SMs to assign
//! \param maxSms             : Maximum SMs to assign
//! \param pbSmLimitsExceeded : Returned boolean indicating if failure is due
//!                             to SM limits being exceeded
//!
//! \return OK if successful, not OK on failure or if unable to assign enough
//! SMs to saturate all links of all routes being tested in all
//! directions
RC TestModeLwda::AssignSms
(
    UINT32 minSms
   ,UINT32 maxSms
   ,bool  *pbSmLimitsExceeded
)
{
    RC rc;
    map<GpuSubdevice *, SmInfo> smsAssigned;

    DEFERNAME(resetTestData) { ResetTestData(); };

    *pbSmLimitsExceeded = false;
    for (auto & testMapEntry : GetTestData())
    {
        // Eradicate any previous assignments
        testMapEntry.second.pDutInTestData->Reset();
        testMapEntry.second.pDutOutTestData->Reset();

        TestRouteLwda *pTestRouteLwda;
        pTestRouteLwda = static_cast<TestRouteLwda *>(testMapEntry.first.get());

        GpuSubdevice *pLocSub = pTestRouteLwda->GetLocalSubdevice();
        if (smsAssigned.find(pLocSub) == smsAssigned.end())
        {
            SmInfo smInfo;
            CHECK_RC(GetSmInfo(pTestRouteLwda, pTestRouteLwda->GetLocalLwdaDevice(), &smInfo));
            smsAssigned[pLocSub] = smInfo;
        }

        if (pTestRouteLwda->GetTransferType() == TT_P2P)
        {
            GpuSubdevice *pRemSub = pTestRouteLwda->GetRemoteSubdevice();
            if (smsAssigned.find(pRemSub) == smsAssigned.end())
            {
                SmInfo smInfo;
                CHECK_RC(GetSmInfo(pTestRouteLwda,
                                   pTestRouteLwda->GetRemoteLwdaDevice(),
                                   &smInfo));
                smsAssigned[pRemSub] = smInfo;
            }
            continue;
        }

        // Sysmem and loopback routes require SMs on the local GPU so
        // assign them first
        TransferDir td = GetTransferDir();
        if (pTestRouteLwda->GetTransferType() == TT_SYSMEM)
            td = GetSysmemTransferDir();

        UINT32 inSmCount;
        UINT32 outSmCount;
        UINT32 readBwKiBpsPerSm = 0;
        UINT32 writeBwKiBpsPerSm = 0;
        CHECK_RC(GetBandwidthPerSmKiBps(pLocSub, &readBwKiBpsPerSm, &writeBwKiBpsPerSm));
        CHECK_RC(pTestRouteLwda->GetRequiredSms(td,
                                                static_cast<UINT32>(m_TransferWidth),
                                                true,
                                                readBwKiBpsPerSm,
                                                writeBwKiBpsPerSm,
                                                minSms,
                                                maxSms,
                                                &inSmCount,
                                                &outSmCount));

        // If testing system memory via loopback (i.e. with source and
        // destination both system memory).  Use the maximum sm count
        // and choose to put it into the input sm count - unlike normal loopback
        // it doesnt matter whether in or out is chosen since both reads and
        // writes will occur through lwlink regardless of the choice
        if ((pTestRouteLwda->GetTransferType() == TT_SYSMEM) &&
            GetSysmemLoopback())
        {
            inSmCount  = max(inSmCount, outSmCount);
            outSmCount = 0;
        }

        // For loopback, use in/out sm count depending upon setting.  Input means that
        // reads will occur over lwlink, output means that writes will occur
        if (pTestRouteLwda->GetTransferType() == TT_LOOPBACK)
        {
            inSmCount  = GetLoopbackInput() ? inSmCount : 0;
            outSmCount = GetLoopbackInput() ? 0 : outSmCount;
        }

        // If there are not enough sms on the DUT remaining to saturate the
        // current route then fail
        const UINT32 totalSmsReq =
            smsAssigned[pLocSub].assignedSms + inSmCount + outSmCount;
        if (totalSmsReq > smsAssigned[pLocSub].maxSms)
        {
            *pbSmLimitsExceeded = true;
            return RC::UNSUPPORTED_SYSTEM_CONFIG;
        }

        TestDirectionDataLwda *pInLwda;
        pInLwda = static_cast<TestDirectionDataLwda *>(testMapEntry.second.pDutInTestData.get());

        if (inSmCount)
        {
            pInLwda->SetHwLocal(true);
            pInLwda->SetSmCount(inSmCount);
            pInLwda->SetThreadsPerSm(smsAssigned[pLocSub].threadsPerSm);
            pInLwda->SetTransferHw(LwLinkImpl::HT_SM);
            pInLwda->SetTransferWidth(m_TransferWidth);
            pInLwda->SetSubtestType(m_SubtestType);
            smsAssigned[pLocSub].assignedSms += inSmCount;
        }
        else
        {
            pInLwda->Reset();
        }

        TestDirectionDataLwda *pOutLwda;
        pOutLwda =
            static_cast<TestDirectionDataLwda *>(testMapEntry.second.pDutOutTestData.get());

        if (outSmCount)
        {
            pOutLwda->SetHwLocal(true);
            pOutLwda->SetSmCount(outSmCount);
            pOutLwda->SetThreadsPerSm(smsAssigned[pLocSub].threadsPerSm);
            pOutLwda->SetTransferHw(LwLinkImpl::HT_SM);
            pOutLwda->SetTransferWidth(m_TransferWidth);
            pOutLwda->SetSubtestType(m_SubtestType);
            smsAssigned[pLocSub].assignedSms += outSmCount;
        }
        else
        {
            pOutLwda->Reset();
        }

        if ((pInLwda->GetTransferHw() == LwLinkImpl::HT_NONE) &&
            (pOutLwda->GetTransferHw() == LwLinkImpl::HT_NONE))
        {
            Printf(Tee::PriError,
                   "%s : No SMs assigned on route :\n%s\n",
                   __FUNCTION__, pTestRouteLwda->GetString("    ").c_str());
            return RC::SOFTWARE_ERROR;
        }
    }

    for (auto & testMapEntry : GetTestData())
    {
        TestRouteLwda *pTestRouteLwda;
        pTestRouteLwda = static_cast<TestRouteLwda *>(testMapEntry.first.get());

        if (pTestRouteLwda->GetTransferType() != TT_P2P)
            continue;

        P2PAllocMode p2pAlloc   = m_P2PAllocMode;
        UINT32 dutInLocSmCount  = 0;
        UINT32 dutOutLocSmCount = 0;
        UINT32 dutInRemSmCount  = 0;
        UINT32 dutOutRemSmCount = 0;

        GpuSubdevice *pLocSub = pTestRouteLwda->GetLocalSubdevice();
        GpuSubdevice *pRemSub = pTestRouteLwda->GetRemoteSubdevice();
        UINT32 readBwKiBpsPerSm = 0;
        UINT32 writeBwKiBpsPerSm = 0;
        CHECK_RC(GetBandwidthPerSmKiBps(pLocSub, &readBwKiBpsPerSm, &writeBwKiBpsPerSm));
        MASSERT(readBwKiBpsPerSm > 0 && writeBwKiBpsPerSm > 0);
        CHECK_RC(pTestRouteLwda->GetRequiredSms(GetTransferDir(),
                                                static_cast<UINT32>(m_TransferWidth),
                                                true,
                                                readBwKiBpsPerSm,
                                                writeBwKiBpsPerSm,
                                                minSms,
                                                maxSms,
                                                &dutInLocSmCount,
                                                &dutOutLocSmCount));
        CHECK_RC(GetBandwidthPerSmKiBps(pRemSub, &readBwKiBpsPerSm, &writeBwKiBpsPerSm));
        CHECK_RC(pTestRouteLwda->GetRequiredSms(GetTransferDir(),
                                                static_cast<UINT32>(m_TransferWidth),
                                                false,
                                                readBwKiBpsPerSm,
                                                writeBwKiBpsPerSm,
                                                minSms,
                                                maxSms,
                                                &dutInRemSmCount,
                                                &dutOutRemSmCount));

        if (p2pAlloc == P2P_ALLOC_DEFAULT)
        {
            // Assign Sms for P2P transfer based on the following priority:
            //
            //    1. If all necessary sms for generating input P2P traffic to the
            //       DUT can be allocated on the DUT, then do so
            //    2. If all necessary sms for generating output P2P traffic from the
            //       DUT can be allocated on the DUT, then do so
            //    3. If all necessary sms generating output P2P traffic for all
            //       directions under test can be allocated on the DUT, then do so
            //    4. Allocate all sms for generating output P2P traffic for all
            //       directions under test on the remote peer device
            //
            // Note that is important not to assume that the local and remote sm
            // counts are identical since the GPUs may not be 100% symmetrical (for
            // instance the sm bandwidth will change depending upon the gpcclk on
            // each GPU)

            const UINT32 allLocReq =
                smsAssigned[pLocSub].assignedSms + dutInLocSmCount + dutOutLocSmCount;
            if ((GetTransferDir() != TD_OUT_ONLY) &&
                ((smsAssigned[pLocSub].assignedSms + dutInLocSmCount) <= smsAssigned[pLocSub].maxSms) &&          //$
                ((smsAssigned[pRemSub].assignedSms + dutOutRemSmCount) <= smsAssigned[pRemSub].maxSms))           //$
            {
                p2pAlloc = P2P_ALLOC_IN_LOCAL;
            }
            else if ((GetTransferDir() != TD_IN_ONLY) &&
                     ((smsAssigned[pLocSub].assignedSms + dutOutLocSmCount) <= smsAssigned[pLocSub].maxSms) &&    //$
                     ((smsAssigned[pRemSub].assignedSms + dutInRemSmCount) <= smsAssigned[pRemSub].maxSms))       //$
            {
                p2pAlloc = P2P_ALLOC_OUT_LOCAL;
            }
            else if (allLocReq <= smsAssigned[pLocSub].maxSms)
            {
                p2pAlloc = P2P_ALLOC_ALL_LOCAL;
            }
            else
            {
                p2pAlloc = P2P_ALLOC_ALL_REMOTE;
            }
        }

        bool bInTransferLocal    = false;
        bool bOutTransferLocal   = false;
        UINT32 dutInSmCount      = 0;
        UINT32 dutOutSmCount     = 0;
        UINT32 dutLocalSmCount   = 0;
        UINT32 dutRemoteSmCount  = 0;
        switch (p2pAlloc)
        {
            case P2P_ALLOC_ALL_LOCAL:
                bInTransferLocal    = true;
                bOutTransferLocal   = true;
                dutInSmCount      = dutInLocSmCount;
                dutOutSmCount     = dutOutLocSmCount;
                dutLocalSmCount   = dutInLocSmCount + dutOutLocSmCount;
                dutRemoteSmCount  = 0;
                break;
            case P2P_ALLOC_OUT_LOCAL:
                bInTransferLocal    = false;
                bOutTransferLocal   = true;
                dutInSmCount      = dutInRemSmCount;
                dutOutSmCount     = dutOutLocSmCount;
                dutLocalSmCount   = dutOutSmCount;
                dutRemoteSmCount  = dutInSmCount;
                break;
            case P2P_ALLOC_IN_LOCAL:
                bInTransferLocal    = true;
                bOutTransferLocal   = false;
                dutInSmCount      = dutInLocSmCount;
                dutOutSmCount     = dutOutRemSmCount;
                dutLocalSmCount   = dutInSmCount;
                dutRemoteSmCount  = dutOutSmCount;
                break;
            case P2P_ALLOC_ALL_REMOTE:
                bInTransferLocal    = false;
                bOutTransferLocal   = false;
                dutInSmCount      = dutInRemSmCount;
                dutOutSmCount     = dutOutRemSmCount;
                dutLocalSmCount   = 0;
                dutRemoteSmCount  = dutInSmCount + dutOutSmCount;
                break;
            default:
                Printf(Tee::PriError, "%s : Unknown P2P allocation mode %d on route :\n%s\n",
                       __FUNCTION__, p2pAlloc, pTestRouteLwda->GetString("    ").c_str());
                return RC::SOFTWARE_ERROR;
        }

        // Ensure a valid configuration
        if (((smsAssigned[pLocSub].assignedSms + dutLocalSmCount) > smsAssigned[pLocSub].maxSms) ||
            ((smsAssigned[pRemSub].assignedSms + dutRemoteSmCount) > smsAssigned[pRemSub].maxSms))

        {
            Printf(GetPrintPri(),
                   "%s : SM assignment failed on route :\n%s\n",
                   __FUNCTION__, pTestRouteLwda->GetString("    ").c_str());
            *pbSmLimitsExceeded = true;
            return RC::UNSUPPORTED_SYSTEM_CONFIG;
        }

        TestDirectionDataLwda *pInLwda;
        pInLwda = static_cast<TestDirectionDataLwda *>(testMapEntry.second.pDutInTestData.get());

        if (dutInSmCount)
        {
            GpuSubdevice *pHwSub = bInTransferLocal ? pLocSub : pRemSub;
            pInLwda->SetHwLocal(bInTransferLocal);
            pInLwda->SetSmCount(dutInSmCount);
            pInLwda->SetThreadsPerSm(smsAssigned[pHwSub].threadsPerSm);
            pInLwda->SetTransferHw(LwLinkImpl::HT_SM);
            pInLwda->SetTransferWidth(m_TransferWidth);
            pInLwda->SetSubtestType(m_SubtestType);
            smsAssigned[pHwSub].assignedSms += dutInSmCount;
        }
        else
        {
            pInLwda->Reset();
        }

        TestDirectionDataLwda *pOutLwda;
        pOutLwda =
            static_cast<TestDirectionDataLwda *>(testMapEntry.second.pDutOutTestData.get());

        if (dutOutSmCount)
        {
            GpuSubdevice *pHwSub = bInTransferLocal ? pLocSub : pRemSub;
            pOutLwda->SetHwLocal(bOutTransferLocal);
            pOutLwda->SetSmCount(dutOutSmCount);
            pOutLwda->SetThreadsPerSm(smsAssigned[pHwSub].threadsPerSm);
            pOutLwda->SetTransferHw(LwLinkImpl::HT_SM);
            pOutLwda->SetTransferWidth(m_TransferWidth);
            pOutLwda->SetSubtestType(m_SubtestType);
            smsAssigned[pHwSub].assignedSms += dutOutSmCount;
        }
        else
        {
            pOutLwda->Reset();
        }

        if ((pInLwda->GetTransferHw() == LwLinkImpl::HT_NONE) &&
            (pOutLwda->GetTransferHw() == LwLinkImpl::HT_NONE))
        {
            Printf(Tee::PriError,
                   "%s : No SMs assigned on route :\n%s\n",
                   __FUNCTION__, pTestRouteLwda->GetString("    ").c_str());
            return RC::SOFTWARE_ERROR;
        }
    }

    resetTestData.Cancel();
    return rc;
}

//------------------------------------------------------------------------------
RC TestModeLwda::ConfigureGups
(
    UINT32 unroll
   ,bool bAtomicWrite
   ,GupsMemIndexType gupsMemIdx
   ,UINT32 memSize
)
{
    if ((unroll == 0) || (unroll > 8) || (unroll & (unroll - 1)))
    {
        Printf(Tee::PriError, "%s : GUPS testing only supports unroll factors of 1, 2, 4, 8\n",
               __FUNCTION__);
        return RC::BAD_PARAMETER;
    }

    m_GupsUnroll       = unroll;
    m_bGupsAtomicWrite = bAtomicWrite;
    m_GupsMemIdx       = gupsMemIdx;
    m_GupsMemorySize   = memSize;
    return OK;
}

//------------------------------------------------------------------------------
//! \brief Dump surfaces for the specified frame number
//!
//! \param fnamePrefix : filename prefix to use
//!
//! \return OK if dump was successful, not OK otherwise
RC TestModeLwda::DumpSurfaces(string fnamePrefix)
{
    RC rc;
    for (auto testMapEntry : GetTestData())
    {
        string fnameBase =
            fnamePrefix + "_" + testMapEntry.first->GetFilenameString();

        string fname;
        TestDirectionDataLwda *pTdLwda;
        pTdLwda =
            static_cast<TestDirectionDataLwda *>(testMapEntry.second.pDutInTestData.get());
        if (pTdLwda->IsInUse())
        {
            CHECK_RC(pTdLwda->DumpMemory(fnameBase + "_in"));
        }

        pTdLwda =
            static_cast<TestDirectionDataLwda *>(testMapEntry.second.pDutOutTestData.get());
        if (pTdLwda->IsInUse())
        {
            CHECK_RC(pTdLwda->DumpMemory(fnameBase + "_out"));
        }
    }
    return rc;
}

//------------------------------------------------------------------------------
//! \brief Returns the maximum number of SMs present on any GPU on any route
//!
//! \return Maximum number of SMs present on any GPU on any route
UINT32 TestModeLwda::GetMaxSms()
{
    UINT32 maxSms = 0;
    for (auto const & testRteData : GetTestData())
    {
        TestRouteLwda *pTrLwda;
        pTrLwda = static_cast<TestRouteLwda *>(testRteData.first.get());

        maxSms = max(pTrLwda->GetLocalMaxSms(),
                         max(pTrLwda->GetRemoteMaxSms(),
                             maxSms));
    }
    return maxSms;
}

//------------------------------------------------------------------------------
//! \brief Print the details of the test mode
//!
//! \param pri    : Priority to print at
//! \param prefix : Prefix for each line of the print
//!
//! \return OK if initialization was successful, not OK otherwise
void TestModeLwda::Print(Tee::Priority pri, string prefix)
{
    string testModeStr = prefix + "Dir    Sysmem Dir    # Rte    HwType\n" +
                         prefix + "------------------------------------\n";

    testModeStr += prefix +
        Utility::StrPrintf("%3s    %10s    %5u    %6s\n\n",
               TransferDirStr(GetTransferDir()).c_str(),
               TransferDirStr(GetSysmemTransferDir()).c_str(),
               GetNumRoutes(),
               "SM");

    UINT32 linkColWidth = 0;
    UINT32 ttColWidth = 0;
    for (auto const & testRteData : GetTestData())
    {
        TestRoutePtr pTestRoute = testRteData.first;
        size_t tempSize = pTestRoute->GetLocalDevString().size() +
                          3 +
                          pTestRoute->GetLinkString(false).size();
        linkColWidth = max(static_cast<UINT32>(tempSize), linkColWidth);
        tempSize = pTestRoute->GetRemoteDevString().size() +
                   3 +
                   pTestRoute->GetLinkString(true).size();
        linkColWidth = max(static_cast<UINT32>(tempSize), linkColWidth);

        tempSize = TransferTypeStr(pTestRoute->GetTransferType()).size();
        if ((pTestRoute->GetTransferType() == TT_SYSMEM) && GetSysmemLoopback())
            tempSize += 3;
        ttColWidth   = max(static_cast<UINT32>(tempSize), ttColWidth);
    }

    // Ensure that the max Link field width is at least 7 (length of "Link(s)")
    static const string locLinkStr = "Loc Link(s)";
    static const string remLinkStr = "Rem Link(s)";
    if (linkColWidth < locLinkStr.size())
        linkColWidth = static_cast<UINT32>(locLinkStr.size());

    static const string typeStr = "Type";
    if (ttColWidth < typeStr.size())
        ttColWidth = static_cast<UINT32>(typeStr.size());

    // Construct the header and the line under it, padding the lines with
    // appropriate characters based on the maximum link field width
    string hdrStr = prefix;
    hdrStr += "  ";
    hdrStr += string(linkColWidth - locLinkStr.size(), ' ');
    hdrStr += locLinkStr + "   ";
    hdrStr += string(ttColWidth - typeStr.size(), ' ');
    hdrStr += typeStr + "   ";
    hdrStr += string(linkColWidth - remLinkStr.size(), ' ');
    hdrStr += remLinkStr;
    hdrStr += "   Input SMs (threads)   Output SMs (threads)";

    UINT32 dashCount = hdrStr.size() - prefix.size();
    testModeStr += hdrStr + "\n";
    testModeStr += prefix;
    testModeStr += "  ";
    testModeStr += string(dashCount, '-');
    testModeStr += "\n";

    for (auto const & testRteData : GetTestData())
    {
        TestRoutePtr pTestRoute = testRteData.first;
        const string locLinkStr = pTestRoute->GetLocalDevString() +
                                  " : " +
                                  pTestRoute->GetLinkString(false);
        const string remLinkStr = pTestRoute->GetRemoteDevString() +
                                  " : " +
                                  pTestRoute->GetLinkString(true);

        string typeStr = TransferTypeStr(pTestRoute->GetTransferType());

        if ((pTestRoute->GetTransferType() == TT_SYSMEM) && GetSysmemLoopback())
            typeStr += "(L)";

        testModeStr += Utility::StrPrintf(
                "%s  %*s   %*s   %*s",
                prefix.c_str(),
                linkColWidth,
                locLinkStr.c_str(),
                ttColWidth,
                typeStr.c_str(),
                linkColWidth,
                remLinkStr.c_str());

        TestDirectionDataLwda *pTdInLwda;
        TestDirectionDataLwda *pTdOutLwda;
        pTdInLwda =
            static_cast<TestDirectionDataLwda *>(testRteData.second.pDutInTestData.get());
        pTdOutLwda =
            static_cast<TestDirectionDataLwda *>(testRteData.second.pDutOutTestData.get());
        const string inSmStr = pTdInLwda->GetSmString();
        const string outSmStr = pTdOutLwda->GetSmString();
        testModeStr += Utility::StrPrintf("   %19s   %20s",
                                          inSmStr.c_str(),
                                          outSmStr.c_str());
        testModeStr += "\n";
    }
    Printf(pri, "\n%s\n", testModeStr.c_str());
}

//------------------------------------------------------------------------------
//! \brief Release resources on both the "dut" and any remote devices.  Note
//! that this only releases resource usage and does not actually free the
//! resources since the resources may be used in future test modes
//!
//! \return OK if successful, not OK if resource release failed
RC TestModeLwda::ReleaseResources()
{
    StickyRC rc;

    rc = TestMode::ReleaseResources();

    for (auto & testMapEntry : GetTestData())
    {
        TestRouteLwda *pTestRouteLwda = static_cast<TestRouteLwda *>(testMapEntry.first.get());

        Lwca::Device locLwdaDev = pTestRouteLwda->GetLocalLwdaDevice();
        Lwca::Device remLwdaDev = pTestRouteLwda->GetRemoteLwdaDevice();

        if (!m_pLwda->IsContextInitialized(locLwdaDev))
            continue;

        if ((pTestRouteLwda->GetTransferType() == TT_LOOPBACK) &&
            m_pLwda->IsContextInitialized(locLwdaDev))
        {
            rc = m_pLwda->DisableLoopbackAccess(locLwdaDev);
        }
        else if (pTestRouteLwda->GetTransferType() == TT_P2P)
        {
            if (!m_pLwda->IsContextInitialized(remLwdaDev))
                continue;
            rc = m_pLwda->DisablePeerAccess(locLwdaDev, remLwdaDev);
            rc = m_pLwda->DisablePeerAccess(remLwdaDev, locLwdaDev);
        }
    }

    m_CopyTriggers.Free();

    return rc;
}

//------------------------------------------------------------------------------
// TestModeLwda protected implementation
//------------------------------------------------------------------------------

//------------------------------------------------------------------------------
// TestModeLwda private implementation
//------------------------------------------------------------------------------

//------------------------------------------------------------------------------
//! \brief Verify the destination surface for the specific test
//! route/direction
//!
//! \param pRoute   : Pointer to the test route
//! \param pTestDir : Pointer to the test direction
//!
//! \return OK if verification was successful, not OK otherwise
RC TestModeLwda::CheckDestinationSurface
(
    TestRoutePtr pRoute,
    TestDirectionDataPtr pTestDir
)
{
    if ((m_SubtestType == ST_GUPS) || !pTestDir->IsInUse())
        return OK;

    Printf(GetPrintPri(),
           "%s : Checking %s destination surface of test route:\n"
           "%s\n",
           __FUNCTION__,
           pTestDir->IsInput() ? "input" : "output",
           pRoute->GetString("    ").c_str());

    string failStrBase = "Error : Mismatch at block %u, offset %u on %s destination surface\n"
                         "        Got 0x%x, expected 0x%x\n";

    if (!pTestDir->IsHwLocal())
    {
        failStrBase += Utility::StrPrintf("        Remote Device : %s\n",
                                          pRoute->GetRemoteDevString().c_str());
    }
    failStrBase += Utility::StrPrintf("        Route         :\n%s\n\n",
                                      pRoute->GetString("          ").c_str());

    TestDirectionDataLwda * pTdLwda;
    pTdLwda =  static_cast<TestDirectionDataLwda *>(pTestDir.get());

    return pTdLwda->CheckDestinationSurface(GetNumErrorsToPrint(),
                                            failStrBase);
}

//------------------------------------------------------------------------------
/* virtual */ TestMode::TestDirectionDataPtr TestModeLwda::CreateTestDirectionData()
{
    auto tdData = make_unique<TestDirectionDataLwda>(GetAllocMgr());
    tdData->SetAddCpuTraffic(GetAddCpuTraffic());
    tdData->SetSysmemLoopback(GetSysmemLoopback());
    return TestDirectionDataPtr(move(tdData));
}

//------------------------------------------------------------------------------
// Get the bandwidth of the SMs which will vary depending upon kernel and launch config
RC TestModeLwda::GetBandwidthPerSmKiBps
(
     GpuSubdevice *pSubdev
    ,UINT32 *pReadBwPerSmKiBps
    ,UINT32 *pWriteBwPerSmKiBps
)
{
    // TODO (CE2D version)
    MASSERT(m_SubtestType == ST_GUPS);

    // Numbers that were determined experimentally.
    //
    // The GUPS kernel has an unroll factor for unrolling loops and the achievable
    // bandwidth is determined by that unroll factor (1, 2, 4, 8) which is what
    // the index into these arrays is.
    //
    // These arrays contain the approximate bandwidth that each SM will achieve
    // per MHz of GPC clock per U32 written.
    //
    // I.e. if the transfer width is 4 bytes, the GPC clock was 780MHz, and the
    // unroll factor was 2, then the SM bandwidth would be:
    //
    //   Read : 1628.906 * 780 * 4 / 4 = ~ 1270546 KiBps
    //   Write : 1940.43 * 780 * 4 / 4 = ~ 1513535 KiBps
    //
    // However if the transfer width was 16 bytes, the GPC clock was 1455MHz and
    // the unroll factor was 8, then the SM bandwidth would be:
    //
    //   Read : 2080.078 * 1455 * 16 / 4 = ~ 12106054 KiBps
    //   Write : 1942.383 * 1455 * 16 / 4 = ~ 11304669 KiBps
    //
    static const vector<FLOAT64> s_WriteBwKiBpsPerSmPerGpcMhzPerU32 =
        { 1816.406, 1940.43, 1941.406, 1942.383 };
    static const vector<FLOAT64> s_ReadBwKiBpsPerSmPerGpcMhzPerU32 =
        { 1303.711, 1628.906, 1888.672, 2080.078 };

    RC rc;
    UINT64 gpcClockFreq;
    if (pSubdev->IsEmOrSim())
    {
        static const UINT64 POR_GPCCLK_FREQ = 1800000000ULL;
        gpcClockFreq = POR_GPCCLK_FREQ;
    }
    else
    {
        CHECK_RC(pSubdev->GetClock(Gpu::ClkGpc, &gpcClockFreq, nullptr, nullptr, nullptr));
    }

    UINT32 idx = static_cast<UINT32>(Utility::BitScanForward(m_GupsUnroll));
    TransferWidth effectiveTw = m_TransferWidth;

    // when using linear transfers each SM's effective transfer width as far as bandwidth
    if (m_GupsMemIdx == LwLinkKernel::LINEAR)
        effectiveTw = TW_4BYTES;

    *pReadBwPerSmKiBps  = static_cast<UINT32>(static_cast<FLOAT64>(gpcClockFreq) *
                                              s_ReadBwKiBpsPerSmPerGpcMhzPerU32[idx] *
                                              effectiveTw / 4.0 /
                                              1.0e6);
    *pWriteBwPerSmKiBps = static_cast<UINT32>(static_cast<FLOAT64>(gpcClockFreq) *
                                              s_WriteBwKiBpsPerSmPerGpcMhzPerU32[idx] *
                                              effectiveTw / 4.0 /
                                              1.0e6);
    return OK;
}

//------------------------------------------------------------------------------
UINT64 TestModeLwda::GetMemorySize
(
    TestRoutePtr pRoute
   ,GpuTestConfiguration *pTestConfig
)
{
    UINT64 memorySize;

    switch (m_SubtestType)
    {
        case ST_CE2D:
            memorySize  = pTestConfig->SurfaceWidth();
            memorySize *= pTestConfig->SurfaceHeight();
            memorySize *= sizeof(UINT32);
            memorySize *= m_Ce2dMemorySizeFactor;
            break;

        default:
        case ST_GUPS:
            memorySize = m_GupsMemorySize;
            if (memorySize)
                break;

            FrameBuffer *pFb = pRoute->GetLocalSubdevice()->GetFB();
            UINT64 minimumL2Size = pFb ? pFb->GetL2CacheSize() : 0;
            if (pRoute->GetTransferType() == TT_P2P)
            {
                pFb = pRoute->GetRemoteSubdevice()->GetFB();
                if (pFb)
                {
                    minimumL2Size = minimumL2Size ? min(static_cast<UINT64>(pFb->GetL2CacheSize()),
                                                                            minimumL2Size) :
                                                    pFb->GetL2CacheSize();
                }
            }

            // Callwlate the amount of memory per-link for this particular route
            auto pLwLink = pRoute->GetLocalLwLinkDev()->GetInterface<LwLink>();
            memorySize = minimumL2Size / pLwLink->GetMaxLinks() / 2;
            memorySize *= pRoute->GetWidth();

            if (!memorySize)
                memorySize = m_TransferWidth * DEFAULT_GUPS_TRANSFER_COUNT;

            memorySize = ALIGN_DOWN(memorySize, static_cast<UINT64>(m_TransferWidth));
            break;

    }

    return memorySize;
}

//------------------------------------------------------------------------------
RC TestModeLwda::GetSmInfo(TestRouteLwda *pTrLwda, Lwca::Device dev, SmInfo *pSmInfo)
{
    AllocMgrLwda::LwdaFunction *pFunction;
    AllocMgrLwda *pAllocMgr = static_cast<AllocMgrLwda *>(GetAllocMgr().get());

    // The amount of launchable threads and therefore all SM assignment will be
    // different depending upon the GPU as well as the function that is being
    // launched
    RC rc;
    if (!m_pLwda->IsContextInitialized(dev))
    {
        CHECK_RC(m_pLwda->InitContext(dev));
    }
    CHECK_RC(pAllocMgr->AcquireFunction(dev,  GetFunctionName(m_SubtestType), &pFunction));

    DEFER() { pAllocMgr->ReleaseFunction(pFunction); };

    UINT32 gridSize = 0;
    if (pTrLwda->GetLocalLwdaDevice().GetHandle() == dev.GetHandle())
    {
        pSmInfo->maxSms = pTrLwda->GetLocalMaxSms();
        // Get the maximum block size for launching the function.  By passing
        // in the total number of threads available on the device as the
        // maximum block size, LWCA will return full oclwpancy values.  I.e.
        // threadsPerSm will contain the maximum number of threads that can be
        // launched per SM and gridSize will be the maximum number of of grids
        // that can be launched (which should equal the total number of SMs on
        // the device)
        CHECK_RC(pFunction->func.GetMaxBlockSize(&pSmInfo->threadsPerSm,
                                                 &gridSize,
                                                 pTrLwda->GetLocalMaxThreads()));
        return rc;
    }
    pSmInfo->maxSms = pTrLwda->GetRemoteMaxSms();
    CHECK_RC(pFunction->func.GetMaxBlockSize(&pSmInfo->threadsPerSm,
                                             &gridSize,
                                             pTrLwda->GetRemoteMaxThreads()));
    return rc;
}

//------------------------------------------------------------------------------
//! \brief Setup the copies for the given test direction
//!
//! \param pTestDir : Pointer to test direction to setup copies for
//!
//! \return OK if successful, not OK if setting up the channels failed
RC TestModeLwda::SetupCopies(TestDirectionDataPtr pTestDir)
{
    if (!pTestDir->IsInUse())
        return OK;

    LwLinkKernel::CopyTriggers *pCopyTrigger =
        static_cast<LwLinkKernel::CopyTriggers *>(m_CopyTriggers.GetPtr());
    pCopyTrigger->bWaitStart = true;
    pCopyTrigger->bWaitStop  = true;

    return OK;
}

//------------------------------------------------------------------------------
/* virtual */ RC TestModeLwda::TriggerAllCopies()
{
    if (m_WarmupSec != 0)
        Tasker::Sleep(m_WarmupSec * 1000.0);
    LwLinkKernel::CopyTriggers *pCopyTrigger =
        static_cast<LwLinkKernel::CopyTriggers *>(m_CopyTriggers.GetPtr());
    pCopyTrigger->bWaitStart = false;
    return OK;
}

//------------------------------------------------------------------------------
RC TestModeLwda::WaitForCopies(FLOAT64 timeoutMs)
{
    StickyRC rc;

    if (m_RuntimeSec != 0)
        Tasker::Sleep(m_RuntimeSec * 1000.0);

    LwLinkKernel::CopyTriggers *pCopyTrigger =
        static_cast<LwLinkKernel::CopyTriggers *>(m_CopyTriggers.GetPtr());
    pCopyTrigger->bWaitStop = false;
    for (auto const & testRteData : GetTestData())
    {
        TestDirectionDataLwda * pTdLwda;
        pTdLwda = static_cast<TestDirectionDataLwda *>(testRteData.second.pDutInTestData.get());
        if (pTdLwda->IsInUse())
            rc = pTdLwda->Synchronize();

        pTdLwda = static_cast<TestDirectionDataLwda *>(testRteData.second.pDutOutTestData.get());
        if (pTdLwda->IsInUse())
            rc = pTdLwda->Synchronize();
    }

    return rc;
}

//------------------------------------------------------------------------------
// TestDirectionDataLwda public implementation
//------------------------------------------------------------------------------

//------------------------------------------------------------------------------
TestModeLwda::TestDirectionDataLwda::TestDirectionDataLwda
(
    AllocMgrPtr pAllocMgr
) : TestDirectionData(pAllocMgr)
   ,m_pAllocMgr(static_cast<AllocMgrLwda *>(pAllocMgr.get()))
{
}

//------------------------------------------------------------------------------
RC TestModeLwda::TestDirectionDataLwda::Acquire
(
    TransferType tt
   ,Lwca::HostMemory *pCopyTriggers
   ,GpuDevice *pLocDev
   ,Lwca::Device locLwdaDev
   ,Lwca::Device remLwdaDev
   ,GpuTestConfiguration *pTestConfig
   ,UINT64 memorySize
)
{
    RC rc;

    if (!IsInUse())
        return OK;

    // GUPS test only uses one surface (destination)
    if (m_SubtestType != ST_GUPS)
    {
        switch (tt)
        {
            case TT_SYSMEM:
                {
                    // DUT Input testing requires source surface in sysmem and
                    // destination in FB, DUT Output testing is the reverse
                    const Memory::Location loc =
                        (GetSysmemLoopback() || IsInput()) ? Memory::Coherent : Memory::Fb;
                    CHECK_RC(m_pAllocMgr->AcquireMemory(locLwdaDev,
                                                        memorySize,
                                                        loc,
                                                        &m_pSrcMem));
                }
                break;
            case TT_LOOPBACK:
                // For loopback both surfaces are in FB
                CHECK_RC(m_pAllocMgr->AcquireMemory(locLwdaDev,
                                                    memorySize,
                                                    Memory::Fb,
                                                    &m_pSrcMem));
                if (IsInput())
                {
                    CHECK_RC(m_pSrcMem->MapLoopback());
                }
                break;
            case TT_P2P:
                // For P2P testing, Input testing requires source be acquired on
                // the remote device and destination be acquired on the DUT.
                // Output testing is the reverse
                CHECK_RC(m_pAllocMgr->AcquireMemory(IsInput() ? remLwdaDev : locLwdaDev,
                                                    memorySize,
                                                    Memory::Fb,
                                                    &m_pSrcMem));
                break;
            default:
                Printf(Tee::PriError,
                       "%s : Unknown transfer type %d!\n",
                       __FUNCTION__, tt);
                return RC::SOFTWARE_ERROR;
        }
    }

    switch (tt)
    {
        case TT_SYSMEM:
            {
                const Memory::Location loc =
                    ((m_SubtestType == ST_GUPS) || GetSysmemLoopback() || !IsInput()) ?
                        Memory::Coherent : Memory::Fb;
                CHECK_RC(m_pAllocMgr->AcquireMemory(locLwdaDev,
                                                    memorySize,
                                                    loc,
                                                    &m_pDstMem));
            }
            break;
        case TT_LOOPBACK:
            // For loopback both surfaces are in FB
            CHECK_RC(m_pAllocMgr->AcquireMemory(locLwdaDev,
                                                memorySize,
                                                Memory::Fb,
                                                &m_pDstMem));
            if ((m_SubtestType == ST_GUPS) || !IsInput())
            {
                CHECK_RC(m_pDstMem->MapLoopback());
            }
            break;
        case TT_P2P:
            {
                // For P2P testing, Input testing requires source be acquired on
                // the remote device and destination be acquired on the DUT.
                // Output testing is the reverse
                Lwca::Device allocDev = IsInput() ? locLwdaDev : remLwdaDev;
                if (m_SubtestType == ST_GUPS)
                    allocDev = IsHwLocal() ? remLwdaDev : locLwdaDev;
                else
                    allocDev = IsInput() ? locLwdaDev : remLwdaDev;
                CHECK_RC(m_pAllocMgr->AcquireMemory(allocDev,
                                                    memorySize,
                                                    Memory::Fb,
                                                    &m_pDstMem));
            }
            break;
        default:
            Printf(Tee::PriError,
                   "%s : Unknown transfer type %d!\n",
                   __FUNCTION__, tt);
            return RC::SOFTWARE_ERROR;
    }

    Lwca::Device hwDev = IsHwLocal() ? locLwdaDev : remLwdaDev;
    if (!hwDev.IsValid())
    {
        Printf(Tee::PriError,
               "%s : Cannot acquire transfer HW on non-existent remote device!\n",
               __FUNCTION__);
        return RC::SOFTWARE_ERROR;
    }

    CHECK_RC(m_pAllocMgr->AcquireFunction(hwDev, GetFunctionName(m_SubtestType), &m_pFunction));

    m_pFunction->func.SetBlockDim(m_ThreadsPerSm);
    m_pFunction->func.SetGridDim(m_SmCount);

    CHECK_RC(m_pAllocMgr->AcquireMemory(hwDev,
                                        m_SmCount*sizeof(LwLinkKernel::CopyStats),
                                        Memory::Coherent,
                                        &m_pCopyStatsMem));
    m_pCopyTriggers = pCopyTriggers->GetPtr();
    m_DevCopyTriggers = pCopyTriggers->GetDevicePtr(hwDev);

    CHECK_RC(TestMode::TestDirectionData::Acquire(tt, pLocDev, pTestConfig));
    return rc;
}

//------------------------------------------------------------------------------
RC TestModeLwda::TestDirectionDataLwda::CheckDestinationSurface
(
    UINT32 numErrorsToPrint,
    const string &failStrBase
)
{
    StickyRC rc;

    // Start by assuming the surfaces are already in sysmem
   AllocMgrLwda::LwdaMemory * pGoldMem = m_pSrcMem;
   AllocMgrLwda::LwdaMemory * pTestMem = m_pDstMem;

    // If either the source or destination memory is not in sysmem
    // acquire new system memory to copy the data to for verification
    if (m_pSrcMem->GetLocation() == Memory::Fb)
    {
        CHECK_RC(m_pAllocMgr->AcquireMemory(m_pSrcMem->GetDevice(),
                                            m_pSrcMem->GetSize(),
                                            Memory::Coherent,
                                            &pGoldMem));
    }

    DEFERNAME(releaseGold)
    {
        m_pAllocMgr->ReleaseMemory(pGoldMem);
    };
    if (m_pDstMem->GetLocation() == Memory::Fb)
    {
        CHECK_RC(m_pAllocMgr->AcquireMemory(m_pDstMem->GetDevice(),
                                            m_pDstMem->GetSize(),
                                            Memory::Coherent,
                                            &pTestMem));
    }
    DEFERNAME(releaseTest)
    {
        m_pAllocMgr->ReleaseMemory(pTestMem);
    };

    // Copy the appropriate surfaces to sysmem
    if (m_pSrcMem != pGoldMem)
        CHECK_RC(m_pSrcMem->Get(pGoldMem));
    else
        releaseGold.Cancel();
    if (m_pDstMem != pTestMem)
        CHECK_RC(m_pDstMem->Get(pTestMem));
    else
        releaseTest.Cancel();

    UINT32 errorCount = 0;
    const size_t blockMemSize = pTestMem->GetSize() * m_SmCount / m_SmCount;
    // The full height of the surface isnt always copied.  Only check the portion
    // of the surface that the m_Copies affect
    for (UINT32 dstBlock = 0; dstBlock < m_SmCount && (errorCount < numErrorsToPrint); dstBlock++)
    {
        // TODO (CE2D version) UINT32 srcBlock  = m_SmCount - (m_TotalIterations % warpSize);
        UINT32 srcBlock = 0;
        srcBlock %= m_SmCount;

        UINT32 *srcMem = static_cast<UINT32 *>(pGoldMem->GetPtr()) +
                             srcBlock * blockMemSize / sizeof(UINT32);
        UINT32 *dstMem = static_cast<UINT32 *>(pTestMem->GetPtr()) +
                             dstBlock * blockMemSize / sizeof(UINT32);

        for (size_t lwrIdx = 0;
             (lwrIdx < blockMemSize) && (errorCount < numErrorsToPrint);
             lwrIdx += sizeof(UINT32))
        {
            if (srcMem[lwrIdx] != dstMem[lwrIdx])
            {
                Printf(Tee::PriError,
                       failStrBase.c_str(),
                       dstBlock,
                       static_cast<UINT32>(lwrIdx),
                       IsInput() ? "input" : "output",
                       dstMem[lwrIdx],
                       srcMem[lwrIdx]);
                rc = RC::BUFFER_MISMATCH;
                errorCount++;
            }
        }
    }

    return rc;
}

//------------------------------------------------------------------------------
void TestModeLwda::TestDirectionDataLwda::ConfigureGups
(
    UINT32 unroll
   ,bool bAtomicWrite
   ,GupsMemIndexType gupsMemIdx
)
{
    if (!IsInUse())
        return;

    m_GupsConfig.transferWidth =
        (gupsMemIdx == LwLinkKernel::RANDOM) ? static_cast<UINT32>(m_TransferWidth) :
                                               TW_4BYTES;
    m_GupsConfig.testData      = static_cast<device_ptr>(m_pDstMem->GetDevicePtr());
    m_GupsConfig.testDataSize  = static_cast<UINT32>(m_pDstMem->GetSize());
    m_GupsConfig.memIdxType    = gupsMemIdx;
    m_GupsConfig.unroll        = static_cast<UINT32>(unroll);
    m_GupsConfig.copyStats     = static_cast<device_ptr>(m_pCopyStatsMem->GetDevicePtr());
    m_GupsConfig.copyTriggers  = static_cast<device_ptr>(m_DevCopyTriggers);
    m_GupsConfig.testType      = (IsHwLocal() == IsInput()) ? LwLinkKernel::READ :
                                                              LwLinkKernel::WRITE;
    if ((m_GupsConfig.testType == LwLinkKernel::WRITE) && bAtomicWrite)
        m_GupsConfig.testType = LwLinkKernel::ATOMIC;
}

//------------------------------------------------------------------------------
RC TestModeLwda::TestDirectionDataLwda::DumpMemory(string fnameBase) const
{
    RC rc;

    string fname;

    FileHolder f;
    if (m_pSrcMem->GetSize())
    {
        fname = fnameBase + "_src.png";
        CHECK_RC(f.Open(fname, "wb"));
        CHECK_RC(Utility::FWrite(m_pSrcMem->GetPtr(), m_pSrcMem->GetSize(), 1, f.GetFile()));
    }

    fname = fnameBase + "_dst.png";
    CHECK_RC(f.Open(fname, "wb"));
    CHECK_RC(Utility::FWrite(m_pDstMem->GetPtr(), m_pDstMem->GetSize(), 1, f.GetFile()));

    return rc;
}

//------------------------------------------------------------------------------
/* virtual */ string TestModeLwda::TestDirectionDataLwda::GetBandwidthFailString() const
{
    // GUPS write bandwidth may be capped due to credits.  Return this specific
    // information if this is the case
    if (IsInUse() && (m_SubtestType == ST_GUPS) && (IsHwLocal() != IsInput()))
    {
        return "\n******* GUPS Write Bandwidth may be capped due to LwLink credits *******\n";
    }
    return "";
}

//------------------------------------------------------------------------------
/* virtual */ UINT64 TestModeLwda::TestDirectionDataLwda::GetBytesPerLoop() const
{
    return m_pDstMem->GetSize();
}

//------------------------------------------------------------------------------
//! \brief Get a string containing the SM information for the specified test
//! direction
string TestModeLwda::TestDirectionDataLwda::GetSmString() const
{
    if (!IsInUse())
        return "N/A";
    return Utility::StrPrintf("%c(%u) (%u)", IsHwLocal() ? 'L' : 'R',
                              m_SmCount,
                              m_SmCount * m_ThreadsPerSm);
}

//------------------------------------------------------------------------------
/* virtual */ UINT32 TestModeLwda::TestDirectionDataLwda::GetTransferWidth() const
{
    return static_cast<UINT32>(m_TransferWidth);
}

//------------------------------------------------------------------------------
/* virtual */ bool TestModeLwda::TestDirectionDataLwda::IsInUse() const
{
    return TestDirectionData::IsInUse() || (m_SmCount != 0);
}

//------------------------------------------------------------------------------
/* virtual */ RC TestModeLwda::TestDirectionDataLwda::Release
(
    bool *pbResourcesReleased
)
{
    if (!IsInUse())
        return OK;

    StickyRC rc;

    if (m_pSrcMem)
    {
        rc = m_pSrcMem->UnmapLoopback();
        rc = m_pAllocMgr->ReleaseMemory(m_pSrcMem);
        m_pSrcMem = nullptr;
        *pbResourcesReleased = true;
    }
    if (m_pDstMem)
    {
        rc = m_pDstMem->UnmapLoopback();
        rc = m_pAllocMgr->ReleaseMemory(m_pDstMem);
        m_pDstMem = nullptr;
        *pbResourcesReleased = true;
    }
    if (m_pCopyStatsMem)
    {
        rc = m_pAllocMgr->ReleaseMemory(m_pCopyStatsMem);
        m_pCopyStatsMem = nullptr;
        *pbResourcesReleased = true;
    }
    if (m_pFunction)
    {
        rc = m_pAllocMgr->ReleaseFunction(m_pFunction);
        m_pFunction = nullptr;
        *pbResourcesReleased = true;
    }

    rc = TestMode::TestDirectionData::Release(pbResourcesReleased);

    return rc;
}

//------------------------------------------------------------------------------
/* virtual */ void TestModeLwda::TestDirectionDataLwda::Reset()
{
    m_SmCount      = 0;
    m_ThreadsPerSm = 0;
    TestDirectionData::Reset();
}

//------------------------------------------------------------------------------
//! \brief Setup the test surfaces for use in the particular test direction
//!
//! \param pTestDir : Pointer to the test direction
//!
//! \return OK if setup was successful, not OK otherwise
RC TestModeLwda::TestDirectionDataLwda::SetupTestSurfaces
(
    GpuTestConfiguration *pTestConfig
)
{
    if (!IsInUse())
        return OK;

    RC rc;

    // TODO (CE2D version) - Initialize copy data

    CHECK_RC(TestMode::TestDirectionData::SetupTestSurfaces(pTestConfig));

    return rc;
}

//------------------------------------------------------------------------------
//! \brief Start all copies on a test direction (i.e. flush all channels).
//!
//! \param pTestDir    : Pointer to test direction to flush channels on
//!
//! \return OK if successful, not OK if flushes failed
RC TestModeLwda::TestDirectionDataLwda::StartCopies()
{
    if (!IsInUse())
        return OK;

    RC rc;
    CHECK_RC(TestMode::TestDirectionData::StartCopies());

    switch (m_SubtestType)
    {
        case ST_GUPS:
            CHECK_RC(m_pFunction->func.Launch(m_pFunction->stream, m_GupsConfig));
            break;
        case ST_CE2D:
            Printf(Tee::PriError, "%s : CE2D emulation version not supported %d\n",
                   __FUNCTION__, m_SubtestType);
            return RC::UNSUPPORTED_FUNCTION;
        default:
            Printf(Tee::PriError, "%s : Unknown suptest type %d\n", __FUNCTION__, m_SubtestType);
            return RC::SOFTWARE_ERROR;
    }

    return rc;
}

//------------------------------------------------------------------------------
RC TestModeLwda::TestDirectionDataLwda::Synchronize()
{
    if (!IsInUse())
        return OK;

    return m_pFunction->stream.Synchronize();
}

//------------------------------------------------------------------------------
//! \brief Update copy statistics for the specified test direction
//!
//! \param pTestDir : Pointer to the test direction
//!
RC TestModeLwda::TestDirectionDataLwda::UpdateCopyStats(UINT32 numLoops)
{
    if (!IsInUse())
        return OK;

    LwLinkKernel::CopyStats *pCopyStats =
        static_cast<LwLinkKernel::CopyStats *>(m_pCopyStatsMem->GetPtr());

    LwLinkKernel::CopyStats totalStats = { };
    for (UINT32 i = 0; i < m_SmCount; i++, pCopyStats++)
    {
        totalStats.nanoseconds += pCopyStats->nanoseconds;
        totalStats.bytes       += pCopyStats->bytes;
    }
    // Increment the total bytes written so far for this direction
    AddCopyBytes(static_cast<UINT64>(totalStats.bytes));
    AddTimeNs(static_cast<UINT64>(totalStats.nanoseconds) / m_SmCount);
    return OK;
}

