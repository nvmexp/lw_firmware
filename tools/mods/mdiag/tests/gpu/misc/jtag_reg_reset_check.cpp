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
//
// TODO: explore the possibility of moving reset tests to HW tree. we cannot access reset value of jtag register from sw tree
//
// Test : Verify that test logic is reset upon Fundamental reset, and not reset upon just hot reset
//  - see bug 1236179 for more details
//
// This test runs in 2 different modes
//
// Fundamental reset mode: A fundamental reset should reset the test logic
// Steps:
//    1. Configure reset registerts to ENABLE a fundamental reset upon hot reset
//    2. Write to a JTAG register with some data
//    3. Read back JTAG register to verify write
//    4. Trigger fundamental reset ( through hot reset )
//    5. Read JTAG register written to earlier to verify the register was reset
//
// Hot reset (without fundamental reset) mode: A hot reset without fundamental reset should *NOT* reset test logic
// Steps:
//    1. Configure reset registerts to DISABLE a fundamental reset upon hot reset
//    2. Write to a JTAG register with some data
//    3. Read back JTAG register to verify write
//    4. Trigger Hot reset
//    5. Read JTAG register written to earlier to verify the register still retains written data
//
/*****************************************************/
#include "mdiag/tests/stdtest.h"

#include "jtag_reg_reset_check.h"

#include "core/include/cmdline.h"
#include "core/include/jscript.h"
#include "device/interface/pcie.h"
#include "gpu/include/gpusbdev.h"
#include "hopper/gh100/dev_xtl_ep_pcfg_gpu.h"
#include "hopper/gh100/dev_xtl_ep_pri.h"
#include "mdiag/lwgpu.h"
#include "mdiag/tests/test_state_report.h"
#include "mdiag/utils/crc.h"
#include "mdiag/utils/cregctrl.h"
#include "mdiag/utils/lwgpu_channel.h"
#include "lwmisc.h"

extern const ParamDecl jtag_reg_reset_check_params[] = {
    SIMPLE_PARAM("-test_fundamental_rst", "Test whether the fundamental reset does reset the test logic"),
    SIMPLE_PARAM("-test_no_fundamental_rst", "Test Hot reset without fundamental reset does NOT reset the test logic"),
    SIMPLE_PARAM("-reg_fundamental_rst", "Test Hot reset with fundamental reset config registers"),

    PARAM_SUBDECL(lwgpu_single_params),
    LAST_PARAM
};

// Constructor - init and get command line parameters
JtagRegResetCheck::JtagRegResetCheck(ArgReader *m_params) : LWGpuSingleChannelTest(m_params)
{
    m_param_test_fundamental_rst = false;
    m_param_test_no_fundamental_rst = false;
    m_param_test_fundamental_reset = false;
    m_bar0_data = 0;
    m_irqSet_data = 0;
    m_vendid_data = 0;
    m_barreg1_data = 0;
    m_barreg2_data = 0;
    m_barreg3_data = 0;
    m_barreg4_data = 0;
    m_barreg5_data = 0;
    m_bar0_addr = 0;
    m_irqSet_addr = 0;
    m_vendid_addr = 0;
    m_bar1lo_addr = 0;
    m_bar1hi_addr = 0;
    m_bar2lo_addr = 0;
    m_bar2hi_addr = 0;
    m_bar3_addr = 0;
    m_pci_domain_num = 0;
    m_pci_bus_num  = 0;
    m_pci_dev_num  = 0;
    m_pci_func_num = 0;

    if (m_params->ParamPresent("-test_fundamental_rst"))
        m_param_test_fundamental_rst = true;
    if (m_params->ParamPresent("-test_no_fundamental_rst"))
        m_param_test_no_fundamental_rst = true;
    if (m_params->ParamPresent("-reg_fundamental_rst"))
        m_param_test_fundamental_reset = true;
    if (m_params->ParamPresent("-project_ga100"))
        m_clk_mask_reg_length = CLK_MASK_REG_LENTH_GA100;
    else
        m_clk_mask_reg_length = CLK_MASK_REG_LENTH_GH100;

    //Refer bug 200568151 Comment #13. 
    lwgpu = LWGpuResource::FindFirstResource();
    UINT32 arch = lwgpu->GetArchitecture();
    
    if (arch == Gpu::GA100)
    {
        InfoPrintf("Current architecture for which the test run is GA100\n");
        m_clk_mask_reg_length = CLK_MASK_REG_LENTH_GA100;
    }
    #if LWCFG(GLOBAL_GPU_IMPL_GA102)
    else if (arch == Gpu::GA102)
    {
        InfoPrintf("Current architecture for which the test run is GA102\n");
        m_clk_mask_reg_length = CLK_MASK_REG_LENTH_GA102;
    }
    #endif
    #if LWCFG(GLOBAL_GPU_IMPL_GA103)
    else if (arch == Gpu::GA103)
    {
        InfoPrintf("Current architecture for which the test run is GA103\n");
        m_clk_mask_reg_length = CLK_MASK_REG_LENTH_GA103;
    }
    #endif
    #if LWCFG(GLOBAL_GPU_IMPL_GA104)
    else if (arch == Gpu::GA104)
    {
        InfoPrintf("Current architecture for which the test run is GA104\n");
        m_clk_mask_reg_length = CLK_MASK_REG_LENTH_GA104;
    }
    #endif
    #if LWCFG(GLOBAL_GPU_IMPL_GA106)
    else if (arch == Gpu::GA106)
    {
        InfoPrintf("Current architecture for which the test run is GA106\n");
        m_clk_mask_reg_length = CLK_MASK_REG_LENTH_GA106;
    }
    #endif
    #if LWCFG(GLOBAL_GPU_IMPL_GA107)
    else if (arch == Gpu::GA107)
    {
        InfoPrintf("Current architecture for which the test run is GA107\n");
        m_clk_mask_reg_length = CLK_MASK_REG_LENTH_GA107;
    }
    #endif
    #if LWCFG(GLOBAL_GPU_IMPL_GH100)
    else if (arch == Gpu::GH100)
    {
        InfoPrintf("Current architecture for which the test run is GH100\n");
        m_clk_mask_reg_length = CLK_MASK_REG_LENTH_GH100;
    }
    #endif
}

