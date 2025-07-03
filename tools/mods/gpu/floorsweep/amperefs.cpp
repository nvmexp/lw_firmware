/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2018-2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#include "amperefs.h"
#include "gpu/include/gpusbdev.h"
#include "gpu/reghal/reghal.h"
#include "core/include/coreutility.h"
#include "core/include/gpu.h"
#include "core/include/platform.h"
#include "class/cl90e7.h"

#include <algorithm>

//------------------------------------------------------------------------------
static const ParamDecl s_FloorSweepingParamsAmpere[] =
{
    UNSIGNED_PARAM("syspipe_enable",  "System pipes enable"),
    UNSIGNED_PARAM("syspipe_disable", "System pipes disable"),
    UNSIGNED_PARAM("ofa_enable",  "Optical Flow Engine (OFA) enable"),
    UNSIGNED_PARAM("ofa_disable", "Optical Flow Engine (OFA) disable"),
    UNSIGNED_PARAM("lwjpg_enable",  "LW JPEG Engine enable"),
    UNSIGNED_PARAM("lwjpg_disable", "LW JPEG Engine disable"),
    LAST_PARAM
};
//------------------------------------------------------------------------------
static ParamConstraints s_FsParamConstraintsAmpere[] =
{
    MUTUAL_EXCLUSIVE_PARAM("syspipe_enable", "syspipe_disable"),
    MUTUAL_EXCLUSIVE_PARAM("ofa_enable", "ofa_disable"),
    MUTUAL_EXCLUSIVE_PARAM("lwjpg_enable", "lwjpg_disable"),
    LAST_CONSTRAINT
};

AmpereFs::AmpereFs(GpuSubdevice *pSubdev) : TU10xFs(pSubdev)
{
    m_FsData.emplace_back(FST_SYSPIPE, "syspipe", "SYS_PIPE_DISABLE", MODS_FUSE_CTRL_OPT_SYS_PIPE_DATA, MODS_FUSE_OPT_SYS_PIPE_DISABLE_DATA);
    m_FsData.emplace_back(FST_OFA,     "ofa",     "OFA_DISABLE",      MODS_FUSE_CTRL_OPT_OFA_DATA,      MODS_FUSE_OPT_OFA_DISABLE_DATA);
    m_FsData.emplace_back(FST_LWJPG,   "lwjpg",   "LWJPG_DISABLE",    MODS_FUSE_CTRL_OPT_LWJPG_DATA,    MODS_FUSE_OPT_LWJPG_DISABLE_DATA);
}

UINT32 AmpereFs::MaskFromHwDevInfo(GpuSubdevice::HwDevType devType) const
{
    UINT32 mask = 0;
    const vector<GpuSubdevice::HwDevInfo> & hwDevInfo = m_pSub->GetHwDevInfo();
    for (auto lwrDev : hwDevInfo)
    {
        if (lwrDev.hwType == devType)
        {
            MASSERT(lwrDev.hwInstance < 32);
            mask |= (1 << lwrDev.hwInstance);
        }
    }
    return mask;
}

//------------------------------------------------------------------------------
// See https://confluence.lwpu.com/display/AMPCTXSWPRIHUB/Ampere+PRI_HUB#AmperePRI_HUB-HowdoesSWinitializetheprivring?
RC AmpereFs::InitializePriRing_FirstStage(bool bInSetup)
{
    // Adding 10us delay to avoid race condition
    Platform::SleepUS(10);

    RegHal &regs = m_pSub->Regs();
    regs.Write32(MODS_PPRIV_MASTER_RING_GLOBAL_CTL_RING_RESET_ASSERTED);
    regs.Write32(MODS_PPRIV_MASTER_RING_GLOBAL_CTL_RING_RESET_DEASSERTED);

    // Bug 731411: For RTL perf, avoid unneeded read loop at startup
    if (!(Platform::GetSimulationMode() == Platform::RTL && bInSetup))
    {
        // wait for the toggle to make it around the ring.
        UINT32 data;
        for (int index = 0; index < 20; index++)
        {
            data = regs.Read32(MODS_PPRIV_MASTER_RING_COMMAND);
        }
        (void)(data);
    }

    return RC::OK;
}

//------------------------------------------------------------------------------
RC AmpereFs::ReadFloorsweepArg(ArgReader * pArgRead, FsData *pFsData)
{
    const string fsEnable  = pFsData->name + "_enable";
    const string fsDisable = pFsData->name + "_disable";

    pFsData->bPresent =  pArgRead->ParamPresent(fsEnable.c_str()) ||
                         pArgRead->ParamPresent(fsDisable.c_str());
    if (pFsData->bPresent)
    {
        const RegHal& regs = m_pSub->Regs();
        if (!regs.IsSupported(pFsData->ctrlField) ||
            !regs.HasRWAccess(regs.ColwertToAddress(pFsData->ctrlField)))
        {
            Printf(Tee::PriError,
                   "%s floorsweeping not supported on %s\n",
                   pFsData->name.c_str(),
                   Gpu::DeviceIdToString(m_pSub->DeviceId()).c_str());
            return RC::BAD_PARAMETER;
        }

        // Only one of the 2 arguments can be present due to mutual exclusion constraint
        const UINT32 validMask = regs.LookupFieldValueMask(pFsData->ctrlField);

        pFsData->mask = pArgRead->ParamUnsigned(fsEnable.c_str(), 0);
        pFsData->mask = pArgRead->ParamUnsigned(fsDisable.c_str(), pFsData->mask);

        if (pFsData->mask & ~validMask)
        {
            Printf(Tee::PriError,
                   "invalid %s floorsweeping value : 0x%x\n",
                   pFsData->name.c_str(), pFsData->mask);
            return RC::BAD_PARAMETER;
        }

        if (pArgRead->ParamPresent(fsEnable.c_str()))
            pFsData->mask = ~pFsData->mask & validMask;
    }

    return OK;
}

