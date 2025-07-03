/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2017,2019, 2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */
#ifndef _GC6PLUS_H_
#define _GC6PLUS_H_

#include <map>
#include "mdiag/utils/types.h"
#include "mdiag/tests/gpu/lwgpu_single.h"
#include "mdiag/IRegisterMap.h"
#include "i2cslave_fermi.h"
#include "mdiag/utils/randstrm.h"
#include "gpu/include/pcicfg.h"


#define FIND_REGISTER(map_ptr, reg_ptr, regname) \
    if (!(reg_ptr = map_ptr->FindRegister(regname))) \
    { \
        ErrPrintf("GC6Plus: Failed to find %s register\n", regname); \
        return (1);  \
    }
#define FIND_FIELD(reg_ptr, field_ptr, fieldname) \
    if (!(field_ptr = reg_ptr->FindField(fieldname))) \
    { \
        ErrPrintf("GC6Plus: Failed to find %s field\n", fieldname); \
        return (1);  \
    }
#define FIND_VALUE(field_ptr, value_ptr, valuename) \
    if (!(value_ptr = field_ptr->FindValue(valuename))) \
    { \
        ErrPrintf("GC6Plus: Failed to find %s value\n", valuename); \
        return (1);  \
    }
#define RETURN_IF_ERR(func) \
    if (func) \
    { \
        ErrPrintf("GC6Plus: Unrecoverable error detected\n"); \
        return (1); \
    }

// These command opcodes and registers need to stay consistent with the
// PMU ucode.  They are used for communication between the MODS mdiag
// test and PMU ucode.  The PMU ucode files can be found at:
//   //hw/<chip_tree>/stand_sim/cc_tests/pmu/fullchip/bsi_ram/*/app.c
#define PMU_NOOP          0x00
#define GC5_ENTRY         0x01
#define GC6_ENTRY         0x02
#define PMU_HALT          0xff
#define PMU_COMMAND_REG   "LW_PPWR_PMU_DEBUG_TAG"
#define ERR_COUNT_REG     "LW_PPWR_PMU_MAILBOX(8)"
#define HEARTBEAT_REG     "LW_PPWR_PMU_MAILBOX(9)"

class GC6Plus : public LWGpuSingleChannelTest
{

public:

    GC6Plus(ArgReader *params);

    virtual ~GC6Plus(void);

    static Test *Factory(ArgDatabase *args);
    
    // a little extra setup to be done
    virtual int Setup(void);
    
    // Run() overridden - now we're a real test!
    virtual void Run(void);
    
    // a little extra cleanup to be done
    virtual void CleanUp(void);


protected:
    int gc6plus_entry_test();
    int gc6plus_steady_test();

private:
    typedef enum
    {
        GC5,
        GC6,
        CYA,
        STEADY,
    } ModeType;

    typedef enum
    {
        DEFAULT,
        GPIO_NOPULL,
      //Add more testbench configs as needed
    } TBMode;


    typedef enum
    {
        TTYPE_ENTRY,
        TTYPE_STEADY,
      //Add more enums as needed
    } TestType;

    typedef enum
    {
        TEST_NONE,
        TEST_ENTRY_GC5,
        TEST_ENTRY_GC6,
        TEST_STEADY_IO_CLOBBER,
      //Add more test name enums as needed. Gets set by "-test" parsing
    } TestName;

    typedef enum
    {
        ABORT,
        EXIT,
        RESET,
    } ExitType;

