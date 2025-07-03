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

#include "therm_overt_legacy.h"

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

extern SObject Gpu_Object;

extern const ParamDecl therm_overt_legacy_params[] =
{
    SIMPLE_PARAM("-check_legacy_therm_overt",            "Testing the legacy therm overt functionality"),
    PARAM_SUBDECL(lwgpu_single_params),
    LAST_PARAM
};

static int run_legacy_therm_overt = 0;

LegacyThermOvert::LegacyThermOvert(ArgReader *params) :
    LWGpuSingleChannelTest(params)
{
    if  (params->ParamPresent("-check_legacy_therm_overt"))
        run_legacy_therm_overt = 1;
}

LegacyThermOvert::~LegacyThermOvert(void)
{
}

STD_TEST_FACTORY(therm_overt_legacy, LegacyThermOvert)

INT32
LegacyThermOvert::Setup(void)
{
    lwgpu = LWGpuResource::FindFirstResource();

    ch = lwgpu->CreateChannel();
    if(!ch)
    {
        ErrPrintf("LegacyThermOvert: Setup failed to create channel\n");
        return (0);
    }

    m_regMap = lwgpu->GetRegisterMap();
    if(!m_regMap)
    {
        ErrPrintf("LegacyThermOvert: it can only be run on GPU's that support a "
            "register map!\n");
        return (0);
    }

    getStateReport()->init("legcy_therm_overt_gv100");
    getStateReport()->enable();

    return 1;
}

void
LegacyThermOvert::CleanUp(void)
{
    if(ch)
    {
        delete ch;
        ch = 0;
    }

    if (lwgpu)
    {
        DebugPrintf("LegacyThermOvert::CleanUp(): Releasing GPU.\n");
        lwgpu = 0;
    }
    LWGpuSingleChannelTest::CleanUp();
}

void
LegacyThermOvert::Run(void)
{
    SetStatus(TEST_INCOMPLETE);
    InfoPrintf("Starting LegacyThermOvert test\n");
    if (run_legacy_therm_overt)
    {
        InfoPrintf("Starting checking overt latch enable \n");
        if(CheckLegacyThermOvert())
        {
            SetStatus(TEST_FAILED);
            getStateReport()->runFailed(" Test of check_legacy_therm_overt failed \n");
            ErrPrintf("legacy_therm_overt :: check_legacy_therm_overt test failed \n");
            return;
        }
    }
    SetStatus(TEST_SUCCEEDED);
    getStateReport()->crcCheckPassedGold();
}

