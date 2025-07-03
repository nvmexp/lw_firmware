/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2009-2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#include "pextest.h"

#include "core/include/jsonlog.h"
#include "core/include/mgrmgr.h"
#include "core/include/utility.h"
#include "core/include/version.h"
#include "device/interface/pcie.h"
#include "gpu/include/gpudev.h"
#include "gpu/include/gpusbdev.h"
#include "gpu/utility/pcie/pexdev.h"

//-----------------------------------------------------------------------------
PexTest::PexTest()
 : m_pPcie(NULL),
   m_SavedId(0),
   m_UpStreamDepth(0),
   m_ExtraAspmToTest(Pci::ASPM_DISABLED),
   m_PexVerbose(false),
   m_UseInternalCounters(false),
   m_UsePollPcieStatus(false),
   m_CheckCountAfterGetError(true),
   m_PStateLockRequired(true),
   m_EnablePexGenArbitration(false),
   m_IgnorePexErrors(false),
   m_IgnorePexHwCounters(false),
   m_DisplayPexCounts(false),
   m_AllowCoverageHole(false),
   m_PexInfoToRestore(PexDevMgr::REST_ALL),
   m_AllowedLineErrors(0),
   m_AllowedCRC(0),
   m_AllowedFailedL0sExits(0),
   m_AllowedNAKsRcvd(0),
   m_AllowedNAKsSent(0)
{
}
//-----------------------------------------------------------------------------
PexTest::~PexTest()
{
}

//-----------------------------------------------------------------------------
RC PexTest::LockGpuPStates()
{
    return m_PStateOwner.ClaimPStates(GetBoundGpuSubdevice());
}

//-----------------------------------------------------------------------------
RC PexTest::Setup()
{
    RC rc;
    CHECK_RC(GpuTest::Setup());
    m_pPcie = GetBoundTestDevice()->GetInterface<Pcie>();
    m_PexErrors.clear();
    m_AllowedCorrLinkError = m_TestConfig.PexCorrErrsAllowed();
    m_AllowedNonFatalError = m_TestConfig.PexNonFatalErrsAllowed();
    m_AllowedUnSuppReq     = m_TestConfig.PexUnSuppReqAllowed();
    m_AllowedLineErrors    = m_TestConfig.PexLineErrorsAllowed();
    m_AllowedCRC           = m_TestConfig.PexCRCErrorsAllowed();
    m_AllowedNAKsRcvd      = m_TestConfig.PexNAKsRcvdAllowed();
    m_AllowedNAKsSent      = m_TestConfig.PexNAKsSentAllowed();
    m_AllowedFailedL0sExits = m_TestConfig.PexFailedL0sExitsAllowed();
    PexDevMgr* pPexDevMgr = (PexDevMgr*)DevMgrMgr::d_PexDevMgr;
    CHECK_RC(pPexDevMgr->SavePexSetting(&m_SavedId,m_PexInfoToRestore));
    // setup data structures that store PEX errors and origial PEX settings
    CHECK_RC(SetupPcieErrors(GetBoundTestDevice()));

    if (m_PStateLockRequired && GetBoundGpuSubdevice())
    {
        CHECK_RC(LockGpuPStates());
        if (!m_EnablePexGenArbitration)
        {
            Perf* pPerf = GetBoundGpuSubdevice()->GetPerf();
            if (pPerf->IsPState30Supported() &&
                pPerf->ClkDomainType(Gpu::ClkPexGen) != Perf::FIXED)
            {
                CHECK_RC(pPerf->DisableArbitration(Gpu::ClkPexGen));
            }
        }
    }
    return rc;
}

//-----------------------------------------------------------------------------
void PexTest::ReleaseGpuPStates()
{
    m_PStateOwner.ReleasePStates();
}