// Destructor
JtagRegResetCheck::~JtagRegResetCheck(void)
{
}

// Create the boilerplate factory design pattern function for a test
// std_test_factoy(testname,classname)- test.h file has the define
STD_TEST_FACTORY(jtag_reg_reset_check, JtagRegResetCheck)

// Use SetupGpuPointers function to fetch the lwgpu pointer.
int JtagRegResetCheck::Setup(void)
{

  int returlwal = SetupGpuPointers();

  getStateReport()->init("jtag_reg_reset_check");
  getStateReport()->enable();

  return returlwal;
}

// After reset/reinitialize the gpu we need to fetch the lwgpu pointer again
// Below function will help to fetch the pointers again.
int JtagRegResetCheck::SetupGpuPointers(void)
{

  lwgpu = LWGpuResource::FindFirstResource();

  m_regMap = lwgpu->GetRegisterMap();
  if (!m_regMap)
  {
      ErrPrintf("JtagRegResetCheck::Setup: can't get a register map from the GPU!\n");
      return 0;
  }

  return 1;

}

void JtagRegResetCheck::CleanUp(void)
{
    DebugPrintf("JtagRegResetCheck::CleanUp(): Releasing GPU.\n");
    LWGpuSingleChannelTest::CleanUp();
}

void JtagRegResetCheck::Run(void)
{
    SetStatus(TEST_INCOMPLETE);
    bool fail = false;

    GpuSubdevice * pSubdev;
    pSubdev = lwgpu->GetGpuSubdevice();

    // Save PCI parameters
    auto pGpuPcie = pSubdev->GetInterface<Pcie>();
    m_pci_domain_num = pGpuPcie->DomainNumber();
    m_pci_bus_num    = pGpuPcie->BusNumber();
    m_pci_dev_num    = pGpuPcie->DeviceNumber();
    m_pci_func_num   = pGpuPcie->FunctionNumber();

    if (m_param_test_fundamental_rst == true)
    {
        InfoPrintf("Test fundamental reset DOES reset test logic\n");
        fail = testFundamentalRst();
    }

    if (m_param_test_no_fundamental_rst == true)
    {
        InfoPrintf("Test Hot reset without fundamental reset DOES NOT reset test logic\n");
        fail = fail || testNoFundamentalRst();
    }
    if (m_param_test_fundamental_reset == true)
    {
        InfoPrintf("Test fundamental reset DOES reset config space logic\n");
        fail = fundamentalReset();
    }

    // enable pfifo before test exit - see function for more details
    //Ampere:This is no longer needed as LW_PMC_ENABLE_PFIFO is being removed.See bug 2345711
    //fail = fail || enable_gr_pfifo_engine();

    if (fail)
    {
        SetStatus(TEST_FAILED);
        getStateReport()->runFailed("Error: Test Failed!\n");
        return;
    }

    SetStatus(TEST_SUCCEEDED);
    getStateReport()->crcCheckPassedGold();
}

bool JtagRegResetCheck::testNoFundamentalRst()
{
    // This variable fail is used to capture temporary return values.
    bool fail = true;

    InfoPrintf("Test No fundamental reset case - beginning \n");

    GpuSubdevice * pSubdev = lwgpu->GetGpuSubdevice();
    
    vector<GpuSubdevice::JtagCluster> jc;
    jc.push_back(GpuSubdevice::JtagCluster(CHIPLET_GMB_S0, CLK_MASK_INST_ID));
    UINT32 capDis = 0;
    UINT32 updDis = 0;

    UINT32 dataLength = (CLK_MASK_REG_LENTH_GA100 + 31)/32;
    vector<UINT32> data(dataLength, 0);
    data[0] = JTAG_WRITE_DATA_DWORD_0;

    // STEP 1 : disable fundamental reset because we want to do hot reset, its enabled by default
    InfoPrintf("Test No fundamental reset case - Disabling fundamental reset upon Hot reset \n");
    fail = disable_fundamental_reset_on_hot_rst();
    if (fail)
    {
        ErrPrintf("Error:testNoFundamentalRst: Write to disable fundamental_reset_on_hot_rst Failed!\n");
        return fail;
    }
    save_config_space();
    
    // Added for setting ACCESS_SEC_MODE to 0x0 for turing. This is
    // the requirement to access all non-ISM registers in turing.
    lwgpu->SetRegFldNum("LW_PJTAG_ACCESS_SEC", "_SWITCH_TO_STANDARD", 1); // Configure ACCESS_SEC to SWITCH_TO_STANDARD since ISM_MODE is default.

    Utility::DelayNS(100); // Wait for 10k cycles before reading the ACCESS_SEC_MODE status.
    UINT32 rd_data;
    lwgpu->GetRegFldNum("LW_PJTAG_ACCESS_SEC", "_MODE", &rd_data);
    if (rd_data != 0) {
        ErrPrintf("LW_PJTAG_ACCESS_SEC_MODE != 0x0 ... read as %d \n", rd_data);
        return fail;
    }
    // Logic to read and confirm ACCESS_SEC_MODE to 0x0 ends here.

    // Write Jtag Timeout Register
    lwgpu->SetRegFldDef("LW_XAL_EP_JTAG_TIMEOUT", "_PERIOD", "_MAX");
    lwgpu->SetRegFldDef("LW_XAL_EP_PCIE_RSP_PRI_TIMEOUT", "_PERIOD", "_MAX");
    
    // STPE 2 : Write magic data into JTAG register
    
    InfoPrintf("Test No fundamental reset case - Writing to JTAG register \n");
    
    InfoPrintf("Start WriteHost2Jtag\n");
    pSubdev->WriteHost2Jtag(jc, m_clk_mask_reg_length, data, capDis, updDis);

    // STEP 3 : Read JTAG register back to confirm write
    //
    InfoPrintf("Start ReadHost2Jtag\n");
    pSubdev->ReadHost2Jtag(jc, m_clk_mask_reg_length, &data, capDis, updDis);
    
    if(data[0] != JTAG_WRITE_DATA_DWORD_0)
    {           
        ErrPrintf("Error : Write to Jtag register Failed!\n");
        return fail;
    }
    
    // STEP 4 : Apply hot reset ;
    // NOTE: ASSERT_FUNDAMENTAL_RESET_ON_HOT_RESET is set to 0, hot_reset won't trigger fundamental reset
    InfoPrintf("Test No fundamental reset case - applying Hot reset (without fundamental reset) \n");
    hot_reset();
    
    // restore the config space registers here .
    fail = restore_config_space();
    if (fail)
    {
        ErrPrintf("Error:testNoFundamentalRst: Restoring Config space access Failed!\n");
        return fail;
    }

    // Initialize GPU after Hot reset
    gpu_initialize();

    // STEP 5 : Verify JTAG register after reset - expect register to retain write data
    InfoPrintf("Test No fundamental reset case - read JTAG register after reset\n");
    
    pSubdev->ReadHost2Jtag(jc, m_clk_mask_reg_length, &data, capDis, updDis);
    
    if(data[0] != JTAG_WRITE_DATA_DWORD_0)
    {           
        ErrPrintf("Test No fundamental reset case - JTAG register check after reset Failed!\n");
        return fail;
    }

    InfoPrintf("Test No fundamental reset case - Test PASSED! \n");
    return false;
}

