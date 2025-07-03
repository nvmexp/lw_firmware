/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2014-2022 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#include "gp10xfs.h"
#include "core/include/utility.h"
#include "core/include/platform.h"
#include "core/include/argpproc.h"
#include "core/include/argread.h"
#include "core/include/argdb.h"
#include "gpu/reghal/reghal.h"
#include "pascal/gp100/dev_fuse.h"
#include "pascal/gp100/hwproject.h"
#include "pascal/gp100/dev_top.h"

//------------------------------------------------------------------------------
// access the below parameters via:
// mods ... mdiag.js -fermi_fs <feature-string>
// a "feature-string" is <name>:<value>[:<value>...]
//
static const ParamDecl s_FloorSweepingParamsGP10x[] =
{
    { "gpc_pes_enable",    "uu", ParamDecl::PARAM_MULTI_OK, 0, 0,
                              "PE enable. Usage <gpc_num> <pes_enable>"},
    { "gpc_pes_disable",   "uu", ParamDecl::PARAM_MULTI_OK, 0, 0,
                              "PE disable. Usage <gpc_num> <pes_disable>"},
    { "half_fbpa_enable",  "u", 0, 0, 0, "Half FBPA enable"},
    { "half_fbpa_disable", "u", 0, 0, 0, "Half FBPA disable"},
    { "hbm_enable",        "u", ParamDecl::PARAM_MULTI_OK, 0, 0,
                              "HBM site enable. Usage <hbm_site_enable>"},
    { "hbm_disable",       "u", ParamDecl::PARAM_MULTI_OK, 0, 0,
                              "HBM site disable. Usage <hbm_site_disable>"},
    LAST_PARAM
};

//------------------------------------------------------------------------------
static ParamConstraints s_FsParamConstraintsGP10x[] =
{
   MUTUAL_EXCLUSIVE_PARAM("gpc_pes_enable", "gpc_pes_disable"),
   MUTUAL_EXCLUSIVE_PARAM("half_fbpa_enable", "half_fbpa_disable"),
   MUTUAL_EXCLUSIVE_PARAM("hbm_enable", "hbm_disable"),
   LAST_CONSTRAINT
};

//------------------------------------------------------------------------------
GP10xFs::GP10xFs(GpuSubdevice *pSubdev):
    GM20xFs(pSubdev)
{
    m_FsGpcPesParamPresent.resize(LW_FUSE_CTRL_OPT_PES_GPC__SIZE_1, false);
    m_GpcPesMasks.resize(LW_FUSE_CTRL_OPT_PES_GPC__SIZE_1, 0);
    m_SavedGpcPesMasks.resize(LW_FUSE_CTRL_OPT_PES_GPC__SIZE_1);

    m_FsHalfFbpaParamPresent = false;
    m_HalfFbpaMask = 0;
    m_SavedHalfFbpaMask = 0;

    m_HbmParamPresent = false;
    m_HbmMask = 0;
}

