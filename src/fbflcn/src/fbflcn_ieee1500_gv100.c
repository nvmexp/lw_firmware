/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2016-2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

#include <falcon-intrinsics.h>

#include "fbflcn_ieee1500_gv100.h"
#include "fbflcn_hbm_mclk_switch.h"
#include "fbflcn_defines.h"
#include "fbflcn_helpers.h"
#include "fbflcn_table_headers.h"
#include "fbflcn_table.h"
#include "fbflcn_access.h"
#include "fbflcn_interrupts.h"
#include "fbflcn_mutex.h"
#include "priv.h"

#include "osptimer.h"
#include "memory.h"
#include "fbflcn_objfbflcn.h"
#include "config/g_memory_private.h"

// include manuals
#include "dev_fbpa.h"
#include "dev_fbfalcon_csb.h"
#include "dev_pmgr.h"

#define RegRd(adr) REG_RD32(BAR0, adr)
#define RegWr(adr,data) REG_WR32(BAR0, adr,data)

extern OBJFBFLCN Fbflcn;
extern REG_BOX* lwr;
extern REG_BOX* new;

LwU8 gTempTrackingSwitchControl;
LwU8 gTempTrackingCoeffN;
LwU8 gTempTrackingCoeffM;
LwU16 gTempTrackingThresholdHigh;
LwU16 gTempTrackingThresholdLow;

LwU8 gTempTrackingMode;
LwU32 gTempTrackingRecord[FBFLCN_TEMP_RECORD_STORAGE];

LwU32 gTempTrackingCounter = 0;
LwU32 gTempUpSwitchCounter = 0;
LwU32 gTempDownSwitchCounter = 0;

void hbm_temp_switch_gv100(void);


LwU8
readTemperature(LwU8* temp, LwU32 reg)
{
    LwU32 tempRaw = RegRd(reg);
// TODO:  Need to review this!  possibly the valid bit could be in bit 24,
// also JEDEC spec mentions 8 bits for the temp
    *temp = (tempRaw & 0x7f000000) >> 24;
    LwU8 valid = (tempRaw & 0x80000000) >> 31;
    return valid;
}