//------------------------------------------------------------------------------
RC AmpereFs::ReadFloorsweepingArgumentsImpl(ArgDatabase& fsArgDb)
{
    RC rc = OK;

    CHECK_RC(TU10xFs::ReadFloorsweepingArgumentsImpl(fsArgDb));

    string FsArgs = GetFermiFsArgs();
    if (!FsArgs.empty())
    {
        ArgReader argReader(s_FloorSweepingParamsAmpere, s_FsParamConstraintsAmpere);
        if (!argReader.ParseArgs(&fsArgDb))
            return RC::BAD_COMMAND_LINE_ARGUMENT;

        for (auto & lwrFsData : m_FsData)
        {
            CHECK_RC(ReadFloorsweepArg(&argReader, &lwrFsData));
        }

        auto pSyspipeFs = find_if(m_FsData.begin(), m_FsData.end(),
                                [] (const FsData & f)-> bool { return f.type == FST_SYSPIPE; } );
        if ((pSyspipeFs != m_FsData.end()) && pSyspipeFs->bPresent && (pSyspipeFs->mask & 0x1))
        {
            Printf(Tee::PriError, "syspipe 0 may not be disabled!!\n");
            return RC::BAD_PARAMETER;
        }

        const bool bFsAffected =
            any_of(m_FsData.cbegin(),
                   m_FsData.cend(),
                   [](const FsData & f) { return f.bPresent; });
        SetFloorsweepingAffected(GetFloorsweepingAffected() || bFsAffected);
    }

    return rc;
}

//------------------------------------------------------------------------------
void AmpereFs::RestoreFbio() const
{
    //  Now restore them just like any other fermi+ chip
    m_pSub->Regs().Write32Sync(MODS_FUSE_CTRL_OPT_FBIO, m_SavedFbio);
}

//------------------------------------------------------------------------------
void AmpereFs::ResetFbio() const
{
    //  Now reset them just like any other fermi+ chip
    UINT32 fbio =  m_pSub->Regs().Read32(MODS_FUSE_OPT_FBIO_DISABLE);
    m_pSub->Regs().Write32Sync(MODS_FUSE_CTRL_OPT_FBIO, fbio);
}

//------------------------------------------------------------------------------
void AmpereFs::PrintFloorsweepingParams()
{
    TU10xFs::PrintFloorsweepingParams();
    Tee::Priority pri = GetFloorsweepingPrintPriority();

    string params;
    string masks;
    GetFloorsweepingStrings(m_FsData, &params, &masks);
    Printf(pri, "AmpereFs : Floorsweeping parameters present on commandline:%s\n", params.c_str());
    Printf(pri, "AmpereFs : Floorsweeping parameters mask values:%s\n", masks.c_str());
}

//------------------------------------------------------------------------------
void AmpereFs::ApplyFloorsweepingSettings()
{
    TU10xFs::ApplyFloorsweepingSettings();
    ApplyFloorsweepingSettings(m_FsData);
}

//------------------------------------------------------------------------------
void AmpereFs::RestoreFloorsweepingSettings()
{
    Printf(Tee::PriDebug, "AmpereFs::RestoreFloorsweepingSettings: "
           " checking and restoring startup settings\n");

    RestoreFloorsweepingSettings(m_FsData);

    return TU10xFs::RestoreFloorsweepingSettings();
}

//------------------------------------------------------------------------------
void AmpereFs::ResetFloorsweepingSettings()
{
    Printf(Tee::PriDebug, "AmpereFs::ResetFloorsweepingSettings: "
           " reading and resetting settings\n");

    ResetFloorsweepingSettings(m_FsData);

    TU10xFs::ResetFloorsweepingSettings();
}

//------------------------------------------------------------------------------
void AmpereFs::SaveFloorsweepingSettings()
{
    SaveFloorsweepingSettings(&m_FsData);
    return TU10xFs::SaveFloorsweepingSettings();
}

//------------------------------------------------------------------------------
RC AmpereFs::GetFloorsweepingMasks
(
    FloorsweepingMasks* pFsMasks
) const
{
    if (Platform::IsVirtFunMode())
        return RC::UNSUPPORTED_FUNCTION;

    MASSERT(pFsMasks != 0);

    RC rc;
    pFsMasks->clear();
    pFsMasks->insert(pair<string, UINT32>("syspipe_mask", SyspipeMask()));
    pFsMasks->insert(pair<string, UINT32>("ofa_mask", OfaMask()));
    CHECK_RC(TU10xFs::GetFloorsweepingMasks(pFsMasks));
    return rc;
}

