/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2006-2012, 2014-2017, 2020-2021 by LWPU Corporation.  All rights 
 * reserved.  All information contained herein is proprietary and confidential 
 * to LWPU Corporation.  Any use, reproduction, or disclosure without the 
 * written permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */
// To facilitate faster compiles via pre-compiled headers, keep "stdtest.h" first
#include "mdiag/tests/stdtest.h"
#include "ampere/ga102/dev_fuse.h"

/*
1.      There is a script //hw/tesla2_mcp89/lwtools/rtl/scripts/gen_clamp_value_checker.pl , it will be called (//hw/tesla2_mcp89/diag/tests/Makeppfile) when building igt21a to generated a clamp-value-checker(//hw/tesla2_mcp89/diag/tests/<project>/clamp_value_checker.vh).
The clamp-value-checker will classify each clamp attribute like below:

assign clamp_to_0[8] = 0 // clamp_control = PD_tpc0__clamp
    | (|`LW_CHIP_HIER.gr.tpc0.smc.smc2sm_segid_vld_0_mask[6:0])
    | (|`LW_CHIP_HIER.gr.tpc0.tex.f_stage.f2m_status[2:0])
    | `LW_CHIP_HIER.gr.tpc0.tex.m_stage.tex_d_dbg_pm_sel

assign clamp_to_0[4] = 0 // clamp_control = PD_rop0__clamp
    | `LW_CHIP_HIER.gr.zrop0.zbar2zrop_credit
    | `LW_CHIP_HIER.gr.crop0.crd2fbba_p_vld
    | `LW_CHIP_HIER.gr.zrop0.zrd2fbba_p_vld

Signal clamp_to_0[12:0] is ORed by the clamp-signal which should be 1'b0 after clamping.
Signal clamp_to_1[12:0] is ANDed by the clamp-signal which should be 1'b1 after clamping.

Clamp_to_0 and clamp_to_1 are 13-bit signal, each clamp enable signal (gx_clamp, vd_clamp, tpc_clamp, rop_clamp  and so on) will affect the corresponding bit.

2.      Check_clamp.cpp (under //sw/dev/gpu_drv/chips_a/diag/mods/mdiag/tests/gpu/misc) will enable the clamp-enable-signal and then EscapeRead clamp_to_0/clamp_to_1 to make sure whether the corresponding bit is expected.
*/

#include "check_clamp.h"
#include "mdiag/lwgpu.h"
#include "mdiag/utils/lwgpu_channel.h"
#include "mdiag/tests/test_state_report.h"

//fuse addr and enable_data
#define msdec_fspg_enable_addr  0x1b
#define msdec_fspg_enable_mask  0x100000
#define tpc_fspg_enable_addr    0x1b
#define tpc_fspg_enable_mask    0x80000
#define fb_fspg_enable_addr     0x1b
#define fb_fspg_enable_mask     0x200000

#define WRITE_FUSE_DATA(d,r,f,n,wr0,wr1,wr2,wr3,wr4,wr5,wr6,wr7,wr8,wr9,wr10,wr11,wr12,wr13,wr14,wr15,wr16,wr17,wr18,wr19,wr20,wr21,wr22,wr23,wr24,wr25,wr26,wr27)    writeFusedata( LW##d##r, (DRF_MASK(LW##d##r##f)<<DRF_SHIFT(LW##d##r##f)), DRF_NUM(d,r,f,n), wr0,wr1,wr2,wr3,wr4,wr5,wr6,wr7,wr8,wr9,wr10,wr11,wr12,wr13,wr14,wr15,wr16,wr17,wr18,wr19,wr20,wr21,wr22,wr23,wr24,wr25,wr26,wr27)
#define WRITE_FUSE_DATA_DEFINE(d,r,f,n) WRITE_FUSE_DATA(d,r,f,n,fuseVal0_write,fuseVal1_write,fuseVal2_write,fuseVal3_write,fuseVal4_write,fuseVal5_write,fuseVal6_write,fuseVal7_write,fuseVal8_write,fuseVal9_write,fuseVal10_write,fuseVal11_write,fuseVal12_write,fuseVal13_write,fuseVal14_write,fuseVal15_write,fuseVal16_write,fuseVal17_write,fuseVal18_write,fuseVal19_write,fuseVal20_write,fuseVal21_write,fuseVal22_write,fuseVal23_write,fuseVal24_write,fuseVal25_write,fuseVal26_write,fuseVal27_write)

void Check_clamp::writeFusedata(UINT32 addr, UINT32 mask, UINT32 val, UINT32 &fusedata0,UINT32 &fusedata1,UINT32 &fusedata2,UINT32 &fusedata3,UINT32 &fusedata4,UINT32 &fusedata5,UINT32 &fusedata6,UINT32 &fusedata7,UINT32 &fusedata8,UINT32 &fusedata9,UINT32 &fusedata10,UINT32 &fusedata11,UINT32 &fusedata12,UINT32 &fusedata13,UINT32 &fusedata14,UINT32 &fusedata15,UINT32 &fusedata16,UINT32 &fusedata17,UINT32 &fusedata18,UINT32 &fusedata19,UINT32 &fusedata20,UINT32 &fusedata21,UINT32 &fusedata22,UINT32 &fusedata23,UINT32 &fusedata24,UINT32 &fusedata25,UINT32 &fusedata26,UINT32 &fusedata27)

