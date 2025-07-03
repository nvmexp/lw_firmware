/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2006-2021 by LWPU Corporation.  All rights reserved. 
 * All information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */
// To facilitate faster compiles via pre-compiled headers, keep "stdtest.h" first
#include "mdiag/tests/stdtest.h"

#include "l1_substate.h"

#include "core/include/gpu.h"
#include "core/include/rc.h"
#include "device/interface/pcie.h"
#include "gpu/include/gpusbdev.h"
#include "gpu/perf/pmusub.h"
#include "hopper/gh100/dev_lw_xpl.h"
#include "hopper/gh100/dev_xtl_ep_pcfg_gpu.h"
#include "hopper/gh100/dev_xtl_ep_pri.h"
#include "mdiag/lwgpu.h"
#include "mdiag/tests/test_state_report.h"
#include "mdiag/utils/lwgpu_channel.h"
#include "pascal/gp100/dev_host.h"
#include "pascal/gp100/dev_lw_xp.h"
#include "pascal/gp100/dev_trim.h"
#include "sim/IChip.h"
#include "turing/tu102/dev_lw_xve.h"
#include "turing/tu102/dev_lw_xve1.h"
#include "turing/tu102/dev_lw_xve3g_fn_ppc.h"
#include "turing/tu102/dev_lw_xve3g_fn_xusb.h"

// define which tests are run
static int pcipm_l1_2 = 0;
static int pcipm_l1_1 = 0;
static int aspm_l1_2  = 0;
static int aspm_l1_1  = 0;
static int bfm_initiate_exit = 0;
static int selfcheck_EscapeRead = 0;

extern const ParamDecl L1_substate_params[] =
{
    SIMPLE_PARAM("-pcipm_l1_2", "Testing PCIPM L1.2"),
    SIMPLE_PARAM("-pcipm_l1_1", "Testing PCIPM L1.1"),
    SIMPLE_PARAM("-aspm_l1_2", "Testing ASPM L1.2"),
    SIMPLE_PARAM("-aspm_l1_1", "Testing ASPM L1.1"),
    SIMPLE_PARAM("-bfm_initiate_exit", "Testing bfm L1 exit"),
    STRING_PARAM("-pwrimage", "String to specify which binary image to use to bootstrap PMU"),
    STRING_PARAM("-selfcheck_EscapeRead", "selfcheck seq state through EscapedRead seq_st"),
    PARAM_SUBDECL(lwgpu_single_params),
    LAST_PARAM
};

// constructor
L1_substate::L1_substate(ArgReader *params) :
    LWGpuSingleChannelTest(params)
{
    if (params->ParamPresent("-pcipm_l1_2"))
       pcipm_l1_2 = 1;
    if (params->ParamPresent("-pcipm_l1_1"))
       pcipm_l1_1 = 1;
    if (params->ParamPresent("-aspm_l1_2"))
       aspm_l1_2 = 1;
    if (params->ParamPresent("-aspm_l1_1"))
       aspm_l1_1 = 1;
    if (params->ParamPresent("-bfm_initiate_exit"))
       bfm_initiate_exit = 1;
    if ( params->ParamPresent( "-pwrimage" ) > 0 )
        m_paramsString = params->ParamStr( "-pwrimage" );
    if (params->ParamPresent("-selfcheck_EscapeRead"))
       selfcheck_EscapeRead = 1;
}

// destructor
L1_substate::~L1_substate(void)
{
}

// Factory
STD_TEST_FACTORY(L1_substate,L1_substate)

// setup method
int L1_substate::Setup(void)
{
    lwgpu = LWGpuResource::FindFirstResource();
    arch = lwgpu->GetArchitecture();
    ch = lwgpu->CreateChannel();
    if(!ch)
    {
        ErrPrintf("L1_substate::Setup failed to create channel\n");
        return 0;
    }
    getStateReport()->init("L1_substate");
    getStateReport()->enable();

    return 1;
}

//CleanUp
void L1_substate::CleanUp(void)
{
    if(ch)
    {
        delete ch;
        ch = 0;
    }
    if (lwgpu)
    {
        DebugPrintf("Gpio::CleanUp(): Releasing GPU.\n");
        lwgpu = 0;
    }
    LWGpuSingleChannelTest::CleanUp();
}

void L1_substate::DelayNs(UINT32 Lwclks)
{
  // Lwrrently this function is in terms of .2nS / lwclk. This is incorrectly done
  // in RTL. This should be a close approx of 1 nanosecond.
  Platform::EscapeWrite("CLOCK_WAIT",IChip::ECLOCK_LWCLK,0,Lwclks * 5);
}

int L1_substate::Checkstate(UINT32 expected_state )
{
        UINT32 num_try;
        UINT32 seq_st = 0;
        Platform::EscapeRead("deepL1_seq_st", 0, 32, &seq_st);
        InfoPrintf("deepL1_seq_st = %d!\n", seq_st);

        num_try = 1;
        L1_substate::DelayNs(2000);
        Platform::EscapeRead("deepL1_seq_st", 0, 32, &seq_st);
        InfoPrintf("L1_substate state :%08x -- try %d  \n",seq_st,num_try);

        while((num_try < 200) && (seq_st != expected_state))
        {
          L1_substate::DelayNs(2000);
          num_try = num_try +1;
          Platform::EscapeRead("deepL1_seq_st", 0, 32, &seq_st);
          InfoPrintf("L1_substate state :%08x -- try %d  \n",seq_st,num_try);
        }

        if ( (num_try>= 200)&&(seq_st!= expected_state) )
        {
            SetStatus(TEST_FAILED);
            getStateReport()->runFailed("Failed check state.\n");
            ErrPrintf("L1_substate state does not go to expected state : %08x \n", expected_state);
            return(1);
       }
       else return(0);
}

