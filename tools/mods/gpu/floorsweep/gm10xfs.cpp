/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2014-2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#include "gm10xfs.h"
#include "core/include/platform.h"
#include "core/include/utility.h"
#include "core/include/argpproc.h"
#include "core/include/argread.h"
#include "core/include/argdb.h"
#include "mods_reg_hal.h"
#include "cheetah/include/tegra_gpu_file.h"
#include "maxwell/gm108/dev_fuse.h"
#include "maxwell/gm108/dev_trim.h"
#include "maxwell/gm108/dev_graphics_nobundle.h"
#include "maxwell/gm108/hwproject.h"
#include "maxwell/gm108/dev_ext_devices.h"
// Maxwell needs to support the FscontrolMask() interface. This interface expects
// the fs bits to be arranged into a single 32bit value, however maxwell doesn't
// actually have these bit definitions (kepler does) so include dev_top.h to get
// them.
#include "kepler/gk104/dev_top.h"
// For the WAR for Bug 1684053, we need to know how many FBPAs there are per FBP
// However, the register with that info wasn't introduced until Maxwell-2, so we
// need dev_top from gm20x as well.
#include "maxwell/gm204/dev_top.h"

//------------------------------------------------------------------------------
// access the below parameters via:
// mods ... mdiag.js -fermi_fs <feature-string>
// a "feature-string" is <name>:<value>[:<value>...]
//
static const ParamDecl s_FloorSweepingParamsGM10xPlus[] =
{
    { "lwdec_enable",            "u", 0, 0, 0, "gm10x lwdec enable"},
    { "lwdec_disable",           "u", 0, 0, 0, "gm10x lwdec disable"},
    { "lwenc_enable",            "u", 0, 0, 0, "gm10x lwenc enable"},
    { "lwenc_disable",           "u", 0, 0, 0, "gm10x lwenc disable"},
    { "head_enable",             "u", 0, 0, 0,   "gk10x head enable"},
    { "head_disable",            "u", 0, 0, 0,   "gk10x head disable"},

    LAST_PARAM
};

//------------------------------------------------------------------------------
static ParamConstraints s_FsParamConstraintsGM10xPlus[] =
{
   MUTUAL_EXCLUSIVE_PARAM("lwdec_enable", "lwdec_disable"),
   MUTUAL_EXCLUSIVE_PARAM("lwenc_enable", "lwenc_disable"),
   MUTUAL_EXCLUSIVE_PARAM("head_enable", "head_disable"),
   LAST_CONSTRAINT
};

// GM107 yes | GM108 Yes | GM200 partial | GM204 partial | GM206 partial | GM204X partial | GM206X partial
//------------------------------------------------------------------------------
GM10xFs::GM10xFs(GpuSubdevice *pSubdev) :
    FermiFs(pSubdev)

{
    m_SavedGpcTpcMask.resize(LW_FUSE_CTRL_OPT_TPC_GPC__SIZE_1);
}

//------------------------------------------------------------------------------
// GM107 yes | GM108 Yes |
/* virtual */ RC GM10xFs::ReadFloorsweepingArgumentsImpl(ArgDatabase& fsArgDb)
{
    RC rc = OK;

    // parse gk10x+ floorsweeping arguments
    CHECK_RC(FermiFs::ReadFloorsweepingArgumentsGF108Plus(fsArgDb));

    string FsArgs = GetFermiFsArgs();
    if (!FsArgs.empty())
    {
        ArgReader argReader(s_FloorSweepingParamsGM10xPlus, s_FsParamConstraintsGM10xPlus);
        if (!argReader.ParseArgs(&fsArgDb))
            return RC::BAD_COMMAND_LINE_ARGUMENT;

        m_FsLwencParamPresent = argReader.ParamPresent("lwenc_enable") ||
                                argReader.ParamPresent("lwenc_disable");

        m_FsLwdecParamPresent = argReader.ParamPresent("lwdec_enable") ||
                                argReader.ParamPresent("lwdec_disable");

        m_FsHeadParamPresent = argReader.ParamPresent("head_enable") ||
                               argReader.ParamPresent("head_disable");

        // now get the mask values.  If the enable param is specified, use its
        // bitwise negation as the mask value.  If the disable param is specified,
        // use it directly as the mask.   This code works because _enable and _disable
        // are mutually exclusive for a given feature.
        //
        m_LwencMask = ~ argReader.ParamUnsigned("lwenc_enable", ~0U);
        m_LwencMask = argReader.ParamUnsigned("lwenc_disable", m_LwencMask);

        m_LwdecMask = ~ argReader.ParamUnsigned("lwdec_enable", ~0U);
        m_LwdecMask = argReader.ParamUnsigned("lwdec_disable", m_LwdecMask);

        m_HeadMask = ~ argReader.ParamUnsigned("head_enable", 0);
        m_HeadMask = argReader.ParamUnsigned("head_disable", m_HeadMask);

        // FermiFs supports msvld_enable/disable but we don't
        // have that in Maxwell. We should be using lwdec_enable/disable instead.
        // So return an error if this parameter is used.
        if (m_FsMsvldParamPresent == true || m_FsMsdecParamPresent == true)
        {
            if (m_FsMsvldParamPresent == true)
            {
                Printf(Tee::PriHigh,"\"msvld_enable/msvld_disable\" is not supported on Maxwell.\n");
            }
            if (m_FsMsdecParamPresent == true)
            {
                Printf(Tee::PriHigh,"\"msdec_enable/msdec_disable\" is not supported on Maxwell.\n");
            }
            return RC::BAD_COMMAND_LINE_ARGUMENT;
        }
        SetFloorsweepingAffected(GetFloorsweepingAffected() ||
                                 m_FsLwencParamPresent ||
                                 m_FsLwdecParamPresent ||
                                 m_FsHeadParamPresent );
    }

    return rc;
}

//------------------------------------------------------------------------------
/* virtual */ void GM10xFs::PrintFloorsweepingParams()
{
    FermiFs::PrintFloorsweepingParams();
    Tee::Priority pri = GetFloorsweepingPrintPriority();

    Printf(pri, "GM10xGpuSubdevice: Floorsweeping parameters "
           "present on commandline: %s %s %s\n",
            (m_FsLwencParamPresent ? "lwenc" : ""),
            (m_FsLwdecParamPresent ? "lwdec" : ""),
            (m_FsHeadParamPresent ? "head" : ""));
    Printf(pri, "GM10xGpuSubdevice: Floorsweeping parameter "
           "mask values: lwenc=0x%x lwdec=0x%x head=0x%x\n",
            m_LwencMask, m_LwdecMask, m_HeadMask);
}