//-----------------------------------------------------------------------------
RC PexTest::Cleanup()
{
    StickyRC rc;
    if (m_PStateLockRequired && GetBoundGpuSubdevice())
    {
        if (!m_EnablePexGenArbitration)
        {
            Perf* pPerf = GetBoundGpuSubdevice()->GetPerf();
            if (pPerf->IsPState30Supported() &&
                pPerf->ClkDomainType(Gpu::ClkPexGen) != Perf::FIXED)
            {
                rc = pPerf->EnableArbitration(Gpu::ClkPexGen);
            }
        }
        ReleaseGpuPStates();
    }
    rc = GpuTest::Cleanup();
    return rc;
}
//-----------------------------------------------------------------------------
bool PexTest::IsSupported()
{
    // SOC GPUs are not on PCI bus
    if (GetBoundTestDevice()->IsSOC())
    {
        return false;
    }

    if (m_PStateLockRequired && GetBoundGpuSubdevice() &&
        GetBoundGpuSubdevice()->GetPerf()->IsOwned())
    {
        Printf(Tee::PriNormal, "Skipping %s because another test is holding pstates\n",
                GetName().c_str());
        return false;
    }
    return true;
}

//-----------------------------------------------------------------------------
//! \brief Identify the type of ModsTest subclass
//!
/* virtual */ bool PexTest::IsTestType(TestType tt)
{
    return (tt == PEX_TEST) || GpuTest::IsTestType(tt);
}

//-----------------------------------------------------------------------------
RC PexTest::Run()
{
    PexDevMgr* pPexDevMgr = (PexDevMgr*)DevMgrMgr::d_PexDevMgr;
    // request from bug 529707:
    // Attempt to run the PEX test with L0s (or L1 in future). If the test
    // fails, turn off ASPM L0s on upstream port and attempt to re-run the test
    // again. If test passes in the second run, let user know that the failure
    // is due to ASPM
    vector<ForcedAspm>ForcedList;
    RC rc;
    if (OK != (rc = ParseForceAspmArgs(&ForcedList)))
        return rc;

    StickyRC finalRc;
    bool oldUsePollPexStatus = false;

    TestDevicePtr pTestDevice = GetBoundTestDevice();
    oldUsePollPexStatus = m_pPcie->UsePolledErrors();
    m_pPcie->SetUsePolledErrors(m_UsePollPcieStatus);

    PexDevice *pUpStreamDev = nullptr;
    finalRc = m_pPcie->GetUpStreamInfo(&pUpStreamDev);
    if (pUpStreamDev)
    {
        while (pUpStreamDev)
        {
            pUpStreamDev->SetUsePollPcieStatus(m_UsePollPcieStatus);
            pUpStreamDev = pUpStreamDev->GetUpStreamDev();
        }

        if (ForcedList.size() == 0)
        {
            // m_UpStreamDepth is based on boards.js
            for (UINT32 j = 0; j <= m_UpStreamDepth; j++)
            {
                finalRc = pPexDevMgr->SetAspm(pTestDevice,
                                              j,
                                              m_ExtraAspmToTest,
                                              m_ExtraAspmToTest,
                                              false /*don't force ASPM*/);
            }
        }
        else
        {
            for (UINT32 j = 0; j < ForcedList.size(); j++)
            {
                finalRc = pPexDevMgr->SetAspm(pTestDevice,
                                              ForcedList[j].Depth,
                                              ForcedList[j].LocAspm,
                                              ForcedList[j].HostAspm,
                                              true);
            }
        }
    }

    finalRc = RunTest();

    bool TestAspm = (m_ExtraAspmToTest & Pci::ASPM_L0S_L1) != 0;
    if (TestAspm)
    {
        if (finalRc != OK)
        {
            Printf(Tee::PriNormal, "Reruning %s original PEX setting\n",
                   GetName().c_str());

            finalRc = pPexDevMgr->RestorePexSetting(m_SavedId,
                                                    m_PexInfoToRestore,
                                                    false /*don't remove session or else the last would fail*/,
                                                    GetBoundGpuDevice());

            // clear the counts first:
            for (UINT32 i = 0; i < m_PexErrors.size(); i++)
            {
                m_PexErrors[i].ClearCount();
            }

            RC retryRc = RunTest();
            if (OK == retryRc)
            {
                Printf(Tee::PriNormal, "Test passed with original PEX setting. "
                                       "Likely test failed the first time due to ASPM.\n");
                finalRc.Clear();
                finalRc = RC::PCIE_BUS_ERROR;
            }
        }
    }

    if (m_PStateLockRequired)
    {
        finalRc = pPexDevMgr->RestorePexSetting(m_SavedId,
                                                m_PexInfoToRestore,
                                                true /*remove session*/,
                                                GetBoundGpuDevice());
    }
    // clear errors since PEX tests are already 'counted' them
    // we need this specifically for some tests where we don't want to look at
    // the debug counters. Make sure all the counters are cleared in this case

    // Only reset the error counters if the PEX test is actually counting
    // errors from hardware counters.  This allows counting of hardware
    // counter PEX errors outside of the PEX test (specifically used
    // to implement rate based thresholds)
    PexDevice* pPexDev = nullptr;
    if (!m_IgnorePexHwCounters && (pTestDevice->BusType() == Gpu::PciExpress))
    {
        UINT32 pexPort = ~0U;

        finalRc = m_pPcie->ResetErrorCounters();
        CHECK_RC(m_pPcie->GetUpStreamInfo(&pPexDev, &pexPort));
        while (pPexDev)
        {
            pPexDev->ResetErrors(pexPort);
            pexPort = pPexDev->GetParentDPortIdx();
            pPexDev = pPexDev->GetUpStreamDev();
        }
    }
    m_pPcie->SetUsePolledErrors(oldUsePollPexStatus);

    CHECK_RC(m_pPcie->GetUpStreamInfo(&pPexDev));
    while (pPexDev)
    {
        pPexDev->SetUsePollPcieStatus(oldUsePollPexStatus);
        pPexDev = pPexDev->GetUpStreamDev();
    }

    return finalRc;
}

