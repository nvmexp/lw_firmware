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
// For style requirements, all lines of code follow the 80 column rule.
#include "mdiag/tests/stdtest.h"

#include "smartfan_sci.h"
#include "mdiag/lwgpu.h"
#include "mdiag/utils/lwgpu_channel.h"
#include "mdiag/tests/test_state_report.h"
#include "core/include/gpu.h"
#include "sim/IChip.h"
#include "lw_ref_plus_dev_gc6_island.h"
#include "regmap_helper.h"

// Register includes
#include "lw_ref_plus_dev_gc6_island.h"
#include "maxwell/gm107/dev_master.h"

extern const ParamDecl smartfan_sci_params[] =
{
    SIMPLE_PARAM("-over_temp_alert", "Test response to over temperature alert"),
    SIMPLE_PARAM("-over_temp_alert_force", "Test response to over temperature alert using the force register"),
    STRING_PARAM("-strap_sense_delay", "Delay tests by this time to let fan straps be sensed in LW_SCI"),
    PARAM_SUBDECL(lwgpu_single_params),
    LAST_PARAM
};

UINT32 SmartFanSCI::thermAlertForceTest(){
    // Therm setup
    // This is taken from standism/pwr/therm/lw_therm_tests.vtpp
    // lwgpu =  (LWGpuResource*)controller->FindFirstResource("gpu", -1);
    int period = 8000;
    GpuSubdevice *pSubdev = lwgpu->GetGpuSubdevice();
    int temp1 = pSubdev->RegRd32( LW_PMC_ENABLE);
    pSubdev->RegWr32( LW_PMC_ENABLE, temp1 | 0x0000102d ); //Enable PRIV ring, XBAR
    regmap_helper *h_reg = new regmap_helper(lwgpu, m_regMap);

    // Set no alert by setting themperature threshold to 25C
    //SCI strap is assumed to be set to 4 i.e both fans at 33%
    h_reg->drf_def("PGC6_SCI", "FAN_CFG0(0)", "SCI_OVERRIDE", "ENABLE");
    h_reg->drf_def("PGC6_SCI", "FAN_CFG0(0)", "PERIOD", "125KHZ");
    h_reg->drf_def("PGC6_SCI", "FAN_CFG1(0)", "HI", "33");
    h_reg->drf_num("PGC6_SCI", "FAN_TEMP_ALERT(0)", "HI", 144);
    h_reg->drf_def("PGC6_SCI", "FAN_TEMP_ALERT(0)", "OVERRIDE", "ENABLE");

    m_ErrCount += do_update(0);

    h_reg->drf_def("PGC6_SCI", "FAN_CFG0(1)", "SCI_OVERRIDE", "ENABLE");
    h_reg->drf_def("PGC6_SCI", "FAN_CFG0(1)", "PERIOD", "125KHZ");
    h_reg->drf_def("PGC6_SCI", "FAN_CFG1(1)", "HI", "33");
    h_reg->drf_num("PGC6_SCI", "FAN_TEMP_ALERT(1)", "HI", 144);
    h_reg->drf_def("PGC6_SCI", "FAN_TEMP_ALERT(1)", "OVERRIDE", "ENABLE");

    m_ErrCount += do_update(1);


    // Checks that there is no temp alert signal when force is not set
    trigger_smartfan_monitor(period, 0);
    trigger_smartfan_monitor(period, 1);
    m_ErrCount += check_smartfan_period_dutycycle(period, 33, 0);
    m_ErrCount += check_smartfan_period_dutycycle(period, 33, 1);

    // Set an alert by using the THERM_MAX_FAN_FORCE register
    // On Alert switch to 2/3 of clock period
    h_reg->drf_def("THERM", "MAXFAN" , "FORCE", "TRIGGER");

    // Check that duty cycle changes to 67%
    trigger_smartfan_monitor(period, 0);
    trigger_smartfan_monitor(period, 1);
    if(check_smartfan_period_dutycycle(period, 67, 0) && 
       check_smartfan_period_dutycycle(period, 67, 1) )
    {
        return(1);
    }
    // Set no alert by clearing the MAXFAN_FORCE register
    if(h_reg->drf_def("THERM", "MAXFAN", "STATUS", "CLEAR"))
    {
        if(h_reg->drf_def("CPWR_THERM", "MAXFAN", "STATUS_SMARTFAN_SCI", "CLEAR"))
        {
            InfoPrintf("SmartFanSCI:ERROR: Cannot find MAXFAN_STATUS_CLEAR in Manuals\n");
            return(1);
        }
    }

    trigger_smartfan_monitor(period, 0);
    trigger_smartfan_monitor(period, 1);
    if(check_smartfan_period_dutycycle(period, 33, 0) && 
       check_smartfan_period_dutycycle(period, 33, 1))
    {
        return(1);
    }

    return 0;
}

