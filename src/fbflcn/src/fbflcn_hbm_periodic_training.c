/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2020-2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */
#include <falcon-intrinsics.h>
#include "fbflcn_defines.h"
#include "fbflcn_helpers.h"
#include "fbflcn_hbm_periodic_training.h"
#include "dev_pri_ringstation_fbp.h"
#if (!FBFALCONCFG_FEATURE_ENABLED(SUPPORT_NITROC))
#include "fbflcn_table.h"
#include "dev_trim.h"
#include "priv.h"
#include "dev_fbpa.h"
#include "dev_gc6_island.h"
#include "fbflcn_interrupts.h"
#include "fbflcn_mutex.h"
#include "osptimer.h"
#include "dev_pmgr.h"
#else
#include "lw_ref_dev_fbpa.h"
#include "fbflcn_tb_only.h"
typedef LwS32 LwSFXP24_8;
LwU32 gbl_mclk_freq_final = 0;
#endif
#include "lwfixedtypes.h"


//LwU32 periodic_tr_cnt = 0;    // not used lwrrently; for ecs_ratio
LwU32 last_train_time = 0;      // used to compare last time when periodic training ran
LwU32 wrck_period_ps = 20000;   // set in getPeriodicVard() from reading clk regs, 50MHz = 20000

//need 6; one for each HBM site
LwSFXP24_8 baseline_wosc[MAX_SITES] = {0};

LwBool periodic_tr_inited = LW_FALSE;
LwBool periodic_tr_started = LW_FALSE;
extern HBM_DEVICE_ID_STRUCT hbm_device_id;   // for workaround
//VBIOS variables
LwU8 ecs_ratio = 0;
LwU8 wosc_run_time_us = 1;
LwU8 periodic_tr_interval_us = 0;      // also used to disable (0)
LwU8 hw_error_count = 0;
LwU8 wosc_ilwalid_count = 0;
#define MAX_PERIODIC_TR_HW_ERROR    5
#define MAX_PERIODIC_TR_WOSC_ERROR  5

void clearLastTime(void)
{
    if (periodic_tr_interval_us > 0)
    {
        last_train_time = 0;
    }
}

void callwlateWosc(WOSC_STRUCT *wosc, LwSFXP24_8 *ratio_ps)
{
    LwU8 s;
    LwU32 rtl_wosc_count;
    LwU32 ieee_wosc_count;
    LwU32 rtl_measured_time;

    // perform callwlation on HBM sites...
    for (s=0; s < MAX_SITES; s++)
    {
        if (hbm_bcast_data[s].enabled == LW_TRUE)
        {
            // this is from our HW count of wck between the start and stop of wosc
            rtl_wosc_count = wosc->rtl_count;
            ieee_wosc_count = wosc->ieee_count;

            // register data format ([31:8]=value, [7]=valid)
            LwU8 ieee_valid = (ieee_wosc_count >> 7) & 0x1;

            // check if valid
            if ((rtl_wosc_count != 0) && (ieee_valid != 0))
            {
                ieee_wosc_count = ieee_wosc_count >> 8;

                // check overflow?
                rtl_measured_time = wrck_period_ps * rtl_wosc_count;

                LwU32 ratio_divide_by;
                // Hynix errata (HYNIX 2021W43) for wosc
                // 2021W43 <= ES1, workaround needed
                if (hbm_device_id.hynix_es1_workaround)
                {
                    ratio_divide_by = ieee_wosc_count; //es1
                }
                else
                {
                    ratio_divide_by = 2 * ieee_wosc_count; //es2
                }
/*
                if (s == 4)
                {
                    FW_MBOX_WR32(1, ieee_wosc_count);
                    if (hbm_device_id.hynix_es1_workaround)
                    {
                        FW_MBOX_WR32(2, ratio_divide_by|0x1);
                    }
                    else
                    {
                        FW_MBOX_WR32(2, (ratio_divide_by|0x2);
                    }
                }
*/
                // rshift 8 for SFXP24.8
                ratio_ps[s] = (rtl_measured_time << 8) / ratio_divide_by;
                NIO_PRINT("HBM PER_TR - ratio[%d] = %d", s, ratio_ps[s]);

                // debug
                //FW_MBOX_WR32(3, rtl_wosc_count);
                //FW_MBOX_WR32(4, ieee_wosc_count);
                //FW_MBOX_WR32(5, rtl_measured_time);
                //FW_MBOX_WR32(6, (wrck_period_ps * rtl_wosc_count)/(2 * ieee_wosc_count));
                //FW_MBOX_WR32(7, ratio_ps[s]);
            }
            else
            {
                if (rtl_wosc_count == 0)
                {
                    //FW_MBOX_WR32(12, (s<<28)|FBFLCN_ERROR_CODE_HW_WOSC_COUNT_EQ_0);
                    hw_error_count++;
                }

                if (ieee_valid == 0)
                {
                    //FW_MBOX_WR32(13, (s<<28)|FBFLCN_ERROR_CODE_IEEE_WOSC_COUNT_ILWALID);
                    wosc_ilwalid_count++;
                }
            } 
        }
        wosc++;
    }

    if (hw_error_count >= MAX_PERIODIC_TR_HW_ERROR)
    {
        FW_MBOX_WR32(11, hw_error_count);
        FBFLCN_HALT(FBFLCN_ERROR_CODE_HW_WOSC_COUNT_EQ_0);
    }
    if (wosc_ilwalid_count >= MAX_PERIODIC_TR_WOSC_ERROR)
    {
        FW_MBOX_WR32(11, wosc_ilwalid_count);
        FBFLCN_HALT(FBFLCN_ERROR_CODE_IEEE_WOSC_COUNT_ILWALID);
    }
}

