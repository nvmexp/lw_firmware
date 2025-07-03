/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2021-2022 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#include "gpu/tests/iobandwidth.h"

#include "class/clc6b5.h" // AMPERE_DMA_COPY_A
#include "class/clc7b5.h" // AMPERE_DMA_COPY_A
#include "class/clc8b5.h" // HOPPER_DMA_COPY_A
#include "core/include/color.h"
#include "core/include/memtypes.h"
#include "core/include/mgrmgr.h"
#include "core/include/platform.h"
#include "core/include/tee.h"
#include "core/include/utility.h"
#include "gpu/include/floorsweepimpl.h"
#include "gpu/include/gpudev.h"
#include "gpu/include/gpumgr.h"
#include "gpu/include/gpusbdev.h"
#include "gpu/include/testdevice.h"
#include "gpu/include/testdevicemgr.h"
#include "gpu/tests/gputest.h"
#include "gpu/tests/gputestc.h"
#include "gpu/utility/surf2d.h"


// -----------------------------------------------------------------------------
IOBandwidth::IOBandwidth()
{
    m_pTestConfig = GetTestConfiguration();
}

// -----------------------------------------------------------------------------
bool IOBandwidth::IsSupported()
{
    if (GetBoundGpuSubdevice()->IsSOC())
    {
        Printf(Tee::PriLow, "%s : Not supported on SOC\n", GetName().c_str());
        return false;
    }
    if (GetBoundGpuDevice()->GetNumSubdevices() > 1)
    {
        Printf(Tee::PriLow, "%s : Not supported on SLI devices\n", GetName().c_str());
        return false;
    }

    if (GetRemoteTestDevice(&m_pRemoteDevice) != RC::OK)
    {
        Printf(Tee::PriLow, "%s : No remote device found to test\n", GetName().c_str());
        return false;
    }

    return true;
}

// -----------------------------------------------------------------------------
RC IOBandwidth::Setup()
{
    RC rc;

    if (m_pTestConfig->Loops() == 0)
    {
        Printf(Tee::PriError, "%s : Loops are 0, nothing will be tested\n", GetName().c_str());
        return RC::NO_TESTS_RUN;
    }

    CHECK_RC(GpuTest::Setup());

    CHECK_RC(GetRemoteTestDevice(&m_pRemoteDevice));

    CHECK_RC(ConfigureCopyEngines(GetBoundGpuSubdevice(), &m_LocalConfig));

    if (m_pRemoteDevice)
    {
        GpuSubdevice *pPeerSub = m_pRemoteDevice->GetInterface<GpuSubdevice>();

        if (pPeerSub->GetParentDevice()->GetNumSubdevices() > 1)
        {
            Printf(Tee::PriLow, "%s : Not supported on SLI devices\n", GetName().c_str());
            return RC::UNSUPPORTED_SYSTEM_CONFIG;
        }

        if (m_pRemoteDevice != GetBoundTestDevice())
        {
            CHECK_RC(ConfigureCopyEngines(pPeerSub, &m_RemoteConfig));
        }
        else
        {
            m_bLoopback = true;
        }

        if (!UsePhysAddrCopies())
        {
            CHECK_RC(DevMgrMgr::d_GraphDevMgr->AddPeerMapping(GetBoundRmClient(),
                                                              GetBoundGpuSubdevice(),
                                                              pPeerSub));
        }
    }

    m_LocalConfig.surfaces.resize(SURF_COUNT);
    CHECK_RC(SetupSurfaces(&m_LocalConfig));

    CHECK_RC(m_LocalConfig.dmaWrap.Initialize(m_pTestConfig,
                                             m_pTestConfig->NotifierLocation(),
                                             DmaWrapper::COPY_ENGINE,
                                             LW2080_ENGINE_TYPE_COPY_IDX(m_LocalConfig.engineId)));
    m_LocalConfig.dmaWrap.SetUsePipelinedCopies(true);
    CHECK_RC(m_LocalConfig.dmaWrap.SaveCopyTime(true));

    if (!m_bLoopback)
    {
        m_RemoteConfig.surfaces.resize(SURF_COUNT);
        CHECK_RC(SetupSurfaces(&m_RemoteConfig));

        if (m_pRemoteDevice && !m_bLoopback)
        {
            TestDevicePtr pOrigTestDev = GetBoundTestDevice();
            DEFER { m_pTestConfig->BindTestDevice(pOrigTestDev); };
            m_pTestConfig->BindTestDevice(m_pRemoteDevice);
            CHECK_RC(m_RemoteConfig.dmaWrap.Initialize(m_pTestConfig,
                                            m_pTestConfig->NotifierLocation(),
                                            DmaWrapper::COPY_ENGINE,
                                            LW2080_ENGINE_TYPE_COPY_IDX(m_RemoteConfig.engineId)));
            m_RemoteConfig.dmaWrap.SetUsePipelinedCopies(true);
            CHECK_RC(m_RemoteConfig.dmaWrap.SaveCopyTime(true));
        }
    }

    return rc;
}

// -----------------------------------------------------------------------------
RC IOBandwidth::Cleanup()
{
    StickyRC rc;

    rc = m_LocalConfig.dmaWrap.Cleanup();
    rc = m_RemoteConfig.dmaWrap.Cleanup();

    for (UINT32 lwrSurf = SRC_SURF_A; lwrSurf < SURF_COUNT; lwrSurf++)
    {
        if (!m_LocalConfig.surfaces.empty())
        {
            m_LocalConfig.surfaces[lwrSurf].Free();
        }
        if (!m_RemoteConfig.surfaces.empty())
        {
            m_RemoteConfig.surfaces[lwrSurf].Free();
        }
    }

    if (!UsePhysAddrCopies())
    {
        rc = DevMgrMgr::d_GraphDevMgr->RemoveAllPeerMappings(GetBoundGpuSubdevice());
    }

    LwRm * pLwRm = GetBoundRmClient();
    if (m_LocalConfig.bCeRemapped)
    {
        rc = pLwRm->ControlBySubdevice(GetBoundGpuSubdevice(),
                                       LW2080_CTRL_CMD_CE_SET_PCE_LCE_CONFIG,
                                       &m_LocalConfig.ceRestore,
                                       sizeof(m_LocalConfig.ceRestore));
    }
    if (m_RemoteConfig.bCeRemapped)
    {
        rc = pLwRm->ControlBySubdevice(m_pRemoteDevice->GetInterface<GpuSubdevice>(),
                                       LW2080_CTRL_CMD_CE_SET_PCE_LCE_CONFIG,
                                       &m_RemoteConfig.ceRestore,
                                       sizeof(m_RemoteConfig.ceRestore));
    }
    return RC::OK;
}

