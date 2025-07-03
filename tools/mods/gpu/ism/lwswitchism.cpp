/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2016-2022 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#include "core/include/utility.h"
#include "gpu/ism/lwswitchism.h"
#include "core/include/massert.h"
#include "gpu/include/testdevice.h"
#include "lwmisc.h"

#if LWCFG(GLOBAL_LWSWITCH_IMPL_SVNP01)
#include "svnp01_ism.cpp"
#endif
#if LWCFG(GLOBAL_LWSWITCH_IMPL_LR10)
#include "lr10_ism.cpp"
#endif

LwSwitchIsm::LwSwitchIsm(TestDevice* pTestDev, vector<Ism::IsmChain> *pTable)
    : Ism(pTable), m_pTestDev(pTestDev)
{
    MASSERT(pTestDev);
    if (GetNumISMChains() == 0)
    {
        Printf(Tee::PriHigh, "Warning: ISM Table has not been populated\n");
    }
}

//------------------------------------------------------------------------------
/* static */
vector<Ism::IsmChain>* LwSwitchIsm::GetTable(Device::LwDeviceId devId)
{
#if LWCFG(GLOBAL_LWSWITCH_IMPL_SVNP01)
    if (devId == Device::SVNP01)
    {
        if (SVNP01SpeedoChains.empty())
        {
            InitializeSVNP01SpeedoChains();
        }
        return &SVNP01SpeedoChains;
    }
#endif
#if LWCFG(GLOBAL_LWSWITCH_IMPL_LR10)
    if (devId == Device::LR10)
    {
        if (LR10SpeedoChains.empty())
        {
            InitializeLR10SpeedoChains();
        }
        return &LR10SpeedoChains;
    }
#endif
    return nullptr;
}

//------------------------------------------------------------------------------
// Configure the individual bits for a specific ISM using parameters supplied.
RC LwSwitchIsm::ConfigureISMBits
(
    vector<UINT64> &IsmBits
    ,IsmInfo &info
    ,IsmTypes ismType
    ,UINT32 iddq            // 0 = powerUp, 1 = powerDown
    ,UINT32 enable          // 0 = disable, 1 = enable
    ,UINT32 durClkCycles    // how long to run the experiment
    ,UINT32 flags           // Special handling flags
)
{
    RegHal &regs = m_pTestDev->Regs();
    const UINT32 state = (flags & STATE_MASK);

    switch (ismType)
    {
        case LW_ISM_ROSC_bin_v1:
            IsmBits.resize(1, 0);
            // Note we don't need state 2 so mimic state 1 so the calling routine doesn't actually
            // send the 2nd WriteHost2Jtag request.
            if ((state == STATE_1) || (state == STATE_2))
            {
                // State 1: Enable Jtag override
                IsmBits[0] =
                    regs.SetField64(MODS_ISM_ROSC_BIN_V1_JTAG_OVERRIDE, 1) |
                    regs.SetField64(MODS_ISM_ROSC_BIN_V1_IDDQ,          iddq);
            }
            if (state == STATE_3)
            {
                // State 1: Power up & enable to start counting
                IsmBits[0] = (
                   regs.SetField64(MODS_ISM_ROSC_BIN_V1_JTAG_OVERRIDE,  1)             |
                   regs.SetField64(MODS_ISM_ROSC_BIN_V1_SRC_SEL,        info.oscIdx)   |
                   regs.SetField64(MODS_ISM_ROSC_BIN_V1_OUT_DIV,        info.outDiv)   |
                   regs.SetField64(MODS_ISM_ROSC_BIN_V1_IDDQ,           iddq)          |
                   regs.SetField64(MODS_ISM_ROSC_BIN_V1_LOCAL_DURATION, durClkCycles)  |
                   regs.SetField64(MODS_ISM_ROSC_BIN_V1_EN,
                                           (enable || (flags & KEEP_ACTIVE)) ? 1 : 0)
                   );
            }
            if (state == STATE_4)
            {
                // State 4: Power down
                IsmBits[0] = regs.SetField64(MODS_ISM_ROSC_BIN_V1_IDDQ, iddq);
            }
            break;
        case LW_ISM_ROSC_bin_aging_v1:
            IsmBits.resize(2, 0);
            IsmBits[0] = (
                regs.SetField64(MODS_ISM_ROSC_BIN_AGING_V1_SRC_SEL, info.oscIdx) |
                regs.SetField64(MODS_ISM_ROSC_BIN_AGING_V1_OUT_DIV, info.outDiv) |
                regs.SetField64(MODS_ISM_ROSC_BIN_AGING_V1_MODE, info.mode) |
                regs.SetField64(MODS_ISM_ROSC_BIN_AGING_V1_IDDQ, !enable) | // 0=enable, 1=disable
                regs.SetField64(MODS_ISM_ROSC_BIN_AGING_V1_LOCAL_DURATION, durClkCycles) |
                regs.SetField64(MODS_ISM_ROSC_BIN_AGING_V1_PG_DIS, !iddq) | // 1=pwrUp, 0=pwrDn
                regs.SetField64(MODS_ISM_ROSC_BIN_AGING_V1_RO_EN, info.roEnable) |
                regs.SetField64(MODS_ISM_ROSC_BIN_AGING_V1_DCD_EN, info.dcdEn) |
                regs.SetField64(MODS_ISM_ROSC_BIN_AGING_V1_OUT_SEL, info.outSel) |
                regs.SetField64(MODS_ISM_ROSC_BIN_AGING_V1_COUNT_LOW, 0)
                );
            IsmBits[1] = (
                regs.SetField64(MODS_ISM_ROSC_BIN_AGING_V1_COUNT_HIGH, 0) |
                regs.SetField64(MODS_ISM_ROSC_BIN_AGING_V1_DCD_HI_COUNT, 0) |
                regs.SetField64(MODS_ISM_ROSC_BIN_AGING_V1_READY, 0)
                );
            break;

        default:
            Printf(Tee::PriError, "Unknown ISM type!\n");
            return RC::SOFTWARE_ERROR;
    }
    return OK;
}

