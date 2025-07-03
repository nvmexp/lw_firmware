/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2010, 2020-2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

// To facilitate faster compiles via pre-compiled headers, keep "stdtest.h" first.
#include "mdiag/tests/stdtest.h"

#include "vid_pwm.h"
#include "mdiag/lwgpu.h"
#include "mdiag/utils/lwgpu_channel.h"
#include "mdiag/tests/test_state_report.h"
#include "core/include/gpu.h"

static bool new_2nd_pwm = false;
extern const ParamDecl vid_pwm_params[] =
{
    SIMPLE_PARAM("-new_2nd_pwm", "add 2nd pwm"),
    PARAM_SUBDECL(lwgpu_single_params),
    LAST_PARAM
};

static int errCnt;

VidPwm::VidPwm(ArgReader *params) :
    LWGpuSingleChannelTest(params)
{
    if (params->ParamPresent("-new_2nd_pwm"))
        new_2nd_pwm = 1;
}

VidPwm::~VidPwm(void)
{
}

STD_TEST_FACTORY(vid_pwm, VidPwm)

int
VidPwm::Setup(void)
{
    lwgpu = LWGpuResource::FindFirstResource();

    ch = lwgpu->CreateChannel();
    if(!ch)
    {
        ErrPrintf("VidPwm: Setup failed to create channel\n");
        return (0);
    }

    m_regMap = lwgpu->GetRegisterMap();
    if(!m_regMap)
    {
        ErrPrintf("VidPwm: vid_pwm can only be run on GPU's that support a register map!\n");
        return (0);
    }

    getStateReport()->init("vid_pwm");
    getStateReport()->enable();

    return 1;
}

void
VidPwm::CleanUp(void)
{
    if(ch)
    {
        delete ch;
        ch = 0;
    }

    if (lwgpu)
    {
        DebugPrintf("VidPwm::CleanUp(): Releasing GPU.\n");
        lwgpu = 0;
    }
    LWGpuSingleChannelTest::CleanUp();
}

void
VidPwm::Run(void)
{
    //  InfoPrintf Flag to determine whether the test fail because of elw issue or test issue.
    InfoPrintf("FLAG : VidPwm starting ....\n");

    SetStatus(TEST_INCOMPLETE);
    if(new_2nd_pwm)   vidpwmTest();

    if(vidpwmTest())
    {
        SetStatus(TEST_FAILED);
        getStateReport()->runFailed("Failed test\n");
        return;
    }

    if (errCnt)
    {
        SetStatus(TEST_FAILED_CRC);
        getStateReport()->crcCheckFailed("Test failed with miscompares, see logfile for messages.\n");
        return;
    }
    SetStatus(TEST_SUCCEEDED);
    getStateReport()->crcCheckPassedGold();
}

UINT32
VidPwm::vidpwmTest()
{

    UINT32 gpio = 0x0;
    UINT32 i = 0x0;
    errCnt = 0;

    // route pwr2pmgr_vid_pwm to gpio
    // any supported gpio will work, but we will pick on GPIO[0]
    if (route_vid_pwm_to_gpio(gpio))
    {
        return (1);
    }

    // test pwr2pmgr_vid_pwm value at 0 and 1
    for (i = 0; i <= 1; i = i + 1)
    {
        // configure pwr2pmgr_vid_pwm value
        if (cfg_vid_pwm_value(i))
        {
            return (1);
        }
        // check gpio value
        if (check_gpio_value(gpio, i))
        {
            return (1);
        }
    }

    // done with test
    InfoPrintf("VidPwm test completed with %d errors\n", errCnt);
    return (0);  // calling routine will check errCnt for CRC fail
}

