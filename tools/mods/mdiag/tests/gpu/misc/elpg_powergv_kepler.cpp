/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2010-2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

/*-----------------------------------------------------------------------------------------------
Test :    elpg_powergv_kepler.cpp
Purpose:  A fast test which completes within 20 mins on full-chip RTL. We add -skipdevinit for this.
          This test is to check sleep-enable connections in full-chip flat gates are working.
Author:   Saket Jamkar
------------------------------------------------------------------------------------------------- */
#include "mdiag/tests/stdtest.h"
#include "core/include/rc.h"
#include "elpg_powergv_kepler.h"
#include "mdiag/lwgpu.h"
#include "mdiag/utils/lwgpu_channel.h"
#include "mdiag/utils/crc.h"
#include "mdiag/tests/test_state_report.h"
#include "core/include/utility.h"
#include "core/include/gpu.h"
#include "lwmisc.h"
#include "gpu/include/gpusbdev.h"
#include "check_clamp.h"

//Includes for register address defines from //.../drivers/resman/kernel/inc/fermi/
#include "kepler/gk110/hwproject.h"
#include "regmacro.h"
#include "kepler/gk110/dev_graphics_nobundle.h"
#include "pascal/gp10b/dev_fifo.h"
#include "pascal/gp10b/dev_master.h"
#include "kepler/gk104/dev_bus.h"
#include "kepler/gk104/dev_pri_ringmaster.h"
#include "kepler/gk104/dev_pri_ringstation_sys.h"
#include "kepler/gk104/dev_xbar.h"
//Removed kepler dev_top to avoid conflicting defines with ampere dev_top
//#include "kepler/gk104/dev_top.h"
#include "kepler/gk104/dev_therm.h"
#include "kepler/gk104/dev_msenc_pri.h"
#include "kepler/gk104/dev_mspdec_pri.h"
#include "kepler/gk104/dev_msvld_pri.h"
#include "kepler/gk104/dev_msppp_pri.h"
#include "kepler/gk104/dev_ltc.h"
#include "kepler/gk104/dev_fbpa.h"
#include "kepler/gk104/dev_lw_xve.h"
//#include "volta/gv11b/dev_pwr_pri.h"
//#include "volta/gv11b/dev_chiplet_pwr.h"
//#include "volta/gv11b/dev_fb.h"
#include "turing/tu102/dev_pwr_pri.h"
#include "turing/tu102/dev_chiplet_pwr.h"
#include "turing/tu102/dev_fb.h"
// Included for LW_RUNLIST_* registers. (Ampere-10)
#include "ampere/ga102/dev_runlist.h"
#include "ampere/ga102/dev_top.h"
#include "ampere/ga102/dev_fuse.h"

#define DEBUG_PRINT "ELPG_SPEEDY"
#define TEST_NAME     "elpg_powergv_kepler.cpp"
#define START1 0x0

//fuse addr and enable_data
#define msdec_fspg_enable_addr  0x1b
#define msdec_fspg_enable_mask  0x100000
#define tpc_fspg_enable_addr    0x1b
#define tpc_fspg_enable_mask    0x80000

#define WRITE_FUSE_DATA(d,r,f,n,wr0,wr1,wr2,wr3,wr4,wr5,wr6,wr7,wr8,wr9,wr10,wr11,wr12,wr13,wr14,wr15,wr16,wr17,wr18,wr19,wr20,wr21,wr22,wr23,wr24,wr25,wr26,wr27)    writeFusedata( LW##d##r, (DRF_MASK(LW##d##r##f)<<DRF_SHIFT(LW##d##r##f)), DRF_NUM(d,r,f,n), wr0,wr1,wr2,wr3,wr4,wr5,wr6,wr7,wr8,wr9,wr10,wr11,wr12,wr13,wr14,wr15,wr16,wr17,wr18,wr19,wr20,wr21,wr22,wr23,wr24,wr25,wr26,wr27)
#define WRITE_FUSE_DATA_DEFINE(d,r,f,n) WRITE_FUSE_DATA(d,r,f,n,fuseVal0_write,fuseVal1_write,fuseVal2_write,fuseVal3_write,fuseVal4_write,fuseVal5_write,fuseVal6_write,fuseVal7_write,fuseVal8_write,fuseVal9_write,fuseVal10_write,fuseVal11_write,fuseVal12_write,fuseVal13_write,fuseVal14_write,fuseVal15_write,fuseVal16_write,fuseVal17_write,fuseVal18_write,fuseVal19_write,fuseVal20_write,fuseVal21_write,fuseVal22_write,fuseVal23_write,fuseVal24_write,fuseVal25_write,fuseVal26_write,fuseVal27_write)

//int err = 0;
extern const ParamDecl elpg_powergv_kepler_params[] = {
  SIMPLE_PARAM("-register_test", " Check ELPG Registers"),
  SIMPLE_PARAM("-railgating", "Check Railgating Entry"),
  SIMPLE_PARAM("-randomize_regs", " Randomize PG Registers"),
  SIMPLE_PARAM("-pgob", " Check power gating on boot"),
  SIMPLE_PARAM("-gxpg", " Check graphic engine power gating"),
  SIMPLE_PARAM("-vdpg", " Check video engine power gating"),
  SIMPLE_PARAM("-mspg", " Check memory system power gating"),
  SIMPLE_PARAM("-bbox_verify", " Verify BBOX module"),
  //For more information on Centralized ELPG, refer to https://p4viewer.lwpu.com/get///hw/doc/gpu/maxwell/maxwell/design/IAS/power/Maxwell_PowerGating_IAS.docx
  SIMPLE_PARAM("-centralized_elpg", " Enable Centralized power gating"),
  UNSIGNED_PARAM("-seed", " Seed for the random number generation"),
  UNSIGNED_PARAM("-fspg_option", " FSPG Configuration Value"),
  UNSIGNED_PARAM("-fs_control", " LW_PTOP_FS_CONTROL Register Configuration Value"),
  UNSIGNED_PARAM("-fs_control_gpc", " LW_PTOP_FS_CONTROL_GPC Register Configuration Value"),
  UNSIGNED_PARAM("-fs_control_gpc_tpc", " LW_PTOP_FS_CONTROL_GPC_TPC Register Configuration Value"),
  UNSIGNED_PARAM("-fs_control_gpc_zlwll", " LW_PTOP_FS_CONTROL_GPC_ZLWLL Register Configuration Value"),
  PARAM_SUBDECL(lwgpu_single_params),
  LAST_PARAM
}
;

//-----------------------------------------------------------
// Centralized ELPG register defines
//-----------------------------------------------------------
#define REG_DEFINE 0                       //Field index of reg_list for reg define
#define MASK_VAL   1                       //Field index of reg_list for mask value
#define RESET_VAL  2                       //Field index of reg_list for reset value
#define PROD_VAL   3                       //Field index of reg_list for prod value
// Defining the below macros to -1 to disable reset checks
//#define REG_LIST_INDEX_MAX 39              //Max index of reg_list array
//#define PER_GPC_ELPG_NUM_PARTITIONS_MAX 27 //Number of physical GPC partitions-1 (Define ported from hw TOT/include/gmlit3/project.mk, generated from //hw/lwgpu/defs/projects.spec)
//#define PER_GPC_PGRAM_NUM_PARTITIONS_MAX 27 //Number of PGRAM GPC partitions-1 Same as above ELPG_NUM_PARTITIONS_MAX
//#define PER_GPC_NPGRAM_NUM_PARTITIONS_MAX 0 //Number of NPGRAM GPC partitions-1
#define REG_LIST_INDEX_MAX -1              //Max index of reg_list array
#define PER_GPC_ELPG_NUM_PARTITIONS_MAX -1 //Number of physical GPC partitions-1 (Define ported from hw TOT/include/gmlit3/project.mk, generated from //hw/lwgpu/defs/projects.spec)
#define PER_GPC_PGRAM_NUM_PARTITIONS_MAX -1 //Number of PGRAM GPC partitions-1 Same as above ELPG_NUM_PARTITIONS_MAX
#define PER_GPC_NPGRAM_NUM_PARTITIONS_MAX -1 //Number of NPGRAM GPC partitions-1
//-----------------------------------------------------------
// Centralized ELPG register arrays
//-----------------------------------------------------------
unsigned int reg_list[][4] = {
    // Note: Last 2 registers should always be GPC_ELPG_INDEX & GPC_ELPG_PSORDER as they use special indexed addressing
    // General format: {Register define, Mask, Reset value, PROD value} //reg_list[array_index]
    // FBP RAM Registers
//    { LW_PCHIPLET_PWR_FBPS_NPGRAM_REGSEL          , 0xFFFFFFFF, 0x00000000, 0x00000000 },
//    { LW_PCHIPLET_PWR_FBPS_NPGRAM_PSORDER         , 0x0000BFBF, 0x00008180, 0x00008180 },
//    { LW_PCHIPLET_PWR_FBPS_PGRAM_REGSEL           , 0xFFFFFFFF, 0x00000000, 0x00000000 },
//    { LW_PCHIPLET_PWR_FBPS_PGRAM_PSORDER          , 0x0000BFBF, 0x00008180, 0x81818080 },
//  // GPC RAM Registers
//    { LW_PCHIPLET_PWR_GPCS_NPGRAM_REGSEL           , 0xFFFFFFFF, 0x00000000, 0x00000000 },
//    { LW_PCHIPLET_PWR_GPCS_PGRAM_REGSEL            , 0xFFFFFFFF, 0x00000000, 0x00000000 },
//
//  // SYS RAM Registers
//    { LW_PPWR_PMU_SYS_PGRAM_PSORDER                , 0x0000BFBF, 0x00008180, 0x00008180 },
//    { LW_PPWR_PMU_SYS_PGRAM_REGSEL                 , 0xFFFFFFFF, 0x00000000, 0x00000000 },
//    { LW_PPWR_PMU_SYS_NPGRAM_PSORDER               , 0x0000BFBF, 0x00008180, 0x00818180 },
//    { LW_PPWR_PMU_SYS_NPGRAM_REGSEL                , 0xFFFFFFFF, 0x00000000, 0x00000000 },
//
//  // SYS Registers
//  { LW_PPWR_PMU_SYS_ELPG_PSORDER                , 0x0000BFBF, 0x00008180, 0x00008180 }, //reg_list[0]
//  { LW_PPWR_PMU_SYS_ELPG_REGSEL0                , 0xFFFFFFFF, 0x00000000, 0x00000000 }, //reg_list[1]
//  { LW_PPWR_PMU_SYS_ELPG_PDELAY                 , 0x003F00FF, 0x01010155, 0x02010155 }, //reg_list[2]
//  { LW_PPWR_PMU_SYS_ELPG_ZSORDER0               , 0xFFFFFFFF, 0x89ABCDEF, 0x89ABCDEF }, //reg_list[3]
//  { LW_PPWR_PMU_SYS_ELPG_ZSORDER1               , 0xFFFFFFFF, 0x89ABCDEF, 0x00000000 }, //reg_list[4]
//  { LW_PPWR_PMU_SYS_ELPG_ZDELAY0                , 0x7FFFFFFF, 0x75555555, 0x75555555 }, //reg_list[5]
//  { LW_PPWR_PMU_SYS_ELPG_ZDELAY1                , 0x7FFFFFFF, 0x75555555, 0x70000000 }, //reg_list[6]
//  { LW_PPWR_PMU_SYS_ELPG_DEBUG_KEEP_CLAMP       , 0x00000001, 0x00000000, 0x00000000 }, //reg_list[7]
    { LW_PPWR_PMU_ELPG_SEQ_CG                     , 0x00000001, 0x00000000, 0x00000001 }, //reg_list[8]
    { LW_THERM_CTRL_1                             , 0xC0000000, 0x00000000, 0x00000000 }, //reg_list[9]
    // FBP Registers
//  { LW_PCHIPLET_PWR_FBPS_ELPG_PSORDER           , 0x0000BFBF, 0x00008180, 0x81818080 }, //reg_list[10]
//  { LW_PCHIPLET_PWR_FBPS_ELPG_REGSEL0           , 0xFFFFFFFF, 0x00000000, 0x00000000 }, //reg_list[11]
//  { LW_PCHIPLET_PWR_FBPS_ELPG_PDELAY            , 0x003F00FF, 0x03030355, 0x01030355 }, //reg_list[12]
//  { LW_PCHIPLET_PWR_FBPS_ELPG_ZSORDER0          , 0xFFFFFFFF, 0x89ABCDEF, 0x89ABCDEF }, //reg_list[13]
//  { LW_PCHIPLET_PWR_FBPS_ELPG_ZSORDER1          , 0xFFFFFFFF, 0x89ABCDEF, 0x00000000 }, //reg_list[14]
//  { LW_PCHIPLET_PWR_FBPS_ELPG_ZDELAY0           , 0x7FFFFFFF, 0x75555555, 0x75555555 }, //reg_list[15]
//  { LW_PCHIPLET_PWR_FBPS_ELPG_ZDELAY1           , 0x7FFFFFFF, 0x75555555, 0x70000000 }, //reg_list[16]
//  { LW_PCHIPLET_PWR_FBPS_ELPG_DEBUG_KEEP_CLAMP  , 0x00000001, 0x00000000, 0x00000000 }, //reg_list[17]
    // GPC Registers
//  { LW_PCHIPLET_PWR_GPCS_ELPG_REGSEL0           , 0xFFFFFFFF, 0x00000000, 0x00000000 }, //reg_list[18]
//  { LW_PCHIPLET_PWR_GPCS_ELPG_REGSEL1           , 0xFFFFFFFF, 0xFFFFFFFF, 0x00000000 }, //reg_list[19]
//  { LW_PCHIPLET_PWR_GPCS_ELPG_PDELAY            , 0x003F00FF, 0x1b1b1b55, 0x001b1b55 }, //reg_list[20]
//  { LW_PCHIPLET_PWR_GPCS_ELPG_ZSORDER0          , 0xFFFFFFFF, 0x89ABCDEF, 0x89ABCDEF }, //reg_list[21]
//  { LW_PCHIPLET_PWR_GPCS_ELPG_ZSORDER1          , 0xFFFFFFFF, 0x89ABCDEF, 0x00000000 }, //reg_list[22]
//  { LW_PCHIPLET_PWR_GPCS_ELPG_ZDELAY0           , 0x7FFFFFFF, 0x75555555, 0x75555555 }, //reg_list[23]
//  { LW_PCHIPLET_PWR_GPCS_ELPG_ZDELAY1           , 0x7FFFFFFF, 0x75555555, 0x70000000 }, //reg_list[24]
//  { LW_PCHIPLET_PWR_GPCS_ELPG_DEBUG_KEEP_CLAMP  , 0x00000001, 0x00000000, 0x00000000 }, //reg_list[25]
//  { LW_PCHIPLET_PWR_GPC0_ELPG_INDEX             , 0x000003FF, 0x00000000, 0x00000000 }, //reg_list[26]
//  { LW_PCHIPLET_PWR_GPC0_PGRAM_PSORDER          , 0x000000BF, 0x00000080, 0x00000080 }, //reg_list[27]
//  { LW_PCHIPLET_PWR_GPC0_NPGRAM_PSORDER         , 0x000000BF, 0x00000080, 0x00000080 }, //reg_list[28]
//  { LW_PCHIPLET_PWR_GPC0_ELPG_PSORDER           , 0x000000BF, 0x00000080, 0x00000080 }  //reg_list[29]
};

char reg_list_name[][50] = {
    // General format : {Register name} //reg_list_name[array_index]
    // FBP RAM Registers
//    { "LW_PCHIPLET_PWR_FBPS_NPGRAM_REGSEL         " },
//    { "LW_PCHIPLET_PWR_FBPS_NPGRAM_PSORDER        " },
//    { "LW_PCHIPLET_PWR_FBPS_PGRAM_REGSEL          " },
//    { "LW_PCHIPLET_PWR_FBPS_PGRAM_PSORDER          " },
//
//    // GPC RAM Registers
//    { "LW_PCHIPLET_PWR_GPCS_NPGRAM_REGSEL          " },
//    { "LW_PCHIPLET_PWR_GPCS_PGRAM_REGSEL           " },
//
//  // SYS RAM Registers
//    { "LW_PPWR_PMU_SYS_PGRAM_PSORDER                " },
//    { "LW_PPWR_PMU_SYS_PGRAM_REGSEL                 " },
//    { "LW_PPWR_PMU_SYS_NPGRAM_PSORDER               " },
//    { "LW_PPWR_PMU_SYS_NPGRAM_REGSEL                " },
//    // SYS Registers
//  { "LW_PPWR_PMU_SYS_ELPG_PSORDER                 " }, //reg_list_name[0]
//  { "LW_PPWR_PMU_SYS_ELPG_REGSEL0                 " }, //reg_list_name[1]
//  { "LW_PPWR_PMU_SYS_ELPG_PDELAY                  " }, //reg_list_name[2]
//  { "LW_PPWR_PMU_SYS_ELPG_ZSORDER0                " }, //reg_list_name[3]
//  { "LW_PPWR_PMU_SYS_ELPG_ZSORDER1                " }, //reg_list_name[4]
//  { "LW_PPWR_PMU_SYS_ELPG_ZDELAY0                 " }, //reg_list_name[5]
//  { "LW_PPWR_PMU_SYS_ELPG_ZDELAY1                 " }, //reg_list_name[6]
//  { "LW_PPWR_PMU_SYS_ELPG_DEBUG_KEEP_CLAMP        " }, //reg_list_name[7]
    { "LW_PPWR_PMU_ELPG_SEQ_CG                      " }, //reg_list_name[8]
    { "LW_THERM_CTRL_1                              " }, //reg_list_name[9]
//  // FBP Registers
//  { "LW_PCHIPLET_PWR_FBPS_ELPG_PSORDER            " }, //reg_list_name[10]
//  { "LW_PCHIPLET_PWR_FBPS_ELPG_REGSEL0            " }, //reg_list_name[11]
//  { "LW_PCHIPLET_PWR_FBPS_ELPG_PDELAY             " }, //reg_list_name[12]
//  { "LW_PCHIPLET_PWR_FBPS_ELPG_ZSORDER0           " }, //reg_list_name[13]
//  { "LW_PCHIPLET_PWR_FBPS_ELPG_ZSORDER1           " }, //reg_list_name[14]
//  { "LW_PCHIPLET_PWR_FBPS_ELPG_ZDELAY0            " }, //reg_list_name[15]
//  { "LW_PCHIPLET_PWR_FBPS_ELPG_ZDELAY1            " }, //reg_list_name[16]
//  { "LW_PCHIPLET_PWR_FBPS_ELPG_DEBUG_KEEP_CLAMP   " }, //reg_list_name[17]
//  // GPC Registers
//  { "LW_PCHIPLET_PWR_GPCS_ELPG_REGSEL0            " }, //reg_list_name[18]
//  { "LW_PCHIPLET_PWR_GPCS_ELPG_REGSEL1            " }, //reg_list_name[19]
//  { "LW_PCHIPLET_PWR_GPCS_ELPG_PDELAY             " }, //reg_list_name[20]
//  { "LW_PCHIPLET_PWR_GPCS_ELPG_ZSORDER0           " }, //reg_list_name[21]
//  { "LW_PCHIPLET_PWR_GPCS_ELPG_ZSORDER1           " }, //reg_list_name[22]
//  { "LW_PCHIPLET_PWR_GPCS_ELPG_ZDELAY0            " }, //reg_list_name[23]
//  { "LW_PCHIPLET_PWR_GPCS_ELPG_ZDELAY1            " }, //reg_list_name[24]
//  { "LW_PCHIPLET_PWR_GPCS_ELPG_DEBUG_KEEP_CLAMP   " }, //reg_list_name[25]
//  { "LW_PCHIPLET_PWR_GPC0_ELPG_INDEX              " }, //reg_list_name[26]
//  { "LW_PCHIPLET_PWR_GPC0_PGRAM_PSORDER           " }, //reg_list_name[27]
//  { "LW_PCHIPLET_PWR_GPC0_NPGRAM_PSORDER          " }, //reg_list_name[28]
//  { "LW_PCHIPLET_PWR_GPC0_ELPG_PSORDER            " }  //reg_list_name[29]
};

/****************************************************************************/
/* Test the host sequencer changing mclk to intermediate clock frequency.   */
/****************************************************************************/

Elpg_Powergv_Kepler::Elpg_Powergv_Kepler(ArgReader *params) :
  LWGpuSingleChannelTest(params)
{
    fspg_option = 0;
    register_check = 0;
    centralized_elpg = 0;
    randomize_blg_regs = 0;
    seed_1 = 0x111;
    pgob = 0;
    gxpg = 0;
    vdpg = 0;
    mspg = 0;
    railgating=0;
    bbox_verify=0;

    fs_control = 0;
    fs_control_gpc = 0;
    fs_control_gpc_tpc = 0;
    fs_control_gpc_zlwll = 0;

    if (params->ParamPresent("-centralized_elpg")){
        centralized_elpg = 1;
        InfoPrintf("%s: %s: Centralized ELPG mode enabled.\n", DEBUG_PRINT , __FUNCTION__);
    }

    if (params->ParamPresent("-register_test")){
        register_check = 1;
        if (!centralized_elpg){
            gxpg = 1;
        }
        InfoPrintf("%s: %s: Running register test.\n", DEBUG_PRINT , __FUNCTION__);
    }

    if (params->ParamPresent("-randomize_regs")){
        randomize_blg_regs = 1;
        InfoPrintf("ELPG SPEEDY TEST: Randomizing reg values\n");
    }

    if (params->ParamPresent("-pgob")) {
        pgob = 1;
    }

    if (params->ParamPresent("-gxpg")) {
        gxpg = 1;
    }

    if (params->ParamPresent("-railgating")){
        railgating=1;
    }

    if (params->ParamPresent("-vdpg")) {
        vdpg = 1;
    }

    if (params->ParamPresent("-mspg")) {
        mspg = 1;
    }

    if (params->ParamPresent("-bbox_verify")) {
        bbox_verify = 1;
    }

    seed_1 = params->ParamUnsigned("-seed", 24);
    InfoPrintf("ELPG SPEEDY TEST: Seed value = %08x\n", seed_1);

    fspg_option = params->ParamUnsigned("-fspg_option", 0x0);
    InfoPrintf("ELPG SPEEDY TEST: FSPG Configuration Value = %08x\n", fspg_option);

    fs_control = params->ParamUnsigned("-fs_control", 0x0);
    InfoPrintf("ELPG SPEEDY TEST: LW_PTOP_FS_CONTROL Register Configuration Value = %08x\n", fs_control);

    fs_control_gpc = params->ParamUnsigned("-fs_control_gpc", 0x0);
    InfoPrintf("ELPG SPEEDY TEST: LW_PTOP_FS_CONTROL_GPC Register Configuration Value = %08x\n", fs_control_gpc);

    fs_control_gpc_tpc = params->ParamUnsigned("-fs_control_gpc_tpc", 0x0);
    InfoPrintf("ELPG SPEEDY TEST: LW_PTOP_FS_CONTROL_GPC_TPC Configuration Value = %08x\n", fs_control_gpc_tpc);

    fs_control_gpc_zlwll = params->ParamUnsigned("-fs_control_gpc_zlwll", 0x0);
    InfoPrintf("ELPG SPEEDY TEST: LW_PTOP_FS_CONTROL_GPC_ZLWLL Configuration Value = %08x\n", fs_control_gpc_zlwll);

    skip_MMUBind = 1;

}

Elpg_Powergv_Kepler::~Elpg_Powergv_Kepler(void)
{
}

STD_TEST_FACTORY(elpg_powergv_kepler, Elpg_Powergv_Kepler)

//*************************************************
// Set Up Everything Before the Test
//*************************************************

int
Elpg_Powergv_Kepler::Setup(void) {
  InfoPrintf("Entering the Elpg_Powergv_Kepler::Setup() Routine\n");
  lwgpu = LWGpuResource::FindFirstResource();

  m_regMap = lwgpu->GetRegisterMap();

  getStateReport()->init("elpg_powergv_kepler");
  getStateReport()->enable();

  InfoPrintf("Exiting the Elpg_Powergv_Kepler::Setup() Routine\n");
  return 1;
}

//*************************************************
// Clean Up after running the test
//*************************************************

void
Elpg_Powergv_Kepler::CleanUp(void) {
  if (lwgpu) {
    DebugPrintf("Elpg_Powergv_Kepler::CleanUp(): Releasing GPU.\n");
    lwgpu = 0;
  }
  LWGpuSingleChannelTest::CleanUp();
}

//*************************************************
// Running the test
//*************************************************

void
Elpg_Powergv_Kepler::Run(void) {
  InfoPrintf("Entering the Elpg_Powergv_Kepler::Run() Routine\n");
  SetStatus(TEST_INCOMPLETE);
  InfoPrintf("Starting Elpg_Powergv_Kepler test\n");

    if(pgob == 1) {
        if (elpg_pgob_Test()) {
            SetStatus(TEST_FAILED);
            getStateReport()->runFailed("Elpg_pgob test failed\n");
            return;
        }
        InfoPrintf("elpg_pgob_Test() Done !\n");
    }

    if((register_check == 1) && (centralized_elpg == 1)) {
        if(elpg_powergv_register_Test()) {
            InfoPrintf("%s: %s: Register test failed.\n", DEBUG_PRINT , __FUNCTION__);
            SetStatus(TEST_FAILED);
            getStateReport()->runFailed("Elpg_Powergv_Kepler test failed\n");
            return;
        }
        InfoPrintf("%s: %s: Register test done.\n", DEBUG_PRINT , __FUNCTION__);
    }

    if(gxpg == 1) {
        if(centralized_elpg == 1) {
            InfoPrintf("%s: %s: STARTING MAXWELL GXPG TEST... centralized_elpg=%d", DEBUG_PRINT , __FUNCTION__ , centralized_elpg);
            if(elpg_powergv_gx_maxwellTest()) {
                InfoPrintf("%s: %s: GXPG test failed.\n", DEBUG_PRINT , __FUNCTION__);
                SetStatus(TEST_FAILED);
                getStateReport()->runFailed("Elpg_Powergv_Kepler test failed\n");
                return;
            }
            InfoPrintf("%s: %s: GXPG test done.\n", DEBUG_PRINT , __FUNCTION__);
        }
        else {
            InfoPrintf("STARTING KEPLER GXPG TEST.. centralized_elpg=%d", DEBUG_PRINT , __FUNCTION__ , centralized_elpg);
            if(elpg_powergv_gx_keplerTest()) {
                SetStatus(TEST_FAILED);
                getStateReport()->runFailed("Elpg_Powergv_Kepler test failed\n");
                return;
            }
            InfoPrintf("elpg_powergv_gx_keplerTest() Done !\n");

        }
    }

    if(railgating == 1) {
        InfoPrintf("%s: %s: Starting Railgating Test", DEBUG_PRINT , __FUNCTION__);
        if(elpg_powergv_railgating_Test()){
            InfoPrintf("%s: %s: Railgating test failed.\n", DEBUG_PRINT , __FUNCTION__);
            SetStatus(TEST_FAILED);
            getStateReport()->runFailed("Railgating test failed\n");
            return;
        }
        InfoPrintf("%s: %s: Railgating test done.\n", DEBUG_PRINT , __FUNCTION__);
        }

   if(vdpg == 1) {
        if(elpg_powergv_vd_keplerTest()) {
            SetStatus(TEST_FAILED);
            getStateReport()->runFailed("Elpg_Powergv_Vd_Kepler test failed\n");
            return;
        }
        InfoPrintf("elpg_powergv_vd_keplerTest() Done !\n");
    }

    if(mspg == 1) {
        if(elpg_powergv_ms_keplerTest()) {
            SetStatus(TEST_FAILED);
            getStateReport()->runFailed("Elpg_Powergv_Ms_Kepler test failed\n");
            return;
        }
        InfoPrintf("elpg_powergv_ms_keplerTest() Done !\n");
    }

    if(fspg_option != 0) {
        if(elpg_powergv_fspg_keplerTest()) {
            SetStatus(TEST_FAILED);
            getStateReport()->runFailed("Elpg_Powergv_FSPG_Kepler test failed\n");
            return;
        }
        InfoPrintf("elpg_powergv_fspg_keplerTest() Done !\n");
    }

    if(bbox_verify == 1) {
        if(bbox_verify_Test()) {
            SetStatus(TEST_FAILED);
            getStateReport()->runFailed("Elpg_Powergv_Kepler: bbox_verify test failed\n");
            return;
        }
        InfoPrintf("Elpg_Powergv_Kepler: bbox_verify Done !\n");
    }

    InfoPrintf("Finishing Elpg_Powergv_Kepler test\n");
    SetStatus(TEST_SUCCEEDED);
    getStateReport()->crcCheckPassedGold();

    InfoPrintf("Exiting the Elpg_Powergv_Kepler::Run() Routine\n");
}

//***************************************************************
// Validating random values for BLPG registers (added by kgandhi)
//***************************************************************

UINT32
Elpg_Powergv_Kepler::Validate_PG_Reg(UINT32 data) {
  data = (data & 0x48F4FF00) | 0x40000;
  return data;
}

UINT32
Elpg_Powergv_Kepler::Validate_PG1_Reg(UINT32 data) {
  data = data & 0x00FFFFFF;
  return data;
}

UINT32
Elpg_Powergv_Kepler::Validate_slave_PG_Reg(UINT32 data) {
  data = (data & 0x48F40000) | 0x40000;
  return data;
}

const char*
Elpg_Powergv_Kepler::ELPG_enabled(UINT32 data) {
  data = data & 0x00040000;
  if(data > 0){
    return "ELPG enabled";
  } else {
    return "ELPG disabled";
  }
}

int
Elpg_Powergv_Kepler::WriteGateCtrl(int index, UINT32 newFieldValue)
{
    unique_ptr<IRegisterClass> pReg = lwgpu->GetRegister("LW_THERM_GATE_CTRL");
    if (!pReg)
    {
        ErrPrintf("Could not GetRegister(LW_THERM_GATE_CTRL)!\n");
        return (1);
    }

    unique_ptr<IRegisterField> pField;
    if (centralized_elpg)
    {
        pField = pReg->FindField("LW_THERM_GATE_CTRL_ENG_CLK");
        if (!pField)
        {
                ErrPrintf("Could not FindField(LW_THERM_GATE_CTRL_ENG_CLK)!\n");
                return (1);
        }
    }
    else
    {
        pField = pReg->FindField("LW_THERM_GATE_CTRL_BLK_CLK");
        if (!pField)
        {
                ErrPrintf("Could not FindField(LW_THERM_GATE_CTRL_BLK_CLK)!\n");
                return (1);
        }
    }

    UINT32 NewValue = newFieldValue << pField->GetStartBit();
    UINT32 Mask  = pField->GetWriteMask();

    UINT32 Value = lwgpu->RegRd32(pReg->GetAddress(index));

    Value = (Value & ~Mask) | NewValue;

    lwgpu->RegWr32(pReg->GetAddress(index), Value);

    Value = lwgpu->RegRd32(pReg->GetAddress(index));
    InfoPrintf("LW_THERM_GATE_CTRL_%d value is 0x%8x. \n", index, Value);

    return 0;
}

//*************************************************
// Power Gating On Boot Test (Including both GR and VD)
//*************************************************

