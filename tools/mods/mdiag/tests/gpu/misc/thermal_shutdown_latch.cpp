/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2009-2011, 2020-2021 by LWPU Corporation.  All rights reserved. 
 * All information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

// To facilitate faster compiles via pre-compiled headers, keep "stdtest.h"
// first.
// For style requirements, all lines of code follow the 80 column rule.
#include "mdiag/tests/stdtest.h"

#include "thermal_shutdown_latch.h"
#include "mdiag/lwgpu.h"
#include "mdiag/utils/lwgpu_channel.h"
#include "mdiag/tests/test_state_report.h"
#include "core/include/gpu.h"

extern const ParamDecl thermal_shutdown_latch_params[] =
{
    PARAM_SUBDECL(lwgpu_single_params),
    LAST_PARAM
};

static int errCnt;

ThermalShutdownLatch::ThermalShutdownLatch(ArgReader *params) :
    LWGpuSingleChannelTest(params)
{
}

ThermalShutdownLatch::~ThermalShutdownLatch(void)
{
}

STD_TEST_FACTORY(thermal_shutdown_latch, ThermalShutdownLatch)

int
ThermalShutdownLatch::Setup(void)
{
    lwgpu = LWGpuResource::FindFirstResource();
    UINT32 arch = lwgpu->GetArchitecture();
    InfoPrintf(" thermal_shutdown_latch : Setup, arch is 0x%x\n", arch);
    m_arch = arch;

    ch = lwgpu->CreateChannel();
    if(!ch)
    {
        ErrPrintf("ThermalShutdownLatch: Setup failed to create channel\n");
        return (0);
    }

    m_regMap = lwgpu->GetRegisterMap();
    if(!m_regMap)
    {
        ErrPrintf("ThermalShutdownLatch: thermal_shutdown_latch can only be "
            "run on GPU's that support a register map!\n");
        return (0);
    }

    getStateReport()->init("thermal_shutdown_latch");
    getStateReport()->enable();

    return 1;
}

void
ThermalShutdownLatch::CleanUp(void)
{
    if(ch)
    {
        delete ch;
        ch = 0;
    }

    if (lwgpu)
    {
        DebugPrintf("ThermalShutdownLatch::CleanUp(): Releasing GPU.\n");
        lwgpu = 0;
    }
    LWGpuSingleChannelTest::CleanUp();
}

void
ThermalShutdownLatch::Run(void)
{
    SetStatus(TEST_INCOMPLETE);
    //  InfoPrintf Flag to determine whether the test fail because of elw issue or test issue.
    InfoPrintf("FLAG : Thermal Shutdown Latch test starting ....\n");
    if(thermal_shutdown_latchTest())
    {
        SetStatus(TEST_FAILED);
        getStateReport()->runFailed("Failed test\n");
        return;
    }
    if (errCnt)
    {
        SetStatus(TEST_FAILED_CRC);
        getStateReport()->crcCheckFailed(
            "Test failed with miscompares, see logfile for messages.\n");
        return;
    }
    SetStatus(TEST_SUCCEEDED);
    getStateReport()->crcCheckPassedGold();
}

