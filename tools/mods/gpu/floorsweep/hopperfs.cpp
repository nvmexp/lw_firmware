/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2020-2022 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is pcpcrietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#include "hopperfs.h"
#include "core/include/gpu.h"
#include "core/include/platform.h"
#include "core/include/utility.h"
#include "gpu/include/gpusbdev.h"
#include "gpu/reghal/reghal.h"
#include "gpu/utility/fsp.h"

#include "hopper/gh100/dev_fuse.h"

//------------------------------------------------------------------------------
static const ParamDecl s_FloorSweepingParamsHopper[] =
{
    { "gpc_cpc_enable",      "uu", ParamDecl::PARAM_MULTI_OK, 0, 0,
                              "CPC enable. Usage <gpc_num> <cpc_enable>"},
    { "gpc_cpc_disable",     "uu", ParamDecl::PARAM_MULTI_OK, 0, 0,
                              "CPC disable. Usage <gpc_num> <cpc_disable>"},
    UNSIGNED_PARAM("lwlink_perlink_enable",  "LwLink per-link enable"),
    UNSIGNED_PARAM("lwlink_perlink_disable", "LwLink per-link disable"),

    UNSIGNED_PARAM("c2c_enable", "Chip to chip link enable"),
    UNSIGNED_PARAM("c2c_disable", "Chip to chip link disable"),
    LAST_PARAM
};

//------------------------------------------------------------------------------
static ParamConstraints s_FsParamConstraintsHopper[] =
{
   MUTUAL_EXCLUSIVE_PARAM("gpc_cpc_enable",     "gpc_cpc_disable"),
   MUTUAL_EXCLUSIVE_PARAM("lwlink_perlink_enable", "lwlink_perlink_disable"),
   MUTUAL_EXCLUSIVE_PARAM("c2c_enable", "c2c_disable"),
   LAST_CONSTRAINT
};

//------------------------------------------------------------------------------
HopperFs::HopperFs(GpuSubdevice *pSubdev):
    GA10xFs(pSubdev)
{
    m_FsGpcCpcParamPresent.resize(LW_FUSE_CTRL_OPT_CPC_GPC__SIZE_1, false);
    m_GpcCpcMasks.resize(LW_FUSE_CTRL_OPT_CPC_GPC__SIZE_1, 0);
    m_SavedGpcCpcMasks.resize(LW_FUSE_CTRL_OPT_CPC_GPC__SIZE_1);
    m_FsData.emplace_back(FST_LWLINK_LINK, "lwlink_perlink", "LWLINK_LINK_DISABLE",
                          MODS_FUSE_CTRL_OPT_PERLINK_DATA,
                          MODS_FUSE_OPT_PERLINK_DISABLE_DATA);
    m_FsData.emplace_back(FST_C2C, "c2c", "C2C_DISABLE",
                          MODS_FUSE_CTRL_OPT_C2C_DATA,
                          MODS_FUSE_OPT_C2C_DISABLE_DATA);
}

//------------------------------------------------------------------------------
RC HopperFs::ReadFloorsweepingArgumentsImpl(ArgDatabase& fsArgDb)
{
    RC rc;

    // Read pre-Hopper floorsweeping arguments
    CHECK_RC(GA10xFs::ReadFloorsweepingArgumentsImpl(fsArgDb));

    string FsArgs = GetFermiFsArgs();
    if (!FsArgs.empty())
    {
        ArgReader argReader(s_FloorSweepingParamsHopper, s_FsParamConstraintsHopper);
        if (!argReader.ParseArgs(&fsArgDb))
        {
            return RC::BAD_COMMAND_LINE_ARGUMENT;
        }

        // Get gpc_cpc FS params
        bool ilwert = false;
        string gpcCpcParamName = "gpc_cpc_disable";
        if (argReader.ParamPresent("gpc_cpc_enable"))
        {
            ilwert = true;
            gpcCpcParamName = "gpc_cpc_enable";
        }
        bool gpcCpcParamPresent = false;
        UINT32 numGpcCpcArgs = argReader.ParamPresent(gpcCpcParamName.c_str());
        for (UINT32 i = 0; i < numGpcCpcArgs; i++)
        {
            UINT32 gpcNum  = argReader.ParamNUnsigned(gpcCpcParamName.c_str(), i, 0);
            UINT32 cpcMask = argReader.ParamNUnsigned(gpcCpcParamName.c_str(), i, 1);
            cpcMask        = ilwert ? ~cpcMask : cpcMask;

            if (gpcNum >= m_GpcCpcMasks.size())
            {
                Printf(Tee::PriError, "HopperGpuSubdevice::ReadFloorsweeping"
                                      "Arguments: GPC # too large: %d\n", gpcNum);
                return RC::BAD_COMMAND_LINE_ARGUMENT;
            }

            m_GpcCpcMasks[gpcNum]          = cpcMask;
            m_FsGpcCpcParamPresent[gpcNum] = true;
            gpcCpcParamPresent             = true;
        }

        for (auto & lwrFsData : m_FsData)
        {
            CHECK_RC(ReadFloorsweepArg(&argReader, &lwrFsData));
        }
        const bool bFsAffected =
            any_of(m_FsData.cbegin(),
                   m_FsData.cend(),
                   [](const FsData & f) { return f.bPresent; });

        SetFloorsweepingAffected(GetFloorsweepingAffected() ||
                                 gpcCpcParamPresent || bFsAffected);
    }
    return rc;
}