UINT32
Elpg_Powergv_Kepler::elpg_pgob_Test() {

//     GpuSubdevice *pSubdev = lwgpu->GetGpuSubdevice();
//   
//     InfoPrintf("elpg pgob test starts... \n");
//     //Step 1: enable opt_pgob to power gate chip
//     // This step is done by loading fuse image automatically.
//     // fuse image: //hw/fermi1_gf11x/diag/tests/fuse_image_gf11x/<chip>/pgob.image
//     InfoPrintf("Step 1,program fuse opt_pgob by fuse image\n");
//     
//     //Step 2: write PMC_ENABLE register to reset everything
//     pSubdev->RegWr32( LW_PMC_ENABLE, 0x0);
//     int rdat = pSubdev->RegRd32( LW_PMC_ENABLE);
//     InfoPrintf("Step 2,LW_PMC_ENABLE value is 0x%8x. \n", rdat);
//   
//     //Step 3.1a Assert pg_reset for vd engines (new for kepler)
//     int pg_enable = pSubdev->RegRd32( LW_PMC_ELPG_ENABLE);
//     pg_enable |= DRF_DEF(_PMC, _ELPG_ENABLE, _MSPDEC, _DISABLED);
//     pg_enable |= DRF_DEF(_PMC, _ELPG_ENABLE, _MSPPP, _DISABLE); 
//     pg_enable |= DRF_DEF(_PMC, _ELPG_ENABLE, _MSVLD, _DISABLED);
//     pg_enable |= DRF_DEF(_PMC, _ELPG_ENABLE, _MSENC, _DISABLED);
//     pSubdev->RegWr32( LW_PMC_ELPG_ENABLE, pg_enable);
//     rdat = pSubdev->RegRd32( LW_PMC_ELPG_ENABLE);
//     InfoPrintf("Step 3.1a,LW_PMC_ELPG_ENABLE value is 0x%8x. \n", rdat);
// 
//     //Step 3.1b: deassert host2all_blg_reset for GR units and not_pg_reset for VD units (new in kepler).
//     int blg_enable = pSubdev->RegRd32( LW_PMC_ENABLE);
//     blg_enable |= DRF_DEF(_PMC, _ENABLE, _BLG, _ENABLED);
//     blg_enable |= DRF_DEF(_PMC, _ENABLE, _MSPDEC, _ENABLED);
//     blg_enable |= DRF_DEF(_PMC, _ENABLE, _MSPPP, _ENABLE); 
//     blg_enable |= DRF_DEF(_PMC, _ENABLE, _MSVLD, _ENABLED);
//     blg_enable |= DRF_DEF(_PMC, _ENABLE, _MSENC, _ENABLED);
//     pSubdev->RegWr32( LW_PMC_ENABLE, blg_enable);
//     rdat = pSubdev->RegRd32( LW_PMC_ENABLE);
//     InfoPrintf("Step 3.1b,LW_PMC_ENABLE value is 0x%8x. \n", rdat);
// 
//     //Step 3.2: deassert priv_ring reset.
//     int priv_ring_enable = pSubdev->RegRd32( LW_PMC_ENABLE);
//     priv_ring_enable |= DRF_DEF(_PMC, _ENABLE, _PRIV_RING, _ENABLED);
//     pSubdev->RegWr32( LW_PMC_ENABLE, priv_ring_enable);
//     rdat = pSubdev->RegRd32( LW_PMC_ENABLE);
//     InfoPrintf("Step 3.2,LW_PMC_ENABLE value is 0x%8x. \n", rdat);
// 
//     //Step 3.3: deassert pwr reset.
//     int pwr_enable = pSubdev->RegRd32( LW_PMC_ENABLE);
//     pwr_enable |= DRF_DEF(_PMC, _ENABLE, _PWR, _ENABLED);
//     pSubdev->RegWr32( LW_PMC_ENABLE, pwr_enable);
//     rdat = pSubdev->RegRd32( LW_PMC_ENABLE);
//     InfoPrintf("Step 3.3,LW_PMC_ENABLE value is 0x%8x. \n", rdat);
// 
//     //Step 4: assert global clamp_en
//     int  swctrl0 = 0x0;
//     InfoPrintf("Step 4, assert global clamp_en. \n");
//     swctrl0 |= DRF_DEF(_PPWR, _PMU_PG_PSW_MASK, _CLAMPMSK_0, _ENABLE);
//     swctrl0 |= DRF_DEF(_PPWR, _PMU_PG_PSW_MASK, _CLAMPVAL_0, _TRUE);
// //  swctrl0 |= DRF_DEF(_PPWR, _PMU_PG_PSW_MASK, _CLAMPMSK_1, _ENABLE);
// //  swctrl0 |= DRF_DEF(_PPWR, _PMU_PG_PSW_MASK, _CLAMPVAL_1, _TRUE);
//     pSubdev->RegWr32( LW_PPWR_PMU_PG_PSW_MASK, swctrl0);
//     rdat = pSubdev->RegRd32( LW_PPWR_PMU_PG_PSW_MASK);
//     InfoPrintf("Step 4, LW_PPWR_PMU_PG_PSW_MASK value is 0x%8x. \n", rdat);
// 
//     //step 5: check clamp_en and sleep signal to make sure they are forced by opt_pgob
//     bool fuse_blown = true;
//     if(check_clamp_sleep_gr(fuse_blown) == 1) return 1;
// //    if(check_clamp_sleep_vd(fuse_blown) == 1) return 1;
//                                                          
//     // Dummy delay to wait for power-down to complete.   
//     for (int i=0; i<300; i++) {                          
//        rdat = pSubdev->RegRd32( LW_THERM_CTRL_1);        
//     }                                                    
//                                                          
//     //Step 6: Override CTRL_1_PGOB_OVERRIDE to undo this power gating.
//     InfoPrintf("Step 6, Override CTRL_1_PGOB_OVERRIDE to undo this power gating.\n");
//     int ctrl_1 = pSubdev->RegRd32( LW_THERM_CTRL_1);     
//     ctrl_1 |= DRF_DEF(_THERM, _CTRL_1, _PGOB_OVERRIDE, _ENABLED);
//     ctrl_1 |= DRF_DEF(_THERM, _CTRL_1, _PGOB_OVERRIDE_VALUE, _OFF);
//     pSubdev->RegWr32( LW_THERM_CTRL_1, ctrl_1);
//     rdat = pSubdev->RegRd32( LW_THERM_CTRL_1);
//     InfoPrintf("Step 6,LW_THERM_CTRL_1 value is 0x%8x. \n", rdat);
// 
//     // Dummy delay to wait for power-up to complete.
//     for (int i=0; i<1500; i++) {
//       rdat = pSubdev->RegRd32( LW_THERM_CTRL_1);
//     }
// 
//     //Step 7: de-assert global clamp_en
//     swctrl0 = 0;
//     InfoPrintf("Step 7, de-assert global clamp_en. \n");
//     swctrl0 |= DRF_DEF(_PPWR, _PMU_PG_PSW_MASK, _CLAMPMSK_0, _ENABLE);
//     swctrl0 |= DRF_DEF(_PPWR, _PMU_PG_PSW_MASK, _CLAMPVAL_0, _FALSE);
// //  swctrl0 |= DRF_DEF(_PPWR, _PMU_PG_PSW_MASK, _CLAMPMSK_1, _ENABLE);
// //  swctrl0 |= DRF_DEF(_PPWR, _PMU_PG_PSW_MASK, _CLAMPVAL_1, _FALSE);
//     pSubdev->RegWr32( LW_PPWR_PMU_PG_PSW_MASK, swctrl0);
//     rdat = pSubdev->RegRd32( LW_PPWR_PMU_PG_PSW_MASK);
//     InfoPrintf("Step 7, LW_PPWR_PMU_PG_PSW_MASK value is 0x%8x. \n", rdat);
// 
//     //Step 8: assert host2all_blg_reset for GR units and not_pg_reset for VD units (new in kepler).
//     blg_enable = pSubdev->RegRd32( LW_PMC_ENABLE);
//     blg_enable &= DRF_DEF(_PMC, _ENABLE, _BLG, _DISABLED);
//     blg_enable |= DRF_DEF(_PMC, _ENABLE, _MSPDEC, _DISABLED);
//     blg_enable |= DRF_DEF(_PMC, _ENABLE, _MSPPP, _DISABLE); 
//     blg_enable |= DRF_DEF(_PMC, _ENABLE, _MSVLD, _DISABLED);
//     blg_enable |= DRF_DEF(_PMC, _ENABLE, _MSENC, _DISABLED);
//     pSubdev->RegWr32( LW_PMC_ENABLE, blg_enable);
//     rdat = pSubdev->RegRd32( LW_PMC_ENABLE);
//     InfoPrintf("Step 8a,LW_PMC_ENABLE value is 0x%8x. \n", rdat);
// 
//     //de-assert pg_reset for vd engines (new for kepler)
//     pg_enable = pSubdev->RegRd32( LW_PMC_ELPG_ENABLE);
//     pg_enable |= DRF_DEF(_PMC, _ELPG_ENABLE, _MSPDEC, _ENABLED);
//     pg_enable |= DRF_DEF(_PMC, _ELPG_ENABLE, _MSPPP, _ENABLE); 
//     pg_enable |= DRF_DEF(_PMC, _ELPG_ENABLE, _MSVLD, _ENABLED);
//     pg_enable |= DRF_DEF(_PMC, _ELPG_ENABLE, _MSENC, _ENABLED);
//     pSubdev->RegWr32( LW_PMC_ELPG_ENABLE, pg_enable);
//     rdat = pSubdev->RegRd32( LW_PMC_ELPG_ENABLE);
//     InfoPrintf("Step 8b,LW_PMC_ELPG_ENABLE value is 0x%8x. \n", rdat);
// 
//     //Step 9: de-assert all reset.
//     pSubdev->RegWr32( LW_PMC_ENABLE, 0xffffffff ); //Enable all engines 
//     rdat = pSubdev->RegRd32( LW_PMC_ENABLE);
//     InfoPrintf("Step 9,LW_PMC_ENABLE value is 0x%8x. \n", rdat);
// 
//     //Step 10: check clamp_en and sleep signal to make sure they are released when fuse are overrided by CTRL_1_PGOB_OVERRIDE 
//     fuse_blown = false;
//     if(check_clamp_sleep_gr(fuse_blown) == 1) return 1;
// //    if(check_clamp_sleep_vd(fuse_blown) == 1) return 1;
// 
//     //Initialize priv ring
//     pSubdev->RegWr32(LW_PPRIV_MASTER_RING_COMMAND_DATA , 0x503B5443);
//     PrivInit();
// 
//     //Wait some time before finish
//     for(int j=0; j < 100 ; j++)
//     {
//         RdRegArray(0,"LW_PPWR_PMU_PG_SWCTRL");
//         RdRegArray(1,"LW_PPWR_PMU_PG_SWCTRL");
//     }
// 
     return 0;
}

//****************************************************************************************
// Register read/write test
//****************************************************************************************
UINT32
Elpg_Powergv_Kepler::elpg_powergv_register_Test() {

    GpuSubdevice *pSubdev = lwgpu->GetGpuSubdevice();

    int reg_list_iter;
    int reset_err_count;
    int walk1_err_count;
    int walk0_err_count;

    reset_err_count = 0  ;
    walk1_err_count = 0  ;
    walk0_err_count = 0  ;

    InfoPrintf("%s: %s: Starting register test.\n", DEBUG_PRINT , __FUNCTION__);
    //-----------------------------------------------------------
    // Stage 1 : Initialization
    //-----------------------------------------------------------
    InfoPrintf("%s: %s: Stage 1: Initialization.\n", DEBUG_PRINT , __FUNCTION__);

    //Step -3: Clear PMU in Reset
    int temp1 = pSubdev->RegRd32( LW_PMC_ENABLE);
    pSubdev->RegWr32( LW_PMC_ENABLE, temp1 | 0x0000002d ); //Enable PRIV ring, XBAR
    pSubdev->RegRd32( LW_PMC_ENABLE);

    pSubdev->RegWr32(LW_PPRIV_MASTER_RING_COMMAND_DATA , 0x503B5443);

    PrivInit();

    temp1 = pSubdev->RegRd32( LW_PMC_ENABLE);
    pSubdev->RegWr32( LW_PMC_ENABLE, temp1 | 0xffffffff ); //Enable all engines after PRIV init is complete.
    pSubdev->RegRd32( LW_PMC_ENABLE);

    //-----------------------------------------------------------
    // Stage 2 : Reset Check
    //-----------------------------------------------------------

    InfoPrintf("%s: %s: Stage 2: Reset Check.\n", DEBUG_PRINT , __FUNCTION__);

    for (reg_list_iter=0; reg_list_iter<=REG_LIST_INDEX_MAX; reg_list_iter++)
    {
        reset_err_count += ReadCheckVal( reg_list[reg_list_iter][REG_DEFINE], reg_list[reg_list_iter][MASK_VAL], reg_list_name[reg_list_iter], reg_list[reg_list_iter][RESET_VAL] );
    }

    //Special loop for testing GPC indexed addresses
    for (reg_list_iter=0; reg_list_iter<=PER_GPC_ELPG_NUM_PARTITIONS_MAX; reg_list_iter++)
    {
        reset_err_count += WriteReadCheckVal( reg_list[REG_LIST_INDEX_MAX-3][REG_DEFINE], reg_list[REG_LIST_INDEX_MAX-3][MASK_VAL], reg_list_name[REG_LIST_INDEX_MAX-3], reg_list_iter                           );
        reset_err_count += ReadCheckVal(      reg_list[REG_LIST_INDEX_MAX][REG_DEFINE]  , reg_list[REG_LIST_INDEX_MAX][MASK_VAL]  , reg_list_name[REG_LIST_INDEX_MAX]  , reg_list[REG_LIST_INDEX_MAX][RESET_VAL] );
    }
    for (reg_list_iter=0; reg_list_iter<=PER_GPC_NPGRAM_NUM_PARTITIONS_MAX; reg_list_iter++)
    {
        reset_err_count += WriteReadCheckVal( reg_list[REG_LIST_INDEX_MAX-3][REG_DEFINE], reg_list[REG_LIST_INDEX_MAX-3][MASK_VAL], reg_list_name[REG_LIST_INDEX_MAX-3], reg_list_iter                           );
        reset_err_count += ReadCheckVal(      reg_list[REG_LIST_INDEX_MAX-1][REG_DEFINE]  , reg_list[REG_LIST_INDEX_MAX-1][MASK_VAL]  , reg_list_name[REG_LIST_INDEX_MAX-1]  , reg_list[REG_LIST_INDEX_MAX-1][RESET_VAL] );
    }
    for (reg_list_iter=0; reg_list_iter<=PER_GPC_PGRAM_NUM_PARTITIONS_MAX; reg_list_iter++)
    {
        reset_err_count += WriteReadCheckVal( reg_list[REG_LIST_INDEX_MAX-3][REG_DEFINE], reg_list[REG_LIST_INDEX_MAX-3][MASK_VAL], reg_list_name[REG_LIST_INDEX_MAX-3], reg_list_iter                           );
        reset_err_count += ReadCheckVal(      reg_list[REG_LIST_INDEX_MAX-2][REG_DEFINE]  , reg_list[REG_LIST_INDEX_MAX-2][MASK_VAL]  , reg_list_name[REG_LIST_INDEX_MAX-2]  , reg_list[REG_LIST_INDEX_MAX-2][RESET_VAL] + reg_list_iter);
    }

    //-----------------------------------------------------------
    // Stage 3 : Walking 1's : 10101010
    //-----------------------------------------------------------

    InfoPrintf("%s: %s: Stage 3: Walking 1's (10101010).\n", DEBUG_PRINT , __FUNCTION__);

    for (reg_list_iter=0; reg_list_iter<=REG_LIST_INDEX_MAX-4; reg_list_iter++)
    {
        walk1_err_count += WriteReadCheckVal( reg_list[reg_list_iter][REG_DEFINE], reg_list[reg_list_iter][MASK_VAL], reg_list_name[reg_list_iter], 0x10101010 );
    }

    //Special loop for testing GPC indexed addresses
    for (reg_list_iter=0; reg_list_iter<=PER_GPC_ELPG_NUM_PARTITIONS_MAX; reg_list_iter++)
    {
        walk1_err_count += WriteReadCheckVal( reg_list[REG_LIST_INDEX_MAX-3][REG_DEFINE], reg_list[REG_LIST_INDEX_MAX-3][MASK_VAL], reg_list_name[REG_LIST_INDEX_MAX-3], reg_list_iter );
         lwgpu->GetGpuSubdevice()->RegWr32(reg_list[REG_LIST_INDEX_MAX][REG_DEFINE],0x10101010);
     }

    for (reg_list_iter=0; reg_list_iter<=PER_GPC_ELPG_NUM_PARTITIONS_MAX; reg_list_iter++)
    {
        walk1_err_count += WriteReadCheckVal( reg_list[REG_LIST_INDEX_MAX-3][REG_DEFINE], reg_list[REG_LIST_INDEX_MAX-3][MASK_VAL], reg_list_name[REG_LIST_INDEX_MAX-3], reg_list_iter );
        walk1_err_count += ReadCheckVal( reg_list[REG_LIST_INDEX_MAX][REG_DEFINE]  , reg_list[REG_LIST_INDEX_MAX][MASK_VAL]  , reg_list_name[REG_LIST_INDEX_MAX]  , 0x10101010 );
    }
    for (reg_list_iter=0; reg_list_iter<=PER_GPC_PGRAM_NUM_PARTITIONS_MAX; reg_list_iter++)
    {
        walk1_err_count += WriteReadCheckVal( reg_list[REG_LIST_INDEX_MAX-3][REG_DEFINE], reg_list[REG_LIST_INDEX_MAX-3][MASK_VAL], reg_list_name[REG_LIST_INDEX_MAX-3], reg_list_iter );
         lwgpu->GetGpuSubdevice()->RegWr32(reg_list[REG_LIST_INDEX_MAX-2][REG_DEFINE],0x10101010);
     }

    for (reg_list_iter=0; reg_list_iter<=PER_GPC_PGRAM_NUM_PARTITIONS_MAX; reg_list_iter++)
    {
        walk1_err_count += WriteReadCheckVal( reg_list[REG_LIST_INDEX_MAX-3][REG_DEFINE], reg_list[REG_LIST_INDEX_MAX-3][MASK_VAL], reg_list_name[REG_LIST_INDEX_MAX-3], reg_list_iter );
        walk1_err_count += ReadCheckVal( reg_list[REG_LIST_INDEX_MAX-2][REG_DEFINE]  , reg_list[REG_LIST_INDEX_MAX-2][MASK_VAL]  , reg_list_name[REG_LIST_INDEX_MAX-2]  , 0x10101010 );
    }
    for (reg_list_iter=0; reg_list_iter<=PER_GPC_NPGRAM_NUM_PARTITIONS_MAX; reg_list_iter++)
    {
        walk1_err_count += WriteReadCheckVal( reg_list[REG_LIST_INDEX_MAX-3][REG_DEFINE], reg_list[REG_LIST_INDEX_MAX-3][MASK_VAL], reg_list_name[REG_LIST_INDEX_MAX-3], reg_list_iter );
         lwgpu->GetGpuSubdevice()->RegWr32(reg_list[REG_LIST_INDEX_MAX-1][REG_DEFINE],0x10101010);
     }

    for (reg_list_iter=0; reg_list_iter<=PER_GPC_NPGRAM_NUM_PARTITIONS_MAX; reg_list_iter++)
    {
        walk1_err_count += WriteReadCheckVal( reg_list[REG_LIST_INDEX_MAX-3][REG_DEFINE], reg_list[REG_LIST_INDEX_MAX-3][MASK_VAL], reg_list_name[REG_LIST_INDEX_MAX-3], reg_list_iter );
        walk1_err_count += ReadCheckVal( reg_list[REG_LIST_INDEX_MAX-1][REG_DEFINE]  , reg_list[REG_LIST_INDEX_MAX-1][MASK_VAL]  , reg_list_name[REG_LIST_INDEX_MAX-1]  , 0x10101010 );
    }

    //-----------------------------------------------------------
    // Stage 4 : Walking 0's : 01010101
    //-----------------------------------------------------------

    InfoPrintf("%s: %s: Stage 4: Walking 0's (01010101).\n", DEBUG_PRINT , __FUNCTION__);

    for (reg_list_iter=0; reg_list_iter<=REG_LIST_INDEX_MAX-4; reg_list_iter++)
    {
        walk0_err_count += WriteReadCheckVal( reg_list[reg_list_iter][REG_DEFINE], reg_list[reg_list_iter][MASK_VAL], reg_list_name[reg_list_iter], 0x01010101 );
    }

    //Special loop for testing GPC indexed addresses
    for (reg_list_iter=0; reg_list_iter<=PER_GPC_ELPG_NUM_PARTITIONS_MAX; reg_list_iter++)
    {
        walk0_err_count += WriteReadCheckVal( reg_list[REG_LIST_INDEX_MAX-3][REG_DEFINE], reg_list[REG_LIST_INDEX_MAX-3][MASK_VAL], reg_list_name[REG_LIST_INDEX_MAX-3], reg_list_iter );
         lwgpu->GetGpuSubdevice()->RegWr32(reg_list[REG_LIST_INDEX_MAX][REG_DEFINE],0x01010101);
    }

    for (reg_list_iter=0; reg_list_iter<=PER_GPC_ELPG_NUM_PARTITIONS_MAX; reg_list_iter++)
    {
        walk0_err_count += WriteReadCheckVal( reg_list[REG_LIST_INDEX_MAX-3][REG_DEFINE], reg_list[REG_LIST_INDEX_MAX-3][MASK_VAL], reg_list_name[REG_LIST_INDEX_MAX-3], reg_list_iter );
        walk0_err_count += ReadCheckVal( reg_list[REG_LIST_INDEX_MAX][REG_DEFINE]  , reg_list[REG_LIST_INDEX_MAX][MASK_VAL]  , reg_list_name[REG_LIST_INDEX_MAX]  , 0x01010101);
    }
    for (reg_list_iter=0; reg_list_iter<=PER_GPC_PGRAM_NUM_PARTITIONS_MAX; reg_list_iter++)
    {
        walk0_err_count += WriteReadCheckVal( reg_list[REG_LIST_INDEX_MAX-3][REG_DEFINE], reg_list[REG_LIST_INDEX_MAX-3][MASK_VAL], reg_list_name[REG_LIST_INDEX_MAX-3], reg_list_iter );
         lwgpu->GetGpuSubdevice()->RegWr32(reg_list[REG_LIST_INDEX_MAX-2][REG_DEFINE],0x01010101);
    }

    for (reg_list_iter=0; reg_list_iter<=PER_GPC_PGRAM_NUM_PARTITIONS_MAX; reg_list_iter++)
    {
        walk0_err_count += WriteReadCheckVal( reg_list[REG_LIST_INDEX_MAX-3][REG_DEFINE], reg_list[REG_LIST_INDEX_MAX-3][MASK_VAL], reg_list_name[REG_LIST_INDEX_MAX-3], reg_list_iter );
        walk0_err_count += ReadCheckVal( reg_list[REG_LIST_INDEX_MAX-2][REG_DEFINE]  , reg_list[REG_LIST_INDEX_MAX-2][MASK_VAL]  , reg_list_name[REG_LIST_INDEX_MAX-2]  , 0x01010101);
    }
    for (reg_list_iter=0; reg_list_iter<=PER_GPC_NPGRAM_NUM_PARTITIONS_MAX; reg_list_iter++)
    {
        walk0_err_count += WriteReadCheckVal( reg_list[REG_LIST_INDEX_MAX-3][REG_DEFINE], reg_list[REG_LIST_INDEX_MAX-3][MASK_VAL], reg_list_name[REG_LIST_INDEX_MAX-3], reg_list_iter );
         lwgpu->GetGpuSubdevice()->RegWr32(reg_list[REG_LIST_INDEX_MAX-1][REG_DEFINE],0x01010101);
    }

    for (reg_list_iter=0; reg_list_iter<=PER_GPC_NPGRAM_NUM_PARTITIONS_MAX; reg_list_iter++)
    {
        walk0_err_count += WriteReadCheckVal( reg_list[REG_LIST_INDEX_MAX-3][REG_DEFINE], reg_list[REG_LIST_INDEX_MAX-3][MASK_VAL], reg_list_name[REG_LIST_INDEX_MAX-3], reg_list_iter );
        walk0_err_count += ReadCheckVal( reg_list[REG_LIST_INDEX_MAX-1][REG_DEFINE]  , reg_list[REG_LIST_INDEX_MAX-1][MASK_VAL]  , reg_list_name[REG_LIST_INDEX_MAX-1]  , 0x01010101);
    }

    //-----------------------------------------------------------
    // Stage 5 : Final checks
    //-----------------------------------------------------------

    if((reset_err_count!=0) || (walk1_err_count!=0) || (walk0_err_count!=0))
    {
        InfoPrintf("%s: %s: Test complete. Reset errors = %d, Walking 1 errors = %d, Walking 0 errors = %d, Total errors = %d\n", DEBUG_PRINT , __FUNCTION__, reset_err_count, walk1_err_count, walk0_err_count, (reset_err_count+walk1_err_count+walk0_err_count));
        ErrPrintf("%s: %s: Register test failed. \n", DEBUG_PRINT , __FUNCTION__ );
        return 1;
    }
    else
    {
        return 0;
    }
}