//------------------------------------------------------------------------------
// | GP100 yes | GP102 ??? | GP104 ??? | GP107 yes | GP108 ??? |
RC GP10xFs::ReadFloorsweepingArgumentsImpl(ArgDatabase& fsArgDb)
{
    RC rc = OK;

    // parse gm20x+ floorsweeping arguments
    CHECK_RC(GM20xFs::ReadFloorsweepingArgumentsImpl(fsArgDb));

    // Pascal doesn't support floorsweeping the Copy Engine.
    if (m_FsCeParamPresent)
    {
        Printf(Tee::PriHigh, "Floorsweeping the CE is not support on Pascal\n");
        return RC::BAD_COMMAND_LINE_ARGUMENT;
    }

    string FsArgs = GetFermiFsArgs();
    if (!FsArgs.empty())
    {
        RegHal& reghal = m_pSub->Regs();
        // Check that CPU has write access to the FS registers
        // (i.e. Lv0 enabled), and print an error if it doesn't
        // (we can't fail here because some of the AS2 tasks would fail)
        if (!reghal.IsSupported(MODS_FUSE_FLOORSWEEP_PRIV_LEVEL_MASK) &&
            (reghal.IsSupported(MODS_FUSE_OPT_PRIV_LEVEL_MASK) &&
             !reghal.Test32(MODS_FUSE_OPT_PRIV_LEVEL_MASK_WRITE_PROTECTION_ALL_LEVELS_ENABLED)))
        {
            Printf(Tee::PriError, "-floorsweep disabled via priv mask\n");
        }

        ArgReader argReader(s_FloorSweepingParamsGP10x, s_FsParamConstraintsGP10x);
        if (!argReader.ParseArgs(&fsArgDb))
            return RC::BAD_COMMAND_LINE_ARGUMENT;

        // Now get the mask values. If the enable param is specified, use it's
        // bitwise negation as the mask value. If the disable param is specified,
        // use it directly as the mask. This code works because _enable and
        // _disable are mutually exclusive for a given feature.
        bool ilwert = false;
        const char *gpcPesParamName = "gpc_pes_disable";
        if (argReader.ParamPresent("gpc_pes_enable"))
        {
            ilwert = true;
            gpcPesParamName = "gpc_pes_enable";
        }
        UINT32 numGpcPesArgs = argReader.ParamPresent(gpcPesParamName);
        for (UINT32 i = 0; i < numGpcPesArgs; ++i)
        {
            UINT32 gpcNum    = argReader.ParamNUnsigned(gpcPesParamName, i, 0);
            UINT32 pesMask   = argReader.ParamNUnsigned(gpcPesParamName, i, 1);
            pesMask        = ilwert ? ~pesMask : pesMask;

            if (gpcNum >= m_GpcPesMasks.size())
            {
                Printf(Tee::PriHigh, "GP10xGpuSubdevice::ReadFloorsweeping"
                                     "Arguments: gpc # too large: %d\n", gpcNum);
                return RC::BAD_COMMAND_LINE_ARGUMENT;
            }
            if (!reghal.HasRWAccess(MODS_FUSE_CTRL_OPT_PES_GPC, gpcNum))
            {
                Printf(Tee::PriError,
                       "gpc_pes floorsweeping not supported on %s\n",
                       Gpu::DeviceIdToString(m_pSub->DeviceId()).c_str());
                return RC::BAD_COMMAND_LINE_ARGUMENT;
            }

            m_GpcPesMasks[gpcNum]          = pesMask;
            m_FsGpcPesParamPresent[gpcNum] = true;
        }

        bool gpcPesParamPresent = false;
        for (UINT32 i = 0; i < m_FsGpcPesParamPresent.size(); ++i )
        {
            if (m_FsGpcPesParamPresent[i])
            {
                gpcPesParamPresent = true;
                break;
            }
        }

        m_FsHalfFbpaParamPresent = argReader.ParamPresent("half_fbpa_enable") ||
                                   argReader.ParamPresent("half_fbpa_disable");
        m_HalfFbpaMask = ~argReader.ParamUnsigned("half_fbpa_enable", 0);
        m_HalfFbpaMask = ~argReader.ParamUnsigned("half_fbpa_disable", m_HalfFbpaMask);

        // Get HBM Parameters
        bool m_HbmParamPresent = argReader.ParamPresent("hbm_enable") ||
                                 argReader.ParamPresent("hbm_disable");
        m_HbmMask = ~argReader.ParamUnsigned("hbm_enable", ~0U);
        m_HbmMask =  argReader.ParamUnsigned("hbm_disable", m_HbmMask);

        if (m_HbmParamPresent)
        {
            UINT32 fbpNum1, fbpNum2;
            UINT32 maxHbmSites = m_pSub->GetMaxFbpCount() / 2;
            for (UINT32 siteId = 0; siteId < maxHbmSites; siteId++)
            {
                if ((m_HbmMask >> siteId) & 1)
                {
                    if (m_pSub->GetHBMSiteFbps(siteId, &fbpNum1, &fbpNum2) != RC::OK)
                    {
                        Printf(Tee::PriError, "Invalid HBM mask 0x%x\n", m_HbmMask);
                        return RC::BAD_COMMAND_LINE_ARGUMENT;
                    }
                    Printf(Tee::PriWarn, "Disabling FBP %u, %u connected to HBM site %u\n",
                                          fbpNum1, fbpNum2, siteId);
                    m_FbMask |= ((1 << fbpNum1) | (1 << fbpNum2));
                    m_FsFbParamPresent |= true;
                }
            }
        }

        SetFloorsweepingAffected(GetFloorsweepingAffected()
            || gpcPesParamPresent
            || m_FsHalfFbpaParamPresent
            || m_HbmParamPresent);
    }

    return rc;
}

