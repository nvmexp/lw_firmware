/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2019-2022 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#include "hoppergpu.h"

#include "core/include/display.h"
#include "core/include/jscript.h"
#include "core/include/platform.h"
#include "core/include/utility.h"
#include "ctrl/ctrl2080.h"  // for LW2080_CTRL_GPU_GET_ENGINE_RUNLIST_PRI_BASE
#include "device/interface/lwlink.h"
#include "device/interface/pcie.h"

#include "gpu/include/floorsweepimpl.h"
#include "gpu/include/gpudev.h"
#include "gpu/js_gpu.h"
#include "gpu/pcie/turingpcicfg.h"
#include "gpu/perf/perfutil.h"
#include "gpu/reghal/smcreghal.h"
#include "gpu/utility/lwlink/lwlinkdev.h"
#include "gpu/utility/scopereg.h"
#include "gpu/utility/vbios.h"
#include "gpu/utility/fsp.h"

#include "hopper/gh100/dev_mmu.h"
#include "hopper/gh100/dev_runlist.h"
#include "hopper/gh100/dev_xtl_ep_pri.h"
#include "hopper/gh100/hwproject.h"
#include "hopper/gh100/kind_macros.h"
#include "rmlsfm.h"

#include <numeric>             // for std::accumulate()
#include <functional>          // for logical_or
#include <algorithm>           // for min

namespace
{
    const UINT32 DEFAULT_FB_DUAL_REQUEST = ~0U;
    const UINT32 DEFAULT_FB_CLAMSHELL = ~0U;
    const UINT32 DEFAULT_HBM_DUAL_RANKS = ~0U;
    const UINT32 DEFAULT_HBM_NUM_SIDS = 0;
    const UINT32 DEFAULT_FB_MAX_ROW_COUNT = 0;
}

constexpr UINT32 HopperGpuSubdevice::m_FbpsPerSite;

HopperGpuSubdevice::HopperGpuSubdevice
(
    const char *       name,
    Gpu::LwDeviceId    deviceId,
    const PciDevInfo * pPciDevInfo
) :
    GpuSubdevice(name, deviceId, pPciDevInfo)
   ,m_FbDualRanks(DEFAULT_HBM_DUAL_RANKS)
   ,m_FbNumSids(DEFAULT_HBM_NUM_SIDS)
   ,m_FbRowAMaxCount(DEFAULT_FB_MAX_ROW_COUNT)
   ,m_FbRowBMaxCount(DEFAULT_FB_MAX_ROW_COUNT)
   ,m_FbMc2MasterMask(0)
   ,m_FbDualRequest(DEFAULT_FB_DUAL_REQUEST)
   ,m_FbClamshell(DEFAULT_FB_CLAMSHELL)
   ,m_IsSmcEnabled(false)
   ,m_FbBanksPerRowBg(0)
   ,m_LwLinksPerIoctl(0)
{
    ADD_NAMED_PROPERTY(FbMc2MasterMask, UINT32, 0);
    // On Hopper+ all interrupts are using the message protocol that utilizes a log64 tree
    // structure for reporting interrupt events.
    RegHal& regs = GpuSubdevice::Regs();
    IntRegClear();
    for (UINT32 ii = 0;
         regs.IsSupported(MODS_VIRTUAL_FUNCTION_PRIV_CPU_INTR_LEAF, ii);
         ++ii)
    {
        IntRegAdd(MODS_VIRTUAL_FUNCTION_PRIV_CPU_INTR_TOP, ii / 64,
                  1U << ((ii % 64) / 2),
                  MODS_VIRTUAL_FUNCTION_PRIV_CPU_INTR_LEAF, ii,
                  0xffffffffU, 0xffffffffU);
    }
    if (regs.IsSupported(MODS_LWL_LWLIPT_NUM_LINKS))
        m_LwLinksPerIoctl = regs.LookupAddress(MODS_LWL_LWLIPT_NUM_LINKS);

    if (!m_pFspImpl.get())
    {
        m_pFspImpl = make_unique<Fsp>(this);
    }
}

///
// Falcon's UCODE status is stored in the SCTL register. Since we have to deal
// with many falcons copying the bitfield of UCODE level
//
#define LW_FALCON_SCTL_UCODE_LEVEL                         5:4
#define LW_FALCON_SCTL_UCODE_LEVEL_P0                      0x00000000
#define LW_FALCON_SCTL_UCODE_LEVEL_P1                      0x00000001
#define LW_FALCON_SCTL_UCODE_LEVEL_P2                      0x00000002
#define LW_FALCON_SCTL_UCODE_LEVEL_P3                      0x00000003

//------------------------------------------------------------------------------
// Load the chip specific Method Macro Expansion (MME) data
RC HopperGpuSubdevice::LoadMmeShadowData()
{
    Printf(Tee::PriWarn, "GPU has unknown MME methods and is not supported on MODS.\n");
    return RC::UNSUPPORTED_FUNCTION;
}

//------------------------------------------------------------------------------
void HopperGpuSubdevice::CreatePteKindTable()
{
    m_PteKindTable.clear();
    m_PteKindTable.push_back(PTE_KIND_TABLE_ENTRY(GENERIC_MEMORY));
    m_PteKindTable.push_back(PTE_KIND_TABLE_ENTRY(GENERIC_MEMORY_COMPRESSIBLE));
    m_PteKindTable.push_back(PTE_KIND_TABLE_ENTRY(GENERIC_MEMORY_COMPRESSIBLE_DISABLE_PLC));
    m_PteKindTable.push_back(PTE_KIND_TABLE_ENTRY(S8_COMPRESSIBLE_DISABLE_PLC));
    m_PteKindTable.push_back(PTE_KIND_TABLE_ENTRY(Z16_COMPRESSIBLE_DISABLE_PLC));
    m_PteKindTable.push_back(PTE_KIND_TABLE_ENTRY(S8Z24_COMPRESSIBLE_DISABLE_PLC));
    m_PteKindTable.push_back(PTE_KIND_TABLE_ENTRY(Z24S8_COMPRESSIBLE_DISABLE_PLC));
    m_PteKindTable.push_back(PTE_KIND_TABLE_ENTRY(ZF32_X24S8_COMPRESSIBLE_DISABLE_PLC));
    m_PteKindTable.push_back(PTE_KIND_TABLE_ENTRY(S8));
    m_PteKindTable.push_back(PTE_KIND_TABLE_ENTRY(Z16));
    m_PteKindTable.push_back(PTE_KIND_TABLE_ENTRY(Z24S8));
    m_PteKindTable.push_back(PTE_KIND_TABLE_ENTRY(S8Z24));
    m_PteKindTable.push_back(PTE_KIND_TABLE_ENTRY(ZF32_X24S8));
    m_PteKindTable.push_back(PTE_KIND_TABLE_ENTRY(PITCH));
    m_PteKindTable.push_back(PTE_KIND_TABLE_ENTRY(INVALID));
    m_PteKindTable.push_back(PTE_KIND_TABLE_ENTRY(SMSKED_MESSAGE));
}

//PCIe Gen5 visibility to all derived classes

/*virtual*/ unique_ptr<ScopedRegister> HopperGpuSubdevice::EnableScopedBar1PhysMemAccess()
{
    unique_ptr<ScopedRegister> origReg = make_unique<ScopedRegister>(this, MODS_VBUS_VF_BAR1_BLOCK);
    GpuSubdevice::Regs().Write32(MODS_VBUS_VF_BAR1_BLOCK_MODE_PHYSICAL);
    return origReg;
}

/* virtual */ unique_ptr<ScopedRegister> HopperGpuSubdevice::ProgramScopedPraminWindow(UINT64 addr, Memory::Location loc)
{
    MASSERT(loc == Memory::Fb);
    unique_ptr<ScopedRegister> origReg = make_unique<ScopedRegister>(this, MODS_XAL_EP_BAR0_WINDOW);
    auto newVal = GpuSubdevice::Regs().SetField(MODS_XAL_EP_BAR0_WINDOW_BASE, static_cast<UINT32>(addr >> 16));
    GpuSubdevice::Regs().Write32(MODS_XAL_EP_BAR0_WINDOW, newVal);
    return origReg;
}

//------------------------------------------------------------------------------
bool HopperGpuSubdevice::FaultBufferOverflowed() const
{
    return !Regs().Test32(
            MODS_PFB_PRI_MMU_FAULT_STATUS_REPLAYABLE_OVERFLOW_RESET);
}

//------------------------------------------------------------------------------
void HopperGpuSubdevice::ApplyMemCfgProperties(bool inPreVBIOSSetup)
{
    JavaScriptPtr pJs;
    RegHal& regs = GpuSubdevice::Regs();
    UINT32 extraPfbCfg0 = 0, extraPfbCfg0Mask = 0;
    //TODO: Does this even make sense for Hopper? Hopper only has HBM2E & HBM3 memory.
    if (GetFbGddr5x8Mode())
    {
        extraPfbCfg0     |= regs.SetField(MODS_PFB_FBPA_CFG0_CLAMSHELL_X16);
        extraPfbCfg0Mask |= regs.LookupMask(MODS_PFB_FBPA_CFG0_CLAMSHELL);

        extraPfbCfg0     |= regs.SetField(MODS_PFB_FBPA_CFG0_X8_ENABLED);
        extraPfbCfg0Mask |= regs.LookupMask(MODS_PFB_FBPA_CFG0_X8);
    }

    GetMemCfgOverrides();
    bool reqFilterOverrides = false;
    if (m_InpPerFbpRowBits.size() == 1)
    {
        //We will be using FbRowBits js arg for setting the broadcast reg
        //This will override the value given by Mods argument -fb_row <val>
        Printf(Tee::PriLow,
                "Warning: Possibly overriding the value given to -fb_row\n");
        pJs->SetProperty(Gpu_Object, Gpu_FbRowBits, m_InpPerFbpRowBits[0]);
    }

    reqFilterOverrides = (m_InpPerFbpRowBits.size() > 1) ||
                         (m_FbDualRanks != DEFAULT_HBM_DUAL_RANKS) ||
                         (m_FbNumSids != DEFAULT_HBM_NUM_SIDS) ||
                         (m_FbRowAMaxCount != DEFAULT_FB_MAX_ROW_COUNT) ||
                         (m_FbRowBMaxCount != DEFAULT_FB_MAX_ROW_COUNT) ||
                         m_FbMc2MasterMask ||
                         (m_FbDualRequest != DEFAULT_FB_DUAL_REQUEST) ||
                         (m_FbClamshell != DEFAULT_FB_CLAMSHELL) ||
                         m_FbBanksPerRowBg;

    // On hardware or emulation, these are real DRAMs, and you can't just tweak
    // the settings willy-nilly.  You need to use an appropriate VBIOS instead.
    //
    // FB MC2 Master however can be set on hardware so do not check it here
    if (((m_InpPerFbpRowBits.size() > 1) ||
         (m_FbDualRanks != DEFAULT_HBM_DUAL_RANKS) ||
         (m_FbNumSids != DEFAULT_HBM_NUM_SIDS) ||
         (m_FbRowAMaxCount != DEFAULT_FB_MAX_ROW_COUNT) ||
         (m_FbRowBMaxCount != DEFAULT_FB_MAX_ROW_COUNT) ||
         (m_FbDualRequest != DEFAULT_FB_DUAL_REQUEST) ||
         (m_FbClamshell != DEFAULT_FB_CLAMSHELL) ||
         m_FbBanksPerRowBg) &&
        (Platform::GetSimulationMode() == Platform::Hardware))
    {
        Printf(Tee::PriError,
               "Cannot override FB settings on emulation/hardware\n");
        MASSERT(!"Generic assertion failure<refer to previous message>.");
        return;
    }

    GpuSubdevice::ApplyMemCfgProperties_Impl(
            inPreVBIOSSetup, extraPfbCfg0, extraPfbCfg0Mask,
            reqFilterOverrides);

    if (inPreVBIOSSetup || !reqFilterOverrides)
        return;

    //Take care of override registers that are not in the
    //list of Arguments supplied to baseclass Override method
    UINT32 fbpIdx = 0, activeFbps = 0;
    const UINT32 stride = regs.LookupAddress(MODS_FBPA_PRI_STRIDE);
    UINT32 cfg1Reg = regs.LookupAddress(MODS_PFB_FBPA_0_CFG1);
    const UINT32 cfg1RowBaseVal =
        10 - regs.LookupValue(MODS_PFB_FBPA_CFG1_ROWA_10);

    activeFbps = Utility::CountBits(GetFsImpl()->FbMask());
    for (; (fbpIdx < activeFbps) && (fbpIdx < m_InpPerFbpRowBits.size());
         fbpIdx++)
    {
        //Per Requested FBPA
        if (!m_InpPerFbpRowBits[fbpIdx])
        {
            continue;
        }
        UINT32 cfg1val = m_InpPerFbpRowBits[fbpIdx];
        UINT32 pfbCfg1 = RegRd32(cfg1Reg);

        regs.SetField(&pfbCfg1, MODS_PFB_FBPA_CFG1_ROWA,
                      cfg1val - cfg1RowBaseVal);
        regs.SetField(&pfbCfg1, MODS_PFB_FBPA_CFG1_ROWB,
                      cfg1val - cfg1RowBaseVal);
        RegWr32(cfg1Reg, pfbCfg1);
        cfg1Reg += stride;
    }

    if (m_FbNumSids != DEFAULT_HBM_NUM_SIDS)
    {
        UINT32 regValue = regs.Read32(MODS_PFB_FBPA_HBM_CFG0);
        switch (m_FbNumSids)
        {
            case 1:
                regs.SetField(&regValue, MODS_PFB_FBPA_HBM_CFG0_NUM_SIDS_1);
                break;
            case 2:
                regs.SetField(&regValue, MODS_PFB_FBPA_HBM_CFG0_NUM_SIDS_2);
                break;
            case 3:
                regs.SetField(&regValue, MODS_PFB_FBPA_HBM_CFG0_NUM_SIDS_3);
                break;
            default:
                Printf(Tee::PriError,
                       "invalid -fb_hbm_num_sids argument value %u.\n",
                       m_FbNumSids);
                MASSERT(!"Generic assertion failure<refer to previous message>.");
                break;
        }
        regs.Write32(MODS_PFB_FBPA_HBM_CFG0, regValue);
    }
    else if (m_FbDualRanks != DEFAULT_HBM_DUAL_RANKS)
    {
        // TODO: m_FbDualRanks is deprecated in Hopper+.
        // Remove once tests have transitioned to use m_FbNumSids
        UINT32 regValue = regs.Read32(MODS_PFB_FBPA_HBM_CFG0);
        regs.SetField(&regValue, MODS_PFB_FBPA_HBM_CFG0_DUAL_RANK_ENABLE);
        switch (m_FbDualRanks)
        {
            case 0:
                // Zero means no user argument specified, so keep the default
                // register value.
                break;
            case 8:
                regs.SetField(&regValue,
                                MODS_PFB_FBPA_HBM_CFG0_DUAL_RANK_BANK_8);
                break;
            case 16:
                regs.SetField(&regValue,
                                MODS_PFB_FBPA_HBM_CFG0_DUAL_RANK_BANK_16);
                break;
            default:
                Printf(Tee::PriError,
                       "invalid -fb_hbm_ranks_per_bank argument value %u.\n",
                       m_FbDualRanks);
                MASSERT(!"Generic assertion failure<refer to previous message>.");
                break;
        }
        regs.Write32(MODS_PFB_FBPA_HBM_CFG0, regValue);
    }

    if (m_FbRowAMaxCount != DEFAULT_FB_MAX_ROW_COUNT)
    {
        regs.Write32(MODS_PFB_FBPA_CFG2_ROWA_MAX_ROW_COUNT, m_FbRowAMaxCount);
    }

    if (m_FbRowBMaxCount != DEFAULT_FB_MAX_ROW_COUNT)
    {
        regs.Write32(MODS_PFB_FBPA_CFG3_ROWB_MAX_ROW_COUNT, m_FbRowBMaxCount);
    }

    if (IsFbMc2MasterValid() && (m_FbMc2MasterMask != 0))
    {
        ForEachFbpBySite([&] (UINT32 fbp, UINT32 lwrSite, UINT32 siteFbpIdx) {
            regs.Write32(MODS_PPRIV_FBP_FBPx_MC_PRIV_FS_CONFIG,
                         { fbp, 8 },
                         GetFbMc2MasterValue(lwrSite, siteFbpIdx));
        });
    }

    if (m_FbDualRequest != DEFAULT_FB_DUAL_REQUEST)
    {
        UINT32 regValue = regs.Read32(MODS_PFB_FBPA_TIMING21);
        switch (m_FbDualRequest)
        {
            case 0:
                regs.SetField(&regValue, MODS_PFB_FBPA_TIMING21_REFSB_DUAL_REQUEST_DISABLED);
                break;
            case 1:
                regs.SetField(&regValue, MODS_PFB_FBPA_TIMING21_REFSB_DUAL_REQUEST_ENABLED);
                break;
            default:
                Printf(Tee::PriError, "-invalid fb_refsb_dual_request"
                    " argument value %u.\n", m_FbDualRequest);
                MASSERT(!"Generic assertion failure<refer to previous message>.");
                break;
        }
        regs.Write32(MODS_PFB_FBPA_TIMING21, regValue);
    }

    if (m_FbClamshell != DEFAULT_FB_CLAMSHELL)
    {
        UINT32 regValue = regs.Read32(MODS_PFB_FBPA_CFG0);
        switch (m_FbClamshell)
        {
            case 0:
                regs.SetField(&regValue, MODS_PFB_FBPA_CFG0_CLAMSHELL_X32);
                break;
            case 1:
                regs.SetField(&regValue, MODS_PFB_FBPA_CFG0_CLAMSHELL_X16);
                break;
            default:
                Printf(Tee::PriError,
                       "-invalid fb_clamshell argument value %u.\n",
                       m_FbClamshell);
                MASSERT(!"Generic assertion failure<refer to previous message>.");
                break;
        }
        regs.Write32(MODS_PFB_FBPA_CFG0, regValue);
    }

    if (m_FbBanksPerRowBg)
    {
        UINT32 regValue = regs.Read32(MODS_PFB_FBPA_CFG1);
        switch (m_FbBanksPerRowBg)
        {
            case 2:
                regs.SetField(&regValue, MODS_PFB_FBPA_CFG1_NUM_BANKS_PER_ROW_BG_2);
                break;
            case 4:
                regs.SetField(&regValue, MODS_PFB_FBPA_CFG1_NUM_BANKS_PER_ROW_BG_4);
                break;
            case 8:
                regs.SetField(&regValue, MODS_PFB_FBPA_CFG1_NUM_BANKS_PER_ROW_BG_8);
                break;
            default:
                Printf(Tee::PriError,
                       "-invalid fb_banks_per_row_bg argument value %u. \n",
                       m_FbBanksPerRowBg);
                MASSERT(!"Generic assertion failure<refer to previous message>.");
                break;
        }
        regs.Write32(MODS_PFB_FBPA_CFG1, regValue);
    }
}

#ifdef LW_VERIF_FEATURES
/* virtual */
void HopperGpuSubdevice::FilterMemCfgRegOverrides
(
    vector<LW_CFGEX_SET_GRCTX_INIT_OVERRIDES_PARAMS>* pOverrides
)
{
    // For each FB config register we are overriding, also override the matching
    // Multicast priv register, in each range.
    // This keeps the devinit override list safe from vbios that uses multicast.
    // See bug 787526.

    const UINT32 offsetMC0 =
        Regs().LookupAddress(MODS_PFB_FBPA_MC_0_PRI_FBPA_CG) -
        Regs().LookupAddress(MODS_PFB_FBPA_PRI_FBPA_CG);
    const UINT32 offsetMC1 =
        Regs().LookupAddress(MODS_PFB_FBPA_MC_1_PRI_FBPA_CG) -
        Regs().LookupAddress(MODS_PFB_FBPA_PRI_FBPA_CG);
    const UINT32 offsetMC2 =
        Regs().LookupAddress(MODS_PFB_FBPA_MC_2_PRI_FBPA_CG) -
        Regs().LookupAddress(MODS_PFB_FBPA_PRI_FBPA_CG);

    RegHal& regs = TestDevice::Regs();
    LW_CFGEX_SET_GRCTX_INIT_OVERRIDES_PARAMS param = {};

    // Do this before calling GpuSubdevice::FilterMemCfgRegOverrides()
    // since it will then take care of writing to the corresponding _MC_
    // registers as well
    if (m_FbClamshell != DEFAULT_FB_CLAMSHELL)
    {
        UINT32 clamshellMask =
            regs.LookupMask(MODS_PFB_FBPA_CFG0_CLAMSHELL);
        UINT32 clamshellVal = 0;

        switch (m_FbClamshell)
        {
            case 0:
                clamshellVal = regs.SetField(MODS_PFB_FBPA_CFG0_CLAMSHELL_X32);
                break;
            case 1:
                clamshellVal = regs.SetField(MODS_PFB_FBPA_CFG0_CLAMSHELL_X16);
                break;
            default:
                Printf(Tee::PriError,
                       "-invalid fb_clamshell argument value %u.\n",
                       m_FbClamshell);
                MASSERT(!"Generic assertion failure<refer to previous message>.");
                break;
        }
        //Add newer index if required
        param.regAddr = regs.LookupAddress(MODS_PFB_FBPA_CFG0);
        param.orMask  = clamshellVal;
        param.andMask = ~clamshellMask;
        pOverrides->push_back(param);
    }

    if (m_FbBanksPerRowBg)
    {
        UINT32 banksPerRowBgMask =
            regs.LookupMask(MODS_PFB_FBPA_CFG1_NUM_BANKS_PER_ROW_BG);
        UINT32 banksPerRowBgVal = 0;

        switch (m_FbBanksPerRowBg)
        {
            case 2:
                banksPerRowBgVal = regs.SetField(MODS_PFB_FBPA_CFG1_NUM_BANKS_PER_ROW_BG_2);
                break;
            case 4:
                banksPerRowBgVal = regs.SetField(MODS_PFB_FBPA_CFG1_NUM_BANKS_PER_ROW_BG_4);
                break;
            case 8:
                banksPerRowBgVal = regs.SetField(MODS_PFB_FBPA_CFG1_NUM_BANKS_PER_ROW_BG_8);
                break;
            default:
                Printf(Tee::PriError,
                       "-invalid fb_banks_per_row_bg argument value %u.\n",
                       m_FbBanksPerRowBg);
                MASSERT(!"Generic assertion failure<refer to previous message>.");
                break;
        }
        //Add newer index if required
        param.regAddr = regs.LookupAddress(MODS_PFB_FBPA_CFG1);
        param.orMask  = banksPerRowBgVal;
        param.andMask = ~banksPerRowBgMask;
        pOverrides->push_back(param);
    }

    //So far GM20x and GM10x are same. (repeat Broadcast reg settings in
    //corresponding unicast regs)
    GpuSubdevice::FilterMemCfgRegOverrides(offsetMC0, offsetMC1,
                                                      offsetMC2, pOverrides);

    UINT32 fbpIdx = 0, activeFbps = 0;
    const UINT32 stride = Regs().LookupAddress(MODS_FBPA_PRI_STRIDE);
    UINT32 cfg1Reg = Regs().LookupAddress(MODS_PFB_FBPA_0_CFG1);
    const UINT32 cfg1RowBaseVal =
        10 - Regs().LookupValue(MODS_PFB_FBPA_CFG1_ROWA_10);

    activeFbps = Utility::CountBits(GetFsImpl()->FbMask());
    const UINT32 fbps = GetFsImpl()->FbMask();
    MASSERT(fbps);

    for (; (fbpIdx < activeFbps) && (fbpIdx < m_InpPerFbpRowBits.size());
         fbpIdx++)
    {
        //Per Requested FBPA
        if (!(fbps & (1<<fbpIdx)))
        {
            Printf(Tee::PriDebug, "Skip fbpIdx %u\n", fbpIdx);
            continue;
        }
        UINT32 cfg1val = m_InpPerFbpRowBits[fbpIdx];
        if (!cfg1val)
            continue;

        UINT32 pfbCfg1 = 0;
        UINT32 pfbCfg1Mask = 0;
        Regs().SetField(&pfbCfg1, MODS_PFB_FBPA_CFG1_ROWA,
                        cfg1val - cfg1RowBaseVal);
        Regs().SetField(&pfbCfg1, MODS_PFB_FBPA_CFG1_ROWB,
                        cfg1val - cfg1RowBaseVal);
        pfbCfg1Mask |= (Regs().LookupMask(MODS_PFB_FBPA_CFG1_ROWA) |
                        Regs().LookupMask(MODS_PFB_FBPA_CFG1_ROWB));

        bool cfgRegfound = false;
        for (UINT32 i = 0; i < pOverrides->size(); i++)
        {
            //Override the already prepared FBPA_[i] register value if it exists
            param = (*pOverrides)[i];
            if (param.regAddr == cfg1Reg)
            {
                cfgRegfound = true;
                (*pOverrides)[i].orMask = pfbCfg1;
                break;
            }
        }
        if (!cfgRegfound)
        {
            //Add newer index if required
            param.regAddr = cfg1Reg;
            param.orMask = pfbCfg1;
            param.andMask = ~pfbCfg1Mask;
            pOverrides->push_back(param);
        }
        cfg1Reg += stride;
    }

    if (m_FbNumSids != DEFAULT_HBM_NUM_SIDS)
    {
        UINT32 rankMask =
            Regs().LookupMask(MODS_PFB_FBPA_HBM_CFG0_NUM_SIDS);
        UINT32 rankVal = ~0U;
        switch (m_FbNumSids)
        {
            case 1:
                rankVal = Regs().SetField(MODS_PFB_FBPA_HBM_CFG0_NUM_SIDS_1);
                break;
            case 2:
                rankVal = Regs().SetField(MODS_PFB_FBPA_HBM_CFG0_NUM_SIDS_2);
                break;
            case 3:
                rankVal = Regs().SetField(MODS_PFB_FBPA_HBM_CFG0_NUM_SIDS_3);
                break;
            default:
                Printf(Tee::PriError,
                       "invalid -fb_hbm_num_sids argument value %u.\n",
                       m_FbNumSids);
                MASSERT(!"Generic assertion failure<refer to previous message>.");
                break;
        }
        if (rankVal != ~0U)
        {
            //Add newer index if required
            param.regAddr = Regs().LookupAddress(MODS_PFB_FBPA_HBM_CFG0);
            param.orMask  = rankVal;
            param.andMask = ~rankMask;
            pOverrides->push_back(param);
        }
    }
    else if (m_FbDualRanks != DEFAULT_HBM_DUAL_RANKS)
    {
        // TODO: m_FbDualRanks is deprecated in Hopper+.
        // Remove once tests have transitioned to use m_FbNumSids
        UINT32 dualRankMask =
            Regs().LookupMask(MODS_PFB_FBPA_HBM_CFG0_DUAL_RANK);
        UINT32 dualRankVal  =
            Regs().SetField(MODS_PFB_FBPA_HBM_CFG0_DUAL_RANK_ENABLE);

        switch (m_FbDualRanks)
        {
            case 0:
                // Zero means no user argument specified, so keep the default
                // register value.
                break;
            case 8:
                dualRankMask |=
                    Regs().LookupMask(MODS_PFB_FBPA_HBM_CFG0_DUAL_RANK_BANK);
                dualRankVal  |=
                    Regs().SetField(MODS_PFB_FBPA_HBM_CFG0_DUAL_RANK_BANK_8);
                break;
            case 16:
                dualRankMask |=
                    Regs().LookupMask(MODS_PFB_FBPA_HBM_CFG0_DUAL_RANK_BANK);
                dualRankVal  |=
                    Regs().SetField(MODS_PFB_FBPA_HBM_CFG0_DUAL_RANK_BANK_16);
                break;
            default:
                Printf(Tee::PriError,
                       "Invalid -fb_hbm_ranks_per_bank argument value %u.\n",
                       m_FbDualRanks);
                MASSERT(!"Generic assertion failure<refer to previous message>.");
                break;
        }

        //Add newer index if required
        param.regAddr = Regs().LookupAddress(MODS_PFB_FBPA_HBM_CFG0);
        param.orMask  = dualRankVal;
        param.andMask = ~dualRankMask;
        pOverrides->push_back(param);
    }

    if (IsFbMc2MasterValid() && (m_FbMc2MasterMask != 0))
    {
        ForEachFbpBySite([&] (UINT32 fbp, UINT32 lwrSite, UINT32 siteFbpIdx) {
            param.regAddr = Regs().LookupAddress(MODS_PPRIV_FBP_FBPx_MC_PRIV_FS_CONFIG,
                                                 { fbp, 8 });
            param.orMask  = GetFbMc2MasterValue(lwrSite, siteFbpIdx);
            param.andMask = 0;
            pOverrides->push_back(param);
        });
    }

    if (m_FbDualRequest != DEFAULT_FB_DUAL_REQUEST)
    {
        UINT32 dualRankMask =
            regs.LookupMask(MODS_PFB_FBPA_TIMING21_REFSB_DUAL_REQUEST);
        UINT32 dualRankVal = 0;

        switch (m_FbDualRequest)
        {
            case 0:
                dualRankVal = regs.SetField(MODS_PFB_FBPA_TIMING21_REFSB_DUAL_REQUEST_DISABLED);
                break;
            case 1:
                dualRankVal = regs.SetField(MODS_PFB_FBPA_TIMING21_REFSB_DUAL_REQUEST_ENABLED);
                break;
            default:
                Printf(Tee::PriError,
                       "-invalid fb_refsb_dual_request argument value %u.\n",
                       m_FbDualRequest);
                MASSERT(!"Generic assertion failure<refer to previous message>.");
                break;
        }
        //Add newer index if required
        param.regAddr = regs.LookupAddress(MODS_PFB_FBPA_TIMING21);
        param.orMask  = dualRankVal;
        param.andMask = ~dualRankMask;
        pOverrides->push_back(param);
    }
}
#endif

//------------------------------------------------------------------------------
void HopperGpuSubdevice::SetupFeatures()
{
    //TODO: Verify these features apply to Ampere GPUs
    GpuSubdevice::SetupFeatures();

    if (IsInitialized())
    {
        // Setup features that require the system to be properly initialized
        // (i.e. LwRm can be accessed, all GpuSubdevice data setup, registers
        // can be accessed, etc.)

        if (!HasSORXBar())
            ClearHasFeature(GPUSUB_SUPPORTS_SOR_XBAR);

        //For volta+ devices, supports LWLINK connections from GPU to system memory
        if (SupportsInterface<LwLink>() && GetInterface<LwLink>()->IsPostDiscovery())
        {
            vector<LwLink::EndpointInfo> endPointInfo;
            if (GetInterface<LwLink>()->GetDetectedEndpointInfo(&endPointInfo) == OK)
            {
                for (UINT32 i=0; i < endPointInfo.size(); i++)
                    if (endPointInfo[i].devType ==  TestDevice::TYPE_IBM_NPU)
                    {
                        SetHasFeature(GPUSUB_SUPPORTS_CPU_FB_MAPPINGS_THROUGH_LWLINK);
                        break;
                    }
            }
        }
    }
    else
    {
        // Setup features that are independant of system state
        SetHasFeature(GPUSUB_HAS_SEC_DEBUG);
        SetHasFeature(GPUSUB_SUPPORTS_GEN5_PCIE);
        SetHasFeature(GPUSUB_HAS_PRIV_SELWRITY);
        SetHasFeature(GPUSUB_SUPPORTS_ROW_REMAPPING);

        // All display-enabled Maxwell GPUs support Dual Head midframe mclkswitch
        SetHasFeature(GPUSUB_SUPPORTS_DUAL_HEAD_MIDFRAME_SWITCH);

        SetHasFeature(GPUSUB_HAS_PEX_ASPM_L1SS);

        SetHasFeature(GPUSUB_STRAPS_FAN_DEBUG_PWM_66);
        SetHasFeature(GPUSUB_STRAPS_FAN_DEBUG_PWM_33);

        //Setup LW402C_CTRL_CMD_I2C_TABLE_GET_DEV_INFO Call support
        SetHasFeature(GPUSUB_HAS_RMCTRL_I2C_DEVINFO);

        //All Maxwell GPUs support LWPATH rendering extension
        SetHasFeature(GPUSUB_SUPPORTS_LWPR);

        SetHasFeature(GPUSUB_SUPPORTS_MULTIPLE_LAYERS_AND_VPORT);

        // Setup features that are independant of system state
        SetHasFeature(GPUSUB_SUPPORTS_UNCOMPRESSED_RAWIMAGE);

        //Display XBAR is a new feature added from gm20x+ chips.
        SetHasFeature(GPUSUB_SUPPORTS_SOR_XBAR);
        SetHasFeature(GPUSUB_SUPPORTS_SINGLE_HEAD_DUAL_SST);
        SetHasFeature(GPUSUB_SUPPORTS_SINGLE_HEAD_DUAL_MST);

        //ACR is supported on GM20X+ chips but not on GM20b.
        SetHasFeature(GPUSUB_SUPPORTS_ACR_REGION);

        SetHasFeature(GPUSUB_SUPPORTS_HDMI2);

        SetHasFeature(GPUSUB_SUPPORTS_16xMsAA);
        SetHasFeature(GPUSUB_SUPPORTS_TIR);
        SetHasFeature(GPUSUB_SUPPORTS_FILLRECT);
        SetHasFeature(GPUSUB_SUPPORTS_S8_FORMAT);

        // VASpace specific big page size is supported on gm20x+ chips
        SetHasFeature(GPUSUB_SUPPORTS_PER_VASPACE_BIG_PAGE);

        // Secure HW scrubber is present on GP100+ chips (except gp10b)
        SetHasFeature(GPUSUB_HAS_SELWRE_FB_SCRUBBER);

        // GPU has boards.db version 2 entries
        SetHasFeature(GPUSUB_SUPPORTS_BOARDS_DB_V2);

        // FS Check support available Volta+
        SetHasFeature(GPUSUB_SUPPORTS_FS_CHECK);

        // POR Bin data is available Volta+
        SetHasFeature(GPUSUB_SUPPORTS_POR_BIN_CHECK);

        // Volta+ have channel subcontexts
        SetHasFeature(GPUSUB_HAS_SUBCONTEXTS);

        // RM supports retrieving VBIOS GUID
        SetHasFeature(GPUSUB_SUPPORTS_VBIOS_GUID);

        // ECC errors can be priv-injected
        SetHasFeature(GPUSUB_SUPPORTS_ECC_PRIV_INJECTION);

        // Power Regulators can be checked
        SetHasFeature(GPUSUB_SUPPORTS_PWR_REG_CHECK);
        SetHasFeature(GPUSUB_SUPPORTS_POST_L2_COMPRESSION);
        SetHasFeature(GPUSUB_SUPPORTS_GENERIC_MEMORY);

        SetHasFeature(GPUSUB_SUPPORTS_RM_GENERIC_ECC);

        SetHasFeature(GPUSUB_SUPPORTS_HECC_FUSING);

        // Ampere requires providing an engine ID when allocating channels or TSGs
        SetHasFeature(GPUSUB_HOST_REQUIRES_ENGINE_ID);

        // Ampere fusing features
        SetHasFeature(GPUSUB_SUPPORTS_FUSING_WO_DAUGHTER_CARD);
        SetHasFeature(GPUSUB_SUPPORTS_IFF_SW_FUSING);
        SetHasFeature(GPUSUB_HAS_MERGED_FUSE_MACROS);

        // L1 mode is POR on Hopper
        SetHasFeature(GPUSUB_LWLINK_L1_MODE_POR);

        // Has HW for In-Silicon-Testing
        SetHasFeature(GPUSUB_HAS_IST);

        // Logical GPCs are ordered from least to greatest number of TPCs
        SetHasFeature(GPUSUB_GPC_REENUMERATION);

        // Ampere has LW_XVE_RESET_CTRL_GPU_RESET defined, but it is non-functional
        ClearHasFeature(GPUSUB_HAS_GPU_RESET);

        // LcePce remapping is supported
        SetHasFeature(GPUSUB_SUPPORTS_LCE_PCE_REMAPPING);

        SetHasFeature(GPUSUB_SUPPORTS_PSTATES_IN_PMU);
        SetHasFeature(GPUSUB_SUPPORTS_LWLINK_OFF_TRANSITION);
        SetHasFeature(GPUSUB_SUPPORTS_VULKAN);
        SetHasFeature(GPUSUB_SUPPORTS_SCPM_AND_POD);

        // Hopper FBHUB deprecates the block linear swizzle logic Bug 2838538
        SetHasFeature(GPUSUB_DEPRECATED_RAW_SWIZZLE);

        // Ampere+ does not support HS->SAFE->HS transitions, it is required to
        // go through OFF after transitioning from HS->SAFE
        ClearHasFeature(GPUSUB_SUPPORTS_LWLINK_HS_SAFE_CYCLING);

        // Ampere+ TSEC engine does not support the GFE application
        ClearHasFeature(GPUSUB_TSEC_SUPPORTS_GFE_APPLICATION);

        ClearHasFeature(GPUSUB_SUPPORTS_LEGACY_GC6);

        SetHasFeature(GPUSUB_SUPPORTS_FSP);
        SetHasFeature(GPUSUB_SUPPORTS_COMPUTE_ONLY_GPC);
        SetHasFeature(GPUSUB_SUPPORTS_KAPPA_BINNING);
        SetHasFeature(GPUSUB_SUPPORTS_BAR1_P2P);
        SetHasFeature(GPUSUB_SUPPORTS_256B_PCIE_PACKETS);

        // This shouldnt be necessary because support is setup in GpuSubdevice
        // via a RegHal::IsSupported check, but since the old Gen4 manuals have
        // not been deleted in the SW the register is still flagged incorrectly
        // as supported
        ClearHasFeature(GPUSUB_SUPPORTS_ASPM_L1P);
    }
}

