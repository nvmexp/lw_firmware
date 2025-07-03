/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2008-2018, 2020-2021 by LWPU Corporation.  All rights reserved. 
 * All information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */
// To facilitate faster compiles via pre-compiled headers, keep "stdtest.h" first
#include "mdiag/tests/stdtest.h"

#include "jtag2icd_pwr.h"
#include "mdiag/lwgpu.h"
#include "mdiag/utils/lwgpu_channel.h"
#include "mdiag/tests/test_state_report.h"
#include "sim/IChip.h"

#include "core/include/rc.h"
#include "gpu/perf/pmusub.h"

#define LW_PJTAG_ACCESS_CTRL                             0x0000C800 /* RW-4R */
#define LW_PJTAG_ACCESS_CTRL_INSTR_ID                           7:0 /* RW-UF */
#define LW_PJTAG_ACCESS_CTRL_REG_LENGTH                        18:8 /* RW-UF */
#define LW_PJTAG_ACCESS_CTRL_DWORD_EN                         24:19 /* RW-UF */
#define LW_PJTAG_ACCESS_CTRL_CLUSTER_SEL                      29:25 /* RW-UF */
#define LW_PJTAG_ACCESS_CTRL_CTRL_STATUS                      30:30 /* RW-UF */
#define LW_PJTAG_ACCESS_CTRL_REQ_CTRL                         31:31 /* RW-UF */
#define LW_PJTAG_ACCESS_DATA                             0x0000C804 /* RW-4R */
#define LW_PJTAG_ACCESS_DATA_VALUE                             31:0 /* RW--F */

#define LW_PJTAG_ACCESS_CONFIG                           0x0000C808 /* RW-4R */
#define LW_PJTAG_ACCESS_CONFIG_REG_LENGTH                      7:0 /* RW-UF */
#define LW_PJTAG_ACCESS_CONFIG_DWORD_EN                       15:8 /* RW-UF */
#define LW_PJTAG_ACCESS_CONFIG_BURST_MODE                     16:16 /* RW-UF */
#define LW_PJTAG_ACCESS_CONFIG_READ_WRITE                     17:17 /* RW-UF */
#define LW_PJTAG_ACCESS_CONFIG_LAST_CLUSTER_ILW               18:18 /* RW-UF */
#define LW_PJTAG_ACCESS_CONFIG_ISM_MST_EN                     19:19 /* RW-UF */
#define LW_PJTAG_ACCESS_CONFIG_UPDT_DIS                       20:20 /* RW-UF */
#define LW_PJTAG_ACCESS_CONFIG_CAPT_DIS                       21:21 /* RW-UF */
#define LW_PJTAG_ACCESS_CONFIG_PAUSE_CYCLES                   24:22 /* RW-UF */
#define LW_PJTAG_ACCESS_CONFIG_SPARE2                         28:25 /* RW-UF */
#define LW_PJTAG_ACCESS_CONFIG_HOST_CONTROL_ERROR             29:29 /* RW-UF */
#define LW_PJTAG_ACCESS_CONFIG_BURST_READ_WARNING             30:30 /* RW-UF */
#define LW_PJTAG_ACCESS_CONFIG_BURST_READ_ERROR               31:31 /* RW-UF */

#define FALCON_ICD_CMD                            0x200   //FIXME
#define FALCON_ICD_ADDR                           0x204   //FIXME
#define FALCON_ICD_WDATA                          0x208   //FIXME
#define FALCON_ICD_RDATA                          0x20c   //FIXME

#define FALCON_MAILBOX0                           0x40   //FIXME
#define FALCON_MAILBOX1                           0x44   //FIXME

#define FALCON_ICD_CMD_OPC_WCM                       0x0000000d
#define FALCON_ICD_CMD_OPC_RCM                       0x0000000c
#define FALCON_ICD_CMD_OPC_RSTAT                     0x0000000e
#define FALCON_ICD_CMD_OPC_RREG                      0x00000008
#define FALCON_ICD_CMD_OPC_WREG                      0x00000009
#define FALCON_ICD_CMD_OPC_STOP                      0x00000000

#define LW_CMSDEC_FALCON_MAILBOX1                               0x00000044

// define which tests are run
static int jtag2pwr_basic=0;

extern const ParamDecl jtag2icd_pwr_params[] =
{
    SIMPLE_PARAM("-jtag2pwr_basic", "Testing jtag2pwr_basic"),
    STRING_PARAM("-pwrimage", "String to specify which binary image to use to bootstrap PMU"),
    PARAM_SUBDECL(lwgpu_single_params),
    LAST_PARAM
};

// constructor
jtag2icd_pwr::jtag2icd_pwr(ArgReader *params) :
    LWGpuSingleChannelTest(params)
{
    if (params->ParamPresent("-jtag2pwr_basic"))
        jtag2pwr_basic= 1;
    if ( params->ParamPresent( "-pwrimage" ) > 0 )
        m_paramsString = params->ParamStr( "-pwrimage" );
}

// destructor
jtag2icd_pwr::~jtag2icd_pwr(void)
{
}