UINT32  
LegacyThermOvert::SavePciReg()
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
LegacyThermOvert::RestorePciReg()
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
LegacyThermOvert::CheckLegacyThermOvert()
{
    UINT32 data;
    UINT32 m_callwlated_temp;
    UINT32 m_programmed_temp;
    UINT32 m_time_period;
    UINT32 m_overt_enable;
    UINT32 m_threshold_temp;
    UINT32 m_polling_period;
    UINT32 m_stable_period;
    UINT32 m_thresh_time_scale;
    UINT32 m_fs_padctl;
    UINT32 m_failsafe_reg;
    UINT32 m_latchovert_reg;

    lwgpu->GetRegFldNum("LW_PGC6_SCI_FS_OVERT_TS_ADC","_RAW_CODE", &m_programmed_temp);
    InfoPrintf("poweron value of LW_PGC6_SCI_FS_OVERT_TS_ADC_RAW_CODE = 0x%x.\n",m_programmed_temp);
    lwgpu->GetRegFldNum("LW_PGC6_SCI_FS_OVERT_TEMPERATURE","_INTEGER", &m_callwlated_temp);
    InfoPrintf("poweron value of callwlated temp= 0x%x,\n",m_callwlated_temp);
    lwgpu->GetRegFldNum("LW_PGC6_SCI_FS_OVERT_THRESH","_TIME_PERIOD",&m_time_period);
    InfoPrintf("poweron value of OVERT threshold time period= 0x%x,\n",m_time_period);
    lwgpu->GetRegFldNum("LW_PGC6_SCI_FS_OVERT_THRESH","_SCI_OVERT", &m_overt_enable);
    InfoPrintf("poweron value of SCI OVERT enable = 0x%x,\n",m_overt_enable);
    lwgpu->GetRegFldNum("LW_PGC6_SCI_FS_OVERT_THRESH","_TEMP", &m_threshold_temp);
    InfoPrintf("poweron value of overt threshold temp = 0x%x,\n",m_threshold_temp);
    lwgpu->GetRegFldNum("LW_PGC6_SCI_FS_OVERT_TS_SM","_POLLING_PERIOD",&m_polling_period);
    InfoPrintf("poweron value of LW_PGC6_SCI_FS_OVERT_TS_SM_POLLING_PERIOD = 0x%x\n",m_polling_period);
    lwgpu->GetRegFldNum("LW_PGC6_SCI_FS_OVERT_TS_SM","_STABLE_PERIOD",&m_stable_period);
    InfoPrintf("poweron value of LW_PGC6_SCI_FS_OVERT_TS_SM_STABLE_PERIOD = 0x%x\n",m_stable_period);
    lwgpu->GetRegFldNum("LW_PGC6_SCI_FS_OVERT_PADCTL","_FAILSAFE_OVERT",&m_failsafe_reg);
    InfoPrintf("poweron LW_PGC6_SCI_FS_OVERT_PADCTL_FAILSAFE_OVERT = 0x%x\n",m_failsafe_reg);
    lwgpu->GetRegFldNum("LW_PGC6_SCI_FS_OVERT_PADCTL","_LATCH_OVERT",&m_latchovert_reg);
    InfoPrintf("Before pex reset LW_PGC6_SCI_FS_OVERT_PADCTL_LATCH_OVERT = 0x%x\n",m_latchovert_reg);

    Platform::Delay(2);
    lwgpu->GetRegFldNum("LW_PGC6_SCI_FS_OVERT_THRESH","_SCI_OVERT", &m_overt_enable);
    InfoPrintf("Set  LW_PGC6_SCI_FS_OVERT_THRESH_SCI_OVERT= 0x%x.\n",m_overt_enable);

    lwgpu->GetRegFldNum("LW_PGC6_SCI_FS_OVERT_PADCTL","_LATCH_OVERT",&m_latchovert_reg);
    InfoPrintf("After pex reset and before OVERT, LW_PGC6_SCI_FS_OVERT_PADCTL_LATCH_OVERT = 0x%x\n",m_latchovert_reg);

    //Programming this LW_PGC6_SCI_FS_OVERT_TS_SM_POLLING_PERIOD to ZERO will disable polling and cause the TS to sample the temperature every TS_CLK
    lwgpu->SetRegFldDef("LW_PGC6_SCI_FS_OVERT_TS_SM","_POLLING_PERIOD","_NONE");
    Platform::Delay(10);
    lwgpu->GetRegFldNum("LW_PGC6_SCI_FS_OVERT_TS_SM","_POLLING_PERIOD",&m_polling_period);
    InfoPrintf("LW_PGC6_SCI_FS_OVERT_TS_SM_POLLING_PERIOD = 0x%x\n",m_polling_period);

    //Programming this LW_PGC6_SCI_FS_OVERT_TS_SM_STABLE_PERIOD to NONE will cause the TS SM to skip the settling delay and sample the TS output immediately
    lwgpu->SetRegFldDef("LW_PGC6_SCI_FS_OVERT_TS_SM","_STABLE_SCALE","_16_TS_CLK");
    Platform::Delay(10);
    lwgpu->GetRegFldNum("LW_PGC6_SCI_FS_OVERT_TS_SM","_STABLE_SCALE",&m_stable_scale);
    InfoPrintf("LW_PGC6_SCI_FS_OVERT_TS_SM_STABLE_PERIOD = 0x%x\n",m_stable_scale);

    lwgpu->SetRegFldDef("LW_PGC6_SCI_FS_OVERT_THRESH","_TIME_SCALE","_1US");
    Platform::Delay(10);
    lwgpu->GetRegFldNum("LW_PGC6_SCI_FS_OVERT_THRESH","_TIME_SCALE",&m_thresh_time_scale);
    InfoPrintf("Set LW_PGC6_SCI_FS_OVERT_THRESH_TIME_SCALE = 0x%x.\n",m_thresh_time_scale);

    lwgpu->SetRegFldDef("LW_PGC6_SCI_FS_OVERT_PADCTL","_CFG_FAILSAFE","_DISABLE");
    Platform::Delay(10);
    lwgpu->GetRegFldNum("LW_PGC6_SCI_FS_OVERT_PADCTL","_CFG_FAILSAFE",&m_fs_padctl);
    InfoPrintf("Set LW_PGC6_SCI_FS_OVERT_PADCTL_CFG_FAILSAFE = 0x%x.\n",m_fs_padctl);

    //set LW_PGC6_SCI_FS_OVERT_TS_ADC_RAW_CODE
    InfoPrintf("Set temp to 125C through escape write  \n");
    Platform::EscapeWrite("ts_adc_out",0,14,0x3bf0);
    Platform::Delay(10);
    Platform::EscapeRead("ts_adc_out",0,14,&m_programmed_temp);
    InfoPrintf("programmed LW_PGC6_SCI_FS_OVERT_TS_ADC_RAW_CODE = 0x%x.\n",m_programmed_temp);
    Platform::Delay(100);

    lwgpu->GetRegFldNum("LW_PGC6_SCI_FS_OVERT_TS_ADC","_RAW_CODE",&m_raw_code);
    InfoPrintf("Read LW_PGC6_SCI_FS_OVERT_TS_ADC_RAW_CODE = 0x%x.\n",m_raw_code);

    lwgpu->GetRegFldNum("LW_PGC6_SCI_FS_OVERT_TEMPERATURE","_INTEGER", &m_callwlated_temp) ;
    InfoPrintf("callwlated temp = 0x%x,\n",m_callwlated_temp);
    //set LW_PGC6_SCI_FS_OVERT_THRESH_TIME_SCALE, this helps in detecting or clearing overt condition immediately.
    lwgpu->SetRegFldDef("LW_PGC6_SCI_FS_OVERT_THRESH","TIME_SCALE","_1US");
    Platform::Delay(10);
    lwgpu->GetRegFldNum("LW_PGC6_SCI_FS_OVERT_THRESH","_TIME_SCALE",&m_time_period_scale);
    InfoPrintf("Set LW_PGC6_SCI_FS_OVERT_THRESH_TIME_SCALE = 0x%x.\n",m_time_period_scale);
    Platform::Delay(2);

    lwgpu->GetRegFldNum("LW_PGC6_SCI_FS_OVERT_PADCTL","_FAILSAFE_OVERT",&m_failsafe_reg);
    InfoPrintf("failsafe signal when temp is 125C LW_PGC6_SCI_FS_OVERT_PADCTL_FAILSAFE_OVERT = 0x%x\n",m_failsafe_reg);
    Platform::Delay(10);
    lwgpu->GetRegFldNum("LW_PGC6_SCI_FS_OVERT_PADCTL","_LATCH_OVERT",&m_latchovert_reg);
    InfoPrintf("overt latch signal when temp is 125C LW_PGC6_SCI_FS_OVERT_PADCTL_LATCH_OVERT = 0x%x\n",m_latchovert_reg);

    data = 0;
    Platform::EscapeRead("failsafe_overt", 0, 1, &data);
    if (data != 1) 
    {
        InfoPrintf("1 : failsafe should be 1, but now it's 0x%x \n", data);
        return (1);
    }
    InfoPrintf("1: LEGACY OVERT  : failsafe is now 0x%x \n", data);

    Platform::EscapeRead("overt", 0, 1, &data);
    if (data != 1) 
    {
        InfoPrintf("2 : overt pad should be 1, but now it's 0x%x \n", data);
        return (1);
    }
    InfoPrintf("2: LEGACY OVERT  : overt_pad is now 0x%x \n", data);

    Platform::EscapeRead("overt_detect", 0, 1, &data);
    if (data != 1) 
    {
        InfoPrintf("3 : overt_detect pad should be 1, but now it's 0x%x \n", data);
        return (1);
    }
    InfoPrintf(" 3 LEGACY OVERT  : overt_detect_pad is now 0x%x \n", data);

    //THERM OVERT

    //// Initial temperature as 120C
    Platform::EscapeWrite("therm_ts_adc_out", 0, 14, temp2code(120));

    //
    InfoPrintf("Enable  LW_THERM_OVERT_CTRL_OTOB_ENABLE \n");
    if (lwgpu->SetRegFldDef("LW_THERM_OVERT_CTRL","_OTOB_ENABLE","_ON")) 
        InfoPrintf("Set LW_THERM_OVERT_CTRL_OTOB_ENABLE = _ON.\n");
    lwgpu->SetRegFldDef("LW_THERM_SENSOR_1", "_SELECT_SW_A", "_OFF");
    lwgpu->SetRegFldDef("LW_THERM_SENSOR_1", "_SELECT_SW_B", "_OFF");
    lwgpu->SetRegFldDef("LW_THERM_OVERT_CTRL","_THERM_SHUTDOWN_LATCH_ENABLE","_ON");

    InfoPrintf("set LW_THERM_EVT_DEDICATED_OVERT_FILTER_PERIOD_NONE  \n");
    if (lwgpu->SetRegFldDef("LW_THERM_EVT_DEDICATED_OVERT","_FILTER_PERIOD","_NONE")) 
        InfoPrintf("LW_THERM_EVT_DEDICATED_OVERT_FILTER_PERIOD = NONE.\n");

    InfoPrintf("set LW_THERM_EVT_DEDICATED_OVERT_TEMP_SENSOR_ID_TSENSE  \n");
    if (lwgpu->SetRegFldDef("LW_THERM_EVT_DEDICATED_OVERT","_TEMP_SENSOR_ID","_TSENSE")) 
        InfoPrintf("LW_THERM_EVT_DEDICATED_OVERT_TEMP_SENSOR_ID = _TSENSE.\n");

    InfoPrintf("set LW_THERM_SENSOR_1_TS_STABLE_CNT to 0x9  \n");
    if (lwgpu->SetRegFldNum("LW_THERM_SENSOR_1","_TS_STABLE_CNT",0x9)) 
        InfoPrintf("LW_THERM_SENSOR_1_TS_STABLE_CNT = 0x9.\n");
    InfoPrintf("set LW_THERM_SENSOR_6_POLLING_INTERVAL_PERIOD to 0x1 \n");
    if (lwgpu->SetRegFldNum("LW_THERM_SENSOR_6","_POLLING_INTERVAL_PERIOD",0x1)) 
        InfoPrintf("LW_THERM_SENSOR_6_POLLING_INTERVAL_PERIOD = 0x1.\n");

    InfoPrintf("set LW_THERM_SENSOR_6_POLLING_INTERVAL_SCALE_16US \n");
    if (lwgpu->SetRegFldNum("LW_THERM_SENSOR_6","_POLLING_INTERVAL_SCALE",0x0)) 
        InfoPrintf("LW_THERM_SENSOR_6_POLLING_INTERVAL_SCALE = 0x0(_16US) \n");

    // Delay 10us to wait the thermal sensor enable
    Platform::Delay(40);
    Platform::EscapeRead("overt", 0, 1, &data);
    if (data != 0) 
    {
        InfoPrintf("4 : overt pad should be 0, but now it's 0x%x \n", data);
        return (1);
    }
    InfoPrintf(" 4: LEGACY OVERT  :  overt pad is now 0x%x \n", data);

    Platform::EscapeRead("overt_detect", 0, 1, &data);
    if (data != 0) 
    {
        InfoPrintf("6 : overt_detect pad should be 0, but now it's 0x%x \n", data);
        return (1);
    }
    InfoPrintf(" 6: LEGACY OVERT  : overt_detect pad is now 0x%x \n", data);

    GpuPtr pGpu;

    SavePciReg();
    ////assert pex reset
    Platform::EscapeWrite("pex_reset_",0,1,0);
    Platform::Delay(50);
    Platform::EscapeRead("overt", 0, 1, &data);
    if (data != 1) 
    {
        InfoPrintf("7 : overt pad should be 1, but now it's 0x%x \n", data);
        return (1);
    }
    InfoPrintf(" 7: LEGACY OVERT  : overt pad is now 0x%x \n", data);
    Platform::EscapeRead("dedicated_overt", 0, 1, &data);
    if (data != 0) 
    {
        InfoPrintf("8 : dedicated_overt should be 0, but now it's 0x%x \n", data);
        //return (1);
    }
    InfoPrintf(" 8: LEGACY OVERT  : dedicated_overt is now 0x%x \n", data);

    Platform::EscapeRead("overt_detect", 0, 1, &data);
    if (data != 1) 
    {
        InfoPrintf("9 : overt_detect pad should be 1, but now it's 0x%x \n", data);
        return (1);
    }
    InfoPrintf(" 9: LEGACY OVERT  :  overt_detect pad  is now 0x%x \n", data);

    //deassert pex reset
    InfoPrintf(" 9a: LEGACY OVERT  : asserting pex reset  \n");
    Platform::EscapeWrite("pex_reset_",0,1,1);
    Platform::Delay(500);

    RestorePciReg();
    Platform::Delay(30);
    InfoPrintf(" 9b LEGACY OVERT  :  initialize GPU \n");

    gpu_initialize();

    enable_gr_pfifo_engine();

    Platform::Delay(1000);

    InfoPrintf("Enable  LW_THERM_OVERT_CTRL_OTOB_ENABLE \n");
    if (lwgpu->SetRegFldDef("LW_THERM_OVERT_CTRL","_OTOB_ENABLE","_ON")) 
        InfoPrintf("Set LW_THERM_OVERT_CTRL_OTOB_ENABLE = _ON.\n");
    lwgpu->SetRegFldDef("LW_THERM_SENSOR_1", "_SELECT_SW_A", "_OFF");
    lwgpu->SetRegFldDef("LW_THERM_SENSOR_1", "_SELECT_SW_B", "_OFF");
    lwgpu->SetRegFldDef("LW_THERM_OVERT_CTRL","_THERM_SHUTDOWN_LATCH_ENABLE","_ON");

    InfoPrintf("set LW_THERM_EVT_DEDICATED_OVERT_FILTER_PERIOD_NONE  \n");
    if (lwgpu->SetRegFldDef("LW_THERM_EVT_DEDICATED_OVERT","_FILTER_PERIOD","_NONE")) 
        InfoPrintf("LW_THERM_EVT_DEDICATED_OVERT_FILTER_PERIOD = NONE.\n");

    InfoPrintf("set LW_THERM_EVT_DEDICATED_OVERT_TEMP_SENSOR_ID_TSENSE  \n");
    if (lwgpu->SetRegFldDef("LW_THERM_EVT_DEDICATED_OVERT","_TEMP_SENSOR_ID","_TSENSE")) 
        InfoPrintf("LW_THERM_EVT_DEDICATED_OVERT_TEMP_SENSOR_ID = _TSENSE.\n");

    InfoPrintf("set LW_THERM_SENSOR_1_TS_STABLE_CNT to 0x9  \n");
    if (lwgpu->SetRegFldNum("LW_THERM_SENSOR_1","_TS_STABLE_CNT",0x9)) 
        InfoPrintf("LW_THERM_SENSOR_1_TS_STABLE_CNT = 0x9.\n");
    InfoPrintf("set LW_THERM_SENSOR_6_POLLING_INTERVAL_PERIOD to 0x1 \n");
    if (lwgpu->SetRegFldNum("LW_THERM_SENSOR_6","_POLLING_INTERVAL_PERIOD",0x1)) 
        InfoPrintf("LW_THERM_SENSOR_6_POLLING_INTERVAL_PERIOD = 0x1.\n");

    InfoPrintf("set LW_THERM_SENSOR_6_POLLING_INTERVAL_SCALE_16US \n");
    if (lwgpu->SetRegFldNum("LW_THERM_SENSOR_6","_POLLING_INTERVAL_SCALE",0x0)) 
        InfoPrintf("LW_THERM_SENSOR_6_POLLING_INTERVAL_SCALE = 0x0(_16US) \n");

    Platform::Delay(50);
    Platform::EscapeRead("overt", 0, 1, &data);
    if (data != 0) 
    {
        InfoPrintf("10 : overt pad should be 0, but now it's 0x%x \n", data);
        return (1);
    }
    InfoPrintf(" 10: LEGACY OVERT  : overt pad is now 0x%x \n", data);
    Platform::EscapeRead("dedicated_overt", 0, 1, &data);
    if (data != 1) 
    {
        InfoPrintf("11 : dedicated_overt should be 1, but now it's 0x%x \n", data);
        //return (1);
    }
    InfoPrintf(" 11: LEGACY OVERT  : dedicated_overt is now 0x%x \n", data);

    Platform::EscapeRead("overt_detect", 0, 1, &data);
    if (data != 0) 
    {
        InfoPrintf("12 : overt_detect pad should be 0, but now it's 0x%x \n", data);
        return (1);
    }
    InfoPrintf(" 12: LEGACY OVERT  : overt_detect pad  is now 0x%x \n", data);
    //// set temperature as 25C
    Platform::EscapeWrite("therm_ts_adc_out", 0, 14, temp2code(25));

    Platform::Delay(50);
    Platform::EscapeRead("overt", 0, 1, &data);
    if (data != 0) 
    {
        InfoPrintf("13 : overt pad should be 0, but now it's 0x%x \n", data);
        return (1);
    }
    InfoPrintf(" 13: LEGACY OVERT  : overt pad  is now 0x%x \n", data);
    Platform::EscapeRead("dedicated_overt", 0, 1, &data);
    if (data != 0) 
    {
        InfoPrintf("14 : dedicated_overt should be 0, but now it's 0x%x \n", data);
        return (1);
    }

    Platform::EscapeRead("overt_detect", 0, 1, &data);
    if (data != 0) 
    {
        InfoPrintf("15 : overt_detect pad should be 0, but now it's 0x%x \n", data);
        return (1);
    }
    InfoPrintf(" 15 : LEGACY OVERT  : overt_detect pad  is now 0x%x \n", data);
    //assert pex reset
    Platform::EscapeWrite("pex_reset_",0,1,0);
    Platform::Delay(50);
    Platform::EscapeRead("overt", 0, 1, &data);
    if (data != 1) 
    {
        InfoPrintf("16 : overt pad should be 1, but now it's 0x%x \n", data);
        return (1);
    }
    InfoPrintf(" 16 : LEGACY OVERT  :  overt pad  is now 0x%x \n", data);
    Platform::EscapeRead("dedicated_overt", 0, 1, &data);
    if (data != 0) {
        InfoPrintf("17 : dedicated_overt should be 0, but now it's 0x%x \n", data);
        //return (1);
    }
    InfoPrintf(" 17 : LEGACY OVERT  : dedicated_overt is now 0x%x \n", data);

    Platform::EscapeRead("overt_detect", 0, 1, &data);
    if (data != 1) 
    {
        InfoPrintf("18 : overt_detect pad should be 1, but now it's 0x%x \n", data);
        return (1);
    }
    InfoPrintf(" 18 : LEGACY OVERT  : overt_detect pad  is now 0x%x \n", data);

    //deassert pex reset
    Platform::EscapeWrite("pex_reset_",0,1,1);
    Platform::Delay(500);
    InfoPrintf(" 18a : LEGACY OVERT  :  deassert pex_reset is now 0x%x \n", data);


    RestorePciReg();
    Platform::Delay(30);

    InfoPrintf(" 18b : LEGACY OVERT  :  initialize GPU \n");

    // ------------------------------
    // Initialize GPU
    // ==============
    gpu_initialize();

    //Make sure GR is out of reset.
    enable_gr_pfifo_engine();

    Platform::Delay(1000);

    InfoPrintf("Enable  LW_THERM_OVERT_CTRL_OTOB_ENABLE \n");
    if (lwgpu->SetRegFldDef("LW_THERM_OVERT_CTRL","_OTOB_ENABLE","_ON")) 
        InfoPrintf("Set LW_THERM_OVERT_CTRL_OTOB_ENABLE = _ON.\n");
    lwgpu->SetRegFldDef("LW_THERM_SENSOR_1", "_SELECT_SW_A", "_OFF");
    lwgpu->SetRegFldDef("LW_THERM_SENSOR_1", "_SELECT_SW_B", "_OFF");
    lwgpu->SetRegFldDef("LW_THERM_OVERT_CTRL","_THERM_SHUTDOWN_LATCH_ENABLE","_ON");

    InfoPrintf("set LW_THERM_EVT_DEDICATED_OVERT_FILTER_PERIOD_NONE  \n");
    if (lwgpu->SetRegFldDef("LW_THERM_EVT_DEDICATED_OVERT","_FILTER_PERIOD","_NONE")) 
        InfoPrintf("LW_THERM_EVT_DEDICATED_OVERT_FILTER_PERIOD = NONE.\n");

    InfoPrintf("set LW_THERM_EVT_DEDICATED_OVERT_TEMP_SENSOR_ID_TSENSE  \n");
    if (lwgpu->SetRegFldDef("LW_THERM_EVT_DEDICATED_OVERT","_TEMP_SENSOR_ID","_TSENSE")) 
        InfoPrintf("LW_THERM_EVT_DEDICATED_OVERT_TEMP_SENSOR_ID = _TSENSE.\n");

    InfoPrintf("set LW_THERM_SENSOR_1_TS_STABLE_CNT to 0x9  \n");
    if (lwgpu->SetRegFldNum("LW_THERM_SENSOR_1","_TS_STABLE_CNT",0x9)) 
        InfoPrintf("LW_THERM_SENSOR_1_TS_STABLE_CNT = 0x9.\n");
    InfoPrintf("set LW_THERM_SENSOR_6_POLLING_INTERVAL_PERIOD to 0x1 \n");
    if (lwgpu->SetRegFldNum("LW_THERM_SENSOR_6","_POLLING_INTERVAL_PERIOD",0x1)) 
        InfoPrintf("LW_THERM_SENSOR_6_POLLING_INTERVAL_PERIOD = 0x1.\n");

    InfoPrintf("set LW_THERM_SENSOR_6_POLLING_INTERVAL_SCALE_16US \n");
    if (lwgpu->SetRegFldNum("LW_THERM_SENSOR_6","_POLLING_INTERVAL_SCALE",0x0)) 
        InfoPrintf("LW_THERM_SENSOR_6_POLLING_INTERVAL_SCALE = 0x0(_16US) \n");

    Platform::Delay(50);
    Platform::EscapeRead("overt", 0, 1, &data);
    if (data != 1) 
    {
        InfoPrintf("19 : overt pad should be 1, but now it's 0x%x \n", data);
        return (1);
    }
    InfoPrintf(" 19 : LEGACY OVERT  :   overt pad is now 0x%x \n", data);
    Platform::EscapeRead("dedicated_overt", 0, 1, &data);
    if (data != 0) 
    {
        InfoPrintf("20 : dedicated_overt should be 0, but now it's 0x%x \n", data);
        //return (1);
    }
    InfoPrintf(" 20 : LEGACY OVERT  :  dedicated_overt is now 0x%x \n", data);

    Platform::EscapeRead("overt_detect", 0, 1, &data);
    if (data != 1) 
    {
        InfoPrintf("21 : overt_detect pad should be 1, but now it's 0x%x \n", data);
        return (1);
    }
    InfoPrintf(" 21 : LEGACY OVERT  : overt_detect pad is now 0x%x \n", data);

    return(0);

}

