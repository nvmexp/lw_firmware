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

#include "ga10xfs.h"
#include "gpu/include/gpusbdev.h"
#include "gpu/reghal/reghal.h"
#include "core/include/gpu.h"
#include "core/include/platform.h"
#include "ampere/ga102/dev_fuse.h"

//------------------------------------------------------------------------------
static const ParamDecl s_FloorSweepingParamsGA10x[] =
{
    { "gpc_rop_enable",      "uu", ParamDecl::PARAM_MULTI_OK, 0, 0,
                              "ROP enable. Usage <gpc_num> <rop_enable>"},
    { "gpc_rop_disable",     "uu", ParamDecl::PARAM_MULTI_OK, 0, 0,
                              "ROP disable. Usage <gpc_num> <rop_disable>"},
    { "fbp_l2slice_enable",  "uu", ParamDecl::PARAM_MULTI_OK, 0, 0,
                              "L2 Slice enable. Usage <fbp_num> <l2slice_enable>"},
    { "fbp_l2slice_disable", "uu", ParamDecl::PARAM_MULTI_OK, 0, 0,
                              "L2 Slice disable. Usage <fbp_num> <l2slice_disable>"},
    LAST_PARAM
};

//------------------------------------------------------------------------------
static ParamConstraints s_FsParamConstraintsGA10x[] =
{
   MUTUAL_EXCLUSIVE_PARAM("gpc_rop_enable",     "gpc_rop_disable"),
   MUTUAL_EXCLUSIVE_PARAM("fbp_l2slice_enable", "fbp_l2slice_disable"),
   LAST_CONSTRAINT
};

//------------------------------------------------------------------------------
GA10xFs::GA10xFs(GpuSubdevice *pSubdev):
    AmpereFs(pSubdev)
{
    m_FsGpcRopParamPresent.resize(LW_FUSE_CTRL_OPT_ROP_GPC__SIZE_1, false);
    m_GpcRopMasks.resize(LW_FUSE_CTRL_OPT_ROP_GPC__SIZE_1, 0);
    m_SavedGpcRopMasks.resize(LW_FUSE_CTRL_OPT_ROP_GPC__SIZE_1);

    m_FsFbpL2SliceParamPresent.resize(LW_FUSE_CTRL_OPT_FBP_IDX__SIZE_1, false);
    m_FbpL2SliceMasks.resize(LW_FUSE_CTRL_OPT_FBP_IDX__SIZE_1, 0);
    m_SavedFbpL2SliceMasks.resize(LW_FUSE_CTRL_OPT_FBP_IDX__SIZE_1);
}

//------------------------------------------------------------------------------
RC GA10xFs::ReadFloorsweepingArgumentsImpl(ArgDatabase& fsArgDb)
{
    RC rc;

    // Read pre-GA10x floorsweeping arguments
    CHECK_RC(AmpereFs::ReadFloorsweepingArgumentsImpl(fsArgDb));

    if (m_FsFbpaParamPresent)
    {
        Printf(Tee::PriWarn, "FBPA floorsweeping is no longer supported. "
                             "Please remove fbpa_enable/disable from your command line\n");
        m_FsFbpaParamPresent = false;
    }

    string FsArgs = GetFermiFsArgs();
    if (!FsArgs.empty())
    {
        ArgReader argReader(s_FloorSweepingParamsGA10x, s_FsParamConstraintsGA10x);
        if (!argReader.ParseArgs(&fsArgDb))
        {
            return RC::BAD_COMMAND_LINE_ARGUMENT;
        }

        // Get gpc_rop FS params
        bool ilwert = false;
        string gpcRopParamName = "gpc_rop_disable";
        if (argReader.ParamPresent("gpc_rop_enable"))
        {
            ilwert = true;
            gpcRopParamName = "gpc_rop_enable";
        }
        bool gpcRopParamPresent = false;
        UINT32 numGpcRopArgs = argReader.ParamPresent(gpcRopParamName.c_str());
        for (UINT32 i = 0; i < numGpcRopArgs; i++)
        {
            UINT32 gpcNum  = argReader.ParamNUnsigned(gpcRopParamName.c_str(), i, 0);
            UINT32 ropMask = argReader.ParamNUnsigned(gpcRopParamName.c_str(), i, 1);
            ropMask        = ilwert ? ~ropMask : ropMask;

            if (gpcNum >= m_GpcRopMasks.size())
            {
                Printf(Tee::PriError, "GA10xGpuSubdevice::ReadFloorsweeping"
                                      "Arguments: GPC # too large: %d\n", gpcNum);
                return RC::BAD_COMMAND_LINE_ARGUMENT;
            }

            m_GpcRopMasks[gpcNum]          = ropMask;
            m_FsGpcRopParamPresent[gpcNum] = true;
            gpcRopParamPresent             = true;
        }

        // Get fbp_l2slice FS params
        ilwert = false;
        string fbpL2SliceParamName = "fbp_l2slice_disable";
        if (argReader.ParamPresent("fbp_l2slice_enable"))
        {
            ilwert = true;
            fbpL2SliceParamName = "fbp_l2slice_enable";
        }
        bool fbpL2SliceParamPresent = false;
        UINT32 numFbpL2SliceArgs = argReader.ParamPresent(fbpL2SliceParamName.c_str());
        for (UINT32 i = 0; i < numFbpL2SliceArgs; i++)
        {
            UINT32 fbpNum      = argReader.ParamNUnsigned(fbpL2SliceParamName.c_str(), i, 0);
            UINT32 l2SliceMask = argReader.ParamNUnsigned(fbpL2SliceParamName.c_str(), i, 1);
            l2SliceMask        = ilwert ? ~l2SliceMask : l2SliceMask;

            if (fbpNum >= m_FbpL2SliceMasks.size())
            {
                Printf(Tee::PriError, "GA10xGpuSubdevice::ReadFloorsweeping"
                                      "Arguments: FBP # too large: %d\n", fbpNum);
                return RC::BAD_COMMAND_LINE_ARGUMENT;
            }

            m_FbpL2SliceMasks[fbpNum]          = l2SliceMask;
            m_FsFbpL2SliceParamPresent[fbpNum] = true;
            fbpL2SliceParamPresent             = true;
        }

        SetFloorsweepingAffected(GetFloorsweepingAffected() ||
                                 gpcRopParamPresent ||
                                 fbpL2SliceParamPresent);
    }
    return rc;
}

