/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2017-2019, 2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */
#ifndef _GC6plus_Maxwell_H_
#define _GC6plus_Maxwell_H_

#include "mdiag/utils/types.h"
#include <map>
#include "mdiag/tests/gpu/lwgpu_single.h"
#include "mdiag/lwgpu.h"
#include "mdiag/utils/randstrm.h"
#include "i2cslave_fermi.h"
#include "mdiag/IRegisterMap.h"
#include <iostream>
#include <string>
#include "regmap_helper.h"
#include "gpu/include/pcicfg.h"

#define LW_PGC6_SCI_TMR_SYSTEM_RESET_TIMEOUT__HWRESET_VAL  0x000007d0

/* #define LW_FUSE_FUSECTRL                               0x00021000 /\* RW-4R *\/ */
/* #define LW_FUSE_FUSECTRL_CMD                           1:0        /\* RWIVF *\/ */
/* #define LW_FUSE_FUSECTRL_CMD_IDLE                      0x00000000 /\* RWI-V *\/ */
/* #define LW_FUSE_FUSECTRL_CMD_READ                      0x00000001 /\* -W--T *\/ */
/* #define LW_FUSE_FUSECTRL_CMD_WRITE                     0x00000002 /\* -W--T *\/ */
/* #define LW_FUSE_FUSECTRL_CMD_SENSE_CTRL                0x00000003 /\* -W--T *\/ */
/* #define LW_FUSE_FUSECTRL_STATE                         19:16 /\* R--VF *\/ */

/* #define LW_FUSE_FUSEADDR                               0x00021004 /\* RW-4R *\/ */
/* #define LW_FUSE_FUSEADDR_VLDFLD                        5:0        /\* RWIVF *\/ */
/* #define LW_FUSE_FUSEADDR_VLDFLD_INIT                   0x00000000 /\* RWI-V *\/ */

/* #define LW_FUSE_FUSEWDATA                            0x0002100c /\* -W-4R *\/ */
/* #define LW_FUSE_FUSEWDATA_DATA                       31:0       /\* -WIVF *\/ */
/* #define LW_FUSE_FUSEWDATA_DATA_INIT                  0x00000000 /\* -WI-V *\/ */

/* #define LW_FUSE_ENABLE_FUSE_PROGRAM__PRI_ALIAS_0 0x0 */
/* #define LW_FUSE_ENABLE_FUSE_PROGRAM__PRI_ALIAS_0_DATA 0:0 */
/* #define LW_FUSE_ENABLE_FUSE_PROGRAM__PRI_ALIAS_0_WIDTH 1 */
/* #define LW_FUSE_ENABLE_FUSE_PROGRAM__RED_ALIAS_0 0x1 */
/* #define LW_FUSE_ENABLE_FUSE_PROGRAM__RED_ALIAS_0_DATA 0:0 */
/* #define LW_FUSE_ENABLE_FUSE_PROGRAM__RED_ALIAS_0_WIDTH 1 */

/* #define LW_FUSE_FUSECTRL_STATE_IDLE 4 */
#define LW_FUSE_PRIV2INTFC_START                       0x00021020 /* -W-4R */
/* #define LW_FUSE_PRIV2INTFC_START_DATA                  0:0 /\* -WIVF *\/  */
/* #define LW_FUSE_PRIV2INTFC_START_INIT                  0x00000000 /\* -WI-V *\/ */

/* #define LW_PBUS_DEBUG_1 0x00001084 */
#define Fn2CfgWr32(A,D) Platform::PciWrite32(0,0x10,0,2,A,D)
#define Fn2CfgRd32(A,D) Platform::PciRead32 (0,0x10,0,2,A,D)
#define Fn3CfgWr32(A,D) Platform::PciWrite32(0,0x10,0,3,A,D)
#define Fn3CfgRd32(A,D) Platform::PciRead32 (0,0x10,0,3,A,D)
#define Fn0CfgWr32(A,D) Platform::PciWrite32(0,0x10,0,0,A,D)
#define Fn0CfgRd32(A,D) Platform::PciRead32 (0,0x10,0,0,A,D)