int
VidPwm::route_vid_pwm_to_gpio(UINT32 gpio)
{
    unique_ptr<IRegisterClass> reg;
    unique_ptr<IRegisterField> field;
    unique_ptr<IRegisterValue> value;
    char regName[63], fieldName[63], valueName[63];
    UINT32 reg_addr = 0x0;
    UINT32 reg_data = 0x0;

    InfoPrintf("VidPwm: Routing pwr2pmgr_vid_pwm to GPIO %d\n", gpio);

    // find GPIO control register, verify GPIO number is supported
    sprintf(regName, "LW_PMGR_GPIO_%d_OUTPUT_CNTL", gpio);
    if (!(reg = m_regMap->FindRegister(regName)))
    {
        ErrPrintf("VidPwm: Failed to find %s register\n", regName);
        return (1);
    }
    reg_addr = reg->GetAddress();

    // configure GPIO to select VID PWM
    sprintf(fieldName, "LW_PMGR_GPIO_%d_OUTPUT_CNTL_SEL", gpio);
    if (!(field = reg->FindField(fieldName)))
    {
        ErrPrintf("VidPwm: Failed to find %s field\n", fieldName);
        return (1);
    }
    if(!new_2nd_pwm) {
    sprintf(valueName, "LW_PMGR_GPIO_%d_OUTPUT_CNTL_SEL_VID_PWM", gpio);
    if (!(value = field->FindValue(valueName)))
    {
        ErrPrintf("VidPwm: Failed to find %s value\n", valueName);
        return (1);
    }
    } else {
    sprintf(valueName, "LW_PMGR_GPIO_%d_OUTPUT_CNTL_SEL_VID_PWM_1", gpio);
    if (!(value = field->FindValue(valueName)))
    {
        ErrPrintf("VidPwm: Failed to find %s value\n", valueName);
        return (1);
    }
    }
    reg_data = (value->GetValue() << field->GetStartBit());

    // disable output ilwersion
    sprintf(fieldName, "LW_PMGR_GPIO_%d_OUTPUT_CNTL_IO_OUT_ILW", gpio);
    if (!(field = reg->FindField(fieldName)))
    {
        ErrPrintf("VidPwm: Failed to find %s field\n", fieldName);
        return (1);
    }
    sprintf(valueName, "LW_PMGR_GPIO_%d_OUTPUT_CNTL_IO_OUT_ILW_DISABLE", gpio);
    if (!(value = field->FindValue(valueName)))
    {
        ErrPrintf("VidPwm: Failed to find %s value\n", valueName);
        return (1);
    }
    reg_data |= (value->GetValue() << field->GetStartBit());

    // disable open drain
    sprintf(fieldName, "LW_PMGR_GPIO_%d_OUTPUT_CNTL_OPEN_DRAIN", gpio);
    if (!(field = reg->FindField(fieldName)))
    {
        ErrPrintf("VidPwm: Failed to find %s field\n", fieldName);
        return (1);
    }
    sprintf(valueName, "LW_PMGR_GPIO_%d_OUTPUT_CNTL_OPEN_DRAIN_DISABLE", gpio);
    if (!(value = field->FindValue(valueName)))
    {
        ErrPrintf("VidPwm: Failed to find %s value\n", valueName);
        return (1);
    }
    reg_data |= (value->GetValue() << field->GetStartBit());

    // write GPIO control register
    InfoPrintf("VidPwm: Writing %s = 0x%08x\n", reg->GetName(), reg_data);
    lwgpu->RegWr32(reg_addr, reg_data);

    // GPIO output control register needs to be triggered before it will take effect
    InfoPrintf("VidPwm: Triggering update of %s\n", reg->GetName());
    if (!(reg = m_regMap->FindRegister("LW_PMGR_GPIO_OUTPUT_CNTL_TRIGGER")))
    {
        ErrPrintf("VidPwm: Failed to find LW_PMGR_GPIO_OUTPUT_CNTL_TRIGGER register\n");
        return (1);
    }
    reg_addr = reg->GetAddress();
    if (!(field = reg->FindField("LW_PMGR_GPIO_OUTPUT_CNTL_TRIGGER_UPDATE")))
    {
        ErrPrintf("VidPwm: Failed to find LW_PMGR_GPIO_OUTPUT_CNTL_TRIGGER_UPDATE field\n");
        return (1);
    }
    if (!(value = field->FindValue("LW_PMGR_GPIO_OUTPUT_CNTL_TRIGGER_UPDATE_TRIGGER")))
    {
        ErrPrintf("VidPwm: Failed to find LW_PMGR_GPIO_OUTPUT_CNTL_TRIGGER_UPDATE_TRIGGER value\n");
        return (1);
    }
    reg_data = (value->GetValue() << field->GetStartBit());
    InfoPrintf("VidPwm: Writing %s = 0x%08x\n", reg->GetName(), reg_data);
    lwgpu->RegWr32(reg_addr, reg_data);

    return (0);
}