//****************************************************************************************
// Test Graphic Engine Power Gating - Maxwell
//****************************************************************************************
UINT32
Elpg_Powergv_Kepler::elpg_powergv_gx_maxwellTest() {

    GpuSubdevice *pSubdev = lwgpu->GetGpuSubdevice();

    //-----------------------------------------------------------
    // Stage 1 : General Initialization
    //-----------------------------------------------------------
    InfoPrintf("%s: %s: Stage 1: General Initialization.\n", DEBUG_PRINT , __FUNCTION__);

    // check clamp_en and sleep signal to make sure that when the fuse is not blown
    // bool fuse_blown = false;
    // Commenting the below check for gv11b
    // if(check_clamp_sleep_gr(fuse_blown) == 1) return 1;
    // if(check_clamp_sleep_vd(fuse_blown) == 1) return 1;

    //Step -3: Clear PMU in Reset
    int temp1 = pSubdev->RegRd32( LW_PMC_ENABLE);
    pSubdev->RegWr32( LW_PMC_ENABLE, temp1 | 0x0000002d ); //Enable PRIV ring, XBAR
    pSubdev->RegRd32( LW_PMC_ENABLE);
    InfoPrintf("%s: %s: ENABLED PMC\n", DEBUG_PRINT , __FUNCTION__);
    //int temp = pSubdev->RegRd32(LW_PCHIPLET_PWR_GPCS_POWER_CTRL_GR_PARTITION);
    //pSubdev->RegWr32(LW_PCHIPLET_PWR_GPCS_POWER_CTRL_GR_PARTITION , temp | 0x00000100 ); //Enable Descending order
    //if (pSubdev->RegRd32(LW_PCHIPLET_PWR_GPCS_POWER_CTRL_GR_PARTITION);

    pSubdev->RegWr32(LW_PPRIV_MASTER_RING_COMMAND_DATA , 0x503B5443);

    InfoPrintf("%s: %s: Priv init started\n", DEBUG_PRINT , __FUNCTION__);
    PrivInit();
    InfoPrintf("%s: %s: Priv init done\n", DEBUG_PRINT , __FUNCTION__);

    temp1 = pSubdev->RegRd32( LW_PMC_ENABLE);
    pSubdev->RegWr32( LW_PMC_ENABLE, temp1 | 0xffffffff ); //Enable all engines after PRIV init is complete.
    pSubdev->RegRd32( LW_PMC_ENABLE);
    InfoPrintf("%s: %s: All engines enabled\n", DEBUG_PRINT , __FUNCTION__);

    pSubdev->RegWr32( LW_PMC_ELPG_ENABLE,0x20000000 ); //Enable Hub
    pSubdev->RegRd32( LW_PMC_ELPG_ENABLE);
    InfoPrintf("%s: %s: Hub enabled\n", DEBUG_PRINT , __FUNCTION__);

    // Disable power gating for NPG partitions 
    //pSubdev->RegWr32(LW_PCHIPLET_PWR_GPCS_POWER_CTRL_NGR_PARTITION_INDEX, 0x00000000); 
    //pSubdev->RegWr32(LW_PCHIPLET_PWR_GPCS_POWER_CTRL_NGR_PARTITION, 0x0); 
    //pSubdev->RegWr32(LW_PCHIPLET_PWR_FBPS_POWER_CTRL_NGR_PARTITION_INDEX, 0x00000000); 
    //pSubdev->RegWr32(LW_PCHIPLET_PWR_FBPS_POWER_CTRL_NGR_PARTITION, 0x0);
    //pSubdev->RegWr32(LW_PCHIPLET_PWR_FBPS_POWER_CTRL_NGR_PARTITION_INDEX, 0x00000001);
    //pSubdev->RegWr32(LW_PCHIPLET_PWR_FBPS_POWER_CTRL_NGR_PARTITION, 0x0); 
    //pSubdev->RegWr32(LW_PCHIPLET_PWR_FBPS_POWER_CTRL_NGR_PARTITION_INDEX, 0x00000002); 
    //pSubdev->RegWr32(LW_PCHIPLET_PWR_FBPS_POWER_CTRL_NGR_PARTITION, 0x0); 
    //pSubdev->RegWr32(LW_PCHIPLET_PWR_FBPS_POWER_CTRL_NGR_PARTITION_INDEX, 0x00000003);
    //pSubdev->RegWr32(LW_PCHIPLET_PWR_FBPS_POWER_CTRL_NGR_PARTITION, 0x0); 
    //pSubdev->RegWr32(LW_PPWR_PMU_POWER_CTRL_NGR_PARTITION_INDEX, 0x00000000); 
    //pSubdev->RegWr32(LW_PPWR_PMU_POWER_CTRL_NGR_PARTITION, 0x0);
    //pSubdev->RegWr32(LW_PPWR_PMU_POWER_CTRL_NGR_PARTITION_INDEX, 0x00000001); 
    //pSubdev->RegWr32(LW_PPWR_PMU_POWER_CTRL_NGR_PARTITION, 0x0);
    //pSubdev->RegWr32(LW_PPWR_PMU_POWER_CTRL_NGR_PARTITION_INDEX, 0x00000002); 
    //pSubdev->RegWr32(LW_PPWR_PMU_POWER_CTRL_NGR_PARTITION, 0x0); 
    //InfoPrintf("%s: %s: Disable PG for NPG partitions\n", DEBUG_PRINT , __FUNCTION__);

    // Keep count of number of errors
    int gxpg_reg_err_count = 0;

    //----------------------------------------------------------------------
    // Stage 2 : Enter into TPC-PG for TPC3
    //  TPC-PG entry programming for TPC3
    //    1) Write LW_PCHIPLET_PWR_GPCS_RAM_CTRL_TPC3 bits to _ENTER_SD
    //    2) Poll for LW_PCHIPLET_PWR_GPCS_RAM_STATUS_TPC3 bits to _SD
    //    3) Write LW_PCHIPLET_PWR_GPCS_LOGIC_CTRL_TPC3 bits to _ENTER_PG
    //    4) Poll for LW_PCHIPLET_PWR_GPCS_LOGIC_STATUS_TPC3 bits to _PG
    //----------------------------------------------------------------------
//    InfoPrintf("%s: %s: Stage 2: Enter into TPC-PG for TPC3.\n", DEBUG_PRINT , __FUNCTION__);

//    InfoPrintf("%s: %s: Step a.1: Trigger HW GR ram sequencer to do TPC power gating.\n", DEBUG_PRINT , __FUNCTION__);
//    temp1 = pSubdev->RegRd32(LW_PCHIPLET_PWR_GPCS_RAM_CTRL);
//    pSubdev->RegWr32(LW_PCHIPLET_PWR_GPCS_RAM_CTRL , ((0x00002000 & 0x00003000) | (temp1 & ~(0x00003000)))); 
//    gxpg_reg_err_count += ReadCheckVal(LW_PCHIPLET_PWR_GPCS_RAM_CTRL ,  0x00003000 , "LW_PCHIPLET_PWR_GPCS_RAM_CTRL_TPC3" , 0x00002000);  
//
//    UINT32 pwr_gpcs_reg_rd_value;
//    InfoPrintf("%s: %s: Step a.2: Poll HW GR ram sequencer status to ensure ram power gating done  .\n", DEBUG_PRINT , __FUNCTION__);
//    do {
//    pwr_gpcs_reg_rd_value = lwgpu->RegRd32(LW_PCHIPLET_PWR_GPCS_RAM_STATUS);
//    InfoPrintf("PrivReInit::PrivReInitTest: read LW_PCHIPLET_PWR_GPCS_RAM_STATUS,reg_rd_val=0x%x\n", pwr_gpcs_reg_rd_value);
//     } while ((pwr_gpcs_reg_rd_value & 0x00007000) != 0x00005000);
//
//    InfoPrintf("%s: %s: Step a.3: Trigger HW logic sequencer to do TPC power gating.\n", DEBUG_PRINT , __FUNCTION__);
//    temp1 = pSubdev->RegRd32(LW_PCHIPLET_PWR_GPCS_LOGIC_CTRL);
//    pSubdev->RegWr32(LW_PCHIPLET_PWR_GPCS_LOGIC_CTRL , ((0x00001000 & 0x00003000) | (temp1 & ~(0x00003000)))); 
//    gxpg_reg_err_count += ReadCheckVal(LW_PCHIPLET_PWR_GPCS_LOGIC_CTRL ,  0x00003000 , "LW_PCHIPLET_PWR_GPCS_LOGIC_CTRL_TPC3" , 0x00001000);   
//
//    InfoPrintf("%s: %s: Step a.4: Poll HW logic sequencer status to make sure logic power gating done.\n", DEBUG_PRINT , __FUNCTION__);
//    do {
//    pwr_gpcs_reg_rd_value = lwgpu->RegRd32(LW_PCHIPLET_PWR_GPCS_LOGIC_STATUS);
//    InfoPrintf("PrivReInit::PrivReInitTest: read LW_PCHIPLET_PWR_GPCS_LOGIC_STATUS,reg_rd_val=0x%x\n", pwr_gpcs_reg_rd_value);
//     } while ((pwr_gpcs_reg_rd_value & 0x00007000) != 0x00002000);

    //-----------------------------------------------------------
    // Stage 3 : ELPG sequence
    //-----------------------------------------------------------
    InfoPrintf("%s: %s: Stage 3: ELPG sequence.\n", DEBUG_PRINT , __FUNCTION__);

    //-----------------------------------------------------------
    // Step 1 :
    // - Initialize all Centralized ELPG registers to PROD values
    // - This will be done by RM in production
    //-----------------------------------------------------------
    InfoPrintf("%s: %s: Step 1: Initialzing ELPG registers with PROD values.\n", DEBUG_PRINT , __FUNCTION__);

//  for (reg_list_iter=0; reg_list_iter<=REG_LIST_INDEX_MAX-4; reg_list_iter++)
//  {
//      gxpg_reg_err_count += WriteReadCheckVal( reg_list[reg_list_iter][REG_DEFINE], reg_list[reg_list_iter][MASK_VAL], reg_list_name[reg_list_iter], reg_list[reg_list_iter][PROD_VAL] );
//  }
//
//  //Special loop for testing GPC indexed addresses
//  //Normal GPC Partitions
//  for (reg_list_iter=0; reg_list_iter<=PER_GPC_ELPG_NUM_PARTITIONS_MAX; reg_list_iter++)
//  {
//      gxpg_reg_err_count += WriteReadCheckVal( reg_list[REG_LIST_INDEX_MAX-3][REG_DEFINE], reg_list[REG_LIST_INDEX_MAX-3][MASK_VAL], reg_list_name[REG_LIST_INDEX_MAX-3], reg_list_iter);
//      lwgpu->GetGpuSubdevice()->RegWr32(reg_list[REG_LIST_INDEX_MAX][REG_DEFINE],reg_list[REG_LIST_INDEX_MAX][PROD_VAL]);
//  }
//  for (reg_list_iter=0; reg_list_iter<=PER_GPC_ELPG_NUM_PARTITIONS_MAX; reg_list_iter++)
//  {
//      gxpg_reg_err_count += WriteReadCheckVal( reg_list[REG_LIST_INDEX_MAX-3][REG_DEFINE], reg_list[REG_LIST_INDEX_MAX-3][MASK_VAL], reg_list_name[REG_LIST_INDEX_MAX-3], reg_list_iter);
//      gxpg_reg_err_count += ReadCheckVal( reg_list[REG_LIST_INDEX_MAX][REG_DEFINE]  , reg_list[REG_LIST_INDEX_MAX][MASK_VAL]  , reg_list_name[REG_LIST_INDEX_MAX]  , reg_list[REG_LIST_INDEX_MAX][PROD_VAL]  );
//   }
//
//  //GPC PGRAM partitions
//   for (reg_list_iter=0; reg_list_iter<=PER_GPC_PGRAM_NUM_PARTITIONS_MAX; reg_list_iter++)
//  {
//      gxpg_reg_err_count += WriteReadCheckVal( reg_list[REG_LIST_INDEX_MAX-3][REG_DEFINE], reg_list[REG_LIST_INDEX_MAX-3][MASK_VAL], reg_list_name[REG_LIST_INDEX_MAX-3], reg_list_iter);
//      lwgpu->GetGpuSubdevice()->RegWr32(reg_list[REG_LIST_INDEX_MAX-2][REG_DEFINE],reg_list[REG_LIST_INDEX_MAX-2][PROD_VAL]);
//  }
//  for (reg_list_iter=0; reg_list_iter<=PER_GPC_PGRAM_NUM_PARTITIONS_MAX; reg_list_iter++)
//  {
//      gxpg_reg_err_count += WriteReadCheckVal( reg_list[REG_LIST_INDEX_MAX-3][REG_DEFINE], reg_list[REG_LIST_INDEX_MAX-3][MASK_VAL], reg_list_name[REG_LIST_INDEX_MAX-3], reg_list_iter);
//      gxpg_reg_err_count += ReadCheckVal( reg_list[REG_LIST_INDEX_MAX-2][REG_DEFINE]  , reg_list[REG_LIST_INDEX_MAX-2][MASK_VAL]  , reg_list_name[REG_LIST_INDEX_MAX-2]  , reg_list[REG_LIST_INDEX_MAX-2][PROD_VAL]  );
//   }
//
//  //GPC NPGRAM Partitions
//   for (reg_list_iter=0; reg_list_iter<=PER_GPC_NPGRAM_NUM_PARTITIONS_MAX; reg_list_iter++)
//  {
//      gxpg_reg_err_count += WriteReadCheckVal( reg_list[REG_LIST_INDEX_MAX-3][REG_DEFINE], reg_list[REG_LIST_INDEX_MAX-3][MASK_VAL], reg_list_name[REG_LIST_INDEX_MAX-3], reg_list_iter);
//      lwgpu->GetGpuSubdevice()->RegWr32(reg_list[REG_LIST_INDEX_MAX-1][REG_DEFINE],reg_list[REG_LIST_INDEX_MAX-1][PROD_VAL]);
//  }
//  for (reg_list_iter=0; reg_list_iter<=PER_GPC_NPGRAM_NUM_PARTITIONS_MAX; reg_list_iter++)
//  {
//      gxpg_reg_err_count += WriteReadCheckVal( reg_list[REG_LIST_INDEX_MAX-3][REG_DEFINE], reg_list[REG_LIST_INDEX_MAX-3][MASK_VAL], reg_list_name[REG_LIST_INDEX_MAX-3], reg_list_iter);
//      gxpg_reg_err_count += ReadCheckVal( reg_list[REG_LIST_INDEX_MAX-1][REG_DEFINE]  , reg_list[REG_LIST_INDEX_MAX-1][MASK_VAL]  , reg_list_name[REG_LIST_INDEX_MAX-1]  , reg_list[REG_LIST_INDEX_MAX-1][PROD_VAL]  );
//   }

    //Idle threshold programming
    gxpg_reg_err_count += WriteReadCheckVal( LW_PPWR_PMU_PG_IDLEFILTH(0)      ,  0x3F      , "LW_PPWR_PMU_PG_IDLEFILTH(0)"    , 0x30      );
    gxpg_reg_err_count += WriteReadCheckVal( LW_PPWR_PMU_PG_PPUIDLEFILTH(0)   ,  0x3F      , "LW_PPWR_PMU_PG_PPUIDLEFILTH(0)" , 0x34      );

    //MMU BIND
    if(!skip_MMUBind) {
        MMUBindGR(0,pSubdev); // BIND
    }

    //-----------------------------------------------------------
    // Step 2 :
    // - Initialize PWR registers to enable ELPG
    // - This will be done by RM in production
    //-----------------------------------------------------------
    InfoPrintf("%s: %s: Step 2: Initialzing PWR registers to enable ELPG.\n", DEBUG_PRINT , __FUNCTION__);

    gxpg_reg_err_count += WriteReadCheckVal( LW_PPWR_PMU_IDLE_MASK(0)         ,  0x20000F  , "LW_PPWR_PMU_IDLE_MASK(0)"      , 0x20000F   );
    gxpg_reg_err_count += WriteReadCheckVal( LW_PPWR_PMU_IDLE_MASK_1(0)       ,  0x1020    , "LW_PPWR_PMU_IDLE_MASK_1(0)"    , 0x1020     );
    gxpg_reg_err_count += WriteReadCheckVal( LW_PPWR_PMU_PG_CTRL(0)           ,  0x1       , "LW_PPWR_PMU_PG_CTRL(0)"        , 0x3        );
    gxpg_reg_err_count += WriteReadCheckVal( LW_PPWR_PMU_IDLE_MASK_SUPP(0)    ,  0x20000F  , "LW_PPWR_PMU_IDLE_MASK(0)"      , 0x20000F   );
    gxpg_reg_err_count += WriteReadCheckVal( LW_PPWR_PMU_IDLE_MASK_1_SUPP(0)  ,  0x1020    , "LW_PPWR_PMU_IDLE_MASK_1(0)"    , 0x1020     );
    gxpg_reg_err_count += WriteReadCheckVal( LW_PPWR_PMU_IDLE_CTRL_SUPP(0)    ,  0x11      , "LW_PPWR_PMU_IDLE_MASK_1(0)"    , 0x01       );
    gxpg_reg_err_count += WriteReadCheckVal( LW_PPWR_PMU_PG_INTREN(0)         ,  0x1       , "LW_PPWR_PMU_PG_INTREN(0)"      , 0x11111    );
//    gxpg_reg_err_count += WriteReadCheckVal( LW_PPWR_PMU_PG_CFG_RDY      ,  0x1       , "LW_PPWR_PMU_PG_CFG_RDY"   , 0x1        );

    //-----------------------------------------------------------
    // Power Down sequence
    //-----------------------------------------------------------
    InfoPrintf("%s: %s: Starting ELPG Power Down sequence.\n", DEBUG_PRINT , __FUNCTION__);

    //Enabling ELCG in graphics engine by setting LW_THERM_GATE_CTRL_ENG_CLK to AUTO mode for graphics engine
    if (WriteGateCtrl(0, 1)) {
    ErrPrintf("Error : Could not enable ELCG for graphics engine\n" );
    }

    //Enabling ELCG in copy engine by setting LW_THERM_GATE_CTRL_ENG_CLK to AUTO mode for copy engine
    if (WriteGateCtrl(1, 1)) {
    ErrPrintf("Error : Could not enable ELCG for copy engine\n" );
    }

    //-----------------------------------------------------------
    // Step 3 :
    // - Wait for PG_ON to be asserted by ELPG controller
    //-----------------------------------------------------------
    InfoPrintf("%s: %s: Step 3: Waiting for PG_ON interrupt to be raised.\n", DEBUG_PRINT , __FUNCTION__);
//    do {
//        Wait(300);
//    } while ( ReadCheckVal( LW_PPWR_PMU_PG_INTRSTAT(0) , 0x10 , "LW_PPWR_PMU_PG_INTRSTAT(0)" , 0x10 ) != 0 );

    //-----------------------------------------------------------
    // Step 4 :
    // - Disable engine scheduling & execute preempt sequence
    // - This will be done by PMU uCode in production
    //-----------------------------------------------------------
    InfoPrintf("%s: %s: Step 4: Disabling engine scheduling & execute preempt sequence.\n", DEBUG_PRINT , __FUNCTION__);
    /* UPDATE: Since Ampere-10 has updated LW_PFIFO_* with LW_RUNLIST_*, updating the sequence. */
  
    UINT32 num_device = pSubdev->RegRd32(LW_PTOP_DEVICE_INFO_CFG) & 0xfff00000;
    UINT32 device_base = 0x0;

    // Search base for GR_ENG.
    for (UINT32 i = 0; i < num_device; i++ ) {
        // Check if 1st row is of type GR
	if ( ReadCheckVal ( LW_PTOP_DEVICE_INFO2(i), 0xff000000, "LW_PTOP_DEVICE_INFO2_ENUM", 0x80000000 ) ) {
	    // If not, Proceed to next rows until Last row of the group hit.
	    do { i++; } while ( (pSubdev->RegRd32(LW_PTOP_DEVICE_INFO2(i)) & 0x80000000) != 0x0);
	}
	else {
	    device_base = pSubdev->RegRd32(LW_PTOP_DEVICE_INFO2(i+2)) & 0x03fffc00;
	    break;
	}
    }
    InfoPrintf("%s: %s: LW_RUNLIST_* base address: %x.\n", DEBUG_PRINT, __FUNCTION__, device_base);

    //Disable runlist scheduling.
    gxpg_reg_err_count += WriteReadCheckVal( device_base + LW_RUNLIST_SCHED_DISABLE   ,  0x1        , "LW_RUNLIST_SCHED_DISABLE"     , 0x1        );

    // Write to Preempt register.
    pSubdev->RegWr32(device_base + LW_RUNLIST_PREEMPT, 0x0);
	
    // Wait for preempt to clear.
    do { Wait(300); } while ( (pSubdev->RegRd32(device_base + LW_RUNLIST_PREEMPT) & 0x00200000) != 0x0);

    //Ensure there are no pending PBDMA interrupt
    gxpg_reg_err_count += WriteReadCheckVal( device_base + LW_RUNLIST_INTR_0     ,  0x00f00000 , "LW_RUNLIST_INTR_0"            , 0x0        );
    
    //Commenting INTR_STALL since not supported in Ampere.
    //Ensure there are no stalling interrupts
    //gxpg_reg_err_count += WriteReadCheckVal( LW_PFIFO_INTR_STALL           ,  0xF9810101 , "LW_PFIFO_INTR_STALL"        , 0x00000000 );
	

    //-----------------------------------------------------------
    // Step 5 :
    // - Enable holdoff
    // - This will be done by PMU uCode in production
    //-----------------------------------------------------------
    InfoPrintf("%s: %s: Step 5: Enabling holdoff.\n", DEBUG_PRINT , __FUNCTION__);
    gxpg_reg_err_count += WriteReadCheckVal( LW_THERM_ENG_HOLDOFF           ,  0x3     , "LW_THERM_ENG_HOLDOFF"        , 0x00000003 );

    InfoPrintf("%s: %s: Step 5: Waiting for holdoff to be engaged.\n", DEBUG_PRINT , __FUNCTION__);
    do {
        Wait(300);
    } while ( (pSubdev->RegRd32(LW_THERM_ENG_HOLDOFF_ENTER) & 0x3) != 0x3 );

    //-----------------------------------------------------------
    // Step 6 :
    // - Enabling engine scheduling
    // - This will be done by PMU uCode in production
    //-----------------------------------------------------------
    InfoPrintf("%s: %s: Step 6: Enabling engine scheduling.\n", DEBUG_PRINT , __FUNCTION__);
    // UPDATE: Replacing LW_PFIFO_* with LW_RUNLIST_*
    gxpg_reg_err_count += WriteReadCheckVal( device_base + LW_RUNLIST_SCHED_DISABLE        ,  0x1        , "LW_RUNLIST_SCHED_DISABLE"     , 0x0        );

    //-----------------------------------------------------------
    // Step 7 :
    // - Do a GR engine UNBIND
    // - This will be done by PMU uCode in production
    //-----------------------------------------------------------
    InfoPrintf("%s: %s: Step 7: Unbinding the GR engine.\n", DEBUG_PRINT , __FUNCTION__);
    MMUBindGR(1,pSubdev); // UNBIND

    //-----------------------------------------------------------
    // Step 8 :
    // - Assert resets
    // - This will be done by PMU uCode in production
    //-----------------------------------------------------------
    InfoPrintf("%s: %s: Step 8: Assert resets to GR engine.\n", DEBUG_PRINT , __FUNCTION__);
    lwgpu->GetGpuSubdevice()->RegWr32(LW_PGRAPH_PRI_BES_BECS_CTXSW_BE_RESET_CTL,0x000);
    lwgpu->GetGpuSubdevice()->RegWr32(LW_PGRAPH_PRI_FECS_CTXSW_RESET_CTL,0x000);
    lwgpu->GetGpuSubdevice()->RegWr32(LW_PGRAPH_PRI_GPCS_GPCCS_CTXSW_GPC_RESET_CTL,0x000);

    //Force read for flush
    UINT32 readbackData;
    readbackData =  lwgpu->GetGpuSubdevice()->RegRd32(LW_PGRAPH_PRI_FECS_CTXSW_RESET_CTL);
    InfoPrintf("%s: %s: Readback value for LW_PGRAPH_PRI_FECS_CTXSW_RESET_CTL is 0x%08x \n", DEBUG_PRINT , __FUNCTION__, readbackData);

    // Trigger RAM shutdown
    InfoPrintf("%s: %s: Step 8.1: Trigger HW GR ram sequencer to do power gate.\n", DEBUG_PRINT , __FUNCTION__);
    temp1 = pSubdev->RegRd32(LW_PPWR_PMU_RAM_TARGET);
    lwgpu->GetGpuSubdevice()->RegWr32(LW_PPWR_PMU_RAM_TARGET , ((0x00000001 & 0x00000003) | (temp1 & ~(0x00000003)))); 
    gxpg_reg_err_count += ReadCheckVal(LW_PPWR_PMU_RAM_TARGET ,  0x00000003 , "LW_PPWR_PMU_RAM_TARGET_GR" , 0x00000001);  

    UINT32 ppwr_pmu_reg_rd_value;
    InfoPrintf("%s: %s: Step 8.2: Poll HW GR ram sequencer status to ensure GR ram power gating done  .\n", DEBUG_PRINT , __FUNCTION__);
    do {
    ppwr_pmu_reg_rd_value = lwgpu->RegRd32(LW_PPWR_PMU_RAM_STATUS);
    InfoPrintf("PrivReInit::PrivReInitTest: read LW_PPWR_PMU_RAM_STATUS,reg_rd_val=0x%x\n", ppwr_pmu_reg_rd_value);
     } while ((ppwr_pmu_reg_rd_value & 0x00000007) != 0x00000005);

    InfoPrintf("%s: %s: Step 8.3: Trigger HW logic sequencer to do power gate .\n", DEBUG_PRINT , __FUNCTION__);
    temp1 = pSubdev->RegRd32(LW_PPWR_PMU_LOGIC_TARGET);
    lwgpu->GetGpuSubdevice()->RegWr32(LW_PPWR_PMU_LOGIC_TARGET , ((0x00000001 & 0x00000003) | (temp1 & ~(0x00000003)))); 
    gxpg_reg_err_count += ReadCheckVal(LW_PPWR_PMU_LOGIC_TARGET ,  0x00000003 , "LW_PPWR_PMU_LOGIC_TARGET_GR" , 0x00000001);   

    InfoPrintf("%s: %s: Step 8.4: Poll HW logic sequencer status to make sure logic power gating done.\n", DEBUG_PRINT , __FUNCTION__);
    do {
    ppwr_pmu_reg_rd_value = lwgpu->RegRd32(LW_PPWR_PMU_LOGIC_STATUS);
    InfoPrintf("PrivReInit::PrivReInitTest: read LW_PPWR_PMU_LOGIC_STATUS,reg_rd_val=0x%x\n", ppwr_pmu_reg_rd_value);
     } while ((ppwr_pmu_reg_rd_value & 0x00000007) != 0x00000002);

    //InfoPrintf("%s: %s: Step 8.5: If L2 RPPG is enabled, Trigger L2 RAM sequencer to enter RPPG.\n", DEBUG_PRINT , __FUNCTION__);
    //temp1 = pSubdev->RegRd32( LW_PPWR_PMU_RAM_CTRL);
    //lwgpu->GetGpuSubdevice()->RegWr32(LW_PPWR_PMU_RAM_CTRL , ((0x00000010 & 0x00000030) | (temp1 & ~(0x00000030))) );
    //gxpg_reg_err_count += ReadCheckVal(LW_PPWR_PMU_RAM_CTRL ,  0x00000030 , "LW_PPWR_PMU_RAM_CTRL_NGR" , 0x00000010);  
    //InfoPrintf("%s: %s: Step 8.6: If L2 RPPG is enabled, Poll for RPPG entry completion.\n", DEBUG_PRINT , __FUNCTION__);
    //do {
    //ppwr_pmu_reg_rd_value = lwgpu->RegRd32(LW_PPWR_PMU_RAM_STATUS);
    //InfoPrintf("PrivReInit::PrivReInitTest: read LW_PPWR_PMU_RAM_STATUS,reg_rd_val=0x%x\n", ppwr_pmu_reg_rd_value);
    // } while ((ppwr_pmu_reg_rd_value & 0x00000070) != 0x00000020);

    //-----------------------------------------------------------
    // Step 9 :
    // - Clear the PG_ON interrupt
    // - This will be done by PMU uCode in production
    //-----------------------------------------------------------
//    InfoPrintf("%s: %s: Step 9: Clearing PG_ON interrupt.\n", DEBUG_PRINT , __FUNCTION__);
//    lwgpu->GetGpuSubdevice()->RegWr32(LW_PPWR_PMU_PG_INTRSTAT(0),0x10);
//    gxpg_reg_err_count += ReadCheckVal( LW_PPWR_PMU_PG_INTRSTAT(0) ,  0x10 , "LW_PPWR_PMU_PG_INTRSTAT(0)" , 0x00    );

    //-----------------------------------------------------------
    // Step 10 :
    // - Wait for PG_ON_DONE to be asserted by ELPG controller
    //-----------------------------------------------------------
//    InfoPrintf("%s: %s: Step 10: Waiting for PG_ON_DONE interrupt to be raised.\n", DEBUG_PRINT , __FUNCTION__);
//    
//    int ppwr_pmu_pg_intrstat_reg_value;
//    int ppwr_pmu_pg_intrstat_reg_mask = 0x100;
//    do {
//        Wait(2500);
//        ppwr_pmu_pg_intrstat_reg_value = lwgpu->RegRd32(LW_PPWR_PMU_PG_INTRSTAT(0)); 
//      } while ((ppwr_pmu_pg_intrstat_reg_value & ppwr_pmu_pg_intrstat_reg_mask) != 0x100);
//
//    InfoPrintf("%s: Read correct: LW_PPWR_PMU_PG_INTRSTAT(0), Expected: 0x100(0x100 with mask), Actual: 0x%08x(0x%08x with mask)\n", DEBUG_PRINT, ppwr_pmu_pg_intrstat_reg_value, (ppwr_pmu_pg_intrstat_reg_value & ppwr_pmu_pg_intrstat_reg_mask));

    //-----------------------------------------------------------
    // Step 11 :
    // - Clear the PG_ON_DONE interrupt
    // - This will be done by PMU uCode in production
    //-----------------------------------------------------------
   // InfoPrintf("%s: %s: Step 11: Clearing PG_ON_DONE interrupt.\n", DEBUG_PRINT , __FUNCTION__);
   // lwgpu->GetGpuSubdevice()->RegWr32(LW_PPWR_PMU_PG_INTRSTAT(0),0x100);
   // gxpg_reg_err_count += ReadCheckVal( LW_PPWR_PMU_PG_INTRSTAT(0)        ,  0x100        , "LW_PPWR_PMU_PG_INTRSTAT(0)"     , 0x000    );

    //Stay in ELPG for sometime.
    Wait(500);

    // Issue an ilwalidate request to make sure it does not hang as a result of GPCMMU being powered down [see bug 555852]
    MMUIlwalidate(pSubdev);

    //Stay in ELPG for sometime.
    Wait(500);

    //-----------------------------------------------------------
    // Step 12 :
    // - Write to PGOFF register to initiate power off
    // - This will be done by PMU uCode in production
    //-----------------------------------------------------------
//    InfoPrintf("%s: %s: Step 12: Writing to PGOFF register to initiate power off.\n", DEBUG_PRINT , __FUNCTION__);
//    gxpg_reg_err_count += WriteReadCheckVal( LW_PPWR_PMU_PG_PGOFF        ,  0x1        , "LW_PPWR_PMU_PG_PGOFF"     , 0x1    );

    //-----------------------------------------------------------
    // Power Up sequence
    //-----------------------------------------------------------
    InfoPrintf("%s: %s: Starting ELPG Power Up sequence.\n", DEBUG_PRINT , __FUNCTION__);

    //-----------------------------------------------------------
    // Step 13 :
    // - Wait for ENG_RST to be asserted by ELPG controller
    //-----------------------------------------------------------
  //  InfoPrintf("%s: %s: Step 13: Waiting for ENG_RST interrupt to be raised.\n", DEBUG_PRINT , __FUNCTION__);
  //  do {
  //      Wait(300);
  //  } while ( ReadCheckVal( LW_PPWR_PMU_PG_INTRSTAT(0) , 0x100000 , "LW_PPWR_PMU_PG_INTRSTAT(0)" , 0x100000 ) != 0 );

    //-----------------------------------------------------------
    // Step 14 :
    // - Clear the ENG_RST interrupt
    // - This will be done by PMU uCode in production
    //-----------------------------------------------------------
  //  InfoPrintf("%s: %s: Step 14: Clearing ENG_RST interrupt.\n", DEBUG_PRINT , __FUNCTION__);
  //  lwgpu->GetGpuSubdevice()->RegWr32(LW_PPWR_PMU_PG_INTRSTAT(0),0x100000) ;
  //  gxpg_reg_err_count += ReadCheckVal( LW_PPWR_PMU_PG_INTRSTAT(0)        ,  0x100000        , "LW_PPWR_PMU_PG_INTRSTAT(0)"     , 0x000000    );

    //-----------------------------------------------------------
    // Step 15 :
    // - Wait for CTX_RESTORE to be asserted by ELPG controller
    //-----------------------------------------------------------
  //  InfoPrintf("%s: %s: Step 15: Waiting for CTX_RESTORE interrupt to be raised.\n", DEBUG_PRINT , __FUNCTION__);
  //  do {
  //      Wait(300);
  //  } while ( ReadCheckVal( LW_PPWR_PMU_PG_INTRSTAT(0) , 0x1000 , "LW_PPWR_PMU_PG_INTRSTAT(0)" , 0x1000 ) != 0 );

  //  // Intermediate steps for volta
  //  InfoPrintf("%s: %s: Step 15.1: If L2 RPPG is enabled, Trigger L2 RAM sequencer to exit RPPG \n", DEBUG_PRINT , __FUNCTION__);
  //  temp1 = pSubdev->RegRd32( LW_PPWR_PMU_RAM_CTRL);
  //  lwgpu->GetGpuSubdevice()->RegWr32(LW_PPWR_PMU_RAM_CTRL , ((0x00000030 & 0x00000030) | (temp1 & ~(0x00000030)))); 
  //  gxpg_reg_err_count += ReadCheckVal(LW_PPWR_PMU_RAM_CTRL ,  0x00000030 , "LW_PPWR_PMU_RAM_CTRL_NGR" , 0x00000030); 

  //  InfoPrintf("%s: %s: Step 15.2: If L2 RPPG is enabled, Poll for RPPG exit completion \n", DEBUG_PRINT , __FUNCTION__);
  //  do {
  //  ppwr_pmu_reg_rd_value = lwgpu->RegRd32(LW_PPWR_PMU_RAM_STATUS);
  //  InfoPrintf("PrivReInit::PrivReInitTest: read LW_PPWR_PMU_RAM_STATUS,reg_rd_val=0x%x\n", ppwr_pmu_reg_rd_value);
  //   } while ((ppwr_pmu_reg_rd_value & 0x00000070) != 0x00000000);

    InfoPrintf("%s: %s: Step 15.3: Trigger HW logic sequencer to exit power gate \n", DEBUG_PRINT , __FUNCTION__);
    temp1 = pSubdev->RegRd32( LW_PPWR_PMU_LOGIC_TARGET);
    lwgpu->GetGpuSubdevice()->RegWr32(LW_PPWR_PMU_LOGIC_TARGET , ((0x00000000 & 0x00000003) | (temp1 & ~(0x00000003)))); 
    gxpg_reg_err_count += ReadCheckVal(LW_PPWR_PMU_LOGIC_TARGET , 0x00000003, "LW_PPWR_PMU_LOGIC_TARGET_GR" , 0x00000000);  

    InfoPrintf("%s: %s: Step 15.4: Poll HW logic sequencer status to ensure logic power gating exit done\n", DEBUG_PRINT , __FUNCTION__);
    do {
    ppwr_pmu_reg_rd_value = lwgpu->RegRd32(LW_PPWR_PMU_LOGIC_STATUS);
    InfoPrintf("PrivReInit::PrivReInitTest: read LW_PPWR_PMU_LOGIC_STATUS,reg_rd_val=0x%x\n", ppwr_pmu_reg_rd_value);
     } while ((ppwr_pmu_reg_rd_value & 0x00000007) != 0x00000000);

    InfoPrintf("%s: %s: Step 15.5: Trigger HW GR ram sequencer to exit power gate \n", DEBUG_PRINT , __FUNCTION__);
    temp1 = pSubdev->RegRd32( LW_PPWR_PMU_RAM_TARGET);
    lwgpu->GetGpuSubdevice()->RegWr32(LW_PPWR_PMU_RAM_TARGET , ((0x00000000 & 0x00000003) | (temp1 & ~(0x00000003)))); 
    gxpg_reg_err_count += ReadCheckVal(LW_PPWR_PMU_RAM_TARGET ,  0x00000003 , "LW_PPWR_PMU_RAM_TARGET_GR" , 0x00000000);  

    InfoPrintf("%s: %s: Step 15.6: Poll HW GR ram sequencer status to ensure GR ram PG exit done\n", DEBUG_PRINT , __FUNCTION__);
    do {
    ppwr_pmu_reg_rd_value = lwgpu->RegRd32(LW_PPWR_PMU_RAM_STATUS);
    InfoPrintf("PrivReInit::PrivReInitTest: read LW_PPWR_PMU_RAM_STATUS,reg_rd_val=0x%x\n", ppwr_pmu_reg_rd_value);
     } while ((ppwr_pmu_reg_rd_value & 0x00000007) != 0x00000000);

    //-----------------------------------------------------------
    // Step 16 :
    // - De-Assert resets
    // - This will be done by PMU uCode in production
    //-----------------------------------------------------------
    InfoPrintf("%s: %s: Step 16: De-assert resets to GR engine.\n", DEBUG_PRINT , __FUNCTION__);
    lwgpu->GetGpuSubdevice()->RegWr32(LW_PGRAPH_PRI_BES_BECS_CTXSW_BE_RESET_CTL,0x440);
    lwgpu->GetGpuSubdevice()->RegWr32(LW_PGRAPH_PRI_FECS_CTXSW_RESET_CTL,0x550);
    lwgpu->GetGpuSubdevice()->RegWr32(LW_PGRAPH_PRI_GPCS_GPCCS_CTXSW_GPC_RESET_CTL,0xA20);

    //Force read for flush
    UINT32 readbackData2;
    readbackData2 =  lwgpu->GetGpuSubdevice()->RegRd32(LW_PGRAPH_PRI_FECS_CTXSW_RESET_CTL);
    InfoPrintf("%s: %s: Readback value for LW_PGRAPH_PRI_FECS_CTXSW_RESET_CTL is 0x%08x \n", DEBUG_PRINT , __FUNCTION__, readbackData2);

    //-----------------------------------------------------------
    // Step 17 :
    // - Release holdoffs
    // - This will be done by PMU uCode in production
    //-----------------------------------------------------------
    InfoPrintf("%s: %s: Step 17: Disabling holdoff.\n", DEBUG_PRINT , __FUNCTION__);
    gxpg_reg_err_count += WriteReadCheckVal( LW_THERM_ENG_HOLDOFF           ,  0x21     , "LW_THERM_ENG_HOLDOFF"        , 0x00000000 );

    //-----------------------------------------------------------
    // Step 18 :
    // - Clear the CTX_RESTORE interrupt
    // - This will be done by PMU uCode in production
    //-----------------------------------------------------------
 //   InfoPrintf("%s: %s: Step 18: Clearing CTX_RESTORE interrupt.\n", DEBUG_PRINT , __FUNCTION__);
 //   lwgpu->GetGpuSubdevice()->RegWr32(LW_PPWR_PMU_PG_INTRSTAT(0),0x1000);
 //   gxpg_reg_err_count += ReadCheckVal( LW_PPWR_PMU_PG_INTRSTAT(0)        ,  0x1000        , "LW_PPWR_PMU_PG_INTRSTAT(0)"     , 0x0000    );

    //-------------------------------------------------------------------------
    // Stage 4 : Exit TPC-PG for TPC3
    //  TPC-PG exit programming for TPC3
    //    1) Write LW_PCHIPLET_PWR_GPCS_LOGIC_CTRL_TPC3 bits to _EXIT
    //    2) Poll for LW_PCHIPLET_PWR_GPCS_LOGIC_STATUS_TPC3 bits to _POWER_ON
    //    3) Write LW_PCHIPLET_PWR_GPCS_RAM_CTRL_TPC3 bits to _EXIT
    //    4) Poll for LW_PCHIPLET_PWR_GPCS_RAM_STATUS_TPC3 bits to _POWER_ON
    //-------------------------------------------------------------------------
//   InfoPrintf("%s: %s: Stage 4: Exit TPC-PG for TPC3.\n", DEBUG_PRINT , __FUNCTION__);

//    InfoPrintf("%s: %s: Step b.1: Trigger HW logic sequencer to exit TPC power gating \n", DEBUG_PRINT , __FUNCTION__);
//    temp1 = pSubdev->RegRd32(LW_PCHIPLET_PWR_GPCS_LOGIC_CTRL);
//    pSubdev->RegWr32(LW_PCHIPLET_PWR_GPCS_LOGIC_CTRL , ((0x00003000 & 0x00003000) | (temp1 & ~(0x00003000)))); 
//    gxpg_reg_err_count += ReadCheckVal(LW_PCHIPLET_PWR_GPCS_LOGIC_CTRL ,  0x00003000 , "LW_PCHIPLET_PWR_GPCS_LOGIC_CTRL_TPC3", 0x00003000);  
//
//    InfoPrintf("%s: %s: Step b.2: Poll HW logic sequencer status to ensure logic power gating exit done\n", DEBUG_PRINT , __FUNCTION__);
//    do {
//    pwr_gpcs_reg_rd_value = lwgpu->RegRd32(LW_PCHIPLET_PWR_GPCS_LOGIC_STATUS);
//    InfoPrintf("PrivReInit::PrivReInitTest: read LW_PCHIPLET_PWR_GPCS_LOGIC_STATUS,reg_rd_val=0x%x\n", pwr_gpcs_reg_rd_value);
//     } while ((pwr_gpcs_reg_rd_value & 0x00007000) != 0x00000000);
//
//    InfoPrintf("%s: %s: Step b.3: Trigger HW GR ram sequencer to exit TPC power gating \n", DEBUG_PRINT , __FUNCTION__);
//    temp1 = pSubdev->RegRd32(LW_PCHIPLET_PWR_GPCS_RAM_CTRL);
//    pSubdev->RegWr32(LW_PCHIPLET_PWR_GPCS_RAM_CTRL , ((0x00003000 & 0x00003000) | (temp1 & ~(0x00003000)))); 
//    gxpg_reg_err_count += ReadCheckVal(LW_PCHIPLET_PWR_GPCS_RAM_CTRL ,  0x00003000, "LW_PCHIPLET_PWR_GPCS_RAM_CTRL_TPC3", 0x00003000);  
//
//    InfoPrintf("%s: %s: Step b.4: Poll HW GR ram sequencer status to ensure TPC ram PG exit done\n", DEBUG_PRINT , __FUNCTION__);
//    do {
//    pwr_gpcs_reg_rd_value = lwgpu->RegRd32(LW_PCHIPLET_PWR_GPCS_RAM_STATUS);
//    InfoPrintf("PrivReInit::PrivReInitTest: read LW_PCHIPLET_PWR_GPCS_RAM_STATUS,reg_rd_val=0x%x\n", pwr_gpcs_reg_rd_value);
//     } while ((pwr_gpcs_reg_rd_value & 0x00007000) != 0x00000000);

    //-----------------------------------------------------------
    // Step 19 :
    // - Turn of ELPG interrupts before shutdown
    // - This will be done by PMU uCode in production
    //-----------------------------------------------------------
    InfoPrintf("%s: %s: Step 19: Turning off ELPG interrupts before shutdown.\n", DEBUG_PRINT , __FUNCTION__);

    gxpg_reg_err_count += WriteReadCheckVal( LW_PPWR_PMU_PG_INTREN(0)         ,  0x1       , "LW_PPWR_PMU_PG_INTREN(0)"      , 0x00000    );
//    gxpg_reg_err_count += WriteReadCheckVal( LW_PPWR_PMU_PG_CFG_RDY      ,  0x1       , "LW_PPWR_PMU_PG_CFG_RDY"   , 0x0        );

    //-----------------------------------------------------------
    // Do a GR engine BIND followed by an HUBMMU ILWALIDATE  to
    // check that the ilwalidates get sent to GPCMMU when powered up.
    //-----------------------------------------------------------
    if(!skip_MMUBind) {
        MMUBindGR(0,pSubdev); // BIND
    }

    MMUIlwalidate(pSubdev);

    //-----------------------------------------------------------
    // Stage 5 : Final checks
    //-----------------------------------------------------------

    if(gxpg_reg_err_count!=0)
    {
        InfoPrintf("%s: %s: Test complete. GXPG errors = %d\n", DEBUG_PRINT , __FUNCTION__, gxpg_reg_err_count);
        ErrPrintf("%s: %s: Register test failed. \n", DEBUG_PRINT , __FUNCTION__ );
        return 1;
    }
    else
    {
        return 0;
    }
}

