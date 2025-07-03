/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2006-2018, 2020-2021 by LWPU Corporation.  All rights reserved. 
 * All information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */
// To facilitate faster compiles via pre-compiled headers, keep "stdtest.h" first
#include "mdiag/tests/stdtest.h"

/* Test lw_therm module
Basic Tests -
readThermI2CSlaveReg  - reads I2C Slave registers
writeThermI2CSlaveReg - writes I2C Slave registers

readThermPrivReg      - reads Priv registers via I2CSlave
writeThermPrivReg     - writes Priv registers via I2CSlave
*/

#include "i2cslave_fermi.h"
#include "mdiag/lwgpu.h"
#include "mdiag/utils/lwgpu_channel.h"
#include "mdiag/tests/test_state_report.h"

#define opt_int_ts_otob_en_addr  0x3
#define opt_int_ts_otob_en_mask  0x80000
#define opt_i2cs_addr_sel_addr   0x9
#define opt_i2cs_addr_sel_mask   0x300000
#define opt_i2cs_wide_en_addr    0x3
#define opt_i2cs_wide_en_mask    0x080000
#define LW_THERM_I2CS_SENSOR_21_MSK 0x000000FF

// define which tests are run
static int i2cslaveSanity = 0;
static int i2cslaveBasic = 0;
static int i2cscratch = 0;
static int i2cslavePriRead  = 0;
static int i2cslavePriWrite = 0;
static int i2cslavePriStress= 0;
static int i2cslaveArp= 0;
static int i2cslaveGpio= 0;
static int i2cslaveMsgbox= 0;
static int i2cslaveGpio_9= 0;
static int i2cslaveClkdiv=0;
static int i2cslaveOffset=0;
static int i2cslaveSmbAltAddr=0;
static int i2cslaveMsgbox_command=0;
static int i2cslaveMsgbox_datain=0;
static int i2cslaveMsgbox_dataout=0;
static int i2cslaveMsgbox_mutex=0;

extern const ParamDecl i2cslave_fermi_params[] = {
    SIMPLE_PARAM("-i2cslaveBasic", "Testing i2cslaveBasic"),
    SIMPLE_PARAM("-i2cscratch", "Testing i2cscratch"),
    SIMPLE_PARAM("-i2cslaveSanity", "Testing i2cslaveSanity"),
    SIMPLE_PARAM("-i2cslavePriRead", "Testing i2cslavePriRead"),
    SIMPLE_PARAM("-i2cslavePriWrite", "Testing i2cslavePriWrite"),
    SIMPLE_PARAM("-i2cslavePriStress", "Testing i2cslavePriStress"),
    SIMPLE_PARAM("-i2cslaveArp", "Testing i2cslaveArp"),
    SIMPLE_PARAM("-i2cslaveGpio", "Testing i2cslaveGpio"),
    SIMPLE_PARAM("-i2cslaveMsgbox", "Testing i2cslaveMsgbox"),
    SIMPLE_PARAM("-i2cslaveGpio_9", "Testing i2cslaveGpio_9"),
    SIMPLE_PARAM("-i2cslaveClkdiv", "Testing i2cslaveClkdiv"),
    SIMPLE_PARAM("-i2cslaveOffset", "Testing i2cslaveOffset"),
    SIMPLE_PARAM("-i2cslaveSmbAltAddr", "Testing i2cslaveSmbAltAddr"),
    SIMPLE_PARAM("-i2cslaveMsgbox_command", "Testing i2cslaveMsgbox_command"),
    SIMPLE_PARAM("-i2cslaveMsgbox_datain", "Testing i2cslaveMsgbox_datain"),
    SIMPLE_PARAM("-i2cslaveMsgbox_dataout", "Testing i2cslaveMsgbox_dataout"),
    SIMPLE_PARAM("-i2cslaveMsgbox_mutex", "Testing i2cslaveMsgbox_mutex"),
    PARAM_SUBDECL(lwgpu_single_params),
    LAST_PARAM
};

// constructor
i2cslave_fermi::i2cslave_fermi(ArgReader *params) :
    LWGpuSingleChannelTest(params)
{
    if (params->ParamPresent("-i2cslaveBasic"))
       i2cslaveBasic = 1;
    if (params->ParamPresent("-i2cscratch"))
       i2cscratch = 1;
    if (params->ParamPresent("-i2cslaveSanity"))
       i2cslaveSanity = 1;
    if (params->ParamPresent("-i2cslavePriRead"))
       i2cslavePriRead = 1;
    if (params->ParamPresent("-i2cslavePriWrite"))
       i2cslavePriWrite = 1;
    if (params->ParamPresent("-i2cslavePriStress"))
       i2cslavePriStress = 1;
    if (params->ParamPresent("-i2cslaveArp"))
       i2cslaveArp = 1;
    if (params->ParamPresent("-i2cslaveGpio"))
       i2cslaveGpio= 1;
    if (params->ParamPresent("-i2cslaveMsgbox"))
       i2cslaveMsgbox= 1;
    if (params->ParamPresent("-i2cslaveGpio_9"))
       i2cslaveGpio_9= 1;
    if (params->ParamPresent("-i2cslaveClkdiv"))
       i2cslaveClkdiv= 1;
    if (params->ParamPresent("-i2cslaveOffset"))
       i2cslaveOffset= 1;
    if (params->ParamPresent("-i2cslaveSmbAltAddr"))
       i2cslaveSmbAltAddr= 1;
    if (params->ParamPresent("-i2cslaveMsgbox_command"))
       i2cslaveMsgbox_command= 1;
    if (params->ParamPresent("-i2cslaveMsgbox_datain"))
       i2cslaveMsgbox_datain= 1;
    if (params->ParamPresent("-i2cslaveMsgbox_dataout"))
       i2cslaveMsgbox_dataout= 1;
    if (params->ParamPresent("-i2cslaveMsgbox_mutex"))
       i2cslaveMsgbox_mutex= 1;
}

// destructor
i2cslave_fermi::~i2cslave_fermi(void)
{
}

// Factory
STD_TEST_FACTORY(i2cslave_fermi,i2cslave_fermi)

// setup method
int i2cslave_fermi::Setup(void)
{
    lwgpu = LWGpuResource::FindFirstResource();
    m_arch = lwgpu->GetArchitecture();

    ch = lwgpu->CreateChannel();
    if(!ch)
    {
        ErrPrintf("I2cslave::Setup failed to create channel\n");
        return 0;
    }
    getStateReport()->init("i2cslave_fermi");
    getStateReport()->enable();

    return 1;
}

//CleanUp
void i2cslave_fermi::CleanUp(void)
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

// run - main routine
void i2cslave_fermi::Run(void)
{
  const char   *manual_name;
  UINT32   reg_value;
  UINT32   pri_wdat;
  UINT32 pri_rdat1, pri_rdat2;

    InfoPrintf("I2cslave Test at Run\n");
    InfoPrintf("I2cslave support/base registers used for architecture G/GT-%x\n",m_arch);
    SetStatus(TEST_INCOMPLETE);
    lwgpu->SetRegFldNum("LW_THERM_SENSOR_1", "", 0);
    lwgpu->SetRegFldNum("LW_THERM_SENSOR_3", "", 0);

    if((m_arch & 0xff0) == 0x410){  //for gf11x
        //initial GPIO_8,9,12
        if (lwgpu->SetRegFldNum("LW_PMGR_GPIO_INPUT_CNTL_8", "_PINNUM", 8)) { ErrPrintf("could not update LW_PMGR_GPIO_INPUT_CNTL_8"); }
        if (lwgpu->SetRegFldNum("LW_PMGR_GPIO_INPUT_CNTL_9", "_PINNUM", 9)) { ErrPrintf("could not update LW_PMGR_GPIO_INPUT_CNTL_9"); }
        if (lwgpu->SetRegFldNum("LW_PMGR_GPIO_INPUT_CNTL_10", "_PINNUM", 12)) { ErrPrintf("could not update LW_PMGR_GPIO_INPUT_CNTL_10"); }
    }

    if(m_arch > 0x600) {
        if (lwgpu->SetRegFldNum("LW_THERM_POWER_6", "", 0)) { ErrPrintf("could not update LW_THERM_POWER_6"); }
    }

    //======FIXME======
    //Platform::EscapeWrite("drv_opt_i2cs_addr_sel", 0, 2, 0);
    //Platform::EscapeWrite("drv_opt_i2cs_wide_en", 0, 1, 0);
    //Platform::EscapeWrite("drv_opt_int_ts_otob_en", 0, 1, 0);
    //Platform::EscapeWrite("drv_opt_int_ts_a", 0, 10, 0);
    //Platform::EscapeWrite("drv_opt_int_ts_b", 0, 10, 0);
    //lwgpu->SetRegFldDef("LW_FUSE_FUSECTRL", "_CMD", "_READ");
    //=================

    i2cSetup();

     // Disable I2C registers protection(Enable R/W to critical registers),
     // this feature will be tested in a seperate test.
     lwgpu->SetRegFldDef("LW_THERM_THERMAL_I2CS_ENABLE", "_WRITE", "_EN");
     lwgpu->SetRegFldDef("LW_THERM_THERMAL_I2CS_ENABLE", "_READ", "_EN");

    //enable I2C_CFG_PRIV_ENABLE
    if (i2cslavePriStress)
    {
      Platform:: EscapeWrite ("drv_opt_i2cs_wide_en", 0, 1, 1);
      InfoPrintf("test Pri_stress() starting ....\n");
      if (testPriAccess(DEVICE_ADDR_0_FERMI) ) return;
    } //endif_i2cslavePriStress

    if (i2cslaveBasic)
    {
     InfoPrintf("test i2cslaveBasic() starting ....\n");
     if(testBasic())
     {
	 SetStatus(TEST_FAILED);
	 ErrPrintf("i2cslaveBasic test failed\n");
	 return;
     }
     InfoPrintf("test i2cslaveBasic() Done !\n");
    } //endif_i2cslaveBasic

    if (i2cscratch)
    {
     InfoPrintf("test i2cscratch() starting ....\n");
     testscratch();

     InfoPrintf("test i2cscratch() Done !\n");
    } //endif_i2cscratch

    if (i2cslaveArp)
    {
     InfoPrintf("test i2cslaveArp() starting\n");
     if(testArp())
     {
         SetStatus(TEST_FAILED);
         ErrPrintf("i2cslaveArp test failed\n");
         return;
     }
     InfoPrintf("test i2cslaveArp() Done !\n");
    } //endif_i2cslaveArp

    if (i2cslavePriRead)
    {
      InfoPrintf("test Pri_Read() starting ....\n");

      Platform:: EscapeWrite ("drv_opt_i2cs_wide_en", 0, 1, 1);
      i2cPriEn(DEVICE_ADDR_0_FERMI);

      manual_name = "LW_THERM_I2CS_SENSOR_FE";
      if (lwgpu->GetRegNum(manual_name,&reg_value))
      { ErrPrintf("Reg %s did not exist!\n",manual_name); }
      InfoPrintf("Reg %s number 0x%32x\n",manual_name, reg_value);

      lwgpu->GetRegFldNum("LW_THERM_I2CS_SENSOR_FE","", &pri_rdat1);
      pri_rdat2 = setupPriRead(DEVICE_ADDR_0_FERMI,reg_value);
      if ( pri_rdat1 != pri_rdat2 )  {
          ErrPrintf("Priv read value from i2c is incorrect. expected %32x,  got %32x", pri_rdat1, pri_rdat2 );
          SetStatus(TEST_FAILED);
          return;
      }
      else
          InfoPrintf("Priv read value from i2c is correct. expected %32x,  got %32x", pri_rdat1, pri_rdat2 );

      InfoPrintf("test Pri_Read() Done !\n");
    } //endif_i2cslavePriRead

   if (i2cslavePriWrite)
    {
      InfoPrintf("test Pri_Write() starting ....\n");

      Platform:: EscapeWrite ("drv_opt_i2cs_wide_en", 0, 1, 1);
      i2cPriEn(DEVICE_ADDR_0_FERMI);

      manual_name = "LW_THERM_I2CS_SENSOR_03";
      if (lwgpu->GetRegNum(manual_name,&reg_value))
      { ErrPrintf("Reg %s did not exist!\n",manual_name); }
      InfoPrintf("Reg %s number 0x%32x\n",manual_name, reg_value);

      pri_wdat =0x12;
      if (setupPriWrite(DEVICE_ADDR_0_FERMI,reg_value,pri_wdat)) return;

      InfoPrintf("test Pri_Write() Done !\n");
    } //endif_i2cslavePriWrite

   if (i2cslaveGpio)
    {
      Platform:: EscapeWrite ("drv_opt_i2cs_wide_en", 0, 1, 1);
      if (testCheckGpio())
      {
         SetStatus(TEST_FAILED);
         ErrPrintf("i2cslave::checkGpio test failed\n");
         return;
      }

      InfoPrintf("test Check_Gpio() Done !\n");
    } //endif_i2cslaveGpio

   if (i2cslaveClkdiv)
    {
      if (testClkdiv())
      {
         SetStatus(TEST_FAILED);
         ErrPrintf("i2cslave::checkClkdiv test failed\n");
         return;
      }

      InfoPrintf("test Check_Clkdiv() Done !\n");
    } //endif_i2cslaveClkdiv

   if (i2cslaveMsgbox)
    {
      if (testMsgbox())
      {
         SetStatus(TEST_FAILED);
         ErrPrintf("i2cslave::Msgbox test failed\n");
         return;
      }

      InfoPrintf("testMsgbox() Done !\n");
    } //endif_i2cslaveMsgbox

   if (i2cslaveOffset)
    {
      if (testOffset())
      {
         SetStatus(TEST_FAILED);
         ErrPrintf("i2cslave::Offset test failed\n");
         return;
      }

      InfoPrintf("testOffset() Done !\n");
    } //endif_i2cslaveOffset

   if (i2cslaveGpio_9)
    {
      if (testGpio_9())
      {
         SetStatus(TEST_FAILED);
         ErrPrintf("i2cslave::Gpio_9 test failed\n");
         return;
      }

      InfoPrintf("testGpio_9() Done !\n");
    } //endif_Gpio_9

   if (i2cslaveSmbAltAddr)
    {
      if (test_smb_alt_addr())
      {
         SetStatus(TEST_FAILED);
         ErrPrintf("i2cslave::test_smb_alt_addr failed\n");
         return;
      }
      InfoPrintf("test_smb_alt_addr() Done !\n");
    } //endif_smb_alt_addr

    if (i2cslaveSanity)
    {
      if(checkSanity())
      {
          SetStatus(TEST_FAILED);
          getStateReport()->runFailed("Failed checkSanity\n");
          ErrPrintf("I2cslave::checkSanity test failed\n");
          return;
      }
    } //endif_i2cslaveSanity

    if (i2cslaveMsgbox_command)
    {
      if (testMsgbox_command())
      {
         SetStatus(TEST_FAILED);
         ErrPrintf("i2cslave::Msgbox_command test failed\n");
         return;
      }

      InfoPrintf("testMsgbox_command() Done !\n");
    } //endif_i2cslaveMsgbox_command

    if (i2cslaveMsgbox_datain)
    {
      if (testMsgbox_datain())
      {
         SetStatus(TEST_FAILED);
         ErrPrintf("i2cslave::Msgbox_datain test failed\n");
         return;
      }

      InfoPrintf("testMsgbox_datain() Done !\n");
    } //endif_i2cslaveMsgbox_datain

    if (i2cslaveMsgbox_dataout)
    {
      if (testMsgbox_dataout())
      {
         SetStatus(TEST_FAILED);
         ErrPrintf("i2cslave::Msgbox_dataout test failed\n");
         return;
      }

      InfoPrintf("testMsgbox_dataout() Done !\n");
    } //endif_i2cslaveMsgbox_dataout

    if (i2cslaveMsgbox_mutex)
    {
      if (testMsgbox_mutex())
      {
         SetStatus(TEST_FAILED);
         ErrPrintf("i2cslave::Msgbox_mutex test failed\n");
         return;
      }

      InfoPrintf("testMsgbox_mutex() Done !\n");
    } //endif_i2cslaveMsgbox_mutex

    InfoPrintf("I2cslave test complete\n");
    SetStatus(TEST_SUCCEEDED);
    getStateReport()->crcCheckPassedGold();

    ch->WaitForIdle();
}//end_Run

//sanity check, read therm_priv registers default values
int i2cslave_fermi::checkSanity()
{
  InfoPrintf("checkSanity() starting ....\n");

  UINT32   reg_value;
  UINT32   temp_value;

// read I2C slave adress
     if (lwgpu->GetRegNum("LW_THERM_I2C_ADDR",&reg_value))
     { ErrPrintf("reg LW_THERM_I2C_ADDR did not exist!\n"); return (1);}
     InfoPrintf("I2C_Addr number 0x%08x\n",reg_value);

     if (lwgpu->GetRegFldNum("LW_THERM_I2C_ADDR","", &reg_value))
     { ErrPrintf("reg LW_THERM_I2C_ADDR value cannot be read!\n"); return (1);}
     InfoPrintf("I2C_Addr default value 0x%08x\n",reg_value);

     if (lwgpu->GetRegFldNum("LW_THERM_I2C_ADDR","_3",&reg_value))
     { ErrPrintf("reg LW_THERM_I2C_ADDR3 value cannot be read!\n"); return (1);}
     InfoPrintf("I2C_Addr_3 default value 0x%08x\n",reg_value);

    InfoPrintf("I2cslave debug_i'm here\n");

     temp_value = 0xba;
     if (lwgpu->SetRegFldNum("LW_THERM_I2C_ADDR","_3",temp_value))
     { ErrPrintf("set LW_THERM_I2C_ADDR3 value cannot be changed!\n"); return (1);}
     InfoPrintf(" set I2C_Addr value 0x%08x\n",temp_value);

     if (lwgpu->GetRegFldNum("LW_THERM_I2C_ADDR","", &reg_value))
     { ErrPrintf("reg LW_THERM_I2C_ADDR value cannot be read!\n"); return (1);}
     InfoPrintf("I2C_Addr default value 0x%08x\n",reg_value);

  return(0);
}// end_checkSanity

//Basic test, read i2c slave registers default values
int i2cslave_fermi::testBasic()
{
  InfoPrintf("checkSanity() starting ....\n");

  UINT32   expect_value;
  UINT32   i2c_rdat;

     Platform::EscapeWrite("drv_opt_i2cs_addr_sel", 0, 2, 0x0);
     lwgpu->GetRegFldNum("LW_THERM_I2CS_SENSOR_TSCFG","_BANDGAP_VOLT",  &expect_value);
     i2cRdByte(DEVICE_ADDR_0_FERMI,0x32,&i2c_rdat,0);
     InfoPrintf("I2C_Addr DEV_ADDR_0  read back value 0x%08x\n",i2c_rdat);
     if ( i2c_rdat != expect_value) {
        ErrPrintf("DEV_ADDR_0 -- read data mismatch, expect 0x%08x , got 0x%08x\n", expect_value,i2c_rdat);
        return (1);
     }

     Platform::EscapeWrite("drv_opt_i2cs_addr_sel", 0, 2, 0x1);
     lwgpu->GetRegFldNum("LW_THERM_I2CS_SENSOR_TSCFG","_BANDGAP_VOLT",  &expect_value);
     i2cRdByte(DEVICE_ADDR_1_FERMI,0x32,&i2c_rdat,0);
     InfoPrintf("I2C_Addr DEVICE_ADDR_1_FERMI  read back value 0x%08x\n",i2c_rdat);
     if ( i2c_rdat != expect_value) {
        ErrPrintf("DEV_ADDR_1 -- read data mismatch, expect 0x%08x , got 0x%08x\n", expect_value,i2c_rdat);
        return (1);
     }

     Platform::EscapeWrite("drv_opt_i2cs_addr_sel", 0, 2, 0x2);
     lwgpu->GetRegFldNum("LW_THERM_I2CS_SENSOR_TSCFG","_BANDGAP_VOLT",  &expect_value);
     i2cRdByte(DEVICE_ADDR_2_FERMI,0x32,&i2c_rdat,0);
     InfoPrintf("I2C_Addr DEV_ADDR_2  read back value 0x%08x\n",i2c_rdat);
     if ( i2c_rdat != expect_value) {
        ErrPrintf("DEV_ADDR_2 -- read data mismatch, expect 0x%08x , got 0x%08x\n", expect_value,i2c_rdat);
        return (1);
     }

     Platform::EscapeWrite("drv_opt_i2cs_addr_sel", 0, 2, 0x3);
     lwgpu->GetRegFldNum("LW_THERM_I2CS_SENSOR_TSCFG","_BANDGAP_VOLT",  &expect_value);
     i2cRdByte(DEVICE_ADDR_3_FERMI,0x32,&i2c_rdat,0);
     InfoPrintf("I2C_Addr DEVICE_ADDR_3_FERMI  read back value 0x%08x\n",i2c_rdat);
     if ( i2c_rdat != expect_value) {
        ErrPrintf("DEV_ADDR_3 -- read data mismatch, expect 0x%08x , got 0x%08x\n", expect_value,i2c_rdat);
        return (1);
     }

  Platform::EscapeWrite("drv_opt_i2cs_addr_sel", 0, 2, 0x0);

  return(0);
}// end_checkSanity