//------------------------------------------------------------------------------
void HopperFs::PrintFloorsweepingParams()
{
    GA10xFs::PrintFloorsweepingParams();
    Tee::Priority pri = GetFloorsweepingPrintPriority();

    string params;
    string masks;
    GetFloorsweepingStrings(m_FsData, &params, &masks);

    for (UINT32 i = 0; i < m_FsGpcCpcParamPresent.size(); i++)
    {
        if (m_FsGpcCpcParamPresent[i])
        {
            params += Utility::StrPrintf(" gpc_cpc[%u] ", i);
        }
    }

    for (UINT32 i = 0; i < m_GpcCpcMasks.size(); i++)
    {
        masks += Utility::StrPrintf(" gpc_cpc[%u]=0x%x ", i, m_GpcCpcMasks[i]);
    }

    Printf(pri, "HopperFs : Floorsweeping parameters present on commandline:%s\n", params.c_str());
    Printf(pri, "HopperFs : Floorsweeping parameters mask values:%s\n", masks.c_str());
}

//------------------------------------------------------------------------------
// Assert the fs2all_sample_floorsweep signal after changing any floorsweep configuration
// to prevent any instability in the hw units consumimg that signal.
void HopperFs::AssertFsSampleSignal()
{
    // Bug http://lwbugs/200693948
    auto &regs = m_pSub->Regs();
    Utility::DelayNS(500);
    regs.Write32(MODS_FUSE_CTRL_SW_FLOORSWEEP_SAMPLE, 0x1);
    Utility::DelayNS(500);
    regs.Write32(MODS_FUSE_CTRL_SW_FLOORSWEEP_SAMPLE, 0x0);
}

//------------------------------------------------------------------------------
void HopperFs::ApplyFloorsweepingSettings()
{
    GA10xFs::ApplyFloorsweepingSettings();

    // TODO : Command line arguments used in arch tests are naturally not going to be
    // compliant with the interaction between LWLW floorsweeping and per-link floorsweeping
    // Until arch test args can be fixed, MODS needs to auto adjust the lwlink perlink
    // arguments to avoid failures
#ifdef SIM_BUILD
    AdjustLwLinkPerlinkArgs();
#endif

    for (UINT32 i = 0; i < m_GpcCpcMasks.size(); i++)
    {
        if (m_FsGpcCpcParamPresent[i])
        {
            m_pSub->Regs().Write32Sync(MODS_FUSE_CTRL_OPT_CPC_GPC_DATA, i, m_GpcCpcMasks[i]);
        }
    }
    AmpereFs::ApplyFloorsweepingSettings(m_FsData);
    AssertFsSampleSignal();
}

//------------------------------------------------------------------------------
void HopperFs::RestoreFloorsweepingSettings()
{
    auto &regs = m_pSub->Regs();

    for (UINT32 i = 0; i < m_FsGpcCpcParamPresent.size(); i++)
    {
        if (m_FsGpcCpcParamPresent[i])
        {
            regs.Write32Sync(MODS_FUSE_CTRL_OPT_CPC_GPC, i, m_SavedGpcCpcMasks[i]);
        }
    }
    AmpereFs::RestoreFloorsweepingSettings(m_FsData);

    GA10xFs::RestoreFloorsweepingSettings();

    AssertFsSampleSignal();

}

//------------------------------------------------------------------------------
void HopperFs::ResetFloorsweepingSettings()
{
    auto &regs = m_pSub->Regs();

    UINT32 maxGpcs      = m_pSub->GetMaxGpcCount();
    UINT32 gpcCpc       = regs.Read32(MODS_FUSE_OPT_CPC_DISABLE);
    UINT32 numCpcPerGpc = GetMaxCpcCount();
    UINT32 cpcMask      = (1 << numCpcPerGpc) - 1;
    for (UINT32 gpcIndex = 0; gpcIndex < maxGpcs; gpcIndex++)
    {
        UINT32 cpc = (gpcCpc & cpcMask) >> (gpcIndex * numCpcPerGpc);
        regs.Write32Sync(MODS_FUSE_CTRL_OPT_CPC_GPC, gpcIndex, cpc);
        cpcMask <<= numCpcPerGpc;
    }
    AmpereFs::ResetFloorsweepingSettings(m_FsData);

    GA10xFs::ResetFloorsweepingSettings();

    AssertFsSampleSignal();

}

//------------------------------------------------------------------------------
void HopperFs::SaveFloorsweepingSettings()
{
    auto &regs = m_pSub->Regs();

    for (UINT32 i = 0; i < m_SavedGpcCpcMasks.size(); i++)
    {
        m_SavedGpcCpcMasks[i] = regs.Read32(MODS_FUSE_CTRL_OPT_CPC_GPC, i);
    }
    AmpereFs::SaveFloorsweepingSettings(&m_FsData);

    return GA10xFs::SaveFloorsweepingSettings();
}

