/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2020-2022 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */
#include "hopperpcie.h"
#include "core/include/pci.h"
#include "core/include/platform.h"
#include "gpu/include/gpusbdev.h"
#include "core/include/utility.h"
#include "lwmisc.h"
#include "hwref/hopper/gh100/dev_lw_xpl.h"
#include "hwref/hopper/gh100/dev_xtl_ep_pcfg_gpu.h"
//TODO: We still need to pull in all of the APIs from the parent classes that are doing BAR0
//      access to PCI config space or reference the LW_XP_* registers and colwert them as follows:
//1. Colwert all BAR0 access of LW_XVE_* to LW_EP_PCFG_GPU_* and use PCI config cycles
//   Use the new defines in dev_xtl_ep_pcfg_gpu.h when doing PCI config accesses.
//2. Colwert all BAR0 access of LW_XP_* to LW_XPL_* 
//   Use the new defines in dev_xpl.h when doing tthe LW_XPL_* colwersions.

// From https://confluence.lwpu.com/display/LWHS/UPHY+6.0+EOM-FOM
#define LW_MINION_UCODE_CONFIGEOM_FOM_MODE_EYEO            0x1
#define LW_MINION_UCODE_CONFIGEOM_PAM_EYE_SEL_ALL          0x7
#define LW_MINION_UCODE_CONFIGEOM_BER_EYE_SEL_UPPER        0x2
#define LW_MINION_UCODE_CONFIGEOM_BER_EYE_SEL_LOWER        0x1
#define LW_MINION_UCODE_CONFIGEOM_BER_EYE_SEL_ALL          0x3
#define LW_MINION_UCODE_CONFIGEOM_BER_EYE_SEL_GEN1_4       0xf

//-----------------------------------------------------------------------------
bool HopperPcie::DoSupportsFomMode(Pci::FomMode mode)
{
    Pci::PcieLinkSpeed speed = GetLinkSpeed(Pci::SpeedUnknown);
    switch (mode)
    {
        case Pci::EYEO_Y:
            return true;
        case Pci::EYEO_Y_U:
        case Pci::EYEO_Y_L:
            return (speed > Pci::Speed16000MBPS);
        default:
            break;
    }
    return false;
}

//-----------------------------------------------------------------------------
RC HopperPcie::DoReadPadLaneReg
(
    UINT32  linkId,
    UINT32  lane,
    UINT16  addr,
    UINT16* pData
)
{
    const RegHal& regs =  GetSubdevice()->Regs();
    const UINT32 maxLanes = regs.LookupMaskSize(MODS_UXL_LANE_COMMON_MCAST_CTL_LANE_MASK);
    if (lane >= maxLanes)
    {
        Printf(Tee::PriError, "Invalid lane %u specified\n", lane);
        return RC::UNSUPPORTED_HARDWARE_FEATURE;
    }

    RC rc;
    LW2080_CTRL_CMD_BUS_GET_UPHY_DLN_CFG_SPACE_PARAMS getDlnRegParams =
        { addr, (1U << lane), 0 };
    CHECK_RC(LwRmPtr()->ControlBySubdevice(
        GetSubdevice(),
        LW2080_CTRL_CMD_BUS_GET_UPHY_DLN_CFG_SPACE,
        &getDlnRegParams,
        sizeof(getDlnRegParams)
    ));
    *pData = getDlnRegParams.regValue;
    return rc;
}

//-----------------------------------------------------------------------------
RC HopperPcie::DoWritePadLaneReg
(
    UINT32 linkId,
    UINT32 lane,
    UINT16 addr,
    UINT16 data
)
{
    RegHal& regs =  GetSubdevice()->Regs();
    const UINT32 maxLanes = regs.LookupMaskSize(MODS_UXL_LANE_COMMON_MCAST_CTL_LANE_MASK);
    if (lane >= maxLanes)
    {
        Printf(Tee::PriError, "Invalid lane %u specified\n", lane);
        return RC::UNSUPPORTED_HARDWARE_FEATURE;
    }

    if (!regs.HasRWAccess(MODS_UXL_LANE_REG_DIRECT_CTL_1) ||
        !regs.HasRWAccess(MODS_UXL_LANE_COMMON_MCAST_CTL))
    {
        Printf(Tee::PriWarn,
               "PCIE UPHY LANE CTL registers may not be written due to priv protection\n");
        return RC::PRIV_LEVEL_VIOLATION;
    }

    UINT32 laneCtl = regs.Read32(MODS_UXL_LANE_REG_DIRECT_CTL_1);
    regs.SetField(&laneCtl, MODS_UXL_LANE_REG_DIRECT_CTL_1_CFG_WDATA, data);
    regs.SetField(&laneCtl, MODS_UXL_LANE_REG_DIRECT_CTL_1_CFG_ADDR, addr);
    regs.SetField(&laneCtl, MODS_UXL_LANE_REG_DIRECT_CTL_1_CFG_WDS, 1);

    const UINT32 laneMask = regs.Read32(MODS_UXL_LANE_COMMON_MCAST_CTL_LANE_MASK);
    regs.Write32(MODS_UXL_LANE_COMMON_MCAST_CTL_LANE_MASK, (1U << lane));
    regs.Write32(MODS_UXL_LANE_REG_DIRECT_CTL_1, laneCtl);
    regs.Write32(MODS_UXL_LANE_COMMON_MCAST_CTL_LANE_MASK, laneMask);

    return RC::OK;
}

