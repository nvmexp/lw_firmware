 /* LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2006-2015,2018-2021 by LWPU Corporation.  All rights reserved. 
 * All information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */
/*
 * powerCtrlTest_maxwell -
 *   Testing of power controls features within the chip. In tesla2, most features are controlled by register POWERCTRL_* in host.
 * But in maxwell, most the POWERCTRL_* register are removed from host, and use GATE_CTRL in pwr/therm instead.
 * a) Idle slowdown, verify slow down of gpcclk/sysclk/legclk/ltcclk based on idle graphics engine.
 * b) Block-level clock gating, Test whether each block-level-gated clock can be gated by blcg when each unit run at idle
 * c) Priv wakeup, Verify that each blcg clk can wake up in time to respond to a PRI write request.
 * d) per engine clock management ,verify clock gating for each engine when its suspended & disabled
 * e) video engine override, verfy whether the clock gating of video engine can be overrided by elpg_vd when video is at idle
 *
 */
// To facilitate faster compiles via pre-compiled headers, keep "stdtest.h" first.

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Last owner:  Alan Zhai
// Description: Control engine in different status by setting different values to register LW_THERM_GATE_CTRL(i)
// Debug Advice:  1. Update engines in current project;
//                2. It is necessary to dump waveforms;
//                3. Check each engine's clock. Sometimes, some clock may be turned off in the file theProcessArgs.FMODEL,RTL in order to save sim time;
//                4. Add simTop.u_systop_misc.pwr_test_clk_mon_XXX_YYY(LW_CLK_diag_monitor) to waveform. It includes clock, counter, reset, enable and so on.
//                5. Add LW_THERM_GATE_CTRL(i) to confirm control operation.
// If you miss some question, please contact last owner for help.
// ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#include "mdiag/tests/stdtest.h"
#include "powerCtrlTest_maxwell.h"
#include "mdiag/lwgpu.h"
#include "mdiag/tests/test_state_report.h"
#include "mdiag/utils/lwgpu_channel.h"
#include "lwmisc.h"
#include "sim/IChip.h"

//Below needs to be updated to latest include file for each chip.
#include "pascal/gp104/dev_pri_ringstation_sys.h"
#include "pascal/gp104/dev_pri_ringstation_gpc.h"
#include "pascal/gp104/dev_pri_ringstation_fbp.h"

// In case some chip only has 1 TPC per GPC. Define other gpccs2tpccs interface's index.
#ifndef LW_PPRIV_GPC_PRI_MASTER_gpccs2tpccs_pri1
#define LW_PPRIV_GPC_PRI_MASTER_gpccs2tpccs_pri1    0
#endif
#ifndef LW_PPRIV_GPC_PRI_MASTER_gpccs2tpccs_pri2
#define LW_PPRIV_GPC_PRI_MASTER_gpccs2tpccs_pri2    0
#endif
#ifndef LW_PPRIV_GPC_PRI_MASTER_gpccs2tpccs_pri3
#define LW_PPRIV_GPC_PRI_MASTER_gpccs2tpccs_pri3    0
#endif
#ifndef LW_PPRIV_GPC_PRI_MASTER_gpccs2tpccs_pri4
#define LW_PPRIV_GPC_PRI_MASTER_gpccs2tpccs_pri4    0
#endif
#ifndef LW_PPRIV_GPC_PRI_MASTER_gpccs2tpccs_pri5
#define LW_PPRIV_GPC_PRI_MASTER_gpccs2tpccs_pri5    0
#endif
#ifndef LW_PPRIV_GPC_PRI_MASTER_gpccs2tpccs_pri6
#define LW_PPRIV_GPC_PRI_MASTER_gpccs2tpccs_pri6    0
#endif
#ifndef LW_PPRIV_GPC_PRI_MASTER_gpccs2tpccs_pri7
#define LW_PPRIV_GPC_PRI_MASTER_gpccs2tpccs_pri7    0
#endif
#ifndef LW_PPRIV_FBP_PRI_MASTER_becs2crop_pri1
#define LW_PPRIV_FBP_PRI_MASTER_becs2crop_pri1      0
#endif
#ifndef LW_PPRIV_FBP_PRI_MASTER_becs2crop_pri2
#define LW_PPRIV_FBP_PRI_MASTER_becs2crop_pri2      0
#endif
#ifndef LW_PPRIV_FBP_PRI_MASTER_becs2crop_pri3
#define LW_PPRIV_FBP_PRI_MASTER_becs2crop_pri3      0
#endif
#ifndef LW_PPRIV_FBP_PRI_MASTER_becs2crop_pri4
#define LW_PPRIV_FBP_PRI_MASTER_becs2crop_pri4      0
#endif
#ifndef LW_PPRIV_FBP_PRI_MASTER_becs2crop_pri5
#define LW_PPRIV_FBP_PRI_MASTER_becs2crop_pri5      0
#endif
#ifndef LW_PPRIV_FBP_PRI_MASTER_becs2crop_pri6
#define LW_PPRIV_FBP_PRI_MASTER_becs2crop_pri6      0
#endif
#ifndef LW_PPRIV_FBP_PRI_MASTER_becs2crop_pri7
#define LW_PPRIV_FBP_PRI_MASTER_becs2crop_pri7      0
#endif
#ifndef LW_PPRIV_SYS_PRI_MASTER_fecs2lwenc_pri0
#define LW_PPRIV_SYS_PRI_MASTER_fecs2lwenc_pri0     0
#endif
#ifndef LW_PPRIV_SYS_PRI_MASTER_fecs2lwenc_pri1
#define LW_PPRIV_SYS_PRI_MASTER_fecs2lwenc_pri1     0
#endif
#ifndef LW_PPRIV_SYS_PRI_MASTER_fecs2lwenc_pri2
#define LW_PPRIV_SYS_PRI_MASTER_fecs2lwenc_pri2     0
#endif
#ifndef LW_PPRIV_SYS_PRI_MASTER_fecs2sec_pri
#define LW_PPRIV_SYS_PRI_MASTER_fecs2sec_pri       0
#endif

// Compare two clock counts and return 1 if "a" is within 5% of "b"; otherwise, return 0
#define TOLERANCE(a,b) ((((float)a) > 0.90*((float)b)) && (((float)a) < 1.10*((float)b)))

// Needed for xclk gating
#include "lwmisc.h"

extern const ParamDecl powerCtrlTest_maxwell_params[] = {
  SIMPLE_PARAM("-testPerEngineClkGating", "Verifies engine-level clock-gating"),
  SIMPLE_PARAM("-testClkOnForReset", "Verifies engine-level clock is running on reset"),
  SIMPLE_PARAM("-testELCGPriWakeup", "Verifies engine-level clock get enabled for pri-access "),
  SIMPLE_PARAM("-testELCGHubmmuWakeup", "Verifies gr engine-level clock wake up by hubmmu"),
  SIMPLE_PARAM("-testELCGfecsInterfaceReset", "Verifies fecs ELCG interfaces can handle reset cleanly"),
  SIMPLE_PARAM("-testELCGhubmmuInterfaceReset", "Verifies hubmmu ELCG interfaces can handle reset cleanly"),
  SIMPLE_PARAM("-testELCGgpccsInterfaceReset", "Verifies gpccs ELCG interfaces can handle reset cleanly"),
  SIMPLE_PARAM("-testELCGbecsInterfaceReset", "Verifies becs ELCG interfaces can handle reset cleanly"),
  SIMPLE_PARAM("-testPowerModeInterface", "Test fe2pwr_mode interface can handle reset cleanly"),
  SIMPLE_PARAM("-testSMCFloorsweep", "Check SMC Floorsweep doesn't affect ELCG"),
  PARAM_SUBDECL(lwgpu_single_params),
  LAST_PARAM
};

powerCtrlTest_maxwell::powerCtrlTest_maxwell(ArgReader *reader): LWGpuSingleChannelTest(reader)
{
  m_params = reader;
}

powerCtrlTest_maxwell::~powerCtrlTest_maxwell(void)
{
}

Test *powerCtrlTest_maxwell::Factory(ArgDatabase *args)
{
    ArgReader *params = new ArgReader(powerCtrlTest_maxwell_params);
    if (params == NULL)
    {
        ErrPrintf("powerCtrlTest_maxwell: ArgReader create failed\n");
        return 0;
    }
    if (!params->ParseArgs(args))
    {
        ErrPrintf("powerCtrlTest_maxwell: error parsing args!\n");
        return 0;
    }
    return new powerCtrlTest_maxwell(params);
}

int powerCtrlTest_maxwell::Setup(void)
{
    lwgpu = LWGpuResource::FindFirstResource();
    UINT32 arch = lwgpu->GetArchitecture();
    InfoPrintf("powerCtrlTest_maxwell: Setup, arch is 0x%x\n", arch);
    m_arch = arch;
    macros.MacroInit(m_arch);
    SetMaxwellClkGpuResource(lwgpu);
    m_regMap = lwgpu->GetRegisterMap();
    if(!m_regMap)
    {
        ErrPrintf("powerCtrlTest_maxwell can only be run on GPU's that support a register map!\n");
        return (0);
    }
//    LW_ENGINE_TYPE eng_type;
    eng_type.GR = 0;
    if(macros.lw_scal_litter_num_ce_lces > 0)
      eng_type.CE0 = macros.lw_engine_copy0;
    if(macros.lw_scal_litter_num_ce_lces > 1)
      eng_type.CE1 = macros.lw_engine_copy1;
    if(macros.lw_scal_litter_num_ce_lces > 2)
      eng_type.CE2 = macros.lw_engine_copy2;
    if(macros.lw_scal_litter_num_ce_lces > 3)
      eng_type.CE3 = macros.lw_engine_copy3;
    if(macros.lw_scal_litter_num_ce_lces > 4)
      eng_type.CE4 = macros.lw_engine_copy4;
    if(macros.lw_scal_litter_num_ce_lces > 5)
      eng_type.CE5 = macros.lw_engine_copy5;
    if(macros.lw_scal_litter_num_ce_lces > 6)
      eng_type.CE6 = macros.lw_engine_copy6;
    if(macros.lw_scal_litter_num_ce_lces > 7)
      eng_type.CE7 = macros.lw_engine_copy7;
    if(macros.lw_scal_litter_num_ce_lces > 8)
      eng_type.CE8 = macros.lw_engine_copy8;
    if(macros.lw_scal_litter_num_lwdecs >0)
      eng_type.LWDEC = macros.lw_engine_lwdec;
    if(macros.lw_scal_litter_num_lwdecs >1)
      eng_type.LWDEC1 = macros.lw_engine_lwdec1;
    if(macros.lw_scal_litter_num_lwdecs >2)
      eng_type.LWDEC2 = macros.lw_engine_lwdec2;
    if(macros.lw_scal_litter_num_lwdecs >3)
      eng_type.LWDEC3 = macros.lw_engine_lwdec3;
    if(macros.lw_scal_litter_num_lwdecs >4)
      eng_type.LWDEC4 = macros.lw_engine_lwdec4;
    if(macros.lw_scal_litter_num_lwdecs >5)
      eng_type.LWDEC5 = macros.lw_engine_lwdec5;
    if(macros.lw_scal_litter_num_lwdecs >6)
      eng_type.LWDEC6 = macros.lw_engine_lwdec6;
    if(macros.lw_scal_litter_num_lwdecs >7)
      eng_type.LWDEC7 = macros.lw_engine_lwdec7;
    if(macros.lw_scal_litter_num_lwencs >0)
      eng_type.LWDEC = macros.lw_engine_lwenc;
    if(macros.lw_scal_litter_num_lwencs >1)
      eng_type.LWDEC1 = macros.lw_engine_lwenc1;
    if(macros.lw_scal_litter_num_lwencs >2)
      eng_type.LWDEC2 = macros.lw_engine_lwenc2;
    if(macros.lw_scal_litter_num_lwjpgs >0)
      eng_type.LWJPG0 = macros.lw_engine_lwjpg0;
    if(macros.lw_scal_litter_num_lwjpgs >1)
      eng_type.LWJPG1 = macros.lw_engine_lwjpg1;
    if(macros.lw_scal_litter_num_lwjpgs >2)
      eng_type.LWJPG2 = macros.lw_engine_lwjpg2;
    if(macros.lw_scal_litter_num_lwjpgs >3)
      eng_type.LWJPG3 = macros.lw_engine_lwjpg3;
    if(macros.lw_scal_litter_num_lwjpgs >4)
      eng_type.LWJPG4 = macros.lw_engine_lwjpg4;
    if(macros.lw_scal_litter_num_lwjpgs >5)
      eng_type.LWJPG5 = macros.lw_engine_lwjpg5;
    if(macros.lw_scal_litter_num_lwjpgs >6)
      eng_type.LWJPG6 = macros.lw_engine_lwjpg6;
    if(macros.lw_scal_litter_num_lwjpgs >7)
      eng_type.LWJPG7 = macros.lw_engine_lwjpg7;
    if(macros.lw_chip_sec)
      eng_type.SEC =  macros.lw_engine_sec;
    if (macros.lw_chip_ofa)   
      eng_type.OFA =  macros.lw_engine_ofa0;
    maxwell_clock_gating.MAXWELL_CLOCK_GR_ENGINE_GATED     = 1LL << (1 + eng_type.GR);
    maxwell_clock_gating.MAXWELL_CLOCK_CE0_ENGINE_GATED    = 1LL << (1 + eng_type.CE0);
    maxwell_clock_gating.MAXWELL_CLOCK_CE1_ENGINE_GATED    = 1LL << (1 + eng_type.CE1);
    maxwell_clock_gating.MAXWELL_CLOCK_CE2_ENGINE_GATED    = 1LL << (1 + eng_type.CE2);
    maxwell_clock_gating.MAXWELL_CLOCK_CE3_ENGINE_GATED    = 1LL << (1 + eng_type.CE3);
    maxwell_clock_gating.MAXWELL_CLOCK_LWDEC_ENGINE_GATED  = 1LL << (1 + eng_type.LWDEC);
    maxwell_clock_gating.MAXWELL_CLOCK_LWENC_ENGINE_GATED  = 1LL << (1 + eng_type.LWENC);
    maxwell_clock_gating.MAXWELL_CLOCK_OFA_ENGINE_GATED    = 1LL << (1 + eng_type.OFA);
    maxwell_clock_gating.MAXWELL_CLOCK_LWDEC2_ENGINE_GATED = 1LL << (1 + eng_type.LWDEC2);
    maxwell_clock_gating.MAXWELL_CLOCK_LWENC2_ENGINE_GATED = 1LL << (1 + eng_type.LWENC2);
    maxwell_clock_gating.MAXWELL_CLOCK_LWJPG0_ENGINE_GATED = 1LL << (1 + eng_type.LWJPG0);
    maxwell_clock_gating.MAXWELL_CLOCK_LWJPG1_ENGINE_GATED = 1LL << (1 + eng_type.LWJPG1);
    maxwell_clock_gating.MAXWELL_CLOCK_LWDEC1_ENGINE_GATED = 1LL << (1 + eng_type.LWDEC1);
    maxwell_clock_gating.MAXWELL_CLOCK_SEC_ENGINE_GATED    = 1LL << (1 + eng_type.SEC);
    maxwell_clock_gating.MAXWELL_CLOCK_LWENC1_ENGINE_GATED = 1LL << (1 + eng_type.LWENC1);
    maxwell_clock_gating.MAXWELL_CLOCK_LWDEC3_ENGINE_GATED = 1LL << (1 + eng_type.LWDEC3);
    maxwell_clock_gating.MAXWELL_CLOCK_LWJPG2_ENGINE_GATED = 1LL << (1 + eng_type.LWJPG2);
    maxwell_clock_gating.MAXWELL_CLOCK_LWJPG3_ENGINE_GATED = 1LL << (1 + eng_type.LWJPG3);
    maxwell_clock_gating.MAXWELL_CLOCK_CE4_ENGINE_GATED    = 1LL << (1 + eng_type.CE4);       
    maxwell_clock_gating.MAXWELL_CLOCK_CE5_ENGINE_GATED    = 1LL << (1 + eng_type.CE5); 
    maxwell_clock_gating.MAXWELL_CLOCK_CE6_ENGINE_GATED    = 1LL << (1 + eng_type.CE6);
    maxwell_clock_gating.MAXWELL_CLOCK_CE7_ENGINE_GATED    = 1LL << (1 + eng_type.CE7);
    maxwell_clock_gating.MAXWELL_CLOCK_LWDEC4_ENGINE_GATED = 1LL << (1 + eng_type.LWDEC4);
    maxwell_clock_gating.MAXWELL_CLOCK_LWJPG4_ENGINE_GATED = 1LL << (1 + eng_type.LWJPG4);
    maxwell_clock_gating.MAXWELL_CLOCK_LWDEC5_ENGINE_GATED = 1LL << (1 + eng_type.LWDEC5);
    maxwell_clock_gating.MAXWELL_CLOCK_LWJPG5_ENGINE_GATED = 1LL << (1 + eng_type.LWJPG5);
    maxwell_clock_gating.MAXWELL_CLOCK_CE8_ENGINE_GATED    = 1LL << (1 + eng_type.CE8);
    maxwell_clock_gating.MAXWELL_CLOCK_LWDEC6_ENGINE_GATED = 1LL << (1 + eng_type.LWDEC6);
    maxwell_clock_gating.MAXWELL_CLOCK_LWJPG6_ENGINE_GATED = 1LL << (1 + eng_type.LWJPG6);
    maxwell_clock_gating.MAXWELL_CLOCK_LWJPG7_ENGINE_GATED = 1LL << (1 + eng_type.LWJPG7);
    maxwell_clock_gating.MAXWELL_CLOCK_LWDEC7_ENGINE_GATED = 1LL << (1 + eng_type.LWDEC7);
    InitMappedClocks_maxwell(macros,maxwell_clock_gating);
    m_ch = lwgpu->CreateChannel();
    if (!m_ch) {
        ErrPrintf("powerCtrlTest_maxwell::Setup failed to create channel\n");
        return 0;
    }
//    if (OK != m_ch->AllocChannel(0))
//    {
//        ErrPrintf("%s failed to allocate LWGpuChannel!\n", __FUNCTION__);
//        return 0;
//    }

    getStateReport()->init("powerCtrlTest_maxwell");
    getStateReport()->enable();
    Platform::EscapeWrite("EndOfSimAssertDisable", 0, 1, 0x1);
    return 1;
}

void powerCtrlTest_maxwell::CleanUp(void)
{
  if (m_ch) {
    delete m_ch;
    m_ch = 0;
    InfoPrintf("powerCtrlTest_maxwell: delete m_ch\n");
  }

  if (lwgpu) {
    DebugPrintf("powerCtrlTest_maxwell::CleanUp(): Releasing GPU.\n");
    lwgpu = 0;
  }
 /*
  if (m_params) {
    delete m_params;
    m_params = 0;
  }
  */
}

// FIXME Hack Hack Hack. Platform::DelayNs function doesn't seem to be working - it seems off by
// a factor of 10?  Make a private function to do the same thing...
void powerCtrlTest_maxwell::DelayNs(UINT32 Lwclks)
{
  // Lwrrently this function is in terms of .2nS / lwclk. This is incorrectly done
  // in RTL. This should be a close approx of 1 nanosecond.
  Platform::EscapeWrite("CLOCK_WAIT",IChip::ECLOCK_LWCLK,0,Lwclks * 5);
}

// Make a global PMC_ENABLE value that enables all the engines we will test
UINT32 powerCtrlTest_maxwell::setupPMC_ENABLE(void)
{
  if (lwgpu->SetRegFldNum("LW_PMC_DEVICE_ENABLE(0)","",0xffffffff)) {
    ErrPrintf("LW_PMC_DEVICE_ENABLE(0) could not enable all engine\n");
    return 1;
  }else{
      InfoPrintf ("LW_PMC_DEVICE_ENABLE(0): enable all engines\n");
  }
 if(macros.lw_host_max_num_engines > 32) {
  if (lwgpu->SetRegFldNum("LW_PMC_DEVICE_ENABLE(1)","",0xffffffff)) {
    ErrPrintf("LW_PMC_DEVICE_ENABLE(1) could not enable all engine\n");
    return 1;
  }else{
      InfoPrintf ("LW_PMC_DEVICE_ENABLE(1): enable all engines\n");
  }
 }

  if (macros.lw_pwr_pmu_has_engine_reset) {
      if (lwgpu->SetRegFldDef("LW_PPWR_FALCON_ENGINE", "_RESET", "_FALSE")) {
          ErrPrintf ("setupPMC_ENABLE: Failed to set PWR engine reset to false.");
          return 1;
      }else{
          InfoPrintf ("setupPMC_ENABLE: Set PWR engine reset to false.");
      }
  }

  if (macros.lw_sec_falcon_has_engine_reset) {
      if (lwgpu->SetRegFldDef("LW_PSEC_FALCON_ENGINE", "_RESET", "_FALSE")) {
          ErrPrintf ("setupPMC_ENABLE: Failed to set SEC engine reset to false.");
          return 1;
      }else{
          InfoPrintf ("setupPMC_ENABLE: Set SEC engine reset to false.");
      }
  }
  return 0;
}
// Turn off clk idle slowdown
void powerCtrlTest_maxwell::DisableclkSlowdown() {
    // Set all slowdown to disable.
    // LW_POWER_SLOWDOWN_FACTOR, _HW_FAILSAFE_FORCE_DISABLE, _YES
    // LW_POWER_SLOWDOWN_FACTOR, _IDLE_FORCE_DISABLE, _YES
    // LW_POWER_SLOWDOWN_FACTOR, _SW_FORCE_DISABLE, _YES
    lwgpu->SetRegFldNum("LW_THERM_POWER_SLOWDOWN_FACTOR", "", 0xffffffff);
}

// Set engine gating mode to FULLPOWER, AUTOMATIC, or DISABLED.
void powerCtrlTest_maxwell::SetEngineMode(UINT32 engine, LW_ENGINE_MODE mode)
{
  char eng_name[80];
  unique_ptr<IRegisterClass> reg;
  InfoPrintf("SetEngineMode: engine = %d, mode = %d \n",engine, mode);
  UINT32 data, addr;
//  addr = atoi("LW_THERM_GATE_CTRL(0)"); //0x00020200;
  if (!(reg = m_regMap->FindRegister("LW_THERM_GATE_CTRL(0)")))
    {
        ErrPrintf("powerCtrlTest_maxwell: Failed to find LW_THERM_GATE_CTRL(0) register\n");
    }
  addr = reg->GetAddress();
  InfoPrintf("powerCtrlTest_maxwell: addr = 0x%x\n", addr);
  if(engine == eng_type.GR) {
    if (mode == CG_AUTOMATIC) {
      data = lwgpu->RegRd32(addr + 0x4 * macros.lw_engine_graphics);
      data = (data & 0xfffffffc) | 0x00000001;
      lwgpu->RegWr32(addr + 0x4 * macros.lw_engine_graphics, data);
    } else if (mode == CG_FULLPOWER) {
      data = lwgpu->RegRd32(addr + 0x4 * macros.lw_engine_graphics);
      data = (data & 0xfffffffc) | 0x00000000;
      lwgpu->RegWr32(addr + 0x4 * macros.lw_engine_graphics, data);
    } else if (mode == CG_DISABLED) {
      data = lwgpu->RegRd32(addr + 0x4 * macros.lw_engine_graphics);
      data = (data & 0xfffffffc) | 0x00000002;
      lwgpu->RegWr32(addr + 0x4 * macros.lw_engine_graphics, data);
    }
    sprintf(eng_name, "GR");
    }

  if(engine == eng_type.CE0) {
    if (mode == CG_AUTOMATIC) {
      data = lwgpu->RegRd32(addr + 0x4 * macros.lw_engine_copy0);
      data = (data & 0xfffffffc) | 0x00000001;
      lwgpu->RegWr32(addr + 0x4 * macros.lw_engine_copy0, data);
    } else if (mode == CG_FULLPOWER) {
      data = lwgpu->RegRd32(addr + 0x4 * macros.lw_engine_copy0);
      data = (data & 0xfffffffc) | 0x00000000;
      lwgpu->RegWr32(addr + 0x4 * macros.lw_engine_copy0, data);
    } else if (mode == CG_DISABLED) {
      data = lwgpu->RegRd32(addr + 0x4 * macros.lw_engine_copy0);
      data = (data & 0xfffffffc) | 0x00000002;
      lwgpu->RegWr32(addr + 0x4 * macros.lw_engine_copy0, data);
    }
      sprintf(eng_name, "CE0");
    }

  if(engine == eng_type.CE1) {
    if (mode == CG_AUTOMATIC) {
      data = lwgpu->RegRd32(addr + 0x4 * macros.lw_engine_copy1);
      data = (data & 0xfffffffc) | 0x00000001;
      lwgpu->RegWr32(addr + 0x4 * macros.lw_engine_copy1, data);
    } else if (mode == CG_FULLPOWER) {
      data = lwgpu->RegRd32(addr + 0x4 * macros.lw_engine_copy1);
      data = (data & 0xfffffffc) | 0x00000000;
      lwgpu->RegWr32(addr + 0x4 * macros.lw_engine_copy1, data);
    } else if (mode == CG_DISABLED) {
      data = lwgpu->RegRd32(addr + 0x4 * macros.lw_engine_copy1);
      data = (data & 0xfffffffc) | 0x00000002;
      lwgpu->RegWr32(addr + 0x4 * macros.lw_engine_copy1, data);
    }
      sprintf(eng_name, "CE1");
    }

  if(engine == eng_type.CE2) {
    if (mode == CG_AUTOMATIC) {
      data = lwgpu->RegRd32(addr + 0x4 * macros.lw_engine_copy2);
      data = (data & 0xfffffffc) | 0x00000001;
      lwgpu->RegWr32(addr + 0x4 * macros.lw_engine_copy2, data);
    } else if (mode == CG_FULLPOWER) {
      data = lwgpu->RegRd32(addr + 0x4 * macros.lw_engine_copy2);
      data = (data & 0xfffffffc) | 0x00000000;
      lwgpu->RegWr32(addr + 0x4 * macros.lw_engine_copy2, data);
    } else if (mode == CG_DISABLED) {
      data = lwgpu->RegRd32(addr + 0x4 * macros.lw_engine_copy2);
      data = (data & 0xfffffffc) | 0x00000002;
      lwgpu->RegWr32(addr + 0x4 * macros.lw_engine_copy2, data);
    }
      sprintf(eng_name, "CE2");
    }

  if(engine == eng_type.CE3) {
    if (mode == CG_AUTOMATIC) {
      data = lwgpu->RegRd32(addr + 0x4 * macros.lw_engine_copy3);
      data = (data & 0xfffffffc) | 0x00000001;
      lwgpu->RegWr32(addr + 0x4 * macros.lw_engine_copy3, data);
    } else if (mode == CG_FULLPOWER) {
      data = lwgpu->RegRd32(addr + 0x4 * macros.lw_engine_copy3);
      data = (data & 0xfffffffc) | 0x00000000;
      lwgpu->RegWr32(addr + 0x4 * macros.lw_engine_copy3, data);
    } else if (mode == CG_DISABLED) {
      data = lwgpu->RegRd32(addr + 0x4 * macros.lw_engine_copy3);
      data = (data & 0xfffffffc) | 0x00000002;
      lwgpu->RegWr32(addr + 0x4 * macros.lw_engine_copy3, data);
    }
      sprintf(eng_name, "CE3");
    }

  if(engine == eng_type.CE4) {
    if (mode == CG_AUTOMATIC) {
      data = lwgpu->RegRd32(addr + 0x4 * macros.lw_engine_copy4);
      data = (data & 0xfffffffc) | 0x00000001;
      lwgpu->RegWr32(addr + 0x4 * macros.lw_engine_copy4, data);
    } else if (mode == CG_FULLPOWER) {
      data = lwgpu->RegRd32(addr + 0x4 * macros.lw_engine_copy4);
      data = (data & 0xfffffffc) | 0x00000000;
      lwgpu->RegWr32(addr + 0x4 * macros.lw_engine_copy4, data);
    } else if (mode == CG_DISABLED) {
      data = lwgpu->RegRd32(addr + 0x4 * macros.lw_engine_copy4);
      data = (data & 0xfffffffc) | 0x00000002;
      lwgpu->RegWr32(addr + 0x4 * macros.lw_engine_copy4, data);
    }
      sprintf(eng_name, "CE4");
    }

  if(engine == eng_type.CE5) {
    if (mode == CG_AUTOMATIC) {
      data = lwgpu->RegRd32(addr + 0x4 * macros.lw_engine_copy5);
      data = (data & 0xfffffffc) | 0x00000001;
      lwgpu->RegWr32(addr + 0x4 * macros.lw_engine_copy5, data);
    } else if (mode == CG_FULLPOWER) {
      data = lwgpu->RegRd32(addr + 0x4 * macros.lw_engine_copy5);
      data = (data & 0xfffffffc) | 0x00000000;
      lwgpu->RegWr32(addr + 0x4 * macros.lw_engine_copy5, data);
    } else if (mode == CG_DISABLED) {
      data = lwgpu->RegRd32(addr + 0x4 * macros.lw_engine_copy5);
      data = (data & 0xfffffffc) | 0x00000002;
      lwgpu->RegWr32(addr + 0x4 * macros.lw_engine_copy5, data);
    }
      sprintf(eng_name, "CE5");
    }

  if(engine == eng_type.CE6) {
    if (mode == CG_AUTOMATIC) {
      data = lwgpu->RegRd32(addr + 0x4 * macros.lw_engine_copy6);
      data = (data & 0xfffffffc) | 0x00000001;
      lwgpu->RegWr32(addr + 0x4 * macros.lw_engine_copy6, data);
    } else if (mode == CG_FULLPOWER) {
      data = lwgpu->RegRd32(addr + 0x4 * macros.lw_engine_copy6);
      data = (data & 0xfffffffc) | 0x00000000;
      lwgpu->RegWr32(addr + 0x4 * macros.lw_engine_copy6, data);
    } else if (mode == CG_DISABLED) {
      data = lwgpu->RegRd32(addr + 0x4 * macros.lw_engine_copy6);
      data = (data & 0xfffffffc) | 0x00000002;
      lwgpu->RegWr32(addr + 0x4 * macros.lw_engine_copy6, data);
    }
      sprintf(eng_name, "CE6");
    }

  if(engine == eng_type.CE7) {
    if (mode == CG_AUTOMATIC) {
      data = lwgpu->RegRd32(addr + 0x4 * macros.lw_engine_copy7);
      data = (data & 0xfffffffc) | 0x00000001;
      lwgpu->RegWr32(addr + 0x4 * macros.lw_engine_copy7, data);
    } else if (mode == CG_FULLPOWER) {
      data = lwgpu->RegRd32(addr + 0x4 * macros.lw_engine_copy7);
      data = (data & 0xfffffffc) | 0x00000000;
      lwgpu->RegWr32(addr + 0x4 * macros.lw_engine_copy7, data);
    } else if (mode == CG_DISABLED) {
      data = lwgpu->RegRd32(addr + 0x4 * macros.lw_engine_copy7);
      data = (data & 0xfffffffc) | 0x00000002;
      lwgpu->RegWr32(addr + 0x4 * macros.lw_engine_copy7, data);
    }
      sprintf(eng_name, "CE7");
    }

  if(engine == eng_type.CE8) {
    if (mode == CG_AUTOMATIC) {
      data = lwgpu->RegRd32(addr + 0x4 * macros.lw_engine_copy8);
      data = (data & 0xfffffffc) | 0x00000001;
      lwgpu->RegWr32(addr + 0x4 * macros.lw_engine_copy8, data);
    } else if (mode == CG_FULLPOWER) {
      data = lwgpu->RegRd32(addr + 0x4 * macros.lw_engine_copy8);
      data = (data & 0xfffffffc) | 0x00000000;
      lwgpu->RegWr32(addr + 0x4 * macros.lw_engine_copy8, data);
    } else if (mode == CG_DISABLED) {
      data = lwgpu->RegRd32(addr + 0x4 * macros.lw_engine_copy8);
      data = (data & 0xfffffffc) | 0x00000002;
      lwgpu->RegWr32(addr + 0x4 * macros.lw_engine_copy8, data);
    }
      sprintf(eng_name, "CE8");
    }
  
  if(engine == eng_type.CE9) {
    if (mode == CG_AUTOMATIC) {
      data = lwgpu->RegRd32(addr + 0x4 * macros.lw_engine_copy9);
      data = (data & 0xfffffffc) | 0x00000001;
      lwgpu->RegWr32(addr + 0x4 * macros.lw_engine_copy9, data);
    } else if (mode == CG_FULLPOWER) {
      data = lwgpu->RegRd32(addr + 0x4 * macros.lw_engine_copy9);
      data = (data & 0xfffffffc) | 0x00000000;
      lwgpu->RegWr32(addr + 0x4 * macros.lw_engine_copy9, data);
    } else if (mode == CG_DISABLED) {
      data = lwgpu->RegRd32(addr + 0x4 * macros.lw_engine_copy9);
      data = (data & 0xfffffffc) | 0x00000002;
      lwgpu->RegWr32(addr + 0x4 * macros.lw_engine_copy9, data);
    }
      sprintf(eng_name, "CE9");
    }

  if(engine == eng_type.SEC) {
    if (mode == CG_AUTOMATIC) {
      data = lwgpu->RegRd32(addr + 0x4 * macros.lw_engine_sec);
      data = (data & 0xfffffffc) | 0x00000001;
      lwgpu->RegWr32(addr + 0x4 * macros.lw_engine_sec, data);
    } else if (mode == CG_FULLPOWER) {
      data = lwgpu->RegRd32(addr + 0x4 * macros.lw_engine_sec);
      data = (data & 0xfffffffc) | 0x00000000;
      lwgpu->RegWr32(addr + 0x4 * macros.lw_engine_sec, data);
    } else if (mode == CG_DISABLED) {
      data = lwgpu->RegRd32(addr + 0x4 * macros.lw_engine_sec);
      data = (data & 0xfffffffc) | 0x00000002;
      lwgpu->RegWr32(addr + 0x4 * macros.lw_engine_sec, data);
    }
      sprintf(eng_name, "SEC");
    }

  if(engine == eng_type.LWENC) {
    if (mode == CG_AUTOMATIC) {
      data = lwgpu->RegRd32(addr + 0x4 * macros.lw_engine_lwenc);
      data = (data & 0xfffffffc) | 0x00000001;
      lwgpu->RegWr32(addr + 0x4 * macros.lw_engine_lwenc, data);
    } else if (mode == CG_FULLPOWER) {
      data = lwgpu->RegRd32(addr + 0x4 * macros.lw_engine_lwenc);
      data = (data & 0xfffffffc) | 0x00000000;
      lwgpu->RegWr32(addr + 0x4 * macros.lw_engine_lwenc, data);
    } else if (mode == CG_DISABLED) {
      data = lwgpu->RegRd32(addr + 0x4 * macros.lw_engine_lwenc);
      data = (data & 0xfffffffc) | 0x00000002;
      lwgpu->RegWr32(addr + 0x4 * macros.lw_engine_lwenc, data);
    }
      sprintf(eng_name, "LWENC");
    }

  if(engine == eng_type.LWENC2) {
    if (mode == CG_AUTOMATIC) {
      data = lwgpu->RegRd32(addr + 0x4 * macros.lw_engine_lwenc2);
      data = (data & 0xfffffffc) | 0x00000001;
      lwgpu->RegWr32(addr + 0x4 * macros.lw_engine_lwenc2, data);
    } else if (mode == CG_FULLPOWER) {
      data = lwgpu->RegRd32(addr + 0x4 * macros.lw_engine_lwenc2);
      data = (data & 0xfffffffc) | 0x00000000;
      lwgpu->RegWr32(addr + 0x4 * macros.lw_engine_lwenc2, data);
    } else if (mode == CG_DISABLED) {
      data = lwgpu->RegRd32(addr + 0x4 * macros.lw_engine_lwenc2);
      data = (data & 0xfffffffc) | 0x00000002;
      lwgpu->RegWr32(addr + 0x4 * macros.lw_engine_lwenc2, data);
    }
      sprintf(eng_name, "LWENC2");
    }

  if(engine == eng_type.LWENC1) {
    if (mode == CG_AUTOMATIC) {
      data = lwgpu->RegRd32(addr + 0x4 * macros.lw_engine_lwenc1);
      data = (data & 0xfffffffc) | 0x00000001;
      lwgpu->RegWr32(addr + 0x4 * macros.lw_engine_lwenc1, data);
    } else if (mode == CG_FULLPOWER) {
      data = lwgpu->RegRd32(addr + 0x4 * macros.lw_engine_lwenc1);
      data = (data & 0xfffffffc) | 0x00000000;
      lwgpu->RegWr32(addr + 0x4 * macros.lw_engine_lwenc1, data);
    } else if (mode == CG_DISABLED) {
      data = lwgpu->RegRd32(addr + 0x4 * macros.lw_engine_lwenc1);
      data = (data & 0xfffffffc) | 0x00000002;
      lwgpu->RegWr32(addr + 0x4 * macros.lw_engine_lwenc1, data);
    }
      sprintf(eng_name, "LWENC1");
    }

  if(engine == eng_type.LWDEC) {
    if (mode == CG_AUTOMATIC) {
      data = lwgpu->RegRd32(addr + 0x4 * macros.lw_engine_lwdec);
      data = (data & 0xfffffffc) | 0x00000001;
      lwgpu->RegWr32(addr + 0x4 * macros.lw_engine_lwdec, data);
    } else if (mode == CG_FULLPOWER) {
      data = lwgpu->RegRd32(addr + 0x4 * macros.lw_engine_lwdec);
      data = (data & 0xfffffffc) | 0x00000000;
      lwgpu->RegWr32(addr + 0x4 * macros.lw_engine_lwdec, data);
    } else if (mode == CG_DISABLED) {
      data = lwgpu->RegRd32(addr + 0x4 * macros.lw_engine_lwdec);
      data = (data & 0xfffffffc) | 0x00000002;
      lwgpu->RegWr32(addr + 0x4 * macros.lw_engine_lwdec, data);
    }
      sprintf(eng_name, "LWDEC");
    }

  if(engine == eng_type.LWDEC1) {
      if (mode == CG_AUTOMATIC) {
          data = lwgpu->RegRd32(addr + 0x4 * macros.lw_engine_lwdec1);
          data = (data & 0xfffffffc) | 0x00000001;
          lwgpu->RegWr32(addr + 0x4 * macros.lw_engine_lwdec1, data);
      } else if (mode == CG_FULLPOWER) {
          data = lwgpu->RegRd32(addr + 0x4 * macros.lw_engine_lwdec1);
          data = (data & 0xfffffffc) | 0x00000000;
          lwgpu->RegWr32(addr + 0x4 * macros.lw_engine_lwdec1, data);
      } else if (mode == CG_DISABLED) {
          data = lwgpu->RegRd32(addr + 0x4 * macros.lw_engine_lwdec1);
          data = (data & 0xfffffffc) | 0x00000003;
          lwgpu->RegWr32(addr + 0x4 * macros.lw_engine_lwdec1, data);
      }
        sprintf(eng_name, "LWDEC1");
        }
                                                                                  
  if(engine == eng_type.LWDEC2) {
      if (mode == CG_AUTOMATIC) {
          data = lwgpu->RegRd32(addr + 0x4 * macros.lw_engine_lwdec2);
          data = (data & 0xfffffffc) | 0x00000001;
          lwgpu->RegWr32(addr + 0x4 * macros.lw_engine_lwdec2, data);
      } else if (mode == CG_FULLPOWER) {
          data = lwgpu->RegRd32(addr + 0x4 * macros.lw_engine_lwdec2);
          data = (data & 0xfffffffc) | 0x00000000;
          lwgpu->RegWr32(addr + 0x4 * macros.lw_engine_lwdec2, data);
      } else if (mode == CG_DISABLED) {
          data = lwgpu->RegRd32(addr + 0x4 * macros.lw_engine_lwdec2);
          data = (data & 0xfffffffc) | 0x00000003;
          lwgpu->RegWr32(addr + 0x4 * macros.lw_engine_lwdec2, data);
      }
        sprintf(eng_name, "LWDEC2");
        }
                                                                                  
  if(engine == eng_type.LWDEC3) {
      if (mode == CG_AUTOMATIC) {
          data = lwgpu->RegRd32(addr + 0x4 * macros.lw_engine_lwdec3);
          data = (data & 0xfffffffc) | 0x00000001;
          lwgpu->RegWr32(addr + 0x4 * macros.lw_engine_lwdec3, data);
      } else if (mode == CG_FULLPOWER) {
          data = lwgpu->RegRd32(addr + 0x4 * macros.lw_engine_lwdec3);
          data = (data & 0xfffffffc) | 0x00000000;
          lwgpu->RegWr32(addr + 0x4 * macros.lw_engine_lwdec3, data);
      } else if (mode == CG_DISABLED) {
          data = lwgpu->RegRd32(addr + 0x4 * macros.lw_engine_lwdec3);
          data = (data & 0xfffffffc) | 0x00000003;
          lwgpu->RegWr32(addr + 0x4 * macros.lw_engine_lwdec3, data);
      }
        sprintf(eng_name, "LWDEC3");
        }
                                                                                  
  if(engine == eng_type.LWDEC4) {
      if (mode == CG_AUTOMATIC) {
          data = lwgpu->RegRd32(addr + 0x4 * macros.lw_engine_lwdec4);
          data = (data & 0xfffffffc) | 0x00000001;
          lwgpu->RegWr32(addr + 0x4 * macros.lw_engine_lwdec4, data);
      } else if (mode == CG_FULLPOWER) {
          data = lwgpu->RegRd32(addr + 0x4 * macros.lw_engine_lwdec4);
          data = (data & 0xfffffffc) | 0x00000000;
          lwgpu->RegWr32(addr + 0x4 * macros.lw_engine_lwdec4, data);
      } else if (mode == CG_DISABLED) {
          data = lwgpu->RegRd32(addr + 0x4 * macros.lw_engine_lwdec4);
          data = (data & 0xfffffffc) | 0x00000003;
          lwgpu->RegWr32(addr + 0x4 * macros.lw_engine_lwdec4, data);
      }
        sprintf(eng_name, "LWDEC4");
        }
  if(engine == eng_type.LWDEC5) {
      if (mode == CG_AUTOMATIC) {
          data = lwgpu->RegRd32(addr + 0x4 * macros.lw_engine_lwdec5);
          data = (data & 0xfffffffc) | 0x00000001;
          lwgpu->RegWr32(addr + 0x4 * macros.lw_engine_lwdec5, data);
      } else if (mode == CG_FULLPOWER) {
          data = lwgpu->RegRd32(addr + 0x4 * macros.lw_engine_lwdec5);
          data = (data & 0xfffffffc) | 0x00000000;
          lwgpu->RegWr32(addr + 0x4 * macros.lw_engine_lwdec5, data);
      } else if (mode == CG_DISABLED) {
          data = lwgpu->RegRd32(addr + 0x4 * macros.lw_engine_lwdec5);
          data = (data & 0xfffffffc) | 0x00000003;
          lwgpu->RegWr32(addr + 0x4 * macros.lw_engine_lwdec5, data);
      }
        sprintf(eng_name, "LWDEC5");
        }
   if(engine == eng_type.LWDEC6) {
      if (mode == CG_AUTOMATIC) {
          data = lwgpu->RegRd32(addr + 0x4 * macros.lw_engine_lwdec6);
          data = (data & 0xfffffffc) | 0x00000001;
          lwgpu->RegWr32(addr + 0x4 * macros.lw_engine_lwdec6, data);
      } else if (mode == CG_FULLPOWER) {
          data = lwgpu->RegRd32(addr + 0x4 * macros.lw_engine_lwdec6);
          data = (data & 0xfffffffc) | 0x00000000;
          lwgpu->RegWr32(addr + 0x4 * macros.lw_engine_lwdec6, data);
      } else if (mode == CG_DISABLED) {
          data = lwgpu->RegRd32(addr + 0x4 * macros.lw_engine_lwdec6);
          data = (data & 0xfffffffc) | 0x00000003;
          lwgpu->RegWr32(addr + 0x4 * macros.lw_engine_lwdec6, data);
      }
        sprintf(eng_name, "LWDEC6");
        }
   if(engine == eng_type.LWDEC7) {
      if (mode == CG_AUTOMATIC) {
          data = lwgpu->RegRd32(addr + 0x4 * macros.lw_engine_lwdec7);
          data = (data & 0xfffffffc) | 0x00000001;
          lwgpu->RegWr32(addr + 0x4 * macros.lw_engine_lwdec7, data);
      } else if (mode == CG_FULLPOWER) {
          data = lwgpu->RegRd32(addr + 0x4 * macros.lw_engine_lwdec7);
          data = (data & 0xfffffffc) | 0x00000000;
          lwgpu->RegWr32(addr + 0x4 * macros.lw_engine_lwdec7, data);
      } else if (mode == CG_DISABLED) {
          data = lwgpu->RegRd32(addr + 0x4 * macros.lw_engine_lwdec7);
          data = (data & 0xfffffffc) | 0x00000003;
          lwgpu->RegWr32(addr + 0x4 * macros.lw_engine_lwdec7, data);
      }
        sprintf(eng_name, "LWDEC7");
        }
  if(engine == eng_type.LWJPG0) {
      if (mode == CG_AUTOMATIC) {
          data = lwgpu->RegRd32(addr + 0x4 * macros.lw_engine_lwjpg0);
          data = (data & 0xfffffffc) | 0x00000001;
          lwgpu->RegWr32(addr + 0x4 * macros.lw_engine_lwjpg0, data);
      } else if (mode == CG_FULLPOWER) {
          data = lwgpu->RegRd32(addr + 0x4 * macros.lw_engine_lwjpg0);
          data = (data & 0xfffffffc) | 0x00000000;
          lwgpu->RegWr32(addr + 0x4 * macros.lw_engine_lwjpg0, data);
      } else if (mode == CG_DISABLED) {
          data = lwgpu->RegRd32(addr + 0x4 * macros.lw_engine_lwjpg0);
          data = (data & 0xfffffffc) | 0x00000003;
          lwgpu->RegWr32(addr + 0x4 * macros.lw_engine_lwjpg0, data);
      }
        sprintf(eng_name, "LWJPG0");
        }
  if(engine == eng_type.LWJPG1) {
      if (mode == CG_AUTOMATIC) {
          data = lwgpu->RegRd32(addr + 0x4 * macros.lw_engine_lwjpg1);
          data = (data & 0xfffffffc) | 0x00000001;
          lwgpu->RegWr32(addr + 0x4 * macros.lw_engine_lwjpg1, data);
      } else if (mode == CG_FULLPOWER) {
          data = lwgpu->RegRd32(addr + 0x4 * macros.lw_engine_lwjpg1);
          data = (data & 0xfffffffc) | 0x00000000;
          lwgpu->RegWr32(addr + 0x4 * macros.lw_engine_lwjpg1, data);
      } else if (mode == CG_DISABLED) {
          data = lwgpu->RegRd32(addr + 0x4 * macros.lw_engine_lwjpg1);
          data = (data & 0xfffffffc) | 0x00000003;
          lwgpu->RegWr32(addr + 0x4 * macros.lw_engine_lwjpg1, data);
      }
        sprintf(eng_name, "LWJPG1");
        }
  if(engine == eng_type.LWJPG2) {
      if (mode == CG_AUTOMATIC) {
          data = lwgpu->RegRd32(addr + 0x4 * macros.lw_engine_lwjpg2);
          data = (data & 0xfffffffc) | 0x00000001;
          lwgpu->RegWr32(addr + 0x4 * macros.lw_engine_lwjpg2, data);
      } else if (mode == CG_FULLPOWER) {
          data = lwgpu->RegRd32(addr + 0x4 * macros.lw_engine_lwjpg2);
          data = (data & 0xfffffffc) | 0x00000000;
          lwgpu->RegWr32(addr + 0x4 * macros.lw_engine_lwjpg2, data);
      } else if (mode == CG_DISABLED) {
          data = lwgpu->RegRd32(addr + 0x4 * macros.lw_engine_lwjpg2);
          data = (data & 0xfffffffc) | 0x00000003;
          lwgpu->RegWr32(addr + 0x4 * macros.lw_engine_lwjpg2, data);
      }
        sprintf(eng_name, "LWJPG2");
        }
  if(engine == eng_type.LWJPG3) {
      if (mode == CG_AUTOMATIC) {
          data = lwgpu->RegRd32(addr + 0x4 * macros.lw_engine_lwjpg3);
          data = (data & 0xfffffffc) | 0x00000001;
          lwgpu->RegWr32(addr + 0x4 * macros.lw_engine_lwjpg3, data);
      } else if (mode == CG_FULLPOWER) {
          data = lwgpu->RegRd32(addr + 0x4 * macros.lw_engine_lwjpg3);
          data = (data & 0xfffffffc) | 0x00000000;
          lwgpu->RegWr32(addr + 0x4 * macros.lw_engine_lwjpg3, data);
      } else if (mode == CG_DISABLED) {
          data = lwgpu->RegRd32(addr + 0x4 * macros.lw_engine_lwjpg3);
          data = (data & 0xfffffffc) | 0x00000003;
          lwgpu->RegWr32(addr + 0x4 * macros.lw_engine_lwjpg3, data);
      }
        sprintf(eng_name, "LWJPG3");
        }
  if(engine == eng_type.LWJPG4) {
      if (mode == CG_AUTOMATIC) {
          data = lwgpu->RegRd32(addr + 0x4 * macros.lw_engine_lwjpg4);
          data = (data & 0xfffffffc) | 0x00000001;
          lwgpu->RegWr32(addr + 0x4 * macros.lw_engine_lwjpg4, data);
      } else if (mode == CG_FULLPOWER) {
          data = lwgpu->RegRd32(addr + 0x4 * macros.lw_engine_lwjpg4);
          data = (data & 0xfffffffc) | 0x00000000;
          lwgpu->RegWr32(addr + 0x4 * macros.lw_engine_lwjpg4, data);
      } else if (mode == CG_DISABLED) {
          data = lwgpu->RegRd32(addr + 0x4 * macros.lw_engine_lwjpg4);
          data = (data & 0xfffffffc) | 0x00000003;
          lwgpu->RegWr32(addr + 0x4 * macros.lw_engine_lwjpg4, data);
      }
        sprintf(eng_name, "LWJPG4");
        }
  if(engine == eng_type.LWJPG5) {
      if (mode == CG_AUTOMATIC) {
          data = lwgpu->RegRd32(addr + 0x4 * macros.lw_engine_lwjpg5);
          data = (data & 0xfffffffc) | 0x00000001;
          lwgpu->RegWr32(addr + 0x4 * macros.lw_engine_lwjpg5, data);
      } else if (mode == CG_FULLPOWER) {
          data = lwgpu->RegRd32(addr + 0x4 * macros.lw_engine_lwjpg5);
          data = (data & 0xfffffffc) | 0x00000000;
          lwgpu->RegWr32(addr + 0x4 * macros.lw_engine_lwjpg5, data);
      } else if (mode == CG_DISABLED) {
          data = lwgpu->RegRd32(addr + 0x4 * macros.lw_engine_lwjpg5);
          data = (data & 0xfffffffc) | 0x00000003;
          lwgpu->RegWr32(addr + 0x4 * macros.lw_engine_lwjpg5, data);
      }
        sprintf(eng_name, "LWJPG5");
        }
  if(engine == eng_type.LWJPG6) {
      if (mode == CG_AUTOMATIC) {
          data = lwgpu->RegRd32(addr + 0x4 * macros.lw_engine_lwjpg6);
          data = (data & 0xfffffffc) | 0x00000001;
          lwgpu->RegWr32(addr + 0x4 * macros.lw_engine_lwjpg6, data);
      } else if (mode == CG_FULLPOWER) {
          data = lwgpu->RegRd32(addr + 0x4 * macros.lw_engine_lwjpg6);
          data = (data & 0xfffffffc) | 0x00000000;
          lwgpu->RegWr32(addr + 0x4 * macros.lw_engine_lwjpg6, data);
      } else if (mode == CG_DISABLED) {
          data = lwgpu->RegRd32(addr + 0x4 * macros.lw_engine_lwjpg6);
          data = (data & 0xfffffffc) | 0x00000003;
          lwgpu->RegWr32(addr + 0x4 * macros.lw_engine_lwjpg6, data);
      }
        sprintf(eng_name, "LWJPG6");
        }
  if(engine == eng_type.LWJPG7) {
      if (mode == CG_AUTOMATIC) {
          data = lwgpu->RegRd32(addr + 0x4 * macros.lw_engine_lwjpg7);
          data = (data & 0xfffffffc) | 0x00000001;
          lwgpu->RegWr32(addr + 0x4 * macros.lw_engine_lwjpg7, data);
      } else if (mode == CG_FULLPOWER) {
          data = lwgpu->RegRd32(addr + 0x4 * macros.lw_engine_lwjpg7);
          data = (data & 0xfffffffc) | 0x00000000;
          lwgpu->RegWr32(addr + 0x4 * macros.lw_engine_lwjpg7, data);
      } else if (mode == CG_DISABLED) {
          data = lwgpu->RegRd32(addr + 0x4 * macros.lw_engine_lwjpg7);
          data = (data & 0xfffffffc) | 0x00000003;
          lwgpu->RegWr32(addr + 0x4 * macros.lw_engine_lwjpg7, data);
      }
        sprintf(eng_name, "LWJPG7");
        }      
  if(engine == eng_type.OFA) {
      if (mode == CG_AUTOMATIC) {
          data = lwgpu->RegRd32(addr + 0x4 * macros.lw_engine_ofa0);
          data = (data & 0xfffffffc) | 0x00000001;
          lwgpu->RegWr32(addr + 0x4 * macros.lw_engine_ofa0, data);
      } else if (mode == CG_FULLPOWER) {
          data = lwgpu->RegRd32(addr + 0x4 * macros.lw_engine_ofa0);
          data = (data & 0xfffffffc) | 0x00000000;
          lwgpu->RegWr32(addr + 0x4 * macros.lw_engine_ofa0, data);
      } else if (mode == CG_DISABLED) {
          data = lwgpu->RegRd32(addr + 0x4 * macros.lw_engine_ofa0);
          data = (data & 0xfffffffc) | 0x00000003;
          lwgpu->RegWr32(addr + 0x4 * macros.lw_engine_ofa0, data);
      }
        sprintf(eng_name, "OFA");
  } 

  if (mode == CG_AUTOMATIC) {
    InfoPrintf("powerCtrlTest_maxwell: SetEngineMode %s engine set to AUTOMATIC\n", eng_name);
  } else if (mode == CG_FULLPOWER) {
    InfoPrintf("powerCtrlTest_maxwell: SetEngineMode %s engine set to FULLPOWER\n", eng_name);
  } else if (mode == CG_DISABLED) {
    InfoPrintf("powerCtrlTest_maxwell: SetEngineMode %s engine set to DISABLED\n", eng_name);
  }
}

void powerCtrlTest_maxwell::AllEngineForceClksOn(LW_ENGINE_MODE mode)
{

  ForceEngineClksOn(eng_type.GR, mode);
  if (macros.lw_scal_litter_num_ce_lces > 0) {
    ForceEngineClksOn(eng_type.CE0, mode);
  }
  if (macros.lw_scal_litter_num_ce_lces > 1) {
    ForceEngineClksOn(eng_type.CE1, mode);
  }
  if (macros.lw_scal_litter_num_ce_lces > 2) {
    ForceEngineClksOn(eng_type.CE2, mode);
  }
  if (macros.lw_scal_litter_num_ce_lces > 3) {
    ForceEngineClksOn(eng_type.CE3, mode);
  }
  if (macros.lw_scal_litter_num_ce_lces > 4) {
    ForceEngineClksOn(eng_type.CE4, mode);
  }
  if (macros.lw_scal_litter_num_ce_lces > 5) {
    ForceEngineClksOn(eng_type.CE5, mode);
  }
  if (macros.lw_scal_litter_num_ce_lces > 6) {
    ForceEngineClksOn(eng_type.CE6, mode);
  }
  if (macros.lw_scal_litter_num_ce_lces > 7) {
    ForceEngineClksOn(eng_type.CE7, mode);
  }
  if (macros.lw_scal_litter_num_ce_lces > 8) {
    ForceEngineClksOn(eng_type.CE8, mode);
  }
  if (macros.lw_scal_litter_num_ce_lces > 9) {
    ForceEngineClksOn(eng_type.CE9, mode);
  }
  if (macros.lw_chip_sec) {
      ForceEngineClksOn(eng_type.SEC, mode);
  }
  if (macros.lw_chip_ofa) {
      ForceEngineClksOn(eng_type.OFA, mode);
  }
  if (macros.lw_chip_lwjpg) {
    if(macros.lw_scal_litter_num_lwjpgs > 0) {      
      ForceEngineClksOn(eng_type.LWJPG0, mode);
    }
    if(macros.lw_scal_litter_num_lwjpgs > 1) {      
      ForceEngineClksOn(eng_type.LWJPG1, mode);
    }
    if(macros.lw_scal_litter_num_lwjpgs > 2) {      
      ForceEngineClksOn(eng_type.LWJPG2, mode);
    }
    if(macros.lw_scal_litter_num_lwjpgs > 3) {      
      ForceEngineClksOn(eng_type.LWJPG3, mode);
    }
    if(macros.lw_scal_litter_num_lwjpgs > 4) {      
      ForceEngineClksOn(eng_type.LWJPG4, mode);
    }
    if(macros.lw_scal_litter_num_lwjpgs > 5) {      
      ForceEngineClksOn(eng_type.LWJPG5, mode);
    }
    if(macros.lw_scal_litter_num_lwjpgs > 6) {      
      ForceEngineClksOn(eng_type.LWJPG6, mode);
    }
    if(macros.lw_scal_litter_num_lwjpgs > 7) {      
      ForceEngineClksOn(eng_type.LWJPG7, mode);
    }
    
  }
  if (macros.lw_chip_lwdec) {
      ForceEngineClksOn(eng_type.LWDEC, mode);
      if (macros.lw_scal_chip_num_lwdecs > 1) {
          ForceEngineClksOn(eng_type.LWDEC1, mode);
      }
      if (macros.lw_scal_chip_num_lwdecs > 2) {
          ForceEngineClksOn(eng_type.LWDEC2, mode);
      }
      if (macros.lw_scal_chip_num_lwdecs > 3) {
          ForceEngineClksOn(eng_type.LWDEC3, mode);
      }
      if (macros.lw_scal_chip_num_lwdecs > 4) {
          ForceEngineClksOn(eng_type.LWDEC4, mode);
      }
      if (macros.lw_scal_chip_num_lwdecs > 5) {
          ForceEngineClksOn(eng_type.LWDEC5, mode);
      }
      if (macros.lw_scal_chip_num_lwdecs > 6) {
          ForceEngineClksOn(eng_type.LWDEC6, mode);
      }
      if (macros.lw_scal_chip_num_lwdecs > 7) {
          ForceEngineClksOn(eng_type.LWDEC7, mode);
      }
  }
  if (macros.lw_chip_lwenc) {
      ForceEngineClksOn(eng_type.LWENC, mode);
      //some chip has more lwencs.
     if (macros.lw_engine_lwenc1){
      ForceEngineClksOn(eng_type.LWENC1, mode);
     }
     if (macros.lw_engine_lwenc2){
      ForceEngineClksOn(eng_type.LWENC2, mode);
     }
  }
}

// Set force_clks_on to each engine
void powerCtrlTest_maxwell::ForceEngineClksOn(UINT32 engine, LW_ENGINE_MODE mode)
{
  char eng_name[80];
  unique_ptr<IRegisterClass> reg;
  InfoPrintf("ForceEngineClksOn: engine = %d, mode = %d \n",engine, mode);
  UINT32 data,addr;
  // 00020a50 denotes LW_THERM_GATE_CTRL register
  // ENG_CLK [1:0] : 0-->RUN mode, the el clock will run always. 1-->AUTO mode, the clock will run when engine is non-idle. 2-->STOP mode, the clock will be stopped.
  // BLK_CLK [3:2] : 0-->RUN mode, the bl clock will run always ( el clock is running). 1-->AUTO mode, the local BLCG controllers will determine the state of the clocks.
//  addr = atoi("LW_THERM_GATE_CTRL(0)");//0x00020200; //LW_THERM_GATE_CTRL(0);
  if (!(reg = m_regMap->FindRegister("LW_THERM_GATE_CTRL(0)")))
    {
        ErrPrintf("powerCtrlTest_maxwell: Failed to find LW_THERM_GATE_CTRL(0) register\n");
    }
  addr = reg->GetAddress();
  InfoPrintf("powerCtrlTest_maxwell: addr = 0x%x\n", addr);
  if (engine ==  eng_type.GR) {
    if (mode == CG_FULLPOWER) {
      data = lwgpu->RegRd32(addr + 0x4 * macros.lw_engine_graphics);
      data = (data & 0xfffffff3) | 0x00000000;
      lwgpu->RegWr32(addr + 0x4 * macros.lw_engine_graphics, data);
    } else if (mode == CG_AUTOMATIC) {
      data = lwgpu->RegRd32(addr + 0x4 * macros.lw_engine_graphics);
      data = (data & 0xfffffff3) | 0x00000004;
      lwgpu->RegWr32(addr + 0x4 * macros.lw_engine_graphics, data);
    }
    sprintf(eng_name, "GR");
    }

  if (engine ==  eng_type.CE0) {
    if (mode == CG_FULLPOWER) {
      data = lwgpu->RegRd32(addr + 0x4 * macros.lw_engine_copy0);
      data = (data & 0xfffffff3) | 0x00000000;
      lwgpu->RegWr32(addr + 0x4 * macros.lw_engine_copy0, data);
    } else if (mode == CG_AUTOMATIC) {
      data = lwgpu->RegRd32(addr + 0x4 * macros.lw_engine_copy0);
      data = (data & 0xfffffff3) | 0x00000004;
      lwgpu->RegWr32(addr + 0x4 * macros.lw_engine_copy0, data);
    }
      sprintf(eng_name, "CE0");
    }

  if (engine ==  eng_type.CE1) {
    if (mode == CG_FULLPOWER) {
      data = lwgpu->RegRd32(addr + 0x4 * macros.lw_engine_copy1);
      data = (data & 0xfffffff3) | 0x00000000;
      lwgpu->RegWr32(addr + 0x4 * macros.lw_engine_copy1, data);
    } else if (mode == CG_AUTOMATIC) {
      data = lwgpu->RegRd32(addr + 0x4 * macros.lw_engine_copy1);
      data = (data & 0xfffffff3) | 0x00000004;
      lwgpu->RegWr32(addr + 0x4 * macros.lw_engine_copy1, data);
    }
      sprintf(eng_name, "CE1");
    }

  if (engine ==  eng_type.CE2) {
    if (mode == CG_FULLPOWER) {
      data = lwgpu->RegRd32(addr + 0x4 * macros.lw_engine_copy2);
      data = (data & 0xfffffff3) | 0x00000000;
      lwgpu->RegWr32(addr + 0x4 * macros.lw_engine_copy2, data);
    } else if (mode == CG_AUTOMATIC) {
      data = lwgpu->RegRd32(addr + 0x4 * macros.lw_engine_copy2);
      data = (data & 0xfffffff3) | 0x00000004;
      lwgpu->RegWr32(addr + 0x4 * macros.lw_engine_copy2, data);
    }
      sprintf(eng_name, "CE2");
    }

  if (engine ==  eng_type.CE3) {
    if (mode == CG_FULLPOWER) {
      data = lwgpu->RegRd32(addr + 0x4 * macros.lw_engine_copy3);
      data = (data & 0xfffffff3) | 0x00000000;
      lwgpu->RegWr32(addr + 0x4 * macros.lw_engine_copy3, data);
    } else if (mode == CG_AUTOMATIC) {
      data = lwgpu->RegRd32(addr + 0x4 * macros.lw_engine_copy3);
      data = (data & 0xfffffff3) | 0x00000004;
      lwgpu->RegWr32(addr + 0x4 * macros.lw_engine_copy3, data);
    }
      sprintf(eng_name, "CE3");
    }

  if (engine ==  eng_type.CE4) {
    if (mode == CG_FULLPOWER) {
      data = lwgpu->RegRd32(addr + 0x4 * macros.lw_engine_copy4);
      data = (data & 0xfffffff3) | 0x00000000;
      lwgpu->RegWr32(addr + 0x4 * macros.lw_engine_copy4, data);
    } else if (mode == CG_AUTOMATIC) {
      data = lwgpu->RegRd32(addr + 0x4 * macros.lw_engine_copy4);
      data = (data & 0xfffffff3) | 0x00000004;
      lwgpu->RegWr32(addr + 0x4 * macros.lw_engine_copy4, data);
    }
      sprintf(eng_name, "CE4");
    }

  if (engine ==  eng_type.CE5) {
    if (mode == CG_FULLPOWER) {
      data = lwgpu->RegRd32(addr + 0x4 * macros.lw_engine_copy5);
      data = (data & 0xfffffff3) | 0x00000000;
      lwgpu->RegWr32(addr + 0x4 * macros.lw_engine_copy5, data);
    } else if (mode == CG_AUTOMATIC) {
      data = lwgpu->RegRd32(addr + 0x4 * macros.lw_engine_copy5);
      data = (data & 0xfffffff3) | 0x00000004;
      lwgpu->RegWr32(addr + 0x4 * macros.lw_engine_copy5, data);
    }
      sprintf(eng_name, "CE5");
    }

  if (engine ==  eng_type.CE6) {
    if (mode == CG_FULLPOWER) {
      data = lwgpu->RegRd32(addr + 0x4 * macros.lw_engine_copy6);
      data = (data & 0xfffffff3) | 0x00000000;
      lwgpu->RegWr32(addr + 0x4 * macros.lw_engine_copy6, data);
    } else if (mode == CG_AUTOMATIC) {
      data = lwgpu->RegRd32(addr + 0x4 * macros.lw_engine_copy6);
      data = (data & 0xfffffff3) | 0x00000004;
      lwgpu->RegWr32(addr + 0x4 * macros.lw_engine_copy6, data);
    }
      sprintf(eng_name, "CE6");
    }

  if (engine ==  eng_type.CE7) {
    if (mode == CG_FULLPOWER) {
      data = lwgpu->RegRd32(addr + 0x4 * macros.lw_engine_copy7);
      data = (data & 0xfffffff3) | 0x00000000;
      lwgpu->RegWr32(addr + 0x4 * macros.lw_engine_copy7, data);
    } else if (mode == CG_AUTOMATIC) {
      data = lwgpu->RegRd32(addr + 0x4 * macros.lw_engine_copy7);
      data = (data & 0xfffffff3) | 0x00000004;
      lwgpu->RegWr32(addr + 0x4 * macros.lw_engine_copy7, data);
    }
      sprintf(eng_name, "CE7");
    }

  if (engine ==  eng_type.CE8) {
    if (mode == CG_FULLPOWER) {
      data = lwgpu->RegRd32(addr + 0x4 * macros.lw_engine_copy8);
      data = (data & 0xfffffff3) | 0x00000000;
      lwgpu->RegWr32(addr + 0x4 * macros.lw_engine_copy8, data);
    } else if (mode == CG_AUTOMATIC) {
      data = lwgpu->RegRd32(addr + 0x4 * macros.lw_engine_copy8);
      data = (data & 0xfffffff3) | 0x00000004;
      lwgpu->RegWr32(addr + 0x4 * macros.lw_engine_copy8, data);
    }
      sprintf(eng_name, "CE8");
    }

  if (engine ==  eng_type.CE9) {
    if (mode == CG_FULLPOWER) {
      data = lwgpu->RegRd32(addr + 0x4 * macros.lw_engine_copy9);
      data = (data & 0xfffffff3) | 0x00000000;
      lwgpu->RegWr32(addr + 0x4 * macros.lw_engine_copy9, data);
    } else if (mode == CG_AUTOMATIC) {
      data = lwgpu->RegRd32(addr + 0x4 * macros.lw_engine_copy9);
      data = (data & 0xfffffff3) | 0x00000004;
      lwgpu->RegWr32(addr + 0x4 * macros.lw_engine_copy9, data);
    }
      sprintf(eng_name, "CE9");
    }
  if (engine ==  eng_type.SEC) {
    if (mode == CG_FULLPOWER) {
      data = lwgpu->RegRd32(addr + 0x4 * macros.lw_engine_sec);
      data = (data & 0xfffffff3) | 0x00000000;
      lwgpu->RegWr32(addr + 0x4 * macros.lw_engine_sec, data);
    } else if (mode == CG_AUTOMATIC) {
      data = lwgpu->RegRd32(addr + 0x4 * macros.lw_engine_sec);
      data = (data & 0xfffffff3) | 0x00000004;
      lwgpu->RegWr32(addr + 0x4 * macros.lw_engine_sec, data);
    }
      sprintf(eng_name, "SEC");
    }

  if (engine ==  eng_type.LWDEC) {
    if (mode == CG_FULLPOWER) {
      data = lwgpu->RegRd32(addr + 0x4 * macros.lw_engine_lwdec);
      data = (data & 0xfffffff3) | 0x00000000;
      lwgpu->RegWr32(addr + 0x4 * macros.lw_engine_lwdec, data);
    } else if (mode == CG_AUTOMATIC) {
      data = lwgpu->RegRd32(addr + 0x4 * macros.lw_engine_lwdec);
      data = (data & 0xfffffff3) | 0x00000004;
      lwgpu->RegWr32(addr + 0x4 * macros.lw_engine_lwdec, data);
    }
      sprintf(eng_name, "LWDEC");
    }

 if (engine ==  eng_type.LWENC) {
    if (mode == CG_FULLPOWER) {
      data = lwgpu->RegRd32(addr + 0x4 * macros.lw_engine_lwenc);
      data = (data & 0xfffffff3) | 0x00000000;
      lwgpu->RegWr32(addr + 0x4 * macros.lw_engine_lwenc, data);
    } else if (mode == CG_AUTOMATIC) {
      data = lwgpu->RegRd32(addr + 0x4 * macros.lw_engine_lwenc);
      data = (data & 0xfffffff3) | 0x00000004;
      lwgpu->RegWr32(addr + 0x4 * macros.lw_engine_lwenc, data);

    }
      sprintf(eng_name, "LWENC");
    }

 if (engine ==  eng_type.LWENC1) {
    if (mode == CG_FULLPOWER) {
      data = lwgpu->RegRd32(addr + 0x4 * macros.lw_engine_lwenc1);
      data = (data & 0xfffffff3) | 0x00000000;
      lwgpu->RegWr32(addr + 0x4 * macros.lw_engine_lwenc1, data);
    } else if (mode == CG_AUTOMATIC) {
      data = lwgpu->RegRd32(addr + 0x4 * macros.lw_engine_lwenc1);
      data = (data & 0xfffffff3) | 0x00000004;
      lwgpu->RegWr32(addr + 0x4 * macros.lw_engine_lwenc1, data);
    }
      sprintf(eng_name, "LWENC1");
    }

 if (engine ==  eng_type.LWENC2) {
    if (mode == CG_FULLPOWER) {
      data = lwgpu->RegRd32(addr + 0x4 * macros.lw_engine_lwenc2);
      data = (data & 0xfffffff3) | 0x00000000;
      lwgpu->RegWr32(addr + 0x4 * macros.lw_engine_lwenc2, data);
    } else if (mode == CG_AUTOMATIC) {
      data = lwgpu->RegRd32(addr + 0x4 * macros.lw_engine_lwenc2);
      data = (data & 0xfffffff3) | 0x00000004;
      lwgpu->RegWr32(addr + 0x4 * macros.lw_engine_lwenc2, data);
    }
      sprintf(eng_name, "LWENC2");
    }

 if (engine ==  eng_type.LWDEC1) {
    if (mode == CG_FULLPOWER) {
      data = lwgpu->RegRd32(addr + 0x4 * macros.lw_engine_lwdec1);
      data = (data & 0xfffffff3) | 0x00000000;
      lwgpu->RegWr32(addr + 0x4 * macros.lw_engine_lwdec1, data);
    } else if (mode == CG_AUTOMATIC) {
      data = lwgpu->RegRd32(addr + 0x4 * macros.lw_engine_lwdec1);
      data = (data & 0xfffffff3) | 0x00000004;
      lwgpu->RegWr32(addr + 0x4 * macros.lw_engine_lwdec1, data);
    }
      sprintf(eng_name, "LWDEC1");
    }

 if (engine ==  eng_type.LWDEC2) {
    if (mode == CG_FULLPOWER) {
      data = lwgpu->RegRd32(addr + 0x4 * macros.lw_engine_lwdec2);
      data = (data & 0xfffffff3) | 0x00000000;
      lwgpu->RegWr32(addr + 0x4 * macros.lw_engine_lwdec2, data);
    } else if (mode == CG_AUTOMATIC) {
      data = lwgpu->RegRd32(addr + 0x4 * macros.lw_engine_lwdec2);
      data = (data & 0xfffffff3) | 0x00000004;
      lwgpu->RegWr32(addr + 0x4 * macros.lw_engine_lwdec2, data);
    }
      sprintf(eng_name, "LWDEC2");
    }

 if (engine ==  eng_type.LWDEC3) {
    if (mode == CG_FULLPOWER) {
      data = lwgpu->RegRd32(addr + 0x4 * macros.lw_engine_lwdec3);
      data = (data & 0xfffffff3) | 0x00000000;
      lwgpu->RegWr32(addr + 0x4 * macros.lw_engine_lwdec3, data);
    } else if (mode == CG_AUTOMATIC) {
      data = lwgpu->RegRd32(addr + 0x4 * macros.lw_engine_lwdec3);
      data = (data & 0xfffffff3) | 0x00000004;
      lwgpu->RegWr32(addr + 0x4 * macros.lw_engine_lwdec3, data);
    }
      sprintf(eng_name, "LWDEC3");
    }

 if (engine ==  eng_type.LWDEC4) {
    if (mode == CG_FULLPOWER) {
      data = lwgpu->RegRd32(addr + 0x4 * macros.lw_engine_lwdec4);
      data = (data & 0xfffffff3) | 0x00000000;
      lwgpu->RegWr32(addr + 0x4 * macros.lw_engine_lwdec4, data);
    } else if (mode == CG_AUTOMATIC) {
      data = lwgpu->RegRd32(addr + 0x4 * macros.lw_engine_lwdec4);
      data = (data & 0xfffffff3) | 0x00000004;
      lwgpu->RegWr32(addr + 0x4 * macros.lw_engine_lwdec4, data);
    }
      sprintf(eng_name, "LWDEC4");
    }

if (engine ==  eng_type.LWDEC5) {
    if (mode == CG_FULLPOWER) {
      data = lwgpu->RegRd32(addr + 0x4 * macros.lw_engine_lwdec5);
      data = (data & 0xfffffff3) | 0x00000000;
      lwgpu->RegWr32(addr + 0x4 * macros.lw_engine_lwdec5, data);
    } else if (mode == CG_AUTOMATIC) {
      data = lwgpu->RegRd32(addr + 0x4 * macros.lw_engine_lwdec5);
      data = (data & 0xfffffff3) | 0x00000004;
      lwgpu->RegWr32(addr + 0x4 * macros.lw_engine_lwdec5, data);
    }
      sprintf(eng_name, "LWDEC5");
    }

if (engine ==  eng_type.LWDEC6) {
    if (mode == CG_FULLPOWER) {
      data = lwgpu->RegRd32(addr + 0x4 * macros.lw_engine_lwdec6);
      data = (data & 0xfffffff3) | 0x00000000;
      lwgpu->RegWr32(addr + 0x4 * macros.lw_engine_lwdec6, data);
    } else if (mode == CG_AUTOMATIC) {
      data = lwgpu->RegRd32(addr + 0x4 * macros.lw_engine_lwdec6);
      data = (data & 0xfffffff3) | 0x00000004;
      lwgpu->RegWr32(addr + 0x4 * macros.lw_engine_lwdec6, data);
    }
      sprintf(eng_name, "LWDEC6");
    }

if (engine ==  eng_type.LWDEC7) {
    if (mode == CG_FULLPOWER) {
      data = lwgpu->RegRd32(addr + 0x4 * macros.lw_engine_lwdec7);
      data = (data & 0xfffffff3) | 0x00000000;
      lwgpu->RegWr32(addr + 0x4 * macros.lw_engine_lwdec7, data);
    } else if (mode == CG_AUTOMATIC) {
      data = lwgpu->RegRd32(addr + 0x4 * macros.lw_engine_lwdec7);
      data = (data & 0xfffffff3) | 0x00000004;
      lwgpu->RegWr32(addr + 0x4 * macros.lw_engine_lwdec7, data);
    }
      sprintf(eng_name, "LWDEC7");
    }
 if (engine ==  eng_type.OFA) {
    if (mode == CG_FULLPOWER) {
      data = lwgpu->RegRd32(addr + 0x4 * macros.lw_engine_ofa0);
      data = (data & 0xfffffff3) | 0x00000000;
      lwgpu->RegWr32(addr + 0x4 * macros.lw_engine_ofa0, data);
    } else if (mode == CG_AUTOMATIC) {
      data = lwgpu->RegRd32(addr + 0x4 * macros.lw_engine_ofa0);
      data = (data & 0xfffffff3) | 0x00000004;
      lwgpu->RegWr32(addr + 0x4 * macros.lw_engine_ofa0, data);
    }
      sprintf(eng_name, "OFA");
    }

 if (engine ==  eng_type.LWJPG0) {
    if (mode == CG_FULLPOWER) {
      data = lwgpu->RegRd32(addr + 0x4 * macros.lw_engine_lwjpg0);
      data = (data & 0xfffffff3) | 0x00000000;
      lwgpu->RegWr32(addr + 0x4 * macros.lw_engine_lwjpg0, data);
    } else if (mode == CG_AUTOMATIC) {
      data = lwgpu->RegRd32(addr + 0x4 * macros.lw_engine_lwjpg0);
      data = (data & 0xfffffff3) | 0x00000004;
      lwgpu->RegWr32(addr + 0x4 * macros.lw_engine_lwjpg0, data);
    }
      sprintf(eng_name, "LWJPG0");
    }

 if (engine ==  eng_type.LWJPG1) {
    if (mode == CG_FULLPOWER) {
      data = lwgpu->RegRd32(addr + 0x4 * macros.lw_engine_lwjpg1);
      data = (data & 0xfffffff3) | 0x00000000;
      lwgpu->RegWr32(addr + 0x4 * macros.lw_engine_lwjpg1, data);
    } else if (mode == CG_AUTOMATIC) {
      data = lwgpu->RegRd32(addr + 0x4 * macros.lw_engine_lwjpg1);
      data = (data & 0xfffffff3) | 0x00000004;
      lwgpu->RegWr32(addr + 0x4 * macros.lw_engine_lwjpg1, data);
    }
      sprintf(eng_name, "LWJPG1");
    }

 if (engine ==  eng_type.LWJPG2) {
    if (mode == CG_FULLPOWER) {
      data = lwgpu->RegRd32(addr + 0x4 * macros.lw_engine_lwjpg2);
      data = (data & 0xfffffff3) | 0x00000000;
      lwgpu->RegWr32(addr + 0x4 * macros.lw_engine_lwjpg2, data);
    } else if (mode == CG_AUTOMATIC) {
      data = lwgpu->RegRd32(addr + 0x4 * macros.lw_engine_lwjpg2);
      data = (data & 0xfffffff3) | 0x00000004;
      lwgpu->RegWr32(addr + 0x4 * macros.lw_engine_lwjpg2, data);
    }
      sprintf(eng_name, "LWJPG2");
    }

 if (engine ==  eng_type.LWJPG3) {
    if (mode == CG_FULLPOWER) {
      data = lwgpu->RegRd32(addr + 0x4 * macros.lw_engine_lwjpg3);
      data = (data & 0xfffffff3) | 0x00000000;
      lwgpu->RegWr32(addr + 0x4 * macros.lw_engine_lwjpg3, data);
    } else if (mode == CG_AUTOMATIC) {
      data = lwgpu->RegRd32(addr + 0x4 * macros.lw_engine_lwjpg3);
      data = (data & 0xfffffff3) | 0x00000004;
      lwgpu->RegWr32(addr + 0x4 * macros.lw_engine_lwjpg3, data);
    }
      sprintf(eng_name, "LWJPG3");
    }

 if (engine ==  eng_type.LWJPG4) {
    if (mode == CG_FULLPOWER) {
      data = lwgpu->RegRd32(addr + 0x4 * macros.lw_engine_lwjpg4);
      data = (data & 0xfffffff3) | 0x00000000;
      lwgpu->RegWr32(addr + 0x4 * macros.lw_engine_lwjpg4, data);
    } else if (mode == CG_AUTOMATIC) {
      data = lwgpu->RegRd32(addr + 0x4 * macros.lw_engine_lwjpg4);
      data = (data & 0xfffffff3) | 0x00000004;
      lwgpu->RegWr32(addr + 0x4 * macros.lw_engine_lwjpg4, data);
    }
      sprintf(eng_name, "LWJPG4");
    }

 if (engine ==  eng_type.LWJPG5) {
    if (mode == CG_FULLPOWER) {
      data = lwgpu->RegRd32(addr + 0x4 * macros.lw_engine_lwjpg5);
      data = (data & 0xfffffff3) | 0x00000000;
      lwgpu->RegWr32(addr + 0x4 * macros.lw_engine_lwjpg5, data);
    } else if (mode == CG_AUTOMATIC) {
      data = lwgpu->RegRd32(addr + 0x4 * macros.lw_engine_lwjpg5);
      data = (data & 0xfffffff3) | 0x00000004;
      lwgpu->RegWr32(addr + 0x4 * macros.lw_engine_lwjpg5, data);
    }
      sprintf(eng_name, "LWJPG5");
    }

 if (engine ==  eng_type.LWJPG6) {
    if (mode == CG_FULLPOWER) {
      data = lwgpu->RegRd32(addr + 0x4 * macros.lw_engine_lwjpg6);
      data = (data & 0xfffffff3) | 0x00000000;
      lwgpu->RegWr32(addr + 0x4 * macros.lw_engine_lwjpg6, data);
    } else if (mode == CG_AUTOMATIC) {
      data = lwgpu->RegRd32(addr + 0x4 * macros.lw_engine_lwjpg6);
      data = (data & 0xfffffff3) | 0x00000004;
      lwgpu->RegWr32(addr + 0x4 * macros.lw_engine_lwjpg6, data);
    }
      sprintf(eng_name, "LWJPG6");
    }

 if (engine == eng_type.LWJPG7) {
    if (mode == CG_FULLPOWER) {
      data = lwgpu->RegRd32(addr + 0x4 * macros.lw_engine_lwjpg7);
      data = (data & 0xfffffff3) | 0x00000000;
      lwgpu->RegWr32(addr + 0x4 * macros.lw_engine_lwjpg7, data);
    } else if (mode == CG_AUTOMATIC) {
      data = lwgpu->RegRd32(addr + 0x4 * macros.lw_engine_lwjpg7);
      data = (data & 0xfffffff3) | 0x00000004;
      lwgpu->RegWr32(addr + 0x4 * macros.lw_engine_lwjpg7, data);
    }
      sprintf(eng_name, "LWJPG7");
    }    
  

  if (mode == CG_FULLPOWER) {
    InfoPrintf("powerCtrlTest_maxwell: ForceClksOn %s engine at mode RUN\n", eng_name);
  } else if (mode == CG_AUTOMATIC) {
    InfoPrintf("powerCtrlTest_maxwell: ForceClksOn %s engine at mode AUTO\n", eng_name);
  }
}

// Put all engines into the specified mode (FULLPOWER, AUTOMATIC, DISABLED)
void powerCtrlTest_maxwell::AllEngineMode(LW_ENGINE_MODE eng_mode)
{

  SetEngineMode(eng_type.GR,   eng_mode);
if(macros.lw_scal_litter_num_ce_lces > 0){
  SetEngineMode(eng_type.CE0,  eng_mode);
}
if(macros.lw_scal_litter_num_ce_lces > 1){
  SetEngineMode(eng_type.CE1,  eng_mode);
}
if(macros.lw_scal_litter_num_ce_lces > 2){
  SetEngineMode(eng_type.CE2,  eng_mode);
}
if(macros.lw_scal_litter_num_ce_lces > 3){
  SetEngineMode(eng_type.CE3,  eng_mode);
}
if(macros.lw_scal_litter_num_ce_lces > 4){
  SetEngineMode(eng_type.CE4,  eng_mode);
}
if(macros.lw_scal_litter_num_ce_lces > 5){
  SetEngineMode(eng_type.CE5,  eng_mode);
}
if(macros.lw_scal_litter_num_ce_lces > 6){
  SetEngineMode(eng_type.CE6,  eng_mode);
}
if(macros.lw_scal_litter_num_ce_lces > 7){
  SetEngineMode(eng_type.CE7,  eng_mode);
}
if(macros.lw_scal_litter_num_ce_lces > 8){
  SetEngineMode(eng_type.CE8,  eng_mode);
}
if(macros.lw_scal_litter_num_ce_lces > 9){
  SetEngineMode(eng_type.CE9,  eng_mode);
}
if(macros.lw_chip_lwdec){
  SetEngineMode(eng_type.LWDEC,  eng_mode);
  if (macros.lw_scal_litter_num_lwdecs > 1){
      SetEngineMode(eng_type.LWDEC1, eng_mode);
  }
  if (macros.lw_scal_litter_num_lwdecs > 2){
      SetEngineMode(eng_type.LWDEC2, eng_mode);
  }
  if (macros.lw_scal_litter_num_lwdecs > 3){
      SetEngineMode(eng_type.LWDEC3, eng_mode);
  }
  if (macros.lw_scal_litter_num_lwdecs > 4){
      SetEngineMode(eng_type.LWDEC4, eng_mode);
  }
  if (macros.lw_scal_litter_num_lwdecs > 5){
      SetEngineMode(eng_type.LWDEC5, eng_mode);
  }
  if (macros.lw_scal_litter_num_lwdecs > 6){
      SetEngineMode(eng_type.LWDEC6, eng_mode);
  }
  if (macros.lw_scal_litter_num_lwdecs > 7){
      SetEngineMode(eng_type.LWDEC7, eng_mode);
  }
}
if(macros.lw_chip_lwenc){
  SetEngineMode(eng_type.LWENC,  eng_mode);
  //some chip may have two lwencs.
  if (macros.lw_scal_litter_num_lwencs > 1){
    SetEngineMode(eng_type.LWENC1,  eng_mode);
  }
  if (macros.lw_scal_litter_num_lwencs > 2){
    SetEngineMode(eng_type.LWENC2,  eng_mode);
  }
}
if (macros.lw_chip_lwjpg) {
if (macros.lw_scal_litter_num_lwjpgs > 0){        
    SetEngineMode(eng_type.LWJPG0, eng_mode);}
if (macros.lw_scal_litter_num_lwjpgs > 1){
    SetEngineMode(eng_type.LWJPG1, eng_mode);}
if (macros.lw_scal_litter_num_lwjpgs > 2){
    SetEngineMode(eng_type.LWJPG2, eng_mode);}
if (macros.lw_scal_litter_num_lwjpgs > 3){
    SetEngineMode(eng_type.LWJPG3, eng_mode);}
if (macros.lw_scal_litter_num_lwjpgs > 4){
    SetEngineMode(eng_type.LWJPG4, eng_mode);}
if (macros.lw_scal_litter_num_lwjpgs > 5){
    SetEngineMode(eng_type.LWJPG5, eng_mode);}
if (macros.lw_scal_litter_num_lwjpgs > 6){
    SetEngineMode(eng_type.LWJPG6, eng_mode);}
if (macros.lw_scal_litter_num_lwjpgs > 7){
    SetEngineMode(eng_type.LWJPG7, eng_mode);}      
}
if (macros.lw_chip_ofa) {
    SetEngineMode(eng_type.OFA, eng_mode);
}
if(macros.lw_chip_sec){
  SetEngineMode(eng_type.SEC,  eng_mode);
}
  InfoPrintf("powerCtrlTest_maxwell: AllEngineMode set all engines to eng_mode %d\n", eng_mode);
}

void powerCtrlTest_maxwell::ResetELCGClkCounters()
{
    if (Platform::EscapeWrite("pwr_test_clk_mon_reset_", 0, 1, 0x0)) {
        ErrPrintf ("ResetELCGClkCounters: Can't reset ELCG clock monitor");
    }
    DelayNs(100);
    if (Platform::EscapeWrite("pwr_test_clk_mon_reset_", 0, 1, 0x1)) {
        ErrPrintf ("ResetELCGClkCounters: Can't release ELCG Clock monitor reset");
    }
}

void powerCtrlTest_maxwell::EnableELCGClkCounters()
{
    if (Platform::EscapeWrite("pwr_test_clk_mon_en", 0, 1, 0x1)) {
        ErrPrintf ("EnableELCGClkCounters: Can't enable ELCG Clock monitor");
    }
}

// Read all the clock counts that will be checked ELCG.
MaxwellClkCount powerCtrlTest_maxwell::ReadELCGClkCounters()
{
    UINT32 count;
    unsigned int i;
    MaxwellClkCount clkCount;

    DebugPrintf("powerCtrlTest_maxwell: Reading Engine Counters\n");
    if (Platform::GetSimulationMode() != Platform::RTL)
    {
        InfoPrintf("powerCtrlTest_maxwell: ReadELCGClkCounters not supported on non-RTL architectures.  Defaulting to 0xdeadbeef\n");
    }
    for (i = 0; i < NumMappedClocks_maxwell(); i++) 
    {
        if (Platform::GetSimulationMode() == Platform::RTL) 
        {
            Platform::EscapeRead(GetRTLMappedClock_maxwell(i).map_name_clk_count,0x00,4,&count);
            clkCount.clk_elcg[i].count = count;
        } 
        else 
        {
            clkCount.clk_elcg[i].period = 0xdeadbeef;
            clkCount.clk_elcg[i].count = 0xdeadbeef;
        }
    }
    return clkCount;
}

// Print all of the clock counts in the provided record
void powerCtrlTest_maxwell::printEngineClkCounters(MaxwellClkCount clkCount)
{
  InfoPrintf("powerCtrlTest_maxwell: --Print Clk Counts--\n");
    for (UINT32 i=0; i < NumMappedClocks_maxwell(); i++) {
        InfoPrintf("%s = %d; \n", GetRTLMappedClock_maxwell(i).map_name_clk_count, clkCount.clk_elcg[i].count);
    }
  InfoPrintf("--End of Print Clk Counts--\n");
}

// Substract to clock count records
MaxwellClkCount powerCtrlTest_maxwell::SubtractEngineClkCounters(MaxwellClkCount clk_count_start, MaxwellClkCount clk_count_stop)
{
  DebugPrintf("powerCtrlTest_maxwell: Subtracting Engine Counters\n");
  MaxwellClkCount clk_count;

  for (UINT32 i=0; i < NumMappedClocks_maxwell(); i++) {
      clk_count.clk_elcg[i].count = clk_count_stop.clk_elcg[i].count - clk_count_start.clk_elcg[i].count;
  }
  return clk_count;
}

// Check clock counts for the provided mode
void powerCtrlTest_maxwell::checkClkGating(MaxwellClkCount clkcount, bool suspended_or_disabled, unsigned long long int gating_type, bool check_complement)
{
    InfoPrintf("powerCtrlTest_maxwell::checkClkGating, checking clocks, gated = %d, type = 0x%08x, check_comp = %d\n",
    suspended_or_disabled, gating_type, check_complement);

    // Following clocks should be gated
    for (UINT32 i=0; i < NumMappedClocks_maxwell(); i++) {
        if (suspended_or_disabled) {
            // if clk's gating type matches, it should be gated
            if (GetRTLMappedClock_maxwell(i).gating_type & gating_type) {
                if (clkcount.clk_elcg[i].count != 0) {
                    ErrPrintf("-3-$powerCtrlTest_maxwell: Clk %s should be gated, type: 0x%08x, but is not. Clk cnt = %d\n",
                    GetRTLMappedClock_maxwell(i).map_name_clk_count,GetRTLMappedClock_maxwell(i).gating_type, clkcount.clk_elcg[i].count);
                    test_status_fail = true;
                } else {
                    InfoPrintf("%s clock %s is gated as expected\n", __FUNCTION__, GetRTLMappedClock_maxwell(i).map_name_clk_count);
                }
            }
            if (!(GetRTLMappedClock_maxwell(i).gating_type & gating_type) & check_complement) {
                // If clk's gating type doesnt match, check that it isnt gated
                if (clkcount.clk_elcg[i].count == 0) {
                    ErrPrintf("-0-powerCtrlTest_maxwell: Clk %s should not be gated, type 0x%08x, but Clk cnt = 0\n",
                    GetRTLMappedClock_maxwell(i).map_name_clk_count, GetRTLMappedClock_maxwell(i).gating_type);
                    test_status_fail = true;
                } else {
                    InfoPrintf("%s clock %s is not-gated as expected\n", __FUNCTION__, GetRTLMappedClock_maxwell(i).map_name_clk_count);
                }
            }
        } else {
            // if clk's gating type matches, it should not be gated
            if (GetRTLMappedClock_maxwell(i).gating_type & gating_type) {
                //InfoPrintf("powerCtrlTest_maxwell: Clk %s should not be gated, type: %d, Clk cnt = %d\n",
                if (clkcount.clk_elcg[i].count == 0) {
                    ErrPrintf("-1-$powerCtrlTest_maxwell: Clk %s should not be gated, type 0x%08x, but Clk cnt = 0\n",
                    GetRTLMappedClock_maxwell(i).map_name_clk_count, GetRTLMappedClock_maxwell(i).gating_type);
                    test_status_fail = true;
                } else {
                    InfoPrintf("%s, clock %s is not gated as expected\n", __FUNCTION__, GetRTLMappedClock_maxwell(i).map_name_clk_count);
                }
            }
            if (!(GetRTLMappedClock_maxwell(i).gating_type & gating_type) & check_complement) {
                // If clk's gating doesnt match, it should be gated (unlikely test, but put
                // in for completeness)
                if (clkcount.clk_elcg[i].count != 0) {
                    ErrPrintf("-2-powerCtrlTest_maxwell: Clk %s should be gated, type 0x%08x, but is not. Clk cnt = %d\n",
                    GetRTLMappedClock_maxwell(i).map_name_clk_count,GetRTLMappedClock_maxwell(i).gating_type, clkcount.clk_elcg[i].count);
                    test_status_fail = true;
                }else{
                    InfoPrintf("%s, clock %s is gated as expected\n", __FUNCTION__, GetRTLMappedClock_maxwell(i).map_name_clk_count);
                }
            }
        }
    }
}
/*
 * Test function: testSMCFloorsweep
 *  - Verify when SMC is floorsweep the GR ELCG is not impact
 *  Set SMC floorsweep. Then wait GR engine to be idle. Check ELCG clocks are gated.
 *
 */
void powerCtrlTest_maxwell::testSMCFloorsweep(void)
{

  unsigned long long int gatedType, loop;
  UINT32 smc_floorsweep = 0;
  MaxwellClkCount fullpower_clk_count;
  MaxwellClkCount clk_count_1, clk_count_2, clk_count;
  MaxwellClkCount tmp_clkcount1, tmp_clkcount2, base_clkcount;
  bool Gated = true;
  bool NotGated = false;
  bool CheckComplement = true;
  bool DontCheckComplement = false;

  InfoPrintf("%s: Testing SMC Floorsweep.\n", __FUNCTION__);

  //////////////////////////////////////////////////////
  // Set FULLPOWER and verify that clocks ticks
  // Test GR engines

  gatedType = maxwell_clock_gating.MAXWELL_CLOCK_GR_ENGINE_GATED;

  InfoPrintf("%s: Putting GR engine in FULLPOWER mode and verifying all clks are NOT gated due to idle\n", __FUNCTION__);
  if (!m_ch->WaitForIdle()) {
    ErrPrintf("%s: Channel WaitForIdle failed\n", __FUNCTION__);
    return;
  }
  // Turn off idle clk slowdown
  DisableclkSlowdown();
  // Set engines to full power
  SetEngineMode(eng_type.GR, CG_FULLPOWER);
  // Disable all BLCG
  AllEngineForceClksOn(CG_FULLPOWER);
  EnableELCGClkCounters();

  powerCtrlTest_maxwell::DelayNs(10000);
  ResetELCGClkCounters();
  InfoPrintf("%s: 1nd reading counters\n", __FUNCTION__);
  clk_count_1 = ReadELCGClkCounters();
  powerCtrlTest_maxwell::DelayNs(150);
  InfoPrintf("%s: 2nd reading counters\n", __FUNCTION__);
  clk_count_2 = ReadELCGClkCounters();
  ResetELCGClkCounters();
  fullpower_clk_count = SubtractEngineClkCounters(clk_count_1, clk_count_2);
  printEngineClkCounters(fullpower_clk_count);
  // Make sure clks of all engine types are NOT suspended
  checkClkGating(fullpower_clk_count, NotGated, gatedType, DontCheckComplement);
  InfoPrintf("%s: done checking clocks for all engines FULLPOWER\n", __FUNCTION__);
  
  for (loop = 0; loop < 5; loop++) {
      srand(m_arch+loop); // Use chip family as the random seed.
      smc_floorsweep = 0; // Assume the SMC0 should never be floorsweep
    if (macros.lw_scal_litter_max_num_smc_engines > 2)
        smc_floorsweep = smc_floorsweep | rand () % 2 << 1;
    if (macros.lw_scal_litter_max_num_smc_engines > 3)
        smc_floorsweep = smc_floorsweep | rand () % 2 << 2;
    if (macros.lw_scal_litter_max_num_smc_engines > 4)
        smc_floorsweep = smc_floorsweep | rand () % 2 << 3;
    if (macros.lw_scal_litter_max_num_smc_engines > 5)
        smc_floorsweep = smc_floorsweep | rand () % 2 << 4;
    if (macros.lw_scal_litter_max_num_smc_engines > 6)
        smc_floorsweep = smc_floorsweep | rand () % 2 << 5;
    if (macros.lw_scal_litter_max_num_smc_engines > 7)
        smc_floorsweep = smc_floorsweep | rand () % 2 << 6;
    if (macros.lw_scal_litter_max_num_smc_engines > 8)
        smc_floorsweep = smc_floorsweep | rand () % 2 << 7;

    Platform::EscapeWrite("fs2all_sys_pipe_disable", 0, macros.lw_scal_litter_max_num_smc_engines, smc_floorsweep);
    InfoPrintf("%s: loop %d, smc_floorsweep is se to 0x%x\n", __FUNCTION__, loop, smc_floorsweep);

    //////////////////////////////////////////////////////
    // Put GR engine in AUTOMATIC and verify clocks

    InfoPrintf("%s:  putting all engines in AUTOMATIC mode and verifying all clks are gated due to idle\n", __FUNCTION__);
    if (!m_ch->WaitForIdle()) {
        ErrPrintf("%s: Channel WaitForIdle failed\n", __FUNCTION__);
        return;
    }
    SetEngineMode(eng_type.GR, CG_AUTOMATIC);

    powerCtrlTest_maxwell::DelayNs(10000);
    ResetELCGClkCounters();
    InfoPrintf("%s: 1nd reading counters\n", __FUNCTION__);
    clk_count_1 = ReadELCGClkCounters();
	//printEngineClkCounters(clk_count_1); for debug use
    powerCtrlTest_maxwell::DelayNs(150);
    InfoPrintf("%s: 2nd reading counters\n", __FUNCTION__);
    clk_count_2 = ReadELCGClkCounters();
	//printEngineClkCounters(clk_count_2); for debug use
    ResetELCGClkCounters();
    clk_count = SubtractEngineClkCounters(clk_count_1, clk_count_2);
    printEngineClkCounters(clk_count);
    checkClkGating(clk_count, Gated, gatedType, CheckComplement);
    InfoPrintf("%s:  done checking clocks for all engines AUTOMATIC\n", __FUNCTION__);

    //////////////////////////////////////////////////////
    // Put all engines in FULL POWER and verify clocks are running
    SetEngineMode(eng_type.GR, CG_FULLPOWER);
    powerCtrlTest_maxwell::DelayNs(10000);
    ResetELCGClkCounters();
    InfoPrintf("%s: 1nd reading counters\n", __FUNCTION__);
    clk_count_1 = ReadELCGClkCounters();
    powerCtrlTest_maxwell::DelayNs(150);
    InfoPrintf("%s: 2nd reading counters\n", __FUNCTION__);
    clk_count_2 = ReadELCGClkCounters();
    ResetELCGClkCounters();
    fullpower_clk_count = SubtractEngineClkCounters(clk_count_1, clk_count_2);
    printEngineClkCounters(fullpower_clk_count);
    // Make sure clks of all engine types are NOT suspended
    checkClkGating(fullpower_clk_count, NotGated, gatedType, DontCheckComplement);
    InfoPrintf("%s: done checking clocks for all engines FULLPOWER\n", __FUNCTION__);
  }

}


/*
 * Test function: testPerEngineClkGating
 *  - Verify that each engine in the chip gates the clocks when the individual
 * engine is put into automatic/suspended/disabled.
 * config GATE_CTRL register in pwr/therm
 *
 */
void powerCtrlTest_maxwell::testPerEngineClkGating(void)
{

  unsigned long long int gatedType;
  MaxwellClkCount fullpower_clk_count;
  MaxwellClkCount clk_count_1, clk_count_2, clk_count;
  MaxwellClkCount tmp_clkcount1, tmp_clkcount2, base_clkcount;
  bool Gated = true;
  bool NotGated = false;
  bool CheckComplement = true;
  bool DontCheckComplement = false;

  InfoPrintf("powerCtrlTest_maxwell: Testing Per Engine Clock Gating.\n");

  //////////////////////////////////////////////////////
  // Run all engines in FULLPOWER and verify that clocks ticks
  // Test ALL engines

  InfoPrintf("powerCtrlTest_maxwell: testPerEngine, putting all engines in FULLPOWER mode and verifying all clks are NOT gated due to idle\n");
  if (!m_ch->WaitForIdle()) {
    ErrPrintf("powerCtrlTest_maxwell Channel WaitForIdle failed\n");
    return;
  }
  // Turn off idle clk slowdown
  DisableclkSlowdown();
  // Set engines to full power
  AllEngineMode(CG_FULLPOWER);
  // Disable all BLCG
  AllEngineForceClksOn(CG_FULLPOWER);
  EnableELCGClkCounters();

  powerCtrlTest_maxwell::DelayNs(50000);
  ResetELCGClkCounters();
  InfoPrintf("powerCtrlTest_maxwell: 1nd reading counters\n");
  clk_count_1 = ReadELCGClkCounters();
  powerCtrlTest_maxwell::DelayNs(150);
  InfoPrintf("powerCtrlTest_maxwell: 2nd reading counters\n");
  clk_count_2 = ReadELCGClkCounters();
  ResetELCGClkCounters();
  fullpower_clk_count = SubtractEngineClkCounters(clk_count_1, clk_count_2);
  printEngineClkCounters(fullpower_clk_count);
  // Make sure clks of all engine types are NOT suspended
  checkClkGating(fullpower_clk_count, NotGated, allELCGGatedType, DontCheckComplement);
  InfoPrintf("powerCtrlTest_maxwell: testPerEngine, done checking clocks for all engines FULLPOWER\n");

    //////////////////////////////////////////////////////
    // Put all engines in AUTOMATIC and verify clocks except disp

    InfoPrintf("powerCtrlTest_maxwell: testPerEngine, putting all engines in AUTOMATIC mode and verifying all clks are gated due to idle\n");
    if (!m_ch->WaitForIdle()) {
        ErrPrintf("powerCtrlTest_maxwell: Channel WaitForIdle failed\n");
        return;
    }
    AllEngineMode(CG_AUTOMATIC);

    powerCtrlTest_maxwell::DelayNs(50000);
    ResetELCGClkCounters();
    InfoPrintf("powerCtrlTest_maxwell: 1nd reading counters\n");
    clk_count_1 = ReadELCGClkCounters();
    powerCtrlTest_maxwell::DelayNs(150);
    InfoPrintf("powerCtrlTest_maxwell: 2nd reading counters\n");
    clk_count_2 = ReadELCGClkCounters();
    ResetELCGClkCounters();
    clk_count = SubtractEngineClkCounters(clk_count_1, clk_count_2);
    printEngineClkCounters(clk_count);
    checkClkGating(clk_count, Gated, allELCGGatedType, CheckComplement);
    InfoPrintf("powerCtrlTest_maxwell: testPerEngine, done checking clocks for all engines AUTOMATIC\n");

    //////////////////////////////////////////////////////
    // Put all engines in DISABLED and verify clocks
    InfoPrintf("powerCtrlTest_maxwell: testPerEngine, putting all engines in DISABLED mode and verifying all clks are gated\n");
    if (!m_ch->WaitForIdle()) {
        ErrPrintf("powerCtrlTest_maxwell: Channel WaitForIdle failed\n");
        return;
    }

/* Start from Pascal we can't turn GR ELCG to CG_DISABLED since all priv logic is based on GR ELCG clock. Skip this check.
    AllEngineMode(CG_DISABLED);
    powerCtrlTest_maxwell::DelayNs(50000);     // Give time for engine to go idle.
    ResetELCGClkCounters();
    InfoPrintf("powerCtrlTest_maxwell: 1nd reading counters\n");
    clk_count_1 = ReadELCGClkCounters();
    powerCtrlTest_maxwell::DelayNs(150);
    InfoPrintf("powerCtrlTest_maxwell: 2nd reading counters\n");
    clk_count_2 = ReadELCGClkCounters();
    ResetELCGClkCounters();
    clk_count = SubtractEngineClkCounters(clk_count_1, clk_count_2);
    printEngineClkCounters(clk_count);
    // Make sure clks of all engine types ARE suspended
    checkClkGating(clk_count, Gated, allELCGGatedType, CheckComplement);
    InfoPrintf("powerCtrlTest_maxwell: testPerEngine, done checking clocks for all engines DISABLED\n");
*/
    //////////////////////////////////////////////////////////////////////////
    // Put each engine individually into AUTOMATIC and verify.
    InfoPrintf("powerCtrlTest_maxwell: testPerEngine, putting each engine individually into AUTOMATIC\n");

    // First turn all engines on
    AllEngineMode(CG_FULLPOWER);

    // Turn each engine on individually to AUTOMATIC except DISP
    for (UINT32 tmp_count=0; tmp_count < macros.lw_host_max_num_engines; tmp_count++) {
        if (!m_ch->WaitForIdle()) {
            ErrPrintf("powerCtrlTest_maxwell: Channel WaitForIdle failed\n");
            return;
        }
        gatedType = 0;
        if (tmp_count == 0) {
            InfoPrintf("powerCtrlTest_maxwell: testPerEngine checking GR in AUTOMATIC\n");
            SetEngineMode(eng_type.GR, CG_AUTOMATIC);
            gatedType = maxwell_clock_gating.MAXWELL_CLOCK_GR_ENGINE_GATED;
        }else if (tmp_count == macros.lw_engine_lwdec) {
            InfoPrintf("powerCtrlTest_maxwell: testPerEngine checking LWDEC in AUTOMATIC\n");
            SetEngineMode(eng_type.LWDEC, CG_AUTOMATIC);
            gatedType = maxwell_clock_gating.MAXWELL_CLOCK_LWDEC_ENGINE_GATED;
        }else if (tmp_count == macros.lw_engine_lwdec1 && macros.lw_scal_chip_num_lwdecs > 1) {
            InfoPrintf("powerCtrlTest_maxwell: testPerEngine checking LWDEC1 in AUTOMATIC\n");
            SetEngineMode(eng_type.LWDEC1, CG_AUTOMATIC);
            gatedType = maxwell_clock_gating.MAXWELL_CLOCK_LWDEC1_ENGINE_GATED;
        }else if (tmp_count == macros.lw_engine_lwdec2 && macros.lw_scal_chip_num_lwdecs > 2) {
            InfoPrintf("powerCtrlTest_maxwell: testPerEngine checking LWDEC2 in AUTOMATIC\n");
            SetEngineMode(eng_type.LWDEC2, CG_AUTOMATIC);
            gatedType = maxwell_clock_gating.MAXWELL_CLOCK_LWDEC2_ENGINE_GATED;
        }else if (tmp_count == macros.lw_engine_lwdec3 && macros.lw_scal_chip_num_lwdecs > 3) {
            InfoPrintf("powerCtrlTest_maxwell: testPerEngine checking LWDEC3 in AUTOMATIC\n");
            SetEngineMode(eng_type.LWDEC3, CG_AUTOMATIC);
            gatedType = maxwell_clock_gating.MAXWELL_CLOCK_LWDEC3_ENGINE_GATED;
        }else if (tmp_count == macros.lw_engine_lwdec4 && macros.lw_scal_chip_num_lwdecs > 4) {
            InfoPrintf("powerCtrlTest_maxwell: testPerEngine checking LWDEC4 in AUTOMATIC\n");
            SetEngineMode(eng_type.LWDEC4, CG_AUTOMATIC);
            gatedType = maxwell_clock_gating.MAXWELL_CLOCK_LWDEC4_ENGINE_GATED;
        }else if (tmp_count == macros.lw_engine_lwdec5 && macros.lw_scal_litter_num_lwdecs > 5) {
            InfoPrintf("powerCtrlTest_maxwell: testPerEngine checking LWDEC5 in AUTOMATIC\n");
            SetEngineMode(eng_type.LWDEC5, CG_AUTOMATIC);
            gatedType = maxwell_clock_gating.MAXWELL_CLOCK_LWDEC5_ENGINE_GATED;
        }else if (tmp_count == macros.lw_engine_lwdec6 && macros.lw_scal_litter_num_lwdecs > 6) {
            InfoPrintf("powerCtrlTest_maxwell: testPerEngine checking LWDEC6 in AUTOMATIC\n");
            SetEngineMode(eng_type.LWDEC6, CG_AUTOMATIC);
            gatedType = maxwell_clock_gating.MAXWELL_CLOCK_LWDEC6_ENGINE_GATED;
        }else if (tmp_count == macros.lw_engine_lwdec7 && macros.lw_scal_litter_num_lwdecs > 7) {
            InfoPrintf("powerCtrlTest_maxwell: testPerEngine checking LWDEC7 in AUTOMATIC\n");
            SetEngineMode(eng_type.LWDEC7, CG_AUTOMATIC);
            gatedType = maxwell_clock_gating.MAXWELL_CLOCK_LWDEC7_ENGINE_GATED;    
        }else if (tmp_count == macros.lw_engine_lwenc) {
            InfoPrintf("powerCtrlTest_maxwell: testPerEngine checking LWENC in AUTOMATIC\n");
            SetEngineMode(eng_type.LWENC, CG_AUTOMATIC);
            gatedType = maxwell_clock_gating.MAXWELL_CLOCK_LWENC_ENGINE_GATED;
        }else if (tmp_count == macros.lw_engine_lwenc1 && macros.lw_scal_chip_num_lwencs > 1) {
            InfoPrintf("powerCtrlTest_maxwell: testPerEngine checking LWENC1 in AUTOMATIC\n");
            SetEngineMode(eng_type.LWENC1, CG_AUTOMATIC);
            gatedType = maxwell_clock_gating.MAXWELL_CLOCK_LWENC1_ENGINE_GATED;
        }else if (tmp_count == macros.lw_engine_lwenc2 && macros.lw_scal_chip_num_lwencs > 2) {
            InfoPrintf("powerCtrlTest_maxwell: testPerEngine checking LWENC2 in AUTOMATIC\n");
            SetEngineMode(eng_type.LWENC2, CG_AUTOMATIC);
            gatedType = maxwell_clock_gating.MAXWELL_CLOCK_LWENC2_ENGINE_GATED;
        }else if (tmp_count == macros.lw_engine_ofa0) {
            InfoPrintf("powerCtrlTest_maxwell: testPerEngine checking OFA in AUTOMATIC\n");
            SetEngineMode(eng_type.OFA, CG_AUTOMATIC);
            gatedType = maxwell_clock_gating.MAXWELL_CLOCK_OFA_ENGINE_GATED;
        }else if (tmp_count == macros.lw_engine_sec) {
            InfoPrintf("powerCtrlTest_maxwell: testPerEngine checking SEC in AUTOMATIC\n");
            SetEngineMode(eng_type.SEC, CG_AUTOMATIC);
            gatedType = maxwell_clock_gating.MAXWELL_CLOCK_SEC_ENGINE_GATED;
        }else if (tmp_count == macros.lw_engine_lwjpg0) {
            InfoPrintf("powerCtrlTest_maxwell: testPerEngine checking LWJPG0 in AUTOMATIC\n");
            SetEngineMode(eng_type.LWJPG0, CG_AUTOMATIC);
            gatedType = maxwell_clock_gating.MAXWELL_CLOCK_LWJPG0_ENGINE_GATED;
        }else if (tmp_count == macros.lw_engine_lwjpg1) {
            InfoPrintf("powerCtrlTest_maxwell: testPerEngine checking LWJPG1 in AUTOMATIC\n");
            SetEngineMode(eng_type.LWJPG1, CG_AUTOMATIC);
            gatedType = maxwell_clock_gating.MAXWELL_CLOCK_LWJPG1_ENGINE_GATED;
        }else if (tmp_count == macros.lw_engine_lwjpg2) {
            InfoPrintf("powerCtrlTest_maxwell: testPerEngine checking LWJPG2 in AUTOMATIC\n");
            SetEngineMode(eng_type.LWJPG2, CG_AUTOMATIC);
            gatedType = maxwell_clock_gating.MAXWELL_CLOCK_LWJPG2_ENGINE_GATED;
        }else if (tmp_count == macros.lw_engine_lwjpg3) {
            InfoPrintf("powerCtrlTest_maxwell: testPerEngine checking LWJPG3 in AUTOMATIC\n");
            SetEngineMode(eng_type.LWJPG3, CG_AUTOMATIC);
            gatedType = maxwell_clock_gating.MAXWELL_CLOCK_LWJPG3_ENGINE_GATED;
        }else if (tmp_count == macros.lw_engine_lwjpg4) {
            InfoPrintf("powerCtrlTest_maxwell: testPerEngine checking LWJPG4 in AUTOMATIC\n");
            SetEngineMode(eng_type.LWJPG4, CG_AUTOMATIC);
            gatedType = maxwell_clock_gating.MAXWELL_CLOCK_LWJPG4_ENGINE_GATED;
        }else if (tmp_count == macros.lw_engine_lwjpg5) {
            InfoPrintf("powerCtrlTest_maxwell: testPerEngine checking LWJPG5 in AUTOMATIC\n");
            SetEngineMode(eng_type.LWJPG5, CG_AUTOMATIC);
            gatedType = maxwell_clock_gating.MAXWELL_CLOCK_LWJPG5_ENGINE_GATED;
        }else if (tmp_count == macros.lw_engine_lwjpg6) {
            InfoPrintf("powerCtrlTest_maxwell: testPerEngine checking LWJPG6 in AUTOMATIC\n");
            SetEngineMode(eng_type.LWJPG6, CG_AUTOMATIC);
            gatedType = maxwell_clock_gating.MAXWELL_CLOCK_LWJPG6_ENGINE_GATED;
        }else if (tmp_count == macros.lw_engine_lwjpg7) {
            InfoPrintf("powerCtrlTest_maxwell: testPerEngine checking LWJPG7 in AUTOMATIC\n");
            SetEngineMode(eng_type.LWJPG7, CG_AUTOMATIC);
            gatedType = maxwell_clock_gating.MAXWELL_CLOCK_LWJPG7_ENGINE_GATED;    
        }else if ((tmp_count == macros.lw_engine_copy0) && (macros.lw_scal_litter_num_ce_lces > 0) && (macros.lw_scal_litter_num_ce_pces > 0)){
            InfoPrintf("powerCtrlTest_maxwell: testPerEngine checking CE0 in AUTOMATIC\n");
            SetEngineMode(eng_type.CE0, CG_AUTOMATIC);
            gatedType = maxwell_clock_gating.MAXWELL_CLOCK_CE0_ENGINE_GATED;
        }else if ((tmp_count == macros.lw_engine_copy1) && (macros.lw_scal_litter_num_ce_lces > 1) && (macros.lw_scal_litter_num_ce_pces > 1)){
            InfoPrintf("powerCtrlTest_maxwell: testPerEngine checking CE1 in AUTOMATIC\n");
            SetEngineMode(eng_type.CE1, CG_AUTOMATIC);
            gatedType = maxwell_clock_gating.MAXWELL_CLOCK_CE1_ENGINE_GATED;
        }else if ((tmp_count == macros.lw_engine_copy2) && (macros.lw_scal_litter_num_ce_lces > 2) && (macros.lw_scal_litter_num_ce_pces > 2)){
            InfoPrintf("powerCtrlTest_maxwell: testPerEngine checking CE2 in AUTOMATIC\n");
            SetEngineMode(eng_type.CE2, CG_AUTOMATIC);
            gatedType = maxwell_clock_gating.MAXWELL_CLOCK_CE2_ENGINE_GATED;
        }else if ((tmp_count == macros.lw_engine_copy3) && (macros.lw_scal_litter_num_ce_lces > 3) && (macros.lw_scal_litter_num_ce_pces > 3)){
            InfoPrintf("powerCtrlTest_maxwell: testPerEngine checking CE3 in AUTOMATIC\n");
            SetEngineMode(eng_type.CE3, CG_AUTOMATIC);
            gatedType = maxwell_clock_gating.MAXWELL_CLOCK_CE3_ENGINE_GATED;
        }else if ((tmp_count == macros.lw_engine_copy4) && (macros.lw_scal_litter_num_ce_lces > 4) && (macros.lw_scal_litter_num_ce_pces > 4)){
            InfoPrintf("powerCtrlTest_maxwell: testPerEngine checking CE4 in AUTOMATIC\n");
            SetEngineMode(eng_type.CE4, CG_AUTOMATIC);
            gatedType = maxwell_clock_gating.MAXWELL_CLOCK_CE4_ENGINE_GATED;
        }else if ((tmp_count == macros.lw_engine_copy5) && (macros.lw_scal_litter_num_ce_lces > 5) && (macros.lw_scal_litter_num_ce_pces > 5)){
            InfoPrintf("powerCtrlTest_maxwell: testPerEngine checking CE5 in AUTOMATIC\n");
            SetEngineMode(eng_type.CE5, CG_AUTOMATIC);
            gatedType = maxwell_clock_gating.MAXWELL_CLOCK_CE5_ENGINE_GATED;
        }else if ((tmp_count == macros.lw_engine_copy6) && (macros.lw_scal_litter_num_ce_lces > 6) && (macros.lw_scal_litter_num_ce_pces > 6)){
            InfoPrintf("powerCtrlTest_maxwell: testPerEngine checking CE6 in AUTOMATIC\n");
            SetEngineMode(eng_type.CE6, CG_AUTOMATIC);
            gatedType = maxwell_clock_gating.MAXWELL_CLOCK_CE6_ENGINE_GATED;
        }else if ((tmp_count == macros.lw_engine_copy7) && (macros.lw_scal_litter_num_ce_lces > 7) && (macros.lw_scal_litter_num_ce_pces > 7)){
            InfoPrintf("powerCtrlTest_maxwell: testPerEngine checking CE7 in AUTOMATIC\n");
            SetEngineMode(eng_type.CE7, CG_AUTOMATIC);
            gatedType = maxwell_clock_gating.MAXWELL_CLOCK_CE7_ENGINE_GATED;
        }else if ((tmp_count == macros.lw_engine_copy8) && (macros.lw_scal_litter_num_ce_lces > 8) && (macros.lw_scal_litter_num_ce_pces > 8)){
            InfoPrintf("powerCtrlTest_maxwell: testPerEngine checking CE8 in AUTOMATIC\n");
            SetEngineMode(eng_type.CE8, CG_AUTOMATIC);
            gatedType = maxwell_clock_gating.MAXWELL_CLOCK_CE8_ENGINE_GATED;
        }else if ((tmp_count == macros.lw_engine_copy9) && (macros.lw_scal_litter_num_ce_lces > 9) && (macros.lw_scal_litter_num_ce_pces > 9)){
            InfoPrintf("powerCtrlTest_maxwell: testPerEngine checking CE9 in AUTOMATIC\n");
            SetEngineMode(eng_type.CE9, CG_AUTOMATIC);
            gatedType = maxwell_clock_gating.MAXWELL_CLOCK_CE9_ENGINE_GATED;
        }else{
            InfoPrintf("powerCtrlTest_maxwell: this chip doesn't have any engine with engine id equal to %d\n", tmp_count);
            gatedType = 0;
            continue;
        }

        // Skip testing if gatedType == 0
        if (gatedType) {
            // Give time for engine to go idle.
            powerCtrlTest_maxwell::DelayNs(50000);
            ResetELCGClkCounters();
            InfoPrintf("powerCtrlTest_maxwell: 1nd reading counters\n");
            clk_count_1 = ReadELCGClkCounters();
            powerCtrlTest_maxwell::DelayNs(150);
            InfoPrintf("powerCtrlTest_maxwell: 2nd reading counters\n");
            clk_count_2 = ReadELCGClkCounters();
            ResetELCGClkCounters();
            clk_count = SubtractEngineClkCounters(clk_count_1, clk_count_2);
            printEngineClkCounters(clk_count);
            checkClkGating(clk_count, Gated, gatedType, CheckComplement);
            InfoPrintf("powerCtrlTest_maxwell: testPerEngine done checking this engine individually in AUTOMATIC, gatedType = 0x%08x\n", gatedType);

            // Put Everything back to FULLPOWER.
            AllEngineMode(CG_FULLPOWER);
         }
     }
     powerCtrlTest_maxwell::DelayNs(50000);
     InfoPrintf("powerCtrlTest_maxwell: testPerEngine, done testing each engine individually in AUTOMATIC\n");

}

/*
 * Test function: testELCGPriWakeup
 *  - Verify that each engine in the chip enables the clocks when
 * pri access happens.
 * config GATE_CTRL register in pwr/therm
 *
 */
void powerCtrlTest_maxwell::testELCGPriWakeup(void)
{
    //  UINT32 addr, data;
    MaxwellClkCount fullpower_clk_count;
    MaxwellClkCount clk_count_1, clk_count_2, clk_count;
    MaxwellClkCount tmp_clkcount1, tmp_clkcount2, base_clkcount;
    bool Gated = true;
    bool CheckComplement = true;

    UINT32 rv, wv;
    UINT32 wakeup_dly_cnt_val = 0x1;
    const char *my_regname;

    UINT32 i;

    InfoPrintf("powerCtrlTest_maxwell: Testing Per Engine Clock Gating.\n");

    // Turn off idle clk slowdown
    DisableclkSlowdown();
    // Disable BLCG
    AllEngineForceClksOn(CG_FULLPOWER);
    EnableELCGClkCounters();

    //////////////////////////////////////////////////////
    // Put all engines in AUTOMATIC and verify clocks
    InfoPrintf("powerCtrlTest_maxwell: testELCGPriWakeup, putting all engines in AUTOMATIC mode and verifying all clks are gated due to idle\n");
    if (!m_ch->WaitForIdle()) {
        ErrPrintf("powerCtrlTest_maxwell: Channel WaitForIdle failed\n");
        return;
    }

    AllEngineMode(CG_AUTOMATIC);

    // Give time for engine to go idle.
    powerCtrlTest_maxwell::DelayNs(50000);     // Give time for engine to go idle.
    ResetELCGClkCounters();
    clk_count_1 = ReadELCGClkCounters();
    powerCtrlTest_maxwell::DelayNs(150);
    clk_count_2 = ReadELCGClkCounters();
    ResetELCGClkCounters();
    clk_count = SubtractEngineClkCounters(clk_count_1, clk_count_2);
    printEngineClkCounters(clk_count);
    checkClkGating(clk_count, Gated, allELCGGatedType, CheckComplement);
    InfoPrintf("powerCtrlTest_maxwell: testELCGPriWakeup, done checking clocks for all engines are gated before testing GR sys pri access\n");

    //test gr in sys wakeup by pri access (use FE_BLCG register access)
    InfoPrintf("powerCtrlTest_maxwell: testELCGPriWakeup, start checking pri access of gr in sys chiplet\n");
    wv = wakeup_dly_cnt_val;
    if (lwgpu->SetRegFldNum("LW_PGRAPH_PRI_FE_CG","_WAKEUP_DLY_CNT", wakeup_dly_cnt_val, 0)) {
        ErrPrintf("powerCtrlTest_maxwell: testELCGPriWakeup could not update LW_PGRAPH_PRI_FE_CG_WAKEUP_DLY_CNT to make write data 0x%08x\n", wv);
    } else {
        DebugPrintf("powerCtrlTest_maxwell: testELCGPriWakeup write data with wakeup_dly_cnt update will be 0x%08x\n", wv);
    }
    // Verify PRI wakeup by reading back the BLKCG PRI reg and comparing with the write data
    rv = 0;
    if (lwgpu->GetRegFldNum("LW_PGRAPH_PRI_FE_CG","_WAKEUP_DLY_CNT", &rv)) {
        ErrPrintf("powerCtrlTest_maxwell: could not read LW_PGRAPH_PRI_FE_CG register\n");
    }
    if (rv != wv) {
        ErrPrintf("powerCtrlTest_maxwell: testELCGPriWakeup didn't read back LW_PGRAPH_PRI_FE_CG_WAKEUP_DLY_CNT correctly, wv = 0x%08x, rv = 0x%8x\n",
                wv, rv);
        test_status_fail = true;
    } else {
        InfoPrintf("powerCtrlTest_maxwell: testELCGPriWakeup read back LW_PGRAPH_PRI_FE_CG_WAKEUP_DLY_CNT correctly, wv = 0x%08x, rv = 0x%8x\n", wv, rv);
    }
    InfoPrintf("powerCtrlTest_maxwell: testELCGPriWakeup, done checking pri access of gr in sys chiplet\n");

    //test gpcs waken up by pri access (use PROP_BLCG register access)
    InfoPrintf("powerCtrlTest_maxwell: testELCGPriWakeup, start checking pri access of gr in gpc chiplet\n");

    //Test each gpc individulally
    for (i=0; i<n_gpc; i++) {
        powerCtrlTest_maxwell::DelayNs(50000);     // Give time for engine to go idle.
        ResetELCGClkCounters();
        clk_count_1 = ReadELCGClkCounters();
        powerCtrlTest_maxwell::DelayNs(150);
        clk_count_2 = ReadELCGClkCounters();
        ResetELCGClkCounters();
        clk_count = SubtractEngineClkCounters(clk_count_1, clk_count_2);
        printEngineClkCounters(clk_count);
        checkClkGating(clk_count, Gated, allELCGGatedType, CheckComplement);
        InfoPrintf("powerCtrlTest_maxwell: testELCGPriWakeup, done checking clocks for all engines are gated before testing gpcs pri access\n");

        my_regname = "LW_PGRAPH_PRI_GPC0_PROP_CG";

        wv = wakeup_dly_cnt_val;
        if (lwgpu->SetRegFldNum(my_regname,"_WAKEUP_DLY_CNT", wakeup_dly_cnt_val, 0)) {
            ErrPrintf("powerCtrlTest_maxwell: testELCGPriWakeup could not update LW_PGRAPH_PRI_GPC%d_PROP_CG_WAKEUP_DLY_CNT to make write data 0x%08x\n", i, wv);
        } else {
            DebugPrintf("powerCtrlTest_maxwell: testELCGPriWakeup write data with wakeup_dly_cnt update will be 0x%08x\n", wv);
        }
        // Verify PRI wakeup by reading back the BLKCG PRI reg and comparing with the write data
        rv = 0;
        if (lwgpu->GetRegFldNum(my_regname,"_WAKEUP_DLY_CNT", &rv)) {
            ErrPrintf("powerCtrlTest_maxwell: could not read LW_PGRAPH_PRI_GPC%d_PROP_CG register\n", i);
        }
        if (rv != wv) {
            ErrPrintf("powerCtrlTest_maxwell: testELCGPriWakeup didn't read back LW_PGRAPH_PRI_GPC%d_PROP_CG_WAKEUP_DLY_CNT correctly, wv = 0x%08x, rv = 0x%8x\n", i, wv, rv);
            test_status_fail = true;
        } else {
            InfoPrintf("powerCtrlTest_maxwell: testELCGPriWakeup read back LW_PGRAPH_PRI_GPC%d_PROP_CG_WAKEUP_DLY_CNT correctly, wv = 0x%08x, rv = 0x%8x\n", i, wv, rv);
        }
        InfoPrintf("powerCtrlTest_maxwell: testELCGPriWakeup, done checking pri access of gr in gpc%d chiplet\n", i);
    }
  if((m_arch != 3330) && (m_arch != 3086)) {
    if(macros.lw_scal_litter_num_ce_lces > 0 && macros.lw_scal_litter_num_ce_pces > 0) {
        //test ce0 waken up by pri access
        InfoPrintf("powerCtrlTest_maxwell: testELCGPriWakeup, start checking pri access of ce0\n");

        powerCtrlTest_maxwell::DelayNs(50000);     // Give time for engine to go idle.
        ResetELCGClkCounters();
        clk_count_1 = ReadELCGClkCounters();
        powerCtrlTest_maxwell::DelayNs(150);
        clk_count_2 = ReadELCGClkCounters();
        ResetELCGClkCounters();
        clk_count = SubtractEngineClkCounters(clk_count_1, clk_count_2);
        printEngineClkCounters(clk_count);
        checkClkGating(clk_count, Gated, allELCGGatedType, CheckComplement);
        InfoPrintf("powerCtrlTest_maxwell: testELCGPriWakeup, done checking clocks for all engines are gated before testing ce0 pri access\n");

        // Check LW_CE_PCE0_PMM register, write bit0-bit3 as those 4 bits are defined
        wv = 0x5;
        if (lwgpu->SetRegFldNum("LW_CE_PCE0_PMM","",0x5)) {
            ErrPrintf("powerCtrlTest_maxwell: testELCGPriWakeup could not update LW_CE_PCE0_PMM to make write data 0x%08x\n", wv);
        } else {
            DebugPrintf("powerCtrlTest_maxwell: testELCGPriWakeup write data with LW_CE_PCE0_PMM update will be 0x%08x\n", wv);
        }
        // Verify PRI wakeup by reading back the LW_CE_PCE0_PMM reg and comparing with the write data
        rv = 0;
        if (lwgpu->GetRegFldNum("LW_CE_PCE0_PMM","", &rv)) {
            ErrPrintf("powerCtrlTest_maxwell: could not read LW_CE_PCE0_PMM register\n");
        }
        if (rv != wv) {
            ErrPrintf("powerCtrlTest_maxwell: testELCGPriWakeup didn't read back LW_CE_PCE0_PMM correctly, wv = 0x%08x, rv = 0x%8x\n", wv, rv);
            test_status_fail = true;
        } else {
            InfoPrintf("powerCtrlTest_maxwell: testELCGPriWakeup read back LW_CE_PCE0_PMM correctly, wv = 0x%08x, rv = 0x%8x\n", wv, rv);
        }
        InfoPrintf("powerCtrlTest_maxwell: testELCGPriWakeup, done checking pri access of ce0\n");
    }

    if(macros.lw_scal_litter_num_ce_lces > 1 && macros.lw_scal_litter_num_ce_pces > 1) {
        //test ce1 waken up by pri access
        InfoPrintf("powerCtrlTest_maxwell: testELCGPriWakeup, start checking pri access of ce1\n");

        powerCtrlTest_maxwell::DelayNs(50000);     // Give time for engine to go idle.
        ResetELCGClkCounters();
        clk_count_1 = ReadELCGClkCounters();
        powerCtrlTest_maxwell::DelayNs(150);
        clk_count_2 = ReadELCGClkCounters();
        ResetELCGClkCounters();
        clk_count = SubtractEngineClkCounters(clk_count_1, clk_count_2);
        printEngineClkCounters(clk_count);
        checkClkGating(clk_count, Gated, allELCGGatedType, CheckComplement);
        InfoPrintf("powerCtrlTest_maxwell: testELCGPriWakeup, done checking clocks for all engines are gated before testing ce1 pri access\n");

        wv = 0x5;
        if (lwgpu->SetRegFldNum("LW_CE_PCE1_PMM","",wv)) {
            ErrPrintf("powerCtrlTest_maxwell: testELCGPriWakeup could not update LW_CE_PCE1_PMM to make write data 0x%08x\n", wv);
        } else {
            DebugPrintf("powerCtrlTest_maxwell: testELCGPriWakeup write data with LW_CE_PCE1_PMM update will be 0x%08x\n", wv);
        }
        // Verify PRI wakeup by reading back the LW_CE_PCE1_PMM reg and comparing with the write data
        rv = 0;
        if (lwgpu->GetRegFldNum("LW_CE_PCE1_PMM","", &rv)) {
            ErrPrintf("powerCtrlTest_maxwell: could not read LW_CE_PCE1_PMM register\n");
        }
        if (rv != wv) {
            ErrPrintf("powerCtrlTest_maxwell: testELCGPriWakeup didn't read back LW_CE_PCE1_PMM correctly, wv = 0x%08x, rv = 0x%8x\n", wv, rv);
            test_status_fail = true;
        } else {
            InfoPrintf("powerCtrlTest_maxwell: testELCGPriWakeup read back LW_CE_PCE1_PMM correctly, wv = 0x%08x, rv = 0x%8x\n", wv, rv);
        }
        InfoPrintf("powerCtrlTest_maxwell: testELCGPriWakeup, done checking pri access of ce1\n");
    }

    if(macros.lw_scal_litter_num_ce_lces > 2 && macros.lw_scal_litter_num_ce_pces > 2) {
        //test ce2 waken up by pri access
        InfoPrintf("powerCtrlTest_maxwell: testELCGPriWakeup, start checking pri access of ce2\n");

        powerCtrlTest_maxwell::DelayNs(50000);     // Give time for engine to go idle.
        ResetELCGClkCounters();
        clk_count_1 = ReadELCGClkCounters();
        powerCtrlTest_maxwell::DelayNs(150);
        clk_count_2 = ReadELCGClkCounters();
        ResetELCGClkCounters();
        clk_count = SubtractEngineClkCounters(clk_count_1, clk_count_2);
        printEngineClkCounters(clk_count);
        checkClkGating(clk_count, Gated, allELCGGatedType, CheckComplement);
        InfoPrintf("powerCtrlTest_maxwell: testELCGPriWakeup, done checking clocks for all engines are gated before testing ce2 pri access\n");

        wv = 0x5;
        if (lwgpu->SetRegFldNum("LW_CE_PCE2_PMM","",wv)) {
            ErrPrintf("powerCtrlTest_maxwell: testELCGPriWakeup could not update LW_CE_PCE2_PMM to make write data 0x%08x\n", wv);
        } else {
            DebugPrintf("powerCtrlTest_maxwell: testELCGPriWakeup write data with LW_CE_PCE2_PMM update will be 0x%08x\n", wv);
        }
        // Verify PRI wakeup by reading back the LW_CE_PCE2_PMM reg and comparing with the write data
        rv = 0;
        if (lwgpu->GetRegFldNum("LW_CE_PCE2_PMM","", &rv)) {
            ErrPrintf("powerCtrlTest_maxwell: could not read LW_CE_PCE2_PMM register\n");
        }
        if (rv != wv) {
            ErrPrintf("powerCtrlTest_maxwell: testELCGPriWakeup didn't read back LW_CE_PCE2_PMM correctly, wv = 0x%08x, rv = 0x%8x\n", wv, rv);
            test_status_fail = true;
        } else {
            InfoPrintf("powerCtrlTest_maxwell: testELCGPriWakeup read back LW_CE_PCE2_PMM correctly, wv = 0x%08x, rv = 0x%8x\n", wv, rv);
        }
        InfoPrintf("powerCtrlTest_maxwell: testELCGPriWakeup, done checking pri access of ce2\n");
    }

    if(macros.lw_scal_litter_num_ce_lces > 3 && macros.lw_scal_litter_num_ce_pces > 3) {
        //test ce3 waken up by pri access
        InfoPrintf("powerCtrlTest_maxwell: testELCGPriWakeup, start checking pri access of ce3\n");

        powerCtrlTest_maxwell::DelayNs(50000);     // Give time for engine to go idle.
        ResetELCGClkCounters();
        clk_count_1 = ReadELCGClkCounters();
        powerCtrlTest_maxwell::DelayNs(150);
        clk_count_2 = ReadELCGClkCounters();
        ResetELCGClkCounters();
        clk_count = SubtractEngineClkCounters(clk_count_1, clk_count_2);
        printEngineClkCounters(clk_count);
        checkClkGating(clk_count, Gated, allELCGGatedType, CheckComplement);
        InfoPrintf("powerCtrlTest_maxwell: testELCGPriWakeup, done checking clocks for all engines are gated before testing ce3 pri access\n");

        wv = 0x5;
        if (lwgpu->SetRegFldNum("LW_CE_PCE3_PMM","",wv)) {
            ErrPrintf("powerCtrlTest_maxwell: testELCGPriWakeup could not update LW_CE_PCE3_PMM to make write data 0x%08x\n", wv);
        } else {
            DebugPrintf("powerCtrlTest_maxwell: testELCGPriWakeup write data with LW_CE_PCE3_PMM update will be 0x%08x\n", wv);
        }
        // Verify PRI wakeup by reading back the LW_CE_PCE3_PMM reg and comparing with the write data
        rv = 0;
        if (lwgpu->GetRegFldNum("LW_CE_PCE3_PMM","", &rv)) {
            ErrPrintf("powerCtrlTest_maxwell: could not read LW_CE_PCE3_PMM register\n");
        }
        if (rv != wv) {
            ErrPrintf("powerCtrlTest_maxwell: testELCGPriWakeup didn't read back LW_CE_PCE3_PMM correctly, wv = 0x%08x, rv = 0x%8x\n", wv, rv);
            test_status_fail = true;
        } else {
            InfoPrintf("powerCtrlTest_maxwell: testELCGPriWakeup read back LW_CE_PCE3_PMM correctly, wv = 0x%08x, rv = 0x%8x\n", wv, rv);
        }
        InfoPrintf("powerCtrlTest_maxwell: testELCGPriWakeup, done checking pri access of ce3\n");
    }
  }
    if(macros.lw_scal_litter_num_ce_lces > 4 && macros.lw_scal_litter_num_ce_pces > 4) {
        //test ce4 waken up by pri access
        InfoPrintf("powerCtrlTest_maxwell: testELCGPriWakeup, start checking pri access of ce4\n");

        powerCtrlTest_maxwell::DelayNs(50000);     // Give time for engine to go idle.
        ResetELCGClkCounters();
        clk_count_1 = ReadELCGClkCounters();
        powerCtrlTest_maxwell::DelayNs(150);
        clk_count_2 = ReadELCGClkCounters();
        ResetELCGClkCounters();
        clk_count = SubtractEngineClkCounters(clk_count_1, clk_count_2);
        printEngineClkCounters(clk_count);
        checkClkGating(clk_count, Gated, allELCGGatedType, CheckComplement);
        InfoPrintf("powerCtrlTest_maxwell: testELCGPriWakeup, done checking clocks for all engines are gated before testing ce4 pri access\n");

        wv = 0x5;
        if (lwgpu->SetRegFldNum("LW_CE_PCE4_PMM","",0x5)) {
            ErrPrintf("powerCtrlTest_maxwell: testELCGPriWakeup could not update LW_CE_PCE4_PMM to make write data 0x%08x\n", wv);
        } else {
            DebugPrintf("powerCtrlTest_maxwell: testELCGPriWakeup write data with LW_CE_PCE4_PMM update will be 0x%08x\n", wv);
        }
        // Verify PRI wakeup by reading back the LW_CE_PCE4_PMM reg and comparing with the write data
        rv = 0;
        if (lwgpu->GetRegFldNum("LW_CE_PCE4_PMM","", &rv)) {
            ErrPrintf("powerCtrlTest_maxwell: could not read LW_CE_PCE4_PMM register\n");
        }
        if (rv != wv) {
            ErrPrintf("powerCtrlTest_maxwell: testELCGPriWakeup didn't read back LW_CE_PCE4_PMM correctly, wv = 0x%08x, rv = 0x%8x\n", wv, rv);
            test_status_fail = true;
        } else {
            InfoPrintf("powerCtrlTest_maxwell: testELCGPriWakeup read back LW_CE_PCE4_PMM correctly, wv = 0x%08x, rv = 0x%8x\n", wv, rv);
        }
        InfoPrintf("powerCtrlTest_maxwell: testELCGPriWakeup, done checking pri access of ce4\n");
    }

    if(macros.lw_scal_litter_num_ce_lces > 5 && macros.lw_scal_litter_num_ce_pces > 5) {
        //test ce5 waken up by pri access
        InfoPrintf("powerCtrlTest_maxwell: testELCGPriWakeup, start checking pri access of ce5\n");

        powerCtrlTest_maxwell::DelayNs(50000);     // Give time for engine to go idle.
        ResetELCGClkCounters();
        clk_count_1 = ReadELCGClkCounters();
        powerCtrlTest_maxwell::DelayNs(150);
        clk_count_2 = ReadELCGClkCounters();
        ResetELCGClkCounters();
        clk_count = SubtractEngineClkCounters(clk_count_1, clk_count_2);
        printEngineClkCounters(clk_count);
        checkClkGating(clk_count, Gated, allELCGGatedType, CheckComplement);
        InfoPrintf("powerCtrlTest_maxwell: testELCGPriWakeup, done checking clocks for all engines are gated before testing ce5 pri access\n");

        wv = 0x5;
        if (lwgpu->SetRegFldNum("LW_CE_PCE5_PMM","",wv)) {
            ErrPrintf("powerCtrlTest_maxwell: testELCGPriWakeup could not update LW_CE_PCE5_PMM to make write data 0x%08x\n", wv);
        } else {
            DebugPrintf("powerCtrlTest_maxwell: testELCGPriWakeup write data with LW_CE_PCE5_PMM update will be 0x%08x\n", wv);
        }
        // Verify PRI wakeup by reading back the LW_CE_PCE5_PMM reg and comparing with the write data
        rv = 0;
        if (lwgpu->GetRegFldNum("LW_CE_PCE5_PMM","", &rv)) {
            ErrPrintf("powerCtrlTest_maxwell: could not read LW_CE_PCE5_PMM register\n");
        }
        if (rv != wv) {
            ErrPrintf("powerCtrlTest_maxwell: testELCGPriWakeup didn't read back LW_CE_PCE5_PMM correctly, wv = 0x%08x, rv = 0x%8x\n", wv, rv);
            test_status_fail = true;
        } else {
            InfoPrintf("powerCtrlTest_maxwell: testELCGPriWakeup read back LW_CE_PCE5_PMM correctly, wv = 0x%08x, rv = 0x%8x\n", wv, rv);
        }
        InfoPrintf("powerCtrlTest_maxwell: testELCGPriWakeup, done checking pri access of ce5\n");
    }

    if(macros.lw_scal_litter_num_ce_lces > 6 && macros.lw_scal_litter_num_ce_pces > 6) {
        //test ce5 waken up by pri access
        InfoPrintf("powerCtrlTest_maxwell: testELCGPriWakeup, start checking pri access of ce6\n");

        powerCtrlTest_maxwell::DelayNs(50000);     // Give time for engine to go idle.
        ResetELCGClkCounters();
        clk_count_1 = ReadELCGClkCounters();
        powerCtrlTest_maxwell::DelayNs(150);
        clk_count_2 = ReadELCGClkCounters();
        ResetELCGClkCounters();
        clk_count = SubtractEngineClkCounters(clk_count_1, clk_count_2);
        printEngineClkCounters(clk_count);
        checkClkGating(clk_count, Gated, allELCGGatedType, CheckComplement);
        InfoPrintf("powerCtrlTest_maxwell: testELCGPriWakeup, done checking clocks for all engines are gated before testing ce6 pri access\n");

        wv = 0x5;
        if (lwgpu->SetRegFldNum("LW_CE_PCE6_PMM","",wv)) {
            ErrPrintf("powerCtrlTest_maxwell: testELCGPriWakeup could not update LW_CE_PCE6_PMM to make write data 0x%08x\n", wv);
        } else {
            DebugPrintf("powerCtrlTest_maxwell: testELCGPriWakeup write data with LW_CE_PCE6_PMM update will be 0x%08x\n", wv);
        }
        // Verify PRI wakeup by reading back the LW_CE_PCE6_PMM reg and comparing with the write data
        rv = 0;
        if (lwgpu->GetRegFldNum("LW_CE_PCE6_PMM","", &rv)) {
            ErrPrintf("powerCtrlTest_maxwell: could not read LW_CE_PCE6_PMM register\n");
        }
        if (rv != wv) {
            ErrPrintf("powerCtrlTest_maxwell: testELCGPriWakeup didn't read back LW_CE_PCE6_PMM correctly, wv = 0x%08x, rv = 0x%8x\n", wv, rv);
            test_status_fail = true;
        } else {
            InfoPrintf("powerCtrlTest_maxwell: testELCGPriWakeup read back LW_CE_PCE6_PMM correctly, wv = 0x%08x, rv = 0x%8x\n", wv, rv);
        }
        InfoPrintf("powerCtrlTest_maxwell: testELCGPriWakeup, done checking pri access of ce6\n");
    }

    if(macros.lw_scal_litter_num_ce_lces > 7 && macros.lw_scal_litter_num_ce_pces > 7) {
        //test ce5 waken up by pri access
        InfoPrintf("powerCtrlTest_maxwell: testELCGPriWakeup, start checking pri access of ce7\n");

        powerCtrlTest_maxwell::DelayNs(50000);     // Give time for engine to go idle.
        ResetELCGClkCounters();
        clk_count_1 = ReadELCGClkCounters();
        powerCtrlTest_maxwell::DelayNs(150);
        clk_count_2 = ReadELCGClkCounters();
        ResetELCGClkCounters();
        clk_count = SubtractEngineClkCounters(clk_count_1, clk_count_2);
        printEngineClkCounters(clk_count);
        checkClkGating(clk_count, Gated, allELCGGatedType, CheckComplement);
        InfoPrintf("powerCtrlTest_maxwell: testELCGPriWakeup, done checking clocks for all engines are gated before testing ce7 pri access\n");

        wv = 0x5;
        if (lwgpu->SetRegFldNum("LW_CE_PCE7_PMM","",wv)) {
            ErrPrintf("powerCtrlTest_maxwell: testELCGPriWakeup could not update LW_CE_PCE7_PMM to make write data 0x%08x\n", wv);
        } else {
            DebugPrintf("powerCtrlTest_maxwell: testELCGPriWakeup write data with LW_CE_PCE7_PMM update will be 0x%08x\n", wv);
        }
        // Verify PRI wakeup by reading back the LW_CE_PCE7_PMM reg and comparing with the write data
        rv = 0;
        if (lwgpu->GetRegFldNum("LW_CE_PCE7_PMM","", &rv)) {
            ErrPrintf("powerCtrlTest_maxwell: could not read LW_CE_PCE7_PMM register\n");
        }
        if (rv != wv) {
            ErrPrintf("powerCtrlTest_maxwell: testELCGPriWakeup didn't read back LW_CE_PCE7_PMM correctly, wv = 0x%08x, rv = 0x%8x\n", wv, rv);
            test_status_fail = true;
        } else {
            InfoPrintf("powerCtrlTest_maxwell: testELCGPriWakeup read back LW_CE_PCE7_PMM correctly, wv = 0x%08x, rv = 0x%8x\n", wv, rv);
        }
        InfoPrintf("powerCtrlTest_maxwell: testELCGPriWakeup, done checking pri access of ce7\n");
    }

    if(macros.lw_scal_litter_num_ce_lces > 8 && macros.lw_scal_litter_num_ce_pces > 8) {
        //test ce5 waken up by pri access
        InfoPrintf("powerCtrlTest_maxwell: testELCGPriWakeup, start checking pri access of ce5\n");

        powerCtrlTest_maxwell::DelayNs(50000);     // Give time for engine to go idle.
        ResetELCGClkCounters();
        clk_count_1 = ReadELCGClkCounters();
        powerCtrlTest_maxwell::DelayNs(150);
        clk_count_2 = ReadELCGClkCounters();
        ResetELCGClkCounters();
        clk_count = SubtractEngineClkCounters(clk_count_1, clk_count_2);
        printEngineClkCounters(clk_count);
        checkClkGating(clk_count, Gated, allELCGGatedType, CheckComplement);
        InfoPrintf("powerCtrlTest_maxwell: testELCGPriWakeup, done checking clocks for all engines are gated before testing ce8 pri access\n");

        wv = 0x5;
        if (lwgpu->SetRegFldNum("LW_CE_PCE8_PMM","",wv)) {
            ErrPrintf("powerCtrlTest_maxwell: testELCGPriWakeup could not update LW_CE_PCE8_PMM to make write data 0x%08x\n", wv);
        } else {
            DebugPrintf("powerCtrlTest_maxwell: testELCGPriWakeup write data with LW_CE_PCE8_PMM update will be 0x%08x\n", wv);
        }
        // Verify PRI wakeup by reading back the LW_CE_PCE8_PMM reg and comparing with the write data
        rv = 0;
        if (lwgpu->GetRegFldNum("LW_CE_PCE8_PMM","", &rv)) {
            ErrPrintf("powerCtrlTest_maxwell: could not read LW_CE_PCE8_PMM register\n");
        }
        if (rv != wv) {
            ErrPrintf("powerCtrlTest_maxwell: testELCGPriWakeup didn't read back LW_CE_PCE8_PMM correctly, wv = 0x%08x, rv = 0x%8x\n", wv, rv);
            test_status_fail = true;
        } else {
            InfoPrintf("powerCtrlTest_maxwell: testELCGPriWakeup read back LW_CE_PCE8_PMM correctly, wv = 0x%08x, rv = 0x%8x\n", wv, rv);
        }
        InfoPrintf("powerCtrlTest_maxwell: testELCGPriWakeup, done checking pri access of ce8\n");
    }
    if(macros.lw_scal_litter_num_ce_lces > 9 && macros.lw_scal_litter_num_ce_pces > 9) {
        //test ce9 waken up by pri access
        InfoPrintf("powerCtrlTest_maxwell: testELCGPriWakeup, start checking pri access of ce9\n");

        powerCtrlTest_maxwell::DelayNs(50000);     // Give time for engine to go idle.
        ResetELCGClkCounters();
        clk_count_1 = ReadELCGClkCounters();
        powerCtrlTest_maxwell::DelayNs(150);
        clk_count_2 = ReadELCGClkCounters();
        ResetELCGClkCounters();
        clk_count = SubtractEngineClkCounters(clk_count_1, clk_count_2);
        printEngineClkCounters(clk_count);
        checkClkGating(clk_count, Gated, allELCGGatedType, CheckComplement);
        InfoPrintf("powerCtrlTest_maxwell: testELCGPriWakeup, done checking clocks for all engines are gated before testing ce9 pri access\n");

        wv = 0x5;
        if (lwgpu->SetRegFldNum("LW_CE_PCE9_PMM","",wv)) {
            ErrPrintf("powerCtrlTest_maxwell: testELCGPriWakeup could not update LW_CE_PCE9_PMM to make write data 0x%08x\n", wv);
        } else {
            DebugPrintf("powerCtrlTest_maxwell: testELCGPriWakeup write data with LW_CE_PCE9_PMM update will be 0x%08x\n", wv);
        }
        // Verify PRI wakeup by reading back the LW_CE_PCE9_PMM reg and comparing with the write data
        rv = 0;
        if (lwgpu->GetRegFldNum("LW_CE_PCE9_PMM","", &rv)) {
            ErrPrintf("powerCtrlTest_maxwell: could not read LW_CE_PCE9_PMM register\n");
        }
        if (rv != wv) {
            ErrPrintf("powerCtrlTest_maxwell: testELCGPriWakeup didn't read back LW_CE_PCE9_PMM correctly, wv = 0x%08x, rv = 0x%8x\n", wv, rv);
            test_status_fail = true;
        } else {
            InfoPrintf("powerCtrlTest_maxwell: testELCGPriWakeup read back LW_CE_PCE9_PMM correctly, wv = 0x%08x, rv = 0x%8x\n", wv, rv);
        }
        InfoPrintf("powerCtrlTest_maxwell: testELCGPriWakeup, done checking pri access of ce9\n");
    }

    if(macros.lw_engine_lwenc) {
        //test lwenc waken up by pri access
        InfoPrintf("powerCtrlTest_maxwell: testELCGPriWakeup, start checking pri access of lwenc\n");

        powerCtrlTest_maxwell::DelayNs(50000);     // Give time for engine to go idle.
        ResetELCGClkCounters();
        clk_count_1 = ReadELCGClkCounters();
        powerCtrlTest_maxwell::DelayNs(150);
        clk_count_2 = ReadELCGClkCounters();
        ResetELCGClkCounters();
        clk_count = SubtractEngineClkCounters(clk_count_1, clk_count_2);
        printEngineClkCounters(clk_count);
        checkClkGating(clk_count, Gated, allELCGGatedType, CheckComplement);
        InfoPrintf("powerCtrlTest_maxwell: testELCGPriWakeup, done checking clocks for all engines are gated before testing lwenc pri access\n");

        wv = wakeup_dly_cnt_val;
        if (lwgpu->SetRegFldNum("LW_PLWENC0_FALCON_MAILBOX0","_DATA", wakeup_dly_cnt_val, 0)) {
            ErrPrintf("powerCtrlTest_maxwell: testELCGPriWakeup could not update LW_PLWENC0_FALCON_MAILBOX0 to make write data 0x%08x\n", wv);
        } else {
            DebugPrintf("powerCtrlTest_maxwell: testELCGPriWakeup write data with wakeup_dly_cnt update will be 0x%08x\n", wv);
        }
        // Verify PRI wakeup by reading back the BLKCG PRI reg and comparing with the write data
        rv = 0;
        if (lwgpu->GetRegFldNum("LW_PLWENC0_FALCON_MAILBOX0","_DATA", &rv)) {
            ErrPrintf("powerCtrlTest_maxwell: could not read LW_PLWENC0_FALCON_MAILBOX0 register\n");
        }
        if (rv != wv) {
            if ((rv >> 8) == 0xbadf13) {
                InfoPrintf("powerCtrlTest_maxwell: testELCGPriWakeup LWENC0 floorsweep. Skip the check\n");
            }else{
                ErrPrintf("powerCtrlTest_maxwell: testELCGPriWakeup didn't read back LW_PLWENC0_FALCON_MAILBOX0 correctly, wv = 0x%08x, rv = 0x%8x\n", wv, rv);
                test_status_fail = true;
            }
        } else {
            InfoPrintf("powerCtrlTest_maxwell: testELCGPriWakeup read back LW_PLWENC0_FALCON_MAILBOX0 correctly, wv = 0x%08x, rv = 0x%8x\n", wv, rv);
        }

        if(macros.lw_engine_lwenc1 && macros.lw_scal_chip_num_lwencs > 1) {
            powerCtrlTest_maxwell::DelayNs(50000);     // Give time for engine to go idle.
            ResetELCGClkCounters();
            clk_count_1 = ReadELCGClkCounters();
            powerCtrlTest_maxwell::DelayNs(150);
            clk_count_2 = ReadELCGClkCounters();
            ResetELCGClkCounters();
            clk_count = SubtractEngineClkCounters(clk_count_1, clk_count_2);
            printEngineClkCounters(clk_count);
            checkClkGating(clk_count, Gated, allELCGGatedType, CheckComplement);
            InfoPrintf("powerCtrlTest_maxwell: testELCGPriWakeup, done checking clocks for all engines are gated before testing lwenc1 pri access\n");

            wv = wakeup_dly_cnt_val;
            if (lwgpu->SetRegFldNum("LW_PLWENC1_FALCON_MAILBOX0","_DATA", wakeup_dly_cnt_val, 0)) {
                ErrPrintf("powerCtrlTest_maxwell: testELCGPriWakeup could not update LW_PLWENC1_FALCON_MAILBOX0 to make write data 0x%08x\n", wv);
            } else {
                DebugPrintf("powerCtrlTest_maxwell: testELCGPriWakeup write data with wakeup_dly_cnt update will be 0x%08x\n", wv);
            }
            // Verify PRI wakeup by reading back the BLKCG PRI reg and comparing with the write data
            rv = 0;
            if (lwgpu->GetRegFldNum("LW_PLWENC1_FALCON_MAILBOX0","_DATA", &rv)) {
                ErrPrintf("powerCtrlTest_maxwell: could not read LW_PLWENC1_FALCON_MAILBOX0 register\n");
            }
            if (rv != wv) {
                if ((rv >> 8) == 0xbadf13) {
                    InfoPrintf("powerCtrlTest_maxwell: testELCGPriWakeup LWNEC1 floorsweep. Skip the check\n");
                }else{
                    ErrPrintf("powerCtrlTest_maxwell: testELCGPriWakeup didn't read back LW_PLWENC1_FALCON_MAILBOX0 correctly, wv = 0x%08x, rv = 0x%8x\n", wv, rv);
                    test_status_fail = true;
                }
            } else {
                InfoPrintf("powerCtrlTest_maxwell: testELCGPriWakeup read back LW_PLWENC1_FALCON_MAILBOX0 correctly, wv = 0x%08x, rv = 0x%8x\n", wv, rv);
            }
        }

        if(macros.lw_engine_lwenc2 && macros.lw_scal_chip_num_lwencs > 2) {
            powerCtrlTest_maxwell::DelayNs(50000);     // Give time for engine to go idle.
            ResetELCGClkCounters();
            clk_count_1 = ReadELCGClkCounters();
            powerCtrlTest_maxwell::DelayNs(150);
            clk_count_2 = ReadELCGClkCounters();
            ResetELCGClkCounters();
            clk_count = SubtractEngineClkCounters(clk_count_1, clk_count_2);
            printEngineClkCounters(clk_count);
            checkClkGating(clk_count, Gated, allELCGGatedType, CheckComplement);
            InfoPrintf("powerCtrlTest_maxwell: testELCGPriWakeup, done checking clocks for all engines are gated before testing lwenc2 pri access\n");

            wv = wakeup_dly_cnt_val;
            if (lwgpu->SetRegFldNum("LW_PLWENC2_FALCON_MAILBOX0","_DATA", wakeup_dly_cnt_val, 0)) {
                ErrPrintf("powerCtrlTest_maxwell: testELCGPriWakeup could not update LW_PLWENC2_FALCON_MAILBOX0 to make write data 0x%08x\n", wv);
            } else {
                DebugPrintf("powerCtrlTest_maxwell: testELCGPriWakeup write data with wakeup_dly_cnt update will be 0x%08x\n", wv);
            }
            // Verify PRI wakeup by reading back the BLKCG PRI reg and comparing with the write data
            rv = 0;
            if (lwgpu->GetRegFldNum("LW_PLWENC2_FALCON_MAILBOX0","_DATA", &rv)) {
                ErrPrintf("powerCtrlTest_maxwell: could not read LW_PLWENC2_FALCON_MAILBOX0 register\n");
            }
            if (rv != wv) {
                if ((rv >> 8) == 0xbadf13) {
                    InfoPrintf("powerCtrlTest_maxwell: testELCGPriWakeup LWNEC2 floorsweep. Skip the check\n");
                }else{
                    ErrPrintf("powerCtrlTest_maxwell: testELCGPriWakeup didn't read back LW_PLWENC2_FALCON_MAILBOX0 correctly, wv = 0x%08x, rv = 0x%8x\n", wv, rv);
                    test_status_fail = true;
                }
            } else {
                InfoPrintf("powerCtrlTest_maxwell: testELCGPriWakeup read back LW_PLWENC2_FALCON_MAILBOX0 correctly, wv = 0x%08x, rv = 0x%8x\n", wv, rv);
            }
        }

        InfoPrintf("powerCtrlTest_maxwell: testELCGPriWakeup, done checking pri access of lwenc2\n");
    }

    if(macros.lw_engine_lwdec) {
        //test mvdec waken up by pri access
        InfoPrintf("powerCtrlTest_maxwell: testELCGPriWakeup, start checking pri access of lwdec\n");

        powerCtrlTest_maxwell::DelayNs(50000);     // Give time for engine to go idle.
        ResetELCGClkCounters();
        clk_count_1 = ReadELCGClkCounters();
        powerCtrlTest_maxwell::DelayNs(150);
        clk_count_2 = ReadELCGClkCounters();
        ResetELCGClkCounters();
        clk_count = SubtractEngineClkCounters(clk_count_1, clk_count_2);
        printEngineClkCounters(clk_count);
        checkClkGating(clk_count, Gated, allELCGGatedType, CheckComplement);
        InfoPrintf("powerCtrlTest_maxwell: testELCGPriWakeup, done checking clocks for all engines are gated before testing lwdec pri access\n");

        wv = wakeup_dly_cnt_val;
        if (lwgpu->SetRegFldNum("LW_PLWDEC0_FALCON_MAILBOX0","_DATA", wakeup_dly_cnt_val, 0)) {
            ErrPrintf("powerCtrlTest_maxwell: testELCGPriWakeup could not update LW_PLWDEC0_FALCON_MAILBOX0_DATA to make write data 0x%08x\n", wv);
        } else {
            DebugPrintf("powerCtrlTest_maxwell: testELCGPriWakeup write data with wakeup_dly_cnt update will be 0x%08x\n", wv);
        }
        // Verify PRI wakeup by reading back the BLKCG PRI reg and comparing with the write data
        rv = 0;
        if (lwgpu->GetRegFldNum("LW_PLWDEC0_FALCON_MAILBOX0","_DATA", &rv)) {
            ErrPrintf("powerCtrlTest_maxwell: could not read LW_PLWDEC0_FALCON_MAILBOX0 register\n");
        }
        if (rv != wv) {
            if ((rv >> 8) == 0xbadf13) {
                InfoPrintf("powerCtrlTest_maxwell: testELCGPriWakeup LWDEC floorsweep. Skip the check\n");
            }else{
                ErrPrintf("powerCtrlTest_maxwell: testELCGPriWakeup didn't read back LW_PLWDEC0_FALCON_MAILBOX0_DATA correctly, wv = 0x%08x, rv = 0x%8x\n", wv, rv);
                test_status_fail = true;
            }
        } else {
            InfoPrintf("powerCtrlTest_maxwell: testELCGPriWakeup read back LW_PLWDEC0_FALCON_MAILBOX0_DATA correctly, wv = 0x%08x, rv = 0x%8x\n", wv, rv);
        }
        InfoPrintf("powerCtrlTest_maxwell: testELCGPriWakeup, done checking pri access of lwdec\n");

        if(macros.lw_scal_chip_num_lwdecs > 1) {
            powerCtrlTest_maxwell::DelayNs(50000);     // Give time for engine to go idle.
            ResetELCGClkCounters();
            clk_count_1 = ReadELCGClkCounters();
            powerCtrlTest_maxwell::DelayNs(150);
            clk_count_2 = ReadELCGClkCounters();
            ResetELCGClkCounters();
            clk_count = SubtractEngineClkCounters(clk_count_1, clk_count_2);
            printEngineClkCounters(clk_count);
            checkClkGating(clk_count, Gated, allELCGGatedType, CheckComplement);
            InfoPrintf("powerCtrlTest_maxwell: testELCGPriWakeup, done checking clocks for all engines are gated before testing lwdec1 pri access\n");

            wv = wakeup_dly_cnt_val;
            if (lwgpu->SetRegFldNum("LW_PLWDEC1_FALCON_MAILBOX0","_DATA", wakeup_dly_cnt_val, 0)) {
                ErrPrintf("powerCtrlTest_maxwell: testELCGPriWakeup could not update LW_PLWDEC1_FALCON_MAILBOX0_DATA to make write data 0x%08x\n", wv);
            } else {
                DebugPrintf("powerCtrlTest_maxwell: testELCGPriWakeup write data with wakeup_dly_cnt update will be 0x%08x\n", wv);
            }
            // Verify PRI wakeup by reading back the BLKCG PRI reg and comparing with the write data
            rv = 0;
            if (lwgpu->GetRegFldNum("LW_PLWDEC1_FALCON_MAILBOX0","_DATA", &rv)) {
                ErrPrintf("powerCtrlTest_maxwell: could not read LW_PLWDEC1_FALCON_MAILBOX0 register\n");
            }
            if (rv != wv) {
                if ((rv >> 8) == 0xbadf13) {
                    InfoPrintf("powerCtrlTest_maxwell: testELCGPriWakeup LWDEC1 floorsweep. Skip the check\n");
                }else{
                    ErrPrintf("powerCtrlTest_maxwell: testELCGPriWakeup didn't read back LW_PLWDEC1_FALCON_MAILBOX0_DATA correctly, wv = 0x%08x, rv = 0x%8x\n", wv, rv);
                    test_status_fail = true;
                }
            } else {
                InfoPrintf("powerCtrlTest_maxwell: testELCGPriWakeup read back LW_PLWDEC1_FALCON_MAILBOX0_DATA correctly, wv = 0x%08x, rv = 0x%8x\n", wv, rv);
            }
        }

        if(macros.lw_scal_chip_num_lwdecs > 2) {
            powerCtrlTest_maxwell::DelayNs(50000);     // Give time for engine to go idle.
            ResetELCGClkCounters();
            clk_count_1 = ReadELCGClkCounters();
            powerCtrlTest_maxwell::DelayNs(150);
            clk_count_2 = ReadELCGClkCounters();
            ResetELCGClkCounters();
            clk_count = SubtractEngineClkCounters(clk_count_1, clk_count_2);
            printEngineClkCounters(clk_count);
            checkClkGating(clk_count, Gated, allELCGGatedType, CheckComplement);
            InfoPrintf("powerCtrlTest_maxwell: testELCGPriWakeup, done checking clocks for all engines are gated before testing lwdec2 pri access\n");

            wv = wakeup_dly_cnt_val;
            if (lwgpu->SetRegFldNum("LW_PLWDEC2_FALCON_MAILBOX0","_DATA", wakeup_dly_cnt_val, 0)) {
                ErrPrintf("powerCtrlTest_maxwell: testELCGPriWakeup could not update LW_PLWDEC2_FALCON_MAILBOX0_DATA to make write data 0x%08x\n", wv);
            } else {
                DebugPrintf("powerCtrlTest_maxwell: testELCGPriWakeup write data with wakeup_dly_cnt update will be 0x%08x\n", wv);
            }
            // Verify PRI wakeup by reading back the BLKCG PRI reg and comparing with the write data
            rv = 0;
            if (lwgpu->GetRegFldNum("LW_PLWDEC2_FALCON_MAILBOX0","_DATA", &rv)) {
                ErrPrintf("powerCtrlTest_maxwell: could not read LW_PLWDEC2_FALCON_MAILBOX0 register\n");
            }
            if (rv != wv) {
                if ((rv >> 8) == 0xbadf13) {
                    InfoPrintf("powerCtrlTest_maxwell: testELCGPriWakeup LWDEC2 floorsweep. Skip the check\n");
                }else{
                    ErrPrintf("powerCtrlTest_maxwell: testELCGPriWakeup didn't read back LW_PLWDEC2_FALCON_MAILBOX0_DATA correctly, wv = 0x%08x, rv = 0x%8x\n", wv, rv);
                    test_status_fail = true;
                }
            } else {
                InfoPrintf("powerCtrlTest_maxwell: testELCGPriWakeup read back LW_PLWDEC2_FALCON_MAILBOX0_DATA correctly, wv = 0x%08x, rv = 0x%8x\n", wv, rv);
            }
        }

        if(macros.lw_scal_chip_num_lwdecs > 3) {
            powerCtrlTest_maxwell::DelayNs(50000);     // Give time for engine to go idle.
            ResetELCGClkCounters();
            clk_count_1 = ReadELCGClkCounters();
            powerCtrlTest_maxwell::DelayNs(150);
            clk_count_2 = ReadELCGClkCounters();
            ResetELCGClkCounters();
            clk_count = SubtractEngineClkCounters(clk_count_1, clk_count_2);
            printEngineClkCounters(clk_count);
            checkClkGating(clk_count, Gated, allELCGGatedType, CheckComplement);
            InfoPrintf("powerCtrlTest_maxwell: testELCGPriWakeup, done checking clocks for all engines are gated before testing lwdec3 pri access\n");

            wv = wakeup_dly_cnt_val;
            if (lwgpu->SetRegFldNum("LW_PLWDEC3_FALCON_MAILBOX0","_DATA", wakeup_dly_cnt_val, 0)) {
                ErrPrintf("powerCtrlTest_maxwell: testELCGPriWakeup could not update LW_PLWDEC3_FALCON_MAILBOX0_DATA to make write data 0x%08x\n", wv);
            } else {
                DebugPrintf("powerCtrlTest_maxwell: testELCGPriWakeup write data with wakeup_dly_cnt update will be 0x%08x\n", wv);
            }
            // Verify PRI wakeup by reading back the BLKCG PRI reg and comparing with the write data
            rv = 0;
            if (lwgpu->GetRegFldNum("LW_PLWDEC3_FALCON_MAILBOX0","_DATA", &rv)) {
                ErrPrintf("powerCtrlTest_maxwell: could not read LW_PLWDEC3_FALCON_MAILBOX0 register\n");
            }
            if (rv != wv) {
                if ((rv >> 8) == 0xbadf13) {
                    InfoPrintf("powerCtrlTest_maxwell: testELCGPriWakeup LWDEC3 floorsweep. Skip the check\n");
                }else{
                    ErrPrintf("powerCtrlTest_maxwell: testELCGPriWakeup didn't read back LW_PLWDEC3_FALCON_MAILBOX0_DATA correctly, wv = 0x%08x, rv = 0x%8x\n", wv, rv);
                    test_status_fail = true;
                }
            } else {
                InfoPrintf("powerCtrlTest_maxwell: testELCGPriWakeup read back LW_PLWDEC3_FALCON_MAILBOX0_DATA correctly, wv = 0x%08x, rv = 0x%8x\n", wv, rv);
            }
        }

        if(macros.lw_scal_chip_num_lwdecs > 4) {
            powerCtrlTest_maxwell::DelayNs(50000);     // Give time for engine to go idle.
            ResetELCGClkCounters();
            clk_count_1 = ReadELCGClkCounters();
            powerCtrlTest_maxwell::DelayNs(150);
            clk_count_2 = ReadELCGClkCounters();
            ResetELCGClkCounters();
            clk_count = SubtractEngineClkCounters(clk_count_1, clk_count_2);
            printEngineClkCounters(clk_count);
            checkClkGating(clk_count, Gated, allELCGGatedType, CheckComplement);
            InfoPrintf("powerCtrlTest_maxwell: testELCGPriWakeup, done checking clocks for all engines are gated before testing lwdec4 pri access\n");

            wv = wakeup_dly_cnt_val;
            if (lwgpu->SetRegFldNum("LW_PLWDEC4_FALCON_MAILBOX0","_DATA", wakeup_dly_cnt_val, 0)) {
                ErrPrintf("powerCtrlTest_maxwell: testELCGPriWakeup could not update LW_PLWDEC4_FALCON_MAILBOX0_DATA to make write data 0x%08x\n", wv);
            } else {
                DebugPrintf("powerCtrlTest_maxwell: testELCGPriWakeup write data with wakeup_dly_cnt update will be 0x%08x\n", wv);
            }
            // Verify PRI wakeup by reading back the BLKCG PRI reg and comparing with the write data
            rv = 0;
            if (lwgpu->GetRegFldNum("LW_PLWDEC4_FALCON_MAILBOX0","_DATA", &rv)) {
                ErrPrintf("powerCtrlTest_maxwell: could not read LW_PLWDEC4_FALCON_MAILBOX0 register\n");
            }
            if (rv != wv) {
                if ((rv >> 8) == 0xbadf13) {
                    InfoPrintf("powerCtrlTest_maxwell: testELCGPriWakeup LWDEC4 floorsweep. Skip the check\n");
                }else{
                    ErrPrintf("powerCtrlTest_maxwell: testELCGPriWakeup didn't read back LW_PLWDEC4_FALCON_MAILBOX0_DATA correctly, wv = 0x%08x, rv = 0x%8x\n", wv, rv);
                    test_status_fail = true;
                }
            } else {
                InfoPrintf("powerCtrlTest_maxwell: testELCGPriWakeup read back LW_PLWDEC4_FALCON_MAILBOX0_DATA correctly, wv = 0x%08x, rv = 0x%8x\n", wv, rv);
            }
        }
    }
if(macros.lw_scal_litter_num_lwdecs > 5) {
            powerCtrlTest_maxwell::DelayNs(50000);     // Give time for engine to go idle.
            ResetELCGClkCounters();
            clk_count_1 = ReadELCGClkCounters();
            powerCtrlTest_maxwell::DelayNs(150);
            clk_count_2 = ReadELCGClkCounters();
            ResetELCGClkCounters();
            clk_count = SubtractEngineClkCounters(clk_count_1, clk_count_2);
            printEngineClkCounters(clk_count);
            checkClkGating(clk_count, Gated, allELCGGatedType, CheckComplement);
            InfoPrintf("powerCtrlTest_maxwell: testELCGPriWakeup, done checking clocks for all engines are gated before testing lwdec5 pri access\n");

            wv = wakeup_dly_cnt_val;
            if (lwgpu->SetRegFldNum("LW_PLWDEC5_FALCON_MAILBOX0","_DATA", wakeup_dly_cnt_val, 0)) {
                ErrPrintf("powerCtrlTest_maxwell: testELCGPriWakeup could not update LW_PLWDEC5_FALCON_MAILBOX0_DATA to make write data 0x%08x\n", wv);
            } else {
                DebugPrintf("powerCtrlTest_maxwell: testELCGPriWakeup write data with wakeup_dly_cnt update will be 0x%08x\n", wv);
            }
            // Verify PRI wakeup by reading back the BLKCG PRI reg and comparing with the write data
            rv = 0;
            if (lwgpu->GetRegFldNum("LW_PLWDEC5_FALCON_MAILBOX0","_DATA", &rv)) {
                ErrPrintf("powerCtrlTest_maxwell: could not read LW_PLWDEC5_FALCON_MAILBOX0 register\n");
            }
            if (rv != wv) {
                if ((rv >> 8) == 0xbadf13) {
                    InfoPrintf("powerCtrlTest_maxwell: testELCGPriWakeup LWDEC5 floorsweep. Skip the check\n");
                }else{
                    ErrPrintf("powerCtrlTest_maxwell: testELCGPriWakeup didn't read back LW_PLWDEC5_FALCON_MAILBOX0_DATA correctly, wv = 0x%08x, rv = 0x%8x\n", wv, rv);
                    test_status_fail = true;
                }
            } else {
                InfoPrintf("powerCtrlTest_maxwell: testELCGPriWakeup read back LW_PLWDEC5_FALCON_MAILBOX0_DATA correctly, wv = 0x%08x, rv = 0x%8x\n", wv, rv);
            }
        }
    

if(macros.lw_scal_litter_num_lwdecs > 6) {
            powerCtrlTest_maxwell::DelayNs(50000);     // Give time for engine to go idle.
            ResetELCGClkCounters();
            clk_count_1 = ReadELCGClkCounters();
            powerCtrlTest_maxwell::DelayNs(150);
            clk_count_2 = ReadELCGClkCounters();
            ResetELCGClkCounters();
            clk_count = SubtractEngineClkCounters(clk_count_1, clk_count_2);
            printEngineClkCounters(clk_count);
            checkClkGating(clk_count, Gated, allELCGGatedType, CheckComplement);
            InfoPrintf("powerCtrlTest_maxwell: testELCGPriWakeup, done checking clocks for all engines are gated before testing lwdec6 pri access\n");

            wv = wakeup_dly_cnt_val;
            if (lwgpu->SetRegFldNum("LW_PLWDEC6_FALCON_MAILBOX0","_DATA", wakeup_dly_cnt_val, 0)) {
                ErrPrintf("powerCtrlTest_maxwell: testELCGPriWakeup could not update LW_PLWDEC6_FALCON_MAILBOX0_DATA to make write data 0x%08x\n", wv);
            } else {
                DebugPrintf("powerCtrlTest_maxwell: testELCGPriWakeup write data with wakeup_dly_cnt update will be 0x%08x\n", wv);
            }
            // Verify PRI wakeup by reading back the BLKCG PRI reg and comparing with the write data
            rv = 0;
            if (lwgpu->GetRegFldNum("LW_PLWDEC6_FALCON_MAILBOX0","_DATA", &rv)) {
                ErrPrintf("powerCtrlTest_maxwell: could not read LW_PLWDEC6_FALCON_MAILBOX0 register\n");
            }
            if (rv != wv) {
                if ((rv >> 8) == 0xbadf13) {
                    InfoPrintf("powerCtrlTest_maxwell: testELCGPriWakeup LWDEC6 floorsweep. Skip the check\n");
                }else{
                    ErrPrintf("powerCtrlTest_maxwell: testELCGPriWakeup didn't read back LW_PLWDEC6_FALCON_MAILBOX0_DATA correctly, wv = 0x%08x, rv = 0x%8x\n", wv, rv);
                    test_status_fail = true;
                }
            } else {
                InfoPrintf("powerCtrlTest_maxwell: testELCGPriWakeup read back LW_PLWDEC6_FALCON_MAILBOX0_DATA correctly, wv = 0x%08x, rv = 0x%8x\n", wv, rv);
            }
        }
    

if(macros.lw_scal_litter_num_lwdecs > 7) {
            powerCtrlTest_maxwell::DelayNs(50000);     // Give time for engine to go idle.
            ResetELCGClkCounters();
            clk_count_1 = ReadELCGClkCounters();
            powerCtrlTest_maxwell::DelayNs(150);
            clk_count_2 = ReadELCGClkCounters();
            ResetELCGClkCounters();
            clk_count = SubtractEngineClkCounters(clk_count_1, clk_count_2);
            printEngineClkCounters(clk_count);
            checkClkGating(clk_count, Gated, allELCGGatedType, CheckComplement);
            InfoPrintf("powerCtrlTest_maxwell: testELCGPriWakeup, done checking clocks for all engines are gated before testing lwdec7 pri access\n");

            wv = wakeup_dly_cnt_val;
            if (lwgpu->SetRegFldNum("LW_PLWDEC7_FALCON_MAILBOX0","_DATA", wakeup_dly_cnt_val, 0)) {
                ErrPrintf("powerCtrlTest_maxwell: testELCGPriWakeup could not update LW_PLWDEC7_FALCON_MAILBOX0_DATA to make write data 0x%08x\n", wv);
            } else {
                DebugPrintf("powerCtrlTest_maxwell: testELCGPriWakeup write data with wakeup_dly_cnt update will be 0x%08x\n", wv);
            }
            // Verify PRI wakeup by reading back the BLKCG PRI reg and comparing with the write data
            rv = 0;
            if (lwgpu->GetRegFldNum("LW_PLWDEC7_FALCON_MAILBOX0","_DATA", &rv)) {
                ErrPrintf("powerCtrlTest_maxwell: could not read LW_PLWDEC7_FALCON_MAILBOX0 register\n");
            }
            if (rv != wv) {
                if ((rv >> 8) == 0xbadf13) {
                    InfoPrintf("powerCtrlTest_maxwell: testELCGPriWakeup LWDEC7 floorsweep. Skip the check\n");
                }else{
                    ErrPrintf("powerCtrlTest_maxwell: testELCGPriWakeup didn't read back LW_PLWDEC7_FALCON_MAILBOX0_DATA correctly, wv = 0x%08x, rv = 0x%8x\n", wv, rv);
                    test_status_fail = true;
                }
            } else {
                InfoPrintf("powerCtrlTest_maxwell: testELCGPriWakeup read back LW_PLWDEC7_FALCON_MAILBOX0_DATA correctly, wv = 0x%08x, rv = 0x%8x\n", wv, rv);
            }
        }
    
    if(macros.lw_engine_ofa0) {
        InfoPrintf("powerCtrlTest_maxwell: testELCGPriWakeup, start checking pri access of ofa\n");

        powerCtrlTest_maxwell::DelayNs(50000);     // Give time for engine to go idle.
        ResetELCGClkCounters();
        clk_count_1 = ReadELCGClkCounters();
        powerCtrlTest_maxwell::DelayNs(150);
        clk_count_2 = ReadELCGClkCounters();
        ResetELCGClkCounters();
        clk_count = SubtractEngineClkCounters(clk_count_1, clk_count_2);
        printEngineClkCounters(clk_count);
        checkClkGating(clk_count, Gated, allELCGGatedType, CheckComplement);
        InfoPrintf("powerCtrlTest_maxwell: testELCGPriWakeup, done checking clocks for all engines are gated before testing ofa pri access\n");

        wv = 0xbeefdead;
        if (lwgpu->SetRegFldNum("LW_POFA_FALCON_MAILBOX0","", wv, 0)) {
            ErrPrintf("powerCtrlTest_maxwell: testELCGPriWakeup could not update LW_POFA_FALCON_MAILBOX0 to make write data 0x%08x\n", wv);
        } else {
            DebugPrintf("powerCtrlTest_maxwell: testELCGPriWakeup write data with update will be 0x%08x\n", wv);
        }
        //Verify PRI wakeup by reading back the BLKCG PRI reg and comparing with the write data
        rv = 0;
        if (lwgpu->GetRegFldNum("LW_POFA_FALCON_MAILBOX0","", &rv)) {
            ErrPrintf("powerCtrlTest_maxwell: could not read LW_POFA_FALCON_MAILBOX0 register\n");
        }
        if (rv != wv) {
            ErrPrintf("powerCtrlTest_maxwell: testELCGPriWakeup didn't read back LW_POFA_FALCON_MAILBOX0 correctly, wv = 0x%08x, rv = 0x%8x\n", wv, rv);
            test_status_fail = true;
        } else {
            InfoPrintf("powerCtrlTest_maxwell: testELCGPriWakeup read back LW_POFA_FALCON_MAILBOX0 correctly, wv = 0x%08x, rv = 0x%8x\n", wv, rv);
        }
        InfoPrintf("powerCtrlTest_maxwell: testELCGPriWakeup, done checking pri access of ofa\n");
    }
if(macros.lw_scal_litter_num_lwjpgs > 0) {
            powerCtrlTest_maxwell::DelayNs(50000);     // Give time for engine to go idle.
            ResetELCGClkCounters();
            clk_count_1 = ReadELCGClkCounters();
            powerCtrlTest_maxwell::DelayNs(150);
            clk_count_2 = ReadELCGClkCounters();
            ResetELCGClkCounters();
            clk_count = SubtractEngineClkCounters(clk_count_1, clk_count_2);
            printEngineClkCounters(clk_count);
            checkClkGating(clk_count, Gated, allELCGGatedType, CheckComplement);
            InfoPrintf("powerCtrlTest_maxwell: testELCGPriWakeup, done checking clocks for all engines are gated before testing lwjpg0 pri access\n");

            wv = wakeup_dly_cnt_val;
            if (lwgpu->SetRegFldNum("LW_PLWJPG0_FALCON_MAILBOX0","_DATA", wakeup_dly_cnt_val, 0)) {
                ErrPrintf("powerCtrlTest_maxwell: testELCGPriWakeup could not update LW_PLWJPG0_FALCON_MAILBOX0_DATA to make write data 0x%08x\n", wv);
            } else {
                DebugPrintf("powerCtrlTest_maxwell: testELCGPriWakeup write data with wakeup_dly_cnt update will be 0x%08x\n", wv);
            }
            // Verify PRI wakeup by reading back the BLKCG PRI reg and comparing with the write data
            rv = 0;
            if (lwgpu->GetRegFldNum("LW_PLWJPG0_FALCON_MAILBOX0","_DATA", &rv)) {
                ErrPrintf("powerCtrlTest_maxwell: could not read LW_PLWJPG0_FALCON_MAILBOX0 register\n");
            }
            if (rv != wv) {
                if ((rv >> 8) == 0xbadf13) {
                    InfoPrintf("powerCtrlTest_maxwell: testELCGPriWakeup LWJPG0 floorsweep. Skip the check\n");
                }else{
                    ErrPrintf("powerCtrlTest_maxwell: testELCGPriWakeup didn't read back LW_PLWJPG0_FALCON_MAILBOX0_DATA correctly, wv = 0x%08x, rv = 0x%8x\n", wv, rv);
                    test_status_fail = true;
                }
            } else {
                InfoPrintf("powerCtrlTest_maxwell: testELCGPriWakeup read back LW_PLWJPG0_FALCON_MAILBOX0_DATA correctly, wv = 0x%08x, rv = 0x%8x\n", wv, rv);
            }
        }
   
    if(macros.lw_scal_litter_num_lwjpgs > 1) {
            powerCtrlTest_maxwell::DelayNs(50000);     // Give time for engine to go idle.
            ResetELCGClkCounters();
            clk_count_1 = ReadELCGClkCounters();
            powerCtrlTest_maxwell::DelayNs(150);
            clk_count_2 = ReadELCGClkCounters();
            ResetELCGClkCounters();
            clk_count = SubtractEngineClkCounters(clk_count_1, clk_count_2);
            printEngineClkCounters(clk_count);
            checkClkGating(clk_count, Gated, allELCGGatedType, CheckComplement);
            InfoPrintf("powerCtrlTest_maxwell: testELCGPriWakeup, done checking clocks for all engines are gated before testing lwjpg1 pri access\n");

            wv = wakeup_dly_cnt_val;
            if (lwgpu->SetRegFldNum("LW_PLWJPG1_FALCON_MAILBOX0","_DATA", wakeup_dly_cnt_val, 0)) {
                ErrPrintf("powerCtrlTest_maxwell: testELCGPriWakeup could not update LW_PLWJPG1_FALCON_MAILBOX0_DATA to make write data 0x%08x\n", wv);
            } else {
                DebugPrintf("powerCtrlTest_maxwell: testELCGPriWakeup write data with wakeup_dly_cnt update will be 0x%08x\n", wv);
            }
            // Verify PRI wakeup by reading back the BLKCG PRI reg and comparing with the write data
            rv = 0;
            if (lwgpu->GetRegFldNum("LW_PLWJPG1_FALCON_MAILBOX0","_DATA", &rv)) {
                ErrPrintf("powerCtrlTest_maxwell: could not read LW_PLWJPG1_FALCON_MAILBOX0 register\n");
            }
            if (rv != wv) {
                if ((rv >> 8) == 0xbadf13) {
                    InfoPrintf("powerCtrlTest_maxwell: testELCGPriWakeup LWJPG1 floorsweep. Skip the check\n");
                }else{
                    ErrPrintf("powerCtrlTest_maxwell: testELCGPriWakeup didn't read back LW_PLWJPG1_FALCON_MAILBOX0_DATA correctly, wv = 0x%08x, rv = 0x%8x\n", wv, rv);
                    test_status_fail = true;
                }
            } else {
                InfoPrintf("powerCtrlTest_maxwell: testELCGPriWakeup read back LW_PLWJPG1_FALCON_MAILBOX0_DATA correctly, wv = 0x%08x, rv = 0x%8x\n", wv, rv);
            }
        }

    if(macros.lw_scal_litter_num_lwjpgs > 2) {
            powerCtrlTest_maxwell::DelayNs(50000);     // Give time for engine to go idle.
            ResetELCGClkCounters();
            clk_count_1 = ReadELCGClkCounters();
            powerCtrlTest_maxwell::DelayNs(150);
            clk_count_2 = ReadELCGClkCounters();
            ResetELCGClkCounters();
            clk_count = SubtractEngineClkCounters(clk_count_1, clk_count_2);
            printEngineClkCounters(clk_count);
            checkClkGating(clk_count, Gated, allELCGGatedType, CheckComplement);
            InfoPrintf("powerCtrlTest_maxwell: testELCGPriWakeup, done checking clocks for all engines are gated before testing lwjpg2 pri access\n");

            wv = wakeup_dly_cnt_val;
            if (lwgpu->SetRegFldNum("LW_PLWJPG2_FALCON_MAILBOX0","_DATA", wakeup_dly_cnt_val, 0)) {
                ErrPrintf("powerCtrlTest_maxwell: testELCGPriWakeup could not update LW_PLWJPG2_FALCON_MAILBOX0_DATA to make write data 0x%08x\n", wv);
            } else {
                DebugPrintf("powerCtrlTest_maxwell: testELCGPriWakeup write data with wakeup_dly_cnt update will be 0x%08x\n", wv);
            }
            // Verify PRI wakeup by reading back the BLKCG PRI reg and comparing with the write data
            rv = 0;
            if (lwgpu->GetRegFldNum("LW_PLWJPG2_FALCON_MAILBOX0","_DATA", &rv)) {
                ErrPrintf("powerCtrlTest_maxwell: could not read LW_PLWJPG2_FALCON_MAILBOX0 register\n");
            }
            if (rv != wv) {
                if ((rv >> 8) == 0xbadf13) {
                    InfoPrintf("powerCtrlTest_maxwell: testELCGPriWakeup LWJPG2 floorsweep. Skip the check\n");
                }else{
                    ErrPrintf("powerCtrlTest_maxwell: testELCGPriWakeup didn't read back LW_PLWJPG2_FALCON_MAILBOX0_DATA correctly, wv = 0x%08x, rv = 0x%8x\n", wv, rv);
                    test_status_fail = true;
                }
            } else {
                InfoPrintf("powerCtrlTest_maxwell: testELCGPriWakeup read back LW_PLWJPG2_FALCON_MAILBOX0_DATA correctly, wv = 0x%08x, rv = 0x%8x\n", wv, rv);
            }
        }

    if(macros.lw_scal_litter_num_lwjpgs > 3) {
            powerCtrlTest_maxwell::DelayNs(50000);     // Give time for engine to go idle.
            ResetELCGClkCounters();
            clk_count_1 = ReadELCGClkCounters();
            powerCtrlTest_maxwell::DelayNs(150);
            clk_count_2 = ReadELCGClkCounters();
            ResetELCGClkCounters();
            clk_count = SubtractEngineClkCounters(clk_count_1, clk_count_2);
            printEngineClkCounters(clk_count);
            checkClkGating(clk_count, Gated, allELCGGatedType, CheckComplement);
            InfoPrintf("powerCtrlTest_maxwell: testELCGPriWakeup, done checking clocks for all engines are gated before testing lwjpg3 pri access\n");

            wv = wakeup_dly_cnt_val;
            if (lwgpu->SetRegFldNum("LW_PLWJPG3_FALCON_MAILBOX0","_DATA", wakeup_dly_cnt_val, 0)) {
                ErrPrintf("powerCtrlTest_maxwell: testELCGPriWakeup could not update LW_PLWJPG3_FALCON_MAILBOX0_DATA to make write data 0x%08x\n", wv);
            } else {
                DebugPrintf("powerCtrlTest_maxwell: testELCGPriWakeup write data with wakeup_dly_cnt update will be 0x%08x\n", wv);
            }
            // Verify PRI wakeup by reading back the BLKCG PRI reg and comparing with the write data
            rv = 0;
            if (lwgpu->GetRegFldNum("LW_PLWJPG3_FALCON_MAILBOX0","_DATA", &rv)) {
                ErrPrintf("powerCtrlTest_maxwell: could not read LW_PLWJPG3_FALCON_MAILBOX0 register\n");
            }
            if (rv != wv) {
                if ((rv >> 8) == 0xbadf13) {
                    InfoPrintf("powerCtrlTest_maxwell: testELCGPriWakeup LWJPG3 floorsweep. Skip the check\n");
                }else{
                    ErrPrintf("powerCtrlTest_maxwell: testELCGPriWakeup didn't read back LW_PLWJPG3_FALCON_MAILBOX0_DATA correctly, wv = 0x%08x, rv = 0x%8x\n", wv, rv);
                    test_status_fail = true;
                }
            } else {
                InfoPrintf("powerCtrlTest_maxwell: testELCGPriWakeup read back LW_PLWJPG3_FALCON_MAILBOX0_DATA correctly, wv = 0x%08x, rv = 0x%8x\n", wv, rv);
            }
        }

    if(macros.lw_scal_litter_num_lwjpgs > 4) {
            powerCtrlTest_maxwell::DelayNs(50000);     // Give time for engine to go idle.
            ResetELCGClkCounters();
            clk_count_1 = ReadELCGClkCounters();
            powerCtrlTest_maxwell::DelayNs(150);
            clk_count_2 = ReadELCGClkCounters();
            ResetELCGClkCounters();
            clk_count = SubtractEngineClkCounters(clk_count_1, clk_count_2);
            printEngineClkCounters(clk_count);
            checkClkGating(clk_count, Gated, allELCGGatedType, CheckComplement);
            InfoPrintf("powerCtrlTest_maxwell: testELCGPriWakeup, done checking clocks for all engines are gated before testing lwjpg4 pri access\n");

            wv = wakeup_dly_cnt_val;
            if (lwgpu->SetRegFldNum("LW_PLWJPG4_FALCON_MAILBOX0","_DATA", wakeup_dly_cnt_val, 0)) {
                ErrPrintf("powerCtrlTest_maxwell: testELCGPriWakeup could not update LW_PLWJPG4_FALCON_MAILBOX0_DATA to make write data 0x%08x\n", wv);
            } else {
                DebugPrintf("powerCtrlTest_maxwell: testELCGPriWakeup write data with wakeup_dly_cnt update will be 0x%08x\n", wv);
            }
            // Verify PRI wakeup by reading back the BLKCG PRI reg and comparing with the write data
            rv = 0;
            if (lwgpu->GetRegFldNum("LW_PLWJPG4_FALCON_MAILBOX0","_DATA", &rv)) {
                ErrPrintf("powerCtrlTest_maxwell: could not read LW_PLWJPG4_FALCON_MAILBOX0 register\n");
            }
            if (rv != wv) {
                if ((rv >> 8) == 0xbadf13) {
                    InfoPrintf("powerCtrlTest_maxwell: testELCGPriWakeup LWJPG4 floorsweep. Skip the check\n");
                }else{
                    ErrPrintf("powerCtrlTest_maxwell: testELCGPriWakeup didn't read back LW_PLWJPG4_FALCON_MAILBOX0_DATA correctly, wv = 0x%08x, rv = 0x%8x\n", wv, rv);
                    test_status_fail = true;
                }
            } else {
                InfoPrintf("powerCtrlTest_maxwell: testELCGPriWakeup read back LW_PLWJPG4_FALCON_MAILBOX0_DATA correctly, wv = 0x%08x, rv = 0x%8x\n", wv, rv);
            }
        }

    if(macros.lw_scal_litter_num_lwjpgs > 5) {
            powerCtrlTest_maxwell::DelayNs(50000);     // Give time for engine to go idle.
            ResetELCGClkCounters();
            clk_count_1 = ReadELCGClkCounters();
            powerCtrlTest_maxwell::DelayNs(150);
            clk_count_2 = ReadELCGClkCounters();
            ResetELCGClkCounters();
            clk_count = SubtractEngineClkCounters(clk_count_1, clk_count_2);
            printEngineClkCounters(clk_count);
            checkClkGating(clk_count, Gated, allELCGGatedType, CheckComplement);
            InfoPrintf("powerCtrlTest_maxwell: testELCGPriWakeup, done checking clocks for all engines are gated before testing lwjpg5 pri access\n");

            wv = wakeup_dly_cnt_val;
            if (lwgpu->SetRegFldNum("LW_PLWJPG5_FALCON_MAILBOX0","_DATA", wakeup_dly_cnt_val, 0)) {
                ErrPrintf("powerCtrlTest_maxwell: testELCGPriWakeup could not update LW_PLWJPG5_FALCON_MAILBOX0_DATA to make write data 0x%08x\n", wv);
            } else {
                DebugPrintf("powerCtrlTest_maxwell: testELCGPriWakeup write data with wakeup_dly_cnt update will be 0x%08x\n", wv);
            }
            // Verify PRI wakeup by reading back the BLKCG PRI reg and comparing with the write data
            rv = 0;
            if (lwgpu->GetRegFldNum("LW_PLWJPG5_FALCON_MAILBOX0","_DATA", &rv)) {
                ErrPrintf("powerCtrlTest_maxwell: could not read LW_PLWJPG5_FALCON_MAILBOX0 register\n");
            }
            if (rv != wv) {
                if ((rv >> 8) == 0xbadf13) {
                    InfoPrintf("powerCtrlTest_maxwell: testELCGPriWakeup LWJPG5 floorsweep. Skip the check\n");
                }else{
                    ErrPrintf("powerCtrlTest_maxwell: testELCGPriWakeup didn't read back LW_PLWJPG5_FALCON_MAILBOX0_DATA correctly, wv = 0x%08x, rv = 0x%8x\n", wv, rv);
                    test_status_fail = true;
                }
            } else {
                InfoPrintf("powerCtrlTest_maxwell: testELCGPriWakeup read back LW_PLWJPG05FALCON_MAILBOX0_DATA correctly, wv = 0x%08x, rv = 0x%8x\n", wv, rv);
            }
        }

    if(macros.lw_scal_litter_num_lwjpgs > 6) {
            powerCtrlTest_maxwell::DelayNs(50000);     // Give time for engine to go idle.
            ResetELCGClkCounters();
            clk_count_1 = ReadELCGClkCounters();
            powerCtrlTest_maxwell::DelayNs(150);
            clk_count_2 = ReadELCGClkCounters();
            ResetELCGClkCounters();
            clk_count = SubtractEngineClkCounters(clk_count_1, clk_count_2);
            printEngineClkCounters(clk_count);
            checkClkGating(clk_count, Gated, allELCGGatedType, CheckComplement);
            InfoPrintf("powerCtrlTest_maxwell: testELCGPriWakeup, done checking clocks for all engines are gated before testing lwjpg6 pri access\n");

            wv = wakeup_dly_cnt_val;
            if (lwgpu->SetRegFldNum("LW_PLWJPG6_FALCON_MAILBOX0","_DATA", wakeup_dly_cnt_val, 0)) {
                ErrPrintf("powerCtrlTest_maxwell: testELCGPriWakeup could not update LW_PLWJPG6_FALCON_MAILBOX0_DATA to make write data 0x%08x\n", wv);
            } else {
                DebugPrintf("powerCtrlTest_maxwell: testELCGPriWakeup write data with wakeup_dly_cnt update will be 0x%08x\n", wv);
            }
            // Verify PRI wakeup by reading back the BLKCG PRI reg and comparing with the write data
            rv = 0;
            if (lwgpu->GetRegFldNum("LW_PLWJPG6_FALCON_MAILBOX0","_DATA", &rv)) {
                ErrPrintf("powerCtrlTest_maxwell: could not read LW_PLWJPG6_FALCON_MAILBOX0 register\n");
            }
            if (rv != wv) {
                if ((rv >> 8) == 0xbadf13) {
                    InfoPrintf("powerCtrlTest_maxwell: testELCGPriWakeup LWJPG6 floorsweep. Skip the check\n");
                }else{
                    ErrPrintf("powerCtrlTest_maxwell: testELCGPriWakeup didn't read back LW_PLWJPG6_FALCON_MAILBOX0_DATA correctly, wv = 0x%08x, rv = 0x%8x\n", wv, rv);
                    test_status_fail = true;
                }
            } else {
                InfoPrintf("powerCtrlTest_maxwell: testELCGPriWakeup read back LW_PLWJPG6_FALCON_MAILBOX0_DATA correctly, wv = 0x%08x, rv = 0x%8x\n", wv, rv);
            }
        }

    if(macros.lw_scal_litter_num_lwjpgs > 7) {
            powerCtrlTest_maxwell::DelayNs(50000);     // Give time for engine to go idle.
            ResetELCGClkCounters();
            clk_count_1 = ReadELCGClkCounters();
            powerCtrlTest_maxwell::DelayNs(150);
            clk_count_2 = ReadELCGClkCounters();
            ResetELCGClkCounters();
            clk_count = SubtractEngineClkCounters(clk_count_1, clk_count_2);
            printEngineClkCounters(clk_count);
            checkClkGating(clk_count, Gated, allELCGGatedType, CheckComplement);
            InfoPrintf("powerCtrlTest_maxwell: testELCGPriWakeup, done checking clocks for all engines are gated before testing lwjpg7 pri access\n");

            wv = wakeup_dly_cnt_val;
            if (lwgpu->SetRegFldNum("LW_PLWJPG7_FALCON_MAILBOX0","_DATA", wakeup_dly_cnt_val, 0)) {
                ErrPrintf("powerCtrlTest_maxwell: testELCGPriWakeup could not update LW_PLWJPG7_FALCON_MAILBOX0_DATA to make write data 0x%08x\n", wv);
            } else {
                DebugPrintf("powerCtrlTest_maxwell: testELCGPriWakeup write data with wakeup_dly_cnt update will be 0x%08x\n", wv);
            }
            // Verify PRI wakeup by reading back the BLKCG PRI reg and comparing with the write data
            rv = 0;
            if (lwgpu->GetRegFldNum("LW_PLWJPG7_FALCON_MAILBOX0","_DATA", &rv)) {
                ErrPrintf("powerCtrlTest_maxwell: could not read LW_PLWJPG7_FALCON_MAILBOX0 register\n");
            }
            if (rv != wv) {
                if ((rv >> 8) == 0xbadf13) {
                    InfoPrintf("powerCtrlTest_maxwell: testELCGPriWakeup LWJPG7 floorsweep. Skip the check\n");
                }else{
                    ErrPrintf("powerCtrlTest_maxwell: testELCGPriWakeup didn't read back LW_PLWJPG7_FALCON_MAILBOX0_DATA correctly, wv = 0x%08x, rv = 0x%8x\n", wv, rv);
                    test_status_fail = true;
                }
            } else {
                InfoPrintf("powerCtrlTest_maxwell: testELCGPriWakeup read back LW_PLWJPG07FALCON_MAILBOX0_DATA correctly, wv = 0x%08x, rv = 0x%8x\n", wv, rv);
            }
        }

    if(macros.lw_engine_sec) {
        InfoPrintf("powerCtrlTest_maxwell: testELCGPriWakeup, start checking pri access of sec\n");

        powerCtrlTest_maxwell::DelayNs(50000);     // Give time for engine to go idle.
        ResetELCGClkCounters();
        clk_count_1 = ReadELCGClkCounters();
        powerCtrlTest_maxwell::DelayNs(150);
        clk_count_2 = ReadELCGClkCounters();
        ResetELCGClkCounters();
        clk_count = SubtractEngineClkCounters(clk_count_1, clk_count_2);
        printEngineClkCounters(clk_count);
        checkClkGating(clk_count, Gated, allELCGGatedType, CheckComplement);
        InfoPrintf("powerCtrlTest_maxwell: testELCGPriWakeup, done checking clocks for all engines are gated before testing sec pri access\n");

        wv = 0xbeefdead;
        if (lwgpu->SetRegFldNum("LW_PSEC_FALCON_MAILBOX0","", wv, 0)) {
            ErrPrintf("powerCtrlTest_maxwell: testELCGPriWakeup could not update LW_PSEC_FALCON_MAILBOX0 to make write data 0x%08x\n", wv);
        } else {
            DebugPrintf("powerCtrlTest_maxwell: testELCGPriWakeup write data with update will be 0x%08x\n", wv);
        }
        //Verify PRI wakeup by reading back the BLKCG PRI reg and comparing with the write data
        rv = 0;
        if (lwgpu->GetRegFldNum("LW_PSEC_FALCON_MAILBOX0","", &rv)) {
            ErrPrintf("powerCtrlTest_maxwell: could not read LW_PSEC_FALCON_MAILBOX0 register\n");
        }
        if (rv != wv) {
            ErrPrintf("powerCtrlTest_maxwell: testELCGPriWakeup didn't read back LW_PSEC_FALCON_MAILBOX0 correctly, wv = 0x%08x, rv = 0x%8x\n", wv, rv);
            test_status_fail = true;
        } else {
            InfoPrintf("powerCtrlTest_maxwell: testELCGPriWakeup read back LW_PSEC_FALCON_MAILBOX0 correctly, wv = 0x%08x, rv = 0x%8x\n", wv, rv);
        }
        InfoPrintf("powerCtrlTest_maxwell: testELCGPriWakeup, done checking pri access of sec\n");
    }
    //
    if (test_status_fail) {
        ErrPrintf("powerCtrlTest_maxwell: testELCGPriWakeup, Failed in checking ELCG waken up by pri accessp\n");
    }
    else {
        InfoPrintf("powerCtrlTest_maxwell: testELCGPriWakeup, Passed in checking ELCG waken up by pri accessp\n");
    }
}

/*
 * Test function: testClkOnForReset
 *  - Verify that each engine enables the clocks after reset
 */
void powerCtrlTest_maxwell::testClkOnForReset(void)
{
  unsigned long long int gatedType = 0 ;
  MaxwellClkCount fullpower_clk_count;
  MaxwellClkCount clk_count_1, clk_count_2, clk_count;
  MaxwellClkCount tmp_clkcount1, tmp_clkcount2, base_clkcount;
  bool Gated = true;
  bool NotGated = false;
  bool CheckComplement = true;
  bool DontCheckComplement = false;
  UINT32 data, mask;

  InfoPrintf("powerCtrlTest_maxwell: Testing Per Engine Clock Running for Reset.\n");

  //////////////////////////////////////////////////////
  // Run all engines in FULLPOWER and verify that clocks ticks
  InfoPrintf("powerCtrlTest_maxwell: testClkOnForReset, putting all engines in FULLPOWER mode and verifying all clks are NOT gated due to idle\n");
  if (!m_ch->WaitForIdle()) {
    ErrPrintf("powerCtrlTest_maxwell Channel WaitForIdle failed\n");
    return;
  }
  // Turn off idle clk slowdown
  DisableclkSlowdown();
  // Set engines to full power
  AllEngineMode(CG_FULLPOWER);
  // Disable BLCG
  AllEngineForceClksOn(CG_FULLPOWER);
    EnableELCGClkCounters();

  powerCtrlTest_maxwell::DelayNs(10000); //give time for engines to go idle
  ResetELCGClkCounters();
  InfoPrintf("powerCtrlTest_maxwell: 1nd reading counters\n");
  clk_count_1 = ReadELCGClkCounters();
  powerCtrlTest_maxwell::DelayNs(150);
  InfoPrintf("powerCtrlTest_maxwell: 2nd reading counters\n");
  clk_count_2 = ReadELCGClkCounters();
  ResetELCGClkCounters();
  fullpower_clk_count = SubtractEngineClkCounters(clk_count_1, clk_count_2);
  printEngineClkCounters(fullpower_clk_count);
  // Make sure clks of all engine types are NOT suspended
  checkClkGating(fullpower_clk_count, NotGated, allELCGGatedType, DontCheckComplement);
  InfoPrintf("powerCtrlTest_maxwell: testClkOnForReset, done checking clocks for all engines FULLPOWER\n");

  //////////////////////////////////////////////////////
  // Put all engines in AUTOMATIC and verify clocks
  InfoPrintf("powerCtrlTest_maxwell: testClkOnForReset, putting all engines in AUTOMATIC mode and verifying all clks are gated due to idle\n");
  if (!m_ch->WaitForIdle()) {
    ErrPrintf("powerCtrlTest_maxwell: Channel WaitForIdle failed\n");
    return;
  }
  AllEngineMode(CG_AUTOMATIC);

  // Give time for engine to go idle.
  powerCtrlTest_maxwell::DelayNs(50000);     // Give time for engine to go idle.
  ResetELCGClkCounters();
  clk_count_1 = ReadELCGClkCounters();
  powerCtrlTest_maxwell::DelayNs(150);
  clk_count_2 = ReadELCGClkCounters();
  ResetELCGClkCounters();
  clk_count = SubtractEngineClkCounters(clk_count_1, clk_count_2);
  printEngineClkCounters(clk_count);
  checkClkGating(clk_count, Gated, allELCGGatedType, CheckComplement);
  InfoPrintf("powerCtrlTest_maxwell: testClkOnForReset, done checking clocks for all engines AUTOMATIC\n");

  //set eng_delay_before to be a big value so that we can verify that clock is running before
  //elcg will re-enabled
  //for (UINT32 i=0; i<24; i++) {
  //  lwgpu->SetRegFldDef("LW_THERM_GATE_CTRL(i)", "_ENG_DELAY_BEFORE",  "_MAX");
  //}

  //Enable reset for each engine and verify its clock is running after reset
    powerCtrlTest_maxwell::DelayNs(500);
    UINT32 testcase = macros.lw_host_max_num_engines;

  for (UINT32 tmp_count=0; tmp_count < testcase; tmp_count++) {

    if (tmp_count == macros.lw_engine_graphics) {
      InfoPrintf("powerCtrlTest_maxwell: testClkOnForReset checking GR clock runing for reset\n");
      gatedType = maxwell_clock_gating.MAXWELL_CLOCK_GR_ENGINE_GATED;
      lwgpu->GetRegFldNum("LW_PMC_DEVICE_ENABLE(macros.lw_device_info_graphics0_reset/32)", "", &data);
      mask = 0x00000000;
      mask = mask | (0x1 << macros.lw_device_info_graphics0_reset%32);
      mask = ~mask;
      data = data & mask;
      if (lwgpu->SetRegFldNum("LW_PMC_DEVICE_ENABLE(macros.lw_device_info_graphics0_reset/32)","",data)) {
        ErrPrintf("Could not be updated LW_PMC_DEVICE_ENABLE_0 to value 0x%x\n", data);
      }
    } else if ((tmp_count == macros.lw_engine_copy0) && (macros.lw_scal_litter_num_ce_lces > 0)) {
        InfoPrintf("powerCtrlTest_maxwell: testClkOnForReset checking CE0 clock runing for reset\n");
        gatedType = maxwell_clock_gating.MAXWELL_CLOCK_CE0_ENGINE_GATED;
        lwgpu->GetRegFldNum("LW_PMC_DEVICE_ENABLE(macros.lw_device_info_copy0_reset/32)", "", &data);
        mask = 0x00000000;
        mask = mask | (0x1 << macros.lw_device_info_copy0_reset%32);
        mask = ~mask;
        data = data & mask;
        if (lwgpu->SetRegFldNum("LW_PMC_DEVICE_ENABLE(macros.lw_device_info_copy0_reset/32)","",data)) {
            ErrPrintf("Could not be updated LW_PMC_DEVICE_ENABLE_0 to value 0x%x \n", data);
         }
    } else if ((tmp_count == macros.lw_engine_copy1) && (macros.lw_scal_litter_num_ce_lces > 1)){
        InfoPrintf("powerCtrlTest_maxwell: testClkOnForReset checking CE1 clock runing for reset\n");
        gatedType = maxwell_clock_gating.MAXWELL_CLOCK_CE1_ENGINE_GATED;
        lwgpu->GetRegFldNum("LW_PMC_DEVICE_ENABLE(macros.lw_device_info_copy1_reset/32)", "", &data);
        mask = 0x00000000;
        mask = mask | (0x1 << macros.lw_device_info_copy1_reset%32);
        mask = ~mask;
        data = data & mask;
        if (lwgpu->SetRegFldNum("LW_PMC_DEVICE_ENABLE(macros.lw_device_info_copy1_reset/32)","",data)) {
            ErrPrintf("Could not be updated LW_PMC_DEVICE_ENABLE_0 to value 0x%x\n", data);
       }
    } else if ((tmp_count == macros.lw_engine_copy2) && (macros.lw_scal_litter_num_ce_lces > 2)){
        InfoPrintf("powerCtrlTest_maxwell: testClkOnForReset checking CE2 clock runing for reset\n");
        gatedType = maxwell_clock_gating.MAXWELL_CLOCK_CE2_ENGINE_GATED;
        lwgpu->GetRegFldNum("LW_PMC_DEVICE_ENABLE(macros.lw_device_info_copy2_reset/32)", "", &data);
        mask = 0x00000000;
        mask = mask | (0x1 << macros.lw_device_info_copy2_reset%32);
        mask = ~mask;
        data = data & mask;
        if (lwgpu->SetRegFldNum("LW_PMC_DEVICE_ENABLE(macros.lw_device_info_copy2_reset/32)","", data)) {
            ErrPrintf("Could not be updated LW_PMC_DEVICE_ENABLE_0 to value 0x%x\n", data);
        }
    } else if ((tmp_count == macros.lw_engine_copy3) && (macros.lw_scal_litter_num_ce_lces > 3)) {
        InfoPrintf("powerCtrlTest_maxwell: testClkOnForReset checking CE3 clock runing for reset\n");
        gatedType = maxwell_clock_gating.MAXWELL_CLOCK_CE3_ENGINE_GATED;
        lwgpu->GetRegFldNum("LW_PMC_DEVICE_ENABLE(macros.lw_device_info_copy3_reset/32)", "", &data);
        mask = 0x00000000;
        mask = mask | (0x1 << macros.lw_device_info_copy3_reset%32);
        mask = ~mask;
        data = data & mask;
        if (lwgpu->SetRegFldNum("LW_PMC_DEVICE_ENABLE(macros.lw_device_info_copy3_reset/32)","",data)) {
            ErrPrintf("Could not be updated LW_PMC_DEVICE_ENABLE_0 to value 0x%x\n", data);
         }
    } else if ((tmp_count == macros.lw_engine_copy4 ) && (macros.lw_scal_litter_num_ce_lces > 4)) {
        InfoPrintf("powerCtrlTest_maxwell: testClkOnForReset checking CE4 clock runing for reset\n");
        gatedType = maxwell_clock_gating.MAXWELL_CLOCK_CE4_ENGINE_GATED;
        lwgpu->GetRegFldNum("LW_PMC_DEVICE_ENABLE(macros.lw_device_info_copy4_reset/32)", "", &data);
        mask = 0x00000000;
        mask = mask | (0x1 << macros.lw_device_info_copy4_reset%32);
        mask = ~mask;
        data = data & mask;
        if (lwgpu->SetRegFldNum("LW_PMC_DEVICE_ENABLE(macros.lw_device_info_copy4_reset/32)","", data)) {
            ErrPrintf("Could not be updated LW_PMC_DEVICE_ENABLE_0 to value 0x%x\n", data);
       }
    } else if ((tmp_count == macros.lw_engine_copy5 ) && (macros.lw_scal_litter_num_ce_lces > 5)) {
        InfoPrintf("powerCtrlTest_maxwell: testClkOnForReset checking CE5 clock runing for reset\n");
        gatedType = maxwell_clock_gating.MAXWELL_CLOCK_CE5_ENGINE_GATED;
        lwgpu->GetRegFldNum("LW_PMC_DEVICE_ENABLE(macros.lw_device_info_copy5_reset/32)", "", &data);
        mask = 0x00000000;
        mask = mask | (0x1 << macros.lw_device_info_copy5_reset%32);
        mask = ~mask;
        data = data & mask;
        if (lwgpu->SetRegFldNum("LW_PMC_DEVICE_ENABLE(macros.lw_device_info_copy5_reset/32)","", data)) {
            ErrPrintf("Could not be updated LW_PMC_DEVICE_ENABLE_0 to value 0x%x\n", data);
        }
    } else if ((tmp_count == macros.lw_engine_copy6 ) && (macros.lw_scal_litter_num_ce_lces > 6)) {
        InfoPrintf("powerCtrlTest_maxwell: testClkOnForReset checking CE6 clock runing for reset\n");
        gatedType = maxwell_clock_gating.MAXWELL_CLOCK_CE6_ENGINE_GATED;
        lwgpu->GetRegFldNum("LW_PMC_DEVICE_ENABLE(macros.lw_device_info_copy6_reset/32)", "", &data);
        mask = 0x00000000;
        mask = mask | (0x1 << macros.lw_device_info_copy6_reset%32);
        mask = ~mask;
        data = data & mask;
        if (lwgpu->SetRegFldNum("LW_PMC_DEVICE_ENABLE(macros.lw_device_info_copy6_reset/32)","", data)) {
            ErrPrintf("Could not be updated LW_PMC_DEVICE_ENABLE_0 to value 0x%x\n", data);
        }
    } else if ((tmp_count == macros.lw_engine_copy7 ) && (macros.lw_scal_litter_num_ce_lces > 7)) {
        InfoPrintf("powerCtrlTest_maxwell: testClkOnForReset checking CE7 clock runing for reset\n");
        gatedType = maxwell_clock_gating.MAXWELL_CLOCK_CE7_ENGINE_GATED;
        lwgpu->GetRegFldNum("LW_PMC_DEVICE_ENABLE(macros.lw_device_info_copy7_reset/32)", "", &data);
        mask = 0x00000000;
        mask = mask | (0x1 << macros.lw_device_info_copy7_reset%32);
        mask = ~mask;
        data = data & mask;
        if (lwgpu->SetRegFldNum("LW_PMC_DEVICE_ENABLE(macros.lw_device_info_copy7_reset/32)","", data)) {
            ErrPrintf("Could not be updated LW_PMC_DEVICE_ENABLE_0 to value 0x%x\n", data);
        }
    } else if ((tmp_count == macros.lw_engine_copy8 ) && (macros.lw_scal_litter_num_ce_lces > 8)) {
        InfoPrintf("powerCtrlTest_maxwell: testClkOnForReset checking CE8 clock runing for reset\n");
        gatedType = maxwell_clock_gating.MAXWELL_CLOCK_CE8_ENGINE_GATED;
        lwgpu->GetRegFldNum("LW_PMC_DEVICE_ENABLE(macros.lw_device_info_copy8_reset/32)", "", &data);
        mask = 0x00000000;
        mask = mask | (0x1 << macros.lw_device_info_copy8_reset%32);
        mask = ~mask;
        data = data & mask;
        if (lwgpu->SetRegFldNum("LW_PMC_DEVICE_ENABLE(macros.lw_device_info_copy8_reset/32)","", data)) {
            ErrPrintf("Could not be updated LW_PMC_DEVICE_ENABLE_0 to value 0x%x\n", data);
        }
    } else if ((tmp_count == macros.lw_engine_copy9 ) && (macros.lw_scal_litter_num_ce_lces > 9)) {
        InfoPrintf("powerCtrlTest_maxwell: testClkOnForReset checking CE9 clock runing for reset\n");
        gatedType = maxwell_clock_gating.MAXWELL_CLOCK_CE9_ENGINE_GATED;
        lwgpu->GetRegFldNum("LW_PMC_DEVICE_ENABLE(macros.lw_device_info_copy9_reset/32)", "", &data);
        mask = 0x00000000;
        mask = mask | (0x1 << macros.lw_device_info_copy9_reset%32);
        mask = ~mask;
        data = data & mask;
        if (lwgpu->SetRegFldNum("LW_PMC_DEVICE_ENABLE(macros.lw_device_info_copy9_reset/32)","", data)) {
            ErrPrintf("Could not be updated LW_PMC_DEVICE_ENABLE_0 to value 0x%x\n", data);
        }
    } else if (tmp_count == macros.lw_engine_lwenc ) {
        InfoPrintf("powerCtrlTest_maxwell: testClkOnForReset checking LWENC clock runing for reset\n");
        gatedType = maxwell_clock_gating.MAXWELL_CLOCK_LWENC_ENGINE_GATED;
        lwgpu->GetRegFldNum("LW_PMC_DEVICE_ENABLE(macros.lw_device_info_lwenc0_reset/32)", "", &data);
        mask = 0x00000000;
        mask = mask | (0x1 << macros.lw_device_info_lwenc0_reset%32);
        mask = ~mask;
        data = data & mask;
        if (lwgpu->SetRegFldNum("LW_PMC_DEVICE_ENABLE(macros.lw_device_info_lwenc0_reset/32)","",data)) {
            ErrPrintf("Could not be updated LW_PMC_DEVICE_ENABLE_0 to value 0x%x\n", data);
        }
    } else if (tmp_count == macros.lw_engine_lwdec) {
        InfoPrintf("powerCtrlTest_maxwell: testClkOnForReset checking LWDEC clock runing for reset\n");
        gatedType = maxwell_clock_gating.MAXWELL_CLOCK_LWDEC_ENGINE_GATED;
        lwgpu->GetRegFldNum("LW_PMC_DEVICE_ENABLE(macros.lw_device_info_lwdec0_reset/32)", "", &data);
        mask = 0x00000000;
        mask = mask | (0x1 << macros.lw_device_info_lwdec0_reset%32);
        mask = ~mask;
        data = data & mask;
        if (lwgpu->SetRegFldNum("LW_PMC_DEVICE_ENABLE(macros.lw_device_info_lwdec0_reset/32)","",data)) {
            ErrPrintf("Could not be updated LW_PMC_DEVICE_ENABLE_0 to value 0x%x\n", data);
        }
    } else if (tmp_count == macros.lw_engine_lwdec1 && macros.lw_scal_chip_num_lwdecs > 1) {
        InfoPrintf("powerCtrlTest_maxwell: testClkOnForReset checking LWDEC1 clock running for reset\n");
        gatedType = maxwell_clock_gating.MAXWELL_CLOCK_LWDEC1_ENGINE_GATED;
        lwgpu->GetRegFldNum("LW_PMC_DEVICE_ENABLE(macros.lw_device_info_lwdec1_reset/32)", "", &data);
        mask = 0x00000000;
        mask = mask | (0x1 << macros.lw_device_info_lwdec1_reset%32);
        mask = ~mask;
        data = data & mask;
        if (lwgpu->SetRegFldNum("LW_PMC_DEVICE_ENABLE(macros.lw_device_info_lwdec1_reset/32)", "", data)) {
            ErrPrintf("Could not be updated LW_PMC_DEVICE_ENABLE_0 to value 0x%x\n", data);
        }
    } else if (tmp_count == macros.lw_engine_lwdec2 && macros.lw_scal_chip_num_lwdecs > 2) {
        InfoPrintf("powerCtrlTest_maxwell: testClkOnForReset checking LWDEC2 clock running for reset\n");
        gatedType = maxwell_clock_gating.MAXWELL_CLOCK_LWDEC2_ENGINE_GATED;
        lwgpu->GetRegFldNum("LW_PMC_DEVICE_ENABLE(macros.lw_device_info_lwdec2_reset/32)", "", &data);
        mask = 0x00000000;
        mask = mask | (0x1 << macros.lw_device_info_lwdec2_reset%32);
        mask = ~mask;
        data = data & mask;
        if (lwgpu->SetRegFldNum("LW_PMC_DEVICE_ENABLE(macros.lw_device_info_lwdec2_reset/32)", "", data)) {
            ErrPrintf("Could not be updated LW_PMC_DEVICE_ENABLE_0 to value 0x%x\n", data);
        }
    } else if (tmp_count == macros.lw_engine_lwdec3 && macros.lw_scal_chip_num_lwdecs > 3) {
        InfoPrintf("powerCtrlTest_maxwell: testClkOnForReset checking LWDEC3 clock running for reset\n");
        gatedType = maxwell_clock_gating.MAXWELL_CLOCK_LWDEC3_ENGINE_GATED;
        lwgpu->GetRegFldNum("LW_PMC_DEVICE_ENABLE(macros.lw_device_info_lwdec3_reset/32)", "", &data);
        mask = 0x00000000;
        mask = mask | (0x1 << macros.lw_device_info_lwdec3_reset%32);
        mask = ~mask;
        data = data & mask;
        if (lwgpu->SetRegFldNum("LW_PMC_DEVICE_ENABLE(macros.lw_device_info_lwdec3_reset/32)", "", data)) {
            ErrPrintf("Could not be updated LW_PMC_DEVICE_ENABLE_0 to value 0x%x\n", data);
        }
    } else if (tmp_count == macros.lw_engine_lwdec4 && macros.lw_scal_chip_num_lwdecs > 4) {
        InfoPrintf("powerCtrlTest_maxwell: testClkOnForReset checking LWDEC4 clock running for reset\n");
        gatedType = maxwell_clock_gating.MAXWELL_CLOCK_LWDEC4_ENGINE_GATED;
        lwgpu->GetRegFldNum("LW_PMC_DEVICE_ENABLE(macros.lw_device_info_lwdec4_reset/32)", "", &data);
        mask = 0x00000000;
        mask = mask | (0x1 << macros.lw_device_info_lwdec4_reset%32);
        mask = ~mask;
        data = data & mask;
        if (lwgpu->SetRegFldNum("LW_PMC_DEVICE_ENABLE(macros.lw_device_info_lwdec4_reset/32)", "", data)) {
            ErrPrintf("Could not be updated LW_PMC_DEVICE_ENABLE_0 to value 0x%x\n", data);
        }
    } else if (tmp_count == macros.lw_engine_lwdec5 && macros.lw_scal_litter_num_lwdecs > 5) {
        InfoPrintf("powerCtrlTest_maxwell: testClkOnForReset checking LWDEC5 clock running for reset\n");
        gatedType = maxwell_clock_gating.MAXWELL_CLOCK_LWDEC5_ENGINE_GATED;
        lwgpu->GetRegFldNum("LW_PMC_DEVICE_ENABLE(macros.lw_device_info_lwdec5_reset/32)", "", &data);
        mask = 0x00000000;
        mask = mask | (0x1 << macros.lw_device_info_lwdec5_reset%32);
        mask = ~mask;
        data = data & mask;
        if (lwgpu->SetRegFldNum("LW_PMC_DEVICE_ENABLE(macros.lw_device_info_lwdec5_reset/32)", "", data)) {
            ErrPrintf("Could not be updated LW_PMC_DEVICE_ENABLE_0 to value 0x%x\n", data);
        }
    }else if (tmp_count == macros.lw_engine_lwdec6 && macros.lw_scal_litter_num_lwdecs > 6) {
        InfoPrintf("powerCtrlTest_maxwell: testClkOnForReset checking LWDEC6 clock running for reset\n");
        gatedType = maxwell_clock_gating.MAXWELL_CLOCK_LWDEC6_ENGINE_GATED;
        lwgpu->GetRegFldNum("LW_PMC_DEVICE_ENABLE(macros.lw_device_info_lwdec6_reset/32)", "", &data);
        mask = 0x00000000;
        mask = mask | (0x1 << macros.lw_device_info_lwdec6_reset%32);
        mask = ~mask;
        data = data & mask;
        if (lwgpu->SetRegFldNum("LW_PMC_DEVICE_ENABLE(macros.lw_device_info_lwdec6_reset/32)", "", data)) {
            ErrPrintf("Could not be updated LW_PMC_DEVICE_ENABLE_0 to value 0x%x\n", data);
        }
    }else if (tmp_count == macros.lw_engine_lwdec7 && macros.lw_scal_litter_num_lwdecs > 7) {
        InfoPrintf("powerCtrlTest_maxwell: testClkOnForReset checking LWDEC7 clock running for reset\n");
        gatedType = maxwell_clock_gating.MAXWELL_CLOCK_LWDEC7_ENGINE_GATED;
        lwgpu->GetRegFldNum("LW_PMC_DEVICE_ENABLE(macros.lw_device_info_lwdec7_reset/32)", "", &data);
        mask = 0x00000000;
        mask = mask | (0x1 << macros.lw_device_info_lwdec7_reset%32);
        mask = ~mask;
        data = data & mask;
        if (lwgpu->SetRegFldNum("LW_PMC_DEVICE_ENABLE(macros.lw_device_info_lwdec7_reset/32)", "", data)) {
            ErrPrintf("Could not be updated LW_PMC_DEVICE_ENABLE_0 to value 0x%x\n", data);
        }
        }else if (tmp_count == macros.lw_engine_sec) {
        InfoPrintf("powerCtrlTest_maxwell: testClkOnForReset checking SEC clock runing for reset\n");
        gatedType = maxwell_clock_gating.MAXWELL_CLOCK_SEC_ENGINE_GATED;
        lwgpu->GetRegFldNum("LW_PMC_DEVICE_ENABLE(macros.lw_device_info_sec_reset/32)", "", &data);
        mask = 0x00000000;
        mask = mask | (0x1 << macros.lw_device_info_sec_reset%32);
        mask = ~mask;
        data = data & mask;
        if (lwgpu->SetRegFldNum("LW_PMC_DEVICE_ENABLE(macros.lw_device_info_sec_reset/32)","", data)) {
            ErrPrintf("Could not be updated LW_PMC_DEVICE_ENABLE_0 to value 0x%x\n", data);
        }
        if (lwgpu->SetRegFldDef("LW_PSEC_FALCON_ENGINE", "_RESET", "_TRUE")){
            ErrPrintf("Could not write _TRUE to _RESET\n");
        }
        Platform::Delay(1);
        if (lwgpu->SetRegFldDef("LW_PSEC_FALCON_ENGINE", "_RESET", "_FALSE")){
            ErrPrintf("Could not write _FALSE to _RESET\n");
        }
    } else if (tmp_count == macros.lw_engine_lwenc2) {
        InfoPrintf("powerCtrlTest_maxwell: testClkOnForReset checking LWENC2 clock runing for reset\n");
        gatedType = maxwell_clock_gating.MAXWELL_CLOCK_LWENC2_ENGINE_GATED;
        lwgpu->GetRegFldNum("LW_PMC_DEVICE_ENABLE(macros.lw_device_info_lwenc2_reset/32)", "", &data);
        mask = 0x00000000;
        mask = mask | (0x1 << macros.lw_device_info_lwenc2_reset%32);
        mask = ~mask;
        data = data & mask;
        if (lwgpu->SetRegFldNum("LW_PMC_DEVICE_ENABLE(macros.lw_device_info_lwenc2_reset/32)","", data)) {
            ErrPrintf("Could not be updated LW_PMC_DEVICE_ENABLE_0 to value 0x%x\n", data);
        }
    } else if (tmp_count == macros.lw_engine_lwenc1) {
        InfoPrintf("powerCtrlTest_maxwell: testClkOnForReset checking LWENC1 clock runing for reset\n");
        gatedType = maxwell_clock_gating.MAXWELL_CLOCK_LWENC1_ENGINE_GATED;
        lwgpu->GetRegFldNum("LW_PMC_DEVICE_ENABLE(macros.lw_device_info_lwenc1_reset/32)", "", &data);
        mask = 0x00000000;
        mask = mask | (0x1 << macros.lw_device_info_lwenc1_reset%32);
        mask = ~mask;
        data = data & mask;
        if (lwgpu->SetRegFldNum("LW_PMC_DEVICE_ENABLE(macros.lw_device_info_lwenc1_reset/32)","", data)) {
            ErrPrintf("Could not be updated LW_PMC_DEVICE_ENABLE_0 to value 0x%x\n", data);
        }
    } else if (tmp_count == macros.lw_engine_lwjpg0) {
        InfoPrintf("powerCtrlTest_maxwell: testClkOnForReset checking LWJPG0 clock running for reset\n");
        gatedType = maxwell_clock_gating.MAXWELL_CLOCK_LWJPG0_ENGINE_GATED;
        lwgpu->GetRegFldNum("LW_PMC_DEVICE_ENABLE(macros.lw_device_info_lwjpg0_reset/32)", "", &data);
        mask = 0x00000000;
        mask = mask | (0x1 << macros.lw_device_info_lwjpg0_reset%32);
        mask = ~mask;
        data = data & mask;
        if (lwgpu->SetRegFldNum("LW_PMC_DEVICE_ENABLE(macros.lw_device_info_lwjpg0_reset/32)", "", data)) {
            ErrPrintf("Could not be updated LW_PMC_DEVICE_ENABLE_0 to value 0x%x\n", data);
        }
    }else if (tmp_count == macros.lw_engine_lwjpg1) {
        InfoPrintf("powerCtrlTest_maxwell: testClkOnForReset checking LWJPG1 clock running for reset\n");
        gatedType = maxwell_clock_gating.MAXWELL_CLOCK_LWJPG1_ENGINE_GATED;
        lwgpu->GetRegFldNum("LW_PMC_DEVICE_ENABLE(macros.lw_device_info_lwjpg1_reset/32)", "", &data);
        mask = 0x00000000;
        mask = mask | (0x1 << (macros.lw_device_info_lwjpg1_reset % 32) );
        mask = ~mask;
        data = data & mask;
        if (lwgpu->SetRegFldNum("LW_PMC_DEVICE_ENABLE(macros.lw_device_info_lwjpg1_reset/32)", "", data)) {
            ErrPrintf("Could not be updated LW_PMC_DEVICE_ENABLE_0 to value 0x%x\n", data);
        }
    }else if (tmp_count == macros.lw_engine_lwjpg2) {
        InfoPrintf("powerCtrlTest_maxwell: testClkOnForReset checking LWJPG2 clock running for reset\n");
        gatedType = maxwell_clock_gating.MAXWELL_CLOCK_LWJPG2_ENGINE_GATED;
        lwgpu->GetRegFldNum("LW_PMC_DEVICE_ENABLE(macros.lw_device_info_lwjpg2_reset/32)", "", &data);
        mask = 0x00000000;
        mask = mask | (0x1 << (macros.lw_device_info_lwjpg2_reset % 32) );
        mask = ~mask;
        data = data & mask;
        if (lwgpu->SetRegFldNum("LW_PMC_DEVICE_ENABLE(macros.lw_device_info_lwjpg2_reset/32)", "", data)) {
            ErrPrintf("Could not be updated LW_PMC_DEVICE_ENABLE_0 to value 0x%x\n", data);
        }
    }else if (tmp_count == macros.lw_engine_lwjpg3) {
        InfoPrintf("powerCtrlTest_maxwell: testClkOnForReset checking LWJPG3 clock running for reset\n");
        gatedType = maxwell_clock_gating.MAXWELL_CLOCK_LWJPG3_ENGINE_GATED;
        lwgpu->GetRegFldNum("LW_PMC_DEVICE_ENABLE(macros.lw_device_info_lwjpg3_reset/32)", "", &data);
        mask = 0x00000000;
        mask = mask | (0x1 << ( macros.lw_device_info_lwjpg3_reset  % 32) );
        mask = ~mask;
        data = data & mask;
        if (lwgpu->SetRegFldNum("LW_PMC_DEVICE_ENABLE(macros.lw_device_info_lwjpg3_reset/32)", "", data)) {
            ErrPrintf("Could not be updated LW_PMC_DEVICE_ENABLE_0 to value 0x%x\n", data);
        }
    }else if (tmp_count == macros.lw_engine_lwjpg4) {
        InfoPrintf("powerCtrlTest_maxwell: testClkOnForReset checking LWJPG4 clock running for reset\n");
        gatedType = maxwell_clock_gating.MAXWELL_CLOCK_LWJPG4_ENGINE_GATED;
        lwgpu->GetRegFldNum("LW_PMC_DEVICE_ENABLE(macros.lw_device_info_lwjpg4_reset/32)", "", &data);
        mask = 0x00000000;
        mask = mask | (0x1 << ( macros.lw_device_info_lwjpg4_reset  % 32) );
        mask = ~mask;
        data = data & mask;
        if (lwgpu->SetRegFldNum("LW_PMC_DEVICE_ENABLE(macros.lw_device_info_lwjpg4_reset/32)", "", data)) {
            ErrPrintf("Could not be updated LW_PMC_DEVICE_ENABLE_0 to value 0x%x\n", data);
        }
    }else if (tmp_count == macros.lw_engine_lwjpg5) {
        InfoPrintf("powerCtrlTest_maxwell: testClkOnForReset checking LWJPG5 clock running for reset\n");
        gatedType = maxwell_clock_gating.MAXWELL_CLOCK_LWJPG5_ENGINE_GATED;
        lwgpu->GetRegFldNum("LW_PMC_DEVICE_ENABLE(macros.lw_device_info_lwjpg5_reset/32)", "", &data);
        mask = 0x00000000;
        mask = mask | (0x1 << (macros.lw_device_info_lwjpg5_reset  % 32) );
        mask = ~mask;
        data = data & mask;
        if (lwgpu->SetRegFldNum("LW_PMC_DEVICE_ENABLE(macros.lw_device_info_lwjpg5_reset/32)", "", data)) {
            ErrPrintf("Could not be updated LW_PMC_DEVICE_ENABLE_0 to value 0x%x\n", data);
        }
    }else if (tmp_count == macros.lw_engine_lwjpg6) {
        InfoPrintf("powerCtrlTest_maxwell: testClkOnForReset checking LWJPG6 clock running for reset\n");
        gatedType = maxwell_clock_gating.MAXWELL_CLOCK_LWJPG6_ENGINE_GATED;
        lwgpu->GetRegFldNum("LW_PMC_DEVICE_ENABLE(macros.lw_device_info_lwjpg6_reset/32)", "", &data);
        mask = 0x00000000;
        mask = mask | (0x1 << ( macros.lw_device_info_lwjpg6_reset % 32) );
        mask = ~mask;
        data = data & mask;
        if (lwgpu->SetRegFldNum("LW_PMC_DEVICE_ENABLE(macros.lw_device_info_lwjpg6_reset/32)", "", data)) {
            ErrPrintf("Could not be updated LW_PMC_DEVICE_ENABLE_0 to value 0x%x\n", data);
        }
    }else if (tmp_count == macros.lw_engine_lwjpg7) {
        InfoPrintf("powerCtrlTest_maxwell: testClkOnForReset checking LWJPG7 clock running for reset\n");
        gatedType = maxwell_clock_gating.MAXWELL_CLOCK_LWJPG7_ENGINE_GATED;
        lwgpu->GetRegFldNum("LW_PMC_DEVICE_ENABLE(macros.lw_device_info_lwjpg7_reset/32)", "", &data);
        mask = 0x00000000;
        mask = mask | (0x1 << ( macros.lw_device_info_lwjpg7_reset%32  ) );
        mask = ~mask;
        data = data & mask;
        if (lwgpu->SetRegFldNum("LW_PMC_DEVICE_ENABLE(macros.lw_device_info_lwjpg7_reset/32)", "", data)) {
            ErrPrintf("Could not be updated LW_PMC_DEVICE_ENABLE_0 to value 0x%x\n", data);
        }
    }else if (tmp_count == macros.lw_engine_ofa0) {
        InfoPrintf("powerCtrlTest_maxwell: testClkOnForReset checking OFA clock running for reset\n");
        gatedType = maxwell_clock_gating.MAXWELL_CLOCK_OFA_ENGINE_GATED;
        lwgpu->GetRegFldNum("LW_PMC_DEVICE_ENABLE(macros.lw_device_info_ofa0_reset/32)", "", &data);
        mask = 0x00000000;
        mask = mask | (0x1 << macros.lw_device_info_ofa0_reset%32);
        mask = ~mask;
        data = data & mask;
        if (lwgpu->SetRegFldNum("LW_PMC_DEVICE_ENABLE(macros.lw_device_info_ofa0_reset/32)", "", data)) {
            ErrPrintf("Could not be updated LW_PMC_DEVICE_ENABLE_0 to value 0x%x\n", data);
        }
    } else {
      InfoPrintf("powerCtrlTest_maxwell: testClkOnForReset idx%d doesn't point to any existing engine.\n", tmp_count);
      gatedType = 0;
    }

    // Skip testing if gatedType == 0
    if (gatedType) {
      // Give time for engine to react to the reset and the pwr2xx_clk_disable signal to transfer to targeted engine through virtual wire.
      powerCtrlTest_maxwell::DelayNs(50000);
      ResetELCGClkCounters();
      clk_count_1 = ReadELCGClkCounters();
      powerCtrlTest_maxwell::DelayNs(150);
      clk_count_2 = ReadELCGClkCounters();
      ResetELCGClkCounters();
      clk_count = SubtractEngineClkCounters(clk_count_1, clk_count_2);
      printEngineClkCounters(clk_count);
      checkClkGating(clk_count, NotGated, gatedType, DontCheckComplement);  //should not be gated when resert is assert
      InfoPrintf("powerCtrlTest_maxwell: testClkOnForReset, done checking this engine clocks on right after reset, gateType=0x%08x\n",gatedType);

      //check again after some time later
      powerCtrlTest_maxwell::DelayNs(50000);
      ResetELCGClkCounters();
      clk_count_1 = ReadELCGClkCounters();
      powerCtrlTest_maxwell::DelayNs(150);
      clk_count_2 = ReadELCGClkCounters();
      ResetELCGClkCounters();
      clk_count = SubtractEngineClkCounters(clk_count_1, clk_count_2);
      printEngineClkCounters(clk_count);
      checkClkGating(clk_count, NotGated, gatedType, DontCheckComplement);  //should not be gated when resert is assert
      InfoPrintf("powerCtrlTest_maxwell: testClkOnForReset, done checking this engine clocks on during reset, gateType=0x%08x\n",gatedType);
    }

    //de-assert reset signals
    if (tmp_count == macros.lw_engine_graphics) {
        lwgpu->GetRegFldNum("LW_PMC_DEVICE_ENABLE(0)", "", &data);
        mask = 0x00000000;
        mask = mask | (0x1 << macros.lw_device_info_graphics0_reset%32);
        if(macros.lw_scal_litter_max_num_smc_engines > 1)
          mask     |= (0x1 << macros.lw_device_info_graphics1_reset%32) ;
        if(macros.lw_scal_litter_max_num_smc_engines > 2)
          mask     |= (0x1 << macros.lw_device_info_graphics2_reset%32) ;
        if(macros.lw_scal_litter_max_num_smc_engines > 3)
          mask     |= (0x1 << macros.lw_device_info_graphics3_reset%32) ;
        if(macros.lw_scal_litter_max_num_smc_engines > 4)
          mask     |= (0x1 << macros.lw_device_info_graphics4_reset%32);
        if(macros.lw_scal_litter_max_num_smc_engines > 5)
          mask     |= (0x1 << macros.lw_device_info_graphics5_reset%32) ;
        if(macros.lw_scal_litter_max_num_smc_engines > 6)
          mask     |= (0x1 << macros.lw_device_info_graphics6_reset%32) ;
        if(macros.lw_scal_litter_max_num_smc_engines > 7)
          mask     |= (0x1 << macros.lw_device_info_graphics7_reset%32);
        data = data | mask;
        if (lwgpu->SetRegFldNum("LW_PMC_DEVICE_ENABLE(0)","",data)) {
            ErrPrintf("Could not be updated LW_PMC_DEVICE_ENABLE_0 to value 0x%x\n", data);
        }
    } else if ((tmp_count == macros.lw_engine_copy0) && (macros.lw_scal_litter_num_ce_lces > 0)){
        lwgpu->GetRegFldNum("LW_PMC_DEVICE_ENABLE(macros.lw_device_info_copy0_reset/32)", "", &data);
        mask = 0x00000000;
        mask = mask | (0x1 << macros.lw_device_info_copy0_reset%32);
        data = data | mask;
       if (lwgpu->SetRegFldNum("LW_PMC_DEVICE_ENABLE(macros.lw_device_info_copy0_reset/32)","", data)) {
      ErrPrintf("Could not be updated LW_PMC_DEVICE_ENABLE_0 to value 0x%x\n", data);
       }
    } else if ((tmp_count == macros.lw_engine_copy1)  && (macros.lw_scal_litter_num_ce_lces > 1)){
        lwgpu->GetRegFldNum("LW_PMC_DEVICE_ENABLE(macros.lw_device_info_copy1_reset/32)", "", &data);
        mask = 0x00000000;
        mask = mask | (0x1 << macros.lw_device_info_copy1_reset%32);
        data = data | mask;
      if (lwgpu->SetRegFldNum("LW_PMC_DEVICE_ENABLE(macros.lw_device_info_copy1_reset/32)","",data)) {
        ErrPrintf("Could not be updated LW_PMC_DEVICE_ENABLE_0 to value 0x%x\n", data);
      }
    } else if ((tmp_count == macros.lw_engine_copy2) && (macros.lw_scal_litter_num_ce_lces > 2)){
        lwgpu->GetRegFldNum("LW_PMC_DEVICE_ENABLE(macros.lw_device_info_copy2_reset/32)", "", &data);
        mask = 0x00000000;
        mask = mask | (0x1 << macros.lw_device_info_copy2_reset%32);
        data = data | mask;
       if (lwgpu->SetRegFldNum("LW_PMC_DEVICE_ENABLE(macros.lw_device_info_copy2_reset/32)","",data)) {
      ErrPrintf("Could not be updated LW_PMC_DEVICE_ENABLE_0 to value 0x%x\n", data);
       }
    } else if ((tmp_count == macros.lw_engine_copy3) && (macros.lw_scal_litter_num_ce_lces > 3)) {
        lwgpu->GetRegFldNum("LW_PMC_DEVICE_ENABLE(macros.lw_device_info_copy3_reset/32)", "", &data);
        mask = 0x00000000;
        mask = mask | (0x1 << macros.lw_device_info_copy3_reset%32);
        data = data | mask;
       if (lwgpu->SetRegFldNum("LW_PMC_DEVICE_ENABLE(macros.lw_device_info_copy3_reset/32)","",data)) {
      ErrPrintf("Could not be updated LW_PMC_DEVICE_ENABLE_0 to value 0x%x\n", data);
       }
    } else if ((tmp_count == macros.lw_engine_copy4) && (macros.lw_scal_litter_num_ce_lces > 4)) {
        lwgpu->GetRegFldNum("LW_PMC_DEVICE_ENABLE(macros.lw_device_info_copy4_reset/32)", "", &data);
        mask = 0x00000000;
        mask = mask | (0x1 << macros.lw_device_info_copy4_reset%32);
        data = data | mask;
       if (lwgpu->SetRegFldNum("LW_PMC_DEVICE_ENABLE(macros.lw_device_info_copy4_reset/32)","",data)) {
      ErrPrintf("Could not be updated LW_PMC_DEVICE_ENABLE_0 to value 0x%x\n", data);
       }
    } else if ((tmp_count == macros.lw_engine_copy5) && (macros.lw_scal_litter_num_ce_lces > 5)) {
        lwgpu->GetRegFldNum("LW_PMC_DEVICE_ENABLE(macros.lw_device_info_copy5_reset/32)", "", &data);
        mask = 0x00000000;
        mask = mask | (0x1 << macros.lw_device_info_copy5_reset%32);
        data = data | mask;
       if (lwgpu->SetRegFldNum("LW_PMC_DEVICE_ENABLE(macros.lw_device_info_copy5_reset/32)","",data)) {
      ErrPrintf("Could not be updated LW_PMC_DEVICE_ENABLE_0 to value 0x%x\n", data);
       }
    } else if ((tmp_count == macros.lw_engine_copy6) && (macros.lw_scal_litter_num_ce_lces > 6)) {
        lwgpu->GetRegFldNum("LW_PMC_DEVICE_ENABLE(macros.lw_device_info_copy6_reset/32)", "", &data);
        mask = 0x00000000;
        mask = mask | (0x1 << macros.lw_device_info_copy6_reset%32);
        data = data | mask;
       if (lwgpu->SetRegFldNum("LW_PMC_DEVICE_ENABLE(macros.lw_device_info_copy6_reset/32)","",data)) {
      ErrPrintf("Could not be updated LW_PMC_DEVICE_ENABLE_0 to value 0x%x\n", data);
       }
    } else if ((tmp_count == macros.lw_engine_copy7) && (macros.lw_scal_litter_num_ce_lces > 7)) {
        lwgpu->GetRegFldNum("LW_PMC_DEVICE_ENABLE(macros.lw_device_info_copy7_reset/32)", "", &data);
        mask = 0x00000000;
        mask = mask | (0x1 << macros.lw_device_info_copy7_reset%32);
        data = data | mask;
       if (lwgpu->SetRegFldNum("LW_PMC_DEVICE_ENABLE(macros.lw_device_info_copy7_reset/32)","",data)) {
      ErrPrintf("Could not be updated LW_PMC_DEVICE_ENABLE_0 to value 0x%x\n", data);
       }
    } else if ((tmp_count == macros.lw_engine_copy8) && (macros.lw_scal_litter_num_ce_lces > 8)) {
        lwgpu->GetRegFldNum("LW_PMC_DEVICE_ENABLE(macros.lw_device_info_copy8_reset/32)", "", &data);
        mask = 0x00000000;
        mask = mask | (0x1 << macros.lw_device_info_copy8_reset%32);
        data = data | mask;
       if (lwgpu->SetRegFldNum("LW_PMC_DEVICE_ENABLE(macros.lw_device_info_copy8_reset/32)","",data)) {
      ErrPrintf("Could not be updated LW_PMC_DEVICE_ENABLE_0 to value 0x%x\n", data);
       }
    } else if ((tmp_count == macros.lw_engine_copy9) && (macros.lw_scal_litter_num_ce_lces > 9)) {
        lwgpu->GetRegFldNum("LW_PMC_DEVICE_ENABLE(macros.lw_device_info_copy9_reset/32)", "", &data);
        mask = 0x00000000;
        mask = mask | (0x1 << macros.lw_device_info_copy9_reset%32);
        data = data | mask;
       if (lwgpu->SetRegFldNum("LW_PMC_DEVICE_ENABLE(macros.lw_device_info_copy9_reset/32)","",data)) {
      ErrPrintf("Could not be updated LW_PMC_DEVICE_ENABLE_0 to value 0x%x\n", data);
       }
    } else if (tmp_count == macros.lw_engine_lwenc) {
        lwgpu->GetRegFldNum("LW_PMC_DEVICE_ENABLE(macros.lw_device_info_lwenc0_reset/32)", "", &data);
        mask = 0x00000000;
        mask = mask | (0x1 << macros.lw_device_info_lwenc0_reset%32);
        data = data | mask;
       if (lwgpu->SetRegFldNum("LW_PMC_DEVICE_ENABLE(macros.lw_device_info_lwenc0_reset/32)","",data)) {
      ErrPrintf("Could not be updated LW_PMC_DEVICE_ENABLE_0 to value 0x%x\n", data);
       }
    } else if (tmp_count == macros.lw_engine_lwdec) {
        lwgpu->GetRegFldNum("LW_PMC_DEVICE_ENABLE(macros.lw_device_info_lwdec0_reset/32)", "", &data);
        mask = 0x00000000;
        mask = mask | (0x1 << macros.lw_device_info_lwdec0_reset%32);
        data = data | mask;
       if (lwgpu->SetRegFldNum("LW_PMC_DEVICE_ENABLE(macros.lw_device_info_lwdec0_reset/32)","",data)) {
      ErrPrintf("Could not be updated LW_PMC_DEVICE_ENABLE_0 to value 0x%x\n", data);
       }
    } else if (tmp_count == macros.lw_engine_lwdec1 && macros.lw_scal_chip_num_lwdecs > 1) {
        lwgpu->GetRegFldNum("LW_PMC_DEVICE_ENABLE(macros.lw_device_info_lwdec1_reset/32)", "", &data);
        mask = 0x00000000;
        mask = mask | (0x1 << macros.lw_device_info_lwdec1_reset%32);
        data = data | mask;
       if (lwgpu->SetRegFldNum("LW_PMC_DEVICE_ENABLE(macros.lw_device_info_lwdec1_reset/32)","",data)) {
      ErrPrintf("Could not be updated LW_PMC_DEVICE_ENABLE_0 to value 0x%x\n", data);
       }
    } else if (tmp_count == macros.lw_engine_lwdec2 && macros.lw_scal_chip_num_lwdecs > 2) {
        lwgpu->GetRegFldNum("LW_PMC_DEVICE_ENABLE(macros.lw_device_info_lwdec2_reset/32)", "", &data);
        mask = 0x00000000;
        mask = mask | (0x1 << macros.lw_device_info_lwdec2_reset%32);
        data = data | mask;
       if (lwgpu->SetRegFldNum("LW_PMC_DEVICE_ENABLE(macros.lw_device_info_lwdec2_reset/32)","",data)) {
      ErrPrintf("Could not be updated LW_PMC_DEVICE_ENABLE_0 to value 0x%x\n", data);
       }
    } else if (tmp_count == macros.lw_engine_lwdec3 && macros.lw_scal_chip_num_lwdecs > 3) {
        lwgpu->GetRegFldNum("LW_PMC_DEVICE_ENABLE(macros.lw_device_info_lwdec3_reset/32)", "", &data);
        mask = 0x00000000;
        mask = mask | (0x1 << macros.lw_device_info_lwdec3_reset%32);
        data = data | mask;
       if (lwgpu->SetRegFldNum("LW_PMC_DEVICE_ENABLE(macros.lw_device_info_lwdec3_reset/32)","",data)) {
      ErrPrintf("Could not be updated LW_PMC_DEVICE_ENABLE_0 to value 0x%x\n", data);
       }
    } else if (tmp_count == macros.lw_engine_lwdec4 && macros.lw_scal_chip_num_lwdecs > 4) {
        lwgpu->GetRegFldNum("LW_PMC_DEVICE_ENABLE(macros.lw_device_info_lwdec4_reset/32)", "", &data);
        mask = 0x00000000;
        mask = mask | (0x1 << macros.lw_device_info_lwdec4_reset%32);
        data = data | mask;
       if (lwgpu->SetRegFldNum("LW_PMC_DEVICE_ENABLE(macros.lw_device_info_lwdec4_reset/32)","",data)) {
      ErrPrintf("Could not be updated LW_PMC_DEVICE_ENABLE_0 to value 0x%x\n", data);
       }
    } else if (tmp_count == macros.lw_engine_lwdec5 && macros.lw_scal_litter_num_lwdecs > 5) {
        lwgpu->GetRegFldNum("LW_PMC_DEVICE_ENABLE(macros.lw_device_info_lwdec5_reset/32)", "", &data);
        mask = 0x00000000;
        mask = mask | (0x1 << macros.lw_device_info_lwdec5_reset%32);
        data = data | mask;
       if (lwgpu->SetRegFldNum("LW_PMC_DEVICE_ENABLE(macros.lw_device_info_lwdec5_reset/32)","",data)) {
      ErrPrintf("Could not be updated LW_PMC_DEVICE_ENABLE_0 to value 0x%x\n", data);
       }
    }else if (tmp_count == macros.lw_engine_lwdec6 && macros.lw_scal_litter_num_lwdecs > 6) {
        lwgpu->GetRegFldNum("LW_PMC_DEVICE_ENABLE(macros.lw_device_info_lwdec6_reset/32)", "", &data);
        mask = 0x00000000;
        mask = mask | (0x1 << macros.lw_device_info_lwdec6_reset%32);
        data = data | mask;
       if (lwgpu->SetRegFldNum("LW_PMC_DEVICE_ENABLE(macros.lw_device_info_lwdec6_reset/32)","",data)) {
      ErrPrintf("Could not be updated LW_PMC_DEVICE_ENABLE_0 to value 0x%x\n", data);
       }
    }else if (tmp_count == macros.lw_engine_lwdec7 && macros.lw_scal_litter_num_lwdecs > 7) {
        lwgpu->GetRegFldNum("LW_PMC_DEVICE_ENABLE(macros.lw_device_info_lwdec7_reset/32)", "", &data);
        mask = 0x00000000;
        mask = mask | (0x1 << macros.lw_device_info_lwdec7_reset%32);
        data = data | mask;
       if (lwgpu->SetRegFldNum("LW_PMC_DEVICE_ENABLE(macros.lw_device_info_lwdec7_reset/32)","",data)) {
      ErrPrintf("Could not be updated LW_PMC_DEVICE_ENABLE_0 to value 0x%x\n", data);
       }
    }else if (tmp_count == macros.lw_engine_sec) {
        lwgpu->GetRegFldNum("LW_PMC_DEVICE_ENABLE(macros.lw_device_info_sec_reset/32)", "", &data);
        mask = 0x00000000;
        mask = mask | (0x1 << macros.lw_device_info_sec_reset%32);
        data = data | mask;
       if (lwgpu->SetRegFldNum("LW_PMC_DEVICE_ENABLE(macros.lw_device_info_sec_reset/32)","",data)) {
      ErrPrintf("Could not be updated LW_PMC_DEVICE_ENABLE_0 to value 0x%x\n", data);
       }
    } else if (tmp_count == macros.lw_engine_lwjpg0 && macros.lw_scal_litter_num_lwjpgs > 0) {
        lwgpu->GetRegFldNum("LW_PMC_DEVICE_ENABLE(macros.lw_device_info_lwjpg0_reset/32)", "", &data);
        mask = 0x00000000;
        mask = mask | (0x1 << macros.lw_device_info_lwjpg0_reset%32);
        data = data | mask;
       if (lwgpu->SetRegFldNum("LW_PMC_DEVICE_ENABLE(macros.lw_device_info_lwjpg0_reset/32)","",data)) {
      ErrPrintf("Could not be updated LW_PMC_DEVICE_ENABLE_0 to value 0x%x\n", data);
       }
    }else if (tmp_count == macros.lw_engine_lwjpg1 && macros.lw_scal_litter_num_lwjpgs > 1) {
        lwgpu->GetRegFldNum("LW_PMC_DEVICE_ENABLE(macros.lw_device_info_lwjpg1_reset/32)", "", &data);
        mask = 0x00000000;
        mask = mask | (0x1 << ( macros.lw_device_info_lwjpg1_reset%32) );
        data = data | mask;
       if (lwgpu->SetRegFldNum("LW_PMC_DEVICE_ENABLE(macros.lw_device_info_lwjpg1_reset/32)","",data)) {
      ErrPrintf("Could not be updated LW_PMC_DEVICE_ENABLE_0 to value 0x%x\n", data);
       }
    }else if (tmp_count == macros.lw_engine_lwjpg2 && macros.lw_scal_litter_num_lwjpgs > 2) {
        lwgpu->GetRegFldNum("LW_PMC_DEVICE_ENABLE(macros.lw_device_info_lwjpg2_reset/32)", "", &data);
        mask = 0x00000000;
        mask = mask | (0x1 << (macros.lw_device_info_lwjpg2_reset%32 ) );
        data = data | mask;
       if (lwgpu->SetRegFldNum("LW_PMC_DEVICE_ENABLE(macros.lw_device_info_lwjpg2_reset/32)","",data)) {
      ErrPrintf("Could not be updated LW_PMC_DEVICE_ENABLE_0 to value 0x%x\n", data);
       }
    }else if (tmp_count == macros.lw_engine_lwjpg3 && macros.lw_scal_litter_num_lwjpgs > 3) {
        lwgpu->GetRegFldNum("LW_PMC_DEVICE_ENABLE(macros.lw_device_info_lwjpg3_reset/32)", "", &data);
        mask = 0x00000000;
        mask = mask | (0x1 << (macros.lw_device_info_lwjpg3_reset%32 % 32) );
        data = data | mask;
       if (lwgpu->SetRegFldNum("LW_PMC_DEVICE_ENABLE(macros.lw_device_info_lwjpg3_reset/32)","",data)) {
      ErrPrintf("Could not be updated LW_PMC_DEVICE_ENABLE_0 to value 0x%x\n", data);
       }
    }else if (tmp_count == macros.lw_engine_lwjpg4 && macros.lw_scal_litter_num_lwjpgs > 4) {
        lwgpu->GetRegFldNum("LW_PMC_DEVICE_ENABLE(macros.lw_device_info_lwjpg4_reset/32)", "", &data);
        mask = 0x00000000;
        mask = mask | (0x1 << (macros.lw_device_info_lwjpg4_reset%32 % 32) );
        data = data | mask;
       if (lwgpu->SetRegFldNum("LW_PMC_DEVICE_ENABLE(macros.lw_device_info_lwjpg4_reset/32)","",data)) {
      ErrPrintf("Could not be updated LW_PMC_DEVICE_ENABLE_0 to value 0x%x\n", data);
       }
    }else if (tmp_count == macros.lw_engine_lwjpg5 && macros.lw_scal_litter_num_lwjpgs > 5) {
        lwgpu->GetRegFldNum("LW_PMC_DEVICE_ENABLE(macros.lw_device_info_lwjpg5_reset/32)", "", &data);
        mask = 0x00000000;
        mask = mask | (0x1 << (macros.lw_device_info_lwjpg5_reset%32 % 32) );
        data = data | mask;
       if (lwgpu->SetRegFldNum("LW_PMC_DEVICE_ENABLE(macros.lw_device_info_lwjpg5_reset/32)","",data)) {
      ErrPrintf("Could not be updated LW_PMC_DEVICE_ENABLE_0 to value 0x%x\n", data);
       }
    }else if (tmp_count == macros.lw_engine_lwjpg6 && macros.lw_scal_litter_num_lwjpgs > 6) {
        lwgpu->GetRegFldNum("LW_PMC_DEVICE_ENABLE(macros.lw_device_info_lwjpg6_reset/32)", "", &data);
        mask = 0x00000000;
        mask = mask | (0x1 << (macros.lw_device_info_lwjpg6_reset%32 % 32) );
        data = data | mask;
       if (lwgpu->SetRegFldNum("LW_PMC_DEVICE_ENABLE(macros.lw_device_info_lwjpg6_reset/32)","",data)) {
      ErrPrintf("Could not be updated LW_PMC_DEVICE_ENABLE_0 to value 0x%x\n", data);
       }
    }else if (tmp_count == macros.lw_engine_lwjpg7 && macros.lw_scal_litter_num_lwjpgs > 7) {
        lwgpu->GetRegFldNum("LW_PMC_DEVICE_ENABLE(macros.lw_device_info_lwjpg7_reset/32)", "", &data);
        mask = 0x00000000;
        mask = mask | (0x1 << (macros.lw_device_info_lwjpg7_reset%32 % 32) );
        data = data | mask;
       if (lwgpu->SetRegFldNum("LW_PMC_DEVICE_ENABLE(macros.lw_device_info_lwjpg7_reset/32)","",data)) {
      ErrPrintf("Could not be updated LW_PMC_DEVICE_ENABLE_0 to value 0x%x\n", data);
       }
    }else if (tmp_count == macros.lw_engine_ofa0) {
        lwgpu->GetRegFldNum("LW_PMC_DEVICE_ENABLE(macros.lw_device_info_ofa0_reset/32)", "", &data);
        mask = 0x00000000;
        mask = mask | (0x1 << macros.lw_device_info_ofa0_reset%32);
        data = data | mask;
       if (lwgpu->SetRegFldNum("LW_PMC_DEVICE_ENABLE(macros.lw_device_info_ofa0_reset/32)","",data)) {
      ErrPrintf("Could not be updated LW_PMC_DEVICE_ENABLE_0 to value 0x%x\n", data);
       }
    } else if (tmp_count == macros.lw_engine_lwenc1) {
        lwgpu->GetRegFldNum("LW_PMC_DEVICE_ENABLE(macros.lw_device_info_lwenc1_reset/32)", "", &data);
        mask = 0x00000000;
        mask = mask | (0x1 << macros.lw_device_info_lwenc1_reset%32);
        data = data | mask;
       if (lwgpu->SetRegFldNum("LW_PMC_DEVICE_ENABLE(macros.lw_device_info_lwenc1_reset/32)","",data)) {
      ErrPrintf("Could not be updated LW_PMC_DEVICE_ENABLE_0 to value 0x%x\n", data);
       }
    } else if (tmp_count == macros.lw_engine_lwenc2) {
        lwgpu->GetRegFldNum("LW_PMC_DEVICE_ENABLE(macros.lw_device_info_lwenc2_reset/32)", "", &data);
        mask = 0x00000000;
        mask = mask | (0x1 << macros.lw_device_info_lwenc2_reset%32);
        data = data | mask;
       if (lwgpu->SetRegFldNum("LW_PMC_DEVICE_ENABLE(macros.lw_device_info_lwenc2_reset/32)","", data)) {
      ErrPrintf("Could not be updated LW_PMC_DEVICE_ENABLE_0 to value 0x%x\n", data);
       }
    }

    powerCtrlTest_maxwell::DelayNs(100); //wait all engines to react to the reset

    powerCtrlTest_maxwell::DelayNs(50000); //wait other engines to go idle

    // Skip testing if gatedType == 0
    if (gatedType) {
      ResetELCGClkCounters();
      clk_count_1 = ReadELCGClkCounters();
      powerCtrlTest_maxwell::DelayNs(150);
      clk_count_2 = ReadELCGClkCounters();
      ResetELCGClkCounters();
      clk_count = SubtractEngineClkCounters(clk_count_1, clk_count_2);
      printEngineClkCounters(clk_count);
      checkClkGating(clk_count, Gated, allELCGGatedType, CheckComplement);  //should be gated after reset is not asserted and engine is idle
      InfoPrintf("powerCtrlTest_maxwell: testClkOnForReset, done checking this engine clocks are gated after reset is de-asserted, gateType=0x%08x\n",gatedType);
    }
  }

  //check all engine clocks are gated after eng_delay_before expires
  powerCtrlTest_maxwell::DelayNs(50000); //wait other engines to go idle
  ResetELCGClkCounters();
  clk_count_1 = ReadELCGClkCounters();
  powerCtrlTest_maxwell::DelayNs(150);
  clk_count_2 = ReadELCGClkCounters();
  ResetELCGClkCounters();
  clk_count = SubtractEngineClkCounters(clk_count_1, clk_count_2);
  printEngineClkCounters(clk_count);
  checkClkGating(clk_count, Gated, allELCGGatedType, CheckComplement);
  InfoPrintf("powerCtrlTest_maxwell: testClkOnForReset, done checking clocks are gated after reset is gone and engines are back to idle\n");

  InfoPrintf("powerCtrlTest_maxwell: testClkOnForReset, done testing each engine clks on for reset individually\n");
}

/*
 * Test function:testELCGHubmmuWakeup
 *  - Verify that gr engine enables the clock by wakeup from hubmmu
 */
void powerCtrlTest_maxwell::testELCGHubmmuWakeup(void)
{
  MaxwellClkCount fullpower_clk_count;
  MaxwellClkCount clk_count_1, clk_count_2, clk_count;
  MaxwellClkCount tmp_clkcount1, tmp_clkcount2, base_clkcount;
  bool Gated = true;
  bool NotGated = false;
  bool CheckComplement = true;
  bool DontCheckComplement = false;
  UINT32 gatedTypeToTest;

  InfoPrintf("powerCtrlTest_maxwell: Start Testing GR Engine Clock Running after Wakeup from hubmmu.\n");

  //////////////////////////////////////////////////////
  // Run all engines in FULLPOWER and verify that clocks ticks
  InfoPrintf("powerCtrlTest_maxwell: testELCGHubmmuWakeup, putting all engines in FULLPOWER mode and verifying all clks are NOT gated due to idle\n");
  if (!m_ch->WaitForIdle()) {
    ErrPrintf("powerCtrlTest_maxwell Channel WaitForIdle failed\n");
    return;
  }
  // Turn off idle clk slowdown
  DisableclkSlowdown();
  // Set engines to full power
  AllEngineMode(CG_FULLPOWER);
  // Disable BLCG
  AllEngineForceClksOn(CG_FULLPOWER);
    EnableELCGClkCounters();

  powerCtrlTest_maxwell::DelayNs(10000);
  ResetELCGClkCounters();
  clk_count_1 = ReadELCGClkCounters();
  powerCtrlTest_maxwell::DelayNs(150);
  clk_count_2 = ReadELCGClkCounters();
  ResetELCGClkCounters();
  fullpower_clk_count = SubtractEngineClkCounters(clk_count_1, clk_count_2);
  printEngineClkCounters(fullpower_clk_count);
  // Make sure clks of all engine types are NOT suspended
  checkClkGating(fullpower_clk_count, NotGated, allELCGGatedType, DontCheckComplement);
  InfoPrintf("powerCtrlTest_maxwell: testELCGHubmmuWakeup, done checking clocks are on for all engines FULLPOWER\n");

  //////////////////////////////////////////////////////
  // Put all engines in AUTOMATIC and verify clocks except disp
  InfoPrintf("powerCtrlTest_maxwell: testELCGHubmmuWakeup, putting all engines in AUTOMATIC mode and verifying all clks are gated due to idle\n");
  if (!m_ch->WaitForIdle()) {
    ErrPrintf("powerCtrlTest_maxwell: Channel WaitForIdle failed\n");
    return;
  }
  AllEngineMode(CG_AUTOMATIC);

  // Give time for engine to go idle.
  powerCtrlTest_maxwell::DelayNs(50000);     // Give time for engine to go idle.
  ResetELCGClkCounters();
  clk_count_1 = ReadELCGClkCounters();
  powerCtrlTest_maxwell::DelayNs(150);
  clk_count_2 = ReadELCGClkCounters();
  ResetELCGClkCounters();
  clk_count = SubtractEngineClkCounters(clk_count_1, clk_count_2);
  printEngineClkCounters(clk_count);
  checkClkGating(clk_count, Gated, allELCGGatedType, CheckComplement);
  InfoPrintf("powerCtrlTest_maxwell: testELCGHubmmuWakeup, done checking clocks for gr engines AUTOMATIC\n");

  gatedTypeToTest = maxwell_clock_gating.MAXWELL_CLOCK_GR_ENGINE_GATED;

  //issue wakeup from hubmmu and verify gr clock is enabled
  Platform::EscapeWrite("hubmmu_gtctl_wakeup_override", 0, 1, 0x1);

  //make sure GR is waken up
  powerCtrlTest_maxwell::DelayNs(500);     // Give time for GR engine to react to wakeup call from hubmmu
  ResetELCGClkCounters();
  clk_count_1 = ReadELCGClkCounters();
  powerCtrlTest_maxwell::DelayNs(150);
  clk_count_2 = ReadELCGClkCounters();
  ResetELCGClkCounters();
  clk_count = SubtractEngineClkCounters(clk_count_1, clk_count_2);
  printEngineClkCounters(clk_count);
  checkClkGating(clk_count, NotGated, gatedTypeToTest, DontCheckComplement);
  InfoPrintf("powerCtrlTest_maxwell: testELCGHubmmuWakeup, done checking GR clocks after wakeup from hubmmu\n");

  // Release hubmmu override.
  Platform::EscapeWrite("hubmmu_gtctl_wakeup_override", 0, 1, 0x0);
  // Give time for GR engine to go idle.
  powerCtrlTest_maxwell::DelayNs(50000);     // Give time for engine to go idle.
  ResetELCGClkCounters();
  clk_count_1 = ReadELCGClkCounters();
  powerCtrlTest_maxwell::DelayNs(150);
  clk_count_2 = ReadELCGClkCounters();
  ResetELCGClkCounters();
  clk_count = SubtractEngineClkCounters(clk_count_1, clk_count_2);
  printEngineClkCounters(clk_count);
  checkClkGating(clk_count, Gated, allELCGGatedType, CheckComplement);
  InfoPrintf("powerCtrlTest_maxwell: testELCGHubmmuWakeup, done checking GR clocks are gated after GR idle again after wakeup from hubmmu\n");

  // Set engines to full power
  AllEngineMode(CG_FULLPOWER);
  powerCtrlTest_maxwell::DelayNs(50000);
  InfoPrintf("powerCtrlTest_maxwell: testELCGHubmmuWakeup, done testing gr engine waken up by hubmmu\n");
}

/*
 * Test function: testELCGfecsInterfaceReset
 *  - Verify that fecs elcg interface can handle resets cleanly
 *  - As we cannot toggle "reset_IB_dly_", which is the reset used by the interface,
 *  - We only verify that host2fecs_reset_ don't break this interface
 */
void powerCtrlTest_maxwell::testELCGfecsInterfaceReset(void)
{
  MaxwellClkCount fullpower_clk_count;
  MaxwellClkCount clk_count_1, clk_count_2, clk_count;
  MaxwellClkCount tmp_clkcount1, tmp_clkcount2, base_clkcount;
  bool Gated = true;
  bool NotGated = false;
  bool CheckComplement = true;
  bool DontCheckComplement = false;

  UINT32 gtctl_state;
  UINT32 i;
  UINT32 my_state;
  UINT32 rdat;

  lwgpu->SetRegFldNum("LW_THERM_FECS_IDLE_FILTER","_VALUE", 0x10);
  lwgpu->GetRegFldNum("LW_THERM_FECS_IDLE_FILTER","_VALUE",&rdat);
  InfoPrintf("powerCtrlTest_maxwell: Set LW_THERM_FECS_IDLE_FILTER to short value to decrease simulation time, rdat = %x.\n", rdat);

  InfoPrintf("powerCtrlTest_maxwell: Testing fecs ELCG Interfaces Can Handle Reset Cleanly.\n");

  //////////////////////////////////////////////////////
  // Run all engines in FULLPOWER and verify that clocks ticks
  InfoPrintf("powerCtrlTest_maxwell: testELCGfecsInterfaceReset, putting all engines in FULLPOWER mode and verifying all clks are NOT gated due to idle\n");
  if (!m_ch->WaitForIdle()) {
    ErrPrintf("powerCtrlTest_maxwell Channel WaitForIdle failed\n");
    return;
  }
  // Turn off idle clk slowdown
  DisableclkSlowdown();
  // Set engines to full power
  AllEngineMode(CG_FULLPOWER);
  // Disable BLCG
  AllEngineForceClksOn(CG_FULLPOWER);
  EnableELCGClkCounters();

  powerCtrlTest_maxwell::DelayNs(8000);
  ResetELCGClkCounters();
  InfoPrintf("powerCtrlTest_maxwell: 1nd reading counters\n");
  clk_count_1 = ReadELCGClkCounters();
  powerCtrlTest_maxwell::DelayNs(150);
  InfoPrintf("powerCtrlTest_maxwell: 2nd reading counters\n");
  clk_count_2 = ReadELCGClkCounters();
  ResetELCGClkCounters();
  fullpower_clk_count = SubtractEngineClkCounters(clk_count_1, clk_count_2);
  printEngineClkCounters(fullpower_clk_count);
  // Make sure clks of all engine types are NOT suspended
  checkClkGating(fullpower_clk_count, NotGated, allELCGGatedType, DontCheckComplement);
  InfoPrintf("powerCtrlTest_maxwell: testELCGfecsInterfaceReset, done checking clocks for all engines FULLPOWER\n");

  //////////////////////////////////////////////////////
  // Put all engines in AUTOMATIC and verify clocks except disp
  InfoPrintf("powerCtrlTest_maxwell: testELCGfecsInterfaceReset, putting all engines in AUTOMATIC mode and verifying all clks are gated due to idle\n");
  if (!m_ch->WaitForIdle()) {
    ErrPrintf("powerCtrlTest_maxwell: Channel WaitForIdle failed\n");
    return;
  }
  AllEngineMode(CG_AUTOMATIC);

  // Give time for engine to go idle.
  powerCtrlTest_maxwell::DelayNs(8000);     // Give time for engine to go idle.
  ResetELCGClkCounters();
  clk_count_1 = ReadELCGClkCounters();
  powerCtrlTest_maxwell::DelayNs(150);
  clk_count_2 = ReadELCGClkCounters();
  ResetELCGClkCounters();
  clk_count = SubtractEngineClkCounters(clk_count_1, clk_count_2);
  printEngineClkCounters(clk_count);
  checkClkGating(clk_count, Gated, allELCGGatedType, CheckComplement);
  InfoPrintf("powerCtrlTest_maxwell: testELCGfecsInterfaceReset, done checking clocks for all engines AUTOMATIC\n");

  //test gtctl_fecs interface
  InfoPrintf("powerCtrlTest_maxwell: testELCGfecsInterfaceReset, Start checking gtctl_fecs interface handle reset\n");

    //There are 6 possible gtctl_state:
    //`define ST_ON_BUSY 3'b000
    //`define ST_ON_IDLE 3'b001
    //`define ST_WAIT_DREQ_ACK 3'b010
    //`define ST_OFF 3'b011
    //`define ST_WAKEUP 3'b100
    //`define ST_WAIT_EN_ACK 3'b101
    // wakeup ___________|^^^^^^^^^^^^^^^^^|_____________
    // state   3 3 3 3 3 3 4 5 0 0 0 0 0 0 1 2 3 3 3 3
    int wakeup_state_seq[]={4, 5, 0};
    for (int k=0; k<3; k++) {
    my_state = wakeup_state_seq[k];

      //We may not see all possible gtctl state after it finishes
      //issue a pri access to wakeup fecs
      if (lwgpu->SetRegFldNum("LW_PGRAPH_PRI_FE_CG","_WAKEUP_DLY_CNT", 0, 0)) {
        ErrPrintf("powerCtrlTest_maxwell: testELCGfecsInterfaceReset could not update LW_PGRAPH_PRI_FE_CG_WAKEUP_DLY_CNT to make write data 0\n");
      } else {
        InfoPrintf("powerCtrlTest_maxwell: testELCGfecsInterfaceReset write data with wakeup_dly_cnt update will be 0\n");
      }

      //make sure fecs_gtctl goes into ST_OFF state so that write fecs_gtctl_wakeup_override can wakeup fecs_gtctl FSM
      powerCtrlTest_maxwell::DelayNs(8000);

      //force fecs_gtctl_wakeup_override to wakeup fecs_gtctl
      unsigned int r_val;
      int try_count = 0;
      Platform::EscapeWrite("fecs_gtctl_wakeup_override", 0, 1, 0x1);
      Platform::EscapeRead("fecs_gtctl_wakeup_override", 0x00, 4, &r_val);
      while (r_val != 1 && try_count < 100) {
          Platform::Delay(1);
          Platform::EscapeRead("fecs_gtctl_wakeup_override", 0x00, 4, &r_val);
          try_count++;
      }
      if(r_val != 1) {
          test_status_fail = true;
          ErrPrintf("powerCtrlTest_maxwell: testELCGfecsInterfaceReset: Failed to EscapeWrite fecs_gtctl_wakeup_override. Expected 1, actually get 0x%x\n", r_val);
      }

      Platform::EscapeRead("therm_ectl_gtctl_fecs_state", 0x00, 4, &gtctl_state);
      InfoPrintf("powerCtrlTest_maxwell: testELCGfecsInterfaceReset: current gtctl_state is %d.\n", gtctl_state);
      InfoPrintf("powerCtrlTest_maxwell: testELCGfecsInterfaceReset: Waiting for gtctl_state to go to state=%d.\n", my_state);
      i = 0;
      while ( (gtctl_state != my_state) && (i<200) ) {
        powerCtrlTest_maxwell::DelayNs(10);
        i++;
        Platform::EscapeRead("therm_ectl_gtctl_fecs_state", 0, 4, &gtctl_state);
        InfoPrintf("powerCtrlTest_maxwell: testELCGfecsInterfaceReset: current gtctl_state is %d.\n", gtctl_state);
      }
      Platform::EscapeWrite("fecs_gtctl_wakeup_override", 0, 1, 0x0);
      if (gtctl_state != my_state) {
          test_status_fail = true;
        ErrPrintf("powerCtrlTest_maxwell: testELCGfecsInterfaceReset: Timed out waiting for gtctl_state to go to state=%d . Last state = %d\n", my_state, gtctl_state);
      }
      else {
          //assert host2fecs_reset_
          unsigned int r_val;
          int try_count = 0;
          Platform::EscapeWrite("host2fecs0_reset_override", 0, 1, 0x1);  //assert reset
          Platform::EscapeRead("host2fecs0_reset_override", 0, 4, &r_val);
          while (r_val != 1 && try_count < 100) {
              Platform::Delay(1);
              Platform::EscapeRead("host2fecs0_reset_override", 0, 4, &r_val);
              try_count++;
          }
          if(r_val != 1) test_status_fail = true, ErrPrintf("powerCtrlTest_maxwell: testELCGfecsInterfaceReset: Failed to EscapeWrite host2fecs0_reset_override. Expected 1, actually get 0x%x\n", r_val);
          Platform::EscapeWrite("host2fecs0_reset_override", 0, 1, 0x0);  //de-assert reset

        // Give time for engine to go idle and check all engines are gated.
        powerCtrlTest_maxwell::DelayNs(15000);     // Give time for engine to go idle.
        ResetELCGClkCounters();
        clk_count_1 = ReadELCGClkCounters();
        powerCtrlTest_maxwell::DelayNs(150);
        clk_count_2 = ReadELCGClkCounters();
        ResetELCGClkCounters();
        clk_count = SubtractEngineClkCounters(clk_count_1, clk_count_2);
        printEngineClkCounters(clk_count);
        checkClkGating(clk_count, Gated, allELCGGatedType, CheckComplement);
        InfoPrintf("powerCtrlTest_maxwell: testELCGfecsInterfaceReset: done checking gtctl_fecs interface reset at state = %d\n", my_state);
      }
    }
    int idle_state_seq[]={1, 2, 3};
    for (int k=0; k<3; k++) {
    my_state = idle_state_seq[k];

      //We may not see all possible gtctl state after it finishes
      //issue a pri access to wakeup fecs
      if (lwgpu->SetRegFldNum("LW_PGRAPH_PRI_FE_CG","_WAKEUP_DLY_CNT", 0, 0)) {
        ErrPrintf("powerCtrlTest_maxwell: testELCGfecsInterfaceReset could not update LW_PGRAPH_PRI_FE_CG_WAKEUP_DLY_CNT to make write data 0\n");
      } else {
        DebugPrintf("powerCtrlTest_maxwell: testELCGfecsInterfaceReset write data with wakeup_dly_cnt update will be 0\n");
      }

      //make sure fecs_gtctl goes into ST_OFF state so that write fecs_gtctl_wakeup_override can wakeup fecs_gtctl FSM
      powerCtrlTest_maxwell::DelayNs(8000);

      //force fecs_gtctl_wakeup_override to wakeup fecs_gtctl
      unsigned int r_val;
      int try_count = 0;
      Platform::EscapeWrite("fecs_gtctl_wakeup_override", 0, 1, 0x1);
      Platform::EscapeRead("fecs_gtctl_wakeup_override", 0, 4, &r_val);
      while (r_val != 1 && try_count < 100) {
          Platform::Delay(1);
          Platform::EscapeRead("fecs_gtctl_wakeup_override", 0, 4, &r_val);
          try_count++;
      }
      if(r_val != 1) {
          test_status_fail = true;
          ErrPrintf("powerCtrlTest_maxwell: testELCGfecsInterfaceReset: Failed to EscapeWrite fecs_gtctl_wakeup_override. Expected 1, actually get 0x%x\n", r_val);
      }
      Platform::Delay(1);
      Platform::EscapeWrite("fecs_gtctl_wakeup_override", 0, 1, 0x0);

      Platform::EscapeRead("therm_ectl_gtctl_fecs_state", 0, 4, &gtctl_state);
      InfoPrintf("powerCtrlTest_maxwell: testELCGfecsInterfaceReset: current gtctl_state is %d.\n", gtctl_state);
      InfoPrintf("powerCtrlTest_maxwell: testELCGfecsInterfaceReset: Waiting for gtctl_state to go to state=%d.\n", my_state);
      i = 0;
      while ( (gtctl_state != my_state) && (i<1000) ) {
        powerCtrlTest_maxwell::DelayNs(1);
        i++;
        Platform::EscapeRead("therm_ectl_gtctl_fecs_state", 0, 4, &gtctl_state);
        InfoPrintf("powerCtrlTest_maxwell: testELCGfecsInterfaceReset: current gtctl_state is %d.\n", gtctl_state);
      }
      if (gtctl_state != my_state) {
          test_status_fail = true;
        ErrPrintf("powerCtrlTest_maxwell: testELCGfecsInterfaceReset: Timed out waiting for gtctl_state to go to state=%d . Last state = %d\n", my_state, gtctl_state);
      }
      else {
          //assert host2fecs_reset_
          unsigned int r_val;
          int try_count = 0;
          Platform::EscapeWrite("host2fecs0_reset_override", 0, 1, 0x1);  //assert reset
          Platform::EscapeRead("host2fecs0_reset_override", 0, 4, &r_val);
          while (r_val != 1 && try_count < 100) {
              Platform::Delay(1);
              Platform::EscapeRead("host2fecs0_reset_override", 0, 4, &r_val);
              try_count++;
          }
          if(r_val != 1) test_status_fail = true, ErrPrintf("powerCtrlTest_maxwell: testELCGfecsInterfaceReset: Failed to EscapeWrite host2fecs0_reset_override. Expected 1, actually get 0x%x\n", r_val);
          Platform::EscapeWrite("host2fecs0_reset_override", 0, 1, 0x0);  //de-assert reset

        // Give time for engine to go idle and check all engines are gated.
        powerCtrlTest_maxwell::DelayNs(15000);     // Give time for engine to go idle.
        ResetELCGClkCounters();
        clk_count_1 = ReadELCGClkCounters();
        powerCtrlTest_maxwell::DelayNs(150);
        clk_count_2 = ReadELCGClkCounters();
        ResetELCGClkCounters();
        clk_count = SubtractEngineClkCounters(clk_count_1, clk_count_2);
        printEngineClkCounters(clk_count);
        checkClkGating(clk_count, Gated, allELCGGatedType, CheckComplement);
        InfoPrintf("powerCtrlTest_maxwell: testELCGfecsInterfaceReset: done checking gtctl_fecs interface reset at state = %d\n", my_state);
      }
    }
  powerCtrlTest_maxwell::DelayNs(50000);
  InfoPrintf("powerCtrlTest_maxwell: testELCGfecsInterfaceReset, Done with checking gtctl_fecs interface handle reset\n");

}

/*
 * Test function: testELCGhubmmuInterfaceReset
 *  - Verify that hubmmu elcg interface can handle resets cleanly
 */
void powerCtrlTest_maxwell::testELCGhubmmuInterfaceReset(void)
{
  MaxwellClkCount fullpower_clk_count;
  MaxwellClkCount clk_count_1, clk_count_2, clk_count;
  MaxwellClkCount tmp_clkcount1, tmp_clkcount2, base_clkcount;
  bool Gated = true;
  bool NotGated = false;
  bool CheckComplement = true;
  bool DontCheckComplement = false;

  UINT32 gtctl_state;
  UINT32 i;
  UINT32 my_state;
  UINT32 rdat;

  lwgpu->SetRegFldNum("LW_THERM_HUBMMU_IDLE_FILTER","_VALUE", 0x10);
  lwgpu->GetRegFldNum("LW_THERM_HUBMMU_IDLE_FILTER","_VALUE",&rdat);
  InfoPrintf("powerCtrlTest_maxwell: Set LW_THERM_HUBMMU_IDLE_FILTER to short value to decrease simulation time, rdat = %x.\n", rdat);

  InfoPrintf("powerCtrlTest_maxwell: Testing hubmmu ELCG Interfaces Can Handle Reset Cleanly.\n");

  // Test ALL engines
  //////////////////////////////////////////////////////
  // Run all engines in FULLPOWER and verify that clocks ticks
  InfoPrintf("powerCtrlTest_maxwell: testELCGhubmmuInterfaceReset, putting all engines in FULLPOWER mode and verifying all clks are NOT gated due to idle\n");
  if (!m_ch->WaitForIdle()) {
    ErrPrintf("powerCtrlTest_maxwell Channel WaitForIdle failed\n");
    return;
  }
  // Turn off idle clk slowdown
  DisableclkSlowdown();
  // Set engines to full power
  AllEngineMode(CG_FULLPOWER);
  // Disable BLCG
  AllEngineForceClksOn(CG_FULLPOWER);
    EnableELCGClkCounters();

  powerCtrlTest_maxwell::DelayNs(10000);
  ResetELCGClkCounters();
  InfoPrintf("powerCtrlTest_maxwell: 1nd reading counters\n");
  clk_count_1 = ReadELCGClkCounters();
  powerCtrlTest_maxwell::DelayNs(150);
  InfoPrintf("powerCtrlTest_maxwell: 2nd reading counters\n");
  clk_count_2 = ReadELCGClkCounters();
  ResetELCGClkCounters();
  fullpower_clk_count = SubtractEngineClkCounters(clk_count_1, clk_count_2);
  printEngineClkCounters(fullpower_clk_count);
  // Make sure clks of all engine types are NOT suspended
  checkClkGating(fullpower_clk_count, NotGated, allELCGGatedType, DontCheckComplement);
  InfoPrintf("powerCtrlTest_maxwell: testELCGhubmmuInterfaceReset, done checking clocks for all engines FULLPOWER\n");

  //////////////////////////////////////////////////////
  // Put all engines in AUTOMATIC and verify clocks except disp
  InfoPrintf("powerCtrlTest_maxwell: testELCGhubmmuInterfaceReset, putting all engines in AUTOMATIC mode and verifying all clks are gated due to idle\n");
  if (!m_ch->WaitForIdle()) {
    ErrPrintf("powerCtrlTest_maxwell: Channel WaitForIdle failed\n");
    return;
  }
  AllEngineMode(CG_AUTOMATIC);

  // Give time for engine to go idle.
  powerCtrlTest_maxwell::DelayNs(50000);     // Give time for engine to go idle.
  ResetELCGClkCounters();
  clk_count_1 = ReadELCGClkCounters();
  powerCtrlTest_maxwell::DelayNs(150);
  clk_count_2 = ReadELCGClkCounters();
  ResetELCGClkCounters();
  clk_count = SubtractEngineClkCounters(clk_count_1, clk_count_2);
  printEngineClkCounters(clk_count);
  checkClkGating(clk_count, Gated, allELCGGatedType, CheckComplement);
  InfoPrintf("powerCtrlTest_maxwell: testELCGhubmmuInterfaceReset, done checking clocks for all engines AUTOMATIC\n");

  //test gtctl_hubmmu interface
  InfoPrintf("powerCtrlTest_maxwell: testELCGhubmmuInterfaceReset, Start checking gtctl_hubmmu interface handle reset\n");
    //There are 6 possible gtctl_state:
    //`define ST_ON_BUSY 3'b000
    //`define ST_ON_IDLE 3'b001
    //`define ST_WAIT_DREQ_ACK 3'b010
    //`define ST_OFF 3'b011
    //`define ST_WAKEUP 3'b100
    //`define ST_WAIT_EN_ACK 3'b101
    // wakeup ___________|^^^^^^^^^^^^^^^^^|_____________
    // state   3 3 3 3 3 3 4 5 0 0 0 0 0 0 1 2 3 3 3 3
    int wakeup_state_seq[]={4, 5, 0};
    for (int k=0; k<3; k++) {
    my_state = wakeup_state_seq[k];
      //We may not see all possible gtctl state after it finishes
      //do a hubmmu ilwalidate to wakeup hubmmu_gtctl
      if (lwgpu->SetRegFldDef("LW_PFB_PRI_MMU_ILWALIDATE","_ALL_VA","_TRUE")) {
        ErrPrintf("LW_PFB_PRI_MMU_ILWALIDATE could not be updated for the value _ALL_VA_TRUE\n");
      }
      if (lwgpu->SetRegFldDef("LW_PFB_PRI_MMU_ILWALIDATE","_TRIGGER","_TRUE")) {
        ErrPrintf("LW_PFB_PRI_MMU_ILWALIDATE could not be updated for the value _TRIGGER_TRUE\n");
      }

      //make sure hubmmu_gtctl goes into ST_OFF state so that write hubmmu_gtctl_wakeup_override can wakeup hubmmu_gtctl FSM
      powerCtrlTest_maxwell::DelayNs(8000);

      //force hubmmu_gtctl_wakeup_override to wakeup hubmmu_gtctl
      unsigned int r_val;
      int try_count=0;
      Platform::EscapeWrite("hubmmu_gtctl_wakeup_override", 0, 1, 0x1);
      Platform::EscapeRead("hubmmu_gtctl_wakeup_override", 0, 4, &r_val);
      while (r_val != 1 && try_count < 100) {
          Platform::Delay(1);
          Platform::EscapeRead("hubmmu_gtctl_wakeup_override", 0, 4, &r_val);
          try_count++;
      }
      if(r_val != 1) ErrPrintf("powerCtrlTest_maxwell: testELCGhubmmuInterfaceReset, fail to assert hubmmu_gtctl_wakeup_override. Expect 1, actually get 0x%x\n", r_val);

      Platform::EscapeRead("therm_ectl_gtctl_hubmmu_state", 0, 4, &gtctl_state);
      InfoPrintf("powerCtrlTest_maxwell: testELCGhubmmuInterfaceReset: current gtctl_state is %d.\n", gtctl_state);
      InfoPrintf("powerCtrlTest_maxwell: testELCGhubmmuInterfaceReset: Waiting for gtctl_state to go to state=%d.\n", my_state);
      i = 0;
      while ( (gtctl_state != my_state) && (i<1000) ) {
        powerCtrlTest_maxwell::DelayNs(10);
        i++;
        Platform::EscapeRead("therm_ectl_gtctl_hubmmu_state", 0, 4, &gtctl_state);
        InfoPrintf("powerCtrlTest_maxwell: testELCGhubmmuInterfaceReset: current gtctl_state is %d.\n", gtctl_state);
      }
      Platform::EscapeWrite("hubmmu_gtctl_wakeup_override", 0, 1, 0x0);
      if (gtctl_state != my_state) {
          test_status_fail = true;
        ErrPrintf("powerCtrlTest_maxwell: testELCGhubmmuInterfaceReset: Timed out waiting for gtctl_state to go to state=%d . Last state = %d\n", my_state, gtctl_state);
      }
      else {
        //issue host2hub_reset
          unsigned int r_val;
          int try_count = 0;
          Platform::EscapeWrite("host2hub_reset_override", 0, 1, 0x1);
          Platform::EscapeRead("host2hub_reset_override", 0, 4, &r_val);
          while (r_val != 1 && try_count < 100) {
              Platform::Delay(1);
              Platform::EscapeRead("host2hub_reset_override", 0, 4, &r_val);
              try_count++;
          }
          if(r_val != 1) ErrPrintf("powerCtrlTest_maxwell: testELCGhubmmuInterfaceReset, fail to assert host2hub_reset_override\n");
          Platform::EscapeWrite("host2hub_reset_override", 0, 1, 0x0);

        // Give time for engine to go idle and check all engines are gated.
        powerCtrlTest_maxwell::DelayNs(50000);     // Give time for engine to go idle.
        ResetELCGClkCounters();
        clk_count_1 = ReadELCGClkCounters();
        powerCtrlTest_maxwell::DelayNs(150);
        clk_count_2 = ReadELCGClkCounters();
        ResetELCGClkCounters();
        clk_count = SubtractEngineClkCounters(clk_count_1, clk_count_2);
        printEngineClkCounters(clk_count);
        checkClkGating(clk_count, Gated, allELCGGatedType, CheckComplement);
        InfoPrintf("powerCtrlTest_maxwell: testELCGhubmmuInterfaceReset: done checking gtctl_hubmmu interface reset at state = %d\n", my_state);
      }
    }
    int idle_state_seq[]={1, 2, 3};
    for (int k=0; k<3; k++) {
    my_state = idle_state_seq[k];
      //We may not see all possible gtctl state after it finishes
      //do a hubmmu ilwalidate to wakeup hubmmu_gtctl
      if (lwgpu->SetRegFldDef("LW_PFB_PRI_MMU_ILWALIDATE","_ALL_VA","_TRUE")) {
        ErrPrintf("LW_PFB_PRI_MMU_ILWALIDATE could not be updated for the value _ALL_VA_TRUE\n");
      }
      if (lwgpu->SetRegFldDef("LW_PFB_PRI_MMU_ILWALIDATE","_TRIGGER","_TRUE")) {
        ErrPrintf("LW_PFB_PRI_MMU_ILWALIDATE could not be updated for the value _TRIGGER_TRUE\n");
      }

      //make sure hubmmu_gtctl goes into ST_OFF state so that write hubmmu_gtctl_wakeup_override can wakeup hubmmu_gtctl FSM
      powerCtrlTest_maxwell::DelayNs(8000);

      //force hubmmu_gtctl_wakeup_override to wakeup hubmmu_gtctl
      unsigned int r_val;
      int try_count=0;
      Platform::EscapeWrite("hubmmu_gtctl_wakeup_override", 0, 1, 0x1);
      Platform::EscapeRead("hubmmu_gtctl_wakeup_override", 0, 4, &r_val);
      while (r_val != 1 && try_count < 100) {
          Platform::Delay(1);
          Platform::EscapeRead("hubmmu_gtctl_wakeup_override", 0, 4, &r_val);
          try_count++;
      }
      if(r_val != 1) ErrPrintf("powerCtrlTest_maxwell: testELCGhubmmuInterfaceReset, fail to assert hubmmu_gtctl_wakeup_override\n");
      Platform::Delay(1);
      Platform::EscapeWrite("hubmmu_gtctl_wakeup_override", 0, 1, 0x0);

      Platform::EscapeRead("therm_ectl_gtctl_hubmmu_state", 0, 4, &gtctl_state);
      InfoPrintf("powerCtrlTest_maxwell: testELCGhubmmuInterfaceReset: current gtctl_state is %d.\n", gtctl_state);
      InfoPrintf("powerCtrlTest_maxwell: testELCGhubmmuInterfaceReset: Waiting for gtctl_state to go to state=%d.\n", my_state);
      i = 0;
      while ( (gtctl_state != my_state) && (i<1000) ) {
        powerCtrlTest_maxwell::DelayNs(10);
        i++;
        Platform::EscapeRead("therm_ectl_gtctl_hubmmu_state", 0, 4, &gtctl_state);
        InfoPrintf("powerCtrlTest_maxwell: testELCGhubmmuInterfaceReset: current gtctl_state is %d.\n", gtctl_state);
      }
      if (gtctl_state != my_state) {
          test_status_fail = true;
        ErrPrintf("powerCtrlTest_maxwell: testELCGhubmmuInterfaceReset: Timed out waiting for gtctl_state to go to state=%d . Last state = %d\n", my_state, gtctl_state);
      }
      else {
        //issue host2hub_reset
          unsigned int r_val;
          int try_count = 0;
          Platform::EscapeWrite("host2hub_reset_override", 0, 1, 0x1);
          Platform::EscapeRead("host2hub_reset_override", 0, 4, &r_val);
          while (r_val != 1 && try_count < 100) {
              Platform::Delay(1);
              Platform::EscapeRead("host2hub_reset_override", 0, 4, &r_val);
              try_count++;
          }
          if(r_val != 1) ErrPrintf("powerCtrlTest_maxwell: testELCGhubmmuInterfaceReset, fail to assert host2hub_reset_override\n");
          Platform::EscapeWrite("host2hub_reset_override", 0, 1, 0x0);

        // Give time for engine to go idle and check all engines are gated.
        powerCtrlTest_maxwell::DelayNs(50000);     // Give time for engine to go idle.
        ResetELCGClkCounters();
        clk_count_1 = ReadELCGClkCounters();
        powerCtrlTest_maxwell::DelayNs(150);
        clk_count_2 = ReadELCGClkCounters();
        ResetELCGClkCounters();
        clk_count = SubtractEngineClkCounters(clk_count_1, clk_count_2);
        printEngineClkCounters(clk_count);
        checkClkGating(clk_count, Gated, allELCGGatedType, CheckComplement);
        InfoPrintf("powerCtrlTest_maxwell: testELCGhubmmuInterfaceReset: done checking gtctl_hubmmu interface reset at state = %d\n", my_state);
      }
    }

  // Put Everything back to FULLPOWER.
  AllEngineMode(CG_FULLPOWER);
  powerCtrlTest_maxwell::DelayNs(100000);// To allow all credits being released to sender after end of sim.

  InfoPrintf("powerCtrlTest_maxwell: testELCGhubmmuInterfaceReset, Done with checking gtctl_hubmmu interface handle reset\n");

}

/*
 * Test function: testELCGgpccsInterfaceReset
 *  - Verify that gpccs elcg interface can handle resets cleanly
 */
void powerCtrlTest_maxwell::testELCGgpccsInterfaceReset(void)
{
  MaxwellClkCount fullpower_clk_count;
  MaxwellClkCount clk_count_1, clk_count_2, clk_count;
  MaxwellClkCount tmp_clkcount1, tmp_clkcount2, base_clkcount;
  bool Gated = true;
  bool NotGated = false;
  bool CheckComplement = true;
  bool DontCheckComplement = false;

  UINT32 gtctl_state;
  UINT32 i, k;
  UINT32 my_state;
  UINT32 fs_disable;
  const char *my_regname;
  const char *my_signame;
  const char *my_fs_sig;

  InfoPrintf("powerCtrlTest_maxwell: Testing gpccs ELCG Interfaces Can Handle Reset Cleanly.\n");

  // Test ALL engines
  //////////////////////////////////////////////////////
  // Run all engines in FULLPOWER and verify that clocks ticks
  InfoPrintf("powerCtrlTest_maxwell: testELCGgpccsInterfaceReset, putting all engines in FULLPOWER mode and verifying all clks are NOT gated due to idle\n");
  if (!m_ch->WaitForIdle()) {
    ErrPrintf("powerCtrlTest_maxwell Channel WaitForIdle failed\n");
    return;
  }
  // Turn off idle clk slowdown
  DisableclkSlowdown();
  // Set engines to full power
  AllEngineMode(CG_FULLPOWER);
  // Disable BLCG
  AllEngineForceClksOn(CG_FULLPOWER);
  EnableELCGClkCounters();

  powerCtrlTest_maxwell::DelayNs(8000);
  ResetELCGClkCounters();
  InfoPrintf("powerCtrlTest_maxwell: 1nd reading counters\n");
  clk_count_1 = ReadELCGClkCounters();
  powerCtrlTest_maxwell::DelayNs(150);
  InfoPrintf("powerCtrlTest_maxwell: 2nd reading counters\n");
  clk_count_2 = ReadELCGClkCounters();
  ResetELCGClkCounters();
  fullpower_clk_count = SubtractEngineClkCounters(clk_count_1, clk_count_2);
  printEngineClkCounters(fullpower_clk_count);
  // Make sure clks of all engine types are NOT suspended
  checkClkGating(fullpower_clk_count, NotGated, allELCGGatedType, DontCheckComplement);
  InfoPrintf("powerCtrlTest_maxwell: testELCGgpccsInterfaceReset, done checking clocks for all engines FULLPOWER\n");

  //////////////////////////////////////////////////////
  // Put all engines in AUTOMATIC and verify clocks except disp
  InfoPrintf("powerCtrlTest_maxwell: testELCGgpccsInterfaceReset, putting all engines in AUTOMATIC mode and verifying all clks are gated due to idle\n");
  if (!m_ch->WaitForIdle()) {
    ErrPrintf("powerCtrlTest_maxwell: Channel WaitForIdle failed\n");
    return;
  }
  AllEngineMode(CG_AUTOMATIC);

  // Give time for engine to go idle.
  powerCtrlTest_maxwell::DelayNs(8000);     // Give time for engine to go idle.
  ResetELCGClkCounters();
  clk_count_1 = ReadELCGClkCounters();
  powerCtrlTest_maxwell::DelayNs(150);
  clk_count_2 = ReadELCGClkCounters();
  ResetELCGClkCounters();
  clk_count = SubtractEngineClkCounters(clk_count_1, clk_count_2);
  printEngineClkCounters(clk_count);
  checkClkGating(clk_count, Gated, allELCGGatedType, CheckComplement);
  InfoPrintf("powerCtrlTest_maxwell: testELCGgpccsInterfaceReset, done checking clocks for all engines AUTOMATIC\n");

  //test gtctl_gpccs interface
    for (k=0; k<n_gpc; k++) {
      switch (k) {
        case 0:
          my_regname = "LW_PGRAPH_PRI_GPC0_PROP_CG";
          my_signame = "gpc0_gpcpwr_gtctl_sever_state";
          my_fs_sig  = "fs2gpc0_gpc_disable";
          break;
        case 1:
          my_regname = "LW_PGRAPH_PRI_GPC1_PROP_CG";
          my_signame = "gpc1_gpcpwr_gtctl_sever_state";
          my_fs_sig  = "fs2gpc1_gpc_disable";
          break;
        case 2:
          my_regname = "LW_PGRAPH_PRI_GPC2_PROP_CG";
          my_signame = "gpc2_gpcpwr_gtctl_sever_state";
          my_fs_sig  = "fs2gpc2_gpc_disable";
          break;
        case 3:
          my_regname = "LW_PGRAPH_PRI_GPC3_PROP_CG";
          my_signame = "gpc3_gpcpwr_gtctl_sever_state";
          my_fs_sig  = "fs2gpc3_gpc_disable";
          break;
        case 4:
          my_regname = "LW_PGRAPH_PRI_GPC4_PROP_CG";
          my_signame = "gpc4_gpcpwr_gtctl_sever_state";
          my_fs_sig  = "fs2gpc4_gpc_disable";
          break;
        case 5:
          my_regname = "LW_PGRAPH_PRI_GPC5_PROP_CG";
          my_signame = "gpc5_gpcpwr_gtctl_sever_state";
          my_fs_sig  = "fs2gpc5_gpc_disable";
          break;
        case 6:
          my_regname = "LW_PGRAPH_PRI_GPC6_PROP_CG";
          my_signame = "gpc6_gpcpwr_gtctl_sever_state";
          my_fs_sig  = "fs2gpc6_gpc_disable";
          break;
        case 7:
          my_regname = "LW_PGRAPH_PRI_GPC7_PROP_CG";
          my_signame = "gpc7_gpcpwr_gtctl_sever_state";
          my_fs_sig  = "fs2gpc7_gpc_disable";
          break;
        default:
          my_regname = "LW_PGRAPH_PRI_GPC0_PROP_CG";
          my_signame = "gpc0_gpcpwr_gtctl_sever_state";
          my_fs_sig  = "fs2gpc0_gpc_disable";
          break;
      }

      // Check if the current GPC was floorswept
      Platform::EscapeRead(my_fs_sig, 0, 1, &fs_disable);
      // Skip if GPC was floorswept
      if(fs_disable!=0){
          continue;
      }

      InfoPrintf("powerCtrlTest_maxwell: testELCGgpccsInterfaceReset, Start checking gtctl_gpc%d interface handle reset%d\n", k);

      //There are 6 possible gtctl_state: ON_BUSY(0), ON_IDLE(1), WAKEUP(2), OFF(3), ON_IDLE_WAIT_RX(4), ON_IDLE_WAIT2GATE(5)
      //With the latest change that move delay_before into eng_gate_ctrl, ON_IDLE_WAIT2GATE will not be used
      for (my_state = 0; my_state < 5; my_state++) {
        //issue a gpc pri access to wakeup gpc_gtctl
        if (lwgpu->SetRegFldNum(my_regname,"_WAKEUP_DLY_CNT", 0, 0)) {
          ErrPrintf("powerCtrlTest_maxwell: testELCGgpccsInterfaceReset could not update LW_PGRAPH_PRI_GPC%d_PROP_CG_WAKEUP_DLY_CNT to 0\n", k);
        } else {
          DebugPrintf("powerCtrlTest_maxwell: testELCGgpccsInterfaceReset write LW_PGRAPH_PRI_GPC%d_PROP_CG_WAKEUP_DLY_CNT to 0\n",k);
        }

        Platform::EscapeRead(my_signame, 0, 4, &gtctl_state);
        InfoPrintf("powerCtrlTest_maxwell: testELCGgpccsInterfaceReset: current gtctl_state is %d.\n", gtctl_state);
        InfoPrintf("powerCtrlTest_maxwell: testELCGgpccsInterfaceReset: Waiting for gtctl_state to go to state=%d.\n", my_state);
        i = 0;
        while ( (gtctl_state != my_state) && (i<200) ) {
          powerCtrlTest_maxwell::DelayNs(10);
          i++;
          Platform::EscapeRead(my_signame, 0, 4, &gtctl_state);
          InfoPrintf("powerCtrlTest_maxwell: testELCGgpccsInterfaceReset: current gtctl_state is %d.\n", gtctl_state);
        }
        if (gtctl_state != my_state) {
            test_status_fail = true;
            ErrPrintf("powerCtrlTest_maxwell: testELCGgpccsInterfaceReset: Timed out waiting for gtctl_state to go to state=%d . Last state = %d\n", my_state, gtctl_state);
        }
        else {
          //assert fecs2gpccs_engine_reset_
          Platform::EscapeWrite("fecs2gpccs_engine_reset_override", 0, 1, 0x1);  //assert reset
          powerCtrlTest_maxwell::DelayNs(100);
          Platform::EscapeWrite("fecs2gpccs_engine_reset_override", 0, 1, 0x0);  //de-assert reset

          // Give time for engine to go idle and check all engines are gated.
          powerCtrlTest_maxwell::DelayNs(8000);     // Give time for engine to go idle.
          ResetELCGClkCounters();
          clk_count_1 = ReadELCGClkCounters();
          powerCtrlTest_maxwell::DelayNs(150);
          clk_count_2 = ReadELCGClkCounters();
          ResetELCGClkCounters();
          clk_count = SubtractEngineClkCounters(clk_count_1, clk_count_2);
          printEngineClkCounters(clk_count);
          checkClkGating(clk_count, Gated, allELCGGatedType, CheckComplement);
          InfoPrintf("powerCtrlTest_maxwell: testELCGgpccsInterfaceReset: done checking gtctl_gpc%d interface reset at state = %d\n", k, my_state);
        }
      }
    }
  powerCtrlTest_maxwell::DelayNs(50000);
  InfoPrintf("powerCtrlTest_maxwell: testELCGgpccsInterfaceReset, Done with checking gtctl_gpccs interface handle reset\n");

}

/*
 * Test function: testELCGbecsInterfaceReset
 *  - Verify that becs elcg interface can handle resets cleanly
 */
void powerCtrlTest_maxwell::testELCGbecsInterfaceReset(void)
{
  MaxwellClkCount fullpower_clk_count;
  MaxwellClkCount clk_count_1, clk_count_2, clk_count;
  MaxwellClkCount tmp_clkcount1, tmp_clkcount2, base_clkcount;
  bool Gated = true;
  bool NotGated = false;
  bool CheckComplement = true;
  bool DontCheckComplement = false;

  UINT32 gtctl_state;
  UINT32 i, k;
  UINT32 my_state;
  UINT32 fs_disable;
  const char *wakeup_signal;
  const char *my_signame;
  const char *my_fs_sig;

  InfoPrintf("powerCtrlTest_maxwell: Testing becs ELCG Interfaces Can Handle Reset Cleanly.\n");

  // Test ALL engines
  //////////////////////////////////////////////////////
  // Run all engines in FULLPOWER and verify that clocks ticks
  InfoPrintf("powerCtrlTest_maxwell: testELCGbecsInterfaceReset, putting all engines in FULLPOWER mode and verifying all clks are NOT gated due to idle\n");
  if (!m_ch->WaitForIdle()) {
    ErrPrintf("powerCtrlTest_maxwell Channel WaitForIdle failed\n");
    return;
  }
  // Turn off idle clk slowdown
  DisableclkSlowdown();
  // Set engines to full power
  AllEngineMode(CG_FULLPOWER);
  // Disable BLCG
  AllEngineForceClksOn(CG_FULLPOWER);
  EnableELCGClkCounters();

  powerCtrlTest_maxwell::DelayNs(8000);
  ResetELCGClkCounters();
  InfoPrintf("powerCtrlTest_maxwell: 1nd reading counters\n");
  clk_count_1 = ReadELCGClkCounters();
  powerCtrlTest_maxwell::DelayNs(150);
  InfoPrintf("powerCtrlTest_maxwell: 2nd reading counters\n");
  clk_count_2 = ReadELCGClkCounters();
  ResetELCGClkCounters();
  fullpower_clk_count = SubtractEngineClkCounters(clk_count_1, clk_count_2);
  printEngineClkCounters(fullpower_clk_count);
  // Make sure clks of all engine types are NOT suspended
  checkClkGating(fullpower_clk_count, NotGated, allELCGGatedType, DontCheckComplement);
  InfoPrintf("powerCtrlTest_maxwell: testELCGbecsInterfaceReset, done checking clocks for all engines FULLPOWER\n");

  //////////////////////////////////////////////////////
  // Put all engines in AUTOMATIC and verify clocks except disp
  InfoPrintf("powerCtrlTest_maxwell: testELCGbecsInterfaceReset, putting all engines in AUTOMATIC mode and verifying all clks are gated due to idle\n");
  if (!m_ch->WaitForIdle()) {
    ErrPrintf("powerCtrlTest_maxwell: Channel WaitForIdle failed\n");
    return;
  }
  AllEngineMode(CG_AUTOMATIC);

  // Give time for engine to go idle.
  powerCtrlTest_maxwell::DelayNs(8000);     // Give time for engine to go idle.
  ResetELCGClkCounters();
  clk_count_1 = ReadELCGClkCounters();
  powerCtrlTest_maxwell::DelayNs(150);
  clk_count_2 = ReadELCGClkCounters();
  ResetELCGClkCounters();
  clk_count = SubtractEngineClkCounters(clk_count_1, clk_count_2);
  printEngineClkCounters(clk_count);
  checkClkGating(clk_count, Gated, allELCGGatedType, CheckComplement);
  InfoPrintf("powerCtrlTest_maxwell: testELCGbecsInterfaceReset, done checking clocks for all engines AUTOMATIC\n");

  //test gtctl_becs interface
  for (k=0; k<n_fbp; k++) {
    switch (k) {
        case 0:
            wakeup_signal = "fbp0_fbppwr_gtctl_wakeup_override";
            my_signame    = "fbp0_fbppwr_gtctl_sever_state";
            my_fs_sig     = "fs2fbp0_fbp_disable";
            break;
        case 1:
            wakeup_signal = "fbp1_fbppwr_gtctl_wakeup_override";
            my_signame    = "fbp1_fbppwr_gtctl_sever_state";
            my_fs_sig     = "fs2fbp1_fbp_disable";
            break;
        case 2:
            wakeup_signal = "fbp2_fbppwr_gtctl_wakeup_override";
            my_signame    = "fbp2_fbppwr_gtctl_sever_state";
            my_fs_sig     = "fs2fbp2_fbp_disable";
            break;
        case 3:
            wakeup_signal = "fbp3_fbppwr_gtctl_wakeup_override";
            my_signame    = "fbp3_fbppwr_gtctl_sever_state";
            my_fs_sig     = "fs2fbp3_fbp_disable";
            break;
        case 4:
            wakeup_signal = "fbp4_fbppwr_gtctl_wakeup_override";
            my_signame    = "fbp4_fbppwr_gtctl_sever_state";
            my_fs_sig     = "fs2fbp4_fbp_disable";
            break;
        case 5:
            wakeup_signal = "fbp5_fbppwr_gtctl_wakeup_override";
            my_signame    = "fbp5_fbppwr_gtctl_sever_state";
            my_fs_sig     = "fs2fbp5_fbp_disable";
            break;
        default:
            wakeup_signal = "fbp0_fbppwr_gtctl_wakeup_override";
            my_signame    = "fbp0_fbppwr_gtctl_sever_state";
            my_fs_sig     = "fs2fbp0_fbp_disable";
            break;
    }

        // Check if the current FBP was floorswept
        Platform::EscapeRead(my_fs_sig, 0, 1, &fs_disable);
        // Skip if FBP was floorswept
        if(fs_disable!=0) {
            continue;
        }

        InfoPrintf("powerCtrlTest_maxwell: testELCGbecsInterfaceReset, Start checking gtctl_becs%d interface handle reset\n",k);

        //There are 6 possible gtctl_state:
        //`define ST_ON_BUSY 3'b000
        //`define ST_ON_IDLE 3'b001
        //`define ST_WAIT_DREQ_ACK 3'b010
        //`define ST_OFF 3'b011
        //`define ST_WAKEUP 3'b100
        //`define ST_WAIT_EN_ACK 3'b101
        // wakeup ___________|^^^^^^^^^^^^^^^^^|_____________
        // state   3 3 3 3 3 3 4 5 0 0 0 0 0 0 1 2 3 3 3 3
        int wakeup_state_seq[]={4, 5, 0};
        for (int l=0; l<3; l++) {
        my_state = wakeup_state_seq[l];

        //force fbp0_fbppwr_gtctl_wakeup_override to wakeup becs_gtctl
        Platform::EscapeWrite(wakeup_signal, 0, 1, 0x1);

        Platform::EscapeRead(my_signame, 0, 4, &gtctl_state);
        InfoPrintf("powerCtrlTest_maxwell: testELCGbecsInterfaceReset: current gtctl_state is %d.\n", gtctl_state);
        InfoPrintf("powerCtrlTest_maxwell: testELCGbecsInterfaceReset: Waiting for gtctl_state to go to state=%d.\n", my_state);
        i = 0;
        while ( (gtctl_state != my_state) && (i<200) ) {
          powerCtrlTest_maxwell::DelayNs(10);
          i++;
          Platform::EscapeRead(my_signame, 0, 4, &gtctl_state);
          InfoPrintf("powerCtrlTest_maxwell: testELCGbecsInterfaceReset: current gtctl_state is %d.\n", gtctl_state);
        }
        Platform::EscapeWrite(wakeup_signal, 0, 1, 0x0);
        if (gtctl_state != my_state) {
        test_status_fail = true;
          ErrPrintf("powerCtrlTest_maxwell: testELCGbecsInterfaceReset: Timed out waiting for gtctl_state to go to state=%d . Last state = %d\n", my_state, gtctl_state);
        }
        else {

          //assert fecs2becs_engine_reset_
          Platform::EscapeWrite("fecs2becs_engine_reset_override", 0, 1, 0x1);  //assert reset
          powerCtrlTest_maxwell::DelayNs(150);
          Platform::EscapeWrite("fecs2becs_engine_reset_override", 0, 1, 0x0);  //de-assert reset

          // Give time for engine to go idle and check all engines are gated.
          powerCtrlTest_maxwell::DelayNs(8000);     // Give time for engine to go idle.
          ResetELCGClkCounters();
          clk_count_1 = ReadELCGClkCounters();
          powerCtrlTest_maxwell::DelayNs(150);
          clk_count_2 = ReadELCGClkCounters();
          ResetELCGClkCounters();
          clk_count = SubtractEngineClkCounters(clk_count_1, clk_count_2);
          printEngineClkCounters(clk_count);
          checkClkGating(clk_count, Gated, allELCGGatedType, CheckComplement);
          InfoPrintf("powerCtrlTest_maxwell: testELCGbecsInterfaceReset: done checking gtctl_becs interface reset at state = %d\n", my_state);
        }
      }
        int idle_state_seq[]={1, 2, 3};
        for (int l=0; l<3; l++) {
        my_state = idle_state_seq[l];

        //force fbp0_fbppwr_gtctl_wakeup_override to wakeup becs_gtctl
        Platform::EscapeWrite(wakeup_signal, 0, 1, 0x1);
        Platform::Delay(1);
        Platform::EscapeWrite(wakeup_signal, 0, 1, 0x0);

        Platform::EscapeRead(my_signame, 0, 4, &gtctl_state);
        InfoPrintf("powerCtrlTest_maxwell: testELCGbecsInterfaceReset: current gtctl_state is %d.\n", gtctl_state);
        InfoPrintf("powerCtrlTest_maxwell: testELCGbecsInterfaceReset: Waiting for gtctl_state to go to state=%d.\n", my_state);
        i = 0;
        while ( (gtctl_state != my_state) && (i<200) ) {
          powerCtrlTest_maxwell::DelayNs(10);
          i++;
          Platform::EscapeRead(my_signame, 0, 4, &gtctl_state);
          InfoPrintf("powerCtrlTest_maxwell: testELCGbecsInterfaceReset: current gtctl_state is %d.\n", gtctl_state);
    }
        if (gtctl_state != my_state) {
        test_status_fail = true;
          ErrPrintf("powerCtrlTest_maxwell: testELCGbecsInterfaceReset: Timed out waiting for gtctl_state to go to state=%d . Last state = %d\n", my_state, gtctl_state);
        }
        else {

          //assert fecs2becs_engine_reset_
          Platform::EscapeWrite("fecs2becs_engine_reset_override", 0, 1, 0x1);  //assert reset
          powerCtrlTest_maxwell::DelayNs(150);
          Platform::EscapeWrite("fecs2becs_engine_reset_override", 0, 1, 0x0);  //de-assert reset

          // Give time for engine to go idle and check all engines are gated.
          powerCtrlTest_maxwell::DelayNs(8000);     // Give time for engine to go idle.
          ResetELCGClkCounters();
          clk_count_1 = ReadELCGClkCounters();
          powerCtrlTest_maxwell::DelayNs(150);
          clk_count_2 = ReadELCGClkCounters();
          ResetELCGClkCounters();
          clk_count = SubtractEngineClkCounters(clk_count_1, clk_count_2);
          printEngineClkCounters(clk_count);
          checkClkGating(clk_count, Gated, allELCGGatedType, CheckComplement);
          InfoPrintf("powerCtrlTest_maxwell: testELCGbecsInterfaceReset: done checking gtctl_becs interface reset at state = %d\n", my_state);
        }
      }
    }
  powerCtrlTest_maxwell::DelayNs(50000);
  InfoPrintf("powerCtrlTest_maxwell: testELCGbecsInterfaceReset, Done with checking gtctl_becs interface handle reset\n");

}

/*
 * Test function: testPowerModeInterface
 *  - Verify that gr2pwr_blcg_disable interface can handle resets cleanly
 */
void powerCtrlTest_maxwell::testPowerModeInterface(void)
{
//  UINT32 i, j;
//  UINT32 req_status, force_clks_on_status;
  UINT32 i, j, mask, data, smc_index;
  UINT32 req_status, force_clks_on_status, force_full_power_status, force_slow_status;
  //test gr2pwr_blcg_disable interface
  InfoPrintf("powerCtrlTest_maxwell: testPowerModeInterface, Start checking gr2pwr_blcg_disable interface handle reset\n");

  //modes switch by loop 10 times:
  //            PWR_BLCG_DISABLE_AUTO  ->  PWR_BLCG_DISABLE_TRUE
  //                      ^                          v
  //                    \ <-           <-           <- /
  //
  //It's equal            0            ->            1
  //                      ^                          v
  //                    \ <-           <-           <- /

  InfoPrintf("powerCtrlTest_maxwell: m_arch is 0x%x.\n", m_arch);
 if( (m_arch != 3330) && (m_arch != 3086) ) {
  //repeat the test for 10 times
  for (j=0; j< 10; j++) {
    //request LW_PGRAPH_PRI_FECS_PWR_BLCG_DISABLE to be _TRUE
    InfoPrintf("powerCtrlTest_maxwell: testPowerModeInterface, start checking PWR_BLCG_DISABLE request from FECS, round=%d\n", j);

    lwgpu->SetRegFldNum("LW_PGRAPH_PRI_FECS_PWR_BLCG", "_DISABLE", 1);
    lwgpu->SetRegFldNum("LW_PGRAPH_PRI_FECS_PWR_BLCG", "_REQ", 1);

    i = 0;
    req_status = 1;
    do {
      lwgpu->GetRegFldNum("LW_PGRAPH_PRI_FECS_PWR_BLCG","_REQ",&req_status); //check mode request status
      InfoPrintf("powerCtrlTest_maxwell: testPowerModeInterface: current mode request status is %d.\n", req_status);
      powerCtrlTest_maxwell::DelayNs(10);
      i++;
    } while ( (req_status != 0) && (i<1000)  );
    if (req_status != 0) {
      ErrPrintf("powerCtrlTest_maxwell: testPowerModeInterface: Timed out waiting for PWR_BLCG_DISABLE request to be acked!\n");
      test_status_fail = true;
    }

    //check whether force_clks_on is asserted after FORCE_ON request from FE
    force_clks_on_status = 0;
    powerCtrlTest_maxwell::DelayNs(10); //wait until the therm to react to the mode_req
    Platform::EscapeRead("pwr2all_gr_force_clks_on", 0, 1, &force_clks_on_status);
    if (force_clks_on_status!=1) {
      ErrPrintf("powerCtrlTest_maxwell: testPowerModeInterface: pwr2all_gr_force_clks_on should be 1, not 0, after PWR_BLCG_DISABLE request from FECS!\n");
      test_status_fail = true;
    }
    else {
      InfoPrintf("powerCtrlTest_maxwell: testPowerModeInterface: Verifed pwr2all_gr_force_clks_on is 1 after PWR_BLCG_DISABLE request from FECS !\n");
    }

    // fe2all_engine_reset_ interface to PWR was removed in gh100
    powerCtrlTest_maxwell::DelayNs(100);

    // Set BLCG to AUTO
    lwgpu->SetRegFldNum("LW_PGRAPH_PRI_FECS_PWR_BLCG", "_DISABLE", 0);
    lwgpu->SetRegFldNum("LW_PGRAPH_PRI_FECS_PWR_BLCG", "_REQ", 1);

    i = 0;
    req_status = 1;
    do {
      lwgpu->GetRegFldNum("LW_PGRAPH_PRI_FECS_PWR_BLCG","_REQ",&req_status); //check mode request status
      InfoPrintf("powerCtrlTest_maxwell: testPowerModeInterface: current mode request status is %d.\n", req_status);
      powerCtrlTest_maxwell::DelayNs(10);
      i++;
    } while ( (req_status != 0) && (i<1000)  );
    if (req_status != 0) {
      ErrPrintf("powerCtrlTest_maxwell: testPowerModeInterface: Timed out waiting for PWR_BLCG_DISABLE request to be acked!\n");
      test_status_fail = true;
    }

    //check whether force_clks_on are de-asserted 
    force_clks_on_status = 1;
    powerCtrlTest_maxwell::DelayNs(10); //wait until the therm to react to the mode_req
    Platform::EscapeRead("pwr2all_gr_force_clks_on", 0, 1, &force_clks_on_status);
    if (force_clks_on_status!=0) {
      ErrPrintf("powerCtrlTest_maxwell: testPowerModeInterface: pwr2all_gr_force_clks_on should be 0, not 1, after PWR_BLCG_DISABLE == AUTO!\n");
      test_status_fail = true;
    }
    else {
      InfoPrintf("powerCtrlTest_maxwell: testPowerModeInterface: Verifed pwr2all_gr_force_clks_on is 0 after PWR_BLCG_DISABLE == AUTO!\n");
    }
  }

  if (test_status_fail == true) {
    ErrPrintf("powerCtrlTest_maxwell: testPowerModeInterface, Failed in checking gr2pwr_blcg_disable interface\n");
  }
  else {
    InfoPrintf("powerCtrlTest_maxwell: testPowerModeInterface, Succeed in checking gr2pwr_blcg_disable interface\n");
  }
} else {
for (j=0; j< 10; j++) {
    // if the chip has SMC feature, randomly pic one of the fe(fe_comp)2pwr_mode interface to test.
    if (macros.lw_sys_has_smc_chiplet) {
        smc_index = rand () % macros.lw_scal_litter_max_num_smc_engines;
        InfoPrintf("powreCtrlTest_maxwell: testPowerModeInterface, SMC is enabled. Check smc_idx%d for loop %d\n", smc_index, j);
    }else{
        smc_index = 0;
    }
    //request pwr_mode to be FORCE_ON
    InfoPrintf("powerCtrlTest_maxwell: testPowerModeInterface, start checking FORCE_ON mode request from FE, round=%d\n", j);

    if (WritePowerMode("_FORCE_ON", smc_index))
    {
      ErrPrintf("LW_PGRAPH_PRI_FE_PWR_MODE could not be updated for the value _MODE_FORCE_ON\n");
    }
    i = 0;
    req_status = 1;
    do {
      lwgpu->GetRegFldNum("LW_PGRAPH_PRI_FE_PWR_MODE","_REQ",&req_status); //check mode request status
      InfoPrintf("powerCtrlTest_maxwell: testPowerModeInterface: current mode request status is %d.\n", req_status);
      powerCtrlTest_maxwell::DelayNs(10);
      i++;
    } while ( (req_status != 0) && (i<1000)  );
    if (req_status != 0) {
      ErrPrintf("powerCtrlTest_maxwell: testPowerModeInterface: Timed out waiting for fe2pwr mode request to be acked!\n");
      test_status_fail = true;
    }

    //check whether force_clks_on/force_full_power are asserted after FORCE_ON request from FE
    force_clks_on_status = 0;
    force_full_power_status = 0;
    force_slow_status = 1;
    powerCtrlTest_maxwell::DelayNs(10); //wait until the therm to react to the mode_req
    Platform::EscapeRead("pwr2all_gr_force_clks_on", 0, 1, &force_clks_on_status);
    Platform::EscapeRead("pwr2all_gr_force_full_power", 0, 1, &force_full_power_status);
    Platform::EscapeRead("ectl2tctl_gr_slow_req", 0, 1, &force_slow_status);
    if (force_clks_on_status!=1) {
      ErrPrintf("powerCtrlTest_maxwell: testPowerModeInterface: pwr2all_gr_force_clks_on should be 1, not 0, after FORCE_ON request from FE!\n");
      test_status_fail = true;
    }
    else {
      InfoPrintf("powerCtrlTest_maxwell: testPowerModeInterface: Verifed pwr2all_gr_force_clks_on is 1 after FORCE_ON request from FE !\n");
    }
    if (force_full_power_status!=1) {
      ErrPrintf("powerCtrlTest_maxwell: testPowerModeInterface: pwr2all_gr_force_full_power should be 1, not 0, after FORCE_ON request from FE!\n");
      test_status_fail = true;
    }
    else {
      InfoPrintf("powerCtrlTest_maxwell: testPowerModeInterface: Verifed pwr2all_gr_force_full_power is 1 after FORCE_ON request from FE !\n");
    }
    if (force_slow_status!=0) {
      ErrPrintf("powerCtrlTest_maxwell: testPowerModeInterface: ectl2tctl_gr_slow_req should be 0, not 1, after FORCE_ON request from FE!\n");
      test_status_fail = true;
    }
    else {
      InfoPrintf("powerCtrlTest_maxwell: testPowerModeInterface: Verifed ectl2tctl_gr_slow_req is 0 after FORCE_ON request from FE !\n");
    }

    //request pwr_mode to be FORCE_POWER
    InfoPrintf("powerCtrlTest_maxwell: testPowerModeInterface, start checking FORCE_POWER mode request from FE, round=%d\n", j);
    if (WritePowerMode("_FORCE_POWER", smc_index))
    {
      ErrPrintf("LW_PGRAPH_PRI_FE_PWR_MODE could not be updated for the value _MODE_FORCE_POWER\n");
    }
    i = 0;
    req_status = 1;
    do {
      lwgpu->GetRegFldNum("LW_PGRAPH_PRI_FE_PWR_MODE","_REQ",&req_status); //check mode request status
      InfoPrintf("powerCtrlTest_maxwell: testPowerModeInterface: current mode request status is %d.\n", req_status);
      powerCtrlTest_maxwell::DelayNs(10);
      i++;
    } while ( (req_status != 0) && (i<1000)  );
    if (req_status != 0) {
      ErrPrintf("powerCtrlTest_maxwell: testPowerModeInterface: Timed out waiting for fe2pwr mode request to be acked!\n");
      test_status_fail = true;
    }

    //check whether force_full_power is asserted after FORCE_POWER request from FE
    force_clks_on_status = 1;
    force_full_power_status = 0;
    force_slow_status = 1;
    powerCtrlTest_maxwell::DelayNs(10); //wait until the therm to react to the mode_req
    Platform::EscapeRead("pwr2all_gr_force_clks_on", 0, 1, &force_clks_on_status);
    Platform::EscapeRead("pwr2all_gr_force_full_power", 0, 1, &force_full_power_status);
    Platform::EscapeRead("ectl2tctl_gr_slow_req", 0, 1, &force_slow_status);
    if (force_clks_on_status!=0) {
      ErrPrintf("powerCtrlTest_maxwell: testPowerModeInterface: pwr2all_gr_force_clks_on should be 0, not 1, after FORCE_POWER request from FE!\n");
      test_status_fail = true;
    }
    else {
      InfoPrintf("powerCtrlTest_maxwell: testPowerModeInterface: Verifed pwr2all_gr_force_clks_on is 0 after FORCE_POWER request from FE !\n");
    }
    if (force_full_power_status!=1) {
      ErrPrintf("powerCtrlTest_maxwell: testPowerModeInterface: pwr2all_gr_force_full_power should be 1, not 0, after FORCE_POWER request from FE!\n");
      test_status_fail = true;
    }
    else {
      InfoPrintf("powerCtrlTest_maxwell: testPowerModeInterface: Verifed pwr2all_gr_force_full_power is 1 after FORCE_POWER request from FE !\n");
    }
    if (force_slow_status!=0) {
      ErrPrintf("powerCtrlTest_maxwell: testPowerModeInterface: ectl2tctl_gr_slow_req should be 0, not 1, after FORCE_POWER request from FE!\n");
      test_status_fail = true;
    }
    else {
      InfoPrintf("powerCtrlTest_maxwell: testPowerModeInterface: Verifed ectl2tctl_gr_slow_req is 0 after FORCE_POWER request from FE !\n");
    }

    //request pwr_mode to be SLOW
    InfoPrintf("powerCtrlTest_maxwell: testPowerModeInterface, start checking SLOW mode request from FE, round=%d\n", j);
    if (WritePowerMode("_SLOW", smc_index))
    {
      ErrPrintf("LW_PGRAPH_PRI_FE_PWR_MODE could not be updated for the value _MODE_SLOW\n");
    }
    i = 0;
    req_status = 1;
    do {
      lwgpu->GetRegFldNum("LW_PGRAPH_PRI_FE_PWR_MODE","_REQ",&req_status); //check mode request status
      InfoPrintf("powerCtrlTest_maxwell: testPowerModeInterface: current mode request status is %d.\n", req_status);
      powerCtrlTest_maxwell::DelayNs(10);
      i++;
    } while ( (req_status != 0) && (i<1000)  );
    if (req_status != 0) {
      ErrPrintf("powerCtrlTest_maxwell: testPowerModeInterface: Timed out waiting for fe2pwr mode request to be acked!\n");
      test_status_fail = true;
    }

    //check whether force_full_power are asserted after SLOW request from FE
    force_clks_on_status = 1;
    force_full_power_status = 1;
    force_slow_status = 0;
    powerCtrlTest_maxwell::DelayNs(10); //wait until the therm to react to the mode_req
    Platform::EscapeRead("pwr2all_gr_force_clks_on", 0, 1, &force_clks_on_status);
    Platform::EscapeRead("pwr2all_gr_force_full_power", 0, 1, &force_full_power_status);
    Platform::EscapeRead("ectl2tctl_gr_slow_req", 0, 1, &force_slow_status);
    if (force_clks_on_status!=0) {
      ErrPrintf("powerCtrlTest_maxwell: testPowerModeInterface: pwr2all_gr_force_clks_on should be 0, not 1, after SLOW mode request from FE!\n");
      test_status_fail = true;
    }
    else {
      InfoPrintf("powerCtrlTest_maxwell: testPowerModeInterface: Verifed pwr2all_gr_force_clks_on is 0 after SLOW mode request from FE !\n");
    }
    if (force_full_power_status!=0) {
      ErrPrintf("powerCtrlTest_maxwell: testPowerModeInterface: pwr2all_gr_force_full_power should be 0, not 1, after SLOW mode request from FE!\n");
      test_status_fail = true;
    }
    else {
      InfoPrintf("powerCtrlTest_maxwell: testPowerModeInterface: Verifed pwr2all_gr_force_full_power is 0 after SLOW mode request from FE !\n");
    }
    if (force_slow_status!=1) {
      ErrPrintf("powerCtrlTest_maxwell: testPowerModeInterface: ectl2tctl_gr_slow_req should be 1, not 0, after SLOW mode request from FE!\n");
      test_status_fail = true;
    }
    else {
      InfoPrintf("powerCtrlTest_maxwell: testPowerModeInterface: Verifed ectl2tctl_gr_slow_req is 1 after SLOW mode request from FE !\n");
    }

    // reset GR engine to trigger fe2all_engine_reset_
    InfoPrintf("powerCtrlTest_maxwell: testPowerModeInterface, start checking pwr_mode after GR reset, round=%d\n", j);
    lwgpu->GetRegFldNum("LW_PMC_DEVICE_ENABLE(0)", "", &data);
    mask = 0x00000000;
    mask = mask | (0x1 << macros.lw_device_info_graphics0_reset);
    mask = ~mask;
    data = data & mask;
    if (lwgpu->SetRegFldNum("LW_PMC_DEVICE_ENABLE(0)","",data)) {
        ErrPrintf("Could not update LW_PMC_DEVICE_ENABLE_0 to value 0x%x\n", data);
    }

    powerCtrlTest_maxwell::DelayNs(100);

    lwgpu->GetRegFldNum("LW_PMC_DEVICE_ENABLE(0)", "", &data);
    mask = 0x00000000;
    mask = mask | (0x1 << macros.lw_device_info_graphics0_reset);
    data = data | mask;
    if (lwgpu->SetRegFldNum("LW_PMC_DEVICE_ENABLE(0)","",data)) {
        ErrPrintf("Could not update LW_PMC_DEVICE_ENABLE_0 to value 0x%x\n", data);
    }

    //check whether force_clks_on/force_full_power are de-asserted after fe2all_engine_reset_
    force_clks_on_status = 1;
    force_full_power_status = 1;
    force_slow_status = 1;
    powerCtrlTest_maxwell::DelayNs(10); //wait until the therm to react to the mode_req
    Platform::EscapeRead("pwr2all_gr_force_clks_on", 0, 1, &force_clks_on_status);
    Platform::EscapeRead("pwr2all_gr_force_full_power", 0, 1, &force_full_power_status);
    Platform::EscapeRead("ectl2tctl_gr_slow_req", 0, 1, &force_slow_status);
    if (force_clks_on_status!=0) {
      ErrPrintf("powerCtrlTest_maxwell: testPowerModeInterface: pwr2all_gr_force_clks_on should be 0, not 1, after GR engine reset !\n");
      test_status_fail = true;
    }
    else {
      InfoPrintf("powerCtrlTest_maxwell: testPowerModeInterface: Verifed pwr2all_gr_force_clks_on is 0 after GR engine reset !\n");
    }
    if (force_full_power_status!=0) {
      ErrPrintf("powerCtrlTest_maxwell: testPowerModeInterface: pwr2all_gr_force_full_power should be 0, not 1, after GR engine reset!\n");
      test_status_fail = true;
    }
    else {
      InfoPrintf("powerCtrlTest_maxwell: testPowerModeInterface: Verifed pwr2all_gr_force_full_power is 0 after GR engine reset !\n");
    }
    if (force_slow_status!=0) {
      ErrPrintf("powerCtrlTest_maxwell: testPowerModeInterface: ectl2tctl_gr_slow_req should be 0, not 1, after GR engine reset!\n");
      test_status_fail = true;
    }
    else {
      InfoPrintf("powerCtrlTest_maxwell: testPowerModeInterface: Verifed ectl2tctl_gr_slow_req is 0 after GR engine reset !\n");
    }
  }

  if (test_status_fail == true) {
    ErrPrintf("powerCtrlTest_maxwell: testPowerModeInterface, Failed in checking whether fe2pwr_mode interface can handle reset cleanly\n");
  }
  else {
    InfoPrintf("powerCtrlTest_maxwell: testPowerModeInterface, Succeed in checking whether fe2pwr_mode interface can handle reset cleanly\n");
  }
}
  powerCtrlTest_maxwell::DelayNs(10000);
}

int powerCtrlTest_maxwell::WritePowerMode(string Mode, UINT32 smc_idx)
{
    UINT32 mode, reqSend, data;

    // Start from GA100 SMC feature is introduced. When SMC is enalbed
    // there will be multiple fe2pwr_mode interface. We should test all of them. 
    // so use the smc_idx to indicate which fe2pwr_mode interface will be set. 
    // If SMC id not defined, this smc_idx will be ignored.
    if (macros.lw_sys_has_smc_chiplet) {
        if (smc_idx >= macros.lw_scal_litter_max_num_smc_engines) {
            ErrPrintf("Error, smc_idx (%d) should be no larger than the maximum SMC idx (%d)", smc_idx, macros.lw_scal_litter_max_num_smc_engines-1);
            return 1;
        }
        lwgpu->GetRegFldNum("LW_PPRIV_SYS_BAR0_TO_PRI_WINDOW", "", &data); // Just read this register to flush out previous priv accesses.
        data = smc_idx | 1 << 31;
        lwgpu->SetRegFldNum("LW_PPRIV_SYS_BAR0_TO_PRI_WINDOW", "", data);
        // Make sure the write successed.
        data = 0;
        lwgpu->GetRegFldNum("LW_PPRIV_SYS_BAR0_TO_PRI_WINDOW", "", &data);
        if (data != (smc_idx | 1 << 31)) {
            ErrPrintf("Error, Failed to set LW_PPRIV_SYS_BAR0_TO_PRI_WINDOW. Expected 0x%x, actual get 0x%x", (smc_idx | 1 << 31), data);
            return 1;
        }
    }

    // If this succeeds, the next calls must succeed
    if (lwgpu->GetRegFldDef("LW_PGRAPH_PRI_FE_PWR_MODE", "_MODE", Mode.c_str(), &mode) ||
        lwgpu->GetRegFldDef("LW_PGRAPH_PRI_FE_PWR_MODE", "_REQ", "_SEND", &reqSend))
    {
        ErrPrintf("Failed with GetRegFldDef\n");
        return 1;
    }

    unique_ptr<IRegisterClass> pReg = lwgpu->GetRegister("LW_PGRAPH_PRI_FE_PWR_MODE");
    unique_ptr<IRegisterField> pMode = pReg->FindField("LW_PGRAPH_PRI_FE_PWR_MODE_MODE");
    unique_ptr<IRegisterField> pReq = pReg->FindField("LW_PGRAPH_PRI_FE_PWR_MODE_REQ");

    UINT32 NewValue = (mode << pMode->GetStartBit()) | (reqSend << pReq->GetStartBit());
    UINT32 Mask  = pMode->GetWriteMask() | pReq->GetWriteMask();

    UINT32 Value = lwgpu->RegRd32(pReg->GetAddress());

    Value = (Value & ~Mask) | NewValue;

    lwgpu->RegWr32(pReg->GetAddress(), Value);

    return 0;
}
// Initialize some global constants related to which engines are present in the chip,
// and some default enable/disable values for engine-level and block-level clock-gating.
int powerCtrlTest_maxwell::InitSetup(void)
{
  MYGRINFO grInfo;
  LW2080_CTRL_GR_GET_INFO_PARAMS grInfoParams = {0};

  LwRmPtr pLwRm;
  LwRm::Handle hSubdevice = pLwRm->GetSubdeviceHandle(lwgpu->GetGpuSubdevice());
  RC rc = OK;

  //Get fb info
  LW2080_CTRL_FB_INFO fbInfo = { LW2080_CTRL_FB_INFO_INDEX_FBP_COUNT };
  LW2080_CTRL_FB_GET_INFO_PARAMS Params = { 1, LW_PTR_TO_LwP64(&fbInfo) };
  CHECK_RC(pLwRm->Control(hSubdevice,
                            LW2080_CTRL_CMD_FB_GET_INFO,
                            &Params, sizeof(Params)));
  n_fbp = fbInfo.data;
  InfoPrintf("There are %d fbps available in the design!\n", n_fbp);

  // get general gr info
  grInfo.bufferAlignment.index          = LW2080_CTRL_GR_INFO_INDEX_BUFFER_ALIGNMENT;
  grInfo.swizzleAlignment.index         = LW2080_CTRL_GR_INFO_INDEX_SWIZZLE_ALIGNMENT;
  grInfo.vertexCacheSize.index          = LW2080_CTRL_GR_INFO_INDEX_VERTEX_CACHE_SIZE;
  grInfo.vpeCount.index                 = LW2080_CTRL_GR_INFO_INDEX_VPE_COUNT;
  grInfo.shaderPipeCount.index          = LW2080_CTRL_GR_INFO_INDEX_SHADER_PIPE_COUNT;
  grInfo.threadStackScalingFactor.index = LW2080_CTRL_GR_INFO_INDEX_THREAD_STACK_SCALING_FACTOR;
  grInfo.shaderPipeSubCount.index       = LW2080_CTRL_GR_INFO_INDEX_SHADER_PIPE_SUB_COUNT;
  grInfo.smRegBankCount.index           = LW2080_CTRL_GR_INFO_INDEX_SM_REG_BANK_COUNT;
  grInfo.smRegBankRegCount.index        = LW2080_CTRL_GR_INFO_INDEX_SM_REG_BANK_REG_COUNT;
  grInfo.smVersion.index                = LW2080_CTRL_GR_INFO_INDEX_SM_VERSION;
  grInfo.maxWarpsPerSm.index            = LW2080_CTRL_GR_INFO_INDEX_MAX_WARPS_PER_SM;
  grInfo.maxThreadsPerWarp.index        = LW2080_CTRL_GR_INFO_INDEX_MAX_THREADS_PER_WARP;
  grInfo.geomGsObufEntries.index        = LW2080_CTRL_GR_INFO_INDEX_GEOM_GS_OBUF_ENTRIES;
  grInfo.geomXbufEntries.index          = LW2080_CTRL_GR_INFO_INDEX_GEOM_XBUF_ENTRIES;
  grInfo.fbMemReqGranularity.index      = LW2080_CTRL_GR_INFO_INDEX_FB_MEMORY_REQUEST_GRANULARITY;
  grInfo.hostMemReqGranularity.index    = LW2080_CTRL_GR_INFO_INDEX_HOST_MEMORY_REQUEST_GRANULARITY;
  grInfo.litterNumGpcs.index            = LW2080_CTRL_GR_INFO_INDEX_LITTER_NUM_GPCS;
  grInfo.litterNumFbps.index            = LW2080_CTRL_GR_INFO_INDEX_LITTER_NUM_FBPS;
  grInfo.litterNumZlwllBanks.index      = LW2080_CTRL_GR_INFO_INDEX_LITTER_NUM_ZLWLL_BANKS;
  grInfo.litterNumTpcPerGpc.index       = LW2080_CTRL_GR_INFO_INDEX_LITTER_NUM_TPC_PER_GPC;

  grInfoParams.grInfoListSize = sizeof (grInfo) / sizeof (LW2080_CTRL_GR_INFO);
  grInfoParams.grInfoList = (LwP64)&grInfo;

  CHECK_RC(pLwRm->Control(hSubdevice,
                            LW2080_CTRL_CMD_GR_GET_INFO,
                            (void*)&grInfoParams,
                            sizeof(grInfoParams)));

  n_gpc = (unsigned int)grInfo.shaderPipeCount.data;
  n_tpc = (unsigned int)grInfo.shaderPipeSubCount.data/n_gpc;
  InfoPrintf("There are %d gpcs available in the design!\n", n_gpc);
  InfoPrintf("There are %d tpcs available in each gpc!\n", n_tpc);

  // Setup some global constants
  setupPMC_ENABLE();
  
  // Assign allELCGGatedType. This parameter is used in all ELCG tests.
  allELCGGatedType =  maxwell_clock_gating.MAXWELL_CLOCK_GR_ENGINE_GATED 
                    | maxwell_clock_gating.MAXWELL_CLOCK_CE0_ENGINE_GATED  
                    | maxwell_clock_gating.MAXWELL_CLOCK_CE1_ENGINE_GATED 
                    | maxwell_clock_gating.MAXWELL_CLOCK_CE2_ENGINE_GATED
                    | maxwell_clock_gating.MAXWELL_CLOCK_CE3_ENGINE_GATED 
                    | maxwell_clock_gating.MAXWELL_CLOCK_CE4_ENGINE_GATED 
                    | maxwell_clock_gating.MAXWELL_CLOCK_CE5_ENGINE_GATED
                    | maxwell_clock_gating.MAXWELL_CLOCK_CE6_ENGINE_GATED
                    | maxwell_clock_gating.MAXWELL_CLOCK_CE7_ENGINE_GATED 
                    | maxwell_clock_gating.MAXWELL_CLOCK_CE8_ENGINE_GATED 
                    | maxwell_clock_gating.MAXWELL_CLOCK_CE9_ENGINE_GATED 
                    | maxwell_clock_gating.MAXWELL_CLOCK_LWENC_ENGINE_GATED
                    | maxwell_clock_gating.MAXWELL_CLOCK_LWENC1_ENGINE_GATED 
                    | maxwell_clock_gating.MAXWELL_CLOCK_LWENC2_ENGINE_GATED
                    | maxwell_clock_gating.MAXWELL_CLOCK_LWDEC_ENGINE_GATED 
                    | maxwell_clock_gating.MAXWELL_CLOCK_LWDEC1_ENGINE_GATED 
                    | maxwell_clock_gating.MAXWELL_CLOCK_LWDEC2_ENGINE_GATED
                    | maxwell_clock_gating.MAXWELL_CLOCK_LWDEC3_ENGINE_GATED
                    | maxwell_clock_gating.MAXWELL_CLOCK_LWDEC4_ENGINE_GATED
                    | maxwell_clock_gating.MAXWELL_CLOCK_LWDEC5_ENGINE_GATED
                    | maxwell_clock_gating.MAXWELL_CLOCK_LWDEC6_ENGINE_GATED
                    | maxwell_clock_gating.MAXWELL_CLOCK_LWDEC7_ENGINE_GATED
                    | maxwell_clock_gating.MAXWELL_CLOCK_LWJPG0_ENGINE_GATED
                    | maxwell_clock_gating.MAXWELL_CLOCK_LWJPG1_ENGINE_GATED
                    | maxwell_clock_gating.MAXWELL_CLOCK_LWJPG2_ENGINE_GATED
                    | maxwell_clock_gating.MAXWELL_CLOCK_LWJPG3_ENGINE_GATED
                    | maxwell_clock_gating.MAXWELL_CLOCK_LWJPG4_ENGINE_GATED
                    | maxwell_clock_gating.MAXWELL_CLOCK_LWJPG5_ENGINE_GATED
                    | maxwell_clock_gating.MAXWELL_CLOCK_LWJPG6_ENGINE_GATED
                    | maxwell_clock_gating.MAXWELL_CLOCK_LWJPG7_ENGINE_GATED
                    | maxwell_clock_gating.MAXWELL_CLOCK_OFA_ENGINE_GATED
                    | maxwell_clock_gating.MAXWELL_CLOCK_SEC_ENGINE_GATED
                    ;
  return 0;
}

void powerCtrlTest_maxwell::Run(void)
{
  test_status_fail = false;
  //  InfoPrintf Flag to determine whether the test fail because of elw issue or test issue.
  InfoPrintf("FLAG : powerCtrlTest_maxwell starting ....\n");

  SetStatus(TEST_INCOMPLETE);

  InfoPrintf("powerCtrlTest_maxwell: Run, doing some more setup\n");

  InfoPrintf("powerCtrlTest_maxwell: register access test\n");

  InitSetup();

  InfoPrintf("powerCtrlTest_maxwell: Run, begin testing\n");

    // FIXME WAR for utilsclk running at 108Mhz, should remove those lines after VBIOS update.
    //==========================================
//    if(lwgpu->SetRegFldNum("LW_PTRIM_SYS_XTAL4X_CTRL","",0x00000000))
//    {
//        CHECK_PLL_TEST_RC(SETREG_ERROR,run_mode);
//    }
//    else
//    {
//        InfoPrintf("Set xtal4x pll unbypass sucessfully\n");
//    }
    //===========================================

    // Initialize vcodiv_jtag
    Platform::EscapeWrite("drv_gpcclk_vcodiv_jtag", 0, 6, 4);
    Platform::EscapeWrite("drv_sysclk_vcodiv_jtag", 0, 6, 8);
    Platform::EscapeWrite("drv_ltcclk_vcodiv_jtag", 0, 6, 4);
    Platform::EscapeWrite("drv_legclk_vcodiv_jtag", 0, 6, 38);

  //Enable LW_PGRAPH_PRI_GPCS_TPCS_PE_BLKCG_CG
  UINT32 data;
  lwgpu->RegWr32(0x00419804, 0x1f);
  data = lwgpu->RegRd32(0x00419804);
  InfoPrintf("LW_PGRAPH_PRI_GPCS_TPCS_PE_BLKCG_CG = %x\n", data);

  // Start from Pascal, CE engines has PCE and LCE define. LCG number may >= PCE number.
  // TO verify every PCE, test should first map LCE and PCE>
  // Simply in this test we will map
  // LCE0->PCE0
  // LCE1->PCE1
  // LCE2->PCE2
  // ...
  // For those LCE number larger than PCEs, we don't test them.
  
  // Setup LW_CE_GRCE_CONFIG to 0.
  lwgpu->SetRegFldNum("LW_CE_GRCE_CONFIG(0)", "_SHARED", 0);
  lwgpu->SetRegFldNum("LW_CE_GRCE_CONFIG(1)", "_SHARED", 0);

  if (macros.lw_scal_litter_num_ce_lces > 0 && macros.lw_scal_litter_num_ce_pces > 0) {
    lwgpu->SetRegFldNum("LW_CE_PCE2LCE_CONFIG(0)", "_PCE_ASSIGNED_LCE", 0);
  }
  if (macros.lw_scal_litter_num_ce_lces > 1 && macros.lw_scal_litter_num_ce_pces > 1) {
    lwgpu->SetRegFldNum("LW_CE_PCE2LCE_CONFIG(1)", "_PCE_ASSIGNED_LCE", 1);
  }
  if (macros.lw_scal_litter_num_ce_lces > 2 && macros.lw_scal_litter_num_ce_pces > 2) {
    lwgpu->SetRegFldNum("LW_CE_PCE2LCE_CONFIG(2)", "_PCE_ASSIGNED_LCE", 2);
  }
  if (macros.lw_scal_litter_num_ce_lces > 3 && macros.lw_scal_litter_num_ce_pces > 3) {
    lwgpu->SetRegFldNum("LW_CE_PCE2LCE_CONFIG(3)", "_PCE_ASSIGNED_LCE", 3);
  }
  if (macros.lw_scal_litter_num_ce_lces > 4 && macros.lw_scal_litter_num_ce_pces > 4) {
    lwgpu->SetRegFldNum("LW_CE_PCE2LCE_CONFIG(4)", "_PCE_ASSIGNED_LCE", 4);
  }
  if (macros.lw_scal_litter_num_ce_lces > 5 && macros.lw_scal_litter_num_ce_pces > 5) {
    lwgpu->SetRegFldNum("LW_CE_PCE2LCE_CONFIG(5)", "_PCE_ASSIGNED_LCE", 5);
  }
  if (macros.lw_scal_litter_num_ce_lces > 6 && macros.lw_scal_litter_num_ce_pces > 6) {
    lwgpu->SetRegFldNum("LW_CE_PCE2LCE_CONFIG(6)", "_PCE_ASSIGNED_LCE", 6);
  }
  if (macros.lw_scal_litter_num_ce_lces > 7 && macros.lw_scal_litter_num_ce_pces > 7) {
    lwgpu->SetRegFldNum("LW_CE_PCE2LCE_CONFIG(7)", "_PCE_ASSIGNED_LCE", 7);
  }
  if (macros.lw_scal_litter_num_ce_lces > 8 && macros.lw_scal_litter_num_ce_pces > 8) {
    lwgpu->SetRegFldNum("LW_CE_PCE2LCE_CONFIG(8)", "_PCE_ASSIGNED_LCE", 8);
  }
  if (macros.lw_scal_litter_num_ce_lces > 9 && macros.lw_scal_litter_num_ce_pces > 9) {
    lwgpu->SetRegFldNum("LW_CE_PCE2LCE_CONFIG(9)", "_PCE_ASSIGNED_LCE", 9);
  }
  if (macros.lw_scal_litter_num_ce_lces > 10 && macros.lw_scal_litter_num_ce_pces > 10) {
    lwgpu->SetRegFldNum("LW_CE_PCE2LCE_CONFIG(10)", "_PCE_ASSIGNED_LCE", 10);
  }
  
  if (m_params->ParamPresent("-testPerEngineClkGating")) {
    testPerEngineClkGating();
   }
  if (m_params->ParamPresent("-testClkOnForReset")) {
    testClkOnForReset();
   }
  if (m_params->ParamPresent("-testELCGPriWakeup")) {
    testELCGPriWakeup();
   }
  if (m_params->ParamPresent("-testELCGHubmmuWakeup")) {
    testELCGHubmmuWakeup();
   }
  if (m_params->ParamPresent("-testELCGfecsInterfaceReset")) {
    testELCGfecsInterfaceReset();
   }
  if (m_params->ParamPresent("-testELCGhubmmuInterfaceReset")) {
    testELCGhubmmuInterfaceReset();
   }
  if (m_params->ParamPresent("-testELCGgpccsInterfaceReset")) {
    testELCGgpccsInterfaceReset();
   }
  if (m_params->ParamPresent("-testELCGbecsInterfaceReset")) {
    testELCGbecsInterfaceReset();
   }
  if (m_params->ParamPresent("-testPowerModeInterface")) {
    testPowerModeInterface();
  }
  if (m_params->ParamPresent("-testSMCFloorsweep")) {
    testSMCFloorsweep();
   }

  InfoPrintf("powerCtrlTest_maxwell: Run, all tests finished\n");

  if (test_status_fail) {
    SetStatus(TEST_FAILED);
    getStateReport()->runFailed("powerCtrlTest_maxwell test failed\n");
  } else {
    SetStatus(TEST_SUCCEEDED);
    getStateReport()->runSucceeded();
  }
}