//------------------------------------------------------------------------------
// ApplyFloorsweepingSettings passed via the command line arguments.
// Note:
// Most of the internal variables are maintained in FermiKeplerGpuSubdevice
// however the register structure for some of these registers have been simplified
// The simplification requires utilization of new individual m_SavedFsxxxx vars.
// GM107 yes | GM108 Yes | GM200 yes | GM204 yes | GM206 yes | GM204X yes | GM206X yes
void GM10xFs::ApplyFloorsweepingSettings()
{
    // All floorsweeping registers should have a write with ack on non-hardware
    // platforms. see http://lwbugs/1043132
    auto& regs = m_pSub->Regs();

    if (m_FsDisplayParamPresent)
    {
        UINT32 regVal = m_SavedFsDispControl;
        regs.SetField(&regVal, MODS_FUSE_CTRL_OPT_DISPLAY_DATA, m_DisplayMask);
        regs.Write32Sync(MODS_FUSE_CTRL_OPT_DISPLAY, regVal);
    }

    if (m_FsLwdecParamPresent)
    {
        UINT32 regVal = m_SavedFsLwdecControl;
        regs.SetField(&regVal, MODS_FUSE_CTRL_OPT_LWDEC_DATA, m_LwdecMask);
        regs.Write32Sync(MODS_FUSE_CTRL_OPT_LWDEC, regVal);
    }

    if (m_FsFbioShiftOverrideParamPresent)
    {
        UINT32 regVal = m_SavedFsFbioShiftOverrideControl;
        regs.SetField(&regVal, MODS_FUSE_CTRL_OPT_FBIO_SHIFT_OVERRIDE_DATA,
                      m_FbioShiftOverrideMask);
        regs.Write32Sync(MODS_FUSE_CTRL_OPT_FBIO_SHIFT_OVERRIDE, regVal);
    }

    if (m_FsCeParamPresent && IsCeFloorsweepingSupported())
    {
        UINT32 regVal = m_SavedFsCeControl;
        regs.SetField(&regVal, MODS_FUSE_CTRL_OPT_CE_DATA, m_CeMask);
        regs.Write32Sync(MODS_FUSE_CTRL_OPT_CE, regVal);
    }

    if (m_FsLwencParamPresent)
    {
        UINT32 regVal = m_SavedFsLwencControl;
        regs.SetField(&regVal, MODS_FUSE_CTRL_OPT_LWENC_DATA, m_LwencMask);
        regs.Write32Sync(MODS_FUSE_CTRL_OPT_LWENC, regVal);
    }

    if (m_FsGpcParamPresent)
    {
        regs.Write32Sync(MODS_FUSE_CTRL_OPT_GPC_DATA, m_GpcMask);
    }

    if (m_FsFbParamPresent)
    {
        UINT32 inferredMask = 0;
        if (m_pSub->HasBug(1684053))
        {
            // Bug 1684053 - On GP100, FBPs can't be fused, so
            // we have to infer the correct values based on FBIO
            // If all FBIOs under an FBP are fused, we can assume
            // the FBP is meant to be disabled
            UINT32 fbio = regs.Read32(MODS_FUSE_OPT_FBIO_DISABLE);
            UINT32 numFbps = m_pSub->GetMaxFbpCount();
            UINT32 ratio = m_pSub->Regs().Read32(MODS_PTOP_SCAL_NUM_FBPA_PER_FBP);
            UINT32 submask = (1 << ratio) - 1;
            for (UINT32 i = 0; i < numFbps; i++)
            {
                if (((fbio >> i * ratio) & submask) == submask)
                {
                    inferredMask |= 1 << i;
                }
            }
        }
        regs.Write32Sync(MODS_FUSE_CTRL_OPT_FBP_DATA, m_FbMask | inferredMask);
    }

    if (m_FsFbioParamPresent)
        regs.Write32Sync(MODS_FUSE_CTRL_OPT_FBIO_DATA, m_FbioMask);

    if (m_FsFbioShiftParamPresent)
        regs.Write32Sync(MODS_FUSE_CTRL_OPT_FBIO_SHIFT_DATA, m_FbioShiftMask);

    // There is one 32bit TPC mask for each GPC. There are 32 TPC mask registers
    // Enough support for upto 32 GPCs, with each GPC supporting upto 32 TPCs.
    UINT32 gpcNum = 0;
    for (gpcNum = 0; gpcNum < m_GpcTpcMasks.size(); gpcNum++)
    {
        if (!m_FsGpcTpcParamPresent[gpcNum])
            continue;

        regs.Write32Sync(MODS_FUSE_CTRL_OPT_TPC_GPC_DATA, gpcNum, m_GpcTpcMasks[gpcNum]);
    }

    // There is one 32b ZLWLL mask register per GPC
    for (gpcNum = 0; gpcNum < m_GpcZlwllMasks.size(); gpcNum++)
    {
        // if user hasn't specified a zlwll mask for this gpc then skip it
        //
        if (!m_FsGpcZlwllParamPresent[gpcNum])
            continue;

        if (regs.IsSupported(MODS_FUSE_CTRL_OPT_ZLWLL_GPC))
        {
            regs.Write32Sync(MODS_FUSE_CTRL_OPT_ZLWLL_GPC_DATA, gpcNum, m_GpcZlwllMasks[gpcNum]);
        }
    }

    if (m_FsPcieLaneParamPresent)
    {
        regs.Write32Sync(MODS_FUSE_CTRL_OPT_PCIE_LANE_DATA, m_PcieLaneMask);
    }

    if (m_FsFbpaParamPresent)
    {
        regs.Write32Sync(MODS_FUSE_CTRL_OPT_FBPA_DATA, m_FbpaMask);
    }

    if (m_FsSpareParamPresent)
    {
        regs.Write32Sync(MODS_FUSE_CTRL_OPT_SPARE_FS_DATA, m_SpareMask);
    }

    if (m_FsHeadParamPresent)
    {
        regs.Write32Sync(MODS_FUSE_CTRL_OPT_DISP_HEAD_DATA, m_HeadMask);
    }
}

