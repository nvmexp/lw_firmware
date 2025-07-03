/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2013-2022 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#include "fermifs.h"
#include "core/include/platform.h"
#include "core/include/gpu.h"
#include "core/include/utility.h"
#include "fermi/gf100/dev_graphics_nobundle.h"
#include "fermi/gf100/hwproject.h"
#include "fermi/gf100/dev_master.h"
#include "gpu/utility/fsp.h"

// We need to include gf108 version of dev_top and dev_fuse to access the following regs:
// LW_PTOP_FS_CONTROL_FBPA
// LW_PTOP_FS_CONTROL_SPARE
// LW_PTOP_FS_CONTROL_PCIE_LANE
#include "fermi/gf108/dev_top.h"

// @@@ http://lwbugs/594530 is is filed to put this into the manual.
#ifndef LW_PTOP_SCRATCH1_FS_SW_PROTECT
#define LW_PTOP_SCRATCH1_FS_SW_PROTECT                          0:0
#endif

#ifndef LW_PTOP_SCRATCH1_FS_SW_PROTECT_OFF
#define LW_PTOP_SCRATCH1_FS_SW_PROTECT_OFF               0x00000000
#endif

#ifndef LW_PTOP_SCRATCH1_FS_SW_PROTECT_ON
#define LW_PTOP_SCRATCH1_FS_SW_PROTECT_ON                0x00000001
#endif

static const UINT32 s_MaxGpcsForFloorsweeping = 12; // large enough to accomodate AD102

// access the below parameters via:
// mods ... mdiag.js -fermi_fs <feature-string>
// a "feature-string" is <name>:<value>[:<value>...]
//

static const ParamDecl s_FloorSweepingParams[] =
{
    // contained in LW_PTOP_FS_CONTROL
    //
    { "display_enable",          "u", 0, 0, 0, "gf100 display enable"},
    { "display_disable",         "u", 0, 0, 0, "gf100 display disable"},
    { "msdec_enable",            "u", 0, 0, 0, "gf100 msdec enable"},
    { "msdec_disable",           "u", 0, 0, 0, "gf100 msdec disable"},
    { "msvld_enable",            "u", 0, 0, 0, "gf100 msvld enable"},
    { "msvld_disable",           "u", 0, 0, 0, "gf100 msvld disable"},
    { "fbio_shift_override_enable",  "u", 0, 0, 0,
                                   "gf100 fbio_shift_override enable"},
    { "fbio_shift_override_disable", "u", 0, 0, 0,
                                  "gf100 fbio_shift_override disable"},
    { "ce_enable",               "u", 0, 0, 0, "gf100 ce enable"},
    { "ce_disable",              "u", 0, 0, 0, "gf100 ce disable"},

    // contained in LW_PTOP_FS_CONTROL_GPC
    //
    { "gpc_enable",              "u", 0, 0, 0, "gf100 gpc enable"},
    { "gpc_disable",             "u", 0, 0, 0, "gf100 gpc disable"},

    // contained in LW_PTOP_FS_CONTROL_GPC_TPC
    //
    { "gpc_tpc_enable", "uu", ParamDecl::PARAM_MULTI_OK, 0, 0,
                "gf100 enable tpcs in a gpc.  Usage <gpc num> <tpc_enable>"},
    { "gpc_tpc_disable", "uu", ParamDecl::PARAM_MULTI_OK, 0, 0,
              "gf100 disable tpcs in a gpc.  Usage <gpc num> <tpc_disable>"},

    // contained in LW_PTOP_FS_CONTROL_GPC_ZLWLL
    //
    { "gpc_zlwll_enable","uu",  ParamDecl::PARAM_MULTI_OK, 0, 0,
          "gf100 enable zlwlls in a gpc.  Usage <gpc num> <zlwll_enable>"},
    { "gpc_zlwll_disable","uu",  ParamDecl::PARAM_MULTI_OK, 0, 0,
          "gf100 disable zlwlls in a gpc.  Usage <gpc num> <zlwll_disable>"},

    // contained in LW_PTOP_FS_CONTROL_FBP
    //
    { "fbp_enable",    "u", 0, 0, 0, "gf100 fb enable"},
    { "fbp_disable",   "u", 0, 0, 0, "gf100 fb disable"},

    // contained in LW_PTOP_FS_CONTROL_FBIO
    //
    { "fbio_enable",     "u", 0, 0, 0,  "gf100 fbio enable"},
    { "fbio_disable",    "u", 0, 0, 0,  "gf100 fbio disable"},

    // contained in LW_PTOP_FS_CONTROL_FBIO_SHIFT
    //
    { "fbio_shift",      "u", 0, 0, 0,  "gf100 fbio_shift"},

    LAST_PARAM
};

static const ParamDecl s_FloorSweepingParamsGF108Plus[] =
{
    // contained in LW_PTOP_FS_CONTROL
    //
    { "pcie_lane_enable",          "u", 0, 0, 0, "gf108 pcie lane enable"},
    { "pcie_lane_disable",         "u", 0, 0, 0, "gf108 pcie lane disable"},
    // contained in LW_PTOP_FS_CONTROL_FBPA
    //
    { "fbpa_enable",     "u", 0, 0, 0,  "gf108 fbpa enable"},
    { "fbpa_disable",    "u", 0, 0, 0,  "gf108 fbpa disable"},
    // contained in LW_PTOP_FS_CONTROL_SPARE
    //
    { "spare_enable",     "u", 0, 0, 0,  "gf108 spare enable"},
    { "spare_disable",    "u", 0, 0, 0,  "gf108 spare disable"},

    LAST_PARAM
};

static ParamConstraints s_FsParamConstraints[] =
{
   MUTUAL_EXCLUSIVE_PARAM("display_enable", "display_disable"),
   MUTUAL_EXCLUSIVE_PARAM("msdec_enable", "msdec_disable"),
   MUTUAL_EXCLUSIVE_PARAM("msvld_enable", "msvld_disable"),
   MUTUAL_EXCLUSIVE_PARAM("fbio_shift_override_enable",
                          "fbio_shift_override_disable"),
   MUTUAL_EXCLUSIVE_PARAM("ce_enable", "ce_disable"),
   MUTUAL_EXCLUSIVE_PARAM("gpc_enable", "gpc_disable"),
   MUTUAL_EXCLUSIVE_PARAM("gpc_tpc_enable", "gpc_tpc_disable"),
   MUTUAL_EXCLUSIVE_PARAM("gpc_zlwll_enable", "gpc_zlwll_disable"),
   MUTUAL_EXCLUSIVE_PARAM("fbp_enable", "fbp_disable"),
   MUTUAL_EXCLUSIVE_PARAM("display_enable", "display_disable"),
   MUTUAL_EXCLUSIVE_PARAM("fbio_enable", "fbio_disable"),
   LAST_CONSTRAINT
};

static ParamConstraints s_FsParamConstraintsGF108Plus[] =
{
    MUTUAL_EXCLUSIVE_PARAM("pcie_lane_enable", "pcie_lane_disable"),
    MUTUAL_EXCLUSIVE_PARAM("fbpa_enable", "fbpa_disable"),
    MUTUAL_EXCLUSIVE_PARAM("spare_enable", "spare_disable"),
    LAST_CONSTRAINT
};

//------------------------------------------------------------------------------
void FermiFs::SetVbiosControlsFloorsweep
(
    bool vbiosControlsFloorsweep
)
{
    if (Platform::GetSimulationMode() != Platform::Hardware)
    {
        // Writing scratch registers seems to offend fmodel & RTL.
        return;
    }

    // Set bit to indicate if VBIOS should overrride the floorsweeping so or not
    if (vbiosControlsFloorsweep)
    {
        Printf(Tee::PriDebug,
            "PTOP_SCRATCH1_FS_SW_PROTECT = 0 -- vbios should override FS.\n");
        m_pSub->Regs().Write32(MODS_PTOP_SCRATCH1_FS_SW_PROTECT_OFF);
    }
    else
    {
        Printf(Tee::PriDebug,
                "PTOP_SCRATCH0_FS_SW_PROTECT = 1 -- vbios should leave FS alone.\n");
        m_pSub->Regs().Write32(MODS_PTOP_SCRATCH1_FS_SW_PROTECT_ON);
    }
}

//------------------------------------------------------------------------------
// Default implementation, used when FermiFs is the final concrete class
FermiFs::FermiFs(GpuSubdevice *pSubdev):
    FloorsweepImpl(pSubdev)
{
    Init();
    FS_ADD_ARRAY_NAMED_PROPERTY(TpcEnableMaskOnGpc, s_MaxGpcsForFloorsweeping, 0);
    FS_ADD_ARRAY_NAMED_PROPERTY(ZlwllEnableMaskOnGpc, s_MaxGpcsForFloorsweeping, 0);
}

//------------------------------------------------------------------------------
// Initialize internal members.
void FermiFs::Init()
{
    m_SavedGpcTpcMask.resize(LW_PTOP_FS_CONTROL_GPC_TPC__SIZE_1);
    m_SavedGpcZlwllMask.resize(LW_PTOP_FS_CONTROL_GPC_ZLWLL__SIZE_1);

    m_GpcTpcMasks.resize(s_MaxGpcsForFloorsweeping, 0);
    m_GpcZlwllMasks.resize(s_MaxGpcsForFloorsweeping, 0);

    m_FsGpcTpcParamPresent.resize(s_MaxGpcsForFloorsweeping, false);
    m_FsGpcZlwllParamPresent.resize(s_MaxGpcsForFloorsweeping, false);
}

//------------------------------------------------------------------------------
/*static*/ bool FermiFs::PollMasterRingCommand(void *pRegs)
{
    UINT32 data = static_cast<RegHal*>(pRegs)->Read32(MODS_PPRIV_MASTER_RING_COMMAND);
    return (data == 0);
}