LwU32 fbflcn_ieee1500_gv100_read_temperature(void)
{

    RegWr(multicastMasterWriteFromBrodcastRegister(LW_PFB_FBPA_FBIO_HBM_TEST_I1500_INSTR), 0x0000000F);
    RegWr(multicastMasterWriteFromBrodcastRegister(LW_PFB_FBPA_FBIO_HBM_TEST_I1500_MODE),0x8);
#ifdef LOG_IEEE1500
    REG_WR32(CSB, LW_CFBFALCON_FIRMWARE_MAILBOX(13), gDisabledFBPAMask);
#endif  // LOG_IEEE1500

    LwU32 tempMax = 0;
    //LwU32 tempVec = 0;

    // read Site A
    if ((gDisabledFBPAMask & 0x000F) != 0x000F)
    {
        LwU8 tempA = 0;
        LwU8 ilwalidA = 0;
        ilwalidA = readTemperature(&tempA, LW_PFB_FBPA_0_FBIO_HBM_TEST_I1500_DATA);
#ifdef LOG_IEEE1500
        REG_WR32(CSB, LW_CFBFALCON_FIRMWARE_MAILBOX(6), LOG_IEEE1500_MARK(0x0100));
        REG_WR32(CSB, LW_CFBFALCON_FIRMWARE_MAILBOX(7), tempA);
#endif // LOG_IEEE1500
        if (ilwalidA == 0)
        {
            //tempVec |= tempA;
            if (tempA > tempMax)
            {
                tempMax = tempA;
            }
        }
    }

    // read Site B
    if ((gDisabledFBPAMask & 0x00F0) != 0x00F0)
    {
        LwU8 tempB = 0;
        LwU8 ilwalidB = 0;
        ilwalidB = readTemperature(&tempB, LW_PFB_FBPA_4_FBIO_HBM_TEST_I1500_DATA);
#ifdef LOG_IEEE1500
        REG_WR32(CSB, LW_CFBFALCON_FIRMWARE_MAILBOX(6), LOG_IEEE1500_MARK(0x0200));
        REG_WR32(CSB, LW_CFBFALCON_FIRMWARE_MAILBOX(7), tempB);
#endif // LOG_IEEE1500
        if (ilwalidB == 0)
        {
            //tempVec |= (tempB << 8);
            if (tempB > tempMax)
            {
                tempMax = tempB;
            }
        }
    }

    // read Site C
    if ((gDisabledFBPAMask & 0x0F00) != 0x0F00)
    {
        LwU8 tempC = 0;
        LwU8 ilwalidC = 0;
        ilwalidC = readTemperature(&tempC, LW_PFB_FBPA_8_FBIO_HBM_TEST_I1500_DATA);
#ifdef LOG_IEEE1500
        REG_WR32(CSB, LW_CFBFALCON_FIRMWARE_MAILBOX(6), LOG_IEEE1500_MARK(0x0300));
        REG_WR32(CSB, LW_CFBFALCON_FIRMWARE_MAILBOX(7), tempC);
#endif // LOG_IEEE1500
        if (ilwalidC == 0)
        {
            //tempVec |= (tempC << 16);
            if (tempC > tempMax)
            {
                tempMax = tempC;
            }
        }
    }

    // read Site D
    if ((gDisabledFBPAMask & 0xF000) != 0xF000)
    {
        LwU8 tempD = 0;
        LwU8 ilwalidD = 0;
        ilwalidD = readTemperature(&tempD, LW_PFB_FBPA_C_FBIO_HBM_TEST_I1500_DATA);
#ifdef LOG_IEEE1500
        REG_WR32(CSB, LW_CFBFALCON_FIRMWARE_MAILBOX(6), LOG_IEEE1500_MARK(0x0400));
        REG_WR32(CSB, LW_CFBFALCON_FIRMWARE_MAILBOX(7), tempD);
#endif // LOG_IEEE1500
        if (ilwalidD == 0)
        {
            //tempVec |= (tempD << 24);
            if (tempD > tempMax)
            {
                tempMax = tempD;
            }
        }
    }

    return tempMax;
}


