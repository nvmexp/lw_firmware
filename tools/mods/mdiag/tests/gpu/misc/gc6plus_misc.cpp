/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2017-2018, 2020-2021 by LWPU Corporation.  All rights reserved. 
 * All information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

// To facilitate faster compiles via pre-compiled headers, keep "stdtest.h"
// first.
#include "mdiag/tests/stdtest.h"

#include "core/include/rc.h"
#include "gc6plus_misc.h"
#include "mdiag/lwgpu.h"
#include "mdiag/utils/lwgpu_channel.h"
#include "mdiag/tests/test_state_report.h"
#include "core/include/gpu.h"
#include "gpio_engine.h"

// flags to indicate which test to run
static bool priv_level_test = false;
static bool host_ptimer_test = false;
static bool selwre_timer_test = false;
static bool sci_individual_alt_ctl_test = false;
static bool sci_hw_gpio_copy = false;

extern const ParamDecl gc6plus_misc_params[] =
{
    // test args for priv level test
    SIMPLE_PARAM("-priv_level_test", "Test LW_PGC6 Priv Level protection"),
    STRING_PARAM("-priv_level_reg", "Register for priv_level test"),
    STRING_PARAM("-priv_level_mask", "Protection mask for priv_level test"),
    UNSIGNED_PARAM("-priv_level_array", "Register array index for priv_level"),
    UNSIGNED_PARAM("-ring_intr_poll_timeout", "Poll timeout for ring intr to propagate (in us, default 3 us"),

    // test args for host ptimer and secure timer test
    SIMPLE_PARAM("-host_ptimer_test", "Test HOST PTIMER drift"),
    SIMPLE_PARAM("-selwre_timer_test", "Test Secure Timer drift"),
    UNSIGNED_PARAM("-system_time_0", "Specify system_time_1"),
    UNSIGNED_PARAM("-system_time_1", "Specify system_time_0"),
    UNSIGNED_PARAM("-ptimer_delay_ms", "Delay between PTIMER reads (in ms, default 10 ms)"),
    UNSIGNED_PARAM("-max_ptimer_drift_ns", "Max allowed PTIMER drift (in ns, default 100 ns)"),
    SIMPLE_PARAM("-read_time_using_escape_read", "Read timers using EscapeRead"),

    // test args for SCI individual ALT_CTL test and HW Gpio copy test
    SIMPLE_PARAM("-sci_individual_alt_ctl_test", "Test SCI Individual ALT_CTL control"),
    SIMPLE_PARAM("-sci_hw_gpio_copy", "Test SCI HW Gpio copy"),
    UNSIGNED_PARAM("-gpio", "Test one GPIO index"),
    UNSIGNED_PARAM("-gpio_min", "Test GPIOs starting from GPIO index"),
    UNSIGNED_PARAM("-gpio_max", "Test GPIOs ending with GPIO index"),

    PARAM_SUBDECL(lwgpu_single_params),
    LAST_PARAM
};


GC6PlusMisc::GC6PlusMisc(ArgReader *params) :
    LWGpuSingleChannelTest(params)
{
    InfoPrintf("Starting GC6Plus Misc constructor\n");
    // test args for priv level test
    if (params->ParamPresent("-priv_level_test"))
    {
        priv_level_test = true;
    }
    m_priv_level_regname = params->ParamStr("-priv_level_reg", NULL);
    m_priv_level_maskname = params->ParamStr("-priv_level_mask", NULL);
    m_priv_level_array_index = params->ParamUnsigned("-priv_level_array", 0);
    m_ring_intr_poll_timeout = params->ParamUnsigned(
        "-ring_intr_poll_timeout", 3);
    
    // test args for host ptimer test
    if (params->ParamPresent("-host_ptimer_test"))
    {
        host_ptimer_test = true;
    }
    InfoPrintf("after setting host ptimer test variable GC6Plus Misc constructor\n");
    if (params->ParamPresent("-selwre_timer_test"))
    {
        selwre_timer_test = true;
    }
    m_system_time_0 = params->ParamUnsigned("-system_time_0", 0);
    m_system_time_1 = params->ParamUnsigned("-system_time_1", 0);
    m_ptimer_delay_ms = params->ParamUnsigned("-ptimer_delay_ms", 10);
    m_max_ptimer_drift_ns = params->ParamUnsigned("-max_ptimer_drift_ns", 100);
    m_read_time_using_escape_read =
        ((params->ParamPresent("-read_time_using_escape_read")) != 0);

    // test args for SCI individual ALT_CTL test and HW Gpio copy
    if (params->ParamPresent("-sci_individual_alt_ctl_test"))
    {
        sci_individual_alt_ctl_test = true;
    }
    if (params->ParamPresent("-sci_hw_gpio_copy"))
    {
        sci_hw_gpio_copy = true;
    }
    m_gpio = params->ParamUnsigned("-gpio", -1);
    m_gpio_min = params->ParamUnsigned("-gpio_min", -1);
    m_gpio_max = params->ParamUnsigned("-gpio_max", -1);
    InfoPrintf("End of constructor\n");
}

GC6PlusMisc::~GC6PlusMisc(void)
{
}

STD_TEST_FACTORY(gc6plus_misc, GC6PlusMisc)

int
GC6PlusMisc::Setup(void)
{
    lwgpu = LWGpuResource::FindFirstResource();

    ch = lwgpu->CreateChannel();
    if(!ch)
    {
        ErrPrintf("GC6PlusMisc: Setup failed to create channel\n");
        return (0);
    }

    m_regMap = lwgpu->GetRegisterMap();
    if(!m_regMap)
    {
        ErrPrintf("GC6PlusMisc: gc6plus_misc can only be run on GPU's that "
            "support a register map!\n");
        return (0);
    }

    getStateReport()->init("gc6plus_misc");
    getStateReport()->enable();

    return 1;
}

void
GC6PlusMisc::CleanUp(void)
{
    if(ch)
    {
        delete ch;
        ch = 0;
    }

    if (lwgpu)
    {
        DebugPrintf("GC6PlusMisc::CleanUp(): Releasing GPU.\n");
        lwgpu = 0;
    }
    LWGpuSingleChannelTest::CleanUp();
}