//------------------------------------------------------------------------------
// RestoreFloorsweepingSettings()
//
// restores floorsweeping registers that have been changed
//
// this function is only called if there definitely is at least one
// floorsweeping register that has changed
//
// GM107 yes | GM108 Yes | GM200 yes | GM204 yes | GM206 yes | GM204X yes | GM206X yes
void GM10xFs::RestoreFloorsweepingSettings()
{
    Printf(Tee::PriDebug, "GM10xGpuSubdevice::RestoreFloorsweepingSettings: "
           " checking and restoring startup settings\n");
    auto& regs = m_pSub->Regs();

    if (m_FsDisplayParamPresent)
    {
        regs.Write32Sync(MODS_FUSE_CTRL_OPT_DISPLAY, m_SavedFsDispControl);
    }

    if (m_FsLwencParamPresent)
    {
        regs.Write32Sync(MODS_FUSE_CTRL_OPT_LWENC, m_SavedFsLwencControl);
    }

    if (m_FsLwdecParamPresent)
    {
        regs.Write32Sync(MODS_FUSE_CTRL_OPT_LWDEC, m_SavedFsLwdecControl);
    }

    if (m_FsFbioShiftOverrideParamPresent)
    {
        regs.Write32Sync(MODS_FUSE_CTRL_OPT_FBIO_SHIFT_OVERRIDE,
                m_SavedFsFbioShiftOverrideControl);
    }

    if (m_FsCeParamPresent && IsCeFloorsweepingSupported())
    {
        regs.Write32Sync(MODS_FUSE_CTRL_OPT_CE, m_SavedFsCeControl);
    }

    if (m_FsGpcParamPresent)
        regs.Write32Sync(MODS_FUSE_CTRL_OPT_GPC, m_SavedGpcMask);

    if (m_FsFbParamPresent)
        regs.Write32Sync(MODS_FUSE_CTRL_OPT_FBP, m_SavedFbpMask);

    for (UINT32 i = 0; i < m_FsGpcTpcParamPresent.size(); ++i)
    {
        if (m_FsGpcTpcParamPresent[i])
        {
            regs.Write32Sync(MODS_FUSE_CTRL_OPT_TPC_GPC, i, m_SavedGpcTpcMask[i]);
        }
    }

    if (regs.IsSupported(MODS_FUSE_CTRL_OPT_ZLWLL_GPC))
    {
        for (UINT32 i = 0; i < m_FsGpcZlwllParamPresent.size(); ++i)
        {
            if (m_FsGpcZlwllParamPresent[i])
            {
                regs.Write32Sync(MODS_FUSE_CTRL_OPT_ZLWLL_GPC, i, m_SavedGpcZlwllMask[i]);
            }
        }
    }

    if (m_FsFbioParamPresent)
        RestoreFbio();

    if (m_FsFbioShiftParamPresent)
        regs.Write32Sync(MODS_FUSE_CTRL_OPT_FBIO_SHIFT, m_SavedFbioShift);

    if (m_FsPcieLaneParamPresent)
        regs.Write32Sync(MODS_FUSE_CTRL_OPT_PCIE_LANE, m_SavedFsPcieLaneControl);

    if (m_FsFbpaParamPresent)
        regs.Write32Sync(MODS_FUSE_CTRL_OPT_FBPA, m_SavedFbpa);

    if (m_FsSpareParamPresent)
        regs.Write32Sync(MODS_FUSE_CTRL_OPT_SPARE_FS, m_SavedSpare);

    if (m_FsHeadParamPresent)
        regs.Write32Sync(MODS_FUSE_CTRL_OPT_DISP_HEAD, m_SavedHead);
}

//------------------------------------------------------------------------------
// GM107 yes | GM108 Yes | GM200 yes | GM204 yes | GM206 yes | GM204X yes | GM206X yes
void GM10xFs::ResetFloorsweepingSettings()
{
    Printf(Tee::PriDebug, "GM10xGpuSubdevice::ResetFloorsweepingSettings: "
           " reading and resetting settings\n");

    auto& regs = m_pSub->Regs();

    UINT32 display;
    if (RC::OK == regs.Read32Priv(MODS_FUSE_OPT_DISPLAY_DISABLE, &display))
    {
        (void)regs.Write32PrivSync(MODS_FUSE_CTRL_OPT_DISPLAY, display);
    }

    UINT32 lwenc;
    if (RC::OK == regs.Read32Priv(MODS_FUSE_OPT_LWENC_DISABLE, &lwenc))
    {
        (void)regs.Write32PrivSync(MODS_FUSE_CTRL_OPT_LWENC, lwenc);
    }

    UINT32 lwdec;
    if (RC::OK == regs.Read32Priv(MODS_FUSE_OPT_LWDEC_DISABLE, &lwdec))
    {
        (void)regs.Write32PrivSync(MODS_FUSE_CTRL_OPT_LWDEC, lwdec);
    }

    if (m_FsFbioShiftOverrideParamPresent)
    {
        UINT32 shiftOverride;
        if (RC::OK == regs.Read32Priv(MODS_FUSE_OPT_FBIO_SHIFT, &shiftOverride))
        {
            (void)regs.Write32PrivSync(MODS_FUSE_CTRL_OPT_FBIO_SHIFT_OVERRIDE,
                                       (shiftOverride == 0) ? 0 : 1);
        }
    }

    if (IsCeFloorsweepingSupported())
    {
        UINT32 ce = regs.Read32(MODS_FUSE_OPT_CE_DISABLE);
        regs.Write32Sync(MODS_FUSE_CTRL_OPT_CE, ce);
    }

    UINT32 gpc = regs.Read32(MODS_FUSE_OPT_GPC_DISABLE);
    regs.Write32Sync(MODS_FUSE_CTRL_OPT_GPC, gpc);

    UINT32 fbp = 0;
    if (m_pSub->HasBug(1684053))
    {
        // Bug 1684053 - On GP100, FBPs can't be fused, so
        // we have to infer the correct values based on FBIO
        // If all FBIOs under an FBP are fused, we can assume
        // the FBP is meant to be disabled
        UINT32 fbio = regs.Read32(MODS_FUSE_OPT_FBIO_DISABLE);
        UINT32 numFbps = regs.LookupMaskSize(MODS_FUSE_OPT_FBP_DISABLE_DATA);
        UINT32 numFbios = regs.LookupMaskSize(MODS_FUSE_OPT_FBIO_DISABLE_DATA);
        UINT32 ratio = numFbios / numFbps;
        UINT32 submask = (1 << ratio) - 1;
        for (UINT32 i = 0; i < numFbps; i++)
        {
            if (((fbio >> i * ratio) & submask) == submask)
            {
                fbp |= 1 << i;
            }
        }
    }
    else
    {
        fbp = regs.Read32(MODS_FUSE_OPT_FBP_DISABLE);
    }
    regs.Write32Sync(MODS_FUSE_CTRL_OPT_FBP, fbp);

    UINT32 maxGpcs = m_pSub->GetMaxGpcCount();
    UINT32 maxTpcs = m_pSub->GetMaxTpcCount();

    //no risk of overflow since this is per gpc
    UINT32 tpcMask = (1 << maxTpcs) - 1;
    if (regs.IsSupported(MODS_FUSE_OPT_TPC_DISABLE))
    {
        UINT32 gpcTpc = regs.Read32(MODS_FUSE_OPT_TPC_DISABLE);
        //number of tpcs and zlwlls differ per gpc,
        //so we keep track of the masks separately
        for (UINT32 gpcIndex = 0; gpcIndex < maxGpcs; gpcIndex++)
        {
            UINT32 tpc = (gpcTpc & tpcMask) >> (gpcIndex * maxTpcs);
            regs.Write32(MODS_FUSE_CTRL_OPT_TPC_GPC, gpcIndex, tpc);
            tpcMask <<= maxTpcs;
        }
    }

    if (regs.IsSupported(MODS_FUSE_CTRL_OPT_ZLWLL_GPC))
    {
        UINT32 maxZlwlls = m_pSub->GetMaxZlwllCount();
        UINT32 zlwllMask = (1 << maxZlwlls) - 1;
        UINT32 gpcZLwll = regs.Read32(MODS_FUSE_OPT_ZLWLL_DISABLE);
        for (UINT32 gpcIndex = 0; gpcIndex < maxGpcs; gpcIndex++)
        {
            UINT32 zlwll = (gpcZLwll & zlwllMask) >> (gpcIndex * maxZlwlls);
            regs.Write32(MODS_FUSE_CTRL_OPT_ZLWLL_GPC, gpcIndex, zlwll);
            zlwllMask <<= maxZlwlls;
        }
    }

    ResetFbio();

    if (m_FsFbioShiftParamPresent)
    {
        UINT32 fbioShift = regs.Read32(MODS_FUSE_OPT_FBIO_SHIFT);
        regs.Write32Sync(MODS_FUSE_CTRL_OPT_FBIO_SHIFT, fbioShift);
    }

    UINT32 pcieLane = regs.Read32(MODS_FUSE_OPT_PCIE_LANE_DISABLE);
    regs.Write32Sync(MODS_FUSE_CTRL_OPT_PCIE_LANE, pcieLane);

    if (regs.IsSupported(MODS_FUSE_OPT_FBPA_DISABLE))
    {
        UINT32 fbpa = regs.Read32(MODS_FUSE_OPT_FBPA_DISABLE);
        regs.Write32Sync(MODS_FUSE_CTRL_OPT_FBPA, fbpa);
    }

    UINT32 spare = regs.Read32(MODS_FUSE_OPT_SPARE_FS);
    regs.Write32Sync(MODS_FUSE_CTRL_OPT_SPARE_FS, spare);

    UINT32 dispHead = regs.Read32(MODS_FUSE_OPT_DISP_HEAD_DISABLE);
    regs.Write32Sync(MODS_FUSE_CTRL_OPT_DISP_HEAD, dispHead);
}

