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

#include "check_dedicated_overt_gk110.h"
#include "mdiag/lwgpu.h"
#include "mdiag/utils/lwgpu_channel.h"
#include "mdiag/tests/test_state_report.h"
#include "core/include/gpu.h"

extern const ParamDecl check_dedicated_overt_gk110_params[] =
{
    SIMPLE_PARAM("-check_overt_pin",            "Testing the connection of dedicated overt pin"),
    PARAM_SUBDECL(lwgpu_single_params),
    LAST_PARAM
};

static int run_connect_overt_pin = 0;

OvertPin::OvertPin(ArgReader *params) :
    LWGpuSingleChannelTest(params)
{
    if  (params->ParamPresent("-check_overt_pin"))
        run_connect_overt_pin = 1;
}

OvertPin::~OvertPin(void)
{
}

STD_TEST_FACTORY(check_dedicated_overt_gk110, OvertPin)

INT32
OvertPin::Setup(void)
{
    lwgpu = LWGpuResource::FindFirstResource();

    ch = lwgpu->CreateChannel();
    if(!ch)
    {
        ErrPrintf("OvertPin: Setup failed to create channel\n");
        return (0);
    }

    m_regMap = lwgpu->GetRegisterMap();
    if(!m_regMap)
    {
        ErrPrintf("OvertPin: it can only be run on GPU's that support a "
            "register map!\n");
        return (0);
    }

    getStateReport()->init("check_dedicated_overt_gk110");
    getStateReport()->enable();

    return 1;
}

void
OvertPin::CleanUp(void)
{
    if(ch)
    {
        delete ch;
        ch = 0;
    }

    if (lwgpu)
    {
        DebugPrintf("OvertPin::CleanUp(): Releasing GPU.\n");
        lwgpu = 0;
    }
    LWGpuSingleChannelTest::CleanUp();
}

void
OvertPin::Run(void)
{

    //  InfoPrintf Flag to determine whether the test fail because of elw issue or test issue.
    InfoPrintf("FLAG : OvertPin test starting ....\n");
    SetStatus(TEST_INCOMPLETE);

    if (run_connect_overt_pin)
    {
        InfoPrintf("Starting checking connect of dedicated overt pin by int temp \n");
        if(checkOvertDedicatedPin())
        {
            SetStatus(TEST_FAILED);
            getStateReport()->runFailed(" Test of check_overt_pin failed \n");
            ErrPrintf("check_dedicated_overt :: check_overt_pin test failed \n");
            return;
        }
    }
    SetStatus(TEST_SUCCEEDED);
    getStateReport()->crcCheckPassedGold();
}

/*
 * Purpose of this test:
 *   Verify OVERT pin connection of both directions.
 */
UINT32
OvertPin::checkOvertDedicatedPin()
{
    UINT32 data;

    InfoPrintf("===== Test OVERT pad connection =====\n");

    InfoPrintf("Check initial state of overt\n");
    InfoPrintf("LW_THERM_OVERT_CTRL_EXT_OVERT_ASSERTED reflects overt input\n");
    lwgpu->GetRegFldNum("LW_THERM_OVERT_CTRL", "_EXT_OVERT_ASSERTED", &data);
    if (data != 0) {
        ErrPrintf("EXT_OVERT is asserted. OVERT_CTRL_EXT_OVERT_ASSERTED = %d\n", data);
        return (1);
    }

    InfoPrintf("opt_split_rail_overt_bypass_fuse is set in test arg to prevent test from suicide.\n");
    // Stop any systemmatic shutdown or split rail clamp
    // Set fuse opt_split_rail_overt_bypass_fuse to prevent clamp
    // It's set by loading a fuse image in test arg.

    InfoPrintf("Trigger OVERT output\n");
    if(lwgpu->SetRegFldDef("LW_THERM_OVERT_CTRL","_OTOB_ENABLE","_ON")) InfoPrintf("LW_THERM_OVERT_CTRL_OTOB_ENABLE = _ON.\n");
    if(lwgpu->SetRegFldDef("LW_THERM_OVERT_EN","_THERMAL_0","_ENABLED")) InfoPrintf("LW_THERM_OVERT_EN_THERMAL_0 = _ENABLED.\n");
    if(lwgpu->SetRegFldDef("LW_THERM_EVT_THERMAL_0","_MODE","_FORCED")) InfoPrintf("LW_THERM_EVT_THERMAL_0_MODE = _FORCED.\n");

    Platform::Delay(1);

    InfoPrintf("Check state of overt by checking loop-back value.\n");
    lwgpu->GetRegFldNum("LW_THERM_OVERT_CTRL", "_EXT_OVERT_ASSERTED", &data);
    if (data != 1) {
        ErrPrintf("EXT_OVERT isn't asserted. OVERT_CTRL_EXT_OVERT_ASSERTED = %d\n", data);
        return (1);
    }

    return(0);
}

