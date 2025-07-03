/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2021-2022 by LWPU Corporation.  All rights 
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

#include "ad10x_jtag_reg_reset_check.h"
#include "mdiag/tests/test_state_report.h"
#include "lwmisc.h"

#include "mdiag/lwgpu.h"
#include "mdiag/utils/lwgpu_channel.h"
#include "mdiag/utils/crc.h"
#include "mdiag/tests/test_state_report.h"
#include "core/include/jscript.h"
#include "gpu/include/gpusbdev.h"
#include "device/interface/pcie.h"
#include "mdiag/utils/cregctrl.h"
#include "core/include/cmdline.h"

extern const ParamDecl ad10x_jtag_reg_reset_check_params[] = {
    SIMPLE_PARAM("-test_fundamental_rst", "Test whether the fundamental reset does reset the test logic"),
    SIMPLE_PARAM("-test_no_fundamental_rst", "Test Hot reset without fundamental reset does NOT reset the test logic"),
    SIMPLE_PARAM("-reg_fundamental_rst", "Test Hot reset with fundamental reset config registers"),
    PARAM_SUBDECL(lwgpu_single_params),
    LAST_PARAM
};

// Constructor - init and get command line parameters
Ad10xJtagRegResetCheck::Ad10xJtagRegResetCheck(ArgReader *m_params) : LWGpuSingleChannelTest(m_params)
{
    m_param_test_fundamental_rst = false;
    m_param_test_no_fundamental_rst = false;
    m_param_test_fundamental_reset = false;
    m_bar0_data = 0;
    m_irqSet_data = 0;
    m_vendid_data = 0;
    m_bar1lo_data = 0;
    m_bar1hi_data = 0;
    m_bar2lo_data = 0;
    m_bar2hi_data = 0;
    m_bar3_data = 0;
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

    //Refer bug 200568151 Comment #13. 
    lwgpu = LWGpuResource::FindFirstResource();
    UINT32 arch = lwgpu->GetArchitecture();
    
    m_clk_mask_reg_length = s_clkMaskRegLenthAd102;
    clkMaskAccessMask0 = s_clkMaskAccessMask0Ad102;
    clkMaskAccessMask1 = s_clkMaskAccessMask1Ad102;
    
    if (arch == Gpu::GA100)
    {
        InfoPrintf("Current architecture for which the test run is GA100\n");
        m_clk_mask_reg_length = s_clkMaskRegLenthGa100;
    }
#if LWCFG(GLOBAL_GPU_IMPL_AD102)
    if(arch == Gpu::AD102)
    {
        InfoPrintf("Current architecture for which the test run is AD102\n");
        m_clk_mask_reg_length = s_clkMaskRegLenthAd102;
        clkMaskAccessMask0 = s_clkMaskAccessMask0Ad102;
        clkMaskAccessMask1 = s_clkMaskAccessMask1Ad102;
    }
#endif
#if LWCFG(GLOBAL_GPU_IMPL_AD103)
    if(arch == Gpu::AD103)
    {
        InfoPrintf("Current architecture for which the test run is AD103\n");
        m_clk_mask_reg_length = s_clkMaskRegLenthAd103;
        clkMaskAccessMask0 = s_clkMaskAccessMask0Ad103;
        clkMaskAccessMask1 = s_clkMaskAccessMask1Ad103;

    }
#endif
#if LWCFG(GLOBAL_GPU_IMPL_AD104)
    if(arch == Gpu::AD104)
    {
        InfoPrintf("Current architecture for which the test run is AD104\n");
        m_clk_mask_reg_length = s_clkMaskRegLenthAd104;
        clkMaskAccessMask0 = s_clkMaskAccessMask0Ad104;
        clkMaskAccessMask1 = s_clkMaskAccessMask1Ad104;

    }
#endif
#if LWCFG(GLOBAL_GPU_IMPL_AD106)
    if(arch == Gpu::AD106)
    {
        InfoPrintf("Current architecture for which the test run is AD106\n");
        m_clk_mask_reg_length = s_clkMaskRegLenthAd106;
        clkMaskAccessMask0 = s_clkMaskAccessMask0Ad106;
        clkMaskAccessMask1 = s_clkMaskAccessMask1Ad106;
    }
#endif
#if LWCFG(GLOBAL_GPU_IMPL_AD107)
    if(arch == Gpu::AD107)
    {
        InfoPrintf("Current architecture for which the test run is AD107\n");
        m_clk_mask_reg_length = s_clkMaskRegLenthAd107;
        clkMaskAccessMask0 = s_clkMaskAccessMask0Ad107;
        clkMaskAccessMask1 = s_clkMaskAccessMask1Ad107;
    }
#endif
}

