/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2006-2014, 2020-2021 by LWPU Corporation.  All rights reserved.  
 * All information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */
// To facilitate faster compiles via pre-compiled headers, keep "stdtest.h" first
#include "mdiag/tests/stdtest.h"

/* Test internal thermal sensor and related GPIOs (8,9,12).

Basic Tests - using gpio[8,9,12] (from gk110, gpio[8] was replaced by a dedicate overt pin)
  checkInternalSensorInterrupt - checks that temperature thresholds produce correct interrupts
  checkGpioConn     - checks that connections to GPIO / overt pin, input and output, are correct
  checkSensorClk - check that tsense2tsensor_clk is about 1Mhz always
  checkFuse - check that connection from fuse to LW_THERM is ok and that they are properly used to callwlate temperature.

checkSlowdownFactor - checks that the hw_failsafe and sw_therm slowdown factor are correctly used, check pwm is also used correctly.
checkFilter - checks that the temperature_overlimit is set after the proper delay

setGpioAlt(int bit,int en) - This procedure enables/disables the alternate function of gpio[bit] pin so it could be used
setGpio(int bit, int en, int value) - This procedure enables/disables the gpio[bit] pin and sets to the value input
resetRegister - disable interrupts and clears the THERMAL_TRIGGER interrupt

checkclkcount - check gpcclk count.
checkExpectFSslowdown - checks whether HW_FAILSAFE_SLOWDOWN_ON is triggered as expected

*/
#include "thermalctrl_maxwell.h"
#include "mdiag/lwgpu.h"
#include "mdiag/utils/lwgpu_channel.h"
#include "mdiag/utils/crc.h"
#include "mdiag/tests/test_state_report.h"

#include "lwmisc.h"
#include "maxwell_rtl_clocks.h"

#ifdef _MIN
#  undef _MIN
#endif
#ifdef _MAX
#  undef _MAX
#endif

extern const ParamDecl thermalCtrlGpu_maxwell_params[] = {
    SIMPLE_PARAM("-check_therm2host_intr",    "Test connection of therm interrupt to host"),
    SIMPLE_PARAM("-check_gpio_connection",    "Check thermal_alert/power_alert input and pwr2pmgr_temp_alert output"),
    SIMPLE_PARAM("-check_fuse",    "Check fuse connection: opt_int_ts_a, opt_int_ts_b"),
    SIMPLE_PARAM("-slowdown", "Testing slowdown functionalities"),
    SIMPLE_PARAM("-idle_slowdown",     "Testing idle_slowdown"),
    SIMPLE_PARAM("-therm_sensor_connectivity", "Testing therm sensor connectivity"),
    SIMPLE_PARAM("-dvco_slowdown", "Request DVCO slowdown only"),
    PARAM_SUBDECL(lwgpu_single_params),
    LAST_PARAM
};

static int check_therm2host_intr_run = 0;
static int check_gpio_connection_run = 0;
static int check_fuse_run = 0;
static int slowdown_run = 0;
static int idle_slowdown_run = 0;
static int therm_sensor_connectivity_run = 0;
static int dvco_slowdown_run = 0;

ThermalCtrl_maxwell::ThermalCtrl_maxwell(ArgReader *params) :
    LWGpuSingleChannelTest(params)
{
    if (params->ParamPresent("-check_therm2host_intr"))
        check_therm2host_intr_run = 1;
    if (params->ParamPresent("-check_gpio_connection"))
        check_gpio_connection_run = 1;
    if (params->ParamPresent("-check_fuse"))
        check_fuse_run = 1;
    if (params->ParamPresent("-slowdown"))
        slowdown_run = 1;
    if (params->ParamPresent("-idle_slowdown"))
        idle_slowdown_run = 1;
    if (params->ParamPresent("-therm_sensor_connectivity"))
        therm_sensor_connectivity_run = 1;
    if (params->ParamPresent("-dvco_slowdown"))
        dvco_slowdown_run = 1;
}

ThermalCtrl_maxwell::~ThermalCtrl_maxwell(void)
{
}

Test *ThermalCtrl_maxwell::Factory(ArgDatabase *args)
{
    ArgReader *params = new ArgReader(thermalCtrlGpu_maxwell_params);

    if( !params->ParseArgs(args) ) {
        ErrPrintf("ThermalCtrl_maxwell: error parsing args!\n");
        return(0);
    }

    return(new ThermalCtrl_maxwell(params));
}

int ThermalCtrl_maxwell::Setup(void)
{
    lwgpu = LWGpuResource::FindFirstResource();
    m_arch = lwgpu->GetArchitecture();
    InfoPrintf("m_arch = 0x%x", m_arch);
    macros.MacroInit(m_arch);

    SetMaxwellClkGpuResource(lwgpu);

    ch = lwgpu->CreateChannel();
    if(!ch)
    {
        ErrPrintf("ThermalCtrl_maxwell::Setup failed to create channel\n");
        return 0;
    }

    getStateReport()->init("thermalCtrlGpu_maxwell");
    getStateReport()->enable();

    // The temperature events moved to different registers starting in gk110.
    // We will test the existence of LW_THERM_EVENT_TRIGGER to determine whether
    // we access the alerts in new registers or the old registers.
    return 1;
}

void ThermalCtrl_maxwell::CleanUp(void)
{
    if(ch)
    {
        delete ch;
        ch = 0;
    }

    if (lwgpu)
    {
        DebugPrintf("Gpio::CleanUp(): Releasing GPU.\n");
        lwgpu = 0;
    }

    LWGpuSingleChannelTest::CleanUp();
}

