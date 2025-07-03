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

#include "hotrst_reinit.h"

#include "ampere/ga100/dev_fuse.h"
#include "ampere/ga100/dev_graphics_nobundle.h"
#include "ampere/ga100/dev_host.h"
#include "ampere/ga100/dev_master.h"
#include "ampere/ga100/dev_lw_xp.h"
#include "ampere/ga100/dev_timer.h"
#include "ampere/ga100/dev_trim.h"
#include "ampere/ga102/dev_lw_xve.h"
#include "core/include/cmdline.h"
#include "core/include/jscript.h"
#include "device/interface/pcie.h"
#include "disp/v02_02/dev_disp.h"
#include "gpu/include/gpusbdev.h"
#include "hopper/gh100/dev_bus.h"
#include "hopper/gh100/dev_pwr_pri.h"
#include "hopper/gh100/dev_therm.h"
#include "hopper/gh100/dev_xtl_ep_pcfg_gpu.h"
#include "hopper/gh100/dev_xtl_ep_pri.h"
#include "mdiag/lwgpu.h"
#include "mdiag/tests/test_state_report.h"
#include "mdiag/utils/crc.h"
#include "mdiag/utils/cregctrl.h"
#include "mdiag/utils/lwgpu_channel.h"
#include "lwmisc.h"

extern SObject Gpu_Object;  //yuck, move to .h

extern const ParamDecl hotrst_reinit_params[] = {
  SIMPLE_PARAM("-bringup",    "Testing bringup mode...  needs no special VCS args"),

  SIMPLE_PARAM("-testhotrst1", "Testing the hot reset in mod 1"),
  SIMPLE_PARAM("-testhotrst2", "Testing the hot reset in mod 2"),
  SIMPLE_PARAM("-testhotrst3", "Testing the hot reset in mod 3"),
  SIMPLE_PARAM("-testhotrst4", "Testing the hot reset in mod 4"),
  SIMPLE_PARAM("-testswrst", "Testing the SW reset"),

//  SIMPLE_PARAM("-prod", "Testing production mode...  needs +refpresent_1 VCS arg"),
//  SIMPLE_PARAM("-multi_level", "Testing production mode...  needs +refpresent_1 & +refpresent_0  VCS arg"),
  PARAM_SUBDECL(lwgpu_single_params),
  LAST_PARAM
};

static int bringup = 0;

static int testhotrst1_flag = 0;
static int testhotrst2_flag = 0;
static int testhotrst3_flag = 0;
static int testhotrst4_flag = 0;
static int testswrst_flag = 0;

// testhotrst1_flag = 0;
// testhotrst2_flag = 0;
// testhotrst3_flag = 0;
// testhotrst4_flag = 0;

HotRstReInit::HotRstReInit(ArgReader *params) : LWGpuSingleChannelTest(params)
{
    if (params->ParamPresent("-bringup"))
        bringup = 1;

    if (params->ParamPresent("-testhotrst1"))
        testhotrst1_flag = 1;

    if (params->ParamPresent("-testhotrst2"))
        testhotrst2_flag = 1;

    if (params->ParamPresent("-testhotrst3"))
        testhotrst3_flag = 1;

    if (params->ParamPresent("-testhotrst4"))
        testhotrst4_flag = 1;

    if (params->ParamPresent("-testswrst"))
        testswrst_flag = 1;
}

HotRstReInit::~HotRstReInit(void)
{
}

STD_TEST_FACTORY(hotrst_reinit, HotRstReInit)

int HotRstReInit::Setup(void) {

    int returlwal = SetupGpuPointers();

    getStateReport()->init("hotrst_reinit");
    getStateReport()->enable();

    return returlwal;
}

int HotRstReInit::SetupGpuPointers(void)
{
    lwgpu = LWGpuResource::FindFirstResource();

    ch = lwgpu->CreateChannel();
    if(!ch)
    {
        ErrPrintf("HotRstReInit::Setup failed to create channel\n");
        return 0;
    }

    m_regMap = lwgpu->GetRegisterMap();
    if(!m_regMap)
    {
        ErrPrintf("HotRstReInit::Setup: can't get a register map from the GPU!\n");
        return 0;
    }

    return 1;
}

void HotRstReInit::CleanUp(void)
{
    if(ch)
    {
        delete ch;
        ch = 0;
    }

    if (lwgpu) {
        DebugPrintf("HotRstReInit::CleanUp(): Releasing GPU.\n");
        lwgpu = 0;
    }
    LWGpuSingleChannelTest::CleanUp();
}

bool HotRstReInit::GetInitValue(IRegisterClass* reg, UINT32 testAddr, UINT32 *initVal,UINT32 *initMask)
{
    unique_ptr<IRegisterField> field;
    unique_ptr<IRegisterValue> value;

    UINT32 tmpVal, bit, startBit, endBit, one = 1;
    bool hasInitValue=false;
    DebugPrintf("CRegTestGpu::GetInitValue: addr = 0x%08x\n", testAddr);

    *initVal=0;
    *initMask=0;
    field = reg->GetFieldHead();
    while(field)
    {
         startBit = field->GetStartBit();
         endBit   = field->GetEndBit();
         DebugPrintf("  Field %s access=%s start_bit=%d end_bit=%d\n", field->GetName(), field->GetAccess(), startBit, endBit);
         value = field->GetValueHead();
         while(value)
         {
             string name(value->GetName());
             tmpVal=value->GetValue();
             DebugPrintf("Value %s val=0x%08x\n", name.c_str(), tmpVal);
             string::size_type pos=name.rfind("_INIT");
             if(pos==name.size()-5)
             {
                 *initVal = *initVal | (tmpVal << startBit);
                 for (bit=startBit; bit<=endBit; bit++)
                 {
                    *initMask =*initMask | (one << bit);
                 }
                 DebugPrintf("accumulate init value =0x%08x\n",  *initVal);
                 DebugPrintf("accumulate init mask  =0x%08x\n",  *initMask);
                 hasInitValue=true;
             }
             value = field->GetValueNext();
         }
         field = reg->GetFieldNext();
    }
    return hasInitValue;
}

bool  HotRstReInit::check_results( UINT32 address, UINT32 expected_data)
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

void HotRstReInit::Run(void) {
    SetStatus(TEST_INCOMPLETE);

    InfoPrintf("About to perform the Hot Rst ReInit test\n");

    if (testhotrst1_flag==1) {
        if(testhotrst1()) {
        SetStatus(TEST_FAILED);
        getStateReport()->runFailed("Failed test\n");
        return;
      }
    }
    if (testhotrst2_flag==1) {
       if(testhotrst2()) {
        SetStatus(TEST_FAILED);
        getStateReport()->runFailed("Failed test\n");
        return;
      }

    }
    if (testhotrst3_flag==1) {
        if(testhotrst3()) {
        SetStatus(TEST_FAILED);
        getStateReport()->runFailed("Failed test\n");
        return;
      }
    }
    if (testhotrst4_flag==1) {
        if(testhotrst4()) {
        SetStatus(TEST_FAILED);
        getStateReport()->runFailed("Failed test\n");
        return;
      }
    }
    if (testswrst_flag==1) {
        if(testswrst()) {
        SetStatus(TEST_FAILED);
        getStateReport()->runFailed("Failed test\n");
        return;
      }
    }
    SetStatus(TEST_SUCCEEDED);
    getStateReport()->crcCheckPassedGold();
}