//------------------------------------------------------------------------------
bool AmpereFs::IsFloorsweepingValid() const
{
#ifdef INCLUDE_FSLIB
    bool isValid = true;
    string fsError = "";

    // GPC Masks
    FsLib::GpcMasks gpcMasks = {};
    FsLib::FbpMasks fbpMasks = {};

    GetFsLibMask(&gpcMasks);
    GetFsLibMask(&fbpMasks);

    // Check with Arch FS library
    if (!FsLib::IsFloorsweepingValid(m_pSub->GetDeviceId(), gpcMasks, fbpMasks, fsError))
    {
        Printf(Tee::PriError, "Requested floorsweeping invalid.\n"
                              "FsLib Error : %s\n", fsError.c_str());
        isValid = false;
    }

    return isValid;
#else
    Printf(Tee::PriLow, "FsLib not included. Bypassing FS validity check\n");
    return true;
#endif
}

//------------------------------------------------------------------------------
#ifdef INCLUDE_FSLIB
void AmpereFs::GetFsLibMask(FsLib::GpcMasks *pGpcMasks) const
{
    MASSERT(pGpcMasks);

    const UINT32 maxGpcs = m_pSub->GetMaxGpcCount();
    const UINT32 maxTpcs = m_pSub->GetMaxTpcCount();
    const UINT32 maxPes  = m_pSub->GetMaxPesCount();

    pGpcMasks->gpcMask = (m_FsGpcParamPresent ? m_GpcMask : ~GpcMask()) &
                         ((1 << maxGpcs) - 1);
    for (UINT32 gpcNum = 0; gpcNum < maxGpcs; gpcNum++)
    {
        bool isGpcDisabled    = (pGpcMasks->gpcMask & (1 << gpcNum)) != 0;
        UINT32 defaultTpcMask = isGpcDisabled ? ~0U : ~TpcMask(gpcNum);
        UINT32 defaultPesMask = isGpcDisabled ? ~0U : ~PesMask(gpcNum);

        pGpcMasks->tpcPerGpcMasks[gpcNum] = (m_FsGpcTpcParamPresent[gpcNum] ?
                                             m_GpcTpcMasks[gpcNum] :
                                             defaultTpcMask) &
                                            ((1 << maxTpcs) - 1);
        pGpcMasks->pesPerGpcMasks[gpcNum] = (m_FsGpcPesParamPresent[gpcNum] ?
                                             m_GpcPesMasks[gpcNum] :
                                             defaultPesMask) &
                                             ((1 << maxPes) - 1);
    }
}

void AmpereFs::GetFsLibMask(FsLib::FbpMasks *pFbpMasks) const
{
    MASSERT(pFbpMasks);
    RegHal &regs = m_pSub->Regs();

    const UINT32 maxFbps            = m_pSub->GetMaxFbpCount();
    const UINT32 maxFbpas           = regs.Read32(MODS_PTOP_SCAL_NUM_FBPAS);
    const UINT32 fbpaStride         = maxFbpas / maxFbps;
    const UINT32 fbpaOnFbpMask      = (1 << fbpaStride) - 1;
    const UINT32 halfFbpaStride     = GetHalfFbpaWidth();
    const UINT32 halfFbpaOnFbpMask  = (1 << halfFbpaStride) - 1;

    const UINT32 fbioMask     = m_FsFbioParamPresent ? m_FbioMask : ~FbioMask();
    const UINT32 fbpaMask     = m_FsFbpaParamPresent ? m_FbpaMask : ~FbpaMask();
    const UINT32 halfFbpaMask = m_FsHalfFbpaParamPresent ?
                                m_HalfFbpaMask :
                                regs.Read32(MODS_FUSE_STATUS_OPT_HALF_FBPA);

    pFbpMasks->fbpMask = (m_FsFbParamPresent ? m_FbMask : ~FbMask()) &
                         ((1 << maxFbps) - 1);
    for (UINT32 fbNum = 0; fbNum < maxFbps; fbNum++)
    {
        bool isFbpDisabled    = (pFbpMasks->fbpMask & (1 << fbNum)) != 0;
        UINT32 defaultLtcMask = isFbpDisabled ? ~0U : ~L2Mask(fbNum);

        // FPBA stride == FBIO stride
        pFbpMasks->fbpaPerFbpMasks[fbNum]     = (fbpaMask >> fbNum * fbpaStride) & fbpaOnFbpMask;
        pFbpMasks->fbioPerFbpMasks[fbNum]     = (fbioMask >> fbNum * fbpaStride) & fbpaOnFbpMask;
        pFbpMasks->halfFbpaPerFbpMasks[fbNum] = (halfFbpaMask >> fbNum * halfFbpaStride) &
                                                 halfFbpaOnFbpMask;
        pFbpMasks->ltcPerFbpMasks[fbNum] = (m_FsFbpL2ParamPresent[fbNum] ?
                                            m_FbpL2Masks[fbNum] :
                                            defaultLtcMask) &
                                            ((1 << regs.Read32(MODS_PTOP_SCAL_NUM_LTC_PER_FBP)) - 1);
    }
}
#endif

//------------------------------------------------------------------------------
RC AmpereFs::PrintMiscFuseRegs()
{
    RC rc;
    CHECK_RC(TU10xFs::PrintMiscFuseRegs());

    PrintMiscFuseRegs(m_FsData);
    return OK;
}