//-----------------------------------------------------------------------------
RC PexTest::SetupPcieErrors(GpuSubdevice* pSubdev)
{
    return SetupPcieErrors(pSubdev->GetTestDevice());
}

//-----------------------------------------------------------------------------
RC PexTest::SetupPcieErrors(TestDevicePtr pTestDevice)
{
    if (pTestDevice->BusType() != Gpu::PciExpress)
    {
        return OK;
    }

    RC rc;
    PexDevice* pPexDev = 0;
    UINT32 upStreamId = 0;
    CHECK_RC(m_pPcie->GetUpStreamInfo(&pPexDev, &upStreamId));

    PexErrorsList newErrorList(1);

    // Add error counters for upstream PCI bridges
    if (pPexDev)
    {
        newErrorList.ReSize(pPexDev->GetDepth() + 1);
        UINT32 lwrrState;
        CHECK_RC(pPexDev->GetDownStreamAspmState(&lwrrState, upStreamId));
        m_UpStreamAspmCap.push_back(pPexDev->GetAspmCap());
    }

    const UINT32 inst = pTestDevice->DevInst();
    if (m_PexErrors.size() < inst)
    {
        PexErrorsList dummy(0);
        m_PexErrors.resize(inst, dummy);
    }
    if (m_PexErrors.size() == inst)
    {
        m_PexErrors.push_back(newErrorList);
    }
    else
    {
        m_PexErrors[inst] = newErrorList;
    }

    return rc;
}

//-----------------------------------------------------------------------------
RC PexTest::GetPexErrors(TestDevicePtr pTestDevice, PexTest::PexErrorsList** ppErrorList)
{
    const UINT32 inst = pTestDevice->DevInst();
    if (inst < m_PexErrors.size())
    {
        PexErrorsList* const pErrorList = &m_PexErrors[inst];
        if (pErrorList->m_ListLength > 0)
        {
            *ppErrorList = pErrorList;
            return OK;
        }
    }
    Printf(Tee::PriNormal,
           "PCI error counters not set up for dev %u\n",
           inst);
    return RC::SOFTWARE_ERROR;
}

//-----------------------------------------------------------------------------
RC PexTest::GetPcieErrors(GpuSubdevice* pSubdevice)
{
    return GetPcieErrors(pSubdevice->GetTestDevice());
}

