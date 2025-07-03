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

#include "failsafe_overt.h"

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

extern const ParamDecl failsafe_overt_params[] =
{
    SIMPLE_PARAM("-check_overt_xtalclk_gated", "Testing the faisafe overt when xtalxlk is gated"),
    SIMPLE_PARAM("-check_pex_reset", "Testing failsafe overt with pex reset asserted"),
    SIMPLE_PARAM("-check_pex_reset_fuses_sensed_hightemp", "Testing failsafe overt with pex reset asserted and after fuse sensing"),
    SIMPLE_PARAM("-check_pex_reset_fuses_sensed_lowtemp","Testing failsafe overt with pex reset asserted and after fuse sensing"),
    SIMPLE_PARAM("-check_pex_reset_fuses_not_sensed", "Testing failsafe overt with pex reset asserted and before fuse are sensed"),
    SIMPLE_PARAM("-check_fundamental_reset", "Testing failsafe overt with fundamental reset asserted"),
    SIMPLE_PARAM("-check_fundamental_reset_fuses_sensed_hightemp","Testing failsafe overt with fundamental reset asserted and after fuse sensing"),
    SIMPLE_PARAM("-check_fundamental_reset_fuses_sensed_lowtemp","Testing failsafe overt with fundamental reset asserted and after fuse sensing"),
    SIMPLE_PARAM("-check_fundamental_reset_fuses_not_sensed", "Testing failsafe overt with fundamental reset asserted and before fuse are sensed"),
    SIMPLE_PARAM("-check_sw_reset", "Testing failsafe overt with sw reset asserted"),
    SIMPLE_PARAM("-check_sw_reset_fuses_sensed_hightemp", "Testing failsafe overt with sw reset asserted and fuse values"),
    SIMPLE_PARAM("-check_sw_reset_fuses_sensed_lowtemp", "Testing failsafe overt with sw reset asserted and fuse values"),
    SIMPLE_PARAM("-check_hot_reset", "Testing failsafe overt with hot reset asserted"),
    SIMPLE_PARAM("-check_hot_reset_fuses_sensed_hightemp", "Testing failsafe overt with hot reset asserted and fuse values"),
    SIMPLE_PARAM("-check_hot_reset_fuses_sensed_lowtemp", "Testing failsafe overt with hot reset asserted and fuse values"),
    UNSIGNED_PARAM("-adc_out_delay","delay for wait of TS_ADC_OUT change"),
    UNSIGNED_PARAM("-bg_temp_coeff","bg temp coeff value for test expectation"),
    UNSIGNED_PARAM("-bg_voltage","bg_voltage value for test expectation"),
    UNSIGNED_PARAM("-pcie_gen","pcie gen for save and restore of registers, 5 for gen5 and 4 for gen4"),
    PARAM_SUBDECL(lwgpu_single_params),
    LAST_PARAM
};

static int testovert_xtalclk_gated = 0;
static int testpex_rst=0;
static int testpex_rst_fuses_sensed_hightemp=0;
static int testpex_rst_fuses_sensed_lowtemp=0;
static int testpex_rst_fuses_not_sensed=0;
static int testfundamental_rst=0;
static int testfundamental_rst_fuses_not_sensed=0;
static int testfundamental_rst_fuses_sensed_hightemp=0;
static int testfundamental_rst_fuses_sensed_lowtemp=0;
static int testsw_rst=0;
static int testsw_rst_fuses_hightemp=0;
static int testsw_rst_fuses_lowtemp=0;
static int testhot_rst=0;
static int testhot_rst_fuses_hightemp=0;
static int testhot_rst_fuses_lowtemp=0;
unsigned int adc_out_delay = 200;
unsigned int bg_temp_coeff = 2; //LW_PGC6_SCI_FS_OVERT_FUSE_BG_TEMP_COEFF
unsigned int bg_voltage = 0x58; //LW_PGC6_SCI_FS_OVERT_FUSE_
unsigned int pcie_gen = 5; 

FailSafeOvertPin::FailSafeOvertPin(ArgReader *params) :
    LWGpuSingleChannelTest(params)
{
    if  (params->ParamPresent("-check_overt_xtalclk_gated"))
        testovert_xtalclk_gated= 1;
    if  (params->ParamPresent("-check_pex_reset"))
        testpex_rst=1;
    if  (params->ParamPresent("-check_pex_reset_fuses_sensed_hightemp"))
        testpex_rst_fuses_sensed_hightemp=1;
    if  (params->ParamPresent("-check_pex_reset_fuses_sensed_lowtemp"))
        testpex_rst_fuses_sensed_lowtemp=1;
    if  (params->ParamPresent("-check_pex_reset_fuses_not_sensed"))
        testpex_rst_fuses_not_sensed=1;
    if  (params->ParamPresent("-check_fundamental_reset"))
        testfundamental_rst=1;
    if  (params->ParamPresent("-check_fundamental_reset_fuses_not_sensed"))
        testfundamental_rst_fuses_not_sensed=1;
    if  (params->ParamPresent("-check_fundamental_reset_fuses_sensed_hightemp"))
        testfundamental_rst_fuses_sensed_hightemp=1;
    if  (params->ParamPresent("-check_fundamental_reset_fuses_sensed_lowtemp"))
        testfundamental_rst_fuses_sensed_lowtemp=1;
    if  (params->ParamPresent("-check_sw_reset"))
        testsw_rst=1;
    if  (params->ParamPresent("-check_sw_reset_fuses_sensed_hightemp"))
        testsw_rst_fuses_hightemp=1;
    if  (params->ParamPresent("-check_sw_reset_fuses_sensed_lowtemp"))
        testsw_rst_fuses_lowtemp=1;
    if  (params->ParamPresent("-check_hot_reset"))
        testhot_rst=1;
    if  (params->ParamPresent("-check_hot_reset_fuses_sensed_hightemp"))
        testhot_rst_fuses_hightemp=1;
    if  (params->ParamPresent("-check_hot_reset_fuses_sensed_lowtemp"))
        testhot_rst_fuses_lowtemp=1;
    if  (params->ParamPresent("-adc_out_delay"))
     	adc_out_delay = params->ParamUnsigned("-adc_out_delay",200);
    if  (params->ParamPresent("-bg_temp_coeff"))
     	bg_temp_coeff = params->ParamUnsigned("-bg_temp_coeff",2);
    if  (params->ParamPresent("-bg_voltage"))
     	bg_voltage = params->ParamUnsigned("-bg_voltage",0x58);
    if  (params->ParamPresent("-pcie_gen"))
     	pcie_gen = params->ParamUnsigned("-pcie_gen",5);
	

}

FailSafeOvertPin::~FailSafeOvertPin(void)
{
}

STD_TEST_FACTORY(failsafe_overt, FailSafeOvertPin)

    INT32
FailSafeOvertPin::Setup(void)
{
    int returlwal = SetupGpuPointers();
    getStateReport()->init("failsafe_overt");
    getStateReport()->enable();

    return returlwal;
}
    INT32
FailSafeOvertPin::SetupGpuPointers(void)
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
FailSafeOvertPin::CleanUp(void)
{
    if(ch)
    {
        delete ch;
        ch = 0;
    }

    if (lwgpu)
    {
        DebugPrintf("FailSafeOvertPin::CleanUp(): Releasing GPU.\n");
        lwgpu = 0;
    }
    LWGpuSingleChannelTest::CleanUp();
}

    void
FailSafeOvertPin::Run(void)
{
    SetStatus(TEST_INCOMPLETE);
    if (testovert_xtalclk_gated)
    {
        InfoPrintf("Starting FailSafeOvert verif by gating XTAL clock \n");
        if(CheckXtalClockGate())
        {
            SetStatus(TEST_FAILED);
            getStateReport()->runFailed("Failed test\n");
            return;
        }
    }
    InfoPrintf("Starting FailSafeOvertPin test\n");
    if (testpex_rst)
    {
        InfoPrintf("Starting Failsafe OVERT verification during PEX reset \n");
        if(CheckPexReset())
        {
            SetStatus(TEST_FAILED);
            getStateReport()->runFailed("Failed test\n");
            return;
        }
    }
    if (testpex_rst_fuses_sensed_hightemp)
    {
        InfoPrintf("Starting Failsafe OVERT verification during PEX reset with A_adj and B_adj fuse values\n");
        if(CheckPexResetFusesSensedHighTemp())
        {
            SetStatus(TEST_FAILED);
            getStateReport()->runFailed("Failed test\n");
            return;
        }
    }
    if (testpex_rst_fuses_sensed_lowtemp)
    {
        InfoPrintf("Starting Failsafe OVERT verification during PEX reset with A_adj and B_adj fuse values\n");
        if(CheckPexResetFusesSensedLowTemp())
        {
            SetStatus(TEST_FAILED);
            getStateReport()->runFailed("Failed test\n");
            return;
        }
    }
    if (testpex_rst_fuses_not_sensed)
    {
        InfoPrintf("Starting Failsafe OVERT verification during PEX reset before fuse sensing\n");
        if(CheckPexResetFusesNotSensed())
        {
            SetStatus(TEST_FAILED);
            getStateReport()->runFailed("Failed test\n");
            return;
        }
    }
    if (testfundamental_rst)
    {
        InfoPrintf("Starting Failsafe OVERT verification during PEX reset \n");
        if(CheckFundamentalReset())
        {
            SetStatus(TEST_FAILED);
            getStateReport()->runFailed("Failed test\n");
            return;
        }
    }
    if (testfundamental_rst_fuses_sensed_hightemp)
    {
        InfoPrintf("Starting Failsafe OVERT verification during PEX reset and after fuses are sensed\n");
        if(CheckFundamentalResetFusesSensedHighTemp())
        {
            SetStatus(TEST_FAILED);
            getStateReport()->runFailed("Failed test\n");
            return;
        }
    }
    if (testfundamental_rst_fuses_sensed_lowtemp)
    {
        InfoPrintf("Starting Failsafe OVERT verification during PEX reset and after fuses are sensed\n");
        if(CheckFundamentalResetFusesSensedLowTemp())
        {
            SetStatus(TEST_FAILED);
            getStateReport()->runFailed("Failed test\n");
            return;
        }
    }
    if (testfundamental_rst_fuses_not_sensed)
    {
        InfoPrintf("Starting Failsafe OVERT verification during PEX reset before fuses are sensed\n");
        if(CheckFundamentalResetFusesNotSensed())
        {
            SetStatus(TEST_FAILED);
            getStateReport()->runFailed("Failed test\n");
            return;
        }
    }
    if (testsw_rst)
    {
        InfoPrintf("Starting Failsafe OVERT verification during SW reset \n");
        if(CheckSoftRest())
        {
            SetStatus(TEST_FAILED);
            getStateReport()->runFailed("Failed test\n");
            return;
        }
    }
    if (testsw_rst_fuses_hightemp)
    {
        InfoPrintf("Starting Failsafe OVERT verification during SW reset and fuses\n");
        if(CheckSoftRestFusesSensedHighTemp())
        {
            SetStatus(TEST_FAILED);
            getStateReport()->runFailed("Failed test\n");
            return;
        }
    }
    if (testsw_rst_fuses_lowtemp)
    {
        InfoPrintf("Starting Failsafe OVERT verification during SW reset and fuses\n");
        if(CheckSoftRestFusesSensedLowTemp())
        {
            SetStatus(TEST_FAILED);
            getStateReport()->runFailed("Failed test\n");
            return;
        }
    }
    if (testhot_rst)
    {
        InfoPrintf("Starting Failsafe OVERT verification during HOT reset \n");
        if(CheckHotRest())
        {
            SetStatus(TEST_FAILED);
            getStateReport()->runFailed("Failed test\n");
            return;
        }
    }
    if (testhot_rst_fuses_hightemp)
    {
        InfoPrintf("Starting Failsafe OVERT verification during HOT reset and fuses\n");
        if(CheckHotRestFusesSensedHighTemp())
        {
            SetStatus(TEST_FAILED);
            getStateReport()->runFailed("Failed test\n");
            return;
        }
    }
    if (testhot_rst_fuses_lowtemp)
    {
        InfoPrintf("Starting Failsafe OVERT verification during HOT reset and fuses\n");
        if(CheckHotRestFusesSensedLowTemp())
        {
            SetStatus(TEST_FAILED);
            getStateReport()->runFailed("Failed test\n");
            return;
        }
    }
    SetStatus(TEST_SUCCEEDED);
    getStateReport()->crcCheckPassedGold();
}
//TODO:use only one function for callwlating raw code and replace constants(16384 and 8192) in expression with defines
UINT32
FailSafeOvertPin::temperature2code_final(INT32 temperature)
{
    lwgpu->GetRegFldNum("LW_PGC6_SCI_FS_OVERT_TEMP_COEFF_FINAL","_A",&final_a);
    InfoPrintf("value of final_a= 0x%x.\n",final_a);
    lwgpu->GetRegFldNum("LW_PGC6_SCI_FS_OVERT_TEMP_COEFF_FINAL","_B",&final_b);
    InfoPrintf("value of final_b= 0x%x.\n",final_b);
    int numerator = temperature * 16384 + final_b * 8192;
    int denominator = final_a;
    UINT32 raw_code = (numerator / denominator) + ((numerator % denominator) != 0);
    InfoPrintf("temp %d, code %x\n", temperature, raw_code);
    return raw_code;
}
UINT32
FailSafeOvertPin::temperature2code_nominal(INT32 temperature)
{
    lwgpu->GetRegFldNum("LW_PGC6_SCI_FS_OVERT_TEMP_COEFF_NOM","_A",&nominal_a);
    InfoPrintf("value of nominal_a= 0x%x.\n",nominal_a);
    lwgpu->GetRegFldNum("LW_PGC6_SCI_FS_OVERT_TEMP_COEFF_NOM","_B",&nominal_b);
    InfoPrintf("value of nominal_b= 0x%x.\n",nominal_b);
    int numerator = temperature * 16384 + nominal_b * 8192;
    int denominator = nominal_a;
    UINT32 raw_code = (numerator / denominator) + ((numerator % denominator) != 0);
    InfoPrintf("temp %d, code %x\n", temperature, raw_code);
    return raw_code;
}