//------------------------------------------------------------------------------
// Dump the ISM bits for a given ISM type.
RC LwSwitchIsm::DumpISMBits(UINT32 ismType, vector<UINT64>& ismBits)
{
    RegHal &regs = m_pTestDev->Regs();
    switch (ismType)
    {
        case LW_ISM_ROSC_bin_v1:
            Printf(Tee::PriNormal,
                "{ rosc_bin_v1:%d.%d.%d.%d.%d.%d.%d.%d }",
                (int)regs.GetField64(ismBits[0], MODS_ISM_ROSC_BIN_V1_READY),
                (int)regs.GetField64(ismBits[0], MODS_ISM_ROSC_BIN_V1_COUNT),
                (int)regs.GetField64(ismBits[0], MODS_ISM_ROSC_BIN_V1_EN),
                (int)regs.GetField64(ismBits[0], MODS_ISM_ROSC_BIN_V1_LOCAL_DURATION),
                (int)regs.GetField64(ismBits[0], MODS_ISM_ROSC_BIN_V1_JTAG_OVERRIDE),
                (int)regs.GetField64(ismBits[0], MODS_ISM_ROSC_BIN_V1_IDDQ),
                (int)regs.GetField64(ismBits[0], MODS_ISM_ROSC_BIN_V1_OUT_DIV),
                (int)regs.GetField64(ismBits[0], MODS_ISM_ROSC_BIN_V1_SRC_SEL));
            break;
        case LW_ISM_ROSC_bin_aging_v1:
            Printf(Tee::PriNormal,
                "{rosc_bin_aging_v1:%d.%d.%d.%d.%d.%d.%d.%d.%d.%d.%d.%d}",
                (int)regs.GetField64(ismBits[1], MODS_ISM_ROSC_BIN_AGING_V1_READY),
                (int)regs.GetField64(ismBits[1], MODS_ISM_ROSC_BIN_AGING_V1_DCD_HI_COUNT),
                (((int)regs.GetField64(ismBits[1], MODS_ISM_ROSC_BIN_AGING_V1_COUNT_HIGH) << 2) |
                  (int)regs.GetField64(ismBits[0], MODS_ISM_ROSC_BIN_AGING_V1_COUNT_LOW)),
                (int)regs.GetField64(ismBits[0], MODS_ISM_ROSC_BIN_AGING_V1_OUT_SEL),
                (int)regs.GetField64(ismBits[0], MODS_ISM_ROSC_BIN_AGING_V1_DCD_EN),
                (int)regs.GetField64(ismBits[0], MODS_ISM_ROSC_BIN_AGING_V1_RO_EN),
                (int)regs.GetField64(ismBits[0], MODS_ISM_ROSC_BIN_AGING_V1_PG_DIS),
                (int)regs.GetField64(ismBits[0], MODS_ISM_ROSC_BIN_AGING_V1_LOCAL_DURATION),
                (int)regs.GetField64(ismBits[0], MODS_ISM_ROSC_BIN_AGING_V1_MODE),
                (int)regs.GetField64(ismBits[0], MODS_ISM_ROSC_BIN_AGING_V1_IDDQ),
                (int)regs.GetField64(ismBits[0], MODS_ISM_ROSC_BIN_AGING_V1_OUT_DIV),
                (int)regs.GetField64(ismBits[0], MODS_ISM_ROSC_BIN_AGING_V1_SRC_SEL));
            break;

        default:
            Printf(Tee::PriError, "Unknown ISM type!\n");
            return RC::SOFTWARE_ERROR;
    }
    return OK;
}