void readIeeeWosc(WOSC_STRUCT* wosc)
{
    //LwU32 i1500_status;
    LwU8 s, p;
    LwU32 instr;

    NIO_PRINT("HBM PER_TR - inside readIeeeWosc");
    // lowsec_mask written in dev_init from HS

    // fence to protect multicast
    REG_WR32_STALL(BAR0, LW_PPRIV_FBP_FBPS_PRI_FENCE, 0x0);                         

    // start WOSC
    NIO_PRINT("HBM PER_TR - starting wosc");
    REG_WR32_STALL(BAR0, multicastMasterWriteFromBrodcastRegister(LW_PFB_FBPA_FBIO_HBM_TEST_I1500_INSTR), (WIR_DWORD_MODE|WIR_WOSC_RUN));
    REG_WR32_STALL(BAR0, multicastMasterWriteFromBrodcastRegister(LW_PFB_FBPA_FBIO_HBM_TEST_I1500_MODE),(ONE_DWORD_NEEDED|0x1));
    REG_WR32_STALL(BAR0, multicastMasterWriteFromBrodcastRegister(LW_PFB_FBPA_FBIO_HBM_TEST_I1500_DATA_LOWSEC),0x1);

    //for (s=0; s < MAX_SITES; s++)
    //{
    //    if (hbmSiteEnabled[s] == LW_TRUE)
    //    {
    //        REG_WR32_STALL(BAR0, unicast(LW_PFB_FBPA_FBIO_HBM_TEST_I1500_INSTR,p), (WIR_DWORD_MODE|WIR_WOSC_RUN));
    //        REG_WR32_STALL(BAR0, unicast(LW_PFB_FBPA_FBIO_HBM_TEST_I1500_MODE,p),(ONE_DWORD_NEEDED|0x1));
    //        REG_WR32_STALL(BAR0, unicast(LW_PFB_FBPA_FBIO_HBM_TEST_I1500_DATA_LOWSEC,p),0x1);
    //    }
    //}

    // wait for specified time
    OS_PTIMER_SPIN_WAIT_US(wosc_run_time_us);

    // stop WOSC
    NIO_PRINT("HBM PER_TR - stopping wosc");
    REG_WR32_STALL(BAR0, multicastMasterWriteFromBrodcastRegister(LW_PFB_FBPA_FBIO_HBM_TEST_I1500_INSTR), (WIR_DWORD_MODE|WIR_WOSC_RUN));
    REG_WR32_STALL(BAR0, multicastMasterWriteFromBrodcastRegister(LW_PFB_FBPA_FBIO_HBM_TEST_I1500_MODE),(ONE_DWORD_NEEDED|0x1));
    REG_WR32_STALL(BAR0, multicastMasterWriteFromBrodcastRegister(LW_PFB_FBPA_FBIO_HBM_TEST_I1500_DATA_LOWSEC),0);

    // write instruction to read count
    // Check the design wosc_count
    // needed?
    //Bobby suggested a read here, before WOSC_COUNT read to ensure state machine doesn't break if a priv access happens

    //REG_WR32(BAR0, multicastMasterWriteFromBrodcastRegister(LW_PFB_FBPA_FBIO_HBM_TEST_I1500_INSTR), WIR_WOSC_COUNT);
    //REG_WR32(BAR0, multicastMasterWriteFromBrodcastRegister(LW_PFB_FBPA_FBIO_HBM_TEST_I1500_MODE),25);

//    instr = WIR_DWORD_MODE | WIR_WOSC_COUNT;
//    REG_WR32_STALL(BAR0, multicastMasterWriteFromBrodcastRegister(LW_PFB_FBPA_FBIO_HBM_TEST_I1500_INSTR), instr);
//    REG_WR32_STALL(BAR0, multicastMasterWriteFromBrodcastRegister(LW_PFB_FBPA_FBIO_HBM_TEST_I1500_MODE), (ONE_DWORD_NEEDED | 25));

    // add fence to protect multicast, also needed for PA falcon
    REG_WR32_STALL(BAR0, LW_PPRIV_FBP_FBPS_PRI_FENCE, 0x0);                         

    // Check the design wosc_count
    // needed?
    //Bobby suggested a read here, before WOSC_COUNT read to ensure state machine doesn't break if a priv access happens
    // read flush on all PA's
    //for (p=0; p < MAX_PARTS; p++) {
    //    if (isPartitionEnabled(p) == LW_TRUE)
    //    {
    //        REG_RD32(BAR0, unicast(LW_PFB_FBPA_HBM_CFG0, p));     
    //    }
    //}

    //OS_PTIMER_SPIN_WAIT_US(wosc_run_time_us);

    // read HBM sites...
    for (s=0; s < MAX_SITES; s++)
    {
        if (hbm_bcast_data[s].enabled == LW_TRUE)
        {
            p = hbm_bcast_data[s].pa[0];
            NIO_PRINT("HBM PER_TR - p = %d ", p);
            // note channel in our case is 0:15, channel select is 11:8 and bit 14 used for channel broadcast
            instr = WIR_DWORD_MODE | ((hbm_bcast_data[s].valid_channel & 0xF) << 8) | WIR_WOSC_COUNT;

            //REG_WR32_STALL(BAR0, unicast(LW_PFB_FBPA_FBIO_HBM_TEST_I1500_INSTR,p), (WIR_DWORD_MODE|WIR_WOSC_RUN));
            //REG_WR32_STALL(BAR0, unicast(LW_PFB_FBPA_FBIO_HBM_TEST_I1500_MODE,p),(ONE_DWORD_NEEDED|0x1));
            //REG_WR32_STALL(BAR0, unicast(LW_PFB_FBPA_FBIO_HBM_TEST_I1500_DATA_LOWSEC,p),0);

            REG_WR32_STALL(BAR0, unicast(LW_PFB_FBPA_FBIO_HBM_TEST_I1500_INSTR, p), instr);
            REG_WR32_STALL(BAR0, unicast(LW_PFB_FBPA_FBIO_HBM_TEST_I1500_MODE, p), (ONE_DWORD_NEEDED | 25));

            // get IEEE1500 wosc_count
            wosc->ieee_count = REG_RD32(BAR0, unicast(LW_PFB_FBPA_FBIO_HBM_TEST_I1500_DATA, p));

            // this is from our HW count of wck between the start and stop of wosc
            wosc->rtl_count = REG_RD32(BAR0, unicast(LW_PFB_FBPA_FBIO_HBM_TEST_I1500_WOSC_COUNT, p));

            //debug
            //if (s == 4) {
            if ((wosc->rtl_count == 0) || (wosc->ieee_count == 0)) {
                //FW_MBOX_WR32(12, wosc->ieee_count);
                FW_MBOX_WR32(8,REG_RD32(BAR0, unicast(LW_PFB_FBPA_FBIO_HBM_TEST_I1500_WOSC_COUNT, p)));
                FW_MBOX_WR32(9,REG_RD32(BAR0, unicast(LW_PFB_FBPA_FBIO_HBM_TEST_I1500_WOSC_COUNT, p)));
                FW_MBOX_WR32(10,REG_RD32(BAR0, unicast(LW_PFB_FBPA_FBIO_HBM_TEST_I1500_WOSC_COUNT, p)));
                FW_MBOX_WR32(11, (p<<24)|unicast(LW_PFB_FBPA_FBIO_HBM_TEST_I1500_INSTR, p));
                FW_MBOX_WR32(13,wosc->ieee_count);
                FBFLCN_HALT(FBFLCN_ERROR_CODE_HW_WOSC_COUNT_EQ_0);
            }
            NIO_PRINT("HBM PER_TR - rtl_wosc_count = %d ieee_wosc_count = %d index %d", wosc->rtl_count,  wosc->ieee_count,s);
        }
        wosc++;
    }
}

