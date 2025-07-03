/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2013-2021 by LWPU Corporation.  All rights reserved. 
 * All information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

// To facilitate faster compiles via pre-compiled headers, keep "stdtest.h" first.
#include "mdiag/tests/stdtest.h"

#include "t_vr_input_side_sensing.h"
#include "mdiag/tests/test_state_report.h"

extern const ParamDecl vr_input_side_sensing_params[] =
{
    SIMPLE_PARAM("-mux_sel_connection", "Test THERM <-> PMGR <-> gpio connection"),
    SIMPLE_PARAM("-adc_therm_connection", "Test ADC to Therm connection"),
    SIMPLE_PARAM("-adc_input_connection", "Test pad to ADC connection"),
    PARAM_SUBDECL(lwgpu_single_params),
    LAST_PARAM
};

STD_TEST_FACTORY(vr_input_side_sensing, VRInputSideSensingConnectionTest);

VRInputSideSensingConnectionTest::VRInputSideSensingConnectionTest(ArgReader *params):
    LWGpuSingleChannelTest(params), errCnt(0)
{
    if(params->ParamPresent("-mux_sel_connection")) test_mux_sel_connection = true;
    else test_mux_sel_connection = false;
    if(params->ParamPresent("-adc_therm_connection")) test_adc_therm_connection = true;
    else test_adc_therm_connection = false;
    if(params->ParamPresent("-adc_input_connection")) test_adc_input_connection = true;
    else test_adc_input_connection = false;
}

int VRInputSideSensingConnectionTest::Setup(void)
{
    lwgpu = LWGpuResource::FindFirstResource();

    m_arch = lwgpu->GetArchitecture();

    ch = lwgpu->CreateChannel();
    if(!ch)
    {
        ErrPrintf("VRInputSideSensingConnectionTest::Setup failed to create channel\n");
        return 0;
    }

    getStateReport()->init("VRInputSideSensingConnectionTest");
    getStateReport()->enable();
    return 1;
}
void VRInputSideSensingConnectionTest::CleanUp(void)
{
    if(ch)
    {
        delete ch;
        ch = 0;
    }

    if (lwgpu)
    {
        DebugPrintf("VRInputSideSensingConnectionTest::CleanUp(): Releasing GPU.\n");
        lwgpu = 0;
    }
    LWGpuSingleChannelTest::CleanUp();
}

void VRInputSideSensingConnectionTest::Run(void)
{
    //  InfoPrintf Flag to determine whether the test fail because of elw issue or test issue.
    InfoPrintf("VR Input Side Sensing Connection Test starting ....\n");

    SetStatus(TEST_INCOMPLETE);
    if (test_mux_sel_connection) errCnt += TestMuxSelConnection();
    if (test_adc_therm_connection) errCnt += TestAdcThermConnection();
    if (test_adc_input_connection) errCnt += TestAdcInputConnection();

    if(errCnt)
    {
        SetStatus(TEST_FAILED);
        getStateReport()->runFailed("Failed test\n");
        InfoPrintf("VRInputSideSensingConnectionTest:: Test Failed with %d errors\n", errCnt);
    }
    else
    {
        SetStatus(TEST_SUCCEEDED);
        getStateReport()->crcCheckPassedGold();
        InfoPrintf("VRInputSideSensingConnectionTest:: Test Passed\n");
    }
    InfoPrintf("VR Input Side Sensing Connection Test END.\n");
}