//------------------------------------------------------------------------------
bool HopperGpuSubdevice::IsFeatureEnabled(Feature feature)
{
    RegHal& regs = Regs();

    MASSERT(HasFeature(feature));
    switch (feature)
    {
        case GPUSUB_HAS_SEC_DEBUG:
        {
            return regs.Test32(MODS_FUSE_OPT_SELWRE_SECENGINE_DEBUG_DIS_DATA_NO);
        }

        // Ampere is displayless
        case GPUSUB_HAS_KFUSES:
        case GPUSUB_HAS_AZALIA_CONTROLLER:
            return false;

        // -----  GF100GpuSubdevice::IsFeatureEnabled  -----
        // This register doesn't exist on Hopper
        case GPUSUB_STRAPS_PEX_PLL_EN_TERM100:
            return false;

        // Hardware always assumes there is a ROM and exits gracefully if no ROM
        // is found. IFR (Init from ROM) will no longer check for the ROM's
        // presence so no need to have the NOROM error field. Therefore
        // LW_PEXTDEV_BOOT_0_STRAP_SUB_VENDOR has been removed(see B1040649)
        case GPUSUB_STRAPS_SUB_VENDOR:
            return false;

        // Register no longer exists on Maxwell.
        case GPUSUB_STRAPS_SLOT_CLK_CFG:
            return false;

        case GPUSUB_STRAPS_FAN_DEBUG_PWM_66:
            return Regs().Test32(MODS_PGC6_SCI_FAN_DEBUG_PWM_STRAP_66);

        case GPUSUB_STRAPS_FAN_DEBUG_PWM_33:
            return Regs().Test32(MODS_PGC6_SCI_FAN_DEBUG_PWM_STRAP_33);

        default:
            return GpuSubdevice::IsFeatureEnabled(feature);
    }
}

//------------------------------------------------------------------------------
void HopperGpuSubdevice::SetupBugs()
{
    if (IsInitialized())
    {
        // Setup bugs that require the system to be properly initialized
        // (i.e. LwRm can be accessed, all GpuSubdevice data setup, registers
        // can be accessed, etc.)

        // This bug is for the vttgen misc1_pad which has hardcoded values for
        // all but chip gf106.
        SetHasBug(548966);
        // for gf106, the vttgen i2c_pad has the hardcoded values-bug 569987
        SetHasBug(569987);

        // Maxwell gpu need iddq value to be multiplied by 50
        // Speedo values are correct.
        SetHasBug(598305);

        // Setup bugs that require the system to be properly initialized
        // (i.e. LwRm can be accessed, all GpuSubdevice data setup, registers
        // can be accessed, etc.)

        SetHasBug(1804258); //Compiling a compute shader causes compute init in GL which is broken
        SetHasBug(1842362); //Injecting HDRPARITYERR can cause hw hangs
        if (IsDFPGA())
        {
            SetHasBug(1826061); //Input LUT can't be enabled with or just after FP16. fixed by ECO.
        }
        // New PMA control register for feeding the EMULATION CAPTURE signal to stop VPM dumps
        // is not connected
        SetHasBug(200497337);
    }
    else
    {
        // Setup bugs that do not require the system to be properly initialized

        // G80 and above should not touch VGA registers on simulation
        // (TODO : This was fixed...where?)
        SetHasBug(143732);

        // Bug 321377 : G82+ cannot set PCLK directly
        SetHasBug(321377);

        //Pri ring gets broken if device in daisy chain gets floorswept.
        //Speedos can't be read in all partitions because of this.
        SetHasBug(598576);

        // Fuse reads are inconsistent. Sometimes get back 0xFFFFFFFF. Need fuse
        // related code to make sure we have consistent reads first
        SetHasBug(622250);

        // In 8+24 VCAA mode one fine-raster transaction carries 1x2 pixels
        // worth of data. So to generate one quad (2X2 pixels) for SM, PROP
        // needs to stitch 2 transaction together.
        // TC might run out of it's resources when it sees the first
        // transaction of the quad, and it may force tile_end and flush the
        // tile with only first half of the quad.
        // That would cause PROP to break quad into 2 transactions with 1x2
        // pixels covered in each and they both will end-up in different
        // subtiles.
        SetHasBug(618730);

        // TXD instructions need inputs for partial derivitives sanitized on
        // non-pixel shaders
        SetHasBug(630560);

        // PCIE correctable errors during link speed toggle
        SetHasBug(361633);

        // Floorsweeping not compatible with GC6 cycles (all GMxxx)
        SetHasBug(1173195);

        // If DVFS is enabled, one cannot rely on PLL_LOCK bit for GPC PLL
        SetHasBug(1371047);

        // Lwca layer wont place USERD in sysmem unless all allocations are in sysmem
        SetHasBug(1862474);

        // Volta LwDisplay tests are not compatible with parallel test groups
        SetHasBug(1888064);

        // Volta (like GP10x) lwrrently requires debug mode for SEC 2 test to run
        SetHasBug(1746899);

        SetHasBug(200296583); //Possible bad register read: addr: 0x61d98c

        // Turing POR dropped FLR on azalia/xusb/ppc due to h/w bugs. check bug 2110231 for ampere
        SetHasBug(2108973);

        if (Platform::GetSimulationMode() == Platform::RTL)
        {
            // Many FBPA PRIV_LEVEL_MASK registers are unsupported in RTL
            SetHasBug(2355623);
        }
        // LwdaRandom has issues with the work creation kernel
        SetHasBug(2964522);
        // Bringup Issues with FLReset and floorsweeping
        SetHasBug(3471757);
        SetHasBug(3472315);

        // RM returning fixed value for C2C bandwidth
        SetHasBug(3479179);

        // CASK issue with conselwtive kernel launches
        SetHasBug(3522446);
    }
}

/*************************************************************************************************/
// This version of  ReadHost2Jtag() allows the caller to target multiple jtag clusters
// (aka chiplets) on the same Host2Jtag read operation which is required for some HBM memory tests.
// The hw/lw/doc/dft/SOCF/Distributed_DFT_Test_Control.docx describes all the supported types of
// Jtag access.
/* virtual */
RC HopperGpuSubdevice::ReadHost2Jtag
(
     vector<JtagCluster> &jtagClusters  // vector of { chipletId, instrId } tuples
    ,UINT32 chainLen                    // number of bits in the chain or cluster
    ,vector<UINT32> *pResult            // location to store the data
    ,UINT32 capDis                      // sets the LW_PJTAG_ACCESS_CONFIG_CAPT_DIS bit, default=0
    ,UINT32 updtDis                     // set the LW_PJTAG_ACCESS_CONFIG_UPDT_DIS bit, default=0
)
{
    RC rc;
    UINT32 dwordEn = 0;
    UINT32 dwordEnMax = ((chainLen+31)/32);
    UINT32 data = 0;
    pResult->clear();
    Tee::Priority pri = (m_VerboseJtag) ? Tee::PriNormal : Tee::PriLow;
    RegHal& regs = GpuSubdevice::Regs();
    UINT32 clkEn = 0;
    CHECK_RC(regs.Read32Priv(MODS_PTRIM_SYS_JTAGINTFC_JTAGTM_INTFC_CLK_EN, &clkEn));
    if (!clkEn)
    {
        CHECK_RC(regs.Write32Priv(MODS_PTRIM_SYS_JTAGINTFC_JTAGTM_INTFC_CLK_EN_ON));
    }
    DEFER
    {
        if (!clkEn)
        {
            regs.Write32(MODS_PTRIM_SYS_JTAGINTFC_JTAGTM_INTFC_CLK_EN_OFF);
        }
    };

    CHECK_RC(regs.Write32Priv(MODS_PJTAG_ACCESS_CTRL, 0));
    while (dwordEn < dwordEnMax)
    {
        for (UINT32 ii = 0; ii < jtagClusters.size(); ii++)
        {
            if (dwordEn == 0)
            {
                // Write DWORD_EN, REG_LENGTH, BURST, for each chain.
                data = 0;
                if ((jtagClusters.size() > 1) && (ii != jtagClusters.size()-1))
                {
                    regs.SetField(&data, MODS_PJTAG_ACCESS_CONFIG_LAST_CLUSTER_ILW, 1);
                }
                regs.SetField(&data, MODS_PJTAG_ACCESS_CONFIG_REG_LENGTH, chainLen >> 11);
                regs.SetField(&data, MODS_PJTAG_ACCESS_CONFIG_DWORD_EN,   dwordEn >> 6);
                regs.SetField(&data, MODS_PJTAG_ACCESS_CONFIG_UPDT_DIS,   updtDis);
                regs.SetField(&data, MODS_PJTAG_ACCESS_CONFIG_CAPT_DIS,   capDis);
                regs.SetField(&data, MODS_PJTAG_ACCESS_CONFIG_CLUSTER_SEL,
                          ((jtagClusters[ii].chipletId >>
                          regs.LookupMaskSize(MODS_PJTAG_ACCESS_CTRL_CLUSTER_SEL)) &
                          regs.LookupFieldValueMask(MODS_PJTAG_ACCESS_CONFIG_CLUSTER_SEL))); // 3 MSBs
                regs.SetField(&data, MODS_PJTAG_ACCESS_CONFIG_AUTO_DWORD_INCR, 1);
                regs.Write32Priv(MODS_PJTAG_ACCESS_CONFIG, data);

                data = 0;
                regs.SetField(&data, MODS_PJTAG_ACCESS_CTRL_REG_LENGTH,  chainLen);
                regs.SetField(&data, MODS_PJTAG_ACCESS_CTRL_INSTR_ID,    jtagClusters[ii].instrId);
                regs.SetField(&data, MODS_PJTAG_ACCESS_CTRL_CLUSTER_SEL,
                              (jtagClusters[ii].chipletId &
                               regs.LookupFieldValueMask(MODS_PJTAG_ACCESS_CTRL_CLUSTER_SEL)));//5 LSBs
                regs.SetField(&data, MODS_PJTAG_ACCESS_CTRL_DWORD_EN,    dwordEn);
                regs.SetField(&data, MODS_PJTAG_ACCESS_CTRL_REQ_CTRL,    1);
                regs.Write32(MODS_PJTAG_ACCESS_CTRL, data);
            }

            // if it's the first jtagCtrl write, poll for status==1
            if (ii == 0)
            {
                // We expect CTRL_STATUS to become non-zero within 150us in the
                // worst case
                RC rc = POLLWRAP_HW(PollJtagForNonzero, static_cast<void*>(this), 1);
                if (OK != rc)
                {
                    Printf(pri, "Failed to read good Jtag Ctrl Status\n");
                    return RC::HW_STATUS_ERROR;
                }
            }
        }
        // now read the data
        data = regs.Read32(MODS_PJTAG_ACCESS_DATA);
        if ((dwordEn == dwordEnMax-1) && ((chainLen % 32) != 0))
        {
            data = data >> (32-(chainLen%32));
        }
        pResult->push_back(data);
        dwordEn++;
    }
    regs.Write32(MODS_PJTAG_ACCESS_CONFIG, 0);
    regs.Write32(MODS_PJTAG_ACCESS_CTRL, 0);

    Printf(pri, "ReadHost2Jtag: Read data size:%u [", (UINT32)pResult->size());
    for (UINT32 i = static_cast<UINT32>(pResult->size()); i > 0; i--)
    {
        Printf(pri, " 0x%x ", (*pResult)[i-1]);
    }
    Printf(pri, " ] from cluster(s) [");
    for (UINT32 i = 0; i < jtagClusters.size(); i++)
    {
        Printf(pri, " [ \"chipletId\":%u \"instrId\":%u ]", jtagClusters[i].chipletId,
               jtagClusters[i].instrId);
    }
    Printf(pri, " ] with chain length %u\n", chainLen);
    return rc;
}

/*************************************************************************************************/
// This version of WriteHost2Jtag() allows the caller to target multiple jtag clusters
// (aka chiplets) on the same Host2Jtag write operation, which is required for some HBM memory
// tests. The Distributed_DFT_Test_Control.docx describes all the supported types of Jtag access.
/*virtual*/
RC HopperGpuSubdevice::WriteHost2Jtag
(
     vector<JtagCluster> &jtagClusters // vector of { chipletId, instrId } tuples
    ,UINT32 chainLen                   // number of bits in the chain
    ,const vector<UINT32> &inputData   // location to store the data
    ,UINT32 capDis                     // sets the LW_PJTAG_ACCESS_CONFIG_CAPT_DIS bit, default=0
    ,UINT32 updtDis                    // sets the LW_PJTAG_ACCESS_CONFIG_UPDT_DIS bit, default=0
)
{
    RC rc;
    UINT32 dwordEn = 0;
    UINT32 data = 0;
    Tee::Priority pri = (m_VerboseJtag) ? Tee::PriNormal : Tee::PriLow;
    RegHal& regs = GpuSubdevice::Regs();
    UINT32 clkEn = 0;
    CHECK_RC(regs.Read32Priv(MODS_PTRIM_SYS_JTAGINTFC_JTAGTM_INTFC_CLK_EN, &clkEn));

    if (inputData.size() != ((chainLen+31)/32))
    {
        Printf(pri, "Chainlength is incorrect for sizeof InputData[]\n");
        return RC::BAD_PARAMETER;
    }
    if (!clkEn)
    {
        CHECK_RC(regs.Write32Priv(MODS_PTRIM_SYS_JTAGINTFC_JTAGTM_INTFC_CLK_EN_ON));
    }
    DEFER
    {
        if (!clkEn)
        {
            regs.Write32(MODS_PTRIM_SYS_JTAGINTFC_JTAGTM_INTFC_CLK_EN_OFF);
        }
    };

    Printf(pri, "WriteHost2Jtag: Writing data of size %u [", (UINT32)inputData.size());
    for (UINT32 i = static_cast<UINT32>(inputData.size()); i > 0;  i--)
    {
        Printf(pri, " 0x%x ", inputData[i-1]);
    }
    Printf(pri, " ] to cluster [");
    for (UINT32 i = 0; i < jtagClusters.size(); i++)
    {
        Printf(pri, " [ \"chipletId\":%u \"instrId\":%u ]", jtagClusters[i].chipletId,
               jtagClusters[i].instrId);
    }
    Printf(pri, " ] with chain length %u\n", chainLen);


    CHECK_RC(regs.Write32Priv(MODS_PJTAG_ACCESS_CTRL, 0));
    for (UINT32 ii = 0; ii < jtagClusters.size(); ii++)
    {
        data = 0x00;
        // if multiple clusters and not the last cluster
        if ((jtagClusters.size() > 1) && (ii < jtagClusters.size()-1))
        {
            //Note the PLAIN_MODE is really the LAST_CLUSTER_ILW bit. The refmanuals are wrong
            regs.SetField(&data, MODS_PJTAG_ACCESS_CONFIG_LAST_CLUSTER_ILW, 1);
        }
        regs.SetField(&data, MODS_PJTAG_ACCESS_CONFIG_REG_LENGTH, chainLen >> 11);
        regs.SetField(&data, MODS_PJTAG_ACCESS_CONFIG_DWORD_EN,   dwordEn >> 6);
        regs.SetField(&data, MODS_PJTAG_ACCESS_CONFIG_UPDT_DIS,   updtDis);
        regs.SetField(&data, MODS_PJTAG_ACCESS_CONFIG_CAPT_DIS,   capDis);
        regs.SetField(&data, MODS_PJTAG_ACCESS_CONFIG_CLUSTER_SEL,
                      ((jtagClusters[ii].chipletId >>
                       regs.LookupMaskSize(MODS_PJTAG_ACCESS_CTRL_CLUSTER_SEL)) &
                       regs.LookupFieldValueMask(MODS_PJTAG_ACCESS_CONFIG_CLUSTER_SEL))); //3 MSBs
        regs.Write32Priv(MODS_PJTAG_ACCESS_CONFIG, data);

        data = 0;

        regs.SetField(&data, MODS_PJTAG_ACCESS_CTRL_REG_LENGTH,  chainLen);
        regs.SetField(&data, MODS_PJTAG_ACCESS_CTRL_INSTR_ID,    jtagClusters[ii].instrId);
        regs.SetField(&data, MODS_PJTAG_ACCESS_CTRL_CLUSTER_SEL,
                      (jtagClusters[ii].chipletId &
                       regs.LookupFieldValueMask(MODS_PJTAG_ACCESS_CTRL_CLUSTER_SEL))); //5 LSBs
        regs.SetField(&data, MODS_PJTAG_ACCESS_CTRL_REQ_CTRL,    1);
        regs.Write32(MODS_PJTAG_ACCESS_CTRL, data);

        if (ii == 0)
        {
            // We expect CTRL_STATUS to become non-zero within 150us in the
            // worst case
            RC rc = POLLWRAP_HW(PollJtagForNonzero, static_cast<void*>(this), 1);
            if (OK != rc)
            {
                Printf(pri, "Failed to read good Jtag Ctrl Status\n");
                return RC::HW_STATUS_ERROR;
            }
        }
    }
    // write the data
    while (dwordEn < inputData.size())
    {
        regs.Write32(MODS_PJTAG_ACCESS_DATA, inputData[dwordEn]);
        dwordEn++;
    }
    regs.Write32(MODS_PJTAG_ACCESS_CTRL, 0);
    return rc;
}

//------------------------------------------------------------------------------
// Return the size (in bits) of the selwre_id field.
RC HopperGpuSubdevice::GetSelwreKeySize(UINT32 *pChkSize, UINT32 *pCfgSize)
{
    RegHal& regs = TestDevice::Regs();

    *pChkSize = regs.LookupMaskSize(MODS_JTAG_SEC_CHK_testmaster_selwreid);
    *pCfgSize = regs.LookupAddress(MODS_JTAG_SEC_CFG_CHAINLENGTH);

    return OK;
}

//------------------------------------------------------------------------------
// Provide the caller with the required cluster information needed to unlock the
// jtag chains.
// Inputs: pInfo->selwreKeys: A vector containing the secure keys, must not be empty.
//
// Outputs: The requirement is to provide info on 3 chains.
// jtagClusters[0] = Command chain
// jtagClusters[1] = Config chain
// jtagClusters[2] = Check chain
RC HopperGpuSubdevice::GetH2jUnlockInfo(h2jUnlockInfo *pInfo)
{
    RC rc;
    UINT32 hiBit, loBit;

    RegHal& regs = TestDevice::Regs();

    pInfo->numChiplets = regs.LookupAddress(MODS_JTAG_NUM_CLUSTERS);
    regs.LookupField(MODS_JTAG_SEC_CFG_features, &hiBit, &pInfo->cfgFeatureStartBit);
    regs.LookupField(MODS_JTAG_SEC_UNLOCK_STATUS_features, &hiBit, &pInfo->unlockFeatureStartBit);
    pInfo->numFeatureBits = regs.LookupMaskSize(MODS_JTAG_SEC_UNLOCK_STATUS_features);

    // Move the secure keys into the proper bit positions for the SEC_CHK chain.
    pInfo->selwreData.assign((regs.LookupAddress(MODS_JTAG_SEC_CHK_CHAINLENGTH)+31)/32, 0x00);
    regs.LookupField(MODS_JTAG_SEC_CHK_testmaster_selwreid, &hiBit, &loBit);
    CHECK_RC(Utility::CopyBits(pInfo->selwreKeys, // source
                               pInfo->selwreData, // dest
                               0,                 // src offset
                               loBit,            // dest offset
                               regs.LookupMaskSize(MODS_JTAG_SEC_CHK_testmaster_selwreid))); // numbits

    // Setup the bitstream for the SEC_CFG chain.
    pInfo->cfgData.assign((regs.LookupAddress(MODS_JTAG_SEC_CFG_CHAINLENGTH)+31)/32, 0x00);
    regs.LookupField(MODS_JTAG_SEC_CFG_features, &hiBit, &loBit);
    CHECK_RC(Utility::CopyBits(pInfo->cfgMask,
                               pInfo->cfgData,
                               0,
                               loBit,
                               regs.LookupMaskSize(MODS_JTAG_SEC_CFG_features)));

    // Setup the bitstream for the SEC_CHK chain after the secure data has been written
    pInfo->chkData.clear();
    pInfo->chkData.assign((regs.LookupAddress(MODS_JTAG_SEC_CHK_CHAINLENGTH)+31)/32, 0x00);

    // Setup the bitstream for the SEC_CMD chain
    pInfo->cmdData.assign((regs.LookupAddress(MODS_JTAG_SEC_CMD_CHAINLENGTH)+31)/32, 0x00);
    vector<UINT32> temp(1, 1);
    regs.LookupField(MODS_JTAG_SEC_CMD_testmaster_sha_cal_start, &hiBit, &loBit);
    CHECK_RC(Utility::CopyBits(temp,
                               pInfo->cmdData,
                               0,
                               loBit,
                               regs.LookupMaskSize(MODS_JTAG_SEC_CMD_testmaster_sha_cal_start)));

    // Setup the bitstream for the SEC_UNLOCK_STATUS chain.
    pInfo->unlockData.clear();
    pInfo->unlockData.assign((regs.LookupAddress(MODS_JTAG_SEC_UNLOCK_STATUS_CHAINLENGTH)+31)/32,
                             0x00);

    pInfo->clusters.clear();
    pInfo->clusters.push_back(GpuSubdevice::JtagCluster(regs.LookupAddress(MODS_JTAG_SEC_CMD_CLUSTER_ID),
                                                        regs.LookupAddress(MODS_JTAG_SEC_CMD_ID),
                                                        regs.LookupAddress(MODS_JTAG_SEC_CMD_CHAINLENGTH)));
    pInfo->clusters.push_back(GpuSubdevice::JtagCluster(regs.LookupAddress(MODS_JTAG_SEC_CFG_CLUSTER_ID),
                                                        regs.LookupAddress(MODS_JTAG_SEC_CFG_ID),
                                                        regs.LookupAddress(MODS_JTAG_SEC_CFG_CHAINLENGTH)));
    pInfo->clusters.push_back(GpuSubdevice::JtagCluster(regs.LookupAddress(MODS_JTAG_SEC_CHK_CLUSTER_ID),
                                                        regs.LookupAddress(MODS_JTAG_SEC_CHK_ID),
                                                        regs.LookupAddress(MODS_JTAG_SEC_CHK_CHAINLENGTH)));
    // There are LW_JTAG_NUM_CLUSTERS_tuxxx clusters for the LW_JTAG_SEC_UNLOCK_STATUS_ID_tuxxx
    // chains. This entry gives the 1st cluster ID and total length for all the clusters.
    // access via pInfo->clusters[selwnlock]
    pInfo->clusters.push_back(GpuSubdevice::JtagCluster(0,
                                                        regs.LookupAddress(MODS_JTAG_SEC_UNLOCK_STATUS_ID),
                                                        regs.LookupAddress(MODS_JTAG_SEC_UNLOCK_STATUS_CHAINLENGTH)));
    return rc;
}

//------------------------------------------------------------------------------
/* virtual */
// Unlock the Host2Jtag access.
// For unfused parts the secCfg can be 0xFFFFFFFF, and the contents of selwreKeys
// can be all zeros.
// For fused parts secCfg must be appropriate for the selwreKeys and are provided
// by the Green Hill Server.
RC HopperGpuSubdevice::UnlockHost2Jtag
(
    const vector<UINT32> &secCfgMask,   // bitmask of features to unlock (see defines above)
    const vector<UINT32> &selwreKeys    // vector of secure keys for the provided bitmask
)
{
    RC rc;
    h2jUnlockInfo unlockInfo;
    unlockInfo.selwreKeys = selwreKeys;
    unlockInfo.cfgMask = secCfgMask;

    // Get the detailed data from the GPU class on how to unlock jtag.
    CHECK_RC(GetH2jUnlockInfo(&unlockInfo));
    CHECK_RC(UnlockHost2Jtag(&unlockInfo));
    return OK;
}

//------------------------------------------------------------------------------
// Verify the feature bits in the unlock status chain match the feature bits in
// the config chain. Note bit fields vary for different types of  GPU so the
// data must be overloaded.
RC HopperGpuSubdevice::VerifyHost2JtagUnlockStatus(h2jUnlockInfo *pUnlockInfo)
{
    RC rc;
    vector<UINT32> cfgFeatures((pUnlockInfo->numFeatureBits+31)/32, 0x00);
    vector<UINT32> unlockFeatures((pUnlockInfo->numFeatureBits+31)/32, 0x00);
    CHECK_RC(Utility::CopyBits(pUnlockInfo->cfgData,    //src
                      cfgFeatures,                      //dst
                      pUnlockInfo->cfgFeatureStartBit,  //srcBitOffset
                      0,                                //dstBitOffset
                      pUnlockInfo->numFeatureBits));    //numBits

    CHECK_RC(Utility::CopyBits(pUnlockInfo->unlockData,     //src
                      unlockFeatures,                       //dst
                      pUnlockInfo->unlockFeatureStartBit,   //srcBitOffset
                      0,                                    //dstBitOffset
                      pUnlockInfo->numFeatureBits));        //numBits
    if (unlockFeatures != cfgFeatures)
    {
        Tee::Priority pri = (m_VerboseJtag) ? Tee::PriNormal : Tee::PriLow;
        Printf(pri,
               "Failed to verify h2j unlock status. One or more features were not unlocked!\n");
        return RC::UNEXPECTED_RESULT;
    }
    return rc;
}

//------------------------------------------------------------------------------
RC HopperGpuSubdevice::UnlockHost2Jtag(h2jUnlockInfo *pUnlockInfo)
{
    RC rc;
    Tee::Priority pri = (m_VerboseJtag) ? Tee::PriNormal : Tee::PriLow;
    RegHal& regs = GpuSubdevice::Regs();
    UINT32 clkEn = 0;
    CHECK_RC(regs.Read32Priv(MODS_PTRIM_SYS_JTAGINTFC_JTAGTM_INTFC_CLK_EN, &clkEn));
    if (!clkEn)
    {
        CHECK_RC(regs.Write32Priv(MODS_PTRIM_SYS_JTAGINTFC_JTAGTM_INTFC_CLK_EN_ON));
    }
    DEFER
    {
        if (!clkEn)
        {
            regs.Write32Priv(MODS_PTRIM_SYS_JTAGINTFC_JTAGTM_INTFC_CLK_EN_OFF);
        }
    };

    // If possible set the PJTAG_ACCESS_SEC to STANDARD mode to allow jtag debugging.
    // By default this register is PLM write protected at level 3. So the user must pass a proper
    // hulk license on the mods command line using -hulk_cert <filename>
    if (regs.Test32(MODS_PJTAG_ACCESS_SEC_MODE_ISM_ONLY) &&
        regs.HasRWAccess(MODS_PJTAG_ACCESS_SEC))
    {
        regs.Write32(MODS_PJTAG_ACCESS_SEC_SWITCH_TO_STANDARD_REQ);
    }

    vector<GpuSubdevice::JtagCluster> jtagClusters;
    Printf(pri, "UnlockHost2Jtag:Start\n");
    // 1.Program JTAG_SEC_CFG chain with the features that need to be unlocked.
    jtagClusters.push_back(pUnlockInfo->clusters[secCfg]);
    CHECK_RC(WriteHost2Jtag(jtagClusters
                   ,pUnlockInfo->clusters[secCfg].chainLen //chainLen
                   ,pUnlockInfo->cfgData)); //InputData

    // 2.Program JTAG_SEC_CHK chain with the secure keys
    jtagClusters[0] = pUnlockInfo->clusters[secChk];
    CHECK_RC(WriteHost2Jtag(jtagClusters,
                            pUnlockInfo->clusters[secChk].chainLen,
                            pUnlockInfo->selwreData));

    //3.Program JTAG_SEC_CMD chain with value 0X1 to start SHA callwlation
    jtagClusters[0] = pUnlockInfo->clusters[secCmd];
    CHECK_RC(WriteHost2Jtag(jtagClusters,
                            pUnlockInfo->clusters[secCmd].chainLen,
                            pUnlockInfo->cmdData));

    //3a. Read back the secure keys from the security engine to give enough time for the SHA2
    //    callwlation to complete.
    jtagClusters[0] = pUnlockInfo->clusters[secChk];
    CHECK_RC(ReadHost2Jtag(jtagClusters,
                           pUnlockInfo->clusters[secChk].chainLen,
                           &pUnlockInfo->chkData));

    //4.Program JTAG_SEC_CMD chain with value 0x00 to clear out pending request.
    pUnlockInfo->cmdData.assign(pUnlockInfo->cmdData.size(), 0x00);
    jtagClusters[0] = pUnlockInfo->clusters[secCmd];
    CHECK_RC(WriteHost2Jtag(jtagClusters,
                            pUnlockInfo->clusters[secCmd].chainLen,
                            pUnlockInfo->cmdData));

    //5.Read JTAG JTAG_SEC_UNLOCK_STATUS register in all chiplets. This is a special register
    //  where the data is generated internally and gets distributed to all chains.
    jtagClusters.clear();
    for (UINT32 i = 0; i < pUnlockInfo->numChiplets; i++)
    {
        jtagClusters.push_back(GpuSubdevice::JtagCluster(
                        i, pUnlockInfo->clusters[selwnlock].instrId));
    }
    CHECK_RC(ReadHost2Jtag(jtagClusters,
                            pUnlockInfo->clusters[selwnlock].chainLen,
                            &pUnlockInfo->unlockData));

    //5a. Verify all of the requested features were unlocked.
    CHECK_RC(VerifyHost2JtagUnlockStatus(pUnlockInfo));

    Printf(pri, "UnlockHost2Jtag:End\n");
    return rc;
}

//------------------------------------------------------------------------------
void HopperGpuSubdevice::ApplyJtagChanges(UINT32 processFlag)
{
    RC rc;
    if (!IsEmulation() &&
        (Platform::GetSimulationMode() == Platform::Hardware) &&
        Platform::HasClientSideResman())
    {
        if (processFlag != PostRMInit)
            return;

        Tee::Priority pri = (m_VerboseJtag) ? Tee::PriNormal : Tee::PriLow;
        h2jUnlockInfo unlockInfo;
        UINT32 secChkSize = 0;
        UINT32 secCfgSize = 0;
        if (OK != (rc = GetSelwreKeySize(&secChkSize, &secCfgSize)))
        {
            Printf(pri, "WARNING... unable to get sizes of SelwreKeys...\n");
            Printf(pri, "Jtag changes will not be applied.\n");
            return;
        }

        secChkSize = (secChkSize+31) / 32;
        unlockInfo.selwreKeys.assign(secChkSize, 0x00);

        secCfgSize = (secCfgSize+31) / 32;
        unlockInfo.cfgMask.assign(secCfgSize, 0xFFFFFFFF);
        if (OK != (rc = GetH2jUnlockInfo(&unlockInfo)))
        {
            Printf(pri,
                   "WARNING... failed to get Host2Jtag Unlock Info..\n");
            Printf(pri, "Jtag changes will not be applied.\n");
            return;
        }

        if (OK != (rc = UnlockHost2Jtag(&unlockInfo)))
        {
            Printf(pri, "WARNING... Failed to unlock Jtag for access!\n");
        }
    }

    return;
}

//------------------------------------------------------------------------------
/* virtual */bool HopperGpuSubdevice::DoesFalconExist(UINT32 falconId)
{
    if (OK == InitFalconBases())
        return m_FalconBases.count(falconId) != 0;
    return false;
}

//------------------------------------------------------------------------------
//! \brief saves the falcon status [NS, LS or HS] into pLevel
//! \ param pRegisterBases is  array containing base addresses
//! of the various falcons
//! \param falconId is the id of the falcon for which mode is
//! queried
//! \param pLevel is address where mode of falconId is stored
//!
RC HopperGpuSubdevice::GetFalconStatus
(
    UINT32 falconId,
    UINT32 *pLevel
)
{
    RC rc;
    CHECK_RC(InitFalconBases());

    *pLevel = RegRd32(m_FalconBases[falconId] + Regs().LookupAddress(MODS_PFALCON_FALCON_SCTL));
    *pLevel = Regs().GetField(*pLevel, MODS_PFALCON_FALCON_SCTL_UCODE_LEVEL);

    return OK;
}

//------------------------------------------------------------------------------
// m_FalconBases map is filled with the falcon base addresses
// TODO: Verify that we actually have the falcon engines below.
/* virtual */RC HopperGpuSubdevice::InitFalconBases()
{
    if (!m_FalconBases.empty())
    {
        return OK;
    }

    RC rc = OK;
    LW2080_CTRL_GPU_GET_ENGINES_V2_PARAMS engineParams = { 0 };
    LwRmPtr pLwRm;

    CHECK_RC(pLwRm->Control(pLwRm->GetSubdeviceHandle(this),
                               LW2080_CTRL_CMD_GPU_GET_ENGINES_V2,
                               &engineParams,
                               sizeof (engineParams)));

    for (UINT32 i = 0; i < engineParams.engineCount; ++i)
    {
        switch (engineParams.engineList[i])
        {
            //Doesn't exist on GH100, not sure about GH20x yet.
            case LW2080_ENGINE_TYPE_LWENC0:
                m_FalconBases[LSF_FALCON_ID_LWENC0] = Regs().LookupAddress(MODS_FALCON_LWENC0_BASE);
            break;
            //Doesn't exist on GH100, not sure about GH20x yet.
            case LW2080_ENGINE_TYPE_LWENC1:
                m_FalconBases[LSF_FALCON_ID_LWENC1] = Regs().LookupAddress(MODS_FALCON_LWENC1_BASE);
            break;
            case LW2080_ENGINE_TYPE_GRAPHICS:
                m_FalconBases[LSF_FALCON_ID_FECS] =
                    Regs().LookupAddress(MODS_PGRAPH_PRI_FECS_FALCON_IRQSSET);
                m_FalconBases[LSF_FALCON_ID_GPCCS] =
                    Regs().LookupAddress(MODS_PGRAPH_PRI_GPCS_GPCCS_FALCON_IRQSSET);
            break;
            case LW2080_ENGINE_TYPE_LWDEC0:
                m_FalconBases[LSF_FALCON_ID_LWDEC] = Regs().LookupAddress(MODS_FALCON_LWDEC0_BASE);
            break;
// TODO : RM doesnt have defines for these yet
//            case LW2080_ENGINE_TYPE_LWDEC1:
//                m_FalconBases[LSF_FALCON_ID_LWDEC1] = Regs().LookupAddress(MODS_FALCON_LWDEC1_BASE);
//            break;
//            case LW2080_ENGINE_TYPE_LWDEC2:
//                m_FalconBases[LSF_FALCON_ID_LWDEC2] = Regs().LookupAddress(MODS_FALCON_LWDEC2_BASE);
//            break;
        }
    }

    m_FalconBases[LSF_FALCON_ID_PMU] = Regs().LookupAddress(MODS_FALCON_PWR_BASE);

    return rc;
}

//------------------------------------------------------------------------------
void HopperGpuSubdevice::PrintMonitorInfo(Gpu::MonitorFilter monFilter)
{
    PrintMonitorInfo(nullptr, monFilter);
}