/*
 * not used
void readEcs(void)
{
    //TODO need to fix/check this FN
    // start reading error check and scrub info via IEEE1500
    LwU8 s = 0;
    LwU32 instr = ((hbm_bcast_data[s].valid_channel & 0xF) << 8) | WIR_ECS_ERROR_LOG;
    REG_WR32(BAR0, multicastMasterWriteFromBrodcastRegister(LW_PFB_FBPA_FBIO_HBM_TEST_I1500_INSTR), instr);
    REG_WR32(BAR0, multicastMasterWriteFromBrodcastRegister(LW_PFB_FBPA_FBIO_HBM_TEST_I1500_MODE),0x1);
    REG_WR32(BAR0, multicastMasterWriteFromBrodcastRegister(LW_PFB_FBPA_FBIO_HBM_TEST_I1500_DATA_LOWSEC),0xFFFFFFFF);

    // data length = 216
    // [0]      sid0_pc0_ecs_valid
    // [26:1]   sid0_pc0_ecs[25:0]
    // [27]     sid0_pc1_ecs_valid
    // [53:28]  sid0_pc1_ecs[25:0]
    // [54]     sid1_pc0_ecs_valid
    // [80:55]  sid1_pc0_ecs[25:0]... till sid3_pc1
    REG_WR32(BAR0, multicastMasterWriteFromBrodcastRegister(LW_PFB_FBPA_FBIO_HBM_TEST_I1500_INSTR), WIR_WOSC_RUN);
    REG_WR32(BAR0, multicastMasterWriteFromBrodcastRegister(LW_PFB_FBPA_FBIO_HBM_TEST_I1500_MODE),0x1);
    REG_WR32(BAR0, multicastMasterWriteFromBrodcastRegister(LW_PFB_FBPA_FBIO_HBM_TEST_I1500_DATA_LOWSEC),0);
}
*/

