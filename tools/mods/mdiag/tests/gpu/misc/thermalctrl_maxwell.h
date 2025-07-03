/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2006-2007, 2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */
#ifndef _THERMALCTRL_MAXWELL_H_
#define _THERMALCTRL_MAXWELL_H_

#include "mdiag/utils/types.h"
#include "mdiag/tests/gpu/lwgpu_single.h"
#include "pwr_macro.h"

#define LW_VIRTUAL_FUNCTION_PRIV_CPU_INTR_THERMAL_VECTOR 146 // Find the macro define in <chip>_ref.txt from SW tree
#define INTR_BIT   18                                        // INTR_BIT   = LW_VIRTUAL_FUNCTION_PRIV_CPU_INTR_THERMAL_VECTOR%32
#define INTR_INDEX 4                                         // INTR_INDEX = LW_VIRTUAL_FUNCTION_PRIV_CPU_INTR_THERMAL_VECTOR/32
#define CPU_INTR_LEAF 0xb81010                               // CPU_INTR_LEAF = 0xb81000 + INTR_INDEX * 4

enum IDLE_TYPE_MAXWELL {
  TYPE_ENG_MAXWELL    = 0,
  TYPE_PMU_MAXWELL    = 1,
  TYPE_SLOW_MAXWELL   = 2,
  TYPE_ALWAYS_MAXWELL = 3,
  TYPE_NEVER_MAXWELL  = 4
};

enum PMU_SRC_MAXWELL {
  PMU_GR_MAXWELL   = 0,
  PMU_VD_MAXWELL   = 1,
  PMU_CE_MAXWELL   = 2,
  PMU_DISP_MAXWELL = 3,
  PMU_OTHER_MAXWELL = 8
};

class ThermalCtrl_maxwell : public LWGpuSingleChannelTest {
public:
    ThermalCtrl_maxwell(ArgReader *params);

    virtual ~ThermalCtrl_maxwell(void);

    static Test *Factory(ArgDatabase *args);

    // a little extra setup to be done
    virtual int Setup(void);

    // Run() overridden - now we're a real test!
    virtual void Run(void);

    // a little extra cleanup to be done
    virtual void CleanUp(void);

protected:

    void setGpioAlt(int bit, int en);
    void setGpio(int bit, int en, int value);
    int resetRegister();
    UINT32 temp2code(UINT32 temp);
    void SetInterruptTypeHigh();
    void SetInterruptTypeLow();
    void SetInterruptTypeBoth();
    void SetInterruptTypeDisable();
    int checkExpectFSslowdown(bool exp_sd, int i);
    void SetSWSlowdown(int div_factor);
    void SetFS2Local(UINT32 addr, int factor);
    int checkclkcount(float expect_factor_gpc, UINT32 &fullspeed_cnt_ref);
    void SetIdleSD_type(IDLE_TYPE_MAXWELL condition_a , IDLE_TYPE_MAXWELL condition_b);
    void SetIdleSD_eng_src(int eng_src);
    void SetIdleSD_pmu_src(PMU_SRC_MAXWELL pmu_src);
    int checkExpectIdleslowdown(bool exp_sd, int i);

    int checkGpioConn();
    int checkFuse();
    int checkSensorClk();
    int checkSDOB();
    int checkIdleSlowdown();
    int checkThermSensorConnectivity();
    int checkPgoot_Int();
    int checkPgoot_Ext();
    int checkTherm2HostIntrConnection();

    int check_power_gating(bool gated);

    float Slow_Factor(int factor,bool extend, int jtag_val);
    int setSlowfactor(int factor);

    int checkSlowdown();
    int setSwSlowdown(int gpc);
    int checkDvcoSlowdown();

private:
    UINT32     m_arch;
    bool       m_has_gk110_alert_regs;
    Macro macros;

    int saveOrCheck(float expected_slow_factor, UINT32 &fullspeed_cnt_ref, UINT32 count);
};

#ifdef MAKE_TEST_LIST
CREATE_TEST_LIST_ENTRY(thermalCtrlGpu_maxwell, ThermalCtrl_maxwell, "GPU Thermal Control Test");
#undef TEST_LIST_HEAD
#define TEST_LIST_HEAD &thermalCtrlGpu_maxwell_testentry
#endif

#endif  // _THERMALCTRL_H_
