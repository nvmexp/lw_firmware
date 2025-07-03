/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2021-2022 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#include "lagunapcie.h"
#include "core/include/platform.h"
#include "ctrl_dev_internal_lwswitch.h"
#include "device/interface/lwlink/lwlregs.h"
#include "gpu/include/testdevice.h"
#include "mods_reg_hal.h"

// From https://confluence.lwpu.com/display/LWHS/UPHY+6.0+EOM-FOM
#define LW_MINION_UCODE_CONFIGEOM_FOM_MODE_EYEO            0x1
#define LW_MINION_UCODE_CONFIGEOM_PAM_EYE_SEL_ALL          0x7
#define LW_MINION_UCODE_CONFIGEOM_BER_EYE_SEL_UPPER        0x2
#define LW_MINION_UCODE_CONFIGEOM_BER_EYE_SEL_LOWER        0x1
#define LW_MINION_UCODE_CONFIGEOM_BER_EYE_SEL_ALL          0x3
#define LW_MINION_UCODE_CONFIGEOM_BER_EYE_SEL_GEN1_4       0xf

//-----------------------------------------------------------------------------
RC LagunaPcie::DoGetAspmCyaL1SubState(UINT32 *pState)
{
    MASSERT(pState);

    *pState = Pci::PM_SUB_DISABLED;

    // RTL can turn off the PCIE interface and only allows certain register accesses
    // to PCIE in this state without crashing, these CYAs are not allowed
    if (Platform::IsVirtFunMode() || (Platform::GetSimulationMode() == Platform::RTL))
    {
        return RC::OK;
    }

    RegHal& regs       = GetDevice()->GetInterface<LwLinkRegs>()->Regs();
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

//-----------------------------------------------------------------------------
RC LagunaPcie::DoGetAspmCyaL1SubStateProtected(UINT32 *pState) const
{
    MASSERT(pState);

    *pState = Pci::PM_SUB_DISABLED;

    // RTL can turn off the PCIE interface and only allows certain register accesses
    // to PCIE in this state without crashing, these CYAs are not allowed
    if (Platform::IsVirtFunMode() || (Platform::GetSimulationMode() == Platform::RTL))
    {
        return RC::OK;
    }

    const RegHal& regs = GetDevice()->GetInterface<LwLinkRegs>()->Regs();
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

//-----------------------------------------------------------------------------
UINT32 LagunaPcie::DoGetAspmCyaState()
{
    // RTL can turn off the PCIE interface and only allows certain register accesses
    // to PCIE in this state without crashing, these CYAs are not allowed
    if (Platform::IsVirtFunMode() || (Platform::GetSimulationMode() == Platform::RTL))
    {
        return Pci::ASPM_DISABLED;
    }

    RegHal& regs       = GetDevice()->GetInterface<LwLinkRegs>()->Regs();
    UINT32  cya2Status = 0;

    RC rc;
    UINT32 aspmState = Pci::ASPM_DISABLED;
    cya2Status = regs.Read32(MODS_XTL_EP_PRI_PWR_MGMT_CYA2);

    if (regs.TestField(cya2Status, MODS_XTL_EP_PRI_PWR_MGMT_CYA2_ASPM_L0S_ENABLE, 1))
    {
        aspmState |= Pci::ASPM_L0S;
    }
    if (regs.TestField(cya2Status, MODS_XTL_EP_PRI_PWR_MGMT_CYA2_ASPM_L1_ENABLE, 1))
    {
        aspmState |= Pci::ASPM_L1;
    }
    return aspmState;
}

RC LagunaPcie::DoGetAspmCyaStateProtected(UINT32 *pState) const
{
    MASSERT(pState);

    *pState = Pci::ASPM_DISABLED;

    // RTL can turn off the PCIE interface and only allows certain register accesses
    // to PCIE in this state without crashing, these CYAs are not allowed
    if (Platform::IsVirtFunMode() || (Platform::GetSimulationMode() == Platform::RTL))
    {
        return RC::OK;
    }

    const RegHal& regs = GetDevice()->GetInterface<LwLinkRegs>()->Regs();
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
RC LagunaPcie::DoGetAspmEntryCounts(AspmEntryCounters *pCounts)
{
    MASSERT(pCounts);

    // These counters are not supported on LS10
    pCounts->XmitL0SEntry  = 0;
    pCounts->UpstreamL0SEntry = 0;
    pCounts->DeepL1Entry = 0;
    pCounts->L1SS_DeepL1Timouts = 0;
    pCounts->L1PEntry = 0;
    pCounts->L1_ShortDuration = 0;
    
    MASSERT(pCounts);

    RC rc;
    LWSWITCH_PEX_GET_COUNTERS_PARAMS getCountersParams = { };
    getCountersParams.pexCounterMask = LWSWITCH_PEX_COUNTER_L1_ENTRY_COUNT |
                                       LWSWITCH_PEX_COUNTER_L1_1_ENTRY_COUNT |
                                       LWSWITCH_PEX_COUNTER_L1_2_ENTRY_COUNT |
                                       LWSWITCH_PEX_COUNTER_L1_2_ABORT_COUNT;
    CHECK_RC(GetDevice()->GetLibIf()->Control(GetLibHandle(),
         LwLinkDevIf::LibInterface::CONTROL_LWSWITCH_PEX_GET_COUNTERS,
         &getCountersParams,
         sizeof(getCountersParams)));

    pCounts->L1Entry = getCountersParams.pexCounters[Utility::BitScanForward(
            LWSWITCH_PEX_COUNTER_L1_ENTRY_COUNT)];
    pCounts->L1_1_Entry = getCountersParams.pexCounters[Utility::BitScanForward(
            LWSWITCH_PEX_COUNTER_L1_1_ENTRY_COUNT)];
    pCounts->L1_2_Entry = getCountersParams.pexCounters[Utility::BitScanForward(
            LWSWITCH_PEX_COUNTER_L1_2_ENTRY_COUNT)];
    pCounts->L1_2_Abort = getCountersParams.pexCounters[Utility::BitScanForward(
            LWSWITCH_PEX_COUNTER_L1_2_ABORT_COUNT)];

    const RegHal& regs = GetDevice()->GetInterface<LwLinkRegs>()->Regs();
    pCounts->L1ClockPmPllPdEntry = regs.Read32(MODS_XPL_PL_LTSSM_L1_PLL_PD_ENTRY_COUNT_VALUE);
    pCounts->L1ClockPmCpmEntry = regs.Read32(MODS_XPL_PL_LTSSM_L1_CPM_ENTRY_COUNT_VALUE);
    return RC::OK;
}

//-----------------------------------------------------------------------------
RC LagunaPcie::DoGetL1ClockPmCyaSubstate(UINT32 *pState) const
{
    MASSERT(pState);

    *pState = L1_CLK_PM_DISABLED;

    // RTL can turn off the PCIE interface and only allows certain register accesses
    // to PCIE in this state without crashing, these CYAs are not allowed
    if (Platform::IsVirtFunMode() || (Platform::GetSimulationMode() == Platform::RTL))
    {
        return RC::OK;
    }

    const RegHal& regs = GetDevice()->GetInterface<LwLinkRegs>()->Regs();
    UINT32  cya2Status = 0;

    RC rc;
    CHECK_RC(regs.Read32Priv(MODS_XTL_EP_PRI_PWR_MGMT_CYA2, &cya2Status));

    if (regs.TestField(cya2Status, MODS_XTL_EP_PRI_PWR_MGMT_CYA2_ASPM_L1_PLL_PD_ENABLE, 1))
    {
        *pState |= L1_CLK_PM_PLLPD;
    }
    if (regs.TestField(cya2Status, MODS_XTL_EP_PRI_PWR_MGMT_CYA2_CLOCK_PM_ENABLE, 1))
    {
        *pState |= L1_CLK_PM_CPM;
    }
    return RC::OK;
}

//-----------------------------------------------------------------------------
RC LagunaPcie::DoGetL1ClockPmCyaSubstateProtected(UINT32 *pState) const
{
    MASSERT(pState);

    *pState = L1_CLK_PM_DISABLED;

    // RTL can turn off the PCIE interface and only allows certain register accesses
    // to PCIE in this state without crashing, these CYAs are not allowed
    if (Platform::IsVirtFunMode() || (Platform::GetSimulationMode() == Platform::RTL))
    {
        return RC::OK;
    }

    const RegHal& regs = GetDevice()->GetInterface<LwLinkRegs>()->Regs();
    UINT32  cya1Status = 0;

    RC rc;
    CHECK_RC(regs.Read32Priv(MODS_XTL_EP_PRI_PWR_MGMT_CYA1, &cya1Status));

    if (regs.TestField(cya1Status, MODS_XTL_EP_PRI_PWR_MGMT_CYA1_ASPM_L1_PLL_PD_ENABLE, 1))
    {
        *pState |= L1_CLK_PM_PLLPD;
    }
    if (regs.TestField(cya1Status, MODS_XTL_EP_PRI_PWR_MGMT_CYA1_CLOCK_PM_ENABLE, 1) &&
        regs.Test32(MODS_UXL_PLL_REG_CTL_0_REFCLKBUF_EN_ENABLED))
    {
        *pState |= L1_CLK_PM_CPM;
    }
    return RC::OK;
}

//-----------------------------------------------------------------------------
RC LagunaPcie::DoResetAspmEntryCounters()
{ 
    RC rc;
    LWSWITCH_PEX_CLEAR_COUNTERS_PARAMS clearCountersParams = { };
    clearCountersParams.pexCounterMask = LWSWITCH_PEX_COUNTER_L1_ENTRY_COUNT |
                                         LWSWITCH_PEX_COUNTER_L1_1_ENTRY_COUNT |
                                         LWSWITCH_PEX_COUNTER_L1_2_ENTRY_COUNT |
                                         LWSWITCH_PEX_COUNTER_L1_2_ABORT_COUNT;
    CHECK_RC(GetDevice()->GetLibIf()->Control(GetLibHandle(),
         LwLinkDevIf::LibInterface::CONTROL_LWSWITCH_PEX_CLEAR_COUNTERS,
         &clearCountersParams,
         sizeof(clearCountersParams)));

    RegHal& regs    = GetDevice()->GetInterface<LwLinkRegs>()->Regs();
    UINT32 clearReg = regs.Read32(MODS_XPL_PL_LTSSM_ERROR_COUNTER_RESET);
    regs.SetField(&clearReg, MODS_XPL_PL_LTSSM_ERROR_COUNTER_RESET_L1_PLL_PD_ENTRY_COUNT_PENDING);
    regs.SetField(&clearReg, MODS_XPL_PL_LTSSM_ERROR_COUNTER_RESET_L1_CPM_ENTRY_COUNT_PENDING);
    regs.Write32(MODS_XPL_PL_LTSSM_ERROR_COUNTER_RESET, clearReg);
    return rc;
}

//-----------------------------------------------------------------------------
RC LagunaPcie::DoSetAspmCyaState(UINT32 state)
{
    // RTL can turn off the PCIE interface and only allows certain register accesses
    // to PCIE in this state without crashing, these CYAs are not allowed
    if (Platform::IsVirtFunMode() || (Platform::GetSimulationMode() == Platform::RTL))
    {
        return RC::UNSUPPORTED_FUNCTION;
    }

    RegHal& regs       = GetDevice()->GetInterface<LwLinkRegs>()->Regs();
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
RC LagunaPcie::DoSetAspmCyaL1SubState(UINT32 state)
{
    // RTL can turn off the PCIE interface and only allows certain register accesses
    // to PCIE in this state without crashing, these CYAs are not allowed
    if (Platform::IsVirtFunMode() || (Platform::GetSimulationMode() == Platform::RTL))
    {
        return RC::UNSUPPORTED_FUNCTION;
    }

    RegHal& regs       = GetDevice()->GetInterface<LwLinkRegs>()->Regs();
    UINT32  cya2Status = 0;

    RC rc;
    CHECK_RC(regs.Read32Priv(MODS_XTL_EP_PRI_PWR_MGMT_CYA2, &cya2Status));

    regs.SetField(&cya2Status, MODS_XTL_EP_PRI_PWR_MGMT_CYA2_ASPM_L1_1_ENABLE,
                  (state & Pci::PM_SUB_L11) ? 1 : 0);
    regs.SetField(&cya2Status, MODS_XTL_EP_PRI_PWR_MGMT_CYA2_ASPM_L1_2_ENABLE,
                  (state & Pci::PM_SUB_L12) ? 1 : 0);

    Printf(Tee::PriLow, "SetAspmL1ssCya on GPU: 0x%x\n", cya2Status);
    CHECK_RC(regs.Write32Priv(MODS_XTL_EP_PRI_PWR_MGMT_CYA2, cya2Status));
    return RC::OK;
}

//-----------------------------------------------------------------------------
RC LagunaPcie::DoSetL1ClockPmCyaSubstate(UINT32 state)
{
    // RTL can turn off the PCIE interface and only allows certain register accesses
    // to PCIE in this state without crashing, these CYAs are not allowed
    if (Platform::IsVirtFunMode() || (Platform::GetSimulationMode() == Platform::RTL))
    {
        return RC::OK;
    }

    RegHal& regs       = GetDevice()->GetInterface<LwLinkRegs>()->Regs();
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
bool LagunaPcie::DoSupportsFomMode(Pci::FomMode mode)
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
RC LagunaPcie::GetEomSettings(Pci::FomMode mode, EomSettings* pSettings)
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

//-----------------------------------------------------------------------------
//! \brief Helper function to get the PCIE lanes for Rx or Tx
//!
//! \param isTx : Set to 'true' for Tx, 'false' for Rx
//!
//! \return PCIE lanes (16 bits, each one corresponding to a lane)
UINT32 LagunaPcie::GetRegLanes(bool isTx) const
{
    UINT32 val = 0;
    const RegHal& regs = GetDevice()->GetInterface<LwLinkRegs>()->Regs();

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
RC LagunaPcie::DoReadPadLaneReg(UINT32 linkId, UINT32 lane, UINT16 addr, UINT16 * pData)
{
    RegHal& regs = GetDevice()->GetInterface<LwLinkRegs>()->Regs();
    const UINT32 maxLanes = regs.LookupMaskSize(MODS_UXL_LANE_COMMON_MCAST_CTL_LANE_MASK);
    if (lane >= maxLanes)
    {
        Printf(Tee::PriError, "Invalid lane %u specified\n", lane);
        return RC::UNSUPPORTED_HARDWARE_FEATURE;
    }

    RC rc;
    LWSWITCH_GET_PEX_UPHY_DLN_CFG_SPACE_PARAMS getDlnRegParams = { };
    getDlnRegParams.regAddress = addr;
    getDlnRegParams.laneSelectMask = 1U << lane;
    CHECK_RC(GetDevice()->GetLibIf()->Control(GetLibHandle(),
         LwLinkDevIf::LibInterface::CONTROL_LWSWITCH_PEX_GET_UPHY_DLN_CFG_SPACE,
         &getDlnRegParams,
         sizeof(getDlnRegParams)));
    *pData = getDlnRegParams.regValue;
    return rc;
}

RC LagunaPcie::DoWritePadLaneReg(UINT32 linkId, UINT32 lane, UINT16 addr, UINT16 data)
{
    RegHal& regs = GetDevice()->GetInterface<LwLinkRegs>()->Regs();
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

RC LagunaPcie::DoReadPllReg(UINT32 ioctl, UINT16 addr, UINT16 * pData)
{
    RegHal& regs = GetDevice()->GetInterface<LwLinkRegs>()->Regs();

    if (!regs.HasRWAccess(MODS_UXL_LANE_REG_DIRECT_CTL_1) ||
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