//------------------------------------------------------------------------------
// GM107 yes | GM108 Yes | GM200 yes | GM204 yes | GM206 yes | GM204X yes | GM206X yes
void GM10xFs::SaveFloorsweepingSettings()
{
    // save all floorsweeping settings that could conceivably be changed by this
    // GpuSubdevice (in ApplyFloorsweepingSettings)
    //

    auto& regs = m_pSub->Regs();

    (void)regs.Read32Priv(MODS_FUSE_CTRL_OPT_DISPLAY, &m_SavedFsDispControl);
    (void)regs.Read32Priv(MODS_FUSE_CTRL_OPT_LWDEC, &m_SavedFsLwdecControl);
    (void)regs.Read32Priv(MODS_FUSE_CTRL_OPT_LWENC, &m_SavedFsLwencControl);

    if (m_FsFbioShiftOverrideParamPresent)
    {
        (void)regs.Read32Priv(MODS_FUSE_CTRL_OPT_FBIO_SHIFT_OVERRIDE,
                              &m_SavedFsFbioShiftOverrideControl);
    }

    if (IsCeFloorsweepingSupported())
        (void)regs.Read32Priv(MODS_FUSE_CTRL_OPT_CE, &m_SavedFsCeControl);
    (void)regs.Read32Priv(MODS_FUSE_CTRL_OPT_GPC, &m_SavedGpcMask);
    (void)regs.Read32Priv(MODS_FUSE_CTRL_OPT_FBP, &m_SavedFbpMask);
    (void)regs.Read32Priv(MODS_FUSE_CTRL_OPT_FBIO, &m_SavedFbio);

    if (m_FsFbioShiftParamPresent)
    {
        (void)regs.Read32Priv(MODS_FUSE_CTRL_OPT_FBIO_SHIFT,
                              &m_SavedFbioShift);
    }

    // save the per-gpc individual subunit enables
    //
    const UINT32 maxGpc = m_pSub->GetMaxGpcCount();
    MASSERT(maxGpc <= (UINT32)m_SavedGpcTpcMask.size());
    for (UINT32 i = 0; i < maxGpc; ++i)
    {
        (void)regs.Read32Priv(MODS_FUSE_CTRL_OPT_TPC_GPC, i,
                              &m_SavedGpcTpcMask[i]);
    }

    if (regs.IsSupported(MODS_FUSE_CTRL_OPT_ZLWLL_GPC))
    {
        // Lazy initialize this array
        if (m_SavedGpcZlwllMask.size() == 0)
        {
            m_SavedGpcZlwllMask.resize(regs.LookupArraySize(MODS_FUSE_CTRL_OPT_ZLWLL_GPC, 1));
        }
        for (UINT32 i = 0; i < m_SavedGpcZlwllMask.size(); ++i)
        {
            (void)regs.Read32Priv(MODS_FUSE_CTRL_OPT_ZLWLL_GPC, i,
                                  &m_SavedGpcZlwllMask[i]);
        }
    }

    (void)regs.Read32Priv(MODS_FUSE_CTRL_OPT_PCIE_LANE,
                          &m_SavedFsPcieLaneControl);
    (void)regs.Read32Priv(MODS_FUSE_CTRL_OPT_FBPA, &m_SavedFbpa);
    (void)regs.Read32Priv(MODS_FUSE_CTRL_OPT_SPARE_FS, &m_SavedSpare);
    (void)regs.Read32Priv(MODS_FUSE_CTRL_OPT_DISP_HEAD, &m_SavedHead);
}