//-----------------------------------------------------------------------------
RC PexTest::GetPcieErrors(TestDevicePtr pTestDevice)
{
    MASSERT(pTestDevice);
    if (m_IgnorePexErrors || (pTestDevice->BusType() != Gpu::PciExpress))
    {
        return OK;
    }

    RC rc;
    UINT32 Depth = 0;
    PexErrorsList* pErrorList = 0;
    CHECK_RC(GetPexErrors(pTestDevice, &pErrorList));
    MASSERT(pErrorList);

    CallbackArguments args;

    args.Push(pTestDevice->DevInst());
    CHECK_RC(FireCallbacks(PRE_PEX_CHECK, Callbacks::STOP_ON_ERROR,
                           args, "Pre-Pex Check:"));
    CHECK_RC(CountPcieCtrlErrors(pTestDevice, NULL, pErrorList, 0));

    // move upwards until root (in the future, we will also check the
    // link speed and link width all the way up to root)
    Depth = 1;
    PexDevice* pPexDev;
    CHECK_RC(m_pPcie->GetUpStreamInfo(&pPexDev));
    while (pPexDev && !pPexDev->IsRoot())
    {
        CHECK_RC(CountPcieCtrlErrors(pTestDevice, pPexDev, pErrorList, Depth));
        pPexDev = pPexDev->GetUpStreamDev();
        Depth++;
    }

    if (m_CheckCountAfterGetError)
    {
        CHECK_RC(CheckPcieErrors(pTestDevice));
    }
    return rc;
}

//-----------------------------------------------------------------------------
RC PexTest::CheckPcieErrors(GpuSubdevice* pSubdevice)
{
    return CheckPcieErrors(pSubdevice->GetTestDevice());
}