//------------------------------------------------------------------------------
void GA10xFs::PrintFloorsweepingParams()
{
    AmpereFs::PrintFloorsweepingParams();
    Tee::Priority pri = GetFloorsweepingPrintPriority();

    Printf(pri, "GA10xGpuSubdevice: Floorsweeping parameters "
                "present on commandline: ");
    for (UINT32 i = 0; i < m_FsGpcRopParamPresent.size(); i++)
    {
        if (m_FsGpcRopParamPresent[i])
        {
            Printf(pri, "gpc_rop[%u] ", i);
        }
    }
    for (UINT32 i = 0; i < m_FsFbpL2SliceParamPresent.size(); i++)
    {
        if (m_FsFbpL2SliceParamPresent[i])
        {
            Printf(pri, "fbp_l2slice[%u] ", i);
        }
    }

    Printf(pri, "\nGA10xGpuSubdevice: Floorsweeping parameters "
                "mask values: ");
    for (UINT32 i = 0; i < m_GpcRopMasks.size(); i++)
    {
        Printf(pri, "gpc_rop[%u]=0x%x ", i, m_GpcRopMasks[i]);
    }
    Printf(pri, "\n");
    if (m_pSub->Regs().IsSupported(MODS_FUSE_CTRL_OPT_L2SLICE_FBP, 0))
    {
        for (UINT32 i = 0; i < m_FbpL2SliceMasks.size(); i++)
        {
            Printf(pri, "fbp_l2slice[%u]=0x%x ", i, m_FbpL2SliceMasks[i]);
        }
        Printf(pri, "\n");
    }
}

//------------------------------------------------------------------------------
void GA10xFs::ApplyFloorsweepingSettings()
{
    AmpereFs::ApplyFloorsweepingSettings();

    for (UINT32 i = 0; i < m_GpcRopMasks.size(); i++)
    {
        if (m_FsGpcRopParamPresent[i])
        {
            m_pSub->Regs().Write32Sync(MODS_FUSE_CTRL_OPT_ROP_GPC_DATA, i, m_GpcRopMasks[i]);
        }
    }
    for (UINT32 i = 0; i < m_FbpL2SliceMasks.size(); i++)
    {
        if (m_FsFbpL2SliceParamPresent[i])
        {
            m_pSub->Regs().Write32Sync(MODS_FUSE_CTRL_OPT_L2SLICE_FBP_DATA, i,
                                       m_FbpL2SliceMasks[i]);
        }
    }
}

//------------------------------------------------------------------------------
void GA10xFs::RestoreFloorsweepingSettings()
{
    auto &regs = m_pSub->Regs();

    for (UINT32 i = 0; i < m_FsGpcRopParamPresent.size(); i++)
    {
        if (m_FsGpcRopParamPresent[i])
        {
            regs.Write32Sync(MODS_FUSE_CTRL_OPT_ROP_GPC, i, m_SavedGpcRopMasks[i]);
        }
    }

    for (UINT32 i = 0; i < m_FsFbpL2SliceParamPresent.size(); i++)
    {
        if (m_FsFbpL2SliceParamPresent[i])
        {
            regs.Write32Sync(MODS_FUSE_CTRL_OPT_L2SLICE_FBP, i, m_SavedFbpL2SliceMasks[i]);
        }
    }

    return AmpereFs::RestoreFloorsweepingSettings();
}

//------------------------------------------------------------------------------
void GA10xFs::ResetFloorsweepingSettings()
{
    auto &regs = m_pSub->Regs();

    UINT32 maxGpcs      = m_pSub->GetMaxGpcCount();
    UINT32 gpcRop       = regs.Read32(MODS_FUSE_OPT_ROP_DISABLE);
    UINT32 numRopPerGpc = GetMaxRopCount();
    UINT32 ropMask      = (1 << numRopPerGpc) - 1;
    for (UINT32 gpcIndex = 0; gpcIndex < maxGpcs; gpcIndex++)
    {
        UINT32 rop = (gpcRop & ropMask) >> (gpcIndex * numRopPerGpc);
        regs.Write32Sync(MODS_FUSE_CTRL_OPT_ROP_GPC, gpcIndex, rop);
        ropMask <<= numRopPerGpc;
    }

    if (regs.IsSupported(MODS_FUSE_OPT_L2SLICE_FBPx_DISABLE, 0))
    {
        UINT32 maxFbps = m_pSub->GetMaxFbpCount();
        if (regs.IsSupported(MODS_FUSE_OPT_L2SLICE_FBPx_DISABLE, maxFbps-1))
        {
            for (UINT32 fbpIndex = 0; fbpIndex < maxFbps; fbpIndex++)
            {
                UINT32 l2Slice = regs.Read32(MODS_FUSE_OPT_L2SLICE_FBPx_DISABLE, fbpIndex);
                regs.Write32Sync(MODS_FUSE_CTRL_OPT_L2SLICE_FBP, fbpIndex, l2Slice);
            }
        }
        else
        {
            MASSERT(!"L2SLICE_FBPx_DISABLE registers not defined for all FBPs!\n");
        }
    }

    AmpereFs::ResetFloorsweepingSettings();
}

