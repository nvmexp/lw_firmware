/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2009-2021 by LWPU Corporation.  All rights 
 * reserved. All information contained herein is proprietary and confidential 
 * to LWPU Corporation.  Any use, reproduction, or disclosure without the 
 * written permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

// To facilitate faster compiles via pre-compiled headers, keep "stdtest.h"
// first.
// For style requirements, all lines of code follow the 80 column rule.
#include "mdiag/tests/stdtest.h"

#include "therm_overt_latch.h"

#include "core/include/gpu.h"
#include "core/include/jscript.h"
#include "device/interface/pcie.h"
#include "gpu/include/gpusbdev.h"
#include "hopper/gh100/dev_xtl_ep_pcfg_gpu.h"
#include "maxwell/gm107/dev_bus.h"
#include "maxwell/gm107/dev_disp_addendum.h"
#include "maxwell/gm107/dev_fuse.h"
#include "maxwell/gm107/dev_graphics_nobundle.h"
#include "maxwell/gm107/dev_host.h"
#include "maxwell/gm107/dev_master.h"
#include "maxwell/gm107/dev_lw_xve.h"
#include "maxwell/gm107/dev_timer.h"
#include "mdiag/lwgpu.h"
#include "mdiag/tests/test_state_report.h"
#include "mdiag/utils/crc.h"
#include "mdiag/utils/lwgpu_channel.h"
#include "pascal/gp100/dev_trim.h"
#include "pascal/gp104/dev_pwr_pri.h"
#include "pascal/gp104/dev_sec_pri.h"
#include "rm.h"

extern const ParamDecl therm_overt_latch_params[] =
{
    SIMPLE_PARAM("-check_legacy_overt_latch",            "Testing the legacy overt latch with sw writes"),
    SIMPLE_PARAM("-check_overt_before_fuse_sense",               "Testing latching of overt before fuse sense"),
    PARAM_SUBDECL(lwgpu_single_params),
    LAST_PARAM
};

static int run_legacy_overt_latch = 0;
static int run_overt_latch_before_fuse_sense = 0;

ThermOvertLatch::ThermOvertLatch(ArgReader *params) :
    LWGpuSingleChannelTest(params)
{
    if  (params->ParamPresent("-check_legacy_overt_latch"))
        run_legacy_overt_latch = 1;
    if  (params->ParamPresent("-check_overt_before_fuse_sense"))
        run_overt_latch_before_fuse_sense = 1;
}

ThermOvertLatch::~ThermOvertLatch(void)
{
}

STD_TEST_FACTORY(therm_overt_latch, ThermOvertLatch)

    INT32
ThermOvertLatch::Setup(void)
{
    lwgpu = LWGpuResource::FindFirstResource();

    ch = lwgpu->CreateChannel();
    if(!ch)
    {
        ErrPrintf("ThermOvertLatch: Setup failed to create channel\n");
        return (0);
    }

    m_regMap = lwgpu->GetRegisterMap();
    if(!m_regMap)
    {
        ErrPrintf("ThermOvertLatch: it can only be run on GPU's that support a "
                "register map!\n");
        return (0);
    }

    getStateReport()->init("therm_overt_latch");
    getStateReport()->enable();

    return 1;
}

    void
ThermOvertLatch::CleanUp(void)
{
    if(ch)
    {
        delete ch;
        ch = 0;
    }

    if (lwgpu)
    {
        DebugPrintf("ThermOvertLatch::CleanUp(): Releasing GPU.\n");
        lwgpu = 0;
    }
    LWGpuSingleChannelTest::CleanUp();
}

    void