//------------------------------------------------------------------------------
RC FermiFs::ApplyFloorsweepingChanges(bool InPreVBIOSSetup)
{
    RC rc;

    bool bUseFspForFs = false;
    bool bSetModsControlsFs = true;
    if (m_pSub->HasFeature(Device::Feature::GPUSUB_SUPPORTS_FSP) &&
        (Platform::GetSimulationMode() == Platform::Hardware) &&
        m_pSub->IsGFWBootStarted())
    {
        bUseFspForFs = true;
    }

    if (GetFloorsweepingAffected() && !InPreVBIOSSetup && !bUseFspForFs)
    {
        // Restore VBIOS control of floorsweep in PostVBIOSSetup, so that
        // if mods doesn't override FSweep next time, the VBIOS owns it.
        SetVbiosControlsFloorsweep(true);
    }

    if (GetFloorsweepingChangesApplied())
    {
        return RC::OK;
    }

    // Read and parse FS args for all platforms though we don't really apply
    // those FS settings to SW modules
    CHECK_RC(ReadFloorsweepingArguments());

    if (IsFloorsweepingAllowed())
    {
        // floorsweeping contains register access protocols that cause non-RTL
        // and non-HW models severe indigestion, don't attempt HW floorsweeping
        // on the SW models
        //

        bool resetFloorsweeping    = GetResetFloorsweeping();
        bool floorsweepingAffected = GetFloorsweepingAffected();
        bool adjustFsArgs          = GetAdjustFloorsweepingArgs();
        bool addLwrFsMask          = GetAddLwrFsMask();
        bool adjustL2NocArgs       = GetAdjustL2NocFloorsweepingArgs();
        bool fixFsInitMasks        = GetFixFloorsweepingInitMasks();
        if (!resetFloorsweeping)
        {
            SaveFloorsweepingSettings();

            PrintFloorsweepingParams();
        }

        // if no floorsweeping settings are affected then there's nothing to do
        //
        if (floorsweepingAffected || resetFloorsweeping || fixFsInitMasks)
        {
            // disable FB only when it is floorswept (see bug 796974)
            if (m_FsFbParamPresent || resetFloorsweeping)
            {
                UINT32 data = m_pSub->RegRd32(LW_PMC_ENABLE);

                // Disable FB before FS programming. See bug 753721 for details.
                data = FLD_SET_DRF(_PMC, _ENABLE, _PFB, _DISABLED, data);
                m_pSub->RegWr32(LW_PMC_ENABLE, data);
            }

            if (m_IsPriRingInitRequired)
            {
                // Reset PGRAPH before changing the graphics floorsweeping registers
                GraphicsEnable(false);

                CHECK_RC(InitializePriRing_FirstStage(true));
            }

            // Request floorsweeping via the FSP if reqd. This will:
            // - Lock out the chip
            // - Lower the PLMs of the FS regs
            // - Set the SW enable bit
            // - Set persistent scratch to indicate MODS floorsweeping should take affect on the
            //   next boot cycle.
            if (bUseFspForFs)
            {
                RegHal &regs = m_pSub->Regs();            
                if (regs.IsSupported(MODS_FUSE_TPC_FLOORSWEEP_PRIV_LEVEL_MASK) &&            
                    regs.Test32(MODS_FUSE_TPC_FLOORSWEEP_PRIV_LEVEL_MASK_WRITE_PROTECTION_ALL_LEVELS_ENABLED) &&            
                    regs.IsSupported(MODS_FUSE_FLOORSWEEP_PRIV_LEVEL_MASK) &&            
                    regs.Test32(MODS_FUSE_FLOORSWEEP_PRIV_LEVEL_MASK_WRITE_PROTECTION_ALL_LEVELS_ENABLED))            
                {            
                    Printf(Tee::PriNormal, "Skipping FSP FS because PLMs are already lowered!\n");            
                }
                else
                {
                    Fsp *pFsp = m_pSub->GetFspImpl();
                    MASSERT(pFsp);
                    CHECK_RC(pFsp->SendFspMsg(Fsp::DataUseCase::Floorsweep,
                                              Fsp::DataFlag::OneShot,
                                              nullptr));
                    bSetModsControlsFs = false;
                }
            }

            // Reset floorsweeping settings if required
            if (resetFloorsweeping)
            {
                ResetFloorsweepingSettings();
                SaveFloorsweepingSettings();
                PrintFloorsweepingParams();
            }
            // Add the fused mask to the mask read from cmd line
            if (addLwrFsMask)
            {
                CHECK_RC(AddLwrFsMask());
            }
            // Complete any floorsweeping dependencies if requested
            StickyRC adjustArgsRc = RC::OK;
            if (adjustL2NocArgs)
            {
                adjustArgsRc = AdjustL2NocFloorsweepingArguments();
            }
            if (adjustFsArgs)
            {
                adjustArgsRc = AdjustDependentFloorsweepingArguments();
            }

            // Fix floorsweeping inconsistencies after ATE level floorsweeping is done
            if (fixFsInitMasks)
            {
                CHECK_RC(FixFloorsweepingInitMasks(&floorsweepingAffected));
                SetFloorsweepingAffected(GetFloorsweepingAffected() || floorsweepingAffected);
            }

            if (adjustArgsRc != RC::OK ||
                CheckFloorsweepingArguments() != RC::OK ||
                !IsFloorsweepingValid())
            {
                SetFloorsweepingAffected(false);

                Printf(Tee::PriError, "Invalid floorsweeping parameters\n");
                return RC::BAD_COMMAND_LINE_ARGUMENT;
            }

            if (floorsweepingAffected)
            {
                ApplyFloorsweepingSettings();
            }

            // With FSP we are going to issue a FLReset() so there is no need to do the pri-ring
            // initialization.
            if (!bUseFspForFs) 
            {
                if (!InPreVBIOSSetup &&
                    (Platform::GetSimulationMode() == Platform::Hardware) &&
                    m_IsPriRingInitRequired)
                {
                    // after changing FS settings, GF100 requires SW to run the pri-ring
                    // (re)-initialization sequence.  The VBIOS POST always inits the
                    // pri-ring, and since VBIOS POST is run on all platforms except
                    // Hardware, we only need to init the pri ring ourselves here in MODS
                    // when running on Hardware.
                    //
                    rc = InitializePriRing();
                    if (rc != RC::OK)
                    {
                        Printf(Tee::PriError, "Pri ring init FAILED! GPU needs to be reset!\n");
                        return rc;
                    }

                    //enable graphics again once fs registers have been applied
                    GraphicsEnable(true);
                }
            }
        }

        if (floorsweepingAffected && bSetModsControlsFs)
        {
            SetVbiosControlsFloorsweep(false);
        }
        else
        {
            SetVbiosControlsFloorsweep(true);
        }
        SetFloorsweepingChangesApplied(true);

        // Clear hardware configuration after floorsweeping has been applied
        m_pSub->ClearHwDevInfo();
    }

    CheckFbioCount();

    return rc;
}

//------------------------------------------------------------------------------
void FermiFs::SaveFloorsweepingSettingsFermi()
{
    // save all floorsweeping settings that could conceivably be changed by this
    // GF100GpuSubdevice (in ApplyFloorsweepingSettings)
    //

    // LW_PTOP_FS_CONTROL contains display, msdev, msvld, fbio_shift_override,
    //      and ce controls
    //
    m_SavedFsControl = m_pSub->RegRd32(LW_PTOP_FS_CONTROL);

    m_SavedGpcMask = m_pSub->RegRd32(LW_PTOP_FS_CONTROL_GPC);
    m_SavedFbpMask = m_pSub->RegRd32(LW_PTOP_FS_CONTROL_FBP);

    // save the per-gpc individual subunit enables
    //
    const UINT32 maxGpc = m_pSub->GetMaxGpcCount();
    MASSERT(maxGpc <= (UINT32)m_SavedGpcTpcMask.size());
    for (UINT32 i = 0; i < maxGpc; ++i)
        m_SavedGpcTpcMask[i] = m_pSub->RegRd32(LW_PTOP_FS_CONTROL_GPC_TPC(i));

    for (UINT32 i = 0; i < m_SavedGpcZlwllMask.size(); ++i)
        m_SavedGpcZlwllMask[i] = m_pSub->RegRd32(LW_PTOP_FS_CONTROL_GPC_ZLWLL(i));

    m_SavedFbio = m_pSub->RegRd32(LW_PTOP_FS_CONTROL_FBIO);
    m_SavedFbioShift = m_pSub->RegRd32(LW_PTOP_FS_CONTROL_FBIO_SHIFT);
}

//------------------------------------------------------------------------------
void FermiFs::SaveFloorsweepingSettings()
{
    SaveFloorsweepingSettingsFermi();
    m_SavedFbpa = m_pSub->RegRd32(LW_PTOP_FS_CONTROL_FBPA);
    m_SavedSpare = m_pSub->RegRd32(LW_PTOP_FS_CONTROL_SPARE);
}

//------------------------------------------------------------------------------
// Read the floorsweeping arguments from the command line.
//
RC FermiFs::ReadFloorsweepingArguments()
{
    RC rc = OK;

    // TODO: check for old styled here
    string FsArgs = GetFermiFsArgs();
    if (FsArgs.empty())
        return rc;

    ArgDatabase fsArgDatabase;
    CHECK_RC(PrepareArgDatabase(FsArgs, &fsArgDatabase));

    // common floorsweeping logic for all gk100+ chips
    CHECK_RC(ReadFloorsweepingArgumentsCommon(fsArgDatabase));

    // chip-specific floorsweeping arguments
    CHECK_RC(ReadFloorsweepingArgumentsImpl(fsArgDatabase));

    CheckIfPriRingInitIsRequired();

    // all parsing is done, check for unrecognized arguments
    if (fsArgDatabase.CheckForUnusedArgs(true))
        return RC::BAD_COMMAND_LINE_ARGUMENT;

    return rc;
}