#define DEFAULT_EXIT_DELAY 100
#define DEFAULT_BSI_RAM_FILE "/home/scratch.traces02/non3d/pwr/gc6plus/bsi_ram_content_PMU_ltssm.txt"

#define Find_Register(map_ptr, reg_ptr, regname) \
    if (!(reg_ptr = map_ptr->FindRegister(regname))) \
    { \
        ErrPrintf("gc6plus_maxwell: Failed to find %s register\n", regname); \
        return (1); \
    }
#define Find_Field(reg_ptr, field_ptr, fieldname) \
    if (!(field_ptr = reg_ptr->FindField(fieldname))) \
    { \
        ErrPrintf("gc6plus_maxwell: Failed to find %s field\n", fieldname); \
        return (1); \
    }
#define Find_Value(field_ptr, value_ptr, valuename) \
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



class func_pointer{
public:
    int operator() (LWGpuSingleChannelTest *, GpuSubdevice *);
};

class GC6plus_Maxwell : public LWGpuSingleChannelTest {
public:

    GC6plus_Maxwell(ArgReader *params);
    
    virtual ~GC6plus_Maxwell(void);
    
    static Test *Factory(ArgDatabase *args);
    
    // a little extra setup to be done
    virtual int Setup(void);
    
    // Run() overridden - now we're a real test!
    virtual void Run(void);
    
    // a little extra cleanup to be done
    virtual void CleanUp(void);

    int configure_wakeup_timer(GpuSubdevice *, int);
    int wakeupTimerEvent(GpuSubdevice *, std::string);
    int gc6plus_init(GpuSubdevice *);
    int printWakeEnables(GpuSubdevice *);
    void EscapeWrite(const char *Path, UINT32 Index, UINT32 Size, UINT32 Value);
    void EscapeWritePrint(const char *Path, UINT32 Index, UINT32 Size, UINT32 Value);
    int EscapeRead(const char *Path, UINT32 Index, UINT32 Size, UINT32 *Value);
    int writePrivRegs(const unsigned int (*priv_config)[2], unsigned int count, GpuSubdevice *pSubdev);
    regmap_helper *h_reg;

protected:
    GpuSubdevice *pSubdev_;
    UINT32 gc6plus_register_maxwellTest();
    UINT32 gc6plus_reg_unmapped_maxwellTest(); 
    UINT32 gc6plus_ptimerTest();
    UINT32 gc5_entry();
    UINT32 gc6_entry();
    
    
    UINT32 gc6plus_wakeupTest(bool additional_wakeup_events=false,
			      bool timer_wakeup_enabled=true);
    UINT32 fgc6_wakeupTest(bool,bool,
                           std::string, int,
                           int);
    UINT32 fgc6_entry(GpuSubdevice *);
    UINT32 fgc6_precredit_if(GpuSubdevice *, std::string);
    UINT32 fgc6_rst_control(GpuSubdevice *, std::string);
    int fgc6_xclk_source(GpuSubdevice *, std::string);
    int fgc6_Clockgating(GpuSubdevice *, std::string);
    UINT32 fgc6_exit(GpuSubdevice *);
    UINT32 fgc6_intrGpuEventTest(int program_additional_wakeups=0);
    UINT32 fgc6_gpioWakeupTest(std::string, int);
    UINT32 fgc6_deepl1_setup(GpuSubdevice *);
    UINT32 fgc6_l1_setup(GpuSubdevice *);
    UINT32 rtd3_wakeupTest(bool additional_wakeup_events=false,
			      bool timer_wakeup_enabled=true);
    UINT32 rtd3_setup(GpuSubdevice *);
    
    UINT32 rtd3_sli_Clockgating(GpuSubdevice *, std::string);
    UINT32 rtd3_sli_entry(GpuSubdevice *);
    UINT32 rtd3_sli_exit(GpuSubdevice *); 
    UINT32 gc6plus_XUSB_PPC_wakeup(bool additional_wakeup_events=false,
			      bool timer_wakeup_enabled=true);
    UINT32 gc6plus_intrGpuEventTest(int program_additional_wakeups=0);
    UINT32 gc6plus_gpioWakeupTest(std::string, int);
    UINT32 gc6plus_swWakeupTest();
    UINT32 gc6plus_gc6EntryTest();
    UINT32 gc6plus_pcieWakeupTest();
    UINT32 gc6plus_clampTest();
    int program_additional_wakeups(GpuSubdevice *, int );