//------------------------------------------------------------------------------
void HopperGpuSubdevice::PrintMonitorInfo(RegHal *pRegs, Gpu::MonitorFilter monFilter)
{
    if (nullptr == pRegs)
        pRegs = &Regs();

    bool graphicsExists = true;
    const bool monitorOnlyEngines = (monFilter == Gpu::MF_ONLY_ENGINES);

    UINT32 pbdmaChannelSize = pRegs->IsSupported(MODS_PPBDMA_CHANNEL) ?
        pRegs->LookupArraySize(MODS_PPBDMA_CHANNEL, 1) : 0;

    UINT32 pbdmaStatusSchedSize = pRegs->IsSupported(MODS_PPBDMA_STATUS_SCHED) ?
            pRegs->LookupArraySize(MODS_PPBDMA_STATUS_SCHED, 1) : 0;

    // Check to see if certain engines are removed for emulation.
    // An engine that is stubbed out should not have its status checked.
    if (IsEmulation())
    {
        if (pRegs->Test32(MODS_PBUS_EMULATION_STUB0_GR_REMOVED))
        {
            graphicsExists = false;
            Printf(Tee::PriNormal, "Graphics engine stubbed out for emulation.\n");
        }
    }

    string statusStr;

    if (graphicsExists)
    {
        // Acquire mutex for enabling/disabling SMC to ensure that SMC is not
        // enabled/disabled until SMC sensitive regs are read in this monitor
        // iteration
        Tasker::MutexHolder mh(GetSmcEnabledMutex());
        if (!IsSMCEnabled())
        {
            UINT32 PgraphStatus = pRegs->Read32(MODS_PGRAPH_STATUS);
            statusStr = Utility::StrPrintf(" PGRAPH=0x%08x", PgraphStatus);
        }
        else if (m_GetSmcRegHalsCb)
        {
            vector<SmcRegHal *> smcRegHals;

            // Acquire the mutex for SmcRegHal to ensure they are not freed
            // before SmcRegHal usage is complete here
            Tasker::MutexHolder mh(m_GetSmcRegHalsCb(&smcRegHals));

            for (auto const & lwrRegHal : smcRegHals)
            {
                UINT32 PgraphStatus = lwrRegHal->Read32(MODS_PGRAPH_STATUS);

                LW2080_CTRL_GPU_GET_HW_ENGINE_ID_PARAMS hwIdParams = { };
                hwIdParams.engineList[0] = LW2080_ENGINE_TYPE_GR(lwrRegHal->GetSmcEngineId());
                GetHwEngineIds(lwrRegHal->GetLwRmPtr(), 1, &hwIdParams);

                vector<GpuSubdevice::HwDevInfo>::const_iterator hwEngInfo = GetHwDevInfo().end();
                if ((hwIdParams.hwEngineID[0] != LW2080_CTRL_GPU_GET_HW_ENGINE_ID_NULL) &&
                    (hwIdParams.hwEngineID[0] != LW2080_CTRL_GPU_GET_HW_ENGINE_ID_ERROR)) //$
                {
                    hwEngInfo = GetHwEngineInfo(hwIdParams.hwEngineID[0]);
                }

                if (hwEngInfo != GetHwDevInfo().end())
                {
                    statusStr += Utility::StrPrintf(" PGRAPH(%u)=0x%08x",
                                                    hwEngInfo->hwInstance,
                                                    PgraphStatus);
                }
                else
                {
                    statusStr += Utility::StrPrintf(" PGRAPH(%u,%u)=0x%08x",
                                                    lwrRegHal->GetSwizzId(),
                                                    lwrRegHal->GetSmcEngineId(),
                                                    PgraphStatus);
                }
            }
        }
    }

    const UINT32 lwdecMask = GetFsImpl()->LwdecMask();
    for (UINT32 ii = 0; pRegs->IsSupported(MODS_PLWDEC_FALCON_IDLESTATE) &&
         ii < pRegs->LookupArraySize(MODS_PLWDEC_FALCON_IDLESTATE, 1); ii++)
    {
        if (!(lwdecMask & (1 << ii)))
            continue;
        UINT32 LwdecStatus  = pRegs->Read32(MODS_PLWDEC_FALCON_IDLESTATE, ii);
        statusStr += Utility::StrPrintf(" LWDEC%d=0x%08x", ii, LwdecStatus);
    }

    const UINT32 lwencMask = GetFsImpl()->LwencMask();
    for (UINT32 ii = 0; pRegs->IsSupported(MODS_PLWENC_FALCON_IDLESTATE) &&
         ii < pRegs->LookupArraySize(MODS_PLWENC_FALCON_IDLESTATE, 1); ii++)
    {
        if (!(lwencMask & (1 << ii)))
            continue;
        UINT32 LwencStatus  = pRegs->Read32(MODS_PLWENC_FALCON_IDLESTATE, ii);
        statusStr += Utility::StrPrintf(" LWENC%d=0x%08x", ii, LwencStatus);
    }

    const UINT32 ofaMask = GetFsImpl()->OfaMask();
    if (ofaMask)
    {
        // If for some reason there is more than one OFA then this code would need to
        // be adjusted lwrrently there is only one
        MASSERT(ofaMask == 0x1);
        statusStr +=
            Utility::StrPrintf(" OFA=0x%08x", pRegs->Read32(MODS_POFA_FALCON_IDLESTATE));
    }

    const UINT32 lwjpgMask = GetFsImpl()->LwjpgMask();
    for (UINT32 ii = 0; pRegs->IsSupported(MODS_PLWJPG_FALCON_IDLESTATE) &&
         ii < pRegs->LookupArraySize(MODS_PLWJPG_FALCON_IDLESTATE, 1); ii++)
    {
        if (!(lwjpgMask & (1 << ii)))
            continue;
        UINT32 lwjpgStatus  = pRegs->Read32(MODS_PLWJPG_FALCON_IDLESTATE, ii);
        statusStr += Utility::StrPrintf(" LWJPG%d=0x%08x", ii, lwjpgStatus);
    }

    Printf(Tee::PriNormal, "Status:%s\n", statusStr.c_str());

    if (!monitorOnlyEngines)
    {
        // Store the reads so we can print all values at once and avoid line splits since in
        // arch runs reading a register can cause model prints to the log
        string pbdmaSched = "LW_PPBDMA_STATUS_SCHED : ";
        for (UINT32 i = 0; i < pbdmaStatusSchedSize; ++i)
        {
            pbdmaSched +=
                Utility::StrPrintf("0x%08x ", pRegs->Read32(MODS_PPBDMA_STATUS_SCHED, i));
        }
        Printf(Tee::PriNormal, "%s\n", pbdmaSched.c_str());

        // Acquire mutex for enabling/disabling SMC to ensure that SMC is not
        // enabled/disabled until SMC sensitive regs are read in this monitor
        // iteration
        Tasker::MutexHolder mh(GetSmcEnabledMutex());
        if (!IsSMCEnabled())
        {
            Printf(Tee::PriNormal, "Runlist Status:\n");
            PrintRunlistInfo(pRegs);
        }
        else if (m_GetSmcRegHalsCb)
        {
            vector<SmcRegHal *> smcRegHals;

            // Acquire the mutex for SmcRegHal to ensure they are not freed
            // before SmcRegHal usage is complete here
            Tasker::MutexHolder mh(m_GetSmcRegHalsCb(&smcRegHals));

            set<LwRm *> printedClients;
            for (auto const & lwrSmcRegHal : smcRegHals)
            {
                if (printedClients.count(lwrSmcRegHal->GetLwRmPtr()))
                    continue;
                Printf(Tee::PriNormal, "Runlist Status for swizid %u:\n", lwrSmcRegHal->GetSwizzId());
                PrintRunlistInfo(lwrSmcRegHal);
                printedClients.insert(lwrSmcRegHal->GetLwRmPtr());
            }
        }

        const UINT32 reg_size = 6;
        UINT32 reg[reg_size] = { };
        for (UINT32 i = 0; i < pbdmaChannelSize; ++i)
        {
            string label = Utility::StrPrintf("PPBDMA(%d):", i);
            reg[0] = pRegs->Read32(MODS_PPBDMA_CHANNEL, i);
            reg[1] = pRegs->Read32(MODS_PPBDMA_PB_HEADER, i);
            reg[2] = pRegs->Read32(MODS_PPBDMA_GP_GET, i);
            reg[3] = pRegs->Read32(MODS_PPBDMA_GP_PUT, i);
            reg[4] = pRegs->Read32(MODS_PPBDMA_GET, i);
            reg[5] = pRegs->Read32(MODS_PPBDMA_PUT, i);
            if (0 != std::accumulate(reg, reg + reg_size, 0, logical_or<UINT32>()))
            {
                Printf(Tee::PriNormal, "%10s CHANNEL=0x%08x PB_HEADER=0x%08x GP_GET=0x%08x"
                        " GP_PUT=0x%08x GET=0x%08x PUT=0x%08x\n",
                        label.c_str(), reg[0], reg[1], reg[2], reg[3], reg[4], reg[5]);
                fill(reg, reg + reg_size, 0);
            }

            reg[0] = pRegs->Read32(MODS_PPBDMA_PB_COUNT, i);
            reg[1] = pRegs->Read32(MODS_PPBDMA_PB_FETCH, i);
            reg[2] = pRegs->Read32(MODS_PPBDMA_PB_HEADER, i);
            reg[3] = pRegs->Read32(MODS_PPBDMA_INTR_0, i);
            if (0 != std::accumulate(reg, reg + reg_size, 0, logical_or<UINT32>()))
            {
                Printf(Tee::PriNormal, "%10s PB_COUNT=0x%08x PB_FETCH=0x%08x PB_HEADER=0x%08x"
                       " INTR_0=0x%08x\n",
                        label.c_str(), reg[0], reg[1], reg[2], reg[3]);
                fill(reg, reg + reg_size, 0);
            }

            reg[0] = pRegs->Read32(MODS_PPBDMA_METHOD0, i);
            reg[1] = pRegs->Read32(MODS_PPBDMA_DATA0, i);
            reg[2] = pRegs->Read32(MODS_PPBDMA_METHOD1, i);
            reg[3] = pRegs->Read32(MODS_PPBDMA_DATA1, i);
            if (0 != std::accumulate(reg, reg + reg_size, 0, logical_or<UINT32>()))
            {
                Printf(Tee::PriNormal, "%10s METHOD0=0x%08x DATA0=0x%08x METHOD1=0x%08x"
                       " DATA1=0x%08x\n",
                        label.c_str(), reg[0], reg[1], reg[2], reg[3]);
                fill(reg, reg + reg_size, 0);
            }
        }
    }

    string ceStr = "CE Status : ";
    const UINT32 ceSize = pRegs->LookupArraySize(MODS_CE_LCE_STATUS, 1);
    for (UINT32 i = 0; i < ceSize; i++)
    {
        ceStr += Utility::StrPrintf("LCE%d=0x%08x ", i, pRegs->Read32(MODS_CE_LCE_STATUS, i));
    }
    Printf(Tee::PriNormal, "%s\n", ceStr.c_str());
}

//------------------------------------------------------------------------------
void HopperGpuSubdevice::GetMemCfgOverrides()
{
    JavaScriptPtr pJs;
    vector <UINT32> inpFbpRowBits;
    RC rc =  pJs->GetProperty(Gpu_Object, Gpu_PerFbpRowBits, &inpFbpRowBits);
    if (OK != rc || (inpFbpRowBits.empty()))
    {
        m_InpPerFbpRowBits.clear();
        Printf(Tee::PriDebug, "Note: fb_row_bits_perfbp isn't specified.\n");
        rc.Clear();
    }
    else
    {
        //Hardcoded value on any platform
        const UINT32 totalSupportedFbpas = Regs().Read32(MODS_PTOP_SCAL_NUM_FBPAS);
        if (inpFbpRowBits.size() > totalSupportedFbpas)
        {
            Printf(Tee::PriLow,
                    "Warning: We discard unsupported Fbpa Cfg request.\n");
            inpFbpRowBits.erase(inpFbpRowBits.begin() + totalSupportedFbpas,
                    inpFbpRowBits.end());
        }
        //Limits for GM20x, are they the same for Pascal?
        const UINT32 minRowBits =
            Regs().LookupValue(MODS_PFB_FBPA_CFG1_ROWA_10);
        const UINT32 maxRowBits =
            Regs().LookupValue(MODS_PFB_FBPA_CFG1_ROWA_17);
        const UINT32 cfg1RowBaseVal =
            10 - Regs().LookupValue(MODS_PFB_FBPA_CFG1_ROWA_10);
        const UINT32 activeFbps = GetFsImpl()->FbMask();
        MASSERT(activeFbps); // Should not be read as non zero (even RTL)
        for (UINT32 ii=0; ii < inpFbpRowBits.size(); ii++)
        {
            if (!(activeFbps & (1<<ii)))
            {
                Printf(Tee::PriDebug,
                        "%s: Skip floorswept fbp %u\n",
                        __FUNCTION__, (1<<ii));
                inpFbpRowBits[ii] = 0;
                continue;
            }

            // To maintain similarlity to Ref-Man. definitions, input values
            // are taken to be Register value + cfg1RowBaseVal
            if ((inpFbpRowBits[ii] < minRowBits + cfg1RowBaseVal) ||
                (inpFbpRowBits[ii] > maxRowBits + cfg1RowBaseVal))
            { //value is out of range; try next regval
                Printf(Tee::PriWarn,
                        "Fbp Row bits (%u) is out of range."
                        " Not overriding Fbp_%u \n", ii, inpFbpRowBits[ii]);
                inpFbpRowBits[ii] = 0;
                continue;
            }
        }
        m_InpPerFbpRowBits = inpFbpRowBits;
    }

    rc = pJs->GetProperty(Gpu_Object, Gpu_FbHbmNumSids, &m_FbNumSids);
    if (RC::OK != rc && RC::ILWALID_OBJECT_PROPERTY != rc)
    {
        Printf(Tee::PriError, "Failed with %s while fetching Gpu_FbHbmNumSids.\n", rc.Message());
    }
    rc.Clear();

    // TODO: m_FbDualRanks is deprecated in Hopper+.
    // Remove once tests have transitioned to use m_FbNumSids
    bool fbHbmDualRankEnabled = false;
    rc = pJs->GetProperty(Gpu_Object, Gpu_FbHbmDualRankEnabled, &fbHbmDualRankEnabled);
    if (OK == rc && fbHbmDualRankEnabled)
    {
        m_FbDualRanks = 0;
        pJs->GetProperty(Gpu_Object, Gpu_FbHbmBanksPerRank, &m_FbDualRanks);
        rc.Clear();
    }

    NamedProperty *pFbMc2MasterMaskProp;
    UINT32         fbMc2MasterMask = 0;
    if ((OK != GetNamedProperty("FbMc2MasterMask", &pFbMc2MasterMaskProp)) ||
        (OK != pFbMc2MasterMaskProp->Get(&fbMc2MasterMask)))
    {
        Printf(Tee::PriDebug, "FB MC2 master configuration is not valid!!\n");
    }
    else
    {
        m_FbMc2MasterMask = fbMc2MasterMask;
    }

    rc = pJs->GetProperty(Gpu_Object, Gpu_FbRowAMaxCount, &m_FbRowAMaxCount);
    rc.Clear();
    rc = pJs->GetProperty(Gpu_Object, Gpu_FbRowBMaxCount, &m_FbRowBMaxCount);
    rc.Clear();
    rc = pJs->GetProperty(Gpu_Object, Gpu_FbRefsbDualRequest, &m_FbDualRequest);
    rc.Clear();
    rc = pJs->GetProperty(Gpu_Object, Gpu_FbClamshell, &m_FbClamshell);
    rc.Clear();
    rc = pJs->GetProperty(Gpu_Object, Gpu_FbBanksPerRowBg, &m_FbBanksPerRowBg);
    rc.Clear();
}

//------------------------------------------------------------------------------
//! \brief Queries the read mask, write mask and start addresses of
//!  the various ACR regions
//! \param self explanatory
//!
RC HopperGpuSubdevice::GetWPRInfo
(
    UINT32 regionID,
    UINT32 *pReadMask,
    UINT32 *pWriteMask,
    UINT64 *pStartAddr,
    UINT64 *pEndAddr
)
{
    RC rc;

    RegHal& regs = GpuSubdevice::Regs();

    if (regionID == 1)
    {
        *pReadMask = regs.Read32(MODS_PFB_PRI_MMU_WPR_ALLOW_READ_WPR1);
        *pWriteMask = regs.Read32(MODS_PFB_PRI_MMU_WPR_ALLOW_WRITE_WPR1);
        *pStartAddr = regs.Read32(MODS_PFB_PRI_MMU_WPR1_ADDR_LO_VAL);
    }
    else if (regionID == 2)
    {
        *pReadMask = regs.Read32(MODS_PFB_PRI_MMU_WPR_ALLOW_READ_WPR2);
        *pWriteMask = regs.Read32(MODS_PFB_PRI_MMU_WPR_ALLOW_WRITE_WPR2);
        *pStartAddr = regs.Read32(MODS_PFB_PRI_MMU_WPR2_ADDR_LO_VAL);
    }
    else
    {
        // Only WPR1/WPR2 are supported on Ampere
        Printf(Tee::PriError, "Requested WPR info for invalid region (%u)\n", regionID);
        return RC::BAD_PARAMETER;
    }

    *pStartAddr = (*pStartAddr) << Regs().LookupAddress(MODS_PFB_PRI_MMU_VPR_ADDR_LO_ALIGNMENT);

    return rc;
}

/* static */ bool HopperGpuSubdevice::SetBootStraps
(
    Gpu::LwDeviceId deviceId
    ,const PciDevInfo& pciInfo
    ,UINT32 boot0Strap
    ,UINT32 boot3Strap
    ,UINT64 fbStrap
    ,UINT32 gc6Strap
)
{
    RC rc;
    // This routine is called before the GpuSubdevice is created, so we can't use the
    // regular ModsRegHal interface to actually set the registers, however by creating
    // a local reghaltable it can be used to query offsets and fields
    RegHalTable regs;
    regs.Initialize(Device::LwDeviceId(deviceId));

    UINT32 lwrBoot0Strap = 0;
    UINT32 lwrBoot3Strap = 0;
    UINT32 lwrGc6Strap = 0;
    const bool isGC6StrapSupported = regs.IsSupported(MODS_PGC6_SCI_STRAP);

    JavaScriptPtr pJs;

    lwrBoot0Strap = MEM_RD32((char *)pciInfo.LinLwBase + regs.LookupAddress(MODS_PEXTDEV_BOOT_0));
    lwrBoot3Strap = MEM_RD32((char *)pciInfo.LinLwBase + regs.LookupAddress(MODS_PEXTDEV_BOOT_3));

    if (isGC6StrapSupported)
        lwrGc6Strap = MEM_RD32((char*)pciInfo.LinLwBase + regs.LookupAddress(MODS_PGC6_SCI_STRAP));

    // Hopper Gen4 models are not compliant with PCI BAR resize specification as they do not
    // advertize the functionality in the AER block yet BAR resize still needs to function.
    bool bUseLegacyBarResize = false;
    UINT08 pciCapBase;
    if (RC::OK != Pci::FindCapBase(pciInfo.PciDomain, pciInfo.PciBus, pciInfo.PciDevice,
                                   pciInfo.PciFunction, PCI_CAP_ID_PCIE, &pciCapBase))
    {
        Printf(Tee::PriNormal,
               "Unable to find PCI capability base, using legacy Gen4 BAR resize\n");
        bUseLegacyBarResize = true;
    }
    else
    {
        UINT32 linkStatus;
        if (RC::OK != Platform::PciRead32(pciInfo.PciDomain, pciInfo.PciBus, pciInfo.PciDevice,
                                          pciInfo.PciFunction,
                                          pciCapBase + LW_PCI_CAP_PCIE_LINK_CAP,
                                          &linkStatus))
        {
            Printf(Tee::PriError,
                   "Unable to read PCI link capability, using legacy Gen4 BAR resize\n");
            bUseLegacyBarResize = true;
        }
        else
        {
            // Use PCI spec defined BAR resize for Gen5, Gen4 models do not advertize the
            // resizable BAR AER block
            bUseLegacyBarResize = DRF_VAL(_PCI_CAP, _PCIE_LINK_CAP, _LINK_SPEED, linkStatus) != 5;
        }
    }

    if (boot0Strap == 0xffffffff) // option '-boot_0_strap' not specified
    {
        // Default straps
        boot0Strap = lwrBoot0Strap;
    }
    else
    {
        Printf(Tee::PriNormal, "Boot0Strap set to 0x%x\n", boot0Strap);
    }

    // There are no defaults to set for this register on Pascal
    if (boot3Strap == 0xffffffff) // option '-boot_3_strap' not specified
    {
        boot3Strap = lwrBoot3Strap;
    }
    else
    {
        Printf(Tee::PriNormal, "Boot3Strap set to 0x%x\n", boot3Strap);
    }

    if (isGC6StrapSupported)
    {
        if (gc6Strap == 0xffffffff) // option '-gc6_strap' not specified
        {
            gc6Strap = lwrGc6Strap;
            // Set Lwdqro strap?
            INT32 openGL = 0;
            pJs->GetProperty(Gpu_Object, Gpu_StrapOpenGL, &openGL);
            // Now see if we need to apply the -openGL property
            if ((openGL != -1) ||
                (Platform::GetSimulationMode() != Platform::Hardware))
            {
                // On Pascal we default to Lwdqro mode not VdChip like other Gpus.
                if (openGL == -1)
                    openGL = 1;
                if (openGL != 0)
                {
                    Printf(Tee::PriLow, "Strap as Lwdqro\n");
                    regs.SetField(&gc6Strap, MODS_PGC6_SCI_STRAP_DEVID_SEL_REBRAND);
                }
                else
                {
                    Printf(Tee::PriLow, "Strap as VdChip\n");
                    regs.SetField(&gc6Strap, MODS_PGC6_SCI_STRAP_DEVID_SEL_ORIGINAL);
                }
            }

            INT32  strapRamcfg;
            pJs->GetProperty(Gpu_Object, Gpu_StrapRamcfg, &strapRamcfg);
            if ((strapRamcfg != -1) ||
                (Platform::GetSimulationMode() != Platform::Hardware))
            {
                if (strapRamcfg == -1)
                    strapRamcfg = 0;
                regs.SetField(&gc6Strap, MODS_PGC6_SCI_STRAP_RAMCFG, strapRamcfg);
            }
        }
        else
        {
            Printf(Tee::PriNormal, "Gc6Strap set to 0x%x\n", gc6Strap);
        }
    }

    // Only apply fbStrap value of zero on RTL.  Applying fbStrap of zero on
    // Fmodel/Amodel (i.e. forcing a default of 256M) breaks various AS2 tests
    if ((fbStrap == 0) && (Platform::GetSimulationMode() == Platform::RTL))
    {
        fbStrap = 256;
    }

    UINT32 legacyFbStrapRegAddress = 0;
    UINT32 legacyFbStrapRegValue   = 0;
    if (bUseLegacyBarResize && (fbStrap != 0))
    {
        Printf(Tee::PriNormal, "Using legacy Gen4 BAR resize\n");
        if ((fbStrap & (fbStrap - 1)) != 0)
        {
            Printf(Tee::PriError, "Invalid STRAP_FB setting : %llu\n", fbStrap);
            return false;
        }
        else
        {
            legacyFbStrapRegValue = Utility::BitScanReverse(fbStrap);

            // Normally we would check against BAR_SIZE_MAX via reghal but the manuals sill
            // say the max is 1TB instead of 8EB and there are no explicit sizes in the manuals
            // above 1TB either (43 represents 1ULL << 43 which is 8EB and the max for Hopper).
            // This should be changed to a reghal lookup once the manuals are updated
            static constexpr UINT32 maxBarSize = 43;
            if ((legacyFbStrapRegValue > maxBarSize) ||
                (legacyFbStrapRegValue < regs.LookupValue(MODS_EP_PCFG_GPU_PF_RESIZE_BAR_CTRL_BAR_SIZE_MIN)))
            {
                Printf(Tee::PriError, "Invalid STRAP_FB setting : %llu\n", fbStrap);
                return false;
            }
        }

        UINT16 capAddr = 0;
        UINT16 capSize = 0;
        if (Pci::GetExtendedCapInfo(pciInfo.PciDomain, pciInfo.PciBus,
                                    pciInfo.PciDevice, pciInfo.PciFunction,
                                    Pci::RESIZABLE_BAR_ECI, 0,
                                    &capAddr, &capSize) != RC::OK)
        {
#ifdef SIM_BUILD
            // Some RTLs have bad extended capability lists; use default address
            Printf(Tee::PriWarn,
                   "Cannot find BAR1 resize capability struct\n");
            capAddr = regs.LookupAddress(MODS_EP_PCFG_GPU_PF_RESIZE_BAR_HEADER);
#else
            Printf(Tee::PriError,
                   "Cannot find BAR1 resize capability struct\n");
            return false;
#endif
        }
        legacyFbStrapRegAddress = capAddr +
            regs.LookupAddress(MODS_EP_PCFG_GPU_PF_RESIZE_BAR_CTRL) -
            regs.LookupAddress(MODS_EP_PCFG_GPU_PF_RESIZE_BAR_HEADER);

        if (Platform::GetSimulationMode() == Platform::Hardware)
        {
            UINT32 lwrFbStrap = 0;
            if (Platform::PciRead32(pciInfo.PciDomain, pciInfo.PciBus,
                                    pciInfo.PciDevice, pciInfo.PciFunction,
                                    legacyFbStrapRegAddress,
                                    &lwrFbStrap) != RC::OK)
             {
                 Printf(Tee::PriError, "Failed to read BAR1 resize register\n");
                 return false;
             }
             lwrFbStrap = regs.GetField(lwrFbStrap,
                                        MODS_EP_PCFG_GPU_PF_RESIZE_BAR_CTRL_BAR_SIZE);
            if (legacyFbStrapRegValue > lwrFbStrap)
            {
                Printf(Tee::PriError, "Cannot expand BAR1 on Emulation/Silicon!\n");
                return false;
            }
        }
    }

    if (Platform::GetSimulationMode() == Platform::RTL)
    {
        Platform::EscapeWrite("LW_PEXTDEV_BOOT_0", regs.LookupAddress(MODS_PEXTDEV_BOOT_0),
                              4, boot0Strap);
        Platform::EscapeWrite("LW_PEXTDEV_BOOT_3", regs.LookupAddress(MODS_PEXTDEV_BOOT_3),
                              4, boot3Strap);
        if (isGC6StrapSupported)
        {
            Platform::EscapeWrite("LW_PGC6_SCI_STRAP", regs.LookupAddress(MODS_PGC6_SCI_STRAP),
                                  4, gc6Strap);
        }
    }


    if (boot0Strap != lwrBoot0Strap)
    {
        regs.SetField(&boot0Strap, MODS_PEXTDEV_BOOT_0_STRAP_OVERWRITE_ENABLED);
        MEM_WR32(static_cast<char*>(pciInfo.LinLwBase) + regs.LookupAddress(MODS_PEXTDEV_BOOT_0),
                 boot0Strap);
    }

    if (boot3Strap != lwrBoot3Strap)
    {
        regs.SetField(&boot3Strap, MODS_PEXTDEV_BOOT_3_STRAP_1_OVERWRITE_ENABLED);
        MEM_WR32((char *)pciInfo.LinLwBase + regs.LookupAddress(MODS_PEXTDEV_BOOT_3), boot3Strap);
    }

    if (fbStrap != 0)
    {
        if (bUseLegacyBarResize)
        {
            UINT32 bar1Resize = 0;
            if (Platform::PciRead32(pciInfo.PciDomain, pciInfo.PciBus,
                                    pciInfo.PciDevice, pciInfo.PciFunction,
                                    legacyFbStrapRegAddress,
                                    &bar1Resize) != RC::OK)
            {
                Printf(Tee::PriError, "Failed to read BAR1 resize register\n");
                return false;
            }

            regs.SetField(&bar1Resize,
                          MODS_EP_PCFG_GPU_PF_RESIZE_BAR_CTRL_BAR_SIZE,
                          legacyFbStrapRegValue);
            if (Platform::PciWrite32(pciInfo.PciDomain, pciInfo.PciBus,
                                     pciInfo.PciDevice, pciInfo.PciFunction,
                                     legacyFbStrapRegAddress,
                                     bar1Resize) != RC::OK)
            {
                Printf(Tee::PriError, "Failed to write BAR1 resize register\n");
                return false;
            }
        }
        else if (RC::OK != Pci::ResizeBar(pciInfo.PciDomain, pciInfo.PciBus, pciInfo.PciDevice,
                                          pciInfo.PciFunction, pciCapBase, 1, fbStrap))
        {
            Printf(Tee::PriError, "Failed to resize BAR1\n");
            return false;
        }
    }

    if (isGC6StrapSupported && (gc6Strap != lwrGc6Strap))
    {
        MEM_WR32((char*)pciInfo.LinLwBase + regs.LookupAddress(MODS_PGC6_SCI_STRAP_OVERRIDE),
                 gc6Strap);
        MEM_WR32((char*)pciInfo.LinLwBase + regs.LookupAddress(MODS_PGC6_SCI_STRAP),
                 gc6Strap);
    }

    return true;
}

//------------------------------------------------------------------------------
// Pascal's definition of _STRAP_RAMCFG changed.
UINT32 HopperGpuSubdevice::MemoryStrap()
{
    return Regs().Read32(MODS_PGC6_SCI_STRAP_RAMCFG);
}

RC HopperGpuSubdevice::GetSmEccEnable(bool *pEnable, bool *pOverride) const
{
    UINT32 value = Regs().Read32(MODS_FUSE_FEATURE_OVERRIDE_ECC);
    if (pEnable)
    {
        *pEnable = Regs().TestField(value, MODS_FUSE_FEATURE_OVERRIDE_ECC_SM_LRF_ENABLED);
    }

    if (pOverride)
    {
        *pOverride = Regs().TestField(value, MODS_FUSE_FEATURE_OVERRIDE_ECC_SM_LRF_OVERRIDE_TRUE);
    }

    return OK;
}

//-------------------------------------------------------------------------------
RC HopperGpuSubdevice::SetSmEccEnable(bool bEnable, bool bOverride)
{
    UINT32 value = Regs().Read32(MODS_FUSE_FEATURE_OVERRIDE_ECC);
    Regs().SetField(&value, MODS_FUSE_FEATURE_OVERRIDE_ECC_SM_LRF, bEnable ? 1 : 0);
    Regs().SetField(&value, MODS_FUSE_FEATURE_OVERRIDE_ECC_SM_LRF_OVERRIDE,
                    bOverride ? 1 : 0);
    Regs().Write32(MODS_FUSE_FEATURE_OVERRIDE_ECC, value);

    return OK;
}

//-------------------------------------------------------------------------------
UINT32 HopperGpuSubdevice::GetMmeInstructionRamSize()
{
    // Ref: https://p4viewer.lwpu.com/getfile///hw/doc/gpu/turing/turing/design/Functional_Descriptions/Turing_MME64_FD.docx //$
    // Section 5.1.3 MME CPU
    return 3072; // 3K of 32-bit chunks
}

//-------------------------------------------------------------------------------
UINT32 HopperGpuSubdevice::GetMmeVersion()
{
    // This will be used by MMESimulator to pick the macros in order
    // to get the value of method released by MME
    return 2;
}

//------------------------------------------------------------------------------
UINT32 HopperGpuSubdevice::GetPLLCfg(PLLType Pll) const
{
    if (Pll == GPC_PLL)
    {
        return Regs().Read32(MODS_PTRIM_SYS_GPCPLL_CFG);
    }
    else
    {
        return GpuSubdevice::GetPLLCfg(Pll);
    }

}

//------------------------------------------------------------------------------
void HopperGpuSubdevice::SetPLLCfg(PLLType Pll, UINT32 cfg)
{
    if (Pll == GPC_PLL)
    {
        Regs().Write32(MODS_PTRIM_SYS_GPCPLL_CFG, cfg);
    }
    else
    {
        GpuSubdevice::SetPLLCfg(Pll, cfg);
    }
}

//------------------------------------------------------------------------------
void HopperGpuSubdevice::SetPLLLocked(PLLType Pll)
{
    UINT32 Reg = GetPLLCfg(Pll);

    switch (Pll)
    {
        case SP_PLL0:
            Regs().SetField(&Reg, MODS_PVTRIM_SYS_SPPLL0_CFG_EN_LCKDET_POWER_ON);
            break;
        case SP_PLL1:
            Regs().SetField(&Reg, MODS_PVTRIM_SYS_SPPLL1_CFG_EN_LCKDET_POWER_ON);
            break;
        case SYS_PLL:
            Regs().SetField(&Reg, MODS_PTRIM_SYS_SYSPLL_CFG_EN_LCKDET_POWER_ON);
            break;
        case GPC_PLL:
            // use broadcast register
            Regs().SetField(&Reg, MODS_PTRIM_SYS_GPCPLL_CFG_EN_LCKDET_POWER_ON);
            break;
        case XBAR_PLL:
            Regs().SetField(&Reg, MODS_PTRIM_SYS_XBARPLL_CFG_EN_LCKDET_POWER_ON);
            break;
        default:
            MASSERT(!"Invalid PLL type.");
    }

    SetPLLCfg(Pll, Reg);
}

//------------------------------------------------------------------------------
bool HopperGpuSubdevice::IsPLLLocked(PLLType Pll) const
{
    UINT32 Reg = GetPLLCfg(Pll);

    switch (Pll)
    {
        case SP_PLL0:
            return Regs().TestField(Reg, MODS_PVTRIM_SYS_SPPLL0_CFG_PLL_LOCK_TRUE);
        case SP_PLL1:
            return Regs().TestField(Reg, MODS_PVTRIM_SYS_SPPLL1_CFG_PLL_LOCK_TRUE);
        case SYS_PLL:
            return Regs().TestField(Reg, MODS_PTRIM_SYS_SYSPLL_CFG_PLL_LOCK_TRUE);
        case GPC_PLL:
            // use broadcast register
            return Regs().TestField(Reg, MODS_PTRIM_SYS_GPCPLL_CFG_PLL_LOCK_TRUE);
        case XBAR_PLL:
            return Regs().TestField(Reg, MODS_PTRIM_SYS_XBARPLL_CFG_PLL_LOCK_TRUE);
        default:
            MASSERT(!"Invalid PLL type.");
    }
    return false;
}

/* virtual */
void HopperGpuSubdevice::GetPLLList(vector<PLLType> *pPllList) const
{
    static const PLLType MyPllList[] = { SP_PLL0, SP_PLL1, SYS_PLL, GPC_PLL,
        XBAR_PLL }; // No LTC on Volta and up.

    MASSERT(pPllList != NULL);
    pPllList->resize(NUMELEMS(MyPllList));
    copy(&MyPllList[0], &MyPllList[NUMELEMS(MyPllList)], pPllList->begin());
}

//-------------------------------------------------------------------------------
bool HopperGpuSubdevice::IsPLLEnabled(PLLType Pll) const
{
    if (Pll == GPC_PLL)
    {
        return Regs().Test32(MODS_PTRIM_SYS_GPCPLL_CFG_ENABLE_YES);
    }
    else
    {
        return GpuSubdevice::IsPLLEnabled(Pll);
    }
}

//------------------------------------------------------------------------------
RC HopperGpuSubdevice::PrepareEndOfSimCheck(Gpu::ShutDownMode mode)
{
    RC rc;

    const bool bWaitgfxDeviceIdle = true;
    const bool bForceFenceSyncBeforeEos = true;
    CHECK_RC(PrepareEndOfSimCheckImpl(mode, bWaitgfxDeviceIdle, bForceFenceSyncBeforeEos));

    return rc;
}

//------------------------------------------------------------------------------
void HopperGpuSubdevice::EndOfSimDelay() const
{
    Platform::Delay(5);   // Delay 5us before deasserting END_OF_SIM
}

//-----------------------------------------------------------------------------
/*virtual*/ bool HopperGpuSubdevice::PollAllChannelsIdle(LwRm* pLwRm)
{
    vector<LwU32> engines;
    GpuDevice* pGpuDev = GetParentDevice();
    if (pLwRm == nullptr)
        pLwRm = Regs().GetLwRmPtr();

    pGpuDev->GetEngines(&engines, pLwRm);
    UINT32 count = (UINT32) engines.size();

    LW2080_CTRL_GPU_GET_HW_ENGINE_ID_PARAMS hwIdParams = { };
    memcpy(hwIdParams.engineList, &engines[0], sizeof(UINT32) * count);
    if (GetHwEngineIds(pLwRm, count, &hwIdParams) != OK)
    {
        MASSERT(!"Failed to retrieve HW engine Ids!");
    }

    LW2080_CTRL_GPU_GET_ENGINE_RUNLIST_PRI_BASE_PARAMS baseParams = { };
    memcpy(baseParams.engineList, &engines[0], sizeof(UINT32) * count);
    if (GetRunlistPriBases(pLwRm, count, &baseParams) != OK)
    {
        MASSERT(!"Failed to retrieve runlist pri bases!");
    }

    for (UINT32 i =0; i < count; i++)
    {
        const LwU32 priBase = baseParams.runlistPriBase[i];
        if ( priBase == LW2080_CTRL_GPU_GET_ENGINE_RUNLIST_PRI_BASE_NULL ||
             priBase == LW2080_CTRL_GPU_GET_ENGINE_RUNLIST_PRI_BASE_ERROR)
        {
            continue;
        }

        if (IsSMCEnabled())
        {
            if (m_GetSmcRegHalsCb)
            {
                vector<SmcRegHal *> smcRegHals;

                // Acquire the mutex for SmcRegHal to ensure they are not freed
                // before SmcRegHal usage is complete here
                Tasker::MutexHolder mh(m_GetSmcRegHalsCb(&smcRegHals));

                for (auto const & lwrRegHal : smcRegHals)
                {
                    if (pLwRm == lwrRegHal->GetLwRmPtr())
                    {
                        if ((lwrRegHal->Test32(engines[i], MODS_RUNLIST_INFO_ENG_IDLE_FALSE)) ||
                            (lwrRegHal->Test32(engines[i], MODS_RUNLIST_INFO_RUNLIST_IDLE_FALSE)))
                        {
                            return false;
                        }
                        if ((hwIdParams.hwEngineID[i] != LW2080_CTRL_GPU_GET_HW_ENGINE_ID_NULL) &&
                            (hwIdParams.hwEngineID[i] != LW2080_CTRL_GPU_GET_HW_ENGINE_ID_ERROR))
                        {
                            auto hwEngInfo = GetHwEngineInfo(hwIdParams.hwEngineID[i]);
                            if (hwEngInfo != GetHwDevInfo().end())
                            {
                                if (lwrRegHal->Test32(engines[i], MODS_RUNLIST_ENGINE_STATUS0_ENGINE_BUSY, hwEngInfo->runlistId))
                                {
                                    return false;
                                }
                            }
                        }
                    }
                }
            }
        }
        else
        {
            if (Regs().Test32(engines[i], MODS_RUNLIST_INFO_ENG_IDLE_FALSE) ||
                 Regs().Test32(engines[i], MODS_RUNLIST_INFO_RUNLIST_IDLE_FALSE))
            {
                return false;
            }
            if ((hwIdParams.hwEngineID[i] != LW2080_CTRL_GPU_GET_HW_ENGINE_ID_NULL) &&
                (hwIdParams.hwEngineID[i] != LW2080_CTRL_GPU_GET_HW_ENGINE_ID_ERROR))
            {
                auto hwEngInfo = GetHwEngineInfo(hwIdParams.hwEngineID[i]);
                if (hwEngInfo != GetHwDevInfo().end())
                {
                    if (Regs().Test32(engines[i], MODS_RUNLIST_ENGINE_STATUS0_ENGINE_BUSY, hwEngInfo->runlistId))
                    {
                        return false;
                    }
                }
            }
        }
    }

    return true;
}