//------------------------------------------------------------------------------
// | GP100 yes | GP102 ??? | GP104 ??? | GP107 yes | GP108 ??? |
void GP10xFs::PrintFloorsweepingParams()
{
    GM20xFs::PrintFloorsweepingParams();
    Tee::Priority pri = GetFloorsweepingPrintPriority();

    Printf(pri, "GP10xGpuSubdevice: Floorsweeping parameters "
                "present on commandline: ");
    for (UINT32 i = 0; i < m_FsGpcPesParamPresent.size(); ++i )
    {
        if (m_FsGpcPesParamPresent[i])
        {
            Printf(pri, "gpc_pes[%d] ", i);
        }
    }
    Printf(pri, "\n");

    Printf(pri, "GP10xGpuSubdevice: Floorsweeping parameters "
                "mask values: ");
    for (UINT32 i = 0; i < m_GpcPesMasks.size(); ++i)
    {
        Printf(pri, "gpc_pes[%d]=0x%x ", i, m_GpcPesMasks[i]);
    }
    Printf(pri, "\n");

    if (m_FsHalfFbpaParamPresent)
    {
        Printf(pri, "half_fbpa=0x%x\n", m_HalfFbpaMask);
    }

    if (m_HbmParamPresent)
    {
        Printf(pri, "hbm=0x%x\n", m_HbmMask);
    }
}

//------------------------------------------------------------------------------
// ApplyFloorsweepingSettings passed via the command line arguments.
// Note:
// Most of the internal variables are maintained in FermiFs however the register
// structure for some of these registers have been simplified. The
// simplification requires utilization of new individual m_SavedFsxxxx vars.
//
// | GP100 yes | GP102 ??? | GP104 ??? | GP107 yes | GP108 ??? |
void GP10xFs::ApplyFloorsweepingSettings()
{
    ApplyFloorsweepingSettingsInternal();
    //The number of lwenc engines for GP100 is 3
    if (m_FsLwencParamPresent)
    {
        UINT32 regVal = m_SavedFsLwencControl;
        m_pSub->Regs().SetField(&regVal,
                                MODS_FUSE_CTRL_OPT_LWENC_DATA,
                                m_LwencMask);
        m_pSub->Regs().Write32Sync(MODS_FUSE_CTRL_OPT_LWENC, regVal);
    }
}
void GP10xFs::ApplyFloorsweepingSettingsInternal()
{
    GM20xFs::ApplyFloorsweepingSettings();
    // All floorsweeping registers should have a write with ack on non-hardware
    // platforms. see http://lwbugs/1043132

    for (UINT32 i = 0; i < m_GpcPesMasks.size(); i++)
    {
        if (m_FsGpcPesParamPresent[i])
        {
            m_pSub->Regs().Write32Sync(MODS_FUSE_CTRL_OPT_PES_GPC_DATA, i, m_GpcPesMasks[i]);
        }
    }
}

//------------------------------------------------------------------------------
/* virtual */ RC GP10xFs::CheckFloorsweepingArguments()
{
    RC rc;
    if (IsFloorsweepingAllowed())
    {
        UINT32 maxPesCount = m_pSub->GetMaxPesCount();
        for (UINT32 i = 0; i < m_GpcPesMasks.size(); i++)
        {
            if (m_FsGpcPesParamPresent[i])
            {
                CHECK_RC(CheckFloorsweepingMask(
                                m_pSub->Regs().Read32(MODS_FUSE_STATUS_OPT_PES_GPC, i),
                                m_GpcPesMasks[i], maxPesCount, "PES"));

            }
        }
    }
    return GM20xFs::CheckFloorsweepingArguments();
}

//------------------------------------------------------------------------------
// RestoreFloorsweepingSettings()
//
// restores floorsweeping registers that have been changed
//
// this function is only called if there definitely is at least one
// floorsweeping register that has changed
//
// | GP100 yes | GP102 ??? | GP104 ??? | GP107 yes | GP108 ??? |
void GP10xFs::RestoreFloorsweepingSettings()
{
    Printf(Tee::PriDebug, "GP100GpuSubdevice::RestoreFloorsweepingSettings: "
           " checking and restoring startup settings\n");

    for (UINT32 i = 0; i < m_FsGpcPesParamPresent.size(); ++i)
    {
        if (m_FsGpcPesParamPresent[i])
        {
            m_pSub->Regs().Write32Sync(MODS_FUSE_CTRL_OPT_PES_GPC, i,
                                       m_SavedGpcPesMasks[i]);
        }
    }

    return(GM20xFs::RestoreFloorsweepingSettings());
}

