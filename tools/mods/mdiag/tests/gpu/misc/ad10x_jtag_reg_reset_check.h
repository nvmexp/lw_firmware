/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2021-2022 by LWPU Corporation.  All rights reserved. 
 * All information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#ifndef _AD10X_JTAG_REG_RESET_CHECK_
#define _AD10X_JTAG_REG_RESET_CHECK_

#include "mdiag/utils/types.h"
#include "mdiag/tests/gpu/lwgpu_single.h"
#include "mdiag/lwgpu.h"

#include "mdiag/utils/cregexcl.h"
#include "mdiag/IRegisterMap.h"

#include <map>
using std::map;

extern SObject Gpu_Object;

class Ad10xJtagRegResetCheck : public LWGpuSingleChannelTest {
public:
    Ad10xJtagRegResetCheck(ArgReader *m_params);

    virtual ~Ad10xJtagRegResetCheck(void);

    static Test *Factory(ArgDatabase *args);

    // a little extra setup to be done
    virtual int Setup(void);

    // gives the test valid pointer access to the gpu resources after an init/reset
    virtual int SetupGpuPointers(void);

    // Run() overridden - now we're a real test!
    virtual void Run(void);

    // a little extra cleanup to be done
    virtual void CleanUp(void);

protected:
    bool testFundamentalRst();
    bool testNoFundamentalRst();
    bool fundamentalReset();

private:
    bool GetInitValue        (IRegisterClass* reg, UINT32 testAddr, UINT32 *initVal, UINT32 *initMask);
    bool disable_fundamental_reset_on_hot_rst(void);
    bool enable_fundamental_reset_on_hot_rst(void);
    bool disable_xve_cfg_reset_on_hot_rst(void);
    void hot_reset(void);
    bool gpu_initialize(void);
    bool wait_for_jtag_ctrl(void);
    bool test_logic_reg_write(UINT32 dword0,UINT32 dword1);
    bool test_logic_reg_read_check(UINT32 dword0,UINT32 dword1);
    void host2jtag_data_read(UINT32& data_value, UINT32 reg_length, bool last_dword);
    void host2jtag_data_write(UINT32 data_value);
    bool host2jtag_ctrl_write(UINT32 req_ctrl, UINT32 status, UINT32 chiplet_sel, UINT32 dword_en, UINT32 reg_length,UINT32 instr_id, bool read_or_write);
    void host2jtag_config_write(UINT32 config_write_data);
    void save_config_space();
    bool restore_config_space();
    bool pci_write(UINT32 offset, UINT32 data);

    ArgReader *m_params;
    IRegisterMap*  m_regMap;
    IRegisterClass *m_reg;

    // These parameters control which subtest gets run. If m_param_test_fundamental_rst
    // is present then then subtest testFundamentalRst will get run and if
    // m_param_test_no_fundamental_rst is present then the subtest testNoFundamentalRst
    // will get excelwted. Subtest descriptions are there in the .cpp file.
    bool m_param_test_fundamental_rst;
    bool m_param_test_no_fundamental_rst;
    bool m_param_test_fundamental_reset;

    // These are for storing the config space between resets. We need to restore the config space registers after the
    // reset , since it resets the config space.Gpu pointers won't be available just after reset so we need to store
    // both the address and data
    UINT32 m_bar0_data;
    UINT32 m_irqSet_data;
    UINT32 m_vendid_data;
    UINT32 m_bar1lo_data;
    UINT32 m_bar1hi_data;
    UINT32 m_bar2lo_data;
    UINT32 m_bar2hi_data;
    UINT32 m_bar3_data;
    UINT32 m_bar0_addr;
    UINT32 m_irqSet_addr;
    UINT32 m_vendid_addr;
    UINT32 m_bar1lo_addr;
    UINT32 m_bar1hi_addr;
    UINT32 m_bar2lo_addr;
    UINT32 m_bar2hi_addr;
    UINT32 m_bar3_addr;
    UINT32 m_clk_mask_reg_length;

