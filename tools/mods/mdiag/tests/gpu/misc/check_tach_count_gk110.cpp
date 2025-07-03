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

#include "check_tach_count_gk110.h"
#include "mdiag/lwgpu.h"
#include "mdiag/utils/lwgpu_channel.h"
#include "mdiag/tests/test_state_report.h"
#include "core/include/gpu.h"

extern const ParamDecl check_tach_count_gk110_params[] =
{
    SIMPLE_PARAM("-check_tach_count",            "Testing the connection of tach count input"),
    PARAM_SUBDECL(lwgpu_single_params),
    LAST_PARAM
};

static int run_connect_tach_count = 0;

TachCount::TachCount(ArgReader *params) :
    LWGpuSingleChannelTest(params)
{
    if  (params->ParamPresent("-check_tach_count"))
        run_connect_tach_count = 1;
}

TachCount::~TachCount(void)
{
}

STD_TEST_FACTORY(check_tach_count_gk110, TachCount)

INT32
TachCount::Setup(void)
{
    lwgpu = LWGpuResource::FindFirstResource();

    ch = lwgpu->CreateChannel();
    if(!ch)
    {
        ErrPrintf("TachCount: Setup failed to create channel\n");
        return (0);
    }

    m_regMap = lwgpu->GetRegisterMap();
    if(!m_regMap)
    {
        ErrPrintf("TachCount: it can only be run on GPU's that support a "
            "register map!\n");
        return (0);
    }

    getStateReport()->init("check_tach_count_gk110");
    getStateReport()->enable();

    return 1;
}

void
TachCount::CleanUp(void)
{
    if(ch)
    {
        delete ch;
        ch = 0;
    }

    if (lwgpu)
    {
        DebugPrintf("TachCount::CleanUp(): Releasing GPU.\n");
        lwgpu = 0;
    }
    LWGpuSingleChannelTest::CleanUp();
}

void
TachCount::Run(void)
{
    InfoPrintf("FLAG : TachCount test starting ....\n");
    SetStatus(TEST_INCOMPLETE);

    if (run_connect_tach_count)
    {
        InfoPrintf("Starting checking connect of tach count \n");
        if(check_connect_tach_count())
        {
            SetStatus(TEST_FAILED);
            getStateReport()->runFailed(" Test of check_tach_count failed \n");
            ErrPrintf("check_tach_count :: check_tach_count test failed \n");
            return;
        }
    }
    SetStatus(TEST_SUCCEEDED);
    getStateReport()->crcCheckPassedGold();
}

UINT32
TachCount::check_connect_tach_count()
{
    UINT32 data;
    //resetRegisters();
    // tach_count_in ==> active low
    // Initialize pmgr2pwr_tach_count_in from pmgr
    Platform::Delay(2);
    Platform::EscapeWrite("tach_count_in_0", 0, 1, 1);
    Platform::EscapeWrite("tach_count_in_1", 0, 1, 1);
    // Check initialization of pmgr2pwr_tach_count_in from therm
    Platform::Delay(10);
    data = 0;
    Platform::EscapeRead("pwr_tach_count_in_0", 0, 1, &data);
    if (data != 1) 
    {
        ErrPrintf("1 : pwr_tach_count_in_0 should be 1, but now it's 0x%x \n", data);
        return (1);
    }
    data = 0;
    Platform::EscapeRead("pwr_tach_count_in_1", 0, 1, &data);
    if (data != 1) 
    {
        ErrPrintf("2 : pwr_tach_count_in_1 should be 1, but now it's 0x%x \n", data);
        return (1);
    }
    // Assert pmgr2pwr_tach_count_in from pmgr
    Platform::Delay(2);
    Platform::EscapeWrite("tach_count_in_0", 0, 1, 0);
    Platform::EscapeWrite("tach_count_in_1", 0, 1, 0);
    Platform::Delay(10);
    Platform::EscapeRead("pwr_tach_count_in_0", 0, 1, &data);
    if (data != 0) 
    {
        ErrPrintf("3 : pwr_tach_count_in_0 should be 0, but now it's 0x%x \n", data);
        return (1);
    }
    data=1;
    Platform::EscapeRead("pwr_tach_count_in_1", 0, 1, &data);
    if (data != 0) 
    {
        ErrPrintf("4 : pwr_tach_count_in_1 should be 0, but now it's 0x%x \n", data);
        return (1);
    }
    // De-assert pmgr2pwr_tach_count_in from pmgr
    Platform::Delay(2);
    return (0);  // calling routine will check m_ErrCount for CRC fail
}