//GK110,GK20a will by default reset reset the entire GPU upon a SBR deassertion edge.
//A CYA LW_XVE_RESET_CTRL_ASSERT_FUNDAMENTAL_RESET_ON_HOT_RESET can revert the behaviour back to gk10x
void HotRstReInit::disable_sbr_on_hotrst(void)
{
  UINT32 address;
  UINT32 wdata;
  UINT32 init_val;

  address = LW_XVE_RESET_CTRL + DEVICE_BASE(LW_PCFG);
  init_val = lwgpu->GetGpuSubdevice()->RegRd32(address);
  wdata = init_val & ~(0x00000001 << (0 ? LW_XVE_RESET_CTRL_ASSERT_FUNDAMENTAL_RESET_ON_HOT_RESET));
  InfoPrintf("HotRstReInit::disable_sbr_on_hotrst done!\n");

  lwgpu->GetGpuSubdevice()->RegWr32(address,wdata);
}

void HotRstReInit::enable_gpu_hot_rst(void)
{
  UINT32 address;
  UINT32 wdata;
  UINT32 init_val = 0x00000100;
  
 //Bug 2595120: GPU_HOT_RESET was split from LW_XVE_PRIV_MISC for the purpose of PLM protection to LW_XVE_PRIV_MISC_CYA_RESET_GPU
  address = LW_XVE_PRIV_MISC_CYA_RESET_GPU + DEVICE_BASE(LW_PCFG);
  wdata = init_val | (0x00000001 << (0 ? LW_XVE_PRIV_MISC_CYA_RESET_GPU_HOT_RESET));

  lwgpu->GetGpuSubdevice()->RegWr32(address,wdata);
}

void HotRstReInit::enable_cfg_hot_rst(void)
{
  UINT32 address;
  UINT32 wdata;
  UINT32 init_val = 0x00000101;

 //Bug 2595120: CFG_HOT_RESET was split from LW_XVE_PRIV_XV_0 for the purpose of PLM protection to LW_XVE_PRIV_XV_0_CYA_RESET_CFG
  address = LW_XVE_PRIV_XV_0_CYA_RESET_CFG + DEVICE_BASE(LW_PCFG);
  wdata = init_val  | (0x00000001 << (0 ? LW_XVE_PRIV_XV_0_CYA_RESET_CFG_HOT_RESET));
  //| (0x00000001 << (0 ? LW_XVE_PRIV_XV_0_CYA_CFG_DL_UP))

  lwgpu->GetGpuSubdevice()->RegWr32(address, wdata);
}

void HotRstReInit::disable_gpu_hot_rst(void)
{
  UINT32 address;
  UINT32 wdata;
  UINT32 init_val = 0x00000100;

 //Bug 2595120: GPU_HOT_RESET was split from LW_XVE_PRIV_MISC for the purpose of PLM protection to LW_XVE_PRIV_MISC_CYA_RESET_GPU
  address = LW_XVE_PRIV_MISC_CYA_RESET_GPU + DEVICE_BASE(LW_PCFG);
  wdata = init_val & ~(0x00000001 << (0 ? LW_XVE_PRIV_MISC_CYA_RESET_GPU_HOT_RESET));

  lwgpu->GetGpuSubdevice()->RegWr32(address,wdata);
}

void HotRstReInit::disable_cfg_hot_rst(void)
{
  UINT32 address;
  UINT32 wdata;
  UINT32 init_val = 0x00000101;

 //Bug 2595120: CFG_HOT_RESET was split from LW_XVE_PRIV_XV_0 for the purpose of PLM protection to LW_XVE_PRIV_XV_0_CYA_RESET_CFG
  address = LW_XVE_PRIV_XV_0_CYA_RESET_CFG + DEVICE_BASE(LW_PCFG);
  wdata = init_val & ~(0x00000001 << (0 ? LW_XVE_PRIV_XV_0_CYA_RESET_CFG_DL_UP));
  wdata = wdata & ~(0x00000001 << (0 ? LW_XVE_PRIV_XV_0_CYA_RESET_CFG_HOT_RESET));

  lwgpu->GetGpuSubdevice()->RegWr32(address, wdata);
}

void HotRstReInit::disable_fundamental_reset_on_hot_rst (void)
{
  InfoPrintf("Disabling FUNDAMENTAL_RESET_ON_HOT_RESET \n");
  UINT32 read_data;
  UINT32 address;
  UINT32 wdata;

  address = LW_XVE_RESET_CTRL + DEVICE_BASE(LW_PCFG);
  read_data = lwgpu->GetGpuSubdevice()->RegRd32(address);
  wdata = read_data & 0xfffdffff ;
  lwgpu->GetGpuSubdevice()->RegWr32(address,wdata);

}

void HotRstReInit::CorruptRegisters(UINT32 pciDomainNum, UINT32 pciBusNum, UINT32 pciDevNum, UINT32 pciFuncNum)
{
  InfoPrintf("Corrupting some reg values for Hot Rst ReInit test\n");
  
  UINT32 wdata = 0xdeadbeef;
  UINT32 address = 0;
  
  InfoPrintf("write to LW_XTL_EP_PRI_COLD_RESET_SCRATCH\n");
  address = LW_XTL_EP_PRI_COLD_RESET_SCRATCH;
  Platform::PciWrite32(pciDomainNum,
                       pciBusNum,
                       pciDevNum,
                       pciFuncNum,
                       address, wdata);

  InfoPrintf("write to LW_XTL_EP_PRI_HOT_RESET_SCRATCH\n");
  address = LW_XTL_EP_PRI_HOT_RESET_SCRATCH;
  Platform::PciWrite32(pciDomainNum,
                       pciBusNum,
                       pciDevNum,
                       pciFuncNum,
                       address, wdata);

  InfoPrintf("write to LW_PPWR_PMU_DEBUG_TAG\n");
  address = LW_PPWR_PMU_DEBUG_TAG;
  lwgpu->GetGpuSubdevice()->RegWr32(address,wdata);

  //LW_THERM_BOOT_FLAGS[29:0] is spare reserved for future.
  //can't touch 30,31 bit, it triggeres recovery mode.
  address = LW_THERM_BOOT_FLAGS;
  wdata = 0x33aa33aa;
  lwgpu->GetGpuSubdevice()->RegWr32(address,wdata);
}

