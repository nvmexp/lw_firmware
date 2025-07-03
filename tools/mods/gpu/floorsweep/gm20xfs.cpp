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

#include "gm20xfs.h"

#include "core/include/argdb.h"
#include "core/include/argpproc.h"
#include "core/include/argread.h"
#include "core/include/platform.h"
#include "core/include/utility.h"
#include "device/interface/pcie.h"
#include "gpu/reghal/reghal.h"
#include "maxwell/gm200/dev_fifo.h"
#include "maxwell/gm200/dev_fuse.h"
#include "maxwell/gm200/dev_lw_xve.h"
#include "maxwell/gm200/dev_pbdma.h"
#include "maxwell/gm200/dev_top.h"

//------------------------------------------------------------------------------
// access the below parameters via:
// mods ... mdiag.js -fermi_fs <feature-string>
// a "feature-string" is <name>:<value>[:<value>...]
//
static const ParamDecl s_FloorSweepingParamsGM20x[] =
{
    { "fbp_rop_l2_enable",          "uu", ParamDecl::PARAM_MULTI_OK, 0, 0,
                                    "ROP L2 enable. Usage <fbp_num> <rop_l2_enable>"},
    { "fbp_rop_l2_disable",         "uu", ParamDecl::PARAM_MULTI_OK, 0, 0,
                                    "ROP L2 disable. Usage <fbp_num> <rop_l2_disable>"},
    { "fbp_ltc_enable",             "uu", ParamDecl::PARAM_MULTI_OK, 0, 0,
                                    "L2 enable. Usage <fbp_num> <ltc_enable>"},
    { "fbp_ltc_disable",            "uu", ParamDecl::PARAM_MULTI_OK, 0, 0,
                                    "L2 disable. Usage <fbp_num> <ltc_disable>"},
    LAST_PARAM
};

//------------------------------------------------------------------------------
static ParamConstraints s_FsParamConstraintsGM20x[] =
{
   MUTUAL_EXCLUSIVE_PARAM("fbp_rop_l2_enable", "fbp_rop_l2_disable"),
   MUTUAL_EXCLUSIVE_PARAM("fbp_ltc_enable", "fbp_ltc_disable"),
   LAST_CONSTRAINT
};

//------------------------------------------------------------------------------
GM20xFs::GM20xFs(GpuSubdevice *pSubdev):
    GM10xFs(pSubdev)
{
    m_FsFbpL2ParamPresent.resize(LW_FUSE_CTRL_OPT_FBP_IDX__SIZE_1,
                                 false);

    m_FbpL2Masks.resize(LW_FUSE_CTRL_OPT_FBP_IDX__SIZE_1, 0);

    m_SavedFbpL2Masks.resize(LW_FUSE_CTRL_OPT_FBP_IDX__SIZE_1);
}