//------------------------------------------------------------------------------
void GA10xFs::SaveFloorsweepingSettings()
{
    auto &regs = m_pSub->Regs();

    for (UINT32 i = 0; i < m_SavedGpcRopMasks.size(); i++)
    {
        m_SavedGpcRopMasks[i] = regs.Read32(MODS_FUSE_CTRL_OPT_ROP_GPC, i);
    }

    if (regs.IsSupported(MODS_FUSE_CTRL_OPT_L2SLICE_FBP, 0))
    {
        for (UINT32 i = 0; i < m_SavedFbpL2SliceMasks.size(); i++)
        {
            m_SavedFbpL2SliceMasks[i] = regs.Read32(MODS_FUSE_CTRL_OPT_L2SLICE_FBP, i);
        }
    }

    return AmpereFs::SaveFloorsweepingSettings();
}

//------------------------------------------------------------------------------
RC GA10xFs::PrintMiscFuseRegs()
{
    RC rc;
    CHECK_RC(AmpereFs::PrintMiscFuseRegs());

    auto &regs = m_pSub->Regs();

    Printf(Tee::PriNormal, " OPT_ROP_DIS : 0x%x\n",
                             regs.Read32(MODS_FUSE_OPT_ROP_DISABLE));

    UINT32 maxFbps = m_pSub->GetMaxFbpCount();
    for (UINT32 fbpIndex = 0; fbpIndex < maxFbps; fbpIndex++)
    {
        if (regs.IsSupported(MODS_FUSE_OPT_L2SLICE_FBPx_DISABLE, fbpIndex))
        {
            Printf(Tee::PriNormal, " OPT_L2SLICE_FBP%u_DIS : 0x%x\n",
                                     fbpIndex,
                                     regs.Read32(MODS_FUSE_OPT_L2SLICE_FBPx_DISABLE, fbpIndex));
        }
    }

    return rc;
}

//------------------------------------------------------------------------------
RC GA10xFs::GetFloorsweepingMasks
(
    FloorsweepingMasks* pFsMasks
) const
{
    if (Platform::IsVirtFunMode())
    {
        return RC::UNSUPPORTED_FUNCTION;
    }

    MASSERT(pFsMasks != 0);
    RC rc;

    UINT32 gpcMask = GpcMask();
    UINT32 gpcNum = 0;
    while (gpcMask != 0)
    {
        if (gpcMask & 1)
        {
            string fsmaskName = Utility::StrPrintf("gpcRop[%d]_mask", gpcNum);
            pFsMasks->insert(pair<string, UINT32>(fsmaskName, RopMask(gpcNum)));
        }
        gpcNum++;
        gpcMask >>= 1;
    }

    UINT32 fbpMask = FbpMask();
    UINT32 fbpNum = 0;
    while (fbpMask != 0)
    {
        if (fbpMask & 1)
        {
            string fsmaskName = Utility::StrPrintf("fbpL2Slice[%d]_mask", fbpNum);
            pFsMasks->insert(pair<string, UINT32>(fsmaskName, L2SliceMask(fbpNum)));
        }
        fbpNum++;
        fbpMask >>= 1;
    }

    CHECK_RC(AmpereFs::GetFloorsweepingMasks(pFsMasks));
    return rc;
}

//------------------------------------------------------------------------------
bool GA10xFs::SupportsRopMask(UINT32 hwGpcNum) const
{
    return m_pSub->Regs().IsSupported(MODS_FUSE_STATUS_OPT_ROP_GPC, hwGpcNum);
}

//------------------------------------------------------------------------------
UINT32 GA10xFs::RopMask(UINT32 hwGpcNum) const
{
    UINT32 hwRopMask    = 0;
    UINT32 rops         = m_pSub->Regs().Read32(MODS_FUSE_STATUS_OPT_ROP_GPC, hwGpcNum);

    hwRopMask  = ((1 << GetMaxRopCount()) - 1);
    hwRopMask &= ~rops;

    return hwRopMask;
}

//------------------------------------------------------------------------------
UINT32 GA10xFs::GetMaxRopCount() const
{
    return m_pSub->Regs().Read32(MODS_PTOP_SCAL_NUM_ROP_PER_GPC);
}

//------------------------------------------------------------------------------
UINT32 GA10xFs::GetMaxSlicePerLtcCount() const
{
    // Note:
    // LW_PTOP_SCAL_NUM_SLICES_PER_LTC gives the number of slices active per LTC
    // instead of max slices per LTC (see http://lwbugs/2606117)
    return m_pSub->Regs().LookupAddress(MODS_SCAL_LITTER_NUM_SLICES_PER_LTC);
}