// gf100 is in a transition from the old tesla-style "mods global" floorsweeping
// arguments to the "new-style" GpuSubdevice-specific floorsweeping arguments.
// ReadFloorsweepingArguments() reads the results of the old-style arguments
// (the Get*Mask() functions in base GpuSubdevice)), and combines this with
// the results of processing the new-style parameters from
// GpuSubdevice::GetFsArgs() (which for fermi is hooked up to the
// mods global JS cmdline parameter "-fermi_fs") into internal mask data
// members.
//
// The results of the processing are left in private mask variables and
// m_Fs<feature>ParamPresent private members, which
// ApplyFloorsweepingSettings() uses to write GPU registers and which
// RestoreFloorsweepingSettings() uses to determine which registers need to
// be restored at the end of the run
//
RC FermiFs::ReadFloorsweepingArgumentsCommon(ArgDatabase& fsArgDatabase)
{
    RC rc;

    // start by reading the values of the old-style cmdline FS parameters
    // into the mask variables, and marking them as present
    //
    m_FbMask = GetFbEnableMask();
    m_GpcMask = GetGpcEnableMask();
    GetTpcEnableMaskOnGpc(&m_GpcTpcMasks);
    GetZlwllEnableMaskOnGpc(&m_GpcZlwllMasks);
    bool oldFsArgsPresent = false;

    // get the new style floorsweeping parameter string
    //
    string FsArgs = GetFermiFsArgs();

    // compatibility mode: we accept either old-style FS parameters
    // (-fb_mask, -gpc_mask, -tpc_mask_on_gpc, -zlwll_mask_on_gpc), or
    // new-style (-fermi_fs <feature-string>).  Mixing them is not allowed.
    //
    m_FsFbParamPresent = m_FbMask != 0;
    m_FsGpcParamPresent = m_GpcMask != 0;
    oldFsArgsPresent = m_FsFbParamPresent || m_FsGpcParamPresent;
    for (UINT32 i = 0; i < m_GpcTpcMasks.size(); ++i)
    {
        if (m_GpcTpcMasks[i] != 0)
        {
            m_FsGpcTpcParamPresent[i] = true;
            oldFsArgsPresent = true;
        }
    }
    for (UINT32 i = 0; i < m_GpcZlwllMasks.size(); ++i)
    {
        if (m_GpcZlwllMasks[i] != 0)
        {
            m_FsGpcZlwllParamPresent[i] = true;
            oldFsArgsPresent = true;
        }
    }

    if (oldFsArgsPresent)
    {
        SetFloorsweepingAffected(true);
        PrintFloorsweepingParams();

        // there are old FS parameters on the cmdline.  If there are no new
        // FS parameters, we're done
        //
        if (FsArgs.empty())
            return rc;

        Printf(Tee::PriHigh, "GF100GpuSubdevice::ReadFloorsweepingArguments(): "
               "mixing old and new floorsweeping parameters not allowed\n");
        MASSERT(!"mixing old and new floorsweeping parameters not allowed");
    }

    // there are no old FS parameters.   If there are also no new FS parameters,
    // we're done
    //
    if (FsArgs.empty())
        return rc;

    // there are new style (-fermi_fs) FS parameters
    //
    ArgReader argReader(s_FloorSweepingParams, s_FsParamConstraints);
    if (!argReader.ParseArgs(&fsArgDatabase))
    {
        return RC::BAD_COMMAND_LINE_ARGUMENT;
    }

    // argReader is ready, now check for new-style FS parameters
    //

    // we need to know if the params are present because HW wants to be able to
    // specify a zero-valued feature value to the HW, so zero can no longer mean "not
    // specified"
    //
    m_FsDisplayParamPresent =  argReader.ParamPresent("display_enable") ||
                               argReader.ParamPresent("display_disable");

    m_FsMsdecParamPresent =  argReader.ParamPresent("msdec_enable") ||
                             argReader.ParamPresent("msdec_disable");

    m_FsMsvldParamPresent = argReader.ParamPresent("msvld_enable") ||
                            argReader.ParamPresent("msvld_disable");

    m_FsFbioShiftOverrideParamPresent =
                      argReader.ParamPresent("fbio_shift_override_enable") ||
                      argReader.ParamPresent("fbio_shift_override_disable");

    m_FsCeParamPresent = argReader.ParamPresent("ce_enable") ||
                         argReader.ParamPresent("ce_disable");

    m_FsGpcParamPresent =  argReader.ParamPresent("gpc_enable") ||
                           argReader.ParamPresent("gpc_disable");

    m_FsFbParamPresent = argReader.ParamPresent("fbp_enable") ||
                         argReader.ParamPresent("fbp_disable");

    m_FsFbioParamPresent = argReader.ParamPresent("fbio_enable") ||
                           argReader.ParamPresent("fbio_disable");

    m_FsFbioShiftParamPresent = argReader.ParamPresent("fbio_shift") > 0;

    // now get the mask values.  If the enable param is specified, use its
    // bitwise negation as the mask value.  If the disable param is specified,
    // use it directly as the mask.   This code works because _enable and _disable
    // are mutually exclusive for a given feature.
    //
    m_DisplayMask = ~ argReader.ParamUnsigned("display_enable", ~0U);
    m_DisplayMask = argReader.ParamUnsigned("display_disable", m_DisplayMask);

    m_MsdecMask = ~ argReader.ParamUnsigned("msdec_enable", ~0U);
    m_MsdecMask = argReader.ParamUnsigned("msdec_disable", m_MsdecMask);

    m_MsvldMask = ~ argReader.ParamUnsigned("msvld_enable", ~0U);
    m_MsvldMask = argReader.ParamUnsigned("msvld_disable", m_MsvldMask);

    m_FbioShiftOverrideMask = ~ argReader.ParamUnsigned(
                                                 "fbio_shift_override_enable", ~0U);
    m_FbioShiftOverrideMask = argReader.ParamUnsigned(
                                                 "fbio_shift_override_disable",
                                                 m_FbioShiftOverrideMask);
    // fbioShiftOverride is 1-enable, 0-disable
    //
    m_FbioShiftOverrideMask = ~m_FbioShiftOverrideMask;

    m_CeMask = ~ argReader.ParamUnsigned("ce_enable", ~0U);
    m_CeMask = argReader.ParamUnsigned("ce_disable", m_CeMask);

    m_GpcMask = ~ argReader.ParamUnsigned("gpc_enable", ~0U);
    m_GpcMask = argReader.ParamUnsigned("gpc_disable", m_GpcMask);

    m_FbMask = ~ argReader.ParamUnsigned("fbp_enable", ~0U);
    m_FbMask = argReader.ParamUnsigned("fbp_disable", m_FbMask);

    m_FbioMask = ~ argReader.ParamUnsigned("fbio_enable", ~0U);
    m_FbioMask = argReader.ParamUnsigned("fbio_disable", m_FbioMask);

    m_FbioShiftMask = argReader.ParamUnsigned("fbio_shift", 0);

    bool ilwert = false;
    const char *gpcTpcParamName = "gpc_tpc_disable";
    if (argReader.ParamPresent("gpc_tpc_enable"))
    {
        ilwert = true;
        gpcTpcParamName = "gpc_tpc_enable";
    }
    UINT32 num_GpcTpcArgs = argReader.ParamPresent(gpcTpcParamName);
    for (UINT32 i = 0; i < num_GpcTpcArgs; ++i)
    {
        UINT32 GpcNum = argReader.ParamNUnsigned(gpcTpcParamName, i, 0);
        UINT32 TpcMask = argReader.ParamNUnsigned(gpcTpcParamName, i, 1);
        TpcMask = ilwert ? ~TpcMask : TpcMask;

        if (GpcNum >= m_GpcTpcMasks.size())
        {
            Printf(Tee::PriHigh, "GF100GpuSubdevice::ReadFloorsweeping"
                   "Arguments: gpc # too large: %d\n", GpcNum);
            return RC::BAD_COMMAND_LINE_ARGUMENT;
        }

        m_GpcTpcMasks[GpcNum] = TpcMask;
        m_FsGpcTpcParamPresent[GpcNum] = true;
    }

    ilwert = false;
    const char *gpcZlwllParamName = "gpc_zlwll_disable";
    if (argReader.ParamPresent("gpc_zlwll_enable"))
    {
        ilwert = true;
        gpcZlwllParamName = "gpc_zlwll_enable";
    }
    UINT32 num_GpcZlwllArgs = argReader.ParamPresent(gpcZlwllParamName);
    for (UINT32 i = 0; i < num_GpcZlwllArgs; ++i)
    {
        UINT32 GpcNum = argReader.ParamNUnsigned(gpcZlwllParamName, i, 0);
        UINT32 ZlwllMask = argReader.ParamNUnsigned(gpcZlwllParamName, i, 1);
        ZlwllMask = ilwert ? ~ZlwllMask : ZlwllMask;

        if (GpcNum >= m_GpcZlwllMasks.size())
        {
            Printf(Tee::PriHigh, "GF100GpuSubdevice::ReadFloorsweeping"
                   "Arguments: gpc # too large: %d\n", GpcNum);
            return RC::BAD_COMMAND_LINE_ARGUMENT;
        }

        m_GpcZlwllMasks[GpcNum] = ZlwllMask;
        m_FsGpcZlwllParamPresent[GpcNum] = true;
    }

    // keep track of whether or not any floorsweeping settings change
    //
    SetFloorsweepingAffected((m_FsDisplayParamPresent ||
                               m_FsMsdecParamPresent ||
                               m_FsMsvldParamPresent ||
                               m_FsFbioShiftOverrideParamPresent ||
                               m_FsCeParamPresent || m_FsGpcParamPresent ||
                               m_FsFbParamPresent || m_FsFbioParamPresent ||
                               m_FsFbioShiftParamPresent));
    for (UINT32 i = 0; i < m_FsGpcTpcParamPresent.size(); ++i )
        if (m_FsGpcTpcParamPresent[i])
            SetFloorsweepingAffected(true);
    for (UINT32 i = 0; i < m_FsGpcZlwllParamPresent.size(); ++i )
        if (m_FsGpcZlwllParamPresent[i])
            SetFloorsweepingAffected(true);

    if (m_pSub->IsPrimary() && !m_pSub->IsGFWBootStarted() &&
                (m_FsFbParamPresent || m_FsFbioParamPresent
                || m_FsFbioShiftParamPresent || m_FsFbioShiftOverrideParamPresent))
    {
#ifdef SIM_BUILD
        // Allow floorsweeping on sim because it is legal on emulation.
        // We can't distinguish between silicon and emulation at this point.
        if (true)
#else
        if (GpuPtr()->GetForceRepost())
#endif
        {
            Printf(Tee::PriHigh,
              "****************************** WARNING *****************************\n"
              "* Attempting to floorsweep FBPs on what MODS has determined is the *\n"
              "*     primary GPU.  If the GPU really is primary this can have     *\n"
              "*       unpredictable results (hangs/crashes/loss of display)      *\n"
              "********************************************************************\n");
        }
        else
        {
            m_FsFbParamPresent = false;
            m_FsFbioParamPresent = false;
            m_FsFbioShiftParamPresent = false;
            m_FsFbioShiftOverrideParamPresent = false;
            Printf(Tee::PriHigh, "Unable to floorsweep FBPs on a primary GPU!\n");
            return RC::BAD_COMMAND_LINE_ARGUMENT;
        }
    }

    return rc;
}

//------------------------------------------------------------------------------
RC FermiFs::AdjustDependentFloorsweepingArguments()
{
    return RC::OK;
}

//------------------------------------------------------------------------------
RC FermiFs::AddLwrFsMask()
{
    return RC::OK;
}

//------------------------------------------------------------------------------
void FermiFs::CheckIfPriRingInitIsRequired()
{
    // GPC/FBP clusters need priv ring init + GR reset
    m_IsPriRingInitRequired = m_IsPriRingInitRequired ||
                           m_FsFbioShiftOverrideParamPresent ||
                           m_FsFbParamPresent ||
                           m_FsFbioParamPresent ||
                           m_FsFbioShiftParamPresent ||
                           m_FsFbpaParamPresent ||
                           m_FsGpcParamPresent;

    for (UINT32 i = 0; i < m_FsGpcTpcParamPresent.size(); ++i)
         m_IsPriRingInitRequired = m_IsPriRingInitRequired || m_FsGpcTpcParamPresent[i];

    for (UINT32 i = 0; i < m_FsGpcZlwllParamPresent.size(); ++i)
        m_IsPriRingInitRequired = m_IsPriRingInitRequired || m_FsGpcZlwllParamPresent[i];

    // If we add -fix_fs_init_masks in the cmd line, we are assuming Fs regs like FBP
    // regs would be affected.
    m_IsPriRingInitRequired = m_IsPriRingInitRequired || GetFixFloorsweepingInitMasks();
}

