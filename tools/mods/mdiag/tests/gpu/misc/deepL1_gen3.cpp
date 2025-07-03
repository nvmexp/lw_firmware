/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2006-2021 by LWPU Corporation.  All rights 
 * reserved.  All information contained herein is proprietary and confidential 
 * to LWPU Corporation.  Any use, reproduction, or disclosure without the 
 * written permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */
// To facilitate faster compiles via pre-compiled headers, keep "stdtest.h" first
#include "mdiag/tests/stdtest.h"

#include "deepL1_gen3.h"

#include "core/include/gpu.h"
#include "core/include/rc.h"
#include "device/interface/pcie.h"
#include "gpu/include/gpusbdev.h"
#include "gpu/perf/pmusub.h"
#include "hopper/gh100/dev_lw_xpl.h"
#include "hopper/gh100/dev_xtl_ep_pcfg_gpu.h"
#include "hopper/gh100/dev_xtl_ep_pri.h"
#include "mdiag/lwgpu.h"
#include "mdiag/tests/test_state_report.h"
#include "mdiag/utils/lwgpu_channel.h"
#include "sim/IChip.h"
#include "turing/tu102/dev_host.h"
#include "turing/tu102/dev_lw_xp.h"
#include "turing/tu102/dev_lw_xve.h"
#include "turing/tu102/dev_lw_xve1.h"
#include "turing/tu102/dev_lw_xve3g_fn_ppc.h"
#include "turing/tu102/dev_lw_xve3g_fn_xusb.h"
#include "turing/tu102/dev_trim.h"

// define which tests are run
static int pexpll_pwr_down = 0;
static int pmu_wakeup = 0;
static int selfcheck_EscapeRead = 0;
static int beacon_wakeup_req_de = 0;
static int beacon_wakeup_req = 0;
static int beacon_wakeup_nondeepL1 = 0;
static int deepL1_for_pcode = 0;
static int deepL1_gen2 = 0;

extern const ParamDecl deepL1_gen3_params[] = {
    SIMPLE_PARAM("-pexpll_pwr_down", "Testing deepL1"),
    SIMPLE_PARAM("-pmu_wakeup", "Testing deepL1 with pmu pri-write wake up"),
    SIMPLE_PARAM("-beacon_wakeup_req_de", "Testing deepL1 with beacon wake up, CLKREQ deasserted"),
    SIMPLE_PARAM("-beacon_wakeup_req", "Testing deepL1 with beacon wake up, CLKREQ asserted"),
    SIMPLE_PARAM("-beacon_wakeup_nondeepL1", "Testing deepL1 with beacon wake up, dGPU didn't go into deepL1"),
    SIMPLE_PARAM("-deepL1_for_pcode", "Enhance the deepL1 test to help debug the production code"),
    SIMPLE_PARAM("-deepL1_gen2", "Testing deepL1 in Gen2 speed"),
    STRING_PARAM("-pwrimage", "String to specify which binary image to use to bootstrap PMU"),
    STRING_PARAM("-selfcheck_EscapeRead", "selfcheck seq state through EscapedRead seq_st"),
    PARAM_SUBDECL(lwgpu_single_params),
    LAST_PARAM
};

// constructor
deepL1_gen3::deepL1_gen3(ArgReader *params) :
    LWGpuSingleChannelTest(params)
{
    if (params->ParamPresent("-pexpll_pwr_down"))
       pexpll_pwr_down = 1;
    if (params->ParamPresent("-pmu_wakeup"))
       pmu_wakeup = 1;
    if (params->ParamPresent("-beacon_wakeup_req_de"))
       beacon_wakeup_req_de = 1;
    if (params->ParamPresent("-beacon_wakeup_req"))
       beacon_wakeup_req = 1;
    if (params->ParamPresent("-beacon_wakeup_nondeepL1"))
       beacon_wakeup_nondeepL1 = 1;
    if (params->ParamPresent("-deepL1_for_pcode"))
       deepL1_for_pcode = 1;
    if (params->ParamPresent("-deepL1_gen2"))
       deepL1_gen2 = 1;
    if ( params->ParamPresent( "-pwrimage" ) > 0 )
        m_paramsString = params->ParamStr( "-pwrimage" );
    if (params->ParamPresent("-selfcheck_EscapeRead"))
       selfcheck_EscapeRead = 1;
    
}

// destructor
deepL1_gen3::~deepL1_gen3(void)
{
}

// Factory
STD_TEST_FACTORY(deepL1_gen3,deepL1_gen3)

// setup method
int deepL1_gen3::Setup(void)
{
    lwgpu = LWGpuResource::FindFirstResource();
    arch = lwgpu->GetArchitecture();
    ch = lwgpu->CreateChannel();
    if(!ch)
    {
        ErrPrintf("deepL1_gen3::Setup failed to create channel\n");
        return 0;
    }
    getStateReport()->init("deepL1_gen3");
    getStateReport()->enable();

    return 1;
}

//CleanUp
void deepL1_gen3::CleanUp(void)
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

void deepL1_gen3::DelayNs(UINT32 Lwclks)
{
  // Lwrrently this function is in terms of .2nS / lwclk. This is incorrectly done
  // in RTL. This should be a close approx of 1 nanosecond.
  Platform::EscapeWrite("CLOCK_WAIT",IChip::ECLOCK_LWCLK,0,Lwclks * 5);
}

int deepL1_gen3::Checkstate(UINT32 expected_state )
{
        UINT32 num_try;
        UINT32 seq_st = 0;
        Platform::EscapeRead("deepL1_seq_st", 0, 32, &seq_st);
        InfoPrintf("deepL1_seq_st = %d!\n", seq_st);

        num_try = 1;
        deepL1_gen3::DelayNs(2000);
        Platform::EscapeRead("deepL1_seq_st", 0, 32, &seq_st);
        InfoPrintf("deepL1_gen3 state :%08x -- try %d  \n",seq_st,num_try);

       while((num_try < 200) && (seq_st != expected_state)) {
          deepL1_gen3::DelayNs(2000);
          num_try = num_try +1;
          Platform::EscapeRead("deepL1_seq_st", 0, 32, &seq_st);
          InfoPrintf("deepL1_gen3 state :%08x -- try %d  \n",seq_st,num_try);
       }

       if ( (num_try>= 200)&&(seq_st!= expected_state) ) {
       SetStatus(TEST_FAILED);
       getStateReport()->runFailed("Failed check state.\n");
       ErrPrintf("deepL1_gen3 state does not go to expected state : %08x \n", expected_state);
       return(1);
       }
       else return(0);
}

