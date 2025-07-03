/*
 *  LWIDIA_COPYRIGHT_BEGIN
 *
 *  Copyright 2016-2021 by LWPU Corporation. All rights reserved. All
 *  information contained herein is proprietary and confidential to LWPU
 *  Corporation. Any use, reproduction, or disclosure without the written
 *  permission of LWPU Corporation is prohibited.
 *
 *  LWIDIA_COPYRIGHT_END
 */

#include "mdiag/tests/stdtest.h"

#include "check_connect.h"
#include "mdiag/lwgpu.h"
#include "mdiag/utils/lwgpu_channel.h"
#include "mdiag/tests/test_state_report.h"
#include "core/include/gpu.h"

static bool gp10b_connect = false;
static bool host2pmu_reset_connect = false;
static bool adc_connect = false;
static bool dwb_connect = false;
static bool sci2therm_xtal_gate_stretch_connect = false;
static bool disp_2pmu_intr_connect = false;
static bool fuse2pwr_debug_mode_connect = false;
static bool pmgr2pwr_ext_alert_connect = false;
static bool priv_block_connect = false;
static bool vid_pwm = false;
static bool misc_connect = false;
static bool hshub2x_connect = false;
static bool gpcadc_connect = false;
static bool xtl2therm_pci_connect = false;
extern const ParamDecl check_connect_params[] =
{
    SIMPLE_PARAM("-gp10b_connect", "add gp10b connectivity"),
    SIMPLE_PARAM("-host2pmu_reset_connect", "add host2pmu_reset connectivity"),
    SIMPLE_PARAM("-adc_connect",  "add adc_connect"),
	SIMPLE_PARAM("-gpcadc_connect", "add gpcadc_connect"),
    SIMPLE_PARAM("-xtl2therm_pci_connect", "add xtl2therm_pci_connect"),
    SIMPLE_PARAM("-dwb_connect",  "add dwb_connect"),
    SIMPLE_PARAM("-sci2therm_xtal_gate_stretch_connect",  "add sci2therm_xtal_gate_stretch_connect"),
    SIMPLE_PARAM("-disp_2pmu_intr_connect", "add disp_2pmu_intr_connect"),
    SIMPLE_PARAM("-fuse2pwr_debug_mode_connect", "add fuse2pwr_debug_mode_connect"),
    SIMPLE_PARAM("-pmgr2pwr_ext_alert_connect", "add pmgr2pwr_ext_alert_connect"),
    SIMPLE_PARAM("-priv_block_connect", "add priv_block_connect"),
    SIMPLE_PARAM("-vid_pwm", "add vid_pwm"),
    SIMPLE_PARAM("-misc_connect", "add misc_connect"),
    SIMPLE_PARAM("-hshub2x_connect", "add hshub2x_connect"),
    PARAM_SUBDECL(lwgpu_single_params),
    LAST_PARAM
};

CheckConnect::CheckConnect(ArgReader *params) :
  LWGpuSingleChannelTest(params)
{
  if(params->ParamPresent("-gp10b_connect"))  gp10b_connect = 1;
  if(params->ParamPresent("-host2pmu_reset_connect"))  host2pmu_reset_connect = 1;
  if(params->ParamPresent("-adc_connect"))  adc_connect = 1;
  if(params->ParamPresent("-gpcadc_connect"))  gpcadc_connect = 1;
  if(params->ParamPresent("-xtl2therm_pci_connect"))  xtl2therm_pci_connect = 1;
  if(params->ParamPresent("-dwb_connect"))  dwb_connect = 1;
  if(params->ParamPresent("-sci2therm_xtal_gate_stretch_connect"))  sci2therm_xtal_gate_stretch_connect = 1;
  if(params->ParamPresent("-disp_2pmu_intr_connect")) disp_2pmu_intr_connect = 1;
  if(params->ParamPresent("-fuse2pwr_debug_mode_connect")) fuse2pwr_debug_mode_connect = 1;
  if(params->ParamPresent("-pmgr2pwr_ext_alert_connect")) pmgr2pwr_ext_alert_connect = 1;
  if(params->ParamPresent("-priv_block_connect")) priv_block_connect = 1;
  if(params->ParamPresent("-vid_pwm")) vid_pwm = 1;
  if(params->ParamPresent("-misc_connect")) misc_connect = 1;
  if(params->ParamPresent("-hshub2x_connect")) hshub2x_connect = 1;
}

CheckConnect::~CheckConnect(void)
{
}

STD_TEST_FACTORY(check_connect, CheckConnect);

int
CheckConnect::Setup(void)
{
    lwgpu = LWGpuResource::FindFirstResource();
    m_arch = lwgpu->GetArchitecture();
    InfoPrintf("m_arch is 0x%x.\n", m_arch);
    macros.MacroInit(m_arch);
    ch = lwgpu->CreateChannel();
    if(!ch)
    {
        ErrPrintf("Connect:  Setup failed to create channel\n");
        return (0);
    }

    m_regMap = lwgpu->GetRegisterMap();
    if(!m_regMap)
    {
        ErrPrintf("Connect:  Connect can only be run on GPU's that support a register map!\n");
        return (0);
    }

    getStateReport()->init("check_connect");
    getStateReport()->enable();

    return 1;
}

void
CheckConnect::CleanUp(void)
{
    if(ch)
    {
        delete ch;
        ch = 0;
    }

    if(lwgpu)
    {
        DebugPrintf("Connect::CleanUp(): Releasing GPU.\n");
        lwgpu = 0;
    }
    LWGpuSingleChannelTest::CleanUp();
}

