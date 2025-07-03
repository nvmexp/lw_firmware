/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2010      by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

// To facilitate faster compiles via pre-compiled headers, keep "stdtest.h" first.
#include "mdiag/tests/stdtest.h"

#include "gp104_dvco_sanity.h"
#include "mdiag/lwgpu.h"
#include "mdiag/utils/lwgpu_channel.h"
#include "mdiag/tests/test_state_report.h"
#include "core/include/gpu.h"

static bool gp104_dvco_sanity = false;

extern const ParamDecl gp104_dvco_sanity_params[] =
{
    SIMPLE_PARAM("-gp104_dvco_sanity", "add gp104 dvco sanity check"),
    PARAM_SUBDECL(lwgpu_single_params),
    LAST_PARAM
};

Gp104DvcoSanity::Gp104DvcoSanity(ResourceController *controller, ArgReader *params) :
    LWGpuSingleChannelTest(controller, params)
{
    if(params->ParamPresent("-gp104_dvco_sanity"))
      gp104_dvco_sanity = 1;
}

Gp104DvcoSanity::~Gp104DvcoSanity(void)
{
}

STD_TEST_FACTORY(gp104_dvco_sanity, Gp104DvcoSanity);

int
Gp104DvcoSanity::Setup(void)
{
    lwgpu = (LWGpuResource*)controller->FindFirstResource("gpu", -1);
    lwgpu->AcquireResource(Resource::ACQUIRE_SHARED);

    ch = lwgpu->CreateChannel();
    if(!ch)
    {
        lwgpu->ReleaseResource();
        ErrPrintf("Gp104DvcoSanity: Setup failed to create channel\n");
        return (0);
    }

    m_regMap = lwgpu->GetRegisterMap();
    if(!m_regMap)
    {
        ErrPrintf("Gp104DvcoSanity: gp104dvcosanity can only be run on GPU's that support a register map!\n");
        return (0);
    }

    getStateReport()->init("gp104_dvco_sanity");
    getStateReport()->enable();

    return 1;
}

void
Gp104DvcoSanity::CleanUp(void)
{
   if(ch)
    {
        delete ch;
        ch = 0;
    }

    if (lwgpu)
    {
        DebugPrintf("Gp104DvcoSanity::CleanUp(): Releasing GPU.\n");
        lwgpu->ReleaseResource();
        lwgpu = 0;
    }
    LWGpuSingleChannelTest::CleanUp();

}

void
Gp104DvcoSanity::Run(void)
{
  //  InfoPrintf Flag to determine whether the test fail because of elw issue or test issue.
    InfoPrintf("FLAG : gp104_connect_sanity starting ....\n");

    SetStatus(TEST_INCOMPLETE);

    if(gp104_dvco_sanity)   errCnt += gp104_dvco_sanity_check();

     if(errCnt)
      {
        SetStatus(TEST_FAILED);
        ErrPrintf("Gp104DvcoSanity::gp104_dvco_sanity_check test failed\n");
        return;
      }
     else
      {
        SetStatus(TEST_SUCCEEDED);
        getStateReport()->crcCheckPassedGold();
        return;
      }

    InfoPrintf("gp104_dvco_sanity test END.\n");
}