// was using MUTEX_REQ_TIMEOUT_NS = 50000 before
#define PERIODIC_TR_MUTEX_REQ_TIMEOUT_NS  1000000
void readPeriodicTr(LwSFXP24_8* wosc_ratio)
{
    NIO_PRINT("HBM PER_TR : inside readPeriodicTr \n");
    LwU32 max_time_for_2_runs = (periodic_tr_interval_us * 2 * NS_PER_TIMER_TICKS) << US_SHIFT_TO_TIMER_TICKS;
    WOSC_STRUCT wosc_values[6];

    //extern LwBool gbl_in_mclk_sw;
    //FW_MBOX_WR32(2,gbl_in_mclk_sw);
    // Acquire mutex
    LwU8 token = 0;
    MUTEX_HANDLER ieee1500Mutex;
    ieee1500Mutex.registerMutex = IEEE1500_MUTEX_REG(IEEE1500_MUTEX_INDEX);
    ieee1500Mutex.registerIdRelease = IEEE1500_MUTEX_ID_RELEASE;
    ieee1500Mutex.registerIdAcquire = IEEE1500_MUTEX_ID_ACQUIRE;
    ieee1500Mutex.valueInitialLock = IEEE1500_MUTEX_REG_INITIAL_LOCK;
    ieee1500Mutex.valueAcquireInit = IEEE1500_MUTEX_ID_ACQUIRE_VALUE_INIT;
    ieee1500Mutex.valueAcquireNotAvailable = IEEE1500_MUTEX_ID_ACQUIRE_VALUE_NOT_AVAIL;

    NIO_PRINT("HBM PER_TR : acquiring ieee1500 mutex \n");
    LW_STATUS status = mutexAcquireByIndex_GV100(&ieee1500Mutex, PERIODIC_TR_MUTEX_REQ_TIMEOUT_NS, &token);
    NIO_PRINT("HBM PER_TR : acquired ieee1500 mutex \n");

    if (status == LW_OK)
    {
        LwU32 lwrrent_time = REG_RD32(CSB, LW_CFBFALCON_FALCON_PTIMER0);
        //FW_MBOX_WR32(7, lwrrent_time);

        if ((last_train_time != 0) && ((lwrrent_time - last_train_time) > max_time_for_2_runs))
        {
/*            FW_MBOX_WR32(8, last_train_time);
            FW_MBOX_WR32(9, max_time_for_2_runs);
            FW_MBOX_WR32(10,FBFLCN_ERROR_CODE_PERIODIC_TIMEOUT_EXCEEDED);
*/            //FBFLCN_HALT(FBFLCN_ERROR_CODE_PERIODIC_TIMEOUT_EXCEEDED);
        }

        NIO_PRINT("HBM PER_TR - calling readIeeeWosc task");
        readIeeeWosc(&wosc_values[0]);
        NIO_PRINT("HBM PER_TR - readIeeeWosc task done");

        if (ecs_ratio != 0)
        {
        /* Not used
            periodic_tr_cnt++;

            if (periodic_tr_cnt >= ecs_ratio)
            {
                readEcs();
                periodic_tr_cnt = 0;
            }
        */
        }

        // release mutex
        status = mutexReleaseByIndex_GV100(&ieee1500Mutex, token);
        if(status != LW_OK) {
            FBFLCN_HALT(FBFLCN_ERROR_CODE_PERIODIC_TRAINING_GPIO_MUTEX_NOT_RELEASED);
        }
        NIO_PRINT("HBM PER_TR - mutex released");

        // do callwlation / value checks
        NIO_PRINT("HBM PER_TR - Callwlate wosc ratio");
        callwlateWosc(&wosc_values[0], &wosc_ratio[0]);
    }
    else    // mutex error
    {
        FW_MBOX_WR32(7, status);
        FW_MBOX_WR32(8, PERIODIC_TR_MUTEX_REQ_TIMEOUT_NS);
        status = mutexReleaseByIndex_GV100(&ieee1500Mutex, token);
        FBFLCN_HALT(FBFLCN_ERROR_CODE_MCLK_SWITCH_MUTEX_NOT_OBTAINED);
    }

    last_train_time = REG_RD32(CSB, LW_CFBFALCON_FALCON_PTIMER0);
}

