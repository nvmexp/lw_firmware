/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2014-2018,2020 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#include "maxwellpcie.h"
#include "core/include/pci.h"
#include "core/include/platform.h"
#include "gpu/include/gpusbdev.h"
#include "core/include/utility.h"

#include "lwmisc.h"
#include "hwref/maxwell/gm107/dev_lw_xve.h"
#include "hwref/maxwell/gm107/dev_lw_xp.h"

//-----------------------------------------------------------------------------
//! \brief Get the ASPM entry counters
//!
//! \param pCounts : Pointer to returned ASPM entry counts
//!
//! \return OK if successful, not OK otherwise
/* virtual */ RC MaxwellPcie::DoGetAspmEntryCounts
(
    Pcie::AspmEntryCounters *pCounts
)
{
    MASSERT(pCounts);
    RC rc;

    CHECK_RC(FermiPcie::DoGetAspmEntryCounts(pCounts));

    LW2080_CTRL_BUS_GET_PEX_COUNTERS_PARAMS params = { 0 };
    params.pexCounterMask =
        LW2080_CTRL_BUS_PEX_COUNTER_L1_1_ENTRY_COUNT |
        LW2080_CTRL_BUS_PEX_COUNTER_L1_2_ENTRY_COUNT |
        LW2080_CTRL_BUS_PEX_COUNTER_L1_2_ABORT_COUNT |
        LW2080_CTRL_BUS_PEX_COUNTER_L1SS_TO_DEEP_L1_TIMEOUT_COUNT |
        LW2080_CTRL_BUS_PEX_COUNTER_L1_SHORT_DURATION_COUNT;

    CHECK_RC(LwRmPtr()->ControlBySubdevice(GetSubdevice(),
                                           LW2080_CTRL_CMD_BUS_GET_PEX_COUNTERS,
                                           &params,
                                           sizeof(params)));
    pCounts->L1_1_Entry = params.pexCounters[Utility::BitScanForward(
            LW2080_CTRL_BUS_PEX_COUNTER_L1_1_ENTRY_COUNT)];
    pCounts->L1_2_Entry = params.pexCounters[Utility::BitScanForward(
            LW2080_CTRL_BUS_PEX_COUNTER_L1_2_ENTRY_COUNT)];
    pCounts->L1_2_Abort = params.pexCounters[Utility::BitScanForward(
            LW2080_CTRL_BUS_PEX_COUNTER_L1_2_ABORT_COUNT)];
    pCounts->L1SS_DeepL1Timouts = params.pexCounters[Utility::BitScanForward(
            LW2080_CTRL_BUS_PEX_COUNTER_L1SS_TO_DEEP_L1_TIMEOUT_COUNT)];
    pCounts->L1_ShortDuration = params.pexCounters[Utility::BitScanForward(
            LW2080_CTRL_BUS_PEX_COUNTER_L1_SHORT_DURATION_COUNT)];

    return rc;
}

//-----------------------------------------------------------------------------
//! \brief Reset the ASPM entry counters
//!
//! \return OK if successful, not OK otherwise
/* virtual */ RC MaxwellPcie::DoResetAspmEntryCounters()
{
    RC rc;
    CHECK_RC(FermiPcie::DoResetAspmEntryCounters());

    LW2080_CTRL_BUS_CLEAR_PEX_COUNTERS_PARAMS params = { 0 };
    params.pexCounterMask =
        LW2080_CTRL_BUS_PEX_COUNTER_L1_1_ENTRY_COUNT |
        LW2080_CTRL_BUS_PEX_COUNTER_L1_2_ENTRY_COUNT |
        LW2080_CTRL_BUS_PEX_COUNTER_L1_2_ABORT_COUNT |
        LW2080_CTRL_BUS_PEX_COUNTER_L1SS_TO_DEEP_L1_TIMEOUT_COUNT |
        LW2080_CTRL_BUS_PEX_COUNTER_L1_SHORT_DURATION_COUNT;

    CHECK_RC(LwRmPtr()->ControlBySubdevice(GetSubdevice(),
                                           LW2080_CTRL_CMD_BUS_CLEAR_PEX_COUNTERS,
                                           &params,
                                           sizeof(params)));

    // RM supports reading these counters but never added code for clearing them
    RegHal& regs = GetSubdevice()->Regs();
    UINT32 errorCounterReset = regs.Read32(MODS_XP_ERROR_COUNTER_RESET);
    regs.SetField(&errorCounterReset, MODS_XP_ERROR_COUNTER_RESET_L1_2_ABORT_COUNT_PENDING);
    regs.SetField(&errorCounterReset, MODS_XP_ERROR_COUNTER_RESET_L1_SHORT_DURATION_COUNT_PENDING);
    regs.SetField(&errorCounterReset,
                  MODS_XP_ERROR_COUNTER_RESET_L1_SUBSTATE_TO_DEEP_L1_TIMEOUT_COUNT_PENDING);
    regs.Write32(MODS_XP_ERROR_COUNTER_RESET, errorCounterReset);

    return rc;
}