//------------------------------------------------------------------------------
bool HopperFs::IsFloorsweepingValid() const
{
    bool bFsValid = true;

    UINT32 lwlinkMask = m_FsLwlinkParamPresent ? m_LwlinkMask : ~LwLinkMask();
    lwlinkMask &= ((1U << m_pSub->Regs().LookupAddress(MODS_SCAL_LITTER_NUM_IOCTRL)) - 1);

    if (lwlinkMask)
    {
        auto pLwLinkLinkFs = find_if(m_FsData.begin(), m_FsData.end(),
                                [] (const FsData & f)-> bool { return f.type == FST_LWLINK_LINK; } );
        MASSERT(pLwLinkLinkFs != m_FsData.end());

        if (pLwLinkLinkFs->bPresent && (pLwLinkLinkFs->mask != 0))
        {
            const UINT32 linksPerIoctrl = m_pSub->Regs().LookupAddress(MODS_LWL_LWLIPT_NUM_LINKS);
            const UINT32 ioctrlLinkMask = (1U << linksPerIoctrl) - 1;

            INT32 floorsweptIoctrl = Utility::BitScanForward(lwlinkMask);
            while (floorsweptIoctrl != -1)
            {
                const UINT32 reqDisableLinkMask =
                    ioctrlLinkMask << (floorsweptIoctrl * linksPerIoctrl);
                if ((pLwLinkLinkFs->mask & reqDisableLinkMask) != reqDisableLinkMask)
                {
                    bFsValid = false;
                    Printf(Tee::PriError,
                     "HopperFs : LWLW %d disabled, but link mask 0x%x in LWLW %d is enabled\n",
                     floorsweptIoctrl,
                     ~(pLwLinkLinkFs->mask >> (floorsweptIoctrl * linksPerIoctrl)) & ioctrlLinkMask,
                     floorsweptIoctrl);
                }
                floorsweptIoctrl = Utility::BitScanForward(lwlinkMask, floorsweptIoctrl + 1);
            }
        }
        else
        {
            // TODO : Command line arguments used in arch tests are naturally not going to be
            // compliant with the interaction between LWLW floorsweeping and per-link floorsweeping
            // Until arch test args can be fixed, ignore missing per-link args.  The args will
            // be fixed during application
#ifndef SIM_BUILD
            bFsValid = false;
#endif
            Printf(Tee::PriError,
                   "HopperFs : LWLW disable mask 0x%x, but all links left enabled\n",
                   lwlinkMask);
        }
    }

    return bFsValid ? GA10xFs::IsFloorsweepingValid() : false;
}

//------------------------------------------------------------------------------
RC HopperFs::PrintMiscFuseRegs()
{
    RC rc;
    CHECK_RC(GA10xFs::PrintMiscFuseRegs());

    auto &regs = m_pSub->Regs();

    Printf(Tee::PriNormal, " OPT_CPC_DIS : 0x%x\n",
                             regs.Read32(MODS_FUSE_OPT_CPC_DISABLE));
    AmpereFs::PrintMiscFuseRegs(m_FsData);

    return rc;
}

//------------------------------------------------------------------------------
RC HopperFs::AdjustDependentFloorsweepingArguments()
{
    AdjustLwLinkPerlinkArgs();

    return GA10xFs::AdjustDependentFloorsweepingArguments();
}

//------------------------------------------------------------------------------
RC HopperFs::GetFloorsweepingMasks
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
            string fsmaskName = Utility::StrPrintf("gpcCpc[%d]_mask", gpcNum);
            pFsMasks->insert(pair<string, UINT32>(fsmaskName, CpcMask(gpcNum)));
        }
        gpcNum++;
        gpcMask >>= 1;
    }
    pFsMasks->insert(pair<string, UINT32>("lwlink_link_mask", LwLinkLinkMask()));
    pFsMasks->insert(pair<string, UINT32>("c2c_mask", C2CMask()));

    CHECK_RC(GA10xFs::GetFloorsweepingMasks(pFsMasks));
    return rc;
}

//------------------------------------------------------------------------------
bool HopperFs::SupportsCpcMask(UINT32 hwGpcNum) const
{
    return m_pSub->Regs().IsSupported(MODS_FUSE_STATUS_OPT_CPC_GPC, hwGpcNum);
}

//------------------------------------------------------------------------------
UINT32 HopperFs::CpcMask(UINT32 hwGpcNum) const
{
    UINT32 hwCpcMask = 0;
    UINT32 cpcs      = m_pSub->Regs().Read32(MODS_FUSE_STATUS_OPT_CPC_GPC, hwGpcNum);

    hwCpcMask  = ((1 << GetMaxCpcCount()) - 1);
    hwCpcMask &= ~cpcs;

    return hwCpcMask;
}

//------------------------------------------------------------------------------
UINT32 HopperFs::LwLinkLinkMask() const
{
    const UINT32 maxLinks = m_pSub->Regs().LookupAddress(MODS_SCAL_LITTER_NUM_LWLINK);
    return ~m_pSub->Regs().Read32(MODS_FUSE_STATUS_OPT_PERLINK_DATA) & ((1 << maxLinks) - 1);
}

/* virtual */ UINT32 HopperFs::LwLinkLinkMaskDisable() const
{
    return m_pSub->Regs().Read32(MODS_FUSE_STATUS_OPT_PERLINK_DATA);
}

//------------------------------------------------------------------------------
UINT32 HopperFs::C2CMask() const
{
    // Even though there are 2 C2C blocks in PTOP_DEVICE_INFO there is only a single valid bit
    // in the C2C related FUSE registers and it floorsweeps both C2C blocks
    //
    // There is no SCAL_LITTER define and the STATUS/CTRL register data fields are all
    // 16 bits wide despite only using 1 bit
    return ~m_pSub->Regs().Read32(MODS_FUSE_STATUS_OPT_C2C_DATA) & 0x1;
}

/* virtual */ UINT32 HopperFs::C2CMaskDisable() const
{
    return m_pSub->Regs().Read32(MODS_FUSE_STATUS_OPT_C2C_DATA);
}

//------------------------------------------------------------------------------
UINT32 HopperFs::GetMaxCpcCount() const
{
    return m_pSub->Regs().LookupAddress(MODS_SCAL_LITTER_NUM_CPC_PER_GPC);
}

//------------------------------------------------------------------------------
void HopperFs::CheckIfPriRingInitIsRequired()
{
    for (UINT32 i = 0; i < m_FsGpcCpcParamPresent.size(); i++)
    {
        m_IsPriRingInitRequired = m_IsPriRingInitRequired || m_FsGpcCpcParamPresent[i];
    }

    GA10xFs::CheckIfPriRingInitIsRequired();
}