void getPeriodicVars(void)
{
    if (periodic_tr_inited == LW_FALSE)
    {
        BOOT_TRAINING_HBM_PERIODIC_TRAINING *pHBMTrainingPeriodicTr = &gTT.bootTrainingTable.PeriodicTr;

        ecs_ratio = pHBMTrainingPeriodicTr->ecs_read_ratio;
        wosc_run_time_us = pHBMTrainingPeriodicTr->oscillator_run_time; // 1
        periodic_tr_inited = LW_TRUE;

#if (FBFALCONCFG_FEATURE_ENABLED(VBIOS_INCLUDED_IN_BINARY))
//temp disable for debug
        periodic_tr_interval_us = 0;        // force to 0 (disable) for ATE
        return;
#else
        periodic_tr_interval_us = pHBMTrainingPeriodicTr->interval;     // 20
#endif

        if (gTT.bootTrainingTable.MemBootTrainTblFlags.flag_skip_boot_training == LW_TRUE)
        {
            periodic_tr_interval_us = 0;        // force to 0 (disable) when skip boot is set 
            return;
        }

        // determine wrck_period_ps
        // 11:5
        LwU32 testmacro_div_sel = DRF_VAL(_PTRIM, _FBPA_BCAST_FBIO,
          _HBM_TESTMACRO_CLK_DIV_SEL, REG_RD32(BAR0, LW_PTRIM_FBPA_BCAST_FBIO));

        LwU32 dramclk_alt_switch = REG_RD32(BAR0,
                LW_PTRIM_SYS_DRAMCLK_ALT_SWITCH_DIVIDER);

        // 5:0, 0x1e = 30; div_by = 16 (30/2+1)
        LwU32 divider_sel_div_by = (DRF_VAL(_PTRIM, _SYS_DRAMCLK_ALT_SWITCH_DIVIDER,
                _DIVIDER_SEL, dramclk_alt_switch) / 2) + 1;

        // 15:12
        LwU32 clock_source_sel = DRF_VAL(_PTRIM, _SYS_DRAMCLK_ALT_SWITCH_DIVIDER,
                _CLOCK_SOURCE_SEL, dramclk_alt_switch);

        LwU32 source_clk_freq_mhz = (clock_source_sel == 0x4) ? 1620 : 27;

        wrck_period_ps = (1000000 * divider_sel_div_by * testmacro_div_sel) / source_clk_freq_mhz;
        NIO_PRINT("HBM PER_TR - wrck_period_ps %d", wrck_period_ps);
        //FW_MBOX_WR32(2, (wrck_period_ps<<16)|periodic_tr_interval_us);


        LwU8 config_hbm = REF_VAL(LW_PFB_FBPA_FBIO_BROADCAST_DDR_MODE,
                        REG_RD32(BAR0, LW_PFB_FBPA_FBIO_BROADCAST));

        // MAX_SITES = 6;       total HBM sites
        // MAX_PARTS = 24;      total partitions in chip
        // PARTS_PER_SITE = 4; available paritions per site
        // CHANNELS_IN_PA = 4;  available channels per partition

        if (config_hbm == LW_PFB_FBPA_FBIO_BROADCAST_DDR_MODE_HBM2)
        {
            periodic_tr_interval_us = 0;        // disable since not supported on hbm2
        }
    }
}