// -----------------------------------------------------------------------------
void IOBandwidth::PrintJsProperties(Tee::Priority pri)
{
    GpuTest::PrintJsProperties(pri);

    Printf(pri, "IOBandwidth Js Properties:\n");
    string ttStr;
    switch (m_TransferType)
    {
        case TransferType::READS:            ttStr = "Reads"; break;
        case TransferType::WRITES:           ttStr = "Writes"; break;
        case TransferType::READS_AND_WRITES: ttStr = "Reads/Writes"; break;
        default:
            MASSERT(!"Unknown transfer type");
    }
    Printf(pri, "\tTransferType:                   %s\n",   ttStr.c_str());
    Printf(pri, "\tCopiesPerLoop:                  %u\n",   m_CopiesPerLoop);
    Printf(pri, "\tSurfSizeFactor:                 %u\n",   m_SurfSizeFactor);
    Printf(pri, "\tSrcSurfMode:                    %u\n",   m_SrcSurfMode);
    Printf(pri, "\tSrcSurfModePercent:             %u\n",   m_SrcSurfModePercent);
    Printf(pri, "\tShowBandwidthData:              %s\n",   m_ShowBandwidthData ? "true" : "false");  //$
    Printf(pri, "\tSkipBandwidthCheck:             %s\n",   m_SkipBandwidthCheck ? "true" : "false"); //$
    Printf(pri, "\tSkipSurfaceCheck:               %s\n",   m_SkipSurfaceCheck ? "true" : "false");   //$
    Printf(pri, "\tNumErrorsToPrint:               %u\n",   m_NumErrorsToPrint);
    Printf(pri, "\tIgnorePceRequirements:          %s\n",   m_IgnorePceRequirements ? "true" : "false");   //$
    Printf(pri, "\tRuntimeMs:                      %u\n",   m_RuntimeMs);
    
}

// -----------------------------------------------------------------------------
RC IOBandwidth::AllocateSurface(Surface2D * pSurf, GpuDevice * pGpuDev, Memory::Location memLoc)
{
    const ColorUtils::Format colorFormat =
        ColorUtils::ColorDepthToFormat(m_pTestConfig->DisplayDepth());
    const UINT32 totalSurfHeight = m_pTestConfig->SurfaceHeight() * m_SurfSizeFactor;

    pSurf->SetWidth(m_pTestConfig->SurfaceWidth());
    pSurf->SetHeight(totalSurfHeight);
    pSurf->SetColorFormat(colorFormat);
    pSurf->SetProtect(Memory::ReadWrite);
    pSurf->SetLayout(Surface2D::Pitch);
    pSurf->SetLocation(memLoc);
    return pSurf->Alloc(pGpuDev);
}

// -----------------------------------------------------------------------------
RC IOBandwidth::CheckBandwidth(TransferType tt, UINT64 elapsedTimeUs, CeLocation ceLoc, UINT32 loop)
{
    RC rc;

    const UINT64 rawBwKiBps = GetRawBandwidthKiBps();

    const UINT32 transferWidth = m_pTestConfig->SurfaceWidth() *
                                 m_pTestConfig->DisplayDepth() / 8;
    const UINT64 bytesPerCopy = static_cast<UINT64>(transferWidth) *
                                m_pTestConfig->SurfaceHeight() *
                                m_SurfSizeFactor;
    const UINT64 totalBytes = bytesPerCopy * loop * m_CopiesPerLoop;
    UINT64 actualBwKiBps = 0;
    if (elapsedTimeUs != 0ULL)
    {
        actualBwKiBps = static_cast<UINT64>(totalBytes * 1e6 / 1024ULL / elapsedTimeUs);
    }

    string bwSummaryString =
        Utility::StrPrintf("Testing %s on %s\n"
                           "---------------------------------------------------\n",
                           GetTestedInterface(),
                           GetDeviceString(GetBoundTestDevice()).c_str());

    UINT64 dataBwKiBps = GetDataBandwidthKiBps(tt);
    if (m_bLoopback)
    {
        // Loopback and remote GPU (bidirectional traffic) are considered R/W
        dataBwKiBps = GetDataBandwidthKiBps(TransferType::READS_AND_WRITES);

        bwSummaryString += Utility::StrPrintf("Remote Device                   = %s\n",
                                              GetDeviceString(GetBoundTestDevice()).c_str());
        bwSummaryString +=
            Utility::StrPrintf("Transfer Method                 = Loopback Unidirectional %s\n",
                               (tt == TransferType::READS) ? "Read" : "Write");
    }
    else if (m_pRemoteDevice)
    {
        // Loopback and remote GPU (bidirectional traffic) are considered R/W
        dataBwKiBps = GetDataBandwidthKiBps(TransferType::READS_AND_WRITES);

        bwSummaryString += Utility::StrPrintf("Remote Device                   = %s\n",
                                              GetDeviceString(m_pRemoteDevice).c_str());
        bwSummaryString +=
            Utility::StrPrintf("Transfer Method                 = Bidirectional %s\n",
                               (tt == TransferType::READS) ? "Read" : "Write");
        bwSummaryString +=
            Utility::StrPrintf("Transfer Source                 = %s\n",
                               (ceLoc == CeLocation::LOCAL) ?
                                   GetBoundTestDevice()->GetName().c_str() :
                                   m_pRemoteDevice->GetName().c_str());
    }
    else
    {
        bwSummaryString += Utility::StrPrintf("Remote Device                   = Sysmem\n");
        bwSummaryString +=
            Utility::StrPrintf("Transfer Method                 = Unidirectional %s\n",
                               (tt == TransferType::READS) ? "Read" : "Write");
    }
    const UINT64 minimumBwKiBps = dataBwKiBps * GetBandwidthThresholdPercent(tt) / 100ULL;
    const FLOAT64 percentRawBw = static_cast<FLOAT64>(actualBwKiBps) * 100.0 / dataBwKiBps;

    if (!m_pRemoteDevice || m_bLoopback)
    {
        bwSummaryString += Utility::StrPrintf("Transfer Source                 = %s\n",
                                              GetBoundTestDevice()->GetName().c_str());
    }
    if (m_bLoopback || m_pRemoteDevice)
    {
        bwSummaryString += Utility::StrPrintf("Transfer Direction              = I/O\n");
    }
    else
    {
        bwSummaryString += Utility::StrPrintf("Transfer Direction              = %s\n",
                                              (tt == TransferType::READS) ? "I" : "O");
    }
    bwSummaryString += Utility::StrPrintf("Transfer Width                  = %u\n",
                                          transferWidth);
    if (tt == TransferType::READS)
    {
        bwSummaryString += Utility::StrPrintf("Measurement Direction           = %s\n",
                                              (ceLoc == CeLocation::LOCAL) ? "I" : "O");
    }
    else
    {
        bwSummaryString += Utility::StrPrintf("Measurement Direction           = %s\n",
                                              (ceLoc == CeLocation::LOCAL) ? "O" : "I");
    }
    bwSummaryString += Utility::StrPrintf("Bytes Per Copy                  = %llu\n"
                                          "Total Bytes Transferred         = %llu\n"
                                          "Elapsed Time                    = %.3f ms\n",
                                          bytesPerCopy,
                                          totalBytes,
                                          static_cast<FLOAT64>(elapsedTimeUs) / 1000.0);
    bwSummaryString += GetInterfaceBwSummary();
    bwSummaryString += Utility::StrPrintf("Total Raw Bandwidth             = %llu KiBps\n"
                                          "Maximum Data Bandwidth          = %llu KiBps\n"
                                          "Bandwidth Threshold             = %.1f%%\n"
                                          "Minimum Bandwidth               = %llu KiBps\n",
                                          rawBwKiBps,
                                          dataBwKiBps,
                                          static_cast<FLOAT32>(GetBandwidthThresholdPercent(tt)),
                                          minimumBwKiBps);

    if (elapsedTimeUs != 0ULL)
    {
        bwSummaryString += Utility::StrPrintf("Actual Bandwidth                = %llu KiBps\n"
                                              "Percent Raw Bandwidth           = %.1f%%\n",
                                              actualBwKiBps,
                                              percentRawBw);
    }
    else
    {
        bwSummaryString += "Actual Bandwidth                = Unknown\n"
                           "Percent Raw Bandwidth           = Unknown\n"
                           "\n******* Elapsed time is 0, unable to callwlate bandwidth *******\n";
    }

    if (actualBwKiBps < minimumBwKiBps)
        bwSummaryString += "\n******* Bandwidth too low! *******\n";

    const bool bBwFail = ((actualBwKiBps < minimumBwKiBps) && !m_SkipBandwidthCheck);

    Tee::Priority pri = m_ShowBandwidthData ? Tee::PriNormal : GetVerbosePrintPri();
    if (bBwFail)
        pri = m_SkipBandwidthCheck ? Tee::PriWarn : Tee::PriError;

    Printf(pri, "%s\n", bwSummaryString.c_str());
    return bBwFail ? RC::BANDWIDTH_OUT_OF_RANGE : RC::OK;
}