bool JtagRegResetCheck::testFundamentalRst()
{
    // This is used to capture temporary return values
    bool fail = true;

    // calling jtag reg access API through GpuSubdevice
    GpuSubdevice * pSubdev = lwgpu->GetGpuSubdevice();
    
    vector<GpuSubdevice::JtagCluster> jc;
    jc.push_back(GpuSubdevice::JtagCluster(CHIPLET_GMB_S0, CLK_MASK_INST_ID));
    UINT32 capDis = 0;
    UINT32 updDis = 0;

    UINT32 dataLength = (CLK_MASK_REG_LENTH_GA100 + 31)/32;
    vector<UINT32> data(dataLength, 0);
    data[0] = JTAG_WRITE_DATA_DWORD_0;

    // Save off some config space
    // Fundamental reset would reset the entire GPU , So we need to restore the config space reg after reset.
    // else test would fail for these errors SBIOS assigned incorrect BAR0 - offset 0x0, size 0x1000000
    // Info: LWGpuResource::ScanSystem: there are no gpu's to scan...
    save_config_space();

    InfoPrintf("Test fundamental reset case - beginning \n");

    // STEP 1 : enable fundamental reset upon Hot reset
    InfoPrintf("Test fundamental reset case - Enable fundamental reset upon Hot reset \n");
    fail = enable_fundamental_reset_on_hot_rst();
    if (fail)
    {
        ErrPrintf("Error:testFundamentalRst: Write to enable fundamental_reset_on_hot_rst Failed!\n");
        return fail;
    }

    // Added for setting ACCESS_SEC_MODE to 0x0 for turing. This is
    // the requirement to access all non-ISM registers in turing.
    lwgpu->SetRegFldNum("LW_PJTAG_ACCESS_SEC", "_SWITCH_TO_STANDARD", 1); // Configure ACCESS_SEC to SWITCH_TO_STANDARD since ISM_MODE is default.

    Utility::DelayNS(100); // Wait for 10k cycles before reading the ACCESS_SEC_MODE status.
    UINT32 rd_data;
    lwgpu->GetRegFldNum("LW_PJTAG_ACCESS_SEC", "_MODE", &rd_data);
    if (rd_data != 0) {
        ErrPrintf("LW_PJTAG_ACCESS_SEC_MODE != 0x0 ... read as %d \n", rd_data);
        return fail;
    }
    
    // Write Jtag Timeout Register
    lwgpu->SetRegFldDef("LW_XAL_EP_JTAG_TIMEOUT", "_PERIOD", "_MAX");
    lwgpu->SetRegFldDef("LW_XAL_EP_PCIE_RSP_PRI_TIMEOUT", "_PERIOD", "_MAX");

    // STEP 2 : Write magic data into JTAG register
     
    InfoPrintf("Test No fundamental reset case - Writing to JTAG register \n");
    pSubdev->WriteHost2Jtag(jc, m_clk_mask_reg_length, data, capDis, updDis);


    // STEP 3 : Read JTAG register back to confirm write
    InfoPrintf("Start ReadHost2Jtag\n");
    pSubdev->ReadHost2Jtag(jc, m_clk_mask_reg_length, &data, capDis, updDis);
    
    if(data[0] != JTAG_WRITE_DATA_DWORD_0)
    {           
        ErrPrintf("Error : Write to Jtag register Failed!\n");
        return fail;
    }

    // STEP 4 : Apply hot reset ;
    // NOTE: ASSERT_FUNDAMENTAL_RESET_ON_HOT_RESET is set to 1, so fundamental reset is asserted
    InfoPrintf("Test fundamental reset case - applying Hot reset (without fundamental reset) \n");
    hot_reset();

    // restore the config space registers here .
    fail = restore_config_space();
    if (fail)
    {
        ErrPrintf("Error:testFundamentalRst: Restoring Config space access Failed!\n");
        return fail;
    }

    // Initialize GPU after Hot reset
    gpu_initialize();

    // STEP 5 : Verify JTAG register after reset - expect register to retain write data
    InfoPrintf("Test fundamental reset case - read JTAG register to verify register was reset \n");
    pSubdev->ReadHost2Jtag(jc, m_clk_mask_reg_length, &data, capDis, updDis);
    
    if(data[0] != JTAG_WRITE_DATA_DWORD_0)
    {           
        ErrPrintf("Test fundamental reset case - JTAG register check after reset Failed!\n");
        return fail;
    }

    InfoPrintf("Test fundamental reset case - Test PASSED! \n");
    return false;
}
bool JtagRegResetCheck::fundamentalReset()
{
    // This is used to capture temporary return values
    bool fail = true;

    // Save off some config space
    // Fundamental reset would reset the entire GPU , So we need to restore the config space reg after reset.
    // else test would fail for these errors SBIOS assigned incorrect BAR0 - offset 0x0, size 0x1000000
    // Info: LWGpuResource::ScanSystem: there are no gpu's to scan...
    save_config_space();

    InfoPrintf("Test fundamental reset case - beginning \n");

    // STEP 1 : enable fundamental reset upon Hot reset
    InfoPrintf("Test fundamental reset case - Enable fundamental reset upon Hot reset \n");
    fail = enable_fundamental_reset_on_hot_rst();
    if (fail)
    {
        ErrPrintf("Error:fundamentalReset: Write to enable fundamental_reset_on_hot_rst Failed!\n");
        return fail;
    }
    UINT32 mods_scratch_init;
    int mods_scratch_address = LW_XTL_EP_PRI_MODS_SCRATCH ;

    //Get LW_XTL_EP_PRI_MODS_SCRATCH init value
    Platform::PciRead32(m_pci_domain_num,
                       m_pci_bus_num,
                       m_pci_dev_num,
                       m_pci_func_num,
                       mods_scratch_address,&mods_scratch_init);
    
    //Write 0xffffff to LW_XTL_EP_PRI_MODS_SCRATCH
    if (pci_write(mods_scratch_address,0xffffff))
    {
        ErrPrintf("Test fundamental reset case - register write failed before reset\n");
        return fail;
    }
    
    Platform::Delay(100);

    // STEP 4 : Apply hot reset ;
    // NOTE: ASSERT_FUNDAMENTAL_RESET_ON_HOT_RESET is set to 1, so fundamental reset is asserted
    InfoPrintf("Test fundamental reset case - applying Hot reset (without fundamental reset) \n");
    hot_reset();

    // restore the config space registers here .
    fail = restore_config_space();
    if (fail)
    {
        ErrPrintf("Error:fundamentalReset: Restoring Config space access Failed!\n");
        return fail;
    }

    // Initialize GPU after Hot reset
    gpu_initialize();

    UINT32 rdval;
    Platform::PciRead32(m_pci_domain_num,
                       m_pci_bus_num,
                       m_pci_dev_num,
                       m_pci_func_num,
                       mods_scratch_address, &rdval);
    if (rdval != mods_scratch_init)
    {
        ErrPrintf("FundRegResetCheck: LW_XTL_EP_PRI_MODS_SCRATCH Expected=0x%x,actual=0x%x \n",mods_scratch_init,rdval);
        fail = true;
    }

    if (fail)
    {
        InfoPrintf("Test fundamental reset case - register check after reset Failed! \n");
        return fail;
    }

    InfoPrintf("Test fundamental reset case - Test PASSED! \n");
    return false;
}