//------------------------------------------------------------------------------
UINT32 FermiFs::GetGpcEnableMask() const
{
    GpuSubdevice::NamedProperty *pGpcEnableMaskProp;
    UINT32         GpcEnableMask = 0;
    if ((OK != m_pSub->GetNamedProperty("GpcEnableMask",&pGpcEnableMaskProp)) ||
        (OK != pGpcEnableMaskProp->Get(&GpcEnableMask)))
    {
        Printf(Tee::PriDebug, "GpcEnableMask is not valid!!\n");
    }
    return GpcEnableMask;
}

//------------------------------------------------------------------------------
void FermiFs::GetTpcEnableMaskOnGpc(vector<UINT32> *pValues) const
{
    GpuSubdevice::NamedProperty *pTpcEnableMaskOnGpcProp;

    pValues->resize(s_MaxGpcsForFloorsweeping, 0);
    if ((OK != m_pSub->GetNamedProperty("TpcEnableMaskOnGpc", &pTpcEnableMaskOnGpcProp)) ||
        (OK != pTpcEnableMaskOnGpcProp->Get(pValues)))
    {
        Printf(Tee::PriDebug, "TpcEnableMaskOnGpc is not valid!!\n");
    }
}

//------------------------------------------------------------------------------
void FermiFs::GetZlwllEnableMaskOnGpc(vector<UINT32> *pValues) const
{
    GpuSubdevice::NamedProperty *pZlwllEnableMaskOnGpcProp;

    pValues->resize(s_MaxGpcsForFloorsweeping, 0);
    if ((OK != m_pSub->GetNamedProperty("ZlwllEnableMaskOnGpc", &pZlwllEnableMaskOnGpcProp)) ||
        (OK != pZlwllEnableMaskOnGpcProp->Get(pValues)))
    {
        Printf(Tee::PriDebug, "ZlwllEnableMaskOnGpc is not valid!!\n");
    }
}

//------------------------------------------------------------------------------
void FermiFs::PrintFloorsweepingParamsFermi() const
{
    Tee::Priority pri = GetFloorsweepingPrintPriority();

    Printf(pri, "GF100GpuSubdevice: FloorsweepingAffected=%d\n",
           GetFloorsweepingAffected());

    Printf(pri, "GF100GpuSubdevice: Floorsweeping parameters "
           "present on commandline: %s %s %s %s %s %s %s %s %s ",
           (m_FsDisplayParamPresent ? "display" : ""),
           (m_FsMsdecParamPresent ? "msdec" : ""),
           (m_FsMsvldParamPresent ? "msvld" : ""),
           (m_FsFbioShiftOverrideParamPresent ?
                                           "fbio_shift_override" : "" ),
           (m_FsCeParamPresent ? "ce" : ""),
           (m_FsGpcParamPresent ? "gpc" : ""),
           (m_FsFbParamPresent ? "fbp" : ""),
           (m_FsFbioParamPresent ? "fbio" : ""),
           (m_FsFbioShiftParamPresent ? "fbio_shift" : ""));

    for (UINT32 i = 0; i < m_FsGpcTpcParamPresent.size(); ++i)
        if (m_FsGpcTpcParamPresent[i])
            Printf(pri, "gpctpc[%d] ", i);
    for (UINT32 i = 0; i < m_FsGpcZlwllParamPresent.size(); ++i)
        if (m_FsGpcZlwllParamPresent[i])
            Printf(pri, "gpczlwll[%d] ", i);
    Printf(pri, "\n");

    Printf(pri, "GF100GpuSubdevice: Floorsweeping parameter "
           "mask values: display=0x%x msdec=0x%x msvld=0x%x fbio_shift_overr"
           "ide=0x%x ce=0x%x gpc=0x%x fb=0x%x fbio=0x%x fbio_shift=0x%x ",
           m_DisplayMask, m_MsdecMask, m_MsvldMask, m_FbioShiftOverrideMask,
           m_CeMask, m_GpcMask, m_FbMask, m_FbioMask, m_FbioShiftMask);
    for (UINT32 i = 0; i < m_GpcTpcMasks.size(); ++i)
        Printf(pri, "gpctpc[%d]=0x%x ", i, m_GpcTpcMasks[i]);
    for (UINT32 i = 0; i < m_GpcZlwllMasks.size(); ++i)
        Printf(pri, "gpczlwll[%d]=0x%x ", i, m_GpcZlwllMasks[i]);
    Printf(pri, "\n");
}

//------------------------------------------------------------------------------
void FermiFs::PrintFloorsweepingParams()
{
    PrintFloorsweepingParamsFermi();

    Tee::Priority pri = GetFloorsweepingPrintPriority();
    Printf(pri, "GF108PlusGpuSubdevice: Floorsweeping parameters "
           "present on commandline: %s %s %s ",
           (m_FsPcieLaneParamPresent ? "pcie_lane" : ""),
           (m_FsFbpaParamPresent ? "fbpa" : ""),
           (m_FsSpareParamPresent ? "spare" : ""));
    Printf(pri, "\n");
    Printf(pri, "GF108PlusGpuSubdevice: Floorsweeping parameter "
           "mask values: pcie_lane=0x%x fbpa=0x%x spare=0x%x ",
           m_PcieLaneMask, m_FbpaMask, m_SpareMask);
    Printf(pri, "\n");
}

//------------------------------------------------------------------------------
void FermiFs::PostRmShutdown(bool bInPreVBIOSSetup)
{
    if (GetFloorsweepingAffected() && GetRestoreFloorsweeping() && IsFloorsweepingAllowed())
    {
        if (m_IsPriRingInitRequired)
        {
            //Bug 1601153: reset graphics before restoring gpc/fbp fs clusters
            GraphicsEnable(false);

            InitializePriRing_FirstStage(false);
        }

        RestoreFloorsweepingSettings();

        if (m_IsPriRingInitRequired)
        {
            if (InitializePriRing() != OK)
            {
                Printf(Tee::PriHigh, "pri ring init FAILED! GPU needs to be "
                "reset!\n");
            }

            // Restart the priv ring so that priv ring related values will
            // be similar to POST/Reset values on the next MODS run.
            // Only do this if the GPU is *not* primary or if FB floorsweeping
            // arguments are present (in which case the GPU *better* be primary
            // or other problems may have arisen during the MODS run).  Also
            // skip this on RTL since it causes an assert to have the priv ring
            // in reset upon exiting the RTL simulator.
            //
            if (Platform::GetSimulationMode() != Platform::RTL)
            {
                if (!m_pSub->IsPrimary()      ||
                        m_FsFbParamPresent        ||
                        m_FsFbioParamPresent      ||
                        m_FsFbioShiftParamPresent ||
                        m_FsFbioShiftOverrideParamPresent)
                {
                    InitializePriRing_FirstStage(false);
                }
            }

            //deassert graphics reset afterwards
            GraphicsEnable(true);
        }
    }
}

//------------------------------------------------------------------------------
// before any floorsweeping control regs are touched, we must
// first toggle priv ring enable
//
// from https://wiki.lwpu.com/fermi/index.php/Fermi_PRIV#How_does_SW_initialize_the_priv_ring.3F:
//
// 1. Toggle the host PRIV ring enable bit to stop the priv ring:
//
// a. Write LW_PMC_ENABLE_PRIV_RING_DISABLED (0x00000200[5] = 0)
// b. Write LW_PMC_ENABLE_PRIV_RING_ENABLED  (0x00000200[5] = 1)
//
RC FermiFs::InitializePriRing_FirstStage(bool bInSetup)
{
    // Adding 10us delay to avoid race condition
    Platform::SleepUS(10);

    RegHal &regs = m_pSub->Regs();
    if (regs.IsSupported(MODS_PMC_ENABLE_PRIV_RING))
    {
        regs.Write32(MODS_PMC_ENABLE_PRIV_RING_DISABLED);
        regs.Write32(MODS_PMC_ENABLE_PRIV_RING_ENABLED);
    }

    // Bug 731411: for RTL perf, avoid unneeded read loop at startup
    if (!(Platform::GetSimulationMode() == Platform::RTL && bInSetup))
    {
        // wait for the toggle to make it around the ring.
        UINT32 data;
        for (UINT32 index = 0; index < 20; index++)
        {
            data = regs.Read32(MODS_PPRIV_MASTER_RING_COMMAND);
        }
        (void)(data);
    }

    return RC::OK;
}

//------------------------------------------------------------------------------
// InitializePriRing()
//
// this sequence was taken from this wiki page:
// https://wiki.lwpu.com/fermi/index.php/Fermi_PRIV#How_does_SW_initialize_the_priv_ring.3F
// RestoreFloorsweepingSettings
// on 2009-02-19
//
RC FermiFs::InitializePriRing()
{
    RC rc;
    RegHal &regs = m_pSub->Regs();

    Printf(Tee::PriDebug, "GF100GpuSubdevice::InitializePriRing: running...\n");

    // steps 1 and 2 are implemented in InitializePriRing_FirstStage()
    // and called from ApplyFloorsweepingSettings and
    // RestoreFloorsweepingSettings
    //

    // 3. Configure the pri ring to drop transactions if the ring isn't started:
    // write LW_PPRIV_SYS_PRIV_DECODE_CONFIG_RING_DROP_ON_RING_NOT_STARTED
    // (0x122204[2:0]=0x1).
    //
    regs.Write32(MODS_PPRIV_SYS_PRIV_DECODE_CONFIG_RING_DROP_ON_RING_NOT_STARTED);

    // 4. Turn off reset to the ring. Write
    // LW_PPRIV_MASTER_RING_GLOBAL_CTL_RING_RESET_DEASSERTED to turn of the
    // reset being sent out on the ring (0x121c60[0]=0).
    //
    regs.Write32(MODS_PPRIV_MASTER_RING_GLOBAL_CTL_RING_RESET_DEASSERTED);

    // 5. Tell the pri ring masterstation to enumerate the number of stations on
    // the ring. This is how software can determine the configuration of the
    // chip; ringstations in floorswept chiplets will not respond to the
    // enumeration command. To enumerate the ring, write the following to the
    // LW_PPRIV_MASTER_RING_COMMAND register :
    //
    // Set the broadcast group field to all stations
    // (LW_PPRIV_MASTER_RING_COMMAND_CMD_ENUMERATE_STATIONS_BC_GRP ==
    //     LW_PPRIV_MASTER_RING_COMMAND_CMD_ENUMERATE_STATIONS_BC_GRP_ALL)
    //     (0x121c4c[8:6]=0) .
    //
    // Set the command field to enumerate (LW_PPRIV_MASTER_RING_COMMAND_CMD
    //       ==LW_PPRIV_MASTER_RING_COMMAND_CMD_ENUMERATE_STATIONS)
    //          (0x121c4c[5:0]=3).
    //
    UINT32 val = regs.Read32(MODS_PPRIV_MASTER_RING_COMMAND);
    regs.SetField(&val, MODS_PPRIV_MASTER_RING_COMMAND_CMD_ENUMERATE_STATIONS);
    regs.SetField(&val, MODS_PPRIV_MASTER_RING_COMMAND_CMD_ENUMERATE_STATIONS_BC_GRP_ALL);
    regs.Write32(MODS_PPRIV_MASTER_RING_COMMAND, val);

    // 6. Poll on the LW_PPRIV_MASTER_RING_COMMAND register returning to all
    // zeros before writing the next command (0x121c4c).
    //
    Printf(Tee::PriDebug,
           "GF100GpuSubdevice::InitializePriRing: polling "
           "LW_PPRIV_MASTER_RING_COMMAND == 0...");
    CHECK_RC(POLLWRAP_HW(PollMasterRingCommand, &regs, 30 * 1000));
    Printf(Tee::PriDebug, "...done\n" );

    // 7. Write the LW_PPRIV_MASTER_RING_COMMAND_DATA reg with the
    // START_RING_SEED value --any nonzero data pattern is fine here. VBIOS
    // uses 0x503B4B49 (0x121c48).
    //
    // The instructions say "any nonzero data pattern is fine", so write
    // a memorable pattern:
    //
    regs.Write32(MODS_PPRIV_MASTER_RING_COMMAND_DATA, 0xc0ffee);

    // 8. Write the command register with the start command
    // (LW_PPRIV_MASTER_RING_COMMAND_CMD ==
    //        LW_PPRIV_MASTER_RING_COMMAND_CMD_START_RING )
    //        (0x121c4c[5:0]=1).
    //
    regs.Write32(MODS_PPRIV_MASTER_RING_COMMAND_CMD_START_RING);

    // 9. Configure the priv ring to wait for startup to complete before
    // accepting transactions from fecs. This is quicker than polling:
    // Write LW_PPRIV_SYS_PRIV_DECODE_CONFIG_RING_WAIT_FOR_RING_START_COMPLETE
    // (0x122204[2:0]=0x2)
    //
    regs.Write32(MODS_PPRIV_SYS_PRIV_DECODE_CONFIG_RING_WAIT_FOR_RING_START_COMPLETE);

    // 10. (Optional - not in VBIOS) If the LW_PPRIV_MASTER_RING_START_RESULTS
    // register contains the value
    // LW_PPRIV_MASTER_RING_START_RESULTS_CONNECTIVITY_PASS, then the ring
    // started successfully. (0x121c50[0]==1)
    //
    // Since the instructions say this step is optional, MODS will skip it
    //

    Printf(Tee::PriDebug, "GF100GpuSubdevice::InitializePriRing() finished\n");

    return rc;
}

