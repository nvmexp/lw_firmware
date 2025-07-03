/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2008-2009, 2020-2021 by LWPU Corporation.  All rights reserved. 
 * All information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */
// To facilitate faster compiles via pre-compiled headers, keep "stdtest.h" first
#include "mdiag/tests/stdtest.h"

#include "uart2icd_pwr.h"
#include "mdiag/lwgpu.h"
#include "mdiag/utils/lwgpu_channel.h"
#include "mdiag/tests/test_state_report.h"
#include "sim/IChip.h"

#define LW_CPWR_PMU_UART_TX                0x0660
#define LW_CPWR_PMU_UART_RX                0x0664
#define LW_CPWR_PMU_UART_CTL               0x0668
#define LW_CPWR_PMU_UART_THRESHOLD         0x066c
#define LW_CPWR_PMU_UART_INTR              0x0670
#define LW_CPWR_PMU_UART_INTR_EN           0x0674
#define LW_CPWR_PMU_UART_DEBUG             0x0678
#define LW_CPWR_PMU_UART_DEBUG_CTL         0x067c

#define LW_PPWR_FALCON_MAILBOX0            0x0010a040

// define which tests are run
static int uart2icd_sanity=0;

extern const ParamDecl uart2icd_pwr_params[] =
{
    SIMPLE_PARAM("-uart2icd_sanity", "sanity test for uart2icd"),
    PARAM_SUBDECL(lwgpu_single_params),
    LAST_PARAM
};

// constructor
uart2icd_pwr::uart2icd_pwr(ArgReader *params) :
    LWGpuSingleChannelTest(params)
{
    if (params->ParamPresent("-uart2icd_sanity"))
        uart2icd_sanity = 1;
}

// destructor
uart2icd_pwr::~uart2icd_pwr(void)
{
}

// Factory
STD_TEST_FACTORY(uart2icd_pwr,uart2icd_pwr)

// setup method
int uart2icd_pwr::Setup(void)
{
    lwgpu = LWGpuResource::FindFirstResource();
    ch = lwgpu->CreateChannel();
    if(!ch)
    {
        ErrPrintf("uart2icd_pwr::Setup failed to create channel\n");
        return 0;
    }
    getStateReport()->init("uart2icd_pwr");
    getStateReport()->enable();

    return 1;
}

//CleanUp
void uart2icd_pwr::CleanUp(void)
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

void uart2icd_pwr::DelayNs(UINT32 Lwclks)
{
  // Lwrrently this function is in terms of .2nS / lwclk. This is incorrectly done
  // in RTL. This should be a close approx of 1 nanosecond.
  Platform::EscapeWrite("CLOCK_WAIT",IChip::ECLOCK_LWCLK,0,Lwclks * 5);
}

// run - main routine
void uart2icd_pwr::Run(void)
{
    //  InfoPrintf Flag to determine whether the test fail because of elw issue or test issue.
    InfoPrintf("FLAG : uart2icd_pwr starting ....\n");

    if(uart2icd_sanity)
    {
        if (uart2icd_sanityTest())
        {
        SetStatus(TEST_FAILED);
        ErrPrintf("uart2icd_pwr::uart2icd_sanityTest failed\n");
        return;
        }

    InfoPrintf("uart2icd_sanityTest() Done !\n");
    } //endif_uart2icd_sanityTest

    InfoPrintf("uart2icd_pwr test complete\n");
    SetStatus(TEST_SUCCEEDED);
    getStateReport()->crcCheckPassedGold();

    ch->WaitForIdle();
}//end_Run