UINT32 SmartFanSCI::thermAlertTest(){
    // Therm setup
    // This is taken from standism/pwr/therm/lw_therm_tests.vtpp
    // lwgpu =  (LWGpuResource*)controller->FindFirstResource("gpu", -1);
    int period = 8000;
    GpuSubdevice *pSubdev = lwgpu->GetGpuSubdevice();
    int temp1 = pSubdev->RegRd32( LW_PMC_ENABLE);
    pSubdev->RegWr32( LW_PMC_ENABLE, temp1 | 0x0000102d ); //Enable PRIV ring, XBAR
    regmap_helper *h_reg = new regmap_helper(lwgpu, m_regMap);

    // Override Temperaure in Therm to 50C
    h_reg->drf_num("THERM", "SENSOR_1", "TS_STABLE_CNT", 0);
    h_reg->drf_num("THERM", "SENSOR_0", "TEMP_OVERRIDE", 50);
    h_reg->drf_def("THERM", "SENSOR_0", "TEMP_SELECT", "OVERRIDE");
    h_reg->drf_def("THERM", "CTRL_0", "ALERT_CTRL", "ONLY_HIGH");
    h_reg->drf_def("THERM", "CTRL_2", "ALERT_UNMASK", "ALERT0");
    h_reg->drf_def("THERM", "POWER_6", "THERMAL_FILTER_PERIOD", "NONE");
    h_reg->drf_def("THERM", "POWER_6", "THERMAL_FILTER_SCALE", "16US");

    // Platform::Delay(800);  // Let strap sense clocks elaps
    // Set no alert by setting themperature threshold to 25C
    h_reg->drf_def("PGC6_SCI", "FAN_CFG0", "SCI_OVERRIDE", "ENABLE");
    h_reg->drf_def("PGC6_SCI", "FAN_CFG0", "PERIOD", "125KHZ");
    h_reg->drf_def("PGC6_SCI", "FAN_CFG1", "HI", "33");
    h_reg->drf_num("PGC6_SCI", "FAN_TEMP_ALERT", "HI", 144);
    h_reg->drf_def("PGC6_SCI", "FAN_TEMP_ALERT", "OVERRIDE", "ENABLE");

    m_ErrCount += do_update(0);

    // Set no alert by setting themperature threshold to 75C
    h_reg->drf_num("THERM", "EVT_ALERT_0H", "TEMP_THRESHOLD", 75);
    trigger_smartfan_monitor(period,0);
    m_ErrCount += check_smartfan_period_dutycycle(period, 33,0);

    // Set an alert by setting themperature threshold to 25C
    // On Alert switch to 2/3 of clock period
    h_reg->drf_num("THERM", "EVT_ALERT_0H", "TEMP_THRESHOLD", 25);

    // Check that duty cycle changes to 67%
    trigger_smartfan_monitor(period,0);
    if(check_smartfan_period_dutycycle(period, 67,0))
    {
        return(1);
    }
    // Set no alert by setting themperature threshold to 25C
    h_reg->drf_num("THERM", "EVT_ALERT_0H", "TEMP_THRESHOLD", 75);
    trigger_smartfan_monitor(period,0);
    if(check_smartfan_period_dutycycle(period, 33,0))
    {
        return(1);
    }

    return 0;
}

SmartFanSCI::SmartFanSCI(ArgReader *params) :
    LWGpuSingleChannelTest(params)
{
    if (params->ParamPresent("-over_temp_alert")){
            InfoPrintf("SmartFanSCI: Over temperature alert test\n");
            over_temp_alert = 1;
    }
    else{
            over_temp_alert = 0;
    }
    if (params->ParamPresent("-over_temp_alert_force")){
            InfoPrintf("SmartFanSCI: Over temperature alert force test\n");
            over_temp_alert_force = 1;
    }
    else{
            over_temp_alert_force = 0;
    }
}

SmartFanSCI::~SmartFanSCI(void)
{
}

STD_TEST_FACTORY(smartfan_sci, SmartFanSCI)

INT32
SmartFanSCI::Setup(void)
{
    InfoPrintf("SmartFanSCI:: Entering setup\n");
    lwgpu = LWGpuResource::FindFirstResource();

    ch = lwgpu->CreateChannel();
    if(!ch)
    {
        ErrPrintf("SmartFanSCI: Setup failed to create channel\n");
        return (0);
    }
    InfoPrintf("SmartFanSCI:: channel created\n");

    m_regMap = lwgpu->GetRegisterMap();
    if(!m_regMap)
    {
        ErrPrintf("SmartFanSCI: smartfan can only be run on GPU's that support a "
                  "register map!\n");
        return (0);
    }

    InfoPrintf("SmartFanSCI:: regmap found\n");
    getStateReport()->init("SmartFanSCI");
    getStateReport()->enable();

    return 1;
}

void
SmartFanSCI::CleanUp(void)
{
    if(ch)
    {
        delete ch;
        ch = 0;
    }

    if (lwgpu)
    {
        DebugPrintf("SmartFanSCI::CleanUp(): Releasing GPU.\n");
        lwgpu = 0;
    }
    LWGpuSingleChannelTest::CleanUp();
}

