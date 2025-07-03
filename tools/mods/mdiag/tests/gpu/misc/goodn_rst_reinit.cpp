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
// To facilitate faster compiles via pre-compiled headers, keep "stdtest.h" first.

/*****************************************************/
//test for top level reset and reinit recovery        /
/*****************************************************/
#include "mdiag/tests/stdtest.h"

#include "goodn_rst_reinit.h"

#include "ampere/ga100/dev_bus.h"
#include "ampere/ga100/dev_fuse.h"
#include "ampere/ga100/dev_graphics_nobundle.h"
#include "ampere/ga100/dev_master.h"
#include "ampere/ga100/dev_lw_xve.h"
#include "ampere/ga100/dev_pwr_pri.h"
#include "ampere/ga100/dev_sec_pri.h"
#include "ampere/ga100/dev_timer.h"
#include "ampere/ga100/dev_trim.h"
#include "core/include/gpu.h"
#include "core/include/jscript.h"
#include "device/interface/pcie.h"
#include "gpu/include/gpusbdev.h"
#include "hopper/gh100/dev_xtl_ep_pcfg_gpu.h"
#include "mdiag/lwgpu.h"
#include "mdiag/tests/test_state_report.h"
#include "mdiag/utils/crc.h"
#include "mdiag/utils/lwgpu_channel.h"
#include "rm.h"

extern const ParamDecl goodn_rst_reinit_params[] = {
  SIMPLE_PARAM("-bringup",    "Testing bringup mode...  needs no special VCS args"),
  SIMPLE_PARAM("-xve_rst",    "Testing XVE reset control mode...  needs no special VCS args"),
//  SIMPLE_PARAM("-prod", "Testing production mode...  needs +refpresent_1 VCS arg"),
//  SIMPLE_PARAM("-multi_level", "Testing production mode...  needs +refpresent_1 & +refpresent_0  VCS arg"),
  PARAM_SUBDECL(lwgpu_single_params),
  LAST_PARAM
};

static int bringup = 0;
static int xve_rst = 0;

GoodnRstReInit::GoodnRstReInit(ArgReader *params) : LWGpuSingleChannelTest(params)
{
    if (params->ParamPresent("-bringup"))
        bringup = 1;
    if (params->ParamPresent("-xve_rst"))
        xve_rst = 1;
    else
        InfoPrintf("goodn_rst_reinit.cpp test - Parameter xve_rst is not set for this test\n");
}

GoodnRstReInit::~GoodnRstReInit(void)
{
}

STD_TEST_FACTORY(goodn_rst_reinit, GoodnRstReInit)

int GoodnRstReInit::Setup(void) {
    lwgpu = LWGpuResource::FindFirstResource();
    //UINT32 arch = lwgpu->GetArchitecture();

    ch = lwgpu->CreateChannel();
    if(!ch)
    {
        ErrPrintf("GoodnRstReInit::Setup failed to create channel\n");
        return 0;
    }

    getStateReport()->init("goodn_rst_reinit");
    getStateReport()->enable();

    return 1;
}

void GoodnRstReInit::CleanUp(void) {
    if(ch)
    {
        delete ch;
        ch = 0;
    }

    if (lwgpu) {
        DebugPrintf("GoodnRstReInit::CleanUp(): Releasing GPU.\n");
        lwgpu = 0;
    }
    LWGpuSingleChannelTest::CleanUp();
}

bool  GoodnRstReInit::check_results( UINT32 address, UINT32 expected_data)
{
    UINT32 read_data;
    read_data = lwgpu->GetGpuSubdevice()->RegRd32(address);
    InfoPrintf("Read 0x%x from address: 0x%x\n", read_data,address);
    if (read_data != expected_data) {
       getStateReport()->runFailed("Failed Self Check Register Read\n");
       ErrPrintf("Failed Self Check Reg Read. Expected 0x%x, Actual 0x%x NOTE: Please make sure pluargs  +heed_fuses are set.. \n",expected_data, read_data);
       SetStatus(TEST_FAILED);
       return(false);
    }
    return(true);
}