bool HotRstReInit::CheckRegisterReset(UINT32 pciDomainNum, UINT32 pciBusNum, UINT32 pciDevNum, UINT32 pciFuncNum)
{
  
  UINT32 errCnt = 0;
  UINT32 rdata = 0;
  UINT32 address = 0;
  InfoPrintf("Checking corrupted registers got reset...\n");

  address = LW_XTL_EP_PRI_HOT_RESET_SCRATCH;
  Platform::PciRead32(pciDomainNum,
                       pciBusNum,
                       pciDevNum,
                       pciFuncNum,
                       address, &rdata);
  
  if (rdata != 0x00000000) 
  {
       getStateReport()->runFailed("Failed Self Check Register Read for LW_XTL_EP_PRI_HOT_RESET_SCRATCH\n");
       ErrPrintf("Failed Self Check Reg Read. Expected 0x00000000, Actual 0x%x \n",rdata);
       SetStatus(TEST_FAILED);
       errCnt++;
  }

  address = LW_PPWR_PMU_DEBUG_TAG;
  rdata = lwgpu->GetGpuSubdevice()->RegRd32(address);

  if (rdata != 0x00000000) 
  {
       getStateReport()->runFailed("Failed Self Check Register Read for LW_PPWR_PMU_DEBUG_TAG\n");
       ErrPrintf("Failed Self Check Reg Read. Expected 0x00000000, Actual 0x%x \n",rdata);
       SetStatus(TEST_FAILED);
       errCnt++;
  }
  return errCnt==0;
}

bool HotRstReInit::CheckRegisterNotReset(UINT32 pciDomainNum, UINT32 pciBusNum, UINT32 pciDevNum, UINT32 pciFuncNum)
{
  
  UINT32 errCnt = 0;
  UINT32 rdata = 0;
  UINT32 address = 0;
  InfoPrintf("Checking corrupted registers didn't get reset...\n");
  
  address = LW_XTL_EP_PRI_COLD_RESET_SCRATCH;
  Platform::PciRead32(pciDomainNum,
                       pciBusNum,
                       pciDevNum,
                       pciFuncNum,
                       address, &rdata);
  
  if (rdata != 0xdeadbeef) 
  {
       getStateReport()->runFailed("Failed Self Check Register Read for LW_XTL_EP_PRI_COLD_RESET_SCRATCH\n");
       ErrPrintf("Failed Self Check Reg Read. Expected 0xdeadbeef, Actual 0x%x \n",rdata);
       SetStatus(TEST_FAILED);
       errCnt++;
  }

  address = LW_THERM_BOOT_FLAGS;
  rdata = lwgpu->GetGpuSubdevice()->RegRd32(address);
  
  if (rdata != 0x33aa33aa) 
  {
       getStateReport()->runFailed("Failed Self Check Register Read for LW_THERM_BOOT_FLAGS\n");
       ErrPrintf("Failed Self Check Reg Read. Expected 0xdeadbeef, Actual 0x%x \n",rdata);
       SetStatus(TEST_FAILED);
       errCnt++;
  }
  return errCnt==0;
}

void HotRstReInit::hot_reset(void)
{

  GpuPtr pGpu;

  pGpu = GpuPtr();

  // Release all Mdiag resources, which will be reinitalized
  // in the below ScanSystem call.
  LWGpuResource::FreeResources();

  ////////////////////////////
  // XXX Not clear if a shutdown should be done before hotreset ???
  // lwgpu->GetGpuSubdevice()->Shutdown();
  pGpu->ShutDown(Gpu::ShutDownMode::Normal);

  Platform::SleepUS(150);

  InfoPrintf("Sending HotReset\n");

  ////////////////////////////
  // Send in Hot reset
  Platform::EscapeWrite("doHotReset",0,1,1);
  Platform::Delay(80);

  ////////////////////////////
  // Exit out of Reset
  // May not be necessary
  Platform::EscapeWrite("exitHotReset",0,1,1);
  InfoPrintf("Exiting hot reset ...\n");

  Platform::SleepUS(100);
  //Other delays enforced by HW

}

UINT32 HotRstReInit::gpu_initialize(void)
{
  GpuPtr pGpu;
  JavaScriptPtr pJs;
  RC     rc;

  pGpu = GpuPtr();

  InfoPrintf("Initialize GPU ... ");

  //Only Reinit what is necessary
  CHECK_RC( pJs->SetProperty(Gpu_Object.GetJSObject(), "SkipDevInit", true) );
  CHECK_RC( pJs->SetProperty(Gpu_Object.GetJSObject(), "SkipConfigSim", true) );


  // Reinitialize Gpu
  pGpu->Initialize();
  //Bug 2812901, enabling interrupts after initializing GPU, since HookISR is skipped in skipDevInit
  Platform::EnableInterrupts();

  // Gpu::Shutdown (called in hot_reset) frees all Gpu stuff, thus the
  // entire LW*Resource system in Mdiag needs a rescan to reflect such
  // drastic change. Otherwise, most, if not all, of the pointers would
  // be invalid/dangling. LWGpuResource::ScanSystem does this job
  // well.
  LWGpuResource::ScanSystem(CommandLine::ArgDb());

  Platform::SleepUS(80);

  InfoPrintf("Done!\n");

  //Fetch all the valid pointers for this new gpu, since it was reinitialized
  MASSERT(SetupGpuPointers());

  return 0;
}