//-----------------------------------------------------------------------------
RC HopperPcie::DoReadPllReg(UINT32 ioctl, UINT16 addr, UINT16* pData)
{
    RegHal& regs = GetSubdevice()->Regs();

    if (!regs.HasRWAccess(MODS_UXL_PLL_REG_DIRECT_CTL_1) ||
        !regs.HasRWAccess(MODS_UXL_LANE_COMMON_MCAST_CTL))
    {
        Printf(Tee::PriWarn,
               "PCIE UPHY PLL CTL registers may not be read due to priv protection\n");
        return RC::PRIV_LEVEL_VIOLATION;
    }

    UINT32 pllCtl0 = regs.Read32(MODS_UXL_PLL_REG_DIRECT_CTL_1);
    regs.SetField(&pllCtl0,
                  MODS_UXL_PLL_REG_DIRECT_CTL_1_CFG_ADDR, addr);
    regs.SetField(&pllCtl0, MODS_UXL_PLL_REG_DIRECT_CTL_1_CFG_RDS, 1);
    regs.Write32(MODS_UXL_PLL_REG_DIRECT_CTL_1, pllCtl0);

    *pData = static_cast<UINT16>(
            regs.Read32(MODS_UXL_PLL_REG_DIRECT_CTL_2_CFG_RDATA));
    return RC::OK;
}

//-----------------------------------------------------------------------------
RC HopperPcie::DoGetAspmCyaL1SubState(UINT32 *pState)
{
    MASSERT(pState);

    *pState = Pci::PM_SUB_DISABLED;

    // RTL can turn off the PCIE interface and only allows certain register accesses
    // to PCIE in this state without crashing, these CYAs are not allowed
    if (Platform::IsVirtFunMode() || (Platform::GetSimulationMode() == Platform::RTL))
    {
        return RC::OK;
    }

    RegHal& regs       = GetSubdevice()->Regs();
    UINT32  cya2Status = 0;

    RC rc;
    CHECK_RC(regs.Read32Priv(MODS_XTL_EP_PRI_PWR_MGMT_CYA2, &cya2Status));

    if (regs.TestField(cya2Status, MODS_XTL_EP_PRI_PWR_MGMT_CYA2_ASPM_L1_1_ENABLE, 1))
    {
        *pState |= Pci::PM_SUB_L11;
    }
    if (regs.TestField(cya2Status, MODS_XTL_EP_PRI_PWR_MGMT_CYA2_ASPM_L1_2_ENABLE, 1))
    {
        *pState |= Pci::PM_SUB_L12;
    }
    return RC::OK;
}

RC HopperPcie::DoGetAspmCyaL1SubStateProtected(UINT32 *pState) const
{
    MASSERT(pState);

    *pState = Pci::PM_SUB_DISABLED;

    // RTL can turn off the PCIE interface and only allows certain register accesses
    // to PCIE in this state without crashing, these CYAs are not allowed
    if (Platform::IsVirtFunMode() || (Platform::GetSimulationMode() == Platform::RTL))
    {
        return RC::OK;
    }

    const RegHal& regs = GetSubdevice()->Regs();
    UINT32  cya1Status = 0;

    RC rc;
    CHECK_RC(regs.Read32Priv(MODS_XTL_EP_PRI_PWR_MGMT_CYA1, &cya1Status));

    if (regs.TestField(cya1Status, MODS_XTL_EP_PRI_PWR_MGMT_CYA1_ASPM_L1_1_ENABLE, 1))
    {
        *pState |= Pci::PM_SUB_L11;
    }
    if (regs.TestField(cya1Status, MODS_XTL_EP_PRI_PWR_MGMT_CYA1_ASPM_L1_2_ENABLE, 1))
    {
        *pState |= Pci::PM_SUB_L12;
    }
    return RC::OK;
}

RC HopperPcie::DoGetAspmCyaStateProtected(UINT32 *pState) const
{
    MASSERT(pState);

    *pState = Pci::ASPM_DISABLED;

    // RTL can turn off the PCIE interface and only allows certain register accesses
    // to PCIE in this state without crashing, these CYAs are not allowed
    if (Platform::IsVirtFunMode() || (Platform::GetSimulationMode() == Platform::RTL))
    {
        return RC::OK;
    }

    const RegHal& regs = GetSubdevice()->Regs();
    UINT32  cya1Status = 0;

    RC rc;
    CHECK_RC(regs.Read32Priv(MODS_XTL_EP_PRI_PWR_MGMT_CYA1, &cya1Status));

    if (regs.TestField(cya1Status, MODS_XTL_EP_PRI_PWR_MGMT_CYA1_ASPM_L0S_ENABLE, 1))
    {
        *pState |= Pci::ASPM_L0S;
    }
    if (regs.TestField(cya1Status, MODS_XTL_EP_PRI_PWR_MGMT_CYA1_ASPM_L1_ENABLE, 1))
    {
        *pState |= Pci::ASPM_L1;
    }
    return RC::OK;
}

//-----------------------------------------------------------------------------
RC HopperPcie::DoGetAspmL1SSCapability(UINT32 *pCap)
{
    MASSERT(pCap);
    RC rc;

    *pCap = Pci::PM_SUB_DISABLED;

    // RTL can turn off the PCIE interface and only allows certain register accesses
    // to PCIE in this state without crashing, these CYAs are not allowed
    if (Platform::IsVirtFunMode() || (Platform::GetSimulationMode() == Platform::RTL))
    {
        return RC::OK;
    }

    const RegHal& regs = GetSubdevice()->Regs();
    UINT32 capsReg = regs.Read32(MODS_EP_PCFG_GPU_L1_PM_SS_CAP_REGISTER);
    if (regs.TestField(capsReg, MODS_EP_PCFG_GPU_L1_PM_SS_CAP_REGISTER_L1_PM_SS_SUPPORT_SUPPORTED))
    {
        if (regs.TestField(capsReg, MODS_EP_PCFG_GPU_L1_PM_SS_CAP_REGISTER_ASPM_L11_SUPPORT_SUPPORTED))
        {
            *pCap |= Pci::PM_SUB_L11;
        }
        if (regs.TestField(capsReg, MODS_EP_PCFG_GPU_L1_PM_SS_CAP_REGISTER_ASPM_L12_SUPPORT_SUPPORTED))
        {
            *pCap |= Pci::PM_SUB_L12;
        }
    }

    return RC::OK;
}