//-----------------------------------------------------------------------------
RC PexTest::CheckPcieErrors(TestDevicePtr pTestDevice)
{
    RC rc;
    StickyRC pexErrorRc;
    MASSERT(pTestDevice);
    JsonItem jsi;
    jsi.SetCategory(JsonItem::JSONLOG_PEXINFO);
    jsi.SetJsonLimitedAllowAllFields(true);
    jsi.SetTag("pex_error_threshold_exceeded");

    if (m_IgnorePexErrors || (pTestDevice->BusType() != Gpu::PciExpress))
    {
        return OK;
    }

    PexErrorsList* pErrorList = 0;
    CHECK_RC(GetPexErrors(pTestDevice, &pErrorList));
    MASSERT(pErrorList);

    for (UINT32 i = 0;
         (i < pErrorList->m_ListLength) && (m_AllowedCorrLinkError.size() > i);
         i++)
    {
        PexErrorCounts *pLocCounts = &pErrorList->m_LocalCounts[i];
        PexErrorCounts *pHostCounts = &pErrorList->m_HostCounts[i];
        PexErrorCounts  LocThresh;
        PexErrorCounts  HostThresh;
        rc.Clear();

        // Setup the local thresolds so that a simple comparison can be done
        // to determine whether thresholds have been exceeded
        CHECK_RC(LocThresh.SetCount(PexErrorCounts::CORR_ID,
                                    true,
                                    m_AllowedCorrLinkError[i].LocError));
        CHECK_RC(LocThresh.SetCount(PexErrorCounts::NON_FATAL_ID,
                                    true,
                                    m_AllowedNonFatalError[i].LocError));
        CHECK_RC(LocThresh.SetCount(PexErrorCounts::FATAL_ID,
                                    true,
                                    0));
        CHECK_RC(LocThresh.SetCount(PexErrorCounts::UNSUP_REQ_ID,
                                    true,
                                    m_AllowedUnSuppReq[i].LocError));
        if (m_UseInternalCounters)
        {
            CHECK_RC(LocThresh.SetCount(PexErrorCounts::DETAILED_LINEERROR_ID,
                                        true,
                                        m_AllowedLineErrors));
            CHECK_RC(LocThresh.SetCount(PexErrorCounts::DETAILED_CRC_ID,
                                        true,
                                        m_AllowedCRC));
            CHECK_RC(LocThresh.SetCount(PexErrorCounts::DETAILED_NAKS_R_ID,
                                        true,
                                        m_AllowedNAKsRcvd));
            CHECK_RC(LocThresh.SetCount(PexErrorCounts::DETAILED_NAKS_S_ID,
                                        true,
                                        m_AllowedNAKsSent));
            CHECK_RC(LocThresh.SetCount(PexErrorCounts::DETAILED_FAILEDL0SEXITS_ID,
                                        true,
                                        m_AllowedFailedL0sExits));
        }

        // Host and local thresolds are the same except correctable,
        // unsupported requests, and non-fatal are settable on a per-link basis
        HostThresh = LocThresh;
        CHECK_RC(HostThresh.SetCount(PexErrorCounts::CORR_ID,
                                     true,
                                     m_AllowedCorrLinkError[i].HostError));
        CHECK_RC(HostThresh.SetCount(PexErrorCounts::UNSUP_REQ_ID,
                                     true,
                                     m_AllowedUnSuppReq[i].HostError));
        CHECK_RC(HostThresh.SetCount(PexErrorCounts::NON_FATAL_ID,
                                     true,
                                     m_AllowedNonFatalError[i].HostError));

        // Compare the thresholds.  This compares all thresholds that are valid
        // in both the counts and threshold structures.
        if ((*pLocCounts > LocThresh) || (*pHostCounts > HostThresh))
        {
            rc = RC::PCIE_BUS_ERROR;
            pexErrorRc = rc;
        }

        bool bPrintedDepth = false;
        if (m_TestConfig.Verbose() || m_DisplayPexCounts || rc != OK)
        {
            string errString = "";
            string infoString = "";
            // For each error do the print if errors were collected
            // during this test.  Maintain backwards compatibility with
            // prints by alternating between host/local prints on the same
            // error type
            for (UINT32 errIdx = PexErrorCounts::CORR_ID;
                  errIdx < PexErrorCounts::NUM_ERR_COUNTERS; errIdx++)
            {
                if (!m_UseInternalCounters &&
                    PexErrorCounts::IsInternalOnly(errIdx))
                {
                    continue;
                }

                string ErrString = PexErrorCounts::GetString(errIdx);
                for (UINT32 locHost = 0; locHost < 2; locHost++)
                {
                    PexErrorCounts *pErrors = locHost ? pHostCounts :
                                                        pLocCounts;

                    if (!(pErrors->IsValid(errIdx)))
                        continue;

                    if (!pErrors->IsThreshold(errIdx) &&
                        (Utility::GetSelwrityUnlockLevel() < Utility::SUL_LWIDIA_NETWORK))
                    {
                        continue;
                    }

                    if (!bPrintedDepth)
                    {
                        errString +=
                            Utility::StrPrintf("At depth %d from dev %u:\n",
                                               i, pTestDevice->DevInst());
                        jsi.SetField("dev", pTestDevice->DevInst());
                        jsi.SetField("depth", i);

                        bPrintedDepth = true;
                    }

                    PexErrorCounts *pThresh = locHost ? &HostThresh :
                                                        &LocThresh;
                    UINT32 ErrCount = pErrors->GetCount(errIdx);
                    UINT32 ErrThresh = pThresh->GetCount(errIdx);

                    if (!(pErrors->IsThreshold(errIdx)))
                    {
                        infoString +=
                            Utility::StrPrintf(
                               "    %s %s = %u\n",
                               locHost ? "Host" : "Local",
                               ErrString.c_str(),
                               ErrCount);
                        string OneWordErrString = ErrString;
                        OneWordErrString.erase(remove(OneWordErrString.begin(),
                                                      OneWordErrString.end(), ' '),
                                                      OneWordErrString.end());
                        OneWordErrString.erase(remove(OneWordErrString.begin(),
                                                      OneWordErrString.end(), '-'),
                                                      OneWordErrString.end());
                        jsi.SetField(((locHost ? string("host_") : string("local_"))
                                       + OneWordErrString).c_str(), ErrCount);
                    }
                    else if (ErrThresh != (UINT32)~0)
                    {
                        errString +=
                            Utility::StrPrintf("  %s %s (allowed) = %u (%u)\n",
                               locHost ? "Host" : "Local",
                               ErrString.c_str(),
                               ErrCount, ErrThresh);
                        string OneWordErrString = ErrString;
                        OneWordErrString.erase(remove(OneWordErrString.begin(),
                                                      OneWordErrString.end(), ' '),
                                                      OneWordErrString.end());
                        OneWordErrString.erase(remove(OneWordErrString.begin(),
                                                      OneWordErrString.end(), '-'),
                                                      OneWordErrString.end());
                        jsi.SetField(((locHost ? string("host_") : string("loc_"))
                                       + OneWordErrString).c_str(), ErrCount);
                        jsi.SetField(((locHost ? string("host_") : string("loc_")) +
                                     OneWordErrString + "_allowed").c_str(), ErrThresh);
                    }
                    else
                    {
                        errString +=
                            Utility::StrPrintf("  %s %s (allowed) = %u (no limit)\n",
                               locHost ? "Host" : "Local",
                               ErrString.c_str(),
                               ErrCount);
                        string OneWordErrString = ErrString;
                        OneWordErrString.erase(remove(OneWordErrString.begin(),
                                                      OneWordErrString.end(), ' '),
                                                      OneWordErrString.end());
                        OneWordErrString.erase(remove(OneWordErrString.begin(),
                                                      OneWordErrString.end(), '-'),
                                                      OneWordErrString.end());
                        jsi.SetField(((locHost ? string("host_") : string("loc_"))
                                       + OneWordErrString).c_str(), ErrCount);
                        jsi.SetField(((locHost ? string("host_") : string("loc_")) +
                                     OneWordErrString + "_allowed").c_str(), "unlimited");
                    }
                }
            }
            if (infoString != "")
            {
                errString += "  ***** Non-threshold counts *****\n";
                errString += infoString;
            }
            Printf(Tee::PriHigh, "%s", errString.c_str());

            GetJsonExit()->SetField("pexdev_depth", i);
            if (pLocCounts->IsValid(PexErrorCounts::CORR_ID))
            {
                GetJsonExit()->SetField("PEX_CE_loc",
                          pLocCounts->GetCount(PexErrorCounts::CORR_ID));
            }
            if (pHostCounts->IsValid(PexErrorCounts::CORR_ID))
            {
                GetJsonExit()->SetField("PEX_CE_host",
                          pHostCounts->GetCount(PexErrorCounts::CORR_ID));
            }
            if (OK != rc && JsonLogStream::IsOpen())
            {
                jsi.WriteToLogfile();
            }
        }
    }
    return (OK == rc) ? pexErrorRc : rc;
}
//-----------------------------------------------------------------------------
UINT32 PexTest::UpStreamDepth()
{
    return m_UpStreamDepth;
}