//------------------------------------------------------------------------------
RC HopperGpuSubdevice::GetMaxCeBandwidthKiBps(UINT32 *pBwKiBps)
{
    MASSERT(pBwKiBps);

    RC rc;
    UINT64 sysClockFreq;
    if (IsEmOrSim())
    {
        static const UINT64 POR_SYSCLK_FREQ = 868000000ULL;
        sysClockFreq = POR_SYSCLK_FREQ;
    }
    else
    {
        CHECK_RC(GetClock(Gpu::ClkSys, &sysClockFreq, nullptr, nullptr, nullptr));
    }

    // Clock speed in kHz
    FLOAT64 fBwKBps = static_cast<FLOAT64>(sysClockFreq) / 1000.0;

    // HSCE has a 32B interface to HUB. When making 256B requests, HSCE sends 1
    // cycle of headers (read and write header combo) + 8 cycles of write data.
    // So CE can at best send 256/9 = 28.44 B/cycle. CE runs on sysclk, so
    // whatever the sysclk is, will give the max CE BW. GP100 is targeting
    // 720MHz. So HSCE max BW will be 20.47GB/sec. The 28.44 B/cycle is only if
    // requests are 256B. Smaller requests will result in higher header overhead
    // and lower data Bytes per cycle. The CE interface to HUB is not changing.
    // So this is not changing from Pascal to Volta.
    fBwKBps *= 28.44;

    // Colwert Mbps to KiBps
    *pBwKiBps = static_cast<UINT32>(fBwKBps * 1000.0 / 1024.0);

    return rc;
}

//-----------------------------------------------------------------------------
/* static */ bool HopperGpuSubdevice::PollForFBStopAck(void *pVoidPollArgs)
{
    GpuSubdevice* subdev = static_cast<GpuSubdevice*>(pVoidPollArgs);
    return subdev->Regs().Test32(MODS_PPWR_PMU_GPIO_INPUT_FB_STOP_ACK_TRUE);
}

//------------------------------------------------------------------------------
RC HopperGpuSubdevice::AssertFBStop()
{
    RC rc;
    Regs().Write32(MODS_PPWR_PMU_GPIO_OUTPUT_SET, 0x4);

    // Wait for ACK
    CHECK_RC(POLLWRAP_HW(PollForFBStopAck, static_cast<void*>(this), 1000));

    return rc;
}

//------------------------------------------------------------------------------
void HopperGpuSubdevice::DeAssertFBStop()
{
    Regs().Write32(MODS_PPWR_PMU_GPIO_OUTPUT_CLEAR, 0x4);
}

//-----------------------------------------------------------------------------
// Return a vector of offset, value pairs for all of the strap registers.
/*virtual */
RC HopperGpuSubdevice::GetStraps(vector<pair<UINT32, UINT32>>* pStraps)
{
    pair <UINT32, UINT32> boot0(Regs().LookupAddress(MODS_PEXTDEV_BOOT_0),
                                Regs().Read32(MODS_PEXTDEV_BOOT_0));
    pair <UINT32, UINT32> boot3(Regs().LookupAddress(MODS_PEXTDEV_BOOT_3),
                                Regs().Read32(MODS_PEXTDEV_BOOT_3));
    pStraps->clear();
    pStraps->push_back(boot0);
    pStraps->push_back(boot3);

    if (Regs().IsSupported(MODS_PGC6_SCI_STRAP))
    {
        pair <UINT32, UINT32> sci(Regs().LookupAddress(MODS_PGC6_SCI_STRAP),
                                  Regs().Read32(MODS_PGC6_SCI_STRAP));
        pStraps->push_back(sci);
    }
    return OK;
}

//*****************************************************************************
// Load the IrqInfo struct (used to hook/unhook interrupts) with the
// default info from GpuSubdevice, and Ampere mask registers.
//
void HopperGpuSubdevice::LoadIrqInfo
(
    IrqParams* pIrqInfo,
    UINT08    irqType,
    UINT32    irqNumber,
    UINT32    maskInfoCount
)
{
    const RegHal& regs = GpuSubdevice::Regs();

    GpuSubdevice::LoadIrqInfo(pIrqInfo, irqType, irqNumber, maskInfoCount);
    for (unsigned intrTree = 0; intrTree < maskInfoCount; ++intrTree)
    {
        auto pMask = &pIrqInfo->maskInfoList[intrTree];
        pMask->irqPendingOffset = regs.LookupAddress(
                MODS_VIRTUAL_FUNCTION_PRIV_CPU_INTR_TOP, intrTree);
        pMask->irqEnabledOffset = regs.LookupAddress(
                MODS_VIRTUAL_FUNCTION_PRIV_CPU_INTR_TOP_EN_SET, intrTree);
        pMask->irqEnableOffset = regs.LookupAddress(
                MODS_VIRTUAL_FUNCTION_PRIV_CPU_INTR_TOP_EN_SET, intrTree);
        pMask->irqDisableOffset = regs.LookupAddress(
                MODS_VIRTUAL_FUNCTION_PRIV_CPU_INTR_TOP_EN_CLEAR, intrTree);
        pMask->andMask          = 0;
        pMask->orMask           = 0xFFFFFFFF;
        pMask->maskType         = MODS_XP_MASK_TYPE_IRQ_DISABLE;
    }
}

void HopperGpuSubdevice::SetSWIntrPending
(
    unsigned intrTree,
    bool pending
)
{
    MASSERT(intrTree < GetNumIntrTrees());
    RegHal& regs = GpuSubdevice::Regs();
    const UINT32 doorbellVector =
        regs.LookupValue(MODS_CTRL_CPU_DOORBELL_VECTORID_VALUE_CONSTANT);

    // Make sure we're controlling the right interrupt tree.
    // In Ampere, it's moot since there's only one tree, which
    // supports up to 2048 interrupt vectors.  (Each bit in
    // CPU_INTR_TOP maps to two 32-bit leaf registers, so
    // 2*32*32=2048).  This MASSERT is just in case a future chip
    // supports more trees.
    //
    MASSERT(intrTree == doorbellVector / 2048);

    // Set or clear the doorbell interrupt
    //
    if (pending)
    {
        regs.Write32(
                MODS_VIRTUAL_FUNCTION_PRIV_CPU_INTR_LEAF_TRIGGER_VECTOR,
                doorbellVector);
    }
    else
    {
        regs.Write32(MODS_VIRTUAL_FUNCTION_PRIV_CPU_INTR_LEAF,
                     doorbellVector / 32, 1U << (doorbellVector % 32));
    }
}

void HopperGpuSubdevice::SetIntrEnable
(
    unsigned intrTree,
    IntrEnable state
)
{
    MASSERT(intrTree < GetNumIntrTrees());
    RegHal& regs          = GpuSubdevice::Regs();
    UINT32  regClearValue = 0;
    UINT32  regSetValue   = 0;

    switch (state)
    {
        case intrDisabled:
            regSetValue   = 0;
            regClearValue = ~regSetValue;
            break;
        case intrHardware:
            regClearValue = 0;
            regSetValue   = ~regClearValue;
            break;
        case intrSoftware:
        {
            const UINT32 doorbellVector =
                regs.LookupValue(MODS_CTRL_CPU_DOORBELL_VECTORID_VALUE_CONSTANT);
            MASSERT(doorbellVector/2048 == intrTree); //See SetSWIntrPending
            const UINT32 leafIndex = doorbellVector / 32;
            const UINT32 leafBit   = 1U << (doorbellVector % 32);
            regSetValue = 1U << ((leafIndex/2) % 32); // 2 leafs per top bit
            regClearValue = ~regSetValue;
            regs.Write32(MODS_VIRTUAL_FUNCTION_PRIV_CPU_INTR_LEAF_EN_CLEAR,
                         leafIndex, ~leafBit);
            regs.Write32(MODS_VIRTUAL_FUNCTION_PRIV_CPU_INTR_LEAF_EN_SET,
                         leafIndex, leafBit);
            break;
        }
        default:
            MASSERT(!"Invalid state");
        return;
    }
    regs.Write32(MODS_VIRTUAL_FUNCTION_PRIV_CPU_INTR_TOP_EN_CLEAR, intrTree,
                 regClearValue);
    regs.Write32(MODS_VIRTUAL_FUNCTION_PRIV_CPU_INTR_TOP_EN_SET, intrTree,
                 regSetValue);
}

unsigned HopperGpuSubdevice::GetNumIntrTrees() const
{
    return 1;
}

//------------------------------------------------------------------------------
UINT32 HopperGpuSubdevice::Io3GioConfigPadStrap()
{
    // The fuses tied to this API don't exist on Hopper. see http://lwbugs/200709054
    MASSERT(!"This function is deprectated!");
    return 0;
}

//------------------------------------------------------------------------------
UINT32 HopperGpuSubdevice::RamConfigStrap()
{
    return Regs().Read32(MODS_PGC6_SCI_STRAP_RAMCFG);
}

bool HopperGpuSubdevice::IsQuadroEnabled()
{
    bool fuseEnabled;

    if (Platform::GetSimulationMode() == Platform::Hardware)
    {
        fuseEnabled = !(Regs().Test32(MODS_FUSE_OPT_OPENGL_EN_DATA_INIT));
    }
    else
    {
        fuseEnabled = true;
    }

    if (fuseEnabled)
    {
        if (Regs().Test32(MODS_PTOP_DEBUG_1_OPENGL_ON))
        {
            return true;
        }
    }

    return false;
}

//------------------------------------------------------------------------------
RC HopperGpuSubdevice::GetAteSpeedo(UINT32 SpeedoNum, UINT32 *pSpeedo, UINT32 *pVersion)
{
    MASSERT(pSpeedo);
    MASSERT(pVersion);
    RC rc;

    if (SpeedoNum >= NumAteSpeedo())
    {
        return RC::BAD_PARAMETER;
    }

    if (Regs().Test32(MODS_FUSE_SPEEDOINFO_PRIV_LEVEL_MASK_READ_PROTECTION_LEVEL2_ENABLED_FUSE1))
    {
        Printf(Tee::PriDebug, "Speedo info is priv-protected. Use HULK to view.\n");
        return RC::UNSUPPORTED_FUNCTION;
    }
    UINT32 SpeedoData = RegRd32(Regs().LookupAddress(MODS_FUSE_OPT_SPEEDO0) +
                                SpeedoNum * sizeof(UINT32));
    *pSpeedo = SpeedoData;
    *pVersion = Regs().Read32(MODS_FUSE_OPT_SPEEDO_REV_DATA);

    return rc;
}

//-----------------------------------------------------------------------------
RC HopperGpuSubdevice::GetAteSramIddq(UINT32 *pIddq)
{
    MASSERT(pIddq);

    RC rc;
    if (!Regs().IsSupported(MODS_FUSE_OPT_TMDS_VOLT_CTRL_CYA0))
    {
        rc = RC::UNSUPPORTED_FUNCTION;
    }
    else
    {
        UINT32 IddqData = Regs().Read32(MODS_FUSE_OPT_TMDS_VOLT_CTRL_CYA0_DATA);
        if (IddqData == 0)
        {
            rc = RC::UNSUPPORTED_FUNCTION;
        }
        *pIddq    = IddqData * (HasBug(598305) ? 50 : 1);
    }

    return rc;
}

//-----------------------------------------------------------------------------
RC HopperGpuSubdevice::GetAteMsvddIddq(UINT32 *pIddq)
{
    // GH100 will have separate LWVDD and MSVDD IDDQ values fused
    // This function get the MSVDD IDDQ value. IDDQ fusing: opt_iddq_cp = MSVDD IDDQ
    MASSERT(pIddq);

    if (Regs().Test32(MODS_FUSE_IDDQINFO_PRIV_LEVEL_MASK_READ_PROTECTION_LEVEL0_DISABLE))
    {
        Printf(Tee::PriDebug, "IDDQ info is priv-protected. Use HULK to view.\n");
        return RC::PRIV_LEVEL_VIOLATION;
    }

    UINT32 iddqData = Regs().Read32(MODS_FUSE_OPT_IDDQ_CP_DATA);

    *pIddq = 0;
    if (!iddqData)
    {
        return RC::UNSUPPORTED_FUNCTION;
    }

    *pIddq = iddqData * (HasBug(598305) ? 50 : 1);

    return RC::OK;
}

//-----------------------------------------------------------------------------
RC HopperGpuSubdevice::GetSramBin(INT32* pSramBin)
{
    if (Regs().Test32(MODS_FUSE_SRAM_VMIN_PRIV_LEVEL_MASK_READ_PROTECTION_LEVEL2_ENABLED_FUSE1))
    {
        Printf(Tee::PriDebug, "SRAM VMin bin info is priv-protected. Use HULK to view.\n");
        return RC::UNSUPPORTED_FUNCTION;
    }

    // The SRAM Vmin Bin value is a 4-bit value
    *pSramBin = Regs().Read32(MODS_FUSE_OPT_SRAM_VMIN_BIN) & 0xF;

    return RC::OK;
}

//-----------------------------------------------------------------------------
/*virtual*/ void HopperGpuSubdevice::EnablePwrBlock()
{
    Regs().Write32(MODS_PPWR_FALCON_ENGINE_RESET_FALSE);
}

//-----------------------------------------------------------------------------
/*virtual*/ void HopperGpuSubdevice::DisablePwrBlock()
{
    Regs().Write32(MODS_PPWR_FALCON_ENGINE_RESET_TRUE);
}

//-----------------------------------------------------------------------------
/*virtual*/ void HopperGpuSubdevice::EnableSecBlock()
{
    Regs().Write32(MODS_PSEC_FALCON_ENGINE_RESET_FALSE);
}

//-----------------------------------------------------------------------------
/*virtual*/ void HopperGpuSubdevice::DisableSecBlock()
{
    Regs().Write32(MODS_PSEC_FALCON_ENGINE_RESET_TRUE);
}

//------------------------------------------------------------------------------
UINT32 HopperGpuSubdevice::NumAteSpeedo() const
{
    // refer to LW_FUSE_OPT_SPEEDO0/LW_FUSE_OPT_SPEEDO1/etc in
    // drivers/common/inc/hwref/maxwell/gm107/dev_fuse.h
    return 3;
}

//------------------------------------------------------------------------------
RC HopperGpuSubdevice::GetAteIddq(UINT32 *pIddq, UINT32 *pVersion)
{
    // Note: There are 3 implementations for reading ATE Iddq info.
    // 1.GT21xGpuSubdevice::GetAteIddq()
    // 2.GpuSubdevice::GetAteIddq()
    // 3.GF11xGpuSubdevice::GetAteIddq()
    // 1&2 algorithm is identical, only register access is different.
    // 3.Writes to the LW_PTOP_DEBUG_1_FUSES_VISIBLE field which is undefined
    //   for Maxwell.

    // Maxwell has priv_level_selwrity feature that will prevent access to the
    // iddq version and value when its enabled. During normal bringup and
    // production testing this feature is disabled. At the end of testing the
    // feature is enabled and access is denied.
    MASSERT(pIddq && pVersion);

    if (Regs().Test32(MODS_FUSE_SPEEDOINFO_PRIV_LEVEL_MASK_READ_PROTECTION_LEVEL2_ENABLED_FUSE1))
    {
        Printf(Tee::PriDebug, "IDDQ info is priv-protected. Use HULK to view.\n");
        return RC::UNSUPPORTED_FUNCTION;
    }

    UINT32 IddqData = Regs().Read32(MODS_FUSE_OPT_IDDQ_DATA);
    UINT32 IddqRev = Regs().Read32(MODS_FUSE_OPT_IDDQ_REV_DATA);

    RC rc;
    if ((IddqRev == 0) && (IddqData == 0))
    {
        rc = RC::UNSUPPORTED_FUNCTION;
    }

    *pIddq    = IddqData * (HasBug(598305) ? 50 : 1);
    *pVersion = IddqRev;

    return rc;
}

//------------------------------------------------------------------------------
RC HopperGpuSubdevice::GetAteRev(UINT32* pRevision)
{
    *pRevision = Regs().Read32(MODS_FUSE_OPT_LWST_ATE_REV_DATA);
    return OK;
}

//------------------------------------------------------------------------------
UINT32 HopperGpuSubdevice::GetMaxCeCount() const
{
    return Regs().Read32(MODS_PTOP_SCAL_NUM_CES);
}

//------------------------------------------------------------------------------
UINT32 HopperGpuSubdevice::GetMaxPceCount() const
{
    return Regs().LookupAddress(MODS_CE_PCE2LCE_CONFIG__SIZE_1);
}

//------------------------------------------------------------------------------
UINT32 HopperGpuSubdevice::GetMaxGrceCount() const
{
    return Regs().LookupAddress(MODS_CE_GRCE_CONFIG__SIZE_1);
}

//------------------------------------------------------------------------------
UINT32 HopperGpuSubdevice::GetMaxSyspipeCount() const
{
    return Regs().LookupAddress(MODS_SCAL_LITTER_MAX_NUM_SMC_ENGINES);
}

//------------------------------------------------------------------------------
RC HopperGpuSubdevice::GetPceLceMapping(LW2080_CTRL_CE_SET_PCE_LCE_CONFIG_PARAMS* pParams)
{
    MASSERT(pParams);
    *pParams = {};

    RC rc;
    RegHal& regs = Regs();

    for (UINT32 i = 0; i < GetMaxPceCount(); i++)
        pParams->pceLceMap[i] = regs.Read32(MODS_CE_PCE2LCE_CONFIG_PCE_ASSIGNED_LCE, i);
    for (UINT32 i = 0; i < GetMaxGrceCount(); i++)
        pParams->grceSharedLceMap[i] = regs.Read32(MODS_CE_GRCE_CONFIG_SHARED_LCE, i);

    return rc;
}

//------------------------------------------------------------------------------
UINT32 HopperGpuSubdevice::GetMmeShadowRamSize()
{
    // Ref: https://p4viewer.lwpu.com/getfile///hw/doc/gpu/turing/turing/design/Functional_Descriptions/Turing_MME64_FD.docx //$
    // Section 5.1.4 MME Shadow RAM

    return static_cast<UINT32>(6_KB) / 4; // 6KB divided into 32-bit chunks
}

//-------------------------------------------------------------------------------
UINT32 HopperGpuSubdevice::GetMaxZlwllCount() const
{
    return Regs().Read32(MODS_PTOP_SCAL_NUM_ZLWLL_BANKS);
}

//------------------------------------------------------------------------------
bool HopperGpuSubdevice::IsDisplayFuseEnabled()
{
    if (Regs().Test32(MODS_FUSE_STATUS_OPT_DISPLAY_DATA_DISABLE))
    {
        return false;
    }
    return true;
}

//-------------------------------------------------------------------------------
UINT32 HopperGpuSubdevice::GetTexCountPerTpc() const
{
    // See decoder ring, or the number of pipes in
    // LW_PGRAPH_PRI_GPC0_TPC0_TEX_M_ROUTING_SEL
    return 2;
}

//------------------------------------------------------------------------------
UINT32 HopperGpuSubdevice::UserStrapVal()
{
    // These registers are removed starting on Maxwell, since the GpuSubdevice
    // implementation is Fermi based just return a default value
    return 0xFFFFFFFF;
}

//------------------------------------------------------------------------------
UINT64 HopperGpuSubdevice::FramebufferBarSize() const
{
    return FbApertureSize() / (1024*1024);
}

//------------------------------------------------------------------------------
RC HopperGpuSubdevice::SetFbMinimum(UINT32 fbMinMB)
{
    // 8bits == 1KB page size has been dropped by Maxwell, 9 bits is now the
    // minimumColumnSize. RowSize is unchanged.
    const UINT32 minFbColumnBits = 9; //2K page size minimum
    const UINT32 minFbRowBits = 10; //
    const UINT32 minBankBits = 3;
    return SetFbMinimumInternal(
        fbMinMB,
        minFbColumnBits,
        minFbRowBits,
        minBankBits);
}

//------------------------------------------------------------------------------
// Turn on/off the GPU power through the EC
// Note: This API is intended ONLY for Emulation machines that have a netlist
// which supports this backdoor register. Use at you own discretion.
// Note: This API is called by Xp::CallACPIGeneric() to put the GPU into/outof
// reset on the Emulator. Note that this register is not actually part of the
// memory mapped GPU registers in BAR0. There is code in the emulator that
// snoops register address's and routes these address to the PLDA.
RC HopperGpuSubdevice::SetBackdoorGpuPowerGc6(bool pwrOn)
{
    if (IsEmulation())
    {
        if (pwrOn)
        {
            RC rc;
            LwRmPtr pLwRm;
            LW2080_CTRL_POWER_FEATURE_GC6_INFO_PARAMS gc6Params;
            memset(&gc6Params, 0, sizeof(gc6Params));
            CHECK_RC(pLwRm->Control(pLwRm->GetSubdeviceHandle(this),
                        (UINT32)LW2080_CTRL_CMD_POWER_FEATURE_GET_GC6_INFO,
                        &gc6Params,
                        sizeof (gc6Params)));
            // If power islands are enabled we write to EC_2, otherwise write to
            // EC_1
            if (FLD_TEST_DRF(2080_CTRL_POWER, _FEATURE_GC6_INFO,
                             _PGISLAND_PRESENT, _TRUE,
                             gc6Params.gc6InfoMask))
            {
                //toggle bit 12
                Regs().Write32(MODS_BRIDGE_GPU_BACKDOOR_EC_2_WAKEUP_ASSERT_GPIO_12, 1);
                Utility::Delay(150); // make sure the pulse is at least 150us.
                Regs().Write32(MODS_BRIDGE_GPU_BACKDOOR_EC_2_WAKEUP_ASSERT_GPIO_12, 0);
            }
            else
            {
                Regs().Write32(MODS_BRIDGE_GPU_BACKDOOR_EC_1, 0);
            }
        }
        else // write to EC_1 regardless of power island status.
        {
            // Note: Writing this register has no effect when using Maxwell style GC6
            Regs().Write32(MODS_BRIDGE_GPU_BACKDOOR_EC_1, 1);
        }
    }
    return OK;
}
//------------------------------------------------------------------------------
 RC HopperGpuSubdevice::PrintMiscFuseRegs()
{
    return GetFsImpl()->PrintMiscFuseRegs();
}

//------------------------------------------------------------------------------
RC HopperGpuSubdevice::DumpEccAddress() const
{
    RC rc;
    LwRmPtr pLwRm;
    Tee::Priority pri = Tee::PriNormal;

    LW2080_CTRL_ECC_GET_AGGREGATE_ERROR_ADDRESSES_PARAMS eccParams = { 0 };
    CHECK_RC(pLwRm->ControlBySubdevice(this,
                                       LW2080_CTRL_CMD_ECC_GET_AGGREGATE_ERROR_ADDRESSES,
                                       &eccParams,
                                       sizeof(eccParams)));

    if (FLD_TEST_DRF(2080_CTRL_ECC, _GET_AGGREGATE_ERROR_ADDRESSES_FLAGS,
                     _OVERFLOW, _TRUE, eccParams.flags))
    {
        Printf(Tee::PriWarn, "Number of aggregate ECC addresses with error are more"
                " than maximum entries that can be returned\n");
    }

    map<Ecc::Unit, vector<LW2080_ECC_AGGREGATE_ADDRESS>> eccAddresses;

    for (UINT32 idx = 0; idx < eccParams.addressCount; idx++)
    {
        const Ecc::Unit lwrUnit = Ecc::ToEclwnitFromRmEclwnit(eccParams.entry[idx].unit);
        if (eccAddresses.find(lwrUnit) == eccAddresses.end())
            eccAddresses[lwrUnit] = { };
        eccAddresses[lwrUnit].push_back(eccParams.entry[idx]);
    }

    static const char * s_FbHeader  = "      Type  Part   Subp         Col        Row      Bank\n"
                                      "    ----------------------------------------------------";
    static const char * s_L2Header  = "      Type   LTC  Slice     Address\n"
                                      "    -------------------------------";
    static const char * s_GrHeader  = "      Type   GPC    TPC     Address\n"
                                      "    -------------------------------";
    static const char * s_GpcMmuHeader  = "      Type   GPC    MMU     Address\n"
                                          "    -------------------------------";
    static const char * s_HsHubHeader  =  "      Type HSHUB  Block     Address\n"
                                          "    -------------------------------";
    for (Ecc::Unit lwrUnit = Ecc::Unit::FIRST; lwrUnit < Ecc::Unit::MAX;
          lwrUnit = Ecc::GetNextUnit(lwrUnit))
    {
        if (!Ecc::IsAddressReportingSupported(lwrUnit))
            continue;

        Printf(pri, "\n  %s ECC addresses: \n", Ecc::ToString(lwrUnit));

        UINT32 totalCount = 0;
        if (eccAddresses.find(lwrUnit) != eccAddresses.end())
            totalCount = UNSIGNED_CAST(UINT32, eccAddresses[lwrUnit].size());
        Printf(pri, "    Total count: %u\n", totalCount);
        if (!totalCount)
            continue;

        const char * pHeader = nullptr;
        const Ecc::UnitType unitType = Ecc::GetUnitType(lwrUnit);
        if (lwrUnit == Ecc::Unit::DRAM)
        {
            pHeader = s_FbHeader;
        }
        else if (lwrUnit == Ecc::Unit::L2)
        {
            pHeader = s_L2Header;
        }
        else if (lwrUnit == Ecc::Unit::GPCMMU)
        {
            pHeader = s_GpcMmuHeader;
        }
        else if (lwrUnit == Ecc::Unit::HSHUB)
        {
            pHeader = s_HsHubHeader;
        }
        else if ((lwrUnit != Ecc::Unit::TEX) &&
                 (unitType == Ecc::UnitType::GPC))
        {
            pHeader = s_GrHeader;
        }
        MASSERT(pHeader);
        if (pHeader == nullptr)
        {
            Printf(Tee::PriError, "%s : Unknown ECC type\n", __FUNCTION__);
            return RC::SOFTWARE_ERROR;
        }
        Printf(pri, "%s\n", pHeader);
        for (auto const & lwrAddressData : eccAddresses[lwrUnit])
        {
            string type;
            if (lwrUnit == Ecc::Unit::DRAM)
            {
                FrameBuffer * pFb = GetFB();
                const UINT32 columnDivisor = pFb->GetBurstSize() / pFb->GetAmapColumnSize();
                const UINT32 columnMultiplier = pFb->GetPseudoChannelsPerChannel();
                if (FLD_TEST_DRF(2080_ECC, _AGGREGATE_ADDRESS_ERROR_TYPE,
                    _CORR, _TRUE, lwrAddressData.errorType))
                {
                    type = "SBE";
                }
                else if (FLD_TEST_DRF(2080_ECC, _AGGREGATE_ADDRESS_ERROR_TYPE,
                    _UNCORR, _TRUE, lwrAddressData.errorType))
                {
                    type = "DBE";
                }
                else
                {
                    type = "NONE";
                }
                Printf(pri, "    %6s  %4u  %5u  %#10x  %#9x  %#8x\n",
                       type.c_str(),
                       lwrAddressData.location.dram.partition,
                       lwrAddressData.location.dram.sublocation,
                       (lwrAddressData.address.rbc.column / columnDivisor) * columnMultiplier,
                       lwrAddressData.address.rbc.row,
                       lwrAddressData.address.rbc.bank);
            }
            else if ((unitType == Ecc::UnitType::GPC) ||
                     (lwrUnit == Ecc::Unit::L2) ||
                     (lwrUnit == Ecc::Unit::HSHUB))
            {
                if (FLD_TEST_DRF(2080_ECC, _AGGREGATE_ADDRESS_ERROR_TYPE,
                    _CORR, _TRUE, lwrAddressData.errorType))
                {
                    type = "CORR";
                }
                else if (FLD_TEST_DRF(2080_ECC, _AGGREGATE_ADDRESS_ERROR_TYPE,
                    _UNCORR, _TRUE, lwrAddressData.errorType))
                {
                    type = "UNCORR";
                }
                else
                {
                    type = "NONE";
                }
                const string sublocStr =
                    (lwrUnit == Ecc::Unit::HSHUB) ?
                        Ecc::HsHubSublocationString(lwrAddressData.location.hshub.sublocation) :
                        Utility::StrPrintf("%u", lwrAddressData.location.lrf.sublocation);
                Printf(pri, "    %6s  %4u  %5s  %#10x\n",
                       type.c_str(),
                       lwrAddressData.location.lrf.location,
                       sublocStr.c_str(),
                       lwrAddressData.address.rawAddress);
            }
        }
    }

    return OK;
}

//-------------------------------------------------------------------------------------------------
// To enable / disable ECC, MODS must send a message to the Falcon Secure Processor (FSP). Then
// after the next SwReset() the action will take place. Management of when to issue the
// SwReset is in Gpu::Initialize().
RC HopperGpuSubdevice::DisableEcc()
{
    RC rc;
    Fsp *pFsp = GetFspImpl();
    MASSERT(pFsp);
    vector<UINT08> payload(1, 0x0); // payload to disable ECC
    CHECK_RC(pFsp->SendFspMsg(Fsp::DataUseCase::Ecc,
                              Fsp::DataFlag::OneShot,
                              payload,
                              nullptr));
    return rc;
}

//-------------------------------------------------------------------------------------------------
RC HopperGpuSubdevice::EnableEcc()
{
    RC rc;
    Fsp *pFsp = GetFspImpl();
    MASSERT(pFsp);
    vector<UINT08> payload(1, 0x1); // payload to enable ECC
    CHECK_RC(pFsp->SendFspMsg(Fsp::DataUseCase::Ecc,
                              Fsp::DataFlag::OneShot,
                              payload,
                              nullptr));
    return rc;
}

//------------------------------------------------------------------------------
RC HopperGpuSubdevice::DisableDbi()
{
    RegHal& regs = TestDevice::Regs();
    regs.Write32(MODS_PFB_FBPA_FBIO_CONFIG_DBI, 0x0);
    if (FrameBuffer::IsHbm(this))
    {
        UINT32 mrs = regs.Read32(MODS_PFB_FBPA_GENERIC_MRS);
        regs.SetField(&mrs, MODS_PFB_FBPA_GENERIC_MRS_ADR_HBM_RDBI, 0);
        regs.SetField(&mrs, MODS_PFB_FBPA_GENERIC_MRS_ADR_HBM_WDBI, 0);
        regs.Write32(MODS_PFB_FBPA_GENERIC_MRS, mrs);
    }
    else
    {
        UINT32 mrs = regs.Read32(MODS_PFB_FBPA_GENERIC_MRS1);
        regs.SetField(&mrs, MODS_PFB_FBPA_GENERIC_MRS1_ADR_GDDR5_RDBI_DISABLED);
        regs.SetField(&mrs, MODS_PFB_FBPA_GENERIC_MRS1_ADR_GDDR5_WDBI_DISABLED);
        regs.Write32(MODS_PFB_FBPA_GENERIC_MRS1, mrs);
    }
    return RC::OK;
}

//------------------------------------------------------------------------------
RC HopperGpuSubdevice::EnableDbi()
{
    RegHal& regs = TestDevice::Regs();
    regs.Write32(MODS_PFB_FBPA_FBIO_CONFIG_DBI,
                 regs.SetField(MODS_PFB_FBPA_FBIO_CONFIG_DBI_IN_ON_ENABLED) |
                 regs.SetField(MODS_PFB_FBPA_FBIO_CONFIG_DBI_OUT_ON_ENABLED) |
                 regs.SetField(MODS_PFB_FBPA_FBIO_CONFIG_DBI_OUT_MODE_SSO));
    if (FrameBuffer::IsHbm(this))
    {
        UINT32 mrs = regs.Read32(MODS_PFB_FBPA_GENERIC_MRS);
        regs.SetField(&mrs, MODS_PFB_FBPA_GENERIC_MRS_ADR_HBM_RDBI, 1);
        regs.SetField(&mrs, MODS_PFB_FBPA_GENERIC_MRS_ADR_HBM_WDBI, 1);
        regs.Write32(MODS_PFB_FBPA_GENERIC_MRS, mrs);
    }
    else
    {
        UINT32 mrs = regs.Read32(MODS_PFB_FBPA_GENERIC_MRS1);
        regs.SetField(&mrs, MODS_PFB_FBPA_GENERIC_MRS1_ADR_GDDR5_RDBI_ENABLED);
        regs.SetField(&mrs, MODS_PFB_FBPA_GENERIC_MRS1_ADR_GDDR5_WDBI_ENABLED);
        regs.Write32(MODS_PFB_FBPA_GENERIC_MRS1, mrs);
    }
    return RC::OK;
}

//------------------------------------------------------------------------------
RC HopperGpuSubdevice::SkipInfoRomRowRemapping()
{
    RegHal& regs = Regs();

    UINT32 scratchVal = regs.Read32(MODS_PBUS_SW_SCRATCH, 1);
    scratchVal |= BIT(11); // Set LW_GFW_GLOBAL_SKIP_INFOROM_RRL_PROCESS_REQUESTED_TRUE
    regs.Write32(MODS_PBUS_SW_SCRATCH, 1, scratchVal);

    return RC::OK;

}

//------------------------------------------------------------------------------
// Replaces the legacy mechanism of setting bit 12 of LW_PBUS_SW_SCRATCH register.
RC HopperGpuSubdevice::DisableRowRemapper()
{
    RC rc;
    Fsp *pFsp = GetFspImpl();
    MASSERT(pFsp);
    CHECK_RC(pFsp->SendFspMsg(Fsp::DataUseCase::RowRemap,
                              Fsp::DataFlag::OneShot,
                              nullptr));

    return rc;
}

namespace
{
    struct IlwalidateCompbitCachePollArgs
    {
        GpuSubdevice *gpuSubdevice;
        UINT32 ltc;
        UINT32 slice;
    };

    static bool PollCompbitCacheSliceIlwalidated(void *pvArgs)
    {
        IlwalidateCompbitCachePollArgs *args =
            static_cast<IlwalidateCompbitCachePollArgs *>(pvArgs);

        const UINT32 addr = args->gpuSubdevice->Regs().LookupAddress(MODS_PLTCG_LTC0_LTS0_CBC_CTRL_1)
                         + (args->ltc * args->gpuSubdevice->Regs().LookupAddress(MODS_LTC_PRI_STRIDE))
                         + (args->slice * args->gpuSubdevice->Regs().LookupAddress(MODS_LTS_PRI_STRIDE));

        UINT32 value = args->gpuSubdevice->RegRd32(addr);
        return args->gpuSubdevice->Regs().TestField(value,
            MODS_PLTCG_LTC0_LTS0_CBC_CTRL_1_ILWALIDATE_INACTIVE);
    }
}

//-----------------------------------------------------------------------------
//! \brief Ilwalidate the compression bit cache
RC HopperGpuSubdevice::IlwalidateCompBitCache(FLOAT64 timeoutMs)
{
    RC rc;
    const UINT32 ltcNum    = GetFB()->GetLtcCount();

    MASSERT(ltcNum > 0);

    Regs().Write32(MODS_PLTCG_LTCS_LTSS_CBC_CTRL_1_ILWALIDATE_ACTIVE);

    // Wait until comptaglines for all LTCs and slices are set.
    for (UINT32 ltc = 0; ltc < ltcNum; ++ltc)
    {
        const UINT32 ltcSlices = GetFB()->GetSlicesPerLtc(ltc);
        MASSERT(ltcSlices > 0);

        for (UINT32 slice = 0; slice < ltcSlices; ++slice)
        {
            IlwalidateCompbitCachePollArgs args;
            args.gpuSubdevice = this;
            args.ltc = ltc;
            args.slice = slice;

            CHECK_RC(POLLWRAP_HW(&PollCompbitCacheSliceIlwalidated, &args, timeoutMs));
        }
    }

    return rc;
}

//-----------------------------------------------------------------------------
//! \brief Get the number of comptag lines per cache line.
UINT32 HopperGpuSubdevice::GetComptagLinesPerCacheLine()
{
    MASSERT(!"GetComptagLinesPerCacheLine not supported on Hopper");
    return 0;
}
//-----------------------------------------------------------------------------
//! \brief Get the compression bit cache line size.
UINT32 HopperGpuSubdevice::GetCompCacheLineSize()
{
    return 512 << Regs().Read32(MODS_PLTCG_LTCS_LTSS_CBC_PARAM_CACHE_LINE_SIZE);
}
//-----------------------------------------------------------------------------
//! \brief Get the compression bit cache base address
UINT32 HopperGpuSubdevice::GetCbcBaseAddress()
{
    return Regs().Read32(MODS_PLTCG_LTCS_LTSS_CBC_BASE_ADDRESS);
}

//------------------------------------------------------------------------------
bool HopperGpuSubdevice::ApplyFbCmdMapping
(
    const string &FbCmdMapping,
    UINT32 *pPfbCfgBcast,
    UINT32 *pPfbCfgBcastMask
) const
{
    if (FbCmdMapping == "GDDR5_170_X8")
        Regs().SetField(pPfbCfgBcast,
                MODS_PFB_FBPA_FBIO_BROADCAST_CMD_MAPPING_GDDR5_170_X8);
    else if (FbCmdMapping == "GDDR5_170_MIRR_X8")
        Regs().SetField(pPfbCfgBcast,
                MODS_PFB_FBPA_FBIO_BROADCAST_CMD_MAPPING_GDDR5_170_MIRR_X8);
    else if (FbCmdMapping == "SDDR4_BGA78")
        Regs().SetField(pPfbCfgBcast,
                MODS_PFB_FBPA_FBIO_BROADCAST_CMD_MAPPING_SDDR4_BGA78);
    else
        return GpuSubdevice::ApplyFbCmdMapping(
                FbCmdMapping, pPfbCfgBcast, pPfbCfgBcastMask);

    *pPfbCfgBcastMask |=
        Regs().LookupMask(MODS_PFB_FBPA_FBIO_BROADCAST_CMD_MAPPING);
    return true;
}

