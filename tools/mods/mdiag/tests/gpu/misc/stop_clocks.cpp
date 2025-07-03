/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2011, 2020-2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

// To facilitate faster compiles via pre-compiled headers, keep "stdtest.h"
// first.
#include "mdiag/tests/stdtest.h"

#include "stop_clocks.h"
#include "gpio_engine.h"
#include "mdiag/lwgpu.h"
#include "mdiag/utils/lwgpu_channel.h"
#include "mdiag/tests/test_state_report.h"
#include "core/include/gpu.h"
#include "gpu/perf/pmusub.h"

// flags to indicate which test to run
static bool stop_clocks_intr = false;

extern const ParamDecl stop_clocks_params[] =
{
    SIMPLE_PARAM("-stop_clocks_intr", "Testing Stop Clocks wake on Interrupt"),
    PARAM_SUBDECL(lwgpu_single_params),
    LAST_PARAM
};

StopClocks::StopClocks(ArgReader *params) :
    LWGpuSingleChannelTest(params)
{
    if (params->ParamPresent("-stop_clocks_intr"))
    {
        stop_clocks_intr = true;
    }
}

StopClocks::~StopClocks(void)
{
}

STD_TEST_FACTORY(stop_clocks, StopClocks)

int
StopClocks::Setup(void)
{
    lwgpu = LWGpuResource::FindFirstResource();

    ch = lwgpu->CreateChannel();
    if(!ch)
    {
        ErrPrintf("StopClocks: Setup failed to create channel\n");
        return (0);
    }

    m_regMap = lwgpu->GetRegisterMap();
    if(!m_regMap)
    {
        ErrPrintf("StopClocks: stop_clocks can only be run on GPU's that "
            "support a register map!\n");
        return (0);
    }

    getStateReport()->init("stop_clocks");
    getStateReport()->enable();

    return 1;
}

void
StopClocks::CleanUp(void)
{
    if(ch)
    {
        delete ch;
        ch = 0;
    }

    if (lwgpu)
    {
        DebugPrintf("StopClocks::CleanUp(): Releasing GPU.\n");
        lwgpu = 0;
    }
    LWGpuSingleChannelTest::CleanUp();
}

