/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2017-2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */
/*-----------------------------------------------------------------------------------------------
Test :    gc6plus_maxwell.cpp 
Purpose:  A fast test which completes within 20 mins on full-chip RTL. We add -skipdevinit for this.
          This test is to check gc6plus connections in full-chip flat gates are working.
------------------------------------------------------------------------------------------------- */
#include "mdiag/tests/stdtest.h"

#include "gc6plus_maxwell.h"

#include "ampere/ga102/dev_fuse.h"
#include "as2_speedup_golden_sim_gc6plus_seq_default_sdr.h"
#include "check_clamp.h"
#include "core/include/gpu.h"
#include "core/include/rc.h"
#include "core/include/utility.h"
#include "device/interface/pcie.h"
#include "golden_sim_fgc6_default.h"
#include "golden_sim_gc6plus_seq_default_sdr_fc_directed.h"
#include "golden_sim_rtd3_default.h"
#include "golden_sim_rtd3_sli_default.h"
#include "gpio_engine.h"
#include "gpu/include/gpusbdev.h"
#include "gpu/pcie/pcieimpl.h"
#include "mdiag/lwgpu.h"
#include "mdiag/tests/test_state_report.h"
#include "mdiag/utils/crc.h"
#include "mdiag/utils/lwgpu_channel.h"
#include "lwmisc.h"
#include "sim/IChip.h"
#include "stdlib.h"

#include <fstream>
#include <iostream>
#include <string>
#include <sstream>
#include <iomanip>

#define TEST_NAME     "gc6plus_maxwell.cpp"

GC6plus_Maxwell::ModeType GC6plus_Maxwell::mode = INIT_MODE;
GC6plus_Maxwell::SeqType GC6plus_Maxwell::seq = INIT_SEQ;
GC6plus_Maxwell::Fgc6Param GC6plus_Maxwell::fgc6_mode=NONE;
GC6plus_Maxwell::Fgc6Param GC6plus_Maxwell::fgc6_wakeup_type = WAKEUP_TIMER;
//int err = 0;
extern const ParamDecl gc6plus_maxwell_params[] = {
  SIMPLE_PARAM("-gc5", "GC5"),
  SIMPLE_PARAM("-gc6", "GC6"),
  SIMPLE_PARAM("-rtd3", "RTD3"),
  SIMPLE_PARAM("-exit", "Exit"),
  SIMPLE_PARAM("-abort", "Abort"),
  SIMPLE_PARAM("-reset", "Reset"),
  SIMPLE_PARAM("-steady", "Steady State Test"),
  SIMPLE_PARAM("-is_fmodel", "Running on Fmodel"),
  SIMPLE_PARAM("-as2_speedup", "Running on AS2, applying speedup."),
  SIMPLE_PARAM("-register_test", " Check GC6+ Registers"),
  SIMPLE_PARAM("-rpg_test", " Check BSI RAM RPG"),
  SIMPLE_PARAM("-unmapped_test", " Check GC6+ Registers for unmapped SCI location access"), 
  SIMPLE_PARAM("-randomize_regs", " Randomize GC6+ Registers"),
  SIMPLE_PARAM("-bsi2pmu_test", " BSI2PMU boot strap"),
  SIMPLE_PARAM("-ptimer_test", " Test ptimer can be read and written correctly"),
  SIMPLE_PARAM("-sw_force_wakeup_test", " Test GC5/GC6 entry and exit/abort through SW forced wakeups"),
  SIMPLE_PARAM("-pex_wakeup", " Test GC5/GC6 exit/abort from PCIe events"),
  SIMPLE_PARAM("-clkreq_in_gc6", "Enable the new CLKREQ feature for GC6"),
  SIMPLE_PARAM("-bsi_padctrl_in_gc6", "Test SW Controllability for BSI_PADCTRL"), 
  SIMPLE_PARAM("-rand_lwvdd_ok_offset", "Offset LWVDD_OK for the island LDO(s) by a random amount"),
  STRING_PARAM("-wakeup_timer", " Test GC5/GC6 entry and exit/abort through a wakeup timer"), 
  STRING_PARAM("-fgc6_wakeuptest", " Test FGC6 entry and exit/abort through a wakeup timer"),
  SIMPLE_PARAM("-ppc_rtd3_wake", " Wakeup RTD3 through a ppc event "), 
  SIMPLE_PARAM("-xusb_rtd3_wake", " Wakeup RTD3 through a xusb event "), 
  STRING_PARAM("-rtd3_wakeup_timer", " Test RTD3 entry and exit/abort through a wakeup timer"), 
  STRING_PARAM("-rtd3_cpu_wakeup", " Test RTD3 entry and exit/abort through a CPU wakeup"), 
  STRING_PARAM("-rtd3_hpd_wakeup", " Test RTD3 entry and exit/abort through a hpd wakeup"), 
  SIMPLE_PARAM("-rtd3_system_reset", " Test RTD3 entry and exit/abort through a system reset"), 
  STRING_PARAM("-hpd_wakeup", " Test GC5/GC6 entry and exit/abort through a hpd wakeup"), 
  STRING_PARAM("-fgc6_hpd_wakeup", " Test FGC6 entry and exit/abort through a hpd wakeup"), 
  SIMPLE_PARAM("-xusb_ppc_event", " Test GC6/FGC6 entry and exit through a wake up from XUSB and PPC event "), 
  SIMPLE_PARAM("-gpio", " Test GC5/GC6 entry and exit/abort through a GPIO wakeup"),
  SIMPLE_PARAM("-gpio_wakeup_test", " Test GPIO events"),
  SIMPLE_PARAM("-simultaneous_gpio_wakeup", "Assert an additional wakeup"),
  STRING_PARAM("-simultaneous_gpio_wakeup_delay_ns", "Delay between escape Write to EC and GPIO"),
  SIMPLE_PARAM("-intr_gpu_event", " Test GC5/GC6 entry and exit on EC initiated GPU_Event"),
  SIMPLE_PARAM("-fgc6_intr_gpu_event", " Test GC5/GC6 entry and exit on EC initiated GPU_Event"),
  SIMPLE_PARAM("-intr_pex_reset", " Test GC5 entry and reset on EC initiated PEX_Reset"),
  SIMPLE_PARAM("-gc6_exit_i2c_test", " Test gc6 entry and exit through an i2c request"),
  SIMPLE_PARAM("-gc6_exit_i2c_timeout_test", " Test SCI Timeout (SCL) on gc6 exit through an i2c request"), 
  SIMPLE_PARAM("-gc6_abort_i2c_test", " Test gc6 entry and abort through an i2c request"),
  SIMPLE_PARAM("-gc6_sci_i2c_test", " Test gc6 entry and i2c access to SCI"),
  SIMPLE_PARAM("-gc6_simultaneous_i2c_gpio_test", " Test gc6 exit with simultaneous i2c and gpio wakeups"),
  SIMPLE_PARAM("-i2c_therm_test", " i2c therm test through SCI in steady state"),
  SIMPLE_PARAM("-test_bsi_priv", "Test BSI priv for i2c_therm_Test"),  
  SIMPLE_PARAM("-system_reset", "test system reset in steady state"),
  SIMPLE_PARAM("-fundamental_reset", "Fundamental reset in steady state"),
  SIMPLE_PARAM("-software_reset", "Software reset"), 
  STRING_PARAM("-bsiRamFile", "path to file containing bsi ram ctrl/data"),
  SIMPLE_PARAM("-clamp_test", "Fail clamp asserts to make sure they are working"),
  SIMPLE_PARAM("-gpu_event_clash", "SCI and EC both assert GPU_EVENT"),
  SIMPLE_PARAM("-cya_mode", "cya_mode"),
  SIMPLE_PARAM("-arm_bsi_cya_gc6_mode", "arm_bsi_cya_gc6_mode"),
  SIMPLE_PARAM("-gc6_abort_skip_disable", "Disable GC6 abort skip"),
  SIMPLE_PARAM("-multiple_wakeups", "Run the multiple wakeups test"),
  STRING_PARAM("-entry_delay","The amount of time (in usec) SCI will wait in entry state before transitioning"),
  STRING_PARAM("-exit_delay","The amount of time (in usec) to delay after triggering GC5/GC6 before calling cleanup routine"),
  UNSIGNED_PARAM("-ec_reset_width_us", "Width of EC generated reset (in us, default 100 us)"),
  UNSIGNED_PARAM("-ec_gpu_event_width_us", "Width of EC generated GPU_EVENT (in us, default 100 us)"),
  UNSIGNED_PARAM("-polling_timeout_us", "Timeout for polling loops (in us, default 5000 us)"),
  UNSIGNED_PARAM("-sci_gpu_event_width_us", "On GC6 Entry/Exit, this is how long SCI asserts GPU_EVENT (units in us, default = 20us)"),
  UNSIGNED_PARAM("-seed", "Random seed used for any randomization"),
  UNSIGNED_PARAM("-lwvdd_ok_val", "LDO LWVDD_OK value. Applies to both BSI and SCI. Gray Coded"),
  SIGNED_PARAM("-lwvdd_ok_offset", "Offset of BSI LDO, in mV relative to SCI LDO. If rand_lwvdd_ok_offset is used, this parameter is ignored (default=0)"),
  SIMPLE_PARAM("-system_reset_mon", "Enable GPIO input for system reset mon"),
  SIMPLE_PARAM("-bsi2pmu_bar0", "Tests different bar 0 accesss with PMU "),
  SIMPLE_PARAM("-bsi2pmu_32_phases", "Tests all 32 phases of  BSI2PMU "),
  STRING_PARAM("-map_misc_0", " GPIO pin to be mapped for misc_0"), 
   // IFF and IFF related code
  SIMPLE_PARAM("-initfromfuse_gc6", "Test verifies IFF at gc6 exit"),
  SIMPLE_PARAM("-initfromrom_gc6", "Test verifies IFR at gc6 exit"),
  SIMPLE_PARAM("-fgc6_l1", " Set FGC6 mode to L1"),
  SIMPLE_PARAM("-fgc6_l2", " Set FGC6 mode to L2"),
  SIMPLE_PARAM("-fgc6_deep_l1", " Set FGC6 mode to Deep L1"),
  SIMPLE_PARAM("-rtd3_sli", " Test verifies RTD3 SLI"),
  PARAM_SUBDECL(lwgpu_single_params),
  LAST_PARAM
};

int GC6plus_Maxwell::gc6plus_init(GpuSubdevice *pSubdev){
    unique_ptr<IRegisterClass> reg;
    unique_ptr<IRegisterField> field;
    unique_ptr<IRegisterValue> value;
    string regname, fieldname, valuename;
    UINT32 reg_addr;
    UINT32 reg_data;
    UINT32 rand = 0;
    int rand_signed = 0;
    int err = 0;
    UINT32 addr;
    UINT32 mask;
    UINT32 start_bit;
    UINT32 reg_rd_val, reg_wr_val;
    m_rndStream = new RandomStream(m_seed, 0x222, 0x333);


    this->debug_tag_data = 0xa1115a90;
    this->pcieInit(pSubdev);
    this->priv_init(pSubdev);
    this->init_ptimer(pSubdev);

    //This is not needed for FGC6 in L1
    if(!is_fgc6 & !is_fgc6_l2) {
	    DebugPrintf("Writing rtd3_pcie_bfm_reset  to 1\n");
	    EscapeWrite("rtd3_pcie_bfm_reset", 0, 1, 1);
    }
    //This is added to the init sequence for the ECO tracked in bug 1451839
    //This bit enables the ECO, the ECO is disabled by default
    reg_data = ReadReg(pSubdev, m_regMap, "LW_PGC6_BSI_SPARE");
    InfoPrintf("gc6plus_maxwell_gc6plus_init: LW_PGC6_BSI_SPARE is 0x%08x \n", reg_data);

    reg_data &= ~0x80;

    Find_Register(m_regMap, reg, "LW_PGC6_BSI_SPARE"); 
    reg_addr = reg->GetAddress();
    pSubdev->RegWr32(reg_addr, reg_data);
    InfoPrintf("gc6plus_maxwell_gc6plus_init: Wiring LW_PGC6_BSI_SPARE with 0x%08x \n", reg_data);

    //This is needed for the HOST ECO tracked in bug 1455832 
    if (!(reg = m_regMap->FindRegister("LW_PMC_ELPG_ENABLE"))) {
	ErrPrintf("gc6plus_init: register LW_PMC_ELPG_ENABLE is not defined!\n");
	return (1);
	err++;
    }
    if (!(field = reg->FindField("LW_PMC_ELPG_ENABLE_XBAR"))) {
	ErrPrintf("gc6plus_init: field LW_PMC_ELPG_ENABLE_XBAR is not defined!\n");
	return (1);
	err++;
    }
    if (!(value = field->FindValue("LW_PMC_ELPG_ENABLE_XBAR_ENABLED"))) {
	ErrPrintf("gc6plus_init: value LW_PMC_ELPG_ENABLE_XBAR_ENABLED is not defined!\n");
	return (1);
	err++;
    }
    addr = reg->GetAddress();
    reg_rd_val = lwgpu->RegRd32(addr);
    DebugPrintf("gc6plus_init: read LW_PMC_ELPG_ENABLE,reg_rd_val=0x%x\n",reg_rd_val);
    mask = field->GetMask();
    start_bit = field->GetStartBit();
    reg_wr_val = (reg_rd_val | mask) & (~mask | ((value->GetValue()  << start_bit)));

    if (!(field = reg->FindField("LW_PMC_ELPG_ENABLE_L2"))) {
	ErrPrintf("gc6plus_init: field LW_PMC_ELPG_ENABLE_L2 is not defined!\n");
	return (1);
	err++;
    }

    if (!(value = field->FindValue("LW_PMC_ELPG_ENABLE_L2_ENABLED"))) {
	ErrPrintf("gc6plus_init: value LW_PMC_ELPG_ENABLE_L2_ENABLED is not defined!\n");
	return (1);
	err++;
    }

    mask = field->GetMask();
    reg_wr_val = (reg_wr_val | mask) & (~mask | ((value->GetValue()  << start_bit)));

    if (!(field = reg->FindField("LW_PMC_ELPG_ENABLE_PFB"))) {
	InfoPrintf("gc6plus_init: WARNING: field LW_PMC_ELPG_ENABLE_PFB is not defined! Possibly removed in the manuals.\n");
    }
    else {
	if (!(value = field->FindValue("LW_PMC_ELPG_ENABLE_PFB_ENABLED"))) {
	    InfoPrintf("gc6plus_init: WARNING: value LW_PMC_ELPG_ENABLE_PFB_ENABLED is not defined! Possibly removed in the manuals.\n");
	}
	mask = field->GetMask();
    } 
    reg_wr_val = (reg_wr_val | mask) & (~mask | ((value->GetValue()  << start_bit)));

    if (!(field = reg->FindField("LW_PMC_ELPG_ENABLE_HUB"))) {
	ErrPrintf("gc6plus_init: field LW_PMC_ELPG_ENABLE_HUB is not defined!\n");
	return (1);
	err++;
    }

    if (!(value = field->FindValue("LW_PMC_ELPG_ENABLE_HUB_ENABLED"))) {
	ErrPrintf("gc6plus_init: value LW_PMC_ELPG_ENABLE_HUB_ENABLED is not defined!\n");
	return (1);
	err++;
    }

    mask = field->GetMask();
    reg_wr_val = (reg_wr_val | mask) & (~mask | ((value->GetValue()  << start_bit)));

    lwgpu->RegWr32(addr,reg_wr_val);
    DebugPrintf("gc6plus_init: write LW_PMC_ELPG_ENABLE,reg_wr_val=0x%x\n",reg_wr_val);

    // configure FSM configuration
    regname =  "LW_PGC6_SCI_FSM_CFG";
    if (!(reg = m_regMap->FindRegister(regname.c_str())))
    {
	ErrPrintf("gc6plus_maxwell: Failed to find %s register\n", regname.c_str());
	return (1);
    }
    reg_addr = reg->GetAddress();
    fieldname = "LW_PGC6_SCI_FSM_CFG_SELF_DRIVE_RST";
    if (!(field = reg->FindField(fieldname.c_str())))
    {
	ErrPrintf("gc6plus_maxwell: Failed to find %s field\n", fieldname.c_str());
	return (1);
    }
    valuename = "LW_PGC6_SCI_FSM_CFG_SELF_DRIVE_RST_ENABLE";
    if (!(value = field->FindValue(valuename.c_str())))
    {
	ErrPrintf("gc6plus_maxwell: Failed to find %s value\n", valuename.c_str());
	return (1);
    }
    reg_data = value->GetValue() << field->GetStartBit();
    fieldname = "LW_PGC6_SCI_FSM_CFG_RESET_GPU_EVENT";
    if (!(field = reg->FindField(fieldname.c_str())))
    {
	ErrPrintf("gc6plus_maxwell: Failed to find %s field\n", fieldname.c_str());
	return (1);
    }
    valuename = "LW_PGC6_SCI_FSM_CFG_RESET_GPU_EVENT_DISABLE";
    if (!(value = field->FindValue(valuename.c_str())))
    {
	ErrPrintf("gc6plus_maxwell: Failed to find %s value\n", valuename.c_str());
	return (1);
    }
    reg_data |= value->GetValue() << field->GetStartBit();
    fieldname = "LW_PGC6_SCI_FSM_CFG_SKIP_GC6_ABORT";
    if (!(field = reg->FindField(fieldname.c_str())))
    {
	ErrPrintf("gc6plus_maxwell: Failed to find %s field\n", fieldname.c_str());
	return (1);
    }
    if (abort_skip_disable_check == 1)
    {
	valuename = "LW_PGC6_SCI_FSM_CFG_SKIP_GC6_ABORT_DISABLE";
	InfoPrintf("gc6plus_maxwell: GC6 Abort skip disabled");
    }
    else
    {
	valuename = "LW_PGC6_SCI_FSM_CFG_SKIP_GC6_ABORT_ENABLE";
	InfoPrintf("gc6plus_maxwell: GC6 Abort skip enabled");
    } 

    if (!(value = field->FindValue(valuename.c_str())))
    {
	ErrPrintf("gc6plus_maxwell: Failed to find %s value\n", valuename.c_str());
	return (1);
    }
    reg_data |= value->GetValue() << field->GetStartBit();
    fieldname = "LW_PGC6_SCI_FSM_CFG_SR_UPDATE_MODE";
    if (!(field = reg->FindField(fieldname.c_str())))
    {
	ErrPrintf("gc6plus_maxwell: Failed to find %s field\n", fieldname.c_str());
	return (1);
    }
    valuename = "LW_PGC6_SCI_FSM_CFG_SR_UPDATE_MODE_SECOND_EDGE";
    if (!(value = field->FindValue(valuename.c_str())))
    {
	ErrPrintf("gc6plus_maxwell: Failed to find %s value\n", valuename.c_str());
	return (1);
    }
    reg_data |= value->GetValue() << field->GetStartBit();

    //The following fields are added for RTD3 and FGC6
    if(this->mode==RTD3 || is_fgc6 ){

	//enable INTERNAL_RESET_DRIVE field for RTD3
	fieldname = "LW_PGC6_SCI_FSM_CFG_INTERNAL_RESET_DRIVE";
	if (!(field = reg->FindField(fieldname.c_str())))
	{
	    //ErrPrintf("gc6plus_maxwell: Failed to find %s field\n", fieldname.c_str());
	    ErrPrintf("%s: Failed to find %s field\n", __FUNCTION__, fieldname.c_str()); 
	    return (1);
	}
	valuename = "LW_PGC6_SCI_FSM_CFG_INTERNAL_RESET_DRIVE_ENABLE";
	if (!(value = field->FindValue(valuename.c_str())))
	{
	    ErrPrintf("%s: Failed to find %s value\n", __FUNCTION__, valuename.c_str());
	    return (1);
	}
	//InfoPrintf("gc6plus_maxwell: RTD3 internal reset drive enabled");
	InfoPrintf("%s: RTD3 internal reset drive enabled\n", __FUNCTION__); 
	reg_data |= value->GetValue() << field->GetStartBit();
    }

    if(this->mode==RTD3){
        //enable RTD3 reset monitor register
        fieldname = "LW_PGC6_SCI_FSM_CFG_RTD3_RESET_MON";
        if (!(field = reg->FindField(fieldname.c_str())))
        {
            //ErrPrintf("gc6plus_maxwell: Failed to find %s field\n", fieldname.c_str());
            ErrPrintf("%s: Failed to find %s field\n", __FUNCTION__, fieldname.c_str()); 
            return (1);
        }
        valuename = "LW_PGC6_SCI_FSM_CFG_RTD3_RESET_MON_ENABLE";
        if (!(value = field->FindValue(valuename.c_str())))
        {
            ErrPrintf("%s: Failed to find %s value\n", __FUNCTION__, valuename.c_str());
            return (1);
        }
        //InfoPrintf("gc6plus_maxwell: RTD3 reset monitor enabled");
        InfoPrintf("%s: RTD3 reset monitor enabled \n", __FUNCTION__); 
        reg_data |= value->GetValue() << field->GetStartBit();

        //configure pch timer values for RTD3
        EscapeWrite("pch_timer_rtd3_entry_value", 0, 10 , 50); // Increasing to fit in Clean SW fx for L2 entry - bug 2544459 
        EscapeWrite("pch_timer_rtd3_entry_factor", 0, 2 , 0);  

        EscapeWrite("pch_timer_rtd3_res_value", 0, 10 , 10); //300- 11*27*1
        EscapeWrite("pch_timer_rtd3_res_factor", 0, 2 , 0);  

        EscapeWrite("pch_timer_gpu_wake_value", 0, 10 , 11); //300- 11*27*1
        EscapeWrite("pch_timer_gpu_wake_factor", 0, 2 , 0);  

        EscapeWrite("pch_timer_pex_assert_value", 0, 10 , 15); //300- 11*27*1
        EscapeWrite("pch_timer_pex_assert_factor", 0, 2 , 0);  

        EscapeWrite("pch_timer_pex_deassert_value", 0, 10 , 50); //300- 11*27*1
        EscapeWrite("pch_timer_pex_deassert_factor", 0, 2 , 0);  

        EscapeWrite("pch_timer_rst_pex_assert_value", 0, 10 , 15); //300- 11*27*1
        EscapeWrite("pch_timer_rst_pex_assert_factor", 0, 2 , 0);  

        EscapeWrite("pch_timer_rst_pex_deassert_value", 0, 10 , 50); //300- 11*27*1
        EscapeWrite("pch_timer_rst_pex_deassert_factor", 0, 2 , 0);  

        // bits[1:0] - 0x3 to slect gpu wake  // bits [3:2] - 0x1 to select timer medium 
        if(rtd3_wakeup_timer_check || rtd3_hpd_test)
        {
            EscapeWrite("gc6plus_pch_stub_mode", 0, 8 , 3);
            EscapeWrite("gc6plus_rtd3_sequence_monitor_mode", 0, 8 , 2);
        }
        // bits[1:0] - 0x1 to slect cpu wake  // bits [3:2] - 0x1 to select timer medium
        else if(rtd3_cpu_wakeup_check && !rtd3_system_reset_check)
        {
            EscapeWrite("gc6plus_pch_stub_mode", 0, 8 , 1);
            EscapeWrite("gc6plus_rtd3_sequence_monitor_mode", 0, 8 , 1);
        }
        else if(rtd3_system_reset_check)
        {
            EscapeWrite("gc6plus_pch_stub_mode", 0, 8 , 2);
            EscapeWrite("gc6plus_rtd3_sequence_monitor_mode", 0, 8 , 3);
            Platform::Delay(10); 
            EscapeWrite("gc6plus_rtd3_sequence_monitor_mode", 0, 8 , 1);
        }
    }

    if(bsi_padctrl_in_gc6_check){
        EscapeWrite("gc6plus_rtd3_sequence_monitor_mode", 0, 8 , 0);
    }
    if(this->fgc6_mode == L2){
	InfoPrintf("FGC6 : enable seq montior for fgc6 L2");
	EscapeWrite("gc6plus_rtd3_sequence_monitor_mode", 0, 8 , 20);
    }
    else if(this->fgc6_mode == L1){
	InfoPrintf("FGC6 : enable seq montior for fgc6 L1");
	EscapeWrite("gc6plus_rtd3_sequence_monitor_mode", 0, 8 , 14);  //FGC6_L1_PROD
    }
    else if (this->fgc6_mode == Deep_L1){ 
	InfoPrintf("FGC6 : enable seq montior for fgc6 DeepL1");
	EscapeWrite("gc6plus_rtd3_sequence_monitor_mode", 0, 8 , 14);  //FGC6_L1_PROD - Trying with Deep L1
    }

    pSubdev->RegWr32(reg_addr, reg_data);
    InfoPrintf("gc6plus_maxwell: Writing %s = 0x%08x\n", reg->GetName(),
	    reg_data);



    // I2C address for Therm
    Find_Register(m_regMap, reg, "LW_PGC6_SCI_GPU_I2CS_CFG");
    reg_addr = reg->GetAddress();
    pSubdev->RegWr32(reg_addr, 0x0f25019e);

    // configure power sequence 
    RETURN_IF_ERR(this->configure_power_sequencer(pSubdev));

    //LDO Initialization
    regname = "LW_PGC6_BSI_ANALOGCTRL";
    if (!(reg = m_regMap->FindRegister(regname.c_str())))
    {
	ErrPrintf("gc6plus_maxwell: Failed to find %s register\n", regname.c_str());
	return (1);
    }
    reg_addr = reg->GetAddress();
    fieldname = "LW_PGC6_BSI_ANALOGCTRL_LWVDDTRIP";
    if (!(field = reg->FindField(fieldname.c_str())))
    {
	ErrPrintf("gc6plus_maxwell: Failed to find %s field\n", fieldname.c_str());
	return (1);
    }
    reg_data = (m_lwvdd_ok_val << field->GetStartBit()) & field->GetMask();
    lwgpu->RegWr32(reg_addr,reg_data);

    regname = "LW_PGC6_SCI_LDO_CNTL";
    if (!(reg = m_regMap->FindRegister(regname.c_str())))
    {
	ErrPrintf("gc6plus_maxwell: Failed to find %s register\n", regname.c_str());
	return (1);
    }
    reg_addr = reg->GetAddress();
    fieldname = "LW_PGC6_SCI_LDO_CNTL_LWVDD_OK_LVL";
    if (!(field = reg->FindField(fieldname.c_str())))
    {
	ErrPrintf("gc6plus_maxwell: Failed to find %s field\n", fieldname.c_str());
	return (1);
    }
    reg_data = (m_lwvdd_ok_val << field->GetStartBit()) & field->GetMask();
    lwgpu->RegWr32(reg_addr,reg_data);

    //configure random offset for BSI trip point. Can be a plus or minus offset. 
    if(enab_rand_lwvdd_ok_offset)
    {
	rand = m_rndStream->RandomUnsigned(0,63);
	rand_signed = rand - 32;
	m_lwvdd_ok_offset = (unsigned int) rand_signed;
    }
    Platform::EscapeWrite("gc6plus_opelwr_stub_lwvdd_ok_offset",0, 8, m_lwvdd_ok_offset);

    // configure gpios
    this->configure_gpios(pSubdev);

    //Enable SW status registers
    this->assertSwEnables(pSubdev);

    this->ramInit(pSubdev, bsiRamFileName);

    // Write the RAM valid field in the BSI_CTRL and enable BSI to send BSI2SCI_event
    Find_Register(m_regMap, reg, "LW_PGC6_BSI_CTRL");
    reg_addr = reg->GetAddress();
    reg_data = pSubdev->RegRd32(reg_addr);
    reg_data = ModifyFieldVal(reg.get(), reg_data, "LW_PGC6_BSI_CTRL", "_BSI2SCI_CLKREQ", "_ENABLE");
    reg_data = ModifyFieldVal(reg.get(), reg_data, "LW_PGC6_BSI_CTRL", "_BSI2SCI_RXSTAT", "_DISABLE");
    reg_data = ModifyFieldVal(reg.get(), reg_data, "LW_PGC6_BSI_CTRL", "_RAMVALID", "_TRUE");
    pSubdev->RegWr32(reg_addr, reg_data);

    if(clkreq_in_gc6_check == 1)
    {
	regname =  "LW_PGC6_BSI_XTAL_DOMAIN_FEATURES";
	if (!(reg = m_regMap->FindRegister(regname.c_str())))
	{
	    ErrPrintf("gc6plus_maxwell: Failed to find %s register\n", regname.c_str());
	    return (1);
	}
	reg_addr = reg->GetAddress();
	fieldname = "LW_PGC6_BSI_XTAL_DOMAIN_FEATURES_CLKREQ_IN_GC6";
	if (!(field = reg->FindField(fieldname.c_str())))
	{
	    ErrPrintf("gc6plus_maxwell: Failed to find %s field\n", fieldname.c_str());
	    return (1);
	}
	valuename = "LW_PGC6_BSI_XTAL_DOMAIN_FEATURES_CLKREQ_IN_GC6_ENABLE";
	if (!(value = field->FindValue(valuename.c_str())))
	{
	    ErrPrintf("gc6plus_maxwell: Failed to find %s value\n", valuename.c_str());
	    return (1);
	}
	reg_data = value->GetValue() << field->GetStartBit();
	lwgpu->RegWr32(reg_addr,reg_data);
    }

    //Program BSI clk delay to be max possible - to match value in arch tests
    InfoPrintf("Writing to LW_PGC6_SCI_SLCG\n");
    Find_Register(m_regMap, reg, "LW_PGC6_SCI_SLCG");
    reg_addr = reg->GetAddress();
    reg_data=0x24;
    pSubdev->RegWr32(reg_addr, reg_data);


    if (bsi_padctrl_in_gc6_check) ///bug 200411598 BSI_PAD_CTRL- per pad checks
    {
	InfoPrintf("Writing to BSI_XTAL_DOMAIN_FEATURES");
	Find_Register(m_regMap, reg, "LW_PGC6_BSI_XTAL_DOMAIN_FEATURES");
	reg_addr = reg->GetAddress();
	reg_data = pSubdev->RegRd32(reg_addr);
	reg_data = ModifyFieldVal(reg.get(), reg_data, "LW_PGC6_BSI_XTAL_DOMAIN_FEATURES", "_PER_PADCTRL_OVERRIDE", "_ENABLE");
	pSubdev->RegWr32(reg_addr, reg_data);
	Platform::Delay(1); 
	
	InfoPrintf("gc6plus_maxwell:Writing to BSI_PADCTRL register- overrides\n");
	
	Find_Register(m_regMap, reg, "LW_PGC6_BSI_PADCTRL");
	reg_addr = reg->GetAddress();
	reg_data = pSubdev->RegRd32(reg_addr);
	reg_data = ModifyFieldVal(reg.get(), reg_data, "LW_PGC6_BSI_PADCTRL", "_OVERRIDE_PADSUSPEND", "_ENABLE");
	reg_data = ModifyFieldVal(reg.get(), reg_data, "LW_PGC6_BSI_PADCTRL", "_OVERRIDE_PEXLPMODE", "_ENABLE");
	reg_data = ModifyFieldVal(reg.get(), reg_data, "LW_PGC6_BSI_PADCTRL", "_OVERRIDE_CLKREQEN", "_ENABLE");
	reg_data = ModifyFieldVal(reg.get(), reg_data, "LW_PGC6_BSI_PADCTRL", "_OVERRIDE_GC5MODE", "_ENABLE");
	reg_data = ModifyFieldVal(reg.get(), reg_data, "LW_PGC6_BSI_PADCTRL", "_OVERRIDE_ALT_CTL_SEL", "_ENABLE");
	reg_data = ModifyFieldVal(reg.get(), reg_data, "LW_PGC6_BSI_PADCTRL", "_OVERRIDE_WAKE_CORE_CLMP", "_ENABLE");
	reg_data = ModifyFieldVal(reg.get(), reg_data, "LW_PGC6_BSI_PADCTRL", "_OVERRIDE_WAKE_ALT_CTL_SEL", "_ENABLE");
	pSubdev->RegWr32(reg_addr, reg_data);
    }
	Platform::Delay(1); 
      
    if (bsi_padctrl_in_gc6_check) ///bug 200411598 BSI_PAD_CTRL- per pad checks
    {
	InfoPrintf("gc6plus_maxwell:Writing to BSI_PADCTRL register- enables\n");
	Find_Register(m_regMap, reg, "LW_PGC6_BSI_PADCTRL");
	reg_addr = reg->GetAddress();
	reg_data = pSubdev->RegRd32(reg_addr);
	reg_data = ModifyFieldVal(reg.get(), reg_data, "LW_PGC6_BSI_PADCTRL", "_PEXLPMODE", "_ENABLE");
	reg_data = ModifyFieldVal(reg.get(), reg_data, "LW_PGC6_BSI_PADCTRL", "_CLKREQEN", "_ENABLE");
	reg_data = ModifyFieldVal(reg.get(), reg_data, "LW_PGC6_BSI_PADCTRL", "_GC5MODE", "_DISABLE");
	reg_data = ModifyFieldVal(reg.get(), reg_data, "LW_PGC6_BSI_PADCTRL", "_ALT_CTL_SEL", "_ENABLE");
	reg_data = ModifyFieldVal(reg.get(), reg_data, "LW_PGC6_BSI_PADCTRL", "_WAKE_CORE_CLMP", "_ENABLE");
	reg_data = ModifyFieldVal(reg.get(), reg_data, "LW_PGC6_BSI_PADCTRL", "_WAKE_ALT_CTL_SEL", "_ENABLE");
	pSubdev->RegWr32(reg_addr, reg_data);
	
	Platform::Delay(1); 
	
	Find_Register(m_regMap, reg, "LW_PGC6_BSI_PADCTRL");
	reg_addr = reg->GetAddress();
	reg_data = pSubdev->RegRd32(reg_addr);
	reg_data = ModifyFieldVal(reg.get(), reg_data, "LW_PGC6_BSI_PADCTRL", "_PADSUSPEND", "_ENABLE");
	pSubdev->RegWr32(reg_addr, reg_data);
    }

    return (0);
}

int GC6plus_Maxwell::priv_init(GpuSubdevice *pSubdev){
    unique_ptr<IRegisterClass> reg;
    UINT32 reg_addr;
    UINT32 reg_data;

    DebugPrintf("priv_init(): starting\n");
    //check clamp_en and sleep signal to make sure that when the fuse is not blown
    //bool fuse_blown = false;
    //if(check_clamp_gc6plus(fuse_blown) == 1) return 1;
  
    //Step -3: Clear PMU in Reset
    Find_Register(m_regMap, reg, "LW_PMC_ENABLE");
    DebugPrintf("priv_init(): Find register for PMC_ENABLE is done.\n");
    reg_addr = reg->GetAddress();
    DebugPrintf("priv_init(): Get Address for PMC_ENABLE is done.\n");
    reg_data = pSubdev->RegRd32(reg_addr);
    DebugPrintf("priv_init(): Read for PMC_ENABLE is done.\n");
    pSubdev->RegWr32(reg_addr, reg_data | 0x0000002c ); // enable BUF_reset, xbar, l2, priv_ring
    pSubdev->RegRd32(reg_addr);

    Find_Register(m_regMap, reg, "LW_PPRIV_MASTER_RING_COMMAND_DATA");
    DebugPrintf("priv_init(): Find register for LW_PPRIV_MASTER_RING_COMMAND_DATA is done.\n");
    reg_addr = reg->GetAddress();
    DebugPrintf("priv_init(): Get Address for LW_PPRIV_MASTER_RING_COMMAND_DATA is done.\n");
    pSubdev->RegWr32(reg_addr, 0x503B5443);
    DebugPrintf("priv_init(): Write for LW_PPRIV_MASTER_RING_COMMAND_DATA is done.\n");
    
    PrivInit();
  
    Find_Register(m_regMap, reg, "LW_PMC_ENABLE");
    reg_addr = reg->GetAddress();
    reg_data = pSubdev->RegRd32(reg_addr);
    pSubdev->RegWr32(reg_addr, reg_data | 0xffffffff ); //Enable all engines after PRIV init is complete.
    pSubdev->RegRd32(reg_addr);
    DebugPrintf("gc6plus_maxwell: priv_init(): finished\n");

    return (0);
}

int GC6plus_Maxwell::init_ptimer(GpuSubdevice *pSubdev){
    unique_ptr<IRegisterClass> reg;
    unique_ptr<IRegisterField> field;
    UINT32 reg_addr;
    UINT32 reg_data;
    int err = 0;
    // Write the configuration and some random value into the PTIMER_TIME
    // Stop the PTIMER when configuring it (else subsequent readback will miscompare)
    WriteRegFieldVal(pSubdev, m_regMap, "LW_PGC6_SCI_PTIMER_ENABLE", "_CTL", "_NO");

    
    //InfoPrintf("gc6plus_maxwell_gc6plus_init: Wiring LW_PGC6_BSI_SPARE with 0x%08x \n", reg_data);
    ReadWriteRegFieldVal(pSubdev, m_regMap, "LW_PGC6_SCI_PTIMER_CFG", "_DENOMINATOR", "_27MHZ_REF");
    ReadWriteRegFieldVal(pSubdev, m_regMap, "LW_PGC6_SCI_PTIMER_CFG", "_DENOMINATOR", "_27MHZ_REF");
    ReadWriteRegFieldVal(pSubdev, m_regMap, "LW_PGC6_SCI_PTIMER_CFG", "_INTEGER", "_27MHZ_REF");
   
    if (!(reg = m_regMap->FindRegister("LW_PGC6_SCI_PTIMER_TIME_1"))) {
        ErrPrintf("gc6plus_init: register LW_PGC6_SCI_PTIMER_TIME_1 is not defined!\n");
        err++;
        return (1);
    }
    if (!(field = reg->FindField("LW_PGC6_SCI_PTIMER_TIME_1_NSEC"))) {
        ErrPrintf("gc6plus_init: field LW_PGC6_SCI_PTIMER_TIME_1_NSEC is not defined!\n");
        err++;
        return (1);
    }
    reg_addr = reg->GetAddress();
    reg_data = 0x01234567 << (field->GetStartBit());
    pSubdev->RegWr32(reg_addr, reg_data);
    InfoPrintf("%s: Wiring LW_PGC6_SCI_PTIMER_TIME_1_NSEC with 0x%08x \n",__FUNCTION__, reg_data);

    if (!(reg = m_regMap->FindRegister("LW_PGC6_SCI_PTIMER_TIME_0"))) {
        ErrPrintf("%s: register LW_PGC6_SCI_PTIMER_TIME_0 is not defined!\n",__FUNCTION__);
        err++;
        return (1);
    }
    if (!(field = reg->FindField("LW_PGC6_SCI_PTIMER_TIME_0_NSEC"))) {
        ErrPrintf("%s: field LW_PGC6_SCI_PTIMER_TIME_0_NSEC is not defined!\n",__FUNCTION__);
        err++;
        return (1);
    }
    reg_addr = reg->GetAddress();
    reg_data = 0x89abcdef << (field->GetStartBit());
    pSubdev->RegWr32(reg_addr, reg_data);
    InfoPrintf("%s: Wiring LW_PGC6_SCI_PTIMER_TIME_0_NSEC with 0x%08x \n",__FUNCTION__, reg_data);
    
    ReadWriteRegFieldVal(pSubdev, m_regMap, "LW_PGC6_SCI_PTIMER_TIME_0", "_UPDATE", "_TRIGGER");
    
    // Read back PTIMER_TIME to make sure everything is okay (PTIMER has not been enabled yet)
    ReadWriteRegFieldVal(pSubdev, m_regMap, "LW_PGC6_SCI_PTIMER_READ", "_SAMPLE", "_TRIGGER");
    
    Find_Register(m_regMap, reg, "LW_PGC6_SCI_PTIMER_TIME_0");
    reg_addr = reg->GetAddress();
    pSubdev->RegRd32(reg_addr);
    Find_Register(m_regMap, reg, "LW_PGC6_SCI_PTIMER_TIME_1");
    reg_addr = reg->GetAddress();
    pSubdev->RegRd32(reg_addr);
    
    // Start the PTIMER
    ReadWriteRegFieldVal(pSubdev, m_regMap, "LW_PGC6_SCI_PTIMER_ENABLE", "_CTL", "_YES");
    DebugPrintf("init_ptimer done\n");

    return (0);
}

int GC6plus_Maxwell::configure_power_sequencer(GpuSubdevice *pSubdev){
    unique_ptr<IRegisterClass> reg;
    unique_ptr<IRegisterField> field;
    unique_ptr<IRegisterValue> value;
    string regname, fieldname, valuename;
    UINT32 reg_addr;
    UINT32 reg_data;

    //assumption - GPIO xbar has been set up correctly
    this->pSubdev_ = lwgpu->GetGpuSubdevice();

    //Sequence come from included header file generated in <HW_TOT>/ip/power/gc6_island/<version>/defs/public/registers/<litter>/fc_directed_headers
    //Header files are copied into the SW tree
    unsigned int index;
    int seq_index = 0;
    int vid_pwm_period    = 33;   // Pick nice round number
    int vid_pwm_hi_normal = 17;   // we expect to start/end in P8

    // load
    InfoPrintf("gc6plus_maxwell: Loading power sequencer\n");
    if(as2_speedup_check){
        for (index = 0;
            index < sizeof (as2_speedup_golden_sim_gc6plus_seq_default_sdr_pwr_seq_cfg) /
            sizeof (* as2_speedup_golden_sim_gc6plus_seq_default_sdr_pwr_seq_cfg); index++)
        {
            pSubdev->RegWr32(
                as2_speedup_golden_sim_gc6plus_seq_default_sdr_pwr_seq_cfg[index][0],
                as2_speedup_golden_sim_gc6plus_seq_default_sdr_pwr_seq_cfg[index][1]);
            InfoPrintf("gc6plus_maxwell: Writing 0x%08x = 0x%08x\n",
                as2_speedup_golden_sim_gc6plus_seq_default_sdr_pwr_seq_cfg[index][0],
                as2_speedup_golden_sim_gc6plus_seq_default_sdr_pwr_seq_cfg[index][1]);
            seq_index++;
        }
    } 
    else if(this->mode==RTD3)
    {
        
     if(is_rtd3_sli)
	{
        for (index = 0;
            index < sizeof (golden_sim_rtd3_sli_default_pwr_seq_cfg) /
            sizeof (* golden_sim_rtd3_sli_default_pwr_seq_cfg); index++)
         {
            pSubdev->RegWr32(
                golden_sim_rtd3_sli_default_pwr_seq_cfg[index][0],
                golden_sim_rtd3_sli_default_pwr_seq_cfg[index][1]);
    		InfoPrintf("%s: RTD3 SLI Writing 0x%08x = 0x%08x \n", 
				__FUNCTION__, 
                golden_sim_rtd3_sli_default_pwr_seq_cfg[index][0],
                golden_sim_rtd3_sli_default_pwr_seq_cfg[index][1]);
            seq_index++;
         }
       }
      
      else 
      {
	
	for (index = 0;
            index < sizeof (golden_sim_rtd3_default_pwr_seq_cfg) /
            sizeof (* golden_sim_rtd3_default_pwr_seq_cfg); index++)
        {
            pSubdev->RegWr32(
                golden_sim_rtd3_default_pwr_seq_cfg[index][0],
                golden_sim_rtd3_default_pwr_seq_cfg[index][1]);
            //InfoPrintf("gc6plus_maxwell: Writing 0x%08x = 0x%08x\n",
    		InfoPrintf("%s: RTD3 Writing 0x%08x = 0x%08x \n", 
				__FUNCTION__, 
                golden_sim_rtd3_default_pwr_seq_cfg[index][0],
                golden_sim_rtd3_default_pwr_seq_cfg[index][1]);
            seq_index++;
        }
      }
    } 
    else if(is_fgc6 && !(this->mode == RTD3) )
    {
        for (index = 0;
            index < sizeof (golden_sim_fgc6_default_pwr_seq_cfg) /
            sizeof (* golden_sim_fgc6_default_pwr_seq_cfg); index++)
        {
            pSubdev->RegWr32(
                golden_sim_fgc6_default_pwr_seq_cfg[index][0],
                golden_sim_fgc6_default_pwr_seq_cfg[index][1]);
            //InfoPrintf("gc6plus_maxwell: Writing 0x%08x = 0x%08x\n",
    		InfoPrintf("%s: FGC6 Writing 0x%08x = 0x%08x \n", 
				__FUNCTION__, 
                golden_sim_fgc6_default_pwr_seq_cfg[index][0],
                golden_sim_fgc6_default_pwr_seq_cfg[index][1]);
            seq_index++;
        }
    } 
    else 
    {
        for (index = 0;
            index < sizeof (golden_sim_gc6plus_seq_default_sdr_fc_directed_pwr_seq_cfg) /
            sizeof (* golden_sim_gc6plus_seq_default_sdr_fc_directed_pwr_seq_cfg); index++)
        {
            pSubdev->RegWr32(
                golden_sim_gc6plus_seq_default_sdr_fc_directed_pwr_seq_cfg[index][0],
                golden_sim_gc6plus_seq_default_sdr_fc_directed_pwr_seq_cfg[index][1]);
            InfoPrintf("gc6plus_maxwell: Writing 0x%08x = 0x%08x\n",
                golden_sim_gc6plus_seq_default_sdr_fc_directed_pwr_seq_cfg[index][0],
                golden_sim_gc6plus_seq_default_sdr_fc_directed_pwr_seq_cfg[index][1]);
            seq_index++;
        }
        
    }    
    //Halt Vector For Debug
    this->halt_vector = seq_index;

    Find_Register(m_regMap, reg, "LW_PGC6_SCI_PWR_SEQ_INST");
    reg_addr = reg->GetAddress(seq_index++);
    reg_data = ModifyFieldVal(reg.get(), 0, "LW_PGC6_SCI_PWR_SEQ_INST", "_OPCODE", "_HALT");

    pSubdev->RegWr32(reg_addr,reg_data);
    InfoPrintf("gc6plus_maxwell: %s : Writing 0x%08x = 0x%08x\n",__FUNCTION__,reg_addr,reg_data);

    // Mimics software call to deassert alt_mux_sel + wait 10 xtal_clk
    // Mimics software call to deassert fb_clamp + wait 10 xtal_clk
    this->sw_pwr_seq_vector = seq_index;
    Find_Register(m_regMap, reg, "LW_PGC6_SCI_PWR_SEQ_INST");
    reg_addr = reg->GetAddress(seq_index++);
    Find_Field(reg, field, "LW_PGC6_SCI_PWR_SEQ_INST_DELAY_INDEX");
    reg_data = 0x1 << (field->GetStartBit());
    reg_data = ModifyFieldVal(reg.get(), reg_data, "LW_PGC6_SCI_PWR_SEQ_INST", "_OPCODE", "_SET_STATE_DELAY");
    reg_data = ModifyFieldVal(reg.get(), reg_data, "LW_PGC6_SCI_PWR_SEQ_INST", "_SEQ_STATE_INDEX", "_ALT_MUX_SEL");
    reg_data = ModifyFieldVal(reg.get(), reg_data, "LW_PGC6_SCI_PWR_SEQ_INST", "_SEQ_STATE_DATA", "_DEASSERT");
    reg_data = ModifyFieldVal(reg.get(), reg_data, "LW_PGC6_SCI_PWR_SEQ_INST", "_SEQ_PWM_UPDATE", "_TRIGGER");
    pSubdev->RegWr32(reg_addr,reg_data);
    InfoPrintf("gc6plus_maxwell: %s : Writing 0x%08x = 0x%08x\n",__FUNCTION__,reg_addr,reg_data);
     
    Find_Register(m_regMap, reg, "LW_PGC6_SCI_PWR_SEQ_INST");
    reg_addr = reg->GetAddress(seq_index++);
    Find_Field(reg, field, "LW_PGC6_SCI_PWR_SEQ_INST_DELAY_INDEX");
    reg_data = 0x1 << (field->GetStartBit());
    reg_data = ModifyFieldVal(reg.get(), reg_data, "LW_PGC6_SCI_PWR_SEQ_INST", "_OPCODE", "_SET_STATE_DELAY");
    reg_data = ModifyFieldVal(reg.get(), reg_data, "LW_PGC6_SCI_PWR_SEQ_INST", "_SEQ_STATE_INDEX", "_FB_CLAMP");
    reg_data = ModifyFieldVal(reg.get(), reg_data, "LW_PGC6_SCI_PWR_SEQ_INST", "_SEQ_STATE_DATA", "_DEASSERT");
    reg_data = ModifyFieldVal(reg.get(), reg_data, "LW_PGC6_SCI_PWR_SEQ_INST", "_SEQ_PWM_UPDATE", "_TRIGGER");
    pSubdev->RegWr32(reg_addr,reg_data);
    InfoPrintf("gc6plus_maxwell: %s : Writing 0x%08x = 0x%08x\n",__FUNCTION__,reg_addr,reg_data);
    
    Find_Register(m_regMap, reg, "LW_PGC6_SCI_PWR_SEQ_INST");
    reg_addr = reg->GetAddress(seq_index++);
    reg_data = ModifyFieldVal(reg.get(), 0 , "LW_PGC6_SCI_PWR_SEQ_INST", "_OPCODE", "_HALT");
    pSubdev->RegWr32(reg_addr,reg_data);
    InfoPrintf("gc6plus_maxwell: %s : Writing 0x%08x = 0x%08x\n",__FUNCTION__,reg_addr,reg_data);

    this->pwr_seq_end_vector = seq_index;


    /*configure PWM_VID for when the sequence starts to take over*/
    //period = 818kHz. 
    //min volt = .51V
    //max volt = 1.19V
    //51.5% duty cycle ~ .86V. This is the default
    // VID with bit spread
    InfoPrintf("gc6plus_maxwell: Programming SCI VID_PWM: period = %d, "
        "hi = %d\n", vid_pwm_period, vid_pwm_hi_normal);


    Find_Register(m_regMap, reg, "LW_PGC6_SCI_VID_CFG0(0)");
    reg_addr = reg->GetAddress();
    Find_Field(reg, field, "LW_PGC6_SCI_VID_CFG0_PERIOD");
    reg_data = vid_pwm_period << (field->GetStartBit());
    reg_data = ModifyFieldVal(reg.get(), reg_data, "LW_PGC6_SCI_VID_CFG0", "_BIT_SPREAD", "_ENABLE");
    reg_data = ModifyFieldVal(reg.get(), reg_data, "LW_PGC6_SCI_VID_CFG0", "_MIN_SPREAD", "_SHORTEST");
    pSubdev->RegWr32(reg_addr,reg_data);
    InfoPrintf("gc6plus_maxwell: %s : Writing 0x%08x = 0x%08x\n",__FUNCTION__,reg_addr,reg_data);
    
    Find_Register(m_regMap, reg, "LW_PGC6_SCI_VID_CFG1(0)");
    reg_addr = reg->GetAddress();
    Find_Field(reg, field, "LW_PGC6_SCI_VID_CFG1_HI");
    reg_data = vid_pwm_hi_normal << (field->GetStartBit());
    reg_data = ModifyFieldVal(reg.get(), reg_data, "LW_PGC6_SCI_VID_CFG1", "_UPDATE", "_TRIGGER");
    pSubdev->RegWr32(reg_addr,reg_data);
    InfoPrintf("gc6plus_maxwell: %s : Writing 0x%08x = 0x%08x\n",__FUNCTION__,reg_addr,reg_data);

    Find_Register(m_regMap, reg, "LW_PGC6_SCI_VID_CFG0(1)");
    reg_addr = reg->GetAddress();
    Find_Field(reg, field, "LW_PGC6_SCI_VID_CFG0_PERIOD");
    reg_data = vid_pwm_period << (field->GetStartBit());
    reg_data = ModifyFieldVal(reg.get(), reg_data, "LW_PGC6_SCI_VID_CFG0", "_BIT_SPREAD", "_ENABLE");
    reg_data = ModifyFieldVal(reg.get(), reg_data, "LW_PGC6_SCI_VID_CFG0", "_MIN_SPREAD", "_SHORTEST");
    pSubdev->RegWr32(reg_addr,reg_data);
    InfoPrintf("gc6plus_maxwell: %s : Writing 0x%08x = 0x%08x\n",__FUNCTION__,reg_addr,reg_data);
    
    Find_Register(m_regMap, reg, "LW_PGC6_SCI_VID_CFG1(1)");
    reg_addr = reg->GetAddress();
    Find_Field(reg, field, "LW_PGC6_SCI_VID_CFG1_HI");
    reg_data = vid_pwm_hi_normal << (field->GetStartBit());
    reg_data = ModifyFieldVal(reg.get(), reg_data, "LW_PGC6_SCI_VID_CFG1", "_UPDATE", "_TRIGGER");
    pSubdev->RegWr32(reg_addr,reg_data);
    InfoPrintf("gc6plus_maxwell: %s : Writing 0x%08x = 0x%08x\n",__FUNCTION__,reg_addr,reg_data);


    // program VID_PWM in THERM
    InfoPrintf("gc6plus_maxwell: Programming THERM VID_PWM: period = %d, "
        "hi = %d\n", vid_pwm_period * 4, vid_pwm_hi_normal * 4);
    regname =  "LW_THERM_VID0_PWM";
    if (!(reg = m_regMap->FindRegister(regname.c_str())))
    {
        ErrPrintf("gc6plus_maxwell: Failed to find %s register\n", regname.c_str());
        return (1);
    }
    reg_addr = reg->GetAddress();
    fieldname = "LW_THERM_VID0_PWM_PERIOD";
    if (!(field = reg->FindField(fieldname.c_str())))
    {
        ErrPrintf("gc6plus_maxwell: Failed to find %s field\n", fieldname.c_str());
        return (1);
    }
    reg_data = (((vid_pwm_period * 4) << field->GetStartBit()) & field->GetMask());
    fieldname = "LW_THERM_VID0_PWM_BIT_SPREAD";
    if (!(field = reg->FindField(fieldname.c_str())))
    {
        ErrPrintf("gc6plus_maxwell: Failed to find %s field\n", fieldname.c_str());
        return (1);
    }
    valuename = "LW_THERM_VID0_PWM_BIT_SPREAD_ENABLE";
    if (!(value = field->FindValue(valuename.c_str())))
    {
        ErrPrintf("gc6plus_maxwell: Failed to find %s value\n", valuename.c_str());
        return (1);
    }
    reg_data |= value->GetValue() << field->GetStartBit();
    fieldname = "LW_THERM_VID0_PWM_MIN_SPREAD";
    if (!(field = reg->FindField(fieldname.c_str())))
    {
        ErrPrintf("gc6plus_maxwell: Failed to find %s field\n", fieldname.c_str());
        return (1);
    }
    valuename = "LW_THERM_VID0_PWM_MIN_SPREAD_SHORTEST";
    if (!(value = field->FindValue(valuename.c_str())))
    {
        ErrPrintf("gc6plus_maxwell: Failed to find %s value\n", valuename.c_str());
        return (1);
    }
    reg_data |= value->GetValue() << field->GetStartBit();
    pSubdev->RegWr32(reg_addr, reg_data);
    InfoPrintf("gc6plus_maxwell: Writing %s = 0x%08x\n", reg->GetName(), reg_data);
    regname = "LW_THERM_VID1_PWM";
    if (!(reg = m_regMap->FindRegister(regname.c_str())))
    {
        ErrPrintf("gc6plus_maxwell: Failed to find %s register\n", regname.c_str());
        return (1);
    }
    reg_addr = reg->GetAddress();
    fieldname = "LW_THERM_VID1_PWM_HI";
    if (!(field = reg->FindField(fieldname.c_str())))
    {
        ErrPrintf("gc6plus_maxwell: Failed to find %s field\n", fieldname.c_str());
        return (1);
    }
    reg_data = (((vid_pwm_hi_normal * 4) << field->GetStartBit()) & field->GetMask());

    pSubdev->RegWr32(reg_addr, reg_data);


    regname = "LW_THERM_VID_PWM_TRIGGER_MASK";
    if (!(reg = m_regMap->FindRegister(regname.c_str())))
    {
        ErrPrintf("gc6plus_maxwell: Failed to find %s register\n", regname.c_str());
        return (1);
    }
    reg_addr = reg->GetAddress();
    fieldname = "LW_THERM_VID_PWM_TRIGGER_MASK_0_SETTING_NEW";
    if (!(field = reg->FindField(fieldname.c_str())))
    {
        ErrPrintf("gc6plus_maxwell: Failed to find %s field\n", fieldname.c_str());
        return (1);
    }
    valuename = "LW_THERM_VID_PWM_TRIGGER_MASK_0_SETTING_NEW_TRIGGER";
    if (!(value = field->FindValue(valuename.c_str())))
    {
        ErrPrintf("gc6plus_maxwell: Failed to find %s value\n", valuename.c_str());
        return (1);
    }
    reg_data = value->GetValue() << field->GetStartBit();
    pSubdev->RegWr32(reg_addr, reg_data);
    InfoPrintf("gc6plus_maxwell: Writing %s = 0x%08x\n", reg->GetName(), reg_data);


    // Initial sequencer state - all power supplies powered up and all clamps deasserted
    Find_Register(m_regMap, reg, "LW_PGC6_SCI_PWR_SEQ_ARMED_SEQ_STATE");
    reg_addr = reg->GetAddress();
    reg_data = ModifyFieldVal(reg.get(), 0, "LW_PGC6_SCI_PWR_SEQ_ARMED_SEQ_STATE", "_FB_CLAMP", "_DEASSERT");
    reg_data = ModifyFieldVal(reg.get(), reg_data, "LW_PGC6_SCI_PWR_SEQ_ARMED_SEQ_STATE", "_XTAL_CORE_CLAMP", "_DEASSERT");
    reg_data = ModifyFieldVal(reg.get(), reg_data, "LW_PGC6_SCI_PWR_SEQ_ARMED_SEQ_STATE", "_PAD_CLAMP", "_DEASSERT");
    reg_data = ModifyFieldVal(reg.get(), reg_data, "LW_PGC6_SCI_PWR_SEQ_ARMED_SEQ_STATE", "_ALT_MUX_SEL", "_DEASSERT");
    reg_data = ModifyFieldVal(reg.get(), reg_data, "LW_PGC6_SCI_PWR_SEQ_ARMED_SEQ_STATE", "_PEX_RST", "_DEASSERT");
    reg_data = ModifyFieldVal(reg.get(), reg_data, "LW_PGC6_SCI_PWR_SEQ_ARMED_SEQ_STATE", "_MAIN_3V3_EN", "_ASSERT");
    reg_data = ModifyFieldVal(reg.get(), reg_data, "LW_PGC6_SCI_PWR_SEQ_ARMED_SEQ_STATE", "_FBVDDQ_GPU_EN", "_DEASSERT");
    reg_data = ModifyFieldVal(reg.get(), reg_data, "LW_PGC6_SCI_PWR_SEQ_ARMED_SEQ_STATE", "_PEXVDD_VID", "_DEASSERT");
    pSubdev->RegWr32(reg_addr,reg_data);
    InfoPrintf("gc6plus_maxwell: %s : Writing 0x%08x = 0x%08x\n",__FUNCTION__,reg_addr,reg_data);
    Find_Register(m_regMap, reg, "LW_PGC6_SCI_PWR_SEQ_STATE");
    reg_addr = reg->GetAddress();
    reg_data = ModifyFieldVal(reg.get(), 0, "LW_PGC6_SCI_PWR_SEQ_STATE", "_UPDATE", "_TRIGGER");
    pSubdev->RegWr32(reg_addr,reg_data);
    InfoPrintf("gc6plus_maxwell: %s : Writing 0x%08x = 0x%08x\n",__FUNCTION__,reg_addr,reg_data);

    return (0);
}



int GC6plus_Maxwell::configure_gpios(GpuSubdevice *pSubdev){
    unique_ptr<IRegisterClass> reg;
    unique_ptr<IRegisterField> field;
    unique_ptr<IRegisterValue> value;
    string regname, fieldname, valuename;
    UINT32 reg_addr;
    UINT32 reg_data;

    Find_Register(m_regMap, reg, "LW_PGC6_SCI_GPIO_INPUT_HPD");
    UINT32 hpd_size, hpd_limit; 
    int HPD_LIMIT;
    // The register array is expected to be 1-dimensional.  Check this and
    // then loop through all the registers in the array.
    if (reg->GetArrayDimensions() != 1)
    {
        ErrPrintf("%s: Register %s array dimension is not 1, dimension = %d\n", __FUNCTION__, reg->GetName(), reg->GetArrayDimensions());
        return (1);
    }
    reg->GetFormula1(hpd_limit, hpd_size);
    InfoPrintf("%s: Found %s array limit = %d\n", __FUNCTION__, reg->GetName(), hpd_limit); 
    HPD_LIMIT = hpd_limit;
    
    InfoPrintf("Configure GPIO: configure_gpios()\n");
    // GPIO Input assignments (0, 1, 15, 19, 21) are pulled down in gpio_engine.vx

    /*FIXME
    GPIO # - Purpose - DCB Function
    0  HPD  HPD(0)
    1 - FB_CLAMP - FB_CLAMP 
    2 - LWVDD_PEXVDD_EN - GENERIC 
    4 - PWM_VID - PWM_SERIAL_VID 
    5 - MAIN_3V3_EN - GENERIC 
    6 - GPU_EVENT - GENERIC 
    7 - FBVDDQ_GPU_EN - GENERIC 
    8 - PEXVDD_VID - GENERIC
    */

    gpio_index_hpd[0]          =  0;//Pulled Down by default
    gpio_index_fb_clamp        =  1;
    gpio_index_lwvdd_pexvdd_en =  2;
    gpio_index_fbvdd_q_mem_en  =  3;
    gpio_index_lwvdd_vid_pwm   =  4;
    gpio_index_msvdd_vid_pwm   =  23;
    gpio_index_main_3v3_en     =  5;
    gpio_index_fsm_gpu_event   =  6;
    gpio_index_fbvddq_gpu_en   =  7;
    if(!is_fgc6) {
    gpio_index_pexvdd_vid      =  25; //8;
    } else {
    gpio_index_pexvdd_vid      =  25; //GPIO[20] will be used for FGC6 feature. 
    //Since GPIO[8] is used for PMGR and GPIO[20] is used as HPD in DIRECTED tests,we are using GPIO[25] for FGC6 Directed tests alone.
    }
 
    gpio_index_hpd[1]          = 10;
    gpio_index_hpd[2]          = 11;
    gpio_index_hpd[3]          = 12;
    gpio_index_hpd[4]          = 13;
    gpio_index_hpd[5]          = 14;
    gpio_index_hpd[6]          = 22;
 
    //Save 16 for smartfan
    gpio_index_power_alert     = 17;
     

    if(map_misc_0_check == 1){
        gpio_index_misc[0]  = misc_0_gpio_pin;
        InfoPrintf("gc6plus_maxwell :configure_gpios():Mapping GPIO pin for misc_0 from testarg -map_misc_0\n"); 
        if(misc_0_gpio_pin == 0){
            gpio_index_hpd[0]  =  18;
        }
        else if(misc_0_gpio_pin == 10){
            gpio_index_hpd[1]  =  18;
        }
        else if(misc_0_gpio_pin == 11){
            gpio_index_hpd[2]  =  18;
        }
        else if(misc_0_gpio_pin == 12){
            gpio_index_hpd[3]  =  18;
        }
        else if(misc_0_gpio_pin == 13){
            gpio_index_hpd[4]  =  18;
        }
        else if(misc_0_gpio_pin == 14){
            gpio_index_hpd[5]  =  18;
        }
        else if(misc_0_gpio_pin == 16){
            gpio_index_misc[0]  =  16;
        }
        else if(misc_0_gpio_pin == 18){
            gpio_index_misc[0] = 18;
        }
        else if(misc_0_gpio_pin == 19){
            gpio_index_misc[1] = 18;
        }
        else if(misc_0_gpio_pin == 20){
            gpio_index_misc[2] = 18;
        }
        else if(misc_0_gpio_pin == 21){
            gpio_index_misc[3] = 18;
        }
        else{
            gpio_index_misc[0]  = 18;
            InfoPrintf("gc6plus_maxwell: configure_gpios(): Only pins 0, 10-14,16,18-21 allowed for misc_0: using default GPIO pin 18 for misc_0\n");    
        }
    }
    else {
        gpio_index_misc[0]  = 18;
        gpio_index_misc[1]  = 19; //Pulled Down by default
        gpio_index_misc[2]  = 20;
        gpio_index_misc[3]  = 21; //Pulled Down by default
    }
    if( misc_0_gpio_pin != 19){
         gpio_index_misc[1]  = 19; //Pulled Down by default
    }
    if( misc_0_gpio_pin != 20){
         gpio_index_misc[2]  = 20; 
    }
    if( misc_0_gpio_pin != 21){
         gpio_index_misc[3]  = 21; //Pulled Down by default
    }
    gpio_index_gpu_gc6_rst     =  9;
    gpio_index_sys_rst_mon     = 15;

    // only the following are supported by gc6plus_stubs
    EscapeWrite("gc6plus_lwvdd_pexvdd_en_gpio",0, 4,gpio_index_lwvdd_pexvdd_en);
    EscapeWrite("gc6plus_fbvdd_q_mem_en_gpio", 0, 4, gpio_index_fbvdd_q_mem_en);
    EscapeWrite("gc6plus_lwvdd_vid_gpio", 0, 4, gpio_index_lwvdd_vid_pwm);
    EscapeWrite("gc6plus_msvdd_vid_gpio", 0, 4, gpio_index_msvdd_vid_pwm);
    EscapeWrite("gc6plus_main_3v3_en_gpio", 0, 4, gpio_index_main_3v3_en);
    EscapeWrite("gc6plus_gpu_event_gpio", 0, 4, gpio_index_fsm_gpu_event);
    EscapeWrite("gc6plus_fbvddq_gpu_en_gpio", 0, 4, gpio_index_fbvddq_gpu_en);
    EscapeWrite("gc6plus_pexvdd_vid_gpio", 0, 4, gpio_index_pexvdd_vid);
    EscapeWrite("gc6plus_gpu_gc6_rst_gpio", 0, 4, gpio_index_gpu_gc6_rst);
    EscapeWrite("gc6plus_sys_rst_mon_gpio", 0, 4, gpio_index_sys_rst_mon);

    Find_Register(m_regMap, reg, "LW_PGC6_SCI_GPIO_OUTPUT_CNTL");
    reg_addr = reg->GetAddress(gpio_index_fb_clamp);
    reg_data = ModifyFieldVal(reg.get(), 0, "LW_PGC6_SCI_GPIO_OUTPUT_CNTL", "_SEL", "_SEQ_STATE_FB_CLAMP");
    reg_data = ModifyFieldVal(reg.get(), reg_data, "LW_PGC6_SCI_GPIO_OUTPUT_CNTL", "_DRIVE_MODE", "_OPEN_SOURCE");
    reg_data = ModifyFieldVal(reg.get(), reg_data, "LW_PGC6_SCI_GPIO_OUTPUT_CNTL", "_POLARITY", "_ACTIVE_HIGH");
    pSubdev->RegWr32(reg_addr,reg_data);
    InfoPrintf("gc6plus_maxwell: %s : Writing 0x%08x = 0x%08x\n",__FUNCTION__,reg_addr,reg_data);

    // Configure PMGR GPIO output controls
    // FIXME: there are more GPIOs that need to be configured than
    // just VID_PWM, GPU GC6 reset, and GC5 mode.
    

    //Configuring PMGR GPIO to VID PWM
    InfoPrintf("gc6plus_maxwell: Configuring PMGR GPIO %d for VID_PWM\n", gpio_index_msvdd_vid_pwm);
    regname = Utility::StrPrintf("LW_GPIO_OUTPUT_CNTL");
    if (regname.c_str() < 0)
    {
        ErrPrintf("gc6plus_maxwell: Failed to generate GPIO config regname\n");
        return (1);
    }
    
    if (!(reg = m_regMap->FindRegister(regname.c_str())))
    {
        ErrPrintf("gc6plus_maxwell: Failed to find %s register\n", regname.c_str());
        return (1);
    }
    
    reg_addr = reg->GetAddress(gpio_index_msvdd_vid_pwm);
    fieldname =  "LW_GPIO_OUTPUT_CNTL_SEL";
    if (!(field = reg->FindField(fieldname.c_str())))
    {
        ErrPrintf("gc6plus_maxwell: Failed to find %s field\n", fieldname.c_str());
        return (1);
    }

    InfoPrintf("gc6plus_maxwell: Setting PMGR GPIO %d SEL field to VID PWM \n",gpio_index_msvdd_vid_pwm);
    valuename = "LW_GPIO_OUTPUT_CNTL_SEL_VID_PWM"; 
    if (!(value = field->FindValue(valuename.c_str())))
    {
   	    ErrPrintf("gc6plus_maxwell: Failed to find %s value\n", valuename.c_str());
   	    return (1);
   	}
    reg_data = value->GetValue() << field->GetStartBit(); 
    pSubdev->RegWr32(reg_addr, reg_data);
    InfoPrintf("gc6plus_maxwell: Writing %s = 0x%08x\n", reg->GetName(),reg_data);

    //Configuring PMGR GPIO for GC5 MODE(GC5_MODE (PEXVDD_VID) - force de-assert)
    InfoPrintf("gc6plus_maxwell: Configuring PMGR GPIO %d for GC5_MODE\n",gpio_index_pexvdd_vid);
    regname = Utility::StrPrintf("LW_GPIO_OUTPUT_CNTL");
    if (regname.c_str() < 0)
    {
        ErrPrintf("gc6plus_maxwell: Failed to generate GPIO config regname\n");
        return (1);
    }
    
    if (!(reg = m_regMap->FindRegister(regname.c_str())))
    {
        ErrPrintf("gc6plus_maxwell: Failed to find %s register\n", regname.c_str());
        return (1);
    }
    
    reg_addr = reg->GetAddress(gpio_index_pexvdd_vid);
    fieldname =  "LW_GPIO_OUTPUT_CNTL_SEL";
    if (!(field = reg->FindField(fieldname.c_str())))
    {
        ErrPrintf("gc6plus_maxwell: Failed to find %s field\n", fieldname.c_str());
        return (1);
    }

    InfoPrintf("gc6plus_maxwell: Setting PMGR GPIO %d SEL field to NORMAL \n",gpio_index_pexvdd_vid);
    valuename = "LW_GPIO_OUTPUT_CNTL_SEL_NORMAL"; 
    if (!(value = field->FindValue(valuename.c_str())))
    {
   	    ErrPrintf("gc6plus_maxwell: Failed to find %s value\n", valuename.c_str());
   	    return (1);
   	}
    reg_data = value->GetValue() << field->GetStartBit(); 
        
    fieldname =  "LW_GPIO_OUTPUT_CNTL_IO_OUT_EN";
    if (!(field = reg->FindField(fieldname.c_str())))
    {
        ErrPrintf("gc6plus_maxwell: Failed to find %s field\n", fieldname.c_str());
        return (1);
    }

    InfoPrintf("gc6plus_maxwell: Setting PMGR GPIO %d OUT EN field to YES \n",gpio_index_pexvdd_vid);
    valuename = "LW_GPIO_OUTPUT_CNTL_IO_OUT_EN_YES"; 
    if (!(value = field->FindValue(valuename.c_str())))
    {
   	    ErrPrintf("gc6plus_maxwell: Failed to find %s value\n", valuename.c_str());
   	    return (1);
   	}
    reg_data |= value->GetValue() << field->GetStartBit(); 

    fieldname =  "LW_GPIO_OUTPUT_CNTL_IO_OUTPUT";
    if (!(field = reg->FindField(fieldname.c_str())))
    {
        ErrPrintf("gc6plus_maxwell: Failed to find %s field\n", fieldname.c_str());
        return (1);
    }

    InfoPrintf("gc6plus_maxwell: Setting PMGR GPIO %d IO OUPUT field to 0 \n",gpio_index_pexvdd_vid);
    valuename = "LW_GPIO_OUTPUT_CNTL_IO_OUTPUT_0"; 
    if (!(value = field->FindValue(valuename.c_str())))
    {
   	    ErrPrintf("gc6plus_maxwell: Failed to find %s value\n", valuename.c_str());
   	    return (1);
   	}
    reg_data |= value->GetValue() << field->GetStartBit(); 
    pSubdev->RegWr32(reg_addr, reg_data);
    InfoPrintf("gc6plus_maxwell: Writing %s = 0x%08x\n", reg->GetName(),reg_data);

    //Configuring PMGR GPIO for for GPU GC6 reset(PEX_RST_HOLD)
    InfoPrintf("gc6plus_maxwell: Configuring PMGR GPIO %d for GPU GC6 reset\n",gpio_index_gpu_gc6_rst);
    regname = Utility::StrPrintf("LW_GPIO_OUTPUT_CNTL");
    if (regname.c_str() < 0)
    {
        ErrPrintf("gc6plus_maxwell: Failed to generate GPIO config regname\n");
        return (1);
    }
    
    if (!(reg = m_regMap->FindRegister(regname.c_str())))
    {
        ErrPrintf("gc6plus_maxwell: Failed to find %s register\n", regname.c_str());
        return (1);
    }
    
    reg_addr = reg->GetAddress(gpio_index_gpu_gc6_rst);
    fieldname =  "LW_GPIO_OUTPUT_CNTL_SEL";
    if (!(field = reg->FindField(fieldname.c_str())))
    {
        ErrPrintf("gc6plus_maxwell: Failed to find %s field\n", fieldname.c_str());
        return (1);
    }

    InfoPrintf("gc6plus_maxwell: Setting PMGR GPIO %d SEL field to NORMAL \n",gpio_index_gpu_gc6_rst);
    valuename = "LW_GPIO_OUTPUT_CNTL_SEL_NORMAL"; 
    if (!(value = field->FindValue(valuename.c_str())))
    {
   	    ErrPrintf("gc6plus_maxwell: Failed to find %s value\n", valuename.c_str());
   	    return (1);
   	}
    reg_data = value->GetValue() << field->GetStartBit(); 
        
    fieldname =  "LW_GPIO_OUTPUT_CNTL_IO_OUT_EN";
    if (!(field = reg->FindField(fieldname.c_str())))
    {
        ErrPrintf("gc6plus_maxwell: Failed to find %s field\n", fieldname.c_str());
        return (1);
    }

    InfoPrintf("gc6plus_maxwell: Setting PMGR GPIO %d OUT EN field to NO \n",gpio_index_gpu_gc6_rst);
    valuename = "LW_GPIO_OUTPUT_CNTL_IO_OUT_EN_NO"; 
    if (!(value = field->FindValue(valuename.c_str())))
    {
   	    ErrPrintf("gc6plus_maxwell: Failed to find %s value\n", valuename.c_str());
   	    return (1);
   	}
    reg_data |= value->GetValue() << field->GetStartBit(); 
    pSubdev->RegWr32(reg_addr, reg_data);
    InfoPrintf("gc6plus_maxwell: Writing %s = 0x%08x\n", reg->GetName(),reg_data);

    
    //Configuring PMGR GPIO for GC6 reset(FBVDDQ_GPU_EN)
    InfoPrintf("gc6plus_maxwell: Configuring PMGR GPIO %d for GC6 reset\n",gpio_index_fbvddq_gpu_en);
    regname = Utility::StrPrintf("LW_GPIO_OUTPUT_CNTL");
    if (regname.c_str() < 0)
    {
        ErrPrintf("gc6plus_maxwell: Failed to generate GPIO config regname\n");
        return (1);
    }
    
    if (!(reg = m_regMap->FindRegister(regname.c_str())))
    {
        ErrPrintf("gc6plus_maxwell: Failed to find %s register\n", regname.c_str());
        return (1);
    }
    
    reg_addr = reg->GetAddress(gpio_index_fbvddq_gpu_en);
    fieldname =  "LW_GPIO_OUTPUT_CNTL_SEL";
    if (!(field = reg->FindField(fieldname.c_str())))
    {
        ErrPrintf("gc6plus_maxwell: Failed to find %s field\n", fieldname.c_str());
        return (1);
    }

    InfoPrintf("gc6plus_maxwell: Setting PMGR GPIO %d SEL field to NORMAL \n",gpio_index_fbvddq_gpu_en);
    valuename = "LW_GPIO_OUTPUT_CNTL_SEL_NORMAL"; 
    if (!(value = field->FindValue(valuename.c_str())))
    {
   	    ErrPrintf("gc6plus_maxwell: Failed to find %s value\n", valuename.c_str());
   	    return (1);
   	}
    reg_data = value->GetValue() << field->GetStartBit(); 
        
    fieldname =  "LW_GPIO_OUTPUT_CNTL_IO_OUT_EN";
    if (!(field = reg->FindField(fieldname.c_str())))
    {
        ErrPrintf("gc6plus_maxwell: Failed to find %s field\n", fieldname.c_str());
        return (1);
    }

    InfoPrintf("gc6plus_maxwell: Setting PMGR GPIO %d OUT EN field to YES \n",gpio_index_fbvddq_gpu_en);
    valuename = "LW_GPIO_OUTPUT_CNTL_IO_OUT_EN_YES"; 
    if (!(value = field->FindValue(valuename.c_str())))
    {
   	    ErrPrintf("gc6plus_maxwell: Failed to find %s value\n", valuename.c_str());
   	    return (1);
   	}
    reg_data |= value->GetValue() << field->GetStartBit(); 

    fieldname =  "LW_GPIO_OUTPUT_CNTL_IO_OUTPUT";
    if (!(field = reg->FindField(fieldname.c_str())))
    {
        ErrPrintf("gc6plus_maxwell: Failed to find %s field\n", fieldname.c_str());
        return (1);
    }

    InfoPrintf("gc6plus_maxwell: Setting PMGR GPIO %d IO OUPUT field to 0 \n",gpio_index_fbvddq_gpu_en);
    valuename = "LW_GPIO_OUTPUT_CNTL_IO_OUTPUT_0"; 
    if (!(value = field->FindValue(valuename.c_str())))
    {
   	    ErrPrintf("gc6plus_maxwell: Failed to find %s value\n", valuename.c_str());
   	    return (1);
   	}
    reg_data |= value->GetValue() << field->GetStartBit(); 

    fieldname =  "LW_GPIO_OUTPUT_CNTL_PULLUD";
    if (!(field = reg->FindField(fieldname.c_str())))
    {
        ErrPrintf("gc6plus_maxwell: Failed to find %s field\n", fieldname.c_str());
        return (1);
    }

    InfoPrintf("gc6plus_maxwell: Setting PMGR GPIO %d PULLUD to NONE \n",gpio_index_fbvddq_gpu_en);
    valuename = "LW_GPIO_OUTPUT_CNTL_PULLUD_NONE"; 
    if (!(value = field->FindValue(valuename.c_str())))
    {
   	    ErrPrintf("gc6plus_maxwell: Failed to find %s value\n", valuename.c_str());
   	    return (1);
   	}
    reg_data |= value->GetValue() << field->GetStartBit(); 

    fieldname =  "LW_GPIO_OUTPUT_CNTL_OPEN_DRAIN";
    if (!(field = reg->FindField(fieldname.c_str())))
    {
        ErrPrintf("gc6plus_maxwell: Failed to find %s field\n", fieldname.c_str());
        return (1);
    }

    InfoPrintf("gc6plus_maxwell: Setting PMGR GPIO %d OPEN DRAIN MODE is Disabled \n",gpio_index_fbvddq_gpu_en);
    valuename = "LW_GPIO_OUTPUT_CNTL_OPEN_DRAIN_DISABLE"; 
    if (!(value = field->FindValue(valuename.c_str())))
    {
   	    ErrPrintf("gc6plus_maxwell: Failed to find %s value\n", valuename.c_str());
   	    return (1);
   	}
    reg_data |= value->GetValue() << field->GetStartBit(); 
    pSubdev->RegWr32(reg_addr, reg_data);
    InfoPrintf("gc6plus_maxwell: Writing %s = 0x%08x\n", reg->GetName(),reg_data);


    // Trigger PMGR GPIO update after all PMGR GPIOs are configured
    InfoPrintf("gc6plus_maxwell: Triggering PMGR GPIO update\n");
    regname = "LW_PMGR_GPIO_OUTPUT_CNTL_TRIGGER";
    if (regname.c_str() < 0)
    {
        ErrPrintf("gc6plus_maxwell: Failed to generate GPIO config regname\n");
        return (1);
    }

    if (!(reg = m_regMap->FindRegister(regname.c_str())))
    {
        ErrPrintf("gc6plus_maxwell: Failed to find %s register\n", regname.c_str());
        return (1);
    }
    reg_addr = reg->GetAddress();
    fieldname = "LW_PMGR_GPIO_OUTPUT_CNTL_TRIGGER_UPDATE";
    if (!(field = reg->FindField(fieldname.c_str())))
    {
        ErrPrintf("gc6plus_maxwell: Failed to find %s field\n", fieldname.c_str());
        return (1);
    }
    valuename = "LW_PMGR_GPIO_OUTPUT_CNTL_TRIGGER_UPDATE_TRIGGER";
    if (!(value = field->FindValue(valuename.c_str())))
    {
        ErrPrintf("gc6plus_maxwell: Failed to find %s value\n", valuename.c_str());
        return (1);
    }
    reg_data = value->GetValue() << field->GetStartBit();
    pSubdev->RegWr32(reg_addr, reg_data);
    InfoPrintf("gc6plus_maxwell: Writing %s = 0x%08x\n", reg->GetName(),
        reg_data);

    // Now we configure SCI GPIO outputs
    Find_Register(m_regMap, reg, "LW_PGC6_SCI_GPIO_OUTPUT_CNTL");
    reg_addr = reg->GetAddress(gpio_index_msvdd_vid_pwm);
    reg_data = ModifyFieldVal(reg.get(), 0, "LW_PGC6_SCI_GPIO_OUTPUT_CNTL", "_SEL", "_VID_PWM_0");
    reg_data = ModifyFieldVal(reg.get(), reg_data, "LW_PGC6_SCI_GPIO_OUTPUT_CNTL", "_DRIVE_MODE", "_FULL_DRIVE");
    reg_data = ModifyFieldVal(reg.get(), reg_data, "LW_PGC6_SCI_GPIO_OUTPUT_CNTL", "_POLARITY", "_ACTIVE_HIGH");
    pSubdev->RegWr32(reg_addr,reg_data);
    InfoPrintf("gc6plus_maxwell: %s : Writing 0x%08x = 0x%08x\n",__FUNCTION__,reg_addr,reg_data);

    Find_Register(m_regMap, reg, "LW_PGC6_SCI_GPIO_OUTPUT_CNTL");
    reg_addr = reg->GetAddress(gpio_index_lwvdd_vid_pwm);
    reg_data = ModifyFieldVal(reg.get(), 0, "LW_PGC6_SCI_GPIO_OUTPUT_CNTL", "_SEL", "_VID_PWM_1");
    reg_data = ModifyFieldVal(reg.get(), reg_data, "LW_PGC6_SCI_GPIO_OUTPUT_CNTL", "_DRIVE_MODE", "_FULL_DRIVE");
    reg_data = ModifyFieldVal(reg.get(), reg_data, "LW_PGC6_SCI_GPIO_OUTPUT_CNTL", "_POLARITY", "_ACTIVE_HIGH");
    pSubdev->RegWr32(reg_addr,reg_data);
    InfoPrintf("gc6plus_maxwell: %s : Writing 0x%08x = 0x%08x\n",__FUNCTION__,reg_addr,reg_data);
    
    Find_Register(m_regMap, reg, "LW_PGC6_SCI_GPIO_OUTPUT_CNTL");
    reg_addr = reg->GetAddress(gpio_index_main_3v3_en);
    reg_data = ModifyFieldVal(reg.get(), 0, "LW_PGC6_SCI_GPIO_OUTPUT_CNTL", "_SEL", "_SEQ_STATE_MAIN_3V3_EN");
    reg_data = ModifyFieldVal(reg.get(), reg_data, "LW_PGC6_SCI_GPIO_OUTPUT_CNTL", "_DRIVE_MODE", "_OPEN_DRAIN");
    reg_data = ModifyFieldVal(reg.get(), reg_data, "LW_PGC6_SCI_GPIO_OUTPUT_CNTL", "_POLARITY", "_ACTIVE_HIGH");
    pSubdev->RegWr32(reg_addr,reg_data);
    InfoPrintf("gc6plus_maxwell: %s : Writing 0x%08x = 0x%08x\n",__FUNCTION__,reg_addr,reg_data);
    
    Find_Register(m_regMap, reg, "LW_PGC6_SCI_GPIO_OUTPUT_CNTL");
    reg_addr = reg->GetAddress(gpio_index_fsm_gpu_event);
    reg_data = ModifyFieldVal(reg.get(), 0, "LW_PGC6_SCI_GPIO_OUTPUT_CNTL", "_SEL", "_FSM_GPU_EVENT");
    reg_data = ModifyFieldVal(reg.get(), reg_data, "LW_PGC6_SCI_GPIO_OUTPUT_CNTL", "_DRIVE_MODE", "_OPEN_DRAIN");
    reg_data = ModifyFieldVal(reg.get(), reg_data, "LW_PGC6_SCI_GPIO_OUTPUT_CNTL", "_POLARITY", "_ACTIVE_LOW");
    pSubdev->RegWr32(reg_addr,reg_data);
    InfoPrintf("gc6plus_maxwell: %s : Writing 0x%08x = 0x%08x\n",__FUNCTION__,reg_addr,reg_data);
    
    Find_Register(m_regMap, reg, "LW_PGC6_SCI_GPIO_OUTPUT_CNTL");
    reg_addr = reg->GetAddress(gpio_index_fbvddq_gpu_en);
    reg_data = ModifyFieldVal(reg.get(), 0, "LW_PGC6_SCI_GPIO_OUTPUT_CNTL", "_SEL", "_SEQ_STATE_FBVDDQ_GPU_EN");
    reg_data = ModifyFieldVal(reg.get(), reg_data, "LW_PGC6_SCI_GPIO_OUTPUT_CNTL", "_DRIVE_MODE", "_FULL_DRIVE");
    reg_data = ModifyFieldVal(reg.get(), reg_data, "LW_PGC6_SCI_GPIO_OUTPUT_CNTL", "_POLARITY", "_ACTIVE_HIGH");
    pSubdev->RegWr32(reg_addr,reg_data);
    InfoPrintf("gc6plus_maxwell: %s : Writing 0x%08x = 0x%08x\n",__FUNCTION__,reg_addr,reg_data);
    
    Find_Register(m_regMap, reg, "LW_PGC6_SCI_GPIO_OUTPUT_CNTL");
    reg_addr = reg->GetAddress(gpio_index_pexvdd_vid);
    reg_data = ModifyFieldVal(reg.get(), 0, "LW_PGC6_SCI_GPIO_OUTPUT_CNTL", "_SEL", "_SEQ_STATE_PEXVDD_VID");
    reg_data = ModifyFieldVal(reg.get(), reg_data, "LW_PGC6_SCI_GPIO_OUTPUT_CNTL", "_DRIVE_MODE", "_FULL_DRIVE");
    reg_data = ModifyFieldVal(reg.get(), reg_data, "LW_PGC6_SCI_GPIO_OUTPUT_CNTL", "_POLARITY", "_ACTIVE_HIGH");
    pSubdev->RegWr32(reg_addr,reg_data);
    InfoPrintf("gc6plus_maxwell: %s : Writing 0x%08x = 0x%08x\n",__FUNCTION__,reg_addr,reg_data);
    this->setGpioPull(gpio_index_pexvdd_vid, GPIO_PULL_DOWN);
    this->setGpioDrive(gpio_index_pexvdd_vid, GPIO_DRV_NONE);

    // configure GPU self driven reset GPIO before enabling
    InfoPrintf("gc6plus_maxwell: Configuring SCI GPIO %d for GPU GC6 reset\n",
    gpio_index_gpu_gc6_rst);
    regname = Utility::StrPrintf("LW_PGC6_SCI_GPIO_OUTPUT_CNTL");
    if (regname.c_str() < 0)
    {
        ErrPrintf("gc6plus_maxwell: Failed to generate GPIO config regname\n");
        return (1);
    }
    
    if (!(reg = m_regMap->FindRegister(regname.c_str())))
    {
        ErrPrintf("gc6plus_maxwell: Failed to find %s register\n", regname.c_str());
        return (1);
    }
    
    reg_addr = reg->GetAddress(gpio_index_gpu_gc6_rst);
    fieldname =  "LW_PGC6_SCI_GPIO_OUTPUT_CNTL_SEL";
    if (!(field = reg->FindField(fieldname.c_str())))
    {
        ErrPrintf("gc6plus_maxwell: Failed to find %s field\n", fieldname.c_str());
        return (1);
    }

    if(this->mode==RTD3 || is_fgc6)
    {
        
        InfoPrintf("gc6plus_maxwell: Setting SEL to normal \n");
    	valuename = "LW_PGC6_SCI_GPIO_OUTPUT_CNTL_SEL_NORMAL";
    	if (!(value = field->FindValue(valuename.c_str())))
    	{
    	    ErrPrintf("gc6plus_maxwell: Failed to find %s value\n", valuename.c_str());
    	    return (1);
    	}
        reg_data = value->GetValue() << field->GetStartBit(); 
        InfoPrintf("gc6plus_maxwell: Writing %s = 0x%08x\n", reg->GetName(),reg_data);
    	fieldname =  "LW_PGC6_SCI_GPIO_OUTPUT_CNTL_NORM_OUT_DATA";
    	if (!(field = reg->FindField(fieldname.c_str())))
    	{
    	    ErrPrintf("gc6plus_maxwell: Failed to find %s field\n", fieldname.c_str());
    	    return (1);
    	}
    	valuename = "LW_PGC6_SCI_GPIO_OUTPUT_CNTL_NORM_OUT_DATA_DEASSERT";
    	if (!(value = field->FindValue(valuename.c_str())))
    	{
    	   	ErrPrintf("gc6plus_maxwell: Failed to find %s value\n", valuename.c_str());
    	   	return (1);
    	}
    	reg_data |= value->GetValue() << field->GetStartBit(); 
        	
	
    }
    else
    {
        valuename = "LW_PGC6_SCI_GPIO_OUTPUT_CNTL_SEL_SEQ_STATE_PEX_RST";
        if (!(value = field->FindValue(valuename.c_str())))
        {
        ErrPrintf("gc6plus_maxwell: Failed to find %s value\n", valuename.c_str());
        return (1);
        }
        reg_data = value->GetValue() << field->GetStartBit(); 
    }
	

    fieldname = "LW_PGC6_SCI_GPIO_OUTPUT_CNTL_POLARITY";
    if (!(field = reg->FindField(fieldname.c_str())))
    {
        ErrPrintf("gc6plus_maxwell: Failed to find %s field\n", fieldname.c_str());
        return (1);
    }
    valuename = "LW_PGC6_SCI_GPIO_OUTPUT_CNTL_POLARITY_ACTIVE_LOW";
    if (!(value = field->FindValue(valuename.c_str())))
    {
        ErrPrintf("gc6plus_maxwell: Failed to find %s value\n", valuename.c_str());
        return (1);
    }
    reg_data |= value->GetValue() << field->GetStartBit();
    
    fieldname = "LW_PGC6_SCI_GPIO_OUTPUT_CNTL_DRIVE_MODE";
    if (!(field = reg->FindField(fieldname.c_str())))
    {
        ErrPrintf("gc6plus_maxwell: Failed to find %s field\n", fieldname.c_str());
        return (1);
    }
    valuename = "LW_PGC6_SCI_GPIO_OUTPUT_CNTL_DRIVE_MODE_OPEN_DRAIN";
    if (!(value = field->FindValue(valuename.c_str())))
    {
        ErrPrintf("gc6plus_maxwell: Failed to find %s value\n", valuename.c_str());
        return (1);
    }
    reg_data |= value->GetValue() << field->GetStartBit();
    
    pSubdev->RegWr32(reg_addr, reg_data);
    InfoPrintf("gc6plus_maxwell: Writing %s(%d) = 0x%08x\n", reg->GetName(),
    gpio_index_gpu_gc6_rst, reg_data);

    this->setGpioPull(gpio_index_gpu_gc6_rst, GPIO_PULL_UP);
    this->setGpioDrive(gpio_index_gpu_gc6_rst, GPIO_DRV_NONE);
    // flush previous priv write before enabling GPU reset feedback path
    Find_Register(m_regMap, reg, "LW_PGC6_SCI_GPIO_OUTPUT_CNTL");
    reg_addr = reg->GetAddress(gpio_index_gpu_gc6_rst);
    pSubdev->RegRd32(reg_addr);
    InfoPrintf("gc6plus_maxwell: Enabling GPU GC6 reset feedback to PEX rst\n");
    EscapeWrite("gc6plus_sdr_mode_ready", 0, 1, 1);

    //Seeing a problem that the first transaction in the for loop is blocked. So adding a dummy read here to ensure that the HPD(0) is programed correctly.
    ReadReg(pSubdev, m_regMap, "LW_PGC6_SCI_GPIO_INPUT_HPD");

    //HPD////////////////////////////////////////////////////////////////////////////////////////////////////////////
    Find_Register(m_regMap, reg, "LW_PGC6_SCI_GPIO_INPUT_HPD");
    
    for(int i=0;i< HPD_LIMIT;i++){
        reg_addr = reg->GetAddress(i);
        reg_data = ModifyFieldValWithData(reg.get(), 0, "LW_PGC6_SCI_GPIO_INPUT_HPD", "_SELECT", gpio_index_hpd[i]);
        reg_data = ModifyFieldVal(reg.get(), reg_data, "LW_PGC6_SCI_GPIO_INPUT_HPD", "_GLITCH_FILTER", "_DISABLE");
        reg_data = ModifyFieldVal(reg.get(), reg_data, "LW_PGC6_SCI_GPIO_INPUT_HPD", "_ILWERT", "_NO");
        pSubdev->RegWr32(reg_addr, reg_data);
        
        this->setGpioDrive(gpio_index_hpd[i], GPIO_DRV_FULL); //Program GPIO_DRV_FULL, GPIO engine is the sole driver
        this->setGpioPull(gpio_index_hpd[i], GPIO_PULL_NONE); //Pull up GPIO Outputs
        
        this->deassertGpio(pSubdev, "hpd", i);

        Find_Register(m_regMap, reg, "LW_PGC6_SCI_GPIO_INPUT_HPD");
        reg_addr = reg->GetAddress(i);
        
        InfoPrintf("HPD_GPIO[%d] is on gpio %d = 0x%x\n", i, gpio_index_hpd[i], pSubdev->RegRd32(reg_addr));
    }
    //MISC////////////////////////////////////////////////////////////////////////////////////////////////////////////
    Find_Register(m_regMap, reg, "LW_PGC6_SCI_GPIO_INPUT_MISC");
    for(int i=0;i<4;i++){
        reg_addr = reg->GetAddress(i);
        reg_data = ModifyFieldValWithData(reg.get(), 0, "LW_PGC6_SCI_GPIO_INPUT_MISC", "_SELECT", gpio_index_misc[i]);
        reg_data = ModifyFieldVal(reg.get(), reg_data, "LW_PGC6_SCI_GPIO_INPUT_MISC", "_GLITCH_FILTER", "_DISABLE");
        reg_data = ModifyFieldVal(reg.get(), reg_data, "LW_PGC6_SCI_GPIO_INPUT_MISC", "_ILWERT", "_NO");
        pSubdev->RegWr32(reg_addr, reg_data);
        this->setGpioDrive(gpio_index_misc[i], GPIO_DRV_FULL); //Program GPIO_DRV_FULL, GPIO engine is the sole driver
        this->setGpioPull(gpio_index_misc[i], GPIO_PULL_NONE); //Pull up GPIO Outputs
        
        this->deassertGpio(pSubdev, "misc", i);
        InfoPrintf("Misc_GPIO[%d] is on gpio %d = 0x%x\n", i, gpio_index_misc[i], pSubdev->RegRd32(reg_addr));
    }
    //Power Alert////////////////////////////////////////////////////////////////////////////////////////////////////////////
    Find_Register(m_regMap, reg, "LW_PGC6_SCI_GPIO_INPUT_POWER_ALERT");
    reg_addr = reg->GetAddress();
    reg_data = ModifyFieldValWithData(reg.get(), 0, "LW_PGC6_SCI_GPIO_INPUT_POWER_ALERT", "_SELECT", gpio_index_power_alert);
    reg_data = ModifyFieldVal(reg.get(), reg_data, "LW_PGC6_SCI_GPIO_INPUT_POWER_ALERT", "_GLITCH_FILTER", "_DISABLE");
    reg_data = ModifyFieldVal(reg.get(), reg_data, "LW_PGC6_SCI_GPIO_INPUT_POWER_ALERT", "_ILWERT", "_NO");
    pSubdev->RegWr32(reg_addr, reg_data);
    this->setGpioDrive(gpio_index_power_alert, GPIO_DRV_FULL); //Program GPIO_DRV_FULL, GPIO engine is the sole driver
    this->setGpioPull(gpio_index_power_alert, GPIO_PULL_NONE); //Pull up GPIO Outputs
        
    this->deassertGpio(pSubdev, "power_alert", 0);
    InfoPrintf("Power_Alert_GPIO is on gpio GPIO_PULL_NONE%d = 0x%x\n", gpio_index_power_alert, ReadReg(pSubdev, m_regMap, "LW_PGC6_SCI_GPIO_INPUT_POWER_ALERT"));
    
    //GPU_Event////////////////////////////////////////////////////////////////////////////////////////////////////////////
    Find_Register(m_regMap, reg, "LW_PGC6_SCI_GPIO_INPUT_GPU_EVENT");
    reg_addr = reg->GetAddress();
    reg_data = ModifyFieldValWithData(reg.get(), 0, "LW_PGC6_SCI_GPIO_INPUT_GPU_EVENT", "_SELECT", gpio_index_fsm_gpu_event);
    reg_data = ModifyFieldVal(reg.get(), reg_data, "LW_PGC6_SCI_GPIO_INPUT_GPU_EVENT", "_GLITCH_FILTER", "_DISABLE");

    if (is_fmodel)
    {
        reg_data = ModifyFieldVal(reg.get(), reg_data, "LW_PGC6_SCI_GPIO_INPUT_GPU_EVENT", "_ILWERT", "_NO");
    }
    else
    {
        reg_data = ModifyFieldVal(reg.get(), reg_data, "LW_PGC6_SCI_GPIO_INPUT_GPU_EVENT", "_ILWERT", "_YES");
    }
    pSubdev->RegWr32(reg_addr, reg_data);

    this->setGpioDrive(gpio_index_fsm_gpu_event, GPIO_DRV_OPEN_DRAIN); //Program GPIO_DRV_FULL, GPIO engine is the sole driver
    this->setGpioPull(gpio_index_fsm_gpu_event, GPIO_PULL_UP); //Pull down GPU_Event GPIO Outputs
    
    this->deassertGpio(pSubdev, "gpu_event", 0);
    InfoPrintf("GPU_Event_GPIO is on gpio %d = 0x%x\n", gpio_index_fsm_gpu_event, ReadReg(pSubdev, m_regMap, "LW_PGC6_SCI_GPIO_INPUT_GPU_EVENT"));

    //GPU_EVENT Assertion Time
    InfoPrintf("gc6plus_maxwell: Configuring LW_PGC6_SCI_TMR_GPU_EVENT_ASSERT\n");
    regname = "LW_PGC6_SCI_TMR_GPU_EVENT_ASSERT"; 
    if (regname.c_str() < 0)
    {
        ErrPrintf("gc6plus_maxwell: Failed to generate register name, LW_PGC6_SCI_TMR_GPU_EVENT_ASSERT\n");
        return (1);
    }

    if (!(reg = m_regMap->FindRegister(regname.c_str())))
    {
        ErrPrintf("gc6plus_maxwell: Failed to find %s register\n", regname.c_str());
        return (1);
    }
    reg_addr = reg->GetAddress();
    fieldname = "LW_PGC6_SCI_TMR_GPU_EVENT_ASSERT_TIMEOUT";
    if (!(field = reg->FindField(fieldname.c_str())))
    {
        ErrPrintf("gc6plus_maxwell: Failed to find %s field\n", fieldname.c_str());
        return (1);
    }
    reg_data = m_sci_gpu_event_width_us << field->GetStartBit();
    pSubdev->RegWr32(reg_addr, reg_data);
    InfoPrintf("gc6plus_maxwell: Writing %s(%d) = 0x%08x\n", reg->GetName(),
        gpio_index_gpu_gc6_rst, reg_data);



    //SYS_RST_MON///////////////////////////////////////////////////////////////
    if (enab_system_reset_mon)
    {
        InfoPrintf("gc6plus_maxwell: Configuring GPIO %d for system rst mon\n",
            gpio_index_sys_rst_mon);
        this->setGpioDrive(gpio_index_sys_rst_mon, GPIO_DRV_NONE);
        this->setGpioPull(gpio_index_sys_rst_mon, GPIO_PULL_UP);
        regname = "LW_PGC6_SCI_GPIO_INPUT_SYS_PEX_RST"; 
        if (regname.c_str() < 0)
        {
            ErrPrintf("gc6plus_maxwell: Failed to generate GPIO regname\n");
            return (1);
        }

        if (!(reg = m_regMap->FindRegister(regname.c_str())))
        {
            ErrPrintf("gc6plus_maxwell: Failed to find %s register\n", regname.c_str());
            return (1);
        }
        reg_addr = reg->GetAddress();
        fieldname = "LW_PGC6_SCI_GPIO_INPUT_SYS_PEX_RST_SELECT";
        if (!(field = reg->FindField(fieldname.c_str())))
        {
            ErrPrintf("gc6plus_maxwell: Failed to find %s field\n", fieldname.c_str());
            return (1);
        }
        reg_data = ((gpio_index_sys_rst_mon << field->GetStartBit()) &
            field->GetMask());
        fieldname = "LW_PGC6_SCI_GPIO_INPUT_SYS_PEX_RST_ILWALID";
        if (!(field = reg->FindField(fieldname.c_str())))
        {
            ErrPrintf("gc6plus_maxwell: Failed to find %s field\n", fieldname.c_str());
            return (1);
        }
        valuename = "LW_PGC6_SCI_GPIO_INPUT_SYS_PEX_RST_ILWALID_NO";
        if (!(value = field->FindValue(valuename.c_str())))
        {
            ErrPrintf("gc6plus_maxwell: Failed to find %s value\n", valuename.c_str());
            return (1);
        }
        reg_data |= value->GetValue() << field->GetStartBit();
        fieldname = "LW_PGC6_SCI_GPIO_INPUT_SYS_PEX_RST_GLITCH_FILTER";
        if (!(field = reg->FindField(fieldname.c_str())))
        {
            ErrPrintf("gc6plus_maxwell: Failed to find %s field\n", fieldname.c_str());
            return (1);
        }
        valuename = "LW_PGC6_SCI_GPIO_INPUT_SYS_PEX_RST_GLITCH_FILTER_DISABLE";
        if (!(value = field->FindValue(valuename.c_str())))
        {
            ErrPrintf("gc6plus_maxwell: Failed to find %s value\n", valuename.c_str());
            return (1);
        }
        reg_data |= value->GetValue() << field->GetStartBit();
        fieldname = "LW_PGC6_SCI_GPIO_INPUT_SYS_PEX_RST_ILWERT";
        if (!(field = reg->FindField(fieldname.c_str())))
        {
            ErrPrintf("gc6plus_maxwell: Failed to find %s field\n", fieldname.c_str());
            return (1);
        }
        valuename = "LW_PGC6_SCI_GPIO_INPUT_SYS_PEX_RST_ILWERT_YES";
        if (!(value = field->FindValue(valuename.c_str())))
        {
            ErrPrintf("gc6plus_maxwell: Failed to find %s value\n", valuename.c_str());
            return (1);
        }
        reg_data |= value->GetValue() << field->GetStartBit();
        pSubdev->RegWr32(reg_addr, reg_data);
        InfoPrintf("gc6plus_maxwell: Writing %s = 0x%08x\n", reg->GetName(),
            reg_data);
        EscapeWrite("gc6plus_sys_rst_mon_enable", 0, 1, 1);
    }
    return (0);
}

//BSI RAM init now uses PMU ucode and ltssm disable is done through a BAR0 write through PMU
int GC6plus_Maxwell::ramInit(GpuSubdevice *pSubdev, string &bsiRamFileName){
    //DebugPrintf("ramInit(): Basic RAM is initialized\n");
    //if (GC6plus_Maxwell::gc6plus_bsiram_content(pSubdev,bsiRamFileName)){
    //    ErrPrintf("bsiram_content function failed\n");
    //}
    unique_ptr<IRegisterClass> reg;
    UINT32 reg_addr;
    
    DebugPrintf("ramInit(): Basic RAM is initialized through testbench backdoor mechanism\n");

    Find_Register(m_regMap, reg, "LW_PGC6_BSI_BOOTPHASES");
    reg_addr = reg->GetAddress();
    pSubdev->RegWr32(reg_addr, ( (0x2 << 25) | (0x2 << 20) | (0x1 << 15) | (0x2 << 5) | (0x2) ) ); 
    return (0); 
}    
    
    
//FIXME: Needs to be filled
int GC6plus_Maxwell::pcieInit(GpuSubdevice *pSubdev){
    
    RC rc;

    InfoPrintf("gc6plus_maxwell: Saving PCIE config space\n");

    pSubdev = lwgpu->GetGpuSubdevice();
    auto pGpuPcie = pSubdev->GetInterface<Pcie>(); 
    m_pPciCfg = pSubdev->AllocPciCfgSpace();
    rc = m_pPciCfg->Initialize(pGpuPcie->DomainNumber(),
                               pGpuPcie->BusNumber(),
                               pGpuPcie->DeviceNumber(),
                               pGpuPcie->FunctionNumber());
    if (rc != OK)
    {
        ErrPrintf("gc6plus_maxwell: Error in PciCfgSpace.Initialize\n");
        return (1);
    }

    rc = m_pPciCfg->Save();
    if (rc != OK)
    {
        ErrPrintf("gc6plus_maxwell: Error in PciCfgSpace.Save\n");
        return (1);
    }

    return (0);
}

int GC6plus_Maxwell::pcieRestore(){
    
    UINT32 timeout_ms;
    RC rc;

    InfoPrintf("gc6plus_maxwell: Polling for PCIE config space to be ready\n");
    timeout_ms = 1000 / 1000;
    rc = m_pPciCfg->CheckCfgSpaceReady(timeout_ms);
    if (rc != OK)
    {
        ErrPrintf("gc6plus_maxwell: Error in PciCfgSpace.CheckCfgSpaceReady\n");
        return (1);
    }

    InfoPrintf("gc6plus_maxwell: Restoring PCIE config space\n");
    rc = m_pPciCfg->Restore();
    if (rc != OK)
    {
        ErrPrintf("gc6plus_maxwell: Error in PciCfgSpace.Restore\n");
        return (1);
    }

    return (0);
}

int GC6plus_Maxwell::gc6plus_cleanup(GpuSubdevice *pSubdev){
    DebugPrintf("gc6plus_cleanup(): cleanup\n");
    UINT32 ltssm_state_is_ready = 0;
    UINT32 time_out_cnt = 1000;

    int err = 0;
    unique_ptr<IRegisterClass> reg;
    unique_ptr<IRegisterField> field;
    unique_ptr<IRegisterValue> value;
    UINT32 addr;
    UINT32 mask;
    UINT32 start_bit;
    UINT32 reg_rd_val, reg_wr_val;
    UINT32 reg_addr, reg_data;
    UINT32 temp_val;

    UINT32 gc6_bsi_padctrl_padsuspend = 1;
    UINT32 gc6_bsi_padctrl_pexlpmode = 1;
    UINT32 gc6_bsi_padctrl_clkreqen = 1;
    UINT32 gc6_bsi_padctrl_gc5mode = 1;
    UINT32 gc6_bsi_padctrl_alt_ctl_sel = 1;
    UINT32 gc6_bsi_padctrl_wake_core_clmp = 1;
    UINT32 gc6_bsi_padctrl_wake_alt_ctl_sel = 1;
 
    //Link needs to be enabled once GC6 exit has been done and the GPU is out of reset.
    if( (this->mode == GC6 || this->mode == RTD3 ) && 
        (this->seq == EXIT || this->seq == RESET || this->seq == ABORT) && 
        (Platform::GetSimulationMode() == Platform::RTL))
    {
        InfoPrintf("Escape Read to sci state loop\n");
        UINT32 sci_state;
    
        EscapeRead("sci_debug_fsm_state", 0, 4, &sci_state);
        InfoPrintf("sci_state: 0x%x\n", sci_state);
        Find_Register(m_regMap, reg, "LW_PGC6_SCI_DEBUG_MASTER_FSM");
        Find_Field(reg, field, "LW_PGC6_SCI_DEBUG_MASTER_FSM_STATE");
        value = field->FindValue("LW_PGC6_SCI_DEBUG_MASTER_FSM_STATE_STEADY");
        temp_val = value->GetValue();
        while(sci_state != temp_val)
        {
            Platform::Delay(5);
            EscapeRead("sci_debug_fsm_state", 0, 4, &sci_state);
            InfoPrintf("sci_state: 0x%x\n", sci_state);
        }

	    if(this->mode == RTD3)
        {
            DebugPrintf("Writing rtd3_pcie_bfm_reset  to 0\n");
            EscapeWrite("rtd3_pcie_bfm_reset", 0, 1, 0);
        }

        InfoPrintf("is_fgc6: %d\n", is_fgc6);
        InfoPrintf("RTD3 mode :  %d\n", this->mode == RTD3);
        if(!is_fgc6 && !(this->mode == RTD3) )
        {
            InfoPrintf("Escape write to ltssm\n");
            EscapeWrite("gc6plus_pcie_link_escape_hi", 0, 1, 0);
            EscapeWrite("gc6plus_pcie_link_escape_lo", 0, 1, 1);
        }   

    }

    DebugPrintf("Writing rtd3_pcie_bfm_reset  to 0\n");
    EscapeWrite("rtd3_pcie_bfm_reset", 0, 1, 0);
    //Poll on LTSSM State for link to be enabled. 

    EscapeRead("xp3g_pl_ltssm_state_is_link_ready_escape", 0, 1, &ltssm_state_is_ready);

    DebugPrintf("gc6plus_cleanup(): Polling on link been ready.\n");
    
    while(ltssm_state_is_ready == 0){
        
        if(time_out_cnt == 0){
            ErrPrintf("gc6plus_maxwell: Timeout polling on ltssm_state_is_ready after 1000 loops. The link is not ready.\n");
            err++;
            return 1;
        }
        time_out_cnt--;
        Platform::Delay(1);  
        EscapeRead("xp3g_pl_ltssm_state_is_link_ready_escape", 0, 1, &ltssm_state_is_ready);
        DebugPrintf("gc6plus_cleanup(): Polling on ltssm_state_is_ready loop %d.\n",(1000-time_out_cnt));
    }

    //Program BFM to expect a PM_PME  msg once we exit L2 due to RTD3 exit
	if(this->mode == RTD3)
    {
        DebugPrintf("Writing rtd3_pcie_prepare_for_l2_exit to 1\n");
        EscapeWrite("rtd3_pcie_prepare_for_l2_exit", 0, 1, 1);

	}

    //Restore PCIE Config Space
    this->pcieRestore();

    //Re-initialize PRIV
    this->priv_init(pSubdev);
    
    DebugPrintf("gc6plus_cleanup: mimic SW deasserting alt_mux_sel, then fb_clamp\n");
    //Mimics SW: deasserts alt_mux_sel, then fb_clamp
    Find_Register(m_regMap, reg, "LW_PGC6_SCI_PWR_SEQ_ENTRY_VEC3");
    reg_addr = reg->GetAddress();
    
    Find_Field(reg, field, "LW_PGC6_SCI_PWR_SEQ_ENTRY_VEC3_INDEX");
    reg_data = this->sw_pwr_seq_vector << (field->GetStartBit());
    reg_data = ModifyFieldVal(reg.get(), reg_data, "LW_PGC6_SCI_PWR_SEQ_ENTRY_VEC3", "_TRIGGER", "_EXELWTE");
    pSubdev->RegWr32(reg_addr,reg_data);
    InfoPrintf("gc6plus_maxwell: %s : Writing 0x%08x = 0x%08x\n",__FUNCTION__,reg_addr,reg_data);
    
    Platform::Delay(4);

    if (!(reg = m_regMap->FindRegister("LW_PMC_ELPG_ENABLE"))) {
        ErrPrintf("gc6plus_cleanup: register LW_PMC_ELPG_ENABLE is not defined!\n");
        err++;
        return (1);
    }
    if (!(field = reg->FindField("LW_PMC_ELPG_ENABLE_XBAR"))) {
        ErrPrintf("gc6plus_cleanup: field LW_PMC_ELPG_ENABLE_XBAR is not defined!\n");
        err++;
        return (1);
    }
    if (!(value = field->FindValue("LW_PMC_ELPG_ENABLE_XBAR_ENABLED"))) {
        ErrPrintf("gc6plus_cleanup: value LW_PMC_ELPG_ENABLE_XBAR_ENABLED is not defined!\n");
        err++;
        return (1);
    }
    addr = reg->GetAddress();
    reg_rd_val = lwgpu->RegRd32(addr);
    DebugPrintf("gc6plus_cleanup: read LW_PMC_ELPG_ENABLE,reg_rd_val=0x%x\n",reg_rd_val);
    mask = field->GetMask();
    start_bit = field->GetStartBit();
    reg_wr_val = (reg_rd_val | mask) & (~mask | ((value->GetValue()  << start_bit)));
    
    if (!(field = reg->FindField("LW_PMC_ELPG_ENABLE_L2"))) {
        ErrPrintf("gc6plus_cleanup: field LW_PMC_ELPG_ENABLE_L2 is not defined!\n");
        err++;
        return (1);
    }

    if (!(value = field->FindValue("LW_PMC_ELPG_ENABLE_L2_ENABLED"))) {
        ErrPrintf("gc6plus_cleanup: value LW_PMC_ELPG_ENABLE_L2_ENABLED is not defined!\n");
        err++;
        return (1);
    }

    mask = field->GetMask();
    reg_wr_val = (reg_wr_val | mask) & (~mask | ((value->GetValue()  << start_bit)));
    
    if (!(field = reg->FindField("LW_PMC_ELPG_ENABLE_PFB"))) {
        InfoPrintf("gc6plus_cleanup: WARNING: field LW_PMC_ELPG_ENABLE_PFB is not defined! Possibly removed in the manuals. \n");
    }
    else {
        if (!(value = field->FindValue("LW_PMC_ELPG_ENABLE_PFB_ENABLED"))) {
                InfoPrintf("gc6plus_cleanup: WARNING: value LW_PMC_ELPG_ENABLE_PFB_ENABLED is not defined! Possibly removed in the manuals.\n");
        mask = field->GetMask();
        }
    }
    reg_wr_val = (reg_wr_val | mask) & (~mask | ((value->GetValue()  << start_bit)));
    
    if (!(field = reg->FindField("LW_PMC_ELPG_ENABLE_HUB"))) {
        ErrPrintf("gc6plus_cleanup: field LW_PMC_ELPG_ENABLE_HUB is not defined!\n");
        err++;
        return (1);
    }

    if (!(value = field->FindValue("LW_PMC_ELPG_ENABLE_HUB_ENABLED"))) {
        ErrPrintf("gc6plus_cleanup: value LW_PMC_ELPG_ENABLE_HUB_ENABLED is not defined!\n");
        err++;
        return (1);
    }

    mask = field->GetMask();
    reg_wr_val = (reg_wr_val | mask) & (~mask | ((value->GetValue()  << start_bit)));

    lwgpu->RegWr32(addr,reg_wr_val);
    DebugPrintf("gc6plus_cleanup: write LW_PMC_ELPG_ENABLE,reg_wr_val=0x%x\n",reg_wr_val);

   
    if(this->mode == GC5){
             
        if (!(reg = m_regMap->FindRegister("LW_PTRIM_SYS_FBIO_MODE_SWITCH"))) {
            ErrPrintf("gc6plus_cleanup: register LW_PTRIM_SYS_FBIO_MODE_SWITCH is not defined!\n");
            err++;
            return (1);
        }
        if (!(field = reg->FindField("LW_PTRIM_SYS_FBIO_MODE_SWITCH_DRAMPLL_CLAMP_GC5"))) {
            ErrPrintf("gc6plus_cleanup: field LW_PTRIM_SYS_FBIO_MODE_SWITCH_DRAMPLL_CLAMP_GC5 is not defined!\n");
            err++;
            return (1);
        }
        
        addr = reg->GetAddress();
        reg_rd_val = lwgpu->RegRd32(addr);
        DebugPrintf("gc6plus_cleanup: read LW_PTRIM_SYS_FBIO_MODE_SWITCH_DRAMPLL_CLAMP_GC5,reg_rd_val=0x%x\n",reg_rd_val);
        mask = field->GetMask();
        start_bit = field->GetStartBit();
        reg_wr_val = (reg_rd_val | mask) & (~mask | (0x0 << start_bit));
        lwgpu->RegWr32(addr,reg_wr_val);
        DebugPrintf("gc6plus_cleanup: write LW_PTRIM_SYS_FBIO_MODE,reg_wr_val=0x%x\n",reg_wr_val);
    }

     if (bsi_padctrl_in_gc6_check) ///bug 200411598 BSI_PAD_CTRL- per pad checks
     {
	 InfoPrintf("gc6plus_cleanup() :Writing disables to BSI_PADCTRL register- disables\n");
	 Find_Register(m_regMap, reg, "LW_PGC6_BSI_PADCTRL");
	 reg_addr = reg->GetAddress();
	 reg_data = pSubdev->RegRd32(reg_addr);
	 reg_data = ModifyFieldVal(reg.get(), reg_data, "LW_PGC6_BSI_PADCTRL", "_PADSUSPEND", "_DISABLE");
	 pSubdev->RegWr32(reg_addr, reg_data);

	 Platform::Delay(1); 
        
	 Find_Register(m_regMap, reg, "LW_PGC6_BSI_PADCTRL");
	 reg_addr = reg->GetAddress();
	 reg_data = pSubdev->RegRd32(reg_addr);
	 reg_data = ModifyFieldVal(reg.get(), reg_data, "LW_PGC6_BSI_PADCTRL", "_PEXLPMODE", "_DISABLE");
	 reg_data = ModifyFieldVal(reg.get(), reg_data, "LW_PGC6_BSI_PADCTRL", "_CLKREQEN", "_ENABLE");
	 reg_data = ModifyFieldVal(reg.get(), reg_data, "LW_PGC6_BSI_PADCTRL", "_GC5MODE", "_DISABLE");
	 reg_data = ModifyFieldVal(reg.get(), reg_data, "LW_PGC6_BSI_PADCTRL", "_ALT_CTL_SEL", "_DISABLE");
	 reg_data = ModifyFieldVal(reg.get(), reg_data, "LW_PGC6_BSI_PADCTRL", "_WAKE_CORE_CLMP", "_DISABLE");
	 reg_data = ModifyFieldVal(reg.get(), reg_data, "LW_PGC6_BSI_PADCTRL", "_WAKE_ALT_CTL_SEL", "_DISABLE");
	 pSubdev->RegWr32(reg_addr, reg_data);
     }	

     Platform::Delay(3); 

     if (bsi_padctrl_in_gc6_check) ///bug 200411598 BSI_PAD_CTRL- per pad checks
     {
        InfoPrintf("gc6plus_cleanup(): Checking BSI clamps after RTD3 exit \n");

        EscapeRead("pgc6_bsi_padctrl_padsuspend", 0, 1, &gc6_bsi_padctrl_padsuspend);
        EscapeRead("pgc6_bsi_padctrl_pexlpmode", 0, 1, &gc6_bsi_padctrl_pexlpmode);
        EscapeRead("pgc6_bsi_padctrl_clkreqen", 0, 1, &gc6_bsi_padctrl_clkreqen);
        EscapeRead("pgc6_bsi_padctrl_gc5mode", 0, 1, &gc6_bsi_padctrl_gc5mode);
        EscapeRead("pgc6_bsi_padctrl_alt_ctl_sel", 0, 1, &gc6_bsi_padctrl_alt_ctl_sel);
        EscapeRead("pgc6_bsi_padctrl_wake_core_clmp", 0, 1, &gc6_bsi_padctrl_wake_core_clmp);
        EscapeRead("pgc6_bsi_padctrl_wake_ctl_sel", 0, 1, &gc6_bsi_padctrl_wake_alt_ctl_sel);
    
        temp_val= gc6_bsi_padctrl_padsuspend | gc6_bsi_padctrl_pexlpmode | gc6_bsi_padctrl_clkreqen | gc6_bsi_padctrl_gc5mode | gc6_bsi_padctrl_alt_ctl_sel | gc6_bsi_padctrl_wake_core_clmp | gc6_bsi_padctrl_wake_alt_ctl_sel;
	
        if(temp_val)
        {
          ErrPrintf("gc6plus_cleanup(): bsi pads not de-asserted after exitting RTD3 !!\n");
          err++;
          return 1;
        }

     }

    Platform::Delay(1); 

    DebugPrintf("priv_init(): clearing clk_req\n");
    //Disable CLK_REQ line (clears the BSI outputs that need to be done)
    ReadWriteRegFieldVal(pSubdev, m_regMap, "LW_PGC6_BSI_PADCTRL", "_CLKREQEN", "_DISABLE");

    DebugPrintf("priv_init(): finished\n");

    this->program_gpu_event_wake(pSubdev, false);       //remove GPU_EVENT based wakeups on cleanup
    
    return (0);    
}

int GC6plus_Maxwell::DelayNs(UINT32 Lwclks){
  // Lwrrently this function is in terms of .2nS / lwclk. This is incorrectly done
  // in RTL. This should be a close approx of 1 nanosecond.
  EscapeWrite("CLOCK_WAIT",IChip::ECLOCK_LWCLK,0,Lwclks * 5);
  return (0);
}

int GC6plus_Maxwell::gc6plus_triggerMode(GpuSubdevice *pSubdev){
    string regname;
    unique_ptr<IRegisterClass> reg;
    UINT32 reg_addr;

    InfoPrintf("gc6plus_triggerMode(): Trigger the mode set in class variable mode\n"); 

    //Change entry delay RTD3TODO - do || 
    if(entry_delay_us != 0x1){
        if(this->mode == GC5){
            regname = "LW_PGC6_SCI_TMR_GC5_ENTRY_HOLDOFF";
        }else if(this->mode == GC6 || this->mode == RTD3){
            regname = "LW_PGC6_SCI_TMR_GPU_EVENT_HOLDOFF";
        }else{
            ErrPrintf("ERROR: gc6plus_init(): Should not change entry delay without setting mode first.\n");
            return (1);
        }
        Find_Register(m_regMap, reg, regname.c_str());
        reg_addr = reg->GetAddress();
        pSubdev->RegWr32(reg_addr, entry_delay_us);
    }

    
    switch(this->mode)
    {
        case GC5:
            this->gc5Mode(pSubdev);
            break;
        case GC6:
            this->gc6Mode(pSubdev);
            break;
        case FGC6:
            this->fgc6Mode(pSubdev);
            break;
        case RTD3:
            this->rtd3Mode(pSubdev);
            break;
        case CYA:
            this->cyaMode(pSubdev);
            break;
        case STEADY:
            ErrPrintf("ERROR: gc6plus_triggerMode(): Mode STEADY not implmented. Remove this if the mode is defined\n");
            break;
        default:
            ErrPrintf("ERROR: gc6plus_triggerMode(): Mode Cannot be undefined. Should not enter default case in mode function\n");
            break;
    }
    
    return (0);
}
//FIXME: Bootphases
int GC6plus_Maxwell::gc5Mode(GpuSubdevice *pSubdev){

    int err = 0;
    unique_ptr<IRegisterClass> reg;
    unique_ptr<IRegisterField> field;
    unique_ptr<IRegisterValue> value;
    UINT32 temp_val;
    UINT32 addr;
    UINT32 mask;
    UINT32 start_bit;
    UINT32 reg_rd_val,reg_wr_val;

    InfoPrintf("gc5Mode(): Entering GC5\n");
    //Trigger GC5 Entry
       
    if (!(reg = m_regMap->FindRegister("LW_PTRIM_SYS_FBIO_MODE_SWITCH"))) {
        ErrPrintf("gc5Mode::CheckRegister: register LW_PTRIM_SYS_FBIO_MODE_SWITCH is not defined!\n");
        err++;
        return (1);
    }
    if (!(field = reg->FindField("LW_PTRIM_SYS_FBIO_MODE_SWITCH_DRAMPLL_CLAMP_GC5"))) {
        ErrPrintf("gc5Mode::CheckRegister: field LW_PTRIM_SYS_FBIO_MODE_SWITCH_DRAMPLL_CLAMP_GC5 is not defined!\n");
        err++;
        return (1);
    }
    
    addr = reg->GetAddress();
    reg_rd_val = lwgpu->RegRd32(addr);
    DebugPrintf("gc5Mode: read LW_PTRIM_SYS_FBIO_MODE,reg_rd_val=0x%x\n",reg_rd_val);
    mask = field->GetMask();
    start_bit = field->GetStartBit();
    reg_wr_val = (reg_rd_val | mask) & (~mask | (0x1 << start_bit));
    lwgpu->RegWr32(addr,reg_wr_val);
    DebugPrintf("gc5Mode: write LW_PTRIM_SYS_FBIO_MODE,reg_wr_val=0x%x\n",reg_wr_val);
    
    
    
    Find_Register(m_regMap, reg, "LW_PGC6_BSI_CTRL");
    Find_Field(reg, field, "LW_PGC6_BSI_CTRL_MODE");
    value = field->FindValue("LW_PGC6_BSI_CTRL_MODE_GC5");
    temp_val = value->GetValue();
    EscapeWrite("bsi_ctrl_mode_escape", 0, 2, temp_val);
    
    Find_Register(m_regMap, reg, "LW_PGC6_BSI_CTRL");
    Find_Field(reg, field, "LW_PGC6_BSI_CTRL_CYA");
    value = field->FindValue("LW_PGC6_BSI_CTRL_CYA_GC5");
    temp_val = value->GetValue();
    EscapeWrite("bsi_ctrl_cya_escape", 0, 1, temp_val);
    
    Find_Register(m_regMap, reg, "LW_PGC6_SCI_CNTL");
    Find_Field(reg, field, "LW_PGC6_SCI_CNTL_ENTRY_TRIGGER");
    value = field->FindValue("LW_PGC6_SCI_CNTL_ENTRY_TRIGGER_GC5");
    temp_val = value->GetValue();
    EscapeWrite("sci_cntl_entry_trigger_escape", 0, 2, temp_val);
    
    Find_Register(m_regMap, reg, "LW_PGC6_SCI_CNTL");
    Find_Field(reg, field, "LW_PGC6_SCI_CNTL_SW_STATUS_COPY_CLEAR");
    value = field->FindValue("LW_PGC6_SCI_CNTL_SW_STATUS_COPY_CLEAR_TRIGGER");
    temp_val = value->GetValue();
    EscapeWrite("sci_cntl_sw_status_copy_clear_escape", 0, 1, temp_val);
    InfoPrintf("Triggered GC5 entry\n");

    //FIXME
    // Number of bootphases, LW_PGC6_BSI_BOOTPHASES_GC5STOP[9:5], The number indicates the phase to stop on.
    //pSubdev->RegWr32(LW_PGC6_BSI_BOOTPHASES, 0x1 << 15 );    

    return (0);
};
//FIXME: Bootphases
int GC6plus_Maxwell::gc6Mode(GpuSubdevice *pSubdev){
    InfoPrintf("gc6Mode(): Entering GC6\n");
    UINT32 ltssm_state_is_disabled = 0;
    UINT32 time_out_cnt = 1000;
    unique_ptr<IRegisterClass> reg;
    unique_ptr<IRegisterField> field;
    unique_ptr<IRegisterValue> value;
    UINT32 temp_val;
    int err = 0; 

    //Link disable needed by PCIe BFM once GC6 entry has been triggered.
    if ( !is_fgc6 )
    {
        if( (this->seq == EXIT || this->seq == RESET || this->seq == ABORT) && 
            (Platform::GetSimulationMode() == Platform::RTL) )
        {
            Platform::Delay(1); //delay for 1us so that all pri transactions are done before disabling the link
            EscapeWrite("gc6plus_pcie_link_escape_hi", 0, 1, 1);
        }
        EscapeRead("xp3g_pl_ltssm_state_is_disabled_escape", 0, 1, &ltssm_state_is_disabled);

        DebugPrintf("gc6Mode(): Polling on link been diabled.\n");
    
        while(ltssm_state_is_disabled == 0)
        {
        
            if(time_out_cnt == 0)
            {
                ErrPrintf("gc6plus_maxwell: Timeout polling on ltssm_state_is_disabled after 1000 loops. The link is not disabled.\n");
                err++; 
                return 1;
            }
            time_out_cnt--;
            Platform::Delay(1);  
            EscapeRead("xp3g_pl_ltssm_state_is_disabled_escape", 0, 1, &ltssm_state_is_disabled);
            DebugPrintf("gc6plus_gc6Mode(): Polling on ltssm_state_is_disabled loop %d.\n",(1000-time_out_cnt));
        }
    }
    else
    {
	this->fgc6_precredit_if(pSubdev, "assert");   //for latch/clamp async
	Platform::Delay(1);
	this->fgc6_xclk_source(pSubdev, "entry");   //for selecting xclk source
	Platform::Delay(1);
	this->fgc6_Clockgating(pSubdev, "enable");   //for latch/clamp async
	Platform::Delay(1);
	this->fgc6_entry(pSubdev);
	Platform::Delay(1);
    }

    if(arm_bsi_cya_gc6_mode_check == 1)
    {
        //Trigger GC6 Entry

        Find_Register(m_regMap, reg, "LW_PGC6_BSI_CTRL");
        Find_Field(reg, field, "LW_PGC6_BSI_CTRL_MODE");
        value = field->FindValue("LW_PGC6_BSI_CTRL_MODE_CYA");
        temp_val = value->GetValue();
        EscapeWrite("bsi_ctrl_mode_escape", 0, 2, temp_val);
        
        Find_Register(m_regMap, reg, "LW_PGC6_BSI_CTRL");
        Find_Field(reg, field, "LW_PGC6_BSI_CTRL_CYA");
        value = field->FindValue("LW_PGC6_BSI_CTRL_CYA_GC6");
        temp_val = value->GetValue();
        EscapeWrite("bsi_ctrl_cya_escape", 0, 1, temp_val);
        
        Platform::Delay(1);  
        
        Find_Register(m_regMap, reg, "LW_PGC6_SCI_CNTL");
        Find_Field(reg, field, "LW_PGC6_SCI_CNTL_ENTRY_TRIGGER");
        value = field->FindValue("LW_PGC6_SCI_CNTL_ENTRY_TRIGGER_GC6");
        temp_val = value->GetValue();
        EscapeWrite("sci_cntl_entry_trigger_escape", 0, 2,temp_val);
        
        Find_Register(m_regMap, reg, "LW_PGC6_SCI_CNTL");
        Find_Field(reg, field, "LW_PGC6_SCI_CNTL_SW_STATUS_COPY_CLEAR");
        value = field->FindValue("LW_PGC6_SCI_CNTL_SW_STATUS_COPY_CLEAR_TRIGGER");
        temp_val = value->GetValue();
        EscapeWrite("sci_cntl_sw_status_copy_clear_escape", 0, 1, temp_val);

        InfoPrintf("Triggered GC6 entry, with BSI armed with CYA mode \n");
    } 
    else 
    {
        //Trigger GC6 Entry

        Find_Register(m_regMap, reg, "LW_PGC6_BSI_CTRL");
        Find_Field(reg, field, "LW_PGC6_BSI_CTRL_MODE");
        value = field->FindValue("LW_PGC6_BSI_CTRL_MODE_GC6");
        temp_val = value->GetValue();
        EscapeWrite("bsi_ctrl_mode_escape", 0, 2, temp_val);
        
        Find_Register(m_regMap, reg, "LW_PGC6_BSI_CTRL");
        Find_Field(reg, field, "LW_PGC6_BSI_CTRL_CYA");
        value = field->FindValue("LW_PGC6_BSI_CTRL_CYA_GC6");
        temp_val = value->GetValue();
        EscapeWrite("bsi_ctrl_cya_escape", 0, 1, temp_val);
        
        Platform::Delay(1);  
        
        Find_Register(m_regMap, reg, "LW_PGC6_SCI_CNTL");
        Find_Field(reg, field, "LW_PGC6_SCI_CNTL_ENTRY_TRIGGER");
        value = field->FindValue("LW_PGC6_SCI_CNTL_ENTRY_TRIGGER_GC6");
        temp_val = value->GetValue();
        EscapeWrite("sci_cntl_entry_trigger_escape", 0, 2, temp_val);
    
        Find_Register(m_regMap, reg, "LW_PGC6_SCI_CNTL");
        Find_Field(reg, field, "LW_PGC6_SCI_CNTL_SW_STATUS_COPY_CLEAR");
        value = field->FindValue("LW_PGC6_SCI_CNTL_SW_STATUS_COPY_CLEAR_TRIGGER");
        temp_val = value->GetValue();
        EscapeWrite("sci_cntl_sw_status_copy_clear_escape", 0, 1, temp_val);

        InfoPrintf("Triggered GC6 entry\n");
    }


    return (0);
};

int GC6plus_Maxwell::cyaMode(GpuSubdevice *pSubdev){
    unique_ptr<IRegisterClass> reg;
    UINT32 reg_addr;
    UINT32 reg_data;

    DebugPrintf("cyaMode: CYA mode is turned on for BSI2PMU\n");
    Find_Register(m_regMap, reg, "LW_PGC6_BSI_CTRL");
    reg_addr = reg->GetAddress();
    reg_data = pSubdev->RegRd32(reg_addr);
    //Turn on CYA mode
    reg_data = ModifyFieldVal(reg.get(), reg_data, "LW_PGC6_BSI_CTRL", "_MODE", "_CYA");
    pSubdev->RegWr32(reg_addr, reg_data);
    ////Triggering the BSI_PMUTXTRIG (need the rising edge for RTL)
    reg_data = ModifyFieldVal(reg.get(), reg_data, "LW_PGC6_BSI_PMUTXTRIG", "_TRIG", "_FALSE");
    pSubdev->RegWr32(reg_addr, reg_data);
    reg_data = ModifyFieldVal(reg.get(), reg_data, "LW_PGC6_BSI_PMUTXTRIG", "_TRIG", "_TRUE");
    pSubdev->RegWr32(reg_addr, reg_data);
    reg_data = ModifyFieldVal(reg.get(), reg_data, "LW_PGC6_BSI_PMUTXTRIG", "_TRIG", "_FALSE");
    pSubdev->RegWr32(reg_addr, reg_data);

    return (0);
};

int GC6plus_Maxwell::configure_wakeup_timer(GpuSubdevice *pSubdev, int wakeup_timer_us){
    unique_ptr<IRegisterClass> reg;
    UINT32 reg_addr;
    UINT32 reg_data;

    Find_Register(m_regMap, reg, "LW_PGC6_SCI_WAKEUP_TMR_CNTL");
    reg_addr = reg->GetAddress();

    if(this->seq == EXIT){
        reg_data = ModifyFieldValWithData(reg.get(), 0, "LW_PGC6_SCI_WAKEUP_TMR_CNTL", "_TIMER", wakeup_timer_us);
        DebugPrintf("configure_wakeup_timer(): EXIT: setting timer to %dusec\n", wakeup_timer_us);
    }else if(this->seq == ABORT){
        if(wakeup_timer_check || rtd3_wakeup_timer_check || fgc6_wake_test) {
        int abort_wakeup_timer_us = 0;//Wakeup immediately to trigger abort
        reg_data = ModifyFieldValWithData(reg.get(), 0, "LW_PGC6_SCI_WAKEUP_TMR_CNTL", "_TIMER", abort_wakeup_timer_us);
        DebugPrintf("configure_wakeup_timer(): ABORT: setting timer to %dusec\n", abort_wakeup_timer_us);
        }else {
        reg_data = ModifyFieldValWithData(reg.get(), 0, "LW_PGC6_SCI_WAKEUP_TMR_CNTL", "_TIMER", wakeup_timer_us);
            DebugPrintf("configure_wakeup_timer(): Setup wakeup timer as an exit event; EXIT: setting timer to %dusec\n", wakeup_timer_us);
        }
    }else if(this->seq == RESET){
        //Only used for GPU_EVENT_Clash Reset
        reg_data = ModifyFieldValWithData(reg.get(), 0, "LW_PGC6_SCI_WAKEUP_TMR_CNTL", "_TIMER", wakeup_timer_us);
        DebugPrintf("configure_wakeup_timer(): GPU_Event Clash Reset: setting timer to %dusec\n", wakeup_timer_us);
    }else{
        ErrPrintf("configure_wakeup_timer(): ERROR: sequence not recognized. Only Exit and Abort are legal.\n");
        return (1);
    }
    pSubdev->RegWr32(reg_addr, reg_data);

    return (0);
}

int GC6plus_Maxwell::wakeupTimerEvent(GpuSubdevice *pSubdev, std::string input){
    //Timer value to be set to Max. value so that wakeup timer event is not logged even if it is disabled.
    // If timer is not set to high value, it is kept as 0 which makes the timer expire and wakeup timer event to be logged.
    unique_ptr<IRegisterClass> reg;
    unique_ptr<IRegisterField> field;
    unique_ptr<IRegisterValue> value;
    UINT32 temp_val;
    
    if (input == "enable"){
        DebugPrintf("wakeupTimerEvent: Enable wake up timer \n");
        ReadWriteRegFieldVal(pSubdev, m_regMap, "LW_PGC6_SCI_WAKE_EN", "_WAKE_TIMER", "_ENABLE");
    }
    else if(input == "disable"){
        DebugPrintf("wakeupTimerEvent: Disable wake up timer \n"); 
        ReadWriteRegFieldVal(pSubdev, m_regMap, "LW_PGC6_SCI_WAKE_EN", "_WAKE_TIMER", "_DISABLE");
        WriteRegFieldVal(pSubdev, m_regMap, "LW_PGC6_SCI_WAKEUP_TMR_CNTL", "_TIMER", "_MAX");
        Find_Register(m_regMap, reg, "LW_PGC6_SCI_WAKEUP_TMR_CNTL");
        Find_Field(reg, field, "LW_PGC6_SCI_WAKEUP_TMR_CNTL_TIMER");
        value = field->FindValue("LW_PGC6_SCI_WAKEUP_TMR_CNTL_TIMER_MAX");
        temp_val = value->GetValue();

        DebugPrintf("wakeupTimerEvent():setting timer to max value: %dusec\n", temp_val);
    }
    else{
        ErrPrintf("Error: wakeupTimerEvent: this function can only enable or disable wakeup timer");
        return (1);
    }
    return (0);
};

int GC6plus_Maxwell::fgc6_xclk_source(GpuSubdevice *pSubdev, std::string input){
    if(this->fgc6_mode == L1) {   //xclk is gated as a part of Deep L1 sequence - so need to execute below sequence 

        if (input == "entry"){
            InfoPrintf("FGC6: fgc6_xclk_source: FGC6 entry sequence \n");

            InfoPrintf("FGC6 : For Entry  writing to LW_PTRIM_SYS_IND_SYS_CORE_CLK_PEXCLK_CTRL reg to _AUTO_DISABLE and select PEX_REFCLK as source \n");
            EscapeWrite("pexclk_ctrl_xclk_auto", 0, 1, 0);
            EscapeWrite("pexclk_ctrl_xclk", 0, 4, 2);
        }
        else if(input == "exit"){
            InfoPrintf("FGC6: fgc6_xclk_source: FGC6 entry sequence \n");

            InfoPrintf("FGC6 : For Exit writing to select XCLK_INIT as source and LW_PTRIM_SYS_IND_SYS_CORE_CLK_PEXCLK_CTRL reg to _AUTO_ENABLE \n");
            EscapeWrite("pexclk_ctrl_xclk", 0, 4, 3);
            EscapeWrite("pexclk_ctrl_xclk_auto", 0, 1, 1);
        }
        else{
            ErrPrintf("Error: fgc6_xclk_source: this function can only have entry  or exit");
            return (1);
        }
    }  
    return (0);
}


int GC6plus_Maxwell::fgc6_Clockgating(GpuSubdevice *pSubdev, std::string input){

    int err = 0;
    UINT32 time_out_cnt = 1000;
    UINT32 cg_status = 0;
    UINT32 FGC6_CG_STATUS = 0x000000aa;  //wait for all clocks to be stopped

    if (input == "enable"){
	InfoPrintf("FGC6: fgc6_Clockgating: Enable Clock gating \n");


	InfoPrintf("FGC6 : writing LW_PTRIM_SYS_FGC6_CLK_GATE to gate clocks \n");

	EscapeWrite("sys_fgc6_clk_gate_mgmt_clk_disable", 0, 1, 1);
	EscapeWrite("sys_fgc6_clk_gate_host_clk_disable", 0, 1, 1);
	EscapeWrite("sys_fgc6_clk_gate_alt_lwl_common_clk_disable", 0, 1, 1);
	EscapeWrite("sys_fgc6_clk_gate_alt_xclk_disable", 0, 1, 1);
	EscapeWrite("sys_fgc6_clk_gate_sysclk_xpxv_clk_disable", 0, 1, 1);


	InfoPrintf("FGC6 : writing LW_PTRIM_SYS_AUX_TX_RDET_DIV to gate clocks \n");
	EscapeWrite("sys_fgc6_aux_rx_bypass_refclk_disable", 0, 1, 1);
	EscapeWrite("sys_fgc6_aux_lx_aux_tx_rdet_clk_pcie_dis", 0, 1, 1);
	EscapeWrite("sys_fgc6_aux_lx_aux_tx_rdet_clk_pcie_dis_ovrd", 0, 1, 1);

	InfoPrintf("FGC6 : writing LW_PTRIM_SYS_LWLINK_AUX_TX_RDET_DIV to gate clocks \n");
	EscapeWrite("lwlink_fgc6_aux_gate", 0, 1, 1);
	EscapeWrite("lwlink_fgc6_aux_gate_ovrd", 0, 1, 1);

	InfoPrintf("FGC6 : writing LW_PTRIM_SYS_LWLINK_FGC6_CLK_GATE to gate clocks \n");
	EscapeWrite("lwlink_fgc6_clk_gate", 0, 1, 1);


	InfoPrintf("FGC6 : Pooling LW_PTRIM_SYS_FGC6_CLK_GATE for clocks to be gated - including alt_xclk  \n");
	EscapeRead("sys_fgc6_clk_gate", 0, 9, &cg_status);

	while(cg_status != FGC6_CG_STATUS)
	{
	    if(time_out_cnt == 0)
	    {
		ErrPrintf("fgc6plus_clockgating: Timeout polling on clock not gated after 1000 loops. \n");
		err++; 
		return 1;
	    }
	    time_out_cnt--;
	    Platform::Delay(10);  

	    EscapeRead("sys_fgc6_clk_gate", 0, 9, &cg_status);
	    cg_status &= FGC6_CG_STATUS; 

	    DebugPrintf("gc6plus_fgc6Mode(): Polling on clock gated loop %d.\n",(1000-time_out_cnt));
	}


    }
    else if(input == "disable"){
	InfoPrintf("FGC6: fgc6_Clockgating: Disable Clock gating \n");

	InfoPrintf("FGC6 : writing LW_PTRIM_SYS_FGC6_CLK_GATE to ungate clocks \n");
	EscapeWrite("sys_fgc6_clk_gate_mgmt_clk_disable", 0, 1, 0);
	EscapeWrite("sys_fgc6_clk_gate_host_clk_disable", 0, 1, 0);
	EscapeWrite("sys_fgc6_clk_gate_alt_lwl_common_clk_disable", 0, 1, 0);
	EscapeWrite("sys_fgc6_clk_gate_alt_xclk_disable", 0, 1, 0);
	EscapeWrite("sys_fgc6_clk_gate_sysclk_xpxv_clk_disable", 0, 1, 0);

	InfoPrintf("FGC6 : writing LW_PTRIM_SYS_AUX_TX_RDET_DIV to ungate clocks\n");
	EscapeWrite("sys_fgc6_aux_rx_bypass_refclk_disable", 0, 1, 0);
	EscapeWrite("sys_fgc6_aux_lx_aux_tx_rdet_clk_pcie_dis", 0, 1, 0);
	EscapeWrite("sys_fgc6_aux_lx_aux_tx_rdet_clk_pcie_dis_ovrd", 0, 1, 1);

	InfoPrintf("FGC6 : writing LW_PTRIM_SYS_LWLINK_AUX_TX_RDET_DIV to ungate clocks\n");
	EscapeWrite("lwlink_fgc6_aux_gate", 0, 1, 0);
	EscapeWrite("lwlink_fgc6_aux_gate_ovrd", 0, 1, 1);

	InfoPrintf("FGC6 : writing LW_PTRIM_SYS_LWLINK_FGC6_CLK_GATE to ungate clocks\n");
	EscapeWrite("lwlink_fgc6_clk_gate", 0, 1, 0);

	InfoPrintf("FGC6 : Pooling LW_PTRIM_SYS_FGC6_CLK_GATE for clocks to be ungated \n");
	EscapeRead("sys_fgc6_clk_gate", 0, 9, &cg_status);
	

	while(cg_status != 0x00000000)
	{
	    if(time_out_cnt == 0)
	    {
		ErrPrintf("fgc6plus_clockgating: Timeout polling on clock not enabled after 1000 loops. \n");
		err++; 
		return 1;
	    }
	    time_out_cnt--;
	    Platform::Delay(10);  
	    EscapeRead("sys_fgc6_clk_gate", 0, 9, &cg_status);
	    cg_status &= FGC6_CG_STATUS; 

	    DebugPrintf("gc6plus_fgc6Mode(): Polling on clock enabled loop %d.\n",(1000-time_out_cnt));
	}


    }
    else{
	ErrPrintf("Error: fgc6_Clockgating: this function can only enable or disable Clock gating");
	return (1);
    }
    return (0);
}

int GC6plus_Maxwell::assertGpio(GpuSubdevice *pSubdev, std::string input, int index){
    std::stringstream sstm;
    int gindex = gpio_index(input, index);
    sstm << "Gpio_" <<  gindex << "_state";
    char* gpio_state_escape;
    string s_esc = sstm.str();
    gpio_state_escape = new char[s_esc.size() + 1];
    strcpy(gpio_state_escape, s_esc.c_str());

    unique_ptr<IRegisterClass> reg;
    Find_Register(m_regMap, reg, "LW_PGC6_SCI_GPIO_INPUT_HPD");
    UINT32 hpd_size, hpd_limit;  
    int HPD_LIMIT;
    // The register array is expected to be 1-dimensional.  Check this and
    // then loop through all the registers in the array.
    if (reg->GetArrayDimensions() != 1)
    {
        ErrPrintf("%s: Register %s array dimension is not 1, dimension = %d\n", __FUNCTION__, reg->GetName(), reg->GetArrayDimensions());
        return (1);
    }
    reg->GetFormula1(hpd_limit, hpd_size);
    InfoPrintf("%s: Found %s array limit = %d\n", __FUNCTION__, reg->GetName(), hpd_limit); 
    HPD_LIMIT = hpd_limit;
    
    if(input == "hpd" && 0 <= index && index < HPD_LIMIT)
    {
        InfoPrintf("Asserting GPIO[%d]: hpd[%d], writing to %s\n", gindex, index, gpio_state_escape);
        EscapeWrite(gpio_state_escape, 0, 1, 1);
    }
    else if(input == "misc" && 0 <= index && index <= 3)
    {
        InfoPrintf("Asserting GPIO[%d]: misc[%d], writing to %s\n", gindex, index, gpio_state_escape);
        EscapeWrite(gpio_state_escape, 0, 1, 1);
    }
    else if(input == "power_alert" && index == 0)
    {
        InfoPrintf("Asserting GPIO[%d]: power_alert, writing to %s\n", gindex, gpio_state_escape);
        EscapeWrite(gpio_state_escape, 0, 1, 1);
    }
    else if(input == "gpu_event" && index == 0)
    {
        InfoPrintf("Asserting GPIO[%d]: gpu_event, writing to %s\n", gindex, gpio_state_escape);
        EscapeWrite(gpio_state_escape, 0, 1, 0);//Active Low
    }
    else{
        ErrPrintf("Input: %s with index %d not supported, hpd_limit = %d \n", input.c_str(), index, hpd_limit);
        return (1);
    }
    delete[] gpio_state_escape;

    return (0);
}

int GC6plus_Maxwell::deassertGpio(GpuSubdevice *pSubdev, std::string input, int index){
    std::stringstream sstm;
    int gindex = gpio_index(input, index);
    sstm << "Gpio_" <<  gindex << "_state";
    char* gpio_state_escape;
    string s_esc = sstm.str();
    gpio_state_escape = new char[s_esc.size() + 1];
    strcpy(gpio_state_escape, s_esc.c_str());
    
    unique_ptr<IRegisterClass> reg;
    Find_Register(m_regMap, reg, "LW_PGC6_SCI_GPIO_INPUT_HPD");
    UINT32 hpd_size, hpd_limit;  
    int HPD_LIMIT;
    // The register array is expected to be 1-dimensional.  Check this and
    // then loop through all the registers in the array.
    if (reg->GetArrayDimensions() != 1)
    {
        ErrPrintf("%s: Register %s array dimension is not 1, dimension = %d\n", __FUNCTION__, reg->GetName(), reg->GetArrayDimensions());
        return (1);
    }
    reg->GetFormula1(hpd_limit, hpd_size);
    InfoPrintf("%s: Found %s array limit = %d\n", __FUNCTION__, reg->GetName(), hpd_limit);
    HPD_LIMIT = hpd_limit;
     
    if(input == "hpd" && 0 <= index && index < HPD_LIMIT)
    {
        InfoPrintf("Deasserting GPIO[%d]: hpd[%d], writing to %s\n", gindex, index, gpio_state_escape);
        EscapeWrite(gpio_state_escape, 0, 1, 0);
    }
    else if(input == "misc" && 0 <= index && index <= 3)
    {
        InfoPrintf("Deasserting GPIO[%d]: misc[%d], writing to %s\n", gindex, index, gpio_state_escape);
        EscapeWrite(gpio_state_escape, 0, 1, 0);
    }
    else if(input == "power_alert" && index == 0)
    {
        InfoPrintf("Deasserting GPIO[%d]: power_alert, writing to %s\n", gindex, gpio_state_escape);
        EscapeWrite(gpio_state_escape, 0, 1, 0);
    }
    else if(input == "gpu_event" && index == 0)
    {
        InfoPrintf("Deasserting GPIO[%d]: gpu_event, writing to %s\n", gindex, gpio_state_escape);
        EscapeWrite(gpio_state_escape, 0, 1, 1);//Active Low
    }
    else{
        ErrPrintf("Input: %s with index %d not supported\n", input.c_str(), index);
        return (1);
    }
    delete[] gpio_state_escape;

    return (0);
}

int GC6plus_Maxwell::gpio_index(std::string input, int index){
    unique_ptr<IRegisterClass> reg;
    Find_Register(m_regMap, reg, "LW_PGC6_SCI_GPIO_INPUT_HPD");
    UINT32 hpd_size, hpd_limit; 
    int HPD_LIMIT;
    // The register array is expected to be 1-dimensional.  Check this and
    // then loop through all the registers in the array.
    if (reg->GetArrayDimensions() != 1)
    {
        ErrPrintf("%s: Register %s array dimension is not 1, dimension = %d\n", __FUNCTION__, reg->GetName(), reg->GetArrayDimensions());
        return (1);
    }
    reg->GetFormula1(hpd_limit, hpd_size);
    InfoPrintf("%s: Found %s array limit = %d\n", __FUNCTION__, reg->GetName(), hpd_limit);
    HPD_LIMIT = hpd_limit;
    
    if (input == "hpd" && 0 <= index && index < HPD_LIMIT){
        return gpio_index_hpd[index];
    }
    else if (input == "misc" && 0 <= index && index <= 3){
        return gpio_index_misc[index];
    }
    else if (input == "power_alert" && index == 0){
        return gpio_index_power_alert;
    }
    else if (input == "gpu_event" && index == 0){
        return gpio_index_fsm_gpu_event;
    }
    else{
        return -1;//No Match
    }
}

int GC6plus_Maxwell::setGpioDrive(int index, UINT32 drive){
    //Use escape writes to set drive
    std::stringstream sstm;
    sstm << "Gpio_" <<  index << "_drv";
    string s_esc = sstm.str();
    char* gpio_drv_escape = new char[s_esc.size() + 1];
    strcpy(gpio_drv_escape, s_esc.c_str());

    DebugPrintf("Setting drive on GPIO[%d], writing to %s\n", index, gpio_drv_escape);
    EscapeWrite(gpio_drv_escape, 0, 4, drive);
    delete[] gpio_drv_escape;
    return (0);
}

int GC6plus_Maxwell::setGpioPull(int index, UINT32 pull){
    //Use escape writes to set pull
    std::stringstream sstm;
    sstm << "Gpio_" <<  index << "_pull";
    string s_esc = sstm.str();
    char* gpio_pull_escape = new char[s_esc.size() + 1];
    strcpy(gpio_pull_escape, s_esc.c_str());

    DebugPrintf("Setting pull on GPIO[%d], writing to %s\n", index, gpio_pull_escape);
    EscapeWrite(gpio_pull_escape, 0, 4, pull);
    delete[] gpio_pull_escape;

    return (0);
}

int GC6plus_Maxwell::assertSwEnables(GpuSubdevice *pSubdev){
    unique_ptr<IRegisterClass> reg;
    unique_ptr<IRegisterField> field;
    unique_ptr<IRegisterValue> value;
    string regname, fieldname, valuename;
    UINT32 reg_addr;
    UINT32 reg_data;
    
    Find_Register(m_regMap, reg, "LW_PGC6_SCI_GPIO_INPUT_HPD");
    UINT32 hpd_size, hpd_limit;  
    int HPD_LIMIT;
    // The register array is expected to be 1-dimensional.  Check this and
    // then loop through all the registers in the array.
    if (reg->GetArrayDimensions() != 1)
    {
        ErrPrintf("%s: Register %s array dimension is not 1, dimension = %d\n", __FUNCTION__, reg->GetName(), reg->GetArrayDimensions());
        return (1);
    }
    reg->GetFormula1(hpd_limit, hpd_size);
    InfoPrintf("%s: Found %s array limit = %d\n", __FUNCTION__, reg->GetName(), hpd_limit);
    HPD_LIMIT = hpd_limit;
     
    //Enable all SW Interrupts
    InfoPrintf("gc6plus_maxwell: Configuring SW interrupt masks\n");
    regname = "LW_PGC6_SCI_SW_INTR0_EN";
    if (!(reg = m_regMap->FindRegister(regname.c_str())))
    {
        ErrPrintf("gc6plus_maxwell: Failed to find %s register\n", regname.c_str());
        return (1);
    }
    reg_addr = reg->GetAddress();
    fieldname = "LW_PGC6_SCI_SW_INTR0_EN_GPU_EVENT";
    if (!(field = reg->FindField(fieldname.c_str())))
    {
        ErrPrintf("gc6plus_maxwell: Failed to find %s field\n", fieldname.c_str());
        return (1);
    }
    valuename = "LW_PGC6_SCI_SW_INTR0_EN_GPU_EVENT_ENABLE";
    if (!(value = field->FindValue(valuename.c_str())))
    {
        ErrPrintf("gc6plus_maxwell: Failed to find %s value\n", valuename.c_str());
        return (1);
    }
    reg_data = value->GetValue() << field->GetStartBit();
    fieldname = "LW_PGC6_SCI_SW_INTR0_EN_WAKE_TIMER";
    if (!(field = reg->FindField(fieldname.c_str())))
    {
        ErrPrintf("gc6plus_maxwell: Failed to find %s field\n", fieldname.c_str());
        return (1);
    }
    valuename = "LW_PGC6_SCI_SW_INTR0_EN_WAKE_TIMER_ENABLE";
    if (!(value = field->FindValue(valuename.c_str())))
    {
        ErrPrintf("gc6plus_maxwell: Failed to find %s value\n", valuename.c_str());
        return (1);
    }
    reg_data |= value->GetValue() << field->GetStartBit();
    fieldname = "LW_PGC6_SCI_SW_INTR0_EN_I2CS";
    if (!(field = reg->FindField(fieldname.c_str())))
    {
        ErrPrintf("gc6plus_maxwell: Failed to find %s field\n", fieldname.c_str());
        return (1);
    }
    valuename = "LW_PGC6_SCI_SW_INTR0_EN_I2CS_ENABLE";
    if (!(value = field->FindValue(valuename.c_str())))
    {
        ErrPrintf("gc6plus_maxwell: Failed to find %s value\n", valuename.c_str());
        return (1);
    }
    reg_data |= value->GetValue() << field->GetStartBit();
    fieldname = "LW_PGC6_SCI_SW_INTR0_EN_BSI_EVENT";
    if (!(field = reg->FindField(fieldname.c_str())))
    {
        ErrPrintf("gc6plus_maxwell: Failed to find %s field\n", fieldname.c_str());
        return (1);
    }
    valuename = "LW_PGC6_SCI_SW_INTR0_EN_BSI_EVENT_ENABLE";
    if (!(value = field->FindValue(valuename.c_str())))
    {
        ErrPrintf("gc6plus_maxwell: Failed to find %s value\n", valuename.c_str());
        return (1);
    }
    reg_data |= value->GetValue() << field->GetStartBit();
   
    for(int i=0;i< HPD_LIMIT;i++){
        fieldname = Utility::StrPrintf("LW_PGC6_SCI_SW_INTR0_EN_HPD_IRQ_%d", i);
        if (fieldname.c_str() < 0)
        {
            ErrPrintf("gc6plus_maxwell: Failed to generate LW_PGC6_SCI_SW_INTR0_EN_HPD_IRQ_ fieldname\n");
            return (1);
        }

        if (!(field = reg->FindField(fieldname.c_str())))
        {
            ErrPrintf("gc6plus_maxwell: Failed to find %s field\n", fieldname.c_str());
            return (1);
        }
        valuename = Utility::StrPrintf("LW_PGC6_SCI_SW_INTR0_EN_HPD_IRQ_%d_ENABLE", i);
        if (valuename.c_str() < 0)
        {
            ErrPrintf("gc6plus_maxwell: Failed to generate SCI_SW_INTR0_EN_HPD_IRQ_%d_ENABLE value name\n",i);
            return (1);
        }

        if (!(value = field->FindValue(valuename.c_str())))
        {
            ErrPrintf("gc6plus_maxwell: Failed to find %s value\n", valuename.c_str());
            return (1);
        }
        reg_data |= value->GetValue() << field->GetStartBit();
    }
    fieldname = "LW_PGC6_SCI_SW_INTR0_EN_GPU_EVENT_ENTRY_RESPONSE_ERR";
    if (!(field = reg->FindField(fieldname.c_str())))
    {
        ErrPrintf("gc6plus_maxwell: Failed to find %s field\n", fieldname.c_str());
        return (1);
    }
    valuename = "LW_PGC6_SCI_SW_INTR0_EN_GPU_EVENT_ENTRY_RESPONSE_ERR_ENABLE";
    if (!(value = field->FindValue(valuename.c_str())))
    {
        ErrPrintf("gc6plus_maxwell: Failed to find %s value\n", valuename.c_str());
        return (1);
    }
    reg_data |= value->GetValue() << field->GetStartBit();
    fieldname = "LW_PGC6_SCI_SW_INTR0_EN_GPU_EVENT_EXIT_RETRY";
    if (!(field = reg->FindField(fieldname.c_str())))
    {
        ErrPrintf("gc6plus_maxwell: Failed to find %s field\n", fieldname.c_str());
        return (1);
    }
    valuename = "LW_PGC6_SCI_SW_INTR0_EN_GPU_EVENT_EXIT_RETRY_ENABLE";
    if (!(value = field->FindValue(valuename.c_str())))
    {
        ErrPrintf("gc6plus_maxwell: Failed to find %s value\n", valuename.c_str());
        return (1);
    }
    reg_data |= value->GetValue() << field->GetStartBit();
    fieldname = "LW_PGC6_SCI_SW_INTR0_EN_GC5_EXIT";
    if (!(field = reg->FindField(fieldname.c_str())))
    {
        ErrPrintf("gc6plus_maxwell: Failed to find %s field\n", fieldname.c_str());
        return (1);
    }
    valuename = "LW_PGC6_SCI_SW_INTR0_EN_GC5_EXIT_ENABLE";
    if (!(value = field->FindValue(valuename.c_str())))
    {
        ErrPrintf("gc6plus_maxwell: Failed to find %s value\n", valuename.c_str());
        return (1);
    }
    reg_data |= value->GetValue() << field->GetStartBit();
    fieldname = "LW_PGC6_SCI_SW_INTR0_EN_GC6_EXIT";
    if (!(field = reg->FindField(fieldname.c_str())))
    {
        ErrPrintf("gc6plus_maxwell: Failed to find %s field\n", fieldname.c_str());
        return (1);
    }
    valuename = "LW_PGC6_SCI_SW_INTR0_EN_GC6_EXIT_ENABLE";
    if (!(value = field->FindValue(valuename.c_str())))
    {
        ErrPrintf("gc6plus_maxwell: Failed to find %s value\n", valuename.c_str());
        return (1);
    }
    reg_data |= value->GetValue() << field->GetStartBit();
    fieldname = "LW_PGC6_SCI_SW_INTR0_EN_GC5_ABORT";
    if (!(field = reg->FindField(fieldname.c_str())))
    {
        ErrPrintf("gc6plus_maxwell: Failed to find %s field\n", fieldname.c_str());
        return (1);
    }
    valuename = "LW_PGC6_SCI_SW_INTR0_EN_GC5_ABORT_ENABLE";
    if (!(value = field->FindValue(valuename.c_str())))
    {
        ErrPrintf("gc6plus_maxwell: Failed to find %s value\n", valuename.c_str());
        return (1);
    }
    reg_data |= value->GetValue() << field->GetStartBit();
    fieldname = "LW_PGC6_SCI_SW_INTR0_EN_GC6_ABORT";
    if (!(field = reg->FindField(fieldname.c_str())))
    {
        ErrPrintf("gc6plus_maxwell: Failed to find %s field\n", fieldname.c_str());
        return (1);
    }
    valuename = "LW_PGC6_SCI_SW_INTR0_EN_GC6_ABORT_ENABLE";
    if (!(value = field->FindValue(valuename.c_str())))
    {
        ErrPrintf("gc6plus_maxwell: Failed to find %s value\n", valuename.c_str());
        return (1);
    }
    reg_data |= value->GetValue() << field->GetStartBit();
    fieldname = "LW_PGC6_SCI_SW_INTR0_EN_SR_UPDATE";
    if (!(field = reg->FindField(fieldname.c_str())))
    {
        ErrPrintf("gc6plus_maxwell: Failed to find %s field\n", fieldname.c_str());
        return (1);
    }
    valuename = "LW_PGC6_SCI_SW_INTR0_EN_SR_UPDATE_ENABLE";
    if (!(value = field->FindValue(valuename.c_str())))
    {
        ErrPrintf("gc6plus_maxwell: Failed to find %s value\n", valuename.c_str());
        return (1);
    }
    reg_data |= value->GetValue() << field->GetStartBit();
    fieldname = "LW_PGC6_SCI_SW_INTR0_EN_GPU_EVENT_ENTRY_TIMEOUT";
    if (!(field = reg->FindField(fieldname.c_str())))
    {
        ErrPrintf("gc6plus_maxwell: Failed to find %s field\n", fieldname.c_str());
        return (1);
    }
    valuename = "LW_PGC6_SCI_SW_INTR0_EN_GPU_EVENT_ENTRY_TIMEOUT_ENABLE";
    if (!(value = field->FindValue(valuename.c_str())))
    {
        ErrPrintf("gc6plus_maxwell: Failed to find %s value\n", valuename.c_str());
        return (1);
    }
    reg_data |= value->GetValue() << field->GetStartBit();

    //RTD3TODO  CPU event dont need check - enable based on mode
    if(this->mode == RTD3){
    	
    	InfoPrintf("%s: Setting RTD3 SW_INTRO register for CPU and GPU events\n",__FUNCTION__,fieldname.c_str() ,value->GetValue());
    	
    	fieldname = "LW_PGC6_SCI_SW_INTR0_EN_RTD3_CPU_EVENT";
        if (!(field = reg->FindField(fieldname.c_str())))
        {
            //ErrPrintf("gc6plus_maxwell: Failed to find %s field\n", fieldname.c_str());
            ErrPrintf("%s: Failed to find %s field\n", __FUNCTION__,fieldname.c_str());
            return (1);
        }
        valuename = "LW_PGC6_SCI_SW_INTR0_EN_RTD3_CPU_EVENT_ENABLE";
        if (!(value = field->FindValue(valuename.c_str())))
        {
            //ErrPrintf("gc6plus_maxwell: Failed to find %s value\n", valuename.c_str());
            ErrPrintf("%s: Failed to find %s value\n", __FUNCTION__,valuename.c_str());
            return (1);
        }
        reg_data |= value->GetValue() << field->GetStartBit();
        InfoPrintf("%s: Setting %s field 0x%08x \n",__FUNCTION__,fieldname.c_str() ,value->GetValue());
        
        	
        fieldname = "LW_PGC6_SCI_SW_INTR0_EN_RTD3_GPU_EVENT";
        if (!(field = reg->FindField(fieldname.c_str())))
        {
            //ErrPrintf("gc6plus_maxwell: Failed to find %s field\n", fieldname.c_str());
            ErrPrintf("%s: Failed to find %s field\n", __FUNCTION__,fieldname.c_str());
            return (1);
        }
        valuename = "LW_PGC6_SCI_SW_INTR0_EN_RTD3_GPU_EVENT_ENABLE";
        if (!(value = field->FindValue(valuename.c_str())))
        {
            //ErrPrintf("gc6plus_maxwell: Failed to find %s value\n", valuename.c_str());
            ErrPrintf("%s: Failed to find %s value\n", __FUNCTION__,valuename.c_str());
            return (1);
        }
        reg_data |= value->GetValue() << field->GetStartBit();
    	InfoPrintf("%s: Setting %s field 0x%08x \n",__FUNCTION__,fieldname.c_str() ,value->GetValue());
    }
    pSubdev->RegWr32(reg_addr, reg_data);
    InfoPrintf("gc6plus_maxwell: Writing %s = 0x%08x\n", reg->GetName(),
      reg_data);
    
    Find_Register(m_regMap, reg, "LW_PGC6_SCI_SW_INTR1_RISE_EN");
    reg_addr = reg->GetAddress();
    reg_data = pSubdev->RegRd32(reg_addr);

    
    for(int i=0;i< HPD_LIMIT;i++){
        fieldname = Utility::StrPrintf("_HPD_%d", i);
        if (fieldname.c_str() < 0)
        {
            ErrPrintf("gc6plus_maxwell: Failed to generate _HPD_%d fieldname\n",i);
            return (1);
        } 
        reg_data = ModifyFieldVal(reg.get(), reg_data, "LW_PGC6_SCI_SW_INTR1_RISE_EN", fieldname, "_ENABLE");
    }   
    
     
    reg_data = ModifyFieldVal(reg.get(), reg_data, "LW_PGC6_SCI_SW_INTR1_RISE_EN", "_MISC_0", "_ENABLE");
    reg_data = ModifyFieldVal(reg.get(), reg_data, "LW_PGC6_SCI_SW_INTR1_RISE_EN", "_MISC_1", "_ENABLE");
    reg_data = ModifyFieldVal(reg.get(), reg_data, "LW_PGC6_SCI_SW_INTR1_RISE_EN", "_MISC_2", "_ENABLE");
    reg_data = ModifyFieldVal(reg.get(), reg_data, "LW_PGC6_SCI_SW_INTR1_RISE_EN", "_MISC_3", "_ENABLE");
    reg_data = ModifyFieldVal(reg.get(), reg_data, "LW_PGC6_SCI_SW_INTR1_RISE_EN", "_POWER_ALERT", "_ENABLE");
    for(int i=0;i< HPD_LIMIT;i++){
        fieldname = Utility::StrPrintf("_HPD_STATUS_%d", i);
        if (fieldname.c_str() < 0)
        {
            ErrPrintf("gc6plus_maxwell: Failed to generate _HPD_STATUS_%d fieldname\n",i);
            return (1);
        } 
        reg_data = ModifyFieldVal(reg.get(), reg_data, "LW_PGC6_SCI_SW_INTR1_RISE_EN", fieldname, "_ENABLE");
    }   
    pSubdev->RegWr32(reg_addr, reg_data);

    Find_Register(m_regMap, reg, "LW_PGC6_SCI_SW_INTR1_FALL_EN");
    reg_addr = reg->GetAddress();
    reg_data = pSubdev->RegRd32(reg_addr);
    
    for(int i=0;i< HPD_LIMIT;i++){
        fieldname = Utility::StrPrintf("_HPD_%d", i);
        if (fieldname.c_str() < 0)
        {
            ErrPrintf("gc6plus_maxwell: Failed to generate _HPD_%d fieldname\n",i);
            return (1);
        } 
        reg_data = ModifyFieldVal(reg.get(), reg_data, "LW_PGC6_SCI_SW_INTR1_FALL_EN", fieldname, "_ENABLE");
    }   
    reg_data = ModifyFieldVal(reg.get(), reg_data, "LW_PGC6_SCI_SW_INTR1_FALL_EN", "_MISC_0", "_ENABLE");
    reg_data = ModifyFieldVal(reg.get(), reg_data, "LW_PGC6_SCI_SW_INTR1_FALL_EN", "_MISC_1", "_ENABLE");
    reg_data = ModifyFieldVal(reg.get(), reg_data, "LW_PGC6_SCI_SW_INTR1_FALL_EN", "_MISC_2", "_ENABLE");
    reg_data = ModifyFieldVal(reg.get(), reg_data, "LW_PGC6_SCI_SW_INTR1_FALL_EN", "_MISC_3", "_ENABLE");
    reg_data = ModifyFieldVal(reg.get(), reg_data, "LW_PGC6_SCI_SW_INTR1_FALL_EN", "_POWER_ALERT", "_ENABLE");
    for(int i=0;i< HPD_LIMIT;i++){
        fieldname = Utility::StrPrintf("_HPD_STATUS_%d", i);
        if (fieldname.c_str() < 0)
        {
            ErrPrintf("gc6plus_maxwell: Failed to generate _HPD_STATUS_%d fieldname\n",i);
            return (1);
        } 
        reg_data = ModifyFieldVal(reg.get(), reg_data, "LW_PGC6_SCI_SW_INTR1_FALL_EN", fieldname, "_ENABLE");
    }   
    pSubdev->RegWr32(reg_addr, reg_data);
    return (0);
}

int GC6plus_Maxwell::program_ec_mode(GpuSubdevice *pSubdev){
    unique_ptr<IRegisterClass> reg;
    UINT32 reg_addr;

    UINT32 gc6plus_ec_stub_mode;
    DebugPrintf("program_ec_mode(): Programming EC Stub\n");
    switch(this->seq){
    case EXIT:
        //Enable GPU_Event Interrupt Wakeup
        ReadWriteRegFieldVal(pSubdev, m_regMap, "LW_PGC6_SCI_WAKE_EN", "_GPU_EVENT", "_ENABLE");
        
        EscapeWrite("gc6plus_ec_stub_mode", 0, 1, 0x1);//EC_STUB_FORCE_GPU_EVENT_WAKEUP
    break;
    case ABORT: 
        //Enable GPU_Event Interrupt Wakeup
        ReadWriteRegFieldVal(pSubdev, m_regMap, "LW_PGC6_SCI_WAKE_EN", "_GPU_EVENT", "_ENABLE");
        
        EscapeWrite("gc6plus_ec_stub_mode", 0, 1, 0x5);//ec stub GPU Event Abort;
        break;
    case RESET:
        if(this->mode == GC5){
            EscapeWrite("gc6plus_ec_stub_mode", 0, 1, 0x3);//EC_STUB_FORCE_PEX_RESET
        }else if(this->mode == GC6){
            EscapeWrite("gc6plus_ec_stub_mode", 0, 1, 0x2);//EC_STUB_FORCE_GPU_EVENT_RESET;
            
        //Change the GPU_Event system reset time for RTL sim (20 us)
        Find_Register(m_regMap, reg, "LW_PGC6_SCI_TMR_SYSTEM_RESET");
        reg_addr = reg->GetAddress();
        pSubdev->RegWr32(reg_addr, 20);
        }
        break;
    default:
        ErrPrintf("ERROR: program_ec_mode(): No sequence detected for GPU Event test\n");
        break;
    EscapeRead("gc6plus_ec_stub_mode", 0, 4, &gc6plus_ec_stub_mode);
    InfoPrintf("gc6plus_ec_stub_mode is 0x%x\n", gc6plus_ec_stub_mode);
    } 
    return (0);
}

int GC6plus_Maxwell::program_gpu_event_wake(GpuSubdevice *pSubdev, bool enable_wake){
    if(enable_wake){
        InfoPrintf("program_gpu_event_wake(): Enabling GPU_EVENT based wakeups on SCI\n");
        ReadWriteRegFieldVal(pSubdev, m_regMap, "LW_PGC6_SCI_WAKE_EN", "_GPU_EVENT", "_ENABLE");
    }
    else{
        InfoPrintf("program_gpu_event_wake(): Disabling GPU_EVENT based wakeups on SCI\n");
        ReadWriteRegFieldVal(pSubdev, m_regMap, "LW_PGC6_SCI_WAKE_EN", "_GPU_EVENT", "_DISABLE");
    }

    return (0);
}

UINT32 GC6plus_Maxwell::getEntryTimer(GpuSubdevice *pSubdev){
    //Check how long it tok to enter GC5/GC6
    UINT32 entry_time = ReadReg(pSubdev, m_regMap, "LW_PGC6_SCI_ENTRY_TMR");
    InfoPrintf("entry_time = %d \n", entry_time);
    return entry_time;
}

UINT32 GC6plus_Maxwell::getResidentTimer(GpuSubdevice *pSubdev){
    //Check how long we were in GC5/GC6
    UINT32 resident_time = ReadReg(pSubdev, m_regMap, "LW_PGC6_SCI_RESIDENT_TMR");
    InfoPrintf("resident_time = %d \n", resident_time);
    return resident_time;
}

UINT32 GC6plus_Maxwell::getExitTimer(GpuSubdevice *pSubdev){
    //Check how long it took to exit GC5/GC6
    UINT32 exit_time = ReadReg(pSubdev, m_regMap, "LW_PGC6_SCI_EXIT_TMR");
    InfoPrintf("exit_time = %d \n", exit_time);
    return exit_time;
}

UINT32 GC6plus_Maxwell::checkTimers(GpuSubdevice *pSubdev){

    UINT32 reg_val;
    reg_val = ReadReg(pSubdev, m_regMap, "LW_PGC6_SCI_FSM_CFG"); // Reading LW_PGC6_SCI_FSM_CFG(0x0010e808)
    //FIXME use LW_PGC6_SCI_FSM_CFG_SKIP_GC6_ABORT_MSK instead of 0x00000004
    reg_val = reg_val & 0x00000004; // Masking all bits except bit 2 for LW_PGC6_SCI_FSM_CFG_SKIP_GC6_ABORT bit

    //Return pass or fail for GC5 or GC6
    switch(this->seq)
    {
        case EXIT:
            if(getEntryTimer(pSubdev) && getExitTimer(pSubdev)){
                DebugPrintf("checkTimers(): EXIT: Entry Time > 0 && Exit Time > 0\n");
                return 1;
            }
            ErrPrintf("checkTimers(): ERROR: EXIT case: Entry Time == 0 || Exit Time == 0\n");
            return 0;
        case ENTRY:
            ErrPrintf("checkTimers(): ERROR: Seq Entry not implemented yet. Remove this if the seq is defined");
            return 0;
        case ABORT:// FIXME: use LW_PGC6_SCI_FSM_CFG_SKIP_GC6_ABORT_ENABLE_ADJ instead of 0x00000004  
            if( reg_val != 0x00000004 || this->mode == GC5 ) {
                if(getResidentTimer(pSubdev) == 0 && getExitTimer(pSubdev) == 0){
                    DebugPrintf("checkTimers(): ABORT: Resident Time == 0 && Exit Time == 0\n");
                    return 1;
                }
            }
            else {
                if(getEntryTimer(pSubdev) && getExitTimer(pSubdev)){
                    DebugPrintf("checkTimers(): EXIT (ABORT BYPASS): Entry Time > 0 && Exit Time > 0\n");
                    return 1;
                }
            }   
            ErrPrintf("checkTimers(): ERROR: ABORT: Resident Time > 0 || Exit Time > 0\n");
            return 0;
        case RESET:
            DebugPrintf("checkTimers(): RESET: SCI Timers are reset\n");
            return 1;
        default:
            ErrPrintf("checkTimers(): Seqeunce Type cannot be undefined. Should not enter default case in seq function");
            return 0;
    }
    return (0);
}

UINT32 GC6plus_Maxwell::getSwIntr0Status(GpuSubdevice *pSubdev){
    //checking Current SW intr0 status
    UINT32 sw_intr0_status_lwrr = ReadReg(pSubdev, m_regMap, "LW_PGC6_SCI_SW_INTR0_STATUS_LWRR");
    InfoPrintf("printSwIntr0Status(): SCI_SW_INTR0_STATUS_LWRR = 0x%x \n", sw_intr0_status_lwrr);
    return sw_intr0_status_lwrr;
}

UINT32 GC6plus_Maxwell::getSwIntr1Status(GpuSubdevice *pSubdev){
    //checking Current SW intr1 status
    UINT32 sw_intr1_status_lwrr = ReadReg(pSubdev, m_regMap, "LW_PGC6_SCI_SW_INTR1_STATUS_LWRR");
    InfoPrintf("printSwIntr1Status(): SCI_SW_INTR1_STATUS_LWRR = 0x%x \n", sw_intr1_status_lwrr);
    return sw_intr1_status_lwrr;
}

int GC6plus_Maxwell::printSwIntrXStatus(GpuSubdevice *pSubdev){
    //Print both SW Status Regs
    this->getSwIntr0Status(pSubdev);
    this->getSwIntr1Status(pSubdev);
    return (0);
}

int GC6plus_Maxwell::printWakeEnables(GpuSubdevice *pSubdev){
    //Show Wake Enables
    InfoPrintf("LW_PGC6_SCI_WAKE_EN = 0x%x \n", ReadReg(pSubdev, m_regMap, "LW_PGC6_SCI_WAKE_EN"));
    InfoPrintf("LW_PGC6_SCI_WAKE_FALL_EN = 0x%x \n", ReadReg(pSubdev, m_regMap, "LW_PGC6_SCI_WAKE_FALL_EN"));
    InfoPrintf("LW_PGC6_SCI_WAKE_RISE_EN = 0x%x \n", ReadReg(pSubdev, m_regMap, "LW_PGC6_SCI_WAKE_RISE_EN"));
    return (0);
}
//FIXME: Need to fill this method
int GC6plus_Maxwell::setClkReq(GpuSubdevice *pSubdev, int data){

    return (0);
}

UINT32 GC6plus_Maxwell::getState(GpuSubdevice *pSubdev){
    //checking current state
    UINT32 debug_master_fsm = ReadReg(pSubdev, m_regMap, "LW_PGC6_SCI_DEBUG_MASTER_FSM");
    InfoPrintf("getState(): LW_PGC6_SCI_DEBUG_MASTER_FSM = 0x%x \n", debug_master_fsm);
    return debug_master_fsm;
}

UINT32 GC6plus_Maxwell::checkSteadyState(GpuSubdevice *pSubdev){
    //checking if we are in steady state
    unique_ptr<IRegisterClass> reg;
    unique_ptr<IRegisterField> field;
    unique_ptr<IRegisterValue> value;
    UINT32 temp_val;
    Find_Register(m_regMap, reg, "LW_PGC6_SCI_DEBUG_MASTER_FSM");
    Find_Field(reg, field, "LW_PGC6_SCI_DEBUG_MASTER_FSM_STATE");
    value = field->FindValue("LW_PGC6_SCI_DEBUG_MASTER_FSM_STATE_STEADY");
    temp_val = value->GetValue();
    int rv = (this->getState(pSubdev) == temp_val);
    DebugPrintf("GC6PLUS Speedy: checkSteadyState return %d\n", rv);
    return rv;
}

int GC6plus_Maxwell::waitForSteadyState(GpuSubdevice *pSubdev){
    //busy wait until we get to steady state
    DebugPrintf("waitForSteadyState(): Start\n");
    while(this->checkSteadyState(pSubdev) == 0){
        DebugPrintf("waitForSteadyState(): Not in steady state. State = 0x%x\n", this->getState(pSubdev));
        Platform::Delay(10);
    }    
    
    return (0);
}

int GC6plus_Maxwell::waitForGC5(GpuSubdevice *pSubdev){
    //busy wait until we get to GC5
    unique_ptr<IRegisterClass> reg;
    unique_ptr<IRegisterField> field;
    unique_ptr<IRegisterValue> value;
    UINT32 temp_val;
    DebugPrintf("waitForGC5(): Start\n");
    Find_Register(m_regMap, reg, "LW_PGC6_SCI_DEBUG_MASTER_FSM");
    Find_Field(reg, field, "LW_PGC6_SCI_DEBUG_MASTER_FSM_STATE");
    value = field->FindValue("LW_PGC6_SCI_DEBUG_MASTER_FSM_STATE_GC5");
    temp_val = value->GetValue();
    while(this->getState(pSubdev) != temp_val){
        DebugPrintf("waitForGC5(): Not in GC5\n");
    }
    return (0);
}

int GC6plus_Maxwell::waitForGC6(GpuSubdevice *pSubdev){
    //busy wait until we get to GC6
    unique_ptr<IRegisterClass> reg;
    unique_ptr<IRegisterField> field;
    unique_ptr<IRegisterValue> value;
    UINT32 temp_val;
    DebugPrintf("waitForGC6(): Start\n");
    Find_Register(m_regMap, reg, "LW_PGC6_SCI_DEBUG_MASTER_FSM");
    Find_Field(reg, field, "LW_PGC6_SCI_DEBUG_MASTER_FSM_STATE");
    value = field->FindValue("LW_PGC6_SCI_DEBUG_MASTER_FSM_STATE_GC6");
    temp_val = value->GetValue();
    while(this->getState(pSubdev) != temp_val){
        DebugPrintf("waitForGC6(): Not in GC6\n");
    }
    return (0);
}

UINT32 GC6plus_Maxwell::checkDebugTag(GpuSubdevice *pSubdev){
    //Read PMU Scratch: Debug Tag (0x0010A5F0)
    if (seq != RESET && seq != ABORT && this->mode != GC5 ){
        int retval = ReadReg(pSubdev, m_regMap, "LW_PPWR_PMU_DEBUG_TAG");
        InfoPrintf("PMU_Scratch: Debug Tag = 0x%x, expected = 0x%x \n", retval, this->debug_tag_data);
        return retval == this->debug_tag_data;
    }
    else{
        // BSI was reset, pass trivially
        return 1;
    }
}

UINT32 GC6plus_Maxwell::check_sw_intr_status(GpuSubdevice* pSubdev){
    UINT32 exp_intr0_status_lwrr = 0;
    UINT32 exp_intr1_status_lwrr = 0;
    string valuename;
    //FIXME: The BSI_EVENT is waived as we do not have any PMU ucode to setup the PEX Macro. Once we have the PMU ucode. Remove this waive condition
    // The clkreq is always asserted which triggers the BSI_EVENT.
    
    unique_ptr<IRegisterClass> reg;
    unique_ptr<IRegisterField> field;
    unique_ptr<IRegisterValue> value;
    UINT32 reg_data;

    Find_Register(m_regMap, reg, "LW_PGC6_SCI_SW_INTR0_STATUS_LWRR");
    Find_Field(reg, field, "LW_PGC6_SCI_SW_INTR0_STATUS_LWRR_BSI_EVENT");
    Find_Value(field, value, "LW_PGC6_SCI_SW_INTR0_STATUS_LWRR_BSI_EVENT_PENDING");
    UINT32 temp_value = value->GetValue();
    UINT32 LW_PGC6_SCI_SW_INTR0_STATUS_LWRR_BSI_EVENT_PENDING_ADJ = temp_value << field->GetStartBit();
    Find_Field(reg, field, "LW_PGC6_SCI_SW_INTR0_STATUS_LWRR_GC6_EXIT");
    Find_Value(field, value, "LW_PGC6_SCI_SW_INTR0_STATUS_LWRR_GC6_EXIT_PENDING");
    temp_value = value->GetValue();
    UINT32 LW_PGC6_SCI_SW_INTR0_STATUS_LWRR_GC6_EXIT_PENDING_ADJ = temp_value << field->GetStartBit();
    Find_Field(reg, field, "LW_PGC6_SCI_SW_INTR0_STATUS_LWRR_GC5_EXIT");
    Find_Value(field, value, "LW_PGC6_SCI_SW_INTR0_STATUS_LWRR_GC5_EXIT_PENDING");
    temp_value = value->GetValue();
    UINT32 LW_PGC6_SCI_SW_INTR0_STATUS_LWRR_GC5_EXIT_PENDING_ADJ = temp_value << field->GetStartBit();
    Find_Field(reg, field, "LW_PGC6_SCI_SW_INTR0_STATUS_LWRR_GC6_ABORT");
    Find_Value(field, value, "LW_PGC6_SCI_SW_INTR0_STATUS_LWRR_GC6_ABORT_PENDING");
    temp_value = value->GetValue();
    UINT32 LW_PGC6_SCI_SW_INTR0_STATUS_LWRR_GC6_ABORT_PENDING_ADJ = temp_value << field->GetStartBit();
    Find_Field(reg, field, "LW_PGC6_SCI_SW_INTR0_STATUS_LWRR_GC5_ABORT");
    Find_Value(field, value, "LW_PGC6_SCI_SW_INTR0_STATUS_LWRR_GC5_ABORT_PENDING");
    temp_value = value->GetValue();
    UINT32 LW_PGC6_SCI_SW_INTR0_STATUS_LWRR_GC5_ABORT_PENDING_ADJ = temp_value << field->GetStartBit();
    Find_Field(reg, field, "LW_PGC6_SCI_SW_INTR0_STATUS_LWRR_WAKE_TIMER");
    Find_Value(field, value, "LW_PGC6_SCI_SW_INTR0_STATUS_LWRR_WAKE_TIMER_PENDING");
    temp_value = value->GetValue();
    UINT32 LW_PGC6_SCI_SW_INTR0_STATUS_LWRR_WAKE_TIMER_PENDING_ADJ = temp_value << field->GetStartBit();
    Find_Field(reg, field, "LW_PGC6_SCI_SW_INTR0_STATUS_LWRR_GPU_EVENT");
    Find_Value(field, value, "LW_PGC6_SCI_SW_INTR0_STATUS_LWRR_GPU_EVENT_PENDING");
    temp_value = value->GetValue();
    UINT32 LW_PGC6_SCI_SW_INTR0_STATUS_LWRR_GPU_EVENT_PENDING_ADJ = temp_value << field->GetStartBit();
    Find_Field(reg, field, "LW_PGC6_SCI_SW_INTR0_STATUS_LWRR_SR_UPDATE");
    Find_Value(field, value, "LW_PGC6_SCI_SW_INTR0_STATUS_LWRR_SR_UPDATE_PENDING");
    temp_value = value->GetValue();
    UINT32 LW_PGC6_SCI_SW_INTR0_STATUS_LWRR_SR_UPDATE_PENDING_ADJ = temp_value << field->GetStartBit();
    Find_Field(reg, field, "LW_PGC6_SCI_SW_INTR0_STATUS_LWRR_I2CS");
    Find_Value(field, value, "LW_PGC6_SCI_SW_INTR0_STATUS_LWRR_I2CS_PENDING");
    temp_value = value->GetValue();
    UINT32 LW_PGC6_SCI_SW_INTR0_STATUS_LWRR_I2CS_PENDING_ADJ = temp_value << field->GetStartBit();
    Find_Register(m_regMap, reg, "LW_PGC6_SCI_SW_INTR1_STATUS_LWRR");
    Find_Field(reg, field, "LW_PGC6_SCI_SW_INTR1_STATUS_LWRR_MISC_0");
    Find_Value(field, value, "LW_PGC6_SCI_SW_INTR1_STATUS_LWRR_MISC_0_PENDING");
    temp_value = value->GetValue();
    UINT32 LW_PGC6_SCI_SW_INTR1_STATUS_LWRR_MISC_0_PENDING_ADJ = temp_value << field->GetStartBit();

    UINT32 waive_additional_flags_intr0 = LW_PGC6_SCI_SW_INTR0_STATUS_LWRR_BSI_EVENT_PENDING_ADJ;
    UINT32 waive_additional_flags_intr1 = 0;
    UINT32 reg_val;

    Find_Register(m_regMap, reg, "LW_PGC6_SCI_FSM_CFG");
    UINT32 reg_addr = reg->GetAddress();
    reg_val = pSubdev->RegRd32(reg_addr); // Reading LW_PGC6_SCI_FSM_CFG(0x0010e808)
    //reg_val = pSubdev->RegRd32(LW_PGC6_SCI_FSM_CFG); // Reading LW_PGC6_SCI_FSM_CFG(0x0010e808)
    Find_Field(reg, field, "LW_PGC6_SCI_FSM_CFG_SKIP_GC6_ABORT");
    reg_val = reg_val & ~field->GetMask(); // Masking all bits except bit 2 for LW_PGC6_SCI_FSM_CFG_SKIP_GC6_ABORT bit

    UINT32 sequence_flags = (seq == EXIT)? (mode == GC6 ? LW_PGC6_SCI_SW_INTR0_STATUS_LWRR_GC6_EXIT_PENDING_ADJ : LW_PGC6_SCI_SW_INTR0_STATUS_LWRR_GC5_EXIT_PENDING_ADJ): (
        (seq == ABORT) ? (mode == GC6 ? ( (reg_val == 0x00000004) ? LW_PGC6_SCI_SW_INTR0_STATUS_LWRR_GC6_EXIT_PENDING_ADJ : LW_PGC6_SCI_SW_INTR0_STATUS_LWRR_GC6_ABORT_PENDING_ADJ) : LW_PGC6_SCI_SW_INTR0_STATUS_LWRR_GC5_ABORT_PENDING_ADJ): ((seq == RESET) ? 0:0));// FIXME: LW_PGC6_SCI_FSM_CFG_SKIP_GC6_ABORT_ENABLE_ADJ instead of 0x00000004 
    
    exp_intr0_status_lwrr |= sequence_flags;

    if(this->mode ==RTD3)
    {
	
    //reset the seq flag for rtd3- it might have been set based on previous sequence flag assign statement
        sequence_flags = 0;
        Find_Register(m_regMap, reg, "LW_PGC6_SCI_SW_INTR0_STATUS_LWRR");
        Find_Field(reg, field, "LW_PGC6_SCI_SW_INTR0_STATUS_LWRR_RTD3_CPU_EVENT");
        Find_Value(field, value, "LW_PGC6_SCI_SW_INTR0_STATUS_LWRR_RTD3_CPU_EVENT_PENDING");
        temp_value = value->GetValue();
    	UINT32 LW_PGC6_SCI_SW_INTR0_STATUS_LWRR_RTD3_CPU_EVENT_PENDING_ADJ = temp_value << field->GetStartBit();

    	Find_Field(reg, field, "LW_PGC6_SCI_SW_INTR0_STATUS_LWRR_GC6_EXIT");
        Find_Value(field, value, "LW_PGC6_SCI_SW_INTR0_STATUS_LWRR_GC6_EXIT_PENDING");
        temp_value = value->GetValue();
    	UINT32 LW_PGC6_SCI_SW_INTR0_STATUS_LWRR_GC6_EXIT_PENDING_ADJ = temp_value << field->GetStartBit();

    	exp_intr0_status_lwrr = (LW_PGC6_SCI_SW_INTR0_STATUS_LWRR_RTD3_CPU_EVENT_PENDING_ADJ | LW_PGC6_SCI_SW_INTR0_STATUS_LWRR_GC6_EXIT_PENDING_ADJ) ;

    }
    
    DebugPrintf("GC6PLUS Speedy: check_sw_intr_status(): hpd_test is 0x%x\n",hpd_test); 
    DebugPrintf("GC6PLUS Speedy: check_sw_intr_status(): fgc6_hpd_test is 0x%x\n",fgc6_hpd_test); 
    
    if (wakeup_timer_check || rtd3_wakeup_timer_check || fgc6_wake_test)
    { 
        if(!ppc_rtd3_wake && !xusb_rtd3_wake)
        {
            exp_intr0_status_lwrr |= LW_PGC6_SCI_SW_INTR0_STATUS_LWRR_WAKE_TIMER_PENDING_ADJ;
        }
    }
    else if (hpd_test || rtd3_hpd_test || fgc6_hpd_test)
    {
        Find_Register(m_regMap, reg, "LW_PGC6_SCI_SW_INTR1_STATUS_LWRR");
        Find_Field(reg, field, "LW_PGC6_SCI_SW_INTR1_STATUS_LWRR_HPD_0");
        
        valuename = Utility::StrPrintf("LW_PGC6_SCI_SW_INTR1_STATUS_LWRR_HPD_%d_PENDING", hpd_gpio_num);
        if (valuename.c_str() < 0){
            ErrPrintf("gc6plus_maxwell: Failed to generate LW_PGC6_SCI_SW_INTR1_STATUS_LWRR_HPD_X_PENDING valuename.\n");
            return (1);
        }   

        Find_Value(field, value, valuename.c_str());
        reg_data = value->GetValue();
        exp_intr1_status_lwrr = (reg_data);
        DebugPrintf("GC6PLUS Speedy: check_sw_intr_status(): hpd_gpio_num is 0x%x, LW_PGC6_SCI_SW_INTR1_STATUS_LWRR_HPD_X_PENDING is 0x%x\n",hpd_gpio_num, reg_data); 

    }
    else if(gpio_wakeup_check){
        exp_intr1_status_lwrr |= LW_PGC6_SCI_SW_INTR1_STATUS_LWRR_MISC_0_PENDING_ADJ;
    }else if(gpio){
        exp_intr1_status_lwrr |= LW_PGC6_SCI_SW_INTR1_STATUS_LWRR_MISC_0_PENDING_ADJ;
    }else if(intr_gpu_event_check || intr_pex_reset_check || fgc6_intr_gpu_event_check){
        if(this->seq == EXIT || this->seq == ABORT){
            exp_intr0_status_lwrr |= LW_PGC6_SCI_SW_INTR0_STATUS_LWRR_GPU_EVENT_PENDING_ADJ;
            waive_additional_flags_intr0 |= LW_PGC6_SCI_SW_INTR0_STATUS_LWRR_WAKE_TIMER_PENDING_ADJ;
            waive_additional_flags_intr0 |= LW_PGC6_SCI_SW_INTR0_STATUS_LWRR_SR_UPDATE_PENDING_ADJ; 
        }else if(this->seq == RESET){
            waive_additional_flags_intr0 = 0;
        }
    }else if(gc6_exit_i2c_check || gc6_abort_i2c_check || i2c_gpio_check || gc6_exit_i2c_timeout_check){
        exp_intr0_status_lwrr |= LW_PGC6_SCI_SW_INTR0_STATUS_LWRR_I2CS_PENDING_ADJ;
        if(i2c_gpio_check){
            exp_intr1_status_lwrr |= LW_PGC6_SCI_SW_INTR1_STATUS_LWRR_MISC_0_PENDING_ADJ;
        }
    }   
    if (seq == RESET || rtd3_system_reset_check){
        exp_intr1_status_lwrr = exp_intr0_status_lwrr = 0;
    }
    
    UINT32 waived_intr0_status = (exp_intr0_status_lwrr | waive_additional_flags_intr0);
    UINT32 waived_intr1_status = (exp_intr1_status_lwrr | waive_additional_flags_intr1);
    InfoPrintf("GC6plus_maxwell: check_sw_intr_status(): expected sw_intr0_status_lwrr: 0x%x, expected sw_intr1_status_lwrr: 0x%x\n", exp_intr0_status_lwrr, exp_intr1_status_lwrr);
    InfoPrintf("GC6plus_maxwell: %s Waived unchecked interrupt events, waived_intr0_status 0x%x\n", __FUNCTION__, waived_intr0_status);
    InfoPrintf("GC6plus_maxwell: %s Waived unchecked interrupt events, waived_intr1_status 0x%x\n", __FUNCTION__, waived_intr1_status);
    
    UINT32 sw_intr0_status_lwrr = this->getSwIntr0Status(pSubdev);
    UINT32 sw_intr1_status_lwrr = this->getSwIntr1Status(pSubdev);
    
    int rv0 = (sw_intr0_status_lwrr == exp_intr0_status_lwrr) || (sw_intr0_status_lwrr & ~(waive_additional_flags_intr0)) == exp_intr0_status_lwrr;
    int rv1 = (sw_intr1_status_lwrr == exp_intr1_status_lwrr) || (sw_intr1_status_lwrr & ~(waive_additional_flags_intr1)) == exp_intr1_status_lwrr;
    InfoPrintf("GC6plus_maxwell: check_sw_intr_status return rv0: %d rv1: %d\n", 
                rv0, rv1);
    //1 = good 0 = fail.
    if (!(rv0 && rv1))
    {
        ErrPrintf("gc6plus_maxwell: %s LW_PGC6_SCI_SW_INTRx_STATUS_LWRR MISMATCH!\n", __FUNCTION__);
        ErrPrintf("gc6plus_maxwell: %s LW_PGC6_SCI_SW_INTR0_STATUS_LWRR = 0x%08x, SCI_INTR0_STATUS_LWRR with waivers masked = 0x%08x, Expected = 0x%08x ,  \n", __FUNCTION__, sw_intr0_status_lwrr, (sw_intr0_status_lwrr & ~(waive_additional_flags_intr0)),exp_intr0_status_lwrr );
        ErrPrintf("gc6plus_maxwell: %s LW_PGC6_SCI_SW_INTR1_STATUS_LWRR = 0x%08x, SCI_INTR1_STATUS_LWRR with waivers masked = 0x%08x, Expected = 0x%08x ,  \n", __FUNCTION__, sw_intr1_status_lwrr, (sw_intr1_status_lwrr & ~(waive_additional_flags_intr1)),exp_intr1_status_lwrr );
        return (1);
    }


    return rv0 & rv1;
}

/****************************************************************************/
/* Test basic GC6plus functionalities, such priv access, gpio, ...          */
/****************************************************************************/
GC6plus_Maxwell::GC6plus_Maxwell(ArgReader *params) :
    LWGpuSingleChannelTest(params)
{
    // IFF or IFR related code
    //initializing the variables
    initfromfuse_gc6_check=0;
    initfromrom_gc6_check=0;

    is_fmodel = 0;
    is_fgc6 = 0;
    is_fgc6_l2=0;
    is_rtd3_sli=0;
    register_check = 0;
    rpg_pri_error_check = 0;
    as2_speedup_check = 0;
    register_unmapped_check = 0;
    randomize_regs = 0;
    bsi2pmu_check  = 0;
    ptimer_check = 0;
    sw_force_wakeup_check = 0;
    pcie_check = 0;
    clkreq_in_gc6_check = 0;
    bsi_padctrl_in_gc6_check = 0;
    wakeup_timer_check = 0;
    wakeup_timer_us = 0;
    map_misc_0_check = 0;
    misc_0_gpio_pin  = 0;
    gpio_wakeup_check = 0;
    simultaneous_gpio_wakeup = 0;
    simultaneous_gpio_wakeup_delay_ns = 50;
    intr_gpu_event_check = 0;
    fgc6_intr_gpu_event_check = 0;
    intr_pex_reset_check = 0;
    bsiRamFile_check = 0;
    i2ctherm_check = 0;
    priv_access_check = 0;
    gc6_sci_i2c_check = 0;
    gc6_exit_i2c_check = 0;
    gc6_exit_i2c_timeout_check = 0;
    gc6_abort_i2c_check = 0;
    gpu_event_clash_check = 0;
    i2c_gpio_check = 0;
    abort_skip_disable_check = 0;
    system_reset_check = 0;
    enab_system_reset_mon = false;
    fundamental_reset_check = 0;
    software_reset_check = 0;
    clamp_check = 0;
    fgc6_wake_test = 0;
    hpd_test = 0;
    fgc6_hpd_test = 0;
    hpd_gpio_num = 0;
    gpio = 0;
    pcie = 0;
    cya_mode = 0;
    arm_bsi_cya_gc6_mode_check = 0;
    multiple_wakeups = 0;
    bsi2pmu_bar0_check = 0;
    bsi2pmu_32_phases_check = 0;

    //RTD3 wakeup timer check variables
    ppc_rtd3_wake = 0;
    xusb_rtd3_wake = 0;
    rtd3_wakeup_timer_check = 0;
    rtd3_cpu_wakeup_check = 0;
    rtd3_hpd_test =0;
    rtd3_system_reset_check = 0;

    if (params->ParamPresent("-is_fmodel")){
        InfoPrintf("\n\nGC6PLUS SPEEDY TEST: Asserting FMODEL flag\n\n");
        is_fmodel = 1;
    }else is_fmodel = 0;
    if (params->ParamPresent("-gc5")){
        this->mode = GC5;
        InfoPrintf("\n\nGC6PLUS SPEEDY TEST: Asserting GC5 Mode flag\n\n");
    }
    if (params->ParamPresent("-gc6")){
        this->mode = GC6;
        InfoPrintf("\n\nGC6PLUS SPEEDY TEST: Asserting GC6 Mode flag\n\n");
    }
    if (params->ParamPresent("-rtd3")){
        this->mode = RTD3;
        InfoPrintf("\n\nRTD3 SPEEDY TEST: Asserting RTD3 Mode flag\n\n");
    }
    if (params->ParamPresent("-exit")){
        this->seq = EXIT;
        InfoPrintf("\n\nGC6PLUS SPEEDY TEST: Asserting Exit Sequence flag\n\n");
    }
    if (params->ParamPresent("-abort")){
        this->seq = ABORT;
        InfoPrintf("\n\nGC6PLUS SPEEDY TEST: Asserting Abort  Sequenceflag\n\n");
    }
    if (params->ParamPresent("-reset")){
        this->seq = RESET;
        InfoPrintf("\n\nGC6PLUS SPEEDY TEST: Asserting Reset Sequence flag\n\n");
    }
    if (params->ParamPresent("-steady")){
        this->mode = STEADY;
        InfoPrintf("\n\nGC6PLUS SPEEDY TEST: Asserting Steady Mode flag\n\n");
    }
    if (params->ParamPresent("-as2_speedup")){
        as2_speedup_check = 1;
        InfoPrintf("\n\nGC6PLUS SPEEDY TEST: Speedup the tests for AS2.\n\n");
    }
    if (params->ParamPresent("-register_test")){
        register_check = 1;
        InfoPrintf("GC6PLUS SPEEDY TEST: Running register test.\n");
    }
    if (params->ParamPresent("-rpg_test")){
	rpg_pri_error_check = 1;
	InfoPrintf("GC6PLUS SPEEDY TEST: Running register test.\n");
    }

    if (params->ParamPresent("-unmapped_test")){
        register_unmapped_check = 1;
        InfoPrintf("GC6PLUS SPEEDY TEST: Running unmapped SCI register test.\n");
    }


    if (params->ParamPresent("-randomize_regs")){
        randomize_regs = 1;
        InfoPrintf("GC6PLUS SPEEDY TEST: Randomizing register values\n");
    }
    if (params->ParamPresent("-bsi2pmu_test")){
        bsi2pmu_check = 1;
        InfoPrintf("GC6PLUS SPEEDY TEST: Check BSI2PMU bootstrap\n");
    }
    if (params->ParamPresent("-ptimer_test")){
        ptimer_check = 1;
        InfoPrintf("GC6PLUS SPEEDY TEST: Check PTIMER can be correctly read and incremented");
    }
    if (params->ParamPresent("-sw_force_wakeup_test")){
        sw_force_wakeup_check  = 1;
        InfoPrintf("\n\nGC6PLUS SPEEDY TEST: Enter and exit gc5 through a SW forced wakeup timer\n\n");
    }
    if (params->ParamPresent("-pex_wakeup")){
        pcie_check  = 1;
        InfoPrintf("\n\nGC6PLUS SPEEDY TEST: Enter and exit gc5 through a PCIe event\n\n");
    }
    if (params->ParamPresent("-clkreq_in_gc6")){
        clkreq_in_gc6_check = 1;
        InfoPrintf("\n\nGC6PLUS SPEEDY TEST: Enable the de-assertion of clkreq_en at the end of the test.\n\n");
    }

    if (params->ParamPresent("-wakeup_timer")){
        wakeup_timer_check  = 1;
        wakeup_timer_us = atoi(params->ParamStr("-wakeup_timer", NULL));
        InfoPrintf("\n\nGC6PLUS SPEEDY TEST: Enter and exit/abort GC5/GC6 through a %dus wakeup timer\n\n", wakeup_timer_us);
    }

    if (params->ParamPresent("-fgc6_wakeuptest")){
        InfoPrintf("\n\nFGC6 SPEEDY TEST: Entering the if condn\n\n");
        this->mode = GC6;
        this->fgc6_wakeup_type = WAKEUP_TIMER;
        fgc6_wake_test = 1;  
        wakeup_timer_us = atoi(params->ParamStr("-fgc6_wakeuptest", NULL));
        InfoPrintf("\n\nFGC6 SPEEDY TEST: Enter and exit/abort FGC6 through a %dus wakeup timer\n\n", wakeup_timer_us);
    }

    if (params->ParamPresent("-bsi_padctrl_in_gc6")){
        bsi_padctrl_in_gc6_check = 1;
        InfoPrintf("\n\nGC6PLUS SPEEDY TEST: Enable the check for SW controllability for BSI_PADCTRL \n\n");
    }

    if (params->ParamPresent("-xusb_ppc_event")){
        xusb_ppc_event_check = 1;
        InfoPrintf("\n\nGC6PLUS SPEEDY TEST: Enter and exit/abort GC5/GC6 through xusb ppc event\n\n", wakeup_timer_us);
    }

    // TODO RTD3
    if (params->ParamPresent("-rtd3")){
        this->mode = RTD3;
        InfoPrintf("\n\nRTD3 SPEEDY TEST: Asserting RTD3 Mode flag\n\n");
    }
    if (params->ParamPresent("-ppc_rtd3_wake")){
        ppc_rtd3_wake = 1; 
        InfoPrintf("\n\nRTD3 SPEEDY TEST: Asserting PPC wake during RTD3\n\n");
    }
    if (params->ParamPresent("-xusb_rtd3_wake")){
        xusb_rtd3_wake = 1; 
        InfoPrintf("\n\nRTD3 SPEEDY TEST: Asserting xusb wake during RTD3\n\n");
    }
    if (params->ParamPresent("-rtd3_wakeup_timer")){
        rtd3_wakeup_timer_check  = 1;
        wakeup_timer_us = atoi(params->ParamStr("-rtd3_wakeup_timer", NULL));
        InfoPrintf("\n\nRTD3 SPEEDY TEST: Enter and exit/abort RTD3 through a %dus wakeup timer\n\n", wakeup_timer_us);
    }
    if (params->ParamPresent("-rtd3_cpu_wakeup")){
        rtd3_cpu_wakeup_check  = 1;
        //wakeup_timer_us = atoi(params->ParamStr("-rtd3_cpu_wakeup", NULL));
        InfoPrintf("\n\nRTD3 SPEEDY TEST: Enter and exit/abort RTD3 through a cpu wakeup \n\n");
    }
    if (params->ParamPresent("-rtd3_hpd_wakeup")){
        rtd3_hpd_test = 1;  
        hpd_gpio_num = atoi(params->ParamStr("-rtd3_hpd_wakeup", NULL));
        InfoPrintf("\n\nGC6PLUS SPEEDY TEST: hpd_test is 0x%d and hpd_gpio_num is %d \n\n", hpd_test, hpd_gpio_num);
        InfoPrintf("\n\nGC6PLUS SPEEDY TEST: Enter and exit/abort RTD3 through a HPD gpio wakeup \n\n");
    }
    if (params->ParamPresent("-rtd3_system_reset")){
        rtd3_system_reset_check = 1;
        InfoPrintf("\n\nRTD3 SPEEDY TEST: testing rtd3 system reset \n\n");
    }
    if (params->ParamPresent("-map_misc_0")){
        map_misc_0_check  = 1;
        misc_0_gpio_pin  = atoi(params->ParamStr("-map_misc_0", NULL));
        InfoPrintf("\n\nGC6PLUS SPEEDY TEST: Mapping misc_0 to GPIO pin %d \n\n", misc_0_gpio_pin);
    }
    if (params->ParamPresent("-gpio")){
        gpio  = 1;
        InfoPrintf("\n\nGC6PLUS SPEEDY TEST: Enter and exit/abort GC5/GC6 through a gpio wakeup \n\n");
    }
    if (params->ParamPresent("-hpd_wakeup")){
        hpd_test = 1;  
        hpd_gpio_num = atoi(params->ParamStr("-hpd_wakeup", NULL));
        InfoPrintf("\n\nGC6PLUS SPEEDY TEST: hpd_test is 0x%d and hpd_gpio_num is %d \n\n", hpd_test, hpd_gpio_num);
        InfoPrintf("\n\nGC6PLUS SPEEDY TEST: Enter and exit/abort GC5/GC6 through a HPD gpio wakeup \n\n");
    }
    if (params->ParamPresent("-fgc6_hpd_wakeup")){
        InfoPrintf("\n\nFGC6 SPEEDY TEST: Entering the HPD wakeup condn\n\n");
        this->mode = GC6;
        this->fgc6_wakeup_type = HPD_WAKEUP;
        fgc6_hpd_test = 1;  
        hpd_gpio_num = atoi(params->ParamStr("-fgc6_hpd_wakeup", NULL));
        InfoPrintf("\n\nFGC6 SPEEDY TEST: fgc6_hpd_test is 0x%d and hpd_gpio_num is %d \n\n", fgc6_hpd_test, hpd_gpio_num);
        InfoPrintf("\n\nFGC6 SPEEDY TEST: Enter and exit/abort FGC6 through a HPD gpio wakeup \n\n");
    }

    if (params->ParamPresent("-gpio_wakeup_test")){
        gpio_wakeup_check  = 1;
        InfoPrintf("\n\nGC6PLUS SPEEDY TEST: Test GPIO Events \n\n");
    }
    if (params->ParamPresent("-simultaneous_gpio_wakeup")){
        simultaneous_gpio_wakeup = 1;
        InfoPrintf ("\n\nGC6PLUS SPEEDY TEST: Simultaneously assert a gpio");
    }
    if (params->ParamPresent("-simultaneous_gpio_wakeup_delay_ns")){
        simultaneous_gpio_wakeup_delay_ns = atoi(params->ParamStr("-simultaneous_gpio_wakeup_delay_ns", NULL));
        InfoPrintf("GC6PLUS SPEEDY TEST: GPIO delay %d\n\n", simultaneous_gpio_wakeup_delay_ns);
    }
    if (params->ParamPresent("-intr_gpu_event")){
        intr_gpu_event_check  = 1;
        InfoPrintf("\n\nGC6PLUS SPEEDY TEST: Enter and exit GC5/GC6 through a EC initiated GPU_Event\n\n");
    }
    if (params->ParamPresent("-fgc6_intr_gpu_event")){
        fgc6_intr_gpu_event_check  = 1;
        this->mode = GC6;
        this->fgc6_wakeup_type = GPU_EVENT;
        InfoPrintf("\n\nGC6PLUS SPEEDY TEST: Enter and exit GC5/GC6 through a EC initiated GPU_Event\n\n");
    }
    if (params->ParamPresent("-intr_pex_reset")){
        intr_pex_reset_check  = 1;
        this->seq = RESET;
        InfoPrintf("\n\nGC6PLUS SPEEDY TEST: Test Reset through PEX Reset in GC5\n\n");
    }
    if (params->ParamPresent("-bsiRamFile")){
        InfoPrintf("GC6PLUS SPEEDY TEST: Check BSI2PMU with filename\n");
        bsiRamFileName = params->ParamStr("-bsiRamFile", NULL);
        InfoPrintf("%s: filename = %s\n", __FUNCTION__, bsiRamFileName.c_str());
        bsiRamFile_check = 1;
    } else {
        bsiRamFileName = DEFAULT_BSI_RAM_FILE;
    }  
    if (params->ParamPresent("-i2c_therm_test")){
        i2ctherm_check = 1;
        InfoPrintf("GC6PLUS SPEEDY TEST: Check I2C Therm Access\n");
    }
    if (params->ParamPresent("-test_bsi_priv")){
        priv_access_check = 1;
        InfoPrintf("GC6PLUS SPEEDY TEST: Check BSI Priv Access for I2C Therm Test\n");
    }
    if (params->ParamPresent("-gc6_simultaneous_i2c_gpio_test")){
        i2c_gpio_check = 1;
        InfoPrintf("GC6PLUS SPEEDY TEST: Check simultaneous I2C Therm Access and  GPIO wakeup\n");
    }
    if (params->ParamPresent("-gc6_exit_i2c_test")){
        gc6_exit_i2c_check = 1;
        InfoPrintf("GC6PLUS SPEEDY TEST: GC5 exit due to I2C event\n");
    }   
    if (params->ParamPresent("-gc6_exit_i2c_timeout_test")){
        gc6_exit_i2c_timeout_check = 1;
        InfoPrintf("GC6PLUS SPEEDY TEST: SCI Timeout(SCL) on GC6 exit due to I2C event\n");
    }   
    if (params->ParamPresent("-gc6_abort_i2c_test")){
        gc6_abort_i2c_check = 1;
        InfoPrintf("GC6PLUS SPEEDY TEST: GC5 abort due to I2C event\n");
    }
    if (params->ParamPresent("-gc6_sci_i2c_test")){
        gc6_sci_i2c_check = 1;
        InfoPrintf("GC6PLUS SPEEDY TEST: GC5 abort due to I2C event\n");
    }
    if (params->ParamPresent("-system_reset")){
        system_reset_check = 1;
        InfoPrintf("GC6PLUS SPEEDY TEST: System reset check in steady state\n");
    }
    if (params->ParamPresent("-fundamental_reset")){
        fundamental_reset_check = 1;
        InfoPrintf("\n\nGC6PLUS SPEEDY TEST: Fundamental reset in steady state\n\n");
    }

    if (params->ParamPresent("-software_reset")){
        software_reset_check = 1;
        InfoPrintf("\n\nGC6PLUS SPEEDY TEST: Software reset in steady state\n\n");
    }

    if (params->ParamPresent("-clamp_test")){
        clamp_check = 1;
        InfoPrintf("GC6PLUS SPEEDY TEST: Check clamp RTL asserts\n");
    }
    if (params->ParamPresent("-gc6_abort_skip_disable")){
        abort_skip_disable_check = 1;
        InfoPrintf("GC6PLUS SPEEDY TEST: GC6 Abort skip disabled\n");
    }
    if (params->ParamPresent("-gpu_event_clash")){
        gpu_event_clash_check = 1;
        InfoPrintf("\n\nGC6PLUS SPEEDY TEST: SCI and EC_Stub both assert GPU_EVENT\n\n");
    }
    if (params->ParamPresent("-cya_mode")){
        cya_mode = 1;
        InfoPrintf("\n\nGC6PLUS SPEEDY TEST: CYA mode enabled\n\n");
    }
    if (params->ParamPresent("-arm_bsi_cya_gc6_mode")){
        arm_bsi_cya_gc6_mode_check = 1;
        InfoPrintf("\n\nGC6PLUS SPEEDY TEST: Arming BSI with GC6 CYA mode enabled\n\n");
    }
    if (params->ParamPresent("-entry_delay")){
        entry_delay_us = atoi(params->ParamStr("-entry_delay", NULL));
        InfoPrintf("\n\nGC6PLUS SPEEDY TEST: entry delay = %dus\n\n", entry_delay_us);
    }else{
        entry_delay_us = 0x1;
    }
    if (params->ParamPresent("-exit_delay")){
        exit_delay_us = atoi(params->ParamStr("-exit_delay", NULL));
        InfoPrintf("\n\nGC6PLUS SPEEDY TEST: exit delay = %dus\n\n", exit_delay_us);
    }else{//default
        exit_delay_us = DEFAULT_EXIT_DELAY;
    }
    if (params->ParamPresent("-system_reset_mon")){
        enab_system_reset_mon = true;
    }
    if (params->ParamPresent("-rand_lwvdd_ok_offset"))
    {
        enab_rand_lwvdd_ok_offset = true;
    }
    if (params->ParamPresent("-multiple_wakeups")){
        multiple_wakeups = 1;
        InfoPrintf("\n\nGC6PLUS SPEEDY TEST: multiple wakeups test\n\n");
    }
    if (params->ParamPresent("-bsi2pmu_bar0")){
        bsi2pmu_bar0_check = 1;
        InfoPrintf("GC6PLUS SPEEDY TEST: Check different combination of Bar0 accesses with PMU using BSI2PMU phases \n");
    }
    if (params->ParamPresent("-bsi2pmu_32_phases")){
        bsi2pmu_32_phases_check = 1;
        InfoPrintf("GC6PLUS SPEEDY TEST: Check all 32 phases of BSI2PMU \n");
    }
    m_ec_reset_width_us = params->ParamUnsigned("-ec_reset_width_us", 100);
    m_ec_gpu_event_width_us = params->ParamUnsigned("-ec_gpu_event_width_us",
        100);
    m_polling_timeout_us = params->ParamUnsigned("-polling_timeout_us", 5000);
    m_sci_gpu_event_width_us = params->ParamUnsigned("-sci_gpu_event_width_us",20);
    m_seed = params->ParamUnsigned("-seed",0x1fc6);
    m_lwvdd_ok_val = params->ParamUnsigned("-lwvdd_ok_val",0x2);
    m_lwvdd_ok_offset = (UINT32) params->ParamSigned("-lwvdd_ok_offset",0); //cast as unsigned integer for EscapeWrite. Expect leading 1's to stay (used in the testbench), if negative.

    InfoPrintf("\n\nGC6PLUS SPEEDY TEST: SCI GPU EVENT Assertion Width = %dus\n\n", m_sci_gpu_event_width_us);

    // IFF and IFR related code
    //if initfromfuse_gc6 arg is present then set initfromfuse_gc6_check to 1.
    if (params->ParamPresent("-initfromfuse_gc6"))
    {
        initfromfuse_gc6_check  = 1;
        InfoPrintf("\n\nGC6PLUS SPEEDY TEST: Verify IFF at gc6 exit. \n");
    }
    //if initfromrom_gc6 arg is present then set initfromrom_gc6_check to 1.
    if (params->ParamPresent("-initfromrom_gc6"))
    {
        initfromrom_gc6_check  = 1;
        InfoPrintf("\n\nGC6PLUS SPEEDY TEST: Verify IFR at gc6 exit. \n");
    }
    if (params->ParamPresent("-fgc6_l1")){
        this->mode = GC6;
        is_fgc6 = 1;
        InfoPrintf("\n\nGC6PLUS SPEEDY TEST: Setting FGC6 flag\n\n");
        this->fgc6_mode = L1;
        InfoPrintf("\n\nFGC6 : Asserting L1 Mode flag\n\n");
    }
    if (params->ParamPresent("-fgc6_l2")){
        this->mode = RTD3;
        is_fgc6_l2=1;
        InfoPrintf("\n\nRTD3 SPEEDY TEST: Asserting RTD3 Mode flag\n\n");
        this->fgc6_mode = L2;
        InfoPrintf("\n\nFGC6 : Asserting L2 Mode flag\n\n");
        InfoPrintf("\n\nRTD3 : Asserting FGC6 clamps and latches in RTD3\n\n");
    }
   //add RTD3 SLI testing
   if (params->ParamPresent("-rtd3_sli")){
        this->mode =RTD3;
	is_rtd3_sli = 1;
        InfoPrintf("\n\nRTD3 SLI TEST: Setting RTD3 SLI flag\n\n");
    }

    if (params->ParamPresent("-fgc6_deep_l1")){
        this->mode = GC6;
        this->fgc6_mode = Deep_L1;
        InfoPrintf("\n\nFGC6 : Asserting Deep L1 Mode flag\n\n");
    }


}

GC6plus_Maxwell::~GC6plus_Maxwell(void)
{
}

STD_TEST_FACTORY(gc6plus_maxwell, GC6plus_Maxwell)

//*************************************************
// Set Up Everything Before the Test
//*************************************************
int 
GC6plus_Maxwell::Setup(void) {
    DebugPrintf("Entering the GC6plus_Maxwell::Setup() Routine\n");
    lwgpu = LWGpuResource::FindFirstResource();

    m_regMap = lwgpu->GetRegisterMap();
    getStateReport()->init("gc6plus_maxwell");
    getStateReport()->enable();

    DebugPrintf("Exiting the GC6plus_Maxwell::Setup() Routine\n");
    return 1;
}

//*************************************************
// Clean Up after running the test
//*************************************************
void
GC6plus_Maxwell::CleanUp(void) {
    if (lwgpu) {
        DebugPrintf("GC6plus_Maxwell::CleanUp(): Releasing GPU.\n");
        lwgpu = 0;
    }
    LWGpuSingleChannelTest::CleanUp();
}

//*************************************************
// Running the test
//*************************************************
void
GC6plus_Maxwell::Run(void) {
    InfoPrintf("Entering the GC6plus_Maxwell::Run() Routine\n");
    SetStatus(TEST_INCOMPLETE);
    InfoPrintf("Starting gc6plus_maxwell test\n");
    m_ErrCount = 0;
    if (register_check == 1) {
        InfoPrintf("Starting Register Check Test\n");
        if(gc6plus_register_maxwellTest()) {
            SetStatus(TEST_FAILED);
            getStateReport()->runFailed("gc6plus_register_maxwell test failed\n");
            return;
        }
        InfoPrintf("gc6plus_register_maxwellTest() Done !\n");
    }
    if (register_unmapped_check == 1) {
        InfoPrintf("Starting gc6plus_reg_unmapped_maxwell Test\n");
        if(gc6plus_reg_unmapped_maxwellTest()) {
            SetStatus(TEST_FAILED);
            getStateReport()->runFailed("gc6plus_reg_unmapped_maxwell test failed\n");
            return;
        }
        InfoPrintf("gc6plus_reg_unmapped_maxwellTest() Done !\n");
    }   
    if (bsi2pmu_check == 1) {
        InfoPrintf("Starting bsi2pmu Test\n");
        if(gc6plus_bsi2pmu_maxwellTest(bsiRamFileName)) {
            SetStatus(TEST_FAILED);
            getStateReport()->runFailed("gc6plus_bsi2pmu_maxwell test failed\n");
            return;
        }
        InfoPrintf("gc6plus_bsi2pmu_maxwellTest() Done !\n");
    }
    if (bsi2pmu_32_phases_check == 1) {
        if(gc6plus_bsi2pmu_32_phases_maxwellTest(bsiRamFileName)) {
            SetStatus(TEST_FAILED);
            getStateReport()->runFailed("gc6plus_bsi2pmu_32_phases_maxwell test failed\n");
            return;
        }
        InfoPrintf("gc6plus_bsi2pmu_maxwellTest() Done !\n");
    }
    if (ptimer_check == 1) {
        InfoPrintf("Starting ptimer Test\n");
        if(gc6plus_ptimerTest()) {
            SetStatus(TEST_FAILED);
            getStateReport()->runFailed("PTimer Test Failed\n");
            return;
        }
        InfoPrintf("gc6plus_ptimerTest() Done !\n");
    }
    if (sw_force_wakeup_check == 1){
        InfoPrintf("Starting SW Force Wakeup Test\n");
        if (gc6plus_swWakeupTest()){
            SetStatus(TEST_FAILED);
            getStateReport()->runFailed("SW Force Wakeup Test failed\n");
            return;
        }
        InfoPrintf("gc5_SW_wakeup_exit test Done!\n");
    }
    if (pcie_check == 1){
        InfoPrintf("Starting gc5 PCIe exit Test\n");
        if (gc6plus_pcieWakeupTest()){
            SetStatus(TEST_FAILED);
            getStateReport()->runFailed("gc5 PCIe exit test failed\n");
            return;
        }
        InfoPrintf("gc5 PCIe exit test Done!\n");
    }
    
    if (i2ctherm_check == 1){
        InfoPrintf("Starting gc6plus_i2ctherm Test\n");
        if(gc6plus_i2ctherm_Test()) {
            SetStatus(TEST_FAILED);
            getStateReport()->runFailed("gc6plus_i2ctherm test failed\n");
            return;
        }
        InfoPrintf("gc6plus_i2ctherm_Test() Done !\n");
        
    }
    if (i2c_gpio_check == 1){
        InfoPrintf("Starting gc6plus_i2c_gpio Test\n");
        if(gc6plus_simultaneous_i2c_gpio_Test()) {
            SetStatus(TEST_FAILED);
            getStateReport()->runFailed("gc6plus_i2c_gpio test failed\n");
            return;
        }
        InfoPrintf("gc6plus_simultaneous_i2c_gpio_Test() Done !\n");
        
    }
    if(gc6_exit_i2c_check == 1){
        InfoPrintf("Starting gc6plus_gc6_exit_i2c Test\n");
        if(gc6plus_gc6_exit_i2c_Test()) {
            SetStatus(TEST_FAILED);
            getStateReport()->runFailed("gc6plus_gc6_exit_i2c test failed\n");
            return;
        }
        InfoPrintf("gc6plus_gc6_exit_i2c_Test() Done !\n");
        
    }   
    if(gc6_exit_i2c_timeout_check == 1){
        InfoPrintf("Starting gc6plus_gc6_exit_i2c_timeout Test\n");
        if(gc6plus_gc6_exit_i2c_timeout_Test()) {
            SetStatus(TEST_FAILED);
            getStateReport()->runFailed("gc6plus_gc6_exit_i2c_timeout test failed\n");
            return;
        }
        InfoPrintf("gc6plus_gc6_exit_i2c_timeout_Test() Done !\n");
        
    }
    if(gc6_abort_i2c_check == 1){
        InfoPrintf("Starting Test\n");
        if(gc6plus_gc6_abort_i2c_Test()) {
            SetStatus(TEST_FAILED);
            getStateReport()->runFailed("gc6plus_gc6_abort_i2c test failed\n");
            return;
        }
        InfoPrintf("gc6plus_gc6_abort_i2c_Test() Done !\n");
        
    }
    if(gc6_sci_i2c_check == 1){
        InfoPrintf("Starting gc6plus_sci_i2c Test\n");
        if(gc6plus_sci_i2c_Test()) {
            SetStatus(TEST_FAILED);
            getStateReport()->runFailed("gc6plus_sci_i2c test failed\n");
            return;
        }
        InfoPrintf("gc6plus_sci_i2c_Test() Done !\n");
        
    }
    if (clamp_check == 1){
        InfoPrintf("Starting Clamp check Test\n");
        if (gc6plus_clampTest()){
            SetStatus(TEST_FAILED);
            getStateReport()->runFailed("Clamp check test failed\n");
            return;
        }
        InfoPrintf("Clamp check test Done!\n");
    }
    if (wakeup_timer_check == 1){
        InfoPrintf("Starting Wakeup Timer Test\n");
        if (gc6plus_wakeupTest(false, true)){
            SetStatus(TEST_FAILED);
            getStateReport()->runFailed("Wakeup Timer Test failed\n");
            return;
        }
        InfoPrintf("Wakeup Timer test Done!\n");
    }
    if (fgc6_wake_test == 1){
        InfoPrintf("Starting FGC6 Wakeup Timer Test\n");
        if (fgc6_wakeupTest(false, true, "no_hpd", 0,simultaneous_gpio_wakeup)){
            SetStatus(TEST_FAILED);
            getStateReport()->runFailed("FGC6 Wakeup Timer Test failed\n");
            return;
        }
        InfoPrintf(" FGC6 Wakeup Timer test Done!\n");
    }
    if (rtd3_wakeup_timer_check == 1 || rtd3_cpu_wakeup_check ==1 || rtd3_system_reset_check){
        InfoPrintf("Starting RTD3 Wakeup Timer/CPU Wake Test\n");
        //EscapeWritePrint("rtd3_pcie_bfm_reset", 0, 1, 1);
        //if (rtd3_wakeupTest("nohpd",0)){
        if (rtd3_wakeupTest(false,true)){
            SetStatus(TEST_FAILED);
            getStateReport()->runFailed("RTD3 Wakeup Timer/ CPU Wake Test failed\n");
            return;
        }
        InfoPrintf(" RTD3 Wakeup Timer/ CPU Wake test Done!\n");
    }

    if (xusb_ppc_event_check == 1){
        InfoPrintf("Starting XUSB PPC Event Test for gc6/fgc6 \n");
        if (gc6plus_XUSB_PPC_wakeup(false, true)){
            SetStatus(TEST_FAILED);
            getStateReport()->runFailed("XUSB PPC Event Test failed\n");
            return;
        }
        InfoPrintf("XUSB PPC Event Test Done!\n");
    }
    if (gpio == 1)
    {
        InfoPrintf("Starting Test\n");
        if(fgc6_hpd_test == 1){
            switch (hpd_gpio_num)
            {
                case 0 :
                {
                    if(fgc6_wakeupTest(false,false,"hpd",hpd_gpio_num,simultaneous_gpio_wakeup))
                    {
                        SetStatus(TEST_FAILED);
                        getStateReport()->runFailed("GPIO HPD 0 Wakeup Test failed\n" );
                        return;
                    }
                    break;
                } 
                case 1 :
                {
                    if(fgc6_wakeupTest(false,false,"hpd",hpd_gpio_num,simultaneous_gpio_wakeup))
                    {
                        SetStatus(TEST_FAILED);
                        getStateReport()->runFailed("GPIO HPD 1 Wakeup Test failed\n" );
                        return;
                    }
                    break;
                } 
                case 2 :
                {
                    if(fgc6_wakeupTest(false,false,"hpd",hpd_gpio_num,simultaneous_gpio_wakeup))
                    {
                        SetStatus(TEST_FAILED);
                        getStateReport()->runFailed("GPIO HPD 2 Wakeup Test failed\n" );
                        return;
                    }
                    break;
                } 
                case 3 :
                {
                    if(fgc6_wakeupTest(false,false,"hpd",hpd_gpio_num,simultaneous_gpio_wakeup))
                    {
                        SetStatus(TEST_FAILED);
                        getStateReport()->runFailed("GPIO HPD 3 Wakeup Test failed\n" );
                        return;
                    }
                    break;
                } 
                case 4 :
                {
                    if(fgc6_wakeupTest(false,false,"hpd",hpd_gpio_num,simultaneous_gpio_wakeup))
                    {
                        SetStatus(TEST_FAILED);
                        getStateReport()->runFailed("GPIO HPD 4 Wakeup Test failed\n" );
                        return;
                    }
                    break;
                } 

                case 5 :
                {
                    if(fgc6_wakeupTest(false,false,"hpd",hpd_gpio_num,simultaneous_gpio_wakeup))
                    {
                        SetStatus(TEST_FAILED);
                        getStateReport()->runFailed("GPIO HPD 5 Wakeup Test failed\n" );
                        return;
                    }
                    break;
                } 
                case 6 :
                {
                    if(fgc6_wakeupTest(false,false,"hpd",hpd_gpio_num,simultaneous_gpio_wakeup))
                    {
                        SetStatus(TEST_FAILED);
                        getStateReport()->runFailed("GPIO HPD 6 Wakeup Test failed\n" );
                        return;
                    }
                    break;
                } 
                default :
                {
                    ErrPrintf("Only Gpio 0-6 is supported right now\n");
                    return;
                    break;
                }

            }
        
        } 
        else if(hpd_test == 1){
            switch (hpd_gpio_num)
            {
                case 0 :
                {
                    if(gc6plus_gpioWakeupTest("hpd",hpd_gpio_num))
                    {
                        SetStatus(TEST_FAILED);
                        getStateReport()->runFailed("GPIO HPD 0 Wakeup Test failed\n" );
                        return;
                    }
                    break;
                } 
                case 1 :
                {
                    if(gc6plus_gpioWakeupTest("hpd",hpd_gpio_num))
                    {
                        SetStatus(TEST_FAILED);
                        getStateReport()->runFailed("GPIO HPD 1 Wakeup Test failed\n" );
                        return;
                    }
                    break;
                } 
                case 2 :
                {
                    if(gc6plus_gpioWakeupTest("hpd",hpd_gpio_num))
                    {
                        SetStatus(TEST_FAILED);
                        getStateReport()->runFailed("GPIO HPD 2 Wakeup Test failed\n" );
                        return;
                    }
                    break;
                } 
                case 3 :
                {
                    if(gc6plus_gpioWakeupTest("hpd",hpd_gpio_num))
                    {
                        SetStatus(TEST_FAILED);
                        getStateReport()->runFailed("GPIO HPD 3 Wakeup Test failed\n" );
                        return;
                    }
                    break;
                } 
                case 4 :
                {
                    if(gc6plus_gpioWakeupTest("hpd",hpd_gpio_num))
                    {
                        SetStatus(TEST_FAILED);
                        getStateReport()->runFailed("GPIO HPD 4 Wakeup Test failed\n" );
                        return;
                    }
                    break;
                } 

                case 5 :
                {
                    if(gc6plus_gpioWakeupTest("hpd",hpd_gpio_num))
                    {
                        SetStatus(TEST_FAILED);
                        getStateReport()->runFailed("GPIO HPD 5 Wakeup Test failed\n" );
                        return;
                    }
                    break;
                } 
                case 6 :
                {
                    if(gc6plus_gpioWakeupTest("hpd",hpd_gpio_num))
                    {
                        SetStatus(TEST_FAILED);
                        getStateReport()->runFailed("GPIO HPD 6 Wakeup Test failed\n" );
                        return;
                    }
                    break;
                } 
                default :
                {
                    ErrPrintf("Only Gpio 0-6 is supported right now\n");
                    return;
                    break;
                }

            }
        
        }
     	else if(rtd3_hpd_test == 1)
        {
                if(rtd3_wakeupTest(false,true))
                {
                    SetStatus(TEST_FAILED);
                    getStateReport()->runFailed("GPIO HPD # Wakeup Test failed\n");
                    return;
                }
    	}
    	else 
        {
                if (gc6plus_gpioWakeupTest("misc",0)){
                    SetStatus(TEST_FAILED);
                    getStateReport()->runFailed("GPIO Wakeup Test failed\n");
                    return;
                }
        }
        InfoPrintf("GPIO Wakeup Test Done!\n");
    }
    if (gpio_wakeup_check == 1){
        InfoPrintf("Starting GPIO Wakeup Test\n");
        if (gc6plus_wakeupTest(true, false)){
            SetStatus(TEST_FAILED);
            getStateReport()->runFailed("GPIO Wakeup Test failed\n");
            return;
        }
        InfoPrintf("GPIO Wakeup test Done!\n");
    }
    if (intr_gpu_event_check == 1){
        InfoPrintf("Starting GPU_Event Test (Include extra GPIO wakeup = %d)\n",simultaneous_gpio_wakeup);
        if (gc6plus_intrGpuEventTest(simultaneous_gpio_wakeup)){
            SetStatus(TEST_FAILED);
            getStateReport()->runFailed("GPU_Event Test failed\n");
            return;
        }
        InfoPrintf("GPU_Event Test Done!\n");
    }
    if (fgc6_intr_gpu_event_check == 1){
        InfoPrintf("Starting GPU_Event Test (Include extra GPIO wakeup = %d)\n",simultaneous_gpio_wakeup);
	    this->mode = GC6;
    	this->fgc6_wakeup_type = GPU_EVENT;
        if (fgc6_wakeupTest(false,false, "no_hpd", 0,simultaneous_gpio_wakeup)){
            SetStatus(TEST_FAILED);
            getStateReport()->runFailed("GPU_Event Test failed\n");
            return;
        }
        InfoPrintf("GPU_Event Test Done!\n");
    }
    if (intr_pex_reset_check == 1){
        InfoPrintf("Starting GC5 Pex Reset Test\n");
        if (gc6plus_pexResetGC5Test()){
            SetStatus(TEST_FAILED);
            getStateReport()->runFailed("GC5 PEX Reset test failed\n");
            return;
        }
        InfoPrintf("GC5 PEX Reset test Done!\n");
    }
    if (system_reset_check == 1){
        InfoPrintf("Starting System Reset Test (cya mode = %d)\n", cya_mode);
        if (cya_mode){
            if (gc6plus_systemResetCYATest()){
                SetStatus(TEST_FAILED);
                getStateReport()->runFailed("System Reset CYA-mode test failed\n");
            }
        }
        else if (gc6plus_systemResetTest()){
            SetStatus(TEST_FAILED);
            getStateReport()->runFailed(" Reset test failed\n");
            return;
        }
    }
    if (arm_bsi_cya_gc6_mode_check == 1){
       /* InfoPrintf("Starting Wakeup Timer Test with BSI arm with CYA mode\n");
        if (gc6plus_wakeupTest(false, true)){
            SetStatus(TEST_FAILED);
            getStateReport()->runFailed("Wakeup Timer Test with BSI arm with CYA mode failed\n");
            return;
        }
        InfoPrintf("Wakeup Timer test with BSI arm with CYA mode Done!\n");*/
    }
    if (fundamental_reset_check == 1){
        InfoPrintf("Starting Fundamental Reset Test\n");
        if (gc6plus_fundamentalResetTest()){
            SetStatus(TEST_FAILED);
            getStateReport()->runFailed("Fundamental Reset test failed\n");
            return;
        }
        InfoPrintf("Fundamental Reset test Done!\n");
    }
    if (software_reset_check == 1){
        InfoPrintf("Starting Software Reset Test\n");
        if (gc6plus_softwareResetTest()){
            SetStatus(TEST_FAILED);
            getStateReport()->runFailed("Software Reset test failed\n");
            return;
        }
        InfoPrintf("Software Reset test Done!\n");
    }   
    //Enable when ready
    /*
    if (multiple_wakeups == 1){
        if (multiple_wakeupsTest()){
            SetStatus(TEST_FAILED);
            getStateReport()->runFailed("multiple wakeups  test failed\n");
            return;
        }
        InfoPrintf("multiple_wakeup Reset test Done!\n");
    } */           
    if (bsi2pmu_bar0_check == 1) {
        //if(gc6plus_bsi2pmu_bar0_maxwellTest()) {
        if(gc6plus_bsi2pmu_bar0_maxwellTest(bsiRamFileName)) {
            SetStatus(TEST_FAILED);
            getStateReport()->runFailed("gc6plus_bsi2pmu_bar0_maxwell test failed\n");
            return;
        }
        InfoPrintf("gc6plus_bsi2pmu_maxwellTest() Done !\n");
    }
    InfoPrintf("gc6plus_maxwell: Test completed with %d miscompares\n", m_ErrCount);
    if (m_ErrCount)
    {
        SetStatus(TEST_FAILED);
        getStateReport()->runFailed("Test failed with miscompares, see logfile for messages.\n");
        return;
    }

    InfoPrintf("Finishing gc6plus_maxwell test\n");
    SetStatus(TEST_SUCCEEDED); 
    InfoPrintf("Exiting the GC6plus_Maxwell::Run() Routine\n");
}
 
//****************************************************************************************
//  Test therm access through I2C in steady state
//****************************************************************************************

UINT32 GC6plus_Maxwell::gc6plus_i2ctherm_Test() {
  
    DebugPrintf("checkSanity() starting ....\n");
  
    UINT32   expect_value;
    UINT32   i2c_rdat;
    GpuSubdevice *pSubdev = lwgpu->GetGpuSubdevice();
    UINT32  i2c_reg_adr, i2c_slv_adr;
    
    UINT32 wr_val;
    UINT32 rd_val  = 0;
    string regName;
 
    RETURN_IF_ERR(this->gc6plus_init(pSubdev));
    
    //GpuSubdevice *pSubdev = lwgpu->GetGpuSubdevice();
    i2c_slv_adr = DEVICE_ADDR_0_FERMI;
    i2c_reg_adr = 0xFE; //for therm
    //i2c_slv_adr = 0x4D; // for SCI
    //i2c_reg_adr = 0x40; //for SCI
    
    EscapeWrite("drv_opt_i2cs_addr_sel", 0, 1, 0x0);
    DebugPrintf("Done with Escape write\n");
    //expect_value = lwgpu->RegRd32(LW_LWI2CS_SENSOR_FE);
    expect_value = 0xDE; // Value stored in 0xFE register (dev_i2cs.ref) which is the device id 
    DebugPrintf("expected value 0x%08x\n",expect_value);
    
    gc6plus_i2cRdByte(i2c_slv_adr,i2c_reg_adr,&i2c_rdat,0,pSubdev, 1); // slave addr= 0x9E, reg. addr = 0xFE, 0 = read byte
    DebugPrintf("I2C_Addr DEV_ADDR_0  read back value 0x%08x\n",i2c_rdat);
    if ( i2c_rdat != expect_value) { 
        ErrPrintf("DEV_ADDR_0 -- read data mismatch, expect 0x%08x , got 0x%08x\n", expect_value,i2c_rdat);
        return (1);
    }

    if(priv_access_check){
        //Begin of BSI pri access to test islands being disabled using straps and fuses
        regName = "LW_PGC6_BSI_SCRATCH";
        wr_val = 0x55555555;
        WrRegArray(0, regName.c_str(), wr_val);
        rd_val = RdRegArray(0, regName.c_str());
        DebugPrintf("Read BSI priv access value: 0x%08x\n",rd_val);
        int temp_val = rd_val >> 16;
        //if ( rd_val != 0xbadf1002 ) {
        //Making it only checks for 0xbadf, because the error code seem to change per chip generation 
        if ( temp_val != 0xbadf ) {
            ErrPrintf("%s read ERROR: Expected 0x%08x, Got 0x%08x\n",
                      regName.c_str(), 0xbad, temp_val);
            return 1;
        }
    }

    return 0;     
}  
//****************************************************************************************


//****************************************************************************************
//  Test GC6 exit due to I2C event
//****************************************************************************************

UINT32 GC6plus_Maxwell::gc6plus_gc6_exit_i2c_Test() {
    unique_ptr<IRegisterClass> reg;
    UINT32 reg_addr;
    UINT32 reg_data;

    UINT32   expect_value;
    UINT32   i2c_rdat;
    GpuSubdevice *pSubdev = lwgpu->GetGpuSubdevice();
    UINT32  i2c_reg_adr, i2c_slv_adr;
    i2c_slv_adr = DEVICE_ADDR_0_FERMI;
    i2c_reg_adr = 0xFE; //for therm
    
    int wakeup_timer_us = 500; //Delay is large as we don't expect islands to exit on wakeup timer. This is just a fall back exiting option so that the test does not hang.

    RETURN_IF_ERR(this->gc6plus_init(pSubdev));
    //Enable i2c event Wakeup
    ReadWriteRegFieldVal(pSubdev, m_regMap, "LW_PGC6_SCI_WAKE_EN", "_I2CS", "_ENABLE");
    
    Find_Register(m_regMap, reg, "LW_PGC6_SCI_GPU_I2CS_CFG");
    reg_addr = reg->GetAddress();
    reg_data = pSubdev->RegRd32(reg_addr);
    reg_data = ModifyFieldValWithData(reg.get(), reg_data, "LW_PGC6_SCI_GPU_I2CS_CFG", "_I2C_ADDR", DEVICE_ADDR_0_FERMI);
    reg_data = ModifyFieldVal(reg.get(), reg_data, "LW_PGC6_SCI_GPU_I2CS_CFG", "_SCL_TIMEOUT_SCALE", "_1MSEC");
    pSubdev->RegWr32(reg_addr, reg_data);

    //Making THERM I2CS state INVALID 
    Find_Register(m_regMap, reg, "LW_PGC6_SCI_GPU_I2CS_REPLAY");
    reg_addr = reg->GetAddress();
    reg_data = pSubdev->RegRd32(reg_addr);
    reg_data = ModifyFieldVal(reg.get(), reg_data, "LW_PGC6_SCI_GPU_I2CS_REPLAY", "_THERM_I2CS_STATE", "_ILWALID");
    pSubdev->RegWr32(reg_addr, reg_data);
    DebugPrintf("Setting Therm I2CS Replay State to Invalid 0x%x\n",reg_data);
 
    //GCX entry after setting up wakeup timer
    this->configure_wakeup_timer(pSubdev, wakeup_timer_us);
    this->wakeupTimerEvent(pSubdev, "enable");
    //Enter GC5/GC6
    this->gc6plus_triggerMode(pSubdev);

    Platform::Delay(10); //delay for 10us for GC6 to kick in 
    
    //Trigger I2C event         
    Platform::EscapeWrite("drv_opt_i2cs_addr_sel", 0, 2, 0x0);
    DebugPrintf("Done with Escape write\n");
    
    expect_value = 0xDE; // Value stored in 0xFE register (dev_i2cs.ref) which is the device id 
    gc6plus_i2cRdByte(i2c_slv_adr,i2c_reg_adr,&i2c_rdat,0, pSubdev, 1); // slave addr= 0x4F, reg. addr = 0xFE, 0 = read byte
   

    if ( i2c_rdat != expect_value) { 
        ErrPrintf("DEV_ADDR_0 -- read data mismatch, expect 0x%08x , got 0x%08x\n", expect_value,i2c_rdat);
        return (1);
    }
    
    return ((
            checkSteadyState(pSubdev) //we are now in steady state
            && check_sw_intr_status(pSubdev) 
            ) ? 0: 1);
}  

//****************************************************************************************

//****************************************************************************************
//  Test GC6 abort due to I2C event
//****************************************************************************************

UINT32 GC6plus_Maxwell::gc6plus_gc6_abort_i2c_Test() {
    unique_ptr<IRegisterClass> reg;
    unique_ptr<IRegisterField> field;
    UINT32 reg_addr;
    UINT32 reg_data;

    UINT32   expect_value;
    UINT32   i2c_rdat;
    GpuSubdevice *pSubdev = lwgpu->GetGpuSubdevice();
    UINT32  i2c_reg_adr, i2c_slv_adr;
    i2c_slv_adr = DEVICE_ADDR_0_FERMI;
    i2c_reg_adr = 0xFE; //for therm
    
    int wakeup_timer_us = 2000;//Delay is large as we don't expect islands to exit on wakeup timer. This is just a fall back exiting option so that the test does not hang. 
    RETURN_IF_ERR(this->gc6plus_init(pSubdev));
    //Enable i2c event Wakeup
    ReadWriteRegFieldVal(pSubdev, m_regMap, "LW_PGC6_SCI_WAKE_EN", "_I2CS", "_ENABLE");
    
    Find_Register(m_regMap, reg, "LW_PGC6_SCI_GPU_I2CS_CFG");
    reg_addr = reg->GetAddress();
    reg_data = pSubdev->RegRd32(reg_addr);
    reg_data = ModifyFieldValWithData(reg.get(), reg_data, "LW_PGC6_SCI_GPU_I2CS_CFG", "_I2C_ADDR", DEVICE_ADDR_0_FERMI);
    reg_data = ModifyFieldVal(reg.get(), reg_data, "LW_PGC6_SCI_GPU_I2CS_CFG", "_SCL_TIMEOUT_SCALE", "_1MSEC");
    pSubdev->RegWr32(reg_addr, reg_data);

    ////Making THERM I2CS state INVALID 
    Find_Register(m_regMap, reg,  "LW_PGC6_SCI_GPU_I2CS_REPLAY");
    reg_addr = reg->GetAddress();
    reg_data = pSubdev->RegRd32(reg_addr);
    reg_data = ModifyFieldVal(reg.get(), reg_data, "LW_PGC6_SCI_GPU_I2CS_REPLAY", "_THERM_I2CS_STATE", "_ILWALID");
    pSubdev->RegWr32(reg_addr, reg_data);
    DebugPrintf("Setting Therm I2CS Replay State to Invalid 0x%x\n",reg_data);

    //Hardcode the power sequencer to have a long delay using delay_table entry b for the GC5/6 entry so that the I2C transaction can hit the abort window.
    // Lwrrently at 50 us, i2c transaction takes around 16 us to finish  
        
    Find_Register(m_regMap, reg, "LW_PGC6_SCI_PWR_SEQ_ENTRY_VEC1");
    reg_addr = reg->GetAddress();
    UINT32 entry_vec = pSubdev->RegRd32(reg_addr);
    Find_Field(reg, field, "LW_PGC6_SCI_PWR_SEQ_ENTRY_VEC1_GC6_ENTRY");
    UINT32 temp_start_bit = field->GetStartBit();
    UINT32 temp_end_bit = field->GetEndBit();
    UINT32 temp_mask = (0x1 << (temp_end_bit - temp_start_bit + 1)) - 1;
    UINT32 temp_entry_vac = entry_vec;
    UINT32 gc6_entry_start = (temp_entry_vac >> temp_start_bit) & temp_mask; 
    DebugPrintf("gc6plus_maxwell: %s: PWR_SEQ_ENTRY_VEC1 = 0x%08x, GC6 VEC start_bit = 0x%08x, end_bit = 0x%08x, mask = 0x%08x, gc6_entry_start = 0x%08x, shifted vec = 0x%08x \n",__FUNCTION__, entry_vec, temp_start_bit, temp_end_bit, temp_mask, gc6_entry_start, (temp_entry_vac >> temp_start_bit));
    
    Find_Field(reg, field, "LW_PGC6_SCI_PWR_SEQ_ENTRY_VEC1_GC5_ENTRY");
    temp_start_bit = field->GetStartBit();
    temp_end_bit = field->GetEndBit();
    temp_mask = (0x1 << (temp_end_bit - temp_start_bit + 1)) - 1;
    temp_entry_vac = entry_vec;
    UINT32 gc5_entry_start = (temp_entry_vac >> temp_start_bit) & temp_mask; 
    DebugPrintf("gc6plus_maxwell: %s: PWR_SEQ_ENTRY_VEC1 = 0x%08x, GC5 VEC start_bit = 0x%08x, end_bit = 0x%08x, mask = 0x%08x, gc5_entry_start = 0x%08x, shifted vec = 0x%08x \n",__FUNCTION__, entry_vec, temp_start_bit, temp_end_bit, temp_mask, gc5_entry_start, (temp_entry_vac >> temp_start_bit));

    Find_Register(m_regMap, reg, "LW_PGC6_SCI_PWR_SEQ_INST");
    reg_addr = reg->GetAddress(gc6_entry_start);
    pSubdev->RegWr32( reg_addr, 0x133b ); 
    Find_Register(m_regMap, reg, "LW_PGC6_SCI_PWR_SEQ_INST");
    reg_addr = reg->GetAddress(gc5_entry_start);
    pSubdev->RegWr32( reg_addr, 0x133b ); 
    
    //GCX entry after setting up wakeup timer
    this->configure_wakeup_timer(pSubdev, wakeup_timer_us);
    this->wakeupTimerEvent(pSubdev, "enable");
    //this->gc6plus_triggerMode(pSubdev);

    
    //Trigger I2C event         
    Platform::EscapeWrite("drv_opt_i2cs_addr_sel", 0, 2, 0x0);
    DebugPrintf("Done with Escape write\n");
    
    expect_value = 0xDE; // Value stored in 0xFE register (dev_i2cs.ref) which is the device id 
    gc6plus_i2cRdByte(i2c_slv_adr,i2c_reg_adr,&i2c_rdat,0, pSubdev, 1); // slave addr= 0x4F, reg. addr = 0xFE, 0 = read byte
    if ( i2c_rdat != expect_value) { 
        ErrPrintf("DEV_ADDR_0 -- read data mismatch, expect 0x%08x , got 0x%08x\n", expect_value,i2c_rdat);
        return (1);
    }
    
    //Cleanup functions (with priv_init)
    this->gc6plus_cleanup(pSubdev);
    return ((
            checkSteadyState(pSubdev) //we are now in steady state
            && check_sw_intr_status(pSubdev) 
            ) ? 0: 1);  
}  

//****************************************************************************************


//****************************************************************************************
//  Test I2C access in SCI 
//****************************************************************************************

UINT32 GC6plus_Maxwell::gc6plus_sci_i2c_Test() {
    unique_ptr<IRegisterClass> reg;
    UINT32 reg_addr;
    UINT32 reg_data;

    //UINT32   expect_value;
    UINT32   i2c_rdat;
    GpuSubdevice *pSubdev = lwgpu->GetGpuSubdevice();
    INT32  i2c_sci_slv_adr, reg_val;
    
    i2c_sci_slv_adr = 0x4b; //for sci
    reg_val = 0;

    int wakeup_timer_us = 1000; //delay set to 1000us

    RETURN_IF_ERR(this->gc6plus_init(pSubdev));
    //Enable i2c event Wakeup
    ReadWriteRegFieldVal(pSubdev, m_regMap, "LW_PGC6_SCI_WAKE_EN", "_I2CS", "_ENABLE");
    
    Find_Register(m_regMap, reg, "LW_PGC6_SCI_GPU_I2CS_CFG");
    reg_addr = reg->GetAddress();
    reg_data = pSubdev->RegRd32(reg_addr);
    reg_data = ModifyFieldValWithData(reg.get(), reg_data, "LW_PGC6_SCI_GPU_I2CS_CFG", "_I2C_ADDR", DEVICE_ADDR_0_FERMI);
    reg_data = ModifyFieldVal(reg.get(), reg_data, "LW_PGC6_SCI_GPU_I2CS_CFG", "_SCL_TIMEOUT_SCALE", "_1MSEC");
    pSubdev->RegWr32(reg_addr, reg_data);

    //GCX entry after setting up wakeup timer
    this->configure_wakeup_timer(pSubdev, wakeup_timer_us);
    this->wakeupTimerEvent(pSubdev, "enable");
    reg_val = ReadReg(pSubdev, m_regMap, "LW_PGC6_SCI_WAKE_EN");
    DebugPrintf("Value of LW_PGC6_SCI_WAKE_EN = 0x%x\n", reg_val);
    this->gc6plus_triggerMode(pSubdev);

    Platform::Delay(5); //delay for 5us for GC6 to kick in 
    
    //Trigger I2C event         
    Platform::EscapeWrite("drv_opt_i2cs_addr_sel", 0, 2, 0x0);
    DebugPrintf("Done with Escape write\n");
    
    
    Find_Register(m_regMap, reg, "LW_PGC6_SCI_WAKE_EN");
    reg_addr = reg->GetAddress();
    gc6plus_i2cWrByte(i2c_sci_slv_adr,0x50, reg_addr, 1); // slave addr= 0x4b, reg. addr = 0x50, 1 = write word
    gc6plus_i2cWrByte(i2c_sci_slv_adr,0x48, 0x10, 0); // slave addr= 0x4b, reg. addr = 0x48, 0 = write byte
    gc6plus_i2cRdByte(i2c_sci_slv_adr,0x48, &i2c_rdat, 0, pSubdev, 0); // slave addr= 0x4F, reg. addr = 0xFE, 0 = read byte
    gc6plus_i2cRdByte(i2c_sci_slv_adr,0x54, &i2c_rdat, 1, pSubdev, 0); // slave addr= 0x4F, reg. addr = 0xFE, 0 = read byte

    if (poll_for_fsm_state("STEADY", true))
    {
        ErrPrintf("gc6plus_maxwell: Error in polling for FSM state\n");
        m_ErrCount++;
        return (1);
    }
    this->gc6plus_cleanup(pSubdev);
       
    return i2c_rdat == (UINT32)reg_val ? 0 : 1;
}  

/*****************************************************************************************/

/*****************************************************************************************/
/****  GC6 simultaneous I2C and GPIO wakeup                                          *****/
/*****************************************************************************************/

UINT32 GC6plus_Maxwell::gc6plus_simultaneous_i2c_gpio_Test(){
    GpuSubdevice *pSubdev = lwgpu->GetGpuSubdevice();
    int additional_wakeup_events = 1;
    UINT32   expect_value;
    UINT32   i2c_rdat;
    UINT32  i2c_reg_adr, i2c_slv_adr;
    
    unique_ptr<IRegisterClass> reg;
    UINT32 reg_addr;
    UINT32 reg_data;

    //Init functions
    RETURN_IF_ERR(this->gc6plus_init(pSubdev));
    
    // Program any additional wakeup events (Lwrrently only does GPIO Misc wakeup)
    DebugPrintf("additional_wakeup_events %d\n", additional_wakeup_events);
    this->program_additional_wakeups(pSubdev, additional_wakeup_events);

    this->wakeupTimerEvent(pSubdev, "disable");
    
    this->printWakeEnables(pSubdev);
    this->getSwIntr0Status(pSubdev);
    this->getSwIntr1Status(pSubdev);

    i2c_slv_adr = DEVICE_ADDR_0_FERMI;
    i2c_reg_adr = 0xFE; //for therm    
    //Enable i2c event Wakeup
    ReadWriteRegFieldVal(pSubdev, m_regMap, "LW_PGC6_SCI_WAKE_EN", "_I2CS", "_ENABLE");
    
    Find_Register(m_regMap, reg, "LW_PGC6_SCI_GPU_I2CS_CFG");
    reg_addr = reg->GetAddress();
    reg_data = pSubdev->RegRd32(reg_addr);
    reg_data = ModifyFieldValWithData(reg.get(), reg_data, "LW_PGC6_SCI_GPU_I2CS_CFG", "_I2C_ADDR", DEVICE_ADDR_0_FERMI);
    reg_data = ModifyFieldVal(reg.get(), reg_data, "LW_PGC6_SCI_GPU_I2CS_CFG", "_SCL_TIMEOUT_SCALE", "_1MSEC");
    pSubdev->RegWr32(reg_addr, reg_data);

    ////Making THERM I2CS state INVALID 
    Find_Register(m_regMap, reg,  "LW_PGC6_SCI_GPU_I2CS_REPLAY");
    reg_addr = reg->GetAddress();
    reg_data = pSubdev->RegRd32(reg_addr);
    reg_data = ModifyFieldVal(reg.get(), reg_data, "LW_PGC6_SCI_GPU_I2CS_REPLAY", "_THERM_I2CS_STATE", "_ILWALID");
    pSubdev->RegWr32(reg_addr, reg_data);
    DebugPrintf("Setting Therm I2CS Replay State to Invalid 0x%x\n",reg_data);
 
    //Enter GC5/GC6
    this->gc6plus_triggerMode(pSubdev);
    Platform::Delay(10);
    // Delay until we are in GCx
    if (poll_for_fsm_state("STEADY", false))
    {
        ErrPrintf("gc6plus_maxwell: Error in polling for FSM state\n");
        m_ErrCount++;
        return (1);
    } 
        
    //Trigger I2C event         
    Platform::EscapeWrite("drv_opt_i2cs_addr_sel", 0, 2, 0x0);
    DebugPrintf("Done with Escape write\n");
    expect_value = 0xDE; // Value stored in 0xFE register (dev_i2cs.ref) which is the device id 
    gc6plus_i2cRdByte(i2c_slv_adr,i2c_reg_adr,&i2c_rdat,0, pSubdev, 1); // slave addr= 0x4F, reg. addr = 0xFE, 0 = read byte
    Platform::Delay(100);
    // Wait till we are in STEADY state
    if (poll_for_fsm_state("STEADY", true))
    {
        ErrPrintf("gc6plus_maxwell: Error in polling for FSM state\n");
        m_ErrCount++;
        return (1);
    }

    if ( i2c_rdat != expect_value) { 
        ErrPrintf("DEV_ADDR_0 -- read data mismatch, expect 0x%08x , got 0x%08x\n", expect_value,i2c_rdat);
        return (1);
    }
    
    //Cleanup functions (with priv_init)
    this->gc6plus_cleanup(pSubdev);
    
    return ((
            checkSteadyState(pSubdev) //we are now in steady state
            && check_sw_intr_status(pSubdev) 
            ) ? 0: 1);
};


//****************************************************************************************
// Read BSI ram contents from a text file and execute priv accesses
// also check if the accesses were successful
//****************************************************************************************

UINT32
GC6plus_Maxwell::gc6plus_bsiram_content(GpuSubdevice *dev,string &bsiRamFileName){
    string line, reg_val_str;
    string pattern_match;
    size_t found, pos;
    ifstream BsiRamFile;
    UINT32 reg_val;
    UINT32 return_reg_val;
    UINT32 reg_addr;
    unique_ptr<IRegisterClass> reg;

    BsiRamFile.open (bsiRamFileName.c_str());
    if (BsiRamFile.is_open()){
        while (BsiRamFile.good()){
            getline(BsiRamFile, line);
            pattern_match = "LW_PGC6_BSI_RAMCTRL";
            found = line.find(pattern_match);
            if(found != string::npos){
                DebugPrintf("gc6plus_bsi2pmu_maxwellTest() read ctrl line %s !\n",line.c_str());
                pos = line.find("0x");
                reg_val_str = line.substr(pos+2);
                DebugPrintf("gc6plus_bsi2pmu_maxwellTest() read ctrl substr %s !\n",reg_val_str.c_str());
                stringstream colwert (reg_val_str);
                colwert >> std::hex >> reg_val;
                Find_Register(m_regMap, reg, "LW_PGC6_BSI_RAMCTRL");
                reg_addr = reg->GetAddress();
                dev->RegWr32(reg_addr, reg_val);
                DebugPrintf("gc6plus_bsi2pmu_maxwellTest() wrote to bsictrl, %x !\n",reg_val);
                Find_Register(m_regMap, reg, "LW_PGC6_BSI_RAMCTRL");
                reg_addr = reg->GetAddress();
                return_reg_val = dev->RegRd32(reg_addr);
                if (return_reg_val != reg_val) {
                    ErrPrintf("Expected 0x%08x, Got 0x%08x\n", reg_val, return_reg_val);
                    return 1;
                } else {
                    DebugPrintf("gc6plus_bsi2pmu_maxwellTest() confirmed write to bsictrl, %x !\n",return_reg_val);
                }    
            }
            pattern_match = "LW_PGC6_BSI_RAMDATA";
            found = line.find(pattern_match);
            if(found != string::npos){
                DebugPrintf("gc6plus_bsi2pmu_maxwellTest() read data line %s !\n",line.c_str());
                pos = line.find("0x");
                reg_val_str = line.substr(pos+2);
                DebugPrintf("gc6plus_bsi2pmu_maxwellTest() read data substr %s !\n",reg_val_str.c_str());
                stringstream colwert (reg_val_str);
                colwert >> std::hex >> reg_val;
                Find_Register(m_regMap, reg, "LW_PGC6_BSI_RAMDATA");
                reg_addr = reg->GetAddress();
                dev->RegWr32(reg_addr, reg_val);
                DebugPrintf("gc6plus_bsi2pmu_maxwellTest() wrote to bsidata, %x !\n",reg_val);
                Find_Register(m_regMap, reg, "LW_PGC6_BSI_RAMDATA");
                reg_addr = reg->GetAddress();
                return_reg_val = dev->RegRd32(reg_addr);
                if (return_reg_val != reg_val) {
                    ErrPrintf("Expected 0x%08x, Got 0x%08x\n", reg_val, return_reg_val);
                    return 1;
                } else {
                    DebugPrintf("gc6plus_bsi2pmu_maxwellTest() confirmed write to bsidata, %x !\n",return_reg_val);
                }    
            }
        }
    } else {
        ErrPrintf("Could not open bsi ram text file\n");
        return 1;
        
    }
    return 0;
}
//****************************************************************************************
// Read BSI ram contents from a text file and execute priv accesses
// also check if the accesses were successful
//****************************************************************************************

UINT32
GC6plus_Maxwell::gc6plus_bsiram_content_autoincr(GpuSubdevice *dev,string &bsiRamFileName){
    string line, reg_val_str;
    string pattern_match;
    size_t found, pos;
    ifstream BsiRamFile;
    UINT32 reg_val;
    UINT32 reg_addr;
    unique_ptr<IRegisterClass> reg;
    UINT32 return_reg_val;

    BsiRamFile.open (bsiRamFileName.c_str());
    if (BsiRamFile.is_open()){
        while (BsiRamFile.good()){
            getline(BsiRamFile, line);
            pattern_match = "LW_PGC6_BSI_RAMCTRL";
            found = line.find(pattern_match);
            if(found != string::npos){
                DebugPrintf("gc6plus_bsi2pmu_maxwellTest() read ctrl line %s !\n",line.c_str());
                pos = line.find("0x");
                reg_val_str = line.substr(pos+2);
                DebugPrintf("gc6plus_bsi2pmu_maxwellTest() read ctrl substr %s !\n",reg_val_str.c_str());
                stringstream colwert (reg_val_str);
                colwert >> std::hex >> reg_val;
                Find_Register(m_regMap, reg, "LW_PGC6_BSI_RAMCTRL");
                reg_addr = reg->GetAddress();
                dev->RegWr32(reg_addr, reg_val);
                DebugPrintf("gc6plus_bsi2pmu_maxwellTest() wrote to bsictrl, %x !\n",reg_val);
                return_reg_val = dev->RegRd32(reg_addr);
                if (return_reg_val != reg_val) {
                    ErrPrintf("Expected 0x%08x, Got 0x%08x\n", reg_val, return_reg_val);
                    return 1;
                } else {
                    DebugPrintf("gc6plus_bsi2pmu_maxwellTest() confirmed write to bsictrl, %x !\n",return_reg_val);
                }    
            }
            pattern_match = "LW_PGC6_BSI_RAMDATA";
            found = line.find(pattern_match);
            if(found != string::npos){
                DebugPrintf("gc6plus_bsi2pmu_maxwellTest() read data line %s !\n",line.c_str());
                pos = line.find("0x");
                reg_val_str = line.substr(pos+2);
                DebugPrintf("gc6plus_bsi2pmu_maxwellTest() read data substr %s !\n",reg_val_str.c_str());
                stringstream colwert (reg_val_str);
                colwert >> std::hex >> reg_val;
                Find_Register(m_regMap, reg, "LW_PGC6_BSI_RAMDATA");
                reg_addr = reg->GetAddress();
                dev->RegWr32(reg_addr, reg_val);
                DebugPrintf("gc6plus_bsi2pmu_maxwellTest() wrote to bsidata, %x !\n",reg_val);
            }
        }
    } else {
        ErrPrintf("Could not open bsi ram text file\n");
        return 1;
        
    }
    return 0;
}
//****************************************************************************************
// Test GC6plus Island bsi2pmu test 
//****************************************************************************************
UINT32
GC6plus_Maxwell::gc6plus_bsi2pmu_maxwellTest(string &bsiRamFileName) {
    unique_ptr<IRegisterClass> reg;
    UINT32 reg_addr;
    UINT32 reg_data;
    UINT32 temp1;

    GpuSubdevice *pSubdev = lwgpu->GetGpuSubdevice();

    //-----------------------------------------------------------
    // Stage 1 : Initialization
    //----------------------------------------------------------
    DebugPrintf("Gc6plus bsi2pmu Test : Stage 1: Initialization\n");
  
    //check clamp_en and sleep signal to make sure that when the fuse is not blown
    //bool fuse_blown = false;
    //if(check_clamp_gc6plus(fuse_blown) == 1) return 1;
  
    // Clear PMU in Reset
    Find_Register(m_regMap, reg, "LW_PMC_ENABLE");
    reg_addr = reg->GetAddress();
    reg_data = pSubdev->RegRd32(reg_addr);
    pSubdev->RegWr32(reg_addr, reg_data | 0x0000002c ); // enable BUF_reset, xbar, l2, priv_ring
    pSubdev->RegRd32(reg_addr);

    Find_Register(m_regMap, reg, "LW_PPRIV_MASTER_RING_COMMAND_DATA");
    reg_addr = reg->GetAddress();
    pSubdev->RegWr32(reg_addr, 0x503B5443);
    
    PrivInit();
  
    Find_Register(m_regMap, reg, "LW_PMC_ENABLE");
    reg_addr = reg->GetAddress();
    reg_data = pSubdev->RegRd32(reg_addr);
    pSubdev->RegWr32(reg_addr, reg_data | 0xffffffff ); //Enable all engines after PRIV init is complete.
    pSubdev->RegRd32(reg_addr);
    DebugPrintf("gc6plus_maxwell: priv_init(): finished\n");
    
    if(GC6plus_Maxwell::gc6plus_bsiram_content(pSubdev,bsiRamFileName)){
       ErrPrintf("gc6plus_maxwell: %s : gc6plus_bsiram_content function failed to load BSI ram from txt file\n", __FUNCTION__);
        return 1;
    }

    // Writing a known value to a PMU priv reg to facilitate self-checking
    Find_Register(m_regMap, reg,"LW_PGC6_BSI_RAMCTRL");
    reg_addr = reg->GetAddress(); 
    pSubdev->RegWr32(reg_addr, 0x70);

    Find_Register(m_regMap, reg,"LW_PGC6_BSI_RAMDATA");
    reg_addr = reg->GetAddress(); 
    pSubdev->RegWr32(reg_addr, 0x20200); //PRIV ADDR

    Find_Register(m_regMap, reg,"LW_PGC6_BSI_RAMCTRL");
    reg_addr = reg->GetAddress(); 
    pSubdev->RegWr32(reg_addr, 0x74);
    temp1 = pSubdev->RegRd32(0x20200);
    DebugPrintf("gc6plus_bsi2pmu_maxwellTest() read from therm reg, %x !\n",temp1);
    
    Find_Register(m_regMap, reg,"LW_PGC6_BSI_RAMDATA");
    reg_addr = reg->GetAddress(); 
    pSubdev->RegWr32(reg_addr, temp1 | 0x1); //PRIV DATA
    DebugPrintf("gc6plus_bsi2pmu_maxwellTest() wrote to therm reg, %x !\n",(temp1 | 0x1));
    // Number of bootphases, LW_PGC6_BSI_BOOTPHASES_GC6STOP[19:15], now set as 2
    Find_Register(m_regMap, reg,"LW_PGC6_BSI_RAMCTRL");
    reg_addr = reg->GetAddress(); 
    pSubdev->RegWr32(reg_addr, 0x1 << 15 ); 
    
    //CYA Mode on
    Find_Register(m_regMap, reg,"LW_PGC6_BSI_CTRL");
    reg_addr = reg->GetAddress(); 
    pSubdev->RegWr32(reg_addr, (0x3)); 
    Platform::Delay(10); //1us
    //Trigger PMU transfer
    pSubdev->RegWr32(reg_addr, (0x1 << 16 | 0x3 | 0x1 << 8));
    Platform::Delay(10); //1us

    //Triggering the BSI_PMUTXTRIG (need the rising edge for RTL)
    Find_Register(m_regMap, reg,"LW_PGC6_BSI_PMUTXTRIG");
    reg_addr = reg->GetAddress(); 
    pSubdev->RegWr32(reg_addr, 0x0);
    Platform::Delay(10); //1us
    pSubdev->RegWr32(reg_addr, 0x1);

    temp1 = pSubdev->RegRd32(0x20200);
    DebugPrintf("gc6plus_bsi2pmu_maxwellTest() read from therm reg, %x !\n",temp1);
    temp1 = pSubdev->RegRd32(0x20200);
    DebugPrintf("gc6plus_bsi2pmu_maxwellTest() read from therm reg, %x !\n",temp1);
    temp1 = pSubdev->RegRd32(0x20200);
    DebugPrintf("gc6plus_bsi2pmu_maxwellTest() read from therm reg, %x !\n",temp1);
    temp1 = pSubdev->RegRd32(0x20200);
    DebugPrintf("gc6plus_bsi2pmu_maxwellTest() read from therm reg, %x !\n",temp1);

    return 0;
}

//****************************************************************************************

//****************************************************************************************
// Test GC6plus Island PRIV access .
//****************************************************************************************
UINT32 GC6plus_Maxwell::gc6plus_register_maxwellTest() {
    unique_ptr<IRegisterClass> reg;
    UINT32 reg_addr;
    UINT32 reg_data;

    UINT32 wr_val = 0x55555555;
    UINT32 rd_val = 0;
    UINT32 timeout_cnt1= 10;
    
    string regName;

    GpuSubdevice *pSubdev = lwgpu->GetGpuSubdevice();
  
   
    //Initialization
    InfoPrintf("GC6plus Register Test : Stage 1: Initialization\n");
  
    //check clamp_en and sleep signal to make sure that when the fuse is not blown
    //bool fuse_blown = false;
    //if(check_clamp_gc6plus(fuse_blown) == 1) return 1;
  
    // Clear PMU in Reset
    Find_Register(m_regMap, reg, "LW_PMC_ENABLE");
    reg_addr = reg->GetAddress();
    reg_data = pSubdev->RegRd32(reg_addr);
    pSubdev->RegWr32(reg_addr, reg_data | 0x0000002c ); // enable BUF_reset, xbar, l2, priv_ring
    pSubdev->RegRd32(reg_addr);

    Find_Register(m_regMap, reg, "LW_PPRIV_MASTER_RING_COMMAND_DATA");
    reg_addr = reg->GetAddress();
    pSubdev->RegWr32(reg_addr, 0x503B5443);
    
    PrivInit();
  
    Find_Register(m_regMap, reg, "LW_PMC_ENABLE");
    reg_addr = reg->GetAddress();
    reg_data = pSubdev->RegRd32(reg_addr);
    pSubdev->RegWr32(reg_addr, reg_data | 0xffffffff ); //Enable all engines after PRIV init is complete.
    pSubdev->RegRd32(reg_addr);
    DebugPrintf("gc6plus_maxwell: priv_init(): finished\n");
  
    if(register_check)
    {  
        if(randomize_regs ){
            //FIXME: REPLACE with gc6+ priv access
            m_rndStream = new RandomStream(m_seed, 0x222, 0x333);
            m_rndStream->RandomUnsigned(0x0, 0xFFFFFFFF);
     
        }
        else{
            //Begin of BSI pri access
            regName = "LW_PGC6_BSI_SCRATCH";
            wr_val = 0x55555555;
            WrRegArray(0, regName.c_str(), wr_val);
            rd_val = RdRegArray(0, regName.c_str());
            if ( rd_val != wr_val ) {
                ErrPrintf("%s read ERROR: Expected 0x%08x, Got 0x%08x\n",
                          regName.c_str(), wr_val, rd_val);
                return 1;
            }
            
            regName = "LW_PGC6_BSI_SCRATCH";
            wr_val = 0xAAAAAAAA;
            WrRegArray(0, regName.c_str(), wr_val);
            rd_val = RdRegArray(0, regName.c_str());
            if ( rd_val != wr_val ) {
                ErrPrintf("%s read ERROR: Expected 0x%08x, Got 0x%08x\n",
                          regName.c_str(), wr_val, rd_val);
                return 1;
            }
            //End of BSI pri access
     
            //Begin of SCI pri access 
            wr_val = 0x55555555;
            Find_Register(m_regMap, reg, "LW_PGC6_SCI_SPARE");
            reg_addr = reg->GetAddress();
            pSubdev->RegWr32( reg_addr,   wr_val);
            WriteReadCheckVal( reg_addr,  0xff, "LW_PGC6_SCI_SPARE,", wr_val   );

            wr_val = 0xAAAAAAAA;
            pSubdev->RegWr32( reg_addr,   wr_val);
            WriteReadCheckVal( reg_addr,  0xff, "LW_PGC6_SCI_SPARE,", wr_val   );
            //End of SCI pri access

        }
    }


  if (rpg_pri_error_check)
{
    //   setting to DWORD address mode
    Find_Register(m_regMap, reg,"LW_PGC6_BSI_HIGH_SELWRE_FEATURES");
    reg_addr = reg->GetAddress();
    pSubdev->RegWr32(reg_addr, 0x5);

    InfoPrintf("GC6plus Register Test : BSI RAM RPG- Putting bank0 in PG\n");

    // write to PGCTRL to put bank 0 in PG
    Find_Register(m_regMap, reg,"LW_PGC6_BSI_RAM_PGCTRL");
    reg_addr = reg->GetAddress();
    pSubdev->RegWr32(reg_addr, 0x2);

    Platform::Delay(100);

    //  read lower bank in PG  to confirm badf5046 is returned  
    InfoPrintf("GC6plus Register Test : Accessing Bank 0 in PG to get pri error\n");

    Find_Register(m_regMap, reg,"LW_PGC6_BSI_RAMCTRL");
    reg_addr = reg->GetAddress();
    pSubdev->RegWr32(reg_addr, 0x0);

    
    Find_Register(m_regMap, reg,"LW_PGC6_BSI_RAMDATA");
    reg_addr = reg->GetAddress();
    reg_data = pSubdev->RegRd32(reg_addr);
    regName = "LW_PGC6_BSI_RAMDATA";
    DebugPrintf("%s reading LW_PGC6_BSI_RAMDATA reg. Got 0x%08x\n", 
    regName.c_str(), reg_data);
	

    while(reg_data != 0xbadf5046)
    {
		if(timeout_cnt1==0)
		{
                ErrPrintf("%s read ERROR: Expected 0x%08x, Got 0x%08x\n", 
		regName.c_str(),0xbadf5046 , reg_data);
         	return 1;
		}
	 timeout_cnt1--;
	 Platform::Delay(1);
	 DebugPrintf("GC6plus Register Test: Polling on LW_PGC6_BSI_RAMDATA %d.\n",(10-timeout_cnt1));
    }
    Platform::Delay(100);

    //check RAM SIZE = 0x0 
    Find_Register(m_regMap, reg,"LW_PGC6_BSI_RAM");
    reg_addr = reg->GetAddress();
    reg_data = pSubdev->RegRd32(reg_addr);
    regName = "LW_PGC6_BSI_RAM";
	if(reg_data != 0x0)
	{
                ErrPrintf("%s read ERROR: Expected 0x%08x, Got 0x%08x\n", 
                regName.c_str(), 0x0, reg_data);

	}
        Platform::Delay(100);

    //write to PGCTRL to bring bank 0 out of PG
    InfoPrintf("GC6plus Register Test : BSI RAM RPG- Bringing bank 0 out of PG\n");
    Find_Register(m_regMap, reg,"LW_PGC6_BSI_RAM_PGCTRL");
    reg_addr = reg->GetAddress();
    pSubdev->RegWr32(reg_addr, 0x0);

    Platform::Delay(100);

    //check RAM SIZE = 0x40 to validate higher banks are in PG 
    Find_Register(m_regMap, reg,"LW_PGC6_BSI_RAM");
    reg_addr = reg->GetAddress();
    reg_data = pSubdev->RegRd32(reg_addr);
    regName = "LW_PGC6_BSI_RAM";
    if(reg_data != 0x40)
    {
          ErrPrintf("%s read ERROR: Expected 0x%08x, Got 0x%08x\n", 
          regName.c_str(), 0x40, reg_data);
    }
    Platform::Delay(100);

    //read higher bank in PG  to confirm badf5046 is returned  
    InfoPrintf("GC6plus Register Test : Accessing higher bank in PG to get pri error\n");
    Find_Register(m_regMap, reg,"LW_PGC6_BSI_RAMCTRL");
    reg_addr = reg->GetAddress();
    pSubdev->RegWr32(reg_addr, 0x65e2);

    
    Find_Register(m_regMap, reg,"LW_PGC6_BSI_RAMDATA");
    reg_addr = reg->GetAddress();
    reg_data = pSubdev->RegRd32(reg_addr);
    regName = "LW_PGC6_BSI_RAMDATA";

    timeout_cnt1=10;	
    while(reg_data != 0xbadf5046)
    {
	if(timeout_cnt1==0)
	{
          ErrPrintf("%s read ERROR: Expected 0x%08x, Got 0x%08x for higher bank in PG \n", 
regName.c_str(),0xbadf5046 , reg_data);
           return 1;
	}
	 timeout_cnt1--;
	 Platform::Delay(1);
	 DebugPrintf("GC6plus Register Test: Polling on LW_PGC6_BSI_RAMDATA %d.\n",(10-timeout_cnt1));
     }
        Platform::Delay(100);

}

    return 0;
}

//****************************************************************************************
// Test GC6plus Island PRIV access to empty BSI/SCI locations 
//****************************************************************************************
UINT32 GC6plus_Maxwell::gc6plus_reg_unmapped_maxwellTest() {
    unique_ptr<IRegisterClass> reg;
    UINT32 reg_addr;
    UINT32 reg_data;
  
    UINT32 wr_val  = 0x55555555;
    UINT32 rd_val  = 0;
    UINT32 rd_val1 = 0; 
    UINT32 rd_val2 = 0;
    string regName;

    GpuSubdevice *pSubdev = lwgpu->GetGpuSubdevice();
  
    //Initialization
    InfoPrintf("GC6plus Register Test : Stage 1: Initialization\n");
    
    //Clear PMU in Reset
    Find_Register(m_regMap, reg, "LW_PMC_ENABLE");
    reg_addr = reg->GetAddress();
    reg_data = pSubdev->RegRd32(reg_addr);
    pSubdev->RegWr32(reg_addr, reg_data | 0x0000002c ); // enable BUF_reset, xbar, l2, priv_ring
    pSubdev->RegRd32(reg_addr);

    Find_Register(m_regMap, reg, "LW_PPRIV_MASTER_RING_COMMAND_DATA");
    reg_addr = reg->GetAddress();
    pSubdev->RegWr32(reg_addr, 0x503B5443);
    
    PrivInit();
  
    Find_Register(m_regMap, reg, "LW_PMC_ENABLE");
    reg_addr = reg->GetAddress();
    reg_data = pSubdev->RegRd32(reg_addr);
    pSubdev->RegWr32(reg_addr, reg_data | 0xffffffff ); //Enable all engines after PRIV init is complete.
    pSubdev->RegRd32(reg_addr);
    DebugPrintf("gc6plus_maxwell: priv_init(): finished\n");
  
    if(register_unmapped_check){  
        if(randomize_regs ){
            //FIXME: REPLACE with gc6+ priv access
            m_rndStream = new RandomStream(m_seed, 0x222, 0x333);
            m_rndStream->RandomUnsigned(0x0, 0xFFFFFFFF);
     
        }
        else{
            //Begin of BSI pri access
            regName = "LW_PGC6_BSI_SCRATCH";
            wr_val = 0x55555555;
            WrRegArray(0, regName.c_str(), wr_val);
            rd_val = RdRegArray(0, regName.c_str());

            if ( rd_val != wr_val ) {
                ErrPrintf("%s read ERROR: Expected 0x%08x, Got 0x%08x\n",
                          regName.c_str(), wr_val, rd_val);
                return 1;
            }
            
            regName = "LW_PGC6_BSI_SCRATCH";
            wr_val = 0xAAAAAAAA;
            WrRegArray(0, regName.c_str(), wr_val);
            rd_val = RdRegArray(0, regName.c_str());

            if ( rd_val != wr_val ) {
                ErrPrintf("%s read ERROR: Expected 0x%08x, Got 0x%08x\n",
                          regName.c_str(), wr_val, rd_val);
                return 1;
            }
            //End of BSI pri access
     
            //Begin of SCI pri access 
            wr_val = 0x55555555;
            Find_Register(m_regMap, reg, "LW_PGC6_SCI_SPARE");
            reg_addr = reg->GetAddress();
            pSubdev->RegWr32(reg_addr ,   wr_val);
            InfoPrintf(" LW_PGC6_SCI_SPARE Address value: 0x%08x \n",reg_addr );
            m_ErrCount += WriteReadCheckVal( reg_addr,  0xff, "LW_PGC6_SCI_SPARE,", wr_val);


            wr_val = 0xAAAAAAAA;
            Find_Register(m_regMap, reg, "LW_PGC6_SCI_SPARE");
            reg_addr = reg->GetAddress();
            pSubdev->RegWr32( reg_addr,   wr_val);
            m_ErrCount += WriteReadCheckVal( reg_addr,  0xff, "LW_PGC6_SCI_SPARE,", wr_val);
            //End of SCI pri access
        
            //Writing to unmapped SCI locations
            wr_val = 0x55555555;
            pSubdev->RegWr32( 0x0010ebc8 ,   wr_val);
            wr_val = 0xAAAAAAAA;
            pSubdev->RegWr32( 0x0010ebcc ,   wr_val);

            //Reading from the same unmapped SCI locations
            rd_val1 = pSubdev->RegRd32( 0x0010ebc8);
            rd_val2 = pSubdev->RegRd32( 0x0010ebcc);
            InfoPrintf(" The two values read from unmapped SCI locations were 0x%08x and 0x%08x \n", rd_val1, rd_val2);
            
            if( rd_val1 != 0xbadf5040 || rd_val2 != 0xbadf5040){
                return 1;
                InfoPrintf(" ERROR: The two values read from unmapped SCI locations were 0x%08x and 0x%08x \n", rd_val1, rd_val2);
            }
            //Writing to unmapped BSI locations
            //FIXME : Find a scalable way to find unmapped region.
            InfoPrintf(" Writing to unmapped BSI location\n");
            wr_val = 0x55555555;
            pSubdev->RegWr32( 0x0010e610 ,   wr_val);
            Platform::Delay(10); //10us delay: waiting for PRI access timeout before next PRI access
            //Reading from the same unmapped BSI locations
            InfoPrintf(" Reading from unmapped BSI location\n");
            rd_val1 = pSubdev->RegRd32( 0x0010e610);
            InfoPrintf(" The value read from unmapped BSI location is 0x%08x \n", rd_val1);
            Platform::Delay(10); //10us delay: waiting for PRI access timeout before next PRI access
            
            //GM10x returns badf1002 and in GM20x it returns 0xbadf5040
            if( (rd_val1 != 0xbadf1002 ) && (rd_val1 != 0xbadf5040)){
                InfoPrintf("ERROR: The value read from unmapped BSI location is 0x%08x \n", rd_val1);
                return 1;
            }

           //Begin of SCI pri access 
            wr_val = 0x55555555;
            Find_Register(m_regMap, reg, "LW_PGC6_SCI_SPARE");
            pSubdev->RegWr32(reg_addr ,   wr_val);
            m_ErrCount += WriteReadCheckVal( reg_addr,  0xff, "LW_PGC6_SCI_SPARE,", wr_val);

            wr_val = 0xAAAAAAAA;
            pSubdev->RegWr32(reg_addr,   wr_val);
            m_ErrCount += WriteReadCheckVal( reg_addr,  0xff, "LW_PGC6_SCI_SPARE,", wr_val);
            //End of SCI pri access

            //Begin of BSI pri access
            regName = "LW_PGC6_BSI_SCRATCH";
            wr_val = 0x55555555;
            WrRegArray(0, regName.c_str(), wr_val);
            rd_val = RdRegArray(0, regName.c_str());

            if ( rd_val != wr_val ) {
                ErrPrintf("%s read ERROR: Expected 0x%08x, Got 0x%08x\n",
                          regName.c_str(), wr_val, rd_val);
                return 1;
            }
            
            regName = "LW_PGC6_BSI_SCRATCH";
            wr_val = 0xAAAAAAAA;
            WrRegArray(0, regName.c_str(), wr_val);
            rd_val = RdRegArray(0, regName.c_str());

            if ( rd_val != wr_val ) {
                ErrPrintf("%s read ERROR: Expected 0x%08x, Got 0x%08x\n",
                          regName.c_str(), wr_val, rd_val);
                return 1;
            }
            //End of BSI pri access
         
        }
    }

    return 0;
}

UINT32 GC6plus_Maxwell::gc6plus_ptimerTest(){
    unique_ptr<IRegisterClass> reg;
    UINT32 reg_addr;
    UINT32 reg_data;

    UINT32 rd_val1, rd_val2, rd_val3;
    GpuSubdevice *pSubdev = lwgpu->GetGpuSubdevice();
    //Init functions
    RETURN_IF_ERR(this->gc6plus_init(pSubdev));

    //Enable PTIMER (should already be enabled)
    WriteRegFieldVal(pSubdev, m_regMap, "LW_PGC6_SCI_PTIMER_ENABLE", "_CTL", "_YES");
    Platform::Delay(1);

    //Write to the Trigger to Read
    Find_Register(m_regMap, reg, "LW_PGC6_SCI_PTIMER_READ");
    reg_addr = reg->GetAddress();
    reg_data = ModifyFieldVal(reg.get(), 0, "LW_PGC6_SCI_PTIMER_READ", "_SAMPLE", "_TRIGGER");
    pSubdev->RegWr32(reg_addr, reg_data);

    Find_Register(m_regMap, reg, "LW_PGC6_SCI_PTIMER_TIME_0");
    reg_addr = reg->GetAddress();
    rd_val1 = pSubdev->RegRd32(reg_addr);
    Platform::Delay(15);

    //Write to the Trigger to Read
    Find_Register(m_regMap, reg, "LW_PGC6_SCI_PTIMER_READ");
    reg_addr = reg->GetAddress();
    reg_data = ModifyFieldVal(reg.get(), 0, "LW_PGC6_SCI_PTIMER_READ", "_SAMPLE", "_TRIGGER");
    pSubdev->RegWr32(reg_addr, reg_data);

    Find_Register(m_regMap, reg, "LW_PGC6_SCI_PTIMER_TIME_0");
    reg_addr = reg->GetAddress();
    rd_val2 = pSubdev->RegRd32(reg_addr);
    
    InfoPrintf(" The two values read were 0x%08x and 0x%08x \n",
         rd_val1, rd_val2);
    rd_val3 = rd_val2 - rd_val1 ;
    rd_val3 = rd_val3 * 32 ;
    InfoPrintf(" The difference between the two is  %d ns \n",
         rd_val3);

    if ( rd_val1 == rd_val2 ) {
        ErrPrintf("PTimer read ERROR: First read 0x%08x, Second read 0x%08x\n",
          rd_val1, rd_val2);
        return 1;
    }
    
    return 0;
}
//TODO: rename to reflect only GPIO wakeups
int GC6plus_Maxwell::program_additional_wakeups(GpuSubdevice *pSubdev, 
                                                 int additional_wakeup_events=0){
    DebugPrintf("programming %d additional wakeups\n", additional_wakeup_events);
    if (additional_wakeup_events == 0)
        return 1; 
    if (is_fmodel){
        WriteRegFieldVal(pSubdev, m_regMap, "LW_PGC6_SCI_DEBUG_SW_INTR1_FRC", "_MISC_0", "_ENABLE");
        WriteRegFieldVal(pSubdev, m_regMap, "LW_PGC6_SCI_DEBUG_SW_INTR_CTRL", "_FORCE_DURING", "_GC5_GC6");
    }
    else{
        //Enable Rise and Fall Wake on GPIO MISC_0
        ReadWriteRegFieldVal(pSubdev, m_regMap, "LW_PGC6_SCI_WAKE_RISE_EN", "_MISC_0", "_ENABLE");
        ReadWriteRegFieldVal(pSubdev, m_regMap, "LW_PGC6_SCI_WAKE_FALL_EN", "_MISC_0", "_ENABLE");
        UINT32 check_data;
        std::stringstream sstm; 
        sstm << "Gpio_" <<  gpio_index_misc[0] << "_drv";
        string s_esc = sstm.str();
        char* gpio_drv_escape = new char[s_esc.size() + 1];
        strcpy(gpio_drv_escape, s_esc.c_str());

        EscapeWrite(gpio_drv_escape, 0, 4, GPIO_DRV_FULL); // Program GPIO_DRV_FULL, GPIO engine is the sole driver 
        EscapeRead(gpio_drv_escape, 0, 4, &check_data);
        if (check_data != GPIO_DRV_FULL){
            ErrPrintf("GC6plus_Maxwell: Failure in EscapeWrite of %s. Wrote %d, "
                      "Read %d\n", gpio_drv_escape, GPIO_DRV_FULL, check_data);
        }
        else{
            DebugPrintf("GC6plus_Maxwell: EscapeWrite of %s. Wrote %d, "
                       "Read %d\n", gpio_drv_escape, GPIO_DRV_FULL, check_data);
        }
        //
        // assertGpio(pSubdev, "misc", 0);    
    }
    return (0);
}



//the function write_read_chk modifies certain bits of the reg using the and_mask and or_mask and then reads back the modified data to check if the read data is equal to the modified data.

int GC6plus_Maxwell::write_read_check(GpuSubdevice *pSubdev, char* regname, UINT32 and_mask, UINT32 or_mask)
{
    UINT32 read_data;
    UINT32 rd_data;
    unique_ptr<IRegisterClass> reg;
    UINT32 reg_addr;
    Find_Register(m_regMap, reg, regname);
    reg_addr = reg->GetAddress();
    rd_data = pSubdev->RegRd32(reg_addr);
    pSubdev->RegWr32(reg_addr, (rd_data & and_mask) | or_mask);
    read_data = pSubdev->RegRd32(reg_addr);
    if(read_data == ((rd_data & and_mask) | or_mask)){
        InfoPrintf("write_read_check() correct: Expected 0x%08x Got 0x%08x \n", (rd_data & and_mask) | or_mask, read_data);
        return 0;
    }
    else{
        ErrPrintf("write_read_check() ERROR: Expected 0x%08x Got 0x%08x \n", (rd_data & and_mask) | or_mask, read_data);
        return 1;
    }
}

//the function check_rmw reads the register "reg" (i.e. read_val) and then checks whether the read value is equal to expected value or not. the check is done in following two steps. Here we are checking only the fields which are modified by or_mask and and_mask. The iff_ifr value will tell if the check is for IFF or IFR i.e. if iff_ifr value is 1 then it means the check is for IFF otherwise IFR.
//1. The write to register(as Part of IFF/IFR) was final_write = (read_value & and_mask) | or_mask;
// Thus, fields which are set in or_mask will reflect directly in final value. So, if (read_val & or_mask) != or_mask then its an error for IFF but not in case of IFR.
//2. Ignoring bits which are already checked by or_mask. If and_mask is having few zeros then the final value will have zeros at the same place provided corresponding or_mask is zero. So, if (~(and_mask | or_mask)) & read_val) != 0x0 then its an error for IFF but not in case of IFR.
int GC6plus_Maxwell::check_rmw(GpuSubdevice *pSubdev,char* regname,UINT32 and_mask,UINT32 or_mask,int iff_ifr)
{
    UINT32 read_val;
    unique_ptr<IRegisterClass> reg;
    UINT32 reg_addr;
    Find_Register(m_regMap, reg, regname);
    reg_addr = reg->GetAddress();
    read_val = pSubdev->RegRd32(reg_addr);
    if((read_val & or_mask) != or_mask){
        ErrPrintf("check_rmw() Error: Read value 0x%08x is not matching with expected based on or_mask 0x%08x ", read_val, or_mask);
        // for the case iff_ifr = 1 i.e. IFF, if (read_val & or_mask) != or_mask then it means read value and expected value are not same for IFF and thus for this error the value to be returned is 1 = iff_ifr.
        // for the case iff_ifr = 0 i.e. IFR, if (read_val & or_mask) != or_mask then it means read value and expected value are not same for IFR and thus in this case returned value is 0 = iff_ifr.
        return iff_ifr;       
    }
    if( ((~(and_mask | or_mask)) & read_val) != 0x0){
        ErrPrintf("check_rmw() Error: Read value 0x%08x is not matching with expected. or_mask 0x%08x, and_mask 0x%08x ", read_val, or_mask, and_mask);
        return iff_ifr;
    }
    return !(iff_ifr);

}

//the function check_dw checks whether read value and expected value are equal or not. If not then it will return 1(i.e. error).
//The iff_ifr value will tell if the check is for IFF or IFR i.e. if iff_ifr value is 1 then it means the check is for IFF otherwise IFR.
int GC6plus_Maxwell::check_dw(GpuSubdevice *pSubdev, char* regname, UINT32 expected_val,int iff_ifr)
{
    UINT32 read_val;
    unique_ptr<IRegisterClass> reg;
    UINT32 reg_addr;
    Find_Register(m_regMap, reg, regname);
    reg_addr = reg->GetAddress();
    read_val = pSubdev->RegRd32(reg_addr);
    if(read_val != expected_val)
    {
        ErrPrintf("check_dw() Error: Read value 0x%08x is not matching with expected value 0x%08x", read_val, expected_val);
        // for the case iff_ifr = 1 i.e. IFF, if read_val != expected_val then it means read value and expected value are not same for IFF and thus for this error the value to be returned is 1 = iff_ifr.
        // for the case iff_ifr = 0 i.e. IFR, if read_val != expected_val then it means read value and expected value are not same for IFR and thus in this case returned value is 0 = iff_ifr.
        return iff_ifr;
    }
    return !(iff_ifr);
}    

UINT32 GC6plus_Maxwell::gc6plus_wakeupTest(bool additional_wakeup_events,
                                           bool timer_wakeup_enabled){
    string regname;
    unique_ptr<IRegisterClass> reg;
    UINT32 reg_addr;
    UINT32 reg_data;

    //int wakeup_timer_us = 20;
    GpuSubdevice *pSubdev = lwgpu->GetGpuSubdevice();

    UINT32 iff_done ;

    //Init functions
    RETURN_IF_ERR(this->gc6plus_init(pSubdev));
    
    // Program any additional wakeup events (Lwrrently only does GPIO Misc wakeup)
    DebugPrintf("additional_wakeup_events %d\n", additional_wakeup_events);
    this->program_additional_wakeups(pSubdev, additional_wakeup_events);

    // ARM wakeup timer
    if (timer_wakeup_enabled){
        this->configure_wakeup_timer(pSubdev, wakeup_timer_us);
        this->wakeupTimerEvent(pSubdev, "enable");
    }
    else{
        this->wakeupTimerEvent(pSubdev, "disable");
    }


    this->printWakeEnables(pSubdev);
    this->getSwIntr0Status(pSubdev);
    this->getSwIntr1Status(pSubdev);



    //IFF and IFR Related code
    InfoPrintf("!!! GC6 enter !!!\n");
    bool read_check_error = 1;   // to check the iff/ifr at gc6 exit.

    // Corrupting the registers before GC6 entry.
    if(initfromfuse_gc6_check == 1){
        if(IFF_write_check(pSubdev)){
            return 1;
        }
    }

    if(initfromrom_gc6_check == 1){
        if(IFR_write_check(pSubdev)){
            return 1;
        }
    }



    //Enter GC5/GC6
    this->gc6plus_triggerMode(pSubdev);

    if (additional_wakeup_events) {
        if (~is_fmodel){
            int gpio_delay  = (seq == EXIT)? ((wakeup_timer_us / 4) + 5)  : 1;
            Platform::Delay(gpio_delay);
            assertGpio(pSubdev, "misc", 0);
        }
        // FIXME: FMODEL GPIO abort is not handled here
    }

    // Delay until we are out of GCx
    if (poll_for_fsm_state("STEADY", false))
    {
        ErrPrintf("gc6plus_maxwell: Error in polling for FSM state\n");
        m_ErrCount++;
        return (1);
    }
    if (poll_for_fsm_state("STEADY", true))
    {
        ErrPrintf("gc6plus_maxwell: Error in polling for FSM state\n");
        m_ErrCount++;
        return (1);
    }

    //Cleanup functions (with priv_init)
    this->gc6plus_cleanup(pSubdev);

    //Check PRIV is working
    //SCI (LW_PGC6_SCI_PWR_SEQ_ENTRY_VEC3 is bits 31,5:0, resets to 0)
    Find_Register(m_regMap, reg, "LW_PGC6_SCI_PWR_SEQ_ENTRY_VEC3");
    reg_addr = reg->GetAddress();
    InfoPrintf("LW_PGC6_SCI_PWR_SEQ_ENTRY_VEC3 is 0x%x\n" ,pSubdev->RegRd32(reg_addr));
    pSubdev->RegWr32(reg_addr, 0x15);
    InfoPrintf("Writing 0x15 to LW_PGC6_SCI_PWR_SEQ_ENTRY_VEC3\nLW_PGC6_SCI_PWR_SEQ_ENTRY_VEC3 is 0x%x\n" ,ReadReg(pSubdev, m_regMap, "LW_PGC6_SCI_PWR_SEQ_ENTRY_VEC3"));
    //BSI (LW_PGC6_BSI_SCRATCH(0) is bits 31:0 resets to 0)
    InfoPrintf("LW_PGC6_BSI_SCRATCH(0) is 0x%x\n" ,ReadReg(pSubdev, m_regMap, "LW_PGC6_BSI_SCRATCH(0)"));
    Find_Register(m_regMap, reg, "LW_PGC6_BSI_SCRATCH(0)");
    reg_addr = reg->GetAddress();
    pSubdev->RegWr32(reg_addr,0x12345678);
    InfoPrintf("Writing 0x12345678 to LW_PGC6_BSI_SCRATCH(0)\nLW_PGC6_BSI_SCRATCH(0) is 0x%x\n" ,ReadReg(pSubdev, m_regMap, "LW_PGC6_BSI_SCRATCH(0)"));
    //PMU (LW_PPWR_PMU_DEBUG_TAG is bits 31:0 resets to 0)
    Find_Register(m_regMap, reg, "LW_PPWR_PMU_DEBUG_TAG");
    reg_addr = reg->GetAddress();
    UINT32 DEBUG_TAG = pSubdev->RegRd32(reg_addr);
    InfoPrintf("LW_PPWR_PMU_DEBUG_TAG is 0x%x\n" ,DEBUG_TAG);
    pSubdev->RegWr32(reg_addr, 0x90abcdef);
    reg_data = pSubdev->RegRd32(reg_addr);
    InfoPrintf("Writing 0x90abcdef to LW_PPWR_PMU_DEBUG_TAG\nLW_PPWR_PMU_DEBUG_TAG is 0x%x\n" ,reg_data);
    pSubdev->RegWr32(reg_addr, this->debug_tag_data);

    //print out statuses
    this->printSwIntrXStatus(pSubdev);
    this->getState(pSubdev);

    //Check how long we were in gcX for
    this->getResidentTimer(pSubdev);

    //IFF and IFR related code.
    InfoPrintf("!!! GC6 exit !!!");

    
    if(initfromfuse_gc6_check == 1 || initfromrom_gc6_check == 1 ){
        InfoPrintf("initializing GPU after GC6 exit");
        RETURN_IF_ERR(this->gc6plus_init(pSubdev));
        Find_Register(m_regMap, reg, "LW_XVE_BAR0");
        reg_addr = reg->GetAddress();
        lwgpu->CfgWr32(reg_addr,0xb8000000);    // setting the BAR0 registers

        regname = "LW_XVE_DEV_CTRL";
        Find_Register(m_regMap, reg, "LW_XVE_DEV_CTRL");
        reg_addr = reg->GetAddress();
        lwgpu->CfgWr32(reg_addr,0x100107);
    }
    
    // verifying IFF and IFR at GC6 exit
    if(initfromfuse_gc6_check == 1)
    {
        // make sure IFF exelwtion completes ,iff read check has to happen only after the iff exelwtion completes.
        if(lwgpu->GetRegFldNum("LW_PBUS_IFR_STATUS1","_IFF_DONE",&iff_done))
        {
            ErrPrintf("Couldn't get the reg LW_PBUS_IFR_STATUS1.\n");
            return (1);
        } 
        else 
        {
            InfoPrintf("Read 0x%x from LW_PBUS_IFR_STATUS1\n", iff_done);
        }

        while(iff_done == 0x0) 
        {
            ErrPrintf ("IFF is still exelwting\n");
            lwgpu->GetRegFldNum("LW_PBUS_IFR_STATUS1","_IFF_DONE",&iff_done);
            return (1);
        }
        
        InfoPrintf ("IFF exelwtion is over\n");
        if(IFF_read_check(pSubdev)){
            read_check_error = 0;
        }
    }

    if(initfromrom_gc6_check == 1){
        if(IFR_read_check(pSubdev)){
            read_check_error = 0;
        }
    }

    //Check for:
    return ((
               check_sw_intr_status(pSubdev) //Check Pending Wakeup Interrupts and GC5/GC6 Exit/Abort flags
            && checkSteadyState(pSubdev) //we are now in steady state
            && checkTimers(pSubdev) //Check GC5/GC6 Entry, Resident, and Exit Timers
            && checkDebugTag(pSubdev)
            && read_check_error  // check IFF/IFR at gc6 exit.
            ) ? 0: 1);
};

UINT32 GC6plus_Maxwell::gc6plus_systemResetCYATest(){
    GpuSubdevice *pSubdev = lwgpu->GetGpuSubdevice();
    unique_ptr<IRegisterClass> reg;
    UINT32 reg_addr;
    this->priv_init(pSubdev); 
    m_regMap = lwgpu->GetRegisterMap();
    if(!m_regMap)
    {
        ErrPrintf("GC6PLUS SPEEDY: smartfan can only be run on GPU's that support a " "register map!\n");
        return (1);
    }
    
    WriteRegFieldVal(pSubdev, m_regMap, "LW_FUSE_EN_SW_OVERRIDE", "_VAL", "_ENABLE");
    WriteRegFieldVal(pSubdev, m_regMap, "LW_FUSE_OPT_GC6_ISLAND_DISABLE", "_DATA", "__YES");
    reg = m_regMap->FindRegister("LW_THERM_PWM_ACT0");
    reg_addr = reg->GetAddress();
    InfoPrintf ("LW_THERM_PWM_ACT0_PERIOD = %0xd", lwgpu->RegRd32(reg_addr));
    Platform::Delay(5);
    InfoPrintf ("LW_THERM_PWM_ACT0_PERIOD = %0xd", lwgpu->RegRd32(reg_addr));
    return 0;
}

UINT32 GC6plus_Maxwell::gc6plus_systemResetTest(){
    GpuSubdevice *pSubdev = lwgpu->GetGpuSubdevice();
    UINT32 sci_fan_0, sci_fan_1;
    int extraPassChecks = 0;
    int resetFailed = 0;
    unique_ptr<IRegisterClass> reg;
    UINT32 reg_addr;
    UINT32 reg_data;

    // Init functions
    RETURN_IF_ERR(this->gc6plus_init(pSubdev));

    this->program_gpu_event_wake(pSubdev, true);
    
    // write to smartfan registerts
    Find_Register(m_regMap, reg, "LW_PGC6_SCI_FAN_CFG0");
    reg_addr = reg->GetAddress();
    ReadWriteVal(pSubdev, reg_addr, 0x1fff, 0x1234); 
    Find_Register(m_regMap, reg, "LW_PGC6_SCI_FAN_CFG1");
    reg_addr = reg->GetAddress();
    ReadWriteVal(pSubdev, reg_addr, 0x1fff, 0x1234); 

    //Capture Smart Fan Values
    sci_fan_0 = ReadReg(pSubdev, m_regMap, "LW_PGC6_SCI_FAN_CFG0");
    sci_fan_1 = ReadReg(pSubdev, m_regMap, "LW_PGC6_SCI_FAN_CFG1");
    DebugPrintf("LW_PGC6_SCI_FAN_CFG0 before reset = 0x%x\n", sci_fan_0);
    DebugPrintf("LW_PGC6_SCI_FAN_CFG1 before reset = 0x%x\n", sci_fan_1);
   
    //Write a known value to SCI's Volatile scratch register to see if fundamental reset happens.
    Find_Register(m_regMap, reg,"LW_PGC6_SCI_SCRATCH");
    reg_addr = reg->GetAddress();
    pSubdev->RegWr32(reg_addr, 0x00ff1234);
    DebugPrintf("gc6plus_maxwell: %s Writing 0x00ff1234 to LW_PGC6_SCI_SCRATCH \n", __FUNCTION__);

    Find_Register(m_regMap, reg,"LW_PGC6_BSI_SCRATCH(0)");
    reg_addr = reg->GetAddress();
    pSubdev->RegWr32(reg_addr, 0x00ff1234);
    DebugPrintf("gc6plus_maxwell: %s Writing 0x00ff1234 to LW_PGC6_BSI_SCRATCH(0) \n", __FUNCTION__);
 
    Platform::Delay(2);
  
    resetFailed = this->toggleSystemReset();

    Platform::Delay(5); //
 
    


    //Cleanup functions (with priv_init)
    this->gc6plus_cleanup(pSubdev);
    
    Find_Register(m_regMap, reg, "LW_PGC6_SCI_SCRATCH");
    reg_addr = reg->GetAddress();
    reg_data = pSubdev->RegRd32(reg_addr);
    DebugPrintf("gc6plus_maxwell: %s : Reading LW_PGC6_SCI_SCRATCH. \n");
    if( reg_data != 0x0){
        ErrPrintf("gc6plus_maxwell: %s : LW_PGC6_SCI_SCRATCH did not get reset, it reads 0x%08x", __FUNCTION__,reg_data);
        return 1;
    } else {
        DebugPrintf("gc6plus_maxwell: %s : LW_PGC6_SCI_SCRATCH got reset correctly by system reset. \n", __FUNCTION__);
    }
    Find_Register(m_regMap, reg,"LW_PGC6_BSI_SCRATCH(0)");
    reg_addr = reg->GetAddress();
    reg_data = ReadReg(pSubdev, m_regMap,"LW_PGC6_BSI_SCRATCH(0)");
    DebugPrintf("Reading LW_PGC6_BSI_SCRATCH(0). \n");
    if( reg_data != 0x0){
        ErrPrintf("gc6plus_maxwell: %s LW_PGC6_BSI_SCRATCH(0) did not get reset, it reads 0x%08x", __FUNCTION__, reg_data);
        return 1;
    } else {
        DebugPrintf("gc6plus_maxwell: %s LW_PGC6_BSI_SCRATCH(0) got reset correctly by system reset. \n", __FUNCTION__);
    } 
    
    //Check smartfan Registers are NOT reset
    sci_fan_0 = ReadReg(pSubdev, m_regMap, "LW_PGC6_SCI_FAN_CFG0");
    sci_fan_1 = ReadReg(pSubdev, m_regMap, "LW_PGC6_SCI_FAN_CFG1");
    DebugPrintf("LW_PGC6_SCI_FAN_CFG0 after reset = 0x%x\n", sci_fan_0);
    DebugPrintf("LW_PGC6_SCI_FAN_CFG1 after reset = 0x%x\n", sci_fan_1);
    
    UINT32 sci_tmr_sys_reset = ReadReg(pSubdev, m_regMap, "LW_PGC6_SCI_TMR_SYSTEM_RESET");
    DebugPrintf("LW_PGC6_SCI_TMR_SYSTEM_RESET after reset = 0x%x\n", sci_tmr_sys_reset);
        
    //Combine checks
    extraPassChecks = ((sci_tmr_sys_reset == LW_PGC6_SCI_TMR_SYSTEM_RESET_TIMEOUT__HWRESET_VAL)
                       && ((sci_fan_0 & 0x1fff) == 0x1234)
                       && ((sci_fan_1 & 0x1fff) == 0x1234)
                       && (resetFailed == 0));

    return (
            extraPassChecks
            ) ? 0: 1;
}

UINT32 GC6plus_Maxwell::gc6plus_intrGpuEventTest(int simultaneous_gpio_wakeup){
    GpuSubdevice *pSubdev = lwgpu->GetGpuSubdevice();
    int extraPassChecks = 1;
    UINT32 sci_fan_0, sci_fan_1;
    UINT32 sci_tmr_sys_reset;
    const char * statename;
    int resetFailed = 0;
    unique_ptr<IRegisterClass> reg;
    unique_ptr<IRegisterField> field;
    UINT32 reg_addr;

    InfoPrintf("gc6plus_maxwell: Entering gc6plus_intrGpuEventTest. gpu_event_clash_check = %d. sequence = %d\n\n",gpu_event_clash_check,this->seq);

    //Init functions
    RETURN_IF_ERR(this->gc6plus_init(pSubdev));

    this->program_gpu_event_wake(pSubdev, true);

    // Program any additional wakeup events
    // Lwrrently only does GPIO Misc wakeup 
    this->program_additional_wakeups(pSubdev, simultaneous_gpio_wakeup);

    if(gpu_event_clash_check){
        Find_Register(m_regMap, reg, "LW_PGC6_SCI_TMR_GPU_EVENT_HOLDOFF");
        reg_addr = reg->GetAddress();
        pSubdev->RegWr32(reg_addr, 1);
        
        Find_Register(m_regMap, reg, "LW_PGC6_SCI_TMR_GPU_EVENT_ASSERT");
        reg_addr = reg->GetAddress();
        pSubdev->RegWr32(reg_addr, 5);
        
        Find_Register(m_regMap, reg, "LW_PGC6_SCI_TMR_SYSTEM_RESET");
        reg_addr = reg->GetAddress();
        pSubdev->RegWr32(reg_addr, 20);
        this->configure_wakeup_timer(pSubdev, 50);
        this->wakeupTimerEvent(pSubdev, "enable");
    }
    
    if(this->seq == RESET){
        //Check smartfan registers
        DebugPrintf("LW_PGC6_SCI_FAN_CFG0 before reset = 0x%x\n",
            ReadReg(pSubdev, m_regMap, "LW_PGC6_SCI_FAN_CFG0"));
        DebugPrintf("LW_PGC6_SCI_FAN_CFG1 before reset = 0x%x\n",
            ReadReg(pSubdev, m_regMap, "LW_PGC6_SCI_FAN_CFG1"));
        
        //Write to smartfan registers
        Find_Register(m_regMap, reg, "LW_PGC6_SCI_FAN_CFG0");
        reg_addr = reg->GetAddress();
        pSubdev->RegWr32(reg_addr, 0x12000678);
        
        Find_Register(m_regMap, reg, "LW_PGC6_SCI_FAN_CFG1");
        reg_addr = reg->GetAddress();
        pSubdev->RegWr32(reg_addr, 0xdef);
        
        //Write non-default value to SCI_TMR_SYSTEM_RESET
        //FIXME: This is untested. Remove this fixme if -intr_pex_reset
        // (-gc5 && -gc6) -reset pass.
        
        Find_Register(m_regMap, reg, "LW_PGC6_SCI_TMR_SYSTEM_RESET"); 
        reg_addr = reg->GetAddress();
        pSubdev->RegWr32(reg_addr, 20);
    }

    //Print Status and Wakeup Registers
    this->printWakeEnables(pSubdev);
    this->printSwIntrXStatus(pSubdev);

    //Enter GC5/GC6
    this->gc6plus_triggerMode(pSubdev);


    // Perform EC functions for GPU_EVENT wake and system reset
    // RESET: force EC reset once in GC5/GC6 for X us
    // EXIT: force EC GPU_EVENT once in GC5/GC6 for X us
    // ABORT: force EC GPU_EVENT once in GC5/GC6_ENTRY_START for X us
    // simultaneous_gpio_wakeup : for EXIT only, assert misc0 wakeup X us
    //   after EC GPU_EVENT
    switch (this->seq)
    {
        case RESET :
            InfoPrintf("gc6plus_maxwell: Performing RESET sequence test\n");
            // wait until we reach GC5 or GC6 state before asserting reset
            if (this->mode == GC5)
            {
                statename = "GC5";
            }
            else if (this->mode == GC6)
            {
                statename = "GC6";
            }
            else
            {
                ErrPrintf("gc6plus_maxwell: Unknown mode %d in seq RESET\n",
                this->seq);
                return (1);
            }
            if (poll_for_fsm_state(statename, true))
            {
                ErrPrintf("gc6plus_maxwell: Error in polling for FSM state\n");
                m_ErrCount++;
                return (1);
            }


            resetFailed = this->toggleSystemReset();

            //Cleanup functions (with priv_init)
            this->gc6plus_cleanup(pSubdev);

            //Check smartfan Registers are NOT reset
            sci_fan_0 = ReadReg(pSubdev, m_regMap, "LW_PGC6_SCI_FAN_CFG0");
            sci_fan_1 = ReadReg(pSubdev, m_regMap, "LW_PGC6_SCI_FAN_CFG1");
            InfoPrintf("LW_PGC6_SCI_FAN_CFG0 after reset = 0x%x\n", sci_fan_0);
            InfoPrintf("LW_PGC6_SCI_FAN_CFG1 after reset = 0x%x\n", sci_fan_1);
            
            //Check other register(s) ARE reset
            sci_tmr_sys_reset = ReadReg(pSubdev, m_regMap, "LW_PGC6_SCI_TMR_SYSTEM_RESET");
            InfoPrintf("LW_PGC6_SCI_TMR_SYSTEM_RESET after reset = 0x%x\n",
                sci_tmr_sys_reset);

            //Combine checks
            extraPassChecks = ((sci_tmr_sys_reset ==
                LW_PGC6_SCI_TMR_SYSTEM_RESET_TIMEOUT__HWRESET_VAL)
                           && (sci_fan_0 == 0x12000678)
                           && (sci_fan_1 == 0xdef)
                           && resetFailed == 0);
            InfoPrintf("Reset extraPassChecks: %s\n",
                extraPassChecks ? "pass" : "fail");
            break;

        case EXIT :
            InfoPrintf("gc6plus_maxwell: Performing EXIT sequence test\n");
            // wait until we reach GC5 or GC6 state before asserting reset
            if (this->mode == GC5)
            {
                statename = "GC5";
            }
            else if (this->mode == GC6)
            {
                statename = "GC6";
            }
            else
            {
                ErrPrintf("gc6plus_maxwell: Unknown mode %d in seq RESET\n",
                this->seq);
                return (1);
            }
            if (poll_for_fsm_state(statename, true))
            {
                ErrPrintf("gc6plus_maxwell: Error in polling for FSM state\n");
                m_ErrCount++;
            }

            // Now assert GPU_EVENT wake from EC
            InfoPrintf("gc6plus_maxwell: Asserting GPU_EVENT wake from EC\n");
            Platform::EscapeWrite("gc6plus_ec_gpu_event_", 0, 1, 0);
            Platform::EscapeWrite("gc6plus_ec_gpu_event_override", 0, 1, 1);

            // simultaneous wake from GPIO
            if (simultaneous_gpio_wakeup)
            {
                if (this->simultaneous_gpio_wakeup_delay_ns <
                    m_ec_gpu_event_width_us)
                {
                    Platform::Delay(this->simultaneous_gpio_wakeup_delay_ns);
                    assertGpio(pSubdev, "misc", 0);
                    Platform::Delay(m_ec_gpu_event_width_us -
                        this->simultaneous_gpio_wakeup_delay_ns);
                    Platform::EscapeWrite("gc6plus_ec_gpu_event_", 0, 1, 1);
                }
                else
                {
                    Platform::Delay(m_ec_gpu_event_width_us);
                    Platform::EscapeWrite("gc6plus_ec_gpu_event_", 0, 1, 1);
                    Platform::Delay(this->simultaneous_gpio_wakeup_delay_ns -
                        m_ec_gpu_event_width_us);
                    assertGpio(pSubdev, "misc", 0);
                }
            }
            else
            {
                Platform::Delay(m_ec_gpu_event_width_us);
                Platform::EscapeWrite("gc6plus_ec_gpu_event_", 0, 1, 1);
            }

            // Wait for FSM to exit back to STEADY state
            statename = "STEADY";
            if (poll_for_fsm_state(statename, true))
            {
                ErrPrintf("gc6plus_maxwell: Error in polling for FSM state\n");
                m_ErrCount++;
            }

            //Cleanup functions (with priv_init)
            this->gc6plus_cleanup(pSubdev);
            
            //BSI2PMU Copy Priv Write to PMU Debug Tag Register
            Find_Register(m_regMap, reg, "LW_PGC6_SCI_SW_INTR0_STATUS_LWRR");
            if (!(field = reg->FindField("LW_PGC6_SCI_SW_INTR0_STATUS_LWRR_GPU_EVENT")))
            {
                ErrPrintf("%s: Failed to find LW_PGC6_SCI_SW_INTR0_STATUS_LWRR_GPU_EVENT field\n", __FUNCTION__);
            }
            extraPassChecks = this->getSwIntr0Status(pSubdev) &
                field->GetMask()
                && checkDebugTag(pSubdev);
            InfoPrintf("Exit/Abort extraPassChecks: %s\n",
              extraPassChecks ? "pass" : "fail");
            break;

        case ABORT :
            InfoPrintf("gc6plus_maxwell: Performing ABORT sequence test\n");
            // wait until we reach GC5 or GC6 state before asserting reset
            if (this->mode == GC5)
            {
                statename = "GC5_ENTRY_START";
            }
            else if (this->mode == GC6)
            {
                statename = "GC6_ENTRY_START1";
            }
            else
            {
                ErrPrintf("gc6plus_maxwell: Unknown mode %d in seq RESET\n",
                this->seq);
                return (1);
            }
            if (poll_for_fsm_state(statename, true))
            {
                ErrPrintf("gc6plus_maxwell: Error in polling for FSM state\n");
                m_ErrCount++;
            }

            // Now assert GPU_EVENT wake from EC
            InfoPrintf("gc6plus_maxwell: Asserting GPU_EVENT wake from EC\n");
            Platform::EscapeWrite("gc6plus_ec_gpu_event_", 0, 1, 0);
            Platform::EscapeWrite("gc6plus_ec_gpu_event_override", 0, 1, 1);
            Platform::Delay(m_ec_gpu_event_width_us);
            Platform::EscapeWrite("gc6plus_ec_gpu_event_", 0, 1, 1);

            // Wait for FSM to exit back to STEADY state
            statename = "STEADY";
            if (poll_for_fsm_state(statename, true))
            {
                ErrPrintf("gc6plus_maxwell: Error in polling for FSM state\n");
                m_ErrCount++;
            }

            //Cleanup functions (with priv_init)
            this->gc6plus_cleanup(pSubdev);
            
            //BSI2PMU Copy Priv Write to PMU Debug Tag Register
            Find_Register(m_regMap, reg, "LW_PGC6_SCI_SW_INTR0_STATUS_LWRR");
            if (!(field = reg->FindField("LW_PGC6_SCI_SW_INTR0_STATUS_LWRR_GPU_EVENT")))
            {
                ErrPrintf("%s: Failed to find LW_PGC6_SCI_SW_INTR0_STATUS_LWRR_GPU_EVENT field\n", __FUNCTION__);
            }
            extraPassChecks = this->getSwIntr0Status(pSubdev) &
                field->GetMask()
                && checkDebugTag(pSubdev);
            InfoPrintf("Exit/Abort extraPassChecks: %s\n",
                extraPassChecks ? "pass" : "fail");
            break;
        default : break;
    }

    //checking sw intr status
    this->printSwIntrXStatus(pSubdev);
    this->getState(pSubdev);
    
    //Check how long we were in gcX for
    this->getResidentTimer(pSubdev);

    //Check for:
    return ((
               check_sw_intr_status(pSubdev) //Check Pending Wakeup Interrupts and GC5/GC6 Exit/Abort flags
            && checkSteadyState(pSubdev) //we are now in steady state
            && checkTimers(pSubdev) //Check GC5/GC6 Entry, Resident, and Exit Timers
            && extraPassChecks // Different checks for Reset vs GPU_Event Exit/Abort
            ) ? 0: 1);
};

UINT32 GC6plus_Maxwell::gc6plus_gpioWakeupTest(std::string input, int index){
    unique_ptr<IRegisterClass> reg;
    UINT32 reg_addr;
    //int wakeup_timer_us = 40;
    GpuSubdevice *pSubdev = lwgpu->GetGpuSubdevice();
    int extraPassChecks = 1;
    RETURN_IF_ERR(this->gc6plus_init(pSubdev));

    // ARM wakeup timer
    this->wakeupTimerEvent(pSubdev, "disable");
    
    //print out statuses
    this->printSwIntrXStatus(pSubdev);

    //Enable Rise Wakeups for GPIOs Enable all the RISE_EN
    Find_Register(m_regMap, reg, "LW_PGC6_SCI_WAKE_RISE_EN");
    reg_addr = reg->GetAddress();
    pSubdev->RegWr32(reg_addr, 0xffffffff); 
    
    //Enable Fall Wakeups?
    //Enable Level Sensitive Wakeups?

    //Show Wake Enables
    this->printWakeEnables(pSubdev);
    
    //Enter GC5/GC6
    this->gc6plus_triggerMode(pSubdev);
    
    if(this->seq == EXIT){
        Platform::Delay(10);
    }
    
    if (poll_for_fsm_state("STEADY", false))
    {
        ErrPrintf("gc6plus_maxwell: Error in polling for FSM state\n");
        m_ErrCount++;
    }

    this->assertGpio(pSubdev, input, index);    
    Platform::Delay(10);
    this->deassertGpio(pSubdev, input, index);    
    // Delay until we are out of GCx
    if (poll_for_fsm_state("STEADY", true))
    {
        ErrPrintf("gc6plus_maxwell: Error in polling for FSM state\n");
        m_ErrCount++;
    }
    
    this->gc6plus_cleanup(pSubdev);
    
    //print out statuses
    this->printSwIntrXStatus(pSubdev);
    
    //Check how long we were in GC5 for
    this->getResidentTimer(pSubdev);

    DebugPrintf("GC6PLUS Speedy: gc6plus_gpioWakeupTest(): hpd_test is 0x%x\n",hpd_test); 
    
    //Check for:
    return ((
               check_sw_intr_status(pSubdev) //Check Pending Wakeup Interrupts and GC5/GC6 Exit/Abort flags //FIXME: How do we pass the expected wakeup?
            && checkSteadyState(pSubdev) //we are now in steady state
            && checkTimers(pSubdev) //Check GC5/GC6 Entry, Resident, and Exit Timers
            && checkDebugTag(pSubdev)
            && extraPassChecks
            ) ? 0: 1);
}

UINT32 GC6plus_Maxwell::gc6plus_swWakeupTest(){
    unique_ptr<IRegisterClass> reg;
    UINT32 reg_addr;
    UINT32 reg_data;
    //int wakeup_timer_us = 20;
    GpuSubdevice *pSubdev = lwgpu->GetGpuSubdevice();
    RETURN_IF_ERR(this->gc6plus_init(pSubdev));
    
    //checking sw intr status
    DebugPrintf("sw_intr0_status: 0x%x \n", ReadReg(pSubdev, m_regMap, "LW_PGC6_SCI_SW_INTR0_STATUS_LWRR"));

    // ARM wakeup timer
    //this->configure_wakeup_timer(pSubdev, wakeup_timer_us);
    this->wakeupTimerEvent(pSubdev, "disable");

    //Turn on SW Forced Wakeup Timer GC5 Exit
    Find_Register(m_regMap, reg, "LW_PGC6_SCI_DEBUG_SW_INTR0_FRC");
    reg_addr = reg->GetAddress();
    reg_data = ModifyFieldVal(reg.get(), 0, "LW_PGC6_SCI_DEBUG_SW_INTR0_FRC", "__WAKE_TIMER", "_ENABLE");
    pSubdev->RegWr32(reg_addr, reg_data);
    DebugPrintf("LW_PGC6_SCI_DEBUG_SW_INTR0_FRC: 0x%x \n", reg_data);
    
    Find_Register(m_regMap, reg, "LW_PGC6_SCI_DEBUG_SW_INTR_CTRL");
    reg_addr = reg->GetAddress();
    //reg_data = pSubdev->RegRd32(reg_addr);
    reg_data = ModifyFieldVal(reg.get(), 0, "LW_PGC6_SCI_DEBUG_SW_INTR_CTRL", "_FORCE_DURING", "_GC5_GC6");
    pSubdev->RegWr32(reg_addr, reg_data);
    DebugPrintf("LW_PGC6_SCI_DEBUG_SW_INTR0_FRC: 0x%x \n", reg_data);

    //Enter GC5/GC6
    this->gc6plus_triggerMode(pSubdev);

    // delay until we are out of GC5
    if (poll_for_fsm_state("STEADY", false))
    {
        ErrPrintf("gc6plus_maxwell: Error in polling for FSM state\n");
        m_ErrCount++;
    }
    if (poll_for_fsm_state("STEADY", true))
    {
        ErrPrintf("gc6plus_maxwell: Error in polling for FSM state\n");
        m_ErrCount++;
    }

    //checking sw intr status
    UINT32 sw_intr0_status = ReadReg(pSubdev, m_regMap, "LW_PGC6_SCI_SW_INTR0_STATUS_LWRR");
    DebugPrintf("sw_intr0_status: 0x%x \n", sw_intr0_status);

    //Check for:
    return ((
               check_sw_intr_status(pSubdev) //Check Pending Wakeup Interrupts and GC5/GC6 Exit/Abort flags
            && checkSteadyState(pSubdev) //we are now in steady state
            ) ? 0: 1);
}

UINT32 GC6plus_Maxwell::gc6plus_pcieWakeupTest(){
    unique_ptr<IRegisterClass> reg;
    UINT32 reg_addr;
    UINT32 reg_data;
    //int wakeup_timer_us = 100;
    GpuSubdevice *pSubdev = lwgpu->GetGpuSubdevice();

    this->getSwIntr0Status(pSubdev);
    RETURN_IF_ERR(this->gc6plus_init(pSubdev));
    //this->pcieInit(pSubdev); //Needs to be implemented

    // ARM wakeup timer
    //this->configure_wakeup_timer(pSubdev, wakeup_timer_us);
    this->wakeupTimerEvent(pSubdev, "disable");

    this->getSwIntr0Status(pSubdev);

    Find_Register(m_regMap, reg, "LW_PGC6_BSI_CTRL");
    reg_addr = reg->GetAddress();
    //Read LW_PGC6_BSI_CTRL
    reg_data = pSubdev->RegRd32(reg_addr);
    DebugPrintf("LW_PGC6_BSI_CTRL 0x%x \n", reg_data);
    //Enable CLK_REQ
    reg_data = ModifyFieldVal(reg.get(), reg_data, "LW_PGC6_BSI_CTRL", "_BSI2SCI_CLKREQ", "_ENABLE");
    //Enable RX_STAT_IDLE
    reg_data = ModifyFieldVal(reg.get(), reg_data, "LW_PGC6_BSI_CTRL", "_BSI2SCI_RXSTAT", "_ENABLE");
    pSubdev->RegWr32(reg_addr, reg_data);
    DebugPrintf("LW_PGC6_BSI_CTRL 0x%x \n", reg_data);

    //Enable bsi2sci_event Wakeup
    ReadWriteRegFieldVal(pSubdev, m_regMap, "LW_PGC6_SCI_WAKE_EN", "_BSI_EVENT", "_ENABLE");

    //Enter GC5/GC6
    this->gc6plus_triggerMode(pSubdev);
    Platform::Delay(10);

    //Trigger PCIe event (and consequently a BSI2SCI Event)
    //this->setClkReq(pSubdev, 1);//FIXME: This still needs to be implemented

    //Triggering a Read/Write to SCI should cause a wakeup
    InfoPrintf("Triggering a read to SCI\n");//FIXME: Need to trigger a read

    if (poll_for_fsm_state("STEADY", false))
    {
        ErrPrintf("gc6plus_maxwell: Error in polling for FSM state\n");
        m_ErrCount++;
    }
    if (poll_for_fsm_state("STEADY", true))
    {
        ErrPrintf("gc6plus_maxwell: Error in polling for FSM state\n");
        m_ErrCount++;
    }

    this->gc6plus_cleanup(pSubdev);

    //Show Wake Enables
    this->printWakeEnables(pSubdev);

    //checking SW intr status
    this->printSwIntrXStatus(pSubdev);
    
    this->getState(pSubdev);
    
    //Check how long we were in GC5 for
    this->getResidentTimer(pSubdev);
    
    //Check for:
    return ((
               check_sw_intr_status(pSubdev) //Check Pending Wakeup Interrupts and GC5/GC6 Exit/Abort flags
            && checkSteadyState(pSubdev) //we are now in steady state
            ) ? 0: 1);
}

UINT32 GC6plus_Maxwell::gc6plus_clampTest(){
    int wakeup_timer_us = 30;
    GpuSubdevice *pSubdev = lwgpu->GetGpuSubdevice();
    
    //Init functions
    RETURN_IF_ERR(this->gc6plus_init(pSubdev));

    //Trigger wakeup
    this->configure_wakeup_timer(pSubdev, wakeup_timer_us);
    this->wakeupTimerEvent(pSubdev, "enable");
   
    if(this->mode == GC5){
        InfoPrintf("gc6plus_gc5_clamp_escape_lo = 1\n");
        EscapeWrite("gc6plus_gc5_clamp_escape_lo", 0, 1, 1);
        Platform::Delay(1);
        InfoPrintf("gc6plus_gc5_clamp_escape_lo = 0\n");
        EscapeWrite("gc6plus_gc5_clamp_escape_lo", 0, 1, 0);
        Platform::Delay(1);
        InfoPrintf("gc6plus_gc5_clamp_escape_hi = 1\n");
        EscapeWrite("gc6plus_gc5_clamp_escape_hi", 0, 1, 1);
        Platform::Delay(1);
        InfoPrintf("gc6plus_gc5_clamp_escape_hi = 0\n");
        EscapeWrite("gc6plus_gc5_clamp_escape_hi", 0, 1, 0);
        Platform::Delay(1);
    } else {
        InfoPrintf("gc6plus_clamp_escape_lo = 1\n");
        EscapeWrite("gc6plus_clamp_escape_lo", 0, 1, 1);
        Platform::Delay(1);
        InfoPrintf("gc6plus_clamp_escape_lo = 0\n");
        EscapeWrite("gc6plus_clamp_escape_lo", 0, 1, 0);
        Platform::Delay(1);
        InfoPrintf("gc6plus_clamp_escape_hi = 1\n");
        EscapeWrite("gc6plus_clamp_escape_hi", 0, 1, 1);
        Platform::Delay(1);
        InfoPrintf("gc6plus_clamp_escape_hi = 0\n");
        EscapeWrite("gc6plus_clamp_escape_hi", 0, 1, 0);
        Platform::Delay(1);
    }        
    
    //Enter GC5/GC6
    this->gc6plus_triggerMode(pSubdev);
    Platform::Delay(10);
    
    if(this->mode == GC5){
        InfoPrintf("gc6plus_gc5_clamp_escape_lo = 1\n");
        EscapeWrite("gc6plus_gc5_clamp_escape_lo", 0, 1, 1);
        Platform::Delay(1);
        InfoPrintf("gc6plus_gc5_clamp_escape_lo = 0\n");
        EscapeWrite("gc6plus_gc5_clamp_escape_lo", 0, 1, 0);
        Platform::Delay(1);
        InfoPrintf("gc6plus_gc5_clamp_escape_hi = 1\n");
        EscapeWrite("gc6plus_gc5_clamp_escape_hi", 0, 1, 1);
        Platform::Delay(1);
        InfoPrintf("gc6plus_gc5_clamp_escape_hi = 0\n");
        EscapeWrite("gc6plus_gc5_clamp_escape_hi", 0, 1, 0);
        Platform::Delay(1);
    } else {
        InfoPrintf("gc6plus_clamp_escape_lo = 1\n");
        EscapeWrite("gc6plus_clamp_escape_lo", 0, 1, 1);
        Platform::Delay(1);
        InfoPrintf("gc6plus_clamp_escape_lo = 0\n");
        EscapeWrite("gc6plus_clamp_escape_lo", 0, 1, 0);
        Platform::Delay(1);
        InfoPrintf("gc6plus_clamp_escape_hi = 1\n");
        EscapeWrite("gc6plus_clamp_escape_hi", 0, 1, 1);
        Platform::Delay(1);
        InfoPrintf("gc6plus_clamp_escape_hi = 0\n");
        EscapeWrite("gc6plus_clamp_escape_hi", 0, 1, 0);
        Platform::Delay(1);
    
    }
    if (poll_for_fsm_state("STEADY", false))
    {
        ErrPrintf("gc6plus_maxwell: Error in polling for FSM state\n");
        m_ErrCount++;
    }
    if (poll_for_fsm_state("STEADY", true))
    {
        ErrPrintf("gc6plus_maxwell: Error in polling for FSM state\n");
        m_ErrCount++;
    }
    this->gc6plus_cleanup(pSubdev);
    
    return 0;//Use RTL Asserts to monitor this
}
//****************************************************************************************
// Reset Tests
//****************************************************************************************
UINT32 GC6plus_Maxwell::gc6plus_pexResetGC5Test(){
    //Wrapper function to call a function which will execute PEX_Reset in GC5 mode through EC_Stub
    return gc6plus_intrGpuEventTest(0);
}

UINT32 GC6plus_Maxwell::gc6plus_fundamentalResetTest(){
    string regname;
    unique_ptr<IRegisterClass> reg;
    UINT32 reg_addr;
    UINT32 reg_data;

    GpuSubdevice *pSubdev = lwgpu->GetGpuSubdevice();

    //Init functions
    RETURN_IF_ERR(this->gc6plus_init(pSubdev));
    //Write a known value to SCI's Volatile scratch register to see if fundamental reset happens.
    Find_Register(m_regMap, reg,"LW_PGC6_SCI_SCRATCH");
    reg_addr = reg->GetAddress();
    pSubdev->RegWr32(reg_addr, 0x00ff1234);
    DebugPrintf("gc6plus_maxwell: Writing 0x00ff1234 to LW_PGC6_SCI_SCRATCH \n");

    Find_Register(m_regMap, reg,"LW_PGC6_BSI_SCRATCH(0)");
    reg_addr = reg->GetAddress();
    pSubdev->RegWr32(reg_addr, 0x00ff1234);
    DebugPrintf("gc6plus_maxwell: Writing 0x00ff1234 to LW_PGC6_BSI_SCRATCH(0) \n");

    Platform::Delay(2);

    Find_Register(m_regMap, reg, "LW_XVE_RESET_CTRL");
    reg_addr = reg->GetAddress();
    reg_addr = 0x88000 + reg_addr;//TODO: Fix hard coded 0x88000 with LW_PCFG_0 (or LW_PCFG__BASE)

    reg_data = 0x0002007e;//pSubdev->RegRd32(reg_addr);
    DebugPrintf("gc6plus_maxwell: gc6plus_fundamentalResetTest: Value of %s 0x%08x\n", regname.c_str(), reg_data);
    pSubdev->RegWr32(reg_addr, reg_data | 0x20000);
    Platform::Delay(30);

    InfoPrintf("hot_reset : asserting HotReset\n");

    // Send in Hot reset
    Platform::EscapeWrite("doHotReset",0,1,1);
    Platform::Delay(160);

    // Exit out of Reset
    // XXX - May not be necessary
    Platform::EscapeWrite("exitHotReset",0,1,1);
    InfoPrintf("hot_reset : Exiting hot reset ...\n");
    Platform::Delay(30);
    //Restore PCIE Config Space
    this->pcieRestore();

    //Re-initialize PRIV
    this->priv_init(pSubdev);


    Find_Register(m_regMap, reg, "LW_PGC6_SCI_SCRATCH");
    reg_addr = reg->GetAddress();
    reg_data = pSubdev->RegRd32(reg_addr);
    DebugPrintf("gc6plus_maxwell: gc6plus_fundamentalResetTest: Reading LW_PGC6_SCI_SCRATCH. \n");
    if( reg_data != 0x0){
        ErrPrintf("gc6plus_maxwell: gc6plus_fundamentalResetTest: LW_PGC6_SCI_SCRATCH did not get reset, it reads 0x%08x",reg_data);
        return 1;
    } else {
        DebugPrintf("gc6plus_maxwell: gc6plus_fundamentalResetTest: LW_PGC6_SCI_SCRATCH got reset correctly by fundamental reset. \n");
    }
    Find_Register(m_regMap, reg,"LW_PGC6_BSI_SCRATCH(0)");
    reg_addr = reg->GetAddress();
    reg_data = ReadReg(pSubdev, m_regMap,"LW_PGC6_BSI_SCRATCH(0)");
    DebugPrintf("Reading LW_PGC6_BSI_SCRATCH(0). \n");
    if( reg_data != 0x0){
        ErrPrintf("gc6plus_maxwell: LW_PGC6_BSI_SCRATCH(0) did not get reset, it reads 0x%08x", reg_data);
        return 1;
    } else {
        DebugPrintf("gc6plus_maxwell: LW_PGC6_BSI_SCRATCH(0) got reset correctly by fundamental reset. \n");
    }

    Platform::Delay(300);

    return 0;//Check using a testpoint TESTPOINT_CHECK_GC6__FUNDAMENTAL_RESET
}

UINT32 GC6plus_Maxwell::gc6plus_softwareResetTest(){
    string regname;
    unique_ptr<IRegisterClass> reg;
    UINT32 reg_addr;
    UINT32 reg_data;

    GpuSubdevice *pSubdev = lwgpu->GetGpuSubdevice();
    
    //Init functions
    RETURN_IF_ERR(this->gc6plus_init(pSubdev));
    
   //Write a known value to SCI's Volatile scratch register to see if fundamental reset happens.
    Find_Register(m_regMap, reg,"LW_PGC6_SCI_SCRATCH");
    reg_addr = reg->GetAddress();
    pSubdev->RegWr32(reg_addr, 0x00ff1234);
    DebugPrintf("gc6plus_maxwell: %s Writing 0x00ff1234 to LW_PGC6_SCI_SCRATCH \n", __FUNCTION__);

    Find_Register(m_regMap, reg,"LW_PGC6_BSI_SCRATCH(0)");
    reg_addr = reg->GetAddress();
    pSubdev->RegWr32(reg_addr, 0x00ff1234);
    DebugPrintf("gc6plus_maxwell: %s Writing 0x00ff1234 to LW_PGC6_BSI_SCRATCH(0) \n", __FUNCTION__);

    Platform::Delay(2); 
    
    regname = "LW_XVE_SW_RESET";
    Find_Register(m_regMap, reg, "LW_XVE_SW_RESET");
    reg_addr = reg->GetAddress();
    reg_addr = 0x88000 + reg_addr;       //TODO: Fix hard coded 0x88000 with LW_PCFG_0 (or LW_PCFG__BASE)
    reg_data = pSubdev->RegRd32(reg_addr);
    //Asserting Reset
    DebugPrintf("gc6plus_maxwell: gc6plus_softwareResetTest: Value of %s 0x%08x\n",regname.c_str(), reg_data);
    pSubdev->RegWr32(reg_addr, (reg_data | 0x1));     // setting LW_XVE_SW_RESET_RESET (bit 0) to 1
    //reg_data = pSubdev->RegRd32(reg_addr);
    //DebugPrintf("gc6plus_maxwell: gc6plus_softwareResetTest: Value of %s 0x%08x\n",regname.c_str(), reg_data);
    reg_addr = reg->GetAddress();
    //reg_addr=((((reg_addr)&(0xf00))<<16) | ((reg_addr)& 0xfffff0ff));
    //lwgpu->CfgWr32(0x81000718, reg_data & 0xfffffffe);  // setting LW_XVE_SW_RESET_RESET (bit 0) to 0
    Platform::Delay(5); 
    RC rc;
    pSubdev = lwgpu->GetGpuSubdevice();
    auto pGpuPcie = pSubdev->GetInterface<Pcie>(); 
    
    m_pPciCfg = pSubdev->AllocPciCfgSpace();
    rc = m_pPciCfg->Initialize(pGpuPcie->DomainNumber(),
                               pGpuPcie->BusNumber(),
                               pGpuPcie->DeviceNumber(),
                               pGpuPcie->FunctionNumber());

    UINT32 pci_domain_num = pGpuPcie->DomainNumber();
    UINT32 pci_bus_num = pGpuPcie->BusNumber();
    UINT32 pci_dev_num = pGpuPcie->DeviceNumber();
    UINT32 pci_func_num = pGpuPcie->FunctionNumber();
    DebugPrintf("gc6plus_maxwell: pcie_write 0x%08x \n",(reg_data & 0xfffffffe));
    
    CHECK_RC(Platform::PciWrite32(pci_domain_num,
                pci_bus_num,
                pci_dev_num,
                pci_func_num,
                reg_addr = reg->GetAddress(), (reg_data & 0xfffffffe))
            );
    
    //Wait for 10us for the reset to take effect
    Platform::Delay(10); 

   
    //Re-initialize PRIV
    this->priv_init(pSubdev);

    Find_Register(m_regMap, reg, "LW_PGC6_SCI_SCRATCH");
    reg_addr = reg->GetAddress();
    reg_data = pSubdev->RegRd32(reg_addr);
    DebugPrintf("gc6plus_maxwell: %s : Reading LW_PGC6_SCI_SCRATCH. \n");
    if( reg_data != 0x0){
        ErrPrintf("gc6plus_maxwell: %s : LW_PGC6_SCI_SCRATCH did not get reset, it reads 0x%08x", __FUNCTION__,reg_data);
        return 1;
    } else {
        DebugPrintf("gc6plus_maxwell: %s : LW_PGC6_SCI_SCRATCH got reset correctly by software reset. \n", __FUNCTION__);
    }
    Find_Register(m_regMap, reg,"LW_PGC6_BSI_SCRATCH(0)");
    reg_addr = reg->GetAddress();
    reg_data = ReadReg(pSubdev, m_regMap,"LW_PGC6_BSI_SCRATCH(0)");
    DebugPrintf("Reading LW_PGC6_BSI_SCRATCH(0). \n");
    if( reg_data != 0x0){
        ErrPrintf("gc6plus_maxwell: %s LW_PGC6_BSI_SCRATCH(0) did not get reset, it reads 0x%08x", __FUNCTION__, reg_data);
        return 1;
    } else {
        DebugPrintf("gc6plus_maxwell: %s LW_PGC6_BSI_SCRATCH(0) got reset correctly by softwre reset. \n", __FUNCTION__);
    }

    this->gc6plus_cleanup(pSubdev); 
    
    return 0;//Check using a testpoint for software reset
}

//Toggle system reset. No cleanup checks are done here. These should be done in the test itself. 
//Uses escape writes in order to toggle the EC's version of pex_reset
UINT32 GC6plus_Maxwell::toggleSystemReset(){
    const char * statename;

    // Now assert reset from EC
    InfoPrintf("gc6plus_maxwell: Asserting reset from EC\n");
    Platform::EscapeWrite("gc6plus_ec_pex_reset_", 0, 1, 0);
    Platform::EscapeWrite("gc6plus_ec_pex_reset_override", 0, 1, 1);
    Platform::EscapeWrite("gc6plus_ec_gpu_event_", 0, 1, 0);
    Platform::EscapeWrite("gc6plus_ec_gpu_event_override", 0, 1, 1);
    Platform::Delay(m_ec_reset_width_us);
    Platform::EscapeWrite("gc6plus_ec_pex_reset_", 0, 1, 1);
    Platform::EscapeWrite("gc6plus_ec_gpu_event_", 0, 1, 1);
    InfoPrintf("gc6plus_maxwell: Deasserting reset from EC\n");
   // Wait for FSM to exit back to STEADY state
    statename = "STEADY";
    if (poll_for_fsm_state(statename, true))
    {
        ErrPrintf("gc6plus_maxwell: Error in polling for FSM state\n");
        return 1;
    }

    return 0;
}

//****************************************************************************************
// Miscellaneous functions used in the tests
//****************************************************************************************
UINT32 GC6plus_Maxwell::ModifyFieldVal(IRegisterClass* reg, UINT32 reg_data, string regname, string fieldname,  string valuename)//ModifyFieldVal
{
    string tempname;
    unique_ptr<IRegisterField> field;
    unique_ptr<IRegisterValue> value;

    tempname = regname;
    tempname += fieldname;
    Find_Field(reg, field, tempname.c_str());
    tempname += valuename;
    InfoPrintf("%s: Starting to find value.\n", __FUNCTION__);
    if (!(value = field->FindValue(tempname.c_str())))
    {
        ErrPrintf("%s: Failed to find %s value\n", __FUNCTION__, tempname.c_str());
    }
    InfoPrintf("%s: Finished to find value %s \n", __FUNCTION__, tempname.c_str());
    UINT32 data_new = (reg_data & ~field->GetMask()) |
        (value->GetValue() << field->GetStartBit());
    InfoPrintf("%s: Return data = 0x%08x\n", __FUNCTION__, data_new);
    return(data_new);
}

int GC6plus_Maxwell::ReadWriteRegFieldVal(GpuSubdevice* pSubdev, IRegisterMap* regmap, string regname, string fieldname, string valuename)//ReadWriteRegFieldVal
{
    unique_ptr<IRegisterClass> reg;
    Find_Register(regmap, reg, regname.c_str());
    UINT32 reg_addr = reg->GetAddress();
    UINT32 reg_data = pSubdev->RegRd32(reg_addr);
    reg_data = ModifyFieldVal(reg.get(), reg_data, regname, fieldname, valuename);
    pSubdev->RegWr32(reg_addr, reg_data);
    InfoPrintf("%s: Finished writing address 0x%08x with 0x%08x.\n", __FUNCTION__, reg_addr, reg_data);

    return (0);
}

int GC6plus_Maxwell::WriteRegFieldVal(GpuSubdevice* pSubdev, IRegisterMap* regmap, string regname, string fieldname, string valuename)//ReadWriteRegFieldVal
{
    unique_ptr<IRegisterClass> reg;
    Find_Register(regmap, reg, regname.c_str());
    UINT32 reg_addr = reg->GetAddress();
    UINT32 reg_data = ModifyFieldVal(reg.get(), 0, regname, fieldname, valuename);
    pSubdev->RegWr32(reg_addr, reg_data);
    InfoPrintf("%s: Finished writing address 0x%08x with 0x%08x.\n", __FUNCTION__, reg_addr, reg_data);
    return (0);
}

UINT32 GC6plus_Maxwell::ModifyFieldValWithData(IRegisterClass* reg, UINT32 reg_data, string regname, string fieldname, UINT32 value)//ModifyFieldValWithData
{
    string tempname;
    unique_ptr<IRegisterField> field;

    tempname = regname;
    tempname += fieldname;
    Find_Field(reg, field, tempname.c_str());
    UINT32 data_new = (reg_data & ~field->GetMask()) |
        (value << field->GetStartBit());
    InfoPrintf("%s: Finished to modify value returning 0x%08x \n", __FUNCTION__, data_new);
    return(data_new);
}

UINT32 GC6plus_Maxwell::ReadReg(GpuSubdevice* pSubdev, IRegisterMap* regmap, string regname)
{
    unique_ptr<IRegisterClass> reg;
    Find_Register(regmap, reg, regname.c_str());
    UINT32 reg_addr = reg->GetAddress();
    UINT32 reg_data = pSubdev->RegRd32(reg_addr);
    return(reg_data);
}

int GC6plus_Maxwell::ReadWriteVal(GpuSubdevice *pSubdev, UINT32 reg, UINT32 dataMsk, UINT32 data) {
    UINT32 retData;
    
    retData =  pSubdev->RegRd32(reg);
    pSubdev->RegWr32(reg, (retData & ~dataMsk) | data);

    InfoPrintf("%s: Writing Register 0x%08x with Mask = 0x%08x and data = 0x%08x \n", __FUNCTION__, reg, dataMsk, data );
    return (0);
}

int GC6plus_Maxwell::WriteReadCheckVal(UINT32 reg, UINT32 regMsk, const char *regName, UINT32 data) {
    UINT32 retData;
    
    lwgpu->GetGpuSubdevice()->RegWr32(reg, data);
    retData =  lwgpu->GetGpuSubdevice()->RegRd32(reg);
    if ((retData & regMsk) == (data & regMsk)) {
        DebugPrintf("%s read correct: Expected 0x%08x(0x%08x with mask), Got 0x%08x(0x%08x with mask)\n",
                 regName, data, (data & regMsk), retData, (retData & regMsk));
        return 0;
    } else {
        ErrPrintf("%s read ERROR: Expected 0x%08x(0x%08x with mask), Got 0x%08x(0x%08x with mask)\n",
                regName, data, (data & regMsk), retData, (retData & regMsk));
        return 1;
    }
}

int GC6plus_Maxwell::ReadCheckVal(UINT32 reg, UINT32 regMsk, const char *regName, UINT32 data) {
    UINT32 retData;
    retData =  lwgpu->GetGpuSubdevice()->RegRd32(reg);
    if ((retData & regMsk) == (data & regMsk)) {
        DebugPrintf("%s read correct: Expected 0x%08x(0x%08x with mask), Got 0x%08x(0x%08x with mask)\n",
                 regName, data, (data & regMsk), retData, (retData & regMsk));
        return 0;
    } else {
        ErrPrintf("%s read ERROR: Expected 0x%08x(0x%08x with mask), Got 0x%08x(0x%08x with mask)\n",
                regName, data, (data & regMsk), retData, (retData & regMsk));
        return 1;
    }
}

int
GC6plus_Maxwell::ChkRegVal2(UINT32 reg, UINT32 regMsk, const char *regName, UINT32 data) {
    UINT32 retData;
    
    retData =  lwgpu->GetGpuSubdevice()->RegRd32(reg);
    if ((retData & regMsk) == (data & regMsk)) {
        DebugPrintf("%s read correct: Expected 0x%08x(0x%08x with mask), Got 0x%08x(0x%08x with mask)\n",
                 regName, data, (data & regMsk), retData, (retData & regMsk));
        return 0;
    } else {
        ErrPrintf("%s read error: Expected 0x%08x(0x%08x with mask), Got 0x%08x(0x%08x with mask)\n",
                regName, data, (data & regMsk), retData, (retData & regMsk));
        return 1;
    }
}

int
GC6plus_Maxwell::ChkRegVal3(UINT32 reg, UINT32 regMsk, const char *regName, UINT32 data) {
    UINT32 retData;
    
    retData =  lwgpu->GetGpuSubdevice()->RegRd32(reg);
    if ((retData & regMsk) == (data & regMsk)) {
        DebugPrintf("%s read correct: Expected 0x%08x(0x%08x with mask), Got 0x%08x(0x%08x with mask)\n",
                 regName, data, (data & regMsk), retData, (retData & regMsk));
        return 0;
    } else {
        InfoPrintf("%s Expected 0x%08x(0x%08x with mask), Got 0x%08x(0x%08x with mask)\n",
                regName, data, (data & regMsk), retData, (retData & regMsk));
        return 1;
    }
}

int 
GC6plus_Maxwell::WrRegArray(int index, const char *regName, UINT32 data)
{
    UINT32 Value;
    unique_ptr<IRegisterClass> pReg = lwgpu->GetRegister(regName);
    if (!pReg)
    {
        ErrPrintf("Could not GetRegister(%s)!\n", regName);
        return (1);
    }

    lwgpu->RegWr32(pReg->GetAddress(index), data);

    Value = lwgpu->RegRd32(pReg->GetAddress(index));
    DebugPrintf("%s%d value is 0x%8x. \n", regName, index, Value);

    return 0;
}

UINT32
GC6plus_Maxwell::RdRegArray(int index, const char *regName)
{
    UINT32 Value;
    unique_ptr<IRegisterClass> pReg = lwgpu->GetRegister(regName);
    if (!pReg)
    {
        ErrPrintf("Could not GetRegister(%s)!\n", regName);
        return (0);
    }

    Value = lwgpu->RegRd32(pReg->GetAddress(index));

    return Value;
}

int GC6plus_Maxwell::senseFuse(void)
{
/*This fuction program one fuse with the address addr and value data
*/
    unique_ptr<IRegisterClass> reg;
    unique_ptr<IRegisterField> field;
    unique_ptr<IRegisterValue> value;
    UINT32 reg_addr;
    UINT32 reg_data;
    UINT32 LW_FUSE_FUSECTRL_STATE_IDLE_value;

    UINT32 i;
    UINT32 fuse_state;
    int errCnt = 0;
    GpuSubdevice *pSubdev = lwgpu->GetGpuSubdevice();
    DebugPrintf("FuseTest:  senseFuse begins\n");
    lwgpu->RegWr32(LW_FUSE_FUSEADDR,1);
    Find_Register(m_regMap, reg, "LW_FUSE_FUSECTRL");
    Find_Field(reg, field, "LW_FUSE_FUSECTRL_STATE");
    Find_Value(field, value, "LW_FUSE_FUSECTRL_STATE_IDLE");
    LW_FUSE_FUSECTRL_STATE_IDLE_value = value->GetValue();
     
    pSubdev->Regs().Write32(MODS_FUSE_FUSECTRL_CMD_SENSE_CTRL);
    i=0;
    do {
        i++;
        Find_Register(m_regMap, reg, "LW_FUSE_FUSECTRL");
        reg_addr = reg->GetAddress();
        Find_Field(reg, field, "LW_FUSE_FUSECTRL_STATE");
        reg_data = pSubdev->RegRd32(reg_addr);
        fuse_state = reg_data & ~field->GetMask();
    } while ((fuse_state != LW_FUSE_FUSECTRL_STATE_IDLE_value) && (i<8000));

    if (fuse_state != LW_FUSE_FUSECTRL_STATE_IDLE_value) {
        ErrPrintf("FuseTest: Error in senseFuse. Timed out waiting for fuse_state to return to IDLE . Last state = %d Iteration = %d\n", fuse_state, i);
        errCnt++;
    } 
    DebugPrintf("FuseTest:  senseFuse ends, Iterations = %d\n",i);
    return errCnt;
}

int GC6plus_Maxwell::programFuse(UINT32 addr, UINT32 data, UINT32 mask)
{
/*This fuction program one fuse with the address addr and value data
*/
    unique_ptr<IRegisterClass> reg;
    unique_ptr<IRegisterField> field;
    unique_ptr<IRegisterValue> value;
    UINT32 reg_addr;
    UINT32 reg_data;
    UINT32 LW_FUSE_FUSECTRL_STATE_IDLE_value;

    UINT32 i;
    UINT32 fuse_state;
    UINT32 orig_data;
    int errCnt = 0;
    GpuSubdevice *pSubdev = lwgpu->GetGpuSubdevice();
    DebugPrintf("FuseTest:  programFuse begins\n");

    Find_Register(m_regMap, reg, "LW_FUSE_FUSECTRL");
    Find_Field(reg, field, "LW_FUSE_FUSECTRL_STATE");
    Find_Value(field, value, "LW_FUSE_FUSECTRL_STATE_IDLE");
    LW_FUSE_FUSECTRL_STATE_IDLE_value = value->GetValue();
    
    //Read orignal value
    lwgpu->RegWr32(LW_FUSE_FUSEADDR,addr);
    DebugPrintf("FuseTest:  Write FUSECTRL_CMD to READ\n");
    pSubdev->Regs().Write32(MODS_FUSE_FUSECTRL_CMD_READ);
    i=0;
    do {
        i++;
        Find_Register(m_regMap, reg, "LW_FUSE_FUSECTRL");
        reg_addr = reg->GetAddress();
        Find_Field(reg, field, "LW_FUSE_FUSECTRL_STATE");
        reg_data = pSubdev->RegRd32(reg_addr);
        fuse_state = reg_data & ~field->GetMask();
    } while ((fuse_state != LW_FUSE_FUSECTRL_STATE_IDLE_value) && (i<8000));

    if (fuse_state != LW_FUSE_FUSECTRL_STATE_IDLE_value) {
        ErrPrintf("FuseTest: Error in programFuse. Timed out waiting for fuse_state to return to IDLE . Last state = %d Iteration = %d\n", fuse_state, i);
        errCnt++;
    } 
    lwgpu->GetRegFldNum("LW_FUSE_FUSERDATA", "_DATA", &orig_data);

    //write new value to fuse
    DebugPrintf("data = %x", data);
    data = mask & data;
    DebugPrintf("data = %x , orig_data = %x, mask = %x\n", data, orig_data, mask);
    Find_Register(m_regMap, reg, "LW_FUSE_FUSEWDATA");
    reg_addr = reg->GetAddress();
    lwgpu->RegWr32(reg_addr,data);
    EscapeWrite("fuse_src_core_r",0,1,1);
    DebugPrintf("FuseTest:  Write FUSECTRL_CMD to WRTIE\n");
    pSubdev->Regs().Write32(MODS_FUSE_FUSECTRL_CMD_WRITE);
    i=0;
    do {
        i++;
        Find_Register(m_regMap, reg, "LW_FUSE_FUSECTRL");
        reg_addr = reg->GetAddress();
        Find_Field(reg, field, "LW_FUSE_FUSECTRL_STATE");
        reg_data = pSubdev->RegRd32(reg_addr);
        fuse_state = reg_data & ~field->GetMask();
    } while ((fuse_state != LW_FUSE_FUSECTRL_STATE_IDLE_value) && (i<8000));

    if (fuse_state != LW_FUSE_FUSECTRL_STATE_IDLE_value) {
        ErrPrintf("FuseTest: Error in programFuse. Timed out waiting for fuse_state to return to IDLE . Last state = %d Iteration = %d\n", fuse_state, i);
        errCnt++;
    } 
    EscapeWrite("fuse_src_core_r",0,1,0);

    //read again to make that new value can be seen from fuse_dataout 
    DebugPrintf("FuseTest:  Write FUSECTRL_CMD to READ\n");
    pSubdev->Regs().Write32(MODS_FUSE_FUSECTRL_CMD_READ);
    i=0;
    do {
        i++;
        Find_Register(m_regMap, reg, "LW_FUSE_FUSECTRL");
        reg_addr = reg->GetAddress();
        Find_Field(reg, field, "LW_FUSE_FUSECTRL_STATE");
        reg_data = pSubdev->RegRd32(reg_addr);
        fuse_state = reg_data & ~field->GetMask();
    } while ((fuse_state != LW_FUSE_FUSECTRL_STATE_IDLE_value) && (i<8000));

    if (fuse_state != LW_FUSE_FUSECTRL_STATE_IDLE_value) {
        ErrPrintf("FuseTest: Error in programFuse. Timed out waiting for fuse_state to return to IDLE . Last state = %d Iteration = %d\n", fuse_state, i);
        errCnt++;
    }

    DebugPrintf("FuseTest:  programFuse ends, Iterations = %d, address is %8x, data is %8x\n",i, addr, data);
    return errCnt;

}

int GC6plus_Maxwell::updateFuseopt(void)
{
/*This fuction program one fuse with the address addr and value data
*/
    DebugPrintf("FuseTest: updateFuseopt begins \n");
    lwgpu->RegWr32(LW_FUSE_PRIV2INTFC_START,1);
     
//    lwgpu->RegWr32(LW_FUSE_FUSERDATA,data);
//make sure the state machine is in idle state
    //DelayNs(40);        //clear start to avoid reread fuse data
    Platform::Delay(1);
    lwgpu->RegWr32(LW_FUSE_PRIV2INTFC_START,0);
    //DelayNs(15000);     //because fuse_state cannot indicate all of fuse rows have been read out and updated chip options regiser, we should wait
    Platform::Delay(15);
    
    DebugPrintf("FuseTest: updateFuseopt ends.\n");

    return (0);
}

int GC6plus_Maxwell::Enable_fuse_program(void)
{
    unique_ptr<IRegisterClass> reg;
    unique_ptr<IRegisterField> field;
    unique_ptr<IRegisterValue> value;
    UINT32 reg_addr;
    UINT32 reg_data;
    UINT32 LW_FUSE_FUSECTRL_STATE_IDLE_value;

    GpuSubdevice *pSubdev = lwgpu->GetGpuSubdevice();
    UINT32 fuse_state;

    Find_Register(m_regMap, reg, "LW_FUSE_FUSECTRL");
    Find_Field(reg, field, "LW_FUSE_FUSECTRL_STATE");
    Find_Value(field, value, "LW_FUSE_FUSECTRL_STATE_IDLE");
    LW_FUSE_FUSECTRL_STATE_IDLE_value = value->GetValue();
    
    DebugPrintf("FuseTest: About to run diag Fuse.\n");
    if (lwgpu->SetRegFldDef("LW_PBUS_DEBUG_1","_FUSES_VISIBLE","_ENABLED"))
    { ErrPrintf("Couldn't set LW_PBUS_DEBUG_1_FUSES_VISIBLE to _ENABLED\n");}
    EscapeWrite("fuse_src_core_r",0,1,0);
    EscapeWrite("bond_ignfuse_r",0,1,0);
    // Wait for idle
    DebugPrintf("FuseTest: Waiting for CMD_IDLE.\n");
    do {
        Find_Register(m_regMap, reg, "LW_FUSE_FUSECTRL");
        reg_addr = reg->GetAddress();
        Find_Field(reg, field, "LW_FUSE_FUSECTRL_STATE");
        reg_data = pSubdev->RegRd32(reg_addr);
        fuse_state = reg_data & ~field->GetMask();
    } while (fuse_state != LW_FUSE_FUSECTRL_STATE_IDLE_value);

    Platform::Delay(1);  
    DebugPrintf("FuseTest: Waiting for CMD_IDLE.\n");
    pSubdev->Regs().Write32(MODS_FUSE_FUSECTRL_CMD_READ);
    // Wait for idle
    DebugPrintf("FuseTest: READ DEFAULT FUSE VALE,Waiting for CMD_IDLE...\n");
    do {
        Find_Register(m_regMap, reg, "LW_FUSE_FUSECTRL");
        reg_addr = reg->GetAddress();
        Find_Field(reg, field, "LW_FUSE_FUSECTRL_STATE");
        reg_data = pSubdev->RegRd32(reg_addr);
        fuse_state = reg_data & ~field->GetMask();
    } while (fuse_state != LW_FUSE_FUSECTRL_STATE_IDLE_value);  

    Platform::Delay(15);    //make sure fuse_sense_done is set
    
    programFuse(0, 0x1, 0x1);
    
    //after write the first 2 rows, sense fuses to give the global configurations
    senseFuse();
    return (0);
}

int GC6plus_Maxwell::PrivInit(void) {
    DebugPrintf("PrivInit(): starting\n");
    int err = 0;
    
    unique_ptr<IRegisterClass> reg;
    unique_ptr<IRegisterField> field;
    unique_ptr<IRegisterValue> value;
    UINT32 addr;
    UINT32 mask;
    UINT32 start_bit;
    UINT32 reg_rd_val,reg_wr_val;
    //UINT32 reg_rd_val_masked,reg_wr_val_masked;
    
    UINT32 arch = lwgpu->GetArchitecture();
    if(arch < 0x400) {
        return 1;
    }
      
    //reset priv ring master removed in TU10x. TODO remove code if works
    if (!(reg = m_regMap->FindRegister("LW_PMC_ENABLE"))) {
        ErrPrintf("PrivReInit::CheckRegister: register LW_PMC_ENABLE is not defined!\n");
        err++;
    }
/*    if (!(field = reg->FindField("LW_PMC_ENABLE_PRIV_RING"))) {
        ErrPrintf("PrivReInit::CheckRegister: field LW_PMC_ENABLE_PRIV_RING is not defined!\n");
        err++;
    }
    if (!(value = field->FindValue("LW_PMC_ENABLE_PRIV_RING_DISABLED"))) {
        ErrPrintf("PrivReInit::CheckRegister: value LW_PMC_ENABLE_PRIV_RING_DISABLED is not defined!\n");
        err++;
    }
    addr = reg->GetAddress();
    reg_rd_val = lwgpu->RegRd32(addr);
    DebugPrintf("PrivInit:: read LW_PMC_ENABLE,reg_rd_val=0x%x\n",reg_rd_val);
    mask = field->GetMask();
    start_bit = field->GetStartBit();
    reg_wr_val = (0x0 | mask) & (~mask | (value->GetValue() << start_bit));
    lwgpu->RegWr32(addr,reg_wr_val);
    DebugPrintf("PrivInit:: write LW_PMC_ENABLE,reg_wr_val=0x%x\n",reg_wr_val);
    
    if (!(value = field->FindValue("LW_PMC_ENABLE_PRIV_RING_ENABLED"))) {
        ErrPrintf("PrivInit:: value LW_PMC_ENABLE_PRIV_RING_ENABLED is not defined!\n");
        err++;
    }
    reg_wr_val = (reg_rd_val | mask) & (~mask | (value->GetValue() << start_bit));
    lwgpu->RegWr32(addr,reg_wr_val);
    DebugPrintf("PrivInit::PrivReInitTest: write LW_PMC_ENABLE,reg_wr_val=0x%x\n",reg_wr_val);
  */  
    //new priv ring sequence
    //start priv ring
    DebugPrintf("PrivInit: start PRIV ring\n");
    if (!(reg = m_regMap->FindRegister("LW_PPRIV_MASTER_RING_COMMAND"))) {
        ErrPrintf("PrivInit:: register LW_PPRIV_MASTER_RING_COMMAND is not defined!\n");
        err++;
    }
    if (!(field = reg->FindField("LW_PPRIV_MASTER_RING_COMMAND_CMD"))) {
        ErrPrintf("PrivInit:: field LW_PPRIV_MASTER_RING_COMMAND_CMD is not defined!\n");
        err++;
    }
    if (!(value = field->FindValue("LW_PPRIV_MASTER_RING_COMMAND_CMD_ENUMERATE_AND_START_RING"))) {
        ErrPrintf("PrivInit:: value LW_PPRIV_MASTER_RING_COMMAND_CMD_ENUMERATE_AND_START_RING is not defined!\n");
        err++;
    }
    addr = reg->GetAddress();
   
    //Read RING_COMMAND 20 times to allow reset packet to propogate down the pri ring. 
    for (int i = 0; i < 20; i++){
        lwgpu->RegRd32(addr);  
    }
    
    mask = field->GetMask();
    start_bit = field->GetStartBit();
    reg_rd_val = lwgpu->RegRd32(addr); //FIXME Add a dummy read.  Somehow the write got dropped which may because ring de-reset not fully finished yet. 
    DebugPrintf("PrivInit: read LW_PPRIV_MASTER_RING_COMMAND,got reg_rd_val=0x%x\n",reg_rd_val); 
    reg_rd_val = 0; //FIXME cleaning all the bits to zero and only writting the bits of interest
    reg_wr_val = (reg_rd_val | mask) & (~mask | (value->GetValue() << start_bit));
    lwgpu->RegWr32(addr,reg_wr_val);
    DebugPrintf("PrivInit: write LW_PPRIV_MASTER_RING_COMMAND,reg_wr_val=0x%x\n",reg_wr_val); 

    //Configure the ring that it will queue up the priv transactions that comes before the ring is initialized
    if (!(reg = m_regMap->FindRegister("LW_PPRIV_SYS_PRIV_DECODE_CONFIG"))) {
        ErrPrintf("PrivInit:: register LW_PPRIV_SYS_PRIV_DECODE_CONFIG is not defined!\n");
        err++;
    }
    if (!(field = reg->FindField("LW_PPRIV_SYS_PRIV_DECODE_CONFIG_RING"))) {
        ErrPrintf("PrivInit:: field LW_PPRIV_SYS_PRIV_DECODE_CONFIG_RING is not defined!\n");
        err++;
    }

    if (!(value = field->FindValue("LW_PPRIV_SYS_PRIV_DECODE_CONFIG_RING_WAIT_FOR_RING_START_COMPLETE"))) {
        ErrPrintf("PrivInit:: value LW_PPRIV_SYS_PRIV_DECODE_CONFIG_RING_WAIT_FOR_RING_START_COMPLETE is not defined!\n");
        err++;
    }
    addr = reg->GetAddress();
    reg_rd_val = lwgpu->RegRd32(addr);
    DebugPrintf("PrivInit:: read LW_PPRIV_SYS_PRIV_DECODE_CONFIG,reg_rd_val=0x%x\n",reg_rd_val);
    mask = field->GetMask();
    start_bit = field->GetStartBit();
    reg_wr_val = (reg_rd_val | mask) & (~mask | (value->GetValue() << start_bit));
    lwgpu->RegWr32(addr,reg_wr_val);
    DebugPrintf("PrivInit:: write LW_PPRIV_SYS_PRIV_DECODE_CONFIG,reg_wr_val=0x%x\n",reg_wr_val);
    
    //deassert the priv ring reset
    if (!(reg = m_regMap->FindRegister("LW_PPRIV_MASTER_RING_GLOBAL_CTL"))) {
        ErrPrintf("PrivInit:: register LW_PPRIV_MASTER_RING_GLOBAL_CTL is not defined!\n");
        err++;
    }
    if (!(field = reg->FindField("LW_PPRIV_MASTER_RING_GLOBAL_CTL_RING_RESET"))) {
        ErrPrintf("PrivInit:: field LW_PPRIV_MASTER_RING_GLOBAL_CTL_RING_RESET is not defined!\n");
        err++;
    }
    if (!(value = field->FindValue("LW_PPRIV_MASTER_RING_GLOBAL_CTL_RING_RESET_DEASSERTED"))) {
        ErrPrintf("PrivInit:: value LW_PPRIV_MASTER_RING_GLOBAL_CTL_RING_RESET_DEASSERTED is not defined!\n");
        err++;
    }
    addr = reg->GetAddress();
    reg_rd_val = lwgpu->RegRd32(addr);
    DebugPrintf("PrivInit: read LW_PPRIV_MASTER_RING_GLOBAL_CTL,reg_rd_val=0x%x\n",reg_rd_val);
    mask = field->GetMask();
    start_bit = field->GetStartBit();
    reg_wr_val = (reg_rd_val | mask) & (~mask | (value->GetValue() << start_bit));
    lwgpu->RegWr32(addr,reg_wr_val);
    DebugPrintf("PrivInit: write LW_PPRIV_MASTER_RING_GLOBAL_CTL,reg_wr_val=0x%x\n",reg_wr_val);

    //poll the ring status
    if (!(reg = m_regMap->FindRegister("LW_PPRIV_MASTER_RING_COMMAND"))) {
        ErrPrintf("PrivInit:: register LW_PPRIV_MASTER_RING_COMMAND is not defined!\n");
        err++;
    }
    if (!(field = reg->FindField("LW_PPRIV_MASTER_RING_COMMAND_CMD"))) {
        ErrPrintf("PrivInit:: field LW_PPRIV_MASTER_RING_COMMAND_CMD is not defined!\n");
        err++;
    }
    addr = reg->GetAddress();
    mask = field->GetMask();
    do {
        reg_rd_val = lwgpu->RegRd32(addr);
        DebugPrintf("PrivInit:: read LW_PPRIV_MASTER_RING_COMMAND,reg_rd_val=0x%x\n",reg_rd_val);
    } while ((reg_rd_val & mask) != 0);

    
    //check the active cluster numbers.
    //SYS
    DebugPrintf("PrivInit: SYS\n");
    if (!(reg = m_regMap->FindRegister("LW_PPRIV_MASTER_RING_ENUMERATE_RESULTS_SYS"))) {
        ErrPrintf("PrivInit:: register LW_PPRIV_MASTER_RING_ENUMERATE_RESULTS_SYS is not defined!\n");
        err++;
    }
    if (!(field = reg->FindField("LW_PPRIV_MASTER_RING_ENUMERATE_RESULTS_SYS_COUNT"))) {
        ErrPrintf("PrivInit:: field LW_PPRIV_MASTER_RING_ENUMERATE_RESULTS_SYS_COUNT is not defined!\n");
        err++;
    }
    if (!(value = field->FindValue("LW_PPRIV_MASTER_RING_ENUMERATE_RESULTS_SYS_COUNT_NONE"))) {
        ErrPrintf("PrivInit:: value LW_PPRIV_MASTER_RING_ENUMERATE_RESULTS_SYS_COUNT_NONE is not defined!\n");
        err++;
    }
    addr = reg->GetAddress();
    reg_rd_val = lwgpu->RegRd32(addr);
    DebugPrintf("PrivReInit:: read LW_PPRIV_MASTER_RING_ENUMERATE_RESULTS_SYS,reg_rd_val=0x%x\n",reg_rd_val);
    if (reg_rd_val == value->GetValue()) {
        ErrPrintf("PrivInit:: the SYS count is zero!\n");
        err++;
    }
    //FBP
    DebugPrintf("PrivInit: Skipping FBP\n");
    //GPC
    DebugPrintf("PrivInit: GPC\n");
    if (!(reg = m_regMap->FindRegister("LW_PPRIV_MASTER_RING_ENUMERATE_RESULTS_GPC"))) {
        ErrPrintf("PrivInit:: register LW_PPRIV_MASTER_RING_ENUMERATE_RESULTS_GPC is not defined!\n");
        err++;
    }
    if (!(field = reg->FindField("LW_PPRIV_MASTER_RING_ENUMERATE_RESULTS_GPC_COUNT"))) {
        ErrPrintf("PrivInit:: field LW_PPRIV_MASTER_RING_ENUMERATE_RESULTS_GPC_COUNT is not defined!\n");
        err++;
    }
    if (!(value = field->FindValue("LW_PPRIV_MASTER_RING_ENUMERATE_RESULTS_GPC_COUNT_NONE"))) {
        ErrPrintf("PrivInit:: value LW_PPRIV_MASTER_RING_ENUMERATE_RESULTS_GPC_COUNT_NONE is not defined!\n");
        err++;
    }
    addr = reg->GetAddress();
    reg_rd_val = lwgpu->RegRd32(addr);
    DebugPrintf("PrivInit:: read LW_PPRIV_MASTER_RING_ENUMERATE_RESULTS_GPC,reg_rd_val=0x%x\n",reg_rd_val);
    if (reg_rd_val == value->GetValue()) {
        ErrPrintf("PrivInit:: the GPC count is zero!\n");
        err++;
    }

    return (0);
}

UINT32 GC6plus_Maxwell::check_clamp_gc6plus(bool fuse_blown) {
    return 0;
}

//********************************************************************************
// i2cRdByte function lwstomized for GC6 plus
//********************************************************************************
int GC6plus_Maxwell::SetThermI2csValid(GpuSubdevice *pSubdev)
{
    unique_ptr<IRegisterClass> reg;
    UINT32 reg_addr;
    UINT32 reg_data;
    Find_Register(m_regMap, reg, "LW_PGC6_SCI_GPU_I2CS_REPLAY");
    reg_addr = reg->GetAddress();
    reg_data = pSubdev->RegRd32(reg_addr);
    reg_data = ModifyFieldVal(reg.get(), reg_data, "LW_PGC6_SCI_GPU_I2CS_REPLAY", "_THERM_I2CS_STATE", "_VALID");
    pSubdev->RegWr32(reg_addr, reg_data);
    DebugPrintf("Setting Therm I2CS Replay State to Valid 0x%x\n",reg_data);
    
    return (0);
}

int GC6plus_Maxwell::gc6plus_i2cRdByte( int i2c_dev,  int i2c_adr, UINT32 *i2c_rdat,int use32, GpuSubdevice *pSubdev, int therm_i2c_access)
{
    UINT32     i2c_rdat_value = 0;
    UINT32   sm_cmd_done = 0 ; 
    UINT32   sm_no_ack = 0; 
    UINT32   read_i2c_dev = 0;
    UINT32   read_i2c_adr = 0;
    int      num_try;
    if(gc6_abort_i2c_check == 1){
        this->gc6plus_triggerMode(pSubdev); 
    }
    Platform:: EscapeWrite ("sm_dev_addr", 0, 7, i2c_dev);
    Platform:: EscapeRead ("sm_dev_addr", 0, 7, &read_i2c_dev);
    DebugPrintf("i2c dev adr read back - %08x ....\n",read_i2c_dev);
    Platform:: EscapeWrite ("sm_dev_cmd", 0, 8, i2c_adr);
    Platform:: EscapeRead ("sm_dev_cmd", 0, 8, &read_i2c_adr);
    DebugPrintf("i2c dev cmd read back - %08x ....\n",i2c_adr);

    if (use32 == 1) {
        DebugPrintf("i2cRdWord() - %08x starting ....\n",i2c_adr);
        Platform:: EscapeWrite ("sm_ctrl_cmd", 0, 4, READ_WORD);
    } else {
        DebugPrintf("i2cRdByte() - %08x starting ....\n",i2c_adr);
        Platform:: EscapeWrite ("sm_ctrl_cmd", 0, 4, READ_BYTE);
    }
  
    Platform:: EscapeWrite ("sm_ctrl_we", 0, 1, 1);
    DebugPrintf("I2C rd byte function done with sm_ctrl_we escape write : 1\n");
    Platform::Delay(2); //2us pulse to start the i2c transaction
    Platform:: EscapeWrite ("sm_ctrl_we", 0, 1, 0); // start transaction
    DebugPrintf("I2C rd byte function done with sm_ctrl_we escape write : 0\n");
    Platform::Delay(8); // delay to detect therm address and in turn the I2C event

    Platform::Delay(3); // delay to detect therm address and in turn the I2C event
    Platform::DelayNS(8);// delay in 10s of ns to match the I2C and GPIO wakeup events 
    // Asserting and Deasserting GPIO to trigger GPIO event in case of simultaneous i2c and gpio test
    if(i2c_gpio_check == 1){
        this->assertGpio(pSubdev, "misc", 0);    
        Platform::Delay(10);
        this->deassertGpio(pSubdev, "misc", 0);
    }
    if(therm_i2c_access == 1){
        if(this->mode == GC6 || this->mode == GC5){
            this->gc6plus_cleanup(pSubdev);
        }
        DebugPrintf("I2C rd byte function writing to therm valid register\n");
        this->SetThermI2csValid(pSubdev);
    }
  
    num_try = 1;  
    Platform::Delay(40); // delay before we start polling on the I2C transaction done and ack signals.
    Platform:: EscapeRead("sm_done", 0, 1, &sm_cmd_done);
    Platform:: EscapeRead("sm_no_ack", 0, 1, &sm_no_ack);
    DebugPrintf("BESTCmd I2C cmd sm_done :%x -- try %d  \n",sm_cmd_done,num_try);
    DebugPrintf("BESTCmd I2C cmd sm_no_ack :%x \n",sm_no_ack);

    while((num_try < 6) && (sm_cmd_done == 0x0)) { 
        Platform::Delay(30);
        num_try = num_try +1;
        Platform:: EscapeRead("sm_done", 0, 1, &sm_cmd_done);
        Platform:: EscapeRead("sm_no_ack", 0, 1, &sm_no_ack);
        DebugPrintf("BESTCmd.v I2C cmd sm_done :%x -- try %d \n",sm_cmd_done,num_try);
        DebugPrintf("BESTCmd I2C cmd sm_no_ack :%x \n",sm_no_ack);
    }

    if (sm_cmd_done ==0x1) { 
        if (sm_no_ack == 0x1) {
            ErrPrintf("I2C Rd_byte NO ACK : adr=%08x data=%08x \n",i2c_adr,i2c_rdat_value);
            Platform:: EscapeWrite ("sm_done", 0, 1, 0);
            return (1);
        } else {
            Platform:: EscapeRead("sm_rdata", 0, 32, &i2c_rdat_value);
            DebugPrintf("I2C Rd_byte Done : adr=%08x data=%08x \n",i2c_adr,i2c_rdat_value);
        }
    } 
    else {
        ErrPrintf("I2C Rd_byte : TimeOut\n");
        Platform:: EscapeWrite ("sm_done", 0, 1, 0);
        return (1);
    }

    *i2c_rdat = i2c_rdat_value;
    Platform:: EscapeWrite ("sm_done", 0, 1, 0);

    return(0);
}// end_i2cRdByte

int GC6plus_Maxwell::gc6plus_i2cWrByte(int i2c_dev, int i2c_adr, UINT32 i2c_wdat, int use32) 
{
    UINT32   sm_cmd_done ; 
    UINT32   sm_no_ack; 
    int      num_try;


    Platform:: EscapeWrite ("sm_dev_addr", 0, 7, i2c_dev);
    Platform:: EscapeWrite ("sm_dev_cmd",  0, 8, i2c_adr);
    if (use32 == 1) {
        DebugPrintf("i2cWrWord() - %08x starting ....\n",i2c_adr);
        Platform:: EscapeWrite ("sm_ctrl_cmd", 0, 4, WRITE_WORD);
    } else {
        DebugPrintf("i2cWrByte() - %08x starting ....\n",i2c_adr);
        Platform:: EscapeWrite ("sm_ctrl_cmd", 0, 4, WRITE_BYTE);}
    Platform:: EscapeWrite ("sm_wdata", 0, 32, i2c_wdat);
    Platform:: EscapeWrite ("sm_data_we", 0, 1, 1);
    Platform:: EscapeWrite ("sm_ctrl_we", 0, 1, 1);

    Platform::Delay(2); //2us delay
    Platform:: EscapeWrite ("sm_data_we", 0, 1, 0);
    Platform:: EscapeWrite ("sm_ctrl_we", 0, 1, 0);
   
    Platform::Delay(20); //2us delay
  
    num_try = 1;
    Platform:: EscapeRead("sm_done", 0, 1, &sm_cmd_done);
    Platform:: EscapeRead("sm_no_ack", 0, 1, &sm_no_ack);
    DebugPrintf("BESTCmd I2C cmd sm_done : %08x -- try %d \n",sm_cmd_done,num_try);

    while((num_try < 6) && (sm_cmd_done == 0x0)) { 
        Platform::Delay(30);
        Platform:: EscapeRead("sm_done", 0, 1, &sm_cmd_done);
        Platform:: EscapeRead("sm_no_ack", 0, 1, &sm_no_ack);
        num_try = num_try +1;
        DebugPrintf("BESTCmd I2C cmd sm_done : %08x -- try %d \n",sm_cmd_done,num_try);
        

    }
     
    if (sm_cmd_done ==0x1) 
    { 
        if (sm_no_ack == 0x1)
        {
            ErrPrintf("I2C Wr_byte NO ACK : adr=%08x data=%08x \n",i2c_adr,i2c_wdat);return (1);
        } else {
            DebugPrintf("I2C Wr_byte Done : adr=%08x data=%08x \n",i2c_adr,i2c_wdat);
        }
    } else {
        ErrPrintf("I2C Wr_byte : TimeOut\n"); return (1);}

    Platform:: EscapeWrite ("sm_done", 0, 1, 0);

    Platform::Delay(20); //2us delay
    return(0);
}// end_i2cWrByte
   

// This routine polls on the SCI Master FSM state using EscapeReads.
// It can poll until the state is equal or !equal to the desired value.
UINT32
GC6plus_Maxwell::poll_for_fsm_state(const char *state, bool equal)
{
    unique_ptr<IRegisterClass> reg;
    unique_ptr<IRegisterField> field;
    unique_ptr<IRegisterValue> value;
    string regname, fieldname, valuename;
    UINT32 state_value, fsm_state, polling_attempts;
    bool done;

    // get value of state from LW_PGC6_SCI_DEBUG_MASTER_FSM_STATE
    regname = "LW_PGC6_SCI_DEBUG_MASTER_FSM";
    if (!(reg = m_regMap->FindRegister(regname.c_str())))
    {
        ErrPrintf("gc6plus_maxwell: Failed to find %s register\n", regname.c_str());
        return (1);
    }
    fieldname = "LW_PGC6_SCI_DEBUG_MASTER_FSM_STATE";
    if (!(field = reg->FindField(fieldname.c_str())))
    {
        ErrPrintf("gc6plus_maxwell: Failed to find %s field\n", fieldname.c_str());
        return (1);
    }
    valuename = fieldname + "_" + state;
    if (!(value = field->FindValue(valuename.c_str())))
    {
        ErrPrintf("gc6plus_maxwell: Failed to find %s value\n", valuename.c_str());
        return (1);
    }
    state_value = value->GetValue();

    InfoPrintf("gc6plus_maxwell: Polling sci_debug_fsm_state to be %0s to %s "
        "(0x%x)\n", equal ? "equal" : "not equal", state, state_value);

    // We limit the number of polls so we do not hang the test.
    polling_attempts = 1;
    Platform::EscapeRead("sci_debug_fsm_state", 0, 4, &fsm_state);
    if (equal)
    {
        done = (fsm_state == state_value);
    }
    else
    {
        done = (fsm_state != state_value);
    }
    while (!done && (polling_attempts < m_polling_timeout_us))
    {
        if ((polling_attempts % 10) == 1)
        {
            InfoPrintf("gc6plus_maxwell: Polling attempt #%d: "
                "sci_debug_fsm_state = 0x%x\n", polling_attempts, fsm_state);
        }
        Platform::Delay(1);  // delay 1 microsecond
        polling_attempts = polling_attempts + 1;
        Platform::EscapeRead("sci_debug_fsm_state", 0, 4, &fsm_state);
        if (equal)
        {
            done = (fsm_state == state_value);
        }
        else
        {
            done = (fsm_state != state_value);
        }
    }

    InfoPrintf("gc6plus_maxwell: Polling done after attempt #%d: "
        "sci_debug_fsm_state = 0x%x\n", polling_attempts, fsm_state);
    if (!done)
    {
        InfoPrintf("gc6plus_maxwell: Timeout polling sci_debug_fsm_state to be "
            "%0s to %s after %d attempts\n", equal ? "equal" : "not equal",
            state, m_polling_timeout_us);
        return (1);
    }

    InfoPrintf("gc6plus_maxwell: Done polling for sci_debug_fsm_state\n");
    return (0);
}
//************************************************************************************************
// i2cRdByte function lwstomized for GC6 plus I2C Timeout test: gc6plus_gc6_exit_i2c_timeout_Test()  
//************************************************************************************************
int GC6plus_Maxwell::gc6plus_i2cRdByte_timeout( int i2c_dev,  int i2c_adr, UINT32 *i2c_rdat,int use32, GpuSubdevice *pSubdev, int therm_i2c_access)
{
    UINT32     i2c_rdat_value = 0;
    UINT32   sm_cmd_done = 0 ; 
    UINT32   sm_no_ack = 0; 
    UINT32   read_i2c_dev = 0;
    UINT32   read_i2c_adr = 0;
    int      num_try;

    Platform:: EscapeWrite ("sm_dev_addr", 0, 7, i2c_dev);
    Platform:: EscapeRead ("sm_dev_addr", 0, 7, &read_i2c_dev);
    DebugPrintf("i2c dev adr read back - %08x ....\n",read_i2c_dev);
    Platform:: EscapeWrite ("sm_dev_cmd", 0, 8, i2c_adr);
    Platform:: EscapeRead ("sm_dev_cmd", 0, 8, &read_i2c_adr);
    DebugPrintf("i2c dev cmd read back - %08x ....\n",i2c_adr);

    if (use32 == 1) {
        DebugPrintf("i2cRdWord() - %08x starting ....\n",i2c_adr);
        Platform:: EscapeWrite ("sm_ctrl_cmd", 0, 4, READ_WORD);
    } else {
        DebugPrintf("i2cRdByte() - %08x starting ....\n",i2c_adr);
        Platform:: EscapeWrite ("sm_ctrl_cmd", 0, 4, READ_BYTE);
    }
  
    Platform:: EscapeWrite ("sm_ctrl_we", 0, 1, 1);
    DebugPrintf("I2C rd byte function done with sm_ctrl_we escape write : 1\n");
    Platform::Delay(2); //2us pulse to start the transaction. We deassert the value so as not to do a repeat transaction.
    Platform:: EscapeWrite ("sm_ctrl_we", 0, 1, 0); // start transaction
    DebugPrintf("I2C rd byte function done with sm_ctrl_we escape write : 0\n");

    Platform::Delay(10);// delay for therm address to be detected which triggers the I2C wakeup event
    num_try = 1;   
    Platform:: EscapeRead("sm_done", 0, 1, &sm_cmd_done);
    Platform:: EscapeRead("sm_no_ack", 0, 1, &sm_no_ack);
    DebugPrintf("BESTCmd I2C cmd sm_done :%08x -- try %d  \n",sm_cmd_done,num_try);
    DebugPrintf("BESTCmd I2C cmd sm_no_ack :%08x \n",sm_no_ack);

    while((num_try < 120) && (sm_cmd_done == 0x0)) { 
        Platform::Delay(1); // delay before polling again on i2c transaction done and ack signals.
        num_try = num_try +1;
        Platform:: EscapeRead("sm_done", 0, 1, &sm_cmd_done);
        Platform:: EscapeRead("sm_no_ack", 0, 1, &sm_no_ack);
        DebugPrintf("BESTCmd.v I2C cmd sm_done :%08x -- try %d \n",sm_cmd_done,num_try);
        DebugPrintf("BESTCmd I2C cmd sm_no_ack :%08x \n",sm_no_ack);
    }
    if (therm_i2c_access == 1){
        if(this->mode == GC6 || this->mode == GC5){
            this->gc6plus_cleanup(pSubdev);
        }
        DebugPrintf("I2C rd byte function writing to therm valid register\n");
        this->SetThermI2csValid(pSubdev);
    }
    
    if (sm_cmd_done ==0x1) { 
        if (sm_no_ack == 0x1) {
            ErrPrintf("I2C Rd_byte NO ACK : adr=%08x data=%08x \n",i2c_adr,i2c_rdat_value);
            Platform:: EscapeWrite ("sm_done", 0, 1, 0);
            return (1);
        } else {
            Platform:: EscapeRead("sm_rdata", 0, 32, &i2c_rdat_value);
            DebugPrintf("I2C Rd_byte Done : adr=%08x data=%08x \n",i2c_adr,i2c_rdat_value);
          }
    } 
    else {
        ErrPrintf("I2C Rd_byte : TimeOut\n");
        Platform:: EscapeWrite ("sm_done", 0, 1, 0);
        return (1);
    }

    *i2c_rdat = i2c_rdat_value;
    Platform:: EscapeWrite ("sm_done", 0, 1, 0);

    return(0);
}// end_i2cRdByte_timeout

//****************************************************************************************
//  Test SCI Timeout for GC6 exit: SCI timeout refers to SCL stretching exceeding the timeout
//  value since the I2CS THERM STATE is still INVALID and the buffered transaction is dropped.
//  Any further I2C transactions should be carried out thereafter.
//****************************************************************************************

UINT32 GC6plus_Maxwell::gc6plus_gc6_exit_i2c_timeout_Test() {  
    unique_ptr<IRegisterClass> reg;
    UINT32 reg_addr;
    UINT32 reg_data;

    UINT32   expect_value;
    UINT32   i2c_rdat;
    int   rd_status;    
    GpuSubdevice *pSubdev = lwgpu->GetGpuSubdevice();
    UINT32  i2c_reg_adr, i2c_slv_adr;
    i2c_slv_adr = DEVICE_ADDR_0_FERMI;
    i2c_reg_adr = 0xFE; //for therm
    
    int wakeup_timer_us = 500; //Delay is large as we don't expect islands to exit on wakeup timer. This is just a fall back exiting option so that the test does not hang.

    RETURN_IF_ERR(this->gc6plus_init(pSubdev));
    //Enable i2c event Wakeup
    ReadWriteRegFieldVal(pSubdev, m_regMap, "LW_PGC6_SCI_WAKE_EN", "_I2CS", "_ENABLE");

    Find_Register(m_regMap, reg, "LW_PGC6_SCI_GPU_I2CS_CFG");
    reg_addr = reg->GetAddress();
    reg_data = pSubdev->RegRd32(reg_addr);
    reg_data = ModifyFieldValWithData(reg.get(), reg_data, "LW_PGC6_SCI_GPU_I2CS_CFG", "_I2C_ADDR", DEVICE_ADDR_0_FERMI);
    reg_data = ModifyFieldVal(reg.get(), reg_data, "LW_PGC6_SCI_GPU_I2CS_CFG", "_SCL_TIMEOUT_SCALE", "_1USEC");
    pSubdev->RegWr32(reg_addr, reg_data);
    
    ////Making THERM I2CS state INVALID 
    Find_Register(m_regMap, reg, "LW_PGC6_SCI_GPU_I2CS_REPLAY");
    reg_addr = reg->GetAddress();
    reg_data = pSubdev->RegRd32(reg_addr);
    reg_data = ModifyFieldVal(reg.get(), reg_data, "LW_PGC6_SCI_GPU_I2CS_REPLAY", "_THERM_I2CS_STATE", "_ILWALID");
    pSubdev->RegWr32(reg_addr, reg_data);
    DebugPrintf("Setting Therm I2CS Replay State to Invalid 0x%x\n",reg_data);
   
    //GCX entry after setting up wakeup timer
    this->configure_wakeup_timer(pSubdev, wakeup_timer_us);
    this->wakeupTimerEvent(pSubdev, "enable");
    //Enter GC5/GC6
    this->gc6plus_triggerMode(pSubdev);

    Platform::Delay(15); //delay for 15us for GC6 to kick in 
    
    //Trigger I2C event         
    Platform::EscapeWrite("drv_opt_i2cs_addr_sel", 0, 2, 0x0);
    DebugPrintf("Done with Escape write: drv_opt_i2cs_addr_sel = 0x0\n");
    
    expect_value = 0xDE; // Value stored in 0xFE register (dev_i2cs.ref) which is the device id 
    rd_status = gc6plus_i2cRdByte_timeout(i2c_slv_adr,i2c_reg_adr,&i2c_rdat,0, pSubdev, 1); // slave addr= 0x4F, reg. addr = 0xFE, 0 = read byte
    if ( !rd_status ) { 
        InfoPrintf("Read status = 0 : 1st I2C read transaction done.\n");       
        if ( i2c_rdat != expect_value) { 
            InfoPrintf("1st read: DEV_ADDR_0 -- read data mismatch, expect 0x%08x , got 0x%08x\n", expect_value,i2c_rdat);
        }
        else {
            InfoPrintf("1st read: DEV_ADDR_0 -- read data match, expect 0x%08x , got 0x%08x\n", expect_value,i2c_rdat);
        }   
    }
    else {
            InfoPrintf("Read status = 1 : 1st I2C read timed out!\n"); 
    }   
    // Changing back the SCL timeout scale to 1MSEC from 1USEC
    Find_Register(m_regMap, reg, "LW_PGC6_SCI_GPU_I2CS_CFG");
    reg_addr = reg->GetAddress();
    reg_data = pSubdev->RegRd32(reg_addr);
    reg_data = ModifyFieldValWithData(reg.get(), reg_data, "LW_PGC6_SCI_GPU_I2CS_CFG", "_I2C_ADDR", DEVICE_ADDR_0_FERMI);
    reg_data = ModifyFieldVal(reg.get(), reg_data, "LW_PGC6_SCI_GPU_I2CS_CFG", "_SCL_TIMEOUT_SCALE", "_1MSEC");
    pSubdev->RegWr32(reg_addr, reg_data);
    DebugPrintf("LW_PGC6_SCI_GPU_I2CS_CFG = 0x%x\n",reg_data); 
    Platform::Delay(10); //delay for getting back to STEADY state and 2nd transaction can be made
    // Second Read Byte Request which should not time out
    rd_status = gc6plus_i2cRdByte_timeout(i2c_slv_adr,i2c_reg_adr,&i2c_rdat,0, pSubdev, 1); // slave addr= 0x4F, reg. addr = 0xFE, 0 = read byte
    if ( rd_status ) {
            InfoPrintf("Read status = 1 : 2nd I2C read timed out!\n");  
    } 
    else {
        InfoPrintf("Read status = 0 : 2nd I2C read done!\n");
        if ( i2c_rdat != expect_value) { 
            InfoPrintf("2nd read: DEV_ADDR_0 -- read data mismatch, expect 0x%08x , got 0x%08x\n", expect_value,i2c_rdat);
        }
        else {
            InfoPrintf("2nd read: DEV_ADDR_0 -- read data match, expect 0x%08x , got 0x%08x\n", expect_value,i2c_rdat);
        }   
    }           

    if ( i2c_rdat != expect_value) { 
        ErrPrintf("DEV_ADDR_0 -- read data mismatch, expect 0x%08x , got 0x%08x\n", expect_value,i2c_rdat);
        return (1);
    }
             
    //Check if Exit oclwrred
    return ((
            checkSteadyState(pSubdev) //we are now in steady state
            && check_sw_intr_status(pSubdev) //on the expected wakeup events
            ) ? 0: 1);
}  

//********************************************************************************************
// IFF_write_check() and IFR_write_check() functions to corrupt the registers before GC6 entry  
//********************************************************************************************

int GC6plus_Maxwell::IFF_write_check(GpuSubdevice *pSubdev)
{
    InfoPrintf("!!!!!!!!!!! IFF write check enter!!!!!!!!!!!");
    //For registers like LW_PBUS_SW_INTR_1/1(Software interrupt scratch registers), the corrupted value is random. 
    //For other registers only those bits have been corrupted which were also modified as part of IFF/IFR. 
    if(write_read_check(pSubdev, "LW_PBUS_SW_INTR_1", 0x00000000, 0x00000012)){
        ErrPrintf(" ERROR: write_read_check()\n");
        return 1;
    }
    if(write_read_check(pSubdev, "LW_PBUS_SW_INTR_2", 0x00000000, 0x00000023)){
        ErrPrintf(" ERROR: write_read_check()\n");
        return 1;
    }
    //FIXME: Fullchip IFF/IFR team, please fix this, due to the changes to the fuses.
    //if(write_read_check(pSubdev, "LW_PEXTDEV_BOOT_3", 0xFDFFFFFF, 0x01000000)){  
    //    ErrPrintf(" ERROR: write_read_check()\n");
    //    return 1;
    //}
    if(write_read_check(pSubdev, "LW_PBUS_DEBUG_0", 0xF0F00F0F, 0x8F8F8F8F)){
        ErrPrintf(" ERROR: write_read_check()\n");
        return 1;
    }
    if(write_read_check(pSubdev, "LW_GPIO_OUTPUT_CNTL(10)", 0xFFFFFF00, 0x00000058)){
        ErrPrintf(" ERROR: write_read_check()\n");
        return 1;
    }
    if(write_read_check(pSubdev, "LW_PTRIM_SYS_LTCPLL_CFG2", 0xFFFFFFFF, 0x00380000)){
         ErrPrintf(" ERROR: write_read_check()\n");
        return 1;
    }
    if(write_read_check(pSubdev, "LW_THERM_I2CS_SENSOR_03", 0xFFFFFFFF, 0x0000000D)){
        ErrPrintf(" ERROR: write_read_check()\n");
        return 1;
    }
    InfoPrintf("!!! IFF write check exit !!!");
    return 0;                           
}

int GC6plus_Maxwell::IFR_write_check(GpuSubdevice *pSubdev)
{
    InfoPrintf("!!!!!!!!!!! IFR write check enter!!!!!!!!!!!");
    if(write_read_check(pSubdev, "LW_PBUS_SW_INTR_1", 0xFFFFFFFF ,0x00000034)){
        ErrPrintf(" ERROR: write_read_check()\n");
        return 1;
    }
    if(write_read_check(pSubdev, "LW_PBUS_SW_INTR_2", 0xFFFFFFFF, 0x00000045)){
        ErrPrintf(" ERROR: write_read_check()\n");
        return 1;
    }
    //FIXME: Fullchip IFF/IFR team, please fix this, due to the changes to the fuses.
    //if(write_read_check(pSubdev, "LW_PEXTDEV_BOOT_3", 0xFDFFFFFF, 0x01000000)){ 
    //    ErrPrintf(" ERROR: write_read_check()\n");
    //    return 1;
    //}
    if(write_read_check(pSubdev, "LW_PBUS_DEBUG_0", 0xF0F00F0F, 0x8F8F8F8F)){
        ErrPrintf(" ERROR: write_read_check()\n");
        return 1;
    }
    if(write_read_check(pSubdev, "LW_GPIO_OUTPUT_CNTL(10)", 0xFFFFFF00, 0x00000058)){
        ErrPrintf(" ERROR: write_read_check()\n");
        return 1;
    }
    if(write_read_check(pSubdev, "LW_PTRIM_SYS_LTCPLL_CFG2", 0xFFFFFFFF, 0x00380000)){
        ErrPrintf(" ERROR: write_read_check()\n");
        return 1;
    }
    if(write_read_check(pSubdev, "LW_THERM_I2CS_SENSOR_03", 0xFFFFFFFF, 0x0000000D)){
        ErrPrintf(" ERROR: write_read_check()\n");
        return 1;
    }
    InfoPrintf("!!!!!!!!!!!! IFR write check exit !!!!!!!!!!!");
    return 0;                       
}

//********************************************************************************************
// IFF_read_check() and IFR_read_check() functions verifying IFF and IFR at GC6 exit 
//********************************************************************************************

bool GC6plus_Maxwell::IFF_read_check(GpuSubdevice *pSubdev)
{
    InfoPrintf("!!!!!!!!!!! IFF read check enter !!!!!!!!!!!");
    //the and_mask and or_mask (or the expected value in some cases) provided here are same as used at the time of creating the fuse_image.
    if(check_dw(pSubdev,"LW_PBUS_SW_INTR_1",0x00000058,1)){
        return 1;
    }
    if(check_dw(pSubdev,"LW_PBUS_SW_INTR_2",0xbadcbabe,1)){
        return 1;
    }
    //FIXME: Fullchip IFF/IFR team, please fix this, due to the changes to the fuses.
    //if(check_rmw(pSubdev,"LW_PEXTDEV_BOOT_3",0xFDFFFFFF,0x81000000,1)){ 
    //    return 1;
    //}
    if(check_rmw(pSubdev,"LW_PBUS_DEBUG_0",0xF0F00F0F,0x8F8E8F8E,1)){
        return 1;
    }
    if(check_rmw(pSubdev,"LW_GPIO_OUTPUT_CNTL(10)",0xFFFFFF00,0x0000005A,1)){
        return 1;
    }
    if(check_rmw(pSubdev,"LW_PTRIM_SYS_LTCPLL_CFG2",0xFFFFFFFF,0x003A0000,1)){
        return 1;
    }
    if(check_rmw(pSubdev,"LW_THERM_I2CS_SENSOR_03",0xFFFFFFF0,0x0000000F,1)){
        return 1;
    }
    InfoPrintf("!!! IFF read check exit !!!");
    return 0; 
}

bool GC6plus_Maxwell::IFR_read_check(GpuSubdevice *pSubdev)
{
    InfoPrintf("!!! IFR read check enter !!!");
    if(check_rmw(pSubdev,"LW_PBUS_SW_INTR_1",0xBFFFF000,0x0000000C,0)){
        return 1;
    }
    if(check_dw(pSubdev,"LW_PBUS_SW_INTR_2",0Xbadcbabe,0)){
        return 1;
    }
    //FIXME: Fullchip IFF/IFR team, please fix this, due to the changes to the fuses.
    //if(check_rmw(pSubdev,"LW_PEXTDEV_BOOT_3",0xFDFFFFFF,0x81000000,0)){ 
    //    return 1;
    //}
    if(check_rmw(pSubdev,"LW_PBUS_DEBUG_0",0xF0F00F0F,0x8F8E8F8E,0)){
        return 1;
    }
    if(check_rmw(pSubdev,"LW_GPIO_OUTPUT_CNTL(10)",0xFFFFFF00,0x0000005A,0)){
        return 1;
    } 
    if(check_rmw(pSubdev,"LW_PTRIM_SYS_LTCPLL_CFG2",0xFFFFFFFF,0x003A0000,0)){
        return 1;
    }
    if(check_rmw(pSubdev,"LW_THERM_I2CS_SENSOR_03",0xFFFFFFFF,0x0000000F,0)){
        return 1;
    }
    InfoPrintf("!!! IFR read check exit !!!");
    return 0;
  
}

//********************************************************************************
// BSI2PMU testing bar 0 access with PMU using AUTO increament RAM content file
//********************************************************************************
UINT32
GC6plus_Maxwell::gc6plus_bsi2pmu_bar0_maxwellTest(string &bsiRamFileName) {
    unique_ptr<IRegisterClass> reg;
    UINT32 reg_addr;
    UINT32 reg_data;

    GpuSubdevice *pSubdev = lwgpu->GetGpuSubdevice();

    int timeout_count = 100000;
    //-----------------------------------------------------------
    // Stage 1 : Initialization
    //----------------------------------------------------------
    DebugPrintf("Gc6plus bsi2pmu bar 0 Test : Stage 1: Initialization\n");
    //check clamp_en and sleep signal to make sure that when the fuse is not blown
    //bool fuse_blown = false;
    //if(check_clamp_gc6plus(fuse_blown) == 1) return 1;

    // Clear PMU in Reset
    Find_Register(m_regMap, reg, "LW_PMC_ENABLE");
    reg_addr = reg->GetAddress();
    reg_data = pSubdev->RegRd32(reg_addr);
    pSubdev->RegWr32(reg_addr, reg_data | 0x0000002c ); // enable BUF_reset, xbar, l2, priv_ring
    pSubdev->RegRd32(reg_addr);

    Find_Register(m_regMap, reg, "LW_PPRIV_MASTER_RING_COMMAND_DATA");
    reg_addr = reg->GetAddress();
    pSubdev->RegWr32(reg_addr, 0x503B5443);

    PrivInit();

    Find_Register(m_regMap, reg, "LW_PMC_ENABLE");
    reg_addr = reg->GetAddress();
    reg_data = pSubdev->RegRd32(reg_addr);
    pSubdev->RegWr32(reg_addr, reg_data | 0xffffffff ); //Enable all engines after PRIV init is complete.
    pSubdev->RegRd32(reg_addr);
    DebugPrintf("gc6plus_maxwell: priv_init(): finished\n");

    // Number of bootphases, LW_PGC6_BSI_BOOTPHASES_GC6STOP[19:15], now set as 16 
    Find_Register(m_regMap, reg, "LW_PGC6_BSI_BOOTPHASES");
    reg_addr = reg->GetAddress();
    pSubdev->RegWr32(reg_addr, 0x1 << 15 ); 

    Find_Register(m_regMap, reg, "LW_PGC6_BSI_CTRL");
    reg_addr = reg->GetAddress();
    //CYA Mode on
    //pSubdev->RegWr32(reg_addr, (0x3)); kkedit 
    //Trigger PMU transfer
    pSubdev->RegWr32(reg_addr, (0x1 << 16 | 0x3 | 0x1 << 8));

    //Triggering the BSI_PMUTXTRIG (need the rising edge for RTL)
    Find_Register(m_regMap, reg,"LW_PGC6_BSI_PMUTXTRIG");
    reg_addr = reg->GetAddress();
    pSubdev->RegWr32(reg_addr, 0x0);
    pSubdev->RegWr32(reg_addr, 0x1); 

    //Poll on the last priv write to happen before continue
    Find_Register(m_regMap, reg,"LW_PPWR_PMU_MAILBOX");
    reg_addr = reg->GetAddress(2);
    while(!(pSubdev->RegRd32(reg_addr)  == 0xffffffff)){
        Platform::Delay(1);

        DebugPrintf("gc6plus_maxwell: Polling for PMU_MAILBOX(2) to be 0xffffffff\n");
        timeout_count = timeout_count - 1;

        if( timeout_count == 0){
            ErrPrintf("gc6plus_bsi2pmu_bar0: time out counter in while loop reached. \n");
        }
    }

    Find_Register(m_regMap, reg,"LW_PPWR_PMU_MAILBOX");
    reg_addr = reg->GetAddress(0);
    UINT32 temp_m0 = pSubdev->RegRd32(reg_addr);
    reg_addr = reg->GetAddress(1);
    UINT32 temp_m1 = pSubdev->RegRd32(reg_addr);
    reg_addr = reg->GetAddress(2);
    UINT32 temp_m2 = pSubdev->RegRd32(reg_addr);

    Find_Register(m_regMap, reg,"LW_PGC6_BSI_SCRATCH");
    reg_addr = reg->GetAddress(0);
    UINT32 temp_s0 = pSubdev->RegRd32(reg_addr);
    reg_addr = reg->GetAddress(1);
    UINT32 temp_s1 = pSubdev->RegRd32(reg_addr);
    reg_addr = reg->GetAddress(2);
    UINT32 temp_s2 = pSubdev->RegRd32(reg_addr);
    reg_addr = reg->GetAddress(3);
    UINT32 temp_s3 = pSubdev->RegRd32(reg_addr);

    DebugPrintf("PMU_MAILBOX(0) is %x ZZZZZZZZZZZZZZZZZZ\n", temp_m0);
    DebugPrintf("PMU_MAILBOX(1) is %x ZZZZZZZZZZZZZZZZZZ\n", temp_m1);
    DebugPrintf("PMU_MAILBOX(2) is %x ZZZZZZZZZZZZZZZZZZ\n", temp_m2);
    DebugPrintf("BSI_SCRATCH(0) is %x ZZZZZZZZZZZZZZZZZZ\n", temp_s0);
    DebugPrintf("BSI_SCRATCH(1) is %x ZZZZZZZZZZZZZZZZZZ\n", temp_s1);
    DebugPrintf("BSI_SCRATCH(2) is %x ZZZZZZZZZZZZZZZZZZ\n", temp_s2);
    DebugPrintf("BSI_SCRATCH(3) is %x ZZZZZZZZZZZZZZZZZZ\n", temp_s3);

    if( (temp_m0 == 0xdeaddead) &
        (temp_m1 == 0xbeefbeef) & 
        (temp_m2 == 0xffffffff) & 
        (temp_s0 == 0xaaaaaaaa) &
        (temp_s1 == 0xbbbbbbbb) &
        (temp_s2 == 0xcccccccc) &
        (temp_s3 == 0xdddddddd) ){

        DebugPrintf("bsi2pmu_bar0: PASS, all the know writes has been read and confirmed");
        return 0;
    }else {
        return 1;
    }

    return 0;
}

//****************************************************************************************
// Test GC6plus Island bsi2pmu all 32 phases test 
//****************************************************************************************
UINT32
GC6plus_Maxwell::gc6plus_bsi2pmu_32_phases_maxwellTest(string &bsiRamFileName) {
    unique_ptr<IRegisterClass> reg;
    UINT32 reg_addr;
    UINT32 reg_data;
    UINT32 temp_m1;

    GpuSubdevice *pSubdev = lwgpu->GetGpuSubdevice();

    int timeout_count = 100000;
    //-----------------------------------------------------------
    // Stage 1 : Initialization
    //----------------------------------------------------------
    DebugPrintf("Gc6plus bsi2pmu Test : Stage 1: Initialization\n");
    //check clamp_en and sleep signal to make sure that when the fuse is not blown
    //bool fuse_blown = false;
    //if(check_clamp_gc6plus(fuse_blown) == 1) return 1;

    // Clear PMU in Reset
    Find_Register(m_regMap, reg, "LW_PMC_ENABLE");
    reg_addr = reg->GetAddress();
    reg_data = pSubdev->RegRd32(reg_addr);
    pSubdev->RegWr32(reg_addr, reg_data | 0x0000002c ); // enable BUF_reset, xbar, l2, priv_ring
    pSubdev->RegRd32(reg_addr);

    Find_Register(m_regMap, reg, "LW_PPRIV_MASTER_RING_COMMAND_DATA");
    reg_addr = reg->GetAddress();
    pSubdev->RegWr32(reg_addr, 0x503B5443);

    PrivInit();

    Find_Register(m_regMap, reg, "LW_PMC_ENABLE");
    reg_addr = reg->GetAddress();
    reg_data = pSubdev->RegRd32(reg_addr);
    pSubdev->RegWr32(reg_addr, reg_data | 0xffffffff ); //Enable all engines after PRIV init is complete.
    pSubdev->RegRd32(reg_addr);
    DebugPrintf("gc6plus_maxwell: priv_init(): finished\n");

    // Number of bootphases, LW_PGC6_BSI_BOOTPHASES_GC6STOP[19:15], now set as 32
    Find_Register(m_regMap, reg, "LW_PGC6_BSI_BOOTPHASES");
    reg_addr = reg->GetAddress();
    pSubdev->RegWr32(reg_addr, 0x1f << 15 ); 

    Find_Register(m_regMap, reg, "LW_PGC6_BSI_CTRL");
    reg_addr = reg->GetAddress();
    //CYA Mode on
    pSubdev->RegWr32(reg_addr, (0x3)); 
    //Trigger PMU transfer
    pSubdev->RegWr32(reg_addr, (0x1 << 16 | 0x3 | 0x1 << 8));

    //Triggering the BSI_PMUTXTRIG (need the rising edge for RTL)
    Find_Register(m_regMap, reg,"LW_PGC6_BSI_PMUTXTRIG");
    reg_addr = reg->GetAddress();
    pSubdev->RegWr32(reg_addr, 0x0);
    pSubdev->RegWr32(reg_addr, 0x1);

    Find_Register(m_regMap, reg,"LW_PGC6_SCI_PWR_SEQ_INST");
    reg_addr = reg->GetAddress(63);
    while(!(pSubdev->RegRd32(reg_addr) == 0x40)){    
        Platform::Delay(1);

        timeout_count = timeout_count - 1;

        if( timeout_count == 0){
            ErrPrintf("gc6plus_bsi2pmu_32phases: time out counter in while loop reached. \n");
        }

    }

    Find_Register(m_regMap, reg,"LW_PGC6_SCI_PWR_SEQ_INST");
    for(UINT32 i=0; i< 64;i++){
        reg_addr = reg->GetAddress(i);
        temp_m1 = pSubdev->RegRd32(reg_addr);
        if(temp_m1 != (i+1)){

            ErrPrintf("gc6plus_maxwell: %s Register mismatch from expected. Expected 0x%08x, Actual 0x%08x = 0x%08x \n",__FUNCTION__, (i+1), reg_addr, temp_m1 );
            return (1);
        }
    }

    InfoPrintf("gc6plus_maxwell: %s PASSED. \n",__FUNCTION__);
    return 0;
}

UINT32 GC6plus_Maxwell::rtd3_sli_Clockgating(GpuSubdevice *pSubdev, std::string input){

    int err = 0;
    UINT32 time_out_cnt = 500;
    UINT32 cg_status = 0;
    UINT32 FGC6_CG_STATUS = 0x0000003f;  //wait for all clocks to be stopped

    
    if (input == "enable"){
	InfoPrintf("RTD3_SLI: rtd3_sli_Clockgating: Enable Clock gating \n");

	InfoPrintf("RTD3_SLI : writing LW_PTRIM_SYS_FGC6_CLK_GATE to gate clocks \n");

	EscapeWrite("sys_fgc6_clk_gate_mgmt_clk_disable", 0, 1, 1);
	EscapeWrite("sys_fgc6_clk_gate_host_clk_disable", 0, 1, 1);
	EscapeWrite("sys_fgc6_clk_gate_alt_lwl_common_clk_disable", 0, 1, 1);

	InfoPrintf("RTD3_SLI : writing LW_PTRIM_SYS_AUX_TX_RDET_DIV to gate clocks \n");
	EscapeWrite("sys_fgc6_aux_rx_bypass_refclk_disable", 0, 1, 1);

	InfoPrintf("RTD3_SLI : writing LW_PTRIM_SYS_LWLINK_AUX_TX_RDET_DIV to gate clocks \n");
	EscapeWrite("lwlink_fgc6_aux_gate", 0, 1, 1);
	EscapeWrite("lwlink_fgc6_aux_gate_ovrd", 0, 1, 1);

	InfoPrintf("RTD3_SLI : writing LW_PTRIM_SYS_LWLINK_FGC6_CLK_GATE to gate clocks \n");
	EscapeWrite("lwlink_fgc6_clk_gate", 0, 1, 1);


	InfoPrintf("RTD3_SLI: Pooling LW_PTRIM_SYS_FGC6_CLK_GATE for clocks to be gated - including alt_xclk  \n");
	EscapeRead("sys_fgc6_clk_gate", 0, 9, &cg_status);

	while(cg_status != FGC6_CG_STATUS)
	{
	    if(time_out_cnt == 0)
	    {
        	InfoPrintf("FGC6 : Timeout polling FGC6 clk gate  \n");
		ErrPrintf("fgc6plus_clockgating: Timeout polling on clock not gated after 1000 loops. \n");
		err++; 
		return 1;
	    }
	    time_out_cnt--;
	    Platform::Delay(10);  

	    EscapeRead("sys_fgc6_clk_gate", 0, 9, &cg_status);
	    cg_status &= FGC6_CG_STATUS; 

	    DebugPrintf("gc6plus_fgc6Mode(): Polling on clock gated loop %d.\n",(1000-time_out_cnt));
	}


    }
    else if(input == "disable"){
	InfoPrintf("RTD3_SLI:  Disable LWLINK Clock gating \n");

	InfoPrintf("RTD3_SLI : writing LW_PTRIM_SYS_FGC6_CLK_GATE to ungate clocks \n");
	EscapeWrite("sys_fgc6_clk_gate_mgmt_clk_disable", 0, 1, 0);
	EscapeWrite("sys_fgc6_clk_gate_host_clk_disable", 0, 1, 0);
	EscapeWrite("sys_fgc6_clk_gate_alt_lwl_common_clk_disable", 0, 1, 0);

	InfoPrintf("RTD3_SLI : writing LW_PTRIM_SYS_AUX_TX_RDET_DIV to ungate clocks\n");
	EscapeWrite("sys_fgc6_aux_rx_bypass_refclk_disable", 0, 1, 0);

	InfoPrintf("RTD3_SLI : writing LW_PTRIM_SYS_LWLINK_AUX_TX_RDET_DIV to ungate clocks\n");
	EscapeWrite("lwlink_fgc6_aux_gate", 0, 1, 0);
	EscapeWrite("lwlink_fgc6_aux_gate_ovrd", 0, 1, 0);

	InfoPrintf("RTD3_SLI : writing LW_PTRIM_SYS_LWLINK_FGC6_CLK_GATE to ungate clocks\n");
	EscapeWrite("lwlink_fgc6_clk_gate", 0, 1, 0);
	
	InfoPrintf("RTD3_SLI : Pooling LW_PTRIM_SYS_FGC6_CLK_GATE for clocks to be ungated \n");
	EscapeRead("sys_fgc6_clk_gate", 0, 9, &cg_status);

	while(cg_status != 0x00000000)
	{
	    if(time_out_cnt == 0)
	    {
		InfoPrintf("RTD3_SLI: Timeout polling on clock not enabled after 1000 loops. \n");

		ErrPrintf("fgc6plus_clockgating: Timeout polling on clock not enabled after 1000 loops. \n");
		err++; 
		return 1;
	    }
	    time_out_cnt--;
	    Platform::Delay(10);  
	    EscapeRead("sys_fgc6_clk_gate", 0, 9, &cg_status);
	    cg_status &= FGC6_CG_STATUS; 

	    DebugPrintf("RTD3_SLI: Polling on clock enabled loop %d.\n",(1000-time_out_cnt));
	}


    }
    else{
	ErrPrintf("RTD3_SLI:rtd3_sli_Clockgating: this function can only enable or disable Clock gating");
	return (1);
    }
    return (0);

}
UINT32 GC6plus_Maxwell::rtd3_sli_entry(GpuSubdevice *pSubdev){
    unique_ptr<IRegisterClass> reg;
    unique_ptr<IRegisterField> field;
    unique_ptr<IRegisterValue> value;
    UINT32 temp_val;

    //latch enable
    InfoPrintf("RTD3_SLI : Setting SCI2LWLINK latch enable to 1 \n");
    Find_Register(m_regMap, reg, "LW_PGC6_SCI_FGC6_LWLINK");
    Find_Field(reg, field, "LW_PGC6_SCI_FGC6_LWLINK_LATCH");
    value = field->FindValue("LW_PGC6_SCI_FGC6_LWLINK_LATCH_ENABLE");
    temp_val = value->GetValue();
    EscapeWrite("sci_fgc6_lwlink_latch_escape", 0, 1, temp_val);
    Platform::Delay(1);

    //clamp enable
    InfoPrintf("RTD3_SLI : Setting SCI2LWLINK clamp enable to 1\n");
    Find_Register(m_regMap, reg, "LW_PGC6_SCI_FGC6_LWLINK");
    Find_Field(reg, field, "LW_PGC6_SCI_FGC6_LWLINK_CLAMP");
    value = field->FindValue("LW_PGC6_SCI_FGC6_LWLINK_CLAMP_ENABLE");
    temp_val = value->GetValue();
    EscapeWrite("sci_fgc6_lwlink_clamp_escape", 0, 1, temp_val);
    Platform::Delay(1);
    return(0);

}

UINT32 GC6plus_Maxwell::rtd3_sli_exit(GpuSubdevice *pSubdev){
    unique_ptr<IRegisterClass> reg;
    unique_ptr<IRegisterField> field;
    unique_ptr<IRegisterValue> value;
    UINT32 temp_val;
       //clamp enable
    InfoPrintf("RTD3_SLI : Setting SCI2LWLINK clamp enable to 0\n");
    Find_Register(m_regMap, reg, "LW_PGC6_SCI_FGC6_LWLINK");
    Find_Field(reg, field, "LW_PGC6_SCI_FGC6_LWLINK_CLAMP");
    value = field->FindValue("LW_PGC6_SCI_FGC6_LWLINK_CLAMP_DISABLE");
    temp_val = value->GetValue();
    EscapeWrite("sci_fgc6_lwlink_clamp_escape", 0, 1, temp_val);
    Platform::Delay(1);

    //latch enable
    InfoPrintf("RTD3_SLI : Setting SCI2LWLINK latch enable to 0 \n");
    Find_Register(m_regMap, reg, "LW_PGC6_SCI_FGC6_LWLINK");
    Find_Field(reg, field, "LW_PGC6_SCI_FGC6_LWLINK_LATCH");
    value = field->FindValue("LW_PGC6_SCI_FGC6_LWLINK_LATCH_DISABLE");
    temp_val = value->GetValue();
    EscapeWrite("sci_fgc6_lwlink_latch_escape", 0, 1, temp_val);
    Platform::Delay(1);
 
 return(0);

}

int GC6plus_Maxwell::fgc6Mode(GpuSubdevice *pSubdev){
    InfoPrintf("fgc6Mode(): Entering FGC6\n");
    UINT32 ltssm_state_is_disabled = 0;
    UINT32 time_out_cnt = 1000;
    unique_ptr<IRegisterClass> reg;
    unique_ptr<IRegisterField> field;
    unique_ptr<IRegisterValue> value;
    UINT32 temp_val;
    int err = 0; 

    //Link disable needed by PCIe BFM once GC6 entry has been triggered.
    if( (this->seq == EXIT || this->seq == RESET || this->seq == ABORT) && (Platform::GetSimulationMode() == Platform::RTL) )
    {
        EscapeWrite("gc6plus_pcie_link_escape_hi", 0, 1, 1);
    }
    EscapeRead("xp3g_pl_ltssm_state_is_disabled_escape", 0, 1, &ltssm_state_is_disabled);

    DebugPrintf("gc6Mode(): Polling on link been diabled.\n");
    
    while(ltssm_state_is_disabled == 0)
    {
        
        if(time_out_cnt == 0)
        {
            ErrPrintf("fgc6plus_maxwell: Timeout polling on ltssm_state_is_disabled after 1000 loops. The link is not disabled.\n");
            err++; 
            return 1;
        }
        time_out_cnt--;
        Platform::Delay(1);  
        EscapeRead("xp3g_pl_ltssm_state_is_disabled_escape", 0, 1, &ltssm_state_is_disabled);
        DebugPrintf("gc6plus_fgc6Mode(): Polling on ltssm_state_is_disabled loop %d.\n",(1000-time_out_cnt));
    }

    if(arm_bsi_cya_gc6_mode_check == 1)
    {
        //Trigger GC6 Entry

        Find_Register(m_regMap, reg, "LW_PGC6_BSI_CTRL");
        Find_Field(reg, field, "LW_PGC6_BSI_CTRL_MODE");
        value = field->FindValue("LW_PGC6_BSI_CTRL_MODE_CYA");
        temp_val = value->GetValue();
        EscapeWrite("bsi_ctrl_mode_escape", 0, 2, temp_val);
        
        Find_Register(m_regMap, reg, "LW_PGC6_BSI_CTRL");
        Find_Field(reg, field, "LW_PGC6_BSI_CTRL_CYA");
        value = field->FindValue("LW_PGC6_BSI_CTRL_CYA_GC6");
        temp_val = value->GetValue();
        EscapeWrite("bsi_ctrl_cya_escape", 0, 1, temp_val);
        
        Platform::Delay(1);  
        
        Find_Register(m_regMap, reg, "LW_PGC6_SCI_CNTL");
        Find_Field(reg, field, "LW_PGC6_SCI_CNTL_ENTRY_TRIGGER");
        value = field->FindValue("LW_PGC6_SCI_CNTL_ENTRY_TRIGGER_GC6");
        temp_val = value->GetValue();
        EscapeWrite("sci_cntl_entry_trigger_escape", 0, 2,temp_val);
        
        Find_Register(m_regMap, reg, "LW_PGC6_SCI_CNTL");
        Find_Field(reg, field, "LW_PGC6_SCI_CNTL_SW_STATUS_COPY_CLEAR");
        value = field->FindValue("LW_PGC6_SCI_CNTL_SW_STATUS_COPY_CLEAR_TRIGGER");
        temp_val = value->GetValue();
        EscapeWrite("sci_cntl_sw_status_copy_clear_escape", 0, 1, temp_val);

        InfoPrintf("Triggered FGC6 entry, with BSI armed with CYA mode \n");
    } 
    else 
    {
        //Trigger GC6 Entry

        Find_Register(m_regMap, reg, "LW_PGC6_BSI_CTRL");
        Find_Field(reg, field, "LW_PGC6_BSI_CTRL_MODE");
        value = field->FindValue("LW_PGC6_BSI_CTRL_MODE_GC6");
        temp_val = value->GetValue();
        EscapeWrite("bsi_ctrl_mode_escape", 0, 2, temp_val);
        
        Find_Register(m_regMap, reg, "LW_PGC6_BSI_CTRL");
        Find_Field(reg, field, "LW_PGC6_BSI_CTRL_CYA");
        value = field->FindValue("LW_PGC6_BSI_CTRL_CYA_GC6");
        temp_val = value->GetValue();
        EscapeWrite("bsi_ctrl_cya_escape", 0, 1, temp_val);
        
        Platform::Delay(1);  
        
        Find_Register(m_regMap, reg, "LW_PGC6_SCI_CNTL");
        Find_Field(reg, field, "LW_PGC6_SCI_CNTL_ENTRY_TRIGGER");
        value = field->FindValue("LW_PGC6_SCI_CNTL_ENTRY_TRIGGER_GC6");
        temp_val = value->GetValue();
        EscapeWrite("sci_cntl_entry_trigger_escape", 0, 2, temp_val);
    
        Find_Register(m_regMap, reg, "LW_PGC6_SCI_CNTL");
        Find_Field(reg, field, "LW_PGC6_SCI_CNTL_SW_STATUS_COPY_CLEAR");
        value = field->FindValue("LW_PGC6_SCI_CNTL_SW_STATUS_COPY_CLEAR_TRIGGER");
        temp_val = value->GetValue();
        EscapeWrite("sci_cntl_sw_status_copy_clear_escape", 0, 1, temp_val);

        InfoPrintf("Triggered FGC6 entry\n");
    }

    return (0);
};

UINT32 GC6plus_Maxwell::fgc6_deepl1_setup(GpuSubdevice *pSubdev){

    unique_ptr<IRegisterClass> reg;
    unique_ptr<IRegisterValue> value;
    unique_ptr<IRegisterField> field;
    UINT32 reg_addr;
    UINT32 reg_data;

    int err = 0;
    UINT32 mask;
    UINT32 start_bit;
    UINT32 SET_CLOCK_PM = 0x00000100;  //set CLOCK_PM 1 LINK_CONTROL_STATUS reg
    UINT32 LW_THERM_GATE_CTRL_21 = 0x00020254;  //set CLOCK_PM 1 LINK_CONTROL_STATUS reg
    UINT32 PEX_PLL_XCLK_DLY = 0x00017700;  //set CLOCK_PM 1 LINK_CONTROL_STATUS reg

    InfoPrintf("FGC6 : Call PCIE L1 programmation first!!   \n");
    if(fgc6_l1_setup(pSubdev)) {
        InfoPrintf("FGC6: L1 setup register writes function erroring out");
    }

    InfoPrintf("FGC6 : writing  LW_XP_PRI_XP_CG: _IDLE_CG_EN : _ENABLED and CG_DLY_CNT   \n");
    ReadWriteRegFieldVal(pSubdev, m_regMap,"LW_XP_PRI_XP_CG","_IDLE_CG_DLY_CNT","_HWINIT");
    ReadWriteRegFieldVal(pSubdev, m_regMap,"LW_XP_PRI_XP_CG","_IDLE_CG_EN","_ENABLED");

    InfoPrintf("FGC6 : writing  LW_PCFG_XVE_PRIV_MISC: _CYA_PMCSR_POWER_STATE_D3HOT_IGNORE : _INIT   \n");
    ReadWriteRegFieldVal(pSubdev, m_regMap,"LW_PCFG_XVE_PRIV_MISC","_CYA_PMCSR_POWER_STATE_D3HOT_IGNORE","_INIT");

    InfoPrintf("FGC6 : writing LW_PCFG_XVE1_LINK_CONTROL_STATUS : _ACTIVE_STATE_LINK_PM_CONTROL : _L1_ENABLE_L0S_ENABLE \n");
    ReadWriteRegFieldVal(pSubdev, m_regMap,"LW_PCFG_XVE1_LINK_CONTROL_STATUS","_ACTIVE_STATE_LINK_PM_CONTROL","_L1_ENABLE_L0S_ENABLE");

    InfoPrintf("FGC6 : writing LW_PCFG_XVE_LINK_CONTROL_STATUS : _ACTIVE_STATE_LINK_PM_CONTROL : _L1_ENABLE_L0S_ENABLE \n");
    ReadWriteRegFieldVal(pSubdev, m_regMap,"LW_PCFG_XVE_LINK_CONTROL_STATUS","_ACTIVE_STATE_LINK_PM_CONTROL","_L1_ENABLE_L0S_ENABLE");

    InfoPrintf("FGC6 : writing LW_PCFG_XVE_LINK_CONTROL_STATUS : _CLOCK_PM  : 1 \n");
    Find_Register(m_regMap, reg, "LW_PCFG_XVE_LINK_CONTROL_STATUS");
    reg_addr = reg->GetAddress();
    Fn0CfgRd32(reg_addr, &reg_data);
    reg_data = reg_data | SET_CLOCK_PM;
    Fn0CfgWr32(reg_addr,reg_data);     // setting CLOCK_PM  

    InfoPrintf("FGC6 : writing LW_XVE_XUSB_LINK_CONTROL_STATUS_CLOCK_PM : _CLOCK_PM  : 1 \n");
    Find_Register(m_regMap, reg, "LW_XVE_XUSB_LINK_CONTROL_STATUS");
    reg_addr = reg->GetAddress();
    Fn2CfgRd32(reg_addr, &reg_data);
    reg_data = reg_data | SET_CLOCK_PM;
    Fn2CfgWr32(reg_addr,reg_data);     // setting CLOCK_PM  

    InfoPrintf("FGC6 : writing LW_XVE_PPC_LINK_CONTROL_STATUS_CLOCK_PM : _CLOCK_PM  : 1 \n");
    Find_Register(m_regMap, reg, "LW_XVE_PPC_LINK_CONTROL_STATUS");
    reg_addr = reg->GetAddress();
    Fn3CfgRd32(reg_addr, &reg_data);
    reg_data = reg_data | SET_CLOCK_PM;
    Fn3CfgWr32(reg_addr,reg_data);     // setting CLOCK_PM  

    InfoPrintf("FGC6 : writing  LW_THERM_GATE_CTRL_21 XVE (0x00020254)  : 1 \n");
    reg_addr = LW_THERM_GATE_CTRL_21;
    Fn0CfgRd32(reg_addr, &reg_data);
    reg_data = reg_data & 0xfffffff7;
    Fn0CfgWr32(reg_addr,reg_data);     // setting _BLK_CLK_AUTO [ INIT value for reg]

    InfoPrintf("FGC6 : writing LW_PCFG_XVE_PRIV_XV_0 : _CYA_L0S_ENABLE :_ENABLED  \n");
    ReadWriteRegFieldVal(pSubdev, m_regMap,"LW_PCFG_XVE_PRIV_XV_0","_CYA_L0S_ENABLE","_ENABLED");

    InfoPrintf("FGC6 : writing LW_PTRIM_SYS_IND_SYS_CORE_CLKSRC :_HOSTCLK_AUTO  : _ENABLE \n");
    ReadWriteRegFieldVal(pSubdev, m_regMap,"LW_PTRIM_SYS_IND_SYS_CORE_CLKSRC","_HOSTCLK_AUTO","_ENABLE");

    InfoPrintf("FGC6 : writing LW_PCFG_XVE_PRIV_XV_BLKCG2 :_HOST2XV_HOST_IDLE_WAKE_EN  : _ENABLED \n");
    ReadWriteRegFieldVal(pSubdev, m_regMap,"LW_PCFG_XVE_PRIV_XV_BLKCG2","_HOST2XV_HOST_IDLE_WAKE_EN","_ENABLED");
    InfoPrintf("FGC6 : writing LW_PCFG_XVE_PRIV_XV_BLKCG2 :_UPSTREAM_REQ_WAKE_EN  : _DISABLED \n");
    ReadWriteRegFieldVal(pSubdev, m_regMap,"LW_PCFG_XVE_PRIV_XV_BLKCG2","_UPSTREAM_REQ_WAKE_EN","_DISABLED");

    InfoPrintf("FGC6 : writing LW_XP_PEX_PLL  \n");
    Find_Register(m_regMap, reg, "LW_XP_PEX_PLL");
    reg_addr = reg->GetAddress();
    reg_data = reg_data & 0xff;
    if (!(field = reg->FindField("LW_XP_PEX_PLL_PWRDN_D0_EN"))) {
        ErrPrintf("fgc6_deepl1_setup: field LW_XP_PEX_PLL_PWRDN_D0_EN is not defined!\n");
        return (1);
        err++;
    }
    if (!(value = field->FindValue("LW_XP_PEX_PLL_PWRDN_D0_EN_ENABLED"))) {
        ErrPrintf("fgc6_deepl1_setup: value LW_XP_PEX_PLL_PWRDN_D0_EN_ENABLED is not defined!\n");
        return (1);
        err++;
    }
    mask = field->GetMask();
    start_bit = field->GetStartBit();
    reg_data = (reg_data | mask) & (~mask | ((value->GetValue()  << start_bit)));
    if (!(field = reg->FindField("LW_XP_PEX_PLL_PWRDN_D3_EN"))) {
        ErrPrintf("fgc6_deepl1_setup: field LW_XP_PEX_PLL_PWRDN_D3_EN is not defined!\n");
        return (1);
        err++;
    }
    if (!(value = field->FindValue("LW_XP_PEX_PLL_PWRDN_D3_EN_ENABLED"))) {
        ErrPrintf("fgc6_deepl1_setup: value LW_XP_PEX_PLL_PWRDN_D3_EN_ENABLED is not defined!\n");
        return (1);
        err++;
    }
    mask = field->GetMask();
    start_bit = field->GetStartBit();
    reg_data = (reg_data | mask) & (~mask | ((value->GetValue()  << start_bit)));
    reg_data = reg_data | PEX_PLL_XCLK_DLY;
    pSubdev->RegWr32(reg_addr, reg_data); //   LW_XP_PEX_PLL : 0x00017709 with LW_XP_PEX_PLL_CLK_REQ_EN_DISBLED

    InfoPrintf("FGC6 : writing LW_XP_PL_PAD_PWRDN  \n");
    Find_Register(m_regMap, reg, "LW_XP_PL_PAD_PWRDN");
    reg_addr = reg->GetAddress();
    reg_data = pSubdev->RegRd32(reg_addr);
    reg_data = reg_data & 0xcfffffee;
    if (!(field = reg->FindField("LW_XP_PL_PAD_PWRDN_L1"))) {
        ErrPrintf("fgc6_deepl1_setup: field LW_XP_PL_PAD_PWRDN_L1 is not defined!\n");
        return (1);
        err++;
    }
    if (!(value = field->FindValue("LW_XP_PL_PAD_PWRDN_L1_EN"))) {
        ErrPrintf("fgc6_deepl1_setup: value LW_XP_PL_PAD_PWRDN_L1_EN is not defined!\n");
        return (1);
        err++;
    }
    mask = field->GetMask();
    start_bit = field->GetStartBit();
    reg_data = (reg_data | mask) & (~mask | ((value->GetValue()  << start_bit)));
    if (!(field = reg->FindField("LW_XP_PL_PAD_PWRDN_IDLE_EN_L1_LANE0"))) {
        ErrPrintf("fgc6_deepl1_setup: field LW_XP_PL_PAD_PWRDN_IDLE_EN_L1_LANE0 is not defined!\n");
        return (1);
        err++;
    }
    if (!(value = field->FindValue("LW_XP_PL_PAD_PWRDN_IDLE_EN_L1_LANE0_ENABLED"))) {
        ErrPrintf("fgc6_deepl1_setup: value LW_XP_PL_PAD_PWRDN_IDLE_EN_L1_LANE0_ENABLED is not defined!\n");
        return (1);
        err++;
    }
    mask = field->GetMask();
    start_bit = field->GetStartBit();
    reg_data = (reg_data | mask) & (~mask | ((value->GetValue()  << start_bit)));
    if (!(field = reg->FindField("LW_XP_PL_PAD_PWRDN_SLEEP_MODE_L1"))) {
        ErrPrintf("fgc6_deepl1_setup: field LW_XP_PL_PAD_PWRDN_SLEEP_MODE_L1 is not defined!\n");
        return (1);
        err++;
    }
    if (!(value = field->FindValue("LW_XP_PL_PAD_PWRDN_SLEEP_MODE_L1_L1"))) {
        ErrPrintf("fgc6_deepl1_setup: value LW_XP_PL_PAD_PWRDN_SLEEP_MODE_L1_L1 is not defined!\n");
        return (1);
        err++;
    }
    mask = field->GetMask();
    start_bit = field->GetStartBit();
    reg_data = (reg_data | mask) & (~mask | ((value->GetValue()  << start_bit)));
    pSubdev->RegWr32(reg_addr, reg_data); //   LW_XP_PL_PAD_PWRDN : 0xec000035

    return(0);
};


UINT32 GC6plus_Maxwell::fgc6_l1_setup(GpuSubdevice *pSubdev){

    unique_ptr<IRegisterClass> reg;
    unique_ptr<IRegisterField> field;
    unique_ptr<IRegisterValue> value;
    int err = 0;
    UINT32 mask;
    UINT32 start_bit;
    UINT32 reg_addr;
    UINT32 reg_data;

    //Program ASPM for  XUSB - code from diag/tests/systop_misc_pcie.vx 
    InfoPrintf("FGC6 : program LW_XVE_XUSB_LINK_CONTROL_STATUS : _ACTIVE_STATE_LINK_PM_CONTROL : _L1_ENABLE_L0S_DISABLE \n");
    Find_Register(m_regMap, reg, "LW_XVE_XUSB_LINK_CONTROL_STATUS");
    reg_addr = reg->GetAddress();
    Fn2CfgRd32(reg_addr, &reg_data);
    if (!(field = reg->FindField("LW_XVE_XUSB_LINK_CONTROL_STATUS_ACTIVE_STATE_LINK_PM_CONTROL"))) {
	ErrPrintf("fgc6_l1_setup: field LW_XVE_XUSB_LINK_CONTROL_STATUS_ACTIVE_STATE_LINK_PM_CONTROL is not defined!\n");
	return (1);
	err++;
    }
    if (!(value = field->FindValue("LW_XVE_XUSB_LINK_CONTROL_STATUS_ACTIVE_STATE_LINK_PM_CONTROL_L1_ENABLE_L0S_ENABLE"))) {
	ErrPrintf("fgc6_l1_setup: value LW_XVE_XUSB_LINK_CONTROL_STATUS_ACTIVE_STATE_LINK_PM_CONTROL_L1_ENABLE_L0S_ENABLE is not defined!\n");
	return (1);
	err++;
    }
    mask = field->GetMask();
    start_bit = field->GetStartBit();
    reg_data = (reg_data | mask) & (~mask | ((value->GetValue()  << start_bit)));
    if (!(field = reg->FindField("LW_XVE_XUSB_LINK_CONTROL_STATUS_SLOT_CLOCK_CONFIGURATON"))) {
	ErrPrintf("fgc6_l1_setup: field LW_XVE_XUSB_LINK_CONTROL_STATUS_SLOT_CLOCK_CONFIGURATON is not defined!\n");
	return (1);
	err++;
    }
    if (!(value = field->FindValue("LW_XVE_XUSB_LINK_CONTROL_STATUS_SLOT_CLOCK_CONFIGURATON_INIT"))) {
	ErrPrintf("fgc6_l1_setup: value LW_XVE_XUSB_LINK_CONTROL_STATUS_SLOT_CLOCK_CONFIGURATON_INIT is not defined!\n");
	return (1);
	err++;
    }
    mask = field->GetMask();
    start_bit = field->GetStartBit();
    reg_data = (reg_data | mask) & (~mask | ((value->GetValue()  << start_bit)));
    InfoPrintf("Writing to LW_XVE_XUSB_LINK_CONTROL_STATUS reg with data 0x%x\n" ,reg_data);
    Fn2CfgWr32(reg_addr,reg_data);     // setting LW_XVE_XUSB_LINK_CONTROL  reg_data = 0x10000003;

    //Program ASPM for PPC - code from diag/tests/systop_misc_pcie.vx 
    InfoPrintf("FGC6 : program  LW_XVE_PPC_LINK_CONTROL_STATUS : _ACTIVE_STATE_LINK_PM_CONTROL : _L1_ENABLE_L0S_DISABLE \n");
    Find_Register(m_regMap, reg, "LW_XVE_PPC_LINK_CONTROL_STATUS");
    reg_addr = reg->GetAddress();
    Fn3CfgRd32(reg_addr, &reg_data);
    if (!(field = reg->FindField("LW_XVE_PPC_LINK_CONTROL_STATUS_ACTIVE_STATE_LINK_PM_CONTROL"))) {
	ErrPrintf("fgc6_l1_setup: field LW_XVE_PPC_LINK_CONTROL_STATUS_ACTIVE_STATE_LINK_PM_CONTROL is not defined!\n");
	return (1);
	err++;
    }
    if (!(value = field->FindValue("LW_XVE_PPC_LINK_CONTROL_STATUS_ACTIVE_STATE_LINK_PM_CONTROL_L1_ENABLE_L0S_ENABLE"))) {
	ErrPrintf("fgc6_l1_setup: value LW_XVE_PPC_LINK_CONTROL_STATUS_ACTIVE_STATE_LINK_PM_CONTROL_L1_ENABLE_L0S_ENABLE is not defined!\n");
	return (1);
	err++;
    }
    mask = field->GetMask();
    start_bit = field->GetStartBit();
    reg_data = (reg_data | mask) & (~mask | ((value->GetValue()  << start_bit)));
    if (!(field = reg->FindField("LW_XVE_PPC_LINK_CONTROL_STATUS_SLOT_CLOCK_CONFIGURATON"))) {
	ErrPrintf("fgc6_l1_setup: field LW_XVE_PPC_LINK_CONTROL_STATUS_SLOT_CLOCK_CONFIGURATON is not defined!\n");
	return (1);
	err++;
    }
    if (!(value = field->FindValue("LW_XVE_PPC_LINK_CONTROL_STATUS_SLOT_CLOCK_CONFIGURATON_INIT"))) {
	ErrPrintf("fgc6_l1_setup: value LW_XVE_PPC_LINK_CONTROL_STATUS_SLOT_CLOCK_CONFIGURATON_INIT is not defined!\n");
	return (1);
	err++;
    }
    mask = field->GetMask();
    start_bit = field->GetStartBit();
    reg_data = (reg_data | mask) & (~mask | ((value->GetValue()  << start_bit)));
    InfoPrintf("Writing to LW_XVE_PPC_LINK_CONTROL_STATUS reg with data 0x%x\n" ,reg_data);
    Fn3CfgWr32(reg_addr,reg_data);     // setting LW_XVE_PPC_LINK_CONTROL_STATUS reg_data = 0x10000003;


    //GC6plus_Maxwell::ReadWriteRegFieldVal(GpuSubdevice* pSubdev, IRegisterMap* regmap, string regname, string fieldname, string valuename)
    //ReadWriteRegFieldVal(pSubdev, m_regMap,"","","")
    InfoPrintf("FGC6 : writing LW_PCFG_XVE1_LINK_CONTROL_STATUS : _ACTIVE_STATE_LINK_PM_CONTROL : _L1_ENABLE_L0S_DISABLE \n");
    ReadWriteRegFieldVal(pSubdev, m_regMap,"LW_PCFG_XVE1_LINK_CONTROL_STATUS","_ACTIVE_STATE_LINK_PM_CONTROL","_L1_ENABLE_L0S_DISABLE");

    InfoPrintf("FGC6 : writing LW_PCFG_XVE_LINK_CONTROL_STATUS : _ACTIVE_STATE_LINK_PM_CONTROL : _L1_ENABLE_L0S_DISABLE \n");
    ReadWriteRegFieldVal(pSubdev, m_regMap,"LW_PCFG_XVE_LINK_CONTROL_STATUS","_ACTIVE_STATE_LINK_PM_CONTROL","_L1_ENABLE_L0S_DISABLE");

    InfoPrintf("FGC6 : writing  LW_XP_PRI_XP_CG: _IDLE_CG_EN : _ENABLED \n");
    ReadWriteRegFieldVal(pSubdev, m_regMap,"LW_XP_PRI_XP_CG","_IDLE_CG_EN","_ENABLED");

    InfoPrintf("FGC6 : writing LW_PCFG_XVE_PRI_XVE_CG : _IDLE_CG_EN : _ENABLED \n");
    ReadWriteRegFieldVal(pSubdev, m_regMap,"LW_PCFG_XVE_PRI_XVE_CG","_IDLE_CG_EN","_ENABLED");

    InfoPrintf("FGC6 : writing LW_PCFG_XVE_PRIV_XV_0 : _CYA_L0S_ENABLE :_DISABLED  \n");
    ReadWriteRegFieldVal(pSubdev, m_regMap,"LW_PCFG_XVE_PRIV_XV_0","_CYA_L0S_ENABLE","_DISABLED");

    InfoPrintf("FGC6 : writing LW_PCFG_XVE_PRIV_XV_0 :_CYA_L1_ENABLE  : _ENABLED \n");
    ReadWriteRegFieldVal(pSubdev, m_regMap,"LW_PCFG_XVE_PRIV_XV_0","_CYA_L1_ENABLE","_ENABLED");
    return(0);
};

UINT32 GC6plus_Maxwell::fgc6_entry(GpuSubdevice *pSubdev){

    unique_ptr<IRegisterClass> reg;
    unique_ptr<IRegisterField> field;
    unique_ptr<IRegisterValue> value;
    UINT32 temp_val;

    if(is_fgc6_l2)
    {
        //precredit i/f assert
        this->fgc6_precredit_if(pSubdev, "assert");   
        Platform::Delay(1);
    }

    //SCI clamp, latch
    //Latch enable
    InfoPrintf("FGC6 : writing enable to SCI latch reg\n");
    Find_Register(m_regMap, reg, "LW_PGC6_SCI_FGC6_LWLINK");
    Find_Field(reg, field, "LW_PGC6_SCI_FGC6_LWLINK_LATCH");
    value = field->FindValue("LW_PGC6_SCI_FGC6_LWLINK_LATCH_ENABLE");
    temp_val = value->GetValue();
    EscapeWrite("sci_fgc6_lwlink_latch_escape", 0, 1, temp_val);
    Platform::Delay(1);

    //clamp enable
    InfoPrintf("FGC6 : writing enable to SCI clamp reg\n");
    Find_Register(m_regMap, reg, "LW_PGC6_SCI_FGC6_LWLINK");
    Find_Field(reg, field, "LW_PGC6_SCI_FGC6_LWLINK_CLAMP");
    value = field->FindValue("LW_PGC6_SCI_FGC6_LWLINK_CLAMP_ENABLE");
    temp_val = value->GetValue();
    EscapeWrite("sci_fgc6_lwlink_clamp_escape", 0, 1, temp_val);
    Platform::Delay(1);

    if(!is_fgc6_l2)
    {
        //BSI clamp, latch
        //latch enable
        InfoPrintf("FGC6 : writing enable to BSI latch reg\n");
        Find_Register(m_regMap, reg, "LW_PGC6_BSI_FGC6_PCIE");
        Find_Field(reg, field, "LW_PGC6_BSI_FGC6_PCIE_LATCH");
        value = field->FindValue("LW_PGC6_BSI_FGC6_PCIE_LATCH_ENABLE");
        temp_val = value->GetValue();
        EscapeWrite("bsi_fgc6_pcie_latch_escape", 0, 1, temp_val);
        Platform::Delay(1);

        //clamp enable
        //InfoPrintf("FGC6 : writing enable to BSI clamp reg\n");
        Find_Register(m_regMap, reg, "LW_PGC6_BSI_FGC6_PCIE");
        Find_Field(reg, field, "LW_PGC6_BSI_FGC6_PCIE_CLAMP");
        value = field->FindValue("LW_PGC6_BSI_FGC6_PCIE_CLAMP_ENABLE");
        temp_val = value->GetValue();
        EscapeWrite("bsi_fgc6_pcie_clamp_escape", 0, 1, temp_val);
        Platform::Delay(1);
    }
    return(0);
}


UINT32 GC6plus_Maxwell::fgc6_precredit_if(GpuSubdevice *pSubdev, std::string input){

    unique_ptr<IRegisterClass> reg;
    unique_ptr<IRegisterField> field;
    unique_ptr<IRegisterValue> value;
    UINT32 temp_val;


    if (input == "assert"){
	//precredit i/f assert
	InfoPrintf("FGC6 : writing assert to precredit registers\n");
	Find_Register(m_regMap, reg, "LW_PGC6_BSI_FGC6_PCIE");
	Find_Field(reg, field, "LW_PGC6_BSI_FGC6_PCIE_RST_PRECREDIT_IF");
	Find_Value(field, value, "LW_PGC6_BSI_FGC6_PCIE_RST_PRECREDIT_IF_ASSERT");
	temp_val = value->GetValue();
	EscapeWrite("bsi_fgc6_pcie_rst_precredit_if_escape", 0, 1, temp_val);
    }
    else if(input == "deassert"){
	//precredit i/f deassert
	InfoPrintf("FGC6 : writing deassert to precredit registers\n");
	Find_Register(m_regMap, reg, "LW_PGC6_BSI_FGC6_PCIE");
	Find_Field(reg, field, "LW_PGC6_BSI_FGC6_PCIE_RST_PRECREDIT_IF");
	Find_Value(field, value, "LW_PGC6_BSI_FGC6_PCIE_RST_PRECREDIT_IF_DEASSERT");
	temp_val = value->GetValue();
	EscapeWrite("bsi_fgc6_pcie_rst_precredit_if_escape", 0, 1, temp_val);
    }
    else{
	ErrPrintf("Error: fgc6_precredit_if: this function can only assert or deassert ");
	return (1);
    }

    return (0);
}



UINT32 GC6plus_Maxwell::fgc6_rst_control(GpuSubdevice *pSubdev, std::string input){

    unique_ptr<IRegisterClass> reg;
    unique_ptr<IRegisterField> field;
    unique_ptr<IRegisterValue> value;
    UINT32 temp_val;


    if(this->fgc6_mode == L2) { 

	if (input == "assert"){

	    //reset control assert
	    InfoPrintf("FGC6 : writing assert to reset control reg\n");
	    Find_Register(m_regMap, reg, "LW_PGC6_BSI_FGC6_PCIE");
	    Find_Field(reg, field, "LW_PGC6_BSI_FGC6_PCIE_RESET_CTRL");
	    Find_Value(field, value, "LW_PGC6_BSI_FGC6_PCIE_RESET_CTRL_ASSERT");
	    temp_val = value->GetValue();
	    EscapeWrite("bsi_fgc6_pcie_reset_ctrl_escape", 0, 1, temp_val);
	}
	else if (input == "deassert"){
	    //reset control de-assert
	    InfoPrintf("FGC6 : writing deassert to reset ctrl reg\n");
	    Find_Register(m_regMap, reg, "LW_PGC6_BSI_FGC6_PCIE");
	    Find_Field(reg, field, "LW_PGC6_BSI_FGC6_PCIE_RESET_CTRL");
	    Find_Value(field, value, "LW_PGC6_BSI_FGC6_PCIE_RESET_CTRL_DEASSERT");
	    temp_val = value->GetValue();
	    EscapeWrite("bsi_fgc6_pcie_reset_ctrl_escape", 0, 1, temp_val);
	}
	else{
	    ErrPrintf("Error: fgc6_rst_control: this function can only assert or deassert ");
	    return (1);
	}

    }

    return (0);
}

UINT32 GC6plus_Maxwell::fgc6_exit(GpuSubdevice *pSubdev){
    unique_ptr<IRegisterClass> reg;
    unique_ptr<IRegisterField> field;
    UINT32 temp_val;

    if(!is_fgc6_l2)
    {
        //BSI clamp, latch
        //clamp disable
        InfoPrintf("FGC6 : writing disable to BSI clamp reg\n");
        Find_Register(m_regMap, reg, "LW_PGC6_BSI_FGC6_PCIE");
        Find_Field(reg, field, "LW_PGC6_BSI_FGC6_PCIE_CLAMP");
        temp_val = field->FindValue("LW_PGC6_BSI_FGC6_PCIE_CLAMP_DISABLE")->GetValue();
        EscapeWrite("bsi_fgc6_pcie_clamp_escape", 0, 1, temp_val);
        Platform::Delay(1);

        //latch disable
        InfoPrintf("FGC6 : writing disable to BSI latch reg\n");
        Find_Register(m_regMap, reg, "LW_PGC6_BSI_FGC6_PCIE");
        Find_Field(reg, field, "LW_PGC6_BSI_FGC6_PCIE_LATCH");
        temp_val = field->FindValue("LW_PGC6_BSI_FGC6_PCIE_LATCH_DISABLE")->GetValue();
        EscapeWrite("bsi_fgc6_pcie_latch_escape", 0, 1, temp_val);
        Platform::Delay(1);
    }

    //SCI clamp, latch
    //clamp disable
    InfoPrintf("FGC6 : writing disable to SCI clamp reg\n");
    Find_Register(m_regMap, reg, "LW_PGC6_SCI_FGC6_LWLINK");
    Find_Field(reg, field, "LW_PGC6_SCI_FGC6_LWLINK_CLAMP");
    temp_val = field->FindValue("LW_PGC6_SCI_FGC6_LWLINK_CLAMP_DISABLE")->GetValue();
    EscapeWrite("sci_fgc6_lwlink_clamp_escape", 0, 1, temp_val);	  
    Platform::Delay(1);

    //Latch disable
    InfoPrintf("FGC6 : writing disable to SCI latch reg\n");
    Find_Register(m_regMap, reg, "LW_PGC6_SCI_FGC6_LWLINK");
    Find_Field(reg, field, "LW_PGC6_SCI_FGC6_LWLINK_LATCH");
    temp_val = field->FindValue("LW_PGC6_SCI_FGC6_LWLINK_LATCH_DISABLE")->GetValue();
    EscapeWrite("sci_fgc6_lwlink_latch_escape", 0, 1, temp_val);
    Platform::Delay(1);

    if(is_fgc6_l2)
    {
        //precredit i/f deassert 
        this->fgc6_precredit_if(pSubdev, "deassert");   
        Platform::Delay(1);
    }

    Platform::Delay(1);

    return 1;
}



UINT32 GC6plus_Maxwell::fgc6_wakeupTest(bool additional_wakeup_events,bool timer_wakeup_enabled,
	std::string input, int index,
  int simultaneous_gpio_wakeup) {
    string regname;
    unique_ptr<IRegisterClass> reg;
    UINT32 reg_addr;
    UINT32 reg_data;
    GpuSubdevice *pSubdev = lwgpu->GetGpuSubdevice();
    unique_ptr<IRegisterField> field;
    int extraPassChecks;
    const char * statename;
    UINT32 ltssm_state_is_l1 = 0;
    UINT32 ltssm_state_is_deep_l1 = 0;
    UINT32 time_out_cnt = 1000;
    UINT32 pwr_reset_IB_dly_n = 0;
    int err = 0; 

    InfoPrintf("FGC6 wakeup test function entered\n");

    if (this->fgc6_wakeup_type == HPD_WAKEUP) {
        extraPassChecks = 1;
    }

    if (this->fgc6_wakeup_type == GPU_EVENT) 
    {
        //for GPU_event function
        extraPassChecks = 1;
        //end
        InfoPrintf("fgc6: Entering fgc6_intrGpuEventTest. gpu_event_clash_check = %d. sequence = %d\n\n",gpu_event_clash_check,this->seq);
    }

    //Init functions
    RETURN_IF_ERR(this->gc6plus_init(pSubdev));
    // EscapeWrite("gc6plus_rtd3_sequence_monitor_mode", 0, 8 , 15);
    //EscapeWrite("gc6plus_rtd3_sequence_monitor_mode", 0, 8 , 13);



    if (this->fgc6_wakeup_type == HPD_WAKEUP) 
    {
        InfoPrintf("FGC6: disabling wakeup timer in HPD_WAKEUP condn");
        // ARM wakeup timer
        this->wakeupTimerEvent(pSubdev, "disable");
        //print out statuses
        this->printSwIntrXStatus(pSubdev);

        //Enable Rise Wakeups for GPIOs Enable all the RISE_EN
        Find_Register(m_regMap, reg, "LW_PGC6_SCI_WAKE_RISE_EN");
        reg_addr = reg->GetAddress();
        pSubdev->RegWr32(reg_addr, 0xffffffff); 

        //Show Wake Enables
        this->printWakeEnables(pSubdev);

    } 
    else if (this->fgc6_wakeup_type == WAKEUP_TIMER) 
    {
        // Program any additional wakeup events (Lwrrently only does GPIO Misc wakeup)
        InfoPrintf("FGC6: Setting wakeup timer enable in WAKEUP_TIMER condn");
        DebugPrintf("additional_wakeup_events %d\n", additional_wakeup_events);
        this->program_additional_wakeups(pSubdev, additional_wakeup_events);

        // ARM wakeup timer
        if (timer_wakeup_enabled){
            this->configure_wakeup_timer(pSubdev, wakeup_timer_us);
            this->wakeupTimerEvent(pSubdev, "enable");
        }
        else{
            this->wakeupTimerEvent(pSubdev, "disable");
        }
        this->printWakeEnables(pSubdev);
        this->getSwIntr0Status(pSubdev);
        this->getSwIntr1Status(pSubdev);

    } 
    else if (this->fgc6_wakeup_type == GPU_EVENT) 
    {
        InfoPrintf("FGC6: in GPU_EVENT condn");
        this->program_gpu_event_wake(pSubdev, true);

        // Program any additional wakeup events
        // Lwrrently only does GPIO Misc wakeup 
        this->program_additional_wakeups(pSubdev, simultaneous_gpio_wakeup);

        if(gpu_event_clash_check)
        {
            Find_Register(m_regMap, reg, "LW_PGC6_SCI_TMR_GPU_EVENT_HOLDOFF");
            reg_addr = reg->GetAddress();
            pSubdev->RegWr32(reg_addr, 1);

            Find_Register(m_regMap, reg, "LW_PGC6_SCI_TMR_GPU_EVENT_ASSERT");
            reg_addr = reg->GetAddress();
            pSubdev->RegWr32(reg_addr, 5);

            Find_Register(m_regMap, reg, "LW_PGC6_SCI_TMR_SYSTEM_RESET");
            reg_addr = reg->GetAddress();
            pSubdev->RegWr32(reg_addr, 20);
            this->configure_wakeup_timer(pSubdev, 50);
            this->wakeupTimerEvent(pSubdev, "enable");
        }

        //Print Status and Wakeup Registers
        this->printWakeEnables(pSubdev);
        this->printSwIntrXStatus(pSubdev);
    }




    //IFF and IFR Related code
    InfoPrintf("!!! GC6 enter !!!\n");

    //trigger the clamps and latches for FGC6
    //this->fgc6_wakeup_type = WAKEUP_TIMER;

    //Write registers for L1 mode.
    if(this->fgc6_mode == L1) 
    {

        if(fgc6_l1_setup(pSubdev)) {
            InfoPrintf("FGC6: L1 setup register writes function erroring out");
        }

        //poll for entering L1 state
        EscapeRead("xp3g_pl_ltssm_state_is_l1_escape", 0, 1, &ltssm_state_is_l1);

        DebugPrintf("FGC6: Polling on link L1.\n");

        while(ltssm_state_is_l1 == 0){

            if(time_out_cnt == 0){
                ErrPrintf("FGC6: Timeout polling on ltssm_state_is_l1 after 1000 loops. The link is not in L1.\n");
                err++; 
                return 1;
            }
            time_out_cnt--;
            Platform::Delay(1);  
            EscapeRead("xp3g_pl_ltssm_state_is_l1_escape", 0, 1, &ltssm_state_is_l1);
            DebugPrintf("FGC6: Polling on ltssm_state_is_l1 loop %d.\n",(1000-time_out_cnt));
        }
    } 

    //Write registers for DeepL1 mode.
    if(this->fgc6_mode == Deep_L1) 
    {

        if(fgc6_deepl1_setup(pSubdev)) {
            InfoPrintf("FGC6: Deep-L1 setup register writes function erroring out");
        }

        //poll for entering L1 state
        EscapeRead("xp3g_pl_ltssm_state_is_deep_l1_escape", 0, 1, &ltssm_state_is_deep_l1);

        DebugPrintf("FGC6: Polling on link Deep L1.\n");

        while(ltssm_state_is_deep_l1 == 0){

            if(time_out_cnt == 0){
                ErrPrintf("FGC6: Timeout polling on ltssm_state_is_deep_l1 after 1000 loops. The link is not in L1.\n");
                err++; 
                return 1;
            }
            time_out_cnt--;
            Platform::Delay(1);  
            EscapeRead("xp3g_pl_ltssm_state_is_deep_l1_escape", 0, 1, &ltssm_state_is_deep_l1);
            DebugPrintf("FGC6: Polling on ltssm_state_is_deep_l1 loop %d.\n",(1000-time_out_cnt));
        }


    }   

    //Enter FGC6
    this->gc6plus_triggerMode(pSubdev);

    if (this->fgc6_wakeup_type == WAKEUP_TIMER) {
        InfoPrintf("FGC6: FGC6 triggerred entered WAKEUP_TIMER\n");
        if (additional_wakeup_events) {
            if (~is_fmodel){
                int gpio_delay  = (seq == EXIT)? ((wakeup_timer_us / 4) + 5)  : 1;
                Platform::Delay(gpio_delay);
                assertGpio(pSubdev, "misc", 0);
            }
            // FIXME: FMODEL GPIO abort is not handled here
        }
    } else if (this->fgc6_wakeup_type == HPD_WAKEUP) {
        InfoPrintf("FGC6: FGC6 triggerred entered HPD_WAKEUP\n");
        if(this->seq == EXIT){
            Platform::Delay(10);
        }
    } 


    // Delay until we are out of GCx
    if (this->fgc6_wakeup_type != GPU_EVENT) {
        if (poll_for_fsm_state("STEADY", false)) {
            ErrPrintf("FGC6: Error in polling for FSM state\n");
            m_ErrCount++;
            return (1);
        }

        if (this->fgc6_wakeup_type == HPD_WAKEUP) {
            this->assertGpio(pSubdev, input, index);    
            Platform::Delay(10);
            this->deassertGpio(pSubdev, input, index);
        }

        if (poll_for_fsm_state("STEADY", true)){
            ErrPrintf("FGC6: Error in polling for FSM state\n");
            m_ErrCount++;
            return (1);
        }
    }

    else if (this->fgc6_wakeup_type == GPU_EVENT) {

        switch (this->seq)
        {
          case EXIT :
            InfoPrintf("FGC6: Starting gpio event in GPU_EVENT\n");
            // wait until we reach  GC6 state before asserting reset
            statename = "GC6";
            if (poll_for_fsm_state(statename, true))
            {
                ErrPrintf("fgc6: Error in polling for FSM state\n");
                m_ErrCount++;
            }

            // Now assert GPU_EVENT wake from EC
            InfoPrintf("FGC6: Asserting GPU_EVENT wake from EC\n");
            Platform::EscapeWrite("gc6plus_ec_gpu_event_", 0, 1, 0);
            Platform::EscapeWrite("gc6plus_ec_gpu_event_override", 0, 1, 1);

            // simultaneous wake from GPIO
            if (simultaneous_gpio_wakeup)
            {
                if (this->simultaneous_gpio_wakeup_delay_ns <
                    m_ec_gpu_event_width_us)
                {
                    Platform::Delay(this->simultaneous_gpio_wakeup_delay_ns);
                    assertGpio(pSubdev, "misc", 0);
                    Platform::Delay(m_ec_gpu_event_width_us -
                        this->simultaneous_gpio_wakeup_delay_ns);
                    Platform::EscapeWrite("gc6plus_ec_gpu_event_", 0, 1, 1);
                }
                else
                {
                    Platform::Delay(m_ec_gpu_event_width_us);
                    Platform::EscapeWrite("gc6plus_ec_gpu_event_", 0, 1, 1);
                    Platform::Delay(this->simultaneous_gpio_wakeup_delay_ns -
                        m_ec_gpu_event_width_us);
                    assertGpio(pSubdev, "misc", 0);
                }
            }
            else
            {
                Platform::Delay(m_ec_gpu_event_width_us);
                Platform::EscapeWrite("gc6plus_ec_gpu_event_", 0, 1, 1);
            }

            // Wait for FSM to exit back to STEADY state
            statename = "STEADY";
            if (poll_for_fsm_state(statename, true))
            {
                ErrPrintf("gc6plus_maxwell: Error in polling for FSM state\n");
                m_ErrCount++;
            }
            break;


          default : break;       
        }
    }



    Platform::Delay(500);
    //We expect chip to be out of reset here - sys_pwr_reset_IB_dly_n=1

    InfoPrintf("fgc6_wakeupTest(): Check PWR is out of Reset to exec FGC6 exit sequence.\n");
    EscapeRead("sys_pwr_reset_IB_dly_n", 0, 1, &pwr_reset_IB_dly_n);


    if(pwr_reset_IB_dly_n == 0){
        ErrPrintf("fgc6_wakeupTest FGC6: Chip is not out of Reset!!!.\n");
        err++;
        return 1;
    }


    //disable the fgc6 registers
    this->fgc6_Clockgating(pSubdev, "enable");
    Platform::Delay(1);
    this->fgc6_rst_control(pSubdev, "assert");   //need clocks to be running - applicable if L2
    Platform::Delay(1);
    this->fgc6_exit(pSubdev);	
    Platform::Delay(1);
    this->fgc6_Clockgating(pSubdev, "disable");
    Platform::Delay(1);
    this->fgc6_xclk_source(pSubdev, "exit");   //for selecting xclk source
    Platform::Delay(1);
    this->fgc6_precredit_if(pSubdev, "deassert");   //need clocks to be running
    Platform::Delay(1);
    this->fgc6_rst_control(pSubdev, "deassert");   //need clocks to be running - applicable if L2
    Platform::Delay(1);


    InfoPrintf("FGC6: fgc6_exit function done");
    //Cleanup functions (with priv_init)
    this->gc6plus_cleanup(pSubdev);


    InfoPrintf("FGC6: cleanup done");
    if (this->fgc6_wakeup_type == WAKEUP_TIMER) {
        //Check PRIV is working
        //SCI (LW_PGC6_SCI_PWR_SEQ_ENTRY_VEC3 is bits 31,5:0, resets to 0)
        InfoPrintf("FGC6: entering WAKEUP_TIMER condn after cleanup");
        Find_Register(m_regMap, reg, "LW_PGC6_SCI_PWR_SEQ_ENTRY_VEC3");
        reg_addr = reg->GetAddress();
        InfoPrintf("LW_PGC6_SCI_PWR_SEQ_ENTRY_VEC3 is 0x%x\n" ,pSubdev->RegRd32(reg_addr));
        pSubdev->RegWr32(reg_addr, 0x15);
        InfoPrintf("Writing 0x15 to LW_PGC6_SCI_PWR_SEQ_ENTRY_VEC3\nLW_PGC6_SCI_PWR_SEQ_ENTRY_VEC3 is 0x%x\n" ,ReadReg(pSubdev, m_regMap, "LW_PGC6_SCI_PWR_SEQ_ENTRY_VEC3"));
        //BSI (LW_PGC6_BSI_SCRATCH(0) is bits 31:0 resets to 0)
        InfoPrintf("LW_PGC6_BSI_SCRATCH(0) is 0x%x\n" ,ReadReg(pSubdev, m_regMap, "LW_PGC6_BSI_SCRATCH(0)"));
        Find_Register(m_regMap, reg, "LW_PGC6_BSI_SCRATCH(0)");
        reg_addr = reg->GetAddress();
        pSubdev->RegWr32(reg_addr,0x12345678);
        InfoPrintf("Writing 0x12345678 to LW_PGC6_BSI_SCRATCH(0)\nLW_PGC6_BSI_SCRATCH(0) is 0x%x\n" ,ReadReg(pSubdev, m_regMap, "LW_PGC6_BSI_SCRATCH(0)"));
        //PMU (LW_PPWR_PMU_DEBUG_TAG is bits 31:0 resets to 0)
        Find_Register(m_regMap, reg, "LW_PPWR_PMU_DEBUG_TAG");
        reg_addr = reg->GetAddress();
        UINT32 DEBUG_TAG = pSubdev->RegRd32(reg_addr);
        InfoPrintf("LW_PPWR_PMU_DEBUG_TAG is 0x%x\n" ,DEBUG_TAG);
        pSubdev->RegWr32(reg_addr, 0x90abcdef);
        reg_data = pSubdev->RegRd32(reg_addr);
        InfoPrintf("Writing 0x90abcdef to LW_PPWR_PMU_DEBUG_TAG\nLW_PPWR_PMU_DEBUG_TAG is 0x%x\n" ,reg_data);
        pSubdev->RegWr32(reg_addr, this->debug_tag_data);

        //print out statuses
        this->printSwIntrXStatus(pSubdev);
        this->getState(pSubdev);

        //Check how long we were in gcX for
        this->getResidentTimer(pSubdev);

        InfoPrintf("!!! FGC6 exit WAKEUP_TIMER!!!");



        //Check for:
        return ((
              check_sw_intr_status(pSubdev) //Check Pending Wakeup Interrupts and GC5/GC6 Exit/Abort flags
              && checkSteadyState(pSubdev) //we are now in steady state
              && checkTimers(pSubdev) //Check GC5/GC6 Entry, Resident, and Exit Timers
              && checkDebugTag(pSubdev)
              ) ? 0: 1);

    } else if (this->fgc6_wakeup_type == HPD_WAKEUP) {

        InfoPrintf("FGC6: entering HPD_WAKEUP condn after cleanup");
        //print out statuses
        this->printSwIntrXStatus(pSubdev);

        //Check how long we were in GC5 for
        this->getResidentTimer(pSubdev);

        DebugPrintf("FGC6 Speedy: fgc6_WakeupTest(): fgc6_hpd_test is 0x%x\n",fgc6_hpd_test); 
        InfoPrintf("!!! FGC6 exit HPD_EVENT!!!");
        //Check for:
        return ((
              check_sw_intr_status(pSubdev) //Check Pending Wakeup Interrupts and GC5/GC6 Exit/Abort flags 				//FIXME: How do we pass the expected wakeup?
              && checkSteadyState(pSubdev) //we are now in steady state
              && checkTimers(pSubdev) //Check GC5/GC6 Entry, Resident, and Exit Timers
              && checkDebugTag(pSubdev)
              && extraPassChecks
              ) ? 0: 1);
    } else if (this->fgc6_wakeup_type == GPU_EVENT) {

        InfoPrintf("FGC6: entering GPU_EVENT condn after cleanup");
        switch (this->seq)
        {
          case EXIT:
            //BSI2PMU Copy Priv Write to PMU Debug Tag Register
            Find_Register(m_regMap, reg, "LW_PGC6_SCI_SW_INTR0_STATUS_LWRR");
            if (!(field = reg->FindField("LW_PGC6_SCI_SW_INTR0_STATUS_LWRR_GPU_EVENT")))
            {
                ErrPrintf("%s: Failed to find LW_PGC6_SCI_SW_INTR0_STATUS_LWRR_GPU_EVENT field\n", __FUNCTION__);
            }
            extraPassChecks = this->getSwIntr0Status(pSubdev) &
              field->GetMask()
              && checkDebugTag(pSubdev);
            InfoPrintf("Exit/Abort extraPassChecks: %s\n",
                extraPassChecks ? "pass" : "fail");
            break;

          default : break;       
        }

        //checking sw intr status
        this->printSwIntrXStatus(pSubdev);
        this->getState(pSubdev);

        //Check how long we were in gcX for
        this->getResidentTimer(pSubdev);
        InfoPrintf("!!! FGC6 exit GPU_EVENT !!!");

        //Check for:
        return ((
              check_sw_intr_status(pSubdev) //Check Pending Wakeup Interrupts and GC5/GC6 Exit/Abort flags
              && checkSteadyState(pSubdev) //we are now in steady state
              && checkTimers(pSubdev) //Check GC5/GC6 Entry, Resident, and Exit Timers
              && extraPassChecks // Different checks for Reset vs GPU_Event Exit/Abort
              ) ? 0: 1);

    }

    return(1);
};





int GC6plus_Maxwell::enabledFunctionPme_enable(GpuSubdevice *pSubdev){


    unique_ptr<IRegisterClass> reg;
    UINT32 reg_addr;
    UINT32 reg_data;
    string regname, fieldname, valuename;

    //Read values before SCI PME gets programmed
    Find_Register(m_regMap, reg, "LW_PGC6_SCI_XVE_PME");
    reg_addr = reg->GetAddress();
    reg_data=pSubdev->RegRd32(reg_addr);
    InfoPrintf("%s: Reading %s = 0x%08x\n",__FUNCTION__ , reg->GetName(),reg_data);

    Find_Register(m_regMap, reg, "LW_XVE_PWR_MGMT_1");
    reg_addr = reg->GetAddress();
    Fn0CfgRd32(reg_addr, &reg_data);
    InfoPrintf("%s: Reading %s = 0x%08x\n",__FUNCTION__ , reg->GetName(),reg_data);
    Fn0CfgWr32(reg_addr, (reg_data | 0x100));     // setting LW_XVE_PWR_MGMT_1(bit 8) to 1

    //Check if XVE writes happened as expected 
    Find_Register(m_regMap, reg, "LW_PGC6_SCI_XVE_PME");
    reg_addr = reg->GetAddress();
    reg_data=pSubdev->RegRd32(reg_addr);
    InfoPrintf("%s: Reading after PME en programming %s = 0x%08x\n",__FUNCTION__ , reg->GetName(),reg_data);
    Platform::Delay(1);  
    return 0;
}





int GC6plus_Maxwell::rtd3Mode(GpuSubdevice *pSubdev){
    UINT32 ltssm_state_is_l2 = 0;
    UINT32 time_out_cnt = 1000;
    unique_ptr<IRegisterClass> reg;
    unique_ptr<IRegisterField> field;
    UINT32 temp_val;
    int err = 0; 
    UINT32 gc6_bsi_padctrl_padsuspend = 0;
    UINT32 gc6_bsi_padctrl_pexlpmode = 0;
    UINT32 gc6_bsi_padctrl_clkreqen = 0;
    UINT32 gc6_bsi_padctrl_gc5mode = 0;
    UINT32 gc6_bsi_padctrl_alt_ctl_sel = 0;
    UINT32 gc6_bsi_padctrl_wake_core_clmp = 0;
    UINT32 gc6_bsi_padctrl_wake_alt_ctl_sel = 0;
    if(is_rtd3_sli)
    {    
        InfoPrintf("rtd3Mode(): Entering RTD3 SLI sequence \n");
         //this->lwlink_L2_entry();
	//Platform::Delay(1);
        InfoPrintf("rtd3Mode(): Gating LWLINK clocks\n");
	this->rtd3_sli_Clockgating(pSubdev, "enable");   //clock crossings to lwlink gated
	Platform::Delay(1);
        InfoPrintf("rtd3Mode(): Enabling LWLINK clamp and latch\n");
	this->rtd3_sli_entry(pSubdev);   //for asserting sci2lwlink latch/clamp 
	Platform::Delay(3);
	this->rtd3_sli_Clockgating(pSubdev, "disable");   //clock crossings to lwlink gated
    
    
    }
    
   InfoPrintf("rtd3Mode(): Entering RTD3\n");
    
    //Setting PME_en for all functions
    InfoPrintf("rtd3Mode(): Setting PME_En for all functions before entering RTD3\n");
    this->enabledFunctionPme_enable(pSubdev);

    //Link disable needed by PCIe BFM once GC6 entry has been triggered.
    Platform::Delay(2);  //Delay before asserting L2 incase there are pending transactions
    if( (this->seq == EXIT || this->seq == RESET || this->seq == ABORT) && (Platform::GetSimulationMode() == Platform::RTL) ){
        EscapeWrite("rtd3_pcie_force_l2_entry", 0, 1, 1);
        //EscapeWrite("gc6plus_pcie_link_escape_hi", 0, 1, 1);
    }

    //TEJUEscapeRead("xp3g_pl_ltssm_state_is_disabled_escape", 0, 1, &ltssm_state_is_disablet we pool );
    EscapeRead("rtd3_pcie_state_is_l2", 0, 1, &ltssm_state_is_l2);

    DebugPrintf("rtd3Mode(): Polling on link been diabled.\n");

    while(ltssm_state_is_l2 == 0){

        if(time_out_cnt == 0){
            ErrPrintf("gc6plus_maxwell: Timeout polling on ltssm_state_is_l2 after 1000 loops. The link is not in l2.\n");
            err++; 
            return 1;
        }
        time_out_cnt--;
        Platform::Delay(1);  
        //TEJUEscapeRead("xp3g_pl_ltssm_state_is_disabled_escape", 0, 1, &ltssm_state_is_disabled);
        EscapeRead("rtd3_pcie_state_is_l2", 0, 1, &ltssm_state_is_l2);
        DebugPrintf("gc6plus_rtd3Mode(): Polling on ltssm_state_is_l2 loop %d.\n",(1000-time_out_cnt));
    }
     Platform::Delay(5);
     InfoPrintf("rtd3Mode() : Cleaner Fix for putting PCIE in L2  \n"); //Bug 2544459
     Find_Register(m_regMap, reg, "LW_PTRIM_SYS_IND_SYS_CORE_CLK_PEXCLK_CTRL");
     Find_Field(reg, field, "LW_PTRIM_SYS_IND_SYS_CORE_CLK_PEXCLK_CTRL_XCLK_AUTO");
     temp_val = field->FindValue("LW_PTRIM_SYS_IND_SYS_CORE_CLK_PEXCLK_CTRL_XCLK_AUTO_DISABLE")->GetValue();
     EscapeWrite("pexclk_ctrl_xclk_auto", 0, 1, temp_val);
     
     Find_Register(m_regMap, reg, "LW_PTRIM_SYS_IND_SYS_CORE_CLK_PEXCLK_CTRL");
     Find_Field(reg, field, "LW_PTRIM_SYS_IND_SYS_CORE_CLK_PEXCLK_CTRL_XCLK");
     temp_val = field->FindValue("LW_PTRIM_SYS_IND_SYS_CORE_CLK_PEXCLK_CTRL_XCLK_ALT_XCLK")->GetValue();
     EscapeWrite("pexclk_ctrl_xclk", 0, 4, temp_val);
     Platform::Delay(10);  
     
     Find_Register(m_regMap, reg, "LW_XP_PEX_PLL_CTL1");
     Find_Field(reg, field, "LW_XP_PEX_PLL_CTL1_PLL_RSTN");
     temp_val = field->FindValue("LW_XP_PEX_PLL_CTL1_PLL_RSTN_DISABLE")->GetValue();
     EscapeWrite("pexpll_rstn", 0, 1, temp_val);
     Platform::Delay(3);

     Find_Register(m_regMap, reg, "LW_XP_PEX_PLL_CTL1");
     Find_Field(reg, field, "LW_XP_PEX_PLL_CTL1_PLL_PWR_OVRD");
     temp_val = field->FindValue("LW_XP_PEX_PLL_CTL1_PLL_PWR_OVRD_ENABLE")->GetValue();
     EscapeWrite("pexpll_pwr_ovrd", 0, 1, temp_val);
     Platform::Delay(3);

     Find_Register(m_regMap, reg, "LW_XP_PEX_PLL_CTL1");
     Find_Field(reg, field, "LW_XP_PEX_PLL_CTL1_PLL_SLEEP");
     temp_val = field->FindValue("LW_XP_PEX_PLL_CTL1_PLL_SLEEP_ENABLE")->GetValue();
     EscapeWrite("pexpll_sleep", 0, 2, temp_val);
     Platform::Delay(3);

     Find_Register(m_regMap, reg, "LW_XP_PEX_PLL_CTL1");
     Find_Field(reg, field, "LW_XP_PEX_PLL_CTL1_PLL_IDDQ");
     temp_val = field->FindValue("LW_XP_PEX_PLL_CTL1_PLL_IDDQ_ENABLE")->GetValue();
     EscapeWrite("pexpll_iddq", 0, 1, temp_val);
     Platform::Delay(3);


    if (this->fgc6_mode != L2){
        DebugPrintf("writing rtd3_pcie_bfm_reset to 1 \n");
        EscapeWrite("rtd3_pcie_bfm_reset", 0, 1, 1);
    }
    //EscapeWritePrint("rtd3_pcie_bfm_reset", 0, 1, 1);

    // asserting all the FGC6 clamps and latches. 
    if (this->fgc6_mode == L2){
        DebugPrintf("gc6plus_rtd3Mode(): asserting fgc6 clamps and latches.\n");
        this->fgc6_entry(pSubdev);
    }

    if (bsi_padctrl_in_gc6_check) ///bug 200411598 BSI_PAD_CTRL- per pad checks
    {
        InfoPrintf("gc6plus_rtd3Mode(): Checking bsi pads' assertion before entering RTD3\n");

        EscapeRead("pgc6_bsi_padctrl_padsuspend", 0, 1, &gc6_bsi_padctrl_padsuspend);
        EscapeRead("pgc6_bsi_padctrl_pexlpmode", 0, 1, &gc6_bsi_padctrl_pexlpmode);
        EscapeRead("pgc6_bsi_padctrl_clkreqen", 0, 1, &gc6_bsi_padctrl_clkreqen);
        EscapeRead("pgc6_bsi_padctrl_gc5mode", 0, 1, &gc6_bsi_padctrl_gc5mode);
        EscapeRead("pgc6_bsi_padctrl_alt_ctl_sel", 0, 1, &gc6_bsi_padctrl_alt_ctl_sel);
        EscapeRead("pgc6_bsi_padctrl_wake_core_clmp", 0, 1, &gc6_bsi_padctrl_wake_core_clmp);
        EscapeRead("pgc6_bsi_padctrl_wake_ctl_sel", 0, 1, &gc6_bsi_padctrl_wake_alt_ctl_sel);
    
        temp_val= gc6_bsi_padctrl_padsuspend & gc6_bsi_padctrl_pexlpmode & ~gc6_bsi_padctrl_clkreqen & ~gc6_bsi_padctrl_gc5mode & gc6_bsi_padctrl_alt_ctl_sel & gc6_bsi_padctrl_wake_core_clmp & gc6_bsi_padctrl_wake_alt_ctl_sel;
	
        if(!temp_val)
	{
          ErrPrintf("gc6plus_rtd3Mode(): bsi pads not asserted before entering RTD3 !!\n");
          err++;
          return 1;
	}

    }

    //Trigger RTD3 Entry //TEJ
    Find_Register(m_regMap, reg, "LW_PGC6_BSI_CTRL");
    Find_Field(reg, field, "LW_PGC6_BSI_CTRL_MODE");
    temp_val = field->FindValue("LW_PGC6_BSI_CTRL_MODE_GC6")->GetValue();
    EscapeWrite("bsi_ctrl_mode_escape", 0, 2, temp_val);

    Platform::Delay(1);  

    Find_Register(m_regMap, reg, "LW_PGC6_SCI_CNTL");
    Find_Field(reg, field, "LW_PGC6_SCI_CNTL_ENTRY_TRIGGER");
    temp_val = field->FindValue("LW_PGC6_SCI_CNTL_ENTRY_TRIGGER_RTD3")->GetValue();
    EscapeWrite("sci_cntl_entry_trigger_escape", 0, 2, temp_val);

    Find_Register(m_regMap, reg, "LW_PGC6_SCI_CNTL");
    Find_Field(reg, field, "LW_PGC6_SCI_CNTL_SW_STATUS_COPY_CLEAR");
    temp_val = field->FindValue("LW_PGC6_SCI_CNTL_SW_STATUS_COPY_CLEAR_TRIGGER")->GetValue();
    EscapeWrite("sci_cntl_sw_status_copy_clear_escape", 0, 1, temp_val);

    //InfoPrintf("Triggered RTD3 entry\n");
    InfoPrintf("%s: Triggered RTD3 entry\n", __FUNCTION__);

    return (0);
};

//RTD3 setup function to configure HW_INTR registers
UINT32 GC6plus_Maxwell::rtd3_setup(GpuSubdevice *pSubdev){

    unique_ptr<IRegisterClass> reg;
    string regname, fieldname, valuename;
    UINT32 reg_addr;
    UINT32 reg_data;
    
    this->wakeupTimerEvent(pSubdev, "disable");        
    

    InfoPrintf("%s: Configuring LW_PGC6_SCI_WAKE_EN registers\n",__FUNCTION__);
    Find_Register(m_regMap, reg, "LW_PGC6_SCI_WAKE_EN");
    reg_addr = reg->GetAddress();
    pSubdev->RegWr32(reg_addr, 0x00000000);
    reg_data = pSubdev->RegRd32(reg_addr);
    InfoPrintf("%s: Trying to read after writing all 0's 0x0000_0000 to LW_PGC6_SCI_WAKE_EN %s = 0x%08x\n",__FUNCTION__ , reg->GetName(),reg_data);
    reg_data = ModifyFieldVal(reg.get(), reg_data, "LW_PGC6_SCI_WAKE_EN", "_RTD3_CPU_EVENT", "_ENABLE");
    pSubdev->RegWr32(reg_addr, reg_data);
    InfoPrintf("%s: Writing %s = 0x%08x\n",__FUNCTION__ , reg->GetName(),reg_data);

    InfoPrintf("%s: Configuring LW_PGC6_SCI_WAKE_RISE_EN registers\n",__FUNCTION__);
    Find_Register(m_regMap, reg, "LW_PGC6_SCI_WAKE_RISE_EN");
    reg_addr = reg->GetAddress();
    pSubdev->RegWr32(reg_addr, 0x00000000);
    reg_data=pSubdev->RegRd32(reg_addr);
    InfoPrintf("%s: Writing %s = 0x%08x\n",__FUNCTION__ , reg->GetName(),reg_data);

    InfoPrintf("%s: Configuring LW_PGC6_SCI_WAKE_FALL_EN registers\n",__FUNCTION__);
    Find_Register(m_regMap, reg, "LW_PGC6_SCI_WAKE_FALL_EN");
    reg_addr = reg->GetAddress();
    pSubdev->RegWr32(reg_addr, 0x00000000);
    reg_data=pSubdev->RegRd32(reg_addr);
    InfoPrintf("%s: Writing %s = 0x%08x\n",__FUNCTION__ , reg->GetName(),reg_data);

    InfoPrintf("%s: Configuring LW_PGC6_SCI_HW_INTR_EN registers\n",__FUNCTION__);
    Find_Register(m_regMap, reg, "LW_PGC6_SCI_HW_INTR_EN");
    reg_addr = reg->GetAddress();
    pSubdev->RegWr32(reg_addr, 0xFFFFFFF7);
    reg_data = pSubdev->RegRd32(reg_addr);
    InfoPrintf("%s: Trying to read after writing all 1's 0xFFFF_FFFF to LW_PGC6_SCI_HW_INTR_EN %s = 0x%08x\n",__FUNCTION__ , reg->GetName(),reg_data);
    reg_data = ModifyFieldVal(reg.get(), reg_data, "LW_PGC6_SCI_HW_INTR_EN", "_RTD3_CPU_EVENT", "_DISABLE");
    pSubdev->RegWr32(reg_addr, reg_data);
    InfoPrintf("%s: Writing %s = 0x%08x\n",__FUNCTION__ , reg->GetName(),reg_data);


    InfoPrintf("%s: Configuring LW_PGC6_SCI_HW_INTR_RISE_EN registers\n",__FUNCTION__);
    Find_Register(m_regMap, reg, "LW_PGC6_SCI_HW_INTR_RISE_EN");
    reg_addr = reg->GetAddress();
    pSubdev->RegWr32(reg_addr, 0xFFFFFFFF);
    reg_data = pSubdev->RegRd32(reg_addr);
    InfoPrintf("%s: Writing %s = 0x%08x\n",__FUNCTION__ , reg->GetName(),reg_data);

    InfoPrintf("%s: Configuring LW_PGC6_SCI_HW_INTR_FALL_EN registers\n",__FUNCTION__);
    Find_Register(m_regMap, reg, "LW_PGC6_SCI_HW_INTR_FALL_EN");
    reg_addr = reg->GetAddress();
    pSubdev->RegWr32(reg_addr, 0xFFFFFFFF);
    reg_data = pSubdev->RegRd32(reg_addr);
    InfoPrintf("%s: Writing %s = 0x%08x\n",__FUNCTION__ , reg->GetName(),reg_data);

    return 0;
}

UINT32 GC6plus_Maxwell::rtd3_wakeupTest(bool additional_wakeup_events,
    bool timer_wakeup_enabled){
    unique_ptr<IRegisterClass> reg;
    string regname;
    UINT32 reg_addr;
    UINT32 reg_data;
    UINT32 pwr_reset_IB_dly_n = 0;
    int err=0;
    //int wakeup_timer_us = 20;
    GpuSubdevice *pSubdev = lwgpu->GetGpuSubdevice();

    if(this->fgc6_mode == L2)
    {
        InfoPrintf("FGC6 : enable seq montior for -> RTD3 FGC6");
        EscapeWrite("gc6plus_rtd3_sequence_monitor_mode", 0, 8 , 2);//rtd3_gpu_wake

        Platform::Delay(1);

        InfoPrintf("FGC6 : enable seq montior for RTD3 -> FGC6");
        EscapeWrite("gc6plus_rtd3_sequence_monitor_mode", 0, 8 , 11);//bsi_clkreq

        Platform::Delay(1);

        InfoPrintf("FGC6 : enable seq montior for RTD3 -> FGC6");
        EscapeWrite("gc6plus_rtd3_sequence_monitor_mode", 0, 8 , 16);//fgc6_l2_gpu_wake
    }


    //Init functions
    RETURN_IF_ERR(this->gc6plus_init(pSubdev));

    //set up HW INTR registers
    this->rtd3_setup(pSubdev);



    //configure wakeup timer
    if (rtd3_wakeup_timer_check){
        if(!ppc_rtd3_wake && !xusb_rtd3_wake)
        {
            this->configure_wakeup_timer(pSubdev, wakeup_timer_us);
        }
    }

    this->printWakeEnables(pSubdev);
    this->getSwIntr0Status(pSubdev);
    this->getSwIntr1Status(pSubdev);

    InfoPrintf("%s: !!! RTD3 enter !!!\n", __FUNCTION__);

    //Enter RTD3
    this->gc6plus_triggerMode(pSubdev);

    // RTD3TODO Delay until we are out of GCx  poll for RTD3_ENTRY
    if(rtd3_wakeup_timer_check || rtd3_cpu_wakeup_check)
    {
        // Delay until we are out of GCx
        if (poll_for_fsm_state("STEADY", false))
        {
            ErrPrintf("gc6plus_maxwell: Error in polling for FSM state\n");
            m_ErrCount++;
            return (1);
        }
        if (poll_for_fsm_state("RTD3", true)){
            ErrPrintf("%s : Error in polling for FSM state\n",__FUNCTION__);
            m_ErrCount++;
        }

        //if ppc_rtd3_wake selected, force a ppc_wake through ppc_ao_pme_status
        if(ppc_rtd3_wake)
        {
            Platform::Delay(20);
            InfoPrintf("gc6plus_maxwell:: forcing ppc event\n");
            EscapeWrite("ppc_ao_pme_status", 0, 1, 1);
        }
        //if xusb_rtd3_wake selected, force a xusb_wake through xusb_ao_pme_status
        if(xusb_rtd3_wake)
        {
            Platform::Delay(20);
            InfoPrintf("gc6plus_maxwell:: forcing xusb event\n");
            EscapeWrite("xusb_ao_pme_status", 0, 1, 1);
        }


    }
    else if (rtd3_hpd_test){
        if (poll_for_fsm_state("RTD3", true)){
            ErrPrintf("%s : Error in polling for FSM state\n",__FUNCTION__);
            m_ErrCount++;
        }
        this->assertGpio(pSubdev, "hpd", hpd_gpio_num);    
        Platform::Delay(5);
        this->deassertGpio(pSubdev, "hpd", hpd_gpio_num);    
    }
    if (poll_for_fsm_state("STEADY", true))
    {
        ErrPrintf("%s: Error in polling for FSM state\n", __FUNCTION__);
        m_ErrCount++;
        return (1);
    }  

    //wait long enough before cleanup so that pex rst is de asserted for the second time
    if(rtd3_system_reset_check){
        Platform::Delay(40);
    }	

    if (is_rtd3_sli) //execute RTD3 SLI exit sequence
    {
        Platform::Delay(6000);
        //We expect chip to be out of reset here - sys_pwr_reset_IB_dly_n=1

        InfoPrintf("RTD3_SLI: Check PWR is out of Reset to exec RTD3 SLI exit sequence.\n");
        EscapeRead("sys_pwr_reset_IB_dly_n", 0, 1, &pwr_reset_IB_dly_n);


        if(pwr_reset_IB_dly_n == 0)
        {
            ErrPrintf("RTD3_SLI: Chip is not out of Reset!!!.\n");
            err++;
            return 1;
        }

        InfoPrintf("RTD3_SLI exit started\n");
        this->rtd3_sli_Clockgating(pSubdev, "enable");   
        Platform::Delay(1);
        InfoPrintf("RTD3_SLI: De-asserting LWLINK clamp and latch\n");
        this->rtd3_sli_exit(pSubdev);   //release lwlink clamp and latch 
        Platform::Delay(1);
        InfoPrintf("RTD3_SLI: Ungating LWLINK clocks\n"); //ungate lwlink clocks
        this->rtd3_sli_Clockgating(pSubdev, "disable");   
        Platform::Delay(1);
    } 

    //disable the fgc6 registers
    if (this->fgc6_mode == L2) {
        this->fgc6_exit(pSubdev);
        InfoPrintf(" %s: fgc6_exit function done\n", __FUNCTION__ );
    }

    //Cleanup functions (with priv_init)
    this->gc6plus_cleanup(pSubdev);

    //Check PRIV is working
    //SCI (LW_PGC6_SCI_PWR_SEQ_ENTRY_VEC3 is bits 31,5:0, resets to 0)
    Find_Register(m_regMap, reg, "LW_PGC6_SCI_PWR_SEQ_ENTRY_VEC3");
    reg_addr = reg->GetAddress();
    //InfoPrintf("LW_PGC6_SCI_PWR_SEQ_ENTRY_VEC3 is 0x%x\n" ,pSubdev->RegRd32(reg_addr));
    InfoPrintf("%s: LW_PGC6_SCI_PWR_SEQ_ENTRY_VEC3 is 0x%x\n", __FUNCTION__,pSubdev->RegRd32(reg_addr));
    pSubdev->RegWr32(reg_addr, 0x15);
    InfoPrintf("%s: Writing 0x15 to LW_PGC6_SCI_PWR_SEQ_ENTRY_VEC3\nLW_PGC6_SCI_PWR_SEQ_ENTRY_VEC3 is 0x%x\n", __FUNCTION__,ReadReg(pSubdev, m_regMap, "LW_PGC6_SCI_PWR_SEQ_ENTRY_VEC3"));
    //BSI (LW_PGC6_BSI_SCRATCH(0) is bits 31:0 resets to 0)
    InfoPrintf("%s: LW_PGC6_BSI_SCRATCH(0) is 0x%x\n",__FUNCTION__,ReadReg(pSubdev, m_regMap, "LW_PGC6_BSI_SCRATCH(0)"));
    Find_Register(m_regMap, reg, "LW_PGC6_BSI_SCRATCH(0)");
    reg_addr = reg->GetAddress();
    pSubdev->RegWr32(reg_addr,0x12345678);
    InfoPrintf("%s: Writing 0x12345678 to LW_PGC6_BSI_SCRATCH(0)\nLW_PGC6_BSI_SCRATCH(0) is 0x%x\n",__FUNCTION__,ReadReg(pSubdev, m_regMap, "LW_PGC6_BSI_SCRATCH(0)"));
    //PMU (LW_PPWR_PMU_DEBUG_TAG is bits 31:0 resets to 0)
    Find_Register(m_regMap, reg, "LW_PPWR_PMU_DEBUG_TAG");
    reg_addr = reg->GetAddress();
    UINT32 DEBUG_TAG = pSubdev->RegRd32(reg_addr);
    InfoPrintf("%s: LW_PPWR_PMU_DEBUG_TAG is 0x%x\n",__FUNCTION__,DEBUG_TAG);
    pSubdev->RegWr32(reg_addr, 0x90abcdef);
    reg_data = pSubdev->RegRd32(reg_addr);
    InfoPrintf("%s: Writing 0x90abcdef to LW_PPWR_PMU_DEBUG_TAG\nLW_PPWR_PMU_DEBUG_TAG is 0x%x\n",__FUNCTION__,reg_data);
    pSubdev->RegWr32(reg_addr, this->debug_tag_data);

    //print out statuses
    this->printSwIntrXStatus(pSubdev);
    this->getState(pSubdev);

    //Check how long we were in gcX for
    this->getResidentTimer(pSubdev);

    InfoPrintf("%s: !!! RTD3 before return !!!\n",__FUNCTION__);

    //Check for:
    return ((
          check_sw_intr_status(pSubdev) //Check Pending Wakeup Interrupts and GC5/GC6 Exit/Abort flags
          && checkSteadyState(pSubdev) //we are now in steady state
          && checkTimers(pSubdev) //Check GC5/GC6 Entry, Resident, and Exit Timers
          && checkDebugTag(pSubdev)
          ) ? 0: 1);
};

void GC6plus_Maxwell::EscapeWritePrint(const char *Path, UINT32 Index, UINT32 Size, UINT32 Value){
    InfoPrintf("Writing Escape Write  \n");
    InfoPrintf("Writing %s at index= %d size= %d value = %d \n",*Path,Index,Size,Value);
    InfoPrintf("Writing %s at index= 0x%x size= 0x%x value = 0x%x \n",*Path,Index,Size,Value);
    EscapeWrite(Path,Index,Size,Value);
}

int GC6plus_Maxwell::enableXusbPpcEvent(GpuSubdevice *pSubdev){
        InfoPrintf("Enabling XUSB_PPC event register in SCI \n");
        ReadWriteRegFieldVal(pSubdev, m_regMap, "LW_PGC6_SCI_WAKE_EN", "_XUSB_PPC_EVENT", "_ENABLE");
        return(0);
}



UINT32 GC6plus_Maxwell::gc6plus_XUSB_PPC_wakeup(bool additional_wakeup_events,
                                           bool timer_wakeup_enabled){
    string regname;
    unique_ptr<IRegisterClass> reg;
    UINT32 reg_addr;
    UINT32 reg_data;

    //int wakeup_timer_us = 20;
    GpuSubdevice *pSubdev = lwgpu->GetGpuSubdevice();

    UINT32 iff_done ;

    InfoPrintf("Inside xusb_ppc_event function \n");
    //Init functions
    RETURN_IF_ERR(this->gc6plus_init(pSubdev));
    
    // Program any additional wakeup events (Lwrrently only does GPIO Misc wakeup)
    DebugPrintf("additional_wakeup_events %d\n", additional_wakeup_events);
    this->program_additional_wakeups(pSubdev, additional_wakeup_events);
    InfoPrintf("enabling xusb ppc event register \n");
    this->enableXusbPpcEvent(pSubdev);

    // ARM wakeup timer
    if (timer_wakeup_enabled){
        this->configure_wakeup_timer(pSubdev, wakeup_timer_us);
        this->wakeupTimerEvent(pSubdev, "enable");
    }
    else{
        this->wakeupTimerEvent(pSubdev, "disable");
    }


    this->printWakeEnables(pSubdev);
    this->getSwIntr0Status(pSubdev);
    this->getSwIntr1Status(pSubdev);
    //Enable xusb ppc event wake up




    //IFF and IFR Related code
    InfoPrintf("!!! GC6 enter !!!\n");
    bool read_check_error = 1;   // to check the iff/ifr at gc6 exit.

    // Corrupting the registers before GC6 entry.
    if(initfromfuse_gc6_check == 1){
        if(IFF_write_check(pSubdev)){
            return 1;
        }
    }

    if(initfromrom_gc6_check == 1){
        if(IFR_write_check(pSubdev)){
            return 1;
        }
    }



    //Enter GC5/GC6
    this->gc6plus_triggerMode(pSubdev);

    if (additional_wakeup_events) {
        if (~is_fmodel){
            int gpio_delay  = (seq == EXIT)? ((wakeup_timer_us / 4) + 5)  : 1;
            Platform::Delay(gpio_delay);
            assertGpio(pSubdev, "misc", 0);
        }
        // FIXME: FMODEL GPIO abort is not handled here
    }
    Platform::Delay(10);
    InfoPrintf("forcing xusb event \n");
    EscapeWrite("gc6plus_xusb_ao_pme_status", 0, 1, 1);

    // Delay until we are out of GCx
    if (poll_for_fsm_state("STEADY", false))
    {
        ErrPrintf("gc6plus_maxwell: Error in polling for FSM state\n");
        m_ErrCount++;
        return (1);
    }
    if (poll_for_fsm_state("STEADY", true))
    {
        ErrPrintf("gc6plus_maxwell: Error in polling for FSM state\n");
        m_ErrCount++;
        return (1);
    }

    //Cleanup functions (with priv_init)
    this->gc6plus_cleanup(pSubdev);

    //Check PRIV is working
    //SCI (LW_PGC6_SCI_PWR_SEQ_ENTRY_VEC3 is bits 31,5:0, resets to 0)
    Find_Register(m_regMap, reg, "LW_PGC6_SCI_PWR_SEQ_ENTRY_VEC3");
    reg_addr = reg->GetAddress();
    InfoPrintf("LW_PGC6_SCI_PWR_SEQ_ENTRY_VEC3 is 0x%x\n" ,pSubdev->RegRd32(reg_addr));
    pSubdev->RegWr32(reg_addr, 0x15);
    InfoPrintf("Writing 0x15 to LW_PGC6_SCI_PWR_SEQ_ENTRY_VEC3\nLW_PGC6_SCI_PWR_SEQ_ENTRY_VEC3 is 0x%x\n" ,ReadReg(pSubdev, m_regMap, "LW_PGC6_SCI_PWR_SEQ_ENTRY_VEC3"));
    //BSI (LW_PGC6_BSI_SCRATCH(0) is bits 31:0 resets to 0)
    InfoPrintf("LW_PGC6_BSI_SCRATCH(0) is 0x%x\n" ,ReadReg(pSubdev, m_regMap, "LW_PGC6_BSI_SCRATCH(0)"));
    Find_Register(m_regMap, reg, "LW_PGC6_BSI_SCRATCH(0)");
    reg_addr = reg->GetAddress();
    pSubdev->RegWr32(reg_addr,0x12345678);
    InfoPrintf("Writing 0x12345678 to LW_PGC6_BSI_SCRATCH(0)\nLW_PGC6_BSI_SCRATCH(0) is 0x%x\n" ,ReadReg(pSubdev, m_regMap, "LW_PGC6_BSI_SCRATCH(0)"));
    //PMU (LW_PPWR_PMU_DEBUG_TAG is bits 31:0 resets to 0)
    Find_Register(m_regMap, reg, "LW_PPWR_PMU_DEBUG_TAG");
    reg_addr = reg->GetAddress();
    UINT32 DEBUG_TAG = pSubdev->RegRd32(reg_addr);
    InfoPrintf("LW_PPWR_PMU_DEBUG_TAG is 0x%x\n" ,DEBUG_TAG);
    pSubdev->RegWr32(reg_addr, 0x90abcdef);
    reg_data = pSubdev->RegRd32(reg_addr);
    InfoPrintf("Writing 0x90abcdef to LW_PPWR_PMU_DEBUG_TAG\nLW_PPWR_PMU_DEBUG_TAG is 0x%x\n" ,reg_data);
    pSubdev->RegWr32(reg_addr, this->debug_tag_data);

    //print out statuses
    this->printSwIntrXStatus(pSubdev);
    this->getState(pSubdev);

    //Check how long we were in gcX for
    this->getResidentTimer(pSubdev);

    //IFF and IFR related code.
    InfoPrintf("!!! GC6 exit !!!");

    
    if(initfromfuse_gc6_check == 1 || initfromrom_gc6_check == 1 ){
        InfoPrintf("initializing GPU after GC6 exit");
        RETURN_IF_ERR(this->gc6plus_init(pSubdev));    // initializing GPU after GC6 exit
        Find_Register(m_regMap, reg, "LW_XVE_BAR0");
        reg_addr = reg->GetAddress();
        lwgpu->CfgWr32(reg_addr,0xb8000000);    // setting the BAR0 registers

        regname = "LW_XVE_DEV_CTRL";
        Find_Register(m_regMap, reg, "LW_XVE_DEV_CTRL");
        reg_addr = reg->GetAddress();
        lwgpu->CfgWr32(reg_addr,0x100107);
    }
    
    // verifying IFF and IFR at GC6 exit
    if(initfromfuse_gc6_check == 1)
    {
        // make sure IFF exelwtion completes ,iff read check has to happen only after the iff exelwtion completes.
        if(lwgpu->GetRegFldNum("LW_PBUS_IFR_STATUS1","_IFF_DONE",&iff_done))
        {
            ErrPrintf("Couldn't get the reg LW_PBUS_IFR_STATUS1.\n");
            return (1);
        } 
        else 
        {
            InfoPrintf("Read 0x%x from LW_PBUS_IFR_STATUS1\n", iff_done);
        }

        while(iff_done == 0x0) 
        {
            ErrPrintf ("IFF is still exelwting\n");
            lwgpu->GetRegFldNum("LW_PBUS_IFR_STATUS1","_IFF_DONE",&iff_done);
            return (1);
        }
        
        InfoPrintf ("IFF exelwtion is over\n");
        if(IFF_read_check(pSubdev)){
            read_check_error = 0;
        }
    }

    if(initfromrom_gc6_check == 1){
        if(IFR_read_check(pSubdev)){
            read_check_error = 0;
        }
    }

    //Check for:
    return ((
               check_sw_intr_status(pSubdev) //Check Pending Wakeup Interrupts and GC5/GC6 Exit/Abort flags
            && checkSteadyState(pSubdev) //we are now in steady state
            && checkTimers(pSubdev) //Check GC5/GC6 Entry, Resident, and Exit Timers
            && checkDebugTag(pSubdev)
            && read_check_error  // check IFF/IFR at gc6 exit.
            ) ? 0: 1);
};





