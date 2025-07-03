/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2013-2014, 2020-2021 by LWPU Corporation.  All rights reserved. 
 * All information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

// To facilitate faster compiles via pre-compiled headers, keep "stdtest.h" first.
#include "mdiag/tests/stdtest.h"

#include "fbpa_cattrip.h"
#include "mdiag/tests/test_state_report.h"

extern const ParamDecl fbpa_cattrip_params[] =
{
    SIMPLE_PARAM("-connection", "Test fbpa cattrip signal connection"),
    PARAM_SUBDECL(lwgpu_single_params),
    LAST_PARAM
};

STD_TEST_FACTORY(fbpa_cattrip, FbpaCattripTest);

FbpaCattripTest::FbpaCattripTest(ArgReader *params):
    LWGpuSingleChannelTest(params), errCnt(0)
{
    if(params->ParamPresent("-connection")) test_connection = true;
    else test_connection = false;

}

int FbpaCattripTest::Setup(void)
{
    lwgpu = LWGpuResource::FindFirstResource();

    m_arch = lwgpu->GetArchitecture();

    macros.MacroInit(m_arch);

    ch = lwgpu->CreateChannel();
    if(!ch)
    {
        ErrPrintf("FbpaCattripTest::Setup failed to create channel\n");
        return 0;
    }

    getStateReport()->init("FbpaCattrip");
    getStateReport()->enable();
    return 1;
}
void FbpaCattripTest::CleanUp(void)
{
    if(ch)
    {
        delete ch;
        ch = 0;
    }

    if (lwgpu)
    {
        DebugPrintf("FbpaCattripTest::CleanUp(): Releasing GPU.\n");
        lwgpu = 0;
    }
    LWGpuSingleChannelTest::CleanUp();
}

void FbpaCattripTest::Run(void)
{
    //  InfoPrintf Flag to determine whether the test fail because of elw issue or test issue.
    InfoPrintf("FLAG : FBPA CATTRIP Test starting ....\n");

    SetStatus(TEST_INCOMPLETE);
    if (macros.lw_chip_fb_pa_hbm_support) { // Only run test when this macro is defined.
        if (test_connection) errCnt += TestConnection();
    }else{
        InfoPrintf("Skip run test since related feature is not enabled in this chip\n");
    }

    if(errCnt)
    {
        SetStatus(TEST_FAILED);
        getStateReport()->runFailed("Failed test\n");
        InfoPrintf("FbpaCattripTest:: Test Failed with %d errors\n", errCnt);
    }
    else
    {
        SetStatus(TEST_SUCCEEDED);
        getStateReport()->crcCheckPassedGold();
        InfoPrintf("FbpaCattripTest:: Test Passed\n");
    }
    InfoPrintf("FBPA CATTRIP Test END.\n");
}

int FbpaCattripTest::TestConnection()
{
    UINT32 overt_status;
    int errCnt=0;

    // Ref          http://lwbugs/200033051/41
    InfoPrintf("Reg init values: 0x%08x, 0x%08x, 0x%08x, 0x%08x\n", \
            lwgpu->RegRd32(0x00908348), \
            lwgpu->RegRd32(0x0091c348), \
            lwgpu->RegRd32(0x00928348), \
            lwgpu->RegRd32(0x0093c348) \
            );
    // Force all cattrip signal to 0
    lwgpu->RegWr32(0x00908348, 0x00700000);
    lwgpu->RegWr32(0x0091c348, 0x00700000);
    lwgpu->RegWr32(0x00928348, 0x00700000);
    lwgpu->RegWr32(0x0093c348, 0x00700000);
    Platform::Delay(1);// 1us

    // Before asserting overt, turn off the path triggering split_rail_sd__clamp,
    // otherwise the test will kill itself.
    lwgpu->SetRegFldDef("LW_PPWR_PMU_SPLIT_RAIL", "_CARE_OVERT", "_NO");

    // Check overt=0
    Platform::EscapeRead("overt", 0, 1, &overt_status);
    InfoPrintf("Set 0000, EscapeRead of OVERT = 0x%0x\n", overt_status);
    if (overt_status != 1) // OVERT is active low
    {
        ErrPrintf("Overt is triggered when all fbpa cattrip signals are 0.");
        errCnt++;
    }

    // Force each cattrip signal to 1, one at a time
    UINT32 addr[4] = {0x00908348, 0x0091c348, 0x00928348, 0x0093c348};
    for (int i=0; i<4; i++) {
        InfoPrintf("i = %d\n", i);
        lwgpu->RegWr32(addr[i], 0x00700080);
        // Delay for a while
        Platform::Delay(1);// 1us
        Platform::EscapeRead("overt", 0, 1, &overt_status);
        InfoPrintf("EscapeRead of OVERT = 0x%0x\n", overt_status);
        // Check overt=1
        if (overt_status != 0)
        {
            ErrPrintf("Overt isn't triggered.\n");
            errCnt++;
        }
        lwgpu->RegWr32(addr[i], 0x00700000);
        Platform::Delay(1);// 1us
    }

    return errCnt;
}