//------------------------------------------------------------------------------
bool GA10xFs::SupportsL2SliceMask(UINT32 hwFbpNum) const
{
    const RegHal& regs = m_pSub->Regs();
    return regs.IsSupported(MODS_FUSE_STATUS_OPT_L2SLICE_FBP, hwFbpNum);
}

//------------------------------------------------------------------------------
UINT32 GA10xFs::L2SliceMask(UINT32 hwFbpNum) const
{
    RegHal &regs = m_pSub->Regs();

    if (!SupportsL2SliceMask(hwFbpNum))
    {
        return 0;
    }

    UINT32 numL2SlicesPerFbp = regs.Read32(MODS_PTOP_SCAL_NUM_LTC_PER_FBP) *
                               GetMaxSlicePerLtcCount();
    UINT32 l2SliceMask = ~regs.Read32(MODS_FUSE_STATUS_OPT_L2SLICE_FBP, hwFbpNum) &
                         ((1 << numL2SlicesPerFbp) - 1);
    return l2SliceMask;
}

//------------------------------------------------------------------------------
/* virtual */ UINT32 GA10xFs::PceMask() const
{
    return m_pSub->Regs().Read32(MODS_CE_PCE_MAP_VALUE);
}

//------------------------------------------------------------------------------
RC GA10xFs::AdjustL2NocFloorsweepingArguments()
{
    return RC::UNSUPPORTED_FUNCTION;
}

//------------------------------------------------------------------------------
#ifdef INCLUDE_FSLIB
void GA10xFs::GetFsLibMask(FsLib::GpcMasks *pGpcMasks) const
{
    MASSERT(pGpcMasks);
    AmpereFs::GetFsLibMask(pGpcMasks);

    const UINT32 maxGpcs = m_pSub->GetMaxGpcCount();
    const UINT32 maxRops = GetMaxRopCount();
    for (UINT32 gpcNum = 0; gpcNum < maxGpcs; gpcNum++)
    {
        bool isGpcDisabled    = (pGpcMasks->gpcMask & (1 << gpcNum)) != 0;
        UINT32 defaultRopMask = isGpcDisabled ? ~0U : ~RopMask(gpcNum);

        pGpcMasks->ropPerGpcMasks[gpcNum] = (m_FsGpcRopParamPresent[gpcNum] ?
                                             m_GpcRopMasks[gpcNum] :
                                             defaultRopMask) &
                                            ((1 << maxRops) - 1);
    }
}

void GA10xFs::GetFsLibMask(FsLib::FbpMasks *pFbpMasks) const
{
    MASSERT(pFbpMasks);
    AmpereFs::GetFsLibMask(pFbpMasks);

    RegHal &regs = m_pSub->Regs();

    if (regs.IsSupported(MODS_FUSE_CTRL_OPT_L2SLICE_FBP, 0))
    {
        const UINT32 maxFbps          = m_pSub->GetMaxFbpCount();
        const UINT32 maxL2SlicePerFbp = regs.Read32(MODS_PTOP_SCAL_NUM_LTC_PER_FBP) *
                                        GetMaxSlicePerLtcCount();

        pFbpMasks->fbpMask = (m_FsFbParamPresent ? m_FbMask : ~FbMask()) &
                             ((1 << maxFbps) - 1);
        for (UINT32 fbNum = 0; fbNum < maxFbps; fbNum++)
        {
            bool isFbpDisabled        = (pFbpMasks->fbpMask & (1 << fbNum)) != 0;
            UINT32 defaultL2SliceMask = isFbpDisabled ? ~0U : ~L2SliceMask(fbNum);

            pFbpMasks->l2SlicePerFbpMasks[fbNum] = (m_FsFbpL2SliceParamPresent[fbNum] ?
                                                    m_FbpL2SliceMasks[fbNum] :
                                                    defaultL2SliceMask) &
                                                    ((1 << maxL2SlicePerFbp) - 1);
        }
    }
}
#endif

//------------------------------------------------------------------------------
RC GA10xFs::AdjustFbpFloorsweepingArguments()
{
    RC rc;
    auto &regs = m_pSub->Regs();

    CHECK_RC(AmpereFs::AdjustFbpFloorsweepingArguments());

    const UINT32 maxFbps = m_pSub->GetMaxFbpCount();
    if (maxFbps != 0)
    {
        if (regs.IsSupported(MODS_FUSE_CTRL_OPT_L2SLICE_FBP, 0))
        {
            const UINT32 numLtcPerFbp      = regs.Read32(MODS_PTOP_SCAL_NUM_LTC_PER_FBP);
            const UINT32 numL2SlicesPerLtc = GetMaxSlicePerLtcCount();
            const UINT32 l2SliceMask       = (1U << numL2SlicesPerLtc) - 1;

            for (UINT32 fbNum = 0; fbNum < maxFbps; fbNum++)
            {
                if (!m_FsFbpL2SliceParamPresent[fbNum] &&
                     m_FsFbpL2ParamPresent[fbNum])
                {
                    bool l2SliceMaskModified = false;
                    for (UINT32 ltc = 0; ltc < numLtcPerFbp; ltc++)
                    {
                        // Disable L2 slices if coresponding LTC is floorswept
                        if ((m_FbpL2Masks[fbNum] >> ltc) & 1)
                        {
                            m_FbpL2SliceMasks[fbNum] |= (l2SliceMask << (ltc * numL2SlicesPerLtc));
                            l2SliceMaskModified       = true;
                        }
                    }
                    if (l2SliceMaskModified)
                    {
                        m_FsFbpL2SliceParamPresent[fbNum] = true;
                        Printf(Tee::PriWarn, "Adjusting L2 slice[%d] to 0x%x\n",
                                              fbNum, m_FbpL2SliceMasks[fbNum]);
                    }
                }
            }
        }
    }

    return rc;
}