//****************************************************************************************
// Railgating test
//****************************************************************************************
UINT32
Elpg_Powergv_Kepler::elpg_powergv_railgating_Test() {

    GpuSubdevice *pSubdev = lwgpu->GetGpuSubdevice();

    InfoPrintf("%s: %s: Starting railgating test.\n", DEBUG_PRINT , __FUNCTION__);
    //-----------------------------------------------------------
    // Stage 1 : Initialization
    //-----------------------------------------------------------
    InfoPrintf("%s: %s: Stage 1: Initialization.\n", DEBUG_PRINT , __FUNCTION__);

    //Step -3: Clear PMU in Reset
    int temp1 = pSubdev->RegRd32( LW_PMC_ENABLE);
    pSubdev->RegWr32( LW_PMC_ENABLE, temp1 | 0x0000002d ); //Enable PRIV ring, XBAR
    pSubdev->RegRd32( LW_PMC_ENABLE);

    pSubdev->RegWr32(LW_PPRIV_MASTER_RING_COMMAND_DATA , 0x503B5443);

    PrivInit();

    temp1 = pSubdev->RegRd32( LW_PMC_ENABLE);
    pSubdev->RegWr32( LW_PMC_ENABLE, temp1 | 0xffffffff ); //Enable all engines after PRIV init is complete.
    pSubdev->RegRd32( LW_PMC_ENABLE);

    pSubdev->RegWr32( LW_PMC_ELPG_ENABLE,0x20000000 ); //Enable Hub
    pSubdev->RegRd32( LW_PMC_ELPG_ENABLE);

    int reg_list_iter;
    int gxpg_reg_err_count;

    gxpg_reg_err_count = 0;

    InfoPrintf("%s: %s: Step 1: Initialzing ELPG registers with PROD values.\n", DEBUG_PRINT , __FUNCTION__);

    for (reg_list_iter=0; reg_list_iter<=REG_LIST_INDEX_MAX-4; reg_list_iter++)
    {
        gxpg_reg_err_count += WriteReadCheckVal( reg_list[reg_list_iter][REG_DEFINE], reg_list[reg_list_iter][MASK_VAL], reg_list_name[reg_list_iter], reg_list[reg_list_iter][PROD_VAL] );
    }

    //Special loop for testing GPC indexed addresses
    //Normal GPC Partitions
    for (reg_list_iter=0; reg_list_iter<=PER_GPC_ELPG_NUM_PARTITIONS_MAX; reg_list_iter++)
    {
        gxpg_reg_err_count += WriteReadCheckVal( reg_list[REG_LIST_INDEX_MAX-3][REG_DEFINE], reg_list[REG_LIST_INDEX_MAX-3][MASK_VAL], reg_list_name[REG_LIST_INDEX_MAX-3], reg_list_iter);
        lwgpu->GetGpuSubdevice()->RegWr32(reg_list[REG_LIST_INDEX_MAX][REG_DEFINE],reg_list[REG_LIST_INDEX_MAX][PROD_VAL]);
    }
    for (reg_list_iter=0; reg_list_iter<=PER_GPC_ELPG_NUM_PARTITIONS_MAX; reg_list_iter++)
    {
        gxpg_reg_err_count += WriteReadCheckVal( reg_list[REG_LIST_INDEX_MAX-3][REG_DEFINE], reg_list[REG_LIST_INDEX_MAX-3][MASK_VAL], reg_list_name[REG_LIST_INDEX_MAX-3], reg_list_iter);
        gxpg_reg_err_count += ReadCheckVal( reg_list[REG_LIST_INDEX_MAX][REG_DEFINE]  , reg_list[REG_LIST_INDEX_MAX][MASK_VAL]  , reg_list_name[REG_LIST_INDEX_MAX]  , reg_list[REG_LIST_INDEX_MAX][PROD_VAL]  );
     }

    //GPC PGRAM partitions
     for (reg_list_iter=0; reg_list_iter<=PER_GPC_PGRAM_NUM_PARTITIONS_MAX; reg_list_iter++)
    {
        gxpg_reg_err_count += WriteReadCheckVal( reg_list[REG_LIST_INDEX_MAX-3][REG_DEFINE], reg_list[REG_LIST_INDEX_MAX-3][MASK_VAL], reg_list_name[REG_LIST_INDEX_MAX-3], reg_list_iter);
        lwgpu->GetGpuSubdevice()->RegWr32(reg_list[REG_LIST_INDEX_MAX-2][REG_DEFINE],reg_list[REG_LIST_INDEX_MAX-2][PROD_VAL]);
    }
    for (reg_list_iter=0; reg_list_iter<=PER_GPC_PGRAM_NUM_PARTITIONS_MAX; reg_list_iter++)
    {
        gxpg_reg_err_count += WriteReadCheckVal( reg_list[REG_LIST_INDEX_MAX-3][REG_DEFINE], reg_list[REG_LIST_INDEX_MAX-3][MASK_VAL], reg_list_name[REG_LIST_INDEX_MAX-3], reg_list_iter);
        gxpg_reg_err_count += ReadCheckVal( reg_list[REG_LIST_INDEX_MAX-2][REG_DEFINE]  , reg_list[REG_LIST_INDEX_MAX-2][MASK_VAL]  , reg_list_name[REG_LIST_INDEX_MAX-2]  , reg_list[REG_LIST_INDEX_MAX-2][PROD_VAL]  );
     }

    //GPC NPGRAM Partitions
     for (reg_list_iter=0; reg_list_iter<=PER_GPC_NPGRAM_NUM_PARTITIONS_MAX; reg_list_iter++)
    {
        gxpg_reg_err_count += WriteReadCheckVal( reg_list[REG_LIST_INDEX_MAX-3][REG_DEFINE], reg_list[REG_LIST_INDEX_MAX-3][MASK_VAL], reg_list_name[REG_LIST_INDEX_MAX-3], reg_list_iter);
        lwgpu->GetGpuSubdevice()->RegWr32(reg_list[REG_LIST_INDEX_MAX-1][REG_DEFINE],reg_list[REG_LIST_INDEX_MAX-1][PROD_VAL]);
    }
    for (reg_list_iter=0; reg_list_iter<=PER_GPC_NPGRAM_NUM_PARTITIONS_MAX; reg_list_iter++)
    {
        gxpg_reg_err_count += WriteReadCheckVal( reg_list[REG_LIST_INDEX_MAX-3][REG_DEFINE], reg_list[REG_LIST_INDEX_MAX-3][MASK_VAL], reg_list_name[REG_LIST_INDEX_MAX-3], reg_list_iter);
        gxpg_reg_err_count += ReadCheckVal( reg_list[REG_LIST_INDEX_MAX-1][REG_DEFINE]  , reg_list[REG_LIST_INDEX_MAX-1][MASK_VAL]  , reg_list_name[REG_LIST_INDEX_MAX-1]  , reg_list[REG_LIST_INDEX_MAX-1][PROD_VAL]  );
     }

    //-----------------------------------------------------------
    // Stage 2 : Railgating Sequence
    //-----------------------------------------------------------
    InfoPrintf("%s: %s: Stage 2: Railgating Sequence.\n", DEBUG_PRINT , __FUNCTION__);

    //Ensure STATUS is exit Done
    InfoPrintf("Step 2: Poll for Exit Done in the beginnig\n");
    //The below register is no longer valid for volta & beyond. So removing it from the test.
    //while ( ReadCheckVal( LW_PPWR_PMU_RAMS_RG_CTRL , 0xC0000000 , "LW_PPWR_PMU_RAMS_RG_CTRL" , 0x40000000 ) != 0 );
    InfoPrintf("Step 3: Exit Done after Initial Boot up.\n");
    //Trigger Entry
    InfoPrintf("Step 4: Trigger ENTRY\n");
    //The below register is no longer valid for volta & beyond. So removing it from the test.
    //lwgpu->GetGpuSubdevice()->RegWr32(LW_PPWR_PMU_RAMS_RG_CTRL, 0x40000001);
    //Poll for Entry done
    InfoPrintf("Step 5: Poll for Entry done bit de-asserted.\n");
    //The below register is no longer valid for volta & beyond. So removing it from the test.
    //while ( ReadCheckVal( LW_PPWR_PMU_RAMS_RG_CTRL , 0x00000001 , "LW_PPWR_PMU_RAMS_RG_CTRL" , 0x00000000 ) != 0 );
    InfoPrintf("Step 6: ENTRY done bit de-asserted\n");
    //Also check status Bit.
    InfoPrintf("Step 7: Poll for Entry done Status Bit\n");
    //The below register is no longer valid for volta & beyond. So removing it from the test.
    //while ( ReadCheckVal( LW_PPWR_PMU_RAMS_RG_CTRL , 0xC0000000 , "LW_PPWR_PMU_RAMS_RG_CTRL" , 0xC0000000 ) != 0 );
    InfoPrintf("Step 8: ENTRY done according to status Bit\n");

    //-----------------------------------------------------------
    // Stage 3 : Final checks
    //-----------------------------------------------------------

    if(gxpg_reg_err_count!=0)
    {
        InfoPrintf("%s: %s: Test complete. Railgating errors = %d\n", DEBUG_PRINT , __FUNCTION__, gxpg_reg_err_count);
        ErrPrintf("%s: %s: Railgating test failed. \n", DEBUG_PRINT , __FUNCTION__ );
        return 1;
    }
    else
    {
        return 0;
    }
}