// Note: board designs that support turning off the external voltage regulator
// for core lwvdd power due to over temperature conditions must use GPIO[8]
// for this feature.  This is because this feature must operate even before SW
// has had a chance to configure the GPIOs, so naturally the GPIO must be
// hardcoded.  A fuse configuration bit is used to disable this feature in
// board designs where GPIO[8] does not control the external voltage regulator.
// Since the Thermal Shutdown Latch feature is an extention to the core lwvdd
// power shutdown feature, this tests expects the GPIO latch function to be in
// GPIO[8].  Although the power shutdown on overtemp feature exists in Tesla
// and Fermi architectures, the shutdown latch feature is expected to be
// implemented in the Fermi architecture starting with gf11x.
//
// Note: starting with gk110 chips, the over temperature signalling has been
// moved from GPIO[8] to a dedicated OVERT pin.  This pin contains the Thermal
// Shutdown Latch feature while GPIO[8] returns to a generic GPIO pin.  In the
// description below, OVERT refers to the OVERT pin for gk110 chips and after,
// and refers to GPIO[8] for chips between gf11x and gk10x.
//
// Test Description
// When the GPU detects its internal temperature reaching the critical
// threshold,
// it performs an emergency shutdown of the core lwvdd to prevent damage to the
// chip.  This is done by asserting OVERT (active low) to signal the external
// voltage regulator to turn off.  With the core power off, the core logic
// cannot
// maintain an asserted level on OVERT, so the external pullup on OVERT will
// eventually signal the external regulator to turn on the core power.  If the
// GPU is still over-temperature, OVERT will assert and the core power will
// turn off again.  This may cause oscillation of the external regulator, which
// is undesirable.
//
// To avoid the oscillation, a latch is built into the OVERT pad on the 3.3V
// I/O power domain (the I/O power domain is independent from the core power
// domain and does not automatically turn off when the core power is off).  This
// latch maintains the asserted level on OVERT until the latch is cleared
// by PEX reset.
//
// The ThermalShutdownLatch test exercises the over temperature condition
// that assert OVERT and verifies that the latch behaves as specified.
// Note: Although core power is not turned off during this test, the over
// temperature condition is removed and the test verifies that the latch
// maintains the asserted level on the OVERT output.
UINT32
ThermalShutdownLatch::thermal_shutdown_latchTest()
{

    int loop;
    bool shutdown_fuse, shutdown_override, latch_enabled;
    UINT32 overt, force_latch_reset;
    UINT32 gpio8;
    errCnt = 0;

    if (check_therm_shutdown_support())
    {
        return (1);
    }

    // configure overt alert to be active
    InfoPrintf("ThermalShutdownLatch: Configuring overt alert to be active\n");
    if (lwgpu->SetRegFldDef("LW_THERM_SENSOR_6", "_POWER", "_ON"))
    {
        ErrPrintf("ThermalShutdownLatch: Failed to configure "
            "LW_THERM_SENSOR_6_POWER\n");
        return (1);
    }
    if (lwgpu->SetRegFldDef("LW_THERM_SENSOR_0", "_TEMP_SELECT", "_OVERRIDE"))
    {
        ErrPrintf("ThermalShutdownLatch: Failed to configure "
            "LW_THERM_SENSOR_0_TEMP_SELECT\n");
        return (1);
    }
    if (lwgpu->SetRegFldNum("LW_THERM_SENSOR_0", "_TEMP_OVERRIDE", 125<<5)) //125C
    {
        ErrPrintf("ThermalShutdownLatch: Failed to configure "
            "LW_THERM_SENSOR_0_TEMP_OVERRIDE\n");
        return (1);
    }
    if (lwgpu->SetRegFldNum("LW_THERM_SENSOR_1", "_TS_STABLE_CNT", 0))
    {
        ErrPrintf("ThermalShutdownLatch: Failed to configure "
            "LW_THERM_SENSOR_1_TS_STABLE_CNT\n");
        return (1);
    }
    if (lwgpu->SetRegFldDef("LW_THERM_SENSOR_1", "_TS_STABLE_CNT_RST", "_CNT"))
    {
        ErrPrintf("ThermalShutdownLatch: Failed to configure "
            "LW_THERM_SENSOR_1_TS_STABLE_CNT_RST\n");
        return (1);
    }
    if (lwgpu->SetRegFldNum("LW_THERM_POWER_6", "_THERMAL_FILTER_PERIOD", 0))
    {
        ErrPrintf("ThermalShutdownLatch: Failed to configure "
            "LW_THERM_POWER_6_THERMAL_FILTER_PERIOD\n");
        return (1);
    }

    InfoPrintf("ThermalShutdownLatch: Configuring  LW_THERM_OVERT_CTRL_OTOB to be disable\n");
    if (lwgpu->SetRegFldNum("LW_THERM_OVERT_CTRL", "_OTOB_ENABLE", 0))
    {
        ErrPrintf("ThermalShutdownLatch: Failed to configure "
            "LW_THERM_OVERT_CTRL_OTOB\n");
        return (1);
    }

    // overt latch in lwvdd domain was added later
    if (lwgpu->SetRegFldDef("LW_THERM_OVERT_CTRL", "_SEL", "_OVERT_PAD"))
    {
        ErrPrintf("ThermalShutdownLatch: Failed to configure "
            "LW_THERM_OVERT_CTRL_SEL_OVERT_PAD\n");
        return (1);
    }
    // Don't let overt affect split rail reset
    if (lwgpu->SetRegFldDef("LW_FUSE_OPT_SPLIT_RAIL_OVERT_BYPASS_FUSE", "_DATA", "_ENABLE")) {
        ErrPrintf("ThermalShutdownLatch: Failed to configure "
            "LW_FUSE_OPT_SPLIT_RAIL_OVERT_BYPASS_FUSE_DATA_ENABLE\n");
        return (1);
    }

    // The OVERT threshold is in LW_THERM_I2CS_SENSOR_20, but has moved to
    // LW_THERM_EVT_OVERT in gk110.  We need to check the other register if
    // the write to the first one fails.
    if (lwgpu->SetRegFldNum("LW_THERM_EVT_THERMAL_0", "_TEMP_THRESHOLD", 70<<5))
    {
        ErrPrintf("ThermalShutdownLatch: Failed to configure "
            "LW_THERM_EVT_THERMAL_0_TEMP_THRESHOLD.\n");
        return (1);
    }
    if (lwgpu->SetRegFldDef("LW_THERM_OVERT_EN", "_THERMAL_0", "_ENABLED"))
    {
        ErrPrintf("ThermalShutdownLatch: Failed to configure THERMAL_0 to OVERT enabled.\n");
        return (1);
    }

    // go through 4 loops:
    //   1: fuse disabled, no priv override
    //   2: fuse enabled, no priv override
    //   3: fuse disabled with priv override
    //   4: fuse enabled with priv override
    // in each loop, assert overt condition and check OVERT, then deassert overt
    // condition and check OVERT

    //set lw_fuse_en_sw_override to 1 before writing fuse registers
    if (config_en_sw_override_fuse(0x1))
    {
        return (1);
    }

    for (loop = 0; loop < 4; loop++)
    {
        shutdown_fuse = loop & 0x1;
        shutdown_override = (loop >> 1) & 0x1;

        // decode the configuration and summarize whether the latch feature is
        // enabled or not
        // when override is enabled, always override priv enable/disable to
        // opposite of fuse setting
        latch_enabled = (shutdown_fuse ^ shutdown_override);
        InfoPrintf("ThermalShutdownLatch: Testing with thermal shutdown latch "
            "feature %s\n", latch_enabled ? "enabled" : "disabled");

        // configure feature through fuse and priv override
        if (config_therm_shutdown_fuse(shutdown_fuse))
        {
            return (1);
        }
        if (config_therm_shutdown_priv_enable_disable(shutdown_override &&
            !shutdown_fuse, shutdown_override &&  shutdown_fuse))
        {
            return (1);
        }

        // generate overt alert on OVERT and verify thermal shutdown is active
        // (polarity is active low)
        if (config_overtemp_on_boot_fuse(1))
        {
            return (1);
        }
        if (check_therm_shutdown_state(0))
        {
            return (1);
        }

        // remove overt alert and recheck thermal shutdown based on whether the
        // latching function is enabled
        if (config_overtemp_on_boot_fuse(0))
        {
            return (1);
        }
        if (check_therm_shutdown_state(!latch_enabled))
        {
            return (1);
        }

        // clear latch for next loop
        clear_therm_shutdown_latch();
    }

    // this section checks the latch clearing behavior
    // when PEX reset (connected to HV_HIZ_N of pad) is asserted, the latch
    // should always clear
    // in case the ZI port of the I/O pad is fenced with PEX reset, we rely on
    // EscapeRead to check OVERT output
    // first, check that OVERT can assert when PEX reset is deasserted
    if (config_therm_shutdown_priv_enable_disable(1,0))
    {
        return (1);
    }
    if (config_overtemp_on_boot_fuse(1))
    {
        return (1);
    }
    InfoPrintf("ThermalShutdownLatch: Checking Thermal Shutdown state on "
        "OVERT through EscapeRead\n");

    if ((m_arch & 0xff0) >= 0x610) {
        Platform::EscapeRead("overt", 0, 4, &overt);
        InfoPrintf("ThermalShutdownLatch: EscapeRead of OVERT = 0x%0x\n", overt);
        if (overt != 0)
        {
            ErrPrintf("ThermalShutdownLatch: Thermal Shutdown on OVERT (active "
                      "low) is not active when PEX Reset is deasserted: detected %d, "
                      "expected %d\n", overt, 0);
            errCnt++;
        }

        // second, check that OVERT cannot assert when PEX reset is asserted
        InfoPrintf("ThermalShutdownLatch: Asserting PEX_reset_ on Thermal "
                   "Shutdown pad (OVERT)\n");
        Platform::EscapeWrite("force_thermal_shutdown_latch_reset", 0, 4, 1);
        Platform::EscapeRead("force_thermal_shutdown_latch_reset", 0, 4,
                             &force_latch_reset);
        if (force_latch_reset != 1)
        {
            ErrPrintf("ThermalShutdownLatch: Unexpected failure in EscapeWrite of "
                      "force_thermal_shutdown_latch_reset\n");
            errCnt++;
        }
        Platform::Delay(1);
        Platform::EscapeRead("overt", 0, 4, &overt);
        InfoPrintf("ThermalShutdownLatch: EscapeRead of OVERT = 0x%0x\n", overt);
        if (overt != 1)
        {
            ErrPrintf("ThermalShutdownLatch: PEX Reset did not disable Thermal "
                      "Shutdown on OVERT (active low): detected %d, expected %d\n", overt, 1);
            errCnt++;
        }
    } else {
        Platform::EscapeRead("gpio_8", 0, 4, &gpio8);
        InfoPrintf("ThermalShutdownLatch: EscapeRead of OVERT = 0x%0x\n", gpio8);
        if (gpio8 != 0)
        {
            ErrPrintf("ThermalShutdownLatch: Thermal Shutdown on OVERT (active "
                      "low) is not active when PEX Reset is deasserted: detected %d, "
                      "expected %d\n", gpio8, 0);
            errCnt++;
        }

        // second, check that OVERT cannot assert when PEX reset is asserted
        InfoPrintf("ThermalShutdownLatch: Asserting PEX_reset_ on Thermal "
                   "Shutdown pad (OVERT)\n");
        Platform::EscapeWrite("force_thermal_shutdown_latch_reset", 0, 4, 1);
        Platform::EscapeRead("force_thermal_shutdown_latch_reset", 0, 4,
                             &force_latch_reset);
        if (force_latch_reset != 1)
        {
            ErrPrintf("ThermalShutdownLatch: Unexpected failure in EscapeWrite of "
                      "force_thermal_shutdown_latch_reset\n");
            errCnt++;
        }
        Platform::Delay(1);
        Platform::EscapeRead("gpio_8", 0, 4, &gpio8);
        InfoPrintf("ThermalShutdownLatch: EscapeRead of OVERT = 0x%0x\n", gpio8);
        if (gpio8 != 1)
        {
            ErrPrintf("ThermalShutdownLatch: PEX Reset did not disable Thermal "
                      "Shutdown on OVERT (active low): detected %d, expected %d\n", gpio8, 1);
            errCnt++;
        }
    }

    // release reset on OVERT prior to ending test
    Platform::EscapeWrite("force_thermal_shutdown_latch_reset", 0, 4, 0);

    // done with test
    InfoPrintf("Thermal Shutdown Latch test completed with %d errors\n",
        errCnt);
    return (0);  // calling routine will check errCnt for CRC fail
}

