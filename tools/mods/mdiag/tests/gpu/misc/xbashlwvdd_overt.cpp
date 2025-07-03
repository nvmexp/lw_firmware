/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2009-2011,2017,2019-2021 by LWPU Corporation.  All rights 
 * reserved.  All information contained herein is proprietary and confidential 
 * to LWPU Corporation.  Any use, reproduction, or disclosure without the 
 * written permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

// To facilitate faster compiles via pre-compiled headers, keep "stdtest.h"
// first.
// For style requirements, all lines of code follow the 80 column rule.
#include "mdiag/tests/stdtest.h"

#include "xbashlwvdd_overt.h"

#include "core/include/gpu.h"
#include "core/include/jscript.h"
#include "device/interface/pcie.h"
#include "gpu/include/gpusbdev.h"
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

extern const ParamDecl xbashlwvdd_overt_params[] =
{
    SIMPLE_PARAM("-xbash",            "Testing overt by xbashing lwvdd domain"),
    PARAM_SUBDECL(lwgpu_single_params),
    LAST_PARAM
};

static int run_xbash = 0;

XbashOnOvert::XbashOnOvert(ArgReader *params) :
    LWGpuSingleChannelTest(params)
{
    if  (params->ParamPresent("-xbash"))
        run_xbash = 1;
}
XbashOnOvert::~XbashOnOvert(void)
{
}

STD_TEST_FACTORY(xbashlwvdd_overt, XbashOnOvert)

    INT32
XbashOnOvert::Setup(void)
{
    lwgpu = LWGpuResource::FindFirstResource();

    ch = lwgpu->CreateChannel();
    if(!ch)
    {
        ErrPrintf("XbashOnOvert: Setup failed to create channel\n");
        return (0);
    }

    m_regMap = lwgpu->GetRegisterMap();
    if(!m_regMap)
    {
        ErrPrintf("XbashOnOvert: it can only be run on GPU's that support a "
                "register map!\n");
        return (0);
    }

    getStateReport()->init("xbashlwvdd_overt");
    getStateReport()->enable();

    return 1;
}

    void
XbashOnOvert::CleanUp(void)
{
    if(ch)
    {
        delete ch;
        ch = 0;
    }

    if (lwgpu)
    {
        DebugPrintf("XbashOnOvert::CleanUp(): Releasing GPU.\n");
        lwgpu = 0;
    }
    LWGpuSingleChannelTest::CleanUp();
}

    void
XbashOnOvert::Run(void)
{
    SetStatus(TEST_INCOMPLETE);
    InfoPrintf("Starting XbashOnOvert test\n");
    if (run_xbash)
    {
        if(CheckXbashOnOvert())
        {
            SetStatus(TEST_FAILED);
            getStateReport()->runFailed(" Test of XbashOnOvert failed \n");
            ErrPrintf("XbashOnOvert :: XbashOnOvert  test failed \n");
            return;
        }
    }

    SetStatus(TEST_SUCCEEDED);
    getStateReport()->crcCheckPassedGold();
}
extern SObject Gpu_Object;

UINT32
XbashOnOvert::temperature2code_final(UINT32 temperature)
{
    UINT32 raw_code = temperature * 10;
    InfoPrintf("temp %d, code %x\n", temperature, raw_code);
    return raw_code;
}

void
XbashOnOvert::SCI_initial_programming_xbash()
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

    lwgpu->SetRegFldDef("LW_FUSE_CTRL_OPT_GC6_ISLAND","_DATA","_DISABLE");
    Platform::Delay(10);
    lwgpu->GetRegFldNum("LW_FUSE_CTRL_OPT_GC6_ISLAND","_DATA",&m_gc6_island);
    InfoPrintf("LW_FUSE_CTRL_OPT_GC6_ISLAND_DATA = 0x%x\n",m_gc6_island);
    if (m_gc6_island!= 0x1) {
        ErrPrintf("LW_FUSE_CTRL_OPT_GC6_ISLAND_DATA should be 0x1, but now it's 0x%x \n", m_gc6_island);
    }

}

    UINT32
