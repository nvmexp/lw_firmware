/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2016-2017 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

#include <falcon-intrinsics.h>

#include "temp_tracking.h"
#include "fbflcn_gddr_mclk_switch.h"
#include "fbflcn_defines.h"
#include "fbflcn_helpers.h"
#include "fbflcn_table_headers.h"
#include "fbflcn_table.h"
#include "fbflcn_access.h"
#include "fbflcn_interrupts.h"
#include "fbflcn_mutex.h"
#include "dev_pwr_pri.h"
#include "priv.h"

#include "osptimer.h"
#include "memory.h"
#include "fbflcn_objfbflcn.h"
#include "config/g_memory_private.h"
extern OBJFBFLCN Fbflcn;
#include "config/g_fbfalcon_private.h"

// include manuals
#include "dev_fbpa.h"
#include "dev_pmgr.h"
#include "dev_top.h"

#include "memory_gddr.h"

//#define LOG_TEMP 0

#ifdef LOG_TEMP
#define LOG_TEMP_INDEX 0x4
#define LOG_TEMP_MARK(d) (((LOG_TEMP_INDEX) & 0xff) << 24) | ((d) & 0x00ffff)
#define TEMP_REPORT_SWITCH_MAILBOX 7
#define TEMP_REPORT_UP_SWITCH_MAILBOX 8
#define TEMP_REPORT_DOWN_SWITCH_MAILBOX 9
#endif

extern REGBOX* lwr_reg;
extern REGBOX* new_reg;
extern LwU8 gPlatformType;

LwU8 gTempTrackingSwitchControl=0;      // refresh switch enable
LwU8 gTempInterval=0;                   // temp tracking enable
LwU8 gTempTrackingCoeffN;
LwU8 gTempTrackingCoeffM;
LwU32 gTempReg;             // temp with sensor bit31=en/disable, bit15=temp_valid, bit13:0=SFXP9.5

// values in SFXP 9.5 or 11.5 (signed fixed point)
LwS16 gTempTrackingThresholdHigh;
LwS16 gTempTrackingThresholdLow;

// uses enums:
// FBFLCN_TEMP_MODE_HIGH_SETTINGS (2x = 2)
// FBFLCN_TEMP_MODE_LOW_SETTINGS (1x = 1)
LwU8 gTempTrackingMode;
LwBool gTempX8Dram;
volatile LwU32 gTempAllowIlwalidms = STARTUP_ILWALID_MS;
volatile LwU32 gTempStartTimens=0;
LwS32 gTempTrackingRecord[FBFLCN_TEMP_RECORD_STORAGE];

LwU32 gTempTrackingCounter = 0;
LwU32 gTempUpSwitchCounter = 0;
LwU32 gTempDownSwitchCounter = 0;
LwU8 gTempSamples;

void gddr_temp_refresh_sw(void);

LwBool checkIfMscgIsEngaged(void);
void disableMscg(void);

__attribute__((always_inline)) LwS16 lwu16_SFXP95_to_Lws16(LwU16); // lwu16 (9.5) to lws16
LwS16 lwu16_SFXP95_to_Lws16(LwU16 uval)
{
    LwS16 rtlwal = uval;
    // check bit 13 if 1, sign extend
    if ((0x200 & uval) > 0)
    {
        rtlwal |= 0xC000;
    }

    return rtlwal;
}

LwU16 getTempInterval(LwU8);
inline LwU16 getTempInterval(LwU8 tempIndex)
{
    LwU16 value;

    switch(tempIndex)
    {
        case TEMPSAMPLINGINTERVAL_1MS:
            value = 1;
            break;
        case TEMPSAMPLINGINTERVAL_5MS:
            value = 5;
            break;
        case TEMPSAMPLINGINTERVAL_10MS:
            value = 10;
            break;
        case TEMPSAMPLINGINTERVAL_20MS:
            value = 20;
            break;
        case TEMPSAMPLINGINTERVAL_50MS:
            value = 50;
            break;
        case TEMPSAMPLINGINTERVAL_100MS:
            value = 100;
            break;
        case TEMPSAMPLINGINTERVAL_200MS:
            value = 200;
            break;
        case TEMPSAMPLINGINTERVAL_500MS:
            value = 500;
            break;
        case TEMPSAMPLINGINTERVAL_1000MS:
            value = 1000;
            break;
        default:
            value = TEMPSAMPLINGINTERVAL_DISABLED;
    }
    return value;
}