//------------------------------------------------------------------------------
void FermiFs::ApplyFloorsweepingSettings()
{
    ApplyFloorsweepingSettingsFermi();

    if (m_FsPcieLaneParamPresent)
    {
        UINT32 fsControl = m_pSub->RegRd32(LW_PTOP_FS_CONTROL);

        fsControl = FLD_SET_DRF_NUM(_PTOP,_FS_CONTROL,_PCIE_LANE,
                                    m_PcieLaneMask, fsControl);

        m_pSub->RegWr32(LW_PTOP_FS_CONTROL, fsControl);
    }

    if (m_FsFbpaParamPresent)
        m_pSub->RegWr32(LW_PTOP_FS_CONTROL_FBPA, m_FbpaMask);

    if (m_FsSpareParamPresent)
        m_pSub->RegWr32(LW_PTOP_FS_CONTROL_SPARE, m_SpareMask);

}

//------------------------------------------------------------------------------
void FermiFs::ApplyFloorsweepingSettingsFermi()
{
    // LW_PTOP_FS_CONTROL holds multiple masks.  For each mask
    // that has been specified on the cmdline, write it into
    // the proper field of the register
    //
    if (m_FsDisplayParamPresent || m_FsMsdecParamPresent ||
        m_FsMsvldParamPresent   || m_FsFbioShiftOverrideParamPresent ||
        m_FsCeParamPresent)
    {
        // since we may be writing only a subset of the register, we must make
        // a read-modify-write access
        //
        UINT32 fsControl = m_SavedFsControl;

        if (m_FsDisplayParamPresent)
            fsControl = FLD_SET_DRF_NUM(_PTOP,_FS_CONTROL,_DISPLAY,
                                        m_DisplayMask, fsControl);
        if (m_FsMsdecParamPresent)
            fsControl = FLD_SET_DRF_NUM(_PTOP,_FS_CONTROL,_MSDEC,
                                        m_MsdecMask, fsControl);
        if (m_FsMsvldParamPresent)
            fsControl = FLD_SET_DRF_NUM(_PTOP,_FS_CONTROL,_MSVLD_SEC,
                                        m_MsvldMask, fsControl);
        if (m_FsFbioShiftOverrideParamPresent)
            fsControl = FLD_SET_DRF_NUM(_PTOP,_FS_CONTROL,_FBIO_SHIFT_OVERRIDE,
                                        m_FbioShiftOverrideMask, fsControl);
        if (m_FsCeParamPresent)
            fsControl = FLD_SET_DRF_NUM(_PTOP,_FS_CONTROL,_CE,
                                        m_CeMask, fsControl);
        m_pSub->RegWr32(LW_PTOP_FS_CONTROL, fsControl);
    }

    if (m_FsGpcParamPresent)
        m_pSub->RegWr32(LW_PTOP_FS_CONTROL_GPC, m_GpcMask);

    if (m_FsFbParamPresent)
        m_pSub->RegWr32(LW_PTOP_FS_CONTROL_FBP, m_FbMask);

    if (m_FsFbioParamPresent)
        m_pSub->RegWr32(LW_PTOP_FS_CONTROL_FBIO, m_FbioMask);

    if (m_FsFbioShiftParamPresent)
        m_pSub->RegWr32(LW_PTOP_FS_CONTROL_FBIO_SHIFT, m_FbioShiftMask);

    // tpc enable bits in gpcs are arranged across
    // LW_PTOP_FS_CONTROL_GPC_TPC__SIZE_1 registers
    // each register contains LW_PTOP_FS_CONTROL_GPC_TPC_IDX___SIZE_1 gpc-tpc
    // masks.
    // each mask contains LW_PTOP_FS_CONTROL_GPC_TPC_IDX___SIZE_2 bits
    // each bit corresponds to a particular tpc within a particular gpc
    //
    UINT32 gpcGroups = LW_PTOP_FS_CONTROL_GPC_TPC__SIZE_1;
    UINT32 gpcsPerGroup = LW_PTOP_FS_STATUS_GPC_TPC_IDX___SIZE_1;
    for (UINT32 gpcGroup = 0; gpcGroup < gpcGroups; ++gpcGroup)
    {
        for (UINT32 gpcInGroup = 0; gpcInGroup < gpcsPerGroup; ++gpcInGroup)
        {
            UINT32 gpcNum = (gpcGroup * gpcsPerGroup) + gpcInGroup;

            // can this gpcNum possibly exist on this GPU?
            // if not, skip it
            //
            if (gpcNum >= m_GpcTpcMasks.size())
                continue;

            // if user hasn't specified a tpc mask for this gpc then skip it
            //
            if (m_FsGpcTpcParamPresent[gpcNum] == false)
                continue;

            // we have a mask to apply for this gpc.  Start with the
            // current value for this whole reg from the HW, then replace every
            // tpc bit in gpcNum's field from the user specified mask
            //
            UINT32 regAddr = LW_PTOP_FS_CONTROL_GPC_TPC(gpcGroup);
            UINT32 mask = m_pSub->RegRd32(regAddr);
            for (UINT32 bitNum = 0;
                 bitNum < LW_PTOP_FS_CONTROL_GPC_TPC_IDX___SIZE_2;
                 ++bitNum)
            {
                UINT32 bitVal = (m_GpcTpcMasks[gpcNum] >> bitNum) & 1;

                // set bit LW_PTOP_FS_CONTROL_GPC_TPC_IDX(gpcInGroup, bitNum)
                // to bitVal
                //
                mask = FLD_SET_DRF_NUM(_PTOP, _FS_CONTROL,
                                       _GPC_TPC_IDX(gpcInGroup, bitNum),
                                       bitVal, mask);
            }

            // write out the new tpc settings for this gpc
            //
            m_pSub->RegWr32(regAddr, mask);
        }
    }

    // zlwll enable bits in gpcs are arranged across
    // LW_PTOP_FS_CONTROL_GPC_ZLWLL__SIZE_1 registers
    // each register contains LW_PTOP_FS_CONTROL_GPC_ZLWLL_IDX___SIZE_1
    // gpc-zlwll masks.
    // each mask contains LW_PTOP_FS_CONTROL_GPC_ZLWLL_IDX___SIZE_2 bits
    // each bit corresponds to a particular zlwll within a particular gpc
    //
    gpcGroups =  LW_PTOP_FS_CONTROL_GPC_ZLWLL__SIZE_1;
    gpcsPerGroup = LW_PTOP_FS_STATUS_GPC_ZLWLL_IDX___SIZE_1;
    for (UINT32 gpcGroup = 0; gpcGroup < gpcGroups; ++gpcGroup)
    {
        for (UINT32 gpcInGroup = 0; gpcInGroup < gpcsPerGroup; ++gpcInGroup)
        {
            UINT32 gpcNum = (gpcGroup * gpcsPerGroup) + gpcInGroup;

            // can this gpcNum possibly exist on this GPU?
            // if not, skip it
            //
            if (gpcNum >= m_GpcZlwllMasks.size())
                continue;

            // if user hasn't specified a zlwll mask for this gpc then skip it
            //
            if (m_FsGpcZlwllParamPresent[gpcNum] == false)
                continue;

            // we have a mask to apply for this gpc.  Start with the
            // current value for this whole reg from the HW, then replace every
            // zlwll bit in gpcNum's field from the user specified mask
            //
            UINT32 regAddr = LW_PTOP_FS_CONTROL_GPC_ZLWLL(gpcGroup);
            UINT32 mask = m_pSub->RegRd32(regAddr);
            for (UINT32 bitNum = 0;
                 bitNum < LW_PTOP_FS_CONTROL_GPC_ZLWLL_IDX___SIZE_2;
                 ++bitNum)
            {
                UINT32 bitVal = (m_GpcZlwllMasks[gpcNum] >> bitNum) & 1;

                // set bit LW_PTOP_FS_CONTROL_GPC_ZLWLL_IDX(gpcInGroup, bitNum)
                // to bitVal
                //
                mask = FLD_SET_DRF_NUM(_PTOP, _FS_CONTROL,
                                       _GPC_ZLWLL_IDX(gpcInGroup, bitNum),
                                       bitVal, mask);
            }

            // write out the new zlwll settings for this gpc
            //
            m_pSub->RegWr32(regAddr, mask);
        }
    }
}