void getBaselineWosc(void)
{
    getPeriodicVars();

    if ((periodic_tr_interval_us > 0) && (0xDEADDEAD!=REG_RD32(BAR0, LW_PFB_FBPA_FALCON_MONITOR)))
    {

        NIO_PRINT("HBM_PER_TR_SEQ -Baseline wosc\n");
        // check if need to disable if lower freq, or handled in vbios?
        readPeriodicTr(&baseline_wosc[0]);

        // removed use of Scratch group 26/27 for oscillator values as its not needed for single binary
#if (!FBFALCONCFG_FEATURE_ENABLED(SUPPORT_NITROC))
        // store baseline wosc values for runtime use later, not needed for single binary
        //REG_WR32(BAR0, LW_PGC6_AON_SELWRE_SCRATCH_GROUP_26(0), baseline_wosc[0]);
        //REG_WR32(BAR0, LW_PGC6_AON_SELWRE_SCRATCH_GROUP_26(1), baseline_wosc[1]);
        //REG_WR32(BAR0, LW_PGC6_AON_SELWRE_SCRATCH_GROUP_26(2), baseline_wosc[2]);
        //REG_WR32(BAR0, LW_PGC6_AON_SELWRE_SCRATCH_GROUP_26(3), baseline_wosc[3]);
        //REG_WR32(BAR0, LW_PGC6_AON_SELWRE_SCRATCH_GROUP_27(0), baseline_wosc[4]);
        //REG_WR32(BAR0, LW_PGC6_AON_SELWRE_SCRATCH_GROUP_27(1), baseline_wosc[5]);
#else
        //There will be a single ucode where scratch registers aren't needed. Hence, not putting effort
        //to implement _AON_SELWRE_SCRATCH_GROUP
        REG_WR32(BAR0, LW_CFBFALCON_FIRMWARE_MAILBOX(10), baseline_wosc[0]);
        REG_WR32(BAR0, LW_CFBFALCON_FIRMWARE_MAILBOX(11), baseline_wosc[1]);
        REG_WR32(BAR0, LW_CFBFALCON_FIRMWARE_MAILBOX(12), baseline_wosc[2]);
        REG_WR32(BAR0, LW_CFBFALCON_FIRMWARE_MAILBOX(13), baseline_wosc[3]);
        REG_WR32(BAR0, LW_CFBFALCON_FIRMWARE_MAILBOX(14), baseline_wosc[4]);
        REG_WR32(BAR0, LW_CFBFALCON_FIRMWARE_MAILBOX(15), baseline_wosc[5]);
#endif

        last_train_time=0;      // clear last time check
    }
}

// called from main.c
void initPeriodicTraining(void)
{
    getPeriodicVars();

    if (periodic_tr_interval_us > 0)
    {
        //Set force periodic updates to 0 for periodic training
        LwU32 trng_msc = REG_RD32(BAR0, LW_PFB_FBPA_FBIO_HBM_TRNG_MISC);
        trng_msc = FLD_SET_DRF_NUM(_PFB, _FBPA_FBIO_HBM_TRNG_MISC, _OB_DQ_FORCE_PERIODIC_UPDATES, 0, trng_msc);
        REG_WR32(BAR0, LW_PFB_FBPA_FBIO_HBM_TRNG_MISC, trng_msc);

        // removed use of Scratch group 26/27 for oscillator values as its not needed for single binary
#if (!FBFALCONCFG_FEATURE_ENABLED(SUPPORT_NITROC))
        // restore baseline values from boot time training, not needed for single binary
        //baseline_wosc[0] = REG_RD32(BAR0, LW_PGC6_AON_SELWRE_SCRATCH_GROUP_26(0));
        //baseline_wosc[1] = REG_RD32(BAR0, LW_PGC6_AON_SELWRE_SCRATCH_GROUP_26(1));
        //baseline_wosc[2] = REG_RD32(BAR0, LW_PGC6_AON_SELWRE_SCRATCH_GROUP_26(2));
        //baseline_wosc[3] = REG_RD32(BAR0, LW_PGC6_AON_SELWRE_SCRATCH_GROUP_26(3));
        //baseline_wosc[4] = REG_RD32(BAR0, LW_PGC6_AON_SELWRE_SCRATCH_GROUP_27(0));
        //baseline_wosc[5] = REG_RD32(BAR0, LW_PGC6_AON_SELWRE_SCRATCH_GROUP_27(1));
#else
        baseline_wosc[0] = REG_RD32(BAR0, LW_CFBFALCON_FIRMWARE_MAILBOX(10));
        baseline_wosc[1] = REG_RD32(BAR0, LW_CFBFALCON_FIRMWARE_MAILBOX(11));
        baseline_wosc[2] = REG_RD32(BAR0, LW_CFBFALCON_FIRMWARE_MAILBOX(12));
        baseline_wosc[3] = REG_RD32(BAR0, LW_CFBFALCON_FIRMWARE_MAILBOX(13));
        baseline_wosc[4] = REG_RD32(BAR0, LW_CFBFALCON_FIRMWARE_MAILBOX(14));
        baseline_wosc[5] = REG_RD32(BAR0, LW_CFBFALCON_FIRMWARE_MAILBOX(15));
#endif

        LwU8 s;
        // check valid hbm sites && baseline, for TB mclk_sw only test
        for (s=0; s < MAX_SITES; s++)
        {
            if ((baseline_wosc[s] == 0) && (hbm_bcast_data[s].enabled == LW_TRUE))
            {
                getBaselineWosc();
                break;
            }
        }

#if (!FBFALCONCFG_FEATURE_ENABLED(SUPPORT_NITROC))
        startPeriodicTraining();
#endif
    }
}