UINT32  
FailSafeOvertPin::SavePciReg(UINT32 pcie_gen)
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

  if (pcie_gen == 4) {
    //PCIE GEN 4 code
    ////////////////////////////
    //Save off some config space
    //
    address = DEVICE_BASE(LW_PCFG) + LW_XVE_DEV_CTRL;
    devctrl = lwgpu->GetGpuSubdevice()->RegRd32(address);
    address = DEVICE_BASE(LW_PCFG) + LW_XVE_REV_ID;
    rev_id = lwgpu->GetGpuSubdevice()->RegRd32(address);
    address = DEVICE_BASE(LW_PCFG) + LW_XVE_MISC_1;
    misc_1 = lwgpu->GetGpuSubdevice()->RegRd32(address);
    address = DEVICE_BASE(LW_PCFG) + LW_XVE_BAR0;
    bar0addr = lwgpu->GetGpuSubdevice()->RegRd32(address);
    address = DEVICE_BASE(LW_PCFG) + LW_XVE_INTR_GNT;
    irqSet = lwgpu->GetGpuSubdevice()->RegRd32(address);
    address = DEVICE_BASE(LW_PCFG) + LW_XVE_SUBSYSTEM;
    vendid = lwgpu->GetGpuSubdevice()->RegRd32(address);
    address = DEVICE_BASE(LW_PCFG) + LW_XVE_BAR1_LO;
    bar1lo = lwgpu->GetGpuSubdevice()->RegRd32(address);
    address = DEVICE_BASE(LW_PCFG) + LW_XVE_BAR1_HI;
    bar1hi = lwgpu->GetGpuSubdevice()->RegRd32(address);
    address = DEVICE_BASE(LW_PCFG) + LW_XVE_BAR2_LO;
    bar2lo = lwgpu->GetGpuSubdevice()->RegRd32(address);
    address = DEVICE_BASE(LW_PCFG) + LW_XVE_BAR2_HI;
    bar2hi = lwgpu->GetGpuSubdevice()->RegRd32(address);
    address = DEVICE_BASE(LW_PCFG) + LW_XVE_BAR3;
    bar3addr = lwgpu->GetGpuSubdevice()->RegRd32(address);
 
  } else if (pcie_gen == 5) {
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
  }

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
FailSafeOvertPin::RestorePciReg(UINT32 pcie_gen)
{
    InfoPrintf("failsafe_overt.cpp : Restoring PCI config Space registers ....");

    RC             rc;

  if (pcie_gen == 4) {
    //PCIE Gen4  changes
    address = LW_XVE_DEV_CTRL;
    CHECK_RC(Platform::PciWrite32(pci_domain_num,
                pci_bus_num,
                pci_dev_num,
                pci_func_num,
                address,devctrl)
            );
    address = LW_XVE_REV_ID;
    CHECK_RC(Platform::PciWrite32(pci_domain_num,
                pci_bus_num,
                pci_dev_num,
                pci_func_num,
                address,rev_id)
            );
    address = LW_XVE_MISC_1;
    CHECK_RC(Platform::PciWrite32(pci_domain_num,
                pci_bus_num,
                pci_dev_num,
                pci_func_num,
                address,misc_1)
            );
    address = LW_XVE_BAR0;
    CHECK_RC(Platform::PciWrite32(pci_domain_num,
                pci_bus_num,
                pci_dev_num,
                pci_func_num,
                address,bar0addr)
            );
    address = LW_XVE_INTR_GNT;
    CHECK_RC(Platform::PciWrite32(pci_domain_num,
                pci_bus_num,
                pci_dev_num,
                pci_func_num,
                address,irqSet)
            );
    address = LW_XVE_SUBSYSTEM;
    CHECK_RC(Platform::PciWrite32(pci_domain_num,
                pci_bus_num,
                pci_dev_num,
                pci_func_num,
                address,vendid)
            );
    address = LW_XVE_BAR1_LO;
    CHECK_RC(Platform::PciWrite32(pci_domain_num,
                pci_bus_num,
                pci_dev_num,
                pci_func_num,
                address,bar1lo)
            );
    address = LW_XVE_BAR1_HI;
    CHECK_RC(Platform::PciWrite32(pci_domain_num,
                pci_bus_num,
                pci_dev_num,
                pci_func_num,
                address,bar1hi)
            );
    address = LW_XVE_BAR2_LO;
    CHECK_RC(Platform::PciWrite32(pci_domain_num,
                pci_bus_num,
                pci_dev_num,
                pci_func_num,
                address,bar2lo)
            );
    address = LW_XVE_BAR2_HI;
    CHECK_RC(Platform::PciWrite32(pci_domain_num,
                pci_bus_num,
                pci_dev_num,
                pci_func_num,
                address,bar2hi)
            );
    address = LW_XVE_BAR3;
    CHECK_RC(Platform::PciWrite32(pci_domain_num,
                pci_bus_num,
                pci_dev_num,
                pci_func_num,
                address,bar3addr)
            );
    InfoPrintf("failsafe_overt.cpp : Restored PCI GEN4 config Space registers");
  } else if (pcie_gen == 5) {
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
    InfoPrintf("failsafe_overt.cpp : Restored PCI GEN5 config Space registers");
  }
    InfoPrintf("failsafe_overt.cpp : Restored PCI config Space registers");
	return 1;
}