//------------------------------------------------------------------------------
UINT32 AmpereFs::LwdecMask() const
{
    return MaskFromHwDevInfo(GpuSubdevice::HwDevType::HDT_LWDEC);
}

//------------------------------------------------------------------------------
UINT32 AmpereFs::LwencMask() const
{
    return MaskFromHwDevInfo(GpuSubdevice::HwDevType::HDT_LWENC);
}

//------------------------------------------------------------------------------
UINT32 AmpereFs::SyspipeMask() const
{
    return MaskFromHwDevInfo(GpuSubdevice::HwDevType::HDT_GRAPHICS);
}

//------------------------------------------------------------------------------
UINT32 AmpereFs::OfaMask() const
{
    return MaskFromHwDevInfo(GpuSubdevice::HwDevType::HDT_OFA);
}

//------------------------------------------------------------------------------
UINT32 AmpereFs::LwjpgMask() const
{
    return MaskFromHwDevInfo(GpuSubdevice::HwDevType::HDT_LWJPG);
}

//------------------------------------------------------------------------------
UINT32 AmpereFs::PceMask() const
{
    UINT32 pceMask = (1U << m_pSub->GetMaxPceCount()) - 1;

    const RegHal & regs = m_pSub->Regs();

    // If spare bit 0 is not set then there is no PCE floorweeping
    UINT32 spareBit = 0;
    if (regs.Read32Priv(MODS_FUSE_SPARE_BIT_x, 0, &spareBit) != RC::OK)
    {
        Printf(Tee::PriWarn, "Cannot read PCE mask\n");
    }
    if (spareBit == 0)
        return pceMask;

    // Each spare bit register floorsweeps a pair of PCEs in ascending order
    for (UINT32 bitNum = 1; bitNum <= ((m_pSub->GetMaxPceCount() + 1) >> 1); bitNum++)
    {
        // The bits are disables
        if (regs.Read32(MODS_FUSE_SPARE_BIT_x, bitNum) != 0)
            pceMask &= ~(3U << ((bitNum - 1) * 2));
    }
    return pceMask;
}

//------------------------------------------------------------------------------
UINT32 AmpereFs::LwLinkMask() const
{
    return ~m_pSub->Regs().Read32(MODS_FUSE_STATUS_OPT_LWLINK) &
           ((1U << m_pSub->Regs().LookupAddress(MODS_SCAL_LITTER_NUM_IOCTRL)) - 1);
}

//------------------------------------------------------------------------------
void AmpereFs::CheckIfPriRingInitIsRequired()
{
    auto pSyspipeFs = find_if(m_FsData.begin(), m_FsData.end(),
                            [] (const FsData & f)-> bool { return f.type == FST_SYSPIPE; } );
    m_IsPriRingInitRequired = m_IsPriRingInitRequired ||
                              (pSyspipeFs == m_FsData.end()) ||
                              pSyspipeFs->bPresent;
    TU10xFs::CheckIfPriRingInitIsRequired();
}

/* virtual */ UINT32 AmpereFs::GetEngineResetMask()
{
    MASSERT(!"GetEngineResetMask not implemented on Ampere");
    return 0;
}

//-----------------------------------------------------------------------------
//! Trigger a SW reset
/* virtual */ RC AmpereFs::SwReset(bool bReinitPriRing)
{
    RC rc;

    // First Reset PMC
    // This code was added to solve a problem that RM was seeing with the Microsoft new Win8 TDR
    // tests. The GPU is not hung but given more work then it can handle in 2 seconds. As a
    // result we have some outstanding IO operations that will cause us issues in the future.
    // Although this should not be the case here we are doing it for insurance.
    CHECK_RC(m_pSub->EnableHwDevice(GpuSubdevice::HwDevType::HDT_ALL_ENGINES, false));

    // For Ampere the only block not part of Enable Engines (since it is not considered an
    // engine) that needs to be disabled for SW Reset is PDISP.  Disable that explicitly here
    m_pSub->Regs().Write32Sync(MODS_PMC_ENABLE_PDISP_DISABLED);

    CHECK_RC(m_pSub->FLReset());

    if (bReinitPriRing)
    {
        CHECK_RC(InitializePriRing_FirstStage(false));
        CHECK_RC(InitializePriRing());
    }

    return OK;
}

//-----------------------------------------------------------------------------
void AmpereFs::ApplyFloorsweepingSettings(const vector<FsData> & fsData)
{
    for (auto const & lwrFsData : fsData)
    {
        if (lwrFsData.bPresent)
            m_pSub->Regs().Write32Sync(lwrFsData.ctrlField, lwrFsData.mask);
    }
}

//-----------------------------------------------------------------------------
void AmpereFs::GetFloorsweepingStrings
(
    const vector<FsData> & fsData,
    string * pParamStr,
    string * pMaskStr
)
{
    for (auto const & lwrFsData : fsData)
    {
        if (lwrFsData.bPresent)
        {
            pParamStr->append(" " + lwrFsData.name);
            pMaskStr->append(" " + Utility::StrPrintf(" %s=0x%x",
                                                      lwrFsData.name.c_str(),
                                                      lwrFsData.mask));
        }
    }
}

//-----------------------------------------------------------------------------
void AmpereFs::RestoreFloorsweepingSettings(const vector<FsData> & fsData)
{
    for (auto const & lwrFsData : fsData)
    {
        if (lwrFsData.bPresent)
            m_pSub->Regs().Write32Sync(lwrFsData.ctrlField, lwrFsData.savedMask);
    }
}