int VRInputSideSensingConnectionTest::TestMuxSelConnection()
{
    UINT32 gpio_9_status;
    UINT32 data=0;
    int errCnt=0;

    /* Part 1: Check PWR -> PMGR connection of adc_mux_sel */

    // Config PMGR to route adc_mux_sel to GPIO9
    lwgpu->SetRegFldDef("LW_GPIO_OUTPUT_CNTL(9)", "_SEL", "_ADC_MUX_SEL");
    lwgpu->SetRegFldDef("LW_GPIO_OUTPUT_CNTL(9)", "_IO_OUT_EN", "_YES");

    lwgpu->SetRegFldDef("LW_PMGR_GPIO_OUTPUT_CNTL_TRIGGER", "_UPDATE", "_TRIGGER");
    Platform::Delay(10);// 10us

    // Check initial value
    Platform::EscapeRead("gpio_io_9", 0, 1, &gpio_9_status);
    gpio_9_status &= 1;

    InfoPrintf("EscapeRead of gpio_io_9 returns 0x%0x\n", gpio_9_status);

    if(gpio_9_status != 0) {
        ErrPrintf("Initial gpio_io_9 is not 0\n");
        errCnt++;
    }

    // Config therm to drive mux_sel high
    lwgpu->SetRegFldNum("LW_THERM_ADC_PWM", "_PERIOD", 100);
    lwgpu->SetRegFldNum("LW_THERM_ADC_PWM", "_HI", 100);
    lwgpu->SetRegFldNum("LW_THERM_ADC_PWM_GEN", "_HI_LAST", 100);
    lwgpu->SetRegFldNum("LW_THERM_ADC_PWM_GEN", "_ENABLE", 1);

    lwgpu->SetRegFldDef("LW_THERM_ADC_CTRL", "_ACTIVE_CHANNELS_NUM", "_MAX");
    lwgpu->SetRegFldDef("LW_THERM_ADC_CTRL", "_ADC_SENSING", "_ON");

    // Check gpio 9 value
    Platform::Delay(10);// 10us
    Platform::EscapeRead("gpio_io_9", 0, 1, &gpio_9_status);
    gpio_9_status &= 1;

    InfoPrintf("EscapeRead of gpio_io_9 returns 0x%0x\n", gpio_9_status);

    if(gpio_9_status != 1) {
        ErrPrintf("gpio_io_9 is not 1\n");
        errCnt++;
    }

    // Config therm to drive mux_sel low
    lwgpu->SetRegFldNum("LW_THERM_ADC_PWM", "_HI", 0);
    lwgpu->SetRegFldNum("LW_THERM_ADC_PWM_GEN", "_HI_LAST", 0);

    // Check gpio 9 value
    Platform::Delay(10);// 10us
    Platform::EscapeRead("gpio_io_9", 0, 1, &gpio_9_status);
    gpio_9_status &= 1;

    InfoPrintf("EscapeRead of gpio_io_9 returns 0x%0x\n", gpio_9_status);

    if(gpio_9_status != 0) {
        ErrPrintf("gpio_io_9 is not 0\n");
        errCnt++;
    }

    /*  Part 2: Check PMGR -> PWR connection of pmgr2pwr_adc_mux_sel */
    Platform::EscapeWrite("pmgr_adc_mux_sel", 0, 1, 1);
    Platform::Delay(2);// 2us
    data = 0;
    Platform::EscapeRead("pwr_adc_mux_sel", 0, 1, &data);
    if (data != 1) {
        ErrPrintf("1 : pwr_adc_mux_sel should be 1, but it's read back as 0x%x \n", data);
        errCnt++;
    }

    Platform::EscapeWrite("pmgr_adc_mux_sel", 0, 1, 0);
    Platform::Delay(2);// 2us
    Platform::EscapeRead("pwr_adc_mux_sel", 0, 1, &data);
    if (data != 0) {
        ErrPrintf("3 :pwr_adc_mux_sel should be 0, but it's read back as 0x%x \n", data);
        errCnt++;
    }

    return errCnt;
}