//------------------------------------------------------------------------------
/* virtual */ RC GM10xFs::CheckFloorsweepingArguments()
{
    RC rc;
    auto& regs = m_pSub->Regs();
    vector<const char*> failedArgs;

    if (IsFloorsweepingAllowed())
    {
        if (m_FsGpcParamPresent)
        {
            CHECK_RC(CheckFloorsweepingMask(regs.Read32(MODS_FUSE_STATUS_OPT_GPC),
                     m_GpcMask, m_pSub->GetMaxGpcCount(), "GPC"));
            if (!regs.HasRWAccess(MODS_FUSE_CTRL_OPT_GPC))
            {
                failedArgs.push_back("gpc");
            }
        }

        if (m_FsFbParamPresent)
        {
            CHECK_RC(CheckFloorsweepingMask(regs.Read32(MODS_FUSE_STATUS_OPT_FBP),
                     m_FbMask, m_pSub->GetMaxFbpCount(), "FBP"));
            if (!regs.HasRWAccess(MODS_FUSE_CTRL_OPT_FBP))
            {
                failedArgs.push_back("fbp");
            }
            if (!regs.HasRWAccess(MODS_FUSE_CTRL_OPT_FBP) ||
                (m_pSub->HasBug(1684053) &&
                 (!regs.HasReadAccess(MODS_FUSE_OPT_FBIO_DISABLE) ||
                  !regs.HasReadAccess(MODS_PTOP_SCAL_NUM_FBPA_PER_FBP))))
            {
                failedArgs.push_back("fbp");
            }
        }

        UINT32 gpcNum = 0;
        UINT32 maxTpcCount = m_pSub->GetMaxTpcCount();
        for (gpcNum = 0; gpcNum < m_GpcTpcMasks.size(); gpcNum++)
        {
            if (!m_FsGpcTpcParamPresent[gpcNum])
                continue;

            CHECK_RC(CheckFloorsweepingMask(regs.Read32(MODS_FUSE_STATUS_OPT_TPC_GPC, gpcNum),
                     m_GpcTpcMasks[gpcNum], maxTpcCount, "TPC"));
            if (!regs.HasRWAccess(MODS_FUSE_CTRL_OPT_TPC_GPC, gpcNum))
            {
                failedArgs.push_back("gpc_tpc");
            }
        }

        if (regs.IsSupported(MODS_FUSE_STATUS_OPT_ZLWLL_GPC))
        {
            UINT32 maxZlwllCount = m_pSub->GetMaxZlwllCount();
            for (gpcNum = 0; gpcNum < m_GpcZlwllMasks.size(); gpcNum++)
            {
                if (!m_FsGpcZlwllParamPresent[gpcNum])
                    continue;

                CHECK_RC(CheckFloorsweepingMask(
                            regs.Read32(MODS_FUSE_STATUS_OPT_ZLWLL_GPC, gpcNum),
                            m_GpcZlwllMasks[gpcNum], maxZlwllCount, "ZLWLL"));
                if (!regs.HasRWAccess(MODS_FUSE_CTRL_OPT_ZLWLL_GPC, gpcNum))
                {
                    failedArgs.push_back("gpc_zlwll");
                }
            }
        }

        if (m_FsDisplayParamPresent &&
            !regs.HasRWAccess(MODS_FUSE_CTRL_OPT_DISPLAY))
        {
            failedArgs.push_back("display");
        }

        if (m_FsLwdecParamPresent &&
            !regs.HasRWAccess(MODS_FUSE_CTRL_OPT_LWDEC))
        {
            failedArgs.push_back("lwdec");
        }

        if (m_FsFbioShiftOverrideParamPresent &&
            !regs.HasRWAccess(MODS_FUSE_CTRL_OPT_FBIO_SHIFT_OVERRIDE))
        {
            failedArgs.push_back("fbio_shift_override");
        }

        if (m_FsCeParamPresent &&
            IsCeFloorsweepingSupported() &&
            !regs.HasRWAccess(MODS_FUSE_CTRL_OPT_CE))
        {
            failedArgs.push_back("ce");
        }

        if (m_FsLwencParamPresent &&
            !regs.HasRWAccess(MODS_FUSE_CTRL_OPT_LWENC))
        {
            failedArgs.push_back("lwenc");
        }

        if (m_FsFbioParamPresent &&
            !regs.HasRWAccess(MODS_FUSE_CTRL_OPT_FBIO))
        {
            failedArgs.push_back("fbio");
        }

        if (m_FsFbioShiftParamPresent &&
            !regs.HasRWAccess(MODS_FUSE_CTRL_OPT_FBIO_SHIFT))
        {
            failedArgs.push_back("fbio_shift");
        }

        if (m_FsPcieLaneParamPresent &&
            !regs.HasRWAccess(MODS_FUSE_CTRL_OPT_PCIE_LANE))
        {
            failedArgs.push_back("pcie_lane");
        }

        if (m_FsFbpaParamPresent &&
            !regs.HasRWAccess(MODS_FUSE_CTRL_OPT_FBPA))
        {
            failedArgs.push_back("fbpa");
        }

        if (m_FsSpareParamPresent &&
            !regs.HasRWAccess(MODS_FUSE_CTRL_OPT_SPARE_FS))
        {
            failedArgs.push_back("spare");
        }

        if (m_FsHeadParamPresent &&
            !regs.HasRWAccess(MODS_FUSE_CTRL_OPT_DISP_HEAD))
        {
            failedArgs.push_back("head");
        }

        if (m_pSub->DeviceId() == Gpu::GM107)
        {
            const UINT32 maxTpcCount = m_pSub->GetMaxTpcCount();
            const UINT32 maxTpcDisabledMask = (1 << maxTpcCount) - 1;
            const UINT32 disallowedTpcDisabledMask = 7;
            const UINT32 gpcTpcMask = m_GpcTpcMasks[0] & maxTpcDisabledMask;

            //Bug 1373450
            if ((gpcTpcMask & disallowedTpcDisabledMask)
                == disallowedTpcDisabledMask)
            {
                Printf(Tee::PriHigh, "GM107GpuSubdevice::"
                                     "CheckFloorsweepingArguments():"
                                     " Cannot have the first three TPCs"
                                     " all disabled\n");
                return RC::BAD_COMMAND_LINE_ARGUMENT;
            }
        }
    }

    if (!failedArgs.empty())
    {
        for (const char* failedArg: failedArgs)
        {
            Printf(Tee::PriError, "%s floorsweeping not supported on %s\n",
                   failedArg,
                   Gpu::DeviceIdToString(m_pSub->DeviceId()).c_str());
        }
        return RC::BAD_COMMAND_LINE_ARGUMENT;
    }
    return FermiFs::CheckFloorsweepingArguments();
}

//------------------------------------------------------------------------------
/* virtual */ RC GM10xFs::GetFloorsweepingMasks
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

    CHECK_RC(FermiFs::GetFloorsweepingMasks(pFsMasks));

    //TODO: Add any Maxwell generic floorsweeping masks here:

    return rc;
}