//------------------------------------------------------------------------------
//! \brief Check whether the floorsweeping maks passed by the user are
//! compatible with the chip.
//!
RC FermiFs::CheckFloorsweepingMask
(
    UINT32 Allowed,
    UINT32 Requested,
    UINT32 Max,
    const string &Region
) const
{
    UINT32 bitNum  = 0;
    UINT32 maxMask = BIT(Max) - 1;
    for (bitNum = 0; bitNum < Max; bitNum++)
    {
        if ( (Allowed & (1<<bitNum)) && !(Requested & (1<<bitNum)) )
        {
            Printf(Tee::PriError, "Invalid floorsweeping configuration requested\n"
                                  "%s Max count:              %u\n"
                                  "%s Minimum disable mask:   0x%08x\n"
                                  "%s Requested disable mask: 0x%08x\n"
                                  "Attempting to enable floorswept %s %u\n",
                                  Region.c_str(), Max,
                                  Region.c_str(), Allowed,
                                  Region.c_str(), Requested & maxMask,
                                  Region.c_str(), bitNum);
            return RC::BAD_COMMAND_LINE_ARGUMENT;
        }
    }
    return RC::OK;
}

//------------------------------------------------------------------------------
//! \brief Check whether the floorsweeping arguments passed by the user are
//! compatible with each other.
//!
//! This method performs the following checks:
//! a. If all TPCs within a GPC are disabled, the GPC must be disabled (else error).
//! b. If all ZLWLLs within a GPC are disabled, the GPC must be disabled (else error).
//! c. Within a GPC, num ZLWLL >= num TPC (else error).
//!
//! \return OK if there are no errors, else RC::BAD_COMMAND_LINE_ARGUMENT
RC FermiFs::CheckFloorsweepingArguments()
{
    // fmodel's POR configurations (num_tpcs, num_fpbs) can violate this
    // FS checking logic for some tests, thus we need to bypass the check.
    // see bug 830265
    if (IsFloorsweepingAllowed())
    {
        UINT32 maxGpcCount = m_pSub->GetMaxGpcCount();
        UINT32 maxTpcCount = m_pSub->GetMaxTpcCount();
        UINT32 maxTpcDisabledMask = (1 << maxTpcCount) - 1;
        if (maxGpcCount > m_GpcTpcMasks.size())
        {
            Printf(Tee::PriError, "GF100GpuSubdevice::CheckFloorsweepingArguments() "
                   "m_GpcTpcMasks[] = %ld maxGpcCount = %d. "
                   "Check the value of s_MaxGpcsForFloorsweeping!\n",
                   m_GpcTpcMasks.size(), maxGpcCount);
            return RC::SOFTWARE_ERROR;
        }
        for (UINT32 i = 0; i < maxGpcCount; i ++)
        {
            UINT32 lwrGpcTpcMask = m_GpcTpcMasks[i] & maxTpcDisabledMask;
            UINT32 lwrGpcZlwllMask = m_GpcZlwllMasks[i] & maxTpcDisabledMask;

            // If neither of the TPC and ZLWLL floorsweeping parameters are present,
            // continue.
            if ((!m_FsGpcTpcParamPresent[i]) && (!m_FsGpcZlwllParamPresent[i]))
            {
                continue;
            }

            // If ALL the TPCs/ZLWLLs on this GPC are floorswept, check if this GPC
            // is also floorswept.
            if ((lwrGpcTpcMask == maxTpcDisabledMask) ||
                (lwrGpcZlwllMask == maxTpcDisabledMask))
            {
                // If this GPC not floorswept, return error.
                if (!(m_GpcMask & (1 << i)))
                {
                    Printf(Tee::PriHigh, "GF100GpuSubdevice::"
                    "CheckFloorsweepingArguments(): Cannot have the GPC[%d] enabled if the"
                    " TPCs or ZLWLLs in the same GPC are disabled.\n", i);
                    return RC::BAD_COMMAND_LINE_ARGUMENT;
                }
            }

            UINT32 numLwrGpcTpcs = maxTpcCount;
            UINT32 numLwrGpcZlwlls = maxTpcCount; // Max zlwlls == Max tpcs

            // If the TPC floorsweeping parameter for this GPC is not passed,
            // i.e., lwrGpcTpcMask = 0, all the TPCs are enabled.
            numLwrGpcTpcs -= Utility::CountBits(lwrGpcTpcMask);

            // If the ZLWLL floorsweeping parameter for this GPC is not passed,
            // i.e., lwrGpcZlwllMask = 0, all the ZLWLLs are enabled.
            numLwrGpcZlwlls -= Utility::CountBits(lwrGpcZlwllMask);

            if (numLwrGpcZlwlls < numLwrGpcTpcs)
            {
                Printf(Tee::PriHigh, "GF100GpuSubdevice::"
                "CheckFloorsweepingArguments(): Cannot have the number of ZLWLLs"
                " enabled in a GPC[%d] less than the number of TPCs enabled in the same GPC.\n", i);
                return RC::BAD_COMMAND_LINE_ARGUMENT;
            }
        }
    }
    else
    {
        //TODO: appropriate checking for A/Fmodel goes here
    }

    return OK;
}

//------------------------------------------------------------------------------
void FermiFs::RestoreFloorsweepingSettings()
{
    RestoreFloorsweepingSettingsFermi();

    if (m_FsPcieLaneParamPresent)
        m_pSub->RegWr32(LW_PTOP_FS_CONTROL, GetSavedFsControl());
    if (m_FsFbpaParamPresent)
        m_pSub->RegWr32(LW_PTOP_FS_CONTROL_FBPA, m_SavedFbpa);
    if (m_FsSpareParamPresent)
        m_pSub->RegWr32(LW_PTOP_FS_CONTROL_SPARE, m_SavedSpare);
}

//------------------------------------------------------------------------------
// RestoreFloorsweepingSettings()
//
// restores floorsweeping registers that have been changed
//
// this function is only called if there definitely is at least one
// floorsweeping register that has changed
//
void FermiFs::RestoreFloorsweepingSettingsFermi()
{
    Printf(Tee::PriDebug, "GF100GpuSubdevice::RestoreFloorsweepingSettings: "
           " checking and restoring startup settings\n");

    if (m_FsDisplayParamPresent || m_FsMsdecParamPresent ||
        m_FsMsvldParamPresent || m_FsFbioShiftOverrideParamPresent ||
        m_FsCeParamPresent)
    {
        m_pSub->RegWr32(LW_PTOP_FS_CONTROL, m_SavedFsControl);
    }

    if (m_FsGpcParamPresent)
        m_pSub->RegWr32(LW_PTOP_FS_CONTROL_GPC, m_SavedGpcMask);

    if (m_FsFbParamPresent)
        m_pSub->RegWr32(LW_PTOP_FS_CONTROL_FBP, m_SavedFbpMask);

    // figure out which GpcTpc regs need to be restored (if any)...
    // for each affected GPC, figure out which register index holds that
    // gpc
    //
    set<UINT32> GpcTpcToRestore;
    for (UINT32 i = 0; i < m_FsGpcTpcParamPresent.size(); ++i)
    {
        if (m_FsGpcTpcParamPresent[i])
            GpcTpcToRestore.insert(i / LW_PTOP_FS_CONTROL_GPC_TPC_IDX___SIZE_1);
    }

    // ...and restore any which need to be restored
    //
    set<UINT32>::const_iterator it = GpcTpcToRestore.begin();
    for ( ; it != GpcTpcToRestore.end(); ++it)
    {
        UINT32 regNum = *it;
        m_pSub->RegWr32(LW_PTOP_FS_CONTROL_GPC_TPC(regNum), m_SavedGpcTpcMask[regNum]);
    }

    // figure out which GpcZlwll regs need to be restored (if any)...
    // for each affected GPC, figure out which register index holds that
    // gpc
    //
    set<UINT32> GpcZlwllToRestore;
    for (UINT32 i = 0; i < m_FsGpcZlwllParamPresent.size(); ++i)
    {
        if (m_FsGpcZlwllParamPresent[i])
            GpcZlwllToRestore.insert(i / LW_PTOP_FS_CONTROL_GPC_ZLWLL_IDX___SIZE_1);
    }

    // ...and restore any which need to be restored
    //
    for (it = GpcZlwllToRestore.begin(); it != GpcZlwllToRestore.end(); ++it)
    {
        UINT32 regNum = *it;
        m_pSub->RegWr32(LW_PTOP_FS_CONTROL_GPC_ZLWLL(regNum), m_SavedGpcZlwllMask[regNum]);
    }

    if (m_FsFbioParamPresent)
        RestoreFbio();

    if (m_FsFbioShiftParamPresent)
        m_pSub->RegWr32(LW_PTOP_FS_CONTROL_FBIO_SHIFT, m_SavedFbioShift);
}

//------------------------------------------------------------------------------
void FermiFs::ResetFloorsweepingSettings()
{
    RegHal &regs = m_pSub->Regs();

    ResetFloorsweepingSettingsFermi();

    UINT32 display = regs.Read32(MODS_FUSE_OPT_DISPLAY_DISABLE);
    UINT32 msdec = regs.Read32(MODS_FUSE_OPT_MSDEC_DISABLE);
    UINT32 msvld = regs.Read32(MODS_FUSE_OPT_MSVLD_SEC_DISABLE);
    UINT32 fbioShiftOverride = (regs.Read32(MODS_FUSE_OPT_FBIO_SHIFT)==0) ? 0 : 1;
    UINT32 ce = regs.Read32(MODS_FUSE_OPT_CE_DISABLE);
    UINT32 fscontrol = m_pSub->RegRd32(LW_PTOP_FS_CONTROL);
    fscontrol = fscontrol | DRF_NUM(_PTOP, _FS_CONTROL, _DISPLAY,   display)
                          | DRF_NUM(_PTOP, _FS_CONTROL, _MSDEC,     msdec)
                          | DRF_NUM(_PTOP, _FS_CONTROL, _MSVLD_SEC, msvld)
                          | DRF_NUM(_PTOP, _FS_CONTROL, _FBIO_SHIFT_OVERRIDE,
                                                        fbioShiftOverride)
                          | DRF_NUM(_PTOP, _FS_CONTROL, _CE, ce);
    m_pSub->RegWr32(LW_PTOP_FS_CONTROL, fscontrol);

    if (regs.IsSupported(MODS_FUSE_OPT_FBPA_DISABLE))
    {
        UINT32 fbpa = regs.Read32(MODS_FUSE_OPT_FBPA_DISABLE);
        m_pSub->RegWr32(LW_PTOP_FS_CONTROL_FBPA, fbpa);
    }

    UINT32 spare = regs.Read32(MODS_FUSE_OPT_SPARE_FS);
    m_pSub->RegWr32(LW_PTOP_FS_CONTROL_SPARE, spare);
}