//------------------------------------------------------------------------------
void HopperFs::AdjustLwLinkPerlinkArgs()
{
    UINT32 lwlinkMask = m_FsLwlinkParamPresent ? m_LwlinkMask : ~LwLinkMask();
    lwlinkMask &= ((1U << m_pSub->Regs().LookupAddress(MODS_SCAL_LITTER_NUM_IOCTRL)) - 1);

    if (lwlinkMask)
    {
        auto pLwLinkLinkFs = find_if(m_FsData.begin(), m_FsData.end(),
                                [] (const FsData & f)-> bool { return f.type == FST_LWLINK_LINK; } );
        MASSERT(pLwLinkLinkFs != m_FsData.end());

        const UINT32 linksPerIoctrl = m_pSub->Regs().LookupAddress(MODS_LWL_LWLIPT_NUM_LINKS);
        const UINT32 ioctrlLinkMask = (1U << linksPerIoctrl) - 1;

        INT32 floorsweptIoctrl = Utility::BitScanForward(lwlinkMask);

        UINT32 lwlinkLinkMask = 0;
        while (floorsweptIoctrl != -1)
        {
            lwlinkLinkMask |= ioctrlLinkMask << (floorsweptIoctrl * linksPerIoctrl);
            floorsweptIoctrl = Utility::BitScanForward(lwlinkMask, floorsweptIoctrl + 1);
        }

        if ((lwlinkLinkMask & pLwLinkLinkFs->mask) != lwlinkLinkMask)
        {
            pLwLinkLinkFs->bPresent = true;
            pLwLinkLinkFs->mask |= lwlinkLinkMask;
            Printf(Tee::PriWarn, "Adjusting LwLinkLink mask to 0x%x\n", pLwLinkLinkFs->mask);
        }
    }
}

//------------------------------------------------------------------------------
/*static*/ bool HopperFs::PollPriRingCmdNone(void *pRegs)
{
    return static_cast<RegHal*>(pRegs)->Test32(MODS_PPRIV_SYS_PRI_RING_INIT_CMD_NONE);
}

/*static*/ bool HopperFs::PollPriRingStatus(void *pRegs)
{
    RegHal *pReg = static_cast<RegHal*>(pRegs);
    return !(pReg->Test32(MODS_PPRIV_SYS_PRI_RING_INIT_STATUS_DEAD) ||
             pReg->Test32(MODS_PPRIV_SYS_PRI_RING_INIT_STATUS_CMD_RDY));
}

// See https://confluence.lwpu.com/display/HOPCTXSWPRIHUB/Hopper+PRI_HUB#HopperPRI_HUB-HowdoesSWinitializetheprivring?
RC HopperFs::InitializePriRing_FirstStage(bool bInSetup)
{
    RC rc;
    RegHal &regs = m_pSub->Regs();

    // Per https://lwbugs/3135518, assert and de-assert the memsys reset before and after ring init
    if (regs.HasRWAccess(MODS_PMC_ELPG_ENABLE))
    {
        UINT32 regVal = regs.Read32(MODS_PMC_ELPG_ENABLE);
        regs.SetField(&regVal, MODS_PMC_ELPG_ENABLE_XBAR_DISABLED);
        regs.SetField(&regVal, MODS_PMC_ELPG_ENABLE_L2_DISABLED);
        regs.SetField(&regVal, MODS_PMC_ELPG_ENABLE_HUB_DISABLED);
        regs.Write32(MODS_PMC_ELPG_ENABLE, regVal);
    }

    if (regs.HasRWAccess(MODS_PPRIV_SYS_PRI_RING_INIT))
    {
        if (!regs.Test32(MODS_PPRIV_SYS_PRI_RING_INIT_STATUS_ALIVE_IN_SAFE_MODE))
        {
            // Step 1 (Part 1, pre FS if any)
            // Safe start the ring
            m_pSub->Regs().Write32(MODS_PPRIV_SYS_PRI_RING_INIT_CMD_SAFE_START);

            //  Step 2
            //  Poll for LW_PPRIV_SYS_PRI_RING_INIT_CMD_NONE
            CHECK_RC(POLLWRAP_HW(PollPriRingCmdNone, &regs, 1));

            // Step 3
            // Poll for LW_PPRIV_SYS_PRI_RING_INIT_STATUS != LW_PPRIV_SYS_PRI_RING_INIT_STATUS_DEAD &&
            // LW_PPRIV_SYS_PRI_RING_INIT_STATUS != LW_PPRIV_SYS_PRI_RING_INIT_STATUS_CMD_RDY
            CHECK_RC(POLLWRAP_HW(PollPriRingStatus, &regs, 1));
            if (regs.Test32(MODS_PPRIV_SYS_PRI_RING_INIT_STATUS_ERROR))
            {
                Printf(Tee::PriError, "Pri-ring initialization safe start failed\n");
                return RC::WAS_NOT_INITIALIZED;
            }
        }
    }
    return rc;
}

RC HopperFs::InitializePriRing()
{
    RC rc;
    RegHal &regs = m_pSub->Regs();

    if (regs.HasRWAccess(MODS_PPRIV_SYS_PRI_RING_INIT))
    {
        // Step 1 (part 2, post FS if any)
        // Enumerate and start the ring in normal-mode
        regs.Write32(MODS_PPRIV_SYS_PRI_RING_INIT_CMD_ENUMERATE_AND_START);

        // Step 2
        //  Poll for LW_PPRIV_SYS_PRI_RING_INIT_CMD_NONE
        CHECK_RC(POLLWRAP_HW(PollPriRingCmdNone, &regs, 1));

        // Step 3
        // Poll for LW_PPRIV_SYS_PRI_RING_INIT_STATUS != LW_PPRIV_SYS_PRI_RING_INIT_STATUS_DEAD &&
        // LW_PPRIV_SYS_PRI_RING_INIT_STATUS != LW_PPRIV_SYS_PRI_RING_INIT_STATUS_CMD_RDY
        CHECK_RC(POLLWRAP_HW(PollPriRingStatus, &regs, 1));
        if (regs.Test32(MODS_PPRIV_SYS_PRI_RING_INIT_STATUS_ERROR))
        {
            Printf(Tee::PriError, "Pri-ring initialization failed\n");
            return RC::WAS_NOT_INITIALIZED;
        }
    }

    if (regs.HasRWAccess(MODS_PMC_ELPG_ENABLE))
    {
        UINT32 regVal = regs.Read32(MODS_PMC_ELPG_ENABLE);
        regs.SetField(&regVal, MODS_PMC_ELPG_ENABLE_XBAR_ENABLED);
        regs.SetField(&regVal, MODS_PMC_ELPG_ENABLE_L2_ENABLED);
        regs.SetField(&regVal, MODS_PMC_ELPG_ENABLE_HUB_ENABLED);
        regs.Write32(MODS_PMC_ELPG_ENABLE, regVal);
    }

    return rc;
}