//------------------------------------------------------------------------------
RC GA10xFs::AdjustPostGpcFloorsweepingArguments()
{
    RC rc;

    CHECK_RC(AmpereFs::AdjustPostGpcFloorsweepingArguments());

    const UINT32 maxGpcs    = m_pSub->GetMaxGpcCount();
    const UINT32 maxRops    = GetMaxRopCount();
    const UINT32 ropGpcMask = ((1 << maxRops) - 1);
    if (m_FsGpcParamPresent)
    {
        for (UINT32 gpcNum = 0; gpcNum < maxGpcs; gpcNum++)
        {
            // If GPC is disabled, disable all underlying ROPs
            if (((m_GpcMask >> gpcNum) & 1) && !m_FsGpcRopParamPresent[gpcNum])
            {
                m_GpcRopMasks[gpcNum] = ropGpcMask;
                m_FsGpcRopParamPresent[gpcNum] = true;
                Printf(Tee::PriWarn, "Adjusting ROP mask[%d] to 0x%x\n",
                                      gpcNum, m_GpcRopMasks[gpcNum]);
            }
        }
    }

    return rc;
}


//------------------------------------------------------------------------------
RC GA10xFs::FixFloorsweepingInitMasks(bool *pFsAffected)
{
    bool bIsSltFused = false;
    if (m_pSub->IsSltFused(&bIsSltFused) != RC::OK)
    {
        Printf(Tee::PriWarn, "fix_fs_init_masks is not supported!\n");
        return RC::OK;
    }
    if (bIsSltFused)
    {
        Printf(Tee::PriError, "fix_fs_init_masks can be used only before slt floorsweeping!\n");
        return RC::BAD_COMMAND_LINE_ARGUMENT;
    }
    return FixFbFloorsweepingInitMasks(pFsAffected);
}

//------------------------------------------------------------------------------
RC GA10xFs::FixFbFloorsweepingInitMasks(bool *pFsAffected)
{
    RC rc;

    auto &regs = m_pSub->Regs();
    const UINT32 maxFbps = m_pSub->GetMaxFbpCount();
    if (maxFbps != 0)
    {
        // Check that none of the FBP dependent fs params (which are independent of fbNum)
        // are specified with -floorsweep arg
        if (m_FsFbParamPresent || m_FsFbioParamPresent)
        {
            Printf(Tee::PriError, "FB related floorsweep params should not be used with "
                                  "fix_fs_init_masks!\n");
            return RC::BAD_COMMAND_LINE_ARGUMENT;
        }

        const UINT32 fbp = regs.Read32(MODS_FUSE_OPT_FBP_DISABLE);
        bool fbMaskModified = false;
        bool fbioMaskModified = false;
        UINT32 l2MaskModified = 0;
        UINT32 l2SliceMaskModified = 0;

        // Initializing here as we will be disabling these FBPs in any case
        m_FbMask = fbp;

        for (UINT32 fbNum = 0; fbNum < maxFbps; fbNum++)
        {
            // Check that remaining FBP dependent args (those dependent of fbNum) are also
            // absent
            if (m_FsFbpL2ParamPresent[fbNum] || m_FsFbpL2SliceParamPresent[fbNum])
            {
                Printf(Tee::PriError, "FB related floorsweep params should not be used with "
                            "fix_fs_post_ate!\n");
                return RC::BAD_COMMAND_LINE_ARGUMENT;
            }

            // Based on L2 slices, fix LTCs and vice versa
            CHECK_RC(FixFbpLtcFloorsweepingMasks(fbNum,
                                                 &l2MaskModified, &l2SliceMaskModified,
                                                 pFsAffected));

            // Fix the rest of the FBP args
            CHECK_RC(FixFbpFloorsweepingMasks(fbNum,
                                              &fbMaskModified, &fbioMaskModified,
                                              &l2MaskModified, &l2SliceMaskModified,
                                              pFsAffected));

        }

        CHECK_RC(FixHalfFbpaFloorsweepingMasks(pFsAffected));

        // Print modified masks
        if (fbMaskModified)
        {
            m_FsFbParamPresent = true;
            Printf(Tee::PriWarn, "Fixing FB mask to 0x%x\n", m_FbMask);
        }
        if (fbioMaskModified)
        {
            m_FsFbioParamPresent = true;
            Printf(Tee::PriWarn, "Fixing FBIO mask to 0x%x\n", m_FbioMask);
        }
        for (UINT32 fbNum = 0; fbNum < maxFbps; fbNum++)
        {
            if ((l2MaskModified >> fbNum) & 1)
            {
                m_FsFbpL2ParamPresent[fbNum] = true;
                Printf(Tee::PriWarn, "Fixing L2 Mask [%d] to 0x%x\n",
                                      fbNum, m_FbpL2Masks[fbNum]);
            }
            if ((l2SliceMaskModified >> fbNum) & 1)
            {
                m_FsFbpL2SliceParamPresent[fbNum] = true;
                Printf(Tee::PriWarn, "Fixing L2 slice[%d] to 0x%x\n",
                                      fbNum, m_FbpL2SliceMasks[fbNum]);
            }
        }
    }

    return rc;
}

