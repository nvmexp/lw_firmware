/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2008-2012,2014-2016,2018, 2020-2021 by LWPU Corporation.  All 
 * rights reserved.  All information contained herein is proprietary and 
 * confidential to LWPU Corporation.  Any use, reproduction, or disclosure 
 * without the written permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */
// To facilitate faster compiles via pre-compiled headers, keep "stdtest.h" first
#include "mdiag/tests/stdtest.h"

#include "jtag2icd_lwdec.h"
#include "mdiag/lwgpu.h"
#include "mdiag/utils/lwgpu_channel.h"
#include "mdiag/tests/test_state_report.h"
#include "sim/IChip.h"
#include "volta/gv100/dev_host.h"

#define CTRL_STATUS_BUSY        0     // used in jtag2icd_lwdec::WaitForCtrl
#define MAX_HOST_CTRL_ATTEMPTS  15    // used in jtag2icd_lwdec::WaitForCtrl

#define FALCON_ICD_CMD                                        0x200   //FIXME
#define FALCON_ICD_ADDR                                       0x204   //FIXME
#define FALCON_ICD_WDATA                                      0x208   //FIXME
#define FALCON_ICD_RDATA                                      0x20c   //FIXME

#define FALCON_MAILBOX0                                       0x40   //FIXME
#define FALCON_MAILBOX1                                       0x44   //FIXME
#define LW_CLWDEC_CAP0                                        0x00000da8

#define FALCON_ICD_CMD_OPC_WCM                                0x0000000d
#define FALCON_ICD_CMD_OPC_RCM                                0x0000000c
#define FALCON_ICD_CMD_OPC_RSTAT                              0x0000000e
#define FALCON_ICD_CMD_OPC_RREG                               0x00000008
#define FALCON_ICD_CMD_OPC_WREG                               0x00000009
#define FALCON_ICD_CMD_OPC_STOP                               0x00000000

#define LW_CLWDEC_FALCON_MAILBOX1                             0x00001100
#define LW_PLWDEC_FALCON_MAILBOX1                             0x00084044

// define which tests are run
static int jtag2lwdecBasic = 0;

extern const ParamDecl jtag2icd_lwdec_params[] =
{
    SIMPLE_PARAM("-jtag2lwdec_basic", "Testing jtag2lwdec_basic"),
    PARAM_SUBDECL(lwgpu_single_params),
    LAST_PARAM
};

// constructor
jtag2icd_lwdec::jtag2icd_lwdec(ArgReader *params) :
    LWGpuSingleChannelTest(params)
{
    if (params->ParamPresent("-jtag2lwdec_basic"))
        jtag2lwdecBasic = 1;
}

// destructor
jtag2icd_lwdec::~jtag2icd_lwdec(void)
{
}

// Factory
STD_TEST_FACTORY(jtag2icd_lwdec,jtag2icd_lwdec)

// setup method
int jtag2icd_lwdec::Setup(void)
{
    lwgpu = LWGpuResource::FindFirstResource();
    arch = lwgpu->GetArchitecture();
    InfoPrintf("jtag2icd_lwdec: Setup, arch is 0x%x\n", arch);
    ch = lwgpu->CreateChannel();
    if (!ch)
    {
        ErrPrintf("jtag2icd_lwdec::Setup failed to create channel\n");
        return 0;
    }
    getStateReport()->init("jtag2icd_lwdec");
    getStateReport()->enable();

    return 1;
}

// CleanUp
void jtag2icd_lwdec::CleanUp(void)
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

void jtag2icd_lwdec::DelayNs(UINT32 Lwclks)
{
    // Lwrrently this function is in terms of .2nS / lwclk. This is incorrectly done
    // in RTL. This should be a close approx of 1 nanosecond.
    Platform::EscapeWrite("CLOCK_WAIT",IChip::ECLOCK_LWCLK,0,Lwclks * 5);
}

// run - main routine
void jtag2icd_lwdec::Run(void)
{
    if (jtag2lwdecBasic)
    {
        if (Jtag2icdLwdecTest())
        {
            SetStatus(TEST_FAILED);
            ErrPrintf("jtag2icd_lwdec::Jtag2icdLwdecTest failed\n");
            return;
        }

        InfoPrintf("Jtag2icdLwdecTest() Done !\n");
    } // endif_jtag2lwdecBasic

    InfoPrintf("jtag2icd_lwdec test complete\n");
    SetStatus(TEST_SUCCEEDED);
    getStateReport()->crcCheckPassedGold();

    ch->WaitForIdle();
}// end_Run