    UINT32 gc6plus_bsi2pmu_maxwellTest(string &fname);
    UINT32 gc6plus_bsi2pmu_32_phases_maxwellTest(string &fname);
    UINT32 gc6plus_bsiram_content(GpuSubdevice *dev, string &fname);
    UINT32 gc6plus_bsiram_content_autoincr(GpuSubdevice *dev, string &fname);
    UINT32 gc6plus_i2ctherm_Test();
    UINT32 gc6plus_gc6_exit_i2c_Test();
    UINT32 gc6plus_gc6_exit_i2c_timeout_Test();
    UINT32 gc6plus_gc6_abort_i2c_Test();
    UINT32 gc6plus_sci_i2c_Test();
    UINT32 gc6plus_simultaneous_i2c_gpio_Test();
    UINT32 gc6plus_bsi2pmu_bar0_maxwellTest(string &fname);
    
    // multiple wakeups Tests
    UINT32 multiple_wakeupsTest();
    
    //Reset Tests
    UINT32 gc6plus_pexResetGC5Test();

    UINT32 gc6plus_systemResetTest();
    UINT32 gc6plus_systemResetCYATest();
    UINT32 gc6plus_fundamentalResetTest();

    UINT32 gc6plus_softwareResetTest(); 
   
    int gc6plus_i2cRdByte(int, int, UINT32*, int, GpuSubdevice *, int);
    int gc6plus_i2cRdByte_timeout(int, int, UINT32*, int, GpuSubdevice *, int);
    int gc6plus_i2cWrByte(int, int, UINT32, int);
    int SetThermI2csValid(GpuSubdevice *);
    UINT32 check_clamp_gc6plus(bool fuse_blown);
    int VGA_INDEXED_WRITE(UINT32 addr, UINT32 data);
    int ReadWriteVal(GpuSubdevice *, UINT32 reg, UINT32 dataMsk, UINT32 data);
    int WriteReadCheckVal(UINT32 reg, UINT32 regMsk, const char *regName, UINT32 data);
    int ReadCheckVal(UINT32 reg, UINT32 regMsk, const char *regName, UINT32 data); 
    int ChkRegVal2(UINT32 reg, UINT32 regMsk, const char *regName, UINT32 data);
    int ChkRegVal3(UINT32 reg, UINT32 regMsk, const char *regName, UINT32 data);
    
    UINT32 ModifyFieldVal(IRegisterClass*, UINT32, string regname, string fieldname, string valuename);
    int ReadWriteRegFieldVal(GpuSubdevice*, IRegisterMap*, string regname, string fieldname, string valuename);
    int WriteRegFieldVal(GpuSubdevice*, IRegisterMap*, string regname, string fieldname, string valuename);
    UINT32 ModifyFieldValWithData(IRegisterClass*, UINT32 reg_data, string regname, string fieldname, UINT32);
    UINT32 ReadReg(GpuSubdevice*, IRegisterMap*, string regname);
    
    int IFF_write_check(GpuSubdevice *);
    int IFR_write_check(GpuSubdevice *);
    bool IFF_read_check(GpuSubdevice *);
    bool IFR_read_check(GpuSubdevice *); 