extern SObject Gpu_Object;
UINT32
FailSafeOvertPin::CheckXtalClockGate()
{
    //TODO: use random function to generate temperature randomly instead of 126C and 94C always.
    temp126_rawcode = 1260;
    InfoPrintf("returned raw_code = 0x%x\n",temp126_rawcode);
    temp94_rawcode = 940;
    InfoPrintf("returned raw_code = 0x%x\n",temp94_rawcode);

    SCI_initial_programming(1);

    //Register programming to enable XTAL clock gating
    lwgpu->SetRegFldDef("LW_PGC6_SCI_PWR_SEQ_ARMED_SEQ_STATE","_XTAL_CORE_CLAMP","_ASSERT");
    Platform::Delay(10);
    lwgpu->SetRegFldDef("LW_PGC6_SCI_PWR_SEQ_STATE","_UPDATE","_TRIGGER");
    Platform::Delay(10);

     SavePciReg(pcie_gen);


    //set LW_PGC6_SCI_FS_OVERT_TS_ADC_RAW_CODE
    //TEMPERATURE = (FINAL_A * RAW_CODE / 16384) - (FINAL_B / 2)
    InfoPrintf("Set temp to 126C through escape write  \n");
    Platform::EscapeWrite("tsense_mini_temp",0,32,temp126_rawcode);
    Platform::Delay(adc_out_delay); // This is callwlated based on the ts_clk freq as13.5MHz. This clk frequency is achieved by programming LW_PGC6_SCI_FS_OVERT_TS_SM_TS_CLK_DIV to 0x2 At this frequency, ts_clk requires 155us for completing 2048 clk cycles. This value of 155us is rounded to 200us. be in agreement of TS_CLK_DIV because tsense_mini_temp will take effect in maximum of 2100 TS_CLK cycles.
    Platform::EscapeRead("tsense_mini_temp",0,32,&m_ts_adc_raw_code);
    InfoPrintf("programmed LW_PGC6_SCI_FS_OVERT_TS_ADC_RAW_CODE = 0x%x.\n",m_ts_adc_raw_code);

    data = 0;
    Platform::EscapeRead("failsafe_overt", 0, 1, &data);
    if (data != 1) {
        ErrPrintf("1 : failsafe should be 1, but now it's 0x%x \n", data);
        return (1);
    }
    InfoPrintf("1: FAILSAFE OVERT  : failsafe_overt is now 0x%x \n", data);
    Platform::EscapeRead("overt", 0, 1, &data);
    if (data != 0) {
        ErrPrintf("2 : overt pad should be 0, but now it's 0x%x \n", data);
        return (1);
    }
    InfoPrintf("2: FAILSAFE OVERT  : overt pad is now 0x%x \n", data);

    Platform::EscapeWrite("tsense_mini_temp",0,32,temp94_rawcode);
    Platform::Delay(adc_out_delay); // This is callwlated based on the ts_clk freq as13.5MHz. This clk frequency is achieved by programming LW_PGC6_SCI_FS_OVERT_TS_SM_TS_CLK_DIV to 0x2 At this frequency, ts_clk requires 155us for completing 2048 clk cycles. This value of 155us is rounded to 200us. be in agreement of TS_CLK_DIV because tsense_mini_temp will take effect in maximum of 2100 TS_CLK cycles.
    Platform::EscapeRead("tsense_mini_temp",0,32,&m_ts_adc_raw_code);
    InfoPrintf("programmed LW_PGC6_SCI_FS_OVERT_TS_ADC_RAW_CODE = 0x%x.\n",m_ts_adc_raw_code);

    Platform::Delay(50);
    Platform::EscapeWrite("pex_reset_",0,1,0);
    Platform::Delay(50);
    Platform::EscapeWrite("pex_reset_",0,1,1);
    Platform::Delay(50);
    //Restore config space
    RestorePciReg(pcie_gen);
   
    Platform::Delay(30);

    // ------------------------------
    // Initialize GPU
    // ==============
    gpu_initialize();

   

    //Make sure GR is out of reset.
    enable_gr_pfifo_engine();
    
    Platform::Delay(1000);

    return (0);
}
UINT32
FailSafeOvertPin::CheckPexReset()
{
    //TODO: use random function to generate temperature randomly instead of 126C and 94C always.
    temp126_rawcode = 1260;
    InfoPrintf("returned raw_code = 0x%x",temp126_rawcode);
    temp94_rawcode = 940;
    InfoPrintf("returned raw_code = 0x%x",temp94_rawcode);

    SCI_initial_programming(0);
	
    SavePciReg(pcie_gen);
       //Assert PEX_reset before writing 126C to register
    //Reset LOW
    Platform::EscapeWrite("pex_reset_",0,1,0);
    Platform::Delay(50);

    //set LW_PGC6_SCI_FS_OVERT_TS_ADC_RAW_CODE
    //TEMPERATURE = (FINAL_A * RAW_CODE / 16384) - (FINAL_B / 2)
    InfoPrintf("Set temp to 126C through escape write  \n");
    Platform::EscapeWrite("tsense_mini_temp",0,32,temp126_rawcode);
    Platform::Delay(200); // This is callwlated based on the ts_clk freq as13.5MHz. This clk frequency is achieved by programming LW_PGC6_SCI_FS_OVERT_TS_SM_TS_CLK_DIV to 0x2 At this frequency, ts_clk requires 155us for completing 2048 clk cycles. This value of 155us is rounded to 200us. be in agreement of TS_CLK_DIV because tsense_mini_temp will take effect in maximum of 2100 TS_CLK cycles.
    //Platform::EscapeRead("sci_fs_overt_temperature_integer",0,14,&m_ts_adc_raw_code);
    Platform::EscapeRead("tsense_mini_temp",0,32,&m_ts_adc_raw_code);
    InfoPrintf("programmed LW_PGC6_SCI_FS_OVERT_TS_ADC_RAW_CODE = 0x%x.\n",m_ts_adc_raw_code);
    //Platform::Delay(1);

    data = 0;
    Platform::EscapeRead("failsafe_overt", 0, 1, &data);
    if (data != 1) {
        ErrPrintf("1 : failsafe should be 1, but now it's 0x%x \n", data);
        return (1);
    }
    InfoPrintf("1: FAILSAFE OVERT  : failsafe_overt is now 0x%x \n", data);
    Platform::EscapeRead("overt", 0, 1, &data);
    if (data != 0) {
        ErrPrintf("2 : overt pad should be 0, but now it's 0x%x \n", data);
        return (1);
    }
    InfoPrintf("2: FAILSAFE OVERT  : overt pad is now 0x%x \n", data);
    /*Platform::EscapeWrite("pex_reset_",0,1,1);
    Platform::Delay(50);
    Platform::EscapeWrite("pex_reset_",0,1,0);*/
    Platform::Delay(50);

    //UINT32 read_raw_code = temp94_rawcode;
    Platform::EscapeWrite("tsense_mini_temp",0,32,temp94_rawcode);
    Platform::Delay(200); // This is callwlated based on the ts_clk freq as13.5MHz. This clk frequency is achieved by programming LW_PGC6_SCI_FS_OVERT_TS_SM_TS_CLK_DIV to 0x2 At this frequency, ts_clk requires 155us for completing 2048 clk cycles. This value of 155us is rounded to 200us. be in agreement of TS_CLK_DIV because tsense_mini_temp will take effect in maximum of 2100 TS_CLK cycles.
    //Platform::EscapeRead("sci_fs_overt_temperature_integer",0,14,&m_ts_adc_raw_code);
    Platform::EscapeRead("tsense_mini_temp",0,32,&m_ts_adc_raw_code);
    InfoPrintf("programmed LW_PGC6_SCI_FS_OVERT_TS_ADC_RAW_CODE = 0x%x.\n",m_ts_adc_raw_code);
    //Platform::Delay(20);

    //FIXME: this is just for testing 
    Platform::EscapeWrite("pex_reset_",0,1,1);
    Platform::Delay(50);
    Platform::EscapeWrite("pex_reset_",0,1,0);
    //FIXME: this is just for testing 


    Platform::Delay(50);
    data = 0;
    Platform::EscapeRead("overt", 0, 1, &data);
    if (data != 1) {
        ErrPrintf("3 : overt pad should be 1, but now it's 0x%x \n", data);
        return (1);
    }
    InfoPrintf("3: FAILSAFE OVERT  : overt pad is now 0x%x \n", data);

    Platform::EscapeRead("failsafe_overt", 0, 1, &data);
    if (data != 0) {
        ErrPrintf("4 : failsafe should be 0, but now it's 0x%x \n", data);
        return (1);
    }
    InfoPrintf("4: FAILSAFE OVERT  : failsafe_overt is now 0x%x \n", data);
    Platform::EscapeWrite("pex_reset_",0,1,1);
    Platform::Delay(50);
    Platform::EscapeWrite("pex_reset_",0,1,0);
    Platform::Delay(50);
    Platform::EscapeWrite("pex_reset_",0,1,1);
    Platform::Delay(50);
    //Restore config space
    RestorePciReg(pcie_gen);
    
    Platform::Delay(30);

    // ------------------------------
    // Initialize GPU
    // ==============
    gpu_initialize();

    //Make sure GR is out of reset.
    enable_gr_pfifo_engine();

    Platform::Delay(1000);

    return (0);
}

    UINT32
FailSafeOvertPin::CheckFundamentalReset()
{
    temp126_rawcode = 1260;
    InfoPrintf("returned raw_code = 0x%x",temp126_rawcode);
    temp94_rawcode = 940;
    InfoPrintf("returned raw_code = 0x%x",temp94_rawcode);

    SCI_initial_programming(0);

    GpuPtr pGpu;

    JavaScriptPtr pJs;
    RC             rc;


	
    SavePciReg(pcie_gen);
        InfoPrintf("hot_reset : asserting HotReset\n");
    // Send in Hot reset
    Platform::EscapeWrite("doHotReset",0,1,1);
    Platform::Delay(20);

    //set LW_PGC6_SCI_FS_OVERT_TS_ADC_RAW_CODE
    //TEMPERATURE = (FINAL_A * RAW_CODE / 16384) - (FINAL_B / 2)
    InfoPrintf("Set temp to 126C through escape write  \n");
    Platform::EscapeWrite("tsense_mini_temp",0,32,temp126_rawcode);
    Platform::Delay(200); // This is callwlated based on the ts_clk freq as13.5MHz. This clk frequency is achieved by programming LW_PGC6_SCI_FS_OVERT_TS_SM_TS_CLK_DIV to 0x2 At this frequency, ts_clk requires 155us for completing 2048 clk cycles. This value of 155us is rounded to 200us. be in agreement of TS_CLK_DIV because tsense_mini_temp will take effect in maximum of 2100 TS_CLK cycles.
    Platform::EscapeRead("sci_fs_overt_temperature_integer",0,14,&m_callwlated_temp);
    Platform::EscapeRead("tsense_mini_temp",0,32,&m_ts_adc_raw_code);
    InfoPrintf("programmed LW_PGC6_SCI_FS_OVERT_TS_ADC_RAW_CODE = 0x%x.\n",m_ts_adc_raw_code);
    InfoPrintf("callwlated LW_PGC6_SCI_FS_OVERT_TS_ADC_RAW_CODE = 0x%x.\n",m_callwlated_temp);
    // Platform::Delay(20);

    data = 0;
    Platform::EscapeRead("failsafe_overt", 0, 1, &data);
    if (data != 1) {
        ErrPrintf("1 : failsafe should be 1, but now it's 0x%x \n", data);
        return (1);
    }
    InfoPrintf(" 1 : FAILSAFE OVERT  : failsafe is now 0x%x \n", data);
    Platform::EscapeRead("overt", 0, 1, &data);
    if (data != 0) {
        ErrPrintf("2 : overt pad should be 0, but now it's 0x%x \n", data);
        return (1);
    }
    InfoPrintf(" 2 : FAILSAFE OVERT  : overt pad is now 0x%x \n", data);
    //Platform::EscapeWrite("doHotReset",0,1,0);
    //InfoPrintf("hot_reset : Exiting hot reset ...\n");
    Platform::Delay(50);

    Platform::EscapeWrite("tsense_mini_temp",0,32,temp94_rawcode );
    Platform::Delay(200); // This is callwlated based on the ts_clk freq as13.5MHz. This clk frequency is achieved by programming LW_PGC6_SCI_FS_OVERT_TS_SM_TS_CLK_DIV to 0x2 At this frequency, ts_clk requires 155us for completing 2048 clk cycles. This value of 155us is rounded to 200us. be in agreement of TS_CLK_DIV because tsense_mini_temp will take effect in maximum of 2100 TS_CLK cycles.
    Platform::EscapeRead("sci_fs_overt_temperature_integer",0,14,&m_callwlated_temp);
    Platform::EscapeRead("tsense_mini_temp",0,32,&m_ts_adc_raw_code);
    InfoPrintf("programmed LW_PGC6_SCI_FS_OVERT_TS_ADC_RAW_CODE = 0x%x.\n",m_ts_adc_raw_code);
    InfoPrintf("callwlated LW_PGC6_SCI_FS_OVERT_TS_ADC_RAW_CODE = 0x%x.\n",m_callwlated_temp);
    //Platform::Delay(20);

    data = 0;
    Platform::EscapeRead("overt", 0, 1, &data);
    if (data != 0) {
        ErrPrintf("3 : overt pad should be 0, but now it's 0x%x \n", data);
        return (1);
    }
    InfoPrintf(" 3 : FAILSAFE OVERT  : overt pad is now 0x%x \n", data);

    Platform::EscapeRead("failsafe_overt", 0, 1, &data);
    if (data != 0) {
        ErrPrintf("4 : failsafe should be 0, but now it's 0x%x \n", data);
        return (1);
    }
    InfoPrintf(" 4 : FAILSAFE OVERT  : failsafe is now 0x%x \n", data);
    //Platform::EscapeWrite("doHotReset",0,1,0);
    //InfoPrintf("hot_reset : Exiting hot reset ...\n");
    Platform::Delay(2);
    InfoPrintf("  FAILSAFE OVERT  asserting pex reset 0x%x \n", data);
    //assert pex reset to clear overt latch
    Platform::EscapeWrite("pex_reset_",0,1,0);
    Platform::Delay(50);
    InfoPrintf("  FAILSAFE OVERT de asserting pex reset 0x%x \n", data);

    //de-assert pex reset
    Platform::EscapeWrite("pex_reset_",0,1,1);
    Platform::Delay(50);
	
    RestorePciReg(pcie_gen);
        Platform::Delay(30);

    // ------------------------------
    // Initialize GPU
    // ==============
    gpu_initialize();

    enable_gr_pfifo_engine();

    Platform::Delay(1000);

    return(0);

}
    UINT32