{
    switch (addr) {
        case 0:
            fusedata0 = (fusedata0 & ~mask) | val; break;
        case 1:
            fusedata1 = (fusedata1 & ~mask) | val; break;
        case 2:
            fusedata2 = (fusedata2 & ~mask) | val; break;
        case 3:
            fusedata3 = (fusedata3 & ~mask) | val; break;
        case 4:
            fusedata4 = (fusedata4 & ~mask) | val; break;
        case 5:
            fusedata5 = (fusedata5 & ~mask) | val; break;
        case 6:
            fusedata6 = (fusedata6 & ~mask) | val; break;
        case 7:
            fusedata7 = (fusedata7 & ~mask) | val; break;
        case 8:
            fusedata8 = (fusedata8 & ~mask) | val; break;
        case 9:
            fusedata9 = (fusedata9 & ~mask) | val; break;
        case 10:
            fusedata10 = (fusedata10 & ~mask) | val; break;
        case 11:
            fusedata11 = (fusedata11 & ~mask) | val; break;
        case 12:
            fusedata12 = (fusedata12 & ~mask) | val; break;
        case 13:
            fusedata13 = (fusedata13 & ~mask) | val; break;
        case 14:
            fusedata14 = (fusedata14 & ~mask) | val; break;
        case 15:
            fusedata15 = (fusedata15 & ~mask) | val; break;
        case 16:
            fusedata16 = (fusedata16 & ~mask) | val; break;
        case 17:
            fusedata17 = (fusedata17 & ~mask) | val; break;
        case 18:
            fusedata18 = (fusedata18 & ~mask) | val; break;
        case 19:
            fusedata19 = (fusedata19 & ~mask) | val; break;
        case 20:
            fusedata20 = (fusedata20 & ~mask) | val; break;
        case 21:
            fusedata21 = (fusedata21 & ~mask) | val; break;
        case 22:
            fusedata22 = (fusedata22 & ~mask) | val; break;
        case 23:
            fusedata23 = (fusedata23 & ~mask) | val; break;
        case 24:
            fusedata24 = (fusedata24 & ~mask) | val; break;
        case 25:
            fusedata25 = (fusedata25 & ~mask) | val; break;
        case 26:
            fusedata26 = (fusedata26 & ~mask) | val; break;
        case 27:
            fusedata27 = (fusedata27 & ~mask) | val; break;
    }
}

// define which tests are run
static int clamp_vd   = 0;
static int clamp_gx   = 0;
static int fspg_msdec = 0;
static int fspg_sm_tpc    = 0;
static int fspg_tpc    = 0;
static int fspg_fb  = 0;
static int clamp_vic   = 0;
static int fspg_vic  = 0;

extern const ParamDecl check_clamp_params[] = {
    SIMPLE_PARAM("-clamp_vd", "Testing clamp_vd"),
    SIMPLE_PARAM("-clamp_gx", "Testing clamp_gx"),
    SIMPLE_PARAM("-fspg_msdec", "Testing fspg_msdec"),
    SIMPLE_PARAM("-fspg_sm_tpc",  "Testing fspg_sm_tpc"),
    SIMPLE_PARAM("-fspg_tpc",  "Testing fspg_tpc"),
    SIMPLE_PARAM("-fspg_fb",  "Testing fspg_fb"),
    SIMPLE_PARAM("-clamp_vic", "Testing clamp_vic"),
    SIMPLE_PARAM("-fspg_vic",  "Testing fspg_vic"),

    PARAM_SUBDECL(lwgpu_single_params),
    LAST_PARAM
};

// constructor
Check_clamp::Check_clamp(ArgReader *params) :
    LWGpuSingleChannelTest(params)
{
    if (params->ParamPresent("-clamp_vd"))
        clamp_vd = 1;
    if (params->ParamPresent("-clamp_gx"))
        clamp_gx = 1;
    if (params->ParamPresent("-fspg_msdec"))
        fspg_msdec = 1;
    if (params->ParamPresent("-fspg_sm_tpc"))
        fspg_sm_tpc = 1;
    if (params->ParamPresent("-fspg_tpc"))
        fspg_tpc = 1;
    if (params->ParamPresent("-fspg_fb"))
        fspg_fb = 1;
    if (params->ParamPresent("-clamp_vic"))
        clamp_vic = 1;
    if (params->ParamPresent("-fspg_vic"))
        fspg_vic = 1;
}

// destructor
Check_clamp::~Check_clamp(void)
{
}

// Factory
STD_TEST_FACTORY(check_clamp,Check_clamp)

// setup method
int Check_clamp::Setup(void)
{
    lwgpu = LWGpuResource::FindFirstResource();
    m_arch = lwgpu->GetArchitecture();
    ch = lwgpu->CreateChannel();
    if(!ch)
    {
        ErrPrintf("Check_clamp::Setup failed to create channel\n");
        return 0;
    }
    getStateReport()->init("Check_clamp");
    getStateReport()->enable();

    return 1;
}

//CleanUp
void Check_clamp::CleanUp(void)
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

int Check_clamp::Read_and_check_clamp_enable(UINT32 length, UINT32 clamp_0_bit, UINT32 clamp_1_bit, int i)
{
    UINT32 clamp_to_0,clamp_to_1;
    int k;
    UINT32 length_1 = 0;
    for (k=length-1; k>=0; k--)  length_1 = length_1 | 1<<k ;
    InfoPrintf("length_1 = %x \n", length_1);
    lwgpu->SetRegFldNum("LW_PPWR_FALCON_MAILBOX0", "_DATA", i);
    Platform::Delay(12);
    Platform:: EscapeRead("clamp_to_0", 0, length, &clamp_to_0);
    Platform:: EscapeRead("clamp_to_1", 0, length, &clamp_to_1);
    if(((clamp_to_0 & clamp_0_bit) != 0x0) | ((clamp_to_1 | clamp_1_bit) != length_1)){
      ErrPrintf("Check_clamp enable fails at step %x; clamp_to_0 expected is %x, real is %x; clamp_to_1 expected is %x, real is %x \n",i, ~clamp_0_bit & length_1 ,clamp_to_0 ,~clamp_1_bit & length_1 ,clamp_to_1);
      SetStatus(TEST_FAILED);
      getStateReport()->runFailed("Failed Read_and_check_clamp_enable\n");
      return(1);
      }
    return(0);
}