//-----------------------------------------------------------------------------
//! \brief Get the current Secure Canary Path Monitor status of the GPU.
//!
//! \return The current SCPM status of the GPU
/* virtual */ RC HopperPcie::DoGetScpmStatus(UINT32 * pStatus) const
{
    RC rc;
    if (!pStatus)
    {
        return RC::BAD_PARAMETER;
    }
    *pStatus = 0;
    UINT16 extCapOffset = 0;
    UINT16 extCapSize   = 0;
    // Get the generic header
    CHECK_RC(Pci::GetExtendedCapInfo(DomainNumber(), BusNumber(),
                                     DeviceNumber(), FunctionNumber(),
                                     Pci::VSEC_ECI, 0,
                                     &extCapOffset, &extCapSize));

    // VSEC_DEBUG_SEC is at offet 0x10 from the generic header.
    CHECK_RC(Platform::PciRead32(DomainNumber(), BusNumber(),
                                 DeviceNumber(), FunctionNumber(),
                                 extCapOffset + 0x10,
                                 pStatus));

    return RC::OK;
}

//-----------------------------------------------------------------------------
//! \brief Decode the SCPM status bits.
//! \return a string with the decoded bits;
//! Refer to LW_EP_PCFG_GPU_VSEC_DEBUG_SEC in dev_xtl_ep_pcfg_gpu.h for bit definitions.
//! Each bit in the SCPM status corresponds to a sec_fault sensor or boot error. SCPM, POD, DCLS 
//  are sensors that trigger when an attack is detected. IFF errors are all errors that should not
//  happen during boot. The SCPM status shows what sensor(or boot error) triggered the sec_fault.
// Acronyms:
// POD  = Power On Detector
// SCPM = Security Canary Path Monitor
// DCLS = Dual Core Lock Step
/*virtual*/ string HopperPcie::DoDecodeScpmStatus(UINT32 status) const
{
    string decode;
    bool bComma = false;
    const char * sensors[] = 
    {
        "FUSE_POD"      //$ Triggered if the fuse block is powered up without being reset correctly
        "FUSE_SCPM"     //$ Triggered if a voltage/clock glitch causes the fuse block functional logic to run out of spec.
        "FSP_SCPM"      //$ SCPM in the FSP is triggered if a voltage/clock glitch causes the fuse block functional logic to run out of spec.
        "SEC2_SCPM"     //$ SCPM in the SEC2 is triggered if a voltage/clock glitch causes the fuse block functional logic to run out of spec.
        "FSP_DCLS"      //$ DCLS monitor in the FSP is triggered if the two FSP cores running the exact same code ever come up with a different result
        "SEC2_DCLS"     //$ DCLS monitor in the SEC2 is triggered if the two SEC cores running the exact same code ever come up with a different result
        "GSP_DCLS"      //$ DCLS monitor in the GSP is triggered if the two GSP cores running the exact same code ever come up with a different result
        "PMU_DCLS"      //$ DCLS monitor in the PMU is triggered if the two PMU cores running the exact same code ever come up with a different result
        "SEQ_TOO_BIG"         
        "PRE_IFF_CRC"         
        "POST_IFF_CRC"        
        "ECC"                 
        "CMD"                 
        "PRI"                 
        "WDG"                 
    };
    for (int i = 0; i < 16; i++)
    {
        if (bComma)
        {
            decode += ", ";
        }
        if (status & (1 << i))
        {
            decode += sensors[i];
            bComma = true;
        }
    }
    if (DRF_VAL(_EP_PCFG_GPU, _VSEC_DEBUG_SEC, _FAULT_BOOTFSM, status))
    {
        decode += ", BOOTFSM";
        decode += Utility::StrPrintf("(POS=0x%x)",
                                     DRF_VAL(_EP_PCFG_GPU, _VSEC_DEBUG_SEC, _IFF_POS, status));
    }
    return decode;
}

//-----------------------------------------------------------------------------
//! \brief Get the ASPM entry counters
//!
//! \param pCounts : Pointer to returned ASPM entry counts
//!
//! \return OK if successful, not OK otherwise
/* virtual */ RC HopperPcie::DoGetAspmEntryCounts(Pcie::AspmEntryCounters *pCounts)
{
    MASSERT(pCounts);
    RC rc;
    LW2080_CTRL_BUS_GET_PEX_COUNTERS_PARAMS params = { 0 };
    params.pexCounterMask =
        LW2080_CTRL_BUS_PEX_COUNTER_L1_1_ENTRY_COUNT |
        LW2080_CTRL_BUS_PEX_COUNTER_L1_2_ENTRY_COUNT |
        LW2080_CTRL_BUS_PEX_COUNTER_L1_2_ABORT_COUNT |
        LW2080_CTRL_BUS_PEX_COUNTER_L1_ENTRY_COUNT;

    CHECK_RC(LwRmPtr()->ControlBySubdevice(GetSubdevice(),
                                           LW2080_CTRL_CMD_BUS_GET_PEX_COUNTERS,
                                           &params,
                                           sizeof(params)));
    pCounts->L1Entry =params.pexCounters[Utility::BitScanForward(
            LW2080_CTRL_BUS_PEX_COUNTER_L1_ENTRY_COUNT)];
    pCounts->L1_1_Entry = params.pexCounters[Utility::BitScanForward(
            LW2080_CTRL_BUS_PEX_COUNTER_L1_1_ENTRY_COUNT)];
    pCounts->L1_2_Entry = params.pexCounters[Utility::BitScanForward(
            LW2080_CTRL_BUS_PEX_COUNTER_L1_2_ENTRY_COUNT)];
    pCounts->L1_2_Abort = params.pexCounters[Utility::BitScanForward(
            LW2080_CTRL_BUS_PEX_COUNTER_L1_2_ABORT_COUNT)];

    const RegHal& regs = GetSubdevice()->Regs();
    pCounts->L1ClockPmPllPdEntry = regs.Read32(MODS_XPL_PL_LTSSM_L1_PLL_PD_ENTRY_COUNT);
    pCounts->L1ClockPmCpmEntry = regs.Read32(MODS_XPL_PL_LTSSM_L1_CPM_ENTRY_COUNT);
    return rc;
}