void
GC6PlusMisc::Run(void)
{
    bool ran_test = false;
    SetStatus(TEST_INCOMPLETE);
    InfoPrintf("Starting GC6Plus Misc testing\n");
    m_ErrCount = 0;
    m_max_polling_attempts = MAX_POLLING_ATTEMPTS;

    if (priv_level_test)
    {
        if (PrivLevelTest())
        {
            SetStatus(TEST_FAILED);
            getStateReport()->runFailed("Failed test\n");
            return;
        }
        ran_test = true;
    }
    if (host_ptimer_test)
    {
        if (HostPtimerTest())
        {
            SetStatus(TEST_FAILED);
            getStateReport()->runFailed("Failed test\n");
            return;
        }
        ran_test = true;
    }
    if (selwre_timer_test)
    {
        if (SelwreTimerTest())
        {
            SetStatus(TEST_FAILED);
            getStateReport()->runFailed("Failed test\n");
            return;
        }
        ran_test = true;
    }
    if (sci_individual_alt_ctl_test)
    {
        if (SciIndvAltCtlTest())
        {
            SetStatus(TEST_FAILED);
            getStateReport()->runFailed("Failed test\n");
            return;
        }
        ran_test = true;
    }
    if (sci_hw_gpio_copy)
    {
        if (SciHwGpioCopyTest())
        {
            SetStatus(TEST_FAILED);
            getStateReport()->runFailed("Failed test\n");
            return;
        }
        ran_test = true;
    }


    if (!ran_test)
    {
        ErrPrintf("No GC6Plus Misc test specified, no test ran\n");
        return;
    }

    InfoPrintf("GC6PlusMisc: Test completed with %d miscompares\n",
        m_ErrCount);
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


// The PrivLevelTest exercises the PRIV_LEVEL protection on LW_PGC6_*
// priv registers.  We need to modify the opt_priv_sec_en fuse in order for
// the test to modify the PRIV_LEVEL masks during testing.  Otherwise, once
// the protection level on the registers are raised, they are no longer
// accessible by the test and there is no way to verify its behavior.
// The following conditions are tested:
//   1) CPU can access registers when PRIV_LEVEL 0 is unlocked
//   2) CPU cannot access registers when PRIV_LEVEL 0 is locked
UINT32
GC6PlusMisc::PrivLevelTest()
{
    InfoPrintf("GC6PlusMisc: Running priv_level test\n");

    // priv_level register and mask are mandatory test args
    if (m_priv_level_regname == NULL)
    {
        ErrPrintf("GC6PlusMisc: Missing test arg: -priv_level_reg\n");
        return (1);
    }
    if (m_priv_level_maskname == NULL)
    {
        ErrPrintf("GC6PlusMisc: Missing test arg: -priv_level_mask\n");
        return (1);
    }

    // This tests generates and checks priv errors.  We do not want
    // the RM to take these errors, so we need to mask them.  We update
    // the interrupt mask here and restore it at the end of the test.
    if (disable_priv_err_intr())
    {
        ErrPrintf("GC6PlusMisc: Failure in disabling priv error interrupt\n");
        return (1);
    }

    // Clear out any stale priv error that may have oclwrred before.
    if (clear_pri_fecserr())
    {
        ErrPrintf("GC6PlusMisc: Failure in clearing pri_fecserr\n");
        return (1);
    }
    if (clear_sys_priv_error())
    {
        ErrPrintf("GC6PlusMisc: Failure in clearing sys_priv_error\n");
        return (1);
    }

    // The priv interfaces may be configured to generate an ack immediately
    // on write requests, or wait until the write completes before generating
    // an ack.  This affects which register the priv violation is reported to.
    // We will get the current ack modes and use it when checking the location
    // of the priv violation.
    if (get_priv_ack_mode(&m_cpu_bar0_immediate_ack))
    {
        ErrPrintf("GC6PlusMisc: Failure in getting priv ack mode\n");
        return (1);
    }

    if (test_priv_protect(m_priv_level_regname, m_priv_level_maskname,
        m_priv_level_array_index))
    {
        ErrPrintf("GC6PlusMisc: Failure in read/write testing\n");
        return (1);
    }

    return (0);
}


// The HostPtimerTest checks the drift of the HOST PTIMER against the SCI
// PTIMER.  At the beginning of the test, the SCI PTIMER is configured and
// then copied to the HOST PTIMER.  Both timers are read to generate a
// baseline of the offset.  Then the test delays for XXX ms and reads both
// timers again.  The time difference should match the offset, within a
// margin of error.
UINT32
GC6PlusMisc::HostPtimerTest()
{
    unique_ptr<IRegisterClass> reg;
    const char *regname;
    UINT32 reg_addr, reg_data;
    UINT32 sci_time_1, sci_time_0;
    INT32 offset, drift;
    INT64 difference, max_difference;

    InfoPrintf("GC6PlusMisc: Running host_ptimer test\n");

    // Configure SCI PTIMER to system time
    if (set_sci_ptimer(m_system_time_1, m_system_time_0))
    {
        ErrPrintf("GC6PlusMisc: Failed to set SCI PTIMER\n");
        return (1);
    }

    // Perform a dummy read to flush out any writes that may affect the
    // latency of SCI PRIV reads.
    regname = "LW_PGC6_SCI_DEBUG_STATUS";
    if (!(reg = m_regMap->FindRegister(regname)))
    {
        ErrPrintf("GC6PlusMisc: Failed to find %s register\n", regname);
        return (1);
    }
    reg_addr = reg->GetAddress();
    reg_data = lwgpu->RegRd32(reg_addr);
    InfoPrintf("GC6PlusMisc: Reading %s = 0x%08x\n", reg->GetName(), reg_data);

    // get SCI PTIMER
    if (get_sci_ptimer(&sci_time_1, &sci_time_0))
    {
        ErrPrintf("GC6PlusMisc: Failed to get SCI PTIMER\n");
        return (1);
    }

    // write this value into HOST PTIMER
    if (set_host_ptimer(sci_time_1, sci_time_0))
    {
        ErrPrintf("GC6PlusMisc: Failed to set HOST PTIMER\n");
        return (1);
    }

    // Measure the baseline offset between SCI and HOST ptimer.
    // This is the baseline offset.
    if (get_ptimer_difference(&difference))
    {
        ErrPrintf("GC6PlusMisc: Failed to get difference between SCI and HOST "
            "PTIMER\n");
        return (1);
    }
    InfoPrintf("GC6PlusMisc: Baseline offset between SCI and HOST PTIMER = "
        "%d ns\n", difference);

    // The offset needs to fit in a 32-bit number.  Therefore, we put a limit
    // on the 64-bit max difference we expect.  With priv reads, we expect the
    // difference to be smaller than 1 us.  But for EscapeRead, we do not have
    // the priv access latency to equalize the difference.  Therefore, the
    // limit using EscapeRead will be larger.
    if (m_read_time_using_escape_read)
    {
        max_difference = 10000;
    }
    else
    {
        max_difference = 1000;
    }
    if ((difference < -max_difference) || (difference > max_difference))
    {
        ErrPrintf("GC6PlusMisc: Offset is too large (exceeds %d ns)\n",
            max_difference);
        m_ErrCount++;
    }
    offset = UINT32(difference);


    // delay
    InfoPrintf("GC6PlusMisc: Delaying %d ms\n", m_ptimer_delay_ms);
    Platform::Delay(m_ptimer_delay_ms * 1000);  // delay units are in us


    // Reread timers and recompute the offset.
    // Any change is considered drift.
    if (get_ptimer_difference(&difference))
    {
        ErrPrintf("GC6PlusMisc: Failed to get difference between SCI and HOST "
            "PTIMER\n");
        return (1);
    }
    InfoPrintf("GC6PlusMisc: Offset between SCI and HOST PTIMER = %d ns\n",
        difference);

    // subtract the baseline offset from the difference to get the drift
    drift = UINT32(difference) - offset;
    InfoPrintf("GC6PlusMisc: Drift from last reading = %d ns\n", drift);

    // Check that the drift is within allowed range
    if ((drift < -m_max_ptimer_drift_ns) || (drift > m_max_ptimer_drift_ns))
    {
        ErrPrintf("GC6PlusMisc: Drift is too large (exceeds %d ns)\n",
            m_max_ptimer_drift_ns);
        m_ErrCount++;
    }

    return (0);
}


// The SelwreTimerTest checks the drift of the HOST PTIMER against the SCI
// Secure Timer.  At the beginning of the test, the SCI Secure Timer is
// configured and then copied to the HOST PTIMER.  Both timers are read to
// generate a baseline of the offset.  Then the test delays for XXX ms and
// reads both timers again.  The time difference should match the offset,
// within a margin of error.
UINT32
GC6PlusMisc::SelwreTimerTest()
{
    unique_ptr<IRegisterClass> reg;
    const char *regname;
    UINT32 reg_addr, reg_data;
    UINT32 sci_time_1, sci_time_0;
    UINT32 loop_count, max_attempts;
    INT32 rc, offset, drift;
    INT64 difference, max_difference;
    bool done;

    InfoPrintf("GC6PlusMisc: Running selwre_timer test\n");

    // Configure Secure Timer to system time
    if (set_selwre_timer(m_system_time_1, m_system_time_0))
    {
        ErrPrintf("GC6PlusMisc: Failed to set Secure Timer\n");
        return (1);
    }

    // get SCI Secure Timer
    // TIME_1 could wrap when we read TIME_0, but it should be impossible to
    // detect it twice in a row.  We should only need 2 attempts to get a valid
    // SCI Secure Timer read.
    done = false;
    loop_count = 0;
    max_attempts = 2;
    while (!done && (loop_count < max_attempts))
    {
        // Perform a dummy read to flush out any writes that may affect the
        // latency of SCI PRIV reads.
        regname = "LW_PGC6_SCI_DEBUG_STATUS";
        if (!(reg = m_regMap->FindRegister(regname)))
        {
            ErrPrintf("GC6PlusMisc: Failed to find %s register\n", regname);
            return (1);
        }
        reg_addr = reg->GetAddress();
        reg_data = lwgpu->RegRd32(reg_addr);
        InfoPrintf("GC6PlusMisc: Reading %s = 0x%08x\n", reg->GetName(),
            reg_data);

        rc = get_selwre_timer(&sci_time_1, &sci_time_0);
        if (rc > 0)
        {
            ErrPrintf("GC6PlusMisc: Failed to get Secure Timer\n");
            return (1);
        }

        // We got a good TIMER read if rc == 0
        // We got a bad TIMER read if rc == -1
        done = (rc == 0);
        loop_count = loop_count + 1;
    }

    if (!done)
    {
        ErrPrintf("GC6PlusMisc: Could not get stable SCI Secure Timer "
            "after %d attempts\n", max_attempts);
        m_ErrCount++;
    }

    // write this value into HOST PTIMER
    if (set_host_ptimer(sci_time_1, sci_time_0))
    {
        ErrPrintf("GC6PlusMisc: Failed to set HOST PTIMER\n");
        return (1);
    }

    // Measure the baseline offset between Secure Timer and HOST ptimer.
    // This is the baseline offset.
    if (get_selwre_timer_difference(&difference))
    {
        ErrPrintf("GC6PlusMisc: Failed to get difference between SCI Secure "
            "Timer and HOST PTIMER\n");
        return (1);
    }
    InfoPrintf("GC6PlusMisc: Baseline offset between SCI Secure Timer and HOST "
        "PTIMER = %d ns\n", difference);

    // The offset needs to fit in a 32-bit number.  Therefore, we put a limit
    // on the 64-bit max difference we expect.  With priv reads, we expect the
    // difference to be smaller than 1 us.  But for EscapeRead, we do not have
    // the priv access latency to equalize the difference.  Therefore, the
    // limit using EscapeRead will be larger.
    if (m_read_time_using_escape_read)
    {
        max_difference = 10000;
    }
    else
    {
        max_difference = 1000;
    }
    if ((difference < -max_difference) || (difference > max_difference))
    {
        ErrPrintf("GC6PlusMisc: Offset is too large (exceeds %d ns)\n",
            max_difference);
        m_ErrCount++;
    }
    offset = UINT32(difference);


    // delay
    InfoPrintf("GC6PlusMisc: Delaying %d ms\n", m_ptimer_delay_ms);
    Platform::Delay(m_ptimer_delay_ms * 1000);  // delay units are in us


    // Reread timers and recompute the offset.
    // Any change is considered drift.
    if (get_selwre_timer_difference(&difference))
    {
        ErrPrintf("GC6PlusMisc: Failed to get difference between SCI Secure "
            "Timer and HOST PTIMER\n");
        return (1);
    }
    InfoPrintf("GC6PlusMisc: Offset between SCI Secure Timer and HOST PTIMER "
        "= %d ns\n", difference);

    // subtract the baseline offset from the difference to get the drift
    drift = UINT32(difference) - offset;
    InfoPrintf("GC6PlusMisc: Drift from last reading = %d ns\n", drift);

    // Check that the drift is within allowed range
    if ((drift < -m_max_ptimer_drift_ns) || (drift > m_max_ptimer_drift_ns))
    {
        ErrPrintf("GC6PlusMisc: Drift is too large (exceeds %d ns)\n",
            m_max_ptimer_drift_ns);
        m_ErrCount++;
    }

    return (0);
}




// This routine takes 1 protected register through its paces.  It checks:
//  - CPU can access the register group when it is not protected
//  - CPU can lock the protection group
//  - CPU can no longer access the register and priv_error is reported
//  - disable security fuse, unlock protection group, re-enable security fuse
//  - CPU can access the register group
INT32
GC6PlusMisc::test_priv_protect (const char *priv_regname, const char
    *priv_mask_regname, UINT32 array_index)
{
    unique_ptr<IRegisterClass> reg;
    char regname[127];
    UINT32 reg_addr, rw_mask, init_data, dimension;
    UINT32 write_protection, write_violation, read_protection, read_violation;

    // Check that the register exist
    if (!(reg = m_regMap->FindRegister(priv_regname)))
    {
        ErrPrintf("GC6PlusMisc: Failed to find %s register\n", priv_regname);
        return (1);
    }

    // Get the address of the register.
    // If a non-zero array index was specified, check if the register is
    // an array.  If it is not an array, then it is an error to specify
    // a non-zero array index.
    dimension = reg->GetArrayDimensions();
    if (dimension == 0)
    {
        // Non-array register.  Array index is not valid.
        if (array_index > 0)
        {
            ErrPrintf("GC6PlusMisc: Array index %d specified for a non-array "
                "register %s\n", array_index, priv_regname);
            return (1);
        }
        reg_addr = reg->GetAddress();
        strcpy(regname, priv_regname);
    }
    else if (dimension == 1)
    {
        // 1-D register array.  Array index in valid.
        reg_addr = reg->GetAddress(array_index);
        // Add the array dimension to the register name if the register is
        // an array.  The register name is used for printing out Info messages
        // to the logfile.
        if (sprintf(regname, "%s(%d)", priv_regname, array_index) < 0)
        {
            ErrPrintf("GC6PlusMisc: Failed to generate array regname\n");
            return (1);
        }
    }
    else
    {
        ErrPrintf("GC6PlusMisc: No support for testing 2-D register arrays\n");
        return (1);
    }

    // Get the read/write mask for the register under test.
    // The read/write test should be performed on bits that can be read and
    // written.  We will get miscompares if we try to do a read/write test on
    // bits that are write_only or read_only.
    rw_mask = reg->GetReadMask() & reg->GetWriteMask();

    // Get the HW init value, if it has one
    if (!reg->GetHWInitValue(&init_data))
    {
        // This register has no HW init value, so any value is just as
        // valid as any other value.  We will use zeros for simplicity.
        init_data = 0;
    }


    // Force the PRIV_LEVEL protection masks to ALL_UNLOCKED prior to starting
    // the test.  We cannot make any assumptions on the HW init value since
    // the GPU security team has changed the HW init value without notification
    // to the affected units.
    write_protection = 0x7;
    write_violation = 0x0;
    read_protection = 0x7;
    read_violation = 0x0;
    if (override_level_mask(priv_mask_regname, write_protection,
        write_violation, read_protection, read_violation))
    {
        ErrPrintf("GC6PlusMisc: Failed to override %s\n", priv_mask_regname);
        return (1);
    }

    // Do a read/write test to verify that CPU can access the register.
    if (read_write_test(regname, reg_addr, rw_mask, init_data,
        write_protection, write_violation, read_protection, read_violation))
    {
        ErrPrintf("GC6PlusMisc: Failure in read/write test on %s\n", regname);
        return (1);
    }

    // Verify that CPU can lock the protection group for itself (LEVEL 0),
    // but not for other levels (LEVEL 1 or 2).  The CPU will try to lock
    // all PRIV levels, but it should succeed only with LEVEL 0.  We will
    // enable write protection only.  Reads will not be protected for now.
    // Note: a value of 1 means access is granted, a value of 0 means
    // access is protected.
    write_protection = 0x0;
    write_violation = 0x1;
    if (cfg_level_mask(priv_mask_regname, write_protection, write_violation,
        read_protection, read_violation))
    {
        ErrPrintf("GC6PlusMisc: Failed to configure %s\n", priv_mask_regname);
        return (1);
    }

    // We should not have been able to lock LEVEL 1 or LEVEL 2, only LEVEL 0.
    // Check that the PRIV_LEVEL_MASK was written correctly.  This is verified
    // by updating the expected write_protection value from 0x0 to 0x6 prior
    // to checking the level mask.
    write_protection = 0x7 & ~(0x1 << PRIV_LEVEL_CPU_BAR0);
    if (chk_level_mask(priv_mask_regname, write_protection, write_violation,
        read_protection, read_violation))
    {
        ErrPrintf("GC6PlusMisc: Failed to check %s\n", priv_mask_regname);
        return (1);
    }

    // Do a read/write test with writes locked.
    if (read_write_test(regname, reg_addr, rw_mask, init_data,
        write_protection, write_violation, read_protection, read_violation))
    {
        ErrPrintf("GC6PlusMisc: Failure in read/write test on %s\n", regname);
        return (1);
    }

    // Lock read accesses.  Attempt to lock reads for all levels, but we
    // know that only locking LEVEL 0 will succeed.
    read_protection = 0x0;
    read_violation = 0x1;
    if (cfg_level_mask(priv_mask_regname, write_protection, write_violation,
        read_protection, read_violation))
    {
        ErrPrintf("GC6PlusMisc: Failed to configure %s\n", priv_mask_regname);
        return (1);
    }

    // Similar to writes, we should not have been able to lock LEVEL 1 or
    // LEVEL 2.
    read_protection = 0x7 & ~(0x1 << PRIV_LEVEL_CPU_BAR0);
    if (chk_level_mask(priv_mask_regname, write_protection, write_violation,
        read_protection, read_violation))
    {
        ErrPrintf("GC6PlusMisc: Failed to check %s\n", priv_mask_regname);
        return (1);
    }

    // Do a read/write test with both reads and writes locked.
    if (read_write_test(regname, reg_addr, rw_mask, init_data,
        write_protection, write_violation, read_protection, read_violation))
    {
        ErrPrintf("GC6PlusMisc: Failure in read/write test on %s\n", regname);
        return (1);
    }

    // Try to unlock PRIV_LEVEL_MASK through CPU priv.  Since the CPU priv is
    // lwrrently locked for reads/writes, we should only be able to update the
    // VIOLATION fields, not the PROTECTION fields.  We will attempt to unlock
    // LEVEL 0 (which should not succeed) and lock all higher LEVELs (again,
    // should not succeed).  The VIOLATION fields are not locked, so writing
    // those should succeed.
    write_protection = (0x1 << PRIV_LEVEL_CPU_BAR0);
    write_violation = 0x0;
    read_protection = (0x1 << PRIV_LEVEL_CPU_BAR0);
    read_violation = 0x1;
    if (cfg_level_mask(priv_mask_regname, write_protection, write_violation,
        read_protection, read_violation))
    {
        ErrPrintf("GC6PlusMisc: Failed to configure %s\n", priv_mask_regname);
        return (1);
    }
    // PROTECTION fields should not have changed.  Restore them to their
    // pre-write value before checking the level_mask.
    write_protection = 0x7 & ~(0x1 << PRIV_LEVEL_CPU_BAR0);
    read_protection = 0x7 & ~(0x1 << PRIV_LEVEL_CPU_BAR0);
    if (chk_level_mask(priv_mask_regname, write_protection, write_violation,
        read_protection, read_violation))
    {
        ErrPrintf("GC6PlusMisc: Failed to check %s\n", priv_mask_regname);
        return (1);
    }

    // Disable all protection masks to give CPU full access once more.
    write_protection = 0x7;
    write_violation = 0x1;
    read_protection = 0x7;
    read_violation = 0x1;
    if (override_level_mask(priv_mask_regname, write_protection,
        write_violation, read_protection, read_violation))
    {
        ErrPrintf("GC6PlusMisc: Failed to override %s\n", priv_mask_regname);
        return (1);
    }

    // CPU priv should have full read/write access.
    if (read_write_test(regname, reg_addr, rw_mask, init_data,
        write_protection, write_violation, read_protection, read_violation))
    {
        ErrPrintf("GC6PlusMisc: Failure in read/write test on %s\n", regname);
        return (1);
    }

    return (0);
}

// This routine performs a read/write test of the register with CPU priv.
// It writes an alternating 01 pattern, followed by an alternating 10 pattern.
// Only the RW bits are tested and checked.  The original value, if applicable,
// is restored at the end of the test.
INT32
GC6PlusMisc::read_write_test (const char *regname, UINT32 reg_addr,
    UINT32 rw_mask, UINT32 init_data, UINT32 wr_protection,
    UINT32 wr_violation, UINT32 rd_protection, UINT32 rd_violation)
{
    UINT32 reg_data, orig_data, exp_data;
    bool wr_locked, rd_locked;

    // Determine whether accesses are locked based on the access interface
    // and protection masks.
    wr_locked = is_locked(PRIV_LEVEL_CPU_BAR0, wr_protection);
    rd_locked = is_locked(PRIV_LEVEL_CPU_BAR0, rd_protection);

    InfoPrintf("GC6PlusMisc: Performing %s read/write test on %s: wr = %s, "
        "rd = %s\n", "CPU_BAR0", regname, wr_locked ? "locked" : "unlocked",
        rd_locked ? "locked" : "unlocked");

    // Read the register and save the original data to restore at the end
    // of the read/write test.  If reads are locked, then assume the original
    // data is the HW init value.
    if (!rd_locked)
    {
        // Reads are not locked, so we can read the original value.
        orig_data = lwgpu->RegRd32(reg_addr);
        InfoPrintf("GC6PlusMisc: Reading %s = 0x%08x\n", regname,
            orig_data);
    }
    else
    {
        orig_data = init_data;
    }
    exp_data = orig_data;


    // write alternating zero/one pattern
    reg_data = 0x55555555 & rw_mask;
    if (write_and_check_reg(regname, reg_addr, reg_data, wr_protection,
        wr_violation))
    {
        ErrPrintf("GC6PlusMisc: Failed to write %s\n", regname);
        return (1);
    }
    // update the expected data if writes are not locked
    if (!wr_locked)
    {
        exp_data = reg_data;
    }

    // Read back what we just wrote and check the results.
    if (read_and_check_reg(regname, reg_addr, exp_data, rw_mask,
        rd_protection, rd_violation))
    {
        ErrPrintf("GC6PlusMisc: Failed to read and check %s\n", regname);
        return (1);
    }

    // write another alternating zero/one pattern
    reg_data = 0xAAAAAAAA & rw_mask;
    if (write_and_check_reg(regname, reg_addr, reg_data, wr_protection,
        wr_violation))
    {
        ErrPrintf("GC6PlusMisc: Failed to write %s\n", regname);
        return (1);
    }
    // update the expected data if writes are not locked
    if (!wr_locked)
    {
        exp_data = reg_data;
    }

    // Read back what we just wrote and check the results.
    if (read_and_check_reg(regname, reg_addr, exp_data, rw_mask,
        rd_protection, rd_violation))
    {
        ErrPrintf("GC6PlusMisc: Failed to read and check %s\n", regname);
        return (1);
    }

    // Restore the HW init value if writes are not locked.
    if (!wr_locked)
    {
        if (write_and_check_reg(regname, reg_addr, orig_data, wr_protection,
            wr_violation))
        {
            ErrPrintf("GC6PlusMisc: Failed to write %s\n", regname);
            return (1);
        }
    }

    InfoPrintf("GC6PlusMisc: Done with %s read/write test on %s: wr = %s, "
        "rd = %s\n", "CPU_BAR0", regname, wr_locked ? "locked" : "unlocked",
        rd_locked ? "locked" : "unlocked");

    return (0);
}


// This routine examines the requesting priv level and level masks and returns
// a boolean whether the register is locked.  Request level 3 cannot be locked.
// Level mask contains bit mask for levels 0, 1, and 2.  There is no mask for
// level 3 since level 3 cannot be locked.  A level mask value of 0 means
// access is locked.  A level mask value of 1 means access is unlocked.
bool
GC6PlusMisc::is_locked (UINT32 req_level, UINT32 level_mask)
{
    bool locked;
    locked = (req_level < 3) && ((~level_mask >> req_level) & 0x1);
    return (locked);
}

// This routine examines the requesting priv level, level masks and violation
// mask and returns a boolean whether a pri error is expected.
bool
GC6PlusMisc::exp_violation (UINT32 req_level, UINT32 level_mask, UINT32
    level_violation)
{
    bool violation;
    violation = level_violation && is_locked (req_level, level_mask);
    return (violation);
}

// This routine pieces together the data for priv error code.  If error
// violation reporting is disabled, it returns zeros.  This is the data
// that a locked register would return as read data.
INT32
GC6PlusMisc::get_error_code (UINT32 req_level, UINT32 level_mask, UINT32
    level_violation, UINT32 *error_code)
{
    UINT32 pri_error_code, level_mask_4bit;

    if (level_violation)
    {
        // Although the error code is in LW_PTIMER_PRI_TIMEOUT_FECS_ERRCODE,
        // its definition is the same as phantom register LW_PPRIV_SYS_PRI.
        // Since the manual does not specify the format for the former
        // register, we will use the later register to interpret the error
        // code value.
        // Note: As of CL 8120516, the phantom register LW_PPRIV_SYS_PRI has
        // been removed.  As a result, we can no longer parse the value of
        // LW_PPRIV_SYS_PRI_ERROR_CODE_FECS_PRI_CLIENT_PRIV_LEVEL_VIOLATION
        // since its field is no longer part of any register.  This is a step
        // backwards, but we have no other choice but to hardcode its value.
        pri_error_code = ERROR_CODE_FECS_PRI_CLIENT_PRIV_LEVEL_VIOLATION;

        // The 3-bit level_mask needs to be colwerted to a 4-bit level_mask.
        // Level 3 mask bit is implied 1 and cannot be locked.
        level_mask_4bit = (1 << 3) | level_mask;

        // These fields are not defined in the manual.  The field positions
        // are specified in bug 744259 in a 1/29/2011 post.  They have to be
        // hardcoded because they are not defined in any manual.
        *error_code = (pri_error_code << 8) |
                      (req_level << 4) |
                      (level_mask_4bit << 0);
    }
    else
    {
        // Priv errors are silently dropped, so read data is all zeros
        *error_code = 0;
    }

    return (0);
}

// This routine checks and clears the PRI_FECSERR.  The FECS pri error is
// expected on non immediate_ack read and write protected registers.  It also
// checks the case where a violation is reported when none is expected.
INT32
GC6PlusMisc::check_pri_fecserr (UINT32 addr, UINT32 write, UINT32 wr_data,
    UINT32 req_level, UINT32 level_mask, UINT32 level_violation)
{
    unique_ptr<IRegisterClass> reg;
    unique_ptr<IRegisterField> field;
    unique_ptr<IRegisterValue> value;
    UINT32 reg_addr, reg_data;
    const char *regname, *fieldname, *valuename;
    UINT32 act_pri_fecserr, exp_pri_fecserr;
    UINT32 act_fecs_errcode, exp_fecs_errcode;
    UINT32 act_save_0_fecs_tgt, exp_save_0_fecs_tgt;
    UINT32 act_save_0_addr, exp_save_0_addr;
    UINT32 act_save_0_write, exp_save_0_write;
    UINT32 act_save_0_to, exp_save_0_to;
    UINT32 act_save_1_wr_data, exp_save_1_wr_data;

    // Look for an interrupt in LW_PBUS_INTR_0_PRI_FECSERR
    regname = "LW_PBUS_INTR_0";
    if (!(reg = m_regMap->FindRegister(regname)))
    {
        ErrPrintf("GC6PlusMisc: Failed to find %s register\n", regname);
        return (1);
    }
    reg_addr = reg->GetAddress();
    reg_data = lwgpu->RegRd32(reg_addr);
    InfoPrintf("GC6PlusMisc: Reading %s = 0x%08x\n", reg->GetName(), reg_data);

    fieldname = "LW_PBUS_INTR_0_PRI_FECSERR";
    if (!(field = reg->FindField(fieldname)))
    {
        ErrPrintf("GC6PlusMisc: Failed to find %s field\n", fieldname);
        return (1);
    }
    act_pri_fecserr = (reg_data & field->GetMask()) >> field->GetStartBit();

    // Examine the requesting priv level and level masks to see if we expect
    // a violation to be reported or not.
    if (exp_violation(req_level, level_mask, level_violation))
    {
        valuename = "LW_PBUS_INTR_0_PRI_FECSERR_PENDING";
    }
    else
    {
        valuename = "LW_PBUS_INTR_0_PRI_FECSERR_NOT_PENDING";
    }
    if (!(value = field->FindValue(valuename)))
    {
        ErrPrintf("GC6PlusMisc: Failed to find %s value\n", valuename);
        return (1);
    }
    exp_pri_fecserr = value->GetValue();

    if (act_pri_fecserr != exp_pri_fecserr)
    {
        ErrPrintf("GC6PlusMisc: Mismatch in %s: expected = %d, actual = %d\n",
            field->GetName(), exp_pri_fecserr, act_pri_fecserr);
        m_ErrCount++;
    }

    // continue with pri_error processing if one was detected
    valuename = "LW_PBUS_INTR_0_PRI_FECSERR_NOT_PENDING";
    if (!(value = field->FindValue(valuename)))
    {
        ErrPrintf("GC6PlusMisc: Failed to find %s value\n", valuename);
        return (1);
    }
    if (act_pri_fecserr == value->GetValue())
    {
        // PRI_FECSERR is not pending, so no need to continue processing.
        return (0);
    }


    // Found PRI_FECSERR, so we need to check the details of the priv
    // transaction and clear the error.
    InfoPrintf("GC6PlusMisc: Found PRI_FECSERR, checking FECS_ERRCODE\n");

    // check error code
    regname = "LW_PTIMER_PRI_TIMEOUT_FECS_ERRCODE";
    if (!(reg = m_regMap->FindRegister(regname)))
    {
        ErrPrintf("GC6PlusMisc: Failed to find %s register\n", regname);
        return (1);
    }
    reg_addr = reg->GetAddress();
    reg_data = lwgpu->RegRd32(reg_addr);
    InfoPrintf("GC6PlusMisc: Reading %s = 0x%08x\n", reg->GetName(), reg_data);
    act_fecs_errcode = reg_data;

    // Get the expected error code.
    if (get_error_code(req_level, level_mask, level_violation,
        &exp_fecs_errcode))
    {
        ErrPrintf("GC6PlusMisc: Failure in getting priv error code\n");
        return (1);
    }

    if (act_fecs_errcode != exp_fecs_errcode)
    {
        ErrPrintf("GC6PlusMisc: Mismatch in %s: expected = 0x%08x, actual = "
            "0x%08x\n", regname, exp_fecs_errcode, act_fecs_errcode);
        m_ErrCount++;
    }

    // Check the fields in LW_PTIMER_PRI_TIMEOUT_SAVE_0
    regname = "LW_PTIMER_PRI_TIMEOUT_SAVE_0";
    if (!(reg = m_regMap->FindRegister(regname)))
    {
        ErrPrintf("GC6PlusMisc: Failed to find %s register\n", regname);
        return (1);
    }
    reg_addr = reg->GetAddress();
    reg_data = lwgpu->RegRd32(reg_addr);
    InfoPrintf("GC6PlusMisc: Reading %s = 0x%08x\n", reg->GetName(), reg_data);

    // Check SAVE_0_FECS_TGT
    regname = "LW_PTIMER_PRI_TIMEOUT_SAVE_0_FECS_TGT";
    if (!(field = reg->FindField(regname)))
    {
        ErrPrintf("GC6PlusMisc: Failed to find %s field\n", regname);
        return (1);
    }
    // We detected a FECS error, therefore, the priv access must have gone
    // through FECS.
    exp_save_0_fecs_tgt = 1;
    act_save_0_fecs_tgt = (reg_data & field->GetMask()) >> field->GetStartBit();
    if (act_save_0_fecs_tgt != exp_save_0_fecs_tgt)
    {
        ErrPrintf("GC6PlusMisc: Mismatch in %s: expected = %d, actual = %d\n",
            field->GetName(), exp_save_0_fecs_tgt, act_save_0_fecs_tgt);
        m_ErrCount++;
    }

    // Check SAVE_0_ADDR
    regname = "LW_PTIMER_PRI_TIMEOUT_SAVE_0_ADDR";
    if (!(field = reg->FindField(regname)))
    {
        ErrPrintf("GC6PlusMisc: Failed to find %s field\n", regname);
        return (1);
    }
    // The saved address is a word (not byte) address.
    exp_save_0_addr = (addr >> 2);  // colwert byte to word addr
    act_save_0_addr = (reg_data & field->GetMask()) >> field->GetStartBit();
    if (act_save_0_addr != exp_save_0_addr)
    {
        ErrPrintf("GC6PlusMisc: Mismatch in %s: expected = 0x%08x, actual = "
            "0x%08x\n", field->GetName(), exp_save_0_addr, act_save_0_addr);
        m_ErrCount++;
    }

    // Check SAVE_0_WRITE
    regname = "LW_PTIMER_PRI_TIMEOUT_SAVE_0_WRITE";
    if (!(field = reg->FindField(regname)))
    {
        ErrPrintf("GC6PlusMisc: Failed to find %s field\n", regname);
        return (1);
    }
    exp_save_0_write = write;
    act_save_0_write = (reg_data & field->GetMask()) >> field->GetStartBit();
    if (act_save_0_write != exp_save_0_write)
    {
        ErrPrintf("GC6PlusMisc: Mismatch in %s: expected = %d, actual = %d\n",
            field->GetName(), exp_save_0_write, act_save_0_write);
        m_ErrCount++;
    }

    // Check SAVE_0_TO
    regname = "LW_PTIMER_PRI_TIMEOUT_SAVE_0_TO";
    if (!(field = reg->FindField(regname)))
    {
        ErrPrintf("GC6PlusMisc: Failed to find %s field\n", regname);
        return (1);
    }
    // This field indicates a PRI timeout, FECS error, or SQUASH.  We
    // expect an FECS error, so the expected value is 1.
    exp_save_0_to = 1;
    act_save_0_to = (reg_data & field->GetMask()) >> field->GetStartBit();
    if (act_save_0_to != exp_save_0_to)
    {
        ErrPrintf("GC6PlusMisc: Mismatch in %s: expected = %d, actual = %d\n",
            field->GetName(), exp_save_0_to, act_save_0_to);
        m_ErrCount++;
    }

    // Check the fields in LW_PTIMER_PRI_TIMEOUT_SAVE_1
    regname = "LW_PTIMER_PRI_TIMEOUT_SAVE_1";
    if (!(reg = m_regMap->FindRegister(regname)))
    {
        ErrPrintf("GC6PlusMisc: Failed to find %s register\n", regname);
        return (1);
    }
    reg_addr = reg->GetAddress();
    reg_data = lwgpu->RegRd32(reg_addr);
    InfoPrintf("GC6PlusMisc: Reading %s = 0x%08x\n", reg->GetName(), reg_data);

    // This register contains the write data if the transaction was a
    // write.  Otherwise, it contains zeros.
    if (write)
    {
        exp_save_1_wr_data = wr_data;
    }
    else
    {
        exp_save_1_wr_data = 0;
    }
    act_save_1_wr_data = reg_data;
    if (act_save_1_wr_data != exp_save_1_wr_data)
    {
        ErrPrintf("GC6PlusMisc: Mismatch in %s: expected = 0x%08x, actual = "
            "0x%08x\n", reg->GetName(), exp_save_1_wr_data, act_save_1_wr_data);
        m_ErrCount++;
    }

    if (clear_pri_fecserr())
    {
        ErrPrintf("GC6PlusMisc: Failure in clearing pri_fecserr\n");
        return (1);
    }

    return (0);
}


// This routine clears the pri_fecserr.
INT32 GC6PlusMisc::clear_pri_fecserr()
{
    unique_ptr<IRegisterClass> reg;
    unique_ptr<IRegisterField> field;
    unique_ptr<IRegisterValue> value;
    UINT32 reg_addr, reg_data;
    const char *regname, *fieldname, *valuename;
    UINT32 index, pri_error_size;

    InfoPrintf("GC6PlusMisc: Clearing PRI_FECSERR\n");

    // Write zeros to the following registers:
    //  LW_PTIMER_PRI_TIMEOUT_SAVE_0
    //  LW_PTIMER_PRI_TIMEOUT_SAVE_1
    //  LW_PTIMER_PRI_TIMEOUT_FECS_ERRCODE
    const char *pri_error_regname[] = {"LW_PTIMER_PRI_TIMEOUT_SAVE_0",
                                       "LW_PTIMER_PRI_TIMEOUT_SAVE_1",
                                       "LW_PTIMER_PRI_TIMEOUT_FECS_ERRCODE"};
    pri_error_size = 3;

    for (index = 0; index < pri_error_size; index = index + 1)
    {
        if (!(reg = m_regMap->FindRegister(pri_error_regname[index])))
        {
            ErrPrintf("GC6PlusMisc: Failed to find %s register\n",
                pri_error_regname[index]);
            return (1);
        }
        reg_addr = reg->GetAddress();
        reg_data = 0;
        lwgpu->RegWr32(reg_addr, reg_data);
        InfoPrintf("GC6PlusMisc: Writing %s = 0x%08x\n", reg->GetName(),
            reg_data);
    }

    // Then clear the interrupt in:
    //  LW_PBUS_INTR_0_PRI_FECSERR
    regname = "LW_PBUS_INTR_0";
    if (!(reg = m_regMap->FindRegister(regname)))
    {
        ErrPrintf("GC6PlusMisc: Failed to find %s register\n", regname);
        return (1);
    }
    reg_addr = reg->GetAddress();
    fieldname = "LW_PBUS_INTR_0_PRI_FECSERR";
    if (!(field = reg->FindField(fieldname)))
    {
        ErrPrintf("GC6PlusMisc: Failed to find %s field\n", fieldname);
        return (1);
    }
    valuename = "LW_PBUS_INTR_0_PRI_FECSERR_RESET";
    if (!(value = field->FindValue(valuename)))
    {
        ErrPrintf("GC6PlusMisc: Failed to find %s value\n", valuename);
        return (1);
    }
    reg_data = value->GetValue() << field->GetStartBit();
    lwgpu->RegWr32(reg_addr, reg_data);
    InfoPrintf("GC6PlusMisc: Writing %s = 0x%08x\n", reg->GetName(), reg_data);

    return (0);
}


// This routine checks and clears the SYS_PRIV_ERROR.  The SYS pri error
// is expected on immediate_ack write protected registers.  It also checks
// the case where a violation is reported when none is expected.
INT32
GC6PlusMisc::check_sys_priv_error (UINT32 addr, UINT32 write, UINT32 wr_data,
    UINT32 req_level, UINT32 level_mask, UINT32 level_violation)
{
    unique_ptr<IRegisterClass> reg;
    unique_ptr<IRegisterField> field;
    const char *regname, *fieldname;
    UINT32 reg_addr, reg_data;
    UINT32 act_priv_error_code, exp_priv_error_code;
    UINT32 act_priv_error_level, exp_priv_error_level;
    UINT32 act_priv_error_adr, exp_priv_error_adr;
    UINT32 act_priv_error_wrdat, exp_priv_error_wrdat;
    INT32 rc;

    // Note that we are looking here for priv errors on immediate_ack writes.
    // Because the write was ack immediately, the priv error may not be in the
    // status register when we read it (that is the nature of immediate_ack
    // transactions).  We need to guarantee that the write completes
    // before we read the status register.  Unfortunately, there is no
    // architectural way to guarantee it without forcing a non immediate_ack
    // write.  This is not ideal, but we have to resort to polling.
    fieldname = "LW_PPRIV_MASTER_RING_INTERRUPT_STATUS0_GBL_WRITE_ERROR_SYS";
    rc = poll_reg_field("LW_PPRIV_MASTER_RING_INTERRUPT_STATUS0",
        "GBL_WRITE_ERROR_SYS", 1, true, m_ring_intr_poll_timeout);
    if (rc == 1)
    {
        ErrPrintf("GC6PlusMisc: Error in polling ring interrupt status\n");
        return (1);
    }
    else if (rc == -1)
    {
        // No GBL_WRITE_ERROR found
        if (exp_violation(req_level, level_mask, level_violation))
        {
            ErrPrintf("GC6PlusMisc: Mismatch in %s: expected = %d, actual "
                "= %d\n", fieldname, 1, 0);
            m_ErrCount++;
        }
        // GBL_WRITE_ERROR is not pending, so no need to clear error
        return (0);
    }
    else
    {
        // GBL_WRITE_ERROR found
        if (!exp_violation(req_level, level_mask, level_violation))
        {
            ErrPrintf("GC6PlusMisc: Mismatch in %s: expected = %d, actual "
                "= %d\n", fieldname, 0, 1);
            m_ErrCount++;
        }
    }


    // Found GBL_WRITE_ERROR, so we need to check the details of the priv
    // transaction and clear the error.
    InfoPrintf("GC6PlusMisc: Found %s, checking %s registers\n", fieldname,
        "LW_PPRIV_SYS_PRIV_ERROR_*");

    // check error code
    regname = "LW_PPRIV_SYS_PRIV_ERROR_CODE";
    if (!(reg = m_regMap->FindRegister(regname)))
    {
        ErrPrintf("GC6PlusMisc: Failed to find %s register\n", regname);
        return (1);
    }
    reg_addr = reg->GetAddress();
    reg_data = lwgpu->RegRd32(reg_addr);
    InfoPrintf("GC6PlusMisc: Reading %s = 0x%08x\n", reg->GetName(), reg_data);
    act_priv_error_code = reg_data;

    // Get the expected error code.
    if (get_error_code(req_level, level_mask, level_violation,
        &exp_priv_error_code))
    {
        ErrPrintf("GC6PlusMisc: Failure in getting priv error code\n");
        return (1);
    }

    if (act_priv_error_code != exp_priv_error_code)
    {
        ErrPrintf("GC6PlusMisc: Mismatch in %s: expected = 0x%08x, actual = "
            "0x%08x\n", regname, exp_priv_error_code, act_priv_error_code);
        m_ErrCount++;
    }

    // Check the fields in LW_PPRIV_SYS_PRIV_ERROR_ADR
    regname = "LW_PPRIV_SYS_PRIV_ERROR_ADR";
    if (!(reg = m_regMap->FindRegister(regname)))
    {
        ErrPrintf("GC6PlusMisc: Failed to find %s register\n", regname);
        return (1);
    }
    reg_addr = reg->GetAddress();
    reg_data = lwgpu->RegRd32(reg_addr);
    InfoPrintf("GC6PlusMisc: Reading %s = 0x%08x\n", reg->GetName(), reg_data);

    // Check LW_PPRIV_SYS_PRIV_ERROR_ADR_ADDRESS
    fieldname = "LW_PPRIV_SYS_PRIV_ERROR_ADR_ADDRESS";
    if (!(field = reg->FindField(fieldname)))
    {
        ErrPrintf("GC6PlusMisc: Failed to find %s field\n", fieldname);
        return (1);
    }
    // The saved address is a word address.
    exp_priv_error_adr = addr;
    act_priv_error_adr = (reg_data & field->GetMask()) >> field->GetStartBit();
    if (act_priv_error_adr != exp_priv_error_adr)
    {
        ErrPrintf("GC6PlusMisc: Mismatch in %s: expected = 0x%08x, actual = 0x"
            "%08x\n", field->GetName(), exp_priv_error_adr, act_priv_error_adr);
        m_ErrCount++;
    }

    // Check the LW_PPRIV_SYS_PRIV_ERROR_WRDAT field only if it was a write
    if (write)
    {
        regname = "LW_PPRIV_SYS_PRIV_ERROR_WRDAT";
        if (!(reg = m_regMap->FindRegister("LW_PPRIV_SYS_PRIV_ERROR_WRDAT")))
        {
            ErrPrintf("GC6PlusMisc: Failed to find %s register\n", regname);
            return (1);
        }
        reg_addr = reg->GetAddress();
        reg_data = lwgpu->RegRd32(reg_addr);
        InfoPrintf("GC6PlusMisc: Reading %s = 0x%08x\n", reg->GetName(),
            reg_data);

        exp_priv_error_wrdat = wr_data;
        act_priv_error_wrdat = reg_data;
        if (act_priv_error_wrdat != exp_priv_error_wrdat)
        {
            ErrPrintf("GC6PlusMisc: Mismatch in %s: expected = 0x%08x, actual "
                "= 0x%08x\n", reg->GetName(), exp_priv_error_wrdat,
                act_priv_error_wrdat);
            m_ErrCount++;
        }
    }

    // Check the fields in LW_PPRIV_SYS_PRIV_ERROR_INFO.  We are interested
    // in the PRIV_LEVEL field only.  The other fields (SUBID, LOCAL_ORDERING,
    // SENDING_RS, ORPHAN, PRIV_MASTER, and PENDING) are beyond the scope of
    // this test.
    regname = "LW_PPRIV_SYS_PRIV_ERROR_INFO";
    if (!(reg = m_regMap->FindRegister(regname)))
    {
        ErrPrintf("GC6PlusMisc: Failed to find %s register\n", regname);
        return (1);
    }
    reg_addr = reg->GetAddress();
    reg_data = lwgpu->RegRd32(reg_addr);
    InfoPrintf("GC6PlusMisc: Reading %s = 0x%08x\n", reg->GetName(), reg_data);

    // Check LW_PPRIV_SYS_PRIV_ERROR_INFO_PRIV_LEVEL
    fieldname = "LW_PPRIV_SYS_PRIV_ERROR_INFO_PRIV_LEVEL";
    if (!(field = reg->FindField(fieldname)))
    {
        ErrPrintf("GC6PlusMisc: Failed to find %s field\n", fieldname);
        return (1);
    }
    exp_priv_error_level = req_level;
    act_priv_error_level = (reg_data & field->GetMask()) >>
        field->GetStartBit();
    if (act_priv_error_level != exp_priv_error_level)
    {
        ErrPrintf("GC6PlusMisc: Mismatch in %s: expected = %d, actual = %d\n",
            field->GetName(), exp_priv_error_level, act_priv_error_level);
        m_ErrCount++;
    }

    // clear pri_error
    if (clear_sys_priv_error())
    {
        ErrPrintf("GC6PlusMisc: Failure in clearing sys_priv_error\n");
        return (1);
    }

    return (0);
}


// This routine clears the sys_priv_error.
INT32 GC6PlusMisc::clear_sys_priv_error()
{
    unique_ptr<IRegisterClass> reg;
    unique_ptr<IRegisterField> field;
    unique_ptr<IRegisterValue> value;
    UINT32 reg_addr, reg_data;
    const char *regname, *fieldname, *valuename;

    InfoPrintf("GC6PlusMisc: Clearing SYS_PRIV_ERROR\n");

    // Make sure no RING_COMMAND is in progress before we trigger a new
    // ring command.
    if (poll_for_ring_command_done(m_max_polling_attempts))
    {
        ErrPrintf("GC6PlusMisc: Failure in polling RING_COMMAND\n");
        return (1);
    }

    // Sending a RING_COMMAND_ACK_INTERRUPT clears the interrupt in
    // LW_PPRIV_MASTER_RING_INTERRUPT_STATUS0.
    regname = "LW_PPRIV_MASTER_RING_COMMAND";
    if (!(reg = m_regMap->FindRegister(regname)))
    {
        ErrPrintf("GC6PlusMisc: Failed to find %s register\n", regname);
        return (1);
    }
    reg_addr = reg->GetAddress();

    fieldname = "LW_PPRIV_MASTER_RING_COMMAND_CMD";
    if (!(field = reg->FindField(fieldname)))
    {
        ErrPrintf("GC6PlusMisc: Failed to find %s field\n", fieldname);
        return (1);
    }
    valuename = "LW_PPRIV_MASTER_RING_COMMAND_CMD_ACK_INTERRUPT";
    if (!(value = field->FindValue(valuename)))
    {
        ErrPrintf("GC6PlusMisc: Failed to find %s value\n", valuename);
        return (1);
    }
    reg_data = value->GetValue() << field->GetStartBit();

    lwgpu->RegWr32(reg_addr, reg_data);
    InfoPrintf("GC6PlusMisc: Writing %s = 0x%08x\n", reg->GetName(), reg_data);

    // Make sure RING_COMMAND completes before moving on in the test.
    if (poll_for_ring_command_done(m_max_polling_attempts))
    {
        ErrPrintf("GC6PlusMisc: Failure in polling RING_COMMAND\n");
        return (1);
    }

    return (0);
}


// This routine overrides the PRIV_LEVEL_MASK.
INT32
GC6PlusMisc::override_level_mask(const char *regname, UINT32 wr_protection,
    UINT32 wr_violation, UINT32 rd_protection, UINT32 rd_violation)
{
    InfoPrintf("GC6PlusMisc: Overriding %s protection masks\n", regname);

    // Force the priv security level to disable
    if (set_priv_selwrity(false))
    {
        ErrPrintf("GC6PlusMisc: Failure in configuring priv security "
            "override\n");
        return (1);
    }

    // Now we can freely write the protection level masks
    if (cfg_level_mask(regname, wr_protection, wr_violation,
        rd_protection, rd_violation))
    {
        ErrPrintf("GC6PlusMisc: Failed to configure %s\n", regname);
        return (1);
    }
    // Add a readback check to flush the write before setting the fuse.
    if (chk_level_mask(regname, wr_protection, wr_violation,
        rd_protection, rd_violation))
    {
        ErrPrintf("GC6PlusMisc: Failed to check %s\n", regname);
        return (1);
    }

    // Enable the priv security level and resume testing
    if (set_priv_selwrity(true))
    {
        ErrPrintf("GC6PlusMisc: Failure in configuring priv security "
            "override\n");
        return (1);
    }

    return (0);
}

// This routine configures the PRIV_LEVEL_MASK.
INT32
GC6PlusMisc::cfg_level_mask (const char *regname, UINT32 wr_protection,
    UINT32 wr_violation, UINT32 rd_protection, UINT32 rd_violation)
{
    unique_ptr<IRegisterClass> reg;
    unique_ptr<IRegisterField> field;
    char tempname[127];
    UINT32 reg_addr, reg_data;

    InfoPrintf("GC6PlusMisc: Configuring %s through %s\n", regname, "CPU_BAR0");

    // First, find the PRIV_LEVEL register.
    if (!(reg = m_regMap->FindRegister(regname)))
    {
        ErrPrintf("GC6PlusMisc: Failed to find %s register\n", regname);
        return (1);
    }
    reg_addr = reg->GetAddress();

    // Second, piece together the WRITE/READ PROTECTION/VIOLATION values to
    // form the register write data.
    strcpy(tempname, reg->GetName());
    strcat(tempname, "_WRITE_PROTECTION");
    if (!(field = reg->FindField(tempname)))
    {
        ErrPrintf("GC6PlusMisc: Failed to find %s field\n", tempname);
        return (1);
    }
    reg_data = ((wr_protection << field->GetStartBit()) & field->GetMask());

    strcpy(tempname, reg->GetName());
    strcat(tempname, "_WRITE_VIOLATION");
    if (!(field = reg->FindField(tempname)))
    {
        ErrPrintf("GC6PlusMisc: Failed to find %s field\n", tempname);
        return (1);
    }
    reg_data |= ((wr_violation << field->GetStartBit()) & field->GetMask());

    strcpy(tempname, reg->GetName());
    strcat(tempname, "_READ_PROTECTION");
    if (!(field = reg->FindField(tempname)))
    {
        ErrPrintf("GC6PlusMisc: Failed to find %s field\n", tempname);
        return (1);
    }
    reg_data |= ((rd_protection << field->GetStartBit()) & field->GetMask());

    strcpy(tempname, reg->GetName());
    strcat(tempname, "_READ_VIOLATION");
    if (!(field = reg->FindField(tempname)))
    {
        ErrPrintf("GC6PlusMisc: Failed to find %s field\n", tempname);
        return (1);
    }
    reg_data |= ((rd_violation << field->GetStartBit()) & field->GetMask());

    // Lastly, generate the write requests.
    lwgpu->RegWr32(reg_addr, reg_data);
    InfoPrintf("GC6PlusMisc: Writing %s = 0x%08x\n", reg->GetName(), reg_data);

    return (0);
}


// This routine checks the PRIV_LEVEL_MASK for mismatches.
INT32
GC6PlusMisc::chk_level_mask (const char *regname, UINT32 wr_protection,
    UINT32 wr_violation, UINT32 rd_protection, UINT32 rd_violation)
{
    unique_ptr<IRegisterClass> reg;
    unique_ptr<IRegisterField> field;
    UINT32 reg_addr, reg_data;
    char tempname[127];
    UINT32 field_value;

    InfoPrintf("GC6PlusMisc: Checking priv level mask %s\n", regname);

    // Find the PRIV_LEVEL register and read its value
    if (!(reg = m_regMap->FindRegister(regname)))
    {
        ErrPrintf("GC6PlusMisc: Failed to find %s register\n", regname);
        return (1);
    }
    reg_addr = reg->GetAddress();
    reg_data = lwgpu->RegRd32(reg_addr);
    InfoPrintf("GC6PlusMisc: Reading %s = 0x%08x\n", reg->GetName(), reg_data);

    // Verify all fields in PRIV_LEVEL_MASK register
    strcpy(tempname, reg->GetName());
    strcat(tempname, "_WRITE_PROTECTION");
    if (!(field = reg->FindField(tempname)))
    {
        ErrPrintf("GC6PlusMisc: Failed to find %s field\n", tempname);
        return (1);
    }
    field_value = (reg_data & field->GetMask()) >> field->GetStartBit();
    if (field_value != wr_protection)
    {
        ErrPrintf("GC6PlusMisc: Mismatch in %s: expected = %d, actual = %d\n",
            field->GetName(), wr_protection, field_value);
        m_ErrCount++;
    }

    strcpy(tempname, reg->GetName());
    strcat(tempname, "_WRITE_VIOLATION");
    if (!(field = reg->FindField(tempname)))
    {
        ErrPrintf("GC6PlusMisc: Failed to find %s field\n", tempname);
        return (1);
    }
    field_value = (reg_data & field->GetMask()) >> field->GetStartBit();
    if (field_value != wr_violation)
    {
        ErrPrintf("GC6PlusMisc: Mismatch in %s: expected = %d, actual = %d\n",
            field->GetName(), wr_violation, field_value);
        m_ErrCount++;
    }

    strcpy(tempname, reg->GetName());
    strcat(tempname, "_READ_PROTECTION");
    if (!(field = reg->FindField(tempname)))
    {
        ErrPrintf("GC6PlusMisc: Failed to find %s field\n", tempname);
        return (1);
    }
    field_value = (reg_data & field->GetMask()) >> field->GetStartBit();
    if (field_value != rd_protection)
    {
        ErrPrintf("GC6PlusMisc: Mismatch in %s: expected = %d, actual = %d\n",
            field->GetName(), rd_protection, field_value);
        m_ErrCount++;
    }

    strcpy(tempname, reg->GetName());
    strcat(tempname, "_READ_VIOLATION");
    if (!(field = reg->FindField(tempname)))
    {
        ErrPrintf("GC6PlusMisc: Failed to find %s field\n", tempname);
        return (1);
    }
    field_value = (reg_data & field->GetMask()) >> field->GetStartBit();
    if (field_value != rd_violation)
    {
        ErrPrintf("GC6PlusMisc: Mismatch in %s: expected = %d, actual = %d\n",
            field->GetName(), rd_violation, field_value);
        m_ErrCount++;
    }

    return (0);
}


// This routine reads data through CPU priv and checks against the expected
// data.  If the register is locked for read access, then the read data is
// compared to zeros.
INT32
GC6PlusMisc::read_and_check_reg (const char *regname, UINT32 reg_addr,
    UINT32 exp_data, UINT32 rw_mask, UINT32 level_mask, UINT32 level_violation)
{
    UINT32 reg_data, error_code;

    // Check that level_mask value is valid.
    if (level_mask > 7)
    {
        ErrPrintf("GC6PlusMisc: Detected invalid level_mask = %d.  Valid "
            "range is (0..7)\n", level_mask);
        return (1);
    }

    // Read the register
    reg_data = lwgpu->RegRd32(reg_addr);
    InfoPrintf("GC6PlusMisc: Reading %s = 0x%08x\n", regname, reg_data);

    // compare actual data against expected data
    if (is_locked(PRIV_LEVEL_CPU_BAR0, level_mask))
    {
        // get the error code for a locked register
        if (get_error_code(PRIV_LEVEL_CPU_BAR0, level_mask, level_violation,
            &error_code))
        {
            ErrPrintf("GC6PlusMisc: Failure in getting priv error code\n");
            return (1);
        }
        // reads were locked, so expected data should match the error code
        if (reg_data != error_code)
        {
            ErrPrintf("GC6PlusMisc: Mismatch in locked %s: expected = 0x%08x, "
                "actual = 0x%08x\n", regname, error_code, reg_data);
            m_ErrCount++;
        }
    }
    else
    {
        // reads were unlocked, so compare actual data against expected data
        // check just the read/write bits since read only bits may not match
        if ((reg_data & rw_mask) != (exp_data & rw_mask))
        {
            ErrPrintf("GC6PlusMisc: Mismatch in %s: expected = 0x%08x, "
                "actual = 0x%08x, rw_mask = 0x%08x\n", regname, exp_data,
                reg_data, rw_mask);
            m_ErrCount++;
        }
    }

    // Check if the read generated a pri error violation or not.  For
    // non immediate_ack accesses, we expect the error in PRI_FECSERR.
    // Reads are never immediate_ack accesses.
    if (check_pri_fecserr(reg_addr, 0, 0, PRIV_LEVEL_CPU_BAR0, level_mask,
        level_violation))
    {
        ErrPrintf("GC6PlusMisc: Failure in checking FECS_PRIV error\n");
        return (1);
    }
    Platform::Delay(10); 
    
    return (0);
}


// This routine writes data through CPU priv.  If the register is locked and
// a write violation is expected to be reported, that error is checked and
// cleared.
INT32
GC6PlusMisc::write_and_check_reg (const char *regname, UINT32 reg_addr,
    UINT32 wr_data, UINT32 level_mask, UINT32 level_violation)
{
    // Check that level_mask value is valid.
    if (level_mask > 7)
    {
        ErrPrintf("GC6PlusMisc: Detected invalid level_mask = %d.  Valid "
            "range is (0..7)\n", level_mask);
        return (1);
    }

    // Write the register
    lwgpu->RegWr32(reg_addr, wr_data);
    InfoPrintf("GC6PlusMisc: Writing %s = 0x%08x\n", regname, wr_data);

    // Check if the write generated a pri error violation or not.  Writes can
    // be immediate_ack or not.  Non immediate_ack writes are expected in
    // PRI_FECSERR, while immediate_ack writes are expected in SYS_PRIV_ERROR.
    if (m_cpu_bar0_immediate_ack)
    {
        if (check_sys_priv_error(reg_addr, 1, wr_data, PRIV_LEVEL_CPU_BAR0,
            level_mask, level_violation))
        {
            ErrPrintf("GC6PlusMisc: Failure in checking SYS_PRIV error\n");
            return (1);
        }
    }
    else
    {
        if (check_pri_fecserr(reg_addr, 1, wr_data, PRIV_LEVEL_CPU_BAR0,
            level_mask, level_violation))
        {
            ErrPrintf("GC6PlusMisc: Failure in checking FECS_PRIV error\n");
            return (1);
        }
    }
    Platform::Delay(10); 
    
    return (0);
}


// This routine disables priv error interupts to RM.
INT32
GC6PlusMisc::disable_priv_err_intr()
{
    unique_ptr<IRegisterClass> reg;
    unique_ptr<IRegisterField> field;
    unique_ptr<IRegisterValue> value;
    const char *regname, *fieldname, *valuename;
    UINT32 reg_addr, reg_data, reg_index;
    UINT32 pri_fecserr;

    // Starting GP100, the register to mask the interrupt is
    // LW_PMC_INTR_EN_CLEAR(0).  Fields in this register are not defined,
    // but mirrors the fields in LW_PMC_INTR(0) register.
    if (m_regMap->FindRegister("LW_PMC_INTR_EN_CLEAR"))
    {
        // We need to get the PRIV_RING field position from LW_PMC_INTR
        regname = "LW_PMC_INTR";
        reg = m_regMap->FindRegister(regname);
        if (!reg)
        {
            ErrPrintf("GC6PlusMisc: Failed to find %s register\n", regname);
            return (1);
        }
        fieldname = "LW_PMC_INTR_PRIV_RING";
        field = reg->FindField(fieldname);
        if (!field)
        {
            ErrPrintf("GC6PlusMisc: Failed to find %s field\n", fieldname);
            return (1);
        }
        // Writing 1 to LW_PMC_INTR_EN_CLEAR(i) fields clears the INTR_EN mask
        reg_data = (1 << field->GetStartBit());

        // Now write that data to the INTR_EN_CLEAR register
        regname = "LW_PMC_INTR_EN_CLEAR";
        reg = m_regMap->FindRegister(regname);
        if (!reg)
        {
            ErrPrintf("GC6PlusMisc: Failed to find %s register\n", regname);
            return (1);
        }
        reg_index = 0;  // stalling interrupts are at index 0
        reg_addr = reg->GetAddress(reg_index);
        lwgpu->RegWr32(reg_addr, reg_data);
        InfoPrintf("GC6PlusMisc: Writing %s(%d) = 0x%08x\n",
            reg->GetName(), reg_index, reg_data);
    }
    else
    {
        // the interrupt mask is at LW_PMC_INTR_MSK_0_PRIV_RING
        // HACK: manuals were upated to make this register a phantom.  As such,
        // MODS cannot find this register.  We have to hardcode the values.
        reg_addr = LW_PMC_INTR_MSK_0_ADDR; // LW_PMC_INTR_MSK_0
        reg_data = lwgpu->RegRd32(reg_addr); // LW_PMC_INTR_MSK_0
        InfoPrintf("GC6PlusMisc: Reading LW_PMC_INTR_MSK_0 = 0x%08x\n",
            reg_data);

        // Disable LW_PMC_INTR_MSK_0_PRIV_RING interrupt
        // Write LW_PMC_INTR_MSK_0_PRIV_RING to _DISABLED, bit[30]
        reg_data = reg_data & ~(1 << 30);
        lwgpu->RegWr32(reg_addr, reg_data);
        InfoPrintf("GC6PlusMisc: Writing LW_PMC_INTR_MSK_0 = 0x%08x\n",
            reg_data);
    }


    // the interrupt mask is at LW_PBUS_INTR_EN_0_PRI_FECSERR
    regname = "LW_PBUS_INTR_EN_0";
    if (!(reg = m_regMap->FindRegister(regname)))
    {
        ErrPrintf("GC6PlusMisc: Failed to find %s register\n", regname);
        return (1);
    }
    reg_addr = reg->GetAddress();
    reg_data = lwgpu->RegRd32(reg_addr);
    InfoPrintf("GC6PlusMisc: Reading %s = 0x%08x\n", reg->GetName(), reg_data);

    fieldname = "LW_PBUS_INTR_EN_0_PRI_FECSERR";
    if (!(field = reg->FindField(fieldname)))
    {
        ErrPrintf("GC6PlusMisc: Failed to find %s field\n", fieldname);
        return (1);
    }

    // disable the interrupt
    valuename = "LW_PBUS_INTR_EN_0_PRI_FECSERR_DISABLED";
    if (!(value = field->FindValue(valuename)))
    {
        ErrPrintf("GC6PlusMisc: Failed to find %s value\n", valuename);
        return (1);
    }

    // no need to write register if interrupt is already disabled
    pri_fecserr = (reg_data & field->GetMask()) >> field->GetStartBit();
    if (pri_fecserr != value->GetValue())
    {
        InfoPrintf("GC6PlusMisc: Disabling PRI_FECSERR_0 interrupts\n");
        reg_data = (reg_data & ~field->GetMask()) |
                   (value->GetValue() << field->GetStartBit());
        lwgpu->RegWr32(reg_addr, reg_data);
        InfoPrintf("GC6PlusMisc: Writing %s = 0x%08x\n", reg->GetName(),
            reg_data);
    }
    else
    {
        InfoPrintf("GC6PlusMisc: PRI_FECSERR_0 interrupts are already "
            "disabled\n");
    }

    // the interrupt mask is at LW_PBUS_INTR_EN_1_PRI_FECSERR
    regname = "LW_PBUS_INTR_EN_1";
    if (!(reg = m_regMap->FindRegister(regname)))
    {
        ErrPrintf("GC6PlusMisc: Failed to find %s register\n", regname);
        return (1);
    }
    reg_addr = reg->GetAddress();
    reg_data = lwgpu->RegRd32(reg_addr);
    InfoPrintf("GC6PlusMisc: Reading %s = 0x%08x\n", reg->GetName(), reg_data);

    fieldname = "LW_PBUS_INTR_EN_1_PRI_FECSERR";
    if (!(field = reg->FindField(fieldname)))
    {
        ErrPrintf("GC6PlusMisc: Failed to find %s field\n", fieldname);
        return (1);
    }

    // disable the interrupt
    valuename = "LW_PBUS_INTR_EN_1_PRI_FECSERR_DISABLED";
    if (!(value = field->FindValue(valuename)))
    {
        ErrPrintf("GC6PlusMisc: Failed to find %s value\n", valuename);
        return (1);
    }

    // no need to write register if interrupt is already disabled
    pri_fecserr = (reg_data & field->GetMask()) >> field->GetStartBit();
    if (pri_fecserr != value->GetValue())
    {
        InfoPrintf("GC6PlusMisc: Disabling PRI_FECSERR_1 interrupts\n");
        reg_data = (reg_data & ~field->GetMask()) |
                   (value->GetValue() << field->GetStartBit());
        lwgpu->RegWr32(reg_addr, reg_data);
        InfoPrintf("GC6PlusMisc: Writing %s = 0x%08x\n", reg->GetName(),
            reg_data);
    }
    else
    {
        InfoPrintf("GC6PlusMisc: PRI_FECSERR_1 interrupts are already "
            "disabled\n");
    }

    return (0);
}

// This routine polls LW_PPRIV_MASTER_RING_COMMAND until it is done.
INT32
GC6PlusMisc::poll_for_ring_command_done(UINT32 max_attempts)
{
    unique_ptr<IRegisterClass> reg;
    unique_ptr<IRegisterField> field;
    unique_ptr<IRegisterValue> value;
    const char *regname, *fieldname, *valuename;
    UINT32 reg_addr, reg_data;
    UINT32 command;
    UINT32 polling_attempts;
    bool done;

    // The ring command is in LW_PPRIV_MASTER_RING_COMMAND_CMD field.
    regname = "LW_PPRIV_MASTER_RING_COMMAND";
    if (!(reg = m_regMap->FindRegister(regname)))
    {
        ErrPrintf("GC6PlusMisc: Failed to find %s register\n", regname);
        return (1);
    }
    reg_addr = reg->GetAddress();

    fieldname = "LW_PPRIV_MASTER_RING_COMMAND_CMD";
    if (!(field = reg->FindField(fieldname)))
    {
        ErrPrintf("GC6PlusMisc: Failed to find %s field\n", fieldname);
        return (1);
    }
    valuename = "LW_PPRIV_MASTER_RING_COMMAND_CMD_NO_CMD";
    if (!(value = field->FindValue(valuename)))
    {
        ErrPrintf("GC6PlusMisc: Failed to find %s value\n", valuename);
        return (1);
    }

    // We limit the number of polls so we do not hang the test.
    polling_attempts = 1;
    reg_data = lwgpu->RegRd32(reg_addr);
    command = (reg_data & field->GetMask()) >> field->GetStartBit();
    done = (command == value->GetValue());
    while (!done && (polling_attempts < max_attempts))
    {
        if ((polling_attempts % 10) == 1)
        {
            InfoPrintf("GC6PlusMisc: Polling RING_COMMAND done attempt #%d: "
                "%s = 0x%02x\n", polling_attempts, field->GetName(), command);
        }
        Platform::Delay(1);
        polling_attempts = polling_attempts + 1;
        reg_data = lwgpu->RegRd32(reg_addr);
        command = (reg_data & field->GetMask()) >> field->GetStartBit();
        done = (command == value->GetValue());
    }

    InfoPrintf("GC6PlusMisc: Polling RING_COMMAND done attempt #%d: "
        "%s = 0x%02x\n", polling_attempts, field->GetName(), command);
    if (!done)
    {
        ErrPrintf("GC6PlusMisc: Timeout waiting for %s to be 0x%02x after %d "
            "attempts\n", field->GetName(), value->GetValue(), max_attempts);
        return (1);
    }

    InfoPrintf("GC6PlusMisc: Done polling for RING_COMMAND to complete\n");
    return (0);
}


// This routine polls the specified register field through PRIV read.
// It can poll until the value is equal or !equal to the desired value.
INT32
GC6PlusMisc::poll_reg_field(const char *regname,
                            const char *fieldname,
                            UINT32 match_value,
                            bool equal,
                            UINT32 timeout)
{
    unique_ptr<IRegisterClass> reg;
    unique_ptr<IRegisterField> field;
    char tempname[127];
    UINT32 reg_addr, reg_data, act_value, polling_attempts;
    bool done;

    // get the register field to poll on
    if (!(reg = m_regMap->FindRegister(regname)))
    {
        ErrPrintf("GC6PlusMisc: Failed to find %s register\n", regname);
        return (1);
    }
    reg_addr = reg->GetAddress();
    strcpy(tempname, regname);
    strcat(tempname, "_");
    strcat(tempname, fieldname);
    if (!(field = reg->FindField(tempname)))
    {
        ErrPrintf("GC6PlusMisc: Failed to find %s field\n", tempname);
        return (1);
    }

    InfoPrintf("GC6PlusMisc: Polling %s field to be %0s to 0x%x\n",
        field->GetName(), equal ? "equal" : "not equal", match_value);

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
            InfoPrintf("GC6PlusMisc: Polling attempt #%d: %s = 0x%x\n",
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

    InfoPrintf("GC6PlusMisc: Polling done after attempt #%d: %s = 0x%x\n",
        polling_attempts, field->GetName(), act_value);
    if (!done)
    {
        InfoPrintf("GC6PlusMisc: Timeout polling %s field to be %0s to 0x%x "
            "after %d attempts\n", field->GetName(), equal ? "equal" :
            "not equal", match_value, polling_attempts);
        return (-1);
    }

    InfoPrintf("GC6PlusMisc: Done polling %s\n", field->GetName());
    return (0);
}


// This routine sets the priv security enable fuse.
INT32
GC6PlusMisc::set_priv_selwrity(bool enable)
{
    unique_ptr<IRegisterClass> reg;
    unique_ptr<IRegisterField> field;
    unique_ptr<IRegisterValue> value;
    const char *regname, *fieldname, *valuename;
    UINT32 reg_addr, reg_data;
    UINT32 opt_priv_sec_en;

    InfoPrintf("GC6PlusMisc: Configuring opt_priv_sec_en to %0s\n",
        enable ? "enable" : "disable");

    // The priv security enable field is in LW_FUSE_CTRL_OPT_PRIV_SEC_EN
    regname = "LW_FUSE_CTRL_OPT_PRIV_SEC_EN";
    if (!(reg = m_regMap->FindRegister(regname)))
    {
        ErrPrintf("GC6PlusMisc: Failed to find %s register\n", regname);
        return (1);
    }
    reg_addr = reg->GetAddress();
    reg_data = lwgpu->RegRd32(reg_addr);
    InfoPrintf("GC6PlusMisc: Reading %s = 0x%08x\n", reg->GetName(), reg_data);

    fieldname = "LW_FUSE_CTRL_OPT_PRIV_SEC_EN_DATA";
    if (!(field = reg->FindField(fieldname)))
    {
        ErrPrintf("GC6PlusMisc: Failed to find %s field\n", fieldname);
        return (1);
    }

    // set new value
    if (enable)
    {
        valuename = "LW_FUSE_CTRL_OPT_PRIV_SEC_EN_DATA_YES";
    }
    else
    {
        valuename = "LW_FUSE_CTRL_OPT_PRIV_SEC_EN_DATA_NO";
    }
    if (!(value = field->FindValue(valuename)))
    {
        ErrPrintf("GC6PlusMisc: Failed to find %s value\n", valuename);
        return (1);
    }

    // override priv security if it is not the desired value
    opt_priv_sec_en = (reg_data & field->GetMask()) >> field->GetStartBit();
    if (opt_priv_sec_en != value->GetValue())
    {
        reg_data = (reg_data & ~field->GetMask()) |
            (value->GetValue() << field->GetStartBit());
        lwgpu->RegWr32(reg_addr, reg_data);
        InfoPrintf("GC6PlusMisc: Writing %s = 0x%08x\n", reg->GetName(),
            reg_data);

        // There is some latency until the security fuse propagates.  We
        // need to flush the write before we begin any other access that
        // depends on the fuse setting.
        reg_data = lwgpu->RegRd32(reg_addr);
        InfoPrintf("GC6PlusMisc: Reading %s = 0x%08x\n", reg->GetName(),
            reg_data);
    }

    return (0);
}

// This routine gets the current priv ack mode.  This is used to determine
// whether writes are ack'ed immediately or upon completion.  This affects
// where priv violations are reported.
// Starting in GK20X, the LW_PMC_PRI_WR_ACK_CTRL have been deprecated.  SW
// still references the XVE control, but that control does nothing.  If these
// fields do not exist, then we have to hardcode them.
INT32
GC6PlusMisc::get_priv_ack_mode (bool *cpu_immediate_ack)
{
    unique_ptr<IRegisterClass> reg;
    unique_ptr<IRegisterField> field;
    unique_ptr<IRegisterValue> value;
    const char *regname, *fieldname, *valuename;
    UINT32 reg_addr, reg_data, wr_ack_ctrl;

    // The XVE configuration field may exist, even though it does nothing.
    // This was done to avoid codependencies with SW access to this field.
    // Check the PMU configuration first.  If it exists, we are pre-GK20X and
    // the PRI_WR_ACK_CTRL are fully supported.  If it does not exist, then
    // we use hardcoded defaults.

    // The configuration for PMU_BAR0 is found at
    // LW_PMC_PRI_WR_ACK_CTRL_PMU_EN field.
    regname = "LW_PMC_PRI_WR_ACK_CTRL_PMU";
    if (!(reg = m_regMap->FindRegister(regname)))
    {
        InfoPrintf("GC6PlusMisc: Failed to find %s register.  Defaulting "
            "to hardcoded values.\n", regname);

        // Post-GK20X, the CPU_BAR0 is expected to never wait for an ack.
        *cpu_immediate_ack = true;
        InfoPrintf("GC6PlusMisc: CPU_BAR0 writes generate ack immediately\n");
        return (0);
    }

    // The configuration for CPU_BAR0 is found at
    // LW_PMC_PRI_WR_ACK_CTRL_XVE_EN field.
    regname = "LW_PMC_PRI_WR_ACK_CTRL_XVE";
    if (!(reg = m_regMap->FindRegister(regname)))
    {
        ErrPrintf("GC6PlusMisc: Failed to find %s register\n", regname);
        return (1);
    }
    fieldname = "LW_PMC_PRI_WR_ACK_CTRL_XVE_EN";
    if (!(field = reg->FindField(fieldname)))
    {
        ErrPrintf("GC6PlusMisc: Failed to find %s field\n", fieldname);
        return (1);
    }
    reg_addr = reg->GetAddress();
    reg_data = lwgpu->RegRd32(reg_addr);
    InfoPrintf("GC6PlusMisc: Reading %s = 0x%08x\n", reg->GetName(), reg_data);

    valuename = "LW_PMC_PRI_WR_ACK_CTRL_XVE_EN_DISABLED";
    if (!(value = field->FindValue(valuename)))
    {
        ErrPrintf("GC6PlusMisc: Failed to find %s value\n", valuename);
        return (1);
    }

    // check current value against the immediate_ack value
    wr_ack_ctrl = (reg_data & field->GetMask()) >> field->GetStartBit();
    if (wr_ack_ctrl == value->GetValue())
    {
        *cpu_immediate_ack = true;
        InfoPrintf("GC6PlusMisc: CPU_BAR0 writes generate ack immediately\n");
    }
    else
    {
        *cpu_immediate_ack = false;
        InfoPrintf("GC6PlusMisc: CPU_BAR0 writes generate ack upon "
            "completion\n");
    }

    return (0);
}

INT32
GC6PlusMisc::set_sci_ptimer(UINT32 time_1, UINT32 time_0)
{
    unique_ptr<IRegisterClass> reg;
    unique_ptr<IRegisterField> field;
    unique_ptr<IRegisterValue> value;
    const char *regname, *fieldname, *valuename;
    UINT32 reg_addr, reg_data;
    UINT32 act_time_1, act_time_0;

    InfoPrintf("GC6PlusMisc: Configuring SCI PTIMER\n");

    // Configure SCI PTIMER to run on 27 MHz source clock
    regname = "LW_PGC6_SCI_PTIMER_CFG";
    if (!(reg = m_regMap->FindRegister(regname)))
    {
        ErrPrintf("GC6PlusMisc: Failed to find %s register\n", regname);
        return (1);
    }
    reg_addr = reg->GetAddress();
    // DENOMINATOR
    fieldname = "LW_PGC6_SCI_PTIMER_CFG_DENOMINATOR";
    if (!(field = reg->FindField(fieldname)))
    {
        ErrPrintf("GC6PlusMisc: Failed to find %s field\n", fieldname);
        return (1);
    }
    valuename = "LW_PGC6_SCI_PTIMER_CFG_DENOMINATOR_27MHZ_REF";
    if (!(value = field->FindValue(valuename)))
    {
        ErrPrintf("GC6PlusMisc: Failed to find %s value\n", valuename);
        return (1);
    }
    reg_data = (value->GetValue() << field->GetStartBit());
    // NUMERATOR
    fieldname = "LW_PGC6_SCI_PTIMER_CFG_NUMERATOR";
    if (!(field = reg->FindField(fieldname)))
    {
        ErrPrintf("GC6PlusMisc: Failed to find %s field\n", fieldname);
        return (1);
    }
    valuename = "LW_PGC6_SCI_PTIMER_CFG_NUMERATOR_27MHZ_REF";
    if (!(value = field->FindValue(valuename)))
    {
        ErrPrintf("GC6PlusMisc: Failed to find %s value\n", valuename);
        return (1);
    }
    reg_data |= (value->GetValue() << field->GetStartBit());
    // INTEGER
    fieldname = "LW_PGC6_SCI_PTIMER_CFG_INTEGER";
    if (!(field = reg->FindField(fieldname)))
    {
        ErrPrintf("GC6PlusMisc: Failed to find %s field\n", fieldname);
        return (1);
    }
    valuename = "LW_PGC6_SCI_PTIMER_CFG_INTEGER_27MHZ_REF";
    if (!(value = field->FindValue(valuename)))
    {
        ErrPrintf("GC6PlusMisc: Failed to find %s value\n", valuename);
        return (1);
    }
    reg_data |= (value->GetValue() << field->GetStartBit());
    lwgpu->RegWr32(reg_addr, reg_data);
    InfoPrintf("GC6PlusMisc: Writing %s = 0x%08x\n", reg->GetName(), reg_data);

    // Write specified time to PTIMER
    regname = "LW_PGC6_SCI_PTIMER_TIME_1";
    if (!(reg = m_regMap->FindRegister(regname)))
    {
        ErrPrintf("GC6PlusMisc: Failed to find %s register\n", regname);
        return (1);
    }
    reg_addr = reg->GetAddress();
    fieldname = "LW_PGC6_SCI_PTIMER_TIME_1_NSEC";
    if (!(field = reg->FindField(fieldname)))
    {
        ErrPrintf("GC6PlusMisc: Failed to find %s field\n", fieldname);
        return (1);
    }
    act_time_1 = (time_1 & field->GetMask());
    reg_data = act_time_1;
    lwgpu->RegWr32(reg_addr, reg_data);
    InfoPrintf("GC6PlusMisc: Writing %s = 0x%08x\n", reg->GetName(), reg_data);

    regname = "LW_PGC6_SCI_PTIMER_TIME_0";
    if (!(reg = m_regMap->FindRegister(regname)))
    {
        ErrPrintf("GC6PlusMisc: Failed to find %s register\n", regname);
        return (1);
    }
    reg_addr = reg->GetAddress();
    fieldname = "LW_PGC6_SCI_PTIMER_TIME_0_NSEC";
    if (!(field = reg->FindField(fieldname)))
    {
        ErrPrintf("GC6PlusMisc: Failed to find %s field\n", fieldname);
        return (1);
    }
    act_time_0 = (time_0 & field->GetMask());
    reg_data = act_time_0;
    fieldname = "LW_PGC6_SCI_PTIMER_TIME_0_UPDATE";
    if (!(field = reg->FindField(fieldname)))
    {
        ErrPrintf("GC6PlusMisc: Failed to find %s field\n", fieldname);
        return (1);
    }
    valuename = "LW_PGC6_SCI_PTIMER_TIME_0_UPDATE_TRIGGER";
    if (!(value = field->FindValue(valuename)))
    {
        ErrPrintf("GC6PlusMisc: Failed to find %s value\n", valuename);
        return (1);
    }
    reg_data |= (value->GetValue() << field->GetStartBit());
    lwgpu->RegWr32(reg_addr, reg_data);
    InfoPrintf("GC6PlusMisc: Writing %s = 0x%08x\n", reg->GetName(), reg_data);

    InfoPrintf("GC6PlusMisc: SCI PTIMER written with 0x%08x%08x\n",
        act_time_1, act_time_0);

    // Start PTIMER
    regname = "LW_PGC6_SCI_PTIMER_ENABLE";
    if (!(reg = m_regMap->FindRegister(regname)))
    {
        ErrPrintf("GC6PlusMisc: Failed to find %s register\n", regname);
        return (1);
    }
    reg_addr = reg->GetAddress();
    fieldname = "LW_PGC6_SCI_PTIMER_ENABLE_CTL";
    if (!(field = reg->FindField(fieldname)))
    {
        ErrPrintf("GC6PlusMisc: Failed to find %s field\n", fieldname);
        return (1);
    }
    valuename = "LW_PGC6_SCI_PTIMER_ENABLE_CTL_YES";
    if (!(value = field->FindValue(valuename)))
    {
        ErrPrintf("GC6PlusMisc: Failed to find %s value\n", valuename);
        return (1);
    }
    reg_data = (value->GetValue() << field->GetStartBit());
    lwgpu->RegWr32(reg_addr, reg_data);
    InfoPrintf("GC6PlusMisc: Writing %s = 0x%08x\n", reg->GetName(), reg_data);

    return (0);
}

INT32
GC6PlusMisc::set_selwre_timer(UINT32 time_1, UINT32 time_0)
{
    unique_ptr<IRegisterClass> reg;
    unique_ptr<IRegisterField> field;
    unique_ptr<IRegisterValue> value;
    const char *regname, *fieldname, *valuename;
    UINT32 reg_addr, reg_data;
    UINT32 act_time_1, act_time_0;

    InfoPrintf("GC6PlusMisc: Configuring SCI Secure Timer\n");

    // Configure Secure timer to run on 27 MHz source clock
    regname = "LW_PGC6_SCI_SEC_TIMER_CFG";
    if (!(reg = m_regMap->FindRegister(regname)))
    {
        ErrPrintf("GC6PlusMisc: Failed to find %s register\n", regname);
        return (1);
    }
    reg_addr = reg->GetAddress();
    // DENOMINATOR
    fieldname = "LW_PGC6_SCI_SEC_TIMER_CFG_DENOMINATOR";
    if (!(field = reg->FindField(fieldname)))
    {
        ErrPrintf("GC6PlusMisc: Failed to find %s field\n", fieldname);
        return (1);
    }
    valuename = "LW_PGC6_SCI_SEC_TIMER_CFG_DENOMINATOR_27MHZ_REF";
    if (!(value = field->FindValue(valuename)))
    {
        ErrPrintf("GC6PlusMisc: Failed to find %s value\n", valuename);
        return (1);
    }
    reg_data = (value->GetValue() << field->GetStartBit());
    // NUMERATOR
    fieldname = "LW_PGC6_SCI_SEC_TIMER_CFG_NUMERATOR";
    if (!(field = reg->FindField(fieldname)))
    {
        ErrPrintf("GC6PlusMisc: Failed to find %s field\n", fieldname);
        return (1);
    }
    valuename = "LW_PGC6_SCI_SEC_TIMER_CFG_NUMERATOR_27MHZ_REF";
    if (!(value = field->FindValue(valuename)))
    {
        ErrPrintf("GC6PlusMisc: Failed to find %s value\n", valuename);
        return (1);
    }
    reg_data |= (value->GetValue() << field->GetStartBit());
    // INTEGER
    fieldname = "LW_PGC6_SCI_SEC_TIMER_CFG_INTEGER";
    if (!(field = reg->FindField(fieldname)))
    {
        ErrPrintf("GC6PlusMisc: Failed to find %s field\n", fieldname);
        return (1);
    }
    valuename = "LW_PGC6_SCI_SEC_TIMER_CFG_INTEGER_27MHZ_REF";
    if (!(value = field->FindValue(valuename)))
    {
        ErrPrintf("GC6PlusMisc: Failed to find %s value\n", valuename);
        return (1);
    }
    reg_data |= (value->GetValue() << field->GetStartBit());
    lwgpu->RegWr32(reg_addr, reg_data);
    InfoPrintf("GC6PlusMisc: Writing %s = 0x%08x\n", reg->GetName(), reg_data);

    // Write specified time to TIMER
    regname = "LW_PGC6_SCI_SEC_TIMER_TIME_1";
    if (!(reg = m_regMap->FindRegister(regname)))
    {
        ErrPrintf("GC6PlusMisc: Failed to find %s register\n", regname);
        return (1);
    }
    reg_addr = reg->GetAddress();
    fieldname = "LW_PGC6_SCI_SEC_TIMER_TIME_1_NSEC";
    if (!(field = reg->FindField(fieldname)))
    {
        ErrPrintf("GC6PlusMisc: Failed to find %s field\n", fieldname);
        return (1);
    }
    act_time_1 = (time_1 & field->GetMask());
    reg_data = act_time_1;
    lwgpu->RegWr32(reg_addr, reg_data);
    InfoPrintf("GC6PlusMisc: Writing %s = 0x%08x\n", reg->GetName(), reg_data);

    regname = "LW_PGC6_SCI_SEC_TIMER_TIME_0";
    if (!(reg = m_regMap->FindRegister(regname)))
    {
        ErrPrintf("GC6PlusMisc: Failed to find %s register\n", regname);
        return (1);
    }
    reg_addr = reg->GetAddress();
    fieldname = "LW_PGC6_SCI_SEC_TIMER_TIME_0_NSEC";
    if (!(field = reg->FindField(fieldname)))
    {
        ErrPrintf("GC6PlusMisc: Failed to find %s field\n", fieldname);
        return (1);
    }
    act_time_0 = (time_0 & field->GetMask());
    reg_data = act_time_0;
    fieldname = "LW_PGC6_SCI_SEC_TIMER_TIME_0_UPDATE";
    if (!(field = reg->FindField(fieldname)))
    {
        ErrPrintf("GC6PlusMisc: Failed to find %s field\n", fieldname);
        return (1);
    }
    valuename = "LW_PGC6_SCI_SEC_TIMER_TIME_0_UPDATE_TRIGGER";
    if (!(value = field->FindValue(valuename)))
    {
        ErrPrintf("GC6PlusMisc: Failed to find %s value\n", valuename);
        return (1);
    }
    reg_data |= (value->GetValue() << field->GetStartBit());
    lwgpu->RegWr32(reg_addr, reg_data);
    InfoPrintf("GC6PlusMisc: Writing %s = 0x%08x\n", reg->GetName(), reg_data);

    InfoPrintf("GC6PlusMisc: SCI Secure Timer written with 0x%08x%08x\n",
        act_time_1, act_time_0);

    return (0);
}

INT32
GC6PlusMisc::get_sci_ptimer(UINT32 *time_1, UINT32 *time_0)
{
    unique_ptr<IRegisterClass> reg;
    unique_ptr<IRegisterField> field;
    unique_ptr<IRegisterValue> value;
    const char *regname, *fieldname, *valuename;
    UINT32 reg_addr, reg_data;

    InfoPrintf("GC6PlusMisc: Getting SCI PTIMER value\n");

    // Snapshot current SCI PTIMER
    regname = "LW_PGC6_SCI_PTIMER_READ";
    if (!(reg = m_regMap->FindRegister(regname)))
    {
        ErrPrintf("GC6PlusMisc: Failed to find %s register\n", regname);
        return (1);
    }
    reg_addr = reg->GetAddress();
    fieldname = "LW_PGC6_SCI_PTIMER_READ_SAMPLE";
    if (!(field = reg->FindField(fieldname)))
    {
        ErrPrintf("GC6PlusMisc: Failed to find %s field\n", fieldname);
        return (1);
    }
    valuename = "LW_PGC6_SCI_PTIMER_READ_SAMPLE_TRIGGER";
    if (!(value = field->FindValue(valuename)))
    {
        ErrPrintf("GC6PlusMisc: Failed to find %s value\n", valuename);
        return (1);
    }
    reg_data = (value->GetValue() << field->GetStartBit());
    lwgpu->RegWr32(reg_addr, reg_data);
    InfoPrintf("GC6PlusMisc: Writing %s = 0x%08x\n", reg->GetName(), reg_data);

    // Readback TIME_0 and TIME_1.  Apply field mask, but do not right
    // justify the fields.
    regname = "LW_PGC6_SCI_PTIMER_TIME_1";
    if (!(reg = m_regMap->FindRegister(regname)))
    {
        ErrPrintf("GC6PlusMisc: Failed to find %s register\n", regname);
        return (1);
    }
    reg_addr = reg->GetAddress();
    reg_data = lwgpu->RegRd32(reg_addr);
    InfoPrintf("GC6PlusMisc: Reading %s = 0x%08x\n", reg->GetName(), reg_data);

    fieldname = "LW_PGC6_SCI_PTIMER_TIME_1_NSEC";
    if (!(field = reg->FindField(fieldname)))
    {
        ErrPrintf("GC6PlusMisc: Failed to find %s field\n", fieldname);
        return (1);
    }
    *time_1 = (reg_data & field->GetMask());

    regname = "LW_PGC6_SCI_PTIMER_TIME_0";
    if (!(reg = m_regMap->FindRegister(regname)))
    {
        ErrPrintf("GC6PlusMisc: Failed to find %s register\n", regname);
        return (1);
    }
    reg_addr = reg->GetAddress();
    reg_data = lwgpu->RegRd32(reg_addr);
    InfoPrintf("GC6PlusMisc: Reading %s = 0x%08x\n", reg->GetName(), reg_data);

    fieldname = "LW_PGC6_SCI_PTIMER_TIME_0_NSEC";
    if (!(field = reg->FindField(fieldname)))
    {
        ErrPrintf("GC6PlusMisc: Failed to find %s field\n", fieldname);
        return (1);
    }
    *time_0 = (reg_data & field->GetMask());

    return (0);
}

INT32
GC6PlusMisc::get_selwre_timer(UINT32 *time_1, UINT32 *time_0)
{
    unique_ptr<IRegisterClass> reg0;
    unique_ptr<IRegisterClass> reg1;
    unique_ptr<IRegisterField> field0;
    unique_ptr<IRegisterField> field1;
    const char *regname, *fieldname;
    UINT32 reg_addr, reg_data;
    UINT32 old_time_1;

    InfoPrintf("GC6PlusMisc: Getting SCI Secure Timer value\n");

    // We need to read TIME_1, TIME_0, and TIME_1.  If TIME_1 is different
    // between the two reads, then we wrapped TIME_0 and cannot tell which
    // TIME_1 reading is correct.  We could probably take an educated guess,
    // but it is easier to return (-1) and let the calling program implement
    // a retry.

    // Read TIME_1
    regname = "LW_PGC6_SCI_SEC_TIMER_TIME_1";
    if (!(reg1 = m_regMap->FindRegister(regname)))
    {
        ErrPrintf("GC6PlusMisc: Failed to find %s register\n", regname);
        return (1);
    }
    reg_addr = reg1->GetAddress();
    reg_data = lwgpu->RegRd32(reg_addr);
    InfoPrintf("GC6PlusMisc: Reading %s = 0x%08x\n", reg1->GetName(), reg_data);

    fieldname = "LW_PGC6_SCI_SEC_TIMER_TIME_1_NSEC";
    if (!(field1 = reg1->FindField(fieldname)))
    {
        ErrPrintf("GC6PlusMisc: Failed to find %s field\n", fieldname);
        return (1);
    }
    old_time_1 = (reg_data & field1->GetMask());

    // Read TIME_0
    regname = "LW_PGC6_SCI_SEC_TIMER_TIME_0";
    if (!(reg0 = m_regMap->FindRegister(regname)))
    {
        ErrPrintf("GC6PlusMisc: Failed to find %s register\n", regname);
        return (1);
    }
    reg_addr = reg0->GetAddress();
    reg_data = lwgpu->RegRd32(reg_addr);
    InfoPrintf("GC6PlusMisc: Reading %s = 0x%08x\n", reg0->GetName(), reg_data);

    fieldname = "LW_PGC6_SCI_SEC_TIMER_TIME_0_NSEC";
    if (!(field0 = reg0->FindField(fieldname)))
    {
        ErrPrintf("GC6PlusMisc: Failed to find %s field\n", fieldname);
        return (1);
    }
    *time_0 = (reg_data & field0->GetMask());

    // Read TIME_1 again
    reg_addr = reg1->GetAddress();
    reg_data = lwgpu->RegRd32(reg_addr);
    InfoPrintf("GC6PlusMisc: Reading %s = 0x%08x\n", reg1->GetName(), reg_data);
    *time_1 = (reg_data & field1->GetMask());

    if (old_time_1 != (reg_data & field1->GetMask()))
    {
        // Wrap detected, we read back an invalid time
        InfoPrintf("GC6PlusMisc: Wrap detected in %s field, old = 0x%08x, new "
            "= 0x%08x\n", fieldname, old_time_1, reg_data & field1->GetMask());
        return (-1);
    }

    return (0);
}

INT32
GC6PlusMisc::set_host_ptimer(UINT32 time_1, UINT32 time_0)
{
    unique_ptr<IRegisterClass> reg;
    unique_ptr<IRegisterField> field;
    unique_ptr<IRegisterValue> value;
    const char *regname, *fieldname, *valuename;
    UINT32 reg_addr, reg_data;
    UINT32 act_time_1, act_time_0;

    InfoPrintf("GC6PlusMisc: Configuring HOST PTIMER\n");

    // Configure HOST PTIMER to run on 108 MHz source clock
    regname = "LW_PTIMER_TIMER_CFG0";
    if (!(reg = m_regMap->FindRegister(regname)))
    {
        ErrPrintf("GC6PlusMisc: Failed to find %s register\n", regname);
        return (1);
    }
    reg_addr = reg->GetAddress();
    // DENOMINATOR
    fieldname = "LW_PTIMER_TIMER_CFG0_DEN";
    if (!(field = reg->FindField(fieldname)))
    {
        ErrPrintf("GC6PlusMisc: Failed to find %s field\n", fieldname);
        return (1);
    }
    valuename = "LW_PTIMER_TIMER_CFG0_DEN_108MHZ_REF";
    if (!(value = field->FindValue(valuename)))
    {
        ErrPrintf("GC6PlusMisc: Failed to find %s value\n", valuename);
        return (1);
    }
    reg_data = (value->GetValue() << field->GetStartBit());
    // NUMERATOR
    fieldname = "LW_PTIMER_TIMER_CFG0_NUM";
    if (!(field = reg->FindField(fieldname)))
    {
        ErrPrintf("GC6PlusMisc: Failed to find %s field\n", fieldname);
        return (1);
    }
    valuename = "LW_PTIMER_TIMER_CFG0_NUM_108MHZ_REF";
    if (!(value = field->FindValue(valuename)))
    {
        ErrPrintf("GC6PlusMisc: Failed to find %s value\n", valuename);
        return (1);
    }
    reg_data |= (value->GetValue() << field->GetStartBit());
    lwgpu->RegWr32(reg_addr, reg_data);
    InfoPrintf("GC6PlusMisc: Writing %s = 0x%08x\n", reg->GetName(), reg_data);

    regname = "LW_PTIMER_TIMER_CFG1";
    if (!(reg = m_regMap->FindRegister(regname)))
    {
        ErrPrintf("GC6PlusMisc: Failed to find %s register\n", regname);
        return (1);
    }
    reg_addr = reg->GetAddress();
    // INTEGER
    fieldname = "LW_PTIMER_TIMER_CFG1_INTEGER";
    if (!(field = reg->FindField(fieldname)))
    {
        ErrPrintf("GC6PlusMisc: Failed to find %s field\n", fieldname);
        return (1);
    }
    valuename = "LW_PTIMER_TIMER_CFG1_INTEGER_108MHZ_REF";
    if (!(value = field->FindValue(valuename)))
    {
        ErrPrintf("GC6PlusMisc: Failed to find %s value\n", valuename);
        return (1);
    }
    reg_data = (value->GetValue() << field->GetStartBit());
    lwgpu->RegWr32(reg_addr, reg_data);
    InfoPrintf("GC6PlusMisc: Writing %s = 0x%08x\n", reg->GetName(), reg_data);

    // Write specified time to PTIMER
    regname = "LW_PTIMER_TIME_1";
    if (!(reg = m_regMap->FindRegister(regname)))
    {
        ErrPrintf("GC6PlusMisc: Failed to find %s register\n", regname);
        return (1);
    }
    reg_addr = reg->GetAddress();
    fieldname = "LW_PTIMER_TIME_1_NSEC";
    if (!(field = reg->FindField(fieldname)))
    {
        ErrPrintf("GC6PlusMisc: Failed to find %s field\n", fieldname);
        return (1);
    }
    act_time_1 = (time_1 & field->GetMask());
    reg_data = act_time_1;
    lwgpu->RegWr32(reg_addr, reg_data);
    InfoPrintf("GC6PlusMisc: Writing %s = 0x%08x\n", reg->GetName(), reg_data);

    regname = "LW_PTIMER_TIME_0";
    if (!(reg = m_regMap->FindRegister(regname)))
    {
        ErrPrintf("GC6PlusMisc: Failed to find %s register\n", regname);
        return (1);
    }
    reg_addr = reg->GetAddress();
    fieldname = "LW_PTIMER_TIME_0_NSEC";
    if (!(field = reg->FindField(fieldname)))
    {
        ErrPrintf("GC6PlusMisc: Failed to find %s field\n", fieldname);
        return (1);
    }
    act_time_0 = (time_0 & field->GetMask());
    reg_data = act_time_0;
    lwgpu->RegWr32(reg_addr, reg_data);
    InfoPrintf("GC6PlusMisc: Writing %s = 0x%08x\n", reg->GetName(), reg_data);

    InfoPrintf("GC6PlusMisc: HOST PTIMER written with 0x%08x%08x\n",
        act_time_1, act_time_0);

    // Perform a read to flush out the writes since the subsequent access may
    // be EscapeRead and we want to be sure the writes have completed before
    // the EscapeRead oclwrs.
    reg_data = lwgpu->RegRd32(reg_addr);
    InfoPrintf("GC6PlusMisc: Reading %s = 0x%08x\n", reg->GetName(), reg_data);

    return (0);
}

INT32
GC6PlusMisc::get_host_ptimer(UINT32 *time_1, UINT32 *time_0)
{
    unique_ptr<IRegisterClass> reg0;
    unique_ptr<IRegisterClass> reg1;
    unique_ptr<IRegisterField> field0;
    unique_ptr<IRegisterField> field1;
    const char *regname, *fieldname;
    UINT32 reg_addr, reg_data;
    UINT32 old_time_1;

    InfoPrintf("GC6PlusMisc: Getting HOST PTIMER value\n");

    // We need to read TIME_1, TIME_0, and TIME_1.  If TIME_1 is different
    // between the two reads, then we wrapped TIME_0 and cannot tell which
    // TIME_1 reading is correct.  We could probably take an educated guess,
    // but it is easier to return (-1) and let the calling program implement
    // a retry.

    // Read TIME_1
    regname = "LW_PTIMER_TIME_1";
    if (!(reg1 = m_regMap->FindRegister(regname)))
    {
        ErrPrintf("GC6PlusMisc: Failed to find %s register\n", regname);
        return (1);
    }
    reg_addr = reg1->GetAddress();
    reg_data = lwgpu->RegRd32(reg_addr);
    InfoPrintf("GC6PlusMisc: Reading %s = 0x%08x\n", reg1->GetName(), reg_data);

    fieldname = "LW_PTIMER_TIME_1_NSEC";
    if (!(field1 = reg1->FindField(fieldname)))
    {
        ErrPrintf("GC6PlusMisc: Failed to find %s field\n", fieldname);
        return (1);
    }
    old_time_1 = (reg_data & field1->GetMask());

    // Read TIME_0
    regname = "LW_PTIMER_TIME_0";
    if (!(reg0 = m_regMap->FindRegister(regname)))
    {
        ErrPrintf("GC6PlusMisc: Failed to find %s register\n", regname);
        return (1);
    }
    reg_addr = reg0->GetAddress();
    reg_data = lwgpu->RegRd32(reg_addr);
    InfoPrintf("GC6PlusMisc: Reading %s = 0x%08x\n", reg0->GetName(), reg_data);

    fieldname = "LW_PTIMER_TIME_0_NSEC";
    if (!(field0 = reg0->FindField(fieldname)))
    {
        ErrPrintf("GC6PlusMisc: Failed to find %s field\n", fieldname);
        return (1);
    }
    *time_0 = (reg_data & field0->GetMask());

    // Read TIME_1 again
    reg_addr = reg1->GetAddress();
    reg_data = lwgpu->RegRd32(reg_addr);
    InfoPrintf("GC6PlusMisc: Reading %s = 0x%08x\n", reg1->GetName(), reg_data);
    *time_1 = (reg_data & field1->GetMask());

    if (old_time_1 != (reg_data & field1->GetMask()))
    {
        // Wrap detected, we read back an invalid time
        InfoPrintf("GC6PlusMisc: Wrap detected in %s field, old = 0x%08x, new "
            "= 0x%08x\n", fieldname, old_time_1, reg_data & field1->GetMask());
        return (-1);
    }

    return (0);
}


INT32
GC6PlusMisc::get_ptimer_difference(INT64 *difference)
{
    unique_ptr<IRegisterClass> reg;
    unique_ptr<IRegisterField> field;
    const char *regname, *fieldname;
    UINT32 reg_addr, reg_data;
    UINT32 sci_time_1, sci_time_0, host_time_1, host_time_0;
    UINT32 loop_count, max_attempts;
    INT32 rc;
    UINT64 sci_time, host_time;
    bool done;

    InfoPrintf("GC6PlusMisc: Getting the difference between HOST and SCI "
        "PTIMER values\n");

    // Read the timers using EscapeRead or PRIV, depending on what the test
    // wants to gild.  Reading through PRIV is more realistic on silicon, but
    // PRIV latency differences will appear as drift.  Reading with EscapeReads
    // does not model real silicon behavior, but does eliminate PRIV latency
    // differences.
    if (m_read_time_using_escape_read)
    {
        // Read SCI PTIMER using EscapeRead
        // Justify timer to match 64-bit ptime register
        regname = "LW_PGC6_SCI_PTIMER_TIME_1";
        if (!(reg = m_regMap->FindRegister(regname)))
        {
            ErrPrintf("GC6PlusMisc: Failed to find %s register\n", regname);
            return (1);
        }
        fieldname = "LW_PGC6_SCI_PTIMER_TIME_1_NSEC";
        if (!(field = reg->FindField(fieldname)))
        {
            ErrPrintf("GC6PlusMisc: Failed to find %s field\n", fieldname);
            return (1);
        }
        Platform::EscapeRead("sci_ptimer_ptime1", 0, 4, &reg_data);
        InfoPrintf("GC6PlusMisc: EscapeRead sci_ptimer_ptime1 = 0x%08x\n",
            reg_data);
        sci_time_1 = (reg_data << field->GetStartBit());

        regname = "LW_PGC6_SCI_PTIMER_TIME_0";
        if (!(reg = m_regMap->FindRegister(regname)))
        {
            ErrPrintf("GC6PlusMisc: Failed to find %s register\n", regname);
            return (1);
        }
        fieldname = "LW_PGC6_SCI_PTIMER_TIME_0_NSEC";
        if (!(field = reg->FindField(fieldname)))
        {
            ErrPrintf("GC6PlusMisc: Failed to find %s field\n", fieldname);
            return (1);
        }
        Platform::EscapeRead("sci_ptimer_ptime0", 0, 8, &reg_data);
        InfoPrintf("GC6PlusMisc: EscapeRead sci_ptimer_ptime0 = 0x%08x\n",
            reg_data);
        sci_time_0 = (reg_data << field->GetStartBit());

        // Read HOST PTIMER using EscapeRead
        // Justify timer to match 64-bit ptime register
        regname = "LW_PTIMER_TIME_1";
        if (!(reg = m_regMap->FindRegister(regname)))
        {
            ErrPrintf("GC6PlusMisc: Failed to find %s register\n", regname);
            return (1);
        }
        reg_addr = reg->GetAddress();
        fieldname = "LW_PTIMER_TIME_1_NSEC";
        if (!(field = reg->FindField(fieldname)))
        {
            ErrPrintf("GC6PlusMisc: Failed to find %s field\n", fieldname);
            return (1);
        }
        Platform::EscapeRead("host_ptimer_ptime1", 0, 4, &reg_data);
        InfoPrintf("GC6PlusMisc: EscapeRead host_ptimer_ptime1 = 0x%08x\n",
            reg_data);
        host_time_1 = (reg_data << field->GetStartBit());

        regname = "LW_PTIMER_TIME_0";
        if (!(reg = m_regMap->FindRegister(regname)))
        {
            ErrPrintf("GC6PlusMisc: Failed to find %s register\n", regname);
            return (1);
        }
        reg_addr = reg->GetAddress();
        fieldname = "LW_PTIMER_TIME_0_NSEC";
        if (!(field = reg->FindField(fieldname)))
        {
            ErrPrintf("GC6PlusMisc: Failed to find %s field\n", fieldname);
            return (1);
        }
        Platform::EscapeRead("host_ptimer_ptime0", 0, 4, &reg_data);
        InfoPrintf("GC6PlusMisc: EscapeRead host_ptimer_ptime0 = 0x%08x\n",
            reg_data);
        host_time_0 = (reg_data << field->GetStartBit());
    }
    else
    {
        // Measure the difference/drift between SCI and HOST ptimer.
        // PTIMER could wrap to TIME_1, but it should be impossible to detect
        // it twice in a row.  We should only need 2 attempts to get a valid
        // HOST PTIMER read.
        done = false;
        loop_count = 0;
        max_attempts = 2;
        while (!done && (loop_count < max_attempts))
        {
            // Perform a dummy read to flush out any writes that may affect the
            // latency of SCI PRIV reads.
            regname = "LW_PGC6_SCI_DEBUG_STATUS";
            if (!(reg = m_regMap->FindRegister(regname)))
            {
                ErrPrintf("GC6PlusMisc: Failed to find %s register\n", regname);
                return (1);
            }
            reg_addr = reg->GetAddress();
            reg_data = lwgpu->RegRd32(reg_addr);
            InfoPrintf("GC6PlusMisc: Reading %s = 0x%08x\n", reg->GetName(),
                reg_data);

            // get SCI and HOST PTIMER
            if (get_sci_ptimer(&sci_time_1, &sci_time_0))
            {
                ErrPrintf("GC6PlusMisc: Failed to get SCI PTIMER\n");
                return (1);
            }
            rc = get_host_ptimer(&host_time_1, &host_time_0);
            if (rc > 0)
            {
                ErrPrintf("GC6PlusMisc: Failed to get HOST PTIMER\n");
                return (1);
            }

            // We got a good HOST PTIMER read if rc == 0
            // We got a bad HOST PTIMER read if rc == -1
            done = (rc == 0);
            loop_count = loop_count + 1;
        }

        if (!done)
        {
            ErrPrintf("GC6PlusMisc: Could not get stable HOST PTIMER after %d "
                "attempts\n", max_attempts);
            m_ErrCount++;
        }
    }

    // callwlate difference = SCI_TIMER - HOST_PTIMER
    sci_time = (UINT64(sci_time_1) << 32) | sci_time_0;
    host_time = (UINT64(host_time_1) << 32) | host_time_0;
    *difference = sci_time - host_time;
    InfoPrintf("GC6PlusMisc: Difference between SCI and HOST PTIMER = "
        "%d ns\n", *difference);

    return (0);
}

INT32
GC6PlusMisc::get_selwre_timer_difference(INT64 *difference)
{
    unique_ptr<IRegisterClass> reg;
    unique_ptr<IRegisterField> field;
    const char *regname, *fieldname;
    UINT32 reg_addr, reg_data;
    UINT32 sci_time_1, sci_time_0, host_time_1, host_time_0;
    UINT32 loop_count, max_attempts;
    INT32 rc_sci, rc_host;
    UINT64 sci_time, host_time;
    bool done;

    InfoPrintf("GC6PlusMisc: Getting the difference between SCI Secure Timer "
        "and HOST PTIMER values\n");

    // Read the timers using EscapeRead or PRIV, depending on what the test
    // wants to gild.  Reading through PRIV is more realistic on silicon, but
    // PRIV latency differences will appear as drift.  Reading with EscapeReads
    // does not model real silicon behavior, but does eliminate PRIV latency
    // differences.
    if (m_read_time_using_escape_read)
    {
        // Read SCI Secure Timer using EscapeRead
        // Justify timer to match 64-bit ptime register
        regname = "LW_PGC6_SCI_SEC_TIMER_TIME_1";
        if (!(reg = m_regMap->FindRegister(regname)))
        {
            ErrPrintf("GC6PlusMisc: Failed to find %s register\n", regname);
            return (1);
        }
        fieldname = "LW_PGC6_SCI_SEC_TIMER_TIME_1_NSEC";
        if (!(field = reg->FindField(fieldname)))
        {
            ErrPrintf("GC6PlusMisc: Failed to find %s field\n", fieldname);
            return (1);
        }
        Platform::EscapeRead("selwre_timer_ptime1", 0, 4, &reg_data);
        InfoPrintf("GC6PlusMisc: EscapeRead selwre_timer_ptime1 = 0x%08x\n",
            reg_data);
        sci_time_1 = (reg_data << field->GetStartBit());

        regname = "LW_PGC6_SCI_SEC_TIMER_TIME_0";
        if (!(reg = m_regMap->FindRegister(regname)))
        {
            ErrPrintf("GC6PlusMisc: Failed to find %s register\n", regname);
            return (1);
        }
        fieldname = "LW_PGC6_SCI_SEC_TIMER_TIME_0_NSEC";
        if (!(field = reg->FindField(fieldname)))
        {
            ErrPrintf("GC6PlusMisc: Failed to find %s field\n", fieldname);
            return (1);
        }
        Platform::EscapeRead("selwre_timer_ptime0", 0, 8, &reg_data);
        InfoPrintf("GC6PlusMisc: EscapeRead selwre_timer_ptime0 = 0x%08x\n",
            reg_data);
        sci_time_0 = (reg_data << field->GetStartBit());

        // Read HOST PTIMER using EscapeRead
        // Justify timer to match 64-bit ptime register
        regname = "LW_PTIMER_TIME_1";
        if (!(reg = m_regMap->FindRegister(regname)))
        {
            ErrPrintf("GC6PlusMisc: Failed to find %s register\n", regname);
            return (1);
        }
        reg_addr = reg->GetAddress();
        fieldname = "LW_PTIMER_TIME_1_NSEC";
        if (!(field = reg->FindField(fieldname)))
        {
            ErrPrintf("GC6PlusMisc: Failed to find %s field\n", fieldname);
            return (1);
        }
        Platform::EscapeRead("host_ptimer_ptime1", 0, 4, &reg_data);
        InfoPrintf("GC6PlusMisc: EscapeRead host_ptimer_ptime1 = 0x%08x\n",
            reg_data);
        host_time_1 = (reg_data << field->GetStartBit());

        regname = "LW_PTIMER_TIME_0";
        if (!(reg = m_regMap->FindRegister(regname)))
        {
            ErrPrintf("GC6PlusMisc: Failed to find %s register\n", regname);
            return (1);
        }
        reg_addr = reg->GetAddress();
        fieldname = "LW_PTIMER_TIME_0_NSEC";
        if (!(field = reg->FindField(fieldname)))
        {
            ErrPrintf("GC6PlusMisc: Failed to find %s field\n", fieldname);
            return (1);
        }
        Platform::EscapeRead("host_ptimer_ptime0", 0, 4, &reg_data);
        InfoPrintf("GC6PlusMisc: EscapeRead host_ptimer_ptime0 = 0x%08x\n",
            reg_data);
        host_time_0 = (reg_data << field->GetStartBit());
    }
    else
    {
        // Measure the difference/drift between SCI Secure Timer and HOST
        // ptimer.  Both TIMERs could wrap to TIME_1, but it should be
        // impossible to detect it twice in a row.  We should only need 3
        // attempts to get valid TIMER reads.
        done = false;
        loop_count = 0;
        max_attempts = 3;
        while (!done && (loop_count < max_attempts))
        {
            // Perform a dummy read to flush out any writes that may affect the
            // latency of SCI PRIV reads.
            regname = "LW_PGC6_SCI_DEBUG_STATUS";
            if (!(reg = m_regMap->FindRegister(regname)))
            {
                ErrPrintf("GC6PlusMisc: Failed to find %s register\n", regname);
                return (1);
            }
            reg_addr = reg->GetAddress();
            reg_data = lwgpu->RegRd32(reg_addr);
            InfoPrintf("GC6PlusMisc: Reading %s = 0x%08x\n", reg->GetName(),
                reg_data);

            // get SCI and HOST PTIMER
            rc_sci = get_selwre_timer(&sci_time_1, &sci_time_0);
            if (rc_sci > 0)
            {
                ErrPrintf("GC6PlusMisc: Failed to get SCI Secure Timer\n");
                return (1);
            }
            rc_host = get_host_ptimer(&host_time_1, &host_time_0);
            if (rc_host > 0)
            {
                ErrPrintf("GC6PlusMisc: Failed to get HOST PTIMER\n");
                return (1);
            }

            // We got a good TIMER read if rc == 0
            // We got a bad TIMER read if rc == -1
            done = (rc_sci == 0) && (rc_host == 0);
            loop_count = loop_count + 1;
        }

        if (!done && (rc_sci != 0))
        {
            ErrPrintf("GC6PlusMisc: Could not get stable SCI Secure Timer "
                "after %d attempts\n", max_attempts);
            m_ErrCount++;
        }
        if (!done && (rc_host != 0))
        {
            ErrPrintf("GC6PlusMisc: Could not get stable HOST PTIMER "
                "after %d attempts\n", max_attempts);
            m_ErrCount++;
        }
    }

    // callwlate difference = Selwre_Timer - HOST_PTIMER
    sci_time = (UINT64(sci_time_1) << 32) | sci_time_0;
    host_time = (UINT64(host_time_1) << 32) | host_time_0;
    *difference = sci_time - host_time;
    InfoPrintf("GC6PlusMisc: Difference between SCI Secure Timer and HOST "
        "PTIMER = %d ns\n", *difference);

    return (0);
}

UINT32
GC6PlusMisc::SciIndvAltCtlTest()
{
    InfoPrintf("GC6PlusMisc: Running sci_individual_alt_ctl test\n");

    unique_ptr<IRegisterClass> reg;
    unique_ptr<IRegisterField> field;
    unique_ptr<IRegisterValue> value;
    const char *regname, *fieldname, *valuename;
    UINT32 reg_addr, reg_data, array_size, array_stride;
    INT32 gpio_index;
    UINT32 tb_gpio_drive, tb_gpio_pull;
    UINT32 exp_data;
    UINT32 sci_gpio_output_cntl, pmgr_gpio_output_cntl;
    UINT32 norm_out_data_deassert, norm_out_data_assert;
    UINT32 sci_drive_gcx, sci_drive_always;
    UINT32 norm_in_data_low, norm_in_data_high;
    UINT32 pmgr_out_enab_tristate;

    // Get the dimensions for the GPIO Output Control register
    regname = "LW_PGC6_SCI_GPIO_OUTPUT_CNTL";
    reg = m_regMap->FindRegister(regname);
    if (!reg)
    {
        ErrPrintf("GC6PlusMisc: Failed to find %s register\n", regname);
        return (1);
    }
    reg->GetFormula1(array_size, array_stride);
    InfoPrintf("GC6PlusMisc: Register %s(i) has size = %d, stride = %d\n",
        regname, array_size, array_stride);

    // Parse the fields in the register and extract field values for later use
    // SEL = NORMAL
    fieldname = "LW_PGC6_SCI_GPIO_OUTPUT_CNTL_SEL";
    field = reg->FindField(fieldname);
    if (!field)
    {
        ErrPrintf("GC6PlusMisc: Failed to find %s field\n", fieldname);
        return (1);
    }
    valuename = "LW_PGC6_SCI_GPIO_OUTPUT_CNTL_SEL_NORMAL";
    value = field->FindValue(valuename);
    if (!value)
    {
        ErrPrintf("GC6PlusMisc: Failed to find %s value\n", valuename);
        return (1);
    }
    sci_gpio_output_cntl = (value->GetValue() << field->GetStartBit());
    // DRIVE_MODE = FULL_DRIVE
    fieldname = "LW_PGC6_SCI_GPIO_OUTPUT_CNTL_DRIVE_MODE";
    field = reg->FindField(fieldname);
    if (!field)
    {
        ErrPrintf("GC6PlusMisc: Failed to find %s field\n", fieldname);
        return (1);
    }
    valuename = "LW_PGC6_SCI_GPIO_OUTPUT_CNTL_DRIVE_MODE_FULL_DRIVE";
    value = field->FindValue(valuename);
    if (!value)
    {
        ErrPrintf("GC6PlusMisc: Failed to find %s value\n", valuename);
        return (1);
    }
    sci_gpio_output_cntl |= (value->GetValue() << field->GetStartBit());
    // POLARITY = ACTIVE_HIGH
    fieldname = "LW_PGC6_SCI_GPIO_OUTPUT_CNTL_POLARITY";
    field = reg->FindField(fieldname);
    if (!field)
    {
        ErrPrintf("GC6PlusMisc: Failed to find %s field\n", fieldname);
        return (1);
    }
    valuename = "LW_PGC6_SCI_GPIO_OUTPUT_CNTL_POLARITY_ACTIVE_HIGH";
    value = field->FindValue(valuename);
    if (!value)
    {
        ErrPrintf("GC6PlusMisc: Failed to find %s value\n", valuename);
        return (1);
    }
    sci_gpio_output_cntl |= (value->GetValue() << field->GetStartBit());

    // NORM_OUT_DATA = DEASSERT, ASSERT
    fieldname = "LW_PGC6_SCI_GPIO_OUTPUT_CNTL_NORM_OUT_DATA";
    field = reg->FindField(fieldname);
    if (!field)
    {
        ErrPrintf("GC6PlusMisc: Failed to find %s field\n", fieldname);
        return (1);
    }
    valuename = "LW_PGC6_SCI_GPIO_OUTPUT_CNTL_NORM_OUT_DATA_DEASSERT";
    value = field->FindValue(valuename);
    if (!value)
    {
        ErrPrintf("GC6PlusMisc: Failed to find %s value\n", valuename);
        return (1);
    }
    norm_out_data_deassert = (value->GetValue() << field->GetStartBit());
    valuename = "LW_PGC6_SCI_GPIO_OUTPUT_CNTL_NORM_OUT_DATA_ASSERT";
    value = field->FindValue(valuename);
    if (!value)
    {
        ErrPrintf("GC6PlusMisc: Failed to find %s value\n", valuename);
        return (1);
    }
    norm_out_data_assert = (value->GetValue() << field->GetStartBit());

    // SCI_DRIVE = GCX, ALWAYS
    fieldname = "LW_PGC6_SCI_GPIO_OUTPUT_CNTL_SCI_DRIVE";
    field = reg->FindField(fieldname);
    if (!field)
    {
        ErrPrintf("GC6PlusMisc: Failed to find %s field\n", fieldname);
        return (1);
    }
    valuename = "LW_PGC6_SCI_GPIO_OUTPUT_CNTL_SCI_DRIVE_GCX";
    value = field->FindValue(valuename);
    if (!value)
    {
        ErrPrintf("GC6PlusMisc: Failed to find %s value\n", valuename);
        return (1);
    }
    sci_drive_gcx = (value->GetValue() << field->GetStartBit());
    valuename = "LW_PGC6_SCI_GPIO_OUTPUT_CNTL_SCI_DRIVE_ALWAYS";
    value = field->FindValue(valuename);
    if (!value)
    {
        ErrPrintf("GC6PlusMisc: Failed to find %s value\n", valuename);
        return (1);
    }
    sci_drive_always = (value->GetValue() << field->GetStartBit());

    // NORM_IN_DATA = LOW, HIGH
    fieldname = "LW_PGC6_SCI_GPIO_OUTPUT_CNTL_NORM_IN_DATA";
    field = reg->FindField(fieldname);
    if (!field)
    {
        ErrPrintf("GC6PlusMisc: Failed to find %s field\n", fieldname);
        return (1);
    }
    valuename = "LW_PGC6_SCI_GPIO_OUTPUT_CNTL_NORM_IN_DATA_LOW";
    value = field->FindValue(valuename);
    if (!value)
    {
        ErrPrintf("GC6PlusMisc: Failed to find %s value\n", valuename);
        return (1);
    }
    norm_in_data_low = (value->GetValue() << field->GetStartBit());
    valuename = "LW_PGC6_SCI_GPIO_OUTPUT_CNTL_NORM_IN_DATA_HIGH";
    value = field->FindValue(valuename);
    if (!value)
    {
        ErrPrintf("GC6PlusMisc: Failed to find %s value\n", valuename);
        return (1);
    }
    norm_in_data_high = (value->GetValue() << field->GetStartBit());
        
    fieldname = "LW_PGC6_SCI_GPIO_OUTPUT_CNTL_PMGR_OUT_ENAB";
    field = reg->FindField(fieldname);
    if (!field)
    {
        ErrPrintf("GC6PlusMisc: Failed to find %s field\n", fieldname);
        return (1);
    }
    valuename = "LW_PGC6_SCI_GPIO_OUTPUT_CNTL_PMGR_OUT_ENAB_TRISTATE";
    value = field->FindValue(valuename);
    if (!value)
    {
        ErrPrintf("GC6PlusMisc: Failed to find %s value\n", valuename);
        return (1);
    }
    pmgr_out_enab_tristate = (value->GetValue() << field->GetStartBit());

    // update and error check which GPIOs to test
    if (check_gpio_min_max(array_size))
    {
        ErrPrintf("GC6PlusMisc: Error in determining which GPIOs to test\n");
        return (0);
    }

    // We dont want PMGR to report any interrupts to RM since RM is not
    // set up to handle GPIO interrupts.
    if (disable_pmgr_intr())
    {
        ErrPrintf("GC6PlusMisc: Failed to disable PMGR interrupts\n");
        return (1);
    }

    // loop through the specified GPIOs
    InfoPrintf("GC6PlusMisc: Testing Gpio %d to %d\n", m_gpio_min, m_gpio_max);
    for (gpio_index = m_gpio_min; gpio_index <= m_gpio_max; gpio_index++)
    {
        // save original tb_gpio configuration for restoring later
        if (get_tb_gpio_cfg(gpio_index, &tb_gpio_drive, &tb_gpio_pull))
        {
            ErrPrintf("GC6PlusMisc: Failed to get Testbench GPIO "
                "configuration\n");
            return (1);
        }

        // reconfigure testbench to apply weak pulldown on GPIO
        if (cfg_tb_gpio(gpio_index, GPIO_DRV_NONE, GPIO_PULL_DOWN))
        {
            ErrPrintf("GC6PlusMisc: Failed to configure Testbench GPIO\n");
            return (1);
        }

        // disable PMGR from driving GPIO
        if (disable_pmgr_gpio(gpio_index, &pmgr_gpio_output_cntl))
        {
            ErrPrintf("GC6PlusMisc: Failed to disable PMGR GPIO\n");
            return (1);
        }

        reg_addr = reg->GetAddress(gpio_index);
        // SCI GPIO: deassert in GCX
        reg_data = sci_gpio_output_cntl | norm_out_data_deassert |
            sci_drive_gcx;
        exp_data = reg_data | norm_in_data_low | pmgr_out_enab_tristate;
        lwgpu->RegWr32(reg_addr, reg_data);
        InfoPrintf("GC6PlusMisc: Writing %s(%d) = 0x%08x\n", reg->GetName(),
            gpio_index, reg_data);
        reg_data = lwgpu->RegRd32(reg_addr);
        InfoPrintf("GC6PlusMisc: Reading %s(%d) = 0x%08x\n", reg->GetName(),
            gpio_index, reg_data);
        if (reg_data != exp_data)
        {
            ErrPrintf("GC6PlusMisc: Mismatch in %s(%d): expected = 0x%08x, "
                "actual = 0x%08x\n", reg->GetName(), gpio_index, exp_data,
                reg_data);
            m_ErrCount++;
        }

        // SCI GPIO: assert in GCX
        reg_data = sci_gpio_output_cntl | norm_out_data_assert |
            sci_drive_gcx;
        exp_data = reg_data | norm_in_data_low | pmgr_out_enab_tristate;
        lwgpu->RegWr32(reg_addr, reg_data);
        InfoPrintf("GC6PlusMisc: Writing %s(%d) = 0x%08x\n", reg->GetName(),
            gpio_index, reg_data);
        reg_data = lwgpu->RegRd32(reg_addr);
        InfoPrintf("GC6PlusMisc: Reading %s(%d) = 0x%08x\n", reg->GetName(),
            gpio_index, reg_data);
        if (reg_data != exp_data)
        {
            ErrPrintf("GC6PlusMisc: Mismatch in %s(%d): expected = 0x%08x, "
                "actual = 0x%08x\n", reg->GetName(), gpio_index, exp_data,
                reg_data);
            m_ErrCount++;
        }

        // SCI GPIO: deassert always
        reg_data = sci_gpio_output_cntl | norm_out_data_deassert |
            sci_drive_always;
        exp_data = reg_data | norm_in_data_low | pmgr_out_enab_tristate;
        lwgpu->RegWr32(reg_addr, reg_data);
        InfoPrintf("GC6PlusMisc: Writing %s(%d) = 0x%08x\n", reg->GetName(),
            gpio_index, reg_data);
        reg_data = lwgpu->RegRd32(reg_addr);
        InfoPrintf("GC6PlusMisc: Reading %s(%d) = 0x%08x\n", reg->GetName(),
            gpio_index, reg_data);
        if (reg_data != exp_data)
        {
            ErrPrintf("GC6PlusMisc: Mismatch in %s(%d): expected = 0x%08x, "
                "actual = 0x%08x\n", reg->GetName(), gpio_index, exp_data,
                reg_data);
            m_ErrCount++;
        }

        // SCI GPIO: assert always
        reg_data = sci_gpio_output_cntl | norm_out_data_assert |
            sci_drive_always;
        exp_data = reg_data | norm_in_data_high | pmgr_out_enab_tristate;
        lwgpu->RegWr32(reg_addr, reg_data);
        InfoPrintf("GC6PlusMisc: Writing %s(%d) = 0x%08x\n", reg->GetName(),
            gpio_index, reg_data);
        reg_data = lwgpu->RegRd32(reg_addr);
        InfoPrintf("GC6PlusMisc: Reading %s(%d) = 0x%08x\n", reg->GetName(),
            gpio_index, reg_data);
        if (reg_data != exp_data)
        {
            ErrPrintf("GC6PlusMisc: Mismatch in %s(%d): expected = 0x%08x, "
                "actual = 0x%08x\n", reg->GetName(), gpio_index, exp_data,
                reg_data);
            m_ErrCount++;
        }

        // restore original PMGR configuration 
        if (restore_pmgr_gpio(gpio_index, pmgr_gpio_output_cntl))
        {
            ErrPrintf("GC6PlusMisc: Failed to disable PMGR GPIO\n");
            return (1);
        }

        // restore original testbench configuration
        if (cfg_tb_gpio(gpio_index, tb_gpio_drive, tb_gpio_pull))
        {
            ErrPrintf("GC6PlusMisc: Failed to configure Testbench GPIO\n");
            return (1);
        }
    }

    return (0);
}

UINT32
GC6PlusMisc::SciHwGpioCopyTest()
{
    InfoPrintf("GC6PlusMisc: Running sci_hw_gpio_copy test\n");

    unique_ptr<IRegisterClass> reg;
    unique_ptr<IRegisterField> field;
    unique_ptr<IRegisterValue> value;
    const char *regname, *fieldname, *valuename;
    UINT32 reg_addr, reg_data, array_size, array_stride;
    INT32 gpio_index;
    UINT32 exp_data, act_data;
    UINT32 pmgr_gpio_output_cntl;

    // Get the dimensions for the GPIO Output Control register
    regname = "LW_PGC6_SCI_GPIO_OUTPUT_CNTL";
    reg = m_regMap->FindRegister(regname);
    if (!reg)
    {
        ErrPrintf("GC6PlusMisc: Failed to find %s register\n", regname);
        return (1);
    }
    reg->GetFormula1(array_size, array_stride);
    InfoPrintf("GC6PlusMisc: Register %s(i) has size = %d, stride = %d\n",
        regname, array_size, array_stride);


    // update and error check which GPIOs to test
    if (check_gpio_min_max(array_size))
    {
        ErrPrintf("GC6PlusMisc: Error in determining which GPIOs to test\n");
        return (0);
    }


    // We dont want PMGR to report any interrupts to RM since RM is not
    // set up to handle GPIO interrupts.
    if (disable_pmgr_intr())
    {
        ErrPrintf("GC6PlusMisc: Failed to disable PMGR interrupts\n");
        return (1);
    }

    // loop through the specified GPIOs
    InfoPrintf("GC6PlusMisc: Testing Gpio %d to %d\n", m_gpio_min, m_gpio_max);
    for (gpio_index = m_gpio_min; gpio_index <= m_gpio_max; gpio_index++)
    {
        // disable PMGR from driving GPIO
        if (disable_pmgr_gpio(gpio_index, &pmgr_gpio_output_cntl))
        {
            ErrPrintf("GC6PlusMisc: Failed to disable PMGR GPIO\n");
            return (1);
        }

        // read and check SCI GPIO output cntl register
        reg_addr = reg->GetAddress(gpio_index);
        reg_data = lwgpu->RegRd32(reg_addr);
        InfoPrintf("GC6PlusMisc: Reading %s(%d) = 0x%08x\n", reg->GetName(),
            gpio_index, reg_data);
        // check PMGR_OUT_DATA and PMGR_OUT_ENAB (should be tristate)
        fieldname = "LW_PGC6_SCI_GPIO_OUTPUT_CNTL_PMGR_OUT_DATA";
        field = reg->FindField(fieldname);
        if (!field)
        {
            ErrPrintf("GC6PlusMisc: Failed to find %s field\n", fieldname);
            return (1);
        }
        act_data = (reg_data & field->GetMask()) >> field->GetStartBit();
        valuename = "LW_PGC6_SCI_GPIO_OUTPUT_CNTL_PMGR_OUT_DATA_LOW";
        value = field->FindValue(valuename);
        if (!value)
        {
            ErrPrintf("GC6PlusMisc: Failed to find %s value\n", valuename);
            return (1);
        }
        exp_data = value->GetValue();
        if (exp_data != act_data)
        {
            ErrPrintf("GC6PlusMisc: Mismatch in field %s: expected = %d, "
                "actual = %d\n", field->GetName(), exp_data, act_data);
            m_ErrCount++;
        }
        fieldname = "LW_PGC6_SCI_GPIO_OUTPUT_CNTL_PMGR_OUT_ENAB";
        field = reg->FindField(fieldname);
        if (!field)
        {
            ErrPrintf("GC6PlusMisc: Failed to find %s field\n", fieldname);
            return (1);
        }
        act_data = (reg_data & field->GetMask()) >> field->GetStartBit();
        valuename = "LW_PGC6_SCI_GPIO_OUTPUT_CNTL_PMGR_OUT_ENAB_TRISTATE";
        value = field->FindValue(valuename);
        if (!value)
        {
            ErrPrintf("GC6PlusMisc: Failed to find %s value\n", valuename);
            return (1);
        }
        exp_data = value->GetValue();
        if (exp_data != act_data)
        {
            ErrPrintf("GC6PlusMisc: Mismatch in field %s: expected = %d, "
                "actual = %d\n", field->GetName(), exp_data, act_data);
            m_ErrCount++;
        }

        // configure PMGR GPIO output cntl register to drive to 1
        if (configure_pmgr_gpio(gpio_index))
        {
            ErrPrintf("GC6PlusMisc: Failed to configure PMGR GPIO\n");
            return (1);
        }

        // read and check SCI GPIO output cntl register
        reg_addr = reg->GetAddress(gpio_index);
        reg_data = lwgpu->RegRd32(reg_addr);
        InfoPrintf("GC6PlusMisc: Reading %s(%d) = 0x%08x\n", reg->GetName(),
            gpio_index, reg_data);
        // check PMGR_OUT_DATA and PMGR_OUT_ENAB (should be drive to 1)
        fieldname = "LW_PGC6_SCI_GPIO_OUTPUT_CNTL_PMGR_OUT_DATA";
        field = reg->FindField(fieldname);
        if (!field)
        {
            ErrPrintf("GC6PlusMisc: Failed to find %s field\n", fieldname);
            return (1);
        }
        act_data = (reg_data & field->GetMask()) >> field->GetStartBit();
        valuename = "LW_PGC6_SCI_GPIO_OUTPUT_CNTL_PMGR_OUT_DATA_HIGH";
        value = field->FindValue(valuename);
        if (!value)
        {
            ErrPrintf("GC6PlusMisc: Failed to find %s value\n", valuename);
            return (1);
        }
        exp_data = value->GetValue();
        if (exp_data != act_data)
        {
            ErrPrintf("GC6PlusMisc: Mismatch in field %s: expected = %d, "
                "actual = %d\n", field->GetName(), exp_data, act_data);
            m_ErrCount++;
        }
        fieldname = "LW_PGC6_SCI_GPIO_OUTPUT_CNTL_PMGR_OUT_ENAB";
        field = reg->FindField(fieldname);
        if (!field)
        {
            ErrPrintf("GC6PlusMisc: Failed to find %s field\n", fieldname);
            return (1);
        }
        act_data = (reg_data & field->GetMask()) >> field->GetStartBit();
        valuename = "LW_PGC6_SCI_GPIO_OUTPUT_CNTL_PMGR_OUT_ENAB_ACTIVE";
        value = field->FindValue(valuename);
        if (!value)
        {
            ErrPrintf("GC6PlusMisc: Failed to find %s value\n", valuename);
            return (1);
        }
        exp_data = value->GetValue();
        if (exp_data != act_data)
        {
            ErrPrintf("GC6PlusMisc: Mismatch in field %s: expected = %d, "
                "actual = %d\n", field->GetName(), exp_data, act_data);
            m_ErrCount++;
        }

        // restore original PMGR configuration 
        if (restore_pmgr_gpio(gpio_index, pmgr_gpio_output_cntl))
        {
            ErrPrintf("GC6PlusMisc: Failed to disable PMGR GPIO\n");
            return (1);
        }
    }

    return (0);
}

INT32
GC6PlusMisc::check_gpio_min_max(UINT32 array_size)
{
    INT32 max_index;

    // Determine the start and ending GPIO index.
    // If -gpio was specified, test 1 GPIO index only.
    // Otherwise, test between -gpio_min and -gpio_max.
    max_index = array_size - 1;
    if (m_gpio_min < 0)
    {
        m_gpio_min = 0;
    }
    if (m_gpio_max < 0)
    {
        m_gpio_max = max_index;
    }
    if (m_gpio >= 0)
    {
        // Test one GPIO
        m_gpio_min = m_gpio;
        m_gpio_max = m_gpio;
    }

    // Error check the gpio_min and gpio_max indexes
    if (m_gpio_min > max_index)
    {
        ErrPrintf("GC6PlusMisc: Illegal GPIO min index = %d (range 0 to %d)\n",
            m_gpio_min, max_index);
        m_ErrCount++;
        return (1);
    }
    if (m_gpio_max > max_index)
    {
        ErrPrintf("GC6PlusMisc: Illegal GPIO max index = %d (range 0 to %d)\n",
            m_gpio_max, max_index);
        m_ErrCount++;
        return (1);
    }
    if (m_gpio_min > m_gpio_max)
    {
        ErrPrintf("GC6PlusMisc: No GPIO(s) selected (gpio_min = %d, "
            "gpio_max = %d)\n", m_gpio_min, m_gpio_max);
        m_ErrCount++;
        return (1);
    }

    return (0);
}

INT32
GC6PlusMisc::get_tb_gpio_cfg(UINT32 index, UINT32 *drive, UINT32 *pull)
{
    char esc_name[22] = {0} ;
    UINT32 rd_data;

    InfoPrintf("GC6PlusMisc: Getting Testbench GPIO %d configuration\n", index);

    // queury the drive mode
    if (sprintf(esc_name, "Gpio_%d_drv", index) < 0)
    {
        ErrPrintf("GC6PlusMisc: Failed to generate GPIO escape name\n");
        return (1);
    }

    Platform::EscapeRead(esc_name, 0, 1, &rd_data);
    InfoPrintf("GC6PlusMisc: Drive EscapeRead %s = %d\n", esc_name, rd_data);

    for (int i=8; i<22; i++)
    {
        esc_name[i] = '\0';
    }

    Platform::EscapeRead(esc_name, 0, 1, &rd_data);
    InfoPrintf("GC6PlusMisc: EscapeRead %s = %d\n", esc_name, rd_data);
    *drive = rd_data;

    // queury the weakpull
    if (sprintf(esc_name, "Gpio_%d_pull", index) < 0)
    {
        ErrPrintf("GC6PlusMisc: Failed to generate GPIO escape name\n");
        return (1);
    }

    Platform::EscapeRead(esc_name, 0, 1, &rd_data);
    InfoPrintf("GC6PlusMisc: Pull EscapeRead %s = %d\n", esc_name, rd_data);
    
    for (int i=8; i<22; i++)
    {
        esc_name[i] = '\0';
    }

    Platform::EscapeRead(esc_name, 0, 1, &rd_data);
    InfoPrintf("GC6PlusMisc: EscapeRead %s = %d\n", esc_name, rd_data);
    *pull = rd_data;

    return (0);
}

INT32
GC6PlusMisc::cfg_tb_gpio(UINT32 index, UINT32 drive, UINT32 pull)
{
    char esc_name[22] = {0};
    UINT32 check_data = 0;

    InfoPrintf("GC6PlusMisc: Reconfiguring Testbench GPIO %d\n", index);

    // configure the drive mode
    if (sprintf(esc_name, "Gpio_%d_drv", index) < 0)
    {
        ErrPrintf("GC6PlusMisc: Failed to generate GPIO escape name\n");
        return (1);
    }
    InfoPrintf("GC6PlusMisc: EscapeWrite %s = %d\n", esc_name, drive);
    Platform::EscapeWrite(esc_name, 0, 1, drive);
    Platform::EscapeRead(esc_name, 0, 1, &check_data);
    if (check_data != drive)
    {
        ErrPrintf("GC6PlusMisc: Failure in EscapeWrite of %s.  Wrote %d, "
            "Read %d\n", esc_name, drive, check_data);
        m_ErrCount++;
    }

    // configure the weakpull
    if (sprintf(esc_name, "Gpio_%d_pull", index) < 0)
    {
        ErrPrintf("GC6PlusMisc: Failed to generate GPIO escape name\n");
        return (1);
    }
    InfoPrintf("GC6PlusMisc: EscapeWrite %s = %d\n", esc_name, pull);
    Platform::EscapeWrite(esc_name, 0, 1, pull);
    Platform::EscapeRead(esc_name, 0, 1, &check_data);
    if (check_data != pull)
    {
        ErrPrintf("GC6PlusMisc: Failure in EscapeWrite of %s.  Wrote %d, "
            "Read %d\n", esc_name, pull, check_data);
        m_ErrCount++;
    }

    return (0);
}

INT32
GC6PlusMisc::disable_pmgr_intr()
{
    unique_ptr<IRegisterClass> reg;
    const char *regname;
    UINT32 reg_addr, reg_data;

    InfoPrintf("GC6PlusMisc: Disabling PMGR interrupts\n");

    regname = "LW_PMGR_RM_INTR_MSK_GPIO_LIST_1";
    reg = m_regMap->FindRegister(regname);
    if (!reg)
    {
        ErrPrintf("GC6PlusMisc: Failed to find %s register\n", regname);
        return (1);
    }
    reg_addr = reg->GetAddress();
    reg_data = 0;
    lwgpu->RegWr32(reg_addr, reg_data);
    InfoPrintf("GC6PlusMisc: Writing %s = 0x%08x\n", reg->GetName(), reg_data);

    regname = "LW_PMGR_RM_INTR_MSK_GPIO_LIST_2";
    reg = m_regMap->FindRegister(regname);
    if (!reg)
    {
        ErrPrintf("GC6PlusMisc: Failed to find %s register\n", regname);
        return (1);
    }
    reg_addr = reg->GetAddress();
    lwgpu->RegWr32(reg_addr, reg_data);
    InfoPrintf("GC6PlusMisc: Writing %s = 0x%08x\n", reg->GetName(), reg_data);

    return (0);
}

INT32
GC6PlusMisc::disable_pmgr_gpio(UINT32 index, UINT32 *data)
{
    unique_ptr<IRegisterClass> reg;
    unique_ptr<IRegisterField> field;
    unique_ptr<IRegisterValue> value;
    UINT32 reg_addr, reg_data;
    string regname, fieldname, valuename;

    InfoPrintf("GC6PlusMisc: Disabling PMGR GPIO %d\n", index);

    //Disabling PMGR GPIO
    regname = Utility::StrPrintf("LW_GPIO_OUTPUT_CNTL");
    if (regname.c_str() < 0)
    {
        ErrPrintf("GC6PlusMisc: Failed to generate GPIO config regname\n");
        return (1);
    }
    
    if (!(reg = m_regMap->FindRegister(regname.c_str())))
    {
        ErrPrintf("GC6PlusMisc: Failed to find %s register\n", regname.c_str());
        return (1);
    }
    
    reg_addr = reg->GetAddress(index);

    //Read register and save away original data
    reg_data = lwgpu->RegRd32(reg_addr);
    InfoPrintf("GC6PlusMisc: Reading %s = 0x%08x\n", reg->GetName(), reg_data);

    //Pass back original value for restoring later
    *data = reg_data;

    fieldname =  "LW_GPIO_OUTPUT_CNTL_SEL";
    if (!(field = reg->FindField(fieldname.c_str())))
    {
        ErrPrintf("GC6PlusMisc: Failed to find %s field\n", fieldname.c_str());
        return (1);
    }

    InfoPrintf("GC6PlusMisc: Setting PMGR GPIO %d SEL field to NORMAL \n",index);
    valuename = "LW_GPIO_OUTPUT_CNTL_SEL_NORMAL"; 
    if (!(value = field->FindValue(valuename.c_str())))
    {
   	    ErrPrintf("GC6PlusMisc: Failed to find %s value\n", valuename.c_str());
   	    return (1);
   	}
    reg_data = value->GetValue() << field->GetStartBit(); 
        
    fieldname =  "LW_GPIO_OUTPUT_CNTL_IO_OUT_EN";
    if (!(field = reg->FindField(fieldname.c_str())))
    {
        ErrPrintf("GC6PlusMisc: Failed to find %s field\n", fieldname.c_str());
        return (1);
    }

    InfoPrintf("GC6PlusMisc: Setting PMGR GPIO %d OUT EN field to NO \n",index);
    valuename = "LW_GPIO_OUTPUT_CNTL_IO_OUT_EN_NO"; 
    if (!(value = field->FindValue(valuename.c_str())))
    {
   	    ErrPrintf("GC6PlusMisc: Failed to find %s value\n", valuename.c_str());
   	    return (1);
   	}
    reg_data |= value->GetValue() << field->GetStartBit(); 

    fieldname =  "LW_GPIO_OUTPUT_CNTL_IO_OUTPUT";
    if (!(field = reg->FindField(fieldname.c_str())))
    {
        ErrPrintf("GC6PlusMisc: Failed to find %s field\n", fieldname.c_str());
        return (1);
    }

    InfoPrintf("GC6PlusMisc: Setting PMGR GPIO %d IO OUPUT field to 0 \n",index);
    valuename = "LW_GPIO_OUTPUT_CNTL_IO_OUTPUT_0"; 
    if (!(value = field->FindValue(valuename.c_str())))
    {
   	    ErrPrintf("GC6PlusMisc: Failed to find %s value\n", valuename.c_str());
   	    return (1);
   	}
    reg_data |= value->GetValue() << field->GetStartBit(); 

    fieldname =  "LW_GPIO_OUTPUT_CNTL_IO_OUT_ILW";
    if (!(field = reg->FindField(fieldname.c_str())))
    {
        ErrPrintf("GC6PlusMisc: Failed to find %s field\n", fieldname.c_str());
        return (1);
    }

    InfoPrintf("GC6PlusMisc: Setting PMGR GPIO %d ILW Mode to Disable \n",index);
    valuename = "LW_GPIO_OUTPUT_CNTL_IO_OUT_ILW_DISABLE"; 
    if (!(value = field->FindValue(valuename.c_str())))
    {
   	    ErrPrintf("GC6PlusMisc: Failed to find %s value\n", valuename.c_str());
   	    return (1);
   	}
    reg_data |= value->GetValue() << field->GetStartBit(); 

    fieldname =  "LW_GPIO_OUTPUT_CNTL_OPEN_DRAIN";
    if (!(field = reg->FindField(fieldname.c_str())))
    {
        ErrPrintf("GC6PlusMisc: Failed to find %s field\n", fieldname.c_str());
        return (1);
    }

    InfoPrintf("GC6PlusMisc: Setting PMGR GPIO %d OPEN DRAIN MODE is Disabled \n",index);
    valuename = "LW_GPIO_OUTPUT_CNTL_OPEN_DRAIN_DISABLE"; 
    if (!(value = field->FindValue(valuename.c_str())))
    {
   	    ErrPrintf("GC6PlusMisc: Failed to find %s value\n", valuename.c_str());
   	    return (1);
   	}
    reg_data |= value->GetValue() << field->GetStartBit(); 
    lwgpu->RegWr32(reg_addr, reg_data);
    InfoPrintf("GC6PlusMisc: Writing %s = 0x%08x\n", reg->GetName(),reg_data);

    // trigger PMGR GPIO update
    if (trigger_pmgr_gpio_update())
    {
        ErrPrintf("GC6PlusMisc: Failure in triggering PMGR GPIO update\n");
        return (1);
    }

    return (0);
}

INT32
GC6PlusMisc::configure_pmgr_gpio(UINT32 index)
{
    unique_ptr<IRegisterClass> reg;
    unique_ptr<IRegisterField> field;
    unique_ptr<IRegisterValue> value;
    UINT32 reg_addr, reg_data;
    string regname, fieldname, valuename;

    InfoPrintf("GC6PlusMisc: Configuring PMGR GPIO %d\n", index);

    //Configuring PMGR GPIO
    regname = Utility::StrPrintf("LW_GPIO_OUTPUT_CNTL");
    if (regname.c_str() < 0)
    {
        ErrPrintf("GC6PlusMisc: Failed to generate GPIO config regname\n");
        return (1);
    }
    
    if (!(reg = m_regMap->FindRegister(regname.c_str())))
    {
        ErrPrintf("GC6PlusMisc: Failed to find %s register\n", regname.c_str());
        return (1);
    }
    
    reg_addr = reg->GetAddress(index);

    //Read register and print the original data before reading
    reg_data = lwgpu->RegRd32(reg_addr);
    InfoPrintf("GC6PlusMisc: Reading %s = 0x%08x\n", reg->GetName(), reg_data);

    fieldname =  "LW_GPIO_OUTPUT_CNTL_SEL";
    if (!(field = reg->FindField(fieldname.c_str())))
    {
        ErrPrintf("GC6PlusMisc: Failed to find %s field\n", fieldname.c_str());
        return (1);
    }

    InfoPrintf("GC6PlusMisc: Setting PMGR GPIO %d SEL field to NORMAL \n",index);
    valuename = "LW_GPIO_OUTPUT_CNTL_SEL_NORMAL"; 
    if (!(value = field->FindValue(valuename.c_str())))
    {
   	    ErrPrintf("GC6PlusMisc: Failed to find %s value\n", valuename.c_str());
   	    return (1);
   	}
    reg_data = value->GetValue() << field->GetStartBit(); 
        
    fieldname =  "LW_GPIO_OUTPUT_CNTL_IO_OUT_EN";
    if (!(field = reg->FindField(fieldname.c_str())))
    {
        ErrPrintf("GC6PlusMisc: Failed to find %s field\n", fieldname.c_str());
        return (1);
    }

    InfoPrintf("GC6PlusMisc: Setting PMGR GPIO %d OUT EN field to YES \n",index);
    valuename = "LW_GPIO_OUTPUT_CNTL_IO_OUT_EN_YES"; 
    if (!(value = field->FindValue(valuename.c_str())))
    {
   	    ErrPrintf("GC6PlusMisc: Failed to find %s value\n", valuename.c_str());
   	    return (1);
   	}
    reg_data |= value->GetValue() << field->GetStartBit(); 

    fieldname =  "LW_GPIO_OUTPUT_CNTL_IO_OUTPUT";
    if (!(field = reg->FindField(fieldname.c_str())))
    {
        ErrPrintf("GC6PlusMisc: Failed to find %s field\n", fieldname.c_str());
        return (1);
    }

    InfoPrintf("GC6PlusMisc: Setting PMGR GPIO %d IO OUPUT field to 1 \n",index);
    valuename = "LW_GPIO_OUTPUT_CNTL_IO_OUTPUT_1"; 
    if (!(value = field->FindValue(valuename.c_str())))
    {
   	    ErrPrintf("GC6PlusMisc: Failed to find %s value\n", valuename.c_str());
   	    return (1);
   	}
    reg_data |= value->GetValue() << field->GetStartBit(); 

    fieldname =  "LW_GPIO_OUTPUT_CNTL_IO_OUT_ILW";
    if (!(field = reg->FindField(fieldname.c_str())))
    {
        ErrPrintf("GC6PlusMisc: Failed to find %s field\n", fieldname.c_str());
        return (1);
    }

    InfoPrintf("GC6PlusMisc: Setting PMGR GPIO %d ILW Mode to Disable \n",index);
    valuename = "LW_GPIO_OUTPUT_CNTL_IO_OUT_ILW_DISABLE"; 
    if (!(value = field->FindValue(valuename.c_str())))
    {
   	    ErrPrintf("GC6PlusMisc: Failed to find %s value\n", valuename.c_str());
   	    return (1);
   	}
    reg_data |= value->GetValue() << field->GetStartBit(); 

    fieldname =  "LW_GPIO_OUTPUT_CNTL_OPEN_DRAIN";
    if (!(field = reg->FindField(fieldname.c_str())))
    {
        ErrPrintf("GC6PlusMisc: Failed to find %s field\n", fieldname.c_str());
        return (1);
    }

    InfoPrintf("GC6PlusMisc: Setting PMGR GPIO %d OPEN DRAIN MODE is Disabled \n",index);
    valuename = "LW_GPIO_OUTPUT_CNTL_OPEN_DRAIN_DISABLE"; 
    if (!(value = field->FindValue(valuename.c_str())))
    {
   	    ErrPrintf("GC6PlusMisc: Failed to find %s value\n", valuename.c_str());
   	    return (1);
   	}
    reg_data |= value->GetValue() << field->GetStartBit(); 
    lwgpu->RegWr32(reg_addr, reg_data);
    InfoPrintf("GC6PlusMisc: Writing %s = 0x%08x\n", reg->GetName(),reg_data);

    // trigger PMGR GPIO update
    if (trigger_pmgr_gpio_update())
    {
        ErrPrintf("GC6PlusMisc: Failure in triggering PMGR GPIO update\n");
        return (1);
    }

    return (0);
}

INT32
GC6PlusMisc::restore_pmgr_gpio(UINT32 index, UINT32 reg_data)
{
    unique_ptr<IRegisterClass> reg;
    UINT32 reg_addr;
    string regname;

    InfoPrintf("GC6PlusMisc: Restoring PMGR GPIO %d\n", index);

    regname = Utility::StrPrintf("LW_GPIO_OUTPUT_CNTL");
    if (regname.c_str() < 0)
    {
        ErrPrintf("GC6PlusMisc: Failed to generate GPIO config regname\n");
        return (1);
    }
    
    if (!(reg = m_regMap->FindRegister(regname.c_str())))
    {
        ErrPrintf("GC6PlusMisc: Failed to find %s register\n", regname.c_str());
        return (1);
    }
    
    reg_addr = reg->GetAddress(index);

    //Restore register with original data
    lwgpu->RegWr32(reg_addr, reg_data);
    InfoPrintf("GC6PlusMisc: Writing %s = 0x%08x\n", reg->GetName(), reg_data);

    //Trigger PMGR GPIO update
    if (trigger_pmgr_gpio_update())
    {
        ErrPrintf("GC6PlusMisc: Failure in triggering PMGR GPIO update\n");
        return (1);
    }

    return (0);
}

INT32
GC6PlusMisc::trigger_pmgr_gpio_update()
{
    unique_ptr<IRegisterClass> reg;
    unique_ptr<IRegisterField> field;
    unique_ptr<IRegisterValue> value;
    const char *regname, *fieldname, *valuename;
    UINT32 reg_addr, reg_data;

    InfoPrintf("GC6PlusMisc: Triggering PMGR GPIO update\n");

    regname = "LW_PMGR_GPIO_OUTPUT_CNTL_TRIGGER";
    reg = m_regMap->FindRegister(regname);
    if (!reg)
    {
        ErrPrintf("GC6PlusMisc: Failed to find %s register\n", regname);
        return (1);
    }
    fieldname = "LW_PMGR_GPIO_OUTPUT_CNTL_TRIGGER_UPDATE";
    field = reg->FindField(fieldname);
    if (!field)
    {
        ErrPrintf("GC6PlusMisc: Failed to find %s field\n", fieldname);
        return (1);
    }
    valuename = "LW_PMGR_GPIO_OUTPUT_CNTL_TRIGGER_UPDATE_TRIGGER";
    value = field->FindValue(valuename);
    if (!value)
    {
        ErrPrintf("GC6PlusMisc: Failed to find %s value\n", valuename);
        return (1);
    }

    reg_addr = reg->GetAddress();
    reg_data = (value->GetValue() << field->GetStartBit());
    lwgpu->RegWr32(reg_addr, reg_data);
    InfoPrintf("GC6PlusMisc: Writing %s = 0x%08x\n", reg->GetName(), reg_data);

    // perform a read to flush all writes before continuing with the test
    reg_data = lwgpu->RegRd32(reg_addr);
    InfoPrintf("GC6PlusMisc: Reading %s = 0x%08x\n", reg->GetName(), reg_data);

    return (0);
}