//-----------------------------------------------------------------------------
void AmpereFs::ResetFloorsweepingSettings(const vector<FsData> & fsData)
{
    RegHal& regs = m_pSub->Regs();
    for (auto const & lwrFsData : fsData)
    {
        UINT32 dis;
        if (regs.Read32Priv(lwrFsData.disableField, &dis) == RC::OK)
        {
            (void)regs.Write32PrivSync(lwrFsData.ctrlField, dis);
        }
    }
}

//-----------------------------------------------------------------------------
void AmpereFs::SaveFloorsweepingSettings(vector<FsData> * pFsData)
{
    const RegHal& regs = m_pSub->Regs();
    for (auto & lwrFsData : *pFsData)
    {
        (void)regs.Read32Priv(lwrFsData.ctrlField, &lwrFsData.savedMask);
    }
}

//-----------------------------------------------------------------------------
void AmpereFs::PrintMiscFuseRegs(const vector<FsData> & fsData)
{
    for (auto & lwrFsData : fsData)
    {
        if (m_pSub->Regs().IsSupported(lwrFsData.disableField))
        {
            Printf(Tee::PriNormal, " %s : 0x%x\n", lwrFsData.disableStr.c_str(),
                            m_pSub->Regs().Read32(lwrFsData.disableField));
        }
    }
}

//-----------------------------------------------------------------------------
void AmpereFs::GraphicsEnable(bool bEnable)
{
    // Ignore the return value for enabling graphics since it may or may not
    // actually be present
    m_pSub->EnableHwDevice(GpuSubdevice::HwDevType::HDT_GRAPHICS, bEnable);
}

//-----------------------------------------------------------------------------
RC AmpereFs::AdjustL2NocFloorsweepingArguments()
{
    RC rc;

    // If FBPs are not floorswept through the command line, we don't need any
    // additional L2 NoC based FBP floorsweeping
    if (!m_FsFbParamPresent)
    {
        return RC::OK;
    }

    // Find number of active uGPUs based on GPC floorsweeping
    const UINT32 maxGpcs = m_pSub->GetMaxGpcCount();
    const UINT32 gpcMask = (m_FsGpcParamPresent ? m_GpcMask : ~GpcMask()) &
                           ((1 << maxGpcs) - 1);

    UINT32 uGpuActiveMask = 0;
    for (UINT32 gpc = 0; gpc < maxGpcs; gpc++)
    {
        // If GPC is enabled, mark that uGPU as active
        if (!((gpcMask >> gpc) & 1))
        {
            INT32 uGpu = m_pSub->GetUGpuFromGpc(gpc);
            MASSERT(uGpu >= 0);
            uGpuActiveMask |= (1 << uGpu);
        }
    }
    const UINT32 numUGpusActive = Utility::CountBits(static_cast<UINT64>(uGpuActiveMask));

    // Get FBPs connected to HBM B and HBM E
    // These L2 NOC pairs should always be disabled together
    UINT32 fbpB1, fbpB2, fbpE1, fbpE2;
    CHECK_RC(m_pSub->GetHBMSiteFbps(1, &fbpB1, &fbpB2));
    CHECK_RC(m_pSub->GetHBMSiteFbps(4, &fbpE1, &fbpE2));

    UINT32 newFbMask = 0;
    const UINT32 maxFbps = m_pSub->GetMaxFbpCount();
    for (UINT32 fbNum = 0; fbNum < maxFbps; fbNum++)
    {
        const bool isFbpDisabled = (m_FbMask & (1 << fbNum)) != 0;
        if (isFbpDisabled)
        {
            INT32 l2NocPair = m_pSub->GetFbpNocPair(fbNum);
            MASSERT(l2NocPair >= 0);

            const bool l2NocDisabled = (m_FbMask & (1 << l2NocPair)) != 0;
            if (!l2NocDisabled)
            {
                INT32 l2NolwGpu = m_pSub->GetUGpuFromFbp(static_cast<UINT32>(l2NocPair));
                MASSERT(l2NolwGpu >= 0);

                // If more than 1 uGPU is active OR
                // The FBP is tried to HBMs B & E OR
                // the uGPU the paired FBP lies in is inactive,
                // disable the paired FBP
                if ((numUGpusActive > 1) ||
                    (fbNum == fbpB1 || fbNum == fbpB2 || fbNum == fbpE1 || fbNum == fbpE2) ||
                    !((uGpuActiveMask >> l2NolwGpu) & 1))
                {
                    Printf(Tee::PriWarn, "Disabling FBP %u since its L2 NOC pair %u is disabled\n",
                                          static_cast<UINT32>(l2NocPair), fbNum);
                    newFbMask |= (1 << l2NocPair);
                }
            }
        }
    }
    m_FbMask |= newFbMask;
    CHECK_RC(AdjustFbpFloorsweepingArguments());

    return rc;
}