//------------------------------------------------------------------------------
RC HopperGpuSubdevice::PrepareEndOfSimCheckImpl
(
    Gpu::ShutDownMode mode
    ,bool bWaitgfxDeviceIdle
    ,bool bForceFenceSyncBeforeEos
)
{
    RC rc;

    CHECK_RC(GpuSubdevice::PrepareEndOfSimCheck(mode));

    Regs().Write32(MODS_PSMCARB_TIMESTAMP_CTRL,
                   Regs().SetField(MODS_PSMCARB_TIMESTAMP_CTRL_DISABLE_TICK_TRUE));

    CHECK_RC(PollRegValue(Regs().LookupAddress(MODS_PSMCARB_DEBUG),
                          0,
                          Regs().LookupMask(MODS_PSMCARB_DEBUG_TIMESTAMP_STATUS),
                          Tasker::GetDefaultTimeoutMs()));

    if (bWaitgfxDeviceIdle)
    {
        // Wait LW_PGRAPH_STATUS_STATE_IDLE
        Printf(Tee::PriNormal, "%s: Wait LW_PGRAPH_STATUS_STATE "
            "to be idle\n", __FUNCTION__);

        if (!IsSMCEnabled())
        {
            CHECK_RC(PollRegValue(Regs().LookupAddress(MODS_PGRAPH_STATUS),
                                  Regs().SetField(MODS_PGRAPH_STATUS_STATE_IDLE),
                                  Regs().LookupMask(MODS_PGRAPH_STATUS_STATE),
                                  Tasker::NO_TIMEOUT));
        }
        else
        {
            MASSERT(m_GetSmcRegHalsCb);
            vector<SmcRegHal *> smcRegHals;

            // Acquire the mutex for SmcRegHal to ensure they are not freed
            // before SmcRegHal usage is complete here
            Tasker::MutexHolder mh(m_GetSmcRegHalsCb(&smcRegHals));

            for (auto const & lwrRegHal : smcRegHals)
            {
                CHECK_RC(lwrRegHal->PollRegValue(MODS_PGRAPH_STATUS,
                                                 Regs().SetField(MODS_PGRAPH_STATUS_STATE_IDLE),
                                                 Regs().LookupMask(MODS_PGRAPH_STATUS_STATE),
                                                 Tasker::NO_TIMEOUT));
            }

        }
    }

    // always insert fence sync to ensure the poll is completed before EOS if chip is gm20x+.
    // Bug 200048362
    if ((Gpu::ShutDownMode::QuickAndDirty != mode) || (bForceFenceSyncBeforeEos))
    {
        CHECK_RC(InsertEndOfSimFence(Regs().LookupAddress(MODS_PPRIV_SYS_PRI_FENCE),
                                     Regs().LookupAddress(MODS_PPRIV_GPC_GPC0_PRI_FENCE),
                                     Regs().LookupAddress(MODS_GPC_PRIV_STRIDE),
                                     Regs().LookupAddress(MODS_PPRIV_FBP_FBP0_PRI_FENCE),
                                     Regs().LookupAddress(MODS_FBP_PRIV_STRIDE),
                                     bForceFenceSyncBeforeEos));
    }

    CHECK_RC(Tasker::PollHw(Tasker::GetDefaultTimeoutMs(),
                            [this]()->bool { return !HasPendingInterrupt(); },
                            __FUNCTION__));

    return rc;
}

//------------------------------------------------------------------------------
bool HopperGpuSubdevice::GetFbGddr5x8Mode()
{
    NamedProperty *pFbGddr5x8ModeProp;
    bool           FbGddr5x8Mode = false;
    if ((OK != GetNamedProperty("FbGddr5x8Mode", &pFbGddr5x8ModeProp)) ||
        (OK != pFbGddr5x8ModeProp->Get(&FbGddr5x8Mode)))
    {
        Printf(Tee::PriDebug, "FbGddr5x8Mode is not valid!!\n");
    }
    return FbGddr5x8Mode;
}

//------------------------------------------------------------------------------
/*virtual*/ UINT32 HopperGpuSubdevice::GetUserdAlignment() const
{
    return (1 << Regs().LookupAddress(MODS_RAMUSERD_BASE_SHIFT));
}

//------------------------------------------------------------------------------
// Iterate through the FBPs by HBM site and call the specified function which
// should expect parameters to be passed as (FBB, Site, SiteFbpIndex)
template<typename T>
void HopperGpuSubdevice::ForEachFbpBySite(T func)
{
    const UINT32 fbpMask = GetFsImpl()->FbpMask();
    const UINT32 siteCount = Regs().Read32(MODS_PTOP_SCAL_NUM_FBPS) / m_FbpsPerSite;
    for (UINT32 lwrSite = 0; lwrSite < siteCount; ++lwrSite)
    {
        const vector<UINT32> siteFbps = GetSiteFbps(lwrSite);
        MASSERT(m_FbpsPerSite == static_cast<UINT32>(siteFbps.size()));

        for (size_t ii = 0; ii < siteFbps.size(); ++ii)
        {
            if (fbpMask & (1 << siteFbps[ii]))
            {
                func(siteFbps[ii], lwrSite, static_cast<UINT32>(ii));
            }
        }
    }
}

//------------------------------------------------------------------------------
// The FBPs assigned to each site on GA100 are not contiguous as on previous
// chips. Return a vector of the FBPs in the site.  Note that order is important
// within this vector since the FBPAs within each FBP correspond to a particular
// site/channel mapping.  The FBPAs for the FBP in index 0 correspond to channels
// 0,1 within a site and the FBPAs for the FBP in index 1 corresponds to channels
// 2,3
vector<UINT32> HopperGpuSubdevice::GetSiteFbps(UINT32 site)
{
    switch (site)
    {
        case 0:
            return { 0, 2 };
        case 1:
            return { 4, 1 };
        case 2:
            return { 3, 5 };
        case 3:
            return { 7, 9 };
        case 4:
            return { 11, 6 };
        case 5:
            return { 8, 10 };
        default:
            MASSERT(!"Invalid site!");
            break;
    }
    return { Regs().Read32(MODS_PTOP_SCAL_NUM_FBPS), Regs().Read32(MODS_PTOP_SCAL_NUM_FBPS) };
}

//------------------------------------------------------------------------------
bool HopperGpuSubdevice::IsFbMc2MasterValid()
{
    // It is unset, so therefore valid
    if (m_FbMc2MasterMask == 0)
        return true;

    const RegHal &regs = Regs();
    const UINT32 fbpasPerFb   = (regs.Read32(MODS_PTOP_SCAL_NUM_FBPAS) /
                                 regs.Read32(MODS_PTOP_SCAL_NUM_FBPS));
    const UINT32 fbpasPerSite = m_FbpsPerSite * fbpasPerFb;
    const UINT32 siteCount    = regs.Read32(MODS_PTOP_SCAL_NUM_FBPS) / m_FbpsPerSite;

    UINT32 fbpMask = GetFsImpl()->FbpMask();
    bool bValidConfig = true;
    for (UINT32 lwrSite = 0; lwrSite < siteCount; ++lwrSite)
    {
        const vector<UINT32> siteFbps = GetSiteFbps(lwrSite);
        MASSERT(m_FbpsPerSite == static_cast<UINT32>(siteFbps.size()));

        UINT32 siteFbpMask = 0;

        // Create a virtual FBP bitmask for the site since MC2 master is specified
        // on the command line as a 4bit bitmask per site that indicates the FBPA
        // in the site that is the master.
        for (size_t ii = 0; ii < siteFbps.size(); ++ii)
        {
            siteFbpMask |= (fbpMask & (1 << siteFbps[ii])) ? (1 << ii) : 0;
        }

        // If any FBPs for the current site are enabled, then validate that the
        // MC2 master specified for the site is valid (exactly one FPBA specified
        // on a non floorswept FBP)
        if (siteFbpMask)
        {
            UINT32 siteMasterMask = (m_FbMc2MasterMask >>
                                     (lwrSite * fbpasPerSite)) &
                                    ((1 << fbpasPerSite) - 1);
            UINT32 siteMasterBits = Utility::CountBits(siteMasterMask);
            if (siteMasterBits == 0)
            {
                Printf(Tee::PriError,
                       "No MC2 master specified for site %u\n", lwrSite);
                bValidConfig = false;
            }
            else if (siteMasterBits > 1)
            {
                Printf(Tee::PriError,
                       "Multiple MC2 masters (0x%1x) specified for site %u\n",
                       siteMasterBits, lwrSite);
                bValidConfig = false;
            }

            // Ensure that the fbpa flagged as master is on a FB that is enabled
            INT32 fbMaster = Utility::BitScanForward(siteMasterMask) / fbpasPerFb;
            if (!(siteFbpMask & (1 << fbMaster)))
            {
                Printf(Tee::PriError,
                       "MC2 master is on floorswept FBP for site %u\n",
                       lwrSite);
                bValidConfig = false;
            }
        }
    }

    if (!bValidConfig)
    {
        Printf(Tee::PriError, "Invalid FB MC2 master configuration 0x%08x\n",
               m_FbMc2MasterMask);
        MASSERT(!"Generic assertion failure<refer to previous message>.");
    }
    return bValidConfig;
}

UINT32 HopperGpuSubdevice::GetFbMc2MasterValue(UINT32 site, UINT32 siteFbpIdx)
{
    const RegHal &regs = Regs();
    const UINT32 fbpasPerFb   = (regs.Read32(MODS_PTOP_SCAL_NUM_FBPAS) /
                                 regs.Read32(MODS_PTOP_SCAL_NUM_FBPS));
    const UINT32 fbpasPerSite = m_FbpsPerSite * fbpasPerFb;

    INT32 paMaster = Utility::BitScanForward((m_FbMc2MasterMask >>
                                              (fbpasPerSite * site)) &
                                             ((1 << fbpasPerSite) - 1));

    // Validity should already have been checked so there should be exactly
    // one bit set for the site
    MASSERT(paMaster != -1);

    static UINT32 noMasterOnFbpVal = ~0U;
    if (~0U == noMasterOnFbpVal)
    {
        // Clear all enable bits for the FBPA master in the mask.  Enable bits
        // for each PA are the odd bits in
        for (UINT32 i = 0; i < fbpasPerFb; i++)
            noMasterOnFbpVal &= ~(1 << (1 + (2 * i)));
    }
    UINT32 masterFbpVal = noMasterOnFbpVal;

    // The master bit for a PA in the FBP register
    masterFbpVal |= (1 << (1 + (2 * (paMaster % fbpasPerFb))));

    return ((paMaster / fbpasPerFb) == siteFbpIdx) ? masterFbpVal : noMasterOnFbpVal;
}

//------------------------------------------------------------------------------
//
RC HopperGpuSubdevice::GetVprSize
(
    UINT64 *pSize,
    UINT64 *pAlignment
)
{
    RC rc;
    PHYSADDR dummyAddr;

    rc = Platform::GetVPR(&dummyAddr, pSize);

    // If GetVPR is unsupported, that means the current platform
    // doesn't support MODS creating a VPR region.
    if (RC::UNSUPPORTED_FUNCTION == rc ||
        !Regs().IsSupported(MODS_MMU_PROTECTION_REGION_ADDR_ALIGN))
    {
        *pSize = 0;
        *pAlignment = 0;
        return OK;
    }

    *pAlignment = 1ULL << Regs().LookupValue(MODS_MMU_PROTECTION_REGION_ADDR_ALIGN);

    return rc;
}

//------------------------------------------------------------------------------
//
RC HopperGpuSubdevice::GetAcr1Size
(
    UINT64 *pSize,
    UINT64 *pAlignment
)
{
    RC rc;
    JavaScriptPtr pJs;

    // Return size=0 if ACR1 is not supported

    if (!Regs().IsSupported(MODS_PFB_PRI_MMU_WPR_ADDR_LO) ||
        !Regs().IsSupported(MODS_PFB_PRI_MMU_WPR_ADDR_HI) ||
        !Regs().IsSupported(MODS_MMU_WRITE_PROTECTION_REGION_ADDR_ALIGN))
    {
        *pSize = 0;
        *pAlignment = 0;
        return OK;
    }

    // Check to see if RM has already allocated an ACR1 region.

    UINT64 acr1Start = Regs().Read32(MODS_PFB_PRI_MMU_WPR_ADDR_LO, 1);
    acr1Start = acr1Start << Regs().LookupAddress(MODS_PFB_PRI_MMU_VPR_ADDR_LO_ALIGNMENT);

    UINT64 acr1End = Regs().Read32(MODS_PFB_PRI_MMU_WPR_ADDR_HI, 1);
    acr1End = acr1End << Regs().LookupAddress(MODS_PFB_PRI_MMU_VPR_ADDR_HI_ALIGNMENT);

    Printf(Tee::PriDebug, "ACR: acr1Start = 0x%llx  acr1End = 0x%llx\n",
           acr1Start, acr1End);

    // If the ACR2 region is valid, that means RM has already allocated it.
    if (acr1Start <= acr1End)
    {
        *pSize = 0;
    }

    // Otherwise, MODS needs to allocate the ACR2 region (assuming the user
    // has specified one).
    else
    {
        CHECK_RC(pJs->GetProperty(Gpu_Object, Gpu_Acr1Size, pSize));
    }

    *pAlignment = 1ULL << Regs().LookupValue(MODS_MMU_WRITE_PROTECTION_REGION_ADDR_ALIGN);

    return rc;
}

//------------------------------------------------------------------------------
//
RC HopperGpuSubdevice::GetAcr2Size
(
    UINT64 *pSize,
    UINT64 *pAlignment
)
{
    RC rc;
    JavaScriptPtr pJs;

    // Return size=0 if ACR1 is not supported

    if (!Regs().IsSupported(MODS_PFB_PRI_MMU_WPR_ADDR_LO) ||
        !Regs().IsSupported(MODS_PFB_PRI_MMU_WPR_ADDR_HI) ||
        !Regs().IsSupported(MODS_MMU_WRITE_PROTECTION_REGION_ADDR_ALIGN))
    {
        *pSize = 0;
        *pAlignment = 0;
        return OK;
    }

    // Check to see if RM has already allocated an ACR2 region.

    UINT64 acr2Start = Regs().Read32(MODS_PFB_PRI_MMU_WPR_ADDR_LO, 2);
    acr2Start = acr2Start << Regs().LookupAddress(MODS_PFB_PRI_MMU_VPR_ADDR_LO_ALIGNMENT);

    UINT64 acr2End = Regs().Read32(MODS_PFB_PRI_MMU_WPR_ADDR_HI, 2);
    acr2End = acr2End << Regs().LookupAddress(MODS_PFB_PRI_MMU_VPR_ADDR_HI_ALIGNMENT);

    Printf(Tee::PriDebug, "ACR: acr2Start = 0x%llx  acr2End = 0x%llx\n",
           acr2Start, acr2End);

    // If the ACR1 region is valid, that means RM has already allocated it.
    if (acr2Start <= acr2End)
    {
        *pSize = 0;
    }

    // Otherwise, MODS needs to allocate the ACR1 region (assuming the user
    // has specified one).
    else
    {
        CHECK_RC(pJs->GetProperty(Gpu_Object, Gpu_Acr2Size, pSize));
    }

    *pAlignment = 1ULL << Regs().LookupValue(MODS_MMU_WRITE_PROTECTION_REGION_ADDR_ALIGN);

    return rc;
}

//------------------------------------------------------------------------------
//
RC HopperGpuSubdevice::SetVprRegisters
(
    UINT64 size,
    UINT64 physAddr
)
{
    RC rc;
    UINT32 regValue;
    UINT32 regAddrShift = Regs().LookupAddress(MODS_PFB_PRI_MMU_VPR_ADDR_LO_ALIGNMENT);

    regValue = LwU64_LO32(physAddr >> regAddrShift);
    Printf(Tee::PriDebug,
        "VPR: Regs().Write32(MODS_PFB_PRI_MMU_VPR_ADDR_LO_VAL, 0x%08x)\n", regValue);
    Regs().Write32(MODS_PFB_PRI_MMU_VPR_ADDR_LO_VAL, regValue);

    regAddrShift = Regs().LookupAddress(MODS_PFB_PRI_MMU_VPR_ADDR_HI_ALIGNMENT);
    regValue = LwU64_LO32((physAddr + size - 1) >> regAddrShift);
    Printf(Tee::PriDebug,
        "VPR: Regs().Write32(MODS_PFB_PRI_MMU_VPR_ADDR_HI_VAL, 0x%08x)\n", regValue);
    Regs().Write32(MODS_PFB_PRI_MMU_VPR_ADDR_HI_VAL, regValue);

    JavaScriptPtr pJs;
    UINT32 vprCya0;
    UINT32 vprCya1;
    RC rc1 = pJs->GetProperty(Gpu_Object, Gpu_VprCya0, &vprCya0);
    RC rc2 = pJs->GetProperty(Gpu_Object, Gpu_VprCya1, &vprCya1);

    if ((OK == rc1) && (OK == rc2))
    {
        Printf(Tee::PriDebug, "VPR:  vprCya0 = 0x%08x  vprCya1 = 0x%08x\n", vprCya0, vprCya1);

        UINT64 cyaFull = vprCya0 | ((UINT64) vprCya1 << 32);

        //
        // CYA values go into the _DATA field of VPR_INFO using INDEX values of
        // CYA_LO and CYA_HI.  However, in CYA_LO the first bit of _DATA is
        // oclwpied by the IN_USE field.  So we take the full value, shift by 1
        // to make room for IN_USE and then split between the _DATA fields of
        // CYA_LO and CYA_HI.
        //
        cyaFull <<= 1;

        const UINT32 mask = Regs().LookupFieldValueMask(MODS_PFB_PRI_MMU_VPR_WPR_VAL);
        const UINT32 maskSize = Regs().LookupMaskSize(MODS_PFB_PRI_MMU_VPR_WPR_VAL);
        const UINT32 cyaLo = LwU64_LO32(mask & cyaFull);
        const UINT32 cyaHi = LwU64_LO32(mask & (cyaFull >> maskSize));

        Printf(Tee::PriDebug, "VPR:  cyaLo = 0x%08x  cyaHi = 0x%08x\n", cyaLo, cyaHi);

        regValue = 0;
        Regs().SetField(&regValue, MODS_PFB_PRI_MMU_VPR_WPR_VAL, cyaLo);
        Printf(Tee::PriDebug, "VPR: Regs().Write32(MODS_PFB_PRI_MMU_VPR_CYA_LO, 0x%08x)\n",
            regValue);
        Regs().Write32(MODS_PFB_PRI_MMU_VPR_CYA_LO, regValue);

        regValue = 0;
        Regs().SetField(&regValue, MODS_PFB_PRI_MMU_VPR_WPR_VAL, cyaHi);
        Printf(Tee::PriDebug, "VPR: Regs().Write32(MODS_PFB_PRI_MMU_VPR_CYA_HI, 0x%08x)\n",
            regValue);
        Regs().Write32(MODS_PFB_PRI_MMU_VPR_CYA_HI, regValue);

        regValue = Regs().Read32(MODS_PFB_PRI_MMU_VPR_MODE);
        Regs().SetField(&regValue, MODS_PFB_PRI_MMU_VPR_MODE_IN_USE_TRUE);
        Printf(Tee::PriDebug, "VPR: Regs().Write32(MODS_PFB_PRI_MMU_VPR_MODE_IN_USE_TRUE)\n");
        Regs().Write32(MODS_PFB_PRI_MMU_VPR_MODE, regValue);
    }

    do
    {
        regValue = Regs().Read32(MODS_PFB_PRI_MMU_VPR_MODE);

    } while (Regs().Test32(MODS_PFB_PRI_MMU_VPR_MODE_FETCH_TRUE));

    regValue = Regs().SetField(MODS_PFB_PRI_MMU_VPR_MODE_FETCH_TRUE);
    Regs().Write32(MODS_PFB_PRI_MMU_VPR_MODE, regValue);

    do
    {
        regValue = Regs().Read32(MODS_PFB_PRI_MMU_VPR_MODE);

    } while (Regs().Test32(MODS_PFB_PRI_MMU_VPR_MODE_FETCH_TRUE));

    return OK;
}

//------------------------------------------------------------------------------
//
RC HopperGpuSubdevice::SetAcrRegisters
(
    UINT64 acr1Size,
    UINT64 acr1PhysAddr,
    UINT64 acr2Size,
    UINT64 acr2PhysAddr
)
{
    RC rc;

    UINT32 regValue;
    UINT32 regAddrShift;

    // Both VPR and ACR region addresses are shifted the same amount
    // before being written to registers, but the ACR region must be
    // 128K aligned.
    const UINT64 acrAddrAlign = 0x20000ULL;

    if (acr1Size)
    {
        regAddrShift = Regs().LookupAddress(MODS_PFB_PRI_MMU_WPR1_ADDR_LO_ALIGNMENT);
        regValue = LwU64_LO32(acr1PhysAddr >> regAddrShift);
        Printf(Tee::PriDebug,
               "ACR: Regs().Write32(MODS_PFB_PRI_MMU_WPR1_ADDR_LO_VAL, 0x%08x)\n",
               regValue);
        Regs().Write32(MODS_PFB_PRI_MMU_WPR1_ADDR_LO_VAL, regValue);

        regAddrShift = Regs().LookupAddress(MODS_PFB_PRI_MMU_WPR1_ADDR_HI_ALIGNMENT);
        const UINT64 acr1End = ALIGN_DOWN((acr1PhysAddr + acr1Size - 1), acrAddrAlign);
        regValue = LwU64_LO32(acr1End >> regAddrShift);
        Printf(Tee::PriDebug,
               "ACR: Regs().Write32(MODS_PFB_PRI_MMU_WPR1_ADDR_HI_VAL, 0x%08x)\n",
               regValue);
        Regs().Write32(MODS_PFB_PRI_MMU_WPR1_ADDR_HI_VAL, regValue);
    }

    if (acr2Size)
    {
        regAddrShift = Regs().LookupAddress(MODS_PFB_PRI_MMU_WPR2_ADDR_LO_ALIGNMENT);
        regValue = LwU64_LO32(acr2PhysAddr >> regAddrShift);
        Printf(Tee::PriDebug,
            "ACR: Regs().Write32(MODS_PFB_PRI_MMU_WPR2_ADDR_LO_VAL, 0x%08x)\n",
            regValue);
        Regs().Write32(MODS_PFB_PRI_MMU_WPR2_ADDR_LO_VAL, regValue);

        regAddrShift = Regs().LookupAddress(MODS_PFB_PRI_MMU_WPR2_ADDR_HI_ALIGNMENT);
        const UINT64 acr2End = ALIGN_DOWN((acr2PhysAddr + acr2Size - 1), acrAddrAlign);
        regValue = LwU64_LO32(acr2End >> regAddrShift);
        Printf(Tee::PriDebug,
            "ACR: Regs().Write32(MODS_PFB_PRI_MMU_WPR2_ADDR_HI_VAL, 0x%08x)\n",
            regValue);
        Regs().Write32(MODS_PFB_PRI_MMU_WPR2_ADDR_HI_VAL, regValue);
    }

    UINT32 acr1ReadMask;
    UINT32 acr1WriteMask;
    UINT32 acr2ReadMask;
    UINT32 acr2WriteMask;
    JavaScriptPtr pJs;
    CHECK_RC(pJs->GetProperty(Gpu_Object, Gpu_Acr1ReadMask, &acr1ReadMask));
    CHECK_RC(pJs->GetProperty(Gpu_Object, Gpu_Acr1WriteMask, &acr1WriteMask));
    CHECK_RC(pJs->GetProperty(Gpu_Object, Gpu_Acr2ReadMask, &acr2ReadMask));
    CHECK_RC(pJs->GetProperty(Gpu_Object, Gpu_Acr2WriteMask, &acr2WriteMask));

    // RM always sets the level 3 access bit, so do the same here.
    regValue = 0;
    Regs().SetField(&regValue, MODS_PFB_PRI_MMU_WPR_ALLOW_READ_WPR1, acr1ReadMask);
    Regs().SetField(&regValue, MODS_PFB_PRI_MMU_WPR_ALLOW_READ_WPR1_SELWRE3_YES);
    Regs().SetField(&regValue, MODS_PFB_PRI_MMU_WPR_ALLOW_READ_WPR2, acr2ReadMask);
    Regs().SetField(&regValue, MODS_PFB_PRI_MMU_WPR_ALLOW_READ_WPR2_SELWRE3_YES);
    Printf(Tee::PriDebug,
        "ACR: Regs().Write32(MODS_PFB_PRI_MMU_WPR_ALLOW_READ, 0x%08x)\n",
        regValue);
    Regs().Write32(MODS_PFB_PRI_MMU_WPR_ALLOW_READ, regValue);

    regValue = 0;
    Regs().SetField(&regValue, MODS_PFB_PRI_MMU_WPR_ALLOW_WRITE_WPR1, acr1WriteMask);
    Regs().SetField(&regValue, MODS_PFB_PRI_MMU_WPR_ALLOW_WRITE_WPR1_SELWRE3_YES);
    Regs().SetField(&regValue, MODS_PFB_PRI_MMU_WPR_ALLOW_WRITE_WPR2, acr2WriteMask);
    Regs().SetField(&regValue, MODS_PFB_PRI_MMU_WPR_ALLOW_WRITE_WPR2_SELWRE3_YES);
    Printf(Tee::PriDebug,
        "ACR: Regs().Write32(MODS_PFB_PRI_MMU_WPR_ALLOW_WRITE, 0x%08x)\n",
        regValue);
    Regs().Write32(MODS_PFB_PRI_MMU_WPR_ALLOW_WRITE, regValue);

    return rc;
}

//-----------------------------------------------------------------------------
UINT32 HopperGpuSubdevice::GetKindTraits(INT32 pteKind)
{
    INT32 kind = 0;

    if (static_cast<UINT32>(pteKind) == Regs().LookupValue(MODS_MMU_PTE_KIND_GENERIC_MEMORY) ||
        PTEKIND_TO_KIND_FOR_CLIENT(pteKind) == Regs().LookupValue(MODS_MMU_PTE_KIND_GENERIC_MEMORY))
    {
        kind |= Generic_Memory;
    }

    if (PTEKIND_COMPRESSIBLE(pteKind))
    {
        kind |= Compressible;
    }

    if (KIND_C(pteKind))
    {
        kind |= Color;
    }

    if (PTEKIND_Z(pteKind))
    {
        kind |= Z;
    }

    if (KIND_Z24S8(pteKind))
    {
        kind |= Z24S8;
    }

    if (PTEKIND_PITCH(pteKind))
    {
        kind |= Pitch;
    }

    return kind;
}

//-----------------------------------------------------------------------------
UINT32 HopperGpuSubdevice::GetMmeStartAddrRamSize()
{
    // Ref: https://p4viewer.lwpu.com/getfile///hw/doc/gpu/turing/turing/design/Functional_Descriptions/Turing_MME64_FD.docx //$
    // Section 4.5 LoadMmeStartAddressRamPointer
    // NOTE: Start Address RAM and CallMmeMacro/Data size are the same value.

    return 224; // 224 32-bit entries
}

//-----------------------------------------------------------------------------
UINT32 HopperGpuSubdevice::GetMmeDataRamSize()
{
    // Ref: https://p4viewer.lwpu.com/getfile///hw/doc/gpu/turing/turing/design/Functional_Descriptions/Turing_MME64_FD.docx //$
    // Section 5.1.5 Memory read / write

    return static_cast<UINT32>(16_KB) / 4; // 16KB divided into 32-bit chunks
}

//-----------------------------------------------------------------------------
UINT32 HopperGpuSubdevice::GetIntrStatus(unsigned intrTree) const
{
    MASSERT(intrTree < GetNumIntrTrees());
    const RegHal& regs = GpuSubdevice::Regs();
    return regs.Read32(MODS_VIRTUAL_FUNCTION_PRIV_CPU_INTR_TOP, intrTree);
}

bool HopperGpuSubdevice::GetSWIntrStatus(unsigned intrTree) const
{
    // This MASSERT is just to ensure that the interrupts are validated only for intrTree as 0.
    // If in future, we start validating other intrTree's then this function needs to be updated.
    MASSERT(intrTree == 0);
    const RegHal& regs = GpuSubdevice::Regs();
    const UINT32 doorbellVector =
        regs.LookupValue(MODS_CTRL_CPU_DOORBELL_VECTORID_VALUE_CONSTANT);

    // The leaf register is obtained by "GPU Interrupt Vector/32" and the corresponding
    // leaf bit in the leaf register is obtained by "GPU Interrupt Vector%32"
    // if the leaf bit in the leaf register is 1 (implies it is pending)
    // CPU_DOORBELL is the interrupt source/wire for SOFTWARE interrupts
    // Info from: //hw/doc/gpu/turing/turing/design/IAS/Host/turing_interrupt_map.xlsx
    UINT32 softwarePendingReg = regs.Read32(MODS_VIRTUAL_FUNCTION_PRIV_CPU_INTR_LEAF,
                     doorbellVector / 32);
    return (softwarePendingReg & (1 << (doorbellVector % 32))) != 0;
}

//------------------------------------------------------------------------------
// Assert/DeAssert PEXRST# on PLDA EMU
// Note: This API is intended ONLY for Emulation machines that have a netlist
// which supports this backdoor register. Use at you own discretion.
// Note: This API is called by RTD3 RmTest to assert/deassert pexrst# on the Emulator.
// Note that this register is not actually part of the memory mapped GPU registers
// in BAR0.
// There is code in the emulator that snoops register address's and
// routes these address to the PLDA.
// TODO SC need to use reghal style for register accessing instead of hardcoding.
// Lwrrently there is no full bit definition in HW manual and the RmTest is needed
// for LwLink. Tracking bug#200387913
RC HopperGpuSubdevice::SetBackdoorGpuAssertPexRst(bool isAsserted)
{
    LwU32 regVal = 0;

    if (!IsEmulation())
    {
        return RC::UNSUPPORTED_DEVICE;
    }

    if (isAsserted)
    {

        Printf(Tee::PriNormal, "EMU Asserting PCH PEXRST# after predefined delay\n");

        regVal = Regs().SetField(MODS_BRIDGE_GPU_BACKDOOR_EC_3_PCH_STUB_ENABLE);
        Regs().Write32(MODS_BRIDGE_GPU_BACKDOOR_EC_3, regVal); // Enable PCH Stub

        Regs().SetField(&regVal, MODS_BRIDGE_GPU_BACKDOOR_EC_3_PEXRST_OVERRIDE_ENABLE);
        Regs().SetField(&regVal, MODS_BRIDGE_GPU_BACKDOOR_EC_3_PEXRST_ASSERT);
        Regs().Write32(MODS_BRIDGE_GPU_BACKDOOR_EC_3, regVal); // Enable PCH Stub
    }
    else
    {
        Regs().SetField(&regVal, MODS_BRIDGE_GPU_BACKDOOR_EC_3_PEXRST_OVERRIDE_ENABLE);
        Regs().SetField(&regVal, MODS_BRIDGE_GPU_BACKDOOR_EC_3_PEXRST_DEASSERT);
        Printf(Tee::PriNormal, "EMU deAsserting PCH PEXRST# after predefined delay\n");
        Regs().Write32(MODS_BRIDGE_GPU_BACKDOOR_EC_3, regVal); // Enabled PEX_RST# override and adessert
    }

    return OK;
}

//------------------------------------------------------------------------------
// Put PLDA downstream port to L2 on PLDA EMU
// Note: This API is intended ONLY for Emulation machines that have a netlist
// which supports this backdoor register. Use at you own discretion.
// Note: This API is called by RTD3 RmTest to initiate PMU message handshaek
// on the Emulator and link will be put into L2 after that.
// Note that this register is not actually part of the memory mapped GPU registers
// in BAR0.
// There is code in the emulator that snoops register address's and
// routes these address to the PLDA.
// TODO SC need to use reghal style for register accessing instead of hardcoding.
// Lwrrently there is no full bit definition in HW manual and the RmTest is needed
// for LwLink. Tracking bug#200387913
RC HopperGpuSubdevice::SetBackdoorGpuLinkL2(bool isL2)
{
    RC rc;
    LwRmPtr pLwRm;
    UINT32 value = 0;

    // Lwrrently supoort L2 and L0 only

    if (!IsEmulation())
    {
        Printf(Tee::PriError, "Only support link status in EMU\n");
        return RC::UNSUPPORTED_FUNCTION;
    }

    value = Regs().Read32(MODS_BRIDGE_REG_PM_CONTROL);

    Printf(Tee::PriNormal, "REG_PM_CONTROL in entry is 0x%x\n", value);
    if (isL2)
    {
        //directly set because manual was not giving full bit difinition
        Regs().SetField(&value, MODS_BRIDGE_REG_PM_CONTROL_DSP_FORCE_L2_ENABLE);
    }
    else
    {
        //clear bit 0
        Regs().SetField(&value, MODS_BRIDGE_REG_PM_CONTROL_DSP_FORCE_L2_DISABLE);
    }

    Printf(Tee::PriNormal, "Setting REG_PM_CONTROL to 0x%x\n", value);
    Regs().Write32(MODS_BRIDGE_REG_PM_CONTROL, value); // Write back door PM control register

    if (isL2)
    {
        while (LW_TRUE)
        {
            value = Regs().Read32(MODS_BRIDGE_GPU_BACKDOOR_LTSSM_STATUS0); // Read backdoor LTSSM register
            // polling backdoor ltssm status to confirm
            if (Regs().TestField(value, MODS_BRIDGE_GPU_BACKDOOR_LTSSM_STATUS0_SWITCH_DSP_L2))
            {
                break;
            }
            Printf(Tee::PriNormal, "LW_BRIDGE_GPU_BACKDOOR_LTSSM_STATUS0 in entry is 0x%x\n", value);
            Tasker::Sleep(1000);
        }
    }
    else // Link state L0
    {
        Regs().Write32(MODS_BRIDGE_REG_PM_CONTROL, 0); // Clear backdoor PM control register

        Utility::Delay(30000);  // delay for link to be up, or EMU reboots

        while (LW_TRUE)
        {
            Printf(Tee::PriNormal, "Before reading LSTTM_STATUS\n");
            value = Regs().Read32(MODS_BRIDGE_GPU_BACKDOOR_LTSSM_STATUS0); // Read backdoor LTSSM register
            Printf(Tee::PriNormal, "END reading LSTTM_STATUS\n");
            Printf(Tee::PriNormal, "LW_BRIDGE_GPU_BACKDOOR_LTSSM_STATUS0 in RTD3 exit is 0x%x\n", value);

            if (Regs().TestField(value, MODS_BRIDGE_GPU_BACKDOOR_LTSSM_STATUS0_SWITCH_DSP_L0))
            {
                break;
            }
        }
    }

    return rc;
}

//-----------------------------------------------------------------------------
RC HopperGpuSubdevice::GetKappaInfo(UINT32* pKappa, UINT32* pVersion, INT32* pValid)
{
    MASSERT(pKappa);
    MASSERT(pVersion);
    MASSERT(pValid);
    RC rc;

    if (Regs().Test32(MODS_FUSE_KAPPAINFO_PRIV_LEVEL_MASK_READ_PROTECTION_LEVEL0_DISABLE))
    {
        Printf(Tee::PriDebug, "Speedo info is priv-protected. Use HULK to view.\n");
        return RC::UNSUPPORTED_FUNCTION;
    }

    // TODO: They haven't yet decided which part of the data is the version
    *pVersion = 0;
    *pKappa = Regs().Read32(MODS_FUSE_OPT_KAPPA_INFO_DATA);
    *pValid = -1;

    return rc;
}

/* virtual */ unique_ptr<PciCfgSpace> HopperGpuSubdevice::AllocPciCfgSpace() const
{
    return make_unique<TuringPciCfgSpace>(this);
}

