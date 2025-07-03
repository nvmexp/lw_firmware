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

#include "tu10xfs.h"

#include "class/cl90e7.h"
#include "core/include/platform.h"
#include "device/interface/xusbhostctrl.h"
#include "gpu/reghal/reghal.h"

#include <algorithm>

//------------------------------------------------------------------------------
// access the below parameters via:
// mods ... mdiag.js -fermi_fs <feature-string>
// a "feature-string" is <name>:<value>[:<value>...]
//
static const ParamDecl s_FloorSweepingParamsTU10x[] =
{
    { "lwlink_enable",           "u", 0, 0, 0, "lwlink mask enable"},
    { "lwlink_disable",          "u", 0, 0, 0, "lwlink mask disable"},
    { "half_ltc",                  0, 0, 0, 0, "half L2-Cache mode"},
    LAST_PARAM
};

static ParamConstraints s_FsParamConstraintsTU10x[] =
{
   MUTUAL_EXCLUSIVE_PARAM("lwlink_enable", "lwlink_disable"),
   LAST_CONSTRAINT
};

//------------------------------------------------------------------------------
UINT32 TU10xFs::LwLinkMask() const
{
    // There is only one IOCTRL on Turing and there is no define in hwproject.h for it
    return ~m_pSub->Regs().Read32(MODS_FUSE_STATUS_OPT_LWLINK) & 1;
}

//------------------------------------------------------------------------------
// Floorsweeping contains register access protocols that cause non-RTL
// and non-HW pre-Pascal models severe indigestion, so we don't attempt HW
// floorsweeping on the SW models. However there is an effort to get this working
// on FModel for Turing. So allow it for now. If it doesn't work we can remove
// this function and fallback to the original implementation.
//
/*virtual*/ bool TU10xFs::IsFloorsweepingAllowed() const
{
    Platform::SimulationMode simMode = Platform::GetSimulationMode();
    if (simMode == Platform::Amodel || Platform::IsVirtFunMode())
    {
        return false;
    }
    return true;
}

void TU10xFs::ApplyFloorsweepingSettings()
{
    GV10xFs::ApplyFloorsweepingSettings();

    auto &regs = m_pSub->Regs();

    if (m_FsLwlinkParamPresent)
    {
        regs.Write32Sync(MODS_FUSE_CTRL_OPT_LWLINK_DATA, m_LwlinkMask);
    }
    if (m_FsHalfLtcParamPresent)
    {
        regs.Write32Sync(MODS_FUSE_CTRL_OPT_HALF_LTC_DATA_ENABLE);
    }
}

void TU10xFs::RestoreFloorsweepingSettings()
{
    auto &regs = m_pSub->Regs();

    if (m_FsLwlinkParamPresent)
    {
        regs.Write32Sync(MODS_FUSE_CTRL_OPT_LWLINK_DATA, m_SavedFsLwlinkControl);
    }
    if (m_FsHalfLtcParamPresent)
    {
        regs.Write32Sync(MODS_FUSE_CTRL_OPT_HALF_LTC_DATA_ENABLE);
    }

    GV10xFs::RestoreFloorsweepingSettings();
}

void TU10xFs::SaveFloorsweepingSettings()
{
    (void)m_pSub->Regs().Read32Priv(MODS_FUSE_CTRL_OPT_LWLINK_DATA,
                                    &m_SavedFsLwlinkControl);
    GV10xFs::SaveFloorsweepingSettings();
}

void TU10xFs::ResetFloorsweepingSettings()
{
    auto &regs = m_pSub->Regs();

    UINT32 lwLinkData;
    if (RC::OK == regs.Read32Priv(MODS_FUSE_OPT_LWLINK_DISABLE_DATA,
                                  &lwLinkData))
    {
        (void)regs.Write32PrivSync(MODS_FUSE_CTRL_OPT_LWLINK_DATA, lwLinkData);
    }

    UINT32 halfLtcData;
    if (RC::OK == regs.Read32Priv(MODS_FUSE_OPT_HALF_LTC_ENABLE, &halfLtcData))
    {
        (void)regs.Write32PrivSync(MODS_FUSE_CTRL_OPT_HALF_LTC, halfLtcData);
    }

    GV10xFs::ResetFloorsweepingSettings();
}

