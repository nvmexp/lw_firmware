/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2006-2015 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#include "maxwell_rtl_clocks.h"
#include "string.h"

// Needed for xclk gating
#include "lwmisc.h"

// elcg clock configuration start
static vector<maxwell_rtl_clock_struct> elcg_rtl_mapped_clocks;
// elcg configuration end

static LWGpuResource *m_pLwGpu = NULL;
void SetMaxwellClkGpuResource(LWGpuResource *pLwGpu)
{
        m_pLwGpu = pLwGpu;
}

unsigned NumMappedClocks_maxwell()
{
    return elcg_rtl_mapped_clocks.size();
}

maxwell_rtl_clock_struct GetRTLMappedClock_maxwell(unsigned i)
{
    return elcg_rtl_mapped_clocks[i];
}

void InitMappedClocks_maxwell (Macro macros,MAXWELL_CLOCK_GATING maxwell_clock_gating)
{
    LWGpuResource* lwgpu = LWGpuResource::FindFirstResource();
    UINT32 m_arch = lwgpu->GetArchitecture();
    if (macros.lw_scal_litter_num_ce_lces > 0 && macros.lw_scal_litter_num_ce_pces > 0)
        elcg_rtl_mapped_clocks.push_back({"pwr_test_clk_count_CE0_elcg", MAXWELL_CLOCK_SOURCE_SYSCLK, maxwell_clock_gating.MAXWELL_CLOCK_CE0_ENGINE_GATED, MAXWELL_CE0_FORCE_FULL_POWER});
if(m_arch != 3086) {
    if (macros.lw_scal_litter_num_ce_lces > 1 && macros.lw_scal_litter_num_ce_pces > 1)
        elcg_rtl_mapped_clocks.push_back({"pwr_test_clk_count_CE1_elcg", MAXWELL_CLOCK_SOURCE_SYSCLK, maxwell_clock_gating.MAXWELL_CLOCK_CE1_ENGINE_GATED, MAXWELL_CE1_FORCE_FULL_POWER});
}
    if (macros.lw_scal_litter_num_ce_lces > 2 && macros.lw_scal_litter_num_ce_pces > 2)
        elcg_rtl_mapped_clocks.push_back({"pwr_test_clk_count_CE2_elcg", MAXWELL_CLOCK_SOURCE_SYSCLK, maxwell_clock_gating.MAXWELL_CLOCK_CE2_ENGINE_GATED, MAXWELL_CE2_FORCE_FULL_POWER});

    if (macros.lw_scal_litter_num_ce_lces > 3 && macros.lw_scal_litter_num_ce_pces > 3)
        elcg_rtl_mapped_clocks.push_back({"pwr_test_clk_count_CE3_elcg", MAXWELL_CLOCK_SOURCE_SYSCLK, maxwell_clock_gating.MAXWELL_CLOCK_CE3_ENGINE_GATED, MAXWELL_CE3_FORCE_FULL_POWER});

    if (macros.lw_scal_litter_num_ce_lces > 4 && macros.lw_scal_litter_num_ce_pces > 4)
        elcg_rtl_mapped_clocks.push_back({"pwr_test_clk_count_CE4_elcg", MAXWELL_CLOCK_SOURCE_SYSCLK, maxwell_clock_gating.MAXWELL_CLOCK_CE4_ENGINE_GATED, MAXWELL_CE4_FORCE_FULL_POWER});

    if (macros.lw_scal_litter_num_ce_lces > 5 && macros.lw_scal_litter_num_ce_pces > 5)
        elcg_rtl_mapped_clocks.push_back({"pwr_test_clk_count_CE5_elcg", MAXWELL_CLOCK_SOURCE_SYSCLK, maxwell_clock_gating.MAXWELL_CLOCK_CE5_ENGINE_GATED, MAXWELL_CE5_FORCE_FULL_POWER});

    if (macros.lw_scal_litter_num_ce_lces > 6 && macros.lw_scal_litter_num_ce_pces > 6)
        elcg_rtl_mapped_clocks.push_back({"pwr_test_clk_count_CE6_elcg", MAXWELL_CLOCK_SOURCE_SYSCLK, maxwell_clock_gating.MAXWELL_CLOCK_CE6_ENGINE_GATED, MAXWELL_CE6_FORCE_FULL_POWER});

    if (macros.lw_scal_litter_num_ce_lces > 7 && macros.lw_scal_litter_num_ce_pces > 7)
       elcg_rtl_mapped_clocks.push_back({"pwr_test_clk_count_CE7_elcg", MAXWELL_CLOCK_SOURCE_SYSCLK, maxwell_clock_gating.MAXWELL_CLOCK_CE7_ENGINE_GATED, MAXWELL_CE7_FORCE_FULL_POWER});

    if (macros.lw_scal_litter_num_ce_lces > 8 && macros.lw_scal_litter_num_ce_pces > 8)
       elcg_rtl_mapped_clocks.push_back({"pwr_test_clk_count_CE8_elcg", MAXWELL_CLOCK_SOURCE_SYSCLK, maxwell_clock_gating.MAXWELL_CLOCK_CE8_ENGINE_GATED, MAXWELL_CE8_FORCE_FULL_POWER});

    if (macros.lw_scal_litter_num_ce_lces > 9 && macros.lw_scal_litter_num_ce_pces > 9)
       elcg_rtl_mapped_clocks.push_back({"pwr_test_clk_count_CE9_elcg", MAXWELL_CLOCK_SOURCE_SYSCLK, maxwell_clock_gating.MAXWELL_CLOCK_CE9_ENGINE_GATED, MAXWELL_CE9_FORCE_FULL_POWER});

    //
    elcg_rtl_mapped_clocks.push_back({"pwr_test_clk_count_GR_noelcg", MAXWELL_CLOCK_SOURCE_GPCCLK, 0, 0});
    elcg_rtl_mapped_clocks.push_back({"pwr_test_clk_count_GR_elcg", MAXWELL_CLOCK_SOURCE_GPCCLK, maxwell_clock_gating.MAXWELL_CLOCK_GR_ENGINE_GATED, MAXWELL_GR_FORCE_FULL_POWER});
    //
    if (macros.lw_chip_lwdec) {
        elcg_rtl_mapped_clocks.push_back({"pwr_test_clk_count_LWDEC_elcg", MAXWELL_CLOCK_SOURCE_LWDCLK, maxwell_clock_gating.MAXWELL_CLOCK_LWDEC_ENGINE_GATED, MAXWELL_LWDEC_FORCE_FULL_POWER});
        if (macros.lw_scal_chip_num_lwdecs > 1) 
            elcg_rtl_mapped_clocks.push_back({"pwr_test_clk_count_LWDEC1_elcg", MAXWELL_CLOCK_SOURCE_LWDCLK, maxwell_clock_gating.MAXWELL_CLOCK_LWDEC1_ENGINE_GATED, MAXWELL_LWDEC1_FORCE_FULL_POWER});
        if (macros.lw_scal_chip_num_lwdecs > 2)
            elcg_rtl_mapped_clocks.push_back({"pwr_test_clk_count_LWDEC2_elcg", MAXWELL_CLOCK_SOURCE_LWDCLK, maxwell_clock_gating.MAXWELL_CLOCK_LWDEC2_ENGINE_GATED, MAXWELL_LWDEC2_FORCE_FULL_POWER});
        if (macros.lw_scal_chip_num_lwdecs > 3)
            elcg_rtl_mapped_clocks.push_back({"pwr_test_clk_count_LWDEC3_elcg", MAXWELL_CLOCK_SOURCE_LWDCLK, maxwell_clock_gating.MAXWELL_CLOCK_LWDEC3_ENGINE_GATED, MAXWELL_LWDEC3_FORCE_FULL_POWER});
        if (macros.lw_scal_chip_num_lwdecs > 4)
            elcg_rtl_mapped_clocks.push_back({"pwr_test_clk_count_LWDEC4_elcg", MAXWELL_CLOCK_SOURCE_LWDCLK, maxwell_clock_gating.MAXWELL_CLOCK_LWDEC4_ENGINE_GATED, MAXWELL_LWDEC4_FORCE_FULL_POWER});
        if (macros.lw_scal_chip_num_lwdecs > 5)
            elcg_rtl_mapped_clocks.push_back({"pwr_test_clk_count_LWDEC5_elcg", MAXWELL_CLOCK_SOURCE_LWDCLK, maxwell_clock_gating.MAXWELL_CLOCK_LWDEC5_ENGINE_GATED, 0});
        if (macros.lw_scal_chip_num_lwdecs > 6)
            elcg_rtl_mapped_clocks.push_back({"pwr_test_clk_count_LWDEC6_elcg", MAXWELL_CLOCK_SOURCE_LWDCLK, maxwell_clock_gating.MAXWELL_CLOCK_LWDEC6_ENGINE_GATED, 0});
        if (macros.lw_scal_chip_num_lwdecs > 7)
            elcg_rtl_mapped_clocks.push_back({"pwr_test_clk_count_LWDEC7_elcg", MAXWELL_CLOCK_SOURCE_LWDCLK, maxwell_clock_gating.MAXWELL_CLOCK_LWDEC7_ENGINE_GATED, 0});

    }
    if(macros.lw_chip_lwjpg) {
        if (macros.lw_scal_litter_num_lwjpgs > 0){
            elcg_rtl_mapped_clocks.push_back({"pwr_test_clk_count_LWJPG0_elcg", MAXWELL_CLOCK_SOURCE_LWDCLK, maxwell_clock_gating.MAXWELL_CLOCK_LWJPG0_ENGINE_GATED, 0});
        }
        if (macros.lw_scal_litter_num_lwjpgs > 1){
            elcg_rtl_mapped_clocks.push_back({"pwr_test_clk_count_LWJPG1_elcg", MAXWELL_CLOCK_SOURCE_LWDCLK, maxwell_clock_gating.MAXWELL_CLOCK_LWJPG1_ENGINE_GATED, 0});
        }
        if (macros.lw_scal_litter_num_lwjpgs > 2){
            elcg_rtl_mapped_clocks.push_back({"pwr_test_clk_count_LWJPG2_elcg", MAXWELL_CLOCK_SOURCE_LWDCLK, maxwell_clock_gating.MAXWELL_CLOCK_LWJPG2_ENGINE_GATED, 0});
        }
        if (macros.lw_scal_litter_num_lwjpgs > 3){
            elcg_rtl_mapped_clocks.push_back({"pwr_test_clk_count_LWJPG3_elcg", MAXWELL_CLOCK_SOURCE_LWDCLK, maxwell_clock_gating.MAXWELL_CLOCK_LWJPG3_ENGINE_GATED, 0});
        }
        if (macros.lw_scal_litter_num_lwjpgs > 4){
            elcg_rtl_mapped_clocks.push_back({"pwr_test_clk_count_LWJPG4_elcg", MAXWELL_CLOCK_SOURCE_LWDCLK, maxwell_clock_gating.MAXWELL_CLOCK_LWJPG4_ENGINE_GATED, 0});
        }
        if (macros.lw_scal_litter_num_lwjpgs > 5){
            elcg_rtl_mapped_clocks.push_back({"pwr_test_clk_count_LWJPG5_elcg", MAXWELL_CLOCK_SOURCE_LWDCLK, maxwell_clock_gating.MAXWELL_CLOCK_LWJPG5_ENGINE_GATED, 0});
        }
        if (macros.lw_scal_litter_num_lwjpgs > 6){
            elcg_rtl_mapped_clocks.push_back({"pwr_test_clk_count_LWJPG6_elcg", MAXWELL_CLOCK_SOURCE_LWDCLK, maxwell_clock_gating.MAXWELL_CLOCK_LWJPG6_ENGINE_GATED, 0});
        }
        if (macros.lw_scal_litter_num_lwjpgs > 7){
            elcg_rtl_mapped_clocks.push_back({"pwr_test_clk_count_LWJPG7_elcg", MAXWELL_CLOCK_SOURCE_LWDCLK, maxwell_clock_gating.MAXWELL_CLOCK_LWJPG7_ENGINE_GATED, 0});
        }
    }
    //
    if (macros.lw_chip_ofa) {
        elcg_rtl_mapped_clocks.push_back({"pwr_test_clk_count_OFA_elcg", MAXWELL_CLOCK_SOURCE_LWDCLK, maxwell_clock_gating.MAXWELL_CLOCK_OFA_ENGINE_GATED, MAXWELL_OFA_FORCE_FULL_POWER});
    }
    if(macros.lw_chip_sec && !macros.lw_gpu_core_ip){
        elcg_rtl_mapped_clocks.push_back({"pwr_test_clk_count_SEC_elcg", MAXWELL_CLOCK_SOURCE_LWDCLK, 0, MAXWELL_SEC_FORCE_FULL_POWER});
}
    //
    if (macros.lw_chip_lwenc) {
        elcg_rtl_mapped_clocks.push_back({"pwr_test_clk_count_LWENC_msdclk_noelcg", MAXWELL_CLOCK_SOURCE_LWDCLK, 0, MAXWELL_LWENC_FORCE_FULL_POWER});
        elcg_rtl_mapped_clocks.push_back({"pwr_test_clk_count_LWENC_mseclk_elcg", MAXWELL_CLOCK_SOURCE_LWDCLK,  maxwell_clock_gating.MAXWELL_CLOCK_LWENC_ENGINE_GATED, MAXWELL_LWENC_FORCE_FULL_POWER});
        if (macros.lw_scal_chip_num_lwencs > 1) {
            elcg_rtl_mapped_clocks.push_back({"pwr_test_clk_count_LWENC1_msdclk_noelcg", MAXWELL_CLOCK_SOURCE_LWDCLK, 0, MAXWELL_LWENC1_FORCE_FULL_POWER});
            elcg_rtl_mapped_clocks.push_back({"pwr_test_clk_count_LWENC1_mseclk_elcg", MAXWELL_CLOCK_SOURCE_LWDCLK,  maxwell_clock_gating.MAXWELL_CLOCK_LWENC1_ENGINE_GATED, MAXWELL_LWENC1_FORCE_FULL_POWER});
        }
        if (macros.lw_scal_chip_num_lwencs > 2) {
            elcg_rtl_mapped_clocks.push_back({"pwr_test_clk_count_LWENC2_msdclk_noelcg", MAXWELL_CLOCK_SOURCE_LWDCLK, 0, MAXWELL_LWENC2_FORCE_FULL_POWER});
            elcg_rtl_mapped_clocks.push_back({"pwr_test_clk_count_LWENC2_mseclk_elcg", MAXWELL_CLOCK_SOURCE_LWDCLK,  maxwell_clock_gating.MAXWELL_CLOCK_LWENC2_ENGINE_GATED, MAXWELL_LWENC2_FORCE_FULL_POWER});
        }
    }
    //
    elcg_rtl_mapped_clocks.push_back({"pwr_test_clk_count_SYS_noelcg", MAXWELL_CLOCK_SOURCE_SYSCLK, 0, 0});
}