    int priv_init(GpuSubdevice *);
    int init_ptimer(GpuSubdevice *);
    int configure_power_sequencer(GpuSubdevice *);
    int configure_gpios(GpuSubdevice *);
    int program_gpu_event_wake(GpuSubdevice *, bool enable_wake = true);
    int ramInit(GpuSubdevice *, string &fname);
    int pcieInit(GpuSubdevice *);
    int pcieRestore();
    int gc6plus_cleanup(GpuSubdevice *);
    int setClkReq(GpuSubdevice *, int);
    string bsiRamFileName;
    int bsiRamFile_check, i2ctherm_check, gc6_exit_i2c_check, gc6_exit_i2c_timeout_check, gc6_abort_i2c_check, gc6_sci_i2c_check, i2c_gpio_check;
    int gc6plus_triggerMode(GpuSubdevice *pSubdev);
    int gc5Mode(GpuSubdevice *);
    int gc6Mode(GpuSubdevice *);
    int fgc6Mode(GpuSubdevice *);
    int rtd3Mode(GpuSubdevice *);
    int cyaMode(GpuSubdevice *);
    int assertGpio(GpuSubdevice *, std::string input, int index);
    int deassertGpio(GpuSubdevice *, std::string input, int index);
    int gpio_index(std::string , int);
    int setGpioDrive(int, UINT32);
    int setGpioPull(int, UINT32);
    int assertSwEnables(GpuSubdevice *);
    int program_ec_mode(GpuSubdevice *);
    int enableXusbPpcEvent(GpuSubdevice *);
    int enabledFunctionPme_enable(GpuSubdevice *pSubdev);

    UINT32 toggleSystemReset();

    UINT32 getEntryTimer(GpuSubdevice *);
    UINT32 getResidentTimer(GpuSubdevice *);
    UINT32 getExitTimer(GpuSubdevice *);
    UINT32 checkTimers(GpuSubdevice *);
    UINT32 getSwIntr0Status(GpuSubdevice *);
    UINT32 getSwIntr1Status(GpuSubdevice *);
    int printSwIntrXStatus(GpuSubdevice *);
    UINT32 getState(GpuSubdevice *);
    UINT32 checkSteadyState(GpuSubdevice *);
    int waitForSteadyState(GpuSubdevice *);
    int waitForGC5(GpuSubdevice *);
    int waitForGC6(GpuSubdevice *);
    UINT32 checkDebugTag(GpuSubdevice *);
    UINT32 check_sw_intr_status(GpuSubdevice *);


// IFF and IFR related code
int write_read_check(GpuSubdevice *,char* reg,UINT32 and_mask,UINT32 or_mask);
int check_rmw(GpuSubdevice *, char* reg,UINT32 and_mask,UINT32 or_mask,int iff_ifr);
int check_dw(GpuSubdevice *pSubdev,char* reg,UINT32 expected_val,int iff_ifr);
int initfromfuse_gc6;
int initfromfuse_gc6_check;
int initfromrom_gc6;
int initfromrom_gc6_check;


    int fs_control;
    int fs_control_gpc;
    int fs_control_gpc_tpc;
    int fs_control_gpc_zlwll;
    int register_check;
    int rpg_pri_error_check;
    int as2_speedup_check;
    int register_unmapped_check;
    int randomize_regs;
    int ptimer_check;
    int sw_force_wakeup_check;
    int pcie_check;
    int clkreq_in_gc6_check;
    int clamp_check;
    int fgc6_wake_test;
    int cya_mode;
    int arm_bsi_cya_gc6_mode_check;
    int arm_bsi_cya_fgc6_mode_check;
    int multiple_wakeups;

    int is_fmodel;
    int is_fgc6;
    int is_fgc6_l2;
    int is_rtd3_sli;

    int wakeup_timer_check;
    int wakeup_timer_us;
    int xusb_ppc_event_check;
    int map_misc_0_check;
    int misc_0_gpio_pin;
    int priv_access_check;
    int gpio_wakeup_check;
    int simultaneous_gpio_wakeup;
    int simultaneous_gpio_wakeup_delay_ns;
    int intr_gpu_event_check;
    int fgc6_intr_gpu_event_check;
    int intr_pex_reset_check;
    int gpu_event_clash_check;
    int abort_skip_disable_check;
    int entry_delay_us;
    int exit_delay_us;
    bool enab_system_reset_mon;
    bool enab_rand_lwvdd_ok_offset;
    UINT32 m_lwvdd_ok_val;
    UINT32 m_lwvdd_ok_offset;
    int ppc_rtd3_wake;
    int xusb_rtd3_wake;
    int rtd3_wakeup_timer_check;
    int rtd3_cpu_wakeup_check;
    int rtd3_hpd_test;
    int rtd3_system_reset_check;
   