UINT32 PexTest::AllowCoverageHole(UINT32 GpuSpeedCap)
{
    PexDevice *pUpStreamDev;
    UINT32 port;
    if (!m_AllowCoverageHole ||
        (OK != m_pPcie->GetUpStreamInfo(&pUpStreamDev, &port)) ||
        (nullptr == pUpStreamDev))
    {
        return GpuSpeedCap;
    }

    UINT32 UpstreamSpeed = pUpStreamDev->GetDownStreamLinkSpeedCap(port);
    Printf(Tee::PriNormal,
            "Warning: Test only what upstream supports: %d\n",
            UpstreamSpeed);
    if (GpuSpeedCap > UpstreamSpeed)
    {
        GpuSpeedCap = UpstreamSpeed;
    }

    return GpuSpeedCap;
}
//-----------------------------------------------------------------------------
// Format of parsed string: "'Depth_LocAspm_HostAspm|Depth_LocAspm_HostAspm"
// This is passed in through -testarg TestNum
// We'll store this in a list of forced ASPM parameters
//  Example: -testarg 30 ForceAspm "'0_1_1|1_1_2'"
// More info here:
// https://wiki.lwpu.com/engwiki/index.php/MODS/PEX#testargs_for_PEX_tests
RC PexTest::ParseForceAspmArgs(vector<ForcedAspm> *pForcedAspmList)
{
    if (m_ForceAspm.size() == 0)
    {   // no force ASPM
        return OK;
    }

    RC rc;
    vector<string> Tokens;
    CHECK_RC(Utility::Tokenizer(m_ForceAspm, "_|", &Tokens));
    if (Tokens.size() % 3)
    {
        Printf(Tee::PriNormal, "invalid parameters for force ASPM\n");
        return RC::BAD_PARAMETER;
    }

    for (UINT32 i = 0; i < Tokens.size(); i+=3)
    {
        ForcedAspm NewParam = {0};
        NewParam.Depth    = strtoul(Tokens[i].c_str(), NULL, 10);
        NewParam.LocAspm  = strtoul(Tokens[i + 1].c_str(), NULL, 10);
        NewParam.HostAspm = strtoul(Tokens[i + 2].c_str(), NULL, 10);
        pForcedAspmList->push_back(NewParam);
    }

    return rc;
}