// By default hot reset hits fundamental reset , in this method we set the ASSERT_FUNDAMENTAL_RESET_ON_HOT_RESET to
// DISABLED to prevent that .
bool JtagRegResetCheck::disable_fundamental_reset_on_hot_rst (void)
{
    InfoPrintf("Disabling FUNDAMENTAL_RESET_ON_HOT_RESET \n");
    bool fail = false ;
    UINT32 reg_rd_val = 0;
    UINT32 golden_val = 0;

    // Reg: LW_PCFG_XTL_EP_PRI_RESET
    // Field : ASSERT_FUNDAMENTAL_RESET_ON_HOT_RESET - if set, a fundamental reset is asserted upon hot reset - disable it

    if (lwgpu->SetRegFldDef("LW_XTL_EP_PRI_RESET", "_ASSERT_FUNDAMENTAL_RESET_ON_HOT_RESET", "_NORESET"))
    {
        ErrPrintf("JtagRegResetCheck: disable_fundamental_reset_on_hot_rst, Could not be updated LW_XTL_EP_PRI_RESET_ASSERT_FUNDAMENTAL_RESET_ON_HOT_RESET to value _NORESET\n");
        fail = true;
    }

    // Verify the above write
    lwgpu->GetRegFldNum("LW_XTL_EP_PRI_RESET", "_ASSERT_FUNDAMENTAL_RESET_ON_HOT_RESET", &reg_rd_val);
    lwgpu->GetRegFldDef("LW_XTL_EP_PRI_RESET", "_ASSERT_FUNDAMENTAL_RESET_ON_HOT_RESET", "_NORESET", &golden_val);
    if (reg_rd_val != golden_val)
    {
        ErrPrintf("JtagRegResetCheck: disable_fundamental_reset_on_hot_rst, Verify write failed.. reg_rd_val=0x%x,golden_val=0x%x \n",reg_rd_val,golden_val);
        fail = true;
    }

    return fail;
}

bool JtagRegResetCheck::enable_fundamental_reset_on_hot_rst (void)
{
    InfoPrintf("Enabling FUNDAMENTAL_RESET_ON_HOT_RESET \n");
    bool fail = false ;
    // Reg: LW_PCFG_XVE_RESET_CRTL
    // Field : ASSERT_FUNDAMENTAL_RESET_ON_HOT_RESET - if set, a fundamental reset is asserted upon hot reset - ENABLE it
    if (lwgpu->SetRegFldDef("LW_XTL_EP_PRI_RESET", "_ASSERT_FUNDAMENTAL_RESET_ON_HOT_RESET", "_RESET"))
    {
        ErrPrintf("JtagRegResetCheck: enable_fundamental_reset_on_hot_rst, Could not be updated LW_XTL_EP_PRI_RESET_ASSERT_FUNDAMENTAL_RESET_ON_HOT_RESET to value _ENABLED\n");
        fail = true;
    }
    return fail;
}