XbashOnOvert::CheckXbashOnOvert()
{
    temp126_rawcode = temperature2code_final(126);
    InfoPrintf("returned raw_code = 0x%x",temp126_rawcode);
    temp94_rawcode = temperature2code_final(94);
    InfoPrintf("returned raw_code = 0x%x",temp94_rawcode);

    UINT32 address;

    GpuPtr pGpu;

    JavaScriptPtr pJs;
    RC             rc;

    GpuSubdevice * pSubdev;

    pSubdev = lwgpu->GetGpuSubdevice();

    ////////////////////////////
    //Save off some config space
    //
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
    //Save Off PCI parameters for drv
    //
    auto pGpuPcie = pSubdev->GetInterface<Pcie>();
    UINT32 pci_domain_num = pGpuPcie->DomainNumber();
    UINT32 pci_bus_num = pGpuPcie->BusNumber();
    UINT32 pci_dev_num = pGpuPcie->DeviceNumber();
    UINT32 pci_func_num = pGpuPcie->FunctionNumber();

    //save PMC_ENABLE bits
    //required for clean exit
    lwgpu->GetRegFldNum("LW_PMC_DEVICE_ENABLE(0)", "", &m_pmc_device_enable);
    lwgpu->GetRegFldNum("LW_PMC_DEVICE_ELPG_ENABLE(0)", "", &m_pmc_device_elpg_enable);
    lwgpu->GetRegFldNum("LW_PMC_ENABLE", "", &m_pmc_enable);
    lwgpu->GetRegFldNum("LW_PMC_ELPG_ENABLE(0)", "", &m_pmc_elpg_enable);

    SCI_initial_programming_xbash();

    //set LW_PGC6_SCI_FS_OVERT_TS_ADC_RAW_CODE
    //TEMPERATURE = (FINAL_A * RAW_CODE / 16384) - (FINAL_B / 2)
    InfoPrintf("Set temp to 126C through escape write  \n");
    Platform::EscapeWrite("tsense_mini_temp",0,14,temp126_rawcode);
    Platform::Delay(200);//This is callwlated based on the ts_clk freq as13.5MHz. This clk frequency is achieved by programming LW_PGC6_SCI_FS_OVERT_TS_SM_TS_CLK_DIV to 0x2 At this frequency, ts_clk requires 155us for completing 2048 clk cycles. This value of 155us is rounded to 200us.
    Platform::EscapeRead("tsense_mini_temp",0,14,&m_programmed_temp);
    InfoPrintf("programmed LW_PGC6_SCI_FS_OVERT_TS_ADC_RAW_CODE = 0x%x.\n",m_programmed_temp);

    InfoPrintf("Set temp to 94C through escape write  \n");
    Platform::EscapeWrite("tsense_mini_temp",0,14,temp94_rawcode);
    Platform::Delay(200);//This is callwlated based on the ts_clk freq as13.5MHz. This clk frequency is achieved by programming LW_PGC6_SCI_FS_OVERT_TS_SM_TS_CLK_DIV to 0x2 At this frequency, ts_clk requires 155us for completing 2048 clk cycles. This value of 155us is rounded to 200us.

    data=1;
    Platform::EscapeRead("overt", 0, 1, &data);
    if (data != 0) {
        InfoPrintf("1 : overt pad should be 0, but now it's 0x%x \n", data);
        return (1);
    }
    Platform::EscapeRead("overt_detect", 0, 1, &data);
    if (data != 0) {
        InfoPrintf("2 : overt_detect pad should be 0, but now it's 0x%x \n", data);
        return (1);
    }

    //assert pex reset
    Platform::EscapeWrite("pex_reset_",0,1,0);
    Platform::Delay(50);

    //deassert pex reset
    Platform::EscapeWrite("pex_reset_",0,1,1);
    Platform::Delay(5);
    //pGpu->ShutDown(Gpu::ShutDownMode::Normal);

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

    //Trigger another OVERT condition

    SCI_initial_programming_xbash();

    //set LW_PGC6_SCI_FS_OVERT_TS_ADC_RAW_CODE
    //TEMPERATURE = (FINAL_A * RAW_CODE / 16384) - (FINAL_B / 2)
    InfoPrintf("Set temp to 126C through escape write  \n");
    Platform::EscapeWrite("tsense_mini_temp",0,14,temp126_rawcode);
    Platform::Delay(200);//This is callwlated based on the ts_clk freq as13.5MHz. This clk frequency is achieved by programming LW_PGC6_SCI_FS_OVERT_TS_SM_TS_CLK_DIV to 0x2 At this frequency, ts_clk requires 155us for completing 2048 clk cycles. This value of 155us is rounded to 200us.
    Platform::EscapeRead("tsense_mini_temp",0,14,&m_programmed_temp);
    InfoPrintf("programmed LW_PGC6_SCI_FS_OVERT_TS_ADC_RAW_CODE = 0x%x.\n",m_programmed_temp);

    data = 0;
    Platform::EscapeRead("failsafe_overt", 0, 1, &data);
    if (data != 1) {
        ErrPrintf("2 : failsafe should be 1, but now it's 0x%x \n", data);
        return (1);
    }
    Platform::EscapeRead("overt", 0, 1, &data);
    if (data != 0) {
        ErrPrintf("2 : overt pad should be 0, but now it's 0x%x \n", data);
        return (1);
    }

    InfoPrintf("Set temp to 94C through escape write  \n");
    Platform::EscapeWrite("tsense_mini_temp",0,14,temp94_rawcode);
    Platform::Delay(200);//This is callwlated based on the ts_clk freq as13.5MHz. This clk frequency is achieved by programming LW_PGC6_SCI_FS_OVERT_TS_SM_TS_CLK_DIV to 0x2 At this frequency, ts_clk requires 155us for completing 2048 clk cycles. This value of 155us is rounded to 200us.

    data=1;
    Platform::EscapeRead("overt", 0, 1, &data);
    if (data != 0) {
        InfoPrintf("1 : overt pad should be 0, but now it's 0x%x \n", data);
        return (1);
    }
    Platform::EscapeRead("overt_detect", 0, 1, &data);
    if (data != 0) {
        InfoPrintf("2 : overt_detect pad should be 0, but now it's 0x%x \n", data);
        return (1);
    }

    //assert pex reset
    Platform::EscapeWrite("pex_reset_",0,1,0);
    Platform::Delay(50);

    //deassert pex reset
    Platform::EscapeWrite("pex_reset_",0,1,1);
    Platform::Delay(5);
    //pGpu->ShutDown(Gpu::ShutDownMode::Normal);

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
XbashOnOvert::gpu_initialize()
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

    void
XbashOnOvert::enable_gr_pfifo_engine()
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
XbashOnOvert::SetupGpuPointers(void)
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