//****************************************************************************************
// Test Graphic Engine Power Gating - Kepler
//****************************************************************************************
UINT32
Elpg_Powergv_Kepler::elpg_powergv_gx_keplerTest() {

  GpuSubdevice *pSubdev = lwgpu->GetGpuSubdevice();

  //-----------------------------------------------------------
  // Stage 1 : Initialization
  //----------------------------------------------------------
  InfoPrintf("Graphic ELPG Power Gating Test : Stage 1: Initialization\n");

  //check clamp_en and sleep signal to make sure that when the fuse is not blown
  bool fuse_blown = false;
  if(check_clamp_sleep_gr(fuse_blown) == 1) return 1;
//  if(check_clamp_sleep_vd(fuse_blown) == 1) return 1;

  //Step -3: Clear PMU in Reset
  int temp1 = pSubdev->RegRd32( LW_PMC_ENABLE);
  pSubdev->RegWr32( LW_PMC_ENABLE, temp1 | 0x0000002d ); //Enable PRIV ring, XBAR
  pSubdev->RegRd32( LW_PMC_ENABLE);

  pSubdev->RegWr32(LW_PPRIV_MASTER_RING_COMMAND_DATA , 0x503B5443);

  PrivInit();

//  if(fspg_option != 0x0){
//    pSubdev->RegWr32(LW_PTOP_FS_CONTROL_GPC_TPC(0), fspg_option);
//    InfoPrintf("Graphic ELPG Power Gating Test : Stage 1.5: FSPG Configuration Applied\n");
//  }

  temp1 = pSubdev->RegRd32( LW_PMC_ENABLE);
  pSubdev->RegWr32( LW_PMC_ENABLE, temp1 | 0xffffffff ); //Enable all engines after PRIV init is complete.
  pSubdev->RegRd32( LW_PMC_ENABLE);

  if(register_check)
  {
    pSubdev->RegWr32( LW_PGRAPH_PRI_GPCS_TPCS_TEX_IN_PG,   0x240200);
    pSubdev->RegWr32( LW_PGRAPH_PRI_GPCS_TPCS_TEX_X_PG,    0x240200);
    pSubdev->RegWr32( LW_PGRAPH_PRI_GPCS_TPCS_TEX_D_PG,    0x200);
    pSubdev->RegWr32( LW_PGRAPH_PRI_GPCS_TPCS_TEX_T_PG,    0x40200);
    pSubdev->RegWr32( LW_PGRAPH_PRI_GPCS_TPCS_PE_PG,       0x240200);
    pSubdev->RegWr32( LW_PGRAPH_PRI_GPCS_TPCS_L1C_PG,      0x240200);
    pSubdev->RegWr32( LW_PGRAPH_PRI_GPCS_PPCS_WWDX_PG,     0x240200);
    pSubdev->RegWr32( LW_PGRAPH_PRI_GPCS_PPCS_CBM_PG,      0x200);
    pSubdev->RegWr32( LW_PGRAPH_PRI_GPCS_TPCS_MPC_PG,      0x40200);
    pSubdev->RegWr32( LW_PGRAPH_PRI_GPCS_TPCS_SM_BLPG_0_PG,   0x240200);
    pSubdev->RegWr32( LW_PGRAPH_PRI_GPCS_TPCS_SM_BLPG_1_PG,   0x240200);
    pSubdev->RegWr32( LW_PGRAPH_PRI_GPCS_TPCS_SM_BLPG_2_PG,   0x240200);
    pSubdev->RegWr32( LW_PGRAPH_PRI_GPCS_TPCS_SM_BLPG_3_PG,   0x240200);
    pSubdev->RegWr32( LW_PGRAPH_PRI_GPCS_TPCS_SM_BLPG_4_PG,   0x240200);
    pSubdev->RegWr32( LW_PGRAPH_PRI_GPCS_TPCS_SM_BLPG_5_PG,   0x240200);

    pSubdev->RegWr32( LW_PGRAPH_PRI_GPCS_TPCS_TEX_IN_PG1,  0xaabbaaaa);
    pSubdev->RegWr32( LW_PGRAPH_PRI_GPCS_TPCS_TEX_X_PG1,   0x12121212);
    pSubdev->RegWr32( LW_PGRAPH_PRI_GPCS_TPCS_TEX_D_PG1,   0x223332);
    pSubdev->RegWr32( LW_PGRAPH_PRI_GPCS_TPCS_TEX_T_PG1,   0x333fff);
    pSubdev->RegWr32( LW_PGRAPH_PRI_GPCS_TPCS_PE_PG1,      0xffffff);
    pSubdev->RegWr32( LW_PGRAPH_PRI_GPCS_TPCS_L1C_PG1,     0xf0f0f0);
    pSubdev->RegWr32( LW_PGRAPH_PRI_GPCS_PPCS_WWDX_PG1,    0x6a6a6a);
    pSubdev->RegWr32( LW_PGRAPH_PRI_GPCS_PPCS_CBM_PG1,     0x777777);
    pSubdev->RegWr32( LW_PGRAPH_PRI_GPCS_TPCS_MPC_PG1,     0x889999);
    pSubdev->RegWr32( LW_PGRAPH_PRI_GPCS_TPCS_SM_BLPG_0_PG1,      0x994444);
    pSubdev->RegWr32( LW_PGRAPH_PRI_GPCS_TPCS_SM_BLPG_1_PG1,      0x994444);
    pSubdev->RegWr32( LW_PGRAPH_PRI_GPCS_TPCS_SM_BLPG_2_PG1,      0x994444);
    pSubdev->RegWr32( LW_PGRAPH_PRI_GPCS_TPCS_SM_BLPG_3_PG1,      0x994444);
    pSubdev->RegWr32( LW_PGRAPH_PRI_GPCS_TPCS_SM_BLPG_4_PG1,      0x994444);
    pSubdev->RegWr32( LW_PGRAPH_PRI_GPCS_TPCS_SM_BLPG_5_PG1,      0x994444);

    WriteReadCheckVal( LW_PGRAPH_PRI_GPCS_TPCS_TEX_IN_PG,  0xffffffff, "LW_PGRAPH_PRI_GPCS_TPCS_TEX_IN_PG", 0x240200   );
    WriteReadCheckVal( LW_PGRAPH_PRI_GPCS_TPCS_TEX_X_PG,   0xffffffff, "LW_PGRAPH_PRI_GPCS_TPCS_TEX_X_PG",  0x240200 );
    WriteReadCheckVal( LW_PGRAPH_PRI_GPCS_TPCS_TEX_D_PG,   0xffffffff, "LW_PGRAPH_PRI_GPCS_TPCS_TEX_D_PG",  0x200      );
    WriteReadCheckVal( LW_PGRAPH_PRI_GPCS_TPCS_TEX_T_PG,   0xffffffff, "LW_PGRAPH_PRI_GPCS_TPCS_TEX_T_PG",  0x40200    );
    WriteReadCheckVal( LW_PGRAPH_PRI_GPCS_TPCS_PE_PG,      0xffffffff, "LW_PGRAPH_PRI_GPCS_TPCS_PE_PG",     0x240200   );
    WriteReadCheckVal( LW_PGRAPH_PRI_GPCS_TPCS_L1C_PG,     0xffffffff, "LW_PGRAPH_PRI_GPCS_TPCS_L1C_PG",    0x240200 );
    WriteReadCheckVal( LW_PGRAPH_PRI_GPCS_PPCS_WWDX_PG,    0xffffffff, "LW_PGRAPH_PRI_GPCS_PPCS_WWDX_PG",   0x240200   );
    WriteReadCheckVal( LW_PGRAPH_PRI_GPCS_PPCS_CBM_PG,     0xffffffff, "LW_PGRAPH_PRI_GPCS_PPCS_CBM_PG",    0x200      );
    WriteReadCheckVal( LW_PGRAPH_PRI_GPCS_TPCS_MPC_PG,     0xffffffff, "LW_PGRAPH_PRI_GPCS_TPCS_MPC_PG",    0x40200    );
    WriteReadCheckVal( LW_PGRAPH_PRI_GPCS_TPCS_SM_BLPG_0_PG,      0xffffffff, "LW_PGRAPH_PRI_GPCS_TPCS_SM_BLPG_0_PG",     0x240200 );
    WriteReadCheckVal( LW_PGRAPH_PRI_GPCS_TPCS_SM_BLPG_1_PG,      0xffffffff, "LW_PGRAPH_PRI_GPCS_TPCS_SM_BLPG_1_PG",     0x240200 );
    WriteReadCheckVal( LW_PGRAPH_PRI_GPCS_TPCS_SM_BLPG_2_PG,      0xffffffff, "LW_PGRAPH_PRI_GPCS_TPCS_SM_BLPG_2_PG",     0x240200 );
    WriteReadCheckVal( LW_PGRAPH_PRI_GPCS_TPCS_SM_BLPG_3_PG,      0xffffffff, "LW_PGRAPH_PRI_GPCS_TPCS_SM_BLPG_3_PG",     0x240200 );
    WriteReadCheckVal( LW_PGRAPH_PRI_GPCS_TPCS_SM_BLPG_4_PG,      0xffffffff, "LW_PGRAPH_PRI_GPCS_TPCS_SM_BLPG_4_PG",     0x240200 );
    WriteReadCheckVal( LW_PGRAPH_PRI_GPCS_TPCS_SM_BLPG_5_PG,      0xffffffff, "LW_PGRAPH_PRI_GPCS_TPCS_SM_BLPG_5_PG",     0x240200 );

    WriteReadCheckVal( LW_PGRAPH_PRI_GPCS_TPCS_TEX_IN_PG1, 0xffffffff, "LW_PGRAPH_PRI_GPCS_TPCS_TEX_IN_PG1", 0xaabbaaaa);
    WriteReadCheckVal( LW_PGRAPH_PRI_GPCS_TPCS_TEX_X_PG1,  0xffffffff, "LW_PGRAPH_PRI_GPCS_TPCS_TEX_X_PG1",  0x12121212);
    WriteReadCheckVal( LW_PGRAPH_PRI_GPCS_TPCS_TEX_D_PG1,  0xffffffff, "LW_PGRAPH_PRI_GPCS_TPCS_TEX_D_PG1",  0x223332);
    WriteReadCheckVal( LW_PGRAPH_PRI_GPCS_TPCS_TEX_T_PG1,  0xffffffff, "LW_PGRAPH_PRI_GPCS_TPCS_TEX_T_PG1",  0x333fff);
    WriteReadCheckVal( LW_PGRAPH_PRI_GPCS_TPCS_PE_PG1,     0xffffffff, "LW_PGRAPH_PRI_GPCS_TPCS_PE_PG1",     0xffffff);
    WriteReadCheckVal( LW_PGRAPH_PRI_GPCS_TPCS_L1C_PG1,    0xffffffff, "LW_PGRAPH_PRI_GPCS_TPCS_L1C_PG1",    0xf0f0f0);
    WriteReadCheckVal( LW_PGRAPH_PRI_GPCS_PPCS_WWDX_PG1,   0xffffffff, "LW_PGRAPH_PRI_GPCS_PPCS_WWDX_PG1",   0x6a6a6a);
    WriteReadCheckVal( LW_PGRAPH_PRI_GPCS_PPCS_CBM_PG1,    0xffffffff, "LW_PGRAPH_PRI_GPCS_PPCS_CBM_PG1",    0x777777);
    WriteReadCheckVal( LW_PGRAPH_PRI_GPCS_TPCS_MPC_PG1,    0xffffffff, "LW_PGRAPH_PRI_GPCS_TPCS_MPC_PG1",    0x889999);
    WriteReadCheckVal( LW_PGRAPH_PRI_GPCS_TPCS_SM_BLPG_0_PG1,     0xffffffff, "LW_PGRAPH_PRI_GPCS_TPCS_SM_BLPG_0_PG1",     0x994444);
    WriteReadCheckVal( LW_PGRAPH_PRI_GPCS_TPCS_SM_BLPG_1_PG1,     0xffffffff, "LW_PGRAPH_PRI_GPCS_TPCS_SM_BLPG_1_PG1",     0x994444);
    WriteReadCheckVal( LW_PGRAPH_PRI_GPCS_TPCS_SM_BLPG_2_PG1,     0xffffffff, "LW_PGRAPH_PRI_GPCS_TPCS_SM_BLPG_2_PG1",     0x994444);
    WriteReadCheckVal( LW_PGRAPH_PRI_GPCS_TPCS_SM_BLPG_3_PG1,     0xffffffff, "LW_PGRAPH_PRI_GPCS_TPCS_SM_BLPG_3_PG1",     0x994444);
    WriteReadCheckVal( LW_PGRAPH_PRI_GPCS_TPCS_SM_BLPG_4_PG1,     0xffffffff, "LW_PGRAPH_PRI_GPCS_TPCS_SM_BLPG_4_PG1",     0x994444);
    WriteReadCheckVal( LW_PGRAPH_PRI_GPCS_TPCS_SM_BLPG_5_PG1,     0xffffffff, "LW_PGRAPH_PRI_GPCS_TPCS_SM_BLPG_5_PG1",     0x994444);

    //----------------------------------------------------------------------------------
    // Stage 3 : Check that broadcasting works : not a comprehensive check
    //----------------------------------------------------------------------------------
    ReadCheckVal( TPC_REG_ADDR(TEX_IN_PG, 0, 1),   0xffffffff, "LW_PGRAPH_PRI_GPC0_TPC1_TEX_IN_PG ", 0x240200   );
    ReadCheckVal( TPC_REG_ADDR(TEX_X_PG, 1, 1),   0xffffffff, "LW_PGRAPH_PRI_GPC1_TPC1_TEX_X_PG  ", 0x240200 );
    ReadCheckVal( LW_PGRAPH_PRI_GPC0_TPC0_TEX_D_PG,   0xffffffff, "LW_PGRAPH_PRI_GPC0_TPC0_TEX_D_PG  ", 0x200      );
    ReadCheckVal( TPC_REG_ADDR(TEX_T_PG, 1, 0),   0xffffffff, "LW_PGRAPH_PRI_GPC1_TPC0_TEX_T_PG  ", 0x40200    );
    ReadCheckVal( TPC_REG_ADDR(PE_PG, 0, 1),   0xffffffff, "LW_PGRAPH_PRI_GPC0_TPC1_PE_PG     ", 0x240200   );
    ReadCheckVal( TPC_REG_ADDR(L1C_PG, 1, 1),   0xffffffff, "LW_PGRAPH_PRI_GPC1_TPC1_L1C_PG    ", 0x240200 );
    ReadCheckVal( LW_PGRAPH_PRI_GPC0_PPC0_WWDX_PG,   0xffffffff, "LW_PGRAPH_PRI_GPC0_PPC0_WWDX_PG   ", 0x240200   );
    ReadCheckVal( PPC_REG_ADDR(CBM_PG, 1, 0),   0xffffffff, "LW_PGRAPH_PRI_GPC1_PPC0_CBM_PG    ", 0x200      );
    ReadCheckVal( TPC_REG_ADDR(MPC_PG, 0, 1),   0xffffffff, "LW_PGRAPH_PRI_GPC0_TPC1_MPC_PG    ", 0x40200    );
    ReadCheckVal( TPC_REG_ADDR(SM_BLPG_0_PG, 1, 1),   0xffffffff, "LW_PGRAPH_PRI_GPC1_TPC1_SM_BLPG_0_PG     ", 0x240200 );
    ReadCheckVal( TPC_REG_ADDR(SM_BLPG_1_PG, 1, 1),   0xffffffff, "LW_PGRAPH_PRI_GPC1_TPC1_SM_BLPG_1_PG     ", 0x240200 );
    ReadCheckVal( TPC_REG_ADDR(SM_BLPG_2_PG, 1, 1),   0xffffffff, "LW_PGRAPH_PRI_GPC1_TPC1_SM_BLPG_2_PG     ", 0x240200 );
    ReadCheckVal( TPC_REG_ADDR(SM_BLPG_3_PG, 1, 1),   0xffffffff, "LW_PGRAPH_PRI_GPC1_TPC1_SM_BLPG_3_PG     ", 0x240200 );

    ReadCheckVal( TPC_REG_ADDR(TEX_IN_PG1, 0, 1),  0xffffffff, "LW_PGRAPH_PRI_GPC0_TPC1_TEX_IN_PG1",  0xaabbaaaa);
    ReadCheckVal( TPC_REG_ADDR(TEX_X_PG1, 1, 1),  0xffffffff, "LW_PGRAPH_PRI_GPC1_TPC1_TEX_X_PG1 ",  0x12121212);
    ReadCheckVal( LW_PGRAPH_PRI_GPC0_TPC0_TEX_D_PG1 ,  0xffffffff, "LW_PGRAPH_PRI_GPC0_TPC0_TEX_D_PG1 ",  0x223332);
    ReadCheckVal( TPC_REG_ADDR(TEX_T_PG1, 1, 0),  0xffffffff, "LW_PGRAPH_PRI_GPC1_TPC0_TEX_T_PG1 ",  0x333fff);
    ReadCheckVal( TPC_REG_ADDR(PE_PG1, 0, 1),  0xffffffff, "LW_PGRAPH_PRI_GPC0_TPC1_PE_PG1    ",  0xffffff);
    ReadCheckVal( TPC_REG_ADDR(L1C_PG1, 1, 1),  0xffffffff, "LW_PGRAPH_PRI_GPC1_TPC1_L1C_PG1   ",  0xf0f0f0);
    ReadCheckVal( LW_PGRAPH_PRI_GPC0_PPC0_WWDX_PG1,  0xffffffff, "LW_PGRAPH_PRI_GPC0_PPC0_WWDX_PG1  ",  0x6a6a6a);
    ReadCheckVal( PPC_REG_ADDR(CBM_PG1, 1, 0),  0xffffffff, "LW_PGRAPH_PRI_GPC1_PPC0_CBM_PG1   ",  0x777777);
    ReadCheckVal( TPC_REG_ADDR(MPC_PG1, 0, 1),  0xffffffff, "LW_PGRAPH_PRI_GPC0_TPC1_MPC_PG1   ",  0x889999);
    ReadCheckVal( TPC_REG_ADDR(SM_BLPG_0_PG1, 1, 1),  0xffffffff, "LW_PGRAPH_PRI_GPC1_TPC1_SM_BLPG_0_PG1    ",  0x994444);
    ReadCheckVal( TPC_REG_ADDR(SM_BLPG_1_PG1, 1, 1),  0xffffffff, "LW_PGRAPH_PRI_GPC1_TPC1_SM_BLPG_1_PG1    ",  0x994444);
    ReadCheckVal( TPC_REG_ADDR(SM_BLPG_2_PG1, 1, 1),  0xffffffff, "LW_PGRAPH_PRI_GPC1_TPC1_SM_BLPG_2_PG1    ",  0x994444);
    ReadCheckVal( TPC_REG_ADDR(SM_BLPG_3_PG1, 1, 1),  0xffffffff, "LW_PGRAPH_PRI_GPC1_TPC1_SM_BLPG_3_PG1    ",  0x994444);
  }
  else{
    if(randomize_blg_regs ){

      UINT32 pg_val, pg1_val;
      const char*  elpg_enabled;
      m_rndStream = new RandomStream(seed_1, 0x222, 0x333);
      pg_val = Validate_slave_PG_Reg(m_rndStream->RandomUnsigned(0x0, 0xFFFFFFFF));
      pg1_val = Validate_PG1_Reg(m_rndStream->RandomUnsigned(0x0, 0xFFFFFFFF));
      pSubdev->RegWr32( LW_PGRAPH_PRI_BE0_CROP_FP16_PG    , pg_val);
      pSubdev->RegWr32( LW_PGRAPH_PRI_BE0_CROP_FP16_PG1   , pg1_val);
      elpg_enabled = ELPG_enabled(pg_val);
      InfoPrintf("PG reg val: LW_PGRAPH_PRI_BE0_CROP_FP16_PG, %08x, %s\n",pg_val, elpg_enabled);
      InfoPrintf("PG1 reg val: LW_PGRAPH_PRI_BE0_CROP_FP16_PG1, %08x\n",pg1_val);

      pg_val = Validate_slave_PG_Reg(m_rndStream->RandomUnsigned(0x0, 0xFFFFFFFF));
      pg1_val = Validate_PG1_Reg(m_rndStream->RandomUnsigned(0x0, 0xFFFFFFFF));
      pSubdev->RegWr32( LW_PGRAPH_PRI_BE0_CROP_PART2_PG    , pg_val);
      pSubdev->RegWr32( LW_PGRAPH_PRI_BE0_CROP_PART2_PG1   , pg1_val);
      elpg_enabled = ELPG_enabled(pg_val);
      InfoPrintf("PG reg val: LW_PGRAPH_PRI_BE0_CROP_PART2_PG, %08x, %s\n",pg_val, elpg_enabled);
      InfoPrintf("PG1 reg val: LW_PGRAPH_PRI_BE0_CROP_PART2_PG1, %08x\n",pg1_val);

      pg_val = Validate_slave_PG_Reg(m_rndStream->RandomUnsigned(0x0, 0xFFFFFFFF));
      pg1_val = Validate_PG1_Reg(m_rndStream->RandomUnsigned(0x0, 0xFFFFFFFF));
      pSubdev->RegWr32( LW_PGRAPH_PRI_BE0_CROP_PG    , pg_val);
      pSubdev->RegWr32( LW_PGRAPH_PRI_BE0_CROP_PG1   , pg1_val);
      elpg_enabled = ELPG_enabled(pg_val);
      InfoPrintf("PG reg val: LW_PGRAPH_PRI_BE0_CROP_PG, %08x, %s\n",pg_val, elpg_enabled);
      InfoPrintf("PG1 reg val: LW_PGRAPH_PRI_BE0_CROP_PG1, %08x\n",pg1_val);

      pg_val = Validate_slave_PG_Reg(m_rndStream->RandomUnsigned(0x0, 0xFFFFFFFF));
      pg1_val = Validate_PG1_Reg(m_rndStream->RandomUnsigned(0x0, 0xFFFFFFFF));
      pSubdev->RegWr32( LW_PGRAPH_PRI_BE0_CROP_TAGS_PG    , pg_val);
      pSubdev->RegWr32( LW_PGRAPH_PRI_BE0_CROP_TAGS_PG1   , pg1_val);
      elpg_enabled = ELPG_enabled(pg_val);
      InfoPrintf("PG reg val: LW_PGRAPH_PRI_BE0_CROP_TAGS_PG, %08x, %s\n",pg_val, elpg_enabled);
      InfoPrintf("PG1 reg val: LW_PGRAPH_PRI_BE0_CROP_TAGS_PG1, %08x\n",pg1_val);

      pg_val = Validate_PG_Reg(m_rndStream->RandomUnsigned(0x0, 0xFFFFFFFF));
      pg1_val = Validate_PG1_Reg(m_rndStream->RandomUnsigned(0x0, 0xFFFFFFFF));
      pSubdev->RegWr32( LW_PGRAPH_PRI_BE0_CROP_US816G_PG    , pg_val);
      pSubdev->RegWr32( LW_PGRAPH_PRI_BE0_CROP_US816G_PG1   , pg1_val);
      elpg_enabled = ELPG_enabled(pg_val);
      InfoPrintf("PG reg val: LW_PGRAPH_PRI_BE0_CROP_US816G_PG, %08x, %s\n",pg_val, elpg_enabled);
      InfoPrintf("PG1 reg val: LW_PGRAPH_PRI_BE0_CROP_US816G_PG1, %08x\n",pg1_val);

      pg_val = Validate_slave_PG_Reg(m_rndStream->RandomUnsigned(0x0, 0xFFFFFFFF));
      pg1_val = Validate_PG1_Reg(m_rndStream->RandomUnsigned(0x0, 0xFFFFFFFF));
      pSubdev->RegWr32( LW_PGRAPH_PRI_BE0_CROP_US816_PG    , pg_val);
      pSubdev->RegWr32( LW_PGRAPH_PRI_BE0_CROP_US816_PG1   , pg1_val);
      elpg_enabled = ELPG_enabled(pg_val);
      InfoPrintf("PG reg val: LW_PGRAPH_PRI_BE0_CROP_US816_PG, %08x, %s\n",pg_val, elpg_enabled);
      InfoPrintf("PG1 reg val: LW_PGRAPH_PRI_BE0_CROP_US816_PG1, %08x\n",pg1_val);

      pg_val = Validate_PG_Reg(m_rndStream->RandomUnsigned(0x0, 0xFFFFFFFF));
      pg1_val = Validate_PG1_Reg(m_rndStream->RandomUnsigned(0x0, 0xFFFFFFFF));
      pSubdev->RegWr32( LW_PGRAPH_PRI_BE0_RDM_PG    , pg_val);
      pSubdev->RegWr32( LW_PGRAPH_PRI_BE0_RDM_PG1   , pg1_val);
      elpg_enabled = ELPG_enabled(pg_val);
      InfoPrintf("PG reg val: LW_PGRAPH_PRI_BE0_RDM_PG, %08x, %s\n",pg_val, elpg_enabled);
      InfoPrintf("PG1 reg val: LW_PGRAPH_PRI_BE0_RDM_PG1, %08x\n",pg1_val);

      pg_val = Validate_slave_PG_Reg(m_rndStream->RandomUnsigned(0x0, 0xFFFFFFFF));
      pg1_val = Validate_PG1_Reg(m_rndStream->RandomUnsigned(0x0, 0xFFFFFFFF));
      pSubdev->RegWr32( LW_PGRAPH_PRI_BE0_RDM_RPM_C_PG    , pg_val);
      pSubdev->RegWr32( LW_PGRAPH_PRI_BE0_RDM_RPM_C_PG1   , pg1_val);
      elpg_enabled = ELPG_enabled(pg_val);
      InfoPrintf("PG reg val: LW_PGRAPH_PRI_BE0_RDM_RPM_C_PG, %08x, %s\n",pg_val, elpg_enabled);
      InfoPrintf("PG1 reg val: LW_PGRAPH_PRI_BE0_RDM_RPM_C_PG1, %08x\n",pg1_val);

      pg_val = Validate_slave_PG_Reg(m_rndStream->RandomUnsigned(0x0, 0xFFFFFFFF));
      pg1_val = Validate_PG1_Reg(m_rndStream->RandomUnsigned(0x0, 0xFFFFFFFF));
      pSubdev->RegWr32( LW_PGRAPH_PRI_BE0_RDM_RPM_Z_PG    , pg_val);
      pSubdev->RegWr32( LW_PGRAPH_PRI_BE0_RDM_RPM_Z_PG1   , pg1_val);
      elpg_enabled = ELPG_enabled(pg_val);
      InfoPrintf("PG reg val: LW_PGRAPH_PRI_BE0_RDM_RPM_Z_PG, %08x, %s\n",pg_val, elpg_enabled);
      InfoPrintf("PG1 reg val: LW_PGRAPH_PRI_BE0_RDM_RPM_Z_PG1, %08x\n",pg1_val);

      pg_val = Validate_slave_PG_Reg(m_rndStream->RandomUnsigned(0x0, 0xFFFFFFFF));
      pg1_val = Validate_PG1_Reg(m_rndStream->RandomUnsigned(0x0, 0xFFFFFFFF));
      pSubdev->RegWr32( LW_PGRAPH_PRI_BE0_ZROP_EXPAND_PG    , pg_val);
      pSubdev->RegWr32( LW_PGRAPH_PRI_BE0_ZROP_EXPAND_PG1   , pg1_val);
      elpg_enabled = ELPG_enabled(pg_val);
      InfoPrintf("PG reg val: LW_PGRAPH_PRI_BE0_ZROP_EXPAND_PG, %08x, %s\n",pg_val, elpg_enabled);
      InfoPrintf("PG1 reg val: LW_PGRAPH_PRI_BE0_ZROP_EXPAND_PG1, %08x\n",pg1_val);

      pg_val = Validate_slave_PG_Reg(m_rndStream->RandomUnsigned(0x0, 0xFFFFFFFF));
      pg1_val = Validate_PG1_Reg(m_rndStream->RandomUnsigned(0x0, 0xFFFFFFFF));
      pSubdev->RegWr32( LW_PGRAPH_PRI_BE0_ZROP_PG    , pg_val);
      pSubdev->RegWr32( LW_PGRAPH_PRI_BE0_ZROP_PG1   , pg1_val);
      elpg_enabled = ELPG_enabled(pg_val);
      InfoPrintf("PG reg val: LW_PGRAPH_PRI_BE0_ZROP_PG, %08x, %s\n",pg_val, elpg_enabled);
      InfoPrintf("PG1 reg val: LW_PGRAPH_PRI_BE0_ZROP_PG1, %08x\n",pg1_val);

      pg_val = Validate_slave_PG_Reg(m_rndStream->RandomUnsigned(0x0, 0xFFFFFFFF));
      pg1_val = Validate_PG1_Reg(m_rndStream->RandomUnsigned(0x0, 0xFFFFFFFF));
      pSubdev->RegWr32( LW_PGRAPH_PRI_DS_PG    , pg_val);
      pSubdev->RegWr32( LW_PGRAPH_PRI_DS_PG1   , pg1_val);
      elpg_enabled = ELPG_enabled(pg_val);
      InfoPrintf("PG reg val: LW_PGRAPH_PRI_DS_PG, %08x, %s\n",pg_val, elpg_enabled);
      InfoPrintf("PG1 reg val: LW_PGRAPH_PRI_DS_PG1, %08x\n",pg1_val);

      pg_val = Validate_PG_Reg(m_rndStream->RandomUnsigned(0x0, 0xFFFFFFFF));
      pg1_val = Validate_PG1_Reg(m_rndStream->RandomUnsigned(0x0, 0xFFFFFFFF));
      pSubdev->RegWr32( LW_PGRAPH_PRI_FE_PG    , pg_val);
      pSubdev->RegWr32( LW_PGRAPH_PRI_FE_PG1   , pg1_val);
      elpg_enabled = ELPG_enabled(pg_val);
      InfoPrintf("PG reg val: LW_PGRAPH_PRI_FE_PG, %08x, %s\n",pg_val, elpg_enabled);
      InfoPrintf("PG1 reg val: LW_PGRAPH_PRI_FE_PG1, %08x\n",pg1_val);

      pg_val = Validate_slave_PG_Reg(m_rndStream->RandomUnsigned(0x0, 0xFFFFFFFF));
      pg1_val = Validate_PG1_Reg(m_rndStream->RandomUnsigned(0x0, 0xFFFFFFFF));
      pSubdev->RegWr32( LW_PGRAPH_PRI_GPC0_CRSTR_PG    , pg_val);
      pSubdev->RegWr32( LW_PGRAPH_PRI_GPC0_CRSTR_PG1   , pg1_val);
      elpg_enabled = ELPG_enabled(pg_val);
      InfoPrintf("PG reg val: LW_PGRAPH_PRI_GPC0_CRSTR_PG, %08x, %s\n",pg_val, elpg_enabled);
      InfoPrintf("PG1 reg val: LW_PGRAPH_PRI_GPC0_CRSTR_PG1, %08x\n",pg1_val);

      pg_val = Validate_slave_PG_Reg(m_rndStream->RandomUnsigned(0x0, 0xFFFFFFFF));
      pg1_val = Validate_PG1_Reg(m_rndStream->RandomUnsigned(0x0, 0xFFFFFFFF));
      pSubdev->RegWr32( LW_PGRAPH_PRI_GPC0_FRSTR_PG    , pg_val);
      pSubdev->RegWr32( LW_PGRAPH_PRI_GPC0_FRSTR_PG1   , pg1_val);
      elpg_enabled = ELPG_enabled(pg_val);
      InfoPrintf("PG reg val: LW_PGRAPH_PRI_GPC0_FRSTR_PG, %08x, %s\n",pg_val, elpg_enabled);
      InfoPrintf("PG1 reg val: LW_PGRAPH_PRI_GPC0_FRSTR_PG1, %08x\n",pg1_val);

      pg_val = Validate_PG_Reg(m_rndStream->RandomUnsigned(0x0, 0xFFFFFFFF));
      pg1_val = Validate_PG1_Reg(m_rndStream->RandomUnsigned(0x0, 0xFFFFFFFF));
      pSubdev->RegWr32( LW_PGRAPH_PRI_GPC0_GCC_PG    , pg_val);
      pSubdev->RegWr32( LW_PGRAPH_PRI_GPC0_GCC_PG1   , pg1_val);
      elpg_enabled = ELPG_enabled(pg_val);
      InfoPrintf("PG reg val: LW_PGRAPH_PRI_GPC0_GCC_PG, %08x, %s\n",pg_val, elpg_enabled);
      InfoPrintf("PG1 reg val: LW_PGRAPH_PRI_GPC0_GCC_PG1, %08x\n",pg1_val);

      pg_val = Validate_PG_Reg(m_rndStream->RandomUnsigned(0x0, 0xFFFFFFFF));
      pg1_val = Validate_PG1_Reg(m_rndStream->RandomUnsigned(0x0, 0xFFFFFFFF));
      pSubdev->RegWr32( LW_PGRAPH_PRI_GPC0_GPM_PD_PG    , pg_val);
      pSubdev->RegWr32( LW_PGRAPH_PRI_GPC0_GPM_PD_PG1   , pg1_val);
      elpg_enabled = ELPG_enabled(pg_val);
      InfoPrintf("PG reg val: LW_PGRAPH_PRI_GPC0_GPM_PD_PG, %08x, %s\n",pg_val, elpg_enabled);
      InfoPrintf("PG1 reg val: LW_PGRAPH_PRI_GPC0_GPM_PD_PG1, %08x\n",pg1_val);

      pg_val = Validate_slave_PG_Reg(m_rndStream->RandomUnsigned(0x0, 0xFFFFFFFF));
      pg1_val = Validate_PG1_Reg(m_rndStream->RandomUnsigned(0x0, 0xFFFFFFFF));
      pSubdev->RegWr32( LW_PGRAPH_PRI_GPC0_GPM_RPT_PG    , pg_val);
      pSubdev->RegWr32( LW_PGRAPH_PRI_GPC0_GPM_RPT_PG1   , pg1_val);
      elpg_enabled = ELPG_enabled(pg_val);
      InfoPrintf("PG reg val: LW_PGRAPH_PRI_GPC0_GPM_RPT_PG, %08x, %s\n",pg_val, elpg_enabled);
      InfoPrintf("PG1 reg val: LW_PGRAPH_PRI_GPC0_GPM_RPT_PG1, %08x\n",pg1_val);

      pg_val = Validate_slave_PG_Reg(m_rndStream->RandomUnsigned(0x0, 0xFFFFFFFF));
      pg1_val = Validate_PG1_Reg(m_rndStream->RandomUnsigned(0x0, 0xFFFFFFFF));
      pSubdev->RegWr32( LW_PGRAPH_PRI_GPC0_GPM_SD_PG    , pg_val);
      pSubdev->RegWr32( LW_PGRAPH_PRI_GPC0_GPM_SD_PG1   , pg1_val);
      elpg_enabled = ELPG_enabled(pg_val);
      InfoPrintf("PG reg val: LW_PGRAPH_PRI_GPC0_GPM_SD_PG, %08x, %s\n",pg_val, elpg_enabled);
      InfoPrintf("PG1 reg val: LW_PGRAPH_PRI_GPC0_GPM_SD_PG1, %08x\n",pg1_val);

      pg_val = Validate_slave_PG_Reg(m_rndStream->RandomUnsigned(0x0, 0xFFFFFFFF));
      pg1_val = Validate_PG1_Reg(m_rndStream->RandomUnsigned(0x0, 0xFFFFFFFF));
      pSubdev->RegWr32( LW_PGRAPH_PRI_GPC0_MMU_PG    , pg_val);
      pSubdev->RegWr32( LW_PGRAPH_PRI_GPC0_MMU_PG1   , pg1_val);
      elpg_enabled = ELPG_enabled(pg_val);
      InfoPrintf("PG reg val: LW_PGRAPH_PRI_GPC0_MMU_PG, %08x, %s\n",pg_val, elpg_enabled);
      InfoPrintf("PG1 reg val: LW_PGRAPH_PRI_GPC0_MMU_PG1, %08x\n",pg1_val);

      pg_val = Validate_slave_PG_Reg(m_rndStream->RandomUnsigned(0x0, 0xFFFFFFFF));
      pg1_val = Validate_PG1_Reg(m_rndStream->RandomUnsigned(0x0, 0xFFFFFFFF));
      pSubdev->RegWr32( LW_PGRAPH_PRI_GPC0_PROP_PG    , pg_val);
      pSubdev->RegWr32( LW_PGRAPH_PRI_GPC0_PROP_PG1   , pg1_val);
      elpg_enabled = ELPG_enabled(pg_val);
      InfoPrintf("PG reg val: LW_PGRAPH_PRI_GPC0_PROP_PG, %08x, %s\n",pg_val, elpg_enabled);
      InfoPrintf("PG1 reg val: LW_PGRAPH_PRI_GPC0_PROP_PG1, %08x\n",pg1_val);

      pg_val = Validate_PG_Reg(m_rndStream->RandomUnsigned(0x0, 0xFFFFFFFF));
      pg1_val = Validate_PG1_Reg(m_rndStream->RandomUnsigned(0x0, 0xFFFFFFFF));
      pSubdev->RegWr32( LW_PGRAPH_PRI_GPC0_SETUP_PG    , pg_val);
      pSubdev->RegWr32( LW_PGRAPH_PRI_GPC0_SETUP_PG1   , pg1_val);
      elpg_enabled = ELPG_enabled(pg_val);
      InfoPrintf("PG reg val: LW_PGRAPH_PRI_GPC0_SETUP_PG, %08x, %s\n",pg_val, elpg_enabled);
      InfoPrintf("PG1 reg val: LW_PGRAPH_PRI_GPC0_SETUP_PG1, %08x\n",pg1_val);

      pg_val = Validate_slave_PG_Reg(m_rndStream->RandomUnsigned(0x0, 0xFFFFFFFF));
      pg1_val = Validate_PG1_Reg(m_rndStream->RandomUnsigned(0x0, 0xFFFFFFFF));
      pSubdev->RegWr32( LW_PGRAPH_PRI_GPC0_SWDX_PG    , pg_val);
      pSubdev->RegWr32( LW_PGRAPH_PRI_GPC0_SWDX_PG1   , pg1_val);
      elpg_enabled = ELPG_enabled(pg_val);
      InfoPrintf("PG reg val: LW_PGRAPH_PRI_GPC0_SWDX_PG, %08x, %s\n",pg_val, elpg_enabled);
      InfoPrintf("PG1 reg val: LW_PGRAPH_PRI_GPC0_SWDX_PG1, %08x\n",pg1_val);

      pg_val = Validate_slave_PG_Reg(m_rndStream->RandomUnsigned(0x0, 0xFFFFFFFF));
      pg1_val = Validate_PG1_Reg(m_rndStream->RandomUnsigned(0x0, 0xFFFFFFFF));
      pSubdev->RegWr32( LW_PGRAPH_PRI_GPC0_TC_PG    , pg_val);
      pSubdev->RegWr32( LW_PGRAPH_PRI_GPC0_TC_PG1   , pg1_val);
      elpg_enabled = ELPG_enabled(pg_val);
      InfoPrintf("PG reg val: LW_PGRAPH_PRI_GPC0_TC_PG, %08x, %s\n",pg_val, elpg_enabled);
      InfoPrintf("PG1 reg val: LW_PGRAPH_PRI_GPC0_TC_PG1, %08x\n",pg1_val);

      pg_val = Validate_slave_PG_Reg(m_rndStream->RandomUnsigned(0x0, 0xFFFFFFFF));
      pg1_val = Validate_PG1_Reg(m_rndStream->RandomUnsigned(0x0, 0xFFFFFFFF));
      pSubdev->RegWr32( LW_PGRAPH_PRI_GPC0_WDXPS_PG    , pg_val);
      pSubdev->RegWr32( LW_PGRAPH_PRI_GPC0_WDXPS_PG1   , pg1_val);
      elpg_enabled = ELPG_enabled(pg_val);
      InfoPrintf("PG reg val: LW_PGRAPH_PRI_GPC0_WDXPS_PG, %08x, %s\n",pg_val, elpg_enabled);
      InfoPrintf("PG1 reg val: LW_PGRAPH_PRI_GPC0_WDXPS_PG1, %08x\n",pg1_val);

      pg_val = Validate_slave_PG_Reg(m_rndStream->RandomUnsigned(0x0, 0xFFFFFFFF));
      pg1_val = Validate_PG1_Reg(m_rndStream->RandomUnsigned(0x0, 0xFFFFFFFF));
      pSubdev->RegWr32( LW_PGRAPH_PRI_GPC0_WIDCLIP_PG    , pg_val);
      pSubdev->RegWr32( LW_PGRAPH_PRI_GPC0_WIDCLIP_PG1   , pg1_val);
      elpg_enabled = ELPG_enabled(pg_val);
      InfoPrintf("PG reg val: LW_PGRAPH_PRI_GPC0_WIDCLIP_PG, %08x, %s\n",pg_val, elpg_enabled);
      InfoPrintf("PG1 reg val: LW_PGRAPH_PRI_GPC0_WIDCLIP_PG1, %08x\n",pg1_val);

      pg_val = Validate_PG_Reg(m_rndStream->RandomUnsigned(0x0, 0xFFFFFFFF));
      pg1_val = Validate_PG1_Reg(m_rndStream->RandomUnsigned(0x0, 0xFFFFFFFF));
      pSubdev->RegWr32( LW_PGRAPH_PRI_GPC0_ZLWLL_PG    , pg_val);
      pSubdev->RegWr32( LW_PGRAPH_PRI_GPC0_ZLWLL_PG1   , pg1_val);
      elpg_enabled = ELPG_enabled(pg_val);
      InfoPrintf("PG reg val: LW_PGRAPH_PRI_GPC0_ZLWLL_PG, %08x, %s\n",pg_val, elpg_enabled);
      InfoPrintf("PG1 reg val: LW_PGRAPH_PRI_GPC0_ZLWLL_PG1, %08x\n",pg1_val);

      pg_val = Validate_slave_PG_Reg(m_rndStream->RandomUnsigned(0x0, 0xFFFFFFFF));
      pg1_val = Validate_PG1_Reg(m_rndStream->RandomUnsigned(0x0, 0xFFFFFFFF));
      pSubdev->RegWr32( LW_PGRAPH_PRI_GPCS_PPCS_CBM_PG    , pg_val);
      pSubdev->RegWr32( LW_PGRAPH_PRI_GPCS_PPCS_CBM_PG1   , pg1_val);
      elpg_enabled = ELPG_enabled(pg_val);
      InfoPrintf("PG reg val: LW_PGRAPH_PRI_GPCS_PPCS_CBM_PG, %08x, %s\n",pg_val, elpg_enabled);
      InfoPrintf("PG1 reg val: LW_PGRAPH_PRI_GPCS_PPCS_CBM_PG1, %08x\n",pg1_val);

      pg_val = Validate_PG_Reg(m_rndStream->RandomUnsigned(0x0, 0xFFFFFFFF));
      pg1_val = Validate_PG1_Reg(m_rndStream->RandomUnsigned(0x0, 0xFFFFFFFF));
      pSubdev->RegWr32( LW_PGRAPH_PRI_GPCS_TPCS_L1C_PG    , pg_val);
      pSubdev->RegWr32( LW_PGRAPH_PRI_GPCS_TPCS_L1C_PG1   , pg1_val);
      elpg_enabled = ELPG_enabled(pg_val);
      InfoPrintf("PG reg val: LW_PGRAPH_PRI_GPCS_TPCS_L1C_PG, %08x, %s\n",pg_val, elpg_enabled);
      InfoPrintf("PG1 reg val: LW_PGRAPH_PRI_GPCS_TPCS_L1C_PG1, %08x\n",pg1_val);

      pg_val = Validate_slave_PG_Reg(m_rndStream->RandomUnsigned(0x0, 0xFFFFFFFF));
      pg1_val = Validate_PG1_Reg(m_rndStream->RandomUnsigned(0x0, 0xFFFFFFFF));
      pSubdev->RegWr32( LW_PGRAPH_PRI_GPCS_TPCS_MPC_PG    , pg_val);
      pSubdev->RegWr32( LW_PGRAPH_PRI_GPCS_TPCS_MPC_PG1   , pg1_val);
      elpg_enabled = ELPG_enabled(pg_val);
      InfoPrintf("PG reg val: LW_PGRAPH_PRI_GPCS_TPCS_MPC_PG, %08x, %s\n",pg_val, elpg_enabled);
      InfoPrintf("PG1 reg val: LW_PGRAPH_PRI_GPCS_TPCS_MPC_PG1, %08x\n",pg1_val);

      pg_val = Validate_slave_PG_Reg(m_rndStream->RandomUnsigned(0x0, 0xFFFFFFFF));
      pg1_val = Validate_PG1_Reg(m_rndStream->RandomUnsigned(0x0, 0xFFFFFFFF));
      pSubdev->RegWr32( LW_PGRAPH_PRI_GPCS_TPCS_PE_PG    , pg_val);
      pSubdev->RegWr32( LW_PGRAPH_PRI_GPCS_TPCS_PE_PG1   , pg1_val);
      elpg_enabled = ELPG_enabled(pg_val);
      InfoPrintf("PG reg val: LW_PGRAPH_PRI_GPCS_TPCS_PE_PG, %08x, %s\n",pg_val, elpg_enabled);
      InfoPrintf("PG1 reg val: LW_PGRAPH_PRI_GPCS_TPCS_PE_PG1, %08x\n",pg1_val);

      pg_val = Validate_PG_Reg(m_rndStream->RandomUnsigned(0x0, 0xFFFFFFFF));
      pg1_val = Validate_PG1_Reg(m_rndStream->RandomUnsigned(0x0, 0xFFFFFFFF));
      pSubdev->RegWr32( LW_PGRAPH_PRI_GPCS_TPCS_SM_BLPG_0_PG    , pg_val);
      pSubdev->RegWr32( LW_PGRAPH_PRI_GPCS_TPCS_SM_BLPG_0_PG1   , pg1_val);
      elpg_enabled = ELPG_enabled(pg_val);
      InfoPrintf("PG reg val: LW_PGRAPH_PRI_GPCS_TPCS_SM_BLPG_0_PG, %08x, %s\n",pg_val, elpg_enabled);
      InfoPrintf("PG1 reg val: LW_PGRAPH_PRI_GPCS_TPCS_SM_BLPG_0_PG1, %08x\n",pg1_val);

      pg_val = Validate_PG_Reg(m_rndStream->RandomUnsigned(0x0, 0xFFFFFFFF));
      pg1_val = Validate_PG1_Reg(m_rndStream->RandomUnsigned(0x0, 0xFFFFFFFF));
      pSubdev->RegWr32( LW_PGRAPH_PRI_GPCS_TPCS_SM_BLPG_1_PG    , pg_val);
      pSubdev->RegWr32( LW_PGRAPH_PRI_GPCS_TPCS_SM_BLPG_1_PG1   , pg1_val);
      elpg_enabled = ELPG_enabled(pg_val);
      InfoPrintf("PG reg val: LW_PGRAPH_PRI_GPCS_TPCS_SM_BLPG_1_PG, %08x, %s\n",pg_val, elpg_enabled);
      InfoPrintf("PG1 reg val: LW_PGRAPH_PRI_GPCS_TPCS_SM_BLPG_1_PG1, %08x\n",pg1_val);

      pg_val = Validate_PG_Reg(m_rndStream->RandomUnsigned(0x0, 0xFFFFFFFF));
      pg1_val = Validate_PG1_Reg(m_rndStream->RandomUnsigned(0x0, 0xFFFFFFFF));
      pSubdev->RegWr32( LW_PGRAPH_PRI_GPCS_TPCS_SM_BLPG_2_PG    , pg_val);
      pSubdev->RegWr32( LW_PGRAPH_PRI_GPCS_TPCS_SM_BLPG_2_PG1   , pg1_val);
      elpg_enabled = ELPG_enabled(pg_val);
      InfoPrintf("PG reg val: LW_PGRAPH_PRI_GPCS_TPCS_SM_BLPG_2_PG, %08x, %s\n",pg_val, elpg_enabled);
      InfoPrintf("PG1 reg val: LW_PGRAPH_PRI_GPCS_TPCS_SM_BLPG_2_PG1, %08x\n",pg1_val);

      pg_val = Validate_PG_Reg(m_rndStream->RandomUnsigned(0x0, 0xFFFFFFFF));
      pg1_val = Validate_PG1_Reg(m_rndStream->RandomUnsigned(0x0, 0xFFFFFFFF));
      pSubdev->RegWr32( LW_PGRAPH_PRI_GPCS_TPCS_SM_BLPG_3_PG    , pg_val);
      pSubdev->RegWr32( LW_PGRAPH_PRI_GPCS_TPCS_SM_BLPG_3_PG1   , pg1_val);
      elpg_enabled = ELPG_enabled(pg_val);
      InfoPrintf("PG reg val: LW_PGRAPH_PRI_GPCS_TPCS_SM_BLPG_3_PG, %08x, %s\n",pg_val, elpg_enabled);
      InfoPrintf("PG1 reg val: LW_PGRAPH_PRI_GPCS_TPCS_SM_BLPG_3_PG1, %08x\n",pg1_val);

      pg_val = Validate_PG_Reg(m_rndStream->RandomUnsigned(0x0, 0xFFFFFFFF));
      pg1_val = Validate_PG1_Reg(m_rndStream->RandomUnsigned(0x0, 0xFFFFFFFF));
      pSubdev->RegWr32( LW_PGRAPH_PRI_GPCS_TPCS_SM_BLPG_4_PG    , pg_val);
      pSubdev->RegWr32( LW_PGRAPH_PRI_GPCS_TPCS_SM_BLPG_4_PG1   , pg1_val);
      elpg_enabled = ELPG_enabled(pg_val);
      InfoPrintf("PG reg val: LW_PGRAPH_PRI_GPCS_TPCS_SM_BLPG_4_PG, %08x, %s\n",pg_val, elpg_enabled);
      InfoPrintf("PG1 reg val: LW_PGRAPH_PRI_GPCS_TPCS_SM_BLPG_4_PG1, %08x\n",pg1_val);

      pg_val = Validate_PG_Reg(m_rndStream->RandomUnsigned(0x0, 0xFFFFFFFF));
      pg1_val = Validate_PG1_Reg(m_rndStream->RandomUnsigned(0x0, 0xFFFFFFFF));
      pSubdev->RegWr32( LW_PGRAPH_PRI_GPCS_TPCS_SM_BLPG_5_PG    , pg_val);
      pSubdev->RegWr32( LW_PGRAPH_PRI_GPCS_TPCS_SM_BLPG_5_PG1   , pg1_val);
      elpg_enabled = ELPG_enabled(pg_val);
      InfoPrintf("PG reg val: LW_PGRAPH_PRI_GPCS_TPCS_SM_BLPG_5_PG, %08x, %s\n",pg_val, elpg_enabled);
      InfoPrintf("PG1 reg val: LW_PGRAPH_PRI_GPCS_TPCS_SM_BLPG_5_PG1, %08x\n",pg1_val);

      pg_val = Validate_PG_Reg(m_rndStream->RandomUnsigned(0x0, 0xFFFFFFFF));
      pg1_val = Validate_PG1_Reg(m_rndStream->RandomUnsigned(0x0, 0xFFFFFFFF));
      pSubdev->RegWr32( LW_PGRAPH_PRI_GPCS_TPCS_TEX_D_PG    , pg_val);
      pSubdev->RegWr32( LW_PGRAPH_PRI_GPCS_TPCS_TEX_D_PG1   , pg1_val);
      elpg_enabled = ELPG_enabled(pg_val);
      InfoPrintf("PG reg val: LW_PGRAPH_PRI_GPCS_TPCS_TEX_D_PG, %08x, %s\n",pg_val, elpg_enabled);
      InfoPrintf("PG1 reg val: LW_PGRAPH_PRI_GPCS_TPCS_TEX_D_PG1, %08x\n",pg1_val);

      pg_val = Validate_PG_Reg(m_rndStream->RandomUnsigned(0x0, 0xFFFFFFFF));
      pg1_val = Validate_PG1_Reg(m_rndStream->RandomUnsigned(0x0, 0xFFFFFFFF));
      pSubdev->RegWr32( LW_PGRAPH_PRI_GPCS_TPCS_TEX_IN_PG    , pg_val);
      pSubdev->RegWr32( LW_PGRAPH_PRI_GPCS_TPCS_TEX_IN_PG1   , pg1_val);
      elpg_enabled = ELPG_enabled(pg_val);
      InfoPrintf("PG reg val: LW_PGRAPH_PRI_GPCS_TPCS_TEX_IN_PG, %08x, %s\n",pg_val, elpg_enabled);
      InfoPrintf("PG1 reg val: LW_PGRAPH_PRI_GPCS_TPCS_TEX_IN_PG1, %08x\n",pg1_val);

      pg_val = Validate_slave_PG_Reg(m_rndStream->RandomUnsigned(0x0, 0xFFFFFFFF));
      pg1_val = Validate_PG1_Reg(m_rndStream->RandomUnsigned(0x0, 0xFFFFFFFF));
      pSubdev->RegWr32( LW_PGRAPH_PRI_GPCS_TPCS_TEX_TRM_PG    , pg_val);
      pSubdev->RegWr32( LW_PGRAPH_PRI_GPCS_TPCS_TEX_TRM_PG1   , pg1_val);
      elpg_enabled = ELPG_enabled(pg_val);
      InfoPrintf("PG reg val: LW_PGRAPH_PRI_GPCS_TPCS_TEX_TRM_PG, %08x, %s\n",pg_val, elpg_enabled);
      InfoPrintf("PG1 reg val: LW_PGRAPH_PRI_GPCS_TPCS_TEX_TRM_PG1, %08x\n",pg1_val);

      pg_val = Validate_slave_PG_Reg(m_rndStream->RandomUnsigned(0x0, 0xFFFFFFFF));
      pg1_val = Validate_PG1_Reg(m_rndStream->RandomUnsigned(0x0, 0xFFFFFFFF));
      pSubdev->RegWr32( LW_PGRAPH_PRI_GPCS_TPCS_TEX_T_PG    , pg_val);
      pSubdev->RegWr32( LW_PGRAPH_PRI_GPCS_TPCS_TEX_T_PG1   , pg1_val);
      elpg_enabled = ELPG_enabled(pg_val);
      InfoPrintf("PG reg val: LW_PGRAPH_PRI_GPCS_TPCS_TEX_T_PG, %08x, %s\n",pg_val, elpg_enabled);
      InfoPrintf("PG1 reg val: LW_PGRAPH_PRI_GPCS_TPCS_TEX_T_PG1, %08x\n",pg1_val);

      pg_val = Validate_slave_PG_Reg(m_rndStream->RandomUnsigned(0x0, 0xFFFFFFFF));
      pg1_val = Validate_PG1_Reg(m_rndStream->RandomUnsigned(0x0, 0xFFFFFFFF));
      pSubdev->RegWr32( LW_PGRAPH_PRI_GPCS_TPCS_TEX_X_PG    , pg_val);
      pSubdev->RegWr32( LW_PGRAPH_PRI_GPCS_TPCS_TEX_X_PG1   , pg1_val);
      elpg_enabled = ELPG_enabled(pg_val);
      InfoPrintf("PG reg val: LW_PGRAPH_PRI_GPCS_TPCS_TEX_X_PG, %08x, %s\n",pg_val, elpg_enabled);
      InfoPrintf("PG1 reg val: LW_PGRAPH_PRI_GPCS_TPCS_TEX_X_PG1, %08x\n",pg1_val);

      pg_val = Validate_PG_Reg(m_rndStream->RandomUnsigned(0x0, 0xFFFFFFFF));
      pg1_val = Validate_PG1_Reg(m_rndStream->RandomUnsigned(0x0, 0xFFFFFFFF));
      pSubdev->RegWr32( LW_PGRAPH_PRI_GPCS_PPCS_WWDX_PG    , pg_val);
      pSubdev->RegWr32( LW_PGRAPH_PRI_GPCS_PPCS_WWDX_PG1   , pg1_val);
      elpg_enabled = ELPG_enabled(pg_val);
      InfoPrintf("PG reg val: LW_PGRAPH_PRI_GPCS_PPCS_WWDX_PG, %08x, %s\n",pg_val, elpg_enabled);
      InfoPrintf("PG1 reg val: LW_PGRAPH_PRI_GPCS_PPCS_WWDX_PG1, %08x\n",pg1_val);

      pg_val = Validate_slave_PG_Reg(m_rndStream->RandomUnsigned(0x0, 0xFFFFFFFF));
      pg1_val = Validate_PG1_Reg(m_rndStream->RandomUnsigned(0x0, 0xFFFFFFFF));
      pSubdev->RegWr32( LW_PGRAPH_PRI_PDB_PG    , pg_val);
      pSubdev->RegWr32( LW_PGRAPH_PRI_PDB_PG1   , pg1_val);
      elpg_enabled = ELPG_enabled(pg_val);
      InfoPrintf("PG reg val: LW_PGRAPH_PRI_PDB_PG, %08x, %s\n",pg_val, elpg_enabled);
      InfoPrintf("PG1 reg val: LW_PGRAPH_PRI_PDB_PG1, %08x\n",pg1_val);

      pg_val = Validate_PG_Reg(m_rndStream->RandomUnsigned(0x0, 0xFFFFFFFF));
      pg1_val = Validate_PG1_Reg(m_rndStream->RandomUnsigned(0x0, 0xFFFFFFFF));
      pSubdev->RegWr32( LW_PGRAPH_PRI_PD_PG    , pg_val);
      pSubdev->RegWr32( LW_PGRAPH_PRI_PD_PG1   , pg1_val);
      elpg_enabled = ELPG_enabled(pg_val);
      InfoPrintf("PG reg val: LW_PGRAPH_PRI_PD_PG, %08x, %s\n",pg_val, elpg_enabled);
      InfoPrintf("PG1 reg val: LW_PGRAPH_PRI_PD_PG1, %08x\n",pg1_val);

      pg_val = Validate_slave_PG_Reg(m_rndStream->RandomUnsigned(0x0, 0xFFFFFFFF));
      pg1_val = Validate_PG1_Reg(m_rndStream->RandomUnsigned(0x0, 0xFFFFFFFF));
      pSubdev->RegWr32( LW_PGRAPH_PRI_RSTR2D_PG    , pg_val);
      pSubdev->RegWr32( LW_PGRAPH_PRI_RSTR2D_PG1   , pg1_val);
      elpg_enabled = ELPG_enabled(pg_val);
      InfoPrintf("PG reg val: LW_PGRAPH_PRI_RSTR2D_PG, %08x, %s\n",pg_val, elpg_enabled);
      InfoPrintf("PG1 reg val: LW_PGRAPH_PRI_RSTR2D_PG1, %08x\n",pg1_val);

      pg_val = Validate_slave_PG_Reg(m_rndStream->RandomUnsigned(0x0, 0xFFFFFFFF));
      pg1_val = Validate_PG1_Reg(m_rndStream->RandomUnsigned(0x0, 0xFFFFFFFF));
      pSubdev->RegWr32( LW_PGRAPH_PRI_SCC_PG    , pg_val);
      pSubdev->RegWr32( LW_PGRAPH_PRI_SCC_PG1   , pg1_val);
      elpg_enabled = ELPG_enabled(pg_val);
      InfoPrintf("PG reg val: LW_PGRAPH_PRI_SCC_PG, %08x, %s\n",pg_val, elpg_enabled);
      InfoPrintf("PG1 reg val: LW_PGRAPH_PRI_SCC_PG1, %08x\n",pg1_val);

    }
    else{

      InfoPrintf("ELPG SPEEDY TEST: Running default gfx test.\n");

      // Step -1: Enable BLPG here

      //GR units in SYS
      pSubdev->RegWr32( LW_PGRAPH_PRI_CWD_PG                  ,0x10240000);  //NEW in kepler
      pSubdev->RegWr32( LW_PGRAPH_PRI_DS_PG                   ,0x10240400);
      pSubdev->RegWr32( LW_PGRAPH_PRI_FE_PG                   ,0x10240800);
      pSubdev->RegWr32( LW_PGRAPH_PRI_PD_PG                   ,0x10240c00);
      pSubdev->RegWr32( LW_PGRAPH_PRI_PDB_PG                  ,0x10241000);
      pSubdev->RegWr32( LW_PGRAPH_PRI_RSTR2D_PG               ,0x10241400);
      pSubdev->RegWr32( LW_PGRAPH_PRI_SCC_PG                  ,0x10241800);
      pSubdev->RegWr32( LW_PGRAPH_PRI_SKED_PG                 ,0x10241c00);  //NEW in kepler

      //GR units in GPC
      pSubdev->RegWr32( LW_PGRAPH_PRI_GPCS_CRSTR_PG           ,0x10242000);
      pSubdev->RegWr32( LW_PGRAPH_PRI_GPCS_FRSTR_PG           ,0x10242400);
      pSubdev->RegWr32( LW_PGRAPH_PRI_GPCS_GCC_PG             ,0x10242800);
      pSubdev->RegWr32( LW_PGRAPH_PRI_GPCS_GPM_RPT_PG         ,0x10242c00);
      pSubdev->RegWr32( LW_PGRAPH_PRI_GPCS_GPM_PD_PG          ,0x10243000);
      pSubdev->RegWr32( LW_PGRAPH_PRI_GPCS_GPM_SD_PG          ,0x10243400);
      pSubdev->RegWr32( LW_PGRAPH_PRI_GPCS_PROP_PG            ,0x10243800);
      pSubdev->RegWr32( LW_PGRAPH_PRI_GPCS_SETUP_PG           ,0x10243c00);
      pSubdev->RegWr32( LW_PGRAPH_PRI_GPCS_TC_PG              ,0x10244000);
      pSubdev->RegWr32( LW_PGRAPH_PRI_GPCS_SWDX_PG            ,0x10244400);
      pSubdev->RegWr32( LW_PGRAPH_PRI_GPCS_WIDCLIP_PG         ,0x10244800);
      pSubdev->RegWr32( LW_PGRAPH_PRI_GPCS_ZLWLL_PG           ,0x10244c00);
      pSubdev->RegWr32( LW_PGRAPH_PRI_GPCS_WDXPS_PG           ,0x10245000);
      pSubdev->RegWr32( LW_PGRAPH_PRI_GPCS_MMU_PG             ,0x10245400);

      //GR units in PPC
      pSubdev->RegWr32( LW_PGRAPH_PRI_GPCS_PPCS_CBM_PG        ,0x12246800);
      pSubdev->RegWr32( LW_PGRAPH_PRI_GPCS_PPCS_PES_PG        ,0x12246c00);
      pSubdev->RegWr32( LW_PGRAPH_PRI_GPCS_PPCS_WWDX_PG       ,0x12147000);

      //GR units in TPC
      pSubdev->RegWr32( LW_PGRAPH_PRI_GPCS_TPCS_MPC_PG        ,0x12247400);
      pSubdev->RegWr32( LW_PGRAPH_PRI_GPCS_TPCS_PE_PG         ,0x12247800);
      pSubdev->RegWr32( LW_PGRAPH_PRI_GPCS_TPCS_TEX_D_PG      ,0x12147c00);
      pSubdev->RegWr32( LW_PGRAPH_PRI_GPCS_TPCS_TEX_T_PG      ,0x12248000);
      pSubdev->RegWr32( LW_PGRAPH_PRI_GPCS_TPCS_TEX_IN_PG     ,0x12148400);
      pSubdev->RegWr32( LW_PGRAPH_PRI_GPCS_TPCS_TEX_TRM_PG    ,0x12248800);
      pSubdev->RegWr32( LW_PGRAPH_PRI_GPCS_TPCS_TEX_X_PG      ,0x12248c00);

      //GR units in SML1C
      pSubdev->RegWr32( LW_PGRAPH_PRI_GPCS_TPCS_L1C_PG        ,0x12149000);
      pSubdev->RegWr32( LW_PGRAPH_PRI_GPCS_TPCS_SM_BLPG_0_PG  ,0x12149400);
      pSubdev->RegWr32( LW_PGRAPH_PRI_GPCS_TPCS_SM_BLPG_1_PG  ,0x12149800);
      pSubdev->RegWr32( LW_PGRAPH_PRI_GPCS_TPCS_SM_BLPG_2_PG  ,0x12149c00);
      pSubdev->RegWr32( LW_PGRAPH_PRI_GPCS_TPCS_SM_BLPG_3_PG  ,0x1214a000);
      pSubdev->RegWr32( LW_PGRAPH_PRI_GPCS_TPCS_SM_BLPG_4_PG  ,0x1214a400);
      pSubdev->RegWr32( LW_PGRAPH_PRI_GPCS_TPCS_SM_BLPG_5_PG  ,0x1214a800);

      //GR units in FBP
      pSubdev->RegWr32( LW_PGRAPH_PRI_BES_CROP_PG             ,0x1024a400);
      pSubdev->RegWr32( LW_PGRAPH_PRI_BES_CROP_PART2_PG       ,0x1024a800);
      pSubdev->RegWr32( LW_PGRAPH_PRI_BES_RDM_PG              ,0x1024ac00);
      pSubdev->RegWr32( LW_PGRAPH_PRI_BES_RDM_RPM_C_PG        ,0x1024b000);
      pSubdev->RegWr32( LW_PGRAPH_PRI_BES_RDM_RPM_Z_PG        ,0x1024b400);
      pSubdev->RegWr32( LW_PGRAPH_PRI_BES_ZROP_PG             ,0x1024b800);
      pSubdev->RegWr32( LW_PGRAPH_PRI_BES_ZROP_EXPAND_PG      ,0x1024bc00);
   }
  //Step 0: Syscfg register from PMU is being configured here
  pSubdev->RegWr32( LW_PPWR_PMU_PG_CTRL(0), 1 );

  //Step 1: Enable the interrupts in ELPG
  WrRegArray(0, "LW_PPWR_PMU_PG_INTREN", 0x3111);

  //Step 2: Set idle threshold
  WrRegArray(0, "LW_PPWR_PMU_PG_IDLEFILTH", 40);
  WrRegArray(0, "LW_PPWR_PMU_PG_PPUIDLEFILTH", 4);

    //-----------------------------------------------------------
    // Do a GR engine BIND followed by an UNBIND to mimic the
    // unbind done by RM before power down.
    //-----------------------------------------------------------
  if(!skip_MMUBind) {
      MMUBindGR(0,pSubdev); // BIND
  }
  MMUBindGR(1,pSubdev); // UNBIND

  //-----------------------------------------------------------
  // Stage 2: Power-Down with SWCTRL
  //-----------------------------------------------------------
  InfoPrintf("Graphic ELPG Power Gating Test : Stage 2: Power-Down with SWCTRL\n");

  // Step 1: Turn on Clamps
  int swctrl = 0x0;
  swctrl |= DRF_DEF(_PPWR, _PMU_PG_SWCTRL, _PWR, _ON);
  swctrl |= DRF_NUM(_PPWR, _PMU_PG_SWCTRL, _ENABLE, 1);
  swctrl |= DRF_NUM(_PPWR, _PMU_PG_SWCTRL, _CLAMPSWITCH, 1);
  swctrl |= DRF_DEF(_PPWR, _PMU_PG_SWCTRL, _CLAMP, _ON);

  WrRegArray(0, "LW_PPWR_PMU_PG_SWCTRL", swctrl);

  // Step 2: Power down
  swctrl = 0x0;
  swctrl |= DRF_DEF(_PPWR, _PMU_PG_SWCTRL, _PWR, _OFF);
  swctrl |= DRF_NUM(_PPWR, _PMU_PG_SWCTRL, _ENABLE, 1);

  WrRegArray(0, "LW_PPWR_PMU_PG_SWCTRL", swctrl);

  for(int j=0; j < 100 ; j++)
  {
    RdRegArray(0,"LW_PPWR_PMU_PG_SWCTRL");
  }

  // Issue an ilwalidate request to make sure it does not hang as a result of GPCMMU being powered down [see bug 555852]
  MMUIlwalidate(pSubdev);

  //-----------------------------------------------------------
  // Stage 3 : Power-Up Using SWCTRL
  //-----------------------------------------------------------
  InfoPrintf("Graphic ELPG Power Gating Test : Stage 3: Power-Up Using SWCTRL\n");

  // Step 1: Power up
  swctrl = 0x0;
  swctrl |= DRF_DEF(_PPWR, _PMU_PG_SWCTRL, _PWR, _ON);
  swctrl |= DRF_NUM(_PPWR, _PMU_PG_SWCTRL, _ENABLE, 1);

  WrRegArray(0, "LW_PPWR_PMU_PG_SWCTRL", swctrl);

  // Adding dummy reads to extend the delay between power up and reset
  for(int j=0; j < 100 ; j++)
  {
    RdRegArray(0,"LW_PPWR_PMU_PG_SWCTRL");
  }

  // Step 2: Assert force_clks_on before asserting Reset for Graphics
  if (WriteGateCtrl(0, 0)) {
    ErrPrintf("Graphic ELPG Power Gating Test : Could not assert force clks on\n" );
  }

  // Step 3: Assert Reset for Graphics
  int retData =  lwgpu->GetGpuSubdevice()->RegRd32(LW_PMC_ELPG_ENABLE);
  pSubdev->RegWr32( LW_PMC_ELPG_ENABLE ,retData & 0xffffefff);

  // Step 4: Release clamps
  swctrl = 0x0;
  swctrl |= DRF_DEF(_PPWR, _PMU_PG_SWCTRL, _PWR, _ON);
  swctrl |= DRF_NUM(_PPWR, _PMU_PG_SWCTRL, _ENABLE, 1);
  swctrl |= DRF_NUM(_PPWR, _PMU_PG_SWCTRL, _CLAMPSWITCH, 1);
  swctrl |= DRF_DEF(_PPWR, _PMU_PG_SWCTRL, _CLAMP, _OFF);

  WrRegArray(0, "LW_PPWR_PMU_PG_SWCTRL", swctrl);

  // Step 5: Disable swctrl
  swctrl = 0x0;
  WrRegArray(0, "LW_PPWR_PMU_PG_SWCTRL", swctrl);

  // Step 6:  Deassert Reset Engine
  retData =  lwgpu->GetGpuSubdevice()->RegRd32(LW_PMC_ELPG_ENABLE);
  pSubdev->RegWr32( LW_PMC_ELPG_ENABLE ,retData | 0x1000);

  // Step 7: Deassert force_clks_on
  if (WriteGateCtrl(0, 1)) {
    ErrPrintf("Graphic ELPG Power Gating Test : Could not deassert force clks on\n" );
  }

    //-----------------------------------------------------------
    // Do a GR engine BIND followed by an HUBMMU ILWALIDATE  to
    // check that the ilwalidates get sent to GPCMMU when powered up.
    //-----------------------------------------------------------
  if(!skip_MMUBind) {
    MMUBindGR(0,pSubdev); // BIND
    }
  MMUIlwalidate(pSubdev);
  }
  return 0;
}