FailSafeOvertPin::CheckPexResetFusesNotSensed()
{
    data=0;
    Platform::EscapeRead("overt", 0, 1, &data);
    if (data != 0) {
        InfoPrintf("1 : overt pad should be 0, but now it's 0x%x \n", data);
        return (1);
    }
    InfoPrintf(" 1: FAILSAFE OVERT : CheckPexResetFusesNotSensed overt pad is now 0x%x \n", data);
    Platform::EscapeRead("overt_detect", 0, 1, &data);
    if (data != 0) {
        InfoPrintf("2 : overt_detect pad should be 0, but now it's 0x%x \n", data);
        return (1);
    }
    InfoPrintf(" 2: FAILSAFE OVERT : CheckPexResetFusesNotSensed overt_detect is now 0x%x \n", data);

    Platform::EscapeRead("latch_overt", 0, 1, &data);
    if (data != 1) {
        InfoPrintf("3 : latch_overt should be 1, but now it's 0x%x \n", data);
        return (1);
    }
    InfoPrintf(" 3: FAILSAFE OVERT : CheckPexResetFusesNotSensed latch_overt is now 0x%x \n", data);
    return(0);
}

    UINT32
FailSafeOvertPin::CheckPexResetFusesSensedHighTemp()
{
    temp126_rawcode = temperature2code_nominal(126);
    InfoPrintf("returned raw_code = 0x%x",temp126_rawcode);
    temp94_rawcode = temperature2code_nominal(94);
    InfoPrintf("returned raw_code = 0x%x",temp94_rawcode);

    SCI_initial_programming(0);

    GpuPtr pGpu;
	
    SavePciReg(pcie_gen);
    
    //Assert PEX_reset before writing 126C to register
    //Reset LOW
    Platform::EscapeWrite("pex_reset_",0,1,0);
    Platform::Delay(50);

    //verifying bandgap fuse values with opt_sci_bg_valid is 0
    data =0;
    Platform::EscapeRead("sci_fs_overt_fuse_bg_temp_coeff", 0, 1, &data);
    if (data != bg_temp_coeff) {
        ErrPrintf("1 : BG_TEMP_COEFF should be 0x%x, but now it's 0x%x \n",bg_temp_coeff,data);
        return (1);
    }else{
        InfoPrintf("LW_PGC6_SCI_FS_OVERT_TS_ADJ_BG_TEMP_COEFF = 0x%x\n",data);
    }
    data =0;
    Platform::EscapeRead("sci_fs_overt_fuse_bg_voltage", 0, 1, &data);
    if (data != bg_voltage) {
        ErrPrintf("2 : bg_voltage should be 0x%x, but now it's 0x%x \n",bg_voltage,data );
        return (1);
    }else{
        InfoPrintf("LW_PGC6_SCI_FS_OVERT_TS_ADJ_BG_VOLTAGE = 0x%x\n",data);
    }

    //set LW_PGC6_SCI_FS_OVERT_TS_ADC_RAW_CODE
    //TEMPERATURE = (FINAL_A * RAW_CODE / 16384) - (FINAL_B / 2)
    InfoPrintf("Set temp to 126C through escape write  \n");
    Platform::EscapeWrite("ts_adc_out",0,14,temp126_rawcode);
    Platform::Delay(30);
    Platform::EscapeRead("ts_adc_out",0,14,&m_ts_adc_raw_code);
    InfoPrintf("programmed LW_PGC6_SCI_FS_OVERT_TS_ADC_RAW_CODE = 0x%x.\n",m_ts_adc_raw_code);

    data = 0;
    Platform::EscapeRead("failsafe_overt", 0, 1, &data);
    if (data != 0) {
        ErrPrintf("3 : failsafe should be 0, but now it's 0x%x \n", data);
        return (1);
    }
    Platform::EscapeRead("overt", 0, 1, &data);
    if (data != 1) {
        ErrPrintf("4 : overt pad should be 1, but now it's 0x%x \n", data);
        return (1);
    }

    Platform::EscapeWrite("pex_reset_",0,1,1);
    Platform::Delay(50);

    //Restore config space
    RestorePciReg(pcie_gen);
   
    Platform::Delay(30);

    // ------------------------------
    // Initialize GPU
    // ==============
    gpu_initialize();

    enable_gr_pfifo_engine();

    Platform::Delay(1000);

    return(0);
}

    UINT32
FailSafeOvertPin::CheckPexResetFusesSensedLowTemp()
{
    temp126_rawcode = temperature2code_nominal(126);
    InfoPrintf("returned raw_code = 0x%x \n",temp126_rawcode);
    temp94_rawcode = temperature2code_nominal(94);
    InfoPrintf("returned raw_code = 0x%x \n",temp94_rawcode);
    temp94_final_rawcode = temperature2code_final(94);
    InfoPrintf("returned raw_code = 0x%x \n",temp94_final_rawcode);

    SCI_initial_programming(0);
	

    SavePciReg(pcie_gen);
   
    //Assert PEX_reset before writing 126C to register
    //Reset LOW
    Platform::EscapeWrite("pex_reset_",0,1,0);
    Platform::Delay(50);

    //verifying bandgap fuse values with opt_sci_bg_valid is 1
    data =0;
    Platform::EscapeRead("sci_fs_overt_fuse_bg_temp_coeff", 0, 1, &data);
    if (data != bg_temp_coeff) {
        ErrPrintf("1 : BG_TEMP_COEFF should be 0x%x, but now it's 0x%x \n",bg_temp_coeff,data );
        return (1);
    }else{
        InfoPrintf("LW_PGC6_SCI_FS_OVERT_TS_ADJ_BG_TEMP_COEFF = 0x%x\n",data);
    }
    data =0;
    Platform::EscapeRead("sci_fs_overt_fuse_bg_voltage", 0, 1, &data);
    if (data != bg_voltage) {
        ErrPrintf("2 : bg_voltage should be 0x%x, but now it's 0x%x \n",bg_voltage,data );
        return (1);
    }else{
        InfoPrintf("LW_PGC6_SCI_FS_OVERT_TS_ADJ_BG_VOLTAGE = 0x%x\n",data);
    }

    //set LW_PGC6_SCI_FS_OVERT_TS_ADC_RAW_CODE
    //TEMPERATURE = (FINAL_A * RAW_CODE / 16384) - (FINAL_B / 2)
    InfoPrintf("Set temp to 126C through escape write  \n");
    //Platform::EscapeWrite("sci_fs_overt_ts_adc_raw_code",0,14,0x2cf0);
    Platform::EscapeWrite("ts_adc_out",0,14,temp94_rawcode);
    Platform::Delay(30);
    Platform::EscapeRead("ts_adc_out",0,14,&m_ts_adc_raw_code);
    InfoPrintf("programmed LW_PGC6_SCI_FS_OVERT_TS_ADC_RAW_CODE = 0x%x.\n",m_ts_adc_raw_code);

    data = 0;
    Platform::EscapeRead("failsafe_overt", 0, 1, &data);
    if (data != 1) {
        ErrPrintf("1 : failsafe should be 1, but now it's 0x%x \n", data);
        return (1);
    }
    Platform::EscapeRead("overt", 0, 1, &data);
    if (data != 0) {
        ErrPrintf("2 : overt pad should be 0, but now it's 0x%x \n", data);
        return (1);
    }
    Platform::EscapeWrite("pex_reset_",0,1,1);
    Platform::Delay(500);

    InfoPrintf("Set temp to 94C through escape write  \n");
    Platform::EscapeWrite("ts_adc_out",0,14,temp94_final_rawcode);
    Platform::Delay(30);
    Platform::EscapeRead("sci_fs_overt_temperature_integer",0,14,&m_callwlated_temp);
    Platform::EscapeRead("ts_adc_out",0,14,&m_ts_adc_raw_code);
    InfoPrintf("programmed LW_PGC6_SCI_FS_OVERT_TS_ADC_RAW_CODE = 0x%x.\n",m_ts_adc_raw_code);
    InfoPrintf("callwlated LW_PGC6_SCI_FS_OVERT_TS_ADC_RAW_CODE = 0x%x.\n",m_callwlated_temp);

    Platform::EscapeWrite("pex_reset_",0,1,0);
    Platform::Delay(50);

    Platform::EscapeWrite("pex_reset_",0,1,1);
    Platform::Delay(50);
	
    RestorePciReg(pcie_gen);
   
    Platform::Delay(30);

    // ------------------------------
    // Initialize GPU
    // ==============
    gpu_initialize();

    //Make sure GR is out of reset.
    enable_gr_pfifo_engine();

    Platform::Delay(1000);

    return(0);
}

    UINT32
FailSafeOvertPin::CheckFundamentalResetFusesNotSensed()
{
    data=0;
    Platform::EscapeRead("overt", 0, 1, &data);
    if (data != 0) {
        InfoPrintf("1 : overt pad should be 0, but now it's 0x%x \n", data);
        return (1);
    }else {
        InfoPrintf("overt pad is 0x%x \n", data);
    }
    Platform::EscapeRead("overt_detect", 0, 1, &data);
    if (data != 0) {
        InfoPrintf("2 : overt_detect pad should be 0, but now it's 0x%x \n", data);
        return (1);
    }

    Platform::EscapeRead("latch_overt", 0, 1, &data);
    if (data != 1) {
        InfoPrintf("3 : latch_overt should be 1, but now it's 0x%x \n", data);
        return (1);
    }else {
        InfoPrintf("overt latch is 0x%x \n", data);
    }
    return(0);
}
    UINT32