// -----------------------------------------------------------------------------
RC IOBandwidth::ConfigureCopyEngines
(
    GpuSubdevice   * pGpuSubdevice,
    EndpointConfig * pEndpointConfig
)
{
    RC rc;

    MASSERT(pGpuSubdevice);
    MASSERT(pEndpointConfig);

    const UINT64 bwKiBps = max(GetDataBandwidthKiBps(TransferType::READS),
                               GetDataBandwidthKiBps(TransferType::WRITES));

    UINT32 ceBwKiBps;
    CHECK_RC(pGpuSubdevice->GetMaxCeBandwidthKiBps(&ceBwKiBps));

    UINT32 requiredPces = static_cast<UINT32>((bwKiBps + (ceBwKiBps - 1)) / ceBwKiBps);

    UINT32 hshubPceMask = pGpuSubdevice->GetHsHubPceMask();
    UINT32 fbhubPceMask = pGpuSubdevice->GetFbHubPceMask();
    UINT32 hshubPceCount = Utility::CountBits(hshubPceMask);

    // Ensure that there are enough HSHUB PCEs in the GPU to saturate the interface
    if (hshubPceCount < requiredPces)
    {
        Printf(m_IgnorePceRequirements ? Tee::PriWarn : Tee::PriError,
               "%s : Not enough HSHUB PCEs to saturate bandwidth on %s, %u available, "
               "%u required\n",
               GetName().c_str(), pGpuSubdevice->GetName().c_str(), hshubPceCount, requiredPces);
        if (!m_IgnorePceRequirements)
            return RC::UNSUPPORTED_SYSTEM_CONFIG;
    }

    vector<UINT32> classIds;
    classIds.push_back(AMPERE_DMA_COPY_A);
    classIds.push_back(AMPERE_DMA_COPY_B);
    classIds.push_back(HOPPER_DMA_COPY_A);

    vector<UINT32> engineList;
    GpuDevice *pGpuDev = pGpuSubdevice->GetParentDevice();
    CHECK_RC(pGpuDev->GetPhysicalEnginesFilteredByClass(classIds, &engineList));

    auto PrintCeInfo = [&] (UINT32 lce, UINT32 pceMask) -> void
    {
        string ceStr = Utility::StrPrintf("%s : Using LCE %u(", GetName().c_str(), lce);
        INT32 lwrPce = Utility::BitScanForward(pceMask);
        string comma;
        while (lwrPce != -1)
        {
            ceStr += comma + Utility::StrPrintf("%d", lwrPce);
            if (comma.empty())
                comma = ",";
            lwrPce = Utility::BitScanForward(pceMask, lwrPce + 1);
        }
        VerbosePrintf("%s) on %s\n",
                      ceStr.c_str(),
                      pGpuSubdevice->GetName().c_str());
    };

    // Check if there is already a LCE that contains the necessary amount of PCEs
    UINT32 maxPceEngine = 0;
    UINT32 maxEnginePceCount = 0;
    UINT32 enginePceMask = 0;
    LwRm * pLwRm = GetBoundRmClient();
    for (auto engine : engineList)
    {
        LW2080_CTRL_CE_GET_CE_PCE_MASK_PARAMS params = { };
        params.ceEngineType = engine;
        params.pceMask = 0x0;
        CHECK_RC(pLwRm->ControlBySubdevice(pGpuSubdevice,
                                           LW2080_CTRL_CMD_CE_GET_CE_PCE_MASK,
                                           &params, sizeof(params)));

        const UINT32 enginePceCount = static_cast<UINT32>(Utility::CountBits(params.pceMask));
        if (enginePceCount >= requiredPces)
        {
            pEndpointConfig->engineId = engine;
            PrintCeInfo(LW2080_ENGINE_TYPE_COPY_IDX(engine), params.pceMask);
            return RC::OK;
        }
        if (enginePceCount > maxEnginePceCount)
        {
            maxPceEngine = engine;
            maxEnginePceCount = enginePceCount;
            enginePceMask = params.pceMask;
        }
    }

    if (!pGpuSubdevice->HasFeature(GpuSubdevice::GPUSUB_SUPPORTS_LCE_PCE_REMAPPING))
    {
        Printf(m_IgnorePceRequirements ? Tee::PriWarn : Tee::PriError,
               "%s : No single LCE found with at least %u PCEs and %s does not support "
               "PCE remapping\n",
               GetName().c_str(), requiredPces, pGpuSubdevice->GetName().c_str());
        if (!m_IgnorePceRequirements)
            return RC::UNSUPPORTED_SYSTEM_CONFIG;

        PrintCeInfo(LW2080_ENGINE_TYPE_COPY_IDX(maxPceEngine), enginePceMask);

        // If it is not possible to reconfigure the LCEs so that there is a LCE with the required
        // number of PCEs choose the LCE with the most PCEs
        pEndpointConfig->engineId = maxPceEngine;
        return RC::OK;
    }

    pEndpointConfig->engineId = ~0U;

    // Search over all engines this time so that GRCE LCEs are not automatically removed
    // It is necessary to track the GRCEs to come up with an appropriate new configuration
    engineList.clear();
    CHECK_RC(pGpuDev->GetEnginesFilteredByClass(classIds, &engineList));
    set<UINT32> grceEngines;

    // Find the first LCE that is not a GRCE and is also even and also was not previously shared
    // as a GRCE.  Some testing on GA100 shows reconfiguring to use a LCE that was previously
    // shared as a GRCE results in lesser bandwidth on the reconfigured LCE
    //
    // LCEs that have > 4 PCEs need to be EVEN LCEs (ODD LCEs are not latency sized on esched size
    // for many PCEs).
    CHECK_RC(pGpuSubdevice->GetPceLceMapping(&pEndpointConfig->ceRestore));
    for (auto engine : engineList)
    {
        LW2080_CTRL_CE_GET_CAPS_V2_PARAMS params = {};
        params.ceEngineType = engine;
        CHECK_RC(pLwRm->ControlBySubdevice(pGpuDev->GetSubdevice(0),
                                           LW2080_CTRL_CMD_CE_GET_CAPS_V2,
                                           &params, sizeof(params)));

        const UINT32 engineIdx = LW2080_ENGINE_TYPE_COPY_IDX(engine);

        if (LW2080_CTRL_CE_GET_CAP(params.capsTbl, LW2080_CTRL_CE_CAPS_CE_GRCE))
        {
            grceEngines.insert(engine);
        }
        else if ((pEndpointConfig->engineId == ~0U) &&
                 (engineIdx != pEndpointConfig->ceRestore.grceSharedLceMap[0]) &&
                 (engineIdx != pEndpointConfig->ceRestore.grceSharedLceMap[1]) &&
                 ((engineIdx % 2) == 0))
        {
            pEndpointConfig->engineId = engine;
        }
    }

    if (pEndpointConfig->engineId == ~0U)
    {
        Printf(m_IgnorePceRequirements ? Tee::PriWarn : Tee::PriError,
               "%s : No even async LCE found on %s\n",
               GetName().c_str(), pGpuSubdevice->GetName().c_str());
        if (!m_IgnorePceRequirements)
            return RC::UNSUPPORTED_SYSTEM_CONFIG;

        // Use the first even engine if there is one, if there is not just use the first engine
        for (auto engine : engineList)
        {
            if ((LW2080_ENGINE_TYPE_COPY_IDX(engine) % 2) == 0)
            {
                pEndpointConfig->engineId = engine;
                break;
            }
        }
        if (pEndpointConfig->engineId == ~0U)
            pEndpointConfig->engineId = engineList[0];

        // Erase the used engine from the grce engines so the reconfig doesnt try and assing
        // a FBHUB pce to it since it will be used for the test itself
        grceEngines.erase(pEndpointConfig->engineId);
    }

    // To avoid creating an overly complex algorithm to reconfigure CEs, assign one PCE to
    // each GRCE and any remaining to the the LCE that will be used for the test
    if ((hshubPceCount + Utility::CountBits(fbhubPceMask)) < (grceEngines.size() + 1))
    {
        Printf(Tee::PriError,
               "%s : Unable to create a valid LCE/PCE configuration on %s\n",
               GetName().c_str(), pGpuSubdevice->GetName().c_str());
        return RC::UNSUPPORTED_SYSTEM_CONFIG;
    }

    LW2080_CTRL_CE_SET_PCE_LCE_CONFIG_PARAMS ceMap = {};
    ceMap.ceEngineType = LW2080_ENGINE_TYPE_COPY0; // This only makes a differnence in SMC mode

    // Assign one PCE to each GRCE.  Originally it was tried to create a single LCE with all the
    // HSHUB PCEs and share that one LCE with all GRCEs but sharing the same LCE with all GRCEs
    // is not a supported HW configuration
    for (auto const &grceEngine : grceEngines)
    {
        INT32 pce = Utility::BitScanForward(fbhubPceMask);
        if (pce != -1)
        {
            fbhubPceMask &= ~(1 << pce);
        }
        else
        {
            pce = Utility::BitScanReverse(hshubPceMask);
            MASSERT(pce != -1);
            hshubPceMask &= ~(1 << pce);
        }
        ceMap.pceLceMap[pce] = LW2080_ENGINE_TYPE_COPY_IDX(grceEngine);
    }

    hshubPceCount = Utility::CountBits(hshubPceMask);
    if (hshubPceCount < requiredPces)
    {
        Printf(m_IgnorePceRequirements ? Tee::PriWarn : Tee::PriError,
               "%s : Not enough HSHUB PCEs to saturate bandwidth on %s after GRCE assignment, "
               "%u available, %u required\n",
               GetName().c_str(),
               pGpuSubdevice->GetName().c_str(),
               hshubPceCount,
               requiredPces);
        if (!m_IgnorePceRequirements)
            return RC::UNSUPPORTED_SYSTEM_CONFIG;
        requiredPces = hshubPceCount;
    }

    INT32 lwrPce = Utility::BitScanForward(hshubPceMask);
    UINT32 mappedCount = 0;
    const UINT32 engineIdx = LW2080_ENGINE_TYPE_COPY_IDX(pEndpointConfig->engineId);

    // Assign all necessary HSHUB PCEs to the engine used for the test
    UINT32 actualPceMask = 0;
    while ((lwrPce != -1) && (mappedCount < requiredPces))
    {
        actualPceMask |= (1 << lwrPce);
        ceMap.pceLceMap[lwrPce] = engineIdx;
        lwrPce = Utility::BitScanForward(hshubPceMask, lwrPce + 1);
        mappedCount++;
    }

    // Mark any unused PCEs as unused
    const UINT32 unusedMask = fbhubPceMask | (hshubPceMask & ~actualPceMask);
    lwrPce = Utility::BitScanForward(unusedMask);
    while (lwrPce != -1)
    {
        ceMap.pceLceMap[lwrPce] = 0xF;
        lwrPce = Utility::BitScanForward(unusedMask, lwrPce + 1);
    }

    // GRCEs are not sharing PCEs with a LCE in the custom config
    ceMap.grceSharedLceMap[0] = 0xF;
    ceMap.grceSharedLceMap[1] = 0xF;

    VerbosePrintf("%s : Remapped PCEs\n", GetName().c_str());
    PrintCeInfo(engineIdx, actualPceMask);

    pEndpointConfig->ceRestore.ceEngineType = pEndpointConfig->engineId;
    CHECK_RC(pLwRm->ControlBySubdevice(pGpuSubdevice,
                                       LW2080_CTRL_CMD_CE_SET_PCE_LCE_CONFIG,
                                       &ceMap, sizeof(ceMap)));
    pEndpointConfig->bCeRemapped = true;
    return rc;
}