void
CheckConnect::Run(void)
{
    //  InfoPrintf Flag to determine whether the test fail because of elw issue or test issue.
    InfoPrintf("FLAG : check_connect starting ....\n");

    SetStatus(TEST_INCOMPLETE);

    if(gp10b_connect)
    {
      InfoPrintf("CheckConnect: Running check_gp10b_connect\n");
      if(check_gp10b_connect())
      {
        SetStatus(TEST_FAILED);
        getStateReport()->runFailed("Failed check_gp10b_connect\n");
        ErrPrintf("CheckConnect::check_gp10b_connect test failed\n");
        return;
      }
    }

    if(host2pmu_reset_connect)
    {
      InfoPrintf("CheckConnect: Running check_host2pmu_reset_connect\n");
      if(check_host2pmu_reset_connect())
      {
        SetStatus(TEST_FAILED);
        getStateReport()->runFailed("Failed check_host2pmu_reset_connect\n");
        ErrPrintf("CheckConnect::check_host2pmu_reset_connect test failed\n");
        return;
      }
    }

    if(adc_connect)
    {
      InfoPrintf("CheckConnect: Running check_adc_connect\n");
      if(check_adc_connect())
      {
        SetStatus(TEST_FAILED);
        getStateReport()->runFailed("Failed check_adc_connect\n");
        ErrPrintf("CheckConnect::check_adc_connect test failed\n");
        return;
      }
    }

	if(gpcadc_connect)
    {
      InfoPrintf("CheckConnect: Running check_gpcadc_connect\n");
      if(check_gpcadc_connect())
      {
        SetStatus(TEST_FAILED);
        getStateReport()->runFailed("Failed check_gpcadc_connect\n");
        ErrPrintf("CheckConnect::check_gpcadc_connect test failed\n");
        return;
      }
    }

    if(xtl2therm_pci_connect)
    {
      InfoPrintf("CheckConnect: Running check_xtl2therm_pci_connect\n");
      if(check_xtl2therm_pci_connect())
      {
        SetStatus(TEST_FAILED);
        getStateReport()->runFailed("Failed check_xtl2therm_pci_connect\n");
        ErrPrintf("CheckConnect::check_xtl2therm_pci_connect test failed\n");
        return;
      }
    }

    if(dwb_connect)
    {
      InfoPrintf("CheckConnect: Running check_dwb_connect\n");
      if(check_dwb_connect())
      {
        SetStatus(TEST_FAILED);
        getStateReport()->runFailed("Failed check_dwb_connect test failed\n");
        ErrPrintf("CheckConnect::check_dwb_connect test failed\n");
        return;
      }
    }

    if(sci2therm_xtal_gate_stretch_connect)
    {
      InfoPrintf("CheckConnect: Running check_sci2therm_xtal_gate_stretch_connect\n");
      if(check_sci2therm_xtal_gate_stretch_connect())
      {
        SetStatus(TEST_FAILED);
        getStateReport()->runFailed("Failed check_sci2therm_xtal_gate_stretch_connect\n");
        ErrPrintf("CheckConnect::check_sci2therm_xtal_gate_stretch_connect\n");
        return;
      }
    }

    if(disp_2pmu_intr_connect)
    {
      InfoPrintf("CheckConnect: Running check_disp_2pmu_intr_connect\n");
      if(check_disp_2pmu_intr_connect())
      {
        SetStatus(TEST_FAILED);
        getStateReport()->runFailed("Failed check_disp_2pmu_intr_connect\n");
        ErrPrintf("CheckConnect::check_disp_2pmu_intr_connect\n");
        return;
      }
    }

    if(fuse2pwr_debug_mode_connect){
      
	  InfoPrintf("CheckConnect: Running check_fuse2pwr_debug_mode_connect\n");
      if(check_fuse2pwr_debug_mode_connect()){
          SetStatus(TEST_FAILED);
          getStateReport()->runFailed("Failed check_fuse2pwr_debug_mode_connect\n");
          ErrPrintf("CheckConnect::check_fuse2pwr_debug_mode_connect\n");
          return;
      }
    }
 
    if(pmgr2pwr_ext_alert_connect){
      InfoPrintf("CheckConnect: Running check_pmgr2pwr_ext_alert_connect\n");
      if(check_pmgr2pwr_ext_alert_connect()){
        SetStatus(TEST_FAILED);
        getStateReport()->runFailed("Failed check_pmgr2pwr_ext_alert_connect\n");
        ErrPrintf("CheckConnect::check_pmgr2pwr_ext_alert_connect\n");
        return;
      }
    }
 
    if(priv_block_connect){
      InfoPrintf("CheckConnect: Running check_priv_block_connect\n");
      if(check_priv_block_connect()){
        SetStatus(TEST_FAILED);
        getStateReport()->runFailed("Failed check_priv_block_connect\n");
        ErrPrintf("CheckConnect::check_priv_block_connect\n");
        return;
      }
    }

    if(vid_pwm){
      InfoPrintf("CheckConnect: Running check_vid_pwm\n");
      if(check_vid_pwm()){
        SetStatus(TEST_FAILED);
        getStateReport()->runFailed("Failed check_vid_pwm\n");
        ErrPrintf("CheckConnect::check_vid_pwm\n");
        return;
      }
    }

    if(misc_connect){
      InfoPrintf("CheckConnect: Running check_misc_connect\n");
      if(check_misc_connect()){
        SetStatus(TEST_FAILED);
        getStateReport()->runFailed("Failed check_misc_connect\n");
        ErrPrintf("CheckConnect::check_misc_connect\n");
        return;
      }
    }
    if(hshub2x_connect){
      InfoPrintf("CheckConnect: Running check_hshub2x_connect\n");
      if(check_hshub2x_connect()){
        SetStatus(TEST_FAILED);
        getStateReport()->runFailed("Failed check_hshub2x_connect\n");
        ErrPrintf("CheckConnect::check_hshub2x_connect\n");
        return;
      }
    }
    SetStatus(TEST_SUCCEEDED);
    getStateReport()->crcCheckPassedGold();

    InfoPrintf("connect test END.\n");
    return;
}

int CheckConnect::check_gp10b_connect() {
    UINT32 data = 0x0;

    InfoPrintf("Starting gp10b connectivity check\n");
    Platform::EscapeWrite("pwr2gr_gpc_partition14_sleep_0", 0, 1, 0);
    Platform::EscapeWrite("pwr2gr_gpc_partition14_sleep_7", 0, 1, 0);
    Platform::EscapeRead("gpu0_pwr_valid", 0, 1, &data);
    Platform::Delay(5);
    if (data != 1) { ErrPrintf("1 gpu0_pwr_valid error\n"); return (1); }

    Platform::EscapeWrite("pwr2gr_gpc_partition14_sleep_0", 0, 1, 1);
    Platform::EscapeWrite("pwr2gr_gpc_partition14_sleep_7", 0, 1, 1);
    Platform::EscapeRead("gpu0_pwr_valid", 0, 1, &data);
    Platform::Delay(5);
    if (data != 0) { ErrPrintf("2 gpu0_pwr_valid error\n"); return (1); }

    Platform::EscapeWrite("pwr2gr_gpc_partition14_sleep_0", 0, 1, 1);
    Platform::EscapeWrite("pwr2gr_gpc_partition14_sleep_7", 0, 1, 0);
    Platform::EscapeRead("gpu0_pwr_valid", 0, 1, &data);
    Platform::Delay(5);
    if (data != 0) { ErrPrintf("3 gpu0_pwr_valid error\n"); return (1); }

    Platform::EscapeWrite("pwr2gr_gpc_partition14_sleep_0", 0, 1, 0);
    Platform::EscapeWrite("pwr2gr_gpc_partition14_sleep_7", 0, 1, 1);
    Platform::EscapeRead("gpu0_pwr_valid", 0, 1, &data);
    Platform::Delay(5);
    if (data != 0) { ErrPrintf("4 gpu0_pwr_valid error\n"); return (1); }

    Platform::EscapeWrite("pwr2gr_gpc_partition21_sleep_0", 0, 1, 0);
    Platform::EscapeWrite("pwr2gr_gpc_partition21_sleep_7", 0, 1, 0);
    Platform::EscapeRead("gpu1_pwr_valid", 0, 1, &data);
    Platform::Delay(5);
    if (data != 1) { ErrPrintf("1 gpu1_pwr_valid error\n"); return (1); }

    Platform::EscapeWrite("pwr2gr_gpc_partition21_sleep_0", 0, 1, 1);
    Platform::EscapeWrite("pwr2gr_gpc_partition21_sleep_7", 0, 1, 1);
    Platform::EscapeRead("gpu1_pwr_valid", 0, 1, &data);
    Platform::Delay(5);
    if (data != 0) { ErrPrintf("2 gpu1_pwr_valid error\n"); return (1); }

    Platform::EscapeWrite("pwr2gr_gpc_partition21_sleep_0", 0, 1, 1);
    Platform::EscapeWrite("pwr2gr_gpc_partition21_sleep_7", 0, 1, 0);
    Platform::EscapeRead("gpu1_pwr_valid", 0, 1, &data);
    Platform::Delay(5);
    if (data != 0) { ErrPrintf("3 gpu1_pwr_valid error\n"); return (1); }

    Platform::EscapeWrite("pwr2gr_gpc_partition21_sleep_0", 0, 1, 0);
    Platform::EscapeWrite("pwr2gr_gpc_partition21_sleep_7", 0, 1, 1);
    Platform::EscapeRead("gpu1_pwr_valid", 0, 1, &data);
    Platform::Delay(5);
    if (data != 0) { ErrPrintf("4 gpu1_pwr_valid error\n"); return (1); }
    return (0);
}