//Basic test, read i2c slave registers default values
int i2cslave_fermi::testscratch()
{
  InfoPrintf("testscratch() starting ....\n");

  UINT32   expect_value;
//  UINT32   i2c_rdat;

     lwgpu->SetRegFldNum("LW_THERM_I2C_SCRATCH0_ADDR","",  0x800000a0);
     // == Write from priv, read from I2C ==
     lwgpu->SetRegFldNum("LW_THERM_I2C_SCRATCH_MUTEX(0)","",  0x01010101);
     lwgpu->SetRegFldNum("LW_THERM_I2C_SCRATCH_INDEX0","",   0x1f);
     lwgpu->SetRegFldNum("LW_THERM_I2C_SCRATCH_DATA0","",    0x12345678);
     lwgpu->SetRegFldNum("LW_THERM_I2C_SCRATCH_STATE0","",   0x80000000);
     //trigger task by mailbox0
     lwgpu->SetRegFldNum("LW_PPWR_PMU_MAILBOX(0)","",   0x32343234);
     //
     while(1) {
      lwgpu->GetRegFldNum("LW_PPWR_PMU_MAILBOX(1)","",  &expect_value);
      if(expect_value!=0x32343235) {
       InfoPrintf("waiting 1...\n");
      } else {
       break;
      }
     }
     // == Write from I2C, read from PRIV ==
//     InfoPrintf("I2C_Addr DEV_ADDR_0  read back value 0x%08x\n",i2c_rdat);
//     if ( i2c_rdat != expect_value) {
//        ErrPrintf("DEV_ADDR_0 -- read data mismatch, expect 0x%08x , got 0x%08x\n", expect_value,i2c_rdat);
//        return (1);
//     }
     lwgpu->SetRegFldNum("LW_THERM_I2C_SCRATCH_INDEX0","",   0x00030000);
     lwgpu->SetRegFldNum("LW_THERM_I2C_SCRATCH_STATE0","",    0x80000000);
     lwgpu->SetRegFldNum("LW_THERM_I2C_SCRATCH_STATE0","",    0x80000000);
     //trigger task by mailbox0
     lwgpu->SetRegFldNum("LW_PPWR_PMU_MAILBOX(0)","",   0x32343236);
     while(1) {
      lwgpu->GetRegFldNum("LW_PPWR_PMU_MAILBOX(1)","",  &expect_value);
      if(expect_value!=0x32343237) {
       InfoPrintf("waiting 1...\n");
      } else {
       break;
      }
     }
     // Read from priv
     lwgpu->SetRegFldNum("LW_THERM_I2C_SCRATCH_INDEX0","",   0x1);
     lwgpu->GetRegFldNum("LW_THERM_I2C_SCRATCH_DATA0","",  &expect_value);
     if ( expect_value != 0x87654321)
       ErrPrintf("read data mismatch, got 0x%08x\n", expect_value);
//     InfoPrintf("I2C_Addr DEVICE_ADDR_1_FERMI  read back value 0x%08x\n",i2c_rdat);
//     if ( i2c_rdat != expect_value) {
//        ErrPrintf("DEV_ADDR_1 -- read data mismatch, expect 0x%08x , got 0x%08x\n", expect_value,i2c_rdat);
//        return (1);
//     }

  return(0);
}// end testscratch

int i2cslave_fermi::i2cRdByte( int i2c_dev,  int i2c_adr, UINT32 *i2c_rdat,int use32)
{
  UINT32     i2c_rdat_value = 0;
  UINT32   sm_cmd_done ;
  UINT32   sm_no_ack;
  int      num_try;

  Platform:: EscapeWrite ("sm_dev_addr", 0, 7, i2c_dev);
  Platform:: EscapeWrite ("sm_dev_cmd", 0, 8, i2c_adr);

  if (use32 == 1) {
     InfoPrintf("i2cRdWord() - %08x starting ....\n",i2c_adr);
     Platform:: EscapeWrite ("sm_ctrl_cmd", 0, 4, READ_WORD);
  } else {
     InfoPrintf("i2cRdByte() - %08x starting ....\n",i2c_adr);
     Platform:: EscapeWrite ("sm_ctrl_cmd", 0, 4, READ_BYTE);}
  Platform:: EscapeWrite ("sm_ctrl_we", 0, 1, 1);

  Platform::Delay(2); //2us delay
  Platform:: EscapeWrite ("sm_ctrl_we", 0, 1, 0);

  num_try = 1;
  Platform::Delay(40);
  Platform:: EscapeRead("sm_done", 0, 1, &sm_cmd_done);
  Platform:: EscapeRead("sm_no_ack", 0, 1, &sm_no_ack);
  InfoPrintf("BESTCmd I2C cmd sm_done :%08x -- try %d  \n",sm_cmd_done & 0x1,num_try);

  while((num_try < 6) && ( (sm_cmd_done & 0x1) == 0x0)) {
     Platform::Delay(30);
     num_try = num_try +1;
     Platform:: EscapeRead("sm_done", 0, 1, &sm_cmd_done);
     Platform:: EscapeRead("sm_no_ack", 0, 1, &sm_no_ack);
     InfoPrintf("BESTCmd.v I2C cmd sm_done :%08x -- try %d \n",sm_cmd_done & 0x1,num_try);

     int i;
     UINT32     reg_value;
     for (i=0 ; i< 0; i++){  //disabled
         if (lwgpu->GetRegFldNum("LW_THERM_I2CS_SENSOR_20","", &reg_value))
         { ErrPrintf("reg LW_THERM_I2CS_SENSOR_20 value cannot be read!\n");return (1);}
         InfoPrintf("Reg LW_THERM_I2CS_SENSOR_20 -- 0x%32x\n", reg_value);
         Platform::Delay(10);
     }

  }

  if ( (sm_cmd_done & 0x1) ==0x1)
  {
    if ( (sm_no_ack & 0x1) == 0x1)
    {
       ErrPrintf("I2C Rd_byte NO ACK : adr=%08x data=%08x \n",i2c_adr,i2c_rdat_value);return (1);
    } else {
       Platform:: EscapeRead("sm_rdata", 0, 32, &i2c_rdat_value);
       InfoPrintf("I2C Rd_byte Done : adr=%08x data=%08x \n",i2c_adr,i2c_rdat_value);
    }
  } else {
   ErrPrintf("I2C Rd_byte : TimeOut\n"); return (1);}

  *i2c_rdat = i2c_rdat_value;
  Platform:: EscapeWrite ("sm_done_w", 0, 1, 1);
  Platform::Delay(1);
  Platform:: EscapeWrite ("sm_done_w", 0, 1, 0);
  return(0);
}// end_i2cRdByte

int i2cslave_fermi::i2cWrByte( int i2c_dev,  int i2c_adr, UINT32 i2c_wdat,int use32)
{
  UINT32   sm_cmd_done ;
  UINT32   sm_no_ack;
  int      num_try;

  Platform:: EscapeWrite ("sm_dev_addr", 0, 7, i2c_dev);
  Platform:: EscapeWrite ("sm_dev_cmd",  0, 8, i2c_adr);
  if (use32 == 1) {
     InfoPrintf("i2cWrWord() - %08x starting ....\n",i2c_adr);
     Platform:: EscapeWrite ("sm_ctrl_cmd", 0, 4, WRITE_WORD);
  } else {
     InfoPrintf("i2cWrByte() - %08x starting ....\n",i2c_adr);
     Platform:: EscapeWrite ("sm_ctrl_cmd", 0, 4, WRITE_BYTE);}
  Platform:: EscapeWrite ("sm_wdata", 0, 32, i2c_wdat);
  Platform:: EscapeWrite ("sm_data_we", 0, 1, 1);
  Platform:: EscapeWrite ("sm_ctrl_we", 0, 1, 1);

  Platform::Delay(2); //2us delay
  Platform:: EscapeWrite ("sm_data_we", 0, 1, 0);
  Platform:: EscapeWrite ("sm_ctrl_we", 0, 1, 0);

  Platform::Delay(20); //2us delay

  num_try = 1;
  Platform:: EscapeRead("sm_done", 0, 1, &sm_cmd_done);
  Platform:: EscapeRead("sm_no_ack", 0, 1, &sm_no_ack);
  InfoPrintf("BESTCmd I2C cmd sm_done : %08x -- try %d \n",sm_cmd_done & 0x1 ,num_try);

  while((num_try < 6) && ( (sm_cmd_done& 0x1) == 0x0)) {
     Platform::Delay(30);
     Platform:: EscapeRead("sm_done", 0, 1, &sm_cmd_done);
     Platform:: EscapeRead("sm_no_ack", 0, 1, &sm_no_ack);
     num_try = num_try +1;
     InfoPrintf("BESTCmd I2C cmd sm_done : %08x -- try %d \n",sm_cmd_done & 0x1 ,num_try);

  int i;
  UINT32     reg_value;
        for (i=0 ; i< 0; i++){ //disabled
         if (lwgpu->GetRegFldNum("LW_THERM_I2CS_SENSOR_20","", &reg_value))
            { ErrPrintf("reg LW_THERM_I2CS_SENSOR_20 value cannot be read!\n");return (1);}
              InfoPrintf("Reg LW_THERM_I2CS_SENSOR_20 -- 0x%32x\n", reg_value);
              Platform::Delay(10);
        }

  }

  if ( (sm_cmd_done & 0x1) ==0x1)
  {
    if (sm_no_ack == 0x1)
    {
       ErrPrintf("I2C Wr_byte NO ACK : adr=%08x data=%08x \n",i2c_adr,i2c_wdat);return (1);
    } else {
       InfoPrintf("I2C Wr_byte Done : adr=%08x data=%08x \n",i2c_adr,i2c_wdat);
    }
  } else {
   ErrPrintf("I2C Wr_byte : TimeOut\n"); return (1);}

   Platform:: EscapeWrite ("sm_done_w", 0, 1, 1);
   Platform::Delay(1);
   Platform:: EscapeWrite ("sm_done_w", 0, 1, 0);

   Platform::Delay(20); //2us delay
  return(0);
}// end_i2cWrByte

int i2cslave_fermi::i2cSetup()
{
  InfoPrintf("i2cSetup() starting ....\n");

  UINT32   reg_value;
//  UINT32   temp_value;

// change hostclk_src_select
//     temp_value = 0; // 0- works, 1-does not work 3-
  //   if (lwgpu->SetRegFldNum("LW_PHOST_CLKSRC", "_HOSTCLK_FULL_SPEED", temp_value))
 //    { ErrPrintf("couldn't write LW_PHOST_CLKSRC\n"); return 1; }

//     if (lwgpu->SetRegFldDef("LW_PHOST_CLKSRC", "_HOSTCLK_AUTO", "DISABLE"))
//     { ErrPrintf("couldn't write LW_PHOST_CLKSRC_HOSTCLK_AUTO\n"); return 1; }
//     InfoPrintf("Change HOSTCLK_AUTO_ENABLE to DISABLE value 0x%08x\n",0);

/*
     if (lwgpu->SetRegFldNum("LW_PHOST_CLKSRC", "_HOSTCLK", temp_value))
     { ErrPrintf("couldn't write LW_PHOST_CLKSRC\n"); return 1; }
     InfoPrintf("Change HOSTCLK value 0x%08x\n",temp_value);

     if (lwgpu->GetRegFldNum("LW_PHOST_CLKSRC","_HOSTCLK", &reg_value))
     { ErrPrintf("couldn't read LW_PHOST_CLKSRC\n"); return 1; }
     InfoPrintf("LW_PHOST_CLKSRC_HOSTCLK value 0x%08x\n",reg_value);
*/

/*
// Setup Spike Filter to lower value
     if (lwgpu->GetRegFldNum("LW_THERM_I2C_CFG","_SPIKE", &reg_value))
     { ErrPrintf("reg LW_THERM_I2C_CFG_SPIKE value cannot be read!\n");}
     InfoPrintf("I2C_CFG_SPIKE default value 0x%08x\n",reg_value);

     temp_value = 0x02;
     if (lwgpu->SetRegFldNum("LW_THERM_I2C_CFG","_SPIKE",temp_value))
     { ErrPrintf("set LW_THERM_I2C_CFG_SPIKE value cannot be changed!\n");}
     InfoPrintf(" set I2C_CFG_SPIKE value 0x%x\n",temp_value);

     if (lwgpu->GetRegFldNum("LW_THERM_I2C_CFG","_SPIKE", &reg_value))
     { ErrPrintf("reg LW_THERM_I2C_CFG_SPIKE value cannot be read!\n");}
     InfoPrintf("I2C_CFG_SPIKE default value 0x%08x\n",reg_value);

// Increade Setup time
     if (lwgpu->GetRegFldNum("LW_THERM_I2C_CFG","_SETUP", &reg_value))
     { ErrPrintf("reg LW_THERM_I2C_CFG_SETUP value cannot be read!\n");}
     InfoPrintf("I2C_CFG_SETUP default value 0x%08x\n",reg_value);

     temp_value = 0x04;
     if (lwgpu->SetRegFldNum("LW_THERM_I2C_CFG","_SETUP",temp_value))
     { ErrPrintf("set LW_THERM_I2C_CFG_SETUP value cannot be changed!\n");}
     InfoPrintf(" set I2C_CFG_SETUP value 0x%x\n",temp_value);

     if (lwgpu->GetRegFldNum("LW_THERM_I2C_CFG","_SETUP", &reg_value))
     { ErrPrintf("reg LW_THERM_I2C_CFG_SETUP value cannot be read!\n");}
     InfoPrintf("I2C_CFG_SETUP default value 0x%08x\n",reg_value);
 */

  Platform:: EscapeRead("opt_i2cs_addr_sel", 0, 2, &reg_value);

  return(0);
}// end_i2cSetup

int i2cslave_fermi::i2cPriEn( int i2c_dev)
{
  UINT32   reg_value;

  InfoPrintf("i2cPriEn() starting ....\n");

//  Platform:: EscapeWrite ("opt_i2cs_wide_en", 0, 1, 1);
  Platform:: EscapeWrite ("opt_i2cs_addr_sel", 0, 2, 0x0); //check Dev_Adr

  Platform:: EscapeRead("opt_i2cs_wide_en", 0, 1, &reg_value);
  InfoPrintf("BESTCmd.v opt_i2cs_wide_en :%01x  \n",reg_value);

  if (lwgpu->GetRegFldNum("LW_THERM_I2C_ADDR","", &reg_value))
  { ErrPrintf("reg LW_THERM_I2C_ADDR value cannot be read!\n");return (1);}
  InfoPrintf("I2C_Addr default value 0x%08x\n",reg_value);

  if (lwgpu->SetRegFldNum("LW_THERM_I2C_CFG","_PRIV_ENABLE",0x1))
  { ErrPrintf("set LW_THERM_I2C_CFG_PRIV_ENABLE cannot be changed!\n");}
  InfoPrintf(" set I2C_CFG_PRIV_ENABLE \n");

 // use the following to enable key
  i2cWrByte(i2c_dev,0x4C,0x68,0);
  i2cWrByte(i2c_dev,0x4D,0x74,0);
  i2cWrByte(i2c_dev,0x4E,0x76,0);
  i2cWrByte(i2c_dev,0x4F,0x6E,0);

  return(0);
}// end_i2PriEn

int i2cslave_fermi::testPriAccess(int i2c_dev)
{
  UINT32   reg_value;
  UINT32   reg_wdat;
  const char   *manual_name;
  UINT32 pri_rdat1, pri_rdat2;

  InfoPrintf("testPriAccess() starting ....\n");
 // use the following to enable key
  i2cPriEn(i2c_dev);

  //Read from Pri
  manual_name = "LW_THERM_I2CS_SENSOR_FE";
  if (lwgpu->GetRegNum(manual_name,&reg_value))
  { ErrPrintf("Reg %s did not exist!\n",manual_name);return (1);}
  InfoPrintf("Reg %s number 0x%32x\n",manual_name, reg_value);

  setupPriRead(i2c_dev,reg_value);
  lwgpu->GetRegFldNum("LW_THERM_I2CS_SENSOR_FE","", &pri_rdat1);
  pri_rdat2 = setupPriRead(DEVICE_ADDR_0_FERMI,reg_value);
  if ( pri_rdat1 != pri_rdat2 )  {
    ErrPrintf("Priv read value from i2c is incorrect. expected %32x,  got %32x", pri_rdat1, pri_rdat2 );
    SetStatus(TEST_FAILED);
    return (1);
  }
  else
    InfoPrintf("Priv read value from i2c is correct. expected %32x,  got %32x", pri_rdat1, pri_rdat2 );

  //Write from Pri
  manual_name = "LW_THERM_I2CS_SENSOR_03";
  if (lwgpu->GetRegNum(manual_name,&reg_value))
  { ErrPrintf("Reg %s did not exist!\n",manual_name);return (1);}
  InfoPrintf("Reg %s number 0x%32x\n",manual_name, reg_value);

  reg_wdat =0x12;
  if (setupPriWrite(i2c_dev,reg_value,reg_wdat)) return (1);

  //Read from Pri
  manual_name = "LW_THERM_I2CS_SENSOR_03";
  if (lwgpu->GetRegNum(manual_name,&reg_value))
  { ErrPrintf("Reg %s did not exist!\n",manual_name);return (1);}
  InfoPrintf("Reg %s number 0x%32x\n",manual_name, reg_value);

  setupPriRead(i2c_dev,reg_value);

  return(0);
}// end_testPriAccess

int i2cslave_fermi::setupPriRead(int i2c_dev, UINT32 pri_adr)
{
    UINT32   i2c_rdat;
    int    i2c_pri_ack ;
    int    num_try;

  InfoPrintf("Setup Read from Priv Address 0x%32x\n",pri_adr);

 // use the following to setup adr & dat
  i2cWrByte(i2c_dev,0x50,pri_adr,1);
  i2cWrByte(i2c_dev,0x48,0x1f,0);

  num_try = 1;
  Platform::Delay(50);
  i2cRdByte(i2c_dev,0x48,&i2c_rdat,0);
  InfoPrintf("I2C Rd_byte- pri_csr:%08x -- try %d \n",i2c_rdat,num_try);
  i2c_pri_ack = ((i2c_rdat & 0x80) ==0x80);

  while((num_try < 4) && (i2c_pri_ack ==0x0)) {
     Platform::Delay(40);
     num_try = num_try +1;
     i2cRdByte(i2c_dev,0x48,&i2c_rdat,0);
     InfoPrintf("I2C Rd_byte- pri_csr:%08x  -- try %d \n",i2c_rdat,num_try);
     i2c_pri_ack = ((i2c_rdat & 0x80) ==0x80);
  }

  if (i2c_pri_ack  ==0x1)
  {
    i2cRdByte(i2c_dev,0x54,&i2c_rdat,1);
    InfoPrintf("I2c Pri-Read ACKed : adr=%32x data=%32x \n",pri_adr,i2c_rdat);
  } else {
   ErrPrintf("I2c Pri-Read : TimeOut\n"); return (1);}

  return(i2c_rdat);
}// end_setupPriRead

int i2cslave_fermi::setupPriWrite(int i2c_dev, UINT32 pri_adr,UINT32 pri_wdat)
{
  int    i2c_pri_ack ;
  int    num_try;
  UINT32   i2c_rdat;

  InfoPrintf("Setup Write to Priv Adr 0x%32x Dat 0x%32x\n",pri_adr,pri_wdat);

 // use the following to enable key
  i2cWrByte(i2c_dev,0x54,pri_wdat,1);
  i2cWrByte(i2c_dev,0x50,pri_adr,1);
  i2cWrByte(i2c_dev,0x48,0x0f,0);

  Platform::Delay(50); //2us delay
  num_try = 1;
  i2cRdByte(i2c_dev,0x48,&i2c_rdat,0);
  InfoPrintf("I2C Rd_byte- pri_csr:%08x -- try %d \n",i2c_rdat,num_try);
  i2c_pri_ack = ((i2c_rdat & 0x80) ==0x80);

  while((num_try < 4) && (i2c_pri_ack ==0x0)) {
     Platform::Delay(40); //2us delay
     i2cRdByte(i2c_dev,0x48,&i2c_rdat,0);
     num_try = num_try +1;
     InfoPrintf("I2C Rd_byte- pri_csr:%08x  -- try %d \n",i2c_rdat,num_try);
     i2c_pri_ack = ((i2c_rdat & 0x80) ==0x80);
  }

  if (i2c_pri_ack  ==0x1)
  {
    InfoPrintf("I2c Pri-Write ACKed : adr=%32x data=%32x \n",pri_adr,pri_wdat);
    i2c_rdat = setupPriRead(i2c_dev,pri_adr);
    if (i2c_rdat != pri_wdat) {
          ErrPrintf("Priv read back value from i2c is incorrect. expected %32x,  got %32x", pri_wdat, i2c_rdat);
          SetStatus(TEST_FAILED);
          return (1);
    }
    else
          InfoPrintf("Priv read back value from i2c is correct. expected %32x,  got %32x", pri_wdat, i2c_rdat);

  } else {
   ErrPrintf("I2c Pri-Write : TimeOut\n"); return (1);}

  return(0);
}// end_setupPriWrite