int
ThermalShutdownLatch::check_therm_shutdown_support()
{
    unique_ptr<IRegisterClass> reg;
    unique_ptr<IRegisterField> field;

    // identify if the GPU supports the Thermal Shutdown Latch feature
    // we will check this by testing whether the priv override fields for the
    // fuse exists
    InfoPrintf("ThermalShutdownLatch: Checking whether GPU supports Thermal "
        "Shutdown Latch feature\n");
    if ((reg = m_regMap->FindRegister("LW_THERM_OVERT_CTRL")))
    {
        if (!(field = reg->FindField(
            "LW_THERM_OVERT_CTRL_THERM_SHUTDOWN_LATCH_DISABLE")))
        {
            ErrPrintf("ThermalShutdownLatch: Cannot find "
                "LW_THERM_CTRL1_THERM_SHUTDOWN_LATCH_DISABLE field, "
                "Thermal Shutdown Latch is not supported\n");
            ErrPrintf("ThermalShutdownLatch: Please recompile RTL with "
                "LW_CHIP_THERM_SHUTDOWN_LATCH defined\n");
            return (1);
        }
        if (!(field = reg->FindField(
            "LW_THERM_OVERT_CTRL_THERM_SHUTDOWN_LATCH_ENABLE")))
        {
            ErrPrintf("ThermalShutdownLatch: Cannot find "
                "LW_THERM_CTRL1_THERM_SHUTDOWN_LATCH_ENABLE field, "
                "Thermal Shutdown Latch is not supported\n");
            ErrPrintf("ThermalShutdownLatch: Please recompile RTL with "
                "LW_CHIP_THERM_SHUTDOWN_LATCH defined\n");
            return (1);
        }
    }
    else
    {
        ErrPrintf("ThermalShutdownLatch: Register LW_THERM_CTRL1 does not "
            "exist.  Cannot determine whether Thermal Shutdown Latch is "
            "supported\n");
        return (1);
    }
    InfoPrintf("ThermalShutdownLatch: Thermal Shutdown Latch feature is "
        "supported\n");

    // indicate in logfile whether over temperature is supported with a
    // dedicated OVERT pin or on GPIO[8]
    if ((reg = m_regMap->FindRegister("LW_THERM_OVERT_CTRL")))
    {
        InfoPrintf("ThermalShutdownLatch: Over Temperature is reported on "
            "dedicated OVERT pin\n");
    }
    else
    {
        InfoPrintf("ThermalShutdownLatch: Over Temperature is reported on "
            "GPIO[8]\n");
    }
    return (0);
}