// run - main routine
void L1_substate::Run(void)
{
      UINT32 data = 0;
      UINT32 xve_multi_func = 0;

      InfoPrintf("L1_substate Test at Run\n");
      SetStatus(TEST_INCOMPLETE);

      InfoPrintf("test L1_substate() starting ....\n");

    //-------------------------------------------------------------
    // Set LW_XPL_PL_PAD_CTL_PRI_XPL_TX/RX/XCLK_CG (0x00084000+0x2700) IDLE_CG_DLY_CNT to zero, no filter
    // enable  LW_XPL_PL_PAD_CTL_PRI_XPL_RXCLK_CG_IDLE_CG_EN
    //-------------------------------------------------------------
      data = lwgpu->RegRd32(0x00086700);
      data = FLD_SET_DRF(_XPL, _PL_PAD_CTL_PRI_XPL_XCLK_CG, _IDLE_CG_DLY_CNT, _HWINIT, data);
      data = FLD_SET_DRF(_XPL, _PL_PAD_CTL_PRI_XPL_XCLK_CG, _IDLE_CG_EN, _ENABLED, data);
      lwgpu->RegWr32(0x00086700, data);
      
      data = lwgpu->RegRd32(0x00086710);
      data = FLD_SET_DRF(_XPL, _PL_PAD_CTL_PRI_XPL_TXCLK_CG, _IDLE_CG_DLY_CNT, _HWINIT, data);
      data = FLD_SET_DRF(_XPL, _PL_PAD_CTL_PRI_XPL_TXCLK_CG, _IDLE_CG_EN, _ENABLED, data);
      lwgpu->RegWr32(0x00086710, data);

      data = lwgpu->RegRd32(0x00086720);
      data = FLD_SET_DRF(_XPL, _PL_PAD_CTL_PRI_XPL_RXCLK_CG, _IDLE_CG_DLY_CNT, _HWINIT, data);
      data = FLD_SET_DRF(_XPL, _PL_PAD_CTL_PRI_XPL_RXCLK_CG, _IDLE_CG_EN, _ENABLED, data);
      lwgpu->RegWr32(0x00086720, data);
 

      Platform::Delay(10);

    //---------------------------------------------------------------
    // set LW_XTL_EP_PRI_MISCL (0x00091B28) LW_XTL_EP_PRI_MISCL_CYA_PMCSR_POWER_STATE_D3HOT_IGNORE to 0
    //---------------------------------------------------------------
      data = lwgpu->RegRd32(0x00091B28);
      data = data & 0xfffffffe ;
      lwgpu->RegWr32(0x00091B28, data);

      Platform::Delay(10);

      //-----------------------------------------------------
      // Read LW_PCFG_XVE_MISC_1 (0x0008800c)
      // check LW_PCFG_XVE_MISC_1_HEADER_TYPE bit status
      //----------------------------------------------------
      //xve_multi_func = lwgpu->RegRd32(0x0008800c);
      //InfoPrintf("xve_multi_func = %x \n",xve_multi_func);
      //xve_multi_func = xve_multi_func & 0x00800000;
      xve_multi_func = 0;
      InfoPrintf("xve_multi_func = %x \n",xve_multi_func);


    //---------------------------------------------------------------
    // set LW_PCFG_XVE1_PRIV_MISC (0x0008a14c) LW_XVE1_PRIV_MISC_CYA_PMCSR_POWER_STATE_D3HOT_IGNORE to 0
    //---------------------------------------------------------------
    if (xve_multi_func == 0x00800000) {
        data = lwgpu->RegRd32(0x8a000+LW_XVE1_PRIV_MISC);
        data = data & 0x7fffffff ;
        lwgpu->RegWr32(0x8a000+LW_XVE1_PRIV_MISC, data);
    }
    Platform::Delay(10);

    //---------------------------------------------------------
    // Config idle counter register if selfcheck_EscapeRead=0
    //---------------------------------------------------------

    if (!selfcheck_EscapeRead)
    {
        //Enable idle_mask(1) for deepL1 event
        data = lwgpu->RegRd32(0x0010be44);
        data = data | 0x08000000;
        lwgpu->RegWr32(0x0010be44 , data);

        //assert reset for idle_counter(1)
        data = lwgpu->RegRd32(0x0010bf84);
        data = data | 0x80000000;
        lwgpu->RegWr32(0x0010bf84 , data);

        //de-assert reset for idle_counter(1)
        data = lwgpu->RegRd32(0x0010bf84);
        data = data & 0x00000000;
        lwgpu->RegWr32(0x0010bf84 , data);

        //Set idle_ctl(1) to idle count
        data = lwgpu->RegRd32(0x0010bfc4);
        data = data | 0x00000001;
        lwgpu->RegWr32(0x0010bfc4 , data);
    }

    //-----------------------------------------------------
    // LW_EP_PCFG_GPU_LINK_CONTROL_STATUS (0x00000070)  in dev_xtl_ep_pcfg_gpu.h
    //  - Set LW_EP_PCFG_GPU_LINK_CONTROL_STATUS_CLOCK_PM_ (bit 8) to 1'b1.
    //-----------------------------------------------------
    data = lwgpu->CfgRd32(LW_EP_PCFG_GPU_LINK_CONTROL_STATUS);
    data = data | 0x100;
    lwgpu->CfgWr32(LW_EP_PCFG_GPU_LINK_CONTROL_STATUS, data);
    
    //to check xusb and ppc presence
    //Read LW_PCFG_XVE_XUSB_STATUS and _PPC_STATUS
    // Following assignment is only required in cases where explicity fuse is required for 
    // disabling XUSB/PPC and reading throught status registers. Commenting out now
    UINT32 xusb_present;// = (lwgpu->RegRd32(0x88000 + LW_XVE_XUSB_STATUS) & 0x80000000); 
    UINT32 ppc_present;//  = (lwgpu->RegRd32(0x88000 + LW_XVE_PPC_STATUS) & 0x80000000);

    //Check through escape read if xusb & ppc are present
    //Code is implemented in systop_misc_pcie.vx
    UINT32 get_xusb_enable_from_defines;
    UINT32 get_ppc_enable_from_defines;
    Platform::EscapeRead("get_xusb_enable_from_defines",0,1,&get_xusb_enable_from_defines);
    Platform::EscapeRead("get_ppc_enable_from_defines",0,1,&get_ppc_enable_from_defines);
   
    if(!get_xusb_enable_from_defines) xusb_present = 0;
    if(!get_ppc_enable_from_defines) ppc_present = 0;
 
   
    ////////////////////////////
    //Save Off PCI parameters for drv
    //
    GpuSubdevice * pSubdev;
    pSubdev = lwgpu->GetGpuSubdevice();
    auto pGpuPcie = pSubdev->GetInterface<Pcie>();
    UINT32 pci_domain_num = pGpuPcie->DomainNumber();
    UINT32 pci_bus_num = pGpuPcie->BusNumber();
    UINT32 pci_dev_num = pGpuPcie->DeviceNumber();
    //UINT32 pci_func_num = pGpuPcie->FunctionNumber();

    //-----------------------------------------------------
    // -set LW_XVE_XUSB_LINK_CONTROL_STATUS_CLOCK_PM (bit 8) to 1'b1
    //-----------------------------------------------------
    if( (xusb_present == 0x80000000) && (ppc_present == 0x80000000) ){
        Platform::PciWrite32(pci_domain_num,
                           pci_bus_num,
                           pci_dev_num,
                           2, // xusb fn num
                           LW_XVE_LINK_CONTROL_STATUS, //offset is same for all functions
                           DRF_NUM(_XVE, _LINK_CONTROL_STATUS, _CLOCK_PM, 1));

        //-----------------------------------------------------
        // -set LW_XVE_PPC_LINK_CONTROL_STATUS_CLOCK_PM (bit 8) to 1'b1
        //-----------------------------------------------------
        Platform::PciWrite32(pci_domain_num,
                           pci_bus_num,
                           pci_dev_num,
                           3, // xusb fn num
                           LW_XVE_LINK_CONTROL_STATUS, //offset is same for all functions
                           DRF_NUM(_XVE, _LINK_CONTROL_STATUS, _CLOCK_PM, 1));
    }
    //-----------------------------------------------------
    // LW_XVE1_LINK_CONTROL_STATUS (0x00000088)  in dev_lw_xve1.ref
    //  - Set LW_XVE1_LINK_CONTROL_STATUS_CLOCK_PM (bit 8) to 1'b1.
    //-----------------------------------------------------
    if (xve_multi_func == 0x00800000) {
            data = lwgpu->RegRd32(0x8a000+LW_XVE1_LINK_CONTROL_STATUS);
            data = data | 0x100;
            lwgpu->RegWr32(0x8a000+LW_XVE1_LINK_CONTROL_STATUS, data);
    }

    //-----------------------------------------------------
    // Setting up LW_THERM_GATE_CTRL(21) XVE (0x00020254)
    //  - Set BLK_CLK to AUTO mode bit3:2 , 2'b1
    //  In Ampere XVE BLCG register has moved to shared register. Refer bug http://lwbugs/2623075/6
    //-----------------------------------------------------
    data = lwgpu->RegRd32(0x00020280);
    data = data & 0xffffffff;
    lwgpu->RegWr32(0x00020280, data);

    //-----------------------------------------------------
    // Setting up LW_XVE_PRIV_XV_1 (0x00000488)
    //-----------------------------------------------------
    //  - Disable LW_XVE_PRIV_XV_1_CYA_XVE_MULTIFUNC
    //data = lwgpu->RegRd32(0x00088488);
    //data = FLD_SET_DRF(_XVE, _PRIV_XV_1, _CYA_XVE_MULTIFUNC, _ENABLE, data);
    //lwgpu->RegWr32(0x00088488, data);

    //-----------------------------------------------------
    // Setting up LW_XTL_EP_PRI_PWR_MGMT_CYA1(0x00091B10)
    //-----------------------------------------------------
    //L0S_ENABLE_ENABLED L1_ENABLE_ENABLED
    data = lwgpu->RegRd32(0x00091B10);
    //data = FLD_SET_DRF(_XTL_EP_PRI, _PRI_PWR_MGMT_CYA1, _CYA_L0S_ENABLE, _ENABLED, data);
    data = FLD_SET_DRF(_XTL_EP_PRI, _PWR_MGMT_CYA1, _PCIPM_L1_1_ENABLE, _INIT, data);
    lwgpu->RegWr32(0x00091B10, data);
 

    //-----------------------------------------------------
    // Setting up LW_XVE1_PRIV_XV_0(0x00000150)
    //-----------------------------------------------------
    //L0S_ENABLE_ENABLED L1_ENABLE_ENABLED
    if (xve_multi_func == 0x00800000) {
            data = lwgpu->RegRd32(0x8a000+LW_XVE1_PRIV_XV_0);
            data = data | 0x180;
            lwgpu->RegWr32(0x8a000+LW_XVE1_LINK_CONTROL_STATUS, data);
    }

    //-----------------------------------------------------
    // Setting up LW_PTRIM_SYS_IND_SYS_CORE_CLKSRC (0x00138fc0)
    //-----------------------------------------------------
    //LW_PTRIM_SYS_IND_SYS_CORE_CLKSRC_HOSTCLK_AUTO_ENABLE
    //No HOSTCLK in GH100
    //data = lwgpu->RegRd32(LW_PTRIM_SYS_IND_SYS_CORE_CLKSRC);
    //data = FLD_SET_DRF(_PTRIM, _SYS_IND_SYS_CORE_CLKSRC, _HOSTCLK_AUTO, _ENABLE, data);
    //lwgpu->RegWr32(LW_PTRIM_SYS_IND_SYS_CORE_CLKSRC, data);

    //-----------------------------------------------------
    // Enable wakeup from host (allowing pmu wakeup)
    //-----------------------------------------------------
    //  - enable  LW_XTL_EP_PRI_PRIV_XV_BLKCG2
    //data = lwgpu->RegRd32(0x00091418);
    //data = FLD_SET_DRF(_XTL_EP_PRI, _PRIV_XV_BLKCG2, _HOST2XV_HOST_IDLE_WAKE_EN, _ENABLED, data);
    //lwgpu->RegWr32(0x00091418, data);
    //  - disable LW_XVE_PRIV_XV_BLKCG2_UPSTREAM_REQ_WAKE_EN
    //  Removing this programming due to bug 200708002
    //data = lwgpu->RegRd32(0x00091418);
    //data = FLD_SET_DRF(_XTL_EP_PRI, _PRIV_XV_BLKCG2, _UPSTREAM_REQ_WAKE_EN, _DISABLED, data);
    //lwgpu->RegWr32(0x00091418, data);
 
    //-----------------------------------------------------
    // Setting up LW_XTL_EP_PRI_PWR_MGMT_CYA1 (0x00091B10)
    // Setting up LW_XTL_EP_PRI_PWR_MGMT_CYA2 (0x00091B14)
    //-----------------------------------------------------
    //  - Enable LW_XTL_EP_PRI_PWR_MGMT_CYA1/2_ASPM_L1_PLL_PD_ENABLE
    //  - Enable LW_XTL_EP_PRI_PWR_MGMT_CYA1/2_PCIPM_L1_PLL_PD_ENABLE
    //  - Enable LW_XTL_EP_PRI_PWR_MGMT_CYA1/2_CLOCK_PM_ENABLE
    data = lwgpu->RegRd32(0x00091B10);
    data = data | 0x190;
    lwgpu->RegWr32(0x00091B10, data);

    data = lwgpu->RegRd32(0x00091B14);
    data = data | 0x190;
    lwgpu->RegWr32(0x00091B14, data);
    
    //-----------------------------------------------------
    // Setting up LW_XPL_PL_PAD_CTL_PAD_PWRDN (0x00086300)
    //-----------------------------------------------------
    data = lwgpu->RegRd32(0x00086300);
    data = data & 0xf3fffff6;
    data = data | 0x0c000009;
    lwgpu->RegWr32(0x00086300, data);

    //-----------------------------------------------------
    // LW_EP_PCFG_GPU_CONTROL_AND_STATUS (0x00000070)  in dev_lw_xve.ref
    //  - Set LW_EP_PCFG_GPU_CONTROL_AND_STATUS (bit 1:0) to 2'b11.(L0s and L1 supported)
    //-----------------------------------------------------
    data = lwgpu->CfgRd32(LW_EP_PCFG_GPU_LINK_CONTROL_STATUS);
    //data = FLD_SET_DRF_NUM(_EP_PCFG_GPU, _LINK_CONTROL_STATUS, _ASPM_CTRL, 3, data);
    //data = FLD_SET_DRF_NUM(_EP_PCFG_GPU, _LINK_CONTROL_STATUS, _EXTENDED_SYNC, 0, data);
    lwgpu->CfgWr32(LW_EP_PCFG_GPU_LINK_CONTROL_STATUS, ((data & 0xffffff7f)| 0x3));



    //-----------------------------------------------------
    // LW_XVE1_LINK_CONTROL_STATUS in dev_lw_xve3g_fn1.ref
    //  - Set LW_XVE1_LINK_CONTROL_STATUS_ACTIVE_STATE_LINK_PM_CONTROL to 2 or 3.
    //-----------------------------------------------------
    if (xve_multi_func == 0x00800000) {
        data = lwgpu->RegRd32(0x8a000 + LW_XVE1_LINK_CONTROL_STATUS);
        data = (data & 0xfffffffc) | 0x3;
        lwgpu->RegWr32(0x8a000 + LW_XVE1_LINK_CONTROL_STATUS, data);
        InfoPrintf("Write LW_XVE1_LINK_CONTROL_STATUS_ACTIVE_STATE_LINK_PM_CONTROL to LW_XVE1_LINK_CONTROL_STATUS_ACTIVE_STATE_LINK_PM_CONTROL_L1_ENABLE_L0S_ENABLE!\n");
    }

    //-----------------------------------------------------
    // LW_XVE_LINK_CONTROL_STATUS in dev_lw_xve3g_fn0.ref
    //  -Set LW_XVE_LINK_CONTROL_STATUS_EXTENDED_SYNCH to 0
    //-----------------------------------------------------
    //data = lwgpu->RegRd32(0x88000+LW_XVE_LINK_CONTROL_STATUS);
    //data = data & 0xffffff7f;
    //lwgpu->RegWr32(0x88000+LW_XVE_LINK_CONTROL_STATUS, data);
    //InfoPrintf("Write LW_XVE_LINK_CONTROL_STATUS_EXTENDED_SYNCH to 0!\n");

    //-----------------------------------------------------
    // LW_XVE1_LINK_CONTROL_STATUS in dev_lw_xve3g_fn1.ref
    //  -Set LW_XVE1_LINK_CONTROL_STATUS_EXTENDED_SYNCH to 0
    //-----------------------------------------------------
    if (xve_multi_func == 0x00800000) {
        data = lwgpu->RegRd32(0x8a000+LW_XVE1_LINK_CONTROL_STATUS);
        data = data & 0xffffff7f;
        lwgpu->RegWr32(0x8a000+LW_XVE1_LINK_CONTROL_STATUS, data);
        InfoPrintf("Write LW_XVE1_LINK_CONTROL_STATUS_EXTENDED_SYNCH to 0!\n");
    }
    //pmu_xp2pmu_state_is_l1_1/2 are defeatured as per 200623594, hence replacing them with output of XP unit lw_x16_xpl_ltssm_state_is_l1_1/2
    UINT32 lw_x16_xpl_ltssm_state_is_l1_1 = 0;
    UINT32 lw_x16_xpl_ltssm_state_is_l1_2 = 0;
    if (pcipm_l1_2)
    {
        InfoPrintf("test pcipm_l1_2 Start !\n");

        //-----------------------------------------------------
        // Check L1 substate capability
        // LW_XVE_L1_PM_SUBSTATES_CAP in dev_lw_xve3g_fn0.ref
        //  - Check LW_XVE_L1_PM_SUBSTATES_CAP_L1_PM_SUBSTATES_SUPPORTED, LW_XVE_L1_PM_SUBSTATES_CAP_PCI_PM_L1_2_SUPPORTED
        //-----------------------------------------------------
        InfoPrintf("Check L1 PM Substate Capability.\n");
        data = lwgpu->CfgRd32(0x02000094);
        if ((data & 0x11) != 0x11)
        {
            InfoPrintf("ERROR!! L1 PM Substates capability is not supported.\n");
        }
        else
        {
          InfoPrintf("L1 PM Substates capability is supported.\n");
        }
        
        //Disabling ASPM for PCIPM tests, else DUT tries to enter into ASPM state when there is idleness, sometimes gets PME_NAK message during end of sim, leading to assertion error
        data = lwgpu->CfgRd32(LW_EP_PCFG_GPU_LINK_CONTROL_STATUS);
        //data = FLD_SET_DRF_NUM(_EP_PCFG_GPU, _LINK_CONTROL_STATUS, _ASPM_CTRL, 0, data);
        lwgpu->CfgWr32(LW_EP_PCFG_GPU_LINK_CONTROL_STATUS, (data & 0xfffffffc));

        //-----------------------------------------------------
        // Put device to L1.2 state
        // LW_XVE_PWR_MGMT_1 in dev_lw_xve3g_fn0.ref
        //  - Set LW_XVE_L1_PM_SUBSTATES_CTRL1_PCIPM_L1_2_EN and other Substate Enables to 0 .
        //-----------------------------------------------------
        data = lwgpu->CfgRd32(0x02000098);
        data = FLD_SET_DRF(_EP_PCFG_GPU,_L1_PM_SS_CONTROL_1_REGISTER,_PCIPM_L12_ENABLE,_ENABLE,data);
        lwgpu->CfgWr32(0x02000098, data);
        InfoPrintf("Write PCIPM_L1_2_EN to 1!\n");

        //-----------------------------------------------------
        // Put device to D3hot state
        // LW_XVE_PWR_MGMT_1 in dev_lw_xve.ref
        //  - Set LW_XVE_PWR_MGMT_1_PWR_STATE to D3HOT.
        //-----------------------------------------------------
        data = lwgpu->CfgRd32(LW_EP_PCFG_GPU_CONTROL_AND_STATUS);
        data = FLD_SET_DRF(_EP_PCFG_GPU, _CONTROL_AND_STATUS, _POWER_STATE, _D3, data);
        lwgpu->CfgWr32(LW_EP_PCFG_GPU_CONTROL_AND_STATUS, data);
 

        //-----------------------------------------------------
        // Put device to D3hot state
        // LW_XVE1_PWR_MGMT_1 in dev_lw_xve1.ref
        //  - Set LW_XVE1_PWR_MGMT_1_PWR_STATE to D3hot.
        //-----------------------------------------------------
        if (xve_multi_func == 0x00800000) {
            data = lwgpu->RegRd32(0x8a000+LW_XVE1_PWR_MGMT_1);
            data = (data & 0xfffffffc) | 0x3;
            lwgpu->RegWr32(0x8a000+LW_XVE1_PWR_MGMT_1, data);
            InfoPrintf("Put device into D3hot!\n");
        }

        //-----------------------------------------------------
        //Put device to D3hot state
        // LW_XVE_XUSB_PWR_MGMT_1 in dev_lw_xve.ref
        //  - Set LW_XVE_XUSB_PWR_MGMT_1_PWR_STATE to D3HOT.
        //----------------------------------------------------- 
        if( (xusb_present == 0x80000000) && (ppc_present == 0x80000000) ){
            Platform::PciWrite32(pci_domain_num,
                               pci_bus_num,
                               pci_dev_num,
                               2, // xusb fn num
                               LW_XVE_XUSB_PWR_MGMT_1, //offset is same for all functions
                               DRF_NUM(_XVE_XUSB, _PWR_MGMT_1, _PWR_STATE, 3));
            
            //-----------------------------------------------------
            // Put device to D3hot state
            // LW_XVE_PPC_PWR_MGMT_1 in dev_lw_xve.ref
            //  - Set LW_XVE_PPC_PWR_MGMT_1_PWR_STATE to D3HOT.
            //----------------------------------------------------- 
            Platform::PciWrite32(pci_domain_num,
                               pci_bus_num,
                               pci_dev_num,
                               3, // xusb fn num
                               LW_XVE_PPC_PWR_MGMT_1, //offset is same for all functions
                               DRF_NUM(_XVE_PPC, _PWR_MGMT_1, _PWR_STATE, 3));
        }
        //-----------------------------------------------------
        // Delay long enough to enter L1.2
        // During the period, pmu will issue priv access thru bar0 interface of host
        //-----------------------------------------------------
        if(selfcheck_EscapeRead)
        {
            InfoPrintf("Check whether goes to PDN_WAKE_UP_EVENT_WAIT state\n");
            if (Checkstate(0x40000)) return;

             //  - enable LW_XVE_PEX_PLL5_UPSTREAM_REQ_WAKE_EN (which is disabled by default)
            //data = lwgpu->RegRd32(0x00091418);
            //data = FLD_SET_DRF(_XTL_EP_PRI, _PRIV_XV_BLKCG2, _UPSTREAM_REQ_WAKE_EN, _ENABLED, data);
            //lwgpu->RegWr32(0x00091418, data);


            InfoPrintf("Check whether revert to SEQ_IDLE state\n");
            if (Checkstate(0x1)) return;
        }

        InfoPrintf("Waiting for delay 100 \n");
        Platform::Delay(100);
        InfoPrintf("Done waiting for delay 100, expecting to enter L1.2\n");

        int time_count = 0;
        bool state_matched = 0;

        while ( (time_count < 100) && !state_matched)
        {
            Platform::EscapeRead("lw_x16_xpl_ltssm_state_is_l1_1",0,1,&lw_x16_xpl_ltssm_state_is_l1_1);
            Platform::EscapeRead("lw_x16_xpl_ltssm_state_is_l1_2",0,1,&lw_x16_xpl_ltssm_state_is_l1_2);
            if ((lw_x16_xpl_ltssm_state_is_l1_1 == 0x0) && (lw_x16_xpl_ltssm_state_is_l1_2 == 0x1))
            {
                state_matched = 1;
            }
            else
            {
               time_count++;
            }
        }
        if (!state_matched)
        {
             SetStatus(TEST_FAILED);
             getStateReport()->runFailed("Failed check L1 substate.\n");
             ErrPrintf("Priv read addr 0x0010a040(LW_PPWR_PMU_GPIO_1_INPUT_XP2PMU_STATE_IS_L1_2, LW_PPWR_PMU_GPIO_1_INPUT_XP2PMU_STATE_IS_L1_1), got data = %x\n", data);
             return;
        }
        else
        {
            InfoPrintf("Priv read addr 0x0010a040(LW_PPWR_PMU_GPIO_1_INPUT_XP2PMU_STATE_IS_L1_2, LW_PPWR_PMU_GPIO_1_INPUT_XP2PMU_STATE_IS_L1_1), got data = %x\n", data);
        }

        if (bfm_initiate_exit)
        {
            InfoPrintf("BFM initiate exit");
        }
        else
        {
           //-----------------------------------------------------
           // Initiate Exit L1 -- go to recovery
           // LW_XP3G_PL_LINK_CONFIG_0 in dev_lw_xp3g.ref
           //  - Set LW_XP3G_PL_LINK_CONFIG_0_LTSSM_DIRECTIVE set to LW_XP3G_PL_LINK_CONFIG_0_LTSSM_DIRECTIVE_GOTO_RECOVERY (0xc)
           //-----------------------------------------------------
           data = lwgpu->RegRd32(0x00085008);
           data = FLD_SET_DRF(_XPL, _PL_LTSSM_LINK_CONFIG_0, _LTSSM_DIRECTIVE,_GOTO_RECOVERY, data);
           lwgpu->RegWr32(0x00085008, data);
           InfoPrintf("Write LW_XP3G_PL_LINK_CONFIG_0_LTSSM_DIRECTIVE to _GOTO_RECOVERY!\n");
        }
        //-----------------------------------------------------
        // LW_XVE_PWR_MGMT_1 in dev_lw_xve3g_fn0.ref
        //  - Set LW_XVE_L1_PM_SUBSTATES_CTRL1_PCIPM_L1_2_EN to 0
        //-----------------------------------------------------
        data = lwgpu->CfgRd32(0x02000098);
        data = FLD_SET_DRF(_EP_PCFG_GPU,_L1_PM_SS_CONTROL_1_REGISTER,_PCIPM_L12_ENABLE,_DISABLE,data);
        lwgpu->CfgWr32(0x02000098, data);
        InfoPrintf("Write PCIPM_L1_2_EN to 0!\n");

        //-----------------------------------------------------
        // Put device to D0 state
        // LW_XVE_PWR_MGMT_1 in dev_lw_xve.ref
        //  - Set LW_XVE_PWR_MGMT_1_PWR_STATE to D0.
        //-----------------------------------------------------
        data = lwgpu->CfgRd32(LW_EP_PCFG_GPU_CONTROL_AND_STATUS);
        data = FLD_SET_DRF(_EP_PCFG_GPU, _CONTROL_AND_STATUS, _POWER_STATE, _D0, data);
        lwgpu->CfgWr32(LW_EP_PCFG_GPU_CONTROL_AND_STATUS, data);
        InfoPrintf("Put device into D0!\n");

        //-----------------------------------------------------
        // Put device to D0 state
        // LW_XVE1_PWR_MGMT_1 in dev_lw_xve1.ref
        //  - Set LW_XVE1_PWR_MGMT_1_PWR_STATE to D0.
        //-----------------------------------------------------
        if (xve_multi_func == 0x00800000) {
            data = lwgpu->RegRd32(0x8a000+LW_XVE1_PWR_MGMT_1);
            data = (data & 0xfffffffc) | 0x0;
            lwgpu->RegWr32(0x8a000+LW_XVE1_PWR_MGMT_1, data);
            InfoPrintf("Put device into D0!\n");
        }

        //-----------------------------------------------------
        //Put device to D0 state
        // LW_XVE_XUSB_PWR_MGMT_1 in dev_lw_xve.ref
        //  - Set LW_XVE_XUSB_PWR_MGMT_1_PWR_STATE to D0.
        //-----------------------------------------------------
        if( (xusb_present == 0x80000000) && (ppc_present == 0x80000000) ){
            Platform::PciWrite32(pci_domain_num,
                               pci_bus_num,
                               pci_dev_num,
                               2, // xusb fn num
                               LW_XVE_XUSB_PWR_MGMT_1, //offset is same for all functions
                               DRF_NUM(_XVE_XUSB, _PWR_MGMT_1, _PWR_STATE, 0));
            
            //-----------------------------------------------------
            // Put device to D0 state
            // LW_XVE_PPC_PWR_MGMT_1 in dev_lw_xve.ref
            //  - Set LW_XVE_PPC_PWR_MGMT_1_PWR_STATE to D0.
            //----------------------------------------------------- 
            Platform::PciWrite32(pci_domain_num,
                               pci_bus_num,
                               pci_dev_num,
                               3, // xusb fn num
                               LW_XVE_PPC_PWR_MGMT_1, //offset is same for all functions
                               DRF_NUM(_XVE_PPC, _PWR_MGMT_1, _PWR_STATE, 0));
        }
        
        InfoPrintf("Now make sure priv operation works - device resumed ok!\n");
        data = 0x5a5a;
        lwgpu->RegWr32(0x0010a040, data);
        InfoPrintf("Priv write addr 0x0010a040(LW_PPWR_FALCON_MAILBOX0), data = %x\n", data);

        InfoPrintf("test L1_substate pcipm_l1_2() Done !\n");
    } //endif_pcipm_l1_2

    if (pcipm_l1_1)
    {
        InfoPrintf("test pcipm_l1_1 Start !\n");

        //-----------------------------------------------------
        // Check L1 substate capability
        // LW_XVE_L1_PM_SUBSTATES_CAP in dev_lw_xve3g_fn0.ref
        //  - Check LW_XVE_L1_PM_SUBSTATES_CAP_L1_PM_SUBSTATES_SUPPORTED, LW_XVE_L1_PM_SUBSTATES_CAP_PCI_PM_L1_1_SUPPORTED
        //-----------------------------------------------------
        InfoPrintf("Check L1 PM Substate Capability.\n");
        data = lwgpu->CfgRd32(0x02000094);
        if ((data & 0x12) != 0x12)
        {
            InfoPrintf("ERROR!! L1 PM Substates capability is not supported.\n");
        }
        else
        {
            InfoPrintf("L1 PM Substates capability is supported.\n");
        }

        //Disabling ASPM for PCIPM tests, else DUT tries to enter into ASPM state when there is idleness, sometimes gets PME_NAK message during end of sim, leading to assertion error
        data = lwgpu->CfgRd32(LW_EP_PCFG_GPU_LINK_CONTROL_STATUS);
        //data = FLD_SET_DRF_NUM(_EP_PCFG_GPU, _LINK_CONTROL_STATUS, _ASPM_CTRL, 0, data);
        lwgpu->CfgWr32(LW_EP_PCFG_GPU_LINK_CONTROL_STATUS, (data & 0xfffffffc));

        //-----------------------------------------------------
        // Put device to L1.1 state
        // LW_XVE_PWR_MGMT_1 in dev_lw_xve3g_fn0.ref
        //  - Set LW_XVE_L1_PM_SUBSTATES_CTRL1_PCIPM_L1_1_EN and other Substate Enables to 0 .
        //-----------------------------------------------------
        data = lwgpu->CfgRd32(0x02000098);
        data = FLD_SET_DRF(_EP_PCFG_GPU,_L1_PM_SS_CONTROL_1_REGISTER,_PCIPM_L11_ENABLE,_ENABLE,data);
        lwgpu->CfgWr32(0x02000098, data);
        InfoPrintf("Write PCIPM_L1_1_EN to 1!\n");

        //-----------------------------------------------------
        // Put device to D3hot state
        // LW_XVE_PWR_MGMT_1 in dev_lw_xve.ref
        //  - Set LW_XVE_PWR_MGMT_1_PWR_STATE to D3HOT.
        //-----------------------------------------------------
        data = lwgpu->CfgRd32(LW_EP_PCFG_GPU_CONTROL_AND_STATUS);
        data = FLD_SET_DRF(_EP_PCFG_GPU, _CONTROL_AND_STATUS, _POWER_STATE, _D3, data);
        lwgpu->CfgWr32(LW_EP_PCFG_GPU_CONTROL_AND_STATUS, data);
        InfoPrintf("Put device into D3HOT!\n");
 
        //-----------------------------------------------------
        // Put device to D3hot state
        // LW_XVE1_PWR_MGMT_1 in dev_lw_xve1.ref
        //  - Set LW_XVE1_PWR_MGMT_1_PWR_STATE to D3hot.
        //-----------------------------------------------------
        if (xve_multi_func == 0x00800000) {
            data = lwgpu->RegRd32(0x8a000+LW_XVE1_PWR_MGMT_1);
            data = (data & 0xfffffffc) | 0x3;
            lwgpu->RegWr32(0x8a000+LW_XVE1_PWR_MGMT_1, data);
            InfoPrintf("Put device into D3hot!\n");
        }

        //-----------------------------------------------------
        //Put device to D3hot state
        // LW_XVE_XUSB_PWR_MGMT_1 in dev_lw_xve.ref
        //  - Set LW_XVE_XUSB_PWR_MGMT_1_PWR_STATE to D3HOT.
        //----------------------------------------------------- 
        if( (xusb_present == 0x80000000) && (ppc_present == 0x80000000) ){
            Platform::PciWrite32(pci_domain_num,
                               pci_bus_num,
                               pci_dev_num,
                               2, // xusb fn num
                               LW_XVE_XUSB_PWR_MGMT_1, //offset is same for all functions
                               DRF_NUM(_XVE_XUSB, _PWR_MGMT_1, _PWR_STATE, 3));
            
            //-----------------------------------------------------
            // Put device to D3hot state
            // LW_XVE_PPC_PWR_MGMT_1 in dev_lw_xve.ref
            //  - Set LW_XVE_PPC_PWR_MGMT_1_PWR_STATE to D3HOT.
            //----------------------------------------------------- 
            Platform::PciWrite32(pci_domain_num,
                               pci_bus_num,
                               pci_dev_num,
                               3, // xusb fn num
                               LW_XVE_PPC_PWR_MGMT_1, //offset is same for all functions
                               DRF_NUM(_XVE_PPC, _PWR_MGMT_1, _PWR_STATE, 3));
        }    
        //-----------------------------------------------------
        // Delay long enough to enter L1.1
        // During the period, pmu will issue priv access thru bar0 interface of host
        //-----------------------------------------------------
        if(selfcheck_EscapeRead)
        {
            InfoPrintf("Check whether goes to PDN_WAKE_UP_EVENT_WAIT state\n");
            if (Checkstate(0x40000)) return;

            //  - enable LW_XVE_PEX_PLL5_UPSTREAM_REQ_WAKE_EN (which is disabled by default)
            //data = lwgpu->RegRd32(0x00091418);
            //data = FLD_SET_DRF(_XTL_EP_PRI, _PRIV_XV_BLKCG2, _UPSTREAM_REQ_WAKE_EN, _ENABLED, data);
            //lwgpu->RegWr32(0x00091418, data);

            InfoPrintf("Check whether revert to SEQ_IDLE state\n");
            if (Checkstate(0x1)) return;
        }

        InfoPrintf("Waiting for delay 100 \n");
        Platform::Delay(100);
        InfoPrintf("Done waiting for delay 100, expecting to enter L1.1\n");

        int time_count = 0;
        bool state_matched = 0;
        while ( (time_count < 100) && !state_matched)
        {
            Platform::EscapeRead("lw_x16_xpl_ltssm_state_is_l1_1",0,1,&lw_x16_xpl_ltssm_state_is_l1_1);
            Platform::EscapeRead("lw_x16_xpl_ltssm_state_is_l1_2",0,1,&lw_x16_xpl_ltssm_state_is_l1_2);
            if ((lw_x16_xpl_ltssm_state_is_l1_1 == 0x1) && (lw_x16_xpl_ltssm_state_is_l1_2 == 0x0))
            {
                state_matched = 1;
            }
            else
            {
                time_count++;
            }
        }
        if (!state_matched)
        {
            SetStatus(TEST_FAILED);
            getStateReport()->runFailed("Failed check L1 substate.\n");
            ErrPrintf("Priv read addr 0x0010a040(LW_PPWR_PMU_GPIO_1_INPUT_XP2PMU_STATE_IS_L1_2, LW_PPWR_PMU_GPIO_1_INPUT_XP2PMU_STATE_IS_L1_1), got data = %x\n", data);
            return;
        }
        else
        {
            InfoPrintf("Priv read addr 0x0010a040(LW_PPWR_PMU_GPIO_1_INPUT_XP2PMU_STATE_IS_L1_2, LW_PPWR_PMU_GPIO_1_INPUT_XP2PMU_STATE_IS_L1_1), got data = %x\n", data);
        }

        if (bfm_initiate_exit)
        {
            InfoPrintf("BFM initiate exit");
        }
        else
        {
            //-----------------------------------------------------
            // Initiate Exit L1 -- go to recovery
            // LW_XP3G_PL_LINK_CONFIG_0 in dev_lw_xp3g.ref
            //  - Set LW_XP3G_PL_LINK_CONFIG_0_LTSSM_DIRECTIVE set to LW_XP3G_PL_LINK_CONFIG_0_LTSSM_DIRECTIVE_GOTO_RECOVERY (0xc)
            //-----------------------------------------------------
            data = lwgpu->RegRd32(0x00085008);
            data = FLD_SET_DRF(_XPL, _PL_LTSSM_LINK_CONFIG_0, _LTSSM_DIRECTIVE,_GOTO_RECOVERY, data);
            lwgpu->RegWr32(0x00085008, data);
            InfoPrintf("Write LW_XP3G_PL_LINK_CONFIG_0_LTSSM_DIRECTIVE to _GOTO_RECOVERY!\n");

        }

        //-----------------------------------------------------
        // LW_XVE_PWR_MGMT_1 in dev_lw_xve3g_fn0.ref
        //  - Set LW_XVE_L1_PM_SUBSTATES_CTRL1_PCIPM_L1_1_EN to 0
        //-----------------------------------------------------
        data = lwgpu->CfgRd32(0x02000098);
        data = FLD_SET_DRF(_EP_PCFG_GPU,_L1_PM_SS_CONTROL_1_REGISTER,_PCIPM_L11_ENABLE,_DISABLE,data);
        lwgpu->CfgWr32(0x02000098, data);
        InfoPrintf("Write PCIPM_L1_1_EN to 0!\n");

        //-----------------------------------------------------
        // Put device to D0 state
        // LW_XVE_PWR_MGMT_1 in dev_lw_xve.ref
        //  - Set LW_XVE_PWR_MGMT_1_PWR_STATE to D0.
        //-----------------------------------------------------
        data = lwgpu->CfgRd32(LW_EP_PCFG_GPU_CONTROL_AND_STATUS);
        data = FLD_SET_DRF(_EP_PCFG_GPU, _CONTROL_AND_STATUS, _POWER_STATE, _D0, data);
        lwgpu->CfgWr32(LW_EP_PCFG_GPU_CONTROL_AND_STATUS, data);
        InfoPrintf("Put device into D0!\n");


        //-----------------------------------------------------
        // Put device to D0 state
        // LW_XVE1_PWR_MGMT_1 in dev_lw_xve1.ref
        //  - Set LW_XVE1_PWR_MGMT_1_PWR_STATE to D0.
        //-----------------------------------------------------
        if (xve_multi_func == 0x00800000) {
            data = lwgpu->RegRd32(0x8a000+LW_XVE1_PWR_MGMT_1);
            data = (data & 0xfffffffc) | 0x0;
            lwgpu->RegWr32(0x8a000+LW_XVE1_PWR_MGMT_1, data);
            InfoPrintf("Put device into D0!\n");
        }

         //-----------------------------------------------------
        //Put device to D0 state
        // LW_XVE_XUSB_PWR_MGMT_1 in dev_lw_xve.ref
        //  - Set LW_XVE_XUSB_PWR_MGMT_1_PWR_STATE to D0.
        //----------------------------------------------------- 
        if( (xusb_present == 0x80000000) && (ppc_present == 0x80000000) ){ 
            Platform::PciWrite32(pci_domain_num,
                               pci_bus_num,
                               pci_dev_num,
                               2, // xusb fn num
                               LW_XVE_XUSB_PWR_MGMT_1, //offset is same for all functions
                               DRF_NUM(_XVE_XUSB, _PWR_MGMT_1, _PWR_STATE, 0));
            
            //-----------------------------------------------------
            // Put device to D0 state
            // LW_XVE_PPC_PWR_MGMT_1 in dev_lw_xve.ref
            //  - Set LW_XVE_PPC_PWR_MGMT_1_PWR_STATE to D0.
            //----------------------------------------------------- 
            Platform::PciWrite32(pci_domain_num,
                               pci_bus_num,
                               pci_dev_num,
                               3, // xusb fn num
                               LW_XVE_PPC_PWR_MGMT_1, //offset is same for all functions
                               DRF_NUM(_XVE_PPC, _PWR_MGMT_1, _PWR_STATE, 0));
        }

        InfoPrintf("Now make sure priv operation works - device resumed ok!\n");
        data = 0x5a5a;
        lwgpu->RegWr32(0x0010a040, data);
        InfoPrintf("Priv write addr 0x0010a040(LW_PPWR_FALCON_MAILBOX0), data = %x\n", data);

        InfoPrintf("test L1_substate pcipm_l1_1() Done !\n");
    } //endif_pcipm_l1_1

    if (aspm_l1_2)
    {
        InfoPrintf("test aspm_l1_2 Start !\n");

        //-----------------------------------------------------
        // Check L1 substate capability
        // LW_XVE_L1_PM_SUBSTATES_CAP in dev_lw_xve3g_fn0.ref
        //  - Check LW_XVE_L1_PM_SUBSTATES_CAP_L1_PM_SUBSTATES_SUPPORTED, LW_XVE_L1_PM_SUBSTATES_CAP_ASPM_PM_L1_2_SUPPORTED
        //-----------------------------------------------------
        InfoPrintf("Check L1 PM Substate Capability.\n");
        data = lwgpu->CfgRd32(0x02000094);
        if ((data & 0x14) != 0x14)
        {
            InfoPrintf("ERROR!! L1 PM Substates capability is not supported.\n");
        }
        else
        {
            InfoPrintf("L1 PM Substates capability is supported.\n");
        }

        

        //-----------------------------------------------------
        // Put device to L1.2 state
        // LW_XVE_PWR_MGMT_1 in dev_lw_xve3g_fn0.ref
        //  - Set LW_XVE_L1_PM_SUBSTATES_CTRL1_PCIPM_L1_2_EN and other Substate Enables to 0 .
        //-----------------------------------------------------
        data = lwgpu->CfgRd32(0x02000098);
        data = FLD_SET_DRF(_EP_PCFG_GPU,_L1_PM_SS_CONTROL_1_REGISTER,_ASPM_L12_ENABLE,_ENABLE,data);
        lwgpu->CfgWr32(0x02000098, data);
        InfoPrintf("Write ASPM_L1_2_EN to 1!\n");

        //-----------------------------------------------------
        // Put device to D0 state
        // LW_XVE_PWR_MGMT_1 in dev_lw_xve.ref
        //  - Set LW_XVE_PWR_MGMT_1_PWR_STATE to D0.
        //-----------------------------------------------------
        data = lwgpu->CfgRd32(LW_EP_PCFG_GPU_CONTROL_AND_STATUS);
        data = FLD_SET_DRF(_EP_PCFG_GPU, _CONTROL_AND_STATUS, _POWER_STATE, _D0, data);
        lwgpu->CfgWr32(LW_EP_PCFG_GPU_CONTROL_AND_STATUS, data);
        InfoPrintf("Put device into D0!\n");

        //-----------------------------------------------------
        // Put device to D0 state
        // LW_XVE1_PWR_MGMT_1 in dev_lw_xve1.ref
        //  - Set LW_XVE1_PWR_MGMT_1_PWR_STATE to D0.
        //-----------------------------------------------------
        if (xve_multi_func == 0x00800000) {
            data = lwgpu->RegRd32(0x8a000+LW_XVE1_PWR_MGMT_1);
            data = (data & 0xfffffffc) | 0x0;
            lwgpu->RegWr32(0x8a000+LW_XVE1_PWR_MGMT_1, data);
            InfoPrintf("Put device into D0!\n");
        }

         //-----------------------------------------------------
        //Put device to D0 state
        // LW_XVE_XUSB_PWR_MGMT_1 in dev_lw_xve.ref
        //  - Set LW_XVE_XUSB_PWR_MGMT_1_PWR_STATE to D0.
        //-----------------------------------------------------
        if( (xusb_present == 0x80000000) && (ppc_present == 0x80000000) ){
            Platform::PciWrite32(pci_domain_num,
                               pci_bus_num,
                               pci_dev_num,
                               2, // xusb fn num
                               LW_XVE_XUSB_PWR_MGMT_1, //offset is same for all functions
                               DRF_NUM(_XVE_XUSB, _PWR_MGMT_1, _PWR_STATE, 0));
            
            //-----------------------------------------------------
            // Put device to D0 state
            // LW_XVE_PPC_PWR_MGMT_1 in dev_lw_xve.ref
            //  - Set LW_XVE_PPC_PWR_MGMT_1_PWR_STATE to D0.
            //----------------------------------------------------- 
            Platform::PciWrite32(pci_domain_num,
                               pci_bus_num,
                               pci_dev_num,
                               3, // xusb fn num
                               LW_XVE_PPC_PWR_MGMT_1, //offset is same for all functions
                               DRF_NUM(_XVE_PPC, _PWR_MGMT_1, _PWR_STATE, 0));
        }

        //-----------------------------------------------------
        // Set LW_XVE_DEVICE_CONTROL_STATUS_2_LTR_ENABLE to 1
        // LW_XVE_DEVICE_CONTROL_STATUS_2 in dev_lw_xve3g_fn0.ref
        //-----------------------------------------------------
        data = lwgpu->CfgRd32(LW_EP_PCFG_GPU_DEVICE_CONTROL_STATUS_2);
        //data = FLD_SET_DRF(_EP_PCFG_GPU,_DEVICE_CONTROL_STATUS_2,_LTR_ENABLE,_ENABLE);
        data = (data & 0xfffffbff) | 0x400;
        lwgpu->CfgWr32(LW_EP_PCFG_GPU_DEVICE_CONTROL_STATUS_2, data);
        InfoPrintf("Write LW_EP_PCFG_GPU_DEVICE_CONTROL_STATUS_2_LTR_ENABLE to 1!\n");

        //-----------------------------------------------------
        // Set LW_XVE1_DEVICE_CONTROL_STATUS_2_LTR_ENABLE to 1
        // LW_XVE1_DEVICE_CONTROL_STATUS_2 in dev_lw_xve3g_fn1.ref
        //-----------------------------------------------------
        if (xve_multi_func == 0x00800000) {
            data = lwgpu->RegRd32(0x8a000+LW_XVE1_DEVICE_CONTROL_STATUS_2);
            data = (data & 0xfffffbff) | 0x400;
            lwgpu->RegWr32(0x8a000+LW_XVE1_DEVICE_CONTROL_STATUS_2, data);
            InfoPrintf("Write LW_XVE1_DEVICE_CONTROL_STATUS_2_LTR_ENABLE to 1!\n");
        }

        //-----------------------------------------------------
        // Delay long enough to enter ASPM L1.2
        // During the period, pmu will issue priv access thru bar0 interface of host
        //-----------------------------------------------------
        if(selfcheck_EscapeRead)
        {
            InfoPrintf("Check whether goes to PDN_WAKE_UP_EVENT_WAIT state\n");
            if (Checkstate(0x40000)) return;

            //  - enable LW_XVE_PEX_PLL5_UPSTREAM_REQ_WAKE_EN (which is disabled by default)
            //data = lwgpu->RegRd32(0x00091418);
            //data = FLD_SET_DRF(_XTL_EP_PRI, _PRIV_XV_BLKCG2, _UPSTREAM_REQ_WAKE_EN, _ENABLED, data);
            //lwgpu->RegWr32(0x00091418, data);

            InfoPrintf("Check whether revert to SEQ_IDLE state\n");
            if (Checkstate(0x1)) return;
        }

        InfoPrintf("Waiting for delay 100 \n");
        Platform::Delay(100);
        InfoPrintf("Done waiting for delay 100, expecting to enter L1.2\n");

        int time_count = 0;
        bool state_matched = 0;
        while ( (time_count < 100) && !state_matched)
        {
            Platform::EscapeRead("lw_x16_xpl_ltssm_state_is_l1_1",0,1,&lw_x16_xpl_ltssm_state_is_l1_1);
            Platform::EscapeRead("lw_x16_xpl_ltssm_state_is_l1_2",0,1,&lw_x16_xpl_ltssm_state_is_l1_2);
            if ((lw_x16_xpl_ltssm_state_is_l1_1 == 0x0) && (lw_x16_xpl_ltssm_state_is_l1_2 == 0x1))
            {
                state_matched = 1;
            }
            else
            {
                time_count++;
            }
        }
        if (!state_matched)
        {
            SetStatus(TEST_FAILED);
            getStateReport()->runFailed("Failed check L1 substate.\n");
            ErrPrintf("Priv read addr 0x0010a040(LW_PPWR_PMU_GPIO_1_INPUT_XP2PMU_STATE_IS_L1_2, LW_PPWR_PMU_GPIO_1_INPUT_XP2PMU_STATE_IS_L1_1), got data = %x\n", data);
            return;
        }
        else
        {
            InfoPrintf("Priv read addr 0x0010a040(LW_PPWR_PMU_GPIO_1_INPUT_XP2PMU_STATE_IS_L1_2, LW_PPWR_PMU_GPIO_1_INPUT_XP2PMU_STATE_IS_L1_1), got data = %x\n", data);
        }

        if (bfm_initiate_exit)
        {
            InfoPrintf("BFM initiate exit");
        }
        else
        {
            //-----------------------------------------------------
            // Initiate Exit L1 -- go to recovery
            // LW_XP3G_PL_LINK_CONFIG_0 in dev_lw_xp3g.ref
            //  - Set LW_XP3G_PL_LINK_CONFIG_0_LTSSM_DIRECTIVE set to LW_XP3G_PL_LINK_CONFIG_0_LTSSM_DIRECTIVE_GOTO_RECOVERY (0xc)
            //-----------------------------------------------------
            data = lwgpu->RegRd32(0x00085008);
            data = FLD_SET_DRF(_XPL, _PL_LTSSM_LINK_CONFIG_0, _LTSSM_DIRECTIVE,_GOTO_RECOVERY, data);
            lwgpu->RegWr32(0x00085008, data);
            InfoPrintf("Write LW_XP3G_PL_LINK_CONFIG_0_LTSSM_DIRECTIVE to _GOTO_RECOVERY!\n");
        }

        InfoPrintf("Now make sure priv operation works - device resumed ok!\n");
        data = 0x5a5a;
        lwgpu->RegWr32(0x0010a040, data);
        InfoPrintf("Priv write addr 0x0010a040(LW_PPWR_FALCON_MAILBOX0), data = %x\n", data);

        //-----------------------------------------------------
        // Set LW_XVE_DEVICE_CONTROL_STATUS_2_LTR_ENABLE back to 0
        // LW_XVE_DEVICE_CONTROL_STATUS_2 in dev_lw_xve3g_fn0.ref
        //----------------------------------------------------
        data = lwgpu->CfgRd32(LW_EP_PCFG_GPU_DEVICE_CONTROL_STATUS_2);
        data = FLD_SET_DRF(_EP_PCFG_GPU,_DEVICE_CONTROL_STATUS_2,_LTR_ENABLE,_DEFAULT,data);
        lwgpu->CfgWr32(LW_EP_PCFG_GPU_DEVICE_CONTROL_STATUS_2, data);
        InfoPrintf("Write LW_EP_PCFG_GPU_DEVICE_CONTROL_STATUS_2_LTR_ENABLE to 0!\n");
        

        //-----------------------------------------------------
        // LW_XVE_PWR_MGMT_1 in dev_lw_xve3g_fn0.ref
        //  -Restore LW_XVE_L1_PM_SUBSTATES_CTRL1_ASPM_L1_2_EN back to 0.
        //-----------------------------------------------------
        data = lwgpu->CfgRd32(0x02000098);
        data = FLD_SET_DRF(_EP_PCFG_GPU,_L1_PM_SS_CONTROL_1_REGISTER,_ASPM_L12_ENABLE,_DISABLE,data);
         lwgpu->CfgWr32(0x02000098, data);
        InfoPrintf("Restore ASPM_L1_2_EN back to 0!\n");

        InfoPrintf("test L1_substate aspm_l1_2() Done !\n");
    } //endif_aspm_l1_2

    if (aspm_l1_1)
    {
        InfoPrintf("test aspm_l1_1 Start !\n");

        //-----------------------------------------------------
        // Check L1 substate capability
        // LW_XVE_L1_PM_SUBSTATES_CAP in dev_lw_xve3g_fn0.ref
        //  - Check LW_XVE_L1_PM_SUBSTATES_CAP_L1_PM_SUBSTATES_SUPPORTED, LW_XVE_L1_PM_SUBSTATES_CAP_ASPM_PM_L1_1_SUPPORTED
        //-----------------------------------------------------
        InfoPrintf("Check L1 PM Substate Capability.\n");
        data = lwgpu->CfgRd32(0x02000094);
        if ((data & 0x18) != 0x18)
        {
          InfoPrintf("ERROR!! L1 PM Substates capability is not supported.\n");
        }
        else
        {
          InfoPrintf("L1 PM Substates capability is supported.\n");
        }

        //-----------------------------------------------------
        // Put device to L1.1 state
        // LW_XVE_PWR_MGMT_1 in dev_lw_xve3g_fn0.ref
        //  - Set LW_XVE_L1_PM_SUBSTATES_CTRL1_ASPM_L1_1_EN to 1 and other Substate Enables to 0.
        //-----------------------------------------------------
        data = lwgpu->CfgRd32(0x02000098);
        data = FLD_SET_DRF(_EP_PCFG_GPU,_L1_PM_SS_CONTROL_1_REGISTER,_ASPM_L11_ENABLE,_ENABLE,data);
        lwgpu->CfgWr32(0x02000098, data);
        InfoPrintf("Write PCIPM_L1_1_EN to 1!\n"); 

        //-----------------------------------------------------
        // Put device to D0 state
        // LW_XVE_PWR_MGMT_1 in dev_lw_xve.ref
        //  - Set LW_XVE_PWR_MGMT_1_PWR_STATE to D0.
        //-----------------------------------------------------
        data = lwgpu->CfgRd32(LW_EP_PCFG_GPU_CONTROL_AND_STATUS);
        data = FLD_SET_DRF(_EP_PCFG_GPU, _CONTROL_AND_STATUS, _POWER_STATE, _D0, data);
        lwgpu->CfgWr32(LW_EP_PCFG_GPU_CONTROL_AND_STATUS, data);


        //-----------------------------------------------------
        // Put device to D0 state
        // LW_XVE1_PWR_MGMT_1 in dev_lw_xve1.ref
        //  - Set LW_XVE1_PWR_MGMT_1_PWR_STATE to D0.
        //-----------------------------------------------------
        if (xve_multi_func == 0x00800000) {
            data = lwgpu->RegRd32(0x8a000+LW_XVE1_PWR_MGMT_1);
            data = (data & 0xfffffffc) | 0x0;
            lwgpu->RegWr32(0x8a000+LW_XVE1_PWR_MGMT_1, data);
            InfoPrintf("Put device into D0!\n");
        }

        //-----------------------------------------------------
        //Put device to D0 state
        // LW_XVE_XUSB_PWR_MGMT_1 in dev_lw_xve.ref
        //  - Set LW_XVE_XUSB_PWR_MGMT_1_PWR_STATE to D0.
        //-----------------------------------------------------
        if( (xusb_present == 0x80000000) && (ppc_present == 0x80000000) ){
            Platform::PciWrite32(pci_domain_num,
                               pci_bus_num,
                               pci_dev_num,
                               2, //xusb fn num
                               LW_XVE_XUSB_PWR_MGMT_1, //offset is same for all functions
                               DRF_NUM(_XVE_XUSB, _PWR_MGMT_1, _PWR_STATE, 0));
            
            //-----------------------------------------------------
            // Put device to D0 state
            // LW_XVE_PPC_PWR_MGMT_1 in dev_lw_xve.ref
            //  - Set LW_XVE_PPC_PWR_MGMT_1_PWR_STATE to D0.
            //----------------------------------------------------- 
            Platform::PciWrite32(pci_domain_num,
                               pci_bus_num,
                               pci_dev_num,
                               3, // xusb fn num
                               LW_XVE_PPC_PWR_MGMT_1, //offset is same for all functions
                               DRF_NUM(_XVE_PPC, _PWR_MGMT_1, _PWR_STATE, 0));
        }
        //-----------------------------------------------------
        // Delay long enough to enter ASPM L1.1
        // During the period, pmu will issue priv access thru bar0 interface of host
        //-----------------------------------------------------
        if(selfcheck_EscapeRead)
        {
            InfoPrintf("Check whether goes to PDN_WAKE_UP_EVENT_WAIT state\n");
            if (Checkstate(0x40000)) return;

            //  - enable LW_XVE_PEX_PLL5_UPSTREAM_REQ_WAKE_EN (which is disabled by default)
            //data = lwgpu->RegRd32(0x00091418);
            //data = FLD_SET_DRF(_XTL_EP_PRI, _PRIV_XV_BLKCG2, _UPSTREAM_REQ_WAKE_EN, _ENABLED, data);
            //lwgpu->RegWr32(0x00091418, data);

            InfoPrintf("Check whether revert to SEQ_IDLE state\n");
            if (Checkstate(0x1)) return;
        }

        InfoPrintf("Waiting for delay 100 \n");
        Platform::Delay(100);
        InfoPrintf("Done waiting for delay 100, expecting to enter L1.1\n");

        int time_count = 0;
        bool state_matched = 0;
        while ( (time_count < 100) && !state_matched)
        {
            Platform::EscapeRead("lw_x16_xpl_ltssm_state_is_l1_1",0,1,&lw_x16_xpl_ltssm_state_is_l1_1);
            Platform::EscapeRead("lw_x16_xpl_ltssm_state_is_l1_2",0,1,&lw_x16_xpl_ltssm_state_is_l1_2);
            if ((lw_x16_xpl_ltssm_state_is_l1_1 == 0x1) && (lw_x16_xpl_ltssm_state_is_l1_2 == 0x0))
            {
                state_matched = 1;
            }
            else
            {
                time_count++;
            }
        }
        if (!state_matched)
        {
            SetStatus(TEST_FAILED);
            getStateReport()->runFailed("Failed check L1 substate.\n");
            ErrPrintf("Priv read addr 0x0010a040(LW_PPWR_PMU_GPIO_1_INPUT_XP2PMU_STATE_IS_L1_2, LW_PPWR_PMU_GPIO_1_INPUT_XP2PMU_STATE_IS_L1_1), got data = %x\n", data);
            return;
        }
        else
        {
            InfoPrintf("Priv read addr 0x0010a040(LW_PPWR_PMU_GPIO_1_INPUT_XP2PMU_STATE_IS_L1_2, LW_PPWR_PMU_GPIO_1_INPUT_XP2PMU_STATE_IS_L1_1), got data = %x\n", data);
        }

        if (bfm_initiate_exit)
        {
            InfoPrintf("BFM initiate exit");
        }
        else
        {
            //-----------------------------------------------------
            // Initiate Exit L1 -- go to recovery
            // LW_XP3G_PL_LINK_CONFIG_0 in dev_lw_xp3g.ref
            //  - Set LW_XP3G_PL_LINK_CONFIG_0_LTSSM_DIRECTIVE set to LW_XP3G_PL_LINK_CONFIG_0_LTSSM_DIRECTIVE_GOTO_RECOVERY (0xc)
            //-----------------------------------------------------
            data = lwgpu->RegRd32(0x00085008);
            data = FLD_SET_DRF(_XPL, _PL_LTSSM_LINK_CONFIG_0, _LTSSM_DIRECTIVE,_GOTO_RECOVERY, data);
            lwgpu->RegWr32(0x00085008, data);
            InfoPrintf("Write LW_XP3G_PL_LINK_CONFIG_0_LTSSM_DIRECTIVE to _GOTO_RECOVERY!\n");
        }

        InfoPrintf("Now make sure priv operation works - device resumed ok!\n");
        data = 0x5a5a;
        lwgpu->RegWr32(0x0010a040, data);
        InfoPrintf("Priv write addr 0x0010a040(LW_PPWR_FALCON_MAILBOX0), data = %x\n", data);

        //-----------------------------------------------------
        // LW_XVE_PWR_MGMT_1 in dev_lw_xve3g_fn0.ref
        //  - Restore LW_XVE_L1_PM_SUBSTATES_CTRL1_ASPM_L1_1_EN to 0.
        //-----------------------------------------------------
        data = lwgpu->CfgRd32(0x02000098);
        data = FLD_SET_DRF(_EP_PCFG_GPU,_L1_PM_SS_CONTROL_1_REGISTER,_ASPM_L11_ENABLE,_DISABLE,data);
        lwgpu->CfgWr32(0x02000098, data);
        InfoPrintf("Restore ASPM_L1_1_EN back to 0!\n");

        InfoPrintf("test L1_substate aspm_l1_1() Done !\n");
    } //endif_aspm_l1_1

    SetStatus(TEST_SUCCEEDED);
    getStateReport()->crcCheckPassedGold();

    ch->WaitForIdle();
}//end_Run