UINT32
LegacyThermOvert::temp2code(UINT32 temp)
{
    UINT32 code;

        // see bug 330249
        //for gt21x , see bug 330240
        // temperature = temp_code * 0.0261 - 274.2,  nominally

        // SENSOR   |    A_NOMINAL    | B_NOMINAL
        // TSENSE_E | 0.0261  (0x1AC) | -274.2 (0x224)  <-- 16ff (gp100) see dev_therm.ref
        code = UINT32 ((temp + 274.2 + 1) * 38.314176 );

    InfoPrintf("temp %d, code %d\n", temp, code);
    return code;
}

UINT32
LegacyThermOvert::gpu_initialize()
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
LegacyThermOvert::enable_gr_pfifo_engine()
{
  InfoPrintf("Enabling GR/Pfifo engine \n");
  
  //Bars are setup
    //restore PMC_ENABLE bits
    lwgpu->SetRegFldNum("LW_PMC_DEVICE_ENABLE(0)", "", m_pmc_device_enable);
    lwgpu->SetRegFldNum("LW_PMC_DEVICE_ELPG_ENABLE(0)", "", m_pmc_device_elpg_enable);
    lwgpu->SetRegFldNum("LW_PMC_ENABLE", "", m_pmc_enable);
    lwgpu->SetRegFldNum("LW_PMC_ELPG_ENABLE(0)", "", m_pmc_elpg_enable);
}

INT32
LegacyThermOvert::SetupGpuPointers(void)
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