LwU32 temperature_read_event(void)
{
    //
    // Acquire mutex
    //
    LwU8 token;

    MUTEX_HANDLER ieee1500Mutex;
    ieee1500Mutex.registerMutex = IEEE1500_MUTEX_REG(IEEE1500_MUTEX_INDEX);
    ieee1500Mutex.registerIdRelease = IEEE1500_MUTEX_ID_RELEASE;
    ieee1500Mutex.registerIdAcquire = IEEE1500_MUTEX_ID_ACQUIRE;
    ieee1500Mutex.valueInitialLock = IEEE1500_MUTEX_REG_INITIAL_LOCK;
    ieee1500Mutex.valueAcquireInit = IEEE1500_MUTEX_ID_ACQUIRE_VALUE_INIT;
    ieee1500Mutex.valueAcquireNotAvailable = IEEE1500_MUTEX_ID_ACQUIRE_VALUE_NOT_AVAIL;

    LW_STATUS status = mutexAcquireByIndex_GV100(&ieee1500Mutex, MUTEX_REQ_TIMEOUT_NS, &token);

    if (status != LW_OK)
    {
        //
        // TODO-stefans: 0 should correspond to LW_OK. Defines aren't correctly
        // included anywhere today.
        //
        return 0;
    }

    LwU32 temp = fbflcn_ieee1500_gv100_read_temperature();

    //
    // release mutex
    //
    status = mutexReleaseByIndex_GV100(&ieee1500Mutex, token);
    if(status != LW_OK) {
        FBFLCN_HALT(FBFLCN_ERROR_CODE_I1500_GPIO_MUTEX_NOT_RELEASED);
    }

    gTempTrackingCounter++;
#ifdef STATS_IEEE1500_REPORT_SWITCH_MAILBOX
    REG_WR32(CSB, STATS_IEEE1500_REPORT_SWITCH_MAILBOX,gTempTrackingCounter);
#endif

    // update record buffer
    LwU8 i;
    for (i=FBFLCN_TEMP_RECORD_STORAGE-1; i>0; i--)
    {
        gTempTrackingRecord[i] = gTempTrackingRecord[i-1];
    }
    gTempTrackingRecord[0] = temp;

    // detect temperature range and initiate switch
    if (temp >= gTempTrackingThresholdHigh)
    {
        LwU8 samples = gTempTrackingCoeffM;
        LwU8 fail = 0;
        LwU8 i;
        // avoid overrunning the sample buffer
        if (samples > FBFLCN_TEMP_RECORD_STORAGE)
        {
            samples = FBFLCN_TEMP_RECORD_STORAGE;
        }
        for (i=0; i<samples; i++)
        {
            if (gTempTrackingRecord[i] < gTempTrackingThresholdHigh)
            {
                fail = 1;
            }
        }
        // check for switch to higher temp profile
        if (fail == 0)
        {
            REG_WR32(CSB, LW_CFBFALCON_FIRMWARE_MAILBOX(13),2);
            if ((gTempTrackingSwitchControl == 1) &&
                    (gTempTrackingMode == FBFLCN_TEMP_MODE_LOW_SETTINGS))
            {
                // exlwte switch to high temp
                gTempTrackingMode = FBFLCN_TEMP_MODE_HIGH_SETTINGS;
                hbm_temp_switch_gv100();
                gTempUpSwitchCounter++;
#ifdef STATS_IEEE1500_REPORT_UP_SWITCH_MAILBOX
                REG_WR32(CSB, STATS_IEEE1500_REPORT_UP_SWITCH_MAILBOX,gTempUpSwitchCounter);
#endif
            }
        }
    }
    else if (temp >= gTempTrackingThresholdLow)
    {
        // demilitarized temperature gap
        ;
    }
    else
    {
        // collect last N samples
        LwU8 samples = gTempTrackingCoeffN;
        LwU8 fail = 0;
        LwU8 i;
        // avoid overrunning the sample buffer
        if (samples > FBFLCN_TEMP_RECORD_STORAGE)
        {
            samples = FBFLCN_TEMP_RECORD_STORAGE;
        }
        for (i=0; i<samples; i++)
        {
            if (gTempTrackingRecord[i] >= gTempTrackingThresholdHigh)
            {
                fail = 1;
            }
        }
        // check for switch to lower temp profile
        if ((gTempTrackingSwitchControl == 1) &&
                (fail == 0) &&
                (gTempTrackingMode == FBFLCN_TEMP_MODE_HIGH_SETTINGS))
        {
            // execute switch to low temp
            gTempTrackingMode = FBFLCN_TEMP_MODE_LOW_SETTINGS;
            hbm_temp_switch_gv100();

            gTempDownSwitchCounter++;
#ifdef STATS_IEEE1500_REPORT_DOWN_SWITCH_MAILBOX
            REG_WR32(CSB, STATS_IEEE1500_REPORT_DOWN_SWITCH_MAILBOX,gTempDownSwitchCounter);
#endif

        }
    }

    return temp;
}

LwU32 initTempTracking(void)
{
    gTempTrackingMode = FBFLCN_TEMP_MODE_AT_BOOT;
    return 0;
}

// each time we start temp tracking we read from the current g_biosTarget to get the same target as the
// last mclk switch