int
ThermalShutdownLatch::config_therm_shutdown_fuse(bool value)
{
    unique_ptr<IRegisterClass> reg;
    unique_ptr<IRegisterField> field;
    UINT32 reg_addr, reg_data;

    // program the fuse for the thermal shutdown latch feature
    InfoPrintf("ThermalShutdownLatch: Configuring "
        "LW_FUSE_OPT_THERM_SHUTDOWN_LATCH fuse to %d\n", value);
    if ((reg = m_regMap->FindRegister("LW_FUSE_OPT_THERM_SHUTDOWN_LATCH")))
    {
        reg_addr = reg->GetAddress();
        if ((field = reg->FindField("LW_FUSE_OPT_THERM_SHUTDOWN_LATCH_DATA")))
        {
            reg_data = (value << field->GetStartBit());
        }
        else
        {
            ErrPrintf("ThermalShutdownLatch: Failed to find "
                "LW_FUSE_OPT_THERM_SHUTDOWN_LATCH_DATA field\n");
            return (1);
        }
        lwgpu->RegWr32(reg_addr, reg_data);
    }
    else
    {
        ErrPrintf("ThermalShutdownLatch: Failed to find "
            "LW_FUSE_OPT_THERM_SHUTDOWN_LATCH register\n");
        return (1);
    }
    return (0);
}