// Disable XVE CFG space reset on hot reset - so we don't have to restore CFG space
// after reset.  If this field "_CYA_CFG_HOT_RESET" is set then hot reset will reset
// the xve CFG space, else it won't.  Here we disable this, so the CFG space
// register values will be retained after hot reset gets deasserted.  If the CFG
// space gets cleared during the hot reset time then we need to restore the CFG
// register values before we do any more access.
bool JtagRegResetCheck::disable_xve_cfg_reset_on_hot_rst (void)
{
    bool fail = false ;
    UINT32 reg_rd_val = 0;
    UINT32 golden_val = 0;
    
    // Bug 2800397: LW_XVE_PRIV_XV_0_CYA_CFG_HOT_RESET moved to LW_XVE_PRIV_XV_0_CYA_RESET_CFG
    // Reg: LW_PCFG_XVE_PRIV_XV_0_CYA_RESET_CFG
    // Field : _HOT_RESET - disable it
    // Field : _DL_UP - disable it
    if (lwgpu->SetRegFldDef("LW_PCFG_XVE_PRIV_XV_0_CYA_RESET_CFG", "_HOT_RESET","_NORESET"))
    {
        ErrPrintf("JtagRegResetCheck: disable_xve_cfg_reset_on_hot_rst, Could not be updated LW_PCFG_XVE_PRIV_XV_0_CYA_RESET_CFG_HOT_RESET to value _NORESET\n");
        fail = true;
    }
    if (lwgpu->SetRegFldDef("LW_PCFG_XVE_PRIV_XV_0_CYA_RESET_CFG", "_DL_UP","_NORESET"))
    {
        ErrPrintf("JtagRegResetCheck: disable_xve_cfg_reset_on_hot_rst, Could not be updated LW_PCFG_XVE_PRIV_XV_0_CYA_RESET_CFG_DL_UP to value _NORESET\n");
        fail = true;
    }

    // Verify the above write
    lwgpu->GetRegFldNum("LW_PCFG_XVE_PRIV_XV_0_CYA_RESET_CFG", "_HOT_RESET", &reg_rd_val);
    lwgpu->GetRegFldDef("LW_PCFG_XVE_PRIV_XV_0_CYA_RESET_CFG", "_HOT_RESET","_NORESET", &golden_val);
    if (reg_rd_val != golden_val)
    {
        ErrPrintf("JtagRegResetCheck: disable_xve_cfg_reset_on_hot_rst, Verify write failed.. reg_rd_val=0x%x,golden_val=0x%x \n",reg_rd_val,golden_val);
        fail = true;
    }
    lwgpu->GetRegFldNum("LW_PCFG_XVE_PRIV_XV_0_CYA_RESET_CFG", "_DL_UP", &reg_rd_val);
    lwgpu->GetRegFldDef("LW_PCFG_XVE_PRIV_XV_0_CYA_RESET_CFG", "_DL_UP","_NORESET", &golden_val);
    if (reg_rd_val != golden_val)
    {
        ErrPrintf("JtagRegResetCheck: disable_xve_cfg_reset_on_hot_rst, Verify write (_CYA_CFG_DL_UP) failed.. reg_rd_val=0x%x,golden_val=0x%x \n",reg_rd_val,golden_val);
        fail = true;
    }
    return fail;

}

// Save the config space registers before the fundamental reset . Fundamental reset would reset the entire GPU
// So we need to restore these registers later.
void JtagRegResetCheck::save_config_space()
{
    RC rc;
    int address = 0;
    address = LW_EP_PCFG_GPU_BARREG0;
    Platform::PciRead32(m_pci_domain_num,
                       m_pci_bus_num,
                       m_pci_dev_num,
                       m_pci_func_num,
                       address,&m_bar0_data);

    address = LW_EP_PCFG_GPU_MISC;
    Platform::PciRead32(m_pci_domain_num,
                       m_pci_bus_num,
                       m_pci_dev_num,
                       m_pci_func_num,
                       address,&m_irqSet_data);

    address = LW_EP_PCFG_GPU_SUBSYSTEM_ID;
    Platform::PciRead32(m_pci_domain_num,
                       m_pci_bus_num,
                       m_pci_dev_num,
                       m_pci_func_num,
                       address,&m_vendid_data);

    address = LW_EP_PCFG_GPU_BARREG1;
    Platform::PciRead32(m_pci_domain_num,
                       m_pci_bus_num,
                       m_pci_dev_num,
                       m_pci_func_num,
                       address,&m_barreg1_data);

    address = LW_EP_PCFG_GPU_BARREG2;
    Platform::PciRead32(m_pci_domain_num,
                       m_pci_bus_num,
                       m_pci_dev_num,
                       m_pci_func_num,
                       address,&m_barreg2_data);

    address = LW_EP_PCFG_GPU_BARREG3;
    Platform::PciRead32(m_pci_domain_num,
                       m_pci_bus_num,
                       m_pci_dev_num,
                       m_pci_func_num,
                       address,&m_barreg3_data);

    address = LW_EP_PCFG_GPU_BARREG4;
    Platform::PciRead32(m_pci_domain_num,
                       m_pci_bus_num,
                       m_pci_dev_num,
                       m_pci_func_num,
                       address,&m_barreg4_data);

    address = LW_EP_PCFG_GPU_BARREG5;
    Platform::PciRead32(m_pci_domain_num,
                       m_pci_bus_num,
                       m_pci_dev_num,
                       m_pci_func_num,
                       address,&m_barreg5_data);
}