/*
 * Last owner : Alan Zhai
 * Purpose    : check HOST to PMU RESET connection
 * Strategy   : 1) enbale perfmux otherwise there are some assersion errors
 *                 because perfmux is disable by default
 *              2) write 0 and read data, compare
 *              3) write 0 and read data, compare
 * Plz contact last owner if you miss new questions.
 */
int CheckConnect::check_host2pmu_reset_connect(){
    UINT32 data = 0x0;
    UINT32 err_cnt = 0;

    InfoPrintf("Starting host2pmu_reset connect check\n");

    // step1 : enable perfmux by writing 1 to LW_PPWR_PM_CTRL_ENABLE
    lwgpu->GetRegFldNum("LW_PPWR_PMU_PM", "_CTRL_ENABLE", &data);
    InfoPrintf("LW_PPWR_PM_CTRL_ENABLE is 0x%x", data);
    lwgpu->SetRegFldDef("LW_PPWR_PMU_PM", "_CTRL_ENABLE", "_ENABLED");

    // step2: write 0 to PMU_ENG_reset_reset_ and then check the value
    Platform::EscapeWrite("PMU_ENG_reset_reset_", 0, 1, 0);
    Platform::Delay(10);


  //if (macros.lw_gpu_core_ip) {
  //  Platform::EscapeRead("host2pwr_reset_", 0, 1, &data);
  //  if ((data & 0x1) != 0) { ErrPrintf("Error. case 2.\n"); err_cnt++; }
  //}

    Platform::EscapeRead("PMU_ENG_reset_reset_fecs_", 0, 1, &data);
    if ((data & 0x1) != 0) { ErrPrintf("Error. case 3.\n"); err_cnt++; }

    InfoPrintf("End check PMU_ENG_reset_reset_ = 0\n");
    Platform::Delay(10);

    // step3: write 1 to PMU_ENG_reset_reset_ and then check the value
    Platform::EscapeWrite("PMU_ENG_reset_reset_", 0, 1, 1);
    Platform::Delay(10);


  //if (macros.lw_gpu_core_ip) {
  //  Platform::EscapeRead("host2pwr_reset_", 0, 1, &data);
  //  if ((data & 0x1) != 1) { ErrPrintf("Error. case 5.\n"); err_cnt++; }
  //}

    Platform::EscapeRead("PMU_ENG_reset_reset_fecs_", 0, 1, &data);
    if ((data & 0x1) != 1) { ErrPrintf("Error. case 6.\n"); err_cnt++; }

    InfoPrintf("End check PMU_ENG_reset_reset_ = 1\n");

    InfoPrintf("Error Number is %d\n", err_cnt);
    InfoPrintf("End reset connect check\n");
    return err_cnt;
}

//connection check for adc
int CheckConnect::check_adc_connect()
{
     UINT32 data = 0x0;
     InfoPrintf("Starting adc connectivity check\n");
//
     Platform::EscapeWrite("clk2pwr_msvdd_adc", 0, 7, 0x7f);
     Platform::Delay(5);
     Platform::EscapeRead("clk2pwr_msvdd_adc_pwr", 0, 7, &data);
     InfoPrintf("data is 0x%x\n", data);
     if((data & 0x7f) != 0x7f) {
       ErrPrintf("case 1: adc connection error!\n");
       return (1);
     }

     Platform::EscapeWrite("clk2pwr_msvdd_adc", 0, 7, 0x0);
     Platform::Delay(5);
     Platform::EscapeRead("clk2pwr_msvdd_adc_pwr", 0, 7, &data);
     InfoPrintf("data is 0x%x\n", data);
     if((data & 0x7f) != 0x0)
     {
       ErrPrintf("case 0: adc connection error!\n");
       return (1);
     }
     InfoPrintf("End adc connect check\n");
     return (0);
}