//------------------------------------------------------------------------------
// | GP100 yes | GP102 ??? | GP104 ??? | GP107 yes | GP108 ??? |
void GP10xFs::ResetFloorsweepingSettings()
{
    Printf(Tee::PriDebug, "GP100GpuSubdevice::ResetFloorsweepingSettings: "
           " reading and resetting settings\n");

    RegHal& regs = m_pSub->Regs();
    const UINT32 maxGpcs = m_pSub->GetMaxGpcCount();
    if (regs.IsSupported(MODS_FUSE_OPT_PES_DISABLE))
    {
        const UINT32 pesStride = PesOnGpcStride();
        const UINT32 gpcPes = regs.Read32(MODS_FUSE_OPT_PES_DISABLE);
        UINT32 mask = (1 << pesStride)-1;
        for (UINT32 gpcIndex = 0; gpcIndex < maxGpcs; gpcIndex++)
        {
            // write each fused value to the fuse control.
            UINT32 pes = (gpcPes & mask) >> (gpcIndex * pesStride);
            (void)regs.Write32Priv(MODS_FUSE_CTRL_OPT_PES_GPC, gpcIndex, pes);

            mask <<= pesStride;
        }
    }
    else if (regs.IsSupported(MODS_FUSE_OPT_PES_GPCx_DISABLE, 0))
    {
        for (UINT32 gpcIndex = 0; gpcIndex < maxGpcs; gpcIndex++)
        {
            UINT32 pes = regs.Read32(MODS_FUSE_OPT_PES_GPCx_DISABLE, gpcIndex);
            (void)regs.Write32Priv(MODS_FUSE_CTRL_OPT_PES_GPC, gpcIndex, pes);
        } 
    }

    GM20xFs::ResetFloorsweepingSettings();
}

//------------------------------------------------------------------------------
// | GP100 yes | GP102 ??? | GP104 ??? | GP107 yes | GP108 ??? |
void GP10xFs::SaveFloorsweepingSettings()
{
    // Save all floorsweeping settings that could conceivably be changed by this
    // GpuSubdevice (in ApplyFloorsweepingSettings)
    //
    const RegHal& regs = m_pSub->Regs();
    for (UINT32 i = 0; i < m_SavedGpcPesMasks.size(); ++i)
    {
        (void)regs.Read32Priv(MODS_FUSE_CTRL_OPT_PES_GPC, i,
                              &m_SavedGpcPesMasks[i]);
    }

    return(GM20xFs::SaveFloorsweepingSettings());
}

//------------------------------------------------------------------------------
// | GP100 yes | GP102 ??? | GP104 ??? | GP107 yes | GP108 ??? |
RC GP10xFs::GetFloorsweepingMasks
(
    FloorsweepingMasks* pFsMasks
) const
{
    if(Platform::IsVirtFunMode())
    {
        return RC::UNSUPPORTED_FUNCTION;
    }

    RC rc;

    MASSERT(pFsMasks != 0);
    pFsMasks->clear();

    CHECK_RC(GM20xFs::GetFloorsweepingMasks(pFsMasks));

    //Add any GP100 specific floorsweeping masks:
    UINT32 gpcMask = GpcMask();
    UINT32 gpcNum  = 0;
    while (gpcMask != 0)
    {
        if (gpcMask & 1)
        {
            string fsmaskName = Utility::StrPrintf("gpcPes[%d]_mask", gpcNum);
            pFsMasks->insert(pair<string, UINT32>(fsmaskName, PesMask(gpcNum)));
        }

        gpcNum++;
        gpcMask >>= 1;
    }

    return rc;
}