// -----------------------------------------------------------------------------
RC IOBandwidth::DoTransfers
(
    TransferType tt,
    const TransferSetup & localSetup,
    const TransferSetup & remoteSetup,
    UINT64 *pLocalElapsedTimeUs,
    UINT64 *pRemoteElapsedTimeUs
)
{
    RC rc;

    for (UINT32 lwrCopy = 0; lwrCopy < m_CopiesPerLoop; lwrCopy++)
    {
        bool bFirstCopy = (lwrCopy == 0);
        bool bLastCopy = (lwrCopy == (m_CopiesPerLoop - 1));
        CHECK_RC(QueueCopy(tt, &m_LocalConfig.dmaWrap, localSetup, bFirstCopy, bLastCopy));
        if (m_pRemoteDevice && !m_bLoopback)
        {
            CHECK_RC(QueueCopy(tt, &m_RemoteConfig.dmaWrap, remoteSetup, bFirstCopy, bLastCopy));
        }
        m_LwrSrcSurf = (m_LwrSrcSurf == SRC_SURF_A) ? SRC_SURF_B : SRC_SURF_A;
    }
    CHECK_RC(m_LocalConfig.dmaWrap.Wait(m_pTestConfig->TimeoutMs()));

    UINT64 startTimeUs, endTimeUs;
    CHECK_RC(m_LocalConfig.dmaWrap.GetCopyTimeUs(&startTimeUs, &endTimeUs));
    *pLocalElapsedTimeUs = endTimeUs - startTimeUs;
    if (m_pRemoteDevice && !m_bLoopback)
    {
        CHECK_RC(m_RemoteConfig.dmaWrap.Wait(m_pTestConfig->TimeoutMs()));
        CHECK_RC(m_RemoteConfig.dmaWrap.GetCopyTimeUs(&startTimeUs, &endTimeUs));
        *pRemoteElapsedTimeUs = endTimeUs - startTimeUs;
    }
    return rc;
}