//connection check for lwvdd_adc
int CheckConnect::check_gpcadc_connect()
{
     UINT32 wdata = 0x0;
     UINT32 rdata = 0x0;
	 UINT32 i =0;
     UINT32 fs_disable;
     char tempfs[40];
	 char tempwpd[40];
	 char tempwvld[40];
	 char temprpd[40];
	 char temprvld[40];
     char tempwack[40];
     char temprack[40];
     InfoPrintf("Starting gpc adc connectivity check\n");
	for(i=0;i< macros.lw_scal_litter_num_gpcs ;i++){
        //step1: check the floorsweep signal
        sprintf(tempfs,"fs2gpc%d_gpc_disable",i);
        Platform::EscapeRead(tempfs, 0, 1, &fs_disable);
        InfoPrintf("fs2gpc%d_gpc_disable is 0x%x\n", i, fs_disable);
        // Skip if GPC was floorswept
        if(fs_disable!=0){
            continue;
        }
        //step2: check the connection between CLK and GPCPWR
		InfoPrintf("Starting gpc adc connectivity check between CLK and GPCPWR\n");
        wdata = rand() & 0x7f;
		sprintf(tempwpd,"clk2gpc%d_lwvdd_adc_pd",i);
		sprintf(tempwvld,"clk2gpc%d_lwvdd_adc_vld",i);
		sprintf(temprpd,"clk2gpc%d_lwvdd_adc_pd_gpc",i);
		sprintf(temprvld,"clk2gpc%d_lwvdd_adc_vld_gpc",i);
		//check pd with random wdata
		Platform::EscapeWrite(tempwpd, 0, 7, wdata);
		Platform::Delay(5);
		Platform::EscapeRead(temprpd, 0, 7, &rdata);
		InfoPrintf("data is 0x%x\n", rdata);
		if((rdata & 0x7f) != wdata) {
			ErrPrintf("case 1: adc_pd connection error!\n");
		return (1);
		}
		//check vld with 1
		Platform::EscapeWrite(tempwvld, 0, 1, 1);
		Platform::Delay(5);
		Platform::EscapeRead(temprvld, 0, 1, &rdata);
		InfoPrintf("data is 0x%x\n", rdata);
		if((rdata & 0x1) != 1) {
			ErrPrintf("case 1: adc_vld connection error!\n");
		return (1);
		}
		//check pd with ilwerted wdata
		wdata = ~wdata &0x7f;
		Platform::EscapeWrite(tempwpd, 0, 7, wdata);
		Platform::Delay(5);
		Platform::EscapeRead(temprpd, 0, 7, &rdata);
		InfoPrintf("data is 0x%x\n", rdata);
		if((rdata & 0x7f) != wdata) {
			ErrPrintf("case 0: adc_pd connection error!\n");
		return (1);
		}
		//check vld with 0
		Platform::EscapeWrite(tempwvld, 0, 1, 0);
		Platform::Delay(5);
		Platform::EscapeRead(temprvld, 0, 1, &rdata);
		InfoPrintf("data is 0x%x\n", rdata);
		if((rdata & 0x1) != 0) {
			ErrPrintf("case 0: adc_vld connection error!\n");
		return (1);
		}
    }
    for(i=0;i< macros.lw_scal_litter_num_gpcs ;i++){
        //step3: check the floorsweep signal
        sprintf(tempfs,"fs2gpc%d_gpc_disable",i);
        Platform::EscapeRead(tempfs, 0, 1, &fs_disable);
        InfoPrintf("fs2gpc%d_gpc_disable is 0x%x\n", i, fs_disable);
        // Skip if GPC was floorswept
        if(fs_disable!=0){
            continue;
        }
        //step4: check the connection between GPCPWR and PWR
		InfoPrintf("Starting gpc adc connectivity check between GPCPWR and PWR\n");
        wdata = rand() & 0x7f;
		sprintf(tempwpd,"gpcpwr2pwr_lwvdd_adc_gpc%d_pd",i);
		sprintf(tempwvld,"gpcpwr2pwr_lwvdd_adc_gpc%d_req",i);
        sprintf(tempwack,"pwr2gpcpwr_lwvdd_adc_gpc%d_ack",i);
        sprintf(temprack,"pwr2gpcpwr_lwvdd_adc_gpc%d_ack_gpc",i);
		sprintf(temprpd,"gpcpwr2pwr_lwvdd_adc_gpc%d_pd_pwr",i);
		sprintf(temprvld,"gpcpwr2pwr_lwvdd_adc_gpc%d_req_pwr",i);
		//check req with 1
		Platform::EscapeWrite(tempwvld, 0, 1, 1);
		Platform::Delay(5);
		Platform::EscapeRead(temprvld, 0, 1, &rdata);
		InfoPrintf("data is 0x%x\n", rdata);
		if((rdata & 0x1) != 1) {
			ErrPrintf("case 1: gpcpwr2pwr_lwvdd_adc_gpc%d_req connection error!\n",i);
		return (1);
		}
		//check req with 0
		Platform::EscapeWrite(tempwvld, 0, 1, 0);
		Platform::Delay(5);
		Platform::EscapeRead(temprvld, 0, 1, &rdata);
		InfoPrintf("data is 0x%x\n", rdata);
		if((rdata & 0x1) != 0) {
			ErrPrintf("case 0: gpcpwr2pwr_lwvdd_adc_gpc%d_req connection error!\n",i);
		return (1);
		}
		//check pd with random wdata
		Platform::EscapeWrite(tempwpd, 0, 7, wdata);
		Platform::Delay(5);
		Platform::EscapeRead(temprpd, 0, 7, &rdata);
		InfoPrintf("data is 0x%x\n", rdata);
		if((rdata & 0x7f) != wdata) {
			ErrPrintf("case 1: gpcpwr2pwr_lwvdd_adc_gpc%d_pd connection error!\n",i);
		return (1);
		}
		//check pd with ilwerted wdata
		wdata = ~wdata &0x7f;
		Platform::EscapeWrite(tempwpd, 0, 7, wdata);
		Platform::Delay(5);
		Platform::EscapeRead(temprpd, 0, 7, &rdata);
		InfoPrintf("data is 0x%x\n", rdata);
		if((rdata & 0x7f) != wdata) {
			ErrPrintf("case 0: gpcpwr2pwr_lwvdd_adc_gpc%d_pd connection error!\n",i);
		return (1);
		}
        //check ack with 1
		Platform::EscapeWrite(tempwack, 0, 1, 1);
		Platform::Delay(5);
		Platform::EscapeRead(temprack, 0, 1, &rdata);
		InfoPrintf("data is 0x%x\n", rdata);
		if((rdata & 0x1) != 1) {
			ErrPrintf("case 1: pwr2gpcpwr_lwvdd_adc_gpc%d_ack connection error!\n",i);
		return (1);
		}
		//check ack with 0
		Platform::EscapeWrite(tempwack, 0, 1, 0);
		Platform::Delay(5);
		Platform::EscapeRead(temprack, 0, 1, &rdata);
		InfoPrintf("data is 0x%x\n", rdata);
		if((rdata & 0x1) != 0) {
			ErrPrintf("case 0: pwr2gpcpwr_lwvdd_adc_gpc%d_ack connection error!\n",i);
		return (1);
		}


	}

     	InfoPrintf("End gpc adc connect check\n");
     	return (0);
}

//[PCIE <-> PWR] Added and connected "xtl2therm_pci_fib/mask_rev" signals https://lwbugs/200629097
int CheckConnect::check_xtl2therm_pci_connect()
{
    UINT32 wdata = 0x0;
    UINT32 rdata = 0x0;
    InfoPrintf("Starting xtl2therm_pci connectivity check\n");

    //check xtl2therm_pci_fib_rev with random wdata
    wdata = rand() & 0xf;
	Platform::EscapeWrite("xtl2therm_pci_fib_rev_xtl", 0, 4, wdata);
	Platform::Delay(5);
	Platform::EscapeRead("xtl2therm_pci_fib_rev_pwr", 0, 4, &rdata);
	InfoPrintf("data is 0x%x\n", rdata);
	if((rdata & 0xf) != wdata) {
		ErrPrintf("case 1: xtl2therm_pci_fib_rev connection has error!\n");
	return (1);
	}
    //check xtl2therm_pci_fib_rev with ilwerted wdata
	wdata = ~wdata &0xf;
    Platform::EscapeWrite("xtl2therm_pci_fib_rev_xtl", 0, 4, wdata);
	Platform::Delay(5);
	Platform::EscapeRead("xtl2therm_pci_fib_rev_pwr", 0, 4, &rdata);
	InfoPrintf("data is 0x%x\n", rdata);
	if((rdata & 0xf) != wdata) {
		ErrPrintf("case 2: xtl2therm_pci_fib_rev connection has error!\n");
	return (1);
	}


    //check xtl2therm_pci_mask_rev with random wdata
    wdata = rand() & 0xf;
	Platform::EscapeWrite("xtl2therm_pci_mask_rev_xtl", 0, 4, wdata);
	Platform::Delay(5);
	Platform::EscapeRead("xtl2therm_pci_mask_rev_pwr", 0, 4, &rdata);
	InfoPrintf("data is 0x%x\n", rdata);
	if((rdata & 0xf) != wdata) {
		ErrPrintf("case 3: xtl2therm_pci_mask_rev connection has error!\n");
	return (1);
	}
    //check xtl2therm_pci_mask_rev with ilwerted wdata
	wdata = ~wdata &0xf;
    Platform::EscapeWrite("xtl2therm_pci_mask_rev_xtl", 0, 4, wdata);
	Platform::Delay(5);
	Platform::EscapeRead("xtl2therm_pci_mask_rev_pwr", 0, 4, &rdata);
	InfoPrintf("data is 0x%x\n", rdata);
	if((rdata & 0xf) != wdata) {
		ErrPrintf("case 4: xtl2therm_pci_mask_rev connection has error!\n");
	return (1);
	}

    InfoPrintf("End xtl2therm_pci_connect connect check\n");
    return (0);
}