// Factory
STD_TEST_FACTORY(jtag2icd_pwr,jtag2icd_pwr)

// setup method
int jtag2icd_pwr::Setup(void)
{
    lwgpu = LWGpuResource::FindFirstResource();
    arch = lwgpu->GetArchitecture();
    InfoPrintf("jtag2icd_pwr: Setup, arch is 0x%x\n", arch);
    ch = lwgpu->CreateChannel();
    if(!ch)
    {
        ErrPrintf("jtag2icd_pwr::Setup failed to create channel\n");
        return 0;
    }
    getStateReport()->init("jtag2icd_pwr");
    getStateReport()->enable();

    return 1;
}

//CleanUp
void jtag2icd_pwr::CleanUp(void)
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

void jtag2icd_pwr::DelayNs(UINT32 Lwclks)
{
  // Lwrrently this function is in terms of .2nS / lwclk. This is incorrectly done
  // in RTL. This should be a close approx of 1 nanosecond.
  Platform::EscapeWrite("CLOCK_WAIT",IChip::ECLOCK_LWCLK,0,Lwclks * 5);
}

// run - main routine
void jtag2icd_pwr::Run(void)
{
    //  InfoPrintf Flag to determine whether the test fail because of elw issue or test issue.
    InfoPrintf("FLAG : jtag2icd_pwr starting ....\n");

    if(jtag2pwr_basic)
    {
        if (jtag2icd_pwrTest())
        {
        SetStatus(TEST_FAILED);
        ErrPrintf("jtag2icd_pwr::jtag2icd_pwrTest failed\n");
        return;
        }

    InfoPrintf("jtag2icd_pwrTest() Done !\n");
    } //endif_jtag2pwr_basic

    InfoPrintf("jtag2icd_pwr test complete\n");
    SetStatus(TEST_SUCCEEDED);
    getStateReport()->crcCheckPassedGold();

    ch->WaitForIdle();
}//end_Run

bool jtag2icd_pwr::host2JtagCfg(int instr, int length, int chiplet, int dw, int status, int req)
{
    UINT32 h2jRegCtrl=0;
    h2jRegCtrl |= REF_NUM(LW_PJTAG_ACCESS_CTRL_INSTR_ID, instr);
    h2jRegCtrl |= REF_NUM(LW_PJTAG_ACCESS_CTRL_REG_LENGTH, length);
    h2jRegCtrl |= REF_NUM(LW_PJTAG_ACCESS_CTRL_CLUSTER_SEL, chiplet);
    h2jRegCtrl |= REF_NUM(LW_PJTAG_ACCESS_CTRL_DWORD_EN, dw);
    h2jRegCtrl |= REF_NUM(LW_PJTAG_ACCESS_CTRL_CTRL_STATUS, status);
    h2jRegCtrl |= REF_NUM(LW_PJTAG_ACCESS_CTRL_REQ_CTRL, 0); //clean old req before each new req
    lwgpu->RegWr32(LW_PJTAG_ACCESS_CTRL, h2jRegCtrl);
    h2jRegCtrl |= REF_NUM(LW_PJTAG_ACCESS_CTRL_REQ_CTRL, req);
    lwgpu->RegWr32(LW_PJTAG_ACCESS_CTRL, h2jRegCtrl);

    Platform::ClockSimulator(10);
    bool timeOut = req ? true : false;
    UINT32 getStatus = 0;
    for(int i = 0; (getStatus == 0) && (req == 1) && (i< 50); i++)
    {
        getStatus = lwgpu->RegRd32(LW_PJTAG_ACCESS_CTRL);
        getStatus = REF_VAL(LW_PJTAG_ACCESS_CTRL_CTRL_STATUS, getStatus);
        if(getStatus==1)
            timeOut = false;
    }
    if(timeOut)
    {
        ErrPrintf("%s: pull the field CTRL_STATUS of the reg LW_PJTAG_ACCESS_CTRL time out\n",__FUNCTION__);
        return false;
    }
    return true;
}

