/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2011, 2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */
#ifndef _STOP_CLOCKS_H_
#define _STOP_CLOCKS_H_

// test various PMU and LW_THERM clock stop features

#include "mdiag/utils/types.h"
#include "mdiag/tests/gpu/lwgpu_single.h"
#include "mdiag/IRegisterMap.h"

// These command opcodes and registers need to stay consistent
// with the PMU ucode.  They can be found in:
// //hw/<chip_tree>/stand_sim/cc_tests/pmu/fullchip/pmu_csb/app.c
#define SC_COMMAND_REG   "LW_PPWR_PMU_DEBUG_TAG"
#define SC_PARAM_1_REG   "LW_PPWR_PMU_MAILBOX(1)"
#define SC_PMU_DONE      0x00
#define SC_STOP_CLOCK    0x90
#define SC_PMU_HALT      0xff
#define SC_GPIO_SLEEP    4

// This GPIO is used for generating interupts.  It must be unique from the
// GPIO used for to indicate Deep L1 Sleep condition.
#define SC_GPIO_INTR     5

// PCI config space (LW_XVE_*) registers are accessed by adding 0x88000 to
// the BAR0 address.
#define LW_PCFG_OFFSET   0x88000

class StopClocks : public LWGpuSingleChannelTest
{

public:

    StopClocks(ArgReader *params);

    virtual ~StopClocks(void);

    static Test *Factory(ArgDatabase *args);

    // a little extra setup to be done
    virtual int Setup(void);

    // Run() overridden - now we're a real test!
    virtual void Run(void);

    // a little extra cleanup to be done
    virtual void CleanUp(void);

protected:
    UINT32 ThermStopClocksIntrTest();

private:

    IRegisterMap* m_regMap;
    UINT32 m_ErrCount;
    UINT32 m_max_polling_attempts;

    INT32 check_stop_clocks_intr_support();
    INT32 setup_sleep_status(UINT32 gpio);
    INT32 setup_intr(UINT32 gpio);
    INT32 mask_gpio_intr(UINT32 gpio);
    INT32 config_gpio_input(UINT32 gpio);
    INT32 config_gpio_output(UINT32 gpio);
    INT32 trigger_gpio_update();
    INT32 setup_xclk_for_stopping();
    INT32 set_div4_mode();
    INT32 set_one_src(const char *clkname, const char *pllname, UINT32 ldiv);
    INT32 set_pexpll_lockdet(UINT32 wait_delay);
    INT32 set_xclk_cond_stopclk(bool enable);
    INT32 set_gpio_state(UINT32 gpio, UINT32 state);
    INT32 set_sleep_ctrl(const char *wake_event);
    INT32 set_pmu_command(UINT32 command, UINT32 param1);
    INT32 poll_for_pmu_done();
    INT32 config_xp_blcg();
    INT32 enable_deep_l1();
    INT32 exit_deep_l1();
    INT32 config_idle_counter(UINT32 index);
    INT32 config_gate_ctrl(UINT32 index, const char *mode);
    INT32 config_xp_power1();
    INT32 config_xp_power2();
    INT32 check_host_intr(bool active);
    INT32 clear_gpio_intr(UINT32 gpio);
    UINT32 get_gpio_state(UINT32 gpio);
    INT32 poll_gpio_state(UINT32 gpio, UINT32 state);
    INT32 check_xclk_freq (UINT32 interval_ns, INT32 exp_freq_khz);
    INT32 check_sleep_status(const char *wake_event);

};

#ifdef MAKE_TEST_LIST
CREATE_TEST_LIST_ENTRY(stop_clocks, StopClocks,
    "GPU PMU/LW_THERM Stop Clocks Test");
#undef TEST_LIST_HEAD
#define TEST_LIST_HEAD &stop_clocks_testentry
#endif

#endif  // _STOP_CLOCKS_H_