int
VidPwm::cfg_vid_pwm_value(UINT32 data)
{
    unique_ptr<IRegisterClass> reg;
    unique_ptr<IRegisterField> field;
    unique_ptr<IRegisterValue> value;
    UINT32 reg_addr = 0x0;
    UINT32 reg_data = 0x0;

    InfoPrintf("VidPwm: Configuring pwr2pmgr_vid_pwm to %d\n", data);

    // configure period to 1
    if (!(reg = m_regMap->FindRegister("LW_THERM_VID0_PWM")))
    {
        ErrPrintf("VidPwm: Failed to find LW_THERM_VID0_PWM register\n");
        return (1);
    }
    if(new_2nd_pwm)
      reg_addr = reg->GetAddress(1);
    else
      reg_addr = reg->GetAddress();
    if (!(field = reg->FindField("LW_THERM_VID0_PWM_PERIOD")))
    {
        ErrPrintf("VidPwm: Failed to find LW_THERM_VID0_PWM_PERIOD field\n");
        return (1);
    }
    reg_data = (1 << field->GetStartBit());
    InfoPrintf("VidPwm: Writing %s = 0x%08x\n", reg->GetName(), reg_data);
    lwgpu->RegWr32(reg_addr, reg_data);

    // configure hi to 0 or 1
    if (!(reg = m_regMap->FindRegister("LW_THERM_VID1_PWM")))
    {
        ErrPrintf("VidPwm: Failed to find LW_THERM_VID1_PWM register\n");
        return (1);
    }
    if(new_2nd_pwm)
      reg_addr = reg->GetAddress(1);
    else
      reg_addr = reg->GetAddress();
    if (!(field = reg->FindField("LW_THERM_VID1_PWM_HI")))
    {
        ErrPrintf("VidPwm: Failed to find LW_THERM_VID1_PWM_HI field\n");
        return (1);
    }
    reg_data = (data << field->GetStartBit());

    // trigger VID PWM update
    if (!(field = reg->FindField("LW_THERM_VID1_PWM_SETTING_NEW")))
    {
        ErrPrintf("VidPwm: Failed to find LW_THERM_VID1_PWM_SETTING_NEW field\n");
        return (1);
    }
    if (!(value = field->FindValue("LW_THERM_VID1_PWM_SETTING_NEW_TRIGGER")))
    {
        ErrPrintf("VidPwm: Failed to find LW_THERM_VID1_PWM_SETTING_NEW_TRIGGER value\n");
        return (1);
    }
    reg_data |= (value->GetValue() << field->GetStartBit());
    InfoPrintf("VidPwm: Writing %s = 0x%08x\n", reg->GetName(), reg_data);
    lwgpu->RegWr32(reg_addr, reg_data);

    return (0);
}

int
VidPwm::check_gpio_value(UINT32 gpio, UINT32 data)
{
    unique_ptr<IRegisterClass> reg;
    unique_ptr<IRegisterField> field;
    char regName[63], fieldName[63];
    UINT32 reg_addr = 0x0;
    UINT32 reg_data = 0x0;
    UINT32 gpio_in = 0x0;
    UINT32 i = 0x0;

    InfoPrintf("VidPwm: Checking value of GPIO %d\n", gpio);

    // read GPIO output control register (this register has a field that reflects the current input value)
    sprintf(regName, "LW_PMGR_GPIO_%d_OUTPUT_CNTL", gpio);
    if (!(reg = m_regMap->FindRegister(regName)))
    {
        ErrPrintf("VidPwm: Failed to find %s register\n", regName);
        return (1);
    }
    reg_addr = reg->GetAddress();
    //just to delay some time
    for (i = 0; i <= 10; i = i + 1) {
      reg_data = lwgpu->RegRd32(reg_addr);
    }
    reg_data = lwgpu->RegRd32(reg_addr);
    InfoPrintf("SmartFan: Reading %s = 0x%08x\n", reg->GetName(), reg_data);

    // get gpio input value
    sprintf(fieldName, "LW_PMGR_GPIO_%d_OUTPUT_CNTL_IO_INPUT", gpio);
    if (!(field = reg->FindField(fieldName)))
    {
        ErrPrintf("VidPwm: Failed to find %s field\n", fieldName);
        return (1);
    }
    gpio_in = (reg_data & field->GetMask()) >> field->GetStartBit();

    if (gpio_in != data)
    {
        ErrPrintf("VidPwm: Mismatch in %s value: actual = %d, expected = %d\n",
            field->GetName(), gpio_in, data);
        errCnt = errCnt + 1;
    }

    return (0);
}