ThermOvertLatch::Run(void)
{
    SetStatus(TEST_INCOMPLETE);
    InfoPrintf("Starting ThermOvertLatch test\n");
    if (run_legacy_overt_latch)
    {
        InfoPrintf("Starting checking overt latch enable \n");
        InfoPrintf("Added new PCIE registers \n");
        if(CheckThermOvertLatch())
        {
            SetStatus(TEST_FAILED);
            getStateReport()->runFailed(" Test of check_legacy_overt_latch_enabled failed \n");
            ErrPrintf("therm_overt_latch :: check_legacy_overt_latch_enabled test failed \n");
            return;
        }
    }
    if (run_overt_latch_before_fuse_sense)
    {
        InfoPrintf("Starting checking overt latch before fuse sense \n");
        if(CheckOvertBeforeFuseSense())
        {
            SetStatus(TEST_FAILED);
            getStateReport()->runFailed(" Test of check_overt_before_fuse_sense failed \n");
            ErrPrintf("therm_overt_latch :: check_overt_before_fuse_sense  test failed \n");
            return;
        }
    }

    SetStatus(TEST_SUCCEEDED);
    getStateReport()->crcCheckPassedGold();
}
extern SObject Gpu_Object;



UINT32  
ThermOvertLatch::SavePciReg()
{

    InfoPrintf("failsafe_overt.cpp : Saving PCI config Space registers....");
    GpuSubdevice * pSubdev;
    pSubdev = lwgpu->GetGpuSubdevice();
    
  RC             rc;

    ////////////////////////////
    //Save Off PCI parameters for drv
    //
    auto pGpuPcie = pSubdev->GetInterface<Pcie>();
    pci_domain_num = pGpuPcie->DomainNumber();
    pci_bus_num = pGpuPcie->BusNumber();
    pci_dev_num = pGpuPcie->DeviceNumber();
    pci_func_num = pGpuPcie->FunctionNumber();

    ////////////////////////////
    //Save off some config space
    //
    address = LW_EP_PCFG_GPU_CTRL_CMD_AND_STATUS;
    CHECK_RC(Platform::PciRead32(pci_domain_num,
                pci_bus_num,
                pci_dev_num,
                pci_func_num,
                address,&cmd_status)
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
                address,&cache_lat)
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
                address,&misc)
            );
    address = LW_EP_PCFG_GPU_SUBSYSTEM_ID;
    CHECK_RC(Platform::PciRead32(pci_domain_num,
                pci_bus_num,
                pci_dev_num,
                pci_func_num,
                address,&subsys_id)
            );
    address = LW_EP_PCFG_GPU_BARREG1;
    CHECK_RC(Platform::PciRead32(pci_domain_num,
                pci_bus_num,
                pci_dev_num,
                pci_func_num,
                address,&bar1addr)
            );
    address = LW_EP_PCFG_GPU_BARREG2;
    CHECK_RC(Platform::PciRead32(pci_domain_num,
                pci_bus_num,
                pci_dev_num,
                pci_func_num,
                address,&bar2addr)
            );
    address = LW_EP_PCFG_GPU_BARREG3;
    CHECK_RC(Platform::PciRead32(pci_domain_num,
                pci_bus_num,
                pci_dev_num,
                pci_func_num,
                address,&bar3addr)
            );
    address = LW_EP_PCFG_GPU_BARREG4;
    CHECK_RC(Platform::PciRead32(pci_domain_num,
                pci_bus_num,
                pci_dev_num,
                pci_func_num,
                address,&bar4addr)
            );
    address = LW_EP_PCFG_GPU_BARREG5;
    CHECK_RC(Platform::PciRead32(pci_domain_num,
                pci_bus_num,
                pci_dev_num,
                pci_func_num,
                address,&bar5addr)
            );


    //save PMC_ENABLE bits
    //required for clean exit
    lwgpu->GetRegFldNum("LW_PMC_DEVICE_ENABLE(0)", "", &m_pmc_device_enable);
    lwgpu->GetRegFldNum("LW_PMC_DEVICE_ELPG_ENABLE(0)", "", &m_pmc_device_elpg_enable);
    lwgpu->GetRegFldNum("LW_PMC_ENABLE", "", &m_pmc_enable);
    lwgpu->GetRegFldNum("LW_PMC_ELPG_ENABLE(0)", "", &m_pmc_elpg_enable);
    InfoPrintf("failsafe_overt.cpp : Saved  PCI config Space registers");
	return 1;


}