bool jtag2icd_pwr::setJTAGPat(int instr, int length, UINT32 address, UINT32 data,  UINT32 write, UINT32 start)
{
   //     data    = 0xbeefbeef;
   // write   = 1;
   // FALCON_MAILBOX=0x1040
    UINT32 i;
    bool timeOut = true;
    UINT32 jtagCtrl    = 0;
    UINT32 jtagConfig    = 0;
    UINT32 jtag_data[4]; // chain length is 108 bits, so 5 words needed
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
    //checksum = 0x6f, tmpIcdPat3 = 0x3f, data = 0xbeefbeef, addr = 0x1040

    // fill in status_bits, address, data, byte enable, rw, start, App ID, checksum
    // TOT/vmod/ieee1500/chip/gp100/jtagreg/lw_top/s0_0/jtag2pwr_falcon.jrd
    memset(jtag_data,0,sizeof(jtag_data));
    jtag_data[0] = ((0x0<<14) |                        // status_bits[15:14]--map--> [15:14]   --->status_bits[0:1]
                   (((address >> 2) &  0xFFFF) << 16)); //address[45:16] --map--> [31:16] ---->address[2:17]
    jtag_data[1] = (((address >> 18) & 0x3FFF) |         // address[45:16]  --map--> [0:13]  ---->address[18:31]
                   ((data & 0x3FFFF) << 14));            // data[46:77]   --map--> [14:31]    ->data[0:17]
    jtag_data[2] = (((data >> 18) & 0x3FFF) |         //data[46:77]        --map--> [0:13]   --->data[18:31]
                (0xF << 14) |                  // byte enable[78:81]      --map--> [14:17]  --->byte enable[0:3]
                (write << 18) |              // read or write[82]         --map--> [18]     -->write[0]
                (start << 19) |            // start transactions[83]      --map--> [19]     -->start[0]
                (0x0 << 20) |             // App ID[84:85]               --map--> [20:21]  -->AppID[0:1]
                (checksum << 22));         // checksum[86:93]            --map--> [22:29]  -->checksum[0:7]
   InfoPrintf("%s: WRITE: instr 0x%x, length %d, data is 0x%x, 0x%x, 0x%x \n",__FUNCTION__, instr, length, jtag_data[1], jtag_data[2], jtag_data[3]);

   // Turn off host2jtag request
    lwgpu->RegWr32(LW_PJTAG_ACCESS_CONFIG, 0);

    // Write DWORD_EN and REG_LENGTH to ACCESS_CONFIG
    //lwgpu->SetRegFldNum("LW_PJTAG_ACCESS_CONFIG","_REG_LENGTH_MSB",length>>11);
    jtagConfig = REF_NUM(LW_PJTAG_ACCESS_CONFIG_REG_LENGTH, ((length & 0x7F800) >> 11));
    lwgpu->RegWr32(LW_PJTAG_ACCESS_CONFIG, jtagConfig);

    jtagCtrl = REF_NUM(LW_PJTAG_ACCESS_CTRL_REG_LENGTH, (length & 0x7FF));
    jtagCtrl |= REF_NUM(LW_PJTAG_ACCESS_CTRL_INSTR_ID, instr);
    jtagCtrl |= REF_NUM(LW_PJTAG_ACCESS_CTRL_CLUSTER_SEL, 1);
    jtagCtrl |= REF_NUM(LW_PJTAG_ACCESS_CTRL_REQ_CTRL, 1);
    lwgpu->RegWr32(LW_PJTAG_ACCESS_CTRL, jtagCtrl);

    Platform::DelayNS(10);

    for(int i = 0; (i< 50); i++)
    {
        jtagCtrl = lwgpu->RegRd32(LW_PJTAG_ACCESS_CTRL);
        jtagCtrl = REF_VAL(LW_PJTAG_ACCESS_CTRL_CTRL_STATUS, jtagCtrl);
        if(jtagCtrl==1){
            timeOut = false;
            break;
        }
    }
    if(timeOut)
    {
        ErrPrintf("%s: pull the field CTRL_STATUS of the reg LW_PJTAG_ACCESS_CTRL time out\n",__FUNCTION__);
        return false;
    }

    for(i = 0; i < sizeof(jtag_data)/sizeof(UINT32);i++){
        lwgpu->RegWr32(LW_PJTAG_ACCESS_DATA, jtag_data[i]);
    }

    Platform::DelayNS(10);

    // Turn off host2jtag request
    lwgpu->RegWr32(LW_PJTAG_ACCESS_CTRL, 0);

    return true;

//    //gain the icd patten
//    UINT32 icdPat1 = address;
//    UINT32 icdPat2 = data;
//    UINT32 icdPat3 = 0xf |           //Byte enable
//                     write << 4 |
//                     start << 5 |
//                     checksum << 8;
//
//    icdPat3 = (icdPat3 << 1) | ((icdPat2 & 0x80000000) >> 31);
//    icdPat2 = (icdPat2 << 1) | ((icdPat1 & 0x80000000) >> 31);
//    icdPat1 = (icdPat1 << 1);
//
//    int chiplet_sel = 0;
//
//    if(arch == 0x628)
//        chiplet_sel = 0;      //GK208
//    else if(arch == 0x62a)
//        chiplet_sel = 1;      //GK20a
//
//    InfoPrintf("%s: WRITE: instr 0x%x, length %d, data is 0x%x, 0x%x and 0x%x\n",__FUNCTION__, instr, length, icdPat3, icdPat2, icdPat1);
//
//    //configure host2jtag
//    if(!host2JtagCfg(instr, length-1, chiplet_sel, 0, 0, 1)) return false;   //configure ACCESS_CTRL
//
//    lwgpu->RegWr32(LW_PJTAG_ACCESS_DATA, icdPat1);              //configure ACCESS_DATA
//    InfoPrintf("%s: Write the first DW 0x%x\n",__FUNCTION__, icdPat1);
//    lwgpu->RegWr32(LW_PJTAG_ACCESS_DATA, icdPat2);
//    InfoPrintf("%s: Write the second DW 0x%x\n",__FUNCTION__, icdPat2);
//    lwgpu->RegWr32(LW_PJTAG_ACCESS_DATA, icdPat3);
//    InfoPrintf("%s: Write the third DW 0x%x\n",__FUNCTION__, icdPat3);
//
//    Platform::ClockSimulator(10);
//    host2JtagCfg(instr, length-1, chiplet_sel, 0, 0, 0);                     //release host2jtag request
//
//    return true;
}