int
ThermalShutdownLatch::config_therm_shutdown_priv_enable_disable(bool enable,
    bool disable)
{
    unique_ptr<IRegisterClass> reg;
    unique_ptr<IRegisterField> field;
    UINT32 reg_addr, reg_data;

    // program the priv override for the thermal shutdown latch feature
    InfoPrintf("ThermalShutdownLatch: Configuring LW_THERM_OVERT_CTRL_THERM_"
        "SHUTDOWN_LATCH_ENABLE/DISABLE fields to %d/%d\n", enable, disable);
    if ((reg = m_regMap->FindRegister("LW_THERM_OVERT_CTRL")))
    {
        reg_addr = reg->GetAddress();
        reg_data = lwgpu->RegRd32(reg_addr);
        if ((field = reg->FindField(
            "LW_THERM_OVERT_CTRL_THERM_SHUTDOWN_LATCH_ENABLE")))
        {
            reg_data = (reg_data & ~field->GetMask()) | (enable <<
                field->GetStartBit());
        }
        else
        {
            ErrPrintf("ThermalShutdownLatch: Failed to find "
                "LW_THERM_OVERT_CTRL_THERM_SHUTDOWN_LATCH_ENABLE field\n");
            return (1);
        }
        if ((field = reg->FindField(
            "LW_THERM_OVERT_CTRL_THERM_SHUTDOWN_LATCH_DISABLE")))
        {
            reg_data = (reg_data & ~field->GetMask()) | (disable <<
                field->GetStartBit());
        }
        else
        {
            ErrPrintf("ThermalShutdownLatch: Failed to find "
                "LW_THERM_OVERT_CTRL_THERM_SHUTDOWN_LATCH_DISABLE field\n");
            return (1);
        }
        lwgpu->RegWr32(reg_addr, reg_data);
    }
    else
    {
        ErrPrintf("ThermalShutdownLatch: Failed to find LW_THERM_OVERT_CTRL "
            "register\n");
        return (1);
    }
    return (0);
}

