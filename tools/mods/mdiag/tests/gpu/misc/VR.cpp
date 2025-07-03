/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2015-2017, 2020-2021 by LWPU Corporation.  All rights reserved. 
 * All information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#include "mdiag/tests/stdtest.h"

#include "VR.h"
#include "mdiag/tests/test_state_report.h"

extern const ParamDecl duty_cycle_vr_params[] =
{
        SIMPLE_PARAM("-high", "Callwlate high period"),
        SIMPLE_PARAM("-low", "Callwlate low period"),
        PARAM_SUBDECL(lwgpu_single_params),
        LAST_PARAM
};

STD_TEST_FACTORY(duty_cycle_vr, VR_Test);

VR_Test::VR_Test(ArgReader *params):
    LWGpuSingleChannelTest(params), rate(0)
{
        if(params->ParamPresent("-high")) high = true;
        else high = false;

        if(params->ParamPresent("-low")) low = true;
        else low = false;
}

int VR_Test::Setup(void)
{
    lwgpu = LWGpuResource::FindFirstResource();

    m_arch = lwgpu->GetArchitecture();

    ch = lwgpu->CreateChannel();
    if(!ch)
    {
        ErrPrintf("VR_Test::Setup failed to create channel\n");
        return 0;
    }

    getStateReport()->init("VR_Test");
    getStateReport()->enable();
    return 1;
}

void VR_Test::CleanUp(void)
{
    if(ch)
    {
        delete ch;
        ch = 0;
    }

    if (lwgpu)
    {
        DebugPrintf("VR_Test::CleanUp(): Releasing GPU.\n");
        lwgpu = 0;
    }
    LWGpuSingleChannelTest::CleanUp();
}

void VR_Test::Run(void)
{

    //  InfoPrintf Flag to determine whether the test fail because of elw issue or test issue.
    InfoPrintf("FLAG : VR Test starting ....\n");

    SetStatus(TEST_INCOMPLETE);

    if (high) rate = hightest();
    if (low) rate = lowtest();

    if(rate)
    {
        SetStatus(TEST_SUCCEEDED);
        getStateReport()->crcCheckPassedGold();
        InfoPrintf("VR Test:: Test Passed and the rate is %f\n", rate);

    }
    else
    {
        SetStatus(TEST_FAILED);
        getStateReport()->runFailed("Failed test\n");
        InfoPrintf("VR Test:: Test Failed");

    }
    InfoPrintf("VR Test END.\n");
}

float VR_Test::hightest()
{
        UINT32 whole, vr;
        //UINT32 i;

        InfoPrintf("hightest begins\n");

        lwgpu->SetRegFldDef("LW_THERM_INTR_MONITOR_CTRL(0)","_SOURCE","_NONE");
        lwgpu->SetRegFldDef("LW_THERM_INTR_MONITOR_CTRL(0)","_POLARITY","_HIGH_ACTIVE");
        lwgpu->SetRegFldDef("LW_THERM_INTR_MONITOR_CTRL(0)","_TYPE","_LEVEL");
        lwgpu->SetRegFldDef("LW_THERM_INTR_MONITOR_CTRL(0)","_EN","_ENABLE");
        lwgpu->SetRegFldNum("LW_PMGR_GPIO_INPUT_CNTL_21","_PINNUM",0x15);
        lwgpu->SetRegFldDef("LW_THERM_INTR_MONITOR_CTRL(1)","_SOURCE","_VR");
        lwgpu->SetRegFldDef("LW_THERM_INTR_MONITOR_CTRL(1)","_POLARITY","_HIGH_ACTIVE");
        lwgpu->SetRegFldDef("LW_THERM_INTR_MONITOR_CTRL(1)","_TYPE","_LEVEL");
        lwgpu->SetRegFldDef("LW_THERM_INTR_MONITOR_CTRL(1)","_EN","_ENABLE");

        InfoPrintf("Write the Reg is done, begin to force VR\n");
//      for ( i = 0; i < 800; i++ )
//      {
//              InfoPrintf("This is the %dth loop\n", i);
////            Platform::EscapeWrite("pmgr.pmgr_gpio_if.Gpio[21]",0, 4, 0x1);
Platform::Delay(100);
InfoPrintf("Delay is done\n");
////            Platform::EscapeWrite("pmgr.pmgr_gpio_if.Gpio[21]",0, 4, 0x0);
////            Platform::Delay(1);
////    }
//
        lwgpu->GetRegFldNum("LW_THERM_INTR_MONITOR_STATE(0)","_VALUE",&whole);
        lwgpu->GetRegFldNum("LW_THERM_INTR_MONITOR_STATE(1)","_VALUE",&vr);

        InfoPrintf("The whole is %d and the vr is %d\n", whole, vr);
        return float( float(vr) / float(whole) );
}

float VR_Test::lowtest()
{
        UINT32 whole, vr;
        //UINT32 i;

        InfoPrintf("lowtest begins\n");

        lwgpu->SetRegFldDef("LW_THERM_INTR_MONITOR_CTRL(0)","_SOURCE","_NONE");
        lwgpu->SetRegFldDef("LW_THERM_INTR_MONITOR_CTRL(0)","_POLARITY","_LOW_ACTIVE");
        lwgpu->SetRegFldDef("LW_THERM_INTR_MONITOR_CTRL(0)","_TYPE","_LEVEL");
        lwgpu->SetRegFldDef("LW_THERM_INTR_MONITOR_CTRL(0)","_EN","_ENABLE");
        lwgpu->SetRegFldNum("LW_PMGR_GPIO_INPUT_CNTL_21","_PINNUM",0x15);
        lwgpu->SetRegFldDef("LW_THERM_INTR_MONITOR_CTRL(1)","_SOURCE","_VR");
        lwgpu->SetRegFldDef("LW_THERM_INTR_MONITOR_CTRL(1)","_POLARITY","_LOW_ACTIVE");
        lwgpu->SetRegFldDef("LW_THERM_INTR_MONITOR_CTRL(1)","_TYPE","_LEVEL");
        lwgpu->SetRegFldDef("LW_THERM_INTR_MONITOR_CTRL(1)","_EN","_ENABLE");

        InfoPrintf("Write the Reg is done, begin to force VR\n");
//      for ( i = 0; i < 800; i++ )
////    {
////            InfoPrintf("This is the %dth loop\n", i);
////            Platform::EscapeWrite("pmgr.pmgr_gpio_if.Gpio[21]",0, 4, 0x1);
Platform::Delay(100);
InfoPrintf("Delay is done\n");
////            Platform::EscapeWrite("pmgr.pmgr_gpio_if.Gpio[21]",0, 4, 0x0);
////            Platform::Delay(1);
//      }

        lwgpu->GetRegFldNum("LW_THERM_INTR_MONITOR_STATE(0)","_VALUE",&whole);
        lwgpu->GetRegFldNum("LW_THERM_INTR_MONITOR_STATE(1)","_VALUE",&vr);

        InfoPrintf("The whole is %d and the vr is %d\n", whole, vr);
        return float( float(vr) / float(whole) );
}