// main routine
void ThermalCtrl_maxwell::Run(void)
{

    //  InfoPrintf Flag to determine whether the test fail because of elw issue or test issue.
    InfoPrintf("ThermalCtrl_maxwell Test Starting \n");

    InfoPrintf("Thermal support/base registers used for architecture G/GT-%x\n",m_arch);

    SetStatus(TEST_INCOMPLETE);

    // Initialize vcodiv_jtag
    Platform::EscapeWrite("drv_gpcclk_vcodiv_jtag", 0, 6, 4);
    Platform::EscapeWrite("drv_sysclk_vcodiv_jtag", 0, 6, 8);
    Platform::EscapeWrite("drv_ltcclk_vcodiv_jtag", 0, 6, 4);

    // Initialize temperature
    Platform::EscapeWrite("tb_thermal_sensor_value", 0, 14, temp2code(0));
    Platform::EscapeWrite("tb_thermal_sensor_override", 0, 1, 1);

    // Initialize FACTOR
    // InfoPrintf("configure PWM and FACTOR\n");

        // send interrupts back to host (HW init value changed in gk110)
        lwgpu->SetRegFldNum("LW_THERM_EVENT_INTR_ROUTE", "",  0);

    // make ts_output_stable become valid quickly
    lwgpu->SetRegFldDef("LW_THERM_SENSOR_6", "_POWER", "_ON");
    lwgpu->SetRegFldNum("LW_THERM_SENSOR_1", "", 0);

    if (lwgpu->SetRegFldNum("LW_THERM_POWER_6", "", 0)) { ErrPrintf("could not update LW_THERM_POWER_6"); }

    //Initial threshold to 255 to make sure by default none thermal event is triggered.
    if( macros.lw_chip_thermal_sensor ) {
        lwgpu->SetRegFldNum("LW_THERM_EVT_THERMAL_0", "_TEMP_THRESHOLD",  255<<5);
        lwgpu->SetRegFldNum("LW_THERM_EVT_THERMAL_1", "_TEMP_THRESHOLD",  255<<5);
        lwgpu->SetRegFldNum("LW_THERM_EVT_THERMAL_2", "_TEMP_THRESHOLD",  255<<5);
        lwgpu->SetRegFldNum("LW_THERM_EVT_THERMAL_3", "_TEMP_THRESHOLD",  255<<5);
        lwgpu->SetRegFldNum("LW_THERM_EVT_THERMAL_4", "_TEMP_THRESHOLD",  255<<5);
        lwgpu->SetRegFldNum("LW_THERM_EVT_THERMAL_5", "_TEMP_THRESHOLD",  255<<5);
        lwgpu->SetRegFldNum("LW_THERM_EVT_THERMAL_6", "_TEMP_THRESHOLD",  255<<5);
        lwgpu->SetRegFldNum("LW_THERM_EVT_THERMAL_7", "_TEMP_THRESHOLD",  255<<5);
        lwgpu->SetRegFldNum("LW_THERM_EVT_THERMAL_8", "_TEMP_THRESHOLD",  255<<5);
        lwgpu->SetRegFldNum("LW_THERM_EVT_THERMAL_9", "_TEMP_THRESHOLD",  255<<5);
        lwgpu->SetRegFldNum("LW_THERM_EVT_THERMAL_10", "_TEMP_THRESHOLD",  255<<5);
        lwgpu->SetRegFldNum("LW_THERM_EVT_THERMAL_11", "_TEMP_THRESHOLD",  255<<5);
    }

    UINT32 data;
        //initial GPIO_9,12
		//LW_PMGR_GPIO_9_OUTPUT_CNTL has changed to LW_GPIO_OUTPUT_CNTL(9)
        if (lwgpu->SetRegFldDef("LW_GPIO_OUTPUT_CNTL(9)", "_SEL", "_FAN_ALERT")) { ErrPrintf("could not update LW_GPIO_OUTPUT_CNTL(9)"); }
        if (lwgpu->SetRegFldDef("LW_GPIO_OUTPUT_CNTL(12)", "_SEL", "_INIT")) { ErrPrintf("could not update LW_GPIO_OUTPUT_CNTL(12)"); }

        if (lwgpu->SetRegFldDef("LW_GPIO_OUTPUT_CNTL(9)", "_OPEN_DRAIN", "_ENABLE")) {ErrPrintf("could not write LW_GPIO_OUTPUT_CNTL(9)_OPEN_DRAIN\n"); }
        if (lwgpu->SetRegFldDef("LW_GPIO_OUTPUT_CNTL(9)", "_IO_OUT_ILW", "_ENABLE")) {ErrPrintf("could not write LW_GPIO_OUTPUT_CNTL(9)_IO_OUT_ILW\n"); }
        if (lwgpu->SetRegFldDef("LW_PMGR_GPIO_OUTPUT_CNTL_TRIGGER", "_UPDATE", "_TRIGGER")) {ErrPrintf("could not write LW_PMGR_GPIO_OUTPUT_CNTL_TRIGGER\n"); }

    //set GR engine to FULLPOWER
    if (lwgpu->SetRegFldDef("LW_THERM_GATE_CTRL(0)", "_ENG_CLK", "_RUN")) { ErrPrintf("could not update LW_THERM_GATE_CTRL(0)"); }

    UINT32 addr=0x00020160;
    //set hw failsafe slowdown factor to global initially
    data = lwgpu->RegRd32(addr);
    data = data & 0xffffff7f ;
    lwgpu->RegWr32(addr, data);
    data = lwgpu->RegRd32(addr);
    // InfoPrintf("CLK_SLOWDOWN_0(0) = %x\n", data);

    if (macros.lw_pwr_sysclk_slowdown_support) {
    addr=0x00020164;
    data = lwgpu->RegRd32(addr);
    data = data & 0xffffff7f ;
    lwgpu->RegWr32(addr, data);
    data = lwgpu->RegRd32(addr);
    // InfoPrintf("CLK_SLOWDOWN_0(1) = %x\n", data);
    }
    if (macros.lw_pwr_ltcclk_slowdown_support) {
    addr=0x0002016c;
    data = lwgpu->RegRd32(addr);
    data = data & 0xffffff7f ;
    lwgpu->RegWr32(addr, data);
    data = lwgpu->RegRd32(addr);
    // InfoPrintf("CLK_SLOWDOWN_0(3) = %x\n", data);
    }

    //disable CONFIG2_SLOWDOWN_FACTOR_EXTENDED
    addr=0x00020130;
    data = lwgpu->RegRd32(addr);
    data = data & 0xfeffffff ;
    lwgpu->RegWr32(addr, data);
    data = lwgpu->RegRd32(addr);
    // InfoPrintf("CONFIG2 = %x\n", data);

    //disable gradual slowdown. We do not test gradual slowdown here.
    //gradual slowdown is tested in powerCtrl_fermi -testclkSlowdown.
    if (lwgpu->SetRegFldDef("LW_THERM_CONFIG2","_GRAD_ENABLE","_INIT"))
    {
        ErrPrintf("Could not configure LW_THERM_CONFIG2_GRAD_ENABLE_INIT\n");
    }

    //disable thermal overt output. Otherwise it will influence overt_pad value which will affact overt_IB.
    if (lwgpu->SetRegFldDef("LW_THERM_OVERT_CTRL", "_OTOB_ENABLE", "_OFF"))
    {
        ErrPrintf("Could not configure LW_THERM_CTRL_1_OTOB_ENABLE_OFF\n");
    }

    // mask interrupt to rm
    lwgpu->SetRegFldDef("LW_PMGR_RM_INTR_MSK_GPIO_LIST_1", "_GPIO12_FALLING", "_DISABLED");
    lwgpu->SetRegFldDef("LW_PMGR_RM_INTR_MSK_GPIO_LIST_1", "_GPIO12_RISING", "_DISABLED");

    // this turns off Gpio[], turns off thermal interrupt enable, and clears thermal interrupt register
        Platform::EscapeWrite ("overt_pad", 0, 1, 1);
    setGpio(9, 0,0); setGpio(12, 0,0);
    if(resetRegister())
    {
        SetStatus(TEST_FAILED);
        getStateReport()->runFailed("Failed resetRegister\n");
        ErrPrintf("thermalCtrlGpu_maxwell::resetRegister failed\n");
        return;
    }

    if (check_therm2host_intr_run)
    {
      InfoPrintf("ThermalCtrl_maxwell: Running checkTherm2HostIntrConnection\n");
      if(checkTherm2HostIntrConnection())
      {
          SetStatus(TEST_FAILED);
          getStateReport()->runFailed("Failed checkTherm2HostIntrConnection\n");
          ErrPrintf("thermalCtrlGpu_maxwell::checkTherm2HostIntrConnection test failed\n");
          return;
      }
    }

    if(check_gpio_connection_run) {
        InfoPrintf("ThermalCtrl_maxwell: Running checkGpioConn\n");
        if(checkGpioConn())
        {
            SetStatus(TEST_FAILED);
            getStateReport()->runFailed("Failed checkGpioConn\n");
            ErrPrintf("thermalCtrlGpu_maxwell::checkGpioConn test failed\n");
            return;
        }
    }

    if(check_fuse_run) {
        InfoPrintf("ThermalCtrl_maxwell: Running checkFuse\n");
        if(checkFuse())
        {
            SetStatus(TEST_FAILED);
            getStateReport()->runFailed("Failed checkFuse\n");
            ErrPrintf("thermalCtrlGpu_maxwell::checkFuse test failed\n");
            return;
        }
    }

    if (slowdown_run)
    {
        InfoPrintf("ThermalCtrl_maxwell: Running Slowdown Tests\n");
        if(checkSlowdown())
        {
            SetStatus(TEST_FAILED);
            getStateReport()->runFailed("Failed checkSlowdown\n");
            ErrPrintf("thermalCtrlGpu_maxwell::checkSlowdown test failed\n");
            return;
        }
    }

    if (idle_slowdown_run)
    {
      InfoPrintf("ThermalCtrl_maxwell: Running idle_slowdown Tests\n");
      if(checkIdleSlowdown())
      {
          SetStatus(TEST_FAILED);
          getStateReport()->runFailed("Failed checkIdleSlowdown\n");
          ErrPrintf("thermalCtrlGpu_maxwell::checkIdleSlowdown test failed\n");
          return;
      }
    }

    if (therm_sensor_connectivity_run)
    {
        InfoPrintf("ThermalCtrl_maxwell: Running therm_sensor_connectivity Test\n");
        if(checkThermSensorConnectivity())
        {
            SetStatus(TEST_FAILED);
            getStateReport()->runFailed("Failed checkThermSensorConnectivity\n");
            ErrPrintf("thermalCtrlGpu_maxwell::checkThermSensorConnectivity test failed\n");
            return;
        }
    }

    if (dvco_slowdown_run)
    {
        InfoPrintf("ThermalCtrl_maxwell: Running dvco_slowdown Test\n");
        if(checkDvcoSlowdown())
        {
            SetStatus(TEST_FAILED);
            getStateReport()->runFailed("Failed dvco_slowdown\n");
            ErrPrintf("thermalCtrlGpu_maxwell::checkDvcoSlowdown test failed\n");
            return;
        }
    }

    InfoPrintf("ThermalCtrl_maxwell complete\n");

    SetStatus(TEST_SUCCEEDED);
    getStateReport()->crcCheckPassedGold();
    ch->WaitForIdle();
    return;
}

// enable or disable gpio alt output function.
void ThermalCtrl_maxwell::setGpioAlt(int bit, int en)
{
    if (!en)
    {
        // Disable gpio_alt_func
        if((m_arch & 0xff0) == 0x400){    //gf10x
            if (bit == 8) {
              if (lwgpu->SetRegFldDef("LW_PMGR_ALT_GPIO", "_GPIO_8", "_NORMAL")) { ErrPrintf("could not update LW_PMGR_ALT_GPIO"); }
            } else if (bit == 9) {
              if (lwgpu->SetRegFldDef("LW_PMGR_ALT_GPIO", "_GPIO_9", "_NORMAL")) { ErrPrintf("could not update LW_PMGR_ALT_GPIO"); }
            } else if (bit == 12) {
              if (lwgpu->SetRegFldDef("LW_PMGR_ALT_GPIO", "_GPIO_12", "_NORMAL")) { ErrPrintf("could not update LW_PMGR_ALT_GPIO"); }
            } else {
              ErrPrintf("unsupported gpio bit %d", bit);
            }
        }
        else{    //gf11x
            if (bit == 8) {
              if (lwgpu->SetRegFldDef("LW_GPIO_OUTPUT_CNTL(8)", "_SEL", "_INIT")) { ErrPrintf("could not update LW_GPIO_OUTPUT_CNTL(8)_SEL_INIT"); }
            } else if (bit == 9) {
              if (lwgpu->SetRegFldDef("LW_GPIO_OUTPUT_CNTL(9)", "_SEL", "_INIT")) { ErrPrintf("could not update LW_GPIO_OUTPUT_CNTL(9)_SEL_INIT"); }
            } else if (bit == 12) {
              if (lwgpu->SetRegFldDef("LW_GPIO_OUTPUT_CNTL(12)", "_SEL", "_INIT")) { ErrPrintf("could not update LW_GPIO_OUTPUT_CNTL(12)_SEL_INIT"); }
            } else {
              ErrPrintf("unsupported gpio bit %d", bit);
            }
        }
    }
    else{
        // enable gpio_alt_func
        if((m_arch & 0xff0) == 0x400){    //gf10x
            if (bit == 8) {
              if (lwgpu->SetRegFldDef("LW_PMGR_ALT_GPIO", "_GPIO_8", "_ALTERNATE")) { ErrPrintf("could not update LW_PMGR_ALT_GPIO"); }
            } else if (bit == 9) {
              if (lwgpu->SetRegFldDef("LW_PMGR_ALT_GPIO", "_GPIO_9", "_ALTERNATE")) { ErrPrintf("could not update LW_PMGR_ALT_GPIO"); }
            } else if (bit == 12) {
              if (lwgpu->SetRegFldDef("LW_PMGR_ALT_GPIO", "_GPIO_12", "_ALTERNATE")) { ErrPrintf("could not update LW_PMGR_ALT_GPIO"); }
            } else {
              ErrPrintf("unsupported gpio bit %d", bit);
            }
        }
        else{    //gf11x
            if (bit == 8) {
                    if (lwgpu->SetRegFldDef("LW_GPIO_OUTPUT_CNTL(8)", "_SEL", "_THERMAL_OVERT")) { ErrPrintf("could not update LW_GPIO_OUTPUT_CNTL(8)_SEL_THERMAL_OVERT"); }
            } else if (bit == 9) {
              if (lwgpu->SetRegFldDef("LW_GPIO_OUTPUT_CNTL(9)", "_SEL", "_FAN_ALERT")) { ErrPrintf("could not update LW_GPIO_OUTPUT_CNTL(9)"); }
            } else if (bit == 12) {
              if (lwgpu->SetRegFldNum("LW_GPIO_OUTPUT_CNTL(12)", "_SEL", 0xc)) { ErrPrintf("could not update LW_GPIO_OUTPUT_CNTL(12)"); }
            } else {
              ErrPrintf("unsupported gpio bit %d", bit);
            }
        }
    }
}