    int hpd_test;
    int fgc6_hpd_test;
    int hpd_gpio_num;
    int gpio;
    int pcie;
    UINT32 m_seed;
    int system_reset_check;
    int fundamental_reset_check;
    int software_reset_check; 
    int bsi2pmu_check;
    int gpio_index_misc[4];
    int gpio_index_hpd[7];

    int gpio_index_fb_clamp;
    int gpio_index_power_alert;
    int gpio_index_fsm_gpu_event;
    int gpio_index_msvdd_vid_pwm;
    int gpio_index_lwvdd_vid_pwm;
    int gpio_index_main_3v3_en;
    int gpio_index_lwvdd_pexvdd_en;
    int gpio_index_fbvddq_gpu_en;
    int gpio_index_fbvdd_q_mem_en;
    int gpio_index_pexvdd_vid;
    int gpio_index_gpu_gc6_rst;
    int gpio_index_sys_rst_mon;
    int bsi2pmu_32_phases_check;
    int bsi2pmu_bar0_check;
    int bsi_padctrl_in_gc6_check; 
    int svart_err_check; 
    
    int sw_pwr_seq_vector;
    int halt_vector;
    int pwr_seq_end_vector;
    int debug_tag_data;
    
    int WrRegArray(int index, const char *regName, UINT32 data);
    UINT32 RdRegArray(int index, const char *regName);
    
    RandomStream* m_rndStream;

    int DelayNs(UINT32 LwClks);
    int senseFuse(void);
    int programFuse(UINT32 addr, UINT32 data, UINT32 mask);
    int updateFuseopt(void);
    int Enable_fuse_program(void);
    int PrivInit(void);
    UINT32 poll_for_fsm_state(const char *state, bool equal);
    UINT32 m_ErrCount;
    UINT32 m_ec_reset_width_us;
    UINT32 m_sci_gpu_event_width_us;
    int m_ec_gpu_event_width_us;
    UINT32 m_polling_timeout_us;

private:
    typedef enum {
    GC5,
    GC6,
    FGC6,
    CYA,
    STEADY,
    INIT_MODE,
    RTD3,
    }ModeType;

    typedef enum {
    ENTRY,
    EXIT,
    ABORT,
    RESET,
    INIT_SEQ,
    }SeqType;

    typedef enum {
    NONE,
    L1,
    L2,
    Deep_L1,
    WAKEUP_TIMER,
    GPU_EVENT,
    HPD_WAKEUP,
    }Fgc6Param;
    
    static GC6plus_Maxwell::ModeType mode;
    static GC6plus_Maxwell::SeqType seq;
    static GC6plus_Maxwell::Fgc6Param fgc6_wakeup_type;
    static GC6plus_Maxwell::Fgc6Param fgc6_mode;
    IRegisterMap* m_regMap;  
    unique_ptr<PciCfgSpace> m_pPciCfg;
    ArgReader *params;
    int gc6plus_StdWakeupTest(func_pointer , func_pointer , func_pointer );
    int configurePowerSequencer(GpuSubdevice *pSubdev, ModeType mode);
};


class multiple_wakeups_main_body: public func_pointer{
    int delay_ns;
public:
    multiple_wakeups_main_body(int);
    int operator()(LWGpuSingleChannelTest *, GpuSubdevice *);
};

class multiple_wakeups_enable: public func_pointer{
public:
    multiple_wakeups_enable(){};
    int operator()(LWGpuSingleChannelTest *, GpuSubdevice *);
};
    

// typedef int (*func_pointer)(GC6plus_Maxwell *, GpuSubdevice *);
int no_op_func(GC6plus_Maxwell *, GpuSubdevice *);
#ifdef MAKE_TEST_LIST
CREATE_TEST_LIST_ENTRY(gc6plus_maxwell, GC6plus_Maxwell, "GPU GC6plus_Maxwell Test");
#undef TEST_LIST_HEAD
#define TEST_LIST_HEAD &gc6plus_maxwell_testentry
#endif

#endif  // _GC6plus_Maxwell_H_