UINT32  
ThermOvertLatch::RestorePciReg()
{
    InfoPrintf("failsafe_overt.cpp : Restoring PCI config Space registers ....");

  RC             rc;

    ////////////////////////////
    //Restore config space
    //
    address = LW_EP_PCFG_GPU_CTRL_CMD_AND_STATUS;
    CHECK_RC(Platform::PciWrite32(pci_domain_num,
                pci_bus_num,
                pci_dev_num,
                pci_func_num,
                address,cmd_status)
            );
    address = LW_EP_PCFG_GPU_REVISION_ID_AND_CLASSCODE;
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
                address,cache_lat)
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
                address,misc)
            );
    address = LW_EP_PCFG_GPU_SUBSYSTEM_ID;
    CHECK_RC(Platform::PciWrite32(pci_domain_num,
                pci_bus_num,
                pci_dev_num,
                pci_func_num,
                address,subsys_id)
            );
    address = LW_EP_PCFG_GPU_BARREG1;
    CHECK_RC(Platform::PciWrite32(pci_domain_num,
                pci_bus_num,
                pci_dev_num,
                pci_func_num,
                address,bar1addr)
            );
    address = LW_EP_PCFG_GPU_BARREG2;
    CHECK_RC(Platform::PciWrite32(pci_domain_num,
                pci_bus_num,
                pci_dev_num,
                pci_func_num,
                address,bar2addr)
            );
    address = LW_EP_PCFG_GPU_BARREG3;
    CHECK_RC(Platform::PciWrite32(pci_domain_num,
                pci_bus_num,
                pci_dev_num,
                pci_func_num,
                address,bar3addr)
            );
    address = LW_EP_PCFG_GPU_BARREG4;
    CHECK_RC(Platform::PciWrite32(pci_domain_num,
                pci_bus_num,
                pci_dev_num,
                pci_func_num,
                address,bar4addr)
            );
    address = LW_EP_PCFG_GPU_BARREG5;
    CHECK_RC(Platform::PciWrite32(pci_domain_num,
                pci_bus_num,
                pci_dev_num,
                pci_func_num,
                address,bar5addr)
            );
    InfoPrintf("failsafe_overt.cpp : Restored PCI config Space registers");
	return 1;
}







    UINT32
ThermOvertLatch::CheckOvertBeforeFuseSense()
{
    GpuPtr pGpu;

     SavePciReg();
  

    m_external_overt =0;
    InfoPrintf("Set external overt through escape write  \n");
    Platform::EscapeWrite("external_overt",0,1,0x1);
    Platform::Delay(10);
    Platform::EscapeRead("external_overt",0,1,&m_external_overt);
    InfoPrintf(" external_overt = 0x%x.\n",m_external_overt);

    data=0;
    Platform::EscapeRead("latch_overt", 0, 1, &data);
    if (data != 1) {
        InfoPrintf("1 : latch_overt should be 1, but now it's 0x%x \n", data);
        return (1);
    }

    data=1;
    Platform::EscapeRead("overt", 0, 1, &data);
    if (data != 0) {
        InfoPrintf("2 : overt pad should be 0, but now it's 0x%x \n", data);
        return (1);
    }

    InfoPrintf("deassert external overt through escape write  \n");
    Platform::EscapeWrite("external_overt",0,1,0x0);
    Platform::Delay(10);
    Platform::EscapeRead("external_overt",0,1,&m_external_overt);
    InfoPrintf(" external_overt = 0x%x.\n",m_external_overt);

    data=0;
    Platform::EscapeRead("latch_overt", 0, 1, &data);
    if (data != 1) {
        InfoPrintf("3 : latch_overt should be 1, but now it's 0x%x \n", data);
        return (1);
    }

    data=1;
    Platform::EscapeRead("overt", 0, 1, &data);
    if (data != 0) {
        InfoPrintf("4 : overt pad should be 0, but now it's 0x%x \n", data);
        return (1);
    }

    Platform::EscapeWrite("pex_reset_",0,1,0);
    Platform::Delay(50);

    data=1;
    Platform::EscapeRead("latch_overt", 0, 1, &data);
    if (data != 0) {
        InfoPrintf("5 : latch_overt should be 0, but now it's 0x%x \n", data);
        return (1);
    }

    data=0;
    Platform::EscapeRead("overt", 0, 1, &data);
    if (data != 1) {
        InfoPrintf("6 : overt pad should be 1, but now it's 0x%x \n", data);
        return (1);
    }

     Platform::Delay(5);

    Platform::EscapeWrite("pex_reset_",0,1,1);
    Platform::Delay(50);
    RestorePciReg();
   
    Platform::Delay(30);

    // ------------------------------
    // Initialize GPU
    // ==============
    gpu_initialize();

    //Make sure GR is out of reset.
    //address = LW_PMC_ENABLE;
    //wdata = 0x00101101;
    //lwgpu->GetGpuSubdevice()->RegWr32(address,wdata);
    enable_gr_pfifo_engine();

    Platform::Delay(1000);

    return(0);
}

    UINT32