int i2cslave_fermi::i2cPreArp( )
{
  UINT32   arp_done ;
  UINT32   arp_err;
  int      num_try;

  InfoPrintf("i2c - Pre_Arp starting\n");
  Platform:: EscapeWrite ("sm_arp_cmd", 0, 4, PRE_ARP);
  Platform:: EscapeWrite ("sm_arp_ctrl_we", 0, 1, 1);

  Platform::Delay(2); //2us delay
  Platform:: EscapeWrite ("sm_arp_ctrl_we", 0, 1, 0);

  num_try = 1;
  Platform:: EscapeRead("arp_done", 0, 1, &arp_done);
  Platform:: EscapeRead("arp_err", 0, 1, &arp_err);
  InfoPrintf("I2C Pre_arp  done :%08x arp_err =0x%x -- try %d  \n",arp_done,arp_err,num_try);

  while((num_try < 6) && ( (arp_done & 0x1) == 0x0) ) {
     Platform::Delay(60);
     Platform:: EscapeRead("arp_done", 0, 1, &arp_done);
     Platform:: EscapeRead("arp_err", 0, 1, &arp_err);
     num_try = num_try +1;
     InfoPrintf(" tried %d times \n",num_try);
  }

  if ( (arp_done & 0x1) ==0x1)  {
    if ( (arp_err & 0x1) == 0x1)    {
       InfoPrintf("I2C Pre_arp NO ACK\n");
    } else {
       InfoPrintf("I2C Pre_arp  done :%08x -- try %d \n",arp_done,num_try);}
  } else {
   ErrPrintf("I2C Pre_Arp : TimeOut\n"); return (1);}

  Platform:: EscapeWrite ("arp_done", 0, 1, 0);

  return(0);
}// end_i2cPreArp

int i2cslave_fermi::i2cRstArp( )
{
  UINT32   arp_done ;
  UINT32   arp_err;
  int      num_try;

  InfoPrintf("i2c - Pre_Arp starting\n");
  Platform:: EscapeWrite ("sm_arp_cmd", 0, 4, RST_ARP);
  Platform:: EscapeWrite ("sm_arp_ctrl_we", 0, 1, 1);

  Platform::Delay(2); //2us delay
  Platform:: EscapeWrite ("sm_arp_ctrl_we", 0, 1, 0);

  num_try = 1;
  Platform:: EscapeRead("arp_done", 0, 1, &arp_done);
  Platform:: EscapeRead("arp_err", 0, 1, &arp_err);
  InfoPrintf("I2C Pre_arp  done :%08x -- try %d  \n",arp_done,num_try);

  while((num_try < 6) && ( (arp_done & 0x1) == 0x0)) {
     Platform::Delay(60);
     Platform:: EscapeRead("arp_done", 0, 1, &arp_done);
     Platform:: EscapeRead("arp_err", 0, 1, &arp_err);
     num_try = num_try +1; }

  if ( (arp_done & 0x1) ==0x1)  {
    if ( (arp_err & 0x1) == 0x1)    {
       InfoPrintf("I2C Rst_arp NO ACK\n");
    } else {
       InfoPrintf("I2C Rst_arp  done :%08x -- try %d \n",arp_done,num_try);}
  } else {
   ErrPrintf("I2C Rst_Arp : TimeOut\n"); return (1);}

  Platform:: EscapeWrite ("arp_done", 0, 1, 0);
  return(0);
}// end_i2cRstArp

int i2cslave_fermi::i2cGetUid(UINT32 *get_id_3,UINT32 *get_id_2,UINT32 *get_id_1,UINT32 *get_id_0)
{
  UINT32     arp_rdat_value3 = 0, arp_rdat_value2 = 0, arp_rdat_value1 = 0, arp_rdat_value0 = 0;
  UINT32   arp_done ;
  UINT32   arp_err;
  int      num_try;

  InfoPrintf("i2c - Get_id starting\n");
  Platform:: EscapeWrite ("sm_arp_cmd", 0, 4, GET_UID);
  Platform:: EscapeWrite ("sm_arp_ctrl_we", 0, 1, 1);

  Platform::Delay(2); //2us delay
  Platform:: EscapeWrite ("sm_arp_ctrl_we", 0, 1, 0);

  num_try = 1;
  Platform::Delay(20);
  Platform:: EscapeRead("arp_done", 0, 1, &arp_done);
  Platform:: EscapeRead("arp_err", 0, 1, &arp_err);
  InfoPrintf("I2C Get_Uid  done :%08x -- try %d  \n",arp_done,num_try);

  while((num_try < 6) && (arp_done == 0x0)) {
     Platform::Delay(20);
     Platform:: EscapeRead("arp_done", 0, 1, &arp_done);
     Platform:: EscapeRead("arp_err", 0, 1, &arp_err);
     num_try = num_try +1; }

  if (arp_done ==0x1)  {
    if (arp_err == 0x1)    {
       InfoPrintf("I2C Get_Uid NO ACK\n");
    } else {
       Platform:: EscapeRead("sm_arp_rdata3", 0, 32, &arp_rdat_value3);
       Platform:: EscapeRead("sm_arp_rdata2", 0, 32, &arp_rdat_value2);
       Platform:: EscapeRead("sm_arp_rdata1", 0, 32, &arp_rdat_value1);
       Platform:: EscapeRead("sm_arp_rdata0", 0, 32, &arp_rdat_value0);
       InfoPrintf("I2C Get_Uid done :%32x %32x %32x %32x -- try %d \n",arp_rdat_value3,arp_rdat_value2,arp_rdat_value1,arp_rdat_value0,num_try);}
  } else {
   ErrPrintf("I2C Get_Uid : TimeOut\n"); return (1);}

   Platform:: EscapeWrite ("arp_done", 0, 1, 0);

  *get_id_3 = arp_rdat_value3;
  *get_id_2 = arp_rdat_value2;
  *get_id_1 = arp_rdat_value1;
  *get_id_0 = arp_rdat_value0;

  return(0);
}// end_i2cGetUid

int i2cslave_fermi::i2cSetUid(UINT32 set_id_3,UINT32 set_id_2,UINT32 set_id_1, UINT32 set_id_0,UINT32 arp_adr  )
{
  UINT32   arp_done ;
  UINT32   arp_err;
  int      num_try;

  InfoPrintf("i2c - Set_id starting\n");

  Platform:: EscapeWrite ("sm_arp_wdata3", 0, 32, set_id_3);
  Platform:: EscapeWrite ("sm_arp_wdata2", 0, 32, set_id_2);
  Platform:: EscapeWrite ("sm_arp_wdata1", 0, 32, set_id_1);
  Platform:: EscapeWrite ("sm_arp_wdata0", 0, 32, set_id_0);
  Platform:: EscapeWrite ("sm_arp_set_adr", 0, 8, arp_adr);
  Platform:: EscapeWrite ("sm_arp_data_we", 0, 1, 1);

  Platform::Delay(2); //2us delay
  Platform:: EscapeWrite ("sm_arp_cmd", 0, 4, SET_UID);
  Platform:: EscapeWrite ("sm_arp_ctrl_we", 0, 1, 1);
  Platform:: EscapeWrite ("sm_arp_data_we", 0, 1, 1);

  Platform::Delay(2); //2us delay
  Platform:: EscapeWrite ("sm_arp_data_we", 0, 1, 0);
  Platform:: EscapeWrite ("sm_arp_ctrl_we", 0, 1, 0);

  num_try = 1;
  Platform::Delay(20);
  Platform:: EscapeRead("arp_done", 0, 1, &arp_done);
  Platform:: EscapeRead("arp_err", 0, 1, &arp_err);
  InfoPrintf("I2C Set_Uid  done :%08x -- try %d  \n",arp_done,num_try);

  while((num_try < 6) && (arp_done == 0x0)) {
     Platform::Delay(20);
     Platform:: EscapeRead("arp_done", 0, 1, &arp_done);
     Platform:: EscapeRead("arp_err", 0, 1, &arp_err);
     num_try = num_try +1; }

  if (arp_done ==0x1)  {
    if (arp_err == 0x1)    {
       InfoPrintf("I2C Set_Uid NO ACK\n");
    } else {
//       Platform:: EscapeRead("sm_arp_rdata", 0, 64, &get_id);
      InfoPrintf("I2C Set_Uid done  -- try %d \n",num_try);
    }
  } else {
   ErrPrintf("I2C Set_Uid : TimeOut\n"); return (1);}

   Platform:: EscapeWrite ("arp_done", 0, 1, 0);

  return(0);
}// end_i2cSetUid

//Check Gpio, program Gpio_11, Gpio_12, Gpio_13 --- for Dell
int i2cslave_fermi::testCheckGpio()
{
  UINT32   reg_value;
  UINT32   pri_wdat;

  UINT32   i2c_rdat;
  int    i2c_pri_ack ;
  int    num_try;
  UINT32   data_mask;

  UINT32   fuse_done;

  InfoPrintf("testCheckGpio() starting ....\n");

//  Platform:: EscapeWrite ("opt_i2cs_addr_sel", 0, 2, 0x0); //check Dev_Adr

  Platform:: EscapeRead("opt_i2cs_wide_en", 0, 1, &reg_value);
  InfoPrintf("BESTCmd.v opt_i2cs_wide_en :%01x  \n",reg_value);

  //wait until fuse_sense_done
  num_try = 1;
  Platform:: EscapeRead("fuse2all_fuse_sense_done", 0, 1, &fuse_done);
  InfoPrintf("BESTCmd Fuse_sense_done :%08x -- try %d  \n",fuse_done,num_try);
  while((num_try < 6) && (fuse_done == 0x0)) {
    Platform::Delay(40);
    Platform:: EscapeRead("fuse2all_fuse_sense_done", 0, 1, &fuse_done);
    InfoPrintf("BESTCmd.v Fuse_sense_done :%08x -- try %d \n",fuse_done,num_try);
    num_try = num_try +1;
  }
  if (fuse_done == 0x0) {
    InfoPrintf("BestCmd.v Fuse_sense_done TIMEOUT \n");
  }

 // use the following to enable key
  i2cWrByte(DEVICE_ADDR_0_FERMI,0x4C,0x68,0);
  i2cWrByte(DEVICE_ADDR_0_FERMI,0x4D,0x74,0);
  i2cWrByte(DEVICE_ADDR_0_FERMI,0x4E,0x76,0);
  i2cWrByte(DEVICE_ADDR_0_FERMI,0x4F,0x6E,0);

   // use the following to setup adr & dat
  i2cWrByte(DEVICE_ADDR_0_FERMI,0x50,0x0000E100,1);
  i2cWrByte(DEVICE_ADDR_0_FERMI,0x48,0x1F,0);
 // i2cPollCSR(); //fix_me

  Platform::Delay(50); //2us delay
  num_try = 1;
  i2cRdByte(DEVICE_ADDR_0_FERMI,0x48,&i2c_rdat,0);
  InfoPrintf("I2C Rd_byte- pri_csr:%08x -- try %d \n",i2c_rdat,num_try);
  i2c_pri_ack = ((i2c_rdat & 0x80) ==0x80);

  while((num_try < 4) && (i2c_pri_ack ==0x0)) {
     num_try = num_try +1;
     Platform::Delay(40);
     i2cRdByte(DEVICE_ADDR_0_FERMI,0x48,&i2c_rdat,0);
     InfoPrintf("I2C Rd_byte- pri_csr:%08x  -- try %d \n",i2c_rdat,num_try);
     i2c_pri_ack = ((i2c_rdat & 0x80) ==0x80);
  }

  if (i2c_pri_ack  ==0x1)  {
    i2cRdByte(DEVICE_ADDR_0_FERMI,0x54,&i2c_rdat,1);
    InfoPrintf("I2c Pri-Read ACKed : adr=%32x data=%32x \n",0x0000E100,i2c_rdat);
  } else {
   ErrPrintf("I2c Pri-Read : TimeOut\n"); return (1);}

 // Check GPIO_11 -bit[11] should be 0, if bit[11]==1, set it to 0
 data_mask = 0x00000800;
 if (i2c_rdat & data_mask) {
    InfoPrintf("Setting GPIO_11 to 0 \n");
    pri_wdat = i2c_rdat & ~data_mask;
    i2cWrByte(DEVICE_ADDR_0_FERMI,0x50,0x0000E100,1);
    i2cWrByte(DEVICE_ADDR_0_FERMI,0x54,pri_wdat,1);
    i2cWrByte(DEVICE_ADDR_0_FERMI,0x48,0x0F,0);
    // i2cPollCSR(); //fix_me

    Platform::Delay(50); //2us delay
    num_try = 1;
    i2cRdByte(DEVICE_ADDR_0_FERMI,0x48,&i2c_rdat,0);
    InfoPrintf("I2C Rd_byte- pri_csr:%08x -- try %d \n",i2c_rdat,num_try);
    i2c_pri_ack = ((i2c_rdat & 0x80) ==0x80);

    while((num_try < 4) && (i2c_pri_ack ==0x0)) {
       Platform::Delay(40);
       i2cRdByte(DEVICE_ADDR_0_FERMI,0x48,&i2c_rdat,0);
       num_try = num_try +1;
       InfoPrintf("I2C Rd_byte- pri_csr:%08x  -- try %d \n",i2c_rdat,num_try);
       i2c_pri_ack = ((i2c_rdat & 0x80) ==0x80);
    }

    if (i2c_pri_ack  ==0x1){
      InfoPrintf("I2c Pri-Write ACKed : adr=%32x data=%32x \n",0x0000E100,pri_wdat);
      setupPriRead(DEVICE_ADDR_0_FERMI,0x0000E100);
    } else {
      ErrPrintf("I2c Pri-Write : TimeOut\n"); return (1);}
 } else {
    InfoPrintf("GPIO_11 is set at 0 \n");}

 // Check GPIO_13 -bit[13] should be 0, if bit[13]==1, set it to 0

/////---------Set GPIO_13 as Input ------------(set Bit[21] of adr0x0000E108 to 0)
  InfoPrintf("Checking GPIO_13 state...... \n");
  i2cWrByte(DEVICE_ADDR_0_FERMI,0x50,0x0000E108,1);
  i2cWrByte(DEVICE_ADDR_0_FERMI,0x48,0x1F,0);
 // i2cPollCSR(); //fix_me

   Platform::Delay(50); //2us delay
  num_try = 1;
  i2cRdByte(DEVICE_ADDR_0_FERMI,0x48,&i2c_rdat,0);
  InfoPrintf("I2C Rd_byte- pri_csr:%08x -- try %d \n",i2c_rdat,num_try);
  i2c_pri_ack = ((i2c_rdat & 0x80) ==0x80);

  while((num_try < 4) && (i2c_pri_ack ==0x0)) {
     Platform::Delay(40);
     num_try = num_try +1;
     i2cRdByte(DEVICE_ADDR_0_FERMI,0x48,&i2c_rdat,0);
     InfoPrintf("I2C Rd_byte- pri_csr:%08x  -- try %d \n",i2c_rdat,num_try);
     i2c_pri_ack = ((i2c_rdat & 0x80) ==0x80);
  }

  if (i2c_pri_ack  ==0x1)  {
    i2cRdByte(DEVICE_ADDR_0_FERMI,0x54,&i2c_rdat,1);
    InfoPrintf("I2c Pri-Read ACKed : adr=%32x data=%32x \n",0x0000E108,i2c_rdat);
  } else {
   ErrPrintf("I2c Pri-Read : TimeOut\n"); return (1);}

 data_mask = 0x00200000;
 InfoPrintf("GPIO_13 -bit[21] state : 0x%32x\n",i2c_rdat & data_mask);

 pri_wdat = i2c_rdat & ~data_mask;
 InfoPrintf("write data : 0x%32x\n",pri_wdat);

 data_mask = 0x00003000;
 pri_wdat = i2c_rdat | data_mask;
 InfoPrintf("write data : 0x%32x\n",pri_wdat);

 i2cWrByte(DEVICE_ADDR_0_FERMI,0x50,0x0000E108,1);
 i2cWrByte(DEVICE_ADDR_0_FERMI,0x54,pri_wdat,1);
 i2cWrByte(DEVICE_ADDR_0_FERMI,0x48,0x0F,0);
 // i2cPollCSR(); //fix_me

 Platform::Delay(50); //2us delay
 num_try = 1;
 i2cRdByte(DEVICE_ADDR_0_FERMI,0x48,&i2c_rdat,0);
 InfoPrintf("I2C Rd_byte- pri_csr:%08x -- try %d \n",i2c_rdat,num_try);
 i2c_pri_ack = ((i2c_rdat & 0x80) ==0x80);

 while((num_try < 4) && (i2c_pri_ack ==0x0)) {
     Platform::Delay(40);
     num_try = num_try +1;
     i2cRdByte(DEVICE_ADDR_0_FERMI,0x48,&i2c_rdat,0);
     InfoPrintf("I2C Rd_byte- pri_csr:%08x  -- try %d \n",i2c_rdat,num_try);
     i2c_pri_ack = ((i2c_rdat & 0x80) ==0x80);
}

 if (i2c_pri_ack  ==0x1) {
     InfoPrintf("I2c Pri-Write ACKed : adr=%32x data=%32x \n",0x0000E108,pri_wdat);
     setupPriRead(DEVICE_ADDR_0_FERMI,0x0000E108);
 } else {
    ErrPrintf("I2c Pri-Write : TimeOut\n"); return (1);}

 data_mask = 0x00400000;
 InfoPrintf("GPIO_13 state -bit[22] : 0x%32x\n",i2c_rdat & data_mask);

  return(0);

}//end_testCheckGpio

//Test clkdiv
int i2cslave_fermi::testClkdiv()
{
    InfoPrintf("testClkdiv() starting...\n");
    InfoPrintf("getting ref_count\n");
    UINT32 data, i, prev_count, ref_count;
    Platform::Delay(2);
    Platform::EscapeRead("tb_i2c_clk_ref_counter", 0x00, 32, &data);
    prev_count = data;
    Platform::Delay(10);
    Platform::EscapeRead("tb_i2c_clk_ref_counter", 0x00, 32, &data);
    ref_count = data - prev_count;
    InfoPrintf("ref_count is %d\n", ref_count);

    for (i=0; i<8; i++) {
        if (lwgpu->SetRegFldNum("LW_THERM_I2C_CLK_DIV", "_VAL", i)) {
          ErrPrintf("couldn't write LW_THERM_I2C_CLK_DIV\n"); return 1;
        }
        Platform::Delay(2);
        Platform::EscapeRead("tb_i2c_clk_div_counter", 0x00, 32, &data);
        prev_count = data;
        Platform::Delay(10);
        Platform::EscapeRead("tb_i2c_clk_div_counter", 0x00, 32, &data);
        if ( ref_count*1.05 < (data - prev_count)*(i+1) || ref_count*0.95 > (data - prev_count)*(i+1) ) {
          ErrPrintf("expected tb_i2c_clk_div_counter to be between %d , got %d for i2c_clk_div %d\n",
           ref_count/(i+1), (data-prev_count), (i+1));
          return 1;
        }
      }

    for (i=8; i>0; i--) {
        if (lwgpu->SetRegFldNum("LW_THERM_I2C_CLK_DIV", "_VAL", (i-1))) {
          ErrPrintf("couldn't write LW_THERM_I2C_CLK_DIV\n"); return 1;
        }
        Platform::Delay(2);
        Platform::EscapeRead("tb_i2c_clk_div_counter", 0x00, 32, &data);
        prev_count = data;
        Platform::Delay(10);
        Platform::EscapeRead("tb_i2c_clk_div_counter", 0x00, 32, &data);
        if ( ref_count*1.05 < (data - prev_count)*i || ref_count*0.95 > (data - prev_count)*i ) {
          ErrPrintf("expected tb_i2c_clk_div_counter to be between %d , got %d for i2c_clk_div %d\n",
           ref_count/i, (data-prev_count), i);
          return 1;
        }
      }

  return(0);
}// end_testClkdiv