UINT32 HopperGpuSubdevice::GetDomainBase
(
    UINT32 domainIdx,
    RegHalDomain domain,
    LwRm *pLwRm
)
{
    auto GetPerLwLinkBase = [&] (ModsGpuRegAddress a) -> UINT32
    {
        // This should only be the case if LWLINK is not supported on the GPU in which
        // case we shouldnt be trying to access LWLINK registers
        MASSERT(m_LwLinksPerIoctl > 0);
        return Regs().LookupAddress(a,
                                    {
                                        domainIdx / m_LwLinksPerIoctl,
                                        domainIdx % m_LwLinksPerIoctl
                                    });
    };

    switch (domain)
    {
        case RegHalDomain::RUNLIST:
        {
            MASSERT(domainIdx < LW2080_ENGINE_TYPE_LAST);

            LW2080_CTRL_GPU_GET_ENGINE_RUNLIST_PRI_BASE_PARAMS baseParams = { };
            baseParams.engineList[0] = domainIdx;
            const RC rc = pLwRm->ControlBySubdevice(this,
                     LW2080_CTRL_CMD_GPU_GET_ENGINE_RUNLIST_PRI_BASE,
                     &baseParams, sizeof(baseParams));
            const LwU32 priBase = baseParams.runlistPriBase[0];
            if ((rc != OK) ||
                ((priBase == LW2080_CTRL_GPU_GET_ENGINE_RUNLIST_PRI_BASE_NULL) ||
                 (priBase == LW2080_CTRL_GPU_GET_ENGINE_RUNLIST_PRI_BASE_ERROR)))
            {
                MASSERT(!"Failed to retrieve runlist pri base or pri base is invalid!");
                return 0;
            }
            return baseParams.runlistPriBase[0];
        }
        case RegHalDomain::CHRAM:
        {
            LwU32 config = 0;
            SmcRegHal regs(this, pLwRm, 0, 0);
            regs.Initialize();
            LwU32 offset = regs.Read32(domainIdx, MODS_RUNLIST_CHANNEL_CONFIG_CHRAM_BAR0_OFFSET);
            regs.SetField(&config, MODS_RUNLIST_CHANNEL_CONFIG_CHRAM_BAR0_OFFSET, offset);
            return config;
        }
        case RegHalDomain::LWLDL:
            return GetPerLwLinkBase(MODS_DISCOVERY_IOCTRL_UNICAST_x_SW_DEVICE_BASE_LWLDL_y);

        case RegHalDomain::LWLTLC:
            return GetPerLwLinkBase(MODS_DISCOVERY_IOCTRL_UNICAST_x_SW_DEVICE_BASE_LWLTLC_y);

        case RegHalDomain::LWLCPR:
            return Regs().LookupAddress(MODS_DISCOVERY_IOCTRL_UNICAST_x_SW_DEVICE_BASE_CPR,
                                        domainIdx);
        case RegHalDomain::LWLPLL:
            return Regs().LookupAddress(MODS_DISCOVERY_IOCTRL_UNICAST_x_SW_DEVICE_BASE_PLL,
                                        domainIdx);
        case RegHalDomain::LWLW:
            return Regs().LookupAddress(MODS_DISCOVERY_IOCTRL_UNICAST_x_SW_DEVICE_BASE_IOCTRL,
                                        domainIdx);
        case RegHalDomain::MINION:
            return Regs().LookupAddress(MODS_DISCOVERY_IOCTRL_UNICAST_x_SW_DEVICE_BASE_MINION,
                                        domainIdx);
        case RegHalDomain::LWLIPT:
            return Regs().LookupAddress(MODS_DISCOVERY_IOCTRL_UNICAST_x_SW_DEVICE_BASE_LWLIPT,
                                        domainIdx);
        case RegHalDomain::LWLIPT_LNK:
            return GetPerLwLinkBase(MODS_DISCOVERY_IOCTRL_UNICAST_x_SW_DEVICE_BASE_LWLIPT_LNK_y);

        case RegHalDomain::LWLIPT_LNK_MULTI:
            return Regs().LookupAddress(MODS_DISCOVERY_IOCTRL_UNICAST_x_SW_DEVICE_BASE_LWLIPT_LNK_MULTI, domainIdx);

        case RegHalDomain::LWLTLC_MULTI:
            return Regs().LookupAddress(MODS_DISCOVERY_IOCTRL_UNICAST_x_SW_DEVICE_BASE_LWLTLC_MULTI, domainIdx);

        case RegHalDomain::LWLDL_MULTI:
            return Regs().LookupAddress(MODS_DISCOVERY_IOCTRL_UNICAST_x_SW_DEVICE_BASE_LWLDL_MULTI, domainIdx);
        case RegHalDomain::XTL:
            return DEVICE_BASE(LW_XTL);
        case RegHalDomain::XPL:
            return LW_XPL_BASE_ADDRESS;

        default:
                break;
    }
    return GpuSubdevice::GetDomainBase(domainIdx, domain, pLwRm);
}

RC HopperGpuSubdevice::SetupUserdWriteback(RegHal &regs)
{
    if (Platform::GetSimulationMode() != Platform::Hardware &&
        regs.IsSupported(MODS_RUNLIST_USERD_WRITEBACK))
    {
        RC rc;
        LW2080_CTRL_GPU_GET_ENGINES_V2_PARAMS engParams = { };
        CHECK_RC(regs.GetLwRmPtr()->ControlBySubdevice(this,
                                                       LW2080_CTRL_CMD_GPU_GET_ENGINES_V2,
                                                       &engParams, sizeof(engParams)));

        LW2080_CTRL_GPU_GET_ENGINE_RUNLIST_PRI_BASE_PARAMS baseParams = { };
        memcpy(baseParams.engineList, engParams.engineList, sizeof(LwU32) * engParams.engineCount);
        GetRunlistPriBases(regs.GetLwRmPtr(),
                           engParams.engineCount,
                           &baseParams);

        const UINT32 regOff = regs.LookupAddress(MODS_RUNLIST_USERD_WRITEBACK);
        for (UINT32 lwrEng = 0; lwrEng < engParams.engineCount; lwrEng++)
        {
            const LwU32 priBase = baseParams.runlistPriBase[lwrEng];
            if ((priBase == LW2080_CTRL_GPU_GET_ENGINE_RUNLIST_PRI_BASE_NULL) ||
                (priBase == LW2080_CTRL_GPU_GET_ENGINE_RUNLIST_PRI_BASE_ERROR))
            {
                continue;
            }

            regs.Write32(engParams.engineList[lwrEng],
                           MODS_RUNLIST_USERD_WRITEBACK_TIMER_SHORT);
            regs.Write32(engParams.engineList[lwrEng],
                           MODS_RUNLIST_USERD_WRITEBACK_TIMESCALE_0);

            // Insert the write prevention at the start of the masking array so that
            // it will be guaranteed that the register isnt rewritten
            RegMaskRange maskReg(GetGpuInst(), regOff + priBase, regOff + priBase, 0);
            g_RegWrMask.insert(g_RegWrMask.begin(), maskReg);
        }
    }
    return OK;
}

RC HopperGpuSubdevice::GetRunlistPriBases
(
    LwRm *pLwRm,
    LwU32 engineCount,
    LW2080_CTRL_GPU_GET_ENGINE_RUNLIST_PRI_BASE_PARAMS *pBaseParams
) const
{
    // RM policy for hw engine and runlist base queries is that if an invalid engine is
    // specified it will be indicated by an error entry in the resulting list, so some data
    // will be correct and the invalid engines will have an ERROR token at their resulting
    // entry.  Allow this function to automatically skip any invalid entries, but if not
    // a single entry was valid then consider it a catastrophic failure and fail this function

    // Fill the runlistPriBase with NULL so that catastrophic failure can be detected
    fill(pBaseParams->runlistPriBase,
         pBaseParams->runlistPriBase + engineCount,
         LW2080_CTRL_GPU_GET_ENGINE_RUNLIST_PRI_BASE_NULL);

    const RC rc = pLwRm->ControlBySubdevice(this,
                                            LW2080_CTRL_CMD_GPU_GET_ENGINE_RUNLIST_PRI_BASE,
                                            pBaseParams,
                                            sizeof(*pBaseParams));
    if (OK != rc)
    {
        bool bAnyEngineValid = false;
        for (UINT32 lwrEng = 0; (lwrEng < engineCount) && !bAnyEngineValid; lwrEng++)
        {
            const LwU32 priBase = pBaseParams->runlistPriBase[lwrEng];
            if ((priBase != LW2080_CTRL_GPU_GET_ENGINE_RUNLIST_PRI_BASE_NULL) &&
                (priBase != LW2080_CTRL_GPU_GET_ENGINE_RUNLIST_PRI_BASE_ERROR))
            {
                bAnyEngineValid = true;
            }
        }

        if (!bAnyEngineValid)
        {
            Printf(Tee::PriError, "Failed to get runlist pri base addresses!\n");
            return rc;
        }
        Printf(Tee::PriWarn, "Failed to get one or more runlist pri base addresses!\n");
    }
    return OK;
}

RC HopperGpuSubdevice::GetHwEngineIds
(
    LwRm *pLwRm,
    LwU32 engineCount,
    LW2080_CTRL_GPU_GET_HW_ENGINE_ID_PARAMS *pHwIdParams
)
{
    // RM policy for hw engine and runlist base queries is that if an invalid engine is
    // specified it will be indicated by an error entry in the resulting list, so some data
    // will be correct and the invalid engines will have an ERROR token at their resulting
    // entry.  Allow this function to automatically skip any invalid entries, but if not
    // a single entry was valid then consider it a catastrophic failure and fail this function

    // Fill the runlistPriBase with NULL so that catastrophic failure can be detected
    fill(pHwIdParams->hwEngineID,
         pHwIdParams->hwEngineID + engineCount,
         LW2080_CTRL_GPU_GET_HW_ENGINE_ID_NULL);

    const RC rc = pLwRm->ControlBySubdevice(this,
                                            LW2080_CTRL_CMD_GPU_GET_HW_ENGINE_ID,
                                            pHwIdParams,
                                            sizeof(*pHwIdParams));
    if (OK != rc)
    {
        bool bAnyEngineValid = false;
        for (UINT32 lwrEng = 0; (lwrEng < engineCount) && !bAnyEngineValid; lwrEng++)
        {
            if ((pHwIdParams->hwEngineID[lwrEng] != LW2080_CTRL_GPU_GET_HW_ENGINE_ID_NULL) &&
                (pHwIdParams->hwEngineID[lwrEng] != LW2080_CTRL_GPU_GET_HW_ENGINE_ID_ERROR))
            {
                bAnyEngineValid = true;
            }
        }

        if (!bAnyEngineValid)
        {
            Printf(Tee::PriError, "Failed to get hw engine IDs!\n");
            return rc;
        }
        Printf(Tee::PriWarn, "Failed to get one or more hw engine IDs!\n");
    }
    return OK;
}

bool HopperGpuSubdevice::IsRunlistRegister(UINT32 offset) const
{
    return (offset >= DRF_BASE(LW_RUNLIST) && offset <= DRF_EXTENT(LW_RUNLIST)) ||
           (offset >= DRF_BASE(LW_CHRAM) && offset <= DRF_EXTENT(LW_CHRAM));
}

bool HopperGpuSubdevice::IsPerSMCEngineReg(UINT32 offset) const
{
    // On Ampere lwrrently only pgraph registers are per-smc and only when SMC is enabled
    return IsSMCEnabled() && IsPgraphRegister(offset);
}

void HopperGpuSubdevice::StopVpmCapture()
{
    for (UINT32 i = 0;
         i < Regs().LookupAddress(MODS_PERF_PMASYS_COMMAND_SLICE_TRIGGER_CONTROL__SIZE_1);
         i++)
    {
        Regs().Write32(MODS_PERF_PMASYS_COMMAND_SLICE_TRIGGER_CONTROL_STOPVPMCAPTURE_DOIT, i);
        Printf(Tee::PriNormal,
               "VPM Capture stopped (LW_PERF_PMASYS_COMMAND_SLICE_TRIGGER_CONTROL(%u)=0x%x).\n",
               i, Regs().Read32(MODS_PERF_PMASYS_COMMAND_SLICE_TRIGGER_CONTROL, i));
    }
}

//-----------------------------------------------------------------------------
RC HopperGpuSubdevice::GetXbarSecondaryPortEnabled(UINT32 port, bool *pEnabled) const
{
    RC rc;

    MASSERT(pEnabled);

    UINT32 numL2Slices = GetFB()->GetL2SliceCount();
    if (port >= numL2Slices)
    {
        Printf(Tee::PriError, "GetXbarSecondaryPortEnabled: Invalid port id: %u!"
                              " Device has only %u ports.\n", port, numL2Slices);
        rc = RC::SOFTWARE_ERROR;
        return rc;
    }

    const UINT32 slicesPerLtc = GetFB()->GetMaxSlicesPerLtc();
    const UINT32 ltc = port / slicesPerLtc;
    const UINT32 slice = port % slicesPerLtc;

    UINT32 regVal = RegRd32(Regs().LookupAddress(MODS_PLTCG_LTC0_LTS0_DSTG_CFG2)  +
                            Regs().LookupAddress(MODS_LTC_PRI_STRIDE) * ltc +
                            Regs().LookupAddress(MODS_LTS_PRI_STRIDE) * slice);

    *pEnabled = Regs().TestField(regVal, MODS_PLTCG_LTC0_LTS0_DSTG_CFG2_XBAR_PORT1_ENABLE_ENABLED);

    Printf(Tee::PriLow, "GetXbarSecondaryPortEnabled[%u]: %s\n", port, (*pEnabled ? "yes" : "no"));

    return rc;
}

//-----------------------------------------------------------------------------
RC HopperGpuSubdevice::EnableXbarSecondaryPort(UINT32 port, bool enable)
{
    RC rc;

    UINT32 numL2Slices = GetFB()->GetL2SliceCount();
    if (port >= numL2Slices)
    {
        Printf(Tee::PriError, "EnableXbarSecondaryPort: Invalid port id: %u!"
                              " Device has only %u ports.\n", port, numL2Slices);
        rc = RC::SOFTWARE_ERROR;
        return rc;
    }

    Printf(Tee::PriLow, "EnableXbarSecondaryPort: %u: %s\n", port, (enable ? "En" : "Dis"));

    const UINT32 slicesPerLtc = GetFB()->GetMaxSlicesPerLtc();
    const UINT32 ltc = port / slicesPerLtc;
    const UINT32 slice = port % slicesPerLtc;

    UINT32 regAddr = Regs().LookupAddress(MODS_PLTCG_LTC0_LTS0_DSTG_CFG2)  +
                     Regs().LookupAddress(MODS_LTC_PRI_STRIDE) * ltc +
                     Regs().LookupAddress(MODS_LTS_PRI_STRIDE) * slice;

    UINT32 regVal = RegRd32(regAddr);
    Regs().SetField(&regVal, (enable ? MODS_PLTCG_LTC0_LTS0_DSTG_CFG2_XBAR_PORT1_ENABLE_ENABLED
                                     : MODS_PLTCG_LTC0_LTS0_DSTG_CFG2_XBAR_PORT1_ENABLE_DISABLED));
    RegWr32(regAddr, regVal);

    return rc;
}

//-----------------------------------------------------------------------------
bool HopperGpuSubdevice::IsXbarSecondaryPortConfigurable() const
{
    return Regs().Test32(
            MODS_PLTCG_LTC0_LTS0_DSTG_CFG2_PRIV_LEVEL_MASK_WRITE_PROTECTION_ALL_LEVELS_ENABLED);
}

//-----------------------------------------------------------------------------
void HopperGpuSubdevice::PrintRunlistInfo(RegHal * pRegs)
{
    LW2080_CTRL_GPU_GET_ENGINES_V2_PARAMS engParams = { };
    const RC rc = pRegs->GetLwRmPtr()->ControlBySubdevice(this,
                                                          LW2080_CTRL_CMD_GPU_GET_ENGINES_V2,
                                                          &engParams,
                                                          sizeof(engParams));

    if (rc == OK)
    {
        UINT32 runlistEngineStatusSize = pRegs->IsSupported(MODS_RUNLIST_ENGINE_STATUS0) ?
            pRegs->LookupArraySize(MODS_RUNLIST_ENGINE_STATUS0, 1) : 0;

        UINT32 runlistEngineStatusDebugSize =
            pRegs->IsSupported(MODS_RUNLIST_ENGINE_STATUS_DEBUG) ?
                pRegs->LookupArraySize(MODS_RUNLIST_ENGINE_STATUS_DEBUG, 1) : 0;

        LW2080_CTRL_GPU_GET_HW_ENGINE_ID_PARAMS hwIdParams = { };
        memcpy(hwIdParams.engineList, engParams.engineList, sizeof(LwU32) * engParams.engineCount);
        GetHwEngineIds(pRegs->GetLwRmPtr(), engParams.engineCount, &hwIdParams);

        LW2080_CTRL_GPU_GET_ENGINE_RUNLIST_PRI_BASE_PARAMS baseParams = { };
        memcpy(baseParams.engineList, engParams.engineList, sizeof(LwU32) * engParams.engineCount);
        GetRunlistPriBases(pRegs->GetLwRmPtr(), engParams.engineCount, &baseParams);

        for (UINT32 lwrEng = 0; lwrEng < engParams.engineCount; lwrEng++)
        {
            const LwU32 priBase = baseParams.runlistPriBase[lwrEng];
            if ((priBase == LW2080_CTRL_GPU_GET_ENGINE_RUNLIST_PRI_BASE_NULL) ||
                (priBase == LW2080_CTRL_GPU_GET_ENGINE_RUNLIST_PRI_BASE_ERROR))
            {
                continue;
            }

            Printf(Tee::PriNormal,
                   "  Logical %s (ID=0x%x)",
                   GetLogicalEngineName(engParams.engineList[lwrEng]).c_str(),
                   engParams.engineList[lwrEng]);

            if ((hwIdParams.hwEngineID[lwrEng] != LW2080_CTRL_GPU_GET_HW_ENGINE_ID_NULL) &&
                (hwIdParams.hwEngineID[lwrEng] != LW2080_CTRL_GPU_GET_HW_ENGINE_ID_ERROR))
            {
                auto hwEngInfo = GetHwEngineInfo(hwIdParams.hwEngineID[lwrEng]);
                if (hwEngInfo != GetHwDevInfo().end())
                {
                    Printf(Tee::PriNormal,
                           ", HW %s%u (ID=0x%x)",
                           GpuSubdevice::HwDevTypeToString(hwEngInfo->hwType).c_str(),
                           hwEngInfo->hwInstance,
                           hwIdParams.hwEngineID[lwrEng]);
                }
                else
                {
                    Printf(Tee::PriNormal,
                           ", HW ID=0x%x",
                           hwIdParams.hwEngineID[lwrEng]);
                }
            }
            Printf(Tee::PriNormal, "\n");

            // Reading registers can cause prints in sim MODS, buffer the string
            string temp = "    LW_RUNLIST_ENGINE_STATUS0 : ";
            for (UINT32 i = 0; i < runlistEngineStatusSize; ++i)
            {
                temp += Utility::StrPrintf("0x%08x ",
                                           pRegs->Read32(engParams.engineList[lwrEng],
                                                         MODS_RUNLIST_ENGINE_STATUS0,
                                                         i));
            }
            Printf(Tee::PriNormal, "%s\n", temp.c_str());

            temp = "    LW_RUNLIST_ENGINE_STATUS_DEBUG : ";
            for (UINT32 i = 0; i < runlistEngineStatusDebugSize; ++i)
            {
                temp += Utility::StrPrintf("0x%08x ",
                                           pRegs->Read32(engParams.engineList[lwrEng],
                                                         MODS_RUNLIST_ENGINE_STATUS_DEBUG,
                                                         i));
            }
            Printf(Tee::PriDebug, "%s\n", temp.c_str());
        }
    }
    else
    {
        Printf(Tee::PriError, "Unable to query engine availability on client!\n");
    }
}

//------------------------------------------------------------------------------
vector<GpuSubdevice::HwDevInfo>::const_iterator HopperGpuSubdevice::GetHwEngineInfo(UINT32 hwEngineId)
{
    const vector<GpuSubdevice::HwDevInfo> & hwDevInfo = GetHwDevInfo();

    // Find the matching hwEngineId returned from RM in the HW device info parsed
    // from PTOP and then create a HW Engine string based on that information.
    //
    // When SMC is enabled each SMC will allow matching of the logical engine
    // to the HW engine that RM assigned during SMC configuration
    auto hwEngInfo =
        find_if(hwDevInfo.begin(),
                hwDevInfo.end(),
                [hwEngineId] (const GpuSubdevice::HwDevInfo & lwrInfo) -> bool
    {
        return (lwrInfo.bIsEngine && (lwrInfo.hwEngineId == hwEngineId));
    });

    return hwEngInfo;
}

//------------------------------------------------------------------------------
string HopperGpuSubdevice::GetLogicalEngineName(UINT32 engine)
{
    // The values that are returned from GpuDevice::GetEngineName are global and
    // do not match more modern GPUs, in addition, they cannot be changed since the
    // strings returned from GpuDevice::GetEngineName are used to compare against
    // strings that are present in Arch policy files.
    //
    // Create Ampere specific mappings which can differ from the GpuDevice mappings.
    //
    switch (engine)
    {
        case LW2080_ENGINE_TYPE_GR0:        return "GRAPHICS0";
        case LW2080_ENGINE_TYPE_COPY0:      return "LCE0";
        case LW2080_ENGINE_TYPE_COPY1:      return "LCE1";
        case LW2080_ENGINE_TYPE_COPY2:      return "LCE2";
        case LW2080_ENGINE_TYPE_COPY3:      return "LCE3";
        case LW2080_ENGINE_TYPE_COPY4:      return "LCE4";
        case LW2080_ENGINE_TYPE_COPY5:      return "LCE5";
        case LW2080_ENGINE_TYPE_COPY6:      return "LCE6";
        case LW2080_ENGINE_TYPE_COPY7:      return "LCE7";
        case LW2080_ENGINE_TYPE_COPY8:      return "LCE8";
        case LW2080_ENGINE_TYPE_COPY9:      return "LCE9";
        case LW2080_ENGINE_TYPE_LWDEC0:     return "LWDEC0";
        case LW2080_ENGINE_TYPE_LWENC0:     return "LWENC0";
        case LW2080_ENGINE_TYPE_SEC2:       return "SEC";
        default:                            break;
    }
    return GetParentDevice()->GetEngineName(engine);
}

RC HopperGpuSubdevice::GetKFuseStatus(KFuseStatus *pStatus, FLOAT64 timeoutMs)
{
    if (!IsFeatureEnabled(GPUSUB_HAS_KFUSES))
        return RC::UNSUPPORTED_HARDWARE_FEATURE;

    MASSERT(pStatus);
    RC rc;
    RegHal & regs = Regs();

    // Part 1: Kick off the KFuse state machine
    // incomplete KFuse manual:
    // LW_KFUSE_STATE_RESET value (0 / 1) is not clearly defined
    regs.Write32(MODS_FUSE_KFUSE_STATE, Regs().SetField(MODS_FUSE_KFUSE_STATE_RESET, 1));

    CHECK_RC(Tasker::PollHw(timeoutMs, [&]()->bool
    {
        const UINT32 KfuseIlwalidVal = 0xbadf1002;
        UINT32 state = regs.Read32(MODS_FUSE_KFUSE_STATE);
        return (state != KfuseIlwalidVal) && regs.TestField(state, MODS_FUSE_KFUSE_STATE_DONE, 1);
    }, __FUNCTION__));

    // Part 2: Make sure CRC passes
    pStatus->CrcPass = Regs().Test32(MODS_FUSE_KFUSE_STATE_CRCPASS, 1);

    UINT32 ErrorCount   = regs.Read32(MODS_FUSE_KFUSE_ERRCOUNT);
    pStatus->Error1     = regs.GetField(ErrorCount, MODS_FUSE_KFUSE_ERRCOUNT_ERR_1);
    pStatus->Error2     = regs.GetField(ErrorCount, MODS_FUSE_KFUSE_ERRCOUNT_ERR_2);
    pStatus->Error3     = regs.GetField(ErrorCount, MODS_FUSE_KFUSE_ERRCOUNT_ERR_3);
    pStatus->FatalError = regs.GetField(ErrorCount, MODS_FUSE_KFUSE_ERRCOUNT_ERR_FATAL);

    // Part 3: Read the HDCP Keys. Set the key addr to autoincrement mode
    ScopedRegister OrgVal(this, regs.LookupAddress(MODS_FUSE_KFUSE_KEYADDR));
    regs.Write32(MODS_FUSE_KFUSE_KEYADDR, regs.SetField(MODS_FUSE_KFUSE_KEYADDR_AUTOINC, 1));
    const UINT32 NUM_KEYS = 144;
    pStatus->Keys.clear();
    for (UINT32 i = 0; i < NUM_KEYS; i++)
    {
        UINT32 newKey = regs.Read32(MODS_FUSE_KFUSE_KEYS);
        pStatus->Keys.push_back(newKey);
    }

    return rc;
}

//-------------------------------------------------------------------------------
double HopperGpuSubdevice::GetIssueRate(Instr type)
{
    RegHal& regs = Regs();
    const auto ssVal0 = regs.Read32(MODS_FUSE_FEATURE_OVERRIDE_SM_SPEED_SELECT);
    const auto ssVal1 = regs.Read32(MODS_FUSE_FEATURE_OVERRIDE_SM_SPEED_SELECT_1);
    switch (type)
    {
        case Instr::FFMA:
        {
            if (regs.Test32(MODS_FUSE_FEATURE_READOUT_1_SM_SPEED_SELECT_FFMA_FULL_SPEED))
            {
                return 1.0;
            }

            if (regs.TestField(ssVal0, MODS_FUSE_FEATURE_OVERRIDE_SM_SPEED_SELECT_FFMA_FULL_SPEED))
            {
                return 1.0;
            }
            else if (regs.TestField(ssVal0, MODS_FUSE_FEATURE_OVERRIDE_SM_SPEED_SELECT_FFMA_REDUCED_SPEED_1_2))
            {
                return 1.0 / 2.0;
            }
            else if (regs.TestField(ssVal0, MODS_FUSE_FEATURE_OVERRIDE_SM_SPEED_SELECT_FFMA_REDUCED_SPEED_1_4))
            {
                return 1.0 / 4.0;
            }
            else if (regs.TestField(ssVal0, MODS_FUSE_FEATURE_OVERRIDE_SM_SPEED_SELECT_FFMA_REDUCED_SPEED_1_8))
            {
                return 1.0 / 8.0;
            }
            else if (regs.TestField(ssVal0, MODS_FUSE_FEATURE_OVERRIDE_SM_SPEED_SELECT_FFMA_REDUCED_SPEED_1_16))
            {
                return 1.0 / 16.0;
            }
            else if (regs.TestField(ssVal0, MODS_FUSE_FEATURE_OVERRIDE_SM_SPEED_SELECT_FFMA_REDUCED_SPEED_1_32))
            {
                return 1.0 / 32.0;
            }
            else if (regs.TestField(ssVal0, MODS_FUSE_FEATURE_OVERRIDE_SM_SPEED_SELECT_FFMA_REDUCED_SPEED_1_64))
            {
                return 1.0 / 64.0;
            }
            else if (regs.GetField(ssVal0, MODS_FUSE_FEATURE_OVERRIDE_SM_SPEED_SELECT_FFMA) == 0x7)
            {
                return 1.0 / 64.0;
            }
            else
            {
                MASSERT(!"Invalid instruction issue-rate");
                return 0.0;
            }
        }

        case Instr::HMMA_884_F16_F16:
        case Instr::HMMA_1688_F16_F16:
        case Instr::HMMA_16816_F16_F16:
        case Instr::HGMMA_F16_F16:
        case Instr::HGMMA_SP_F16_F16:
        {
            if (regs.Test32(MODS_FUSE_FEATURE_READOUT_1_SM_SPEED_SELECT_FMLA16_FULL_SPEED))
            {
                return 1.0;
            }

            if (regs.TestField(ssVal0, MODS_FUSE_FEATURE_OVERRIDE_SM_SPEED_SELECT_FMLA16_FULL_SPEED))
            {
                return 1.0;
            }
            else if (regs.TestField(ssVal0, MODS_FUSE_FEATURE_OVERRIDE_SM_SPEED_SELECT_FMLA16_REDUCED_SPEED_1_2))
            {
                return 1.0 / 2.0;
            }
            else if (regs.TestField(ssVal0, MODS_FUSE_FEATURE_OVERRIDE_SM_SPEED_SELECT_FMLA16_REDUCED_SPEED_1_4))
            {
                return 1.0 / 4.0;
            }
            else if (regs.TestField(ssVal0, MODS_FUSE_FEATURE_OVERRIDE_SM_SPEED_SELECT_FMLA16_REDUCED_SPEED_1_8))
            {
                return 1.0 / 8.0;
            }
            else if (regs.TestField(ssVal0, MODS_FUSE_FEATURE_OVERRIDE_SM_SPEED_SELECT_FMLA16_REDUCED_SPEED_1_16))
            {
                return 1.0 / 16.0;
            }
            else if (regs.TestField(ssVal0, MODS_FUSE_FEATURE_OVERRIDE_SM_SPEED_SELECT_FMLA16_REDUCED_SPEED_1_32))
            {
                return 1.0 / 32.0;
            }
            else if (regs.TestField(ssVal0, MODS_FUSE_FEATURE_OVERRIDE_SM_SPEED_SELECT_FMLA16_REDUCED_SPEED_1_64))
            {
                return 1.0 / 64.0;
            }
            else if (regs.GetField(ssVal0, MODS_FUSE_FEATURE_OVERRIDE_SM_SPEED_SELECT_FMLA16) == 0x7)
            {
                return 1.0 / 64.0;
            }
            else
            {
                MASSERT(!"Invalid instruction issue-rate");
                return 0.0;
            }
        }

        case Instr::HMMA_884_F32_F16:
        case Instr::HMMA_1688_F32_F16:
        case Instr::HMMA_1688_F32_BF16:
        case Instr::HMMA_16816_F32_F16:
        case Instr::HMMA_16816_F32_BF16:
        case Instr::HMMA_16832SP_F32_F16:
        case Instr::HGMMA_F32_F16:
        case Instr::HGMMA_F32_BF16:
        case Instr::HGMMA_SP_F32_F16:
        {
            if (regs.Test32(MODS_FUSE_FEATURE_READOUT_1_SM_SPEED_SELECT_FMLA32_FULL_SPEED))
            {
                return 1.0;
            }

            if (regs.TestField(ssVal0, MODS_FUSE_FEATURE_OVERRIDE_SM_SPEED_SELECT_FMLA32_FULL_SPEED))
            {
                return 1.0;
            }
            else if (regs.TestField(ssVal0, MODS_FUSE_FEATURE_OVERRIDE_SM_SPEED_SELECT_FMLA32_REDUCED_SPEED_1_2))
            {
                return 1.0 / 2.0;
            }
            else if (regs.TestField(ssVal0, MODS_FUSE_FEATURE_OVERRIDE_SM_SPEED_SELECT_FMLA32_REDUCED_SPEED_1_4))
            {
                return 1.0 / 4.0;
            }
            else if (regs.TestField(ssVal0, MODS_FUSE_FEATURE_OVERRIDE_SM_SPEED_SELECT_FMLA32_REDUCED_SPEED_1_8))
            {
                return 1.0 / 8.0;
            }
            else if (regs.TestField(ssVal0, MODS_FUSE_FEATURE_OVERRIDE_SM_SPEED_SELECT_FMLA32_REDUCED_SPEED_1_16))
            {
                return 1.0 / 16.0;
            }
            else if (regs.TestField(ssVal0, MODS_FUSE_FEATURE_OVERRIDE_SM_SPEED_SELECT_FMLA32_REDUCED_SPEED_1_32))
            {
                return 1.0 / 32.0;
            }
            else if (regs.TestField(ssVal0, MODS_FUSE_FEATURE_OVERRIDE_SM_SPEED_SELECT_FMLA32_REDUCED_SPEED_1_64))
            {
                return 1.0 / 64.0;
            }
            else if (regs.GetField(ssVal0, MODS_FUSE_FEATURE_OVERRIDE_SM_SPEED_SELECT_FMLA32) == 0x7)
            {
                return 1.0 / 64.0;
            }
            else
            {
                MASSERT(!"Invalid instruction issue-rate");
                return 0.0;
            }
        }

        case Instr::IMMA_8816_S32_S8:
        case Instr::IMMA_16832_S32_S8:
        case Instr::IMMA_16864SP_S32_S8:
        case Instr::IGMMA_S32_S8:
        case Instr::IGMMA_SP_S32_S8:
        {
            if (regs.Test32(MODS_FUSE_FEATURE_READOUT_1_SM_SPEED_SELECT_IMLA0_FULL_SPEED))
            {
                return 1.0;
            }

            if (regs.TestField(ssVal0, MODS_FUSE_FEATURE_OVERRIDE_SM_SPEED_SELECT_IMLA0_FULL_SPEED))
            {
                return 1.0;
            }
            else if (regs.TestField(ssVal0, MODS_FUSE_FEATURE_OVERRIDE_SM_SPEED_SELECT_IMLA0_REDUCED_SPEED_1_2))
            {
                return 1.0 / 2.0;
            }
            else if (regs.TestField(ssVal0, MODS_FUSE_FEATURE_OVERRIDE_SM_SPEED_SELECT_IMLA0_REDUCED_SPEED_1_4))
            {
                return 1.0 / 4.0;
            }
            else if (regs.TestField(ssVal0, MODS_FUSE_FEATURE_OVERRIDE_SM_SPEED_SELECT_IMLA0_REDUCED_SPEED_1_8))
            {
                return 1.0 / 8.0;
            }
            else if (regs.TestField(ssVal0, MODS_FUSE_FEATURE_OVERRIDE_SM_SPEED_SELECT_IMLA0_REDUCED_SPEED_1_16))
            {
                return 1.0 / 16.0;
            }
            else if (regs.TestField(ssVal0, MODS_FUSE_FEATURE_OVERRIDE_SM_SPEED_SELECT_IMLA0_REDUCED_SPEED_1_32))
            {
                return 1.0 / 32.0;
            }
            else if (regs.TestField(ssVal0, MODS_FUSE_FEATURE_OVERRIDE_SM_SPEED_SELECT_IMLA0_REDUCED_SPEED_1_64))
            {
                return 1.0 / 64.0;
            }
            else if (regs.GetField(ssVal0, MODS_FUSE_FEATURE_OVERRIDE_SM_SPEED_SELECT_IMLA0) == 0x7)
            {
                return 1.0 / 64.0;
            }
            else
            {
                MASSERT(!"Invalid instruction issue-rate");
                return 0.0;
            }
        }

        case Instr::BMMA_AND_168256_S32_B1:
        {
            if (regs.Test32(MODS_FUSE_FEATURE_READOUT_1_SM_SPEED_SELECT_IMLA2_FULL_SPEED))
            {
                return 1.0;
            }

            if (regs.TestField(ssVal0, MODS_FUSE_FEATURE_OVERRIDE_SM_SPEED_SELECT_IMLA2_FULL_SPEED))
            {
                return 1.0;
            }
            else if (regs.TestField(ssVal0, MODS_FUSE_FEATURE_OVERRIDE_SM_SPEED_SELECT_IMLA2_REDUCED_SPEED_1_2))
            {
                return 1.0 / 2.0;
            }
            else if (regs.TestField(ssVal0, MODS_FUSE_FEATURE_OVERRIDE_SM_SPEED_SELECT_IMLA2_REDUCED_SPEED_1_4))
            {
                return 1.0 / 4.0;
            }
            else if (regs.TestField(ssVal0, MODS_FUSE_FEATURE_OVERRIDE_SM_SPEED_SELECT_IMLA2_REDUCED_SPEED_1_8))
            {
                return 1.0 / 8.0;
            }
            else if (regs.TestField(ssVal0, MODS_FUSE_FEATURE_OVERRIDE_SM_SPEED_SELECT_IMLA2_REDUCED_SPEED_1_16))
            {
                return 1.0 / 16.0;
            }
            else if (regs.TestField(ssVal0, MODS_FUSE_FEATURE_OVERRIDE_SM_SPEED_SELECT_IMLA2_REDUCED_SPEED_1_32))
            {
                return 1.0 / 32.0;
            }
            else if (regs.TestField(ssVal0, MODS_FUSE_FEATURE_OVERRIDE_SM_SPEED_SELECT_IMLA2_REDUCED_SPEED_1_64))
            {
                return 1.0 / 64.0;
            }
            else if (regs.GetField(ssVal0, MODS_FUSE_FEATURE_OVERRIDE_SM_SPEED_SELECT_IMLA2) == 0x7)
            {
                return 1.0 / 64.0;
            }
            else
            {
                MASSERT(!"Invalid instruction issue-rate");
                return 0.0;
            }
        }

        case Instr::IDP4A_S32_S8:
        {
            if (regs.Test32(MODS_FUSE_FEATURE_READOUT_1_SM_SPEED_SELECT_IMLA3_FULL_SPEED))
            {
                return 1.0;
            }

            if (regs.TestField(ssVal0, MODS_FUSE_FEATURE_OVERRIDE_SM_SPEED_SELECT_IMLA3_FULL_SPEED))
            {
                return 1.0;
            }
            else if (regs.TestField(ssVal0, MODS_FUSE_FEATURE_OVERRIDE_SM_SPEED_SELECT_IMLA3_REDUCED_SPEED_1_2))
            {
                return 1.0 / 2.0;
            }
            else if (regs.TestField(ssVal0, MODS_FUSE_FEATURE_OVERRIDE_SM_SPEED_SELECT_IMLA3_REDUCED_SPEED_1_4))
            {
                return 1.0 / 4.0;
            }
            else if (regs.TestField(ssVal0, MODS_FUSE_FEATURE_OVERRIDE_SM_SPEED_SELECT_IMLA3_REDUCED_SPEED_1_8))
            {
                return 1.0 / 8.0;
            }
            else if (regs.TestField(ssVal0, MODS_FUSE_FEATURE_OVERRIDE_SM_SPEED_SELECT_IMLA3_REDUCED_SPEED_1_16))
            {
                return 1.0 / 16.0;
            }
            else if (regs.TestField(ssVal0, MODS_FUSE_FEATURE_OVERRIDE_SM_SPEED_SELECT_IMLA3_REDUCED_SPEED_1_32))
            {
                return 1.0 / 32.0;
            }
            // No 1/64 speed for IMLA3
            else if (regs.TestField(ssVal0, MODS_FUSE_FEATURE_OVERRIDE_SM_SPEED_SELECT_IMLA3_REDUCED_SPEED_1_64) ||
                     regs.GetField(ssVal0, MODS_FUSE_FEATURE_OVERRIDE_SM_SPEED_SELECT_IMLA3) == 0x7)
            {
                return 1.0 / 32.0;
            }
            else
            {
                MASSERT(!"Invalid instruction issue-rate");
                return 0.0;
            }
        }

        case Instr::HMMA_1684_F32_TF32:
        case Instr::HMMA_1688_F32_TF32:
        case Instr::HGMMA_F32_TF32:
        {
            if (regs.Test32(MODS_FUSE_FEATURE_READOUT_1_SM_SPEED_SELECT_IMLA4_FULL_SPEED))
            {
                return 1.0;
            }

            if (regs.TestField(ssVal1, MODS_FUSE_FEATURE_OVERRIDE_SM_SPEED_SELECT_1_IMLA4_FULL_SPEED))
            {
                return 1.0;
            }
            else if (regs.TestField(ssVal1, MODS_FUSE_FEATURE_OVERRIDE_SM_SPEED_SELECT_1_IMLA4_REDUCED_SPEED_1_2))
            {
                return 1.0 / 2.0;
            }
            else if (regs.TestField(ssVal1, MODS_FUSE_FEATURE_OVERRIDE_SM_SPEED_SELECT_1_IMLA4_REDUCED_SPEED_1_4))
            {
                return 1.0 / 4.0;
            }
            else if (regs.TestField(ssVal1, MODS_FUSE_FEATURE_OVERRIDE_SM_SPEED_SELECT_1_IMLA4_REDUCED_SPEED_1_8))
            {
                return 1.0 / 8.0;
            }
            else if (regs.TestField(ssVal1, MODS_FUSE_FEATURE_OVERRIDE_SM_SPEED_SELECT_1_IMLA4_REDUCED_SPEED_1_16))
            {
                return 1.0 / 16.0;
            }
            else if (regs.TestField(ssVal1, MODS_FUSE_FEATURE_OVERRIDE_SM_SPEED_SELECT_1_IMLA4_REDUCED_SPEED_1_32))
            {
                return 1.0 / 32.0;
            }
            else if (regs.TestField(ssVal1, MODS_FUSE_FEATURE_OVERRIDE_SM_SPEED_SELECT_1_IMLA4_REDUCED_SPEED_1_64))
            {
                return 1.0 / 64.0;
            }
            else if (regs.GetField(ssVal1, MODS_FUSE_FEATURE_OVERRIDE_SM_SPEED_SELECT_1_IMLA4) == 0x7)
            {
                return 1.0 / 64.0;
            }
            else
            {
                MASSERT(!"Invalid instruction issue-rate");
                return 0.0;
            }
        }

        case Instr::DFMA:
        case Instr::DMMA_884:
        {
            if (regs.Test32(MODS_FUSE_FEATURE_READOUT_1_SM_SPEED_SELECT_DP_FULL_SPEED))
            {
                return 1.0;
            }

            if (regs.TestField(ssVal0, MODS_FUSE_FEATURE_OVERRIDE_SM_SPEED_SELECT_DP_FULL_SPEED))
            {
                return 1.0;
            }
            else if (regs.TestField(ssVal0, MODS_FUSE_FEATURE_OVERRIDE_SM_SPEED_SELECT_DP_REDUCED_SPEED))
            {
                return 1.0 / 32.0;
            }
            else
            {
                MASSERT(!"Invalid instruction issue-rate");
                return 0.0;
            }
        }

        case Instr::DMMA_16816:
        {
            if (regs.Test32(MODS_FUSE_FEATURE_READOUT_1_SM_SPEED_SELECT_DP_FULL_SPEED))
            {
                return 1.0;
            }

            if (regs.TestField(ssVal0, MODS_FUSE_FEATURE_OVERRIDE_SM_SPEED_SELECT_DP_FULL_SPEED))
            {
                return 1.0;
            }
            else if (regs.TestField(ssVal0, MODS_FUSE_FEATURE_OVERRIDE_SM_SPEED_SELECT_DP_REDUCED_SPEED))
            {
                return 1.0 / 64.0;
            }
            else
            {
                MASSERT(!"Invalid instruction issue-rate");
                return 0.0;
            }
        }

        case Instr::HFMA2:
            return 1.0;

        default:
            MASSERT(!"Unsupported instruction passed to function 'GetIssueRate'");
            return 0.0;
    }
}