//-----------------------------------------------------------------------------
// The surfaces are quite large (50Mb), so filling the memory using CPU is
// going to be slow. Attempt to do this faster using copy engine
// First create 1/4 the screen height of the surface with a pattern, then copy
// and paste the pattern to the rest of the surface.
RC IOBandwidth::FillSrcSurfaces(EndpointConfig * pEndpointConfig)
{
    RC rc;

    Surface2D patternSurfA;
    Surface2D patternSurfB;
    CHECK_RC(SetupPatternSurfaces(&patternSurfA, &patternSurfB));
    DEFER { patternSurfA.Free(); patternSurfB.Free(); };

    const bool bLocalConfig = (pEndpointConfig != &m_RemoteConfig);
    const UINT32 topPatternHeight =
        static_cast<UINT32>(m_LocalConfig.surfaces[SRC_SURF_A].GetHeight() *
                            (m_SrcSurfModePercent / 100.0));
    const UINT32 topPatternCount = topPatternHeight / patternSurfA.GetHeight();
    const UINT32 topPatternRemain = topPatternHeight % patternSurfA.GetHeight();

    const UINT32 bottomPatternHeight =
        m_LocalConfig.surfaces[SRC_SURF_A].GetHeight() - topPatternHeight;
    const UINT32 bottomPatternCount = bottomPatternHeight / patternSurfA.GetHeight();
    const UINT32 bottomPatternRemain = bottomPatternHeight % patternSurfA.GetHeight();

    GpuDevice *pOrigGpuDev = GetBoundGpuDevice();
    DEFER { m_pTestConfig->BindGpuDevice(pOrigGpuDev); };

    GpuSubdevice * pGpuSub = GetBoundGpuSubdevice();
    if (!bLocalConfig && m_pRemoteDevice)
    {
        pGpuSub = m_pRemoteDevice->GetInterface<GpuSubdevice>();
        m_pTestConfig->BindGpuDevice(pGpuSub->GetParentDevice());
    }

    DmaWrapper dmaWrap;
    CHECK_RC(dmaWrap.Initialize(m_pTestConfig,
                                m_pTestConfig->NotifierLocation(),
                                DmaWrapper::COPY_ENGINE,
                                LW2080_ENGINE_TYPE_COPY_IDX(pEndpointConfig->engineId)));

    for (UINT32 surfIdx = SRC_SURF_A; surfIdx <= SRC_SURF_B; surfIdx++)
    {
        Surface2D * pTopPattern = (surfIdx == SRC_SURF_A) ? &patternSurfA : &patternSurfB;
        Surface2D * pBottomPattern = (surfIdx == SRC_SURF_A) ? &patternSurfB : &patternSurfA;

        UINT64 topCtxDmaOffset = pTopPattern->GetCtxDmaOffsetGpu();
        UINT64 bottomCtxDmaOffset = pBottomPattern->GetCtxDmaOffsetGpu();
        if (!bLocalConfig && m_pRemoteDevice)
        {
            topCtxDmaOffset = pTopPattern->GetCtxDmaOffsetGpuShared(pGpuSub->GetParentDevice());
            bottomCtxDmaOffset =
                pBottomPattern->GetCtxDmaOffsetGpuShared(pGpuSub->GetParentDevice());
        }

        dmaWrap.SetSurfaces(pTopPattern, &pEndpointConfig->surfaces[surfIdx]);
        CHECK_RC(dmaWrap.SetCtxDmaOffsets(topCtxDmaOffset,
                                      pEndpointConfig->surfaces[surfIdx].GetCtxDmaOffsetGpu()));
        for (UINT32 lwrPattern = 0; lwrPattern < topPatternCount; lwrPattern++)
        {
            CHECK_RC(dmaWrap.Copy(0,
                                  0,
                                  pTopPattern->GetPitch(),
                                  pTopPattern->GetHeight(),
                                  0,
                                  lwrPattern * pTopPattern->GetHeight(),
                                  m_pTestConfig->TimeoutMs(),
                                  pGpuSub->GetSubdeviceInst()));
        }

        if (topPatternRemain)
        {
            CHECK_RC(dmaWrap.Copy(0,
                                  0,
                                  pTopPattern->GetPitch(),
                                  topPatternRemain,
                                  0,
                                  topPatternCount * pTopPattern->GetHeight(),
                                  m_pTestConfig->TimeoutMs(),
                                  pGpuSub->GetSubdeviceInst()));
        }

        dmaWrap.SetSurfaces(pBottomPattern, &pEndpointConfig->surfaces[surfIdx]);
        CHECK_RC(dmaWrap.SetCtxDmaOffsets(bottomCtxDmaOffset,
                                      pEndpointConfig->surfaces[surfIdx].GetCtxDmaOffsetGpu()));
        for (UINT32 lwrPattern = 0; lwrPattern < bottomPatternCount; lwrPattern++)
        {
            const UINT32 dstY = topPatternHeight + lwrPattern * pBottomPattern->GetHeight();
            CHECK_RC(dmaWrap.Copy(0,
                                  0,
                                  pBottomPattern->GetPitch(),
                                  pBottomPattern->GetHeight(),
                                  0,
                                  dstY,
                                  m_pTestConfig->TimeoutMs(),
                                  pGpuSub->GetSubdeviceInst()));
        }

        if (bottomPatternRemain)
        {
            const UINT32 dstY =
                topPatternHeight + bottomPatternCount * pBottomPattern->GetHeight();
            CHECK_RC(dmaWrap.Copy(0,
                                  0,
                                  pTopPattern->GetPitch(),
                                  bottomPatternRemain,
                                  0,
                                  dstY,
                                  m_pTestConfig->TimeoutMs(),
                                  pGpuSub->GetSubdeviceInst()));
        }
    }

    return rc;
}

// -----------------------------------------------------------------------------
RC IOBandwidth::FillSurface
(
     Surface2D* pSurf
    ,PatternType pt
    ,UINT32 patternData
    ,UINT32 patternData2
    ,UINT32 patternHeight
)
{
    RC rc;
    switch (pt)
    {
        case PatternType::BARS:
            CHECK_RC(pSurf->FillPatternRect(0,
                                            0,
                                            pSurf->GetWidth(),
                                            patternHeight,
                                            1,
                                            1,
                                            "bars",
                                            "100",
                                            "horizontal"));
            break;
        case PatternType::LINES:
            for (UINT32 lwrY = 0; lwrY < patternHeight; lwrY+=8)
            {
                CHECK_RC(pSurf->FillPatternRect(0,
                                                lwrY,
                                                pSurf->GetWidth(),
                                                patternHeight,
                                                1,
                                                1,
                                                "bars",
                                                "100",
                                                "vertical"));
            }
            break;
        case PatternType::RANDOM:
            {
                string seed = Utility::StrPrintf("seed=%u", patternData);
                CHECK_RC(pSurf->FillPatternRect(0,
                                                0,
                                                pSurf->GetWidth(),
                                                patternHeight,
                                                1,
                                                1,
                                                "random",
                                                seed.c_str(),
                                                nullptr));
            }
            break;
        case PatternType::SOLID:
            CHECK_RC(pSurf->FillRect(0,
                                     0,
                                     pSurf->GetWidth(),
                                     patternHeight,
                                     patternData));
            break;
        case PatternType::ALTERNATE:
        {
            bool patternSelect = false;
            for (UINT32 lwrY = 0; lwrY < patternHeight; lwrY++)
            {
                for (UINT32 lwrX = 0; lwrX < pSurf->GetWidth(); lwrX += 4)
                {
                    patternSelect = !patternSelect;
                    CHECK_RC(pSurf->FillRect(lwrX,
                                             lwrY,
                                             4,
                                             1,
                                             (patternSelect) ? patternData : patternData2));
                }
            }
            break;
        }
        default:
            Printf(Tee::PriError, "%s : Unknown pattern type %d\n",
                   GetName().c_str(), static_cast<INT32>(pt));
            return RC::SOFTWARE_ERROR;
    }

    return RC::OK;
}