bool jtag2icd_pwr::getJTAGPat(int instr, int length, UINT32* rdat)
{
    int chiplet_sel = 1;
    bool timeOut = true;
    UINT32 jtagCtrl    = 0;
    UINT32 jtagConfig    = 0;
    UINT32 dwordEn     = 0;
    UINT32 jtagData    = 0;

    // chain length is 108 bit, so 5 words needed
    UINT32 data[4];
    UINT32 dataArrayLen = 4;

    // Make sure to turn off host2jtag request - this should be redundant
    lwgpu->RegWr32(LW_PJTAG_ACCESS_CONFIG, 0);

    while (dwordEn < dataArrayLen)
    {
        timeOut = true;
        // Write DWORD_EN, REG_LENGTH, BURST_MODE to ACCESS_CONFIG
        //lwgpu->SetRegFldNum("LW_PJTAG_ACCESS_CONFIG","_REG_LENGTH_MSB",length>>11);
        //lwgpu->SetRegFldNum("LW_PJTAG_ACCESS_CONFIG","_DWORD_EN_MSB",dwordEn>>6);
        //lwgpu->SetRegFldNum("LW_PJTAG_ACCESS_CONFIG","_BURST_MODE",1);
        jtagCtrl    = 0;
        jtagConfig  = 0;

        jtagConfig = REF_NUM(LW_PJTAG_ACCESS_CONFIG_REG_LENGTH, ((length & 0x7F800) >> 11));
        jtagConfig |= REF_NUM(LW_PJTAG_ACCESS_CONFIG_DWORD_EN, dwordEn>>6);
        jtagConfig |= REF_NUM(LW_PJTAG_ACCESS_CONFIG_BURST_MODE, 1);
        lwgpu->RegWr32(LW_PJTAG_ACCESS_CONFIG, jtagConfig);

        // Update DWORD_EN
        jtagCtrl = REF_NUM(LW_PJTAG_ACCESS_CTRL_REG_LENGTH, (length & 0x7FF));
        jtagCtrl |= REF_NUM(LW_PJTAG_ACCESS_CTRL_INSTR_ID, instr);
        jtagCtrl |= REF_NUM(LW_PJTAG_ACCESS_CTRL_CLUSTER_SEL, chiplet_sel);
        jtagCtrl |= REF_NUM(LW_PJTAG_ACCESS_CTRL_DWORD_EN, dwordEn);
        jtagCtrl |= REF_NUM(LW_PJTAG_ACCESS_CTRL_REQ_CTRL, 1);
        lwgpu->RegWr32(LW_PJTAG_ACCESS_CTRL, jtagCtrl);

        Platform::DelayNS(10);

        for(int i = 0; (i< 50); i++)
        {
            jtagCtrl = lwgpu->RegRd32(LW_PJTAG_ACCESS_CTRL);
            jtagCtrl = REF_VAL(LW_PJTAG_ACCESS_CTRL_CTRL_STATUS, jtagCtrl);
            if(jtagCtrl==1){
                timeOut = false;
                break;
            }
        }
        if(timeOut)
        {
            ErrPrintf("%s: pull the field CTRL_STATUS of the reg LW_PJTAG_ACCESS_CTRL time out\n",__FUNCTION__);
            return false;
        }

        // Read ACCESS_DATA
        jtagData = lwgpu->RegRd32(LW_PJTAG_ACCESS_DATA);
        InfoPrintf("JTAG read back, jtagData = 0x%x\n", jtagData);

        if ((dwordEn == (dataArrayLen-1)) && ((length%32) != 0))
        {
            jtagData = jtagData >> (32-((length)%32));
        }
        *(data+dwordEn) = jtagData;

        // Increment
        dwordEn++;
    }

    Platform::DelayNS(10);

    // Turn off host2jtag request
    lwgpu->RegWr32(LW_PJTAG_ACCESS_CTRL, 0);

    // get data from bit 46 to 77
    *rdat = ((data[1] >> 14) & 0x3FFFF )|((data[2] & 0x3FFF) << 18);

    return true;

//    int max_dword_en = length / 32;

//    if(arch == 0x628)
//        chiplet_sel = 0;      //GK208
//    else if(arch == 0x62a)
//        chiplet_sel = 1;      //GK20a

//    lwgpu->SetRegFldNum("LW_PJTAG_ACCESS_CONFIG","_REG_LENGTH",0);
//    lwgpu->SetRegFldNum("LW_PJTAG_ACCESS_CONFIG","_DWORD_EN",dword_en);
//
//    if(!host2JtagCfg(instr, length+3, chiplet_sel, dword_en, 0, 1)) return false;
//    UINT32 rdat_tmp0 = lwgpu->RegRd32(LW_PJTAG_ACCESS_DATA);
//    InfoPrintf("%s: read data = 0x%x\n",__FUNCTION__, rdat_tmp0);
//    host2JtagCfg(instr, length+3, chiplet_sel, 0, 0, 0);                     //release host2jtag request
//
//    dword_en = 1;
//    lwgpu->SetRegFldNum("LW_PJTAG_ACCESS_CONFIG","_DWORD_EN",dword_en);
//    if(!host2JtagCfg(instr, length+3, chiplet_sel, dword_en, 0, 1)) return false;
//    UINT32 rdat_tmp1 = lwgpu->RegRd32(LW_PJTAG_ACCESS_DATA);
//    InfoPrintf("%s: read data = 0x%x\n",__FUNCTION__, rdat_tmp1);
//    host2JtagCfg(instr, length+3, chiplet_sel, 0, 0, 0);                     //release host2jtag request
//
//    dword_en = 2;
//    lwgpu->SetRegFldNum("LW_PJTAG_ACCESS_CONFIG","_DWORD_EN",dword_en);
//    if(!host2JtagCfg(instr, length+3, chiplet_sel, dword_en, 0, 1)) return false;
//    UINT32 rdat_tmp2 = lwgpu->RegRd32(LW_PJTAG_ACCESS_DATA);
//    InfoPrintf("%s: read data = 0x%x\n",__FUNCTION__, rdat_tmp2);
//
//    rdat_tmp2 = rdat_tmp2 >> (32 - (length + 4) % 32); // See https://wiki.lwpu.com/engwiki/index.php/Hwcad/GenPads/TestBus about how to read data from jtag chain
//
//    *rdat = ((rdat_tmp1 >> 1) & 0x7fffffff )|((rdat_tmp2 & 0x00000001) << 31);
//
//    Platform::ClockSimulator(10);
//    host2JtagCfg(instr, length+3, chiplet_sel, 0, 0, 0);                     //release host2jtag request
//
//    return true;
}