bool TU10xFs::IsIfrFsEntry(UINT32 address) const
{
    // On Turing, the entire RPR object is used only for FS
    // Assume all entries from the inforom contain FS information
    return true;
}

RC TU10xFs::PreserveFloorsweepingSettings() const
{
    RC rc;

    // Turing floorsweeping preservation is re-using the RPR object
    // that was used for GP100 Lane Repair. Only GP100 chips use the
    // RPR object for Lane Repair, so FS has full control on Turing.
    LW90E7_CTRL_RPR_WRITE_OBJECT_PARAMS ifrParams = { 0 };

    CHECK_RC(PopulateFsInforom(&ifrParams));
    CHECK_RC(LwRmPtr()->ControlSubdeviceChild(m_pSub, GF100_SUBDEVICE_INFOROM,
        LW90E7_CTRL_CMD_RPR_WRITE_OBJECT, &ifrParams, sizeof(ifrParams)));

    return rc;
}

RC TU10xFs::PopulateFsInforomEntry
(
    LW90E7_CTRL_RPR_WRITE_OBJECT_PARAMS *pIfrParams,
    UINT32 address,
    UINT32 data,
    UINT32 maxIfrDataCount
) const
{
    UINT32& entry = pIfrParams->entryCount;

    if (entry > maxIfrDataCount)
    {
        Printf(Tee::PriError, "Too many entries for Preserved FS! Max %u\n",
                               maxIfrDataCount);
        return RC::SOFTWARE_ERROR;
    }

    pIfrParams->repairData[entry].address = address;
    pIfrParams->repairData[entry].data    = data;
    entry++;

    return RC::OK;
}