void SmartFanSCI::Run(void)
{
    SetStatus(TEST_INCOMPLETE);
    InfoPrintf("Starting SmartFanSCI test\n");
    m_ErrCount = 0;
    if (over_temp_alert){
        if (thermAlertTest())
        {
            SetStatus(TEST_FAILED);
            getStateReport()->runFailed("Failed test\n");
            return;
        }
    }
    else if(over_temp_alert_force){
        if (thermAlertForceTest())
        {
            SetStatus(TEST_FAILED);
            getStateReport()->runFailed("Failed test\n");
            return;
        }
    }
    else if(smartfanTest())
    {
        SetStatus(TEST_FAILED);
        getStateReport()->runFailed("Failed test\n");
        return;
    }

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

// Note: board designs that support the GPU controlling the fan at power up
// must use GPIO[16] for this feature.  This is because this feature must
// operate even when PEX reset is still asserted, which means that SW or init
// from ROM had not had a chance to configure the GPIO.  Naturally, the GPIO
// must be hardcoded.  A ternary strap xtaloutbuff configure the fan
// start-up duty cycle of this feature.  This feature is expected to be
// implemented in the Fermi architecture starting with gf11x.
//
// Test Description
// During power up of a GPU that controls the fan PWM waveform, the period and
// duty cycle cannot be configured until after PEX reset is deasserted.  Since
// reset deassertion can be anywhere from 1 to 10 seconds, the GPU may be at
// risk of overheating if the reset is delayed towards the upper end of the max
// latency.  This can be solved by using a pull resistor to configure the fan
// to spin at 100% until SW assumes control of the fan PWM waveform through the
// GPIO.  Unfortunately, a fan spinning at 100% has the most accoustical noise.
// This does not portray the GPU as a finely tuned precision running IC.
//
// An improved solution is to add an astable multivibrator to the board that
// will drive the fan PWM waveform with a lower duty cycle, and therefore has
// less accoustical noise.  However, this solution adds about $0.10 to the board
// cost.
//
// The SmartFanSCI start-up duty cycle feature solves this problem by implementing
// a small circuit on the GPU that lives on a different reset domain than the
// PEX reset domain.  When the core vdd power reaches a stable operating voltage
// (as determined by the power_on_reset detection circuit, which may be on the
// GPU
// or external to the GPU), the SmartFanSCI circuit is taken out of reset, the fan
// PWM control straps are sensed, and GPIO[16] is driven with a PWM waveform
// according to the strapping configuration.  Once SW is up and running, SW may
// reconfigure the fan PWM waveform through priv registers.
//
// This test will confirm the period and duty cycle of the fan PWM waveform on
// GPIO[16] powers on according to the strap configuration.  A different period
// and duty cycle will be configured to confirm that SW can override the default
// strap configuration.  This test relies on the pwm_engine in sysTop.v
// testbench
// to monitor and report the period and duty cycle of the waveform.
UINT32
SmartFanSCI::smartfanTest()
{

    UINT32 fan_pwm_cntl;
    UINT32 num_smart_fans = 2;
    UINT32 period[2], duty_cycle[2];
    UINT32 period_clks, hi_clks;

    // if (check_smartfan_support())
    // {
    //     return (1);
    // }

    // read the strapping value
    EscapeRead("pwm_engine_0_fan_pwm_cntl", 0, 4, &fan_pwm_cntl);
    InfoPrintf("SmartFanSCI: Detected the board strapping values of "
        "fan_pwm_cntl = %d\n", fan_pwm_cntl);

    GpuSubdevice *pSubdev = lwgpu->GetGpuSubdevice();
    int temp1 = pSubdev->RegRd32( LW_PMC_ENABLE);
    pSubdev->RegWr32( LW_PMC_ENABLE, temp1 | 0x0000102d ); //Enable PRIV ring, XBAR
    // check against the value reported in priv register
    Platform::Delay(1001);  // Let strap sense clocks elaps
    if (check_smartfan_straps(fan_pwm_cntl))
    {
        m_ErrCount++;
    }

    // decode strapping value into period and duty cycle
    switch (fan_pwm_cntl)
    {
        case 0 :
            period[0] = 8000;  // With a 27ns clock, the default period is 8000
            period[1] = 8000;  // With a 27ns clock, the default period is 8000
            duty_cycle[0] =  0;
            duty_cycle[1] =  0;
            break;
        case 1 :
            period[0] = 8000;  // With a 27ns clock, the default period is 8000
            period[1] = 8000;  // With a 27ns clock, the default period is 8000
            duty_cycle[0] =  67;
            duty_cycle[1] =  0;
            break;
        case 2 :
            period[0] = 8000;  // With a 27ns clock, the default period is 8000
            period[1] = 8000;  // With a 27ns clock, the default period is 8000
            duty_cycle[0] =  67;
            duty_cycle[1] =  67;
             break;
         case 3 :
            period[0] = 8000;  // With a 27ns clock, the default period is 8000
            period[1] = 8000;  // With a 27ns clock, the default period is 8000
            duty_cycle[0] =  33;
            duty_cycle[1] =  0;
            break;
         case 4 :
            period[0] = 8000;  // With a 27ns clock, the default period is 8000
            period[1] = 8000;  // With a 27ns clock, the default period is 8000
            duty_cycle[0] =  33;
            duty_cycle[1] =  33;
            break;
         case 5 :
            period[0] = 8000;  // With a 27ns clock, the default period is 8000
            period[1] = 8000;  // With a 27ns clock, the default period is 8000
            duty_cycle[0] =  33;
            duty_cycle[1] =  66;
            break;
         case 6 :
            period[0] = 8000;  // With a 27ns clock, the default period is 8000
            period[1] = 8000;  // With a 27ns clock, the default period is 8000
            duty_cycle[0] =  66;
            duty_cycle[1] =  33;
            break;
        default :
            ErrPrintf("SmartFanSCI: Illegal value detected in fan_pwn_cntl.  "
                "Valid range is 0 thru 6, inclusive.\n");
            return (1);
            break;
    }
    // Wait 5 periods
    // DelayNs(period * 5);
    // check period and duty cycle from off chip PWM monitor
    for (unsigned int i = 0; i < num_smart_fans ; i++)
    {
        if (check_smartfan_period_dutycycle(period[i], duty_cycle[i], i))
        {
            return (1);
        }
        // check period and hi clocks from on chip PWM monitor
        if (get_boot_period_hi_clks(fan_pwm_cntl, &period_clks, &hi_clks, i))
        {
            return (1);
        }
        if (check_smartfan_actual_pwm_regs(period_clks, hi_clks, i))
        {
            return (1);
        }
    
        // Wait 5 periods
        // /DelayNs(period * 5);
        // reconfigure period and duty cycle and recheck
        // this is to confirm that SW can reconfigure the PWM waveform
    
       /// Set period between 12000ns and 8000 ns . 12000 > 8000 but doesn't increase test time too much
        period[i] = 15000 - (fan_pwm_cntl / 2 * 1000);
        duty_cycle[i] = 90 - (fan_pwm_cntl / 2 * 5);
        if (get_period_hi_clks(period[i], duty_cycle[i], &period_clks, &hi_clks))
        {
            return (1);
        }
        if (set_smartfan_period_hi(period_clks, hi_clks, i))
        {
            return (1);
        }
    
        // trigger monitor and recheck period and duty cycle from off chip monitor
        trigger_smartfan_monitor(period[i], i);
        if (check_smartfan_period_dutycycle(period[i], duty_cycle[i], i))
        {
            return (1);
        }
        // recheck period and hi clocks from on chip monitor
        if (check_smartfan_actual_pwm_regs(period_clks, hi_clks, i))
        {
            return (1);
        }
    }
        
    // done with test
    InfoPrintf("SmartFanSCI test completed with %d errors\n", m_ErrCount);
    return (0);  // calling routine will check m_ErrCount for CRC fail
}

// INT32 SmartFanSCI::check_smartfan_support()
// {
//     IRegisterClass *reg;
//     IRegisterField *field;
//     UINT32 period_factor;

//     // Identify whether the GPU supports the SmartFanSCI feature.  We will check
//     // this by testing whether the priv register/field for the feature exists.
//     // Note: SmartFanSCI consists of two features: fan sharing, and start-up duty
//     // cycle.  Each feature is controlled by different defined, but start-up
//     // support implies fan sharing is supported
//     InfoPrintf("SmartFanSCI: Checking whether GPU supports Smartfan feature...\n");
//     if (!(reg = m_regMap->FindRegister("LW_THERM_PWM0")))
//     {
//         ErrPrintf("SmartFanSCI: Cannot find LW_THERM_PWM0 register, Smartfan "
//             "sharing and start-up duty cycle features are not supported\n");
//         ErrPrintf("SmartFanSCI: Please recompile RTL with LW_CHIP_FAN_PWM_SHARING_"
//             "SUPPORT and LW_CHIP_FAN_PWM_START_UP_SUPPORT defined\n");
//         return (1);
//     }
//     // LW_THERM_PWM0_CHIP_RESET_DUTY_CYCLE field was added for gf11x chips, but
//     // was replaced by LW_THERM_PWM_CHIP_RESET register in gk10x chips.
//     // Therefore, we need to test for the existence of either of them to
//     // determine whether SmartFanSCI start-up duty cycle feature is supported.
//     // Note: LW_THERM_PWM_CHIP_RESET register may be deprecated for gk110 chips
//     // and later.  We need a substitute register to tell us if the SmartFanSCI
//     // start-up duty cycle feature is supported.  We will use
//     // LW_THERM_PWM_DEBUG_FAN_PWM_STRAP, but this register was not added until
//     // gk10x chips.  Therefore, we need to check both
//     // LW_THERM_PWM0_CHIP_RESET_DUTY_CYCLE or LW_THERM_PWM_DEBUG_FAN_PWM_STRAP.
//     if (!(field = reg->FindField("LW_THERM_PWM0_CHIP_RESET_DUTY_CYCLE")))
//     {
//         if ((reg = m_regMap->FindRegister("LW_THERM_PWM_DEBUG")))
//         {
//             if (!(field = reg->FindField("LW_THERM_PWM_DEBUG_FAN_PWM_STRAP")))
//             {
//                 ErrPrintf("SmartFanSCI: Cannot find LW_THERM_PWM0_CHIP_RESET_DUTY_"
//                     "CYCLE field or LW_THERM_PWM_DEBUG_FAN_PWM_STRAP field, "
//                     "SmartFanSCI start-up duty cycle feature is not supported\n");
//                 ErrPrintf("SmartFanSCI: Please recompile RTL with "
//                     "LW_CHIP_FAN_PWM_START_UP_SUPPORT defined\n");
//                 return (1);
//             }
//         }
//         else
//         {
//             ErrPrintf("SmartFanSCI: Cannot find LW_THERM_PWM0_CHIP_RESET_DUTY_"
//                 "CYCLE field or LW_THERM_PWM_DEBUG register, SmartFanSCI start-up "
//                 "duty cycle feature is not supported\n");
//             ErrPrintf("SmartFanSCI: Please recompile RTL with "
//                 "LW_CHIP_FAN_PWM_START_UP_SUPPORT defined\n");
//             return (1);
//         }
//     }
//     InfoPrintf("SmartFanSCI: Smartfan feature is supported\n");

//     if (get_scale_factor(&period_factor))
//     {
//         return (1);
//     }
//     switch (period_factor)
//     {
//         case 135 :
//             InfoPrintf("SmartFanSCI: Detected units of LW_THERM_PWM0_PERIOD to be "
//                 "74.074 ns (two 27 MHz XTAL clocks)\n");
//             break;
//         case 270 :
//             InfoPrintf("SmartFanSCI: Detected units of LW_THERM_PWM0_PERIOD to be "
//                 "37.037 ns (27 MHz XTAL clock)\n");
//             break;
//         default :
//             ErrPrintf("SmartFanSCI: Unknown value detected for scale_factor: %d.  "
//                 "Recognized values are: 135, 270.", period_factor);
//             return (1);
//             break;
//     }
//     return (0);
// }

// This routine checks the strap value stored in the priv register.
INT32
SmartFanSCI::check_smartfan_straps(UINT32 exp_strap)
{
    unique_ptr<IRegisterClass> reg;
    unique_ptr<IRegisterField> field;
    UINT32 reg_addr, reg_data;
    UINT32 act_strap, mismatch;

    // Not all chips have the strap values available in priv register.
    // If the priv field does not exist, then exit silently.  This is not
    // an error.  Otherwise, verify that the priv field reflects the same
    // strap value as expected by the test.
    mismatch = 0;
    if ((reg = m_regMap->FindRegister("LW_PGC6_SCI_FAN_DEBUG")))
    {
        if ((field = reg->FindField("LW_PGC6_SCI_FAN_DEBUG_PWM_STRAP")))
        {
            // field exists, now read the register
            reg_addr = reg->GetAddress();
            reg_data = lwgpu->RegRd32(reg_addr);
            InfoPrintf("SmartFanSCI: Reading %s = 0x%08x\n", reg->GetName(),
                reg_data);

            // extract the strap value and check against expected
            act_strap = (reg_data & field->GetMask()) >> field->GetStartBit();
            if (act_strap == exp_strap)
            {
                InfoPrintf("SmartFanSCI: Found matching "
                    "LW_PGC6_SCI_FAN_DEBUG_PWM_STRAP = %d\n", act_strap);
            }
            else
            {
                ErrPrintf("SmartFanSCI: Mismatch in "
                    "LW_PGC6_SCI_FAN_DEBUG_PWM_STRAP: expected = %d, "
                    "actual = %d\n", exp_strap, act_strap);
                mismatch++;
            }
        }
    }
    return (mismatch);
}

// This routine returns the scale factor of the time units of the priv register.
// Starting with kepler chips, the units of LW_THERM_PWM0 and LW_THERM_PWM1
// were changed from 2 xtal_in_slow clocks to 1 xtal_in_slow clocks (see
// bug 709094).  We need to check the width of LW_THERM_PWM0_PERIOD to determine
// it units (width is 12-bits if units are 2 xtal_in_slow clocks, width is
// 13-bits if units are 1 xtal_in_slow clocks).  The scale factor corresponds
// indirectly to a 13.5 MHz unit or 27 MHz unit.
INT32
SmartFanSCI::get_scale_factor(UINT32 *factor)
{
    unique_ptr<IRegisterClass> reg;
    unique_ptr<IRegisterField> field;
    UINT32 field_width;

    // Get the width of LW_PGC6_SCI_FAN_CFG0_PERIOD field.  This is subject to change
    // from chip to chip since it is very dependent on which clock SmartFanSCI is
    // implemented on.
    if ((reg = m_regMap->FindRegister("LW_PGC6_SCI_FAN_CFG0")))
    {
        if ((field = reg->FindField("LW_PGC6_SCI_FAN_CFG0_PERIOD")))
        {
            // a 12-bit field implies 13.5 MHz units
            // a 13-bit field implies 27.0 MHz units
            // the default going forward is 27.0 MHz units, so we will
            // assume 27.0 MHz unless the width is 12
            field_width = field->GetEndBit() - field->GetStartBit() + 1;
            if (field_width == 12)
            {
                *factor = 135;
            }
            else
            {
                *factor = 270;
            }
            return (0);
        }
        else
        {
            ErrPrintf("SmartFanSCI: Failed to find LW_PGC6_SCI_FAN_CFG0_PERIOD field\n");
            return (1);
        }
    }
    else
    {
        ErrPrintf("SmartFanSCI: Failed to find LW_PGC6_SCI_FAN_CFG0 register\n");
        return (1);
    }
}

// This routine returns the priv field values for period and hi that are
// loaded by the straps.
INT32
SmartFanSCI::get_boot_period_hi_clks(UINT32 strap, UINT32 *period_clks,
    UINT32 *hi_clks, UINT32 fan_index)
{
    UINT32 period_factor;

    // Decode strapping value into period and hi clocks.
    // These values assume the units are in 2 xtal_in_slow clocks.
    int period_clk_hw_reset_val = LW_PGC6_SCI_FAN_CFG0_PERIOD_125KHZ_ADJ / 2;
    switch (strap)
    {
        case 0 :
            *period_clks = period_clk_hw_reset_val;
            *hi_clks = 0;
            break;
        case 1 :
            if(fan_index == 0) 
            {
                *period_clks = period_clk_hw_reset_val;
                *hi_clks = (*period_clks) * 2 / 3;
            }
            else
            {
                *period_clks = period_clk_hw_reset_val;
                *hi_clks = 0;
            }
            break;
        case 2 :
            //Strap value is 66% for both fans
                *period_clks = period_clk_hw_reset_val;
                *hi_clks = (*period_clks) * 2 / 3;
            break;
        case 3 :
            if(fan_index == 0) 
            {
                *period_clks = period_clk_hw_reset_val;
                *hi_clks = (*period_clks) * 1 / 3;
            }
            else
            {
                *period_clks = period_clk_hw_reset_val;
                *hi_clks = 0;
            }
            break;
        case 4 :
            //Strap value is 33% for both fans
                *period_clks = period_clk_hw_reset_val;
                *hi_clks = (*period_clks) * 1 / 3;
            break;
         case 5 :
            if(fan_index == 0) 
            {
                *period_clks = period_clk_hw_reset_val;
                *hi_clks = (*period_clks) * 1 / 3;
            }
            else
            {
                *period_clks = period_clk_hw_reset_val;
                *hi_clks = (*period_clks) * 2 / 3;
            }
            break;
          case 6 :
            if(fan_index == 0) 
            {
                *period_clks = period_clk_hw_reset_val;
                *hi_clks = (*period_clks) * 2 / 3;
            }
            else
            {
                *period_clks = period_clk_hw_reset_val;
                *hi_clks = (*period_clks) * 1 / 3;
            }
            break;
        default :
            ErrPrintf("SmartFanSCI: Unsupported strap value detected: %d.  "
                "Cannot determine boot values for period and hi.\n", strap);
            return (1);
            break;
    }

    if (get_scale_factor(&period_factor))
    {
        return (1);
    }
    // The values for period_clks and hi_clks selected above assume that
    // the units are in 2 XTAL clocks.  If the units are actually in XTAL
    // clocks, we need to multiply by 2.
    if (period_factor == 270)
    {
        *period_clks *= 2;
        *hi_clks     *= 2;
    }
    return (0);
}

// This routine returns the priv field values for period and hi that corresponds
// to the period and duty cycle provided.
INT32
SmartFanSCI::get_period_hi_clks(UINT32 period_ns, UINT32 duty_cycle_pct,
    UINT32 *period_clks, UINT32 *hi_clks)
{
    UINT32 period_factor;

    if (get_scale_factor(&period_factor))
    {
        return (1);
    }

    // Colwert period and duty_cycle into units of clocks for programming the
    // register.
    *period_clks = (period_ns * period_factor) / 10000;
    *hi_clks = *period_clks * duty_cycle_pct / 100;

    return (0);
}

// This routine starts the pwm_engine in the fullchip testbench to monitor the
// PWM waveform on GPIO[16].  When the monitor is done, the period and duty
// cycle are checked against the expected period and duty cycle.  This is the
// interpretation of the results returned by the pwm_engine monitor (period in
// ns, duty_cycle in %):
//    mon_period   mon_duty_cycle   interpretation
//    ----------   --------------   --------------
//             0        dont_care   PWM waveform is unstable
//            >0         0 or 100   PWM waveform is static
//            >0     0 < dc < 100   PWM waveform is toggling and stable
//
void SmartFanSCI::DelayNs(UINT32 Lwclks){
  // Lwrrently this function is in terms of .2nS / lwclk. This is incorrectly done
  // in RTL. This should be a close approx of 1 nanosecond.
  EscapeWrite("CLOCK_WAIT",IChip::ECLOCK_LWCLK,0,Lwclks * 5);
}

void
SmartFanSCI::trigger_smartfan_monitor(UINT32 period, UINT32 fan_index)
{
    UINT32 mon_done, mon_timeout_us;
    char tempfunc[40];

    // reconfigure the timeout for determining a static fan PWM waveform
    // this should be slightly greater than the expected period
    // colwert from nanoseconds to microseconds, then add 10%
    mon_timeout_us = (period / 1000);
    mon_timeout_us += (mon_timeout_us / 10);
    // make sure we do not truncate/round to 0
    if ((period != 0) && (mon_timeout_us == 0))
    {
        mon_timeout_us = 1;
    }
    sprintf(tempfunc,"pwm_engine_%d_mon_timeout_us", fan_index);
    EscapeWrite(tempfunc, 0, 4, mon_timeout_us);

    // start the monitor
    mon_done = 0;
    InfoPrintf("SmartFanSCI: Starting pwm_engine monitor on fan PWM waveform...\n");
    sprintf(tempfunc,"pwm_engine_%d_mon_done", fan_index);
    EscapeWrite(tempfunc, 0, 4, mon_done);
}

INT32
SmartFanSCI::check_smartfan_period_dutycycle(UINT32 exp_period, UINT32
    exp_duty_cycle, UINT32 fan_index)
{
    UINT32 mon_done, mon_period, mon_duty_cycle;
    UINT32 limit, difference;
    bool bad_period, bad_duty_cycle;
    char tempfunc[40];


    // poll until done
    mon_done = 0;
    while (mon_done == 0)
    {
        Platform::Delay(1);
        sprintf(tempfunc,"pwm_engine_%d_mon_done", fan_index);
        EscapeRead(tempfunc, 0, 4, &mon_done);
    }

    // read period and duty_cycle from pwm_engine monitor
    sprintf(tempfunc,"pwm_engine_%d_mon_period", fan_index);
    EscapeRead(tempfunc,0, 4, &mon_period);
    sprintf(tempfunc,"pwm_engine_%d_mon_duty_cycle", fan_index);
    EscapeRead(tempfunc,0, 4, &mon_duty_cycle);
    InfoPrintf("SmartFanSCI: pwm_engine monitor detected PWM waveform "
        "period = %0d ns, duty_cycle = %0d%%\n on fan index %d", mon_period, mon_duty_cycle, fan_index);

    // check special case of unstable waveform
    if (exp_period == 0)
    {
        if (mon_period != 0)
        {
            ErrPrintf("SmartFanSCI: Expected PWM waveform to be unstable, but "
                "detected stable period = %0d ns on fan_index = %d\n", mon_period, fan_index);
            m_ErrCount++;
        }
        else
        {
            InfoPrintf("SmartFanSCI: Detected unstable period, as expected\n");
        }
    }
    // check special case of static waveform
    else if ((exp_duty_cycle == 0) || (exp_duty_cycle == 100))
    {
        if (exp_duty_cycle != mon_duty_cycle)
        {
            ErrPrintf("SmartFanSCI: Expected PWM waveform duty_cycle to be %0d%%, "
                "but detected duty_cycle = %0d%% on fan index = %d\n", exp_duty_cycle,
                mon_duty_cycle, fan_index);
            m_ErrCount++;
        }
        else
        {
            InfoPrintf("SmartFanSCI: Detected PWM waveform duty_cycle at %0d%%, "
                "as expected\n", mon_duty_cycle);
        }
    }
    // check normal case of toggling and stable waveform
    else
    {
        if (mon_period == 0)
        {
            ErrPrintf("SmartFanSCI: Expected PWM waveform period to be %0d ns, "
                "but detected unstable PWM waveform on fan index %d\n", exp_period,
                fan_index);
            m_ErrCount++;
        }
        else
        {
            // allow 1% tolerance in period
            limit = exp_period / 100;
            if (exp_period < mon_period)
            {
                difference = mon_period - exp_period;
            }
            else
            {
                difference = exp_period - mon_period;
            }
            bad_period = (difference > limit);
            // allow 1% tolerance in duty_cycle
            limit = 1;
            if (exp_duty_cycle < mon_duty_cycle)
            {
                difference = mon_duty_cycle - exp_duty_cycle;
            }
            else
            {
                difference = exp_duty_cycle - mon_duty_cycle;
            }
            bad_duty_cycle = (difference > limit);

            // check results
            if (bad_period)
            {
                ErrPrintf("SmartFanSCI: Mismatch in PWM waveform period: "
                    "expected %0d ns, actual %0d ns on fan index %d\n", 
                    exp_period, mon_period, fan_index);
                m_ErrCount++;
            }
            else
            {
                InfoPrintf("SmartFanSCI: Found matching PWM waveform period: "
                    "expected %0d ns, actual %0d ns\n", exp_period, mon_period);
            }
            if (bad_duty_cycle)
            {
                ErrPrintf("SmartFanSCI: Mismatch in PWM waveform duty_cycle: "
                    "expected %0d%%, actual %0d%% on fan index %d\n", exp_duty_cycle,
                    mon_duty_cycle, fan_index);
                m_ErrCount++;
            }
            else
            {
                InfoPrintf("SmartFanSCI: Found matching PWM waveform duty_cycle: "
                    "expected %0d%%, actual %0d%%\n", exp_duty_cycle,
                    mon_duty_cycle);
            }
        }
    }

    return (0);
}

// This routine sets the fan PWM waveform period and hi time.  After configuring
// the PWM waveform, the new values are triggered.  Lastly, the status is polled
// to confirm when the new values have taken effect.
INT32
SmartFanSCI::set_smartfan_period_hi(UINT32 period_clks, UINT32 hi_clks, UINT32 fan_index)
{
    unique_ptr<IRegisterClass> reg;
    unique_ptr<IRegisterField> field;
    UINT32 reg_addr, reg_data;
    UINT32 max_period, max_attempts=0;

    InfoPrintf("SmartFanSCI: Reprogramming PWM waveform with PERIOD = %0d, "
        "HI = %0d\n for fan index %d", period_clks, hi_clks, fan_index);

    // program LW_PGC6_SCI_FAN_CFG0 with a read/modify/write
    if (!(reg = m_regMap->FindRegister("LW_PGC6_SCI_FAN_CFG0")))
    {
        ErrPrintf("SmartFanSCI: Failed to find LW_PGC6_SCI_FAN_CFG0 register\n");
        return (1);
    }
    reg_addr = reg->GetAddress(fan_index);
    reg_data = lwgpu->RegRd32(reg_addr);
    InfoPrintf("SmartFanSCI: Reading %s = 0x%08x\n", reg->GetName(), reg_data);
    // program the PERIOD value
    if (!(field = reg->FindField("LW_PGC6_SCI_FAN_CFG0_PERIOD")))
    {
        ErrPrintf("SmartFanSCI: Failed to find LW_PGC6_SCI_FAN_CFG0_PERIOD field\n");
        return (1);
    }
    reg_data = (reg_data & ~field->GetMask()) | ((period_clks <<
        field->GetStartBit()) & field->GetMask());
    reg_data |= LW_PGC6_SCI_FAN_CFG0_SCI_OVERRIDE_ENABLE_ADJ;
    lwgpu->RegWr32(reg_addr, reg_data);
    InfoPrintf("SmartFanSCI: Writing %s = 0x%x\n", reg->GetName(), reg_data);
    // use the width of the PERIOD to determine max period
    // the max period will put an upper limit on the number of polling attempts
    // for the PWM update
    max_period = 75 << (field->GetEndBit() - field->GetStartBit() + 1);  // in
    // nanoseconds
    max_attempts = max_period / 1000;  // polling is 1 microseconds between
    // attempts
    // do not round/truncate to 0
    if (max_attempts == 0)
    {
        max_attempts = 1;
    }
    //InfoPrintf("SmartFanSCI: max_period = %0d ns\n", max_period);
    //InfoPrintf("SmartFanSCI: max_attempts = %0d\n", max_attempts);

    // program LW_PGC6_SCI_FAN_CFG1 with a read/modify/write
    if (!(reg = m_regMap->FindRegister("LW_PGC6_SCI_FAN_CFG1")))
    {
        ErrPrintf("SmartFanSCI: Failed to find LW_PGC6_SCI_FAN_CFG1 register\n");
        return (1);
    }
    reg_addr = reg->GetAddress(fan_index);
    reg_data = lwgpu->RegRd32(reg_addr);
    InfoPrintf("SmartFanSCI: Reading %s = 0x%08x\n", reg->GetName(), reg_data);
    // program the HI value
    if (!(field = reg->FindField("LW_PGC6_SCI_FAN_CFG1_HI")))
    {
        ErrPrintf("SmartFanSCI: Failed to find LW_PGC6_SCI_FAN_CFG1_HI field\n");
        return (1);
    }
    reg_data = (reg_data & ~field->GetMask()) | ((hi_clks <<
        field->GetStartBit()) & field->GetMask());
    lwgpu->RegWr32(reg_addr, reg_data);
    return do_update(fan_index);
}

UINT32 SmartFanSCI::do_update(UINT32 fan_index){
    // program the SETTING_NEW value to trigger the update
    unique_ptr<IRegisterClass> reg;
    unique_ptr<IRegisterField> field;
    unique_ptr<IRegisterValue> value;
    UINT32 reg_addr, reg_data;
    UINT32 setting_new;
    UINT32 max_attempts = 600, polling_attempts;

    reg = m_regMap->FindRegister("LW_PGC6_SCI_FAN_CFG1");
    reg_addr = reg->GetAddress(fan_index);
    reg_data = lwgpu->RegRd32(reg_addr);

    if (!(field = reg->FindField("LW_PGC6_SCI_FAN_CFG1_UPDATE")))
    {
        ErrPrintf("SmartFanSCI: Failed to find LW_PGC6_SCI_FAN_CFG1_UPDATE field\n");
        return (1);
    }
    // first, check that there is no update lwrrently pending
    setting_new = (reg_data & field->GetMask()) >> field->GetStartBit();
    if (!(value = field->FindValue("LW_PGC6_SCI_FAN_CFG1_UPDATE_PENDING")))
    {
        ErrPrintf("SmartFanSCI: Failed to find LW_PGC6_SCI_FAN_CFG1_UPDATE_PENDING "
            "value\n");
        return (1);
    }
    if (setting_new == value->GetValue())
    {
        ErrPrintf("SmartFanSCI: Unexpected update in progress.  Aborting test.\n");
        return (1);
    }
    // second, set the update trigger
    if ((value = field->FindValue("LW_PGC6_SCI_FAN_CFG1_UPDATE_TRIGGER")))
    {
        setting_new = value->GetValue();
        reg_data = (reg_data & ~field->GetMask()) | (setting_new <<
            field->GetStartBit());
        lwgpu->RegWr32(reg_addr, reg_data);
        InfoPrintf("SmartFanSCI: Writing %s = 0x%x\n", reg->GetName(), reg_data);
    }
    else
    {
        ErrPrintf("SmartFanSCI: Failed to find TRIGGER in "
            "LW_PGC6_SCI_FAN_CFG1_UPDATE or LW_THERM_PWM1_NEW_REQUEST fields\n");
        return (1);
    }

    // third, poll the register until the update is done
    if (!(value = field->FindValue("LW_PGC6_SCI_FAN_CFG1_UPDATE_DONE")))
    {
        ErrPrintf("SmartFanSCI: Failed to find LW_PGC6_SCI_FAN_CFG1_UPDATE_DONE "
            "value\n");
        return (1);
    }
    polling_attempts = 1;
    reg_data = lwgpu->RegRd32(reg_addr);
    setting_new = (reg_data & field->GetMask()) >> field->GetStartBit();
    while ((setting_new != value->GetValue()) && (polling_attempts <
        max_attempts))
    {
        if ((polling_attempts % 10) == 1)
        {
            InfoPrintf("SmartFanSCI: Polling attempt #%0d: %s = 0x%x\n",
                polling_attempts, field->GetName(), setting_new);
        }
        Platform::Delay(1);  // delay 1 microsecond
        polling_attempts = polling_attempts + 1;
        reg_data = lwgpu->RegRd32(reg_addr);
        setting_new = (reg_data & field->GetMask()) >> field->GetStartBit();
    }
    InfoPrintf("SmartFanSCI: Polling attempt #%0d: %s = 0x%x\n", polling_attempts,
        field->GetName(), setting_new);
    if (setting_new != value->GetValue())
    {
        ErrPrintf("SmartFanSCI: Timeout waiting for LW_PGC6_SCI_FAN_CFG1_UPDATE_"
            "DONE after %0d attempts\n", max_attempts);
        return (1);
    }

    InfoPrintf("SmartFanSCI: Done with PWM waveform update\n");
    return (0);
}

// This routine checks the PWM actual HI/PERIOD stored in the priv registes.
INT32
SmartFanSCI::check_smartfan_actual_pwm_regs(UINT32 period_clks, UINT32 hi_clks, UINT32 fan_index)
{
    unique_ptr<IRegisterClass> reg;
    unique_ptr<IRegisterField> field;
    UINT32 reg_addr, reg_data;
    UINT32 act_hi_clks, act_period_clks;

    // Not all chips have the actual PWM waveform values available in priv
    // registers.  If the priv registers do not exist, then exit silently.
    // This is not an error.  Otherwise, verify that the priv fields reflect
    // the same HI/PERIOD values as what we wrote in the PWM config registers.
    if ((reg = m_regMap->FindRegister("LW_PGC6_SCI_FAN_ACT0")))
    {
        reg_addr = reg->GetAddress(fan_index);
        reg_data = lwgpu->RegRd32(reg_addr);
        InfoPrintf("SmartFanSCI: Reading %s = 0x%08x\n", reg->GetName(), reg_data);
        if ((field = reg->FindField("LW_PGC6_SCI_FAN_ACT0_PERIOD")))
        {
            act_period_clks = (reg_data & field->GetMask()) >>
                field->GetStartBit();
        }
        else
        {
            ErrPrintf("SmartFanSCI: Failed to find LW_PGC6_SCI_FAN_ACT0"
                "field\n");
            return (1);
        }
    }
    else
    {
        // The register does not exist, exit siliently, this is not an error
        return (0);
    }
    // now read the second actual PWM register
    if ((reg = m_regMap->FindRegister("LW_PGC6_SCI_FAN_ACT1")))
    {
        reg_addr = reg->GetAddress(fan_index);
        reg_data = lwgpu->RegRd32(reg_addr);
        InfoPrintf("SmartFanSCI: Reading %s = 0x%08x\n", reg->GetName(), reg_data);
        if ((field = reg->FindField("LW_PGC6_SCI_FAN_ACT1_HI")))
        {
            act_hi_clks = (reg_data & field->GetMask()) >>
                field->GetStartBit();
        }
        else
        {
            ErrPrintf("SmartFanSCI: Failed to find LW_PGC6_SCI_FAN_ACT1 "
                "field\n");
            return (1);
        }
    }
    else
    {
        // It is an error if the first register exists but the second one
        // does not exist.
        ErrPrintf("SmartFanSCI: Found LW_PGC6_SCI_FAN_ACT0 register, "
            "but failed to find LW_PGC6_SCI_FAN_ACT1 register\n");
        return (1);
    }

    // Take care of special cases where we cannot predict what will be in the
    // actual PWM registers:
    // 1) unstable waveform, and 2) static waveform
    if (period_clks == 0)
    {
        // unstable waveform
        InfoPrintf("SmartFanSCI: Expecting unstable waveform.  Skipping check of "
            "LW_PGC6_SCI_FAN_ACT0 and LW_PGC6_SCI_FAN_ACT1\n");
        return (0);
    }
    else if ((hi_clks == 0) || (hi_clks == period_clks))
    {
        // static waveform
        // We cannot guarantee that we ran long enough for the act PWM
        // registers to detect a static waveform (overflow) and update
        // the registers.
        InfoPrintf("SmartFanSCI: Expecting static waveform.  Skipping check of "
            "LW_PGC6_SCI_FAN_ACT0 and LW_PGC6_SCI_FAN_ACT1\n");
    }
    else
    {
        // Compare expected and actual
        if (period_clks == act_period_clks)
        {
            InfoPrintf("SmartFanSCI: Found matching "
                "LW_PGC6_SCI_FAN_ACT0_PERIOD = %d\n", act_period_clks);
        }
        else
        {
            ErrPrintf("SmartFanSCI: Mismatch in LW_PGC6_SCI_FAN_ACT0_PERIOD: "
                "expected = %d, actual = %d\n for fan index = %d", period_clks, 
                act_period_clks, fan_index);
            m_ErrCount++;
        }
        if (hi_clks == act_hi_clks)
        {
            InfoPrintf("SmartFanSCI: Found matching "
                "LW_PGC6_SCI_FAN_ACT1_HI = %d\n", act_hi_clks);
        }
        else
        {
            ErrPrintf("SmartFanSCI: Mismatch in LW_PGC6_SCI_ACT1_HI: "
                "expected = %d, actual = %d for fan index = %d\n", hi_clks, 
                act_hi_clks, fan_index);
            m_ErrCount++;
        }
    }
    return (0);
}

void SmartFanSCI::EscapeWrite(const char *Path, UINT32 Index, UINT32 Size, UINT32 Value){
    if (Platform::GetSimulationMode() == Platform::RTL){
        DebugPrintf("EscapeWrite Path %s %d\n", Path, Value);
        Platform::EscapeWrite(Path, Index, Size, Value);
    }
    else{
        DebugPrintf("Unsupported Platform for EscapeWrite \n");
    }
}

int SmartFanSCI::EscapeRead(const char *Path, UINT32 Index, UINT32 Size, UINT32 *Value){
    if (Platform::GetSimulationMode() == Platform::RTL){
        int rv = Platform::EscapeRead(Path, Index, Size, Value);
        DebugPrintf("EscapeRead Path %s Index %d read %d\n", Path, Index, *Value);
        return rv;
    }
    else{
        DebugPrintf("Unsupported Platform for EscapeWrite \n");
        return 0;
    }
}