int
ThermalShutdownLatch::config_overtemp_on_boot_fuse(bool value)
{
    unique_ptr<IRegisterClass> reg;
    unique_ptr<IRegisterField> field;
    UINT32 reg_addr, reg_data;

    // program the fuse for the over temperature reporting on boot
    InfoPrintf("ThermalShutdownLatch: Configuring "
        "LW_FUSE_OPT_INT_TS_OTOB_EN_DATA fuse to %d\n", value);
    if ((reg = m_regMap->FindRegister("LW_FUSE_OPT_INT_TS_OTOB_EN")))
    {
        reg_addr = reg->GetAddress();
        if ((field = reg->FindField("LW_FUSE_OPT_INT_TS_OTOB_EN_DATA")))
        {
            reg_data = (value << field->GetStartBit());
        }
        else
        {
            ErrPrintf("ThermalShutdownLatch: Failed to find "
                "LW_FUSE_OPT_INT_TS_OTOB_EN_DATA field\n");
            return (1);
        }
        lwgpu->RegWr32(reg_addr, reg_data);
        // verify that the fuse was written correctly
        // this also serves to flush the previous write before checking the
        // thermal shutdown state
        reg_data = lwgpu->RegRd32(reg_addr);
        if (value != ((reg_data & field->GetMask()) >> field->GetStartBit()))
        {
            ErrPrintf("ThermalShutdownLatch: Failed to write "
                "LW_FUSE_OPT_INT_TS_OTOB_EN_DATA to %d\n", value);
            return (1);
        }
    }
    else
    {
        ErrPrintf("ThermalShutdownLatch: Failed to find "
            "LW_FUSE_OPT_INT_TS_OTOB_EN register\n");
        return (1);
    }
    return (0);
}

int
ThermalShutdownLatch::config_en_sw_override_fuse(bool value)
{
    unique_ptr<IRegisterClass> reg;
    unique_ptr<IRegisterField> field;
    UINT32 reg_addr, reg_data;

    InfoPrintf("ThermalShutdownLatch: Configuring "
        "LW_FUSE_EN_SW_OVERRIDE fuse to %d\n", value);
    if ((reg = m_regMap->FindRegister("LW_FUSE_EN_SW_OVERRIDE")))
    {
        reg_addr = reg->GetAddress();
        if ((field = reg->FindField("LW_FUSE_EN_SW_OVERRIDE_VAL")))
        {
            reg_data = (value << field->GetStartBit());
        }
        else
        {
            ErrPrintf("ThermalShutdownLatch: Failed to find "
                "LW_FUSE_EN_SW_OVERRIDE_VAL field\n");
            return (1);
        }
        lwgpu->RegWr32(reg_addr, reg_data);

        reg_data = lwgpu->RegRd32(reg_addr);
        if (value != ((reg_data & field->GetMask()) >> field->GetStartBit()))
        {
            ErrPrintf("ThermalShutdownLatch: Failed to write "
                "LW_FUSE_EN_SW_OVERRIDE_VAL to %d\n", value);
            return (1);
        }
    }
    else
    {
        ErrPrintf("ThermalShutdownLatch: Failed to find "
            "LW_FUSE_EN_SW_OVERRIDE register\n");
        return (1);
    }
    return (0);
}