// Restore the saved config space registers after reset
bool JtagRegResetCheck::restore_config_space()
{
    RC rc;
    int address = 0;

    address = LW_EP_PCFG_GPU_BARREG0;
    if (pci_write(address,m_bar0_data))
    {
        return true;
    }
    address = LW_EP_PCFG_GPU_MISC;
    if (pci_write(address,m_irqSet_data))
    {
        return true;
    }
    address = LW_EP_PCFG_GPU_SUBSYSTEM_ID;
    if (pci_write(address,m_vendid_data))
    {
        return true;
    }
    address = LW_EP_PCFG_GPU_BARREG1;
    if (pci_write(address,m_barreg1_data))
    {
        return true;
    }
    address = LW_EP_PCFG_GPU_BARREG2;
    if (pci_write(address,m_barreg2_data))
    {
        return true;
    }
    address = LW_EP_PCFG_GPU_BARREG3;
    if (pci_write(address,m_barreg3_data))
    {
        return true;
    }
    address = LW_EP_PCFG_GPU_BARREG4;
    if (pci_write(address,m_barreg4_data))
    {
        return true;
    }
    address = LW_EP_PCFG_GPU_BARREG5;
    if (pci_write(address,m_barreg5_data))
    {
        return true;
    }
    return false;
}

// Function for pci write and make sure write happened correctly by a read check
bool JtagRegResetCheck::pci_write(UINT32 offset , UINT32 data)
{
    UINT32 reg_val = 0x0;
    bool fail = false;

    Platform::PciWrite32(m_pci_domain_num,m_pci_bus_num,m_pci_dev_num,m_pci_func_num,offset,data);
    InfoPrintf("Restore the config space m_pci_domain_num = 0x%x , m_pci_bus_num = 0x%x , address =: 0x%x    data = 0x%x \n",m_pci_domain_num,m_pci_bus_num,offset,data);

    // verify the write
    Platform::PciRead32(m_pci_domain_num,m_pci_bus_num,m_pci_dev_num,m_pci_func_num,offset,&reg_val);
    if (reg_val != data)
    {
        ErrPrintf("restore_config_space: Reg address 0x%x  - Read back didn't match write data\n",offset);
        fail = true;
    }
    return fail;
}

// shariq -- common code copied from SoCD tests in HW tree, ideally would want to have this code at a common place to be used by multiple tests
// This function writes to Jtag regisister, first write the control word then write data
// Write the ctrl register with appropriate entries for the different fields
// Read back the control register and check the status bit (bit 30)
// If the status bit is set, start write transactions on the data register starting from the lowest 32 bits. Therefore, if the
// jtag register is 65 bits wide, do a first write of B31:B0, a second write of B63:B32 and then a third write of B64. The top 31 data
// bits in the third write will be dont-cares
// write the data  49'ha000_00000005 to the register JTAG2TMC_BP_CLK_MASK in the test logic - host2jtag interface access
// clk mask jtag register specifications can be found from the below jrd file,
// package/gm204/jtagreg/gmb_s0/JTAG2TMC_BP_CLK_MASK.jrd
// JTAG_WRITE_DATA_DWORD_0 is first 31bit data and JTAG_WRITE_DATA_DWORD_1 is 2nd 31bit data

// DWORD_EN: This field determines which double word of the chain is lwrrently read; this field is ignored for write operations.
// For every access to a chain, all double words of the chain must be accessed in a sequential order. For example, for a read to
// a 35-bit chain, DWORD_EN is first set to b000000 and then b000001 to access the first 32 bits and then the last 3 bits of the
// chain subsequently.
// [Simon Fan]From the logic, the load of dword_en are all RD relate state in host2jtag_interface_distributed.
// My understand is the write in value are in jtag chain neednt store. But for RD, there is STORE state, need know current read value is
// in which position of the jtag chain.

bool JtagRegResetCheck::test_logic_reg_write(UINT32 dword0, UINT32 dword1)
{
    UINT32 status;

    bool fail = false;

    InfoPrintf("test_logic_reg_write - writing to jtag register jtag2tmc_bp_clk_mask ... \n");
    status = 0;

    // Setting captureDR/updateDR to 1. So that the write data won't affect the other functional logic
    host2jtag_config_write(CONFIG_WRITE_DATA);

    // Issue new request on JTAG control reg - req type WRITE
    fail = host2jtag_ctrl_write (REQ_NEW, status, CHIPLET_GMB_S0, DWORD_ZERO, m_clk_mask_reg_length, CLK_MASK_INST_ID, WRITE_INTENT);
    if (fail)
    {
        ErrPrintf("test_logic_reg_write : Write to Jtag contorl reg failed \n");
        return fail;
    }

    // Write to the JTAG Data register
    // TODO: find out how back to back writes to JTAG data register without any control in between works
    host2jtag_data_write (dword0);
    host2jtag_data_write (dword1);

    // This write to the control register is to clear request on jtag control ; Here we explicitly clear the status bit with a
    // write transaction with REQ_CLR .We will do a new control register write for the next request/transaction .
    fail = host2jtag_ctrl_write (REQ_CLR, status, CHIPLET_GMB_S0, DWORD_ZERO, m_clk_mask_reg_length, CLK_MASK_INST_ID, WRITE_INTENT);
    if (fail)
    {
        ErrPrintf("test_logic_reg_write : Write to Jtag contorl reg failed \n");
    }

    return fail;
}

