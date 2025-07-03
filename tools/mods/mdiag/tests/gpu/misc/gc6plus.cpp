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
/*------------------------------------------------------------------------------
Test :    gc6plus.cpp 
Purpose:  Exercise a true GC5/GC6 entry routine
----------------------------------------------------------------------------- */

// To facilitate faster compiles via pre-compiled headers, keep "stdtest.h"
// first.
#include "mdiag/tests/stdtest.h"

#include "gc6plus.h"

#include "core/include/rc.h"
#include "device/interface/pcie.h"
#include "gpio_engine.h"
#include "gpu/pcie/pcieimpl.h"
#include "mdiag/lwgpu.h"
#include "mdiag/tests/test_state_report.h"
#include "mdiag/utils/lwgpu_channel.h"


extern const ParamDecl gc6plus_params[] = {
  SIMPLE_PARAM("-system_reset_mon", "Enable GPIO input for system reset mon"),
  SIMPLE_PARAM("-disable_sdr_mode", "Disable Self-Driven Reset mode"),
  SIMPLE_PARAM("-enab_gpu_event_reset", "Enable GPU_EVENT based system reset detection during GC6"),
  SIMPLE_PARAM("-enab_gc6_abort", "Enable GC6 ABORT"),
  SIMPLE_PARAM("-legacy_sr_update", "SR Update detected in STEADY state only"),
  SIMPLE_PARAM("-sw_wake_timer", "Generate SW_INTR_FRC_WAKE_TIMER exit event"),
  SIMPLE_PARAM("-rand_lwvdd_ok_offset", "Offset LWVDD_OK for the island LDO(s) by a random amount"),
  STRING_PARAM("-test", "Run a gc6plus test. Valid tests: entry_gc5, entry_gc6, steady_io_clobber"),
  UNSIGNED_PARAM("-loops", "Number of loops to run (not supported yet). All settings are the same"),
  UNSIGNED_PARAM("-polling_timeout_us", "Timeout for polling loops (in us, default 5000 us)"),
  UNSIGNED_PARAM("-i2c_snoop_scl_timeout_us", "SCL Low Timeout (in us, default 4000 us)"),
  UNSIGNED_PARAM("-sci_entry_holdoff_us", "Delay to allow for GC5/GC6 ABORT (units in us, GC5 default = 1 us, GC6 default = 10 us)"),
  UNSIGNED_PARAM("-sci_gpu_event_width_us", "On GC6 Entry/Exit, this is how long SCI asserts GPU_EVENT (units in us, default = 20us)"),
  UNSIGNED_PARAM("-ec_reset", "Generate EC Reset after X us"),
  UNSIGNED_PARAM("-ec_reset_width_us", "Width of EC generated reset (in us, default 100 us)"),
  UNSIGNED_PARAM("-wake_ec_gpu_event", "Generate EC GPU_EVENT wake after X us"),
  UNSIGNED_PARAM("-ec_gpu_event_width_us", "Width of EC generated GPU_EVENT (in us, default 100 us)"),
  UNSIGNED_PARAM("-sr_update", "Generate SR Update after X us"),
  UNSIGNED_PARAM("-trigger_delay_us", "Delay before triggering Entry (in us, default 0 us)"),
  UNSIGNED_PARAM("-wake_timer_us", "Wakeup Timer (in us, default 4000 us)"),
  UNSIGNED_PARAM("-wake_gpio_misc_0", "Generate MISC 0 event after X us"),
  UNSIGNED_PARAM("-wake_therm_i2c", "Generate THERM I2C event after X us"),
  UNSIGNED_PARAM("-wake_rxstat", "Generate BSI2SCI RXSTAT event after X us"),
  UNSIGNED_PARAM("-wake_clkreq", "Generate BSI2SCI CLKREQ event after X us"),
  UNSIGNED_PARAM("-i2c_sci_priv_rd_SCRATCH", "Generate SCI I2C PRIV read after X us"),
  UNSIGNED_PARAM("-seed", "Random seed used for any randomization"),
  UNSIGNED_PARAM("-lwvdd_ok_val", "LDO LWVDD_OK value. Applies to both BSI and SCI. Gray Coded"),
  SIGNED_PARAM("-lwvdd_ok_offset", "Offset of BSI LDO, in mV relative to SCI LDO. If rand_lwvdd_ok_offset is used, this parameter is ignored (default=0)"),
  UNSIGNED_PARAM("-therm_i2c_addr", "THERM's device address on SMBus/I2CS i/f (default 0x4f)"),
  PARAM_SUBDECL(lwgpu_single_params),
  LAST_PARAM
};


GC6Plus::GC6Plus(ArgReader *params) :
    LWGpuSingleChannelTest(params)
{
    char tempname[127];
    

    m_mode = STEADY;
    m_tb_mode = DEFAULT;
    m_test_type = TTYPE_STEADY;
    m_test = TEST_NONE;
    enab_system_reset_mon = false;
    disable_sdr_mode = false;
    enab_gpu_event_reset = false;
    enab_gc6_abort = false;
    legacy_sr_update = false;
    enab_sw_wake_timer = false;
    cnf_sci_entry_holdoff = false;
    gen_ec_reset = false;
    gen_wake_ec_gpu_event = false;
    gen_sr_update = false;
    gen_wake_gpio_misc_0 = false;
    gen_wake_therm_i2c = false;
    gen_wake_rxstat = false;
    gen_wake_clkreq = false;
    gen_i2c_sci_priv_rd_SCRATCH = false;
   
   //All "test" parsing is done here. ENUM for types below defined in gc6plus.h
   //m_test - Testname. Must correspond to a defined ENUM (required)
   //m_tb_mode - Testbench config. Must correspond to a defined ENUM (optional)
   //m_test_type - Test class. Lwrrently only entry and steady tests (optional)
   //m_mode - Used for special mode behavior w.r.t gc5/gc6 (optional)
    if (params->ParamPresent("-test"))
    {
        strcpy(tempname,params->ParamStr("-test",NULL));

        if(strcmp(tempname,"entry_gc5") == 0)
        {
            m_test_type = TTYPE_ENTRY;
            m_mode = GC5;
            m_test = TEST_ENTRY_GC5;
        }
        else if(strcmp(tempname,"entry_gc6") == 0)
        {
            m_test_type = TTYPE_ENTRY;
            m_mode = GC6;
            m_test = TEST_ENTRY_GC6;
        }
        else if(strcmp(tempname,"steady_io_clobber") == 0)
        {
            m_tb_mode = GPIO_NOPULL;
            m_test = TEST_STEADY_IO_CLOBBER;
        }
        else
        {
            ErrPrintf("%s-%s: Unable to parse -test. Test will fail\n",
                        __FILE__, __FUNCTION__);
        }
    }
    if (params->ParamPresent("-system_reset_mon"))
    {
        enab_system_reset_mon = true;
    }
    if (params->ParamPresent("-disable_sdr_mode"))
    {
        disable_sdr_mode = true;
    }
    if (params->ParamPresent("-enab_gpu_event_reset"))
    {
        enab_gpu_event_reset = true;
    }
    if (params->ParamPresent("-enab_gc6_abort"))
    {
        enab_gc6_abort = true;
    }
    if (params->ParamPresent("-legacy_sr_update"))
    {
        legacy_sr_update = true;
    }
    if (params->ParamPresent("-sw_wake_timer"))
    {
        enab_sw_wake_timer = true;
    }
    if (params->ParamPresent("-rand_lwvdd_ok_offset"))
    {
        enab_rand_lwvdd_ok_offset = true;
    }
    m_test_loops = params->ParamUnsigned("-loops",1);
    m_seed = params->ParamUnsigned("-seed",0x1fc6);
    m_lwvdd_ok_val = params->ParamUnsigned("-lwvdd_ok_val",0x2);
    m_lwvdd_ok_offset = (UINT32) params->ParamSigned("-lwvdd_ok_offset",0); //cast as unsigned integer for EscapeWrite. Expect leading 1's to stay (used in the testbench), if negative.
    m_polling_timeout_us = params->ParamUnsigned("-polling_timeout_us", 5000);
    i2c_snoop_scl_timeout_us = params->ParamUnsigned(
        "-i2c_snoop_scl_timeout_us", 4000);
    if (params->ParamPresent("-sci_entry_holdoff_us"))
    {
        cnf_sci_entry_holdoff = true;
        sci_entry_holdoff_us = params->ParamUnsigned("-sci_entry_holdoff_us");
    }
    m_sci_gpu_event_width_us = params->ParamUnsigned("-sci_gpu_event_width_us",
        20);
    if (params->ParamPresent("-ec_reset"))
    {
        gen_ec_reset = true;
        ec_reset_delay = params->ParamUnsigned("-ec_reset");
    }
    m_ec_reset_width_us = params->ParamUnsigned("-ec_reset_width_us", 100);
    if (params->ParamPresent("-wake_ec_gpu_event"))
    {
        gen_wake_ec_gpu_event = true;
        wake_ec_gpu_event_delay = params->ParamUnsigned("-wake_ec_gpu_event");
    }
    m_ec_gpu_event_width_us = params->ParamUnsigned("-ec_gpu_event_width_us",
        100);
    if (params->ParamPresent("-sr_update"))
    {
        gen_sr_update = true;
        sr_update_delay = params->ParamUnsigned("-sr_update");
    }
    trigger_delay_us = params->ParamUnsigned("-trigger_delay_us", 0);
    wake_timer_us = params->ParamUnsigned("-wake_timer_us", 4000);
    if (params->ParamPresent("-wake_gpio_misc_0"))
    {
        gen_wake_gpio_misc_0 = true;
        wake_gpio_misc_0_delay = params->ParamUnsigned("-wake_gpio_misc_0");
    }
    if (params->ParamPresent("-wake_therm_i2c"))
    {
        gen_wake_therm_i2c = true;
        wake_therm_i2c_delay = params->ParamUnsigned("-wake_therm_i2c");
    }
    if (params->ParamPresent("-wake_rxstat"))
    {
        gen_wake_rxstat = true;
        wake_rxstat_delay = params->ParamUnsigned("-wake_rxstat");
    }
    if (params->ParamPresent("-wake_clkreq"))
    {
        gen_wake_clkreq = true;
        wake_clkreq_delay = params->ParamUnsigned("-wake_clkreq");
    }
    if (params->ParamPresent("-i2c_sci_priv_rd_SCRATCH"))
    {
        gen_i2c_sci_priv_rd_SCRATCH = true;
        i2c_sci_priv_rd_SCRATCH_delay = params->ParamUnsigned(
            "-i2c_sci_priv_rd_SCRATCH");
    }
    therm_dev_addr = params->ParamUnsigned("-therm_i2c_addr", 0x4f);
}

GC6Plus::~GC6Plus(void)
{
}

STD_TEST_FACTORY(gc6plus, GC6Plus)

int 
GC6Plus::Setup(void)
{

    lwgpu = LWGpuResource::FindFirstResource();

    InfoPrintf("GC6Plus Test Setup\n");

    m_regMap = lwgpu->GetRegisterMap();
    if(!m_regMap)
    {
        ErrPrintf("GC6Plus: gc6plus can only be run on GPU's that "
            "support a register map!\n");
        return (0);
    }

    getStateReport()->init("gc6plus");
    getStateReport()->enable();

    return 1;
}

void
GC6Plus::CleanUp(void)
{
    if (lwgpu) {
        DebugPrintf("GC6Plus::CleanUp(): Releasing GPU.\n");
        lwgpu = 0;
    }
    LWGpuSingleChannelTest::CleanUp();
}

void
GC6Plus::Run(void)
{
    bool ran_test = false;
    UINT32 i = 0;
    InfoPrintf("FLAG : gc6plus starting ...");

    SetStatus(TEST_INCOMPLETE);
    InfoPrintf("%s-%s: Starting gc6plus test\n",__FILE__,__FUNCTION__);
    m_ErrCount = 0;

    //check for invalid test specification
    if(m_test == TEST_NONE)
    {
        ErrPrintf("%s-%s: No test, or invalid test was specified from the commandline. Check traceSet for -test <testname>, and make sure <testname> is supported\n", __FILE__, __FUNCTION__);
        SetStatus(TEST_FAILED);
        getStateReport()->runFailed("Test Launch Failure\n");
    }

    //Allow for multiple loops. Not lwrrently supported, though. 
    //for(i=1;i<=m_test_loops;i++)
    {
        InfoPrintf("%s-%s: Loop %d of %d\n",__FILE__,__FUNCTION__,i, m_test_loops);

        switch(m_test_type)
        {
            case TTYPE_STEADY:
                if(gc6plus_steady_test()) {
                    SetStatus(TEST_FAILED);
                    getStateReport()->runFailed("gc6plus_steady_test() failed\n");
                    return;
                }
                ran_test = true;
                break;
            case TTYPE_ENTRY:
                if(gc6plus_entry_test()) {
                    SetStatus(TEST_FAILED);
                    getStateReport()->runFailed("gc6plus_entry_test() failed\n");
                    return;
                }
                ran_test = true;
                break;
            default:
                break;
        }

    }
    
    //this looks redundant. Keep for now, though. 
    if (!ran_test)
    {
        ErrPrintf("No gc6plus test specified, no test ran\n");
        return;
    }

    InfoPrintf("gc6plus: Test completed with %d miscompares\n", m_ErrCount);
    if (m_ErrCount)
    {
        SetStatus(TEST_FAILED_CRC);
        getStateReport()->crcCheckFailed(
            "Test failed with miscompares, see logfile for messages.\n");
        return;
    }

    SetStatus(TEST_SUCCEEDED);
    getStateReport()->crcCheckPassedGold();
}
 


/******************************************************************************
 gc6plus_entry_test:

This function is a test launcher for all entry tests. Entry tests are tests
that exercise GCx entry/exit utilizing the gc6plus islands. The launcher
switches on the entry test (supplied from the traceSet), performs neccesary
setup, runs the test, cleans up, and checks results. 

Supported Tests:
TEST_ENTRY_GC5
TEST_ENTRY_GC6
******************************************************************************/
int GC6Plus::gc6plus_entry_test()
{

    //1. Print function info 
    InfoPrintf("gc6plus: Running gc6plus_entry_test\n");

    // Save PCIE config for later restore (needed for reset tests)
    RETURN_IF_ERR(save_pcie());

    //2. Set up and launch test
    switch(m_test)
    {
        //TEST_ENTRY_GC5 Test
        case TEST_ENTRY_GC5:
            RETURN_IF_ERR(cfg_fullchip_tb());
            RETURN_IF_ERR(InitPrivRing());
            RETURN_IF_ERR(cfg_therm());
            RETURN_IF_ERR(cfg_pmgr());
            RETURN_IF_ERR(cfg_bsi());
            RETURN_IF_ERR(cfg_sci());

            // Prepare for entry
            RETURN_IF_ERR(prepare_for_entry());

            // Trigger entry and generate external events
            RETURN_IF_ERR(generate_events());
            break;
        //TEST_ENTRY_GC6 Test
        case TEST_ENTRY_GC6:
            RETURN_IF_ERR(cfg_fullchip_tb());
            RETURN_IF_ERR(InitPrivRing());
            RETURN_IF_ERR(cfg_therm());
            RETURN_IF_ERR(cfg_pmgr());
            RETURN_IF_ERR(cfg_bsi());
            RETURN_IF_ERR(cfg_sci());

            // Prepare for entry
            RETURN_IF_ERR(prepare_for_entry());

            // Trigger entry and generate external events
            RETURN_IF_ERR(generate_events());
            break;
        //Error if the test is not supported from the stead_test launcher. 
        default:
            ErrPrintf("%s-%s: Invalid mode, %d. Please provide a valid stead-state" 
                "test in the traceset\n", __FILE__, __FUNCTION__, m_test);
            return (1);
    }

    //3. Check Results and Cleanup

    // Clean up
    RETURN_IF_ERR(cleanup_after_exit());

    // Check test results
    RETURN_IF_ERR(check_exit_results());

    return (0);
}

/******************************************************************************
 gc6plus_steady_test:

This function is a test launcher for all entry tests. Steady tests are tests
that exercize island functionality without actually triggering an entry.
The launcher switches on the entry test (supplied from the traceSet), performs 
neccesary setup, runs the test, cleans up, and checks results. 

Supported Tests:
TEST_STEADY_IO_CLOBBER
******************************************************************************/

int GC6Plus::gc6plus_steady_test()
{
    //1. Print function info
    InfoPrintf("gc6plus: Running gc6plus_steady_test\n");


    //2. Set up and launch test
    switch(m_test)
    {
        //GPIO CLOBBER Test
        case TEST_STEADY_IO_CLOBBER:
            {
                int loop_delay_us = 10;

                // Configuration
                RETURN_IF_ERR(cfg_fullchip_tb());
                RETURN_IF_ERR(InitPrivRing());
                RETURN_IF_ERR(run_io_clobber_test(loop_delay_us));
                
                // Wait until we return to STEADY state, then perform cleanup
                // FIXME: we really should wait for PEX to be up and running
                if (poll_for_fsm_state("STEADY", true, m_polling_timeout_us))
                {
                    ErrPrintf("gc6plus: Error in polling for FSM state\n");
                    m_ErrCount++;
                }
            }
            break;
        //Error if the test is not supported from the stead_test launcher. 
        default:
            ErrPrintf("%s-%s: Invalid mode, %d. Please provide a valid stead-state" 
                "test in the traceset\n", __FILE__, __FUNCTION__, m_test);
            return (1);
    }

    //3. Check Results and Cleanup
    //TODO: What to check?
    //TODO: What to cleanup for steady-state tests?

    return (0);
}