void ThermalCtrl_maxwell::setGpio(int bit, int en, int value)
{

    UINT32 gpio_ext_en, gpio_ext, mask;

    mask = 1 << bit;
    if (!en)   // this disables the Gpio
    {
    Platform::EscapeRead("pmgr.drv_gpio_if_en", 0, 4, &gpio_ext_en);
    gpio_ext_en &= ~mask;
    Platform::EscapeWrite("pmgr.drv_gpio_if_en", 0, 4, gpio_ext_en);
    }
    else{
        // Set the value for Gpio
        Platform::EscapeRead("pmgr.drv_pmgr_gpio", 0, 4, &gpio_ext);
        if (value) {
          gpio_ext |= mask;
        } else {
          gpio_ext &= ~mask;
        }
        Platform::EscapeWrite("pmgr.drv_pmgr_gpio", 0, 4, gpio_ext);
        Platform::EscapeRead("pmgr.drv_gpio_if_en", 0, 4, &gpio_ext_en);
        gpio_ext_en |= mask;
        Platform::EscapeWrite("pmgr.drv_gpio_if_en", 0, 4, gpio_ext_en);
    }
}

int ThermalCtrl_maxwell::resetRegister()
{
    //UINT32 data;

    // Disable Interrupt
        if (lwgpu->SetRegFldNum("LW_THERM_EVENT_INTR_ON_TRIGGERED","",0x0))
        {
            ErrPrintf("could not write LW_THERM_EVENT_INTR_ON_TRIGGERED\n");
            return (1);
        }
        if (lwgpu->SetRegFldNum("LW_THERM_EVENT_INTR_ON_NORMAL","",0x0))
        {
            ErrPrintf("could not write LW_THERM_EVENT_INTR_ON_NORMAL\n");
            return (1);
        }

    // reset thermal triggered interrupt
        lwgpu->SetRegFldNum("LW_THERM_EVENT_INTR_PENDING", "", 0x1F1FF);

    Platform::EscapeWrite("tb_thermal_trig_intr",  0, 1, 0);

    return 0;
}

float ThermalCtrl_maxwell::Slow_Factor(int factor,bool extend, int jtag_val)
{
  float real_factor=0;
  int factor_extend=0;

  //if extend=true, decode it
  if(extend){
     if(factor == 1) factor_extend=0;
     else if(factor == 2) factor_extend=2;
     else if(factor == 4) factor_extend=6;
     else if(factor == 8) factor_extend=14;
     else if(factor == 16) factor_extend=30;
     else if(factor == 32) factor_extend=62;
     else factor_extend=0;
  }
  else
     factor_extend=factor;

  int factor_sum = factor_extend*jtag_val/2 + factor_extend + jtag_val;
  if(factor_sum < 63){
    float fz = (float) 255*(factor_extend+2);
    float fm = (float) (510);
    real_factor = fz/fm;
    InfoPrintf("fz=%f, fm=%f, factor_sum=%d, real_factor=%f\n", fz, fm, factor_sum, real_factor);
    }
  else
    {
    float fd = (float) jtag_val/2;
    float fa = fd + 1;
    float fb = (float) fa/31;
    float fc = (float) ((255)*fb);
    real_factor= (float) 255/fc;
    InfoPrintf("fa=%f, fb=%f, fc=%f, factor_sum=%d, real_factor=%f\n", fa, fb, fc, factor_sum, real_factor);
    }

  InfoPrintf("factor=%d, factor_extend=%d, jtag_val=%d, real_factor=%f\n\n", factor, factor_extend, jtag_val, real_factor);

  return real_factor;
}

/* The function knows the clock monitor paths. They exist at least in dGPU.
    +--------------+-------------------------------------+------------------------------------+-----------------------------------+--------------------------------------------+-----------------------------------------+
    | Clock        | Enable                              | Reset                              | Done                              | Count                                      | Period                                  |
    +--------------+-------------------------------------+------------------------------------+-----------------------------------+--------------------------------------------+-----------------------------------------+
    | gpcclk0_noeg | LW_GPC0_CLK_HIER_clk_mon_enable_gpu | LW_GPC0_CLK_HIER_clk_mon_reset_gpu | LW_GPC0_CLK_HIER_clk_mon_done_gpu | clk_gpcclk_noeg_gpu_count_LW_GPC0_CLK_HIER | gpcclk_noeg_gpu_period_LW_GPC0_CLK_HIER |
    +--------------+-------------------------------------+------------------------------------+-----------------------------------+--------------------------------------------+-----------------------------------------+
    | gpcclk1_noeg | LW_GPC1_CLK_HIER_clk_mon_enable_gpu | LW_GPC1_CLK_HIER_clk_mon_reset_gpu | LW_GPC1_CLK_HIER_clk_mon_done_gpu | clk_gpcclk_noeg_gpu_count_LW_GPC1_CLK_HIER | gpcclk_noeg_gpu_period_LW_GPC1_CLK_HIER |
    +--------------+-------------------------------------+------------------------------------+-----------------------------------+--------------------------------------------+-----------------------------------------+
    | gpcclk2_noeg | LW_GPC2_CLK_HIER_clk_mon_enable_gpu | LW_GPC2_CLK_HIER_clk_mon_reset_gpu | LW_GPC2_CLK_HIER_clk_mon_done_gpu | clk_gpcclk_noeg_gpu_count_LW_GPC2_CLK_HIER | gpcclk_noeg_gpu_period_LW_GPC2_CLK_HIER |
    +--------------+-------------------------------------+------------------------------------+-----------------------------------+--------------------------------------------+-----------------------------------------+
    | gpcclk3_noeg | LW_GPC3_CLK_HIER_clk_mon_enable_gpu | LW_GPC3_CLK_HIER_clk_mon_reset_gpu | LW_GPC3_CLK_HIER_clk_mon_done_gpu | clk_gpcclk_noeg_gpu_count_LW_GPC3_CLK_HIER | gpcclk_noeg_gpu_period_LW_GPC3_CLK_HIER |
    +--------------+-------------------------------------+------------------------------------+-----------------------------------+--------------------------------------------+-----------------------------------------+
    | sysclk_noeg  | LW_SYS0_CLK_HIER_clk_mon_enable_gpu | LW_SYS0_CLK_HIER_clk_mon_reset_gpu | LW_SYS0_CLK_HIER_clk_mon_done_gpu | clk_sysclk_noeg_gpu_count_LW_SYS0_CLK_HIER | sysclk_noeg_gpu_period_LW_SYS0_CLK_HIER |
    +--------------+-------------------------------------+------------------------------------+-----------------------------------+--------------------------------------------+-----------------------------------------+
    | ltcclk0_noeg | LW_FBP0_CLK_HIER_clk_mon_enable_gpu | LW_FBP0_CLK_HIER_clk_mon_reset_gpu | LW_FBP0_CLK_HIER_clk_mon_done_gpu | clk_ltcclk_noeg_gpu_count_LW_FBP0_CLK_HIER | ltcclk_noeg_gpu_period_LW_FBP0_CLK_HIER |
    +--------------+-------------------------------------+------------------------------------+-----------------------------------+--------------------------------------------+-----------------------------------------+
    | ltcclk1_noeg | LW_FBP1_CLK_HIER_clk_mon_enable_gpu | LW_FBP1_CLK_HIER_clk_mon_reset_gpu | LW_FBP1_CLK_HIER_clk_mon_done_gpu | clk_ltcclk_noeg_gpu_count_LW_FBP1_CLK_HIER | ltcclk_noeg_gpu_period_LW_FBP1_CLK_HIER |
    +--------------+-------------------------------------+------------------------------------+-----------------------------------+--------------------------------------------+-----------------------------------------+
    | ltcclk2_noeg | LW_FBP2_CLK_HIER_clk_mon_enable_gpu | LW_FBP2_CLK_HIER_clk_mon_reset_gpu | LW_FBP2_CLK_HIER_clk_mon_done_gpu | clk_ltcclk_noeg_gpu_count_LW_FBP2_CLK_HIER | ltcclk_noeg_gpu_period_LW_FBP2_CLK_HIER |
    +--------------+-------------------------------------+------------------------------------+-----------------------------------+--------------------------------------------+-----------------------------------------+
    | ltcclk3_noeg | LW_FBP3_CLK_HIER_clk_mon_enable_gpu | LW_FBP3_CLK_HIER_clk_mon_reset_gpu | LW_FBP3_CLK_HIER_clk_mon_done_gpu | clk_ltcclk_noeg_gpu_count_LW_FBP3_CLK_HIER | ltcclk_noeg_gpu_period_LW_FBP3_CLK_HIER |
    +--------------+-------------------------------------+------------------------------------+-----------------------------------+--------------------------------------------+-----------------------------------------+

 * Expected slowdown factor is the final value, no matter what mode is used. (legacy mode or extended mode).
 */