FailSafeOvertPin::CheckFundamentalResetFusesSensedHighTemp()
{
    temp126_rawcode = temperature2code_nominal(126);
    InfoPrintf("returned raw_code = 0x%x \n",temp126_rawcode);
    temp94_rawcode = temperature2code_nominal(94);
    InfoPrintf("returned raw_code = 0x%x \n",temp94_rawcode);

    SCI_initial_programming(0);
	

     SavePciReg(pcie_gen);
        InfoPrintf("hot_reset : asserting HotReset\n");
    // Send in Hot reset
    Platform::EscapeWrite("doHotReset",0,1,1);
    Platform::Delay(100);

    //verifying bandgap fuse values opt_sci_bg_valid is 0
    data =0;
    Platform::EscapeRead("sci_fs_overt_fuse_bg_temp_coeff", 0, 1, &data);
    if (data != bg_temp_coeff) {
        ErrPrintf("1 : BG_TEMP_COEFF should be 0x%x, but now it's 0x%x \n",bg_temp_coeff,data );
        return (1);
    }else{
        InfoPrintf("LW_PGC6_SCI_FS_OVERT_TS_ADJ_BG_TEMP_COEFF = 0x%x\n",data);
    }
    data =0;
    Platform::EscapeRead("sci_fs_overt_fuse_bg_voltage", 0, 1, &data);
    if (data != bg_voltage) {
        ErrPrintf("2 : bg_voltage should be 0x%x, but now it's 0x%x \n",bg_voltage,data );
        return (1);
    }else{
        InfoPrintf("LW_PGC6_SCI_FS_OVERT_TS_ADJ_BG_VOLTAGE = 0x%x\n",data);
    }

    //set LW_PGC6_SCI_FS_OVERT_TS_ADC_RAW_CODE
    //TEMPERATURE = (FINAL_A * RAW_CODE / 16384) - (FINAL_B / 2)
    InfoPrintf("Set temp to 126C through escape write  \n");
    //Platform::EscapeWrite("sci_fs_overt_ts_adc_raw_code",0,14,0x3bf0);
    Platform::EscapeWrite("ts_adc_out",0,14,temp126_rawcode);
    Platform::Delay(30);
    Platform::EscapeRead("ts_adc_out",0,14,&m_ts_adc_raw_code);
    InfoPrintf("programmed LW_PGC6_SCI_FS_OVERT_TS_ADC_RAW_CODE = 0x%x.\n",m_ts_adc_raw_code);

    data = 0;
    Platform::EscapeRead("failsafe_overt", 0, 1, &data);
    if (data != 0) {
        ErrPrintf("3 : failsafe should be 0, but now it's 0x%x \n", data);
        return (1);
    }
    Platform::EscapeRead("overt", 0, 1, &data);
    if (data != 1) {
        ErrPrintf("4 : overt pad should be 0, but now it's 0x%x \n", data);
        return (1);
    }
    //Platform::EscapeWrite("doHotReset",0,1,0);
    //InfoPrintf("hot_reset : Exiting hot reset ...\n");
    Platform::Delay(2);
    
    RestorePciReg(pcie_gen);

   
    Platform::Delay(30);

    // ------------------------------
    // Initialize GPU
    // ==============
    gpu_initialize();

    //Make sure GR is out of reset.
    enable_gr_pfifo_engine();

    Platform::Delay(1000);

    return(0);
}
    UINT32
FailSafeOvertPin::CheckFundamentalResetFusesSensedLowTemp()
{
    temp126_rawcode = temperature2code_nominal(126);
    InfoPrintf("returned raw_code = 0x%x \n",temp126_rawcode);
    temp94_rawcode = temperature2code_nominal(94);
    InfoPrintf("returned raw_code = 0x%x \n",temp94_rawcode);
    temp94_final_rawcode = temperature2code_final(94);
    InfoPrintf("returned raw_code = 0x%x \n",temp94_final_rawcode);

    SCI_initial_programming(0);

    SavePciReg(pcie_gen);
 

    InfoPrintf("hot_reset : asserting HotReset\n");
    // Send in Hot reset
    Platform::EscapeWrite("doHotReset",0,1,1);
    Platform::Delay(100);

    //verifying bandgap fuse values with opt_sci_bg_valid is 1
    data =0;
    Platform::EscapeRead("sci_fs_overt_fuse_bg_temp_coeff", 0, 1, &data);
    if (data != bg_temp_coeff) {
        ErrPrintf("1 : BG_TEMP_COEFF should be 0x%x, but now it's 0x%x \n",bg_temp_coeff,data );
        return (1);
    }else{
        InfoPrintf("LW_PGC6_SCI_FS_OVERT_TS_ADJ_BG_TEMP_COEFF = 0x%x\n",data);
    }
    data =0;
    Platform::EscapeRead("sci_fs_overt_fuse_bg_voltage", 0, 1, &data);
    if (data != bg_voltage) {
        ErrPrintf("2 : bg_voltage should be 0x%x, but now it's 0x%x \n",bg_voltage,data );
        return (1);
    }else{
        InfoPrintf("LW_PGC6_SCI_FS_OVERT_TS_ADJ_BG_VOLTAGE = 0x%x\n",data);
    }

    //set LW_PGC6_SCI_FS_OVERT_TS_ADC_RAW_CODE
    //TEMPERATURE = (FINAL_A * RAW_CODE / 16384) - (FINAL_B / 2)
    InfoPrintf("Set temp to 126C through escape write  \n");
    Platform::EscapeWrite("ts_adc_out",0,14,temp94_rawcode);
    Platform::Delay(30);
    Platform::EscapeRead("ts_adc_out",0,14,&m_ts_adc_raw_code);
    InfoPrintf("programmed LW_PGC6_SCI_FS_OVERT_TS_ADC_RAW_CODE = 0x%x.\n",m_ts_adc_raw_code);

    data = 0;
    Platform::EscapeRead("failsafe_overt", 0, 1, &data);
    if (data != 1) {
        ErrPrintf("3 : failsafe should be 1, but now it's 0x%x \n", data);
        return (1);
    }
    Platform::EscapeRead("overt", 0, 1, &data);
    if (data != 0) {
        ErrPrintf("4 : overt pad should be 0, but now it's 0x%x \n", data);
        return (1);
    }
    Platform::EscapeWrite("doHotReset",0,1,0);
    InfoPrintf("hot_reset : Exiting hot reset ...\n");
    Platform::Delay(50);

    InfoPrintf("Set temp to 94C through escape write  \n");
    Platform::EscapeWrite("ts_adc_out",0,14,temp94_final_rawcode);
    Platform::Delay(30);
    Platform::EscapeRead("sci_fs_overt_temperature_integer",0,14,&m_callwlated_temp);
    Platform::EscapeRead("ts_adc_out",0,14,&m_ts_adc_raw_code);
    InfoPrintf("programmed LW_PGC6_SCI_FS_OVERT_TS_ADC_RAW_CODE = 0x%x.\n",m_ts_adc_raw_code);
    InfoPrintf("callwlated LW_PGC6_SCI_FS_OVERT_TS_ADC_RAW_CODE = 0x%x.\n",m_callwlated_temp);

    //assert pex reset to clear overt latch
    Platform::EscapeWrite("pex_reset_",0,1,0);
    Platform::Delay(50);

    //de-assert pex reset
    Platform::EscapeWrite("pex_reset_",0,1,1);
    Platform::Delay(50);
    
    RestorePciReg(pcie_gen);

   
    Platform::Delay(30);

    // ------------------------------
    // Initialize GPU
    // ==============
    gpu_initialize();

    //Make sure GR is out of reset.
    enable_gr_pfifo_engine();

    Platform::Delay(1000);

    return(0);
}
    UINT32
FailSafeOvertPin::CheckSoftRest()
{
    temp126_rawcode = 1260;
    InfoPrintf("returned raw_code = 0x%x \n",temp126_rawcode);
    temp94_rawcode = 940;
    InfoPrintf("returned raw_code = 0x%x \n",temp94_rawcode);

    SCI_initial_programming(0);

    GpuPtr pGpu;


    SavePciReg(pcie_gen);
   

    Platform::Delay(50);
    InfoPrintf("soft_reset : asserting SoftReset\n");
    // Send in soft reset
    lwgpu->SetRegFldNum("LW_PCFG_XVE_SW_RESET", "", 0x3);
    Platform::Delay(1100);
    InfoPrintf("soft_reset : asserting SoftReset done\n");

    //set LW_PGC6_SCI_FS_OVERT_TS_ADC_RAW_CODE
    //TEMPERATURE = (FINAL_A * RAW_CODE / 16384) - (FINAL_B / 2)
    InfoPrintf("Set temp to 126C through escape write  \n");
    //Platform::EscapeWrite("sci_fs_overt_ts_adc_raw_code",0,14,0x3bf0);
    Platform::EscapeWrite("tsense_mini_temp",0,32,temp126_rawcode);
    Platform::Delay(200); // This is callwlated based on the ts_clk freq as13.5MHz. This clk frequency is achieved by programming LW_PGC6_SCI_FS_OVERT_TS_SM_TS_CLK_DIV to 0x2 At this frequency, ts_clk requires 155us for completing 2048 clk cycles. This value of 155us is rounded to 200us. be in agreement of TS_CLK_DIV because tsense_mini_temp will take effect in maximum of 2100 TS_CLK cycles.
    Platform::EscapeRead("sci_fs_overt_temperature_integer",0,14,&m_callwlated_temp);
    Platform::EscapeRead("tsense_mini_temp",0,32,&m_ts_adc_raw_code);
    InfoPrintf("programmed LW_PGC6_SCI_FS_OVERT_TS_ADC_RAW_CODE = 0x%x.\n",m_ts_adc_raw_code);
    InfoPrintf("callwlated LW_PGC6_SCI_FS_OVERT_TS_ADC_RAW_CODE = 0x%x.\n",m_callwlated_temp);

    //Verify OVERT pad stays asserted
    data = 0;
    Platform::EscapeRead("failsafe_overt", 0, 1, &data);
    if (data != 1) {
        ErrPrintf("1 : failsafe should be 1, but now it's 0x%x \n", data);
        return (1);
    }
    Platform::EscapeRead("overt", 0, 1, &data);
    if (data != 0) {
        ErrPrintf("2 : overt pad should be 0, but now it's 0x%x \n", data);
        return (1);
    }
    //assert pex reset
    Platform::EscapeWrite("pex_reset_",0,1,0);
    Platform::Delay(50);
    //Verify OVERT pad remains asserted
    data = 0;
    Platform::EscapeRead("failsafe_overt", 0, 1, &data);
    if (data != 1) {
        ErrPrintf("3 : failsafe should be 1, but now it's 0x%x \n", data);
        return (1);
    }
    Platform::EscapeRead("overt", 0, 1, &data);
    if (data != 0) {
        ErrPrintf("4 : overt pad should be 0, but now it's 0x%x \n", data);
        return (1);
    }
    //deassert pex reset
    Platform::EscapeWrite("pex_reset_",0,1,1);
    Platform::Delay(50);

    //Set TSENSE_MINI temperature below SCI OVERT threshold
    Platform::EscapeWrite("tsense_mini_temp",0,32,temp94_rawcode);
    Platform::Delay(200); // This is callwlated based on the ts_clk freq as13.5MHz. This clk frequency is achieved by programming LW_PGC6_SCI_FS_OVERT_TS_SM_TS_CLK_DIV to 0x2 At this frequency, ts_clk requires 155us for completing 2048 clk cycles. This value of 155us is rounded to 200us. be in agreement of TS_CLK_DIV because tsense_mini_temp will take effect in maximum of 2100 TS_CLK cycles.
    Platform::EscapeRead("sci_fs_overt_temperature_integer",0,14,&m_callwlated_temp);
    Platform::EscapeRead("tsense_mini_temp",0,32,&m_ts_adc_raw_code);
    InfoPrintf("programmed LW_PGC6_SCI_FS_OVERT_TS_ADC_RAW_CODE = 0x%x.\n",m_ts_adc_raw_code);
    InfoPrintf("callwlated LW_PGC6_SCI_FS_OVERT_TS_ADC_RAW_CODE = 0x%x.\n",m_callwlated_temp);
    //Platform::Delay(20);

    data = 0;
    Platform::EscapeRead("overt", 0, 1, &data);
    if (data != 0) {
        ErrPrintf("5 : overt pad should be 0, but now it's 0x%x \n", data);
        return (1);
    }

    Platform::EscapeRead("failsafe_overt", 0, 1, &data);
    if (data != 0) {
        ErrPrintf("6 : failsafe should be 0, but now it's 0x%x \n", data);
        return (1);
    }
    //assert pex reset
    Platform::EscapeWrite("pex_reset_",0,1,0);
    Platform::Delay(50);

    //verify that OVERT clears
    data = 0;
    Platform::EscapeRead("failsafe_overt", 0, 1, &data);
    if (data != 0) {
        ErrPrintf("7 : failsafe should be 0, but now it's 0x%x \n", data);
        return (1);
    }
    Platform::EscapeRead("overt", 0, 1, &data);
    if (data != 1) {
        ErrPrintf("8 : overt pad should be 1, but now it's 0x%x \n", data);
        return (1);
    }
    //deassert pex reset
    Platform::EscapeWrite("pex_reset_",0,1,1);
    Platform::Delay(5);

    RestorePciReg(pcie_gen);
   
    Platform::Delay(30);

    // ------------------------------
    // Initialize GPU
    // ==============
    gpu_initialize();

    //Make sure GR is out of reset.
    enable_gr_pfifo_engine();

    Platform::Delay(1000);

    return(0);

}
    UINT32