//------------------------------------------------------------------------------
/* virtual */ RC GP10xFs::PrintMiscFuseRegs()
{
    RC rc;
    CHECK_RC(GM10xFs::PrintMiscFuseRegs());
    
    RegHal& regs = m_pSub->Regs();
    if (regs.IsSupported(MODS_FUSE_OPT_PES_DISABLE))
    {
        Printf(Tee::PriNormal, " OPT_PES_DIS : 0x%x\n",
                                regs.Read32(MODS_FUSE_OPT_PES_DISABLE));
    }
    else if (regs.IsSupported(MODS_FUSE_OPT_PES_GPCx_DISABLE, 0))
    {
        const UINT32 maxGpcs = m_pSub->GetMaxGpcCount();
        for (UINT32 gpcIndex = 0; gpcIndex < maxGpcs; gpcIndex++)
        {
            Printf(Tee::PriNormal, " OPT_PES_GPC%u_DIS : 0x%x\n",
                                    gpcIndex,
                                    regs.Read32(MODS_FUSE_OPT_PES_GPCx_DISABLE, gpcIndex));
        }
    }

    return RC::OK;
}

//------------------------------------------------------------------------------
/* virtual */ UINT32 GP10xFs::PesMask(UINT32 HwGpcNum) const
{
    // Read Fuse status to get floorsweep mask
    UINT32 pes = 0;
    if (RC::OK != m_pSub->Regs().Read32Priv(MODS_FUSE_STATUS_OPT_PES_GPC,
                                            HwGpcNum, &pes))
    {
        Printf(Tee::PriWarn, "Cannot read PES mask\n");
    }
    UINT32 hwPesMask = ((1 << PesOnGpcStride()) -1);
    hwPesMask &= ~pes;

    return hwPesMask;
}

//------------------------------------------------------------------------------
/* virtual */ UINT32 GP10xFs::FbpMaskImpl() const
{
    // SOC GPUs don't have FB. Fake 1 FB partition for memory tests.
    if (m_pSub->IsSOC())
    {
        return 1;
    }

    // Read Fuse status to get floorsweep mask
    UINT32 fbpFuseStatus = 0;
    if (RC::OK != m_pSub->Regs().Read32Priv(MODS_FUSE_STATUS_OPT_FBP,
                                            &fbpFuseStatus))
    {
        Printf(Tee::PriWarn, "Cannot read FBP mask\n");
    }

    //Hardcoded value on any platforms
    const UINT32 defParts = m_pSub->Regs().Read32(MODS_PTOP_SCAL_NUM_FBPS);

    // Effective active Fbps
    // This is physical mapping. Note that virtual mapping (listed in *_PFB_FBPA_*
    // registers) will work sequentially without holes.
    // Mods should continue to write _PFB_FBPA0 then 1 then 2 ... without
    // worrying about physical fbp mask
    return (~fbpFuseStatus & ((1 << defParts) -1));
}

/* virtual */ void GP10xFs::CheckFbioCount()
{
    // No operation for GP10x. FBPA is disconnected only fbio registers are active
    // In gp100 and gp105, MODS_FUSE_STATUS_OPT_FBPA is useless.
    // We use opt_fbio_disable to floorsweep FBPA
    // The corresponding status register is MODS_FUSE_STATUS_OPT_FBIO
    // Refer bug Bug 200070139
    return;
}
//------------------------------------------------------------------------------
/* virtual */ UINT32 GP10xFs::FbioMaskImpl() const
{
    // SOC GPUs don't have FB. Fake 1 FB partition for memory tests.
    if (m_pSub->IsSOC())
    {
        return 1;
    }

    // Read Fuse status to get floorsweep mask
    UINT32 fbioFuseStatus = 0;
    if (RC::OK != m_pSub->Regs().Read32Priv(MODS_FUSE_STATUS_OPT_FBIO,
                                            &fbioFuseStatus))
    {
        Printf(Tee::PriWarn, "Cannot read FBIO mask\n");
    }

    // The Fbio to Fbp ratio is the same as the Fbpa to Fbp ratio.
    // However we don't have a register to read to get the #FBIOs per FBP or #FBIOs
    // per GPU, so we will use LW_PTOP_SCAL_NUM_FBPAS instead.
    const UINT32 allFbiosMask = (1 << m_pSub->Regs().Read32(MODS_PTOP_SCAL_NUM_FBPAS))-1;

    // Effective active Fbios
    return (~fbioFuseStatus & allFbiosMask);
}