void
StopClocks::Run(void)
{
    bool ran_test = false;
    //  InfoPrintf Flag to determine whether the test fail because of elw issue or test issue.
    InfoPrintf("FLAG : PMU/THERM Stop Clocks test starting ....\n");
    SetStatus(TEST_INCOMPLETE);
    m_ErrCount = 0;
    m_max_polling_attempts = 350;

    if (stop_clocks_intr)
    {
        if (ThermStopClocksIntrTest())
        {
            SetStatus(TEST_FAILED);
            getStateReport()->runFailed("Failed test\n");
            return;
        }
        ran_test = true;
    }

    if (!ran_test)
    {
        ErrPrintf("No PMU/THERM Stop Clocks test specified, no test ran\n");
        return;
    }

    InfoPrintf("StopClocks: Test completed with %d miscompares\n", m_ErrCount);
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

// This test exercises the wakeup on interrupt feature provided in Bug 839014.
// We put PEX into Deep L1 and turn off XCLK.  We use the clock monitor to
// verify that XCLK is off.  An interrupt is generated that wakes up XCLK.
// Again, we use the clock monitor to verify that XCLK is on.
UINT32
StopClocks::ThermStopClocksIntrTest()
{
    PMU *pPmu;
    const UINT32 clk_mon_interval_ns = 10000;

    InfoPrintf("StopClocks: Running stop_clocks_intr test\n");

    // Verify that Stop Clocks wakeup on Interrupt is supported.
    if (check_stop_clocks_intr_support())
    {
        ErrPrintf("StopClocks: Failure in checking stop clocks intr support\n");
        return (1);
    }

    InfoPrintf("StopClocks: Creating PMU Object\n");
    if (lwgpu->GetGpuSubdevice()->GetPmu(&pPmu) != OK)
    {
        ErrPrintf("StopClocks: Unable to get PMU Object\n");
        return (1);
    }
    InfoPrintf("StopClocks: Loading PMU ucode\n");

    /*
    Steps to generate stop_clocks_imem_content and stop_clocks_dmem_content
    - Step 1: Compile source code into binary
      In //hw/lwgpu_pwr/ip/power/pwr/1.0/stand_sim/private/cc_tests/pmu/fullchip/pmu_csb
      ~> make all

      This will generate two files:
      App_fullchip.0x00000000 (imem code)
      App_fullchip.0x10000000 (dmem data)

    - Step 2: Colwert to memory format and generate image header file
      In //hw/lwgpu_pwr/ip/power/pwr/1.0/stand_sim/private/cc_tests/pmu/fullchip/pmu_csb
      ~> //hw/lwgpu/tools/mods/trace_3d/plugins/ba_sampling/bin2mem.pl -i App_fullchip.0x00000000 -o App_fullchip.0x00000000.mem
      ~> //hw/lwgpu/tools/mods/trace_3d/plugins/ba_sampling/bin2mem.pl -i App_fullchip.0x10000000 -o App_fullchip.0x10000000.mem

    - Step 3: Colwert mem file into header file
      In //hw/lwgpu_pwr/ip/power/pwr/1.0/stand_sim/private/cc_tests/pmu/fullchip/pmu_csb
      ~> //hw/lwgpu/tools/mods/trace_3d/plugins/ba_sampling/gen_img.pl -aname stop_clocks_imem_content -tname stop_clocks_code -image App_fullchip.0x00000000.mem -elfbin App_fullchip.out
      ~> //hw/lwgpu/tools/mods/trace_3d/plugins/ba_sampling/gen_img.pl -aname stop_clocks_dmem_content -tname stop_clocks_data -image App_fullchip.0x10000000.mem -elfbin App_fullchip.out

      This will generate two files:
      code_img_stop_clocks_code.h
      code_img_stop_clocks_data.h

    - Step 4: Copy stop_clocks_imem_content/stop_clocks_dmem_content from code_img_stop_clocks_code.h/code_img_stop_clocks_data.h respectively
    */

    UINT32 stop_clocks_imem_content[]={
        0x0849f4bd, 0x9f9fff01, 0x010999e7, 0xfe0894b6, 0xb87e0094, 0x02f80005, 0x000000f8, 0x00000000,
        0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000,
        0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000,
        0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000,
        0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000,
        0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000,
        0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000,
        0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000,
        0xd4bd2c0e, 0xbccfedff, 0x9abc90aa, 0x059f9490, 0xbc909fbc, 0xc9bc909a, 0xffedffb0, 0x0001273e,
        0xb29fedff, 0xa69fb2fc, 0x0818f4fb, 0x0cf4fca6, 0xbd00f8f2, 0x04544f94, 0xfffff9ff, 0x5849fff9,
        0x009ffa04, 0x00f8a4bd, 0x544fe4bd, 0xfffeff04, 0xff045849, 0xf9fa9f9e, 0xf8a4bd00, 0x49f4bd00,
        0x9fff0454, 0xffffdf9f, 0xa4b603ff, 0x049ffd1b, 0xfd07a04f, 0xf9fa059a, 0x00f18f00, 0x07ac4901,
        0x4d009ffa, 0xf4bd07ac, 0xe49fdfff, 0xb370009e, 0xf91000ea, 0xbd07a449, 0x9f9ffff4, 0xfa04584f,
        0xe4b300f9, 0xa4bd0800, 0xe6b100f8, 0xfcf04000, 0xbc02090b, 0x00f8a29f, 0x5449f4bd, 0x9f9fff04,
        0xffffffdf, 0x1ba4b603, 0x4f049ffd, 0x9afd07a0, 0x00f9fa05, 0x5849f4bd, 0x9f9fff04, 0xfa07a44f,
        0xf28f00f9, 0xac490100, 0x009ffa07, 0xbd07ac4e, 0x9feffff4, 0x700094f1, 0x10009ab3, 0x0094b3f9,
        0xf8a4bd08, 0x0096b100, 0x0bfcf040, 0x9fbc0209, 0xbd00f8a2, 0x07c44a94, 0xb6afa9ff, 0x00f81fa5,
        0x004a94bd, 0xafa9ff05, 0xf81caac7, 0x02a08900, 0xfff4bd02, 0x00deff9f, 0xfd800000, 0x9ffa05fe,
        0xbd00f800, 0x02a08a94, 0xafa9ff02, 0xf81fa5b6, 0x027c8900, 0xfff4bd02, 0xf30eff9f, 0xfa04fefd,
        0x00f8009f, 0xbd05f04e, 0x9fe9ff94, 0xffff00df, 0xffa4f000, 0xfd049ffd, 0xe9fa059a, 0xbd00f800,
        0x01404ab4, 0x000a4b7e, 0x20000089, 0xa9ffc4bd, 0x01404ab5, 0x000a037e, 0xaab800f8, 0xf9003584,
        0x94b0b212, 0x010b02a1, 0x4b7e1ab2, 0xff49000a, 0x0104f0ef, 0xb3b4a9ff, 0xf1080000, 0xb21000b5,
        0x7e010c1a, 0x0b000a03, 0xd6048a01, 0x7ebcb200, 0xfb000a03, 0xb222f911, 0x07c441a2, 0x010a04bd,
        0x0001007e, 0xb69f10ff, 0x0bf41f95, 0x027f7ef4, 0x02a08900, 0xfff4bd02, 0x00deff9f, 0xfd800000,
        0x9ffa05fe, 0x7e010a00, 0xb2000100, 0x81010b2a, 0x7e0202a0, 0xbd00029a, 0x7e010a04, 0xff000100,
        0x95b69f10, 0x0190b31f, 0x027c89f4, 0xfff4bd02, 0xf30eff9f, 0xfa04fefd, 0x0041009f, 0x0a04bd05,
        0x01007e01, 0x9f10ff00, 0xb31c99c7, 0x0af40190, 0x01007e01, 0xbd2ab200, 0x029a7eb4, 0xfba4bd00,
        0x0f32f921, 0x042849ff, 0x43009ffa, 0x04bd05f0, 0x41045442, 0x30ff0458, 0x1895b69f, 0x00839ab3,
        0x8396b176, 0x250cf400, 0x00809ab3, 0x8096b148, 0x0b0cf400, 0x320090b3, 0x00041e3e, 0x00819ab3,
        0x829eb341, 0xec3e7d00, 0x9ab30003, 0x0a560085, 0x8596b101, 0x4308f400, 0x00909ab3, 0xff9eb352,
        0x143e6100, 0x010a0004, 0x0001007e, 0x0003763e, 0xff9f20ff, 0x19fa9f90, 0x03e63e00, 0xff20ff00,
        0xfa9f10ff, 0xa4bd00f9, 0x0004203e, 0xf83ea4bd, 0xa4bd0003, 0x0004023e, 0x00015d7e, 0x0004203e,
        0xb87e010a, 0x203e0001, 0x040a0004, 0x0002d57e, 0x0004203e, 0x647ea4bd, 0x02f80002, 0x010a31fb,
        0x0002647e, 0x0003763e, 0xa0f990f9, 0xc0f9b0f9, 0xe0f9d0f9, 0x8afef0f9, 0x89a0f901, 0xbf0002a0,
        0xfca5f99a, 0x00a8fea0, 0xe0fcf0fc, 0xc0fcd0fc, 0xa0fcb0fc, 0x01f890fc, 0x1f8900f8, 0x93fe0005,
        0x04f18900, 0x0091fe00, 0x00042889, 0x8f0090fe, 0x09080000, 0x009ffa1c, 0x0908084f, 0x009ffa10,
        0xf41131f4, 0x00f81031, 0x0005558a, 0x00054d7e, 0x00051f89, 0x890093fe, 0xfe0004f1, 0x28890091,
        0x90fe0004, 0x00008f00, 0xfa1c0908, 0xfb4f009f, 0xfa100948, 0xff0f009f, 0xfa04c449, 0xd449009f,
        0x009ffa07, 0xfa07d849, 0x00df009f, 0x49100000, 0x9ffa0684, 0x49ff0f00, 0x9ffa0760, 0x07644900,
        0x49009ffa, 0x9ffa0770, 0x1131f400, 0xf81031f4, 0xf990f900, 0xf9b0f9a0, 0xf9d0f9c0, 0xfef0f9e0,
        0xa0f9018a, 0x00056b7e, 0xa8fea0fc, 0xfcf0fc00, 0xfcd0fce0, 0xfcb0fcc0, 0xf890fca0, 0xf900f801,
        0xf9a0f990, 0xf9c0f9b0, 0xf9e0f9d0, 0x018afef0, 0xa27ea0f9, 0xa0fc0005, 0xfc00a8fe, 0xfce0fcf0,
        0xfcc0fcd0, 0xfca0fcb0, 0xf801f890, 0x02a08900, 0xf89aa000, 0x09f4bd00, 0x9f9fff08, 0xf9fa040f,
        0x00904f01, 0xbd01f9fa, 0xbd00f8a4, 0xff080f94, 0xbfffbff9, 0x1d0bf494, 0x5809020f, 0x0f009ffa,
        0xfa040908, 0x008f019f, 0x90490100, 0x019ffa00, 0xa48a00f8, 0xc4bd0000, 0xe4bdd4bd, 0x0005f87e,
        0x32f400f8, 0x01cbfe18, 0x0000c88a, 0xd4bdc4bd, 0xf87ee4bd, 0x00f80005, 0x00073e7e, 0x0004887e,
        0x00016089, 0x89019a18, 0x98000140, 0x008c019b, 0xa4f00080, 0x08b5b6ff, 0x0005e47e, 0x0003617e,
        0x00f8a4bd, 0x00017089, 0x74899aa0, 0x9ba00001, 0x00017889, 0x00f89ca0, 0xa1b242f9, 0x890183fe,
        0xfffcffff, 0x98fe9439, 0x01b2fe00, 0x00017089, 0xff499fbf, 0x9429ff8f, 0xfd0cf4b6, 0x9bfe059f,
        0x0174fe00, 0x00017489, 0x97fe99bf, 0x8a03f800, 0xbf00017c, 0x01008fa0, 0x04feb500, 0x9001fbb5,
        0xfcb50109, 0x03fdb502, 0xf0a0a9a0, 0x008fe4bd, 0xefbc0001, 0xf81e3c90, 0x3501ee90, 0xe0b3149f,
        0x1e3c0b2b, 0x00943398, 0x89f43de9, 0x8d000114, 0x3c000180, 0x008fe99f, 0xdebf0400, 0x00010089,
        0xfa059ffd, 0xd9bf06e9, 0x0001788f, 0x9990ffbf, 0xa6d9a040, 0x0708f49f, 0xd9a094bd, 0xfe0047fe,
        0x38fe002b, 0x4941fb00, 0xf43d2400, 0x99909f20, 0x409eb301, 0xe4bdfb24, 0x4d08204c, 0x400b2000,
        0xbc00cdfa, 0x94b690ee, 0x0499b802, 0xef900024, 0x019d7501, 0xf9949b60, 0x0099b803, 0x9e200024,
        0xb204cc90, 0x40dd90fe, 0xb324004f, 0x7dd504e4, 0x12fb7594, 0x7517f975, 0xf93516f9, 0x13ff7528,
        0x4f04c849, 0x9ffa2430, 0x24004f00, 0xfa04cc49, 0x400f009f, 0x9ffa1c09, 0xfa94bd01, 0x00f8009f,
        0xbdfc30f4, 0x014ffe94, 0xa0014efe, 0x90efbff9, 0xe9a001f9, 0x1ef4faa6, 0x0430f4f7, 0x908f00f8,
        0x94bd0001, 0x948ff9a0, 0xf9a00001, 0x03f800f8, 0x97fe94bd, 0x01b9fe00, 0xf080004f, 0x9ffd07c4,
        0x0cc4b604, 0xfe059cfd, 0xb4b6009b, 0x05bafd10, 0xf806dbfa, 0x01948900, 0xf99fbf00, 0xbd97b272,
        0x90aabc04, 0xb602f195, 0xa4b60194, 0x03949002, 0x9001a390, 0x90860112, 0x00850001, 0xc63e0000,
        0x54bc0007, 0xbc6dbf98, 0x0a94c853, 0xb6020b04, 0xd9bc08d4, 0x01a089d0, 0xd0dabc00, 0xbc010090,
        0x4e7ea0a9, 0x79bf0007, 0x94f01fb2, 0x050bf403, 0x908e2fb2, 0x0fa60001, 0xbfca1ef4, 0x019990e9,
        0x71fbe9a0, 0x00019489, 0x42f99fbf, 0x04bd94b2, 0x8302f195, 0x90000190, 0x223e0112, 0x3dbf0008,
        0x01000089, 0x0b040a94, 0x08d4b602, 0x89d0d9bc, 0xbc0001a0, 0x0090d0da, 0xa0a9bc01, 0x4e7ebcb2,
        0x49bf0007, 0x94f01fb2, 0x050bf403, 0x908e2fb2, 0x0fa60001, 0xbfca1ef4, 0x019990e9, 0x41fbe9a0,
        0x948002f9, 0x09bf0001, 0x0001a08e, 0xb6019f90, 0x0fa00294, 0xa0909ebc, 0x40f4b39a, 0x7ebab20e,
        0xbd0007e4, 0xfb09a094, 0x8002f901, 0xbf000194, 0x01a08e09, 0x019f9000, 0xa00294b6, 0x909ebc0f,
        0xf4b39aa0, 0xe47e0c40, 0x94bd0007, 0x01fb09a0, 0x0900804f, 0x009ffa10, 0xadcabadf, 0xfa40090b,
        0x31f4009f, 0x0028f400, 0x00000089, 0x00000c8f, 0xe4bd9dbf, 0x3eff004c, 0xbf0008c8, 0x01ee90f9,
        0xa0049cfd, 0x10ff90f9, 0x1ef4eda6, 0x0f00f8f1, 0xff94bd2c, 0xf9ffdff9, 0xe29dbc9f, 0xaabcfbb2,
        0x909abc90, 0xbd059f94, 0x909fbcc4, 0x3ef09abc, 0xff0008f9, 0x9dbc9fbc, 0xf4efa6e2, 0x00f8f808,
        0xff0004da, 0xbd010b03, 0x0a037ec4, 0x0000da00, 0x804b03ff, 0x7ec4bd00, 0xf8000a03, 0xbdabb200,
        0x00a8dac4, 0x517e03ff, 0x00f80009, 0x0000e08a, 0xc4bdb4bd, 0xe4bdd4bd, 0x0005f87e, 0x2049f4bd,
        0x019ffa04, 0x1409010f, 0xf89f9fff, 0xf802f803, 0x01c4f000, 0xb607a049, 0xcafd1bc4, 0x009cfa05,
        0xfa07a449, 0xf28f009b, 0xac490100, 0x009ffa07, 0xffd900f8, 0xfd03ffff, 0x999004a9, 0x01c4f001,
        0xb605a9fd, 0xa0491bc4, 0x05acfd07, 0x49009afa, 0x9bfa07a4, 0x00f28f00, 0x07ac4901, 0xf8009ffa,
        0xffffd900, 0xa9fd03ff, 0x09517e04, 0x07ac4e00, 0xeffff4bd, 0x0094f19f, 0x009ab370, 0x9eb30a20,
        0xf8094000, 0xb300f80b, 0xeb10009a, 0xffd900f8, 0xff03ffff, 0x00da94a9, 0xff040000, 0x517ea59a,
        0xac4e0009, 0xfff4bd07, 0x94f19fef, 0x9ab37000, 0xb30a2000, 0x0940009e, 0x00f80bf8, 0x10009ab3,
        0x7e00f8eb, 0x4e000951, 0xf4bd07ac, 0xf19fefff, 0xb3700094, 0x0a20009a, 0x40009eb3, 0xf80bf809,
        0x009ab300, 0x00f8eb10, 0xffffffd9, 0x01b4f003, 0xb604a9fd, 0xa0491bb4, 0x05bafd07, 0x8f009bfa,
        0x490100f1, 0x9ffa07ac, 0x7e00f800, 0x4e000a28, 0xf4bd07ac, 0xf19fefff, 0xb3700094, 0x0a20009a,
        0x40009eb3, 0x3e0bf80b, 0xb3000a70, 0xe910009a, 0xa44a94bd, 0xafa9ff07, 0x287e00f8, 0xac4e000a,
        0xfff4bd07, 0x94f19fef, 0x9ab37000, 0xb30a2000, 0x1040009e, 0x9ab30bf8, 0x3e124000, 0xb3000acb,
        0xe410009a, 0x000acb3e, 0xb049e4bd, 0x9f9eff07, 0x000000df, 0xa49fff80, 0x490b0bf4, 0x9eff0988,
        0x4900f8af, 0x9aff07a4, 0xbd00f8af, 0x07a449f4, 0xf8af9fff, 0x0000d900, 0xb4f01000, 0x05a9fd01,
        0xfd1bb4b6, 0xaabf05ab, 0xc4f000f8, 0x0000d901, 0xc4b61000, 0x05a9fd1b, 0xa005acfd, 0xf000f8ab,
        0x00d901c4, 0xb6100000, 0xa9fd1bc4, 0x05acfd05, 0x00f8aba0, 0xd901c4f0, 0x14000000, 0xfd1bc4b6,
        0xacfd05a9, 0xf8aba005, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000,
        0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000,
        0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000,
        0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000,
        0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000,
        0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000,
        0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000};

    UINT32 stop_clocks_dmem_content[]={
        0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000,
        0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000,
        0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000,
        0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000,
        0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000,
        0x00000000, 0x4f525245, 0x55203a52, 0x6f6e6b6e, 0x49206e77, 0x3a315152, 0x61747320, 0x203d2074,
        0x30257830, 0x000a7838, 0x65637845, 0x6f697470, 0x65203a6e, 0x3d696378, 0x30257830, 0x000a7838,
        0x54534554, 0x444e455f, 0x0000000a, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000,
        0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000,
        0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000,
        0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000,
        0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000,
        0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000,
        0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000,
        0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000,
        0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000,
        0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000,
        0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000,
        0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000,
        0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000,
        0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000,
        0x00000000, 0x00000000, 0x00000000};

    // configure DMACTL to disable CTX
    lwgpu->RegWr32(0x0010a10c, 0x0); // LW_PPWR_FALCON_DMACTL (0x0010a10c)

    // loading IMEM/DMEM through PRIV
    UINT32 code_size = sizeof(stop_clocks_imem_content)/sizeof(UINT32)*4; // in bytes
    UINT32 data_size = sizeof(stop_clocks_dmem_content)/sizeof(UINT32)*4; // in bytes
    UINT32 blk = 0;
    UINT32 doff = 0;
    UINT32 size = 0;
    UINT32 data = 0;
    while(code_size > 0){
        UINT32 i = 0;
        doff = 0x100 * blk;
        size = (code_size < 256) ? code_size : 256;
        if(code_size < 256) {
            size = code_size;
            code_size = 0;
        } else {
            size = 256;
            code_size -= 256;
        }
        lwgpu->RegWr32(0x0010a188, blk);  // LW_PPWR_FALCON_IMEMT(0) 0x0010a188
        data = (0x3 << 24) + ( blk << 8); // AINCW[24]=1 AINCR[25]=1 BLK[15:8]=blk
        lwgpu->RegWr32(0x0010a180, data); // LW_PPWR_FALCON_IMEMC(0) 0x0010a180
        while(i < size){
            lwgpu->RegWr32(0x0010a184, stop_clocks_imem_content[(doff+i)/4]); // LW_PPWR_FALCON_IMEMD(0) 0x0010a184
            i += 4; // Byte loaded
        }
        blk++;
    }

    blk = 0;
    while(data_size > 0){
        UINT32 i = 0;
        doff = 0x100 * blk;
        size = (data_size < 256) ? data_size : 256;
        if(data_size < 256) {
            size = data_size;
            data_size = 0;
        } else {
            size = 256;
            data_size -= 256;
        }
        data = (0x3 << 24) + (blk << 8);  // AINCW[24]=1 AINCR[25]=1 BLK[15:8]=blk
        lwgpu->RegWr32(0x0010a1c0, data); // LW_PPWR_FALCON_DMEMC(0) 0x0010a1c0
        while(i < size){
            lwgpu->RegWr32(0x0010a1c4, stop_clocks_dmem_content[(doff+i)/4]); // LW_PPWR_FALCON_DMEMD(0) 0x0010a1c4
            i += 4; // Byte loaded
        }
        blk++;
    }

    // Start CPU
    lwgpu->RegWr32(0x0010a100, 0x2); // LW_PPWR_FALCON_CPUCTL 0x0010a100

    // Setup GPIO for reporting sleep status.  PMU updates the GPIO to tell
    // this test when clocks are stopped.  This tells the test when to start
    // the clock monitor and when it can generate the wakeup interrupt.
    if (setup_sleep_status(SC_GPIO_SLEEP))
    {
        ErrPrintf("StopClocks: Failure in setting up GPIO for sleep status\n");
        return (1);
    }

    // Setup interrupt path from GPIO to XVE.  The test generates the wakeup
    // interrupt after the clock monitor is done.  This interrupt has to
    // propagate from the GPIO to the sleep controller.
    if (setup_intr(SC_GPIO_INTR))
    {
        ErrPrintf("StopClocks: Failure in setting up interrupt path\n");
        return (1);
    }

    // Setup XCLK for stopping.  Switch clocks/PLLs to prepare for stopping
    // XCLK.
    if (setup_xclk_for_stopping())
    {
        ErrPrintf("StopClocks: Failure in setting up XCLK for stopping\n");
        return (1);
    }

    // Check clock frequency.  We should be at XTAL4X prior to Deep L1.
    if (check_xclk_freq(clk_mon_interval_ns, 108000))
    {
        ErrPrintf("StopClocks: Failure in checking xclk frequency\n");
        return (1);
    }

    InfoPrintf("StopClocks: Testing wake-up interrupt through host2xve_intr_nonstall_\n");

    // Send PMU sleep command and the loop index.  The loop index is used
    // by PMU ucode to route the GPIO interrupt through HOST and onto the
    // appropriate interrupt line to the sleep controller.
    // Note: Only one interrupt remains - host2xve_intr_nostall_.
    if (set_pmu_command(SC_STOP_CLOCK, 0))
    {
        ErrPrintf("StopClocks: Failure in setting PMU command\n");
        return (1);
    }

    // Enter Deep L1
    if (enable_deep_l1())
    {
        ErrPrintf("StopClocks: Failure in enabling deep L1\n");
        return (1);
    }

    // Wait until PMU signals that we are really in Deep L1 Sleep mode
    InfoPrintf("StopClocks: Waiting for GPU to enter Deep L1 Sleep\n");
    if (poll_gpio_state(SC_GPIO_SLEEP, 1))
    {
        ErrPrintf("StopClocks: Failure in polling GPIO state\n");
        return (1);
    }

    // Check clock frequency.  xclk should be stopped.
    if (check_xclk_freq(clk_mon_interval_ns, 0))
    {
        ErrPrintf("StopClocks: Failure in checking xclk frequency\n");
        return (1);
    }

    // Generate wake-up interrupt by toggling GPIO
    InfoPrintf("StopClocks: Generating wake-up interrupt\n");
    if (set_gpio_state(SC_GPIO_INTR, 1))
    {
        ErrPrintf("StopClocks: Failure in setting GPIO state\n");
        return (1);
    }
    Platform::Delay(1);
    if (set_gpio_state(SC_GPIO_INTR, 0))
    {
        ErrPrintf("StopClocks: Failure in setting GPIO state\n");
        return (1);
    }

    // Check clock frequency.  xclk should restart immediately and be XTAL4X
    if (check_xclk_freq(clk_mon_interval_ns, 108000))
    {
        ErrPrintf("StopClocks: Failure in checking xclk frequency\n");
        return (1);
    }

    // Wait until PMU is done with Deep L1 Sleep and has signalled that
    // PEXPLL is up and running before we do any priv transactions.
    InfoPrintf("StopClocks: Waiting for PEXPLL to wake-up completely\n");
    if (poll_gpio_state(SC_GPIO_SLEEP, 0))
    {
        ErrPrintf("StopClocks: Failure in polling GPIO state\n");
        return (1);
    }

    // Exit Deep L1
    if (exit_deep_l1())
    {
        ErrPrintf("StopClocks: Failure in exiting deep L1\n");
        return (1);
    }

    // check PMU status from SC_STOP_CLOCK routine
    if (poll_for_pmu_done())
    {
        ErrPrintf("StopClocks: Failure in waiting for PMU command done\n");
        return (1);
    }

    // check wake-up reason
    if (check_sleep_status("INTERRUPT"))
    {
        ErrPrintf("StopClocks: Failure in checking sleep status\n");
        return (1);
    }

    // Check and clean up interrupt.  We examine which host interupt
    // caused the wakeup.  This is to verify that the test achieved the
    // intended coverage of testing both interrupt signals.
    if (check_host_intr(true))
    {
        ErrPrintf("StopClocks: Failure in checking HOST interrupt\n");
        return (1);
    }
    if (clear_gpio_intr(SC_GPIO_INTR))
    {
        ErrPrintf("StopClocks: Failure in clearing GPIO interrupt\n");
        return (1);
    }

    // Send PMU halt command.  param1 is a dont care for the halt command.
    if (set_pmu_command(SC_PMU_HALT, 0))
    {
        ErrPrintf("StopClocks: Failure in setting PMU command\n");
        return (1);
    }
    if (poll_for_pmu_done())
    {
        ErrPrintf("StopClocks: Failure in waiting for PMU command done\n");
        return (1);
    }

    return (0);
}

// This routine verifies whether the Stop Clocks wakeup on Interrupt feature
// is supported.  If LW_THERM_SLEEP_CTRL_0_WAKE_MASK_INTERRUPT exists, then
// the feature is supported.
INT32
StopClocks::check_stop_clocks_intr_support()
{
    unique_ptr<IRegisterClass> reg;
    unique_ptr<IRegisterField> field;
    const char *regname, *fieldname;

    // Verify LW_THERM_SLEEP_CTRL_0_WAKE_MASK_INTERRUPT field exists.
    regname = "LW_THERM_SLEEP_CTRL_0";
    if (!(reg = m_regMap->FindRegister(regname)))
    {
        ErrPrintf("StopClocks: Failed to find %s register.  Stop Clocks "
            "wakeup on Interrupt is not supported.\n", regname);
        return (1);
    }

    fieldname = "LW_THERM_SLEEP_CTRL_0_WAKE_MASK_INTERRUPT";
    if (!(field = reg->FindField(fieldname)))
    {
        ErrPrintf("StopClocks: Failed to find %s field.  Stop Clocks "
            "wakeup on Interrupt is not supported.\n", fieldname);
        return (1);
    }

    InfoPrintf("StopClocks: Stop Clocks wakeup on Interrupt is supported\n");

    return (0);
}

// This routine configures the indicated GPIO for the PMU to report status
// to this test.
INT32
StopClocks::setup_sleep_status(UINT32 gpio)
{
    InfoPrintf("StopClocks: Setting up DeepL1 Sleep status on GPIO %d\n", gpio);

    // Disable GPIO interrupts before using the GPIO for Deep L1 sleep status
    if (mask_gpio_intr(gpio))
    {
        ErrPrintf("StopClocks: Failure in masking GPIO interrupts\n");
        return (1);
    }
    if (config_gpio_output(gpio))
    {
        ErrPrintf("StopClocks: Failure in configuring GPIO as output\n");
        return (1);
    }

    // Trigger GPIO configuration so that it will take effect.
    if (trigger_gpio_update())
    {
        ErrPrintf("StopClocks: Failure in triggering GPIO update\n");
        return (1);
    }

    return (0);
}

// This routine configures the indicated GPIO to generate a GPIO_RISING
// interrupt from PMGR to HOST.
// Host sends interrupt to XVE.
// At the XVE, the interrupt is masked since
// we do not want RM to handle the interrupt, nor do we want the interrupt
// to cause us to exit Deep L1.
INT32
StopClocks::setup_intr(UINT32 gpio)
{
    unique_ptr<IRegisterClass> reg;
    unique_ptr<IRegisterField> field;
    unique_ptr<IRegisterValue> value;
    char regname[127], fieldname[127], valuename[127];
    UINT32 reg_addr, reg_data;
    UINT32 list, index;
    const char *types[] = {"MSK", "EN"};
    const UINT32 num_types = sizeof(types)/sizeof(char*);

    InfoPrintf("StopClocks: Setting up interrupt from GPIO %d to XVE\n", gpio);

    // First, mask all interrupts, then we can enable just the RM interrupt.
    if (mask_gpio_intr(gpio))
    {
        ErrPrintf("StopClocks: Failure in masking GPIO interrupts\n");
        return (1);
    }

    // Second, configure as an input and trigger update
    if (config_gpio_input(gpio))
    {
        ErrPrintf("StopClocks: Failure in setting GPIO state\n");
        return (1);
    }
    if (trigger_gpio_update())
    {
        ErrPrintf("StopClocks: Failure in triggering GPIO update\n");
        return (1);
    }

    // Third, clear any pending GPIO interrupt.
    if (clear_gpio_intr(gpio))
    {
        ErrPrintf("StopClocks: Failure in clearing GPIO interrupt\n");
        return (1);
    }

    // Fourth, configure the GPIO rising to interrupt HOST.
    // We need to enable both the ENABLE and MASK registers.  The MASK allows
    // the status to set, while the ENABLE allows the status to propagate to
    // HOST.

    // There are 16 GPIOs per register, but there are a total of 32 GPIOs.
    // The list ID determines which of the two registers holds the specified
    // gpio.
    list = (gpio >> 4) + 1;
    for (index = 0; index < num_types; index++)
    {
        // Enable the RM interrupt mask.  The PMU and DPU interrupt masks will
        // be automatically disabled when we enable the RM mask.
        if (sprintf(regname, "LW_PMGR_RM_INTR_%s_GPIO_LIST_%d", types[index],
            list) < 0)
        {
            ErrPrintf("StopClocks: Failed to generate GPIO mask regname\n");
            return (1);
        }
        if (!(reg = m_regMap->FindRegister(regname)))
        {
            ErrPrintf("StopClocks: Failed to find %s register\n", regname);
            return (1);
        }
        reg_addr = reg->GetAddress();
        reg_data = lwgpu->RegRd32(reg_addr);
        InfoPrintf("StopClocks: Reading %s = 0x%08x\n",reg->GetName(),reg_data);

        // Enable GPIO rising interrupt mask
        if (sprintf(fieldname, "%s_GPIO%d_RISING", reg->GetName(), gpio) < 0)
        {
            ErrPrintf("StopClocks: Failed to generate GPIO mask fieldname\n");
            return (1);
        }
        if (!(field = reg->FindField(fieldname)))
        {
            ErrPrintf("StopClocks: Failed to find %s field\n", fieldname);
            return (1);
        }
        if (sprintf(valuename, "%s_ENABLED", field->GetName()) < 0)
        {
            ErrPrintf("StopClocks: Failed to generate GPIO mask "
                "valuename\n");
            return (1);
        }
        if (!(value = field->FindValue(valuename)))
        {
            ErrPrintf("StopClocks: Failed to find %s value\n", valuename);
            return (1);
        }
        reg_data = (reg_data & ~field->GetMask()) |
                   (value->GetValue() << field->GetStartBit());

        // Write configuration back to register
        InfoPrintf("StopClocks: Writing %s = 0x%08x\n", reg->GetName(),
            reg_data);
        lwgpu->RegWr32(reg_addr, reg_data);
    }

    // Fifth, disable interrupt as it gets to XVE.  We do not want XVE
    // to forward the interrupt to RM, nor do we want the interrupt to cause
    // us to exit Deep L1.
    strcpy(regname, "LW_XVE_MSI_CTRL");
    if (!(reg = m_regMap->FindRegister(regname)))
    {
        ErrPrintf("StopClocks: Failed to find %s register\n", regname);
        return (1);
    }
    reg_addr = reg->GetAddress() + LW_PCFG_OFFSET;
    reg_data = lwgpu->RegRd32(reg_addr);
    InfoPrintf("StopClocks: Reading %s = 0x%08x\n", reg->GetName(), reg_data);
    strcpy(fieldname, "LW_XVE_MSI_CTRL_MSI");
    if (!(field = reg->FindField(fieldname)))
    {
        ErrPrintf("StopClocks: Failed to find %s field\n", fieldname);
        return (1);
    }
    strcpy(valuename, "LW_XVE_MSI_CTRL_MSI_DISABLE");
    if (!(value = field->FindValue(valuename)))
    {
        ErrPrintf("StopClocks: Failed to find %s value\n", valuename);
        return (1);
    }
    reg_data = (reg_data & ~field->GetMask()) |
               (value->GetValue() << field->GetStartBit());
    InfoPrintf("StopClocks: Writing %s = 0x%08x\n", reg->GetName(), reg_data);
    lwgpu->RegWr32(reg_addr, reg_data);

    strcpy(regname, "LW_XVE_DEV_CTRL");
    if (!(reg = m_regMap->FindRegister(regname)))
    {
        ErrPrintf("StopClocks: Failed to find %s register\n", regname);
        return (1);
    }
    reg_addr = reg->GetAddress() + LW_PCFG_OFFSET;
    reg_data = lwgpu->RegRd32(reg_addr);
    InfoPrintf("StopClocks: Reading %s = 0x%08x\n", reg->GetName(), reg_data);
    strcpy(fieldname, "LW_XVE_DEV_CTRL_CMD_INTERRUPT_DISABLE");
    if (!(field = reg->FindField(fieldname)))
    {
        ErrPrintf("StopClocks: Failed to find %s field\n", fieldname);
        return (1);
    }
    reg_data = (reg_data & ~field->GetMask()) |
               ((1 << field->GetStartBit()) & field->GetMask());
    InfoPrintf("StopClocks: Writing %s = 0x%08x\n", reg->GetName(), reg_data);
    lwgpu->RegWr32(reg_addr, reg_data);

    return (0);
}

// This routine mask the GPIO from generating any interrupts to RM, PMU, or
// DPU.  This allows us to safely use the GPIO without any unintended side
// effects.
INT32
StopClocks::mask_gpio_intr(UINT32 gpio)
{
    unique_ptr<IRegisterClass> reg;
    unique_ptr<IRegisterField> field;
    unique_ptr<IRegisterValue> value;
    char regname[127], fieldname[127], valuename[127];
    UINT32 reg_addr, reg_data;
    UINT32 list, index_target, index_edge;
    const char *targets[] = {"RM", "PMU", "GSP"};
    const char *edges[] = {"RISING", "FALLING"};
    const UINT32 num_targets = sizeof(targets)/sizeof(char*);
    const UINT32 num_edges = sizeof(edges)/sizeof(char*);

    InfoPrintf("StopClocks: Masking interrupts on GPIO %d\n", gpio);

    // GPIOs RISING and FALLING masks are kept in the same 32-bit register.
    // Therefore, a single register supports at most 16 GPIOs.  Since there
    // are up to 32 GPIOs, they are broken into 2 lists.  The list ID is the
    // GPIO index modulo 16.  Note that the list ID starts counting from 1,
    // not 0.
    list = (gpio >> 4) + 1;

    // There are 3 targets that can be interrupted by a GPIO.  We need to
    // mask the interrupt to all targets.
    for (index_target = 0; index_target < num_targets; index_target++)
    {
        if (sprintf(regname, "LW_PMGR_%s_INTR_MSK_GPIO_LIST_%d",
            targets[index_target], list) < 0)
        {
            ErrPrintf("StopClocks: Failed to generate GPIO mask regname\n");
            return (1);
        }
        if (!(reg = m_regMap->FindRegister(regname)))
        {
            ErrPrintf("StopClocks: Failed to find %s register\n", regname);
            return (1);
        }
        reg_addr = reg->GetAddress();
        reg_data = lwgpu->RegRd32(reg_addr);
        InfoPrintf("StopClocks: Reading %s = 0x%08x\n",reg->GetName(),reg_data);

        for (index_edge = 0; index_edge < num_edges; index_edge++)
        {
            // Disable GPIO interrupt mask
            if (sprintf(fieldname, "%s_GPIO%d_%s", reg->GetName(), gpio,
                edges[index_edge]) < 0)
            {
                ErrPrintf("StopClocks: Failed to generate GPIO mask "
                    "fieldname\n");
                return (1);
            }
            if (!(field = reg->FindField(fieldname)))
            {
                ErrPrintf("StopClocks: Failed to find %s field\n", fieldname);
                return (1);
            }
            if (sprintf(valuename, "%s_DISABLED", field->GetName()) < 0)
            {
                ErrPrintf("StopClocks: Failed to generate GPIO mask "
                    "valuename\n");
                return (1);
            }
            if (!(value = field->FindValue(valuename)))
            {
                ErrPrintf("StopClocks: Failed to find %s value\n", valuename);
                return (1);
            }
            reg_data = (reg_data & ~field->GetMask()) |
                       (value->GetValue() << field->GetStartBit());
        }

        // Write configuration back to register
        InfoPrintf("StopClocks: Writing %s = 0x%08x\n",reg->GetName(),reg_data);
        lwgpu->RegWr32(reg_addr, reg_data);
    }

    return (0);
}

// This routine configures the GPIO in input mode and configures the testbench
// to drive a 0.
INT32
StopClocks::config_gpio_input(UINT32 gpio)
{
    unique_ptr<IRegisterClass> reg;
    unique_ptr<IRegisterField> field;
    unique_ptr<IRegisterValue> value;
    char regname[127], fieldname[127], valuename[127], escapename[127];
    UINT32 reg_addr, reg_data, drive_mode, check_data;

    InfoPrintf("StopClocks: Configuring GPIO %d as an input\n", gpio);

    // Find the register that configures the GPIO
    if (sprintf(regname, "LW_PMGR_GPIO_%d_OUTPUT_CNTL", gpio) < 0)
    {
        ErrPrintf("StopClocks: Failed to generate GPIO config regname\n");
        return (1);
    }
    if (!(reg = m_regMap->FindRegister(regname)))
    {
        ErrPrintf("StopClocks: Failed to find %s register\n", regname);
        return (1);
    }
    reg_addr = reg->GetAddress();
    reg_data = lwgpu->RegRd32(reg_addr);
    InfoPrintf("StopClocks: Reading %s = 0x%08x\n", reg->GetName(), reg_data);

    // Configure GPIO to NORMAL mode
    strcpy(fieldname, reg->GetName());
    strcat(fieldname, "_SEL");
    if (!(field = reg->FindField(fieldname)))
    {
        ErrPrintf("StopClocks: Failed to find %s field\n", fieldname);
        return (1);
    }
    strcpy(valuename, field->GetName());
    strcat(valuename, "_NORMAL");
    if (!(value = field->FindValue(valuename)))
    {
        ErrPrintf("StopClocks: Failed to find %s value\n", valuename);
        return (1);
    }
    reg_data = (reg_data & ~field->GetMask()) |
               (value->GetValue() << field->GetStartBit());

    // Configure GPIO to INPUT mode
    strcpy(fieldname, reg->GetName());
    strcat(fieldname, "_IO_OUT_EN");
    if (!(field = reg->FindField(fieldname)))
    {
        ErrPrintf("StopClocks: Failed to find %s field\n", fieldname);
        return (1);
    }
    strcpy(valuename, field->GetName());
    strcat(valuename, "_NO");
    if (!(value = field->FindValue(valuename)))
    {
        ErrPrintf("StopClocks: Failed to find %s value\n", valuename);
        return (1);
    }
    reg_data = (reg_data & ~field->GetMask()) |
               (value->GetValue() << field->GetStartBit());

    // Write data back to register.
    InfoPrintf("StopClocks: Writing %s = 0x%08x\n", reg->GetName(), reg_data);
    lwgpu->RegWr32(reg_addr, reg_data);

    // Configure testbench to drive GPIO
    if (set_gpio_state(gpio, 0))
    {
        ErrPrintf("StopClocks: Failure in setting GPIO state\n");
        return (1);
    }
    if (sprintf(escapename, "Gpio_%d_drv", gpio) < 0)
    {
        ErrPrintf("StopClocks: Failed to generate GPIO escape name\n");
        return (1);
    }
    drive_mode = GPIO_DRV_FULL;
    Platform::EscapeWrite(escapename, 0, 4, drive_mode);
    Platform::EscapeRead(escapename, 0, 4, &check_data);
    if (check_data != drive_mode)
    {
        ErrPrintf("StopClocks: Failure in EscapeWrite of %s.  Wrote %d, "
            "Read %d\n", escapename, drive_mode, check_data);
        m_ErrCount++;
    }

    return (0);
}

// This routine configures the GPIO to output mode and to drive a 0 value.
INT32
StopClocks::config_gpio_output(UINT32 gpio)
{
    unique_ptr<IRegisterClass> reg;
    unique_ptr<IRegisterField> field;
    unique_ptr<IRegisterValue> value;
    char regname[127], fieldname[127], valuename[127];
    UINT32 reg_addr, reg_data;

    InfoPrintf("StopClocks: Configuring GPIO %d as an output\n", gpio);

    // Find the register that configures the GPIO
    if (sprintf(regname, "LW_PMGR_GPIO_%d_OUTPUT_CNTL", gpio) < 0)
    {
        ErrPrintf("StopClocks: Failed to generate GPIO config regname\n");
        return (1);
    }
    if (!(reg = m_regMap->FindRegister(regname)))
    {
        ErrPrintf("StopClocks: Failed to find %s register\n", regname);
        return (1);
    }
    reg_addr = reg->GetAddress();
    reg_data = lwgpu->RegRd32(reg_addr);
    InfoPrintf("StopClocks: Reading %s = 0x%08x\n", reg->GetName(), reg_data);

    // Configure GPIO to NORMAL mode
    strcpy(fieldname, reg->GetName());
    strcat(fieldname, "_SEL");
    if (!(field = reg->FindField(fieldname)))
    {
        ErrPrintf("StopClocks: Failed to find %s field\n", fieldname);
        return (1);
    }
    strcpy(valuename, field->GetName());
    strcat(valuename, "_NORMAL");
    if (!(value = field->FindValue(valuename)))
    {
        ErrPrintf("StopClocks: Failed to find %s value\n", valuename);
        return (1);
    }
    reg_data = (reg_data & ~field->GetMask()) |
               (value->GetValue() << field->GetStartBit());

    // Configure GPIO to OUTPUT mode
    strcpy(fieldname, reg->GetName());
    strcat(fieldname, "_IO_OUT_EN");
    if (!(field = reg->FindField(fieldname)))
    {
        ErrPrintf("StopClocks: Failed to find %s field\n", fieldname);
        return (1);
    }
    strcpy(valuename, field->GetName());
    strcat(valuename, "_YES");
    if (!(value = field->FindValue(valuename)))
    {
        ErrPrintf("StopClocks: Failed to find %s value\n", valuename);
        return (1);
    }
    reg_data = (reg_data & ~field->GetMask()) |
               (value->GetValue() << field->GetStartBit());

    // Configure GPIO to non-iverting
    strcpy(fieldname, reg->GetName());
    strcat(fieldname, "_IO_OUT_ILW");
    if (!(field = reg->FindField(fieldname)))
    {
        ErrPrintf("StopClocks: Failed to find %s field\n", fieldname);
        return (1);
    }
    strcpy(valuename, field->GetName());
    strcat(valuename, "_DISABLE");
    if (!(value = field->FindValue(valuename)))
    {
        ErrPrintf("StopClocks: Failed to find %s value\n", valuename);
        return (1);
    }
    reg_data = (reg_data & ~field->GetMask()) |
               (value->GetValue() << field->GetStartBit());

    // Configure GPIO output value to 0
    strcpy(fieldname, reg->GetName());
    strcat(fieldname, "_IO_OUTPUT");
    if (!(field = reg->FindField(fieldname)))
    {
        ErrPrintf("StopClocks: Failed to find %s field\n", fieldname);
        return (1);
    }
    strcpy(valuename, field->GetName());
    strcat(valuename, "_0");
    if (!(value = field->FindValue(valuename)))
    {
        ErrPrintf("StopClocks: Failed to find %s value\n", valuename);
        return (1);
    }
    reg_data = (reg_data & ~field->GetMask()) |
               (value->GetValue() << field->GetStartBit());

    // Write configuration back to register
    InfoPrintf("StopClocks: Writing %s = 0x%08x\n", reg->GetName(), reg_data);
    lwgpu->RegWr32(reg_addr, reg_data);

    return (0);
}

// This routine triggers the GPIO configuration to be active.
INT32
StopClocks::trigger_gpio_update()
{
    unique_ptr<IRegisterClass> reg;
    unique_ptr<IRegisterField> field;
    unique_ptr<IRegisterValue> value;
    const char *regname, *fieldname, *valuename;
    UINT32 reg_addr, reg_data;

    InfoPrintf("StopClocks: Triggering GPIO update\n");

    // Trigger GPIO configuration so that it will take effect.
    regname = "LW_PMGR_GPIO_OUTPUT_CNTL_TRIGGER";
    if (!(reg = m_regMap->FindRegister(regname)))
    {
        ErrPrintf("StopClocks: Failed to find %s register\n", regname);
        return (1);
    }
    fieldname = "LW_PMGR_GPIO_OUTPUT_CNTL_TRIGGER_UPDATE";
    if (!(field = reg->FindField(fieldname)))
    {
        ErrPrintf("StopClocks: Failed to find %s field\n", fieldname);
        return (1);
    }
    valuename = "LW_PMGR_GPIO_OUTPUT_CNTL_TRIGGER_UPDATE_TRIGGER";
    if (!(value = field->FindValue(valuename)))
    {
        ErrPrintf("StopClocks: Failed to find %s value\n", valuename);
        return (1);
    }
    reg_addr = reg->GetAddress();
    reg_data = value->GetValue() << field->GetStartBit();
    InfoPrintf("StopClocks: Writing %s = 0x%08x\n", reg->GetName(), reg_data);
    lwgpu->RegWr32(reg_addr, reg_data);

    return (0);
}

// This routine prepares XCLK for stopping.  This ilwolves switching ALT_XCLK
// and ALT_HOST to one source modules.  Div4 mode allows lower power when
// the one source module is stopped.  We shorten the PEXPLL lock detection
// time to speed up simulation.  Lastly, we pre-configure the Sleep Controller
// with the wake-up mask set to wake on INTERRUPT.
INT32
StopClocks::setup_xclk_for_stopping()
{
    // Switch both the ALT_HOSTCLK and ALT_XCLK OneSource clock to XTAL4X.
    if (set_one_src("ALT_HOSTCLK", "XTAL4X", 0))
    {
        ErrPrintf("StopClocks: Failure in setting one src\n");
        return (1);
    }
    if (set_one_src("ALT_XCLK", "XTAL4X", 0))
    {
        ErrPrintf("StopClocks: Failure in setting one src\n");
        return (1);
    }
    if (set_div4_mode())
    {
        ErrPrintf("StopClocks: Failure in setting div4 mode\n");
        return (1);
    }

    // Config stop clocks enable for ALT_XCLK.
    if (set_xclk_cond_stopclk(true))
    {
        ErrPrintf("StopClocks: Failure in setting conditional stop clock\n");
        return (1);
    }

    // shorten the PLL lock detection time to 16 us (432 xtal clocks)
    // RTL PLLs lock much more quickly, so we do not need to wait that long
    if (set_pexpll_lockdet(432))
    {
        ErrPrintf("StopClocks: Failure in setting PEX PLL lock detection\n");
        return (1);
    }

    // setup sleep controller
    if (set_sleep_ctrl("INTERRUPT"))
    {
        ErrPrintf("StopClocks: Failure in setting up sleep controller\n");
        return (1);
    }

    return (0);
}

// This routine sets the DIV4 mode for ALT_XCLK and ALT_HOSTCLK.
INT32
StopClocks::set_div4_mode()
{
    unique_ptr<IRegisterClass> reg;
    unique_ptr<IRegisterField> field;
    unique_ptr<IRegisterValue> value;
    char regname[127], fieldname[127], valuename[127];
    UINT32 reg_addr, reg_data;
    UINT32 one_src_index;
    const char *one_src_list[] = {"LW_PTRIM_SYS_HOSTCLK_ALT_LDIV",
                                  "LW_PTRIM_SYS_XCLK_ALT_LDIV"};
    const UINT32 num_regs = sizeof(one_src_list)/sizeof(char*);

    InfoPrintf("StopClocks: Configuring Div4Mode in Linear Dividers\n");
    for (one_src_index = 0; one_src_index < num_regs; one_src_index++)
    {
        strcpy(regname, one_src_list[one_src_index]);
        if (!(reg = m_regMap->FindRegister(regname)))
        {
            ErrPrintf("StopClocks: Failed to find %s register\n", regname);
            return (1);
        }

        // Read and update each register.
        reg_addr = reg->GetAddress();
        reg_data = lwgpu->RegRd32(reg_addr);
        InfoPrintf("StopClocks: Reading %s = 0x%08x\n", reg->GetName(),
            reg_data);

        // Configure the SDIV14 field to INDIV4_MODE.
        strcpy(fieldname, reg->GetName());
        strcat(fieldname, "_SDIV14");
        if (!(field = reg->FindField(fieldname)))
        {
            ErrPrintf("StopClocks: Failed to find %s field\n", fieldname);
            return (1);
        }
        strcpy(valuename, field->GetName());
        strcat(valuename, "_INDIV4_MODE");
        if (!(value = field->FindValue(valuename)))
        {
            ErrPrintf("StopClocks: Failed to find %s value\n", valuename);
            return (1);
        }
        reg_data = (reg_data & ~field->GetMask()) |
                   (value->GetValue() << field->GetStartBit());

        InfoPrintf("StopClocks: Writing %s = 0x%08x\n", reg->GetName(),
            reg_data);
        lwgpu->RegWr32(reg_addr, reg_data);
    }

    return (0);
}

// This routine configures the indicated one source clock module to select
// the indicated pll source.  ldiv and select source are fields that are
// optionally configured depending on the pll source.
INT32
StopClocks::set_one_src(const char *clkname, const char *pllname, UINT32 ldiv)
{
    unique_ptr<IRegisterClass> reg;
    unique_ptr<IRegisterField> field;
    unique_ptr<IRegisterValue> value;
    char regname[127], fieldname[127], valuename[127];
    const char *reg_basename, *pll_fieldname, *pll_valuename;
    UINT32 reg_addr, reg_data;
    bool config_ldiv, config_select;

    InfoPrintf("StopClocks: Configuring One Source Clock %s to PLL %s\n",
        clkname, pllname);

    // Register basename depends on which clock we are configuring.
    if (strcmp(clkname, "ALT_HOSTCLK") == 0)
    {
        reg_basename = "LW_PTRIM_SYS_HOSTCLK_ALT";
    }
    else if (strcmp(clkname, "ALT_XCLK") == 0)
    {
        reg_basename = "LW_PTRIM_SYS_XCLK_ALT";
    }
    else
    {
        ErrPrintf("StopClocks: Unknown clk in set_one_src: %s\n", clkname);
        return (1);
    }

    // Different fields are updated based on the PLL we select for the clock.
    // Also, we configure the pdiv only for ONESRC and SYSPLL pll sources.
    if (strcmp(pllname, "SPPLL0") == 0)
    {
        pll_fieldname = "_ONESRCCLK";
        pll_valuename = "ONESRC0";
        config_ldiv = true;
        config_select = false;
    }
    else if ((strcmp(pllname, "SPPLL1") == 0) ||
             (strcmp(pllname, "SYSPLL") == 0))
    {
        pll_fieldname = "_ONESRCCLK";
        pll_valuename = "ONESRC1";
        config_ldiv = true;
        config_select = true;
    }
    else if (strcmp(pllname, "XTAL_IN") == 0)
    {
        pll_fieldname = "_SLOWCLK";
        pll_valuename = "XTAL_IN";
        config_ldiv = false;
        config_select = false;
    }
    else if (strcmp(pllname, "XTAL4X") == 0)
    {
        pll_fieldname = "_SLOWCLK";
        pll_valuename = "XTAL4X";
        config_ldiv = false;
        config_select = false;
    }
    else
    {
        ErrPrintf("StopClocks: Unknown pll in set_one_src: %s\n", pllname);
        return (1);
    }

    // Configure optional ldiv
    if (config_ldiv)
    {
        strcpy(regname, reg_basename);
        strcat(regname, "_LDIV");
        if (!(reg = m_regMap->FindRegister(regname)))
        {
            ErrPrintf("StopClocks: Failed to find %s register\n", regname);
            return (1);
        }
        reg_addr = reg->GetAddress();
        reg_data = lwgpu->RegRd32(reg_addr);
        InfoPrintf("StopClocks: Reading %s = 0x%08x\n", reg->GetName(),
            reg_data);

        // Update the ldiv field with the ldiv value
        if (sprintf(fieldname, "%s_%sDIV", reg->GetName(), pll_valuename) < 0)
        {
            ErrPrintf("StopClocks: Failed to generate clock ldiv fieldname\n");
            return (1);
        }
        if (!(field = reg->FindField(fieldname)))
        {
            ErrPrintf("StopClocks: Failed to find %s field\n", fieldname);
            return (1);
        }
        reg_data = (reg_data & ~field->GetMask()) |
                   ((ldiv << field->GetStartBit()) & field->GetMask());

        // Write data back to register.
        InfoPrintf("StopClocks: Writing %s = 0x%08x\n", reg->GetName(),
            reg_data);
        lwgpu->RegWr32(reg_addr, reg_data);
    }

    // Switch the clock source.
    strcpy(regname, reg_basename);
    strcat(regname, "_SWITCH");
    if (!(reg = m_regMap->FindRegister(regname)))
    {
        ErrPrintf("StopClocks: Failed to find %s register\n", regname);
        return (1);
    }
    reg_addr = reg->GetAddress();
    reg_data = lwgpu->RegRd32(reg_addr);
    InfoPrintf("StopClocks: Reading %s = 0x%08x\n", reg->GetName(), reg_data);

    // configure optional select field
    if (config_select)
    {
        strcpy(fieldname, reg->GetName());
        strcat(fieldname, "_ONESRCCLK1_SELECT");
        if (!(field = reg->FindField(fieldname)))
        {
            ErrPrintf("StopClocks: Failed to find %s field\n", fieldname);
            return (1);
        }
        strcpy(valuename, field->GetName());
        strcat(valuename, "_");
        strcat(valuename, pllname);
        if (!(value = field->FindValue(valuename)))
        {
            ErrPrintf("StopClocks: Failed to find %s value\n", valuename);
            return (1);
        }
        reg_data = (reg_data & ~field->GetMask()) |
                   (value->GetValue() << field->GetStartBit());
    }

    // update the appropriate pll field based on the pll source
    strcpy(fieldname, reg->GetName());
    strcat(fieldname, pll_fieldname);
    if (!(field = reg->FindField(fieldname)))
    {
        ErrPrintf("StopClocks: Failed to find %s field\n", fieldname);
        return (1);
    }
    strcpy(valuename, field->GetName());
    strcat(valuename, "_");
    strcat(valuename, pll_valuename);
    if (!(value = field->FindValue(valuename)))
    {
        ErrPrintf("StopClocks: Failed to find %s value\n", valuename);
        return (1);
    }
    reg_data = (reg_data & ~field->GetMask()) |
               (value->GetValue() << field->GetStartBit());

    // update the FINALSEL field
    strcpy(fieldname, reg->GetName());
    strcat(fieldname, "_FINALSEL");
    if (!(field = reg->FindField(fieldname)))
    {
        ErrPrintf("StopClocks: Failed to find %s field\n", fieldname);
        return (1);
    }
    strcpy(valuename, field->GetName());
    strcat(valuename, pll_fieldname);
    if (!(value = field->FindValue(valuename)))
    {
        ErrPrintf("StopClocks: Failed to find %s value\n", valuename);
        return (1);
    }
    reg_data = (reg_data & ~field->GetMask()) |
               (value->GetValue() << field->GetStartBit());

    // Write data back to register.
    InfoPrintf("StopClocks: Writing %s = 0x%08x\n", reg->GetName(), reg_data);
    lwgpu->RegWr32(reg_addr, reg_data);

    return (0);
}

// This routine configures the PEXPLL lock detection time.  wait_delay is in
// units of 27 MHz clocks.
INT32
StopClocks::set_pexpll_lockdet(UINT32 wait_delay)
{
    unique_ptr<IRegisterClass> reg;
    unique_ptr<IRegisterField> field;
    const char *regname, *fieldname;
    UINT32 reg_addr, reg_data;

    InfoPrintf("StopClocks: Configuring PEX PLL lock detection delay to %d "
        "clocks\n", wait_delay);

    regname = "LW_PTRIM_SYS_PEXPLL_LOCKDET";
    if (!(reg = m_regMap->FindRegister(regname)))
    {
        ErrPrintf("StopClocks: Failed to find %s register\n", regname);
        return (1);
    }
    reg_addr = reg->GetAddress();
    reg_data = lwgpu->RegRd32(reg_addr);
    InfoPrintf("StopClocks: Reading %s = 0x%08x\n", reg->GetName(), reg_data);

    fieldname = "LW_PTRIM_SYS_PEXPLL_LOCKDET_WAIT_DLY_LENGTH";
    if (!(field = reg->FindField(fieldname)))
    {
        ErrPrintf("StopClocks: Failed to find %s field\n", fieldname);
        return (1);
    }
    reg_data = (reg_data & ~field->GetMask()) |
               ((wait_delay << field->GetStartBit()) & field->GetMask());

    // Write data back to register.
    InfoPrintf("StopClocks: Writing %s = 0x%08x\n", reg->GetName(), reg_data);
    lwgpu->RegWr32(reg_addr, reg_data);

    return (0);
}

// This routine configures the conditional stopclock field for the xclk one
// source module.  This allows or disallows the xclk to stop under control
// of the sleep controller.
INT32
StopClocks::set_xclk_cond_stopclk(bool enable)
{
    unique_ptr<IRegisterClass> reg;
    unique_ptr<IRegisterField> field;
    unique_ptr<IRegisterValue> value;
    const char *regname, *fieldname, *valuename;
    UINT32 reg_addr, reg_data;

    InfoPrintf("StopClocks: Configuring ALT_XCLK stop clock enable to %s\n",
        enable ? "YES" : "NO");

    regname = "LW_PTRIM_SYS_XCLK_ALT_SWITCH";
    if (!(reg = m_regMap->FindRegister(regname)))
    {
        ErrPrintf("StopClocks: Failed to find %s register\n", regname);
        return (1);
    }
    reg_addr = reg->GetAddress();
    reg_data = lwgpu->RegRd32(reg_addr);
    InfoPrintf("StopClocks: Reading %s = 0x%08x\n", reg->GetName(), reg_data);

    fieldname = "LW_PTRIM_SYS_XCLK_ALT_SWITCH_COND_STOPCLK_EN";
    if (!(field = reg->FindField(fieldname)))
    {
        ErrPrintf("StopClocks: Failed to find %s field\n", fieldname);
        return (1);
    }
    if (enable)
    {
        valuename = "LW_PTRIM_SYS_XCLK_ALT_SWITCH_COND_STOPCLK_EN_YES";
    }
    else
    {
        valuename = "LW_PTRIM_SYS_XCLK_ALT_SWITCH_COND_STOPCLK_EN_NO";
    }
    if (!(value = field->FindValue(valuename)))
    {
        ErrPrintf("StopClocks: Failed to find %s value\n", valuename);
        return (1);
    }
    reg_data = (reg_data & ~field->GetMask()) |
               (value->GetValue() << field->GetStartBit());

    // Write data back to register.
    InfoPrintf("StopClocks: Writing %s = 0x%08x\n", reg->GetName(), reg_data);
    lwgpu->RegWr32(reg_addr, reg_data);

    return (0);
}

// This routine updates the testbench state of the specified GPIO.  This is
// done using escape write.  This updates the output state only, it does not
// change the drive or weakpull configuration of the testbench.
INT32
StopClocks::set_gpio_state (UINT32 gpio, UINT32 state)
{
    char escapename[127];
    UINT32 check_data;

    if (sprintf(escapename, "Gpio_%d_state", gpio) < 0)
    {
        ErrPrintf("StopClocks: Failed to generate GPIO escape name\n");
        return (1);
    }
    InfoPrintf("StopClocks: EscapeWrite %s = %d\n", escapename, state);
    Platform::EscapeWrite(escapename, 0, 4, state);
    Platform::EscapeRead(escapename, 0, 4, &check_data);
    if (check_data != state)
    {
        ErrPrintf("StopClocks: Failure in EscapeWrite of %s.  Wrote %d, "
            "Read %d\n", escapename, state, check_data);
        m_ErrCount++;
    }

    return (0);
}

// This routine pre-configures the wakeup mask of the sleep controller.  Only
// a single wakeup event can be enabled with this routine.  This routine does
// not generate a sleep request.  wake_event should be a substring of the
// LW_THERM_SLEEP_CTRL_0_WAKE_MASK_* fields.
INT32
StopClocks::set_sleep_ctrl(const char *wake_event)
{
    unique_ptr<IRegisterClass> reg;
    unique_ptr<IRegisterField> field;
    unique_ptr<IRegisterValue> value;
    char regname[127], fieldname[127], valuename[127];
    UINT32 reg_addr, reg_data;

    InfoPrintf("StopClocks: Setting up sleep controller to wake on %s\n",
        wake_event);

    strcpy(regname, "LW_THERM_SLEEP_CTRL_0");
    if (!(reg = m_regMap->FindRegister(regname)))
    {
        ErrPrintf("StopClocks: Failed to find %s register\n", regname);
        return (1);
    }
    reg_addr = reg->GetAddress();
    strcpy(fieldname, reg->GetName());
    strcat(fieldname, "_WAKE_MASK_");
    strcat(fieldname, wake_event);
    if (!(field = reg->FindField(fieldname)))
    {
        ErrPrintf("StopClocks: Failed to find %s field\n", fieldname);
        return (1);
    }
    strcpy(valuename, field->GetName());
    strcat(valuename, "_ENABLE");
    if (!(value = field->FindValue(valuename)))
    {
        ErrPrintf("StopClocks: Failed to find %s value\n", valuename);
        return (1);
    }
    reg_data = (value->GetValue() << field->GetStartBit());
    InfoPrintf("StopClocks: Writing %s = 0x%08x\n", reg->GetName(), reg_data);
    lwgpu->RegWr32(reg_addr, reg_data);

    return (0);
}

// This routine sends the PMU command along with parameter 1 to PMU ucode.
// PMU ucode will interpret the meaning of the command and parameter 1 and
// perform the appropriate SW action on behalf of this test.  Supported
// commands can be found at the beginning of stop_clocks.h file.
INT32
StopClocks::set_pmu_command (UINT32 command, UINT32 param1)
{
    unique_ptr<IRegisterClass> reg;
    UINT32 reg_addr, reg_data;

    InfoPrintf("StopClocks: Setting up PMU command to 0x%02x, param1 0x%08x\n",
        command, param1);

    if (!(reg = m_regMap->FindRegister(SC_PARAM_1_REG)))
    {
        ErrPrintf("StopClocks: Failed to find %s register\n", SC_PARAM_1_REG);
        return (1);
    }
    reg_addr = reg->GetAddress();
    reg_data = param1;
    InfoPrintf("StopClocks: Writing %s = 0x%08x\n", reg->GetName(), reg_data);
    lwgpu->RegWr32(reg_addr, reg_data);

    if (!(reg = m_regMap->FindRegister(SC_COMMAND_REG)))
    {
        ErrPrintf("StopClocks: Failed to find %s register\n", SC_COMMAND_REG);
        return (1);
    }
    reg_addr = reg->GetAddress();
    reg_data = (command & 0xff) << 24;
    InfoPrintf("StopClocks: Writing %s = 0x%08x\n", reg->GetName(), reg_data);
    lwgpu->RegWr32(reg_addr, reg_data);

    return (0);
}

// This routine waits until the PMU command is done.  This is indicated when
// PMU writes all zeros into the command field.  The return code is in the
// lower 8-bits.  A zero return code indicates success.  A non-zero return
// code indicates failure.  Lwrrently, PMU ucode is not expected to return
// any failures, but we have to check the return code in case the PMU ucode
// is updated to return failues in the future.
INT32
StopClocks::poll_for_pmu_done()
{
    unique_ptr<IRegisterClass> reg;
    UINT32 reg_addr, reg_data;
    UINT32 polling_attempts, command, return_code;
    bool done;

    InfoPrintf("StopClocks: Polling for PMU command to complete\n");

    // The PMU command field is in the upper 8 bits of COMMAND_REG.
    // PMU ucode sets this to zeros when it completes its operation.
    if (!(reg = m_regMap->FindRegister(SC_COMMAND_REG)))
    {
        ErrPrintf("StopClocks: Failed to find %s register\n", SC_COMMAND_REG);
        return (1);
    }
    reg_addr = reg->GetAddress();
    polling_attempts = 1;

    // Check command field for all zeros to indicate when PMU is done.
    // We limit the number of polls so we do not hang the test if PMU is hung.
    reg_data = lwgpu->RegRd32(reg_addr);
    command = (reg_data >> 24) & 0xff;
    done = (command == SC_PMU_DONE);
    while (!done && (polling_attempts < m_max_polling_attempts))
    {
        if ((polling_attempts % 10) == 1)
        {
            InfoPrintf("StopClocks: Polling PMU ucode done attempt #%d: %s[31"
                ":24] = 0x%02x\n", polling_attempts, reg->GetName(), command);
        }
        Platform::Delay(1);  // delay 1 microsecond between polls
        polling_attempts = polling_attempts + 1;
        reg_data = lwgpu->RegRd32(reg_addr);
        command = (reg_data >> 24) & 0xff;
        done = (command == SC_PMU_DONE);
    }

    InfoPrintf("StopClocks: Polling PMU ucode done attempt #%d: %s[31"
        ":24] = 0x%02x\n", polling_attempts, reg->GetName(), command);
    if (!done)
    {
        ErrPrintf("StopClocks: Timeout waiting for %s[31:24] to be 0x%02x "
            "after %d attempts\n", reg->GetName(), SC_PMU_DONE,
            m_max_polling_attempts);
        return (1);
    }

    // Check the return code.  A non-zero value means failure.
    return_code = (reg_data & 0xff);
    if (return_code)
    {
        ErrPrintf("StopClocks: Detected PMU ucode error: return code = 0x%02x"
            "\n", return_code);
        return (1);
    }
    InfoPrintf("StopClocks: Done polling for PMU ucode to complete\n");

    return (0);
}

// This routine configures XP clock BLCG to stay running during L1.  This is
// needed to keep us from entering Deep L1 before we finish all our priv
// transactions (including delays).
INT32
StopClocks::config_xp_blcg()
{
    unique_ptr<IRegisterClass> reg;
    unique_ptr<IRegisterField> field;
    unique_ptr<IRegisterValue> value;
    const char *regname, *fieldname, *valuename;
    UINT32 reg_addr, reg_data;

    InfoPrintf("StopClocks: Configuring XP clock BLCG\n");

    // LW_THERM_GATE_CTRL_BLK_CLK = AUTO
    // Configure Gate control: LW_THERM_GATE_CTRL_XP = 0x17 (as of gk208)
    if (config_gate_ctrl(0x17, "AUTO"))
    {
        ErrPrintf("StopClocks: Failure in configuring Clock Gate control\n");
        return (1);
    }

    // LW_XP_CLKCTRL_BLKCG2_HOST2XV_FORCE_CLK_ON_WAKE_EN = ENABLED
    regname = "LW_XP_CLKCTRL_BLKCG2";
    if (!(reg = m_regMap->FindRegister(regname)))
    {
        ErrPrintf("StopClocks: Failed to find %s register\n", regname);
        return (1);
    }
    reg_addr = reg->GetAddress();
    reg_data = lwgpu->RegRd32(reg_addr);
    InfoPrintf("StopClocks: Reading %s = 0x%08x\n", reg->GetName(), reg_data);
    fieldname = "LW_XP_CLKCTRL_BLKCG2_HOST2XV_FORCE_CLK_ON_WAKE_EN";
    if (!(field = reg->FindField(fieldname)))
    {
        ErrPrintf("StopClocks: Failed to find %s field\n", fieldname);
        return (1);
    }
    valuename = "LW_XP_CLKCTRL_BLKCG2_HOST2XV_FORCE_CLK_ON_WAKE_EN_ENABLED";
    if (!(value = field->FindValue(valuename)))
    {
        ErrPrintf("StopClocks: Failed to find %s value\n", valuename);
        return (1);
    }
    reg_data = (reg_data & ~field->GetMask()) |
               (value->GetValue() << field->GetStartBit());
    InfoPrintf("StopClocks: Writing %s = 0x%08x\n", reg->GetName(), reg_data);
    lwgpu->RegWr32(reg_addr, reg_data);

    return (0);
}

// This routine configures us to enter Deep L1.  This code is modified from
// //sw/dev/gpu_drv/chips_a/diag/mods/mdiag/tests/gpu/misc/power_gk107/
//   power_onesrc_kepler_gk107.cpp and there is little documentation on
// the steps it does and why.  Note that PCIE config space (ie, LW_XVE_*)
// is accessed by adding 0x88000 to the BAR0 address.
INT32
StopClocks::enable_deep_l1()
{
    unique_ptr<IRegisterClass> reg;
    unique_ptr<IRegisterField> field;
    unique_ptr<IRegisterValue> value;
    const char *regname, *fieldname, *valuename;
    UINT32 reg_addr, reg_data;

    InfoPrintf("StopClocks: Enabling Deep L1\n");

    // Write LW_XVE_PRI_XVE_CG_IDLE_CG_DLY_CNT to zero
    // Write LW_XVE_PRI_XVE_CG_IDLE_CG_EN to ENABLED
    regname = "LW_XVE_PRI_XVE_CG";
    if (!(reg = m_regMap->FindRegister(regname)))
    {
        ErrPrintf("StopClocks: Failed to find %s register\n", regname);
        return (1);
    }
    reg_addr = reg->GetAddress() + LW_PCFG_OFFSET;
    reg_data = lwgpu->RegRd32(reg_addr);
    InfoPrintf("StopClocks: Reading %s = 0x%08x\n", reg->GetName(), reg_data);
    fieldname = "LW_XVE_PRI_XVE_CG_IDLE_CG_DLY_CNT";
    if (!(field = reg->FindField(fieldname)))
    {
        ErrPrintf("StopClocks: Failed to find %s field\n", fieldname);
        return (1);
    }
    reg_data = (reg_data & ~field->GetMask());
    fieldname = "LW_XVE_PRI_XVE_CG_IDLE_CG_EN";
    if (!(field = reg->FindField(fieldname)))
    {
        ErrPrintf("StopClocks: Failed to find %s field\n", fieldname);
        return (1);
    }
    valuename = "LW_XVE_PRI_XVE_CG_IDLE_CG_EN_ENABLED";
    if (!(value = field->FindValue(valuename)))
    {
        ErrPrintf("StopClocks: Failed to find %s value\n", valuename);
        return (1);
    }
    reg_data = (reg_data & ~field->GetMask()) |
               (value->GetValue() << field->GetStartBit());
    InfoPrintf("StopClocks: Writing %s = 0x%08x\n", reg->GetName(), reg_data);
    lwgpu->RegWr32(reg_addr, reg_data);

    Platform::Delay(20);

    // Write LW_XVE_PRIV_MISC_CYA_PMCSR_POWER_STATE_D3HOT_IGNORE to zero
    regname = "LW_XVE_PRIV_MISC";
    if (!(reg = m_regMap->FindRegister(regname)))
    {
        ErrPrintf("StopClocks: Failed to find %s register\n", regname);
        return (1);
    }
    reg_addr = reg->GetAddress() + LW_PCFG_OFFSET;
    reg_data = lwgpu->RegRd32(reg_addr);
    InfoPrintf("StopClocks: Reading %s = 0x%08x\n",reg->GetName(),reg_data);
    fieldname = "LW_XVE_PRIV_MISC_CYA_PMCSR_POWER_STATE_D3HOT_IGNORE";
    if (!(field = reg->FindField(fieldname)))
    {
        ErrPrintf("StopClocks: Failed to find %s field\n", fieldname);
        return (1);
    }
    reg_data = (reg_data & ~field->GetMask());
    InfoPrintf("StopClocks: Writing %s = 0x%08x\n", reg->GetName(), reg_data);
    lwgpu->RegWr32(reg_addr, reg_data);

    Platform::Delay(20);

    // Configure IDLE counter
    if (config_idle_counter(1))
    {
        ErrPrintf("StopClocks: Failure in configuring IDLE counter\n");
        return (1);
    }

    // Write LW_XVE_LINK_CONTROL_STATUS_CLOCK_PM to 1
    regname = "LW_XVE_LINK_CONTROL_STATUS";
    if (!(reg = m_regMap->FindRegister(regname)))
    {
        ErrPrintf("StopClocks: Failed to find %s register\n", regname);
        return (1);
    }
    reg_addr = reg->GetAddress() + LW_PCFG_OFFSET;
    reg_data = lwgpu->RegRd32(reg_addr);
    InfoPrintf("StopClocks: Reading %s = 0x%08x\n",reg->GetName(),reg_data);
    fieldname = "LW_XVE_LINK_CONTROL_STATUS_CLOCK_PM";
    if (!(field = reg->FindField(fieldname)))
    {
        ErrPrintf("StopClocks: Failed to find %s field\n", fieldname);
        return (1);
    }
    reg_data = (reg_data & ~field->GetMask()) | (1 << field->GetStartBit());
    InfoPrintf("StopClocks: Writing %s = 0x%08x\n", reg->GetName(), reg_data);
    lwgpu->RegWr32(reg_addr, reg_data);

    // Configure Gate control: LW_THERM_GATE_CTRL_XVE = 0x15 (as of gk208)
    if (config_gate_ctrl(0x15, "AUTO"))
    {
        ErrPrintf("StopClocks: Failure in configuring Clock Gate control\n");
        return (1);
    }

    // Write LW_XVE_PRIV_XV_1_CYA_XVE_MULTIFUNC to DISABLE
    regname = "LW_XVE_PRIV_XV_1";
    if (!(reg = m_regMap->FindRegister(regname)))
    {
        ErrPrintf("StopClocks: Failed to find %s register\n", regname);
        return (1);
    }
    reg_addr = reg->GetAddress() + LW_PCFG_OFFSET;
    reg_data = lwgpu->RegRd32(reg_addr);
    InfoPrintf("StopClocks: Reading %s = 0x%08x\n",reg->GetName(),reg_data);
    fieldname = "LW_XVE_PRIV_XV_1_CYA_XVE_MULTIFUNC";
    if (!(field = reg->FindField(fieldname)))
    {
        ErrPrintf("StopClocks: Failed to find %s field\n", fieldname);
        return (1);
    }
    valuename = "LW_XVE_PRIV_XV_1_CYA_XVE_MULTIFUNC_DISABLE";
    if (!(value = field->FindValue(valuename)))
    {
        ErrPrintf("StopClocks: Failed to find %s value\n", valuename);
        return (1);
    }
    reg_data = (reg_data & ~field->GetMask()) |
               (value->GetValue() << field->GetStartBit());
    InfoPrintf("StopClocks: Writing %s = 0x%08x\n", reg->GetName(), reg_data);
    lwgpu->RegWr32(reg_addr, reg_data);

    // Write LW_XVE_PRIV_XV_0_CYA_L0S_ENABLE to ENABLED
    // Write LW_XVE_PRIV_XV_0_CYA_L1_ENABLE to ENABLED
    regname = "LW_XVE_PRIV_XV_0";
    if (!(reg = m_regMap->FindRegister(regname)))
    {
        ErrPrintf("StopClocks: Failed to find %s register\n", regname);
        return (1);
    }
    reg_addr = reg->GetAddress() + LW_PCFG_OFFSET;
    reg_data = lwgpu->RegRd32(reg_addr);
    InfoPrintf("StopClocks: Reading %s = 0x%08x\n",reg->GetName(),reg_data);
    fieldname = "LW_XVE_PRIV_XV_0_CYA_L0S_ENABLE";
    if (!(field = reg->FindField(fieldname)))
    {
        ErrPrintf("StopClocks: Failed to find %s field\n", fieldname);
        return (1);
    }
    valuename = "LW_XVE_PRIV_XV_0_CYA_L0S_ENABLE_ENABLED";
    if (!(value = field->FindValue(valuename)))
    {
        ErrPrintf("StopClocks: Failed to find %s value\n", valuename);
        return (1);
    }
    reg_data = (reg_data & ~field->GetMask()) |
               (value->GetValue() << field->GetStartBit());
    fieldname = "LW_XVE_PRIV_XV_0_CYA_L1_ENABLE";
    if (!(field = reg->FindField(fieldname)))
    {
        ErrPrintf("StopClocks: Failed to find %s field\n", fieldname);
        return (1);
    }
    valuename = "LW_XVE_PRIV_XV_0_CYA_L1_ENABLE_ENABLED";
    if (!(value = field->FindValue(valuename)))
    {
        ErrPrintf("StopClocks: Failed to find %s value\n", valuename);
        return (1);
    }
    reg_data = (reg_data & ~field->GetMask()) |
               (value->GetValue() << field->GetStartBit());
    InfoPrintf("StopClocks: Writing %s = 0x%08x\n", reg->GetName(), reg_data);
    lwgpu->RegWr32(reg_addr, reg_data);

    // Write LW_XVE_PRIV_XV_BLKCG2_HOST2XV_HOST_IDLE_WAKE_EN to DISABLED
    // Write LW_XVE_PRIV_XV_BLKCG2_UPSTREAM_REQ_WAKE_EN to DISABLED
    regname = "LW_XVE_PRIV_XV_BLKCG2";
    if (!(reg = m_regMap->FindRegister(regname)))
    {
        ErrPrintf("StopClocks: Failed to find %s register\n", regname);
        return (1);
    }
    reg_addr = reg->GetAddress() + LW_PCFG_OFFSET;
    reg_data = lwgpu->RegRd32(reg_addr);
    InfoPrintf("StopClocks: Reading %s = 0x%08x\n",reg->GetName(),reg_data);
    fieldname = "LW_XVE_PRIV_XV_BLKCG2_HOST2XV_HOST_IDLE_WAKE_EN";
    if (!(field = reg->FindField(fieldname)))
    {
        ErrPrintf("StopClocks: Failed to find %s field\n", fieldname);
        return (1);
    }
    valuename = "LW_XVE_PRIV_XV_BLKCG2_HOST2XV_HOST_IDLE_WAKE_EN_DISABLED";
    if (!(value = field->FindValue(valuename)))
    {
        ErrPrintf("StopClocks: Failed to find %s value\n", valuename);
        return (1);
    }
    reg_data = (reg_data & ~field->GetMask()) |
               (value->GetValue() << field->GetStartBit());
    fieldname = "LW_XVE_PRIV_XV_BLKCG2_UPSTREAM_REQ_WAKE_EN";
    if (!(field = reg->FindField(fieldname)))
    {
        ErrPrintf("StopClocks: Failed to find %s field\n", fieldname);
        return (1);
    }
    valuename = "LW_XVE_PRIV_XV_BLKCG2_UPSTREAM_REQ_WAKE_EN_DISABLED";
    if (!(value = field->FindValue(valuename)))
    {
        ErrPrintf("StopClocks: Failed to find %s value\n", valuename);
        return (1);
    }
    reg_data = (reg_data & ~field->GetMask()) |
               (value->GetValue() << field->GetStartBit());
    InfoPrintf("StopClocks: Writing %s = 0x%08x\n", reg->GetName(), reg_data);
    lwgpu->RegWr32(reg_addr, reg_data);

    // Configure XP power
    if (config_xp_power1())
    {
        ErrPrintf("StopClocks: Failure in configuring XP power\n");
        return (1);
    }
    if (config_xp_power2())
    {
        ErrPrintf("StopClocks: Failure in configuring XP power\n");
        return (1);
    }

    // configure XP clock BLCG behavior
    if (config_xp_blcg())
    {
        ErrPrintf("StopClocks: Failure in configuring XP clock BLCG\n");
        return (1);
    }

    // Write LW_XVE_LINK_CONTROL_STATUS_ACTIVE_STATE_LINK_PM_CONTROL to
    // L1_ENABLE_L0S_ENABLE
    regname = "LW_XVE_LINK_CONTROL_STATUS";
    if (!(reg = m_regMap->FindRegister(regname)))
    {
        ErrPrintf("StopClocks: Failed to find %s register\n", regname);
        return (1);
    }
    reg_addr = reg->GetAddress() + LW_PCFG_OFFSET;
    reg_data = lwgpu->RegRd32(reg_addr);
    InfoPrintf("StopClocks: Reading %s = 0x%08x\n",reg->GetName(),reg_data);
    fieldname = "LW_XVE_LINK_CONTROL_STATUS_ACTIVE_STATE_LINK_PM_CONTROL";
    if (!(field = reg->FindField(fieldname)))
    {
        ErrPrintf("StopClocks: Failed to find %s field\n", fieldname);
        return (1);
    }
    valuename = "LW_XVE_LINK_CONTROL_STATUS_ACTIVE_STATE_LINK_PM_CONTROL_L1_ENABLE_L0S_ENABLE";
    if (!(value = field->FindValue(valuename)))
    {
        ErrPrintf("StopClocks: Failed to find %s value\n", valuename);
        return (1);
    }
    reg_data = (reg_data & ~field->GetMask()) |
               (value->GetValue() << field->GetStartBit());
    InfoPrintf("StopClocks: Writing %s = 0x%08x\n", reg->GetName(), reg_data);
    lwgpu->RegWr32(reg_addr, reg_data);

    // Write LW_XVE_PRIV_XV_BLKCG2_CONFIG0_UPDATE_WAKE_EN to ENABLED
    regname = "LW_XVE_PRIV_XV_BLKCG2";
    if (!(reg = m_regMap->FindRegister(regname)))
    {
        ErrPrintf("StopClocks: Failed to find %s register\n", regname);
        return (1);
    }
    reg_addr = reg->GetAddress() + LW_PCFG_OFFSET;
    reg_data = lwgpu->RegRd32(reg_addr);
    InfoPrintf("StopClocks: Reading %s = 0x%08x\n", reg->GetName(), reg_data);
    fieldname = "LW_XVE_PRIV_XV_BLKCG2_CONFIG0_UPDATE_WAKE_EN";
    if (!(field = reg->FindField(fieldname)))
    {
        ErrPrintf("StopClocks: Failed to find %s field\n", fieldname);
        return (1);
    }
    valuename = "LW_XVE_PRIV_XV_BLKCG2_CONFIG0_UPDATE_WAKE_EN_ENABLED";
    if (!(value = field->FindValue(valuename)))
    {
        ErrPrintf("StopClocks: Failed to find %s value\n", valuename);
        return (1);
    }
    reg_data = (reg_data & ~field->GetMask()) |
               (value->GetValue() << field->GetStartBit());
    InfoPrintf("StopClocks: Writing %s = 0x%08x\n", reg->GetName(), reg_data);
    lwgpu->RegWr32(reg_addr, reg_data);

    // Configure XP power
    if (config_xp_power1())
    {
        ErrPrintf("StopClocks: Failure in configuring XP power\n");
        return (1);
    }

    return (0);
}

// This routine configures us to exit Deep L1.  This code is modified from
// //sw/dev/gpu_drv/chips_a/diag/mods/mdiag/tests/gpu/misc/power_gk107/
//   power_onesrc_kepler_gk107.cpp and there is little documentation on
// the steps it does and why.  Note that PCIE config space (ie, LW_XVE_*)
// is accessed by adding 0x88000 to the BAR0 address.
INT32
StopClocks::exit_deep_l1()
{
    unique_ptr<IRegisterClass> reg;
    unique_ptr<IRegisterField> field;
    unique_ptr<IRegisterValue> value;
    const char *regname, *fieldname, *valuename;
    UINT32 reg_addr, reg_data;

    InfoPrintf("StopClocks: Exiting Deep L1\n");

    // Write LW_XVE_PRIV_XV_BLKCG2_CONFIG0_UPDATE_WAKE_EN to ENABLED
    regname = "LW_XVE_PRIV_XV_BLKCG2";
    if (!(reg = m_regMap->FindRegister(regname)))
    {
        ErrPrintf("StopClocks: Failed to find %s register\n", regname);
        return (1);
    }
    reg_addr = reg->GetAddress() + LW_PCFG_OFFSET;
    reg_data = lwgpu->RegRd32(reg_addr);
    InfoPrintf("StopClocks: Reading %s = 0x%08x\n", reg->GetName(), reg_data);
    fieldname = "LW_XVE_PRIV_XV_BLKCG2_CONFIG0_UPDATE_WAKE_EN";
    if (!(field = reg->FindField(fieldname)))
    {
        ErrPrintf("StopClocks: Failed to find %s field\n", fieldname);
        return (1);
    }
    valuename = "LW_XVE_PRIV_XV_BLKCG2_CONFIG0_UPDATE_WAKE_EN_ENABLED";
    if (!(value = field->FindValue(valuename)))
    {
        ErrPrintf("StopClocks: Failed to find %s value\n", valuename);
        return (1);
    }
    reg_data = (reg_data & ~field->GetMask()) |
               (value->GetValue() << field->GetStartBit());
    InfoPrintf("StopClocks: Writing %s = 0x%08x\n", reg->GetName(), reg_data);
    lwgpu->RegWr32(reg_addr, reg_data);

    Platform::Delay(10);

    // Configure Gate control: LW_THERM_GATE_CTRL_XVE = 0x15 (as of gk208)
    if (config_gate_ctrl(0x15, "RUN"))
    {
        ErrPrintf("StopClocks: Failure in configuring Clock Gate control\n");
        return (1);
    }

    Platform::Delay(10);

    // Write LW_XP_PEX_PLL_PWRDN_D0_EN to DISBLED
    // Write LW_XP_PEX_PLL_PWRDN_D3_EN to DISBLED
    regname = "LW_XP_PEX_PLL";
    if (!(reg = m_regMap->FindRegister(regname)))
    {
        ErrPrintf("StopClocks: Failed to find %s register\n", regname);
        return (1);
    }
    reg_addr = reg->GetAddress();
    reg_data = lwgpu->RegRd32(reg_addr);
    InfoPrintf("StopClocks: Reading %s = 0x%08x\n", reg->GetName(), reg_data);

    fieldname = "LW_XP_PEX_PLL_PWRDN_D0_EN";
    if (!(field = reg->FindField(fieldname)))
    {
        ErrPrintf("StopClocks: Failed to find %s field\n", fieldname);
        return (1);
    }
    valuename = "LW_XP_PEX_PLL_PWRDN_D0_EN_DISBLED";
    if (!(value = field->FindValue(valuename)))
    {
        ErrPrintf("StopClocks: Failed to find %s value\n", valuename);
        return (1);
    }
    reg_data = (reg_data & ~field->GetMask()) |
               (value->GetValue() << field->GetStartBit());
    fieldname = "LW_XP_PEX_PLL_PWRDN_D3_EN";
    if (!(field = reg->FindField(fieldname)))
    {
        ErrPrintf("StopClocks: Failed to find %s field\n", fieldname);
        return (1);
    }
    valuename = "LW_XP_PEX_PLL_PWRDN_D3_EN_DISBLED";
    if (!(value = field->FindValue(valuename)))
    {
        ErrPrintf("StopClocks: Failed to find %s value\n", valuename);
        return (1);
    }
    reg_data = (reg_data & ~field->GetMask()) |
               (value->GetValue() << field->GetStartBit());

    InfoPrintf("StopClocks: Writing %s = 0x%08x\n", reg->GetName(), reg_data);
    lwgpu->RegWr32(reg_addr, reg_data);

    return (0);
}

// This routine supports Deep L1 entry.  This code is modified from
// //sw/dev/gpu_drv/chips_a/diag/mods/mdiag/tests/gpu/misc/power_gk107/
//   power_onesrc_kepler_gk107.cpp and there is little documentation on
// the steps it does and why.
INT32
StopClocks::config_idle_counter(UINT32 index)
{
    unique_ptr<IRegisterClass> reg;
    unique_ptr<IRegisterField> field;
    unique_ptr<IRegisterValue> value;
    const char *regname, *fieldname, *valuename;
    UINT32 reg_addr, reg_data;

    InfoPrintf("StopClocks: Enabling IDLE counter register\n");

    // Write LW_PPWR_PMU_IDLE_MASK_XVXP_DEEP to ENABLED
    regname = "LW_PPWR_PMU_IDLE_MASK";
    if (!(reg = m_regMap->FindRegister(regname)))
    {
        ErrPrintf("StopClocks: Failed to find %s register\n", regname);
        return (1);
    }
    reg_addr = reg->GetAddress(index);
    reg_data = lwgpu->RegRd32(reg_addr);
    InfoPrintf("StopClocks: Reading %s(%d) = 0x%08x\n", reg->GetName(), index,
        reg_data);
    fieldname = "LW_PPWR_PMU_IDLE_MASK_XVXP_DEEP";
    if (!(field = reg->FindField(fieldname)))
    {
        ErrPrintf("StopClocks: Failed to find %s field\n", fieldname);
        return (1);
    }
    valuename = "LW_PPWR_PMU_IDLE_MASK_XVXP_DEEP_ENABLED";
    if (!(value = field->FindValue(valuename)))
    {
        ErrPrintf("StopClocks: Failed to find %s value\n", valuename);
        return (1);
    }
    reg_data = (reg_data & ~field->GetMask()) |
               (value->GetValue() << field->GetStartBit());
    InfoPrintf("StopClocks: Writing %s(%d) = 0x%08x\n", reg->GetName(), index,
        reg_data);
    lwgpu->RegWr32(reg_addr, reg_data);

    // Write LW_PPWR_PMU_IDLE_COUNT_RESET to NOT_DONE, then to DONE
    regname = "LW_PPWR_PMU_IDLE_COUNT";
    if (!(reg = m_regMap->FindRegister(regname)))
    {
        ErrPrintf("StopClocks: Failed to find %s register\n", regname);
        return (1);
    }
    reg_addr = reg->GetAddress(index);
    reg_data = lwgpu->RegRd32(reg_addr);
    InfoPrintf("StopClocks: Reading %s(%d) = 0x%08x\n", reg->GetName(), index,
        reg_data);
    fieldname = "LW_PPWR_PMU_IDLE_COUNT_RESET";
    if (!(field = reg->FindField(fieldname)))
    {
        ErrPrintf("StopClocks: Failed to find %s field\n", fieldname);
        return (1);
    }
    valuename = "LW_PPWR_PMU_IDLE_COUNT_RESET_NOT_DONE";
    if (!(value = field->FindValue(valuename)))
    {
        ErrPrintf("StopClocks: Failed to find %s value\n", valuename);
        return (1);
    }
    reg_data = (reg_data & ~field->GetMask()) |
               (value->GetValue() << field->GetStartBit());
    InfoPrintf("StopClocks: Writing %s(%d) = 0x%08x\n", reg->GetName(), index,
        reg_data);
    lwgpu->RegWr32(reg_addr, reg_data);
    reg_data = lwgpu->RegRd32(reg_addr);
    InfoPrintf("StopClocks: Reading %s(%d) = 0x%08x\n", reg->GetName(), index,
        reg_data);
    valuename = "LW_PPWR_PMU_IDLE_COUNT_RESET_DONE";
    if (!(value = field->FindValue(valuename)))
    {
        ErrPrintf("StopClocks: Failed to find %s value\n", valuename);
        return (1);
    }
    reg_data = (reg_data & ~field->GetMask()) |
               (value->GetValue() << field->GetStartBit());
    InfoPrintf("StopClocks: Writing %s(%d) = 0x%08x\n", reg->GetName(), index,
        reg_data);
    lwgpu->RegWr32(reg_addr, reg_data);

    // Write LW_PPWR_PMU_IDLE_CTRL_VALUE to IDLE
    regname = "LW_PPWR_PMU_IDLE_CTRL";
    if (!(reg = m_regMap->FindRegister(regname)))
    {
        ErrPrintf("StopClocks: Failed to find %s register\n", regname);
        return (1);
    }
    reg_addr = reg->GetAddress(index);
    reg_data = lwgpu->RegRd32(reg_addr);
    InfoPrintf("StopClocks: Reading %s(%d) = 0x%08x\n", reg->GetName(), index,
        reg_data);
    fieldname = "LW_PPWR_PMU_IDLE_CTRL_VALUE";
    if (!(field = reg->FindField(fieldname)))
    {
        ErrPrintf("StopClocks: Failed to find %s field\n", fieldname);
        return (1);
    }
    valuename = "LW_PPWR_PMU_IDLE_CTRL_VALUE_IDLE";
    if (!(value = field->FindValue(valuename)))
    {
        ErrPrintf("StopClocks: Failed to find %s value\n", valuename);
        return (1);
    }
    reg_data = (reg_data & ~field->GetMask()) |
               (value->GetValue() << field->GetStartBit());
    InfoPrintf("StopClocks: Writing %s(%d) = 0x%08x\n", reg->GetName(), index,
        reg_data);
    lwgpu->RegWr32(reg_addr, reg_data);

    return (0);
}

// This routine supports Deep L1 entry.  This code is modified from
// //sw/dev/gpu_drv/chips_a/diag/mods/mdiag/tests/gpu/misc/power_gk107/
//   power_onesrc_kepler_gk107.cpp and there is little documentation on
// the steps it does and why.
INT32
StopClocks::config_gate_ctrl(UINT32 index, const char *mode)
{
    unique_ptr<IRegisterClass> reg;
    unique_ptr<IRegisterField> field;
    unique_ptr<IRegisterValue> value;
    const char *regname, *fieldname, *valuename;
    UINT32 reg_addr, reg_data;

    InfoPrintf("StopClocks: Configuring gate control to %s\n", mode);

    // Write LW_THERM_GATE_CTRL_BLK_CLK to AUTO/RUN
    // Write LW_THERM_GATE_CTRL_ENG_CLK to RUN
    regname = "LW_THERM_GATE_CTRL";
    if (!(reg = m_regMap->FindRegister(regname)))
    {
        ErrPrintf("StopClocks: Failed to find %s register\n", regname);
        return (1);
    }
    reg_addr = reg->GetAddress(index);
    reg_data = lwgpu->RegRd32(reg_addr);
    InfoPrintf("StopClocks: Reading %s(%d) = 0x%08x\n", reg->GetName(), index,
        reg_data);
    fieldname = "LW_THERM_GATE_CTRL_BLK_CLK";
    if (!(field = reg->FindField(fieldname)))
    {
        ErrPrintf("StopClocks: Failed to find %s field\n", fieldname);
        return (1);
    }
    if (strcmp(mode, "AUTO") == 0)
    {
        valuename = "LW_THERM_GATE_CTRL_BLK_CLK_AUTO";
        if (!(value = field->FindValue(valuename)))
        {
            ErrPrintf("StopClocks: Failed to find %s value\n", valuename);
            return (1);
        }
        reg_data = (reg_data & ~field->GetMask()) |
                   (value->GetValue() << field->GetStartBit());
    }
    else if (strcmp(mode, "RUN") == 0)
    {
        valuename = "LW_THERM_GATE_CTRL_BLK_CLK_RUN";
        if (!(value = field->FindValue(valuename)))
        {
            ErrPrintf("StopClocks: Failed to find %s value\n", valuename);
            return (1);
        }
        reg_data = (reg_data & ~field->GetMask()) |
                   (value->GetValue() << field->GetStartBit());
        fieldname = "LW_THERM_GATE_CTRL_ENG_CLK";
        if (!(field = reg->FindField(fieldname)))
        {
            ErrPrintf("StopClocks: Failed to find %s field\n", fieldname);
            return (1);
        }
        valuename = "LW_THERM_GATE_CTRL_ENG_CLK_RUN";
        if (!(value = field->FindValue(valuename)))
        {
            ErrPrintf("StopClocks: Failed to find %s value\n", valuename);
            return (1);
        }
        reg_data = (reg_data & ~field->GetMask()) |
                   (value->GetValue() << field->GetStartBit());
    }
    else
    {
        ErrPrintf("StopClocks: Unknown mode in config_gate_ctrl: %s\n", mode);
        return (1);
    }
    InfoPrintf("StopClocks: Writing %s(%d) = 0x%08x\n", reg->GetName(), index,
        reg_data);
    lwgpu->RegWr32(reg_addr, reg_data);

    return (0);
}

// This routine supports Deep L1 entry.  This code is modified from
// //sw/dev/gpu_drv/chips_a/diag/mods/mdiag/tests/gpu/misc/power_gk107/
//   power_onesrc_kepler_gk107.cpp and there is little documentation on
// the steps it does and why.
INT32
StopClocks::config_xp_power1()
{
    unique_ptr<IRegisterClass> reg;
    unique_ptr<IRegisterField> field;
    unique_ptr<IRegisterValue> value;
    const char *regname, *fieldname, *valuename;
    UINT32 reg_addr, reg_data;

    InfoPrintf("StopClocks: Configuring XP power (part 1)\n");

    // Write LW_XP_PEX_PLL_PWRDN_D0_EN to ENABLED
    // Write LW_XP_PEX_PLL_PWRDN_D1_EN to DISBLED
    // Write LW_XP_PEX_PLL_PWRDN_D2_EN to DISBLED
    // Write LW_XP_PEX_PLL_PWRDN_D3_EN to ENABLED
    // Write LW_XP_PEX_PLL_CLK_REQ_EN to ENABLED
    // Write LW_XP_PEX_PLL_PWRDN_SEQ_START_XCLK_DLY to 0x177
    regname = "LW_XP_PEX_PLL";
    if (!(reg = m_regMap->FindRegister(regname)))
    {
        ErrPrintf("StopClocks: Failed to find %s register\n", regname);
        return (1);
    }
    reg_addr = reg->GetAddress();
    fieldname = "LW_XP_PEX_PLL_PWRDN_D0_EN";
    if (!(field = reg->FindField(fieldname)))
    {
        ErrPrintf("StopClocks: Failed to find %s field\n", fieldname);
        return (1);
    }
    valuename = "LW_XP_PEX_PLL_PWRDN_D0_EN_ENABLED";
    if (!(value = field->FindValue(valuename)))
    {
        ErrPrintf("StopClocks: Failed to find %s value\n", valuename);
        return (1);
    }
    reg_data = (value->GetValue() << field->GetStartBit());
    fieldname = "LW_XP_PEX_PLL_PWRDN_D1_EN";
    if (!(field = reg->FindField(fieldname)))
    {
        ErrPrintf("StopClocks: Failed to find %s field\n", fieldname);
        return (1);
    }
    valuename = "LW_XP_PEX_PLL_PWRDN_D1_EN_DISBLED";
    if (!(value = field->FindValue(valuename)))
    {
        ErrPrintf("StopClocks: Failed to find %s value\n", valuename);
        return (1);
    }
    reg_data = (reg_data & ~field->GetMask()) |
               (value->GetValue() << field->GetStartBit());
    fieldname = "LW_XP_PEX_PLL_PWRDN_D2_EN";
    if (!(field = reg->FindField(fieldname)))
    {
        ErrPrintf("StopClocks: Failed to find %s field\n", fieldname);
        return (1);
    }
    valuename = "LW_XP_PEX_PLL_PWRDN_D2_EN_DISBLED";
    if (!(value = field->FindValue(valuename)))
    {
        ErrPrintf("StopClocks: Failed to find %s value\n", valuename);
        return (1);
    }
    reg_data = (reg_data & ~field->GetMask()) |
               (value->GetValue() << field->GetStartBit());
    fieldname = "LW_XP_PEX_PLL_PWRDN_D3_EN";
    if (!(field = reg->FindField(fieldname)))
    {
        ErrPrintf("StopClocks: Failed to find %s field\n", fieldname);
        return (1);
    }
    valuename = "LW_XP_PEX_PLL_PWRDN_D3_EN_ENABLED";
    if (!(value = field->FindValue(valuename)))
    {
        ErrPrintf("StopClocks: Failed to find %s value\n", valuename);
        return (1);
    }
    reg_data = (reg_data & ~field->GetMask()) |
               (value->GetValue() << field->GetStartBit());
    fieldname = "LW_XP_PEX_PLL_CLK_REQ_EN";
    if (!(field = reg->FindField(fieldname)))
    {
        ErrPrintf("StopClocks: Failed to find %s field\n", fieldname);
        return (1);
    }
    valuename = "LW_XP_PEX_PLL_CLK_REQ_EN_ENABLED";
    if (!(value = field->FindValue(valuename)))
    {
        ErrPrintf("StopClocks: Failed to find %s value\n", valuename);
        return (1);
    }
    reg_data = (reg_data & ~field->GetMask()) |
               (value->GetValue() << field->GetStartBit());
    fieldname = "LW_XP_PEX_PLL_PWRDN_SEQ_START_XCLK_DLY";
    if (!(field = reg->FindField(fieldname)))
    {
        ErrPrintf("StopClocks: Failed to find %s field\n", fieldname);
        return (1);
    }
    reg_data = (reg_data & ~field->GetMask()) |
               ((0x177 << field->GetStartBit()) & field->GetMask());
    InfoPrintf("StopClocks: Writing %s = 0x%08x\n", reg->GetName(), reg_data);
    lwgpu->RegWr32(reg_addr, reg_data);

    return (0);
}

// This routine supports Deep L1 entry.  This code is modified from
// //sw/dev/gpu_drv/chips_a/diag/mods/mdiag/tests/gpu/misc/power_gk107/
//   power_onesrc_kepler_gk107.cpp and there is little documentation on
// the steps it does and why.
INT32
StopClocks::config_xp_power2()
{
    unique_ptr<IRegisterClass> reg;
    unique_ptr<IRegisterField> field;
    unique_ptr<IRegisterValue> value;
    const char *regname, *fieldname, *valuename;
    UINT32 reg_addr, reg_data;

    InfoPrintf("StopClocks: Configuring XP power (part 2)\n");

    // Write LW_XP_PL_PAD_PWRDN_L1 to EN
    // Write LW_XP_PL_PAD_PWRDN_SLEEP_MODE_L1 to L1P
    regname = "LW_XP_PL_PAD_PWRDN";
    if (!(reg = m_regMap->FindRegister(regname)))
    {
        ErrPrintf("StopClocks: Failed to find %s register\n", regname);
        return (1);
    }
    reg_addr = reg->GetAddress();
    reg_data = lwgpu->RegRd32(reg_addr);
    InfoPrintf("StopClocks: Reading %s = 0x%08x\n", reg->GetName(), reg_data);
    fieldname = "LW_XP_PL_PAD_PWRDN_L1";
    if (!(field = reg->FindField(fieldname)))
    {
        ErrPrintf("StopClocks: Failed to find %s field\n", fieldname);
        return (1);
    }
    valuename = "LW_XP_PL_PAD_PWRDN_L1_EN";
    if (!(value = field->FindValue(valuename)))
    {
        ErrPrintf("StopClocks: Failed to find %s value\n", valuename);
        return (1);
    }
    reg_data = (reg_data & ~field->GetMask()) |
               (value->GetValue() << field->GetStartBit());
    fieldname = "LW_XP_PL_PAD_PWRDN_SLEEP_MODE_L1";
    if (!(field = reg->FindField(fieldname)))
    {
        ErrPrintf("StopClocks: Failed to find %s field\n", fieldname);
        return (1);
    }
    valuename = "LW_XP_PL_PAD_PWRDN_SLEEP_MODE_L1_L1P";
    if (!(value = field->FindValue(valuename)))
    {
        ErrPrintf("StopClocks: Failed to find %s value\n", valuename);
        return (1);
    }
    reg_data = (reg_data & ~field->GetMask()) |
               (value->GetValue() << field->GetStartBit());
    InfoPrintf("StopClocks: Writing %s = 0x%08x\n", reg->GetName(), reg_data);
    lwgpu->RegWr32(reg_addr, reg_data);

    return (0);
}

// This routine check the readback of the host interrupt.  This is used to
// confirm that the host interrupt that was intended to generate the wakeup
// is still active.  This is a sanity check that we got the coverage we
// intended.
INT32
StopClocks::check_host_intr(bool active)
{
    unique_ptr<IRegisterClass> reg;
    unique_ptr<IRegisterField> field;
    unique_ptr<IRegisterValue> value;
    char regname[127], fieldname[127], valuename[127];
    UINT32 reg_addr, reg_data, inta_value;

    InfoPrintf("StopClocks: Checking HOST interrupt\n");

    // Check host2xve_intr_nonstall interrupt.
    if (sprintf(regname, "LW_PMC_INTR(0)") < 0)
    {
        ErrPrintf("StopClocks: Failed to generate host intr rd regname\n");
        return (1);
    }
    if (!(reg = m_regMap->FindRegister(regname)))
    {
        ErrPrintf("StopClocks: Failed to find %s register\n", regname);
        return (1);
    }
    reg_addr = reg->GetAddress();
    reg_data = lwgpu->RegRd32(reg_addr);
    InfoPrintf("StopClocks: Reading %s = 0x%08x\n", reg->GetName(),
        reg_data);

    strcpy(fieldname, reg->GetName());
    strcat(fieldname, "_PMGR");
    if (!(field = reg->FindField(fieldname)))
    {
        ErrPrintf("StopClocks: Failed to find %s field\n", fieldname);
        return (1);
    }
    inta_value = (reg_data & field->GetMask()) >> field->GetStartBit();

    strcpy(valuename, field->GetName());
    if (active)
    {
        strcat(valuename, "_PENDING");
    }
    else
    {
        strcat(valuename, "_NOT_PENDING");
    }
    if (!(value = field->FindValue(valuename)))
    {
        ErrPrintf("StopClocks: Failed to find %s value\n", valuename);
        return (1);
    }

    if (inta_value != value->GetValue())
    {
        ErrPrintf("StopClocks: Detected mismatch in %s: expected %d, "
            "actual %d\n", field->GetName(), value->GetValue(), inta_value);
        m_ErrCount++;
    }

    return (0);
}

// This routine clears the GPIO interrupt.  Although we only expect RISING
// interrupt to be present, we clear RISING and FALLING interrupts for good
// measure.
INT32
StopClocks::clear_gpio_intr (UINT32 gpio)
{
    unique_ptr<IRegisterClass> reg;
    unique_ptr<IRegisterField> field;
    unique_ptr<IRegisterValue> value;
    char regname[127], fieldname[127], valuename[127];
    UINT32 reg_addr, reg_data;
    UINT32 list, index_edge;
    const char *edges[] = {"RISING", "FALLING"};
    const UINT32 num_edges = sizeof(edges)/sizeof(char*);

    InfoPrintf("StopClocks: Clearing GPIO %d interrupts\n", gpio);

    list = (gpio >> 4) + 1;
    if (sprintf(regname, "LW_PMGR_RM_INTR_GPIO_LIST_%d", list) < 0)
    {
        ErrPrintf("StopClocks: Failed to generate GPIO intr regname\n");
        return (1);
    }
    if (!(reg = m_regMap->FindRegister(regname)))
    {
        ErrPrintf("StopClocks: Failed to find %s register\n", regname);
        return (1);
    }
    reg_addr = reg->GetAddress();
    reg_data = lwgpu->RegRd32(reg_addr);
    InfoPrintf("StopClocks: Reading %s = 0x%08x\n",reg->GetName(),reg_data);

    // Clear both RISING and FALLING edges for good measure.
    for (index_edge = 0; index_edge < num_edges; index_edge++)
    {
        // Clear GPIO interrupt
        if (sprintf(fieldname, "%s_GPIO%d_%s", reg->GetName(), gpio,
            edges[index_edge]) < 0)
        {
            ErrPrintf("StopClocks: Failed to generate GPIO intr fieldname\n");
            return (1);
        }
        if (!(field = reg->FindField(fieldname)))
        {
            ErrPrintf("StopClocks: Failed to find %s field\n", fieldname);
            return (1);
        }
        if (sprintf(valuename, "%s_RESET", field->GetName()) < 0)
        {
            ErrPrintf("StopClocks: Failed to generate GPIO intr valuename\n");
            return (1);
        }
        if (!(value = field->FindValue(valuename)))
        {
            ErrPrintf("StopClocks: Failed to find %s value\n", valuename);
            return (1);
        }
        reg_data = (reg_data & ~field->GetMask()) |
                   (value->GetValue() << field->GetStartBit());
    }

    // Write configuration back to register
    InfoPrintf("StopClocks: Writing %s = 0x%08x\n", reg->GetName(), reg_data);
    lwgpu->RegWr32(reg_addr, reg_data);

    return (0);
}

// This routine gets the GPIO state as seen by the testbench.
UINT32
StopClocks::get_gpio_state (UINT32 gpio)
{
    char escapename[8];
    const char *gpio_data_char;
    UINT32 gpio_data;

    if (sprintf(escapename, "Gpio_%d", gpio) < 0)
    {
        ErrPrintf("StopClocks: Failed to generate GPIO escape name\n");
        m_ErrCount++;
        return (0);
    }
    Platform::EscapeRead(escapename, 0, 4, &gpio_data);
    switch (gpio_data)
    {
        case GPIO_RD_LOW :
            gpio_data_char = "0";
            break;
        case GPIO_RD_HIGH :
            gpio_data_char = "1";
            break;
        case GPIO_RD_HIZ :
            gpio_data_char = "Z";
            break;
        case GPIO_RD_X :
            gpio_data_char = "X";
            break;
        default :
            ErrPrintf("StopClocks: Illegal value detected in %s: %d\n",
                escapename, gpio_data);
            m_ErrCount++;
            return (0);
            break;
    }
    InfoPrintf("StopClocks: EscapeRead of %s: %s\n", escapename,
        gpio_data_char);

    return (gpio_data);
}

// This routine polls the GPIO state until it matches the expected state.
// This is used to synchronize the test with PMU ucode.
INT32
StopClocks::poll_gpio_state (UINT32 gpio, UINT32 state)
{
    UINT32 gpio_state, polling_attempts;
    bool done;

    InfoPrintf("StopClocks: Polling Gpio_%d\n", gpio);

    // Poll for GPIO state
    polling_attempts = 1;
    gpio_state = get_gpio_state(gpio);
    done = (gpio_state == state);
    while (!done && (polling_attempts < m_max_polling_attempts))
    {
        if ((polling_attempts % 10) == 1)
        {
            InfoPrintf("StopClocks: Polling for GPIO_%d state to be %d: "
                "attempt %d\n", gpio, state, polling_attempts);
        }
        Platform::Delay(1);
        polling_attempts = polling_attempts + 1;
        gpio_state = get_gpio_state(gpio);
        done = (gpio_state == state);
    }
    InfoPrintf("StopClocks: Done polling for GPIO_%d state to be %d: "
        "attempt %d\n", gpio, state, polling_attempts);
    if (!done)
    {
        ErrPrintf("StopClocks: Timeout polling for GPIO_%d state to be %d "
            "after %d attempts\n", gpio, state, polling_attempts);
        return (1);
    }

    return (0);
}

// This routine starts the clock monitors, waits for the monitors to complete,
// and verifies the clock counts against the expected frequency.
INT32
StopClocks::check_xclk_freq (UINT32 interval_ns, INT32 exp_freq_khz)
{
    const char *clk_mon_ref_cnt_name, *clk_mon_reset_name, *clk_mon_done_name,
        *clk_mon_count_name, *clk_mon_enable_name;
    UINT32 clk_mon_ref_cnt, clk_mon_done, clk_mon_count;
    UINT32 act_interval_ns, check_data, delay_us;
    INT32 act_freq_khz, tolerance_khz;
    float clk_count_error;

    InfoPrintf("StopClocks: Checking xclk clock monitor\n");

    clk_mon_ref_cnt_name = "LW_SYS0_CORE_CLKS_clk_mon_ref_cnt_out_gpu";
    clk_mon_reset_name   = "LW_SYS0_CORE_CLKS_clk_mon_reset_gpu";
    clk_mon_done_name    = "LW_SYS0_CORE_CLKS_clk_mon_done_gpu";
    clk_mon_count_name   = "clk_xclk_alt_gpu_count_LW_SYS0_CORE_CLKS";
    clk_mon_enable_name   = "LW_SYS0_CLK_HIER_clk_mon_enable_gpu";

    // Now we are ready to start the clock monitor.
    // First, configure the clock monitor interval.
    // The granularity of the monitor interval is in units of 2 ns.  We
    // program the value of (interval_ns / 2) - 1 into the clock monitor.
    clk_mon_ref_cnt = (interval_ns / 2) - 1;
    act_interval_ns = (clk_mon_ref_cnt + 1) * 2;
    InfoPrintf("StopClocks: Configuring clock monitor interval to %d ns\n",
        act_interval_ns);

    Platform::EscapeWrite(clk_mon_enable_name, 0, 4, 1);
    Platform::EscapeWrite(clk_mon_ref_cnt_name, 0, 4, clk_mon_ref_cnt);
    Platform::EscapeRead(clk_mon_ref_cnt_name, 0, 4, &check_data);
    if (check_data != clk_mon_ref_cnt)
    {
        ErrPrintf("StopClocks: Failure in EscapeWrite of %s.  Wrote %d, "
            "Read %d\n", clk_mon_ref_cnt_name, clk_mon_ref_cnt, check_data);
        m_ErrCount++;
    }

    // Second, toggle clk_mon_reset to 1->0 to start clock monitor.  We need
    // to insert a delay between the two escape writes so that the RTL will
    // see the transition.
    Platform::EscapeWrite(clk_mon_reset_name, 0, 4, 1);
    Platform::EscapeRead(clk_mon_reset_name, 0, 4, &check_data);
    if (check_data != 1)
    {
        ErrPrintf("StopClocks: Failure in EscapeWrite of %s.  Wrote %d, "
            "Read %d\n", clk_mon_reset_name, 1, check_data);
        m_ErrCount++;
    }
    Platform::Delay(1);
    Platform::EscapeWrite(clk_mon_reset_name, 0, 4, 0);
    Platform::EscapeRead(clk_mon_reset_name, 0, 4, &check_data);
    if (check_data != 0)
    {
        ErrPrintf("StopClocks: Failure in EscapeWrite of %s.  Wrote %d, "
            "Read %d\n", clk_mon_reset_name, 0, check_data);
        m_ErrCount++;
    }
    InfoPrintf("StopClocks: Starting clock monitor\n");

    // Third, wait for clock monitor to complete.  We will delay for the
    // configured interval, plus 1 us for good measure.
    delay_us = (act_interval_ns / 1000) + 1;
    Platform::Delay(delay_us);

    // Fourth, verify that clock monitor completed.
    Platform::EscapeRead(clk_mon_done_name, 0, 4, &clk_mon_done);
    if (!clk_mon_done)
    {
        ErrPrintf("StopClocks: Clock monitor %s not done after %d us\n",
            clk_mon_done_name, delay_us);
        m_ErrCount++;
    }

    // Last, read the clock count and compare against the expected frequency.
    Platform::EscapeRead(clk_mon_count_name, 0, 4, &clk_mon_count);
    act_freq_khz = (INT32)(1000000.0 * (float)clk_mon_count /
        (float)act_interval_ns);

    // Since the clock is asynchronous with the clock monitoring interval,
    // we must allow a frequency tolerance of +/-1 clk_count and 2 ns.
    // First, estimate the number of clock counts in 2 ns, then add 1 to it.
    clk_count_error = (2.0f * (float)clk_mon_count / (float)act_interval_ns) +
        1.0f;
    // Second, colwert the clock count error into frequency tolerance.
    tolerance_khz = INT32(1000000.0 * clk_count_error / (float)act_interval_ns);

    // Now we can check the actual clock frequency against the expected
    // clock frequency, taking account the error tolerances of the clock
    // monitor.
    if ((act_freq_khz > (exp_freq_khz + tolerance_khz)) ||
        (act_freq_khz < (exp_freq_khz - tolerance_khz)))
    {
        ErrPrintf("StopClocks: Mismatch in xclk:%s: expected %d kHz, "
            "actual %d kHz, tolerance %d kHz\n", clk_mon_count_name,
            exp_freq_khz, act_freq_khz, tolerance_khz);
        m_ErrCount++;
    }
    else
    {
        InfoPrintf("StopClocks: Found matching xclk:%s: expected %d kHz, "
            "actual %d kHz, tolerance %d kHz\n", clk_mon_count_name,
            exp_freq_khz, act_freq_khz, tolerance_khz);
    }
    InfoPrintf("StopClocks: Done with xclk clock monitor\n");

    return (0);
}

// This routine check the sleep status.  We expect to be out of the sleep
// state and that we woke up due the the expected wake_event.
INT32
StopClocks::check_sleep_status(const char *wake_event)
{
    unique_ptr<IRegisterClass> reg;
    unique_ptr<IRegisterField> field;
    unique_ptr<IRegisterValue> value;
    char regname[127], fieldname[127], valuename[127];
    UINT32 reg_addr, reg_data, sleep_req_value, wake_event_value;

    InfoPrintf("StopClocks: Checking sleep controller wake reason: %s\n",
        wake_event);

    // First, make sure sleep controller is done.  The WAKE_REASON is not
    // valid when the sleep controller is still gating clocks.
    strcpy(regname, "LW_THERM_SLEEP_CTRL_0");
    if (!(reg = m_regMap->FindRegister(regname)))
    {
        ErrPrintf("StopClocks: Failed to find %s register\n", regname);
        return (1);
    }
    reg_addr = reg->GetAddress();
    reg_data = lwgpu->RegRd32(reg_addr);
    InfoPrintf("StopClocks: Reading %s = 0x%08x\n", reg->GetName(), reg_data);
    strcpy(fieldname, "LW_THERM_SLEEP_CTRL_0_SLEEP_REQ");
    if (!(field = reg->FindField(fieldname)))
    {
        ErrPrintf("StopClocks: Failed to find %s field\n", fieldname);
        return (1);
    }
    sleep_req_value = (reg_data & field->GetMask()) >> field->GetStartBit();
    strcpy(valuename, "LW_THERM_SLEEP_CTRL_0_SLEEP_REQ_NOT_PENDING");
    if (!(value = field->FindValue(valuename)))
    {
        ErrPrintf("StopClocks: Failed to find %s value\n", valuename);
        return (1);
    }
    if (sleep_req_value != value->GetValue())
    {
        ErrPrintf("StopClocks: Detected mismatch in %s: expected %d, actual "
            "%d\n", field->GetName(), value->GetValue(), sleep_req_value);
        m_ErrCount++;
    }

    // Now that we confirmed the Sleep Controller has waken up, we can check
    // the wake reason.
    strcpy(regname, "LW_THERM_SLEEP_STATUS");
    if (!(reg = m_regMap->FindRegister(regname)))
    {
        ErrPrintf("StopClocks: Failed to find %s register\n", regname);
        return (1);
    }
    reg_addr = reg->GetAddress();
    reg_data = lwgpu->RegRd32(reg_addr);
    InfoPrintf("StopClocks: Reading %s = 0x%08x\n", reg->GetName(), reg_data);

    strcpy(fieldname, reg->GetName());
    strcat(fieldname, "_WAKE_REASON_");
    strcat(fieldname, wake_event);
    if (!(field = reg->FindField(fieldname)))
    {
        ErrPrintf("StopClocks: Failed to find %s field\n", fieldname);
        return (1);
    }
    wake_event_value = (reg_data & field->GetMask()) >> field->GetStartBit();

    strcpy(valuename, field->GetName());
    strcat(valuename, "_YES");
    if (!(value = field->FindValue(valuename)))
    {
        ErrPrintf("StopClocks: Failed to find %s value\n", valuename);
        return (1);
    }

    if (wake_event_value != value->GetValue())
    {
        ErrPrintf("StopClocks: Detected mismatch in %s: expected %d, actual "
            "%d\n", field->GetName(), value->GetValue(), wake_event_value);
        m_ErrCount++;
    }

    return (0);
}