UINT32 HotRstReInit::testhotrst1()
{
  GpuSubdevice * pSubdev;
  RC             rc;

  UINT32 errCnt = 0;
  int address = 0;

  uint bar0Temp = 0;
  
  pSubdev = lwgpu->GetGpuSubdevice();

  m_reg = m_regMap->FindRegister("LW_FUSE_FUSETIME_RD1");
  MASSERT(m_reg);
  GetInitValue(m_reg.get(), UINT32(m_reg->GetAddress()), &m_initVal,&m_initMask);

  DebugPrintf("init value =0x%08x\n",  m_initVal);
  DebugPrintf("init mask  =0x%08x\n",  m_initMask);

  ////////////////////////////
  //Save PCI parameters for drv
  
  UINT32 devCtrl = 0; 
  UINT32 revId = 0;
  UINT32 misc1 = 0;
  UINT32 bar0Addr = 0;
  UINT32 irqSet = 0;
  UINT32 vendId = 0;
  UINT32 bar1Lo = 0;
  UINT32 bar1Hi = 0;
  UINT32 bar2Lo = 0;
  UINT32 bar2Hi = 0;
  UINT32 bar3Addr = 0;

  auto pGpuPcie = pSubdev->GetInterface<Pcie>();
  UINT32 pciDomainNum = pGpuPcie->DomainNumber();
  UINT32 pciBusNum = pGpuPcie->BusNumber();
  UINT32 pciDevNum = pGpuPcie->DeviceNumber();
  UINT32 pciFuncNum = pGpuPcie->FunctionNumber();

  //save config space 
  InfoPrintf("Saving config space\n");
  address = LW_EP_PCFG_GPU_CTRL_CMD_AND_STATUS;
  CHECK_RC(Platform::PciRead32(pciDomainNum,
                       pciBusNum,
                       pciDevNum,
                       pciFuncNum,
                       address, &devCtrl));
  address = LW_EP_PCFG_GPU_REVISION_ID_AND_CLASSCODE;
  CHECK_RC(Platform::PciRead32(pciDomainNum,
                       pciBusNum,
                       pciDevNum,
                       pciFuncNum,
                       address, &revId));
  address = LW_EP_PCFG_GPU_CACHE_LATENCY_HEADER;
  CHECK_RC(Platform::PciRead32(pciDomainNum,
                       pciBusNum,
                       pciDevNum,
                       pciFuncNum,
                       address, &misc1));
  address = LW_EP_PCFG_GPU_BARREG0;
  CHECK_RC(Platform::PciRead32(pciDomainNum,
                       pciBusNum,
                       pciDevNum,
                       pciFuncNum,
                       address, &bar0Addr));
  address = LW_EP_PCFG_GPU_MISC;
  CHECK_RC(Platform::PciRead32(pciDomainNum,
                       pciBusNum,
                       pciDevNum,
                       pciFuncNum,
                       address, &irqSet));
  address = LW_EP_PCFG_GPU_SUBSYSTEM_ID;
  CHECK_RC(Platform::PciRead32(pciDomainNum,
                       pciBusNum,
                       pciDevNum,
                       pciFuncNum,
                       address, &vendId));
  address = LW_EP_PCFG_GPU_BARREG1;
  CHECK_RC(Platform::PciRead32(pciDomainNum,
                       pciBusNum,
                       pciDevNum,
                       pciFuncNum,
                       address, &bar1Lo));
  address = LW_EP_PCFG_GPU_BARREG2;
  CHECK_RC(Platform::PciRead32(pciDomainNum,
                       pciBusNum,
                       pciDevNum,
                       pciFuncNum,
                       address, &bar1Hi));
  address = LW_EP_PCFG_GPU_BARREG3;
  CHECK_RC(Platform::PciRead32(pciDomainNum,
                       pciBusNum,
                       pciDevNum,
                       pciFuncNum,
                       address, &bar2Lo));
  address = LW_EP_PCFG_GPU_BARREG4;
  CHECK_RC(Platform::PciRead32(pciDomainNum,
                       pciBusNum,
                       pciDevNum,
                       pciFuncNum,
                       address, &bar2Hi));
  address = LW_EP_PCFG_GPU_BARREG5;
  CHECK_RC(Platform::PciRead32(pciDomainNum,
                       pciBusNum,
                       pciDevNum,
                       pciFuncNum,
                       address, &bar3Addr));


  // ----------------------------------------------------------------------
  //  #1 Hot-reset
  //  ============
  //       LW_XVE_PRIV_MISC_CYA_RESET_GPU_HOT_RESET = 1
  //       LW_XVE_PRIV_XV_0_CYA_RESET_CFG_HOT_RESET = 1
  // ----------------------------------------------------------------------

  InfoPrintf("Now Begin #1 hot-reset\n");
  enable_gpu_hot_rst();
  enable_cfg_hot_rst();
  disable_fundamental_reset_on_hot_rst();

  // Corrupt some registers delibrately
  CorruptRegisters(pciDomainNum, pciBusNum, pciDevNum, pciFuncNum);

  // Apply hot reset
  hot_reset();

//Restore config space
  InfoPrintf("Restoring config space\n");
  address = LW_EP_PCFG_GPU_CTRL_CMD_AND_STATUS;
  CHECK_RC(Platform::PciWrite32(pciDomainNum,
                       pciBusNum,
                       pciDevNum,
                       pciFuncNum,
                       address, devCtrl));
  address = LW_EP_PCFG_GPU_REVISION_ID_AND_CLASSCODE ;
  CHECK_RC(Platform::PciWrite32(pciDomainNum,
                       pciBusNum,
                       pciDevNum,
                       pciFuncNum,
                       address, revId));
  address = LW_EP_PCFG_GPU_CACHE_LATENCY_HEADER;
  CHECK_RC(Platform::PciWrite32(pciDomainNum,
                       pciBusNum,
                       pciDevNum,
                       pciFuncNum,
                       address, misc1));
  address = LW_EP_PCFG_GPU_BARREG0;
  ////////////////////////////
  //Check that Bar0 has been reset
  CHECK_RC(Platform::PciRead32(pciDomainNum,
                       pciBusNum,
                       pciDevNum,
                       pciFuncNum,
                       address, &bar0Temp));

  if ( bar0Temp != 0 ) 
  {
    ErrPrintf("Failed PCI Self Check Reg Read. Expected 0x0, Actual 0x%x\n",bar0Temp);
    errCnt++;
  }

  CHECK_RC(Platform::PciWrite32(pciDomainNum,
                       pciBusNum,
                       pciDevNum,
                       pciFuncNum,
                       address, bar0Addr));
  address = LW_EP_PCFG_GPU_MISC;
  CHECK_RC(Platform::PciWrite32(pciDomainNum,
                       pciBusNum,
                       pciDevNum,
                       pciFuncNum,
                       address, irqSet));
  address = LW_EP_PCFG_GPU_SUBSYSTEM_ID;
  CHECK_RC(Platform::PciWrite32(pciDomainNum,
                       pciBusNum,
                       pciDevNum,
                       pciFuncNum,
                       address, vendId));
  address = LW_EP_PCFG_GPU_BARREG1;
  CHECK_RC(Platform::PciWrite32(pciDomainNum,
                       pciBusNum,
                       pciDevNum,
                       pciFuncNum,
                       address, bar1Lo));
  address = LW_EP_PCFG_GPU_BARREG2;
  CHECK_RC(Platform::PciWrite32(pciDomainNum,
                       pciBusNum,
                       pciDevNum,
                       pciFuncNum,
                       address, bar1Hi));
  address = LW_EP_PCFG_GPU_BARREG3;
  CHECK_RC(Platform::PciWrite32(pciDomainNum,
                       pciBusNum,
                       pciDevNum,
                       pciFuncNum,
                       address, bar2Lo));
  address = LW_EP_PCFG_GPU_BARREG4;
  CHECK_RC(Platform::PciWrite32(pciDomainNum,
                       pciBusNum,
                       pciDevNum,
                       pciFuncNum,
                       address, bar2Hi));
  address = LW_EP_PCFG_GPU_BARREG5;
  CHECK_RC(Platform::PciWrite32(pciDomainNum,
                       pciBusNum,
                       pciDevNum,
                       pciFuncNum,
                       address, bar3Addr));

//  Platform::SleepUS(120);
  Platform::SleepUS(20);
  // ------------------------------
  // Initialize GPU
  // ==============
  gpu_initialize();

  // It looks like this crp module in HOST int takes a while to go idle after being reset ;
  // so we need to provide enough delay else we would get the assert LW_HOST_INT_crp: Must be empty at end of simulation error.
  Platform::Delay(1000);

  // --------------------------------------
  //  Check Registers
  //  ===============

  if (!CheckRegisterReset(pciDomainNum, pciBusNum, pciDevNum, pciFuncNum)) 
      errCnt++;

  // ---------------------------------------------------------
  //  Returning with status
  //

  if (errCnt >0)
  {
    ErrPrintf("testhotrst1: failed with %d errors\n",errCnt);
    return (1);
  } else
  {
    InfoPrintf("testhotrst1: PASSED!\n");
    return (0);
  }
}