ThermOvertLatch::CheckThermOvertLatch()
{
    m_overt_enable=0;
    lwgpu->GetRegFldNum("LW_PGC6_SCI_FS_OVERT_PADCTL","_CFG_FAILSAFE",&m_overt_enable);
    InfoPrintf("Set LW_PGC6_SCI_FS_OVERT_PADCTL_CFG_FAILSAFE = 0x%x.\n",m_overt_enable);
    m_SCI_SHUTDOWN_LATCH =0;
    lwgpu->SetRegFldDef("LW_PGC6_SCI_FS_OVERT_PADCTL","_CFG_LATCH","_DISABLE");
    Platform::Delay(10);
    lwgpu->GetRegFldNum("LW_PGC6_SCI_FS_OVERT_PADCTL","_CFG_LATCH",&m_SCI_SHUTDOWN_LATCH);
    InfoPrintf("LW_PGC6_SCI_FS_OVERT_PADCTL_CFG_LATCH = 0x%x\n",m_SCI_SHUTDOWN_LATCH);
    GpuPtr pGpu;

    SavePciReg();
   
    m_external_overt =0;
    InfoPrintf("Set external overt through escape write  \n");
    Platform::EscapeWrite("external_overt",0,1,0x1);
    Platform::Delay(10);
    Platform::EscapeRead("external_overt",0,1,&m_external_overt);
    InfoPrintf(" external_overt = 0x%x.\n",m_external_overt);

    InfoPrintf("deassert external overt through escape write  \n");
    Platform::EscapeWrite("external_overt",0,1,0x0);
    Platform::Delay(10);
    Platform::EscapeRead("external_overt",0,1,&m_external_overt);
    InfoPrintf(" external_overt = 0x%x.\n",m_external_overt);

    data=0;
    Platform::EscapeRead("overt_detect", 0, 1, &data);
    if (data != 1) {
        InfoPrintf("1 : overt_detect pad should be 1, but now it's 0x%x \n", data);
        return (1);
    }
    Platform::EscapeWrite("pex_reset_",0,1,0);
    Platform::Delay(50);

    Platform::EscapeWrite("pex_reset_",0,1,1);
    Platform::Delay(50);

    RestorePciReg();
     Platform::Delay(30);

    // ------------------------------
    // Initialize GPU
    // ==============
    gpu_initialize();

    //Make sure GR is out of reset.
    //address = LW_PMC_ENABLE;
    //wdata = 0x00101101;
    //lwgpu->GetGpuSubdevice()->RegWr32(address,wdata);
    enable_gr_pfifo_engine();

    Platform::Delay(1000);

    /*lwgpu->SetRegFldDef("LW_PGC6_SCI_FS_OVERT_PADCTL","_CFG_LATCH","_ENABLE");
    Platform::Delay(10);
    lwgpu->GetRegFldNum("LW_PGC6_SCI_FS_OVERT_PADCTL","_CFG_LATCH",&m_SCI_SHUTDOWN_LATCH);
    InfoPrintf("LW_PGC6_SCI_FS_OVERT_PADCTL_CFG_LATCH = 0x%x\n",m_SCI_SHUTDOWN_LATCH);

    InfoPrintf("Set external overt through escape write  \n");
    Platform::EscapeWrite("external_overt",0,1,0x1);
    Platform::Delay(10);
    Platform::EscapeRead("external_overt",0,1,&m_external_overt);
    InfoPrintf(" external_overt = 0x%x.\n",m_external_overt);

    InfoPrintf("deassert external overt through escape write  \n");
    Platform::EscapeWrite("external_overt",0,1,0x0);
    Platform::Delay(10);
    Platform::EscapeRead("external_overt",0,1,&m_external_overt);
    InfoPrintf(" external_overt = 0x%x.\n",m_external_overt);

    data=0;
    Platform::EscapeRead("overt_detect", 0, 1, &data);
    if (data != 0) {
        InfoPrintf("2 : overt_detect pad should be 0, but now it's 0x%x \n", data);
        return (1);
    }*/

    return(0);

}