//-----------------------------------------------------------------------------
bool AmpereFs::IsIfrFsEntry(UINT32 address) const
{
    // On Ampere, the RPR object is shared between the FS preservation table
    // and TPC repair
    // Assume all address that aren't for LW_FUSE_DISABLE_EXTEND_OPT_TPC_GPC(i)
    // refer to regular floorsweeping
    RegHal &regs = m_pSub->Regs();
    UINT32 numDisableExtendRegs = regs.LookupAddress(MODS_FUSE_DISABLE_EXTEND_OPT_TPC_GPC__SIZE_1);
    for (UINT32 gpc = 0; gpc < numDisableExtendRegs; gpc++)
    {
        if (address == regs.LookupAddress(MODS_FUSE_DISABLE_EXTEND_OPT_TPC_GPC, gpc))
        {
            return false;
        }
    }
    return true;
}

//-----------------------------------------------------------------------------
RC AmpereFs::PreserveFloorsweepingSettings() const
{
    RC rc;

    // On Ampere, the RPR object is shared between the FS preservation table
    // and TPC repair
    LW90E7_CTRL_RPR_GET_INFO_PARAMS lwrrIfrParams = { 0 };
    CHECK_RC(LwRmPtr()->ControlSubdeviceChild(m_pSub, GF100_SUBDEVICE_INFOROM,
        LW90E7_CTRL_CMD_RPR_GET_INFO, &lwrrIfrParams, sizeof(lwrrIfrParams)));

    // Copy all the TPC repair data into the new RPR object
    LW90E7_CTRL_RPR_WRITE_OBJECT_PARAMS newIfrParams = { 0 };
    UINT32& newEntryCount = newIfrParams.entryCount;
    for (UINT32 i = 0; i < lwrrIfrParams.entryCount; i++)
    {
        auto& entry = lwrrIfrParams.repairData[i];
        if (!IsIfrFsEntry(entry.address))
        {
            newIfrParams.repairData[newEntryCount].address = entry.address;
            newIfrParams.repairData[newEntryCount].data    = entry.data;
            newEntryCount++;
        }
    }

    CHECK_RC(PopulateFsInforom(&newIfrParams));
    CHECK_RC(LwRmPtr()->ControlSubdeviceChild(m_pSub, GF100_SUBDEVICE_INFOROM,
        LW90E7_CTRL_CMD_RPR_WRITE_OBJECT, &newIfrParams, sizeof(newIfrParams)));

    return rc;
}

//-----------------------------------------------------------------------------
// Get the Original disable Mask of the units, and add it to the user's mask of cmd line
RC AmpereFs::AddLwrFsMask()
{
    auto& regs = m_pSub->Regs();
    if (IsFloorsweepingAllowed())
    {
        // Add GPC mask
        if (m_FsGpcParamPresent)
        {
            UINT32 lwrGpcDisMask = regs.Read32(MODS_FUSE_STATUS_OPT_GPC);
            // If one bit in Current disable mask is 1, it means that this unit is originally disabled
            // If this bit is 0 in the user's disable mask (user wants to enable it), we need to revise it
            if (lwrGpcDisMask & ~m_GpcMask)
            {
                m_GpcMask |= lwrGpcDisMask;
                Printf(Tee::PriWarn, "Adding the GPC current Mask, adjusting GPC disable mask to 0x%x\n", m_GpcMask);
            }

        }

        // Add FBP mask
        if (m_FsFbParamPresent)
        {
            UINT32 lwrFbpDisMask = regs.Read32(MODS_FUSE_STATUS_OPT_FBP);
            if (lwrFbpDisMask & ~m_FbMask)
            {
                m_FbMask |= lwrFbpDisMask;
                Printf(Tee::PriWarn, "Adding the FBP current Mask, adjusting FBP disable mask to 0x%x\n", m_FbMask);
            }
        }

        // Add FBIO mask
        if (m_FsFbioParamPresent)
        {
            UINT32 lwrFbioDisMask = regs.Read32(MODS_FUSE_STATUS_OPT_FBIO);
            if (lwrFbioDisMask & ~m_FbioMask)
            {
                m_FbioMask |= lwrFbioDisMask;
                Printf(Tee::PriWarn, "Adding the FBIO current Mask, adjusting FBIO disable mask to 0x%x\n", m_FbioMask);
            }
        }

        // Add FBPA mask
        if (regs.IsSupported(MODS_FUSE_STATUS_OPT_FBPA))
        {
            if (m_FsFbpaParamPresent)
            {
                UINT32 lwrFbpaDisMask = regs.Read32(MODS_FUSE_STATUS_OPT_FBPA);
                if (lwrFbpaDisMask & ~m_FbpaMask)
                {
                    m_FbpaMask |= lwrFbpaDisMask;
                    Printf(Tee::PriWarn, "Adding the FBPA current Mask, adjusting FBPA disable mask to 0x%x\n", m_FbpaMask);
                }
            }
        }

        // Add GPC TPC mask
        UINT32 gpcNum = 0;
        for (gpcNum = 0; gpcNum < m_GpcTpcMasks.size(); gpcNum++)
        {
            if (!m_FsGpcTpcParamPresent[gpcNum])
                continue;
            UINT32 lwrGpcTpcDisMask = regs.Read32(MODS_FUSE_STATUS_OPT_TPC_GPC, gpcNum);
            if (lwrGpcTpcDisMask & ~m_GpcTpcMasks[gpcNum])
            {
                m_GpcTpcMasks[gpcNum] |= lwrGpcTpcDisMask;
                Printf(Tee::PriWarn, "Adding the GPC TPC current Mask, adjusting GPC[%d] TPC disable mask to 0x%x\n", gpcNum, m_GpcTpcMasks[gpcNum]);
            }
        }

        // Add GPC PES mask
        for (gpcNum = 0; gpcNum < m_GpcPesMasks.size(); gpcNum++)
        {
            if (!m_FsGpcPesParamPresent[gpcNum])
                continue;
            UINT32 lwrGpcPesDisMask = regs.Read32(MODS_FUSE_STATUS_OPT_PES_GPC, gpcNum);
            if (lwrGpcPesDisMask & ~m_GpcPesMasks[gpcNum])
            {
                m_GpcPesMasks[gpcNum] |= lwrGpcPesDisMask;
                Printf(Tee::PriWarn, "Adding the GPC PES current Mask, adjusting GPC[%d] PES disable mask to 0x%x\n", gpcNum, m_GpcPesMasks[gpcNum]);
            }
        }

        // Add GPC ZLWLL mask
        if (regs.IsSupported(MODS_FUSE_STATUS_OPT_ZLWLL_GPC))
        {
            for (gpcNum = 0; gpcNum < m_GpcZlwllMasks.size(); gpcNum++)
            {
                if (!m_FsGpcZlwllParamPresent[gpcNum])
                    continue;
                UINT32 lwrGpcZlwllDisMask = regs.Read32(MODS_FUSE_STATUS_OPT_ZLWLL_GPC, gpcNum);
                if (lwrGpcZlwllDisMask & ~m_GpcZlwllMasks[gpcNum])
                {
                    m_GpcZlwllMasks[gpcNum] |= lwrGpcZlwllDisMask;
                    Printf(Tee::PriWarn, "Adding the GPC ZLWLL current Mask, adjusting GPC[%d] ZLWLL disable mask to 0x%x\n", gpcNum, m_GpcZlwllMasks[gpcNum]);

                }
            }
        }

        // Add ROP L2 mask
        if (regs.IsSupported(MODS_FUSE_STATUS_OPT_ROP_L2_FBP))
        {
            UINT32 fbpNum = 0;
            for (fbpNum = 0; fbpNum < m_FbpL2Masks.size(); fbpNum++)
            {
                if (!m_FsFbpL2ParamPresent[fbpNum])
                    continue;
                UINT32 lwrFbpL2DisMask = regs.Read32(MODS_FUSE_STATUS_OPT_ROP_L2_FBP, fbpNum);
                if (lwrFbpL2DisMask & ~m_FbpL2Masks[fbpNum])
                {
                    m_FbpL2Masks[fbpNum] |= lwrFbpL2DisMask;
                    Printf(Tee::PriWarn, "Adding the ROP L2 current Mask, adjusting FBP[%d] ROP L2 disable mask to 0x%x\n", fbpNum, m_FbpL2Masks[fbpNum]);
                }
            }
        }
    }
    return RC::OK;
}