//LW_PFB_FBPA_DQR_STATUS_DQ_ICx_SUBPn
LwS16 read_temperature(void)
{
    LwU16 tempMax = 0;
    LwU32 error = 0;
    LwU32 tempValids = 0;
    LwU32 temp = 0;
    LwU8 tempDram = 0;
    LwU8 isValid = 0;
    LwU8 p;     // partition
    LwU8 s;     // subp
    LwU8 i;     // IC
    LwU8 subParts;

    LwU32 unicastAddr;
    LwU8 validCnt=0;
    LwBool isMscgEngaged = checkIfMscgIsEngaged();
    
    if (!isMscgEngaged)
    {
        // set busy bit to make sure it stays disengaged
        REG_WR32(BAR0, LW_PPWR_PMU_PG_SW_CLIENT_3, DRF_DEF(_PPWR,_PMU_PG_SW_CLIENT_3,_ENG4_BUSY_SET,_TRIGGER));
    }
    else
    {
        disableMscg();
    }

    for (p=0; p<MAX_PARTS; p++)
    {
        if (isPartitionEnabled(p))  // for SUBPS=2
        {
            tempValids = REG_RD32(BAR0, unicast(LW_PFB_FBPA_DQR_STATUS_VLD,p));
            if (tempValids == 0)
            {
                error |= (p << 16) | 0x100;
                continue;
            }
            validCnt++;

#ifdef LOG_TEMP
            FW_MBOX_WR32(7, tempValids);
            FW_MBOX_WR32(9, 0x1234CAfE);
#endif
            if (!fbfalconIsHalfPartitionEnabled_HAL(&Fbflcn,p))
            {
                subParts=0;
            }
            else
            {
                subParts=1;
            }

            for (s=2; s-->subParts;)    // s starts at 1 in the loop
            {
                for (i=0; i<2; i++)
                {
                    isValid = (tempValids >> (
                      DRF_SHIFT(LW_PFB_FBPA_DQR_STATUS_VLD_IC0_SUBP0)+(i*2)+s)) &
                        DRF_MASK(LW_PFB_FBPA_DQR_STATUS_VLD_IC0_SUBP0);

                    // valid bit for both x8 parts
                    if (isValid == LW_PFB_FBPA_DQR_STATUS_VLD_IC0_SUBP0_VALID)
                    {
                        unicastAddr = (LW_PFB_FBPA_DQR_STATUS_DQ_IC0_SUBP0-0xA0000) +
                            (i*8) + (s*4) + (p*0x4000);
                        temp = REG_RD32(BAR0, unicastAddr);

#ifdef LOG_TEMP
            FW_MBOX_WR32(4, unicastAddr);
            FW_MBOX_WR32(5, LOG_TEMP_MARK((0x0100*p)+(0x10*s)+i));
            FW_MBOX_WR32(6, temp);
            FW_MBOX_WR32(7, (p<<16)|(s<<8)|i);
            FW_MBOX_WR32(8, tempMax);
#endif // LOG_TEMP
                        // shift over 16 bits to handle PAM4 mode on or off
                        temp = temp >> 16;

                        // mask with max bits
                        tempDram = temp & FBFLCN_TEMP_DRAM_MASK;
                        if (tempDram > tempMax)
                        {
                            tempMax = tempDram;
                        }

                        if (gTempX8Dram)
                        {
                            tempDram = (temp >> 8) & FBFLCN_TEMP_DRAM_MASK;
                            if (tempDram > tempMax)
                            {
                                tempMax = tempDram;
                            }
                        }
#ifdef LOG_TEMP
            FW_MBOX_WR32(9, tempMax);
#endif // LOG_TEMP
                    }
                    else
                    {
                        error |= (p << 16) | (s << 8) | i;
                    }
                } //end ICs
            } //end subps
        }
    }

    // clear busy bit for mscg to be able to engage again
    REG_WR32(BAR0, LW_PPWR_PMU_PG_SW_CLIENT_3, DRF_DEF(_PPWR,_PMU_PG_SW_CLIENT_3,_ENG4_BUSY_CLR,_TRIGGER));

    // did not find a valid in all partitions/subps/ics
    if ((error != 0) && (validCnt == 0))
    {
        gTempReg = FLD_SET_DRF(_CFBFALCON,_MEMORY_TEMPERATURE,_VALUE,_ILWALID,gTempReg);
    }
    else
    {
        gTempReg = FLD_SET_DRF(_CFBFALCON,_MEMORY_TEMPERATURE,_VALUE,_VALID,gTempReg);
    }

/* G6x temp table readout
 *
 * Temperature(C)  DRAM Temp(decimal)  Temp(binary) DQ[15:8] and DQ[7:0]
 *    -40                0              0000 0000
 *     ...             ...                    ...
 *      0               20              0001 0100
 *      2               21              0001 0101
 *      4               22              0001 0110
 *      6               23              0001 0111
 *      8               24              0001 1000
 *     ...             ...                    ...
 *     120              80              0101 0000
 *    >120              80              0101 0000
 */

    // get tempMax in celsius signed fixed point (9.5)
    // return -20C if in emulation
    if (gPlatformType == LW_PTOP_PLATFORM_TYPE_EMU)
    {
        tempMax = 0xFFEC;   // -20 in 2's complement
        gTempReg = FLD_SET_DRF(_CFBFALCON,_MEMORY_TEMPERATURE,_VALUE,_VALID,gTempReg);
    }
    else if (tempMax > 19)
    {
        if (tempMax > 80)   // needed?
        {
            tempMax = 80;
        }
        tempMax = ((tempMax - 20) * 2);
    }
    else    // negative
    {
        tempMax = (~(40 - (tempMax*2)))+1;
    }

    LwS16 rtlwal = tempMax << SFXP16_FRACTIONAL_BITS;

#ifdef LOG_TEMP
    FW_MBOX_WR32(5, LOG_TEMP_MARK(0x8000));
    FW_MBOX_WR32(6, error);
    FW_MBOX_WR32(7, tempMax);
    FW_MBOX_WR32(8, rtlwal);
    FW_MBOX_WR32(9, gTempReg);
#endif // LOG_TEMP

    return rtlwal;
}