int
ThermalShutdownLatch::check_therm_shutdown_state(bool value)
{
    unique_ptr<IRegisterClass> reg;
    unique_ptr<IRegisterField> field;
    UINT32 reg_addr, reg_data;
    UINT32 therm_shutdown;

    // the over temperature output can be on the dedicated OVERT pin or on
    // GPIO[8]
    // the existance of LW_THERM_OVERT_CTRL implies a dedicated OVERT pin
    // otherwise, assume it is on GPIO[8]
    if ((reg = m_regMap->FindRegister("LW_THERM_OVERT_CTRL")))
    {
        InfoPrintf("ThermalShutdownLatch: Checking Thermal Shutdown state\n");
        reg_addr = reg->GetAddress();
        if ((field = reg->FindField("LW_THERM_OVERT_CTRL_EXT_OVERT_ASSERTED")))
        {
            reg_data = lwgpu->RegRd32(reg_addr);
            therm_shutdown = (reg_data & field->GetMask()) >>
                field->GetStartBit();
        }
        else
        {
            ErrPrintf("ThermalShutdownLatch: Failed to find "
                "LW_THERM_OVERT_CTRL_EXT_OVERT_ASSERTED field\n");
            return (1);
        }
        InfoPrintf("ThermalShutdownLatch: %s = 0x%x\n", field->GetName(),
            therm_shutdown);
        // EXT_OVERT_ASSERTED returns the logical value, but we want to compare
        // against the physical value.
        // Since the polarity is active low, we ilwert the logical value to get
        // the physical value.
        therm_shutdown = !therm_shutdown;
    }
    else
    {
        // check PMGR registers for the location of the GPIO[8] input state
        // the definition changed within fermi, so we need to check in two
        // places
        // it will be in the LW_PMGR_GPIO_CNTL2_GPIO_8_INPUT or
        // in LW_PMGR_GPIO_8_OUTPUT_CNTL_IO_INPUT
        InfoPrintf("ThermalShutdownLatch: Checking Thermal Shutdown state on "
            "GPIO[8]\n");
        if ((reg = m_regMap->FindRegister("LW_PMGR_GPIO_CNTL2")))
        {
            reg_addr = reg->GetAddress();
            if ((field = reg->FindField("LW_PMGR_GPIO_CNTL2_GPIO_8_INPUT")))
            {
                reg_data = lwgpu->RegRd32(reg_addr);
                therm_shutdown = (reg_data & field->GetMask()) >>
                    field->GetStartBit();
            }
            else
            {
                ErrPrintf("ThermalShutdownLatch: Failed to find "
                    "LW_PMGR_GPIO_CNTL2_GPIO_8_INPUT field\n");
                return (1);
            }
        }
        else if ((reg = m_regMap->FindRegister("LW_PMGR_GPIO_8_OUTPUT_CNTL")))
        {
            reg_addr = reg->GetAddress();
            if ((field = reg->FindField("LW_PMGR_GPIO_8_OUTPUT_CNTL_IO_INPUT")))
            {
                reg_data = lwgpu->RegRd32(reg_addr);
                therm_shutdown = (reg_data & field->GetMask()) >>
                    field->GetStartBit();
            }
            else
            {
                ErrPrintf("ThermalShutdownLatch: Failed to find "
                    "LW_PMGR_GPIO_8_OUTPUT_CNTL_IO_INPUT field\n");
                return (1);
            }
        }
        else
        {
            ErrPrintf("ThermalShutdownLatch: Failed to find LW_PMGR_GPIO_CNTL2 "
                "or LW_PMGR_GPIO_8_OUTPUT_CNTL registers\n");
            return (1);
        }
        InfoPrintf("ThermalShutdownLatch: %s = 0x%x\n", field->GetName(),
            therm_shutdown);
    }

    // compare against expected value
    if (therm_shutdown != value)
    {
        ErrPrintf("ThermalShutdownLatch: Mismatch in Thermal Shutdown State "
            "(active low): detected %d, expected %d\n", therm_shutdown, value);
        errCnt++;
    }
    return (0);
}

int
ThermalShutdownLatch::clear_therm_shutdown_latch()
{
    // this routine is intended to clear the latch in OVERT
    // the latch is specified to be clear when PEX reset is asserted, which is
    // connected to HV_HIZ_N of the I/O pad
    // this is performed using EscapeWrite to initiate RTL forces to just the
    // I/O pad since the test cannot recover from resetting the entire chip
    UINT32 data;
    InfoPrintf("ThermalShutdownLatch: Asserting PEX_reset_ on Thermal "
        "Shutdown pad clear latch\n");
    Platform::EscapeWrite("force_thermal_shutdown_latch_reset", 0, 4, 1);
    Platform::EscapeRead("force_thermal_shutdown_latch_reset", 0, 4, &data);
    if (data != 1)
    {
        ErrPrintf("ThermalShutdownLatch: Unexpected failure in EscapeWrite of "
            "force_thermal_shutdown_latch_reset\n");
        errCnt++;
    }
    Platform::Delay(1);
    Platform::EscapeWrite("force_thermal_shutdown_latch_reset", 0, 4, 0);
    Platform::EscapeRead("force_thermal_shutdown_latch_reset", 0, 4, &data);
    if (data != 0)
    {
        ErrPrintf("ThermalShutdownLatch: Unexpected failure in EscapeWrite of "
            "force_thermal_shutdown_latch_reset\n");
        errCnt++;
    }
    return (0);
}