int i2cslave_fermi::testMsgbox()
{
    if((m_arch & 0xff0) == 0x400){     //gf10x
    UINT32   reg_value;

    InfoPrintf("testMsgbox() starting\n");
    //step 1 pri write and read
    UINT32 smbus_dat_addr, smbus_cmd_addr;
    UINT32 smbus_wr_dat, smbus_wr_cmd;
    UINT32 smbus_rd_dat, smbus_rd_cmd;
    smbus_wr_dat = 0xbadbeef;
    smbus_wr_cmd = 0xbadcafe;

    InfoPrintf("pri write and read start ...\n");

    // MSGBOX_DATA
    if (lwgpu->GetRegNum("LW_THERM_I2CS_SENSOR_3C",&smbus_dat_addr))
    { ErrPrintf("reg LW_THERM_I2CS_SENSOR_3C did not exist!\n");return (1);}
    InfoPrintf("MSGBOX_DATA number 0x%08x\n",smbus_dat_addr);

    if (lwgpu->SetRegFldNum("LW_THERM_I2CS_SENSOR_3C","",smbus_wr_dat))
    { ErrPrintf("set LW_THERM_I2CS_SENSOR_3C value cannot be changed!\n");return (1);}
    InfoPrintf(" set MSGBOX_DATA value 0x%08x\n",smbus_wr_dat);

    if (lwgpu->GetRegFldNum("LW_THERM_I2CS_SENSOR_3C","",&smbus_rd_dat))
    { ErrPrintf("reg LW_THERM_I2CS_SENSOR_3C value cannot be read!\n");return (1);}
    InfoPrintf("read MSGBOX_DATA value 0x%08x\n",smbus_rd_dat);

    // MSGBOX_CMD
    if (lwgpu->GetRegNum("LW_THERM_I2CS_SENSOR_3D",&smbus_cmd_addr))
    { ErrPrintf("reg LW_THERM_I2CS_SENSOR_3D did not exist!\n");return (1);}
    InfoPrintf("MSGBOX_CMD number 0x%08x\n",smbus_cmd_addr);

    if (lwgpu->SetRegFldNum("LW_THERM_I2CS_SENSOR_3D","",smbus_wr_cmd))
    { ErrPrintf("set LW_THERM_I2CS_SENSOR_3D value cannot be changed!\n");return (1);}
    InfoPrintf(" set MSGBOX_CMD value 0x%08x\n",smbus_wr_cmd);

    if (lwgpu->GetRegFldNum("LW_THERM_I2CS_SENSOR_3D","",&smbus_rd_cmd))
    { ErrPrintf("reg LW_THERM_I2CS_SENSOR_3D value cannot be read!\n");return (1);}
    InfoPrintf("read MSGBOX_CMD value 0x%08x\n",smbus_rd_cmd);

    //step 2 smbus write and read
    UINT32 pri_rd_dat;
    UINT32 pri_rd_cmd;
    i2cPriEn(DEVICE_ADDR_0_FERMI);
    Platform:: EscapeRead("opt_i2cs_wide_en", 0, 1, &reg_value);
    InfoPrintf("BESTCmd.v opt_i2cs_wide_en :%01x  \n",reg_value);
    if ( (reg_value & 0xf) != 0) {
       ErrPrintf("opt_i2cs_wide_en should not be enabled for msgbox test!\n");
       return (1);
    }

    // smbus write msgbox reg
    InfoPrintf("smbus write msgbox reg start ...\n ");
    smbus_wr_dat = 0x12345678;
    i2cWrByte(DEVICE_ADDR_0_FERMI,0x58,smbus_wr_dat,1);
    Platform::Delay(30);
    lwgpu->GetRegFldNum("LW_THERM_I2CS_SENSOR_3C","",&pri_rd_dat);
    if (pri_rd_dat != smbus_wr_dat)
    { ErrPrintf("smbus box reg LW_THERM_I2CS_SENSOR_3C write Error!\n");return (1);}

    InfoPrintf("smbus write smbus cmd");
    smbus_wr_cmd = 0x56781234;
    i2cWrByte(DEVICE_ADDR_0_FERMI,0x5c,smbus_wr_cmd,1);
    Platform::Delay(30);
    lwgpu->GetRegFldNum("LW_THERM_I2CS_SENSOR_3D","",&pri_rd_cmd);
    if (pri_rd_cmd != smbus_wr_cmd)
    { ErrPrintf("smbus box reg LW_THERM_I2CS_SENSOR_3D write Error!\n");return (1);}

    // smbus read msgbox reg
    InfoPrintf("smbus read msgbox reg start ...\n ");
    i2cRdByte(DEVICE_ADDR_0_FERMI,0x58,&smbus_rd_dat,1);
    Platform::Delay(30);
    if (smbus_rd_dat != smbus_wr_dat)
    { ErrPrintf("smbus box reg LW_THERM_I2CS_SENSOR_3C read Error!\n");return (1);}

    i2cRdByte(DEVICE_ADDR_0_FERMI,0x5c,&smbus_rd_cmd,1);
    Platform::Delay(30);
    if (smbus_rd_cmd != smbus_wr_cmd)
    { ErrPrintf("smbus box reg LW_THERM_I2CS_SENSOR_3D read Error!\n");return (1);}

    //step 3 smbus msgbox interrupt
    int msgbox_intr_host, msgbox_intr_pmu;
    UINT32 pmc_intr_0, falcon_irq;
    InfoPrintf("smbus msgbox interrupt start ...\n");
    smbus_wr_cmd = 0x87654321;
    i2cWrByte(DEVICE_ADDR_0_FERMI,0x5c,smbus_wr_cmd,1);
    Platform::Delay(30);

    //test msgbox_intr disable
    lwgpu->SetRegFldNum("LW_THERM_INTR_EN_0","",0x0);   //disable msgbox intr
    Platform::Delay(5);
    InfoPrintf("test msgbox_intr disable\n ");
    lwgpu->GetRegFldNum("LW_PMC_INTR(0)", "", &pmc_intr_0);
    Platform::EscapeRead("pmu_falcon_irqstat", 0x00, 1, &falcon_irq);
    msgbox_intr_host = ((pmc_intr_0 & 0x00040000) != 0);
    msgbox_intr_pmu  = ((falcon_irq & 0x1000) != 0);
    if(msgbox_intr_host | msgbox_intr_pmu)
    { ErrPrintf("Msgbox intr should not be trigger, msgbox_intr enable is not set!\n");return (1);}

    //test msgbox_intr route
    InfoPrintf("enable msgbox_intr\n ");
    lwgpu->SetRegFldNum("LW_THERM_INTR_EN_0","",0x00010000);   //enable msgbox intr
    Platform::Delay(5);
    InfoPrintf("test msgbox_intr route to host\n ");
    lwgpu->GetRegFldNum("LW_PMC_INTR(0)", "", &pmc_intr_0);
    Platform::EscapeRead("pmu_falcon_irqstat", 0x00, 1, &falcon_irq);
    msgbox_intr_host = ((pmc_intr_0 & 0x00040000) != 0);
    msgbox_intr_pmu  = ((falcon_irq & 0x1000) != 0);
    if(msgbox_intr_host & !msgbox_intr_pmu)
      {InfoPrintf("Msgbox intr route to host correctly\n ");}
    else
      {ErrPrintf("Msgbox intr should not route to PMU!\n");return (1);}

    InfoPrintf("test msgbox_intr route to pmu\n ");
    lwgpu->SetRegFldNum("LW_THERM_INTR_ROUTE","",0x00010000);  //set route to pmu
    Platform::Delay(5);
    lwgpu->GetRegFldNum("LW_PMC_INTR(0)", "", &pmc_intr_0);
    Platform::EscapeRead("pmu_falcon_irqstat", 0x00, 1, &falcon_irq);
    msgbox_intr_host = ((pmc_intr_0 & 0x00040000) != 0);
    msgbox_intr_pmu  = ((falcon_irq & 0x1000) != 0);
    if((!msgbox_intr_host) & msgbox_intr_pmu)
      {InfoPrintf("Msgbox intr route to pmu correctly\n ");}
    else
      {ErrPrintf("Msgbox intr should not route to HOST!\n");return (1);}

    //test msgbox_clear
    InfoPrintf("test msgbox_intr clear\n ");
    if(msgbox_intr_host | msgbox_intr_pmu) {
      lwgpu->SetRegFldNum("LW_THERM_I2CS_SENSOR_3D","", 0);         //disable interrupt source
      lwgpu->SetRegFldNum("LW_THERM_INTR_0","", 0x00010000);    //clear therm intr register
    }
    Platform::Delay(5);
    lwgpu->GetRegFldNum("LW_PMC_INTR(0)", "", &pmc_intr_0);
    Platform::EscapeRead("pmu_falcon_irqstat", 0x00, 1, &falcon_irq);
    msgbox_intr_host = ((pmc_intr_0 & 0x00040000) != 0);
    msgbox_intr_pmu  = ((falcon_irq & 0x1000) != 0);
    if(msgbox_intr_host | msgbox_intr_pmu)
      {ErrPrintf("msgbox intr is not clear!\n");return (1);}
    else
      {InfoPrintf("msgbox intr clear correctly\n ");}

    InfoPrintf("msgbox intr test finished\n ");
    } //end gf10x

    if((m_arch & 0xff0) == 0x410){     //gf11x

    }

  return(0);
}// end_testMsgbox

int i2cslave_fermi::testMsgbox_command()
{
    UINT32   reg_value;

    InfoPrintf("testMsgbox_command() starting\n");
    UINT32 priv_wr, priv_rd;
    UINT32 smbus_wr_byte, smbus_rd_byte;
    UINT32 smbus_wr_byte0, smbus_wr_byte1, smbus_wr_byte2, smbus_wr_byte3;
    UINT32 smbus_rd_byte0, smbus_rd_byte1, smbus_rd_byte2, smbus_rd_byte3;
    UINT32 smbus_wr_word, smbus_rd_word;

    i2cPriEn(DEVICE_ADDR_0_FERMI);
    Platform:: EscapeRead("opt_i2cs_wide_en", 0, 1, &reg_value);
    InfoPrintf("BESTCmd.v opt_i2cs_wide_en :%01x  \n",reg_value);
    if ( (reg_value & 0xf) != 0) {
        ErrPrintf("opt_i2cs_wide_en should not be enabled for msgbox test!\n");
        return (1);
    }

    //-----------------------------------------------------------------
    //step 1 pri write , priv read, smbus read word, smbus read byte
    //-----------------------------------------------------------------

    InfoPrintf("step 1: pri write , priv read, smbus read word, smbus read byte ...\n");

    // COMMAND
    priv_wr = 0x11223344;
    priv_rd = 0;
    smbus_rd_word=0;
    smbus_rd_byte0=0; smbus_rd_byte1=0; smbus_rd_byte2=0; smbus_rd_byte3=0;
    smbus_rd_byte=0;

    if (lwgpu->SetRegFldNum("LW_THERM_MSGBOX_COMMAND","", priv_wr))    //priv_wr
    { ErrPrintf("set LW_THERM_MSGBOX_COMMAND value cannot be changed!\n");return (1);}
    InfoPrintf("priv set LW_THERM_MSGBOX_COMMAND value 0x%08x\n",priv_wr);

    if (lwgpu->GetRegFldNum("LW_THERM_MSGBOX_COMMAND","",&priv_rd))    //priv_rd
    { ErrPrintf("reg LW_THERM_MSGBOX_COMMAND value cannot be read!\n");return (1);}
    InfoPrintf("priv read LW_THERM_MSGBOX_COMMAND value 0x%08x\n",priv_rd);

    i2cRdByte(DEVICE_ADDR_0_FERMI,0x5C,&smbus_rd_word,1);              //smbus_rd_word
    InfoPrintf("smbus word read LW_THERM_MSGBOX_COMMAND value 0x%08x\n",smbus_rd_word);

    i2cRdByte(DEVICE_ADDR_0_FERMI,0x70,&smbus_rd_byte0,0);             //smbus_rd_byte
    i2cRdByte(DEVICE_ADDR_0_FERMI,0x71,&smbus_rd_byte1,0);
    i2cRdByte(DEVICE_ADDR_0_FERMI,0x72,&smbus_rd_byte2,0);
    i2cRdByte(DEVICE_ADDR_0_FERMI,0x73,&smbus_rd_byte3,0);
    smbus_rd_byte = (smbus_rd_byte3 << 24) | (smbus_rd_byte2 << 16) | (smbus_rd_byte1 << 8) | smbus_rd_byte0;
    InfoPrintf("smbus byte read LW_THERM_MSGBOX_COMMAND value 0x%08x\n",smbus_rd_byte);

    Platform::Delay(10);

    if(priv_wr != priv_rd)
    { ErrPrintf("LW_THERM_MSGBOX_COMMAND priv_wr != priv_rd!\n");return (1);}

    if (priv_wr != smbus_rd_word)
    { ErrPrintf("smbus word read LW_THERM_MSGBOX_COMMAND Error!\n");return (1);}

    if (priv_wr != smbus_rd_byte)
    { ErrPrintf("smbus byte read LW_THERM_MSGBOX_COMMAND Error!\n");return (1);}

    //---------------------------------------------------------------------
    //step 2 smbus write word, priv read, smbus read word, smbus read byte
    //---------------------------------------------------------------------
    InfoPrintf("step 2: smbus write word , priv read, smbus read word, smbus read byte ...\n");

    // COMMAND
    smbus_wr_word = 0x55667788;
    priv_rd = 0;
    smbus_rd_word=0;
    smbus_rd_byte0=0; smbus_rd_byte1=0; smbus_rd_byte2=0; smbus_rd_byte3=0;
    smbus_rd_byte=0;

    i2cWrByte(DEVICE_ADDR_0_FERMI,0x5C,smbus_wr_word,1);                //smbus_wr_word;
    Platform::Delay(30);

    if (lwgpu->GetRegFldNum("LW_THERM_MSGBOX_COMMAND","",&priv_rd))    //priv_rd
    { ErrPrintf("reg LW_THERM_MSGBOX_COMMAND value cannot be read!\n");return (1);}
    InfoPrintf("priv read LW_THERM_MSGBOX_COMMAND value 0x%08x\n",priv_rd);

    i2cRdByte(DEVICE_ADDR_0_FERMI,0x5C,&smbus_rd_word,1);              //smbus_rd_word
    InfoPrintf("smbus word read LW_THERM_MSGBOX_COMMAND value 0x%08x\n",smbus_rd_word);

    i2cRdByte(DEVICE_ADDR_0_FERMI,0x70,&smbus_rd_byte0,0);             //smbus_rd_byte
    i2cRdByte(DEVICE_ADDR_0_FERMI,0x71,&smbus_rd_byte1,0);
    i2cRdByte(DEVICE_ADDR_0_FERMI,0x72,&smbus_rd_byte2,0);
    i2cRdByte(DEVICE_ADDR_0_FERMI,0x73,&smbus_rd_byte3,0);
    smbus_rd_byte = (smbus_rd_byte3 << 24) | (smbus_rd_byte2 << 16) | (smbus_rd_byte1 << 8) | smbus_rd_byte0;
    InfoPrintf("smbus byte read LW_THERM_MSGBOX_COMMAND value 0x%08x\n",smbus_rd_byte);

    Platform::Delay(10);

    if(smbus_wr_word != priv_rd)
    { ErrPrintf("LW_THERM_MSGBOX_COMMAND smbus_wr_word != priv_rd!\n");return (1);}

    if (smbus_wr_word != smbus_rd_word)
    { ErrPrintf("smbus word read LW_THERM_MSGBOX_COMMAND Error!\n");return (1);}

    if (smbus_wr_word != smbus_rd_byte)
    { ErrPrintf("smbus byte read LW_THERM_MSGBOX_COMMAND Error!\n");return (1);}

    //---------------------------------------------------------------------
    //step 3 smbus write byte, priv read, smbus read word, smbus read byte
    //---------------------------------------------------------------------
    InfoPrintf("step 3: smbus write byte, priv read, smbus read word, smbus read byte ...\n");

    // COMMAND
    smbus_wr_byte  = 0xddccbbaa;
    smbus_wr_byte0 = 0xaa;
    smbus_wr_byte1 = 0xbb;
    smbus_wr_byte2 = 0xcc;
    smbus_wr_byte3 = 0xdd;
    priv_rd = 0;
    smbus_rd_word=0;
    smbus_rd_byte0=0; smbus_rd_byte1=0; smbus_rd_byte2=0; smbus_rd_byte3=0;
    smbus_rd_byte=0;

    i2cWrByte(DEVICE_ADDR_0_FERMI,0x70,smbus_wr_byte0,0);              //smbus_wr_byte;
    i2cWrByte(DEVICE_ADDR_0_FERMI,0x71,smbus_wr_byte1,0);
    i2cWrByte(DEVICE_ADDR_0_FERMI,0x72,smbus_wr_byte2,0);
    i2cWrByte(DEVICE_ADDR_0_FERMI,0x73,smbus_wr_byte3,0);
    Platform::Delay(30);

    if (lwgpu->GetRegFldNum("LW_THERM_MSGBOX_COMMAND","",&priv_rd))    //priv_rd
    { ErrPrintf("reg LW_THERM_MSGBOX_COMMAND value cannot be read!\n");return (1);}
    InfoPrintf("priv read LW_THERM_MSGBOX_COMMAND value 0x%08x\n",priv_rd);

    i2cRdByte(DEVICE_ADDR_0_FERMI,0x5C,&smbus_rd_word,1);              //smbus_rd_word
    InfoPrintf("smbus word read LW_THERM_MSGBOX_COMMAND value 0x%08x\n",smbus_rd_word);

    i2cRdByte(DEVICE_ADDR_0_FERMI,0x70,&smbus_rd_byte0,0);             //smbus_rd_byte
    i2cRdByte(DEVICE_ADDR_0_FERMI,0x71,&smbus_rd_byte1,0);
    i2cRdByte(DEVICE_ADDR_0_FERMI,0x72,&smbus_rd_byte2,0);
    i2cRdByte(DEVICE_ADDR_0_FERMI,0x73,&smbus_rd_byte3,0);
    smbus_rd_byte = (smbus_rd_byte3 << 24) | (smbus_rd_byte2 << 16) | (smbus_rd_byte1 << 8) | smbus_rd_byte0;
    InfoPrintf("smbus byte read LW_THERM_MSGBOX_COMMAND value 0x%08x\n",smbus_rd_byte);

    if(smbus_wr_byte != priv_rd)
    { ErrPrintf("LW_THERM_MSGBOX_COMMAND smbus_wr_word != priv_rd!\n");return (1);}

    if (smbus_wr_byte != smbus_rd_word)
    { ErrPrintf("smbus word read LW_THERM_MSGBOX_COMMAND Error!\n");return (1);}

    if (smbus_wr_byte != smbus_rd_byte)
    { ErrPrintf("smbus byte read LW_THERM_MSGBOX_COMMAND Error!\n");return (1);}

    //--------------------------------------------------------------------------
    // Disable all the offset intrrupts for Bug 776719
    //--------------------------------------------------------------------------

    lwgpu->SetRegFldNum("LW_THERM_INTR_EN_0","",0x0);   //disable offset intr
    Platform::Delay(1);
    InfoPrintf("test offset_intr disable\n ");

    return(0);
}