int Check_clamp::Read_and_check_clamp_disable(UINT32 length, UINT32 clamp_0_bit, UINT32 clamp_1_bit, int i)
{
    UINT32 clamp_to_0,clamp_to_1;
    int k;
    UINT32 length_1 = 0;
    for (k=length-1; k>=0; k--)  length_1 = length_1 | 1<<k ;
    InfoPrintf("length_1 = %x \n", length_1);
    lwgpu->SetRegFldNum("LW_PPWR_FALCON_MAILBOX0", "_DATA", i);
    Platform::Delay(12);
    Platform:: EscapeRead("clamp_to_0", 0, length, &clamp_to_0);
    Platform:: EscapeRead("clamp_to_1", 0, length, &clamp_to_1);
    if((clamp_to_0 & clamp_0_bit) != clamp_0_bit){
      ErrPrintf("Check_clamp disable fails at step %x; clamp_to_0 expected is %x, real is %x.\n",i, ~clamp_0_bit & length_1 ,clamp_to_0);
      SetStatus(TEST_FAILED);
      getStateReport()->runFailed("Failed Read_and_check_clamp_disable\n");
      return(1);
      }
    return(0);
}

int Check_clamp::senseFuse(void)
{
/*This fuction program one fuse with the address addr and value data
*/
    UINT32 i;
    UINT32 fuse_state;
    int errCnt = 0;
    GpuSubdevice *pSubdev = lwgpu->GetGpuSubdevice();
    InfoPrintf("FuseTest:  senseFuse begins\n");
    lwgpu->RegWr32(LW_FUSE_FUSEADDR,1);

//    lwgpu->RegWr32(LW_FUSE_FUSERDATA,data);
//make sure the state machine is in idle state
    pSubdev->Regs().Write32(MODS_FUSE_FUSECTRL_CMD_SENSE_CTRL);
    i=0;
    do {
        i++;
        fuse_state = pSubdev->Regs().Read32(MODS_FUSE_FUSECTRL_STATE);
    } while ((fuse_state != LW_FUSE_FUSECTRL_STATE_IDLE) && (i<8000));

    if (fuse_state != LW_FUSE_FUSECTRL_STATE_IDLE) {
        ErrPrintf("FuseTest: Error in senseFuse. Timed out waiting for fuse_state to return to IDLE . Last state = %d Iteration = %d\n", fuse_state, i);
        errCnt++;
    }
    InfoPrintf("FuseTest:  senseFuse ends, Iterations = %d\n",i);
    return errCnt;

}

int Check_clamp::programFuse(UINT32 addr, UINT32 data, UINT32 mask)

{
/*This fuction program one fuse with the address addr and value data
*/
    UINT32 i;
    UINT32 fuse_state;
    UINT32 orig_data;
    int errCnt = 0;
    GpuSubdevice *pSubdev = lwgpu->GetGpuSubdevice();
    InfoPrintf("FuseTest:  programFuse begins\n");

    //Read orignal value
    lwgpu->RegWr32(LW_FUSE_FUSEADDR,addr);
    InfoPrintf("FuseTest:  Write FUSECTRL_CMD to READ\n");
    pSubdev->Regs().Write32(MODS_FUSE_FUSECTRL_CMD_READ);
    i=0;
    do {
        i++;
        fuse_state = pSubdev->Regs().Read32(MODS_FUSE_FUSECTRL_STATE);
    } while ((fuse_state != LW_FUSE_FUSECTRL_STATE_IDLE) && (i<8000));

    if (fuse_state != LW_FUSE_FUSECTRL_STATE_IDLE) {
        ErrPrintf("FuseTest: Error in programFuse. Timed out waiting for fuse_state to return to IDLE . Last state = %d Iteration = %d\n", fuse_state, i);
        errCnt++;
    }
    lwgpu->GetRegFldNum("LW_FUSE_FUSERDATA", "_DATA", &orig_data);

    //write new value to fuse
    InfoPrintf("data = %x", data);
    data = mask & data;
    InfoPrintf("data = %x , orig_data = %x, mask = %x\n", data, orig_data, mask);
    lwgpu->RegWr32(LW_FUSE_FUSEWDATA,data);
    Platform::EscapeWrite("fuse_src_core_r",0,1,1);
    InfoPrintf("FuseTest:  Write FUSECTRL_CMD to WRTIE\n");
    pSubdev->Regs().Write32(MODS_FUSE_FUSECTRL_CMD_WRITE);
    i=0;
    do {
        i++;
        fuse_state = pSubdev->Regs().Read32(MODS_FUSE_FUSECTRL_STATE);
    } while ((fuse_state != LW_FUSE_FUSECTRL_STATE_IDLE) && (i<8000));

    if (fuse_state != LW_FUSE_FUSECTRL_STATE_IDLE) {
        ErrPrintf("FuseTest: Error in programFuse. Timed out waiting for fuse_state to return to IDLE . Last state = %d Iteration = %d\n", fuse_state, i);
        errCnt++;
    }
    Platform::EscapeWrite("fuse_src_core_r",0,1,0);

    //read again to make that new value can be seen from fuse_dataout
    InfoPrintf("FuseTest:  Write FUSECTRL_CMD to READ\n");
    pSubdev->Regs().Write32(MODS_FUSE_FUSECTRL_CMD_READ);
    i=0;
    do {
        i++;
        fuse_state = pSubdev->Regs().Read32(MODS_FUSE_FUSECTRL_STATE);
    } while ((fuse_state != LW_FUSE_FUSECTRL_STATE_IDLE) && (i<8000));

    if (fuse_state != LW_FUSE_FUSECTRL_STATE_IDLE) {
        ErrPrintf("FuseTest: Error in programFuse. Timed out waiting for fuse_state to return to IDLE . Last state = %d Iteration = %d\n", fuse_state, i);
        errCnt++;
    }

    InfoPrintf("FuseTest:  programFuse ends, Iterations = %d, address is %8x, data is %8x\n",i, addr, data);
    return errCnt;

}