//-----------------------------------------------------------------------------
void HopperFs::PostRmShutdown(bool bInPreVBIOSSetup)
{
    if (GetFloorsweepingAffected() && GetRestoreFloorsweeping() && IsFloorsweepingAllowed())
    {
        // Pause polling interrupts to prevent pri accesses during pri ring initialization
        m_pSub->PausePollInterrupts();
        DEFER
        {
            m_pSub->ResumePollInterrupts();
        };

        if (m_IsPriRingInitRequired)
        {
            // Adding PRI fences to flush out potential inflight pri transactions
            // (See http://lwbugs/3156805/36)
            m_pSub->AddPriFence();

            // Bug 1601153: reset graphics before restoring gpc/fbp fs clusters
            GraphicsEnable(false);

            InitializePriRing_FirstStage(false);
        }

        // Use the FSP to lower the PLMs and allow write access to floorsweeping registers.
        // Note: The lowered PLMs don't take affect until after a FLReset.
        bool bRequestFsViaFsp = false;
        if (m_pSub->HasFeature(Device::Feature::GPUSUB_SUPPORTS_FSP) &&
           (Platform::GetSimulationMode() == Platform::Hardware) &&
            m_pSub->IsGFWBootStarted())
        {
            // Request floorsweeping via the FSP. This will:
            // - Lock out the chip
            // - Lower the PLMs of the FS regs
            // - Set the SW enable bit
            // - Set persistent scratch to indicate MODS floorsweeping should take affect on the
            //   next boot cycle.
            bRequestFsViaFsp = true;
            Fsp *pFsp = m_pSub->GetFspImpl();
            MASSERT(pFsp);
            pFsp->SendFspMsg(Fsp::DataUseCase::Floorsweep,
                             Fsp::DataFlag::OneShot,
                             nullptr);
        }

        RestoreFloorsweepingSettings();

        if (bRequestFsViaFsp)
        {   //Issue a reset to have the new setting take affect, but don't wait for the GFW boot
            // to complete because we are going to shut down the device.
            if (m_pSub->HasBug(3472315) && (Platform::GetSimulationMode() == Platform::Hardware))
            {
                if (m_pSub->HotReset(TestDevice::FundamentalResetOptions::Disable) != RC::OK)
                {
                    Printf(Tee::PriError, "HOT RESET FAILED! GPU may need to be reset!\n");
                }
            }
            else
            {
                if (m_pSub->FLReset() != RC::OK)
                {
                    Printf(Tee::PriError, "FLR FAILED! GPU may need to be reset!\n");
                }
            }
        }
        else
        {
            if (!bInPreVBIOSSetup &&
                (Platform::GetSimulationMode() == Platform::Hardware) &&
                m_IsPriRingInitRequired)
            {
                if (InitializePriRing() != RC::OK)
                {
                    Printf(Tee::PriError, "Pri ring init FAILED! GPU needs to be reset!\n");
                }

                // Deassert graphics reset afterwards
                GraphicsEnable(true);
            }
        }
    }
}

//------------------------------------------------------------------------------
#ifdef INCLUDE_FSLIB
void HopperFs::GetFsLibMask(FsLib::GpcMasks *pGpcMasks) const
{
    MASSERT(pGpcMasks);
    GA10xFs::GetFsLibMask(pGpcMasks);

    const UINT32 maxGpcs = m_pSub->GetMaxGpcCount();
    const UINT32 maxCpcs = GetMaxCpcCount();
    for (UINT32 gpcNum = 0; gpcNum < maxGpcs; gpcNum++)
    {
        bool isGpcDisabled    = (pGpcMasks->gpcMask & (1 << gpcNum)) != 0;
        UINT32 defaultCpcMask = isGpcDisabled ? ~0U : ~CpcMask(gpcNum);

        pGpcMasks->cpcPerGpcMasks[gpcNum] = (m_FsGpcCpcParamPresent[gpcNum] ?
                                             m_GpcCpcMasks[gpcNum] :
                                             defaultCpcMask) &
                                            ((1 << maxCpcs) - 1);
    }
}
#endif

//------------------------------------------------------------------------------
RC HopperFs::SwReset(bool bReinitPriRing)
{
    RC rc;

    // Pause polling interrupts to prevent pri accesses during pri ring initialization
    m_pSub->PausePollInterrupts();
    DEFER
    {
        m_pSub->ResumePollInterrupts();
    };

    if (m_pSub->HasBug(3472315) && (Platform::GetSimulationMode() == Platform::Hardware))
    {   
        // Use HotReset w/o coupling Fundamental Reset
        CHECK_RC(m_pSub->HotReset(TestDevice::FundamentalResetOptions::Disable));
    }
    else
    {
        // Hopper onwards the FLR assertion need not be precondition by quiescing the traffic and
        // generates dummy completion to engines or UR to host as part of FLR handling
        CHECK_RC(m_pSub->FLReset());
    }

    if (bReinitPriRing)
    {
        CHECK_RC(InitializePriRing_FirstStage(false));
        CHECK_RC(InitializePriRing());
    }

    return rc;
}