// called from main.c and mclk_sw
void runHbmPeriodicTr()
{
    if ((periodic_tr_interval_us > 0) && (0xDEADDEAD!=REG_RD32(BAR0, LW_PFB_FBPA_FALCON_MONITOR)))
    {
        LwS32 variation;
        LwU32 trng_broadcast1;
        LwSFXP24_8 variation_ps;
        LwSFXP24_8 ob_frq_offset=0;
        LwS32 p0_freq_mhz;
#if (!FBFALCONCFG_FEATURE_ENABLED(SUPPORT_NITROC))
       p0_freq_mhz = gbl_p0_actual_mhz;
#else
       // for NitroC to use OB_FRQ_OFFSET = 0
       p0_freq_mhz = gbl_mclk_freq_final;
#endif
        LwU8 s, p, i;

        LwSFXP24_8 training_wosc[6]={0};
        NIO_PRINT("HBM_PER_TR_SEQ - runHbmPeriodicTr wosc\n");

        readPeriodicTr(&training_wosc[0]);

        LwSFXP24_8 ps_offset;
        LwU16 unsigned_ps_offset = (gTT.perfMemClkBaseEntry.spare_field1 & 0xFFFF);
        // need to sign extend if negative
        if ((unsigned_ps_offset & 0x8000) > 0)
        {
            ps_offset = (0xFF000000 | (unsigned_ps_offset << 8));
        }
        else
        {
            ps_offset = (unsigned_ps_offset << 8);
        }
        //FW_MBOX_WR32(6, ps_offset);
        //------------------------------------------------------------------------
        // no need to use floor since using integers takes care of this
        // TODO: check that the divides are ok -MW, delete this section after checking
        //LwU32 one_ui_ps = (gbl_hbm_dramclk_period/2);
        //LwS32 variation_ps = (training_wosc.ratio_ps - baseline_wosc.ratio.ps);
        //variation = ((variation_ps/one_ui_ps)*64);

        //debug
        //FW_MBOX_WR32(1, (hbm_device_id.hynix_es1_workaround<<31)|gbl_hbm_dramclk_period);    // one_ui_ps = period/2
        //FW_MBOX_WR32(2, (p0_freq_mhz<<16)|gbl_mclk_freq_final);

        for (s=0; s < MAX_SITES; s++)
        {
            if (hbm_bcast_data[s].enabled == LW_TRUE)
            {
                NIO_PRINT("HBM_PER_TR_SEQ - training_wosc  = %0d baseline_wosc = %d index %d\n", training_wosc[s], baseline_wosc[s], s);
                variation_ps = (training_wosc[s] - baseline_wosc[s]) + ps_offset;
                NIO_PRINT("HBM_PER_TR_SEQ -  variation_ps = %0d \n", variation_ps);
                // note the cast to signed must be done so that variation_ps is correct
                variation_ps = (128 * variation_ps) / (LwS32)gbl_hbm_dramclk_period;
                NIO_PRINT("HBM_PER_TR_SEQ -  variation_ps = %0d \n", variation_ps);

                //debug
                /*
                if (s == 4) {
                      FW_MBOX_WR32(3, variation_ps);
                      FW_MBOX_WR32(4, baseline_wosc[s]);
                      FW_MBOX_WR32(5, training_wosc[s]);
                }
                */
                // if new_freq = p0, OB_FRQ_OFFSET = 0;

                if (gbl_mclk_freq_final != p0_freq_mhz)
                {
                    // div by 1000 to get to GHz
                    //ob_frq_offset = baseline_wosc[s] * 2 * ((LwS32)(p0_freq_mhz - gbl_mclk_freq_final) / 1000) * 64;
                    //simplified...
                    //ob_frq_offset = ((baseline_wosc[s] * 16 * (LwS32)(p0_freq_mhz - gbl_mclk_freq_final)) / 125) / 1000;

                    LwS32 freq_diff = p0_freq_mhz - gbl_mclk_freq_final;
                    ob_frq_offset = (baseline_wosc[s] * 2 * freq_diff) / 15625;
                    variation_ps = variation_ps - ob_frq_offset;
                    //debug
                    //if (s == 4) {
                    //    FW_MBOX_WR32(6, freq_diff);
                    //    FW_MBOX_WR32(7, ob_frq_offset);
                    //    FW_MBOX_WR32(8, variation_ps);
                    //}
                }

                variation = variation_ps >> 8;  // SFXP24.8 to int

                if (variation < -128)
                {
                    variation = -128;
                }
                else if (variation > 127)
                {
                    variation = 127;
                }

                NIO_PRINT("HBM_PER_TR_SEQ -  variation = %0d  gbl_hbm_dramclk_period = %d\n", variation, gbl_hbm_dramclk_period);

                // mask out bit0=cmd(1), allow bit1=dq(0)
                REG_WR32(BAR0, LW_PFB_FBPA_FBIO_HBM_TRNG_BROADCAST_MASK, 0x1);

                p = hbm_bcast_data[s].pa[0];
                trng_broadcast1 = FLD_SET_DRF_NUM(_PFB, _FBPA_FBIO_HBM_TRNG_BROADCAST1, _OB_DDLL_OFFSET, variation, 0);

                //debug
                //if (s == 4) {
                //    FW_MBOX_WR32(9, ob_frq_offset);
                //    FW_MBOX_WR32(10, variation_ps);
                //    FW_MBOX_WR32(11, variation);
                //    //FW_MBOX_WR32(12, trng_broadcast1);
                //}

                // writes the same value to all valid PA in a site; each site could have a different value
                for (i=0; i < hbm_bcast_data[s].numValidPa; i++)
                {
                    p = hbm_bcast_data[s].pa[i];
                    REG_WR32(BAR0, unicast(LW_PFB_FBPA_FBIO_HBM_TRNG_BROADCAST1, p), trng_broadcast1);
                }

                NIO_PRINT("HBM_PER_TR_SEQ - runHbmPeriodicTr wosc done\n");
            }
        }
    }
}