//connection check for dwb
//dwb is not in project gv100, so it is not included in level
//if dwb in other project, just add parameter in level
int CheckConnect::check_dwb_connect()
{
     UINT32 data = 0x0;
     InfoPrintf("Starting dwb connectivity check\n");
//
     Platform::EscapeWrite("disp2pmu_dwb_mspg_ok", 0, 1, 1);
     Platform::Delay(5);
     Platform::EscapeRead("disp2pmu_dwb_mspg_ok_pwr", 0, 1, &data);
     if(data != 0x1) {
       ErrPrintf("000 dwb connection error!\n");
       return (1);
     }
     Platform::EscapeWrite("disp2pmu_dwb_mspg_ok", 0, 1, 0);
     Platform::Delay(5);
     Platform::EscapeRead("disp2pmu_dwb_mspg_ok_pwr", 0, 1, &data);
     if(data != 0x0) {
       ErrPrintf("111 dwb connection error!\n");
       return (1);
     }
     InfoPrintf("End dwb connect check\n");
     return (0);
}

//connection check for sci2therm_xtal_gate_stretch
int CheckConnect::check_sci2therm_xtal_gate_stretch_connect()
{
     UINT32 data = 0x0;
     InfoPrintf("Starting sci2therm_xtal_gate_stretch_connect check.\n");
     InfoPrintf("data is 0x%x.\n", data);
//1
     Platform::EscapeWrite("sci2therm_xtal_gate_stretch", 0, 1, 0);
     InfoPrintf("write 0\n");
     Platform::Delay(5);
     lwgpu->GetRegFldNum("LW_THERM_SENSOR_6", "", &data);
     InfoPrintf("data is 0x%x.\n", data);
     if( (data & 0x4) != 0x0 ) {
       ErrPrintf("000 sci2therm_xtal_gate_stretch error!\n");
       return (1);
     }

     Platform::Delay(5);
//0
     Platform::EscapeWrite("sci2therm_xtal_gate_stretch", 0, 1, 1);
     Platform::Delay(10);
     lwgpu->GetRegFldNum("LW_THERM_SENSOR_6", "", &data);
     InfoPrintf("write 1\n");
     InfoPrintf("data is 0x%x.\n", data);
     if( (data & 0x4) != 0x4 ){
       ErrPrintf("111 sci2therm_xtal_gate_stretch error!\n");
       return (1);
     }

     InfoPrintf("End sci2therm_xtal_gate_stretch check\n");
     return(0);
}

//check disp_2pmu_intr connection
int CheckConnect::check_disp_2pmu_intr_connect()
{
  InfoPrintf("Starting disp_2pmu_intr_connect check...\n");

  UINT32 data = 0x0;
  UINT32 errCnt = 0x0;
  int try_count = 0;

  //================Generate disp_2pmu_intr signal=================
  //step1:  Setting values to disp registers.
  //        The registers are provided by disp team.
  //        If there are some changes from display team, please ask disp for more info.
//  if(lwgpu->SetRegFldNum("LW_PDISP_FE_EVT_EN_SET_CTRL_DISP", "_SW_GENERIC_A", 1))
//    {
//      ErrPrintf("LW_PDISP_FE_EVT_EN_SET_CTRL_DISP_SW_GENERIC_A is not enable, which is not expected.\n");
//      errCnt++;
//    }
//  Platform::Delay(1);
//
//  if(lwgpu->SetRegFldNum("LW_PDISP_FE_PMU_INTR_MSK_CTRL_DISP","_SW_GENERIC_A", 1))
//    {
//      ErrPrintf("LW_PDISP_FE_PMU_INTR_MSK_CTRL_DISP_SW_GENERIC_A is not enable, which is not expected.\n");
//      errCnt++;
//    }
//  Platform::Delay(1);
//
// //Note: configuration below will trigger a pulse. It can't be written in next cycle but the pulse will show in the waveform.
//  if(lwgpu->SetRegFldNum("LW_PDISP_FE_EVT_FORCE_CTRL_DISP", "_SW_GENERIC_A", 1))
//    {
//      ErrPrintf("LW_PDISP_FE_EVT_FORCE_CTRL_DISP_SW_GENERIC_A is not enable, which is not expected.\n");
//      errCnt++;
//    }
//  Platform::Delay(1);
//
//  InfoPrintf("All the registers have been set to status enable\n");
  Platform::EscapeWrite("disp2pmu_intr", 0, 1, 1);
  Platform::Delay(10);
  //step2:  check status of disp_2pmu_intr
  data = 0x0;
  try_count = 0;

  lwgpu->GetRegFldNum("LW_PPWR_PMU_GPIO_INPUT", "", &data);
  InfoPrintf("LW_PPWR_PMU_GPIO_INPUT is 0x%x.\n", data);

  while(((data & 0x40000000) != 0x40000000) && (try_count < 50))
  {
    Platform::Delay(100);
    lwgpu->GetRegFldNum("LW_PPWR_PMU_GPIO_INPUT", "", &data);
    InfoPrintf("LW_PPWR_PMU_GPIO_INPUT is 0x%x, try_count is %d.\n", data, try_count);
    try_count++;
  }

  if((data & 0x40000000) != 0x40000000)
    {
      ErrPrintf("The signal disp_2pmu_intr has not been triggered, which is not expected.\n");
      errCnt++;
    }
  else
    {
      InfoPrintf("The signal disp_2pmu_intr has been triggered, which is expected.\n");
    }

  //step3:  clear the interrupt
  Platform::EscapeWrite("disp2pmu_intr", 0, 1, 0);
  Platform::Delay(10);
//  lwgpu->GetRegFldNum("LW_PDISP_FE_EVT_STAT_CTRL_DISP", "", &data);
//  InfoPrintf("data is 0x%x.\n", data);
//  if((data & 0x10) != 0x10)
//    {
//      InfoPrintf("The interrupt has not been cleared.\n");
//    }
//  else
//    {
//      InfoPrintf("The interrupt has been cleared.\n");
//    }
//
//  data = 0x0;
//  if(lwgpu->SetRegFldNum("LW_PDISP_FE_EVT_STAT_CTRL_DISP", "_SW_GENERIC_A", 1))
//    {
//      ErrPrintf("The interrupt has not been cleared, which is not expected.\n");
//      errCnt++;
//    }

  data = 0x0;
  try_count = 0;

  lwgpu->GetRegFldNum("LW_PPWR_PMU_GPIO_INPUT", "", &data);
  InfoPrintf("LW_PPWR_PMU_GPIO_INPUT is 0x%x.\n", data);
  while(((data & 0x40000000) == 0x40000000) && (try_count < 50))
    {
      // It takes a long time to trigger DISP_2PMU_INTR.
      Platform::Delay(100);
      lwgpu->GetRegFldNum("LW_PPWR_PMU_GPIO_INPUT", "", &data);
      InfoPrintf("LW_PPWR_PMU_GPIO_INPUT is 0x%x, try_count is %d.\n", data, try_count);
      try_count++;
    }

  if((data & 0x40000000) == 0x40000000)
    {
      ErrPrintf("The signal disp_2pmu_intr has been triggered, which is not expected.\n");
      errCnt++;
    }
  else
    {
      InfoPrintf("The signal disp_2pmu_intr has not been triggered, which is expected.\n");
    }

  InfoPrintf("End of check_disp_2pmu_intr_connect\n");
  return errCnt;
}