void Check_clamp::updateFuseopt(void)
{
/*This fuction program one fuse with the address addr and value data
*/
    InfoPrintf("FuseTest: updateFuseopt begins \n");
    lwgpu->RegWr32(LW_FUSE_PRIV2INTFC,1);

//    lwgpu->RegWr32(LW_FUSE_FUSERDATA,data);
//make sure the state machine is in idle state
    //DelayNs(40);        //clear start to avoid reread fuse data
    Platform::Delay(1);
    lwgpu->RegWr32(LW_FUSE_PRIV2INTFC,0);
    //DelayNs(15000);     //because fuse_state cannot indicate all of fuse rows have been read out and updated chip options regiser, we should wait
    Platform::Delay(15);
   // FLD_WR_DRF_DEF(_FUSE, _FUSECTRL, _CMD, _READ);

    InfoPrintf("FuseTest: updateFuseopt ends.\n");

}

void Check_clamp::Enable_fuse_program(void)
{
   GpuSubdevice *pSubdev = lwgpu->GetGpuSubdevice();
   UINT32 fuse_state;

/*
   UINT32 fuseVal0_write = 0x00000000;
   UINT32 fuseVal1_write = 0x00000000;
   UINT32 fuseVal2_write = 0x00000000;
   UINT32 fuseVal3_write = 0x00000000;
   UINT32 fuseVal4_write = 0x00000000;
   UINT32 fuseVal5_write = 0x00000000;
   UINT32 fuseVal6_write = 0x00000000;
   UINT32 fuseVal7_write = 0x00000000;
   UINT32 fuseVal8_write = 0x00000000;
   UINT32 fuseVal9_write = 0x00000000;
   UINT32 fuseVal10_write = 0x00000000;
   UINT32 fuseVal11_write = 0x00000000;
   UINT32 fuseVal12_write = 0x00000000;
   UINT32 fuseVal13_write = 0x00000000;
   UINT32 fuseVal14_write = 0x00000000;
   UINT32 fuseVal15_write = 0x00000000;
   UINT32 fuseVal16_write = 0x00000000;
   UINT32 fuseVal17_write = 0x00000000;
   UINT32 fuseVal18_write = 0x00000000;
   UINT32 fuseVal19_write = 0x00000000;
   UINT32 fuseVal20_write = 0x00000000;
   UINT32 fuseVal21_write = 0x00000000;
   UINT32 fuseVal22_write = 0x00000000;
   UINT32 fuseVal23_write = 0x00000000;
   UINT32 fuseVal24_write = 0x00000000;
   UINT32 fuseVal25_write = 0x00000000;
   UINT32 fuseVal26_write = 0x00000000;
   UINT32 fuseVal27_write = 0x00000000;
*/

    InfoPrintf("FuseTest: About to run diag Fuse.\n");
    if (lwgpu->SetRegFldDef("LW_PBUS_DEBUG_1","_FUSES_VISIBLE","_ENABLED"))
    { ErrPrintf("Couldn't set LW_PBUS_DEBUG_1_FUSES_VISIBLE to _ENABLED\n");}
    Platform::EscapeWrite("fuse_src_core_r",0,1,0);
    Platform::EscapeWrite("bond_ignfuse_r",0,1,0);
    // Wait for idle
    InfoPrintf("FuseTest: Waiting for CMD_IDLE.\n");
    do {
        fuse_state = pSubdev->Regs().Read32(MODS_FUSE_FUSECTRL_STATE);
    } while (fuse_state != LW_FUSE_FUSECTRL_STATE_IDLE);

    Platform::Delay(1);
    InfoPrintf("FuseTest: Waiting for CMD_IDLE.\n");
    pSubdev->Regs().Write32(MODS_FUSE_FUSECTRL_CMD_READ);
    // Wait for idle
    InfoPrintf("FuseTest: READ DEFAULT FUSE VALE,Waiting for CMD_IDLE...\n");
    do {
        fuse_state = pSubdev->Regs().Read32(MODS_FUSE_FUSECTRL_STATE);
    } while (fuse_state != LW_FUSE_FUSECTRL_STATE_IDLE);

    Platform::Delay(15);    //make sure fuse_sense_done is set

    //WRITE_FUSE_DATA_DEFINE(_FUSE,_ENABLE_FUSE_PROGRAM__RED_ALIAS_0,_DATA,1);

    programFuse(0, 0x1, 0x1);
    //after write the first 2 rows, sense fuses to give the global configurations
    senseFuse();
    //programFuse(1, 0x1);
    ////after write the first 2 rows, sense fuses to give the global configurations
    //senseFuse();
}

// run - main routine
void Check_clamp::Run(void)
{

    InfoPrintf("Check_clamp Test at Run\n");
    SetStatus(TEST_INCOMPLETE);
    if (main_test())
       {
           SetStatus(TEST_FAILED);
           ErrPrintf("check_clamp test failed\n");
           return;
       }

       InfoPrintf("check_clamp() Pass !\n");

    InfoPrintf("I2cslave test complete\n");
    SetStatus(TEST_SUCCEEDED);
    getStateReport()->crcCheckPassedGold();
}