//------------------------------------------------------------------------------
RC GA10xFs::FixFbpFloorsweepingMasks
(
    UINT32 fbNum,
    bool *pFbMaskModified,
    bool *pFbioMaskModified,
    UINT32 *pL2MaskModified,
    UINT32 *pL2SliceMaskModified,
    bool *pFsAffected
)
{
    MASSERT(pFbMaskModified);
    MASSERT(pFbioMaskModified);
    MASSERT(pL2MaskModified);
    MASSERT(pL2SliceMaskModified);
    MASSERT(pFsAffected);

    auto &regs = m_pSub->Regs();

    const UINT32 maxFbps = m_pSub->GetMaxFbpCount();

    // FBIO
    const UINT32 maxFbios       = regs.Read32(MODS_PTOP_SCAL_NUM_FBPAS);
    const UINT32 fbio           = regs.Read32(MODS_FUSE_OPT_FBIO_DISABLE);
    const UINT32 fbioStride     = maxFbios / maxFbps;
    const UINT32 fbioFbpMask    = (1U << fbioStride) - 1;

    // LTC/L2SLICE params
    const UINT32 numLtcPerFbp = regs.Read32(MODS_PTOP_SCAL_NUM_LTC_PER_FBP);
    const UINT32 ltcMask      = (1U << numLtcPerFbp) - 1;
    UINT32 numL2SlicesPerLtc  = 0;
    UINT32 l2SliceMask        = 0;
    if (regs.IsSupported(MODS_FUSE_CTRL_OPT_L2SLICE_FBP, fbNum))
    {
        numL2SlicesPerLtc = GetMaxSlicePerLtcCount();
        l2SliceMask       = (1U << numL2SlicesPerLtc) - 1;
    }

    // If all LTC or all FBIOs (inclusive) are disabled in a FB then that
    // FB needs to be disabled
    const bool isLtcDisabled  = m_FbpL2Masks[fbNum] == ltcMask;
    const bool isFbioDisabled = ((fbio >> (fbioStride * fbNum)) & fbioFbpMask)
                                  == fbioFbpMask;
    if (isLtcDisabled || isFbioDisabled)
    {
        // Update FBIO mask
        if (isFbioDisabled)
        {
            m_FbioMask |= fbioFbpMask << (fbioStride * fbNum);
            *pFbioMaskModified = true;
        }

        m_FbMask |= (1 << fbNum);
        *pFbMaskModified = true;
        *pFsAffected = true;
    }

    // Now fix all units under FBP
    if (m_FbMask & (1U << fbNum))
    {
        *pFbMaskModified = true;
        *pFsAffected = true;

        // If the FBP is disabled, so should the corresponding FBIO be
        m_FbioMask |= fbioFbpMask << (fbioStride * fbNum);
        *pFbioMaskModified = true;

        // Disable LTC as well
        m_FbpL2Masks[fbNum] = ltcMask;
        *pL2MaskModified |= (1U << fbNum);
        // Disable L2SLICES for this FBP
        if (regs.IsSupported(MODS_FUSE_CTRL_OPT_L2SLICE_FBP, 0))
        {
            for (UINT32 ltcIdx = 0; ltcIdx < numLtcPerFbp; ltcIdx++)
            {
                // Disable L2SLICES if coresponding LTC is floorswept
                if (m_FbpL2Masks[fbNum] & (1U << ltcIdx))
                {
                    m_FbpL2SliceMasks[fbNum] |=
                        (l2SliceMask << (ltcIdx * numL2SlicesPerLtc));
                    *pL2SliceMaskModified |= (1U << fbNum);
                }
            }
        }
    }

    return RC::OK;
}