//------------------------------------------------------------------------------
void FermiFs::ResetFloorsweepingSettingsFermi()
{
    Printf(Tee::PriDebug, "GF100GpuSubdevice::ResetFloorsweepingSettings: "
           " reading and resetting settings\n");

    RegHal &regs = m_pSub->Regs();

    UINT32 gpc = regs.Read32(MODS_FUSE_OPT_GPC_DISABLE);
    m_pSub->RegWr32(LW_PTOP_FS_CONTROL_GPC, gpc);

    UINT32 fbp = regs.Read32(MODS_FUSE_OPT_FBP_DISABLE);
    m_pSub->RegWr32(LW_PTOP_FS_CONTROL_FBP, fbp);

    UINT32 maxGpcs = m_pSub->GetMaxGpcCount();
    UINT32 maxTpcs = m_pSub->GetMaxTpcCount();
    UINT32 mask = (1 << maxTpcs) - 1;
    UINT32 gpcTpc = regs.Read32(MODS_FUSE_OPT_TPC_DISABLE);
    UINT32 gpcZLwll = regs.Read32(MODS_FUSE_OPT_ZLWLL_DISABLE);
    for (UINT32 gpcIndex = 0; gpcIndex < maxGpcs; gpcIndex++)
    {
        UINT32 tpc = (gpcTpc & mask) >> (gpcIndex * maxTpcs);
        m_pSub->RegWr32(LW_PTOP_FS_CONTROL_GPC_TPC(gpcIndex), tpc);

        UINT32 zlwll = (gpcZLwll & mask) >> (gpcIndex * maxTpcs);
        m_pSub->RegWr32(LW_PTOP_FS_CONTROL_GPC_ZLWLL(gpcIndex), zlwll);

        mask <<= maxTpcs;
    }

    UINT32 fbio = regs.Read32(MODS_FUSE_OPT_FBIO_DISABLE);
    m_pSub->RegWr32(LW_PTOP_FS_CONTROL_FBIO, fbio);

    UINT32 fbioShift = regs.Read32(MODS_FUSE_OPT_FBIO_SHIFT);
    m_pSub->RegWr32(LW_PTOP_FS_CONTROL_FBIO_SHIFT, fbioShift);
}

//------------------------------------------------------------------------------
void FermiFs::RestoreFbio() const
{
    m_pSub->RegWr32(LW_PTOP_FS_CONTROL_FBIO, m_SavedFbio);
}

//------------------------------------------------------------------------------
void FermiFs::CheckFbioCount()
{
    // The number of FBP and FBIO units must be identical.  If they
    // differ in HW, use "lwflash --nopreserve <file>" to erase the
    // illegal floorsweeping.  If they differ in emu, use -fermi_fs as
    // a WAR until they fix the emulator.
    //
    if (Utility::CountBits(FbpMaskImpl()) != Utility::CountBits(FbioMaskImpl()))
    {
        Printf(Tee::PriHigh, "Bad floorsweeping: num FBIOs != num FBPs\n");
        Printf(Tee::PriHigh, "    FBIO enable mask = 0x%02x\n", FbioMaskImpl());
        Printf(Tee::PriHigh, "    FBP  enable mask = 0x%02x\n", FbpMaskImpl());
        if (Platform::GetSimulationMode() != Platform::Hardware)
        {
            Printf(Tee::PriHigh, "    Make sure you are using the correct "
                                 "\"-chipargs -chipPOR=xxx\" argument\n");
        }
        MASSERT(!"Bad floorsweeping: num FBIOs != num FBPs\n");
    }
}

//------------------------------------------------------------------------------
RC FermiFs::ReadFloorsweepingArgumentsGF108Plus(ArgDatabase& fsArgDatabase)
{
    ArgReader argReader(s_FloorSweepingParamsGF108Plus, s_FsParamConstraintsGF108Plus);
    if (!argReader.ParseArgs(&fsArgDatabase))
    {
        return RC::BAD_COMMAND_LINE_ARGUMENT;
    }

    m_FsPcieLaneParamPresent =
            argReader.ParamPresent("pcie_lane_enable") ||
            argReader.ParamPresent("pcie_lane_disable");

    m_FsFbpaParamPresent = argReader.ParamPresent("fbpa_enable") ||
                           argReader.ParamPresent("fbpa_disable");

    m_FsSpareParamPresent = argReader.ParamPresent("spare_enable") ||
                            argReader.ParamPresent("spare_disable");

    m_PcieLaneMask = ~ argReader.ParamUnsigned("pcie_lane_enable", 0);
    m_PcieLaneMask = argReader.ParamUnsigned("pcie_lane_disable", m_PcieLaneMask);

    m_FbpaMask = ~ argReader.ParamUnsigned("fbpa_enable", ~0U);
    m_FbpaMask = argReader.ParamUnsigned("fbpa_disable", m_FbpaMask);

    m_SpareMask = ~ argReader.ParamUnsigned("spare_enable", 0);
    m_SpareMask = argReader.ParamUnsigned("spare_disable", m_SpareMask);

    SetFloorsweepingAffected(GetFloorsweepingAffected() ||
                               m_FsPcieLaneParamPresent ||
                               m_FsFbpaParamPresent ||
                               m_FsSpareParamPresent);

    if (m_pSub->IsPrimary() && !m_pSub->IsGFWBootStarted() && m_FsFbpaParamPresent)
    {
#ifdef SIM_BUILD
        // Allow floorsweeping on sim because it is legal on emulation.
        // We can't distinguish between silicon and emulation at this point.
        if (true)
#else
        if (GpuPtr()->GetForceRepost())
#endif
        {
            Printf(Tee::PriHigh,
              "****************************** WARNING *****************************\n"
              "* Attempting to floorsweep FBPs on what MODS has determined is the *\n"
              "*     primary GPU.  If the GPU really is primary this can have     *\n"
              "*       unpredictable results (hangs/crashes/loss of display)      *\n"
              "********************************************************************\n");
        }
        else
        {
            m_FsFbpaParamPresent = false;
            Printf(Tee::PriHigh, "Unable to floorsweep FBPs on a primary GPU!\n");
            return RC::BAD_COMMAND_LINE_ARGUMENT;
        }
    }

    return OK;
}

//------------------------------------------------------------------------------
/* virtual */ RC FermiFs::GetFloorsweepingMasksFermi
(
    FloorsweepingMasks* pFsMasks
) const
{
    MASSERT(pFsMasks != 0);
    pFsMasks->clear();

    pFsMasks->insert(pair<string, UINT32>("fscontrol_mask", FscontrolMask()));

    pFsMasks->insert(pair<string, UINT32>("fb_mask", FbMaskImpl()));

    pFsMasks->insert(pair<string, UINT32>("fbio_mask", FbioMaskImpl()));

    pFsMasks->insert(pair<string, UINT32>("fbioshift_mask", FbioshiftMaskImpl()));

    UINT32 gpcMask = GpcMask();
    pFsMasks->insert(pair<string, UINT32>("gpc_mask", gpcMask));

    UINT32 gpcNum = 0;
    while (gpcMask != 0)
    {
        if (gpcMask & 1)
        {
            string fsmaskName = Utility::StrPrintf("gpctpc[%d]_mask", gpcNum);
            pFsMasks->insert(pair<string, UINT32>(fsmaskName, TpcMask(gpcNum)));

            fsmaskName = Utility::StrPrintf("gpczlwll[%d]_mask", gpcNum);
            pFsMasks->insert(pair<string, UINT32>(fsmaskName, ZlwllMask(gpcNum)));
        }

        gpcNum ++;
        gpcMask >>= 1;
    }

    return OK;
}

//------------------------------------------------------------------------------
/* virtual */ RC FermiFs::GetFloorsweepingMasks
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

    CHECK_RC(GetFloorsweepingMasksFermi(pFsMasks));

    pFsMasks->insert(pair<string, UINT32>("fbpa_mask", FbpaMaskImpl()));

    pFsMasks->insert(pair<string, UINT32>("spare_mask", SpareMask()));

    return rc;
}

//------------------------------------------------------------------------------
/* virtual */ UINT32 FermiFs::FscontrolMask() const
{
    UINT32 fsctrMask = DRF_SHIFTMASK(LW_PTOP_FS_STATUS_DISPLAY)   |
                       DRF_SHIFTMASK(LW_PTOP_FS_STATUS_MSDEC)     |
                       DRF_SHIFTMASK(LW_PTOP_FS_STATUS_MSVLD_SEC) |
                       DRF_SHIFTMASK(LW_PTOP_FS_STATUS_CE)        |
                       DRF_SHIFTMASK(LW_PTOP_FS_STATUS_PCIE_LANE);

    if (!IsFloorsweepingAllowed())
    {
        // Floorsweeping does not work on Fmodel or Amodel on most GPUs.
        // In that case just return all
        return fsctrMask;
    }

    return (~m_pSub->RegRd32(LW_PTOP_FS_STATUS) & fsctrMask);
}

//------------------------------------------------------------------------------
/* virtual */ UINT32 FermiFs::FbMaskImpl() const
{
    return FbpMaskImpl();
}

//------------------------------------------------------------------------------
/* virtual */ UINT32 FermiFs::FbpMaskImpl() const
{
    RegHal &regs = m_pSub->Regs();

    switch (Platform::GetSimulationMode())
    {
        case Platform::Hardware:
            return (~regs.Read32(MODS_PTOP_FS_STATUS_FBP) &
                    ((1 << m_pSub->RegRd32(LW_PTOP_SCAL_NUM_FBPS)) - 1));
        case Platform::Fmodel:
        case Platform::Amodel:
            return ((1 << regs.Read32(MODS_PPRIV_MASTER_RING_ENUMERATE_RESULTS_FBP))
                    - 1);
        case Platform::RTL:
            return Utility::FindNLowestSetBits(
                        ~regs.Read32(MODS_PTOP_FS_STATUS_FBP) &
                        ((1 << regs.Read32(MODS_PTOP_SCAL_NUM_FBPS)) - 1),
                        regs.Read32(MODS_PPRIV_MASTER_RING_ENUMERATE_RESULTS_FBP));
        default:
            MASSERT(!"Illegal case");
            break;
    }
    return 0;
}

//------------------------------------------------------------------------------
/* virtual */ UINT32 FermiFs::L2MaskImpl(UINT32 HwFbpNum) const
{
    // Before GM20x there was only 1 L2 per FBP
    return 0x1;
}

//------------------------------------------------------------------------------
/* virtual */ UINT32 FermiFs::FbioMaskImpl() const
{
    RegHal &regs = m_pSub->Regs();

    switch (Platform::GetSimulationMode())
    {
        case Platform::Hardware:
            return (~regs.Read32(MODS_PTOP_FS_STATUS_FBIO) &
                    ((1 << regs.Read32(MODS_PTOP_SCAL_NUM_FBPS)) - 1));
        case Platform::Fmodel:
        case Platform::Amodel:
            return ((1 << regs.Read32(MODS_PPRIV_MASTER_RING_ENUMERATE_RESULTS_FBP))
                    - 1);
        case Platform::RTL:
            return Utility::FindNLowestSetBits(
                        ~regs.Read32(MODS_PTOP_FS_STATUS_FBIO) &
                        ((1 << regs.Read32(MODS_PTOP_SCAL_NUM_FBPS)) - 1),
                        regs.Read32(MODS_PPRIV_MASTER_RING_ENUMERATE_RESULTS_FBP));
        default:
            MASSERT(!"Illegal case");
            break;
    }
    return 0;
}