int Check_clamp::main_test(void){

    UINT32 length, clamp_0_bit, clamp_1_bit;
    UINT32 tpc_sm_fs0, tpc_enable, fb_enable, vic_enable;
    UINT32 data1;
    length = 0;
    clamp_0_bit = 0;
    clamp_1_bit = 0;
    tpc_sm_fs0 = 0;
    tpc_enable = 0;
    fb_enable  = 0;
    vic_enable = 0;
    InfoPrintf("check_clamp support/base registers used for architecture G/GT-%x\n",m_arch);

      if (m_arch == 0x218)
         { length = 0x4;
           if(clamp_gx)         { clamp_0_bit = 0xb; clamp_1_bit = 0x4;}
           else if(clamp_vd)    { clamp_0_bit = 0x4; clamp_1_bit = 0xb;}
           else if(fspg_msdec)  { clamp_0_bit = 0x4; clamp_1_bit = 0xb;}
           else if(fspg_sm_tpc) { clamp_0_bit = 0x3; clamp_1_bit = 0xc; tpc_sm_fs0 = 0x3; tpc_enable = 0x1;}
           else                 { clamp_0_bit = 0xa; clamp_1_bit = 0xa;}
         }
      else if (m_arch == 0x216)
         { length = 0xb;
           if(clamp_gx)         { clamp_0_bit = 0x5ff; clamp_1_bit = 0x200;}
           else if(clamp_vd)    { clamp_0_bit = 0x200; clamp_1_bit = 0x5ff;}
           else if(fspg_msdec)  { clamp_0_bit = 0x200; clamp_1_bit = 0x5ff;}
           else if(fspg_sm_tpc) { clamp_0_bit = 0x7f ; clamp_1_bit = 0x780; tpc_sm_fs0 = 0x7; tpc_enable = 0x3;}
           else if(fspg_tpc)    { clamp_0_bit = 0x180 ; clamp_1_bit = 0x67f;tpc_enable = 0x3;}
           else                 { clamp_0_bit = 0xb; clamp_1_bit = 0xb;}
         }
      else if (m_arch == 0x215)
         { length = 0x12;
           if(clamp_gx)         { clamp_0_bit = 0x2ffff; clamp_1_bit = 0x10000;}
           else if(clamp_vd)    { clamp_0_bit = 0x10000; clamp_1_bit = 0x2ffff;}
           else if(fspg_msdec)  { clamp_0_bit = 0x10000; clamp_1_bit = 0x2ffff;}
           else if(fspg_sm_tpc) { clamp_0_bit = 0xfff ;  clamp_1_bit = 0x3f000; tpc_enable = 0xf;}
           else if(fspg_tpc)    { clamp_0_bit = 0xf000 ; clamp_1_bit = 0x30fff; tpc_enable = 0xf;}
           else                 { clamp_0_bit = 0xc; clamp_1_bit = 0xc;}
         }

    if(clamp_gx)
    {
        InfoPrintf("clamp_gx Test starts... \n");
        InfoPrintf("length = %x, clamp_0_bit = %x, clamp_1_bit = %x\n", length, clamp_0_bit, clamp_1_bit);

        lwgpu->SetRegFldNum("LW_PPWR_PMU_ELPG_INTREN", "", 0x2111);
        lwgpu->SetRegFldDef("LW_PPWR_PMU_ELPG_CTRL", "_ENG0", "_ENABLE");
        lwgpu->SetRegFldNum("LW_PPWR_PMU_ELPG_IDLEFILTH_0", "_VALUE", 100);
        lwgpu->SetRegFldNum("LW_PPWR_PMU_ELPG_PPUIDLEFILTH_0", "_VALUE", 100);
        lwgpu->SetRegFldDef("LW_PPWR_PMU_ELPG_CFG_RDY", "_0", "_TRUE");
        Platform::Delay(5);    //wait 5us
        //set field CFG_ERR to force elpg to PG_ON state
        InfoPrintf("put elpg state to PG_ON \n");
        lwgpu->GetRegFldNum("LW_PPWR_PMU_ELPG_INTRSTAT0", "", &data1);
        lwgpu->SetRegFldNum("LW_PPWR_PMU_ELPG_INTRSTAT0", "", data1 | 0x10000);
        //set field PG_ON to force elpg to ELPG_ON_1 state
        InfoPrintf("put elpg state to ELPG_ON_1 \n");
        lwgpu->GetRegFldNum("LW_PPWR_PMU_ELPG_INTRSTAT0", "", &data1);
        lwgpu->SetRegFldNum("LW_PPWR_PMU_ELPG_INTRSTAT0", "", data1 | 0x10);
        //check clamp
        Platform::Delay(5);
        if(Read_and_check_clamp_enable(length, clamp_0_bit, clamp_1_bit, 1)) return 1;
        //set field PG_ON_DONE to force elpg to IDLE state
        lwgpu->GetRegFldNum("LW_PPWR_PMU_ELPG_INTRSTAT0", "", &data1);
        lwgpu->SetRegFldNum("LW_PPWR_PMU_ELPG_INTRSTAT0", "", data1 | 0x100);

        //=========== force elcg_if_override to 0 ============
        Platform::Delay(5);
        //override cya-enable and elpg_type, cya_clk_switch
        InfoPrintf("issue cya elcg_reg\n");
        lwgpu->GetRegFldNum("LW_PPWR_PMU_ELPG_SWCTRL0", "", &data1);
        lwgpu->SetRegFldNum("LW_PPWR_PMU_ELPG_SWCTRL0", "", data1 | 0x10300000);
        Platform::Delay(1);

        //=========== force clamp_gx to 0 ============
        //override cya-enable and cya_clamp_switch, cya_clamp_type
        lwgpu->GetRegFldNum("LW_PPWR_PMU_ELPG_SWCTRL0", "", &data1);
        lwgpu->SetRegFldNum("LW_PPWR_PMU_ELPG_SWCTRL0", "", (data1 & 0) | 0x11000000);
        Platform::Delay(1);

    }

    if(clamp_vd)
    {
    InfoPrintf("clamp_vd Test starts... \n");
    InfoPrintf("length = %x, clamp_0_bit = %x, clamp_1_bit = %x\n", length, clamp_0_bit, clamp_1_bit);

    lwgpu->SetRegFldNum("LW_PPWR_PMU_ELPG_INTREN", "", 0x21110000);
    lwgpu->SetRegFldDef("LW_PPWR_PMU_ELPG_CTRL", "_ENG1", "_ENABLE");
    lwgpu->SetRegFldNum("LW_PPWR_PMU_ELPG_IDLEFILTH_1", "_VALUE", 100);
    lwgpu->SetRegFldNum("LW_PPWR_PMU_ELPG_PPUIDLEFILTH_1", "_VALUE", 100);
    lwgpu->SetRegFldDef("LW_PPWR_PMU_ELPG_CFG_RDY", "_1", "_TRUE");
    Platform::Delay(5);    //wait 5us
    //set field CFG_ERR to force elpg to PG_ON state
    InfoPrintf("put elpg state to PG_ON \n");
    lwgpu->GetRegFldNum("LW_PPWR_PMU_ELPG_INTRSTAT1", "", &data1);
    lwgpu->SetRegFldNum("LW_PPWR_PMU_ELPG_INTRSTAT1", "", data1 | 0x10000);
    //set field PG_ON to force elpg to ELPG_ON_1 state
    InfoPrintf("put elpg state to ELPG_ON_1 \n");
    lwgpu->GetRegFldNum("LW_PPWR_PMU_ELPG_INTRSTAT1", "", &data1);
    lwgpu->SetRegFldNum("LW_PPWR_PMU_ELPG_INTRSTAT1", "", data1 | 0x10);
    //check clamp
    Platform::Delay(5);
    if(Read_and_check_clamp_enable(length, clamp_0_bit, clamp_1_bit, 1)) return 1;
    //set field PG_ON_DONE to force elpg to IDLE state
    lwgpu->GetRegFldNum("LW_PPWR_PMU_ELPG_INTRSTAT1", "", &data1);
    lwgpu->SetRegFldNum("LW_PPWR_PMU_ELPG_INTRSTAT1", "", data1 | 0x100);
    }

    if(fspg_msdec)
    {
    InfoPrintf("length = %x, clamp_0_bit = %x, clamp_1_bit = %x\n", length, clamp_0_bit, clamp_1_bit);

    if((m_arch & 0xFF0) == 0x210) {
     Enable_fuse_program();
     programFuse(msdec_fspg_enable_addr, msdec_fspg_enable_mask, msdec_fspg_enable_mask);
     updateFuseopt();
     }
     else {
     Platform::EscapeWrite("drv_opt_msdec_fspg_enable", 0, 1, 1);   //EscapeWrite fuse in igt21a and mcp89
     }

    //step 1 : enable all
    lwgpu->SetRegFldNum("LW_PBUS_FSPG", "_MSDEC", 0x1);
    lwgpu->SetRegFldNum("LW_PBUS_FS", "_MEVP", 0);
    lwgpu->SetRegFldNum("LW_PBUS_FS4", "_BSP", 0);
    if(Read_and_check_clamp_enable(length, clamp_0_bit, clamp_1_bit, 1)) return 1;

    //step 2 : disable msdec_fspg_enable_unqual
    lwgpu->SetRegFldNum("LW_PBUS_FSPG", "_MSDEC", 0);
    //if(Read_and_check_clamp_disable(length, clamp_0_bit, clamp_1_bit, 2)) return 1;

    //step 3 : enable msdec_fspg_enable_unqual
    lwgpu->SetRegFldNum("LW_PBUS_FSPG", "_MSDEC", 1);
    if(Read_and_check_clamp_enable(length, clamp_0_bit, clamp_1_bit, 3)) return 1;

    //step 4 : disable Floorsweep_mevp_unqual
    lwgpu->SetRegFldNum("LW_PBUS_FS", "_MEVP", 1);
    //if(Read_and_check_clamp_disable(length, clamp_0_bit, clamp_1_bit, 4)) return 1;

    //step 5 : enable Floorsweep_mevp_unqual
    lwgpu->SetRegFldNum("LW_PBUS_FS", "_MEVP", 0);
    if(Read_and_check_clamp_enable(length, clamp_0_bit, clamp_1_bit, 5)) return 1;

    //step 6 : disable Floorsweep4_bsp_unqual
    lwgpu->SetRegFldNum("LW_PBUS_FS4", "_BSP", 1);
    //if(Read_and_check_clamp_disable(length, clamp_0_bit, clamp_1_bit, 6)) return 1;

    //step 7 : enable Floorsweep4_bsp_unqual
    lwgpu->SetRegFldNum("LW_PBUS_FS4", "_BSP", 0);
    if(Read_and_check_clamp_enable(length, clamp_0_bit, clamp_1_bit, 7)) return 1;
    }

    if(fspg_sm_tpc) {

     if((m_arch & 0xFF0) == 0x210) {
     Enable_fuse_program();
     programFuse(tpc_fspg_enable_addr, tpc_fspg_enable_mask, tpc_fspg_enable_mask);
     updateFuseopt();
     }
     else {
     Platform::EscapeWrite("drv_opt_tpc_fspg_enable", 0, 1, 1);    //EscapeWrite fuse in igt21a and mcp89
     }

     if(m_arch != 0x215){

     //step 1 : enable all
     lwgpu->SetRegFldNum("LW_PBUS_FSPG", "_TPC", 0x1);
     lwgpu->SetRegFldNum("LW_PBUS_PER_TPC_SM_FS0", "_TPC0", 0);
     if(m_arch == 0x216){
     lwgpu->SetRegFldNum("LW_PBUS_PER_TPC_SM_FS0", "_TPC1", 0);
     }
     if(Read_and_check_clamp_enable(length, clamp_0_bit, clamp_1_bit, 1)) return 1;

     //step 2 : disable tpc_fspg_enable_unqual
     lwgpu->SetRegFldNum("LW_PBUS_FSPG", "_TPC", 0x0);
     //Read_and_check_clamp_disable(length, clamp_0_bit, clamp_1_bit, 2);

     //step 3 : enable tpc_fspg_enable_unqual
     lwgpu->SetRegFldNum("LW_PBUS_FSPG", "_TPC", 0x1);
     if(Read_and_check_clamp_enable(length, clamp_0_bit, clamp_1_bit, 3)) return 1;

     //step 4 : disable Floorsweep_sm_tpc0_unqual
     lwgpu->SetRegFldNum("LW_PBUS_PER_TPC_SM_FS0", "_TPC0", tpc_sm_fs0);
     if(m_arch == 0x216){
     lwgpu->SetRegFldNum("LW_PBUS_PER_TPC_SM_FS0", "_TPC1", tpc_sm_fs0);
     }
     //Read_and_check_clamp_disable(length, clamp_0_bit, clamp_1_bit, 4);

     //step 5 : enable Floorsweep_tpc_unqual
     lwgpu->SetRegFldNum("LW_PBUS_FS", "_TPC_ENABLE", 0);
     if(Read_and_check_clamp_enable(length, clamp_0_bit, clamp_1_bit, 5)) return 1;

     //step 6 : disable Floorsweep_tpc_unqual
     lwgpu->SetRegFldNum("LW_PBUS_FS", "_TPC_ENABLE", tpc_enable);
     //Read_and_check_clamp_disable(length, clamp_0_bit, clamp_1_bit, 6);

     //step 7 : enable Floorsweep_sm[26:24]
     lwgpu->SetRegFldNum("LW_PBUS_FS", "_SM_ENABLE", 0);
     if(Read_and_check_clamp_enable(length, clamp_0_bit, clamp_1_bit, 7)) return 1;

      }

   if (m_arch == 0x215)
   {

     //step 1: enable all
     lwgpu->SetRegFldNum("LW_PBUS_FS", "_TPC_ENABLE", 0);
     lwgpu->SetRegFldNum("LW_PBUS_FSPG", "_TPC", 0x1);
     if(Read_and_check_clamp_enable(length, clamp_0_bit, clamp_1_bit, 1)) return 1;

     //step 2: disable tpc_fspg_enable_unqual
     lwgpu->SetRegFldNum("LW_PBUS_FSPG", "_TPC", 0);
     if(Read_and_check_clamp_disable(length, clamp_0_bit, clamp_1_bit, 2)) return 1;

     //step 3: enable tpc_fspg_enable_unqual
     lwgpu->SetRegFldNum("LW_PBUS_FSPG", "_TPC", 1);
     if(Read_and_check_clamp_enable(length, clamp_0_bit, clamp_1_bit, 3)) return 1;

     //step 4: disable Floorsweep_tpc_unqual
     lwgpu->SetRegFldNum("LW_PBUS_FS", "_TPC_ENABLE", tpc_enable);
     if(Read_and_check_clamp_disable(length, clamp_0_bit, clamp_1_bit, 4)) return 1;

     //step 5: enable Floorsweep_tpc_unqual
     lwgpu->SetRegFldNum("LW_PBUS_FS", "_TPC_ENABLE", 0);
     if(Read_and_check_clamp_enable(length, clamp_0_bit, clamp_1_bit, 5)) return 1;

     //revert Floorsweep_tpc_unqual to orignal value
     lwgpu->SetRegFldNum("LW_PBUS_FS", "_TPC_ENABLE", tpc_enable);
     lwgpu->SetRegFldNum("LW_PBUS_FSPG", "_TPC", 0);
     }
    }

    if(fspg_tpc){

     //Enable fuse program and make sense_done
     if((m_arch & 0xFF0) == 0x210) {
     Enable_fuse_program();
     programFuse(tpc_fspg_enable_addr, tpc_fspg_enable_mask, tpc_fspg_enable_mask);
     updateFuseopt();
     }
     else {
     Platform::EscapeWrite("drv_opt_tpc_fspg_enable", 0, 1, 1);    //EscapeWrite fuse in igt21a and mcp89
       if (lwgpu->SetRegFldDef("LW_PBUS_DEBUG_4","_GFXCORE_CLK_EN", "_DISABLED")) {
          ErrPrintf("could not update LW_PBUS_DEBUG_4 field _GFXCORE_CLK_EN to _DISABLED\n");
       }
     }

     //step 1: enable all
     lwgpu->SetRegFldNum("LW_PBUS_FS", "_TPC_ENABLE", 0);
     lwgpu->SetRegFldNum("LW_PBUS_FSPG", "_TPC", 0x1);
     if(Read_and_check_clamp_enable(length, clamp_0_bit, clamp_1_bit, 1)) return 1;

     //step 2: disable tpc_fspg_enable_unqual
     lwgpu->SetRegFldNum("LW_PBUS_FSPG", "_TPC", 0);
     if(Read_and_check_clamp_disable(length, clamp_0_bit, clamp_1_bit, 2)) return 1;

     //step 3: enable tpc_fspg_enable_unqual
     lwgpu->SetRegFldNum("LW_PBUS_FSPG", "_TPC", 1);
     if(Read_and_check_clamp_enable(length, clamp_0_bit, clamp_1_bit, 3)) return 1;

     //step 4: disable Floorsweep_tpc_unqual
     lwgpu->SetRegFldNum("LW_PBUS_FS", "_TPC_ENABLE", tpc_enable);
     if(Read_and_check_clamp_disable(length, clamp_0_bit, clamp_1_bit, 4)) return 1;

     //step 5: enable Floorsweep_tpc_unqual
     lwgpu->SetRegFldNum("LW_PBUS_FS", "_TPC_ENABLE", 0);
     if(Read_and_check_clamp_enable(length, clamp_0_bit, clamp_1_bit, 5)) return 1;

     //revert Floorsweep_tpc_unqual to orignal value
     lwgpu->SetRegFldNum("LW_PBUS_FS", "_TPC_ENABLE", tpc_enable);
     lwgpu->SetRegFldNum("LW_PBUS_FSPG", "_TPC", 0);
    }

    if(fspg_fb){
     Platform::EscapeWrite("drv_opt_fb_fspg_enable", 0, 1, 1);    //EscapeWrite fuse in igt21a and mcp89

     //setp 1: enable all
     lwgpu->SetRegFldNum("LW_PBUS_FS", "_FB_ENABLE", 0);
     lwgpu->SetRegFldNum("LW_PBUS_FSPG", "_FB", 0x1);
     if(Read_and_check_clamp_enable(length, clamp_0_bit, clamp_1_bit, 1)) return 1;

     //step 2: disable fb_fspg_enable_unqual
     lwgpu->SetRegFldNum("LW_PBUS_FSPG", "_FB", 0);
     if(Read_and_check_clamp_disable(length, clamp_0_bit, clamp_1_bit, 2)) return 1;

     //step 3: enable fb_fspg_enable_unqual
     lwgpu->SetRegFldNum("LW_PBUS_FSPG", "_FB", 1);
     if(Read_and_check_clamp_enable(length, clamp_0_bit, clamp_1_bit, 3)) return 1;

     //step 4: disable Floorsweep_fb_unqual
     lwgpu->SetRegFldNum("LW_PBUS_FS", "_FB_ENABLE", fb_enable);
     if(Read_and_check_clamp_disable(length, clamp_0_bit, clamp_1_bit, 4)) return 1;

     //step 5: enable Floorsweep_fb_unqual
     lwgpu->SetRegFldNum("LW_PBUS_FS", "_FB_ENABLE", 0);
     if(Read_and_check_clamp_enable(length, clamp_0_bit, clamp_1_bit, 5)) return 1;

     //revert Floorsweep_fb_unqual to orignal value
     lwgpu->SetRegFldNum("LW_PBUS_FS", "_FB_ENABLE", fb_enable);
     lwgpu->SetRegFldNum("LW_PBUS_FSPG", "_FB", 0);
    }

    if(fspg_vic){
     Platform::EscapeWrite("drv_opt_vic_fspg_enable", 0, 1, 1);    //EscapeWrite fuse in igt21a and mcp89

     //setp 1: enable all
     lwgpu->SetRegFldNum("LW_PBUS_FS4", "_VIC", 0);
     lwgpu->SetRegFldNum("LW_PBUS_FSPG", "_VIC", 0x1);
     if(Read_and_check_clamp_enable(length, clamp_0_bit, clamp_1_bit, 1)) return 1;

     //step 2: disable vic_fspg_enable_unqual
     lwgpu->SetRegFldNum("LW_PBUS_FSPG", "_VIC", 0);
     if(Read_and_check_clamp_disable(length, clamp_0_bit, clamp_1_bit, 2)) return 1;

     //step 3: enable vic_fspg_enable_unqual
     lwgpu->SetRegFldNum("LW_PBUS_FSPG", "_VIC", 1);
     if(Read_and_check_clamp_enable(length, clamp_0_bit, clamp_1_bit, 3)) return 1;

     //step 4: disable Floorsweep4_eng5_unqual
     lwgpu->SetRegFldNum("LW_PBUS_FS4", "_VIC", vic_enable);
     if(Read_and_check_clamp_disable(length, clamp_0_bit, clamp_1_bit, 4)) return 1;

     //step 5: enable Floorsweep4_eng5_unqual
     lwgpu->SetRegFldNum("LW_PBUS_FS4", "_VIC", 0);
     if(Read_and_check_clamp_enable(length, clamp_0_bit, clamp_1_bit, 5)) return 1;

     //revert Floorsweep_fb_unqual to orignal value
     lwgpu->SetRegFldNum("LW_PBUS_FS4", "_VIC", vic_enable);
     lwgpu->SetRegFldNum("LW_PBUS_FSPG", "_VIC", 0);
    }

    if(clamp_vic){
    InfoPrintf("clamp_vic Test starts... \n");
    InfoPrintf("length = %x, clamp_0_bit = %x, clamp_1_bit = %x\n", length, clamp_0_bit, clamp_1_bit);

    lwgpu->SetRegFldNum("LW_PPWR_PMU_ELPG_INTREN", "", 0x8222);
    lwgpu->SetRegFldDef("LW_PPWR_PMU_ELPG_CTRL", "_ENG2", "_ENABLE");
    lwgpu->SetRegFldNum("LW_PPWR_PMU_ELPG_IDLEFILTH_2", "_VALUE", 100);
    lwgpu->SetRegFldNum("LW_PPWR_PMU_ELPG_PPUIDLEFILTH_2", "_VALUE", 100);
    lwgpu->SetRegFldDef("LW_PPWR_PMU_ELPG_CFG_RDY", "_2", "_TRUE");
    Platform::Delay(5);    //wait 5us
    //set field CFG_ERR to force elpg to PG_ON state
    InfoPrintf("put elpg state to PG_ON \n");
    lwgpu->GetRegFldNum("LW_PPWR_PMU_ELPG_INTRSTAT2", "", &data1);
    lwgpu->SetRegFldNum("LW_PPWR_PMU_ELPG_INTRSTAT2", "", data1 | 0x10000);
    //set field PG_ON to force elpg to ELPG_ON_1 state
    InfoPrintf("put elpg state to ELPG_ON_1 \n");
    lwgpu->GetRegFldNum("LW_PPWR_PMU_ELPG_INTRSTAT2", "", &data1);
    lwgpu->SetRegFldNum("LW_PPWR_PMU_ELPG_INTRSTAT2", "", data1 | 0x10);
    //check clamp
    Platform::Delay(5);
    if(Read_and_check_clamp_enable(length, clamp_0_bit, clamp_1_bit, 1)) return 1;
    //set field PG_ON_DONE to force elpg to IDLE state
    lwgpu->GetRegFldNum("LW_PPWR_PMU_ELPG_INTRSTAT2", "", &data1);
    lwgpu->SetRegFldNum("LW_PPWR_PMU_ELPG_INTRSTAT2", "", data1 | 0x100);
    }

    InfoPrintf("Check_clamp test complete\n");
    return(0);
}//end_main_test