// Destructor
Ad10xJtagRegResetCheck::~Ad10xJtagRegResetCheck(void)
{
}

// Create the boilerplate factory design pattern function for a test
// std_test_factoy(testname,classname)- test.h file has the define
STD_TEST_FACTORY(ad10x_jtag_reg_reset_check, Ad10xJtagRegResetCheck)

// Use SetupGpuPointers function to fetch the lwgpu pointer.
int Ad10xJtagRegResetCheck::Setup(void)
{

    int returlwal = SetupGpuPointers();

    getStateReport()->init("ad10x_jtag_reg_reset_check");
    getStateReport()->enable();

    return returlwal;
}

// After reset/reinitialize the gpu we need to fetch the lwgpu pointer again
// Below function will help to fetch the pointers again.
int Ad10xJtagRegResetCheck::SetupGpuPointers(void)
{

    lwgpu = LWGpuResource::FindFirstResource();

    m_regMap = lwgpu->GetRegisterMap();
    if(!m_regMap)
    {
        ErrPrintf("Ad10xJtagRegResetCheck::Setup: can't get a register map from the GPU!\n");
        return 0;
    }

    return 1;

}

void Ad10xJtagRegResetCheck::CleanUp(void)
{
    DebugPrintf("Ad10xJtagRegResetCheck::CleanUp(): Releasing GPU.\n");
    LWGpuSingleChannelTest::CleanUp();
}

void Ad10xJtagRegResetCheck::Run(void)
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

bool Ad10xJtagRegResetCheck::testNoFundamentalRst()
{
    // This variable fail is used to capture temporary return values.
    bool fail = true;

    InfoPrintf("Test No fundamental reset case - beginning \n");

    // STEP 1 : disable fundamental reset because we want to do hot reset, its enabled by default
    InfoPrintf("Test No fundamental reset case - Disabling fundamental reset upon Hot reset \n");
    fail = disable_fundamental_reset_on_hot_rst();
    if (fail)
    {
        ErrPrintf("Error:testNoFundamentalRst: Write to disable fundamental_reset_on_hot_rst Failed!\n");
        return fail;
    }

    fail = disable_xve_cfg_reset_on_hot_rst();         // disable CFG reset on hot rst
    if (fail)
    {
        ErrPrintf("Error:testNoFundamentalRst: Write to disable xve_cfg_reset_on_hot_rst Failed!\n");
        return fail;
    }

    // Added for setting ACCESS_SEC_MODE to 0x0 for turing. This is
    // the requirement to access all non-ISM registers in turing.
    lwgpu->SetRegFldNum("LW_PJTAG_ACCESS_SEC", "_SWITCH_TO_STANDARD", 1); // Configure ACCESS_SEC to SWITCH_TO_STANDARD since ISM_MODE is default.

    Utility::DelayNS(100); // Wait for 10k cycles before reading the ACCESS_SEC_MODE status.
    UINT32 rd_data;
    lwgpu->GetRegFldNum("LW_PJTAG_ACCESS_SEC", "_MODE", &rd_data);
    if (rd_data != 0) 
    {
        ErrPrintf("LW_PJTAG_ACCESS_SEC_MODE != 0x0 ... read as %d \n", rd_data);
        return fail;
    }
    // Logic to read and confirm ACCESS_SEC_MODE to 0x0 ends here.

    // STPE 2 : Write magic data into JTAG register
    InfoPrintf("Test No fundamental reset case - Writing to JTAG register \n");
    fail = test_logic_reg_write(JTAG_WRITE_DATA_DWORD_0, JTAG_WRITE_DATA_DWORD_1);
    if (fail)
    {
        ErrPrintf("Error : Write to Jtag register Failed!\n");
        return fail;
    }

    // STEP 3 : Read JTAG register back to confirm write
    InfoPrintf("Test No fundamental reset case - read back JTAG register to confirm write \n");
    fail = test_logic_reg_read_check (JTAG_WRITE_DATA_DWORD_0, JTAG_WRITE_DATA_DWORD_1);
    if (fail)
    {
        ErrPrintf("Error : Read back after write of JTAG reg Failed\n");
        return fail;
    }

    // STEP 4 : Apply hot reset ;
    // NOTE: ASSERT_FUNDAMENTAL_RESET_ON_HOT_RESET is set to 0, hot_reset won't trigger fundamental reset
    InfoPrintf("Test No fundamental reset case - applying Hot reset (without fundamental reset) \n");
    hot_reset();

    // Initialize GPU after Hot reset
    gpu_initialize();

    // STEP 5 : Verify JTAG register after reset - expect register to retain write data
    InfoPrintf("Test No fundamental reset case - read JTAG register after reset\n");
    fail = test_logic_reg_read_check (JTAG_WRITE_DATA_DWORD_0, JTAG_WRITE_DATA_DWORD_1);
    if (fail)
    {
        InfoPrintf("Test No fundamental reset case - JTAG register check after reset Failed! \n");
        return fail;
    }

    InfoPrintf("Test No fundamental reset case - Test PASSED! \n");
    return false;
}