//------------------------------------------------------------------------------
/* virtual */ UINT32 FermiFs::FbpaMaskImpl() const
{
    // Most fermis don't support LW_PTOP_SCAL_NUM_FBPAS, so assume FBPs==FBPAs
    const UINT32 allFbpasMask = (1 << m_pSub->RegRd32(LW_PTOP_SCAL_NUM_FBPS))-1;

    if (!IsFloorsweepingAllowed())
    {
        // Floorsweeping does not work on Amodel or Fmodel on most GPUs.
        // In that case just return all
        return allFbpasMask;
    }

    return (~m_pSub->RegRd32(LW_PTOP_FS_STATUS_FBPA) & allFbpasMask);
}

//------------------------------------------------------------------------------
/* virtual */ UINT32 FermiFs::GpcMask() const
{
    RegHal &regs = m_pSub->Regs();

    switch (Platform::GetSimulationMode())
    {
        case Platform::Hardware:
            return (~regs.Read32(MODS_PTOP_FS_STATUS_GPC) &
                    ((1 << m_pSub->GetMaxGpcCount()) - 1));
        case Platform::Fmodel:
        case Platform::Amodel:
            return ((1 << regs.Read32(MODS_PPRIV_MASTER_RING_ENUMERATE_RESULTS_GPC))
                    - 1);
        case Platform::RTL:
            return Utility::FindNLowestSetBits(
                        ~regs.Read32(MODS_PTOP_FS_STATUS_GPC) &
                        ((1 << m_pSub->GetMaxGpcCount()) - 1),
                        regs.Read32(MODS_PPRIV_MASTER_RING_ENUMERATE_RESULTS_GPC));
        default:
            MASSERT(!"Illegal case");
            break;
    }
    return 0;
}

//------------------------------------------------------------------------------
/* virtual */ UINT32 FermiFs::GfxGpcMask() const
{
    UINT32 gpcMask = GpcMask();
    if (!m_pSub->HasFeature(Device::Feature::GPUSUB_SUPPORTS_COMPUTE_ONLY_GPC))
        return gpcMask;

    UINT32 gfxGpcMask = 0;
    INT32 gpcIdx;
    UINT32 tmpMask;

    while ((gpcIdx = Utility::BitScanForward(gpcMask)) >= 0)
    {
        tmpMask = 1U << gpcIdx;
        gpcMask ^= tmpMask;
        if (m_pSub->IsGpcGraphicsCapable(gpcIdx))
        {
            gfxGpcMask |= tmpMask;
        }
    }

    return gfxGpcMask;
}

//-------------------------------------------------------------------------------
/* virtual */ UINT32 FermiFs::TpcMask(UINT32 HwGpcNum) const
{
    RegHal &regs = m_pSub->Regs();

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
        UINT32 VirtualGpcNum;
        m_pSub->HwGpcToVirtualGpc(HwGpcNum, &VirtualGpcNum);
        UINT32 reg = (regs.LookupAddress(MODS_PGRAPH_PRI_GPC0_GPCCS_FS_GPC) +
                      LW_GPC_PRI_STRIDE * VirtualGpcNum);
        simTpcCount = DRF_VAL(_PGRAPH, _PRI_GPC0_GPCCS_FS_GPC,
                              _NUM_AVAILABLE_TPCS, m_pSub->RegRd32(reg));
    }

    // Callwlate hwTpcMask
    if (simMode == Platform::Hardware || simMode == Platform::RTL)
    {
        UINT32 gpcGroup = HwGpcNum / LW_PTOP_FS_STATUS_GPC_TPC_IDX___SIZE_1;
        UINT32 tpcPerGpcGroup = m_pSub->RegRd32(LW_PTOP_FS_STATUS_GPC_TPC(gpcGroup));

        hwTpcMask = ((1 << m_pSub->GetMaxTpcCount()) - 1);
        switch (HwGpcNum % LW_PTOP_FS_STATUS_GPC_TPC_IDX___SIZE_1)
        {
            case 0:
                hwTpcMask &= ~DRF_VAL(_PTOP_FS, _STATUS_GPC_TPC, _G0, tpcPerGpcGroup);
                break;
            case 1:
                hwTpcMask &= ~DRF_VAL(_PTOP_FS, _STATUS_GPC_TPC, _G1, tpcPerGpcGroup);
                break;
            case 2:
                hwTpcMask &= ~DRF_VAL(_PTOP_FS, _STATUS_GPC_TPC, _G2, tpcPerGpcGroup);
                break;
            case 3:
                hwTpcMask &= ~DRF_VAL(_PTOP_FS, _STATUS_GPC_TPC, _G3, tpcPerGpcGroup);
                break;
        }
    }

    // Return TPC mask
    switch (simMode)
    {
        case Platform::Hardware:
            return hwTpcMask;
        case Platform::Amodel:
        case Platform::Fmodel:
            // Floorsweeping does not work on models.  Just create a mask
            // of available TPCs
            return (1 << simTpcCount) - 1;
        case Platform::RTL:
            return Utility::FindNLowestSetBits(hwTpcMask, simTpcCount);
        default:
            MASSERT(!"Illegal simMode");
            break;
    }

    return 0;
}

//------------------------------------------------------------------------------
/* virtual */ UINT32 FermiFs::FbioshiftMaskImpl() const
{
    if (!IsFloorsweepingAllowed())
    {
        // Floorsweeping does not work on Amodel or FModel on most GPUs.
        // In that case just return no shifts
        return 0;
    }

    UINT32 fbioShiftMask = ((1 << m_pSub->RegRd32(LW_PTOP_SCAL_NUM_FBPS)) - 1);
    return (m_pSub->Regs().Read32(MODS_PTOP_FS_STATUS_FBIO_SHIFT_MUX) &
            fbioShiftMask);
}

//------------------------------------------------------------------------------
/* virtual */ UINT32 FermiFs::ZlwllMask(UINT32 HwGpcNum) const
{
    RegHal &regs = m_pSub->Regs();

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
        UINT32 VirtualGpcNum;
        m_pSub->HwGpcToVirtualGpc(HwGpcNum, &VirtualGpcNum);
        UINT32 reg = (regs.LookupAddress(MODS_PGRAPH_PRI_GPC0_GPCCS_FS_GPC) +
                      LW_GPC_PRI_STRIDE * VirtualGpcNum);
        UINT32 zlwllCount = DRF_VAL(_PGRAPH, _PRI_GPC0_GPCCS_FS_GPC,
                              _NUM_AVAILABLE_ZLWLLS, m_pSub->RegRd32(reg));

        return (1 << zlwllCount) - 1;
    }

    UINT32 gpcGroup = HwGpcNum / LW_PTOP_FS_STATUS_GPC_ZLWLL_IDX___SIZE_1;
    UINT32 zlwllPerGpcGroup = m_pSub->RegRd32(LW_PTOP_FS_STATUS_GPC_ZLWLL(gpcGroup));
    UINT32 zlwllMask = ((1 << m_pSub->GetMaxTpcCount()) - 1); // Max zlwlls == Max tpcs
    UINT32 mask = 0;
    switch (HwGpcNum % LW_PTOP_FS_STATUS_GPC_TPC_IDX___SIZE_1)
    {
        case 0:
            mask = DRF_VAL(_PTOP_FS, _STATUS_GPC_ZLWLL, _G0, ~zlwllPerGpcGroup);
            break;
        case 1:
            mask = DRF_VAL(_PTOP_FS, _STATUS_GPC_ZLWLL, _G1, ~zlwllPerGpcGroup);
            break;
        case 2:
            mask = DRF_VAL(_PTOP_FS, _STATUS_GPC_ZLWLL, _G2, ~zlwllPerGpcGroup);
            break;
        case 3:
            mask = DRF_VAL(_PTOP_FS, _STATUS_GPC_ZLWLL, _G3, ~zlwllPerGpcGroup);
            break;
    }
    return mask & zlwllMask;
}

//------------------------------------------------------------------------------
/* virtual */ UINT32 FermiFs::SmMask
(
    UINT32 HwGpcNum,
    UINT32 HwTpcNum
) const
{
    return 1;
}

//-------------------------------------------------------------------------------
/* virtual */ UINT32 FermiFs::SpareMask() const
{
    UINT32 spareMask = DRF_SHIFTMASK(LW_PTOP_FS_STATUS_SPARE_BITS);

    if (!IsFloorsweepingAllowed())
    {
        // Floorsweeping does not work on Amodel or FModel on most GPUs.
        // In that case just return no shifts
        return spareMask;
    }

    return (~m_pSub->RegRd32(LW_PTOP_FS_STATUS_SPARE) & spareMask);
}

//-------------------------------------------------------------------------------
/* virtual */ UINT32 FermiFs::XpMask() const
{
    return (m_pSub->Regs().Read32(MODS_PTOP_FS_STATUS_PCIE_LANE) ^
            DRF_MASK(LW_PTOP_FS_STATUS_PCIE_LANE));
}

//------------------------------------------------------------------------------
/* virtual */ UINT32 FermiFs::LwdecMask() const
{
    return 0;
}
//------------------------------------------------------------------------------
/* virtual */ UINT32 FermiFs::LwencMask() const
{
    return 0;
}

//------------------------------------------------------------------------------
/* virtual */ void FermiFs::FbEnable(bool bEnable)
{
    UINT32 data = m_pSub->RegRd32(LW_PMC_ENABLE);

    if (bEnable)
    {
        data = FLD_SET_DRF(_PMC, _ENABLE, _PFB, _ENABLED, data);
    }
    else
    {
        data = FLD_SET_DRF(_PMC, _ENABLE, _PFB, _DISABLED, data);
    }

    m_pSub->RegWr32(LW_PMC_ENABLE, data);
}

//------------------------------------------------------------------------------
/* virtual */ void FermiFs::GraphicsEnable(bool bEnable)
{
    UINT32 data = m_pSub->RegRd32(LW_PMC_ENABLE);
    if (bEnable)
    {
        data = FLD_SET_DRF(_PMC, _ENABLE, _PGRAPH, _ENABLED, data);
    }
    else
    {
        data = FLD_SET_DRF(_PMC, _ENABLE, _PGRAPH, _DISABLED, data);
    }
    m_pSub->RegWrSync32(LW_PMC_ENABLE, data);
}

//------------------------------------------------------------------------------
CREATE_GPU_FS_FUNCTION(FermiFs);