FailSafeOvertPin::CheckSoftRestFusesSensedHighTemp()
{
    temp126_rawcode = temperature2code_nominal(126);
    InfoPrintf("returned raw_code = 0x%x \n",temp126_rawcode);
    temp94_rawcode = temperature2code_nominal(94);
    InfoPrintf("returned raw_code = 0x%x \n",temp94_rawcode);

    SCI_initial_programming(0);

    SavePciReg(pcie_gen);
    

    InfoPrintf("soft_reset : asserting SoftReset\n");
    // Send in soft reset
    lwgpu->SetRegFldNum("LW_PCFG_XVE_SW_RESET", "", 0x3);
    Platform::Delay(10);

    //verifying bandgap fuse values opt_sci_bg_valid is 0
    data =0;
    Platform::EscapeRead("sci_fs_overt_fuse_bg_temp_coeff", 0, 1, &data);
    if (data != bg_temp_coeff) {
        ErrPrintf("1 : BG_TEMP_COEFF should be 0x%x, but now it's 0x%x \n",bg_temp_coeff,data );
        return (1);
    }else{
        InfoPrintf("LW_PGC6_SCI_FS_OVERT_TS_ADJ_BG_TEMP_COEFF = 0x%x\n",data);
    }
    data =0;
    Platform::EscapeRead("sci_fs_overt_fuse_bg_voltage", 0, 1, &data);
    if (data != bg_voltage) {
        ErrPrintf("2 : bg_voltage should be 0x%x, but now it's 0x%x \n",bg_voltage,data );
        return (1);
    }else{
        InfoPrintf("LW_PGC6_SCI_FS_OVERT_TS_ADJ_BG_VOLTAGE = 0x%x\n",data);
    }

    //set LW_PGC6_SCI_FS_OVERT_TS_ADC_RAW_CODE
    //TEMPERATURE = (FINAL_A * RAW_CODE / 16384) - (FINAL_B / 2)
    InfoPrintf("Set temp to 126C through escape write  \n");
    Platform::EscapeWrite("ts_adc_out",0,14,temp126_rawcode);
    Platform::Delay(30);
    Platform::EscapeRead("sci_fs_overt_temperature_integer",0,14,&m_callwlated_temp);
    Platform::EscapeRead("ts_adc_out",0,14,&m_ts_adc_raw_code);
    InfoPrintf("programmed LW_PGC6_SCI_FS_OVERT_TS_ADC_RAW_CODE = 0x%x.\n",m_ts_adc_raw_code);
    InfoPrintf("callwlated LW_PGC6_SCI_FS_OVERT_TS_ADC_RAW_CODE = 0x%x.\n",m_callwlated_temp);
    //Platform::Delay(20);

    //Verify OVERT pad did not assert
    data = 0;
    Platform::EscapeRead("failsafe_overt", 0, 1, &data);
    if (data != 0) {
        ErrPrintf("3 : failsafe should be 0, but now it's 0x%x \n", data);
        return (1);
    }
    Platform::EscapeRead("overt", 0, 1, &data);
    if (data != 1) {
        ErrPrintf("4 : overt pad should be 1, but now it's 0x%x \n", data);
        return (1);
    }

    //assert pex reset
    Platform::EscapeWrite("pex_reset_",0,1,0);
    Platform::Delay(50);

    //deassert pex reset
    Platform::EscapeWrite("pex_reset_",0,1,1);
    Platform::Delay(5);
	
    RestorePciReg(pcie_gen);
       Platform::Delay(30);

    // ------------------------------
    // Initialize GPU
    // ==============
    gpu_initialize();

    //Make sure GR is out of reset.
    enable_gr_pfifo_engine();

    Platform::Delay(1000);

    return(0);

}
    UINT32
FailSafeOvertPin::CheckSoftRestFusesSensedLowTemp()
{
    temp126_rawcode = temperature2code_nominal(126);
    InfoPrintf("returned raw_code = 0x%x \n",temp126_rawcode);
    temp94_rawcode = temperature2code_nominal(94);
    InfoPrintf("returned raw_code = 0x%x \n",temp94_rawcode);
    temp94_final_rawcode = temperature2code_final(94);
    InfoPrintf("returned raw_code = 0x%x \n",temp94_final_rawcode);

    SCI_initial_programming(0);

     SavePciReg(pcie_gen);
    
    InfoPrintf("soft_reset : asserting SoftReset\n");
    // Send in soft reset
    lwgpu->SetRegFldNum("LW_PCFG_XVE_SW_RESET", "", 0x3);
    Platform::Delay(10);

    //verifying bandgap fuse values with opt_sci_bg_valid is 1
    data =0;
    Platform::EscapeRead("sci_fs_overt_fuse_bg_temp_coeff", 0, 1, &data);
    if (data != bg_temp_coeff) {
        ErrPrintf("1 : BG_TEMP_COEFF should be 0x%x, but now it's 0x%x \n",bg_temp_coeff,data );
        return (1);
    }else{
        InfoPrintf("LW_PGC6_SCI_FS_OVERT_TS_ADJ_BG_TEMP_COEFF = 0x%x\n",data);
    }
    data =0;
    Platform::EscapeRead("sci_fs_overt_fuse_bg_voltage", 0, 1, &data);
    if (data != bg_voltage) {
        ErrPrintf("2 : bg_voltage should be 0x%x, but now it's 0x%x \n",bg_voltage,data );
        return (1);
    }else{
        InfoPrintf("LW_PGC6_SCI_FS_OVERT_TS_ADJ_BG_VOLTAGE = 0x%x\n",data);
    }

    //set LW_PGC6_SCI_FS_OVERT_TS_ADC_RAW_CODE
    //TEMPERATURE = (FINAL_A * RAW_CODE / 16384) - (FINAL_B / 2)
    InfoPrintf("Set temp to 94C through escape write  \n");
    Platform::EscapeWrite("ts_adc_out",0,14,temp94_rawcode);
    Platform::Delay(30);
    Platform::EscapeRead("sci_fs_overt_temperature_integer",0,14,&m_callwlated_temp);
    Platform::EscapeRead("ts_adc_out",0,14,&m_ts_adc_raw_code);
    InfoPrintf("programmed LW_PGC6_SCI_FS_OVERT_TS_ADC_RAW_CODE = 0x%x.\n",m_ts_adc_raw_code);
    InfoPrintf("callwlated LW_PGC6_SCI_FS_OVERT_TS_ADC_RAW_CODE = 0x%x.\n",m_callwlated_temp);

    //Verify OVERT pad stays asserted
    data = 0;
    Platform::EscapeRead("failsafe_overt", 0, 1, &data);
    if (data != 1) {
        ErrPrintf("3 : failsafe should be 1, but now it's 0x%x \n", data);
        return (1);
    }
    Platform::EscapeRead("overt", 0, 1, &data);
    if (data != 0) {
        ErrPrintf("4 : overt pad should be 0, but now it's 0x%x \n", data);
        return (1);
    }

    InfoPrintf("Set temp to 94C through escape write  \n");
    Platform::EscapeWrite("ts_adc_out",0,14,temp94_final_rawcode);
    Platform::Delay(30);
    Platform::EscapeRead("sci_fs_overt_temperature_integer",0,14,&m_callwlated_temp);
    Platform::EscapeRead("ts_adc_out",0,14,&m_ts_adc_raw_code);
    InfoPrintf("programmed LW_PGC6_SCI_FS_OVERT_TS_ADC_RAW_CODE = 0x%x.\n",m_ts_adc_raw_code);
    InfoPrintf("callwlated LW_PGC6_SCI_FS_OVERT_TS_ADC_RAW_CODE = 0x%x.\n",m_callwlated_temp);

    //assert pex reset
    Platform::EscapeWrite("pex_reset_",0,1,0);
    Platform::Delay(50);

    //deassert pex reset
    Platform::EscapeWrite("pex_reset_",0,1,1);
    Platform::Delay(50);


    RestorePciReg(pcie_gen);
   
    Platform::Delay(30);

    // ------------------------------
    // Initialize GPU
    // ==============
    gpu_initialize();

    //Make sure GR is out of reset.
    enable_gr_pfifo_engine();

    Platform::Delay(1000);

    return(0);

}