int i2cslave_fermi::testMsgbox_datain()
{
    UINT32   reg_value;

    InfoPrintf("testMsgbox_datain() starting\n");
    UINT32 priv_wr, priv_rd;
    UINT32 smbus_wr_byte, smbus_rd_byte;
    UINT32 smbus_wr_byte0, smbus_wr_byte1, smbus_wr_byte2, smbus_wr_byte3;
    UINT32 smbus_rd_byte0, smbus_rd_byte1, smbus_rd_byte2, smbus_rd_byte3;
    UINT32 smbus_wr_word, smbus_rd_word;

    i2cPriEn(DEVICE_ADDR_0_FERMI);
    Platform:: EscapeRead("opt_i2cs_wide_en", 0, 1, &reg_value);
    InfoPrintf("BESTCmd.v opt_i2cs_wide_en :%01x  \n",reg_value);
    if ((reg_value & 0xf) != 0) {
        ErrPrintf("opt_i2cs_wide_en should not be enabled for msgbox test!\n");
        return (1);
    }

    //-----------------------------------------------------------------
    //step 1 pri write , priv read, smbus read word, smbus read byte
    //-----------------------------------------------------------------

    InfoPrintf("step 1: pri write , priv read, smbus read word, smbus read byte ...\n");

    // DATA_IN
    priv_wr = 0x11223344;
    priv_rd = 0;
    smbus_rd_word=0;
    smbus_rd_byte0=0; smbus_rd_byte1=0; smbus_rd_byte2=0; smbus_rd_byte3=0;
    smbus_rd_byte=0;

    if (lwgpu->SetRegFldNum("LW_THERM_MSGBOX_DATA_IN","", priv_wr))    //priv_wr
    { ErrPrintf("set LW_THERM_MSGBOX_DATA_IN value cannot be changed!\n");return (1);}
    InfoPrintf("priv set LW_THERM_MSGBOX_DATA_IN value 0x%08x\n",priv_wr);

    if (lwgpu->GetRegFldNum("LW_THERM_MSGBOX_DATA_IN","",&priv_rd))    //priv_rd
    { ErrPrintf("reg LW_THERM_MSGBOX_DATA_IN value cannot be read!\n");return (1);}
    InfoPrintf("priv read LW_THERM_MSGBOX_DATA_IN value 0x%08x\n",priv_rd);

    i2cRdByte(DEVICE_ADDR_0_FERMI,0x5D,&smbus_rd_word,1);              //smbus_rd_word
    InfoPrintf("smbus word read LW_THERM_MSGBOX_DATA_IN value 0x%08x\n",smbus_rd_word);

    i2cRdByte(DEVICE_ADDR_0_FERMI,0x74,&smbus_rd_byte0,0);             //smbus_rd_byte
    i2cRdByte(DEVICE_ADDR_0_FERMI,0x75,&smbus_rd_byte1,0);
    i2cRdByte(DEVICE_ADDR_0_FERMI,0x76,&smbus_rd_byte2,0);
    i2cRdByte(DEVICE_ADDR_0_FERMI,0x77,&smbus_rd_byte3,0);
    smbus_rd_byte = (smbus_rd_byte3 << 24) | (smbus_rd_byte2 << 16) | (smbus_rd_byte1 << 8) | smbus_rd_byte0;
    InfoPrintf("smbus byte read LW_THERM_MSGBOX_DATA_IN value 0x%08x\n",smbus_rd_byte);

    Platform::Delay(10);

    if(priv_wr != priv_rd)
    { ErrPrintf("LW_THERM_MSGBOX_DATA_IN priv_wr != priv_rd!\n");return (1);}

    if (priv_wr != smbus_rd_word)
    { ErrPrintf("smbus word read LW_THERM_MSGBOX_DATA_IN Error!\n");return (1);}

    if (priv_wr != smbus_rd_byte)
    { ErrPrintf("smbus byte read LW_THERM_MSGBOX_DATA_IN Error!\n");return (1);}

    //---------------------------------------------------------------------
    //step 2 smbus write word, priv read, smbus read word, smbus read byte
    //---------------------------------------------------------------------
    InfoPrintf("step 2: smbus write word , priv read, smbus read word, smbus read byte ...\n");

    // DATA_IN
    smbus_wr_word = 0x55667788;
    priv_rd = 0;
    smbus_rd_word=0;
    smbus_rd_byte0=0; smbus_rd_byte1=0; smbus_rd_byte2=0; smbus_rd_byte3=0;
    smbus_rd_byte=0;

    i2cWrByte(DEVICE_ADDR_0_FERMI,0x5D,smbus_wr_word,1);                //smbus_wr_word;
    Platform::Delay(30);

    if (lwgpu->GetRegFldNum("LW_THERM_MSGBOX_DATA_IN","",&priv_rd))    //priv_rd
    { ErrPrintf("reg LW_THERM_MSGBOX_DATA_IN value cannot be read!\n");return (1);}
    InfoPrintf("priv read LW_THERM_MSGBOX_DATA_IN value 0x%08x\n",priv_rd);

    i2cRdByte(DEVICE_ADDR_0_FERMI,0x5D,&smbus_rd_word,1);              //smbus_rd_word
    InfoPrintf("smbus word read LW_THERM_MSGBOX_DATA_IN value 0x%08x\n",smbus_rd_word);

    i2cRdByte(DEVICE_ADDR_0_FERMI,0x74,&smbus_rd_byte0,0);             //smbus_rd_byte
    i2cRdByte(DEVICE_ADDR_0_FERMI,0x75,&smbus_rd_byte1,0);
    i2cRdByte(DEVICE_ADDR_0_FERMI,0x76,&smbus_rd_byte2,0);
    i2cRdByte(DEVICE_ADDR_0_FERMI,0x77,&smbus_rd_byte3,0);
    smbus_rd_byte = (smbus_rd_byte3 << 24) | (smbus_rd_byte2 << 16) | (smbus_rd_byte1 << 8) | smbus_rd_byte0;
    InfoPrintf("smbus byte read LW_THERM_MSGBOX_DATA_IN value 0x%08x\n",smbus_rd_byte);

    Platform::Delay(10);

    if(smbus_wr_word != priv_rd)
    { ErrPrintf("LW_THERM_MSGBOX_DATA_IN smbus_wr_word != priv_rd!\n");return (1);}

    if (smbus_wr_word != smbus_rd_word)
    { ErrPrintf("smbus word read LW_THERM_MSGBOX_DATA_IN Error!\n");return (1);}

    if (smbus_wr_word != smbus_rd_byte)
    { ErrPrintf("smbus byte read LW_THERM_MSGBOX_DATA_IN Error!\n");return (1);}

    //---------------------------------------------------------------------
    //step 3 smbus write byte, priv read, smbus read word, smbus read byte
    //---------------------------------------------------------------------
    InfoPrintf("step 3: smbus write byte, priv read, smbus read word, smbus read byte ...\n");

    // DATA_IN
    smbus_wr_byte  = 0xddccbbaa;
    smbus_wr_byte0 = 0xaa;
    smbus_wr_byte1 = 0xbb;
    smbus_wr_byte2 = 0xcc;
    smbus_wr_byte3 = 0xdd;
    priv_rd = 0;
    smbus_rd_word=0;
    smbus_rd_byte0=0; smbus_rd_byte1=0; smbus_rd_byte2=0; smbus_rd_byte3=0;
    smbus_rd_byte=0;

    i2cWrByte(DEVICE_ADDR_0_FERMI,0x74,smbus_wr_byte0,0);              //smbus_wr_byte;
    i2cWrByte(DEVICE_ADDR_0_FERMI,0x75,smbus_wr_byte1,0);
    i2cWrByte(DEVICE_ADDR_0_FERMI,0x76,smbus_wr_byte2,0);
    i2cWrByte(DEVICE_ADDR_0_FERMI,0x77,smbus_wr_byte3,0);
    Platform::Delay(30);

    if (lwgpu->GetRegFldNum("LW_THERM_MSGBOX_DATA_IN","",&priv_rd))    //priv_rd
    { ErrPrintf("reg LW_THERM_MSGBOX_DATA_IN value cannot be read!\n");return (1);}
    InfoPrintf("priv read LW_THERM_MSGBOX_DATA_IN value 0x%08x\n",priv_rd);

    i2cRdByte(DEVICE_ADDR_0_FERMI,0x5D,&smbus_rd_word,1);              //smbus_rd_word
    InfoPrintf("smbus word read LW_THERM_MSGBOX_DATA_IN value 0x%08x\n",smbus_rd_word);

    i2cRdByte(DEVICE_ADDR_0_FERMI,0x74,&smbus_rd_byte0,0);             //smbus_rd_byte
    i2cRdByte(DEVICE_ADDR_0_FERMI,0x75,&smbus_rd_byte1,0);
    i2cRdByte(DEVICE_ADDR_0_FERMI,0x76,&smbus_rd_byte2,0);
    i2cRdByte(DEVICE_ADDR_0_FERMI,0x77,&smbus_rd_byte3,0);
    smbus_rd_byte = (smbus_rd_byte3 << 24) | (smbus_rd_byte2 << 16) | (smbus_rd_byte1 << 8) | smbus_rd_byte0;
    InfoPrintf("smbus byte read LW_THERM_MSGBOX_DATA_IN value 0x%08x\n",smbus_rd_byte);

    if(smbus_wr_byte != priv_rd)
    { ErrPrintf("LW_THERM_MSGBOX_DATA_IN smbus_wr_word != priv_rd!\n");return (1);}

    if (smbus_wr_byte != smbus_rd_word)
    { ErrPrintf("smbus word read LW_THERM_MSGBOX_DATA_IN Error!\n");return (1);}

    if (smbus_wr_byte != smbus_rd_byte)
    { ErrPrintf("smbus byte read LW_THERM_MSGBOX_DATA_IN Error!\n");return (1);}

    return(0);
}

int i2cslave_fermi::testMsgbox_dataout()
{
    UINT32   reg_value;

    InfoPrintf("testMsgbox_dataout() starting\n");
    UINT32 priv_wr, priv_rd;
    UINT32 smbus_wr_byte, smbus_rd_byte;
    UINT32 smbus_wr_byte0, smbus_wr_byte1, smbus_wr_byte2, smbus_wr_byte3;
    UINT32 smbus_rd_byte0, smbus_rd_byte1, smbus_rd_byte2, smbus_rd_byte3;
    UINT32 smbus_wr_word, smbus_rd_word;

    i2cPriEn(DEVICE_ADDR_0_FERMI);
    Platform:: EscapeRead("opt_i2cs_wide_en", 0, 1, &reg_value);
    InfoPrintf("BESTCmd.v opt_i2cs_wide_en :%01x  \n",reg_value);
    if ( (reg_value & 0xf) != 0) {
        ErrPrintf("opt_i2cs_wide_en should not be enabled for msgbox test!\n");
        return (1);
    }

    //-----------------------------------------------------------------
    //step 1 pri write , priv read, smbus read word, smbus read byte
    //-----------------------------------------------------------------

    InfoPrintf("step 1: pri write , priv read, smbus read word, smbus read byte ...\n");

    // DATA_OUT
    priv_wr = 0x11223344;
    priv_rd = 0;
    smbus_rd_word=0;
    smbus_rd_byte0=0; smbus_rd_byte1=0; smbus_rd_byte2=0; smbus_rd_byte3=0;
    smbus_rd_byte=0;

    if (lwgpu->SetRegFldNum("LW_THERM_MSGBOX_DATA_OUT","", priv_wr))    //priv_wr
    { ErrPrintf("set LW_THERM_MSGBOX_DATA_OUT value cannot be changed!\n");return (1);}
    InfoPrintf("priv set LW_THERM_MSGBOX_DATA_OUT value 0x%08x\n",priv_wr);

    if (lwgpu->GetRegFldNum("LW_THERM_MSGBOX_DATA_OUT","",&priv_rd))    //priv_rd
    { ErrPrintf("reg LW_THERM_MSGBOX_DATA_OUT value cannot be read!\n");return (1);}
    InfoPrintf("priv read LW_THERM_MSGBOX_DATA_OUT value 0x%08x\n",priv_rd);

    i2cRdByte(DEVICE_ADDR_0_FERMI,0x5E,&smbus_rd_word,1);              //smbus_rd_word
    InfoPrintf("smbus word read LW_THERM_MSGBOX_DATA_OUT value 0x%08x\n",smbus_rd_word);

    i2cRdByte(DEVICE_ADDR_0_FERMI,0x78,&smbus_rd_byte0,0);             //smbus_rd_byte
    i2cRdByte(DEVICE_ADDR_0_FERMI,0x79,&smbus_rd_byte1,0);
    i2cRdByte(DEVICE_ADDR_0_FERMI,0x7a,&smbus_rd_byte2,0);
    i2cRdByte(DEVICE_ADDR_0_FERMI,0x7b,&smbus_rd_byte3,0);
    smbus_rd_byte = (smbus_rd_byte3 << 24) | (smbus_rd_byte2 << 16) | (smbus_rd_byte1 << 8) | smbus_rd_byte0;
    InfoPrintf("smbus byte read LW_THERM_MSGBOX_DATA_OUT value 0x%08x\n",smbus_rd_byte);

    Platform::Delay(10);

    if(priv_wr != priv_rd)
    { ErrPrintf("LW_THERM_MSGBOX_DATA_OUT priv_wr != priv_rd!\n");return (1);}

    if (priv_wr != smbus_rd_word)
    { ErrPrintf("smbus word read LW_THERM_MSGBOX_DATA_OUT Error!\n");return (1);}

    if (priv_wr != smbus_rd_byte)
    { ErrPrintf("smbus byte read LW_THERM_MSGBOX_DATA_OUT Error!\n");return (1);}

    //---------------------------------------------------------------------
    //step 2 smbus write word, priv read, smbus read word, smbus read byte
    //---------------------------------------------------------------------
    InfoPrintf("step 2: smbus write word , priv read, smbus read word, smbus read byte ...\n");

    // DATA_OUT
    smbus_wr_word = 0x55667788;
    priv_rd = 0;
    smbus_rd_word=0;
    smbus_rd_byte0=0; smbus_rd_byte1=0; smbus_rd_byte2=0; smbus_rd_byte3=0;
    smbus_rd_byte=0;

    i2cWrByte(DEVICE_ADDR_0_FERMI,0x5E,smbus_wr_word,1);                //smbus_wr_word;
    Platform::Delay(30);

    if (lwgpu->GetRegFldNum("LW_THERM_MSGBOX_DATA_OUT","",&priv_rd))    //priv_rd
    { ErrPrintf("reg LW_THERM_MSGBOX_DATA_OUT value cannot be read!\n");return (1);}
    InfoPrintf("priv read LW_THERM_MSGBOX_DATA_OUT value 0x%08x\n",priv_rd);

    i2cRdByte(DEVICE_ADDR_0_FERMI,0x5E,&smbus_rd_word,1);              //smbus_rd_word
    InfoPrintf("smbus word read LW_THERM_MSGBOX_DATA_OUT value 0x%08x\n",smbus_rd_word);

    i2cRdByte(DEVICE_ADDR_0_FERMI,0x78,&smbus_rd_byte0,0);             //smbus_rd_byte
    i2cRdByte(DEVICE_ADDR_0_FERMI,0x79,&smbus_rd_byte1,0);
    i2cRdByte(DEVICE_ADDR_0_FERMI,0x7a,&smbus_rd_byte2,0);
    i2cRdByte(DEVICE_ADDR_0_FERMI,0x7b,&smbus_rd_byte3,0);
    smbus_rd_byte = (smbus_rd_byte3 << 24) | (smbus_rd_byte2 << 16) | (smbus_rd_byte1 << 8) | smbus_rd_byte0;
    InfoPrintf("smbus byte read LW_THERM_MSGBOX_DATA_OUT value 0x%08x\n",smbus_rd_byte);

    Platform::Delay(10);

    if(smbus_wr_word != priv_rd)
    { ErrPrintf("LW_THERM_MSGBOX_DATA_OUT smbus_wr_word != priv_rd!\n");return (1);}

    if (smbus_wr_word != smbus_rd_word)
    { ErrPrintf("smbus word read LW_THERM_MSGBOX_DATA_OUT Error!\n");return (1);}

    if (smbus_wr_word != smbus_rd_byte)
    { ErrPrintf("smbus byte read LW_THERM_MSGBOX_DATA_OUT Error!\n");return (1);}

    //---------------------------------------------------------------------
    //step 3 smbus write byte, priv read, smbus read word, smbus read byte
    //---------------------------------------------------------------------
    InfoPrintf("step 3: smbus write byte, priv read, smbus read word, smbus read byte ...\n");

    // DATA_OUT
    smbus_wr_byte  = 0xddccbbaa;
    smbus_wr_byte0 = 0xaa;
    smbus_wr_byte1 = 0xbb;
    smbus_wr_byte2 = 0xcc;
    smbus_wr_byte3 = 0xdd;
    priv_rd = 0;
    smbus_rd_word=0;
    smbus_rd_byte0=0; smbus_rd_byte1=0; smbus_rd_byte2=0; smbus_rd_byte3=0;
    smbus_rd_byte=0;

    i2cWrByte(DEVICE_ADDR_0_FERMI,0x78,smbus_wr_byte0,0);              //smbus_wr_byte;
    i2cWrByte(DEVICE_ADDR_0_FERMI,0x79,smbus_wr_byte1,0);
    i2cWrByte(DEVICE_ADDR_0_FERMI,0x7a,smbus_wr_byte2,0);
    i2cWrByte(DEVICE_ADDR_0_FERMI,0x7b,smbus_wr_byte3,0);
    Platform::Delay(30);

    if (lwgpu->GetRegFldNum("LW_THERM_MSGBOX_DATA_OUT","",&priv_rd))    //priv_rd
    { ErrPrintf("reg LW_THERM_MSGBOX_DATA_OUT value cannot be read!\n");return (1);}
    InfoPrintf("priv read LW_THERM_MSGBOX_DATA_OUT value 0x%08x\n",priv_rd);

    i2cRdByte(DEVICE_ADDR_0_FERMI,0x5E,&smbus_rd_word,1);              //smbus_rd_word
    InfoPrintf("smbus word read LW_THERM_MSGBOX_DATA_OUT value 0x%08x\n",smbus_rd_word);

    i2cRdByte(DEVICE_ADDR_0_FERMI,0x78,&smbus_rd_byte0,0);             //smbus_rd_byte
    i2cRdByte(DEVICE_ADDR_0_FERMI,0x79,&smbus_rd_byte1,0);
    i2cRdByte(DEVICE_ADDR_0_FERMI,0x7a,&smbus_rd_byte2,0);
    i2cRdByte(DEVICE_ADDR_0_FERMI,0x7b,&smbus_rd_byte3,0);
    smbus_rd_byte = (smbus_rd_byte3 << 24) | (smbus_rd_byte2 << 16) | (smbus_rd_byte1 << 8) | smbus_rd_byte0;
    InfoPrintf("smbus byte read LW_THERM_MSGBOX_DATA_OUT value 0x%08x\n",smbus_rd_byte);

    if(smbus_wr_byte != priv_rd)
    { ErrPrintf("LW_THERM_MSGBOX_DATA_OUT smbus_wr_word != priv_rd!\n");return (1);}

    if (smbus_wr_byte != smbus_rd_word)
    { ErrPrintf("smbus word read LW_THERM_MSGBOX_DATA_OUT Error!\n");return (1);}

    if (smbus_wr_byte != smbus_rd_byte)
    { ErrPrintf("smbus byte read LW_THERM_MSGBOX_DATA_OUT Error!\n");return (1);}

    return(0);
}