//-----------------------------------------------------------------------------
//! \brief Get the current ASPM state of the GPU.
//!
//! \return The current ASPM state of the GPU
/* virtual */ UINT32 HopperPcie::DoGetAspmState()
{
    if (Platform::IsVirtFunMode())
    {
        return Pci::ASPM_DISABLED;
    }
    UINT32  ctrlStatus         = 0;

    Platform::PciRead32(DomainNumber(), BusNumber(), DeviceNumber(), FunctionNumber(),
                        LW_EP_PCFG_GPU_LINK_CONTROL_STATUS, &ctrlStatus);

    const UINT32  state  = DRF_VAL(2080_CTRL, _BUS_INFO, _PCIE_LINK_CTRL_STATUS_ASPM, ctrlStatus);

    switch (state)
    {
        case LW2080_CTRL_BUS_INFO_PCIE_LINK_CTRL_STATUS_ASPM_L0S:
            return Pci::ASPM_L0S;
        case LW2080_CTRL_BUS_INFO_PCIE_LINK_CTRL_STATUS_ASPM_L1:
            return Pci::ASPM_L1;
        case LW2080_CTRL_BUS_INFO_PCIE_LINK_CTRL_STATUS_ASPM_L0S_L1:
            return Pci::ASPM_L0S_L1;
        case LW2080_CTRL_BUS_INFO_PCIE_LINK_CTRL_STATUS_ASPM_DISABLED:
            return Pci::ASPM_DISABLED;
    }
    MASSERT(!"Unknown ASPM State");
    return Pci::ASPM_UNKNOWN_STATE;
}

//-----------------------------------------------------------------------------
RC HopperPcie::DoGetL1ClockPmCyaSubstate(UINT32 *pState) const
{
    MASSERT(pState);

    *pState = L1_CLK_PM_DISABLED;

    // RTL can turn off the PCIE interface and only allows certain register accesses
    // to PCIE in this state without crashing, these CYAs are not allowed
    if (Platform::IsVirtFunMode() || (Platform::GetSimulationMode() == Platform::RTL))
    {
        return RC::OK;
    }

    const RegHal& regs = GetSubdevice()->Regs();
    UINT32  cya2Status = 0;

    RC rc;
    CHECK_RC(regs.Read32Priv(MODS_XTL_EP_PRI_PWR_MGMT_CYA2, &cya2Status));

    if (regs.TestField(cya2Status, MODS_XTL_EP_PRI_PWR_MGMT_CYA2_ASPM_L1_PLL_PD_ENABLE, 1))
    {
        *pState |= L1_CLK_PM_PLLPD;
    }
    if (regs.TestField(cya2Status, MODS_XTL_EP_PRI_PWR_MGMT_CYA2_CLOCK_PM_ENABLE, 1))
    {
        *pState |= L1_CLK_PM_PLLPD_CPM;
    }
    return RC::OK;
}

//-----------------------------------------------------------------------------
RC HopperPcie::DoGetL1ClockPmCyaSubstateProtected(UINT32 *pState) const
{
    MASSERT(pState);

    *pState = L1_CLK_PM_DISABLED;

    // RTL can turn off the PCIE interface and only allows certain register accesses
    // to PCIE in this state without crashing, these CYAs are not allowed
    if (Platform::IsVirtFunMode() || (Platform::GetSimulationMode() == Platform::RTL))
    {
        return RC::OK;
    }

    const RegHal& regs = GetSubdevice()->Regs();
    UINT32  cya1Status = 0;

    RC rc;
    CHECK_RC(regs.Read32Priv(MODS_XTL_EP_PRI_PWR_MGMT_CYA1, &cya1Status));

    if (regs.TestField(cya1Status, MODS_XTL_EP_PRI_PWR_MGMT_CYA1_ASPM_L1_PLL_PD_ENABLE, 1))
    {
        *pState |= L1_CLK_PM_PLLPD;
    }
    if (regs.TestField(cya1Status, MODS_XTL_EP_PRI_PWR_MGMT_CYA1_CLOCK_PM_ENABLE, 1))
    {
        *pState |= L1_CLK_PM_CPM;
    }
    return RC::OK;
}

//-----------------------------------------------------------------------------
/* virtual */ RC HopperPcie::DoGetLTREnabled(bool *pEnabled)
{
    RC rc;
    MASSERT(pEnabled);
    UINT32 reg = 0;
    CHECK_RC(Platform::PciRead32(DomainNumber(), BusNumber(), DeviceNumber(), FunctionNumber(),
                                 LW_EP_PCFG_GPU_DEVICE_CAPABILITIES_2, &reg));

    if (FLD_TEST_DRF(_EP_PCFG_GPU, _DEVICE_CAPABILITIES_2, _LTR_MECHANISM, _SUPPORTED, reg))
    {
        CHECK_RC(Platform::PciRead32(DomainNumber(), BusNumber(), DeviceNumber(), FunctionNumber(),
                                     LW_EP_PCFG_GPU_DEVICE_CONTROL_STATUS_2, &reg));

        *pEnabled = FLD_TEST_DRF_NUM(_EP_PCFG_GPU, _DEVICE_CONTROL_STATUS_2, _LTR_ENABLE, 1, reg);
        return RC::OK;
    }
    else
    {
        return RC::UNSUPPORTED_HARDWARE_FEATURE;
    }
}