LwU32 startTempTracking()
{
#if (FBFALCONCFG_CHIP_ENABLED(GV10X) && FBFALCONCFG_FEATURE_ENABLED(FBFALCON_JOB_MCLK_HBM2))
    PerfMemClk11StrapEntry *pMemClkStrapTable;
    pMemClkStrapTable = gTT.pMCSEp;

    PerfMemTweak2xBaseEntry *pMemTweakTable;
    pMemTweakTable = gTT.pMTBEp;

    // get the init settings from the table
    LwU32 HbmRefParam0 = TABLE_VAL (pMemClkStrapTable->HbmRefParam0);
    PerfMemClk11StrapEntryHbmRefParam0Struct* pHbmRefParam0 = (PerfMemClk11StrapEntryHbmRefParam0Struct*)(&HbmRefParam0);

    // TODO:  use enum of interval to callwlate time (ms) since PMC11HRP0Interval is an index
    LwU8 interval = pHbmRefParam0->PMC11HRP0Interval;
    LwU8 multiplier = pHbmRefParam0->PMC11HRP0Multiplier;
    gTempTrackingCoeffN = pHbmRefParam0->PMC11HRP0CoeffN;
    gTempTrackingCoeffM = pHbmRefParam0->PMC11HRP0CoeffM;
    gTempTrackingSwitchControl = pHbmRefParam0->PMC11HRP0SwitchCtrl;
#else

    LwU8 interval = gTT.perfMemClkStrapEntry.HbmRefParam0.PMC11HRP0Interval;
    LwU8 multiplier = gTT.perfMemClkStrapEntry.HbmRefParam0.PMC11HRP0Multiplier;
    gTempTrackingCoeffN = gTT.perfMemClkStrapEntry.HbmRefParam0.PMC11HRP0CoeffN;
    gTempTrackingCoeffM = gTT.perfMemClkStrapEntry.HbmRefParam0.PMC11HRP0CoeffM;
    gTempTrackingSwitchControl = gTT.perfMemClkStrapEntry.HbmRefParam0.PMC11HRP0SwitchCtrl;
#endif

#ifdef LOG_IEEE1500
    REG_WR32(CSB, LW_CFBFALCON_FIRMWARE_MAILBOX(6),HbmRefParam0);
    REG_WR32(CSB, LW_CFBFALCON_FIRMWARE_MAILBOX(7),sizeof(PerfMemClk11StrapEntryHbmRefParam0Struct));

    REG_WR32(CSB, LW_CFBFALCON_FIRMWARE_MAILBOX(8),interval);
    REG_WR32(CSB, LW_CFBFALCON_FIRMWARE_MAILBOX(9),multiplier);
    REG_WR32(CSB, LW_CFBFALCON_FIRMWARE_MAILBOX(10),gTempTrackingCoeffN);
    REG_WR32(CSB, LW_CFBFALCON_FIRMWARE_MAILBOX(11),gTempTrackingCoeffM);
    REG_WR32(CSB, LW_CFBFALCON_FIRMWARE_MAILBOX(12),gTempTrackingSwitchControl);
#endif // LOG_IEEE1500

#if (FBFALCONCFG_CHIP_ENABLED(GV10X) && FBFALCONCFG_FEATURE_ENABLED(FBFALCON_JOB_MCLK_HBM2))
    LwU32 HbmRefParam1 = TABLE_VAL (pMemClkStrapTable->HbmRefParam1);
    //PerfMemClk11StrapEntryHbmRefParam1Struct *pHbmRefParam1 = (PerfMemClk11StrapEntryHbmRefParam1Struct*)(&HbmRefParam1);
    // TODO: fix access to the struct (was failing for an unknown reason - I'm a bit puzzled about this)
    gTempTrackingThresholdLow = HbmRefParam1 & 0xffff;
    gTempTrackingThresholdHigh = (HbmRefParam1 >> 16) & 0xffff;
#else
    gTempTrackingThresholdLow = gTT.perfMemClkStrapEntry.HbmRefParam1.PMC11HbmRefParam1ThresholdTempLow;
    gTempTrackingThresholdHigh = gTT.perfMemClkStrapEntry.HbmRefParam1.PMC11HbmRefParam1ThresholdTempHigh;
#endif

    // clean the record buffer of old values
    LwU32 i;
    for (i=0; i<FBFLCN_TEMP_RECORD_STORAGE-1; i++)
    {
        gTempTrackingRecord[i] = 0;
    }

    // external RT Timer
    LwU32 timerInterval = interval * multiplier;
    if (timerInterval > 0)
    {
        // set the intervall of timer 0
        REG_WR32(CSB, LW_CFBFALCON_TIMER_0_INTERVAL,timerInterval * 1000);
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
    }
    return 0;
}