//****************************************************************************************
// Test Video Engine Power Gating
//****************************************************************************************
UINT32
Elpg_Powergv_Kepler::elpg_powergv_vd_keplerTest() {

  GpuSubdevice *pSubdev = lwgpu->GetGpuSubdevice();

  //-----------------------------------------------------------
  // Stage 1 : Initialization
  //-----------------------------------------------------------
  InfoPrintf("Video ELPG Power Gating Test : Stage 1: Initialization\n");

  //check clamp_en and sleep signal to make sure that when the fuse is not blown

  //Clear PMU in Reset
  int temp1 = pSubdev->RegRd32( LW_PMC_ENABLE);
  pSubdev->RegWr32( LW_PMC_ENABLE, temp1 | 0xffffffff ); //Enable all engines
  pSubdev->RegRd32( LW_PMC_ENABLE);

  pSubdev->RegWr32(LW_PPRIV_MASTER_RING_COMMAND_DATA , 0x503B5443);
  PrivInit();

//  if(fspg_option == 1)
//  {
//    InfoPrintf("Video ELPG Power Gating Test : Stage 0: Write FS_CONTROL\n");
//    pSubdev->RegWr32(LW_PTOP_FS_CONTROL, 0x6);
//  }

  //Enable BLPG here
  pSubdev->RegWr32( LW_PMSPDEC_PG, 0x10240200);
  pSubdev->RegWr32( LW_PMSVLD_PG, 0x10240200);
  pSubdev->RegWr32( LW_PMSPPP_PG, 0x10240200);
  pSubdev->RegWr32( LW_PMSENC_MISC_PG, 0x10240200);

  //Step 0: Syscfg register from PMU is being configured here
  pSubdev->RegWr32( LW_PPWR_PMU_PG_CTRL(0), 0x2 );

  //Step 1: Enable the interrupts in ELPG
  WrRegArray(1, "LW_PPWR_PMU_PG_INTREN", 0x3111);

  //Step 2: Set idle threshold
  WrRegArray(1, "LW_PPWR_PMU_PG_IDLEFILTH", 40);
  WrRegArray(1, "LW_PPWR_PMU_PG_PPUIDLEFILTH", 4);

  //-----------------------------------------------------------
  // Stage 2: Power-Down with SWCTRL
  //-----------------------------------------------------------
  InfoPrintf("Video ELPG Power Gating : Stage 2: Power-Down with SWCTRL\n");

  // Step 1: Turn on Clamps
  int swctrl = 0x0;
  swctrl |= DRF_DEF(_PPWR, _PMU_PG_SWCTRL, _PWR, _ON);
  swctrl |= DRF_NUM(_PPWR, _PMU_PG_SWCTRL, _ENABLE, 1);
  swctrl |= DRF_NUM(_PPWR, _PMU_PG_SWCTRL, _CLAMPSWITCH, 1);
  swctrl |= DRF_DEF(_PPWR, _PMU_PG_SWCTRL, _CLAMP, _ON);
  WrRegArray(1, "LW_PPWR_PMU_PG_SWCTRL", swctrl);

  // Step 2: Power down
  swctrl = 0x0;
  swctrl |= DRF_DEF(_PPWR, _PMU_PG_SWCTRL, _PWR, _OFF);
  swctrl |= DRF_NUM(_PPWR, _PMU_PG_SWCTRL, _ENABLE, 1);
  WrRegArray(1, "LW_PPWR_PMU_PG_SWCTRL", swctrl);

  for(int j=0; j < 100 ; j++)
  {
    RdRegArray(1,"LW_PPWR_PMU_PG_SWCTRL");
  }

  //-----------------------------------------------------------
  // Stage 3 : Power-Up Using SWCTRL
  //-----------------------------------------------------------
  InfoPrintf("Video ELPG Power Gating : Stage 3: Power-Up Using SWCTRL\n");

  // Step 1: Power up
  swctrl = 0x0;
  swctrl |= DRF_DEF(_PPWR, _PMU_PG_SWCTRL, _PWR, _ON);
  swctrl |= DRF_NUM(_PPWR, _PMU_PG_SWCTRL, _ENABLE, 1);
  WrRegArray(1, "LW_PPWR_PMU_PG_SWCTRL", swctrl);

  // Adding dummy reads to extend the delay between power up and reset
  for(int j=0; j < 100 ; j++)
  {
    RdRegArray(1,"LW_PPWR_PMU_PG_SWCTRL");
  }

  // Step 2: Assert force_clks_on before asserting Reset for Graphics
//  if (WriteGateCtrl(0, 0)) {
//    ErrPrintf("Video ELPG Power Gating : Could not assert force clks on\n" );
//  }

  // Step 3: Assert Reset for Video Engines
  int retData =  lwgpu->GetGpuSubdevice()->RegRd32(LW_PMC_ELPG_ENABLE);
  pSubdev->RegWr32( LW_PMC_ELPG_ENABLE ,retData & 0xfff97ffd);

  // Step 4: Release clamps
  swctrl = 0x0;
  swctrl |= DRF_DEF(_PPWR, _PMU_PG_SWCTRL, _PWR, _ON);
  swctrl |= DRF_NUM(_PPWR, _PMU_PG_SWCTRL, _ENABLE, 1);
  swctrl |= DRF_NUM(_PPWR, _PMU_PG_SWCTRL, _CLAMPSWITCH, 1);
  swctrl |= DRF_DEF(_PPWR, _PMU_PG_SWCTRL, _CLAMP, _OFF);
  WrRegArray(1, "LW_PPWR_PMU_PG_SWCTRL", swctrl);

  // Step 5: Disable swctrl
  swctrl = 0x0;
  WrRegArray(1, "LW_PPWR_PMU_PG_SWCTRL", swctrl);

  // Step 6:  Deassert Reset Engine
  retData =  lwgpu->GetGpuSubdevice()->RegRd32(LW_PMC_ELPG_ENABLE);
  pSubdev->RegWr32( LW_PMC_ELPG_ENABLE ,retData | 0x68002);

  // Step 7: Deassert force_clks_on
//  if (WriteGateCtrl(0, 1)) {
//    ErrPrintf("Video ELPG Power Gating : Could not deassert force clks on\n" );
//  }

  return 0;
}