//-----------------------------------------------------------------------------
RC AmpereFs::PopulateFsInforom
(
    LW90E7_CTRL_RPR_WRITE_OBJECT_PARAMS *pIfrParams
) const
{
    RC rc;
    MASSERT(pIfrParams);

    RegHal &regs = m_pSub->Regs();

    UINT32 maxIfrDataCount;
    CHECK_RC(m_pSub->GetInfoRomRprMaxDataCount(&maxIfrDataCount));

    auto pSyspipeFs = find_if(m_FsData.begin(), m_FsData.end(),
                            [] (const FsData & f)-> bool { return f.type == FST_SYSPIPE; } );
    if ((pSyspipeFs != m_FsData.end()) && pSyspipeFs->bPresent)
    {
        CHECK_RC(PopulateFsInforomEntry(pIfrParams,
                                        regs.LookupAddress(regs.ColwertToAddress(pSyspipeFs->ctrlField)),
                                        pSyspipeFs->mask,
                                        maxIfrDataCount));
    }


    auto pLwjpgFs = find_if(m_FsData.begin(), m_FsData.end(),
                            [] (const FsData & f)-> bool { return f.type == FST_LWJPG; } );
    if ((pLwjpgFs != m_FsData.end()) && pLwjpgFs->bPresent)
    {
        CHECK_RC(PopulateFsInforomEntry(pIfrParams,
                                        regs.LookupAddress(regs.ColwertToAddress(pLwjpgFs->ctrlField)),
                                        pLwjpgFs->mask,
                                        maxIfrDataCount));
    }

    auto pOfaFs = find_if(m_FsData.begin(), m_FsData.end(),
                            [] (const FsData & f)-> bool { return f.type == FST_OFA; } );
    if ((pOfaFs != m_FsData.end()) && pOfaFs->bPresent)
    {
        CHECK_RC(PopulateFsInforomEntry(pIfrParams,
                                        regs.LookupAddress(regs.ColwertToAddress(pOfaFs->ctrlField)),
                                        pOfaFs->mask,
                                        maxIfrDataCount));
    }

    CHECK_RC(TU10xFs::PopulateFsInforom(pIfrParams));

    return rc;
}