RC IOBandwidth::GetTransferSetup
(
    TransferType tt,
    EndpointConfig * pSrcConfig,
    EndpointConfig * pDstConfig,
    TransferSetup  * pTransferSetup
)
{
    const bool bSrcLocal = (pSrcConfig == &m_LocalConfig);

    pTransferSetup->pSrcSurfA = &pSrcConfig->surfaces[SRC_SURF_A];
    pTransferSetup->pSrcSurfB = &pSrcConfig->surfaces[SRC_SURF_B];
    pTransferSetup->pDstSurf  = &pDstConfig->surfaces[DST_SURF];

    RC rc;
    if (tt == TransferType::WRITES)
    {
        if (m_pRemoteDevice)
        {
            GpuSubdevice * pPeerSub = GetBoundGpuSubdevice();
            if (!bSrcLocal)
            {
                pPeerSub = m_pRemoteDevice->GetInterface<GpuSubdevice>();
            }
            pTransferSetup->srcCtxDmaOffsetA =
                pSrcConfig->surfaces[SRC_SURF_A].GetCtxDmaOffsetGpu();
            pTransferSetup->srcCtxDmaOffsetB =
                pSrcConfig->surfaces[SRC_SURF_B].GetCtxDmaOffsetGpu();
            if (UsePhysAddrCopies())
            {
                // Using Platform::VirtualToPhysical to get the physical address associated with
                // CPU mapped pointer is the correct way to extract the BAR1 physical address
                // where the memory resides.  Using any of the Surface2D versions of GetPhysAddress
                // does not return a BAR1 address.
                void * pVirtAddr = pDstConfig->surfaces[DST_SURF].GetAddress();
                pTransferSetup->dstCtxDmaOffset =
                    static_cast<UINT64>(Platform::VirtualToPhysical(pVirtAddr));
            }
            else
            {
                pTransferSetup->dstCtxDmaOffset =
                    pDstConfig->surfaces[DST_SURF].GetCtxDmaOffsetGpuPeer(0,
                                                                       pPeerSub->GetParentDevice(),
                                                                       0);
            }
        }
        else
        {
            pTransferSetup->srcCtxDmaOffsetA =
                pSrcConfig->surfaces[SRC_SURF_A].GetCtxDmaOffsetGpu();
            pTransferSetup->srcCtxDmaOffsetB =
                pSrcConfig->surfaces[SRC_SURF_A].GetCtxDmaOffsetGpu();
            pTransferSetup->dstCtxDmaOffset =
                pDstConfig->surfaces[DST_SURF].GetCtxDmaOffsetGpu();
        }
    }
    else
    {
        if (m_pRemoteDevice)
        {
            GpuSubdevice * pPeerSub = GetBoundGpuSubdevice();
            if (bSrcLocal)
            {
                pPeerSub = m_pRemoteDevice->GetInterface<GpuSubdevice>();
            }
            if (UsePhysAddrCopies())
            {
                void * pVirtAddr = pSrcConfig->surfaces[SRC_SURF_A].GetAddress();
                pTransferSetup->srcCtxDmaOffsetA =
                    static_cast<UINT64>(Platform::VirtualToPhysical(pVirtAddr));

                pVirtAddr = pSrcConfig->surfaces[SRC_SURF_B].GetAddress();
                pTransferSetup->srcCtxDmaOffsetB =
                    static_cast<UINT64>(Platform::VirtualToPhysical(pVirtAddr));
            }
            else
            {
                pTransferSetup->srcCtxDmaOffsetA =
                    pSrcConfig->surfaces[SRC_SURF_A].GetCtxDmaOffsetGpuPeer(0,
                                                                       pPeerSub->GetParentDevice(),
                                                                       0);
                pTransferSetup->srcCtxDmaOffsetB =
                    pSrcConfig->surfaces[SRC_SURF_B].GetCtxDmaOffsetGpuPeer(0,
                                                                       pPeerSub->GetParentDevice(),
                                                                       0);
            }
            pTransferSetup->dstCtxDmaOffset =
                pDstConfig->surfaces[DST_SURF].GetCtxDmaOffsetGpu();
        }
        else
        {
            pTransferSetup->srcCtxDmaOffsetA =
                pSrcConfig->surfaces[SRC_SURF_A].GetCtxDmaOffsetGpu();
            pTransferSetup->srcCtxDmaOffsetB =
                pSrcConfig->surfaces[SRC_SURF_A].GetCtxDmaOffsetGpu();
            pTransferSetup->dstCtxDmaOffset =
                pDstConfig->surfaces[DST_SURF].GetCtxDmaOffsetGpu();
        }
    }
    return rc;
}

// -----------------------------------------------------------------------------
RC IOBandwidth::InnerRun(TransferType tt)
{
    RC rc;
    UINT64 totalLocalElapsedTimeUs  = 0;
    UINT64 totalRemoteElapsedTimeUs = 0;

    TransferSetup localSetup;
    TransferSetup remoteSetup;

    if (m_bLoopback)
    {
        GetTransferSetup(tt,
                         &m_LocalConfig,
                         &m_LocalConfig,
                         &localSetup);
    }
    else
    {
        GetTransferSetup(tt,
                         (tt == TransferType::READS) ? &m_RemoteConfig : &m_LocalConfig,
                         (tt == TransferType::READS) ? &m_LocalConfig : &m_RemoteConfig,
                         &localSetup);
        if (m_pRemoteDevice)
        {
            GetTransferSetup(tt,
                             (tt == TransferType::READS) ? &m_LocalConfig : &m_RemoteConfig,
                             (tt == TransferType::READS) ? &m_RemoteConfig : &m_LocalConfig,
                             &remoteSetup);
        }
    }

    bool bDone = false;
    UINT32 loop = 0;
    const UINT32 endLoop = GetTestConfiguration()->Loops();
    while (!bDone)
    {
        UINT64 localElapsedTimeUs  = 0;
        UINT64 remoteElapsedTimeUs = 0;
        CHECK_RC(DoTransfers(tt,
                             localSetup,
                             remoteSetup,
                             &localElapsedTimeUs,
                             &remoteElapsedTimeUs));
        totalLocalElapsedTimeUs  += localElapsedTimeUs;
        totalRemoteElapsedTimeUs += remoteElapsedTimeUs;

        loop++;
        bDone = (loop >= endLoop);

        if (m_RuntimeMs && (Xp::GetWallTimeMS() - GetStartTimeMs()) >= m_RuntimeMs)
        {
            bDone = true;
        }
    }

    if (!m_SkipSurfaceCheck)
    {
        CHECK_RC(VerifyDestinationSurface(localSetup));
        if (m_pRemoteDevice && !m_bLoopback)
        {
            CHECK_RC(VerifyDestinationSurface(remoteSetup));
        }
    }

    if (!m_SkipBandwidthCheck || m_ShowBandwidthData)
    {
        CHECK_RC(CheckBandwidth(tt, totalLocalElapsedTimeUs, CeLocation::LOCAL, loop));
        if (m_pRemoteDevice && !m_bLoopback)
        {
            CHECK_RC(CheckBandwidth(tt, totalRemoteElapsedTimeUs, CeLocation::REMOTE, loop));
        }
    }
    return rc;
}