//****************************************************************************************
// Test Memory System Power Gating
//****************************************************************************************
UINT32
Elpg_Powergv_Kepler::elpg_powergv_ms_keplerTest() {

  GpuSubdevice *pSubdev = lwgpu->GetGpuSubdevice();

  //-----------------------------------------------------------
  // Stage 1 : Initialization
  //-----------------------------------------------------------
  InfoPrintf("Memory System Power Gating Test : Stage 1: Initialization\n");

  //check clamp_en and sleep signal to make sure that when the fuse is not blown

  //Clear PMU in Reset
  int temp1 = pSubdev->RegRd32( LW_PMC_ENABLE);
  pSubdev->RegWr32( LW_PMC_ENABLE, temp1 | 0xffffffff ); //Enable all engines
  pSubdev->RegRd32( LW_PMC_ENABLE);

  pSubdev->RegWr32(LW_PPRIV_MASTER_RING_COMMAND_DATA , 0x503B5443);
  PrivInit();

//  if(fspg_option == 1)
//  {
//    InfoPrintf("Memory System Power Gating Test : Stage 0: Write FS_CONTROL\n");
//    pSubdev->RegWr32(LW_PTOP_FS_CONTROL, 0x6);
//  }

  //Enable BLPG here (Also include GR and VD units because GR and VD have to be powered down first before entering MSPG)

  //SYS MSPG units
  pSubdev->RegWr32( LW_PFB_PRI_FB_NISO_INGRESS_PG, 0x10240200);
  pSubdev->RegWr32( LW_PFB_PRI_FB_NISO_EGRESS_PG, 0x10240200);
  pSubdev->RegWr32( LW_PFB_PRI_MMU_PG, 0x10240200);

  //FBP MSPG units
  pSubdev->RegWr32( LW_PFB_FBPA_PRI_FBPA_PG, 0x10240200);
  pSubdev->RegWr32( LW_PLTCG_LTCS_LTSS_G_PRI_LTS_PG, 0x10240200);
//  pSubdev->RegWr32( LW_PLTCG_LTCS_MISC_DCMP_PRI_DCMP_PG, 0x10240200);
//  pSubdev->RegWr32( LW_PLTCG_LTCS_MISC_PRI_LTC_PG, 0x10240200);

  //Video units (Remove this as VD will not be power gated in gk104)
  //pSubdev->RegWr32( LW_PMSPDEC_PG, 0x10240200);
  //pSubdev->RegWr32( LW_PMSVLD_PG, 0x10240200);
  //pSubdev->RegWr32( LW_PMSPPP_PG, 0x10240200);
  //pSubdev->RegWr32( LW_PMSENC_MISC_PG, 0x10240200);

  //GR units in SYS
  pSubdev->RegWr32( LW_PGRAPH_PRI_CWD_PG                  ,0x10240200);  //NEW in kepler
  pSubdev->RegWr32( LW_PGRAPH_PRI_DS_PG                   ,0x10240200);
  pSubdev->RegWr32( LW_PGRAPH_PRI_FE_PG                   ,0x10240200);
  pSubdev->RegWr32( LW_PGRAPH_PRI_PD_PG                   ,0x10240200);
  pSubdev->RegWr32( LW_PGRAPH_PRI_PDB_PG                  ,0x10240200);
  pSubdev->RegWr32( LW_PGRAPH_PRI_RSTR2D_PG               ,0x10240200);
  pSubdev->RegWr32( LW_PGRAPH_PRI_SCC_PG                  ,0x10240200);
  pSubdev->RegWr32( LW_PGRAPH_PRI_SKED_PG                 ,0x10240200);  //NEW in kepler

  //GR units in GPC
  pSubdev->RegWr32( LW_PGRAPH_PRI_GPCS_CRSTR_PG           ,0x10240200);
  pSubdev->RegWr32( LW_PGRAPH_PRI_GPCS_FRSTR_PG           ,0x10240200);
  pSubdev->RegWr32( LW_PGRAPH_PRI_GPCS_GCC_PG             ,0x10240200);
  pSubdev->RegWr32( LW_PGRAPH_PRI_GPCS_GPM_RPT_PG         ,0x10240200);
  pSubdev->RegWr32( LW_PGRAPH_PRI_GPCS_GPM_PD_PG          ,0x10240200);
  pSubdev->RegWr32( LW_PGRAPH_PRI_GPCS_GPM_SD_PG          ,0x10240200);
  pSubdev->RegWr32( LW_PGRAPH_PRI_GPCS_PROP_PG            ,0x10240200);
  pSubdev->RegWr32( LW_PGRAPH_PRI_GPCS_SETUP_PG           ,0x10240200);
  pSubdev->RegWr32( LW_PGRAPH_PRI_GPCS_TC_PG              ,0x10240200);
  pSubdev->RegWr32( LW_PGRAPH_PRI_GPCS_SWDX_PG            ,0x10240200);
  pSubdev->RegWr32( LW_PGRAPH_PRI_GPCS_WIDCLIP_PG         ,0x10240200);
  pSubdev->RegWr32( LW_PGRAPH_PRI_GPCS_ZLWLL_PG           ,0x10240200);
  pSubdev->RegWr32( LW_PGRAPH_PRI_GPCS_WDXPS_PG           ,0x10240200);
  pSubdev->RegWr32( LW_PGRAPH_PRI_GPCS_MMU_PG             ,0x10240200);

  //GR units in PPC
  pSubdev->RegWr32( LW_PGRAPH_PRI_GPCS_PPCS_CBM_PG        ,0x12240000);
  pSubdev->RegWr32( LW_PGRAPH_PRI_GPCS_PPCS_PES_PG        ,0x12240000);
  pSubdev->RegWr32( LW_PGRAPH_PRI_GPCS_PPCS_WWDX_PG       ,0x12140A00);

  //GR units in TPC
  pSubdev->RegWr32( LW_PGRAPH_PRI_GPCS_TPCS_MPC_PG        ,0x12240000);
  pSubdev->RegWr32( LW_PGRAPH_PRI_GPCS_TPCS_PE_PG         ,0x12240000);
  pSubdev->RegWr32( LW_PGRAPH_PRI_GPCS_TPCS_TEX_D_PG      ,0x12143200);
  pSubdev->RegWr32( LW_PGRAPH_PRI_GPCS_TPCS_TEX_T_PG      ,0x12240000);
  pSubdev->RegWr32( LW_PGRAPH_PRI_GPCS_TPCS_TEX_IN_PG     ,0x12142D00);
  pSubdev->RegWr32( LW_PGRAPH_PRI_GPCS_TPCS_TEX_TRM_PG    ,0x12240000);
  pSubdev->RegWr32( LW_PGRAPH_PRI_GPCS_TPCS_TEX_X_PG      ,0x12240000);

  //GR units in SML1C
  pSubdev->RegWr32( LW_PGRAPH_PRI_GPCS_TPCS_L1C_PG        ,0x12140F00);
  pSubdev->RegWr32( LW_PGRAPH_PRI_GPCS_TPCS_SM_BLPG_0_PG         ,0x12141400);
  pSubdev->RegWr32( LW_PGRAPH_PRI_GPCS_TPCS_SM_BLPG_1_PG         ,0x12141400);
  pSubdev->RegWr32( LW_PGRAPH_PRI_GPCS_TPCS_SM_BLPG_2_PG         ,0x12141400);
  pSubdev->RegWr32( LW_PGRAPH_PRI_GPCS_TPCS_SM_BLPG_3_PG         ,0x12141400);
  pSubdev->RegWr32( LW_PGRAPH_PRI_GPCS_TPCS_SM_BLPG_4_PG         ,0x12141400);
  pSubdev->RegWr32( LW_PGRAPH_PRI_GPCS_TPCS_SM_BLPG_5_PG         ,0x12141400);

  //GR units in FBP
  pSubdev->RegWr32( LW_PGRAPH_PRI_BES_CROP_PG             ,0x10240200);
  pSubdev->RegWr32( LW_PGRAPH_PRI_BES_CROP_PART2_PG       ,0x10240200);
  pSubdev->RegWr32( LW_PGRAPH_PRI_BES_RDM_PG              ,0x10240200);
  pSubdev->RegWr32( LW_PGRAPH_PRI_BES_RDM_RPM_C_PG        ,0x10240200);
  pSubdev->RegWr32( LW_PGRAPH_PRI_BES_RDM_RPM_Z_PG        ,0x10240200);
  pSubdev->RegWr32( LW_PGRAPH_PRI_BES_ZROP_PG             ,0x10240200);
  pSubdev->RegWr32( LW_PGRAPH_PRI_BES_ZROP_EXPAND_PG      ,0x10240200);

  //Step 0: Syscfg register from PMU is being configured here
  pSubdev->RegWr32( LW_PPWR_PMU_PG_CTRL(0), 0x13); //GR, VD and MS

  //Step 1: Enable the interrupts in ELPG
  WrRegArray(0, "LW_PPWR_PMU_PG_INTREN", 0x3111);
  WrRegArray(1, "LW_PPWR_PMU_PG_INTREN", 0x3111);
  WrRegArray(4, "LW_PPWR_PMU_PG_INTREN", 0x3111);

  //Step 2: Set idle threshold
  WrRegArray(0, "LW_PPWR_PMU_PG_IDLEFILTH", 40);
  WrRegArray(1, "LW_PPWR_PMU_PG_IDLEFILTH", 40);
  WrRegArray(4, "LW_PPWR_PMU_PG_IDLEFILTH", 40);
  WrRegArray(0, "LW_PPWR_PMU_PG_PPUIDLEFILTH", 4);
  WrRegArray(1, "LW_PPWR_PMU_PG_PPUIDLEFILTH", 4);

    //-----------------------------------------------------------
    // Do a GR engine BIND followed by an UNBIND to mimic the
    // unbind done by RM before power down.
    //-----------------------------------------------------------
  if(!skip_MMUBind) {
    MMUBindGR(0,pSubdev); // BIND
    }
  MMUBindGR(1,pSubdev); // UNBIND
  //-----------------------------------------------------------
  // Stage 2: Power-Down with SWCTRL
  //-----------------------------------------------------------
  InfoPrintf("Memory System Power Gating : Stage 2: Power-Down with SWCTRL\n");

  //power down gr and video before power down ms units
  InfoPrintf("Memory System Power Gating : Stage 2.1: Power-Down GR and VD with SWCTRL\n");
  // Step 1: Turn on Clamps
  int swctrl = 0x0;
  swctrl |= DRF_DEF(_PPWR, _PMU_PG_SWCTRL, _PWR, _ON);
  swctrl |= DRF_NUM(_PPWR, _PMU_PG_SWCTRL, _ENABLE, 1);
  swctrl |= DRF_NUM(_PPWR, _PMU_PG_SWCTRL, _CLAMPSWITCH, 1);
  swctrl |= DRF_DEF(_PPWR, _PMU_PG_SWCTRL, _CLAMP, _ON);
  WrRegArray(0, "LW_PPWR_PMU_PG_SWCTRL", swctrl);
//  WrRegArray(1, "LW_PPWR_PMU_PG_SWCTRL", swctrl);

  // Step 2: Power down
  swctrl = 0x0;
  swctrl |= DRF_DEF(_PPWR, _PMU_PG_SWCTRL, _PWR, _OFF);
  swctrl |= DRF_NUM(_PPWR, _PMU_PG_SWCTRL, _ENABLE, 1);
  WrRegArray(0, "LW_PPWR_PMU_PG_SWCTRL", swctrl);
//  WrRegArray(1, "LW_PPWR_PMU_PG_SWCTRL", swctrl);
  for(int j=0; j < 100 ; j++)
  {
    RdRegArray(0,"LW_PPWR_PMU_PG_SWCTRL");
//    RdRegArray(1,"LW_PPWR_PMU_PG_SWCTRL");
  }

  // Issue an ilwalidate request to make sure it does not hang as a result of GPCMMU being powered down [see bug 555852]
  MMUIlwalidate(pSubdev);

  //power down ms units
  InfoPrintf("Memory System Power Gating : Stage 2.2: Power-Down MS with SWCTRL\n");
  // Step 1: Turn on Clamps
  swctrl = 0x0;
  swctrl |= DRF_DEF(_PPWR, _PMU_PG_SWCTRL, _PWR, _ON);
  swctrl |= DRF_NUM(_PPWR, _PMU_PG_SWCTRL, _ENABLE, 1);
  swctrl |= DRF_NUM(_PPWR, _PMU_PG_SWCTRL, _CLAMPSWITCH, 1);
  swctrl |= DRF_DEF(_PPWR, _PMU_PG_SWCTRL, _CLAMP, _ON);
  WrRegArray(4, "LW_PPWR_PMU_PG_SWCTRL", swctrl);

  // Step 2: Power down
  swctrl = 0x0;
  swctrl |= DRF_DEF(_PPWR, _PMU_PG_SWCTRL, _PWR, _OFF);
  swctrl |= DRF_NUM(_PPWR, _PMU_PG_SWCTRL, _ENABLE, 1);
  WrRegArray(4, "LW_PPWR_PMU_PG_SWCTRL", swctrl);
  for(int j=0; j < 100 ; j++)
  {
    RdRegArray(4,"LW_PPWR_PMU_PG_SWCTRL");
  }

  //-----------------------------------------------------------
  // Stage 3 : Power-Up Using SWCTRL
  //-----------------------------------------------------------
  InfoPrintf("Memory System Power Gating : Stage 3: Power-Up Using SWCTRL\n");

  // Step 1.1 power up ms units before gr and video
  InfoPrintf("Memory System Power Gating : Stage 3.1: Power-Up MS Using SWCTRL\n");
  swctrl = 0x0;
  swctrl |= DRF_DEF(_PPWR, _PMU_PG_SWCTRL, _PWR, _ON);
  swctrl |= DRF_NUM(_PPWR, _PMU_PG_SWCTRL, _ENABLE, 1);
  WrRegArray(4, "LW_PPWR_PMU_PG_SWCTRL", swctrl);
  // Adding dummy reads to extend the delay between power up and reset
  for(int j=0; j < 100 ; j++)
  {
    RdRegArray(4,"LW_PPWR_PMU_PG_SWCTRL");
  }

  //Step 1.2: Assert force_clks_on before asserting Reset for Graphics
  //if (WriteGateCtrl(0, 0)) {
  //  ErrPrintf("Memory System Power Gating Test : Could not assert force clks on\n" );
  //}

  // Step 1.3: Assert Reset for MSPG
  //Including MSPG units xbar, l2, fbpa, hub; Also includes  gr
  int retData =  lwgpu->GetGpuSubdevice()->RegRd32(LW_PMC_ELPG_ENABLE);
  pSubdev->RegWr32( LW_PMC_ELPG_ENABLE ,retData & 0xdfefeff3);
  retData =  lwgpu->GetGpuSubdevice()->RegRd32(DEVICE_BASE(LW_PCFG) + LW_XVE_RESET_CTRL);
  InfoPrintf("LW_XVE_RESET_CTRL is %x before assert xve2fb interface\n", retData);
  pSubdev->RegWr32( DEVICE_BASE(LW_PCFG) + LW_XVE_RESET_CTRL ,retData | 0x10000);  //assert LW_XVE_RESET_CTRL_FB_INTF_RESET to reset xve2fb intf
  retData =  lwgpu->GetGpuSubdevice()->RegRd32(DEVICE_BASE(LW_PCFG) + LW_XVE_RESET_CTRL);
  InfoPrintf("LW_XVE_RESET_CTRL is %x after asserting xve2fb interface\n", retData);

  // Step 1.4: Release clamps
  swctrl = 0x0;
  swctrl |= DRF_DEF(_PPWR, _PMU_PG_SWCTRL, _PWR, _ON);
  swctrl |= DRF_NUM(_PPWR, _PMU_PG_SWCTRL, _ENABLE, 1);
  swctrl |= DRF_NUM(_PPWR, _PMU_PG_SWCTRL, _CLAMPSWITCH, 1);
  swctrl |= DRF_DEF(_PPWR, _PMU_PG_SWCTRL, _CLAMP, _OFF);
  WrRegArray(4, "LW_PPWR_PMU_PG_SWCTRL", swctrl);

  // Step 1.5: Disable swctrl
  swctrl = 0x0;
  WrRegArray(4, "LW_PPWR_PMU_PG_SWCTRL", swctrl);

  // Step 1.6:  Deassert Reset Engine
  //Including MSPG units xbar, l2, fbpa, hub; Also includes  gr
  retData =  lwgpu->GetGpuSubdevice()->RegRd32(LW_PMC_ELPG_ENABLE);
  pSubdev->RegWr32( LW_PMC_ELPG_ENABLE ,retData | 0x2016900e);
  retData =  lwgpu->GetGpuSubdevice()->RegRd32(DEVICE_BASE(LW_PCFG) + LW_XVE_RESET_CTRL);
  InfoPrintf("LW_XVE_RESET_CTRL is %x before de-assert xve2fb interface\n", retData);
  pSubdev->RegWr32( DEVICE_BASE(LW_PCFG) + LW_XVE_RESET_CTRL, retData & 0xfffeffff);  //de-assert LW_XVE_RESET_CTRL_FB_INTF_RESET to reset xve2fb intf
  retData =  lwgpu->GetGpuSubdevice()->RegRd32(DEVICE_BASE(LW_PCFG) + LW_XVE_RESET_CTRL);
  InfoPrintf("LW_XVE_RESET_CTRL is %x after de-assert xve2fb interface\n", retData);

  // Step 7: Deassert force_clks_on
//  if (WriteGateCtrl(0, 1)) {
//    ErrPrintf("Memory System Power Gating : Could not deassert force clks on\n" );
//  }

  // Step 2: Power up GR
  InfoPrintf("Memory System Power Gating Test : Stage 3.2: Power-Up GR Using SWCTRL\n");

  // Step 1: Power up
  swctrl = 0x0;
  swctrl |= DRF_DEF(_PPWR, _PMU_PG_SWCTRL, _PWR, _ON);
  swctrl |= DRF_NUM(_PPWR, _PMU_PG_SWCTRL, _ENABLE, 1);
  WrRegArray(0, "LW_PPWR_PMU_PG_SWCTRL", swctrl);
//  WrRegArray(1, "LW_PPWR_PMU_PG_SWCTRL", swctrl);
  // Adding dummy reads to extend the delay between power up and reset
  for(int j=0; j < 100 ; j++)
  {
    RdRegArray(0,"LW_PPWR_PMU_PG_SWCTRL");
//    RdRegArray(1,"LW_PPWR_PMU_PG_SWCTRL");
  }

  // Step 2: Assert force_clks_on before asserting Reset for Graphics
  if (WriteGateCtrl(0, 0)) {
    ErrPrintf("Memory System Power Gating Test : Could not assert force clks on\n" );
  }

  // Step 3: Assert Reset for Graphics engines
  retData =  lwgpu->GetGpuSubdevice()->RegRd32(LW_PMC_ELPG_ENABLE);
  pSubdev->RegWr32( LW_PMC_ELPG_ENABLE ,retData & 0xffffefff);

  // Step 4: Release clamps
  swctrl = 0x0;
  swctrl |= DRF_DEF(_PPWR, _PMU_PG_SWCTRL, _PWR, _ON);
  swctrl |= DRF_NUM(_PPWR, _PMU_PG_SWCTRL, _ENABLE, 1);
  swctrl |= DRF_NUM(_PPWR, _PMU_PG_SWCTRL, _CLAMPSWITCH, 1);
  swctrl |= DRF_DEF(_PPWR, _PMU_PG_SWCTRL, _CLAMP, _OFF);
  WrRegArray(0, "LW_PPWR_PMU_PG_SWCTRL", swctrl);
//  WrRegArray(1, "LW_PPWR_PMU_PG_SWCTRL", swctrl);

  // Step 5: Disable swctrl
  swctrl = 0x0;
  WrRegArray(0, "LW_PPWR_PMU_PG_SWCTRL", swctrl);
//  WrRegArray(1, "LW_PPWR_PMU_PG_SWCTRL", swctrl);

  // Step 6:  Deassert Reset Engine
  retData =  lwgpu->GetGpuSubdevice()->RegRd32(LW_PMC_ELPG_ENABLE);
  pSubdev->RegWr32( LW_PMC_ELPG_ENABLE ,retData | 0x1000);

  // Step 7: Deassert force_clks_on
  if (WriteGateCtrl(0, 1)) {
    ErrPrintf("Memory System Power Gating Test : Could not deassert force clks on\n" );
  }

  //-----------------------------------------------------------
  // Do a GR engine BIND followed by an HUBMMU ILWALIDATE  to
  // check that the ilwalidates get sent to GPCMMU when powered up.
  //-----------------------------------------------------------
  if(!skip_MMUBind) {
    MMUBindGR(0,pSubdev); // BIND
    }
  MMUIlwalidate(pSubdev);
  return 0;
}

//****************************************************************************************
// Test FLOOR SWEEP Power Gating (FSPG)
//****************************************************************************************
UINT32
Elpg_Powergv_Kepler::elpg_powergv_fspg_keplerTest() {
/**********************************************************************************************************************
 * This test has been commented as it conflicts with the updates in elpg_powergv_gx_maxwellTest().
 * Details of the conflict are as follows:
 * 	Ampere onwards, LW_PFIFO_* registers (used by the eplg_powergv_gx_maxwellTest()) have been replaced
 * 	with LW_RUNLIST_* registers. The base address for LW_RUNLIST_* is to be read from LW_PTOP_DEVICE_CFG 
 * 	register present under ampere/dev_top.ref. Due to some common register names at different addresses,
 * 	defines of kepler/dev_top.ref conflicts with ampere/dev_top.ref. The LW_PTOP_FS_* registers used by 
 * 	this test were defined in kepler/dev_top.ref (but are not defined in ampere/dev_top.ref). Hence, this
 * 	test was commented till it's updated to registers in Ampere, so as to not break compilation of other tests.
 **********************************************************************************************************************/
/*
  GpuSubdevice *pSubdev = lwgpu->GetGpuSubdevice();

  //-----------------------------------------------------------
  // Stage 1 : Initialization
  //-----------------------------------------------------------
  InfoPrintf("Floor Sweep Power Gating Test : Stage 1: Initialization\n");

  //Enable all engines
  int temp1 = pSubdev->RegRd32( LW_PMC_ENABLE);
  pSubdev->RegWr32( LW_PMC_ENABLE, temp1 | 0xffffffff ); //Enable all engines
  pSubdev->RegRd32( LW_PMC_ENABLE);

  //Init PRIV ring
  pSubdev->RegWr32(LW_PPRIV_MASTER_RING_COMMAND_DATA , 0x503B5443);
  PrivInit();

  // Step 1: Enable FSPG
  InfoPrintf("Floor Sweep Power Gating Test : Stage 2: Enable FSPG\n");
  pSubdev->RegWr32(LW_PTOP_FS_CONTROL, fs_control);
  pSubdev->RegWr32(LW_PTOP_FS_CONTROL_GPC, fs_control_gpc);
  pSubdev->RegWr32(LW_PTOP_FS_CONTROL_GPC_TPC(0), fs_control_gpc_tpc);
  pSubdev->RegWr32(LW_PTOP_FS_CONTROL_GPC_TPC(1), fs_control_gpc_tpc);
  pSubdev->RegWr32(LW_PTOP_FS_CONTROL_GPC_ZLWLL(0), fs_control_gpc_zlwll);
  pSubdev->RegWr32(LW_PTOP_FS_CONTROL_GPC_ZLWLL(1), fs_control_gpc_zlwll);

  // Adding dummy reads to add some delay after enabling FSPG
  InfoPrintf("Floor Sweep Power Gating Test : Stage 3: Wait for a while after enabling FSPG\n");
  for(int j=0; j < 1000 ; j++)
  {
    RdRegArray(1,"LW_PPWR_PMU_PG_SWCTRL");
  }

  InfoPrintf("Floor Sweep Power Gating Test : Stage 4: Done with FSPG test\n");
*/
  return 0;
}

//*******************************************************************************************
// Test BBOX unit in functional(Power gated, Non-power gated) as well as non-functional modes
//*******************************************************************************************
UINT32
Elpg_Powergv_Kepler::bbox_verify_Test() {

  InfoPrintf("/******** BBOX_VERIFY ********/\n");
  InfoPrintf("Commencing BBOX_verfiy_Test\n");
  DelayUsingDummyReads();

  // Toggling scan_en input
  int i;
  int value = 0;
  InfoPrintf("Entering scan_en\n");
  for(i=0; i < 10; i=i+1) {
  value = value | (1<<i);
  Platform::EscapeWrite("scan_en_escape_0",0,10,value);
  DelayUsingDummyReads();
  }
  DelayUsingDummyReads();
  value = 0x3ff;
  for(i=9; i >= 0; i=i-1) {
  value = value ^ (1<<i);
  Platform::EscapeWrite("scan_en_escape_0",0,10,value);
  DelayUsingDummyReads();
  }
  InfoPrintf("Exiting scan_en\n");

  // Toggling test_mode input
  InfoPrintf("Entering test_mode\n");
  Platform::EscapeWrite("test_mode_escape_0",0,1,1);
  DelayUsingDummyReads();
  Platform::EscapeWrite("test_mode_escape_0",0,1,0);
  InfoPrintf("Exiting test_mode\n");

  // Toggling iddq_mode input
  InfoPrintf("Entering iddq_mode\n");
  Platform::EscapeWrite("iddq_mode_escape_0",0,1,1);
  DelayUsingDummyReads();
  Platform::EscapeWrite("iddq_mode_escape_0",0,1,0);
  InfoPrintf("Exiting iddq_mode\n");

  // Toggling fs2all_tpc_disable_escape_0 input
  InfoPrintf("Entering FSPG mode\n");
  value = 0;
  for(i=0; i < 4; i=i+1) {
  value = value | (1<<i);
  Platform::EscapeWrite("fs2all_tpc_disable_escape_0",0,4,value);
  DelayUsingDummyReads();
  }
  DelayUsingDummyReads();
  value = 0xf;
  for(i=3; i >= 0; i=i-1) {
  value = value ^ (1<<i);
  Platform::EscapeWrite("fs2all_tpc_disable_escape_0",0,4,value);
  DelayUsingDummyReads();
  }
  InfoPrintf("Exiting FSPG mode\n");

  // Powering down from zone7->0, powering up from zone0->7
  InfoPrintf("Entering pwr2ram_pg_sleep_en mode\n");
  value = 0;
  for(i=7; i >= 0; i=i-1) {
  value = value | (1<<i);
  Platform::EscapeWrite("pwr2ram_pg_sleep_en_escape_0",0,8,value);
  DelayUsingDummyReads();
  }
  DelayUsingDummyReads();
  value = 0xff;
  for(i=0; i < 8; i=i+1) {
  value = value ^ (1<<i);
  Platform::EscapeWrite("pwr2ram_pg_sleep_en_escape_0",0,8,value);
  DelayUsingDummyReads();
  }
  InfoPrintf("Exiting pwr2ram_pg_sleep_en mode\n");

  DelayUsingDummyReads();
  InfoPrintf("Finished BBOX_verfiy_Test\n");

  return 0;
}

//****************************************************************************************
// Miscellaneous functions used in the test
//****************************************************************************************
int Elpg_Powergv_Kepler::WriteReadCheckVal(UINT32 reg, UINT32 regMsk, const char *regName, UINT32 data) {
  UINT32 retData;

  lwgpu->GetGpuSubdevice()->RegWr32(reg, data);
  retData =  lwgpu->GetGpuSubdevice()->RegRd32(reg);
  if ((retData & regMsk) == (data & regMsk)) {
    InfoPrintf("%s: %s: Read correct: %s: 0x%08x(0x%08x with mask), Got 0x%08x(0x%08x with mask), Mask=0x%08x\n", DEBUG_PRINT , __FUNCTION__, regName, data, (data & regMsk), retData, (retData & regMsk), regMsk);
    DebugPrintf("%s read correct: Expected 0x%08x(0x%08x with mask), Got 0x%08x(0x%08x with mask)\n",
               regName, data, (data & regMsk), retData, (retData & regMsk));
    return 0;
  } else {
    InfoPrintf("%s: %s: Read error: %s: 0x%08x(0x%08x with mask), Got 0x%08x(0x%08x with mask), Mask=0x%08x\n", DEBUG_PRINT , __FUNCTION__, regName, data, (data & regMsk), retData, (retData & regMsk), regMsk);
    ErrPrintf("%s read ERROR: Expected 0x%08x(0x%08x with mask), Got 0x%08x(0x%08x with mask)\n",
              regName, data, (data & regMsk), retData, (retData & regMsk));
    return 1;
  }
}

int Elpg_Powergv_Kepler::ReadCheckVal(UINT32 reg, UINT32 regMsk, const char *regName, UINT32 data) {
  UINT32 retData;
  retData =  lwgpu->GetGpuSubdevice()->RegRd32(reg);
  if ((retData & regMsk) == (data & regMsk)) {
    InfoPrintf("%s: %s: Read correct: %s: 0x%08x(0x%08x with mask), Got 0x%08x(0x%08x with mask)\n", DEBUG_PRINT , __FUNCTION__, regName, data, (data & regMsk), retData, (retData & regMsk));
    DebugPrintf("%s read correct: Expected 0x%08x(0x%08x with mask), Got 0x%08x(0x%08x with mask)\n",
               regName, data, (data & regMsk), retData, (retData & regMsk));
    return 0;
  } else {
    InfoPrintf("%s: %s: Read error: %s: 0x%08x(0x%08x with mask), Got 0x%08x(0x%08x with mask)\n", DEBUG_PRINT , __FUNCTION__, regName, data, (data & regMsk), retData, (retData & regMsk));
    ErrPrintf("%s read ERROR: Expected 0x%08x(0x%08x with mask), Got 0x%08x(0x%08x with mask)\n",
              regName, data, (data & regMsk), retData, (retData & regMsk));
    return 1;
  }
}

int
Elpg_Powergv_Kepler::ChkRegVal2(UINT32 reg, UINT32 regMsk, const char *regName, UINT32 data) {
  UINT32 retData;

  retData =  lwgpu->GetGpuSubdevice()->RegRd32(reg);
  if ((retData & regMsk) == (data & regMsk)) {
    DebugPrintf("%s read correct: Expected 0x%08x(0x%08x with mask), Got 0x%08x(0x%08x with mask)\n",
               regName, data, (data & regMsk), retData, (retData & regMsk));
    return 0;
  } else {
    ErrPrintf("%s read error: Expected 0x%08x(0x%08x with mask), Got 0x%08x(0x%08x with mask)\n",
              regName, data, (data & regMsk), retData, (retData & regMsk));
    return 1;
  }
}

int
Elpg_Powergv_Kepler::ChkRegVal3(UINT32 reg, UINT32 regMsk, const char *regName, UINT32 data) {
  UINT32 retData;

  retData =  lwgpu->GetGpuSubdevice()->RegRd32(reg);
  if ((retData & regMsk) == (data & regMsk)) {
    DebugPrintf("%s read correct: Expected 0x%08x(0x%08x with mask), Got 0x%08x(0x%08x with mask)\n",
               regName, data, (data & regMsk), retData, (retData & regMsk));
    return 0;
  } else {
    InfoPrintf("%s Expected 0x%08x(0x%08x with mask), Got 0x%08x(0x%08x with mask)\n",
              regName, data, (data & regMsk), retData, (retData & regMsk));
    return 1;
  }
}

int
Elpg_Powergv_Kepler::WrRegArray(int index, const char *regName, UINT32 data)
{
    UINT32 Value;
    unique_ptr<IRegisterClass> pReg = lwgpu->GetRegister(regName);
    if (!pReg)
    {
        InfoPrintf("%s: %s: Error: Could not find the register %s\n", DEBUG_PRINT , __FUNCTION__, regName);
        ErrPrintf("Could not GetRegister(%s)!\n", regName);
        return (1);
    }

    lwgpu->RegWr32(pReg->GetAddress(index), data);

    Value = lwgpu->RegRd32(pReg->GetAddress(index));
    InfoPrintf("%s: %s: Read value of %s[%d] is 0x%8x\n", DEBUG_PRINT , __FUNCTION__, regName, index, Value);
    InfoPrintf("%s%d value is 0x%8x. \n", regName, index, Value);

    return 0;
}

UINT32
Elpg_Powergv_Kepler::RdRegArray(int index, const char *regName)
{
    UINT32 Value;
    unique_ptr<IRegisterClass> pReg = lwgpu->GetRegister(regName);
    if (!pReg)
    {
        InfoPrintf("%s: %s: Error: Could not find the register %s\n", DEBUG_PRINT , __FUNCTION__, regName);
        ErrPrintf("Could not GetRegister(%s)!\n", regName);
        return (0);
    }

    Value = lwgpu->RegRd32(pReg->GetAddress(index));

    return Value;
}