LwU32 stopTempTracking(void)
{
    LwU32 ctrl = 0;
    ctrl = FLD_SET_DRF(_CFBFALCON,_TIMER_CTRL,_SOURCE,_PTIMER_1US, ctrl);
    ctrl = FLD_SET_DRF(_CFBFALCON,_TIMER_CTRL,_MODE,_ONE_SHOT, ctrl);
    ctrl = FLD_SET_DRF(_CFBFALCON,_TIMER_CTRL,_GATE,_STOP, ctrl);
    ctrl = FLD_SET_DRF(_CFBFALCON,_TIMER_CTRL,_RESET,_TRIGGER, ctrl);
    REG_WR32(CSB, LW_CFBFALCON_TIMER_CTRL,ctrl);

    FalconInterruptDisablePeriodicTimer();
    return 0;
}

void getTimingValuesForRefi(void)
{
    lwr->cfg0 = REG_RD32(BAR0, LW_PFB_FBPA_CFG0);
    new->cfg0 = lwr->cfg0;

#if (FBFALCONCFG_CHIP_ENABLED(GV10X) && FBFALCONCFG_FEATURE_ENABLED(FBFALCON_JOB_MCLK_HBM2))
    PerfMemTweak2xBaseEntry *pMemTweakTable;
    pMemTweakTable = gTT.pMTBEp;
    LwU16 hbmRefresh = TABLE_VAL (pMemTweakTable->Config4HBMRefresh);

    PerfMemTweak2xBaseEntryHBMRefresh *pHBMRefresh = (PerfMemTweak2xBaseEntryHBMRefresh*)(&hbmRefresh);
    LwU8 tempTrackingRefresh_lo = pHBMRefresh->PMCT2xHBMREFRESH_LO;
    LwU16 tempTrackingRefresh = pHBMRefresh->PMCT2xHBMREFRESH;
    LwU8 tempTrackingRefsb = pHBMRefresh->PMCT2xHBMREFREFSB;
#else
    LwU8 tempTrackingRefresh_lo = gTT.perfMemTweakBaseEntry.Config4HBMRefresh.PMCT2xHBMREFRESH_LO;
    LwU16 tempTrackingRefresh = gTT.perfMemTweakBaseEntry.Config4HBMRefresh.PMCT2xHBMREFRESH;
    LwU8 tempTrackingRefsb =  gTT.perfMemTweakBaseEntry.Config4HBMRefresh.PMCT2xHBMREFREFSB;
#endif

#ifdef LOG_IEEE1500
    REG_WR32(CSB, LW_CFBFALCON_FIRMWARE_MAILBOX(10),hbmRefresh);
    REG_WR32(CSB, LW_CFBFALCON_FIRMWARE_MAILBOX(10),tempTrackingRefresh_lo);
    REG_WR32(CSB, LW_CFBFALCON_FIRMWARE_MAILBOX(10),tempTrackingRefresh);
    REG_WR32(CSB, LW_CFBFALCON_FIRMWARE_MAILBOX(10),tempTrackingRefsb);
#endif  // LOG_IEEE1500

    lwr->config4 = REG_RD32(BAR0, LW_PFB_FBPA_CONFIG4);
#if (FBFALCONCFG_CHIP_ENABLED(GV10X) && FBFALCONCFG_FEATURE_ENABLED(FBFALCON_JOB_MCLK_HBM2))
    new->config4 = TABLE_VAL (pMemTweakTable->Config4);
#else
    new->config4 = gTT.perfMemTweakBaseEntry.Config4;
#endif

#ifdef LOG_IEEE1500
    REG_WR32(CSB, LW_CFBFALCON_FIRMWARE_MAILBOX(10),lwr->config4);
    REG_WR32(CSB, LW_CFBFALCON_FIRMWARE_MAILBOX(10),new->config4);
#endif  // LOG_IEEE1500

    if (gTempTrackingMode == FBFLCN_TEMP_MODE_HIGH_SETTINGS)
    {
        new->config4 = FLD_SET_DRF_NUM(_PFB,_FBPA_CONFIG4,_REFRESH_LO, tempTrackingRefresh_lo, lwr->config4);
        new->config4 = FLD_SET_DRF_NUM(_PFB,_FBPA_CONFIG4,_REFRESH, tempTrackingRefresh,new->config4);
    }
    else
    {
        LwU32 rmw_config4 = 0x3fff & new->config4;
        lwr->config4 = lwr->config4 & (~0x00003fff);
        new->config4 = lwr->config4 | rmw_config4;
    }

#ifdef LOG_IEEE1500
    REG_WR32(CSB, LW_CFBFALCON_FIRMWARE_MAILBOX(10),new->config4);
#endif

    // needed for fifo reset
    lwr->hbm_cfg0 = REG_RD32(BAR0, LW_PFB_FBPA_FBIO_HBM_CFG0);
    new->hbm_cfg0 = lwr->hbm_cfg0;

    // used for MSB of rfc for delay
    lwr->config10 = REG_RD32(BAR0, LW_PFB_FBPA_CONFIG10);
    new->config10 = lwr->config10;

    lwr->timing21 = REG_RD32(BAR0, LW_PFB_FBPA_TIMING21);
#if (FBFALCONCFG_CHIP_ENABLED(GV10X) && FBFALCONCFG_FEATURE_ENABLED(FBFALCON_JOB_MCLK_HBM2))
    new->timing21 = TABLE_VAL (pMemTweakTable->Timing21);
#else
    new->timing21 = gTT.perfMemTweakBaseEntry.Timing21;
#endif

#ifdef LOG_IEEE1500
    REG_WR32(CSB, LW_CFBFALCON_FIRMWARE_MAILBOX(10),lwr->timing21);
    REG_WR32(CSB, LW_CFBFALCON_FIRMWARE_MAILBOX(10),new->timing21);
#endif  // LOG_IEEE1500

    if (gTempTrackingMode == FBFLCN_TEMP_MODE_HIGH_SETTINGS)
    {
        new->timing21 = FLD_SET_DRF_NUM(_PFB,_FBPA_TIMING21,_REFSB,tempTrackingRefsb,lwr->timing21);
    }
    else
    {
        LwU32 rmw_timing21 = 0x1 & new->timing21;
        lwr->timing21 = lwr->timing21 & 0xfffffffe;
        new->timing21 = lwr->timing21 | rmw_timing21;
    }

#ifdef LOG_IEEE1500
    REG_WR32(CSB, LW_CFBFALCON_FIRMWARE_MAILBOX(10),new->timing21);
#endif  // LOG_IEEE1500

    // for asr_acp_control
    lwr->dram_asr = REG_RD32(BAR0, LW_PFB_FBPA_DRAM_ASR);
    new->dram_asr = lwr->dram_asr;

}