UINT32
FailSafeOvertPin::CheckHotRest()
{
    temp126_rawcode = 1260;
    InfoPrintf("returned raw_code = 0x%x \n",temp126_rawcode);
    temp94_rawcode = 940;
    InfoPrintf("returned raw_code = 0x%x \n",temp94_rawcode);

    SCI_initial_programming(0);

    SavePciReg(pcie_gen);
    

    InfoPrintf("hot_reset : asserting HotReset\n");
    // Set _ASSERT_FUNDAMENTAL_RESET_ON_HOT_RESET disabled before asserting hot reset
    lwgpu->SetRegFldDef("LW_PCFG_XVE_RESET_CTRL","_ASSERT_FUNDAMENTAL_RESET_ON_HOT_RESET","_DISABLED");
    Platform::Delay(10);
    InfoPrintf("hot_reset : asserting HotReset\n");
    // Send in Hot reset
    Platform::EscapeWrite("enterHotReset",0,1,1);
    Platform::Delay(20);

    //set LW_PGC6_SCI_FS_OVERT_TS_ADC_RAW_CODE
    //TEMPERATURE = (FINAL_A * RAW_CODE / 16384) - (FINAL_B / 2)
    InfoPrintf("Set temp to 126C through escape write  \n");
    Platform::EscapeWrite("tsense_mini_temp",0,32,temp126_rawcode);
    Platform::Delay(200); // This is callwlated based on the ts_clk freq as13.5MHz. This clk frequency is achieved by programming LW_PGC6_SCI_FS_OVERT_TS_SM_TS_CLK_DIV to 0x2 At this frequency, ts_clk requires 155us for completing 2048 clk cycles. This value of 155us is rounded to 200us. be in agreement of TS_CLK_DIV because tsense_mini_temp will take effect in maximum of 2100 TS_CLK cycles.
    Platform::EscapeRead("sci_fs_overt_temperature_integer",0,14,&m_callwlated_temp);
    Platform::EscapeRead("tsense_mini_temp",0,32,&m_ts_adc_raw_code);
    InfoPrintf("programmed LW_PGC6_SCI_FS_OVERT_TS_ADC_RAW_CODE = 0x%x.\n",m_ts_adc_raw_code);
    InfoPrintf("callwlated LW_PGC6_SCI_FS_OVERT_TS_ADC_RAW_CODE = 0x%x.\n",m_callwlated_temp);

    //Verify OVERT pad stays asserted
    data = 0;
    Platform::EscapeRead("failsafe_overt", 0, 1, &data);
    if (data != 1) {
        ErrPrintf("1 : failsafe should be 1, but now it's 0x%x \n", data);
        return (1);
    }
    Platform::EscapeRead("overt", 0, 1, &data);
    if (data != 0) {
        ErrPrintf("2 : overt pad should be 0, but now it's 0x%x \n", data);
        return (1);
    }
    //assert pex reset
    Platform::EscapeWrite("pex_reset_",0,1,0);
    Platform::Delay(50);
    //Verify OVERT pad remains asserted
    data = 0;
    Platform::EscapeRead("failsafe_overt", 0, 1, &data);
    if (data != 1) {
        ErrPrintf("3 : failsafe should be 1, but now it's 0x%x \n", data);
        return (1);
    }
    Platform::EscapeRead("overt", 0, 1, &data);
    if (data != 0) {
        ErrPrintf("4 : overt pad should be 0, but now it's 0x%x \n", data);
        return (1);
    }
    //deassert pex reset
    Platform::EscapeWrite("pex_reset_",0,1,1);
    Platform::Delay(50);

    //Set TSENSE_MINI temperature below SCI OVERT threshold
    Platform::EscapeWrite("tsense_mini_temp",0,32,temp94_rawcode);
    Platform::Delay(200); // This is callwlated based on the ts_clk freq as13.5MHz. This clk frequency is achieved by programming LW_PGC6_SCI_FS_OVERT_TS_SM_TS_CLK_DIV to 0x2 At this frequency, ts_clk requires 155us for completing 2048 clk cycles. This value of 155us is rounded to 200us. be in agreement of TS_CLK_DIV because tsense_mini_temp will take effect in maximum of 2100 TS_CLK cycles.
    Platform::EscapeRead("sci_fs_overt_temperature_integer",0,14,&m_callwlated_temp);
    Platform::EscapeRead("tsense_mini_temp",0,32,&m_ts_adc_raw_code);
    InfoPrintf("programmed LW_PGC6_SCI_FS_OVERT_TS_ADC_RAW_CODE = 0x%x.\n",m_ts_adc_raw_code);
    InfoPrintf("callwlated LW_PGC6_SCI_FS_OVERT_TS_ADC_RAW_CODE = 0x%x.\n",m_callwlated_temp);
    //Platform::Delay(20);

    data = 0;
    Platform::EscapeRead("overt", 0, 1, &data);
    if (data != 0) {
        ErrPrintf("5 : overt pad should be 0, but now it's 0x%x \n", data);
        return (1);
    }

    Platform::EscapeRead("failsafe_overt", 0, 1, &data);
    if (data != 0) {
        ErrPrintf("6 : failsafe should be 0, but now it's 0x%x \n", data);
        return (1);
    }
    //assert pex reset
    Platform::EscapeWrite("pex_reset_",0,1,0);
    Platform::Delay(50);

    //verify that OVERT clears
    data = 0;
    Platform::EscapeRead("failsafe_overt", 0, 1, &data);
    if (data != 0) {
        ErrPrintf("7 : failsafe should be 0, but now it's 0x%x \n", data);
        return (1);
    }
    Platform::EscapeRead("overt", 0, 1, &data);
    if (data != 1) {
        ErrPrintf("8 : overt pad should be 1, but now it's 0x%x \n", data);
        return (1);
    }
    //deassert pex reset
    Platform::EscapeWrite("pex_reset_",0,1,1);
    Platform::Delay(5);

    // deassert hot reset
    Platform::EscapeWrite("exitHotReset",0,1,1);
    InfoPrintf("hot_reset : Exiting hot reset ...\n");
    Platform::Delay(20);
	
    RestorePciReg(pcie_gen);
       Platform::Delay(30);

    // ------------------------------
    // Initialize GPU
    // ==============
    gpu_initialize();

    //Make sure GR is out of reset.
    enable_gr_pfifo_engine();

    Platform::Delay(1000);

    return(0);
}
    UINT32
FailSafeOvertPin::CheckHotRestFusesSensedHighTemp()
{
    temp126_rawcode = temperature2code_nominal(126);
    InfoPrintf("returned raw_code = 0x%x \n",temp126_rawcode);
    temp94_rawcode = temperature2code_nominal(94);
    InfoPrintf("returned raw_code = 0x%x \n",temp94_rawcode);

    SCI_initial_programming(0);

    SavePciReg(pcie_gen);
   
    InfoPrintf("hot_reset : asserting HotReset\n");
    // Set _ASSERT_FUNDAMENTAL_RESET_ON_HOT_RESET disabled before asserting hot reset
    lwgpu->SetRegFldDef("LW_PCFG_XVE_RESET_CTRL","_ASSERT_FUNDAMENTAL_RESET_ON_HOT_RESET","_DISABLED");
    Platform::Delay(10);
    InfoPrintf("hot_reset : asserting HotReset\n");
    // Send in Hot reset
    Platform::EscapeWrite("enterHotReset",0,1,1);
    Platform::Delay(20);

    //verifying bandgap fuse values with opt_sci_bg_valid is 0
    data =0;
    Platform::EscapeRead("sci_fs_overt_fuse_bg_temp_coeff", 0, 1, &data);
    if (data != bg_temp_coeff) {
        ErrPrintf("1 : BG_TEMP_COEFF should be 0x%x, but now it's 0x%x \n",bg_temp_coeff,data );
        return (1);
    }else{
        InfoPrintf("LW_PGC6_SCI_FS_OVERT_TS_ADJ_BG_TEMP_COEFF = 0x%x\n",data);
    }
    data =0;
    Platform::EscapeRead("sci_fs_overt_fuse_bg_voltage", 0, 1, &data);
    if (data != bg_voltage) {
        ErrPrintf("2 : bg_voltage should be 0x%x, but now it's 0x%x \n",bg_voltage,data );
        return (1);
    }else{
        InfoPrintf("LW_PGC6_SCI_FS_OVERT_TS_ADJ_BG_VOLTAGE = 0x%x\n",data);
    }

    //set LW_PGC6_SCI_FS_OVERT_TS_ADC_RAW_CODE
    //TEMPERATURE = (FINAL_A * RAW_CODE / 16384) - (FINAL_B / 2)
    InfoPrintf("Set temp to 126C through escape write  \n");
    Platform::EscapeWrite("ts_adc_out",0,14,temp126_rawcode);
    Platform::Delay(30);
    Platform::EscapeRead("sci_fs_overt_temperature_integer",0,14,&m_callwlated_temp);
    Platform::EscapeRead("ts_adc_out",0,14,&m_ts_adc_raw_code);
    InfoPrintf("programmed LW_PGC6_SCI_FS_OVERT_TS_ADC_RAW_CODE = 0x%x.\n",m_ts_adc_raw_code);
    InfoPrintf("callwlated LW_PGC6_SCI_FS_OVERT_TS_ADC_RAW_CODE = 0x%x.\n",m_callwlated_temp);

    //Verify OVERT pad stays asserted
    data = 0;
    Platform::EscapeRead("failsafe_overt", 0, 1, &data);
    if (data != 0) {
        ErrPrintf("3 : failsafe should be 0, but now it's 0x%x \n", data);
        return (1);
    }
    Platform::EscapeRead("overt", 0, 1, &data);
    if (data != 1) {
        ErrPrintf("4 : overt pad should be 1, but now it's 0x%x \n", data);
        return (1);
    }

    // deassert hot reset
    Platform::EscapeWrite("exitHotReset",0,1,1);
    InfoPrintf("hot_reset : Exiting hot reset ...\n");
    Platform::Delay(20);

    RestorePciReg(pcie_gen);

    Platform::Delay(30);

    // ------------------------------
    // Initialize GPU
    // ==============
    gpu_initialize();

    //Make sure GR is out of reset.
    enable_gr_pfifo_engine();

    Platform::Delay(1000);

    return(0);
}
    UINT32
FailSafeOvertPin::CheckHotRestFusesSensedLowTemp()
{
    temp126_rawcode = temperature2code_nominal(126);
    InfoPrintf("returned raw_code = 0x%x \n",temp126_rawcode);
    temp94_rawcode = temperature2code_nominal(94);
    InfoPrintf("returned raw_code = 0x%x \n",temp94_rawcode);
    temp94_final_rawcode = temperature2code_final(94);
    InfoPrintf("returned raw_code = 0x%x \n",temp94_final_rawcode);

    SCI_initial_programming(0);
    SavePciReg(pcie_gen);
   
    InfoPrintf("hot_reset : asserting HotReset\n");
    // Set _ASSERT_FUNDAMENTAL_RESET_ON_HOT_RESET disabled before asserting hot reset
    lwgpu->SetRegFldDef("LW_PCFG_XVE_RESET_CTRL","_ASSERT_FUNDAMENTAL_RESET_ON_HOT_RESET","_DISABLED");
    Platform::Delay(10);
    InfoPrintf("hot_reset : asserting HotReset\n");
    // Send in Hot reset
    Platform::EscapeWrite("enterHotReset",0,1,1);
    Platform::Delay(20);

    //verifying bandgap fuse values with opt_sci_bg_valid is 1
    data =0;
    Platform::EscapeRead("sci_fs_overt_fuse_bg_temp_coeff", 0, 1, &data);
    if (data != bg_temp_coeff) {
        ErrPrintf("1 : BG_TEMP_COEFF should be 0x%x, but now it's 0x%x \n",bg_temp_coeff,data );
        return (1);
    }else{
        InfoPrintf("LW_PGC6_SCI_FS_OVERT_TS_ADJ_BG_TEMP_COEFF = 0x%x\n",data);
    }
    data =0;
    Platform::EscapeRead("sci_fs_overt_fuse_bg_voltage", 0, 1, &data);
    if (data != bg_voltage) {
        ErrPrintf("2 : bg_voltage should be 0x%x, but now it's 0x%x \n",bg_voltage,data );
        return (1);
    }else{
        InfoPrintf("LW_PGC6_SCI_FS_OVERT_TS_ADJ_BG_VOLTAGE = 0x%x\n",data);
    }

    //set LW_PGC6_SCI_FS_OVERT_TS_ADC_RAW_CODE
    //TEMPERATURE = (FINAL_A * RAW_CODE / 16384) - (FINAL_B / 2)
    InfoPrintf("Set temp to 94C through escape write  \n");
    Platform::EscapeWrite("ts_adc_out",0,14,temp94_rawcode);
    Platform::Delay(30);
    Platform::EscapeRead("sci_fs_overt_temperature_integer",0,14,&m_callwlated_temp);
    Platform::EscapeRead("ts_adc_out",0,14,&m_ts_adc_raw_code);
    InfoPrintf("programmed LW_PGC6_SCI_FS_OVERT_TS_ADC_RAW_CODE = 0x%x.\n",m_ts_adc_raw_code);
    InfoPrintf("callwlated LW_PGC6_SCI_FS_OVERT_TS_ADC_RAW_CODE = 0x%x.\n",m_callwlated_temp);

    //Verify OVERT pad stays asserted
    data = 0;
    Platform::EscapeRead("failsafe_overt", 0, 1, &data);
    if (data != 1) {
        ErrPrintf("3 : failsafe should be 1, but now it's 0x%x \n", data);
        return (1);
    }
    Platform::EscapeRead("overt", 0, 1, &data);
    if (data != 0) {
        ErrPrintf("4 : overt pad should be 0, but now it's 0x%x \n", data);
        return (1);
    }
    // deassert hot reset
    Platform::EscapeWrite("exitHotReset",0,1,1);
    InfoPrintf("hot_reset : Exiting hot reset ...\n");
    Platform::Delay(20);

    InfoPrintf("Set temp to 94C through escape write  \n");
    Platform::EscapeWrite("ts_adc_out",0,14,temp94_final_rawcode);
    Platform::Delay(30);
    Platform::EscapeRead("sci_fs_overt_temperature_integer",0,14,&m_callwlated_temp);
    Platform::EscapeRead("ts_adc_out",0,14,&m_ts_adc_raw_code);
    InfoPrintf("programmed LW_PGC6_SCI_FS_OVERT_TS_ADC_RAW_CODE = 0x%x.\n",m_ts_adc_raw_code);
    InfoPrintf("callwlated LW_PGC6_SCI_FS_OVERT_TS_ADC_RAW_CODE = 0x%x.\n",m_callwlated_temp);

    //assert pex reset to clear overt latch
    Platform::EscapeWrite("pex_reset_",0,1,0);
    Platform::Delay(50);

    //de-assert pex reset
    Platform::EscapeWrite("pex_reset_",0,1,1);
    Platform::Delay(50);

    RestorePciReg(pcie_gen);
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
FailSafeOvertPin::gpu_initialize()
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
    // be invalid/dangling. LWGpuResource::ScanSystem does this job
    // well.
    LWGpuResource::ScanSystem(CommandLine::ArgDb());

    Platform::SleepUS(80);

    InfoPrintf("Done!\n");

    //Fetch all the valid pointers for this new gpu, since it was reinitialized
    MASSERT(SetupGpuPointers());

    return 0;
}

    void