int VRInputSideSensingConnectionTest::TestAdcThermConnection()
{
    int errCnt=0;
    int delta_v; // -0.4V ~ 0.4V : ADC 0 - 127. We will only cover positive part.
    int cvdd_sense_int;
    // Configure ADC to generate a known output
    // cvdd_sense_int is initialized to 85000. ADC output is 0x40. See http://lwbugs/1889732/22 for details.
    // Set cvdd_sense_int to a value in [65000, 105000)
    // Expected ADC output is (cvdd_sense_int-65000)*2/625;
    delta_v = rand()%40000;
    cvdd_sense_int = delta_v/2 + 85000;
    Platform::EscapeWrite("cvdd_sense_int", 0, 4, cvdd_sense_int);
    InfoPrintf("cvdd_sense_int value is set to %d\n", cvdd_sense_int);
    InfoPrintf("Expected ADC_DOUT[6:0] is %d\n", delta_v/625+64);

    // Clock ADC sequence can be found in https://p4viewer.lwpu.com/get/hw/doc/gpu/SOC/Clocks/Documentation/POR/GA10X/GA10x-clks-ADC_multisampler.docx
    lwgpu->SetRegFldDef("LW_PTRIM_SYS_NAFLL_SYS_PWR_SENSE_ADC_CTRL", "_IDDQ", "_POWER_ON");
    Platform::Delay(7);// us
    lwgpu->SetRegFldDef("LW_PTRIM_SYS_NAFLL_SYS_PWR_SENSE_ADC_CTRL", "_ENABLE", "_YES");
    Platform::Delay(1);// us
    lwgpu->SetRegFldDef("LW_PTRIM_SYS_NAFLL_SYS_PWR_SENSE_ADC_MULTISAMPLER", "_ENABLE", "_ON");
    Platform::Delay(1);// us

    // Check the value received on THERM side
    UINT32 rdata;
    // ADC_RAW_CORRECTION has default value 0x40. Double it to account for multiple sample (samples == 2)
    lwgpu->SetRegFldNum("LW_THERM_ADC_RAW_CORRECTION", "_VALUE", 128);

    // Set ADC PWM 50/100
    lwgpu->SetRegFldNum("LW_THERM_ADC_PWM", "_HI", 50);
    lwgpu->SetRegFldNum("LW_THERM_ADC_PWM", "_PERIOD", 100);

    lwgpu->SetRegFldNum("LW_THERM_ADC_PWM_GEN", "_HI_LAST", 50);
    lwgpu->SetRegFldNum("LW_THERM_ADC_PWM_GEN", "_ENABLE", 1);

    lwgpu->SetRegFldNum("LW_THERM_ADC_CTRL", "_ACTIVE_CHANNELS_NUM", 1);
    lwgpu->SetRegFldNum("LW_THERM_ADC_CTRL", "_ADC_IIR_LENGTH", 0);
    lwgpu->SetRegFldNum("LW_THERM_ADC_CTRL", "_SAMPLE_DELAY", 5);
    lwgpu->SetRegFldNum("LW_THERM_ADC_CTRL", "_MS_MODE", 0);
    lwgpu->SetRegFldDef("LW_THERM_ADC_CTRL", "_ADC_SENSING", "_ON");

    lwgpu->SetRegFldNum("LW_THERM_ADC_RESET", "_LENGTH", 2); // us
    lwgpu->SetRegFldDef("LW_THERM_ADC_RESET", "_SW_RST", "_TRIGGER");

    do lwgpu->GetRegFldNum("LW_THERM_ADC_RESET", "_SW_RST", &rdata);
    while (rdata != 0);

    Platform::Delay(2);// us

    lwgpu->SetRegFldDef("LW_THERM_ADC_SNAPSHOT", "_SNAP", "_TRIGGER"); // Will be done immediately

    lwgpu->SetRegFldDef("LW_THERM_ADC_IIR_VALUE_INDEX", "_INDEX", "_CH0");
    lwgpu->GetRegFldNum("LW_THERM_ADC_IIR_VALUE_DATA", "_VALUE", &rdata);

    InfoPrintf("IIR channel 0 value is 0x%08x\n", rdata);
    if(abs((INT32)(rdata>>22) - (INT32)(delta_v/625)) > 1) errCnt++; // IIR >> (21 + 1). Extra 1 bit is to account for multisample 2

    // Turn off ADC sensing
    lwgpu->SetRegFldDef("LW_THERM_ADC_CTRL", "_ADC_SENSING", "_OFF");
    lwgpu->SetRegFldDef("LW_THERM_ADC_RESET", "_SW_RST", "_TRIGGER");
    Platform::Delay(5);// us

    return errCnt;
}

int VRInputSideSensingConnectionTest::TestAdcInputConnection()
{
    int errCnt=0;

    for (int i=0; i<4; i++) {
        InfoPrintf("Testing combination %d\n", i);
        // Force the value at lw_top
        Platform::EscapeWrite("top_adc_in", 0, 4, (i>>1) & 0x00000001);
        Platform::EscapeWrite("top_adc_in_n", 0, 4, i & 0x00000001);

        // Check the value received by ADC
        Platform::Delay(1);// us
        UINT32 adc_vinp, adc_vinm;
        int adc_vin;
        Platform::EscapeRead("adc_vinp", 0, 4, &adc_vinp);
        Platform::EscapeRead("adc_vinm", 0, 4, &adc_vinm);
        adc_vinp &= 0x00000001;
        adc_vinm &= 0x00000001;
        adc_vin = (adc_vinp<<1) | adc_vinm;
        InfoPrintf("adc_vinp = %d, adc_vinm = %d\n", adc_vinp, adc_vinm);

        if(adc_vin != i) errCnt++;
    }
    return errCnt;
}