int uart2icd_pwr::uart2icd_sanityTest()
{
    UINT32 data = 0;
    InfoPrintf("uart2icd_pwr test starts...\n");
    //initial the test

    /* Steps to generate uart2icd_pwr_imem_content and uart2icd_pwr_dmem_content
       - Step 1: Compile source code into binary
         In //hw/lwgpu_pwr/ip/power/pwr/1.0/stand_sim/private/cc_tests/pmu/fullchip/uart2icd_pwr
         ~> make all FULLCHIP=1

         This will generate two files:
         App_fullchip.0x00000000 (imem code)
         App_fullchip.0x10000000 (dmem data)

       - Step 2: Colwert to memory format and generate image header file
         In //hw/lwgpu_pwr/ip/power/pwr/1.0/stand_sim/private/cc_tests/pmu/fullchip/uart2icd_pwr
         ~> //hw/lwgpu/tools/mods/trace_3d/plugins/ba_sampling/bin2mem.pl -i App_fullchip.0x00000000 -o App_fullchip.0x00000000.mem
         ~> //hw/lwgpu/tools/mods/trace_3d/plugins/ba_sampling/bin2mem.pl -i App_fullchip.0x10000000 -o App_fullchip.0x10000000.mem

       - Step 3: Colwert mem file into header file
         In //hw/lwgpu_pwr/ip/power/pwr/1.0/stand_sim/private/cc_tests/pmu/fullchip/uart2icd_pwr
         ~> //hw/lwgpu/tools/mods/trace_3d/plugins/ba_sampling/gen_img.pl -aname uart2icd_pwr_imem_content -tname uart2icd_pwr_code -image App_fullchip.0x00000000.mem -elfbin App_fullchip.out
         ~> //hw/lwgpu/tools/mods/trace_3d/plugins/ba_sampling/gen_img.pl -aname uart2icd_pwr_dmem_content -tname uart2icd_pwr_data -image App_fullchip.0x10000000.mem -elfbin App_fullchip.out

         This will generate two files:
         code_img_uart2icd_pwr_code.h
         code_img_uart2icd_pwr_data.h

         - Step 4: Copy uart2icd_pwr_imem_content/uart2icd_pwr_dmem_content from code_img_uart2icd_pwr_code.h/code_img_uart2icd_pwr_data.h respectively
     */

     UINT32 uart2icd_pwr_imem_content[]={
         0x0849f4bd, 0x9f9fff01, 0x010999e7, 0xfe0894b6, 0xa77e0094, 0x02f80004, 0x000000f8, 0x00000000,
         0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000,
         0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000,
         0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000,
         0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000,
         0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000,
         0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000,
         0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000,
         0x001012df, 0x06684901, 0x49009ffa, 0xffb8066c, 0xfa021013, 0x010f009f, 0xfa042449, 0x004f009f,
         0x07e04910, 0x0b009ffa, 0xd0548a01, 0x09b97e00, 0xc5010c00, 0x548a08ab, 0x717e00d0, 0xef8f0009,
         0x440900be, 0xf8009ffa, 0xf900f802, 0xf9a0f990, 0xf9c0f9b0, 0xf9e0f9d0, 0x018afef0, 0x4089a0f9,
         0x9abf0004, 0xa0fca5f9, 0xfc00a8fe, 0xfce0fcf0, 0xfcc0fcd0, 0xfca0fcb0, 0xf801f890, 0x02428900,
         0x0093fe00, 0x00021489, 0x890091fe, 0xfe00014b, 0x008f0090, 0x1c090800, 0x4f009ffa, 0x10090808,
         0xf4009ffa, 0x31f41131, 0x8a00f810, 0x7e0002c5, 0x89000270, 0xfe000242, 0x14890093, 0x91fe0002,
         0x014b8900, 0x0090fe00, 0x0800008f, 0x9ffa1c09, 0x48fb4f00, 0x9ffa1009, 0x49ff0f00, 0x9ffa04c4,
         0x07d44900, 0x49009ffa, 0x9ffa07d8, 0x0000df00, 0x84491000, 0x009ffa06, 0x6049ff0f, 0x009ffa07,
         0xfa076449, 0x7049009f, 0x009ffa07, 0xf41131f4, 0x00f81031, 0xa0f990f9, 0xc0f9b0f9, 0xe0f9d0f9,
         0x8afef0f9, 0x7ea0f901, 0xfc000278, 0x00a8fea0, 0xe0fcf0fc, 0xc0fcd0fc, 0xa0fcb0fc, 0x01f890fc,
         0x90f900f8, 0xb0f9a0f9, 0xd0f9c0f9, 0xf0f9e0f9, 0xf9018afe, 0x02af7ea0, 0xfea0fc00, 0xf0fc00a8,
         0xd0fce0fc, 0xb0fcc0fc, 0x90fca0fc, 0x00f801f8, 0x00044089, 0x00f89aa0, 0x080f94bd, 0xffbff9ff,
         0x0bf494bf, 0x09020f1d, 0x009ffa58, 0x0409080f, 0x8f019ffa, 0x49010000, 0x9ffa0090, 0x8a00f801,
         0xbd0000a4, 0xbdd4bdc4, 0x04e77ee4, 0xf400f800, 0xcbfe1832, 0x00c88a01, 0xbdc4bd00, 0x7ee4bdd4,
         0xf80004e7, 0xbd82f900, 0xff0809f4, 0xfe0b7f9f, 0x0000e08a, 0xbdb47bff, 0xbdd4bdc4, 0x059e7ee4,
         0x0079e400, 0x6b0bf440, 0x4901004f, 0x9ffa0680, 0x40004f00, 0x9ffa0409, 0x4294bd00, 0x29ff04e4,
         0x04e0412f, 0x401f19ff, 0x09ff04e8, 0x00fd8a0f, 0xbdb4bd00, 0xbdd4bdc4, 0x04e77ee4, 0xb22bb200,
         0x8a0db21c, 0xbd000108, 0x04e77ee4, 0xbd7bb200, 0xbdd4bdc4, 0x012c8ae4, 0x04e77e00, 0x0a010b00,
         0x0ac17e0e, 0xbeefdf00, 0x5849beef, 0x009ffa04, 0x200079e4, 0x0b720bf4, 0x7e0d0a01, 0x49000ac1,
         0xc4bd07cc, 0xfacf9cff, 0xd049009c, 0xffd4bd07, 0x9dfadf9d, 0x07ec4900, 0x92ff24bd, 0x0092fa2f,
         0xbd07f049, 0x1f91ff14, 0xbd0091fa, 0x07c44e94, 0x40efe9ff, 0x09ff07e8, 0x0240830f, 0x8a3bbf00,
         0x7e00013d, 0xb20004e7, 0x016c8a2b, 0xb21cb200, 0x7ee4bd0d, 0xbf0004e7, 0xb2030a39, 0x0199909b,
         0x977e39a0, 0x79e4000a, 0x0bf50800, 0xb4bd00ce, 0xff068849, 0x69c46f9b, 0x7b0bf402, 0xff04c049,
         0x968a5f9b, 0x5bb20001, 0xd4bdc4bd, 0x34bde4bd, 0x0004e77e, 0xbd082042, 0x9753bc84, 0xf40194f0,
         0x29923d0b, 0x1f98ff20, 0xb24f28ff, 0x04243e40, 0xb20cbf00, 0x01ac8a0b, 0x04009000, 0xe4bdd4bd,
         0x0004e77e, 0x1ef401a6, 0x0021faeb, 0x0001bd8a, 0x1db24cb2, 0xe4bd3bb2, 0x0004e77e, 0x90013390,
         0x34b30422, 0xc049b708, 0x0095fa04, 0x010b120a, 0x000ac17e, 0xf41069c4, 0xb0413b0b, 0xfff4bd07,
         0x4880cf1f, 0xac490004, 0xff0ca007, 0x448f9f9f, 0x9bb20004, 0xe88af9a0, 0xd4bd0001, 0x9e7ee4bd,
         0x09bf0005, 0x4f0019fa, 0x94bd07b4, 0xfa9ff9ff, 0x884900f9, 0x0096fa06, 0x97fa0409, 0x7e0a0a01,
         0xbd00083d, 0x7e81fba4, 0x7e0006ac, 0x890001ab, 0x18000300, 0xe089019a, 0x9b980002, 0x80008c01,
         0xffa4f000, 0x7e08b5b6, 0x7e0004d3, 0xbd000100, 0x8900f8a4, 0xa0000310, 0x0314899a, 0x899ba000,
         0xa0000318, 0xf900f89c, 0xfea1b242, 0xff890183, 0x39fffcff, 0x0098fe94, 0x8901b2fe, 0xbf000310,
         0x8fff499f, 0xb69429ff, 0x9ffd0cf4, 0x009bfe05, 0x890174fe, 0xbf000314, 0x0097fe99, 0x1c8a03f8,
         0xa0bf0003, 0x0002808f, 0xb504feb5, 0x099001fb, 0x02fcb501, 0xa003fdb5, 0xbdf0a0a9, 0x02808fe4,
         0x90efbc00, 0x90f81e3c, 0x9f3501ee, 0x2be0b314, 0x981e3c0b, 0xe9009433, 0x9489f43d, 0x208d0002,
         0x9f3c0003, 0x00008fe9, 0x89debf04, 0xfd000280, 0xe9fa059f, 0x8fd9bf06, 0xbf000318, 0x409990ff,
         0x9fa6d9a0, 0xbd0708f4, 0xfed9a094, 0x2bfe0047, 0x0038fe00, 0xc08941fb, 0x9aa00002, 0x12f900f8,
         0x890181fe, 0xfffcffff, 0x98fe9419, 0x03248900, 0x8990bf00, 0xbf0002c0, 0x0000d99f, 0x09ffbeef,
         0x00f9fa95, 0xfa00fbfa, 0xfdfa00fc, 0x00fefa00, 0x0005ff3e, 0xec94a93f, 0x01ee9003, 0x0a009433,
         0xf83e0109, 0x92f00005, 0x01aa9007, 0xfd049cbb, 0xe4b305d9, 0x94bde204, 0xb300fdfa, 0xbd0c0094,
         0x3ed4bde4, 0x900005d4, 0x2489010f, 0x9fa00003, 0xfb0018fe, 0x24004911, 0x9f20f43d, 0xb3019990,
         0xfb24409e, 0x204ce4bd, 0x20004d08, 0xcdfa400b, 0x90eebc00, 0xb80294b6, 0x00240499, 0x7501ef90,
         0x9b60019d, 0xb803f994, 0x00240099, 0xcc909e20, 0x90feb204, 0x004f40dd, 0x04e4b324, 0x75947dd5,
         0xf97512fb, 0x16f97517, 0x7528f935, 0xc84913ff, 0x24304f04, 0x4f009ffa, 0xcc492400, 0x009ffa04,
         0x1c09400f, 0xbd019ffa, 0x009ffa94, 0x30f400f8, 0xfe94bdfc, 0x4efe014f, 0xbff9a001, 0x01f990ef,
         0xfaa6e9a0, 0xf4f71ef4, 0x00f80430, 0x0003308f, 0xf9a094bd, 0x0003348f, 0x00f8f9a0, 0x94bd03f8,
         0xfe0097fe, 0x004f01b9, 0x07c4f080, 0xb6049ffd, 0x9cfd0cc4, 0x009bfe05, 0xfd10b4b6, 0xdbfa05ba,
         0x8900f806, 0xbf000334, 0xb272f99f, 0xbc04bd97, 0xf19590aa, 0x0194b602, 0x9002a4b6, 0xa3900394,
         0x01129001, 0x00033086, 0x00000085, 0x0007343e, 0xbf9854bc, 0xc853bc6d, 0x0b040a94, 0x08d4b602,
         0x89d0d9bc, 0xbc000340, 0x0090d0da, 0xa0a9bc01, 0x0006bc7e, 0x1fb279bf, 0xf40394f0, 0x2fb2050b,
         0x0003308e, 0x1ef40fa6, 0x90e9bfca, 0xe9a00199, 0x348971fb, 0x9fbf0003, 0x94b242f9, 0xf19504bd,
         0x03308302, 0x01129000, 0x0007903e, 0x00893dbf, 0x0a940100, 0xb6020b04, 0xd9bc08d4, 0x034089d0,
         0xd0dabc00, 0xbc010090, 0xbcb2a0a9, 0x0006bc7e, 0x1fb249bf, 0xf40394f0, 0x2fb2050b, 0x0003308e,
         0x1ef40fa6, 0x90e9bfca, 0xe9a00199, 0x02f941fb, 0x00033480, 0x408e09bf, 0x9f900003, 0x0294b601,
         0x9ebc0fa0, 0xb39aa090, 0xb20e40f4, 0x07527eba, 0xa094bd00, 0xf901fb09, 0x03348002, 0x8e09bf00,
         0x90000340, 0x94b6019f, 0xbc0fa002, 0x9aa0909e, 0x0c40f4b3, 0x0007527e, 0x09a094bd, 0x804f01fb,
         0xfa100900, 0xbadf009f, 0x090badca, 0x009ffa40, 0xf40031f4, 0x00890028, 0x0c8f0000, 0x9dbf0000,
         0x004ce4bd, 0x08363eff, 0x90f9bf00, 0x9cfd01ee, 0x90f9a004, 0xeda610ff, 0xf8f11ef4, 0xbd2c0f00,
         0xdff9ff94, 0xbc9ff9ff, 0xfbb2e29d, 0xbc90aabc, 0x9f94909a, 0xbcc4bd05, 0x9abc909f, 0x08673ef0,
         0x9fbcff00, 0xa6e29dbc, 0xf808f4ef, 0x04da00f8, 0x0b03ff00, 0x7ec4bd01, 0xda000971, 0x03ff0000,
         0xbd00804b, 0x09717ec4, 0xb200f800, 0xdac4bdab, 0x03ff00a8, 0x0008bf7e, 0x0b8a00f8, 0xb4bd0002,
         0xd4bdc4bd, 0xe77ee4bd, 0xf4bd0004, 0xfa042049, 0x010f019f, 0x9fff1409, 0xf803f89f, 0xf000f802,
         0xa04901c4, 0x1bc4b607, 0xfa05cafd, 0xa449009c, 0x009bfa07, 0x0100f28f, 0xfa07ac49, 0x00f8009f,
         0xffffffd9, 0x04a9fd03, 0xf0019990, 0xa9fd01c4, 0x1bc4b605, 0xfd07a049, 0x9afa05ac, 0x07a44900,
         0x8f009bfa, 0x490100f2, 0x9ffa07ac, 0xd900f800, 0x03ffffff, 0x7e04a9fd, 0x4e0008bf, 0xf4bd07ac,
         0xf19fefff, 0xb3700094, 0x0a20009a, 0x40009eb3, 0xf80bf809, 0x009ab300, 0x00f8eb10, 0xffffffd9,
         0x94a9ff03, 0x000000da, 0xa59aff04, 0x0008bf7e, 0xbd07ac4e, 0x9feffff4, 0x700094f1, 0x20009ab3,
         0x009eb30a, 0x0bf80940, 0x9ab300f8, 0xf8eb1000, 0x08bf7e00, 0x07ac4e00, 0xeffff4bd, 0x0094f19f,
         0x009ab370, 0x9eb30a20, 0xf8094000, 0xb300f80b, 0xeb10009a, 0xffd900f8, 0xf003ffff, 0xa9fd01b4,
         0x1bb4b604, 0xfd07a049, 0x9bfa05ba, 0x00f18f00, 0x07ac4901, 0xf8009ffa, 0x09967e00, 0x07ac4e00,
         0xeffff4bd, 0x0094f19f, 0x009ab370, 0x9eb30a20, 0xf80b4000, 0x09de3e0b, 0x009ab300, 0x94bde910,
         0xff07a44a, 0x00f8afa9, 0x0009967e, 0xbd07ac4e, 0x9feffff4, 0x700094f1, 0x20009ab3, 0x009eb30a,
         0x0bf81040, 0x40009ab3, 0x0a393e12, 0x009ab300, 0x393ee410, 0xe4bd000a, 0xff07b049, 0x00df9f9e,
         0xff800000, 0x0bf4a49f, 0x0988490b, 0xf8af9eff, 0x07a44900, 0xf8af9aff, 0x49f4bd00, 0x9fff07a4,
         0xd900f8af, 0x10000000, 0xfd01b4f0, 0xb4b605a9, 0x05abfd1b, 0x00f8aabf, 0xd901c4f0, 0x10000000,
         0xfd1bc4b6, 0xacfd05a9, 0xf8aba005, 0x01c4f000, 0x000000d9, 0x1bc4b610, 0xfd05a9fd, 0xaba005ac,
         0xc4f000f8, 0x0000d901, 0xc4b61400, 0x05a9fd1b, 0xa005acfd, 0xb800f8ab, 0x000114aa, 0xfa02a4b6,
         0x00f800ab, 0x0114aab8, 0xb694bd00, 0xa9ff02a4, 0xf0aba6af, 0x00f80bac, 0x00044c89, 0xf8a89abc,
         0x044c8900, 0xa99bbc00, 0x000000f8, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000,
         0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000};
    
    UINT32 uart2icd_pwr_dmem_content[]={
        0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000,
        0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000,
        0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000,
        0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000,
        0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000,
        0x00000000, 0x4f525245, 0x55203a52, 0x6f6e6b6e, 0x49206e77, 0x3a315152, 0x61747320, 0x203d2074,
        0x30257830, 0x000a7838, 0x65637845, 0x6f697470, 0x65203a6e, 0x3d696378, 0x30257830, 0x000a7838,
        0x49206e49, 0x48205152, 0x6c646e61, 0x203a7265, 0x74617473, 0x2578303d, 0x0a783830, 0x6d695400,
        0x49207265, 0x003a5152, 0x3d746e63, 0x30257830, 0x692c7838, 0x6c76746e, 0x2578303d, 0x2c783830,
        0x3d6c7463, 0x30257830, 0x000a7838, 0x72746e49, 0x74617453, 0x2578303d, 0x0a783830, 0x204f4900,
        0x20515249, 0x29642528, 0x3072203a, 0x2578303d, 0x2c783830, 0x3d306620, 0x30257830, 0x202c7838,
        0x3d306e69, 0x30257830, 0x000a7838, 0x49204f49, 0x203a5152, 0x303d3172, 0x38302578, 0x66202c78,
        0x78303d31, 0x78383025, 0x6e69202c, 0x78303d31, 0x78383025, 0x4d43000a, 0x52492044, 0x53203a51,
        0x3d544154, 0x30257830, 0x000a7838, 0x25783028, 0x29783830, 0x2578303d, 0x0a783830, 0x64705500,
        0x20657461, 0x6c696174, 0x20642520, 0x6d6f7266, 0x25783020, 0x20783830, 0x68206f74, 0x20646165,
        0x30257830, 0x000a7838, 0x30524142, 0x51524920, 0x5453203a, 0x303d5441, 0x38302578, 0x65202c78,
        0x303d7272, 0x38302578, 0x54000a78, 0x5f545345, 0x0a444e45, 0x00000000, 0x00000000, 0x00000000,
        0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000,
        0x00000001, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000,
        0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000,
        0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000,
        0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000,
        0x000005cc, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000,
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
        0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000,
        0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000,
        0x00000000, 0x00000000, 0x00000000};
    
    // configure DMACTL to disable CTX
    lwgpu->RegWr32(0x0010a10c, 0x0); // LW_PPWR_FALCON_DMACTL (0x0010a10c)

    // loading IMEM/DMEM through PRIV
    UINT32 code_size = sizeof(uart2icd_pwr_imem_content)/sizeof(UINT32)*4; // in bytes
    UINT32 data_size = sizeof(uart2icd_pwr_dmem_content)/sizeof(UINT32)*4; // in bytes
    UINT32 blk = 0;
    UINT32 doff = 0;
    UINT32 size = 0;
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
            lwgpu->RegWr32(0x0010a184, uart2icd_pwr_imem_content[(doff+i)/4]); // LW_PPWR_FALCON_IMEMD(0) 0x0010a184
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
             lwgpu->RegWr32(0x0010a1c4, uart2icd_pwr_dmem_content[(doff+i)/4]); // LW_PPWR_FALCON_DMEMD(0) 0x0010a1c4
             i += 4; // Byte loaded
         }
         blk++;
    }

    // Start CPU
    lwgpu->RegWr32(0x0010a100, 0x2); // LW_PPWR_FALCON_CPUCTL 0x0010a100

    data = 0;
    UINT32 retry = 0;
    while(data != 0xbeef && retry <= 10){
        retry++;
        uart2icd_pwr::DelayNs(5000);
        data = lwgpu->RegRd32(0x0010a044); // LW_PPWR_FALCON_MAILBOX1 set to 0xbeef in app.c

        if(retry > 10 && data != 0xbeef){
            SetStatus(TEST_FAILED);
            getStateReport()->runFailed("Timeout Waiting for PMU falcon to finish.\n");
            ErrPrintf("uart2icd_pwr test failed!");
            return 1;
        }
    }

    //trigger the test
    Platform::EscapeWrite("drv_trigger_uart2icd_sanity", 0, 1, 1);

    //end the test
    UINT32 test_done, tmp;
    test_done = 0;
    retry = 0;
    while (!test_done && retry <= 100) {
        retry++;
        Platform::EscapeRead("uart2icd_test_done", 0, 1, &tmp);
        test_done = tmp;
        uart2icd_pwr::DelayNs(5000);

        if(retry > 100 && !test_done){
            SetStatus(TEST_FAILED);
            getStateReport()->runFailed("Timeout Waiting for uart test done signal.\n");
            ErrPrintf("uart2icd_pwr test failed!");
            return 1;
        }
    }

    uart2icd_pwr::DelayNs(5000);
    InfoPrintf("uart2icd_pwr test ends...\n");
    return 0;
}