// FIXME bug 20044880
//  below diagram is logic, which should be verified in iGPU.
//  However, iGPU and dGPU have different instance names so systop_misc_pwr.vx force signal at the top level.
//               (force signal at the top level)
//  +----------------------+  |  +-----------------------------------------------------------------------------------------------------------+
//  |                 fuse |  |  | pwr                      +-----------------------------------------------------------------------------+  |
//  |                      |  |  |                          | pmu                                                                         |  |
//  |  opt_priv_sec_en  --->--|-->--- fs2all_priv_sec_en  -->                                                                             |  |
//  |                      |  |  |                          |   fuse2pwr_debug_mode_real = fs2all_priv_sec_en ? fuse2pwr_debug_mode : 1;  |  |
//  |  opt_sec_debug_en --->--|-->--- fuse2pwr_debug_mode -->                                                                             |  |
//  |                      |  |  |                          |                                                                             |  |
//  |                      |  |  |                          +-----------------------------------------------------------------------------+  |
//  +----------------------+  |  +-----------------------------------------------------------------------------------------------------------+
//
int CheckConnect::check_fuse2pwr_debug_mode_connect(){

  UINT32  data    = 0x0;
  UINT32  errCnt  = 0x0;

  InfoPrintf("Starting fuse2pwr_debug_mode connectivity check\n");

  //Part1: set fs2all_priv_sec_en to 1 and set opt_sec_debug_en to 1
  Platform::Delay(5);
  //step1.1: set opt_priv_sec_en to 1
  Platform::EscapeWrite("opt_priv_sec_en", 0, 1, 1);

  //step1.2: set opt_sec_debug_en to 1
  Platform::EscapeWrite("opt_sec_debug_en", 0, 1, 1);

  //step1.3: read fuse2pwr_debug_mode_real
  Platform::Delay(5);
  data = 0;
  Platform::EscapeRead("fuse2pwr_debug_mode_real", 0, 1, &data);
  if((data & 0x1) != 0x1){
    ErrPrintf("part1: fuse2pwr_debug_mode_real value is %x, which is unexpected.\n", data);
    errCnt++;
  }

  //Part2: set fs2all_priv_sec_en to 1 and set opt_sec_debug_en to 0
  Platform::Delay(5);
  //step2.1: set opt_priv_sec_en to 1
  Platform::EscapeWrite("fs2all_priv_sec_en", 0, 1, 1);

  //step2.2: set opt_sec_debug_en to 0
  Platform::EscapeWrite("opt_sec_debug_en", 0, 1, 0);

  //step2.3: read fuse2pwr_debug_mode_real
  Platform::Delay(5);
  data = 1;
  Platform::EscapeRead("fuse2pwr_debug_mode_real", 0, 1, &data);
  if((data & 0x1) != 0x0){
    ErrPrintf("part2: fuse2pwr_debug_mode_real value is %x, which is unexpected.\n", data);
    errCnt++;
  }

  //Part3: set fs2all_priv_sec_en to 0
  Platform::Delay(5);
  //step3.1: set opt_priv_sec_en to 0
  Platform::EscapeWrite("opt_priv_sec_en", 0, 1, 0);

  //step3.2: read fuse2pwr_debug_mode_real and it should be always equal to 0.
  Platform::Delay(5);
  data = 0;
  Platform::EscapeRead("fuse2pwr_debug_mode_real", 0, 1, &data);
  if((data & 0x1) != 0x1){
    ErrPrintf("part3: fuse2pwr_debug_mode_real value is %x, which is unexpected.\n", data);
    errCnt++;
  }

  InfoPrintf("fuse2pwr_debug_mode test finish. And the error number is %d.\n", errCnt);
  return errCnt;
}

int CheckConnect::check_pmgr2pwr_ext_alert_connect()
{
    UINT32  data    = 0x0;
    UINT32  errCnt  = 0x0;

    Platform::Delay(2);
    Platform::EscapeWrite("pmgr_ext_alert_0", 0, 1, 1);
    Platform::EscapeWrite("pmgr_ext_alert_1", 0, 1, 1);
    
    Platform::Delay(10);
    data = 0;
    Platform::EscapeRead("pwr_ext_alert_0", 0, 1, &data);
    if (data != 1) 
    {
        ErrPrintf("1 : pwr_ext_alert_0 should be 1, but now it's 0x%x \n", data);
        errCnt++;
    }
    data = 0;
    Platform::EscapeRead("pwr_ext_alert_1", 0, 1, &data);
    if (data != 1) 
    {
        ErrPrintf("2 : pwr_ext_alert_1 should be 1, but now it's 0x%x \n", data);
        errCnt++;
    }
    
    Platform::Delay(2);
    Platform::EscapeWrite("pmgr_ext_alert_0", 0, 1, 0);
    Platform::EscapeWrite("pmgr_ext_alert_1", 0, 1, 0);
    Platform::Delay(10);
    Platform::EscapeRead("pwr_ext_alert_0", 0, 1, &data);
    if (data != 0) 
    {
        ErrPrintf("3 :pwr_ext_alert_0 should be 0, but now it's 0x%x \n", data);
        errCnt++;
    }
    data=1;
    Platform::EscapeRead("pwr_ext_alert_1", 0, 1, &data);
    if (data != 0) 
    {
        ErrPrintf("4 : pwr_ext_alert_1 should be 0, but now it's 0x%x \n", data);
        errCnt++;
    }

  return errCnt;
}