//-----------------------------------------------------------------------------
//! \brief Retrieve the ASPM L1SS capability mask from the hardware
//!
//! \param pCap : Pointer to returned ASPM L1SS capability mask
//!
//! \return OK if successful, not OK otherwise
/* virtual */ RC MaxwellPcie::DoGetAspmL1SSCapability(UINT32 *pCap)
{
    MASSERT(pCap);
    const UINT32 addr = LW_XVE_L1_PM_SUBSTATES_CAP + DRF_BASE(LW_PCFG);
    UINT32 mask = GetSubdevice()->RegRd32(addr);

    *pCap = Pci::PM_SUB_DISABLED;
    if (FLD_TEST_DRF(_XVE, _L1_PM_SUBSTATES_CAP, _L1_PM_SUBSTATES_SUPPORTED, _ENABLE, mask))
    {
        if (FLD_TEST_DRF(_XVE, _L1_PM_SUBSTATES_CAP, _ASPM_L1_1_SUPPORTED, _ENABLE, mask))
            *pCap |= Pci::PM_SUB_L11;

        if (FLD_TEST_DRF(_XVE, _L1_PM_SUBSTATES_CAP, _ASPM_L1_2_SUPPORTED, _ENABLE, mask))
            *pCap |= Pci::PM_SUB_L12;
    }

    return OK;
}

/* virtual */ RC MaxwellPcie::DoGetAspmCyaL1SubState(UINT32 *pState)
{
    MASSERT(pState);
    UINT32 mask = GetSubdevice()->RegRd32(LW_XP_L1_SUBSTATE_4);

    *pState = Pci::PM_SUB_DISABLED;
    if (FLD_TEST_DRF(_XP, _L1_SUBSTATE_4, _CYA_ASPM_L1_1_ENABLED, _YES, mask))
        *pState |= Pci::PM_SUB_L11;

    if (FLD_TEST_DRF(_XP, _L1_SUBSTATE_4, _CYA_ASPM_L1_2_ENABLED, _YES, mask))
        *pState |= Pci::PM_SUB_L12;

    return OK;
}

/* virtual */ RC MaxwellPcie::DoGetAspmL1ssEnabled(UINT32 *pState)
{
    MASSERT(pState);
    *pState = Pci::PM_SUB_DISABLED;

    if (!Platform::IsVirtFunMode())
    {
        const RegHal& regs = GetSubdevice()->Regs();
        const UINT32  mask = regs.Read32(MODS_EP_PCFG_GPU_L1_PM_SS_CONTROL_1_REGISTER);

        if (regs.TestField(mask,
                           MODS_EP_PCFG_GPU_L1_PM_SS_CONTROL_1_REGISTER_ASPM_L11_ENABLE, 1))
        {
            *pState |= Pci::PM_SUB_L11;
        }

        if (regs.TestField(mask,
                           MODS_EP_PCFG_GPU_L1_PM_SS_CONTROL_1_REGISTER_ASPM_L12_ENABLE, 1))
        {
            *pState |= Pci::PM_SUB_L12;
        }
    }

    return OK;
}

/* virtual */ UINT32 MaxwellPcie::DoGetL1SubstateOffset()
{
    return LW_XVE_L1_PM_SUBSTATES_EXT_CAP;
}

/* virtual */ RC MaxwellPcie::DoGetLTREnabled(bool *pEnabled)
{
    MASSERT(pEnabled);

    UINT32 reg = GetSubdevice()->RegRd32(DRF_BASE(LW_PCFG) + LW_XVE_DEVICE_CAPABILITIES_2);
    if (FLD_TEST_DRF(_XVE, _DEVICE_CAPABILITIES_2, _LATENCY_TOLERANCE_REPORTING, _SUPPORTED, reg))
    {
        reg = GetSubdevice()->RegRd32(DRF_BASE(LW_PCFG) + LW_XVE_DEVICE_CONTROL_STATUS_2);
        *pEnabled = FLD_TEST_DRF(_XVE, _DEVICE_CONTROL_STATUS_2, _LTR_ENABLE, _ENABLED, reg);
        return OK;
    }
    else
    {
        return RC::UNSUPPORTED_HARDWARE_FEATURE;
    }
}