// run - main routine
void deepL1_gen3::Run(void)
{
      UINT32 data = 0;
      UINT32 xve_multi_func = 0;

      InfoPrintf("deepL1_gen3 Test at Run\n");
      SetStatus(TEST_INCOMPLETE);

      InfoPrintf("test deepL1_gen3() starting ....\n");
      InfoPrintf("MYDEBUG: %d\n", arch);

    //-------------------------------------------------------------
    //-------------------------------------------------------------
    // Set LW_XPL_PL_PAD_CTL_PRI_XPL_TX/RX/XCLK_CG (0x00084000+0x2700) IDLE_CG_DLY_CNT to zero, no filter
    // enable  LW_XPL_PL_PAD_CTL_PRI_XPL_RXCLK_CG_IDLE_CG_EN
    //-------------------------------------------------------------
      data = lwgpu->RegRd32(0x00086700);
      data = FLD_SET_DRF(_XPL, _PL_PAD_CTL_PRI_XPL_XCLK_CG, _IDLE_CG_DLY_CNT, _HWINIT, data);
      data = FLD_SET_DRF(_XPL, _PL_PAD_CTL_PRI_XPL_XCLK_CG, _IDLE_CG_EN, _ENABLED, data);
      lwgpu->RegWr32(0x00086700, data);
      
      data = lwgpu->RegRd32(0x00086710);
      data = FLD_SET_DRF(_XPL, _PL_PAD_CTL_PRI_XPL_TXCLK_CG, _IDLE_CG_DLY_CNT, _HWINIT, data);
      data = FLD_SET_DRF(_XPL, _PL_PAD_CTL_PRI_XPL_TXCLK_CG, _IDLE_CG_EN, _ENABLED, data);
      lwgpu->RegWr32(0x00086710, data);

      data = lwgpu->RegRd32(0x00086720);
      data = FLD_SET_DRF(_XPL, _PL_PAD_CTL_PRI_XPL_RXCLK_CG, _IDLE_CG_DLY_CNT, _HWINIT, data);
      data = FLD_SET_DRF(_XPL, _PL_PAD_CTL_PRI_XPL_RXCLK_CG, _IDLE_CG_EN, _ENABLED, data);
      lwgpu->RegWr32(0x00086720, data);

      Platform::Delay(10);
    
      // bug 200333856
      data = lwgpu->RegRd32(0x00084884);
      data = FLD_SET_DRF(_XPL, _DL_MGR_CYA_1, _ASPM_WINIDLE, _INIT, data);
      lwgpu->RegWr32(0x00084884, data);
      
    //---------------------------------------------------------------
    // set LW_XTL_EP_PRI_MISCL (0x00091B28) LW_XTL_EP_PRI_MISCL_CYA_PMCSR_POWER_STATE_D3HOT_IGNORE to 0
    //---------------------------------------------------------------
      data = lwgpu->RegRd32(0x00091B28);
      data = data & 0xfffffffe ;
      lwgpu->RegWr32(0x00091B28, data);

      Platform::Delay(10);

    //-----------------------------------------------------
    // Load pwrimage.bin for PMU
    //  - See diag/mods/gpu/utility/pmusub.cpp for function defs.
    //-----------------------------------------------------
    if (pmu_wakeup || deepL1_gen2 || deepL1_for_pcode || beacon_wakeup_nondeepL1 || beacon_wakeup_req || beacon_wakeup_req_de )
    {

        PMU *pPmu;

        if (lwgpu->GetGpuSubdevice()->GetPmu(&pPmu) != OK)
        {
            SetStatus(TEST_FAILED);
            ErrPrintf("Unable to get PMU Object\n");
            return;
        }

        /*
         Steps to generate deepL1_gen3_imem_content and deepL1_gen3_dmem_content
         - Step 1: Compile source code into binary
           In //hw/lwgpu_pwr/ip/power/pwr/1.0/stand_sim/private/cc_tests/pmu/fullchip/deepL1_gen3
           ~> make all

           This will generate two files:
           App_fullchip.0x00000000 (imem code)
           App_fullchip.0x10000000 (dmem data)

         - Step 2: Colwert to memory format and generate image header file
           In //hw/lwgpu_pwr/ip/power/pwr/1.0/stand_sim/private/cc_tests/pmu/fullchip/deepL1_gen3
           ~> //hw/lwgpu/tools/mods/trace_3d/plugins/ba_sampling/bin2mem.pl -i App_fullchip.0x00000000 -o App_fullchip.0x00000000.mem
           ~> //hw/lwgpu/tools/mods/trace_3d/plugins/ba_sampling/bin2mem.pl -i App_fullchip.0x10000000 -o App_fullchip.0x10000000.mem

        - Step 3: Colwert mem file into header file
          In //hw/lwgpu_pwr/ip/power/pwr/1.0/stand_sim/private/cc_tests/pmu/fullchip/deepL1_gen3
          ~> //hw/lwgpu/tools/mods/trace_3d/plugins/ba_sampling/gen_img.pl -aname deepL1_gen3_imem_content -tname deepL1_code -image App_fullchip.0x00000000.mem -elfbin App_fullchip.out
          ~> //hw/lwgpu/tools/mods/trace_3d/plugins/ba_sampling/gen_img.pl -aname deepL1_gen3_dmem_content -tname deepL1_data -image App_fullchip.0x10000000.mem -elfbin App_fullchip.out

          This will generate two files:
          code_img_deepL1_code.h
          code_img_deepL1_data.h

        - Step 4: Copy deepL1_gen3_imem_content/deepL1_gen3_dmem_content from code_img_deepL1_code.h/code_img_deepL1_data.h respectively
       */
        
        UINT32 deepL1_gen3_imem_content[]={
            0x0849f4bd, 0x9f9fff01, 0x010999e7, 0xfe0894b6, 0x937e0094, 0x02f80003, 0x000000f8, 0x00000000,
            0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000,
            0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000,
            0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000,
            0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000,
            0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000,
            0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000,
            0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000,
            0x94bd2c0f, 0xffeff9ff, 0xd4bd9ff9, 0xb6029ebb, 0x1c3e08a4, 0xfdff0001, 0x029ebb9f, 0x08f49aa6,
            0xdf00f8f8, 0x80000000, 0xfa07cc49, 0xd049009f, 0x009ffa07, 0x0920004f, 0x009ffa04, 0x00f8a4bd,
            0x2849ff0f, 0x009ffa04, 0xd449f4bd, 0x009ffa07, 0xfa07d849, 0xff0f009f, 0xfa07cc49, 0xd049009f,
            0x009ffa07, 0x00ff008f, 0x9ffa1009, 0x00804f00, 0xdf009ffa, 0x0badcaba, 0x9ffa4009, 0x01238a00,
            0x03287e00, 0x00008f00, 0x07a84901, 0xdf009ffa, 0x80000000, 0xfa07d449, 0xd849009f, 0x009ffa07,
            0x10a0448a, 0xefbeefdb, 0x7ec4bdbe, 0xf400072c, 0x28f40031, 0xcab0df00, 0x44090bad, 0x0a009ffa,
            0x01007e32, 0xa0448a00, 0xbeefdb10, 0xc4bdbeef, 0x00072c7e, 0xadcab1df, 0xfa44090b, 0x31f4009f,
            0x0028f400, 0xfa01ff90, 0x050a009f, 0x0001007e, 0xd489f4bd, 0x9ffa10a7, 0x04999000, 0xf8009ffa,
            0xf900f802, 0xf9a0f990, 0xf9c0f9b0, 0xf9e0f9d0, 0x018afef0, 0xa089a0f9, 0x9abf0002, 0xa0fca5f9,
            0xfc00a8fe, 0xfce0fcf0, 0xfcc0fcd0, 0xfca0fcb0, 0xf801f890, 0x02fa8900, 0x0093fe00, 0x0002cc89,
            0x890091fe, 0xfe000203, 0x008f0090, 0x1c090800, 0x4f009ffa, 0x10090808, 0xf4009ffa, 0x31f41131,
            0x8a00f810, 0x7e000330, 0x89000328, 0xfe0002fa, 0xcc890093, 0x91fe0002, 0x02038900, 0x0090fe00,
            0x0800008f, 0x9ffa1c09, 0x48fb4f00, 0x9ffa1009, 0x49ff0f00, 0x9ffa04c4, 0x07d44900, 0x49009ffa,
            0x9ffa07d8, 0x0000df00, 0x84491000, 0x009ffa06, 0x6049ff0f, 0x009ffa07, 0xfa076449, 0x7049009f,
            0x009ffa07, 0xf41131f4, 0x00f81031, 0xa0f990f9, 0xc0f9b0f9, 0xe0f9d0f9, 0x8afef0f9, 0x7ea0f901,
            0xfc000346, 0x00a8fea0, 0xe0fcf0fc, 0xc0fcd0fc, 0xa0fcb0fc, 0x01f890fc, 0x90f900f8, 0xb0f9a0f9,
            0xd0f9c0f9, 0xf0f9e0f9, 0xf9018afe, 0x037d7ea0, 0xfea0fc00, 0xf0fc00a8, 0xd0fce0fc, 0xb0fcc0fc,
            0x90fca0fc, 0x00f801f8, 0x0002a089, 0x00f89aa0, 0x0809f4bd, 0x0f9f9fff, 0x01f9fa04, 0xfa00904f,
            0xa4bd01f9, 0x94bd00f8, 0xf9ff080f, 0x94bfffbf, 0x0f1d0bf4, 0xfa580902, 0x080f009f, 0x9ffa0409,
            0x00008f01, 0x00904901, 0xf8019ffa, 0x00a48a00, 0xbdc4bd00, 0x7ee4bdd4, 0xf80003d3, 0x1832f400,
            0x8a01cbfe, 0xbd0000c8, 0xbdd4bdc4, 0x03d37ee4, 0x7e00f800, 0x7e000519, 0x89000263, 0x18000160,
            0x4089019a, 0x9b980001, 0x80008c01, 0xffa4f000, 0x7e08b5b6, 0x7e0003bf, 0xbd000140, 0x8900f8a4,
            0xa0000170, 0x0174899a, 0x899ba000, 0xa0000178, 0xf900f89c, 0xfea1b242, 0xff890183, 0x39fffcff,
            0x0098fe94, 0x8901b2fe, 0xbf000170, 0x8fff499f, 0xb69429ff, 0x9ffd0cf4, 0x009bfe05, 0x890174fe,
            0xbf000174, 0x0097fe99, 0x7c8a03f8, 0xa0bf0001, 0x0001008f, 0xb504feb5, 0x099001fb, 0x02fcb501,
            0xa003fdb5, 0xbdf0a0a9, 0x01008fe4, 0x90efbc00, 0x90f81e3c, 0x9f3501ee, 0x2be0b314, 0x981e3c0b,
            0xe9009433, 0x1489f43d, 0x808d0001, 0x9f3c0001, 0x00008fe9, 0x89debf04, 0xfd000100, 0xe9fa059f,
            0x8fd9bf06, 0xbf000178, 0x409990ff, 0x9fa6d9a0, 0xbd0708f4, 0xfed9a094, 0x2bfe0047, 0x0038fe00,
            0x004941fb, 0x20f43d24, 0x0199909f, 0x24409eb3, 0x4ce4bdfb, 0x004d0820, 0xfa400b20, 0xeebc00cd,
            0x0294b690, 0x240499b8, 0x01ef9000, 0x60019d75, 0x03f9949b, 0x240099b8, 0x909e2000, 0xfeb204cc,
            0x4f40dd90, 0xe4b32400, 0x947dd504, 0x7512fb75, 0xf97517f9, 0x28f93516, 0x4913ff75, 0x304f04c8,
            0x009ffa24, 0x4924004f, 0x9ffa04cc, 0x09400f00, 0x019ffa1c, 0x9ffa94bd, 0xf400f800, 0x94bdfc30,
            0xfe014ffe, 0xf9a0014e, 0xf990efbf, 0xa6e9a001, 0xf71ef4fa, 0xf80430f4, 0x01908f00, 0xa094bd00,
            0x01948ff9, 0xf8f9a000, 0xbd03f800, 0x0097fe94, 0x4f01b9fe, 0xc4f08000, 0x049ffd07, 0xfd0cc4b6,
            0x9bfe059c, 0x10b4b600, 0xfa05bafd, 0x00f806db, 0x00019489, 0x72f99fbf, 0x04bd97b2, 0x9590aabc,
            0x94b602f1, 0x02a4b601, 0x90039490, 0x129001a3, 0x01908601, 0x00008500, 0x05a13e00, 0x9854bc00,
            0x53bc6dbf, 0x040a94c8, 0xd4b6020b, 0xd0d9bc08, 0x0001a089, 0x90d0dabc, 0xa9bc0100, 0x05297ea0,
            0xb279bf00, 0x0394f01f, 0xb2050bf4, 0x01908e2f, 0xf40fa600, 0xe9bfca1e, 0xa0019990, 0x8971fbe9,
            0xbf000194, 0xb242f99f, 0x9504bd94, 0x908302f1, 0x12900001, 0x05fd3e01, 0x893dbf00, 0x94010000,
            0x020b040a, 0xbc08d4b6, 0xa089d0d9, 0xdabc0001, 0x010090d0, 0xb2a0a9bc, 0x05297ebc, 0xb249bf00,
            0x0394f01f, 0xb2050bf4, 0x01908e2f, 0xf40fa600, 0xe9bfca1e, 0xa0019990, 0xf941fbe9, 0x01948002,
            0x8e09bf00, 0x900001a0, 0x94b6019f, 0xbc0fa002, 0x9aa0909e, 0x0e40f4b3, 0xbf7ebab2, 0x94bd0005,
            0x01fb09a0, 0x948002f9, 0x09bf0001, 0x0001a08e, 0xb6019f90, 0x0fa00294, 0xa0909ebc, 0x40f4b39a,
            0x05bf7e0c, 0xa094bd00, 0x4f01fb09, 0x10090080, 0xdf009ffa, 0x0badcaba, 0x9ffa4009, 0x0031f400,
            0x890028f4, 0x8f000000, 0xbf00000c, 0x4ce4bd9d, 0xa33eff00, 0xf9bf0006, 0xfd01ee90, 0xf9a0049c,
            0xa610ff90, 0xf11ef4ed, 0x2c0f00f8, 0xf9ff94bd, 0x9ff9ffdf, 0xb2e29dbc, 0x90aabcfb, 0x94909abc,
            0xc4bd059f, 0xbc909fbc, 0xd43ef09a, 0xbcff0006, 0xe29dbc9f, 0x08f4efa6, 0xda00f8f8, 0x03ff0004,
            0xc4bd010b, 0x0007de7e, 0xff0000da, 0x00804b03, 0xde7ec4bd, 0x00f80007, 0xc4bdabb2, 0xff00a8da,
            0x072c7e03, 0x8a00f800, 0xbd0000e0, 0xbdc4bdb4, 0x7ee4bdd4, 0xbd0003d3, 0x042049f4, 0x0f019ffa,
            0xff140901, 0x03f89f9f, 0x00f802f8, 0x4901c4f0, 0xc4b607a0, 0x05cafd1b, 0x49009cfa, 0x9bfa07a4,
            0x00f28f00, 0x07ac4901, 0xf8009ffa, 0xffffd900, 0xa9fd03ff, 0x01999004, 0xfd01c4f0, 0xc4b605a9,
            0x07a0491b, 0xfa05acfd, 0xa449009a, 0x009bfa07, 0x0100f28f, 0xfa07ac49, 0x00f8009f, 0xffffffd9,
            0x04a9fd03, 0x00072c7e, 0xbd07ac4e, 0x9feffff4, 0x700094f1, 0x20009ab3, 0x009eb30a, 0x0bf80940,
            0x9ab300f8, 0xf8eb1000, 0xffffd900, 0xa9ff03ff, 0x0000da94, 0x9aff0400, 0x072c7ea5, 0x07ac4e00,
            0xeffff4bd, 0x0094f19f, 0x009ab370, 0x9eb30a20, 0xf8094000, 0xb300f80b, 0xeb10009a, 0x2c7e00f8,
            0xac4e0007, 0xfff4bd07, 0x94f19fef, 0x9ab37000, 0xb30a2000, 0x0940009e, 0x00f80bf8, 0x10009ab3,
            0xd900f8eb, 0x03ffffff, 0xfd01b4f0, 0xb4b604a9, 0x07a0491b, 0xfa05bafd, 0xf18f009b, 0xac490100,
            0x009ffa07, 0x037e00f8, 0xac4e0008, 0xfff4bd07, 0x94f19fef, 0x9ab37000, 0xb30a2000, 0x0b40009e,
            0x4b3e0bf8, 0x9ab30008, 0xbde91000, 0x07a44a94, 0xf8afa9ff, 0x08037e00, 0x07ac4e00, 0xeffff4bd,
            0x0094f19f, 0x009ab370, 0x9eb30a20, 0xf8104000, 0x009ab30b, 0xa63e1240, 0x9ab30008, 0x3ee41000,
            0xbd0008a6, 0x07b049e4, 0xdf9f9eff, 0x80000000, 0xf4a49fff, 0x88490b0b, 0xaf9eff09, 0xa44900f8,
            0xaf9aff07, 0xf4bd00f8, 0xff07a449, 0x00f8af9f, 0x000000d9, 0x01b4f010, 0xb605a9fd, 0xabfd1bb4,
            0xf8aabf05, 0x01c4f000, 0x000000d9, 0x1bc4b610, 0xfd05a9fd, 0xaba005ac, 0xc4f000f8, 0x0000d901,
            0xc4b61000, 0x05a9fd1b, 0xa005acfd, 0xf000f8ab, 0x00d901c4, 0xb6140000, 0xa9fd1bc4, 0x05acfd05,
            0x00f8aba0, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000,
            0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000,
            0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000,
            0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000,
            0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000,
            0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000,
            0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000,
            0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000};

        UINT32 deepL1_gen3_dmem_content[]={
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
            0x00000000, 0x00000000, 0x00000000, };

        // configure DMACTL to disable CTX
        lwgpu->RegWr32(0x0010a10c, 0x0); // LW_PPWR_FALCON_DMACTL (0x0010a10c)

        // loading IMEM/DMEM through PRIV
        UINT32 code_size = sizeof(deepL1_gen3_imem_content)/sizeof(UINT32)*4; // in bytes
        UINT32 data_size = sizeof(deepL1_gen3_dmem_content)/sizeof(UINT32)*4; // in bytes
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
                lwgpu->RegWr32(0x0010a184, deepL1_gen3_imem_content[(doff+i)/4]); // LW_PPWR_FALCON_IMEMD(0) 0x0010a184
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
                lwgpu->RegWr32(0x0010a1c4, deepL1_gen3_dmem_content[(doff+i)/4]); // LW_PPWR_FALCON_DMEMD(0) 0x0010a1c4
                i += 4; // Byte loaded
            }
            blk++;
        }

        // Start CPU
        lwgpu->RegWr32(0x0010a100, 0x2); // LW_PPWR_FALCON_CPUCTL 0x0010a100
    }

    //---------------------------------------------------------
    // Config idle counter register if selfcheck_EscapeRead=0
    //---------------------------------------------------------

    if (!selfcheck_EscapeRead){
      //Enable idle_mask(1) for deepL1 event
      data = lwgpu->RegRd32(0x0010be44);
      data = data | 0x08000000;
      lwgpu->RegWr32(0x0010be44 , data);

      //assert reset for idle_counter(1)
      data = lwgpu->RegRd32(0x0010bf84);
      data = data | 0x80000000;
      lwgpu->RegWr32(0x0010bf84 , data);

      //de-assert reset for idle_counter(1)
      data = lwgpu->RegRd32(0x0010bf84);
      data = data & 0x00000000;
      lwgpu->RegWr32(0x0010bf84 , data);

      //Set idle_ctl(1) to idle count
      data = lwgpu->RegRd32(0x0010bfc4);
      data = data | 0x00000001;
      lwgpu->RegWr32(0x0010bfc4, data);
    }

    //-----------------------------------------------------
    // LW_EP_PCFG_GPU_LINK_CONTROL_STATUS (0x00000070)  in dev_xtl_ep_pcfg_gpu.h
    //  - Set LW_EP_PCFG_GPU_LINK_CONTROL_STATUS_CLOCK_PM_ (bit 8) to 1'b1.
    //-----------------------------------------------------
    data = lwgpu->CfgRd32(LW_EP_PCFG_GPU_LINK_CONTROL_STATUS);
    data = data | 0x100;
    lwgpu->CfgWr32(LW_EP_PCFG_GPU_LINK_CONTROL_STATUS, data);
   
    //to check xusb and ppc presence
    //Read LW_PCFG_XVE_XUSB_STATUS and _PPC_STATUS
    // Following assignment is only required in cases where explicity fuse is required for 
    // disabling XUSB/PPC and reading throught status registers. Commenting out now
    UINT32 xusb_present;// = (lwgpu->RegRd32(0x88000 + LW_XVE_XUSB_STATUS) & 0x80000000); 
    UINT32 ppc_present;//  = (lwgpu->RegRd32(0x88000 + LW_XVE_PPC_STATUS) & 0x80000000);

    //Check through escape read if xusb & ppc are present
    //Code is implemented in systop_misc_pcie.vx
    UINT32 get_xusb_enable_from_defines = 1;
    UINT32 get_ppc_enable_from_defines = 1;
    Platform::EscapeRead("get_xusb_enable_from_defines",0,1,&get_xusb_enable_from_defines);
    Platform::EscapeRead("get_ppc_enable_from_defines",0,1,&get_ppc_enable_from_defines);
   
    if(!get_xusb_enable_from_defines) xusb_present = 0;
    if(!get_ppc_enable_from_defines) ppc_present = 0;

    InfoPrintf("get_xusb_enable_from_defines = 0x%x, xusb_present = %d\n", get_xusb_enable_from_defines,xusb_present);


    //////////////////////////// 
    //Save Off PCI parameters for drv
    GpuSubdevice * pSubdev;
    pSubdev = lwgpu->GetGpuSubdevice();
    auto pGpuPcie = pSubdev->GetInterface<Pcie>();
    UINT32 pci_domain_num = pGpuPcie->DomainNumber();
    UINT32 pci_bus_num = pGpuPcie->BusNumber();
    UINT32 pci_dev_num = pGpuPcie->DeviceNumber();
    //UINT32 pci_func_num = pGpuPcie->FunctionNumber();

    //-----------------------------------------------------
    // -set LW_XVE_XUSB_LINK_CONTROL_STATUS_CLOCK_PM (bit 8) to 1'b1
    // offset is same for all the functions so just changing function number in config base
    //----------------------------------------------------- 
    //to check xusb and ppc presence
    if( (xusb_present == 0x80000000) && (ppc_present == 0x80000000) ){
        Platform::PciWrite32(pci_domain_num,
                            pci_bus_num,
                            pci_dev_num,
                            2, // xusb fn num
                            LW_XVE_LINK_CONTROL_STATUS, //offset is same for all functions
                            DRF_NUM(_XVE, _LINK_CONTROL_STATUS, _CLOCK_PM, 1));
        
        //-----------------------------------------------------
        // -set LW_XVE_PPC_LINK_CONTROL_STATUS_CLOCK_PM (bit 8) to 1'b1
        //----------------------------------------------------- 
        Platform::PciWrite32(pci_domain_num,
                            pci_bus_num,
                            pci_dev_num,
                            3, // xusb fn num
                            LW_XVE_LINK_CONTROL_STATUS, //offset is same for all functions
                            DRF_NUM(_XVE, _LINK_CONTROL_STATUS, _CLOCK_PM, 1));
    }
    //-----------------------------------------------------
    // Setting up LW_THERM_GATE_CTRL(21) XVE (0x00020254)
    //  - Set BLK_CLK to AUTO mode bit3:2 , 2'b1
    //  In Ampere XVE BLCG register has moved to shared register LW_THERM_GLOBAL_OVERRIDES. Refer bug http://lwbugs/2623075/6
    //  In RUN mode, the block level clocks will be forced to run (assuming the engine clock is running). 
   //   In AUTO mode, the local BLCG controllers will determine the state of clks.
    data = lwgpu->RegRd32(0x00020280);
    data = data & 0xffffffff;
    lwgpu->RegWr32(0x00020280, data);

    //-----------------------------------------------------
    // Setting up LW_XTL_EP_PRI_PWR_MGMT_CYA1(0x00091B10)
    //-----------------------------------------------------
    //L0S_ENABLE_ENABLED L1_ENABLE_ENABLED
    data = lwgpu->RegRd32(0x00091B10);
    //data = FLD_SET_DRF(_XTL_EP_PRI, _PRI_PWR_MGMT_CYA1, _CYA_L0S_ENABLE, _ENABLED, data);
    data = FLD_SET_DRF(_XTL_EP_PRI, _PWR_MGMT_CYA1, _PCIPM_L1_1_ENABLE, _INIT, data);
    lwgpu->RegWr32(0x00091B10, data);

    //Bug 2102211, allow deepl1 entry when hostclk sourced from hostnafll with auto mode disabled
    //for deepl1 entry clk switches xclk and hostclk to alt path, 
    //now hostclk will not switch to alt path when auto mode is disabled (hostnafll path)
    //-----------------------------------------------------
    // Setting up LW_PTRIM_SYS_IND_SYS_CORE_CLKSRC (0x00138fc0)
    //-----------------------------------------------------
    //LW_PTRIM_SYS_IND_SYS_CORE_CLKSRC_HOSTCLK_AUTO_ENABLE
    //No HOSTCLK in GH100
    //data = lwgpu->RegRd32(LW_PTRIM_SYS_IND_SYS_CORE_CLKSRC);
    //if((data & 0x3c0) != 0x180){ //checking hostclk source to be hostnafll
    //data = FLD_SET_DRF(_PTRIM, _SYS_IND_SYS_CORE_CLKSRC, _HOSTCLK_AUTO, _ENABLE, data); 
    //lwgpu->RegWr32(LW_PTRIM_SYS_IND_SYS_CORE_CLKSRC, data);
    //}


    //-----------------------------------------------------
    // Enable wakeup from host (allowing pmu wakeup)
    //-----------------------------------------------------
    //  - enable  LW_XTL_EP_PRI_PRIV_XV_BLKCG2
    //data = lwgpu->RegRd32(0x00091418);
    //data = FLD_SET_DRF(_XTL_EP_PRI, _PRIV_XV_BLKCG2, _HOST2XV_HOST_IDLE_WAKE_EN, _ENABLED, data);
    //lwgpu->RegWr32(0x00091418, data);
    //  - disable LW_XVE_PRIV_XV_BLKCG2_UPSTREAM_REQ_WAKE_EN
    //  Removing this programming due to bug 200708002 
    //data = lwgpu->RegRd32(0x00091418);
    //data = FLD_SET_DRF(_XTL_EP_PRI, _PRIV_XV_BLKCG2, _UPSTREAM_REQ_WAKE_EN, _DISABLED, data);
    //lwgpu->RegWr32(0x00091418, data);

    //-----------------------------------------------------
    // Setting up LW_XTL_EP_PRI_PWR_MGMT_CYA1 (0x00091B10)
    // Setting up LW_XTL_EP_PRI_PWR_MGMT_CYA2 (0x00091B14)
    //-----------------------------------------------------
    //  - Enable LW_XTL_EP_PRI_PWR_MGMT_CYA1/2_ASPM_L1_PLL_PD_ENABLE
    //  - Enable LW_XTL_EP_PRI_PWR_MGMT_CYA1/2_PCIPM_L1_PLL_PD_ENABLE
    //  - Enable LW_XTL_EP_PRI_PWR_MGMT_CYA1/2_CLOCK_PM_ENABLE
    data = lwgpu->RegRd32(0x00091B10);
    data = data | 0x190;
    lwgpu->RegWr32(0x00091B10, data);

    data = lwgpu->RegRd32(0x00091B14);
    data = data | 0x190;
    lwgpu->RegWr32(0x00091B14, data);
    
    //  - Set LW_XPL_PL_LTSSM_LP_12_START_PLL_OFF_MIN_DLY  to 0x177 (1.5uS)
    data = lwgpu->RegRd32(0x000851B0);
    data = ((data & 0xff000000) | 0x177);
    lwgpu->RegWr32(0x000851B0, data);

    //-----------------------------------------------------
    // Setting up LW_XPL_PL_PAD_CTL_PAD_PWRDN (0x00086300)
    //-----------------------------------------------------
    data = lwgpu->RegRd32(0x00086300);
    data = data & 0xf3fffff6;
    data = data | 0x0c000009;
    lwgpu->RegWr32(0x00086300, data);

    if (pmu_wakeup)
    {

        //-----------------------------------------------------
        // Signal pmu to start its ucode operations
        // - LW_PPWR_FALCON_MAILBOX0 @ 0x0010a040
        // - LW_PPWR_FALCON_MAILBOX1 @ 0x0010a044
        //-----------------------------------------------------
        data = 0xbeef;
        lwgpu->RegWr32(0x0010a040, data);
        InfoPrintf("Write to LW_PPWR_FALCON_MAILBOX0 to signal pmu ucode to start operation.\n");

        //-----------------------------------------------------
        // Put device to D3hot state
        // LW_XVE_PWR_MGMT_1 in dev_lw_xve.ref
        //  - Set LW_XVE_PWR_MGMT_1_PWR_STATE to D3HOT.
        //-----------------------------------------------------
        data = lwgpu->CfgRd32(LW_EP_PCFG_GPU_CONTROL_AND_STATUS);
        data = FLD_SET_DRF(_EP_PCFG_GPU, _CONTROL_AND_STATUS, _POWER_STATE, _D3, data);
        lwgpu->CfgWr32(LW_EP_PCFG_GPU_CONTROL_AND_STATUS, data);

        //-----------------------------------------------------
        //Put device to D3hot state
        // LW_XVE_XUSB_PWR_MGMT_1 in dev_lw_xve.ref
        //  - Set LW_XVE_XUSB_PWR_MGMT_1_PWR_STATE to D3HOT.
        //----------------------------------------------------- 
        if( (xusb_present == 0x80000000) && (ppc_present == 0x80000000) ){
            Platform::PciWrite32(pci_domain_num,
                            pci_bus_num,
                            pci_dev_num,
                            2, // xusb fn num
                            LW_XVE_XUSB_PWR_MGMT_1, //offset is same for all functions
                            DRF_NUM(_XVE_XUSB, _PWR_MGMT_1, _PWR_STATE, 3));
            
            //-----------------------------------------------------
            // Put device to D3hot state
            // LW_XVE_PPC_PWR_MGMT_1 in dev_lw_xve.ref
            //  - Set LW_XVE_PPC_PWR_MGMT_1_PWR_STATE to D3HOT.
            //----------------------------------------------------- 
            Platform::PciWrite32(pci_domain_num,
                            pci_bus_num,
                            pci_dev_num,
                            3, // xusb fn num
                            LW_XVE_PPC_PWR_MGMT_1, //offset is same for all functions
                            DRF_NUM(_XVE_PPC, _PWR_MGMT_1, _PWR_STATE, 3));
        }
        //-----------------------------------------------------
        // Check LW_PPWR_FALCON_MAILBOX0/1 to gild the test
        //  - first, put device back to D0 state
        //  - check 0xbeefbeef;
        //-----------------------------------------------------
        if(selfcheck_EscapeRead) {
            InfoPrintf("Check whether goes to PDN_WAKE_UP_EVENT_WAIT state\n");
            if (Checkstate(0x40000)) return;

            //  - enable LW_XVE_PEX_PLL5_UPSTREAM_REQ_WAKE_EN (which is disabled by default)
            //data = lwgpu->RegRd32(0x00091418);
            //data = FLD_SET_DRF(_XTL_EP_PRI, _PRIV_XV_BLKCG2, _UPSTREAM_REQ_WAKE_EN, _ENABLED, data);
            //lwgpu->RegWr32(0x00091418, data);

            InfoPrintf("Check whether revert to SEQ_IDLE state\n");
            if (Checkstate(0x1)) return;
        }

        Platform::Delay(100);

        //-----------------------------------------------------
        // Put device to D0 state
        // LW_XVE_PWR_MGMT_1 in dev_lw_xve.ref
        //  - Set LW_XVE_PWR_MGMT_1_PWR_STATE to D0.
        //-----------------------------------------------------
        InfoPrintf("Put device back to D0 state.\n");
       
        data = lwgpu->CfgRd32(LW_EP_PCFG_GPU_CONTROL_AND_STATUS);
        data = FLD_SET_DRF(_EP_PCFG_GPU, _CONTROL_AND_STATUS, _POWER_STATE, _D0, data);
        lwgpu->CfgWr32(LW_EP_PCFG_GPU_CONTROL_AND_STATUS, data);


        //-----------------------------------------------------
        //Put device to D0 state
        // LW_XVE_XUSB_PWR_MGMT_1 in dev_lw_xve.ref
        //  - Set LW_XVE_XUSB_PWR_MGMT_1_PWR_STATE to D0.
        //-----------------------------------------------------
        if( (xusb_present == 0x80000000) && (ppc_present == 0x80000000) ){
            Platform::PciWrite32(pci_domain_num,
                            pci_bus_num,
                            pci_dev_num,
                            2, // xusb fn num
                            LW_XVE_XUSB_PWR_MGMT_1, //offset is same for all functions
                            DRF_NUM(_XVE_XUSB, _PWR_MGMT_1, _PWR_STATE, 0));
            
            //-----------------------------------------------------
            // Put device to D0 state
            // LW_XVE_PPC_PWR_MGMT_1 in dev_lw_xve.ref
            //  - Set LW_XVE_PPC_PWR_MGMT_1_PWR_STATE to D0.
            //----------------------------------------------------- 
            Platform::PciWrite32(pci_domain_num,
                            pci_bus_num,
                            pci_dev_num,
                            3, // xusb fn num
                            LW_XVE_PPC_PWR_MGMT_1, //offset is same for all functions
                            DRF_NUM(_XVE_PPC, _PWR_MGMT_1, _PWR_STATE, 0));
        }

        Platform::Delay(20);

        InfoPrintf("Now make sure priv operation works - device resumed ok!\n");
        data = 0xbeefbeef;
        lwgpu->RegWr32(0x0010a040, data);
        InfoPrintf("Priv write addr 0x0010a040(LW_PPWR_FALCON_MAILBOX0), data = %x\n", data);

        // Check data of mailbox0
        data = 0xbad;
        data = lwgpu->RegRd32(0x0010a040);
        if (data != 0xbeefbeef) {
            SetStatus(TEST_FAILED);
            getStateReport()->runFailed("Failed check mailbox0.\n");
            ErrPrintf("Priv read addr 0x0010a040(LW_PPWR_FALCON_MAILBOX0), got data = %x\n", data);
            return;
        }

        if(!selfcheck_EscapeRead) {
            data = lwgpu->RegRd32(0x10bf84);
            if(data == 0){
                SetStatus(TEST_FAILED);
                getStateReport()->runFailed("Failed check idle counter.\n");
                ErrPrintf("seq_st does not goes into deepL1 state because there is no deepL1 event!\n");
                return;
            }
        }

        InfoPrintf("test deepL1 pmu_wakeup() Done !\n");
    } //endif_pmu_wakeup

    if (pexpll_pwr_down)
    {

        InfoPrintf("test deepL1 pexpll_pwr_down() Starting !\n");
        //-----------------------------------------------------
        // Read LW_PCFG_XVE_MISC_1 (0x0008800c)
        // check LW_PCFG_XVE_MISC_1_HEADER_TYPE bit status
        //----------------------------------------------------
        //xve_multi_func = lwgpu->RegRd32(0x0008800c);
        //InfoPrintf("xve_multi_func = %x \n",xve_multi_func);
        //xve_multi_func = xve_multi_func & 0x00800000;
        xve_multi_func = 0;
        InfoPrintf("xve_multi_func = %x \n",xve_multi_func);
        //-----------------------------------------------------
        // Put device to D3hot state
        //  LW_EP_PCFG_GPU_CONTROL_AND_STATUS_POWER_STATE_D3 in ghlit1_xchx16/dev_xtl_ep_pcfg_gpu.ref
        //-----------------------------------------------------
        data = lwgpu->CfgRd32(LW_EP_PCFG_GPU_CONTROL_AND_STATUS);
        data = FLD_SET_DRF(_EP_PCFG_GPU, _CONTROL_AND_STATUS, _POWER_STATE, _D3, data);
        lwgpu->CfgWr32(LW_EP_PCFG_GPU_CONTROL_AND_STATUS, data);

        //-----------------------------------------------------
        // Put device to D3hot state
        // LW_XVE1_PWR_MGMT_1 in dev_lw_xve1.ref
        // - Set LW_XVE1_PWR_MGMT_1_PWR_STATE to D3HOT if the device has multifunc
        // both LW_XVE1_PWR_MGMT_1 & LW_XVE_PWR_MGMT_1 maps to 0x64,so adding 0x100 to get
        // the function1 address
        // mask to obtain bits 8 - 10
        // #define MASK_PCI_FUNCTION(address) ((address & 0x700) >> 8)

        //-----------------------------------------------------
        if (xve_multi_func == 0x00800000) {
          data = lwgpu->CfgRd32(0x100+LW_XVE1_PWR_MGMT_1);
          data = FLD_SET_DRF(_XVE1, _PWR_MGMT_1, _PWR_STATE, _D3HOT, data);
          lwgpu->CfgWr32(0x100+LW_XVE1_PWR_MGMT_1, data);
          InfoPrintf("Configured LW_XVE1_PWR_MGMT_1 reg !\n");
        }

        //-----------------------------------------------------
        //Put device to D3hot state
        // LW_XVE_XUSB_PWR_MGMT_1 in dev_lw_xve.ref
        //  - Set LW_XVE_XUSB_PWR_MGMT_1_PWR_STATE to D3HOT.
        //-----------------------------------------------------
        if( (xusb_present == 0x80000000) && (ppc_present == 0x80000000) ){
            Platform::PciWrite32(pci_domain_num,
                            pci_bus_num,
                            pci_dev_num,
                            2, // xusb fn num
                            LW_XVE_XUSB_PWR_MGMT_1, //offset is same for all functions
                            DRF_NUM(_XVE_XUSB, _PWR_MGMT_1, _PWR_STATE, 3));
            
            //-----------------------------------------------------
            // Put device to D3hot state
            // LW_XVE_PPC_PWR_MGMT_1 in dev_lw_xve.ref
            //  - Set LW_XVE_PPC_PWR_MGMT_1_PWR_STATE to D3HOT.
            //----------------------------------------------------- 
            Platform::PciWrite32(pci_domain_num,
                            pci_bus_num,
                            pci_dev_num,
                            3, // xusb fn num
                            LW_XVE_PPC_PWR_MGMT_1, //offset is same for all functions
                            DRF_NUM(_XVE_PPC, _PWR_MGMT_1, _PWR_STATE, 3));
        }
        //-----------------------------------------------------
        // Delay long enough to enter deepL1
        // During the period, pmu will issue priv access thru bar0 interface of host
        //-----------------------------------------------------
        if(selfcheck_EscapeRead) {
            InfoPrintf("Check whether goes to PDN_WAKE_UP_EVENT_WAIT state\n");
            if (Checkstate(0x40000)) return;

            //  - enable LW_XVE_PEX_PLL5_UPSTREAM_REQ_WAKE_EN (which is disabled by default)
            //data = lwgpu->RegRd32(0x00091418);
            //data = FLD_SET_DRF(_XTL_EP_PRI, _PRIV_XV_BLKCG2, _UPSTREAM_REQ_WAKE_EN, _ENABLED, data);
            //lwgpu->RegWr32(0x00091418, data);

            InfoPrintf("Check whether revert to SEQ_IDLE state\n");
            if (Checkstate(0x1)) return;
        }

        Platform::Delay(100);

        //-----------------------------------------------------
        // Put device to D0 state
        //  - Set LW_EP_PCFG_GPU_CONTROL_AND_STATUS to D0.
        //-----------------------------------------------------
        data = lwgpu->CfgRd32(LW_EP_PCFG_GPU_CONTROL_AND_STATUS);
        data = FLD_SET_DRF(_EP_PCFG_GPU, _CONTROL_AND_STATUS, _POWER_STATE, _D0, data);
        lwgpu->CfgWr32(LW_EP_PCFG_GPU_CONTROL_AND_STATUS, data);

        //-----------------------------------------------------
        // Put device to D0 state
        // LW_XVE1_PWR_MGMT_1 in dev_lw_xve1.ref
        //  - Set LW_XVE1_PWR_MGMT_1_PWR_STATE to D0 if the device has multifunc .
        // both LW_XVE1_PWR_MGMT_1 & LW_XVE_PWR_MGMT_1 maps to 0x64 ,so adding 0x100 to get
        // the function1 address
        // mask to obtain bits 8 - 10
        // #define MASK_PCI_FUNCTION(address) ((address & 0x700) >> 8)
        //-----------------------------------------------------
        if (xve_multi_func == 0x00800000) {
          data = lwgpu->CfgRd32(0x100+LW_XVE1_PWR_MGMT_1);
          data = FLD_SET_DRF(_XVE1, _PWR_MGMT_1, _PWR_STATE, _D0, data);
          lwgpu->CfgWr32(0x100+LW_XVE1_PWR_MGMT_1, data);
	}

        //-----------------------------------------------------
        //Put device to D0 state
        // LW_XVE_XUSB_PWR_MGMT_1 in dev_lw_xve.ref
        //  - Set LW_XVE_XUSB_PWR_MGMT_1_PWR_STATE to D0.
        //-----------------------------------------------------
        if( (xusb_present == 0x80000000) && (ppc_present == 0x80000000) ){
        Platform::PciWrite32(pci_domain_num,
                        pci_bus_num,
                        pci_dev_num,
                        2, // xusb fn num
                        LW_XVE_XUSB_PWR_MGMT_1, //offset is same for all functions
                        DRF_NUM(_XVE_XUSB, _PWR_MGMT_1, _PWR_STATE, 0));
        
        //-----------------------------------------------------
        // Put device to D0 state
        // LW_XVE_PPC_PWR_MGMT_1 in dev_lw_xve.ref
        //  - Set LW_XVE_PPC_PWR_MGMT_1_PWR_STATE to D0.
        //----------------------------------------------------- 
        Platform::PciWrite32(pci_domain_num,
                        pci_bus_num,
                        pci_dev_num,
                        3, // xusb fn num
                        LW_XVE_PPC_PWR_MGMT_1, //offset is same for all functions
                        DRF_NUM(_XVE_PPC, _PWR_MGMT_1, _PWR_STATE, 0));
        }
        
        InfoPrintf("Now make sure priv operation works - device resumed ok!\n");
        data = 0x5a5a;
        lwgpu->RegWr32(0x0010a040, data);
        InfoPrintf("Priv write addr 0x0010a040(LW_PPWR_FALCON_MAILBOX0), data = %x\n", data);
        data = 0xbad;
        data = lwgpu->RegRd32(0x0010a040);
        if (data != 0x5a5a) {
            SetStatus(TEST_FAILED);
            getStateReport()->runFailed("Failed check mailbox.\n");
            ErrPrintf("Priv read addr 0x0010a040(LW_PPWR_FALCON_MAILBOX0), got data = %x\n", data);
            return;
        } else {
            InfoPrintf("Priv read addr 0x0010a040(LW_PPWR_FALCON_MAILBOX0), got data = %x\n", data);
        }

        if(!selfcheck_EscapeRead) {
            data = lwgpu->RegRd32(0x10bf84);
            if(data == 0){
                SetStatus(TEST_FAILED);
                getStateReport()->runFailed("Failed check idle counter.\n");
                ErrPrintf("seq_st does not goes into deepL1 state because there is no deepL1 event!\n");
                return;
            }
        }

        InfoPrintf("test deepL1 pexpll_pwr_down() Done !\n");
    } //endif_pexpll_pwr_down

    if (beacon_wakeup_req_de)
    {
        //-----------------------------------------------------
        // Setting up LW_XTL_EP_PRI_PWR_MGMT_CYA1 (0x00091B10)
        // Setting up LW_XTL_EP_PRI_PWR_MGMT_CYA2 (0x00091B14)
        //-----------------------------------------------------
        //  - Enable LW_XTL_EP_PRI_PWR_MGMT_CYA1/2_ASPM_L1_PLL_PD_DISABLE
        //  - Enable LW_XTL_EP_PRI_PWR_MGMT_CYA1/2_PCIPM_L1_PLL_PD_DISABLE
        //  - Enable LW_XTL_EP_PRI_PWR_MGMT_CYA1/2_CLOCK_PM_DISABLE
        data = lwgpu->RegRd32(0x00091B10);
        data = data & 0xe6f;
        lwgpu->RegWr32(0x00091B10, data);

        data = lwgpu->RegRd32(0x00091B14);
        data = data | 0xe6f;
        lwgpu->RegWr32(0x00091B14, data);


        Platform::Delay(100);

        //-----------------------------------------------------
        // Setting up LW_XTL_EP_PRI_PWR_MGMT_CYA1 (0x00091B10)
        // Setting up LW_XTL_EP_PRI_PWR_MGMT_CYA2 (0x00091B14)
        //-----------------------------------------------------
        //  - Enable LW_XTL_EP_PRI_PWR_MGMT_CYA1/2_ASPM_L1_PLL_PD_ENABLE
        //  - Enable LW_XTL_EP_PRI_PWR_MGMT_CYA1/2_PCIPM_L1_PLL_PD_ENABLE
        //  - Enable LW_XTL_EP_PRI_PWR_MGMT_CYA1/2_CLOCK_PM_ENABLE
        data = lwgpu->RegRd32(0x00091B10);
        data = data | 0x190;
        lwgpu->RegWr32(0x00091B10, data);

        data = lwgpu->RegRd32(0x00091B14);
        data = data | 0x190;
        lwgpu->RegWr32(0x00091B14, data);

        //-----------------------------------------------------
        // Signal pmu to start its ucode operations
        // - LW_PPWR_FALCON_MAILBOX0 @ 0x0010a040
        // - LW_PPWR_FALCON_MAILBOX1 @ 0x0010a044
        //-----------------------------------------------------
        data = 0xbeef;
        lwgpu->RegWr32(0x0010a040, data);
        InfoPrintf("Write to LW_PPWR_FALCON_MAILBOX0 to signal pmu ucode to start operation.\n");

        //-----------------------------------------------------
        // Put device to D3hot state
        //  LW_EP_PCFG_GPU_CONTROL_AND_STATUS_POWER_STATE_D3 in ghlit1_xchx16/dev_xtl_ep_pcfg_gpu.ref
        //-----------------------------------------------------
        data = lwgpu->CfgRd32(LW_EP_PCFG_GPU_CONTROL_AND_STATUS);
        data = FLD_SET_DRF(_EP_PCFG_GPU, _CONTROL_AND_STATUS, _POWER_STATE, _D3, data);
        lwgpu->CfgWr32(LW_EP_PCFG_GPU_CONTROL_AND_STATUS, data);

        //-----------------------------------------------------
        // Check LW_PPWR_FALCON_MAILBOX0 to gild the test
        //  - first, put device back to D0 state
        //  - check 0xbeefbeef;
        //-----------------------------------------------------
        if(selfcheck_EscapeRead) {
            InfoPrintf("Check whether goes to PDN_WAKE_UP_EVENT_WAIT state\n");
            if (Checkstate(0x40000)) return;

            //  - enable LW_XVE_PEX_PLL5_UPSTREAM_REQ_WAKE_EN (which is disabled by default)
            //data = lwgpu->RegRd32(0x00091418);
            //data = FLD_SET_DRF(_XTL_EP_PRI, _PRIV_XV_BLKCG2, _UPSTREAM_REQ_WAKE_EN, _ENABLED, data);
            //lwgpu->RegWr32(0x00091418, data);

            InfoPrintf("Check whether revert to SEQ_IDLE state\n");
            if (Checkstate(0x1)) return;
        }

        Platform::Delay(100);

        //-----------------------------------------------------
        // Put device to D0 state
        //  LW_EP_PCFG_GPU_CONTROL_AND_STATUS_POWER_STATE_D0 in ghlit1_xchx16/dev_xtl_ep_pcfg_gpu.ref
        //-----------------------------------------------------
        data = lwgpu->CfgRd32(LW_EP_PCFG_GPU_CONTROL_AND_STATUS);
        data = FLD_SET_DRF(_EP_PCFG_GPU, _CONTROL_AND_STATUS, _POWER_STATE, _D0, data);
        lwgpu->CfgWr32(LW_EP_PCFG_GPU_CONTROL_AND_STATUS, data);

        Platform::Delay(20);

        // Check data of mailbox0
        InfoPrintf("Now make sure priv operation works - device resumed ok!\n");
        data = 0xbeefbeef;
        lwgpu->RegWr32(0x0010a040, data);
        InfoPrintf("Priv write addr 0x0010a040(LW_PPWR_FALCON_MAILBOX0), data = %x\n", data);

        data = 0xbad;
        data = lwgpu->RegRd32(0x0010a040);
        if (data != 0xbeefbeef) {
            SetStatus(TEST_FAILED);
            getStateReport()->runFailed("Failed check mailbox0.\n");
            ErrPrintf("Priv read addr 0x0010a040(LW_PPWR_FALCON_MAILBOX0), got data = %x\n", data);
            return;
        }

        if(!selfcheck_EscapeRead) {
            data = lwgpu->RegRd32(0x10bf84);
            if(data == 0){
                SetStatus(TEST_FAILED);
                getStateReport()->runFailed("Failed check idle counter.\n");
                ErrPrintf("seq_st does not goes into deepL1 state because there is no deepL1 event!\n");
                return;
            }
        }

        InfoPrintf("test deepL1 beacon_wakeup_req_de() Done !\n");
    } //endif_beacon_wakeup_req_de

    if (beacon_wakeup_req)
    {
        //-----------------------------------------------------
        // Setting up LW_XTL_EP_PRI_PWR_MGMT_CYA1 (0x00091B10)
        // Setting up LW_XTL_EP_PRI_PWR_MGMT_CYA2 (0x00091B14)
        //-----------------------------------------------------
        //  - Enable LW_XTL_EP_PRI_PWR_MGMT_CYA1/2_ASPM_L1_PLL_PD_DISABLE
        //  - Enable LW_XTL_EP_PRI_PWR_MGMT_CYA1/2_PCIPM_L1_PLL_PD_DISABLE
        //  - Enable LW_XTL_EP_PRI_PWR_MGMT_CYA1/2_CLOCK_PM_DISABLE
        data = lwgpu->RegRd32(0x00091B10);
        data = data & 0xe6f;
        lwgpu->RegWr32(0x00091B10, data);

        data = lwgpu->RegRd32(0x00091B14);
        data = data | 0xe6f;
        lwgpu->RegWr32(0x00091B14, data);


        Platform::Delay(100);

        //-----------------------------------------------------
        // Setting up LW_XTL_EP_PRI_PWR_MGMT_CYA1 (0x00091B10)
        // Setting up LW_XTL_EP_PRI_PWR_MGMT_CYA2 (0x00091B14)
        //-----------------------------------------------------
        //  - Enable LW_XTL_EP_PRI_PWR_MGMT_CYA1/2_ASPM_L1_PLL_PD_ENABLE
        //  - Enable LW_XTL_EP_PRI_PWR_MGMT_CYA1/2_PCIPM_L1_PLL_PD_ENABLE
        //  - Enable LW_XTL_EP_PRI_PWR_MGMT_CYA1/2_CLOCK_PM_ENABLE
        data = lwgpu->RegRd32(0x00091B10);
        data = data | 0x090; // keep CLKREQ deasserted
        lwgpu->RegWr32(0x00091B10, data);

        data = lwgpu->RegRd32(0x00091B14);
        data = data | 0x090; // keep CLKREQ deasserted
        lwgpu->RegWr32(0x00091B14, data);

        //-----------------------------------------------------
        // Signal pmu to start its ucode operations
        // - LW_PPWR_FALCON_MAILBOX0 @ 0x0010a040
        // - LW_PPWR_FALCON_MAILBOX1 @ 0x0010a044
        //-----------------------------------------------------
        data = 0xbeef;
        lwgpu->RegWr32(0x0010a040, data);
        InfoPrintf("Write to LW_PPWR_FALCON_MAILBOX0 to signal pmu ucode to start operation.\n");

        //-----------------------------------------------------
        // Put device to D3hot state
        //  LW_EP_PCFG_GPU_CONTROL_AND_STATUS_POWER_STATE_D3 in ghlit1_xchx16/dev_xtl_ep_pcfg_gpu.ref
        //-----------------------------------------------------
        data = lwgpu->CfgRd32(LW_EP_PCFG_GPU_CONTROL_AND_STATUS);
        data = FLD_SET_DRF(_EP_PCFG_GPU, _CONTROL_AND_STATUS, _POWER_STATE, _D3, data);
        lwgpu->CfgWr32(LW_EP_PCFG_GPU_CONTROL_AND_STATUS, data);


        //-----------------------------------------------------
        // Check LW_PPWR_FALCON_MAILBOX0/1 to gild the test
        //  - first, put device back to D0 state
        //  - check 0xbeefbeef;
        //-----------------------------------------------------
        if(selfcheck_EscapeRead) {
            InfoPrintf("Check whether goes to PDN_WAKE_UP_EVENT_WAIT state\n");
            if (Checkstate(0x40000)) return;

            //  - enable LW_XVE_PEX_PLL5_UPSTREAM_REQ_WAKE_EN (which is disabled by default)
            //data = lwgpu->RegRd32(0x00091418);
            //data = FLD_SET_DRF(_XTL_EP_PRI, _PRIV_XV_BLKCG2, _UPSTREAM_REQ_WAKE_EN, _ENABLED, data);
            //lwgpu->RegWr32(0x00091418, data);

            InfoPrintf("Check whether revert to SEQ_IDLE state\n");
            if (Checkstate(0x1)) return;
        }

        Platform::Delay(100);

        //-----------------------------------------------------
        // Put device to D0 state
        //  LW_EP_PCFG_GPU_CONTROL_AND_STATUS_POWER_STATE_D0 in ghlit1_xchx16/dev_xtl_ep_pcfg_gpu.ref
        //-----------------------------------------------------
        data = lwgpu->CfgRd32(LW_EP_PCFG_GPU_CONTROL_AND_STATUS);
        data = FLD_SET_DRF(_EP_PCFG_GPU, _CONTROL_AND_STATUS, _POWER_STATE, _D0, data);
        lwgpu->CfgWr32(LW_EP_PCFG_GPU_CONTROL_AND_STATUS, data);

        Platform::Delay(20);

        // Check data of mailbox0
        InfoPrintf("Now make sure priv operation works - device resumed ok!\n");
        data = 0xbeefbeef;
        lwgpu->RegWr32(0x0010a040, data);
        InfoPrintf("Priv write addr 0x0010a040(LW_PPWR_FALCON_MAILBOX0), data = %x\n", data);

        data = 0xbad;
        data = lwgpu->RegRd32(0x0010a040);
        if (data != 0xbeefbeef) {
            SetStatus(TEST_FAILED);
            getStateReport()->runFailed("Failed check mailbox0.\n");
            ErrPrintf("Priv read addr 0x0010a040(LW_PPWR_FALCON_MAILBOX0), got data = %x\n", data);
            return;
        }

        if(!selfcheck_EscapeRead) {
            data = lwgpu->RegRd32(0x10bf84);
            if(data == 0){
                SetStatus(TEST_FAILED);
                getStateReport()->runFailed("Failed check idle counter.\n");
                ErrPrintf("seq_st does not goes into deepL1 state because there is no deepL1 event!\n");
                return;
            }
        }

        InfoPrintf("test deepL1 beacon_wakeup_req() Done !\n");
    } //endif_beacon_wakeup_req

    if (beacon_wakeup_nondeepL1)
    {
        //-----------------------------------------------------
        // Setting up LW_XTL_EP_PRI_PWR_MGMT_CYA1 (0x00091B10)
        // Setting up LW_XTL_EP_PRI_PWR_MGMT_CYA2 (0x00091B14)
        //-----------------------------------------------------
        //  - Enable LW_XTL_EP_PRI_PWR_MGMT_CYA1/2_ASPM_L1_PLL_PD_DISABLE
        //  - Enable LW_XTL_EP_PRI_PWR_MGMT_CYA1/2_PCIPM_L1_PLL_PD_DISABLE
        //  - Enable LW_XTL_EP_PRI_PWR_MGMT_CYA1/2_CLOCK_PM_DISABLE
        data = lwgpu->RegRd32(0x00091B10);
        data = data & 0xe6f;
        lwgpu->RegWr32(0x00091B10, data);

        data = lwgpu->RegRd32(0x00091B14);
        data = data & 0xe6f;
        lwgpu->RegWr32(0x00091B14, data);

        //-----------------------------------------------------
        // Signal pmu to start its ucode operations
        // - LW_PPWR_FALCON_MAILBOX0 @ 0x0010a040
        // - LW_PPWR_FALCON_MAILBOX1 @ 0x0010a044
        //-----------------------------------------------------
        data = 0xbeef;
        lwgpu->RegWr32(0x0010a040, data);
        InfoPrintf("Write to LW_PPWR_FALCON_MAILBOX0 to signal pmu ucode to start operation.\n");

        //-----------------------------------------------------
        // Put device to D3hot state
        //  LW_EP_PCFG_GPU_CONTROL_AND_STATUS_POWER_STATE_D3 in ghlit1_xchx16/dev_xtl_ep_pcfg_gpu.ref
        //-----------------------------------------------------
        data = lwgpu->CfgRd32(LW_EP_PCFG_GPU_CONTROL_AND_STATUS);
        data = FLD_SET_DRF(_EP_PCFG_GPU, _CONTROL_AND_STATUS, _POWER_STATE, _D3, data);
        lwgpu->CfgWr32(LW_EP_PCFG_GPU_CONTROL_AND_STATUS, data);

        //-----------------------------------------------------
        // Check LW_PPWR_FALCON_MAILBOX0 to gild the test
        //  - first, put device back to D0 state
        //  - check 0xbeefbeef;
        //-----------------------------------------------------
        if(selfcheck_EscapeRead) {
            InfoPrintf("Check whether goes to PDN_WAKE_UP_EVENT_WAIT state\n");
            if (Checkstate(0x40000)) return;

            //- enable LW_XVE_PRIV_XV_BLKCG2_UPSTREAM_REQ_WAKE_EN (which is disabled by default)
            //data = lwgpu->RegRd32(0x00091418);
            //data = FLD_SET_DRF(_XTL_EP_PRI, _PRIV_XV_BLKCG2, _UPSTREAM_REQ_WAKE_EN, _ENABLED, data);
            //lwgpu->RegWr32(0x00091418, data);

            InfoPrintf("Check whether revert to SEQ_IDLE state\n");
            if (Checkstate(0x1)) return;
        }

        Platform::Delay(30);

        //-----------------------------------------------------
        // Put device to D0 state
        //  LW_EP_PCFG_GPU_CONTROL_AND_STATUS_POWER_STATE_D0 in ghlit1_xchx16/dev_xtl_ep_pcfg_gpu.ref
        //-----------------------------------------------------
        data = lwgpu->CfgRd32(LW_EP_PCFG_GPU_CONTROL_AND_STATUS);
        data = FLD_SET_DRF(_EP_PCFG_GPU, _CONTROL_AND_STATUS, _POWER_STATE, _D0, data);
        lwgpu->CfgWr32(LW_EP_PCFG_GPU_CONTROL_AND_STATUS, data);

        Platform::Delay(20);

        // Check data of mailbox0
        InfoPrintf("Now make sure priv operation works - device resumed ok!\n");
        data = 0xbeefbeef;
        lwgpu->RegWr32(0x0010a040, data);
        InfoPrintf("Priv write addr 0x0010a040(LW_PPWR_FALCON_MAILBOX0), data = %x\n", data);

        data = 0xbad;
        data = lwgpu->RegRd32(0x0010a040);
        if (data != 0xbeefbeef) {
            SetStatus(TEST_FAILED);
            getStateReport()->runFailed("Failed check mailbox0.\n");
            ErrPrintf("Priv read addr 0x0010a040(LW_PPWR_FALCON_MAILBOX0), got data = %x\n", data);
            return;
        }

        InfoPrintf("test deepL1 beacon_wakeup_nondeepL1() Done !\n");
    } //endif_beacon_wakeup_nondeepL1

    if(deepL1_for_pcode)
    {
        //-----------------------------------------------------
        // LW_EP_PCFG_GPU_CONTROL_AND_STATUS (0x00000070)  in dev_lw_xve.ref
        //  - Set LW_EP_PCFG_GPU_CONTROL_AND_STATUS (bit 1:0) to 2'b11.(L0s and L1 supported)
        //-----------------------------------------------------
        data = lwgpu->CfgRd32(LW_EP_PCFG_GPU_CONTROL_AND_STATUS);
        //data = FLD_SET_DRF_NUM(_EP_PCFG_GPU, _LINK_CONTROL_STATUS, _ASPM_CTRL, 3, data);
        lwgpu->CfgWr32(LW_EP_PCFG_GPU_CONTROL_AND_STATUS, data);

         //-----------------------------------------------------
        // Enable wakeup on PRI write to config0
        //-----------------------------------------------------
        //  - Enable LW_XVE_PRIV_XV_BLKCG2_CONFIG0_UPDATE_WAKE_EN (which is disabled by default)
        //data = lwgpu->RegRd32(0x00091418);
        //data = FLD_SET_DRF(_XTL_EP_PRI, _PRIV_XV_BLKCG2, _CONFIG0_UPDATE_WAKE_EN, _ENABLED, data);
        //lwgpu->RegWr32(0x00091418, data);
        
        //-----------------------------------------------------
        // Setting up LW_XTL_EP_PRI_PWR_MGMT_CYA1 (0x00091B10)
        // Setting up LW_XTL_EP_PRI_PWR_MGMT_CYA2 (0x00091B14)
        //-----------------------------------------------------
        //  - Enable LW_XTL_EP_PRI_PWR_MGMT_CYA1/2_ASPM_L1_PLL_PD_ENABLE
        //  - Enable LW_XTL_EP_PRI_PWR_MGMT_CYA1/2_PCIPM_L1_PLL_PD_ENABLE
        //  - Enable LW_XTL_EP_PRI_PWR_MGMT_CYA1/2_CLOCK_PM_ENABLE
        data = lwgpu->RegRd32(0x00091B10);
        data = data | 0x190;
        lwgpu->RegWr32(0x00091B10, data);

        data = lwgpu->RegRd32(0x00091B14);
        data = data | 0x190;
        lwgpu->RegWr32(0x00091B14, data);

        //  - Set LW_XPL_PL_LTSSM_LP_12_START_PLL_OFF_MIN_DLY  to 0x177 (1.5uS)
        data = lwgpu->RegRd32(0x000851B0);
        data = ((data & 0xff000000) | 0x177);
        lwgpu->RegWr32(0x000851B0, data);

        //-----------------------------------------------------
        // Delay long enough
        // During the period, deepL1 will be entered ,then pmu issue priv access thru bar0 interface of host
        // GPIO31 will toggle as deepL1 enter/exit occur several times
        //-----------------------------------------------------
        Platform::Delay(300); //delay

         //-----------------------------------------------------
        // PRI write to config0
        //-----------------------------------------------------
        //data = lwgpu->RegRd32(0x00091418);
        //data = FLD_SET_DRF(_XTL_EP_PRI, _PRIV_XV_BLKCG2, _CONFIG0_UPDATE_WAKE_EN, _ENABLED, data);
        //lwgpu->RegWr32(0x00091418, data);

        //-----------------------------------------------------
        // Signal pmu
        // - LW_PPWR_FALCON_MAILBOX0 @ 0x0010a040
        // - LW_PPWR_FALCON_MAILBOX1 @ 0x0010a044
        //-----------------------------------------------------
        data = 0xebeb;
        lwgpu->RegWr32(0x0010a040, data);
        InfoPrintf("Write to LW_PPWR_FALCON_MAILBOX0 to set the end of the test.\n");

        if(!selfcheck_EscapeRead) {
            data = lwgpu->RegRd32(0x10bf84);
            if(data == 0){
                SetStatus(TEST_FAILED);
                getStateReport()->runFailed("Failed check idle counter.\n");
                ErrPrintf("seq_st does not goes into deepL1 state because there is no deepL1 event!\n");
                return;
            }
        }

        InfoPrintf("test deepL1 deepL1_for_pcode() Done !\n");

    }

    if(deepL1_gen2)
    {
        //-----------------------------------------------------
        // LW_EP_PCFG_GPU_CONTROL_AND_STATUS (0x00000070)  in dev_lw_xve.ref
        //  - Set LW_EP_PCFG_GPU_CONTROL_AND_STATUS (bit 1:0) to 2'b11.(L0s and L1 supported)
        //-----------------------------------------------------
        data = lwgpu->CfgRd32(LW_EP_PCFG_GPU_CONTROL_AND_STATUS);
        //data = FLD_SET_DRF_NUM(_EP_PCFG_GPU, _LINK_CONTROL_STATUS, _ASPM_CTRL, 3, data);
        lwgpu->CfgWr32(LW_EP_PCFG_GPU_CONTROL_AND_STATUS, data);
        
        //-----------------------------------------------------
        // Setting up LW_XTL_EP_PRI_PWR_MGMT_CYA1 (0x00091B10)
        // Setting up LW_XTL_EP_PRI_PWR_MGMT_CYA2 (0x00091B14)
        //-----------------------------------------------------
        //  - Enable LW_XTL_EP_PRI_PWR_MGMT_CYA1/2_ASPM_L1_PLL_PD_ENABLE
        //  - Enable LW_XTL_EP_PRI_PWR_MGMT_CYA1/2_PCIPM_L1_PLL_PD_ENABLE
        //  - Enable LW_XTL_EP_PRI_PWR_MGMT_CYA1/2_CLOCK_PM_ENABLE
        data = lwgpu->RegRd32(0x00091B10);
        data = data | 0x190;
        lwgpu->RegWr32(0x00091B10, data);

        data = lwgpu->RegRd32(0x00091B14);
        data = data | 0x190;
        lwgpu->RegWr32(0x00091B14, data);

        //  - Set LW_XPL_PL_LTSSM_LP_12_START_PLL_OFF_MIN_DLY  to 0x177 (1.5uS)
        data = lwgpu->RegRd32(0x000851B0);
        data = ((data & 0xff000000) | 0x177);
        lwgpu->RegWr32(0x000851B0, data);

         //-----------------------------------------------------
        // Enable wakeup on PRI write to config0
        //-----------------------------------------------------
        //  - Enable LW_XVE_PRIV_XV_BLKCG2_CONFIG0_UPDATE_WAKE_EN (which is disabled by default)
        //data = lwgpu->RegRd32(0x00091418);
        //data = FLD_SET_DRF(_XTL_EP_PRI, _PRIV_XV_BLKCG2, _CONFIG0_UPDATE_WAKE_EN, _ENABLED, data);
        //lwgpu->RegWr32(0x00091418, data);

        //------------------------------------------------------
        // Switch to Gen2 speed
        //------------------------------------------------------
        InfoPrintf("Switching Kepler chip to gen2 speed.\n");
        //- set LW_XPL_PL_LTSSM_LINK_CONFIG_0 (0x84000 + 0x00001008)
        // LW_EP_PCFG_GPU_LINK_CONTROL_STATUS_2_TARGET_LINK_SPEED_LINK_SPEED_5P0
        data = lwgpu->CfgRd32(LW_EP_PCFG_GPU_LINK_CONTROL_STATUS_2);
        data = (data & 0xfffffff0) | 0x2;
        lwgpu->CfgWr32(LW_EP_PCFG_GPU_LINK_CONTROL_STATUS_2, data);
        Platform::Delay(20); //delay
        data = lwgpu->RegRd32(0x00085008);
        data = FLD_SET_DRF(_XPL, _PL_LTSSM_LINK_CONFIG_0, _TARGET_LINK_SPEED, _5000_MTPS,
                           data);
        data = FLD_SET_DRF(_XPL, _PL_LTSSM_LINK_CONFIG_0, _LTSSM_DIRECTIVE,
                           _CHANGE_SPEED, data);
        lwgpu->RegWr32(0x00085008, data);
        InfoPrintf("Writing to :LW_XPL_PL_LTSSM_LINK_CONFIG_0 0x%x\n", data);
        InfoPrintf("Done setting regs. for gen2 speed.\n");

        //-----------------------------------------------------
        // Delay long enough
        // During the period, deepL1 will be entered ,then pmu issue priv access thru bar0 interface of host
        // GPIO31 will toggle as deepL1 enter/exit occur several times
        //-----------------------------------------------------
        Platform::Delay(300); //delay

         //-----------------------------------------------------
        // PRI write to config0
        //-----------------------------------------------------
        //data = lwgpu->RegRd32(0x00091418);
        //data = FLD_SET_DRF(_XTL_EP_PRI, _PRIV_XV_BLKCG2, _CONFIG0_UPDATE_WAKE_EN, _ENABLED, data);
        //lwgpu->RegWr32(0x00091418, data);

        //-----------------------------------------------------
        // Signal pmu
        // - LW_PPWR_FALCON_MAILBOX0 @ 0x0010a040
        // - LW_PPWR_FALCON_MAILBOX1 @ 0x0010a044
        //-----------------------------------------------------
        data = 0xebeb;
        lwgpu->RegWr32(0x0010a040, data);
        InfoPrintf("Write to LW_PPWR_FALCON_MAILBOX0 to set the end of the test.\n");

        if(!selfcheck_EscapeRead) {
            data = lwgpu->RegRd32(0x10bf84);
            if(data == 0){
                SetStatus(TEST_FAILED);
                getStateReport()->runFailed("Failed check idle counter.\n");
                ErrPrintf("seq_st does not goes into deepL1 state because there is no deepL1 event!\n");
                return;
            }
        }

        InfoPrintf("test deepL1 deepL1_gen2() Done !\n");
        // Switch back to Gen3 speed
        //------------------------------------------------------
        InfoPrintf("Switching Kepler chip back to gen5 speed.\n");
        //- set LW_XPL_PL_LTSSM_LINK_CONFIG_0 (0x84000 + 0x00001008)
        // LW_EP_PCFG_GPU_LINK_CONTROL_STATUS_2_TARGET_LINK_SPEED_LINK_SPEED_5P0
        data = lwgpu->CfgRd32(LW_EP_PCFG_GPU_LINK_CONTROL_STATUS_2);
        data = (data & 0xfffffff0) | 0x5;
        lwgpu->CfgWr32(LW_EP_PCFG_GPU_LINK_CONTROL_STATUS_2, data);
        Platform::Delay(20); //delay
        data = lwgpu->RegRd32(0x00085008);
        data = FLD_SET_DRF(_XPL, _PL_LTSSM_LINK_CONFIG_0, _TARGET_LINK_SPEED, _32000_MTPS,
                           data);
        data = FLD_SET_DRF(_XPL, _PL_LTSSM_LINK_CONFIG_0, _LTSSM_DIRECTIVE,
                           _CHANGE_SPEED, data);
        lwgpu->RegWr32(0x00085008, data);
        InfoPrintf("Writing to :LW_XPL_PL_LTSSM_LINK_CONFIG_0 0x%x\n", data);
        InfoPrintf("Done setting regs. for gen5 speed.\n");
    }
    SetStatus(TEST_SUCCEEDED);
    getStateReport()->crcCheckPassedGold();

    ch->WaitForIdle();
}//end_Run