//------------------------------------------------------------------------------
// Extract the raw count for a given ISM type.
UINT64 LwSwitchIsm::ExtractISMRawCount(UINT32 ismType, vector<UINT64>& ismBits)
{
    RegHal &regs = m_pTestDev->Regs();
    switch (ismType)
    {
        case LW_ISM_ROSC_bin_v1:
            return regs.GetField64(ismBits[0], MODS_ISM_ROSC_BIN_V1_COUNT);
        case LW_ISM_ROSC_bin_aging_v1:
            // Bits 1:0 come from COUNT_LOW (63:62)
            // Bits 15:2 come from COUNT_HI (77:64)
            // Bits 31:16 come from DCD_HI_COUNT (93:78)
            return (regs.GetField64(ismBits[0], MODS_ISM_ROSC_BIN_AGING_V1_COUNT_LOW) |
                   (regs.GetField64(ismBits[1], MODS_ISM_ROSC_BIN_AGING_V1_COUNT_HIGH) << 2) |
                   (regs.GetField64(ismBits[1], MODS_ISM_ROSC_BIN_AGING_V1_DCD_HI_COUNT) << 16));

        default:
            Printf(Tee::PriError, "Unknown ISM type!\n");
            return 0;
    }
}

//------------------------------------------------------------------------------
// Return the number of bits for a given ISM type.
UINT32 LwSwitchIsm::GetISMSize(IsmTypes ismType)
{
    RegHal &regs = m_pTestDev->Regs();
    switch (ismType)
    {
        case LW_ISM_aging:
            return regs.LookupAddress(MODS_ISM_AGING_WIDTH);
        case LW_ISM_aging_v4:
            return regs.LookupAddress(MODS_ISM_AGING_V4_WIDTH);
        case LW_ISM_ctrl_v3:
            return regs.LookupAddress(MODS_ISM_CTRL3_WIDTH);
        case LW_ISM_ROSC_bin_v1:
            return regs.LookupAddress(MODS_ISM_ROSC_BIN_V1_WIDTH);
        case LW_ISM_ROSC_bin_aging:
            return regs.LookupAddress(MODS_ISM_ROSC_BIN_AGING_WIDTH);
        case LW_ISM_ROSC_bin_aging_v1:
            return regs.LookupAddress(MODS_ISM_ROSC_BIN_AGING_V1_WIDTH);
        case LW_ISM_NMEAS_lite_v3          : return regs.LookupAddress(MODS_ISM_NMEAS_LITE_V3_WIDTH);


        default:
            Printf(Tee::PriError, "Unknown ISM type! Returning zero for ISM size!\n");
            return 0;
    }
}