int i2cslave_fermi::testMsgbox_mutex()
{
    UINT32   reg_value;

    InfoPrintf("testMsgbox_mutex() starting\n");
    UINT32 priv_wr, priv_rd;
    UINT32 smbus_wr_byte, smbus_rd_byte;
    UINT32 smbus_wr_word, smbus_rd_word;

    i2cPriEn(DEVICE_ADDR_0_FERMI);
    Platform:: EscapeRead("opt_i2cs_wide_en", 0, 1, &reg_value);
    InfoPrintf("BESTCmd.v opt_i2cs_wide_en :%01x  \n",reg_value);
    if ( (reg_value & 0xf) != 0) {
        ErrPrintf("opt_i2cs_wide_en should not be enabled for msgbox test!\n");
        return (1);
    }

    //-----------------------------------------------------------------
    //step 1 pri write , priv read, smbus read word, smbus read byte
    //-----------------------------------------------------------------

    InfoPrintf("step 1: pri write , priv read, smbus read word, smbus read byte ...\n");

    // MUTEX
    priv_wr = 0x33;
    priv_rd = 0;
    smbus_rd_word=0;
    smbus_rd_byte=0;

    if (lwgpu->SetRegFldNum("LW_THERM_MSGBOX_MUTEX","", priv_wr))    //priv_wr
    { ErrPrintf("set LW_THERM_MSGBOX_MUTEX value cannot be changed!\n");return (1);}
    InfoPrintf("priv set LW_THERM_MSGBOX_MUTEX value 0x%08x\n",priv_wr);

    if (lwgpu->GetRegFldNum("LW_THERM_MSGBOX_MUTEX","",&priv_rd))    //priv_rd
    { ErrPrintf("reg LW_THERM_MSGBOX_MUTEX value cannot be read!\n");return (1);}
    InfoPrintf("priv read LW_THERM_MSGBOX_MUTEX value 0x%08x\n",priv_rd);

    i2cRdByte(DEVICE_ADDR_0_FERMI,0x5F,&smbus_rd_word,1);              //smbus_rd_word
    InfoPrintf("smbus word read LW_THERM_MSGBOX_MUTEX value 0x%08x\n",smbus_rd_word);

    i2cRdByte(DEVICE_ADDR_0_FERMI,0x7c,&smbus_rd_byte,0);             //smbus_rd_byte
    InfoPrintf("smbus byte read LW_THERM_MSGBOX_MUTEX value 0x%08x\n",smbus_rd_byte);

    Platform::Delay(10);

    if(priv_wr != priv_rd)
    { ErrPrintf("LW_THERM_MSGBOX_MUTEX priv_wr != priv_rd!\n");return (1);}

    if (priv_wr != smbus_rd_word)
    { ErrPrintf("smbus word read LW_THERM_MSGBOX_MUTEX Error!\n");return (1);}

    if (priv_wr != smbus_rd_byte)
    { ErrPrintf("smbus byte read LW_THERM_MSGBOX_MUTEX Error!\n");return (1);}

    //---------------------------------------------------------------------
    //step 2  priv write a non-zero data to MUTEX, MUTEX would not change
    //---------------------------------------------------------------------

    InfoPrintf("step 2: priv write a non-zero data to MUTEX, MUTEX would not change\n");

    // MUTEX
    priv_wr = 0x44;
    priv_rd = 0;
    smbus_rd_word=0;
    smbus_rd_byte=0;

    if (lwgpu->SetRegFldNum("LW_THERM_MSGBOX_MUTEX","", priv_wr))    //priv_wr
    { ErrPrintf("set LW_THERM_MSGBOX_MUTEX value cannot be changed!\n");return (1);}
    InfoPrintf("priv set LW_THERM_MSGBOX_MUTEX value 0x%08x\n",priv_wr);

    if (lwgpu->GetRegFldNum("LW_THERM_MSGBOX_MUTEX","",&priv_rd))    //priv_rd
    { ErrPrintf("reg LW_THERM_MSGBOX_MUTEX value cannot be read!\n");return (1);}
    InfoPrintf("priv read LW_THERM_MSGBOX_MUTEX value 0x%08x\n",priv_rd);

    i2cRdByte(DEVICE_ADDR_0_FERMI,0x5F,&smbus_rd_word,1);              //smbus_rd_word
    InfoPrintf("smbus word read LW_THERM_MSGBOX_MUTEX value 0x%08x\n",smbus_rd_word);

    i2cRdByte(DEVICE_ADDR_0_FERMI,0x7c,&smbus_rd_byte,0);             //smbus_rd_byte
    InfoPrintf("smbus byte read LW_THERM_MSGBOX_MUTEX value 0x%08x\n",smbus_rd_byte);

    Platform::Delay(10);

    if(priv_rd != 0x33)
    { ErrPrintf("LW_THERM_MSGBOX_MUTEX priv_wr -- MUTEX should be kept by 0x33, but not\n");return (1);}

    if (smbus_rd_word != 0x33)
    { ErrPrintf("smbus word read LW_THERM_MSGBOX_MUTEX Error -- MUTEX should be kept by 0x33, but not!\n");return (1);}

    if (smbus_rd_byte != 0x33)
    { ErrPrintf("smbus byte read LW_THERM_MSGBOX_MUTEX Error -- MUTEX should be kept by 0x33, but not!\n");return (1);}

    //---------------------------------------------------------------------
    //step 3  priv write a zero data to MUTEX, then priv wirte a non-zero data to MUTEX,
    //        MUTEX would change
    //---------------------------------------------------------------------

    InfoPrintf("step 3: priv write a zero data to MUTEX, then priv wirte a non-zero data to MUTEX\n");

    // MUTEX
    priv_rd = 0;
    smbus_rd_word=0;
    smbus_rd_byte=0;

    priv_wr = 0;
    if (lwgpu->SetRegFldNum("LW_THERM_MSGBOX_MUTEX","", priv_wr))    //priv_wr
    { ErrPrintf("set LW_THERM_MSGBOX_MUTEX value cannot be changed!\n");return (1);}
    InfoPrintf("priv set LW_THERM_MSGBOX_MUTEX value 0x%08x\n",priv_wr);

    priv_wr = 0x55;
    if (lwgpu->SetRegFldNum("LW_THERM_MSGBOX_MUTEX","", priv_wr))    //priv_wr
    { ErrPrintf("set LW_THERM_MSGBOX_MUTEX value cannot be changed!\n");return (1);}
    InfoPrintf("priv set LW_THERM_MSGBOX_MUTEX value 0x%08x\n",priv_wr);

    if (lwgpu->GetRegFldNum("LW_THERM_MSGBOX_MUTEX","",&priv_rd))    //priv_rd
    { ErrPrintf("reg LW_THERM_MSGBOX_MUTEX value cannot be read!\n");return (1);}
    InfoPrintf("priv read LW_THERM_MSGBOX_MUTEX value 0x%08x\n",priv_rd);

    i2cRdByte(DEVICE_ADDR_0_FERMI,0x5F,&smbus_rd_word,1);              //smbus_rd_word
    InfoPrintf("smbus word read LW_THERM_MSGBOX_MUTEX value 0x%08x\n",smbus_rd_word);

    i2cRdByte(DEVICE_ADDR_0_FERMI,0x7c,&smbus_rd_byte,0);             //smbus_rd_byte
    InfoPrintf("smbus byte read LW_THERM_MSGBOX_MUTEX value 0x%08x\n",smbus_rd_byte);

    Platform::Delay(10);

    if(priv_wr != priv_rd)
    { ErrPrintf("LW_THERM_MSGBOX_MUTEX priv_wr != priv_rd!\n");return (1);}

    if (priv_wr != smbus_rd_word)
    { ErrPrintf("smbus word read LW_THERM_MSGBOX_MUTEX Error!\n");return (1);}

    if (priv_wr != smbus_rd_byte)
    { ErrPrintf("smbus byte read LW_THERM_MSGBOX_MUTEX Error!\n");return (1);}

    //---------------------------------------------------------------------
    //step 4  smbus write-word a non-zero data to MUTEX, MUTEX would not change
    //---------------------------------------------------------------------

    InfoPrintf("step 4: smbus write-word a non-zero data to MUTEX, MUTEX would not change\n");

    // MUTEX
    smbus_wr_word = 0x66;
    priv_rd = 0;
    smbus_rd_word=0;
    smbus_rd_byte=0;

    i2cWrByte(DEVICE_ADDR_0_FERMI,0x5F,smbus_wr_word,1);                //smbus_wr_word;
    Platform::Delay(30);

    if (lwgpu->GetRegFldNum("LW_THERM_MSGBOX_MUTEX","",&priv_rd))    //priv_rd
    { ErrPrintf("reg LW_THERM_MSGBOX_MUTEX value cannot be read!\n");return (1);}
    InfoPrintf("priv read LW_THERM_MSGBOX_MUTEX value 0x%08x\n",priv_rd);

    i2cRdByte(DEVICE_ADDR_0_FERMI,0x5F,&smbus_rd_word,1);              //smbus_rd_word
    InfoPrintf("smbus word read LW_THERM_MSGBOX_MUTEX value 0x%08x\n",smbus_rd_word);

    i2cRdByte(DEVICE_ADDR_0_FERMI,0x7c,&smbus_rd_byte,0);             //smbus_rd_byte
    InfoPrintf("smbus byte read LW_THERM_MSGBOX_MUTEX value 0x%08x\n",smbus_rd_byte);

    Platform::Delay(10);

    if(priv_rd != 0x55)
    { ErrPrintf("LW_THERM_MSGBOX_MUTEX priv_rd -- MUTEX should be kept by 0x55, but not\n");return (1);}

    if (smbus_rd_word != 0x55)
    { ErrPrintf("smbus word read LW_THERM_MSGBOX_MUTEX Error -- MUTEX should be kept by 0x55, but not!\n");return (1);}

    if (smbus_rd_byte != 0x55)
    { ErrPrintf("smbus byte read LW_THERM_MSGBOX_MUTEX Error -- MUTEX should be kept by 0x55, but not!\n");return (1);}

    //---------------------------------------------------------------------
    //step 5  smbus write-word a zero data to MUTEX, then smbus write-word a non-zero data to MUTEX,
    //        MUTEX would change
    //---------------------------------------------------------------------

    InfoPrintf("step 5: smbus write-byte a zero data to MUTEX, then smbus wirte-byte a non-zero data to MUTEX\n");

    // MUTEX
    priv_rd = 0;
    smbus_rd_word=0;
    smbus_rd_byte=0;

    smbus_wr_word = 0x0;
    i2cWrByte(DEVICE_ADDR_0_FERMI,0x5F,smbus_wr_word,1);                //smbus_wr_word;
    Platform::Delay(30);

    smbus_wr_word = 0x77;
    i2cWrByte(DEVICE_ADDR_0_FERMI,0x5F,smbus_wr_word,1);                //smbus_wr_word;
    Platform::Delay(30);

    if (lwgpu->GetRegFldNum("LW_THERM_MSGBOX_MUTEX","",&priv_rd))    //priv_rd
    { ErrPrintf("reg LW_THERM_MSGBOX_MUTEX value cannot be read!\n");return (1);}
    InfoPrintf("priv read LW_THERM_MSGBOX_MUTEX value 0x%08x\n",priv_rd);

    i2cRdByte(DEVICE_ADDR_0_FERMI,0x5F,&smbus_rd_word,1);              //smbus_rd_word
    InfoPrintf("smbus word read LW_THERM_MSGBOX_MUTEX value 0x%08x\n",smbus_rd_word);

    i2cRdByte(DEVICE_ADDR_0_FERMI,0x7c,&smbus_rd_byte,0);             //smbus_rd_byte
    InfoPrintf("smbus byte read LW_THERM_MSGBOX_MUTEX value 0x%08x\n",smbus_rd_byte);

    Platform::Delay(10);

    if(smbus_wr_word != priv_rd)
    { ErrPrintf("LW_THERM_MSGBOX_MUTEX smbus_wr_word != priv_rd!\n");return (1);}

    if (smbus_wr_word != smbus_rd_word)
    { ErrPrintf("smbus word read LW_THERM_MSGBOX_MUTEX Error!\n");return (1);}

    if (smbus_wr_word != smbus_rd_byte)
    { ErrPrintf("smbus byte read LW_THERM_MSGBOX_MUTEX Error!\n");return (1);}

    //---------------------------------------------------------------------
    //step 6  smbus write-word a non-zero data to MUTEX, MUTEX would not change
    //---------------------------------------------------------------------

    InfoPrintf("step 6: smbus write-byte a non-zero data to MUTEX, MUTEX would not change\n");

    // MUTEX
    smbus_wr_byte = 0x88;
    priv_rd = 0;
    smbus_rd_word=0;
    smbus_rd_byte=0;

    i2cWrByte(DEVICE_ADDR_0_FERMI,0x7c,smbus_wr_byte,0);                //smbus_wr_byte;
    Platform::Delay(30);

    if (lwgpu->GetRegFldNum("LW_THERM_MSGBOX_MUTEX","",&priv_rd))    //priv_rd
    { ErrPrintf("reg LW_THERM_MSGBOX_MUTEX value cannot be read!\n");return (1);}
    InfoPrintf("priv read LW_THERM_MSGBOX_MUTEX value 0x%08x\n",priv_rd);

    i2cRdByte(DEVICE_ADDR_0_FERMI,0x5F,&smbus_rd_word,1);              //smbus_rd_word
    InfoPrintf("smbus word read LW_THERM_MSGBOX_MUTEX value 0x%08x\n",smbus_rd_word);

    i2cRdByte(DEVICE_ADDR_0_FERMI,0x7c,&smbus_rd_byte,0);             //smbus_rd_byte
    InfoPrintf("smbus byte read LW_THERM_MSGBOX_MUTEX value 0x%08x\n",smbus_rd_byte);

    Platform::Delay(10);

    if(priv_rd != 0x77)
    { ErrPrintf("LW_THERM_MSGBOX_MUTEX priv_rd -- MUTEX should be kept by 0x77, but not\n");return (1);}

    if (smbus_rd_word != 0x77)
    { ErrPrintf("smbus word read LW_THERM_MSGBOX_MUTEX Error -- MUTEX should be kept by 0x77, but not!\n");return (1);}

    if (smbus_rd_byte != 0x77)
    { ErrPrintf("smbus byte read LW_THERM_MSGBOX_MUTEX Error -- MUTEX should be kept by 0x77, but not!\n");return (1);}

    //---------------------------------------------------------------------
    //step 7  smbus write-byte a zero data to MUTEX, then smbus write-word a non-zero data to MUTEX,
    //        MUTEX would change
    //---------------------------------------------------------------------

    InfoPrintf("step 7: smbus write-byte a zero data to MUTEX, then smbus wirte-byte a non-zero data to MUTEX\n");

    // MUTEX
    priv_rd = 0;
    smbus_rd_word=0;
    smbus_rd_byte=0;

    smbus_wr_byte = 0x0;
    i2cWrByte(DEVICE_ADDR_0_FERMI,0x7c,smbus_wr_byte,0);                //smbus_wr_byte;
    Platform::Delay(30);

    smbus_wr_byte = 0x99;
    i2cWrByte(DEVICE_ADDR_0_FERMI,0x7c,smbus_wr_byte,0);                //smbus_wr_byte;
    Platform::Delay(30);

    if (lwgpu->GetRegFldNum("LW_THERM_MSGBOX_MUTEX","",&priv_rd))    //priv_rd
    { ErrPrintf("reg LW_THERM_MSGBOX_MUTEX value cannot be read!\n");return (1);}
    InfoPrintf("priv read LW_THERM_MSGBOX_MUTEX value 0x%08x\n",priv_rd);

    i2cRdByte(DEVICE_ADDR_0_FERMI,0x5F,&smbus_rd_word,1);              //smbus_rd_word
    InfoPrintf("smbus word read LW_THERM_MSGBOX_MUTEX value 0x%08x\n",smbus_rd_word);

    i2cRdByte(DEVICE_ADDR_0_FERMI,0x7c,&smbus_rd_byte,0);             //smbus_rd_byte
    InfoPrintf("smbus byte read LW_THERM_MSGBOX_MUTEX value 0x%08x\n",smbus_rd_byte);

    Platform::Delay(10);

    if(smbus_wr_byte != priv_rd)
    { ErrPrintf("LW_THERM_MSGBOX_MUTEX smbus_wr_byte != priv_rd!\n");return (1);}

    if (smbus_wr_byte != smbus_rd_word)
    { ErrPrintf("smbus word read LW_THERM_MSGBOX_MUTEX Error!\n");return (1);}

    if (smbus_wr_byte != smbus_rd_byte)
    { ErrPrintf("smbus byte read LW_THERM_MSGBOX_MUTEX Error!\n");return (1);}

    return(0);
}

UINT32 i2cslave_fermi::temp2code(UINT32 temp)
{
    UINT32 code;

    // temperature = temp_code * 0.0540161 - 711,  nominally
        code = UINT32 ((temp + 711 + 1) * 18.51299 );

    //HC
    InfoPrintf("temp %d, code %d\n", temp, code);
    return code;
}

int i2cslave_fermi::testGpio_9()
{
//step 1 pri write and read
//     const char *alert_5h_regname;
//     const char *alert_5l_regname;
//     UINT32 alert5h_dat_addr, alert5l_dat_addr, alert5_en_addr,alert5_stat_addr;
//     UINT32 alert5h_wr_dat, alert5l_wr_dat, alert5_wr_en, alert5_wr_stat;
//     UINT32 alert5h_rd_dat, alert5l_rd_dat, alert5_rd_en, alert5_rd_stat;
//     alert5h_wr_dat = 0xab;
//     alert5l_wr_dat = 0xcd;
//     alert5_wr_en   = 0x3;
//     alert5_wr_stat = 0x3;
//
//    //open the sensor
//     if (lwgpu->SetRegFldDef("LW_THERM_SENSOR_0", "_POWER", "_ON")) {ErrPrintf("could not write LW_THERM_SENSOR_0\n"); return(1);}
//     //set tsvref_2_tsense to 1
//     UINT32 myValue = 8;
//     if (WriteDacBgapCntl(1, myValue))
//     {
//         ErrPrintf("Could not  update LW_PDISP_DAC_BGAP_CNTL to 0x8!\n");
//         return (1);
//     }
//
//     InfoPrintf("pri write and read start ...\n");
//     // ALERT5H_DATA
//     // The ALERT_5H/ALERT_5L temperature thresholds moved to a different
//     // register in gk110.  We need to determine which set of registers they are
//     // kept in.  For ALERT_5H, we will first try LW_THERM_I2CS_SENSOR_07.  If
//     // it is not there, we will assume it lives in LW_THERM_EVT_ALERT_5H.  We
//     // will not test the location for ALERT_5L since we can assume it follows
//     // ALERT_5H.
//     alert_5h_regname = "LW_THERM_I2CS_SENSOR_07";
//     alert_5l_regname = "LW_THERM_I2CS_SENSOR_08";
//     if (lwgpu->GetRegNum(alert_5h_regname,&alert5h_dat_addr))
//     {
//         // look for it in the alternate location
//         alert_5h_regname = "LW_THERM_EVT_ALERT_5H";
//         alert_5l_regname = "LW_THERM_EVT_ALERT_5L";
//         if (lwgpu->GetRegNum(alert_5h_regname,&alert5h_dat_addr))
//         {
//             ErrPrintf("ERROR: Could not find ALERT_5H temperature threshold "
//                 "in LW_THERM_I2CS_SENSOR_07 or LW_THERM_EVT_ALERT_5H.\n");
//             return (1);
//         }
//     }
//     InfoPrintf("ALERT5H_DATA number 0x%08x\n",alert5h_dat_addr);
//
//     if (lwgpu->SetRegFldNum(alert_5h_regname,"",alert5h_wr_dat))
//     {
//         ErrPrintf("set %s value cannot be changed!\n", alert_5h_regname);
//         return (1);
//     }
//     InfoPrintf(" set ALERT5H_DATA value 0x%08x\n",alert5h_wr_dat);
//
//     if (lwgpu->GetRegFldNum(alert_5h_regname,"",&alert5h_rd_dat))
//     { ErrPrintf("reg %s value cannot be read!\n", alert_5h_regname);
//         return (1);
//     }
//     InfoPrintf("read ALERT5H_DATA value 0x%08x\n",alert5h_rd_dat);
//
//
//     // ALERT5L_DATA
//     if (lwgpu->GetRegNum(alert_5l_regname,&alert5l_dat_addr))
//     {
//         ErrPrintf("reg %s did not exist!\n", alert_5l_regname);
//         return (1);
//     }
//     InfoPrintf("ALERT5L_DATA number 0x%08x\n",alert5l_dat_addr);
//
//     if (lwgpu->SetRegFldNum(alert_5l_regname,"",alert5l_wr_dat))
//     {
//         ErrPrintf("set %s value cannot be changed!\n", alert_5l_regname);
//         return (1);
//     }
//     InfoPrintf(" set ALERT5L_DATA value 0x%08x\n",alert5l_wr_dat);
//
//     if (lwgpu->GetRegFldNum(alert_5l_regname,"",&alert5l_rd_dat))
//     {
//         ErrPrintf("reg %s value cannot be read!\n", alert_5l_regname);
//         return (1);
//     }
//     InfoPrintf("read ALERT5L_DATA value 0x%08x\n",alert5l_rd_dat);
//
//     // ALERT5_EN
//     if (lwgpu->GetRegNum("LW_THERM_I2CS_SENSOR_09",&alert5_en_addr))
//     { ErrPrintf("reg LW_THERM_I2CS_SENSOR_09 did not exist!\n");return (1);}
//     InfoPrintf("ALERT5_EN number 0x%08x\n",alert5_en_addr);
//
//     if (lwgpu->SetRegFldNum("LW_THERM_I2CS_SENSOR_09","",alert5_wr_en))
//     { ErrPrintf("set LW_THERM_I2CS_SENSOR_09 value cannot be changed!\n");return (1);}
//     InfoPrintf(" set ALERT5_EN value 0x%08x\n",alert5_wr_en);
//
//     if (lwgpu->GetRegFldNum("LW_THERM_I2CS_SENSOR_09","",&alert5_rd_en))
//     { ErrPrintf("reg LW_THERM_I2CS_SENSOR_09 value cannot be read!\n");return (1);}
//     InfoPrintf("read ALERT5_EN value 0x%08x\n",alert5_rd_en);
//
//     // ALERT5_STAT
//     if (lwgpu->GetRegNum("LW_THERM_I2CS_SENSOR_10",&alert5_stat_addr))
//     { ErrPrintf("reg LW_THERM_I2CS_SENSOR_10 did not exist!\n");return (1);}
//     InfoPrintf("ALERT5_STAT number 0x%08x\n",alert5_en_addr);
//
//     if (lwgpu->SetRegFldNum("LW_THERM_I2CS_SENSOR_10","",alert5_wr_stat))
//     { ErrPrintf("set LW_THERM_I2CS_SENSOR_10 value cannot be changed!\n");return (1);}
//     InfoPrintf(" set ALERT5_STAT value 0x%08x\n",alert5_wr_stat);
//
//     if (lwgpu->GetRegFldNum("LW_THERM_I2CS_SENSOR_10","",&alert5_rd_stat))
//     { ErrPrintf("reg LW_THERM_I2CS_SENSOR_10 value cannot be read!\n");return (1);}
//     InfoPrintf("read ALERT5_STAT value 0x%08x\n",alert5_rd_stat);

//step 2 RSL_ATERT5 smbus read and clear test

//     InfoPrintf("RSL_ATERT5 smbus read and clear start ...\n ");
//     Platform::EscapeWrite("tb_thermal_sensor_value", 0, 14, temp2code(85));  //set temp to 85
//     Platform::EscapeWrite("tb_thermal_sensor_override", 0, 1, 1);
//
//     lwgpu->SetRegFldNum(alert_5l_regname,"",0x54);      //set temp_gt_alert5l is not truth
//     lwgpu->SetRegFldNum(alert_5h_regname,"",0x56);      //set temp_gt_alert5h is not truth
//     lwgpu->SetRegFldNum("LW_THERM_I2CS_SENSOR_10","",0x3);       //set RSL_ATERT5 to 2'b11
//
//     Platform::Delay(5);
//
//     InfoPrintf("read value and clear\n");
//     i2cRdByte(DEVICE_ADDR_0_FERMI,0x10,&alert5_rd_stat,0);             //read value and clear
//     Platform::Delay(10);
//     if (alert5_rd_stat != 0x3)
//     { ErrPrintf("smbus reg LW_THERM_I2CS_SENSOR_10 read Error, sould be 0x3!\n");return (1);}
//
//     InfoPrintf("read again and make sure clear is successful\n");
//     i2cRdByte(DEVICE_ADDR_0_FERMI,0x10,&alert5_rd_stat,0);             //read again and make sure clear is successful
//     Platform::Delay(10);
//     if (alert5_rd_stat != 0x0)
//     { ErrPrintf("smbus reg LW_THERM_I2CS_SENSOR_10 read Error, sould be 0x0!\n");return (1);}

//step 3 smbus write and read
//     UINT32 pri_rd_alert5h_dat;
//     UINT32 pri_rd_alert5l_dat;
//     UINT32 pri_rd_alert5_en;
//
//     i2cPriEn(DEVICE_ADDR_0_FERMI);
//     // smbus write
//     InfoPrintf("smbus write reg start ...\n ");
//     alert5h_wr_dat = 0x78;
//     i2cWrByte(DEVICE_ADDR_0_FERMI,0x07,alert5h_wr_dat,0);
//     Platform::Delay(10);
//     lwgpu->GetRegFldNum(alert_5h_regname,"",&pri_rd_alert5h_dat);
//     if (pri_rd_alert5h_dat!= alert5h_wr_dat)
//     {
//         ErrPrintf("smbus reg %s write Error!\n", alert_5h_regname);
//         return (1);
//     }
//
//     alert5l_wr_dat = 0x56;
//     i2cWrByte(DEVICE_ADDR_0_FERMI,0x08,alert5l_wr_dat,0);
//     Platform::Delay(10);
//     lwgpu->GetRegFldNum(alert_5l_regname,"",&pri_rd_alert5l_dat);
//     if (pri_rd_alert5l_dat!= alert5l_wr_dat)
//     {
//         ErrPrintf("smbus reg %s write Error!\n", alert_5l_regname);
//         return (1);
//     }
//
//     alert5_wr_en = 0x2;
//     i2cWrByte(DEVICE_ADDR_0_FERMI,0x09,alert5_wr_en,0);
//     Platform::Delay(10);
//     lwgpu->GetRegFldNum("LW_THERM_I2CS_SENSOR_09","",&pri_rd_alert5_en);
//     if (pri_rd_alert5_en!= alert5_wr_en)
//     { ErrPrintf("smbus reg LW_THERM_I2CS_SENSOR_09 write Error!\n");
//         return (1);
//     }
//
//     // smbus read
//     InfoPrintf("smbus read reg start ...\n ");
//     i2cRdByte(DEVICE_ADDR_0_FERMI,0x07,&alert5h_rd_dat,0);
//     Platform::Delay(10);
//     if (alert5h_rd_dat != alert5h_wr_dat)
//     {
//         ErrPrintf("smbus reg %s read Error!\n", alert_5h_regname);
//         return (1);
//     }
//
//     i2cRdByte(DEVICE_ADDR_0_FERMI,0x08,&alert5l_rd_dat,0);
//     Platform::Delay(10);
//     if (alert5l_rd_dat != alert5l_wr_dat)
//     {
//         ErrPrintf("smbus reg %s read Error!\n", alert_5l_regname);
//         return (1);
//     }
//
//     i2cRdByte(DEVICE_ADDR_0_FERMI,0x09,&alert5_rd_en,0);
//     Platform::Delay(10);
//     if (alert5_rd_en != alert5_wr_en)
//     { ErrPrintf("smbus reg LW_THERM_I2CS_SENSOR_09 read Error!\n");return (1);}

//step 4 GPIO[9] test
     UINT32 data;
     InfoPrintf("Config GPIO[9] start ...\n ");
//     if((m_arch & 0xff0) == 0x400){     //gf10x
//        if (lwgpu->SetRegFldDef("LW_PMGR_ALT_GPIO", "_GPIO_9", "_ALTERNATE")) {ErrPrintf("could not write LW_PMGR_ALT_GPIO_GPIO_9\n"); return(1);}
//        if (lwgpu->SetRegFldDef("LW_PMGR_ALT2_GPIO", "_GPIO_9_OPEN_DRAIN", "_ENABLE")) {ErrPrintf("could not write LW_PMGR_ALT2_GPIO_GPIO_9_OPEN_DRAIN\n"); return(1);}
//     }
//     else{     //gf11x
//        if (lwgpu->SetRegFldDef("LW_PMGR_GPIO_9_OUTPUT_CNTL", "_SEL", "_FAN_ALERT")) { ErrPrintf("could not update LW_PMGR_GPIO_9_OUTPUT_CNTL"); }
//        if (lwgpu->SetRegFldDef("LW_PMGR_GPIO_9_OUTPUT_CNTL", "_OPEN_DRAIN", "_ENABLE")) {ErrPrintf("could not write LW_PMGR_GPIO_9_OUTPUT_CNTL_OPEN_DRAIN\n"); return(1);}
//        if (lwgpu->SetRegFldDef("LW_PMGR_GPIO_OUTPUT_CNTL_TRIGGER", "_UPDATE", "_TRIGGER")) {ErrPrintf("could not write LW_PMGR_GPIO_OUTPUT_CNTL_TRIGGER\n"); return(1);}
//     }
//        if (lwgpu->SetRegFldDef("LW_PMGR_FAN1", "_ALERT_OUTPUT_OVERRIDE", "_ENABLED")) {ErrPrintf("could not write LW_PMGR_FAN1_ALERT_OUTPUT_OVERRIDE\n"); return(1);}
//        if (lwgpu->SetRegFldDef("LW_PMGR_FAN1", "_ALERT_OUTPUT_POLARITY", "_HI")) {ErrPrintf("could not write LW_PMGR_FAN1_ALERT_OUTPUT_POLARITY\n"); return(1);}

//     InfoPrintf("test alert5h enable GPIO[9]");   //In step 2, alert5_en has been set to 0x2
//     Platform::EscapeWrite("tb_thermal_sensor_value", 0, 14, temp2code(121));
//     Platform::EscapeWrite("tb_thermal_sensor_override", 0, 1, 1);
//     Platform::EscapeWrite("fan_overtemp_polarity", 0, 1, 1);
//     Platform::EscapeWrite("fan_overtemp_override", 0, 1, 1);
//     Platform::EscapeWrite("fan_output", 0, 1, 0);
//     lwgpu->SetRegFldDef("LW_THERM_ALERT_EN", "_THERMAL_0", "_ENABLED");
//     lwgpu->SetRegFldDef("LW_THERM_EVT_THERMAL_0", "_MODE", "_FORCED");
//     lwgpu->SetRegFldDef("LW_THERM_EVENT_SELECT", "_THERMAL_0", "_NORMAL");
//     Platform::Delay(5);
//
//     Platform::EscapeRead("Gpio_9", 0, 1, &data);
//     if (data != 1) { ErrPrintf("GPIO[9] isn't 1. case 1.\n"); return (1); }
//
////     InfoPrintf("test alert5l enable GPIO[9]");
////     i2cWrByte(DEVICE_ADDR_0_FERMI,0x09, 0x1, 0);      //set alert5l_en to 0x1
////     Platform::Delay(5);
////     Platform::EscapeWrite("tb_thermal_sensor_value", 0, 14, temp2code(85));
////     Platform::EscapeWrite("tb_thermal_sensor_override", 0, 1, 1);
//     lwgpu->SetRegFldDef("LW_THERM_ALERT_EN", "_THERMAL_0", "_DISABLED");
//     Platform::Delay(5);
//
//     Platform::EscapeRead("Gpio_9", 0, 1, &data);
//     if (data != 0) { ErrPrintf("GPIO[9] isn't 1. case 2.\n"); return (1); }
/////add to check max fan connection
     lwgpu->SetRegFldDef("LW_THERM_MAXFAN", "_FORCE", "_TRIGGER");
     Platform::Delay(10);
     Platform::EscapeRead("therm2sci_temp_alert", 0, 1, &data);
     if (data != 1) { ErrPrintf("max fan connection error\n"); return (1); }

     lwgpu->SetRegFldDef("LW_THERM_MAXFAN", "_STATUS", "_CLEAR");
     Platform::Delay(10);
     Platform::EscapeRead("therm2sci_temp_alert", 0, 1, &data);
     if (data != 0) { ErrPrintf("max fan connection error1\n"); return (1); }
//////check SplitRail PMU connection in gp100 except jtag
//1.
//     InfoPrintf("333333333333333 \n");
//     Platform::EscapeWrite("opt_split_rail_sd_clamp_bypass", 0, 1, 1);
//     Platform::Delay(10);
//     Platform::EscapeRead("split_rail_sd__clamp", 0, 1, &data);
//     if (data != 0) { ErrPrintf("1 split_rail connection error!\n"); return (1); }
//     InfoPrintf("111111111111111111 \n");
////2.
//     Platform::EscapeWrite("opt_split_rail_sd_clamp_bypass", 0, 1, 0);
//     Platform::EscapeWrite("Pex_pwr_goodn_IB", 0, 1, 0);
//     Platform::Delay(10);
//     Platform::EscapeRead("split_rail_sd__clamp", 0, 1, &data);
//     if (data != 1) { ErrPrintf("2 split_rail connection error!\n"); return (1); }
//     InfoPrintf("22222222222222 \n");

//     Platform::EscapeWrite("Pex_pwr_goodn_IB", 0, 1, 1);
//     Platform::Delay(10);
//     Platform::EscapeRead("split_rail_sd__clamp", 0, 1, &data);
//     if (data != 0) { ErrPrintf("1 split_rail connection error!\n"); return (1); }
//     Platform::EscapeWrite("internal_overt_shutting_down_vr", 0, 1, 1);
////     lwgpu->SetRegFldDef("LW_THERM_OVERT_CTRL", "_INPUT_DRIVE_MODE", "_ON");
////     lwgpu->SetRegFldDef("LW_PPWR_SPLIT_RAIL", "_CARE_OVERT", "_YES");
////     Platform::EscapeWrite("overt_IB", 0, 1, 1);
//     Platform::Delay(10);
//     Platform::EscapeRead("split_rail_sd__clamp", 0, 1, &data);
//     if (data != 1) { ErrPrintf("3 split_rail connection error!\n"); return (1); }
//
//     Platform::EscapeWrite("internal_overt_shutting_down_vr", 0, 1, 0);
//     Platform::EscapeRead("split_rail_sd__clamp", 0, 1, &data);
//     if (data != 0) { ErrPrintf("1 split_rail connection error!\n"); return (1); }
//
//     lwgpu->SetRegFldDef("LW_PPWR_SPLIT_RAIL", "_CARE_OVERT", "_YES");
//     Platform::EscapeWrite("overt", 0, 1, 1);
////     Platform::EscapeWrite("overt_IB", 0, 1, 0);
//     Platform::Delay(10);
//     Platform::EscapeRead("split_rail_sd__clamp", 0, 1, &data);
//     if (data != 1) { ErrPrintf("4 split_rail connection error!\n"); return (1); }

     return(0);
}