int ThermalCtrl_maxwell::checkclkcount(float expect_factor_gpc, UINT32 &fullspeed_cnt_ref)
{
    InfoPrintf("checkclkcount() begins -- expect_factor_gpc=%f\n", expect_factor_gpc);

    // Get clock count 
    // If slowdown factor is 1
    //     Save count
    // else
    //     Compare count with saved one

    UINT32 errCnt = 0;

    // Clock monitor usage:
    // step 1: enable clock monitor 
    // step 2: reset count
    // step 3: wait 50us
    // step 4: read count
    // step 5: disable clock monitor

    // Enable clock monitors
    Platform::EscapeWrite("pwr_test_clk_mon_en", 0, 1, 0x1);

    // Reset count
    Platform::EscapeWrite("pwr_test_clk_mon_reset_", 0, 1, 0x0);
    Platform::Delay(1); // 1us
    Platform::EscapeWrite("pwr_test_clk_mon_reset_", 0, 1, 0x1);

    // Wait 50us
    Platform::Delay(50); // 50us

    UINT32 count_value;

  /*
   *  Last owner: Alex Gu
   *  Strategy:   1.  save count value fullspeed==true;
   *              2.  compare count value with the saved value from 1 when fullspeed==false;
   *  Clock Team always change these clk map.
   *  If some of them are not COMPLETE, test will fail.
   *  Please contact the last owner if you miss any new questions.
   */
    Platform::EscapeRead("pwr_test_clk_count_GR_noelcg", 0, 4, &count_value);
    InfoPrintf("GPCCLK clock count in 50 us is %d.\n", count_value);

    // Disable clock monitor
    Platform::EscapeWrite("pwr_test_clk_mon_en", 0, 1, 0x0);

    // Save or Check values
    errCnt += saveOrCheck(expect_factor_gpc, fullspeed_cnt_ref, count_value);

    if (errCnt){
        InfoPrintf("Error: checkclkcount () %d errors detected\n",errCnt);
        return 1;
    }else{
        return 0;
    }
}

UINT32 ThermalCtrl_maxwell::temp2code(UINT32 temp)
{
    UINT32 code;
    static bool first_run = 1;
    static UINT32 int_A, int_B;

    if(first_run) {
        lwgpu->GetRegFldNum("LW_THERM_SENSOR_4", "_HW_A_NOMINAL", &int_A);
        lwgpu->GetRegFldNum("LW_THERM_SENSOR_4", "_HW_B_NOMINAL", &int_B);
        first_run = 0;
    }

    InfoPrintf("int_A = 0x%03x, int_B = 0x%03x\n", int_A, int_B);
//    float A, B;
//      A = int_A / 2**14;
//      B = int_B / 2.0;
    InfoPrintf("A = %f, B = %f\n", int_A/16384.0, int_B/2.0);

    // Temp_Celcius = A * Temp_Code - B
    // Temp_Code = (Temp_Celcius + B) / A
    code = (UINT32)((temp + int_B/2.0)*(1<<14) / int_A);

    InfoPrintf("temp %d, code %d\n", temp, code);
    return code;
}

// set interrupt type for temp going low to high
void ThermalCtrl_maxwell::SetInterruptTypeHigh()
{
    UINT32 rv, rv2;

        // now we can write the interrupt control registers
        lwgpu->SetRegFldNum("LW_THERM_EVENT_INTR_ON_TRIGGERED", "", 0xffffffff);
        lwgpu->SetRegFldNum("LW_THERM_EVENT_INTR_ON_NORMAL", "", 0);

        // read back and print debug in logfile
        lwgpu->GetRegFldNum("LW_THERM_EVENT_INTR_ON_TRIGGERED", "", &rv);
        lwgpu->GetRegFldNum("LW_THERM_EVENT_INTR_ON_NORMAL", "", &rv2);
        InfoPrintf("SetInterruptTypeHigh: LW_THERM_EVENT_INTR_ON_TRIGGERED = "
            "0x%08x, LW_THERM_EVENT_INTR_ON_NORMAL = 0x%08x\n", rv, rv2);
}

// set interrupt type for temp going high to low
void ThermalCtrl_maxwell::SetInterruptTypeLow()
{
UINT32 rv, rv2;
        // now we can write the interrupt control registers
        lwgpu->SetRegFldNum("LW_THERM_EVENT_INTR_ON_TRIGGERED", "", 0);
        lwgpu->SetRegFldNum("LW_THERM_EVENT_INTR_ON_NORMAL", "", 0xffffffff);

        // read back and print debug in logfile
        lwgpu->GetRegFldNum("LW_THERM_EVENT_INTR_ON_TRIGGERED", "", &rv);
        lwgpu->GetRegFldNum("LW_THERM_EVENT_INTR_ON_NORMAL", "", &rv2);
        InfoPrintf("SetInterruptTypeHigh: LW_THERM_EVENT_INTR_ON_TRIGGERED = "
            "0x%08x, LW_THERM_EVENT_INTR_ON_NORMAL = 0x%08x\n", rv, rv2);
}

// set interrupt type to Both
void ThermalCtrl_maxwell::SetInterruptTypeBoth()
{
UINT32 rv, rv2;
        // now we can write the interrupt control registers
        lwgpu->SetRegFldNum("LW_THERM_EVENT_INTR_ON_TRIGGERED", "", 0xffffffff);
        lwgpu->SetRegFldNum("LW_THERM_EVENT_INTR_ON_NORMAL", "", 0xffffffff);

        // read back and print debug in logfile
        lwgpu->GetRegFldNum("LW_THERM_EVENT_INTR_ON_TRIGGERED", "", &rv);
        lwgpu->GetRegFldNum("LW_THERM_EVENT_INTR_ON_NORMAL", "", &rv2);
        InfoPrintf("SetInterruptTypeBoth: LW_THERM_EVENT_INTR_ON_TRIGGERED = "
            "0x%08x, LW_THERM_EVENT_INTR_ON_NORMAL = 0x%08x\n", rv, rv2);
}

// set interrupt type to Disable
void ThermalCtrl_maxwell::SetInterruptTypeDisable()
{
UINT32 rv, rv2;

        // now we can write the interrupt control registers
        lwgpu->SetRegFldNum("LW_THERM_EVENT_INTR_ON_TRIGGERED", "", 0);
        lwgpu->SetRegFldNum("LW_THERM_EVENT_INTR_ON_NORMAL", "", 0);

        // read back and print debug in logfile
        lwgpu->GetRegFldNum("LW_THERM_EVENT_INTR_ON_TRIGGERED", "", &rv);
        lwgpu->GetRegFldNum("LW_THERM_EVENT_INTR_ON_NORMAL", "", &rv2);
        InfoPrintf("SetInterruptTypeDisable: LW_THERM_EVENT_INTR_ON_TRIGGERED = "
            "0x%08x, LW_THERM_EVENT_INTR_ON_NORMAL = 0x%08x\n", rv, rv2);
}

int ThermalCtrl_maxwell::checkExpectFSslowdown(bool exp_sd, int i)
{
    UINT32 data;
    unsigned int clk_hw_failsafe_slowdown;
    int j;
    //read CLK_STATUS_HW_FAILSAFE of gpcclk/sysclk/legclk/ltcclk
    for(j=0; j<4; j++){
        if ((!macros.lw_pwr_sysclk_slowdown_support) && (j==1)) continue;
        if ((!macros.lw_pwr_legclk_slowdown_support) && (j==2)) continue;
        if ((!macros.lw_pwr_ltcclk_slowdown_support) && (j==3)) continue;
        data = 0;
        data = lwgpu->RegRd32(0x000201e0 + j*4);
        clk_hw_failsafe_slowdown = data & 0x00080000;
        InfoPrintf("clk_%d_hw_failsafe_slowdown = %x\n", j, clk_hw_failsafe_slowdown);
        if (exp_sd){
          if (clk_hw_failsafe_slowdown!= 0x80000) {
            ErrPrintf("expect CLK_STATUS_%d_HW_FAILSAFE_SLOWDOWN is ON, but it is OFF, case %d.\n", j, i); return(1);
          }
        }
        else {
          if (clk_hw_failsafe_slowdown!= 0) {
            ErrPrintf("expect CLK_STATUS_%d_HW_FAILSAFE_SLOWDOWN is OFF, but it is ON, case %d.\n", j, i); return(1);
          }
        }
    }
    return 0;
}

// Set the legacy divide factor (fast clock duty cycle 0-255) for SW slowdown:
void ThermalCtrl_maxwell::SetSWSlowdown(int div_factor)
{
  int  i;
  UINT32 addr, data;
  for (i=0;i<4;i++){
      if ((!macros.lw_pwr_sysclk_slowdown_support) && (i==1)) continue;
      if ((!macros.lw_pwr_legclk_slowdown_support) && (i==2)) continue;
      if ((!macros.lw_pwr_ltcclk_slowdown_support) && (i==3)) continue;
      addr = 0x00020180 + 4*i;
      data = lwgpu->RegRd32(addr);
      data = ((data & 0xffffff40) | div_factor );
      lwgpu->RegWr32(addr, data);

      if (div_factor == 0) {
      InfoPrintf("ThermalCtrl_maxwell: SetSlowdown disabling SW_SLOWDOWN , addr=%x, data=%x\n", addr, data);
      }
      else {
      InfoPrintf("ThermalCtrl_maxwell: SetSlowdown enabling SW_SLOWDOWN with divide factor = %d,addr=%x, data=%x \n", div_factor , addr, data);
      }
    }
}