bool jtag2icd_lwdec::Host2JtagCfg
(
    int instr,
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
    h2jRegCtrl |= REF_NUM(LW_PJTAG_ACCESS_CTRL_REQ_CTRL, 0); // clean old req before each new req
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
bool jtag2icd_lwdec::WaitForCtrl()
{
    UINT32 hostStatus = CTRL_STATUS_BUSY;
    InfoPrintf("Waiting for Host Control \n");
    for (UINT32 retries = 0; retries < MAX_HOST_CTRL_ATTEMPTS; retries++)
    {
        hostStatus = REF_VAL(LW_PJTAG_ACCESS_CTRL_CTRL_STATUS, lwgpu->RegRd32(LW_PJTAG_ACCESS_CTRL));
        if (hostStatus != CTRL_STATUS_BUSY)
        {
            return true;
        }
        Platform::DelayNS(100); // delay some amount of time between reads to give hardware a
                                // chance to finish.
    }
    ErrPrintf("Exceeded MAX_HOST_CTRL_ATTEMPTS, attempts = %d \n", MAX_HOST_CTRL_ATTEMPTS);
    return(false);
}

// copy from //hw/lwgpu/tools/mods/trace_3d/plugins/fc_misc/tests/source/h2j_api.cpp
// lastClusterIlw should be true if one except the last of multiple clusters is being written
bool jtag2icd_lwdec::Host2jtagConfigWrite( UINT32 dwordEn, UINT32 regLength, UINT32 readOrWrite, UINT32 captDis, UINT32 updtDis, UINT32 burstMode, bool lastClusterIlw)
{
    UINT32 host2jtagAccessConfig = 0;

    // Write Msb 8 bits of regLength and dwordEn to config register
    regLength = (regLength & 0x0007ffff) >> 11;
    dwordEn   = (dwordEn & 0x0003ffff) >> 6;

    // clear previous operation by writing all zeros to the PJTAG_ACCESS_CONFIG register
    lwgpu->RegWr32(LW_PJTAG_ACCESS_CONFIG, 0);

    host2jtagAccessConfig |= REF_NUM(LW_PJTAG_ACCESS_CONFIG_CAPT_DIS, captDis);
    host2jtagAccessConfig |= REF_NUM(LW_PJTAG_ACCESS_CONFIG_UPDT_DIS, updtDis);
    host2jtagAccessConfig |= REF_NUM(LW_PJTAG_ACCESS_CONFIG_DWORD_EN, dwordEn);
    host2jtagAccessConfig |= REF_NUM(LW_PJTAG_ACCESS_CONFIG_REG_LENGTH, regLength);
    host2jtagAccessConfig |= REF_NUM(LW_PJTAG_ACCESS_CONFIG_LAST_CLUSTER_ILW, lastClusterIlw);
    if (readOrWrite == 0)
    {
        host2jtagAccessConfig |= REF_NUM(LW_PJTAG_ACCESS_CONFIG_BURST, burstMode);
    }

    lwgpu->RegWr32(LW_PJTAG_ACCESS_CONFIG, host2jtagAccessConfig);
    InfoPrintf("Writing Into LW_PJTAG_ACCESS_CONFIG : 0x%x\n", host2jtagAccessConfig);
    InfoPrintf("regLength : 0x%x, dwordEn : 0x%x    \n", regLength, dwordEn);
    return true;
}

// copy from //hw/lwgpu/tools/mods/trace_3d/plugins/fc_misc/tests/source/h2j_api.cpp
bool jtag2icd_lwdec::Host2jtagCtrlWrite(UINT32 reqCtrl, UINT32 status, UINT32 chipletSel, UINT32 dwordEn, UINT32 regLength, UINT32 instrID, bool readOrWrite)
{

    bool fail;
    UINT32 host2jtagRegCtrl = 0;

    host2jtagRegCtrl |= REF_NUM(LW_PJTAG_ACCESS_CTRL_INSTR_ID, instrID);
    host2jtagRegCtrl |= REF_NUM(LW_PJTAG_ACCESS_CTRL_REG_LENGTH, regLength);
    host2jtagRegCtrl |= REF_NUM(LW_PJTAG_ACCESS_CTRL_DWORD_EN, dwordEn);
    host2jtagRegCtrl |= REF_NUM(LW_PJTAG_ACCESS_CTRL_CLUSTER_SEL, chipletSel);
    host2jtagRegCtrl |= REF_NUM(LW_PJTAG_ACCESS_CTRL_CTRL_STATUS, status);
    host2jtagRegCtrl |= REF_NUM(LW_PJTAG_ACCESS_CTRL_REQ_CTRL, reqCtrl);

    InfoPrintf("Writing Into LW_PJTAG_ACCESS_CTRL : 0x%x\n", host2jtagRegCtrl);
    InfoPrintf("rd_or_wr_intent : 0x%x, regLength : 0x%x, Chiplet : 0x%x,instrID : 0x%x    \n", readOrWrite, regLength, chipletSel, instrID);

    lwgpu->RegWr32(LW_PJTAG_ACCESS_CTRL, host2jtagRegCtrl);

    Utility::DelayNS(100);

    if(reqCtrl == 1)
    {
        fail = WaitForCtrl();
        return fail;
    }
    return true;
}

// copy from //hw/lwgpu/tools/mods/trace_3d/plugins/fc_misc/tests/source/h2j_api.cpp
bool jtag2icd_lwdec::Host2jtagDataWrite(UINT32 dataValue)
{
    InfoPrintf("Writing into LW_PJTAG_ACCESS_DATA : 0x%x\n", dataValue);
    lwgpu->RegWr32(LW_PJTAG_ACCESS_DATA, dataValue);

    return true;
}

// copy from //hw/lwgpu/tools/mods/trace_3d/plugins/fc_misc/tests/source/h2j_api.cpp
bool jtag2icd_lwdec::Host2jtagDataRead(UINT32& dataValue, UINT32 dwordEn, UINT32 regLength)
{
    UINT32 numDwords = 0;
    InfoPrintf("Reading from LW_PJTAG_ACCESS_DATA\n ");
    dataValue = lwgpu->RegRd32(LW_PJTAG_ACCESS_DATA);
    numDwords = (regLength + 31) / 32;
    if (dwordEn == (numDwords - 1))
    {
        if ((regLength % 32) != 0)
        {
            dataValue = (dataValue >> (32 - (regLength % 32)));
        }
    }
    InfoPrintf("lwrrentDword : 0x%x\tRead : 0x%x   \n ", dwordEn, dataValue);

    return true;
}

bool jtag2icd_lwdec::Host2jtagUnlockJtag(int clusterID, int instrID, int length)
{
    const UINT32 dwordEn = 0;
    const UINT32 reqCtrl = 1;
    const UINT32 captDis = 0;
    const UINT32 updtDis = 0;
    const UINT32 burstMode = 0;
    UINT32 writeData;

    UINT32 readOrWrite = 1; // write
    InfoPrintf("regLength : 0x%x, ClusterId : 0x%x, instrID : 0x%x \n", length, clusterID, instrID);

    Host2jtagConfigWrite(dwordEn, length, readOrWrite, captDis, updtDis, burstMode, false);
    Host2jtagCtrlWrite(reqCtrl, status, clusterID, dwordEn, length, instrID, readOrWrite);

    writeData = 0xffffffff;
    Host2jtagDataWrite(writeData);
    WaitForCtrl();

    return true;
}

bool jtag2icd_lwdec::SetJTAGPat(int clusterID, int instrID, int length, UINT32 address, UINT32 data, UINT32 write, UINT32 start, UINT32 startBit)
{
    vector<UINT32> jtagData((length + 31) / 32, 0);

    // callwlate checksum
    UINT32 tmpIcdPat3 = 0xf        |  // Byte enable
                        write << 4 |
                        start << 5;

    UINT32 checksum = (address & 0x000000ff) ^
                      ((address & 0x0000ff00) >> 8) ^
                      ((address & 0x00ff0000) >> 16) ^
                      ((address & 0xff000000) >> 24) ^
                      (data & 0x000000ff) ^
                      ((data & 0x0000ff00) >> 8) ^
                      ((data & 0x00ff0000) >> 16) ^
                      ((data & 0xff000000) >> 24) ^
                      (tmpIcdPat3 & 0x000000ff);

    InfoPrintf("checksum = 0x%x, tmpIcdPat3 = 0x%x, data = 0x%x, addr = 0x%x\n", checksum, tmpIcdPat3, data, address);

    // gain the icd patten
    UINT32 icdPat1   = 0;
    UINT32 icdPat2   = 0;
    UINT32 icdPat3   = 0;
    UINT32 icdPat4   = 0;
    UINT32 shiftBit = 0;

    // status_bit address  data  bit_enable read_write start_trans appid check_sum
    //    2        30      32        4        1           1        2        8
    icdPat1 = address; // we don't care the value of status_bit
    icdPat2 = data;
    icdPat3 = 0xf        |    // Byte enable
              write << 4 |
              start << 5 |
              checksum << 8;
    icdPat4 = 0;

    InfoPrintf("%s: Before shift: instr 0x%x, length %d, data is 0x%x, 0x%x, 0x%x and 0x%x\n",
                __FUNCTION__, instrID, length, icdPat1, icdPat2, icdPat3, icdPat4);
    // update the patten according to startBit
    shiftBit = startBit % 32;
    if (shiftBit <= 16)
    {
        icdPat4 = 0;
        icdPat3 = (icdPat3 << shiftBit) | (icdPat2 >> (32 - shiftBit));
        icdPat2 = (icdPat2 << shiftBit) | (icdPat1 >> (32 - shiftBit));
        icdPat1 = (icdPat1 << shiftBit);
    }
    else
    {
        icdPat4 = icdPat3 >> (32 - shiftBit);
        icdPat3 = (icdPat3 << shiftBit) | (icdPat2 >> (32 - shiftBit));
        icdPat2 = (icdPat2 << shiftBit) | (icdPat1 >> (32 - shiftBit));
        icdPat1 = (icdPat1 << shiftBit) ;
    }
    InfoPrintf("%s: After shift: instr 0x%x, length %d, data is 0x%x, 0x%x, 0x%x and 0x%x\n",
                __FUNCTION__, instrID, length, icdPat1, icdPat2, icdPat3, icdPat4);
    // fill in status_bits, address, data, byte enable, rw, start, App ID, checksum from bit 214 to 293

    for (int i = 0; i < (length / 32); i++)
    {
        jtagData[i] = 0;
        if ((i - startBit / 32) == 0)
            jtagData[i] = icdPat1;
        if ((i - startBit / 32) == 1)
            jtagData[i] = icdPat2;
        if ((i - startBit / 32) == 2)
            jtagData[i] = icdPat3;
        if (((i - startBit / 32) == 3) && (shiftBit > 16))
            jtagData[i] = icdPat4;
        InfoPrintf("Info: jtagData[%d] is 0x%x\n", i, jtagData[i]);
    }
    if (length % 32)
    {   // icdPat3 or icdPat4 may here ,it's ok if we dont in end of the chain.
        if ((length / 32 - startBit / 32) == 2)
            jtagData[length / 32] = icdPat3;
        if (((length / 32 - startBit / 32) == 3) && (shiftBit > 16))
            jtagData[length / 32] = icdPat4;
        InfoPrintf("Info: jtagData[%d] is 0x%x\n", length / 32, jtagData[length / 32]);
    }

    // Call host2jtag api for data writting
    // refer to //hw/lwgpu/tools/mods/trace_3d/plugins/fc_misc/tests/source/host2ieee1500.cpp
    UINT32 dwordEn;     // dwordEn is 0 for writes and should be whatever dwords you want to access during reads
    UINT32 status;
    UINT32 readOrWrite;// readOrWrite should be set to 1 for write and 0 for read.
    UINT32 captDis;
    UINT32 updtDis;
    UINT32 burstMode;   // burstMode should be always be 1
    UINT32 reqCtrl;     // reqCtrl should be 1 when you want to make a request and should be cleared at the end of each access

    dwordEn      = 0;
    status        = 0;
    readOrWrite = write;
    captDis      = 0x0;
    updtDis      = 0x0;
    burstMode    = 0x1;
    reqCtrl      = 0x1;

    InfoPrintf("regLength : 0x%x, ClusterId : 0x%x, instrID : 0x%x    \n", length, clusterID, instrID);
    InfoPrintf("Writing %d JTAG register of length %d  Instr Id %d and clusterID %d\n", instrID,  length, instrID, clusterID);
    // Updating config and control register before writing into Jtag chains.
    Host2jtagConfigWrite(dwordEn, length, readOrWrite, captDis, updtDis, burstMode, false);
    Host2jtagCtrlWrite(reqCtrl, status, clusterID, dwordEn, length, instrID, readOrWrite);

    for (int i = 0; i < (length / 32); i++)
    {
        Host2jtagDataWrite(jtagData[i]);
        InfoPrintf("write data is  %x \n", jtagData[i]);
    }
    if (length % 32)
    {
        Host2jtagDataWrite(jtagData[length / 32]);
        InfoPrintf("write data is  %x \n", jtagData[length / 32]);
    }
    return true;
}

bool jtag2icd_lwdec::GetJTAGPat
(
    int clusterID,
    int instrID,
    int length,
    UINT32* rdat,
    UINT32 startBit
)
{
    UINT32 shiftBit   = 0;
    UINT32 icdPat1Readback = 0;
    UINT32 icdPat2Readback = 0;
    UINT32 icdPat3Readback = 0;
    UINT32 icdPat4Readback = 0;

    // Call host2jtag api for data writting
    UINT32 dwordEn;       // dwordEn is 0 for writes and should be whatever dwords you want to access during reads
    UINT32 status;
    UINT32 readOrWrite;  // readOrWrite should be set to 1 for write and 0 for read.
    UINT32 captDis;
    UINT32 updtDis;
    UINT32 burstMode;     // burstMode should be always be 1
    UINT32 reqCtrl;       // reqCtrl should be 1 when you want to make a request and should be cleared at the end of each access
    UINT32 readData;

    dwordEn      = 0;
    status        = 0;
    readOrWrite = 0x0;
    captDis      = 0x0;
    updtDis      = 0x0;
    burstMode    = 0x1;
    reqCtrl      = 0x1;

    reqCtrl      = 0;
    dwordEn      = 0;
    readOrWrite = 1;
    Host2jtagCtrlWrite(reqCtrl, status, clusterID, dwordEn, length, instrID, readOrWrite);

    InfoPrintf("Reading register  of length %d instr id %d and clusterID %d \n", length, instrID, clusterID);
    reqCtrl    = 1;
    readOrWrite = 0;

    UINT32 count = (length + 31) / 32;
    for (UINT32 i = 0; i < count; i++)
    {
        Host2jtagConfigWrite(i, length, readOrWrite, captDis, updtDis, burstMode, false);
        Host2jtagCtrlWrite(reqCtrl, status, clusterID, i, length, instrID, readOrWrite);
        Host2jtagDataRead(readData, i, length);
        InfoPrintf("read data is  %x \n", readData);

        if ((i - startBit / 32) == 0)
        {
            icdPat1Readback= readData;
            InfoPrintf("current read back parten is icdPat1Readback 0x%x\n", icdPat1Readback);
        }
        if ((i - startBit / 32) == 1)
        {
            icdPat2Readback= readData;
            InfoPrintf("current read back parten is icdPat2Readback 0x%x\n", icdPat2Readback);
        }
        if ((i - startBit / 32) == 2)
        {
            icdPat3Readback= readData;
            InfoPrintf("current read back parten is icdPat3Readback 0x%x\n", icdPat3Readback);
        }
        if ((i - startBit / 32) == 3)
        {
            icdPat4Readback= readData;
            InfoPrintf("current read back parten is icdPat4Readback 0x%x\n", icdPat4Readback);
        }
    }
    shiftBit = startBit % 32;
    icdPat1Readback = (icdPat1Readback >> shiftBit) | (icdPat2Readback << (32 - shiftBit)) ;
    icdPat2Readback = (icdPat2Readback >> shiftBit) | (icdPat3Readback << (32 - shiftBit));
    icdPat3Readback = (icdPat3Readback >> shiftBit) | (icdPat4Readback << (32 - shiftBit));
    InfoPrintf("readback data is 0x%x\n", icdPat2Readback);
    *rdat = icdPat2Readback;
    return true;
}

int jtag2icd_lwdec::Jtag2icdLwdecTest()
{
    InfoPrintf("jtag2icd_lwdec test starts after kimiy0 fix...\n");
    int clusterID = 0;
    int instr = 0;
    UINT32 startBit = 0;
    int unlockInstr = 0;
    int unlockLength = 0;
    // chain length is 295 bit
    int length = 0;
    if (arch == 0x707)        // gm107
        instr = 0xd1;         // b11010001. Found in TOT/package/gm107/jtagreg/k_s0/JTAG2LWDEC_FALCON.jrd
    else if (arch == 0x800)   // gp100 //sw/dev/gpu_drv/chips_a/diag/mods/gpu/include/gpulist.h
    {
        instr = 0x9b;         // 8'b10011011   Found in //hw/lwgpu/vmod/ieee1500/chip/instruction_map.yaml
        InfoPrintf("LWDEC is selected in gp100\n");
    }
    else if (arch == Gpu::GP102) // gp102
    {
        InfoPrintf("jtag2icd_lwdec: Setup3, arch is 0x%x\n", arch);
        clusterID = 3; // 'm0_0_jtag2lwdec_falcon_CLUSTER_ID' in tot/vmod/ieee1500/chip/gp102/jtagreg/Host2jtag_info.report
        instr = 0x9b;   //  8'b10011011   Found in //hw/lwgpu_gp102/vmod/ieee1500/chip/instruction_map.yaml
                        //  also value of 'm0_0_jtag2lwdec_falcon_ID' in tot/vmod/ieee1500/chip/gp102/jtagreg/Host2jtag_info.report
        startBit = 22; // 'm0_0_jtag2lwdec_falcon_m0_0_gpjm0msd0_GPJ0MMS0_mms_logic_lwdec_u_part_a_status_bits_LSB' in tot/vmod/ieee1500/chip/gp102/jtagreg/Host2jtag_info.report, the key is 'logic_lwdec_u_part_a_status_bits_LSB'
        length = 105;   // 'm0_0_jtag2lwdec_falcon_CHAINLENGTH' in tot/vmod/ieee1500/chip/gp102/jtagreg/Host2jtag_info.report
        InfoPrintf("LWDEC is selected in gp102\n");
        InfoPrintf("arch = 0x%x, instr = 0x%x, length = %d startBit = %d\n", arch, instr,length, startBit);
        unlockInstr = 254; // 'm0_0_JTAG_SEC_UNLOCK_STATUS_ID' in tot/vmod/ieee1500/chip/gp102/jtagreg/Host2jtag_info.report

        unlockLength = 34; // 'm0_0_JTAG_SEC_UNLOCK_STATUS_CHAINLENGTH' in tot/vmod/ieee1500/chip/gp102/jtagreg/Host2jtag_info.report

    }
    else if (arch == Gpu::GP104) // gp104
    {
        InfoPrintf("jtag2icd_lwdec: Setup3, arch is 0x%x\n", arch);
        clusterID = 3;
        instr = 0x9b;
        startBit = 18;     // 'm0_0_jtag2lwdec_falcon_m0_0_gpdm0msd0_GPD0MMS0_mms_logic_lwdec_u_part_a_status_bits_LSB' in tot/vmod/ieee1500/chip/gp104/jtagreg/Host2jtag_info.report, the key is 'logic_lwdec_u_part_a_status_bits_LSB'
        length = 101;
        InfoPrintf("LWDEC is selected in gp104\n");
        InfoPrintf("arch = 0x%x, instr = 0x%x, length = %d startBit = %d\n", arch, instr, length, startBit);
        unlockInstr = 254;
        unlockLength = 30;
    }
    else if (arch == Gpu::GP106) // gp106
    {
        InfoPrintf("jtag2icd_lwdec: Setup3, arch is 0x%x\n", arch);
        clusterID = 2;
        instr = 0x9b;
        startBit = 15;
        length = 97;
        InfoPrintf("LWDEC is selected in gp106\n");
        InfoPrintf("arch = 0x%x, instr = 0x%x, length = %d startBit = %d\n", arch, instr, length, startBit);
        unlockInstr = 254;
        unlockLength = 30;
    }
    else if (arch == Gpu::GP107) // gp107
    {
        InfoPrintf("jtag2icd_lwdec: Setup3, arch is 0x%x\n", arch);
        clusterID = 1;
        instr = 0x9b;
        startBit = 15;
        length = 110;
        InfoPrintf("LWDEC is selected in gp107\n");
        InfoPrintf("arch = 0x%x, instr = 0x%x, length = %d startBit = %d\n", arch, instr, length, startBit);
        unlockInstr = 254;
        unlockLength = 23;
    }
    else if (arch == Gpu::GP108) // gp108
    {
        InfoPrintf("jtag2icd_lwdec: Setup3, arch is 0x%x\n", arch);
        clusterID = 1;
        instr = 0x9b;
        startBit = 20;
        length = 101;
        InfoPrintf("LWDEC is selected in gp108\n");
        InfoPrintf("arch = 0x%x, instr = 0x%x, length = %d startBit = %d\n", arch, instr, length, startBit);
        unlockInstr = 254;
        unlockLength = 21;
    }
    else if (arch == Gpu::GV100) // gv100
    {
        InfoPrintf("jtag2icd_lwdec: Setup3, arch is 0x%x\n", arch);
        clusterID = 1;
        instr = 0x9b;
        startBit = 37;
        length = 126;
        InfoPrintf("LWDEC is selected in gv100\n");
        InfoPrintf("arch = 0x%x, instr = 0x%x, length = %d startBit = %d\n", arch, instr, length, startBit);
        unlockInstr = 254;
        unlockLength = 33;
    }
    else
    {
        ErrPrintf("The project name is not defined \n");
        return 1;
    }

    InfoPrintf("%s: unlock jtag security status \n",__FUNCTION__);
    Host2jtagUnlockJtag(clusterID, unlockInstr, unlockLength);

    InfoPrintf("%s: arch = 0x%x, instr = 0x%x\n",__FUNCTION__, arch, instr);

    UINT32 address, data, write;
    UINT32 Rdat;

    //===================================================================
    // step 1 -- JTAG write and read FALCON_MAILBOX0 directly
    //===================================================================
    InfoPrintf("step 1 -- JTAG write and read FALCON_MAILBOX0 directly\n");

    //-------------------------------------------------
    // step 1.1 -- JTAG write FALCON_MAILBOX0 directly
    //-------------------------------------------------
    InfoPrintf("step 1.1 -- JTAG write FALCON_MAILBOX0 directly\n");

    // JTAG write LW_CLWDEC_FALCON_MAILBOX0
    InfoPrintf("JTAG write FALCON_MAILBOX0\n");
    address = FALCON_MAILBOX0;
    data    = 0xbeefbeef;
    write   = 1;
    SetJTAGPat(clusterID, instr, length, address, data, write, 1, startBit);

    //-------------------------------------------------
    // step 1.2 -- JTAG read FALCON_MAILBOX0 directly
    //-------------------------------------------------
    InfoPrintf("step 1.2 -- JTAG read FALCON_MAILBOX0 directly\n");

    // JTAG read LW_CLWDEC _FALCON_MAILBOX0
    InfoPrintf("JTAG read FALCON_MAILBOX0\n");
    address = FALCON_MAILBOX0;
    data    = 0;
    write   = 0;
    SetJTAGPat(clusterID, instr, length, address, data, write, 1, startBit);

    // JTAG get the read-back value
    GetJTAGPat(clusterID, instr, length, &Rdat, startBit);
    if (Rdat != 0xbeefbeef)
    {
        ErrPrintf("JTAG read back fail, Rdat = 0x%x\n", Rdat);
        return 1;
    }
    else
    {
        InfoPrintf("JTAG read back pass, Rdat = 0x%x\n", Rdat);
    }

    //===================================================================
    // step 2 -- JTAG write and read FALCON_MAILBOX1 through ICD
    //===================================================================
    InfoPrintf("step 2 -- JTAG write and read FALCON_MAILBOX1 through ICD\n");

    //-------------------------------------------------
    // step 2.0 -- JTAG write STOP through ICD
    //-------------------------------------------------
    InfoPrintf("step 2.0 -- JTAG write STOP through ICD\n");
    InfoPrintf("write FALCON_ICD_CMD to STOP \n");
    address = FALCON_ICD_CMD;
    data    = FALCON_ICD_CMD_OPC_STOP |
              0x2 << 6 ;   // size
    write   = 1;
    SetJTAGPat(clusterID, instr, length, address, data, write, 1, startBit);

    //-------------------------------------------------
    // step 2.1 -- JTAG write FALCON_MAILBOX1 through ICD
    //-------------------------------------------------
    InfoPrintf("step 2.1 -- JTAG write LW_CLWDEC_FALCON_MAILBOX1 through ICD\n");

    // write FALCON_ICD_ADDR to LW_CLWDEC_FALCON_MAILBOX1
    InfoPrintf("write FALCON_ICD_ADDR to LW_CLWDEC_FALCON_MAILBOX1\n");
    address = FALCON_ICD_ADDR;
    data    = LW_CLWDEC_FALCON_MAILBOX1;
    write   = 1;
    SetJTAGPat(clusterID, instr, length, address, data, write, 1, startBit);

    // write FALCON_ICD_WDATA to 0xbadbeef
    InfoPrintf("write FALCON_ICD_WDATA to 0xbadbeef\n");
    address = FALCON_ICD_WDATA;
    data    = 0xbadbeef;
    write   = 1;
    SetJTAGPat(clusterID, instr, length, address, data, write, 1, startBit);

    // write FALCON_ICD_CMD to OPC_WCM
    InfoPrintf("write FALCON_ICD_CMD to OPC_WCM\n");
    address = FALCON_ICD_CMD;
    data    = FALCON_ICD_CMD_OPC_WCM |
              0x2 << 6 ;   // size
    write   = 1;
    SetJTAGPat(clusterID, instr, length, address, data, write, 1, startBit);

    //-------------------------------------------------
    // step 2.2 -- JTAG read LW_CLWDEC_FALCON_MAILBOX1 through ICD
    //-------------------------------------------------
    InfoPrintf("step 2.2 -- JTAG read LW_CLWDEC_FALCON_MAILBOX1 through ICD\n");

    // write FALCON_ICD_ADDR to LW_CLWDEC_FALCON_MAILBOX1
    InfoPrintf("write FALCON_ICD_ADDR to LW_CLWDEC_FALCON_MAILBOX1\n");
    address = FALCON_ICD_ADDR;
    data    = LW_CLWDEC_FALCON_MAILBOX1;
    SetJTAGPat(clusterID, instr, length, address, data, 1, 1, startBit);

    // write FALCON_ICD_CMD to OPC_RCM
    InfoPrintf("write FALCON_ICD_CMD to OPC_RCM\n");
    address = FALCON_ICD_CMD;
    data    = FALCON_ICD_CMD_OPC_RCM |
              0x2 << 6;
    SetJTAGPat(clusterID, instr, length, address, data, 1, 1, startBit);

    //-------------------------------------------------
    // step 2.3 -- JTAG polls ICD_CMD_RDVLD
    //-------------------------------------------------
    InfoPrintf("step 2.3 -- JTAG polls ICD_CMD_RDVLD\n");
    int rdvld = 0;
    int i     = 0;
    address   = FALCON_ICD_CMD;
    data      = 0;
    write     = 0;

    for (i = 0; i < 10; i++)
    {
        // JTAG polls  ICD_CMD_RDVLD
        InfoPrintf("JTAG polls ICD_CMD_RDVLD, times = 0x%x\n", i);
        SetJTAGPat(clusterID, instr, length, address, data, write, 1, startBit);

        // JTAG get the read-back value
        GetJTAGPat(clusterID, instr, length, &Rdat, startBit);
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
    // step 2.4 -- JTAG read FALCON_ICD_RDATA
    //-------------------------------------------------
    InfoPrintf("step 2.4 -- JTAG read FALCON_ICD_RDATA\n");

    // JTAG read FALCON_ICD_RDATA
    InfoPrintf("JTAG read FALCON_ICD_RDATA\n");
    address = FALCON_ICD_RDATA;
    data    = 0;
    write   = 0;
    SetJTAGPat(clusterID, instr, length, address, data, write, 1, startBit);

    // JTAG get the read-back value
    GetJTAGPat(clusterID, instr, length, &Rdat, startBit);
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
