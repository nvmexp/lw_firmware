/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2016,2018-2020 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#include "gv10xfs.h"
#include "gpu/reghal/reghal.h"

//------------------------------------------------------------------------------
/* virtual */ UINT32 GV10xFs::SmMask(UINT32 HwGpcNum, UINT32 HwTpcNum) const
{
    return 3;
}

//------------------------------------------------------------------------------
/* virtual */ UINT32 GV10xFs::HalfFbpaMask() const
{
    return m_pSub->Regs().Read32(MODS_FUSE_STATUS_OPT_HALF_FBPA_DATA);
}

//------------------------------------------------------------------------------
/* virtual */ UINT32 GV10xFs::PceMask() const
{
    // No floorsweeping of PCEs on volta
    return (1U << m_pSub->GetMaxPceCount()) - 1;
}

/* virtual */ UINT32 GV10xFs::GetEngineResetMask()
{
    RegHal &regs = m_pSub->Regs();
    // This mask was actually pulled from RM's bifGetValidEnginesToReset_GV100
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
           regs.LookupMask(MODS_PMC_ENABLE_LWDEC) |
           regs.LookupMask(MODS_PMC_ENABLE_LWENC0) |
           regs.LookupMask(MODS_PMC_ENABLE_LWENC1) |
           regs.LookupMask(MODS_PMC_ENABLE_LWENC2) |
           regs.LookupMask(MODS_PMC_ENABLE_SEC);
}

void GV10xFs::ApplyFloorsweepingSettings()
{
    GP10xFs::ApplyFloorsweepingSettings();

    if (m_FsHalfFbpaParamPresent)
    {
        m_pSub->Regs().Write32Sync(MODS_FUSE_CTRL_OPT_HALF_FBPA_DATA, m_HalfFbpaMask);
    }
}

void GV10xFs::RestoreFloorsweepingSettings()
{
    if (m_FsHalfFbpaParamPresent)
    {
        m_pSub->Regs().Write32Sync(MODS_FUSE_CTRL_OPT_HALF_FBPA_DATA, m_SavedHalfFbpaMask);
    }

    GP10xFs::RestoreFloorsweepingSettings();
}

void GV10xFs::SaveFloorsweepingSettings()
{
    (void)m_pSub->Regs().Read32Priv(MODS_FUSE_CTRL_OPT_HALF_FBPA_DATA,
                                    &m_SavedHalfFbpaMask);
    GP10xFs::SaveFloorsweepingSettings();
}

void GV10xFs::ResetFloorsweepingSettings()
{
    RegHal& regs = m_pSub->Regs();
    UINT32 fbpaEnableMask;
    if (RC::OK == regs.Read32Priv(MODS_FUSE_OPT_HALF_FBPA_ENABLE,
                                  &fbpaEnableMask))
    {
        (void)regs.Write32PrivSync(MODS_FUSE_CTRL_OPT_HALF_FBPA_DATA,
                                   fbpaEnableMask);
    }

    UINT32 maxGpcs = m_pSub->GetMaxGpcCount();
    for (UINT32 gpcIndex = 0; gpcIndex < maxGpcs; gpcIndex++)
    {
        UINT32 tpc;
        if (RC::OK == regs.Read32Priv(MODS_FUSE_OPT_TPC_GPCx_DISABLE,
                                      gpcIndex, &tpc))
        {
            (void)regs.Write32Priv(MODS_FUSE_CTRL_OPT_TPC_GPC, gpcIndex, tpc);
        }
    }

    GP10xFs::ResetFloorsweepingSettings();
}

RC GV10xFs::CheckFloorsweepingArguments()
{
    if (m_FsHalfFbpaParamPresent &&
        !m_pSub->Regs().HasRWAccess(MODS_FUSE_CTRL_OPT_HALF_FBPA))
    {
        Printf(Tee::PriError,
               "half_fbpa floorsweeping not supported on %s\n",
               Gpu::DeviceIdToString(m_pSub->DeviceId()).c_str());
        return RC::BAD_COMMAND_LINE_ARGUMENT;
    }

    return GP10xFs::CheckFloorsweepingArguments();
}

RC GV10xFs::ReadFloorsweepingArgumentsImpl(ArgDatabase& fsArgDb)
{
    RC rc;

    // parse gp10x+ floorsweeping arguments
    CHECK_RC(GP10xFs::ReadFloorsweepingArgumentsImpl(fsArgDb));

    if (m_FsCeParamPresent)
    {
        Printf(Tee::PriHigh, "Floorsweeping the CE is no longer supported\n");
        return RC::BAD_COMMAND_LINE_ARGUMENT;
    }

    if (m_FsFbioShiftParamPresent)
    {
        Printf(Tee::PriHigh, "FBIO Shift is no longer supported\n");
        return RC::BAD_COMMAND_LINE_ARGUMENT;
    }

    return OK;
}

//------------------------------------------------------------------------------
CREATE_GPU_FS_FUNCTION(GV10xFs);