bool jtag2icd_pwr::setJTAGPat_PwrCtrl(UINT32 data)
{
    UINT32 i;
    int instr=0x1f;
    int length=449;
    bool timeOut = true;
    UINT32 jtagCtrl    = 0;
    UINT32 jtagConfig    = 0;
    UINT32 jtag_data[15]; // chain length is 449 bits, so 15 words needed
//    UINT32 tmpIcdPat3 = 0xf |        //Byte enable
//                        write << 4 |
//                        start << 5;
//
//    UINT32 checksum = (address & 0x000000ff)^
//                      ((address & 0x0000ff00) >> 8)^
//                      ((address & 0x00ff0000) >> 16)^
//                      ((address & 0xff000000) >> 24)^
//                      (data & 0x000000ff)^
//                      ((data & 0x0000ff00) >> 8)^
//                      ((data & 0x00ff0000) >> 16)^
//                      ((data & 0xff000000) >> 24)^
//                      (tmpIcdPat3 & 0x000000ff);

//    InfoPrintf("checksum = 0x%x, tmpIcdPat3 = 0x%x, data = 0x%x, addr = 0x%x\n", checksum, tmpIcdPat3, data, address);
    //checksum = 0x6f, tmpIcdPat3 = 0x3f, data = 0xbeefbeef, addr = 0x1040

    // fill in status_bits, address, data, byte enable, rw, start, App ID, checksum
    // TOT/vmod/ieee1500/chip/gp100/jtagreg/lw_top/s0_0/jtag2pwr_falcon.jrd
    memset(jtag_data,0,sizeof(jtag_data));
//    jtag_data[1] = ((0x0<<9) |                        // status_bits[41:42]--map--> [9:10]   --->status_bits[0:1]
//                 (((address >> 2) &  0x1FFFFF) << 11)); //address[72:43] --map--> [31:11] ---->address[2:22]
//    jtag_data[2] = (((address >> 23) & 0x1FF) |         // address[72:43]  --map--> [0:8]  ---->address[23:31]
//                 ((data & 0x7FFFFF) << 9));            // data[73:104]   --map--> [9:31]    ->data[0:22]
//    jtag_data[3] = (((data >> 23) & 0x1FF) |         //data[73:104]        --map--> [0:8]   --->data[23:31]
//              (0xF << 9) |                  // byte enable[105:108]      --map--> [9:12]  --->byte enable[0:3]
//              (write << 13) |              // read or write[109]         --map--> [13]     -->write[0]
//              (start << 14) |            // start transactions[110]      --map--> [14]     -->start[0]
//              (0x0 << 15) |             // App ID[111:112]               --map--> [15:16]  -->AppID[0:1]
//              (checksum << 17));         // checksum[113:120]            --map--> [17:24]  -->checksum[0:7]
   jtag_data[2] = data;
   InfoPrintf("%s: WRITE: instr 0x%x, length %d, data is 0x%x \n",__FUNCTION__, instr, length, jtag_data[1]);

   // Turn off host2jtag request
    lwgpu->RegWr32(LW_PJTAG_ACCESS_CONFIG, 0);

    // Write DWORD_EN and REG_LENGTH to ACCESS_CONFIG
    //lwgpu->SetRegFldNum("LW_PJTAG_ACCESS_CONFIG","_REG_LENGTH_MSB",length>>11);
    jtagConfig = REF_NUM(LW_PJTAG_ACCESS_CONFIG_REG_LENGTH, ((length & 0x7F800) >> 11));
    lwgpu->RegWr32(LW_PJTAG_ACCESS_CONFIG, jtagConfig);

    jtagCtrl = REF_NUM(LW_PJTAG_ACCESS_CTRL_REG_LENGTH, (length & 0x7FF));
    jtagCtrl |= REF_NUM(LW_PJTAG_ACCESS_CTRL_INSTR_ID, instr);
    jtagCtrl |= REF_NUM(LW_PJTAG_ACCESS_CTRL_CLUSTER_SEL, 1);
    jtagCtrl |= REF_NUM(LW_PJTAG_ACCESS_CTRL_REQ_CTRL, 1);
    lwgpu->RegWr32(LW_PJTAG_ACCESS_CTRL, jtagCtrl);

    Platform::DelayNS(10);

    for(int i = 0; (i< 50); i++)
    {
        jtagCtrl = lwgpu->RegRd32(LW_PJTAG_ACCESS_CTRL);
        jtagCtrl = REF_VAL(LW_PJTAG_ACCESS_CTRL_CTRL_STATUS, jtagCtrl);
        if(jtagCtrl==1){
            timeOut = false;
            break;
        }
    }
    if(timeOut)
    {
        ErrPrintf("%s: pull the field CTRL_STATUS of the reg LW_PJTAG_ACCESS_CTRL time out\n",__FUNCTION__);
        return false;
    }

    for(i = 0; i < sizeof(jtag_data)/sizeof(UINT32);i++){
        lwgpu->RegWr32(LW_PJTAG_ACCESS_DATA, jtag_data[i]);
    }

    Platform::DelayNS(10);

    // Turn off host2jtag request
    lwgpu->RegWr32(LW_PJTAG_ACCESS_CTRL, 0);

    return true;

}