int CheckConnect::check_priv_block_connect()
{
    UINT32  data    = 0x0;
    UINT32  errCnt  = 0x0;
    Platform::Delay(2);
    lwgpu->SetRegFldDef("LW_PTRIM_SYS_GC5_STOPCLK_CTRL", "_LWD2CLK_STOPCLK", "_NO");
    Platform::Delay(10);
    Platform::EscapeWrite("gsp2mscg_priv_if_gr_idle", 0, 1, 1);
    Platform::EscapeWrite("gsp2mscg_priv_if_ms_idle", 0, 1, 1);
    Platform::EscapeWrite("gsp2pmu_priv_blocker_intr", 0, 1, 1);
    Platform::EscapeWrite("sec2mscg_priv_if_gr_idle", 0, 1, 1);
    Platform::EscapeWrite("sec2mscg_priv_if_ms_idle", 0, 1, 1);
    Platform::EscapeWrite("sec2pmu_priv_blocker_intr", 0, 1, 1);
    Platform::Delay(10);
    data = 0;
    Platform::EscapeRead("gsp_priv_blocker_gr_idle", 0, 1, &data);
    InfoPrintf("data is 0x%x\n", data);
    if ((data & 0x1) != 1) 
    {
        ErrPrintf("1 : gsp_priv_blocker_gr_idle should be 1, but now it's 0x%x \n", data);
        errCnt++;
    }
    data = 0;
    Platform::EscapeRead("gsp_priv_blocker_ms_idle", 0, 1, &data);
    InfoPrintf("data is 0x%x\n", data);	
    if ((data & 0x1) != 1) 
    {
        ErrPrintf("2 : gsp_priv_blocker_ms_idle should be 1, but now it's 0x%x \n", data);
        errCnt++;
    }
    data = 0;
    Platform::EscapeRead("pwr_gsp2pmu_priv_blocker_intr", 0, 1, &data);
    InfoPrintf("data is 0x%x\n", data);
    if ((data & 0x1) != 1) 
    {
        ErrPrintf("3 : gsp2pmu_priv_blocker_intr should be 1, but now it's 0x%x \n", data);
        errCnt++;
    }
    data = 0;
    Platform::EscapeRead("pwr_sec2pmu_priv_blocker_intr", 0, 1, &data);
    InfoPrintf("data is 0x%x\n", data);
    if ((data & 0x1) != 1) 
    {
        ErrPrintf("4 : sec2pmu_priv_blocker_intr should be 1, but now it's 0x%x \n", data);
        errCnt++;
    }
    data = 0;
    Platform::EscapeRead("sec_priv_blocker_gr_idle", 0, 1, &data);
    InfoPrintf("data is 0x%x\n", data);
    if ((data & 0x1) != 1) 
    {
        ErrPrintf("5 : sec_priv_blocker_gr_idle should be 1, but now it's 0x%x \n", data);
        errCnt++;
    }
    data = 0;
    Platform::EscapeRead("sec_priv_blocker_ms_idle", 0, 1, &data);
    InfoPrintf("data is 0x%x\n", data);
    if ((data & 0x1) != 1) 
    {
        ErrPrintf("6 : sec_priv_blocker_ms_idle should be 1, but now it's 0x%x \n", data);
        errCnt++;
    }
    Platform::Delay(2);
    Platform::EscapeWrite("gsp2mscg_priv_if_gr_idle", 0, 1, 0);
    Platform::EscapeWrite("gsp2mscg_priv_if_ms_idle", 0, 1, 0);
    Platform::EscapeWrite("gsp2pmu_priv_blocker_intr", 0, 1, 0);
    Platform::EscapeWrite("sec2mscg_priv_if_gr_idle", 0, 1, 0);
    Platform::EscapeWrite("sec2mscg_priv_if_ms_idle", 0, 1, 0);
    Platform::EscapeWrite("sec2pmu_priv_blocker_intr", 0, 1, 0);
    Platform::Delay(10);
    data = 1;
    Platform::EscapeRead("gsp_priv_blocker_gr_idle", 0, 1, &data);
    InfoPrintf("data is 0x%x\n", data);
    if ((data & 0x1) != 0) 
    {
        ErrPrintf("1 : gsp_priv_blocker_gr_idle should be 0, but now it's 0x%x \n", data);
        errCnt++;
    }
    data = 1;
    Platform::EscapeRead("gsp_priv_blocker_ms_idle", 0, 1, &data);
    InfoPrintf("data is 0x%x\n", data);
    if ((data & 0x1) != 0) 
    {
        ErrPrintf("2 : gsp_priv_blocker_ms_idle should be 0, but now it's 0x%x \n", data);
        errCnt++;
    }
    data = 1;
    Platform::EscapeRead("pwr_gsp2pmu_priv_blocker_intr", 0, 1, &data);
    InfoPrintf("data is 0x%x\n", data);
    if ((data & 0x1) != 0) 
    {
        ErrPrintf("3 : gsp2pmu_priv_blocker_intr should be 0, but now it's 0x%x \n", data);
        errCnt++;
    }
    data = 1;
    Platform::EscapeRead("pwr_sec2pmu_priv_blocker_intr", 0, 1, &data);
    InfoPrintf("data is 0x%x\n", data);
    if ((data & 0x1) != 0) 
    {
        ErrPrintf("4 : sec2pmu_priv_blocker_intr should be 0, but now it's 0x%x \n", data);
        errCnt++;
    }
    data = 1;
    Platform::EscapeRead("sec_priv_blocker_gr_idle", 0, 1, &data);
    InfoPrintf("data is 0x%x\n", data);
    if ((data & 0x1) != 0) 
    {
        ErrPrintf("5 : sec_priv_blocker_gr_idle should be 0, but now it's 0x%x \n", data);
        errCnt++;
    }
    data = 1;
    Platform::EscapeRead("sec_priv_blocker_ms_idle", 0, 1, &data);
    InfoPrintf("data is 0x%x\n", data);
    if ((data & 0x1) != 0) 
    {
        ErrPrintf("6 : sec_priv_blocker_ms_idle should be 0, but now it's 0x%x \n", data);
        errCnt++;
    }  
    return errCnt;
}

int CheckConnect::check_vid_pwm()
{
    UINT32  data    = 0x0;
    UINT32  errCnt  = 0x0;

    Platform::Delay(2);
    Platform::EscapeWrite("pwr2pmgr_vid_pwm_0", 0, 1, 1);
    Platform::EscapeWrite("pwr2pmgr_vid_pwm_1", 0, 1, 1);
    Platform::EscapeWrite("pwr2pmgr_vid_pwm_2", 0, 1, 1);
    Platform::EscapeWrite("pwr2pmgr_vid_pwm_3", 0, 1, 1);
    
    Platform::Delay(10);
    data = 0;
    Platform::EscapeRead("pmgr_pwr2pmgr_vid_pwm_0", 0, 1, &data);
    if (data != 1) 
    {
        ErrPrintf("1 : pmgr_pwr2pmgr_vid_pwm_0 should be 1, but now it's 0x%x \n", data);
        errCnt++;
    }
    data = 0;
    Platform::EscapeRead("pmgr_pwr2pmgr_vid_pwm_1", 0, 1, &data);
    if (data != 1) 
    {
        ErrPrintf("2 : pmgr_pwr2pmgr_vid_pwm_1 should be 1, but now it's 0x%x \n", data);
        errCnt++;
    }
    data = 0;
    Platform::EscapeRead("pmgr_pwr2pmgr_vid_pwm_2", 0, 1, &data);
    if (data != 1) 
    {
        ErrPrintf("3 : pmgr_pwr2pmgr_vid_pwm_2 should be 1, but now it's 0x%x \n", data);
        errCnt++;
    }
    data = 0;
    Platform::EscapeRead("pmgr_pwr2pmgr_vid_pwm_3", 0, 1, &data);
    if (data != 1) 
    {
        ErrPrintf("4 : pmgr_pwr2pmgr_vid_pwm_3 should be 1, but now it's 0x%x \n", data);
        errCnt++;
    }
    
    Platform::Delay(2);
    Platform::EscapeWrite("pwr2pmgr_vid_pwm_0", 0, 1, 0);
    Platform::EscapeWrite("pwr2pmgr_vid_pwm_1", 0, 1, 0);
    Platform::EscapeWrite("pwr2pmgr_vid_pwm_2", 0, 1, 0);
    Platform::EscapeWrite("pwr2pmgr_vid_pwm_3", 0, 1, 0);
    Platform::Delay(10);
    data = 1;
    Platform::EscapeRead("pmgr_pwr2pmgr_vid_pwm_0", 0, 1, &data);
    if (data != 0) 
    {
        ErrPrintf("5 :pmgr_pwr2pmgr_vid_pwm_0 should be 0, but now it's 0x%x \n", data);
        errCnt++;
    }
    data=1;
    Platform::EscapeRead("pmgr_pwr2pmgr_vid_pwm_1", 0, 1, &data);
    if (data != 0) 
    {
        ErrPrintf("6 : pmgr_pwr2pmgr_vid_pwm_1 should be 0, but now it's 0x%x \n", data);
        errCnt++;
    }
    data=1;
    Platform::EscapeRead("pmgr_pwr2pmgr_vid_pwm_2", 0, 1, &data);
    if (data != 0) 
    {
        ErrPrintf("7 : pmgr_pwr2pmgr_vid_pwm_2 should be 0, but now it's 0x%x \n", data);
        errCnt++;
    }
    data=1;
    Platform::EscapeRead("pmgr_pwr2pmgr_vid_pwm_3", 0, 1, &data);
    if (data != 0) 
    {
        ErrPrintf("8 : pmgr_pwr2pmgr_vid_pwm_3 should be 0, but now it's 0x%x \n", data);
        errCnt++;
    }

  return errCnt;
}