UINT32
ThermOvertLatch::gpu_initialize()
{
    GpuPtr pGpu;
    JavaScriptPtr pJs;
    RC     rc;

    pGpu = GpuPtr();

    InfoPrintf("Initialize GPU ... ");

    //Only Reinit what is necessary
    CHECK_RC( pJs->SetProperty(Gpu_Object.GetJSObject(), "SkipDevInit", true) );
    CHECK_RC( pJs->SetProperty(Gpu_Object.GetJSObject(), "SkipConfigSim", true) );

    // Release all Mdiag resources, which will be reinitalized
    // in the below ScanSystem call.
    LWGpuResource::FreeResources();

    // Reinitialize Gpu
    pGpu->Initialize();

    // Gpu::Shutdown (called in hot_reset) frees all Gpu stuff, thus the
    // entire LW*Resource system in Mdiag needs a rescan to reflect such
    // drastic change. Otherwise, most, if not all, of the pointers would
    // be invalid/dangling. ResourceController::ScanSystem does this job
    // well.
    LWGpuResource::ScanSystem(CommandLine::ArgDb());

    Platform::SleepUS(80);

    InfoPrintf("Done!\n");

    //Fetch all the valid pointers for this new gpu, since it was reinitialized
    MASSERT(SetupGpuPointers());

    return 0;
}

INT32
ThermOvertLatch::SetupGpuPointers(void)
{
    lwgpu = LWGpuResource::FindFirstResource();

    ch = lwgpu->CreateChannel();
    if(!ch)
    {
        ErrPrintf("Setup failed to create channel\n");
        return 0;
    }

    m_regMap = lwgpu->GetRegisterMap();
    if(!m_regMap)
    {
        ErrPrintf("Setup: can't get a register map from the GPU!\n");
        return 0;
    }

    return 1;
}

void
ThermOvertLatch::enable_gr_pfifo_engine()
{
    InfoPrintf("Enabling GR/Pfifo engine \n");
    
    //Bars are setup
    //restore PMC_ENABLE bits
    lwgpu->SetRegFldNum("LW_PMC_DEVICE_ENABLE(0)", "", m_pmc_device_enable);
    lwgpu->SetRegFldNum("LW_PMC_DEVICE_ELPG_ENABLE(0)", "", m_pmc_device_elpg_enable);
    lwgpu->SetRegFldNum("LW_PMC_ENABLE", "", m_pmc_enable);
    lwgpu->SetRegFldNum("LW_PMC_ELPG_ENABLE(0)", "", m_pmc_elpg_enable);
    
}