//------------------------------------------------------------------------------
// GM107 yes | GM108 Yes | GM200 yes | GM204 yes | GM206 yes | GM204X yes | GM206X yes
/* virtual */ UINT32 GM10xFs::FscontrolMask() const
{
    UINT32 fsctrMask = DRF_SHIFTMASK(LW_PTOP_FS_STATUS_DISPLAY)   |
                       DRF_SHIFTMASK(LW_PTOP_FS_STATUS_MSDEC)     |
                       DRF_SHIFTMASK(LW_PTOP_FS_STATUS_MSVLD_SEC) |
                       DRF_SHIFTMASK(LW_PTOP_FS_STATUS_MSENC)     |
                       DRF_SHIFTMASK(LW_PTOP_FS_STATUS_PCIE_LANE);

    // Pascal doesn't support floorsweeping of physical CEs
    if (IsCeFloorsweepingSupported())
        fsctrMask |= DRF_SHIFTMASK(LW_PTOP_FS_STATUS_CE);

    if (!IsFloorsweepingAllowed())
    {
        // Floorsweeping does not work on Amodel or FModel on most GPUs.
        // In that case just return no shifts
        return fsctrMask;
    }
    // Maxwell doesn't really store the FS status bits in a single register. So
    // compose the legacy mask out of the individual registers.
    UINT32 status = 0;
    status |= m_pSub->Regs().Read32(MODS_FUSE_STATUS_OPT_DISPLAY_DATA) <<
                            DRF_SHIFT(LW_PTOP_FS_STATUS_DISPLAY);

    status |= m_pSub->Regs().Read32(MODS_FUSE_STATUS_OPT_LWDEC_DATA) <<
                            DRF_SHIFT(LW_PTOP_FS_STATUS_MSVLD_SEC);

    // MSENC has been renamed to LWENC
    status |= m_pSub->Regs().Read32(MODS_FUSE_STATUS_OPT_LWENC_DATA) <<
                            DRF_SHIFT(LW_PTOP_FS_STATUS_MSENC);

    if (IsCeFloorsweepingSupported())
    {
        status |= m_pSub->Regs().Read32(MODS_FUSE_STATUS_OPT_CE_DATA) <<
                            DRF_SHIFT(LW_PTOP_FS_STATUS_CE);
    }

    status |= m_pSub->Regs().Read32(MODS_FUSE_STATUS_OPT_PCIE_LANE_DATA) <<
                            DRF_SHIFT(LW_PTOP_FS_STATUS_PCIE_LANE);
    return (~status & fsctrMask);
}

//------------------------------------------------------------------------------
// GM107 yes | GM108 Yes | GM200 yes | GM204 yes | GM206 yes | GM204X yes | GM206X yes
/* virtual */ UINT32 GM10xFs::FbpMaskImpl() const
{
    switch (Platform::GetSimulationMode())
    {
        case Platform::Hardware:
        case Platform::Fmodel:
        case Platform::Amodel:
            return (~m_pSub->Regs().Read32(MODS_FUSE_STATUS_OPT_FBP) &
                    ((1 << m_pSub->Regs().Read32(MODS_PTOP_SCAL_NUM_FBPS)) - 1));

        case Platform::RTL:
            return Utility::FindNLowestSetBits(
                    ~m_pSub->Regs().Read32(MODS_FUSE_STATUS_OPT_FBP) &
                    ((1 << m_pSub->Regs().Read32(MODS_PTOP_SCAL_NUM_FBPS)) - 1),
                    m_pSub->Regs().Read32(MODS_PPRIV_MASTER_RING_ENUMERATE_RESULTS_FBP));
        default:
            MASSERT(!"Illegal case");
            break;
    }
    return 0;
}

//------------------------------------------------------------------------------
// GM107 yes | GM108 Yes | GM200 yes | GM204 yes | GM206 yes GM204X yes | GM206X yes
/* virtual */ UINT32 GM10xFs::FbioMaskImpl() const
{
    switch (Platform::GetSimulationMode())
    {
        case Platform::Hardware:
        case Platform::Fmodel:
        case Platform::Amodel:
            return (~m_pSub->Regs().Read32(MODS_FUSE_STATUS_OPT_FBIO) &
                    ((1 << m_pSub->Regs().Read32(MODS_PTOP_SCAL_NUM_FBPS)) - 1));

        case Platform::RTL:
            return Utility::FindNLowestSetBits(
                        ~m_pSub->Regs().Read32(MODS_FUSE_STATUS_OPT_FBIO) &
                        ((1 << m_pSub->Regs().Read32(MODS_PTOP_SCAL_NUM_FBPS)) - 1),
                        m_pSub->Regs().Read32(MODS_PPRIV_MASTER_RING_ENUMERATE_RESULTS_FBP));
        default:
            MASSERT(!"Illegal case");
            break;
    }
    return 0;
}

//------------------------------------------------------------------------------
// GM107 yes | GM108 Yes | GM200 yes | GM204 yes | GM206 yes | GM204X yes | GM206X yes
/* virtual */ UINT32 GM10xFs::FbpaMaskImpl() const
{
    const UINT32 allFbpasMask = (1 << m_pSub->Regs().Read32(MODS_PTOP_SCAL_NUM_FBPAS)) - 1;

    if (!IsFloorsweepingAllowed())
    {
        // Floorsweeping does not work on Amodel or FModel on most GPUs.
        // In that case just return no shifts
        return allFbpasMask;
    }

    return (~m_pSub->Regs().Read32(MODS_FUSE_STATUS_OPT_FBPA) & allFbpasMask);
}

//------------------------------------------------------------------------------
// GM107 yes | GM108 Yes | GM200 yes | GM204 yes | GM206 yes | GM204X yes | GM206X yes
/* virtual */ UINT32 GM10xFs::GpcMask() const
{
    if (m_pSub->IsSOC() && Platform::UsesLwgpuDriver())
    {
        static UINT32 gpcMask = 0;

        if (!gpcMask)
        {
            const string fsMaskFile = CheetAh::GetGpuSysFsFile("gpc_fs_mask");
            if (fsMaskFile.empty())
            {
                // If gpc_fs_mask doesn't exist, we assume we're running with an
                // older chip or an older image where there's only one GPC
                gpcMask = 1;
            }
            else
            {
                string fsmask;
                if (Xp::InteractiveFileRead(fsMaskFile.c_str(), &fsmask) == RC::OK)
                {
                    const UINT32 maxGpcMask = (1u << m_pSub->GetMaxGpcCount()) - 1u;
                    const UINT32 readGpcMask = static_cast<UINT32>(strtoul(fsmask.c_str(), 0, 0));
                    gpcMask = readGpcMask & maxGpcMask;

                    // WAR for T186
                    if (!gpcMask)
                    {
                        gpcMask = maxGpcMask;
                    }
                }
            }
        }

        return gpcMask;
    }

    UINT32 gpcFuse = 0;
    if (m_pSub->Regs().Read32Priv(MODS_FUSE_STATUS_OPT_GPC, &gpcFuse) != RC::OK)
    {
        Printf(Tee::PriWarn, "Cannot read GPC mask\n");
    }

    switch (Platform::GetSimulationMode())
    {
        case Platform::Hardware:
        case Platform::Fmodel:
        case Platform::Amodel:
            return ~gpcFuse & ((1 << m_pSub->GetMaxGpcCount()) - 1);

        case Platform::RTL:
            return Utility::FindNLowestSetBits(
                        ~gpcFuse & ((1 << m_pSub->GetMaxGpcCount()) - 1),
                        m_pSub->Regs().Read32(MODS_PPRIV_MASTER_RING_ENUMERATE_RESULTS_GPC));
        default:
            MASSERT(!"Illegal case");
            break;
    }
    return 0;
}

