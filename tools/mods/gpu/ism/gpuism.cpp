/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2012-2022 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */
#include "core/include/platform.h"
#include "core/include/utility.h"
#include "gpu/include/gpuism.h"
#include "core/include/massert.h"
#include "gpu/include/gpusbdev.h"
#include "core/include/ismspeedo.h"

//------------------------------------------------------------------------------
GpuIsm::GpuIsm(GpuSubdevice *pGpu, vector<IsmChain> *pTable)
    : Ism(pTable), m_pGpu(pGpu)
{
    MASSERT(pGpu);
    if (GetNumISMChains() == 0)
    {
        Printf(Tee::PriNormal, "Warning %s ISM Table has not been populated!\n",
               pGpu->GetClassName().c_str());
    }
}

//------------------------------------------------------------------------------
// Configure the individual bits for a specific ISM using parameters supplied.
// We handle the most common cases here and call the derived class to handle the
// uncommon types.
RC GpuIsm::ConfigureISMBits
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
    RegHal &regs = GetGpuSub()->Regs();
    UINT32 state = (flags & STATE_MASK);

    IsmBits.resize(CallwlateVectorSize(ismType, 64), 0);
    
    switch (ismType)
    {
        case LW_ISM_MINI_1clk_no_ext_ro:
            // same bit definition as 1clk except can't set SRC_SEL = 7
            MASSERT(info.oscIdx != 7);
            // [[fallthrough]] @c++17

        case LW_ISM_MINI_1clk     :
            IsmBits[0] = (
                regs.SetField64(MODS_ISM_MINI_1CLK_SRC_SEL, info.oscIdx) |
                regs.SetField64(MODS_ISM_MINI_1CLK_OUT_DIV, info.outDiv) |
                regs.SetField64(MODS_ISM_MINI_1CLK_MODE,    info.mode)   |
                regs.SetField64(MODS_ISM_MINI_1CLK_INIT,    info.init)   |
                regs.SetField64(MODS_ISM_MINI_1CLK_FINIT,   info.finit)  |
                regs.SetField64(MODS_ISM_MINI_1CLK_IDDQ,    iddq)
            );
            break;

        case LW_ISM_MINI_1clk_no_ext_ro_dbg:
            //same as 1clk_dbg except can't set SRC_SEL = 7
            MASSERT(info.oscIdx != 7);
            // [[fallthrough]] @c++17

        case LW_ISM_MINI_1clk_dbg :
            IsmBits[0] = (
                regs.SetField64(MODS_ISM_MINI_1CLK_DBG_SRC_SEL, info.oscIdx) |
                regs.SetField64(MODS_ISM_MINI_1CLK_DBG_OUT_DIV, info.outDiv) |
                regs.SetField64(MODS_ISM_MINI_1CLK_DBG_MODE,    info.mode)   |
                regs.SetField64(MODS_ISM_MINI_1CLK_DBG_INIT,    info.init)   |
                regs.SetField64(MODS_ISM_MINI_1CLK_DBG_FINIT,   info.finit)  |
                regs.SetField64(MODS_ISM_MINI_1CLK_DBG_IDDQ,    iddq)
            );
            break;

        case LW_ISM_MINI_2clk_no_ext_ro :
            // same bit definition as 2clk except can't set SRC_SEL = 7
            MASSERT(info.oscIdx != 7);
            // [[fallthrough]] @c++17

        case LW_ISM_MINI_2clk     :
            IsmBits[0] = (
                regs.SetField64(MODS_ISM_MINI_2CLK_SRC_SEL,    info.oscIdx)    |
                regs.SetField64(MODS_ISM_MINI_2CLK_OUT_DIV,    info.outDiv)    |
                regs.SetField64(MODS_ISM_MINI_2CLK_MODE,       info.mode)      |
                regs.SetField64(MODS_ISM_MINI_2CLK_INIT,       info.init)      |
                regs.SetField64(MODS_ISM_MINI_2CLK_FINIT,      info.finit)     |
                regs.SetField64(MODS_ISM_MINI_2CLK_REFCLK_SEL, info.refClkSel) |
                regs.SetField64(MODS_ISM_MINI_2CLK_IDDQ,       iddq)
            );
            break;

        case LW_ISM_MINI_2clk_dbg :
            IsmBits[0] = (
                regs.SetField64(MODS_ISM_MINI_2CLK_DBG_SRC_SEL,    info.oscIdx)    |
                regs.SetField64(MODS_ISM_MINI_2CLK_DBG_OUT_DIV,    info.outDiv)    |
                regs.SetField64(MODS_ISM_MINI_2CLK_DBG_MODE,       info.mode)      |
                regs.SetField64(MODS_ISM_MINI_2CLK_DBG_INIT,       info.init)      |
                regs.SetField64(MODS_ISM_MINI_2CLK_DBG_FINIT,      info.finit)     |
                regs.SetField64(MODS_ISM_MINI_2CLK_DBG_REFCLK_SEL, info.refClkSel) |
                regs.SetField64(MODS_ISM_MINI_2CLK_DBG_IDDQ,       iddq)
            );
            break;

        // These ISMs have their own control/enable logic
        case LW_ISM_ROSC_comp     :
            IsmBits[0] = (
              regs.SetField64(MODS_ISM_ROSC_COMP_SRC_SEL, info.oscIdx) |
              regs.SetField64(MODS_ISM_ROSC_COMP_OUT_DIV, info.outDiv) |
              regs.SetField64(MODS_ISM_ROSC_COMP_MODE,    info.mode)   |
              regs.SetField64(MODS_ISM_ROSC_COMP_IDDQ,    iddq)        |
              regs.SetField64(MODS_ISM_ROSC_COMP_LOCAL_DURATION, durClkCycles)
            );
            break;

        case LW_ISM_ROSC_comp_v1   :
            // Note: We don't need state 2 so mimic state 1 to prevent the calling routine from
            // sending the 2nd WriteHost2Jtag request.
            if ((state == STATE_1)  ||  // Transition to PowerOn & disabled.
                (state == STATE_2)  ||  // Transition to PowerOn, disabled & configured
                (state == STATE_4))     // Transition to PowerOff or PowerOn(If KeepActive is set)
            {
                IsmBits[0] = 
                    regs.SetField64(MODS_ISM_ROSC_COMP_V1_JTAG_OVERRIDE, 1) |
                    regs.SetField64(MODS_ISM_ROSC_COMP_V1_IDDQ, iddq);
            }
            else if (flags == STATE_3)  // Transition to PowerOn, enabled & configured
            {
                IsmBits[0] = (
                    regs.SetField64(MODS_ISM_ROSC_COMP_V1_JTAG_OVERRIDE, 1)     |
                    regs.SetField64(MODS_ISM_ROSC_COMP_V1_SRC_SEL, info.oscIdx) |
                    regs.SetField64(MODS_ISM_ROSC_COMP_V1_OUT_DIV, info.outDiv) |
                    regs.SetField64(MODS_ISM_ROSC_COMP_V1_IDDQ, iddq)           |
                    regs.SetField64(MODS_ISM_ROSC_COMP_V1_EN, enable)           |
                    regs.SetField64(MODS_ISM_ROSC_COMP_V1_LOCAL_DURATION, durClkCycles));
            }
            break;

        case LW_ISM_ROSC_comp_v2   :
            // Note: We don't need state 2 so mimic state 1 to prevent the calling routine from
            // sending the 2nd WriteHost2Jtag request.
            if ((state == STATE_1)  ||  // Transition to PowerOn & disabled.
                (state == STATE_2))     // Transition to PowerOn, disabled & configured
            {
                IsmBits[0] = 
                    regs.SetField64(MODS_ISM_ROSC_COMP_V2_JTAG_OVERRIDE, 1) |
                    regs.SetField64(MODS_ISM_ROSC_COMP_V2_IDDQ, iddq);
            }
            else if (flags == STATE_3)  // Transition to PowerOn, enabled & configured
            {
                IsmBits[0] = (
                    regs.SetField64(MODS_ISM_ROSC_COMP_V2_JTAG_OVERRIDE, 1)     |
                    regs.SetField64(MODS_ISM_ROSC_COMP_V2_SRC_SEL, info.oscIdx) |
                    regs.SetField64(MODS_ISM_ROSC_COMP_V2_OUT_DIV, info.outDiv) |
                    regs.SetField64(MODS_ISM_ROSC_COMP_V2_IDDQ, iddq)           |
                    regs.SetField64(MODS_ISM_ROSC_COMP_V2_EN, enable)           |
                    regs.SetField64(MODS_ISM_ROSC_COMP_V2_LOCAL_DURATION, durClkCycles));
            }
            else if (flags == STATE_4) //Transition to PowerOff or PowerOn(If KeepActive is set)
            {
                IsmBits[0] = 
                    regs.SetField64(MODS_ISM_ROSC_COMP_V2_IDDQ, iddq);
            }
            break;

        case LW_ISM_ROSC_bin_metal:
            IsmBits[0] = (
              regs.SetField64(MODS_ISM_ROSC_BIN_METAL_SRC_SEL, info.oscIdx) |
              regs.SetField64(MODS_ISM_ROSC_BIN_METAL_OUT_DIV, info.outDiv) |
              regs.SetField64(MODS_ISM_ROSC_BIN_METAL_MODE,    info.mode)   |
              regs.SetField64(MODS_ISM_ROSC_BIN_METAL_IDDQ,    iddq)        |
              regs.SetField64(MODS_ISM_ROSC_BIN_METAL_LOCAL_DURATION, durClkCycles)
            );
            break;

        case LW_ISM_VNSENSE_ddly  :
            IsmBits[0] = (
              regs.SetField64(MODS_ISM_VNSENSE_DDLY_EN,      enable)      |
              regs.SetField64(MODS_ISM_VNSENSE_DDLY_IDDQ,    iddq)        |
              regs.SetField64(MODS_ISM_VNSENSE_DDLY_OUT_DIV, info.outDiv) |
              regs.SetField64(MODS_ISM_VNSENSE_DDLY_MODE,    info.mode)   |
              regs.SetField64(MODS_ISM_VNSENSE_DDLY_INIT,    info.init)   |
              regs.SetField64(MODS_ISM_VNSENSE_DDLY_FINIT,   info.finit)  |
              regs.SetField64(MODS_ISM_VNSENSE_DDLY_LOCAL_DURATION, durClkCycles)
            );
            break;

        case LW_ISM_ROSC_bin: // different implementations for GF117 & GK10x
            IsmBits[0] = (
              regs.SetField64(MODS_ISM_ROSC_BIN_SRC_SEL,  info.oscIdx) |
              regs.SetField64(MODS_ISM_ROSC_BIN_OUT_DIV,  info.outDiv) |
              regs.SetField64(MODS_ISM_ROSC_BIN_MODE,     info.mode)   |
              regs.SetField64(MODS_ISM_ROSC_BIN_IDDQ,     iddq)        |
              regs.SetField64(MODS_ISM_ROSC_BIN_LOCAL_DURATION, durClkCycles)
            );
            break;

        case LW_ISM_ROSC_bin_v1:
            IsmBits.resize(1, 0);
            IsmBits[0] = (
                regs.SetField64(MODS_ISM_ROSC_BIN_V1_SRC_SEL,        info.oscIdx)   |
                regs.SetField64(MODS_ISM_ROSC_BIN_V1_OUT_DIV,        info.outDiv)   |
                regs.SetField64(MODS_ISM_ROSC_BIN_V1_MODE,           info.mode)     |
                regs.SetField64(MODS_ISM_ROSC_BIN_V1_IDDQ,           iddq)          |
                regs.SetField64(MODS_ISM_ROSC_BIN_V1_LOCAL_DURATION, durClkCycles)  |
                regs.SetField64(MODS_ISM_ROSC_BIN_V1_DCD_EN, 
                                (enable || (flags & KEEP_ACTIVE)) ? info.dcdEn : 0) |
                regs.SetField64(MODS_ISM_ROSC_BIN_V1_OUT_SEL,        info.outSel)   |
                regs.SetField64(MODS_ISM_ROSC_BIN_V1_COUNT,          0)             |
                regs.SetField64(MODS_ISM_ROSC_BIN_V1_DCD_HI_COUNT,   0)             |
                regs.SetField64(MODS_ISM_ROSC_BIN_V1_READY,          0)
            );
            break;

        case LW_ISM_ROSC_bin_v2:
            IsmBits.resize(1, 0);
            // Note we don't need state 2 so mimic state 1 so the calling routine doesn't actually
            // send the 2nd WriteHost2Jtag request.
            if ((state == STATE_1) || (state == STATE_2))
            {
                // State 1: Enable Jtag override
                IsmBits[0] =
                    regs.SetField64(MODS_ISM_ROSC_BIN_V2_JTAG_OVERRIDE, 1) |
                    regs.SetField64(MODS_ISM_ROSC_BIN_V2_IDDQ, iddq);
            }
            if (state == STATE_3)
            {
                // State 1: Power up & enable to start counting
                IsmBits[0] = (
                   regs.SetField64(MODS_ISM_ROSC_BIN_V2_JTAG_OVERRIDE, 1) |
                   regs.SetField64(MODS_ISM_ROSC_BIN_V2_SRC_SEL,        info.oscIdx)   |
                   regs.SetField64(MODS_ISM_ROSC_BIN_V2_OUT_DIV,        info.outDiv)   |
                   regs.SetField64(MODS_ISM_ROSC_BIN_V2_IDDQ,           iddq)          |
                   regs.SetField64(MODS_ISM_ROSC_BIN_V2_LOCAL_DURATION, durClkCycles)  |
                   regs.SetField64(MODS_ISM_ROSC_BIN_V2_EN, 
                                (enable || (flags & KEEP_ACTIVE)) ? 1 : 0)
                   );
            }
            if (state == STATE_4)
            {
                // State 4: Power down
                IsmBits[0] = regs.SetField64(MODS_ISM_ROSC_BIN_V2_IDDQ, iddq);
            }
            break;

        case LW_ISM_ROSC_bin_v3:
            IsmBits.resize(1, 0);
            // Note we don't need state 2 so mimic state 1 so the calling routine doesn't actually
            // send the 2nd WriteHost2Jtag request.
            if ((state == STATE_1) || (state == STATE_2))
            {
                // State 1: Enable Jtag override
                IsmBits[0] =
                    regs.SetField64(MODS_ISM_ROSC_BIN_V3_JTAG_OVERRIDE, 1) |
                    regs.SetField64(MODS_ISM_ROSC_BIN_V3_IDDQ, iddq);
            }
            if (state == STATE_3)
            {
                // State 1: Power up & enable to start counting
                IsmBits[0] = (
                   regs.SetField64(MODS_ISM_ROSC_BIN_V3_JTAG_OVERRIDE, 1) |
                   regs.SetField64(MODS_ISM_ROSC_BIN_V3_SRC_SEL,        info.oscIdx)   |
                   regs.SetField64(MODS_ISM_ROSC_BIN_V3_OUT_DIV,        info.outDiv)   |
                   regs.SetField64(MODS_ISM_ROSC_BIN_V3_OUT_SEL,        info.outSel)   |
                   regs.SetField64(MODS_ISM_ROSC_BIN_V3_IDDQ,           iddq)          |
                   regs.SetField64(MODS_ISM_ROSC_BIN_V3_LOCAL_DURATION, durClkCycles)  |
                   regs.SetField64(MODS_ISM_ROSC_BIN_V3_EN,
                                (enable || (flags & KEEP_ACTIVE)) ? 1 : 0)
                   );
            }
            if (state == STATE_4)
            {
                // State 4: Power down
                IsmBits[0] = regs.SetField64(MODS_ISM_ROSC_BIN_V3_IDDQ, iddq);
            }
            break;

        case LW_ISM_ROSC_bin_v4:
            IsmBits.resize(1, 0);
            // Note we don't need state 2 so mimic state 1 so the calling routine doesn't actually
            // send the 2nd WriteHost2Jtag request.
            if ((state == STATE_1) || (state == STATE_2))
            {
                // State 1: Enable Jtag override
                IsmBits[0] =
                    regs.SetField64(MODS_ISM_ROSC_BIN_V3_JTAG_OVERRIDE, 1) |
                    regs.SetField64(MODS_ISM_ROSC_BIN_V3_IDDQ, iddq);
            }
            if (state == STATE_3)
            {
                // State 1: Power up & enable to start counting
                IsmBits[0] = (
                   regs.SetField64(MODS_ISM_ROSC_BIN_V4_JTAG_OVERRIDE, 1) |
                   regs.SetField64(MODS_ISM_ROSC_BIN_V4_SRC_SEL,        info.oscIdx)   |
                   regs.SetField64(MODS_ISM_ROSC_BIN_V4_OUT_DIV,        info.outDiv)   |
                   regs.SetField64(MODS_ISM_ROSC_BIN_V4_OUT_SEL,        info.outSel)   |
                   regs.SetField64(MODS_ISM_ROSC_BIN_V4_DELAY_SEL,      info.delaySel) |
                   regs.SetField64(MODS_ISM_ROSC_BIN_V4_IDDQ,           iddq)          |
                   regs.SetField64(MODS_ISM_ROSC_BIN_V4_LOCAL_DURATION, durClkCycles)  |
                   regs.SetField64(MODS_ISM_ROSC_BIN_V4_EN,
                                (enable || (flags & KEEP_ACTIVE)) ? 1 : 0)
                   );
            }
            if (state == STATE_4)
            {
                // State 4: Power down
                IsmBits[0] = regs.SetField64(MODS_ISM_ROSC_BIN_V4_IDDQ, iddq);
            }
            break;

        case LW_ISM_imon:
            IsmBits.resize(1, 0);
            // Note we don't need state 2 so mimic state 1 so the calling routine doesn't actually
            // send the 2nd WriteHost2Jtag request.
            if ((state == STATE_1) || (state == STATE_2))
            {
                // State 1: Enable Jtag override
                IsmBits[0] =
                    regs.SetField64(MODS_ISM_IMON_JTAG_OVERRIDE, 1) |
                    regs.SetField64(MODS_ISM_IMON_IDDQ, iddq);
            }
            if (state == STATE_3)
            {
                // State 1: Power up & enable to start counting
                IsmBits[0] = (
                   regs.SetField64(MODS_ISM_IMON_JTAG_OVERRIDE, 1) |
                   regs.SetField64(MODS_ISM_IMON_SRC_SEL,        info.oscIdx)   |
                   regs.SetField64(MODS_ISM_IMON_OUT_DIV,        info.outDiv)   |
                   regs.SetField64(MODS_ISM_IMON_ANALOG_SEL,     info.analogSel)   |
                   regs.SetField64(MODS_ISM_IMON_IDDQ,           iddq)          |
                   regs.SetField64(MODS_ISM_IMON_LOCAL_DURATION, durClkCycles)  |
                   regs.SetField64(MODS_ISM_IMON_EN,
                                (enable || (flags & KEEP_ACTIVE)) ? 1 : 0)
                   );
            }
            if (state == STATE_4)
            {
                // State 4: Power down
                IsmBits[0] = regs.SetField64(MODS_ISM_IMON_IDDQ, iddq);
            }
            break;

        case LW_ISM_imon_v2:
            IsmBits.resize(2, 0);
            // Note we don't need state 2 so mimic state 1 so the calling routine doesn't actually
            // send the 2nd WriteHost2Jtag request.
            if ((state == STATE_1) || (state == STATE_2))
            {
                // State 1: Enable Jtag override
                IsmBits[0] = (regs.SetField64(MODS_ISM_IMON_V2_IDDQ, iddq) |
                              regs.SetField64(MODS_ISM_IMON_V2_JTAG_OVERRIDE, 1));
            }
            if (state == STATE_3)
            {
                // State 1: Power up & enable to start counting
                IsmBits[0] = (
                   regs.SetField64(MODS_ISM_IMON_V2_IDDQ,           iddq)           |
                   regs.SetField64(MODS_ISM_IMON_V2_OUT_DIV,        info.outDiv)    |
                   regs.SetField64(MODS_ISM_IMON_V2_SRC_SEL,        info.oscIdx)    |
                   regs.SetField64(MODS_ISM_IMON_V2_ANALOG_SEL,     info.analogSel) |
                   regs.SetField64(MODS_ISM_IMON_V2_EN,
                                (enable || (flags & KEEP_ACTIVE)) ? 1 : 0)          |
                   regs.SetField64(MODS_ISM_IMON_V2_SPARE_IN,       info.spareIn)   |
                   regs.SetField64(MODS_ISM_IMON_V2_LOCAL_DURATION, durClkCycles)   |
                   regs.SetField64(MODS_ISM_IMON_V2_JTAG_OVERRIDE,  1));
                IsmBits[1] = 
                   regs.SetField64(MODS_ISM_IMON_V2_SPARE_IN,       info.spareOut);
            }
            if (state == STATE_4)
            {
                // State 4: Power down
                IsmBits[0] = regs.SetField64(MODS_ISM_IMON_V2_IDDQ, iddq);
            }
            break;

        case LW_ISM_MINI_2clk_v2  :
            // Note: This macro has several new bits but we don't have an interface
            // that lwrrently exposes them. For reference see GP107's dev_ism.h file.
            IsmBits[0] = (
                regs.SetField64(MODS_ISM_MINI_2CLK_V2_SRC_SEL,    info.oscIdx)    |
                regs.SetField64(MODS_ISM_MINI_2CLK_V2_OUT_DIV,    info.outDiv)    |
                regs.SetField64(MODS_ISM_MINI_2CLK_V2_MODE,       info.mode)      |
                regs.SetField64(MODS_ISM_MINI_2CLK_V2_INIT,       info.init)      |
                regs.SetField64(MODS_ISM_MINI_2CLK_V2_FINIT,      info.finit)     |
                regs.SetField64(MODS_ISM_MINI_2CLK_V2_REFCLK_SEL, info.refClkSel) |
                regs.SetField64(MODS_ISM_MINI_2CLK_V2_IDDQ,       iddq)
            );
            break;
        case LW_ISM_MINI_2clk_dbg_v2:
            IsmBits[0] = (
                regs.SetField64(MODS_ISM_MINI_2CLK_DBG_V2_SRC_SEL,    info.oscIdx)    |
                regs.SetField64(MODS_ISM_MINI_2CLK_DBG_V2_OUT_DIV,    info.outDiv)    |
                regs.SetField64(MODS_ISM_MINI_2CLK_DBG_V2_MODE,       info.mode)      |
                regs.SetField64(MODS_ISM_MINI_2CLK_DBG_V2_INIT,       info.init)      |
                regs.SetField64(MODS_ISM_MINI_2CLK_DBG_V2_FINIT,      info.finit)     |
                regs.SetField64(MODS_ISM_MINI_2CLK_DBG_V2_REFCLK_SEL, info.refClkSel) |
                regs.SetField64(MODS_ISM_MINI_2CLK_DBG_V2_IDDQ,       iddq)           |
                // We dont have an interface to set these values so use power on init values.
                regs.SetField64(MODS_ISM_MINI_2CLK_DBG_V2_HOLD_SEL_INIT)              |
                regs.SetField64(MODS_ISM_MINI_2CLK_DBG_V2_HOLD_SEL_CAPTURE_INIT)      |
                regs.SetField64(MODS_ISM_MINI_2CLK_DBG_V2_HOLD_SEL_LAUNCH_INIT)       |
                regs.SetField64(MODS_ISM_MINI_2CLK_DBG_V2_HOLD_SW_DISABLE_MISMATCH1_INIT)
                );
            break;
            
        case LW_ISM_MINI_2clk_dbg_v3:
            IsmBits[0] = (
                regs.SetField64(MODS_ISM_MINI_2CLK_DBG_V2_SRC_SEL,    info.oscIdx)    |
                regs.SetField64(MODS_ISM_MINI_2CLK_DBG_V2_OUT_DIV,    info.outDiv)    |
                regs.SetField64(MODS_ISM_MINI_2CLK_DBG_V2_MODE,       info.mode)      |
                regs.SetField64(MODS_ISM_MINI_2CLK_DBG_V2_INIT,       info.init)      |
                regs.SetField64(MODS_ISM_MINI_2CLK_DBG_V2_FINIT,      info.finit)     |
                regs.SetField64(MODS_ISM_MINI_2CLK_DBG_V2_REFCLK_SEL, info.refClkSel) |
                regs.SetField64(MODS_ISM_MINI_2CLK_DBG_V2_IDDQ,       iddq)           |
                // We dont have an interface to set these values so use power on init values.
                regs.SetField64(MODS_ISM_MINI_2CLK_DBG_V2_HOLD_SEL, 0)                |
                regs.SetField64(MODS_ISM_MINI_2CLK_DBG_V2_HOLD_SEL_CAPTURE, 0)        |
                regs.SetField64(MODS_ISM_MINI_2CLK_DBG_V2_HOLD_SEL_LAUNCH, 0)         |
                regs.SetField64(MODS_ISM_MINI_2CLK_DBG_V2_HOLD_SW_DISABLE_MISMATCH1, 0)
            );
            break;
                        
        case LW_ISM_TSOSC_a       :
            IsmBits[0] = (
              regs.SetField64(MODS_ISM_TSOSC_A_EN,        enable)      |
              regs.SetField64(MODS_ISM_TSOSC_A_IDDQ,      iddq)        |
              regs.SetField64(MODS_ISM_TSOSC_A_COUNT_SEL, info.oscIdx) |
              regs.SetField64(MODS_ISM_TSOSC_A_OUT_DIV,   info.outDiv) |
              regs.SetField64(MODS_ISM_TSOSC_A_ADJ,       info.adj)    |
              regs.SetField64(MODS_ISM_TSOSC_A_LOCAL_DURATION, durClkCycles)
            );
            break;

        case LW_ISM_TSOSC_a2:
            // This version no longer has the COUNT_SEL field
            IsmBits[0] = (
              regs.SetField64(MODS_ISM_TSOSC_A2_EN,        enable)      |
              regs.SetField64(MODS_ISM_TSOSC_A2_IDDQ,      iddq)        |
              regs.SetField64(MODS_ISM_TSOSC_A2_OUT_DIV,   info.outDiv) |
              regs.SetField64(MODS_ISM_TSOSC_A2_ADJ,       info.adj)    |
              regs.SetField64(MODS_ISM_TSOSC_A2_LOCAL_DURATION, durClkCycles)
            );
            break;

        case LW_ISM_TSOSC_a3:
            // Regarding the additional bit JTAG_OVERWRITE for TSOSC_A3, it is used to
            // determine which interface to program the cell, if JTAG_OVERWRITE is 1,
            // config data is from JTAG interface. Otherwise, config data is from PRIV
            // interface. Since MODS always uses JTAG interface, we set it to 1.
            // Also, CLAMP_DEBUG_CLKOUT is a don't care bit.
            //
            // Reference Bug 200230412: Following is the table that ISM team agreed upon:
            //| ------------------------------------------------------------------------|
            //| Bit             PowerUp PowerUp+Enabled PowerUp+Enabled+trigger PowerDn |
            //| ------------------------------------------------------------------------|
            //| _IDDQ           0       0               0                       1       |
            //| _EN             0       1               1                       0       |
            //| _ADJ            0       user value      user value              0       |
            //| _JTAG_OVERWRITE 0       1               1                       0       |
            //| _DIV            0       user value      user value              0       |
            //| _DURATION       0       0               user value              0       |
            //|-------------------------------------------------------------------------|
            if ((state == STATE_1) || (state == STATE_4))
            {
                // State 1: Power up OR
                // State 4: Power down
                IsmBits[0] |= regs.SetField64(MODS_ISM_TSOSC_A3_IDDQ, (state == STATE_4) ? 1 : 0);
            }
            else if ((state == STATE_2) || (state == STATE_3))
            {
                // State 2: Power up + Enabled OR
                // State 3: Power up + Enabled + trigger
                IsmBits[0] = (
                    regs.SetField64(MODS_ISM_TSOSC_A3_EN, 1)                |
                    regs.SetField64(MODS_ISM_TSOSC_A3_IDDQ, 0)              |
                    regs.SetField64(MODS_ISM_TSOSC_A3_OUT_DIV, info.outDiv) |
                    regs.SetField64(MODS_ISM_TSOSC_A3_ADJ, info.adj)        |
                    regs.SetField64(MODS_ISM_TSOSC_A3_LOCAL_DURATION,
                                    (state == STATE_3) ? durClkCycles : 0)  |
                    regs.SetField64(MODS_ISM_TSOSC_A3_JTAG_OVERWRITE, 1)    |
                    regs.SetField64(MODS_ISM_TSOSC_A3_CLAMP_DEBUG_CLKOUT, 1)
                    );
            }
            else
            {
                Printf(Tee::PriError,
                    "%s : Unknown state for LW_ISM_TSOSC_A3!\n",
                    __FUNCTION__);
                return RC::BAD_PARAMETER;
            }
            break;

        case LW_ISM_TSOSC_a4:
            // Regarding the additional bit JTAG_OVERWRITE for TSOSC_A4, it is used to
            // determine which interface to program the cell, if JTAG_OVERWRITE is 1,
            // config data is from JTAG interface. Otherwise, config data is from PRIV
            // interface. Since MODS always uses JTAG interface, we set it to 1.
            // Also, CLAMP_DEBUG_CLKOUT is a don't care bit.
            //
            // Reference Bug 200230412: Following is the table that ISM team agreed upon:
            //| ------------------------------------------------------------------------|
            //| Bit             PowerUp PowerUp+Enabled PowerUp+Enabled+trigger PowerDn |
            //| ------------------------------------------------------------------------|
            //| _IDDQ           0       0               0                       1       |
            //| _EN             0       1               1                       0       |
            //| _ADJ            0       user value      user value              0       |
            //| _JTAG_OVERWRITE 0       1               1                       0       |
            //| _DIV            0       user value      user value              0       |
            //| _DURATION       0       0               user value              0       |
            //|-------------------------------------------------------------------------|
            if ((state == STATE_1) || (state == STATE_4))
            {
                // State 1: Power up OR
                // State 4: Power down
                IsmBits[0] |= regs.SetField64(MODS_ISM_TSOSC_A4_IDDQ, (state == STATE_4) ? 1 : 0);
            }
            else if ((state == STATE_2) || (state == STATE_3))
            {
                // State 2: Power up + Enabled OR
                // State 3: Power up + Enabled + trigger
                IsmBits[0] = (
                    regs.SetField64(MODS_ISM_TSOSC_A4_EN, 1)                |
                    regs.SetField64(MODS_ISM_TSOSC_A4_IDDQ, 0)              |
                    regs.SetField64(MODS_ISM_TSOSC_A4_OUT_DIV, info.outDiv) |
                    regs.SetField64(MODS_ISM_TSOSC_A4_ADJ, info.adj)        |
                    regs.SetField64(MODS_ISM_TSOSC_A4_LOCAL_DURATION,
                                    (state == STATE_3) ? durClkCycles : 0)  |
                    regs.SetField64(MODS_ISM_TSOSC_A4_JTAG_OVERWRITE, 1)
                    );
            }
            else
            {
                Printf(Tee::PriError,
                    "%s : Unknown state for LW_ISM_TSOSC_A4!\n",
                    __FUNCTION__);
                return RC::BAD_PARAMETER;
            }
            break;

        //Note: The bin_aging MACRO is wired different. The _PG_DIS bit is used to power up the
        //      ISM and the _IDDQ is used to enable the ROs. The code below is correct!
        case LW_ISM_ROSC_bin_aging:
            IsmBits[0] = (
              regs.SetField64(MODS_ISM_ROSC_BIN_AGING_SRC_SEL,   info.oscIdx) |
              regs.SetField64(MODS_ISM_ROSC_BIN_AGING_OUT_DIV,   info.outDiv) |
              regs.SetField64(MODS_ISM_ROSC_BIN_AGING_MODE,      info.mode)   |
              regs.SetField64(MODS_ISM_ROSC_BIN_AGING_IDDQ,      !enable)     | // 0=en, 1=dis
              regs.SetField64(MODS_ISM_ROSC_BIN_AGING_LOCAL_DURATION, durClkCycles) |
              regs.SetField64(MODS_ISM_ROSC_BIN_AGING_PG_DIS,    !iddq)       | //1=pwrUp, 0=pwrDn
              regs.SetField64(MODS_ISM_ROSC_BIN_AGING_RO_EN,     info.roEnable) |
              regs.SetField64(MODS_ISM_ROSC_BIN_AGING_COUNT_LOW, 0)
            );
            IsmBits[1] = regs.SetField64(MODS_ISM_ROSC_BIN_AGING_COUNT_HIGH, 0);
            break;

        case LW_ISM_ROSC_bin_aging_v1:
            // Reference Bug 1851969: Following is the table that ISM team agreed upon:
            //|--------------------------------------------------------------------------------------------------------| $
            //| Field       PowerUp +           PowerUp +               PowerDn +               PowerUp +              | $
            //|             Disabled +          Enabled +               Disabled +              Disabled +             | $
            //|             DurClk = 0          DurClk = user value     DurClk = user value +   DurClk = user value +  | $
            //|                                                         KeepActive = false      KeepActive = true      | $
            //|--------------------------------------------------------------------------------------------------------| $
            //| _IDDQ       0                   0                       1                       0                      | $
            //| _PG_DIS     1                   1                       0                       1                      | $
            //| _RO_EN      user value          user value              0                       user value             | $
            //| _DCD_EN     0                   user value              0                       user value             | $
            //| _DURATION   0                   user value              0                       user value             | $
            //|--------------------------------------------------------------------------------------------------------| $
            IsmBits[0] = (
                regs.SetField64(MODS_ISM_ROSC_BIN_AGING_V1_SRC_SEL, info.oscIdx)         |
                regs.SetField64(MODS_ISM_ROSC_BIN_AGING_V1_OUT_DIV, info.outDiv)         |
                regs.SetField64(MODS_ISM_ROSC_BIN_AGING_V1_MODE, info.mode)              |
                // 0=pwrUp, 1=pwrDn
                regs.SetField64(MODS_ISM_ROSC_BIN_AGING_V1_IDDQ, iddq)                   | 
                regs.SetField64(MODS_ISM_ROSC_BIN_AGING_V1_LOCAL_DURATION, durClkCycles) |
                  // 0=pwrUp, 1=pwrDn
                regs.SetField64(MODS_ISM_ROSC_BIN_AGING_V1_PG_DIS, !iddq)                |
                regs.SetField64(MODS_ISM_ROSC_BIN_AGING_V1_RO_EN, (!iddq ? info.roEnable : 0)) |
                regs.SetField64(MODS_ISM_ROSC_BIN_AGING_V1_DCD_EN,
                    ((enable || (flags & KEEP_ACTIVE)) ? info.dcdEn : 0))                |
                regs.SetField64(MODS_ISM_ROSC_BIN_AGING_V1_OUT_SEL, info.outSel)         |
                regs.SetField64(MODS_ISM_ROSC_BIN_AGING_V1_COUNT_LOW, 0)
                );
            IsmBits[1] = (
                regs.SetField64(MODS_ISM_ROSC_BIN_AGING_V1_COUNT_HIGH, 0)                |
                regs.SetField64(MODS_ISM_ROSC_BIN_AGING_V1_DCD_HI_COUNT, 0)              |
                regs.SetField64(MODS_ISM_ROSC_BIN_AGING_V1_READY, 0)
                );
            break;

        case LW_ISM_aging:
            // Reference Bug 200264005: Following is the table that ISM team agreed upon for 
            // LW_ISM_AGING and LW_ISM_AGING_V1
            //|-------------------------------------------------------------------------------------------------| $
            //| Bit             PowerUp         PowerUp +       PowerUp +       PowerDn +       PowerUp +       | $
            //|                                 open loop       closed loop     KeepActive =    KeepActive =    | $
            //|                                 mode            mode            false           true            | $
            //|-------------------------------------------------------------------------------------------------| $
            //| _IDDQ           0               0               0               1               0               | $
            //| _DIV            0               user value      user value      0               user value      | $
            //| _MODE           0               0               1/2/3           0               0               | $
            //| _DURATION       0               0               user value      0               0               | $
            //| _PACT_SEL       0               user value      user value      0               user value      | $
            //| _NACT_SEL       0               user value      user value      0               user value      | $
            //| _SRC_CLK_SEL    0               user value      user value      0               user value      | $
            //| _EN             0               0               1               0               0               | $
            //|-------------------------------------------------------------------------------------------------| $
            // Also, JTAG_OVERRIDE should always be 1 and OUTPUT_CLAMP should always be 0
            switch (state)
            {
                case STATE_1:
                    // All 0s
                    return OK;
                case STATE_2:
                case STATE_3:
                    IsmBits[0] = (
                        regs.SetField64(MODS_ISM_AGING_SRC_SEL, info.oscIdx)          |
                        regs.SetField64(MODS_ISM_AGING_OUT_DIV, info.outDiv)          |
                        regs.SetField64(MODS_ISM_AGING_IDDQ, iddq)                    |
                        regs.SetField64(MODS_ISM_AGING_MODE, info.mode)               |
                        regs.SetField64(MODS_ISM_AGING_LOCAL_DURATION, 
                                        (info.mode ? durClkCycles : 0))               |
                        regs.SetField64(MODS_ISM_AGING_PACT_SEL, info.pactSel)        |
                        regs.SetField64(MODS_ISM_AGING_NACT_SEL, info.nactSel)        |
                        regs.SetField64(MODS_ISM_AGING_SRC_CLK_SEL, info.srcClkSel)   |
                        regs.SetField64(MODS_ISM_AGING_JTAG_OVERRIDE, 1)              |
                        regs.SetField64(MODS_ISM_AGING_EN, ((info.mode > 0) ? 1 : 0)) |
                        regs.SetField64(MODS_ISM_AGING_OUTPUT_CLAMP, 0)               |
                        regs.SetField64(MODS_ISM_AGING_COUNT, 0)                      |
                        regs.SetField64(MODS_ISM_AGING_READY, 0)
                        );
                    break;
                case STATE_4:
                    IsmBits[0] = (
                        regs.SetField64(MODS_ISM_AGING_SRC_SEL, (!iddq ? info.oscIdx : 0))   |
                        regs.SetField64(MODS_ISM_AGING_OUT_DIV, (!iddq ? info.outDiv : 0))   |
                        regs.SetField64(MODS_ISM_AGING_IDDQ, iddq)                           |
                        // Always 0 in aging mode
                        regs.SetField64(MODS_ISM_AGING_MODE, 0)                              | 
                        regs.SetField64(MODS_ISM_AGING_LOCAL_DURATION, 0)                    |
                        regs.SetField64(MODS_ISM_AGING_PACT_SEL, (!iddq ? info.pactSel : 0)) |
                        regs.SetField64(MODS_ISM_AGING_NACT_SEL, (!iddq ? info.nactSel : 0)) |
                        regs.SetField64(MODS_ISM_AGING_SRC_CLK_SEL, (!iddq ? info.srcClkSel : 0)) |
                        regs.SetField64(MODS_ISM_AGING_JTAG_OVERRIDE, 1)                     |
                        regs.SetField64(MODS_ISM_AGING_EN, 0)                                |
                        regs.SetField64(MODS_ISM_AGING_OUTPUT_CLAMP, 0)                      |
                        regs.SetField64(MODS_ISM_AGING_COUNT, 0)                             |
                        regs.SetField64(MODS_ISM_AGING_READY, 0)
                        );
                    break;
                default:
                    Printf(Tee::PriError,
                        "%s : Unknown state for LW_ISM_aging!\n",
                        __FUNCTION__);
                    return RC::BAD_PARAMETER;
            }
            break;

            case LW_ISM_aging_v1:
            // Reference Bug 200264005: Please refer to the same table as LW_ISM_aging
            switch (state)
            {
                case STATE_1:
                    // All 0s
                    return OK;
                case STATE_2:
                case STATE_3:
                    IsmBits[0] = (
                        regs.SetField64(MODS_ISM_AGING_V1_SRC_SEL, info.oscIdx)          |
                        regs.SetField64(MODS_ISM_AGING_V1_OUT_DIV, info.outDiv)          |
                        regs.SetField64(MODS_ISM_AGING_V1_IDDQ, iddq)                    |
                        regs.SetField64(MODS_ISM_AGING_V1_MODE, info.mode)               |
                        regs.SetField64(MODS_ISM_AGING_V1_LOCAL_DURATION, 
                                        (info.mode ? durClkCycles : 0))                  |
                        regs.SetField64(MODS_ISM_AGING_V1_PACT_SEL, info.pactSel)        |
                        regs.SetField64(MODS_ISM_AGING_V1_NACT_SEL, info.nactSel)        |
                        regs.SetField64(MODS_ISM_AGING_V1_SRC_CLK_SEL, info.srcClkSel)   |
                        regs.SetField64(MODS_ISM_AGING_V1_EN, ((info.mode > 0) ? 1 : 0)) |
                        regs.SetField64(MODS_ISM_AGING_V1_COUNT, 0)                      |
                        regs.SetField64(MODS_ISM_AGING_V1_READY, 0)
                        );
                    break;
                case STATE_4:
                    IsmBits[0] = (
                        regs.SetField64(MODS_ISM_AGING_V1_SRC_SEL, (!iddq ? info.oscIdx : 0))   |
                        regs.SetField64(MODS_ISM_AGING_V1_OUT_DIV, (!iddq ? info.outDiv : 0))   |
                        regs.SetField64(MODS_ISM_AGING_V1_IDDQ, iddq)                           |
                        // Always in aging mode
                        regs.SetField64(MODS_ISM_AGING_V1_MODE, 0)                              | 
                        regs.SetField64(MODS_ISM_AGING_V1_LOCAL_DURATION, 0)                    |
                        regs.SetField64(MODS_ISM_AGING_V1_PACT_SEL, (!iddq ? info.pactSel : 0)) |
                        regs.SetField64(MODS_ISM_AGING_V1_NACT_SEL, (!iddq ? info.nactSel : 0)) |
                        regs.SetField64(MODS_ISM_AGING_V1_SRC_CLK_SEL,
                                        (!iddq ? info.srcClkSel : 0))                           |
                        regs.SetField64(MODS_ISM_AGING_V1_EN, 0)                                |
                        regs.SetField64(MODS_ISM_AGING_V1_COUNT, 0)                             |
                        regs.SetField64(MODS_ISM_AGING_V1_READY, 0)
                        );
                    break;
                default:
                    Printf(Tee::PriError,
                        "%s : Unknown state for LW_ISM_aging_v1!\n",
                        __FUNCTION__);
                    return RC::BAD_PARAMETER;
            }
            break;

        case LW_ISM_aging_v2:
            // Reference Bug 200264005: Please refer to the same table as LW_ISM_aging
            // and LW_ISM_AGING_V1
            //|---------------------------------------------------------------------------------|
            //| Bit           PowerUp   PowerUp +    PowerUp +    PowerDn +     PowerUp +       |
            //|                         open loop    closed loop  KeepActive =  KeepActive =    |
            //|                         mode         mode         false         true            |
            //|---------------------------------------------------------------------------------|
            //| _IDDQ         0         0            0            1             0               |
            //| _DIV          0         user value   user value   0             user value      |
            //| _MODE         0         0            1/2/3        0             0               |
            //| _DURATION     0         0            user value   0             0               |
            //| _PACT_SEL     0         user value   user value   0             user value      |
            //| _NACT_SEL     0         user value   user value   0             user value      |
            //| _SRC_CLK_SEL  0         user value   user value   0             user value      |
            //| _EN           0         0            1            0             0               |
            //|---------------------------------------------------------------------------------|
            // Also, JTAG_OVERRIDE should always be 1
            switch (state)
            {
                case STATE_1:
                    // All 0s
                    return OK;
                case STATE_2:
                case STATE_3:
                    IsmBits[0] = (
                        regs.SetField64(MODS_ISM_AGING_V2_SRC_SEL, info.oscIdx)             |
                        regs.SetField64(MODS_ISM_AGING_V2_OUT_DIV, info.outDiv)             |
                        regs.SetField64(MODS_ISM_AGING_V2_IDDQ, iddq)                       |
                        regs.SetField64(MODS_ISM_AGING_V2_MODE, info.mode)                  |
                        regs.SetField64(MODS_ISM_AGING_V2_LOCAL_DURATION,
                                        (info.mode ? durClkCycles : 0))                     |
                        regs.SetField64(MODS_ISM_AGING_V2_PACT_SEL, info.pactSel)           |
                        regs.SetField64(MODS_ISM_AGING_V2_NACT_SEL, info.nactSel)           |
                        regs.SetField64(MODS_ISM_AGING_V2_SRC_CLK_SEL, info.srcClkSel)      |
                        regs.SetField64(MODS_ISM_AGING_V2_EN, ((info.mode > 0) ? 1 : 0))    |
                        regs.SetField64(MODS_ISM_AGING_V2_JTAG_OVERRIDE, 1)                 |
                        regs.SetField64(MODS_ISM_AGING_V2_COUNT, 0)                         |
                        regs.SetField64(MODS_ISM_AGING_V2_READY, 0)
                        );
                    break;
                case STATE_4:
                    IsmBits[0] = (
                        regs.SetField64(MODS_ISM_AGING_V2_SRC_SEL, (!iddq ? info.oscIdx : 0))   |
                        regs.SetField64(MODS_ISM_AGING_V2_OUT_DIV, (!iddq ? info.outDiv : 0))   |
                        regs.SetField64(MODS_ISM_AGING_V2_IDDQ, iddq)                           |
                        regs.SetField64(MODS_ISM_AGING_V2_MODE, 0)     /*Always in aging mode*/ | 
                        regs.SetField64(MODS_ISM_AGING_V2_LOCAL_DURATION, 0)                    |
                        regs.SetField64(MODS_ISM_AGING_V2_PACT_SEL, (!iddq ? info.pactSel : 0)) |
                        regs.SetField64(MODS_ISM_AGING_V2_NACT_SEL, (!iddq ? info.nactSel : 0)) |
                        regs.SetField64(MODS_ISM_AGING_V2_SRC_CLK_SEL,
                                        (!iddq ? info.srcClkSel : 0))                           |
                        regs.SetField64(MODS_ISM_AGING_V2_JTAG_OVERRIDE, 1)                     |
                        regs.SetField64(MODS_ISM_AGING_V2_EN, 0)                                |
                        regs.SetField64(MODS_ISM_AGING_V2_COUNT, 0)                             |
                        regs.SetField64(MODS_ISM_AGING_V2_READY, 0)
                        );
                    break;
                default:
                    Printf(Tee::PriError,
                           "%s : Unknown state for LW_ISM_aging_v2!\n", __FUNCTION__);
                    return RC::BAD_PARAMETER;
            }
            break;
            
        case LW_ISM_aging_v3: // Modify this for versions 3 & 4
        case LW_ISM_aging_v4: // Version 4 adds JTAG_ENABLE starting at bit 35 and shifts the rest
                              // of the fields down 1 bit.
            // Reference Bug 200264005: Please refer to the same table as LW_ISM_aging
            // and LW_ISM_AGING_V1
            //|----------------------------------------------------------------------------------|
            //| Bit            PowerUp    PowerUp +    PowerUp +     PowerDn +     PowerUp +     |
            //|                           open loop    closed loop   KeepActive =  KeepActive =  |
            //|                           mode         mode          false         true          |
            //|----------------------------------------------------------------------------------|
            //| _IDDQ          0          0            0             1             0             |
            //| _DIV           0          user value   user value    0             user value    |
            //| _MODE          0          0            1/2/3         0             0             |
            //| _DURATION      0          0            user value    0             0             |
            //| _PACT_SEL      0          user value   user value    0             user value    |
            //| _NACT_SEL      0          user value   user value    0             user value    |
            //| _SRC_CLK_SEL   0          user value   user value    0             user value    |
            //| _EN            0          0            1             0             0             |
            //| _JTAG_OVERRIDE 1          1            1             1             1             |
            //|----------------------------------------------------------------------------------|
            switch (state)
            {
                case STATE_1:
                    // All 0s
                    return OK;
                case STATE_2:
                case STATE_3:
                    IsmBits[0] = (
                        regs.SetField64(MODS_ISM_AGING_V3_SRC_SEL, info.oscIdx)     |
                        regs.SetField64(MODS_ISM_AGING_V3_OUT_DIV, info.outDiv)     |
                        regs.SetField64(MODS_ISM_AGING_V3_IDDQ, iddq)               |
                        regs.SetField64(MODS_ISM_AGING_V3_MODE, info.mode)          |
                        regs.SetField64(MODS_ISM_AGING_V3_LOCAL_DURATION,
                                        (info.mode ? durClkCycles : 0))             |
                        regs.SetField64(MODS_ISM_AGING_V3_PACT_SEL, info.pactSel)   |
                        regs.SetField64(MODS_ISM_AGING_V3_NACT_SEL, info.nactSel)   |
                        regs.SetField64(MODS_ISM_AGING_V3_SRC_CLK_SEL, info.srcClkSel));
                     if (ismType == LW_ISM_aging_v3)
                     {
                         IsmBits[0] |= regs.SetField64(MODS_ISM_AGING_V3_EN,
                                                       ((info.mode > 0) ? 1 : 0))           |
                                       regs.SetField64(MODS_ISM_AGING_V3_COUNT, 0)          |
                                       regs.SetField64(MODS_ISM_AGING_V3_DCD_COUNT_LOW, 0);
                         IsmBits[1] =  regs.SetField64(MODS_ISM_AGING_V3_DCD_COUNT_HIGH, 0) |
                                       regs.SetField64(MODS_ISM_AGING_V3_READY, 0);
                     }
                     else
                     {
                         IsmBits[0] |= regs.SetField64(MODS_ISM_AGING_V4_JTAG_OVERRIDE, 1)  |
                                       regs.SetField64(MODS_ISM_AGING_V4_EN,
                                                       ((info.mode > 0) ? 1 : 0))           |
                                       regs.SetField64(MODS_ISM_AGING_V4_COUNT, 0)          |
                                       regs.SetField64(MODS_ISM_AGING_V4_DCD_COUNT_LOW, 0);
                         IsmBits[1] =  regs.SetField64(MODS_ISM_AGING_V4_DCD_COUNT_HIGH, 0) |
                                       regs.SetField64(MODS_ISM_AGING_V4_READY, 0);
                     }
                    break;
                case STATE_4:
                    IsmBits[0] = (
                        regs.SetField64(MODS_ISM_AGING_V3_SRC_SEL, (!iddq ? info.oscIdx : 0)) |
                        regs.SetField64(MODS_ISM_AGING_V3_OUT_DIV, (!iddq ? info.outDiv : 0)) |
                        regs.SetField64(MODS_ISM_AGING_V3_IDDQ, iddq) |
                        regs.SetField64(MODS_ISM_AGING_V3_MODE, 0) | // Always in aging mode
                        regs.SetField64(MODS_ISM_AGING_V3_LOCAL_DURATION, 0) |
                        regs.SetField64(MODS_ISM_AGING_V3_PACT_SEL, (!iddq ? info.pactSel : 0)) |
                        regs.SetField64(MODS_ISM_AGING_V3_NACT_SEL, (!iddq ? info.nactSel : 0)) |
                        regs.SetField64(MODS_ISM_AGING_V3_SRC_CLK_SEL, !iddq ? info.srcClkSel : 0)
                        );

                    if (ismType == LW_ISM_aging_v3)
                    {
                        IsmBits[0] |= regs.SetField64(MODS_ISM_AGING_V3_EN, 0)              |
                                      regs.SetField64(MODS_ISM_AGING_V3_COUNT, 0)           |
                                      regs.SetField64(MODS_ISM_AGING_V3_DCD_COUNT_LOW, 0);
                        IsmBits[1] =  regs.SetField64(MODS_ISM_AGING_V3_DCD_COUNT_HIGH, 0)  |
                                      regs.SetField64(MODS_ISM_AGING_V3_READY, 0);
                    }
                    else
                    {
                        IsmBits[0] |= regs.SetField64(MODS_ISM_AGING_V4_JTAG_OVERRIDE, 1)   |
                                      regs.SetField64(MODS_ISM_AGING_V4_EN, 0)              |
                                      regs.SetField64(MODS_ISM_AGING_V4_COUNT, 0)           |
                                      regs.SetField64(MODS_ISM_AGING_V4_DCD_COUNT_LOW, 0);
                        IsmBits[1] =  regs.SetField64(MODS_ISM_AGING_V4_DCD_COUNT_HIGH, 0)  |
                                      regs.SetField64(MODS_ISM_AGING_V4_READY, 0);
                    }
                    break;
                default:
                    Printf(Tee::PriError,
                        "%s : Unknown state for LW_ISM_aging_v2!\n",
                        __FUNCTION__);
                    return RC::BAD_PARAMETER;
            }
            break;

        case LW_ISM_NMEAS_v2:
        case LW_ISM_NMEAS_v3:
        case LW_ISM_NMEAS_v4:
            // special case noise measurement circuit. Just leave the bits alone for this API.
            // you should really be calling NoiseMeas* APIs to configure those macros.
            break;;

        case LW_ISM_NMEAS_lite:
            // This is needed when NMEAS_lite macro is used in MINI mode
            IsmBits[0] = (
                regs.SetField64(MODS_ISM_NMEAS_LITE_V1_NM_TRIM, 0)        | // n/a in MINI mode
                regs.SetField64(MODS_ISM_NMEAS_LITE_V1_NM_TAP, 0)         | // n/a in MINI mode
                regs.SetField64(MODS_ISM_NMEAS_LITE_V1_IDDQ, !enable)     | // 0=en, 1=dis
                regs.SetField64(MODS_ISM_NMEAS_LITE_V1_MODE, info.mode)   |
                regs.SetField64(MODS_ISM_NMEAS_LITE_V1_SEL, info.oscIdx)  |
                regs.SetField64(MODS_ISM_NMEAS_LITE_V1_DIV, !info.outDiv) |
                regs.SetField64(MODS_ISM_NMEAS_LITE_V1_DIV_CLK_IN, 0)     | // n/a in MINI mode
                regs.SetField64(MODS_ISM_NMEAS_LITE_V1_FIXED_DLY_SEL, 0)  | // n/a in MINI mode
                regs.SetField64(MODS_ISM_NMEAS_LITE_V1_COUNT, 0)
                );
            break;

        case LW_ISM_NMEAS_lite_v2:
            IsmBits.resize(CallwlateVectorSize(ismType, 64), 0);
            // This is needed when NMEAS_lite_v2 macro is used in MINI mode
            IsmBits[0] = (
                regs.SetField64(MODS_ISM_NMEAS_LITE_V2_NM_TRIM, 0)          | // n/a in MINI mode
                regs.SetField64(MODS_ISM_NMEAS_LITE_V2_NM_TAP, 0)           |  // n/a in MINI mode
                regs.SetField64(MODS_ISM_NMEAS_LITE_V2_IDDQ, !enable)       | // 0=en, 1=dis
                regs.SetField64(MODS_ISM_NMEAS_LITE_V2_MODE, info.mode)     |
                regs.SetField64(MODS_ISM_NMEAS_LITE_V2_SEL, info.oscIdx)    |
                regs.SetField64(MODS_ISM_NMEAS_LITE_V2_DIV, !info.outDiv)   |
                regs.SetField64(MODS_ISM_NMEAS_LITE_V2_DIV_CLK_IN, 0)       | // n/a in MINI mode
                regs.SetField64(MODS_ISM_NMEAS_LITE_V2_FIXED_DLY_SEL, 0)    | // n/a in MINI mode
                regs.SetField64(MODS_ISM_NMEAS_LITE_V2_REFCLK_SEL, 0)       | // n/a in MINI mode
                regs.SetField64(MODS_ISM_NMEAS_LITE_V2_COUNT, 0)
                );
            break;
        case LW_ISM_NMEAS_lite_v3:
        case LW_ISM_NMEAS_lite_v4:
        case LW_ISM_NMEAS_lite_v5:
        case LW_ISM_NMEAS_lite_v6:
        case LW_ISM_NMEAS_lite_v7:
        case LW_ISM_NMEAS_lite_v8:
        case LW_ISM_NMEAS_lite_v9:
            // There has been no request to run these MACROs in MINI mode. To setup this macro you 
            // should be using ConfigureNoiseBits().
            break;
        case LW_ISM_lwrrent_sink:
            // you should be using ConfigureISinkBits()
            MASSERT(!"Wrong API used to configure ISink bits");
            return RC::SOFTWARE_ERROR;
        default:
            MASSERT(!"Unknown ISM type!");
            return RC::SOFTWARE_ERROR;
    }
    return OK;
}

//------------------------------------------------------------------------------------------------
// Configure all the ISM controllers on the specified chain.
// - The master ISM controller is the ONLY ISM on the chain, and is always at pChain->ISM[0].
// - There may be multiple sub-controllers on a chain. If so then we program all of them. 
// - pChain is a valid pointer to the ISM chain with the controllers.
RC GpuIsm::ConfigureISMCtrl
(
    const IsmChain *pChain          // The ISM chain with the Ctrl ISM.
    , vector<UINT32>  &jtagArray     // vector to store the bits.
    , bool            bEnable        // enable/disable the controller.
    , UINT32          durClkCycles   // how long you want the experiment to run
    , UINT32          delayClkCycles // how long to delay after the trigger before counting
    , UINT32          trigger        // type of trigger to use.
    , INT32           loopMode       // if true allows continuous looping of the experiments
    , INT32           haltMode       // if true allows halting of a continuous experiments
    , INT32           ctrlSrcSel     // 1 = jtag 0 = priv access
)
{
    if (durClkCycles == 0)
        return RC::BAD_PARAMETER;

    RegHal &regs = GetGpuSub()->Regs();

    if (GetGpuSub()->HasBug(853035))
        durClkCycles--;

    jtagArray.clear();
    jtagArray.resize((pChain->Hdr.Size+31)/32, 0);
    UINT32 bits = 0;
    UINT32 idx = 0;
    while ((bits < pChain->Hdr.Size) && (idx < pChain->ISMs.size()))
    {
        vector<UINT64> ismBits(CallwlateVectorSize(pChain->ISMs[idx].Type, 64), 0);
        // we don't want to simply increment bits by size just incase there gaps in the chain.
        bits = pChain->ISMs[idx].Offset + GetISMSize(pChain->ISMs[idx].Type);
        switch (pChain->ISMs[idx].Type)
        { 
            default:
                Printf(Tee::PriError, "Unknown ISM control type:%d\n", pChain->ISMs[idx].Type);
                return RC::SOFTWARE_ERROR;

            case LW_ISM_ctrl:
                //bits 63:0
                ismBits[0] = (regs.SetField64(MODS_ISM_CTRL_DURATION, durClkCycles) |
                              regs.SetField64(MODS_ISM_CTRL_DELAY, delayClkCycles));
                //bits 128:64
                if (bEnable)
                {
                    ismBits[1] |= regs.SetField64(MODS_ISM_CTRL_ENABLE, 1) |
                                  regs.SetField64(MODS_ISM_CTRL_TRIGGER_SRC, trigger);
                }
                else
                {
                    ismBits[1] |= regs.SetField64(MODS_ISM_CTRL_ENABLE, 0) |
                                  regs.SetField64(MODS_ISM_CTRL_TRIGGER_SRC_OFF);
                }
                break;
    
            case LW_ISM_ctrl_v2:
                // The main difference between version 1 and version 2 are:
                // The done bit has been moved
                // The enable bit has been moved
                // There is a new halt_mode bit
                // There is a new loop_mode bit
                //bits 63:0
                ismBits[0] = (regs.SetField64(MODS_ISM_CTRL2_DURATION, durClkCycles) |
                              regs.SetField64(MODS_ISM_CTRL2_DELAY, delayClkCycles));
    
                //bits 128:64
                ismBits[1] = (regs.SetField64(MODS_ISM_CTRL2_HALT_MODE, haltMode) |
                              regs.SetField64(MODS_ISM_CTRL2_LOOP_MODE, loopMode));
    
                if (bEnable)
                {
                    ismBits[1] |= regs.SetField64(MODS_ISM_CTRL2_ENABLE, 1) |
                                  regs.SetField64(MODS_ISM_CTRL2_TRIGGER_SRC, trigger);
                }
                else
                {
                    ismBits[1] |= regs.SetField64(MODS_ISM_CTRL2_ENABLE, 0) |
                                  regs.SetField64(MODS_ISM_CTRL2_TRIGGER_SRC_OFF);
                }
                break;
    
            case LW_ISM_ctrl_v3:
                // The main difference between version 2 and version 3 are:
                // The done bit has been moved
                // The enable bit has been moved
                // There is a new src_sel bit (which always needs to be set to 1).
                //bits 63:0
                ismBits[0] = regs.SetField64(MODS_ISM_CTRL3_DURATION, durClkCycles) |
                             regs.SetField64(MODS_ISM_CTRL3_DELAY, delayClkCycles);
                //bits 128:64
                ismBits[1] |= regs.SetField64(MODS_ISM_CTRL3_CTRL_SRC_SEL, ctrlSrcSel) |
                              regs.SetField64(MODS_ISM_CTRL3_LOOP_MODE, loopMode)  |
                              regs.SetField64(MODS_ISM_CTRL3_HALT_MODE, haltMode);
                if (bEnable)
                {
                    ismBits[1] |= regs.SetField64(MODS_ISM_CTRL3_ENABLE, 1) |
                                  regs.SetField64(MODS_ISM_CTRL3_TRIGGER_SRC, trigger);
                }
                else
                {
                    ismBits[1] |= regs.SetField64(MODS_ISM_CTRL3_ENABLE, 0) |
                                  regs.SetField64(MODS_ISM_CTRL3_TRIGGER_SRC_OFF);
                }
                break;
    
            case LW_ISM_ctrl_v4:
                // The main difference between version 3 and version 4 are:
                // There is a new CLAMP_OUT bit, which should always be 0
                // The done bit has been moved
                // The enable bit has been moved
                //bits 63:0
                ismBits[0] = (regs.SetField64(MODS_ISM_CTRL4_DURATION, durClkCycles) |
                              regs.SetField64(MODS_ISM_CTRL4_DELAY, delayClkCycles));
    
                //bits 128:64
                ismBits[1] = (regs.SetField64(MODS_ISM_CTRL4_CTRL_SRC_SEL, ctrlSrcSel) |
                              regs.SetField64(MODS_ISM_CTRL4_CTRL_CLAMP_OUT, 0) |
                              regs.SetField64(MODS_ISM_CTRL4_HALT_MODE, haltMode) |
                              regs.SetField64(MODS_ISM_CTRL4_LOOP_MODE, loopMode));
    
                if (bEnable)
                {
                    ismBits[1] |= regs.SetField64(MODS_ISM_CTRL4_ENABLE, 1) |
                                  regs.SetField64(MODS_ISM_CTRL4_TRIGGER_SRC, trigger);
                }
                else
                {
                    ismBits[1] |= regs.SetField64(MODS_ISM_CTRL4_ENABLE, 0) |
                                  regs.SetField64(MODS_ISM_CTRL4_TRIGGER_SRC_OFF);
                }
                break;
        }
        // Add the bits to the chain.
        Utility::CopyBits(ismBits,      //src vector
                          jtagArray,    //dst vector
                          0,            //srcBitOffset
                          pChain->ISMs[idx].Offset, //dstBitOffset
                          GetISMSize(pChain->ISMs[idx].Type)); // numBits

        idx++; // next ISM
    }
    return RC::OK;
}

//------------------------------------------------------------------------------
RC GpuIsm::DumpISMBits(UINT32 ismType, vector<UINT64>& ismBitsArray)
{
    // For all of these types the ISMs fit within the 1st 64bits.
    RegHal &regs = GetGpuSub()->Regs();
    switch (ismType)
    {
        // same bit definition as 1clk except can set SRC_SEL = 7
        case LW_ISM_MINI_1clk_no_ext_ro:
        case LW_ISM_MINI_1clk     :
            Printf(Tee::PriNormal, "{ mini_1clk:%d.%d.%d.%d.%d.%d.%d }",
                (int)regs.GetField64(ismBitsArray[0], MODS_ISM_MINI_1CLK_COUNT),
                (int)regs.GetField64(ismBitsArray[0], MODS_ISM_MINI_1CLK_FINIT),
                (int)regs.GetField64(ismBitsArray[0], MODS_ISM_MINI_1CLK_INIT),
                (int)regs.GetField64(ismBitsArray[0], MODS_ISM_MINI_1CLK_MODE),
                (int)regs.GetField64(ismBitsArray[0], MODS_ISM_MINI_1CLK_IDDQ),
                (int)regs.GetField64(ismBitsArray[0], MODS_ISM_MINI_1CLK_OUT_DIV),
                (int)regs.GetField64(ismBitsArray[0], MODS_ISM_MINI_1CLK_SRC_SEL));
            break;

        //same as 1clk_dbg except cant set SRC_SEL = 7
        case LW_ISM_MINI_1clk_no_ext_ro_dbg:
        case LW_ISM_MINI_1clk_dbg :
            Printf(Tee::PriNormal, "{ mini_1clk_dbg:%d.%d.%d.%d.%d.%d.%d.%d }",
                (int)regs.GetField64(ismBitsArray[0], MODS_ISM_MINI_1CLK_DBG_COUNT),
                (int)regs.GetField64(ismBitsArray[0], MODS_ISM_MINI_1CLK_DBG_CLMP_DEBUG_CLKOUT),
                (int)regs.GetField64(ismBitsArray[0], MODS_ISM_MINI_1CLK_DBG_FINIT),
                (int)regs.GetField64(ismBitsArray[0], MODS_ISM_MINI_1CLK_DBG_INIT),
                (int)regs.GetField64(ismBitsArray[0], MODS_ISM_MINI_1CLK_DBG_MODE),
                (int)regs.GetField64(ismBitsArray[0], MODS_ISM_MINI_1CLK_DBG_IDDQ),
                (int)regs.GetField64(ismBitsArray[0], MODS_ISM_MINI_1CLK_DBG_OUT_DIV),
                (int)regs.GetField64(ismBitsArray[0], MODS_ISM_MINI_1CLK_DBG_SRC_SEL));
            break;

        case LW_ISM_MINI_2clk_dbg :
            Printf(Tee::PriNormal, "{ mini_2clk_dbg:%d.%d.%d.%d.%d.%d.%d.%d.%d }",
                (int)regs.GetField64(ismBitsArray[0], MODS_ISM_MINI_2CLK_DBG_COUNT),
                (int)regs.GetField64(ismBitsArray[0], MODS_ISM_MINI_2CLK_DBG_CLMP_DEBUG_CLKOUT),
                (int)regs.GetField64(ismBitsArray[0], MODS_ISM_MINI_2CLK_DBG_REFCLK_SEL),
                (int)regs.GetField64(ismBitsArray[0], MODS_ISM_MINI_2CLK_DBG_FINIT),
                (int)regs.GetField64(ismBitsArray[0], MODS_ISM_MINI_2CLK_DBG_INIT),
                (int)regs.GetField64(ismBitsArray[0], MODS_ISM_MINI_2CLK_DBG_MODE),
                (int)regs.GetField64(ismBitsArray[0], MODS_ISM_MINI_2CLK_DBG_IDDQ),
                (int)regs.GetField64(ismBitsArray[0], MODS_ISM_MINI_2CLK_DBG_OUT_DIV),
                (int)regs.GetField64(ismBitsArray[0], MODS_ISM_MINI_2CLK_DBG_SRC_SEL));
            break;

        // same bit definition as 2clk except can set SRC_SEL = 7
        case LW_ISM_MINI_2clk_no_ext_ro :
        case LW_ISM_MINI_2clk     :
            Printf(Tee::PriNormal, "{ mini_2clk:%d.%d.%d.%d.%d.%d.%d.%d }",
                (int)regs.GetField64(ismBitsArray[0], MODS_ISM_MINI_2CLK_COUNT),
                (int)regs.GetField64(ismBitsArray[0], MODS_ISM_MINI_2CLK_REFCLK_SEL),
                (int)regs.GetField64(ismBitsArray[0], MODS_ISM_MINI_2CLK_FINIT),
                (int)regs.GetField64(ismBitsArray[0], MODS_ISM_MINI_2CLK_INIT),
                (int)regs.GetField64(ismBitsArray[0], MODS_ISM_MINI_2CLK_MODE),
                (int)regs.GetField64(ismBitsArray[0], MODS_ISM_MINI_2CLK_IDDQ),
                (int)regs.GetField64(ismBitsArray[0], MODS_ISM_MINI_2CLK_OUT_DIV),
                (int)regs.GetField64(ismBitsArray[0], MODS_ISM_MINI_2CLK_SRC_SEL));
            break;

        // These ISMs have their own control/enable logic
        case LW_ISM_ROSC_comp     :
            Printf(Tee::PriNormal, "{ rosc_comp:%d.%d.%d.%d.%d.%d }",
              (int)regs.GetField64(ismBitsArray[0], MODS_ISM_ROSC_COMP_COUNT),
              (int)regs.GetField64(ismBitsArray[0], MODS_ISM_ROSC_COMP_LOCAL_DURATION),
              (int)regs.GetField64(ismBitsArray[0], MODS_ISM_ROSC_COMP_MODE),
              (int)regs.GetField64(ismBitsArray[0], MODS_ISM_ROSC_COMP_IDDQ),
              (int)regs.GetField64(ismBitsArray[0], MODS_ISM_ROSC_COMP_OUT_DIV),
              (int)regs.GetField64(ismBitsArray[0], MODS_ISM_ROSC_COMP_SRC_SEL));
            break;

        case LW_ISM_ROSC_comp_v1  :
            Printf(Tee::PriNormal, "{ rosc_comp:%d.%d.%d.%d.%d.%d.%d.%d }",
                (int)regs.GetField64(ismBitsArray[0], MODS_ISM_ROSC_COMP_V1_SRC_SEL),
                (int)regs.GetField64(ismBitsArray[0], MODS_ISM_ROSC_COMP_V1_OUT_DIV),
                (int)regs.GetField64(ismBitsArray[0], MODS_ISM_ROSC_COMP_V1_IDDQ),
                (int)regs.GetField64(ismBitsArray[0], MODS_ISM_ROSC_COMP_V1_EN),
                (int)regs.GetField64(ismBitsArray[0], MODS_ISM_ROSC_COMP_V1_LOCAL_DURATION),
                (int)regs.GetField64(ismBitsArray[0], MODS_ISM_ROSC_COMP_V1_JTAG_OVERRIDE),
                (int)regs.GetField64(ismBitsArray[0], MODS_ISM_ROSC_COMP_V1_COUNT),
                (int)regs.GetField64(ismBitsArray[0], MODS_ISM_ROSC_COMP_V1_READY));
            break;

        case LW_ISM_ROSC_comp_v2  :
            Printf(Tee::PriNormal, "{ rosc_comp:%d.%d.%d.%d.%d.%d.%d.%d }",
                (int)regs.GetField64(ismBitsArray[0], MODS_ISM_ROSC_COMP_V2_SRC_SEL),
                (int)regs.GetField64(ismBitsArray[0], MODS_ISM_ROSC_COMP_V2_OUT_DIV),
                (int)regs.GetField64(ismBitsArray[0], MODS_ISM_ROSC_COMP_V2_IDDQ),
                (int)regs.GetField64(ismBitsArray[0], MODS_ISM_ROSC_COMP_V2_EN),
                (int)regs.GetField64(ismBitsArray[0], MODS_ISM_ROSC_COMP_V2_LOCAL_DURATION),
                (int)regs.GetField64(ismBitsArray[0], MODS_ISM_ROSC_COMP_V2_JTAG_OVERRIDE),
                (int)regs.GetField64(ismBitsArray[0], MODS_ISM_ROSC_COMP_V2_COUNT),
                (int)regs.GetField64(ismBitsArray[0], MODS_ISM_ROSC_COMP_V2_READY));
            break;

        case LW_ISM_ROSC_bin: // different implementations for GF117 & GK10x
            Printf(Tee::PriNormal, "{ rosc_bin:%d.%d.%d.%d.%d.%d }",
              (int)regs.GetField64(ismBitsArray[0], MODS_ISM_ROSC_BIN_COUNT),
              (int)regs.GetField64(ismBitsArray[0], MODS_ISM_ROSC_BIN_LOCAL_DURATION),
              (int)regs.GetField64(ismBitsArray[0], MODS_ISM_ROSC_BIN_MODE),
              (int)regs.GetField64(ismBitsArray[0], MODS_ISM_ROSC_BIN_IDDQ),
              (int)regs.GetField64(ismBitsArray[0], MODS_ISM_ROSC_BIN_OUT_DIV),
              (int)regs.GetField64(ismBitsArray[0], MODS_ISM_ROSC_BIN_SRC_SEL));
            break;

        case LW_ISM_ROSC_bin_metal:
            Printf(Tee::PriNormal, "{ rosc_bin_metal:%d.%d.%d.%d.%d.%d }",
              (int)regs.GetField64(ismBitsArray[0], MODS_ISM_ROSC_BIN_METAL_COUNT),
              (int)regs.GetField64(ismBitsArray[0], MODS_ISM_ROSC_BIN_METAL_LOCAL_DURATION),
              (int)regs.GetField64(ismBitsArray[0], MODS_ISM_ROSC_BIN_METAL_MODE),
              (int)regs.GetField64(ismBitsArray[0], MODS_ISM_ROSC_BIN_METAL_IDDQ),
              (int)regs.GetField64(ismBitsArray[0], MODS_ISM_ROSC_BIN_METAL_OUT_DIV),
              (int)regs.GetField64(ismBitsArray[0], MODS_ISM_ROSC_BIN_METAL_SRC_SEL));
            break;

        case LW_ISM_VNSENSE_ddly  :
            Printf(Tee::PriNormal, "{ vsense:%d.%d.%d.%d.%d.%d.%d.%d.%d }",
              (int)regs.GetField64(ismBitsArray[0], MODS_ISM_VNSENSE_DDLY_COUNT),
              (int)regs.GetField64(ismBitsArray[0], MODS_ISM_VNSENSE_DDLY_OUT_DIV),
              (int)regs.GetField64(ismBitsArray[0], MODS_ISM_VNSENSE_DDLY_LOCAL_DURATION),
              (int)regs.GetField64(ismBitsArray[0], MODS_ISM_VNSENSE_DDLY_FINIT),
              (int)regs.GetField64(ismBitsArray[0], MODS_ISM_VNSENSE_DDLY_INIT),
              (int)regs.GetField64(ismBitsArray[0], MODS_ISM_VNSENSE_DDLY_ADJ),
              (int)regs.GetField64(ismBitsArray[0], MODS_ISM_VNSENSE_DDLY_MODE),
              (int)regs.GetField64(ismBitsArray[0], MODS_ISM_VNSENSE_DDLY_IDDQ),
              (int)regs.GetField64(ismBitsArray[0], MODS_ISM_VNSENSE_DDLY_EN));
            break;

        case LW_ISM_TSOSC_a       :
            Printf(Tee::PriNormal, "{ tsosc_a:%d.%d.%d.%d.%d.%d.%d.%d }",
              (int)regs.GetField64(ismBitsArray[0], MODS_ISM_TSOSC_A_COUNT),
              (int)regs.GetField64(ismBitsArray[0], MODS_ISM_TSOSC_A_OUT_DIV),
              (int)regs.GetField64(ismBitsArray[0], MODS_ISM_TSOSC_A_COUNT_SEL),
              (int)regs.GetField64(ismBitsArray[0], MODS_ISM_TSOSC_A_LOCAL_DURATION),
              (int)regs.GetField64(ismBitsArray[0], MODS_ISM_TSOSC_A_CLMP_DEBUG_CLKOUT),
              (int)regs.GetField64(ismBitsArray[0], MODS_ISM_TSOSC_A_ADJ),
              (int)regs.GetField64(ismBitsArray[0], MODS_ISM_TSOSC_A_IDDQ),
              (int)regs.GetField64(ismBitsArray[0], MODS_ISM_TSOSC_A_EN));
            break;

        case LW_ISM_aging_v3: // change this for versions 3 & 4
            Printf(Tee::PriNormal,
                "{ ism_aging_v3:%d.%d.%d.%d.%d.%d.%d.%d.%d.%d.%d.%d }",
                (int)regs.GetField64(ismBitsArray[0], MODS_ISM_AGING_V3_SRC_SEL),
                (int)regs.GetField64(ismBitsArray[0], MODS_ISM_AGING_V3_OUT_DIV),
                (int)regs.GetField64(ismBitsArray[0], MODS_ISM_AGING_V3_IDDQ),
                (int)regs.GetField64(ismBitsArray[0], MODS_ISM_AGING_V3_MODE),
                (int)regs.GetField64(ismBitsArray[0], MODS_ISM_AGING_V3_LOCAL_DURATION),
                (int)regs.GetField64(ismBitsArray[0], MODS_ISM_AGING_V3_PACT_SEL),
                (int)regs.GetField64(ismBitsArray[0], MODS_ISM_AGING_V3_NACT_SEL),
                (int)regs.GetField64(ismBitsArray[0], MODS_ISM_AGING_V3_SRC_CLK_SEL),
                (int)regs.GetField64(ismBitsArray[0], MODS_ISM_AGING_V3_EN),
                (int)regs.GetField64(ismBitsArray[0], MODS_ISM_AGING_V3_COUNT),
                (int)regs.GetField64(ismBitsArray[1], MODS_ISM_AGING_V3_DCD_COUNT_HIGH) << 
                     regs.LookupMaskSize(MODS_ISM_AGING_V3_DCD_COUNT_LOW) |
                (int)regs.GetField64(ismBitsArray[0], MODS_ISM_AGING_V3_DCD_COUNT_LOW),
                (int)regs.GetField64(ismBitsArray[1], MODS_ISM_AGING_V3_READY));
            break;

        case LW_ISM_aging_v4:
            Printf(Tee::PriNormal,
                "{ ism_aging_v4:%d.%d.%d.%d.%d.%d.%d.%d.%d.%d.%d.%d.%d }",
                (int)regs.GetField64(ismBitsArray[0], MODS_ISM_AGING_V4_SRC_SEL),
                (int)regs.GetField64(ismBitsArray[0], MODS_ISM_AGING_V4_OUT_DIV),
                (int)regs.GetField64(ismBitsArray[0], MODS_ISM_AGING_V4_IDDQ),
                (int)regs.GetField64(ismBitsArray[0], MODS_ISM_AGING_V4_MODE),
                (int)regs.GetField64(ismBitsArray[0], MODS_ISM_AGING_V4_LOCAL_DURATION),
                (int)regs.GetField64(ismBitsArray[0], MODS_ISM_AGING_V4_PACT_SEL),
                (int)regs.GetField64(ismBitsArray[0], MODS_ISM_AGING_V4_NACT_SEL),
                (int)regs.GetField64(ismBitsArray[0], MODS_ISM_AGING_V4_SRC_CLK_SEL),
                (int)regs.GetField64(ismBitsArray[0], MODS_ISM_AGING_V4_JTAG_OVERRIDE),
                (int)regs.GetField64(ismBitsArray[0], MODS_ISM_AGING_V4_EN),
                (int)regs.GetField64(ismBitsArray[0], MODS_ISM_AGING_V4_COUNT),
                (int)regs.GetField64(ismBitsArray[1], MODS_ISM_AGING_V4_DCD_COUNT_HIGH) <<
                     regs.LookupMaskSize(MODS_ISM_AGING_V4_DCD_COUNT_LOW) |
                (int)regs.GetField64(ismBitsArray[0], MODS_ISM_AGING_V4_DCD_COUNT_LOW),
                (int)regs.GetField64(ismBitsArray[1], MODS_ISM_AGING_V4_READY));
            break;

        case LW_ISM_NMEAS_lite_v2:
            // This is needed when NMEAS_lite_v2 macro is used in MINI mode
            Printf(Tee::PriNormal,
                "{ nmeas_lite_v2:%d.%d.%d.%d.%d.%d.%d.%d.%d.%d }",
                (int)regs.GetField64(ismBitsArray[0], MODS_ISM_NMEAS_LITE_V2_NM_TRIM),
                (int)regs.GetField64(ismBitsArray[0], MODS_ISM_NMEAS_LITE_V2_NM_TAP),
                (int)regs.GetField64(ismBitsArray[0], MODS_ISM_NMEAS_LITE_V2_IDDQ),
                (int)regs.GetField64(ismBitsArray[0], MODS_ISM_NMEAS_LITE_V2_MODE),
                (int)regs.GetField64(ismBitsArray[0], MODS_ISM_NMEAS_LITE_V2_SEL),
                (int)regs.GetField64(ismBitsArray[0], MODS_ISM_NMEAS_LITE_V2_DIV),
                (int)regs.GetField64(ismBitsArray[0], MODS_ISM_NMEAS_LITE_V2_DIV_CLK_IN),
                (int)regs.GetField64(ismBitsArray[0], MODS_ISM_NMEAS_LITE_V2_FIXED_DLY_SEL),
                (int)regs.GetField64(ismBitsArray[0], MODS_ISM_NMEAS_LITE_V2_REFCLK_SEL),
                (int)regs.GetField64(ismBitsArray[0], MODS_ISM_NMEAS_LITE_V2_COUNT));
            break;

        case LW_ISM_TSOSC_a3:
            Printf(Tee::PriNormal,
                "{ tsosc_a3:%d.%d.%d.%d.%d.%d.%d.%d }",
                (int)regs.GetField64(ismBitsArray[0], MODS_ISM_TSOSC_A3_COUNT),
                (int)regs.GetField64(ismBitsArray[0], MODS_ISM_TSOSC_A3_JTAG_OVERWRITE),
                (int)regs.GetField64(ismBitsArray[0], MODS_ISM_TSOSC_A3_OUT_DIV),
                (int)regs.GetField64(ismBitsArray[0], MODS_ISM_TSOSC_A3_LOCAL_DURATION),
                (int)regs.GetField64(ismBitsArray[0], MODS_ISM_TSOSC_A3_CLAMP_DEBUG_CLKOUT),
                (int)regs.GetField64(ismBitsArray[0], MODS_ISM_TSOSC_A3_ADJ),
                (int)regs.GetField64(ismBitsArray[0], MODS_ISM_TSOSC_A3_IDDQ),
                (int)regs.GetField64(ismBitsArray[0], MODS_ISM_TSOSC_A3_EN));
            break;

        case LW_ISM_ROSC_bin_aging_v1:
            Printf(Tee::PriNormal,
                "{ rosc_bin_aging_v1:%d.%d.%d.%d.%d.%d.%d.%d.%d.%d.%d.%d }",
                (int)regs.GetField64(ismBitsArray[1], MODS_ISM_ROSC_BIN_AGING_V1_READY),
                (int)regs.GetField64(ismBitsArray[1], MODS_ISM_ROSC_BIN_AGING_V1_DCD_HI_COUNT),
                ((int)regs.GetField64(ismBitsArray[1], MODS_ISM_ROSC_BIN_AGING_V1_COUNT_HIGH) <<
                       regs.LookupMaskSize(MODS_ISM_ROSC_BIN_AGING_V1_COUNT_LOW) |
                (int)regs.GetField64(ismBitsArray[0], MODS_ISM_ROSC_BIN_AGING_V1_COUNT_LOW)),
                (int)regs.GetField64(ismBitsArray[0], MODS_ISM_ROSC_BIN_AGING_V1_OUT_SEL),
                (int)regs.GetField64(ismBitsArray[0], MODS_ISM_ROSC_BIN_AGING_V1_DCD_EN),
                (int)regs.GetField64(ismBitsArray[0], MODS_ISM_ROSC_BIN_AGING_V1_RO_EN),
                (int)regs.GetField64(ismBitsArray[0], MODS_ISM_ROSC_BIN_AGING_V1_PG_DIS),
                (int)regs.GetField64(ismBitsArray[0], MODS_ISM_ROSC_BIN_AGING_V1_LOCAL_DURATION),
                (int)regs.GetField64(ismBitsArray[0], MODS_ISM_ROSC_BIN_AGING_V1_MODE),
                (int)regs.GetField64(ismBitsArray[0], MODS_ISM_ROSC_BIN_AGING_V1_IDDQ),
                (int)regs.GetField64(ismBitsArray[0], MODS_ISM_ROSC_BIN_AGING_V1_OUT_DIV),
                (int)regs.GetField64(ismBitsArray[0], MODS_ISM_ROSC_BIN_AGING_V1_SRC_SEL));
            break;

        case LW_ISM_NMEAS_lite:
            // This is needed when NMEAS_lite macro is used in MINI mode
            Printf(Tee::PriNormal,
                "{ nmeas_lite_v1:%d.%d.%d.%d.%d.%d.%d.%d.%d }",
                (int)regs.GetField64(ismBitsArray[0], MODS_ISM_NMEAS_LITE_V1_NM_TRIM),
                (int)regs.GetField64(ismBitsArray[0], MODS_ISM_NMEAS_LITE_V1_NM_TAP),
                (int)regs.GetField64(ismBitsArray[0], MODS_ISM_NMEAS_LITE_V1_IDDQ),
                (int)regs.GetField64(ismBitsArray[0], MODS_ISM_NMEAS_LITE_V1_MODE),
                (int)regs.GetField64(ismBitsArray[0], MODS_ISM_NMEAS_LITE_V1_SEL),
                (int)regs.GetField64(ismBitsArray[0], MODS_ISM_NMEAS_LITE_V1_DIV),
                (int)regs.GetField64(ismBitsArray[0], MODS_ISM_NMEAS_LITE_V1_DIV_CLK_IN),
                (int)regs.GetField64(ismBitsArray[0], MODS_ISM_NMEAS_LITE_V1_FIXED_DLY_SEL),
                (int)regs.GetField64(ismBitsArray[0], MODS_ISM_NMEAS_LITE_V1_COUNT));
            break;

        case LW_ISM_aging:
            Printf(Tee::PriNormal,
                "{ ism_aging:%d.%d.%d.%d.%d.%d.%d.%d.%d.%d.%d.%d.%d }",
                (int)regs.GetField64(ismBitsArray[0], MODS_ISM_AGING_READY),
                (int)regs.GetField64(ismBitsArray[0], MODS_ISM_AGING_COUNT),
                (int)regs.GetField64(ismBitsArray[0], MODS_ISM_AGING_OUTPUT_CLAMP),
                (int)regs.GetField64(ismBitsArray[0], MODS_ISM_AGING_EN),
                (int)regs.GetField64(ismBitsArray[0], MODS_ISM_AGING_JTAG_OVERRIDE),
                (int)regs.GetField64(ismBitsArray[0], MODS_ISM_AGING_SRC_CLK_SEL),
                (int)regs.GetField64(ismBitsArray[0], MODS_ISM_AGING_NACT_SEL),
                (int)regs.GetField64(ismBitsArray[0], MODS_ISM_AGING_PACT_SEL),
                (int)regs.GetField64(ismBitsArray[0], MODS_ISM_AGING_LOCAL_DURATION),
                (int)regs.GetField64(ismBitsArray[0], MODS_ISM_AGING_MODE),
                (int)regs.GetField64(ismBitsArray[0], MODS_ISM_AGING_IDDQ),
                (int)regs.GetField64(ismBitsArray[0], MODS_ISM_AGING_OUT_DIV),
                (int)regs.GetField64(ismBitsArray[0], MODS_ISM_AGING_SRC_SEL));
            break;

        case LW_ISM_aging_v1:
            Printf(Tee::PriNormal,
                "{ ism_aging_v1:%d.%d.%d.%d.%d.%d.%d.%d.%d.%d.%d }",
                (int)regs.GetField64(ismBitsArray[0], MODS_ISM_AGING_V1_READY),
                (int)regs.GetField64(ismBitsArray[0], MODS_ISM_AGING_V1_COUNT),
                (int)regs.GetField64(ismBitsArray[0], MODS_ISM_AGING_V1_EN),
                (int)regs.GetField64(ismBitsArray[0], MODS_ISM_AGING_V1_SRC_CLK_SEL),
                (int)regs.GetField64(ismBitsArray[0], MODS_ISM_AGING_V1_NACT_SEL),
                (int)regs.GetField64(ismBitsArray[0], MODS_ISM_AGING_V1_PACT_SEL),
                (int)regs.GetField64(ismBitsArray[0], MODS_ISM_AGING_V1_LOCAL_DURATION),
                (int)regs.GetField64(ismBitsArray[0], MODS_ISM_AGING_V1_MODE),
                (int)regs.GetField64(ismBitsArray[0], MODS_ISM_AGING_V1_IDDQ),
                (int)regs.GetField64(ismBitsArray[0], MODS_ISM_AGING_V1_OUT_DIV),
                (int)regs.GetField64(ismBitsArray[0], MODS_ISM_AGING_V1_SRC_SEL));
            break;

        case LW_ISM_MINI_2clk_v2:
            Printf(Tee::PriNormal, "{ mini2clkV2:%d.%d.%d.%d.%d.%d.%d.%d.%d.%d.%d.%d.%d.%d }",
                   (int)regs.GetField64(ismBitsArray[0], MODS_ISM_MINI_2CLK_V2_SRC_SEL),
                   (int)regs.GetField64(ismBitsArray[0], MODS_ISM_MINI_2CLK_V2_OUT_DIV),
                   (int)regs.GetField64(ismBitsArray[0], MODS_ISM_MINI_2CLK_V2_IDDQ),
                   (int)regs.GetField64(ismBitsArray[0], MODS_ISM_MINI_2CLK_V2_MODE),
                   (int)regs.GetField64(ismBitsArray[0], MODS_ISM_MINI_2CLK_V2_INIT),
                   (int)regs.GetField64(ismBitsArray[0], MODS_ISM_MINI_2CLK_V2_FINIT),
                   (int)regs.GetField64(ismBitsArray[0], MODS_ISM_MINI_2CLK_V2_REFCLK_SEL),
                   (int)regs.GetField64(ismBitsArray[0], MODS_ISM_MINI_2CLK_V2_HOLD_ENABLE),
                   (int)regs.GetField64(ismBitsArray[0], MODS_ISM_MINI_2CLK_V2_HOLD_SEL),
                   (int)regs.GetField64(ismBitsArray[0], MODS_ISM_MINI_2CLK_V2_HOLD_SEL_CAPTURE),
                   (int)regs.GetField64(ismBitsArray[0], MODS_ISM_MINI_2CLK_V2_HOLD_SEL_LAUNCH),
                   (int)regs.GetField64(ismBitsArray[0], MODS_ISM_MINI_2CLK_V2_HOLD_SW_DISABLE_MISMATCH1), //$
                   (int)regs.GetField64(ismBitsArray[0], MODS_ISM_MINI_2CLK_V2_COUNT),
                   (int)regs.GetField64(ismBitsArray[0], MODS_ISM_MINI_2CLK_V2_HOLD_MISMATCH_FLOP)); //$
            break;

        case LW_ISM_MINI_2clk_dbg_v2:
            Printf(Tee::PriNormal, "{ mini2clkV2:%d.%d.%d.%d.%d.%d.%d.%d.%d.%d.%d.%d.%d.%d.%d }",
                   (int)regs.GetField64(ismBitsArray[0], MODS_ISM_MINI_2CLK_DBG_V2_SRC_SEL),
                   (int)regs.GetField64(ismBitsArray[0], MODS_ISM_MINI_2CLK_DBG_V2_OUT_DIV),
                   (int)regs.GetField64(ismBitsArray[0], MODS_ISM_MINI_2CLK_DBG_V2_IDDQ),
                   (int)regs.GetField64(ismBitsArray[0], MODS_ISM_MINI_2CLK_DBG_V2_MODE),
                   (int)regs.GetField64(ismBitsArray[0], MODS_ISM_MINI_2CLK_DBG_V2_INIT),
                   (int)regs.GetField64(ismBitsArray[0], MODS_ISM_MINI_2CLK_DBG_V2_FINIT),
                   (int)regs.GetField64(ismBitsArray[0], MODS_ISM_MINI_2CLK_DBG_V2_REFCLK_SEL),
                   (int)regs.GetField64(ismBitsArray[0], MODS_ISM_MINI_2CLK_DBG_V2_CLMP_DEBUG_CLKOUT),         //$
                   (int)regs.GetField64(ismBitsArray[0], MODS_ISM_MINI_2CLK_DBG_V2_HOLD_ENABLE),
                   (int)regs.GetField64(ismBitsArray[0], MODS_ISM_MINI_2CLK_DBG_V2_HOLD_SEL),
                   (int)regs.GetField64(ismBitsArray[0], MODS_ISM_MINI_2CLK_DBG_V2_HOLD_SEL_CAPTURE),          //$
                   (int)regs.GetField64(ismBitsArray[0], MODS_ISM_MINI_2CLK_DBG_V2_HOLD_SEL_LAUNCH),           //$
                   (int)regs.GetField64(ismBitsArray[0], MODS_ISM_MINI_2CLK_DBG_V2_HOLD_SW_DISABLE_MISMATCH1), //$
                   (int)regs.GetField64(ismBitsArray[0], MODS_ISM_MINI_2CLK_DBG_V2_COUNT),
                   (int)regs.GetField64(ismBitsArray[0], MODS_ISM_MINI_2CLK_DBG_V2_HOLD_MISMATCH_FLOP)); //$
            break;

        case LW_ISM_TSOSC_a2      :
            Printf(Tee::PriNormal, "{ tsosc_a2:%d.%d.%d.%d.%d.%d.%d }",
              (int)regs.GetField64(ismBitsArray[0], MODS_ISM_TSOSC_A2_COUNT),
              (int)regs.GetField64(ismBitsArray[0], MODS_ISM_TSOSC_A2_OUT_DIV),
              (int)regs.GetField64(ismBitsArray[0], MODS_ISM_TSOSC_A2_LOCAL_DURATION),
              (int)regs.GetField64(ismBitsArray[0], MODS_ISM_TSOSC_A2_CLMP_DEBUG_CLKOUT),
              (int)regs.GetField64(ismBitsArray[0], MODS_ISM_TSOSC_A2_ADJ),
              (int)regs.GetField64(ismBitsArray[0], MODS_ISM_TSOSC_A2_IDDQ),
              (int)regs.GetField64(ismBitsArray[0], MODS_ISM_TSOSC_A2_EN));
            break;

        case LW_ISM_MINI_2clk_dbg_v3:
            Printf(Tee::PriNormal, "{ mini_2clk_dbg_v3:%d.%d.%d.%d.%d.%d.%d.%d.%d.%d.%d.%d.%d.%d }", //$
                   (int)regs.GetField64(ismBitsArray[0], MODS_ISM_MINI_2CLK_DBG_V3_SRC_SEL),
                   (int)regs.GetField64(ismBitsArray[0], MODS_ISM_MINI_2CLK_DBG_V3_OUT_DIV),
                   (int)regs.GetField64(ismBitsArray[0], MODS_ISM_MINI_2CLK_DBG_V3_IDDQ),
                   (int)regs.GetField64(ismBitsArray[0], MODS_ISM_MINI_2CLK_DBG_V3_MODE),
                   (int)regs.GetField64(ismBitsArray[0], MODS_ISM_MINI_2CLK_DBG_V3_INIT),
                   (int)regs.GetField64(ismBitsArray[0], MODS_ISM_MINI_2CLK_DBG_V3_FINIT),
                   (int)regs.GetField64(ismBitsArray[0], MODS_ISM_MINI_2CLK_DBG_V3_REFCLK_SEL),
                   (int)regs.GetField64(ismBitsArray[0], MODS_ISM_MINI_2CLK_DBG_V3_HOLD_ENABLE),
                   (int)regs.GetField64(ismBitsArray[0], MODS_ISM_MINI_2CLK_DBG_V3_HOLD_SEL),
                   (int)regs.GetField64(ismBitsArray[0], MODS_ISM_MINI_2CLK_DBG_V3_HOLD_SEL_CAPTURE),          //$
                   (int)regs.GetField64(ismBitsArray[0], MODS_ISM_MINI_2CLK_DBG_V3_HOLD_SEL_LAUNCH),           //$
                   (int)regs.GetField64(ismBitsArray[0], MODS_ISM_MINI_2CLK_DBG_V3_HOLD_SW_DISABLE_MISMATCH1), //$
                   (int)regs.GetField64(ismBitsArray[0], MODS_ISM_MINI_2CLK_DBG_V3_COUNT),
                   (int)regs.GetField64(ismBitsArray[0], MODS_ISM_MINI_2CLK_DBG_V3_HOLD_MISMATCH_FLOP));//$
            break;
        case LW_ISM_aging_v2:
            Printf(Tee::PriNormal,
                "{ ism_aging_v1:%d.%d.%d.%d.%d.%d.%d.%d.%d.%d.%d.%d }",
                (int)regs.GetField64(ismBitsArray[0], MODS_ISM_AGING_V2_READY),
                (int)regs.GetField64(ismBitsArray[0], MODS_ISM_AGING_V2_COUNT),
                (int)regs.GetField64(ismBitsArray[0], MODS_ISM_AGING_V2_EN),
                (int)regs.GetField64(ismBitsArray[0], MODS_ISM_AGING_V2_SRC_CLK_SEL),
                (int)regs.GetField64(ismBitsArray[0], MODS_ISM_AGING_V2_NACT_SEL),
                (int)regs.GetField64(ismBitsArray[0], MODS_ISM_AGING_V2_PACT_SEL),
                (int)regs.GetField64(ismBitsArray[0], MODS_ISM_AGING_V2_LOCAL_DURATION),
                (int)regs.GetField64(ismBitsArray[0], MODS_ISM_AGING_V2_MODE),
                (int)regs.GetField64(ismBitsArray[0], MODS_ISM_AGING_V2_IDDQ),
                (int)regs.GetField64(ismBitsArray[0], MODS_ISM_AGING_V2_OUT_DIV),
                (int)regs.GetField64(ismBitsArray[0], MODS_ISM_AGING_V2_SRC_SEL),
                (int)regs.GetField64(ismBitsArray[0], MODS_ISM_AGING_V2_JTAG_OVERRIDE));
            break;

        case LW_ISM_ROSC_bin_v1:
            Printf(Tee::PriNormal,
                "{ rosc_bin_v1:%d.%d.%d.%d.%d.%d.%d.%d.%d.%d }",
                (int)regs.GetField64(ismBitsArray[0], MODS_ISM_ROSC_BIN_V1_READY),
                (int)regs.GetField64(ismBitsArray[0], MODS_ISM_ROSC_BIN_V1_DCD_HI_COUNT),
                (int)regs.GetField64(ismBitsArray[0], MODS_ISM_ROSC_BIN_V1_COUNT),
                (int)regs.GetField64(ismBitsArray[0], MODS_ISM_ROSC_BIN_V1_OUT_SEL),
                (int)regs.GetField64(ismBitsArray[0], MODS_ISM_ROSC_BIN_V1_DCD_EN),
                (int)regs.GetField64(ismBitsArray[0], MODS_ISM_ROSC_BIN_V1_LOCAL_DURATION),
                (int)regs.GetField64(ismBitsArray[0], MODS_ISM_ROSC_BIN_V1_MODE),
                (int)regs.GetField64(ismBitsArray[0], MODS_ISM_ROSC_BIN_V1_IDDQ),
                (int)regs.GetField64(ismBitsArray[0], MODS_ISM_ROSC_BIN_V1_OUT_DIV),
                (int)regs.GetField64(ismBitsArray[0], MODS_ISM_ROSC_BIN_V1_SRC_SEL));
            break;

        case LW_ISM_ROSC_bin_v2:
            Printf(Tee::PriNormal,
                "{ rosc_bin_v2:%d.%d.%d.%d.%d.%d.%d.%d }",
                (int)regs.GetField64(ismBitsArray[0], MODS_ISM_ROSC_BIN_V2_READY),
                (int)regs.GetField64(ismBitsArray[0], MODS_ISM_ROSC_BIN_V2_COUNT),
                (int)regs.GetField64(ismBitsArray[0], MODS_ISM_ROSC_BIN_V2_EN),
                (int)regs.GetField64(ismBitsArray[0], MODS_ISM_ROSC_BIN_V2_LOCAL_DURATION),
                (int)regs.GetField64(ismBitsArray[0], MODS_ISM_ROSC_BIN_V2_JTAG_OVERRIDE),
                (int)regs.GetField64(ismBitsArray[0], MODS_ISM_ROSC_BIN_V2_IDDQ),
                (int)regs.GetField64(ismBitsArray[0], MODS_ISM_ROSC_BIN_V2_OUT_DIV),
                (int)regs.GetField64(ismBitsArray[0], MODS_ISM_ROSC_BIN_V2_SRC_SEL));
            break;

        case LW_ISM_ROSC_bin_v3:
            Printf(Tee::PriNormal,
                "{ rosc_bin_v3:%d.%d.%d.%d.%d.%d.%d.%d.%d }",
                (int)regs.GetField64(ismBitsArray[0], MODS_ISM_ROSC_BIN_V3_READY),
                (int)regs.GetField64(ismBitsArray[0], MODS_ISM_ROSC_BIN_V3_COUNT),
                (int)regs.GetField64(ismBitsArray[0], MODS_ISM_ROSC_BIN_V3_EN),
                (int)regs.GetField64(ismBitsArray[0], MODS_ISM_ROSC_BIN_V3_LOCAL_DURATION),
                (int)regs.GetField64(ismBitsArray[0], MODS_ISM_ROSC_BIN_V3_JTAG_OVERRIDE),
                (int)regs.GetField64(ismBitsArray[0], MODS_ISM_ROSC_BIN_V3_IDDQ),
                (int)regs.GetField64(ismBitsArray[0], MODS_ISM_ROSC_BIN_V3_OUT_DIV),
                (int)regs.GetField64(ismBitsArray[0], MODS_ISM_ROSC_BIN_V3_SRC_SEL),
                (int)regs.GetField64(ismBitsArray[0], MODS_ISM_ROSC_BIN_V3_OUT_SEL));
            break;

        case LW_ISM_ROSC_bin_v4:
            Printf(Tee::PriNormal,
                "{ rosc_bin_v4:%d.%d.%d.%d.%d.%d.%d.%d.%d.%d }",
                (int)regs.GetField64(ismBitsArray[0], MODS_ISM_ROSC_BIN_V4_READY),
                (int)regs.GetField64(ismBitsArray[0], MODS_ISM_ROSC_BIN_V4_COUNT),
                (int)regs.GetField64(ismBitsArray[0], MODS_ISM_ROSC_BIN_V4_EN),
                (int)regs.GetField64(ismBitsArray[0], MODS_ISM_ROSC_BIN_V4_LOCAL_DURATION),
                (int)regs.GetField64(ismBitsArray[0], MODS_ISM_ROSC_BIN_V4_JTAG_OVERRIDE),
                (int)regs.GetField64(ismBitsArray[0], MODS_ISM_ROSC_BIN_V4_IDDQ),
                (int)regs.GetField64(ismBitsArray[0], MODS_ISM_ROSC_BIN_V4_OUT_DIV),
                (int)regs.GetField64(ismBitsArray[0], MODS_ISM_ROSC_BIN_V4_SRC_SEL),
                (int)regs.GetField64(ismBitsArray[0], MODS_ISM_ROSC_BIN_V4_OUT_SEL),
                (int)regs.GetField64(ismBitsArray[0], MODS_ISM_ROSC_BIN_V4_DELAY_SEL));
            break;

        default:
            MASSERT(!"Unknown ISM type!");
            return RC::SOFTWARE_ERROR;
    }
    return OK;
}
//------------------------------------------------------------------------------
// This routine handles extracting the ISM's raw count field from the ISMs serial
// bits.
UINT64 GpuIsm::ExtractISMRawCount(UINT32 ismType, vector<UINT64>& ismBits)
{
    RegHal &regs = GetGpuSub()->Regs();
    UINT64 rawCount = 0;
    // extract the value
    switch (ismType)
    {
        case LW_ISM_MINI_1clk     :
            rawCount = regs.GetField64(ismBits[0], MODS_ISM_MINI_1CLK_COUNT);
            break;

        case LW_ISM_MINI_1clk_dbg :
            rawCount = regs.GetField64(ismBits[0], MODS_ISM_MINI_1CLK_DBG_COUNT);
            break;

        case LW_ISM_MINI_1clk_no_ext_ro:
            rawCount = regs.GetField64(ismBits[0], MODS_ISM_MINI_1CLK_NO_EXT_RO_COUNT);
            break;

        case LW_ISM_MINI_1clk_no_ext_ro_dbg:
            rawCount = regs.GetField64(ismBits[0], MODS_ISM_MINI_1CLK_NO_EXT_RO_DBG_COUNT);
            break;

        case LW_ISM_MINI_2clk_no_ext_ro:
        case LW_ISM_MINI_2clk     :
            rawCount = regs.GetField64(ismBits[0], MODS_ISM_MINI_2CLK_COUNT);
            break;

        case LW_ISM_MINI_2clk_dbg  :
            rawCount = regs.GetField64(ismBits[0], MODS_ISM_MINI_2CLK_DBG_COUNT);
            break;

        // This ISMs have their own control/enable logic
        case LW_ISM_ROSC_comp     :
            rawCount = regs.GetField64(ismBits[0], MODS_ISM_ROSC_COMP_COUNT);
            break;

        case LW_ISM_ROSC_comp_v1  :
            rawCount = regs.GetField64(ismBits[0], MODS_ISM_ROSC_COMP_V1_COUNT);
            break;

        case LW_ISM_ROSC_comp_v2  :
            rawCount = regs.GetField64(ismBits[0], MODS_ISM_ROSC_COMP_V2_COUNT);
            break;

        case LW_ISM_ROSC_bin_metal:
            rawCount = regs.GetField64(ismBits[0], MODS_ISM_ROSC_BIN_METAL_COUNT);
            break;

        case LW_ISM_VNSENSE_ddly  :
            rawCount = regs.GetField64(ismBits[0], MODS_ISM_VNSENSE_DDLY_COUNT);
            break;

        case LW_ISM_TSOSC_a       :
            rawCount = regs.GetField64(ismBits[0], MODS_ISM_TSOSC_A_COUNT);
            break;

        // this one has different definitions for GF117 & GK10x
        case LW_ISM_ROSC_bin      :
            rawCount = regs.GetField64(ismBits[0], MODS_ISM_ROSC_BIN_COUNT);
            break;

        case LW_ISM_TSOSC_a3:
            rawCount =  regs.GetField64(ismBits[0], MODS_ISM_TSOSC_A3_COUNT);
            break;

        case LW_ISM_ROSC_bin_aging_v1:
            // Bits 1:0 come from COUNT_LOW (63:62)
            // Bits 15:2 come from COUNT_HI (77:64)
            // Bits 31:16 come from DCD_HI_COUNT (93:78)
            rawCount =  (regs.GetField64(ismBits[0], MODS_ISM_ROSC_BIN_AGING_V1_COUNT_LOW) |
                (regs.GetField64(ismBits[1], MODS_ISM_ROSC_BIN_AGING_V1_COUNT_HIGH) << 2)  |
                (regs.GetField64(ismBits[1], MODS_ISM_ROSC_BIN_AGING_V1_DCD_HI_COUNT) << 16));
            break;

        case LW_ISM_NMEAS_lite:
            // This is needed when NMEAS_lite macro is used in MINI mode
            rawCount = regs.GetField64(ismBits[0], MODS_ISM_NMEAS_LITE_V1_COUNT);
            break;

        case LW_ISM_aging:
            rawCount = regs.GetField64(ismBits[0], MODS_ISM_AGING_COUNT);
            break;

        case LW_ISM_aging_v1:
            rawCount = regs.GetField64(ismBits[0], MODS_ISM_AGING_V1_COUNT);
            break;

        case LW_ISM_MINI_2clk_v2:
            rawCount =  regs.GetField64(ismBits[0], MODS_ISM_MINI_2CLK_V2_COUNT);
            break;

        case LW_ISM_MINI_2clk_dbg_v2:
            rawCount = regs.GetField64(ismBits[0], MODS_ISM_MINI_2CLK_DBG_V2_COUNT);
            break;

        case LW_ISM_ROSC_bin_aging:
            rawCount = regs.GetField64(ismBits[1], MODS_ISM_ROSC_BIN_AGING_COUNT_HIGH) <<
                       regs.LookupMaskSize(MODS_ISM_ROSC_BIN_AGING_COUNT_LOW) |
                       regs.GetField64(ismBits[0], MODS_ISM_ROSC_BIN_AGING_COUNT_LOW);
            break;

        case LW_ISM_TSOSC_a2:
            rawCount = regs.GetField64(ismBits[0], MODS_ISM_TSOSC_A2_COUNT);
            break;

        case LW_ISM_TSOSC_a4:
            rawCount = regs.GetField64(ismBits[0], MODS_ISM_TSOSC_A4_COUNT);
            break;

        case LW_ISM_NMEAS_lite_v2:
            rawCount = regs.GetField64(ismBits[0], MODS_ISM_NMEAS_LITE_V2_COUNT);
            break;

        case LW_ISM_aging_v3:
            rawCount = regs.GetField64(ismBits[0], MODS_ISM_AGING_V3_COUNT);
            break;

        case LW_ISM_aging_v4:
            rawCount = regs.GetField64(ismBits[0], MODS_ISM_AGING_V4_COUNT);
            break;

        case LW_ISM_MINI_2clk_dbg_v3:
            rawCount = regs.GetField64(ismBits[0], MODS_ISM_MINI_2CLK_DBG_V3_COUNT);
            break;

        case LW_ISM_aging_v2:
            rawCount = regs.GetField64(ismBits[0], MODS_ISM_AGING_V2_COUNT);
            break;

        case LW_ISM_ROSC_bin_v1:
            rawCount = regs.GetField64(ismBits[0], MODS_ISM_ROSC_BIN_V1_COUNT);
            break;

        case LW_ISM_ROSC_bin_v2:
            rawCount = regs.GetField64(ismBits[0], MODS_ISM_ROSC_BIN_V2_COUNT);
            break;

        case LW_ISM_ROSC_bin_v3:
            rawCount = regs.GetField64(ismBits[0], MODS_ISM_ROSC_BIN_V3_COUNT);
            break;

        case LW_ISM_ROSC_bin_v4:
            rawCount = regs.GetField64(ismBits[0], MODS_ISM_ROSC_BIN_V4_COUNT);
            break;

        case LW_ISM_imon      :
            rawCount = regs.GetField64(ismBits[0], MODS_ISM_IMON_COUNT);
            break;

        case LW_ISM_imon_v2    :
            rawCount = regs.GetField64(ismBits[0], MODS_ISM_IMON_V2_COUNT);
            break;

        case LW_ISM_lwrrent_sink: // there are no count fields in the ISM, but lets not assert.
            break;

        default:
            MASSERT(!"Unknown ISM in Jtag chain\n");
            break;
    }
    return rawCount;
}

//------------------------------------------------------------------------------
UINT32 GpuIsm::ExtractISMRawDcdCount(UINT32 ismType, vector<UINT64>& ismBits)
{
    RegHal &regs = GetGpuSub()->Regs();

    switch (ismType)
    {
        case LW_ISM_aging_v3:
            return (UINT32)(regs.GetField64(ismBits[1], MODS_ISM_AGING_V3_DCD_COUNT_HIGH) <<
                    regs.LookupMaskSize(MODS_ISM_AGING_V3_DCD_COUNT_LOW) |
                    regs.GetField64(ismBits[0], MODS_ISM_AGING_V3_DCD_COUNT_LOW));

        case LW_ISM_aging_v4:
            return (UINT32)(regs.GetField64(ismBits[1], MODS_ISM_AGING_V4_DCD_COUNT_HIGH) <<
                            regs.LookupMaskSize(MODS_ISM_AGING_V4_DCD_COUNT_LOW) |
                            regs.GetField64(ismBits[0], MODS_ISM_AGING_V4_DCD_COUNT_LOW));

        default:
            return (UINT32)ExtractISMRawCount(ismType, ismBits);
    }
}

//------------------------------------------------------------------------------
// Return the number of bits for a given ISM type.
// Note: This utilizes defines from GK10x GPUs. Override this routine if your
// GPU doesn't comform to those sizes.
//
UINT32 GpuIsm::GetISMSize(IsmTypes ismType)
{
    RegHal &regs = GetGpuSub()->Regs();

    switch (ismType)
    {
        case LW_ISM_aging                  : return regs.LookupAddress(MODS_ISM_AGING_WIDTH);                  //$
        case LW_ISM_aging_v1               : return regs.LookupAddress(MODS_ISM_AGING_V1_WIDTH);               //$
        case LW_ISM_aging_v2               : return regs.LookupAddress(MODS_ISM_AGING_V2_WIDTH);               //$
        case LW_ISM_aging_v3               : return regs.LookupAddress(MODS_ISM_AGING_V3_WIDTH);               //$
        case LW_ISM_aging_v4               : return regs.LookupAddress(MODS_ISM_AGING_V4_WIDTH);               //$
        case LW_ISM_cpm                    : return regs.LookupAddress(MODS_ISM_CPM_WIDTH);                    //$
        case LW_ISM_cpm_v1                 : return regs.LookupAddress(MODS_ISM_CPM_V1_WIDTH);                 //$
        case LW_ISM_cpm_v2                 : return regs.LookupAddress(MODS_ISM_CPM_V2_WIDTH);                 //$
        case LW_ISM_cpm_v3                 : return regs.LookupAddress(MODS_ISM_CPM_V3_WIDTH);                 //$
        case LW_ISM_cpm_v4                 : return regs.LookupAddress(MODS_ISM_CPM_V4_WIDTH);                 //$
        case LW_ISM_cpm_v5                 : return regs.LookupAddress(MODS_ISM_CPM_V5_WIDTH);                 //$
        case LW_ISM_cpm_v6                 : return regs.LookupAddress(MODS_ISM_CPM_V6_WIDTH);                 //$
        case LW_ISM_ctrl                   : return regs.LookupAddress(MODS_ISM_CTRL_WIDTH);                   //$           
        case LW_ISM_ctrl_v2                : return regs.LookupAddress(MODS_ISM_CTRL2_WIDTH);                  //$
        case LW_ISM_ctrl_v3                : return regs.LookupAddress(MODS_ISM_CTRL3_WIDTH);                  //$
        case LW_ISM_ctrl_v4                : return regs.LookupAddress(MODS_ISM_CTRL4_WIDTH);                  //$
        case LW_ISM_HOLD                   : return regs.LookupAddress(MODS_ISM_HOLD_WIDTH);                   //$
        case LW_ISM_MINI_1clk              : return regs.LookupAddress(MODS_ISM_MINI_1CLK_WIDTH);              //$
        case LW_ISM_MINI_1clk_dbg          : return regs.LookupAddress(MODS_ISM_MINI_1CLK_DBG_WIDTH);          //$
        case LW_ISM_MINI_1clk_no_ext_ro    : return regs.LookupAddress(MODS_ISM_MINI_1CLK_NO_EXT_RO_WIDTH);    //$
        case LW_ISM_MINI_1clk_no_ext_ro_dbg: return regs.LookupAddress(MODS_ISM_MINI_1CLK_NO_EXT_RO_DBG_WIDTH);//$
        case LW_ISM_MINI_2clk              : return regs.LookupAddress(MODS_ISM_MINI_2CLK_WIDTH);              //$
        case LW_ISM_MINI_2clk_v2           : return regs.LookupAddress(MODS_ISM_MINI_2CLK_V2_WIDTH);           //$
        case LW_ISM_MINI_2clk_dbg          : return regs.LookupAddress(MODS_ISM_MINI_2CLK_DBG_WIDTH);          //$
        case LW_ISM_MINI_2clk_dbg_v2       : return regs.LookupAddress(MODS_ISM_MINI_2CLK_DBG_V2_WIDTH);       //$
        case LW_ISM_MINI_2clk_dbg_v3       : return regs.LookupAddress(MODS_ISM_MINI_2CLK_DBG_V3_WIDTH);       //$
        case LW_ISM_MINI_2clk_no_ext_ro    : return regs.LookupAddress(MODS_ISM_MINI_2CLK_WIDTH);              //$
        case LW_ISM_NMEAS_lite             : return regs.LookupAddress(MODS_ISM_NMEAS_LITE_V1_WIDTH);          //$
        case LW_ISM_NMEAS_lite_v2          : return regs.LookupAddress(MODS_ISM_NMEAS_LITE_V2_WIDTH);          //$
        case LW_ISM_NMEAS_lite_v3          : return regs.LookupAddress(MODS_ISM_NMEAS_LITE_V3_WIDTH);          //$
        case LW_ISM_NMEAS_lite_v4          : return regs.LookupAddress(MODS_ISM_NMEAS_LITE_V4_WIDTH);          //$
        case LW_ISM_NMEAS_lite_v5          : return regs.LookupAddress(MODS_ISM_NMEAS_LITE_V5_WIDTH);          //$
        case LW_ISM_NMEAS_lite_v6          : return regs.LookupAddress(MODS_ISM_NMEAS_LITE_V6_WIDTH);          //$
        case LW_ISM_NMEAS_lite_v7          : return regs.LookupAddress(MODS_ISM_NMEAS_LITE_V7_WIDTH);          //$
        case LW_ISM_NMEAS_lite_v8          : return regs.LookupAddress(MODS_ISM_NMEAS_LITE_V8_WIDTH);          //$
        case LW_ISM_NMEAS_lite_v9          : return regs.LookupAddress(MODS_ISM_NMEAS_LITE_V9_WIDTH);          //$
        case LW_ISM_NMEAS_v2               : return regs.LookupAddress(MODS_ISM_NMEAS_WIDTH);                  //$
        case LW_ISM_NMEAS_v3               : return regs.LookupAddress(MODS_ISM_NMEAS3_WIDTH);                 //$
        case LW_ISM_NMEAS_v4               : return regs.LookupAddress(MODS_ISM_NMEAS4_WIDTH);                 //$
        case LW_ISM_ROSC_bin               : return regs.LookupAddress(MODS_ISM_ROSC_BIN_WIDTH);               //$
        case LW_ISM_ROSC_bin_v1            : return regs.LookupAddress(MODS_ISM_ROSC_BIN_V1_WIDTH);            //$
        case LW_ISM_ROSC_bin_v2            : return regs.LookupAddress(MODS_ISM_ROSC_BIN_V2_WIDTH);            //$
        case LW_ISM_ROSC_bin_v3            : return regs.LookupAddress(MODS_ISM_ROSC_BIN_V3_WIDTH);            //$
        case LW_ISM_ROSC_bin_v4            : return regs.LookupAddress(MODS_ISM_ROSC_BIN_V4_WIDTH);            //$
        case LW_ISM_ROSC_bin_aging         : return regs.LookupAddress(MODS_ISM_ROSC_BIN_AGING_WIDTH);         //$
        case LW_ISM_ROSC_bin_aging_v1      : return regs.LookupAddress(MODS_ISM_ROSC_BIN_AGING_V1_WIDTH);      //$
        case LW_ISM_ROSC_bin_metal         : return regs.LookupAddress(MODS_ISM_ROSC_BIN_METAL_WIDTH);         //$
        case LW_ISM_ROSC_comp              : return regs.LookupAddress(MODS_ISM_ROSC_COMP_WIDTH);              //$
        case LW_ISM_ROSC_comp_v1           : return regs.LookupAddress(MODS_ISM_ROSC_COMP_V1_WIDTH);           //$
        case LW_ISM_ROSC_comp_v2           : return regs.LookupAddress(MODS_ISM_ROSC_COMP_V2_WIDTH);           //$
        case LW_ISM_TSOSC_a                : return regs.LookupAddress(MODS_ISM_TSOSC_A_WIDTH);                //$
        case LW_ISM_TSOSC_a2               : return regs.LookupAddress(MODS_ISM_TSOSC_A2_WIDTH);               //$
        case LW_ISM_TSOSC_a3               : return regs.LookupAddress(MODS_ISM_TSOSC_A3_WIDTH);               //$
        case LW_ISM_TSOSC_a4               : return regs.LookupAddress(MODS_ISM_TSOSC_A4_WIDTH);               //$
        case LW_ISM_VNSENSE_ddly           : return regs.LookupAddress(MODS_ISM_VNSENSE_DDLY_WIDTH);           //$
        case LW_ISM_imon                   : return regs.LookupAddress(MODS_ISM_IMON_WIDTH);                   //$
        case LW_ISM_imon_v2                : return regs.LookupAddress(MODS_ISM_IMON_V2_WIDTH);                //$
        case LW_ISM_lwrrent_sink           : return regs.LookupAddress(MODS_ISM_LWRRENT_SINK_V1_WIDTH);        //$
        default:
            Printf(Tee::PriError, "Unknown ISM type:%d\n", ismType);
            MASSERT(!"Returning zero for ISM size!");
            return 0;
    }
}

//------------------------------------------------------------------------------
// This API will setup all the ISM sub-controllers to run an experiment.
RC GpuIsm::SetupISMSubController
(
    UINT32 dur          // number of clock cycles to run the experiment
    ,UINT32 delay       // number of clock cycles to delay after the trigger
                        // before the ROs start counting.
    ,INT32 trigger      // trigger sourc to use
                        // (0=off, 1=priv write, 2=perf mon trigger, 3=now)
    ,INT32 loopMode     // 1= continue looping the experiment until the haltMode
                        //    has been set to 1.
                        // 0= perform a single experiment
    ,INT32 haltMode     // 1= halt the lwrrently running experiment. Used in
                        //    conjunction with loopMode to stop the repeating the
                        //    same experiment.
                        // 0= don't halt the current experiment.
    ,INT32 ctrlSrcSel   // 1 = jtag 0 = priv access
)
{
    RC rc;
    const IsmChain * pOrigCtrl = m_pCachedISMControlChain;

    vector<IsmChainFilter> chainFilter;
    CHECK_RC(GetISMChains(&chainFilter, LW_ISM_ctrl,    -1, -1, ~0));
    CHECK_RC(GetISMChains(&chainFilter, LW_ISM_ctrl_v2, -1, -1, ~0));
    CHECK_RC(GetISMChains(&chainFilter, LW_ISM_ctrl_v3, -1, -1, ~0));
    CHECK_RC(GetISMChains(&chainFilter, LW_ISM_ctrl_v4, -1, -1, ~0));
    // remove the chain that has the PRIMARY controller.
    for (auto it = chainFilter.begin(); it != chainFilter.end(); it++)
    {
        if (it->pChain->ISMs[0].Flags & Ism::IsmFlags::PRIMARY)
        {
            chainFilter.erase(it);
            break;
        }
    }
    m_pCachedISMControlChain = pOrigCtrl;
    if (chainFilter.size() == 0)
    {
        Printf(Tee::PriError, "No ISM sub-controllers detected!\n");
        return RC::UNSUPPORTED_FUNCTION;
    }

    // We are going to be programming all of the SubControllers so it doesn't make sense to
    // try to cache the last ISM controller for subsequent calls to ReadNoiseMeasLiteOnChain
    // because in this mode we need to be looking at the PRIMARY controller which should have 
    // been the last controller to be setup.
    const bool bCacheController = false;

    for (UINT32 idx = 0; idx < chainFilter.size(); idx++)
    {
        CHECK_RC(SetupISMSubControllerOnChain(
            chainFilter[idx].pChain->Hdr.InstrId,
            chainFilter[idx].pChain->Hdr.Chiplet,
            dur,
            delay,
            trigger,
            loopMode,
            haltMode,
            ctrlSrcSel,
            bCacheController));
    }
    return (rc);
}

//------------------------------------------------------------------------------
// This API will setup all the ISM sub-controllers on a specific chain to run an experiment.
RC GpuIsm::SetupISMSubControllerOnChain
(
    INT32 chainId     // The Jtag chain
    ,INT32 chiplet    // The chiplet for this jtag chain.
    ,UINT32 dur       // number of clock cycles to run the experiment
    ,UINT32 delay     // number of clock cycles to delay after the trigger
                      // before the ROs start counting.
    ,INT32 trigger    // trigger sourc to use
                      // (0=off, 1=priv write, 2=perf mon trigger, 3=now)
    ,INT32 loopMode   // 1= continue looping the experiment until the haltMode
                      //    has been set to 1.
                      // 0= perform a single experiment
    ,INT32 haltMode   // 1= halt the lwrrently running experiment. Used in
                      //    conjunction with loopMode to stop the repeating the
                      //    same experiment.
                      // 0= don't halt the current experiment.
    ,INT32 ctrlSrcSel // 1 = jtag 0 = priv access
    ,bool bCacheController // true = save off the pChain info for subsequent use.
)
{
    RC rc;
    if (GetNumISMChains() == 0)
    {
        Printf(Tee::PriWarn, "SetupISMSubControllerOnChain: ISM table is empty!\n");
        return RC::UNSUPPORTED_FUNCTION;
    }

    // Find the chain with the chainId & chiplet. There should only be 1 chain. However there may
    // be multiple sub-controllers on the chain.
    vector<IsmChainFilter> chainFilter;
    CHECK_RC(GetISMChains(&chainFilter, LW_ISM_ctrl,    chainId, chiplet, ~0));
    CHECK_RC(GetISMChains(&chainFilter, LW_ISM_ctrl_v2, chainId, chiplet, ~0));
    CHECK_RC(GetISMChains(&chainFilter, LW_ISM_ctrl_v3, chainId, chiplet, ~0));
    CHECK_RC(GetISMChains(&chainFilter, LW_ISM_ctrl_v4, chainId, chiplet, ~0));
    if (chainFilter.size() == 0)
    {
        Printf(Tee::PriError,
               "SetupISMSubControllerOnChain:"
               " Unable to locate ISMSubController with chainId:0x%x chiplet:0x%x\n",
               chainId, chiplet);
        return RC::BAD_PARAMETER;
    }
    MASSERT(chainFilter.size() == 1);

    vector <UINT32> jtagArray;
    const IsmChain * pCtrl = chainFilter[0].pChain;

    // Save off this controller chain for subsequent calls to ReadNoiseMeasLiteOnChain()
    if (bCacheController)
    {
        m_pCachedISMControlChain = pCtrl;
    }

    CHECK_RC(ConfigureISMCtrl(pCtrl,
        jtagArray,
        true,       // enable & trigger
        dur,        // duration clock cycles
        delay,
        trigger,    // use the requested trigger.
        loopMode,
        haltMode,
        ctrlSrcSel));

    CHECK_RC(WriteHost2Jtag(pCtrl->Hdr.Chiplet,
                            pCtrl->Hdr.InstrId,
                            pCtrl->Hdr.Size,
                            jtagArray));

    return rc;
}

//------------------------------------------------------------------------------
RC GpuIsm::PollISMCtrlComplete(UINT32 *pComplete)
{
    RC rc;
    vector<UINT32> JtagArray;
    // m_pCachedISMControlChain is the last ISM controller that was programmed. This could be 
    // the PRIMARY controller or one of the chains with subControllers.
    const IsmChain *pChain = m_pCachedISMControlChain;
    if (nullptr == pChain)
    {
        return RC::UNSUPPORTED_FUNCTION;
    }
    CHECK_RC(m_pGpu->ReadHost2Jtag(pChain->Hdr.Chiplet,
                           pChain->Hdr.InstrId,
                           pChain->Hdr.Size,
                           &JtagArray));

    // There may be multiple sub-controllers on the chain. If so we need to wait for all of them
    // to complete.
    *pComplete = 1;
    for (size_t idx = 0; idx < pChain->ISMs.size(); idx++)
    {
        *pComplete &= IsExpComplete(pChain->ISMs[idx].Type, pChain->ISMs[idx].Offset, JtagArray);
    }
    return (rc);
}

//------------------------------------------------------------------------------
UINT32 GpuIsm::IsExpComplete
(
    UINT32 ismType,
    INT32 offset,
    const vector<UINT32> &JtagArray
)
{
    RegHal &regs = GetGpuSub()->Regs();
    UINT32 hi = 0; 
    UINT32 lo = 0; // not used but required by LookupField()
    switch (ismType)
    { 
        case LW_ISM_ctrl:       regs.LookupField(MODS_ISM_CTRL_DONE,  &hi, &lo); break;
        case LW_ISM_ctrl_v2:    regs.LookupField(MODS_ISM_CTRL2_DONE, &hi, &lo); break;
        case LW_ISM_ctrl_v3:    regs.LookupField(MODS_ISM_CTRL3_DONE, &hi, &lo); break;
        case LW_ISM_ctrl_v4:    regs.LookupField(MODS_ISM_CTRL4_DONE, &hi, &lo); break;
    }
    size_t completeBitNum = static_cast<size_t>(offset) + hi;
    size_t dwordNum = completeBitNum / 32;
    completeBitNum %= 32;
    return (static_cast<UINT32>((JtagArray[dwordNum] >> completeBitNum) & 1));
}

//------------------------------------------------------------------------------
// pass-thru mechanism to get access to the JTag chains using the Gpu.
RC GpuIsm::WriteHost2Jtag
(
    UINT32 chiplet,
    UINT32 instruction,
    UINT32 chainLength,
    const vector<UINT32> &inputData
)
{
    return(m_pGpu->WriteHost2Jtag(chiplet,instruction,chainLength,inputData));
}

//------------------------------------------------------------------------------
// pass-thru mechanism to get access to the JTag chains using the Gpu.
RC GpuIsm::ReadHost2Jtag
(
    UINT32 chiplet,
    UINT32 instruction,
    UINT32 chainLength,
    vector<UINT32> *pResult
)
{
    RC rc = m_pGpu->ReadHost2Jtag(chiplet,instruction,chainLength,pResult);
    return rc;
}

//------------------------------------------------------------------------------
RC GpuIsm::GetISMClkFreqKhz(UINT32 *clkSrcFreqKHz)
{
    RC rc;
    RegHal &regs = GetGpuSub()->Regs();
   
    // The Jtag clk is actualy running off of the PEX ref clock and then
    // passed through a programmable divider. So get the reference clk
    // and apply the divider value to get the actual clock value.
    // We ask for this value a lot of times so lets cache it to reduce the burden
    // on RM.
    if (!m_ClkSrcFreqKHz)
    {
        LwU32 pexClkKHz = 0;
        CHECK_RC(m_pGpu->GetClockSourceFreqKHz(LW2080_CTRL_CLK_SOURCE_PEXREFCLK,
                                               &pexClkKHz));
    
        UINT32 value;
        CHECK_RC(regs.Read32Priv(MODS_PTRIM_SYS_JTAGINTFC, &value));
        m_ClkSrcFreqKHz =
            pexClkKHz / (1 << regs.GetField64(value, MODS_PTRIM_SYS_JTAGINTFC_CLK_DIVSEL));
    }
    *clkSrcFreqKHz = m_ClkSrcFreqKHz;
    return OK;
}

//------------------------------------------------------------------------------
// Trigger an ISM experiment when the ISM trigger source = priv write.
// Note: The ISM master controller must be previously setup to trigger on a priv
//       register write for this call to have any effect.
// Note: This is not supported on newer GPUs (ie. Pascal+)
RC GpuIsm::TriggerISMExperiment()
{
    RegHal &regs = GetGpuSub()->Regs();
    if (regs.IsSupported(MODS_PTRIM_SYS_ISM_NM_CTRL))
    {
        regs.Write32(MODS_PTRIM_SYS_ISM_NM_CTRL,
                     regs.SetField(MODS_PTRIM_SYS_ISM_NM_CTRL_ISM_NM_PRI_TRIGGER, 1));
    
        Platform::Delay(50); // 50us, guessing this should be enough.
    
        regs.Write32(MODS_PTRIM_SYS_ISM_NM_CTRL, 
                     regs.SetField(MODS_PTRIM_SYS_ISM_NM_CTRL_ISM_NM_PRI_TRIGGER, 0));
        
        return OK;
    }
    else
        return RC::UNSUPPORTED_FUNCTION;
}

//------------------------------------------------------------------------------
INT32 GpuIsm::GetISMMode(PART_TYPES partType)
{
    return 0;
}

//------------------------------------------------------------------------------
/*virtual*/
RC GpuIsm::GetCPMChainCounts
(
    const IsmChain *pChain          // The chain configuration
    , vector<UINT32> &jtagArray     // Vector of raw bits for this chain
    , vector<CpmInfo> *pCpmResults  // Vector to hold the macro specific data
    , vector<CpmInfo> &cpmParams    // Vector to hold the filter paramters of the chains to read.
)
{
    RC rc;
    RegHal &regs = GetGpuSub()->Regs();
    vector<UINT64> ismBits = { 0 };
    for (size_t idx = 0; idx < pChain->ISMs.size(); idx++)
    {   // look for specific ISM parameter
        if (GetCpmParamIdx(cpmParams,
                           pChain->Hdr.Chiplet,
                           pChain->Hdr.InstrId,
                           pChain->ISMs[idx].Offset) < 0)
        {   // no match... move along.
            continue;
        }

        // Load the bits for a single macro into ismBits[]
        ismBits.resize(CallwlateVectorSize(pChain->ISMs[idx].Type, 64), 0);
        CHECK_RC(Utility::CopyBits(jtagArray,
                                   ismBits,
                                   pChain->ISMs[idx].Offset,
                                   0,
                                   GetISMSize(pChain->ISMs[idx].Type)));
        CpmInfo info = { 0 };
        info.hdr.chainId = pChain->Hdr.InstrId;
        info.hdr.chiplet = pChain->Hdr.Chiplet;
        info.hdr.offset  = pChain->ISMs[idx].Offset;
        switch (pChain->ISMs[idx].Type)
        {
            default:        // So far all chains with CPM ISMs don't have any other ISM types
                continue;   // but we don't to fail if they do. Just ignore them.
                
            case LW_ISM_cpm:
                // Load up the info
                info.hdr.version = 0;
                info.cpm1.enable      = (UINT32)regs.GetField64(ismBits[0], MODS_ISM_CPM_EN);
                info.cpm1.iddq        = (UINT32)regs.GetField64(ismBits[0], MODS_ISM_CPM_IDDQ);
                info.cpm1.resetFunc   = (UINT32)regs.GetField64(ismBits[0], MODS_ISM_CPM_RESET_FUNC);     //$
                info.cpm1.resetMask   = (UINT32)regs.GetField64(ismBits[0], MODS_ISM_CPM_RESET_MASK);     //$
                info.cpm1.launchSkew  = (UINT32)regs.GetField64(ismBits[0], MODS_ISM_CPM_LAUNCH_SKEW);    //$
                info.cpm1.captureSkew = (UINT32)regs.GetField64(ismBits[0], MODS_ISM_CPM_CAPTURE_SKEW);   //$
                info.cpm1.tuneDelay   = (UINT32)regs.GetField64(ismBits[0], MODS_ISM_CPM_TUNE_DELAY);     //$
                info.cpm1.spareIn     = (UINT32)regs.GetField64(ismBits[0], MODS_ISM_CPM_SPARE_IN);       //$
                info.cpm1.resetPath   = (UINT32)regs.GetField64(ismBits[0], MODS_ISM_CPM_RESET_PATH);     //$
                info.cpm1.maskId      = (UINT32)regs.GetField64(ismBits[0], MODS_ISM_CPM_MASK_ID);        //$
                info.cpm1.maskEn      = (UINT32)regs.GetField64(ismBits[0], MODS_ISM_CPM_MASK_EN);        //$
                info.cpm1.maskEdge    = (UINT32)regs.GetField64(ismBits[0], MODS_ISM_CPM_MASK_EDGE);      //$
                info.cpm1.treeOut     = (UINT32)regs.GetField64(ismBits[0], MODS_ISM_CPM_OR_TREE_OUT);    //$
                info.cpm1.status0     =         regs.GetField64(ismBits[0], MODS_ISM_CPM_PATH_TIMING_STATUS0); //$
                info.cpm1.status1     =         regs.GetField64(ismBits[1], MODS_ISM_CPM_PATH_TIMING_STATUS1); //$
                info.cpm1.status2     = (UINT32)regs.GetField64(ismBits[2], MODS_ISM_CPM_PATH_TIMING_STATUS2); //$
                info.cpm1.miscOut     = (UINT32)regs.GetField64(ismBits[2], MODS_ISM_CPM_MISC_OUT);
                break;

            case LW_ISM_cpm_v1:
                info.hdr.version = 1;
                info.cpm1.enable      = (UINT32)regs.GetField64(ismBits[0], MODS_ISM_CPM_V1_EN);
                info.cpm1.iddq        = (UINT32)regs.GetField64(ismBits[0], MODS_ISM_CPM_V1_IDDQ);
                info.cpm1.resetFunc   = (UINT32)regs.GetField64(ismBits[0], MODS_ISM_CPM_V1_RESET_FUNC);   //$
                info.cpm1.resetMask   = (UINT32)regs.GetField64(ismBits[0], MODS_ISM_CPM_V1_RESET_MASK);   //$
                info.cpm1.launchSkew  = (UINT32)regs.GetField64(ismBits[0], MODS_ISM_CPM_V1_LAUNCH_SKEW);  //$
                info.cpm1.captureSkew = (UINT32)regs.GetField64(ismBits[0], MODS_ISM_CPM_V1_CAPTURE_SKEW); //$
                info.cpm1.spareIn     = (UINT32)regs.GetField64(ismBits[0], MODS_ISM_CPM_V1_SPARE_IN);     //$
                info.cpm1.resetPath   = (UINT32)regs.GetField64(ismBits[0], MODS_ISM_CPM_V1_RESET_PATH);   //$
                info.cpm1.maskId      = (UINT32)regs.GetField64(ismBits[0], MODS_ISM_CPM_V1_MASK_ID);      //$
                info.cpm1.maskEn      = (UINT32)regs.GetField64(ismBits[0], MODS_ISM_CPM_V1_MASK_EN);      //$
                info.cpm1.maskEdge    = (UINT32)regs.GetField64(ismBits[0], MODS_ISM_CPM_V1_MASK_EDGE);    //$
                info.cpm1.synthPathConfig =
                                   (UINT32)regs.GetField64(ismBits[0], MODS_ISM_CPM_V1_SYNTH_PATH_CONFIG);       //$
                info.cpm1.calibrationModeSel =
                                   (UINT32)regs.GetField64(ismBits[0], MODS_ISM_CPM_V1_CALIBRATION_MODE_SEL);    //$
                info.cpm1.pgDis       = (UINT32)regs.GetField64(ismBits[0], MODS_ISM_CPM_V1_PG_DIS);             //$
                info.cpm1.treeOut     = (UINT32)regs.GetField64(ismBits[0], MODS_ISM_CPM_V1_OR_TREE_OUT);             //$
                info.cpm1.status0     =         regs.GetField64(ismBits[0], MODS_ISM_CPM_V1_PATH_TIMING_STATUS0);     //$
                info.cpm1.status1     =         regs.GetField64(ismBits[1], MODS_ISM_CPM_V1_PATH_TIMING_STATUS1);     //$
                info.cpm1.status2     = (UINT32)regs.GetField64(ismBits[2], MODS_ISM_CPM_V1_PATH_TIMING_STATUS2);     //$
                info.cpm1.miscOut     = (UINT32)regs.GetField64(ismBits[2], MODS_ISM_CPM_V1_MISC_OUT);                //$
                info.cpm1.garyCntOut  = (UINT32)(regs.GetField64(ismBits[3], MODS_ISM_CPM_V1_GARY_CNT_OUT_HIGH) <<    //$
                                                 regs.LookupMaskSize(MODS_ISM_CPM_V1_GARY_CNT_OUT_LOW) |              //$
                                                 regs.GetField64(ismBits[2], MODS_ISM_CPM_V1_GARY_CNT_OUT_LOW));      //$
                break;
                
            case LW_ISM_cpm_v2:
                info.hdr.version = 2;
                info.cpm2.grayCnt               = (UINT32)regs.GetField64(ismBits[0], MODS_ISM_CPM_V2_GRAY_COUNT);                  //$
                info.cpm2.macroFailureCnt       = (UINT32)regs.GetField64(ismBits[0], MODS_ISM_CPM_V2_MACRO_FAILURE_COUNT);         //$
                info.cpm2.miscOut               = (UINT32)regs.GetField64(ismBits[0], MODS_ISM_CPM_V2_MISC_OUT);                    //$
                info.cpm2.pathTimingStatus      = (UINT32)regs.GetField64(ismBits[0], MODS_ISM_CPM_V2_PATH_TIMING_STATUS);          //$
                info.cpm2.hardMacroCtrlOverride = (UINT32)regs.GetField64(ismBits[0], MODS_ISM_CPM_V2_HARD_MACRO_CTRL_OVERRIDE);    //$
                info.cpm2.hardMacroCtrl         = (UINT32)(regs.GetField64(ismBits[1],MODS_ISM_CPM_V2_HARD_MACRO_CTRL_HIGH) <<      //$
                                                    regs.LookupMaskSize(MODS_ISM_CPM_V2_HARD_MACRO_CTRL_LOW) |                      //$
                                                    regs.GetField64(ismBits[0], MODS_ISM_CPM_V2_HARD_MACRO_CTRL_LOW));              //$
                info.cpm2.instanceIdOverride    = (UINT32)regs.GetField64(ismBits[1], MODS_ISM_CPM_V2_INSTANCE_ID_OVERRIDE);        //$
                info.cpm2.instanceId            = (UINT32)regs.GetField64(ismBits[1], MODS_ISM_CPM_V2_INSTANCE_ID);                 //$
                info.cpm2.samplingPeriod        = (UINT32)regs.GetField64(ismBits[1], MODS_ISM_CPM_V2_SAMPLING_PERIOD);             //$
                info.cpm2.skewCharEn            = (UINT32)regs.GetField64(ismBits[1], MODS_ISM_CPM_V2_SKEW_CHAR_EN);                //$
                info.cpm2.spareIn               = (UINT32)regs.GetField64(ismBits[1], MODS_ISM_CPM_V2_SPARE_IN);                    //$
                break;
                
            // Note: versions 2&3 have the same fields, just in different bit locations
            case LW_ISM_cpm_v3:
                info.hdr.version = 3;
                info.cpm2.hardMacroCtrl         = (UINT32) regs.GetField64(ismBits[0], MODS_ISM_CPM_V3_HARD_MACRO_CTRL);             //$
                info.cpm2.hardMacroCtrlOverride = (UINT32) regs.GetField64(ismBits[0], MODS_ISM_CPM_V3_HARD_MACRO_CTRL_OVERRIDE);    //$
                info.cpm2.instanceId            = (UINT32) regs.GetField64(ismBits[0], MODS_ISM_CPM_V3_INSTANCE_ID);                 //$
                info.cpm2.instanceIdOverride    = (UINT32) regs.GetField64(ismBits[0], MODS_ISM_CPM_V3_INSTANCE_ID_OVERRIDE);        //$
                info.cpm2.samplingPeriod        = (UINT32) regs.GetField64(ismBits[0], MODS_ISM_CPM_V3_SAMPLING_PERIOD);             //$
                info.cpm2.skewCharEn            = (UINT32) regs.GetField64(ismBits[0], MODS_ISM_CPM_V3_SKEW_CHAR_EN);                //$
                info.cpm2.spareIn               = (UINT32) regs.GetField64(ismBits[0], MODS_ISM_CPM_V3_SPARE_IN);                    //$
                info.cpm2.macroFailureCnt       = (UINT32) regs.GetField64(ismBits[0], MODS_ISM_CPM_V3_MACRO_FAILURE_COUNT);         //$
                info.cpm2.pathTimingStatus      = (UINT32)(regs.GetField64(ismBits[1], MODS_ISM_CPM_V3_PATH_TIMING_STATUS_HIGH) <<   //$
                                                    regs.LookupMaskSize(MODS_ISM_CPM_V3_PATH_TIMING_STATUS_LOW) |                    //$
                                                    regs.GetField64(ismBits[0], MODS_ISM_CPM_V3_PATH_TIMING_STATUS_LOW));            //$
                info.cpm2.grayCnt               = (UINT32) regs.GetField64(ismBits[1], MODS_ISM_CPM_V3_GRAY_COUNT);                  //$
                info.cpm2.miscOut               = (UINT32) regs.GetField64(ismBits[1], MODS_ISM_CPM_V3_MISC_OUT);                    //$
                break;

            // Note: version 4 has the same fields as version 3 + PG_EN & PG_EN_OVERRIDE
            case LW_ISM_cpm_v4:
                info.hdr.version = 4;
                info.cpm4.hardMacroCtrl         = (UINT32) regs.GetField64(ismBits[0], MODS_ISM_CPM_V4_HARD_MACRO_CTRL);             //$
                info.cpm4.hardMacroCtrlOverride = (UINT32) regs.GetField64(ismBits[0], MODS_ISM_CPM_V4_HARD_MACRO_CTRL_OVERRIDE);    //$
                info.cpm4.instanceId            = (UINT32) regs.GetField64(ismBits[0], MODS_ISM_CPM_V4_INSTANCE_ID);                 //$
                info.cpm4.instanceIdOverride    = (UINT32) regs.GetField64(ismBits[0], MODS_ISM_CPM_V4_INSTANCE_ID_OVERRIDE);        //$
                info.cpm4.pgEn                  = (UINT32) regs.GetField64(ismBits[0], MODS_ISM_CPM_V4_PG_EN);
                info.cpm4.pgEnOverride          = (UINT32) regs.GetField64(ismBits[0], MODS_ISM_CPM_V4_PG_EN_OVERRIDE);
                info.cpm4.samplingPeriod        = (UINT32) regs.GetField64(ismBits[0], MODS_ISM_CPM_V4_SAMPLING_PERIOD);             //$
                info.cpm4.skewCharEn            = (UINT32) regs.GetField64(ismBits[0], MODS_ISM_CPM_V4_SKEW_CHAR_EN);                //$
                info.cpm4.spareIn               = (UINT32) regs.GetField64(ismBits[0], MODS_ISM_CPM_V4_SPARE_IN);                    //$
                info.cpm4.macroFailureCnt       = (UINT32) regs.GetField64(ismBits[0], MODS_ISM_CPM_V4_MACRO_FAILURE_COUNT);         //$
                info.cpm4.pathTimingStatus      = (UINT32)(regs.GetField64(ismBits[1], MODS_ISM_CPM_V4_PATH_TIMING_STATUS_HIGH) <<   //$
                                                    regs.LookupMaskSize(MODS_ISM_CPM_V4_PATH_TIMING_STATUS_LOW) |                    //$
                                                    regs.GetField64(ismBits[0], MODS_ISM_CPM_V4_PATH_TIMING_STATUS_LOW));            //$
                info.cpm4.grayCnt               = (UINT32) regs.GetField64(ismBits[1], MODS_ISM_CPM_V4_GRAY_COUNT);                  //$
                info.cpm4.miscOut               = (UINT32) regs.GetField64(ismBits[1], MODS_ISM_CPM_V4_MISC_OUT);                    //$
                break;

            case LW_ISM_cpm_v5:
                info.hdr.version = 5;
                info.cpm4.enable                = (UINT32) regs.GetField64(ismBits[0], MODS_ISM_CPM_V5_EN);                          //$
                info.cpm4.resetFunc             = (UINT32) regs.GetField64(ismBits[0], MODS_ISM_CPM_V5_RESET_FUNC);                  //$
                info.cpm4.spareIn               = (UINT32) regs.GetField64(ismBits[0], MODS_ISM_CPM_V5_SPARE_IN);                    //$
                info.cpm4.resetPath             = (UINT32) regs.GetField64(ismBits[0], MODS_ISM_CPM_V5_RESET_PATH);                  //$
                info.cpm4.skewCharEn            = (UINT32) regs.GetField64(ismBits[0], MODS_ISM_CPM_V5_SKEW_CHAR_EN);                //$
                info.cpm4.configData            = (UINT32) regs.GetField64(ismBits[0], MODS_ISM_CPM_V5_CONFIG_DATA);                 //$
                info.cpm4.samplingPeriod        = (UINT32) regs.GetField64(ismBits[0], MODS_ISM_CPM_V5_SAMPLING_PERIOD);             //$
                info.cpm4.dataTypeId            = (UINT32) regs.GetField64(ismBits[0], MODS_ISM_CPM_V5_DATATYPE_ID);                 //$
                info.cpm4.instanceId            = (UINT32) regs.GetField64(ismBits[0], MODS_ISM_CPM_V5_INSTANCE_ID);                 //$
                info.cpm4.dataValid             = (UINT32) regs.GetField64(ismBits[0], MODS_ISM_CPM_V5_DATA_VALID);                  //$
                info.cpm4.pathTimingStatus      = (UINT32) regs.GetField64(ismBits[0], MODS_ISM_CPM_V5_PATH_TIMING_STATUS);          //$
                info.cpm4.miscOut               = (UINT32)(regs.GetField64(ismBits[1], MODS_ISM_CPM_V5_MISC_OUT_HIGH) <<             //$
                                                    regs.LookupMaskSize(MODS_ISM_CPM_V5_MISC_OUT_LOW) |                              //$
                                                    regs.GetField64(ismBits[0], MODS_ISM_CPM_V5_MISC_OUT_LOW));                      //$
                info.cpm4.grayCnt               = (UINT32) regs.GetField64(ismBits[1], MODS_ISM_CPM_V5_GRAY_COUNT);                  //$
                info.cpm4.macroFailureCnt       = (UINT32) regs.GetField64(ismBits[1], MODS_ISM_CPM_V5_MACRO_FAILURE_COUNT);         //$
                break;

            case LW_ISM_cpm_v6:
                info.hdr.version = 6;
                info.cpm6.hardMacroCtrl         = (UINT32) regs.GetField64(ismBits[0], MODS_ISM_CPM_V6_HARD_MACRO_CTRL);             //$
                info.cpm6.hardMacroCtrlOverride = (UINT32) regs.GetField64(ismBits[0], MODS_ISM_CPM_V6_HARD_MACRO_CTRL_OVERRIDE);    //$
                info.cpm6.instanceId            = (UINT32) regs.GetField64(ismBits[0], MODS_ISM_CPM_V6_INSTANCE_ID);                 //$
                info.cpm6.instanceIdOverride    = (UINT32) regs.GetField64(ismBits[0], MODS_ISM_CPM_V6_INSTANCE_ID_OVERRIDE);        //$
                info.cpm6.samplingPeriod        = (UINT32) regs.GetField64(ismBits[0], MODS_ISM_CPM_V6_SAMPLING_PERIOD);             //$
                info.cpm6.skewCharEn            = (UINT32) regs.GetField64(ismBits[0], MODS_ISM_CPM_V6_SKEW_CHAR_EN);                //$
                info.cpm6.spareIn               = (UINT32) regs.GetField64(ismBits[0], MODS_ISM_CPM_V6_SPARE_IN);                    //$
                info.cpm6.macroFailureCnt       = (UINT32) regs.GetField64(ismBits[0], MODS_ISM_CPM_V6_MACRO_FAILURE_COUNT);         //$
                info.cpm6.pathTimingStatus      = (UINT32)(regs.GetField64(ismBits[1], MODS_ISM_CPM_V6_PATH_TIMING_STATUS_HIGH) <<   //$
                                                    regs.LookupMaskSize(MODS_ISM_CPM_V6_PATH_TIMING_STATUS_LOW) |                    //$
                                                    regs.GetField64(ismBits[0], MODS_ISM_CPM_V6_PATH_TIMING_STATUS_LOW));            //$
                info.cpm6.grayCnt               = (UINT32) regs.GetField64(ismBits[1], MODS_ISM_CPM_V6_GRAY_COUNT);                  //$
                info.cpm6.miscOut               = (UINT32) regs.GetField64(ismBits[1], MODS_ISM_CPM_V6_MISC_OUT);                    //$
                break;
        }
        pCpmResults->push_back(info);
    }
    return rc;
}

//------------------------------------------------------------------------------
/*virtual*/
RC GpuIsm::ConfigureCPMBits
(
    IsmTypes        ismType
    ,vector<UINT64> &cpmBits
    ,const CpmInfo  &param
)
{
    RegHal &regs = GetGpuSub()->Regs();
    cpmBits.resize(CallwlateVectorSize(ismType, 64), 0);
    switch (ismType)
    {
        case LW_ISM_cpm:
            cpmBits[0] = (
                regs.SetField64(MODS_ISM_CPM_EN, param.cpm1.enable) |
                regs.SetField64(MODS_ISM_CPM_IDDQ, param.cpm1.iddq) |
                regs.SetField64(MODS_ISM_CPM_RESET_FUNC, param.cpm1.resetFunc) |
                regs.SetField64(MODS_ISM_CPM_RESET_MASK, param.cpm1.resetMask) |
                regs.SetField64(MODS_ISM_CPM_LAUNCH_SKEW, param.cpm1.launchSkew) |
                regs.SetField64(MODS_ISM_CPM_CAPTURE_SKEW, param.cpm1.captureSkew) |
                regs.SetField64(MODS_ISM_CPM_TUNE_DELAY, param.cpm1.tuneDelay) |
                regs.SetField64(MODS_ISM_CPM_SPARE_IN, param.cpm1.spareIn) |
                regs.SetField64(MODS_ISM_CPM_RESET_PATH, param.cpm1.resetPath) |
                regs.SetField64(MODS_ISM_CPM_MASK_ID, param.cpm1.maskId) |
                regs.SetField64(MODS_ISM_CPM_MASK_EN, param.cpm1.maskEn) |
                regs.SetField64(MODS_ISM_CPM_MASK_EDGE, param.cpm1.maskEdge) |
                regs.SetField64(MODS_ISM_CPM_OR_TREE_OUT, param.cpm1.treeOut) |
                regs.SetField64(MODS_ISM_CPM_PATH_TIMING_STATUS0, param.cpm1.status0)
                );
            cpmBits[1] = regs.SetField64(MODS_ISM_CPM_PATH_TIMING_STATUS1, param.cpm1.status1);
            cpmBits[2] = (
                regs.SetField64(MODS_ISM_CPM_PATH_TIMING_STATUS2, param.cpm1.status2) |
                regs.SetField64(MODS_ISM_CPM_MISC_OUT, param.cpm1.miscOut)
                );
            break;
        case LW_ISM_cpm_v1:
            cpmBits[0] = (
                regs.SetField64(MODS_ISM_CPM_V1_EN, param.cpm1.enable) |
                regs.SetField64(MODS_ISM_CPM_V1_IDDQ, param.cpm1.iddq) |
                regs.SetField64(MODS_ISM_CPM_V1_RESET_FUNC, param.cpm1.resetFunc) |
                regs.SetField64(MODS_ISM_CPM_V1_RESET_MASK, param.cpm1.resetMask) |
                regs.SetField64(MODS_ISM_CPM_V1_LAUNCH_SKEW, param.cpm1.launchSkew) |
                regs.SetField64(MODS_ISM_CPM_V1_CAPTURE_SKEW, param.cpm1.captureSkew) |
                regs.SetField64(MODS_ISM_CPM_V1_SPARE_IN, param.cpm1.spareIn) |
                regs.SetField64(MODS_ISM_CPM_V1_RESET_PATH, param.cpm1.resetPath) |
                regs.SetField64(MODS_ISM_CPM_V1_MASK_ID, param.cpm1.maskId) |
                regs.SetField64(MODS_ISM_CPM_V1_MASK_EN, param.cpm1.maskEn) |
                regs.SetField64(MODS_ISM_CPM_V1_MASK_EDGE, param.cpm1.maskEdge) |
                regs.SetField64(MODS_ISM_CPM_V1_PATH_TIMING_STATUS0, param.cpm1.status0) |
                regs.SetField64(MODS_ISM_CPM_V1_SYNTH_PATH_CONFIG, param.cpm1.synthPathConfig) |
                regs.SetField64(MODS_ISM_CPM_V1_CALIBRATION_MODE_SEL, param.cpm1.calibrationModeSel) | //$
                regs.SetField64(MODS_ISM_CPM_V1_PG_DIS, param.cpm1.pgDis) |
                regs.SetField64(MODS_ISM_CPM_V1_OR_TREE_OUT, param.cpm1.treeOut)
                );
            cpmBits[1] = regs.SetField64(MODS_ISM_CPM_V1_PATH_TIMING_STATUS1, param.cpm1.status1);
            cpmBits[2] = (
                regs.SetField64(MODS_ISM_CPM_V1_PATH_TIMING_STATUS2, param.cpm1.status2) |
                regs.SetField64(MODS_ISM_CPM_V1_MISC_OUT, param.cpm1.miscOut)
                );
            break;
        case LW_ISM_cpm_v2:
        {
            UINT64 hardMacroCtrlLow = param.cpm2.hardMacroCtrl & 
                        regs.LookupFieldValueMask(MODS_ISM_CPM_V2_HARD_MACRO_CTRL_LOW);
            UINT64 hardMacroCtrlHigh = param.cpm2.hardMacroCtrl >> 
                        regs.LookupMaskSize(MODS_ISM_CPM_V2_HARD_MACRO_CTRL_LOW);
            cpmBits[0] = (
                regs.SetField64(MODS_ISM_CPM_V2_GRAY_COUNT, param.cpm2.grayCnt) |
                regs.SetField64(MODS_ISM_CPM_V2_MACRO_FAILURE_COUNT, param.cpm2.macroFailureCnt) |
                regs.SetField64(MODS_ISM_CPM_V2_MISC_OUT, param.cpm2.miscOut) |
                regs.SetField64(MODS_ISM_CPM_V2_PATH_TIMING_STATUS, param.cpm2.pathTimingStatus) |
                regs.SetField64(MODS_ISM_CPM_V2_HARD_MACRO_CTRL_OVERRIDE, param.cpm2.hardMacroCtrlOverride) | //$
                regs.SetField64(MODS_ISM_CPM_V2_HARD_MACRO_CTRL_LOW, hardMacroCtrlLow));
            cpmBits[1] = (
                regs.SetField64(MODS_ISM_CPM_V2_HARD_MACRO_CTRL_HIGH, hardMacroCtrlHigh) |
                regs.SetField64(MODS_ISM_CPM_V2_INSTANCE_ID_OVERRIDE, param.cpm2.instanceIdOverride) | //$
                regs.SetField64(MODS_ISM_CPM_V2_INSTANCE_ID, param.cpm2.instanceId) |
                regs.SetField64(MODS_ISM_CPM_V2_SAMPLING_PERIOD, param.cpm2.samplingPeriod) |
                regs.SetField64(MODS_ISM_CPM_V2_SKEW_CHAR_EN, param.cpm2.skewCharEn) |
                regs.SetField64(MODS_ISM_CPM_V2_SPARE_IN, param.cpm2.spareIn));
            break;
        }
        // Version 3 has the same field names as version 2, however bit locations are different
        case LW_ISM_cpm_v3: 
        {
            UINT64 pathTimingLow = param.cpm2.pathTimingStatus & 
                        regs.LookupFieldValueMask(MODS_ISM_CPM_V3_PATH_TIMING_STATUS_LOW);
            UINT64 pathTimingHigh = param.cpm2.pathTimingStatus >> 
                        regs.LookupMaskSize(MODS_ISM_CPM_V3_PATH_TIMING_STATUS_LOW);

            cpmBits[0] = (
                regs.SetField64(MODS_ISM_CPM_V3_HARD_MACRO_CTRL, param.cpm2.hardMacroCtrl) |
                regs.SetField64(MODS_ISM_CPM_V3_HARD_MACRO_CTRL_OVERRIDE, param.cpm2.hardMacroCtrlOverride) | //$
                regs.SetField64(MODS_ISM_CPM_V3_INSTANCE_ID, param.cpm2.instanceId) |
                regs.SetField64(MODS_ISM_CPM_V3_INSTANCE_ID_OVERRIDE, param.cpm2.instanceIdOverride) | //$
                regs.SetField64(MODS_ISM_CPM_V3_SAMPLING_PERIOD, param.cpm2.samplingPeriod) |
                regs.SetField64(MODS_ISM_CPM_V3_SKEW_CHAR_EN, param.cpm2.skewCharEn) |
                regs.SetField64(MODS_ISM_CPM_V3_SPARE_IN, param.cpm2.spareIn) |
                regs.SetField64(MODS_ISM_CPM_V3_MACRO_FAILURE_COUNT, param.cpm2.macroFailureCnt) |
                regs.SetField64(MODS_ISM_CPM_V3_PATH_TIMING_STATUS_LOW, pathTimingLow));

            cpmBits[1] = (
                regs.SetField64(MODS_ISM_CPM_V3_PATH_TIMING_STATUS_HIGH, pathTimingHigh) |
                regs.SetField64(MODS_ISM_CPM_V3_GRAY_COUNT, param.cpm2.grayCnt) |
                regs.SetField64(MODS_ISM_CPM_V3_MISC_OUT, param.cpm2.miscOut));
            break;
        }
        case LW_ISM_cpm_v4: 
        {
            UINT64 pathTimingLow = param.cpm4.pathTimingStatus & 
                        regs.LookupFieldValueMask(MODS_ISM_CPM_V4_PATH_TIMING_STATUS_LOW);
            UINT64 pathTimingHigh = param.cpm4.pathTimingStatus >> 
                        regs.LookupMaskSize(MODS_ISM_CPM_V4_PATH_TIMING_STATUS_LOW);
            cpmBits[0] = (
                regs.SetField64(MODS_ISM_CPM_V4_HARD_MACRO_CTRL, param.cpm4.hardMacroCtrl) |
                regs.SetField64(MODS_ISM_CPM_V4_HARD_MACRO_CTRL_OVERRIDE, param.cpm4.hardMacroCtrlOverride) | //$
                regs.SetField64(MODS_ISM_CPM_V4_INSTANCE_ID, param.cpm4.instanceId) |
                regs.SetField64(MODS_ISM_CPM_V4_INSTANCE_ID_OVERRIDE, param.cpm4.instanceIdOverride) | //$
                regs.SetField64(MODS_ISM_CPM_V4_PG_EN, param.cpm4.pgEn) | //$
                regs.SetField64(MODS_ISM_CPM_V4_PG_EN_OVERRIDE, param.cpm4.pgEnOverride) | //$
                regs.SetField64(MODS_ISM_CPM_V4_SAMPLING_PERIOD, param.cpm4.samplingPeriod) |
                regs.SetField64(MODS_ISM_CPM_V4_SKEW_CHAR_EN, param.cpm4.skewCharEn) |
                regs.SetField64(MODS_ISM_CPM_V4_SPARE_IN, param.cpm4.spareIn) |
                regs.SetField64(MODS_ISM_CPM_V4_MACRO_FAILURE_COUNT, param.cpm4.macroFailureCnt) |
                regs.SetField64(MODS_ISM_CPM_V4_PATH_TIMING_STATUS_LOW, pathTimingLow));
            cpmBits[1] = (
                regs.SetField64(MODS_ISM_CPM_V4_PATH_TIMING_STATUS_HIGH, pathTimingHigh) |
                regs.SetField64(MODS_ISM_CPM_V4_GRAY_COUNT, param.cpm4.grayCnt) |
                regs.SetField64(MODS_ISM_CPM_V4_MISC_OUT, param.cpm4.miscOut));
            break;
        }
        case LW_ISM_cpm_v5: 
        {
            UINT64 miscOutLow = param.cpm4.miscOut & 
                        regs.LookupFieldValueMask(MODS_ISM_CPM_V5_MISC_OUT_LOW);
            UINT64 miscOutHigh = param.cpm4.miscOut >> 
                        regs.LookupMaskSize(MODS_ISM_CPM_V5_MISC_OUT_LOW);
            cpmBits[0] = (
                regs.SetField64(MODS_ISM_CPM_V5_EN, param.cpm4.enable) |
                regs.SetField64(MODS_ISM_CPM_V5_RESET_FUNC, param.cpm4.resetFunc) |
                regs.SetField64(MODS_ISM_CPM_V5_SPARE_IN, param.cpm4.spareIn) |
                regs.SetField64(MODS_ISM_CPM_V5_RESET_PATH, param.cpm4.resetPath) |
                regs.SetField64(MODS_ISM_CPM_V5_SKEW_CHAR_EN, param.cpm4.skewCharEn) |
                regs.SetField64(MODS_ISM_CPM_V5_CONFIG_DATA, param.cpm4.configData) |
                regs.SetField64(MODS_ISM_CPM_V5_SAMPLING_PERIOD, param.cpm4.samplingPeriod) |
                regs.SetField64(MODS_ISM_CPM_V5_DATATYPE_ID, param.cpm4.dataTypeId) |
                regs.SetField64(MODS_ISM_CPM_V5_INSTANCE_ID, param.cpm4.instanceId) |
                regs.SetField64(MODS_ISM_CPM_V5_DATA_VALID, param.cpm4.dataValid) |
                regs.SetField64(MODS_ISM_CPM_V5_PATH_TIMING_STATUS, param.cpm4.pathTimingStatus) |
                regs.SetField64(MODS_ISM_CPM_V5_MISC_OUT_LOW, miscOutLow));
            cpmBits[1] = (
                regs.SetField64(MODS_ISM_CPM_V5_MISC_OUT_HIGH, miscOutHigh) |
                regs.SetField64(MODS_ISM_CPM_V5_GRAY_COUNT, param.cpm4.grayCnt) |
                regs.SetField64(MODS_ISM_CPM_V5_MACRO_FAILURE_COUNT, param.cpm4.macroFailureCnt));
            break;
        }
        case LW_ISM_cpm_v6: 
        {
            UINT64 pathTimingLow = param.cpm6.pathTimingStatus & 
                        regs.LookupFieldValueMask(MODS_ISM_CPM_V6_PATH_TIMING_STATUS_LOW);
            UINT64 pathTimingHigh = param.cpm6.pathTimingStatus >> 
                        regs.LookupMaskSize(MODS_ISM_CPM_V6_PATH_TIMING_STATUS_LOW);
            cpmBits[0] = (
                regs.SetField64(MODS_ISM_CPM_V6_HARD_MACRO_CTRL, param.cpm6.hardMacroCtrl)                  | //$
                regs.SetField64(MODS_ISM_CPM_V6_HARD_MACRO_CTRL_OVERRIDE, param.cpm6.hardMacroCtrlOverride) | //$
                regs.SetField64(MODS_ISM_CPM_V6_INSTANCE_ID, param.cpm6.instanceId)                         | //$
                regs.SetField64(MODS_ISM_CPM_V6_INSTANCE_ID_OVERRIDE, param.cpm6.instanceIdOverride)        | //$
                regs.SetField64(MODS_ISM_CPM_V6_SAMPLING_PERIOD, param.cpm6.samplingPeriod)                 | //$
                regs.SetField64(MODS_ISM_CPM_V6_SKEW_CHAR_EN, param.cpm6.skewCharEn)                        | //$
                regs.SetField64(MODS_ISM_CPM_V6_SPARE_IN, param.cpm6.spareIn)                               | //$
                regs.SetField64(MODS_ISM_CPM_V6_MACRO_FAILURE_COUNT, param.cpm6.macroFailureCnt)            | //$
                regs.SetField64(MODS_ISM_CPM_V6_PATH_TIMING_STATUS_LOW, pathTimingLow));
            cpmBits[1] = (
                regs.SetField64(MODS_ISM_CPM_V6_PATH_TIMING_STATUS_HIGH, pathTimingHigh) |
                regs.SetField64(MODS_ISM_CPM_V6_GRAY_COUNT, param.cpm6.grayCnt)          |
                regs.SetField64(MODS_ISM_CPM_V6_MISC_OUT, param.cpm6.miscOut));
            break;
        }

        default:
            Printf(Tee::PriError, "Unknown LW_ISM_cpm_* type:%d", ismType);
            return RC::BAD_PARAMETER;
    }
    return RC::OK;
}

//------------------------------------------------------------------------------------------------
// Return the 1st index in the params vector that matches.
// chiplet, instrId, offset must be valid values. ie can't be -1
// params[i].chiplet, params[i].instrId, & params[i].offset can be any value. If the value is -1
// it is treated as a wild card '*'.
// The user should put the more specific match patterns in the vector first, followed by more
// generic match patterns. For example setting params[i].instrId = -1 && params[i].chiplet = -1 &&
// params[i].offset = -1 should be the last element in the vector.
INT32 GpuIsm::GetCpmParamIdx
(
    vector<CpmInfo> &params,
    UINT32 chiplet,
    UINT32 instrId,
    INT32 offset
)
{
    // Is there an exact ISM match?
    for (UINT32 i = 0; i < params.size(); i++)
    {
        if (ParamsMatch(params[i].hdr, chiplet, instrId, offset))
        {
            return i;
        }
    }
    return -1;
}

INT32 GpuIsm::GetISinkParamIdx
(
    vector<ISinkInfo> &params,
    UINT32 chiplet,
    UINT32 instrId,
    INT32 offset
)
{
    // Is there an exact ISM match?
    for (UINT32 i = 0; i < params.size(); i++)
    {
        if (ParamsMatch(params[i].hdr, chiplet, instrId, offset))
        {
            return i;
        }
    }
    return -1;
}

bool GpuIsm::ParamsMatch
(
    IsmHeader &header,
    UINT32 chiplet,
    UINT32 instrId,
    INT32 offset
)
{
    // Is there an exact ISM match?
    if (((static_cast<UINT32>(header.chainId) == instrId) || header.chainId == -1) &&
        ((static_cast<UINT32>(header.chiplet) == chiplet) || header.chiplet == -1) &&
        ((header.offset == offset) || header.offset == -1))
    {
        return true;
    }
    return false;
}


//------------------------------------------------------------------------------
// Write one or more CPM cirlwits on this GPU with the supplied parameters.
/*virtual*/
RC GpuIsm::WriteCPMs
(
    vector<CpmInfo> &params        //!< structure with setup information
)
{
    RC rc;
    vector<IsmChainFilter> chainFilter;

    // Get the list of Jtag chains with CPM ISMs
    // When chainId = -1, chiplet = -1 we read all JTag chains with CPM macros.
    // Note: each version is mutually exclusive for a given GPU.
    CHECK_RC(GetISMChains(&chainFilter, LW_ISM_cpm,    -1, -1, ~0));
    CHECK_RC(GetISMChains(&chainFilter, LW_ISM_cpm_v1, -1, -1, ~0));
    CHECK_RC(GetISMChains(&chainFilter, LW_ISM_cpm_v2, -1, -1, ~0));
    CHECK_RC(GetISMChains(&chainFilter, LW_ISM_cpm_v3, -1, -1, ~0));
    CHECK_RC(GetISMChains(&chainFilter, LW_ISM_cpm_v4, -1, -1, ~0));
    CHECK_RC(GetISMChains(&chainFilter, LW_ISM_cpm_v5, -1, -1, ~0));
    CHECK_RC(GetISMChains(&chainFilter, LW_ISM_cpm_v6, -1, -1, ~0));

    // If not found the chainId is probably wrong
    if (chainFilter.empty())
    {
        Printf(Tee::PriError, "GpuIsm::WriteCPMs: No CPM ISMs found.\n");
        return RC::UNSUPPORTED_FUNCTION;
    }

    //for each chain, determine how to program the paramters.
    for (UINT32 idx = 0; idx < chainFilter.size(); idx++)
    {
        INT32 paramIdx = -1;
        bool bWriteTheChain = false;
        const IsmChain * pChain = chainFilter[idx].pChain;

        vector <UINT32> jtagArray;
        jtagArray.resize((pChain->Hdr.Size + 31) / 32, 0);

        // Power up and configure the CPM ISMs
        vector<UINT64> cpmBits = { 0 };
        for (UINT32 ismIdx = 0; ismIdx < pChain->ISMs.size(); ismIdx++)
        {
            paramIdx = GetCpmParamIdx(params,
                                      pChain->Hdr.Chiplet,
                                      pChain->Hdr.InstrId,
                                      pChain->ISMs[ismIdx].Offset);
            if (paramIdx >= 0)
            {
                CHECK_RC(ConfigureCPMBits(pChain->ISMs[ismIdx].Type, cpmBits, params[paramIdx]));
                if (chainFilter[idx].ismMask[ismIdx])
                {
                    bWriteTheChain = true;
                    // Copy the bits to the chain.
                    Utility::CopyBits(cpmBits,
                             jtagArray,
                             0,  //srcBitOffset
                             pChain->ISMs[ismIdx].Offset, // dstBitOffset
                             GetISMSize(pChain->ISMs[ismIdx].Type)); //numBits
                }
            }
        }

        // Send the bits down to JTag
        if (bWriteTheChain)
        {
            CHECK_RC(WriteHost2Jtag(pChain->Hdr.Chiplet,
                                    pChain->Hdr.InstrId,
                                    pChain->Hdr.Size,
                                    jtagArray));
        }
    }
    return rc;
}

//------------------------------------------------------------------------------
// Read the CPM ISM cirlwits on the GPU.
// pCpmResults - pointer to a vector to hold the detailed CPM result info.
// cpmParams   - a vector of CPM Info parameters that can be configured to read all of the CPMs,
//               read CPMs on a single chain, read a single CPM on a chain, or any combination of 
//               the three.
//               The pattern matching algorithm will use the first CpmInfo record that matches, so
//               put the more specific parameters in the vector first.
/*virtual*/
RC GpuIsm::ReadCPMs
(
    vector<CpmInfo> *pCpmResults
    , vector<CpmInfo> &cpmParams
)
{
    RC rc;
    vector<IsmChainFilter> chainFilter;

    // Get the list of Jtag chains with CPMs
    CHECK_RC(GetISMChains(&chainFilter, LW_ISM_cpm,    -1, -1, ~0));
    CHECK_RC(GetISMChains(&chainFilter, LW_ISM_cpm_v1, -1, -1, ~0));
    CHECK_RC(GetISMChains(&chainFilter, LW_ISM_cpm_v2, -1, -1, ~0));
    CHECK_RC(GetISMChains(&chainFilter, LW_ISM_cpm_v3, -1, -1, ~0));
    CHECK_RC(GetISMChains(&chainFilter, LW_ISM_cpm_v4, -1, -1, ~0));
    CHECK_RC(GetISMChains(&chainFilter, LW_ISM_cpm_v5, -1, -1, ~0));
    CHECK_RC(GetISMChains(&chainFilter, LW_ISM_cpm_v6, -1, -1, ~0));

    // If not found there aren't any CPMs
    if (chainFilter.empty())
    {
        Printf(Tee::PriError, "GpuIsm::ReadCPMs: No CPMs found\n");
        return RC::UNSUPPORTED_FUNCTION;
    }

    // If we get a match for any ISM on a chain, we read the entire chain.
    for (UINT32 idx = 0; idx < chainFilter.size(); idx++)
    {
        const IsmChain * pChain = chainFilter[idx].pChain;
        bool bMatch = false;
        for (size_t ismIdx = 0; ismIdx < pChain->ISMs.size(); ismIdx++)
        {
            if (GetCpmParamIdx(cpmParams, 
                               pChain->Hdr.Chiplet,
                               pChain->Hdr.InstrId,
                               pChain->ISMs[ismIdx].Offset) >= 0)
            {
                bMatch = true;
                break;
            }
        }
        if (bMatch)
        {
            vector<UINT32> jtagArray;
                jtagArray.resize((pChain->Hdr.Size + 31) / 32, 0);
        
                CHECK_RC(ReadHost2Jtag(pChain->Hdr.Chiplet,
                                       pChain->Hdr.InstrId,
                                       pChain->Hdr.Size,
                                       &jtagArray));
        
                CHECK_RC(GetCPMChainCounts(pChain, jtagArray, pCpmResults, cpmParams));
        }
    }
    return rc;
}

//------------------------------------------------------------------------------
/*virtual*/
RC GpuIsm::ConfigureISinkBits
(
    IsmTypes        ismType
    ,vector<UINT64> &iSinkBits
    ,const ISinkInfo  &param
)
{
    RegHal &regs = GetGpuSub()->Regs();
    iSinkBits.resize(CallwlateVectorSize(ismType, 64), 0);
    switch (ismType)
    {
        case LW_ISM_lwrrent_sink:
            iSinkBits[0] = (
                regs.SetField64(MODS_ISM_LWRRENT_SINK_V1_LWRRENT_OFF, param.isink1.iOff) |
                regs.SetField64(MODS_ISM_LWRRENT_SINK_V1_LWRRENT_SEL, param.isink1.iSel) |
                regs.SetField64(MODS_ISM_LWRRENT_SINK_V1_SEL_DBG, param.isink1.selDbg)
                );
            break;

        default:
            Printf(Tee::PriError, "Unknown LW_ISM_lwrrent_sink_* type:%d", ismType);
            return RC::BAD_PARAMETER;
    }
    return RC::OK;
}

//------------------------------------------------------------------------------
/*virtual*/
RC GpuIsm::GetISinkChainCounts
(
    const IsmChain *pChain          // The chain configuration
    , vector<UINT32> &jtagArray     // Vector of raw bits for this chain
    , vector<ISinkInfo> *pResults   // Vector to hold the macro specific data
    , vector<ISinkInfo> &params     // Vector to hold the filter paramters of the chains to read.
)
{
    RC rc;
    RegHal &regs = GetGpuSub()->Regs();
    vector<UINT64> ismBits = { 0 };
    for (size_t idx = 0; idx < pChain->ISMs.size(); idx++)
    {   // look for specific ISM parameter
        if (GetISinkParamIdx(params,
                           pChain->Hdr.Chiplet,
                           pChain->Hdr.InstrId,
                           pChain->ISMs[idx].Offset) < 0)
        {   // no match... move along.
            continue;
        }

        // Load the bits for a single macro into ismBits[]
        ismBits.resize(CallwlateVectorSize(pChain->ISMs[idx].Type, 64), 0);
        CHECK_RC(Utility::CopyBits(jtagArray,
                                   ismBits,
                                   pChain->ISMs[idx].Offset,
                                   0,
                                   GetISMSize(pChain->ISMs[idx].Type)));
        ISinkInfo info = { 0 };
        info.hdr.chainId = pChain->Hdr.InstrId;
        info.hdr.chiplet = pChain->Hdr.Chiplet;
        info.hdr.offset  = pChain->ISMs[idx].Offset;
        switch (pChain->ISMs[idx].Type)
        {
            default:        // So far all chains with CURRENT SINK ISMs don't have other ISM types
                continue;   // but we don't to fail if they do. Just ignore them.
                
            case LW_ISM_lwrrent_sink:
                // Load up the info
                info.hdr.version = 1;
                info.isink1.iOff   = (UINT32)regs.GetField64(ismBits[0], MODS_ISM_LWRRENT_SINK_V1_LWRRENT_OFF);  //$
                info.isink1.iSel   = (UINT32)regs.GetField64(ismBits[0], MODS_ISM_LWRRENT_SINK_V1_LWRRENT_SEL);  //$
                info.isink1.selDbg = (UINT32)regs.GetField64(ismBits[0], MODS_ISM_LWRRENT_SINK_V1_SEL_DBG);      //$
                break;
        }
        pResults->push_back(info);
    }
    return rc;
}

//------------------------------------------------------------------------------
// Write one or more CPM cirlwits on this GPU with the supplied parameters.
/*virtual*/
RC GpuIsm::WriteISinks
(
    vector<ISinkInfo> &params        //!< structure with setup information
)
{
    RC rc;
    vector<IsmChainFilter> chainFilter;

    // Get the list of Jtag chains with LWRRENT_SINK ISMs
    // When chainId = -1, chiplet = -1 we read all JTag chains with LWRRENT_SINK macros.
    // Note: each version is mutually exclusive for a given GPU.
    CHECK_RC(GetISMChains(&chainFilter, LW_ISM_lwrrent_sink, -1, -1, ~0));

    // If not found the chainId is probably wrong
    if (chainFilter.empty())
    {
        Printf(Tee::PriError, "GpuIsm::WriteISinks: No LWRRENT_SINK ISMs found.\n");
        return RC::UNSUPPORTED_FUNCTION;
    }

    //for each chain, determine how to program the paramters.
    for (UINT32 idx = 0; idx < chainFilter.size(); idx++)
    {
        INT32 paramIdx = -1;
        bool bWriteTheChain = false;
        const IsmChain * pChain = chainFilter[idx].pChain;

        vector <UINT32> jtagArray;
        jtagArray.resize((pChain->Hdr.Size + 31) / 32, 0);

        // Power up and configure the LWRRENT_SINK ISMs
        vector<UINT64> iSinkBits = { 0 };
        for (UINT32 ismIdx = 0; ismIdx < pChain->ISMs.size(); ismIdx++)
        {
            paramIdx = GetISinkParamIdx(params,
                                        pChain->Hdr.Chiplet,
                                        pChain->Hdr.InstrId,
                                        pChain->ISMs[ismIdx].Offset);
            if (paramIdx >= 0)
            {
                CHECK_RC(ConfigureISinkBits(pChain->ISMs[ismIdx].Type,
                                            iSinkBits, params[paramIdx]));
                if (chainFilter[idx].ismMask[ismIdx])
                {
                    bWriteTheChain = true;
                    // Copy the bits to the chain.
                    Utility::CopyBits(iSinkBits,
                                      jtagArray,
                                      0,  //srcBitOffset
                                      pChain->ISMs[ismIdx].Offset, // dstBitOffset
                                      GetISMSize(pChain->ISMs[ismIdx].Type)); //numBits
                }
            }
        }

        // Send the bits down to JTag
        if (bWriteTheChain)
        {
            CHECK_RC(WriteHost2Jtag(pChain->Hdr.Chiplet,
                                    pChain->Hdr.InstrId,
                                    pChain->Hdr.Size,
                                    jtagArray));
        }
    }
    return rc;
}

//-------------------------------------------------------------------------------------------------
// Read the LWRRENT_SINK ISM cirlwits on the GPU.
// pISinkResults - pointer to a vector to hold the detailed LwrrentSink result info.
// cpmParams   - a vector of ISink Info parameters that can be configured to read all current
//               sinks, read current sinks on a single chain, read a single current sink on a
//               chain, or any combination of the three.
//               The pattern matching algorithm will use the first ISinkInfo record that matches,
//               so put more specific parameters in the vector first.
/*virtual*/
RC GpuIsm::ReadISinks
(
    vector<ISinkInfo> *pResults
    , vector<ISinkInfo> &params
)
{
    RC rc;
    vector<IsmChainFilter> chainFilter;

    // Get the list of Jtag chains with CPMs
    CHECK_RC(GetISMChains(&chainFilter, LW_ISM_lwrrent_sink, -1, -1, ~0));

    // If not found there aren't any CPMs
    if (chainFilter.empty())
    {
        Printf(Tee::PriError, "GpuIsm::ReadISinks: No LWRRENT_SINK ISMs found\n");
        return RC::UNSUPPORTED_FUNCTION;
    }

    // If we get a match for any ISM on a chain, we read the entire chain.
    for (UINT32 idx = 0; idx < chainFilter.size(); idx++)
    {
        const IsmChain * pChain = chainFilter[idx].pChain;
        bool bMatch = false;
        for (size_t ismIdx = 0; ismIdx < pChain->ISMs.size(); ismIdx++)
        {
            if (GetISinkParamIdx(params, 
                                 pChain->Hdr.Chiplet,
                                 pChain->Hdr.InstrId,
                                 pChain->ISMs[ismIdx].Offset) >= 0)
            {
                bMatch = true;
                break;
            }
        }
        if (bMatch)
        {
            vector<UINT32> jtagArray;
            jtagArray.resize((pChain->Hdr.Size + 31) / 32, 0);
        
            CHECK_RC(ReadHost2Jtag(pChain->Hdr.Chiplet,
                                   pChain->Hdr.InstrId,
                                   pChain->Hdr.Size,
                                   &jtagArray));
        
            CHECK_RC(GetISinkChainCounts(pChain, jtagArray, pResults, params));
        }
    }
    return rc;
}

//------------------------------------------------------------------------------
// HOLD measurement APIs by default are not supported.
// The first implementation of this is on T210 CheetAh SOCs
//------------------------------------------------------------------------------

//------------------------------------------------------------------------------
/* static */ bool GpuIsm::PollHoldIsm(void *pvPollData)
{
    PollHoldIsmData *pData = static_cast<PollHoldIsmData *>(pvPollData);
    RegHal *pRegs = pData->pRegs;

    // Must always be index 0 (HOLD ISMs are limited to a single hold ISM per
    // chain and may not be mixed with other ISMs - enforced by read function)
    vector<UINT64> holdBits(pData->pIsm->CallwlateVectorSize(pData->pChain->ISMs[0].Type, 64), 0);
    pData->pollRc = pData->pIsm->ReadHoldIsm(pData->pChain, holdBits);
    if (OK != pData->pollRc)
        return true;

    bool bComplete = false;
    switch (pData->mode)
    {
        case POLL_HOLD_AUTO:
            bComplete = ((pRegs->GetField64(holdBits[0], MODS_ISM_HOLD_MISMATCH_FOUND) == 1) ||
                         (pRegs->GetField64(holdBits[0], MODS_ISM_HOLD_FAULT) == 1));
            break;
        case POLL_HOLD_MANUAL:
            bComplete = (pRegs->GetField64(holdBits[0], MODS_ISM_HOLD_MISMATCH_READY) == 1);
            break;
        case POLL_HOLD_OSC:
            bComplete = (pRegs->GetField64(holdBits[1], MODS_ISM_HOLD_TABLE_ENTRY_VALID) == 1);
            break;
        case POLL_HOLD_READY:
            bComplete = (pRegs->GetField64(holdBits[0], MODS_ISM_HOLD_READY) == 1);
            break;
        default:
            Printf(Tee::PriError, "Unknown Hold ISM poll mode : %d\n", pData->mode);
            pData->pollRc = RC::SOFTWARE_ERROR;
            bComplete = true;
            break;
    }

    return bComplete;
}

namespace
{
    void DisableIsms(Ism *pIsm) { pIsm->EnableISMs(false); }
}
/*virtual*/
RC GpuIsm::ReadHoldISMs //!< Read all Hold ISMs
(
    const HoldInfo   & settings  //!< settings for reading the hold ISMs
    ,vector<HoldInfo>* pInfo     //!< vector to hold the macro specific information
)
{
    RC rc;
    vector<IsmChainFilter> chainFilter;
    CHECK_RC(GetChains(&chainFilter, HOLD, -1, -1, ~0));

    // This reading is self contained and ISMs should be disabled on exit
    // regardless of pass/failure
    Utility::CleanupOnReturnArg<void, void, Ism *> disableIsms(&DisableIsms, this);
    CHECK_RC(EnableISMs(true));

    for (UINT32 idx = 0; idx < chainFilter.size(); idx++)
    {
        CHECK_RC(ReadHoldISMsOnChain(
            chainFilter[idx].pChain->Hdr.InstrId,
            chainFilter[idx].pChain->Hdr.Chiplet,
            settings, pInfo));
    }

    return rc;
}

//------------------------------------------------------------------------------
RC GpuIsm::WriteHoldIsm
(
    const IsmChain *pChain,
    vector<UINT64> &holdBits
)
{
    RC rc;
    vector<UINT32> jtagArray((pChain->Hdr.Size+31)/32, 0);

    // Mask is always 1 (HOLD ISMs are limited to a single hold ISM per
    // chain and may not be mixed with other ISMs - enforced by read function)
    bitset<Ism::maxISMsPerChain> ismMask(0x1);
    CHECK_RC(CopyBits(pChain, jtagArray, holdBits, ismMask));
    CHECK_RC(WriteHost2Jtag(pChain->Hdr.Chiplet,
                            pChain->Hdr.InstrId,
                            pChain->Hdr.Size,
                            jtagArray));
    return rc;
}

//------------------------------------------------------------------------------
// Programming described here
// https://wiki.lwpu.com/wmpwiki/index.php/VLSI-Clocks/LW_ISM_HOLD#Programming_sequence
/*virtual*/
RC GpuIsm::ReadHoldISMsOnChain //!< Read Hold ISMs on a specific chain
(
    INT32               chainId   //!< JTag instruction Id to use
    ,INT32              chiplet   //!< JTag chiplet to use
    ,const HoldInfo   & settings  //!< settings for reading the hold ISMs
    ,vector<HoldInfo> * pInfo     //!< vector to hold the macro specific information
)
{
    RegHal &regs = GetGpuSub()->Regs();
    RC rc;
    vector<IsmChainFilter> chainFilter;
    CHECK_RC(GetChains(&chainFilter, HOLD, chainId, chiplet, ~0));
    if (chainFilter.empty())
        return RC::BAD_PARAMETER;

    // ChainId/Chiplet should be unique
    MASSERT(chainFilter.size() == 1);

    // Only a single HOLD ISM on a chain and it cannot be mixed with other ISMs
    if ((chainFilter[0].ismMask[0] != 1) || (chainFilter[0].ismMask.count() != 1))
    {
        Printf(Tee::PriError,
               "HOLD ISMs are only supported when they are the first and only "
               "ISM on a chain!\n");
        return RC::BAD_PARAMETER;
    }

    const IsmChain * pChain = chainFilter[0].pChain;

    // This reading is self contained and ISMs should be disabled on exit
    // regardless of pass/failure
    Utility::CleanupOnReturnArg<void, void, Ism *> disableIsms(&DisableIsms, this);
    if (GetISMsEnabled())
    {
        // Being called from ReadHoldISMs, allow it to manage ISM disablement
        disableIsms.Cancel();
    }
    else
    {
        CHECK_RC(EnableISMs(true));
    }

    vector<UINT64> holdBits(CallwlateVectorSize(pChain->ISMs[0].Type, 64), 0);
    
    PollHoldIsmData holdPoll = { this, &regs, POLL_HOLD_READY, pChain, OK };
    CHECK_RC(POLLWRAP_HW(PollHoldIsm, &holdPoll, Tasker::GetDefaultTimeoutMs()));
    CHECK_RC(holdPoll.pollRc);

    HoldInfo lwrInfo = settings;
    lwrInfo.chainId = pChain->Hdr.InstrId;
    lwrInfo.chiplet = pChain->Hdr.Chiplet;
    lwrInfo.startBit = pChain->ISMs[0].Offset;

    switch (lwrInfo.mode)
    {
        case HOLD_MANUAL:
            holdBits[0] |= regs.SetField64(MODS_ISM_HOLD_MODE_MANUAL_DEBUG, 1);
            // Intentional fall through

        case HOLD_AUTO:
            holdBits[0] |= regs.SetField64(MODS_ISM_HOLD_MODE_AUTO)          |
                           regs.SetField64(MODS_ISM_HOLD_MODE_SEL, lwrInfo.modeSel);
            if (lwrInfo.bDoubleClk)
                holdBits[0] |= regs.SetField64(MODS_ISM_HOLD_MODE_CLOCK_DOUBLE);
            else
                holdBits[0] |= regs.SetField64(MODS_ISM_HOLD_MODE_CLOCK_SINGLE);
            break;
        case HOLD_OSC:
            holdBits[0] = regs.SetField64(MODS_ISM_HOLD_MODE_OSC)          |
                          regs.SetField64(MODS_ISM_HOLD_MODE_CLOCK_SINGLE);
            break;
        default:
            Printf(Tee::PriError, "ReadHoldISMsOnChain : Unknown hold mode %d\n",
                   lwrInfo.mode);
            return RC::SOFTWARE_ERROR;
    }

    regs.SetField64(&holdBits[0], MODS_ISM_HOLD_WRITE_MODE, 1);
    CHECK_RC(WriteHoldIsm(pChain, holdBits));
    regs.SetField64(&holdBits[0], MODS_ISM_HOLD_WRITE_MODE, 0);

    if (lwrInfo.mode == HOLD_OSC)
    {
        holdBits[0] |= regs.SetField64(MODS_ISM_HOLD_OSC_MODE_CLK_COUNT, lwrInfo.oscClkCount);
        regs.SetField64(&holdBits[0], MODS_ISM_HOLD_WRITE_OSC_MODE_CLK_COUNT, 1);
        CHECK_RC(WriteHoldIsm(pChain, holdBits));
        regs.SetField64(&holdBits[0], MODS_ISM_HOLD_WRITE_OSC_MODE_CLK_COUNT, 0);
    }

    holdBits[0] |= regs.SetField64(MODS_ISM_HOLD_T1_VALUE, lwrInfo.t1Value) |
                   regs.SetField64(MODS_ISM_HOLD_T2_VALUE, lwrInfo.t2Value) |
                   regs.SetField64(MODS_ISM_HOLD_GO, 1);
    CHECK_RC(WriteHoldIsm(pChain, holdBits));

    switch (lwrInfo.mode)
    {
        case HOLD_AUTO:
            holdPoll.mode = POLL_HOLD_AUTO;
            break;
        case HOLD_MANUAL:
            holdPoll.mode = POLL_HOLD_MANUAL;
            break;
        case HOLD_OSC:
            holdPoll.mode = POLL_HOLD_OSC;
            break;
        default:
            return RC::SOFTWARE_ERROR;
    }
    CHECK_RC(POLLWRAP_HW(GpuIsm::PollHoldIsm, &holdPoll, Tasker::GetDefaultTimeoutMs()));
    CHECK_RC(holdPoll.pollRc);

    CHECK_RC(ReadHoldIsm(pChain, holdBits));

    switch (lwrInfo.mode)
    {
        case HOLD_AUTO:
            lwrInfo.bMismatchFound =
                (regs.GetField64(holdBits[0], MODS_ISM_HOLD_MISMATCH_FOUND) == 1);
            lwrInfo.t1RawCount = (UINT32)regs.GetField64(holdBits[1], MODS_ISM_HOLD_T1_READING);
            lwrInfo.t2RawCount = (UINT32)regs.GetField64(holdBits[1], MODS_ISM_HOLD_T2_READING);
            break;
        case HOLD_OSC:
            lwrInfo.t1RawCount  = 
                (UINT32) regs.GetField64(holdBits[0], MODS_ISM_HOLD_T1_OSC_CNT_LOW) |
                (UINT32)(regs.GetField64(holdBits[1], MODS_ISM_HOLD_T1_OSC_CNT_HIGH) <<
                                        regs.LookupMaskSize(MODS_ISM_HOLD_T1_OSC_CNT_LOW));
            lwrInfo.t2RawCount = (UINT32)regs.GetField64(holdBits[1], MODS_ISM_HOLD_T2_OSC_CNT);
            {
                UINT64 pllXPeriodPs = 0;
                CHECK_RC(GetPLLXPeriodPs(&pllXPeriodPs));
                lwrInfo.t1OscPs =
                    ((lwrInfo.oscClkCount * pllXPeriodPs) + (lwrInfo.t1RawCount >> 1)) /
                    lwrInfo.t1RawCount;
                lwrInfo.t2OscPs =
                    ((lwrInfo.oscClkCount * pllXPeriodPs) + (lwrInfo.t2RawCount >> 1)) /
                    lwrInfo.t2RawCount;
            }
            break;

        default:
            return RC::SOFTWARE_ERROR;
    }

    regs.SetField64(&holdBits[0], MODS_ISM_HOLD_GO, 0);
    CHECK_RC(WriteHoldIsm(pChain, holdBits));

    holdPoll.mode = POLL_HOLD_READY;
    CHECK_RC(POLLWRAP_HW(GpuIsm::PollHoldIsm, &holdPoll, Tasker::GetDefaultTimeoutMs()));
    CHECK_RC(holdPoll.pollRc);

    pInfo->push_back(lwrInfo);

    return rc;
}

//------------------------------------------------------------------------------
// Private
RC GpuIsm::ReadHoldIsm
(
    const IsmChain * pChain,
    vector<UINT64> & holdBits
)
{
    RC rc;
    vector<UINT32> jtagArray((pChain->Hdr.Size+31)/32, 0);
    CHECK_RC(ReadHost2Jtag(pChain->Hdr.Chiplet,
                           pChain->Hdr.InstrId,
                           pChain->Hdr.Size,
                           &jtagArray));

    // Must always be index 0 (HOLD ISMs are limited to a single hold ISM per
    // chain and may not be mixed with other ISMs - enforced by read function)
    CHECK_RC(Utility::CopyBits(jtagArray, //src vector
                           holdBits,  //dest vector
                           pChain->ISMs[0].Offset, //src offset
                           0, // dest offset
                           GetISMSize(pChain->ISMs[0].Type))); // numBits
    return rc;
}

//----------------------------------------------------------------------------------
// Setup all NMEAS_lite, NMEAS_lite_v* macros to run an experiment
RC GpuIsm::SetupNoiseMeasLite
(
    const NoiseInfo& param     // structure with setup information
)
{
    RC rc;
    vector<IsmTypes> ismTypes;
    switch (param.version)
    { 
        case 4: ismTypes.push_back(LW_ISM_NMEAS_lite);    break;
        case 5: ismTypes.push_back(LW_ISM_NMEAS_lite_v2); break;
        case 6: ismTypes.push_back(LW_ISM_NMEAS_lite_v3); break;
        case 7: 
            ismTypes.push_back(LW_ISM_NMEAS_lite_v4);
            ismTypes.push_back(LW_ISM_NMEAS_lite_v5);
            ismTypes.push_back(LW_ISM_NMEAS_lite_v6);
            ismTypes.push_back(LW_ISM_NMEAS_lite_v7);
            break;
        case 8:
            ismTypes.push_back(LW_ISM_NMEAS_lite_v8);
            ismTypes.push_back(LW_ISM_NMEAS_lite_v9);
            break;
        default:
            return RC::BAD_PARAMETER;    
    }
    vector<IsmChainFilter> chainFilter;
    for (auto ismType : ismTypes)
    {
        CHECK_RC(GetISMChains(&chainFilter, ismType, -1, -1, Ism::NORMAL_CHAIN));
    }

    for (UINT32 idx = 0; idx < chainFilter.size(); idx++)
    {
        CHECK_RC(SetupNoiseMeasLiteOnChain(
            chainFilter[idx].pChain->Hdr.InstrId,
            chainFilter[idx].pChain->Hdr.Chiplet,
            param));
    }

    return rc;
}

//------------------------------------------------------------------------------
// Setup all the NMEAS_lite macros on a specific chain to run an experiment
RC GpuIsm::SetupNoiseMeasLiteOnChain
(
    INT32 chainId
    ,INT32 chiplet
    ,const NoiseInfo& param   // structure with setup information
)
{
    RC rc;
    vector <UINT32> jtagArray;
    vector<IsmTypes> ismTypes;
    vector<IsmChainFilter> chainFilter;
    const IsmChain * pChain = nullptr;
    
    // Determine the type of NoiseMeas circuit we are trying to run based on the 
    // parameter version. Each NoseMeas version has a different set of parameters and
    // are mutually exclusive.
    switch (param.version)
    { 
        case 4: ismTypes.push_back(LW_ISM_NMEAS_lite);
            break;
        case 5: ismTypes.push_back(LW_ISM_NMEAS_lite_v2);
            break;
        case 6: ismTypes.push_back(LW_ISM_NMEAS_lite_v3);
            break;
        case 7: 
            ismTypes.push_back(LW_ISM_NMEAS_lite_v4);
            ismTypes.push_back(LW_ISM_NMEAS_lite_v5);
            ismTypes.push_back(LW_ISM_NMEAS_lite_v6);
            ismTypes.push_back(LW_ISM_NMEAS_lite_v7);
            break;
        case 8:
            ismTypes.push_back(LW_ISM_NMEAS_lite_v8);
            ismTypes.push_back(LW_ISM_NMEAS_lite_v9);
            break;
        default:
            return RC::BAD_PARAMETER;    
    }

    // Get the Jtag chain for given chainId, chiplet, & ismType
    // Note: At this point we should be dealing with a single chain but the chain can have multiple
    //       ismTypes that need programming. ie. a single chain could have LW_ISM_NMEAS_lite_v6 &
    //       LW_ISM_NMEAS_lite_v7, see GA102.
    for (auto ismType : ismTypes)
    {
        chainFilter.clear();
        CHECK_RC(GetISMChains(&chainFilter, ismType, chainId, chiplet, Ism::NORMAL_CHAIN));

        // If not found the chainId is probably wrong
        if (!chainFilter.empty())
        {
            // ChainId/Chiplet should be unique
            MASSERT(chainFilter.size() == 1);
    
            pChain = chainFilter[0].pChain;
            jtagArray.resize((pChain->Hdr.Size + 31) / 32, 0);
    
            // Configure the NMEAS_lite ISMs as requested
            vector<UINT64> ismBits(CallwlateVectorSize(ismType, 64), 0);
            CHECK_RC(ConfigureNoiseLiteBits(ismType, ismBits, param));
            CHECK_RC(CopyBits(pChain, jtagArray, ismBits, chainFilter[0].ismMask));
        }
    }
    
    if (pChain)
    {
        CHECK_RC(WriteHost2Jtag(pChain->Hdr.Chiplet,
            pChain->Hdr.InstrId,
            pChain->Hdr.Size,
            jtagArray));

        return rc;
    }
    return RC::BAD_PARAMETER;
}

//------------------------------------------------------------------------------
// Configure the noise bits using the MODS regHal macros
RC GpuIsm::ConfigureNoiseLiteBits
(
    IsmTypes          ismType
    ,vector<UINT64>&  ismBits
    ,const NoiseInfo& nMeas
)
{
    RegHal &regs = GetGpuSub()->Regs();
    ismBits.resize(CallwlateVectorSize(ismType, 64), 0);

    switch (ismType)
    {
        case LW_ISM_NMEAS_lite:
            ismBits[0] = (
                regs.SetField64(MODS_ISM_NMEAS_LITE_V1_NM_TRIM,       nMeas.niLite.trim)        |
                regs.SetField64(MODS_ISM_NMEAS_LITE_V1_NM_TAP,        nMeas.niLite.tap)         |
                regs.SetField64(MODS_ISM_NMEAS_LITE_V1_IDDQ,          nMeas.niLite.iddq)        |
                regs.SetField64(MODS_ISM_NMEAS_LITE_V1_MODE,          nMeas.niLite.mode)        |
                regs.SetField64(MODS_ISM_NMEAS_LITE_V1_SEL,           nMeas.niLite.sel)         |
                regs.SetField64(MODS_ISM_NMEAS_LITE_V1_DIV,           nMeas.niLite.div)         |
                regs.SetField64(MODS_ISM_NMEAS_LITE_V1_DIV_CLK_IN,    nMeas.niLite.divClkIn)    |
                regs.SetField64(MODS_ISM_NMEAS_LITE_V1_FIXED_DLY_SEL, nMeas.niLite.fixedDlySel) |
                regs.SetField64(MODS_ISM_NMEAS_LITE_V1_COUNT,         nMeas.niLite.count)
                );
            break;

        case LW_ISM_NMEAS_lite_v2:
            ismBits[0] = (
                regs.SetField64(MODS_ISM_NMEAS_LITE_V2_NM_TRIM,       nMeas.niLite.trim)        |
                regs.SetField64(MODS_ISM_NMEAS_LITE_V2_NM_TAP,        nMeas.niLite.tap)         |
                regs.SetField64(MODS_ISM_NMEAS_LITE_V2_IDDQ,          nMeas.niLite.iddq)        |
                regs.SetField64(MODS_ISM_NMEAS_LITE_V2_MODE,          nMeas.niLite.mode)        |
                regs.SetField64(MODS_ISM_NMEAS_LITE_V2_SEL,           nMeas.niLite.sel)         |
                regs.SetField64(MODS_ISM_NMEAS_LITE_V2_DIV,           nMeas.niLite.div)         |
                regs.SetField64(MODS_ISM_NMEAS_LITE_V2_DIV_CLK_IN,    nMeas.niLite.divClkIn)    |
                regs.SetField64(MODS_ISM_NMEAS_LITE_V2_FIXED_DLY_SEL, nMeas.niLite.fixedDlySel) |
                regs.SetField64(MODS_ISM_NMEAS_LITE_V2_REFCLK_SEL,    nMeas.niLite.refClkSel)   |
                regs.SetField64(MODS_ISM_NMEAS_LITE_V2_COUNT,         nMeas.niLite.count)
                );
            break;

        case LW_ISM_NMEAS_lite_v3:
            ismBits[0] = (
                regs.SetField64(MODS_ISM_NMEAS_LITE_V3_TRIM,           nMeas.niLite3.trim)       |
                regs.SetField64(MODS_ISM_NMEAS_LITE_V3_TAP_SEL,        nMeas.niLite3.tap)        |
                regs.SetField64(MODS_ISM_NMEAS_LITE_V3_IDDQ,           nMeas.niLite3.iddq)       |
                regs.SetField64(MODS_ISM_NMEAS_LITE_V3_MODE,           nMeas.niLite3.mode)       |
                regs.SetField64(MODS_ISM_NMEAS_LITE_V3_CLK_SEL,        nMeas.niLite3.clkSel)     |
                regs.SetField64(MODS_ISM_NMEAS_LITE_V3_OUT_SEL,        nMeas.niLite3.outSel)     |
                regs.SetField64(MODS_ISM_NMEAS_LITE_V3_DIV,            nMeas.niLite3.div)        |
                regs.SetField64(MODS_ISM_NMEAS_LITE_V3_DIV_CLK_IN,     nMeas.niLite3.divClkIn)   |
                regs.SetField64(MODS_ISM_NMEAS_LITE_V3_FIXED_DLY_SEL,  nMeas.niLite3.fixedDlySel)|
                regs.SetField64(MODS_ISM_NMEAS_LITE_V3_REFCLK_SEL,     nMeas.niLite3.refClkSel)  |
                regs.SetField64(MODS_ISM_NMEAS_LITE_V3_JTAG_OVERRIDE,  nMeas.niLite3.jtagOvr)    |
                regs.SetField64(MODS_ISM_NMEAS_LITE_V3_WINDOW_LENGTH,  nMeas.niLite3.winLen)     |
                regs.SetField64(MODS_ISM_NMEAS_LITE_V3_COUNTER_ENABLE, nMeas.niLite3.cntrEn)
                ); 
            ismBits[1] = (
                regs.SetField64(MODS_ISM_NMEAS_LITE_V3_COUNTER_CLEAR,       nMeas.niLite3.cntrClr   ) | //$
                regs.SetField64(MODS_ISM_NMEAS_LITE_V3_COUNTER_DIRECTION,   nMeas.niLite3.cntrDir   ) | //$
                regs.SetField64(MODS_ISM_NMEAS_LITE_V3_COUNTER_TO_READ,     nMeas.niLite3.cntrToRead) | //$
                regs.SetField64(MODS_ISM_NMEAS_LITE_V3_COUNTER_OFFSET,      nMeas.niLite3.cntrOffset) | //$
                regs.SetField64(MODS_ISM_NMEAS_LITE_V3_ERROR_ENABLE,        nMeas.niLite3.errEn     ) | //$
                regs.SetField64(MODS_ISM_NMEAS_LITE_V3_SPARE_IN,            nMeas.niLite3.spareIn   ) | //$
                regs.SetField64(MODS_ISM_NMEAS_LITE_V3_THRESHOLD,           nMeas.niLite3.thresh    ) | //$
                regs.SetField64(MODS_ISM_NMEAS_LITE_V3_MNMX_WIN_ENABLE,     nMeas.niLite3.mnMxWinEn ) | //$
                regs.SetField64(MODS_ISM_NMEAS_LITE_V3_MNMX_WIN_DURATION,   nMeas.niLite3.mnMxWinDur) | //$
                regs.SetField64(MODS_ISM_NMEAS_LITE_V3_THRESHOLD_DIRECTION, nMeas.niLite3.threshDir ) | //$
                regs.SetField64(MODS_ISM_NMEAS_LITE_V3_RO_ENABLE,           nMeas.niLite3.roEn      )   //$
                );       
            // TODO: see if these fields need to be programmed on not  
            ismBits[2] = (
                regs.SetField64(MODS_ISM_NMEAS_LITE_V3_ERROR_OUT,           nMeas.niLite3.errOut    ) | //$
                regs.SetField64(MODS_ISM_NMEAS_LITE_V3_SPARE_OUT,           nMeas.niLite3.spareOut  )   //$
                );       
            break;

        case LW_ISM_NMEAS_lite_v4:
            ismBits[0] = (
                regs.SetField64(MODS_ISM_NMEAS_LITE_V4_TRIM,           nMeas.niLite4.trim)       |
                regs.SetField64(MODS_ISM_NMEAS_LITE_V4_TAP_SEL,        nMeas.niLite4.tap)        |
                regs.SetField64(MODS_ISM_NMEAS_LITE_V4_IDDQ,           nMeas.niLite4.iddq)       |
                regs.SetField64(MODS_ISM_NMEAS_LITE_V4_MODE,           nMeas.niLite4.mode)       |
                regs.SetField64(MODS_ISM_NMEAS_LITE_V4_CLK_SEL,        nMeas.niLite4.clkSel)     |
                regs.SetField64(MODS_ISM_NMEAS_LITE_V4_OUT_SEL,        nMeas.niLite4.outSel)     |
                regs.SetField64(MODS_ISM_NMEAS_LITE_V4_DIV,            nMeas.niLite4.div)        |
                regs.SetField64(MODS_ISM_NMEAS_LITE_V4_DIV_CLK_IN,     nMeas.niLite4.divClkIn)   |
                regs.SetField64(MODS_ISM_NMEAS_LITE_V4_FIXED_DLY_SEL,  nMeas.niLite4.fixedDlySel)|
                regs.SetField64(MODS_ISM_NMEAS_LITE_V4_REFCLK_SEL,     nMeas.niLite4.refClkSel)  |
                regs.SetField64(MODS_ISM_NMEAS_LITE_V4_JTAG_OVERRIDE,  nMeas.niLite4.jtagOvr)    |
                regs.SetField64(MODS_ISM_NMEAS_LITE_V4_WINDOW_LENGTH,  nMeas.niLite4.winLen)     |
                regs.SetField64(MODS_ISM_NMEAS_LITE_V4_COUNTER_ENABLE, nMeas.niLite4.cntrEn)
                ); 
            ismBits[1] = (
                regs.SetField64(MODS_ISM_NMEAS_LITE_V4_COUNTER_CLEAR,       nMeas.niLite4.cntrClr   ) | //$
                regs.SetField64(MODS_ISM_NMEAS_LITE_V4_COUNTER_DIRECTION,   nMeas.niLite4.cntrDir   ) | //$
                regs.SetField64(MODS_ISM_NMEAS_LITE_V4_COUNTER_TO_READ,     nMeas.niLite4.cntrToRead) | //$
                regs.SetField64(MODS_ISM_NMEAS_LITE_V4_COUNTER_OFFSET,      nMeas.niLite4.cntrOffset) | //$
                regs.SetField64(MODS_ISM_NMEAS_LITE_V4_ERROR_ENABLE,        nMeas.niLite4.errEn     ) | //$
                regs.SetField64(MODS_ISM_NMEAS_LITE_V4_SPARE_IN,            nMeas.niLite4.spareIn   ) | //$
                regs.SetField64(MODS_ISM_NMEAS_LITE_V4_THRESHOLD,           nMeas.niLite4.thresh    ) | //$
                regs.SetField64(MODS_ISM_NMEAS_LITE_V4_THRESHOLD_DIRECTION, nMeas.niLite4.threshDir ) | //$
                regs.SetField64(MODS_ISM_NMEAS_LITE_V4_RO_ENABLE,           nMeas.niLite4.roEn      )   //$
                );       
            ismBits[2] = (
                regs.SetField64(MODS_ISM_NMEAS_LITE_V4_ERROR_OUT,           nMeas.niLite4.errOut    ) | //$
                regs.SetField64(MODS_ISM_NMEAS_LITE_V4_SPARE_OUT,           nMeas.niLite4.spareOut  )   //$
                );       
            break;

        case LW_ISM_NMEAS_lite_v5:
            // niLite4 supports NMEAS_lite_v4 - v7
            ismBits[0] = (
                regs.SetField64(MODS_ISM_NMEAS_LITE_V5_TRIM,           nMeas.niLite4.trim)       |
                regs.SetField64(MODS_ISM_NMEAS_LITE_V5_TAP_SEL,        nMeas.niLite4.tap)        |
                regs.SetField64(MODS_ISM_NMEAS_LITE_V5_IDDQ,           nMeas.niLite4.iddq)       |
                regs.SetField64(MODS_ISM_NMEAS_LITE_V5_MODE,           nMeas.niLite4.mode)       |
                regs.SetField64(MODS_ISM_NMEAS_LITE_V5_CLK_SEL,        nMeas.niLite4.clkSel)     |
                regs.SetField64(MODS_ISM_NMEAS_LITE_V5_OUT_SEL,        nMeas.niLite4.outSel)     |
                regs.SetField64(MODS_ISM_NMEAS_LITE_V5_DIV,            nMeas.niLite4.div)        |
                regs.SetField64(MODS_ISM_NMEAS_LITE_V5_DIV_CLK_IN,     nMeas.niLite4.divClkIn)   |
                regs.SetField64(MODS_ISM_NMEAS_LITE_V5_FIXED_DLY_SEL,  nMeas.niLite4.fixedDlySel)|
                regs.SetField64(MODS_ISM_NMEAS_LITE_V5_REFCLK_SEL,     nMeas.niLite4.refClkSel)  |
                regs.SetField64(MODS_ISM_NMEAS_LITE_V5_JTAG_OVERRIDE,  nMeas.niLite4.jtagOvr)    |
                regs.SetField64(MODS_ISM_NMEAS_LITE_V5_WINDOW_LENGTH,  nMeas.niLite4.winLen)     |
                regs.SetField64(MODS_ISM_NMEAS_LITE_V5_COUNTER_ENABLE, nMeas.niLite4.cntrEn)
                ); 
            ismBits[1] = (
                regs.SetField64(MODS_ISM_NMEAS_LITE_V5_COUNTER_CLEAR,       nMeas.niLite4.cntrClr   ) | //$
                regs.SetField64(MODS_ISM_NMEAS_LITE_V5_COUNTER_DIRECTION,   nMeas.niLite4.cntrDir   ) | //$
                regs.SetField64(MODS_ISM_NMEAS_LITE_V5_COUNTER_TO_READ,     nMeas.niLite4.cntrToRead) | //$
                regs.SetField64(MODS_ISM_NMEAS_LITE_V5_COUNTER_OFFSET,      nMeas.niLite4.cntrOffset) | //$
                regs.SetField64(MODS_ISM_NMEAS_LITE_V5_ERROR_ENABLE,        nMeas.niLite4.errEn     ) | //$
                regs.SetField64(MODS_ISM_NMEAS_LITE_V5_SPARE_IN,            nMeas.niLite4.spareIn   ) | //$
                regs.SetField64(MODS_ISM_NMEAS_LITE_V5_THRESHOLD,           nMeas.niLite4.thresh    ) | //$
                regs.SetField64(MODS_ISM_NMEAS_LITE_V5_THRESHOLD_DIRECTION, nMeas.niLite4.threshDir ) | //$
                regs.SetField64(MODS_ISM_NMEAS_LITE_V5_RO_ENABLE,           nMeas.niLite4.roEn      ) | //$
                regs.SetField64(MODS_ISM_NMEAS_LITE_V5_FABRO_ENABLE,        nMeas.niLite4.fabRoEn   ) | //$
                regs.SetField64(MODS_ISM_NMEAS_LITE_V5_FABRO_S_BCAST,       nMeas.niLite4.fabRoSBcast)   | //$
                regs.SetField64(MODS_ISM_NMEAS_LITE_V5_FABRO_SW_POLARITY,   nMeas.niLite4.fabRoSwPol)    | //$
                regs.SetField64(MODS_ISM_NMEAS_LITE_V5_FABRO_IDX_BCAST,     nMeas.niLite4.fabRoIdxBcast) | //$
                regs.SetField64(MODS_ISM_NMEAS_LITE_V5_FABRO_RO_SEL,        nMeas.niLite4.fabRoSel)      | //$
                regs.SetField64(MODS_ISM_NMEAS_LITE_V5_FABRO_SPARE_IN,      nMeas.niLite4.fabRoSpareIn)    //$
                );       
            ismBits[2] = (
                regs.SetField64(MODS_ISM_NMEAS_LITE_V5_ERROR_OUT,           nMeas.niLite4.errOut    ) | //$
                regs.SetField64(MODS_ISM_NMEAS_LITE_V5_SPARE_OUT,           nMeas.niLite4.spareOut  )   //$
                );       
            break;

        case LW_ISM_NMEAS_lite_v6:
            // niLite4 supports NMEAS_lite_v4 - v7
            ismBits[0] = (
                regs.SetField64(MODS_ISM_NMEAS_LITE_V6_TRIM,           nMeas.niLite4.trim)       |
                regs.SetField64(MODS_ISM_NMEAS_LITE_V6_TAP_SEL,        nMeas.niLite4.tap)        |
                regs.SetField64(MODS_ISM_NMEAS_LITE_V6_IDDQ,           nMeas.niLite4.iddq)       |
                regs.SetField64(MODS_ISM_NMEAS_LITE_V6_MODE,           nMeas.niLite4.mode)       |
                regs.SetField64(MODS_ISM_NMEAS_LITE_V6_CLK_SEL,        nMeas.niLite4.clkSel)     |
                regs.SetField64(MODS_ISM_NMEAS_LITE_V6_OUT_SEL,        nMeas.niLite4.outSel)     |
                regs.SetField64(MODS_ISM_NMEAS_LITE_V6_DIV,            nMeas.niLite4.div)        |
                regs.SetField64(MODS_ISM_NMEAS_LITE_V6_DIV_CLK_IN,     nMeas.niLite4.divClkIn)   |
                regs.SetField64(MODS_ISM_NMEAS_LITE_V6_FIXED_DLY_SEL,  nMeas.niLite4.fixedDlySel)|
                regs.SetField64(MODS_ISM_NMEAS_LITE_V6_REFCLK_SEL,     nMeas.niLite4.refClkSel)  |
                regs.SetField64(MODS_ISM_NMEAS_LITE_V6_JTAG_OVERRIDE,  nMeas.niLite4.jtagOvr)    |
                regs.SetField64(MODS_ISM_NMEAS_LITE_V6_ERROR_ENABLE,   nMeas.niLite4.errEn)      |
                regs.SetField64(MODS_ISM_NMEAS_LITE_V6_SPARE_IN,       nMeas.niLite4.spareIn)    | //$
                regs.SetField64(MODS_ISM_NMEAS_LITE_V6_THRESHOLD,      nMeas.niLite4.thresh    ) | //$
                regs.SetField64(MODS_ISM_NMEAS_LITE_V6_THRESHOLD_DIRECTION, nMeas.niLite4.threshDir ) | //$
                regs.SetField64(MODS_ISM_NMEAS_LITE_V6_RO_ENABLE,      nMeas.niLite4.roEn      ) | //$
                regs.SetField64(MODS_ISM_NMEAS_LITE_V6_STREAM_SEL,     nMeas.niLite4.streamSel ) | //$
                regs.SetField64(MODS_ISM_NMEAS_LITE_V6_STREAMING_MODE, nMeas.niLite4.streamMode) | //$
                regs.SetField64(MODS_ISM_NMEAS_LITE_V6_SEL_STREAMING_TRIGGER_SRC,   nMeas.niLite4.selStreamTrigSrc) | //$
                regs.SetField64(MODS_ISM_NMEAS_LITE_V6_SAMPLING_PERIOD,             nMeas.niLite4.samplePeriod    ) | //$
                regs.SetField64(MODS_ISM_NMEAS_LITE_V6_STREAMING_TRIGGER_SRC,       nMeas.niLite4.streamTrigSrc   )   //$
                ); 
            ismBits[1] = (
                regs.SetField64(MODS_ISM_NMEAS_LITE_V6_ERROR_OUT,      nMeas.niLite4.errOut        ) | //$
                regs.SetField64(MODS_ISM_NMEAS_LITE_V6_SPARE_OUT_LOW,  nMeas.niLite4.spareOut &
                                 regs.LookupMask(MODS_ISM_NMEAS_LITE_V6_SPARE_OUT_LOW))   //$
                );       
            ismBits[2] = (
                regs.SetField64(MODS_ISM_NMEAS_LITE_V6_SPARE_OUT_HI,  nMeas.niLite4.spareOut >> 
                                regs.LookupMaskSize(MODS_ISM_NMEAS_LITE_V6_SPARE_OUT_LOW)  )   //$
                );       
            break;

        case LW_ISM_NMEAS_lite_v7:
            // niLite4 supports NMEAS_lite_v4 - v7
            ismBits[0] = (
                regs.SetField64(MODS_ISM_NMEAS_LITE_V7_TRIM,           nMeas.niLite4.trim)       |
                regs.SetField64(MODS_ISM_NMEAS_LITE_V7_TAP_SEL,        nMeas.niLite4.tap)        |
                regs.SetField64(MODS_ISM_NMEAS_LITE_V7_IDDQ,           nMeas.niLite4.iddq)       |
                regs.SetField64(MODS_ISM_NMEAS_LITE_V7_MODE,           nMeas.niLite4.mode)       |
                regs.SetField64(MODS_ISM_NMEAS_LITE_V7_CLK_SEL,        nMeas.niLite4.clkSel)     |
                regs.SetField64(MODS_ISM_NMEAS_LITE_V7_OUT_SEL,        nMeas.niLite4.outSel)     |
                regs.SetField64(MODS_ISM_NMEAS_LITE_V7_DIV,            nMeas.niLite4.div)        |
                regs.SetField64(MODS_ISM_NMEAS_LITE_V7_DIV_CLK_IN,     nMeas.niLite4.divClkIn)   |
                regs.SetField64(MODS_ISM_NMEAS_LITE_V7_FIXED_DLY_SEL,  nMeas.niLite4.fixedDlySel)|
                regs.SetField64(MODS_ISM_NMEAS_LITE_V7_REFCLK_SEL,     nMeas.niLite4.refClkSel)  |
                regs.SetField64(MODS_ISM_NMEAS_LITE_V7_JTAG_OVERRIDE,  nMeas.niLite4.jtagOvr)    |
                regs.SetField64(MODS_ISM_NMEAS_LITE_V7_ERROR_ENABLE,   nMeas.niLite4.errEn)      |
                regs.SetField64(MODS_ISM_NMEAS_LITE_V7_SPARE_IN,       nMeas.niLite4.spareIn)    | //$
                regs.SetField64(MODS_ISM_NMEAS_LITE_V7_THRESHOLD,      nMeas.niLite4.thresh    ) | //$
                regs.SetField64(MODS_ISM_NMEAS_LITE_V7_THRESHOLD_DIRECTION, nMeas.niLite4.threshDir ) | //$
                regs.SetField64(MODS_ISM_NMEAS_LITE_V7_RO_ENABLE,      nMeas.niLite4.roEn      ) | //$
                regs.SetField64(MODS_ISM_NMEAS_LITE_V7_STREAM_SEL,     nMeas.niLite4.streamSel ) | //$
                regs.SetField64(MODS_ISM_NMEAS_LITE_V7_STREAMING_MODE, nMeas.niLite4.streamMode) | //$
                regs.SetField64(MODS_ISM_NMEAS_LITE_V7_SEL_STREAMING_TRIGGER_SRC,   nMeas.niLite4.selStreamTrigSrc) | //$
                regs.SetField64(MODS_ISM_NMEAS_LITE_V7_SAMPLING_PERIOD,             nMeas.niLite4.samplePeriod    ) | //$
                regs.SetField64(MODS_ISM_NMEAS_LITE_V7_STREAMING_TRIGGER_SRC,       nMeas.niLite4.streamTrigSrc   ) | //$
                regs.SetField64(MODS_ISM_NMEAS_LITE_V7_READY_BYPASS,                nMeas.niLite4.readyBypass     )   //$
                ); 
            ismBits[1] =
                regs.SetField64(MODS_ISM_NMEAS_LITE_V7_ERROR_OUT, nMeas.niLite4.errOut);
            ismBits[2] =
                regs.SetField64(MODS_ISM_NMEAS_LITE_V7_SPARE_OUT, nMeas.niLite4.spareOut);
            break;

        case LW_ISM_NMEAS_lite_v8:
            // niLite8 supports NMEAS_lite_v8 - v9
            ismBits[0] = (
                regs.SetField64(MODS_ISM_NMEAS_LITE_V8_IDDQ,             nMeas.niLite8.iddq       )|    //$
                regs.SetField64(MODS_ISM_NMEAS_LITE_V8_MODE,             nMeas.niLite8.mode       )|    //$
                regs.SetField64(MODS_ISM_NMEAS_LITE_V8_OUT_SEL,          nMeas.niLite8.outSel     )|    //$
                regs.SetField64(MODS_ISM_NMEAS_LITE_V8_REFCLK_SEL,       nMeas.niLite8.clkSel     )|    //$
                regs.SetField64(MODS_ISM_NMEAS_LITE_V8_DIV,              nMeas.niLite8.div        )|    //$
                regs.SetField64(MODS_ISM_NMEAS_LITE_V8_CLK_PHASE_ILWERT, nMeas.niLite8.clkPhaseIlw)|    //$
                regs.SetField64(MODS_ISM_NMEAS_LITE_V8_DIV_CLK_IN,       nMeas.niLite8.divClkIn   )|    //$
                regs.SetField64(MODS_ISM_NMEAS_LITE_V8_TRIM_INIT_A,      nMeas.niLite8.trimInitA  )|    //$
                regs.SetField64(MODS_ISM_NMEAS_LITE_V8_TRIM_INIT_B,      nMeas.niLite8.trimInitB  )|    //$
                regs.SetField64(MODS_ISM_NMEAS_LITE_V8_TRIM_TYPE_SEL,    nMeas.niLite8.trimTypeSel)|    //$
                regs.SetField64(MODS_ISM_NMEAS_LITE_V8_TAP_TYPE_SEL,     nMeas.niLite8.tapTypeSel )|    //$
                regs.SetField64(MODS_ISM_NMEAS_LITE_V8_TRIM_INIT_C,      nMeas.niLite8.trimInitC  )|    //$
                regs.SetField64(MODS_ISM_NMEAS_LITE_V8_FIXED_DELAY_SEL,  nMeas.niLite8.fixedDlySel)|    //$
                regs.SetField64(MODS_ISM_NMEAS_LITE_V8_TAP_SEL,          nMeas.niLite8.tapSel     )|    //$
                regs.SetField64(MODS_ISM_NMEAS_LITE_V8_JTAG_OVERRIDE,    nMeas.niLite8.jtagOvr    )|    //$
                regs.SetField64(MODS_ISM_NMEAS_LITE_V8_THRESHOLD_MAX,    nMeas.niLite8.threshMax  )|    //$
                regs.SetField64(MODS_ISM_NMEAS_LITE_V8_THRESHOLD_MIN,    nMeas.niLite8.threshMin  ));   //$
            ismBits[1] = (
                regs.SetField64(MODS_ISM_NMEAS_LITE_V8_THRESHOLD_ENABLE,         nMeas.niLite8.threshEn      )|     //$
                regs.SetField64(MODS_ISM_NMEAS_LITE_V8_SPARE_IN,                 nMeas.niLite8.spareIn       )|     //$
                regs.SetField64(MODS_ISM_NMEAS_LITE_V8_COUNTER_ENABLE,           nMeas.niLite8.cntrEn        )|     //$
                regs.SetField64(MODS_ISM_NMEAS_LITE_V8_WINDOW_LENGTH,            nMeas.niLite8.winLen        )|     //$
                regs.SetField64(MODS_ISM_NMEAS_LITE_V8_COUNTER_CLEAR,            nMeas.niLite8.cntrClr       )|     //$
                regs.SetField64(MODS_ISM_NMEAS_LITE_V8_COUNTER_OFFSET_DIRECTION, nMeas.niLite8.cntrOffsetDir )|     //$
                regs.SetField64(MODS_ISM_NMEAS_LITE_V8_COUNTER_TO_READ,          nMeas.niLite8.cntrToRead    )|     //$
                regs.SetField64(MODS_ISM_NMEAS_LITE_V8_COUNTER_TRIM_OFFSET,      nMeas.niLite8.cntrTrimOffset));    //$
            ismBits[2] = (
                regs.SetField64(MODS_ISM_NMEAS_LITE_V8_READY,                    nMeas.niLite8.ready       )|  //$
                regs.SetField64(MODS_ISM_NMEAS_LITE_V8_THRESHOLD_OUT,            nMeas.niLite8.threshOut   )|  //$
                regs.SetField64(MODS_ISM_NMEAS_LITE_V8_THRESHOLD_MAX_OUT,        nMeas.niLite8.threshMaxOut)|  //$
                regs.SetField64(MODS_ISM_NMEAS_LITE_V8_THRESHOLD_MIN_OUT,        nMeas.niLite8.threshMinOut)|  //$
                regs.SetField64(MODS_ISM_NMEAS_LITE_V8_SPARE_OUT,                nMeas.niLite8.spareOut    )); //$
            break;
            
        case LW_ISM_NMEAS_lite_v9:
            // niLite8 supports NMEAS_lite_v8 - v9
            ismBits[0] = (
                regs.SetField64(MODS_ISM_NMEAS_LITE_V9_IDDQ,                      nMeas.niLite8.iddq       )|    //$
                regs.SetField64(MODS_ISM_NMEAS_LITE_V9_MODE,                      nMeas.niLite8.mode       )|    //$
                regs.SetField64(MODS_ISM_NMEAS_LITE_V9_OUT_SEL,                   nMeas.niLite8.outSel     )|    //$
                regs.SetField64(MODS_ISM_NMEAS_LITE_V9_REFCLK_SEL,                nMeas.niLite8.clkSel     )|    //$
                regs.SetField64(MODS_ISM_NMEAS_LITE_V9_DIV,                       nMeas.niLite8.div        )|    //$
                regs.SetField64(MODS_ISM_NMEAS_LITE_V9_CLK_PHASE_ILWERT,          nMeas.niLite8.clkPhaseIlw)|    //$
                regs.SetField64(MODS_ISM_NMEAS_LITE_V9_DIV_CLK_IN,                nMeas.niLite8.divClkIn   )|    //$
                regs.SetField64(MODS_ISM_NMEAS_LITE_V9_TRIM_INIT_A,               nMeas.niLite8.trimInitA  )|    //$
                regs.SetField64(MODS_ISM_NMEAS_LITE_V9_TRIM_INIT_B,               nMeas.niLite8.trimInitB  )|    //$
                regs.SetField64(MODS_ISM_NMEAS_LITE_V9_TRIM_TYPE_SEL,             nMeas.niLite8.trimTypeSel)|    //$
                regs.SetField64(MODS_ISM_NMEAS_LITE_V9_TAP_TYPE_SEL,              nMeas.niLite8.tapTypeSel )|    //$
                regs.SetField64(MODS_ISM_NMEAS_LITE_V9_TRIM_INIT_C,               nMeas.niLite8.trimInitC  )|    //$
                regs.SetField64(MODS_ISM_NMEAS_LITE_V9_FIXED_DELAY_SEL,           nMeas.niLite8.fixedDlySel)|    //$
                regs.SetField64(MODS_ISM_NMEAS_LITE_V9_TAP_SEL,                   nMeas.niLite8.tapSel     )|    //$
                regs.SetField64(MODS_ISM_NMEAS_LITE_V9_JTAG_OVERRIDE,             nMeas.niLite8.jtagOvr    )|    //$
                regs.SetField64(MODS_ISM_NMEAS_LITE_V9_THRESHOLD_MAX,             nMeas.niLite8.threshMax  )|    //$
                regs.SetField64(MODS_ISM_NMEAS_LITE_V9_THRESHOLD_MIN,             nMeas.niLite8.threshMin  ));   //$
            ismBits[1] = (
                regs.SetField64(MODS_ISM_NMEAS_LITE_V9_THRESHOLD_ENABLE,          nMeas.niLite8.threshEn        )|  //$
                regs.SetField64(MODS_ISM_NMEAS_LITE_V9_SPARE_IN,                  nMeas.niLite8.spareIn         )|  //$
                regs.SetField64(MODS_ISM_NMEAS_LITE_V9_STREAM_SEL,                nMeas.niLite8.streamSel       )|  //$
                regs.SetField64(MODS_ISM_NMEAS_LITE_V9_STREAMING_MODE,            nMeas.niLite8.streamMode      )|  //$
                regs.SetField64(MODS_ISM_NMEAS_LITE_V9_SEL_STREAMING_TRIGGER_SRC, nMeas.niLite8.selStreamTrigSrc)|  //$
                regs.SetField64(MODS_ISM_NMEAS_LITE_V9_SAMPLING_PERIOD,           nMeas.niLite8.samplePeriod    )|  //$
                regs.SetField64(MODS_ISM_NMEAS_LITE_V9_STREAMING_TRIGGER_SRC,     nMeas.niLite8.streamTrigSrc   )|  //$
                regs.SetField64(MODS_ISM_NMEAS_LITE_V9_READY_BYPASS,              nMeas.niLite8.readyBypass     )|  //$
                regs.SetField64(MODS_ISM_NMEAS_LITE_V9_READY,                     nMeas.niLite8.ready           )|  //$
                regs.SetField64(MODS_ISM_NMEAS_LITE_V9_THRESHOLD_OUT,             nMeas.niLite8.threshOut       )); //$
            ismBits[2] = (
                regs.SetField64(MODS_ISM_NMEAS_LITE_V9_THRESHOLD_MAX_OUT,         nMeas.niLite8.threshMaxOut)|  //$
                regs.SetField64(MODS_ISM_NMEAS_LITE_V9_THRESHOLD_MIN_OUT,         nMeas.niLite8.threshMinOut)|  //$
                regs.SetField64(MODS_ISM_NMEAS_LITE_V9_SPARE_OUT,                 nMeas.niLite8.spareOut    )); //$
            break;
        default:
            return RC::BAD_PARAMETER;
    }
    return OK;
}

//------------------------------------------------------------------------------
// Reads the current NMEAS_lite ISM counts on all of the chains.
// Note: When the experiment has been setup with LoopMode=true the experiment will never complete.
//       However, the caller can halt the experiment via SetupISMController() to read the current
//       chain counts. To do the the caller must do the following:
//       a) Halt the experiment via SetupISMController(..., haltMode=true)
//       b) Call this API with bPollExperiment = false
// If bPollExperiment = true the routine will read the complete status from the last cached ISM
// controller. This could be either the PRIMARY controller or one of the chains with the 
// sub-controllers. It depends on call sequence from the user.
RC GpuIsm::ReadNoiseMeasLite
(
    vector<UINT32> *pSpeedos
    ,vector<NoiseInfo> *pInfo
    ,bool bPollExperiment /*default = true*/
)
{
    RC rc;

    vector<IsmChainFilter> chainFilter;
    // LW_ISM_NMEAS_lite, LW_ISM_NMEAS_lite_v2, LW_ISM_NMEAS_lite_v3 are mutually exclusive.
    // The others are not and can exist on the same chain.
    CHECK_RC(GetISMChains(&chainFilter, LW_ISM_NMEAS_lite,    -1, -1, Ism::NORMAL_CHAIN));
    CHECK_RC(GetISMChains(&chainFilter, LW_ISM_NMEAS_lite_v2, -1, -1, Ism::NORMAL_CHAIN));
    CHECK_RC(GetISMChains(&chainFilter, LW_ISM_NMEAS_lite_v3, -1, -1, Ism::NORMAL_CHAIN));
    CHECK_RC(GetISMChains(&chainFilter, LW_ISM_NMEAS_lite_v4, -1, -1, Ism::NORMAL_CHAIN));
    CHECK_RC(GetISMChains(&chainFilter, LW_ISM_NMEAS_lite_v5, -1, -1, Ism::NORMAL_CHAIN));
    CHECK_RC(GetISMChains(&chainFilter, LW_ISM_NMEAS_lite_v6, -1, -1, Ism::NORMAL_CHAIN));
    CHECK_RC(GetISMChains(&chainFilter, LW_ISM_NMEAS_lite_v7, -1, -1, Ism::NORMAL_CHAIN));
    CHECK_RC(GetISMChains(&chainFilter, LW_ISM_NMEAS_lite_v8, -1, -1, Ism::NORMAL_CHAIN));
    CHECK_RC(GetISMChains(&chainFilter, LW_ISM_NMEAS_lite_v9, -1, -1, Ism::NORMAL_CHAIN));

    // Make sure the experiment has completed before reading any of the chains. See note above
    if (bPollExperiment)
    {
        UINT32 complete = 0;
        CHECK_RC(PollISMCtrlComplete(&complete));
        if (!complete)
            return RC::ISM_EXPERIMENT_INCOMPLETE;

        // Note: we are going to do the experiment check in this function so tell the subsequent
        // API not to do the check.
        bPollExperiment = false;
    }

    for (UINT32 idx = 0; idx < chainFilter.size(); idx++)
    {
        CHECK_RC(ReadNoiseMeasLiteOnChain(
            chainFilter[idx].pChain->Hdr.InstrId,
            chainFilter[idx].pChain->Hdr.Chiplet,
            pSpeedos,
            pInfo,
            bPollExperiment));
    }

    return rc;
}

//------------------------------------------------------------------------------
// Reads the current NMEAS_lite ISM counts on one chain.
// Note: When the experiment has been setup with LoopMode=true the experiment will never complete.
//       However, the caller can halt the experiment via SetupISMController() to read the current
//       chain counts. To do the the caller must do the following:
//       a) Halt the experiment via SetupISMController(..., haltMode=true)
//       b) Call this API with bPollExperiment = false
// If bPollExperiment = true the routine will read the complete status from the last cached ISM
// controller. This could be either the PRIMARY controller or one of the chains with the 
// sub-controllers. It depends on call sequence from the user.
RC GpuIsm::ReadNoiseMeasLiteOnChain
(
    INT32 chainId
    ,INT32 chiplet
    ,vector<UINT32> *pSpeedos
    ,vector<NoiseInfo> *pInfo
    ,bool bPollExperiment
)
{
    RC rc;
    vector<IsmChainFilter> chainFilter;

    // Should we make sure the experiment is complete
    if (bPollExperiment)
    {
        UINT32 complete = 0;
        CHECK_RC(PollISMCtrlComplete(&complete));
        if (!complete)
            return RC::ISM_EXPERIMENT_INCOMPLETE;
    }

    // Get the Jtag chain for given chainId, chiplet, & ismType
    CHECK_RC(GetISMChains(&chainFilter, LW_ISM_NMEAS_lite,    chainId, chiplet, Ism::NORMAL_CHAIN)); //$
    CHECK_RC(GetISMChains(&chainFilter, LW_ISM_NMEAS_lite_v2, chainId, chiplet, Ism::NORMAL_CHAIN)); //$
    CHECK_RC(GetISMChains(&chainFilter, LW_ISM_NMEAS_lite_v3, chainId, chiplet, Ism::NORMAL_CHAIN)); //$
    CHECK_RC(GetISMChains(&chainFilter, LW_ISM_NMEAS_lite_v4, chainId, chiplet, Ism::NORMAL_CHAIN)); //$
    CHECK_RC(GetISMChains(&chainFilter, LW_ISM_NMEAS_lite_v5, chainId, chiplet, Ism::NORMAL_CHAIN)); //$
    CHECK_RC(GetISMChains(&chainFilter, LW_ISM_NMEAS_lite_v6, chainId, chiplet, Ism::NORMAL_CHAIN)); //$
    CHECK_RC(GetISMChains(&chainFilter, LW_ISM_NMEAS_lite_v7, chainId, chiplet, Ism::NORMAL_CHAIN)); //$
    CHECK_RC(GetISMChains(&chainFilter, LW_ISM_NMEAS_lite_v8, chainId, chiplet, Ism::NORMAL_CHAIN)); //$
    CHECK_RC(GetISMChains(&chainFilter, LW_ISM_NMEAS_lite_v9, chainId, chiplet, Ism::NORMAL_CHAIN)); //$

    // If not found the chainId is probably wrong
    if (chainFilter.empty())
        return RC::BAD_PARAMETER;

    // ChainId/Chiplet should be unique
    MASSERT(chainFilter.size() == 1);
    const IsmChain * pChain = chainFilter[0].pChain;

    vector <UINT32> jtagArray((pChain->Hdr.Size + 31) / 32, 0);
    CHECK_RC(ReadHost2Jtag(pChain->Hdr.Chiplet,
        pChain->Hdr.InstrId,
        pChain->Hdr.Size,
        &jtagArray));

    CHECK_RC(GetNoiseChainCounts(pChain, jtagArray, pSpeedos, pInfo));

    // Note: After reading the counts the user is responsible for powering down the chains.
    // See http://lwbugs/200468839 comments 20-23
    return (rc);
}

//------------------------------------------------------------------------------
// Get the raw counts from the JTagArray of bits for the given chain and push
// them into the pValues and pInfo vector.
RC GpuIsm::GetNoiseChainCounts
(
    const Ism::IsmChain *pChain     // the chain configuration
    , vector<UINT32> &jtagArray     // vector of raw bits for this chain
    , vector<UINT32> *pValues       // where to put the counts
    , vector<NoiseInfo> *pInfo      // vector to hold the macro specific data
)
{
    RegHal &regs = GetGpuSub()->Regs();
    RC rc;
    vector<UINT64> ismBits;
    for (size_t idx = 0; idx < pChain->ISMs.size(); idx++)
    {
        ismBits.resize(CallwlateVectorSize(pChain->ISMs[idx].Type, 64), 0);
        // Load the bits for a single macro into ismBits[]
        CHECK_RC(Utility::CopyBits(jtagArray,
            ismBits,
            pChain->ISMs[idx].Offset,
            0, // dstOffset
            GetISMSize(pChain->ISMs[idx].Type)));

        NoiseInfo ni = { 0 };
        UINT32 count = 0;
        switch (pChain->ISMs[idx].Type)
        { 
            case LW_ISM_NMEAS_lite:
                count = (UINT32)regs.GetField64(ismBits[0], MODS_ISM_NMEAS_LITE_V1_COUNT);
                pValues->push_back(count);
    
                // Load up the info vector as well
                ni.version = 4;
                ni.niLite.chainId     = pChain->Hdr.InstrId;
                ni.niLite.chiplet     = pChain->Hdr.Chiplet;
                ni.niLite.startBit    = pChain->ISMs[idx].Offset;
                ni.niLite.trim        = (INT32)regs.GetField64(ismBits[0], MODS_ISM_NMEAS_LITE_V1_NM_TRIM);         //$
                ni.niLite.tap         = (INT32)regs.GetField64(ismBits[0], MODS_ISM_NMEAS_LITE_V1_NM_TAP);          //$
                ni.niLite.iddq        = (INT32)regs.GetField64(ismBits[0], MODS_ISM_NMEAS_LITE_V1_IDDQ);            //$
                ni.niLite.mode        = (INT32)regs.GetField64(ismBits[0], MODS_ISM_NMEAS_LITE_V1_MODE);            //$
                ni.niLite.sel         = (INT32)regs.GetField64(ismBits[0], MODS_ISM_NMEAS_LITE_V1_SEL);             //$
                ni.niLite.div         = (INT32)regs.GetField64(ismBits[0], MODS_ISM_NMEAS_LITE_V1_DIV);             //$
                ni.niLite.divClkIn    = (INT32)regs.GetField64(ismBits[0], MODS_ISM_NMEAS_LITE_V1_DIV_CLK_IN);      //$
                ni.niLite.fixedDlySel = (INT32)regs.GetField64(ismBits[0], MODS_ISM_NMEAS_LITE_V1_FIXED_DLY_SEL);   //$
                ni.niLite.count       = (INT32)regs.GetField64(ismBits[0], MODS_ISM_NMEAS_LITE_V1_COUNT);           //$
                pInfo->push_back(ni);
                break;

            case LW_ISM_NMEAS_lite_v2:
                count = (UINT32)regs.GetField64(ismBits[0], MODS_ISM_NMEAS_LITE_V2_COUNT);
                pValues->push_back(count);

                // Load up the info vector as well
                ni.version = 5;
                ni.niLite.chainId     = pChain->Hdr.InstrId;
                ni.niLite.chiplet     = pChain->Hdr.Chiplet;
                ni.niLite.startBit    = pChain->ISMs[idx].Offset;
                ni.niLite.trim        = (INT32)regs.GetField64(ismBits[0], MODS_ISM_NMEAS_LITE_V2_NM_TRIM);       //$
                ni.niLite.tap         = (INT32)regs.GetField64(ismBits[0], MODS_ISM_NMEAS_LITE_V2_NM_TAP);        //$
                ni.niLite.iddq        = (INT32)regs.GetField64(ismBits[0], MODS_ISM_NMEAS_LITE_V2_IDDQ);          //$
                ni.niLite.mode        = (INT32)regs.GetField64(ismBits[0], MODS_ISM_NMEAS_LITE_V2_MODE);          //$
                ni.niLite.sel         = (INT32)regs.GetField64(ismBits[0], MODS_ISM_NMEAS_LITE_V2_SEL);           //$
                ni.niLite.div         = (INT32)regs.GetField64(ismBits[0], MODS_ISM_NMEAS_LITE_V2_DIV);           //$
                ni.niLite.divClkIn    = (INT32)regs.GetField64(ismBits[0], MODS_ISM_NMEAS_LITE_V2_DIV_CLK_IN);    //$
                ni.niLite.fixedDlySel = (INT32)regs.GetField64(ismBits[0], MODS_ISM_NMEAS_LITE_V2_FIXED_DLY_SEL); //$
                ni.niLite.refClkSel   = (INT32)regs.GetField64(ismBits[0], MODS_ISM_NMEAS_LITE_V2_REFCLK_SEL);    //$
                ni.niLite.count       = (INT32)regs.GetField64(ismBits[0], MODS_ISM_NMEAS_LITE_V2_COUNT);         //$
                pInfo->push_back(ni);
                break;

            case LW_ISM_NMEAS_lite_v3:
                {
                UINT64 count64 =
                    regs.GetField64(ismBits[2], MODS_ISM_NMEAS_LITE_V3_COUNT_HI) <<
                    regs.LookupMaskSize(MODS_ISM_NMEAS_LITE_V3_COUNT_LOW) |
                    regs.GetField64(ismBits[1], MODS_ISM_NMEAS_LITE_V3_COUNT_LOW);
                pValues->push_back((UINT32)(count64 & 0xffffffff));
                pValues->push_back((UINT32)(count64 >> 32));

                // Load up the info vector as well
                ni.version = 6;
                ni.niLite3.chainId      = pChain->Hdr.InstrId;
                ni.niLite3.chiplet      = pChain->Hdr.Chiplet;
                ni.niLite3.startBit     = pChain->ISMs[idx].Offset;
                ni.niLite3.trim        = (INT32)regs.GetField64(ismBits[0], MODS_ISM_NMEAS_LITE_V3_TRIM);                  //$
                ni.niLite3.tap         = (INT32)regs.GetField64(ismBits[0], MODS_ISM_NMEAS_LITE_V3_TAP_SEL);               //$     
                ni.niLite3.iddq        = (INT32)regs.GetField64(ismBits[0], MODS_ISM_NMEAS_LITE_V3_IDDQ);                  //$
                ni.niLite3.mode        = (INT32)regs.GetField64(ismBits[0], MODS_ISM_NMEAS_LITE_V3_MODE);                  //$
                ni.niLite3.clkSel      = (INT32)regs.GetField64(ismBits[0], MODS_ISM_NMEAS_LITE_V3_CLK_SEL);               //$
                ni.niLite3.outSel      = (INT32)regs.GetField64(ismBits[0], MODS_ISM_NMEAS_LITE_V3_OUT_SEL);               //$
                ni.niLite3.div         = (INT32)regs.GetField64(ismBits[0], MODS_ISM_NMEAS_LITE_V3_DIV);                   //$
                ni.niLite3.divClkIn    = (INT32)regs.GetField64(ismBits[0], MODS_ISM_NMEAS_LITE_V3_DIV_CLK_IN);            //$
                ni.niLite3.fixedDlySel = (INT32)regs.GetField64(ismBits[0], MODS_ISM_NMEAS_LITE_V3_FIXED_DLY_SEL);         //$
                ni.niLite3.refClkSel   = (INT32)regs.GetField64(ismBits[0], MODS_ISM_NMEAS_LITE_V3_REFCLK_SEL);            //$
                ni.niLite3.jtagOvr     = (INT32)regs.GetField64(ismBits[0], MODS_ISM_NMEAS_LITE_V3_JTAG_OVERRIDE);         //$
                ni.niLite3.winLen      = (INT32)regs.GetField64(ismBits[0], MODS_ISM_NMEAS_LITE_V3_WINDOW_LENGTH);         //$
                ni.niLite3.cntrEn      = (INT32)regs.GetField64(ismBits[1], MODS_ISM_NMEAS_LITE_V3_COUNTER_ENABLE);        //$
                ni.niLite3.cntrClr     = (INT32)regs.GetField64(ismBits[1], MODS_ISM_NMEAS_LITE_V3_COUNTER_CLEAR);         //$
                ni.niLite3.cntrDir     = (INT32)regs.GetField64(ismBits[1], MODS_ISM_NMEAS_LITE_V3_COUNTER_DIRECTION);     //$
                ni.niLite3.cntrToRead  = (INT32)regs.GetField64(ismBits[1], MODS_ISM_NMEAS_LITE_V3_COUNTER_TO_READ);       //$
                ni.niLite3.cntrOffset  = (INT32)regs.GetField64(ismBits[1], MODS_ISM_NMEAS_LITE_V3_COUNTER_OFFSET);        //$
                ni.niLite3.errEn       = (INT32)regs.GetField64(ismBits[1], MODS_ISM_NMEAS_LITE_V3_ERROR_ENABLE);          //$
                ni.niLite3.spareIn     = (INT32)regs.GetField64(ismBits[1], MODS_ISM_NMEAS_LITE_V3_SPARE_IN);              //$
                ni.niLite3.thresh      = (INT32)regs.GetField64(ismBits[1], MODS_ISM_NMEAS_LITE_V3_THRESHOLD);             //$
                ni.niLite3.mnMxWinEn   = (INT32)regs.GetField64(ismBits[1], MODS_ISM_NMEAS_LITE_V3_MNMX_WIN_ENABLE);       //$
                ni.niLite3.mnMxWinDur  = (INT32)regs.GetField64(ismBits[1], MODS_ISM_NMEAS_LITE_V3_MNMX_WIN_DURATION);     //$
                ni.niLite3.threshDir   = (INT32)regs.GetField64(ismBits[1], MODS_ISM_NMEAS_LITE_V3_THRESHOLD_DIRECTION);   //$
                ni.niLite3.roEn        = (INT32)regs.GetField64(ismBits[1], MODS_ISM_NMEAS_LITE_V3_RO_ENABLE);             //$
                // Do not attempt to put the LOW/HIGH registers directly into countLow/countHi.
                // COUNT_HI is 36 bits and will lose precision, instead just extract the low/Hi
                // 32bits from count64
                ni.niLite3.countLow    = (UINT32)(count64 & 0xffffffffULL);
                ni.niLite3.countHi     = (UINT32)((count64 >> 32) & 0xffffffffULL);
                ni.niLite3.ready       = (INT32)regs.GetField64(ismBits[2], MODS_ISM_NMEAS_LITE_V3_READY);                 //$
                ni.niLite3.errOut      = (INT32)regs.GetField64(ismBits[2], MODS_ISM_NMEAS_LITE_V3_ERROR_OUT);             //$
                ni.niLite3.spareOut    = (INT32)regs.GetField64(ismBits[2], MODS_ISM_NMEAS_LITE_V3_SPARE_OUT);             //$
                ni.niLite3.count       = count64;
                pInfo->push_back(ni);
                }
                break;

            case LW_ISM_NMEAS_lite_v4:
                {
                UINT64 count64 =
                    regs.GetField64(ismBits[2], MODS_ISM_NMEAS_LITE_V4_COUNT_HI) <<
                    regs.LookupMaskSize(MODS_ISM_NMEAS_LITE_V4_COUNT_LOW) |
                    regs.GetField64(ismBits[1], MODS_ISM_NMEAS_LITE_V4_COUNT_LOW);
                pValues->push_back((UINT32)(count64 & 0xffffffff));
                pValues->push_back((UINT32)(count64 >> 32));

                // Load up the info vector as well
                ni.version = 7;
                ni.niLite4.chainId      = pChain->Hdr.InstrId;
                ni.niLite4.chiplet      = pChain->Hdr.Chiplet;
                ni.niLite4.startBit     = pChain->ISMs[idx].Offset;
                ni.niLite4.trim        = (INT32)regs.GetField64(ismBits[0], MODS_ISM_NMEAS_LITE_V4_TRIM);                  //$
                ni.niLite4.tap         = (INT32)regs.GetField64(ismBits[0], MODS_ISM_NMEAS_LITE_V4_TAP_SEL);               //$     
                ni.niLite4.iddq        = (INT32)regs.GetField64(ismBits[0], MODS_ISM_NMEAS_LITE_V4_IDDQ);                  //$
                ni.niLite4.mode        = (INT32)regs.GetField64(ismBits[0], MODS_ISM_NMEAS_LITE_V4_MODE);                  //$
                ni.niLite4.clkSel      = (INT32)regs.GetField64(ismBits[0], MODS_ISM_NMEAS_LITE_V4_CLK_SEL);               //$
                ni.niLite4.outSel      = (INT32)regs.GetField64(ismBits[0], MODS_ISM_NMEAS_LITE_V4_OUT_SEL);               //$
                ni.niLite4.div         = (INT32)regs.GetField64(ismBits[0], MODS_ISM_NMEAS_LITE_V4_DIV);                   //$
                ni.niLite4.divClkIn    = (INT32)regs.GetField64(ismBits[0], MODS_ISM_NMEAS_LITE_V4_DIV_CLK_IN);            //$
                ni.niLite4.fixedDlySel = (INT32)regs.GetField64(ismBits[0], MODS_ISM_NMEAS_LITE_V4_FIXED_DLY_SEL);         //$
                ni.niLite4.refClkSel   = (INT32)regs.GetField64(ismBits[0], MODS_ISM_NMEAS_LITE_V4_REFCLK_SEL);            //$
                ni.niLite4.jtagOvr     = (INT32)regs.GetField64(ismBits[0], MODS_ISM_NMEAS_LITE_V4_JTAG_OVERRIDE);         //$
                ni.niLite4.winLen      = (INT32)regs.GetField64(ismBits[0], MODS_ISM_NMEAS_LITE_V4_WINDOW_LENGTH);         //$
                ni.niLite4.cntrEn      = (INT32)regs.GetField64(ismBits[0], MODS_ISM_NMEAS_LITE_V4_COUNTER_ENABLE);        //$
                ni.niLite4.cntrClr     = (INT32)regs.GetField64(ismBits[1], MODS_ISM_NMEAS_LITE_V4_COUNTER_CLEAR);         //$
                ni.niLite4.cntrDir     = (INT32)regs.GetField64(ismBits[1], MODS_ISM_NMEAS_LITE_V4_COUNTER_DIRECTION);     //$
                ni.niLite4.cntrToRead  = (INT32)regs.GetField64(ismBits[1], MODS_ISM_NMEAS_LITE_V4_COUNTER_TO_READ);       //$
                ni.niLite4.cntrOffset  = (INT32)regs.GetField64(ismBits[1], MODS_ISM_NMEAS_LITE_V4_COUNTER_OFFSET);        //$
                ni.niLite4.errEn       = (INT32)regs.GetField64(ismBits[1], MODS_ISM_NMEAS_LITE_V4_ERROR_ENABLE);          //$
                ni.niLite4.spareIn     = (INT32)regs.GetField64(ismBits[1], MODS_ISM_NMEAS_LITE_V4_SPARE_IN);              //$
                ni.niLite4.thresh      = (INT32)regs.GetField64(ismBits[1], MODS_ISM_NMEAS_LITE_V4_THRESHOLD);             //$
                ni.niLite4.threshDir   = (INT32)regs.GetField64(ismBits[1], MODS_ISM_NMEAS_LITE_V4_THRESHOLD_DIRECTION);   //$
                ni.niLite4.roEn        = (INT32)regs.GetField64(ismBits[1], MODS_ISM_NMEAS_LITE_V4_RO_ENABLE);             //$
                // Do not attempt to put the LOW/HIGH registers directly into countLow/countHi.
                // COUNT_LO is 39 bits and will lose precision, instead just extract the low/Hi
                // 32bits from count64
                ni.niLite4.countLow    = (UINT32)(count64 & 0xffffffffULL);
                ni.niLite4.countHi     = (UINT32)((count64 >> 32) & 0xffffffffULL);
                ni.niLite4.ready       = (INT32)regs.GetField64(ismBits[2], MODS_ISM_NMEAS_LITE_V4_READY);                 //$
                ni.niLite4.errOut      = (INT32)regs.GetField64(ismBits[2], MODS_ISM_NMEAS_LITE_V4_ERROR_OUT);             //$
                ni.niLite4.spareOut    = (INT32)regs.GetField64(ismBits[2], MODS_ISM_NMEAS_LITE_V4_SPARE_OUT);             //$
                ni.niLite4.count       = count64;
                pInfo->push_back(ni);
                }
                break;

            case LW_ISM_NMEAS_lite_v5:
                {
                UINT64 count64 =
                    regs.GetField64(ismBits[2], MODS_ISM_NMEAS_LITE_V5_COUNT_HI) <<
                    regs.LookupMaskSize(MODS_ISM_NMEAS_LITE_V5_COUNT_LOW) |
                    regs.GetField64(ismBits[1], MODS_ISM_NMEAS_LITE_V5_COUNT_LOW);
                pValues->push_back((UINT32)(count64 & 0xffffffff));
                pValues->push_back((UINT32)(count64 >> 32));

                // Load up the info vector as well
                ni.version = 7;
                ni.niLite4.chainId      = pChain->Hdr.InstrId;
                ni.niLite4.chiplet      = pChain->Hdr.Chiplet;
                ni.niLite4.startBit     = pChain->ISMs[idx].Offset;
                ni.niLite4.trim        = (INT32)regs.GetField64(ismBits[0], MODS_ISM_NMEAS_LITE_V5_TRIM);                  //$
                ni.niLite4.tap         = (INT32)regs.GetField64(ismBits[0], MODS_ISM_NMEAS_LITE_V5_TAP_SEL);               //$     
                ni.niLite4.iddq        = (INT32)regs.GetField64(ismBits[0], MODS_ISM_NMEAS_LITE_V5_IDDQ);                  //$
                ni.niLite4.mode        = (INT32)regs.GetField64(ismBits[0], MODS_ISM_NMEAS_LITE_V5_MODE);                  //$
                ni.niLite4.clkSel      = (INT32)regs.GetField64(ismBits[0], MODS_ISM_NMEAS_LITE_V5_CLK_SEL);               //$
                ni.niLite4.outSel      = (INT32)regs.GetField64(ismBits[0], MODS_ISM_NMEAS_LITE_V5_OUT_SEL);               //$
                ni.niLite4.div         = (INT32)regs.GetField64(ismBits[0], MODS_ISM_NMEAS_LITE_V5_DIV);                   //$
                ni.niLite4.divClkIn    = (INT32)regs.GetField64(ismBits[0], MODS_ISM_NMEAS_LITE_V5_DIV_CLK_IN);            //$
                ni.niLite4.fixedDlySel = (INT32)regs.GetField64(ismBits[0], MODS_ISM_NMEAS_LITE_V5_FIXED_DLY_SEL);         //$
                ni.niLite4.refClkSel   = (INT32)regs.GetField64(ismBits[0], MODS_ISM_NMEAS_LITE_V5_REFCLK_SEL);            //$
                ni.niLite4.jtagOvr     = (INT32)regs.GetField64(ismBits[0], MODS_ISM_NMEAS_LITE_V5_JTAG_OVERRIDE);         //$
                ni.niLite4.winLen      = (INT32)regs.GetField64(ismBits[0], MODS_ISM_NMEAS_LITE_V5_WINDOW_LENGTH);         //$
                ni.niLite4.cntrEn      = (INT32)regs.GetField64(ismBits[0], MODS_ISM_NMEAS_LITE_V5_COUNTER_ENABLE);        //$
                ni.niLite4.cntrClr     = (INT32)regs.GetField64(ismBits[1], MODS_ISM_NMEAS_LITE_V5_COUNTER_CLEAR);         //$
                ni.niLite4.cntrDir     = (INT32)regs.GetField64(ismBits[1], MODS_ISM_NMEAS_LITE_V5_COUNTER_DIRECTION);     //$
                ni.niLite4.cntrToRead  = (INT32)regs.GetField64(ismBits[1], MODS_ISM_NMEAS_LITE_V5_COUNTER_TO_READ);       //$
                ni.niLite4.cntrOffset  = (INT32)regs.GetField64(ismBits[1], MODS_ISM_NMEAS_LITE_V5_COUNTER_OFFSET);        //$
                ni.niLite4.errEn       = (INT32)regs.GetField64(ismBits[1], MODS_ISM_NMEAS_LITE_V5_ERROR_ENABLE);          //$
                ni.niLite4.spareIn     = (INT32)regs.GetField64(ismBits[1], MODS_ISM_NMEAS_LITE_V5_SPARE_IN);              //$
                ni.niLite4.thresh      = (INT32)regs.GetField64(ismBits[1], MODS_ISM_NMEAS_LITE_V5_THRESHOLD);             //$
                ni.niLite4.threshDir   = (INT32)regs.GetField64(ismBits[1], MODS_ISM_NMEAS_LITE_V5_THRESHOLD_DIRECTION);   //$
                ni.niLite4.roEn        = (INT32)regs.GetField64(ismBits[1], MODS_ISM_NMEAS_LITE_V5_RO_ENABLE);             //$
                ni.niLite4.fabRoEn     = (INT32)regs.GetField64(ismBits[1], MODS_ISM_NMEAS_LITE_V5_FABRO_ENABLE);          //$
                ni.niLite4.fabRoSBcast = (INT32)regs.GetField64(ismBits[1], MODS_ISM_NMEAS_LITE_V5_FABRO_S_BCAST);         //$
                ni.niLite4.fabRoSwPol  = (INT32)regs.GetField64(ismBits[1], MODS_ISM_NMEAS_LITE_V5_FABRO_SW_POLARITY);     //$
                ni.niLite4.fabRoIdxBcast = (INT32)regs.GetField64(ismBits[1], MODS_ISM_NMEAS_LITE_V5_FABRO_IDX_BCAST);     //$
                ni.niLite4.fabRoSel      = (INT32)regs.GetField64(ismBits[1], MODS_ISM_NMEAS_LITE_V5_FABRO_RO_SEL);        //$
                ni.niLite4.fabRoSpareIn  = (INT32)regs.GetField64(ismBits[1], MODS_ISM_NMEAS_LITE_V5_FABRO_SPARE_IN);      //$
                // Do not attempt to put the LOW/HIGH registers directly into countLow/countHi.
                // COUNT_HI is 43 bits and will lose precision, instead just extract the low/Hi
                // 32bits from count64
                ni.niLite4.countLow    = (UINT32)(count64 & 0xffffffffULL);
                ni.niLite4.countHi     = (UINT32)((count64 >> 32) & 0xffffffffULL);
                ni.niLite4.ready       = (INT32)regs.GetField64(ismBits[2], MODS_ISM_NMEAS_LITE_V5_READY);                 //$
                ni.niLite4.errOut      = (INT32)regs.GetField64(ismBits[2], MODS_ISM_NMEAS_LITE_V5_ERROR_OUT);             //$
                ni.niLite4.spareOut    = (INT32)regs.GetField64(ismBits[2], MODS_ISM_NMEAS_LITE_V5_SPARE_OUT);             //$
                ni.niLite4.count       = count64;
                pInfo->push_back(ni);
                }
                break;

            case LW_ISM_NMEAS_lite_v6:
                {
                UINT64 count64 =
                    regs.GetField64(ismBits[1], MODS_ISM_NMEAS_LITE_V6_COUNT_HI) <<
                    regs.LookupMaskSize(MODS_ISM_NMEAS_LITE_V6_COUNT_LOW) |
                    regs.GetField64(ismBits[0], MODS_ISM_NMEAS_LITE_V6_COUNT_LOW);
                pValues->push_back((UINT32)(count64 & 0xffffffff));
                pValues->push_back((UINT32)(count64 >> 32));

                // Load up the info vector as well
                ni.version = 7;
                ni.niLite4.chainId      = pChain->Hdr.InstrId;
                ni.niLite4.chiplet      = pChain->Hdr.Chiplet;
                ni.niLite4.startBit     = pChain->ISMs[idx].Offset;
                ni.niLite4.trim        = (INT32)regs.GetField64(ismBits[0], MODS_ISM_NMEAS_LITE_V6_TRIM);                  //$
                ni.niLite4.tap         = (INT32)regs.GetField64(ismBits[0], MODS_ISM_NMEAS_LITE_V6_TAP_SEL);               //$     
                ni.niLite4.iddq        = (INT32)regs.GetField64(ismBits[0], MODS_ISM_NMEAS_LITE_V6_IDDQ);                  //$
                ni.niLite4.mode        = (INT32)regs.GetField64(ismBits[0], MODS_ISM_NMEAS_LITE_V6_MODE);                  //$
                ni.niLite4.clkSel      = (INT32)regs.GetField64(ismBits[0], MODS_ISM_NMEAS_LITE_V6_CLK_SEL);               //$
                ni.niLite4.outSel      = (INT32)regs.GetField64(ismBits[0], MODS_ISM_NMEAS_LITE_V6_OUT_SEL);               //$
                ni.niLite4.div         = (INT32)regs.GetField64(ismBits[0], MODS_ISM_NMEAS_LITE_V6_DIV);                   //$
                ni.niLite4.divClkIn    = (INT32)regs.GetField64(ismBits[0], MODS_ISM_NMEAS_LITE_V6_DIV_CLK_IN);            //$
                ni.niLite4.fixedDlySel = (INT32)regs.GetField64(ismBits[0], MODS_ISM_NMEAS_LITE_V6_FIXED_DLY_SEL);         //$
                ni.niLite4.refClkSel   = (INT32)regs.GetField64(ismBits[0], MODS_ISM_NMEAS_LITE_V6_REFCLK_SEL);            //$
                ni.niLite4.jtagOvr     = (INT32)regs.GetField64(ismBits[0], MODS_ISM_NMEAS_LITE_V6_JTAG_OVERRIDE);         //$
                ni.niLite4.errEn       = (INT32)regs.GetField64(ismBits[0], MODS_ISM_NMEAS_LITE_V6_ERROR_ENABLE);          //$
                ni.niLite4.spareIn     = (INT32)regs.GetField64(ismBits[0], MODS_ISM_NMEAS_LITE_V6_SPARE_IN);              //$
                ni.niLite4.thresh      = (INT32)regs.GetField64(ismBits[0], MODS_ISM_NMEAS_LITE_V6_THRESHOLD);             //$
                ni.niLite4.threshDir   = (INT32)regs.GetField64(ismBits[0], MODS_ISM_NMEAS_LITE_V6_THRESHOLD_DIRECTION);   //$
                ni.niLite4.roEn        = (INT32)regs.GetField64(ismBits[0], MODS_ISM_NMEAS_LITE_V6_RO_ENABLE);             //$
                ni.niLite4.streamSel   = (INT32)regs.GetField64(ismBits[0], MODS_ISM_NMEAS_LITE_V6_STREAM_SEL);            //$
                ni.niLite4.streamMode  = (INT32)regs.GetField64(ismBits[0], MODS_ISM_NMEAS_LITE_V6_STREAMING_MODE);        //$
                ni.niLite4.selStreamTrigSrc  = (INT32)regs.GetField64(ismBits[0], MODS_ISM_NMEAS_LITE_V6_SEL_STREAMING_TRIGGER_SRC);//$
                ni.niLite4.samplePeriod      = (INT32)regs.GetField64(ismBits[0], MODS_ISM_NMEAS_LITE_V6_SAMPLING_PERIOD);          //$
                ni.niLite4.streamTrigSrc     = (INT32)regs.GetField64(ismBits[0], MODS_ISM_NMEAS_LITE_V6_STREAMING_TRIGGER_SRC);    //$

                // Do not attempt to put the LOW/HIGH registers directly into countLow/countHi.
                // COUNT_HI is 61 bits and will lose precision, instead just extract the low/Hi
                // 32bits from count64
                ni.niLite4.countLow    = (UINT32)(count64 & 0xffffffffULL);
                ni.niLite4.countHi     = (UINT32)((count64 >> 32) & 0xffffffffULL);
                ni.niLite4.ready       = (INT32)regs.GetField64(ismBits[1], MODS_ISM_NMEAS_LITE_V6_READY);            //$
                ni.niLite4.errOut      = (INT32)regs.GetField64(ismBits[1], MODS_ISM_NMEAS_LITE_V6_ERROR_OUT);        //$
                ni.niLite4.spareOut    = (INT32)(regs.GetField64(ismBits[2], MODS_ISM_NMEAS_LITE_V6_SPARE_OUT_HI) <<  //$
                                          regs.LookupMaskSize(MODS_ISM_NMEAS_LITE_V6_SPARE_OUT_LOW) |                 //$
                                          regs.GetField64(ismBits[1], MODS_ISM_NMEAS_LITE_V6_SPARE_OUT_LOW));         //$
                ni.niLite4.count       = count64;
                pInfo->push_back(ni);
                }
                break;

            case LW_ISM_NMEAS_lite_v7:
                {
                UINT64 count64 =
                    regs.GetField64(ismBits[1], MODS_ISM_NMEAS_LITE_V7_COUNT_HI) <<
                    regs.LookupMaskSize(MODS_ISM_NMEAS_LITE_V7_COUNT_LOW) |
                    regs.GetField64(ismBits[0], MODS_ISM_NMEAS_LITE_V7_COUNT_LOW);
                pValues->push_back((UINT32)(count64 & 0xffffffff));
                pValues->push_back((UINT32)(count64 >> 32));

                // Load up the info vector as well
                ni.version = 7;
                ni.niLite4.chainId      = pChain->Hdr.InstrId;
                ni.niLite4.chiplet      = pChain->Hdr.Chiplet;
                ni.niLite4.startBit     = pChain->ISMs[idx].Offset;
                ni.niLite4.trim        = (INT32)regs.GetField64(ismBits[0], MODS_ISM_NMEAS_LITE_V7_TRIM);                  //$
                ni.niLite4.tap         = (INT32)regs.GetField64(ismBits[0], MODS_ISM_NMEAS_LITE_V7_TAP_SEL);               //$     
                ni.niLite4.iddq        = (INT32)regs.GetField64(ismBits[0], MODS_ISM_NMEAS_LITE_V7_IDDQ);                  //$
                ni.niLite4.mode        = (INT32)regs.GetField64(ismBits[0], MODS_ISM_NMEAS_LITE_V7_MODE);                  //$
                ni.niLite4.clkSel      = (INT32)regs.GetField64(ismBits[0], MODS_ISM_NMEAS_LITE_V7_CLK_SEL);               //$
                ni.niLite4.outSel      = (INT32)regs.GetField64(ismBits[0], MODS_ISM_NMEAS_LITE_V7_OUT_SEL);               //$
                ni.niLite4.div         = (INT32)regs.GetField64(ismBits[0], MODS_ISM_NMEAS_LITE_V7_DIV);                   //$
                ni.niLite4.divClkIn    = (INT32)regs.GetField64(ismBits[0], MODS_ISM_NMEAS_LITE_V7_DIV_CLK_IN);            //$
                ni.niLite4.fixedDlySel = (INT32)regs.GetField64(ismBits[0], MODS_ISM_NMEAS_LITE_V7_FIXED_DLY_SEL);         //$
                ni.niLite4.refClkSel   = (INT32)regs.GetField64(ismBits[0], MODS_ISM_NMEAS_LITE_V7_REFCLK_SEL);            //$
                ni.niLite4.jtagOvr     = (INT32)regs.GetField64(ismBits[0], MODS_ISM_NMEAS_LITE_V7_JTAG_OVERRIDE);         //$
                ni.niLite4.errEn       = (INT32)regs.GetField64(ismBits[0], MODS_ISM_NMEAS_LITE_V7_ERROR_ENABLE);          //$
                ni.niLite4.spareIn     = (INT32)regs.GetField64(ismBits[0], MODS_ISM_NMEAS_LITE_V7_SPARE_IN);              //$
                ni.niLite4.thresh      = (INT32)regs.GetField64(ismBits[0], MODS_ISM_NMEAS_LITE_V7_THRESHOLD);             //$
                ni.niLite4.threshDir   = (INT32)regs.GetField64(ismBits[0], MODS_ISM_NMEAS_LITE_V7_THRESHOLD_DIRECTION);   //$
                ni.niLite4.roEn        = (INT32)regs.GetField64(ismBits[0], MODS_ISM_NMEAS_LITE_V7_RO_ENABLE);             //$
                ni.niLite4.streamSel   = (INT32)regs.GetField64(ismBits[0], MODS_ISM_NMEAS_LITE_V7_STREAM_SEL);            //$
                ni.niLite4.streamMode  = (INT32)regs.GetField64(ismBits[0], MODS_ISM_NMEAS_LITE_V7_STREAMING_MODE);        //$
                ni.niLite4.selStreamTrigSrc  = (INT32)regs.GetField64(ismBits[0], MODS_ISM_NMEAS_LITE_V7_SEL_STREAMING_TRIGGER_SRC);//$
                ni.niLite4.samplePeriod      = (INT32)regs.GetField64(ismBits[0], MODS_ISM_NMEAS_LITE_V7_SAMPLING_PERIOD);          //$
                ni.niLite4.streamTrigSrc     = (INT32)regs.GetField64(ismBits[0], MODS_ISM_NMEAS_LITE_V7_STREAMING_TRIGGER_SRC);    //$
                ni.niLite4.readyBypass       = (INT32)regs.GetField64(ismBits[0], MODS_ISM_NMEAS_LITE_V7_READY_BYPASS);    //$

                // Do not attempt to put the LOW/HIGH registers directly into countLow/countHi.
                // COUNT_HI is 61 bits and will lose precision, instead just extract the low/Hi
                // 32bits from count64
                ni.niLite4.countLow    = (UINT32)(count64 & 0xffffffffULL);
                ni.niLite4.countHi     = (UINT32)((count64 >> 32) & 0xffffffffULL);
                ni.niLite4.ready       = (INT32)regs.GetField64(ismBits[1], MODS_ISM_NMEAS_LITE_V7_READY);      //$
                ni.niLite4.errOut      = (INT32)regs.GetField64(ismBits[1], MODS_ISM_NMEAS_LITE_V7_ERROR_OUT);  //$
                ni.niLite4.spareOut    = (INT32)regs.GetField64(ismBits[2], MODS_ISM_NMEAS_LITE_V7_SPARE_OUT);  //$
                ni.niLite4.count       = count64;
                pInfo->push_back(ni);
                }
                break;

            case LW_ISM_NMEAS_lite_v8:
                {
                UINT64 count64 =
                    regs.GetField64(ismBits[2], MODS_ISM_NMEAS_LITE_V8_COUNT_HI) <<
                    regs.LookupMaskSize(MODS_ISM_NMEAS_LITE_V8_COUNT_LOW) |
                    regs.GetField64(ismBits[1], MODS_ISM_NMEAS_LITE_V8_COUNT_LOW);
                pValues->push_back((UINT32)(count64 & 0xffffffff));
                pValues->push_back((UINT32)(count64 >> 32));

                // Load up the info vector as well
                ni.version = 8;
                ni.niLite8.chainId         = pChain->Hdr.InstrId;
                ni.niLite8.chiplet         = pChain->Hdr.Chiplet;
                ni.niLite8.startBit        = pChain->ISMs[idx].Offset;
                ni.niLite8.iddq           = (INT32)regs.GetField64(ismBits[0], MODS_ISM_NMEAS_LITE_V8_IDDQ);                     //$
                ni.niLite8.mode           = (INT32)regs.GetField64(ismBits[0], MODS_ISM_NMEAS_LITE_V8_MODE);                     //$
                ni.niLite8.outSel         = (INT32)regs.GetField64(ismBits[0], MODS_ISM_NMEAS_LITE_V8_OUT_SEL);                  //$
                ni.niLite8.clkSel         = (INT32)regs.GetField64(ismBits[0], MODS_ISM_NMEAS_LITE_V8_REFCLK_SEL);               //$
                ni.niLite8.div            = (INT32)regs.GetField64(ismBits[0], MODS_ISM_NMEAS_LITE_V8_DIV);                      //$
                ni.niLite8.clkPhaseIlw    = (INT32)regs.GetField64(ismBits[0], MODS_ISM_NMEAS_LITE_V8_CLK_PHASE_ILWERT);         //$
                ni.niLite8.divClkIn       = (INT32)regs.GetField64(ismBits[0], MODS_ISM_NMEAS_LITE_V8_DIV_CLK_IN);               //$
                ni.niLite8.trimInitA      = (INT32)regs.GetField64(ismBits[0], MODS_ISM_NMEAS_LITE_V8_TRIM_INIT_A);              //$
                ni.niLite8.trimInitB      = (INT32)regs.GetField64(ismBits[0], MODS_ISM_NMEAS_LITE_V8_TRIM_INIT_B);              //$
                ni.niLite8.trimTypeSel    = (INT32)regs.GetField64(ismBits[0], MODS_ISM_NMEAS_LITE_V8_TRIM_TYPE_SEL);            //$
                ni.niLite8.tapTypeSel     = (INT32)regs.GetField64(ismBits[0], MODS_ISM_NMEAS_LITE_V8_TAP_TYPE_SEL);             //$
                ni.niLite8.trimInitC      = (INT32)regs.GetField64(ismBits[0], MODS_ISM_NMEAS_LITE_V8_TRIM_INIT_C);              //$
                ni.niLite8.fixedDlySel    = (INT32)regs.GetField64(ismBits[0], MODS_ISM_NMEAS_LITE_V8_FIXED_DELAY_SEL);          //$
                ni.niLite8.tapSel         = (INT32)regs.GetField64(ismBits[0], MODS_ISM_NMEAS_LITE_V8_TAP_SEL);                  //$
                ni.niLite8.jtagOvr        = (INT32)regs.GetField64(ismBits[0], MODS_ISM_NMEAS_LITE_V8_JTAG_OVERRIDE);            //$
                ni.niLite8.threshMax      = (INT32)regs.GetField64(ismBits[0], MODS_ISM_NMEAS_LITE_V8_THRESHOLD_MAX);            //$
                ni.niLite8.threshMin      = (INT32)regs.GetField64(ismBits[0], MODS_ISM_NMEAS_LITE_V8_THRESHOLD_MIN);            //$
                ni.niLite8.threshEn       = (INT32)regs.GetField64(ismBits[1], MODS_ISM_NMEAS_LITE_V8_THRESHOLD_ENABLE);         //$
                ni.niLite8.spareIn        = (INT32)regs.GetField64(ismBits[1], MODS_ISM_NMEAS_LITE_V8_SPARE_IN);                 //$
                ni.niLite8.cntrEn         = (INT32)regs.GetField64(ismBits[1], MODS_ISM_NMEAS_LITE_V8_COUNTER_ENABLE);           //$
                ni.niLite8.winLen         = (INT32)regs.GetField64(ismBits[1], MODS_ISM_NMEAS_LITE_V8_WINDOW_LENGTH);            //$
                ni.niLite8.cntrClr        = (INT32)regs.GetField64(ismBits[1], MODS_ISM_NMEAS_LITE_V8_COUNTER_CLEAR);            //$
                ni.niLite8.cntrOffsetDir  = (INT32)regs.GetField64(ismBits[1], MODS_ISM_NMEAS_LITE_V8_COUNTER_OFFSET_DIRECTION); //$
                ni.niLite8.cntrToRead     = (INT32)regs.GetField64(ismBits[1], MODS_ISM_NMEAS_LITE_V8_COUNTER_TO_READ);          //$
                ni.niLite8.cntrTrimOffset = (INT32)regs.GetField64(ismBits[1], MODS_ISM_NMEAS_LITE_V8_COUNTER_TRIM_OFFSET);      //$
                ni.niLite8.ready          = (INT32)regs.GetField64(ismBits[2], MODS_ISM_NMEAS_LITE_V8_READY);                    //$
                ni.niLite8.threshOut      = (INT32)regs.GetField64(ismBits[2], MODS_ISM_NMEAS_LITE_V8_THRESHOLD_OUT);            //$
                ni.niLite8.threshMaxOut   = (INT32)regs.GetField64(ismBits[2], MODS_ISM_NMEAS_LITE_V8_THRESHOLD_MAX_OUT);        //$
                ni.niLite8.threshMinOut   = (INT32)regs.GetField64(ismBits[2], MODS_ISM_NMEAS_LITE_V8_THRESHOLD_MIN_OUT);        //$
                ni.niLite8.spareOut       = (INT32)regs.GetField64(ismBits[2], MODS_ISM_NMEAS_LITE_V8_SPARE_OUT);                //$

                // _COUNT is only 41 bits so its safe to put the count value into the countLow & countHi fields //$
                ni.niLite8.countLow    = (UINT32)(count64 & 0xffffffffULL);
                ni.niLite8.countHi     = (UINT32)((count64 >> 32) & 0xffffffffULL);
                ni.niLite8.count       = count64;
                pInfo->push_back(ni);
                }
                break;

            case LW_ISM_NMEAS_lite_v9:
                {
                UINT64 count64 = regs.GetField64(ismBits[1], MODS_ISM_NMEAS_LITE_V9_COUNT);
                pValues->push_back((UINT32)(count64 & 0xffffffff));
                pValues->push_back((UINT32)(count64 >> 32));

                // Load up the info vector as well
                ni.version = 9;
                ni.niLite8.chainId          = pChain->Hdr.InstrId;
                ni.niLite8.chiplet          = pChain->Hdr.Chiplet;
                ni.niLite8.startBit         = pChain->ISMs[idx].Offset;
                ni.niLite8.iddq             = (INT32)regs.GetField64(ismBits[0], MODS_ISM_NMEAS_LITE_V9_IDDQ);                      //$
                ni.niLite8.mode             = (INT32)regs.GetField64(ismBits[0], MODS_ISM_NMEAS_LITE_V9_MODE);                      //$
                ni.niLite8.outSel           = (INT32)regs.GetField64(ismBits[0], MODS_ISM_NMEAS_LITE_V9_OUT_SEL);                   //$
                ni.niLite8.clkSel           = (INT32)regs.GetField64(ismBits[0], MODS_ISM_NMEAS_LITE_V9_REFCLK_SEL);                //$
                ni.niLite8.div              = (INT32)regs.GetField64(ismBits[0], MODS_ISM_NMEAS_LITE_V9_DIV);                       //$
                ni.niLite8.clkPhaseIlw      = (INT32)regs.GetField64(ismBits[0], MODS_ISM_NMEAS_LITE_V9_CLK_PHASE_ILWERT);          //$
                ni.niLite8.divClkIn         = (INT32)regs.GetField64(ismBits[0], MODS_ISM_NMEAS_LITE_V9_DIV_CLK_IN);                //$
                ni.niLite8.trimInitA        = (INT32)regs.GetField64(ismBits[0], MODS_ISM_NMEAS_LITE_V9_TRIM_INIT_A);               //$
                ni.niLite8.trimInitB        = (INT32)regs.GetField64(ismBits[0], MODS_ISM_NMEAS_LITE_V9_TRIM_INIT_B);               //$
                ni.niLite8.trimTypeSel      = (INT32)regs.GetField64(ismBits[0], MODS_ISM_NMEAS_LITE_V9_TRIM_TYPE_SEL);             //$
                ni.niLite8.tapTypeSel       = (INT32)regs.GetField64(ismBits[0], MODS_ISM_NMEAS_LITE_V9_TAP_TYPE_SEL);              //$
                ni.niLite8.trimInitC        = (INT32)regs.GetField64(ismBits[0], MODS_ISM_NMEAS_LITE_V9_TRIM_INIT_C);               //$
                ni.niLite8.fixedDlySel      = (INT32)regs.GetField64(ismBits[0], MODS_ISM_NMEAS_LITE_V9_FIXED_DELAY_SEL);           //$
                ni.niLite8.tapSel           = (INT32)regs.GetField64(ismBits[0], MODS_ISM_NMEAS_LITE_V9_TAP_SEL);                   //$
                ni.niLite8.jtagOvr          = (INT32)regs.GetField64(ismBits[0], MODS_ISM_NMEAS_LITE_V9_JTAG_OVERRIDE);             //$
                ni.niLite8.threshMax        = (INT32)regs.GetField64(ismBits[0], MODS_ISM_NMEAS_LITE_V9_THRESHOLD_MAX);             //$
                ni.niLite8.threshMin        = (INT32)regs.GetField64(ismBits[0], MODS_ISM_NMEAS_LITE_V9_THRESHOLD_MIN);             //$
                ni.niLite8.threshEn         = (INT32)regs.GetField64(ismBits[1], MODS_ISM_NMEAS_LITE_V9_THRESHOLD_ENABLE);          //$
                ni.niLite8.spareIn          = (INT32)regs.GetField64(ismBits[1], MODS_ISM_NMEAS_LITE_V9_SPARE_IN);                  //$
                ni.niLite8.streamSel        = (INT32)regs.GetField64(ismBits[1], MODS_ISM_NMEAS_LITE_V9_STREAM_SEL);                //$
                ni.niLite8.streamMode       = (INT32)regs.GetField64(ismBits[1], MODS_ISM_NMEAS_LITE_V9_STREAMING_MODE);            //$
                ni.niLite8.selStreamTrigSrc = (INT32)regs.GetField64(ismBits[1], MODS_ISM_NMEAS_LITE_V9_SEL_STREAMING_TRIGGER_SRC); //$
                ni.niLite8.samplePeriod     = (INT32)regs.GetField64(ismBits[1], MODS_ISM_NMEAS_LITE_V9_SAMPLING_PERIOD);           //$
                ni.niLite8.streamTrigSrc    = (INT32)regs.GetField64(ismBits[1], MODS_ISM_NMEAS_LITE_V9_STREAMING_TRIGGER_SRC);     //$
                ni.niLite8.readyBypass      = (INT32)regs.GetField64(ismBits[1], MODS_ISM_NMEAS_LITE_V9_READY_BYPASS);              //$
                ni.niLite8.count            =        regs.GetField64(ismBits[1], MODS_ISM_NMEAS_LITE_V9_COUNT);                     //$
                ni.niLite8.ready            = (INT32)regs.GetField64(ismBits[1], MODS_ISM_NMEAS_LITE_V9_READY);                     //$
                ni.niLite8.threshOut        = (INT32)regs.GetField64(ismBits[1], MODS_ISM_NMEAS_LITE_V9_THRESHOLD_OUT);             //$
                ni.niLite8.threshMaxOut     = (INT32)regs.GetField64(ismBits[2], MODS_ISM_NMEAS_LITE_V9_THRESHOLD_MAX_OUT);         //$
                ni.niLite8.threshMinOut     = (INT32)regs.GetField64(ismBits[2], MODS_ISM_NMEAS_LITE_V9_THRESHOLD_MIN_OUT);         //$
                ni.niLite8.spareOut         = (INT32)regs.GetField64(ismBits[2], MODS_ISM_NMEAS_LITE_V9_SPARE_OUT);                 //$

                // _COUNT is only 41 bits so its safe to put the count value into the countLow & countHi fields //$
                ni.niLite8.countLow    = (UINT32)(count64 & 0xffffffffULL);
                ni.niLite8.countHi     = (UINT32)((count64 >> 32) & 0xffffffffULL);
                pInfo->push_back(ni);
                }
                break;
            case LW_ISM_NMEAS_v2:
                count = (UINT32)regs.GetField64(ismBits[0], MODS_ISM_NMEAS_CNT_OUT_LOW) |
                        (UINT32)(regs.GetField64(ismBits[1], MODS_ISM_NMEAS_CNT_OUT_HI) <<
                                 regs.LookupMaskSize(MODS_ISM_NMEAS_CNT_OUT_LOW));
                pValues->push_back(count);

                // load up the info vector as well
                ni.version = 2;
                ni.ni2.chainId  = pChain->Hdr.InstrId;
                ni.ni2.chiplet  = pChain->Hdr.Chiplet;
                ni.ni2.startBit = pChain->ISMs[idx].Offset;
                ni.ni2.tap      = (UINT32)regs.GetField64(ismBits[0], MODS_ISM_NMEAS_TAP);
                ni.ni2.mode     = (UINT32)regs.GetField64(ismBits[0], MODS_ISM_NMEAS_MODE);
                ni.ni2.outSel   = (UINT32)regs.GetField64(ismBits[0], MODS_ISM_NMEAS_OUT_SEL);
                ni.ni2.thresh   = (UINT32)regs.GetField64(ismBits[0], MODS_ISM_NMEAS_THRESHOLD);
                ni.ni2.trimInit = (UINT32)regs.GetField64(ismBits[0], MODS_ISM_NMEAS_TRIMINIT);
                ni.ni2.measLen  = (UINT32)regs.GetField64(ismBits[0], MODS_ISM_NMEAS_WIN_LEN);
                ni.ni2.count    = count;
                pInfo->push_back(ni);
                break;

            case LW_ISM_NMEAS_v3:
                count = (UINT32)regs.GetField64(ismBits[0], MODS_ISM_NMEAS3_CNT_OUT_LOW) |
                        (UINT32)(regs.GetField64(ismBits[1], MODS_ISM_NMEAS3_CNT_OUT_HI) <<
                                 regs.LookupMaskSize(MODS_ISM_NMEAS3_CNT_OUT_LOW));
                pValues->push_back(count);
                // load up the info vector as well
                ni.version = 3;
                ni.ni3.chainId      = pChain->Hdr.InstrId;
                ni.ni3.chiplet      = pChain->Hdr.Chiplet;
                ni.ni3.startBit     = pChain->ISMs[idx].Offset;
                ni.ni3.hmDiv        = (INT32)regs.GetField64(ismBits[0], MODS_ISM_NMEAS3_HM_DIV);            //$
                ni.ni3.hmFineDlySel = (INT32)regs.GetField64(ismBits[0], MODS_ISM_NMEAS3_HM_FINE_DELAY_SEL); //$
                ni.ni3.hmTapDlySel  = (INT32)regs.GetField64(ismBits[0], MODS_ISM_NMEAS3_HM_TAP_DELAY_SEL);  //$
                ni.ni3.hmTapSel     = (INT32)regs.GetField64(ismBits[0], MODS_ISM_NMEAS3_HM_TAP_SEL);        //$
                ni.ni3.lockPt       = (INT32)regs.GetField64(ismBits[0], MODS_ISM_NMEAS3_LOCK_POINT);        //$
                ni.ni3.mode         = (INT32)regs.GetField64(ismBits[0], MODS_ISM_NMEAS3_MODE);              //$
                ni.ni3.outSel       = (INT32)regs.GetField64(ismBits[0], MODS_ISM_NMEAS3_OUT_SEL);           //$
                ni.ni3.sat          = (INT32)regs.GetField64(ismBits[0], MODS_ISM_NMEAS3_SAT_ON);            //$
                ni.ni3.thresh       = (INT32)regs.GetField64(ismBits[0], MODS_ISM_NMEAS3_THRESHOLD);         //$
                ni.ni3.winLen       = (INT32)regs.GetField64(ismBits[0], MODS_ISM_NMEAS3_WIN_LEN);           //$
                ni.ni3.hmAvgWin     = (INT32)regs.GetField64(ismBits[0], MODS_ISM_NMEAS3_HM_AVG_WIN);        //$
                ni.ni3.fdUpdateOff  = (INT32)regs.GetField64(ismBits[0], MODS_ISM_NMEAS3_FD_UPDATE_OFF);     //$
                ni.ni3.count        = count;
                ni.ni3.hmOut        = (UINT32)regs.GetField64(ismBits[1], MODS_ISM_NMEAS3_HM_OUT);
                ni.ni3.rdy          = (UINT32)regs.GetField64(ismBits[1], MODS_ISM_NMEAS3_READY);
                pInfo->push_back(ni);
                break;

            default:
                // Hmm.. we have a chain with more that NMEAS cirlwits. That's unexpected but not
                // fatal.
                break;
        }
    }
    return rc;
}

//------------------------------------------------------------------------------------------------
// Return the longest required delay time in usecs for any ISM on this chain.
UINT32 GpuIsm::GetDelayTimeUs
(
    const IsmChain * pChain,
    UINT32 durClkCycles
)
{
    UINT32 temp = durClkCycles;
    UINT32 clkFreqKhz = 0;
    GetISMClkFreqKhz(&clkFreqKhz);
    MASSERT(clkFreqKhz != 0);
    float clkFreqMhz = clkFreqKhz / 1000.0f;
    if (GetGpuSub()->HasBug(2365890))
    {
        for (size_t i = 0; i < pChain->ISMs.size(); i++)
        {
            switch (pChain->ISMs[i].Type)
            { 
                // This ISM has some special time delay requirements. The ISM counter logic
                // doesn't have a reset port and will rely on the duration to reset the counter
                // value. This means that when the duration cycle counting is done, the counter
                // will reset from X to 0. After another duration cycle counting is done and the 
                // real counter value will be ready. We are adding a 3rd duration here for margin.
                case LW_ISM_TSOSC_a4:
                case LW_ISM_ROSC_bin_metal:
                case LW_ISM_ROSC_bin_aging_v1:
                case LW_ISM_ROSC_comp:
                    temp = max(durClkCycles * 3, temp);
                    break;
                default:
                    break;
            }
        }
    }
    return (UINT32)(temp / clkFreqMhz);
}

/*virtual*/
RC GpuIsm::SetupNoiseMeas //!< Setup all NMEAS2 & NMEAS3 macros to run an experiment.
(
    NoiseMeasParam param     //!< structure with setup information
)
{
    if ((param.version != 2) && (param.version != 3))
    {
        return RC::BAD_PARAMETER;
    }

    m_NoiseParam = param;
    return OK;
}


/*virtual*/
RC GpuIsm::RunNoiseMeas //!< run the noise measurement experiment on all chains
(
    Ism::NoiseMeasStage stageMask //!< bitmask of the experiment stages to perform.
    ,vector<UINT32>*pValues //!< vector to hold the raw counts
    ,vector<NoiseInfo>*pInfo //!< vector to hold the macro specific information
)
{
    RC rc;
    UINT32 numStages = 0;
    // Get the list of Jtag chains that have LW_ISM_NMEAS_v2 or LW_ISM_NMEAS_v3 macros
    // Note v2 & v3 chains are mutually exclusive and use a different number of stages to program.
    
    vector<IsmChainFilter> chainFilter;
    CHECK_RC(GetISMChains(&chainFilter, LW_ISM_NMEAS_v2, -1, -1, Ism::NORMAL_CHAIN));
    if (!chainFilter.empty())
    {
        numStages = (UINT32)Utility::CountBits(nmsSTAGE_ALL);
    }
    else
    {
        CHECK_RC(GetISMChains(&chainFilter, LW_ISM_NMEAS_v3, -1, -1, Ism::NORMAL_CHAIN));
        if (!chainFilter.empty())
        {
            numStages = 4;
        }
        else // Return failure if there are no NMEAS_v2 or NMEAS_v3 ISMs
        {
            return RC::BAD_PARAMETER;
        }
    }

    //We will perform each stage on all chains before moving to the next stage.
    for (UINT32 stage = 0; stage < numStages; stage++)
    {
        if (stageMask & (1<<stage))
        {
            for (size_t idx = 0; idx < chainFilter.size(); idx++)
            {
                CHECK_RC(RunNoiseMeasOnChain(
                    chainFilter[idx].pChain->Hdr.InstrId,
                    chainFilter[idx].pChain->Hdr.Chiplet,
                    (Ism::NoiseMeasStage)(stage<<1),
                    pValues,
                    pInfo));
            }
        }
    }
    return rc;
}
/*virtual*/
RC GpuIsm::RunNoiseMeasOnChain //!< run the noise measurement experiment on a specific chain
(
    INT32 chainId       //!< JTag instruction Id to use
    ,INT32 chiplet       //!< JTag chiplet to use
    ,Ism::NoiseMeasStage stage //!< bitmask of the experiment statges to perform
    ,vector<UINT32>*pValues //!< vector to hold the raw counts
    ,vector<NoiseInfo>*pInfo //!< vector to hold the macro specific information
)
{
    vector<IsmChainFilter> chainFilter;
    RC rc;
    // We pass in NMEAS because we want to make sure that there is only 1 chain using the chainId
    // and chiplet. This should return a single chain with one or more of the LW_ISM_NMEAS_* ISMs 
    CHECK_RC(GetChains(&chainFilter, NMEAS, chainId, chiplet, Ism::NORMAL_CHAIN));

    // if not found probably wrong chainId or chiplet
    if (chainFilter.empty())
        return RC::BAD_PARAMETER;

    // chain/chiplet must be unique
    MASSERT(chainFilter.size() == 1);
    const IsmChain * pChain = chainFilter[0].pChain;

    vector <UINT32> jtagArray((pChain->Hdr.Size+31)/32, 0);
    // Hardware ISM team has stated that NMEAS chains don't have any other types of ISMs on those
    // chains, so we can switch off the 1st ISM to determine what type of experiement to run.
    // If that ever changes (not likely) we will have to modify this algorithm.
    switch (pChain->ISMs[0].Type)
    { 
        case LW_ISM_NMEAS_v2:
            CHECK_RC(RunNoiseMeasV2OnChain(
                pChain,
                chainFilter[0].ismMask,
                jtagArray,
                m_NoiseParam,
                pValues,
                pInfo,
                stage));
            break;
        case LW_ISM_NMEAS_v3:
            CHECK_RC(RunNoiseMeasV3OnChain(
                pChain,
                chainFilter[0].ismMask,
                jtagArray,
                m_NoiseParam,
                pValues,
                pInfo,
                stage));
            break;
        case LW_ISM_NMEAS_lite:
        case LW_ISM_NMEAS_lite_v2:
            Printf(Tee::PriError, 
                   "RunNoiseMeasOnChain() is the wrong API to program NMEAS_lite ISMs\n"
                   "Check your values for \"chainId\" and \"chiplet\"\n");
            rc = RC::BAD_PARAMETER;
            break;
        case LW_ISM_NMEAS_v4: // ISM team has not given instructions on how to program this ISM.
            Printf(Tee::PriError, "MODS doesn't know how to program the LW_ISM_NMEAS4 ISM.\n");
            rc = RC::SOFTWARE_ERROR;
            break;
        default:
            Printf(Tee::PriError,
                   "MODS doesn't know how to program this NMEAS ISM of type:%d.\n",
                   pChain->ISMs[0].Type);
            rc = RC::SOFTWARE_ERROR;
            break;
    }

    return rc;
}

//------------------------------------------------------------------------------------------------
// Program the noise measurement circuit to take a sample of winLen duration and return the
// measured noise data.
// To setup the circuit we have to manually control the enable & meas lines as follows.
// There are actually 6 different stages of this process:
// Stage 1: Initialize the chain with mode and window length information
// Stage 2: Enable the circuit's power
// Stage 3: Enable the measurement window. This starts measuring the noise for WinLen duration.
// Stage 4: Disable the measurement window. This can happen anytime after WinLen duration
// Stage 5: Read the noise counts.
// Stage 6: Disable the noise circuit.
//
//         :      :_:____________:_:___________:_:__________:__: :
// en      :______| :            : :           : :          :  |_:___________________
//         :      : :            :_:___________: :          :  : :
// meas    :______:_:____________| :           |_:__________:__:_:____________________
//         :      : :            : :           : :          :  : :
// stage   :  1   :2:            :3:           :4:    5     :  :6:
//         :      :              :             : :<-rd cnt->:    :
//         :      :              :             :                 :
//         :      :<-assertion ->:<-  meas   ->:<-de-assertion ->:
RC GpuIsm::RunNoiseMeasV2OnChain
(
    const IsmChain *pChain      // the chain configuration
    ,const bitset<Ism::maxISMsPerChain>& ismMask
    ,vector<UINT32> &jtagArray  // Vector of raw bits for this chain.
    ,NoiseMeasParam noiseParm   // structure containing the experiment parameters
    ,vector<UINT32> *pValues     // vector of the raw counts
    ,vector<NoiseInfo> *pInfo   // vector of macro specific information
    ,UINT32 stageMask           // a bitmask of what stages should be performed
)
{
    RC rc;
    // initialize the array
    jtagArray.assign(jtagArray.size(), 0);
    // This ISM macro is >64 bits so 2 args are needed.
    vector<UINT64> ismBits(CallwlateVectorSize(LW_ISM_NMEAS_v2, 64), 0);

    NoiseMeas2 info = { 0 };   // the actual _NMEAS macro

    // Stage 1 Initialize the circuit
    info.trimInit       = noiseParm.nm2.trimInit;
    info.mode           = noiseParm.nm2.mode;
    info.outSel         = noiseParm.nm2.outSel;
    info.tap            = noiseParm.nm2.tap;
    info.winLen         = noiseParm.nm2.measLen;
    info.thresh         = noiseParm.nm2.thresh;
    info.bEnable        = false;
    info.bJtagOverwrite = true;

    if (stageMask & nmsSTAGE_1)
    {
        CHECK_RC(ConfigureNoiseBits(ismBits, info));
        CHECK_RC(CopyBits(pChain, jtagArray, ismBits, ismMask));
        CHECK_RC(WriteHost2Jtag(pChain->Hdr.Chiplet,
                                pChain->Hdr.InstrId,
                                pChain->Hdr.Size,
                                jtagArray));
        Platform::DelayNS(200);
    }
    // Stage 2 Enable the circuit
    info.bEnable = true;
    if (stageMask & nmsSTAGE_2)
    {
        CHECK_RC(ConfigureNoiseBits(ismBits, info));
        CHECK_RC(CopyBits(pChain, jtagArray, ismBits, ismMask));
        CHECK_RC(WriteHost2Jtag(pChain->Hdr.Chiplet,
                                pChain->Hdr.InstrId,
                                pChain->Hdr.Size,
                                jtagArray));
        Platform::DelayNS(200);
    }
    // Stage 3 Enable the measurement window
    info.bMeas = true;
    if (stageMask & nmsSTAGE_3)
    {
        CHECK_RC(ConfigureNoiseBits(ismBits, info));
        CHECK_RC(CopyBits(pChain, jtagArray, ismBits, ismMask));
        CHECK_RC(WriteHost2Jtag(pChain->Hdr.Chiplet,
                                pChain->Hdr.InstrId,
                                pChain->Hdr.Size,
                                jtagArray));
        UINT32 nmeasClkKHz=0;
        CHECK_RC(GetNoiseMeasClk(&nmeasClkKHz));
        //TODO: double check your math.
        UINT32 delayNs = noiseParm.nm2.measLen * (1000000/nmeasClkKHz);
        // enough time to measure
        Platform::DelayNS(delayNs);
    }

    // Stage 4 Disable the measurement window
    info.bMeas = false;
    if (stageMask & nmsSTAGE_4)
    {
        CHECK_RC(ConfigureNoiseBits(ismBits, info));
        CHECK_RC(CopyBits(pChain, jtagArray, ismBits, ismMask));
        CHECK_RC(WriteHost2Jtag(pChain->Hdr.Chiplet,
                                pChain->Hdr.InstrId,
                                pChain->Hdr.Size,
                                jtagArray));
        Platform::DelayNS(200);
    }

    // We can't get both count vales and soft wrapper values in the same loop becase we
    // need to disable the jtag chain between the access. So store the data into a temporary
    // vector until its complete.
    vector<NoiseInfo> tmpInfo;

    // Stage 5 read the data
    if (stageMask & nmsSTAGE_5)
    {
        CHECK_RC(ReadHost2Jtag(pChain->Hdr.Chiplet,
                                pChain->Hdr.InstrId,
                                pChain->Hdr.Size,
                                &jtagArray));

        // We don't want to loose the counts when we disable the circuit.
        if ((pValues != nullptr) && (pInfo != nullptr))
        {
            CHECK_RC(GetNoiseChainCounts(pChain, jtagArray, pValues, &tmpInfo));
        }
    }

    // Stage 6 Disable the circuit and read the wrapper using priv access
    info.bEnable = false;
    info.bJtagOverwrite = false;
    if (stageMask & nmsSTAGE_6)
    {
        RegHal &regs = GetGpuSub()->Regs();

        CHECK_RC(ConfigureNoiseBits(ismBits, info));
        CHECK_RC(CopyBits(pChain, jtagArray, ismBits, ismMask));
        CHECK_RC(WriteHost2Jtag(pChain->Hdr.Chiplet,
                                pChain->Hdr.InstrId,
                                pChain->Hdr.Size,
                                jtagArray));
        if ((pValues != nullptr) && (pInfo != nullptr) &&
           (pChain->Hdr.VstCtlReg > 0) && (pChain->Hdr.VstDataReg > 0) &&
           (pChain->Hdr.SfeReg > 0))
        {
            for (size_t idx = 0; idx < pChain->ISMs.size(); idx++)
            {
                if (pChain->ISMs[idx].Type == LW_ISM_NMEAS_v2)
                {   // select the macro in the chain
                    GetGpuSub()->RegWr32(pChain->Hdr.SfeReg, static_cast<UINT32>(idx)); 
                    // select the type of data to read
                    GetGpuSub()->RegWr32(pChain->Hdr.VstCtlReg, 
                        regs.SetField(MODS_PGRAPH_PRI_GPC0_TPC0_SM_POWER_VST_CTL_INDEX, 9));
                    tmpInfo[idx].ni2.data = GetGpuSub()->RegRd32(pChain->Hdr.VstDataReg);
                }
            }
        }
    }

    // ok now copy the complete structures to the users vector.
    while (tmpInfo.begin() != tmpInfo.end())
    {
        MASSERT(pInfo != nullptr);
        pInfo->push_back(tmpInfo.front());
        tmpInfo.erase(tmpInfo.begin());
    }
    return rc;
}
//------------------------------------------------------------------------------------------------
// Configure the ismBits for the LW_ISM_NMEAS2 macro
//
RC GpuIsm::ConfigureNoiseBits
(
    vector<UINT64> &nBits
    ,NoiseMeas2  nMeas      //structure containing the new values
)
{
    RegHal &regs = GetGpuSub()->Regs();
    nBits[0] = (
               regs.SetField64(MODS_ISM_NMEAS_EN,             nMeas.bEnable)      |
               regs.SetField64(MODS_ISM_NMEAS_TAP,            nMeas.tap)          |
               regs.SetField64(MODS_ISM_NMEAS_JTAG_OVERWRITE, nMeas.bJtagOverwrite)|
               regs.SetField64(MODS_ISM_NMEAS_MEAS,           nMeas.bMeas)        |
               regs.SetField64(MODS_ISM_NMEAS_MODE,           nMeas.mode)         |
               regs.SetField64(MODS_ISM_NMEAS_OUT_SEL,        nMeas.outSel)       |
               regs.SetField64(MODS_ISM_NMEAS_OUTPUT_CLAMP,   nMeas.bOutClamp)    |
               regs.SetField64(MODS_ISM_NMEAS_THRESHOLD,      nMeas.thresh)       |
               regs.SetField64(MODS_ISM_NMEAS_TRIMINIT,       nMeas.trimInit)     |
               regs.SetField64(MODS_ISM_NMEAS_WIN_LEN,        nMeas.winLen)       |
               regs.SetField64(MODS_ISM_NMEAS_CNT_OUT_LOW,    nMeas.cnt)
                 );
    //noiseInfo.cnt has to span 2 64bit enities 17bits on low64Bits & 3 bits on high64Bits
    nBits[1] = (
                regs.SetField64(MODS_ISM_NMEAS_CNT_OUT_HI,
                    (nMeas.cnt >> regs.LookupMaskSize(MODS_ISM_NMEAS_CNT_OUT_LOW)))   |
                regs.SetField64(MODS_ISM_NMEAS_READY, nMeas.bRdy)
               );
    return OK;
}

//------------------------------------------------------------------------------------------------
// Program the noise measurement circuit to take a sample of winLen duration and return the
// measured noise data.
// To setup the circuit we have to manually control the enable line as follows.
// There are actually 4 different stages of this process:
// Stage 1: Initialize the chain with mode and window length information
// Stage 2: Enable the circuit and start the measurement fore WinLen duration
// Stage 3: Disable the circuit. This can happen anytime after WinLen duration
// Stage 4: Read the noise counts via Jtag.
//
//         :      :_:__________________________:_:          :
// en      :______| :                          : |__________:
//         :      : :                          : :          :
// stage   :  1   :2:                          :3:    4     :
//         :      :<- WinLen noise meas.     ->: :<-rd cnt->:
RC GpuIsm::RunNoiseMeasV3OnChain
(
    const IsmChain *pChain      // the chain configuration
    ,const bitset<Ism::maxISMsPerChain>& ismMask
    ,vector<UINT32> &jtagArray  // Vector of raw bits for this chain.
    ,NoiseMeasParam noiseParm   // structure containing the experiment parameters
    ,vector<UINT32> *pValues     // vector of the raw counts
    ,vector<NoiseInfo> *pInfo   // vector of macro specific information
    ,UINT32 stageMask           // a bitmask of what stages should be performed
)
{
    RC rc;
    // initialize the array
    jtagArray.assign(jtagArray.size(), 0);

    NoiseMeas3 info = { 0 };   // the actual _NMEAS3 macro

    // Stage 1 Initialize the circuit
    info.hmDiv          = noiseParm.nm3.hmDiv;
    info.hmFineDlySel   = noiseParm.nm3.hmFineDlySel;
    info.hmTapDlySel    = noiseParm.nm3.hmTapDlySel;
    info.hmTapSel       = noiseParm.nm3.hmTapSel;
    info.hmTrim         = noiseParm.nm3.hmTrim;
    info.lockPt         = noiseParm.nm3.lockPt;
    info.mode           = noiseParm.nm3.mode;
    info.outSel         = noiseParm.nm3.outSel;
    info.bOutClamp      = !!noiseParm.nm3.outClamp;
    info.bSat           = !!noiseParm.nm3.sat;
    info.thresh         = noiseParm.nm3.thresh;
    info.winLen         = noiseParm.nm3.winLen;
    info.hmAvgWin       = noiseParm.nm3.hmAvgWin;
    info.fdUpdateOff    = !!noiseParm.nm3.fdUpdateOff;

    info.bEnable        = false;
    info.bJtagOverwrite = true;

    vector<UINT64> ismBits(CallwlateVectorSize(LW_ISM_NMEAS_v3, 64), 0);
    if (stageMask & nmsSTAGE_1)
    {
        CHECK_RC(ConfigureNoiseBits(ismBits, info));
        CHECK_RC(CopyBits(pChain, jtagArray, ismBits, ismMask));
        CHECK_RC(WriteHost2Jtag(pChain->Hdr.Chiplet,
                                pChain->Hdr.InstrId,
                                pChain->Hdr.Size,
                                jtagArray));
        Platform::DelayNS(200);
    }
    // Stage 2 Enable the circuit & start the measurement
    info.bEnable = true;
    if (stageMask & nmsSTAGE_2)
    {
        CHECK_RC(ConfigureNoiseBits(ismBits, info));
        CHECK_RC(CopyBits(pChain, jtagArray, ismBits, ismMask));
        CHECK_RC(WriteHost2Jtag(pChain->Hdr.Chiplet,
                                pChain->Hdr.InstrId,
                                pChain->Hdr.Size,
                                jtagArray));
        UINT32 nmeasClkKHz=0;
        CHECK_RC(GetNoiseMeasClk(&nmeasClkKHz));
        UINT32 delayNs = noiseParm.nm3.winLen * (1000000/nmeasClkKHz);
        // wait enough time to measure
        Platform::DelayNS(delayNs);
    }

    // Stage 3 disable the circuit after measurement is complete
    info.bEnable = false;
    info.bJtagOverwrite = false;
    if (stageMask & nmsSTAGE_3)
    {
        CHECK_RC(ConfigureNoiseBits(ismBits, info));
        CHECK_RC(CopyBits(pChain, jtagArray, ismBits, ismMask));
        CHECK_RC(WriteHost2Jtag(pChain->Hdr.Chiplet,
                                pChain->Hdr.InstrId,
                                pChain->Hdr.Size,
                                jtagArray));
    }

    // Stage 4 read the data
    if (stageMask & nmsSTAGE_4)
    {
        CHECK_RC(ReadHost2Jtag(pChain->Hdr.Chiplet,
                                pChain->Hdr.InstrId,
                                pChain->Hdr.Size,
                                &jtagArray));

        // We don't want to loose the counts when we disable the circuit.
        if ((pValues != nullptr) && (pInfo != nullptr))
        {
            CHECK_RC(GetNoiseChainCounts(pChain, jtagArray, pValues, pInfo));
        }
    }
    return rc;
}

//------------------------------------------------------------------------------------------------
// Configure the ismBits for the LW_ISM_NMEAS3 macro
//
RC GpuIsm::ConfigureNoiseBits
(
    vector<UINT64> &nBits
    ,NoiseMeas3  nMeas      //structure containing the new values
)
{
    RegHal &regs = GetGpuSub()->Regs();
    nBits[0] = (
               regs.SetField64(MODS_ISM_NMEAS3_EN,                 nMeas.bEnable)      |
               regs.SetField64(MODS_ISM_NMEAS3_HM_DIV,             nMeas.hmDiv)        |
               regs.SetField64(MODS_ISM_NMEAS3_HM_FINE_DELAY_SEL, nMeas.hmFineDlySel)  |
               regs.SetField64(MODS_ISM_NMEAS3_HM_TAP_DELAY_SEL,  nMeas.hmTapDlySel)   |
               regs.SetField64(MODS_ISM_NMEAS3_HM_TAP_SEL,        nMeas.hmTapSel)      |
               regs.SetField64(MODS_ISM_NMEAS3_HM_TRIM,           nMeas.hmTrim)        |
               regs.SetField64(MODS_ISM_NMEAS3_JTAG_OVERWRITE,    nMeas.bJtagOverwrite)|
               regs.SetField64(MODS_ISM_NMEAS3_LOCK_POINT,        nMeas.lockPt)        |
               regs.SetField64(MODS_ISM_NMEAS3_MODE,              nMeas.mode)          |
               regs.SetField64(MODS_ISM_NMEAS3_OUT_SEL,           nMeas.outSel)        |
               regs.SetField64(MODS_ISM_NMEAS3_OUTPUT_CLAMP,      nMeas.bOutClamp)     |
               regs.SetField64(MODS_ISM_NMEAS3_SAT_ON,            nMeas.bSat)          |
               regs.SetField64(MODS_ISM_NMEAS3_THRESHOLD,         nMeas.thresh)        |
               regs.SetField64(MODS_ISM_NMEAS3_WIN_LEN,           nMeas.winLen)        |
               regs.SetField64(MODS_ISM_NMEAS3_HM_AVG_WIN,        nMeas.hmAvgWin)      |
               regs.SetField64(MODS_ISM_NMEAS3_FD_UPDATE_OFF,     nMeas.fdUpdateOff)   |
               regs.SetField64(MODS_ISM_NMEAS3_CNT_OUT_LOW,       nMeas.cnt)
               );
    //noiseInfo.cnt has to span 2 64bit enities 8bits on low64Bits & 8 bits on high64Bits
    nBits[1] = (
                regs.SetField64(MODS_ISM_NMEAS3_CNT_OUT_HI,
                    (nMeas.cnt >> regs.LookupMaskSize(MODS_ISM_NMEAS3_CNT_OUT_LOW)))|
                regs.SetField64(MODS_ISM_NMEAS3_HM_OUT, nMeas.hmOut)                |
                regs.SetField64(MODS_ISM_NMEAS3_READY, nMeas.bRdy)
               );
    return OK;
}

//==============================================================================
// These APIs should be implemented by a subclass because they are GPU specific.

//------------------------------------------------------------------------------
// Return the proper frequency for callwlating measurement time.
RC GpuIsm::GetNoiseMeasClk(UINT32 *pClkKhz)
{
    MASSERT(!"GetNoiseMeasClk() not implemented!");
    return RC::UNSUPPORTED_FUNCTION;
}

//------------------------------------------------------------------------------
// Apply any special processing to the outDiv field for the purpose of frequency 
// callwlation. Default case is just to return the existing outDiv value.
INT32 GpuIsm::GetISMOutDivForFreqCalc(PART_TYPES PartType, INT32 outDiv)
{
    return outDiv;
}

//------------------------------------------------------------------------------
// Provide the default implementation for most GPUs
INT32 GpuIsm::GetISMOutputDivider(PART_TYPES partType)
{
    switch (partType)
    {
        case MINI    : return 3;
        case COMP    : return 3;
        case BIN     : return 3;
        case METAL   : return 3;
        case IMON    : return 0;
        case VNSENSE : return 3;
        case TSOSC   : return 0;
        case AGING   : return 0;
        case AGING2  : return 0;
        default:
            MASSERT(!"GetISMOutputDivider(): Unknown Type!");
            return 0;
    }
}

//------------------------------------------------------------------------------
UINT32 GpuIsm::GetISMDurClkCycles()
{
    return 250;
}

//------------------------------------------------------------------------------
INT32 GpuIsm::GetOscIdx(PART_TYPES partType)
{
    switch (partType)
    {
        case MINI   : return 2;
        case COMP   : return 0;
        case BIN    : return 0;
        case METAL  : return 0;
        case AGING  : return 0;
        case AGING2 : return 0;
        case IMON   : return 0;
        default:
            MASSERT(!"GetOscIdx(): Unknown Type!");
            return 0;
    }
}

//------------------------------------------------------------------------------
INT32 GpuIsm::GetISMRefClkSel(PART_TYPES partType)
{
    if (partType != IsmSpeedo::MINI)
    {
        MASSERT(!"Unknown partType for GetRefClkSel(). Function not implemented!");
    }
    return 0;
}

// Create a NULL ISM for GPUs where ISM functionality is not supported
GpuIsm *CreateNullIsm(GpuSubdevice *pGpu)
{
    return NULL;
}