RC TU10xFs::PopulateFsInforom
(
    LW90E7_CTRL_RPR_WRITE_OBJECT_PARAMS *pIfrParams
) const
{
    RC rc;
    MASSERT(pIfrParams);

    RegHal &regs = m_pSub->Regs();

    UINT32 maxIfrDataCount;
    CHECK_RC(m_pSub->GetInfoRomRprMaxDataCount(&maxIfrDataCount));

    if (m_FsGpcParamPresent)
    {
        CHECK_RC(PopulateFsInforomEntry(pIfrParams,
                                        regs.LookupAddress(MODS_FUSE_CTRL_OPT_GPC),
                                        m_GpcMask,
                                        maxIfrDataCount));
    }
    for (UINT32 gpcNum = 0; gpcNum < m_GpcTpcMasks.size(); gpcNum++)
    {
        if (!m_FsGpcTpcParamPresent[gpcNum])
            continue;

        CHECK_RC(PopulateFsInforomEntry(pIfrParams,
                                        regs.LookupAddress(MODS_FUSE_CTRL_OPT_TPC_GPC, gpcNum),
                                        m_GpcTpcMasks[gpcNum],
                                        maxIfrDataCount));
    }
    for (UINT32 gpcNum = 0; gpcNum < m_GpcPesMasks.size(); gpcNum++)
    {
        if (!m_FsGpcPesParamPresent[gpcNum])
            continue;

        CHECK_RC(PopulateFsInforomEntry(pIfrParams,
                                        regs.LookupAddress(MODS_FUSE_CTRL_OPT_PES_GPC, gpcNum),
                                        m_GpcPesMasks[gpcNum],
                                        maxIfrDataCount));
    }

    if (m_FsFbParamPresent)
    {
        CHECK_RC(PopulateFsInforomEntry(pIfrParams,
                                        regs.LookupAddress(MODS_FUSE_CTRL_OPT_FBP),
                                        m_FbMask,
                                        maxIfrDataCount));
    }
    if (m_FsFbioParamPresent || m_FsFbpaParamPresent)
    {
        // FBPA and FBIO should be identical, but we don't require both
        // If both are present, make sure they match
        if (m_FsFbioParamPresent && m_FsFbpaParamPresent &&
            m_FbioMask != m_FbpaMask)
        {
            MASSERT(!"FBPA and FBIO masks must match!");
            return RC::BAD_COMMAND_LINE_ARGUMENT;
        }
        CHECK_RC(PopulateFsInforomEntry(pIfrParams,
                                        regs.LookupAddress(MODS_FUSE_CTRL_OPT_FBIO),
                                        m_FbioMask,
                                        maxIfrDataCount));
    }
    for (UINT32 fbpNum = 0; fbpNum < m_FbpL2Masks.size(); fbpNum++)
    {
        if (!m_FsFbpL2ParamPresent[fbpNum])
            continue;

        UINT32 address = 0;
        if (regs.IsSupported(MODS_FUSE_CTRL_OPT_ROP_L2_FBP, fbpNum))
        {
            address = regs.LookupAddress(MODS_FUSE_CTRL_OPT_ROP_L2_FBP, fbpNum);
        }
        else if (regs.IsSupported(MODS_FUSE_CTRL_OPT_LTC_FBP, fbpNum))
        {
            address = regs.LookupAddress(MODS_FUSE_CTRL_OPT_LTC_FBP, fbpNum);
        }
        CHECK_RC(PopulateFsInforomEntry(pIfrParams,
                                        address,
                                        m_FbpL2Masks[fbpNum],
                                        maxIfrDataCount));
    }

    if (m_FsHalfFbpaParamPresent)
    {
        CHECK_RC(PopulateFsInforomEntry(pIfrParams,
                                        regs.LookupAddress(MODS_FUSE_CTRL_OPT_HALF_FBPA),
                                        m_HalfFbpaMask,
                                        maxIfrDataCount));
    }

    if (m_FsHalfLtcParamPresent)
    {
        CHECK_RC(PopulateFsInforomEntry(pIfrParams,
                                        regs.LookupAddress(MODS_FUSE_CTRL_OPT_HALF_LTC),
                                        1,
                                        maxIfrDataCount));
    }

    if (m_FsLwdecParamPresent)
    {
        CHECK_RC(PopulateFsInforomEntry(pIfrParams,
                                        regs.LookupAddress(MODS_FUSE_CTRL_OPT_LWDEC),
                                        m_LwdecMask,
                                        maxIfrDataCount));
    }
    if (m_FsLwencParamPresent)
    {
        CHECK_RC(PopulateFsInforomEntry(pIfrParams,
                                        regs.LookupAddress(MODS_FUSE_CTRL_OPT_LWENC),
                                        m_LwencMask,
                                        maxIfrDataCount));
    }
 
    if (m_FsLwlinkParamPresent)
    {
        CHECK_RC(PopulateFsInforomEntry(pIfrParams,
                                        regs.LookupAddress(MODS_FUSE_CTRL_OPT_LWLINK),
                                        m_LwlinkMask,
                                        maxIfrDataCount));
    }

    return rc;
}

RC TU10xFs::PrintPreservedFloorsweeping() const
{
    RC rc;

    // Turing floorsweeping preservation is using the RPR object
    // that was used for GP100 MemRepair. No Turing chips have
    // MemRepair, so FS has full control over the entries.
    LW90E7_CTRL_RPR_GET_INFO_PARAMS ifrParams = { 0 };

    CHECK_RC(LwRmPtr()->ControlSubdeviceChild(m_pSub, GF100_SUBDEVICE_INFOROM,
        LW90E7_CTRL_CMD_RPR_GET_INFO, &ifrParams, sizeof(ifrParams)));

    if (ifrParams.entryCount == 0)
    {
        return rc;
    }

    Printf(Tee::PriNormal, "FS IFR Records:\n");

    for (UINT32 i = 0; i < ifrParams.entryCount; i++)
    {
        auto& entry = ifrParams.repairData[i];
        if (IsIfrFsEntry(entry.address))
        {
            Printf(Tee::PriNormal, "[0x%08x] = 0x%08x\n", entry.address, entry.data);
        }
    }

    return rc;
}