// Set failsafe factor to local for each clock
void ThermalCtrl_maxwell::SetFS2Local(UINT32 addr, int factor)
{
    UINT32 data;
    data = lwgpu->RegRd32(addr);
    data = (data & 0xffff0000) | (1 << 7) | factor ;
    lwgpu->RegWr32(addr, data);

    data = lwgpu->RegRd32(addr);
    InfoPrintf("CLK_SLOWDOWN_0 addr = %x, data = %x\n", addr, data);
}

// Set idle_slowdown type for condition_a and condition_b
void ThermalCtrl_maxwell::SetIdleSD_type(IDLE_TYPE_MAXWELL condition_a , IDLE_TYPE_MAXWELL condition_b)
{
    int i;
    UINT32 data, addr;
    for (i=0;i<4;i++){
        if ((!macros.lw_pwr_sysclk_slowdown_support) && (i==1)) continue;
        if ((!macros.lw_pwr_legclk_slowdown_support) && (i==2)) continue;
        if ((!macros.lw_pwr_ltcclk_slowdown_support) && (i==3)) continue;
       addr = 0x201a0 + 4*i;
       data = lwgpu->RegRd32(addr);
       data = (data & 0xffff8f8f) | (condition_a << 4) | (condition_b << 12);
       lwgpu->RegWr32(addr, data);

       InfoPrintf("condition_a = %x, condition_b = %x\n", condition_a, condition_b);
    }
}

// Set eng_src for condition_a when select condition_a to ENG_TYPE
void ThermalCtrl_maxwell::SetIdleSD_eng_src(int eng_src)
{
    int i;
    UINT32 data, addr;
    for (i=0;i<4;i++){
        if ((!macros.lw_pwr_sysclk_slowdown_support) && (i==1)) continue;
        if ((!macros.lw_pwr_legclk_slowdown_support) && (i==2)) continue;
        if ((!macros.lw_pwr_ltcclk_slowdown_support) && (i==3)) continue;
       addr = 0x201a0 + 4*i;
       data = lwgpu->RegRd32(addr);
       data = (data & 0xfffffff0) | eng_src;
       lwgpu->RegWr32(addr, data);

       InfoPrintf("eng_src = %x\n", eng_src);
    }
}

// Set pmu_src for condition_a when select condition_a to PMU_TYPE
void ThermalCtrl_maxwell::SetIdleSD_pmu_src(PMU_SRC_MAXWELL pmu_src)
{
    int i;
    UINT32 data, addr;
    for (i=0;i<4;i++){
        if ((!macros.lw_pwr_sysclk_slowdown_support) && (i==1)) continue;
        if ((!macros.lw_pwr_legclk_slowdown_support) && (i==2)) continue;
        if ((!macros.lw_pwr_ltcclk_slowdown_support) && (i==3)) continue;
       addr = 0x201a0 + 4*i;
       data = lwgpu->RegRd32(addr);
       data = (data & 0xfffffff0) | pmu_src;
       lwgpu->RegWr32(addr, data);

       InfoPrintf("pmu_src = %x\n", pmu_src);
    }
}

int ThermalCtrl_maxwell::checkExpectIdleslowdown(bool exp_sd, int i)
{
    UINT32 data;
    unsigned int clk_idle_slowdown;
    //read CLK_STATUS_IDLE_SLOW of gpcclk
    data = 0;
    data = lwgpu->RegRd32(0x000201e0);
    clk_idle_slowdown = data & 0x00040000;
    InfoPrintf("clk_status_idle_slow_0 = %x\n", clk_idle_slowdown);
    if (exp_sd){
        if (clk_idle_slowdown != 0x40000) {
            ErrPrintf("expect CLK_STATUS_IDLE_SLOW_0 is ON, but it is OFF, case %d.\n", i); return(1);
        }
    }
    else {
        if (clk_idle_slowdown!= 0) {
            ErrPrintf("expect CLK_STATUS_IDLE_SLOW_0 is OFF, but it is ON, case %d.\n", i); return(1);
        }
    }
    return 0;
}

/* Set all hw failsafe slowdown factor
*/
int ThermalCtrl_maxwell::setSlowfactor(int factor)
{
    if (macros.lw_chip_thermal_sensor)
    {
        lwgpu->SetRegFldNum("LW_THERM_EVT_EXT_OVERT","_SLOW_FACTOR", factor);
        lwgpu->SetRegFldNum("LW_THERM_EVT_EXT_ALERT","_SLOW_FACTOR", factor);
        lwgpu->SetRegFldNum("LW_THERM_EVT_EXT_POWER","_SLOW_FACTOR", factor);
        lwgpu->SetRegFldNum("LW_THERM_EVT_THERMAL_0","_SLOW_FACTOR", factor);
        lwgpu->SetRegFldNum("LW_THERM_EVT_THERMAL_1","_SLOW_FACTOR", factor);
        lwgpu->SetRegFldNum("LW_THERM_EVT_THERMAL_2","_SLOW_FACTOR", factor);
        lwgpu->SetRegFldNum("LW_THERM_EVT_THERMAL_3","_SLOW_FACTOR", factor);
        lwgpu->SetRegFldNum("LW_THERM_EVT_THERMAL_4","_SLOW_FACTOR", factor);
        lwgpu->SetRegFldNum("LW_THERM_EVT_THERMAL_5","_SLOW_FACTOR", factor);
        lwgpu->SetRegFldNum("LW_THERM_EVT_THERMAL_6","_SLOW_FACTOR", factor);
        lwgpu->SetRegFldNum("LW_THERM_EVT_THERMAL_7","_SLOW_FACTOR", factor);
        lwgpu->SetRegFldNum("LW_THERM_EVT_THERMAL_8","_SLOW_FACTOR", factor);
        lwgpu->SetRegFldNum("LW_THERM_EVT_THERMAL_9","_SLOW_FACTOR", factor);
        lwgpu->SetRegFldNum("LW_THERM_EVT_THERMAL_10","_SLOW_FACTOR", factor);
        lwgpu->SetRegFldNum("LW_THERM_EVT_THERMAL_11","_SLOW_FACTOR", factor);
    }
    if (macros.lw_pwr_tegra_slowdown_support)
    {
        lwgpu->SetRegFldNum("LW_THERM_EXT_THERM_0","_SLOW_FACTOR", factor);
        lwgpu->SetRegFldNum("LW_THERM_EXT_THERM_1","_SLOW_FACTOR", factor);
        lwgpu->SetRegFldNum("LW_THERM_EXT_THERM_2","_SLOW_FACTOR", factor);
    }
    return 0;
}

/* Checks GPIO connection, polarity, input and output
* For each polarity, Gpio 9/12 goes from 0->1->0 to
* check whether HW_FAILSAFE_SLOWDOWN happens correctly
  Input ports:
  +-----------------+-------------------+---------------+
  | Default pin num | GPIO_INPUT_CNTL_x | Function      |
  +-----------------+-------------------+---------------+
  | 9               | 22                | thermal_alert |
  +-----------------+-------------------+---------------+
  | 12              | 23                | power_alert   |
  +-----------------+-------------------+---------------+

  Output ports:
  +------------+---------+
  | SEL        | Pin num |
  +------------+---------+
  | _FAN_ALERT | 9       |
  +------------+---------+

Note: OVERT used to use GPIO8, but later was assigned a dedicated pin.
  Specific test was created for overt. Remove it from this one.
*/