    // These are for storing the pci parameters like bus,dev,fun no . These parameters will be used in the PciWrite32
    // function to write back the config registers.
    UINT32 m_pci_domain_num;
    UINT32 m_pci_bus_num;
    UINT32 m_pci_dev_num;
    UINT32 m_pci_func_num;

    UINT32 clkMaskAccessMask0; 
    UINT32 clkMaskAccessMask1; 

    // Selected write data as 39'h000000a0_05000000.
    static const UINT32 JTAG_WRITE_DATA_DWORD_0  = 0x000ff000;
    static const UINT32 JTAG_WRITE_DATA_DWORD_1  = 0x0000000e;
    // reset value
    static const UINT32 RESET_DATA_DWORD_0   = 0x00000000;
    static const UINT32 RESET_DATA_DWORD_1   = 0x00000000;
    static UINT32 const READ_INTENT = 0;
    static UINT32 const WRITE_INTENT = 1;
    static UINT32 const CTRL_STATUS_BUSY = 0;
    static UINT32 const MAX_HOST_CTRL_ATTEMPTS = 800 ;
    // Disable captureDR/updateDR
    static UINT32 const CONFIG_WRITE_DATA = 0x00300000;

    // CORE_TRIM_CTL jtag register specifications can be found from the below jrd file,
    // vmod/ieee1500/chip/gp100/jtagreg/lw_top/s0_0/CORE_TRIM_CTL.jrd
    // need to update this whenever there is a change in these info for this register.

    static UINT32 const s_clkMaskInstId = 0x72;
    static UINT32 const s_clkMaskRegLenthGa100 = 0x36;
    static UINT32 const s_clkMaskRegLenthAd102 = 0x40;
    static UINT32 const s_clkMaskAccessMask0Ad102 = 0x0;
    static UINT32 const s_clkMaskAccessMask1Ad102 = 0x1f7f8000;
    #if LWCFG(GLOBAL_GPU_IMPL_AD103)
        static UINT32 const s_clkMaskRegLenthAd103 = 0x31;
        static UINT32 const s_clkMaskAccessMask0Ad103 = 0x0;
        static UINT32 const s_clkMaskAccessMask1Ad103 = 0x3eff;
    #endif
    #if LWCFG(GLOBAL_GPU_IMPL_AD104)
        static UINT32 const s_clkMaskRegLenthAd104 = 0x2C;
        static UINT32 const s_clkMaskAccessMask0Ad104 = 0xf8000000;
        static UINT32 const s_clkMaskAccessMask1Ad104 = 0x1f7;
    #endif
    #if LWCFG(GLOBAL_GPU_IMPL_AD106)
        static UINT32 const s_clkMaskRegLenthAd106 = 0x23;
        static UINT32 const s_clkMaskAccessMask0Ad106 = 0xfbfc0000;
        static UINT32 const s_clkMaskAccessMask1Ad106 = 0x0;
    #endif
    #if LWCFG(GLOBAL_GPU_IMPL_AD107)
        static UINT32 const s_clkMaskRegLenthAd107 = 0x22;
        static UINT32 const s_clkMaskAccessMask0Ad107 = 0x7dfe0000;
        static UINT32 const s_clkMaskAccessMask1Ad107 = 0x0;
    #endif
    static UINT32 const s_chipletGmbS0 = 2;
    static UINT32 const s_dwordZero = 0;
    static UINT32 const s_dwordOne = 1;
    static UINT32 const s_reqNew = 1;
    static UINT32 const s_reqClr= 0;

    
    // Number of bits in a UINT32 which is the return type of RegRd32
    // Previous define caused a build failure
    static UINT32 const s_readBits = 32;
};

#ifdef MAKE_TEST_LIST
CREATE_TEST_LIST_ENTRY(ad10x_jtag_reg_reset_check, Ad10xJtagRegResetCheck, "Gm20x+ Fundamental Reset does test register reset Test");
#undef TEST_LIST_HEAD
#define TEST_LIST_HEAD &ad10x_jtag_reg_reset_check_testentry
#endif

#endif