//-----------------------------------------------------------------------------
//! \brief Helper function to get the PCIE lanes for Rx or Tx
//!
//! \param isTx : Set to 'true' for Tx, 'false' for Rx
//!
//! \return PCIE lanes (16 bits, each one corresponding to a lane)
UINT32 HopperPcie::GetRegLanes(bool isTx) const
{
    UINT32 val = 0;
    const RegHal& regs = GetDevice()->Regs();

    UINT32 regVal = regs.Read32(MODS_XPL_PL_LTSSM_LINK_STATUS_0);

    // Map the 5 bit register field value to the corresponding lanes
    if (isTx)
    {
        if (regs.TestField(regVal, MODS_XPL_PL_LTSSM_LINK_STATUS_0_ACTIVE_TX_LANES_NULL))
        {
            val = 0x0;
        }
        else if (regs.TestField(regVal, MODS_XPL_PL_LTSSM_LINK_STATUS_0_ACTIVE_TX_LANES_00_00))
        {
            val = 0x1;
        }
        else if (regs.TestField(regVal, MODS_XPL_PL_LTSSM_LINK_STATUS_0_ACTIVE_TX_LANES_01_01))
        {
            val = 0x2;
        }
        else if (regs.TestField(regVal, MODS_XPL_PL_LTSSM_LINK_STATUS_0_ACTIVE_TX_LANES_02_02))
        {
            val = 0x4;
        }
        else if (regs.TestField(regVal, MODS_XPL_PL_LTSSM_LINK_STATUS_0_ACTIVE_TX_LANES_03_03))
        {
            val = 0x8;
        }
        else if (regs.TestField(regVal, MODS_XPL_PL_LTSSM_LINK_STATUS_0_ACTIVE_TX_LANES_04_04))
        {
            val = 0x10;
        }
        else if (regs.TestField(regVal, MODS_XPL_PL_LTSSM_LINK_STATUS_0_ACTIVE_TX_LANES_05_05))
        {
            val = 0x20;
        }
        else if (regs.TestField(regVal, MODS_XPL_PL_LTSSM_LINK_STATUS_0_ACTIVE_TX_LANES_06_06))
        {
            val = 0x40;
        }
        else if (regs.TestField(regVal, MODS_XPL_PL_LTSSM_LINK_STATUS_0_ACTIVE_TX_LANES_07_07))
        {
            val = 0x80;
        }
        else if (regs.TestField(regVal, MODS_XPL_PL_LTSSM_LINK_STATUS_0_ACTIVE_TX_LANES_08_08))
        {
            val = 0x100;
        }
        else if (regs.TestField(regVal, MODS_XPL_PL_LTSSM_LINK_STATUS_0_ACTIVE_TX_LANES_09_09))
        {
            val = 0x200;
        }
        else if (regs.TestField(regVal, MODS_XPL_PL_LTSSM_LINK_STATUS_0_ACTIVE_TX_LANES_10_10))
        {
            val = 0x400;
        }
        else if (regs.TestField(regVal, MODS_XPL_PL_LTSSM_LINK_STATUS_0_ACTIVE_TX_LANES_11_11))
        {
            val = 0x800;
        }
        else if (regs.TestField(regVal, MODS_XPL_PL_LTSSM_LINK_STATUS_0_ACTIVE_TX_LANES_12_12))
        {
            val = 0x1000;
        }
        else if (regs.TestField(regVal, MODS_XPL_PL_LTSSM_LINK_STATUS_0_ACTIVE_TX_LANES_13_13))
        {
            val = 0x2000;
        }
        else if (regs.TestField(regVal, MODS_XPL_PL_LTSSM_LINK_STATUS_0_ACTIVE_TX_LANES_14_14))
        {
            val = 0x4000;
        }
        else if (regs.TestField(regVal, MODS_XPL_PL_LTSSM_LINK_STATUS_0_ACTIVE_TX_LANES_15_15))
        {
            val = 0x8000;
        }
        else if (regs.TestField(regVal, MODS_XPL_PL_LTSSM_LINK_STATUS_0_ACTIVE_TX_LANES_01_00))
        {
            val = 0x3;
        }
        else if (regs.TestField(regVal, MODS_XPL_PL_LTSSM_LINK_STATUS_0_ACTIVE_TX_LANES_03_02))
        {
            val = 0xC;
        }
        else if (regs.TestField(regVal, MODS_XPL_PL_LTSSM_LINK_STATUS_0_ACTIVE_TX_LANES_05_04))
        {
            val = 0x30;
        }
        else if (regs.TestField(regVal, MODS_XPL_PL_LTSSM_LINK_STATUS_0_ACTIVE_TX_LANES_07_06))
        {
            val = 0xC0;
        }
        else if (regs.TestField(regVal, MODS_XPL_PL_LTSSM_LINK_STATUS_0_ACTIVE_TX_LANES_09_08))
        {
            val = 0x300;
        }
        else if (regs.TestField(regVal, MODS_XPL_PL_LTSSM_LINK_STATUS_0_ACTIVE_TX_LANES_11_10))
        {
            val = 0xC00;
        }
        else if (regs.TestField(regVal, MODS_XPL_PL_LTSSM_LINK_STATUS_0_ACTIVE_TX_LANES_13_12))
        {
            val = 0x3000;
        }
        else if (regs.TestField(regVal, MODS_XPL_PL_LTSSM_LINK_STATUS_0_ACTIVE_TX_LANES_15_14))
        {
            val = 0xC000;
        }
        else if (regs.TestField(regVal, MODS_XPL_PL_LTSSM_LINK_STATUS_0_ACTIVE_TX_LANES_03_00))
        {
            val = 0xF;
        }
        else if (regs.TestField(regVal, MODS_XPL_PL_LTSSM_LINK_STATUS_0_ACTIVE_TX_LANES_07_04))
        {
            val = 0xF0;
        }
        else if (regs.TestField(regVal, MODS_XPL_PL_LTSSM_LINK_STATUS_0_ACTIVE_TX_LANES_11_08))
        {
            val = 0xF00;
        }
        else if (regs.TestField(regVal, MODS_XPL_PL_LTSSM_LINK_STATUS_0_ACTIVE_TX_LANES_15_12))
        {
            val = 0xF000;
        }
        else if (regs.TestField(regVal, MODS_XPL_PL_LTSSM_LINK_STATUS_0_ACTIVE_TX_LANES_07_00))
        {
            val = 0xFF;
        }
        else if (regs.TestField(regVal, MODS_XPL_PL_LTSSM_LINK_STATUS_0_ACTIVE_TX_LANES_15_08))
        {
            val = 0xFF00;
        }
        else if (regs.TestField(regVal, MODS_XPL_PL_LTSSM_LINK_STATUS_0_ACTIVE_TX_LANES_15_00))
        {
            val = 0xFFFF;
        }
    }
    else
    {
        if (regs.TestField(regVal, MODS_XPL_PL_LTSSM_LINK_STATUS_0_ACTIVE_RX_LANES_NULL))
        {
            val = 0x0;
        }
        else if (regs.TestField(regVal, MODS_XPL_PL_LTSSM_LINK_STATUS_0_ACTIVE_RX_LANES_00_00))
        {
            val = 0x1;
        }
        else if (regs.TestField(regVal, MODS_XPL_PL_LTSSM_LINK_STATUS_0_ACTIVE_RX_LANES_01_01))
        {
            val = 0x2;
        }
        else if (regs.TestField(regVal, MODS_XPL_PL_LTSSM_LINK_STATUS_0_ACTIVE_RX_LANES_02_02))
        {
            val = 0x4;
        }
        else if (regs.TestField(regVal, MODS_XPL_PL_LTSSM_LINK_STATUS_0_ACTIVE_RX_LANES_03_03))
        {
            val = 0x8;
        }
        else if (regs.TestField(regVal, MODS_XPL_PL_LTSSM_LINK_STATUS_0_ACTIVE_RX_LANES_04_04))
        {
            val = 0x10;
        }
        else if (regs.TestField(regVal, MODS_XPL_PL_LTSSM_LINK_STATUS_0_ACTIVE_RX_LANES_05_05))
        {
            val = 0x20;
        }
        else if (regs.TestField(regVal, MODS_XPL_PL_LTSSM_LINK_STATUS_0_ACTIVE_RX_LANES_06_06))
        {
            val = 0x40;
        }
        else if (regs.TestField(regVal, MODS_XPL_PL_LTSSM_LINK_STATUS_0_ACTIVE_RX_LANES_07_07))
        {
            val = 0x80;
        }
        else if (regs.TestField(regVal, MODS_XPL_PL_LTSSM_LINK_STATUS_0_ACTIVE_RX_LANES_08_08))
        {
            val = 0x100;
        }
        else if (regs.TestField(regVal, MODS_XPL_PL_LTSSM_LINK_STATUS_0_ACTIVE_RX_LANES_09_09))
        {
            val = 0x200;
        }
        else if (regs.TestField(regVal, MODS_XPL_PL_LTSSM_LINK_STATUS_0_ACTIVE_RX_LANES_10_10))
        {
            val = 0x400;
        }
        else if (regs.TestField(regVal, MODS_XPL_PL_LTSSM_LINK_STATUS_0_ACTIVE_RX_LANES_11_11))
        {
            val = 0x800;
        }
        else if (regs.TestField(regVal, MODS_XPL_PL_LTSSM_LINK_STATUS_0_ACTIVE_RX_LANES_12_12))
        {
            val = 0x1000;
        }
        else if (regs.TestField(regVal, MODS_XPL_PL_LTSSM_LINK_STATUS_0_ACTIVE_RX_LANES_13_13))
        {
            val = 0x2000;
        }
        else if (regs.TestField(regVal, MODS_XPL_PL_LTSSM_LINK_STATUS_0_ACTIVE_RX_LANES_14_14))
        {
            val = 0x4000;
        }
        else if (regs.TestField(regVal, MODS_XPL_PL_LTSSM_LINK_STATUS_0_ACTIVE_RX_LANES_15_15))
        {
            val = 0x8000;
        }
        else if (regs.TestField(regVal, MODS_XPL_PL_LTSSM_LINK_STATUS_0_ACTIVE_RX_LANES_01_00))
        {
            val = 0x3;
        }
        else if (regs.TestField(regVal, MODS_XPL_PL_LTSSM_LINK_STATUS_0_ACTIVE_RX_LANES_03_02))
        {
            val = 0xC;
        }
        else if (regs.TestField(regVal, MODS_XPL_PL_LTSSM_LINK_STATUS_0_ACTIVE_RX_LANES_05_04))
        {
            val = 0x30;
        }
        else if (regs.TestField(regVal, MODS_XPL_PL_LTSSM_LINK_STATUS_0_ACTIVE_RX_LANES_07_06))
        {
            val = 0xC0;
        }
        else if (regs.TestField(regVal, MODS_XPL_PL_LTSSM_LINK_STATUS_0_ACTIVE_RX_LANES_09_08))
        {
            val = 0x300;
        }
        else if (regs.TestField(regVal, MODS_XPL_PL_LTSSM_LINK_STATUS_0_ACTIVE_RX_LANES_11_10))
        {
            val = 0xC00;
        }
        else if (regs.TestField(regVal, MODS_XPL_PL_LTSSM_LINK_STATUS_0_ACTIVE_RX_LANES_13_12))
        {
            val = 0x3000;
        }
        else if (regs.TestField(regVal, MODS_XPL_PL_LTSSM_LINK_STATUS_0_ACTIVE_RX_LANES_15_14))
        {
            val = 0xC000;
        }
        else if (regs.TestField(regVal, MODS_XPL_PL_LTSSM_LINK_STATUS_0_ACTIVE_RX_LANES_03_00))
        {
            val = 0xF;
        }
        else if (regs.TestField(regVal, MODS_XPL_PL_LTSSM_LINK_STATUS_0_ACTIVE_RX_LANES_07_04))
        {
            val = 0xF0;
        }
        else if (regs.TestField(regVal, MODS_XPL_PL_LTSSM_LINK_STATUS_0_ACTIVE_RX_LANES_11_08))
        {
            val = 0xF00;
        }
        else if (regs.TestField(regVal, MODS_XPL_PL_LTSSM_LINK_STATUS_0_ACTIVE_RX_LANES_15_12))
        {
            val = 0xF000;
        }
        else if (regs.TestField(regVal, MODS_XPL_PL_LTSSM_LINK_STATUS_0_ACTIVE_RX_LANES_07_00))
        {
            val = 0xFF;
        }
        else if (regs.TestField(regVal, MODS_XPL_PL_LTSSM_LINK_STATUS_0_ACTIVE_RX_LANES_15_08))
        {
            val = 0xFF00;
        }
        else if (regs.TestField(regVal, MODS_XPL_PL_LTSSM_LINK_STATUS_0_ACTIVE_RX_LANES_15_00))
        {
            val = 0xFFFF;
        }
    }

    return val;
}