void startPeriodicTraining(void)
{
    // setup external RT Timer
    if (periodic_tr_interval_us > 0)
    {
        // FIXME Temp increase of priv timeout
        REG_WR32(CSB, LW_CFBFALCON_BAR0_PRIV_TIMEOUT, 0x00FFFFFF);
        // set the intervall of timer 0
        REG_WR32(CSB, LW_CFBFALCON_TIMER_0_INTERVAL, periodic_tr_interval_us);
        // set up the timer & reset
        LwU32 ctrl = 0;
        ctrl = FLD_SET_DRF(_CFBFALCON,_TIMER_CTRL,_SOURCE,_PTIMER_1US, ctrl);
        ctrl = FLD_SET_DRF(_CFBFALCON,_TIMER_CTRL,_MODE,_ONE_SHOT, ctrl);
        LwU32 setup = FLD_SET_DRF(_CFBFALCON,_TIMER_CTRL,_RESET,_TRIGGER, ctrl);
        REG_WR32(CSB, LW_CFBFALCON_TIMER_CTRL,setup);
        // start the timer
        ctrl = FLD_SET_DRF(_CFBFALCON,_TIMER_CTRL,_GATE,_RUN, ctrl);
        REG_WR32(CSB, LW_CFBFALCON_TIMER_CTRL,ctrl);

        FalconInterruptEnablePeriodicTimer();
        periodic_tr_started = LW_TRUE;

        runHbmPeriodicTr();
    }
}

void stopPeriodicTraining(void)
{
    if (periodic_tr_started)
    {
        LwU32 ctrl = 0;
        ctrl = FLD_SET_DRF(_CFBFALCON,_TIMER_CTRL,_SOURCE,_PTIMER_1US, ctrl);
        ctrl = FLD_SET_DRF(_CFBFALCON,_TIMER_CTRL,_MODE,_ONE_SHOT, ctrl);
        ctrl = FLD_SET_DRF(_CFBFALCON,_TIMER_CTRL,_GATE,_STOP, ctrl);
        ctrl = FLD_SET_DRF(_CFBFALCON,_TIMER_CTRL,_RESET,_TRIGGER, ctrl);
        REG_WR32(CSB, LW_CFBFALCON_TIMER_CTRL,ctrl);

        last_train_time = 0;    // clear last time check
        FalconInterruptDisablePeriodicTimer();
    }
}