void hbm_temp_switch_gv100(void)
{
    FLCN_TIMESTAMP  startFbStopTimeNs = { 0 };
    LwU32           fbStopTimeNs      = 0;

    // load minimal set of timing values
    getTimingValuesForRefi();

    // disable in lwr->dram_asr
    disable_asr_acpd_gv100();

    // update to lwr->timing21
    disable_refsb_gv100();

    memoryStopFb_HAL(&Fbflcn, 0, &startFbStopTimeNs);

    // lwr->cfg9,  1 register
    set_acpd_off();

    // enable_dram_ack uses lwr->cfg0;  3 register
    precharge_all();

    // enable_dram_ack uses lwr->cfg0;   8 registers
    enable_self_refresh();


    RegWr (LW_PFB_FBPA_CONFIG4, new->config4);
    RegWr (LW_PFB_FBPA_TIMING21, new->timing21);

    // need tRFC (config0 and config10) for delay;  2 registers
    disable_self_refresh();

    // TODO: writes new->timing21 again   9 registers
    enable_periodic_refresh();

    // access new->hbm_cfg0  1 register
    reset_read_fifo();

    // writing back cfgo and dram_asr from new to restore, 1 register
    // TODO: cfg0 could be removed from this, but requires additional functions in mclk_switch
    set_acpd_value();

    //
    // Just stroring the return value.
    // To do - stefans: Add the code that takes appropriate action for when we've taken
    // too long during FB stop.
    //
    fbStopTimeNs = memoryStartFb_HAL(&Fbflcn, &startFbStopTimeNs);

}


