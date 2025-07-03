/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2008-2012,2014-2015,2018, 2020-2021 by LWPU Corporation.  All 
 * rights reserved.  All information contained herein is proprietary and 
 * confidential to LWPU Corporation.  Any use, reproduction, or disclosure 
 * without the written permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */
// To facilitate faster compiles via pre-compiled headers, keep "stdtest.h" first
#include "mdiag/tests/stdtest.h"

#include "jtag2icd_lwenc.h"
#include "mdiag/lwgpu.h"
#include "mdiag/utils/lwgpu_channel.h"
#include "mdiag/tests/test_state_report.h"
#include "sim/IChip.h"
#include "gpu/include/gpudev.h"
#include "volta/gv100/dev_host.h"

#define CTRL_STATUS_BUSY        0     // used in jtag2icd_lwenc::wait_for_ctrl
#define MAX_HOST_CTRL_ATTEMPTS  800   // used in jtag2icd_lwenc::wait_for_ctrl

#define FALCON_ICD_CMD                                        0x200   //FIXME
#define FALCON_ICD_ADDR                                       0x204   //FIXME
#define FALCON_ICD_WDATA                                      0x208   //FIXME
#define FALCON_ICD_RDATA                                      0x20c   //FIXME

#define FALCON_MAILBOX0                                       0x40   //FIXME
#define FALCON_MAILBOX1                                       0x44   //FIXME
#define LW_CLWENC_CAP0                                        0x00000da8

#define FALCON_ICD_CMD_OPC_WCM                                0x0000000d
#define FALCON_ICD_CMD_OPC_RCM                                0x0000000c
#define FALCON_ICD_CMD_OPC_RSTAT                              0x0000000e
#define FALCON_ICD_CMD_OPC_RREG                               0x00000008
#define FALCON_ICD_CMD_OPC_WREG                               0x00000009
#define FALCON_ICD_CMD_OPC_STOP                               0x00000000

#define LW_CLWENC_FALCON_MAILBOX1                             0x00000044

#define JTAG_SEC_CMD_CLUSTER_ID                 0
#define JTAG_SEC_CMD_INSTR_ID                   9
#define JTAG_SEC_CMD_sec_length                 24
#define JTAG_SEC_CMD_sha_cal_start              16:16
#define JTAG_SEC_CMD_sha_cal_ready_status       17:17
#define JTAG_SEC_CMD_reserved                   23:18

#define JTAG_SEC_CFG_CLUSTER_ID                 0
#define JTAG_SEC_CFG_INSTR_ID                   8
#define JTAG_SEC_CFG_sec_length                 32
#define JTAG_SEC_CFG_en_scan_dump               16:16
#define JTAG_SEC_CFG_en_atpg_scan               17:17
#define JTAG_SEC_CFG_en_ram_dump                18:18
#define JTAG_SEC_CFG_en_mbist                   19:19
#define JTAG_SEC_CFG_en_iobist                  20:20
#define JTAG_SEC_CFG_en_jtag2host               21:21
#define JTAG_SEC_CFG_en_boundary_scan           22:22
#define JTAG_SEC_CFG_en_wbr                     23:23
#define JTAG_SEC_CFG_en_top_fs                  24:24
#define JTAG_SEC_CFG_reserved                   30:25
#define JTAG_SEC_CFG_global_enable              31:31

#define JTAG_SEC_CHK_CLUSTER_ID                 0
#define JTAG_SEC_CHK_INSTR_ID                   7
#define JTAG_SEC_CHK_sec_length                 272
#define JTAG_SEC_CHK_selwreid                   271:16

//#define JTAG_SEC_UNLOCK_STATUS_CLUSTER_ID                     0...15
#define JTAG_SEC_UNLOCK_STATUS_INSTR_ID         10
#define JTAG_SEC_UNLOCK_STATUS_sec_length       272

// define which tests are run
static int jtag2lwenc_basic      = 0;
static int jtag2lwenc_instance_1 = 0;
static int jtag2lwenc_instance_2 = 0;

extern const ParamDecl jtag2icd_lwenc_params[] =
{
    SIMPLE_PARAM("-jtag2lwenc_basic", "Testing jtag2lwenc_basic"),
    SIMPLE_PARAM("-jtag2lwenc_instance_1", "Testing lwenc1"),
    SIMPLE_PARAM("-jtag2lwenc_instance_2", "Testing lwenc2"),
    PARAM_SUBDECL(lwgpu_single_params),
    LAST_PARAM
};

// constructor
jtag2icd_lwenc::jtag2icd_lwenc(ArgReader *params) :
    LWGpuSingleChannelTest(params)
{
    if (params->ParamPresent("-jtag2lwenc_basic"))
        jtag2lwenc_basic = 1;
    if (params->ParamPresent("-jtag2lwenc_instance_1"))
        jtag2lwenc_instance_1 = 1;
    if (params->ParamPresent("-jtag2lwenc_instance_2"))
        jtag2lwenc_instance_2 = 1;
}

// destructor
jtag2icd_lwenc::~jtag2icd_lwenc(void)
{
}

// Factory
STD_TEST_FACTORY(jtag2icd_lwenc,jtag2icd_lwenc)

// setup method
int jtag2icd_lwenc::Setup(void)
{
    lwgpu = LWGpuResource::FindFirstResource();
    arch = lwgpu->GetArchitecture();
    InfoPrintf("jtag2icd_lwenc: Setup, arch is 0x%x\n", arch);
    ch = lwgpu->CreateChannel();
    if (!ch)
    {
        ErrPrintf("jtag2icd_lwenc::Setup failed to create channel\n");
        return 0;
    }
    getStateReport()->init("jtag2icd_lwenc");
    getStateReport()->enable();

    return 1;
}