int Elpg_Powergv_Kepler::senseFuse(void)
{
/*This fuction program one fuse with the address addr and value data
*/
    UINT32 i;
    UINT32 fuse_state;
    int errCnt = 0;
    GpuSubdevice *pSubdev = lwgpu->GetGpuSubdevice();
    InfoPrintf("FuseTest:  senseFuse begins\n");
    lwgpu->RegWr32(LW_FUSE_FUSEADDR,1);

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

int Elpg_Powergv_Kepler::programFuse(UINT32 addr, UINT32 data, UINT32 mask)

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

void Elpg_Powergv_Kepler::PollMMUFifoEmpty(GpuSubdevice *pSubdev) {
  // Poll for MMU_CTRL_PRI_FIFO_EMPTY == TRUE.
  int fifo_empty_mask = 0x0 | DRF_DEF(_PFB, _PRI_MMU_CTRL, _PRI_FIFO_EMPTY, _TRUE) | DRF_DEF(_PFB, _PRI_MMU_CTRL, _PRI_FIFO_ERROR, _TRUE);
  int fifo_empty_exp  = 0x0 | DRF_DEF(_PFB, _PRI_MMU_CTRL, _PRI_FIFO_EMPTY, _TRUE) | DRF_DEF(_PFB, _PRI_MMU_CTRL, _PRI_FIFO_ERROR, _FALSE);
  int mmu_ctrl = 0x0;
  while(1) {
    mmu_ctrl = pSubdev->RegRd32(LW_PFB_PRI_MMU_CTRL) & fifo_empty_mask;
    InfoPrintf("ELPG SPEEDY TEST: Polling for LW_PFB_PRI_MMU_CTRL_PRI_FIFO_EMPTY == TRUE. LW_PFB_PRI_MMU_CTRL read data = 0x%x \n", mmu_ctrl);
    if ((mmu_ctrl & fifo_empty_mask) == fifo_empty_exp) {break;}
  }
}

void Elpg_Powergv_Kepler::Wait(int count) {
  int i ;
  for (i=0 ; i<count; i++);
}

// Function to bind GR engine
// unbind = 0 => BIND, 1 => UNBIND.
void Elpg_Powergv_Kepler::MMUBindGR(int unbind, GpuSubdevice *pSubdev) {
  int bindreg = 0x0;
  // APERTURE = 0x1 to unbind [using hardcoded value since define does not exist in manuals].
  if (unbind == 1) bindreg = 0x1;
  else bindreg = DRF_DEF(_PFB, _PRI_MMU_BIND_IMB, _APERTURE, _SYS_MEM_C);
  bindreg |= DRF_DEF(_PFB, _PRI_MMU_BIND_IMB, _VOL, _TRUE);
  pSubdev->RegWr32(LW_PFB_PRI_MMU_BIND_IMB, bindreg);
  bindreg = 0x0;
  bindreg |= DRF_DEF(_PFB, _PRI_MMU_BIND, _ENGINE_ID, _VEID0); // changing _GRAPHICS to _VEID0 as Volta supports sub-context feature.
  bindreg |= DRF_DEF(_PFB, _PRI_MMU_BIND, _TRIGGER, _TRUE);
  pSubdev->RegWr32(LW_PFB_PRI_MMU_BIND, bindreg);
  PollMMUFifoEmpty(pSubdev);
}

void Elpg_Powergv_Kepler::MMUIlwalidate(GpuSubdevice *pSubdev) {
  // Issue an ilwalidate MMU request, and poll for MMU_CTRL_PRI_FIFO_EMPTY == TRUE
  // Updating Ilwalidate code for Volta referring from mmugp100.c
  int ilwreg = 0x0;
  ilwreg |= DRF_DEF(_PFB, _PRI_MMU_ILWALIDATE, _ALL_VA, _TRUE);
  ilwreg |= DRF_DEF(_PFB, _PRI_MMU_ILWALIDATE, _ALL_PDB, _TRUE);
  ilwreg |= DRF_DEF(_PFB, _PRI_MMU_ILWALIDATE, _HUBTLB_ONLY, _FALSE);
  ilwreg |= DRF_DEF(_PFB, _PRI_MMU_ILWALIDATE, _TRIGGER, _TRUE);
  pSubdev->RegWr32(LW_PFB_PRI_MMU_ILWALIDATE, ilwreg);
  PollMMUFifoEmpty(pSubdev);
}

void Elpg_Powergv_Kepler::updateFuseopt(void)
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

void Elpg_Powergv_Kepler::Enable_fuse_program(void)
{
   GpuSubdevice *pSubdev = lwgpu->GetGpuSubdevice();
   UINT32 fuse_state;

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

    programFuse(0, 0x1, 0x1);

    //after write the first 2 rows, sense fuses to give the global configurations
    senseFuse();
}

void Elpg_Powergv_Kepler::PrivInit(void) {
  int err = 0;

  unique_ptr<IRegisterClass> reg;
  unique_ptr<IRegisterField> field;
  unique_ptr<IRegisterValue> value;
  UINT32 addr;
  UINT32 mask;
  UINT32 start_bit;
  UINT32 reg_rd_val,reg_wr_val;
  //UINT32 reg_rd_val_masked,reg_wr_val_masked;

  UINT32 arch = lwgpu->GetArchitecture();
  if(arch < 0x400) {
    return;
  }

  //reset priv ring master
 // if (!(reg = m_regMap->FindRegister("LW_PMC_ENABLE"))) {
 //   ErrPrintf("PrivReInit::CheckRegister: register LW_PMC_ENABLE is not defined!\n");
 //   err++;
 // }
 // //if (!(field = reg->FindField("LW_PMC_ENABLE_PRIV_RING"))) {
 // //  ErrPrintf("PrivReInit::CheckRegister: field LW_PMC_ENABLE_PRIV_RING is not defined!\n");
 // //  err++;;

 // //}
 // //if (!(value = field->FindValue("LW_PMC_ENABLE_PRIV_RING_DISABLED"))) {
 // //  ErrPrintf("PrivReInit::CheckRegister: value LW_PMC_ENABLE_PRIV_RING_DISABLED is not defined!\n");
 // //  err++;
 // //}
 // addr = reg->GetAddress();
 // reg_rd_val = lwgpu->RegRd32(addr);
 // InfoPrintf("PrivReInit::PrivReInitTest: read LW_PMC_ENABLE,reg_rd_val=0x%x\n",reg_rd_val);
 // mask = field->GetMask();
 // start_bit = field->GetStartBit();
 // reg_wr_val = (reg_rd_val | mask) & (~mask | (value->GetValue() << start_bit));
 // lwgpu->RegWr32(addr,reg_wr_val);
 // InfoPrintf("PrivReInit::PrivReInitTest: write LW_PMC_ENABLE,reg_wr_val=0x%x\n",reg_wr_val);
 //
 // //if (!(value = field->FindValue("LW_PMC_ENABLE_PRIV_RING_ENABLED"))) {
 // //  ErrPrintf("PrivReInit::PrivReInitTest: value LW_PMC_ENABLE_PRIV_RING_ENABLED is not defined!\n");
 // //  err++;
 // //}
 // reg_wr_val = (reg_rd_val | mask) & (~mask | (value->GetValue() << start_bit));
 // lwgpu->RegWr32(addr,reg_wr_val);
 // InfoPrintf("PrivReInit::PrivReInitTest: write LW_PMC_ENABLE,reg_wr_val=0x%x\n",reg_wr_val);

  //check the connectivity
  if (!(reg = m_regMap->FindRegister("LW_PPRIV_MASTER_RING_START_RESULTS"))) {
    ErrPrintf("PrivReInit::PrivReInitTest: register LW_PPRIV_MASTER_RING_START_RESULTS is not defined!\n");
    err++;
  }
  if (!(field = reg->FindField("LW_PPRIV_MASTER_RING_START_RESULTS_CONNECTIVITY"))) {
    ErrPrintf("PrivReInit::PrivReInitTest: field LW_PPRIV_MASTER_RING_START_RESULTS_CONNECTIVITY is not defined!\n");
    err++;
  }
  if (!(value = field->FindValue("LW_PPRIV_MASTER_RING_START_RESULTS_CONNECTIVITY_FAIL"))) {
    ErrPrintf("PrivReInit::PrivReInitTest: value LW_PPRIV_MASTER_RING_STATUS_RESULTS_CONNECTIVITY_FAIL is not defined!\n");
    err++;
  }
  addr = reg->GetAddress();
  reg_rd_val = lwgpu->RegRd32(addr);
  InfoPrintf("PrivReInit::PrivReInitTest: read LW_PPRIV_MASTER_RING_START_RESULTS,reg_rd_val=0x%x\n",reg_rd_val);

  start_bit = field->GetStartBit();
  mask = field->GetMask();
  if (((reg_rd_val & mask) >> start_bit) != value->GetValue()) {
    ErrPrintf("PrivReInit::PrivReInitTest: expected field value is 0x%x\n",value->GetValue());
    err++;
  }

  //rop on ring not start
  if (!(reg = m_regMap->FindRegister("LW_PPRIV_SYS_PRIV_DECODE_CONFIG"))) {
    ErrPrintf("PrivReInit::PrivReInitTest: register LW_PPRIV_SYS_PRIV_DECODE_CONFIG is not defined!\n");
    err++;
  }
  if (!(field = reg->FindField("LW_PPRIV_SYS_PRIV_DECODE_CONFIG_RING"))) {
    ErrPrintf("PrivReInit::PrivReInitTest: field LW_PPRIV_SYS_PRIV_DECODE_CONFIG_RING is not defined!\n");
    err++;
  }
  if (!(value = field->FindValue("LW_PPRIV_SYS_PRIV_DECODE_CONFIG_RING_DROP_ON_RING_NOT_STARTED"))) {
    ErrPrintf("PrivReInit::PrivReInitTest: value LW_PPRIV_SYS_PRIV_DECODE_CONFIG_RING_DROP_ON_RING_NOT_STARTED is not defined!\n");
    err++;
  }
  addr = reg->GetAddress();
  reg_rd_val = lwgpu->RegRd32(addr);
  InfoPrintf("PrivReInit::PrivReInitTest: read LW_PPRIV_SYS_PRIV_DECODE_CONFIG,reg_rd_val=0x%x\n",reg_rd_val);
  mask = field->GetMask();
  start_bit = field->GetStartBit();
  reg_wr_val = (reg_rd_val | mask) & (~mask | (value->GetValue() << start_bit));
  lwgpu->RegWr32(addr,reg_wr_val);
  InfoPrintf("PrivReInit::PrivReInitTest: write LW_PPRIV_SYS_PRIV_DECODE_CONFIG,reg_wr_val=0x%x\n",reg_wr_val);

  //fast priv ring init
  if (!(reg = m_regMap->FindRegister("LW_PPRIV_MASTER_RING_COMMAND"))) {
    ErrPrintf("PrivReInit::PrivReInitTest: register LW_PPRIV_MASTER_RING_COMMAND is not defined!\n");
    err ++;
  }
  if (!(field = reg->FindField("LW_PPRIV_MASTER_RING_COMMAND_CMD"))) {
    ErrPrintf("PrivReInit::PrivReInitTest: field LW_PPRIV_MASTER_RING_COMMAND_CMD is not defined!\n");
    err++;
  }
  if (!(value = field->FindValue("LW_PPRIV_MASTER_RING_COMMAND_CMD_ENUMERATE_AND_START_RING"))) {
    ErrPrintf("PrivReInit::PrivReInitTest: value LW_PPRIV_MASTER_RING_GLOBAL_COMMAND_CMD_ENUMERATE_AND_START_RING is not defined!\n");
    err++;
  }
  addr = reg->GetAddress();
  reg_rd_val = lwgpu->RegRd32(addr);
  InfoPrintf("PrivReInit::PrivReInitTest: read LW_PPRIV_MASTER_RING_COMMAND,reg_rd_val=0x%x\n",reg_rd_val);
  mask = field->GetMask();
  start_bit = field->GetStartBit();
  reg_wr_val = (reg_rd_val | mask) & (~mask | (value->GetValue() << start_bit));
  lwgpu->RegWr32(addr,reg_wr_val);
  InfoPrintf("PrivReInit::PrivReInitTest: write LW_PPRIV_MASTER_RING_COMMAND,reg_wr_val=0x%x\n",reg_wr_val);

  //wait for startup to complete
  if (!(reg = m_regMap->FindRegister("LW_PPRIV_SYS_PRIV_DECODE_CONFIG"))) {
    ErrPrintf("PrivReInit::PrivReInitTest: register LW_PPRIV_SYS_PRIV_DECODE_CONFIG is not defined!\n");
    err ++;
  }
  if (!(field = reg->FindField("LW_PPRIV_SYS_PRIV_DECODE_CONFIG_RING"))) {
    ErrPrintf("PrivReInit::PrivReInitTest: field LW_PPRIV_SYS_PRIV_DECODE_CONFIG_RING is not defined!\n");
    err++;
  }
  if (!(value = field->FindValue("LW_PPRIV_SYS_PRIV_DECODE_CONFIG_RING_WAIT_FOR_RING_START_COMPLETE"))) {
    ErrPrintf("PrivReInit::PrivReInitTest: value LW_PPRIV_SYS_PRIV_DECODE_CONFIG_RING_WAIT_FOR_RING_START_COMPLETE is not defined!\n");
    err++;
  }
  addr = reg->GetAddress();
  reg_rd_val = lwgpu->RegRd32(addr);
  InfoPrintf("PrivReInit::PrivReInitTest: read LW_PPRIV_SYS_PRIV_DECODE_CONFIG,reg_rd_val=0x%x\n",reg_rd_val);
  mask = field->GetMask();
  start_bit = field->GetStartBit();
  reg_wr_val = (reg_rd_val | mask) & (~mask | (value->GetValue() << start_bit));
  lwgpu->RegWr32(addr,reg_wr_val);
  InfoPrintf("PrivReInit::PrivReInitTest: write LW_PPRIV_SYS_PRIV_DECODE_CONFIG,reg_wr_val=0x%x\n",reg_wr_val);
  //poll the status
  do {
    reg_rd_val = lwgpu->RegRd32(addr);
    InfoPrintf("PrivReInit::PrivReInitTest: LW_PPRIV_SYS_PRIV_DECODE_CONFIG,reg_rd_val=0x%x\n",reg_rd_val);
  } while ((reg_rd_val & mask) != 0x2);

//  //deassert the priv ring reset
//  if (!(reg = m_regMap->FindRegister("LW_PPRIV_MASTER_RING_GLOBAL_CTL"))) {
//    ErrPrintf("PrivReInit::PrivReInitTest: register LW_PPRIV_MASTER_RING_GLOBAL_CTL is not defined!\n");
//    err ++;
//  }
//  if (!(field = reg->FindField("LW_PPRIV_MASTER_RING_GLOBAL_CTL_RING_RESET"))) {
//    ErrPrintf("PrivReInit::PrivReInitTest: field LW_PPRIV_MASTER_RING_GLOBAL_CTL_RING_RESET is not defined!\n");
//    err++;
//  }
//  if (!(value = field->FindValue("LW_PPRIV_MASTER_RING_GLOBAL_CTL_RING_RESET_DEASSERTED"))) {
//    ErrPrintf("PrivReInit::PrivReInitTest: value LW_PPRIV_MASTER_RING_GLOBAL_CTL_RING_RESET_DEASSERTED is not defined!\n");
//    err++;
//  }
//  addr = reg->GetAddress();
//  reg_rd_val = lwgpu->RegRd32(addr);
//  InfoPrintf("PrivReInit:PrivReInitTest: read LW_PPRIV_MASTER_RING_GLOBAL_CTL,reg_rd_val=0x%x\n",reg_rd_val);
//  mask = field->GetMask();
//  start_bit = field->GetStartBit();
//  reg_wr_val = (reg_rd_val | mask) & (~mask | (value->GetValue() << start_bit));
//  lwgpu->RegWr32(addr,reg_wr_val);
//  InfoPrintf("PrivReInit:PrivReInitTest: write LW_PPRIV_MASTER_RING_GLOBAL_CTL,reg_wr_val=0x%x\n",reg_wr_val);
//
//  //enumerate the ring
//  if (!(reg = m_regMap->FindRegister("LW_PPRIV_MASTER_RING_COMMAND"))) {
//    ErrPrintf("PrivReInit::PrivReInitTest: register LW_PPRIV_MASTER_RING_COMMAND is not defined!\n");
//    err ++;
//  }
//  if (!(field = reg->FindField("LW_PPRIV_MASTER_RING_COMMAND_CMD_ENUMERATE_STATIONS_BC_GRP"))) {
//    ErrPrintf("PrivReInit::PrivReInitTest: field LW_PPRIV_MASTER_RING_COMMAND_CMD_ENUMERATE_STATIONS_BC_GRP is not defined!\n");
//    err++;
//  }
//  if (!(value = field->FindValue("LW_PPRIV_MASTER_RING_COMMAND_CMD_ENUMERATE_STATIONS_BC_GRP_ALL"))) {
//    ErrPrintf("PrivReInit::PrivReInitTest: value LW_PPRIV_MASTER_RING_COMMAND_CMD_ENUMERATE_STATIONS_BC_GRP_ALL is not defined!\n");
//    err++;
//  }
//  start_bit = field->GetStartBit();
//  reg_wr_val = value->GetValue() << start_bit;
//  if (!(field = reg->FindField("LW_PPRIV_MASTER_RING_COMMAND_CMD"))) {
//    ErrPrintf("PrivReInit::PrivReInitTest: field LW_PPRIV_MASTER_RING_COMMAND_CMD is not defined!\n");
//    err++;
//  }
//  if (!(value = field->FindValue("LW_PPRIV_MASTER_RING_COMMAND_CMD_ENUMERATE_STATIONS"))) {
//    ErrPrintf("PrivReInit::PrivReInitTest: value LW_PPRIV_MASTER_RING_COMMAND_CMD_ENUMERATE_STATIONS is not defined!\n");
//    err++;
//  }
//  start_bit = field->GetStartBit();
//  mask = field->GetMask();
//  reg_wr_val = reg_wr_val | (value->GetValue() << start_bit);
//  addr = reg->GetAddress();
//  lwgpu->RegWr32(addr,reg_wr_val);
//  InfoPrintf("PrivReInit:PrivReInitTest: write LW_PPRIV_MASTER_RING_COMMAND,reg_wr_val=0x%x\n",reg_wr_val); 
//  //poll the ring status
//  do {
//    reg_rd_val = lwgpu->RegRd32(addr);
//    InfoPrintf("PrivReInit::PrivReInitTest: read LW_PPRIV_MASTER_RING_COMMAND,reg_rd_val=0x%x\n",reg_rd_val);
//  } while ((reg_rd_val & mask) != 0);
//  
//  //check the active cluster numbers.
//  //SYS
//  if (!(reg = m_regMap->FindRegister("LW_PPRIV_MASTER_RING_ENUMERATE_RESULTS_SYS"))) {
//    ErrPrintf("PrivReInit::PrivReInitTest: register LW_PPRIV_MASTER_RING_ENUMERATE_RESULTS_SYS is not defined!\n");
//    err ++;
//  }
//  if (!(field = reg->FindField("LW_PPRIV_MASTER_RING_ENUMERATE_RESULTS_SYS_COUNT"))) {
//    ErrPrintf("PrivReInit::PrivReInitTest: field LW_PPRIV_MASTER_RING_ENUMERATE_RESULTS_SYS_COUNT is not defined!\n");
//    err++;
//  }
//  if (!(value = field->FindValue("LW_PPRIV_MASTER_RING_ENUMERATE_RESULTS_SYS_COUNT_NONE"))) {
//    ErrPrintf("PrivReInit::PrivReInitTest: value LW_PPRIV_MASTER_RING_ENUMERATE_RESULTS_SYS_COUNT_NONE is not defined!\n");
//    err++;
//  }
//  addr = reg->GetAddress();
//  reg_rd_val = lwgpu->RegRd32(addr);
//  InfoPrintf("PrivReInit::PrivReInitTest: read LW_PPRIV_MASTER_RING_ENUMERATE_RESULTS_SYS,reg_rd_val=0x%x\n",reg_rd_val);
//  if (reg_rd_val == value->GetValue()) {
//    ErrPrintf("PrivReInit::PrivReInitTest: the SYS count is zero!\n");
//    err++;
//  }
//  //FBP
//  if (!(reg = m_regMap->FindRegister("LW_PPRIV_MASTER_RING_ENUMERATE_RESULTS_FBP"))) {
//    ErrPrintf("PrivReInit::PrivReInitTest: register LW_PPRIV_MASTER_RING_ENUMERATE_RESULTS_FBP is not defined!\n");
//    err ++;
//  }
//  if (!(field = reg->FindField("LW_PPRIV_MASTER_RING_ENUMERATE_RESULTS_FBP_COUNT"))) {
//    ErrPrintf("PrivReInit::PrivReInitTest: field LW_PPRIV_MASTER_RING_ENUMERATE_RESULTS_FBP_COUNT is not defined!\n");
//    err++;
//  }
//  if (!(value = field->FindValue("LW_PPRIV_MASTER_RING_ENUMERATE_RESULTS_FBP_COUNT_NONE"))) {
//    ErrPrintf("PrivReInit::PrivReInitTest: value LW_PPRIV_MASTER_RING_ENUMERATE_RESULTS_FBP_COUNT_NONE is not defined!\n");
//    err++;
//  }
//  addr = reg->GetAddress();
//  reg_rd_val = lwgpu->RegRd32(addr);
//  InfoPrintf("PrivReInit::PrivReInitTest: read LW_PPRIV_MASTER_RING_ENUMERATE_RESULTS_FBP,reg_rd_val=0x%x\n",reg_rd_val);
//  if (reg_rd_val == value->GetValue()) {
//    ErrPrintf("PrivReInit::PrivReInitTest: the FBP count is zero!\n");
//    err++;
//  }
//  //GPC
//  if (!(reg = m_regMap->FindRegister("LW_PPRIV_MASTER_RING_ENUMERATE_RESULTS_GPC"))) {
//    ErrPrintf("PrivReInit::PrivReInitTest: register LW_PPRIV_MASTER_RING_ENUMERATE_RESULTS_GPC is not defined!\n");
//    err ++;
//  }
//  if (!(field = reg->FindField("LW_PPRIV_MASTER_RING_ENUMERATE_RESULTS_GPC_COUNT"))) {
//    ErrPrintf("PrivReInit::PrivReInitTest: field LW_PPRIV_MASTER_RING_ENUMERATE_RESULTS_GPC_COUNT is not defined!\n");
//    err++;
//  }
//  if (!(value = field->FindValue("LW_PPRIV_MASTER_RING_ENUMERATE_RESULTS_GPC_COUNT_NONE"))) {
//    ErrPrintf("PrivReInit::PrivReInitTest: value LW_PPRIV_MASTER_RING_ENUMERATE_RESULTS_GPC_COUNT_NONE is not defined!\n");
//    err++;
//  }
//  addr = reg->GetAddress();
//  reg_rd_val = lwgpu->RegRd32(addr);
//  InfoPrintf("PrivReInit::PrivReInitTest: read LW_PPRIV_MASTER_RING_ENUMERATE_RESULTS_GPC,reg_rd_val=0x%x\n",reg_rd_val);
//  if (reg_rd_val == value->GetValue()) {
//    ErrPrintf("PrivReInit::PrivReInitTest: the GPC count is zero!\n");
//    err++;
//  }
//
//  //start priv ring
//  reg = m_regMap->FindRegister("LW_PPRIV_MASTER_RING_COMMAND");
//  field = reg->FindField("LW_PPRIV_MASTER_RING_COMMAND_CMD");
//  if (!(value = field->FindValue("LW_PPRIV_MASTER_RING_COMMAND_CMD_START_RING"))) {
//    ErrPrintf("PrivReInit::PrivReInitTest: value LW_PPRIV_MASTER_RING_COMMAND_CMD_START_RING is not defined!\n");
//    err++;
//  }
//  reg_wr_val = (reg_rd_val | mask) & (~mask | (value->GetValue() << start_bit));
//  addr = reg->GetAddress();
//  lwgpu->RegWr32(addr,reg_wr_val);
//  InfoPrintf("PrivReInit:PrivReInitTest: write LW_PPRIV_MASTER_RING_COMMAND,reg_wr_val=0x%x\n",reg_wr_val); 
//
//  //wait for ring start
//  reg = m_regMap->FindRegister("LW_PPRIV_SYS_PRIV_DECODE_CONFIG");
//  field = reg->FindField("LW_PPRIV_SYS_PRIV_DECODE_CONFIG_RING");
//  if (!(value = field->FindValue("LW_PPRIV_SYS_PRIV_DECODE_CONFIG_RING_WAIT_FOR_RING_START_COMPLETE"))) {
//    ErrPrintf("PrivReInit::PrivReInitTest: value LW_PPRIV_SYS_PRIV_DECODE_CONFIG_RING_WAIT_FOR_RING_START_COMPLETE is not defined!\n");
//    err++;
//  }
//  addr = reg->GetAddress();
//  reg_rd_val = lwgpu->RegRd32(addr);
//  InfoPrintf("PrivReInit::PrivReInitTest: read LW_PPRIV_SYS_PRIV_DECODE_CONFIG,reg_rd_val=0x%x\n",reg_rd_val);
//  mask = field->GetMask();
//  start_bit = field->GetStartBit();
//  reg_wr_val = (reg_rd_val | mask) & (~mask | (value->GetValue() << start_bit));
//  lwgpu->RegWr32(addr,reg_wr_val);
//  InfoPrintf("PrivReInit:PrivReInitTest: write LW_PPRIV_SYS_PRIV_DECODE_CONFIG,reg_wr_val=0x%x\n",reg_wr_val); 

  //config a GPC pri reg
  //if (!(reg = m_regMap->FindRegister("LW_PGRAPH_PRI_GPC0_TPC0_PE_VAF"))) {
  //  ErrPrintf("PrivReInit::PrivReInitTest: register LW_PGRAPH_PRI_GPC0_TPC0_PE_VAF is not defined!\n");
  //  err ++;
  //}
  //addr = reg->GetAddress();
  //mask = reg->GetReadMask();
  //reg_rd_val = lwgpu->RegRd32(addr);
  //InfoPrintf("PrivReInit::PrivReInitTest: read LW_PGRAPH_PRI_GPC0_TPC0_PE_VAF,reg_rd_val=0x%x\n",reg_rd_val);
  //reg_wr_val = ~reg_rd_val;
  //lwgpu->RegWr32(addr,reg_wr_val);
  //InfoPrintf("PrivReInit:PrivReInitTest: write LW_PGRAPH_PRI_GPC0_TPC0_PE_VAF,reg_wr_val=0x%x\n",reg_wr_val);
  //reg_rd_val = lwgpu->RegRd32(addr);
  //InfoPrintf("PrivReInit::PrivReInitTest: read LW_PGRAPH_PRI_GPC0_TPC0_PE_VAF,reg_rd_val=0x%x\n",reg_rd_val);
  //reg_rd_val_masked = reg_rd_val & mask;
  //reg_wr_val_masked = reg_wr_val & mask;
  //if (reg_rd_val_masked != reg_wr_val_masked) {
  //  ErrPrintf("PrivReInit::PrivReInitTest: LW_PGRAPH_PRI_GPC0_TPC0_PE_VAF field read data is 0x%x,expected 0x%x\n",reg_rd_val_masked,reg_wr_val_masked);
  //  err++;
  //}
  ////config a FBP pri reg
  //if (!(reg = m_regMap->FindRegister("LW_PGRAPH_PRI_BE0_CROP_IEEE_CLEAN"))) {
  //  ErrPrintf("PrivReInit::PrivReInitTest: register LW_PGRAPH_PRI_BE0_CROP_IEEE_CLEAN is not defined!\n");
  //  err ++;
  //}
  //addr = reg->GetAddress();
  //mask = reg->GetReadMask();
  //reg_rd_val = lwgpu->RegRd32(addr);
  //InfoPrintf("PrivReInit::PrivReInitTest: read LW_PGRAPH_PRI_BE0_CROP_IEEE_CLEAN,reg_rd_val=0x%x\n",reg_rd_val);
  //reg_wr_val = ~reg_rd_val;
  //lwgpu->RegWr32(addr,reg_wr_val);
  //InfoPrintf("PrivReInit:PrivReInitTest: write LW_PGRAPH_PRI_BE0_CROP_IEEE_CLEAN,reg_wr_val=0x%x\n",reg_wr_val);
  //reg_rd_val = lwgpu->RegRd32(addr);
  //InfoPrintf("PrivReInit::PrivReInitTest: read LW_PGRAPH_PRI_BE0_CROP_IEEE_CLEAN,reg_rd_val=0x%x\n",reg_rd_val);
  //reg_rd_val_masked = reg_rd_val & mask;
  //reg_wr_val_masked = reg_wr_val & mask;
  //if (reg_rd_val_masked != reg_wr_val_masked) {
  //  ErrPrintf("PrivReInit::PrivReInitTest: LW_PGRAPH_PRI_BE0_CROP_IEEE_CLEAN field read data is 0x%x,expected 0x%x\n",reg_rd_val_masked,reg_wr_val_masked);
  //  err++;
  //}

  //config a XBAR pri reg
  //if(!skip_mxbar_gnic_reg_access) {
  //  if((arch != 0x708) && (arch != 0x707)) {
  //      if (!(reg = m_regMap->FindRegister("LW_XBAR_MXBAR_CS_PRI_GPC0_GXI_PREG_CTRL"))) {
  //              ErrPrintf("\nPrivReInit::PrivReInitTest: register LW_XBAR_MXBAR_CS_PRI_GPC0_GXI_PREG_CTRL is not defined!\n");
  //              err ++;
  //      }
  //      addr = reg->GetAddress();
  //      mask = reg->GetReadMask();
  //      reg_rd_val = lwgpu->RegRd32(addr);
  //      InfoPrintf("PrivReInit::PrivReInitTest: read LW_XBAR_MXBAR_CS_PRI_GPC0_GXI_PREG_CTRL,reg_rd_val=0x%x\n",reg_rd_val);
  //      reg_wr_val = ~reg_rd_val;
  //      lwgpu->RegWr32(addr,reg_wr_val);
  //      InfoPrintf("PrivReInit:PrivReInitTest: write LW_XBAR_MXBAR_CS_PRI_GPC0_GXI_PREG_CTRL,reg_wr_val=0x%x\n",reg_wr_val);
  //      reg_rd_val = lwgpu->RegRd32(addr);
  //      InfoPrintf("PrivReInit::PrivReInitTest: read LW_XBAR_MXBAR_CS_PRI_GPC0_GXI_PREG_CTRL,reg_rd_val=0x%x\n",reg_rd_val);
  //      reg_rd_val_masked = reg_rd_val & mask;
  //      reg_wr_val_masked = reg_wr_val & mask;
  //      if (reg_rd_val_masked != reg_wr_val_masked) {
  //              ErrPrintf("PrivReInit::PrivReInitTest: LW_XBAR_MXBAR_CS_PRI_GPC0_GXI_PREG_CTRL field read data is 0x%x,expected 0x%x\n",reg_rd_val_masked,reg_wr_val_masked);
  //      err++;
  //      }
  //  }
 // }
}

UINT32 Elpg_Powergv_Kepler::check_clamp_sleep_gr(bool fuse_blown) {
    UINT32 sleep_to_0_gx, sleep_to_1_gx, clamp_to_0_gx, clamp_to_1_gx;

    if(fuse_blown) {
        Platform::EscapeRead("sleep_to_0_gx", 0, 1, &sleep_to_0_gx);
        Platform::EscapeRead("clamp_to_1_gx", 0, 1, &clamp_to_1_gx);
        if((sleep_to_0_gx == 0) & (clamp_to_1_gx == 1)) {
            InfoPrintf("clamp_en and sleep signal are forced by opt_pgob rightly \n");
        }

        if(sleep_to_0_gx != 0) {
            ErrPrintf("sleep signal they are not forced by opt_pgob\n"); return 1;
        }

        if(clamp_to_1_gx != 1) {
            ErrPrintf("clamp_en they are not forced by opt_pgob\n"); return 1;
        }
    }
    else {
        Platform::EscapeRead("sleep_to_1_gx", 0, 1, &sleep_to_1_gx);
        Platform::EscapeRead("clamp_to_0_gx", 0, 1, &clamp_to_0_gx);
        if((sleep_to_1_gx == 1) & (clamp_to_0_gx == 0)) {
            InfoPrintf("clamp_en and sleep signal are released rightly when fuse is not blown (or overrided by CTRL_1_PGOB_OVERRIDE)\n");
        }

        if(sleep_to_1_gx != 1) {
            ErrPrintf("sleep signal they are not released when fuse is not blown (or overrided by CTRL_1_PGOB_OVERRIDE)\n"); return 1;
        }

        if(clamp_to_0_gx != 0) {
            ErrPrintf("clamp_en they are not released when fuse is not blown (or overrided by CTRL_1_PGOB_OVERRIDE)\n"); return 1;
        }
    }

    return 0;
}

UINT32 Elpg_Powergv_Kepler::check_clamp_sleep_vd(bool fuse_blown) {
    UINT32 sleep_to_0_vd, sleep_to_1_vd, clamp_to_0_vd, clamp_to_1_vd;

    if(fuse_blown) {
        Platform::EscapeRead("sleep_to_0_vd", 0, 1, &sleep_to_0_vd);
        Platform::EscapeRead("clamp_to_1_vd", 0, 1, &clamp_to_1_vd);
        if((sleep_to_0_vd == 0) & (clamp_to_1_vd == 1)) {
            InfoPrintf("clamp_en and sleep signal are forced by opt_pgob rightly \n");
        }

        if(sleep_to_0_vd != 0) {
            ErrPrintf("sleep signal they are not forced by opt_pgob\n"); return 1;
        }

        if(clamp_to_1_vd != 1) {
            ErrPrintf("clamp_en they are not forced by opt_pgob\n"); return 1;
        }
    }
    else {
        Platform::EscapeRead("sleep_to_1_vd", 0, 1, &sleep_to_1_vd);
        Platform::EscapeRead("clamp_to_0_vd", 0, 1, &clamp_to_0_vd);
        if((sleep_to_1_vd == 1) & (clamp_to_0_vd == 0)) {
            InfoPrintf("clamp_en and sleep signal are released rightly when fuse is not blown (or overrided by CTRL_1_PGOB_OVERRIDE)\n");
        }

        if(sleep_to_1_vd != 1) {
            ErrPrintf("sleep signal they are not released when fuse is not blown (or overrided by CTRL_1_PGOB_OVERRIDE)\n"); return 1;
        }

        if(clamp_to_0_vd != 0) {
            ErrPrintf("clamp_en they are not released when fuse is not blown (or overrided by CTRL_1_PGOB_OVERRIDE)\n"); return 1;
        }
    }

    return 0;
}

// Function to generate delay using dummy reads
void Elpg_Powergv_Kepler::DelayUsingDummyReads() {

    RdRegArray(0,"LW_PPWR_PMU_PG_SWCTRL");

}