UINT32 HotRstReInit::testhotrst2()
{
  GpuSubdevice * pSubdev;
  RC             rc;

  UINT32 errCnt = 0;
  int address = 0;

  uint bar0_temp = 0;

  pSubdev = lwgpu->GetGpuSubdevice();

  m_reg = m_regMap->FindRegister("LW_FUSE_FUSETIME_RD1");
  MASSERT(m_reg);
  GetInitValue(m_reg.get(), UINT32(m_reg->GetAddress()), &m_initVal,&m_initMask);

  DebugPrintf("init value =0x%08x\n",  m_initVal);
  DebugPrintf("init mask  =0x%08x\n",  m_initMask);

  //Save off some config space
  address = DEVICE_BASE(LW_PCFG) + LW_XVE_DEV_CTRL;
  int devctrl = lwgpu->GetGpuSubdevice()->RegRd32(address);
  address = DEVICE_BASE(LW_PCFG) + LW_XVE_REV_ID;
  int rev_id = lwgpu->GetGpuSubdevice()->RegRd32(address);
  address = DEVICE_BASE(LW_PCFG) + LW_XVE_MISC_1;
  int misc_1 = lwgpu->GetGpuSubdevice()->RegRd32(address);
  address = DEVICE_BASE(LW_PCFG) + LW_XVE_BAR0;
  int bar0addr = lwgpu->GetGpuSubdevice()->RegRd32(address);
  address = DEVICE_BASE(LW_PCFG) + LW_XVE_INTR_GNT;
  int irqSet = lwgpu->GetGpuSubdevice()->RegRd32(address);
  address = DEVICE_BASE(LW_PCFG) + LW_XVE_SUBSYSTEM;
  int vendid = lwgpu->GetGpuSubdevice()->RegRd32(address);
  address = DEVICE_BASE(LW_PCFG) + LW_XVE_BAR1_LO;
  int bar1lo = lwgpu->GetGpuSubdevice()->RegRd32(address);
  address = DEVICE_BASE(LW_PCFG) + LW_XVE_BAR1_HI;
  int bar1hi = lwgpu->GetGpuSubdevice()->RegRd32(address);
  address = DEVICE_BASE(LW_PCFG) + LW_XVE_BAR2_LO;
  int bar2lo = lwgpu->GetGpuSubdevice()->RegRd32(address);
  address = DEVICE_BASE(LW_PCFG) + LW_XVE_BAR2_HI;
  int bar2hi = lwgpu->GetGpuSubdevice()->RegRd32(address);
  address = DEVICE_BASE(LW_PCFG) + LW_XVE_BAR3;
  int bar3addr = lwgpu->GetGpuSubdevice()->RegRd32(address);

  ////////////////////////////
  //Save PCI parameters for drv
  //
  auto pGpuPcie = pSubdev->GetInterface<Pcie>();
  UINT32 pci_domain_num = pGpuPcie->DomainNumber();
  UINT32 pci_bus_num = pGpuPcie->BusNumber();
  UINT32 pci_dev_num = pGpuPcie->DeviceNumber();
  UINT32 pci_func_num = pGpuPcie->FunctionNumber();

  // ----------------------------------------------------------------------
  //  #2 Hot-reset
  //  ============
  //       LW_XVE_PRIV_MISC_CYA_RESET_GPU_HOT_RESET = 0
  //       LW_XVE_PRIV_XV_0_CYA_RESET_CFG_HOT_RESET = 1
  // ----------------------------------------------------------------------

  InfoPrintf("Now Begin #2 hot-reset\n");
  disable_gpu_hot_rst();
  enable_cfg_hot_rst();
  disable_fundamental_reset_on_hot_rst();

  // Corrupt some registers delibrately
  CorruptRegisters(pci_domain_num, pci_bus_num, pci_dev_num, pci_func_num);

  // Apply hot reset
  hot_reset();

  //Restore config space
  address = LW_XVE_DEV_CTRL;
  CHECK_RC(Platform::PciWrite32(pci_domain_num,
                       pci_bus_num,
                       pci_dev_num,
                       pci_func_num,
                       address,devctrl));
  address = LW_XVE_REV_ID;
  CHECK_RC(Platform::PciWrite32(pci_domain_num,
                       pci_bus_num,
                       pci_dev_num,
                       pci_func_num,
                       address,rev_id));
  address = LW_XVE_MISC_1;
  CHECK_RC(Platform::PciWrite32(pci_domain_num,
                       pci_bus_num,
                       pci_dev_num,
                       pci_func_num,
                       address,misc_1));

  ////////////////////////////
  //Check that Bar0 has been reset
  address = LW_XVE_BAR0;
  CHECK_RC(Platform::PciRead32(pci_domain_num,
                       pci_bus_num,
                       pci_dev_num,
                       pci_func_num,
                       address,&bar0_temp));

  if ( bar0_temp != 0 ) {
    ErrPrintf("Failed PCI Self Check Reg Read. Expected 0x0, Actual 0x%x\n",bar0_temp);
    errCnt++;
  }

  CHECK_RC(Platform::PciWrite32(pci_domain_num,
                       pci_bus_num,
                       pci_dev_num,
                       pci_func_num,
                       address,bar0addr));
  address = LW_XVE_INTR_GNT;
  CHECK_RC(Platform::PciWrite32(pci_domain_num,
                       pci_bus_num,
                       pci_dev_num,
                       pci_func_num,
                       address,irqSet));
  address = LW_XVE_SUBSYSTEM;
  CHECK_RC(Platform::PciWrite32(pci_domain_num,
                       pci_bus_num,
                       pci_dev_num,
                       pci_func_num,
                       address,vendid));
  address = LW_XVE_BAR1_LO;
  CHECK_RC(Platform::PciWrite32(pci_domain_num,
                       pci_bus_num,
                       pci_dev_num,
                       pci_func_num,
                       address,bar1lo));
  address = LW_XVE_BAR1_HI;
  CHECK_RC(Platform::PciWrite32(pci_domain_num,
                       pci_bus_num,
                       pci_dev_num,
                       pci_func_num,
                       address,bar1hi));
  address = LW_XVE_BAR2_LO;
  CHECK_RC(Platform::PciWrite32(pci_domain_num,
                       pci_bus_num,
                       pci_dev_num,
                       pci_func_num,
                       address,bar2lo));
  address = LW_XVE_BAR2_HI;
  CHECK_RC(Platform::PciWrite32(pci_domain_num,
                       pci_bus_num,
                       pci_dev_num,
                       pci_func_num,
                       address,bar2hi));
  address = LW_XVE_BAR3;
  CHECK_RC(Platform::PciWrite32(pci_domain_num,
                       pci_bus_num,
                       pci_dev_num,
                       pci_func_num,
                       address,bar3addr));

//  Platform::SleepUS(120);
   Platform::SleepUS(20);

  // ------------------------------
  // Initialize GPU
  // ==============

  gpu_initialize();

  //Make sure GR is out of reset.
  //address = LW_PMC_ENABLE;
  //wdata = 0x00101101;
  //lwgpu->GetGpuSubdevice()->RegWr32(address,wdata);

  Platform::Delay(100);

  // ------------------------------
  //  Check Registers
  //

  if (!CheckRegisterNotReset(pci_domain_num, pci_bus_num, pci_dev_num, pci_func_num)) 
      errCnt++;

  // ---------------------------------------------------------
  //  Returning with status
  //
  if (errCnt >0)
  {
    ErrPrintf("testhotrst2: failed with %d errors\n",errCnt);
    return (1);
  } else
  {
    InfoPrintf("testhotrst2: PASSED!\n");
    return (0);
  }
}