//------------------------------------------------------------------------------
RC GA10xFs::FixFbpLtcFloorsweepingMasks
(
    UINT32 fbNum,
    UINT32 *pL2MaskModified,
    UINT32 *pL2SliceMaskModified,
    bool *pFsAffected
)
{
    MASSERT(pL2MaskModified);
    MASSERT(pL2SliceMaskModified);
    MASSERT(pFsAffected);

    auto &regs = m_pSub->Regs();

    // LTC/L2SLICE params
    UINT32 numLtcPerFbp       = regs.Read32(MODS_PTOP_SCAL_NUM_LTC_PER_FBP);
    const UINT32 ltc          = regs.Read32(MODS_FUSE_OPT_LTC_DISABLE);
    UINT32 numL2SlicesPerLtc  = 0;
    UINT32 l2SliceMask        = 0;
    if (regs.IsSupported(MODS_FUSE_CTRL_OPT_L2SLICE_FBP, fbNum))
    {
        numL2SlicesPerLtc = GetMaxSlicePerLtcCount();
        l2SliceMask       = (1U << numL2SlicesPerLtc) - 1;
    }

    // Based on L2 slices, fix LTCs and vice versa
    if (regs.IsSupported(MODS_FUSE_CTRL_OPT_L2SLICE_FBP, fbNum))
    {
        const UINT32 l2SliceFbpx = regs.Read32(MODS_FUSE_OPT_L2SLICE_FBPx_DISABLE,
                                         fbNum);
        for (UINT32 ltcIdx = 0; ltcIdx < numLtcPerFbp; ltcIdx++)
        {
            // Disable LTC if one or more L2 Slices are floorswept
            // Also disable the remaining L2 Slices for floorswept LTC
            // More complex cases of L2 Slice floorsweeping are being ignored here
            // by simply disabling affected LTC
            if (((l2SliceFbpx & (l2SliceMask << (ltcIdx * numL2SlicesPerLtc))) != 0) ||
                  (ltc & (1U << ((numLtcPerFbp * fbNum) + ltcIdx))))
            {
                m_FbpL2Masks[fbNum]      |= (1U << ltcIdx);
                *pL2MaskModified         |= (1U << fbNum);
                m_FbpL2SliceMasks[fbNum] |=
                    (l2SliceMask << (ltcIdx * numL2SlicesPerLtc));
                *pL2SliceMaskModified    |= (1U << fbNum);

                *pFsAffected              = true;
            }
        }
    }

    return RC::OK;
}

//------------------------------------------------------------------------------
RC GA10xFs::FixHalfFbpaFloorsweepingMasks
(
    bool *pFsAffected
)
{
    MASSERT(pFsAffected);

    auto &regs = m_pSub->Regs();
    const UINT32 maxFbps = m_pSub->GetMaxFbpCount();
    if (maxFbps != 0)
    {
        const UINT32 halfFbpaStride = GetHalfFbpaWidth();
        const UINT32 halfFbpaMask   = (1U << halfFbpaStride) - 1;
        const UINT32 numLtcPerFbp   = regs.Read32(MODS_PTOP_SCAL_NUM_LTC_PER_FBP);
        const UINT32 ltcMask        = (1U << numLtcPerFbp) - 1;

        // Check that none of the FBP dependent fs params are specified with -floorsweep
        if (m_FsHalfFbpaParamPresent)
        {
            Printf(Tee::PriError, "FB related floorsweep params should not be used with "
                    "fix_fs_init_masks!\n");
            return RC::BAD_COMMAND_LINE_ARGUMENT;
        }

        bool halfFbpaModified = false;
        for (UINT32 fbNum = 0; fbNum < maxFbps; fbNum++)
        {
            if (m_FsFbpL2ParamPresent[fbNum] &&
                m_FbpL2Masks[fbNum] != 0 &&
                (m_FbpL2Masks[fbNum] & ltcMask) != ltcMask)
            {
                m_HalfFbpaMask |= halfFbpaMask << (halfFbpaStride * fbNum);
                halfFbpaModified = true;
                *pFsAffected = true;
            }
        }
        if (halfFbpaModified)
        {
            m_FsHalfFbpaParamPresent = true;
            Printf(Tee::PriWarn, "Fixing half fbpa mask to 0x%x\n", m_HalfFbpaMask);
        }
    }

    return RC::OK;
}

//------------------------------------------------------------------------------
void GA10xFs::CheckIfPriRingInitIsRequired()
{
    for (UINT32 i = 0; i < m_FsGpcRopParamPresent.size(); i++)
    {
        m_IsPriRingInitRequired = m_IsPriRingInitRequired || m_FsGpcRopParamPresent[i];
    }

    for (UINT32 i = 0; i < m_FsFbpL2SliceParamPresent.size(); i++)
    {
        m_IsPriRingInitRequired = m_IsPriRingInitRequired || m_FsFbpL2SliceParamPresent[i];
    }

    AmpereFs::CheckIfPriRingInitIsRequired();
}

//------------------------------------------------------------------------------
RC GA10xFs::PopulateFsInforom
(
    LW90E7_CTRL_RPR_WRITE_OBJECT_PARAMS *pIfrParams
) const
{
    RC rc;
    MASSERT(pIfrParams);

    RegHal &regs = m_pSub->Regs();

    UINT32 maxIfrDataCount;
    CHECK_RC(m_pSub->GetInfoRomRprMaxDataCount(&maxIfrDataCount));

    for (UINT32 gpcNum = 0; gpcNum < m_GpcRopMasks.size(); gpcNum++)
    {
        if (!m_FsGpcRopParamPresent[gpcNum])
            continue;

        CHECK_RC(PopulateFsInforomEntry(pIfrParams,
                                        regs.LookupAddress(MODS_FUSE_CTRL_OPT_ROP_GPC, gpcNum),
                                        m_GpcRopMasks[gpcNum],
                                        maxIfrDataCount));
    }

    for (UINT32 fbpNum = 0; fbpNum < m_FbpL2SliceMasks.size(); fbpNum++)
    {
        if (!m_FsFbpL2SliceParamPresent[fbpNum])
            continue;

        CHECK_RC(PopulateFsInforomEntry(pIfrParams,
                                        regs.LookupAddress(MODS_FUSE_CTRL_OPT_L2SLICE_FBP, fbpNum),
                                        m_FbpL2SliceMasks[fbpNum],
                                        maxIfrDataCount));
    }

    CHECK_RC(AmpereFs::PopulateFsInforom(pIfrParams));

    return rc;
}