bool Ad10xJtagRegResetCheck::testFundamentalRst()
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
        ErrPrintf("Error:testFundamentalRst: Write to enable fundamental_reset_on_hot_rst Failed!\n");
        return fail;
    }

    // Added for setting ACCESS_SEC_MODE to 0x0 for turing. This is
    // the requirement to access all non-ISM registers in turing.
    lwgpu->SetRegFldNum("LW_PJTAG_ACCESS_SEC", "_SWITCH_TO_STANDARD", 1); // Configure ACCESS_SEC to SWITCH_TO_STANDARD since ISM_MODE is default.

    Utility::DelayNS(100); // Wait for 10k cycles before reading the ACCESS_SEC_MODE status.
    UINT32 rd_data;
    lwgpu->GetRegFldNum("LW_PJTAG_ACCESS_SEC", "_MODE", &rd_data);
    if (rd_data != 0) 
    {
        ErrPrintf("LW_PJTAG_ACCESS_SEC_MODE != 0x0 ... read as %d \n", rd_data);
        return fail;
    }
    // STEP 2 : Write magic data into JTAG register
    fail = test_logic_reg_write(JTAG_WRITE_DATA_DWORD_0, JTAG_WRITE_DATA_DWORD_1);
    if (fail)
    {
        ErrPrintf("Test fundamental reset case - JTAG register write failed \n");
        return fail;
    }

    // STEP 3 : Read JTAG register back to confirm write
    InfoPrintf("Test fundamental reset case - read back JTAG register to confirm write \n");
    fail = test_logic_reg_read_check (JTAG_WRITE_DATA_DWORD_0, JTAG_WRITE_DATA_DWORD_1);
    if (fail)
    {
        ErrPrintf("Error : Read back after write of JTAG reg Failed\n");
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
    fail = test_logic_reg_read_check (RESET_DATA_DWORD_0, RESET_DATA_DWORD_1);
    if (fail)
    {
        InfoPrintf("Test fundamental reset case - JTAG register check after reset Failed! \n");
        return fail;
    }

    InfoPrintf("Test fundamental reset case - Test PASSED! \n");
    return false;
}
bool Ad10xJtagRegResetCheck::fundamentalReset()
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
        //return fail;
    }
    UINT32 misc_3_init,misc_3_test,alias_init,alias_test,dbg_1_init,dbg_1_test;
    lwgpu->GetRegFldNum("LW_PCFG_XVE_MISC_3", "", &misc_3_init);
    lwgpu->SetRegFldNum("LW_PCFG_XVE_MISC_3", "", 0xffffff);
    lwgpu->GetRegFldNum("LW_PCFG_XVE_MISC_3", "", &misc_3_test);
    if(0xffffff != misc_3_test)
    {
        ErrPrintf("FundRegResetCheck: LW_PCFG_XVE_MISC_3 Expected=0xffffff,actual=0x%x \n",misc_3_test);
        fail = true;
    }
    lwgpu->GetRegFldNum("LW_PCFG_XVE_SUBSYSTEM_ALIAS", "_VENDOR_ID", &alias_init);
    lwgpu->SetRegFldNum("LW_PCFG_XVE_SUBSYSTEM_ALIAS", "_VENDOR_ID", 0xffff);
    lwgpu->GetRegFldNum("LW_PCFG_XVE_SUBSYSTEM_ALIAS", "_VENDOR_ID", &alias_test);
    if(0xffff != alias_test)
    {
        ErrPrintf("FundRegResetCheck:LW_PCFG_XVE_SUBSYSTEM_ALIAS Expected=0xffff,actual=0x%x \n",alias_test);
        fail = true;
    }
    lwgpu->GetRegFldNum("LW_PCFG_XVE_DEBUG_1", "_P2PSLAVE_CPL_TO_DISABLE", &dbg_1_init);
    lwgpu->SetRegFldNum("LW_PCFG_XVE_DEBUG_1", "_P2PSLAVE_CPL_TO_DISABLE", 0x1);
    lwgpu->GetRegFldNum("LW_PCFG_XVE_DEBUG_1", "_P2PSLAVE_CPL_TO_DISABLE", &dbg_1_test);
    if(0x1 != dbg_1_test)
    {
        ErrPrintf("FundRegResetCheck:LW_PCFG_XVE_DEBUG_1 Expected=0x1,actual=0x%x \n",dbg_1_test);
        fail = true;
    }
    if (fail)
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
    lwgpu->GetRegFldNum("LW_PCFG_XVE_MISC_3", "", &rdval);
    if(rdval != misc_3_init)
    {
        ErrPrintf("FundRegResetCheck: LW_PCFG_XVE_MISC_3 Expected=0x%x,actual=0x%x \n",misc_3_init,rdval);
        fail = true;
    }
    lwgpu->GetRegFldNum("LW_PCFG_XVE_SUBSYSTEM_ALIAS", "_VENDOR_ID", &rdval);
    if(rdval != alias_init)
    {
        ErrPrintf("FundRegResetCheck: LW_PCFG_XVE_SUBSYSTEM_ALIAS Expected=0x%x,actual=0x%x \n",rdval,alias_init);
        fail = true;
    }
    lwgpu->GetRegFldNum("LW_PCFG_XVE_DEBUG_1", "_P2PSLAVE_CPL_TO_DISABLE", &rdval);
    if(rdval != dbg_1_init)
    {
        ErrPrintf("FundRegResetCheck: LW_PCFG_XVE_DEBUG_1 Expected=0x%x,actual=0x%x \n",dbg_1_init,rdval);
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
bool Ad10xJtagRegResetCheck::disable_fundamental_reset_on_hot_rst (void)
{
    InfoPrintf("Disabling FUNDAMENTAL_RESET_ON_HOT_RESET \n");
    bool fail = false ;
    UINT32 reg_rd_val = 0;
    UINT32 golden_val = 0;

    // Reg: LW_PCFG_XVE_RESET_CTRL
    // Field : ASSERT_FUNDAMENTAL_RESET_ON_HOT_RESET - if set, a fundamental reset is asserted upon hot reset - disable it
    if(lwgpu->SetRegFldDef("LW_PCFG_XVE_RESET_CTRL", "_ASSERT_FUNDAMENTAL_RESET_ON_HOT_RESET", "_DISABLED"))
    {
        ErrPrintf("Ad10xJtagRegResetCheck: disable_fundamental_reset_on_hot_rst, Could not be updated LW_PCFG_XVE_RESET_CTRL_ASSERT_FUNDAMENTAL_RESET_ON_HOT_RESET to value _DISABLED\n");
        fail = true;
    }

    // Verify the above write
    lwgpu->GetRegFldNum("LW_PCFG_XVE_RESET_CTRL", "_ASSERT_FUNDAMENTAL_RESET_ON_HOT_RESET", &reg_rd_val);
    lwgpu->GetRegFldDef("LW_PCFG_XVE_RESET_CTRL", "_ASSERT_FUNDAMENTAL_RESET_ON_HOT_RESET", "_DISABLED", &golden_val);
    if(reg_rd_val != golden_val)
    {
        ErrPrintf("Ad10xJtagRegResetCheck: disable_fundamental_reset_on_hot_rst, Verify write failed.. reg_rd_val=0x%x,golden_val=0x%x \n",reg_rd_val,golden_val);
        fail = true;
    }

    return fail;
}

bool Ad10xJtagRegResetCheck::enable_fundamental_reset_on_hot_rst (void)
{
    InfoPrintf("Enabling FUNDAMENTAL_RESET_ON_HOT_RESET \n");
    bool fail = false ;
    UINT32 reg_rd_val = 0;
    UINT32 golden_val = 0;

    // Reg: LW_PCFG_XVE_RESET_CRTL
    // Field : ASSERT_FUNDAMENTAL_RESET_ON_HOT_RESET - if set, a fundamental reset is asserted upon hot reset - ENABLE it
    if(lwgpu->SetRegFldDef("LW_PCFG_XVE_RESET_CTRL", "_ASSERT_FUNDAMENTAL_RESET_ON_HOT_RESET", "_ENABLED"))
    {
        ErrPrintf("Ad10xJtagRegResetCheck: enable_fundamental_reset_on_hot_rst, Could not be updated LW_PCFG_XVE_RESET_CTRL_ASSERT_FUNDAMENTAL_RESET_ON_HOT_RESET to value _ENABLED\n");
        fail = true;
    }

    // Verify the above write
    lwgpu->GetRegFldNum("LW_PCFG_XVE_RESET_CTRL", "_ASSERT_FUNDAMENTAL_RESET_ON_HOT_RESET", &reg_rd_val);
    lwgpu->GetRegFldDef("LW_PCFG_XVE_RESET_CTRL", "_ASSERT_FUNDAMENTAL_RESET_ON_HOT_RESET", "_ENABLED", &golden_val);
    if(reg_rd_val != golden_val)
    {
        ErrPrintf("Ad10xJtagRegResetCheck: enable_fundamental_reset_on_hot_rst, Verify write failed.. reg_rd_val=0x%x,golden_val=0x%x \n",reg_rd_val,golden_val);
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
bool Ad10xJtagRegResetCheck::disable_xve_cfg_reset_on_hot_rst (void)
{
    bool fail = false ;
    UINT32 reg_rd_val = 0;
    UINT32 golden_val = 0;
    
    // Bug 2800397: LW_XVE_PRIV_XV_0_CYA_CFG_HOT_RESET moved to LW_XVE_PRIV_XV_0_CYA_RESET_CFG
    // Reg: LW_PCFG_XVE_PRIV_XV_0_CYA_RESET_CFG
    // Field : _HOT_RESET - disable it
    // Field : _DL_UP - disable it
    if(lwgpu->SetRegFldDef("LW_PCFG_XVE_PRIV_XV_0_CYA_RESET_CFG", "_HOT_RESET","_NORESET"))
    {
        ErrPrintf("Ad10xJtagRegResetCheck: disable_xve_cfg_reset_on_hot_rst, Could not be updated LW_PCFG_XVE_PRIV_XV_0_CYA_RESET_CFG_HOT_RESET to value _NORESET\n");
        fail = true;
    }
    if(lwgpu->SetRegFldDef("LW_PCFG_XVE_PRIV_XV_0_CYA_RESET_CFG", "_DL_UP","_NORESET"))
    {
        ErrPrintf("Ad10xJtagRegResetCheck: disable_xve_cfg_reset_on_hot_rst, Could not be updated LW_PCFG_XVE_PRIV_XV_0_CYA_RESET_CFG_DL_UP to value _NORESET\n");
        fail = true;
    }

    // Verify the above write
    lwgpu->GetRegFldNum("LW_PCFG_XVE_PRIV_XV_0_CYA_RESET_CFG", "_HOT_RESET", &reg_rd_val);
    lwgpu->GetRegFldDef("LW_PCFG_XVE_PRIV_XV_0_CYA_RESET_CFG", "_HOT_RESET","_NORESET", &golden_val);
    if(reg_rd_val != golden_val)
    {
        ErrPrintf("Ad10xJtagRegResetCheck: disable_xve_cfg_reset_on_hot_rst, Verify write failed.. reg_rd_val=0x%x,golden_val=0x%x \n",reg_rd_val,golden_val);
        fail = true;
    }
    lwgpu->GetRegFldNum("LW_PCFG_XVE_PRIV_XV_0_CYA_RESET_CFG", "_DL_UP", &reg_rd_val);
    lwgpu->GetRegFldDef("LW_PCFG_XVE_PRIV_XV_0_CYA_RESET_CFG", "_DL_UP","_NORESET", &golden_val);
    if(reg_rd_val != golden_val)
    {
        ErrPrintf("Ad10xJtagRegResetCheck: disable_xve_cfg_reset_on_hot_rst, Verify write (_CYA_CFG_DL_UP) failed.. reg_rd_val=0x%x,golden_val=0x%x \n",reg_rd_val,golden_val);
        fail = true;
    }
    return fail;

}

// Save the config space registers before the fundamental reset . Fundamental reset would reset the entire GPU
// So we need to restore these registers later.
void Ad10xJtagRegResetCheck::save_config_space()
{
    lwgpu->GetRegFldNum("LW_PCFG_XVE_BAR0", "", &m_bar0_data);
    lwgpu->GetRegFldNum("LW_PCFG_XVE_INTR_GNT", "", &m_irqSet_data);
    lwgpu->GetRegFldNum("LW_PCFG_XVE_SUBSYSTEM", "", &m_vendid_data);
    lwgpu->GetRegFldNum("LW_PCFG_XVE_BAR1_LO", "", &m_bar1lo_data);
    lwgpu->GetRegFldNum("LW_PCFG_XVE_BAR1_HI", "", &m_bar1hi_data);
    lwgpu->GetRegFldNum("LW_PCFG_XVE_BAR2_LO", "", &m_bar2lo_data);
    lwgpu->GetRegFldNum("LW_PCFG_XVE_BAR2_HI", "", &m_bar2hi_data);
    lwgpu->GetRegFldNum("LW_PCFG_XVE_BAR3", "", &m_bar3_data);

    m_bar0_addr = m_regMap->FindRegister("LW_XVE_BAR0")->GetAddress();
    m_irqSet_addr = m_regMap->FindRegister("LW_XVE_INTR_GNT")->GetAddress();
    m_vendid_addr = m_regMap->FindRegister("LW_XVE_SUBSYSTEM")->GetAddress();
    m_bar1lo_addr = m_regMap->FindRegister("LW_XVE_BAR1_LO")->GetAddress();
    m_bar1hi_addr = m_regMap->FindRegister("LW_XVE_BAR1_HI")->GetAddress();
    m_bar2lo_addr = m_regMap->FindRegister("LW_XVE_BAR2_LO")->GetAddress();
    m_bar2hi_addr = m_regMap->FindRegister("LW_XVE_BAR2_HI")->GetAddress();
    m_bar3_addr = m_regMap->FindRegister("LW_XVE_BAR3")->GetAddress();
}

// Restore the saved config space registers after reset
bool Ad10xJtagRegResetCheck::restore_config_space()
{
    if(pci_write(m_bar0_addr,m_bar0_data))
    {
        return true;
    }
    if(pci_write(m_irqSet_addr,m_irqSet_data))
    {
        return true;
    }
    if(pci_write(m_vendid_addr,m_vendid_data))
    {
        return true;
    }
    if(pci_write(m_bar1lo_addr,m_bar1lo_data))
    {
        return true;
    }
    if(pci_write(m_bar1hi_addr,m_bar1hi_data))
    {
        return true;
    }
    if(pci_write(m_bar2lo_addr,m_bar2lo_data))
    {
        return true;
    }
    if(pci_write(m_bar2hi_addr,m_bar2hi_data))
    {
        return true;
    }
    if(pci_write(m_bar3_addr,m_bar3_data))
    {
        return true;
    }
    return false;
}

// Function for pci write and make sure write happened correctly by a read check
bool Ad10xJtagRegResetCheck::pci_write(UINT32 offset , UINT32 data)
{
    UINT32 reg_val = 0x0;
    bool fail = false;

    Platform::PciWrite32(m_pci_domain_num,m_pci_bus_num,m_pci_dev_num,m_pci_func_num,offset,data);
    InfoPrintf("Restore the config space m_pci_domain_num = 0x%x , m_pci_bus_num = 0x%x , address =: 0x%x    data = 0x%x \n",m_pci_domain_num,m_pci_bus_num,offset,data);

    // verify the write
    Platform::PciRead32(m_pci_domain_num,m_pci_bus_num,m_pci_dev_num,m_pci_func_num,offset,&reg_val);
    if(reg_val != data)
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

bool Ad10xJtagRegResetCheck::test_logic_reg_write(UINT32 dword0, UINT32 dword1)
{
    UINT32 status;

    bool fail = false;

    InfoPrintf("test_logic_reg_write - writing to jtag register jtag2tmc_bp_clk_mask ... \n");
    status = 0;

    // Setting captureDR/updateDR to 1. So that the write data won't affect the other functional logic
    host2jtag_config_write(CONFIG_WRITE_DATA);

    // Issue new request on JTAG control reg - req type WRITE
    fail = host2jtag_ctrl_write (s_reqNew, status, s_chipletGmbS0, s_dwordZero, m_clk_mask_reg_length, s_clkMaskInstId, WRITE_INTENT);
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
    // write transaction with s_reqClr .We will do a new control register write for the next request/transaction .
    fail = host2jtag_ctrl_write (s_reqClr, status, s_chipletGmbS0, s_dwordZero, m_clk_mask_reg_length, s_clkMaskInstId, WRITE_INTENT);
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
bool Ad10xJtagRegResetCheck::test_logic_reg_read_check(UINT32 const exp_dword0, UINT32 const exp_dword1)
{

    UINT32 read_data = 0;
    UINT32 status = 0;
    bool fail = false;

    InfoPrintf("test_logic_reg_read_check - read jtag register jtag2tmc_bp_clk_mask and check against expected data \n");

    // Setting captureDR/updateDR to 1.
    host2jtag_config_write(CONFIG_WRITE_DATA);

    // Read DWORD ZERO for jtag register
    // Issue new request on JTAG control reg - req type READ
    fail = host2jtag_ctrl_write (s_reqNew, status, s_chipletGmbS0, s_dwordZero, m_clk_mask_reg_length, s_clkMaskInstId, READ_INTENT);
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
    if ((read_data & clkMaskAccessMask0) != (exp_dword0 & clkMaskAccessMask0))
    {
        ErrPrintf("test_logic_reg_read_check : jtag register jtag2tmc_bp_clk_mask: DWORD 0 : read back value is %x, the expected value is %x\n", read_data, exp_dword0);
        return (true);
    }

    // Clear request on jtag control
    fail = host2jtag_ctrl_write (s_reqClr, status, s_chipletGmbS0, s_dwordZero, m_clk_mask_reg_length, s_clkMaskInstId, READ_INTENT);
    if (fail)
    {
        ErrPrintf("test_logic_reg_read_check : Write to Jtag contorl reg failed \n");
        return fail;
    }

    // Setting captureDR/updateDR to 1.
    host2jtag_config_write(CONFIG_WRITE_DATA);

    // Read DWORD ONE for jtag register
    // Issue new request on JTAG control reg - req type READ
    fail = host2jtag_ctrl_write (s_reqNew, status, s_chipletGmbS0, s_dwordOne, m_clk_mask_reg_length, s_clkMaskInstId, READ_INTENT);
    if (fail)
    {
        ErrPrintf("test_logic_reg_read_check : Write to Jtag contorl reg failed \n");
        return fail;
    }

    // read the JTAG data register
    host2jtag_data_read (read_data, m_clk_mask_reg_length, true); // true for last dword

    if ((read_data & clkMaskAccessMask1) != (exp_dword1 & clkMaskAccessMask1))
    {
        ErrPrintf("test_logic_reg_read_check : jtag register jtag2tmc_bp_clk_mask: DWORD 1 : read back value is %x, the expected value is %x\n", read_data, exp_dword1);
        return (true);
    }

    // Clear request on jtag control
    fail = host2jtag_ctrl_write (s_reqClr, status, s_chipletGmbS0, s_dwordOne, m_clk_mask_reg_length, s_clkMaskInstId, READ_INTENT);
    if (fail)
    {
        ErrPrintf("test_logic_reg_read_check : Write to Jtag contorl reg failed \n");
    }

    return fail;
}

// Shut down the gpu and force the hot reset from tb.
void Ad10xJtagRegResetCheck::hot_reset(void)
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

bool Ad10xJtagRegResetCheck::gpu_initialize(void)
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
bool Ad10xJtagRegResetCheck::wait_for_jtag_ctrl(void)
{
    UINT32 hostStatus = CTRL_STATUS_BUSY;
    UINT32 host_ctrl_attempts = 0;

    InfoPrintf("Ad10xJtagRegResetCheck::wait_for_jtag_ctrl : Waiting for Host Control \n");
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

    InfoPrintf("Ad10xJtagRegResetCheck::wait_for_jtag_ctrl : Acquired Host Control\n");
    return false;
}

// function writes the control word to LW_PJTAG_ACCESS_CTRL register .
// CTRL register needs to be set before all write and read.
bool Ad10xJtagRegResetCheck::host2jtag_ctrl_write(UINT32 req_ctrl, UINT32 status, UINT32 chiplet_sel, UINT32 dword_en, UINT32 reg_length, UINT32 instr_id, bool read_or_write)
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

    InfoPrintf("Ad10xJtagRegResetCheck::host2jtag_ctrl_write : Writing Into LW_PJTAG_ACCESS_CTRL : 0x%x\n", host2jtagRegCtrl);
    InfoPrintf("Ad10xJtagRegResetCheck::host2jtag_ctrl_write : read_or_write : %s, reg_length : 0x%x, Chiplet : 0x%x\n", (read_or_write == WRITE_INTENT ? "write" : "read"), reg_length, chiplet_sel);

    if(req_ctrl == s_reqNew)
    {
        if(wait_for_jtag_ctrl())
        {
          ErrPrintf("Ad10xJtagRegResetCheck::host2jtag_ctrl_write:Didn't acquire host control within %d attempts \n",MAX_HOST_CTRL_ATTEMPTS);
          return true;
        }
    }
    return false;
}

// Setting captureDR/updateDR to 1. So that the write data won't affect the other functional logic.
void Ad10xJtagRegResetCheck::host2jtag_config_write(UINT32 config_write_data)
{
    InfoPrintf("Writing into LW_PJTAG_ACCESS_CONFIG : 0x%x\n", config_write_data);
    lwgpu->SetRegFldNum("LW_PJTAG_ACCESS_CONFIG", "",config_write_data);
}

// Function writes the write data to the LW_PJTAG_ACCESS_DATA register
void Ad10xJtagRegResetCheck::host2jtag_data_write(UINT32 data_value)
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
void Ad10xJtagRegResetCheck::host2jtag_data_read(UINT32& data_value, UINT32 reg_length, bool last_dword)
{

    InfoPrintf("Ad10xJtagRegResetCheck::host2jtag_data_read : Reading from LW_PJTAG_ACCESS_DATA\n ");
    //data_value = lwgpu->RegRd32(LW_PJTAG_ACCESS_DATA);
    lwgpu->GetRegFldNum("LW_PJTAG_ACCESS_DATA", "", &data_value);

    InfoPrintf("Ad10xJtagRegResetCheck::host2jtag_data_read : reg_length = 0x%x , s_readBits =0x%x, data_value  =0x%x    \n",reg_length,s_readBits,data_value);
    // If last_dword has less than 32bits, then we need to shift the data (no of valid bits times) to the right.
    if(last_dword)
    {
        if ((reg_length % s_readBits) != 0)
        {
            data_value = ( data_value >> (s_readBits - (reg_length % s_readBits) )) ;
        }
    }
    InfoPrintf("Ad10xJtagRegResetCheck::host2jtag_data_read : Reg read value is data_value = 0x%x \n",data_value);
}