/* virtual */ void AmpereFs::PopulateGpcDisableMasks(JsonItem *pJsi) const
{
    MASSERT(pJsi);

    RegHal &regs = m_pSub->Regs();
    const UINT32 maxGpcs = m_pSub->GetMaxGpcCount();
    const UINT32 maxTpcs = m_pSub->GetMaxTpcCount();
    const UINT32 maxPes  = m_pSub->GetMaxPesCount();
    string gpcDsblMask;
    string pesDsblMask;
    string tpcDsblMask;

    gpcDsblMask = Utility::StrPrintf("0x%x", 
                    (regs.Read32(MODS_FUSE_STATUS_OPT_GPC) & ((1 << maxGpcs) - 1)));
    for (UINT32 gpcNum = 0; gpcNum < maxGpcs; gpcNum++)
    {
        pesDsblMask += Utility::StrPrintf("%s0x%x'", 
                        (gpcNum == 0 ? "'" : ", '"), 
                        (regs.Read32( MODS_FUSE_STATUS_OPT_PES_GPC, gpcNum) & ((1 << maxPes) - 1))
                    );
        tpcDsblMask += Utility::StrPrintf("%s0x%x'", 
                        (gpcNum == 0 ? "'" : ", '"), 
                        (regs.Read32(MODS_FUSE_STATUS_OPT_TPC_GPC, gpcNum) & ((1 << maxTpcs) - 1))
                    );
    }

    pJsi->SetField("OPT_GPC_DISABLE", gpcDsblMask.c_str());
    pJsi->SetField("OPT_PES_DISABLE", ("[" +  pesDsblMask + "]").c_str());
    pJsi->SetField("OPT_TPC_DISABLE", ("[" +  tpcDsblMask + "]").c_str());
}

/* virtual */ void AmpereFs::PopulateFbpDisableMasks(JsonItem *pJsi) const
{
    MASSERT(pJsi);

    RegHal &regs = m_pSub->Regs();
    bool isFbpaSupported = regs.IsSupported(MODS_FUSE_STATUS_OPT_FBPA);
    const UINT32 maxFbps = m_pSub->GetMaxFbpCount();
    const UINT32 maxFbpas = regs.Read32(MODS_PTOP_SCAL_NUM_FBPAS);
    const UINT32 fbpaStride = maxFbpas / maxFbps;
    const UINT32 maxLtcs = regs.Read32(MODS_PTOP_SCAL_NUM_LTC_PER_FBP);
    const UINT32 fbioMask = regs.Read32(MODS_FUSE_STATUS_OPT_FBIO);
    string fbpDsblMask;
    string fbpaDsblMask;
    string fbioDsblMask;
    string ltcDsblMask;

    fbpDsblMask = Utility::StrPrintf("0x%x", 
                    (regs.Read32(MODS_FUSE_STATUS_OPT_FBP) & ((1 << maxFbps) - 1)));
    for (UINT32 fbpNum = 0; fbpNum < maxFbps; fbpNum++)
    {
        if (isFbpaSupported)
        {
            fbpaDsblMask += Utility::StrPrintf("%s0x%x'", 
                            (fbpNum == 0 ? "'" : ", '"), 
                            (regs.Read32(MODS_FUSE_STATUS_OPT_FBPA, fbpNum) & ((1 << fbpaStride) - 1))
                        );
        }
        // FPBA stride == FBIO stride
        fbioDsblMask += Utility::StrPrintf("%s0x%x'", 
                            (fbpNum == 0 ? "'" : ", '"),
                            ((fbioMask >> fbpNum * fbpaStride) & ((1 << fbpaStride) - 1))
                        );
        ltcDsblMask += Utility::StrPrintf("%s0x%x'", 
                            (fbpNum == 0 ? "'" : ", '"), 
                            (regs.Read32(MODS_FUSE_STATUS_OPT_LTC_FBP, fbpNum) & ((1 << maxLtcs) - 1))
                        );
    }

    pJsi->SetField("OPT_FBP_DISABLE", fbpDsblMask.c_str());
    if (isFbpaSupported)
    {
        pJsi->SetField("OPT_FBPA_DISABLE", ("[" +  fbpaDsblMask + "]").c_str());
    }
    pJsi->SetField("OPT_FBIO_DISABLE", ("[" + fbioDsblMask + "]").c_str());
    pJsi->SetField("OPT_LTC_DISABLE", ("[" +  ltcDsblMask + "]").c_str());
}

/* virtual */ void AmpereFs::PopulateMiscDisableMasks(JsonItem *pJsi) const
{
    MASSERT(pJsi);

    if (m_pSub->Regs().IsSupported(MODS_FUSE_STATUS_OPT_SYS_PIPE))
    {
        pJsi->SetField("OPT_SYS_PIPE_DISABLE", 
            Utility::StrPrintf("0x%x", SyspipeMaskDisable()).c_str());
    }
    pJsi->SetField("OPT_OFA_DISABLE", Utility::StrPrintf("0x%x", OfaMaskDisable()).c_str());
    pJsi->SetField("OPT_LWJPG_DISABLE", Utility::StrPrintf("0x%x", LwjpgMaskDisable()).c_str());
    pJsi->SetField("OPT_LWENC_DISABLE", Utility::StrPrintf("0x%x", LwencMaskDisable()).c_str());
    pJsi->SetField("OPT_LWDEC_DISABLE", Utility::StrPrintf("0x%x", LwdecMaskDisable()).c_str());
}

//-----------------------------------------------------------------------------
CREATE_GPU_FS_FUNCTION(AmpereFs);