//-----------------------------------------------------------------------------
RC PexTest::CountPcieCtrlErrors(TestDevicePtr  pTestDevice,
                                PexDevice    * pPexDev,
                                PexErrorsList* pErrorList,
                                UINT32         Depth)
{
    MASSERT(pErrorList && pTestDevice && (pPexDev || (Depth == 0)));
    RC rc;
    StickyRC firstRc;
    PexErrorCounts LocCounters;
    PexErrorCounts HostCounters;

    if (pErrorList->m_ListLength <= Depth)
    {
        return RC::SOFTWARE_ERROR;
    }

    if (Depth == 0)
    {
        firstRc = m_pPcie->GetErrorCounts(
                &LocCounters,
                &HostCounters,
                m_IgnorePexHwCounters ? PexDevice::PEX_COUNTER_FLAG :
                                        PexDevice::PEX_COUNTER_ALL);
    }
    else
    {
        firstRc = pPexDev->GetUpStreamErrorCounts(
                &LocCounters,
                &HostCounters,
                m_IgnorePexHwCounters ? PexDevice::PEX_COUNTER_FLAG :
                                        PexDevice::PEX_COUNTER_ALL);
    }

    // PexErrorCounts implements "+=" to support automatic aclwmulation of
    // counts
    pErrorList->m_LocalCounts[Depth] += LocCounters;
    pErrorList->m_HostCounts[Depth]  += HostCounters;

    PexErrorCounts Thresh;

    CHECK_RC(Thresh.SetCount(PexErrorCounts::FATAL_ID, true, 0));
    CHECK_RC(Thresh.SetCount(PexErrorCounts::NON_FATAL_ID, true, 0));

    // fire a callback for any new correctable, fatal, non-fatal, and
    // unsupported requests
    CHECK_RC(Thresh.SetCount(PexErrorCounts::CORR_ID, true, 0));
    CHECK_RC(Thresh.SetCount(PexErrorCounts::UNSUP_REQ_ID, true, 0));

    if ((LocCounters > Thresh) || (HostCounters > Thresh))
    {
        CallbackArguments args;
        args.Push(pTestDevice->DevInst());
        args.Push(LocCounters.GetCount(PexErrorCounts::CORR_ID));
        args.Push(HostCounters.GetCount(PexErrorCounts::CORR_ID));
        args.Push(LocCounters.GetCount(PexErrorCounts::FATAL_ID));
        args.Push(HostCounters.GetCount(PexErrorCounts::FATAL_ID));
        args.Push(LocCounters.GetCount(PexErrorCounts::NON_FATAL_ID));
        args.Push(HostCounters.GetCount(PexErrorCounts::NON_FATAL_ID));
        args.Push(LocCounters.GetCount(PexErrorCounts::UNSUP_REQ_ID));
        args.Push(HostCounters.GetCount(PexErrorCounts::UNSUP_REQ_ID));
        args.Push(Depth);
        firstRc = FireCallbacks(PEX_ERROR, Callbacks::RUN_ON_ERROR,
                                args, "Pex Error:");
    }

    return firstRc;
}