int CheckConnect::check_hshub2x_connect()
{
    UINT32  data    = 0x1;
    UINT32  errCnt  = 0x0;
    UINT32 i = 0;
    char writeallidle[40];
    char readallidle[40];
    char writenisoidle[40];
    char readnisoidle[40];
    char writemmuidle[40];
    char readmmuidle[40];
    InfoPrintf("Starting hshub connectivity check\n");

    //connection check for hshub_idle
    for (i = 0; i < macros.lw_scal_litter_num_hshubs; i++)
    {
        sprintf(writeallidle,"drv_hshub%d2pmu_all_idle",i);
        sprintf(readallidle,"hshub%d2pmu_all_idle",i);
        sprintf(writenisoidle,"drv_hshub%d2pmu_niso_idle",i);
        sprintf(readnisoidle,"hshub%d2pmu_niso_idle",i);

        //connection check for all_idle
        data = 0;
        Platform::Delay(2);
        Platform::EscapeWrite(writeallidle, 0, 1, 1);
        Platform::Delay(10);
        Platform::EscapeRead(readallidle, 0, 1, &data);
        if (data != 1) 
        {
            ErrPrintf("hshub%d2pmu_all_idle should be 1, but now it's 0x%x \n",i, data);
            errCnt++;
        }
        data = 1;
        Platform::Delay(2);
        Platform::EscapeWrite(writeallidle, 0, 1, 0);
        Platform::Delay(10);
        Platform::EscapeRead(readallidle, 0, 1, &data);
        if (data != 0) 
        {
            ErrPrintf("hshub%d2pmu_all_idle should be 0, but now it's 0x%x \n",i, data);
            errCnt++;
        }
        //connection check for niso_idle
        data = 0;
        Platform::Delay(2);
        Platform::EscapeWrite(writenisoidle, 0, 1, 1);
        Platform::Delay(10);
        Platform::EscapeRead(readnisoidle, 0, 1, &data);
        if (data != 1) 
        {
            ErrPrintf("hshub%d2pmu_all_idle should be 1, but now it's 0x%x \n",i, data);
            errCnt++;
        }
        data = 1;
        Platform::Delay(2);
        Platform::EscapeWrite(writenisoidle, 0, 1, 0);
        Platform::Delay(10);
        Platform::EscapeRead(readnisoidle, 0, 1, &data);
        if (data != 0) 
        {
            ErrPrintf("hshub%d2pmu_all_idle should be 0, but now it's 0x%x \n",i, data);
            errCnt++;
        }
    }

    InfoPrintf("Starting hshubmmu connectivity check\n");
    //connection check for hshubmmu_idle
    sprintf(writemmuidle,"drv_hshubmmu%d2pmu_idle",i);
    sprintf(readmmuidle,"hshubmmu%d2pmu_idle",i);

    //connection check for hshubmmu idle
    data = 0;
    Platform::Delay(2);
    Platform::EscapeWrite("drv_hshubmmu2pmu_idle", 0, 1, 1);
    Platform::Delay(10);
    Platform::EscapeRead("hshubmmu2pmu_idle", 0, 1, &data);
    if (data != 1) 
    {
            ErrPrintf("hshubmmu2pmu_idle should be 1, but now it's 0x%x \n", data);
            errCnt++;
    }
    data = 1;
    Platform::Delay(2);
    Platform::EscapeWrite("drv_hshubmmu2pmu_idle", 0, 1, 0);
    Platform::Delay(10);
    Platform::EscapeRead("hshubmmu2pmu_idle", 0, 1, &data);
    if (data != 0) 
    {
            ErrPrintf("hshubmmu2pmu_idle should be 0, but now it's 0x%x \n", data);
            errCnt++;
    }   

    return errCnt;
}

int CheckConnect::check_misc_connect()
{
    UINT32  data    = 0x0;
    UINT32  errCnt  = 0x0;

    Platform::Delay(2);
    Platform::EscapeWrite("drv_hubmmu2pmu_idle", 0, 1, 1);
    Platform::Delay(10);
    Platform::EscapeWrite("drv_hubmmu2pmu_idle", 0, 1, 0);
    Platform::Delay(10);
    Platform::EscapeRead("hubmmu2pmu_idle", 0, 1, &data);
    if (data != 0) 
    {
        ErrPrintf("hubmmu2pmu_idle should be 0, but now it's 0x%x \n", data);
        errCnt++;
    }

    //dgpu only
    if (m_arch != 3584)
    {
        if (macros.lw_gpu_core_ip__not) 
        {
            data = 0;
            lwgpu->SetRegFldDef("LW_THERM_MAXFAN", "_FORCE", "_TRIGGER");
            Platform::Delay(10);
            Platform::EscapeRead("therm2sci_temp_alert", 0, 4, &data);
            InfoPrintf("data is 0x%x\n", data);
            if ((data & 0x3) != 0x3) 
            { 
                ErrPrintf("max fan connection error\n"); 
                errCnt++; 
            }

            lwgpu->SetRegFldNum("LW_THERM_MAXFAN", "_STATUS_SMARTFAN_SCI_1", 1, &data);
            lwgpu->SetRegFldNum("LW_THERM_MAXFAN", "_STATUS_SMARTFAN_SCI_0", 0, &data);
            lwgpu->SetRegFldNum("LW_THERM_MAXFAN", "", data);
            Platform::Delay(10);
            Platform::EscapeRead("therm2sci_temp_alert", 0, 4, &data);
            InfoPrintf("data is 0x%x\n", data);
            if ((data & 0x3) != 1) 
            { 
                ErrPrintf("max fan connection error1\n"); 
                errCnt++; 
            }

            lwgpu->SetRegFldNum("LW_THERM_MAXFAN", "_STATUS_SMARTFAN_SCI_1", 0, &data);
            lwgpu->SetRegFldNum("LW_THERM_MAXFAN", "_STATUS_SMARTFAN_SCI_0", 1, &data);
            lwgpu->SetRegFldNum("LW_THERM_MAXFAN", "", data);
            Platform::Delay(10);
            Platform::EscapeRead("therm2sci_temp_alert", 0, 4, &data);
            InfoPrintf("data is 0x%x\n", data);
            if ((data & 0x3) != 0) 
            { 
                ErrPrintf("max fan connection error2\n"); 
                errCnt++; 
            }
        }
    }

    data = 0;
    Platform::EscapeWrite("drv_perf2pmu_fb_blocker_intr", 0, 1, 1);
    Platform::Delay(10);
    Platform::EscapeRead("perf2pmu_fb_blocker_intr", 0, 1, &data);
    if (data != 1) 
    { 
        ErrPrintf("perf2pmu_fb_blocker_intr connection error\n"); 
        errCnt++; 
    } 
	
	//dgpu only
    if (macros.lw_gpu_core_ip__not) 
    {
        data = 0;
        Platform::Delay(2);
        Platform::EscapeWrite("ext_overt", 0, 1, 1);
        Platform::Delay(10);
        InfoPrintf("Check state of overt by checking loop-back value.\n");
        Platform::EscapeRead("overt_IB", 0, 1, &data);
        if (data != 1) 
        {
            ErrPrintf("EXT_OVERT not correct. OVERT_CTRL_EXT_OVERT_ASSERTED = %d\n", data);
            errCnt++;
        }
    }
 
    Platform::Delay(2);

    return errCnt;
}