RC TU10xFs::ReadFloorsweepingArgumentsImpl(ArgDatabase& fsArgDb)
{
    RC rc;

    CHECK_RC(GV10xFs::ReadFloorsweepingArgumentsImpl(fsArgDb));

    if (m_FsCeParamPresent)
    {
        Printf(Tee::PriError, "Floorsweeping the CE is no longer supported\n");
        return RC::BAD_COMMAND_LINE_ARGUMENT;
    }

    if (m_FsFbioShiftParamPresent)
    {
        Printf(Tee::PriError, "FBIO Shift is no longer supported\n");
        return RC::BAD_COMMAND_LINE_ARGUMENT;
    }

    // Turing doesn't support floorsweeping ZLWLL
    if (any_of(m_FsGpcZlwllParamPresent.begin(), m_FsGpcZlwllParamPresent.end(),
            [](bool b) { return b; }))
    {
        Printf(Tee::PriError, "Floorsweeping ZLWLL is no longer supported\n");
        return RC::BAD_COMMAND_LINE_ARGUMENT;
    }

    string FsArgs = GetFermiFsArgs();
    if (!FsArgs.empty())
    {
        auto &regs = m_pSub->Regs();

        // Check that CPU has write access to the FS registers
        // (i.e. Lv0 enabled), and print an error if it doesn't
        // (we can't fail here because some of the AS2 tasks would fail)
        if (!regs.Test32(MODS_FUSE_FLOORSWEEP_PRIV_LEVEL_MASK_WRITE_PROTECTION_ALL_LEVELS_ENABLED))
        {
            Printf(Tee::PriError, "-floorsweep disabled via priv mask\n");
        }

        ArgReader argReader(s_FloorSweepingParamsTU10x, s_FsParamConstraintsTU10x);
        if (!argReader.ParseArgs(&fsArgDb))
        {
            return RC::BAD_COMMAND_LINE_ARGUMENT;
        }

        m_FsLwlinkParamPresent = argReader.ParamPresent("lwlink_enable") ||
                                 argReader.ParamPresent("lwlink_disable");

        m_FsHalfLtcParamPresent = argReader.ParamPresent("half_ltc") > 0;

        m_LwlinkMask = ~argReader.ParamUnsigned("lwlink_enable", ~0U);
        m_LwlinkMask = argReader.ParamUnsigned("lwlink_disable", m_LwlinkMask);

        SetFloorsweepingAffected(GetFloorsweepingAffected()
            || m_FsLwlinkParamPresent
            || m_FsHalfLtcParamPresent);

        if (m_FsLwlinkParamPresent &&
            !regs.HasRWAccess(MODS_FUSE_CTRL_OPT_LWLINK))
        {
            Printf(Tee::PriError,
                   "lwlink floorsweeping not supported on %s\n",
                   Gpu::DeviceIdToString(m_pSub->DeviceId()).c_str());
            return RC::BAD_COMMAND_LINE_ARGUMENT;
        }
        if (m_FsHalfLtcParamPresent &&
            (!regs.IsSupported(MODS_FUSE_CTRL_OPT_HALF_LTC) ||
             !regs.HasRWAccess(MODS_FUSE_CTRL_OPT_HALF_LTC)))
        {
            Printf(Tee::PriError,
                   "half_ltc floorsweeping not supported on %s\n",
                   Gpu::DeviceIdToString(m_pSub->DeviceId()).c_str());
            return RC::BAD_COMMAND_LINE_ARGUMENT;
        }
    }

    return OK;
}

void TU10xFs::PrintFloorsweepingParams()
{
    GV10xFs::PrintFloorsweepingParams();
    Tee::Priority pri = GetFloorsweepingPrintPriority();

    Printf(pri, "TU10x floorsweeping parameters present on commandline:%s%s\n",
        (m_FsLwlinkParamPresent ? " lwlink" : ""),
        (m_FsHalfLtcParamPresent ? " half_ltc" : ""));
    Printf(pri, "TU10x floorsweeping parameter mask values: lwlink=0x%x\n",
        m_LwlinkMask);
}