//-----------------------------------------------------------------------------
//! \brief Get the detected PCIE lanes
//!
//! \param pLanes : Detected lanes
RC HopperPcie::DoGetPcieDetectedLanes(UINT32 *pLanes)
{
    GpuSubdevice* pGpuSubdev = GetDevice()->GetInterface<GpuSubdevice>();

    if (!pGpuSubdev->Regs().IsSupported(MODS_XPL_PL_PAD_CTL_LANES_DETECTED))
    {
        return RC::UNSUPPORTED_HARDWARE_FEATURE;
    }

    if (!pLanes)
    {
        return RC::ILWALID_INPUT;
    }

    *pLanes = pGpuSubdev->Regs().Read32(MODS_XPL_PL_PAD_CTL_LANES_DETECTED);

    return RC::OK;
}

//-----------------------------------------------------------------------------
//! \brief Return whether Advanced Error Reporting (AER) is enabled.
//!
//! \return true if AER is enabled, false otherwise
bool HopperPcie::DoAerEnabled() const
{
    const RegHal& regs = GetDevice()->Regs();
    return regs.IsSupported(MODS_EP_PCFG_GPU_AER_HEADER);
}

//-----------------------------------------------------------------------------
//! \brief Enable or disable Advanced Error Reporting (AER)
//!
//! \param bEnable : true to enable AER, false to disable
void HopperPcie::DoEnableAer(bool flag)
{
    if (!DoAerEnabled())
    {
        Printf(Tee::PriError, "Cannot enable AER\n");
        MASSERT(!"Cannot enable AER");
    }
}