UINT32 HotRstReInit::testhotrst3()
{
  GpuSubdevice * pSubdev;
  RC             rc;

  UINT32 errCnt = 0;
  int address = 0;
  uint address_temp =0;
  UINT32 read_data = 0;

  pSubdev = lwgpu->GetGpuSubdevice();

  m_reg = m_regMap->FindRegister("LW_FUSE_FUSETIME_RD1");
  MASSERT(m_reg);
  GetInitValue(m_reg.get(), UINT32(m_reg->GetAddress()), &m_initVal,&m_initMask);

  DebugPrintf("init value =0x%08x\n",  m_initVal);
  DebugPrintf("init mask  =0x%08x\n",  m_initMask);

  ////////////////////////////
  //Save PCI parameters for drv
  //
  //UINT32 pci_bus_num = pSubdev->PciBusNumber();
  //UINT32 pci_dev_num = pSubdev->PciDeviceNumber();
  //UINT32 pci_func_num = pSubdev->PciFunctionNumber();

  address = DEVICE_BASE(LW_PCFG) + LW_XVE_DEV_CTRL;
  read_data = lwgpu->GetGpuSubdevice()->RegRd32(address);
  InfoPrintf("Read 0x%x from LW_XVE_DEV_CTRL address: 0x%x before reset\n", read_data,address);

  address = DEVICE_BASE(LW_PCFG) + LW_XVE_REV_ID;
  read_data = lwgpu->GetGpuSubdevice()->RegRd32(address);
  InfoPrintf("Read 0x%x from LW_XVE_REV_ID address: 0x%x before reset\n", read_data,address);

  address = DEVICE_BASE(LW_PCFG) + LW_XVE_MISC_1;
  read_data = lwgpu->GetGpuSubdevice()->RegRd32(address);
  InfoPrintf("Read 0x%x from LW_XVE_MISC_1 address: 0x%x before reset\n", read_data,address);

  address = DEVICE_BASE(LW_PCFG) + LW_XVE_BAR0;
  read_data = lwgpu->GetGpuSubdevice()->RegRd32(address);
  InfoPrintf("Read 0x%x from LW_XVE_BAR0 address: 0x%x before reset\n", read_data,address);

  address = DEVICE_BASE(LW_PCFG) + LW_XVE_INTR_GNT;
  read_data = lwgpu->GetGpuSubdevice()->RegRd32(address);
  InfoPrintf("Read 0x%x from LW_XVE_INTR_GNT address: 0x%x before reset\n", read_data,address);

  address = DEVICE_BASE(LW_PCFG) + LW_XVE_SUBSYSTEM;
  read_data = lwgpu->GetGpuSubdevice()->RegRd32(address);
  InfoPrintf("Read 0x%x from LW_XVE_SUBSYSTEM address: 0x%x before reset\n", read_data,address);

  address = DEVICE_BASE(LW_PCFG) + LW_XVE_BAR1_LO;
  read_data = lwgpu->GetGpuSubdevice()->RegRd32(address);
  InfoPrintf("Read 0x%x from LW_XVE_BAR1_LO address: 0x%x before reset\n", read_data,address);

  address = DEVICE_BASE(LW_PCFG) + LW_XVE_BAR1_HI;
  read_data = lwgpu->GetGpuSubdevice()->RegRd32(address);
  InfoPrintf("Read 0x%x from LW_XVE_BAR1_HI address: 0x%x before reset\n", read_data,address);

  address = DEVICE_BASE(LW_PCFG) + LW_XVE_BAR2_LO;
  read_data = lwgpu->GetGpuSubdevice()->RegRd32(address);
  InfoPrintf("Read 0x%x from LW_XVE_BAR2_LO address: 0x%x before reset\n", read_data,address);

  address = DEVICE_BASE(LW_PCFG) + LW_XVE_BAR2_HI;
  read_data = lwgpu->GetGpuSubdevice()->RegRd32(address);
  InfoPrintf("Read 0x%x from LW_XVE_BAR2_HI address: 0x%x before reset\n", read_data,address);

  address = DEVICE_BASE(LW_PCFG) + LW_XVE_BAR3;
  read_data = lwgpu->GetGpuSubdevice()->RegRd32(address);
  InfoPrintf("Read 0x%x from LW_XVE_BAR3 address: 0x%x before reset\n", read_data,address);

  address = LW_XVE_PRIV_MISC + DEVICE_BASE(LW_PCFG);
  read_data = lwgpu->GetGpuSubdevice()->RegRd32(address);
  InfoPrintf("Read 0x%x from LW_XVE_PRIV_MISC address: 0x%x before reset\n", read_data,address);

  address = LW_XVE_PRIV_XV_0 + DEVICE_BASE(LW_PCFG);
  read_data = lwgpu->GetGpuSubdevice()->RegRd32(address);
  InfoPrintf("Read 0x%x from LW_XVE_PRIV_XV_0 address: 0x%x before reset\n", read_data,address);

  auto pGpuPcie = pSubdev->GetInterface<Pcie>();
  UINT32 pci_domain_num = pGpuPcie->DomainNumber();
  UINT32 pci_bus_num = pGpuPcie->BusNumber();
  UINT32 pci_dev_num = pGpuPcie->DeviceNumber();
  UINT32 pci_func_num = pGpuPcie->FunctionNumber();

  // ----------------------------------------------------------------------
  //  #3 Hot-reset
  //  ============
  //       LW_XVE_PRIV_MISC_CYA_RESET_GPU_HOT_RESET = 1
  //       LW_XVE_PRIV_XV_0_CYA_RESET_CFG_HOT_RESET = 0
  // ----------------------------------------------------------------------

  InfoPrintf("Now Begin #3 hot-reset\n");
  enable_gpu_hot_rst();
  disable_cfg_hot_rst();
  disable_fundamental_reset_on_hot_rst();

  address = LW_XVE_PRIV_XV_0 + DEVICE_BASE(LW_PCFG);
  read_data = lwgpu->GetGpuSubdevice()->RegRd32(address);
  InfoPrintf("Read 0x%x from LW_XVE_PRIV_XV_0 address: 0x%x before reset\n", read_data,address);

  // Corrupt some registers delibrately
  CorruptRegisters(pci_domain_num, pci_bus_num, pci_dev_num, pci_func_num);

  // Apply hot reset
  hot_reset();

  address = DEVICE_BASE(LW_PCFG) + LW_XVE_DEV_CTRL;
  CHECK_RC(Platform::PciRead32(pci_domain_num,
           pci_bus_num,
           pci_dev_num,
           pci_func_num,
           address, &address_temp));
//  read_data = lwgpu->GetGpuSubdevice()->RegRd32(address);
  InfoPrintf("Read 0x%x from LW_XVE_DEV_CTRL address: 0x%x after reset\n", address_temp,address);

  address = DEVICE_BASE(LW_PCFG) + LW_XVE_REV_ID;
  CHECK_RC(Platform::PciRead32(pci_domain_num,
           pci_bus_num,
           pci_dev_num,
           pci_func_num,
           address, &address_temp));

//  read_data = lwgpu->GetGpuSubdevice()->RegRd32(address);
  InfoPrintf("Read 0x%x from LW_XVE_REV_ID address: 0x%x after reset\n", address_temp,address);

  address = DEVICE_BASE(LW_PCFG) + LW_XVE_MISC_1;
  CHECK_RC(Platform::PciRead32(pci_domain_num,
           pci_bus_num,
           pci_dev_num,
           pci_func_num,
           address, &address_temp));
//  read_data = lwgpu->GetGpuSubdevice()->RegRd32(address);
  InfoPrintf("Read 0x%x from LW_XVE_MISC_1 address: 0x%x after reset\n", address_temp,address);

  address = DEVICE_BASE(LW_PCFG) + LW_XVE_BAR0;
  CHECK_RC(Platform::PciRead32(pci_domain_num,
           pci_bus_num,
           pci_dev_num,
           pci_func_num,
           address, &address_temp));
//  read_data = lwgpu->GetGpuSubdevice()->RegRd32(address);
  InfoPrintf("Read 0x%x from LW_XVE_BAR0 address: 0x%x after reset\n", address_temp,address);

  address = DEVICE_BASE(LW_PCFG) + LW_XVE_INTR_GNT;
  CHECK_RC(Platform::PciRead32(pci_domain_num,
           pci_bus_num,
           pci_dev_num,
           pci_func_num,
           address, &address_temp));
//  read_data = lwgpu->GetGpuSubdevice()->RegRd32(address);
  InfoPrintf("Read 0x%x from LW_XVE_INTR_GNT address: 0x%x after reset\n", address_temp,address);

  address = DEVICE_BASE(LW_PCFG) + LW_XVE_SUBSYSTEM;
  CHECK_RC(Platform::PciRead32(pci_domain_num,
           pci_bus_num,
           pci_dev_num,
           pci_func_num,
           address, &address_temp));
//  read_data = lwgpu->GetGpuSubdevice()->RegRd32(address);
  InfoPrintf("Read 0x%x from LW_XVE_SUBSYSTEM address: 0x%x after reset\n", address_temp,address);

  address = DEVICE_BASE(LW_PCFG) + LW_XVE_BAR1_LO;
  CHECK_RC(Platform::PciRead32(pci_domain_num,
           pci_bus_num,
           pci_dev_num,
           pci_func_num,
           address, &address_temp));
 // read_data = lwgpu->GetGpuSubdevice()->RegRd32(address);
  InfoPrintf("Read 0x%x from LW_XVE_BAR1_LO address: 0x%x after reset\n", address_temp,address);

  address = DEVICE_BASE(LW_PCFG) + LW_XVE_BAR1_HI;
  CHECK_RC(Platform::PciRead32(pci_domain_num,
           pci_bus_num,
           pci_dev_num,
           pci_func_num,
           address, &address_temp));
//  read_data = lwgpu->GetGpuSubdevice()->RegRd32(address);
  InfoPrintf("Read 0x%x from LW_XVE_BAR1_HI address: 0x%x after reset\n", address_temp,address);

  address = DEVICE_BASE(LW_PCFG) + LW_XVE_BAR2_LO;
  CHECK_RC(Platform::PciRead32(pci_domain_num,
           pci_bus_num,
           pci_dev_num,
           pci_func_num,
           address, &address_temp));
//  read_data = lwgpu->GetGpuSubdevice()->RegRd32(address);
  InfoPrintf("Read 0x%x from LW_XVE_BAR2_LO address: 0x%x after reset\n", address_temp,address);

  address = DEVICE_BASE(LW_PCFG) + LW_XVE_BAR2_HI;
  CHECK_RC(Platform::PciRead32(pci_domain_num,
           pci_bus_num,
           pci_dev_num,
           pci_func_num,
           address, &address_temp));
//  read_data = lwgpu->GetGpuSubdevice()->RegRd32(address);
  InfoPrintf("Read 0x%x from LW_XVE_BAR2_HI address: 0x%x after reset\n", address_temp,address);

  address = DEVICE_BASE(LW_PCFG) + LW_XVE_BAR3;
  CHECK_RC(Platform::PciRead32(pci_domain_num,
           pci_bus_num,
           pci_dev_num,
           pci_func_num,
           address, &address_temp));
//  read_data = lwgpu->GetGpuSubdevice()->RegRd32(address);
  InfoPrintf("Read 0x%x from LW_XVE_BAR3 address: 0x%x after reset\n", address_temp,address);

  address = LW_XVE_PRIV_MISC + DEVICE_BASE(LW_PCFG);
  CHECK_RC(Platform::PciRead32(pci_domain_num,
           pci_bus_num,
           pci_dev_num,
           pci_func_num,
           address, &address_temp));
//  read_data = lwgpu->GetGpuSubdevice()->RegRd32(address);
  InfoPrintf("Read 0x%x from LW_XVE_PRIV_MISC address: 0x%x after reset\n", address_temp,address);

  address = LW_XVE_PRIV_XV_0 + DEVICE_BASE(LW_PCFG);
  CHECK_RC(Platform::PciRead32(pci_domain_num,
           pci_bus_num,
           pci_dev_num,
           pci_func_num,
           address, &address_temp));
//  read_data = lwgpu->GetGpuSubdevice()->RegRd32(address);
  InfoPrintf("Read 0x%x from LW_XVE_PRIV_XV_0 address: 0x%x after reset\n", address_temp,address);
  // Skip restore BAR0 here

  // ------------------------------
  // Initialize GPU
  // ==============

  gpu_initialize();

  //Make sure GR is out of reset.
  //address = LW_PMC_ENABLE;
  //wdata = 0x00101101;
  //lwgpu->GetGpuSubdevice()->RegWr32(address,wdata);


  // It looks like this crp module in HOST int takes a while to go idle after being reset ;
  // so we need to provide enough delay else we would get the assert LW_HOST_INT_crp:
  // Must be empty at end of simulation error.
  Platform::Delay(1000);

  // --------------------------------------
  //  Check Registers
  //  ===============

  if (!CheckRegisterReset(pci_domain_num, pci_bus_num, pci_dev_num, pci_func_num)) 
      errCnt++;

  // ---------------------------------------------------------
  //  Returning with status
  //
  if (errCnt >0)
  {
    ErrPrintf("testhotrst3: failed with %d errors\n",errCnt);
    return (1);
  } else
  {
    InfoPrintf("testhotrst3: PASSED!\n");
    return (0);
  }
}