//------------------------------------------------------------------------------
RC HopperFs::ApplyFloorsweepingChanges(bool bInPreVBIOSSetup)
{
    RC rc;

    // Pause polling interrupts to prevent pri accesses during pri ring initialization
    m_pSub->PausePollInterrupts();
    DEFER
    {
        m_pSub->ResumePollInterrupts();
    };

    CHECK_RC(GA10xFs::ApplyFloorsweepingChanges(bInPreVBIOSSetup));
    return rc;
}

//------------------------------------------------------------------------------
RC HopperFs::AdjustPreGpcFloorsweepingArguments(UINT32 *pTpcMaskModified)
{
    RC rc;
    CHECK_RC(AdjustCpcFloorsweepingArguments(pTpcMaskModified));
    CHECK_RC(GA10xFs::AdjustPreGpcFloorsweepingArguments(pTpcMaskModified));
    return rc;
}

//------------------------------------------------------------------------------
RC HopperFs::AdjustPesFloorsweepingArguments(UINT32 *pTpcMaskModified)
{
    return RC::OK;
}

//------------------------------------------------------------------------------
RC HopperFs::AdjustCpcFloorsweepingArguments(UINT32 *pTpcMaskModified)
{
    MASSERT(pTpcMaskModified);

    const UINT32 maxGpcs = m_pSub->GetMaxGpcCount();
    const UINT32 maxTpcs = m_pSub->GetMaxTpcCount();
    const UINT32 maxCpcs = GetMaxCpcCount();

    *pTpcMaskModified = 0;
    if (maxCpcs != 0)
    {
        vector<UINT32> tpcCpcConfig;
        Utility::GetUniformDistribution(maxCpcs, maxTpcs, &tpcCpcConfig);

        for (UINT32 gpcNum = 0; gpcNum < maxGpcs; gpcNum++)
        {
            bool cpcMaskModified = false;

            // GPC disabled, disable all underlying CPCs
            if (m_FsGpcParamPresent && ((m_GpcMask >> gpcNum) & 1))
            {
                if (!m_FsGpcCpcParamPresent[gpcNum])
                {
                    m_GpcCpcMasks[gpcNum] = ~0U;
                    m_FsGpcCpcParamPresent[gpcNum] = true;
                    cpcMaskModified = true;
                }
            }

            // If CPC is disabled, disable all underlying TPCs
            if (m_FsGpcCpcParamPresent[gpcNum] && !m_FsGpcTpcParamPresent[gpcNum])
            {
                UINT32 cpcMask = m_GpcCpcMasks[gpcNum];
                UINT32 tpcCpcShift = 0;
                for (UINT32 cpcNum = 0; cpcNum < maxCpcs; cpcNum++)
                {
                    UINT32 cpcConfigMask = (1 << tpcCpcConfig[cpcNum]) - 1;
                    if ((cpcMask >> cpcNum) & 1)
                    {
                        // Disable all TPCs in the CPC
                        m_GpcTpcMasks[gpcNum] |= cpcConfigMask << tpcCpcShift;
                        *pTpcMaskModified |= (1 << gpcNum);
                    }
                    tpcCpcShift += tpcCpcConfig[cpcNum];
                }
            }

            // If TPCs connected to a CPC are disabled, disable the CPC
            if (!m_FsGpcCpcParamPresent[gpcNum] && m_FsGpcTpcParamPresent[gpcNum])
            {
                UINT32 tpcGpcMask = m_GpcTpcMasks[gpcNum];
                UINT32 tpcCpcShift = 0;
                for (UINT32 cpcNum = 0; cpcNum < maxCpcs; cpcNum++)
                {
                    UINT32 cpcConfigMask = (1 << tpcCpcConfig[cpcNum]) - 1;
                    UINT32 tpcMask = (tpcGpcMask >> tpcCpcShift) &
                                      cpcConfigMask;
                    if (tpcMask == cpcConfigMask)
                    {
                        // Disable the CPC
                        m_GpcCpcMasks[gpcNum] |= 1 << cpcNum;
                        cpcMaskModified = true;
                    }
                    tpcCpcShift += tpcCpcConfig[cpcNum];
                }
            }

            if (cpcMaskModified)
            {
                m_FsGpcCpcParamPresent[gpcNum] = true;
                Printf(Tee::PriWarn, "Adjusting CPC mask[%d] to 0x%x\n",
                                      gpcNum, m_GpcCpcMasks[gpcNum]);
            }
        }
    }

    return RC::OK;
}