    IRegisterMap* m_regMap;  
    UINT32 m_ErrCount;
    UINT32 m_polling_timeout_us;
    ModeType m_mode;
    TBMode m_tb_mode;
    TestType m_test_type;
    TestName m_test;
    unique_ptr<PciCfgSpace> m_pPciCfg;
    UINT32 m_ec_reset_width_us;
    UINT32 m_ec_gpu_event_width_us;
    UINT32 m_sci_gpu_event_width_us;
    UINT32 m_vid_pwm_period;
    UINT32 m_vid_pwm_hi_normal;
    UINT32 m_max_gpios;
    UINT32 m_seed;
    UINT32 gpio_index_hpd[6];
    UINT32 gpio_index_misc[4];
    UINT32 gpio_index_fb_clamp;
    UINT32 gpio_index_power_alert;
    UINT32 gpio_index_fsm_gpu_event;
    UINT32 gpio_index_vid_pwm;
    UINT32 gpio_index_main_3v3_en;
    UINT32 gpio_index_lwvdd_pexvdd_en;
    UINT32 gpio_index_fbvddq_gpu_en;
    UINT32 gpio_index_fbvdd_q_mem_en;
    UINT32 gpio_index_pexvdd_vid;
    UINT32 gpio_index_gpu_gc6_rst;
    UINT32 gpio_index_sys_rst_mon;
    UINT32 gpio_index_pmu_comm;
    UINT32 trigger_delay_us;
    UINT32 sci_entry_holdoff_us;
    UINT32 i2c_snoop_scl_timeout_us;
    UINT32 therm_dev_addr;
    UINT32 first_wake_event_us;
    UINT32 event_window_us;
    UINT32 expected_exit;
    bool enab_system_reset_mon;
    bool gc6_abort_skipped;
    bool first_wake_event_is_wakeup_tmr;
    bool first_wake_event_is_therm_i2c;
    bool cnf_sci_entry_holdoff;
    bool disable_sdr_mode;
    bool enab_gpu_event_reset;
    bool enab_gc6_abort;
    bool legacy_sr_update;
    bool enab_sw_wake_timer;
    bool enab_rand_lwvdd_ok_offset;
    bool gen_wake_gpio_misc_0;
    bool gen_wake_ec_gpu_event;
    bool gen_wake_therm_i2c;
    bool gen_wake_rxstat;
    bool gen_wake_clkreq;
    bool gen_ec_reset;
    bool gen_i2c_sci_priv_rd_SCRATCH;
    bool gen_sr_update;
    UINT32 m_test_loops;
    UINT32 wake_timer_us;
    UINT32 wake_gpio_misc_0_delay;
    UINT32 wake_ec_gpu_event_delay;
    UINT32 wake_therm_i2c_delay;
    UINT32 wake_rxstat_delay;
    UINT32 wake_clkreq_delay;
    UINT32 ec_reset_delay;
    UINT32 i2c_sci_priv_rd_SCRATCH_delay;
    UINT32 sr_update_delay;
    UINT32 m_lwvdd_ok_val;
    UINT32 m_lwvdd_ok_offset;
    RandomStream* m_rndStream;
    
    int cfg_fullchip_tb();
    int save_pcie();
    int restore_pcie();
    int InitPrivRing();
    int cfg_therm();
    int cfg_pmgr();
    int cfg_bsi();
    int trigger_pmu_ucode_load();
    int cfg_sci();
    int cfg_sci_ptimer();
    int cfg_sci_power_sequencer();
    int cfg_sci_gpio();
    int cfg_sci_i2cs_snoop();
    int cfg_pwms(UINT32 period, UINT32 hi, const char *pwm_sel);
    int cfg_gpio_pwm_1hot(UINT32 gpio_num, UINT32 gpio_drv, std::map<unsigned int, unsigned int> &PastRegWrs);
    int prepare_for_entry();
    int generate_events();
    int cleanup_after_exit();
    int check_exit_results();
    int check_resident_timers();
    int check_sw_status();
    int run_io_clobber_test(int loop_delay_us);
    int check_ptimer();
    int check_smartfan();
    int check_i2cs();

    int poll_for_fsm_state(const char *state, bool equal, UINT32 timeout);
    int poll_escape(const char *escapename, UINT32 match_value, bool equal,
        UINT32 timeout);
    int poll_reg_field(const char *regname,
                       const char *fieldname,
                       const char *valuename,
                       bool equal,
                       UINT32 timeout);
    int set_pmu_command(UINT32 command);
    int poll_pmu_command(UINT32 command, bool equal, UINT32 timeout);
    int check_pmu_errors();
    int Reg_RMW(const char *regname, const char *fieldname, const char *valuename);
    int Reg_RMW(const char *regname, const char *fieldname, UINT32 value);
    int check_priv(const char *regname, UINT32 exp_data, UINT32 mask);
    int cfg_gpio_engine(UINT32 gpio, UINT32 drive, UINT32 pull);
    int set_gpio_state(UINT32 gpio, UINT32 state);
    UINT32 get_gpio_state(UINT32 gpio);
    int poll_gpio_state(UINT32 gpio, UINT32 state, bool equal, UINT32 timeout);


    
};

#ifdef MAKE_TEST_LIST
CREATE_TEST_LIST_ENTRY(gc6plus, GC6Plus, "GPU gc6plus Test");
#undef TEST_LIST_HEAD
#define TEST_LIST_HEAD &gc6plus_testentry
#endif

#endif  // _GC6PLUS_H_