UINT32 HotRstReInit::testhotrst4()
{
  GpuSubdevice * pSubdev;
  RC             rc;

  UINT32 errCnt = 0;
  int address = 0;

  pSubdev = lwgpu->GetGpuSubdevice();

  m_reg = m_regMap->FindRegister("LW_FUSE_FUSETIME_RD1");
  MASSERT(m_reg);
  GetInitValue(m_reg.get(), UINT32(m_reg->GetAddress()), &m_initVal,&m_initMask);

  DebugPrintf("init value =0x%08x\n",  m_initVal);
  DebugPrintf("init mask  =0x%08x\n",  m_initMask);

  //Save off some config space
  address = DEVICE_BASE(LW_PCFG) + LW_XVE_DEV_CTRL;
 // int devctrl = lwgpu->GetGpuSubdevice()->RegRd32(address);
  address = DEVICE_BASE(LW_PCFG) + LW_XVE_REV_ID;
//  int rev_id = lwgpu->GetGpuSubdevice()->RegRd32(address);
  address = DEVICE_BASE(LW_PCFG) + LW_XVE_MISC_1;
 // int misc_1 = lwgpu->GetGpuSubdevice()->RegRd32(address);
  address = DEVICE_BASE(LW_PCFG) + LW_XVE_BAR0;
  int bar0addr = lwgpu->GetGpuSubdevice()->RegRd32(address);
  address = DEVICE_BASE(LW_PCFG) + LW_XVE_INTR_GNT;
  int irqSet = lwgpu->GetGpuSubdevice()->RegRd32(address);
  address = DEVICE_BASE(LW_PCFG) + LW_XVE_SUBSYSTEM;
  int vendid = lwgpu->GetGpuSubdevice()->RegRd32(address);
  address = DEVICE_BASE(LW_PCFG) + LW_XVE_BAR1_LO;
  int bar1lo = lwgpu->GetGpuSubdevice()->RegRd32(address);
  address = DEVICE_BASE(LW_PCFG) + LW_XVE_BAR1_HI;
  int bar1hi = lwgpu->GetGpuSubdevice()->RegRd32(address);
  address = DEVICE_BASE(LW_PCFG) + LW_XVE_BAR2_LO;
  int bar2lo = lwgpu->GetGpuSubdevice()->RegRd32(address);
  address = DEVICE_BASE(LW_PCFG) + LW_XVE_BAR2_HI;
  int bar2hi = lwgpu->GetGpuSubdevice()->RegRd32(address);
  address = DEVICE_BASE(LW_PCFG) + LW_XVE_BAR3;
  int bar3addr = lwgpu->GetGpuSubdevice()->RegRd32(address);

  ////////////////////////////
  //Save PCI parameters for drv
  //
  auto pGpuPcie = pSubdev->GetInterface<Pcie>();
  UINT32 pci_domain_num = pGpuPcie->DomainNumber();
  UINT32 pci_bus_num = pGpuPcie->BusNumber();
  UINT32 pci_dev_num = pGpuPcie->DeviceNumber();
  UINT32 pci_func_num = pGpuPcie->FunctionNumber();

  // ----------------------------------------------------------------------
  //  #4 Hot-reset
  //  ============
  //       LW_XVE_PRIV_MISC_CYA_RESET_GPU_HOT_RESET = 0
  //       LW_XVE_PRIV_XV_0_CYA_RESET_CFG_HOT_RESET = 0
  // ----------------------------------------------------------------------

  InfoPrintf("Now Begin #4 hot-reset\n");
  disable_gpu_hot_rst();
  disable_fundamental_reset_on_hot_rst();
  disable_cfg_hot_rst();

  // Corrupt some registers delibrately
  CorruptRegisters(pci_domain_num, pci_bus_num, pci_dev_num, pci_func_num);

  // Apply hot reset
  hot_reset();

  // restore BAR0 here   added by Eric liang
  address = LW_XVE_BAR0;
  CHECK_RC(Platform::PciWrite32(pci_domain_num,
                       pci_bus_num,
                       pci_dev_num,
                       pci_func_num,
                       address,bar0addr));
  address = LW_XVE_INTR_GNT;
  CHECK_RC(Platform::PciWrite32(pci_domain_num,
                       pci_bus_num,
                       pci_dev_num,
                       pci_func_num,
                       address,irqSet));
  address = LW_XVE_SUBSYSTEM;
  CHECK_RC(Platform::PciWrite32(pci_domain_num,
                       pci_bus_num,
                       pci_dev_num,
                       pci_func_num,
                       address,vendid));
  address = LW_XVE_BAR1_LO;
  CHECK_RC(Platform::PciWrite32(pci_domain_num,
                       pci_bus_num,
                       pci_dev_num,
                       pci_func_num,
                       address,bar1lo));
  address = LW_XVE_BAR1_HI;
  CHECK_RC(Platform::PciWrite32(pci_domain_num,
                       pci_bus_num,
                       pci_dev_num,
                       pci_func_num,
                       address,bar1hi));
  address = LW_XVE_BAR2_LO;
  CHECK_RC(Platform::PciWrite32(pci_domain_num,
                       pci_bus_num,
                       pci_dev_num,
                       pci_func_num,
                       address,bar2lo));
  address = LW_XVE_BAR2_HI;
  CHECK_RC(Platform::PciWrite32(pci_domain_num,
                       pci_bus_num,
                       pci_dev_num,
                       pci_func_num,
                       address,bar2hi));
  address = LW_XVE_BAR3;
  CHECK_RC(Platform::PciWrite32(pci_domain_num,
                       pci_bus_num,
                       pci_dev_num,
                       pci_func_num,
                       address,bar3addr));

//  Platform::SleepUS(120);
   Platform::SleepUS(20);

  // ------------------------------
  // Initialize GPU
  // ==============

  gpu_initialize();

  //Make sure GR is out of reset.
  //address = LW_PMC_ENABLE;
  //wdata = 0x00101101;
  //lwgpu->GetGpuSubdevice()->RegWr32(address,wdata);

  Platform::Delay(50);

  // --------------------------------------
  //  Check Registers
  //  ===============

  if (!CheckRegisterNotReset(pci_domain_num, pci_bus_num, pci_dev_num, pci_func_num)) 
      errCnt++;

  // ---------------------------------------------------------
  //  Returning with status
  //
  if (errCnt >0)
  {
    ErrPrintf("testhotrst4: failed with %d errors\n",errCnt);
    return (1);
  } else
  {
    InfoPrintf("testhotrst4: PASSED!\n");
    return (0);
  }
}

UINT32 HotRstReInit::testswrst()
{
    return (0);
}