// This function helps to read the data from jtag register and compare with the expected one.
// Write the ctrl register with appropriate entries for the different fields
// Read back the control register and check the status bit (bit 30)
// If the status bit is set, start read transactions on the data register.
// At the end of the transaction, the requested double word will be available on the rdat bus.
// If the requested double word has less than 32 valid bits, the results are aligned from the MSB side. Thus, if the jtag
// register is 68 bits wide, and the 3rd double word is being accessed, only [B67:B64] of the register would be returned on  bits
// [B31:B28] of the rdat bus.
// trying to read the register  JTAG2TMC_BP_CLK_MASK via Jtag here
// clk mask jtag register specifications can be found from the below jrd file,
// package/gm204/jtagreg/gmb_s0/JTAG2TMC_BP_CLK_MASK.jrd
// This is a 49 bit register , we will get only 32bti data in a single read, need to do the read twice to get the both dword.
// These exp_dword0 & exp_dword1 are the expected read datas , we use these for comparing the DWORDS which host2jtag_data_read returns.
bool JtagRegResetCheck::test_logic_reg_read_check(UINT32 const exp_dword0, UINT32 const exp_dword1)
{

    UINT32 read_data = 0;
    UINT32 status = 0;
    bool fail = false;

    InfoPrintf("test_logic_reg_read_check - read jtag register jtag2tmc_bp_clk_mask and check against expected data \n");

    // Setting captureDR/updateDR to 1.
    host2jtag_config_write(CONFIG_WRITE_DATA);

    // Read DWORD ZERO for jtag register
    // Issue new request on JTAG control reg - req type READ
    fail = host2jtag_ctrl_write (REQ_NEW, status, CHIPLET_GMB_S0, DWORD_ZERO, m_clk_mask_reg_length, CLK_MASK_INST_ID, READ_INTENT);
    if (fail)
    {
        ErrPrintf("test_logic_reg_read_check : Write to Jtag contorl reg failed \n");
        return fail;
    }

    // read the JTAG data register
    host2jtag_data_read (read_data, m_clk_mask_reg_length, false); // false for not last dword

    // Read back value should be same as "data" , except for "pipeline_tdo_flop"
    // Skipping the pipeline_tdo_flop bit check for dword 0.
    // As per the JTAG2TMC_BP_CLK_MASK.jrd file this bit is at 0th position ;
    // so shifting the read data 1 to right, before comparison
    //if ((read_data  >> 1) != (exp_dword0  >> 1))
    if (read_data   != exp_dword0)
    {
        ErrPrintf("test_logic_reg_read_check : jtag register jtag2tmc_bp_clk_mask: DWORD 0 : read back value is %x, the expected value is %x\n", read_data, exp_dword0);
        return (true);
    }

    // Clear request on jtag control
    fail = host2jtag_ctrl_write (REQ_CLR, status, CHIPLET_GMB_S0, DWORD_ZERO, m_clk_mask_reg_length, CLK_MASK_INST_ID, READ_INTENT);
    if (fail)
    {
        ErrPrintf("test_logic_reg_read_check : Write to Jtag contorl reg failed \n");
        return fail;
    }

    // Setting captureDR/updateDR to 1.
    host2jtag_config_write(CONFIG_WRITE_DATA);

    // Read DWORD ONE for jtag register
    // Issue new request on JTAG control reg - req type READ
    fail = host2jtag_ctrl_write (REQ_NEW, status, CHIPLET_GMB_S0, DWORD_ONE, m_clk_mask_reg_length, CLK_MASK_INST_ID, READ_INTENT);
    if (fail)
    {
        ErrPrintf("test_logic_reg_read_check : Write to Jtag contorl reg failed \n");
        return fail;
    }

    // read the JTAG data register
    host2jtag_data_read (read_data, m_clk_mask_reg_length, true); // true for last dword

    if (read_data != exp_dword1)
    {
        ErrPrintf("test_logic_reg_read_check : jtag register jtag2tmc_bp_clk_mask: DWORD 1 : read back value is %x, the expected value is %x\n", read_data, exp_dword1);
        return (true);
    }

    // Clear request on jtag control
    fail = host2jtag_ctrl_write (REQ_CLR, status, CHIPLET_GMB_S0, DWORD_ONE, m_clk_mask_reg_length, CLK_MASK_INST_ID, READ_INTENT);
    if (fail)
    {
        ErrPrintf("test_logic_reg_read_check : Write to Jtag contorl reg failed \n");
    }

    return fail;
}

// Shut down the gpu and force the hot reset from tb.
void JtagRegResetCheck::hot_reset(void)
{

    GpuPtr pGpu;
    pGpu = GpuPtr();

     // Release all Mdiag resources, which will be reinitalized later by the ScanSystem call.
    // Free the frame buffer, PCI, AGP and dma resources.
    // Free the channel and its objects
    LWGpuResource::FreeResources();

    // XXX Not clear if a shutdown should be done before hotreset ???
    // lwgpu->GetGpuSubdevice()->Shutdown();
    // Shut down the GPU resman and OpenGL (if applicable) connection(s).
    //Free resources has to happen before shutdown. see bug #2439191
    pGpu->ShutDown(Gpu::ShutDownMode::Normal);

    InfoPrintf("hot_reset : asserting HotReset\n");

    // Send in Hot reset
    Platform::EscapeWrite("doHotReset",0,1,1);
    Platform::Delay(80);

    // Exit out of Reset
    // XXX - May not be necessary
    Platform::EscapeWrite("exitHotReset",0,1,1);
    InfoPrintf("hot_reset : Exiting hot reset ...\n");

    return;

}

bool JtagRegResetCheck::gpu_initialize(void)
{
    GpuPtr pGpu = GpuPtr();
    JavaScriptPtr pJs;
    RC     rc;

    InfoPrintf("Initialize GPU ... ");

    // Reset the pointer to NULL since we fetch the lwgpu pointer again after
    // Reset/Reinitialization.
    lwgpu = NULL;

    // Only Reinit what is necessary
    CHECK_RC( pJs->SetProperty(Gpu_Object.GetJSObject(), "SkipDevInit", true) );
    CHECK_RC( pJs->SetProperty(Gpu_Object.GetJSObject(), "SkipConfigSim", true) );


    // Reinitialize Gpu
    pGpu->Initialize();
    //Bug 2812901 we need to re enable interrupts as we are skipping devinit before
    Platform::EnableInterrupts();

    // Gpu::Shutdown frees all Gpu stuff, thus the
    // entire LW*Resource system in Mdiag needs a rescan to reflect such
    // drastic change. Otherwise, most, if not all, of the pointers would
    // be invalid/dangling. LWGpuResource::ScanSystem does this job
    // well.
    LWGpuResource::ScanSystem(CommandLine::ArgDb());

    // Fetch all the valid pointers for this new gpu, since it was reinitialized
    MASSERT(SetupGpuPointers());

    InfoPrintf("Done initializing GPU!\n");
    return 0;
}