int Gp104DvcoSanity::gp104_dvco_sanity_check(){
 //   UINT32 data;
//    UINT32 gpcclk_count = 0, new_gpcclk_count = 0, gpcclk_count0 = 0;
    UINT32 wr_data0 = 0x0;
    UINT32 wr_data1 = 0X0;
    const UINT32 interval_ns = 10000;
    int freq_mhz = 0x0;
    InfoPrintf("Starting gp10x dvco sanity check\n");
    lwgpu->SetRegFldDef("LW_PTRIM_SYS_GPC2CLK_REF_SWITCH", "_FINALSEL", "_ONESRCCLK"); //step 2
    lwgpu->SetRegFldDef("LW_PTRIM_GPC_BCAST_GPCPLL_DYN", "_MODE", "_NAFLL"); //step 3
    lwgpu->SetRegFldNum("LW_PTRIM_GPC_BCAST_GPCLUT_SW_FREQ_REQ", "", 0x300003cd);//config LW_PTRIM_GPC_BCAST_GPCLUT_SW_FREQ_REQ
    lwgpu->SetRegFldNum("LW_PTRIM_GPC_BCAST_GPCNAFLL_CFG1", "", 0x38);//step 8 ,a;step 8 ,b, use NAFLL_CFG2 default value
    lwgpu->SetRegFldNum("LW_PTRIM_GPC_BCAST_GPCNAFLL_COEFF", "",0xff000008);//step 8 ,c
    lwgpu->SetRegFldNum("LW_PTRIM_GPC_BCAST_GPCNAFLL_CTRL2", "", 0x084a0000);//step 8 ,d
    lwgpu->SetRegFldNum("LW_PTRIM_GPC_BCAST_GPCNAFLL_CFG1", "", 0x39);//step e
    lwgpu->SetRegFldNum("LW_PTRIM_GPC_BCAST_GPCNAFLL_CFG1", "", 0x31);//step f
    lwgpu->SetRegFldDef("LW_PTRIM_GPC_BCAST_SEL_VCO", "_GPC2CLK_OUT", "_VCO");//step g,step h not needed
    lwgpu->SetRegFldNum("LW_PTRIM_GPC_BCAST_GPCNAFLL_DVCO_SLOW", "", 0x00100104);//setup dvco ratio

//trigger slowdown
    lwgpu->SetRegFldNum("LW_THERM_GRAD_STEPPING_TABLE(0)", "", 0x091c50c1);
    lwgpu->SetRegFldNum("LW_THERM_GRAD_STEPPING_TABLE(1)", "", 0x0e34c2ca);
    lwgpu->SetRegFldNum("LW_THERM_GRAD_STEPPING0", "", 0x11);
    lwgpu->SetRegFldNum("LW_THERM_CLK_TIMING_0(0)", "", 0x10000);
    lwgpu->SetRegFldNum("LW_THERM_CLK_TIMING_0(1)", "", 0x10000);
    lwgpu->SetRegFldNum("LW_THERM_CLK_TIMING_0(2)", "", 0x10000);
    lwgpu->SetRegFldNum("LW_THERM_CLK_TIMING_0(3)", "", 0x10000);
    lwgpu->SetRegFldNum("LW_THERM_CLK_TIMING_0(4)", "", 0x10000);
    lwgpu->SetRegFldNum("LW_THERM_CONFIG2", "", 0x80001000);

    lwgpu->SetRegFldNum("LW_THERM_CLK_SLOWDOWN_1(0)", "", 0x2);//trigger slow down

    Platform::Delay(500);
//    Platform::EscapeWrite("LW_GPC0_CLK_HIER_clk_mon_reset_gpu", 0, 1, 1);//reset counter
//    Platform::EscapeWrite("LW_GPC0_CLK_HIER_clk_mon_enable_gpu", 0, 1, 1);  // enable the clock monitor
//    Platform::EscapeWrite("LW_GPC0_CLK_HIER_clk_mon_ref_cnt_out_gpu", 0, 32, 0xffffffff);
//    Delay(4);
//    Platform::EscapeWrite("LW_GPC0_CLK_HIER_clk_mon_reset_gpu", 0, 1, 0);//~reset counter
//    Delay(4);
//    Platform::EscapeRead("clk_GMG0GPC0_gpc_clks_gpcclk_noeg_gmgg0gp_gd_gpu_count_LW_GPC0_CLK_HIER", 0x00,32,&gpcclk_count0);
//    Delay(40);
//    Platform::EscapeRead("clk_GMG0GPC0_gpc_clks_gpcclk_noeg_gmgg0gp_gd_gpu_count_LW_GPC0_CLK_HIER", 0x00,32,&new_gpcclk_count);
//    gpcclk_count = (new_gpcclk_count - gpcclk_count0)/10;
//    InfoPrintf("000 checkclkcount got %d gpcclks in 4us, gpcclk_count0 = %d and new_gpcclk_count = %d \n", gpcclk_count, gpcclk_count0, new_gpcclk_count);
    lwgpu->SetRegFldNum("LW_PTRIM_GPC_BCAST_CLK_CNTR_NCGPCCLK_CFG", "_NOOFIPCLKS", 0, 0, &wr_data0);
    lwgpu->SetRegFldDef("LW_PTRIM_GPC_BCAST_CLK_CNTR_NCGPCCLK_CFG", "_WRITE_EN", "_ASSERTED", 0, &wr_data0);
    lwgpu->SetRegFldNum("LW_PTRIM_GPC_BCAST_CLK_CNTR_NCGPCCLK_CFG", "_MODE", 1, 0, &wr_data0);
    lwgpu->SetRegFldDef("LW_PTRIM_GPC_BCAST_CLK_CNTR_NCGPCCLK_CFG", "_ENABLE", "_DEASSERTED", 0, &wr_data0);
    lwgpu->SetRegFldDef("LW_PTRIM_GPC_BCAST_CLK_CNTR_NCGPCCLK_CFG", "_RESET", "_ASSERTED", 0, &wr_data0);
    lwgpu->SetRegFldDef("LW_PTRIM_GPC_BCAST_CLK_CNTR_NCGPCCLK_CFG", "_SOURCE", "_GPCCLK", 0, &wr_data0);
    // Assuming that interval_ns is multiple of 1000
    lwgpu->SetRegFldNum("LW_PTRIM_GPC_BCAST_CLK_CNTR_NCGPCCLK_CFG", "_NOOFIPCLKS", 27*interval_ns/1000, 0, &wr_data1);
    lwgpu->SetRegFldDef("LW_PTRIM_GPC_BCAST_CLK_CNTR_NCGPCCLK_CFG", "_WRITE_EN", "_ASSERTED", 0, &wr_data1);
    lwgpu->SetRegFldNum("LW_PTRIM_GPC_BCAST_CLK_CNTR_NCGPCCLK_CFG", "_MODE", 1, 0, &wr_data1);
    lwgpu->SetRegFldDef("LW_PTRIM_GPC_BCAST_CLK_CNTR_NCGPCCLK_CFG", "_ENABLE", "_ASSERTED", 0, &wr_data1);
    lwgpu->SetRegFldDef("LW_PTRIM_GPC_BCAST_CLK_CNTR_NCGPCCLK_CFG", "_RESET", "_DEASSERTED", 0, &wr_data1);
    lwgpu->SetRegFldDef("LW_PTRIM_GPC_BCAST_CLK_CNTR_NCGPCCLK_CFG", "_SOURCE", "_GPCCLK", 0, &wr_data1);

    lwgpu->SetRegFldNum("LW_PTRIM_GPC_BCAST_CLK_CNTR_NCGPCCLK_CFG", "",wr_data0 );
    InfoPrintf("ThermSlowdown: Starting all gpcclk counters\n");
    lwgpu->SetRegFldNum("LW_PTRIM_GPC_BCAST_CLK_CNTR_NCGPCCLK_CFG", "",wr_data1 );
    // Delay
    Platform::Delay(80);

    InfoPrintf("wr_data0 is 0x%x.\n", wr_data0);
    lwgpu->GetRegFldNum("LW_PTRIM_GPC_BCAST_CLK_CNTR_NCGPCCLK_CNT", "", &wr_data0);
    InfoPrintf("wr_data0 is 0x%x.\n", wr_data0);

    freq_mhz = wr_data0/10;
    InfoPrintf("wr_data0 = 0x%x,gpcclk freq is %d mHz.\n",wr_data0,freq_mhz);
    //when 187 <= freq_mhz <= 193, it pass. otherwise it fail.
    if( ((190 -freq_mhz) > 3) | ((freq_mhz - 190) > 3) )
     {
       ErrPrintf("dvco freq Error!\n");
       return(1);
     }
    Platform::Delay(50);
    lwgpu->SetRegFldNum("LW_THERM_CLK_SLOWDOWN_1(0)", "", 0x0);//clear slow down req
    Platform::Delay(500);

    lwgpu->SetRegFldNum("LW_PTRIM_GPC_BCAST_CLK_CNTR_NCGPCCLK_CFG", "_NOOFIPCLKS", 0, 0, &wr_data0);
    lwgpu->SetRegFldDef("LW_PTRIM_GPC_BCAST_CLK_CNTR_NCGPCCLK_CFG", "_WRITE_EN", "_ASSERTED", 0, &wr_data0);
    lwgpu->SetRegFldNum("LW_PTRIM_GPC_BCAST_CLK_CNTR_NCGPCCLK_CFG", "_MODE", 1, 0, &wr_data0);
    lwgpu->SetRegFldDef("LW_PTRIM_GPC_BCAST_CLK_CNTR_NCGPCCLK_CFG", "_ENABLE", "_DEASSERTED", 0, &wr_data0);
    lwgpu->SetRegFldDef("LW_PTRIM_GPC_BCAST_CLK_CNTR_NCGPCCLK_CFG", "_RESET", "_ASSERTED", 0, &wr_data0);
    lwgpu->SetRegFldDef("LW_PTRIM_GPC_BCAST_CLK_CNTR_NCGPCCLK_CFG", "_SOURCE", "_GPCCLK", 0, &wr_data0);
    // Assuming that interval_ns is multiple of 1000
    lwgpu->SetRegFldNum("LW_PTRIM_GPC_BCAST_CLK_CNTR_NCGPCCLK_CFG", "_NOOFIPCLKS", 27*interval_ns/1000, 0, &wr_data1);
    lwgpu->SetRegFldDef("LW_PTRIM_GPC_BCAST_CLK_CNTR_NCGPCCLK_CFG", "_WRITE_EN", "_ASSERTED", 0, &wr_data1);
    lwgpu->SetRegFldNum("LW_PTRIM_GPC_BCAST_CLK_CNTR_NCGPCCLK_CFG", "_MODE", 1, 0, &wr_data1);
    lwgpu->SetRegFldDef("LW_PTRIM_GPC_BCAST_CLK_CNTR_NCGPCCLK_CFG", "_ENABLE", "_ASSERTED", 0, &wr_data1);
    lwgpu->SetRegFldDef("LW_PTRIM_GPC_BCAST_CLK_CNTR_NCGPCCLK_CFG", "_RESET", "_DEASSERTED", 0, &wr_data1);
    lwgpu->SetRegFldDef("LW_PTRIM_GPC_BCAST_CLK_CNTR_NCGPCCLK_CFG", "_SOURCE", "_GPCCLK", 0, &wr_data1);

    lwgpu->SetRegFldNum("LW_PTRIM_GPC_BCAST_CLK_CNTR_NCGPCCLK_CFG", "",wr_data0 );
    Platform::Delay(10);
    InfoPrintf("111 ThermSlowdown: Starting all gpcclk counters\n");
    lwgpu->SetRegFldNum("LW_PTRIM_GPC_BCAST_CLK_CNTR_NCGPCCLK_CFG", "",wr_data1 );
    // Delay
    Platform::Delay(80);

    InfoPrintf("wr_data0 is 0x%x.\n", wr_data0);
    lwgpu->GetRegFldNum("LW_PTRIM_GPC_BCAST_CLK_CNTR_NCGPCCLK_CNT", "", &wr_data0);
    InfoPrintf("wr_data0 is 0x%x.\n", wr_data0);

    freq_mhz = wr_data0/10;
    InfoPrintf("111 wr_data0 = 0x%x,gpcclk freq is %d mHz.\n",wr_data0,freq_mhz);
    //when 1450 <= freq_mhz <= 1550, it pass. otherwise it fail.
    if( ((1500 -freq_mhz) > 50) | ((freq_mhz - 1500) > 50) )
     {
       ErrPrintf("111 dvco freq Error!\n");
       return(1);
     }
    Platform::Delay(50);
///    Platform::EscapeWrite("LW_GPC0_CLK_HIER_clk_mon_reset_gpu", 0, 1, 1);//reset counter
///    Delay(4);
///    Platform::EscapeWrite("LW_GPC0_CLK_HIER_clk_mon_reset_gpu", 0, 1, 0);//~reset counter
///    Delay(4);
///    Platform::EscapeRead("clk_GMG0GPC0_gpc_clks_gpcclk_noeg_gmgg0gp_gd_gpu_count_LW_GPC0_CLK_HIER", 0x00,32,&gpcclk_count0);
///    Delay(40);
///    Platform::EscapeRead("clk_GMG0GPC0_gpc_clks_gpcclk_noeg_gmgg0gp_gd_gpu_count_LW_GPC0_CLK_HIER", 0x00,32,&new_gpcclk_count);
///    gpcclk_count = (new_gpcclk_count - gpcclk_count0)/10;
///    InfoPrintf("111 checkclkcount got %d gpcclks in 4us, gpcclk_count0 = %d and new_gpcclk_count = %d \n", gpcclk_count, gpcclk_count0, new_gpcclk_count);

    Platform::Delay(50);
    return(0);
}