//------------------------------------------------------------------------------
RC HopperFs::AdjustL2NocFloorsweepingArguments()
{
    RegHal &regs = m_pSub->Regs();

    const UINT32 numLtcPerFbp      = regs.Read32(MODS_PTOP_SCAL_NUM_LTC_PER_FBP);
    const UINT32 fullLtcMask       = (1 << numLtcPerFbp) - 1;
    const UINT32 numSlicesPerLtc   = GetMaxSlicePerLtcCount();
    const UINT32 numSlicesPerFbp   = numSlicesPerLtc * numLtcPerFbp;
    const UINT32 fullSliceMask     = (1U << numSlicesPerFbp) - 1;

    UINT32 newFbMask = 0;
    const UINT32 maxFbps = m_pSub->GetMaxFbpCount();
    for (UINT32 fbNum = 0; fbNum < maxFbps; fbNum++)
    {
        // FBP NoC pair
        INT32 pairedFbp = m_pSub->GetFbpNocPair(fbNum);
        MASSERT(pairedFbp >= 0);

        const bool bIsFbpDisabled = (m_FbMask & (1 << fbNum)) != 0;
        if (bIsFbpDisabled)
        {
            const bool bIsPairFbpDisabled = (m_FbMask & (1 << pairedFbp)) != 0;
            if (!bIsPairFbpDisabled)
            {
                Printf(Tee::PriWarn, "Disabling FBP %u since its L2 NOC pair %u is disabled\n",
                                      static_cast<UINT32>(pairedFbp), fbNum);
                newFbMask |= (1 << pairedFbp);
            }
        }
        // LTC Noc Pair
        else if (m_FsFbpL2ParamPresent[fbNum] &&
                (m_FbpL2Masks[fbNum] != 0) && (m_FbpL2Masks[fbNum] != fullLtcMask) &&
                !m_FsFbpL2ParamPresent[pairedFbp])
        {
            UINT32 pairedLtcMask = 0;
            for (UINT32 ltcNum = 0; ltcNum < numLtcPerFbp; ltcNum++)
            {
                bool bIsLtcDisabled = (m_FbpL2Masks[fbNum] & (1 << ltcNum)) != 0;
                if (bIsLtcDisabled)
                {
                    INT32 pairedLtc = m_pSub->GetFbpLtcNocPair(fbNum, ltcNum);
                    pairedLtcMask |= 1 << pairedLtc;
                }
            }
            m_FbpL2Masks[pairedFbp] = pairedLtcMask;
            m_FsFbpL2ParamPresent[pairedFbp] = true;
            Printf(Tee::PriWarn, "Adjusting L2 mask[%d] to 0x%x since paired L2[%d] is set to 0x%x\n",
                                  pairedFbp, pairedLtcMask, fbNum, m_FbpL2Masks[fbNum]);
        }
        // L2 slice NoC Pair
        // Note : Not taking into account max 2 pair FSing rules
        else if (m_FsFbpL2SliceParamPresent[fbNum] &&
                 (m_FbpL2SliceMasks[fbNum] != 0) && (m_FbpL2SliceMasks[fbNum] != fullSliceMask) &&
                 !m_FsFbpL2SliceParamPresent[pairedFbp])
        {
            UINT32 pairedSliceMask = 0;
            for (UINT32 sliceNum = 0; sliceNum < numSlicesPerFbp; sliceNum++)
            {
                bool bIsSliceDisabled = (m_FbpL2SliceMasks[fbNum] & (1 << sliceNum)) != 0;
                if (bIsSliceDisabled)
                {
                    INT32 pairedSlice = m_pSub->GetFbpSliceNocPair(fbNum, sliceNum);
                    pairedSliceMask  |= 1 << pairedSlice;
                }
            }
            m_FbpL2SliceMasks[pairedFbp] = pairedSliceMask;
            m_FsFbpL2SliceParamPresent[pairedFbp] = true;
            Printf(Tee::PriWarn, "Adjusting L2slice mask[%d] to 0x%x since paired L2slice[%d] is set to 0x%x\n",
                                  pairedFbp, pairedSliceMask, fbNum, m_FbpL2SliceMasks[fbNum]);
        }
    }
    m_FbMask |= newFbMask;

    return AdjustFbpFloorsweepingArguments();
}

//------------------------------------------------------------------------------
RC HopperFs::FixFbpFloorsweepingMasks
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

    RC rc;
    CHECK_RC(GA10xFs::FixFbpFloorsweepingMasks(fbNum,
                                               pFbMaskModified, pFbioMaskModified,
                                               pL2MaskModified, pL2SliceMaskModified,
                                               pFsAffected));

    // If FBP is disabled, also disable the paired FBP
    if ((m_FbMask >> fbNum) & 1)
    {
        INT32 pairedFbp = m_pSub->GetFbpNocPair(fbNum);
        MASSERT(pairedFbp >= 0);
        m_FbMask |= (1 << pairedFbp);

        // Assuming this function is called per FBP from 0->fbNum
        if ((static_cast<UINT32>(pairedFbp) < fbNum) &&
            !((m_FbMask >> pairedFbp) & 1))
        {
            CHECK_RC(GA10xFs::FixFbpFloorsweepingMasks(static_cast<UINT32>(pairedFbp),
                                                       pFbMaskModified, pFbioMaskModified,
                                                       pL2MaskModified, pL2SliceMaskModified,
                                                       pFsAffected));
        }
    }

    return rc;
}

//------------------------------------------------------------------------------
RC HopperFs::FixFbpLtcFloorsweepingMasks
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

    RC rc;
    auto &regs = m_pSub->Regs();

    CHECK_RC(GA10xFs::FixFbpLtcFloorsweepingMasks(fbNum,
                                                  pL2MaskModified, pL2SliceMaskModified,
                                                  pFsAffected));

    if (regs.IsSupported(MODS_FUSE_CTRL_OPT_L2SLICE_FBP, fbNum))
    {
        const UINT32 numLtcPerFbp = regs.Read32(MODS_PTOP_SCAL_NUM_LTC_PER_FBP);
        UINT32 numL2SlicesPerLtc = 0;
        UINT32 l2SliceMask = 0;
        if (regs.IsSupported(MODS_FUSE_CTRL_OPT_L2SLICE_FBP, fbNum))
        {
            numL2SlicesPerLtc = GetMaxSlicePerLtcCount();
            l2SliceMask = (1U << numL2SlicesPerLtc) - 1;
        }

        // Disable paired LTCs and corresponding slices
        INT32 pairedFbp = m_pSub->GetFbpNocPair(fbNum);
        MASSERT(pairedFbp >= 0);
        for (UINT32 ltcIdx = 0; ltcIdx < numLtcPerFbp; ltcIdx++)
        {
            if ((m_FbpL2Masks[fbNum] >> ltcIdx) & 1)
            {
                INT32 pairedLtc = m_pSub->GetFbpLtcNocPair(fbNum, ltcIdx);
                MASSERT(pairedLtc >= 0);

                m_FbpL2Masks[pairedFbp] |= (1 << pairedLtc);
                m_FbpL2SliceMasks[pairedFbp] |=
                    (l2SliceMask << (pairedLtc * numL2SlicesPerLtc));

                *pL2MaskModified |= (1U << pairedFbp);
                *pL2SliceMaskModified |= (1U << pairedFbp);
                *pFsAffected = true;
            }
        }
    }
    return rc;
}