int ThermalCtrl_maxwell::checkGpioConn()
{
    UINT32 data;
    UINT32 errCnt=0;
    bool fail=0;

    // enable ext_alert and ext_power slowdown
    lwgpu->SetRegFldNum("LW_THERM_USE_A", "", 0x00000006);// Enable EXT_ALERT and EXT_POWER. Disable others.
    lwgpu->SetRegFldNum("LW_THERM_USE_B", "", 0x00000006);// Enable EXT_ALERT and EXT_POWER. Disable others.

    //enable failsafe slowdown
    setSlowfactor(3/*_DIV8*/);
    data = 0x1;

    // non ilwerted
    setGpio(9, 1, 0); setGpio(12, 1, 0);
    Platform::Delay(2);
    if (checkExpectFSslowdown(false, 0)) 
        errCnt++;

    setGpio(9, 1, 1);
    Platform::Delay(2);
    if (checkExpectFSslowdown(true, 3)) 
        errCnt++;

    setGpio(9, 1, 0);
    Platform::Delay(2);
    if (checkExpectFSslowdown(false, 4)) 
        errCnt++;

    setGpio(12, 1, 1);
    Platform::Delay(2);
    if (checkExpectFSslowdown(true, 5))
        errCnt++;

    setGpio(12, 1, 0);
    Platform::Delay(2);
    if (checkExpectFSslowdown(false, 6)) 
        errCnt++;

    if (errCnt == 0) 
    {
        InfoPrintf("checkGpioConn GPIO 9,12 are checked ok when not ilwerted.\n");
    }
    else
    {
        ErrPrintf("checkGpioConn GPIO 9,12 are checked with %d errors when not ilwerted.\n",errCnt);
        fail=1;
    }
    errCnt = 0;

    // ilwert polarity of GPIO pins
    if (lwgpu->SetRegFldDef("LW_PMGR_GPIO_INPUT_CNTL_22", "_ILW", "_YES")) 
        ErrPrintf("could not update LW_PMGR_GPIO_INPUT_CNTL_22");
    if (lwgpu->SetRegFldDef("LW_PMGR_GPIO_INPUT_CNTL_23", "_ILW", "_YES")) 
        ErrPrintf("could not update LW_PMGR_GPIO_INPUT_CNTL_23"); 

    // from now on, Gpio and overt pin has ilwert polarity
    setGpio(9, 1, 1); setGpio(12, 1, 1);

    setGpio(9, 1, 0);
    Platform::Delay(2);
    if (checkExpectFSslowdown(true, 9)) 
        errCnt++;

    setGpio(9, 1, 1);
    Platform::Delay(2);
    if (checkExpectFSslowdown(false, 10)) 
        errCnt++;

    setGpio(12, 1, 0);
    Platform::Delay(2);
    if (checkExpectFSslowdown(true, 11)) 
        errCnt++;

    setGpio(12, 1, 1);
    Platform::Delay(2);
    if (checkExpectFSslowdown(false, 12)) 
        errCnt++;

    if (errCnt == 0) 
    {
        InfoPrintf("checkGpioConn GPIO 9,12 are checked ok when ilwerted.\n");
    }
    else
    {
        ErrPrintf("checkGpioConn GPIO 9,12 are checked with %d errors when ilwerted.\n",errCnt);
        fail=1;
    }
    errCnt = 0;

    // ==================================================
    // now test lw_therm driving GPIO 9.
    // simTop.lw.Gpio is connected to several drivers/monitors.
    // Disable pmgr_monitors to prevent multi-driving
    setGpio(9, 0, 1);

    // enable gpio bit alternate function, so they can be used as alert output
    setGpioAlt(9, 1);

    if (lwgpu->SetRegFldDef("LW_GPIO_OUTPUT_CNTL(9)", "_OPEN_DRAIN", "_DISABLE")) 
    {
        ErrPrintf("could not write LW_GPIO_OUTPUT_CNTL(9)_OPEN_DRAIN\n"); 
        return(1);
    }
    if (lwgpu->SetRegFldDef("LW_GPIO_OUTPUT_CNTL(9)", "_IO_OUT_ILW", "_DISABLE")) 
    {
        ErrPrintf("could not write LW_GPIO_OUTPUT_CNTL(9)_IO_OUT_ILW\n"); 
        return(1);
    }

    if (lwgpu->SetRegFldDef("LW_PMGR_GPIO_OUTPUT_CNTL_TRIGGER", "_UPDATE", "_TRIGGER")) 
    {
        ErrPrintf("could not write LW_PMGR_GPIO_OUTPUT_CNTL_TRIGGER\n"); 
        return(1);
    }

    // Override fan pwm to _POLARITY when thermal alert happens. Thermal alert is the one generated from LW_THERM_ALERT_EN.
    // LW_PMGR_GPIO_9_OUTPUT_CNTL_SEL_FAN_ALERT also selects this alert signal.
    if (lwgpu->SetRegFldDef("LW_PMGR_FAN1", "_ALERT_OUTPUT_OVERRIDE", "_ENABLED")) 
    {
        ErrPrintf("could not write LW_PMGR_FAN1_ALERT_OUTPUT_OVERRIDE\n"); 
        return(1);
    }
    if (lwgpu->SetRegFldDef("LW_PMGR_FAN1", "_ALERT_OUTPUT_POLARITY", "_HI")) 
    {
        ErrPrintf("could not write LW_PMGR_FAN1_ALERT_OUTPUT_POLARITY\n"); 
        return(1);
    }

    lwgpu->SetRegFldNum("LW_THERM_EVT_THERMAL_4", "_TEMP_THRESHOLD",  50<<5);

    lwgpu->SetRegFldDef("LW_THERM_ALERT_EN", "_THERMAL_4", "_ENABLED"); // Set THERMAL4 for alert trigger.

    Platform::EscapeWrite("tb_thermal_sensor_value", 0, 14, temp2code(20));
    Platform::EscapeWrite("tb_thermal_sensor_override", 0, 1, 1);
    Platform::Delay(2);
    //gpio[9]-alert
    Platform::EscapeRead("pmgr.pmgr_gpio_if.Gpio[9]", 0, 4, &data);
    if (data != 0) 
    { 
        ErrPrintf("GPIO[9] isn't 0. case 1.\n"); 
        errCnt++;
    }

    Platform::EscapeWrite("tb_thermal_sensor_value", 0, 14, temp2code(140));
    Platform::Delay(2);

    Platform::EscapeRead("pmgr.pmgr_gpio_if.Gpio[9]", 0, 4, &data);
    if (data != 1) 
    { 
        ErrPrintf("GPIO[9] isn't 1. case 2.\n"); 
        errCnt++;
    }

    Platform::EscapeWrite("tb_thermal_sensor_value", 0, 14, temp2code(20));
    Platform::Delay(2);

    Platform::EscapeRead("pmgr.pmgr_gpio_if.Gpio[9]", 0, 4, &data);
    if (data != 0) 
    { 
        ErrPrintf("GPIO[9] isn't 0. case 3.\n"); 
        errCnt++;
    }

    // ilwert polarity
    if (lwgpu->SetRegFldDef("LW_GPIO_OUTPUT_CNTL(9)", "_IO_OUT_ILW", "_ENABLE")) 
    {   
        ErrPrintf("could not write LW_GPIO_OUTPUT_CNTL(9)_IO_OUT_ILW\n"); 
    }
    if (lwgpu->SetRegFldDef("LW_PMGR_GPIO_OUTPUT_CNTL_TRIGGER", "_UPDATE", "_TRIGGER")) 
    {
        ErrPrintf("could not write LW_PMGR_GPIO_OUTPUT_CNTL_TRIGGER\n");
        errCnt++;
    }
    Platform::Delay(1);

    Platform::EscapeRead("pmgr.pmgr_gpio_if.Gpio[9]", 0, 4, &data);
    if (data != 1) 
    {
        ErrPrintf("GPIO[9] isn't 1. case 2.\n"); 
        errCnt++;
    }

    Platform::EscapeWrite("tb_thermal_sensor_value", 0, 14, temp2code(79));
    Platform::Delay(2);

    Platform::EscapeRead("pmgr.pmgr_gpio_if.Gpio[9]", 0, 4, &data);
    if (data != 0) 
    { 
        ErrPrintf("GPIO[9] isn't 0. case 3.\n"); 
        errCnt++;
    }

    if (lwgpu->SetRegFldDef("LW_GPIO_OUTPUT_CNTL(9)", "_IO_OUT_ILW", "_DISABLE")) 
    {
        ErrPrintf("could not write LW_GPIO_OUTPUT_CNTL(9)_IO_OUT_ILW\n"); 
        errCnt++;
    }
    if (lwgpu->SetRegFldDef("LW_PMGR_GPIO_OUTPUT_CNTL_TRIGGER", "_UPDATE", "_TRIGGER")) 
    {
        ErrPrintf("could not write LW_PMGR_GPIO_OUTPUT_CNTL_TRIGGER\n"); 
        errCnt++;
    }

    if (errCnt == 0)
    {
        InfoPrintf("alert correctly go to GPIO[9].\n");
    } 
    else
    {
        ErrPrintf("alert does not go to GPIO[9]. %d errors detacted\n",errCnt);
        fail = 1;
    }

    if (fail)
    {
        InfoPrintf("checkGpioConn failed. Please check the above errors\n");
        return 1;
    }
    else
    {
        InfoPrintf("checkGpioConn passed\n");
        return 0;
    }
}

/* Check the Fuse connection and corresponding temperature callwlation.
 * Verify two fuse: opt_int_ts_a, opt_int_ts_b
*/
int ThermalCtrl_maxwell::checkFuse()
{
    UINT32 data, i, exp_data;
    // set opt_int_ts_valid, opt_int_ts_a=2, opt_int_ts_b=6
    //programFuse(opt_int_ts_a_addr, 0x2, opt_int_ts_a_mask);    //set opt_int_ts_a = 2
    //programFuse(opt_int_ts_b_addr, 0x1800, opt_int_ts_b_mask); //set opt_int_ts_b = 6
    //updateFuseopt();
    Platform::EscapeWrite("drv_opt_int_ts_a", 0, 10, 2);
    Platform::EscapeWrite("drv_opt_int_ts_b", 0, 10, 6);
    Platform::Delay(150);   //make sure fuse_sense_done=1

    UINT32 hw_a_nominal, hw_b_nominal;
    lwgpu->GetRegFldNum("LW_THERM_SENSOR_4", "_HW_A_NOMINAL", &hw_a_nominal);
    lwgpu->GetRegFldNum("LW_THERM_SENSOR_4", "_HW_B_NOMINAL", &hw_b_nominal);

    // check fuse is working ok.
    lwgpu->GetRegFldNum("LW_THERM_SENSOR_3", "_HW_A", &data);
    if (data != hw_a_nominal + 2) 
    {
      ErrPrintf("LW_THERM_SENSOR_3_HW_A value. exp: 0x%03x, got: %x\n", hw_a_nominal + 2, data);
      return 1;
    }
    lwgpu->GetRegFldNum("LW_THERM_SENSOR_3", "_HW_B", &data);
    if (data != hw_b_nominal + 6) 
    {
      ErrPrintf("LW_THERM_SENSOR_3_HW_B value. exp: 0x%03x, got: %x\n", hw_b_nominal + 6, data);
      return 1;
    }

    // walking ones for opt_int_ts_a
    for (i = 0; i < 10; i++) 
    {
        data = (0x1 << i);
        Platform::EscapeWrite("drv_opt_int_ts_a", 0, 10, data);

        lwgpu->SetRegFldDef("LW_FUSE_FUSECTRL", "_CMD", "_READ");
        Platform::Delay (2);
        lwgpu->GetRegFldNum("LW_THERM_SENSOR_3", "_HW_A", &data);
        exp_data = hw_a_nominal + (0x1 << i);
        if (exp_data >= 0x400) 
        { // A[9:0] overflow
            exp_data = exp_data - 0x400; 
        }
        if (data != exp_data) 
        {
            ErrPrintf("LW_THERM_SENSOR_3_HW_A value. exp: %x, got: %x\n", exp_data, data);
            return 1;
        }
    }

    // walking ones for opt_int_ts_b
    // opt_int_ts_b is 10 bit signed value. It's expanded to 11 bit in therm logic.
    for (i = 0; i < 10; i++) 
    {
        data = (1 << i) ;
        INT32 data_value;
        // Bit 9 is sign bit.
        if (i == 9) 
            data_value = data - (1 << 10);
        else 
            data_value = data;
        exp_data = (INT32)(hw_b_nominal) + data_value;
        exp_data &= (1 << 11)-1;

        Platform::EscapeWrite("drv_opt_int_ts_b", 0, 10, data);
        lwgpu->SetRegFldDef("LW_FUSE_FUSECTRL", "_CMD", "_READ");
        Platform::Delay(2);
        lwgpu->GetRegFldNum("LW_THERM_SENSOR_3", "_HW_B", &data);
        if (data != exp_data) 
        {
            ErrPrintf("LW_THERM_SENSOR_3_HW_B value. exp: 0x%x, got: 0x%x\n", exp_data, data);
            return 1;
        }
    }

    // restore default of 0
    Platform::EscapeWrite("drv_opt_int_ts_a", 0, 10, 0);
    Platform::EscapeWrite("drv_opt_int_ts_b", 0, 10, 0);

    lwgpu->SetRegFldDef("LW_FUSE_FUSECTRL", "_CMD", "_READ");

    InfoPrintf("checkFuse() PASS checkFuse test\n");

    return 0;
}