//------------------------------------------------------------------------------
// | GM200 yes | GM204 yes | GM206 yes | GM204X yes | GM206X yes | GM20B ?? |
RC GM20xFs::ReadFloorsweepingArgumentsImpl(ArgDatabase& fsArgDb)
{
    RC rc;

    // Parse gm10x+ floorsweeping arguments
    CHECK_RC(GM10xFs::ReadFloorsweepingArgumentsImpl(fsArgDb));

    string FsArgs = GetFermiFsArgs();
    if (!FsArgs.empty())
    {
        ArgReader argReader(s_FloorSweepingParamsGM20x, s_FsParamConstraintsGM20x);
        if (!argReader.ParseArgs(&fsArgDb))
            return RC::BAD_COMMAND_LINE_ARGUMENT;

        bool ropL2Valid = m_pSub->Regs().IsSupported(MODS_FUSE_OPT_ROP_L2_DISABLE);
        bool fbpRopL2ParamPresent = argReader.ParamPresent("fbp_rop_l2_enable") ||
                                    argReader.ParamPresent("fbp_rop_l2_disable");
        bool fbpL2ParamPresent    = argReader.ParamPresent("fbp_ltc_enable") ||
                                    argReader.ParamPresent("fbp_ltc_disable");
        if (!ropL2Valid && fbpRopL2ParamPresent)
        {
            Printf(Tee::PriError, "GM20xGpuSubdevice::ReadFloorsweepingArguments"
                                  " -fbp_rop_l2_enable/disable not supported."
                                  " Use -fbp_ltc_enable/disable instead\n");
            return RC::BAD_COMMAND_LINE_ARGUMENT;
        }
        else if (ropL2Valid && fbpL2ParamPresent)
        {
            Printf(Tee::PriError, "GM20xGpuSubdevice::ReadFloorsweepingArguments"
                                  " -fbp_ltc_enable/disable not supported."
                                  " Use -fbp_rop_l2_enable/disable instead\n");
            return RC::BAD_COMMAND_LINE_ARGUMENT;
        }

        // Now get the mask values. If the enable param is specified, use it's
        // bitwise negation as the mask value. If the disable param is specified,
        // use it directly as the mask. This code works because _enable and
        // _disable are mutually exclusive for a given feature.
        bool ilwert = false;
        string fbpL2ParamName;
        if (ropL2Valid)
        {
            fbpL2ParamName = "fbp_rop_l2_disable";
            if (argReader.ParamPresent("fbp_rop_l2_enable"))
            {
                ilwert = true;
                fbpL2ParamName = "fbp_rop_l2_enable";
            }
        }
        else
        {
            fbpL2ParamName = "fbp_ltc_disable";
            if (argReader.ParamPresent("fbp_ltc_enable"))
            {
                ilwert = true;
                fbpL2ParamName = "fbp_ltc_enable";
            }
        }
        
        fbpL2ParamPresent = false;
        UINT32 numFbpL2Args = argReader.ParamPresent(fbpL2ParamName.c_str());
        for (UINT32 i = 0; i < numFbpL2Args; ++i)
        {
            UINT32 FbpNum = argReader.ParamNUnsigned(fbpL2ParamName.c_str(), i, 0);
            UINT32 L2Mask = argReader.ParamNUnsigned(fbpL2ParamName.c_str(), i, 1);
            L2Mask        = ilwert ? ~L2Mask : L2Mask;

            if (FbpNum >= m_FbpL2Masks.size())
            {
                Printf(Tee::PriHigh, "GM20xGpuSubdevice::ReadFloorsweeping"
                                     "Arguments: fbp # too large: %d\n", FbpNum);
                return RC::BAD_COMMAND_LINE_ARGUMENT;
            }

            m_FbpL2Masks[FbpNum]          = L2Mask;
            m_FsFbpL2ParamPresent[FbpNum] = true;
            fbpL2ParamPresent             = true;

            const RegHal& regs = m_pSub->Regs();
            const bool hasRWAccess =
                regs.IsSupported(MODS_FUSE_CTRL_OPT_ROP_L2_FBP_DATA, i)
                ? regs.HasRWAccess(MODS_FUSE_CTRL_OPT_ROP_L2_FBP, i)
                : regs.IsSupported(MODS_FUSE_CTRL_OPT_LTC_FBP_DATA, i)
                ? regs.HasRWAccess(MODS_FUSE_CTRL_OPT_LTC_FBP, i)
                : true;
            if (!hasRWAccess)
            {
                Printf(Tee::PriError, "%s floorsweeping not supported on %s\n",
                       fbpL2ParamName.c_str(),
                       Gpu::DeviceIdToString(m_pSub->DeviceId()).c_str());
                return RC::BAD_COMMAND_LINE_ARGUMENT;
            }
        }

        SetFloorsweepingAffected(GetFloorsweepingAffected() ||
                                 fbpL2ParamPresent);
    }

    return rc;
}

//------------------------------------------------------------------------------
// | GM200 yes | GM204 yes | GM206 yes | GM204X yes | GM206X yes | GM20B ?? |
void GM20xFs::PrintFloorsweepingParams()
{
    GM10xFs::PrintFloorsweepingParams();
    
    Tee::Priority pri = GetFloorsweepingPrintPriority();
    bool ropL2Valid = m_pSub->Regs().IsSupported(MODS_FUSE_OPT_ROP_L2_DISABLE);

    Printf(pri, "GM20xGpuSubdevice: Floorsweeping parameters "
                "present on commandline: ");
    for (UINT32 i = 0; i < m_FsFbpL2ParamPresent.size(); i++)
    {
        if (m_FsFbpL2ParamPresent[i])
        {
            string paramName = ropL2Valid ? "fbp_rop_l2" : "fbp_ltc";
            Printf(pri, "%s[%d] ", paramName.c_str(), i);
        }
    }
    Printf(pri, "\n");

    Printf(pri, "GM20xGpuSubdevice: Floorsweeping parameters "
                "mask values: ");
    for (UINT32 i = 0; i < m_FbpL2Masks.size(); ++i)
    {
        string paramName = ropL2Valid ? "fbp_rop_l2" : "fbp_ltc";
        Printf(pri, "%s[%d]=0x%x ", paramName.c_str(), i, m_FbpL2Masks[i]);
    }
    Printf(pri, "\n");
}