//-----------------------------------------------------------------------------
//! \return the offset of AER in the PCI Extended Caps list
UINT16 HopperPcie::DoGetAerOffset() const
{
    const RegHal& regs = GetDevice()->Regs();
    return static_cast<UINT16>(regs.LookupAddress(MODS_EP_PCFG_GPU_AER_HEADER));
}

//-----------------------------------------------------------------------------
UINT32 HopperPcie::DoGetL1SubstateOffset()
{
    const RegHal& regs = GetDevice()->Regs();
    return regs.LookupAddress(MODS_EP_PCFG_GPU_L1_PM_SS_HEADER);
}

//-----------------------------------------------------------------------------
RC HopperPcie::DoResetAspmEntryCounters()
{
    RegHal& regs = GetSubdevice()->Regs();
    regs.Write32(MODS_XPL_PL_LTSSM_ERROR_COUNTER_RESET_L1_CPM_ENTRY_COUNT_PENDING);
    regs.Write32(MODS_XPL_PL_LTSSM_ERROR_COUNTER_RESET_L1_PLL_PD_ENTRY_COUNT_PENDING);

    LW2080_CTRL_BUS_CLEAR_PEX_COUNTERS_PARAMS params = { 0 };
    params.pexCounterMask =
        LW2080_CTRL_BUS_PEX_COUNTER_L1_1_ENTRY_COUNT |
        LW2080_CTRL_BUS_PEX_COUNTER_L1_2_ENTRY_COUNT |
        LW2080_CTRL_BUS_PEX_COUNTER_L1_2_ABORT_COUNT |
        LW2080_CTRL_BUS_PEX_COUNTER_L1_ENTRY_COUNT;

    return LwRmPtr()->ControlBySubdevice(GetSubdevice(),
                                         LW2080_CTRL_CMD_BUS_CLEAR_PEX_COUNTERS,
                                         &params,
                                         sizeof(params));
}

//-----------------------------------------------------------------------------
RC HopperPcie::DoSetResetCrsTimeout(UINT32 timeUs)
{
    RegHal & regs = GetDevice()->Regs();
    const UINT32 maxTimeUs = regs.LookupMask(MODS_XTL_EP_PRI_CRS_TIMEOUT_VAL);
    if (timeUs > maxTimeUs)
    {
        Printf(Tee::PriError, "CRS timeout of %u too large, maximum value is %u\n",
               timeUs, maxTimeUs);
        return RC::BAD_PARAMETER;
    }

    RC rc = regs.Write32Priv(MODS_XTL_EP_PRI_CRS_TIMEOUT_VAL, timeUs);
    if (RC::OK != rc)
    {
        if (Platform::GetSimulationMode() != Platform::Hardware)
        {
            Printf(Tee::PriWarn, "Ignoring CRS priv protection on simulation\n");
            regs.Write32(MODS_XTL_EP_PRI_CRS_TIMEOUT_VAL, timeUs);
            rc.Clear();
        }
        else
        {
            Printf(Tee::PriError, "Failed to write CRS timeout value due to priv protection\n");
        }
    }
    return rc;
}