//CleanUp
void jtag2icd_lwenc::CleanUp(void)
{
    if (ch)
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

void jtag2icd_lwenc::DelayNs(UINT32 Lwclks)
{
  // Lwrrently this function is in terms of .2nS / lwclk. This is incorrectly done
  // in RTL. This should be a close approx of 1 nanosecond.
  Platform::EscapeWrite("CLOCK_WAIT",IChip::ECLOCK_LWCLK,0,Lwclks * 5);
}

// run - main routine
void jtag2icd_lwenc::Run(void)
{
    if (jtag2lwenc_basic)
    {
        if (jtag2icd_lwencTest())
        {
        SetStatus(TEST_FAILED);
        ErrPrintf("jtag2icd_lwenc::jtag2icd_lwencTest failed\n");
        return;
        }

    InfoPrintf("jtag2icd_lwencTest() Done !\n");
    } //endif_jtag2lwenc_basic

    InfoPrintf("jtag2icd_lwenc test complete\n");
    SetStatus(TEST_SUCCEEDED);
    getStateReport()->crcCheckPassedGold();

    ch->WaitForIdle();
}//end_Run

bool jtag2icd_lwenc::host2JtagCfg
(   int instr,
    int length,
    int chiplet,
    int dw,
    int status,
    int req
)
{
    UINT32 h2jRegCtrl = 0;
    h2jRegCtrl |= REF_NUM(LW_PJTAG_ACCESS_CTRL_INSTR_ID, instr);
    h2jRegCtrl |= REF_NUM(LW_PJTAG_ACCESS_CTRL_REG_LENGTH, length);
    h2jRegCtrl |= REF_NUM(LW_PJTAG_ACCESS_CTRL_CLUSTER_SEL, chiplet);
    h2jRegCtrl |= REF_NUM(LW_PJTAG_ACCESS_CTRL_DWORD_EN, dw);
    h2jRegCtrl |= REF_NUM(LW_PJTAG_ACCESS_CTRL_CTRL_STATUS, status);
    h2jRegCtrl |= REF_NUM(LW_PJTAG_ACCESS_CTRL_REQ_CTRL, 0); //clean old req before each new req
    lwgpu->RegWr32(LW_PJTAG_ACCESS_CTRL, h2jRegCtrl);
    h2jRegCtrl |= REF_NUM(LW_PJTAG_ACCESS_CTRL_REQ_CTRL, req);
    lwgpu->RegWr32(LW_PJTAG_ACCESS_CTRL, h2jRegCtrl);

    Platform::DelayNS(10);
    bool timeOut = req ? true : false;
    UINT32 getStatus = 0;
    for (int i = 0; (getStatus == 0) && (req == 1) && (i < 50); i++)
    {
        getStatus = lwgpu->RegRd32(LW_PJTAG_ACCESS_CTRL);
        getStatus = REF_VAL(LW_PJTAG_ACCESS_CTRL_CTRL_STATUS, getStatus);
        if (getStatus == 1)
            timeOut = false;
    }
    if (timeOut)
    {
        ErrPrintf("%s: pull the field CTRL_STATUS of the reg LW_PJTAG_ACCESS_CTRL time out\n",__FUNCTION__);
        return false;
    }
    return true;
}

// copy from //hw/lwgpu/tools/mods/trace_3d/plugins/fc_misc/tests/source/h2j_api.cpp
bool jtag2icd_lwenc::wait_for_ctrl()
{
    UINT32 hostStatus = CTRL_STATUS_BUSY;
    UINT32 host_ctrl_attempts   = 0;
    InfoPrintf("Waiting for Host Control \n");
    while (hostStatus == CTRL_STATUS_BUSY)
    {
        hostStatus = lwgpu->RegRd32(LW_PJTAG_ACCESS_CTRL);
        hostStatus = REF_VAL(LW_PJTAG_ACCESS_CTRL_CTRL_STATUS, hostStatus);
        host_ctrl_attempts++;
        if (host_ctrl_attempts > MAX_HOST_CTRL_ATTEMPTS) {
            ErrPrintf("Exceeded MAX_HOST_CTRL_ATTEMPTS, attempts = %d \n", host_ctrl_attempts);
            return false;
        }
    }
    InfoPrintf("Acquired Host Control\n");
    return true;
}

// copy from //hw/lwgpu/tools/mods/trace_3d/plugins/fc_misc/tests/source/h2j_api.cpp
bool jtag2icd_lwenc::host2jtag_config_write( UINT32 dword_en, UINT32 reg_length, UINT32 read_or_write, UINT32 capt_dis, UINT32 updt_dis, UINT32 burst_mode)
{
    UINT32 host2jtagAccessConfig = 0;

    // Write Msb 8 bits of reg_length and dword_en to config register
    reg_length = (reg_length & 0x0007ffff)>>11 ;
    dword_en   = (dword_en & 0x0003ffff)>>6 ;

    host2jtagAccessConfig |=REF_NUM(LW_PJTAG_ACCESS_CONFIG_CAPT_DIS,capt_dis);
    host2jtagAccessConfig |=REF_NUM(LW_PJTAG_ACCESS_CONFIG_UPDT_DIS,updt_dis);
    host2jtagAccessConfig |=REF_NUM(LW_PJTAG_ACCESS_CONFIG_DWORD_EN,dword_en);
    host2jtagAccessConfig |=REF_NUM(LW_PJTAG_ACCESS_CONFIG_REG_LENGTH,reg_length);
    host2jtagAccessConfig |=REF_NUM(LW_PJTAG_ACCESS_CONFIG_LAST_CLUSTER_ILW,0x0);
    if(read_or_write==0)
    {
      host2jtagAccessConfig |=REF_NUM(LW_PJTAG_ACCESS_CONFIG_BURST,burst_mode);
    }

    lwgpu->RegWr32(LW_PJTAG_ACCESS_CONFIG, host2jtagAccessConfig);
    InfoPrintf("Writing Into LW_PJTAG_ACCESS_CONFIG : 0x%x\n", host2jtagAccessConfig);
    InfoPrintf("regLength : 0x%x, dword_en : 0x%x    \n", reg_length,dword_en);
    return true;

}

// copy from //hw/lwgpu/tools/mods/trace_3d/plugins/fc_misc/tests/source/h2j_api.cpp
bool jtag2icd_lwenc::host2jtag_ctrl_write(UINT32 req_ctrl, UINT32 status, UINT32 chiplet_sel, UINT32 dword_en, UINT32 reg_length, UINT32 instr_id, bool read_or_write)
{

    bool fail;
    UINT32 host2jtagRegCtrl = 0;

    host2jtagRegCtrl |= REF_NUM(LW_PJTAG_ACCESS_CTRL_INSTR_ID, instr_id);
    host2jtagRegCtrl |= REF_NUM(LW_PJTAG_ACCESS_CTRL_REG_LENGTH, reg_length);
    host2jtagRegCtrl |= REF_NUM(LW_PJTAG_ACCESS_CTRL_DWORD_EN, dword_en);
    host2jtagRegCtrl |= REF_NUM(LW_PJTAG_ACCESS_CTRL_CLUSTER_SEL, chiplet_sel);
    host2jtagRegCtrl |= REF_NUM(LW_PJTAG_ACCESS_CTRL_CTRL_STATUS, status);
    host2jtagRegCtrl |= REF_NUM(LW_PJTAG_ACCESS_CTRL_REQ_CTRL, req_ctrl);

    InfoPrintf("Writing Into LW_PJTAG_ACCESS_CTRL : 0x%x\n", host2jtagRegCtrl);
    InfoPrintf("rd_or_wr_intent : 0x%x, regLength : 0x%x, Chiplet : 0x%x,instr_id : 0x%x    \n", read_or_write,reg_length,chiplet_sel,instr_id);

    lwgpu->RegWr32(LW_PJTAG_ACCESS_CTRL, host2jtagRegCtrl);

    Utility::DelayNS(100);

    if(req_ctrl == 1)
    {
        fail = wait_for_ctrl();
        return fail;
    }
    return true;
}

// copy from //hw/lwgpu/tools/mods/trace_3d/plugins/fc_misc/tests/source/h2j_api.cpp
bool jtag2icd_lwenc::host2jtag_data_write(UINT32 data_value)
{
    InfoPrintf("Writing into LW_PJTAG_ACCESS_DATA : 0x%x\n", data_value);
    lwgpu->RegWr32(LW_PJTAG_ACCESS_DATA, data_value);

    return true;
}

// copy from //hw/lwgpu/tools/mods/trace_3d/plugins/fc_misc/tests/source/h2j_api.cpp
bool jtag2icd_lwenc::host2jtag_data_read(UINT32& data_value,UINT32 dword_en, UINT32 reg_length)
{
    UINT32 numDwords =0 ;
    InfoPrintf("Reading from LW_PJTAG_ACCESS_DATA\n ");
    data_value = lwgpu->RegRd32(LW_PJTAG_ACCESS_DATA);
    numDwords = reg_length/32 ;
    if (reg_length %32 !=0)
    {
    numDwords++;
    }
    if( dword_en == (numDwords-1) ) {
        if(((reg_length)%32) !=0) {
            data_value = ( data_value >> (32-((reg_length)%32)) ) ;
        }
    }
    InfoPrintf("lwrrentDword : 0x%x\tRead : 0x%x   \n ",dword_en, data_value);

    return true;
}

bool jtag2icd_lwenc::host2jtag_config_write_multiple( UINT32 dword_en, UINT32 reg_length, UINT32 read_or_write, UINT32 capt_dis, UINT32 updt_dis, UINT32 burst_mode)
{
    UINT32 host2jtagAccessConfig = 0;

    // Write Msb 8 bits of reg_length and dword_en to config register
    reg_length = (reg_length & 0x0007ffff)>>11 ;
    dword_en = (dword_en & 0x0003ffff)>>6 ;

    host2jtagAccessConfig |=REF_NUM(LW_PJTAG_ACCESS_CONFIG_CAPT_DIS,capt_dis);
    host2jtagAccessConfig |=REF_NUM(LW_PJTAG_ACCESS_CONFIG_UPDT_DIS,updt_dis);
    host2jtagAccessConfig |=REF_NUM(LW_PJTAG_ACCESS_CONFIG_DWORD_EN,dword_en);
    host2jtagAccessConfig |=REF_NUM(LW_PJTAG_ACCESS_CONFIG_REG_LENGTH,reg_length);
    host2jtagAccessConfig |=REF_NUM(LW_PJTAG_ACCESS_CONFIG_LAST_CLUSTER_ILW,0x1);
    if(read_or_write==0)
    {
        host2jtagAccessConfig |=REF_NUM(LW_PJTAG_ACCESS_CONFIG_BURST,burst_mode);
    }

    lwgpu->RegWr32(LW_PJTAG_ACCESS_CONFIG, host2jtagAccessConfig);
    InfoPrintf("Writing Into LW_PJTAG_ACCESS_CONFIG : 0x%x\n", host2jtagAccessConfig);
    InfoPrintf("regLength : 0x%x, dword_en : 0x%x \n", reg_length,dword_en);

    return true;
}

#if 1
bool jtag2icd_lwenc::host2jtag_unlock_jtag(int cluster_id, int instr_id, int length )
{
    UINT32 dword_en = 0;
    UINT32 req_ctrl = 1;
    UINT32 capt_dis = 0;
    UINT32 updt_dis = 0;
    UINT32 burst_mode = 0;
    UINT32 write_data;

    UINT32 read_or_write = 1; //write
    InfoPrintf("regLength : 0x%x, ClusterId : 0x%x, instr_id : 0x%x \n", length,cluster_id,instr_id);

    host2jtag_config_write(dword_en,length,read_or_write,capt_dis,updt_dis,burst_mode);
    host2jtag_ctrl_write(req_ctrl, status, cluster_id, dword_en, length, instr_id, read_or_write );

    write_data = 0xffffffff;
    host2jtag_data_write(write_data);
    wait_for_ctrl();

    return true;
}
#else
bool jtag2icd_lwenc::host2jtag_unlock_jtag()
{

    UINT32 dword_en = 0;
    UINT32 req_ctrl = 0;
    UINT32 capt_dis = 0;
    UINT32 updt_dis = 0;
    UINT32 burst_mode = 0;
    UINT32 write_data;
    UINT32 i;

    //Step 1: write all 1's to testmaster_JTAG_SEC_CFG_
    UINT32 cluster_id = 0;
    UINT32 id = 8; //CFG
    UINT32 length = 30;
    UINT32 read_or_write = 1; //write
    InfoPrintf("regLength : 0x%x, ClusterId : 0x%x, instr_id : 0x%x \n", length,cluster_id,id);

    host2jtag_config_write(dword_en,length,read_or_write,capt_dis,updt_dis,burst_mode);
    host2jtag_ctrl_write(req_ctrl, status, cluster_id, dword_en, length, id, read_or_write );

    write_data = 0xffffffff;
    host2jtag_data_write(write_data);
    // Clearing the previous request
    req_ctrl = 0;
    host2jtag_ctrl_write(req_ctrl, status, cluster_id, dword_en, length, id, read_or_write);

    //Step 2: Write write all 0's to JTAG_SEC_CHK
    cluster_id = 0;
    id = 7; //CHK
    length = 270;
    read_or_write = 1; //write

    InfoPrintf("regLength : 0x%x, ClusterId : 0x%x, instr_id : 0x%x \n", length,cluster_id,id);
    host2jtag_config_write(dword_en,length,read_or_write,capt_dis,updt_dis,burst_mode);
    host2jtag_ctrl_write(req_ctrl, status, cluster_id, dword_en, length, id, read_or_write );

    for( i=0; i < (length/32 + 1); i++)
    {
        write_data = 0x0;
        host2jtag_data_write(write_data);
    }
    // Clearing the previous request
    req_ctrl = 0;
    host2jtag_ctrl_write(req_ctrl, status, cluster_id, dword_en, length, id, read_or_write);

    //Step 3: Write 0x1 to JTAG_SEC_CMD_sha_cal_start to start SHA callwlation
    cluster_id = 0;
    id = 9; //CMD
    length = 22;
    read_or_write = 1; //write

    InfoPrintf("regLength : 0x%x, ClusterId : 0x%x, instr_id : 0x%x \n", length,cluster_id,id);
    host2jtag_config_write(dword_en,length,read_or_write,capt_dis,updt_dis,burst_mode);
    host2jtag_ctrl_write(req_ctrl, status, cluster_id, dword_en, length, id, read_or_write );

    write_data = 0x00010000;
    host2jtag_data_write(write_data);
    // Clearing the previous request
    req_ctrl = 0;
    host2jtag_ctrl_write(req_ctrl, status, cluster_id, dword_en, length, id, read_or_write);
    //< wait for at least 80 jtag cycles>
    wait_for_ctrl();

    //Step 4: Set JTAG instruction in all chiplets to JTAG_SEC_UNLOCK_STATUS. Trigger jtag shiftDR. Jtag scanin value is DON?T_CARE.
    id = 10; //UNLOCK_STATUS
    length = 238; //14 chiplets, 17 bits per chiplet
    read_or_write = 1; //write

    InfoPrintf("regLength : 0x%x, instr_id : 0x%x \n", length,id);
    for(cluster_id = 0 ; cluster_id <14; cluster_id ++){
        if(cluster_id !=13){
            host2jtag_config_write_multiple(dword_en,length,read_or_write,capt_dis,updt_dis,burst_mode);
        }else{
            host2jtag_config_write(dword_en,length,read_or_write,capt_dis,updt_dis,burst_mode);
        }
        host2jtag_ctrl_write(req_ctrl, status, cluster_id, dword_en, length, id, read_or_write );
    }

    for( i=0; i < (length/32 + 1); i++) {
        write_data = 0x0;
        host2jtag_data_write(write_data);
    }
    // Clearing the previous request
    req_ctrl = 0;
    host2jtag_ctrl_write(req_ctrl, status, cluster_id, dword_en, length, id, read_or_write);

    //Step 5: Set JTAG_SEC_CMD with value 0X0
    cluster_id = 0;
    id = 9; //CMD
    length = 22;
    read_or_write = 1; //write

    InfoPrintf("regLength : 0x%x, ClusterId : 0x%x, instr_id : 0x%x \n", length,cluster_id,id);
    host2jtag_config_write(dword_en,length,read_or_write,capt_dis,updt_dis,burst_mode);
    host2jtag_ctrl_write(req_ctrl, status, cluster_id, dword_en, length, id, read_or_write );

    write_data = 0x0;
    host2jtag_data_write(write_data);
    // Clearing the previous request
    req_ctrl = 0;
    host2jtag_ctrl_write(req_ctrl, status, cluster_id, dword_en, length, id, read_or_write);

    return true;

}
#endif

bool jtag2icd_lwenc::setJTAGPat
(
    int cluster_id,
    int instr_id,
    int length,
    UINT32 address,
    UINT32 data,
    UINT32 write,
    UINT32 start,
    UINT32 start_bit
)
{
    //UINT32 jtag_data[length/32 + 1]; // will cause build error so set the size to a bigger value
    UINT32 jtag_data[20];
    if (length/32 > 20 + 1)
    {
        ErrPrintf("The size of jtag_data is not enough, please make sure it is largger than length/32 +1 = %d \n",length/32+1);
        return false;
    }
    //callwlate checksum
    UINT32 tmpIcdPat3 = 0xf |        //Byte enable
                        write << 4 |
                        start << 5;

    UINT32 checksum = (address & 0x000000ff)^
                      ((address & 0x0000ff00) >> 8)^
                      ((address & 0x00ff0000) >> 16)^
                      ((address & 0xff000000) >> 24)^
                      (data & 0x000000ff)^
                      ((data & 0x0000ff00) >> 8)^
                      ((data & 0x00ff0000) >> 16)^
                      ((data & 0xff000000) >> 24)^
                      (tmpIcdPat3 & 0x000000ff);

    InfoPrintf("checksum = 0x%x, tmpIcdPat3 = 0x%x, data = 0x%x, addr = 0x%x\n", checksum, tmpIcdPat3, data, address);

    //gain the icd patten
     UINT32 icdPat1   = 0;
     UINT32 icdPat2   = 0;
     UINT32 icdPat3   = 0;
     UINT32 icdPat4   = 0;
     UINT32 shift_bit = 0;

     //status_bit address  data  bit_enable read_write start_trans appid check_sum
     //    2        30      32        4        1           1        2        8
     icdPat1 = address; //we don't care the value of status_bit
     icdPat2 = data;
     icdPat3 = 0xf        |    //Byte enable
               write << 4 |
               start << 5 |
               checksum << 8;
     icdPat4 = 0;

     InfoPrintf("%s: Before shift: instr 0x%x, length %d, data is 0x%x, 0x%x, 0x%x and 0x%x\n",
                 __FUNCTION__, instr_id, length, icdPat1, icdPat2, icdPat3, icdPat4);
     //update the patten according to start_bit
     shift_bit = start_bit%32;
     if (shift_bit <= 16)
     {
         icdPat4 = 0;
         icdPat3 = (icdPat3 << shift_bit) | (icdPat2 >> (32-shift_bit));
         icdPat2 = (icdPat2 << shift_bit) | (icdPat1 >> (32-shift_bit));
         icdPat1 = (icdPat1 << shift_bit) ;
     }
     else
     {
         icdPat4 = icdPat3 >> (32-shift_bit);
         icdPat3 = (icdPat3 << shift_bit) | (icdPat2 >> (32-shift_bit));
         icdPat2 = (icdPat2 << shift_bit) | (icdPat1 >> (32-shift_bit));
         icdPat1 = (icdPat1 << shift_bit) ;
     }
     InfoPrintf("%s: After shift: instr 0x%x, length %d, data is 0x%x, 0x%x, 0x%x and 0x%x\n",
                __FUNCTION__, instr_id, length, icdPat1, icdPat2,icdPat3,icdPat4);
    // fill in status_bits, address, data, byte enable, rw, start, App ID, checksum from bit 214 to 293
    memset(jtag_data,0,sizeof(jtag_data));

   for (int i=0; i < (length/32); i++)
   {
       jtag_data[i] = 0;
       if ((i -start_bit/32) == 0)
           jtag_data[i] = icdPat1;
       if ((i -start_bit/32) == 1)
           jtag_data[i] = icdPat2;
       if ((i -start_bit/32) == 2)
           jtag_data[i] = icdPat3;
       if (((i -start_bit/32) == 3) && (shift_bit > 16))
           jtag_data[i] = icdPat4;
       InfoPrintf("Info: jtag_data[%d] is 0x%x\n", i, jtag_data[i]);
   }
   if (length % 32)
   {   //icdPat3 or icdPat4 may here ,it's ok if we dont in end of the chain.
       if ((length/32 -start_bit/32) == 2)
           jtag_data[length/32] = icdPat3;
       if (((length/32 -start_bit/32) == 3) && (shift_bit > 16))
           jtag_data[length/32] = icdPat4;
       InfoPrintf("Info: jtag_data[%d] is 0x%x\n", length/32, jtag_data[length/32]);
   }

   //Call host2jtag api for data writting
   ///  //hw/lwgpu/tools/mods/trace_3d/plugins/fc_misc/tests/source/host2ieee1500.cpp
    UINT32 dword_en;// dword_en is 0 for writes and should be whatever dwords you want to access during reads
    UINT32 status;
    UINT32 read_or_write;// read_or_write should be set to 1 for write and 0 for read.
    UINT32 capt_dis;
    UINT32 updt_dis;
    UINT32 burst_mode;//burst_mode should be always be 1
    UINT32 req_ctrl; //req_ctrl should be 1 when you want to make a request and should be cleared at the end of each access

    dword_en      = 0;
    status        = 0;
    read_or_write = write;
    capt_dis      = 0x0;
    updt_dis      = 0x0;
    burst_mode    = 0x1;
    req_ctrl      = 0x1;

    InfoPrintf("regLength : 0x%x, ClusterId : 0x%x, instr_id : 0x%x    \n", length,cluster_id,instr_id);
    InfoPrintf("Writing %d JTAG register of length %d  Instr Id %d and cluster_id %d\n",instr_id,  length, instr_id, cluster_id);
    // Updating config and control register before writing into Jtag chains.
    host2jtag_config_write(dword_en,length,read_or_write,capt_dis,updt_dis,burst_mode);
    host2jtag_ctrl_write(req_ctrl, status, cluster_id, dword_en, length, instr_id, read_or_write );

    for( int i=0; i < (length/32); i++)
    {
        host2jtag_data_write(jtag_data[i]);
        InfoPrintf("write data is  %x \n", jtag_data[i]);
    }
    if (length % 32)
    {
        host2jtag_data_write(jtag_data[length/32]);
        InfoPrintf("write data is  %x \n", jtag_data[length/32]);
    }
    return true;
}

bool jtag2icd_lwenc::getJTAGPat
(
    int cluster_id,
    int instr_id,
    int length,
    UINT32* rdat,
    UINT32 start_bit
)
{
    UINT32 shift_bit   = 0;
    UINT32 icdPat1_readback = 0;
    UINT32 icdPat2_readback = 0;
    UINT32 icdPat3_readback = 0;
    UINT32 icdPat4_readback = 0;

   //Call host2jtag api for data writting
    UINT32 dword_en;// dword_en is 0 for writes and should be whatever dwords you want to access during reads
    UINT32 status;
    UINT32 read_or_write;// read_or_write should be set to 1 for write and 0 for read.
    UINT32 capt_dis;
    UINT32 updt_dis;
    UINT32 burst_mode;//burst_mode should be always be 1
    UINT32 req_ctrl; //req_ctrl should be 1 when you want to make a request and should be cleared at the end of each access
    UINT32 read_data;

    dword_en      = 0;
    status        = 0;
    read_or_write = 0x0;
    capt_dis      = 0x0;
    updt_dis      = 0x0;
    burst_mode    = 0x1;
    req_ctrl      = 0x1;

    req_ctrl      = 0;
    dword_en      = 0;
    read_or_write = 1;
    host2jtag_ctrl_write(req_ctrl, status, cluster_id, dword_en, length, instr_id, read_or_write);

    InfoPrintf("Reading register  of length %d instr id %d and cluster_id %d \n", length, instr_id, cluster_id);
    req_ctrl    = 1;
    read_or_write = 0;

   for (int i=0; i <= (length/32); i++)
   {

       host2jtag_config_write(i,length,read_or_write,capt_dis,updt_dis,burst_mode);
       host2jtag_ctrl_write(req_ctrl, status, cluster_id, i, length, instr_id, read_or_write);
       host2jtag_data_read(read_data,i,length);
       InfoPrintf("read data is  %x \n", read_data);

       if ((i -start_bit/32) == 0)
       {
           icdPat1_readback= read_data;
           InfoPrintf("current read back parten is icdPat1_readback 0x%x\n", icdPat1_readback);
       }
       if ((i -start_bit/32) == 1)
       {
           icdPat2_readback= read_data;
           InfoPrintf("current read back parten is icdPat2_readback 0x%x\n", icdPat2_readback);
       }
       if ((i - start_bit/32) == 2)
       {
           icdPat3_readback= read_data;
           InfoPrintf("current read back parten is icdPat3_readback 0x%x\n", icdPat3_readback);
       }
       if ((i - start_bit/32) == 3)
       {
           icdPat4_readback= read_data;
           InfoPrintf("current read back parten is icdPat3_readback 0x%x\n", icdPat4_readback);
       }
       }
       shift_bit = start_bit%32;
       icdPat1_readback = (icdPat1_readback >> shift_bit) | (icdPat2_readback << (32-shift_bit)) ;
       icdPat2_readback = (icdPat2_readback >> shift_bit) | (icdPat3_readback << (32-shift_bit));
       icdPat3_readback = (icdPat3_readback >> shift_bit) | (icdPat4_readback << (32-shift_bit));
       InfoPrintf("readback data is 0x%x\n", icdPat2_readback);
       *rdat = icdPat2_readback;
       return true;
}

int jtag2icd_lwenc::jtag2icd_lwencTest()
{
    InfoPrintf("jtag2icd_lwenc test starts after kimiy0 fix...\n");
    int cluster_id = 0;
    int instr = 0;
    UINT32 start_bit = 0;
    int unlock_instr = 0;
    int unlock_length = 0;
    // chain length is 295 bit
    int length = 0;
    if (arch == Gpu::GM107)   // gm107  ,Defined in //sw/dev/gpu_drv/chips_a/diag/mods/gpu/include/gpulist.h
        instr = 0xd2;   // b11010001. Found in TOT/package/gm107/jtagreg/k_s0/JTAG2LWENC_FALCON.jrd
    else if (arch == Gpu::GM204)
         { //gm204
             if (jtag2lwenc_instance_1)
             { // LWENC1
                 instr = 0xbe;   //b10111110, Found in //hw/lwgpu/package/jtagreg/chip/gm204/gmb_s0/JTAG2LWENC1_FALCON.jrd
                 InfoPrintf("LWENC1 is selected in gm204 \n");
             }
             else
             { //  LWENC0
                 instr = 0xbd;   //b10111101, Found in //hw/lwgpu/package/jtagreg/chip/gm204/gmb_s0/JTAG2LWENC0_FALCON.jrd
                 InfoPrintf("LWENC0 is selected in gm204 \n");
             }
         }
    else if (arch == Gpu::GM200)
         { //gm200
             if (jtag2lwenc_instance_1)
             { // LWENC1
                 instr = 0xbe;   //b10111110, Found in //hw/lwgpu_gm200/package/jtagreg/chip/gm200/gmb_s0/JTAG2LWENC1_FALCON.jrd
                 InfoPrintf("LWENC1 is selected in gm200\n");
             }
             else
             { //  LWENC0
                 instr = 0xbd;   //b10111101, Found in //hw/lwgpu_gm200/package/jtagreg/chip/gm200/gmb_s0/JTAG2LWENC0_FALCON.jrd
                 InfoPrintf("LWENC0 is selected in gm200\n");
             }
         }
    else if (arch == Gpu::GM206)
         { //gm206
             if (jtag2lwenc_instance_1)
             { // LWENC1
                 instr = 0xbe;   //b10111110, Found in //hw/lwgpu_gm206/package/jtagreg/chip/gm206/gmb_s0/JTAG2LWENC1_FALCON.jrd
                 InfoPrintf("LWENC1 is selected in gm206\n");
             }
             else
             { //  LWENC0
                 instr = 0xbd;   //b10111101, Found in //hw/lwgpu_gm206/package/jtagreg/chip/gm206/gmb_s0/JTAG2LWENC0_FALCON.jrd
                 InfoPrintf("LWENC0 is selected in gm206\n");
             }
         }
    else if (arch == Gpu::GP100)  //gp100
         {
            if (jtag2lwenc_instance_1) // LWENC1
            {
                instr     = 0x9e;
                start_bit = 101;
                length    = 295;
                InfoPrintf("LWENC1 is selected in gp100\n");
                InfoPrintf("arch = 0x%x, instr = 0x%x, length = %d start_bit = %d\n", arch, instr,length, start_bit);
            }
            else if (jtag2lwenc_instance_2)  // LWENC2
            {
                instr = 0x9e;
                start_bit = 214;
                length = 295;
                InfoPrintf("LWENC2 is selected in gp100\n");
                InfoPrintf("arch = 0x%x, instr = 0x%x, length = %d start_bit = %d\n", arch, instr,length, start_bit);
            }
            else //LWENC0
            {
                instr = 0x9e;
                start_bit = 16;
                length = 295;
                InfoPrintf("LWENC0 is selected in gp100\n");
                InfoPrintf("arch = 0x%x, instr = 0x%x, length = %d start_bit = %d\n", arch, instr,length, start_bit);
            }
         }
    else if (arch == Gpu::GP102) //gp102, value get from //sw/dev/gpu_drv/chips_a/diag/mods/gpu/include/gpulist.h
         {
            if (jtag2lwenc_instance_1) // LWENC1
            {
                cluster_id = 2;
                instr     = 0x9e;//it can be found at $tot/vmod/ieee1500/<chip>/jtagreg/instruction_map.yaml
                start_bit = 18;
                length    = 101;
                InfoPrintf("LWENC1 is selected in gp102\n");
                InfoPrintf("arch = 0x%x, instr = 0x%x, length = %d start_bit = %d\n", arch, instr,length, start_bit);
                //it is assign as m0_0_JTAG_SEC_UNLOCK_STATUS from Host2jtag_info.report
                unlock_instr = 254;
                unlock_length = 34;
            }
            else  // LWENC0
            {
                cluster_id = 3;
                instr = 0x9e;
                start_bit = 19;
                length = 105;
                InfoPrintf("LWENC0 is selected in gp102\n");
                InfoPrintf("arch = 0x%x, instr = 0x%x, length = %d start_bit = %d\n", arch, instr,length, start_bit);
                //it is assign as n0_0_JTAG_SEC_UNLOCK_STATUS from Host2jtag_info.report
                unlock_instr = 254;
                unlock_length = 34;
            }
         }
    else if (arch == Gpu::GP104) //gp104
         {
            InfoPrintf("jtag2icd_lwenc: Setup3, arch is 0x%x\n", arch);
            if (jtag2lwenc_instance_1) // LWENC1
            {
                cluster_id = 2;
                instr     = 0x9e;
                start_bit = 14;
                length    = 97;
                InfoPrintf("LWENC1 is selected in gp104\n");
                InfoPrintf("arch = 0x%x, instr = 0x%x, length = %d start_bit = %d\n", arch, instr,length, start_bit);
                unlock_instr = 254;
                unlock_length = 30;
            }
            else  // LWENC0
            {
                cluster_id = 3;
                instr = 0x9e;
                start_bit = 13;
                length = 101;
                InfoPrintf("LWENC0 is selected in gp104\n");
                InfoPrintf("arch = 0x%x, instr = 0x%x, length = %d start_bit = %d\n", arch, instr,length, start_bit);
                unlock_instr = 254;
                unlock_length = 30;
            }
         }
    else if (arch == Gpu::GP106) //gp106
         {
                cluster_id = 2;
                instr = 0x9e;
                start_bit = 10;
                length = 97;
                InfoPrintf("LWENC0 is selected in gp106\n");
                InfoPrintf("arch = 0x%x, instr = 0x%x, length = %d start_bit = %d\n", arch, instr,length, start_bit);
                unlock_instr = 254;
                unlock_length = 26;
         }
    else if (arch == Gpu::GP107) //gp107
         {
                cluster_id = 1;
                instr = 0x9e;
                start_bit = 8;
                length = 110;
                InfoPrintf("LWENC0 is selected in gp107\n");
                InfoPrintf("arch = 0x%x, instr = 0x%x, length = %d start_bit = %d\n", arch, instr,length, start_bit);
                unlock_instr = 254;
                unlock_length = 23;
         }
    else if (arch == Gpu::GV100) //gv100
         {
            InfoPrintf("jtag2icd_lwenc: Setup3, arch is 0x%x\n", arch);
            if (jtag2lwenc_instance_2) // LWENC2
            {
                cluster_id = 10;
                instr     = 0x9e;
                start_bit = 24;
                length    = 114;
                InfoPrintf("LWENC2 is selected in gv100\n");
                InfoPrintf("arch = 0x%x, instr = 0x%x, length = %d start_bit = %d\n", arch, instr,length, start_bit);
                unlock_instr = 254;
                unlock_length = 33;
            }
            else if (jtag2lwenc_instance_1) // LWENC1, lwenc0 and lwenc1 share the same jtag chain, so the cluster_id is the same
            {
                cluster_id = 16;
                instr     = 0x9e;
                start_bit = 101;
                length    = 197;
                InfoPrintf("LWENC1 is selected in gv100\n");
                InfoPrintf("arch = 0x%x, instr = 0x%x, length = %d start_bit = %d\n", arch, instr,length, start_bit);
                unlock_instr = 254;
                unlock_length = 33;
            }
            else  // LWENC0
            {
                cluster_id = 16;
                instr = 0x9e;
                start_bit = 9;
                length = 197;
                InfoPrintf("LWENC0 is selected in gv100\n");
                InfoPrintf("arch = 0x%x, instr = 0x%x, length = %d start_bit = %d\n", arch, instr,length, start_bit);
                unlock_instr = 254;
                unlock_length = 33;
            }
         }

    else
    {
        ErrPrintf("The project name is not defined \n");
        return 1;
    }

    InfoPrintf("%s: unlock jtag security status \n",__FUNCTION__);
    host2jtag_unlock_jtag(cluster_id, unlock_instr, unlock_length);

    InfoPrintf("%s: arch = 0x%x, instr = 0x%x\n",__FUNCTION__, arch, instr);

    UINT32 address, data, write;
    UINT32 Rdat;

    //===================================================================
    //step 1 -- JTAG write and read FALCON_MAILBOX0 directly
    //===================================================================
    InfoPrintf("step 1 -- JTAG write and read FALCON_MAILBOX0 directly\n");

    //-------------------------------------------------
    //step 1.1 -- JTAG write FALCON_MAILBOX0 directly
    //-------------------------------------------------
    InfoPrintf("step 1.1 -- JTAG write FALCON_MAILBOX0 directly\n");

    //JTAG write LW_CLWENC_FALCON_MAILBOX0
    InfoPrintf("JTAG write FALCON_MAILBOX0\n");
    address = FALCON_MAILBOX0;
    data    = 0xbeefbeef;
    write   = 1;
    setJTAGPat(cluster_id, instr, length, address, data, write, 1, start_bit);

    //-------------------------------------------------
    //step 1.2 -- JTAG read FALCON_MAILBOX0 directly
    //-------------------------------------------------
    InfoPrintf("step 1.2 -- JTAG read FALCON_MAILBOX0 directly\n");

    //JTAG read LW_CLWENC _FALCON_MAILBOX0
    InfoPrintf("JTAG read FALCON_MAILBOX0\n");
    address = FALCON_MAILBOX0;
    data    = 0;
    write   = 0;
    setJTAGPat(cluster_id, instr, length, address, data, write, 1, start_bit);

    //JTAG get the read-back value
    getJTAGPat(cluster_id, instr, length, &Rdat, start_bit);
    if (Rdat != 0xbeefbeef)
    {
        ErrPrintf("JTAG read back fail, Rdat = 0x%x\n", Rdat);
        return 1;
    }
    else
    {
        InfoPrintf("JTAG read back pass, Rdat = 0x%x\n", Rdat);
    }

    //-------------------------------------------------
    //step 1.3 -- JTAG read LW_CLWENC_CAP0 directly
    //-------------------------------------------------
    InfoPrintf("step 1.3 -- JTAG read LW_CLWENC_CAP0 directly\n");

    //JTAG read LW_CLWENC_CAP0
    InfoPrintf("JTAG read LW_CLWENC_CAP0\n");
    address = LW_CLWENC_CAP0;
    data    = 0;
    write   = 0;
    setJTAGPat(cluster_id, instr, length, address, data, write, 1, start_bit);

    //JTAG get the read-back value
    getJTAGPat(cluster_id, instr, length, &Rdat, start_bit);
    if ((Rdat & 0x00000002) != 0x00000000)
    {
        ErrPrintf("JTAG read back fail, Rdat = 0x%x\n", Rdat);
        return 1;
    }
    else
    {
        InfoPrintf("JTAG read back pass, Rdat = 0x%x\n", Rdat);
    }

    //===================================================================
    //step 2 -- JTAG write and read FALCON_MAILBOX1 through ICD
    //===================================================================
    InfoPrintf("step 2 -- JTAG write and read FALCON_MAILBOX1 through ICD\n");

    //-------------------------------------------------
    //step 2.0 -- JTAG write STOP through ICD
    //-------------------------------------------------
    InfoPrintf("step 2.0 -- JTAG write STOP through ICD\n");
    InfoPrintf("write FALCON_ICD_CMD to STOP \n");
    address = FALCON_ICD_CMD;
    data    = FALCON_ICD_CMD_OPC_STOP |
              0x2 << 6 ;   //size
    write   = 1;
    setJTAGPat(cluster_id, instr, length, address, data, write, 1, start_bit);

    //-------------------------------------------------
    //step 2.1 -- JTAG write FALCON_MAILBOX1 through ICD
    //-------------------------------------------------
    InfoPrintf("step 2.1 -- JTAG write LW_CLWENC_FALCON_MAILBOX1 through ICD\n");

    //write FALCON_ICD_ADDR to LW_CLWENC_FALCON_MAILBOX1
    InfoPrintf("write FALCON_ICD_ADDR to LW_CLWENC_FALCON_MAILBOX1\n");
    address = FALCON_ICD_ADDR;
    data    = LW_CLWENC_FALCON_MAILBOX1;
    write   = 1;
    setJTAGPat(cluster_id, instr, length, address, data, write, 1, start_bit);

    //write FALCON_ICD_WDATA to 0xbadbeef
    InfoPrintf("write FALCON_ICD_WDATA to 0xbadbeef\n");
    address = FALCON_ICD_WDATA;
    data    = 0xbadbeef;
    write   = 1;
    setJTAGPat(cluster_id, instr, length, address, data, write, 1, start_bit);

    //write FALCON_ICD_CMD to OPC_WCM
    InfoPrintf("write FALCON_ICD_CMD to OPC_WCM\n");
    address = FALCON_ICD_CMD;
    data    = FALCON_ICD_CMD_OPC_WCM |
              0x2 << 6 ;   //size
    write   = 1;
    setJTAGPat(cluster_id, instr, length, address, data, write, 1, start_bit);

//    //-------------------------------------------------
//    //step 2.2.0 -- PRIV read LW_CLWENC_FALCON_MAILBOX1
//    // In order to make sure ICD has successfully write
//    // targeted data to LW_CLWENC_FALCON_MAILBOX1.
//    //-------------------------------------------------
//    InfoPrintf("step 2.2.0 -- JTAG read LW_CLWENC_FALCON_MAILBOX1 \n");
//    lwgpu->GetRegFldNum("LW_PPWR_FALCON_MAILBOX1","",&data);
//    InfoPrintf("LW_PPWR_FALCON_MAILBOX1 Rdata= %x\n", data);
//    if (data != 0xbadbeef) {
//        ErrPrintf("ICD write data fail, expected 0xbadbeef, actully is 0x%x\n", data);
//        return 1;
//    }else{
//        InfoPrintf("ICD write MAILBOX1 successful\n");
//    }

    //-------------------------------------------------
    //step 2.2 -- JTAG read LW_CLWENC_FALCON_MAILBOX1 through ICD
    //-------------------------------------------------
    InfoPrintf("step 2.2 -- JTAG read LW_CLWENC_FALCON_MAILBOX1 through ICD\n");

    //write FALCON_ICD_ADDR to LW_CLWENC_FALCON_MAILBOX1
    InfoPrintf("write FALCON_ICD_ADDR to LW_CLWENC_FALCON_MAILBOX1\n");
    address = FALCON_ICD_ADDR;
    data    = LW_CLWENC_FALCON_MAILBOX1;
    setJTAGPat(cluster_id, instr, length, address, data, 1, 1, start_bit);

    //write FALCON_ICD_CMD to OPC_RCM
    InfoPrintf("write FALCON_ICD_CMD to OPC_RCM\n");
    address = FALCON_ICD_CMD;
    data    = FALCON_ICD_CMD_OPC_RCM |
              0x2 << 6;
    setJTAGPat(cluster_id, instr, length, address, data, 1, 1, start_bit);

    //-------------------------------------------------
    //step 2.3 -- JTAG polls ICD_CMD_RDVLD
    //-------------------------------------------------
    InfoPrintf("step 2.3 -- JTAG polls ICD_CMD_RDVLD\n");
    int rdvld = 0;
    int i     = 0;
    address   = FALCON_ICD_CMD;
    data      = 0;
    write     = 0;

    for (i = 0; i < 10; i++)
    {
        //JTAG polls  ICD_CMD_RDVLD
        InfoPrintf("JTAG polls ICD_CMD_RDVLD, times = 0x%x\n", i);
        setJTAGPat(cluster_id, instr, length, address, data, write, 1, start_bit);

        //JTAG get the read-back value
        getJTAGPat(cluster_id, instr, length, &Rdat, start_bit);
        InfoPrintf("JTAG read back, Rdat = 0x%x\n", Rdat);

        rdvld = Rdat & 0x8000;
        if (rdvld == 0x8000)
        {
            InfoPrintf("ICD_CMD_RDVLD is valid, break the loop\n");
            break;
        }
    }

    if (i == 10)
    {
        ErrPrintf("JTAG polls ICD_CMD_RDVLD timeout\n");
        return 1;
    }

    //-------------------------------------------------
    //step 2.4 -- JTAG read FALCON_ICD_RDATA
    //-------------------------------------------------
    InfoPrintf("step 2.4 -- JTAG read FALCON_ICD_RDATA\n");

    //JTAG read FALCON_ICD_RDATA
    InfoPrintf("JTAG read FALCON_ICD_RDATA\n");
    address = FALCON_ICD_RDATA;
    data    = 0;
    write   = 0;
    setJTAGPat(cluster_id, instr, length, address, data, write, 1, start_bit);

    //JTAG get the read-back value
    getJTAGPat(cluster_id, instr, length, &Rdat, start_bit);
    if (Rdat != 0xbadbeef)
    {
        ErrPrintf("JTAG read back fail, Rdat = 0x%x\n", Rdat);
        return 1;
     }
    else
    {
        InfoPrintf("JTAG read back pass, Rdat = 0x%x\n", Rdat);
    }

    return 0;
}