//-----------------------------------------------------------------------------
RC PexTest::DisableAspm()
{
    RC rc;

    auto pPcie = GetBoundTestDevice()->GetInterface<Pcie>();

    CHECK_RC(pPcie->SetAspmState(0));

    UINT32 portId = 0;
    PexDevice* pUpStreamDev = nullptr;
    CHECK_RC(pPcie->GetUpStreamInfo(&pUpStreamDev, &portId));
    if (pUpStreamDev)
    {
        CHECK_RC(pUpStreamDev->SetDownStreamAspmState(0, portId, true));
    }

    // set aspm for the rest of the devices
    while (pUpStreamDev && !pUpStreamDev->IsRoot())
    {
        CHECK_RC(pUpStreamDev->SetUpStreamAspmState(0, true));
        portId       = pUpStreamDev->GetUpStreamIndex();
        pUpStreamDev = pUpStreamDev->GetUpStreamDev();
        if (pUpStreamDev)
        {
            CHECK_RC(pUpStreamDev->SetDownStreamAspmState(0, portId, true));
        }
    }

    return rc;
}

//------------------------------------------------------------------------------
// PexErrorsList Constructor and destructor
//------------------------------------------------------------------------------
PexTest::PexErrorsList::PexErrorsList(UINT32 Length)
{
    ReSize(Length);
}
void PexTest::PexErrorsList::ReSize(UINT32 Length)
{
    m_ListLength = Length;
    PexErrorCounts EmptyCounts;
    m_LocalCounts.resize(Length, EmptyCounts);
    m_HostCounts.resize(Length, EmptyCounts);
}
void PexTest::PexErrorsList::ClearCount()
{
    MASSERT(m_LocalCounts.size() == m_HostCounts.size());
    PexErrorCounts EmptyCounts;
    for (UINT32 i = 0 ; i < m_LocalCounts.size(); i++)
    {
        m_LocalCounts[i] = EmptyCounts;
        m_HostCounts[i]  = EmptyCounts;
    }
}

//-----------------------------------------------------------------------------

JS_CLASS_VIRTUAL_INHERIT(PexTest, GpuTest, "PEX test base class");

CLASS_PROP_READWRITE(PexTest, CheckCountAfterGetError, bool,
                     "bool: check pcie count after retrieving PCIE errors");
CLASS_PROP_READWRITE(PexTest, UsePollPcieStatus, bool,
                     "bool: enable polling of PCIE status register instead of debug counters");
CLASS_PROP_READWRITE(PexTest, UseInternalCounters, bool,
                     "bool: enable internal counters in PEX reports");
CLASS_PROP_READWRITE(PexTest, ExtraAspmToTest, UINT32,
                     "UINT32: add to current ASPM setting (OR operation)");
CLASS_PROP_READWRITE(PexTest, ForceAspm, string,
                     "string: Force an ASPM setting");
CLASS_PROP_READWRITE(PexTest, UpStreamDepth, UINT32,
                     "UINT32: number of BR04s on the board");
CLASS_PROP_READWRITE(PexTest, PStateLockRequired, bool,
                     "bool: whether or not we need to lock the PState while running the test");
CLASS_PROP_READWRITE(PexTest, EnablePexGenArbitration, bool,
                     "bool: enable PCIE gen speed perf arbitration");
CLASS_PROP_READWRITE(PexTest, PexInfoToRestore, UINT32,
                     "UINT32: The set of PEX info that the test should restore - ASLM, Speed, ASPM");
CLASS_PROP_READWRITE(PexTest, IgnorePexErrors, bool,
                     "bool: option to disable PCIE error check");
CLASS_PROP_READWRITE(PexTest, IgnorePexHwCounters, bool,
                     "bool: option to disable PCIE error check for PCIE HW counters");
CLASS_PROP_READWRITE(PexTest, AllowCoverageHole, bool,
                             "bool: Test at most what the chipset supports");
CLASS_PROP_READWRITE(PexTest, DisplayPexCounts, bool,
                             "bool: Display only PEX counts");