/*
 * Used to test various idle_conditions.
 * Only keep the part verifying fe2pwr_mode interface
 */
int ThermalCtrl_maxwell::checkIdleSlowdown()
{
    UINT32 errCnt=0;

    // RM may initial idle slowdown during init stage. So disable idle slowdown first to get un-slowed count.
    SetIdleSD_type(TYPE_NEVER_MAXWELL, TYPE_NEVER_MAXWELL);

    // set idle_slowdown factor value for gpcclk
    UINT32 addr, data=0;
    int factor;
    // set gpcclk factor = DIV2 (before extended)
    addr = 0x00020160; factor = 1; // CLK_SLOWDOWN_0(i)
    data = lwgpu->RegRd32(addr);
    data = (data & 0x00c0ffff) | (factor<<16);
    lwgpu->RegWr32(addr, data);

    //======== check condition type ALWAYS and NEVER ===========
    InfoPrintf("//======== check condition type ALWAYS and NEVER ===========\n");
    //CONDITION_A_TYPE_ALWAYS, CONDITION_B_TYPE_NEVER
    SetIdleSD_type(TYPE_ALWAYS_MAXWELL, TYPE_NEVER_MAXWELL);
    if (checkExpectIdleslowdown(true, 1)) 
        errCnt++;

    //CONDITION_A_TYPE_NEVER, CONDITION_B_TYPE_NEVER
    SetIdleSD_type(TYPE_NEVER_MAXWELL, TYPE_NEVER_MAXWELL);
    if (checkExpectIdleslowdown(false, 2)) 
        errCnt++;

    //======== check condition type SLOW ===========
    InfoPrintf("//======== check condition type SLOW ===========\n");
    //CONDITION_A_TYPE_SLOW, CONDITION_B_TYPE_NEVER
    SetIdleSD_type(TYPE_SLOW_MAXWELL, TYPE_NEVER_MAXWELL);

    //select idlesrc_slow[0]
    addr = 0x201a0;
    data = lwgpu->RegRd32(addr);
    data = data & 0xfffffff0 ;
    lwgpu->RegWr32(addr, data);

    if (checkExpectIdleslowdown(false, 3)) 
        errCnt++;

    //CONDITION_A_TYPE_NEVER, CONDITION_B_TYPE_NEVER
    SetIdleSD_type(TYPE_NEVER_MAXWELL, TYPE_NEVER_MAXWELL);
    if (checkExpectIdleslowdown(false, 23)) 
        errCnt++;

    Platform::Delay(2);

    if (errCnt)
    {
        ErrPrintf("checkIdleSlowdown () FAILED with %d errors\n",errCnt);
        return 1;
    }
    else
    {
        InfoPrintf("checkIdleSlowdown () PASS\n");
        return 0;
    }
}

int ThermalCtrl_maxwell::checkThermSensorConnectivity()
{
    UINT32 test_adj, ts_adc;
    UINT32 errCnt=0;
    Platform::EscapeWrite("drv_tsense_ts_avdd", 0, 1, 0x1);
    Platform::EscapeWrite("drv_tsense_ts_avss", 0, 1, 0);
    Platform::EscapeWrite("drv_tsense_vref", 0, 1, 0x1);
    Platform::EscapeWrite("therm_ts_adc_out", 0, 2, 0x2345); // Force macro output
    Platform::Delay(1);
    Platform::EscapeWrite("tb_thermal_sensor_override", 0, 1, 0);
    lwgpu->SetRegFldDef("LW_THERM_TSENSE_0", "_CALI_SRC",  "_REG");

    lwgpu->SetRegFldNum("LW_THERM_TSENSE_0", "_TC_SRC",  0x0);
    InfoPrintf("select LW_THERM_TSENSE_0_TC_SRC_REG\n");

    // step 1, check ts_adc
    Platform::Delay(5);
    Platform::EscapeRead("ts_adc", 0, 4, &ts_adc);
    if (ts_adc != 0x2345) 
    {
        ErrPrintf("ts_adc should be 0x2345, but it is %x\n", ts_adc);
        errCnt++;
    }
    else
    {
        InfoPrintf("ts_adc initial value is 0x2345 as expected.\n");
    }

    // step 2, check ts_calibrate
    lwgpu->SetRegFldNum("LW_THERM_I2CS_SENSOR_TSCFG", "", 0x44332211);
    Platform::Delay(5);
    Platform::EscapeRead("Test_Adj", 0, 4, &test_adj);
    if (test_adj != 0x44332211) 
    {
        ErrPrintf("test_adj should be 0x44332211, but it is %x\n", test_adj);
        errCnt++;
    }
    else
    {
        InfoPrintf("test_adj is 0x44332211 as expected.\n");
    }

    Platform::Delay(2);
    if (errCnt == 0)
    {
        InfoPrintf ("checkThermSensorConnectivity pass\n");
        return 0;
    }
    else
    {
        ErrPrintf ("checkThermSensorConnectivity fail\n");
        return 1;
    }
}

int ThermalCtrl_maxwell::check_power_gating(bool gated)
{
    UINT32 data;
    UINT32 errCnt = 0;
    UINT32 sleep_to_0_gx, sleep_to_1_gx, clamp_to_0_gx, clamp_to_1_gx;
    UINT32 sleep_to_0_vd, sleep_to_1_vd, clamp_to_0_vd, clamp_to_1_vd;

    if(gated == true) {
        //check POWER_HW_FAILSAFE_SHUTDOWN_PWR_GATING
        lwgpu->GetRegFldNum("LW_THERM_POWER_HW_FAILSAFE_SHUTDOWN", "", &data);
        InfoPrintf("LW_THERM_POWER_HW data=%x\n", data);
        if((data & 0x200) != 0x200){   //POWER_HW_FAILSAFE_SHUTDOWN_PWR_GATING
            ErrPrintf("power gating should be triggerred by overtemp, but not\n");
            errCnt++;
        }

        //check failsafe interrupt
        lwgpu->GetRegFldNum("LW_THERM_INTR_0", "", &data);
        InfoPrintf("LW_THERM_INTR_0 data=%x\n", data);
        if((data & 0x200000) != 0x200000){    //INTR_0_THERMAL_TRIGGER_FS_SHUTDOWN
            ErrPrintf("failsafe interrupt should be raised, but not\n");
            errCnt++;
        }

        //check clamp_en and sleep_en signal
        Platform::EscapeRead("sleep_to_0_gx", 0, 1, &sleep_to_0_gx);
        Platform::EscapeRead("clamp_to_1_gx", 0, 1, &clamp_to_1_gx);
        Platform::EscapeRead("sleep_to_0_vd", 0, 1, &sleep_to_0_vd);
        Platform::EscapeRead("clamp_to_1_vd", 0, 1, &clamp_to_1_vd);
        if((sleep_to_0_gx == 0) & (clamp_to_1_gx == 1) & (sleep_to_0_vd == 0) & (clamp_to_1_vd == 1)) {
            InfoPrintf("clamp_en and sleep signal are forced by overtemp rightly \n");
        }

        if((sleep_to_0_gx != 0) & (sleep_to_0_vd != 0) ) {
            ErrPrintf("sleep signal they should be forced by overtemp, but not\n");
            errCnt++;
        }

        if((clamp_to_1_gx != 1) & (clamp_to_1_vd != 1)) {
            ErrPrintf("clamp_en they are should be forced by opt_pgob, but not\n");
            errCnt++;
        }
    }
    else {
        //check POWER_HW_FAILSAFE_SHUTDOWN_PWR_GATING
        lwgpu->GetRegFldNum("LW_THERM_POWER_HW_FAILSAFE_SHUTDOWN", "", &data);
        InfoPrintf("LW_THERM_POWER_HW data=%x\n", data);
        if((data & 0x200) == 0x200){   //POWER_HW_FAILSAFE_SHUTDOWN_PWR_GATING
            ErrPrintf("power gating should be not triggerred by overtemp, but it is\n");
            errCnt++;
        }

        //check failsafe interrupt
        lwgpu->GetRegFldNum("LW_THERM_INTR_0", "", &data);
        InfoPrintf("LW_THERM_INTR_0 data=%x\n", data);
        if((data & 0x200000) == 0x200000){    //INTR_0_THERMAL_TRIGGER_FS_SHUTDOWN
            ErrPrintf("failsafe interrupt should not be raised, but it is\n");
            errCnt++;
        }

        //check clamp_en and sleep_en signal
        Platform::EscapeRead("sleep_to_1_gx", 0, 1, &sleep_to_1_gx);
        Platform::EscapeRead("clamp_to_0_gx", 0, 1, &clamp_to_0_gx);
        Platform::EscapeRead("sleep_to_1_vd", 0, 1, &sleep_to_1_vd);
        Platform::EscapeRead("clamp_to_0_vd", 0, 1, &clamp_to_0_vd);
        if((sleep_to_1_gx == 1) & (clamp_to_0_gx == 0) & (sleep_to_1_vd == 1) & (clamp_to_0_vd == 0)) {
            InfoPrintf("clamp_en and sleep signal are not forced by overtemp\n");
        }

        if((sleep_to_1_gx != 1) & (sleep_to_1_vd != 1)) {
            ErrPrintf("sleep signal they should not be forced by overtemp, but it is\n");
            errCnt++;
        }

        if((clamp_to_0_gx != 0) & (clamp_to_0_vd != 0)) {
            ErrPrintf("clamp_en they should not be forced by overtemp, but it is\n");
            errCnt++;
        }
    }
    Platform::Delay(2);
    if (errCnt == 0) {
        return 0;
    }else{
        return 1;
    }
}