//------------------------------------------------------------------------------
/* virtual */ UINT32 GP10xFs::FbpaMaskImpl() const
{
    // MODS_FUSE_STATUS_OPT_FBPA is useless on GP10x
    // FbMask is equal to active Fbio Mask
    // Following change is in response to http://lwbugs/1563948/26
    return FbioMaskImpl();
}

//------------------------------------------------------------------------------
/*virtual*/ UINT32 GP10xFs::LwencMask() const
{
    // There aren't any lwdec on SOC GPUs
    if (m_pSub->IsSOC())
    {
        return 0;
    }

    switch (Platform::GetSimulationMode())
    {
        case Platform::Hardware:
        case Platform::Fmodel:
        case Platform::Amodel:
        case Platform::RTL: // Hmm.. not too sure about this one.
             return (~m_pSub->Regs().Read32(MODS_FUSE_STATUS_OPT_LWENC)) &
                 ((1 << m_pSub->Regs().LookupArraySize(MODS_FUSE_STATUS_OPT_LWENC_IDX, 1)) - 1);

        default:
            MASSERT(!"Illegal case");
            break;
    }
    return 0;
}

//------------------------------------------------------------------------------
/*virtual*/ void GP10xFs::CheckIfPriRingInitIsRequired()
{
    m_IsPriRingInitRequired = m_IsPriRingInitRequired || m_FsHalfFbpaParamPresent;

    for (UINT32 i = 0; i < m_FsGpcPesParamPresent.size(); ++i)
        m_IsPriRingInitRequired = m_IsPriRingInitRequired || m_FsGpcPesParamPresent[i];

    GM20xFs::CheckIfPriRingInitIsRequired();
}

/* virtual */ UINT32 GP10xFs::GetEngineResetMask()
{
    RegHal &regs = m_pSub->Regs();
    // This mask was actually pulled from RM's bifGetValidEnginesToReset_GP10X
    return regs.LookupMask(MODS_PMC_ENABLE_PGRAPH) |
           regs.LookupMask(MODS_PMC_ENABLE_PMEDIA) |
           regs.LookupMask(MODS_PMC_ENABLE_CE0) |
           regs.LookupMask(MODS_PMC_ENABLE_CE1) |
           regs.LookupMask(MODS_PMC_ENABLE_CE2) |
           regs.LookupMask(MODS_PMC_ENABLE_PFIFO) |
           regs.LookupMask(MODS_PMC_ENABLE_PDISP) |
           regs.LookupMask(MODS_PMC_ENABLE_LWDEC) |
           regs.LookupMask(MODS_PMC_ENABLE_LWENC0) |
           regs.LookupMask(MODS_PMC_ENABLE_LWENC1) |
           regs.LookupMask(MODS_PMC_ENABLE_SEC);
}

//-----------------------------------------------------------------------------
RC GP10xFs::AdjustDependentFloorsweepingArguments()
{
    RC rc;
    CHECK_RC(AdjustGpcFloorsweepingArguments());
    CHECK_RC(AdjustFbpFloorsweepingArguments());
    return rc;
}

RC GP10xFs::AdjustGpcFloorsweepingArguments()
{
    RC rc;

    const UINT32 maxGpcs     = m_pSub->GetMaxGpcCount();
    const UINT32 maxTpcs     = m_pSub->GetMaxTpcCount();
    const UINT32 fullTpcMask = ((1 << maxTpcs) - 1);

    bool   gpcMaskModified = false;
    UINT32 tpcMaskModified = 0;

    CHECK_RC(AdjustPreGpcFloorsweepingArguments(&tpcMaskModified));
    for (UINT32 gpcNum = 0; gpcNum < maxGpcs; gpcNum++)
    {
        // GPC disabled, disable all underlying TPCs
        if (m_FsGpcParamPresent && ((m_GpcMask >> gpcNum) & 1))
        {
            if (!m_FsGpcTpcParamPresent[gpcNum])
            {
                m_GpcTpcMasks[gpcNum] = ~0U;
                m_FsGpcTpcParamPresent[gpcNum] = true;
                tpcMaskModified |= (1 << gpcNum);
            }
        }
        if (((tpcMaskModified >> gpcNum) & 0x1) == 0x1)
        {
            m_FsGpcTpcParamPresent[gpcNum] = true;
            Printf(Tee::PriWarn, "Adjusting TPC mask[%d] to 0x%x\n",
                                  gpcNum, m_GpcTpcMasks[gpcNum]);
        }

        // If all connected TPCs are disabled, disable the GPC
        if (!m_FsGpcParamPresent &&
             m_FsGpcTpcParamPresent[gpcNum] &&
            (m_GpcTpcMasks[gpcNum] & fullTpcMask) == fullTpcMask)
        {
            m_GpcMask |= 1 << gpcNum;
            gpcMaskModified = true;
        }
    }
    if (gpcMaskModified)
    {
        m_FsGpcParamPresent = true;
        Printf(Tee::PriWarn, "Adjusting GPC mask to 0x%x\n", m_GpcMask);
    }
    CHECK_RC(AdjustPostGpcFloorsweepingArguments());

    return RC::OK;
}