//------------------------------------------------------------------------------
// GM107 yes | GM108 Yes | GM200 yes | GM204 yes | GM206 yes | GM204X yes | GM206X yes
/* virtual */ UINT32 GM10xFs::TpcMask(UINT32 HwGpcNum) const
{
    if (Platform::UsesLwgpuDriver())
    {
        static UINT32 tpcMask = 0;

        if (!tpcMask)
        {
            const string fsMaskFile = CheetAh::GetGpuSysFsFile("tpc_fs_mask");
            if (!fsMaskFile.empty())
            {
                string fsmask;
                if (Xp::InteractiveFileRead(fsMaskFile.c_str(), &fsmask) == RC::OK)
                {
                    const UINT32 gpcMask = GpcMask();
                    if (!(gpcMask & (1u << HwGpcNum)))
                    {
                        Printf(Tee::PriLow, "TPC FS mask on GPC %u is 0, because GPC FS mask is 0x%x\n", HwGpcNum, gpcMask);
                    }
                    else
                    {
                        const UINT32 tpcsPerGpc = m_pSub->GetMaxTpcCount();
                        const UINT32 maxTpcMask = ((1u << tpcsPerGpc) - 1u);
                        UINT32 fullTpcMask = static_cast<UINT32>(strtoul(fsmask.c_str(), 0, 0));

                        // TODO temporary WAR for inconsistent tpc_fs_mask
                        if ((gpcMask == 2) && (fullTpcMask <= maxTpcMask))
                        {
                            fullTpcMask <<= tpcsPerGpc;
                        }

                        fullTpcMask >>= tpcsPerGpc * HwGpcNum;

                        tpcMask = fullTpcMask & maxTpcMask;
                    }
                }
            }
        }

        return tpcMask;
    }

    // If the GpcNum was not supplied, get the number of TPCs from the first
    // non-floorswept GPC
    if (HwGpcNum == ~0U)
    {
        INT32 FirstGpc = Utility::BitScanForward(GpcMask());
        if (FirstGpc < 0)
            return 0;
        HwGpcNum = (UINT32)FirstGpc;
    }
    else if (!(GpcMask() & (1 << HwGpcNum)))
    {
        return 0;
    }

    Platform::SimulationMode simMode = Platform::GetSimulationMode();
    UINT32 simTpcCount = 0;     // Number of TPC bits in sim or RTL
    UINT32 hwTpcMask = 0;       // Floorswept TPC mask in hw and RTL

    // Callwlate simTpcCount
    if (simMode != Platform::Hardware)
    {
        UINT32 virtualGpcNum = 0;
        if (OK != m_pSub->HwGpcToVirtualGpc(HwGpcNum, &virtualGpcNum))
        {
            return 0;
        }
        simTpcCount =
            m_pSub->Regs().Read32(MODS_PGRAPH_PRI_GPCx_GPCCS_FS_GPC_NUM_AVAILABLE_TPCS,
                                  virtualGpcNum);
    }

    // Callwlate hwTpcMask
    if (IsFloorsweepingAllowed())
    {
        UINT32 tpcs = 0;
        if (RC::OK != m_pSub->Regs().Read32Priv(MODS_FUSE_STATUS_OPT_TPC_GPC,
                                                HwGpcNum, &tpcs))
        {
            Printf(Tee::PriWarn, "Cannot read TPC mask\n");
        }
        hwTpcMask = ((1 << m_pSub->GetMaxTpcCount()) - 1);
        hwTpcMask &= ~tpcs;
    }

    // Return TPC mask
    switch (simMode)
    {
        case Platform::Hardware:
            return hwTpcMask;
        case Platform::Amodel:
            // Floorsweeping does not work on models.  Just create a mask
            // of available TPCs
            return (1 << simTpcCount) - 1;
        case Platform::Fmodel:
            if(IsFloorsweepingAllowed())
            {
                return hwTpcMask;
            }
            else
            {
                // Floorsweeping may not work on Fmodel. If not just create a mask
                // of available TPCs
                return (1 << simTpcCount) - 1;
            }
        case Platform::RTL:
            return Utility::FindNLowestSetBits(hwTpcMask, simTpcCount);
        default:
            MASSERT(!"Illegal simMode");
            break;
    }

    return 0;
}

//------------------------------------------------------------------------------
/*virtual*/ UINT32 GM10xFs::LwdecMask() const
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
            return (~m_pSub->Regs().Read32(MODS_FUSE_STATUS_OPT_LWDEC) &
                    m_pSub->Regs().LookupFieldValueMask(MODS_FUSE_STATUS_OPT_LWDEC_DATA));
        default:
            MASSERT(!"Illegal case");
            break;
    }
    return 0;
}

//------------------------------------------------------------------------------
/*virtual*/ UINT32 GM10xFs::LwencMask() const
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
            return (~m_pSub->Regs().Read32(MODS_FUSE_STATUS_OPT_LWENC) &
                    m_pSub->Regs().LookupFieldValueMask(MODS_FUSE_STATUS_OPT_LWENC_DATA));
        default:
            MASSERT(!"Illegal case");
            break;
    }
    return 0;
}

//------------------------------------------------------------------------------
/* virtual */ void GM10xFs::RestoreFbio() const
// GM107 yes | GM108 Yes | GM200 yes | GM204 yes | GM206 yes | GM204X yes | GM206X yes
{
    // Bug 908028:
    // We must first power up all the main regulators before restoring FBPs.
    m_pSub->Regs().Write32Sync(MODS_PTRIM_SYS_FBIO_SPARE_FBIO_SPARE_INIT);

    //  Now restore them just like any other fermi+ chip
    m_pSub->Regs().Write32Sync(MODS_FUSE_CTRL_OPT_FBIO, m_SavedFbio);
}

//------------------------------------------------------------------------------
/* virtual */ void GM10xFs::ResetFbio() const
// GM107 yes | GM108 Yes | GM200 yes | GM204 yes | GM206 yes | GM204X yes | GM206X yes
{
    // Bug 908028:
    // We must first power up all the main regulators before restoring FBPs.
    m_pSub->Regs().Write32Sync(MODS_PTRIM_SYS_FBIO_SPARE_FBIO_SPARE_INIT);

    //  Now reset them just like any other fermi+ chip
    UINT32 fbio =  m_pSub->Regs().Read32(MODS_FUSE_OPT_FBIO_DISABLE);
    m_pSub->Regs().Write32Sync(MODS_FUSE_CTRL_OPT_FBIO, fbio);
}