//Basic test, read i2c slave registers default values
int i2cslave_fermi::testArp()
{
  InfoPrintf("testArp() starting\n");
  i2cPreArp();

  Platform::Delay(2);
  i2cRstArp();

  Platform::Delay(40);
//getuid
  Platform::Delay(40);
//setuid

  return(0);
}// end_testArp

int i2cslave_fermi::testOffset()
{
  InfoPrintf("testOffset() starting\n");
//step 1 pri write and read
     bool has_gk110_alert_regs;
     UINT32 dummy;
     UINT32 smbus_offset_addr;
     UINT32 smbus_offset_wr;
     UINT32 smbus_offset_rd;
     UINT32 smbus_offset_wr_dat = 0xef;

     // The thermal alerts moved to new registers in gk110.  We will check for
     // the existance of the new registers and set a flag to tell us where they
     // live.  The test will use this flag to determine where to find the priv
     // fields of interest.
     has_gk110_alert_regs = !lwgpu->GetRegNum("LW_THERM_EVENT_TRIGGER", &dummy);

     InfoPrintf("pri write and read start ...\n");

     if (has_gk110_alert_regs)
     {
         // the test assumes HW init routes all rupts to HOST
         lwgpu->SetRegFldNum("LW_THERM_EVENT_INTR_ROUTE", "", 0x0);
         lwgpu->SetRegFldNum("LW_THERM_INTR_ROUTE", "", 0x0);
         // HW init value changed from 125 C to 110 C when the register moved.
         // Write the value to 125 C since the test is hardcoded to expect this
         // HW init value.
         lwgpu->SetRegFldNum("LW_THERM_EVT_OVERT","", 0x7d);
     }
     if (lwgpu->GetRegNum("LW_THERM_I2CS_SENSOR_03",&smbus_offset_addr))
     { ErrPrintf("reg LW_THERM_I2CS_SENSOR_03 did not exist!\n");return (1);}
     InfoPrintf("smbus_offset addr 0x%08x\n",smbus_offset_addr);

     if (lwgpu->SetRegFldNum("LW_THERM_I2CS_SENSOR_03","",smbus_offset_wr_dat))
     { ErrPrintf("set LW_THERM_I2CS_SENSOR_03 value cannot be changed!\n");return (1);}
     InfoPrintf(" set smbus_offset value 0x%08x\n",smbus_offset_wr_dat);

     if (lwgpu->GetRegFldNum("LW_THERM_I2CS_SENSOR_03","",&smbus_offset_rd))
     { ErrPrintf("reg LW_THERM_I2CS_SENSOR_03 value cannot be read!\n");return (1);}
     InfoPrintf("read smbus_offset value 0x%08x\n",smbus_offset_rd);

//step 2 smbus write and read
     UINT32 pri_rd_dat;
     i2cPriEn(DEVICE_ADDR_0_FERMI);
     // smbus write offset reg
     InfoPrintf("smbus write msgbus_offset reg start ...\n ");
     smbus_offset_wr = 0x78;
     i2cWrByte(DEVICE_ADDR_0_FERMI,0x3,smbus_offset_wr,0);
     Platform::Delay(5);
     lwgpu->GetRegFldNum("LW_THERM_I2CS_SENSOR_03","",&pri_rd_dat);
     if (pri_rd_dat != smbus_offset_wr)
     { ErrPrintf("smbus offset reg LW_THERM_I2CS_SENSOR_03 write Error!\n");return (1);}

     // smbus read offset reg
     InfoPrintf("smbus read msgbus_offset reg start ...\n ");
     i2cRdByte(DEVICE_ADDR_0_FERMI,0x3,&smbus_offset_rd,0);
     Platform::Delay(5);
     if (smbus_offset_rd!= smbus_offset_wr)
     { ErrPrintf("smbus offset reg LW_THERM_I2CS_SENSOR_03 read Error!\n");return (1);}

//step 3 smbus offset interrupt
//     int offset_intr_host, offset_intr_pmu;
//     UINT32 pmc_intr_0, falcon_irq;
//     InfoPrintf("smbus offset interrupt start ...\n");
//
//     //test offset_intr disable
//     lwgpu->SetRegFldNum("LW_THERM_INTR_EN_0","",0x0);   //disable offset intr
//     Platform::Delay(1);
//     InfoPrintf("test offset_intr disable\n ");
//     lwgpu->GetRegFldNum("LW_PMC_INTR(0)", "", &pmc_intr_0);
//     Platform::EscapeRead("pmu_falcon_irqstat", 0x00, 1, &falcon_irq);
//     offset_intr_host = ((pmc_intr_0 & 0x00040000) != 0);
//     offset_intr_pmu  = ((falcon_irq & 0x1000) != 0);
//     if(offset_intr_host | offset_intr_pmu)
//     { ErrPrintf("offset intr should not be trigger, offset_intr enable is not set!\n");return (1);}
//
//     //test offset_intr route
//     InfoPrintf("enable offset_intr\n ");
//     lwgpu->SetRegFldNum("LW_THERM_INTR_EN_0","",0x00020000);   //enable offset intr
//     Platform::Delay(1);
//     InfoPrintf("test offset_intr route to host\n ");
//     lwgpu->GetRegFldNum("LW_PMC_INTR(0)", "", &pmc_intr_0);
//     Platform::EscapeRead("pmu_falcon_irqstat", 0x00, 1, &falcon_irq);
//     offset_intr_host = ((pmc_intr_0 & 0x00040000) != 0);
//     offset_intr_pmu  = ((falcon_irq & 0x1000) != 0);
//     if(offset_intr_host & !offset_intr_pmu)
//       {InfoPrintf("offset intr route to host correctly\n ");}
//     else
//       {ErrPrintf("offset intr should not route to PMU!\n");return (1);}
//
//     InfoPrintf("test offset_intr route to pmu\n ");
//     lwgpu->SetRegFldNum("LW_THERM_INTR_ROUTE","",0x00020000);  //set route to pmu
//     Platform::Delay(1);
//     lwgpu->GetRegFldNum("LW_PMC_INTR(0)", "", &pmc_intr_0);
//     Platform::EscapeRead("pmu_falcon_irqstat", 0x00, 1, &falcon_irq);
//     offset_intr_host = ((pmc_intr_0 & 0x00040000) != 0);
//     offset_intr_pmu  = ((falcon_irq & 0x1000) != 0);
//     if(!offset_intr_host & offset_intr_pmu)
//       {InfoPrintf("offset intr route to pmu correctly\n ");}
//     else
//       {ErrPrintf("offset intr should not route to HOST!\n");return (1);}
//
//     //test offset_intr clear
//     InfoPrintf("test offset_intr clear\n ");
//     if(offset_intr_host | offset_intr_pmu) {
//       lwgpu->SetRegFldNum("LW_THERM_INTR_0","", 0x00020000);    //clear therm intr register
//     }
//     Platform::Delay(1);
//     lwgpu->GetRegFldNum("LW_PMC_INTR(0)", "", &pmc_intr_0);
//     Platform::EscapeRead("pmu_falcon_irqstat", 0x00, 1, &falcon_irq);
//     offset_intr_host = ((pmc_intr_0 & 0x00040000) != 0);
//     offset_intr_pmu  = ((falcon_irq & 0x1000) != 0);
//     if(offset_intr_host | offset_intr_pmu)
//       {ErrPrintf("offset intr is not clear!\n");return (1);}
//     else
//       {InfoPrintf("offset intr clear correctly\n ");}
//
//     InfoPrintf("offset intr test finished\n ");
//     lwgpu->SetRegFldNum("LW_THERM_I2CS_SENSOR_12","", 0);
//
//     InfoPrintf("Disable reporting thermal event pending interrupt to the host/PMU.");
//     lwgpu->SetRegFldNum("LW_THERM_EVENT_INTR_ENABLE", "", 0);

//step 4 temp-offset control temp
//     lwgpu->SetRegFldDef("LW_THERM_SENSOR_0", "_POWER", "_ON");
//     UINT32 intr_0 = 0;
//
//     InfoPrintf("temp-offset control temp start ...\n ");
//
//     //==== test temp-offset affect alert1 ====
//     lwgpu->SetRegFldDef("LW_THERM_EVT_ALERT_1H", "_TEMP_THRESHOLD", "_110C"); // set threshold temperature of alter1h to default value 110C
//
//     Platform::EscapeWrite("tb_thermal_sensor_value", 0, 14, temp2code(109));     //set temp, make it lower than threshold of alter1h
//     Platform::Delay(2);
//     if (has_gk110_alert_regs)
//     {
//         lwgpu->SetRegFldDef("LW_THERM_EVENT_INTR_ON_TRIGGERED", "_ALERT_1H",
//             "_ENABLED");
//     }
//     else
//     {
//         //enable alter1h_intrrupt
//         lwgpu->SetRegFldDef("LW_THERM_CTRL_0", "_ALERT1H_INTERRUPT", "_RISING");
//     }
//     Platform::EscapeWrite("tb_thermal_sensor_override", 0, 1, 1);
//     Platform::Delay(1);
//     //set offset to 0, alter1h_intr should not be set
//     lwgpu->SetRegFldNum("LW_THERM_I2CS_SENSOR_12","", 0);
//     Platform::Delay(1);
//     if (has_gk110_alert_regs)
//     {
//         lwgpu->GetRegFldNum("LW_THERM_EVENT_INTR_PENDING", "_ALERT_1H",
//             &intr_0);
//     }
//     else
//     {
//         lwgpu->GetRegFldNum("LW_THERM_INTR_0", "_THERMAL_TRIGGER_ALERT1H",
//             &intr_0);
//     }
//     Platform::Delay(1);
//     if(intr_0)
//        {ErrPrintf("case 1: alter1h_intr should not be set, but it is set.\n");return (1);}
//     else
//        {InfoPrintf("alter1h_intr is not set when offset is 0x0\n ");}
//
//     //set offset to 2, alter1h_intr should be set
//     lwgpu->SetRegFldNum("LW_THERM_I2CS_SENSOR_12","", 2);
//     Platform::Delay(1);
//     if (has_gk110_alert_regs)
//     {
//         lwgpu->GetRegFldNum("LW_THERM_EVENT_INTR_PENDING", "_ALERT_1H",
//             &intr_0);
//     }
//     else
//     {
//         lwgpu->GetRegFldNum("LW_THERM_INTR_0", "_THERMAL_TRIGGER_ALERT1H",
//             &intr_0);
//     }
//     Platform::Delay(1);
//     if(intr_0)
//        {InfoPrintf("alter1h_intr is set when offset is 0x2\n ");}
//     else
//        {ErrPrintf("case 2: alter1h_intr should be set, but it is not.\n");return (1);}
//
//     lwgpu->SetRegFldNum("LW_THERM_I2CS_SENSOR_12","", 0);
//     InfoPrintf("temp-offset affect alert1 test finished\n ");
//
//     //==== test temp-offset affect alert2 ====
//     Platform::EscapeWrite("tb_thermal_sensor_value", 0, 14, temp2code(104));     //set temp, make it lower than threshold of alter2h
//     Platform::Delay(2);
//     if (has_gk110_alert_regs)
//     {
//         lwgpu->SetRegFldDef("LW_THERM_EVENT_INTR_ON_TRIGGERED", "_ALERT_2H",
//             "_ENABLED");
//     }
//     else
//     {
//         //enable alter2h_intrrupt
//         lwgpu->SetRegFldDef("LW_THERM_CTRL_2", "_ALERT2H_INTERRUPT", "_RISING");
//     }
//     Platform::EscapeWrite("tb_thermal_sensor_override", 0, 1, 1);
//     Platform::Delay(1);
//     //set offset to 0, alter2h_intr should not be set
//     lwgpu->SetRegFldNum("LW_THERM_I2CS_SENSOR_12","", 0);
//     Platform::Delay(1);
//     if (has_gk110_alert_regs)
//     {
//         lwgpu->GetRegFldNum("LW_THERM_EVENT_INTR_PENDING", "_ALERT_2H",
//             &intr_0);
//     }
//     else
//     {
//         lwgpu->GetRegFldNum("LW_THERM_INTR_0", "_THERMAL_TRIGGER_ALERT2H",
//             &intr_0);
//     }
//     Platform::Delay(1);
//     if(intr_0)
//        {ErrPrintf("case 3: alter2h_intr should not be set, but it is set.\n");return (1);}
//     else
//        {InfoPrintf("alter2h_intr is not set when offset is 0x0\n ");}
//
//     //set offset to 2, alter2h_intr should be set
//     lwgpu->SetRegFldNum("LW_THERM_I2CS_SENSOR_12","", 2);
//     Platform::Delay(1);
//     if (has_gk110_alert_regs)
//     {
//         lwgpu->GetRegFldNum("LW_THERM_EVENT_INTR_PENDING", "_ALERT_2H",
//             &intr_0);
//     }
//     else
//     {
//         lwgpu->GetRegFldNum("LW_THERM_INTR_0", "_THERMAL_TRIGGER_ALERT2H",
//             &intr_0);
//     }
//     Platform::Delay(1);
//     if(intr_0)
//        {InfoPrintf("alter2h_intr is set when offset is 0x2\n ");}
//     else
//        {ErrPrintf("case 4: alter2h_intr should be set, but it is not.\n");return (1);}
//
//     lwgpu->SetRegFldNum("LW_THERM_I2CS_SENSOR_12","", 0);
//     InfoPrintf("temp-offset affect alert2 test finished\n ");
//
//     //==== test temp-offset affect alert3 ====
//     lwgpu->SetRegFldDef("LW_THERM_I2CS_SENSOR_38", "_ALERT3H", "_INIT");
//     Platform::EscapeWrite("tb_thermal_sensor_value", 0, 14, temp2code(101));     //set temp, make it lower than threshold of alter3h
//     Platform::Delay(2);
//     if (has_gk110_alert_regs)
//     {
//         lwgpu->SetRegFldDef("LW_THERM_EVENT_INTR_ON_TRIGGERED", "_ALERT_3H",
//             "_ENABLED");
//     }
//     else
//     {
//         //enable alter3h_intrrupt
//         lwgpu->SetRegFldDef("LW_THERM_CTRL_2", "_ALERT3H_INTERRUPT", "_RISING");
//     }
//     Platform::EscapeWrite("tb_thermal_sensor_override", 0, 1, 1);
//     Platform::Delay(1);
//     //set offset to 0, alter3h_intr should not be set
//     lwgpu->SetRegFldNum("LW_THERM_I2CS_SENSOR_12","", 0);
//     Platform::Delay(1);
//     if (has_gk110_alert_regs)
//     {
//         lwgpu->GetRegFldNum("LW_THERM_EVENT_INTR_PENDING", "_ALERT_3H",
//             &intr_0);
//     }
//     else
//     {
//         lwgpu->GetRegFldNum("LW_THERM_INTR_0", "_THERMAL_TRIGGER_ALERT3H",
//             &intr_0);
//     }
//     Platform::Delay(1);
//     if(intr_0)
//        {ErrPrintf("case 5: alter3h_intr should not be set, but it is set.\n");return (1);}
//     else
//        {InfoPrintf("alter3h_intr is not set when offset is 0x0\n ");}
//
//     //set offset to 2, alter3h_intr should be set
//     lwgpu->SetRegFldNum("LW_THERM_I2CS_SENSOR_12","", 2);
//     Platform::Delay(1);
//     if (has_gk110_alert_regs)
//     {
//         lwgpu->GetRegFldNum("LW_THERM_EVENT_INTR_PENDING", "_ALERT_3H",
//             &intr_0);
//     }
//     else
//     {
//         lwgpu->GetRegFldNum("LW_THERM_INTR_0", "_THERMAL_TRIGGER_ALERT3H",
//             &intr_0);
//     }
//     Platform::Delay(1);
//     if(intr_0)
//        {InfoPrintf("alter3h_intr is set when offset is 0x2\n ");}
//     else
//        {ErrPrintf("case 6: alter3h_intr should be set, but it is not.\n");return (1);}
//
//     lwgpu->SetRegFldNum("LW_THERM_I2CS_SENSOR_12","", 0);
//     InfoPrintf("temp-offset affect alert3 test finished\n ");
//
//     //==== test temp-offset affect alert4 ====
//     Platform::EscapeWrite("tb_thermal_sensor_value", 0, 14, temp2code(96));     //set temp, make it lower than threshold of alter4h
//     Platform::Delay(2);
//     if (has_gk110_alert_regs)
//     {
//         lwgpu->SetRegFldDef("LW_THERM_EVENT_INTR_ON_TRIGGERED", "_ALERT_4H",
//             "_ENABLED");
//     }
//     else
//     {
//         //enable alter4h_intrrupt
//         lwgpu->SetRegFldDef("LW_THERM_CTRL_2", "_ALERT4H_INTERRUPT", "_RISING");
//     }
//     Platform::EscapeWrite("tb_thermal_sensor_override", 0, 1, 1);
//     Platform::Delay(1);
//     //set offset to 0, alter4h_intr should not be set
//     lwgpu->SetRegFldNum("LW_THERM_I2CS_SENSOR_12","", 0);
//     Platform::Delay(1);
//     if (has_gk110_alert_regs)
//     {
//         lwgpu->GetRegFldNum("LW_THERM_EVENT_INTR_PENDING", "_ALERT_4H",
//             &intr_0);
//     }
//     else
//     {
//         lwgpu->GetRegFldNum("LW_THERM_INTR_0", "_THERMAL_TRIGGER_ALERT4H",
//             &intr_0);
//     }
//     Platform::Delay(1);
//     if(intr_0)
//        {ErrPrintf("case 7: alter4h_intr should not be set, but it is set.\n");return (1);}
//     else
//        {InfoPrintf("alter4h_intr is not set when offset is 0x0\n ");}
//
//     //set offset to 2, alter4h_intr should be set
//     lwgpu->SetRegFldNum("LW_THERM_I2CS_SENSOR_12","", 2);
//     Platform::Delay(1);
//     if (has_gk110_alert_regs)
//     {
//         lwgpu->GetRegFldNum("LW_THERM_EVENT_INTR_PENDING", "_ALERT_4H",
//             &intr_0);
//     }
//     else
//     {
//         lwgpu->GetRegFldNum("LW_THERM_INTR_0", "_THERMAL_TRIGGER_ALERT4H",
//             &intr_0);
//     }
//     Platform::Delay(1);
//     if(intr_0)
//        {InfoPrintf("alter4h_intr is set when offset is 0x2\n ");}
//     else
//        {ErrPrintf("case 8: alter4h_intr should be set, but it is not.\n");return (1);}
//
//     lwgpu->SetRegFldNum("LW_THERM_I2CS_SENSOR_12","", 0);
//     InfoPrintf("temp-offset affect alert4 test finished\n ");
//
//     //==== test temp-offset will not affect alert0 =====
//     if (has_gk110_alert_regs)
//     {
//         lwgpu->SetRegFldNum("LW_THERM_EVT_ALERT_0H","", 0x50);
//     }
//     else
//     {
//         lwgpu->SetRegFldNum("LW_THERM_I2CS_SENSOR_05","", 0x50);
//     }
//     Platform::EscapeWrite("tb_thermal_sensor_value", 0, 14, temp2code(79));     //set temp, make it lower than threshold of rlhn
//     Platform::Delay(2);
//     if (has_gk110_alert_regs)
//     {
//         lwgpu->SetRegFldDef("LW_THERM_EVENT_INTR_ON_TRIGGERED", "_ALERT_0H",
//             "_ENABLED");
//     }
//     else
//     {
//         //enable rlhn_intrrupt
//         lwgpu->SetRegFldDef("LW_THERM_CTRL_0", "_RLHN_INTERRUPT", "_RISING");
//     }
//
//     Platform::EscapeWrite("tb_thermal_sensor_override", 0, 1, 1);
//     Platform::Delay(1);
//     //set offset to 0, rlhn_intr should not be set
//     lwgpu->SetRegFldNum("LW_THERM_I2CS_SENSOR_12","", 0);
//     Platform::Delay(1);
//     if (has_gk110_alert_regs)
//     {
//         lwgpu->GetRegFldNum("LW_THERM_EVENT_INTR_PENDING", "_ALERT_0H",
//             &intr_0);
//     }
//     else
//     {
//         lwgpu->GetRegFldNum("LW_THERM_INTR_0", "_THERMAL_TRIGGER_RLHN",
//             &intr_0);
//     }
//     Platform::Delay(1);
//     if(intr_0)
//        {ErrPrintf("case 9: rlhn_intr should not be set, but it is set.\n");return (1);}
//     else
//        {InfoPrintf("rlhn_intr is not set when offset is 0x0\n ");}
//
//     //set offset to 2, rlhn_intr also should not be set, because temp-offset won't affect alert0
//     lwgpu->SetRegFldNum("LW_THERM_I2CS_SENSOR_12","", 2);
//     Platform::Delay(1);
//     if (has_gk110_alert_regs)
//     {
//         lwgpu->GetRegFldNum("LW_THERM_EVENT_INTR_PENDING", "_ALERT_0H",
//             &intr_0);
//     }
//     else
//     {
//         lwgpu->GetRegFldNum("LW_THERM_INTR_0", "_THERMAL_TRIGGER_RLHN",
//             &intr_0);
//     }
//     Platform::Delay(1);
//     if(intr_0)
//        {ErrPrintf("case 10: rlhn_intr should not be set, but it is set.\n");return (1);}
//     else
//        {InfoPrintf("rlhn_intr is not set even offset is 0x2, because temp-offset won't affect rlhn\n ");}
//
//     lwgpu->SetRegFldNum("LW_THERM_I2CS_SENSOR_12","", 0);
//
//     //==== test temp-offset will not affect overt =====
//     Platform::EscapeWrite("tb_thermal_sensor_value", 0, 14, temp2code(124));     //set temp, make it lower than threshold of overt
//     Platform::Delay(2);
//     if (has_gk110_alert_regs)
//     {
//         lwgpu->SetRegFldDef("LW_THERM_EVENT_INTR_ON_TRIGGERED", "_OVERT",
//             "_ENABLED");
//     }
//     else
//     {
//         //enable overt_intrrupt
//         lwgpu->SetRegFldDef("LW_THERM_CTRL_0", "_OVERT_INTERRUPT", "_RISING");
//     }
//
//     Platform::EscapeWrite("tb_thermal_sensor_override", 0, 1, 1);
//     Platform::Delay(1);
//     //set offset to 0, overt_intr should not be set
//     lwgpu->SetRegFldNum("LW_THERM_I2CS_SENSOR_12","", 0);
//     Platform::Delay(1);
//     if (has_gk110_alert_regs)
//     {
//         lwgpu->GetRegFldNum("LW_THERM_EVENT_INTR_PENDING", "_OVERT",
//             &intr_0);
//     }
//     else
//     {
//         lwgpu->GetRegFldNum("LW_THERM_INTR_0", "_THERMAL_TRIGGER_OVERT",
//             &intr_0);
//     }
//     Platform::Delay(1);
//     if(intr_0)
//        {ErrPrintf("case 11 : overt_intr should not be set, but it is set.\n");return (1);}
//     else
//        {InfoPrintf("overt_intr is not set when offset is 0x0\n ");}
//
//     //set offset to 2, overt_intr also should not be set, because temp-offset won't affect overt
//     lwgpu->SetRegFldNum("LW_THERM_I2CS_SENSOR_12","", 2);
//     Platform::Delay(1);
//     if (has_gk110_alert_regs)
//     {
//         lwgpu->GetRegFldNum("LW_THERM_EVENT_INTR_PENDING", "_OVERT",
//             &intr_0);
//     }
//     else
//     {
//         lwgpu->GetRegFldNum("LW_THERM_INTR_0", "_THERMAL_TRIGGER_OVERT",
//             &intr_0);
//     }
//     Platform::Delay(1);
//     if(intr_0)
//        {ErrPrintf("case 12: overt_intr should not be set, but it is set.\n");return (1);}
//     else
//        {InfoPrintf("overt_intr is not set even offset is 0x2, because temp-offset won't affect overt\n ");}
//
//     lwgpu->SetRegFldNum("LW_THERM_I2CS_SENSOR_12","", 0);
//
//     //==== test temp-offset will not affect alert5=====
//     UINT32 data;
//     InfoPrintf("Config GPIO[9] start ...\n ");
//     if((m_arch & 0xff0) == 0x400){     //gf10x
//        if (lwgpu->SetRegFldDef("LW_PMGR_ALT_GPIO", "_GPIO_9", "_ALTERNATE")) {ErrPrintf("could not write LW_PMGR_ALT_GPIO_GPIO_9\n"); return(1);}
//        if (lwgpu->SetRegFldDef("LW_PMGR_ALT2_GPIO", "_GPIO_9_OPEN_DRAIN", "_ENABLE")) {ErrPrintf("could not write LW_PMGR_ALT2_GPIO_GPIO_9_OPEN_DRAIN\n"); return(1);}
//     }
//     else{     //gf11x
//        if (lwgpu->SetRegFldDef("LW_PMGR_GPIO_9_OUTPUT_CNTL", "_SEL", "_FAN_ALERT")) { ErrPrintf("could not update LW_PMGR_GPIO_9_OUTPUT_CNTL"); }
//        if (lwgpu->SetRegFldDef("LW_PMGR_GPIO_9_OUTPUT_CNTL", "_OPEN_DRAIN", "_ENABLE")) {ErrPrintf("could not write LW_PMGR_GPIO_9_OUTPUT_CNTL_OPEN_DRAIN\n"); return(1);}
//        if (lwgpu->SetRegFldDef("LW_PMGR_GPIO_OUTPUT_CNTL_TRIGGER", "_UPDATE", "_TRIGGER")) {ErrPrintf("could not write LW_PMGR_GPIO_OUTPUT_CNTL_TRIGGER\n"); return(1);}
//     }
//     if (lwgpu->SetRegFldDef("LW_PMGR_FAN1", "_ALERT_OUTPUT_OVERRIDE", "_ENABLED")) {ErrPrintf("could not write LW_PMGR_FAN1_ALERT_OUTPUT_OVERRIDE\n"); return(1);}
//     if (lwgpu->SetRegFldDef("LW_PMGR_FAN1", "_ALERT_OUTPUT_POLARITY", "_HI")) {ErrPrintf("could not write LW_PMGR_FAN1_ALERT_OUTPUT_POLARITY\n"); return(1);}
//
//     lwgpu->SetRegFldDef("LW_THERM_CTRL_0", "_ALERT_CTRL", "_DISABLED");     //disable alert_ctrl
//     lwgpu->SetRegFldNum("LW_THERM_I2CS_SENSOR_09","", 0x2);                 //enable i2cs_sensor_09_rcl_alert5h_en, so that GPIO_9 would only be triggered by alert5h
//
//     InfoPrintf("test alert5h enable GPIO[9]");   //In step 2, alert5_en has been set to 0x2
//     Platform::EscapeWrite("tb_thermal_sensor_value", 0, 14, temp2code(86));
//     Platform::EscapeWrite("tb_thermal_sensor_override", 0, 1, 1);
//     Platform::Delay(1);
//     Platform::EscapeRead("Gpio_9", 0, 1, &data);
//     if (data != 1) { ErrPrintf("GPIO[9] isn't 1. case 13.\n"); return (1); }
//
//     Platform::EscapeWrite("tb_thermal_sensor_value", 0, 14, temp2code(84));
//     Platform::EscapeWrite("tb_thermal_sensor_override", 0, 1, 1);
//     //set offset to 0, GPIO_9 should not be set
//     lwgpu->SetRegFldNum("LW_THERM_I2CS_SENSOR_12","", 0);
//     Platform::Delay(1);
//     Platform::EscapeRead("Gpio_9", 0, 1, &data);
//     if (data != 0) { ErrPrintf("GPIO[9] isn't 0. case 14.\n"); return (1); }
//
//
//     //set offset to 2, GPIO_9 also should not be set because temp-offset won't affect alert5
//     lwgpu->SetRegFldNum("LW_THERM_I2CS_SENSOR_12","", 0);
//     Platform::Delay(1);
//     Platform::EscapeRead("Gpio_9", 0, 1, &data);
//     if (data != 0) { ErrPrintf("GPIO[9] isn't 0. case 15.\n"); return (1); }
//     else {InfoPrintf("gpio_9 should not be set even offset is 0x2, because temp-offset won't affect overt\n ");}
//
//     // Clear all the interrupt pending bits
//     Platform::Delay(1);
//     lwgpu->SetRegFldNum("LW_THERM_EVENT_INTR_PENDING","", 0xffffffff);
//     InfoPrintf("All the THERM interrupt should be cleared done, and the test can exit normally \n");
//
//
//     Platform::EscapeWrite("tb_thermal_sensor_value", 0, 14, temp2code(25));
//     InfoPrintf("Disable event_trigger_alert_oh by forcing the chip temp < 50C \n");
//
  return(0);
}// end_testOffset