FailSafeOvertPin::enable_gr_pfifo_engine()
{
    InfoPrintf("Enabling GR/Pfifo engine \n");
   
   
    //Bars are setup
    //restore PMC_ENABLE bits
    lwgpu->SetRegFldNum("LW_PMC_DEVICE_ENABLE(0)", "", m_pmc_device_enable);
    lwgpu->SetRegFldNum("LW_PMC_DEVICE_ELPG_ENABLE(0)", "", m_pmc_device_elpg_enable);
    lwgpu->SetRegFldNum("LW_PMC_ENABLE", "", m_pmc_enable);
    lwgpu->SetRegFldNum("LW_PMC_ELPG_ENABLE(0)", "", m_pmc_elpg_enable);
    

}
void
FailSafeOvertPin::SCI_initial_programming(UINT32 s)
{
    //Increasing the TS_CLK frequency to reduce the sim time.
    lwgpu->SetRegFldNum("LW_PGC6_SCI_FS_OVERT_TS_SM","_TS_CLK_DIV",0x2);
    Platform::Delay(10);
    lwgpu->GetRegFldNum("LW_PGC6_SCI_FS_OVERT_TS_SM","_TS_CLK_DIV",&m_ts_clk);
    InfoPrintf("LW_PGC6_SCI_FS_OVERT_TS_SM_TS_CLK_DIV = 0x%x\n",m_ts_clk);
    if (m_ts_clk!= 2) {
        ErrPrintf("LW_PGC6_SCI_FS_OVERT_TS_SM_TS_CLK_DIV should be 2, but now it's 0x%x \n", m_ts_clk);
    }

    //Programming this LW_PGC6_SCI_FS_OVERT_TS_SM_POLLING_PERIOD to ZERO will disable polling and cause the TS to sample the temperature every TS_CLK
    lwgpu->SetRegFldDef("LW_PGC6_SCI_FS_OVERT_TS_SM","_POLLING_PERIOD","_NONE");
    Platform::Delay(10);
    lwgpu->GetRegFldNum("LW_PGC6_SCI_FS_OVERT_TS_SM","_POLLING_PERIOD",&m_polling_period);
    InfoPrintf("LW_PGC6_SCI_FS_OVERT_TS_SM_POLLING_PERIOD = 0x%x\n",m_polling_period);
    if (m_polling_period!= 0) {
        ErrPrintf("LW_PGC6_SCI_FS_OVERT_TS_SM_POLLING_PERIOD should be 0, but now it's 0x%x \n", m_polling_period);
    }

    //Programming this LW_PGC6_SCI_FS_OVERT_TS_SM_STABLE_SCALE to _16_TS_CLK will REDUCE SIMULATION TIME
    lwgpu->SetRegFldDef("LW_PGC6_SCI_FS_OVERT_TS_SM","_STABLE_SCALE","_16_TS_CLK");
    Platform::Delay(10);
    lwgpu->GetRegFldNum("LW_PGC6_SCI_FS_OVERT_TS_SM","_STABLE_SCALE",&m_stable_scale);
    InfoPrintf("LW_PGC6_SCI_FS_OVERT_TS_SM_STABLE_SCALE = 0x%x\n",m_stable_scale);
    if (m_stable_scale!= 0) {
        ErrPrintf("LW_PGC6_SCI_FS_OVERT_TS_SM_STABLE_SCALE should be 0, but now it's 0x%x \n", m_stable_scale);
    }

    lwgpu->GetRegFldNum("LW_PGC6_SCI_FS_OVERT_THRESH","_TIME_PERIOD",&m_time_period);
    InfoPrintf("Set LW_PGC6_SCI_FS_OVERT_THRESH_TIME_PERIOD = 0x%x.\n",m_time_period);
    if (m_time_period!= 1) {
        ErrPrintf("LW_PGC6_SCI_FS_OVERT_THRESH_TIME_PERIOD should be 1, but now it's 0x%x \n", m_time_period);
    }

    lwgpu->SetRegFldDef("LW_PGC6_SCI_FS_OVERT_THRESH","_TIME_SCALE","_1US");
    Platform::Delay(10);
    lwgpu->GetRegFldNum("LW_PGC6_SCI_FS_OVERT_THRESH","_TIME_SCALE",&m_thresh_time_scale);
    InfoPrintf("Set LW_PGC6_SCI_FS_OVERT_THRESH_TIME_SCALE = 0x%x.\n",m_thresh_time_scale);
    if (m_thresh_time_scale!= 0) {
        ErrPrintf("LW_PGC6_SCI_FS_OVERT_THRESH_TIME_SCALE should be 0, but now it's 0x%x \n", m_thresh_time_scale);
    }

    lwgpu->SetRegFldNum("LW_PGC6_SCI_FAN_TEMP_ALERT","_HI",0x90);
    Platform::Delay(10);
    lwgpu->GetRegFldNum("LW_PGC6_SCI_FAN_TEMP_ALERT","_HI",&m_temp_alert_hi);
    InfoPrintf("LW_PGC6_SCI_FAN_TEMP_ALERT_HI = 0x%x\n",m_temp_alert_hi);
    if (m_temp_alert_hi!= 0x90) {
        ErrPrintf("LW_PGC6_SCI_FAN_TEMP_ALERT_HI should be 0x90, but now it's 0x%x \n", m_temp_alert_hi);
    }

    lwgpu->SetRegFldDef("LW_PGC6_SCI_FAN_TEMP_ALERT","_OVERRIDE","_ENABLE");
    Platform::Delay(10);
    lwgpu->GetRegFldNum("LW_PGC6_SCI_FAN_TEMP_ALERT","_OVERRIDE",&m_temp_alert_override);
    InfoPrintf("LW_PGC6_SCI_FAN_TEMP_ALERT_OVERRIDE = 0x%x\n",m_temp_alert_override);
    if ( m_temp_alert_override!= 0x1) {
        ErrPrintf("LW_PGC6_SCI_FAN_TEMP_ALERT_OVERRIDE should be 0x1, but now it's 0x%x \n", m_temp_alert_override);
    }

    lwgpu->SetRegFldDef("LW_PGC6_SCI_FAN_TEMP_ALERT","_FORCE","_NO");
    Platform::Delay(10);
    lwgpu->GetRegFldNum("LW_PGC6_SCI_FAN_TEMP_ALERT","_FORCE",&m_temp_alert_force);
    InfoPrintf("LW_PGC6_SCI_FAN_TEMP_ALERT_FORCE = 0x%x\n",m_temp_alert_force);
    if ( m_temp_alert_force!= 0x0) {
        ErrPrintf("LW_PGC6_SCI_FAN_TEMP_ALERT_FORCE should be 0x0, but now it's 0x%x \n", m_temp_alert_force);
    }

    lwgpu->SetRegFldNum("LW_PGC6_SCI_FAN_CFG1","_HI",0x48);
    Platform::Delay(10);
    lwgpu->GetRegFldNum("LW_PGC6_SCI_FAN_CFG1","_HI",&m_fan_cfg1_hi);
    InfoPrintf("LW_PGC6_SCI_FAN_CFG1_HI = 0x%x\n",m_fan_cfg1_hi);
    if (m_fan_cfg1_hi!= 0x48) {
        ErrPrintf("LW_PGC6_SCI_FAN_CFG1_HI should be 0x48, but now it's 0x%x \n", m_fan_cfg1_hi);
    }

    lwgpu->SetRegFldDef("LW_PGC6_SCI_FAN_CFG1","_UPDATE","_TRIGGER");
    Platform::Delay(10);
    lwgpu->GetRegFldNum("LW_PGC6_SCI_FAN_CFG1","_UPDATE",&m_fan_cfg1_update);
    InfoPrintf("LW_PGC6_SCI_FAN_CFG1_UPDATE = 0x%x\n",m_fan_cfg1_update);
    /*if (m_fan_cfg1_update!= 0x1) {
        ErrPrintf("LW_PGC6_SCI_FAN_CFG1_HI should be 0x48, but now it's 0x%x \n", m_fan_cfg1_update);
    }*/
    if (s == 0){
    lwgpu->SetRegFldDef("LW_FUSE_CTRL_OPT_GC6_ISLAND","_DATA","_DISABLE");
    Platform::Delay(10);
    lwgpu->GetRegFldNum("LW_FUSE_CTRL_OPT_GC6_ISLAND","_DATA",&m_gc6_island);
    InfoPrintf("LW_FUSE_CTRL_OPT_GC6_ISLAND_DATA = 0x%x\n",m_gc6_island);
    if (m_gc6_island!= 0x1) {
        ErrPrintf("LW_FUSE_CTRL_OPT_GC6_ISLAND_DATA should be 0x1, but now it's 0x%x \n", m_gc6_island);
    }
    }
}