RC HopperGpuSubdevice::CheckIssueRateOverride()
{
    RC rc;

    // Check for feature support
    if (!Regs().IsSupported(MODS_FUSE_FEATURE_OVERRIDE_SM_SPEED_SELECT))
    {
        Printf(Tee::PriError,
               "Chip under test does not support instruction-issue rate override (Volta-953)\n");
        return RC::UNSUPPORTED_HARDWARE_FEATURE;
    }
    // Check that register isn't lwrrently PRIV-protected
    if (!Regs().Test32(MODS_FUSE_FEATURE_OVERRIDE_PRIV_LEVEL_MASK_WRITE_PROTECTION_LEVEL0_ENABLE))
    {
        Printf(Tee::PriError,
               "Instruction issue-rate override (Volta-953) not supported on this board "
               "(FUSE_FEATURE_OVERRIDE is PRIV protected)\n");
               return RC::PRIV_LEVEL_VIOLATION;
    }
    return rc;
}


RC HopperGpuSubdevice::GetIssueRateOverride(IssueRateOverrideSettings *pOverrides)
{
    RegHal& regs = Regs();
    pOverrides->speedSelect0 = regs.Read32(MODS_FUSE_FEATURE_OVERRIDE_SM_SPEED_SELECT);
    pOverrides->speedSelect1 = regs.Read32(MODS_FUSE_FEATURE_OVERRIDE_SM_SPEED_SELECT_1);
    return RC::OK;
}

RC HopperGpuSubdevice::IssueRateOverride(const IssueRateOverrideSettings& overrides)
{
    RC rc;
    RegHal& regs = Regs();

    CHECK_RC(CheckIssueRateOverride());
    regs.Write32(MODS_FUSE_FEATURE_OVERRIDE_SM_SPEED_SELECT, overrides.speedSelect0);
    regs.Write32(MODS_FUSE_FEATURE_OVERRIDE_SM_SPEED_SELECT_1, overrides.speedSelect1);
    return rc;
}

RC HopperGpuSubdevice::IssueRateOverride(const string& unit, UINT32 throttle)
{
    RC rc;
    RegHal& regs = Regs();

    CHECK_RC(CheckIssueRateOverride());
    Printf(Tee::PriNormal, "Overriding unit %s issue rate to 1/%d\n", unit.c_str(), throttle);

    UINT32 reg0 = regs.Read32(MODS_FUSE_FEATURE_OVERRIDE_SM_SPEED_SELECT);
    UINT32 reg1 = regs.Read32(MODS_FUSE_FEATURE_OVERRIDE_SM_SPEED_SELECT_1);

    if (unit == "FFMA")
    {
        regs.SetField(&reg0, MODS_FUSE_FEATURE_OVERRIDE_SM_SPEED_SELECT_FFMA_OVERRIDE_TRUE);
        switch (throttle)
        {
            case 1:
                regs.SetField(&reg0, MODS_FUSE_FEATURE_OVERRIDE_SM_SPEED_SELECT_FFMA_FULL_SPEED);
                break;
            case 2:
                regs.SetField(&reg0, MODS_FUSE_FEATURE_OVERRIDE_SM_SPEED_SELECT_FFMA_REDUCED_SPEED_1_2);
                break;
            case 4:
                regs.SetField(&reg0, MODS_FUSE_FEATURE_OVERRIDE_SM_SPEED_SELECT_FFMA_REDUCED_SPEED_1_4);
                break;
            case 8:
                regs.SetField(&reg0, MODS_FUSE_FEATURE_OVERRIDE_SM_SPEED_SELECT_FFMA_REDUCED_SPEED_1_8);
                break;
            case 16:
                regs.SetField(&reg0, MODS_FUSE_FEATURE_OVERRIDE_SM_SPEED_SELECT_FFMA_REDUCED_SPEED_1_16);
                break;
            case 32:
                regs.SetField(&reg0, MODS_FUSE_FEATURE_OVERRIDE_SM_SPEED_SELECT_FFMA_REDUCED_SPEED_1_32);
                break;
            case 64:
                regs.SetField(&reg0, MODS_FUSE_FEATURE_OVERRIDE_SM_SPEED_SELECT_FFMA_REDUCED_SPEED_1_64);
                break;
            default:
                Printf(Tee::PriError,
                       "Issue-rate throttle 1/%d not supported for this unit!\n", throttle);
                return RC::BAD_COMMAND_LINE_ARGUMENT;
        }
    }
    else if (unit == "FMLA16")
    {
        regs.SetField(&reg0, MODS_FUSE_FEATURE_OVERRIDE_SM_SPEED_SELECT_FMLA16_OVERRIDE_TRUE);
        switch (throttle)
        {
            case 1:
                regs.SetField(&reg0, MODS_FUSE_FEATURE_OVERRIDE_SM_SPEED_SELECT_FMLA16_FULL_SPEED);
                break;
            case 2:
                regs.SetField(&reg0, MODS_FUSE_FEATURE_OVERRIDE_SM_SPEED_SELECT_FMLA16_REDUCED_SPEED_1_2);
                break;
            case 4:
                regs.SetField(&reg0, MODS_FUSE_FEATURE_OVERRIDE_SM_SPEED_SELECT_FMLA16_REDUCED_SPEED_1_4);
                break;
            case 8:
                regs.SetField(&reg0, MODS_FUSE_FEATURE_OVERRIDE_SM_SPEED_SELECT_FMLA16_REDUCED_SPEED_1_8);
                break;
            case 16:
                regs.SetField(&reg0, MODS_FUSE_FEATURE_OVERRIDE_SM_SPEED_SELECT_FMLA16_REDUCED_SPEED_1_16);
                break;
            case 32:
                regs.SetField(&reg0, MODS_FUSE_FEATURE_OVERRIDE_SM_SPEED_SELECT_FMLA16_REDUCED_SPEED_1_32);
                break;
            case 64:
                regs.SetField(&reg0, MODS_FUSE_FEATURE_OVERRIDE_SM_SPEED_SELECT_FMLA16_REDUCED_SPEED_1_64);
                break;
            default:
                Printf(Tee::PriError,
                       "Issue-rate throttle 1/%d not supported for this unit!\n", throttle);
                return RC::BAD_COMMAND_LINE_ARGUMENT;
        }
    }
    else if (unit == "FMLA32")
    {
        regs.SetField(&reg0, MODS_FUSE_FEATURE_OVERRIDE_SM_SPEED_SELECT_FMLA32_OVERRIDE_TRUE);
        switch (throttle)
        {
            case 1:
                regs.SetField(&reg0, MODS_FUSE_FEATURE_OVERRIDE_SM_SPEED_SELECT_FMLA32_FULL_SPEED);
                break;
            case 2:
                regs.SetField(&reg0, MODS_FUSE_FEATURE_OVERRIDE_SM_SPEED_SELECT_FMLA32_REDUCED_SPEED_1_2);
                break;
            case 4:
                regs.SetField(&reg0, MODS_FUSE_FEATURE_OVERRIDE_SM_SPEED_SELECT_FMLA32_REDUCED_SPEED_1_4);
                break;
            case 8:
                regs.SetField(&reg0, MODS_FUSE_FEATURE_OVERRIDE_SM_SPEED_SELECT_FMLA32_REDUCED_SPEED_1_8);
                break;
            case 16:
                regs.SetField(&reg0, MODS_FUSE_FEATURE_OVERRIDE_SM_SPEED_SELECT_FMLA32_REDUCED_SPEED_1_16);
                break;
            case 32:
                regs.SetField(&reg0, MODS_FUSE_FEATURE_OVERRIDE_SM_SPEED_SELECT_FMLA32_REDUCED_SPEED_1_32);
                break;
            case 64:
                regs.SetField(&reg0, MODS_FUSE_FEATURE_OVERRIDE_SM_SPEED_SELECT_FMLA32_REDUCED_SPEED_1_64);
                break;
            default:
                Printf(Tee::PriError,
                       "Issue-rate throttle 1/%d not supported for this unit!\n", throttle);
                return RC::BAD_COMMAND_LINE_ARGUMENT;
        }
    }
    else if (unit == "IMLA0")
    {
        regs.SetField(&reg0, MODS_FUSE_FEATURE_OVERRIDE_SM_SPEED_SELECT_IMLA0_OVERRIDE_TRUE);
        switch (throttle)
        {
            case 1:
                regs.SetField(&reg0, MODS_FUSE_FEATURE_OVERRIDE_SM_SPEED_SELECT_IMLA0_FULL_SPEED);
                break;
            case 2:
                regs.SetField(&reg0, MODS_FUSE_FEATURE_OVERRIDE_SM_SPEED_SELECT_IMLA0_REDUCED_SPEED_1_2);
                break;
            case 4:
                regs.SetField(&reg0, MODS_FUSE_FEATURE_OVERRIDE_SM_SPEED_SELECT_IMLA0_REDUCED_SPEED_1_4);
                break;
            case 8:
                regs.SetField(&reg0, MODS_FUSE_FEATURE_OVERRIDE_SM_SPEED_SELECT_IMLA0_REDUCED_SPEED_1_8);
                break;
            case 16:
                regs.SetField(&reg0, MODS_FUSE_FEATURE_OVERRIDE_SM_SPEED_SELECT_IMLA0_REDUCED_SPEED_1_16);
                break;
            case 32:
                regs.SetField(&reg0, MODS_FUSE_FEATURE_OVERRIDE_SM_SPEED_SELECT_IMLA0_REDUCED_SPEED_1_32);
                break;
            case 64:
                regs.SetField(&reg0, MODS_FUSE_FEATURE_OVERRIDE_SM_SPEED_SELECT_IMLA0_REDUCED_SPEED_1_64);
                break;
            default:
                Printf(Tee::PriError,
                       "Issue-rate throttle 1/%d not supported for this unit!\n", throttle);
                return RC::BAD_COMMAND_LINE_ARGUMENT;
        }
    }
    else if (unit == "IMLA1")
    {
        regs.SetField(&reg0, MODS_FUSE_FEATURE_OVERRIDE_SM_SPEED_SELECT_IMLA1_OVERRIDE_TRUE);
        switch (throttle)
        {
            case 1:
                regs.SetField(&reg0, MODS_FUSE_FEATURE_OVERRIDE_SM_SPEED_SELECT_IMLA1_FULL_SPEED);
                break;
            case 2:
                regs.SetField(&reg0, MODS_FUSE_FEATURE_OVERRIDE_SM_SPEED_SELECT_IMLA1_REDUCED_SPEED_1_2);
                break;
            case 4:
                regs.SetField(&reg0, MODS_FUSE_FEATURE_OVERRIDE_SM_SPEED_SELECT_IMLA1_REDUCED_SPEED_1_4);
                break;
            case 8:
                regs.SetField(&reg0, MODS_FUSE_FEATURE_OVERRIDE_SM_SPEED_SELECT_IMLA1_REDUCED_SPEED_1_8);
                break;
            case 16:
                regs.SetField(&reg0, MODS_FUSE_FEATURE_OVERRIDE_SM_SPEED_SELECT_IMLA1_REDUCED_SPEED_1_16);
                break;
            case 32:
                regs.SetField(&reg0, MODS_FUSE_FEATURE_OVERRIDE_SM_SPEED_SELECT_IMLA1_REDUCED_SPEED_1_32);
                break;
            case 64:
                regs.SetField(&reg0, MODS_FUSE_FEATURE_OVERRIDE_SM_SPEED_SELECT_IMLA1_REDUCED_SPEED_1_64);
                break;
            default:
                Printf(Tee::PriError,
                       "Issue-rate throttle 1/%d not supported for this unit!\n", throttle);
                return RC::BAD_COMMAND_LINE_ARGUMENT;
        }
    }
    else if (unit == "IMLA2")
    {
        regs.SetField(&reg0, MODS_FUSE_FEATURE_OVERRIDE_SM_SPEED_SELECT_IMLA2_OVERRIDE_TRUE);
        switch (throttle)
        {
            case 1:
                regs.SetField(&reg0, MODS_FUSE_FEATURE_OVERRIDE_SM_SPEED_SELECT_IMLA2_FULL_SPEED);
                break;
            case 2:
                regs.SetField(&reg0, MODS_FUSE_FEATURE_OVERRIDE_SM_SPEED_SELECT_IMLA2_REDUCED_SPEED_1_2);
                break;
            case 4:
                regs.SetField(&reg0, MODS_FUSE_FEATURE_OVERRIDE_SM_SPEED_SELECT_IMLA2_REDUCED_SPEED_1_4);
                break;
            case 8:
                regs.SetField(&reg0, MODS_FUSE_FEATURE_OVERRIDE_SM_SPEED_SELECT_IMLA2_REDUCED_SPEED_1_8);
                break;
            case 16:
                regs.SetField(&reg0, MODS_FUSE_FEATURE_OVERRIDE_SM_SPEED_SELECT_IMLA2_REDUCED_SPEED_1_16);
                break;
            case 32:
                regs.SetField(&reg0, MODS_FUSE_FEATURE_OVERRIDE_SM_SPEED_SELECT_IMLA2_REDUCED_SPEED_1_32);
                break;
            case 64:
                regs.SetField(&reg0, MODS_FUSE_FEATURE_OVERRIDE_SM_SPEED_SELECT_IMLA2_REDUCED_SPEED_1_64);
                break;
            default:
                Printf(Tee::PriError,
                       "Issue-rate throttle 1/%d not supported for this unit!\n", throttle);
                return RC::BAD_COMMAND_LINE_ARGUMENT;
        }
    }
    else if (unit == "IMLA3")
    {
        regs.SetField(&reg0, MODS_FUSE_FEATURE_OVERRIDE_SM_SPEED_SELECT_IMLA3_OVERRIDE_TRUE);
        switch (throttle)
        {
            case 1:
                regs.SetField(&reg0, MODS_FUSE_FEATURE_OVERRIDE_SM_SPEED_SELECT_IMLA3_FULL_SPEED);
                break;
            case 2:
                regs.SetField(&reg0, MODS_FUSE_FEATURE_OVERRIDE_SM_SPEED_SELECT_IMLA3_REDUCED_SPEED_1_2);
                break;
            case 4:
                regs.SetField(&reg0, MODS_FUSE_FEATURE_OVERRIDE_SM_SPEED_SELECT_IMLA3_REDUCED_SPEED_1_4);
                break;
            case 8:
                regs.SetField(&reg0, MODS_FUSE_FEATURE_OVERRIDE_SM_SPEED_SELECT_IMLA3_REDUCED_SPEED_1_8);
                break;
            case 16:
                regs.SetField(&reg0, MODS_FUSE_FEATURE_OVERRIDE_SM_SPEED_SELECT_IMLA3_REDUCED_SPEED_1_16);
                break;
            case 32:
                regs.SetField(&reg0, MODS_FUSE_FEATURE_OVERRIDE_SM_SPEED_SELECT_IMLA3_REDUCED_SPEED_1_32);
                break;
            // No 1/64 speed for IMLA3
            default:
                Printf(Tee::PriError,
                       "Issue-rate throttle 1/%d not supported for this unit!\n", throttle);
                return RC::BAD_COMMAND_LINE_ARGUMENT;
        }
    }
    else if (unit == "IMLA4")
    {
        regs.SetField(&reg1, MODS_FUSE_FEATURE_OVERRIDE_SM_SPEED_SELECT_1_IMLA4_OVERRIDE_TRUE);
        switch (throttle)
        {
            case 1:
                regs.SetField(&reg1, MODS_FUSE_FEATURE_OVERRIDE_SM_SPEED_SELECT_1_IMLA4_FULL_SPEED);
                break;
            case 2:
                regs.SetField(&reg1, MODS_FUSE_FEATURE_OVERRIDE_SM_SPEED_SELECT_1_IMLA4_REDUCED_SPEED_1_2);
                break;
            case 4:
                regs.SetField(&reg1, MODS_FUSE_FEATURE_OVERRIDE_SM_SPEED_SELECT_1_IMLA4_REDUCED_SPEED_1_4);
                break;
            case 8:
                regs.SetField(&reg1, MODS_FUSE_FEATURE_OVERRIDE_SM_SPEED_SELECT_1_IMLA4_REDUCED_SPEED_1_8);
                break;
            case 16:
                regs.SetField(&reg1, MODS_FUSE_FEATURE_OVERRIDE_SM_SPEED_SELECT_1_IMLA4_REDUCED_SPEED_1_16);
                break;
            case 32:
                regs.SetField(&reg1, MODS_FUSE_FEATURE_OVERRIDE_SM_SPEED_SELECT_1_IMLA4_REDUCED_SPEED_1_32);
                break;
            case 64:
                regs.SetField(&reg1, MODS_FUSE_FEATURE_OVERRIDE_SM_SPEED_SELECT_1_IMLA4_REDUCED_SPEED_1_64);
                break;
            default:
                Printf(Tee::PriError,
                       "Issue-rate throttle 1/%d not supported for this unit!\n", throttle);
                return RC::BAD_COMMAND_LINE_ARGUMENT;
        }
    }
    else if (unit == "DP")
    {
        regs.SetField(&reg0, MODS_FUSE_FEATURE_OVERRIDE_SM_SPEED_SELECT_DP_OVERRIDE_TRUE);
        switch (throttle)
        {
            case 1:
                regs.SetField(&reg0, MODS_FUSE_FEATURE_OVERRIDE_SM_SPEED_SELECT_DP_FULL_SPEED);
                break;
            case 2:
                regs.SetField(&reg0, MODS_FUSE_FEATURE_OVERRIDE_SM_SPEED_SELECT_DP_REDUCED_SPEED);
                break;
            default:
                Printf(Tee::PriError,
                       "Issue-rate throttle 1/%d not supported for this unit!\n", throttle);
                return RC::BAD_COMMAND_LINE_ARGUMENT;
        }
    }
    else
    {
        Printf(Tee::PriError,
               "'%s' is not a valid unit for this device!\n", unit.c_str());
        return RC::BAD_COMMAND_LINE_ARGUMENT;
    }

    // Write overrides
    regs.Write32(MODS_FUSE_FEATURE_OVERRIDE_SM_SPEED_SELECT, reg0);
    regs.Write32(MODS_FUSE_FEATURE_OVERRIDE_SM_SPEED_SELECT_1, reg1);

    return rc;
}


RC HopperGpuSubdevice::SetEccOverrideEnable(LW2080_ECC_UNITS eclwnit, bool bEnabled, bool bOverride)
{
    RegHal& regs = Regs();

    UINT32 eccOverride0 = regs.Read32(MODS_FUSE_FEATURE_OVERRIDE_ECC);
    UINT32 eccOverride1 = regs.Read32(MODS_FUSE_FEATURE_OVERRIDE_ECC_1);
    UINT32 eccOverride2 = regs.Read32(MODS_FUSE_FEATURE_OVERRIDE_ECC_2);

    const UINT32 enableVal = (bEnabled ? 1 : 0);
    const UINT32 overrideVal = (bOverride ? 1 : 0);

    switch (eclwnit)
    {
    case ECC_UNIT_LRF:
        regs.SetField(&eccOverride0, MODS_FUSE_FEATURE_OVERRIDE_ECC_SM_LRF, enableVal);
        regs.SetField(&eccOverride0, MODS_FUSE_FEATURE_OVERRIDE_ECC_SM_LRF_OVERRIDE, overrideVal);
        break;

    case ECC_UNIT_CBU:
        regs.SetField(&eccOverride0, MODS_FUSE_FEATURE_OVERRIDE_ECC_SM_CBU, enableVal);
        regs.SetField(&eccOverride0, MODS_FUSE_FEATURE_OVERRIDE_ECC_SM_CBU_OVERRIDE, overrideVal);
        break;

    case ECC_UNIT_L1DATA:
        regs.SetField(&eccOverride0, MODS_FUSE_FEATURE_OVERRIDE_ECC_SM_L1_DATA, enableVal);
        regs.SetField(&eccOverride0, MODS_FUSE_FEATURE_OVERRIDE_ECC_SM_L1_DATA_OVERRIDE, overrideVal);
        break;

    case ECC_UNIT_L1TAG:
        regs.SetField(&eccOverride0, MODS_FUSE_FEATURE_OVERRIDE_ECC_SM_L1_TAG, enableVal);
        regs.SetField(&eccOverride0, MODS_FUSE_FEATURE_OVERRIDE_ECC_SM_L1_TAG_OVERRIDE, overrideVal);
        regs.SetField(&eccOverride1, MODS_FUSE_FEATURE_OVERRIDE_ECC_1_SM_L1_MISS_LATENCY_FIFO, enableVal);
        regs.SetField(&eccOverride1, MODS_FUSE_FEATURE_OVERRIDE_ECC_1_SM_L1_MISS_LATENCY_FIFO_OVERRIDE, overrideVal);
        regs.SetField(&eccOverride1, MODS_FUSE_FEATURE_OVERRIDE_ECC_1_SM_PIXRF, enableVal);
        regs.SetField(&eccOverride1, MODS_FUSE_FEATURE_OVERRIDE_ECC_1_SM_PIXRF_OVERRIDE, overrideVal);
        break;

    case ECC_UNIT_SM_ICACHE:
        // TODO : L0 ICACHE will move to the new unit
        regs.SetField(&eccOverride1, MODS_FUSE_FEATURE_OVERRIDE_ECC_1_SM_L0_ICACHE, enableVal);
        regs.SetField(&eccOverride1, MODS_FUSE_FEATURE_OVERRIDE_ECC_1_SM_L0_ICACHE_OVERRIDE, overrideVal);
        regs.SetField(&eccOverride1, MODS_FUSE_FEATURE_OVERRIDE_ECC_1_SM_L1_ICACHE, enableVal);
        regs.SetField(&eccOverride1, MODS_FUSE_FEATURE_OVERRIDE_ECC_1_SM_L1_ICACHE_OVERRIDE, overrideVal);
        break;

    case ECC_UNIT_LTC:
        regs.SetField(&eccOverride0, MODS_FUSE_FEATURE_OVERRIDE_ECC_LTC, enableVal);
        regs.SetField(&eccOverride0, MODS_FUSE_FEATURE_OVERRIDE_ECC_LTC_OVERRIDE, overrideVal);
        regs.SetField(&eccOverride1, MODS_FUSE_FEATURE_OVERRIDE_ECC_1_LTC_L2_DCACHE_TAG, enableVal);
        regs.SetField(&eccOverride1, MODS_FUSE_FEATURE_OVERRIDE_ECC_1_LTC_L2_DCACHE_TAG_OVERRIDE, overrideVal);
        regs.SetField(&eccOverride2, MODS_FUSE_FEATURE_OVERRIDE_ECC_2_LTC_CBC, enableVal);
        regs.SetField(&eccOverride2, MODS_FUSE_FEATURE_OVERRIDE_ECC_2_LTC_CBC_OVERRIDE, overrideVal);
        break;

    case ECC_UNIT_DRAM:
        regs.SetField(&eccOverride0, MODS_FUSE_FEATURE_OVERRIDE_ECC_DRAM, enableVal);
        regs.SetField(&eccOverride0, MODS_FUSE_FEATURE_OVERRIDE_ECC_DRAM_OVERRIDE, overrideVal);
        break;

    case ECC_UNIT_GCC_L15:
        regs.SetField(&eccOverride1, MODS_FUSE_FEATURE_OVERRIDE_ECC_1_GCC_L1_5_CACHE, enableVal);
        regs.SetField(&eccOverride1, MODS_FUSE_FEATURE_OVERRIDE_ECC_1_GCC_L1_5_CACHE_OVERRIDE, overrideVal);
        break;

    case ECC_UNIT_GPCMMU:
        regs.SetField(&eccOverride1, MODS_FUSE_FEATURE_OVERRIDE_ECC_1_GPCMMU, enableVal);
        regs.SetField(&eccOverride1, MODS_FUSE_FEATURE_OVERRIDE_ECC_1_GPCMMU_OVERRIDE, overrideVal);
        break;

    case ECC_UNIT_HUBMMU_L2TLB:
    case ECC_UNIT_HUBMMU_HUBTLB:
    case ECC_UNIT_HUBMMU_FILLUNIT:
        regs.SetField(&eccOverride1, MODS_FUSE_FEATURE_OVERRIDE_ECC_1_HUBMMU, enableVal);
        regs.SetField(&eccOverride1, MODS_FUSE_FEATURE_OVERRIDE_ECC_1_HUBMMU_OVERRIDE, overrideVal);
        break;

    // TODO : tentitive new ECC unit for RM
    //case ECC_UNIT_SM_RAMS:
    //    regs.SetField(&eccOverride1, MODS_FUSE_FEATURE_OVERRIDE_ECC_1_SM_L0_ICACHE, enableVal);
    //    regs.SetField(&eccOverride1, MODS_FUSE_FEATURE_OVERRIDE_ECC_1_SM_L0_ICACHE_OVERRIDE, overrideVal);
    //    regs.SetField(&eccOverride2, MODS_FUSE_FEATURE_OVERRIDE_ECC_2_SM_URF, enableVal);
    //    regs.SetField(&eccOverride2, MODS_FUSE_FEATURE_OVERRIDE_ECC_2_SM_URF_OVERRIDE, overrideVal);
    // TODO(stewarts): Falcons are not yet implemented by RM

    // Units not available on Turing
    case ECC_UNIT_L1:
    case ECC_UNIT_SHM:
    case ECC_UNIT_TEX:
        return RC::UNSUPPORTED_HARDWARE_FEATURE;

    default:
        Printf(Tee::PriError, "Unknown ECC unit (LW2080_ECC_UNITS %u)\n",
               static_cast<UINT32>(eclwnit));
        MASSERT(!"Unknown ECC unit");
        return RC::SOFTWARE_ERROR;
    }

    regs.Write32(MODS_FUSE_FEATURE_OVERRIDE_ECC, eccOverride0);
    regs.Write32(MODS_FUSE_FEATURE_OVERRIDE_ECC_1, eccOverride1);
    regs.Write32(MODS_FUSE_FEATURE_OVERRIDE_ECC_2, eccOverride2);

    return RC::OK;
}

//-------------------------------------------------------------------------------
RC HopperGpuSubdevice::GetAndClearL2EccCounts
(
    UINT32 ltc,
    UINT32 slice,
    L2EccCounts * pL2EccCounts
)
{
    MASSERT(pL2EccCounts);

    if ((ltc >= GetFB()->GetLtcCount()) || (slice >= GetFB()->GetSlicesPerLtc(ltc)))
    {
        MASSERT(!"Invalid LTC or SLICE Slice");
        Printf(Tee::PriError, "LTC %u or Slice %u out of range\n",
               ltc, slice);
        return RC::BAD_PARAMETER;
    }

    // Read error count
    RegHal &regs = Regs();
    UINT32 errCountReg = RegRd32(
        regs.LookupAddress(MODS_PLTCG_LTC0_LTS0_L2_CACHE_ECC_CORRECTED_ERR_COUNT) +
        ltc * regs.LookupAddress(MODS_LTC_PRI_STRIDE) +
        slice * regs.LookupAddress(MODS_LTS_PRI_STRIDE));

    // Reset error count to 0
    RegWr32(
        regs.LookupAddress(MODS_PLTCG_LTC0_LTS0_L2_CACHE_ECC_CORRECTED_ERR_COUNT) +
        ltc * regs.LookupAddress(MODS_LTC_PRI_STRIDE) +
        slice * regs.LookupAddress(MODS_LTS_PRI_STRIDE),
        0);
    pL2EccCounts->correctedTotal =
        regs.GetField(errCountReg,
                      MODS_PLTCG_LTC0_LTS0_L2_CACHE_ECC_CORRECTED_ERR_COUNT_TOTAL);
    pL2EccCounts->correctedUnique =
        regs.GetField(errCountReg,
                      MODS_PLTCG_LTC0_LTS0_L2_CACHE_ECC_CORRECTED_ERR_COUNT_UNIQUE);

    // Read error count
    errCountReg = RegRd32(
        regs.LookupAddress(MODS_PLTCG_LTC0_LTS0_L2_CACHE_ECC_UNCORRECTED_ERR_COUNT) +
        ltc * regs.LookupAddress(MODS_LTC_PRI_STRIDE) +
        slice * regs.LookupAddress(MODS_LTS_PRI_STRIDE));

    // Reset error count to 0
    RegWr32(
        regs.LookupAddress(MODS_PLTCG_LTC0_LTS0_L2_CACHE_ECC_UNCORRECTED_ERR_COUNT) +
        ltc * regs.LookupAddress(MODS_LTC_PRI_STRIDE) +
        slice * regs.LookupAddress(MODS_LTS_PRI_STRIDE),
        0);
    pL2EccCounts->uncorrectedTotal =
        regs.GetField(errCountReg,
                      MODS_PLTCG_LTC0_LTS0_L2_CACHE_ECC_UNCORRECTED_ERR_COUNT_TOTAL);
    pL2EccCounts->uncorrectedUnique =
        regs.GetField(errCountReg,
                      MODS_PLTCG_LTC0_LTS0_L2_CACHE_ECC_UNCORRECTED_ERR_COUNT_UNIQUE);

    return RC::OK;
}

//-------------------------------------------------------------------------------
RC HopperGpuSubdevice::SetL2EccCounts
(
    UINT32 ltc,
    UINT32 slice,
    const L2EccCounts & l2EccCounts
)
{
    if ((ltc >= GetFB()->GetLtcCount()) || (slice >= GetFB()->GetSlicesPerLtc(ltc)))
    {
        MASSERT(!"Invalid LTC or SLICE Slice");
        Printf(Tee::PriError, "LTC %u or Slice %u out of range\n",
               ltc, slice);
        return RC::BAD_PARAMETER;
    }

    UINT32 errCountReg = 0;
    RegHal &regs = Regs();
    errCountReg =
        regs.SetField(MODS_PLTCG_LTC0_LTS0_L2_CACHE_ECC_CORRECTED_ERR_COUNT_TOTAL,
                      l2EccCounts.correctedTotal);
    regs.SetField(&errCountReg,
                  MODS_PLTCG_LTC0_LTS0_L2_CACHE_ECC_CORRECTED_ERR_COUNT_UNIQUE,
                  l2EccCounts.correctedUnique);
    RegWr32(
        regs.LookupAddress(MODS_PLTCG_LTC0_LTS0_L2_CACHE_ECC_CORRECTED_ERR_COUNT) +
        ltc * regs.LookupAddress(MODS_LTC_PRI_STRIDE) +
        slice * regs.LookupAddress(MODS_LTS_PRI_STRIDE),
        errCountReg);

    errCountReg =
        regs.SetField(MODS_PLTCG_LTC0_LTS0_L2_CACHE_ECC_UNCORRECTED_ERR_COUNT_TOTAL,
                      l2EccCounts.uncorrectedTotal);
    regs.SetField(&errCountReg,
                  MODS_PLTCG_LTC0_LTS0_L2_CACHE_ECC_UNCORRECTED_ERR_COUNT_UNIQUE,
                  l2EccCounts.uncorrectedUnique);
    RegWr32(
        regs.LookupAddress(MODS_PLTCG_LTC0_LTS0_L2_CACHE_ECC_UNCORRECTED_ERR_COUNT) +
        ltc * regs.LookupAddress(MODS_LTC_PRI_STRIDE) +
        slice * regs.LookupAddress(MODS_LTS_PRI_STRIDE),
        errCountReg);
    return RC::OK;
}

//-------------------------------------------------------------------------------
RC HopperGpuSubdevice::GetAndClearL2EccInterrupts
(
    UINT32 ltc,
    UINT32 slice,
    bool *pbCorrInterrupt,
    bool *pbUncorrInterrupt
)
{
    MASSERT(pbCorrInterrupt);
    MASSERT(pbUncorrInterrupt);

    if ((ltc >= GetFB()->GetLtcCount()) || (slice >= GetFB()->GetSlicesPerLtc(ltc)))
    {
        MASSERT(!"Invalid LTC or SLICE Slice");
        Printf(Tee::PriError, "LTC %u or Slice %u out of range\n",
               ltc, slice);
        return RC::BAD_PARAMETER;
    }

    RegHal &regs = Regs();
    const UINT32 regAddr =
        regs.LookupAddress(MODS_PLTCG_LTC0_LTS0_L2_CACHE_ECC_STATUS) +
        ltc * regs.LookupAddress(MODS_LTC_PRI_STRIDE) +
        slice * regs.LookupAddress(MODS_LTS_PRI_STRIDE);
    UINT32 interruptReg = RegRd32(regAddr);

    *pbCorrInterrupt     =
        regs.TestField(interruptReg,
                       MODS_PLTCG_LTC0_LTS0_L2_CACHE_ECC_STATUS_CORRECTED_ERR_DSTG_PENDING) ||
        regs.TestField(interruptReg,
                       MODS_PLTCG_LTC0_LTS0_L2_CACHE_ECC_STATUS_CORRECTED_ERR_RSTG_PENDING) ||
        regs.TestField(interruptReg,
                       MODS_PLTCG_LTC0_LTS0_L2_CACHE_ECC_STATUS_CORRECTED_ERR_TSTG_PENDING);
    *pbUncorrInterrupt   =
        regs.TestField(interruptReg,
                       MODS_PLTCG_LTC0_LTS0_L2_CACHE_ECC_STATUS_UNCORRECTED_ERR_DSTG_PENDING) ||
        regs.TestField(interruptReg,
                       MODS_PLTCG_LTC0_LTS0_L2_CACHE_ECC_STATUS_UNCORRECTED_ERR_RSTG_PENDING) ||
        regs.TestField(interruptReg,
                       MODS_PLTCG_LTC0_LTS0_L2_CACHE_ECC_STATUS_UNCORRECTED_ERR_TSTG_PENDING);

    // Reset ECC interrupts
    regs.Write32(MODS_PLTCG_LTC0_LTS0_L2_CACHE_ECC_STATUS_RESET_CLEAR);

    return RC::OK;
}

//-----------------------------------------------------------------------------
RC HopperGpuSubdevice::GetHbmLinkRepairRegister
(
    const LaneError& laneError,
    UINT32* pOutRegAddr
) const
{
    MASSERT(pOutRegAddr);

    constexpr bool withDwordSwizzle = false;
    return GpuSubdevice::GetHbmLinkRepairRegister(laneError, pOutRegAddr, withDwordSwizzle);
}