// This routine polls on the SCI Master FSM state using EscapeReads.
// It can poll until the state is equal or !equal to the desired value.
int
GC6Plus::poll_for_fsm_state(const char *state, bool equal, UINT32 timeout)
{
    unique_ptr<IRegisterClass> reg;
    unique_ptr<IRegisterField> field;
    unique_ptr<IRegisterValue> value;
    char tempname[127];
    UINT32 state_value, fsm_state, polling_attempts;
    bool done;

    // get value of state from LW_PGC6_SCI_DEBUG_MASTER_FSM_STATE
    strcpy(tempname, "LW_PGC6_SCI_DEBUG_MASTER_FSM");
    FIND_REGISTER(m_regMap, reg, tempname);
    strcat(tempname, "_STATE");
    FIND_FIELD(reg, field, tempname);
    strcat(tempname, "_");
    strcat(tempname, state);
    FIND_VALUE(field, value, tempname);
    state_value = value->GetValue();

    InfoPrintf("gc6plus: Polling sci_debug_fsm_state to be %0s to %s "
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
    while (!done && (polling_attempts < timeout))
    {
        if ((polling_attempts % 10) == 1)
        {
            InfoPrintf("gc6plus: Polling attempt #%d: sci_debug_fsm_state = "
                "0x%x\n", polling_attempts, fsm_state);
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

    InfoPrintf("gc6plus: Polling done after attempt #%d: sci_debug_fsm_state = "
        "0x%x\n", polling_attempts, fsm_state);
    if (!done)
    {
        InfoPrintf("gc6plus: Timeout polling sci_debug_fsm_state to be "
            "%0s to %s after %d attempts\n", equal ? "equal" : "not equal",
            state, polling_attempts);
        return (1);
    }

    InfoPrintf("gc6plus: Done polling for sci_debug_fsm_state\n");
    return (0);
}


// This routine polls on the Escape variable using EscapeRead.
// It can poll until the value is equal or !equal to the desired value.
int
GC6Plus::poll_escape(const char *escapename, UINT32 match_value, bool equal,
    UINT32 timeout)
{
    UINT32 value, polling_attempts;
    bool done;

    InfoPrintf("gc6plus: Polling %s to be %0s to 0x%x\n",
        escapename, equal ? "equal" : "not equal", match_value);

    // We limit the number of polls so we do not hang the test.
    polling_attempts = 1;
    Platform::EscapeRead(escapename, 0, 4, &value);
    if (equal)
    {
        done = (value == match_value);
    }
    else
    {
        done = (value != match_value);
    }
    while (!done && (polling_attempts < timeout))
    {
        if ((polling_attempts % 10) == 1)
        {
            InfoPrintf("gc6plus: Polling attempt #%d: %s = 0x%x\n",
                polling_attempts, escapename, value);
        }
        Platform::Delay(1);  // delay 1 microsecond
        polling_attempts = polling_attempts + 1;
        Platform::EscapeRead(escapename, 0, 4, &value);
        if (equal)
        {
            done = (value == match_value);
        }
        else
        {
            done = (value != match_value);
        }
    }

    InfoPrintf("gc6plus: Polling done after attempt #%d: %s = 0x%x\n",
        polling_attempts, escapename, value);
    if (!done)
    {
        InfoPrintf("gc6plus: Timeout polling %s to be %0s to 0x%x "
            "after %d attempts\n", escapename, equal ? "equal" : "not equal",
            match_value, polling_attempts);
        return (1);
    }

    InfoPrintf("gc6plus: Done polling for %s\n", escapename);
    return (0);
}


// This routine polls the specified register field through PRIV read.
// It can poll until the value is equal or !equal to the desired value.
int
GC6Plus::poll_reg_field(const char *regname,
                        const char *fieldname,
                        const char *valuename,
                        bool equal,
                        UINT32 timeout)
{
    unique_ptr<IRegisterClass> reg;
    unique_ptr<IRegisterField> field;
    unique_ptr<IRegisterValue> value;
    char tempname[127];
    UINT32 reg_addr, reg_data, act_value, match_value, polling_attempts;
    bool done;

    // get the register field and value to poll for
    FIND_REGISTER(m_regMap, reg, regname);
    reg_addr = reg->GetAddress();
    strcpy(tempname, regname);
    strcat(tempname, "_");
    strcat(tempname, fieldname);
    FIND_FIELD(reg, field, tempname);
    strcat(tempname, "_");
    strcat(tempname, valuename);
    FIND_VALUE(field, value, tempname);
    match_value = value->GetValue();

    InfoPrintf("gc6plus: Polling %s field to be %0s to %s (0x%x)\n",
        field->GetName(), equal ? "equal" : "not equal", valuename,
        match_value);

    // We limit the number of polls so we do not hang the test.
    polling_attempts = 1;
    reg_data = lwgpu->RegRd32(reg_addr);
    act_value = (reg_data & field->GetMask()) >> field->GetStartBit();
    if (equal)
    {
        done = (act_value == match_value);
    }
    else
    {
        done = (act_value != match_value);
    }
    while (!done && (polling_attempts < timeout))
    {
        if ((polling_attempts % 10) == 1)
        {
            InfoPrintf("gc6plus: Polling attempt #%d: %s = 0x%x\n",
                polling_attempts, field->GetName(), act_value);
        }
        Platform::Delay(1);  // delay 1 microsecond
        polling_attempts = polling_attempts + 1;
        reg_data = lwgpu->RegRd32(reg_addr);
        act_value = (reg_data & field->GetMask()) >> field->GetStartBit();
        if (equal)
        {
            done = (act_value == match_value);
        }
        else
        {
            done = (act_value != match_value);
        }
    }

    InfoPrintf("gc6plus: Polling done after attempt #%d: %s = 0x%x\n",
        polling_attempts, field->GetName(), act_value);
    if (!done)
    {
        InfoPrintf("gc6plus: Timeout polling %s field to be %0s to %s "
            "after %d attempts\n", field->GetName(), equal ? "equal" :
            "not equal", valuename, polling_attempts);
        return (1);
    }

    InfoPrintf("gc6plus: Done polling %s\n", field->GetName());
    return (0);
}


// This routine sets the PMU command request.  This is used to request the PMU
// to perform some function, typically GC5 or GC6 entry.
int
GC6Plus::set_pmu_command(UINT32 command)
{
    unique_ptr<IRegisterClass> reg;
    UINT32 reg_addr, reg_data;

    InfoPrintf("gc6plus: Setting PMU Ucode command to 0x%02x\n", command);

    // Update using read/modify/write
    FIND_REGISTER(m_regMap, reg, PMU_COMMAND_REG);
    reg_addr = reg->GetAddress();
    reg_data = lwgpu->RegRd32(reg_addr);
    InfoPrintf("gc6plus: Reading %s = 0x%08x\n", reg->GetName(), reg_data);

    // the command is hardcoded to the upper 8-bits in the register
    reg_data = (reg_data & 0x00ffffff) | ((command & 0xff) << 24);

    // write it back
    lwgpu->RegWr32(reg_addr, reg_data);
    InfoPrintf("gc6plus: Writing %s = 0x%08x\n", reg->GetName(), reg_data);

    return (0);
}

// This routine polls the register containing the PMU command request that
// was requested by this test.  The command is in the upper 8-bits of the
// register.  This is used to indicate when the PMU has completed the command
// and the test can check the results or proceed with another PMU command
// request.
int
GC6Plus::poll_pmu_command(UINT32 command, bool equal, UINT32 timeout)
{
    unique_ptr<IRegisterClass> reg;
    UINT32 reg_addr, reg_data, act_value, polling_attempts;
    bool done;

    InfoPrintf("gc6plus: Polling PMU Ucode command to be %0s to 0x%02x\n",
        equal ? "equal" : "not equal", command);

    // get the register to poll
    // We limit the number of polls so we do not hang the test.
    polling_attempts = 1;
    FIND_REGISTER(m_regMap, reg, PMU_COMMAND_REG);
    reg_addr = reg->GetAddress();
    reg_data = lwgpu->RegRd32(reg_addr);
    // the command is hardcoded to the upper 8-bits in the register
    act_value = (reg_data & 0xff000000) >> 24;
    if (equal)
    {
        done = (act_value == command);
    }
    else
    {
        done = (act_value != command);
    }
    while (!done && (polling_attempts < timeout))
    {
        if ((polling_attempts % 10) == 1)
        {
            InfoPrintf("gc6plus: Polling attempt #%d: %s[31:24] = 0x%02x\n",
                polling_attempts, reg->GetName(), act_value);
        }
        Platform::Delay(1);  // delay 1 microsecond
        polling_attempts = polling_attempts + 1;
        reg_data = lwgpu->RegRd32(reg_addr);
        act_value = (reg_data & 0xff000000) >> 24;
        if (equal)
        {
            done = (act_value == command);
        }
        else
        {
            done = (act_value != command);
        }
    }

    InfoPrintf("gc6plus: Polling done after attempt #%d: %s[31:24] = 0x%02x\n",
        polling_attempts, reg->GetName(), act_value);
    if (!done)
    {
        InfoPrintf("gc6plus: Timeout polling PMU Ucode command to be %0s to "
            "0x%02x after %d attempts\n", equal ? "equal" : "not equal",
            command, polling_attempts);
        return (1);
    }

    InfoPrintf("gc6plus: Done polling PMU Ucode command\n");
    return (0);
}

// This routine transfers the PMU ucode error count to the test's error
// count and then clears it.  PMU ucode increments the register whenever it
// encounters unexpected behavior (invalid entry condition, timeouts, etc.).
int
GC6Plus::check_pmu_errors()
{
    unique_ptr<IRegisterClass> reg;
    UINT32 reg_addr, reg_data;

    // copy over PMU ucode error count and clear it
    InfoPrintf("gc6plus: Checking PMU error count\n");
    FIND_REGISTER(m_regMap, reg, ERR_COUNT_REG);
    reg_addr = reg->GetAddress();
    reg_data = lwgpu->RegRd32(reg_addr);
    InfoPrintf("gc6plus: Reading %s = 0x%08x\n", reg->GetName(), reg_data);
    if (reg_data)
    {
        InfoPrintf("gc6plus: Found %d PMU errors\n", reg_data);
        m_ErrCount += reg_data;
        reg_data = 0;
        lwgpu->RegWr32(reg_addr, reg_data);
        InfoPrintf("gc6plus: Writing %s = 0x%08x\n", reg->GetName(), reg_data);
    }

    return (0);
}

int
GC6Plus::Reg_RMW(const char *regname, const char *fieldname, const char *valuename)
{
    unique_ptr<IRegisterClass> reg;
    unique_ptr<IRegisterField> field;
    unique_ptr<IRegisterValue> value;
    char tempname[127];
    UINT32 reg_addr;
    UINT32 reg_data;

    // Find the register and read it
    FIND_REGISTER(m_regMap, reg, regname);
    reg_addr = reg->GetAddress();
    reg_data = lwgpu->RegRd32(reg_addr);
    InfoPrintf("gc6plus: Reading %s = 0x%08x\n", reg->GetName(), reg_data);

    // Find the field and ENUM value
    strcpy(tempname, reg->GetName());
    strcat(tempname, "_");
    strcat(tempname, fieldname);
    FIND_FIELD(reg, field, tempname);
    strcat(tempname, "_");
    strcat(tempname, valuename);
    FIND_VALUE(field, value, tempname);

    // Modify the read data
    reg_data = (reg_data & ~field->GetMask()) |
        (value->GetValue() << field->GetStartBit());

    // Write it back to the register
    lwgpu->RegWr32(reg_addr, reg_data);
    InfoPrintf("gc6plus: Writing %s = 0x%08x\n", reg->GetName(), reg_data);

    return (0);
}

int
GC6Plus::Reg_RMW(const char *regname, const char *fieldname, UINT32 value)
{
    unique_ptr<IRegisterClass> reg;
    unique_ptr<IRegisterField> field;
    char tempname[127];
    UINT32 reg_addr, reg_data;

    // Find the register and read it
    FIND_REGISTER(m_regMap, reg, regname);
    reg_addr = reg->GetAddress();
    reg_data = lwgpu->RegRd32(reg_addr);
    InfoPrintf("gc6plus: Reading %s = 0x%08x\n", reg->GetName(), reg_data);

    // Find the field and ENUM value
    strcpy(tempname, reg->GetName());
    strcat(tempname, "_");
    strcat(tempname, fieldname);
    FIND_FIELD(reg, field, tempname);

    // Modify the read data
    reg_data = (reg_data & ~field->GetMask()) |
        ((value << field->GetStartBit()) & field->GetMask());

    // Write it back to the register
    lwgpu->RegWr32(reg_addr, reg_data);
    InfoPrintf("gc6plus: Writing %s = 0x%08x\n", reg->GetName(), reg_data);

    return (0);
}

//Configure the gpio_num on SCI and PMGR to drive the PWM0 PWM
//Configure other GPIOs to NORMAL mode with outputs disabled. 
//if gpio_drv == GPIO_DRV_OPEN_DRAIN (1), configure for OPEN_DRAIN. 
//if gpio_drv == GPIO_DRV_FULL (3), configure for full drive
//other values of gpio_drv are not lwrrently supported. 
int GC6Plus::cfg_gpio_pwm_1hot(UINT32 gpio_num, UINT32 gpio_drv, std::map<unsigned int, unsigned int> &PastRegWrs)
{

    unique_ptr<IRegisterClass> reg;
    unique_ptr<IRegisterField> field;
    unique_ptr<IRegisterValue> value;
    char regname[127];
    char tempname[138];
    char tempfunc[40];
    UINT32 reg_addr;
    UINT32 reg_data;
    UINT32 target_gpio = 0;
    UINT32 i = 0;
    string regname2, fieldname, valuename;

    InfoPrintf("%s-%s: Configuring PMGR GPIO %d for PWM_OUTPUT\n",
        __FILE__,__FUNCTION__,gpio_num);

    for(i=0;i<m_max_gpios;i++)
    {
        target_gpio = (i == gpio_num);

        if (target_gpio == 1)
        {
            InfoPrintf("gc6plus: Configuring target GPIO = %d\n",i);
        }

        //Set up PMGR 
        regname2 = Utility::StrPrintf("LW_GPIO_OUTPUT_CNTL");
        if (regname2.c_str() < 0)
        {
            ErrPrintf("gc6plus: Failed to generate GPIO config regname\n");
            return (1);
        }

        if (!(reg = m_regMap->FindRegister(regname2.c_str())))
        {
            ErrPrintf("gc6plus: Failed to find %s register\n", regname2.c_str());
            return (1);
        }

        reg_addr = reg->GetAddress(i);
        fieldname =  "LW_GPIO_OUTPUT_CNTL_SEL";
        if (!(field = reg->FindField(fieldname.c_str())))
        {
            ErrPrintf("gc6plus: Failed to find %s field\n", fieldname.c_str());
            return (1);
        }

        valuename = (target_gpio) ? "LW_GPIO_OUTPUT_CNTL_SEL_PWM_OUTPUT" : "LW_GPIO_OUTPUT_CNTL_SEL_NORMAL";

        if (!(value = field->FindValue(valuename.c_str())))
        {
            ErrPrintf("gc6plus: Failed to find %s value\n", valuename.c_str());
            return (1);
        }
        reg_data = value->GetValue() << field->GetStartBit(); 

        fieldname =  "LW_GPIO_OUTPUT_CNTL_IO_OUT_EN";
        if (!(field = reg->FindField(fieldname.c_str())))
        {
            ErrPrintf("gc6plus: Failed to find %s field\n", fieldname.c_str());
            return (1);
        }

        valuename = (target_gpio) ? "LW_GPIO_OUTPUT_CNTL_IO_OUT_EN_YES" : "LW_GPIO_OUTPUT_CNTL_IO_OUT_EN_NO"; 
        if (!(value = field->FindValue(valuename.c_str())))
        {
            ErrPrintf("gc6plus: Failed to find %s value\n", valuename.c_str());
            return (1);
        }
        reg_data |= value->GetValue() << field->GetStartBit(); 

        //If open drain mode, enable the internal weak pull-up (the testbench should not have one)
        if(gpio_drv == GPIO_DRV_OPEN_DRAIN && target_gpio)
        {
            fieldname =  "LW_GPIO_OUTPUT_CNTL_OPEN_DRAIN";
            if (!(field = reg->FindField(fieldname.c_str())))
            {
                ErrPrintf("gc6plus: Failed to find %s field\n", fieldname.c_str());
                return (1);
            }

            InfoPrintf("gc6plus: Setting PMGR GPIO %d Open Drain Mode to Enable \n",i);
            valuename = "LW_GPIO_OUTPUT_CNTL_OPEN_DRAIN_ENABLE"; 
            if (!(value = field->FindValue(valuename.c_str())))
            {
                ErrPrintf("gc6plus: Failed to find %s value\n", valuename.c_str());
                return (1);
            }
            reg_data |= value->GetValue() << field->GetStartBit(); 
            InfoPrintf("gc6plus: Writing %s = 0x%08x\n", reg->GetName(),reg_data);

            fieldname =  "LW_GPIO_OUTPUT_CNTL_PULLUD";
            if (!(field = reg->FindField(fieldname.c_str())))
            {
                ErrPrintf("gc6plus: Failed to find %s field\n", fieldname.c_str());
                return (1);
            }

            InfoPrintf("gc6plus: Setting PMGR GPIO %d PULLUD to UP \n",i);
            valuename = "LW_GPIO_OUTPUT_CNTL_PULLUD_UP"; 
            if (!(value = field->FindValue(valuename.c_str())))
            {
                ErrPrintf("gc6plus: Failed to find %s value\n", valuename.c_str());
                return (1);
            }
            reg_data |= value->GetValue() << field->GetStartBit(); 
            InfoPrintf("gc6plus: Writing %s = 0x%08x\n", reg->GetName(),reg_data);

        }

        //If the register is not previously written or mismatches, write it
        if(PastRegWrs.find(reg_addr) == PastRegWrs.end() || PastRegWrs[reg_addr] != reg_data )
        {
            lwgpu->RegWr32(reg_addr, reg_data);
            InfoPrintf("gc6plus: Writing %s = 0x%08x\n", reg->GetName(), reg_data);
            PastRegWrs[reg_addr] = reg_data;
        } 
        //else, skip write to save sim time
        else
        {
            InfoPrintf("gc6plus: Skip Writing %s = 0x%08x (already written)\n", reg->GetName(), reg_data);
        }


        //set up SCI
        sprintf(tempfunc,"%s",(target_gpio) ? "_SOR_BACKLIGHT" : "_NORMAL");

        InfoPrintf("%s-%s: Configuring SCI GPIO %d Select for %s\n",
            __FILE__,__FUNCTION__,i,tempfunc);
        if (sprintf(regname, "LW_PGC6_SCI_GPIO_OUTPUT_CNTL") < 0)
        {
            ErrPrintf("gc6plus: Failed to generate GPIO config regname\n");
            return (1);
        }
        FIND_REGISTER(m_regMap, reg, regname);
        reg_addr = reg->GetAddress(i);

        sprintf(tempname, "%s_SEL",regname);
        FIND_FIELD(reg, field, tempname);
        strcat(tempname, tempfunc);
        FIND_VALUE(field, value, tempname);
        reg_data = value->GetValue() << field->GetStartBit();

        //GPIO programming is a bit different here than PMGR
        //No ilwidual control over EN/OUm_pPciCfgT
        if(target_gpio)
        {
            if(gpio_drv == GPIO_DRV_OPEN_DRAIN)
                strcpy(tempfunc,"_OPEN_DRAIN");
            else
                strcpy(tempfunc,"_FULL_DRIVE");
        }
        else
            strcpy(tempfunc,"_DISABLE");

        sprintf(tempname, "%s_DRIVE_MODE",regname);
        FIND_FIELD(reg, field, tempname);
        strcat(tempname, tempfunc);
        FIND_VALUE(field, value, tempname);
        reg_data =  (reg_data & ~field->GetMask()) |
          (value->GetValue() << field->GetStartBit());

        //If the register is not previously written or mismatches, write it
        if(PastRegWrs.find(reg_addr) == PastRegWrs.end() || PastRegWrs[reg_addr] != reg_data )
        {
            lwgpu->RegWr32(reg_addr, reg_data);
            InfoPrintf("gc6plus: Writing %s = 0x%08x\n", reg->GetName(), reg_data);
            PastRegWrs[reg_addr] = reg_data;
        } 
        //else, skip write to save sim time
        else
        {
            InfoPrintf("gc6plus: Skip Writing %s = 0x%08x (already written)\n", reg->GetName(), reg_data);
        }

    }

    // Trigger PMGR GPIO update after all PMGR GPIOs are configured
    InfoPrintf("gc6plus: Triggering PMGR GPIO update\n");
    FIND_REGISTER(m_regMap, reg, "LW_PMGR_GPIO_OUTPUT_CNTL_TRIGGER");
    reg_addr = reg->GetAddress();
    FIND_FIELD(reg,   field, "LW_PMGR_GPIO_OUTPUT_CNTL_TRIGGER_UPDATE");
    FIND_VALUE(field, value, "LW_PMGR_GPIO_OUTPUT_CNTL_TRIGGER_UPDATE_TRIGGER");
    reg_data = value->GetValue() << field->GetStartBit();
    lwgpu->RegWr32(reg_addr, reg_data);
    InfoPrintf("gc6plus: Writing %s = 0x%08x\n", reg->GetName(), reg_data);

    //Flush writes to improve test timing. Don't care about the value in reg_data
    FIND_REGISTER(m_regMap, reg, "LW_PGC6_SCI_SCRATCH");
    reg_addr = reg->GetAddress();
    reg_data = lwgpu->RegRd32(reg_addr);

    return 0;
}


//period and hi are in XTAL clocks
//pwm_sel is "disp" for DISP PWM and "sci" for SCI PWM
int GC6Plus::cfg_pwms(UINT32 period, UINT32 hi, const char *pwm_sel)
{

    unique_ptr<IRegisterClass> reg;
    unique_ptr<IRegisterField> field;
    unique_ptr<IRegisterValue> value;
    UINT32 reg_addr, reg_data;

    InfoPrintf("%s-%s: Configuring PWMs\n", __FILE__,__FUNCTION__);

    if(strcmp(pwm_sel,"pmgr") == 0)
    {
        //Configure PMGR PWM0/1
        FIND_REGISTER(m_regMap, reg, "LW_PMGR_PWM0");
        reg_addr = reg->GetAddress(0);
        FIND_FIELD(reg,   field, "LW_PMGR_PWM0_PERIOD");
        reg_data = (((period) << field->GetStartBit()) &
            field->GetMask());
        lwgpu->RegWr32(reg_addr, reg_data);

        FIND_REGISTER(m_regMap, reg, "LW_PMGR_PWM1");
        reg_addr = reg->GetAddress(0);
        FIND_FIELD(reg,   field, "LW_PMGR_PWM1_HI");
        reg_data = (((hi) << field->GetStartBit()) &
            field->GetMask());
        FIND_FIELD(reg,   field, "LW_PMGR_PWM1_NEW_REQUEST");
        FIND_VALUE(field, value, "LW_PMGR_PWM1_NEW_REQUEST_TRIGGER");
        reg_data |= value->GetValue() << field->GetStartBit();
        lwgpu->RegWr32(reg_addr, reg_data);
        InfoPrintf("gc6plus: Writing %s = 0x%08x\n", reg->GetName(), reg_data);
    }

    //Configure SCI SOR PWM
    if(strcmp(pwm_sel,"sci") == 0)
    {
        FIND_REGISTER(m_regMap, reg, "LW_PGC6_SCI_SOR_PWM_DIV");
        reg_addr = reg->GetAddress(0);
        FIND_FIELD(reg,   field, "LW_PGC6_SCI_SOR_PWM_DIV_DIVIDE");
        reg_data = (((period) << field->GetStartBit()) &
            field->GetMask());
        lwgpu->RegWr32(reg_addr, reg_data);

        FIND_REGISTER(m_regMap, reg, "LW_PGC6_SCI_SOR_PWM_CTL");
        reg_addr = reg->GetAddress(0);
        FIND_FIELD(reg,   field, "LW_PGC6_SCI_SOR_PWM_CTL_DUTY_CYCLE");
        reg_data = (((hi) << field->GetStartBit()) &
            field->GetMask());
        FIND_FIELD(reg,   field, "LW_PGC6_SCI_SOR_PWM_CTL_CLKSEL");
        FIND_VALUE(field, value, "LW_PGC6_SCI_SOR_PWM_CTL_CLKSEL_XTAL");
        reg_data |= value->GetValue() << field->GetStartBit();
        FIND_FIELD(reg,   field, "LW_PGC6_SCI_SOR_PWM_CTL_SETTING_NEW");
        FIND_VALUE(field, value, "LW_PGC6_SCI_SOR_PWM_CTL_SETTING_NEW_TRIGGER");
        reg_data |= value->GetValue() << field->GetStartBit();
        lwgpu->RegWr32(reg_addr, reg_data);
        InfoPrintf("gc6plus: Writing %s = 0x%08x\n", reg->GetName(), reg_data);
    }

    return 0;
}

int
GC6Plus::save_pcie()
{
    GpuSubdevice *pSubdev;
    RC rc;

    InfoPrintf("gc6plus: Saving PCIE config space\n");

    pSubdev = lwgpu->GetGpuSubdevice();
    auto pGpuPcie = pSubdev->GetInterface<Pcie>(); 
    m_pPciCfg = pSubdev->AllocPciCfgSpace();
    rc = m_pPciCfg->Initialize(pGpuPcie->DomainNumber(),
                               pGpuPcie->BusNumber(),
                               pGpuPcie->DeviceNumber(),
                               pGpuPcie->FunctionNumber());
    if (rc != OK)
    {
        ErrPrintf("gc6plus: Error in PciCfgSpace.Initialize\n");
        return (1);
    }

    rc = m_pPciCfg->Save();
    if (rc != OK)
    {
        ErrPrintf("gc6plus: Error in PciCfgSpace.Save\n");
        return (1);
    }

    return (0);
}

int
GC6Plus::restore_pcie()
{
    UINT32 timeout_ms;
    RC rc;

    InfoPrintf("gc6plus: Polling for PCIE config space to be ready\n");
    timeout_ms = m_polling_timeout_us / 1000;
    rc = m_pPciCfg->CheckCfgSpaceReady(timeout_ms);
    if (rc != OK)
    {
        ErrPrintf("gc6plus: Error in PciCfgSpace.CheckCfgSpaceReady\n");
        return (1);
    }

    InfoPrintf("gc6plus: Restoring PCIE config space\n");
    rc = m_pPciCfg->Restore();
    if (rc != OK)
    {
        ErrPrintf("gc6plus: Error in PciCfgSpace.Restore\n");
        return (1);
    }

    return (0);
}

int
GC6Plus::InitPrivRing()
{
    InfoPrintf("gc6plus: Initializing PRIV ring\n");
    
    unique_ptr<IRegisterClass> reg;
    unique_ptr<IRegisterField> field;
    unique_ptr<IRegisterValue> value;
    UINT32 reg_addr, reg_data;
    UINT32 connectivity;
    UINT32 arch = lwgpu->GetArchitecture();
    if(arch < 0x400) {
        return (0);
    }

    // What does this do?
    FIND_REGISTER(m_regMap, reg, "LW_PPRIV_MASTER_RING_COMMAND_DATA");
    reg_addr = reg->GetAddress();
    reg_data = 0x503B5443;
    lwgpu->RegWr32(reg_addr, reg_data);
    InfoPrintf("gc6plus: Writing %s = 0x%08x\n", reg->GetName(), reg_data);


    //Disable all engines before PRIV init starts
    FIND_REGISTER(m_regMap, reg, "LW_PMC_ENABLE");
    reg_addr = reg->GetAddress();
    reg_data = 0x0;
    lwgpu->RegWr32(reg_addr, reg_data);
    InfoPrintf("gc6plus: Writing %s = 0x%08x\n", reg->GetName(), reg_data);
      
    //check the connectivity
    FIND_REGISTER(m_regMap, reg, "LW_PPRIV_MASTER_RING_START_RESULTS");
    reg_addr = reg->GetAddress();
    reg_data = lwgpu->RegRd32(reg_addr);
    InfoPrintf("gc6plus: Reading %s = 0x%08x\n", reg->GetName(), reg_data);
    FIND_FIELD(reg, field, "LW_PPRIV_MASTER_RING_START_RESULTS_CONNECTIVITY");
    FIND_VALUE(field, value,
        "LW_PPRIV_MASTER_RING_START_RESULTS_CONNECTIVITY_FAIL");
    connectivity = (reg_data & field->GetMask()) >> field->GetStartBit();
    if (connectivity != value->GetValue()) {
        ErrPrintf("gc6plus: Unexpected value in %s field: exp = 0x%0x, act = "
            "0x%0x\n", field->GetName(), value->GetValue(), connectivity);
        m_ErrCount++;
    }

    //rop on ring not start
    RETURN_IF_ERR(Reg_RMW("LW_PPRIV_SYS_PRIV_DECODE_CONFIG", "RING",
        "DROP_ON_RING_NOT_STARTED"));
    
    //deassert the priv ring reset
    RETURN_IF_ERR(Reg_RMW("LW_PPRIV_MASTER_RING_GLOBAL_CTL", "RING_RESET",
        "DEASSERTED"));
    
    //enumerate the ring
    InfoPrintf("gc6plus: Enumerating PRIV RING\n");
    FIND_REGISTER(m_regMap, reg, "LW_PPRIV_MASTER_RING_COMMAND");
    reg_addr = reg->GetAddress();
    FIND_FIELD(reg, field, "LW_PPRIV_MASTER_RING_COMMAND_CMD");
    FIND_VALUE(field, value,
        "LW_PPRIV_MASTER_RING_COMMAND_CMD_ENUMERATE_STATIONS");
    reg_data = (value->GetValue() << field->GetStartBit());
    FIND_FIELD(reg, field,
        "LW_PPRIV_MASTER_RING_COMMAND_CMD_ENUMERATE_STATIONS_BC_GRP");
    FIND_VALUE(field, value,
        "LW_PPRIV_MASTER_RING_COMMAND_CMD_ENUMERATE_STATIONS_BC_GRP_ALL");
    reg_data |= (value->GetValue() << field->GetStartBit());
    lwgpu->RegWr32(reg_addr, reg_data);
    InfoPrintf("gc6plus: Writing %s = 0x%08x\n", reg->GetName(), reg_data);

    //poll the ring status
    if (poll_reg_field("LW_PPRIV_MASTER_RING_COMMAND", "CMD", "NO_CMD",
        true, m_polling_timeout_us))
    {
        ErrPrintf("gc6plus: Timeout polling for ring status to complete\n");
        m_ErrCount++;
    }
    
    //start priv ring
    InfoPrintf("gc6plus: Starting PRIV RING\n");
    RETURN_IF_ERR(Reg_RMW("LW_PPRIV_MASTER_RING_COMMAND", "CMD", "START_RING"));

    //wait for ring start
    RETURN_IF_ERR(Reg_RMW("LW_PPRIV_SYS_PRIV_DECODE_CONFIG", "RING",
        "WAIT_FOR_RING_START_COMPLETE"));

    //Enable all engines after PRIV init is compelte.
    FIND_REGISTER(m_regMap, reg, "LW_PMC_ENABLE");
    reg_addr = reg->GetAddress();
    reg_data = 0xffffffff;
    lwgpu->RegWr32(reg_addr, reg_data);
    InfoPrintf("gc6plus: Writing %s = 0x%08x\n", reg->GetName(), reg_data);

    return (0);
}

// This routine configures the testbench for proper operation.  All
// configuration is done through escape writes.  Most configuration are
// GPIO-based, however there are a few gc6plus_stub configuration
// dealing with ec_reset and ec_gpu_event.
int
GC6Plus::cfg_fullchip_tb()
{
    UINT32 rand = 0;
    int rand_signed = 0;
    m_rndStream = new RandomStream(m_seed, 0x222, 0x333);

    if(m_tb_mode == DEFAULT)
    {
        int index;

        // GPIO outputs used by this test:
        //   VID_PWM
        //   PEXVDD_VID
        //   MAIN_3V3_EN
        //   PEX_RST (GPU_GC6_RST)
        //   FBVDDQ_GPU_EN
        //   GPU_EVENT
        // GPIO inputs used by this test:
        //   GPU_EVENT
        //   MISC[0]
        //   SYS_RST_MON

        // These assignments are arbitrary.
        // Not all assignments are used by the test.
        // GPIOs 0, 1, 15, 19, and 21 are pulled down in gpio_engine
        gpio_index_hpd[0]          =  0;
        gpio_index_fb_clamp        =  1;
        gpio_index_lwvdd_pexvdd_en =  2;
        gpio_index_fbvdd_q_mem_en  =  3;
        gpio_index_vid_pwm         =  4;
        gpio_index_main_3v3_en     =  5;
        gpio_index_fsm_gpu_event   =  6;
        gpio_index_fbvddq_gpu_en   =  7;
        gpio_index_pexvdd_vid      =  8;
        gpio_index_gpu_gc6_rst     =  9;
        gpio_index_hpd[1]          = 10;
        gpio_index_hpd[2]          = 11;
        gpio_index_hpd[3]          = 12;
        gpio_index_hpd[4]          = 13;
        gpio_index_hpd[5]          = 14;
        gpio_index_sys_rst_mon     = 15;
        //Save 16 for smartfan
        gpio_index_power_alert     = 17;
        gpio_index_misc[0]         = 18;
        gpio_index_misc[1]         = 19;
        gpio_index_misc[2]         = 20;
        gpio_index_misc[3]         = 21;


        // This GPIO is used by this mdiag test and PMU for synchronizing
        // exelwtion between the two SW threads.  We should avoid any GPIO
        // used as a wakeup, otherwise we need to take extra steps to mask
        // that GPIO from generating a wake event.
        gpio_index_pmu_comm        = 3;

        InfoPrintf("gc6plus: Configuring fullchip testbench\n");

        // configure gc6plus_stubs to map gpio to functions
        // only the following are supported by gc6plus_stubs
        Platform::EscapeWrite("gc6plus_lwvdd_pexvdd_en_gpio",0, 4,
            gpio_index_lwvdd_pexvdd_en);
        Platform::EscapeWrite("gc6plus_fbvdd_q_mem_en_gpio", 0, 4,
            gpio_index_fbvdd_q_mem_en);
        Platform::EscapeWrite("gc6plus_pwm_vid_gpio", 0, 4,
            gpio_index_vid_pwm);
        Platform::EscapeWrite("gc6plus_main_3v3_en_gpio", 0, 4,
            gpio_index_main_3v3_en);
        Platform::EscapeWrite("gc6plus_gpu_event_gpio", 0, 4,
            gpio_index_fsm_gpu_event);
        Platform::EscapeWrite("gc6plus_fbvddq_gpu_en_gpio", 0, 4,
            gpio_index_fbvddq_gpu_en);
        Platform::EscapeWrite("gc6plus_pexvdd_vid_gpio", 0, 4,
            gpio_index_pexvdd_vid);
        Platform::EscapeWrite("gc6plus_gpu_gc6_rst_gpio", 0, 4,
            gpio_index_gpu_gc6_rst);
        Platform::EscapeWrite("gc6plus_sys_rst_mon_gpio", 0, 4,
            gpio_index_sys_rst_mon);

        // configure drive and state for GPIOs that are driven by the gpio_engine
        // These include HPD, MISC, and POWER_ALERT
        for (index = 0; index < 6; index++)
        {
            RETURN_IF_ERR(set_gpio_state(gpio_index_hpd[index], 0));
            RETURN_IF_ERR(cfg_gpio_engine(gpio_index_hpd[index], GPIO_DRV_FULL,
                GPIO_PULL_NONE));
        }
        for (index = 0; index < 4; index++)
        {
            RETURN_IF_ERR(set_gpio_state(gpio_index_misc[index], 0));
            RETURN_IF_ERR(cfg_gpio_engine(gpio_index_misc[index], GPIO_DRV_FULL,
                GPIO_PULL_NONE));
        }
        // power alert is open drain, active low
        RETURN_IF_ERR(set_gpio_state(gpio_index_power_alert, 1));
        RETURN_IF_ERR(cfg_gpio_engine(gpio_index_power_alert, GPIO_DRV_OPEN_DRAIN,
                GPIO_PULL_UP));

        // configure the drive and pull of gpios driven by GPU
        // These include GPU_GC6_RST, VID_PWM, PEXVDD_VID, MAIN_3V3_EN, PEX_RST,
        // and FBVDDQ_GPU_EN
        RETURN_IF_ERR(cfg_gpio_engine(gpio_index_gpu_gc6_rst, GPIO_DRV_NONE,
            GPIO_PULL_UP));
        RETURN_IF_ERR(cfg_gpio_engine(gpio_index_vid_pwm, GPIO_DRV_NONE,
            GPIO_PULL_UP));
        RETURN_IF_ERR(cfg_gpio_engine(gpio_index_pexvdd_vid, GPIO_DRV_NONE,
            GPIO_PULL_UP));
        RETURN_IF_ERR(cfg_gpio_engine(gpio_index_main_3v3_en, GPIO_DRV_NONE,
            GPIO_PULL_UP));
        RETURN_IF_ERR(cfg_gpio_engine(gpio_index_gpu_gc6_rst, GPIO_DRV_NONE,
            GPIO_PULL_UP));
        RETURN_IF_ERR(cfg_gpio_engine(gpio_index_fbvddq_gpu_en, GPIO_DRV_NONE,
            GPIO_PULL_UP));

        // configure the drive and pull of gpios driven by logic in the testbench
        // These include GPU_EVENT and SYS_RST_MON
        RETURN_IF_ERR(cfg_gpio_engine(gpio_index_fsm_gpu_event, GPIO_DRV_NONE,
            GPIO_PULL_UP));
        RETURN_IF_ERR(cfg_gpio_engine(gpio_index_sys_rst_mon, GPIO_DRV_NONE,
            GPIO_PULL_UP));

        // configure the drive and pull of gpio used for mdiag/pmu communication
        RETURN_IF_ERR(set_gpio_state(gpio_index_pmu_comm, 1));
        RETURN_IF_ERR(cfg_gpio_engine(gpio_index_pmu_comm, GPIO_DRV_OPEN_DRAIN,
            GPIO_PULL_UP));

        // The following GPIOs are not configured.  It is unknown whether they
        // actually affect any behavior in the testbench.  We can configure them
        // at a later time if they are actually used.
        //   gpio_index_fb_clamp
        //   gpio_index_lwvdd_pexvdd_en
        //   gpio_index_fbvdd_q_mem_en

        // default for gc6plus_stubs
        Platform::EscapeWrite("gc6plus_ec_pex_reset_", 0, 1, 1);
        Platform::EscapeWrite("gc6plus_ec_pex_reset_override", 0, 1, 1);
        Platform::EscapeWrite("gc6plus_ec_gpu_event_", 0, 1, 1);
        Platform::EscapeWrite("gc6plus_ec_gpu_event_override", 0, 1, 1);

        if (enab_system_reset_mon)
        {
            InfoPrintf("gc6plus: Enabling GPIO system reset monitor\n");
            Platform::EscapeWrite("gc6plus_sys_rst_mon_enable", 0, 1, 1);
        }

        InfoPrintf("gc6plus: Enabling GPU GC6 reset feedback to PEX rst\n");
        Platform::EscapeWrite("gc6plus_sdr_mode_ready", 0, 1, 1);
    }
    else if(m_tb_mode == GPIO_NOPULL)
    {
        m_max_gpios = 22;       //hard-coded for init value

        InfoPrintf("%s-%s: Configuring testbench for IO_CLOBBER test. "
            "All GPIOs should have weak-pulls removed.\n",__FILE__, __FUNCTION__);

        UINT32 array_max, vsize, i;
        unique_ptr<IRegisterClass> reg;
        
        //Get number of GPIOs and store in global variable
        FIND_REGISTER(m_regMap, reg, "LW_PGC6_SCI_GPIO_OUTPUT_CNTL");
        reg->GetFormula1(array_max, vsize);

        m_max_gpios = array_max;
        InfoPrintf("gc6plus: Number of GPIO's=%d\n",array_max);
       
        //configure all GPIOs for no pull at all. 
        for(i=0;i<m_max_gpios;i++)
        {
            RETURN_IF_ERR(cfg_gpio_engine(i, 
            GPIO_DRV_NONE, GPIO_PULL_NONE));

        }
    }

    // configure random offset for BSI trip point
    // Can be a plus or minus offset
    if (enab_rand_lwvdd_ok_offset)
    {
        rand = m_rndStream->RandomUnsigned(0,63);
        rand_signed = rand - 32;
        m_lwvdd_ok_offset = (unsigned int) rand_signed;
    }
    Platform::EscapeWrite("gc6plus_opelwr_stub_lwvdd_ok_offset", 0, 8,
        m_lwvdd_ok_offset);

    // default values for VID_PWM configuration
    // Note: this is dependent on the power sequencer.  If the sequencer
    // source changes, these value may need updating.
    m_vid_pwm_period    = 33;
    m_vid_pwm_hi_normal = 17;

    return (0);
}

// This routine configures the drive and weakpull state of the GPIO engine for
// the specified GPIO.  This is done using escape write.
int
GC6Plus::cfg_gpio_engine(UINT32 gpio, UINT32 drive, UINT32 pull)
{
    char escapename[127];
    UINT32 check_data = 0;

    // configure the drive mode
    if (sprintf(escapename, "Gpio_%d_drv", gpio) < 0)
    {
        ErrPrintf("gc6plus: Failed to generate GPIO escape name\n");
        return (1);
    }
    InfoPrintf("gc6plus: EscapeWrite %s = %d\n", escapename, drive);
    Platform::EscapeWrite(escapename, 0, 1, drive);
    Platform::EscapeRead(escapename, 0, 1, &check_data);
    if (check_data != drive)
    {
        ErrPrintf("gc6plus: Failure in EscapeWrite of %s.  Wrote %d, Read %d\n",            escapename, drive, check_data);
        m_ErrCount++;
    }

    // configure the weakpull
    if (sprintf(escapename, "Gpio_%d_pull", gpio) < 0)
    {
        ErrPrintf("gc6plus: Failed to generate GPIO escape name\n");
        return (1);
    }
    InfoPrintf("gc6plus: EscapeWrite %s = %d\n", escapename, pull);
    Platform::EscapeWrite(escapename, 0, 1, pull);
    Platform::EscapeRead(escapename, 0, 1, &check_data);
    if (check_data != pull)
    {
        ErrPrintf("gc6plus: Failure in EscapeWrite of %s.  Wrote %d, Read %d\n",            escapename, pull, check_data);
        m_ErrCount++;
    }

    return (0);
}

// This routine updates the testbench state of the specified GPIO.  This is
// done using escape write.  This updates the output state only, it does not
// change the drive or weakpull configuration of the testbench.
int
GC6Plus::set_gpio_state(UINT32 gpio, UINT32 state)
{
    char escapename[127];
    UINT32 check_data = 0;

    if (sprintf(escapename, "Gpio_%d_state", gpio) < 0)
    {
        ErrPrintf("gc6plus: Failed to generate GPIO escape name\n");
        return (1);
    }
    InfoPrintf("gc6plus: EscapeWrite %s = %d\n", escapename, state);
    Platform::EscapeWrite(escapename, 0, 1, state);
    Platform::EscapeRead(escapename, 0, 1, &check_data);
    if (check_data != state)
    {
        ErrPrintf("gc6plus: Failure in EscapeWrite of %s.  Wrote %d, Read %d\n",            escapename, state, check_data);
        m_ErrCount++;
    }

    return (0);
}

// This routine gets the GPIO state as seen by the testbench.
UINT32
GC6Plus::get_gpio_state(UINT32 gpio)
{
    char escapename[8];
    const char *gpio_data_char;
    UINT32 gpio_data;

    if (sprintf(escapename, "Gpio_%d", gpio) < 0)
    {
        ErrPrintf("gc6plus: Failed to generate GPIO escape name\n");
        m_ErrCount++;
        return (0);
    }
    Platform::EscapeRead(escapename, 0, 4, &gpio_data);
    switch (gpio_data)
    {
        case GPIO_RD_LOW :
            gpio_data_char = "0";
            break;
        case GPIO_RD_HIGH :
            gpio_data_char = "1";
            break;
        case GPIO_RD_HIZ :
            gpio_data_char = "Z";
            break;
        case GPIO_RD_X :
            gpio_data_char = "X";
            break;
        default :
            ErrPrintf("gc6plus: Illegal value detected in %s: %d\n",
                escapename, gpio_data);
            m_ErrCount++;
            return (0);
            break;
    }
    InfoPrintf("gc6plus: EscapeRead of %s: %s\n", escapename, gpio_data_char);

    return (gpio_data);
}

// This routine polls the GPIO state until it matches the expected state.
int
GC6Plus::poll_gpio_state(UINT32 gpio, UINT32 state, bool equal,
    UINT32 timeout)
{
    UINT32 gpio_state, polling_attempts;
    bool done;

    InfoPrintf("gc6plus: Polling Gpio_%d\n", gpio);

    // Poll for GPIO state
    polling_attempts = 1;
    gpio_state = get_gpio_state(gpio);
    if (equal)
    {
        done = (gpio_state == state);
    }
    else
    {
        done = (gpio_state != state);
    }
    while (!done && (polling_attempts < timeout))
    {
        if ((polling_attempts % 10) == 1)
        {
            InfoPrintf("gc6plus: Polling for GPIO_%d state to be %s to %d: "
                "attempt %d\n", gpio, equal ? "equal" : "not equal", state,
                polling_attempts);
        }
        Platform::Delay(1);
        polling_attempts = polling_attempts + 1;
        gpio_state = get_gpio_state(gpio);
        if (equal)
        {
            done = (gpio_state == state);
        }
        else
        {
            done = (gpio_state != state);
        }
    }
    InfoPrintf("gc6plus: Done polling for GPIO_%d state to be %s to %d: "
        "attempt %d\n", gpio, equal ? "equal" : "not equal", state,
        polling_attempts);
    if (!done)
    {
        ErrPrintf("gc6plus: Timeout polling for GPIO_%d state to be %s to "
            "%d after %d attempts\n", gpio, equal ? "eaual" : "not equal",
            state, polling_attempts);
        return (1);
    }

    return (0);
}

int
GC6Plus::cfg_therm()
{
    unique_ptr<IRegisterClass> reg;
    unique_ptr<IRegisterField> field;
    unique_ptr<IRegisterValue> value;
    UINT32 reg_addr, reg_data;

    InfoPrintf("gc6plus: Configuring THERM registers\n");

    // FIXME: setup Temperature sensor and TEMP_ALERT

    // setup SMBus/I2CS device address
    // Note: the ADDR_X fields in LW_THERM_I2C_ADDR is an 8-bit field while
    // the device address in "therm_dev_addr" is a 7-bit field.  We need to
    // left justify therm_dev_addr in the ADDR_X fields.  We do not know which
    // I2C addr is being selected because we do not know the strap and fuse
    // values.  If we program all four ADDR_X fields the same, then the strap
    // and fuse become a dont care.
    FIND_REGISTER(m_regMap, reg, "LW_THERM_I2C_ADDR");
    reg_addr = reg->GetAddress();
    FIND_FIELD(reg,   field, "LW_THERM_I2C_ADDR_0");
    reg_data = (((therm_dev_addr << 1) << field->GetStartBit()) &
        field->GetMask());
    FIND_FIELD(reg,   field, "LW_THERM_I2C_ADDR_1");
    reg_data |= (((therm_dev_addr << 1) << field->GetStartBit()) &
        field->GetMask());
    FIND_FIELD(reg,   field, "LW_THERM_I2C_ADDR_2");
    reg_data |= (((therm_dev_addr << 1) << field->GetStartBit()) &
        field->GetMask());
    FIND_FIELD(reg,   field, "LW_THERM_I2C_ADDR_3");
    reg_data |= (((therm_dev_addr << 1) << field->GetStartBit()) &
        field->GetMask());
    lwgpu->RegWr32(reg_addr, reg_data);
    InfoPrintf("gc6plus: Writing %s = 0x%08x\n", reg->GetName(), reg_data);

    // setup VID_PWM
    InfoPrintf("gc6plus: Programming THERM VID_PWM: period = %d, hi = %d\n",
        m_vid_pwm_period * 4, m_vid_pwm_hi_normal * 4);

    FIND_REGISTER(m_regMap, reg, "LW_THERM_VID0_PWM");
    reg_addr = reg->GetAddress();
    FIND_FIELD(reg,   field, "LW_THERM_VID0_PWM_PERIOD");
    reg_data = (((m_vid_pwm_period * 4) << field->GetStartBit()) &
        field->GetMask());
    FIND_FIELD(reg,   field, "LW_THERM_VID0_PWM_BIT_SPREAD");
    FIND_VALUE(field, value, "LW_THERM_VID0_PWM_BIT_SPREAD_ENABLE");
    reg_data |= value->GetValue() << field->GetStartBit();
    // Since THERM clock is 4x faster than SCI clock, we want to increase
    // THERM's MIN_SPREAD to 4 clocks (program the field to (4 - 1) = 3).
    // This makes the frequency content of the VID_PWM the same between
    // THERM and SCI.
    FIND_FIELD(reg, field, "LW_THERM_VID0_PWM_MIN_SPREAD");
    reg_data |= ((3 << field->GetStartBit()) & field->GetMask());
    lwgpu->RegWr32(reg_addr, reg_data);
    InfoPrintf("gc6plus: Writing %s = 0x%08x\n", reg->GetName(), reg_data);

    FIND_REGISTER(m_regMap, reg, "LW_THERM_VID1_PWM");
    reg_addr = reg->GetAddress();
    FIND_FIELD(reg,   field, "LW_THERM_VID1_PWM_HI");
    reg_data = (((m_vid_pwm_hi_normal * 4) << field->GetStartBit()) &
        field->GetMask());
    FIND_FIELD(reg,   field, "LW_THERM_VID1_PWM_SETTING_NEW");
    FIND_VALUE(field, value, "LW_THERM_VID1_PWM_SETTING_NEW_TRIGGER");
    reg_data |= value->GetValue() << field->GetStartBit();
    lwgpu->RegWr32(reg_addr, reg_data);
    InfoPrintf("gc6plus: Writing %s = 0x%08x\n", reg->GetName(), reg_data);

    return (0);
}

int
GC6Plus::cfg_pmgr()
{
    unique_ptr<IRegisterClass> reg;
    unique_ptr<IRegisterField> field;
    unique_ptr<IRegisterValue> value;
    UINT32 reg_addr, reg_data;
    string regname, fieldname, valuename;

    InfoPrintf("gc6plus: Configuring PMGR registers\n");

    // configure GPIOs
    // At init time, PMGR is typically hands off when it comes to driving the
    // GPIOs.  We need to configure PMGR for the "shared" GPIOs.  This includes
    // VID_PWM.

    //Configuring PMGR GPIO to VID PWM
    InfoPrintf("gc6plus: Configuring PMGR GPIO %d for VID_PWM\n", gpio_index_vid_pwm);
    regname = Utility::StrPrintf("LW_GPIO_OUTPUT_CNTL");
    if (regname.c_str() < 0)
    {
        ErrPrintf("gc6plus: Failed to generate GPIO config regname\n");
        return (1);
    }
    
    if (!(reg = m_regMap->FindRegister(regname.c_str())))
    {
        ErrPrintf("gc6plus: Failed to find %s register\n", regname.c_str());
        return (1);
    }
    
    reg_addr = reg->GetAddress(gpio_index_vid_pwm);
    fieldname =  "LW_GPIO_OUTPUT_CNTL_SEL";
    if (!(field = reg->FindField(fieldname.c_str())))
    {
        ErrPrintf("gc6plus: Failed to find %s field\n", fieldname.c_str());
        return (1);
    }

    InfoPrintf("gc6plus: Setting PMGR GPIO %d SEL field to VID PWM \n",gpio_index_vid_pwm);
    valuename = "LW_GPIO_OUTPUT_CNTL_SEL_VID_PWM"; 
    if (!(value = field->FindValue(valuename.c_str())))
    {
   	    ErrPrintf("gc6plus: Failed to find %s value\n", valuename.c_str());
   	    return (1);
   	}
    reg_data = value->GetValue() << field->GetStartBit(); 
    lwgpu->RegWr32(reg_addr, reg_data);
    InfoPrintf("gc6plus: Writing %s = 0x%08x\n", reg->GetName(),reg_data);


    //Configuring PMGR GPIO for GC5 MODE(GC5_MODE (PEXVDD_VID) - force de-assert)
    InfoPrintf("gc6plus: Configuring PMGR GPIO %d for GC5_MODE\n",gpio_index_pexvdd_vid);
    regname = Utility::StrPrintf("LW_GPIO_OUTPUT_CNTL");
    if (regname.c_str() < 0)
    {
        ErrPrintf("gc6plus: Failed to generate GPIO config regname\n");
        return (1);
    }
    
    if (!(reg = m_regMap->FindRegister(regname.c_str())))
    {
        ErrPrintf("gc6plus: Failed to find %s register\n", regname.c_str());
        return (1);
    }
    
    reg_addr = reg->GetAddress(gpio_index_pexvdd_vid);
    fieldname =  "LW_GPIO_OUTPUT_CNTL_SEL";
    if (!(field = reg->FindField(fieldname.c_str())))
    {
        ErrPrintf("gc6plus: Failed to find %s field\n", fieldname.c_str());
        return (1);
    }

    InfoPrintf("gc6plus: Setting PMGR GPIO %d SEL field to NORMAL \n",gpio_index_pexvdd_vid);
    valuename = "LW_GPIO_OUTPUT_CNTL_SEL_NORMAL"; 
    if (!(value = field->FindValue(valuename.c_str())))
    {
   	    ErrPrintf("gc6plus: Failed to find %s value\n", valuename.c_str());
   	    return (1);
   	}
    reg_data = value->GetValue() << field->GetStartBit(); 
        
    fieldname =  "LW_GPIO_OUTPUT_CNTL_IO_OUT_EN";
    if (!(field = reg->FindField(fieldname.c_str())))
    {
        ErrPrintf("gc6plus: Failed to find %s field\n", fieldname.c_str());
        return (1);
    }

    InfoPrintf("gc6plus: Setting PMGR GPIO %d OUT EN field to YES \n",gpio_index_pexvdd_vid);
    valuename = "LW_GPIO_OUTPUT_CNTL_IO_OUT_EN_YES"; 
    if (!(value = field->FindValue(valuename.c_str())))
    {
   	    ErrPrintf("gc6plus: Failed to find %s value\n", valuename.c_str());
   	    return (1);
   	}
    reg_data |= value->GetValue() << field->GetStartBit(); 

    fieldname =  "LW_GPIO_OUTPUT_CNTL_IO_OUTPUT";
    if (!(field = reg->FindField(fieldname.c_str())))
    {
        ErrPrintf("gc6plus: Failed to find %s field\n", fieldname.c_str());
        return (1);
    }

    InfoPrintf("gc6plus: Setting PMGR GPIO %d IO OUPUT field to 0 \n",gpio_index_pexvdd_vid);
    valuename = "LW_GPIO_OUTPUT_CNTL_IO_OUTPUT_0"; 
    if (!(value = field->FindValue(valuename.c_str())))
    {
   	    ErrPrintf("gc6plus: Failed to find %s value\n", valuename.c_str());
   	    return (1);
   	}
    reg_data |= value->GetValue() << field->GetStartBit(); 
    lwgpu->RegWr32(reg_addr, reg_data);
    InfoPrintf("gc6plus: Writing %s = 0x%08x\n", reg->GetName(),reg_data);


    //Configuring PMGR GPIO for for GPU GC6 reset(PEX_RST_HOLD)
    InfoPrintf("gc6plus: Configuring PMGR GPIO %d for GPU GC6 reset\n",gpio_index_gpu_gc6_rst);
    regname = Utility::StrPrintf("LW_GPIO_OUTPUT_CNTL");
    if (regname.c_str() < 0)
    {
        ErrPrintf("gc6plus: Failed to generate GPIO config regname\n");
        return (1);
    }
    
    if (!(reg = m_regMap->FindRegister(regname.c_str())))
    {
        ErrPrintf("gc6plus: Failed to find %s register\n", regname.c_str());
        return (1);
    }
    
    reg_addr = reg->GetAddress(gpio_index_gpu_gc6_rst);
    fieldname =  "LW_GPIO_OUTPUT_CNTL_SEL";
    if (!(field = reg->FindField(fieldname.c_str())))
    {
        ErrPrintf("gc6plus: Failed to find %s field\n", fieldname.c_str());
        return (1);
    }

    InfoPrintf("gc6plus: Setting PMGR GPIO %d SEL field to NORMAL \n",gpio_index_gpu_gc6_rst);
    valuename = "LW_GPIO_OUTPUT_CNTL_SEL_NORMAL"; 
    if (!(value = field->FindValue(valuename.c_str())))
    {
   	    ErrPrintf("gc6plus: Failed to find %s value\n", valuename.c_str());
   	    return (1);
   	}
    reg_data = value->GetValue() << field->GetStartBit(); 
        
    fieldname =  "LW_GPIO_OUTPUT_CNTL_IO_OUT_EN";
    if (!(field = reg->FindField(fieldname.c_str())))
    {
        ErrPrintf("gc6plus: Failed to find %s field\n", fieldname.c_str());
        return (1);
    }

    InfoPrintf("gc6plus: Setting PMGR GPIO %d OUT EN field to NO \n",gpio_index_gpu_gc6_rst);
    valuename = "LW_GPIO_OUTPUT_CNTL_IO_OUT_EN_NO"; 
    if (!(value = field->FindValue(valuename.c_str())))
    {
   	    ErrPrintf("gc6plus: Failed to find %s value\n", valuename.c_str());
   	    return (1);
   	}
    reg_data |= value->GetValue() << field->GetStartBit(); 
    lwgpu->RegWr32(reg_addr, reg_data);
    InfoPrintf("gc6plus: Writing %s = 0x%08x\n", reg->GetName(),reg_data);

    
    // mdiag/PMU communication - open drain configuration
    // Note: PMGR does not support open drain output drive for normal GPIO.
    // We hack around this by forcing the GPIO's IO_OUT_EN to tri-state.
    InfoPrintf("gc6plus: Configuring PMGR GPIO %d for PMU communication\n", gpio_index_pmu_comm);
    regname = Utility::StrPrintf("LW_GPIO_OUTPUT_CNTL");
    if (regname.c_str() < 0)
    {
        ErrPrintf("gc6plus: Failed to generate GPIO config regname\n");
        return (1);
    }
    
    if (!(reg = m_regMap->FindRegister(regname.c_str())))
    {
        ErrPrintf("gc6plus: Failed to find %s register\n", regname.c_str());
        return (1);
    }
    
    reg_addr = reg->GetAddress(gpio_index_pmu_comm);
    fieldname =  "LW_GPIO_OUTPUT_CNTL_SEL";
    if (!(field = reg->FindField(fieldname.c_str())))
    {
        ErrPrintf("gc6plus: Failed to find %s field\n", fieldname.c_str());
        return (1);
    }

    InfoPrintf("gc6plus: Setting PMGR GPIO %d SEL field to NORMAL \n",gpio_index_pmu_comm);
    valuename = "LW_GPIO_OUTPUT_CNTL_SEL_NORMAL"; 
    if (!(value = field->FindValue(valuename.c_str())))
    {
   	    ErrPrintf("gc6plus: Failed to find %s value\n", valuename.c_str());
   	    return (1);
   	}
    reg_data = value->GetValue() << field->GetStartBit(); 
        
    fieldname =  "LW_GPIO_OUTPUT_CNTL_IO_OUT_EN";
    if (!(field = reg->FindField(fieldname.c_str())))
    {
        ErrPrintf("gc6plus: Failed to find %s field\n", fieldname.c_str());
        return (1);
    }

    InfoPrintf("gc6plus: Setting PMGR GPIO %d OUT EN field to NO \n",gpio_index_pmu_comm);
    valuename = "LW_GPIO_OUTPUT_CNTL_IO_OUT_EN_NO"; 
    if (!(value = field->FindValue(valuename.c_str())))
    {
   	    ErrPrintf("gc6plus: Failed to find %s value\n", valuename.c_str());
   	    return (1);
   	}
    reg_data |= value->GetValue() << field->GetStartBit(); 

	fieldname =  "LW_GPIO_OUTPUT_CNTL_OPEN_DRAIN";
    if (!(field = reg->FindField(fieldname.c_str())))
    {
        ErrPrintf("gc6plus: Failed to find %s field\n", fieldname.c_str());
        return (1);
    }

    InfoPrintf("gc6plus: Setting PMGR GPIO %d Open Drain Mode to Enable \n",gpio_index_pmu_comm);
    valuename = "LW_GPIO_OUTPUT_CNTL_OPEN_DRAIN_ENABLE"; 
    if (!(value = field->FindValue(valuename.c_str())))
    {
   	    ErrPrintf("gc6plus: Failed to find %s value\n", valuename.c_str());
   	    return (1);
   	}
    reg_data |= value->GetValue() << field->GetStartBit(); 
	
    fieldname =  "LW_GPIO_OUTPUT_CNTL_IO_OUTPUT";
    if (!(field = reg->FindField(fieldname.c_str())))
    {
        ErrPrintf("gc6plus: Failed to find %s field\n", fieldname.c_str());
        return (1);
    }

    InfoPrintf("gc6plus: Setting PMGR GPIO %d IO OUPUT field to 0 \n",gpio_index_pmu_comm);
    valuename = "LW_GPIO_OUTPUT_CNTL_IO_OUTPUT_0"; 
    if (!(value = field->FindValue(valuename.c_str())))
    {
   	    ErrPrintf("gc6plus: Failed to find %s value\n", valuename.c_str());
   	    return (1);
   	}
    reg_data |= value->GetValue() << field->GetStartBit(); 
    lwgpu->RegWr32(reg_addr, reg_data);
    InfoPrintf("gc6plus: Writing %s = 0x%08x\n", reg->GetName(),reg_data);

    // Trigger PMGR GPIO update after all PMGR GPIOs are configured
    InfoPrintf("gc6plus: Triggering PMGR GPIO update\n");
    FIND_REGISTER(m_regMap, reg, "LW_PMGR_GPIO_OUTPUT_CNTL_TRIGGER");
    reg_addr = reg->GetAddress();
    FIND_FIELD(reg,   field, "LW_PMGR_GPIO_OUTPUT_CNTL_TRIGGER_UPDATE");
    FIND_VALUE(field, value, "LW_PMGR_GPIO_OUTPUT_CNTL_TRIGGER_UPDATE_TRIGGER");
    reg_data = value->GetValue() << field->GetStartBit();
    lwgpu->RegWr32(reg_addr, reg_data);
    InfoPrintf("gc6plus: Writing %s = 0x%08x\n", reg->GetName(), reg_data);

    return (0);
}

int
GC6Plus::cfg_bsi()
{
    unique_ptr<IRegisterClass> reg;
    unique_ptr<IRegisterField> field;
    unique_ptr<IRegisterValue> value;
    UINT32 reg_addr, reg_data;
    UINT32 loop, check_data;
    UINT32 last_bsi_addr;
    bool bsi_ram_loaded;

    InfoPrintf("gc6plus: Configuring BSI registers\n");

    //LDO Initialization
    FIND_REGISTER(m_regMap, reg, "LW_PGC6_BSI_ANALOGCTRL");
    reg_addr = reg->GetAddress();
    FIND_FIELD(reg,   field, "LW_PGC6_BSI_ANALOGCTRL_LWVDDTRIP");
    reg_data = (m_lwvdd_ok_val << field->GetStartBit()) & field->GetMask();
    lwgpu->RegWr32(reg_addr,reg_data);

    // check to see if BSI RAM has been loaded
    Platform::EscapeRead("gc6plus_bsi_ram_loaded", 0, 1, &check_data);
    InfoPrintf("gc6plus: EscapeRead of gc6plus_bsi_ram_loaded: %d\n",
        check_data);
    bsi_ram_loaded = (check_data == 1);

    if (!bsi_ram_loaded)
    {
        InfoPrintf("gc6plus: RTL plusarg +bsi_ram_file missing, BSI RAM is "
            "invalid\n");
        // Zero out first phase descriptor so that BSI does not hang when
        // reading X's from the RAM.
        FIND_REGISTER(m_regMap, reg, "LW_PGC6_BSI_RAMCTRL");
        reg_addr = reg->GetAddress();
        FIND_FIELD(reg,   field, "LW_PGC6_BSI_RAMCTRL_WAUTOINCR");
        FIND_VALUE(field, value, "LW_PGC6_BSI_RAMCTRL_WAUTOINCR_ENABLE");
        reg_data = value->GetValue() << field->GetStartBit();
        lwgpu->RegWr32(reg_addr, reg_data);
        InfoPrintf("gc6plus: Writing %s = 0x%08x\n", reg->GetName(), reg_data);
        FIND_REGISTER(m_regMap, reg, "LW_PGC6_BSI_RAMDATA");
        reg_addr = reg->GetAddress();
        reg_data = 0x0;
        for (loop = 0; loop < 5; loop = loop + 1)
        {
            lwgpu->RegWr32(reg_addr, reg_data);
            InfoPrintf("gc6plus: Writing %s = 0x%08x\n", reg->GetName(),
                reg_data);
        }
        return (0);
    }

    // the bootphase configuration is written to the last address of ram
    InfoPrintf("gc6plus: Configuring bootphases\n");
    // first, get the size of BSI RAM
    FIND_REGISTER(m_regMap, reg, "LW_PGC6_BSI_RAM");
    reg_addr = reg->GetAddress();
    reg_data = lwgpu->RegRd32(reg_addr);
    InfoPrintf("gc6plus: Reading %s = 0x%08x\n", reg->GetName(), reg_data);
    FIND_FIELD(reg, field, "LW_PGC6_BSI_RAM_SIZE");
    last_bsi_addr = (reg_data & field->GetMask()) >> field->GetStartBit();
    // LW_PGC6_BSI_RAM_SIZE is in KB, we need colwert the value to bytes.
    // Also, the BSI RAM is addressed in blocks of 4 bytes, so the last
    // address lives at RAM size (in bytes) minus 4
    last_bsi_addr = (last_bsi_addr << 10) - 4;
    // next, read the value from BSI RAM and write to bootphase register
    FIND_REGISTER(m_regMap, reg, "LW_PGC6_BSI_RAMCTRL");
    reg_addr = reg->GetAddress();
    FIND_FIELD(reg, field, "LW_PGC6_BSI_RAMCTRL_ADDR");
    reg_data = ((last_bsi_addr << field->GetStartBit()) & field->GetMask());
    lwgpu->RegWr32(reg_addr, reg_data);
    InfoPrintf("gc6plus: Writing %s = 0x%08x\n", reg->GetName(), reg_data);
    FIND_REGISTER(m_regMap, reg, "LW_PGC6_BSI_RAMDATA");
    reg_addr = reg->GetAddress();
    reg_data = lwgpu->RegRd32(reg_addr);
    InfoPrintf("gc6plus: Reading %s = 0x%08x\n", reg->GetName(), reg_data);

    // copy to bootphase register
    FIND_REGISTER(m_regMap, reg, "LW_PGC6_BSI_BOOTPHASES");
    reg_addr = reg->GetAddress();
    lwgpu->RegWr32(reg_addr, reg_data);
    InfoPrintf("gc6plus: Writing %s = 0x%08x\n", reg->GetName(), reg_data);

    InfoPrintf("gc6plus: Setting BSI RAM valid\n");
    RETURN_IF_ERR(Reg_RMW("LW_PGC6_BSI_CTRL", "RAMVALID", "TRUE"));

    // load PMU with ucode form BSI
    RETURN_IF_ERR(trigger_pmu_ucode_load());

    return (0);
}

// This routine exercises BSI CYA mode to load PMU ucode prior to GC5/GC6
// entry.  It is assumed that the PMU ucode in the BSI RAM is the same that
// is loaded for a GC6 exit.
int
GC6Plus::trigger_pmu_ucode_load()
{
    unique_ptr<IRegisterClass> reg;
    unique_ptr<IRegisterField> field;
    unique_ptr<IRegisterValue> value;
    UINT32 reg_addr, reg_data;

    InfoPrintf("gc6plus: Triggering GC6 CYA to load PMU ucode\n");

    // This should do nothing since PMU should not be running.  We write it
    // to a non-zero value so we can poll it to indicate when PMU ucode is
    // done loading.
    RETURN_IF_ERR(set_pmu_command(PMU_HALT));

    // override BSI pads
    // We are not actually entering GC5/GC6, so we do not want the BSI SM
    // to touch any of the pad controls.
    FIND_REGISTER(m_regMap, reg, "LW_PGC6_BSI_PADCTRL");
    reg_addr = reg->GetAddress();
    FIND_FIELD(reg, field, "LW_PGC6_BSI_PADCTRL_PADSUSPEND");
    FIND_VALUE(field, value, "LW_PGC6_BSI_PADCTRL_PADSUSPEND_DISABLE");
    reg_data = value->GetValue() << field->GetStartBit();
    FIND_FIELD(reg, field, "LW_PGC6_BSI_PADCTRL_OVERRIDE_PADSUSPEND");
    FIND_VALUE(field, value, "LW_PGC6_BSI_PADCTRL_OVERRIDE_PADSUSPEND_ENABLE");
    reg_data |= value->GetValue() << field->GetStartBit();
    FIND_FIELD(reg, field, "LW_PGC6_BSI_PADCTRL_PEXLPMODE");
    FIND_VALUE(field, value, "LW_PGC6_BSI_PADCTRL_PEXLPMODE_DISABLE");
    reg_data |= value->GetValue() << field->GetStartBit();
    FIND_FIELD(reg, field, "LW_PGC6_BSI_PADCTRL_OVERRIDE_PEXLPMODE");
    FIND_VALUE(field, value, "LW_PGC6_BSI_PADCTRL_OVERRIDE_PEXLPMODE_ENABLE");
    reg_data |= value->GetValue() << field->GetStartBit();
    FIND_FIELD(reg, field, "LW_PGC6_BSI_PADCTRL_CLKREQEN");
    FIND_VALUE(field, value, "LW_PGC6_BSI_PADCTRL_CLKREQEN_DISABLE");
    reg_data |= value->GetValue() << field->GetStartBit();
    FIND_FIELD(reg, field, "LW_PGC6_BSI_PADCTRL_OVERRIDE_CLKREQEN");
    FIND_VALUE(field, value, "LW_PGC6_BSI_PADCTRL_OVERRIDE_CLKREQEN_ENABLE");
    reg_data |= value->GetValue() << field->GetStartBit();
    FIND_FIELD(reg, field, "LW_PGC6_BSI_PADCTRL_GC5MODE");
    FIND_VALUE(field, value, "LW_PGC6_BSI_PADCTRL_GC5MODE_DISABLE");
    reg_data |= value->GetValue() << field->GetStartBit();
    FIND_FIELD(reg, field, "LW_PGC6_BSI_PADCTRL_OVERRIDE_GC5MODE");
    FIND_VALUE(field, value, "LW_PGC6_BSI_PADCTRL_OVERRIDE_GC5MODE_ENABLE");
    reg_data |= value->GetValue() << field->GetStartBit();
    lwgpu->RegWr32(reg_addr, reg_data);
    InfoPrintf("gc6plus: Writing %s = 0x%08x\n", reg->GetName(), reg_data);

    // configure GC6 CYA mode
    FIND_REGISTER(m_regMap, reg, "LW_PGC6_BSI_CTRL");
    reg_addr = reg->GetAddress();
    reg_data = lwgpu->RegRd32(reg_addr);
    InfoPrintf("gc6plus: Reading %s = 0x%08x\n", reg->GetName(), reg_data);
    FIND_FIELD(reg, field, "LW_PGC6_BSI_CTRL_MODE");
    FIND_VALUE(field, value, "LW_PGC6_BSI_CTRL_MODE_CYA");
    reg_data = (reg_data & ~field->GetMask()) |
        (value->GetValue() << field->GetStartBit());
    FIND_FIELD(reg, field, "LW_PGC6_BSI_CTRL_CYA");
    FIND_VALUE(field, value, "LW_PGC6_BSI_CTRL_CYA_GC6");
    reg_data = (reg_data & ~field->GetMask()) |
        (value->GetValue() << field->GetStartBit());
    reg_data |= value->GetValue() << field->GetStartBit();
    lwgpu->RegWr32(reg_addr, reg_data);
    InfoPrintf("gc6plus: Writing %s = 0x%08x\n", reg->GetName(), reg_data);

    // trigger BSI to PMU transfer
    FIND_REGISTER(m_regMap, reg, "LW_PGC6_BSI_PMUTXTRIG");
    reg_addr = reg->GetAddress();
    FIND_FIELD(reg, field, "LW_PGC6_BSI_PMUTXTRIG_TRIG");
    FIND_VALUE(field, value, "LW_PGC6_BSI_PMUTXTRIG_TRIG_TRUE");
    reg_data = value->GetValue() << field->GetStartBit();
    lwgpu->RegWr32(reg_addr, reg_data);
    InfoPrintf("gc6plus: Writing %s = 0x%08x\n", reg->GetName(), reg_data);

    // wait until BSI burst is done
    RETURN_IF_ERR(poll_reg_field("LW_PGC6_BSI_PMUTXTRIG", "TRIG", "FALSE",
        true, m_polling_timeout_us));

    // remove pad control overrides
    FIND_REGISTER(m_regMap, reg, "LW_PGC6_BSI_PADCTRL");
    reg_addr = reg->GetAddress();
    FIND_FIELD(reg, field, "LW_PGC6_BSI_PADCTRL_PADSUSPEND");
    FIND_VALUE(field, value, "LW_PGC6_BSI_PADCTRL_PADSUSPEND_DISABLE");
    reg_data = value->GetValue() << field->GetStartBit();
    FIND_FIELD(reg, field, "LW_PGC6_BSI_PADCTRL_OVERRIDE_PADSUSPEND");
    FIND_VALUE(field, value, "LW_PGC6_BSI_PADCTRL_OVERRIDE_PADSUSPEND_DISABLE");
    reg_data |= value->GetValue() << field->GetStartBit();
    FIND_FIELD(reg, field, "LW_PGC6_BSI_PADCTRL_PEXLPMODE");
    FIND_VALUE(field, value, "LW_PGC6_BSI_PADCTRL_PEXLPMODE_DISABLE");
    reg_data |= value->GetValue() << field->GetStartBit();
    FIND_FIELD(reg, field, "LW_PGC6_BSI_PADCTRL_OVERRIDE_PEXLPMODE");
    FIND_VALUE(field, value, "LW_PGC6_BSI_PADCTRL_OVERRIDE_PEXLPMODE_DISABLE");
    reg_data |= value->GetValue() << field->GetStartBit();
    FIND_FIELD(reg, field, "LW_PGC6_BSI_PADCTRL_CLKREQEN");
    FIND_VALUE(field, value, "LW_PGC6_BSI_PADCTRL_CLKREQEN_DISABLE");
    reg_data |= value->GetValue() << field->GetStartBit();
    FIND_FIELD(reg, field, "LW_PGC6_BSI_PADCTRL_OVERRIDE_CLKREQEN");
    FIND_VALUE(field, value, "LW_PGC6_BSI_PADCTRL_OVERRIDE_CLKREQEN_DISABLE");
    reg_data |= value->GetValue() << field->GetStartBit();
    FIND_FIELD(reg, field, "LW_PGC6_BSI_PADCTRL_GC5MODE");
    FIND_VALUE(field, value, "LW_PGC6_BSI_PADCTRL_GC5MODE_DISABLE");
    reg_data |= value->GetValue() << field->GetStartBit();
    FIND_FIELD(reg, field, "LW_PGC6_BSI_PADCTRL_OVERRIDE_GC5MODE");
    FIND_VALUE(field, value, "LW_PGC6_BSI_PADCTRL_OVERRIDE_GC5MODE_DISABLE");
    reg_data |= value->GetValue() << field->GetStartBit();
    lwgpu->RegWr32(reg_addr, reg_data);
    InfoPrintf("gc6plus: Writing %s = 0x%08x\n", reg->GetName(), reg_data);

    // clear BSI_CTRL_MODE since BSI will not clear it when in CYA mode
    RETURN_IF_ERR(Reg_RMW("LW_PGC6_BSI_CTRL", "MODE", "NONE"));

    // wait until PMU ucode is done
    if (poll_pmu_command(PMU_NOOP, true, m_polling_timeout_us))
    {
        ErrPrintf("gc6plus: Error in polling for PMU Ucode command status\n");
        m_ErrCount++;
    }
    // copy over PMU error count and clear it
    RETURN_IF_ERR(check_pmu_errors());

    return (0);
}

int
GC6Plus::cfg_sci()
{
    unique_ptr<IRegisterClass> reg;
    unique_ptr<IRegisterField> field;
    unique_ptr<IRegisterValue> value;
    const char * valuename;
    UINT32 reg_addr, reg_data;
    bool vid_cfg_is_array;
    UINT32 index, limit, stride;


    InfoPrintf("gc6plus: Configuring SCI registers\n");

    // configure PTIMER
    RETURN_IF_ERR(cfg_sci_ptimer());

    // configure FSM configuration
    // Perform error checking and override based on Self-driven reset mode
    InfoPrintf("gc6plus: Configuring SCI FSM mode\n");
    if (disable_sdr_mode)
    {
        // legacy mode of operation (non-SDR)
        // We must have a reset detection method during GC6.
        if (!enab_system_reset_mon && !enab_gpu_event_reset)
        {
            InfoPrintf("gc6plus: Overriding to enable GPU_EVENT reset "
                "detection during GC6\n");
            enab_gpu_event_reset = true;
        }
        // We must disable GC6 aborts since there is no way for SW to wake
        // the PEX link on a GC6 abort.
        if (enab_gc6_abort)
        {
            InfoPrintf("gc6plus: Overriding to disable GC6 ABORT\n");
            enab_gc6_abort = false;
        }
    }
    else
    {
        // POR mode of operation (SDR)
        // GPU_EVENT signals wake only, it must not be used for reset detection
        if (enab_gpu_event_reset)
        {
            InfoPrintf("gc6plus: Overriding to disable GPU_EVENT reset "
                "detection during GC6\n");
            enab_gpu_event_reset = false;
        }
        // No more handshake with EC for GC6 entry/exit, so EC cannot guarantee
        // SR Update is signalled during STEADY state.
        if (legacy_sr_update)
        {
            InfoPrintf("gc6plus: Overriding to disable SR Update "
                "detection in STEADY state only\n");
            legacy_sr_update = false;
        }
    }
    FIND_REGISTER(m_regMap, reg, "LW_PGC6_SCI_FSM_CFG");
    reg_addr = reg->GetAddress();
    FIND_FIELD(reg, field, "LW_PGC6_SCI_FSM_CFG_SELF_DRIVE_RST");
    if (disable_sdr_mode)
    {
        valuename = "LW_PGC6_SCI_FSM_CFG_SELF_DRIVE_RST_DISABLE";
    }
    else
    {
        valuename = "LW_PGC6_SCI_FSM_CFG_SELF_DRIVE_RST_ENABLE";
    }
    FIND_VALUE(field, value, valuename);
    reg_data = value->GetValue() << field->GetStartBit();
    FIND_FIELD(reg,   field, "LW_PGC6_SCI_FSM_CFG_RESET_GPU_EVENT");
    if (enab_gpu_event_reset)
    {
        valuename = "LW_PGC6_SCI_FSM_CFG_RESET_GPU_EVENT_ENABLE";
    }
    else
    {
        valuename = "LW_PGC6_SCI_FSM_CFG_RESET_GPU_EVENT_DISABLE";
    }
    FIND_VALUE(field, value, valuename);
    reg_data |= value->GetValue() << field->GetStartBit();
    FIND_FIELD(reg,   field, "LW_PGC6_SCI_FSM_CFG_SKIP_GC6_ABORT");
    if (enab_gc6_abort)
    {
        valuename = "LW_PGC6_SCI_FSM_CFG_SKIP_GC6_ABORT_DISABLE";
    }
    else
    {
        valuename = "LW_PGC6_SCI_FSM_CFG_SKIP_GC6_ABORT_ENABLE";
    }
    FIND_VALUE(field, value, valuename);
    reg_data |= value->GetValue() << field->GetStartBit();
    FIND_FIELD(reg,   field, "LW_PGC6_SCI_FSM_CFG_SR_UPDATE_MODE");
    if (legacy_sr_update)
    {
        valuename = "LW_PGC6_SCI_FSM_CFG_SR_UPDATE_MODE_STEADY_STATE";
    }
    else
    {
        valuename = "LW_PGC6_SCI_FSM_CFG_SR_UPDATE_MODE_SECOND_EDGE";
    }
    FIND_VALUE(field, value, valuename);
    reg_data |= value->GetValue() << field->GetStartBit();
    lwgpu->RegWr32(reg_addr, reg_data);
    InfoPrintf("gc6plus: Writing %s = 0x%08x\n", reg->GetName(), reg_data);


    // FSM Timer Timeout values
    InfoPrintf("gc6plus: Configuring SCI FSM timers\n");

    // TMR_GPU_EVENT_ASSERT
    FIND_REGISTER(m_regMap, reg, "LW_PGC6_SCI_TMR_GPU_EVENT_ASSERT");
    reg_addr = reg->GetAddress();
    FIND_FIELD(reg, field, "LW_PGC6_SCI_TMR_GPU_EVENT_ASSERT_TIMEOUT");
    reg_data = (m_sci_gpu_event_width_us << field->GetStartBit()) &
        field->GetMask();
    lwgpu->RegWr32(reg_addr, reg_data);
    InfoPrintf("gc6plus: Writing %s = 0x%08x\n", reg->GetName(), reg_data);

    // TMR_GC6_ABORT_HOLDOFF
    if (cnf_sci_entry_holdoff && (m_mode == GC6))
    {
        FIND_REGISTER(m_regMap, reg, "LW_PGC6_SCI_TMR_GC6_ABORT_HOLDOFF");
        reg_addr = reg->GetAddress();
        FIND_FIELD(reg, field, "LW_PGC6_SCI_TMR_GC6_ABORT_HOLDOFF_TIMEOUT");
        reg_data = (sci_entry_holdoff_us << field->GetStartBit()) &
            field->GetMask();
        lwgpu->RegWr32(reg_addr, reg_data);
        InfoPrintf("gc6plus: Writing %s = 0x%08x\n", reg->GetName(), reg_data);
    }

    // TMR_SYSTEM_RESET
    FIND_REGISTER(m_regMap, reg, "LW_PGC6_SCI_TMR_SYSTEM_RESET");
    reg_addr = reg->GetAddress();
    FIND_FIELD(reg, field, "LW_PGC6_SCI_TMR_SYSTEM_RESET_TIMEOUT");
    reg_data = (20 << field->GetStartBit()) & field->GetMask();
    lwgpu->RegWr32(reg_addr, reg_data);
    InfoPrintf("gc6plus: Writing %s = 0x%08x\n", reg->GetName(), reg_data);

    // TMR_GPU_EVENT_HOLDOFF
    // No override required

    // TMR_GC5_ENTRY_HOLDOFF
    if (cnf_sci_entry_holdoff && (m_mode == GC5))
    {
        FIND_REGISTER(m_regMap, reg, "LW_PGC6_SCI_TMR_GC5_ENTRY_HOLDOFF");
        reg_addr = reg->GetAddress();
        FIND_FIELD(reg, field, "LW_PGC6_SCI_TMR_GC5_ENTRY_HOLDOFF_TIMEOUT");
        reg_data = (sci_entry_holdoff_us << field->GetStartBit()) &
            field->GetMask();
        lwgpu->RegWr32(reg_addr, reg_data);
        InfoPrintf("gc6plus: Writing %s = 0x%08x\n", reg->GetName(), reg_data);
    }


    // PWR_SEQUENCER
    RETURN_IF_ERR(cfg_sci_power_sequencer());

    // PWR_SEQ_STATE
    // all clamps are DEASSERT and all power supplies are ASSERT
    FIND_REGISTER(m_regMap, reg, "LW_PGC6_SCI_PWR_SEQ_ARMED_SEQ_STATE");
    reg_addr = reg->GetAddress();
    FIND_FIELD(reg, field, "LW_PGC6_SCI_PWR_SEQ_ARMED_SEQ_STATE_FB_CLAMP");
    FIND_VALUE(field, value,
        "LW_PGC6_SCI_PWR_SEQ_ARMED_SEQ_STATE_FB_CLAMP_DEASSERT");
    reg_data = (value->GetValue() << field->GetStartBit());
    FIND_FIELD(reg, field,
        "LW_PGC6_SCI_PWR_SEQ_ARMED_SEQ_STATE_XTAL_CORE_CLAMP");
    FIND_VALUE(field, value,
        "LW_PGC6_SCI_PWR_SEQ_ARMED_SEQ_STATE_XTAL_CORE_CLAMP_DEASSERT");
    reg_data |= (value->GetValue() << field->GetStartBit());
    FIND_FIELD(reg, field, "LW_PGC6_SCI_PWR_SEQ_ARMED_SEQ_STATE_PAD_CLAMP");
    FIND_VALUE(field, value,
        "LW_PGC6_SCI_PWR_SEQ_ARMED_SEQ_STATE_PAD_CLAMP_DEASSERT");
    reg_data |= (value->GetValue() << field->GetStartBit());
    FIND_FIELD(reg, field, "LW_PGC6_SCI_PWR_SEQ_ARMED_SEQ_STATE_ALT_MUX_SEL");
    FIND_VALUE(field, value,
        "LW_PGC6_SCI_PWR_SEQ_ARMED_SEQ_STATE_ALT_MUX_SEL_DEASSERT");
    reg_data |= (value->GetValue() << field->GetStartBit());
    FIND_FIELD(reg, field, "LW_PGC6_SCI_PWR_SEQ_ARMED_SEQ_STATE_PEX_RST");
    FIND_VALUE(field, value,
        "LW_PGC6_SCI_PWR_SEQ_ARMED_SEQ_STATE_PEX_RST_DEASSERT");
    reg_data |= (value->GetValue() << field->GetStartBit());
    FIND_FIELD(reg, field, "LW_PGC6_SCI_PWR_SEQ_ARMED_SEQ_STATE_MAIN_3V3_EN");
    FIND_VALUE(field, value,
        "LW_PGC6_SCI_PWR_SEQ_ARMED_SEQ_STATE_MAIN_3V3_EN_ASSERT");
    reg_data |= (value->GetValue() << field->GetStartBit());
    FIND_FIELD(reg, field, "LW_PGC6_SCI_PWR_SEQ_ARMED_SEQ_STATE_FBVDDQ_GPU_EN");
    FIND_VALUE(field, value,
        "LW_PGC6_SCI_PWR_SEQ_ARMED_SEQ_STATE_FBVDDQ_GPU_EN_ASSERT");
    reg_data |= (value->GetValue() << field->GetStartBit());
    FIND_FIELD(reg, field, "LW_PGC6_SCI_PWR_SEQ_ARMED_SEQ_STATE_PEXVDD_VID");
    FIND_VALUE(field, value,
        "LW_PGC6_SCI_PWR_SEQ_ARMED_SEQ_STATE_PEXVDD_VID_DEASSERT");
    reg_data |= (value->GetValue() << field->GetStartBit());
    lwgpu->RegWr32(reg_addr, reg_data);
    InfoPrintf("gc6plus: Writing %s = 0x%08x\n", reg->GetName(), reg_data);

    // Trigger armed to active
    FIND_REGISTER(m_regMap, reg, "LW_PGC6_SCI_PWR_SEQ_STATE");
    reg_addr = reg->GetAddress();
    FIND_FIELD(reg,   field, "LW_PGC6_SCI_PWR_SEQ_STATE_UPDATE");
    FIND_VALUE(field, value, "LW_PGC6_SCI_PWR_SEQ_STATE_UPDATE_TRIGGER");
    reg_data = (value->GetValue() << field->GetStartBit());
    lwgpu->RegWr32(reg_addr, reg_data);
    InfoPrintf("gc6plus: Writing %s = 0x%08x\n", reg->GetName(), reg_data);


    // VID_PWM
    FIND_REGISTER(m_regMap, reg, "LW_PGC6_SCI_VID_CFG0");
    FIND_FIELD(reg,   field, "LW_PGC6_SCI_VID_CFG0_PERIOD");
    reg_data = (m_vid_pwm_period << field->GetStartBit()) & field->GetMask();
    FIND_FIELD(reg,   field, "LW_PGC6_SCI_VID_CFG0_BIT_SPREAD");
    FIND_VALUE(field, value, "LW_PGC6_SCI_VID_CFG0_BIT_SPREAD_ENABLE");
    reg_data |= value->GetValue() << field->GetStartBit();
    FIND_FIELD(reg,   field, "LW_PGC6_SCI_VID_CFG0_MIN_SPREAD");
    FIND_VALUE(field, value, "LW_PGC6_SCI_VID_CFG0_MIN_SPREAD_SHORTEST");
    reg_data |= value->GetValue() << field->GetStartBit();
    // LW_PGC6_SCI_VID_CFG0 and LW_PGC6_SCI_VID_CFG1 changed from a non-array
    // register to a 1-D array register in GP10x (16FF technology).  We need
    // to check the register's dimension to tell us how to generate the reg's
    // address.  The LWVDD VID_PWM generator is assigned to index 0 and the
    // SRAM VID_PWM generator is assigned to index 1.  We need to configure
    // both PWM generators.
    vid_cfg_is_array = reg->GetFormula1(limit, stride);
    if (vid_cfg_is_array)
    {
        for (index = 0; index < limit; index = index + 1)
        {
            reg_addr = reg->GetAddress(index);
            lwgpu->RegWr32(reg_addr, reg_data);
            InfoPrintf("gc6plus: Writing %s(%d) = 0x%08x\n", reg->GetName(),
                index, reg_data);
        }
    }
    else
    {
        reg_addr = reg->GetAddress();
        lwgpu->RegWr32(reg_addr, reg_data);
        InfoPrintf("gc6plus: Writing %s = 0x%08x\n", reg->GetName(), reg_data);
    }

    FIND_REGISTER(m_regMap, reg, "LW_PGC6_SCI_VID_CFG1");
    FIND_FIELD(reg,   field, "LW_PGC6_SCI_VID_CFG1_HI");
    reg_data = (m_vid_pwm_hi_normal << field->GetStartBit()) & field->GetMask();
    FIND_FIELD(reg,   field, "LW_PGC6_SCI_VID_CFG1_UPDATE");
    FIND_VALUE(field, value, "LW_PGC6_SCI_VID_CFG1_UPDATE_TRIGGER");
    reg_data |= value->GetValue() << field->GetStartBit();
    if (vid_cfg_is_array)
    {
        for (index = 0; index < limit; index = index + 1)
        {
            reg_addr = reg->GetAddress(index);
            lwgpu->RegWr32(reg_addr, reg_data);
            InfoPrintf("gc6plus: Writing %s(%d) = 0x%08x\n", reg->GetName(),
                index, reg_data);
        }
    }
    else
    {
        reg_addr = reg->GetAddress();
        lwgpu->RegWr32(reg_addr, reg_data);
        InfoPrintf("gc6plus: Writing %s = 0x%08x\n", reg->GetName(), reg_data);
    }

    //LDO Initialization
    FIND_REGISTER(m_regMap, reg, "LW_PGC6_SCI_LDO_CNTL");
    reg_addr = reg->GetAddress();
    FIND_FIELD(reg,   field, "LW_PGC6_SCI_LDO_CNTL_LWVDD_OK_LVL");
    reg_data = (m_lwvdd_ok_val << field->GetStartBit()) & field->GetMask();
    lwgpu->RegWr32(reg_addr,reg_data);


    // GPIO
    RETURN_IF_ERR(cfg_sci_gpio());

    // WAKEUP_TMR
    FIND_REGISTER(m_regMap, reg, "LW_PGC6_SCI_WAKEUP_TMR_CNTL");
    reg_addr = reg->GetAddress();
    FIND_FIELD(reg, field, "LW_PGC6_SCI_WAKEUP_TMR_CNTL_TIMER");
    reg_data = (wake_timer_us << field->GetStartBit()) & field->GetMask();
    lwgpu->RegWr32(reg_addr, reg_data);
    InfoPrintf("gc6plus: Writing %s = 0x%08x\n", reg->GetName(), reg_data);

    // GPU_I2CS_CFG
    RETURN_IF_ERR(cfg_sci_i2cs_snoop());

    // enable all wakeups
    InfoPrintf("gc6plus: Enabling all WAKE and SW_INTR enables\n");
    // WAKE_EN
    FIND_REGISTER(m_regMap, reg, "LW_PGC6_SCI_WAKE_EN");
    reg_addr = reg->GetAddress();
    reg_data = reg->GetWriteMask();
    lwgpu->RegWr32(reg_addr, reg_data);
    InfoPrintf("gc6plus: Writing %s = 0x%08x\n", reg->GetName(), reg_data);

    FIND_REGISTER(m_regMap, reg, "LW_PGC6_SCI_WAKE_RISE_EN");
    reg_addr = reg->GetAddress();
    reg_data = reg->GetWriteMask();
    lwgpu->RegWr32(reg_addr, reg_data);
    InfoPrintf("gc6plus: Writing %s = 0x%08x\n", reg->GetName(), reg_data);

    FIND_REGISTER(m_regMap, reg, "LW_PGC6_SCI_WAKE_FALL_EN");
    reg_addr = reg->GetAddress();
    reg_data = reg->GetWriteMask();
    lwgpu->RegWr32(reg_addr, reg_data);
    InfoPrintf("gc6plus: Writing %s = 0x%08x\n", reg->GetName(), reg_data);

    // SW_INTR_EN
    FIND_REGISTER(m_regMap, reg, "LW_PGC6_SCI_SW_INTR0_EN");
    reg_addr = reg->GetAddress();
    reg_data = reg->GetWriteMask();
    lwgpu->RegWr32(reg_addr, reg_data);
    InfoPrintf("gc6plus: Writing %s = 0x%08x\n", reg->GetName(), reg_data);

    FIND_REGISTER(m_regMap, reg, "LW_PGC6_SCI_SW_INTR1_RISE_EN");
    reg_addr = reg->GetAddress();
    reg_data = reg->GetWriteMask();
    lwgpu->RegWr32(reg_addr, reg_data);
    InfoPrintf("gc6plus: Writing %s = 0x%08x\n", reg->GetName(), reg_data);

    FIND_REGISTER(m_regMap, reg, "LW_PGC6_SCI_SW_INTR1_FALL_EN");
    reg_addr = reg->GetAddress();
    reg_data = reg->GetWriteMask();
    lwgpu->RegWr32(reg_addr, reg_data);
    InfoPrintf("gc6plus: Writing %s = 0x%08x\n", reg->GetName(), reg_data);

    // SW Force
    if (enab_sw_wake_timer)
    {
        // enable wakeup timer during GC5/GC6
        FIND_REGISTER(m_regMap, reg, "LW_PGC6_SCI_DEBUG_SW_INTR0_FRC");
        reg_addr = reg->GetAddress();
        FIND_FIELD(reg, field, "LW_PGC6_SCI_DEBUG_SW_INTR0_FRC_WAKE_TIMER");
        FIND_VALUE(field, value,
            "LW_PGC6_SCI_DEBUG_SW_INTR0_FRC_WAKE_TIMER_ENABLE");
        reg_data = (value->GetValue() << field->GetStartBit());
        lwgpu->RegWr32(reg_addr, reg_data);
        InfoPrintf("gc6plus: Writing %s = 0x%08x\n", reg->GetName(), reg_data);

        FIND_REGISTER(m_regMap, reg, "LW_PGC6_SCI_DEBUG_SW_INTR_CTRL");
        reg_addr = reg->GetAddress();
        FIND_FIELD(reg, field, "LW_PGC6_SCI_DEBUG_SW_INTR_CTRL_FORCE_DURING");
        FIND_VALUE(field, value,
            "LW_PGC6_SCI_DEBUG_SW_INTR_CTRL_FORCE_DURING_GC5_GC6");
        reg_data = (value->GetValue() << field->GetStartBit());
        lwgpu->RegWr32(reg_addr, reg_data);
        InfoPrintf("gc6plus: Writing %s = 0x%08x\n", reg->GetName(), reg_data);
    }

    return (0);
}

int
GC6Plus::cfg_sci_ptimer()
{
    unique_ptr<IRegisterClass> reg;
    unique_ptr<IRegisterField> field;
    unique_ptr<IRegisterValue> value;
    UINT32 reg_addr, reg_data;

    InfoPrintf("gc6plus: Initializing and starting SCI PTIMER\n");

    // Configure PTIMER to run on 27 MHz source clock
    FIND_REGISTER(m_regMap, reg, "LW_PGC6_SCI_PTIMER_CFG");
    reg_addr = reg->GetAddress();
    FIND_FIELD(reg,   field, "LW_PGC6_SCI_PTIMER_CFG_DENOMINATOR");
    FIND_VALUE(field, value, "LW_PGC6_SCI_PTIMER_CFG_DENOMINATOR_27MHZ_REF");
    reg_data = (value->GetValue() << field->GetStartBit());
    FIND_FIELD(reg,   field, "LW_PGC6_SCI_PTIMER_CFG_NUMERATOR");
    FIND_VALUE(field, value, "LW_PGC6_SCI_PTIMER_CFG_NUMERATOR_27MHZ_REF");
    reg_data |= (value->GetValue() << field->GetStartBit());
    FIND_FIELD(reg,   field, "LW_PGC6_SCI_PTIMER_CFG_INTEGER");
    FIND_VALUE(field, value, "LW_PGC6_SCI_PTIMER_CFG_INTEGER_27MHZ_REF");
    reg_data |= (value->GetValue() << field->GetStartBit());
    lwgpu->RegWr32(reg_addr, reg_data);
    InfoPrintf("gc6plus: Writing %s = 0x%08x\n", reg->GetName(), reg_data);

    // Copy arbitrary system time to PTIMER
    FIND_REGISTER(m_regMap, reg, "LW_PGC6_SCI_PTIMER_TIME_1");
    reg_addr = reg->GetAddress();
    FIND_FIELD(reg,   field, "LW_PGC6_SCI_PTIMER_TIME_1_NSEC");
    reg_data = (0x00001233 & field->GetMask());
    lwgpu->RegWr32(reg_addr, reg_data);
    InfoPrintf("gc6plus: Writing %s = 0x%08x\n", reg->GetName(), reg_data);
    FIND_REGISTER(m_regMap, reg, "LW_PGC6_SCI_PTIMER_TIME_0");
    reg_addr = reg->GetAddress();
    FIND_FIELD(reg,   field, "LW_PGC6_SCI_PTIMER_TIME_0_NSEC");
    reg_data = (0xffb3b4bf & field->GetMask());
    FIND_FIELD(reg,   field, "LW_PGC6_SCI_PTIMER_TIME_0_UPDATE");
    FIND_VALUE(field, value, "LW_PGC6_SCI_PTIMER_TIME_0_UPDATE_TRIGGER");
    reg_data |= (value->GetValue() << field->GetStartBit());
    lwgpu->RegWr32(reg_addr, reg_data);
    InfoPrintf("gc6plus: Writing %s = 0x%08x\n", reg->GetName(), reg_data);

    // Start PTIMER
    FIND_REGISTER(m_regMap, reg, "LW_PGC6_SCI_PTIMER_ENABLE");
    reg_addr = reg->GetAddress();
    FIND_FIELD(reg,   field, "LW_PGC6_SCI_PTIMER_ENABLE_CTL");
    FIND_VALUE(field, value, "LW_PGC6_SCI_PTIMER_ENABLE_CTL_YES");
    reg_data = (value->GetValue() << field->GetStartBit());
    lwgpu->RegWr32(reg_addr, reg_data);
    InfoPrintf("gc6plus: Writing %s = 0x%08x\n", reg->GetName(), reg_data);

    return (0);
}

int
GC6Plus::cfg_sci_power_sequencer()
{
    unsigned int index;
    UINT32 reg_addr, reg_data;
    // Sequence taken from //hw/lwgpu_gmlit1/manuals/gc6_island/pwr_seq_source/
    //   golden_sim_gc6plus_seq_default_sdr.txt
    // FIXME: This is a temporary hack until the flow to automatically select
    // and load the correct sequence(s) is implemented.
    const unsigned int golden_sim_gc6plus_seq_default_sdr_pwr_seq_cfg[][2] = {
        // Entry Vectors
        {0x0010e870, 0x0f021506},
        {0x0010e874, 0x0c001c0b},
        {0x0010e878, 0x00001f1d},
        // Sequencer Table
        {0x0010e880, 0x00001331},
        {0x0010e884, 0x00001b11},
        {0x0010e888, 0x00001321},
        {0x0010e88c, 0x00002009},
        {0x0010e890, 0x00001383},
        {0x0010e894, 0x00001a44},
        {0x0010e898, 0x00001343},
        {0x0010e89c, 0x00002011},
        {0x0010e8a0, 0x00001285},
        {0x0010e8a4, 0x00001221},
        {0x0010e8a8, 0x00001a11},
        {0x0010e8ac, 0x00000000},
        {0x0010e8b0, 0x00001331},
        {0x0010e8b4, 0x00001302},
        {0x0010e8b8, 0x00001b11},
        {0x0010e8bc, 0x00001321},
        {0x0010e8c0, 0x0000132f},
        {0x0010e8c4, 0x000013f2},
        {0x0010e8c8, 0x00001360},
        {0x0010e8cc, 0x00002000},
        {0x0010e8d0, 0x00001a44},
        {0x0010e8d4, 0x00002011},
        {0x0010e8d8, 0x00001243},
        {0x0010e8dc, 0x00001345},
        {0x0010e8e0, 0x00001360},
        {0x0010e8e4, 0x00001221},
        {0x0010e8e8, 0x00001211},
        {0x0010e8ec, 0x00001af2},
        {0x0010e8f0, 0x00000000},
        {0x0010e8f4, 0x00001211},
        {0x0010e8f8, 0x00001a01},
        {0x0010e8fc, 0x00001221},
        {0x0010e900, 0x00001211},
        {0x0010e904, 0x00001201},
        {0x0010e908, 0x00001a31},
        // Delay Table
        {0x0010e980, 0x00000000},
        {0x0010e984, 0x0000000a},
        {0x0010e988, 0x00000014},
        {0x0010e98c, 0x00000101},
        {0x0010e990, 0x0000010a},
        {0x0010e994, 0x00000114},
        {0x0010e998, 0x00000205},
        {0x0010e99c, 0x00000214},
        {0x0010e9a0, 0x00000301},
        {0x0010e9a4, 0x00000302},
        {0x0010e9a8, 0x000002dc},
    };

    // load
    InfoPrintf("gc6plus: Loading power sequencer\n");
    for (index = 0;
        index < sizeof (golden_sim_gc6plus_seq_default_sdr_pwr_seq_cfg) /
        sizeof (* golden_sim_gc6plus_seq_default_sdr_pwr_seq_cfg); index++)
    {
        reg_addr = golden_sim_gc6plus_seq_default_sdr_pwr_seq_cfg[index][0],
        reg_data = golden_sim_gc6plus_seq_default_sdr_pwr_seq_cfg[index][1];
        lwgpu->RegWr32(reg_addr, reg_data);
        //InfoPrintf("gc6plus: Writing 0x%08x = 0x%08x\n", reg_addr,
        //    reg_data);
    }

    return (0);
}

int
GC6Plus::cfg_sci_gpio()
{
    unique_ptr<IRegisterClass> reg;
    unique_ptr<IRegisterField> field;
    unique_ptr<IRegisterValue> value;
    const char * valuename;
    UINT32 reg_addr, reg_data;
    UINT32 index;

    // GPIO_OUTPUT
    InfoPrintf("gc6plus: Configuring SCI GPIO outputs\n");
    // gpio_index_main_3v3_en
    InfoPrintf("gc6plus: Configuring MAIN_3V3_EN output to GPIO %d\n",
        gpio_index_main_3v3_en);
    FIND_REGISTER(m_regMap, reg, "LW_PGC6_SCI_GPIO_OUTPUT_CNTL");
    reg_addr = reg->GetAddress(gpio_index_main_3v3_en);
    FIND_FIELD(reg, field, "LW_PGC6_SCI_GPIO_OUTPUT_CNTL_SEL");
    FIND_VALUE(field, value,
        "LW_PGC6_SCI_GPIO_OUTPUT_CNTL_SEL_SEQ_STATE_MAIN_3V3_EN");
    reg_data = (value->GetValue() << field->GetStartBit());
    FIND_FIELD(reg, field, "LW_PGC6_SCI_GPIO_OUTPUT_CNTL_DRIVE_MODE");
    FIND_VALUE(field, value,
        "LW_PGC6_SCI_GPIO_OUTPUT_CNTL_DRIVE_MODE_FULL_DRIVE");
    reg_data |= (value->GetValue() << field->GetStartBit());
    FIND_FIELD(reg, field, "LW_PGC6_SCI_GPIO_OUTPUT_CNTL_POLARITY");
    FIND_VALUE(field, value,
        "LW_PGC6_SCI_GPIO_OUTPUT_CNTL_POLARITY_ACTIVE_HIGH");
    reg_data |= (value->GetValue() << field->GetStartBit());
    lwgpu->RegWr32(reg_addr, reg_data);
    InfoPrintf("gc6plus: Writing %s(%d) = 0x%08x\n", reg->GetName(),
        gpio_index_main_3v3_en, reg_data);

    // gpio_index_fbvddq_gpu_en
    InfoPrintf("gc6plus: Configuring FBVDDQ_GPU_EN output to GPIO %d\n",
        gpio_index_fbvddq_gpu_en);
    FIND_REGISTER(m_regMap, reg, "LW_PGC6_SCI_GPIO_OUTPUT_CNTL");
    reg_addr = reg->GetAddress(gpio_index_fbvddq_gpu_en);
    FIND_FIELD(reg, field, "LW_PGC6_SCI_GPIO_OUTPUT_CNTL_SEL");
    FIND_VALUE(field, value,
        "LW_PGC6_SCI_GPIO_OUTPUT_CNTL_SEL_SEQ_STATE_FBVDDQ_GPU_EN");
    reg_data = (value->GetValue() << field->GetStartBit());
    FIND_FIELD(reg, field, "LW_PGC6_SCI_GPIO_OUTPUT_CNTL_DRIVE_MODE");
    FIND_VALUE(field, value,
        "LW_PGC6_SCI_GPIO_OUTPUT_CNTL_DRIVE_MODE_FULL_DRIVE");
    reg_data |= (value->GetValue() << field->GetStartBit());
    FIND_FIELD(reg, field, "LW_PGC6_SCI_GPIO_OUTPUT_CNTL_POLARITY");
    FIND_VALUE(field, value,
        "LW_PGC6_SCI_GPIO_OUTPUT_CNTL_POLARITY_ACTIVE_HIGH");
    reg_data |= (value->GetValue() << field->GetStartBit());
    lwgpu->RegWr32(reg_addr, reg_data);
    InfoPrintf("gc6plus: Writing %s(%d) = 0x%08x\n", reg->GetName(),
        gpio_index_fbvddq_gpu_en, reg_data);

    // gpio_index_pexvdd_vid
    InfoPrintf("gc6plus: Configuring PEXVDD_VID output to GPIO %d\n",
        gpio_index_pexvdd_vid);
    FIND_REGISTER(m_regMap, reg, "LW_PGC6_SCI_GPIO_OUTPUT_CNTL");
    reg_addr = reg->GetAddress(gpio_index_pexvdd_vid);
    FIND_FIELD(reg, field, "LW_PGC6_SCI_GPIO_OUTPUT_CNTL_SEL");
    FIND_VALUE(field, value,
        "LW_PGC6_SCI_GPIO_OUTPUT_CNTL_SEL_SEQ_STATE_PEXVDD_VID");
    reg_data = (value->GetValue() << field->GetStartBit());
    FIND_FIELD(reg, field, "LW_PGC6_SCI_GPIO_OUTPUT_CNTL_DRIVE_MODE");
    FIND_VALUE(field, value,
        "LW_PGC6_SCI_GPIO_OUTPUT_CNTL_DRIVE_MODE_FULL_DRIVE");
    reg_data |= (value->GetValue() << field->GetStartBit());
    FIND_FIELD(reg, field, "LW_PGC6_SCI_GPIO_OUTPUT_CNTL_POLARITY");
    FIND_VALUE(field, value,
        "LW_PGC6_SCI_GPIO_OUTPUT_CNTL_POLARITY_ACTIVE_HIGH");
    reg_data |= (value->GetValue() << field->GetStartBit());
    lwgpu->RegWr32(reg_addr, reg_data);
    InfoPrintf("gc6plus: Writing %s(%d) = 0x%08x\n", reg->GetName(),
        gpio_index_pexvdd_vid, reg_data);

    // gpio_index_vid_pwm
    InfoPrintf("gc6plus: Configuring VID_PWM output to GPIO %d\n",
        gpio_index_vid_pwm);
    FIND_REGISTER(m_regMap, reg, "LW_PGC6_SCI_GPIO_OUTPUT_CNTL");
    reg_addr = reg->GetAddress(gpio_index_vid_pwm);
    FIND_FIELD(reg, field, "LW_PGC6_SCI_GPIO_OUTPUT_CNTL_SEL");
    valuename = "LW_PGC6_SCI_GPIO_OUTPUT_CNTL_SEL_VID_PWM";
    value = field->FindValue(valuename);
    if (!value)
    {
        // see if alternate/alias name exists
        valuename = "LW_PGC6_SCI_GPIO_OUTPUT_CNTL_SEL_VID_PWM_0";
        value = field->FindValue(valuename);
        if (!value)
        {
            ErrPrintf("GC6Plus: Failed to find %s value\n", valuename);
            return (1);
        }
    }
    reg_data = (value->GetValue() << field->GetStartBit());
    FIND_FIELD(reg, field, "LW_PGC6_SCI_GPIO_OUTPUT_CNTL_DRIVE_MODE");
    FIND_VALUE(field, value,
        "LW_PGC6_SCI_GPIO_OUTPUT_CNTL_DRIVE_MODE_FULL_DRIVE");
    reg_data |= (value->GetValue() << field->GetStartBit());
    FIND_FIELD(reg, field, "LW_PGC6_SCI_GPIO_OUTPUT_CNTL_POLARITY");
    FIND_VALUE(field, value,
        "LW_PGC6_SCI_GPIO_OUTPUT_CNTL_POLARITY_ACTIVE_HIGH");
    reg_data |= (value->GetValue() << field->GetStartBit());
    lwgpu->RegWr32(reg_addr, reg_data);
    InfoPrintf("gc6plus: Writing %s(%d) = 0x%08x\n", reg->GetName(),
        gpio_index_vid_pwm, reg_data);

    // gpio_index_fsm_gpu_event - open drain, active low
    InfoPrintf("gc6plus: Configuring GPU_EVENT output to GPIO %d\n",
        gpio_index_fsm_gpu_event);
    FIND_REGISTER(m_regMap, reg, "LW_PGC6_SCI_GPIO_OUTPUT_CNTL");
    reg_addr = reg->GetAddress(gpio_index_fsm_gpu_event);
    FIND_FIELD(reg, field, "LW_PGC6_SCI_GPIO_OUTPUT_CNTL_SEL");
    FIND_VALUE(field, value, "LW_PGC6_SCI_GPIO_OUTPUT_CNTL_SEL_FSM_GPU_EVENT");
    reg_data = (value->GetValue() << field->GetStartBit());
    FIND_FIELD(reg, field, "LW_PGC6_SCI_GPIO_OUTPUT_CNTL_DRIVE_MODE");
    FIND_VALUE(field, value,
        "LW_PGC6_SCI_GPIO_OUTPUT_CNTL_DRIVE_MODE_OPEN_DRAIN");
    reg_data |= (value->GetValue() << field->GetStartBit());
    FIND_FIELD(reg, field, "LW_PGC6_SCI_GPIO_OUTPUT_CNTL_POLARITY");
    FIND_VALUE(field, value,
        "LW_PGC6_SCI_GPIO_OUTPUT_CNTL_POLARITY_ACTIVE_LOW");
    reg_data |= (value->GetValue() << field->GetStartBit());
    lwgpu->RegWr32(reg_addr, reg_data);
    InfoPrintf("gc6plus: Writing %s(%d) = 0x%08x\n", reg->GetName(),
        gpio_index_fsm_gpu_event, reg_data);

    // gpio_index_gpu_gc6_rst - active low
    InfoPrintf("gc6plus: Configuring GPU_GC6_RST output to GPIO %d\n",
        gpio_index_gpu_gc6_rst);
    FIND_REGISTER(m_regMap, reg, "LW_PGC6_SCI_GPIO_OUTPUT_CNTL");
    reg_addr = reg->GetAddress(gpio_index_gpu_gc6_rst);
    FIND_FIELD(reg, field, "LW_PGC6_SCI_GPIO_OUTPUT_CNTL_SEL");
    FIND_VALUE(field, value,
        "LW_PGC6_SCI_GPIO_OUTPUT_CNTL_SEL_SEQ_STATE_PEX_RST");
    reg_data = (value->GetValue() << field->GetStartBit());
    FIND_FIELD(reg, field, "LW_PGC6_SCI_GPIO_OUTPUT_CNTL_DRIVE_MODE");
    FIND_VALUE(field, value,
        "LW_PGC6_SCI_GPIO_OUTPUT_CNTL_DRIVE_MODE_FULL_DRIVE");
    reg_data |= (value->GetValue() << field->GetStartBit());
    FIND_FIELD(reg, field, "LW_PGC6_SCI_GPIO_OUTPUT_CNTL_POLARITY");
    FIND_VALUE(field, value,
        "LW_PGC6_SCI_GPIO_OUTPUT_CNTL_POLARITY_ACTIVE_LOW");
    reg_data |= (value->GetValue() << field->GetStartBit());
    lwgpu->RegWr32(reg_addr, reg_data);
    InfoPrintf("gc6plus: Writing %s(%d) = 0x%08x\n", reg->GetName(),
        gpio_index_gpu_gc6_rst, reg_data);


    // GPIO_INPUT
    InfoPrintf("gc6plus: Configuring SCI GPIO inputs\n");
    // hpd
    FIND_REGISTER(m_regMap, reg, "LW_PGC6_SCI_GPIO_INPUT_HPD");
    for (index = 0; index < 6; index++)
    {
        InfoPrintf("gc6plus: Configuring HPD %d input to GPIO %d\n",
            index, gpio_index_hpd[index]);
        reg_addr = reg->GetAddress(index);
        FIND_FIELD(reg, field, "LW_PGC6_SCI_GPIO_INPUT_HPD_SELECT");
        reg_data = (gpio_index_hpd[index] << field->GetStartBit()) &
            field->GetMask();
        FIND_FIELD(reg,   field, "LW_PGC6_SCI_GPIO_INPUT_HPD_ILWALID");
        FIND_VALUE(field, value, "LW_PGC6_SCI_GPIO_INPUT_HPD_ILWALID_NO");
        reg_data |= (value->GetValue() << field->GetStartBit());
        FIND_FIELD(reg,   field, "LW_PGC6_SCI_GPIO_INPUT_HPD_GLITCH_FILTER");
        FIND_VALUE(field, value,
            "LW_PGC6_SCI_GPIO_INPUT_HPD_GLITCH_FILTER_DISABLE");
        reg_data |= (value->GetValue() << field->GetStartBit());
        FIND_FIELD(reg,   field, "LW_PGC6_SCI_GPIO_INPUT_HPD_ILWERT");
        FIND_VALUE(field, value, "LW_PGC6_SCI_GPIO_INPUT_HPD_ILWERT_NO");
        reg_data |= (value->GetValue() << field->GetStartBit());
        lwgpu->RegWr32(reg_addr, reg_data);
        InfoPrintf("gc6plus: Writing %s(%d) = 0x%08x\n", reg->GetName(),
            index, reg_data);
    }

    // misc
    FIND_REGISTER(m_regMap, reg, "LW_PGC6_SCI_GPIO_INPUT_MISC");
    for (index = 0; index < 4; index++)
    {
        InfoPrintf("gc6plus: Configuring MISC %d input to GPIO %d\n",
            index, gpio_index_misc[index]);
        reg_addr = reg->GetAddress(index);
        FIND_FIELD(reg, field, "LW_PGC6_SCI_GPIO_INPUT_MISC_SELECT");
        reg_data = (gpio_index_misc[index] << field->GetStartBit()) &
            field->GetMask();
        FIND_FIELD(reg,   field, "LW_PGC6_SCI_GPIO_INPUT_MISC_ILWALID");
        FIND_VALUE(field, value, "LW_PGC6_SCI_GPIO_INPUT_MISC_ILWALID_NO");
        reg_data |= (value->GetValue() << field->GetStartBit());
        FIND_FIELD(reg,   field, "LW_PGC6_SCI_GPIO_INPUT_MISC_GLITCH_FILTER");
        FIND_VALUE(field, value,
            "LW_PGC6_SCI_GPIO_INPUT_MISC_GLITCH_FILTER_DISABLE");
        reg_data |= (value->GetValue() << field->GetStartBit());
        FIND_FIELD(reg,   field, "LW_PGC6_SCI_GPIO_INPUT_MISC_ILWERT");
        FIND_VALUE(field, value, "LW_PGC6_SCI_GPIO_INPUT_MISC_ILWERT_NO");
        reg_data |= (value->GetValue() << field->GetStartBit());
        lwgpu->RegWr32(reg_addr, reg_data);
        InfoPrintf("gc6plus: Writing %s(%d) = 0x%08x\n", reg->GetName(),
            index, reg_data);
    }

    // power_alert - active low
    InfoPrintf("gc6plus: Configuring POWER_ALERT input to GPIO %d\n",
        gpio_index_power_alert);
    FIND_REGISTER(m_regMap, reg, "LW_PGC6_SCI_GPIO_INPUT_POWER_ALERT");
    reg_addr = reg->GetAddress();
    FIND_FIELD(reg, field, "LW_PGC6_SCI_GPIO_INPUT_POWER_ALERT_SELECT");
    reg_data = (gpio_index_power_alert << field->GetStartBit()) &
        field->GetMask();
    FIND_FIELD(reg,   field, "LW_PGC6_SCI_GPIO_INPUT_POWER_ALERT_ILWALID");
    FIND_VALUE(field, value, "LW_PGC6_SCI_GPIO_INPUT_POWER_ALERT_ILWALID_NO");
    reg_data |= (value->GetValue() << field->GetStartBit());
    FIND_FIELD(reg, field, "LW_PGC6_SCI_GPIO_INPUT_POWER_ALERT_GLITCH_FILTER");
    FIND_VALUE(field, value,
        "LW_PGC6_SCI_GPIO_INPUT_POWER_ALERT_GLITCH_FILTER_DISABLE");
    reg_data |= (value->GetValue() << field->GetStartBit());
    FIND_FIELD(reg,   field, "LW_PGC6_SCI_GPIO_INPUT_POWER_ALERT_ILWERT");
    FIND_VALUE(field, value, "LW_PGC6_SCI_GPIO_INPUT_POWER_ALERT_ILWERT_YES");
    reg_data |= (value->GetValue() << field->GetStartBit());
    lwgpu->RegWr32(reg_addr, reg_data);
    InfoPrintf("gc6plus: Writing %s = 0x%08x\n", reg->GetName(), reg_data);

    // gpu_event - active low, bidirectional gpio
    InfoPrintf("gc6plus: Configuring GPU_EVENT input to GPIO %d\n",
        gpio_index_fsm_gpu_event);
    FIND_REGISTER(m_regMap, reg, "LW_PGC6_SCI_GPIO_INPUT_GPU_EVENT");
    reg_addr = reg->GetAddress();
    FIND_FIELD(reg, field, "LW_PGC6_SCI_GPIO_INPUT_GPU_EVENT_SELECT");
    reg_data = (gpio_index_fsm_gpu_event << field->GetStartBit()) &
        field->GetMask();
    FIND_FIELD(reg,   field, "LW_PGC6_SCI_GPIO_INPUT_GPU_EVENT_ILWALID");
    FIND_VALUE(field, value, "LW_PGC6_SCI_GPIO_INPUT_GPU_EVENT_ILWALID_NO");
    reg_data |= (value->GetValue() << field->GetStartBit());
    FIND_FIELD(reg, field, "LW_PGC6_SCI_GPIO_INPUT_GPU_EVENT_GLITCH_FILTER");
    FIND_VALUE(field, value,
        "LW_PGC6_SCI_GPIO_INPUT_GPU_EVENT_GLITCH_FILTER_DISABLE");
    reg_data |= (value->GetValue() << field->GetStartBit());
    FIND_FIELD(reg,   field, "LW_PGC6_SCI_GPIO_INPUT_GPU_EVENT_ILWERT");
    FIND_VALUE(field, value, "LW_PGC6_SCI_GPIO_INPUT_GPU_EVENT_ILWERT_YES");
    reg_data |= (value->GetValue() << field->GetStartBit());
    lwgpu->RegWr32(reg_addr, reg_data);
    InfoPrintf("gc6plus: Writing %s = 0x%08x\n", reg->GetName(), reg_data);

    // sys_pex_rst - active low
    InfoPrintf("gc6plus: Configuring SYS_RST_MON input to GPIO %d\n",
        gpio_index_sys_rst_mon);
    FIND_REGISTER(m_regMap, reg, "LW_PGC6_SCI_GPIO_INPUT_SYS_PEX_RST");
    reg_addr = reg->GetAddress();
    FIND_FIELD(reg, field, "LW_PGC6_SCI_GPIO_INPUT_SYS_PEX_RST_SELECT");
    reg_data = (gpio_index_sys_rst_mon << field->GetStartBit()) &
        field->GetMask();
    FIND_FIELD(reg,   field, "LW_PGC6_SCI_GPIO_INPUT_SYS_PEX_RST_ILWALID");
    FIND_VALUE(field, value, "LW_PGC6_SCI_GPIO_INPUT_SYS_PEX_RST_ILWALID_NO");
    reg_data |= (value->GetValue() << field->GetStartBit());
    FIND_FIELD(reg, field, "LW_PGC6_SCI_GPIO_INPUT_SYS_PEX_RST_GLITCH_FILTER");
    FIND_VALUE(field, value,
        "LW_PGC6_SCI_GPIO_INPUT_SYS_PEX_RST_GLITCH_FILTER_DISABLE");
    reg_data |= (value->GetValue() << field->GetStartBit());
    FIND_FIELD(reg,   field, "LW_PGC6_SCI_GPIO_INPUT_SYS_PEX_RST_ILWERT");
    FIND_VALUE(field, value, "LW_PGC6_SCI_GPIO_INPUT_SYS_PEX_RST_ILWERT_YES");
    reg_data |= (value->GetValue() << field->GetStartBit());
    lwgpu->RegWr32(reg_addr, reg_data);
    InfoPrintf("gc6plus: Writing %s = 0x%08x\n", reg->GetName(), reg_data);

    return (0);
}

int
GC6Plus::cfg_sci_i2cs_snoop()
{
    unique_ptr<IRegisterClass> reg;
    unique_ptr<IRegisterField> field;
    unique_ptr<IRegisterValue> value;
    UINT32 reg_addr, reg_data;
    UINT32 max_scl_timeout_value, scl_timeout_value, scl_timeout_scale;

    InfoPrintf("gc6plus: Configuring SCI I2CS Snoop\n");

    // Ilwalidate I2C and ARP address for now.  They will be configured by
    // PMU ucode just before GC5/GC6 entry.
    FIND_REGISTER(m_regMap, reg, "LW_PGC6_SCI_GPU_I2CS_CFG");
    reg_addr = reg->GetAddress();
    FIND_FIELD(reg, field, "LW_PGC6_SCI_GPU_I2CS_CFG_I2C_ADDR_VALID");
    FIND_VALUE(field, value, "LW_PGC6_SCI_GPU_I2CS_CFG_I2C_ADDR_VALID_NO");
    reg_data = (value->GetValue() << field->GetStartBit());
    FIND_FIELD(reg, field, "LW_PGC6_SCI_GPU_I2CS_CFG_ARP_ADDR_VALID");
    FIND_VALUE(field, value, "LW_PGC6_SCI_GPU_I2CS_CFG_ARP_ADDR_VALID_NO");
    reg_data |= (value->GetValue() << field->GetStartBit());
    FIND_FIELD(reg, field, "LW_PGC6_SCI_GPU_I2CS_CFG_ILWALIDATE_THERM_IN_GC5");
    FIND_VALUE(field, value,
        "LW_PGC6_SCI_GPU_I2CS_CFG_ILWALIDATE_THERM_IN_GC5_YES");
    reg_data |= (value->GetValue() << field->GetStartBit());

    FIND_FIELD(reg, field, "LW_PGC6_SCI_GPU_I2CS_CFG_SCL_TIMEOUT_VALUE");
    // scale SCL timeout down to correct scale
    max_scl_timeout_value = (field->GetMask() >> field->GetStartBit());
    scl_timeout_value = i2c_snoop_scl_timeout_us;
    scl_timeout_scale = 1;  // in us
    while (scl_timeout_value > max_scl_timeout_value)
    {
        scl_timeout_value = scl_timeout_value / 10;
        scl_timeout_scale = scl_timeout_scale * 10;
    }
    if (scl_timeout_scale > 1000)
    {
        InfoPrintf("gc6plus: i2c_snoop_scl_timeout_us (%d) is too "
            "large.  Truncating to max allowable value.\n",
            i2c_snoop_scl_timeout_us);
            scl_timeout_value = max_scl_timeout_value;
            scl_timeout_scale = 1000;
    }
    InfoPrintf("gc6plus: Programming i2c_snoop_scl_timeout_us to %d\n",
        scl_timeout_value * scl_timeout_scale);
    reg_data |= (scl_timeout_value << field->GetStartBit()) &
        field->GetMask();
    FIND_FIELD(reg, field, "LW_PGC6_SCI_GPU_I2CS_CFG_SCL_TIMEOUT_SCALE");
    switch (scl_timeout_scale)
    {
        case 1 :
            FIND_VALUE(field, value,
                "LW_PGC6_SCI_GPU_I2CS_CFG_SCL_TIMEOUT_SCALE_1USEC");
            break;
        case 10 :
            FIND_VALUE(field, value,
                "LW_PGC6_SCI_GPU_I2CS_CFG_SCL_TIMEOUT_SCALE_10USEC");
            break;
        case 100 :
            FIND_VALUE(field, value,
                "LW_PGC6_SCI_GPU_I2CS_CFG_SCL_TIMEOUT_SCALE_100USEC");
            break;
        case 1000 :
            FIND_VALUE(field, value,
                "LW_PGC6_SCI_GPU_I2CS_CFG_SCL_TIMEOUT_SCALE_1MSEC");
            break;
        default :
            ErrPrintf("gc6plus: Error in scl_timescale_value (%d), "
                "not a power of 10\n", scl_timeout_scale);
            return (1);
    }
    reg_data |= (value->GetValue() << field->GetStartBit());
    FIND_FIELD(reg, field, "LW_PGC6_SCI_GPU_I2CS_CFG_REPLAY_SPEED");
    FIND_VALUE(field, value, "LW_PGC6_SCI_GPU_I2CS_CFG_REPLAY_SPEED_SLOWEST");
    reg_data |= (value->GetValue() << field->GetStartBit());
    lwgpu->RegWr32(reg_addr, reg_data);
    InfoPrintf("gc6plus: Writing %s = 0x%08x\n", reg->GetName(), reg_data);

    return (0);
}

int
GC6Plus::prepare_for_entry()
{
    UINT32 check_data = 0;

    InfoPrintf("gc6plus: Preparing for GC5/GC6 entry\n");

    // turn off clocks
    // FIXME:

    // prepare PMU
    // FIXME:

    // check SW_INTR regs
    // We should have no pending interrupts prior to entry
    InfoPrintf("gc6plus: Checking SCI SW INTR Status prior to entry\n");
    RETURN_IF_ERR(check_priv("LW_PGC6_SCI_SW_INTR0_STATUS_LWRR",
        0x00000000, 0xffffffff));
    RETURN_IF_ERR(check_priv("LW_PGC6_SCI_SW_INTR1_STATUS_LWRR",
        0x00000000, 0xffffffff));
    RETURN_IF_ERR(check_priv("LW_PGC6_SCI_SW_INTR0_STATUS_PREV",
        0x00000000, 0xffffffff));
    RETURN_IF_ERR(check_priv("LW_PGC6_SCI_SW_INTR1_STATUS_PREV",
        0x00000000, 0xffffffff));

    // check and disable PEX
    // FIXME: This should be more than just an EscapeWrite
    if (m_mode == GC6)
    {
        InfoPrintf("gc6plus: Disabling PEX in preparation for GC6 entry\n");
        Platform::EscapeWrite("gc6plus_pcie_link_escape_hi", 0, 1, 1);
        Platform::EscapeRead("gc6plus_pcie_link_escape_hi", 0, 1, &check_data);
        if (check_data != 1)
        {
            ErrPrintf("gc6plus: Failure in EscapeWrite of gc6plus_"
                "pcie_link_escape_hi. Wrote %d, Read %d\n", 1, check_data);
            m_ErrCount++;
        }
    }

    // Pre-trigger PMU to start the entry process.  PMU will prepare everything
    // but stop just before the actual entry trigger.  gpio_index_pmu_comm
    // will be asserted by PMU to indicate when PMU is ready for trigger.  Note
    // that there should be no more PRIV accesses after the pre-trigger to PMU.
    switch (m_mode)
    {
        case GC5 :
            InfoPrintf("gc6plus: Pre-triggering PMU for GC5 entry\n");
            RETURN_IF_ERR(set_pmu_command(GC5_ENTRY));
            break;
        case GC6 :
            InfoPrintf("gc6plus: Pre-triggering PMU for GC6 entry\n");
            RETURN_IF_ERR(set_pmu_command(GC6_ENTRY));
            break;
        default :
            return (0);
            break;
    }

    // Poll gpio_index_pmu_comm until the PMU is ready to trigger entry.
    InfoPrintf("gc6plus: Polling for PMU readiness for final entry trigger\n");
    RETURN_IF_ERR(poll_gpio_state(gpio_index_pmu_comm, GPIO_RD_HIGH, true,
        m_polling_timeout_us));

    // short delay to allow PMU to ramp up on polling the GPIO
    Platform::Delay(4);  // delay 4 microsecond

    return (0);
}

int
GC6Plus::check_priv(const char *regname, UINT32 exp_data, UINT32 mask)
{
    unique_ptr<IRegisterClass> reg;
    UINT32 reg_addr, reg_data;

    // find and read register
    FIND_REGISTER(m_regMap, reg, regname);
    reg_addr = reg->GetAddress();
    reg_data = lwgpu->RegRd32(reg_addr);
    InfoPrintf("gc6plus: Reading %s = 0x%08x\n", reg->GetName(), reg_data);

    // Compare against expected value, increment error count if mismatch
    if ((reg_data & mask) != (exp_data & mask))
    {
        ErrPrintf("gc6plus: Mismatch in %s: exp_data = 0x%08x, act_data = 0x"
            "%08x, mask = 0x%08x\n", reg->GetName(), exp_data, reg_data, mask);
        m_ErrCount++;
    }
    return (0);
}


int
GC6Plus::generate_events()
{
    unique_ptr<IRegisterClass> reg;
    unique_ptr<IRegisterField> field;
    const char *regname, *fieldname;
    UINT32 reg_data;
    UINT32 abort_lwtoff, elapse_time, time_adj, max_event_time, check_data,
        reset_data;
    bool gpu_event_assert, last_gpu_event_assert, ec_gpu_event_assert,
        pex_link_up, polling_done, poll_for_gpu_event_done;
    UINT32 trigger_latency = 5;
    UINT32 therm_i2c_latency = 13;

    // Compute the first wake event after the trigger.  This is used
    // later on during the register check phase.
    // Note: Wakeup timer is based on actual trigger of GC5/GC6 entry while
    // other wakups (ec_gpu_event, misc0, etc.) are generated by the test.
    // The test cannot sync precisely with the actual trigger and appears to
    // be a few clocks ahead of the trigger.  Therefore, in the case of a tie
    // between wakeup timer and external wake events, the wakeup timer will be
    // masked.
    first_wake_event_us = wake_timer_us;
    first_wake_event_is_wakeup_tmr = true;
    first_wake_event_is_therm_i2c = false;
    if (gen_wake_therm_i2c && ((wake_therm_i2c_delay -
        trigger_delay_us) <= first_wake_event_us))
    {
        first_wake_event_us = (wake_therm_i2c_delay - trigger_delay_us);
        first_wake_event_is_wakeup_tmr = false;
        first_wake_event_is_therm_i2c = true;
    }
    if (gen_wake_ec_gpu_event && ((wake_ec_gpu_event_delay -
        trigger_delay_us) <= first_wake_event_us))
    {
        first_wake_event_us = (wake_ec_gpu_event_delay - trigger_delay_us);
        first_wake_event_is_wakeup_tmr = false;
        first_wake_event_is_therm_i2c = false;
    }
    if (gen_wake_gpio_misc_0 && ((wake_gpio_misc_0_delay -
        trigger_delay_us) <= first_wake_event_us))
    {
        first_wake_event_us = (wake_gpio_misc_0_delay - trigger_delay_us);
        first_wake_event_is_wakeup_tmr = false;
        first_wake_event_is_therm_i2c = false;
    }
    if (gen_wake_rxstat && ((wake_rxstat_delay -
        trigger_delay_us) <= first_wake_event_us))
    {
        first_wake_event_us = (wake_rxstat_delay - trigger_delay_us);
        first_wake_event_is_wakeup_tmr = false;
        first_wake_event_is_therm_i2c = false;
    }
    if (gen_wake_clkreq && ((wake_clkreq_delay -
        trigger_delay_us) <= first_wake_event_us))
    {
        first_wake_event_us = (wake_clkreq_delay - trigger_delay_us);
        first_wake_event_is_wakeup_tmr = false;
        first_wake_event_is_therm_i2c = false;
    }
    InfoPrintf("gc6plus: Computed time of first wake event = %d us\n",
        first_wake_event_us);


    // Callwlate the cutoff time for ABORT.  This is different between GC5
    // and GC6.  In GC5, the ABORT cutoff is based on
    // SCI_TMR_GC5_ENTRY_HOLDOFF.
    if (m_mode == GC5)
    {
        if (cnf_sci_entry_holdoff)
        {
            // overrided by a test arg
            abort_lwtoff = sci_entry_holdoff_us;
        }
        else
        {
            // Get the HW init value
            regname = "LW_PGC6_SCI_TMR_GC5_ENTRY_HOLDOFF";
            if (!(reg = m_regMap->FindRegister(regname)))
            {
                ErrPrintf("gc6plus: Failed to find %s register\n", regname);
                m_ErrCount++;
            }
            fieldname = "LW_PGC6_SCI_TMR_GC5_ENTRY_HOLDOFF_TIMEOUT";
            if (!(field = reg->FindField(fieldname)))
            {
                ErrPrintf("gc6plus: Failed to find %s field\n", fieldname);
                m_ErrCount++;
            }
            // Use the HW init value if it has one
            if (!reg->GetHWInitValue(&reg_data))
            {
                // No HW init value, flag an error and use a hardcoded default
                ErrPrintf("gc6plus: Field %s has no HW Init value\n",fieldname);
                m_ErrCount++;
                abort_lwtoff = 1;
            }
            else
            {
                // get the value of the appropriate field
                abort_lwtoff = (reg_data & field->GetMask()) >>
                    field->GetStartBit();
            }
        }
    }
    // GC6 ABORT cutoff is just the GC6_ENTRY_START1 state.
    // It would include the GC6_ENTRY_START2 state if SDR is disabled and
    // the EC does not assert PEX reset, but this test is written to always
    // and immediately assert PEX reset when SDR is disabled.  Future
    // enhancements to the EC code may support GPU_EVENT entry response errors
    // and the abort cutoff will need to be updated.
    else
    {
        abort_lwtoff = 0;
    }
    InfoPrintf("gc6plus: Abort cutoff time = %d\n", abort_lwtoff);

    // any delay to the entry trigger increases the window for ABORT
    abort_lwtoff += trigger_delay_us;


    // Now that we have the time of the first wake event and the ABORT cutoff
    // time, we can predict the expected exit results (RESET, ABORT, EXIT).
    // This will be used to check the status registers after exit.
    gc6_abort_skipped = false;
    if (gen_ec_reset)
    {
        // Note that RESET trumps everything.
        InfoPrintf("gc6plus: Expected exit is RESET\n");
        expected_exit = RESET;
    }
    else if (first_wake_event_us <= abort_lwtoff)
    {
        if ((m_mode == GC6) && !enab_gc6_abort)
        {
            // GC6 is always EXIT when SKIP_GC6_ABORT is enabled
            InfoPrintf("gc6plus: Expected exit is GC6 EXIT "
                "(wanted GC6 ABORT, but ABORT not supported)\n");
            expected_exit = EXIT;
            gc6_abort_skipped = true;
        }
        else
        {
            InfoPrintf("gc6plus: Expected exit is %s ABORT\n",
                (m_mode == GC6) ? "GC6" : "GC5");
            expected_exit = ABORT;
        }
    }
    else
    {
        InfoPrintf("gc6plus: Expected exit is %s EXIT\n",
            (m_mode == GC6) ? "GC6" : "GC5");
        expected_exit = EXIT;
    }

    // Go through all the trigger and events to determine the max time that
    // this routine is expected to perform some action.  The trigger delay
    // is performed through PMU (through GPIO signalling) and has
    // some latency before the actual trigger takes effect.  We must not
    // exit this subroutine until we are guaranteed to have left the STEADY
    // state.  This is accomplished by adding addition time to the max
    // event time after the trigger.
    max_event_time = 0;
    // Include a 2 us buffer to deassert GPIO after triggering.
    if ((trigger_delay_us + 2) > max_event_time)
    {
        max_event_time = trigger_delay_us + 2;
    }
    if (gen_ec_reset && ((ec_reset_delay + m_ec_reset_width_us) >
        max_event_time))
    {
        max_event_time = (ec_reset_delay + m_ec_reset_width_us);
    }
    if (gen_wake_ec_gpu_event && ((wake_ec_gpu_event_delay +
        m_ec_gpu_event_width_us) > max_event_time))
    {
        max_event_time = (wake_ec_gpu_event_delay + m_ec_gpu_event_width_us);
    }
    if (gen_wake_gpio_misc_0 && (wake_gpio_misc_0_delay > max_event_time))
    {
        max_event_time = wake_gpio_misc_0_delay;
    }
    if (gen_wake_rxstat && (wake_rxstat_delay > max_event_time))
    {
        max_event_time = wake_rxstat_delay;
    }
    if (gen_wake_clkreq && (wake_clkreq_delay > max_event_time))
    {
        max_event_time = wake_clkreq_delay;
    }
    // There is 13.xxx us latency from the I2C start and I2C wake event.
    // We need to deassert WE control after 2 us of generating the I2C start.
    if (gen_wake_therm_i2c && ((wake_therm_i2c_delay - 11) > max_event_time))
    {
        max_event_time = (wake_therm_i2c_delay - 11);
    }
    if (gen_i2c_sci_priv_rd_SCRATCH && (i2c_sci_priv_rd_SCRATCH_delay >
        max_event_time))
    {
        max_event_time = i2c_sci_priv_rd_SCRATCH_delay;
    }
    if (gen_sr_update && ((sr_update_delay + m_ec_gpu_event_width_us) >
        max_event_time))
    {
        max_event_time = (sr_update_delay + m_ec_gpu_event_width_us);
    }


    // Start the elapse timer at 0 and increment every 1 us until we
    // reach the max_event_timer.  At each interval, perform checks for any
    // event that is needed.
    // Note that some events (entry trigger and THERM I2C) has some latency
    // before the event actually oclwrs.  This has the appearance that these
    // events must be triggered before time 0.  We will add a time adjustment
    // to delay other events so that entry trigger and THERM I2C can be
    // "triggered" in the past,
    elapse_time = 0;
    time_adj = 0;
    if ((trigger_delay_us + time_adj) < trigger_latency)
    {
        time_adj = trigger_latency - trigger_delay_us;
    }
    if (gen_wake_therm_i2c & ((wake_therm_i2c_delay + time_adj) <
        therm_i2c_latency))
    {
        time_adj = therm_i2c_latency - wake_therm_i2c_delay;
    }
    gpu_event_assert = false;
    last_gpu_event_assert = false;
    ec_gpu_event_assert = true;
    // For GC6, the EC polls GPU_EVENT to either 1) wake PEX link, or
    // 2) toggle PEX reset.
    if (m_mode == GC6)
    {
        poll_for_gpu_event_done = false;
    }
    else
    {
        poll_for_gpu_event_done = true;
    }
    polling_done = false;
    while (!polling_done && (elapse_time < m_polling_timeout_us))
    {
        InfoPrintf("gc6plus: Event Generation Time = %d of %d\n",
            (elapse_time - time_adj), max_event_time);
        // check for event action
        // Trigger GC5/GC6 entry
        if ((trigger_delay_us + time_adj) == (elapse_time + trigger_latency))
        {
            InfoPrintf("gc6plus: Synchronizing PMU entry trigger\n");
            RETURN_IF_ERR(set_gpio_state(gpio_index_pmu_comm, 0));
        }
        // Deassert GC5/GC6 entry trigger after 2 us
        if ((trigger_delay_us + 2 + time_adj) == (elapse_time +
            trigger_latency))
        {
            RETURN_IF_ERR(set_gpio_state(gpio_index_pmu_comm, 1));
        }

        // assert EC reset
        if (gen_ec_reset && ((ec_reset_delay + time_adj) == elapse_time))
        {
            InfoPrintf("gc6plus: Asserting EC system_reset\n");
            Platform::EscapeWrite("gc6plus_ec_pex_reset_", 0, 1, 0);
            Platform::EscapeRead("gc6plus_ec_pex_reset_", 0, 1, &check_data);
            if (check_data != 0)
            {
                ErrPrintf("gc6plus: Failure in EscapeWrite of gc6plus_"
                    "ec_pex_reset_.  Wrote %d, Read %d\n", 1, check_data);
                m_ErrCount++;
            }
            // If SCI has the system_reset monitor disabled, then we need to
            // generate a GPU_EVENT wake along with the reset.
            if (!enab_system_reset_mon)
            {
                InfoPrintf("gc6plus: Asserting EC GPU_EVENT\n");
                ec_gpu_event_assert = true;
                Platform::EscapeWrite("gc6plus_ec_gpu_event_", 0, 1, 0);
                Platform::EscapeRead("gc6plus_ec_gpu_event_", 0, 1,
                    &check_data);
                if (check_data != 0)
                {
                    ErrPrintf("gc6plus: Failure in EscapeWrite of "
                        "gc6plus_ec_gpu_event_.  Wrote %d, Read %d\n", 1,
                        check_data);
                    m_ErrCount++;
                }
            }
        }

        // deassert EC reset
        if (gen_ec_reset && ((ec_reset_delay + m_ec_reset_width_us + time_adj)
            == elapse_time))
        {
            InfoPrintf("gc6plus: Deasserting EC system_reset\n");
            Platform::EscapeWrite("gc6plus_ec_pex_reset_", 0, 1, 1);
            Platform::EscapeRead("gc6plus_ec_pex_reset_", 0, 1, &check_data);
            if (check_data != 1)
            {
                ErrPrintf("gc6plus: Failure in EscapeWrite of gc6plus_"
                    "ec_pex_reset_.  Wrote %d, Read %d\n", 1, check_data);
                m_ErrCount++;
            }
            // If SCI has the system_reset monitor disabled, then we need to
            // release GPU_EVENT wake along with the reset.
            if (!enab_system_reset_mon)
            {
                InfoPrintf("gc6plus: Deaserting EC GPU_EVENT\n");
                ec_gpu_event_assert = false;
                Platform::EscapeWrite("gc6plus_ec_gpu_event_", 0, 1, 1);
                Platform::EscapeRead("gc6plus_ec_gpu_event_", 0, 1,
                    &check_data);
                if (check_data != 1)
                {
                    ErrPrintf("gc6plus: Failure in EscapeWrite of "
                        "gc6plus_ec_gpu_event_.  Wrote %d, Read %d\n", 1,
                        check_data);
                    m_ErrCount++;
                }
            }
        }

        // assert EC GPU_EVENT
        if (gen_wake_ec_gpu_event && ((wake_ec_gpu_event_delay + time_adj)
            == elapse_time))
        {
            InfoPrintf("gc6plus: Asserting EC GPU_EVENT\n");
            ec_gpu_event_assert = true;
            Platform::EscapeWrite("gc6plus_ec_gpu_event_", 0, 1, 0);
            Platform::EscapeRead("gc6plus_ec_gpu_event_", 0, 1, &check_data);
            if (check_data != 0)
            {
                ErrPrintf("gc6plus: Failure in EscapeWrite of gc6plus_"
                    "ec_gpu_event_.  Wrote %d, Read %d\n", 1, check_data);
                m_ErrCount++;
            }
        }

        // deassert EC GPU_EVENT
        if (gen_wake_ec_gpu_event && ((wake_ec_gpu_event_delay +
            m_ec_gpu_event_width_us + time_adj) == elapse_time))
        {
            InfoPrintf("gc6plus: Deasserting EC GPU_EVENT\n");
            ec_gpu_event_assert = false;
            Platform::EscapeWrite("gc6plus_ec_gpu_event_", 0, 1, 1);
            Platform::EscapeRead("gc6plus_ec_gpu_event_", 0, 1, &check_data);
            if (check_data != 1)
            {
                ErrPrintf("gc6plus: Failure in EscapeWrite of gc6plus_"
                    "ec_gpu_event_.  Wrote %d, Read %d\n", 1, check_data);
                m_ErrCount++;
            }
        }

        // assert MISC0 GPIO
        if (gen_wake_gpio_misc_0 && ((wake_gpio_misc_0_delay + time_adj)
            == elapse_time))
        {
            InfoPrintf("gc6plus: Asserting MISC[0] GPIO\n");
            RETURN_IF_ERR(set_gpio_state(gpio_index_misc[0], 1));
        }

        // deassert RXSTAT_IDLE
        if (gen_wake_rxstat && ((wake_rxstat_delay + time_adj) == elapse_time))
        {
            InfoPrintf("gc6plus: Deasserting RXSTAT_IDLE\n");
            // FIXME
            ErrPrintf("gc6plus: RXSTAT_IDLE event not implemented\n");
            m_ErrCount++;
        }

        // assert CLKREQ
        if (gen_wake_clkreq && ((wake_clkreq_delay + time_adj) == elapse_time))
        {
            InfoPrintf("gc6plus: Asserting CLKREQ\n");
            // FIXME
            ErrPrintf("gc6plus: CLKREQ event not implemented\n");
            m_ErrCount++;
        }

        // generate THERM I2C
        if (gen_wake_therm_i2c && ((wake_therm_i2c_delay + time_adj) ==
            (elapse_time + therm_i2c_latency)))
        {
            InfoPrintf("gc6plus: Generating THERM I2C transaction\n");
            // Generate a byte read with command 0xFE to THERM
            Platform::EscapeWrite ("sm_dev_addr", 0, 7, therm_dev_addr);
            Platform::EscapeWrite ("sm_dev_cmd",  0, 8, 0xFE);
            Platform::EscapeWrite ("sm_ctrl_cmd", 0, 4, READ_BYTE);
            Platform::EscapeWrite ("sm_ctrl_we",  0, 1, 1);
        }
        // Deassert sm_ctrl_we after 2 us
        if (gen_wake_therm_i2c && ((wake_therm_i2c_delay + 2 + time_adj) ==
            (elapse_time + therm_i2c_latency)))
        {
            Platform::EscapeWrite ("sm_ctrl_we", 0, 1, 0);
        }

        // generate SCI I2C
        if (gen_i2c_sci_priv_rd_SCRATCH && ((i2c_sci_priv_rd_SCRATCH_delay +
            time_adj) == elapse_time))
        {
            InfoPrintf("gc6plus: Generating SCI I2C transaction\n");
            // FIXME
            ErrPrintf("gc6plus: SCI I2C event not implemented\n");
            m_ErrCount++;
        }

        // assert EC SR_UPDATE
        if (gen_sr_update && ((sr_update_delay + time_adj) == elapse_time))
        {
            InfoPrintf("gc6plus: Asserting EC Self-Refresh Update\n");
            ec_gpu_event_assert = true;
            Platform::EscapeWrite("gc6plus_ec_gpu_event_", 0, 1, 0);
            Platform::EscapeRead("gc6plus_ec_gpu_event_", 0, 1, &check_data);
            if (check_data != 0)
            {
                ErrPrintf("gc6plus: Failure in EscapeWrite of gc6plus_"
                    "ec_gpu_event_. Wrote %d, Read %d\n", 1, check_data);
                m_ErrCount++;
            }
        }

        // deassert EC SR_UPDATE
        if (gen_sr_update && ((sr_update_delay + m_ec_gpu_event_width_us +
            time_adj) == elapse_time))
        {
            InfoPrintf("gc6plus: Deasserting EC Self-Refresh Update\n");
            ec_gpu_event_assert = false;
            Platform::EscapeWrite("gc6plus_ec_gpu_event_", 0, 1, 1);
            Platform::EscapeRead("gc6plus_ec_gpu_event_", 0, 1, &check_data);
            if (check_data != 1)
            {
                ErrPrintf("gc6plus: Failure in EscapeWrite of gc6plus_"
                    "ec_gpu_event_. Wrote %d, Read %d\n", 1, check_data);
                m_ErrCount++;
            }
        }

        // Poll for SCI asserting GPU_EVENT, toggle reset in response
        // GPU_EVENT is active low
        if (!poll_for_gpu_event_done)
        {
            gpu_event_assert = (get_gpio_state(gpio_index_fsm_gpu_event) ==
                GPIO_RD_LOW);
        }

        // Handshake for PEX Reset toggle
        if (disable_sdr_mode)
        {

            // Toggle GC6 reset if SCI is asserting GPU_EVENT
            if (gpu_event_assert && !last_gpu_event_assert &&
                !ec_gpu_event_assert)
            {
                InfoPrintf("gc6plus: Detected SCI GPU_EVENT assert, "
                    "toggling PEX_RESET in response\n");
                Platform::EscapeRead("gc6plus_ec_pex_reset_", 0, 1,
                    &check_data);
                reset_data = check_data ^ 0x1;
                Platform::EscapeWrite("gc6plus_ec_pex_reset_", 0, 1,
                    reset_data);
                Platform::EscapeRead("gc6plus_ec_pex_reset_", 0, 1,
                    &check_data);
                if (check_data != reset_data)
                {
                    ErrPrintf("gc6plus: Failure in EscapeWrite of "
                        "gc6plus_ec_pex_reset_.  Wrote %d, Read %d\n",
                        reset_data, check_data);
                    m_ErrCount++;
                }
                // Wake up PEX link if we are deasserting PEX reset
                if (reset_data == 1)
                {
                    // Link needs to be enabled when the EC deasserts PEX reset.
                    // Is this the proper way to reenable the PEX link?
                    InfoPrintf("gc6plus: Enabling PEX after GC6 exit\n");
                    Platform::EscapeWrite("gc6plus_pcie_link_escape_hi", 0, 1,
                        0);
                    Platform::EscapeWrite("gc6plus_pcie_link_escape_lo", 0, 1,
                        1);
                    poll_for_gpu_event_done = true;
                }
            }
        }
        else
        {
            // Wake PEX link when GPU_EVENT asserts
            if (gpu_event_assert && !last_gpu_event_assert)
            {
                InfoPrintf("gc6plus: Detected GPU_EVENT assert, "
                    "checking status of PEX link\n");
                Platform::EscapeRead("gc6plus_pcie_link_escape_hi", 0, 1,
                    &check_data);
                // link is down when escape_hi is 1
                pex_link_up = (check_data != 1);
                InfoPrintf("gc6plus: PEX link is %0s\n",
                    pex_link_up ? "up" : "down");
                if (!pex_link_up)
                {
                    // Wake the PEX link
                    // Is this the proper way to reenable the PEX link?
                    InfoPrintf("gc6plus: Enabling PEX due to GPU_EVENT "
                       "assert\n");
                    Platform::EscapeWrite("gc6plus_pcie_link_escape_hi", 0, 1,
                        0);
                    Platform::EscapeWrite("gc6plus_pcie_link_escape_lo", 0, 1,
                        1);
                }
                poll_for_gpu_event_done = true;
            }
        }

        last_gpu_event_assert = gpu_event_assert;
        Platform::Delay(1);  // delay 1 microsecond
        elapse_time++;
        polling_done = (elapse_time > (max_event_time + time_adj)) &&
            poll_for_gpu_event_done;
    }

    if (!polling_done)
    {
        ErrPrintf("gc6plus: Timeout polling for Event generation\n");
        m_ErrCount++;
    }

    return (0);
}

int
GC6Plus::cleanup_after_exit()
{
    InfoPrintf("gc6plus: Cleaning up after GC5/GC6 exit\n");

    // FIXME: The PMU should be doing the cleanup from the inside out.
    // There should be no PRIV accesses until after PMU completes.  The
    // code below that cleans up using PRIV is a HACK.

    // Restore GPU if it has been reset
    if (gen_ec_reset)
    {
        // Retore PCIE config space
        RETURN_IF_ERR(restore_pcie());
        // Restore PRIV ring
        RETURN_IF_ERR(InitPrivRing());
        // Restore THERM
        RETURN_IF_ERR(cfg_therm());
        // Restore PMGR
        RETURN_IF_ERR(cfg_pmgr());
    }
    else
    {
        // Wait until PMU indicates it has completed the exit routine
        InfoPrintf("gc6plus: Polling for PMU exit complete\n");
        RETURN_IF_ERR(poll_gpio_state(gpio_index_pmu_comm, GPIO_RD_LOW,
            true, m_polling_timeout_us));
    }

    // wait until PMU Ucode is done with its cleanup before continuing the test
    if (poll_pmu_command(PMU_NOOP, true, m_polling_timeout_us))
    {
        ErrPrintf("gc6plus: Error in polling for PMU Ucode command status\n");
        m_ErrCount++;
    }
    // copy over PMU error count and clear it
    RETURN_IF_ERR(check_pmu_errors());

    return (0);
}

int
GC6Plus::check_exit_results()
{
    InfoPrintf("gc6plus: Checking exit results\n");

    RETURN_IF_ERR(check_resident_timers());
    RETURN_IF_ERR(check_sw_status());
    // FIXME: add meaningful checks to ptimer and smartfan
    //RETURN_IF_ERR(check_ptimer());
    //RETURN_IF_ERR(check_smartfan());
    RETURN_IF_ERR(check_i2cs());

    return (0);
}

int
GC6Plus::check_resident_timers()
{
    unique_ptr<IRegisterClass> reg;
    unique_ptr<IRegisterField> field;
    UINT32 reg_addr, reg_data;
    UINT32 entry_timer, resident_timer, exit_timer;

    InfoPrintf("gc6plus: Checking entry/resident/exit timers\n");

    FIND_REGISTER(m_regMap, reg, "LW_PGC6_SCI_ENTRY_TMR");
    reg_addr = reg->GetAddress();
    reg_data = lwgpu->RegRd32(reg_addr);
    InfoPrintf("gc6plus: Reading %s = 0x%08x\n", reg->GetName(), reg_data);
    FIND_FIELD(reg, field, "LW_PGC6_SCI_ENTRY_TMR_TIME");
    entry_timer = (reg_data & field->GetMask()) >> field->GetStartBit();

    FIND_REGISTER(m_regMap, reg, "LW_PGC6_SCI_RESIDENT_TMR");
    reg_addr = reg->GetAddress();
    reg_data = lwgpu->RegRd32(reg_addr);
    InfoPrintf("gc6plus: Reading %s = 0x%08x\n", reg->GetName(), reg_data);
    FIND_FIELD(reg, field, "LW_PGC6_SCI_RESIDENT_TMR_TIME");
    resident_timer = (reg_data & field->GetMask()) >> field->GetStartBit();

    FIND_REGISTER(m_regMap, reg, "LW_PGC6_SCI_EXIT_TMR");
    reg_addr = reg->GetAddress();
    reg_data = lwgpu->RegRd32(reg_addr);
    InfoPrintf("gc6plus: Reading %s = 0x%08x\n", reg->GetName(), reg_data);
    FIND_FIELD(reg, field, "LW_PGC6_SCI_EXIT_TMR_TIME");
    exit_timer = (reg_data & field->GetMask()) >> field->GetStartBit();

    if (expected_exit == RESET)
    {
        // All timers are in reset
        if (entry_timer != 0)
        {
            ErrPrintf("gc6plus: Mismatch in LW_PGC6_SCI_ENTRY_TMR_TIME "
                "field: exp = %d, act = %d\n", 0, entry_timer);
            m_ErrCount++;
        }
        if (resident_timer != 0)
        {
            ErrPrintf("gc6plus: Mismatch in LW_PGC6_SCI_RESIDENT_TMR_TIME "
                "field: exp = %d, act = %d\n", 0, resident_timer);
            m_ErrCount++;
        }
        if (exit_timer != 0)
        {
            ErrPrintf("gc6plus: Mismatch in LW_PGC6_SCI_EXIT_TMR_TIME "
                "field: exp = %d, act = %d\n", 0, exit_timer);
            m_ErrCount++;
        }

    }
    else if (expected_exit == ABORT)
    {
        // Entry timer is >= first_wake_event_us
        // Resident timer is 0
        // Exit timer is is 0
        if (entry_timer < first_wake_event_us)
        {
            ErrPrintf("gc6plus: Mismatch in LW_PGC6_SCI_ENTRY_TMR_TIME "
                "field: exp >= %d, act = %d\n", first_wake_event_us,
                entry_timer);
            m_ErrCount++;
        }
        if (resident_timer != 0)
        {
            ErrPrintf("gc6plus: Mismatch in LW_PGC6_SCI_RESIDENT_TMR_TIME "
                "field: exp = %d, act = %d\n", 0, resident_timer);
            m_ErrCount++;
        }
        if (exit_timer != 0)
        {
            ErrPrintf("gc6plus: Mismatch in LW_PGC6_SCI_EXIT_TMR_TIME "
                "field: exp = %d, act = %d\n", 0, exit_timer);
            m_ErrCount++;
        }

    }
    else  // (expected_exit == EXIT)
    {
        // Entry is > 0
        // Resident = (first_wake_event_us - entry) +0/-2
        // Exit is > 0
        //
        // Speical case: if GC6 ABORT got skipped because it was not enabled,
        // then Resident = 0
        if (entry_timer == 0)
        {
            ErrPrintf("gc6plus: Mismatch in LW_PGC6_SCI_ENTRY_TMR_TIME "
                "field: exp > %d, act = %d\n", 0, entry_timer);
            m_ErrCount++;
        }
        if (gc6_abort_skipped)
        {
            if (resident_timer != 0)
            {
                ErrPrintf("gc6plus: Mismatch in LW_PGC6_SCI_RESIDENT_TMR_TIME "
                    "field: exp = %d, act = %d\n", 0, resident_timer);
                m_ErrCount++;
            }
        }
        else
        {
            if ((resident_timer < (first_wake_event_us - entry_timer - 2)) ||
                (resident_timer > (first_wake_event_us - entry_timer)))
            {
                ErrPrintf("gc6plus: Mismatch in LW_PGC6_SCI_RESIDENT_TMR_TIME "
                    "field: exp = %d +0/-2, act = %d\n",
                    (first_wake_event_us - entry_timer), resident_timer);
                m_ErrCount++;
            }
        }
        if (exit_timer == 0)
        {
            ErrPrintf("gc6plus: Mismatch in LW_PGC6_SCI_EXIT_TMR_TIME "
                "field: exp > %d, act = %d\n", 0, exit_timer);
            m_ErrCount++;
        }
    }

    event_window_us = first_wake_event_us;
    // For EXIT events, all events that oclwrred within the entry time will
    // be captured in the SW status register.
    if ((expected_exit == EXIT) && (event_window_us < entry_timer))
    {
        event_window_us = entry_timer;
    }

    return (0);
}

int
GC6Plus::check_sw_status()
{
    unique_ptr<IRegisterClass> reg;
    unique_ptr<IRegisterField> field;
    unique_ptr<IRegisterValue> value;
    UINT32 reg_addr, reg_data;
    UINT32 act_sw_intr, exp_sw_intr;

    InfoPrintf("gc6plus: Checking SW status\n");

    FIND_REGISTER(m_regMap, reg, "LW_PGC6_SCI_SW_INTR0_STATUS_LWRR");
    reg_addr = reg->GetAddress();
    reg_data = lwgpu->RegRd32(reg_addr);
    InfoPrintf("gc6plus: Reading %s = 0x%08x\n", reg->GetName(), reg_data);
    act_sw_intr = reg_data;
    exp_sw_intr = 0;
    if (gen_wake_ec_gpu_event && (wake_ec_gpu_event_delay <=
        event_window_us))
    {
        FIND_FIELD(reg, field, "LW_PGC6_SCI_SW_INTR0_STATUS_LWRR_GPU_EVENT");
        FIND_VALUE(field, value,
            "LW_PGC6_SCI_SW_INTR0_STATUS_LWRR_GPU_EVENT_PENDING");
        exp_sw_intr |= (value->GetValue() << field->GetStartBit());
    }
    if ((wake_timer_us <= event_window_us) &&
        first_wake_event_is_wakeup_tmr)
    {
        FIND_FIELD(reg, field, "LW_PGC6_SCI_SW_INTR0_STATUS_LWRR_WAKE_TIMER");
        FIND_VALUE(field, value,
            "LW_PGC6_SCI_SW_INTR0_STATUS_LWRR_WAKE_TIMER_PENDING");
        exp_sw_intr |= (value->GetValue() << field->GetStartBit());
    }
    if (gen_wake_therm_i2c && (wake_therm_i2c_delay <= event_window_us) &&
        first_wake_event_is_therm_i2c)
    {
        FIND_FIELD(reg, field, "LW_PGC6_SCI_SW_INTR0_STATUS_LWRR_I2CS");
        FIND_VALUE(field, value,
            "LW_PGC6_SCI_SW_INTR0_STATUS_LWRR_I2CS_PENDING");
        exp_sw_intr |= (value->GetValue() << field->GetStartBit());
    }
    if (gen_wake_clkreq || gen_wake_rxstat)
    {
        FIND_FIELD(reg, field, "LW_PGC6_SCI_SW_INTR0_STATUS_LWRR_BSI_EVENT");
        FIND_VALUE(field, value,
            "LW_PGC6_SCI_SW_INTR0_STATUS_LWRR_BSI_EVENT_PENDING");
        exp_sw_intr |= (value->GetValue() << field->GetStartBit());
    }
    if (gen_sr_update)
    {
        FIND_FIELD(reg, field, "LW_PGC6_SCI_SW_INTR0_STATUS_LWRR_SR_UPDATE");
        FIND_VALUE(field, value,
            "LW_PGC6_SCI_SW_INTR0_STATUS_LWRR_SR_UPDATE_PENDING");
        exp_sw_intr |= (value->GetValue() << field->GetStartBit());
    }
    if ((m_mode == GC5) && (expected_exit == ABORT))
    {
        FIND_FIELD(reg, field, "LW_PGC6_SCI_SW_INTR0_STATUS_LWRR_GC5_ABORT");
        FIND_VALUE(field, value,
            "LW_PGC6_SCI_SW_INTR0_STATUS_LWRR_GC5_ABORT_PENDING");
        exp_sw_intr |= (value->GetValue() << field->GetStartBit());
    }
    else if ((m_mode == GC6) && (expected_exit == ABORT))
    {
        FIND_FIELD(reg, field, "LW_PGC6_SCI_SW_INTR0_STATUS_LWRR_GC6_ABORT");
        FIND_VALUE(field, value,
            "LW_PGC6_SCI_SW_INTR0_STATUS_LWRR_GC6_ABORT_PENDING");
        exp_sw_intr |= (value->GetValue() << field->GetStartBit());
    }
    else if ((m_mode == GC5) && (expected_exit == EXIT))
    {
        FIND_FIELD(reg, field, "LW_PGC6_SCI_SW_INTR0_STATUS_LWRR_GC5_EXIT");
        FIND_VALUE(field, value,
            "LW_PGC6_SCI_SW_INTR0_STATUS_LWRR_GC5_EXIT_PENDING");
        exp_sw_intr |= (value->GetValue() << field->GetStartBit());
    }
    else if ((m_mode == GC6) && (expected_exit == EXIT))
    {
        FIND_FIELD(reg, field, "LW_PGC6_SCI_SW_INTR0_STATUS_LWRR_GC6_EXIT");
        FIND_VALUE(field, value,
            "LW_PGC6_SCI_SW_INTR0_STATUS_LWRR_GC6_EXIT_PENDING");
        exp_sw_intr |= (value->GetValue() << field->GetStartBit());
    }
    // RESET trumps all SW status
    if (expected_exit == RESET)
    {
        exp_sw_intr = 0;
    }
    if (exp_sw_intr != act_sw_intr)
    {
        ErrPrintf("gc6plus: Mismatch in %s register: exp = 0x%08x, "
            "act = 0x%08x\n", reg->GetName(), exp_sw_intr, act_sw_intr);
        m_ErrCount++;
    }


    FIND_REGISTER(m_regMap, reg, "LW_PGC6_SCI_SW_INTR1_STATUS_LWRR");
    reg_addr = reg->GetAddress();
    reg_data = lwgpu->RegRd32(reg_addr);
    InfoPrintf("gc6plus: Reading %s = 0x%08x\n", reg->GetName(), reg_data);
    act_sw_intr = reg_data;
    exp_sw_intr = 0;
    if (gen_wake_gpio_misc_0)
    {
        FIND_FIELD(reg, field, "LW_PGC6_SCI_SW_INTR1_STATUS_LWRR_MISC_0");
        FIND_VALUE(field, value,
            "LW_PGC6_SCI_SW_INTR1_STATUS_LWRR_MISC_0_PENDING");
        exp_sw_intr |= (value->GetValue() << field->GetStartBit());
    }
    // RESET trumps all SW status
    if (expected_exit == RESET)
    {
        exp_sw_intr = 0;
    }
    if (exp_sw_intr != act_sw_intr)
    {
        ErrPrintf("gc6plus: Mismatch in %s register: exp = 0x%08x, "
            "act = 0x%08x\n", reg->GetName(), exp_sw_intr, act_sw_intr);
        m_ErrCount++;
    }


    FIND_REGISTER(m_regMap, reg, "LW_PGC6_SCI_SW_INTR0_STATUS_PREV");
    reg_addr = reg->GetAddress();
    reg_data = lwgpu->RegRd32(reg_addr);
    InfoPrintf("gc6plus: Reading %s = 0x%08x\n", reg->GetName(), reg_data);
    act_sw_intr = reg_data;
    exp_sw_intr = 0;
    if (exp_sw_intr != act_sw_intr)
    {
        ErrPrintf("gc6plus: Mismatch in %s register: exp = 0x%08x, "
            "act = 0x%08x\n", reg->GetName(), exp_sw_intr, act_sw_intr);
        m_ErrCount++;
    }

    FIND_REGISTER(m_regMap, reg, "LW_PGC6_SCI_SW_INTR1_STATUS_PREV");
    reg_addr = reg->GetAddress();
    reg_data = lwgpu->RegRd32(reg_addr);
    InfoPrintf("gc6plus: Reading %s = 0x%08x\n", reg->GetName(), reg_data);
    act_sw_intr = reg_data;
    exp_sw_intr = 0;
    if (exp_sw_intr != act_sw_intr)
    {
        ErrPrintf("gc6plus: Mismatch in %s register: exp = 0x%08x, "
            "act = 0x%08x\n", reg->GetName(), exp_sw_intr, act_sw_intr);
        m_ErrCount++;
    }

    return (0);
}

//IO clobber test tests fullchip asserts to make sure that fullchip hookups are
//working as expected. 
int GC6Plus::run_io_clobber_test(int loop_delay_us)
{

    UINT32 sci_pwm_hi, sci_pwm_period, disp_pwm_hi, disp_pwm_period;
    UINT32 expected_gpio_val = GPIO_RD_HIZ;
    const unsigned int gpio_drvs[2] = {GPIO_DRV_FULL,GPIO_DRV_OPEN_DRAIN};
    //capture previous register writes here. Needed for conditional register writes to save test time.
    std::map<unsigned int, unsigned int> PastRegWrs;         
   
    //set up pwm values to get lots of variation
    sci_pwm_hi = 0;
    disp_pwm_hi = 0;
    sci_pwm_period = 9;
    disp_pwm_period = 13;

    //Activate Asserts
    InfoPrintf("%s-%s: Entering IO Clobber Test. Enabling end2end asserts\n",__FILE__,__FUNCTION__); 
    Platform::EscapeWrite("gc6plus_assert_enable_end2end_asserts", 0, 1, 1);
    
    //Step 1: Confirm that the PWM signals are at least transmitting.
    //        Try 100% and 0% duty, and read back the GPIOs. Test for 0,1,Z
    
    //All non-configured GPIOs should read back Z. All configured gpios should
    //read back exp (0 or 1)
    for(UINT32 exp = 0; exp<= 1; exp++)
    {
        if(exp == 0)
        {
            InfoPrintf("%s-%s: Test for 0's on target GPIO. Test for Z's on other GPIO\n",__FILE__,__FUNCTION__);
            sci_pwm_hi = 0;
            disp_pwm_hi = 0;
        }
        else if(exp == 1)
        {

            InfoPrintf("%s-%s: Test for 1's on target GPIO. Test for Z's on other GPIO\n",__FILE__,__FUNCTION__);
            sci_pwm_hi = sci_pwm_period;
            disp_pwm_hi = disp_pwm_period;
        }

        //Program PWM for new expected value
        RETURN_IF_ERR(cfg_pwms(sci_pwm_period, sci_pwm_hi, "sci"));
        RETURN_IF_ERR(cfg_pwms(disp_pwm_period, disp_pwm_hi, "pmgr"));

        //Delay until new PWM starts
        Platform::Delay(10);

        for(UINT32 target = 0; target<m_max_gpios; target++)
        {
            
            //set up all GPIOs. Target will source PWM. Others are programmed
            //to be disabled
            RETURN_IF_ERR(cfg_gpio_pwm_1hot(target,GPIO_DRV_FULL, PastRegWrs));
            Platform::Delay(loop_delay_us);
            
            //Check all GPIOs to see if values match
            for(UINT32 i = 0; i<m_max_gpios; i++)
            {
                expected_gpio_val = (i == target) ? exp : GPIO_RD_HIZ;
                UINT32 gpio_val = get_gpio_state(i);
                if(gpio_val != expected_gpio_val)
                {
                    ErrPrintf("%s-%s: GPIO(%d) = %d (Expected %d)\n",__FILE__,__FUNCTION__,i,gpio_val,expected_gpio_val);
                    m_ErrCount++;
                } 
                else
                {
                    //escaperead prints value
                    DebugPrintf("%s-%s: GPIO %d = %d (PASS)\n",__FILE__,__FUNCTION__,i,gpio_val);
                }

            }
            
            //Exit if any errors in the previous test
            if(m_ErrCount > 0)
            {
                ErrPrintf("%s-%s: Failures seen during init section. May be a connction issue. Check waves to verify.\n",__FILE__,__FUNCTION__);
                return(1);
                //Throw error and exit. There is clearly a problem
            }
            else
            {
                InfoPrintf("%s-%s: %d Test: GPIO %d (PASS)\n",__FILE__,__FUNCTION__,exp,target);
            }
        }
    }

    //Step 2: Configure different PWM signals and let assert checking take 
    //care of the rest

    //Configure PWMs 
    sci_pwm_hi = 4;
    disp_pwm_hi = 7;
    RETURN_IF_ERR(cfg_pwms(sci_pwm_period, sci_pwm_hi, "sci"));
    RETURN_IF_ERR(cfg_pwms(disp_pwm_period, disp_pwm_hi, "pmgr"));

    InfoPrintf("%s-%s: Starting IO Clobber Stress Test with Simultaneous PWMs\n",__FILE__,__FUNCTION__);


    //Loop through all GPIOs, assigning PWM signals and toggling ALT_CTL_SEL
    //Rely's on fullchip assets to find failures. 
    //May be able to guild differently by not using PWM signals, but PWMs+assert
    //coverage should work
    for(UINT32 i = 0; i < (sizeof (gpio_drvs)/sizeof (* gpio_drvs)); i++)
    {
        for(UINT32 target = 0; target < m_max_gpios; target++)
        {
            //try with ALT_CTL_SEL = 0 first
            RETURN_IF_ERR(cfg_gpio_pwm_1hot(target,gpio_drvs[i], PastRegWrs));
            Platform::Delay(loop_delay_us);
            InfoPrintf("%s-%s: GPIO=%d, PMGR Drive, %s (PASS)\n",__FILE__,__FUNCTION__,target,(i==GPIO_DRV_FULL) ? "Full Drive" : "Open Drain");


            //assert ALT_CTL_SEL
            RETURN_IF_ERR(Reg_RMW("LW_PGC6_SCI_PWR_SEQ_ARMED_SEQ_STATE","ALT_MUX_SEL","ASSERT"));
            RETURN_IF_ERR(Reg_RMW("LW_PGC6_SCI_PWR_SEQ_STATE","UPDATE","TRIGGER"));
            Platform::Delay(loop_delay_us);
            InfoPrintf("%s-%s: GPIO=%d, SCI Drive, %s (PASS)\n",__FILE__,__FUNCTION__,target,(i==GPIO_DRV_FULL) ? "Full Drive" : "Open Drain");
            
            //de-assert ALT_CTL_SEL and move to the next loop
            RETURN_IF_ERR(Reg_RMW("LW_PGC6_SCI_PWR_SEQ_ARMED_SEQ_STATE","ALT_MUX_SEL","DEASSERT"));
            RETURN_IF_ERR(Reg_RMW("LW_PGC6_SCI_PWR_SEQ_STATE","UPDATE","TRIGGER"));
            Platform::Delay(1);
        }
    }

    //TODO - Investigate testing I2CS via the i2c stub
    //TODO - Investigate testing CLKREQ#, but not sure how to toggle manually


    //De-activate Asserts
    Platform::EscapeWrite("gc6plus_assert_enable_end2end_asserts", 0, 1, 0);

    return 0;
}

int
GC6Plus::check_ptimer()
{
    unique_ptr<IRegisterClass> reg;
    unique_ptr<IRegisterField> field;
    unique_ptr<IRegisterValue> value;
    UINT32 reg_addr, reg_data;

    InfoPrintf("gc6plus: Checking PTIMER\n");

    FIND_REGISTER(m_regMap, reg, "LW_PGC6_SCI_PTIMER_READ");
    reg_addr = reg->GetAddress();
    FIND_FIELD(reg,   field, "LW_PGC6_SCI_PTIMER_READ_SAMPLE");
    FIND_VALUE(field, value, "LW_PGC6_SCI_PTIMER_READ_SAMPLE_TRIGGER");
    reg_data = (value->GetValue() << field->GetStartBit());
    lwgpu->RegWr32(reg_addr, reg_data);
    InfoPrintf("gc6plus: Writing %s = 0x%08x\n", reg->GetName(), reg_data);
    FIND_REGISTER(m_regMap, reg, "LW_PGC6_SCI_PTIMER_TIME_1");
    reg_addr = reg->GetAddress();
    reg_data = lwgpu->RegRd32(reg_addr);
    InfoPrintf("gc6plus: Reading %s = 0x%08x\n", reg->GetName(), reg_data);
    FIND_FIELD(reg, field, "LW_PGC6_SCI_PTIMER_TIME_1_NSEC");
    FIND_REGISTER(m_regMap, reg, "LW_PGC6_SCI_PTIMER_TIME_0");
    reg_addr = reg->GetAddress();
    reg_data = lwgpu->RegRd32(reg_addr);
    InfoPrintf("gc6plus: Reading %s = 0x%08x\n", reg->GetName(), reg_data);
    FIND_FIELD(reg, field, "LW_PGC6_SCI_PTIMER_TIME_0_NSEC");

    return (0);
}

int
GC6Plus::check_smartfan()
{
    unique_ptr<IRegisterClass> reg;
    unique_ptr<IRegisterField> field;
    UINT32 reg_addr, reg_data;

    InfoPrintf("gc6plus: Checking Smartfan\n");

    FIND_REGISTER(m_regMap, reg, "LW_PGC6_SCI_FAN_ACT0");
    reg_addr = reg->GetAddress();
    reg_data = lwgpu->RegRd32(reg_addr);
    InfoPrintf("gc6plus: Reading %s = 0x%08x\n", reg->GetName(), reg_data);
    FIND_FIELD(reg, field, "LW_PGC6_SCI_FAN_ACT0_PERIOD");
    FIND_FIELD(reg, field, "LW_PGC6_SCI_FAN_ACT0_OVERFLOW");
    FIND_REGISTER(m_regMap, reg, "LW_PGC6_SCI_FAN_ACT1");
    reg_addr = reg->GetAddress();
    reg_data = lwgpu->RegRd32(reg_addr);
    InfoPrintf("gc6plus: Reading %s = 0x%08x\n", reg->GetName(), reg_data);
    FIND_FIELD(reg, field, "LW_PGC6_SCI_FAN_ACT1_HI");

    return (0);
}

int
GC6Plus::check_i2cs()
{
    UINT32 smbus_nack, i2c_rd_data;

    InfoPrintf("gc6plus: Checking I2CS\n");

    // THERM I2C
    if (gen_wake_therm_i2c)
    {
        if (poll_escape("sm_done", 1, true, m_polling_timeout_us))
        {
            InfoPrintf("gc6plus: Timeout waiting for SMBus master\n");
            m_ErrCount++;
        }
        else
        {
            Platform::EscapeRead("sm_no_ack", 0, 1, &smbus_nack);
            if (smbus_nack)
            {
                InfoPrintf("gc6plus: NACK detected by SMBus master\n");
                m_ErrCount++;
            }
            else
            {
                Platform::EscapeRead("sm_rdata", 0, 32, &i2c_rd_data);
                InfoPrintf("gc6plus: SMBus master read data = 0x%02x\n",
                    i2c_rd_data);
                if (i2c_rd_data != 0xde)
                {
                    ErrPrintf("gc6plus: Mismatch in THERM i2c rd byte: "
                        "exp = 0x%08x, act = 0x%08x\n", 0xde, i2c_rd_data);
                    m_ErrCount++;
                }
            }
        }
    }

    // SCI I2C
    if (gen_i2c_sci_priv_rd_SCRATCH)
    {
        // FIXME
        i2c_rd_data = 0;
    }

    return (0);
}