// -----------------------------------------------------------------------------
RC IOBandwidth::QueueCopy
(
    TransferType          tt,
    DmaWrapper          * pDmaWrap,
    const TransferSetup & transferSetup,
    bool                  bFirstCopy,
    bool                  bLastCopy
)
{
    const UINT64 srcCtxDmaOffset = (m_LwrSrcSurf == SRC_SURF_A) ?
        transferSetup.srcCtxDmaOffsetA : transferSetup.srcCtxDmaOffsetB;
    Surface2D *pSrcSurface = (m_LwrSrcSurf == SRC_SURF_A) ?
        transferSetup.pSrcSurfA : transferSetup.pSrcSurfB;

    pDmaWrap->WriteStartTimeSemaphore(bFirstCopy);
    pDmaWrap->SetFlush(bLastCopy);
    pDmaWrap->SetSurfaces(pSrcSurface, transferSetup.pDstSurf);
    pDmaWrap->SetCtxDmaOffsets(srcCtxDmaOffset, transferSetup.dstCtxDmaOffset);
    SetupDmaWrap(tt, pDmaWrap);
    return pDmaWrap->Copy(0,
                          0,
                          pSrcSurface->GetPitch(),
                          pSrcSurface->GetHeight(),
                          0,
                          0,
                          m_pTestConfig->TimeoutMs(),
                          0,
                          false,
                          bLastCopy,
                          bLastCopy);
}

//-----------------------------------------------------------------------------
RC IOBandwidth::SetupPatternSurfaces(Surface2D *pPatternA, Surface2D *pPatternB)
{
    RC rc;

    const UINT32 NUM_PAT_PER_SCREEN = 4;
    const UINT32 patternHeight = m_pTestConfig->SurfaceHeight() / NUM_PAT_PER_SCREEN;
    const ColorUtils::Format colorFormat =
        ColorUtils::ColorDepthToFormat(m_pTestConfig->DisplayDepth());

    for (UINT32 i = 0; i < 2; i++)
    {
        Surface2D * pSurf = (i == 0) ? pPatternA : pPatternB;
        pSurf->SetWidth(m_pTestConfig->SurfaceWidth());
        pSurf->SetHeight(patternHeight);
        pSurf->SetLocation(Memory::NonCoherent);
        pSurf->SetColorFormat(colorFormat);
        pSurf->SetProtect(Memory::ReadWrite);
        pSurf->SetLayout(Surface2D::Pitch);
        CHECK_RC(pSurf->Alloc(GetBoundGpuDevice()));
        CHECK_RC(pSurf->Map());
        if (m_pRemoteDevice && !m_bLoopback)
        {
            GpuDevice * pRemoteDev =
                m_pRemoteDevice->GetInterface<GpuSubdevice>()->GetParentDevice();
            CHECK_RC(pSurf->MapShared(pRemoteDev));
        }
    }

    {
        Tasker::DetachThread detachThread;
        switch (m_SrcSurfMode)
        {
            case 0:
                CHECK_RC(FillSurface(pPatternA, PatternType::RANDOM, 0, 0, patternHeight));
                CHECK_RC(FillSurface(pPatternB, PatternType::BARS, 0, 0, patternHeight));
                break;
            case 1:
                CHECK_RC(FillSurface(pPatternA, PatternType::RANDOM, 0, 0, patternHeight));
                CHECK_RC(FillSurface(pPatternB, PatternType::RANDOM, 0, 0, patternHeight));
                break;
            case 2:
                CHECK_RC(FillSurface(pPatternA, PatternType::SOLID, 0, 0, patternHeight));
                CHECK_RC(FillSurface(pPatternB, PatternType::SOLID, 0, 0, patternHeight));
                break;
            case 3:
                CHECK_RC(FillSurface(pPatternA,
                                     PatternType::ALTERNATE,
                                     0x00000000,
                                     0xFFFFFFFF,
                                     patternHeight));
                CHECK_RC(FillSurface(pPatternB,
                                     PatternType::ALTERNATE,
                                     0x00000000,
                                     0xFFFFFFFF,
                                     patternHeight));
                break;
            case 4:
                CHECK_RC(FillSurface(pPatternA,
                                     PatternType::ALTERNATE,
                                     0xAAAAAAAA,
                                     0x55555555,
                                     patternHeight));
                CHECK_RC(FillSurface(pPatternB,
                                     PatternType::ALTERNATE,
                                     0xAAAAAAAA,
                                     0x55555555,
                                     patternHeight));
                break;
            case 5:
                CHECK_RC(FillSurface(pPatternA, PatternType::RANDOM, 0, 0, patternHeight));
                CHECK_RC(FillSurface(pPatternB, PatternType::SOLID, 0, 0, patternHeight));
                break;
            case 6:
                CHECK_RC(Memory::FillRgbBars(pPatternA->GetAddress(),
                                             pPatternA->GetWidth(),
                                             patternHeight,
                                             pPatternA->GetColorFormat(),
                                             pPatternA->GetPitch()));
                CHECK_RC(Memory::FillAddress(pPatternB->GetAddress(),
                                             pPatternB->GetWidth(),
                                             patternHeight,
                                             pPatternB->GetBitsPerPixel(),
                                             pPatternB->GetPitch()));
                break;
            default:
                Printf(Tee::PriError, "Invalid SrcSurfMode = %u\n", m_SrcSurfMode);
                return RC::ILWALID_ARGUMENT;
        }
    }
    return rc;
}

//-----------------------------------------------------------------------------
RC IOBandwidth::SetupSurfaces(EndpointConfig * pEndpointConfig)
{
    RC rc;

    const bool bLocalConfig = (pEndpointConfig != &m_RemoteConfig);

    GpuDevice *pAllocDevice;
    GpuDevice *pPeerDevice = nullptr;;
    Memory::Location memLoc;
    if (bLocalConfig)
    {
        pEndpointConfig = &m_LocalConfig;
        pAllocDevice = GetBoundGpuDevice();
        memLoc = Memory::Fb;

        if (m_pRemoteDevice && !m_bLoopback)
        {
            pPeerDevice = m_pRemoteDevice->GetInterface<GpuSubdevice>()->GetParentDevice();
        }
    }
    else
    {
        pEndpointConfig = &m_RemoteConfig;
        if (m_pRemoteDevice && !m_bLoopback)
        {
            pAllocDevice = m_pRemoteDevice->GetInterface<GpuSubdevice>()->GetParentDevice();
            pPeerDevice = GetBoundGpuDevice();
            memLoc = Memory::Fb;
        }
        else
        {
            pAllocDevice = GetBoundGpuDevice();
            memLoc = Memory::Coherent;
        }
    }
    for (auto & lwrSurf : pEndpointConfig->surfaces)
    {
        CHECK_RC(AllocateSurface(&lwrSurf, pAllocDevice, memLoc));
        if (!UsePhysAddrCopies())
        {
            if (pPeerDevice)
            {
                CHECK_RC(lwrSurf.MapPeer(pPeerDevice));
            }
            if (m_bLoopback)
            {
                CHECK_RC(lwrSurf.MapLoopback());
            }
        }
        else
        {
            // For physical copies it is necessary to map the surface to create a BAR1
            // mapping that will be used as the source or destination address for the
            // copy engines.  This mapping needs to persist throughout the entire test.
            // This means that all test surfaces combined must be smaller than the BAR1
            // window
            CHECK_RC(lwrSurf.Map());
        }
    }
    CHECK_RC(FillSrcSurfaces(pEndpointConfig));

    return rc;
}