RC GA10xFs::AddLwrFsMask()
{
    auto& regs = m_pSub->Regs();
    if (IsFloorsweepingAllowed())
    {
        // Add GPC ROP mask
        UINT32 gpcNum = 0;
        for (gpcNum = 0; gpcNum < m_GpcRopMasks.size(); gpcNum++)
        {
            if (!m_FsGpcRopParamPresent[gpcNum])
                continue;
            UINT32 lwrGpcRopDisMask = regs.Read32(MODS_FUSE_STATUS_OPT_ROP_GPC, gpcNum);
            // If one bit in Current disable mask is 1, it means that this unit is originally disabled
            // If this bit is 0 in the user's disable mask (user wants to enable it), we need to revise it
            if (lwrGpcRopDisMask & ~m_GpcRopMasks[gpcNum])
            {
                m_GpcRopMasks[gpcNum] |= lwrGpcRopDisMask;
                Printf(Tee::PriWarn, "Adding the GPC ROP current Mask, adjusting GPC[%d] ROP disable mask to 0x%x\n", gpcNum, m_GpcRopMasks[gpcNum]);
            }
        }

        // Add FBP L2 mask
        if (regs.IsSupported(MODS_FUSE_STATUS_OPT_LTC_FBP))
        {
            UINT32 fbpNum = 0;
            for (fbpNum = 0; fbpNum < m_FbpL2Masks.size(); fbpNum++)
            {
                if (!m_FsFbpL2ParamPresent[fbpNum])
                    continue;
                UINT32 lwrFbpL2DisMask = regs.Read32(MODS_FUSE_STATUS_OPT_LTC_FBP, fbpNum);
                if (lwrFbpL2DisMask & ~m_FbpL2Masks[fbpNum])
                {
                    m_FbpL2Masks[fbpNum] |= lwrFbpL2DisMask;
                    Printf(Tee::PriWarn, "Adding the FBP L2 current Mask, adjusting FBP[%d] L2 disable mask to 0x%x\n", fbpNum, m_FbpL2Masks[fbpNum]);
                }
            }
        }

        // Add FBP L2Slice mask
        if (regs.IsSupported(MODS_FUSE_STATUS_OPT_L2SLICE_FBP))
        {
            UINT32 fbpNum = 0;
            for (fbpNum = 0; fbpNum < m_FbpL2SliceMasks.size(); fbpNum++)
            {
                if (!m_FsFbpL2SliceParamPresent[fbpNum])
                    continue;
                UINT32 lwrFbpL2SliceDisMask = regs.Read32(MODS_FUSE_STATUS_OPT_L2SLICE_FBP, fbpNum);
                if (lwrFbpL2SliceDisMask & ~m_FbpL2SliceMasks[fbpNum])
                {
                    m_FbpL2SliceMasks[fbpNum] |= lwrFbpL2SliceDisMask;
                    Printf(Tee::PriWarn, "Adding the FBP L2 Slice current Mask, adjusting FBP[%d] L2 Slice disable mask to 0x%x\n", fbpNum, m_FbpL2SliceMasks[fbpNum]);
                }
            }
        }
    }
    return AmpereFs::AddLwrFsMask();
}

/* virtual */ void GA10xFs::PopulateGpcDisableMasks(JsonItem *pJsi) const
{
    MASSERT(pJsi);
    AmpereFs::PopulateGpcDisableMasks(pJsi);

    RegHal &regs = m_pSub->Regs();
    const UINT32 maxGpcs = m_pSub->GetMaxGpcCount();
    const UINT32 maxRops = GetMaxRopCount();
    string robDsblMask;

    for (UINT32 gpcNum = 0; gpcNum < maxGpcs; gpcNum++)
    {
        robDsblMask += Utility::StrPrintf("%s0x%x'",
                        (gpcNum == 0 ? "'" : ", '"),
                        (regs.Read32(MODS_FUSE_STATUS_OPT_ROP_GPC, gpcNum) & ((1 << maxRops) - 1))
                    );
    }

    pJsi->SetField("OPT_ROP_DISABLE", ("[" +  robDsblMask + "]").c_str());
}

/* virtual */ void GA10xFs::PopulateFbpDisableMasks(JsonItem *pJsi) const
{
    MASSERT(pJsi);
    AmpereFs::PopulateFbpDisableMasks(pJsi);

    RegHal &regs = m_pSub->Regs();
    const UINT32 maxFbps = m_pSub->GetMaxFbpCount();
    const UINT32 maxL2SlicePerFbp = regs.Read32(MODS_PTOP_SCAL_NUM_LTC_PER_FBP) *
                                        GetMaxSlicePerLtcCount();
    string l2SliceDsblMask;

    for (UINT32 fbNum = 0; fbNum < maxFbps; fbNum++)
    {
        l2SliceDsblMask += Utility::StrPrintf("%s0x%x'",
                            (fbNum == 0 ? "'" : ", '"),
                            (regs.Read32(MODS_FUSE_STATUS_OPT_L2SLICE_FBP, fbNum) & ((1 << maxL2SlicePerFbp) - 1))
                        );
    }

    pJsi->SetField("OPT_L2SLICE_DISABLE", ("[" +  l2SliceDsblMask + "]").c_str());
}

//------------------------------------------------------------------------------
CREATE_GPU_FS_FUNCTION(GA10xFs);