//smb_alt_addr test (bug524243)
//when smb_alt_addr is 1'b1, smb address will be set to I2C_ADDR_2
int i2cslave_fermi::test_smb_alt_addr()
{
    InfoPrintf("Check smb_alt_addr starting ....\n");

    UINT32   expect_value;
//    UINT32   boot3_value;
    UINT32   i2c_rdat;

    //set smbus address to be ADDR_0 through fuse;
    Platform::EscapeWrite("drv_opt_i2cs_addr_sel", 0, 2, 0x0);
    lwgpu->GetRegFldNum("LW_THERM_I2CS_SENSOR_TSCFG","_BANDGAP_VOLT",  &expect_value);
    i2cRdByte(DEVICE_ADDR_0_FERMI,0x32,&i2c_rdat,0);
    InfoPrintf("I2C_Addr DEVICE_ADDR_0_FERMI  read back value 0x%08x\n",i2c_rdat);
    if ( i2c_rdat != expect_value)
    {
        ErrPrintf("DEV_ADDR_0 -- read data mismatch, expect 0x%08x , got 0x%08x\n", expect_value,i2c_rdat);
        return (1);
    }
    Platform::Delay(100);

    //set smb_alt_addr to 1'b1, which will set i2c address to DEVICE_ADDR_1_FERMI
//    lwgpu->GetRegFldNum("LW_PEXTDEV_BOOT_3","",  &boot3_value);
//    boot3_value = boot3_value | 1<<STRAP_SMB_ALT_ADDR | 1<<STRAP_OVERWRITE;
//    lwgpu->SetRegFldNum("LW_PEXTDEV_BOOT_3","",  boot3_value);
    lwgpu->SetRegFldDef("LW_PGC6_SCI_STRAP_OVERRIDE", "_SMB_ALT_ADDR", "_ENABLED");
    lwgpu->SetRegFldDef("LW_PGC6_SCI_STRAP_MASK_OVERRIDE", "_SMB_ALT_ADDR", "_ENABLED");
    Platform::Delay(10);

    lwgpu->GetRegFldNum("LW_THERM_I2CS_SENSOR_TSCFG","_BANDGAP_VOLT",  &expect_value);
    i2cRdByte(DEVICE_ADDR_1_FERMI,0x32,&i2c_rdat,0);
    InfoPrintf("I2C_Addr DEVICE_ADDR_1_FERMI  read back value 0x%08x\n",i2c_rdat);
    if ( i2c_rdat != expect_value)
    {
        ErrPrintf("DEV_ADDR_1 -- read data mismatch, expect 0x%08x , got 0x%08x\n", expect_value,i2c_rdat);
        return (1);
    }

    //set smb_alt_addr to 1'b0
//    lwgpu->GetRegFldNum("LW_PEXTDEV_BOOT_3","",  &boot3_value);
//    boot3_value = boot3_value & (~(1<<STRAP_SMB_ALT_ADDR));
//    boot3_value = boot3_value | 1<<STRAP_OVERWRITE;
//    lwgpu->SetRegFldNum("LW_PEXTDEV_BOOT_3","",  boot3_value);
    lwgpu->SetRegFldDef("LW_PGC6_SCI_STRAP_OVERRIDE", "_SMB_ALT_ADDR", "_DISABLED");
    lwgpu->SetRegFldDef("LW_PGC6_SCI_STRAP_MASK_OVERRIDE", "_SMB_ALT_ADDR", "_DISABLED");
    Platform::Delay(10);

    //Verify ADDR_0 will be used
    lwgpu->GetRegFldNum("LW_THERM_I2CS_SENSOR_TSCFG","_BANDGAP_VOLT",  &expect_value);
    i2cRdByte(DEVICE_ADDR_0_FERMI,0x32,&i2c_rdat,0);
    InfoPrintf("I2C_Addr DEVICE_ADDR_0_FERMI  read back value 0x%08x\n",i2c_rdat);
    if ( i2c_rdat != expect_value)
    {
        ErrPrintf("DEV_ADDR_0 -- read data mismatch, expect 0x%08x , got 0x%08x\n", expect_value,i2c_rdat);
        return (1);
    }

    return(0);
}// end_test_smb_alt_addr

int i2cslave_fermi::WriteDacBgapCntl(int index, UINT32 newFieldValue)
{
    unique_ptr<IRegisterClass> pReg = lwgpu->GetRegister("LW_PDISP_DAC_BGAP");
    if (!pReg)
    {
        ErrPrintf("Could not GetRegister(LW_PDISP_DAC_BGAP)!\n");
        return (1);
    }

    unique_ptr<IRegisterField> pField = pReg->FindField("LW_PDISP_DAC_BGAP_CNTL");
    if (!pField)
    {
        ErrPrintf("Could not FindField(LW_PDISP_DAC_BGAP_CNTL)!\n");
        return (1);
    }

    UINT32 NewValue = newFieldValue << pField->GetStartBit();
    UINT32 Mask  = pField->GetWriteMask();

    UINT32 Value = lwgpu->RegRd32(pReg->GetAddress(index));

    Value = (Value & ~Mask) | NewValue;

    lwgpu->RegWr32(pReg->GetAddress(index), Value);

    return 0;
}