void GoodnRstReInit::Run(void) {
    SetStatus(TEST_INCOMPLETE);

    InfoPrintf("Performing goodn_rst_reinit test. Parameter xve_rst is 0x%x\n", xve_rst);
    if(goodnRstReInitTest()) {
        SetStatus(TEST_FAILED);
        getStateReport()->runFailed("Failed test\n");
        return;
    }
    SetStatus(TEST_SUCCEEDED);
    getStateReport()->crcCheckPassedGold();
}

extern SObject Gpu_Object;  //yuck, move to .h

UINT32 GoodnRstReInit::goodnRstReInitTest()
{

  GpuPtr pGpu;

  JavaScriptPtr pJs;
  RC             rc;

  GpuSubdevice * pSubdev;

//  UINT32 i;
  int errCnt = 0;

  int address = 0;
  int wdata = 0;

  pSubdev = lwgpu->GetGpuSubdevice();
  //pSubdev = GpuPtr()->GetGpuSubdevice();

// READ and check
//Platform::EscapeRead("pcie_clk_spd_toggle_en",0,1,&spd_toggle_en);// note that spd_toggle_en must be an int.
//if (spd_toggle_en != 0) {ErrPrintf("pcie_clk_spd_toggle_en is turned on before test started"); return (1);}
//    InfoPrintf("goodnRstReInitTest::check pcie_clk_spd_toggle_en initial value done\n");

//Platform::EscapeWrite("pcie_clk_spd_toggle_en",0,1,1);
//    InfoPrintf("goodnRstReInitTest::set pcie_clk_spd_toggle_en to 1 done\n");

Platform::Delay(100);  // note that the delay time need to be adjusted if clock frequency is different

  ////////////////////////////
  //Save off some config space
  //
  UINT32 devctrl; 
  UINT32 rev_id ;
  UINT32 misc_1 ;
  UINT32 bar0addr;
  UINT32 irqSet ;
  UINT32 vendid ;
  UINT32 bar1lo ;
  UINT32 bar1hi ;
  UINT32 bar2lo ;
  UINT32 bar2hi ;
  UINT32 bar3addr;

  ////////////////////////////
  //Save Off PCI parameters for drv
  //
  auto pGpuPcie = pSubdev->GetInterface<Pcie>();
  UINT32 pci_domain_num = pGpuPcie->DomainNumber();
  UINT32 pci_bus_num = pGpuPcie->BusNumber();
  UINT32 pci_dev_num = pGpuPcie->DeviceNumber();
  UINT32 pci_func_num = pGpuPcie->FunctionNumber();

  //save config space 
  InfoPrintf("goodn_rst_reinit test. Saving config space\n");
  address = LW_EP_PCFG_GPU_CTRL_CMD_AND_STATUS;
  CHECK_RC(Platform::PciRead32(pci_domain_num,
                       pci_bus_num,
                       pci_dev_num,
                       pci_func_num,
                       address,&devctrl)
  );
  address = LW_EP_PCFG_GPU_REVISION_ID_AND_CLASSCODE;
  CHECK_RC(Platform::PciRead32(pci_domain_num,
                       pci_bus_num,
                       pci_dev_num,
                       pci_func_num,
                       address,&rev_id)
  );
  address = LW_EP_PCFG_GPU_CACHE_LATENCY_HEADER;
  CHECK_RC(Platform::PciRead32(pci_domain_num,
                       pci_bus_num,
                       pci_dev_num,
                       pci_func_num,
                       address,&misc_1)
  );
  address = LW_EP_PCFG_GPU_BARREG0;
  CHECK_RC(Platform::PciRead32(pci_domain_num,
                       pci_bus_num,
                       pci_dev_num,
                       pci_func_num,
                       address,&bar0addr)
  );
  address = LW_EP_PCFG_GPU_MISC;
  CHECK_RC(Platform::PciRead32(pci_domain_num,
                       pci_bus_num,
                       pci_dev_num,
                       pci_func_num,
                       address,&irqSet)
  );
  address = LW_EP_PCFG_GPU_SUBSYSTEM_ID;
  CHECK_RC(Platform::PciRead32(pci_domain_num,
                       pci_bus_num,
                       pci_dev_num,
                       pci_func_num,
                       address,&vendid)
  );
  address = LW_EP_PCFG_GPU_BARREG1;
  CHECK_RC(Platform::PciRead32(pci_domain_num,
                       pci_bus_num,
                       pci_dev_num,
                       pci_func_num,
                       address,&bar1lo)
  );
  address = LW_EP_PCFG_GPU_BARREG2;
  CHECK_RC(Platform::PciRead32(pci_domain_num,
                       pci_bus_num,
                       pci_dev_num,
                       pci_func_num,
                       address,&bar1hi)
  );
  address = LW_EP_PCFG_GPU_BARREG3;
  CHECK_RC(Platform::PciRead32(pci_domain_num,
                       pci_bus_num,
                       pci_dev_num,
                       pci_func_num,
                       address,&bar2lo)
  );
  address = LW_EP_PCFG_GPU_BARREG4;
  CHECK_RC(Platform::PciRead32(pci_domain_num,
                       pci_bus_num,
                       pci_dev_num,
                       pci_func_num,
                       address,&bar2hi)
  );
  address = LW_EP_PCFG_GPU_BARREG5;
  CHECK_RC(Platform::PciRead32(pci_domain_num,
                       pci_bus_num,
                       pci_dev_num,
                       pci_func_num,
                       address,&bar3addr)
  );
  InfoPrintf("goodn_rst_reinit test. - Config space saved. There are the saved off values, LW_EP_PCFG_GPU_CTRL_CMD_AND_STATUS = 0x%x, LW_EP_PCFG_GPU_REVISION_ID_AND_CLASSCODE =  0x%x, LW_EP_PCFG_GPU_CACHE_LATENCY_HEADER =  0x%x, LW_EP_PCFG_GPU_BARREG0 =  0x%x, LW_EP_PCFG_GPU_MISC =  0x%x, LW_EP_PCFG_GPU_SUBSYSTEM_ID =  0x%x, LW_EP_PCFG_GPU_BARREG1 =  0x%x, LW_EP_PCFG_GPU_BARREG2 =  0x%x, LW_EP_PCFG_GPU_BARREG3 =  0x%x, LW_EP_PCFG_GPU_BARREG4 =  0x%x, LW_EP_PCFG_GPU_BARREG5 =   0x%x \n",devctrl, rev_id, misc_1, bar0addr, irqSet, vendid, bar1lo, bar1hi, bar2lo, bar2hi, bar3addr);


  ////////////////////////////
  // Intentionally corrupt the sample regs
  InfoPrintf("goodn_rst_reinit test. Intentionally corrupting sample registers\n");
  wdata = 0xdeadbeef;

  address = LW_FUSE_FUSETIME_PGM1;
  lwgpu->GetGpuSubdevice()->RegWr32(address,wdata);
  address = LW_FUSE_FUSETIME_PGM2;
  lwgpu->GetGpuSubdevice()->RegWr32(address,wdata);
  address = LW_PBUS_SW_SCRATCH(0);
  lwgpu->GetGpuSubdevice()->RegWr32(address,wdata);

  address = LW_PGRAPH_DEBUG_0;
  lwgpu->GetGpuSubdevice()->RegWr32(address,wdata);
  address = LW_PGRAPH_DEBUG_2;
  lwgpu->GetGpuSubdevice()->RegWr32(address,wdata);

  address = LW_PPWR_PMU_DEBUG_TAG;
  lwgpu->GetGpuSubdevice()->RegWr32(address,wdata);
  address = LW_PSEC_FALCON_DBGCTL;
  lwgpu->GetGpuSubdevice()->RegWr32(address,wdata);

  pGpu = GpuPtr();

  ////////////////////////////
  // Shutdown the RM and gpu.
  //
  //GpuPtr()->ShutDown();
  //Bug 2800408 . We need to call below to free resources before calling Gpu::ShutDown() 
  LWGpuResource::FreeResources();
  pGpu->ShutDown(Gpu::ShutDownMode::Normal);

  Platform::Delay(160);

  //re-acquire setup of lwgpu
  //lwgpu->AcquireResource(Resource::ACQUIRE_SHARED);

if ( xve_rst == 0 ) {

  ////////////////////////////
  // Reset LOW
  InfoPrintf("goodn_rst_reinit test. Asserting pex reset\n");
  Platform::EscapeWrite("rst_n",0,1,0);

  Platform::Delay(1200);

  ////////////////////////////
  // Exit out of Reset
  InfoPrintf("goodn_rst_reinit test. De-asserting pex reset\n");
  Platform::EscapeWrite("rst_n",0,1,1);

  Platform::Delay(500);
  //Other delays enforced by HW

  Platform::Delay(20);

}
else {

  ////////////////////////////
  // Write XVE suicide reset control bit
  // with a reasonable duration counter.
  // This is tricky because the suicide bit is not accessible when RM is unloaded !!!

  //Restore config space
  InfoPrintf("goodn_rst_reinit test - Restoring config space in xve_reset == 1 test\n");
  address = LW_EP_PCFG_GPU_CTRL_CMD_AND_STATUS;
  CHECK_RC(Platform::PciWrite32(pci_domain_num,
                       pci_bus_num,
                       pci_dev_num,
                       pci_func_num,
                       address,devctrl)
  );
  address = LW_EP_PCFG_GPU_REVISION_ID_AND_CLASSCODE ;
  CHECK_RC(Platform::PciWrite32(pci_domain_num,
                       pci_bus_num,
                       pci_dev_num,
                       pci_func_num,
                       address,rev_id)
  );
  address = LW_EP_PCFG_GPU_CACHE_LATENCY_HEADER;
  CHECK_RC(Platform::PciWrite32(pci_domain_num,
                       pci_bus_num,
                       pci_dev_num,
                       pci_func_num,
                       address,misc_1)
  );
  address = LW_EP_PCFG_GPU_BARREG0;
  CHECK_RC(Platform::PciWrite32(pci_domain_num,
                       pci_bus_num,
                       pci_dev_num,
                       pci_func_num,
                       address,bar0addr)
  );
  address = LW_EP_PCFG_GPU_MISC;
  CHECK_RC(Platform::PciWrite32(pci_domain_num,
                       pci_bus_num,
                       pci_dev_num,
                       pci_func_num,
                       address,irqSet)
  );
  address = LW_EP_PCFG_GPU_SUBSYSTEM_ID;
  CHECK_RC(Platform::PciWrite32(pci_domain_num,
                       pci_bus_num,
                       pci_dev_num,
                       pci_func_num,
                       address,vendid)
  );
  address = LW_EP_PCFG_GPU_BARREG1;
  CHECK_RC(Platform::PciWrite32(pci_domain_num,
                       pci_bus_num,
                       pci_dev_num,
                       pci_func_num,
                       address,bar1lo)
  );
  address = LW_EP_PCFG_GPU_BARREG2;
  CHECK_RC(Platform::PciWrite32(pci_domain_num,
                       pci_bus_num,
                       pci_dev_num,
                       pci_func_num,
                       address,bar1hi)
  );
  address = LW_EP_PCFG_GPU_BARREG3;
  CHECK_RC(Platform::PciWrite32(pci_domain_num,
                       pci_bus_num,
                       pci_dev_num,
                       pci_func_num,
                       address,bar2lo)
  );
  address = LW_EP_PCFG_GPU_BARREG4;
  CHECK_RC(Platform::PciWrite32(pci_domain_num,
                       pci_bus_num,
                       pci_dev_num,
                       pci_func_num,
                       address,bar2hi)
  );
  address = LW_EP_PCFG_GPU_BARREG5;
  CHECK_RC(Platform::PciWrite32(pci_domain_num,
                       pci_bus_num,
                       pci_dev_num,
                       pci_func_num,
                       address,bar3addr)
  );

        Platform::Delay(30);

  Platform::PciWrite32(pci_domain_num, pci_bus_num, pci_dev_num, pci_func_num, LW_XVE_RESET_CTRL,
                DRF_NUM(_XVE, _RESET_CTRL, _GPU_RESET, 1) |
                DRF_NUM(_XVE, _RESET_CTRL, _RESET_COUNTER_VAL, 256));

  Platform::Delay(150);
  //Other delays enforced by HW

}

//Restore config space
  InfoPrintf("goodn_rst_reinit test - Restoring config space after pex reset test\n");
  address = LW_EP_PCFG_GPU_CTRL_CMD_AND_STATUS;
  CHECK_RC(Platform::PciWrite32(pci_domain_num,
                       pci_bus_num,
                       pci_dev_num,
                       pci_func_num,
                       address,devctrl)
  );
  address = LW_EP_PCFG_GPU_REVISION_ID_AND_CLASSCODE ;
  CHECK_RC(Platform::PciWrite32(pci_domain_num,
                       pci_bus_num,
                       pci_dev_num,
                       pci_func_num,
                       address,rev_id)
  );
  address = LW_EP_PCFG_GPU_CACHE_LATENCY_HEADER;
  CHECK_RC(Platform::PciWrite32(pci_domain_num,
                       pci_bus_num,
                       pci_dev_num,
                       pci_func_num,
                       address,misc_1)
  );
  address = LW_EP_PCFG_GPU_BARREG0;
  CHECK_RC(Platform::PciWrite32(pci_domain_num,
                       pci_bus_num,
                       pci_dev_num,
                       pci_func_num,
                       address,bar0addr)
  );
  address = LW_EP_PCFG_GPU_MISC;
  CHECK_RC(Platform::PciWrite32(pci_domain_num,
                       pci_bus_num,
                       pci_dev_num,
                       pci_func_num,
                       address,irqSet)
  );
  address = LW_EP_PCFG_GPU_SUBSYSTEM_ID;
  CHECK_RC(Platform::PciWrite32(pci_domain_num,
                       pci_bus_num,
                       pci_dev_num,
                       pci_func_num,
                       address,vendid)
  );
  address = LW_EP_PCFG_GPU_BARREG1;
  CHECK_RC(Platform::PciWrite32(pci_domain_num,
                       pci_bus_num,
                       pci_dev_num,
                       pci_func_num,
                       address,bar1lo)
  );
  address = LW_EP_PCFG_GPU_BARREG2;
  CHECK_RC(Platform::PciWrite32(pci_domain_num,
                       pci_bus_num,
                       pci_dev_num,
                       pci_func_num,
                       address,bar1hi)
  );
  address = LW_EP_PCFG_GPU_BARREG3;
  CHECK_RC(Platform::PciWrite32(pci_domain_num,
                       pci_bus_num,
                       pci_dev_num,
                       pci_func_num,
                       address,bar2lo)
  );
  address = LW_EP_PCFG_GPU_BARREG4;
  CHECK_RC(Platform::PciWrite32(pci_domain_num,
                       pci_bus_num,
                       pci_dev_num,
                       pci_func_num,
                       address,bar2hi)
  );
  address = LW_EP_PCFG_GPU_BARREG5;
  CHECK_RC(Platform::PciWrite32(pci_domain_num,
                       pci_bus_num,
                       pci_dev_num,
                       pci_func_num,
                       address,bar3addr)
  );


  Platform::Delay(100);

  //Only init the bare minimum.
//XXX  GpuPtr()->SkipDevInit=true;

  InfoPrintf("goodn_rst_reinit test - Skipping devinit\n");
  CHECK_RC( pJs->SetProperty(Gpu_Object.GetJSObject(), "SkipDevInit", true) );
  InfoPrintf("goodn_rst_reinit test - Skipping config sim\n");
  CHECK_RC( pJs->SetProperty(Gpu_Object.GetJSObject(), "SkipConfigSim", true) );

  ////////////////////////////
  // Re-Initialize
  //
  //GpuPtr()->Initialize();
  InfoPrintf("goodn_rst_reinit test - Initializing gpu again\n");
  pGpu->Initialize();
  //Bug 2812901, enabling interrupts after initializing GPU, since HookISR is skipped in skipDevInit
  InfoPrintf("goodn_rst_reinit test - Enabling interrupts\n");
  Platform::EnableInterrupts();
  //Bug 2800408: We need to call below to create new GpuSubdevice pointers after gpu initialize
  InfoPrintf("goodn_rst_reinit test - Before scan system function\n");
  LWGpuResource::ScanSystem(CommandLine::ArgDb());

  Platform::Delay(50);

  //Do we need to reinit lwgpu, or did Initialize implicitly redefine its target???
  InfoPrintf("goodn_rst_reinit test - Before finding first resource\n");
  lwgpu = LWGpuResource::FindFirstResource();

  ////////////////////////////
  //Test some Registers etc.

  lwgpu->GetRegFldDef("LW_FUSE_FUSETIME_PGM1","_TSUP_MAX","_INIT", &m_initVal_tsup_max);
  lwgpu->GetRegFldDef("LW_FUSE_FUSETIME_PGM1","_TSUP_ADDR","_INIT", &m_initVal_tsup_addr);
  lwgpu->GetRegFldDef("LW_FUSE_FUSETIME_PGM1","_THP_ADDR","_INIT", &m_initVal_thp_addr);
  lwgpu->GetRegFldDef("LW_FUSE_FUSETIME_PGM1","_THP_PS","_INIT", &m_initVal_thp_ps);
  lwgpu->GetRegFldDef("LW_FUSE_FUSETIME_PGM2","_TWIDTH_PGM","_INIT", &m_initVal_twidth_pgm2);
  lwgpu->GetRegFldDef("LW_FUSE_FUSETIME_PGM2","_TSUP_PS","_INIT", &m_initVal_tsup_ps);
  lwgpu->GetRegFldDef("LW_FUSE_FUSETIME_PGM2","_THP_CSPS","_INIT", &m_initVal_thp_csps);

  m_initVal_fusetime_pgm1=((m_initVal_thp_ps<<24) +(m_initVal_thp_addr<<16)+ (m_initVal_tsup_addr<<8)+m_initVal_tsup_max);
  InfoPrintf("callwlated reset value of fusetime_pgm1 0x%x\n",m_initVal_fusetime_pgm1);
  m_initVal_fusetime_pgm2=((m_initVal_thp_csps<<24)+(m_initVal_tsup_ps<<16)+ m_initVal_twidth_pgm2);
  InfoPrintf("callwlated reset value of fusetime_pgm2 0x%x\n",m_initVal_fusetime_pgm2);

  if (!check_results(LW_FUSE_FUSETIME_PGM1,m_initVal_fusetime_pgm1)) {errCnt++;}

  if (!check_results(LW_FUSE_FUSETIME_PGM2,m_initVal_fusetime_pgm2)) {errCnt++;}

  if (!check_results(LW_PBUS_SW_SCRATCH(0),0x00000000)) {errCnt++;}

  if (!check_results(LW_PPWR_PMU_DEBUG_TAG,0x00000000)) {errCnt++;}
  if (!check_results(LW_PSEC_FALCON_DBGCTL,0x00000000)) {errCnt++;}

  if (errCnt >0)
  {
    ErrPrintf("goodnRstReInitTest: failed with %d errors\n",errCnt);
    return (1);
  } else
  {
    InfoPrintf("goodnRstReInitTest: PASSED!\n");
    return (0);
  }
}