//-----------------------------------------------------------------------------
RC HopperGpuSubdevice::EnableHwDevice(GpuSubdevice::HwDevType hwType, bool bEnable)
{
    RegHal &regs = Regs();

    // Most engine / device resets are moving into the engines itself, and also
    // may require some other sequencing before asserting the resets
    // Only adding support for the ones we need lwrrently in MODS
    switch (hwType)
    {
        case GpuSubdevice::HwDevType::HDT_GRAPHICS:
            regs.Write32(MODS_PGRAPH_PRI_FECS_ENGINE_RESET_CTL_RESET, bEnable ? 1 : 0);
            break;
        case GpuSubdevice::HwDevType::HDT_IOCTRL:
            Printf(Tee::PriLow, "IOCTRL links should be enabled by VBIOS by default\n");
            if (bEnable)
            {
                return RC::OK;
            }
            else
            {
                return RC::UNSUPPORTED_FUNCTION;
            }
            break;
        default:
            return RC::UNSUPPORTED_FUNCTION;
    }

    return RC::OK;
}

//-------------------------------------------------------------------------------
GpuPerfmon * HopperGpuSubdevice::CreatePerfmon(void)
{
    return NULL;
}

//-------------------------------------------------------------------------------
RC HopperGpuSubdevice::CheckHbmIeee1500RegAccess(const HbmSite& hbmSite) const
{
    RC rc;

    UINT32 masterFbpaNum = 0;
    CHECK_RC(GetHBMSiteMasterFbpaNumber(hbmSite.Number(), &masterFbpaNum));
    const RegHal& regs = Regs();

    // INSTR, MODE, and STATUS have the same PLM
    MASSERT((regs.LookupAddress(MODS_PFB_FBPA_0_FBIO_HBM_TEST_I1500_INSTR__PRIV_LEVEL_MASK, masterFbpaNum)
             == regs.LookupAddress(MODS_PFB_FBPA_0_FBIO_HBM_TEST_I1500_MODE__PRIV_LEVEL_MASK, masterFbpaNum))
            && (regs.LookupAddress(MODS_PFB_FBPA_0_FBIO_HBM_TEST_I1500_INSTR__PRIV_LEVEL_MASK, masterFbpaNum)
                == regs.LookupAddress(MODS_PFB_FBPA_0_FBIO_HBM_TEST_I1500_STATUS__PRIV_LEVEL_MASK, masterFbpaNum)));

    // Ensure the PLMs map as expected
    MASSERT(regs.LookupAddress(MODS_PFB_FBPA_0_FBIO_SERIAL_PRIV_LEVEL_MASK, masterFbpaNum)
            == regs.LookupAddress(MODS_PFB_FBPA_0_FBIO_HBM_TEST_MACRO__PRIV_LEVEL_MASK, masterFbpaNum));
    MASSERT(regs.LookupAddress(MODS_PFB_FBPA_0_FBIO_I1500_PRIV_LEVEL_MASK, masterFbpaNum)
            == regs.LookupAddress(MODS_PFB_FBPA_0_FBIO_HBM_TEST_I1500_INSTR__PRIV_LEVEL_MASK, masterFbpaNum));
    MASSERT(regs.LookupAddress(MODS_PFB_FBPA_0_FBIO_I1500_DATA_PRIV_LEVEL_MASK, masterFbpaNum)
            == regs.LookupAddress(MODS_PFB_FBPA_0_FBIO_HBM_TEST_I1500_DATA__PRIV_LEVEL_MASK, masterFbpaNum));

    StickyRC firstRc;
    struct PermsRegVal
    {
        const char* regDesc;
        ModsGpuRegValue read;
        ModsGpuRegValue write;
    };
    for (const PermsRegVal& permsReg
             : { PermsRegVal{"IEEE1500 HBM INSTR/MODE/STATUS",
                             MODS_PFB_FBPA_0_FBIO_I1500_PRIV_LEVEL_MASK_READ_PROTECTION_ALL_LEVELS_ENABLED,
                             MODS_PFB_FBPA_0_FBIO_I1500_PRIV_LEVEL_MASK_WRITE_PROTECTION_ALL_LEVELS_ENABLED},
                 PermsRegVal{"IEEE1500 HBM DATA",
                             MODS_PFB_FBPA_0_FBIO_I1500_DATA_PRIV_LEVEL_MASK_READ_PROTECTION_ALL_LEVELS_ENABLED,
                             MODS_PFB_FBPA_0_FBIO_I1500_DATA_PRIV_LEVEL_MASK_WRITE_PROTECTION_ALL_LEVELS_ENABLED},
                 PermsRegVal{"IEEE1500 HBM MACRO",
                             MODS_PFB_FBPA_0_FBIO_SERIAL_PRIV_LEVEL_MASK_READ_PROTECTION_ALL_LEVELS_ENABLED,
                             MODS_PFB_FBPA_0_FBIO_SERIAL_PRIV_LEVEL_MASK_WRITE_PROTECTION_ALL_LEVELS_ENABLED} })
    {
        const bool readPerm = regs.Test32(permsReg.read, masterFbpaNum);
        const bool writePerm = regs.Test32(permsReg.write, masterFbpaNum);

        if (!readPerm || !writePerm)
        {
            string s = Utility::StrPrintf("HBM site %u, FBPA %u: Insufficient permissions - %s register(s)",
                                          hbmSite.Number(), masterFbpaNum, permsReg.regDesc);
            if (Utility::GetSelwrityUnlockLevel() >= Utility::SUL_LWIDIA_NETWORK)
            {
                s += ", missing: ";
                if (!readPerm)
                {
                    s += "read ";
                }

                if (!writePerm)
                {
                    s += "write ";
                }

            }

            Printf(Tee::PriError, "%s\n", s.c_str());

            firstRc = RC::PRIV_LEVEL_VIOLATION;
        }
    }

    return firstRc;
}

//-----------------------------------------------------------------------------
// Time out will depend on RM's memops
//-----------------------------------------------------------------------------
/* virtual */ RC HopperGpuSubdevice::FbFlush(FLOAT64 TimeOutMs)
{
	RC rc;
	LwRmPtr pLwRm;
	LW2080_CTRL_FB_FLUSH_GPU_CACHE_PARAMS flushParams = {};

	flushParams.flags = DRF_DEF(2080, _CTRL_FB_FLUSH_GPU_CACHE_FLAGS, _APERTURE, _VIDEO_MEMORY) |
		DRF_DEF(2080, _CTRL_FB_FLUSH_GPU_CACHE_FLAGS, _FB_FLUSH, _YES);

	//FB flush timeout value will be overridden by RM
	CHECK_RC(pLwRm->Control(pLwRm->GetSubdeviceHandle(this),
		LW2080_CTRL_CMD_FB_FLUSH_GPU_CACHE,
		&flushParams, sizeof(flushParams)));

	return rc;
}

//-----------------------------------------------------------------------------
RC HopperGpuSubdevice::GetIsHulkFlashed(bool* pHulkFlashed)
{
    MASSERT(pHulkFlashed);
    UINT32 reg = Regs().Read32(MODS_PBUS_SW_SCRATCH, 21);
    *pHulkFlashed = reg != 0;
    return OK;
}

//-----------------------------------------------------------------------------
bool HopperGpuSubdevice::IsGpcGraphicsCapable(UINT32 hwGpcNum) const
{
    UINT32 virtualGpcNum = 0;
    if (HwGpcToVirtualGpc(hwGpcNum, &virtualGpcNum) != RC::OK)
        return false;

    return Regs().Test32(
        MODS_PGRAPH_PRI_GPCx_GPCCS_FS_GPC_GRAPHICS_CAPABLE_YES, virtualGpcNum);
}

//-----------------------------------------------------------------------------
UINT32 HopperGpuSubdevice::GetGraphicsTpcMaskOnGpc(UINT32 hwGpcNum) const
{
    INT32 fullGraphicsMask = GetFullGraphicsTpcMask(hwGpcNum);
    UINT32 tpcMask = GetFsImpl()->TpcMask(hwGpcNum);
    return (fullGraphicsMask & tpcMask);
}

//-----------------------------------------------------------------------------
void HopperGpuSubdevice::PostRmShutdown()
{
    const bool bSupportsHoldoff = !IsSOC() &&
                                 (Platform::GetSimulationMode() == Platform::Hardware) &&
                                 Platform::HasClientSideResman() &&
                                 HasFeature(Device::Feature::GPUSUB_SUPPORTS_GFW_HOLDOFF);
    const bool bSupportsFsp = (HasFeature(Device::Feature::GPUSUB_SUPPORTS_FSP) &&
                              (Platform::GetSimulationMode() == Platform::Hardware));
    const bool bNeedsFsChanges = GetFsImpl()->GetFloorsweepingAffected() &&
                                 !GetNamedPropertyValue("NoFsRestore");

    // Make sure we don't leave holdoff bit set on error
    DEFER
    {
        if (bSupportsHoldoff && GetGFWBootHoldoff())
        {
            SetGFWBootHoldoff(false);
        }
    };

    if (bNeedsFsChanges)
    {
        // Set the holdoff bit and reset if required
        if (bSupportsHoldoff)
        {
            SetGFWBootHoldoff(true);
            GetFsImpl()->SwReset(false);

            if (IsGFWBootStarted())
            {
                // Restore FS settings
                GetFsImpl()->PostRmShutdown(true);
                GetFsImpl()->SetVbiosControlsFloorsweep(false);

                // Release holdoff, letting the vbios ucode run devinit
                SetGFWBootHoldoff(false);
                PollGFWBootComplete(Tee::PriLow);

                // Pass FS control to VBIOS for next reset
                GetFsImpl()->SetVbiosControlsFloorsweep(true);
            }
            else
            {
                // Devinit didn't run after FLR, don't use holdoff
                SetGFWBootHoldoff(false);
                GetFsImpl()->PostRmShutdown(false);
            }
        }
        else if (bSupportsFsp)
        {
            // Restore FS settings
            GetFsImpl()->PostRmShutdown(true);

            // Issue reset, let FS complete
            if (HasBug(3472315))
            {
               HotReset(TestDevice::FundamentalResetOptions::Disable);
            }
            else
            {
               FLReset();
            }
            PollGFWBootComplete(Tee::PriLow);
        }
        else
        {
            GetFsImpl()->PostRmShutdown(false);
        }
    }

    RestoreEarlyRegs();

    if (m_SavedBankSwizzle)
    {
        Regs().Write32(MODS_PFB_FBPA_BANK_SWIZZLE, m_OrigBankSwizzle);
    }
}

//-----------------------------------------------------------------------------
RC HopperGpuSubdevice::GetPoisonEnabled(bool* pEnabled)
{
    RC rc;
    RegHal& regs = GpuSubdevice::Regs();

    // Read the current state of the poison enablement
    *pEnabled = regs.Test32(MODS_FUSE_FEATURE_READOUT_3_MEMQUAL_GLOBAL_POISON_EN);

    return rc;
}

//-----------------------------------------------------------------------------
RC HopperGpuSubdevice::OverridePoisolwal(bool bEnable)
{
    RegHal& regs = GpuSubdevice::Regs();
    bool bIsEnabled;
    UINT32 regVal;

    // Read the current state of the poison enablement
    bIsEnabled = regs.Test32(MODS_FUSE_FEATURE_READOUT_3_MEMQUAL_GLOBAL_POISON_EN);

    if (bEnable ^ bIsEnabled)
    {
        // Make sure that we have sufficient privilege to override the global poison fuse.
        if (regs.Test32(MODS_FUSE_FEATURE_OVERRIDE_MEMQUAL_PRIV_LEVEL_MASK_WRITE_PROTECTION_LEVEL0_DISABLE))
        {
            Printf(Tee::PriError, "Insuffient permissions to override poison value\n");
            return RC::PRIV_LEVEL_VIOLATION;
        }

        // Override the fuse value
        regVal = regs.Read32(MODS_FUSE_FEATURE_OVERRIDE_MEMQUAL);
        if (bEnable)
        {
            regs.SetField(&regVal, MODS_FUSE_FEATURE_OVERRIDE_MEMQUAL_GLOBAL_POISON_EN);
        }
        else
        {
            regs.SetField(&regVal, MODS_FUSE_FEATURE_OVERRIDE_MEMQUAL_GLOBAL_POISON_DIS);
        }

        // Enable override
        regs.SetField(&regVal, MODS_FUSE_FEATURE_OVERRIDE_MEMQUAL_GLOBAL_POISON_OVERRIDE_TRUE);

        // Write to register
        regs.Write32(MODS_FUSE_FEATURE_OVERRIDE_MEMQUAL, regVal);
    }
    else
    {
        Printf(Tee::PriWarn, "Requested poison override already matches chip value\n");
    }

    return OK;
}

//-----------------------------------------------------------------------------
RC HopperGpuSubdevice::OverrideSmInternalErrorContainment(bool bEnable)
{
    RegHal& regs = GpuSubdevice::Regs();

    // Read the current state of the SM Intenral Error Containment enablement
    bool bIsEnabled = regs.Test32(MODS_FUSE_FEATURE_READOUT_5_SM_INTERNAL_ERROR_CONTAINMENT_ENABLED);

    if (bEnable ^ bIsEnabled)
    {
        // Make sure that we have sufficient privilege to override the SM internal error containment fuse.
        if (regs.Test32(MODS_FUSE_FEATURE_OVERRIDE_MEMQUAL_PRIV_LEVEL_MASK_WRITE_PROTECTION_LEVEL0_DISABLE))
        {
            Printf(Tee::PriError, "Insuffient permissions to override SM internal error containment value\n");
            return RC::PRIV_LEVEL_VIOLATION;
        }

        // Override the fuse value
        UINT32 regVal = regs.Read32(MODS_FUSE_FEATURE_OVERRIDE_SM_INTERNAL_ERROR_CONTAINMENT);
        if (bEnable)
        {
            regs.SetField(&regVal, MODS_FUSE_FEATURE_OVERRIDE_SM_INTERNAL_ERROR_CONTAINMENT_DATA_ENABLED);
        }
        else
        {
            regs.SetField(&regVal, MODS_FUSE_FEATURE_OVERRIDE_SM_INTERNAL_ERROR_CONTAINMENT_DATA_DISABLED);
        }

        // Enable override
        regs.SetField(&regVal, MODS_FUSE_FEATURE_OVERRIDE_SM_INTERNAL_ERROR_CONTAINMENT_OVERRIDE_TRUE);

        // Write to register
        regs.Write32(MODS_FUSE_FEATURE_OVERRIDE_SM_INTERNAL_ERROR_CONTAINMENT, regVal);
    }
    else
    {
        Printf(Tee::PriWarn, "SM internal error containment override already matches chip value\n");
    }

    return OK;
}

//-----------------------------------------------------------------------------
RC HopperGpuSubdevice::OverrideGccCtxswPoisonContainment(bool bEnable)
{
    RegHal& regs = GpuSubdevice::Regs();

    // Read the current state of the GCC Poison Containment enablement
    bool bIsGccEnabled = regs.Test32(MODS_FUSE_FEATURE_READOUT_5_GCC_DATA_ERROR_CONTAINMENT_ENABLED);

    if (bEnable ^ bIsGccEnabled)
    {
        // Make sure that we have sufficient privilege to override the GCC poison containment fuse.
        if (regs.Test32(MODS_FUSE_FEATURE_OVERRIDE_MEMQUAL_PRIV_LEVEL_MASK_WRITE_PROTECTION_LEVEL0_DISABLE))
        {
            Printf(Tee::PriError, "Insuffient permissions to override GCC poison containment value\n");
            return RC::PRIV_LEVEL_VIOLATION;
        }

        // Override the fuse value
        UINT32 regVal = regs.Read32(MODS_FUSE_FEATURE_OVERRIDE_GCC_DATA_ERROR_CONTAINMENT);
        if (bEnable)
        {
            regs.SetField(&regVal, MODS_FUSE_FEATURE_OVERRIDE_GCC_DATA_ERROR_CONTAINMENT_DATA_ENABLED);
        }
        else
        {
            regs.SetField(&regVal, MODS_FUSE_FEATURE_OVERRIDE_GCC_DATA_ERROR_CONTAINMENT_DATA_DISABLED);
        }

        // Enable override
        regs.SetField(&regVal, MODS_FUSE_FEATURE_OVERRIDE_GCC_DATA_ERROR_CONTAINMENT_OVERRIDE_TRUE);

        // Write to register
        regs.Write32(MODS_FUSE_FEATURE_OVERRIDE_GCC_DATA_ERROR_CONTAINMENT, regVal);
    }
    else
    {
        Printf(Tee::PriWarn, "GCC poison containment override already matches chip value\n");
    }


    // Read the current state of the Ctxsw Poison Containment enablement
    bool bIsCtxswEnabled = regs.Test32(MODS_FUSE_FEATURE_READOUT_5_CTXSW_POISON_ERROR_CONTAINMENT_ENABLED);

    if (bEnable ^ bIsCtxswEnabled)
    {
        // Make sure that we have sufficient privilege to override the Ctxsw poison containment fuse.
        if (regs.Test32(MODS_FUSE_FEATURE_OVERRIDE_MEMQUAL_PRIV_LEVEL_MASK_WRITE_PROTECTION_LEVEL0_DISABLE))
        {
            Printf(Tee::PriError, "Insuffient permissions to override Ctxsw poison containment value\n");
            return RC::PRIV_LEVEL_VIOLATION;
        }

        // Override the fuse value
        UINT32 regVal = regs.Read32(MODS_FUSE_FEATURE_OVERRIDE_CTXSW_POISON_ERROR_CONTAINMENT);
        if (bEnable)
        {
            regs.SetField(&regVal, MODS_FUSE_FEATURE_OVERRIDE_CTXSW_POISON_ERROR_CONTAINMENT_DATA_ENABLED);
        }
        else
        {
            regs.SetField(&regVal, MODS_FUSE_FEATURE_OVERRIDE_CTXSW_POISON_ERROR_CONTAINMENT_DATA_DISABLED);
        }

        // Enable override
        regs.SetField(&regVal, MODS_FUSE_FEATURE_OVERRIDE_CTXSW_POISON_ERROR_CONTAINMENT_OVERRIDE_TRUE);

        // Write to register
        regs.Write32(MODS_FUSE_FEATURE_OVERRIDE_CTXSW_POISON_ERROR_CONTAINMENT, regVal);
    }
    else
    {
        Printf(Tee::PriWarn, "Ctxsw poison containment override already matches chip value\n");
    }

    return OK;
}

UINT32 HopperGpuSubdevice::GetHsHubPceMask() const
{
    return GetFsImpl()->PceMask() & Regs().Read32(MODS_CE_HSH_PCE_MASK);
}

UINT32 HopperGpuSubdevice::GetFbHubPceMask() const
{
    return GetFsImpl()->PceMask() & ~Regs().Read32(MODS_CE_HSH_PCE_MASK);
}


//-----------------------------------------------------------------------------------
RC HopperGpuSubdevice::GetPerGpcClkMonFaults(UINT08 partitionIndex, UINT32 *pFaultMask)
{
    RC rc;
    if (!Regs().IsSupported(MODS_PTRIM_GPC_FMON_FAULT_STATUS_GPCCLK_DIV2, partitionIndex))
    {
        Printf(Tee::PriError, "Clock Monitoring is unsupported for gpcclk with partition = %d \n",
               partitionIndex);
        return RC::UNSUPPORTED_HARDWARE_FEATURE;
    }
    // Check fault status per GPC partition manually because RM doesn't
    // support this in the control call
    if (Regs().Test32(
        MODS_PTRIM_GPC_FMON_FAULT_STATUS_GPCCLK_DIV2_GPCCLK_DIV2_FMON_FAULT_OUT_STATUS_TRUE,
        partitionIndex))
    {
        UINT08 dcFault =  0;
        if (Regs().Test32(
            MODS_PTRIM_GPC_FMON_FAULT_STATUS_GPCCLK_DIV2_GPCCLK_DIV2_FMON_DC_FAULT_TRUE,
            partitionIndex))
        {
            dcFault =  LW2080_CTRL_CLK_DOMAIN_CLK_MON_STATUS_MASK_DC_FAULT;
        }
        UINT08 lowerThresHighFault = 0;
        if (Regs().Test32(
            MODS_PTRIM_GPC_FMON_FAULT_STATUS_GPCCLK_DIV2_GPCCLK_DIV2_FMON_COUNT_LOWER_THRESH_HIGH_FAULT_TRUE,
            partitionIndex))
        {
            lowerThresHighFault = LW2080_CTRL_CLK_DOMAIN_CLK_MON_STATUS_MASK_LOWER_THRESH_FAULT;
        }
        UINT08 higherThresHighFault = 0;
        if (Regs().Test32(
            MODS_PTRIM_GPC_FMON_FAULT_STATUS_GPCCLK_DIV2_GPCCLK_DIV2_FMON_COUNT_HIGHER_THRESH_HIGH_FAULT_TRUE,
            partitionIndex))
        {
            higherThresHighFault = LW2080_CTRL_CLK_DOMAIN_CLK_MON_STATUS_MASK_HIGHER_THRESH_FAULT;
        }
        UINT08 overflowError = 0;
        if (Regs().Test32(
            MODS_PTRIM_GPC_FMON_FAULT_STATUS_GPCCLK_DIV2_GPCCLK_DIV2_FMON_OVERFLOW_ERROR_TRUE,
            partitionIndex))
        {
            overflowError = LW2080_CTRL_CLK_DOMAIN_CLK_MON_STATUS_MASK_OVERFLOW_ERROR;
        }
        *pFaultMask = dcFault | lowerThresHighFault | higherThresHighFault | overflowError;
    }
    return rc;
}

//-----------------------------------------------------------------------------------
RC HopperGpuSubdevice::GetPerFbClkMonFaults(UINT08 partitionIndex, UINT32 *pFaultMask)
{
    RC rc;
    if (!Regs().IsSupported(MODS_PTRIM_FBPA_FMON_FAULT_STATUS_DRAMCLK, partitionIndex))
    {
        Printf(Tee::PriError, "Clock Monitoring is unsupported for dramclk with partition = %d \n",
        partitionIndex);
        return RC::UNSUPPORTED_HARDWARE_FEATURE;
    }

    // Check fault status per FB partition manually because RM doesn't support
    // this in the control call
    if (Regs().Test32(MODS_PTRIM_FBPA_FMON_FAULT_STATUS_DRAMCLK_DRAMCLK_FMON_FAULT_OUT_STATUS_TRUE,
        partitionIndex))
    {
        UINT08 dcFault =  0;
        if (Regs().Test32(
            MODS_PTRIM_FBPA_FMON_FAULT_STATUS_DRAMCLK_DRAMCLK_FMON_DC_FAULT_TRUE,
            partitionIndex))
        {
            dcFault =  LW2080_CTRL_CLK_DOMAIN_CLK_MON_STATUS_MASK_DC_FAULT;
        }
        UINT08 lowerThresHighFault = 0;
        if (Regs().Test32(
            MODS_PTRIM_FBPA_FMON_FAULT_STATUS_DRAMCLK_DRAMCLK_FMON_COUNT_LOWER_THRESH_HIGH_FAULT_TRUE,
            partitionIndex))
        {
            lowerThresHighFault = LW2080_CTRL_CLK_DOMAIN_CLK_MON_STATUS_MASK_LOWER_THRESH_FAULT;
        }
        UINT08 higherThresHighFault = 0;
        if (Regs().Test32(
            MODS_PTRIM_FBPA_FMON_FAULT_STATUS_DRAMCLK_DRAMCLK_FMON_COUNT_HIGHER_THRESH_HIGH_FAULT_TRUE,
            partitionIndex))
        {
            higherThresHighFault = LW2080_CTRL_CLK_DOMAIN_CLK_MON_STATUS_MASK_HIGHER_THRESH_FAULT;
        }
        UINT08 overflowError = 0;
        if (Regs().Test32(
            MODS_PTRIM_FBPA_FMON_FAULT_STATUS_DRAMCLK_DRAMCLK_FMON_OVERFLOW_ERROR_TRUE,
            partitionIndex))
        {
            overflowError = LW2080_CTRL_CLK_DOMAIN_CLK_MON_STATUS_MASK_OVERFLOW_ERROR;
        }
        *pFaultMask = dcFault | lowerThresHighFault | higherThresHighFault | overflowError;
    }
    return rc;
}
bool HopperGpuSubdevice::IsSupportedNonHalClockMonitor(Gpu::ClkDomain clkDomain)
{
    // These clocks are not supported by RM for clock monitoring
    switch (clkDomain)
    {
        case Gpu::ClkPwr:
            return Regs().IsSupported(MODS_PTRIM_SYS_FMON_FAULT_STATUS_PWRCLK);
        case Gpu::ClkPexRef:
            return Regs().IsSupported(MODS_PTRIM_SYS_FMON_FAULT_STATUS_PEX_REFCLK);
        case Gpu::ClkLwlCom:
            return Regs().IsSupported(MODS_PTRIM_SYS_FMON_FAULT_STATUS_LWL_COMMON_CLK);
        case Gpu::ClkX:
            return Regs().IsSupported(MODS_PTRIM_SYS_FMON_FAULT_STATUS_XCLK);
        case Gpu::ClkUtilS:
            return Regs().IsSupported(MODS_PTRIM_SYS_FMON_FAULT_STATUS_UTILSCLK);
        case Gpu::ClkM:
            return Regs().IsSupported(MODS_PTRIM_FBPA_FMON_FAULT_STATUS_DRAMCLK);
        default:
        {
            MASSERT("Unknown Clock Domain \n");
            return false;
        }
    }
}
//-----------------------------------------------------------------------------------
RC HopperGpuSubdevice::GetNonHalClkMonFaultMask(Gpu::ClkDomain clkDom, UINT32 *pFaultMask)
{
    RC rc;
    if (!IsSupportedNonHalClockMonitor(clkDom))
    {
        Printf(Tee::PriError, "Clock Fault Monitoring is not supported for clk = %s \n",
               PerfUtil::ClkDomainToStr(clkDom));
        return RC::UNSUPPORTED_HARDWARE_FEATURE;
    }

    // Only if the FAULT_OUT STATUS register is true, populate the faultMask value
    // Find out the FaultMask for one of these clocks because RM doesn't support
    // clock monitoring for these clocks
    *pFaultMask  = 0;
    switch (clkDom)
    {
        case Gpu::ClkUtilS:
        {
            if (Regs().Test32(
                MODS_PTRIM_SYS_FMON_FAULT_STATUS_UTILSCLK_UTILSCLK_FMON_FAULT_OUT_STATUS_TRUE))
            {
                CHECK_RC(GetFaultMask(
                    MODS_PTRIM_SYS_FMON_FAULT_STATUS_UTILSCLK_UTILSCLK_FMON_DC_FAULT_TRUE,
                    MODS_PTRIM_SYS_FMON_FAULT_STATUS_UTILSCLK_UTILSCLK_FMON_COUNT_LOWER_THRESH_HIGH_FAULT_TRUE,
                    MODS_PTRIM_SYS_FMON_FAULT_STATUS_UTILSCLK_UTILSCLK_FMON_COUNT_HIGHER_THRESH_HIGH_FAULT_TRUE,
                    MODS_PTRIM_SYS_FMON_FAULT_STATUS_UTILSCLK_UTILSCLK_FMON_OVERFLOW_ERROR_TRUE,
                    pFaultMask));
            }
            break;
        }
        case Gpu::ClkPwr:
        {
            if (Regs().Test32(
                MODS_PTRIM_SYS_FMON_FAULT_STATUS_PWRCLK_PWRCLK_FMON_FAULT_OUT_STATUS_TRUE))
            {
                CHECK_RC(GetFaultMask(
                    MODS_PTRIM_SYS_FMON_FAULT_STATUS_PWRCLK_PWRCLK_FMON_DC_FAULT_TRUE,
                    MODS_PTRIM_SYS_FMON_FAULT_STATUS_PWRCLK_PWRCLK_FMON_COUNT_LOWER_THRESH_HIGH_FAULT_TRUE,
                    MODS_PTRIM_SYS_FMON_FAULT_STATUS_PWRCLK_PWRCLK_FMON_COUNT_HIGHER_THRESH_HIGH_FAULT_TRUE,
                    MODS_PTRIM_SYS_FMON_FAULT_STATUS_PWRCLK_PWRCLK_FMON_OVERFLOW_ERROR_TRUE,
                    pFaultMask));
            }
            break;
        }
        case Gpu::ClkLwlCom:
        {
            if (Regs().Test32(
                MODS_PTRIM_SYS_FMON_FAULT_STATUS_LWL_COMMON_CLK_LWL_COMMON_CLK_FMON_FAULT_OUT_STATUS_TRUE))
            {
                CHECK_RC(GetFaultMask(
                        MODS_PTRIM_SYS_FMON_FAULT_STATUS_LWL_COMMON_CLK_LWL_COMMON_CLK_FMON_DC_FAULT_TRUE,
                        MODS_PTRIM_SYS_FMON_FAULT_STATUS_LWL_COMMON_CLK_LWL_COMMON_CLK_FMON_COUNT_LOWER_THRESH_HIGH_FAULT_TRUE,
                        MODS_PTRIM_SYS_FMON_FAULT_STATUS_LWL_COMMON_CLK_LWL_COMMON_CLK_FMON_COUNT_HIGHER_THRESH_HIGH_FAULT_TRUE,
                        MODS_PTRIM_SYS_FMON_FAULT_STATUS_LWL_COMMON_CLK_LWL_COMMON_CLK_FMON_OVERFLOW_ERROR_TRUE,
                        pFaultMask));
            }
            break;
        }
        case Gpu::ClkPexRef:
        {
            if (Regs().Test32(
                MODS_PTRIM_SYS_FMON_FAULT_STATUS_PEX_REFCLK_PEX_REFCLK_FMON_FAULT_OUT_STATUS_TRUE))
            {
                CHECK_RC(GetFaultMask(
                    MODS_PTRIM_SYS_FMON_FAULT_STATUS_PEX_REFCLK_PEX_REFCLK_FMON_DC_FAULT_TRUE,
                    MODS_PTRIM_SYS_FMON_FAULT_STATUS_PEX_REFCLK_PEX_REFCLK_FMON_COUNT_LOWER_THRESH_HIGH_FAULT_TRUE,
                    MODS_PTRIM_SYS_FMON_FAULT_STATUS_PEX_REFCLK_PEX_REFCLK_FMON_COUNT_HIGHER_THRESH_HIGH_FAULT_TRUE,
                    MODS_PTRIM_SYS_FMON_FAULT_STATUS_PEX_REFCLK_PEX_REFCLK_FMON_OVERFLOW_ERROR_TRUE,
                    pFaultMask));
            }
            break;
        }
        case Gpu::ClkX:
        {
            if (Regs().Test32(
                MODS_PTRIM_SYS_FMON_FAULT_STATUS_XCLK_XCLK_FMON_FAULT_OUT_STATUS_TRUE))
            {
                CHECK_RC(GetFaultMask(
                    MODS_PTRIM_SYS_FMON_FAULT_STATUS_XCLK_XCLK_FMON_DC_FAULT_TRUE,
                    MODS_PTRIM_SYS_FMON_FAULT_STATUS_XCLK_XCLK_FMON_COUNT_LOWER_THRESH_HIGH_FAULT_TRUE,
                    MODS_PTRIM_SYS_FMON_FAULT_STATUS_XCLK_XCLK_FMON_COUNT_HIGHER_THRESH_HIGH_FAULT_TRUE,
                    MODS_PTRIM_SYS_FMON_FAULT_STATUS_XCLK_XCLK_FMON_OVERFLOW_ERROR_TRUE,
                    pFaultMask));
            }
            break;
        }
        default:
        {
            MASSERT(!"Unknown call to get Fault Mask \n");
            break;
        }
    }
    return rc;
}

//-----------------------------------------------------------------------------
/* virtual */ UINT32 HopperGpuSubdevice::GetMaxOfaCount() const
{
    return Regs().Read32(MODS_PTOP_SCAL_NUM_OFA);
}

//-----------------------------------------------------------------------------
/* virtual */ bool HopperGpuSubdevice::IsGFWBootStarted()
{
    auto &regs = GpuSubdevice::Regs();

    if (!regs.IsSupported(MODS_THERM_I2CS_SCRATCH_FSP_BOOT_COMPLETE))
        return false;
    // returns true if the BOOT has started or completed (ie any non-zero value)
    return !regs.Test32(MODS_THERM_I2CS_SCRATCH_FSP_BOOT_COMPLETE_STATUS, 0);
}

//-----------------------------------------------------------------------------
/* virtual */ RC HopperGpuSubdevice::PollGFWBootComplete(Tee::Priority pri)
{
    /*
     * From https://lwgrok-02:8621/resman/xref/gfw_ucode/sw/rel/gfw_ucode/r5/v1/src/gfw-telemetry.ads#349
     * Boot_Partition_Status_Init => 0,
     * Boot_SubPartition_Selwreboot_Entry_Started => 1,
     * Boot_SubPartition_Selwreboot_Entry_Completed  => 2,
     * Boot_SubPartition_Devinit_Started => 3,
     * Boot_SubPartition_Devinit_Completed => 4,
     * Boot_SubPartition_Lwlink_Init_Started => 5,
     * Boot_SubPartition_Lwlink_Init_Completed => 6,
     * Boot_SubPartition_Bootfrts_Started => 7,
     * Boot_SubPartition_Bootfrts_Completed => 8,
     * Boot_SubPartition_Posttrain_Started => 9,
     * Boot_SubPartition_Posttrain_Completed => 10,
     * Boot_SubPartition_Posttrain_Wait_Started => 11,
     * Boot_SubPartition_Posttrain_Wait_Completed => 12,
     * Boot_SubPartition_Bootime_Training_Started => 13,
     * Boot_SubPartition_Bootime_Training_Completed => 14,
     * Boot_SubPartition_Bootexit_Started => 15,
     * Boot_SubPartition_Bootexit_Completed => 16,
     * Boot_Partition_Success => 255
     */

    RC rc;

    // Don't poll for FSP boot registers in VF mode.
    if (Platform::IsVirtFunMode())
    {
        return rc;
    }

    rc = PollGpuRegValue(
                MODS_THERM_I2CS_SCRATCH_FSP_BOOT_COMPLETE_STATUS_SUCCESS,
                0,
                GetGFWBootTimeoutMs());

    if (rc != RC::OK)
    {
        Printf(Tee::PriWarn, "GFW boot did not complete. May be due to an invalid FS config\n");
        Printf(Tee::PriNormal, "  Boot status = 0x%08x\n",
                               Regs().Read32(MODS_THERM_I2CS_SCRATCH_FSP_BOOT_COMPLETE_STATUS));


        // https://lwgrok-02:8621/resman/xref/gfw_ucode/sw/rel/gfw_ucode/r5/v1/src/gfw-telemetry.ads#21
        Printf(Tee::PriNormal, "  FSP error status register = 0x%8x\n",
                                Regs().Read32(MODS_PFSP_FALCON_COMMON_SCRATCH_GROUP_2, 0));

        return ReportGFWBootMemoryErrors(pri);
    }

    return rc;
}

//-----------------------------------------------------------------------------
bool HopperGpuSubdevice::GpioIsOutput(UINT32 whichGpio)
{
    if (GpuPtr()->IsRmInitCompleted())
    {
        return GpuSubdevice::GpioIsOutput(whichGpio);
    }
    else
    {
        RegHal& regs = GpuSubdevice::Regs();
        return regs.Test32(MODS_GPIO_OUTPUT_CNTL_IO_OUT_EN_YES, whichGpio);
    }
}

void HopperGpuSubdevice::GpioSetOutput(UINT32 whichGpio, bool level)
{
    if (GpuPtr()->IsRmInitCompleted())
    {
        return GpuSubdevice::GpioSetOutput(whichGpio, level);
    }
    else
    {
        RegHal& regs = GpuSubdevice::Regs();
        // First set the direction to output
        regs.Write32(MODS_GPIO_OUTPUT_CNTL_IO_OUT_EN_YES, whichGpio);
        // Then set the output
        regs.Write32(MODS_GPIO_OUTPUT_CNTL_IO_OUTPUT, whichGpio, level ? 1 : 0);
        // Trigger the GPIO change
        regs.Write32(MODS_PMGR_GPIO_OUTPUT_CNTL_TRIGGER_UPDATE_TRIGGER);
    }
}

bool HopperGpuSubdevice::GpioGetOutput(UINT32 whichGpio)
{
    if (GpuPtr()->IsRmInitCompleted())
    {
        return GpuSubdevice::GpioGetOutput(whichGpio);
    }
    else
    {
        RegHal& regs = GpuSubdevice::Regs();
        return regs.Read32(MODS_GPIO_OUTPUT_CNTL_IO_OUTPUT, whichGpio) != 0;
    }
}

bool HopperGpuSubdevice::GpioGetInput(UINT32 whichGpio)
{
    if (GpuPtr()->IsRmInitCompleted())
    {
        return GpuSubdevice::GpioGetInput(whichGpio);
    }
    else
    {
        RegHal& regs = GpuSubdevice::Regs();
        return regs.Read32(MODS_GPIO_OUTPUT_CNTL_IO_INPUT, whichGpio) != 0;
    }
}

RC HopperGpuSubdevice::SetGpioDirection(UINT32 gpioNum, GpioDirection direction)
{
    RegHal& regs = GpuSubdevice::Regs();

    UINT32 numGpios = regs.LookupAddress(MODS_GPIO_OUTPUT_CNTL__SIZE_1);
    if (gpioNum > numGpios)
    {
        Printf(Tee::PriError, "The chip only has %u GPIOs.\n", numGpios);
        return RC::BAD_PARAMETER;
    }

    regs.Write32(MODS_GPIO_OUTPUT_CNTL_IO_OUT_EN, gpioNum, direction == OUTPUT ? 1 : 0);
    return RC::OK;
}

FLOAT64 HopperGpuSubdevice::GetGFWBootTimeoutMs() const
{
    // HBM3E can take 3s to initialize
    return std::max(4000.0, Tasker::GetDefaultTimeoutMs());
}