RC GP10xFs::AdjustPreGpcFloorsweepingArguments(UINT32 *pTpcMaskModified)
{
    return AdjustPesFloorsweepingArguments(pTpcMaskModified);
}

RC GP10xFs::AdjustPostGpcFloorsweepingArguments()
{
    return RC::OK;
}

RC GP10xFs::AdjustPesFloorsweepingArguments(UINT32 *pTpcMaskModified)
{
    MASSERT(pTpcMaskModified);

    const UINT32 maxGpcs = m_pSub->GetMaxGpcCount();
    const UINT32 maxTpcs = m_pSub->GetMaxTpcCount();
    const UINT32 maxPes  = m_pSub->GetMaxPesCount();

    *pTpcMaskModified = 0;
    if (maxPes != 0)
    {
        vector<UINT32> tpcPesConfig;
        Utility::GetUniformDistribution(maxPes, maxTpcs, &tpcPesConfig);

        for (UINT32 gpcNum = 0; gpcNum < maxGpcs; gpcNum++)
        {
            bool pesMaskModified = false;

            // GPC disabled, disable all underlying PESs
            if (m_FsGpcParamPresent && ((m_GpcMask >> gpcNum) & 1))
            {
                if (!m_FsGpcPesParamPresent[gpcNum])
                {
                    m_GpcPesMasks[gpcNum] = ~0U;
                    m_FsGpcPesParamPresent[gpcNum] = true;
                    pesMaskModified = true;
                }
            }

            // If PES is disabled, disable all underlying TPCs
            if (m_FsGpcPesParamPresent[gpcNum] && !m_FsGpcTpcParamPresent[gpcNum])
            {
                UINT32 pesMask = m_GpcPesMasks[gpcNum];
                UINT32 tpcPesShift = 0;
                for (UINT32 pesNum = 0; pesNum < maxPes; pesNum++)
                {
                    UINT32 pesConfigMask = (1 << tpcPesConfig[pesNum]) - 1;
                    if ((pesMask >> pesNum) & 1)
                    {
                        // Disable all TPCs in the PES
                        m_GpcTpcMasks[gpcNum] |= pesConfigMask << tpcPesShift;
                        *pTpcMaskModified |= (1 << gpcNum);
                    }
                    tpcPesShift += tpcPesConfig[pesNum];
                }
            }

            // If TPCs connected to a PES are disabled, disable the PES
            if (!m_FsGpcPesParamPresent[gpcNum] && m_FsGpcTpcParamPresent[gpcNum])
            {
                UINT32 tpcGpcMask = m_GpcTpcMasks[gpcNum];
                UINT32 tpcPesShift = 0;
                for (UINT32 pesNum = 0; pesNum < maxPes; pesNum++)
                {
                    UINT32 pesConfigMask = (1 << tpcPesConfig[pesNum]) - 1;
                    UINT32 tpcMask = (tpcGpcMask >> tpcPesShift) &
                                     pesConfigMask;
                    if (tpcMask == pesConfigMask)
                    {
                        // Disable the PES
                        m_GpcPesMasks[gpcNum] |= 1 << pesNum;
                        pesMaskModified = true;
                    }
                    tpcPesShift += tpcPesConfig[pesNum];
                }
            }

            if (pesMaskModified)
            {
                m_FsGpcPesParamPresent[gpcNum] = true;
                Printf(Tee::PriWarn, "Adjusting PES mask[%d] to 0x%x\n",
                                      gpcNum, m_GpcPesMasks[gpcNum]);
            }
        }
    }

    return RC::OK;
}