bool jtag2icd_pwr::getJTAGPat_PwrCtrl( UINT32* rdat)
{
    int chiplet_sel = 1;
    int instr = 0x1f;
    int length = 449;
    bool timeOut = true;
    UINT32 jtagCtrl    = 0;
    UINT32 jtagConfig    = 0;
    UINT32 dwordEn     = 0;
    UINT32 jtagData    = 0;

    // chain length is 449 bit, so 15 words needed
    UINT32 data[15];
    UINT32 dataArrayLen = 15;

    // Make sure to turn off host2jtag request - this should be redundant
    lwgpu->RegWr32(LW_PJTAG_ACCESS_CONFIG, 0);

    while (dwordEn < dataArrayLen)
    {
        timeOut = true;
        // Write DWORD_EN, REG_LENGTH, BURST_MODE to ACCESS_CONFIG
        //lwgpu->SetRegFldNum("LW_PJTAG_ACCESS_CONFIG","_REG_LENGTH_MSB",length>>11);
        //lwgpu->SetRegFldNum("LW_PJTAG_ACCESS_CONFIG","_DWORD_EN_MSB",dwordEn>>6);
        //lwgpu->SetRegFldNum("LW_PJTAG_ACCESS_CONFIG","_BURST_MODE",1);
        jtagCtrl    = 0;
        jtagConfig  = 0;

        jtagConfig = REF_NUM(LW_PJTAG_ACCESS_CONFIG_REG_LENGTH, ((length & 0x7F800) >> 11));
        jtagConfig |= REF_NUM(LW_PJTAG_ACCESS_CONFIG_DWORD_EN, dwordEn>>6);
        jtagConfig |= REF_NUM(LW_PJTAG_ACCESS_CONFIG_BURST_MODE, 1);
        lwgpu->RegWr32(LW_PJTAG_ACCESS_CONFIG, jtagConfig);

        // Update DWORD_EN
        jtagCtrl = REF_NUM(LW_PJTAG_ACCESS_CTRL_REG_LENGTH, (length & 0x7FF));
        jtagCtrl |= REF_NUM(LW_PJTAG_ACCESS_CTRL_INSTR_ID, instr);
        jtagCtrl |= REF_NUM(LW_PJTAG_ACCESS_CTRL_CLUSTER_SEL, chiplet_sel);
        jtagCtrl |= REF_NUM(LW_PJTAG_ACCESS_CTRL_DWORD_EN, dwordEn);
        jtagCtrl |= REF_NUM(LW_PJTAG_ACCESS_CTRL_REQ_CTRL, 1);
        lwgpu->RegWr32(LW_PJTAG_ACCESS_CTRL, jtagCtrl);

        Platform::DelayNS(10);

        for(int i = 0; (i< 50); i++)
        {
            jtagCtrl = lwgpu->RegRd32(LW_PJTAG_ACCESS_CTRL);
            jtagCtrl = REF_VAL(LW_PJTAG_ACCESS_CTRL_CTRL_STATUS, jtagCtrl);
            if(jtagCtrl==1){
                timeOut = false;
                break;
            }
        }
        if(timeOut)
        {
            ErrPrintf("%s: pull the field CTRL_STATUS of the reg LW_PJTAG_ACCESS_CTRL time out\n",__FUNCTION__);
            return false;
        }

        // Read ACCESS_DATA
        jtagData = lwgpu->RegRd32(LW_PJTAG_ACCESS_DATA);
        InfoPrintf("JTAG read back, jtagData = 0x%x\n", jtagData);

        if ((dwordEn == (dataArrayLen-1)) && ((length%32) != 0))
        {
            jtagData = jtagData >> (32-((length)%32));
        }
        *(data+dwordEn) = jtagData;

        // Increment
        dwordEn++;
    }

    Platform::DelayNS(10);

    // Turn off host2jtag request
    lwgpu->RegWr32(LW_PJTAG_ACCESS_CTRL, 0);

    // get data from bit 73 to 104
    *rdat = data[2];

    return true;

}

