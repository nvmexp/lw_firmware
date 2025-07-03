/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2013-2017, 2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */
#ifndef _GC6PLUS_MISC_H_
#define _GC6PLUS_MISC_H_

#include "mdiag/utils/types.h"
#include "mdiag/tests/gpu/lwgpu_single.h"
#include "mdiag/IRegisterMap.h"

// The PRIV_LEVEL is lwrrently hardcoded.  This may change in the future.
#define PRIV_LEVEL_CPU_BAR0  0

// Some hardcoded constants used by priv_level test
#define MAX_POLLING_ATTEMPTS 1000
#define ERROR_CODE_FECS_PRI_CLIENT_PRIV_LEVEL_VIOLATION 0x00badf51
#define LW_PMC_INTR_MSK_0_ADDR 0x00000640

class GC6PlusMisc : public LWGpuSingleChannelTest
{

public:

    GC6PlusMisc(ArgReader *params);

    virtual ~GC6PlusMisc(void);

    static Test *Factory(ArgDatabase *args);

    // a little extra setup to be done
    virtual int Setup(void);

    // Run() overridden - now we're a real test!
    virtual void Run(void);

    // a little extra cleanup to be done
    virtual void CleanUp(void);

    INT32 test_priv_protect(const char *priv_regname, const char
        *priv_mask_regname, UINT32 array_index);
    INT32 read_write_test(const char *regname, UINT32 reg_addr,
        UINT32 rw_mask, UINT32 init_data, UINT32 wr_protection,
        UINT32 wr_violation, UINT32 rd_protection, UINT32 rd_violation);
    bool is_locked (UINT32 req_level, UINT32 level_mask);
    bool exp_violation (UINT32 req_level, UINT32 level_mask, UINT32
        level_violation);
    INT32 get_error_code(UINT32 req_level, UINT32 level_mask, UINT32
        level_violation, UINT32 *error_code);
    INT32 check_pri_fecserr(UINT32 addr, UINT32 write, UINT32 wr_data,
        UINT32 req_level, UINT32 level_mask, UINT32 level_violation);
    INT32 clear_pri_fecserr();
    INT32 check_sys_priv_error(UINT32 addr, UINT32 write, UINT32 wr_data,
        UINT32 req_level, UINT32 level_mask, UINT32 level_violation);
    INT32 clear_sys_priv_error();
    INT32 override_level_mask(const char *regname, UINT32 wr_protection,
        UINT32 wr_violation, UINT32 rd_protection, UINT32 rd_violation);
    INT32 cfg_level_mask(const char *regname, UINT32 wr_protection,
        UINT32 wr_violation, UINT32 rd_protection, UINT32 rd_violation);
    INT32 chk_level_mask(const char *regname, UINT32 wr_protection,
        UINT32 wr_violation, UINT32 rd_protection, UINT32 rd_violation);
    INT32 read_and_check_reg(const char *regname, UINT32 reg_addr,
        UINT32 exp_data, UINT32 rw_mask, UINT32 level_mask,
        UINT32 level_violation);
    INT32 write_and_check_reg(const char *regname, UINT32 reg_addr,
        UINT32 wr_data, UINT32 level_mask, UINT32 level_violation);
    INT32 disable_priv_err_intr();
    INT32 poll_for_ring_command_done(UINT32 timeout);
    INT32 poll_reg_field(const char *regname,
                         const char *fieldname,
                         UINT32 match_value,
                         bool equal,
                         UINT32 timeout);
    INT32 set_priv_selwrity(bool enable);
    INT32 get_priv_ack_mode(bool *cpu_immediate_ack);
    INT32 set_sci_ptimer(UINT32 time_1, UINT32 time_0);
    INT32 get_sci_ptimer(UINT32 *time_1, UINT32 *time_0);
    INT32 set_host_ptimer(UINT32 time_1, UINT32 time_0);
    INT32 get_host_ptimer(UINT32 *time_1, UINT32 *time_0);
    INT32 get_ptimer_difference(INT64 *difference);
    INT32 set_selwre_timer(UINT32 time_1, UINT32 time_0);
    INT32 get_selwre_timer(UINT32 *time_1, UINT32 *time_0);
    INT32 get_selwre_timer_difference(INT64 *difference);
    INT32 check_gpio_min_max(UINT32 array_size);
    INT32 get_tb_gpio_cfg(UINT32 index, UINT32 *drive, UINT32 *pull);
    INT32 cfg_tb_gpio(UINT32 index, UINT32 drive, UINT32 pull);
    INT32 disable_pmgr_intr();
    INT32 disable_pmgr_gpio(UINT32 index, UINT32 *data);
    INT32 configure_pmgr_gpio(UINT32 index);
    INT32 restore_pmgr_gpio(UINT32 index, UINT32 data);
    INT32 trigger_pmgr_gpio_update();

protected:
    UINT32 PrivLevelTest();
    UINT32 HostPtimerTest();
    UINT32 SelwreTimerTest();
    UINT32 SciIndvAltCtlTest();
    UINT32 SciHwGpioCopyTest();

private:
    IRegisterMap* m_regMap;
    UINT32 m_ErrCount;
    UINT32 m_max_polling_attempts;
    const char *m_priv_level_regname;
    const char *m_priv_level_maskname;
    UINT32 m_priv_level_array_index;
    UINT32 m_ring_intr_poll_timeout;
    bool m_cpu_bar0_immediate_ack;
    bool m_violation_fields_protected;
    bool m_opt_priv_sec_en_protected;
    UINT32 m_system_time_0;
    UINT32 m_system_time_1;
    UINT32 m_ptimer_delay_ms;
    INT32 m_max_ptimer_drift_ns;
    bool m_read_time_using_escape_read;
    INT32 m_gpio;
    INT32 m_gpio_min;
    INT32 m_gpio_max;
};

#ifdef MAKE_TEST_LIST
CREATE_TEST_LIST_ENTRY(gc6plus_misc, GC6PlusMisc, "GC6Plus Misc test");
#undef TEST_LIST_HEAD
#define TEST_LIST_HEAD &gc6plus_misc_testentry
#endif

#endif  // _GC6PLUS_MISC_H_