/* virtual */ RC GM10xFs::PrintMiscFuseRegs()
{
    auto& regs = m_pSub->Regs();

    Printf(Tee::PriNormal," OPT_MCHIP_EN   : 0x%x\n",
                            regs.Read32(MODS_FUSE_OPT_MCHIP_EN));
    Printf(Tee::PriNormal," OPT_HDCP_EN    : 0x%x\n",
                            regs.Read32(MODS_FUSE_OPT_HDCP_EN));
    Printf(Tee::PriNormal," OPT_GC6_ISLAND : 0x%x\n",
                            regs.Read32(MODS_FUSE_STATUS_OPT_GC6_ISLAND));
    Printf(Tee::PriNormal," OPT_DISPLAY_DIS: 0x%x\n",
                            regs.Read32(MODS_FUSE_STATUS_OPT_DISPLAY));
    Printf(Tee::PriNormal," OPT_LWDEC_DIS  : 0x%x\n",
                            regs.Read32(MODS_FUSE_STATUS_OPT_LWDEC));
    Printf(Tee::PriNormal," OPT_LWENC_DIS  : 0x%x\n",
                            regs.Read32(MODS_FUSE_STATUS_OPT_LWENC));
    Printf(Tee::PriNormal," OPT_ECC_EN     : 0x%x\n",
                            regs.Read32(MODS_FUSE_OPT_ECC_EN));
    if (IsCeFloorsweepingSupported())
    {
        Printf(Tee::PriNormal," OPT_CE_DIS     : 0x%x\n",
                                regs.Read32(MODS_FUSE_STATUS_OPT_CE));
    }
    Printf(Tee::PriNormal," OPT_SEC_DEBUG_EN: 0x%x\n",
                            regs.Read32(MODS_FUSE_OPT_SEC_DEBUG_EN));
    Printf(Tee::PriNormal," OPT_PRIV_SEC_EN: 0x%x\n",
                            regs.Read32(MODS_FUSE_STATUS_OPT_PRIV_SEC_EN));
    return OK;
}

//------------------------------------------------------------------------------
// GM107 yes | GM108 Yes | GM200 yes | GM204 yes | GM206 yes | GM204X yes | GM206X yes
/* virtual */ UINT32 GM10xFs::FbioshiftMaskImpl() const
{
    RegHal &regs = m_pSub->Regs();
    if (!IsFloorsweepingAllowed())
    {
        // Floorsweeping does not work on Amodel or FModel on most GPUs.
        // In that case just return no shifts
        return 0;
    }
    if (!regs.IsSupported(MODS_FUSE_STATUS_OPT_FBIO_SHIFT))
    {
        // FBIO shift was dropped in later chips; always use iron-grid
        return 0;
    }

    UINT32 fbioShiftMask = ((1 << m_pSub->Regs().Read32(MODS_PTOP_SCAL_NUM_FBPS)) - 1);
    return (regs.Read32(MODS_FUSE_STATUS_OPT_FBIO_SHIFT_DATA) &
            fbioShiftMask);
}

//------------------------------------------------------------------------------
// GM107 yes | GM108 Yes | GM200 yes | GM204 yes | GM206 yes | GM204X yes | GM206X yes
/* virtual */ UINT32 GM10xFs::ZlwllMask(UINT32 HwGpcNum) const
{
    auto& regs = m_pSub->Regs();
    if (!regs.IsSupported(MODS_FUSE_STATUS_OPT_ZLWLL_GPC))
    {
        return 0;
    }
    // If the GpcNum was not supplied, get the number of Zlwlls from the first
    // non-floorswept Gpc
    if (HwGpcNum == ~0U)
    {
        INT32 FirstGpc = Utility::BitScanForward(GpcMask());
        if (FirstGpc < 0)
            return 0;
        HwGpcNum = (UINT32)FirstGpc;
    }
    else if (!(GpcMask() & (1 << HwGpcNum)))
    {
        return 0;
    }

    if (!IsFloorsweepingAllowed())
    {
        // Floorsweeping does not work on Amodel or FModel on most GPUs.
        // In that case just return no shifts
        UINT32 virtualGpcNum = 0;
        if (OK != m_pSub->HwGpcToVirtualGpc(HwGpcNum, &virtualGpcNum))
        {
            return 0;
        }
        const UINT32 zlwllCount =
            m_pSub->Regs().Read32(MODS_PGRAPH_PRI_GPCx_GPCCS_FS_GPC_NUM_AVAILABLE_ZLWLLS,
                                  virtualGpcNum);
        return (1 << zlwllCount) - 1;
    }

    UINT32 zlwllPerGpc = regs.Read32(MODS_FUSE_STATUS_OPT_ZLWLL_GPC, HwGpcNum);
    UINT32 zlwllMask = ((1 << m_pSub->GetMaxTpcCount()) - 1); // Max zlwlls == Max tpcs
    return zlwllMask & ~zlwllPerGpc;
}

//------------------------------------------------------------------------------
// GM107 yes | GM108 Yes | GM200 yes | GM204 yes | GM206 yes | GM204X yes | GM206X yes
/* virtual */ UINT32 GM10xFs::SpareMask() const
{
    UINT32 spareMask = m_pSub->Regs().LookupMask(MODS_FUSE_STATUS_OPT_SPARE_FS_DATA);

    if (!IsFloorsweepingAllowed())
    {
        // Floorsweeping does not work on Amodel or FModel on most GPUs.
        // In that case just return no shifts
        return spareMask;
    }

    return (~m_pSub->Regs().Read32(MODS_FUSE_STATUS_OPT_SPARE_FS) & spareMask);
}

//------------------------------------------------------------------------------
// GM107 yes | GM108 Yes | GM200 yes | GM204 yes | GM206 yes | GM204X yes | GM206X yes
/* virtual */ UINT32 GM10xFs::XpMask() const
{
    UINT32 pcieLaneData = 0;
    if (RC::OK != m_pSub->Regs().Read32Priv(MODS_FUSE_STATUS_OPT_PCIE_LANE_DATA,
                                            &pcieLaneData))
    {
        Printf(Tee::PriWarn, "Cannot read XP mask\n");
    }
    return (pcieLaneData ^
            m_pSub->Regs().LookupFieldValueMask(MODS_FUSE_STATUS_OPT_PCIE_LANE_DATA));
}

//------------------------------------------------------------------------------
CREATE_GPU_FS_FUNCTION(GM10xFs);