RC HopperPcie::DoSetAspmCyaState(UINT32 state)
{
    // RTL can turn off the PCIE interface and only allows certain register accesses
    // to PCIE in this state without crashing, these CYAs are not allowed
    if (Platform::IsVirtFunMode() || (Platform::GetSimulationMode() == Platform::RTL))
    {
        return RC::UNSUPPORTED_FUNCTION;
    }

    RegHal& regs       = GetSubdevice()->Regs();
    UINT32  cya2Status = 0;

    RC rc;
    CHECK_RC(regs.Read32Priv(MODS_XTL_EP_PRI_PWR_MGMT_CYA2, &cya2Status));

    regs.SetField(&cya2Status, MODS_XTL_EP_PRI_PWR_MGMT_CYA2_ASPM_L0S_ENABLE,
                  (state & Pci::ASPM_L0S) ? 1 : 0);
    regs.SetField(&cya2Status, MODS_XTL_EP_PRI_PWR_MGMT_CYA2_ASPM_L1_ENABLE,
                  (state & Pci::ASPM_L1) ? 1 : 0);

    Printf(Tee::PriLow, "SetAspmCya on GPU: 0x%x\n", cya2Status);
    CHECK_RC(regs.Write32Priv(MODS_XTL_EP_PRI_PWR_MGMT_CYA2, cya2Status));
    return RC::OK;
}

//-----------------------------------------------------------------------------
RC HopperPcie::DoSetAspmCyaL1SubState(UINT32 state)
{
    // RTL can turn off the PCIE interface and only allows certain register accesses
    // to PCIE in this state without crashing, these CYAs are not allowed
    if (Platform::IsVirtFunMode() || (Platform::GetSimulationMode() == Platform::RTL))
    {
        return RC::UNSUPPORTED_FUNCTION;
    }

    RegHal& regs       = GetSubdevice()->Regs();
    UINT32  cya2Status = 0;

    RC rc;
    CHECK_RC(regs.Read32Priv(MODS_XTL_EP_PRI_PWR_MGMT_CYA2, &cya2Status));

    regs.SetField(&cya2Status, MODS_XTL_EP_PRI_PWR_MGMT_CYA2_ASPM_L1_1_ENABLE,
                  (state & Pci::PM_SUB_L11) ? 1 : 0);
    regs.SetField(&cya2Status, MODS_XTL_EP_PRI_PWR_MGMT_CYA2_ASPM_L1_2_ENABLE,
                  (state & Pci::PM_SUB_L12) ? 1 : 0);

    Printf(Tee::PriLow, "SetAspmCyaL1SubState on GPU: 0x%x\n", cya2Status);
    CHECK_RC(regs.Write32Priv(MODS_XTL_EP_PRI_PWR_MGMT_CYA2, cya2Status));
    return RC::OK;
}

//-----------------------------------------------------------------------------
RC HopperPcie::DoSetL1ClockPmCyaSubstate(UINT32 state)
{
    // RTL can turn off the PCIE interface and only allows certain register accesses
    // to PCIE in this state without crashing, these CYAs are not allowed
    if (Platform::IsVirtFunMode() || (Platform::GetSimulationMode() == Platform::RTL))
    {
        return RC::OK;
    }

    RegHal& regs       = GetSubdevice()->Regs();
    UINT32  cya2Status = 0;

    RC rc;
    CHECK_RC(regs.Read32Priv(MODS_XTL_EP_PRI_PWR_MGMT_CYA2, &cya2Status));

    regs.SetField(&cya2Status, MODS_XTL_EP_PRI_PWR_MGMT_CYA2_ASPM_L1_PLL_PD_ENABLE,
                  (state != L1_CLK_PM_DISABLED) ? 1 : 0);
    regs.SetField(&cya2Status, MODS_XTL_EP_PRI_PWR_MGMT_CYA2_CLOCK_PM_ENABLE,
                  (state & L1_CLK_PM_CPM) ? 1 : 0);

    CHECK_RC(regs.Write32Priv(MODS_XTL_EP_PRI_PWR_MGMT_CYA2, cya2Status));
    return RC::OK;
}

//-----------------------------------------------------------------------------
RC HopperPcie::GetEomSettings(Pci::FomMode mode, EomSettings* pSettings)
{
    pSettings->mode      = LW_MINION_UCODE_CONFIGEOM_FOM_MODE_EYEO;
    pSettings->numBlocks = 0x8;
    pSettings->numErrors = 0x2;

    Pci::PcieLinkSpeed speed = GetLinkSpeed(Pci::SpeedUnknown);

    if (speed > Pci::Speed16000MBPS)
    {
        switch (mode)
        {
            case Pci::EYEO_Y:
                pSettings->berEyeSel = LW_MINION_UCODE_CONFIGEOM_BER_EYE_SEL_ALL;
                break;
            case Pci::EYEO_Y_U:
                pSettings->berEyeSel = LW_MINION_UCODE_CONFIGEOM_BER_EYE_SEL_UPPER;
                break;
            case Pci::EYEO_Y_L:
                pSettings->berEyeSel = LW_MINION_UCODE_CONFIGEOM_BER_EYE_SEL_LOWER;
                break;
            default:
                Printf(Tee::PriError, "Invalid or unsupported FOM mode : %d\n", mode);
                return RC::ILWALID_ARGUMENT;
        }
    }
    else
    {
        if (mode != Pci::EYEO_Y)
        {
            Printf(Tee::PriError, "Invalid or unsupported FOM mode for non-Gen5: %d\n", mode);
            return RC::ILWALID_ARGUMENT;
        }
        pSettings->berEyeSel = LW_MINION_UCODE_CONFIGEOM_BER_EYE_SEL_GEN1_4;
    }
    return RC::OK;
}