// check pwr2host_therm_intr connection
int ThermalCtrl_maxwell::checkTherm2HostIntrConnection() {
    UINT32 data;
    UINT32 errCnt = 0;

    resetRegister();

    // pwr2host_therm_intr isn't expected at the beginning
    data=lwgpu->RegRd32(CPU_INTR_LEAF);
    InfoPrintf("CPU_INTR_LEAF = %d\n", data);
    data = data & (1<<INTR_BIT);
    if(data!=0) {
        ErrPrintf("CPU_INTR_LEAF thermal interrupt was set unexpectedly!\n");
        errCnt++;
    }

    // Force an interrupt
    /*  event_intr_pending_thermal_0  == `LW_THERM_EVENT_INTR_PENDING_THERMAL_0_YES
     *  event_intr_enable_thermal_0   == `LW_THERM_EVENT_INTR_ENABLE_THERMAL_0_YES
     *  event_intr_route_thermal_0    == `LW_THERM_EVENT_INTR_ROUTE_THERMAL_0_TO_HOST
     *  config above can generate host interrupt.
     */
    lwgpu->SetRegFldDef("LW_THERM_EVENT_INTR_TYPE", "_THERMAL_0", "_LEVEL");
    lwgpu->SetRegFldDef("LW_THERM_EVENT_INTR_ON_TRIGGERED", "_THERMAL_0", "_ENABLED");
    lwgpu->SetRegFldDef("LW_THERM_EVENT_INTR_ON_NORMAL", "_THERMAL_0", "_ENABLED");
    lwgpu->SetRegFldDef("LW_THERM_EVENT_INTR_ENABLE", "_THERMAL_0", "_YES");
    lwgpu->SetRegFldDef("LW_THERM_EVENT_INTR_ROUTE", "_THERMAL_0", "_TO_HOST");

    // Confirm interrupt
    lwgpu->GetRegFldNum("LW_THERM_EVENT_INTR_PENDING", "_THERMAL_0", &data);
    InfoPrintf("LW_THERM_EVENT_INTR_PENDING_THERMAL_0 = %d\n", data);
    if(data!=1) {
        ErrPrintf("LW_THERM_EVENT_INTR_PENDING_THERMAL_0 isn't triggered.\n");
        errCnt++;
    }

    // Check host interrupt status
    data=lwgpu->RegRd32(CPU_INTR_LEAF);
    InfoPrintf("CPU_INTR_LEAF = %d\n", data);
    data = data & (1<<INTR_BIT);
    if(data==0) {
        ErrPrintf("CPU_INTR_LEAF thermal interrupt wasn't triggered!\n");
        errCnt++;
    }

    // Clear interrupt
    lwgpu->SetRegFldDef("LW_THERM_EVENT_INTR_TYPE", "_THERMAL_0", "_EDGE");
    lwgpu->SetRegFldDef("LW_THERM_EVENT_INTR_PENDING", "_THERMAL_0", "_CLEAR");// This actually clear all bits.

    // pwr2host_therm_intr isn't expected at the end
    data=lwgpu->RegRd32(CPU_INTR_LEAF);
    InfoPrintf("CPU_INTR_LEAF = %d\n", data);
    data = data & (1<<INTR_BIT);
    if(data!=0) {
        ErrPrintf("CPU_INTR_LEAF thermal interrupt was set unexpectedly!\n");
        errCnt++;
    }

    if (errCnt == 0) {
        InfoPrintf("checkTherm2HostIntrConnection passed.\n");
        return 0;
    }else{
        ErrPrintf("checkTherm2HostIntrConnection failed.\n");
        return 1;
    }
}

/*
  Use checkclkcount() to verify various slowdown.
  +----+--------+
  |    | gpcclk |
  +----+--------+
  | 0  | 1      |
  +----+--------+
  | 1  | 2      |
  +----+--------+
  | 2  | 4      |
  +----+--------+
  | 3  | 8      |
  +----+--------+
  Basic slowdown mode.
  - Legacy mode slowdown factor
  - Gradual slowdown disabled
*/
int ThermalCtrl_maxwell::checkSlowdown() {
    UINT32 errCnt=0;
    UINT32 fullspeed_cnt=0;
    UINT32 &fullspeed_cnt_ref=fullspeed_cnt;

    errCnt += checkclkcount(1, fullspeed_cnt_ref);

    errCnt += setSwSlowdown(1);
    errCnt += checkclkcount(2, fullspeed_cnt_ref);

    errCnt += setSwSlowdown(2);
    errCnt += checkclkcount(4, fullspeed_cnt_ref);

    errCnt += setSwSlowdown(3);
    errCnt += checkclkcount(8, fullspeed_cnt_ref);

    return errCnt;
}

int ThermalCtrl_maxwell::setSwSlowdown(int gpc) {
    // gpcclk
    lwgpu->SetRegFldNum("LW_THERM_CLK_SLOWDOWN_1(0)", "_SW_THERM_FACTOR", gpc);

    return 0;
}

//  Clocks slowdown value is computed as below:
//  ******************************************************************************
//  res_full_width = ( inA_table * inB_table ) / 2 + ( inA_table + inB_table )  **
//  clamp out to 63 if res_full_width > 64                                      **
//  ******************************************************************************
//  where inA_table is from SW, inB_table is PWR slowdown ratio.
//  in the fbpa case, inA_table = 2.
//  when inB_table = 0x3e, res_full_width = 0x3f;
//  when inB_table = 0x1e, res_full_width = 0x3e;
//  Both 0x3f and 0x3e mean divide by 32.
//  Debug Advice:
//  1.  understand the slowdown algorithm;
//  2.  know the values of inA and inB.
//
int ThermalCtrl_maxwell::saveOrCheck(float expected_slow_factor, UINT32 &fullspeed_cnt_ref, UINT32 count) {
    int errCnt = 0;

    if(expected_slow_factor==1) {
        fullspeed_cnt_ref = count;
        InfoPrintf("fullspeed_cnt_ref = %d\n", fullspeed_cnt_ref);
        if(fullspeed_cnt_ref==0) {
            ErrPrintf("Clock count is 0! Check mapped path!\n");
            errCnt++;
        }
    } else {
        InfoPrintf("checksum = expected_slow_factor*clk_slow/clk_fullspeed = %f*%d/%d\n", expected_slow_factor, count, fullspeed_cnt_ref);
        float checksum = (1.0*expected_slow_factor*count)/fullspeed_cnt_ref;
        if(checksum > 0.90 && checksum < 1.10)
            InfoPrintf("checksum = %f\n", checksum);
        else {
            ErrPrintf("checksum = %f\n", checksum);
            errCnt++;
        }
    }

    return errCnt;
}

/*
 * Trigger dvco slowdown only. 
 * Check the frequencies.
 * Note that only gpcclks support dvco slowdow.
 */
int ThermalCtrl_maxwell::checkDvcoSlowdown() {
    int errCnt = 0;
    UINT32 fullspeed_cnt=0;
    UINT32 &fullspeed_cnt_ref=fullspeed_cnt;

    lwgpu->SetRegFldDef("LW_THERM_GRAD_STEPPING0", "_DVCO_FEATURE", "_ENABLE");

    // Save original frequencies
    // Reusing checkclkcount function, but only gpcclk will slowdown.
    errCnt += checkclkcount(1, fullspeed_cnt_ref);

    // Trigger dvco slowdown
    // Index 0 is for gpcclk
    lwgpu->SetRegFldDef("LW_THERM_CLK_SLOWDOWN_1(0)", "_SW_THERM_TRIGGER", "_SET");

    // Check frequencies after dvco slowdown
    // Assume all GPCs are configured with the same dvco divide ratio.
    UINT32 raw_divide_ratio;
    lwgpu->GetRegFldNum("LW_PTRIM_GPC_GPCNAFLL_DVCO_SLOW(0)", "_DIVIDE_RATIO", &raw_divide_ratio);
    InfoPrintf("raw_divide_ratio is %d\n", raw_divide_ratio);

    float divide_ratio = 0;
    // Please check LW_PTRIM_GPC_GPCNAFLL_DVCO_SLOW_DIVIDE_RATIO field definition for details.
    switch(raw_divide_ratio) {
	case 0: divide_ratio = 1.0; break;
	case 1: divide_ratio = 8.0/7.0; break; // 1.14
	case 2: divide_ratio = 4.0/3.0; break; // 1.33
	case 3: divide_ratio = 1.5; break;
	case 4: divide_ratio = 2.0; break;
	case 5: divide_ratio = 4.0; break;
    }
    InfoPrintf("divide_ratio is %f\n", divide_ratio);

    errCnt += checkclkcount(divide_ratio, fullspeed_cnt_ref);

    return errCnt;
}