//------------------------------------------------------------------------------
RC HopperFs::FixHalfFbpaFloorsweepingMasks
(
    bool *pFsAffected
)
{
    // No half FBPA FSing on GH100
    return RC::OK;
}

//------------------------------------------------------------------------------
RC HopperFs::PopulateFsInforom
(
    LW90E7_CTRL_RPR_WRITE_OBJECT_PARAMS *pIfrParams
) const
{
    RC rc;
    MASSERT(pIfrParams);

    RegHal &regs = m_pSub->Regs();

    UINT32 maxIfrDataCount;
    CHECK_RC(m_pSub->GetInfoRomRprMaxDataCount(&maxIfrDataCount));

    for (UINT32 gpcNum = 0; gpcNum < m_GpcCpcMasks.size(); gpcNum++)
    {
        if (!m_FsGpcCpcParamPresent[gpcNum])
            continue;

        CHECK_RC(PopulateFsInforomEntry(pIfrParams,
                                        regs.LookupAddress(MODS_FUSE_CTRL_OPT_CPC_GPC, gpcNum),
                                        m_GpcCpcMasks[gpcNum],
                                        maxIfrDataCount));
    }

    auto pLwLinkLinkFs = find_if(m_FsData.begin(), m_FsData.end(),
                            [] (const FsData & f)-> bool { return f.type == FST_LWLINK_LINK; } );
    if ((pLwLinkLinkFs != m_FsData.end()) && pLwLinkLinkFs->bPresent)
    {
        CHECK_RC(PopulateFsInforomEntry(pIfrParams,
                                        regs.LookupAddress(regs.ColwertToAddress(pLwLinkLinkFs->ctrlField)),
                                        pLwLinkLinkFs->mask,
                                        maxIfrDataCount));
    }

    CHECK_RC(GA10xFs::PopulateFsInforom(pIfrParams));

    return rc;
}

/* virtual */ void HopperFs::PopulateGpcDisableMasks(JsonItem *pJsi) const
{
    MASSERT(pJsi);
    GA10xFs::PopulateGpcDisableMasks(pJsi);

    RegHal &regs = m_pSub->Regs();
    const UINT32 maxGpcs = m_pSub->GetMaxGpcCount();
    const UINT32 maxCpcs = GetMaxCpcCount();
    string cpcDsblMask;

    for (UINT32 gpcNum = 0; gpcNum < maxGpcs; gpcNum++)
    {
        cpcDsblMask += Utility::StrPrintf("%s0x%x'",
                        (gpcNum == 0 ? "'" : ", '"),
                        (regs.Read32(MODS_FUSE_STATUS_OPT_CPC_GPC, gpcNum) & ((1 << maxCpcs) - 1))
                    );
    }

    pJsi->SetField("OPT_CPC_DISABLE", ("[" +  cpcDsblMask + "]").c_str());
}

/* virtual */ void HopperFs::PopulateMiscDisableMasks(JsonItem *pJsi) const
{
    MASSERT(pJsi);
    GA10xFs::PopulateMiscDisableMasks(pJsi);

    RegHal &regs = m_pSub->Regs();
    if (regs.IsSupported(MODS_FUSE_STATUS_OPT_PERLINK))
    {
        pJsi->SetField("OPT_PERLINK_DISABLE",
            Utility::StrPrintf("0x%x", LwLinkLinkMaskDisable()).c_str());
    }
    pJsi->SetField("OPT_C2C_DISABLE", Utility::StrPrintf("0x%x", C2CMaskDisable()).c_str());
}

//------------------------------------------------------------------------------
void HopperFs::SetVbiosControlsFloorsweep
(
    bool vbiosControlsFloorsweep
)
{
    if (Platform::GetSimulationMode() != Platform::Hardware)
    {
        // Writing scratch registers seems to offend fmodel & RTL.
        return;
    }

    // Use bit 0 of LW_XTL_EP_PRI_HOT_RESET_SCRATCH_0 to indicate if
    // VBIOS should overrride the floorsweeping so or not
    static constexpr UINT32 s_VbiosFsMask = 0x1; 
    UINT32 regVal = m_pSub->Regs().Read32(MODS_XTL_EP_PRI_HOT_RESET_SCRATCH_0);
    if (vbiosControlsFloorsweep)
    {
        Printf(Tee::PriDebug,
               "Setting LW_XTL_EP_PRI_HOT_RESET_SCRATCH_0[0] to 0 -- vbios should override FS.\n");
        regVal &= ~s_VbiosFsMask;
    }
    else
    {
        Printf(Tee::PriDebug,
               "Setting LW_XTL_EP_PRI_HOT_RESET_SCRATCH_0[0] to 1 -- vbios should leave FS alone.\n");
        regVal |= s_VbiosFsMask;
    }
    m_pSub->Regs().Write32(MODS_XTL_EP_PRI_HOT_RESET_SCRATCH_0, regVal);
}


//------------------------------------------------------------------------------
CREATE_GPU_FS_FUNCTION(HopperFs);