// -----------------------------------------------------------------------------
RC IOBandwidth::VerifyDestinationSurface(const TransferSetup & setup)
{
    RC rc;

    // Start by assuming the surfaces are already in sysmem.  Also at this point m_LwrSrcSurf
    // points to the next surface that would be copied not the last surface that was copied
    Surface2D * pSrcSurf = (m_LwrSrcSurf == SRC_SURF_A) ? setup.pSrcSurfB : setup.pSrcSurfA;
    Surface2D * pDstSurf = setup.pDstSurf;

    unique_ptr<Surface2D> pSysmemSrcSurf;
    unique_ptr<Surface2D> pSysmemDstSurf;

    // If either the source or destination surface is not in sysmem then DMA the
    // surface to sysmem to verify the data
    if (pSrcSurf->GetLocation() == Memory::Fb)
    {
       pSysmemSrcSurf = make_unique<Surface2D>();
       CHECK_RC(AllocateSurface(pSysmemSrcSurf.get(),
                                pSrcSurf->GetGpuDev(),
                                Memory::NonCoherent));
    }
   
    if (pDstSurf->GetLocation() == Memory::Fb)
    {
       pSysmemDstSurf = make_unique<Surface2D>();
       CHECK_RC(AllocateSurface(pSysmemDstSurf.get(),
                                pDstSurf->GetGpuDev(),
                                Memory::NonCoherent));
    }
   

    if (pSysmemSrcSurf)
    {
        EndpointConfig * pEndpointConfig =
            (pSrcSurf->GetGpuDev() == GetBoundGpuDevice()) ? &m_LocalConfig : &m_RemoteConfig;
    
        pEndpointConfig->dmaWrap.SetSurfaces(pSrcSurf, pSysmemSrcSurf.get());
        pEndpointConfig->dmaWrap.SetCtxDmaOffsets(pSrcSurf->GetCtxDmaOffsetGpu(),
                                                  pSysmemSrcSurf->GetCtxDmaOffsetGpu());
        pEndpointConfig->dmaWrap.SetUsePhysicalCopy(false, false);
    
        CHECK_RC(pEndpointConfig->dmaWrap.Copy(0,
                                               0,
                                               pSrcSurf->GetPitch(),
                                               pSrcSurf->GetHeight(),
                                               0,
                                               0,
                                               m_pTestConfig->TimeoutMs(),
                                               0));
        pSrcSurf = pSysmemSrcSurf.get();
    }
    if (pSysmemDstSurf)
    {
        EndpointConfig * pEndpointConfig =
            (pDstSurf->GetGpuDev() == GetBoundGpuDevice()) ? &m_LocalConfig : &m_RemoteConfig;
    
        pEndpointConfig->dmaWrap.SetSurfaces(pDstSurf, pSysmemDstSurf.get());
        pEndpointConfig->dmaWrap.SetCtxDmaOffsets(pDstSurf->GetCtxDmaOffsetGpu(),
                                                  pSysmemDstSurf->GetCtxDmaOffsetGpu());
        pEndpointConfig->dmaWrap.SetUsePhysicalCopy(false, false);
    
        CHECK_RC(pEndpointConfig->dmaWrap.Copy(0,
                                               0,
                                               pDstSurf->GetPitch(),
                                               pDstSurf->GetHeight(),
                                               0,
                                               0,
                                               m_pTestConfig->TimeoutMs(),
                                               0));
        pDstSurf = pSysmemDstSurf.get();
    }

    bool bUnmapSrc = false;
    bool bUnmapDst = false;

    // Need to track whether the surface is already mapped because in some versions
    // of the test the surface is mapped during setup and needs to remain mapped during
    // the test
    if (!pSrcSurf->IsMapped())
    {
        CHECK_RC(pSrcSurf->Map());
        bUnmapSrc = true;
    }
    if (!pDstSurf->IsMapped())
    {
        CHECK_RC(pDstSurf->Map());
        bUnmapDst = true;
    }

    UINT32 errorCount = 0;
    UINT32 pixelMask = GetPixelMask();
    {
        Tasker::DetachThread detachThread;

        for (UINT32 lwrY = 0;
              lwrY < pSrcSurf->GetHeight() && (errorCount < m_NumErrorsToPrint);
              lwrY++)
        {
            const UINT32 pixels = pSrcSurf->GetWidth();
            vector<UINT32> srcLine(pixels);
            vector<UINT32> dstLine(pixels);

            const UINT32 startOffset = lwrY * pSrcSurf->GetPitch();

            // Compare the surface data line by line
            Platform::VirtualRd(static_cast<UINT08 *>(pSrcSurf->GetAddress()) + startOffset,
                                &srcLine[0],
                                pixels * sizeof(UINT32));
            Platform::VirtualRd(static_cast<UINT08 *>(pDstSurf->GetAddress()) + startOffset,
                                &dstLine[0],
                                pixels * sizeof(UINT32));

            for (UINT32 lwrX = 0; (lwrX < pixels) && (errorCount < m_NumErrorsToPrint); lwrX++)
            {
                const UINT32 srcPixel = srcLine[lwrX] & pixelMask;
                const UINT32 dstPixel = dstLine[lwrX] & pixelMask;

                if (srcPixel != dstPixel)
                {
                    Printf(Tee::PriError,
                           "Mismatch at (%d, %d) on destination surface, got 0x%x,"
                           " expected 0x%x\n",
                           lwrX, lwrY,
                           dstPixel,
                           srcPixel);
                    rc = RC::BUFFER_MISMATCH;
                    errorCount++;
                }
            }
        }
    }

    if (bUnmapSrc)
        pSrcSurf->Unmap();
    if (bUnmapDst)
        pDstSurf->Unmap();
    return rc;
}

// -----------------------------------------------------------------------------
JS_CLASS_VIRTUAL_INHERIT(IOBandwidth, GpuTest, "IO bandwidth base class");

CLASS_PROP_READWRITE(IOBandwidth, TransferType, UINT08,
                     "Transfer types to perform, 0 = read, 1 = write,"
                     " 2 = reads and writes (default = 2)");
CLASS_PROP_READWRITE(IOBandwidth, CopiesPerLoop, UINT32,
                     "Number of times each copy is repeated internally (default = 1)");
CLASS_PROP_READWRITE(IOBandwidth, SurfSizeFactor, UINT32,
                     "Determines size of transfer - multiple of the default screen size"
                     " (default = 50)");
CLASS_PROP_READWRITE(IOBandwidth, SrcSurfMode, UINT32,
                     "Source surface mode (0 = default = random & bars, 1 = all random,"
                     " 2 = all zeroes, 3 = alternate 0x0 & 0xFFFFFFFF,"
                     " 4 = alternate 0xAAAAAAAA & 0x55555555, 5 = random & zeroes,"
                     " 6 = default = RGB bars & address");
CLASS_PROP_READWRITE(IOBandwidth, SrcSurfModePercent, UINT32,
                     "For modes with two types of data, what percent of the source "
                     "surface is filled with the first type. (default = 50)");
CLASS_PROP_READWRITE(IOBandwidth, ShowBandwidthData, bool,
                     "Print out detailed bandwidth data (default = false)");
CLASS_PROP_READWRITE(IOBandwidth, SkipBandwidthCheck, bool,
                     "Skip the bandwidth check (default = false)");
CLASS_PROP_READWRITE(IOBandwidth, SkipSurfaceCheck, bool,
                     "Skip the surface check (default = false)");
CLASS_PROP_READWRITE(IOBandwidth, IgnorePceRequirements, bool,
                     "Whether to ignore PCE requirements when allocating copy engines"
                     " (default = false)");
CLASS_PROP_READWRITE(IOBandwidth, NumErrorsToPrint, UINT32,
                     "Number of errors printed on miscompares"
                     " (default = 10)");
CLASS_PROP_READWRITE(IOBandwidth, RuntimeMs, UINT32,
                     "Runtime for the test in milliseconds (default = 0)");