//------------------------------------------------------------------------------
// Pass-thru mechanism to get access to the JTag chains using the lwswitch.
RC LwSwitchIsm::WriteHost2Jtag
(
    UINT32 chiplet,
    UINT32 instruction,
    UINT32 chainLength,
    const vector<UINT32> &inputData
)
{
    if (m_pTestDev == nullptr)
    {
        Printf(Tee::PriError, "m_pTestDev is null\n");
        return RC::SOFTWARE_ERROR;
    }
    return m_pTestDev->WriteHost2Jtag(chiplet, instruction, chainLength, inputData);
}

//------------------------------------------------------------------------------
// Pass-thru mechanism to get access to the JTag chains using the lwswitch.
RC LwSwitchIsm::ReadHost2Jtag
(
    UINT32 chiplet,
    UINT32 instruction,
    UINT32 chainLength,
    vector<UINT32> *pResult
)
{
    if (m_pTestDev == nullptr)
    {
        Printf(Tee::PriError, "m_pTestDev is null\n");
        return RC::SOFTWARE_ERROR;
    }
    return m_pTestDev->ReadHost2Jtag(chiplet, instruction, chainLength, pResult);
}

//------------------------------------------------------------------------------
RC LwSwitchIsm::GetISMClkFreqKhz
(
    UINT32* clkSrcFreqKHz
)
{
    *clkSrcFreqKHz = 25000; // 25 MHz
    return OK;
}

//------------------------------------------------------------------------------
RC LwSwitchIsm::PollISMCtrlComplete
(
    UINT32 *pComplete
)
{
    return RC::UNSUPPORTED_FUNCTION;
}

//------------------------------------------------------------------------------
UINT32 LwSwitchIsm::IsExpComplete
(
    UINT32 ismType, //Ism::IsmTypes
    INT32 offset,
    const vector<UINT32> &JtagArray
)
{
    Printf(Tee::PriError, "IsExpComplete() not implemented\n");
    return 0;
}

//------------------------------------------------------------------------------
RC LwSwitchIsm::TriggerISMExperiment()
{
    return RC::UNSUPPORTED_FUNCTION;
}

//------------------------------------------------------------------------------
RC LwSwitchIsm::ConfigureISMCtrl
(
    const IsmChain *pChain          // The chain with the master controller
    ,vector<UINT32>  &jtagArray     // Vector of bits for the chain
    ,bool            bEnable        // Enable/Disable the controller
    ,UINT32          durClkCycles   // How long to run the experiment
    ,UINT32          delayClkCycles // How long to wait after the trigger
                                    // before taking samples
    ,UINT32          trigger        // Type of trigger to use
    ,INT32           loopMode       // if true allows continuous looping of the experiments
    ,INT32           haltMode       // if true allows halting of a continuous experiments
    ,INT32           ctrlSrcSel     // 1 = jtag, 0 = priv access
)
{
    return RC::UNSUPPORTED_FUNCTION;
}

//------------------------------------------------------------------------------
UINT32 LwSwitchIsm::GetISMDurClkCycles()
{
    Printf(Tee::PriError, "GetISMDurClkCycles() not implemented\n");
    return 0;
}

//------------------------------------------------------------------------------
INT32 LwSwitchIsm::GetISMMode(PART_TYPES type)
{
    Printf(Tee::PriError, "GetISMMode() not implemented\n");
    return 0;
}

//------------------------------------------------------------------------------
INT32 LwSwitchIsm::GetISMOutputDivider(PART_TYPES partType)
{
    Printf(Tee::PriError, "GetISMOutputDivider() not implemented\n");
    return 0;
}

//------------------------------------------------------------------------------
INT32 LwSwitchIsm::GetISMOutDivForFreqCalc
(
    PART_TYPES PartType,
    INT32 outDiv
)
{
    return outDiv;
}

//------------------------------------------------------------------------------
INT32 LwSwitchIsm::GetISMRefClkSel(PART_TYPES partType)
{
    Printf(Tee::PriError, "GetISMRefClkSel() not implemented\n");
    return 0;
}
//------------------------------------------------------------------------------
INT32 LwSwitchIsm::GetOscIdx(PART_TYPES type)
{
    Printf(Tee::PriError, "GetOscIdx() not implemented\n");
    return 0;
}