void temperature_read_event(void)
{
    if (gTempInterval == 0)
    {
        return;
    }
    LwS16 temp = read_temperature();
    LwU8 valid = REF_VAL(LW_CFBFALCON_MEMORY_TEMPERATURE_VALUE,gTempReg);

    if (valid == LW_CFBFALCON_MEMORY_TEMPERATURE_VALUE_ILWALID)
    {
        LwU32 lwrrentTimens = REG_RD32(CSB, LW_CFBFALCON_FALCON_PTIMER0);
        if ((lwrrentTimens - gTempStartTimens) < (gTempAllowIlwalidms*1000000))
        {
            return;        // ignores invalid until gTempAllowIlwalidms passed from gTempStartTimens
        }
        else
        {
            // change to invalid temp timeout
            FW_MBOX_WR32(11,gTempStartTimens);
            FW_MBOX_WR32(12,lwrrentTimens);
            FW_MBOX_WR32(13,temp);
            FBFLCN_HALT(FBFLCN_ERROR_CODE_TEMPERATURE_READ_FAILED);
        }
    }

    gTempReg = FLD_SET_DRF_NUM(_CFBFALCON,_MEMORY_TEMPERATURE,_FIXED_POINT,(temp & 0x3FFF),gTempReg);
    REG_WR32(CSB, LW_CFBFALCON_MEMORY_TEMPERATURE, gTempReg);
    fbfalconPostDramTemperature_HAL(&Fbflcn, gTempReg);

    gTempTrackingCounter++;
#ifdef TEMP_REPORT_SWITCH_MAILBOX
    FW_MBOX_WR32(TEMP_REPORT_SWITCH_MAILBOX,gTempTrackingCounter);
#endif
#ifdef LOG_TEMP
FW_MBOX_WR32(8,0x88c0ffee);
FW_MBOX_WR32(8,gTempTrackingMode);
FW_MBOX_WR32(8,gTempTrackingThresholdHigh);
FW_MBOX_WR32(8,gTempTrackingThresholdLow);
FW_MBOX_WR32(8,temp);
FW_MBOX_WR32(8,0xc0ffee00);
#endif
//-------------------------------

    // update record buffer, shift down
    LwU8 i;
    for (i=FBFLCN_TEMP_RECORD_STORAGE-1; i>0; i--)
    {
        gTempTrackingRecord[i] = gTempTrackingRecord[i-1];
    }
    gTempTrackingRecord[0] = temp;

// avoid changing refresh?

    if (gTempSamples != FBFLCN_TEMP_RECORD_STORAGE)
    {
        gTempSamples++;
    }
#ifdef LOG_TEMP
// for TB to know how many samples
FW_MBOX_WR32(5,0xba7eba7e);
FW_MBOX_WR32(5,gTempSamples);
#endif

    // detect temperature range and initiate switch
    if ((temp >= gTempTrackingThresholdHigh) && (gTempTrackingMode == FBFLCN_TEMP_MODE_LOW_SETTINGS))
    {
        LwU8 samples = gTempTrackingCoeffM;
        LwU8 fail = 0;
        LwU8 i=0;

        // collect last M samples
        do
        {
            if (gTempTrackingRecord[i] < gTempTrackingThresholdHigh)
            {
                fail = 1;
            }
            i++;
        }
        while ((i<samples) && (fail==0));
#ifdef LOG_TEMP
FW_MBOX_WR32(8,0x99999999);
FW_MBOX_WR32(8,fail);
#endif
        // check for switch to higher temp profile
        if ((fail == 0) && (gTempTrackingSwitchControl == 1))
        {
            // exlwte switch to high temp
            gTempTrackingMode = FBFLCN_TEMP_MODE_HIGH_SETTINGS;
            gddr_temp_refresh_sw();
            gTempUpSwitchCounter++;
#ifdef TEMP_REPORT_UP_SWITCH_MAILBOX
            FW_MBOX_WR32(TEMP_REPORT_UP_SWITCH_MAILBOX,gTempUpSwitchCounter);
#endif
        }
    }
    else if (temp >= gTempTrackingThresholdLow)
    {
        // demilitarized temperature gap
        ;
#ifdef LOG_TEMP
FW_MBOX_WR32(8,0x55555555);
FW_MBOX_WR32(8,temp);
FW_MBOX_WR32(8,gTempTrackingThresholdLow);
#endif
    }
    else if (gTempTrackingMode == FBFLCN_TEMP_MODE_HIGH_SETTINGS)   // && temp < gTempTrackingThresholdLow
    {
        // collect last N samples
        LwU8 samples = gTempTrackingCoeffN;
        LwU8 fail = 0;
        LwU8 i=0;

        do
        {
            if (gTempTrackingRecord[i] >= gTempTrackingThresholdHigh)
            {
                fail = 1;
            }
            i++;
        }
        while ((i<samples) && (fail==0));

#ifdef LOG_TEMP
FW_MBOX_WR32(8,0x11111111);
FW_MBOX_WR32(8,fail);
FW_MBOX_WR32(8,gTempTrackingMode);
#endif
        // check for switch to lower temp profile
        if ((fail == 0) && (gTempTrackingSwitchControl == 1))
        {
            // execute switch to low temp
            gTempTrackingMode = FBFLCN_TEMP_MODE_LOW_SETTINGS;
            gddr_temp_refresh_sw();

            gTempDownSwitchCounter++;
#ifdef TEMP_REPORT_DOWN_SWITCH_MAILBOX
            FW_MBOX_WR32(TEMP_REPORT_DOWN_SWITCH_MAILBOX,gTempDownSwitchCounter);
#endif
        }
    }
    return;
}