/* virtual */ UINT32 TU10xFs::GetEngineResetMask()
{
    RegHal &regs = m_pSub->Regs();
    // This mask was pulled from RM's bifGetValidEnginesToReset_TU101
    return regs.LookupMask(MODS_PMC_ENABLE_PGRAPH) |
           regs.LookupMask(MODS_PMC_ENABLE_PMEDIA) |
           regs.LookupMask(MODS_PMC_ENABLE_CE0) |
           regs.LookupMask(MODS_PMC_ENABLE_CE1) |
           regs.LookupMask(MODS_PMC_ENABLE_CE2) |
           regs.LookupMask(MODS_PMC_ENABLE_CE3) |
           regs.LookupMask(MODS_PMC_ENABLE_CE4) |
           regs.LookupMask(MODS_PMC_ENABLE_CE5) |
           regs.LookupMask(MODS_PMC_ENABLE_CE6) |
           regs.LookupMask(MODS_PMC_ENABLE_CE7) |
           regs.LookupMask(MODS_PMC_ENABLE_CE8) |
           regs.LookupMask(MODS_PMC_ENABLE_PFIFO) |
           regs.LookupMask(MODS_PMC_ENABLE_PWR) |
           regs.LookupMask(MODS_PMC_ENABLE_PDISP) |
           regs.LookupMask(MODS_PMC_ENABLE_LWDEC0) |
           regs.LookupMask(MODS_PMC_ENABLE_LWDEC1) |
           regs.LookupMask(MODS_PMC_ENABLE_LWDEC2) |
           regs.LookupMask(MODS_PMC_ENABLE_LWENC0) |
           regs.LookupMask(MODS_PMC_ENABLE_LWENC1) |
           regs.LookupMask(MODS_PMC_ENABLE_SEC);
}

//-----------------------------------------------------------------------------
//! Trigger a SW reset
/* virtual */ RC TU10xFs::SwReset(bool bReinitPriRing)
{
    RC rc;

    // First Reset PMC
    // This code was added to solve a problem that RM was seeing with the Microsoft new Win8 TDR
    // tests. The GPU is not hung but given more work then it can handle in 2 seconds. As a
    // result we have some outstanding IO operations that will cause us issues in the future.
    // Although this should not be the case here we are doing it for insurance.
    UINT32 pmcMask = GetEngineResetMask();
    RegHal &regs = m_pSub->Regs();

    UINT32 pmcValue = regs.Read32(MODS_PMC_ENABLE);
    pmcValue &= ~pmcMask;
    regs.Write32(MODS_PMC_ENABLE, pmcValue);

    // need a couple dummy reads
    regs.Read32(MODS_PMC_ENABLE);
    regs.Read32(MODS_PMC_ENABLE);

    CHECK_RC(m_pSub->HotReset(GpuSubdevice::FundamentalResetOptions::Disable));

    if (bReinitPriRing)
    {
        CHECK_RC(InitializePriRing_FirstStage(false));
        CHECK_RC(InitializePriRing());
    }

    return OK;
}

//-----------------------------------------------------------------------------
void TU10xFs::FbEnable(bool bEnable)
{
    RegHal &regs = m_pSub->Regs();

    UINT32 pmcElpgValue = regs.Read32(MODS_PMC_ELPG_ENABLE);
    if (bEnable)
    {
        // Enabling FB means disabling ELPG
        regs.SetField(&pmcElpgValue, MODS_PMC_ELPG_ENABLE_HUB_DISABLED);
        regs.SetField(&pmcElpgValue, MODS_PMC_ELPG_ENABLE_L2_DISABLED);
        regs.SetField(&pmcElpgValue, MODS_PMC_ELPG_ENABLE_XBAR_DISABLED);
    }
    else
    {
        // Disabling FB means enabling ELPG
        regs.SetField(&pmcElpgValue, MODS_PMC_ELPG_ENABLE_HUB_ENABLED);
        regs.SetField(&pmcElpgValue, MODS_PMC_ELPG_ENABLE_L2_ENABLED);
        regs.SetField(&pmcElpgValue, MODS_PMC_ELPG_ENABLE_XBAR_ENABLED);
    }
    regs.Write32(MODS_PMC_ELPG_ENABLE, pmcElpgValue);
}

UINT32 TU10xFs::GetHalfFbpaWidth() const
{
    if (m_pSub->HasBug(2156704))
    {
        // TU10x has 1 half_fbpa_enable bit per FBP, but keeps
        // the 2 FBPA/FBP stride of TU101 (defunct).
        return 2;
    }
    else
    {
        return GP10xFs::GetHalfFbpaWidth();
    }
}

CREATE_GPU_FS_FUNCTION(TU10xFs);