//------------------------------------------------------------------------------
// ApplyFloorsweepingSettings passed via the command line arguments.
// Note:
// Most of the internal variables are maintained in FermiFs however the register
// structure for some of these registers have been simplified. The
// simplification requires utilization of new individual m_SavedFsxxxx vars.
//
// | GM200 yes | GM204 yes | GM206 yes | GM204X yes | GM206X yes | GM20B ?? |
void GM20xFs::ApplyFloorsweepingSettings()
{
    GM10xFs::ApplyFloorsweepingSettings();
    // All floorsweeping registers should have a write with ack on non-hardware
    // platforms. see http://lwbugs/1043132

    RegHal &regs = m_pSub->Regs(); 
    for (UINT32 i = 0; i < m_FbpL2Masks.size(); i++)
    {
        if (m_FsFbpL2ParamPresent[i])
        {
            if (regs.IsSupported(MODS_FUSE_CTRL_OPT_ROP_L2_FBP_DATA, i))
            {
                regs.Write32Sync(MODS_FUSE_CTRL_OPT_ROP_L2_FBP_DATA, i, m_FbpL2Masks[i]);
            }
            else if (regs.IsSupported(MODS_FUSE_CTRL_OPT_LTC_FBP_DATA, i))
            {
                regs.Write32Sync(MODS_FUSE_CTRL_OPT_LTC_FBP_DATA, i, m_FbpL2Masks[i]);
            }
        }
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
// | GM200 yes | GM204 yes | GM206 yes | GM204X yes | GM206X yes | GM20B ?? |
void GM20xFs::RestoreFloorsweepingSettings()
{
    Printf(Tee::PriDebug, "GM200GpuSubdevice::RestoreFloorsweepingSettings: "
           " checking and restoring startup settings\n");

    RegHal &regs = m_pSub->Regs(); 
    for (UINT32 i = 0; i < m_FsFbpL2ParamPresent.size(); ++i)
    {
        if (m_FsFbpL2ParamPresent[i])
        {
            if (regs.IsSupported(MODS_FUSE_CTRL_OPT_ROP_L2_FBP, i))
            {
                regs.Write32Sync(MODS_FUSE_CTRL_OPT_ROP_L2_FBP, i, m_SavedFbpL2Masks[i]);
            }
            else if (regs.IsSupported(MODS_FUSE_CTRL_OPT_LTC_FBP, i))
            {
                regs.Write32Sync(MODS_FUSE_CTRL_OPT_LTC_FBP, i, m_SavedFbpL2Masks[i]);
            }
        }
    }

    return(GM10xFs::RestoreFloorsweepingSettings());
}

//------------------------------------------------------------------------------
// | GM200 yes | GM204 yes | GM206 yes | GM204X yes | GM206X yes | GM20B ?? |
void GM20xFs::ResetFloorsweepingSettings()
{
    Printf(Tee::PriDebug, "GM200GpuSubdevice::ResetFloorsweepingSettings: "
           " reading and resetting settings\n");

    RegHal &regs       = m_pSub->Regs();
    UINT32 maxFbps     = m_pSub->GetMaxFbpCount();
    UINT32 numL2PerFbp = regs.Read32(MODS_PTOP_SCAL_NUM_LTC_PER_FBP);
    UINT32 L2Mask      = (1 << numL2PerFbp) - 1;

    UINT32 fbpL2;
    if (regs.Read32Priv(MODS_FUSE_OPT_ROP_L2_DISABLE, &fbpL2) == RC::OK)
    {
        for (UINT32 fbpIndex = 0; fbpIndex < maxFbps; fbpIndex++)
        {
            UINT32 L2 = (fbpL2 & L2Mask) >> (fbpIndex * numL2PerFbp);
            (void)regs.Write32PrivSync(MODS_FUSE_CTRL_OPT_ROP_L2_FBP,
                                       fbpIndex, L2);
            L2Mask <<= numL2PerFbp;
        }
    }
    else if (regs.Read32Priv(MODS_FUSE_OPT_LTC_DISABLE, &fbpL2) == RC::OK)
    {
        UINT32 fbpL2 = regs.Read32(MODS_FUSE_OPT_LTC_DISABLE);
        for (UINT32 fbpIndex = 0; fbpIndex < maxFbps; fbpIndex++)
        {
            UINT32 L2 = (fbpL2 & L2Mask) >> (fbpIndex * numL2PerFbp);
            (void)regs.Write32PrivSync(MODS_FUSE_CTRL_OPT_LTC_FBP,
                                       fbpIndex, L2);
            L2Mask <<= numL2PerFbp;
        }
    }

    GM10xFs::ResetFloorsweepingSettings();
}

//------------------------------------------------------------------------------
// | GM200 yes | GM204 yes | GM206 yes | GM204X yes | GM206X yes | GM20B ?? |
void GM20xFs::SaveFloorsweepingSettings()
{
    // Save all floorsweeping settings that could conceivably be changed by this
    // GpuSubdevice (in ApplyFloorsweepingSettings)
    //
    RegHal &regs = m_pSub->Regs();
    for (UINT32 i = 0; i < m_SavedFbpL2Masks.size(); i++)
    {
        UINT32 mask;
        if (regs.Read32Priv(MODS_FUSE_CTRL_OPT_ROP_L2_FBP, i, &mask) == RC::OK
            || regs.Read32Priv(MODS_FUSE_CTRL_OPT_LTC_FBP, i, &mask) == RC::OK)
        {
            m_SavedFbpL2Masks[i] = mask;
        }
    }

    return GM10xFs::SaveFloorsweepingSettings();

}

//------------------------------------------------------------------------------
// | GM200 yes | GM204 yes | GM206 yes | GM204X yes | GM206X yes | GM20B no |
RC GM20xFs::GetFloorsweepingMasks
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

    CHECK_RC(GM10xFs::GetFloorsweepingMasks(pFsMasks));

    // Add any GM200 specific floorsweeping masks:
    UINT32 fbpMask  = FbMaskImpl();
    UINT32 fbpNum   = 0;
    bool ropL2Valid = m_pSub->Regs().IsSupported(MODS_FUSE_OPT_ROP_L2_DISABLE);
    while (fbpMask != 0)
    {
        if (fbpMask & 1)
        {
            string fsmaskName = Utility::StrPrintf("fbp%s[%d]_mask",
                                                    ropL2Valid ? "RopL2" : "Ltc", fbpNum);
            pFsMasks->insert(pair<string, UINT32>(fsmaskName, L2MaskImpl(fbpNum)));
        }

        fbpNum++;
        fbpMask >>= 1;
    }

    return rc;
}

/* virtual */ RC GM20xFs::PrintMiscFuseRegs()
{
    RC rc;

    CHECK_RC(GM10xFs::PrintMiscFuseRegs());

    if (m_pSub->Regs().IsSupported(MODS_FUSE_OPT_ROP_L2_DISABLE))
    {
        Printf(Tee::PriNormal, " OPT_ROP_L2_DIS : 0x%x\n",
                            m_pSub->Regs().Read32(MODS_FUSE_OPT_ROP_L2_DISABLE));
    }
    else if (m_pSub->Regs().IsSupported(MODS_FUSE_OPT_LTC_DISABLE))
    {
        Printf(Tee::PriNormal, " OPT_LTC_DIS : 0x%x\n",
                             m_pSub->Regs().Read32(MODS_FUSE_OPT_LTC_DISABLE));
    }
    Printf(Tee::PriNormal, " OPT_ZLWLL_DIS  : 0x%x\n",
                        m_pSub->Regs().Read32(MODS_FUSE_OPT_ZLWLL_DISABLE));
    return OK;
}

//------------------------------------------------------------------------------
/* virtual */ UINT32 GM20xFs::FbpMaskImpl() const
{
    // SOC GPUs don't have FB. Fake 1 FB partition for memory tests.
    if (m_pSub->IsSOC())
    {
        return 1;
    }

    //Read Fuse status to get floorsweep mask
    const UINT32 fbpFuseStatus = m_pSub->Regs().Read32(MODS_FUSE_STATUS_OPT_FBP);

    //Hardcoded value on any platforms
    const UINT32 defParts = m_pSub->Regs().Read32(MODS_PTOP_SCAL_NUM_FBPS);

    // Effective active Fbps
    // This is physical mapping. Note that virtual mapping (listed in *_PFB_FBPA_*
    // registers) will work sequentially without holes.
    // Mods should continue to write _PFG_FBPA0 then 1 then 2 ... without
    // worrying about physical fbp mask
    return (~fbpFuseStatus & ((1 << defParts) -1));
}

//------------------------------------------------------------------------------
/* virtual */ UINT32 GM20xFs::FbioMaskImpl() const
{
    // SOC GPUs don't have FB. Fake 1 FB partition for memory tests.
    if (m_pSub->IsSOC())
    {
        return 1;
    }

    //Read Fuse status to get floorsweep mask
    const UINT32 fbioFuseStatus = m_pSub->Regs().Read32(MODS_FUSE_STATUS_OPT_FBIO);

    //Hardcoded value on any platforms
    const UINT32 defParts = m_pSub->Regs().Read32(MODS_PTOP_SCAL_NUM_FBPS);

    // Effective active Fbios
    return (~fbioFuseStatus & ((1 << defParts) -1));
}

//------------------------------------------------------------------------------
// | GM200 yes | GM204 yes | GM206 yes | GM204X yes | GM206X yes | GM20B no |
UINT32 GM20xFs::L2MaskImpl(UINT32 HwFbpNum) const
{
    // If the FbpNum was not supplied, get the number of L2s from the first
    // non-floorswept FBP
    if (HwFbpNum == ~0U)
    {
        INT32 firstFbp = Utility::BitScanForward(FbMaskImpl());
        if (firstFbp < 0)
        {
            return 0;
        }
        HwFbpNum = static_cast<UINT32>(firstFbp);
    }
    else if (!(FbMaskImpl() & (1 << HwFbpNum)))
    {
        return 0;
    }

    RegHal &regs       = m_pSub->Regs();
    UINT32 hwL2Mask    = 0;
    UINT32 numL2PerFbp = regs.Read32(MODS_PTOP_SCAL_NUM_LTC_PER_FBP);
    UINT32 L2s         = 0;
    if (RC::OK != regs.Read32Priv(MODS_FUSE_STATUS_OPT_ROP_L2_FBP,
                                  HwFbpNum, &L2s) &&
        RC::OK != regs.Read32Priv(MODS_FUSE_STATUS_OPT_LTC_FBP,
                                  HwFbpNum, &L2s))
    {
        Printf(Tee::PriWarn, "Cannot read L2 mask\n");
    }
    hwL2Mask = ((1 << numL2PerFbp) - 1);
    hwL2Mask &= ~L2s;

    return hwL2Mask;
}

//------------------------------------------------------------------------------
/*virtual*/ UINT32 GM20xFs::LwencMask() const
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
/*virtual*/ void GM20xFs::CheckIfPriRingInitIsRequired()
{
    for (UINT32 i = 0; i < m_FsFbpL2ParamPresent.size(); ++i)
    {
        m_IsPriRingInitRequired = m_IsPriRingInitRequired || m_FsFbpL2ParamPresent[i];
    }
    GM10xFs::CheckIfPriRingInitIsRequired();
}

//-----------------------------------------------------------------------------
//! Trigger a SW reset
//  In addition it is highly recommended that the PCI config space (GPU & Azalia) be
//  saved before calling this function and restored after calling it.
//  Documentation of the SW Reset:
//  1. https://wiki.lwpu.com/gpuhwmaxwell/index.php/Maxwell_Reset#SW_Reset
//  2. sw/dev/gpu_drv/chips_a/drivers/resman/kernel/bif/maxwell/bifgm200.c:bifDoFullChipReset_Gm200()
//  3. http://lwbugs/1595785
/* virtual */ RC GM20xFs::SwReset(bool bReinitPriRing)
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
    regs.Write32(MODS_PMC_ENABLE,pmcValue);
    // need a couple dummy reads
    regs.Read32(MODS_PMC_ENABLE);
    regs.Read32(MODS_PMC_ENABLE);

    // Before doing SW_RESET, init LW_XP_PL_CYA_1_BLOCK_HOST2XP_HOLD_LTSSM to 1.
    // else when LW_XVE_SW_RESET to 0, host will try to do rom init and will assert
    // HOST2XP_HOLD_LTSSM to 0 which will cause ltssm to goto detect.
    //
    regs.Write32(MODS_XP_PL_CYA_1_BLOCK_HOST2XP_HOLD_LTSSM_ENABLE, 0);

    CHECK_RC(m_pSub->PollGpuRegValue(MODS_XP_PL_CYA_1_BLOCK_HOST2XP_HOLD_LTSSM_ENABLE,
                                     0,
                                     1000.0));

    UINT32 tempRegVal = regs.Read32(MODS_XVE_SW_RESET);

    // Assert LW_XVE_SW_RESET_RESET_ENABLE
    regs.SetField(&tempRegVal, MODS_XVE_SW_RESET_RESET, 1);

    regs.Write32(MODS_XVE_SW_RESET, tempRegVal);

    // sleep for 1ms to let it finish
    Tasker::Sleep(1);

    // Undefined bit fields ( bit 3, 31:28 ) are set after reset enable, therefore,
    // only retrieve defined bit fields and set LW_XVE_SW_RESET_RESET_DISABLE to
    // come out of reset.
    //
    //
    // Come out of reset. Note that when SW/full-chip reset is triggered by the
    // above write to LW_XVE_SW_RESET, BAR0 priv writes do not work and thus
    // this write must be a PCI config bus write.
    //
    auto pPcie = m_pSub->GetInterface<Pcie>();
    MASSERT(pPcie);
    Platform::PciRead32(pPcie->DomainNumber(),
                        pPcie->BusNumber(),
                        pPcie->DeviceNumber(),
                        pPcie->FunctionNumber(),
                        LW_XVE_SW_RESET,
                        &tempRegVal);

    tempRegVal &= regs.LookupMask(MODS_XVE_SW_RESET_RESET) |
                  regs.LookupMask(MODS_XVE_SW_RESET_GPU_ON_SW_RESET) |
                  regs.LookupMask(MODS_XVE_SW_RESET_COUNTER_EN) |
                  regs.LookupMask(MODS_XVE_SW_RESET_COUNTER_VAL) |
                  regs.LookupMask(MODS_XVE_SW_RESET_CLOCK_ON_SW_RESET) |
                  regs.LookupMask(MODS_XVE_SW_RESET_CLOCK_COUNTER_EN) |
                  regs.LookupMask(MODS_XVE_SW_RESET_CLOCK_COUNTER_VAL);

    regs.SetField(&tempRegVal, MODS_XVE_SW_RESET_RESET, 0);

    Platform::PciWrite32(pPcie->DomainNumber(),
                         pPcie->BusNumber(),
                         pPcie->DeviceNumber(),
                         pPcie->FunctionNumber(),
                         LW_XVE_SW_RESET,
                         tempRegVal);

    // IFR re-runs at this point. Sleep for 150ms for any tasks triggered by
    // IFR (such as Firmware Security and Licensing) to finish to avoid races
    Tasker::Sleep(150);

    if (bReinitPriRing)
    {
        CHECK_RC(InitializePriRing_FirstStage(false));
        CHECK_RC(InitializePriRing());
    }

    return OK;
}

/* virtual */ UINT32 GM20xFs::GetEngineResetMask()
{
    RegHal &regs = m_pSub->Regs();
    // This mask was actually pulled from RM's bifGetValidEnginesToReset_GM200
    return regs.LookupMask(MODS_PMC_ENABLE_PGRAPH) |
           regs.LookupMask(MODS_PMC_ENABLE_PMEDIA) |
           regs.LookupMask(MODS_PMC_ENABLE_CE0) |
           regs.LookupMask(MODS_PMC_ENABLE_CE1) |
           regs.LookupMask(MODS_PMC_ENABLE_CE2) |
           regs.LookupMask(MODS_PMC_ENABLE_PFIFO) |
           regs.LookupMask(MODS_PMC_ENABLE_PWR) |
           regs.LookupMask(MODS_PMC_ENABLE_PDISP) |
           regs.LookupMask(MODS_PMC_ENABLE_LWDEC) |
           regs.LookupMask(MODS_PMC_ENABLE_LWENC0) |
           regs.LookupMask(MODS_PMC_ENABLE_LWENC1) |
           regs.LookupMask(MODS_PMC_ENABLE_SEC);
}

//------------------------------------------------------------------------------
CREATE_GPU_FS_FUNCTION(GM20xFs);