// Function wait for host control and timedout when we don't get control
// within the defined 800 attempts
bool JtagRegResetCheck::wait_for_jtag_ctrl(void)
{
    UINT32 hostStatus = CTRL_STATUS_BUSY;
    UINT32 host_ctrl_attempts = 0;

    InfoPrintf("JtagRegResetCheck::wait_for_jtag_ctrl : Waiting for Host Control \n");
    while (hostStatus == CTRL_STATUS_BUSY)
    {
        lwgpu->GetRegFldNum("LW_PJTAG_ACCESS_CTRL", "LW_PJTAG_ACCESS_CTRL_CTRL_STATUS", &hostStatus);
        host_ctrl_attempts++;
        if (host_ctrl_attempts > MAX_HOST_CTRL_ATTEMPTS)
        {
            InfoPrintf("Exceeded MAX_HOST_CTRL_ATTEMPTS, attempts = %d \n", host_ctrl_attempts);
            return true;
        }
    }

    InfoPrintf("JtagRegResetCheck::wait_for_jtag_ctrl : Acquired Host Control\n");
    return false;
}

// function writes the control word to LW_PJTAG_ACCESS_CTRL register .
// CTRL register needs to be set before all write and read.
bool JtagRegResetCheck::host2jtag_ctrl_write(UINT32 req_ctrl, UINT32 status, UINT32 chiplet_sel, UINT32 dword_en, UINT32 reg_length, UINT32 instr_id, bool read_or_write)
{

    UINT32 h2j_reglength = reg_length;

    UINT32 host2jtagRegCtrl = 0;
    //clean up the old request
    lwgpu->SetRegFldNum("LW_PJTAG_ACCESS_CTRL", "", host2jtagRegCtrl);

    lwgpu->SetRegFldNum("LW_PJTAG_ACCESS_CTRL", "_INSTR_ID", instr_id);
    lwgpu->SetRegFldNum("LW_PJTAG_ACCESS_CTRL", "_REG_LENGTH", h2j_reglength);
    lwgpu->SetRegFldNum("LW_PJTAG_ACCESS_CTRL", "_DWORD_EN", dword_en);
    lwgpu->SetRegFldNum("LW_PJTAG_ACCESS_CTRL", "_CLUSTER_SEL", chiplet_sel);
    lwgpu->SetRegFldNum("LW_PJTAG_ACCESS_CTRL", "_CTRL_STATUS", status);
    // for new request
    lwgpu->SetRegFldNum("LW_PJTAG_ACCESS_CTRL", "_REQ_CTRL", req_ctrl);

    lwgpu->GetRegFldNum("LW_PJTAG_ACCESS_CTRL", "", &host2jtagRegCtrl);

    InfoPrintf("JtagRegResetCheck::host2jtag_ctrl_write : Writing Into LW_PJTAG_ACCESS_CTRL : 0x%x\n", host2jtagRegCtrl);
    InfoPrintf("JtagRegResetCheck::host2jtag_ctrl_write : read_or_write : %s, reg_length : 0x%x, Chiplet : 0x%x\n", (read_or_write == WRITE_INTENT ? "write" : "read"), reg_length, chiplet_sel);

    if (req_ctrl == REQ_NEW)
    {
        if (wait_for_jtag_ctrl())
        {
          ErrPrintf("JtagRegResetCheck::host2jtag_ctrl_write:Didn't acquire host control within %d attempts \n",MAX_HOST_CTRL_ATTEMPTS);
          return true;
        }
    }
    return false;
}

// Setting captureDR/updateDR to 1. So that the write data won't affect the other functional logic.
void JtagRegResetCheck::host2jtag_config_write(UINT32 config_write_data)
{
    InfoPrintf("Writing into LW_PJTAG_ACCESS_CONFIG : 0x%x\n", config_write_data);
    lwgpu->SetRegFldNum("LW_PJTAG_ACCESS_CONFIG", "",config_write_data);
}

// Function writes the write data to the LW_PJTAG_ACCESS_DATA register
void JtagRegResetCheck::host2jtag_data_write(UINT32 data_value)
{
    InfoPrintf("Writing into LW_PJTAG_ACCESS_DATA : 0x%x\n", data_value);
    lwgpu->SetRegFldNum("LW_PJTAG_ACCESS_DATA", "", data_value);
}

// Function helps to get the data from the Jtag reg.  Read data is available in
// LW_PJTAG_ACCESS_DATA.  Need to do the shift if the valid data is less than 32bit
// wide bug 1236179 comment #107
// https://wiki.lwpu.com/engwiki/index.php/Hwcad/GenPads/TestBus#Operating_Sequence
// data_value is the read data, reg_length is the bit length of the JTAG register.
// last_dword is set to one when we access the last dword of the jtag register
void JtagRegResetCheck::host2jtag_data_read(UINT32& data_value, UINT32 reg_length, bool last_dword)
{

    InfoPrintf("JtagRegResetCheck::host2jtag_data_read : Reading from LW_PJTAG_ACCESS_DATA\n ");
    //data_value = lwgpu->RegRd32(LW_PJTAG_ACCESS_DATA);
    lwgpu->GetRegFldNum("LW_PJTAG_ACCESS_DATA", "", &data_value);

    InfoPrintf("JtagRegResetCheck::host2jtag_data_read : reg_length = 0x%x , READ_BITS =0x%x, data_value  =0x%x    \n",reg_length,READ_BITS,data_value);
    // If last_dword has less than 32bits, then we need to shift the data (no of valid bits times) to the right.
    if (last_dword)
    {
        if ((reg_length % READ_BITS) != 0)
        {
            data_value = ( data_value >> (READ_BITS - (reg_length % READ_BITS) )) ;
        }
    }
    InfoPrintf("JtagRegResetCheck::host2jtag_data_read : Reg read value is data_value = 0x%x \n",data_value);
}