RC GP10xFs::AdjustFbpFloorsweepingArguments()
{
    auto &regs = m_pSub->Regs();

    // FBPA FS registers are not available on GA10x
    const bool fbpaFsValid = regs.IsSupported(MODS_FUSE_OPT_FBPA_DISABLE);

    const UINT32 maxFbps = m_pSub->GetMaxFbpCount();
    if (maxFbps != 0)
    {
        const UINT32 maxFbpas       = regs.Read32(MODS_PTOP_SCAL_NUM_FBPAS);
        const UINT32 fbiosPerFbp    = maxFbpas / maxFbps;
        const UINT32 fbioFbpMask    = (1 << fbiosPerFbp) - 1;
        const UINT32 ltcMask        = (1 << regs.Read32(MODS_PTOP_SCAL_NUM_LTC_PER_FBP)) - 1;
        const UINT32 halfFbpaStride = GetHalfFbpaWidth();
        const UINT32 halfFbpaMask   = (1 << halfFbpaStride) - 1;

        bool fbioMaskModified = false;
        bool fbpaMaskModified = false;
        // Handle disabled FBPs
        if (m_FsFbParamPresent)
        {
            for (UINT32 fbNum = 0; fbNum < maxFbps; fbNum++)
            {
                if ((m_FbMask >> fbNum) & 1)
                {
                    // Disable FBPAs
                    if (!m_FsFbpaParamPresent && fbpaFsValid)
                    {
                        m_FbpaMask |= fbioFbpMask << (fbiosPerFbp * fbNum);
                        fbpaMaskModified = true;
                    }

                    // Disable FBIOs
                    if (!m_FsFbioParamPresent)
                    {
                        m_FbioMask |= fbioFbpMask << (fbiosPerFbp * fbNum);
                        fbioMaskModified = true;
                    }

                    // Disable L2s
                    if (!m_FsFbpL2ParamPresent[fbNum])
                    {
                        m_FbpL2Masks[fbNum] = ltcMask;
                        m_FsFbpL2ParamPresent[fbNum] = true;
                        Printf(Tee::PriWarn, "Adjusting L2 mask[%d] to 0x%x\n",
                                              fbNum, m_FbpL2Masks[fbNum]);
                    }
                }
            }
        }
        if (fbpaMaskModified)
        {
            m_FsFbpaParamPresent = true;
            Printf(Tee::PriWarn, "Adjusting FBPA mask to 0x%x\n", m_FbpaMask);
        }
        if (fbioMaskModified)
        {
            m_FsFbioParamPresent = true;
            Printf(Tee::PriWarn, "Adjusting FBIO mask to 0x%x\n", m_FbioMask);
        }

        // Handle half fbpa for non-HBM chips
        bool halfFbpaModified = false;
        if (!m_FsHalfFbpaParamPresent && (m_pSub->GetNumHbmSites() == 0))
        {
            for (UINT32 fbNum = 0; fbNum < maxFbps; fbNum++)
            {
                if (m_FsFbpL2ParamPresent[fbNum] &&
                    m_FbpL2Masks[fbNum] != 0 &&
                    (m_FbpL2Masks[fbNum] & ltcMask) != ltcMask)
                {
                    m_HalfFbpaMask |= halfFbpaMask << (halfFbpaStride * fbNum);
                    halfFbpaModified = true;
                }
            }
        }
        if (halfFbpaModified)
        {
            m_FsHalfFbpaParamPresent = true;
            Printf(Tee::PriWarn, "Adjusting half fbpa mask to 0x%x\n", m_HalfFbpaMask);
        }
    }

    return RC::OK;
}

//------------------------------------------------------------------------------
UINT32 GP10xFs::GetHalfFbpaWidth() const
{
    auto &regs = m_pSub->Regs();
    const UINT32 maxFbps  = m_pSub->GetMaxFbpCount();
    const UINT32 maxFbpas = regs.Read32(MODS_PTOP_SCAL_NUM_FBPAS);
    return (maxFbps != 0 ? (maxFbpas / maxFbps) : 0);
}

//------------------------------------------------------------------------------
CREATE_GPU_FS_FUNCTION(GP10xFs);