LwU32 initTempTracking(void)
{
    gTempTrackingMode = FBFLCN_TEMP_MODE_AT_BOOT;
    gTempReg = 0;
    gTempSamples = 0;
    return 0;
}

// each time we start temp tracking we read from the current g_biosTarget to get the same target as the
// last mclk switch

LwU32 startTempTracking()
{
    PerfMemClk11StrapEntry *pMemClkStrapTable;
    pMemClkStrapTable = &gTT.perfMemClkStrapEntry;

    PerfMemTweak2xBaseEntry *pMemTweakTable;
    pMemTweakTable = &gTT.perfMemTweakBaseEntry;

    // get the init settings from the table
    LwU32 RefParam0 = TABLE_VAL (pMemClkStrapTable->HbmRefParam0);
    PerfMemClk11StrapEntryHbmRefParam0Struct* pRefParam0 = (PerfMemClk11StrapEntryHbmRefParam0Struct*)(&RefParam0);
    gTempInterval = pRefParam0->PMC11HRP0Interval;
    LwU8 multiplier = pRefParam0->PMC11HRP0Multiplier;

    gTempTrackingCoeffN = pRefParam0->PMC11HRP0CoeffN;
    gTempTrackingCoeffM = pRefParam0->PMC11HRP0CoeffM;
    gTempTrackingSwitchControl = pRefParam0->PMC11HRP0SwitchCtrl;

    // avoid overrunning the sample buffer
    if (gTempTrackingCoeffM > FBFLCN_TEMP_RECORD_STORAGE)
    {
        gTempTrackingCoeffM = FBFLCN_TEMP_RECORD_STORAGE;
    }
    if (gTempTrackingCoeffN > FBFLCN_TEMP_RECORD_STORAGE)
    {
        gTempTrackingCoeffN = FBFLCN_TEMP_RECORD_STORAGE;
    }

    // check this?
    gTempReg = REG_RD32(CSB, LW_CFBFALCON_MEMORY_TEMPERATURE);
    if (gTempInterval == 0)
    {
        gTempReg = FLD_SET_DRF(_CFBFALCON,_MEMORY_TEMPERATURE,_SENSOR, _DISABLED, gTempReg);
    }
    else
    {
        gTempReg = FLD_SET_DRF(_CFBFALCON,_MEMORY_TEMPERATURE,_SENSOR, _ENABLED, gTempReg);
    }
    REG_WR32(CSB, LW_CFBFALCON_MEMORY_TEMPERATURE, gTempReg);
    fbfalconPostDramTemperature_HAL(&Fbflcn, gTempReg);

    // external RT Timer
    LwU32 timerInterval = getTempInterval(gTempInterval) * multiplier;

//LW_PFB_FBPA_TRAINING_CTRL_MPR_DRAM_DEVICE_X8
//LW_PFB_FBPA_CFG0_X8_ENABLED
    LwU32 trainingCtrl = REG_RD32(BAR0, LW_PFB_FBPA_TRAINING_CTRL);
    if (REF_VAL(LW_PFB_FBPA_TRAINING_CTRL_WCK_PADS, trainingCtrl) ==
            LW_PFB_FBPA_TRAINING_CTRL_WCK_PADS_EIGHT)
    {
        gTempX8Dram = LW_TRUE;
    }
    else
    {
        gTempX8Dram = LW_FALSE;
    }

    LwU32 RefParam1 = TABLE_VAL (pMemClkStrapTable->HbmRefParam1);
    //PerfMemClk11StrapEntryHbmRefParam1Struct *pRefParam1 = (PerfMemClk11StrapEntryHbmRefParam1Struct*)(&RefParam1);
    // TODO: fix access to the struct (was failing for an unknown reason - I'm a bit puzzled about this)
    gTempTrackingThresholdLow = lwu16_SFXP95_to_Lws16((RefParam1 & 0xffff));
    gTempTrackingThresholdHigh = lwu16_SFXP95_to_Lws16(((RefParam1 >> 16) & 0xffff));

#ifdef LOG_TEMP
    FW_MBOX_WR32(3,RefParam0);
    FW_MBOX_WR32(4,sizeof(PerfMemClk11StrapEntryHbmRefParam0Struct));   // 4
    FW_MBOX_WR32(5,gTempInterval);              // 2 enum
    FW_MBOX_WR32(6,timerInterval);              // 10
    FW_MBOX_WR32(7,multiplier);                 // 2
    FW_MBOX_WR32(8,gTempTrackingCoeffN);        // 2
    FW_MBOX_WR32(9,gTempTrackingCoeffM);        // 2
    FW_MBOX_WR32(8,gTempTrackingSwitchControl); // 1
    FW_MBOX_WR32(9,gTempTrackingThresholdLow);  // 45 (0x5a0 9.5)
    FW_MBOX_WR32(8,gTempTrackingThresholdHigh); // 65 (0x820 9.5)
#endif // LOG_TEMP

    if (gTempInterval == 0)
    {
        return 0;
    }

    // clean the record buffer of old values
    LwU32 i;
    for (i=0; i<FBFLCN_TEMP_RECORD_STORAGE-1; i++)
    {
        gTempTrackingRecord[i] = 0;
    }

    LwU32 dqrCtrl;
    dqrCtrl = REG_RD32(BAR0, LW_PFB_FBPA_DQR_CTRL);
    // Note cannot dynamically change period when enables are set
    // No need to set these fields: period, set, clr (done in dev_init) 
    // set the EN_IC*_SUBP*, no need to worry about disabled partitions
    dqrCtrl |= (0xF << DRF_SHIFT(LW_PFB_FBPA_DQR_CTRL_EN_IC0_SUBP0));

    // broadcast write to all partitions
    REG_WR32(BAR0, LW_PFB_FBPA_DQR_CTRL, dqrCtrl);

    if (gTempTrackingSwitchControl != 0)
    {
        // Enable dynamic trefi update
        LwU32 fbpaDebug0;
        fbpaDebug0 = REG_RD32(BAR0, LW_PFB_FBPA_DEBUG_0);
        fbpaDebug0 = FLD_SET_DRF(_PFB_FBPA,_DEBUG_0,_TREFI_UPDATE,_DYNAMIC, fbpaDebug0);
        REG_WR32(BAR0, LW_PFB_FBPA_DEBUG_0, fbpaDebug0);
    }

    gTempStartTimens = REG_RD32(CSB, LW_CFBFALCON_FALCON_PTIMER0);

    // read temp once
    temperature_read_event();

    if (timerInterval > 0)
    {
        // set the interval of timer 0
        REG_WR32(CSB, LW_CFBFALCON_TIMER_0_INTERVAL,timerInterval * 1000);
        // set up the timer & reset
        LwU32 ctrl = 0;
        ctrl = FLD_SET_DRF(_CFBFALCON,_TIMER_CTRL,_SOURCE,_PTIMER_1US, ctrl) |
            FLD_SET_DRF(_CFBFALCON,_TIMER_CTRL,_MODE,_ONE_SHOT, ctrl);
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
    if (gTempInterval == 0)
    {
        return 0;
    }

    // do a read in case it's been a while
    temperature_read_event();

    LwU32 ctrl;
    ctrl = DRF_DEF(_CFBFALCON,_TIMER_CTRL,_SOURCE,_PTIMER_1US) |
        DRF_DEF(_CFBFALCON,_TIMER_CTRL,_MODE,_ONE_SHOT) |
        DRF_DEF(_CFBFALCON,_TIMER_CTRL,_GATE,_STOP) |
        DRF_DEF(_CFBFALCON,_TIMER_CTRL,_RESET,_TRIGGER);
    REG_WR32(CSB, LW_CFBFALCON_TIMER_CTRL,ctrl);

    FalconInterruptDisablePeriodicTimer();

    // clear enables do broadcast stop
    LwU32 dqrCtrl;
    dqrCtrl = REG_RD32(BAR0, LW_PFB_FBPA_DQR_CTRL);
    dqrCtrl &= ~(0xF << DRF_SHIFT(LW_PFB_FBPA_DQR_CTRL_EN_IC0_SUBP0));
    REG_WR32_STALL(BAR0, LW_PFB_FBPA_DQR_CTRL, dqrCtrl);

    gTempInterval = 0;  // disable temp tracking
    disableEvent(EVENT_MASK_TEMP_TRACKING);

    return 0;
}


void getTimingValuesForRefi(LwU32 *config4, LwU32 *timing21)
{
    LwU8 tempTrackingRefresh_lo;// config4
    LwU16 tempTrackingRefresh;  // config4
    LwU8 refsb;                 // timing21
    LwU16 refsbDispatchPeriod;  // timing21

    LwU32 new_config4;
    LwU32 new_timing21;

    PerfMemTweak2xBaseEntry *pMemTweakTable;
    pMemTweakTable = &gTT.perfMemTweakBaseEntry;

    // 1x values from table
//    new_config4 = TABLE_VAL (pMemTweakTable->Config4);
//    new_timing21 = TABLE_VAL (pMemTweakTable->Timing21);

    new_config4 = gTT.perfMemTweakBaseEntry.Config4;
    new_timing21 = gTT.perfMemTweakBaseEntry.Timing21;

    PerfMemClk11StrapEntry *pMemClkStrapTable = &gTT.perfMemClkStrapEntry;
    LwU32 RefParam0 = TABLE_VAL (pMemClkStrapTable->HbmRefParam0);
    PerfMemClk11StrapEntryHbmRefParam0Struct* pRefParam0 = (PerfMemClk11StrapEntryHbmRefParam0Struct*)(&RefParam0);
    gTempTrackingSwitchControl = pRefParam0->PMC11HRP0SwitchCtrl;

    //need to check if enabled in this state
    // get the init settings from the table
    LwU8 enable = pRefParam0->PMC11HRP0Interval;

    if ((enable == 0) || (gTempTrackingSwitchControl == 0) || (gTempTrackingMode == FBFLCN_TEMP_MODE_LOW_SETTINGS))
    {
#ifdef LOG_TEMP
FW_MBOX_WR32(10,0x11111111);
#endif
    }
    else if (gTempTrackingMode == FBFLCN_TEMP_MODE_HIGH_SETTINGS)
    {
        //if high settings, use 2x values from pRefresh->PMCT2xRefresh
        LwU32 Refresh = TABLE_VAL (pMemTweakTable->Config4HBMRefresh);
        PerfMemTweak2xBaseEntryHBMRefresh *pRefresh = (PerfMemTweak2xBaseEntryHBMRefresh*)(&Refresh);

        tempTrackingRefresh_lo = pRefresh->PMCT2xHBMREFRESH_LO;
        tempTrackingRefresh = pRefresh->PMCT2xHBMREFRESH;
        refsb = pRefresh->PMCT2xHBMREFREFSB;
        refsbDispatchPeriod = pRefresh->PMCT2xDISPATCH_PERIOD;

        // Hack - swap setting since vbios has it flipped (VBIOS: 0=enable, 1=disable)
        if (refsb == 0)
        {
            refsb = 1;
        }
        else
        {
            refsb = 0;
        }

        new_config4 = FLD_SET_DRF_NUM(_PFB,_FBPA_CONFIG4,_REFRESH_LO, tempTrackingRefresh_lo, new_config4);
        new_config4 = FLD_SET_DRF_NUM(_PFB,_FBPA_CONFIG4,_REFRESH, tempTrackingRefresh, new_config4);

        new_timing21 = FLD_SET_DRF_NUM(_PFB,_FBPA_TIMING21,_REFSB,refsb, new_timing21);
        new_timing21 = FLD_SET_DRF_NUM(_PFB,_FBPA_TIMING21,_REFSB_DISPATCH_PERIOD,refsbDispatchPeriod,new_timing21);

#ifdef LOG_TEMP
FW_MBOX_WR32(10,0x22222222);
FW_MBOX_WR32(10,tempTrackingRefresh_lo);
FW_MBOX_WR32(10,tempTrackingRefresh);
FW_MBOX_WR32(10,refsb);
FW_MBOX_WR32(10,refsbDispatchPeriod);
#endif  // LOG_TEMP
    }

    (*config4) = new_config4;
    (*timing21) = new_timing21;

#ifdef LOG_TEMP
FW_MBOX_WR32(10,0xFFFFFFFF);
FW_MBOX_WR32(10,new_config4);
FW_MBOX_WR32(10,new_timing21);
#endif  // LOG_TEMP
}

void gddr_temp_refresh_sw(void)
{
    LwU32 newConfig4=0;
    LwU32 newTiming21=0;

    // load minimal set of timing values
    getTimingValuesForRefi(&newConfig4, &newTiming21);

    REG_WR32(BAR0, LW_PFB_FBPA_CONFIG4, newConfig4);
    REG_WR32(BAR0, LW_PFB_FBPA_TIMING21, newTiming21);

    // for static method need to disable first then re-enable refctrl
//    LwU32 refCtrl;
//    refCtrl = REG_RD32(BAR0, LW_PFB_FBPA_REFCTRL);
//    REG_WR32(BAR0, LW_PFB_FBPA_REFCTRL, FLD_SET_DRF(_PFB,_FBPA_REFCTRL,_VALID,_VALID_0,refCtrl));
//    REG_WR32(BAR0, LW_PFB_FBPA_REFCTRL, FLD_SET_DRF(_PFB,_FBPA_REFCTRL,_VALID,_VALID_1,refCtrl));
}

LwBool checkIfMscgIsEngaged(void)
{
    LwU32 mscgStat = REG_RD32(BAR0, LW_PPWR_PMU_PG_STAT(4));

    if (FLD_TEST_DRF(_PPWR, _PMU_PG_STAT, _EST, _ACTIVE, mscgStat) &&
        FLD_TEST_DRF(_PPWR, _PMU_PG_STAT, _PGST, _IDLE, mscgStat))
    {
        return LW_FALSE;
    }

    return LW_TRUE;
}

void disableMscg(void)
{
    // notify mscg to disable
    REG_WR32(BAR0, LW_PPWR_PMU_PG_SW_CLIENT_3, DRF_DEF(_PPWR,_PMU_PG_SW_CLIENT_3,_ENG4_BUSY_SET,_TRIGGER));

    // wait for mscg to disengage
    LwU32 mscgStat = REG_RD32(BAR0, LW_PPWR_PMU_PG_STAT(4));
    while (!(FLD_TEST_DRF(_PPWR, _PMU_PG_STAT, _EST, _ACTIVE, mscgStat) &&
        FLD_TEST_DRF(_PPWR, _PMU_PG_STAT, _PGST, _IDLE, mscgStat)))
    {
        mscgStat = REG_RD32(BAR0, LW_PPWR_PMU_PG_STAT(4));
    }
}