int jtag2icd_pwr::jtag2icd_pwrTest()
{
    InfoPrintf("jtag2icd_pwr test starts...\n");
    Platform:: EscapeWrite ("global_unlock_during_fuse_sensing", 0, 1, 1);
        //-------------------------------------------------//
        //                                                 //
        //             Loading pwrimage.bin                //
        //                                                 //
        //-------------------------------------------------//
    PMU *pPmu;

    InfoPrintf("%s::Begin to load pwrimage.bin  .\n",__FUNCTION__);
    if (lwgpu->GetGpuSubdevice()->GetPmu(&pPmu) != OK)
    {
        ErrPrintf("Unable to get PMU Object\n");
    }

    InfoPrintf("%s:: Successfully to get PMU Object .\n",__FUNCTION__);
    //while (true) {
    //InfoPrintf("%s::Read MAILBOX(0) until it equals to 0x12345678 \n",__FUNCTION__);
    //if((lwgpu->RegRd32(0x0010a040, 0)) == 0x12345678) break;
    //}
    int instr = 0;
    int length = 135;
    if(arch == 0x419)
        instr = 0xd2;   //11010010
    else if(arch == 0x417)
        instr = 0xcc;   //11001100
    else if(arch == 0x604)
        instr = 0xcf;   //11001111
    else if(arch == 0x607)
        instr = 0xce;   //11001110
    else if(arch == 0x610)
        instr = 0xec;   // select D_S0_JTAG2PWR_FALCON. FIXME THIS INSTRUCTION NUMBER MAY CHANGE. So its better to replace this instr with some variable in case it changes again.
    else if(arch == 0x628) {
        instr = 0xca;      //GK208
    }
    else if(arch == 0x62a)
        instr = 0xca;      //GK20a
    else if(arch == 0x707)
        instr = 0xc9;      //GM107 11001001
    else if(arch == 0x708)
        instr = 0xc7;      //GM108 11000111
    else if(arch == 0x724) // gm204
        instr = 0x9a;
    else if(arch == 0x72b) // gm20b. First build tree, then find JTAG info in //hw/lwgpu/package/gm20b/*FALCON*.jrd
        instr = 0x8d;
    else if (arch==0x800){ //gp100
        instr= 0x9f;
        length = 135;
    }
    else if(arch==0x804) {//gp104
        instr = 0x9f;
        length = 108;
    }
    InfoPrintf("%s: arch = 0x%x, instr = 0x%x, length = %d\n",__FUNCTION__, arch, instr, length);

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

    //JTAG write LW_CMSDEC_FALCON_MAILBOX0
    InfoPrintf("JTAG write FALCON_MAILBOX0\n");
    address = FALCON_MAILBOX0;
    data    = 0xbeefbeef;
    write   = 1;
    setJTAGPat(instr, length, address, data, write, 1);

    //-------------------------------------------------
    //step 1.2 -- JTAG read FALCON_MAILBOX0 directly
    //-------------------------------------------------
    InfoPrintf("step 1.2 -- JTAG read FALCON_MAILBOX0 directly\n");

    //JTAG read LW_CMSDEC_FALCON_MAILBOX0
    InfoPrintf("JTAG read FALCON_MAILBOX0\n");
    address = FALCON_MAILBOX0;
    data    = 0;
    write   = 0;
    setJTAGPat(instr, length, address, data, write, 1);
    //JTAG get the read-back value
    getJTAGPat(instr, length,  &Rdat);

    if(Rdat != 0xbeefbeef) {
        ErrPrintf("JTAG read back fail, Rdat = 0x%x\n", Rdat);
        return 1;
    }
    else {
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
    setJTAGPat(instr, length, address, data, write, 1);

    //-------------------------------------------------
    //step 2.1 -- JTAG write FALCON_MAILBOX1 through ICD
    //-------------------------------------------------
    InfoPrintf("step 2.1 -- JTAG write FALCON_MAILBOX1 through ICD\n");

    //write FALCON_ICD_ADDR to LW_CMSDEC_FALCON_MAILBOX0
    InfoPrintf("write FALCON_ICD_ADDR to FALCON_MAILBOX1\n");
    address = FALCON_ICD_ADDR;
    data    = FALCON_MAILBOX1;
    write   = 1;
    setJTAGPat(instr, length, address, data, write, 1);

    //write FALCON_ICD_WDATA to 0xbadbeef
    InfoPrintf("write FALCON_ICD_WDATA to 0xbadbeef\n");
    address = FALCON_ICD_WDATA;
    data    = 0xbadbeef;
    write   = 1;
    setJTAGPat(instr, length, address, data, write, 1);

    //write FALCON_ICD_CMD to OPC_WCM
    InfoPrintf("write FALCON_ICD_CMD to OPC_WCM\n");
    address = FALCON_ICD_CMD;
    data    = FALCON_ICD_CMD_OPC_WCM |
              0x2 << 6 ;   //size
    write   = 1;
    setJTAGPat(instr, length, address, data, write, 1);

    //-------------------------------------------------
    //step 2.2.0 -- PRIV read FALCON_MAILBOX1
    // In order to make sure ICD has successfully write
    // targeted data to MALBOX1.
    //-------------------------------------------------
    InfoPrintf("step 2.2.0 -- JTAG read FALCON_MAILBOX1 \n");
    //read muti times. just to inser some delay to make sure that we can read back the writtten value
    int j=0;
    for (j=0; j<10; j++) {
      lwgpu->GetRegFldNum("LW_PPWR_FALCON_MAILBOX1","",&data);
    }
    InfoPrintf("LW_PPWR_FALCON_MAILBOX1 Rdata= %x\n", data);
    if(data != 0xbadbeef) {
        ErrPrintf("ICD write data fail, expected 0xbadbeef, actully is 0x%x\n", data);
        return 1;
    }else{
        InfoPrintf("ICD write MAILBOX1 successful\n");
    }

    //-------------------------------------------------
    //step 2.2 -- JTAG read FALCON_MAILBOX1 through ICD
    //-------------------------------------------------
    InfoPrintf("step 2.2 -- JTAG read FALCON_MAILBOX1 through ICD\n");

    //write FALCON_ICD_ADDR to FALCON_MAILBOX1
    InfoPrintf("write FALCON_ICD_ADDR to FALCON_MAILBOX1\n");
    address = FALCON_ICD_ADDR;
    //data    = LW_CMSDEC_FALCON_MAILBOX1;
    data    = FALCON_MAILBOX1;
    setJTAGPat(instr, length, address, data, 1, 1);

    //write FALCON_ICD_CMD to OPC_RCM
    InfoPrintf("write FALCON_ICD_CMD to OPC_RCM\n");
    address = FALCON_ICD_CMD;
    data    = FALCON_ICD_CMD_OPC_RCM |
              0x2 << 6;
    setJTAGPat(instr, length, address, data, 1, 1);

    //-------------------------------------------------
    //step 2.3 -- JTAG polls ICD_CMD_RDVLD
    //-------------------------------------------------
    InfoPrintf("step 2.3 -- JTAG polls ICD_CMD_RDVLD\n");
    int rdvld = 0;
    int i = 0;
    address = FALCON_ICD_CMD;
    data    = 0;
    write   = 0;

    for (i=0; i<10; i++) {
        //JTAG polls  ICD_CMD_RDVLD
        InfoPrintf("JTAG polls ICD_CMD_RDVLD, times = 0x%x\n", i);
        setJTAGPat(instr, length, address, data, write, 1);
        //JTAG get the read-back value
        getJTAGPat(instr, length,  &Rdat);
        InfoPrintf("JTAG read back, Rdat = 0x%x\n", Rdat);

        rdvld = Rdat & 0x8000;
        if(rdvld == 0x8000){
            InfoPrintf("ICD_CMD_RDVLD is valid, break the loop\n");
            break;
        }
    }

    if(i == 10){
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
    setJTAGPat(instr, length, address, data, write, 1);
    //JTAG get the read-back value
    getJTAGPat(instr, length, &Rdat);
    if(Rdat != 0xbadbeef) {
        ErrPrintf("JTAG read back fail, Rdat = 0x%x\n", Rdat);
        return 1;
    }
    else {
        InfoPrintf("good,JTAG read back pass, Rdat = 0x%x\n", Rdat);
    }
//access power_ctrl reg
    data = 0x30;
    setJTAGPat_PwrCtrl(data);
    getJTAGPat_PwrCtrl(&Rdat);
    if( (Rdat & 0x30) != 0x30) {
        ErrPrintf("power_ctrl,JTAG read back fail, Rdat = 0x%x\n", Rdat);
        return 1;
    }
    else {
        InfoPrintf("power_ctrl,JTAG read back pass, Rdat = 0x%x\n", Rdat);
    }
//
    return 0;
}
