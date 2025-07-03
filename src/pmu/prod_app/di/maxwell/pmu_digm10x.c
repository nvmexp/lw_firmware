/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2013-2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file   pmu_digm10x.c
 * @brief  HAL-interface for the GM10x Deepidle Engine
 */

/* ------------------------- System Includes -------------------------------- */
#include "pmusw.h"

#include "dev_master.h"
#include "dev_fbpa.h"
#include "dev_trim.h"
#include "dev_trim_addendum.h"
#include "dev_pwr_csb.h"
#include "dev_pwr_pri.h"
#include "dev_fuse.h"
#include "dev_gc6_island.h"
#include "dev_lw_xp.h"
#include "dev_lw_xve.h"
#include "dev_timer.h"
#include "dev_disp.h"
#include "dev_pwr_falcon_csb.h"
#include "dev_pmgr.h"
#include "dev_lw_aza_pri.h"
#include "pmu/pmumailboxid.h"

/* ------------------------- Application Includes --------------------------- */
#include "dbgprintf.h"
#include "lwuproc.h"
#include "flcntypes.h"
#include "pmu_objpmu.h"
#include "pmu_objdisp.h"
#include "pmu_objms.h"
#include "pmu_objpg.h"
#include "pmu_objdi.h"
#include "pmu_objap.h"
#include "pmu_objfifo.h"
#include "pmu_objlpwr.h"
#include "pmu_objic.h"
#include "pmu_disp.h"
#include "pmu_swasr.h"
#include "pmu_cg.h"
#include "pmu_perfmon.h"

#include "config/g_di_private.h"

/* ------------------------- Type Definitions ------------------------------- */
/* ------------------------- Macros and Defines ----------------------------- */

//
// These macros are needed because as there are no macros for below fields
// in HW manuals for the register LW_CPWR_THERM_I2CS_STATE
//
#define      LW_CPWR_THERM_I2CS_STATE_SECONDARY_ADDR                       6:0
#define      LW_CPWR_THERM_I2CS_STATE_SECONDARY_IDLE                       7:7

// These are temporary defines till HW display manual changes gets in TOT
#define LW_PDISP_SOR_PLL_PU_BGAP_DELAY                                 0x00000014
#define LW_PDISP_SOR_PLL_PU_PLL_LOCK_DELAY                             0x000000c8

// Compile time asserts to protect against assumptions.
ct_assert(DRF_BASE(LW_CPWR_THERM_VID0_PWM_PERIOD)       == DRF_BASE(LW_PGC6_SCI_VID_CFG0_PERIOD));
ct_assert(DRF_EXTENT(LW_CPWR_THERM_VID0_PWM_PERIOD)     == DRF_EXTENT(LW_PGC6_SCI_VID_CFG0_PERIOD));
ct_assert(DRF_BASE(LW_CPWR_THERM_VID0_PWM_BIT_SPREAD)   == DRF_BASE(LW_PGC6_SCI_VID_CFG0_BIT_SPREAD));
ct_assert(DRF_EXTENT(LW_CPWR_THERM_VID0_PWM_BIT_SPREAD) == DRF_EXTENT(LW_PGC6_SCI_VID_CFG0_BIT_SPREAD));
ct_assert(DRF_BASE(LW_CPWR_THERM_VID0_PWM_MIN_SPREAD)   == DRF_BASE(LW_PGC6_SCI_VID_CFG0_MIN_SPREAD));
ct_assert(DRF_EXTENT(LW_CPWR_THERM_VID0_PWM_MIN_SPREAD) == DRF_EXTENT(LW_PGC6_SCI_VID_CFG0_MIN_SPREAD));
ct_assert(DRF_BASE(LW_CPWR_THERM_VID1_PWM_HI)           == DRF_BASE(LW_PGC6_SCI_VID_CFG1_HI));
ct_assert(DRF_EXTENT(LW_CPWR_THERM_VID1_PWM_HI)         == DRF_EXTENT(LW_PGC6_SCI_VID_CFG1_HI));

#define LWRM_PMU_DI_MAX_EXIT_LATENCY_US                         1000

/* ------------------------- Global Variables ------------------------------- */
/* ------------------------- External Definitions --------------------------- */
/* ------------------------- Static Variables ------------------------------- */
/* --------------------------- Prototypes ------------------------------------*/

static void   s_diSeqHostPtimerUpdate_GM10X(void);
static void   s_diSeqOsmClocksGate_GM10X(void);
static void   s_diSeqOsmClocksUngate_GM10X(void);
static void   s_diSeqHaltIntrDisable_GM10X(void);
static void   s_diSeqHaltIntrEnable_GM10X(void);
static void   s_diSeqCreateEvents_GM10X(DI_SEQ_CACHE *pCache);
static void   s_diSeqUpdateXpSubstateOffset_GM10X(void);
static void   s_diSeqMaskBsiRxStatEvent_GM10X(void);
static void   s_diSeqBsiPadCtrlClkReqDisable_GM10X(void);
static LwBool s_diSeqSaveThermPwmStateOnI2cIdle_GM10X(DI_SEQ_CACHE *pCache, LwBool bSaveVid);
static LwBool s_diSeqWakeTimerProgram_GM10X(void);
static void   s_diSeqSysPartitionClocksAction_GM10X(DI_SEQ_CACHE *pCache, LwBool bExit);
static void   s_diSeqDeepL1WakeEventsDisable_GM10X(DI_SEQ_CACHE *pCache, LwBool bDisable);
static void   s_diSeqSciWakeTimerEnable_GM10X(LwBool);
static LwU32  s_diSeqFindLowestActiveLane_GM10X(void);
static void   s_diSeqXtalClkSrcRestore_GM10X(void);
static void   s_diSeqBsiClkreqEventConfig_GM10X(LwBool bEntering);
static void   s_diSeqPsiHandlerEntry_GM10X(void);
static void   s_diSeqPsiHandlerExit_GM10X(void);

/* ------------------------- Public Functions ------------------------------- */

/*!
 * @Brief Get list of clocks running from XTAL when SPPLLs are shut down
 *
 * @param[out] pPclkRoutedToXtalList    Clock list
 * @param[out] pListSize                Number of clocks in list
 */
void
diSeqListClkRoutedToXtalGet_GM10X
(
     DI_REG_CACHE **pPclkRoutedToXtalList,
     LwU8          *pListSize
)
{
    static DI_REG_CACHE clkRoutedToXtalList[] =
    {// {Register address, Init value of register cache}
        {LW_PTRIM_SYS_SYS2CLK_OUT_SWITCH , 0x0},
        {LW_PTRIM_SYS_UTILSCLK_OUT_SWITCH, 0x0},
        {LW_PTRIM_SYS_PWRCLK_OUT_SWITCH  , 0x0},
    };

    *pPclkRoutedToXtalList = clkRoutedToXtalList;
    *pListSize             = (LwU8) LW_ARRAY_ELEMENTS(clkRoutedToXtalList);
}

/*!
 * @brief Checks if the azalia engine is inactive
 *
 * @return  LW_TRUE    If azalia inactive
 * @return  LW_FALSE   If azalia active
 */
LwBool
diSeqIsAzaliaInactive_GM10X(void)
{
    LwU32  var1;
    LwU32  var2;
    LwBool bInactive;

    // Is the Azalia controller inactive?
    var1 = REG_RD32(BAR0, LW_AZA_PRI_SDCTL_4);
    var2 = REG_RD32(BAR0, LW_AZA_PRI_SDCTL_5);

    bInactive = (FLD_TEST_DRF(_AZA, _PRI_SDCTL, _RUN, _STOP, var1) &&
                 FLD_TEST_DRF(_AZA, _PRI_SDCTL, _RUN, _STOP, var2));

    // Update abort reason if Azalia is not idle
    if (!bInactive)
    {
        DI_SEQ_ABORT_REASON_UPDATE_NAME(DISP_AZALIA);
    }

    return bInactive;
}

/*!
 * @brief Checks if all heads are inactive
 *
 * @return  LW_TRUE    If heads are inactive
 * @return  LW_FALSE   If heads are active
 */
LwBool
diSeqAreHeadsInactive_GM10X(void)
{
    LwU32 headState;
    LwU32 headIndex;

    // Are all heads inactive?
    for (headIndex = 0; headIndex < Disp.numHeads; headIndex++)
    {
        headState = REG_RD32(BAR0, LW_PDISP_DSI_CORE_HEAD_STATE(headIndex));
        if (!FLD_TEST_DRF(_PDISP, _DSI_CORE_HEAD_STATE, _OPERATING_MODE,
                          _SLEEP, headState))
        {
            DI_SEQ_ABORT_REASON_UPDATE_NAME(DISP_HEAD);
            return LW_FALSE;
        }
    }

    return LW_TRUE;
}

/*!
 * @brief Gates clocks from OSM using STOPCLK HW.
 *
 * Asserts pwr2clk_stopclks signal to gate the clocks at OSM. STOPCLOCK
 * initialization is done by VBIOS. VBIOS sets COND_STOPCLK_EN to _YES in
 * case clock needs to be gated by STOPCLK HW. pwr2clk_stopclks gates all
 * clocks which were added into STOPCLK HW.
 *
 * This is list of clocks gated by STOPCLK. List can change with chip family.
 * - LW_PVTRIM_SYS_VCLK_REF_SWITCH(0)
 * - LW_PVTRIM_SYS_VCLK_REF_SWITCH(1),
 * - LW_PVTRIM_SYS_VCLK_REF_SWITCH(2),
 * - LW_PVTRIM_SYS_VCLK_REF_SWITCH(3),
 * - LW_PTRIM_SYS_DRAMCLK_ALT_SWITCH,
 * - LW_PVTRIM_SYS_VCLK_ALT_SWITCH(0),
 * - LW_PVTRIM_SYS_VCLK_ALT_SWITCH(1),
 * - LW_PVTRIM_SYS_VCLK_ALT_SWITCH(2),
 * - LW_PVTRIM_SYS_VCLK_ALT_SWITCH(3),
 * - LW_PTRIM_SYS_HUB2CLK_OUT_SWITCH,
 * - LW_PVTRIM_SYS_DISPCLK_OUT_SWITCH,
 * - LW_PVTRIM_SYS_AZA2XBITCLK_OUT_SWITCH,
 * - LW_PVTRIM_SYS_MAUDCLK_OUT_SWITCH
 */
static void
s_diSeqOsmClocksGate_GM10X()
{
    LwU32 regVal;

    DBG_PRINTF_DIDLE(("GC5: OSM clocks Gating %u\n", 0, 0, 0, 0));

    regVal = REG_RD32(CSB, LW_CPWR_PMU_GPIO_OUTPUT_SET);
    regVal = FLD_SET_DRF(_CPWR, _PMU_GPIO_OUTPUT_SET, _STOPCLOCKS, _TRUE, regVal);
    REG_WR32(CSB, LW_CPWR_PMU_GPIO_OUTPUT_SET, regVal);
}

/*!
 * @brief Ungates clocks from OSM using STOPCLK HW.
 *
 * This is counter part of ref@s_diSeqOsmClocksGate_GM10X. It de-assert
 * pwr2clk_stopclks signal to ungate the clocks at OSM.
 */
static void
s_diSeqOsmClocksUngate_GM10X(void)
{
    LwU32 regVal;

    DBG_PRINTF_DIDLE(("GC5: OSM clocks Ungating %u\n", 0, 0, 0, 0));

    regVal = REG_RD32(CSB, LW_CPWR_PMU_GPIO_OUTPUT_CLEAR);
    regVal = FLD_SET_DRF(_CPWR, _PMU_GPIO_OUTPUT_CLEAR, _STOPCLOCKS, _TRUE, regVal);
    REG_WR32(CSB, LW_CPWR_PMU_GPIO_OUTPUT_CLEAR, regVal);
}

/*!
 * @breif Enable/Disable SCI Timer for wakeup and sw reporting.
 *
 * TImer will start ticking after SW handoffs control to SCI.
 * Deep-TODO: Break function in 2 parts.
 */
static void
s_diSeqSciWakeTimerEnable_GM10X(LwBool bEnable)
{
    LwU32 regVal;

    if (bEnable)
    {
        // Enable the Timer
        regVal = REG_RD32(FECS, LW_PGC6_SCI_WAKE_EN);
        regVal = FLD_SET_DRF(_PGC6, _SCI_WAKE_EN, _WAKE_TIMER, _ENABLE, regVal);
        REG_WR32(FECS, LW_PGC6_SCI_WAKE_EN, regVal);

        // Enable sw reporting
        regVal = REG_RD32(FECS, LW_PGC6_SCI_SW_INTR0_EN);
        regVal = FLD_SET_DRF(_PGC6, _SCI_SW_INTR0_EN, _WAKE_TIMER, _ENABLE, regVal);
        REG_WR32(FECS, LW_PGC6_SCI_SW_INTR0_EN, regVal);
    }
    else
    {
        // Disable the Timer
        regVal = REG_RD32(FECS, LW_PGC6_SCI_WAKE_EN);
        regVal = FLD_SET_DRF(_PGC6, _SCI_WAKE_EN, _WAKE_TIMER, _DISABLE, regVal);
        REG_WR32(FECS, LW_PGC6_SCI_WAKE_EN, regVal);

        // Disable sw reporting
        regVal = REG_RD32(FECS, LW_PGC6_SCI_SW_INTR0_EN);
        regVal = FLD_SET_DRF(_PGC6, _SCI_SW_INTR0_EN, _WAKE_TIMER, _DISABLE, regVal);
        REG_WR32(FECS, LW_PGC6_SCI_SW_INTR0_EN, regVal);
    }
}

/*!
 * @brief Program SCI Timer for wakeup/Exit
 *
 * @return  LW_FALSE if expiry time is not sane in auto mode.
 */
static LwBool
s_diSeqWakeTimerProgram_GM10X(void)
{
    LwU32   regVal;
    LwU32   wakeTimeUs = 0;
    LwU32   timeLo;
    LwU32   timeHi;

    DBG_PRINTF_DIDLE(("SSC: Programming SCI timer before DI entry\n", 0, 0, 0, 0));

    if (!Di.bWakeupTimer)
    {
        DBG_PRINTF_DIDLE(("SSC: Wakeup timer is disabled\n", 0, 0, 0, 0));
        return LW_TRUE;
    }

    //
    // wakeTimeUs should be callwlated as MIN(maxSleepTimeUs, PTIMER Alarm, Perfmon Callback) for AUTO Mode
    // wakeTimeus = Di.maxSleepTimeUs for FORCE Mode (test mode)
    //
    wakeTimeUs = Di.maxSleepTimeUs;

    if (Di.bModeAuto)
    {
        LwU32 timeTillPerfmonCallback = LW_U32_MAX;

        // Set Timer to wake DI so that at least a break even residency is achieved
        if (FLD_TEST_DRF(_PTIMER, _INTR_EN_0, _ALARM, _ENABLED,
                         REG_RD32(BAR0, LW_PTIMER_INTR_EN_0)))
        {
            // read the current time
            diSnapshotSciPtimer_HAL(&Di, &timeLo, &timeHi);
            // read the ptimer alarm
            regVal = REG_RD32(BAR0, LW_PTIMER_ALARM_0);
            // get time in usec
            wakeTimeUs = (regVal - timeLo) / 1000;
        }

        if (PMUCFG_FEATURE_ENABLED(PMU_PERFMON_TIMER_BASED_SAMPLING))
        {
#if (PMUCFG_FEATURE_ENABLED(PMU_OS_CALLBACK_CENTRALISED))
            if (OS_TMR_CALLBACK_WAS_CREATED(&OsCBPerfMon))
            {
                // time till the next callback in sleep mode
                timeTillPerfmonCallback = LWRTOS_TICK_PERIOD_US *
                    OsCBPerfMon.modeData[OS_TMR_CALLBACK_MODE_SLEEP].ticksNext;
            }
#else
            // time till the next callback
            timeTillPerfmonCallback = LWRTOS_TICK_PERIOD_US *
                osTimerGetNextDelay(&PerfmonOsTimer);
#endif
            // use the lower of the two so we don't skip perfmon sampling
            wakeTimeUs = LW_MIN(wakeTimeUs, timeTillPerfmonCallback);
        }

        //
        // Abort if the alarm time is lower than the current time + latency OR
        // if the time requested is greater than max allowed.
        // In autosetup mode; the wake time param passed in is the latency.
        //
        if (wakeTimeUs <= LWRM_PMU_DI_MAX_EXIT_LATENCY_US)
        {
            if (PMUCFG_FEATURE_ENABLED(PMU_PERFMON_TIMER_BASED_SAMPLING) &&
                (wakeTimeUs == timeTillPerfmonCallback))
            {
                DI_SEQ_ABORT_REASON_UPDATE_NAME(RTOS_CALLBACK);
            }
            else
            {
                DI_SEQ_ABORT_REASON_UPDATE_NAME(PTIMER_ALARM);
            }
            return LW_FALSE;
        }
        wakeTimeUs -= LWRM_PMU_DI_MAX_EXIT_LATENCY_US;    // subtract the latency
    }

    // Write the Timer Value
    regVal = FLD_SET_DRF_NUM(_PGC6, _SCI_WAKEUP_TMR_CNTL, _TIMER, wakeTimeUs, 0);
    REG_WR32(FECS, LW_PGC6_SCI_WAKEUP_TMR_CNTL, regVal);
    s_diSeqSciWakeTimerEnable_GM10X(LW_TRUE);

    return LW_TRUE;
}

/*!
 * @brief Check if I2C Secondary is idle
 *
 * If the Secondary is not idle; return error.
 * Else check for a change in Secondary address from last cached.
 * Save the changed address in SCI.
 *
 * @param[in,out]   pCache      Holds cached register values necessary for
 *                              entering into and exiting out of deepidle
 * @param[in]       bSaveVid    When true; save the vid settings from therm
 *                              registers into the cache.
 *
 * @return  LW_TRUE if secondary idle
 *          LW_FALSE is secondary busy
 */
static LwBool
s_diSeqSaveThermPwmStateOnI2cIdle_GM10X
(
    DI_SEQ_CACHE *pCache,
    LwBool        bSaveVid
)
{
    LwU32   i2csAddr;
    LwU8    i2csSecondaryIdle;
    LwU32   regVal;
    LwBool  bUpdateSCI = LW_FALSE;

    // Read Current I2CS state
    regVal = REG_RD32(CSB, LW_CPWR_THERM_I2CS_STATE);
    i2csSecondaryIdle = DRF_VAL(_CPWR, _THERM_I2CS_STATE, _SECONDARY_IDLE, regVal);
    if (i2csSecondaryIdle)
    {
        i2csAddr = DRF_VAL(_CPWR, _THERM_I2CS_STATE, _SECONDARY_ADDR, regVal);
        if (i2csAddr != pCache->i2cSecondaryAddress)
        {
            // Update.
            pCache->i2cSecondaryAddress = i2csAddr;
            bUpdateSCI = LW_TRUE;
        }
    }
    else
    {
        DI_SEQ_ABORT_REASON_UPDATE_NAME(THERM_I2C_SECONDARY);
        return LW_FALSE;
    }

    if (bUpdateSCI)
    {
        // Update I2CS state in SCI
        regVal = REG_RD32(FECS, LW_PGC6_SCI_GPU_I2CS_CFG);
        regVal = FLD_SET_DRF_NUM(_PGC6,_SCI_GPU_I2CS_CFG, _I2C_ADDR, i2csAddr, regVal);
        regVal = FLD_SET_DRF(_PGC6, _SCI_GPU_I2CS_CFG, _I2C_ADDR_VALID, _YES, regVal);
        REG_WR32(FECS, LW_PGC6_SCI_GPU_I2CS_CFG, regVal);
    }

    if (bSaveVid)
    {
        LwU32 railIdx = 0;
#ifdef LW_CPWR_THERM_VID2_PWM
        PG_VOLT_SET_THERM_VID0_CACHE(railIdx, REG_RD32(CSB, LW_CPWR_THERM_VID0_PWM));
        PG_VOLT_SET_THERM_VID1_CACHE(railIdx, REG_RD32(CSB, LW_CPWR_THERM_VID1_PWM));
#else
        for (railIdx = 0; railIdx < PG_VOLT_RAIL_IDX_MAX; railIdx++)
        {
            if (PG_VOLT_IS_RAIL_SUPPORTED(railIdx))
            {
                PG_VOLT_SET_THERM_VID0_CACHE(railIdx, REG_RD32(CSB, LW_CPWR_THERM_VID0_PWM(railIdx)));
                PG_VOLT_SET_THERM_VID1_CACHE(railIdx, REG_RD32(CSB, LW_CPWR_THERM_VID1_PWM(railIdx)));
            }
        }
#endif
    }

    return LW_TRUE;
}

/*!
 * Enable/Disable GPU events from causing deep L1 exits.
 *
 * More detailed explanation of this WAR can be found in the caller to this
 * function where we disable the events from causing Deep L1 exits.
 *
 * Ref @ Bugs #1343194, #1359137 for SW WAR dislwssion.
 * Ref @ Bug #1361199 to track a HW fix in the future.
 *
 * Note that we only use this WAR when we are in Deep L1 and not in L1
 * substates since L1 substates will use CLKREQ as the wakeup mechanism.
 *
 * @param[in,out]   pCache      Holds cached register values necessary for
 *                              entering into and exiting out of deepidle.
 * @param[in]       bDisable    When true, disable internal wakeup mechanism
 *                              When false, re-enable internal wakeup mechanism
 *
 * @return  void
 */
static void
s_diSeqDeepL1WakeEventsDisable_GM10X
(
    DI_SEQ_CACHE *pCache,
    LwBool        bDisable
)
{
    LwU32 regVal;

    if (bDisable)
    {
        // Cache the current value
        regVal = REG_RD32(BAR0_CFG, LW_XVE_PRIV_XV_BLKCG2);
        pCache->deepL1WakeEn = regVal;

        // Disable all deep L1 wake types
        regVal = FLD_SET_DRF(_XVE, _PRIV_XV_BLKCG2, _HOST2XV_HOST_IDLE_WAKE_EN,
                          _DISABLED, regVal);
        regVal = FLD_SET_DRF(_XVE, _PRIV_XV_BLKCG2, _UPSTREAM_REQ_WAKE_EN,
                          _DISABLED, regVal);
        regVal = FLD_SET_DRF(_XVE, _PRIV_XV_BLKCG2, _CONFIG0_UPDATE_WAKE_EN,
                          _DISABLED, regVal);
        regVal = FLD_SET_DRF(_XVE, _PRIV_XV_BLKCG2, _CONFIG1_UPDATE_WAKE_EN,
                          _DISABLED, regVal);
        regVal = FLD_SET_DRF(_XVE, _PRIV_XV_BLKCG2, _FN0_INTR_PENDING_WAKE_EN,
                          _DISABLED, regVal);
        regVal = FLD_SET_DRF(_XVE, _PRIV_XV_BLKCG2, _FN1_INTR_PENDING_WAKE_EN,
                          _DISABLED, regVal);
        regVal = FLD_SET_DRF(_XVE, _PRIV_XV_BLKCG2, _FN0_EOI_PENDING_WAKE_EN,
                          _DISABLED, regVal);
        regVal = FLD_SET_DRF(_XVE, _PRIV_XV_BLKCG2, _FN1_EOI_PENDING_WAKE_EN,
                          _DISABLED, regVal);
    }
    else
    {
        // Restore the wake types.
        regVal = pCache->deepL1WakeEn;
    }

    REG_WR32(BAR0_CFG, LW_XVE_PRIV_XV_BLKCG2, regVal);
}

/*!
 * @brief Update the XP timer offset
 * XP timer tracks how long to wait before exiting substate.
 */
static void
s_diSeqUpdateXpSubstateOffset_GM10X(void)
{
    LwU32    offset;
    LwU32    val;

    offset = REG_RD32(FECS, LW_PGC6_SCI_XP_CLKREQ);
    offset = DRF_VAL(_PGC6, _SCI_XP_CLKREQ, _TIMER, offset);

    val = REG_RD32(BAR0, LW_XP_L1_SUBSTATE_3);
    val = FLD_SET_DRF_NUM(_XP, _L1_SUBSTATE_3, _L12_WAKEUP_ELAPSED, offset, val);
    REG_WR32(BAR0, LW_XP_L1_SUBSTATE_3, val);
}

/*!
 * @brief Read the ptimer from SCI
 *
 *@param  pTimeLowNs[out]    Content of PTIMER_TIME_0
 *@param  pTimeLowNs[out]    Content of PTIMER_TIME_1
 */
void
diSnapshotSciPtimer_GM10X
(
    LwU32 *pTimeLowNs,
    LwU32 *pTimeHiNs
)
{
    LwU32    regVal;
    // Trigger SCI to update SCI PTIMER TIME_0 and TIME_1 registers
    regVal = REG_RD32(FECS, LW_PGC6_SCI_PTIMER_READ);
    regVal = FLD_SET_DRF(_PGC6, _SCI_PTIMER_READ, _SAMPLE, _TRIGGER, regVal);
    REG_WR32(FECS, LW_PGC6_SCI_PTIMER_READ, regVal);

    // Wait for TIME_0 and TIME_1 registers to be updated
    do
    {
        regVal = REG_RD32(FECS, LW_PGC6_SCI_PTIMER_READ);
    } while (!FLD_TEST_DRF(_PGC6, _SCI_PTIMER_READ, _SAMPLE, _DONE, regVal));

    // Read TIME_0(Low 32 bits) and TIME_1(Hi 32 bits)
    *pTimeLowNs = REG_RD32(FECS, LW_PGC6_SCI_PTIMER_TIME_0);
    *pTimeHiNs  = REG_RD32(FECS, LW_PGC6_SCI_PTIMER_TIME_1);
}

/*!
 * @brief Update the Host PTIMER.
 *
 * Host Ptimer is updated from SCI Ptimer, which is running/valid across
 * GC5 cycles. This function copies SCI PTimer value to host PTimer.
 */
static void
s_diSeqHostPtimerUpdate_GM10X()
{
    LwU32    timeLo;
    LwU32    timeHi;

    DBG_PRINTF_DIDLE(("GC5: Updating Host Timer during SSC exit\n", 0, 0, 0, 0));

    diSnapshotSciPtimer_HAL(&Di, &timeLo, &timeHi);

    // Update Host Ptimer.
    REG_WR32(BAR0, LW_PTIMER_TIME_1, timeHi);
    REG_WR32(BAR0, LW_PTIMER_TIME_0, timeLo);
}

/*!
 * @brief Clocks to be sourced from XTAL & XTAL4X on deepidle enter.
 *
 * Enter: change clocks that are still necessary to be sourced from
 *        XTAL  instead of SPPLLs after we cache current state.
 * Exit:  restore clocks to the original sources using cached values.
 */
void
diSeqXtalClkSrcSet_GM10X(void)
{
    LwU32 regVal;
    LwU8  i;

    for (i = 0; i < Di.clkRoutedToXtalListSize; i++)
    {
        // Read the clock register
        Di.pClkRoutedToXtal[i].cacheVal = regVal =
            REG_RD32(FECS, Di.pClkRoutedToXtal[i].regAddr);

        // Route the clk to XTAL_IN
        regVal = FLD_SET_DRF(_PTRIM, _SYS_SYS2CLK_OUT_SWITCH, _FINALSEL,
                             _SLOWCLK, regVal);
        regVal = FLD_SET_DRF(_PTRIM, _SYS_SYS2CLK_OUT_SWITCH, _SLOWCLK,
                             _XTAL_IN, regVal);
        // Write to the clock register
        REG_WR32(FECS, Di.pClkRoutedToXtal[i].regAddr, regVal);
    }
}

/*!
 * @brief Restore clock sources to original on deepidle exit.
 *
 * Enter: change clocks that are still necessary to be sourced from
 *        XTAL  instead of SPPLLs after we cache current state.
 * Exit:  restore clocks to the original sources using cached values.
 */
static void
s_diSeqXtalClkSrcRestore_GM10X(void)
{
    LwU8 i;

    // Restore the Clk state
    for (i = Di.clkRoutedToXtalListSize; i > 0; i--)
    {
        REG_WR32(FECS, Di.pClkRoutedToXtal[i - 1].regAddr,
                       Di.pClkRoutedToXtal[i - 1].cacheVal);
    }
}

/*!
 * @brief Checks Host Idleness
 *
 * Heres the algorithm:
 * if (HOST_INT_NOT_ACTIVE && HOST_NOT_QUIESCENT)
 *     ok to do GC5
 *
 * if (HOST_INT_NOT_ACTIVE && HOST_EXT_NOT_ACTIVE)
 *     ok to do clockslowdown
 *
 * Please refer to bug 1046002 Comment #20 to get a summary.
 */
LwBool
diSeqIsHostIdle_GM10X(void)
{
    LwU32 regVal;

    regVal = REG_RD32(CSB, LW_CPWR_PMU_IDLE_STATUS);
    if (FLD_TEST_DRF(_CPWR, _PMU_IDLE_STATUS, _HOST_INT_NOT_ACTIVE, _TRUE, regVal) &&
        FLD_TEST_DRF(_CPWR, _PMU_IDLE_STATUS, _HOST_NOT_QUIESCENT, _TRUE, regVal))
    {
        return LW_TRUE;
    }
    else
    {
        DI_SEQ_ABORT_REASON_UPDATE_NAME(HOST);
        return LW_FALSE;
    }
}


/*!
 * @brief Check GPU pending Interrupts
 *
 * @param[in,out]   pCache  Holds cached register values necessary for
 *                          entering into and exiting out of deepidle.
 *
 * @return          LW_TRUE     Interrupts are pending
 *                  LW_FALSE    Interrupts are not pending
 */
LwBool
diSeqIsIntrPendingGpu_GM10X
(
    DI_SEQ_CACHE *pCache
)
{
    LwBool bIntrPending;

    pCache->intrStatus.reg[RM_PMU_DIDLE_INTR_STATUS_REG_PMC_INTR_0] =
        REG_RD32(BAR0, LW_PMC_INTR(0));
    pCache->intrStatus.reg[RM_PMU_DIDLE_INTR_STATUS_REG_PMC_INTR_1] =
        REG_RD32(BAR0, LW_PMC_INTR(1));

    DBG_PRINTF_DIDLE(("GC5:PMC Intr0 0x%x, Intr1: 0x%x",
        pCache->intrStatus.reg[RM_PMU_DIDLE_INTR_STATUS_REG_PMC_INTR_0],
        pCache->intrStatus.reg[RM_PMU_DIDLE_INTR_STATUS_REG_PMC_INTR_1], 0, 0));

    bIntrPending =
        !!(pCache->intrStatus.reg[RM_PMU_DIDLE_INTR_STATUS_REG_PMC_INTR_0] |
           pCache->intrStatus.reg[RM_PMU_DIDLE_INTR_STATUS_REG_PMC_INTR_1]);

   if (bIntrPending)
   {
        DI_SEQ_ABORT_REASON_UPDATE_NAME(INTR_GPU);
   }

   return bIntrPending;
}

/*!
 * @brief Copy clear SCI Pending Interrupts
 *
 * Copy clear SCI Pending Interrupts. This will copy
 * LW_PGC6_SCI_SW_INTR0/1_STATUS_LWRR to LW_PGC6_SCI_SW_INTR0/1_STATUS_PREV
 * and then clear the LWRR registers.
 */
void
diSeqCopyClearSciPendingIntrs_GM10X(void)
{
    // clear the lwrr interrupt status registers
    LwU32 regVal = 0;
    regVal = FLD_SET_DRF_NUM(_PGC6, _SCI_CNTL, _SW_STATUS_COPY_CLEAR,
                             LW_PGC6_SCI_CNTL_SW_STATUS_COPY_CLEAR_TRIGGER, regVal);

    REG_WR32(FECS, LW_PGC6_SCI_CNTL, regVal);
}

/*!
 * @brief Check SCI pending Interrupts
 *
 * @param[in,out]   pCache  Holds cached register values necessary for
 *                          entering into and exiting out of deepidle.
 *
 * @return          LW_TRUE     Interrupts are pending
 *                  LW_FALSE    Interrupts are not pending
 */
LwBool
diSeqIsIntrPendingSci_GM10X
(
    DI_SEQ_CACHE *pCache
)
{
    //
    // For now all interrupts may cause a wakeup. We may use the en mask here while checking.
    // The mask bits are in LW_PGC6_SCI_SW_INTR0_EN, LW_PGC6_SCI_SW_INTR1_RISE_EN and
    // LW_PGC6_SCI_SW_INTR1_FALL_EN
    //
    LwBool bIntrPending;

    pCache->intrStatus.reg[RM_PMU_DIDLE_INTR_STATUS_REG_SCI_INTR_0] =
        REG_RD32(FECS, LW_PGC6_SCI_SW_INTR0_STATUS_LWRR);
    pCache->intrStatus.reg[RM_PMU_DIDLE_INTR_STATUS_REG_SCI_INTR_1] =
        REG_RD32(FECS, LW_PGC6_SCI_SW_INTR1_STATUS_LWRR);

    bIntrPending =
        !!(pCache->intrStatus.reg[RM_PMU_DIDLE_INTR_STATUS_REG_SCI_INTR_0] |
           pCache->intrStatus.reg[RM_PMU_DIDLE_INTR_STATUS_REG_SCI_INTR_1]);
    if (bIntrPending)
    {
        DI_SEQ_ABORT_REASON_UPDATE_NAME(INTR_SCI);
    }

    return bIntrPending;
}

/*!
 * @brief Handoff control to Island and halt PMU.
 *
 * This is last step in DI which hands-off contrl to island and halts PMU. PMU
 * will be resume by BSI by exelwting DI phase in BSI.
 */
void
diSeqIslandHandover_GM10X(void)
{
    LwU32 regVal;

    // Program BSI control mode to GC5 and unmask rxstat_idle event
    regVal = REG_RD32(FECS, LW_PGC6_BSI_CTRL);
    regVal = FLD_SET_DRF(_PGC6, _BSI_CTRL, _BSI2SCI_RXSTAT, _ENABLE, regVal);
    regVal = FLD_SET_DRF(_PGC6, _BSI_CTRL, _MODE, _GC5, regVal);
    REG_WR32(FECS, LW_PGC6_BSI_CTRL, regVal);

    // Disable the halt exception interrupt
    s_diSeqHaltIntrDisable_GM10X();

    //
    // This assembly code does
    // 1. Fetch PC and store in BOOTVEC, so that when PMU falcon is restarted
    //    from halted state, PMU falcon will start exelwting from desired offset
    //    in Imem.
    // 2. SCI is triggered to enter GC5, before PMU goes halt.
    // 3. Upon restart from halted state, used register state is restored and GC5
    //    Exit sequence continues
    //

    asm
    (
        "call GRAB_PC;"           // SP = SP -4;   Call GRAB_PC to fetch the PC
        "addsp -0xC;"             // SP = SP - 12  Decrement the SP to restore a9 and a10
        "pop a9;"                 // SP = SP + 4   Restore a9  from Stack
        "pop a10;"                // SP = SP + 4   Restore a10 from stack
        "addsp 0x4;"              // SP = SP + 4   This increment to needed because of PC stored
                                  //               here before calling GRAB_PC
        "jmp GC5_EXIT;"           // Jump to SSC_EXIT

        "GRAB_PC:"
            "push a10;"            // SP = SP - 4   Save a10 in Stack
            "push a9;"             // SP = SP - 4   Save a9  in Stack
            "addsp 0x8;"           // SP = SP + 8   Increment Stack Pointer to pop PC(program counter)
            "pop a10;"             // SP = SP + 4   pops PC value into a10
            "mvi a9 0x104;"        // Write BootVector(LW_CMSDEC_FALCON_BOOTVEC) address into a9
            "stxb a9 0x0 a10;"     // Storing(blocked) PC value from a10 into BOOTVEC(indirectly writing into a9)

            //
            //  The below steps trigger SCI GC5 entry by writing _ENTRY_TRIGGER_GC5.
            //  NOTE: We dont do _COPY_CLEAR because we dont want to lose the wakeup events
            //  that happened during the window where ucode was not looking for them and was
            //  busy doing GPU powerdown/SCI entry (ie the final substate).
            //
            "mvi a10 0x00000001;"  // Set the bit for GC5_ENTRY.
            "mvi a9  0x1810e800;"  // Trigger register address.
            "std.w a9 a10;"        // The write is deliberately FECS non-blocking as SCI would be stopping the clocks.
            "halt;"                // Put the PMU to HALT.

        //
        // SP is restored to its original value, so no stack corruption;
        // and no registers are corrupted
        //
        "GC5_EXIT:"
    );

    //
    // For efficient L1 substates wakeup, XP substate Offset need to be
    // copied before handling any other Interrupts in PMU.
    // Reference Bug 1246486
    //
    // Deep-TODO: Investigate whether this step is needed for DeepL1
    //
    s_diSeqUpdateXpSubstateOffset_GM10X();

    // Re-enable HALT interrupt
    s_diSeqHaltIntrEnable_GM10X();
}

/*!
 * @brief Disables the Halt Interrupt
 *
 * DI Halts PMU. PMU halt is expected. Disabling interrupt so that RM
 * will not report this Halt as an error.
 */
static void
s_diSeqHaltIntrDisable_GM10X(void)
{
    LwU32 regVal;

    regVal = REG_RD32(CSB, LW_CMSDEC_FALCON_IRQMCLR);
    regVal = FLD_SET_DRF(_CMSDEC, _FALCON_IRQMCLR, _HALT, _SET, regVal);
    REG_WR32_STALL(CSB, LW_CMSDEC_FALCON_IRQMCLR, regVal);
}

/*!
 * @brief Enables the Halt Interrupt
 * This is counter part of ref@s_diSeqHaltIntrDisable_GM10X.
 */
static void
s_diSeqHaltIntrEnable_GM10X(void)
{
    LwU32 regVal;

    // Clear the Halt Interrupt before  unmasking HALT interrupt
    regVal = REG_RD32(CSB, LW_CMSDEC_FALCON_IRQSCLR);
    regVal = FLD_SET_DRF(_CMSDEC, _FALCON_IRQSCLR, _HALT, _SET, regVal);
    REG_WR32_STALL(CSB, LW_CMSDEC_FALCON_IRQSCLR, regVal);

    regVal = REG_RD32(CSB, LW_CMSDEC_FALCON_IRQMSET);
    regVal = FLD_SET_DRF(_CMSDEC, _FALCON_IRQMSET, _HALT, _SET, regVal);
    REG_WR32_STALL(CSB, LW_CMSDEC_FALCON_IRQMSET, regVal);
}

/*!
 * @brief Handle and replay SCI Interrupts/events
 *
 * @param[in,out]   pCache  Holds cached register values necessary for
 *                          entering into and exiting out of deepidle.
 */
static void
s_diSeqCreateEvents_GM10X
(
    DI_SEQ_CACHE *pCache
)
{
    LwU32           regVal;
    LwU32           regVal2;

    // Relinquish SCI control of the GPIOS (deassert alt_mux_sel)
    regVal  = REG_RD32(FECS, LW_PGC6_SCI_PWR_SEQ_ARMED_SEQ_STATE);
    regVal  = FLD_SET_DRF(_PGC6, _SCI_PWR_SEQ_ARMED_SEQ_STATE, _ALT_MUX_SEL,
                          _DEASSERT, regVal);
    regVal2 = REG_RD32(FECS, LW_PGC6_SCI_PWR_SEQ_STATE);
    regVal2 = FLD_SET_DRF(_PGC6, _SCI_PWR_SEQ_STATE, _UPDATE, _TRIGGER, regVal2);
    REG_WR32(FECS, LW_PGC6_SCI_PWR_SEQ_ARMED_SEQ_STATE, regVal);
    REG_WR32(FECS, LW_PGC6_SCI_PWR_SEQ_STATE, regVal2);

    //
    // NOTE: The only events that we really care about here are:
    // 1. the ones that need sw help (example I2CS)
    // 2. the ones that are pulses in nature and can be missed
    // while we are asleep.(irq)
    //
    // We dont care much about the level signals since the hw
    // is retaining state and will catch the level change once it wakes up.
    //

    // first clear the WAKE_TIMER
    s_diSeqSciWakeTimerEnable_GM10X(LW_FALSE);

    //
    // Initiate i2cs replay if i2cs event is pending.
    // As per bug 1280811 comment #9; no need to check for pending before hitting replay.
    //
    regVal = REG_RD32(FECS, LW_PGC6_SCI_GPU_I2CS_REPLAY);
    DBG_PRINTF_DIDLE(("GC5: Restoring I2CS state", 0, 0, 0, 0));
    regVal = FLD_SET_DRF(_PGC6, _SCI_GPU_I2CS_REPLAY, _THERM_I2CS_STATE, _VALID, regVal);
    REG_WR32(FECS,  LW_PGC6_SCI_GPU_I2CS_REPLAY, regVal);

    //
    // Now check if IRQs are pending. If yes; they need to be replayed.
    // Note: not replaying IRQs is safe since the sink generally resends
    // untill serviced. This can be addressed later if required.
    //
    // if irq pending && status == plugged
    // find corresponding gpio and trigger irq through pmgr frc.
    //
}


/*!
 * @brief Confgiure BSI CLKREQ
 *
 * @param[in]     LwBool  bEntering - 1 entering DI
 *                                    0 exiting  DI
 */
static void
s_diSeqBsiClkreqEventConfig_GM10X
(
    LwBool bEntering
)
{
    LwU32    regVal;

    regVal = REG_RD32(FECS, LW_PGC6_BSI_CTRL);
    if (bEntering)
    {
        regVal = FLD_SET_DRF(_PGC6, _BSI_CTRL, _BSI2SCI_CLKREQ, _ENABLE, regVal);
    }
    else
    {
        regVal = FLD_SET_DRF(_PGC6, _BSI_CTRL, _BSI2SCI_CLKREQ, _DISABLE, regVal);
    }
    REG_WR32(FECS, LW_PGC6_BSI_CTRL, regVal);
}

/*!
 * @brief Disable CLKREQEN in BSI_PADCTRL
 */
static void
s_diSeqBsiPadCtrlClkReqDisable_GM10X(void)
{
    LwU32    regVal;

    regVal = REG_RD32(FECS, LW_PGC6_BSI_PADCTRL);
    regVal = FLD_SET_DRF(_PGC6, _BSI_PADCTRL, _CLKREQEN, _DISABLE, regVal);
    REG_WR32(FECS, LW_PGC6_BSI_PADCTRL, regVal);
}

/*!
 * Mask BSI RXSTAT event
 */
static void
s_diSeqMaskBsiRxStatEvent_GM10X(void)
{
    LwU32    regVal;

    // Mask RXSTAT idle to the BSI.
    regVal = REG_RD32(FECS, LW_PGC6_BSI_CTRL);
    regVal = FLD_SET_DRF(_PGC6, _BSI_CTRL, _BSI2SCI_RXSTAT, _DISABLE, regVal);
    REG_WR32(FECS, LW_PGC6_BSI_CTRL, regVal);
}

/*!
 * @brief Switch SYSCLK to SPPLL
 *
 * On DI entry we switch SYSCLK source to SPPLL so that we can disable
 * SYS PLL.
 *
 * @param[in,out]   pCache  Holds cached register values necessary for
 *                          entering into and exiting out of deepidle.
 */
void
diSeqSwitchSysClkToAltPath_GM10X
(
    DI_SEQ_CACHE *pCache
)
{
    LwU32 regVal;

    //
    // check if sys2clk is set to SYSPLL
    // if yes; change it to sppll
    //
    // Read the clock register
    regVal = REG_RD32(FECS, LW_PTRIM_SYS_SYS2CLK_OUT_SWITCH);
    pCache->sys2ClkCfgFinal = regVal;

    if (FLD_TEST_DRF(_PTRIM, _SYS_SYS2CLK_OUT_SWITCH, _FINALSEL, _ONESRCCLK, regVal))
    {
        // if (onesrcclk == onesrc1 && oesrc1_select == syspll)
        if (FLD_TEST_DRF(_PTRIM, _SYS_SYS2CLK_OUT_SWITCH, _ONESRCCLK, _ONESRC1, regVal) &&
            FLD_TEST_DRF(_PTRIM, _SYS_SYS2CLK_OUT_SWITCH, _ONESRCCLK1_SELECT, _SYSPLL, regVal))
        {
            // Switch Clock to SPPLL0
            regVal = FLD_SET_DRF(_PTRIM, _SYS_SYS2CLK_OUT_SWITCH, _ONESRCCLK,
                                 _ONESRC0, regVal);
            REG_WR32(FECS, LW_PTRIM_SYS_SYS2CLK_OUT_SWITCH, regVal);
        }
    }
}

/*!
 * @brief Restore SYSCLK to SYSPLL
 *
 * Restore SYSCLK source to SYSPLL on DI exit.
 *
 * @param[in]       pCache  Holds cached register values necessary for
 *                          entering into and exiting out of deepidle.
 */
void
diSeqRestoreSysClkToSyspll_GM10X
(
    DI_SEQ_CACHE *pCache
)
{
    // Restore the originally saved value
    REG_WR32(FECS, LW_PTRIM_SYS_SYS2CLK_OUT_SWITCH, pCache->sys2ClkCfgFinal);
}

/*!
 * @brief SYS partition clock action for DI
 *
 * On DI entry, this function Power off the sppll, xtal4xpll and
 *               switch utils, pwr and sys clks to xtal.
 * On Exit, restore state.
 *
 * @param[in,out]   pCache  Holds cached register values necessary for
 *                          entering into and exiting out of deepidle.
 * @param[in]       bExit   LW_TRUE on exit
 *                          LW_FALSE on entry
 */
static void
s_diSeqSysPartitionClocksAction_GM10X
(
    DI_SEQ_CACHE *pCache,
    LwBool        bExit
)
{
    if (bExit)
    {
        // Re-enable SPPLLs
        if (PG_IS_SF_ENABLED(Di, DI, SPPLLS))
        {
            diSeqPllPowerup_HAL(&Di, Di.pPllsSppll, Di.pllCountSppll);
        }
        // Re enable XTAL4XPLL
        if (PG_IS_SF_ENABLED(Di, DI, XTAL4XPLL))
        {
            diSeqPllPowerup_HAL(&Di, Di.pPllsXtal4x, Di.pllCountXtal4x);
        }

        // Restore clocks to the original sources
        if (PG_IS_SF_ENABLED(Di, DI, XTAL_SRC_SWITCH))
        {
            s_diSeqXtalClkSrcRestore_GM10X();
        }
    }
    else
    {
        //
        // Change clocks that are still necessary to be sourced from
        // XTAL instead of SPPLLs
        //
        if (PG_IS_SF_ENABLED(Di, DI, XTAL_SRC_SWITCH))
        {
            diSeqXtalClkSrcSet_HAL();
        }

        // Shut down XTAL4XPLL
        if (PG_IS_SF_ENABLED(Di, DI, XTAL4XPLL))
        {
            diSeqPllPowerdown_HAL(&Di, Di.pPllsXtal4x, Di.pllCountXtal4x);
        }

        // Shut down SPPLLs
        if (PG_IS_SF_ENABLED(Di, DI, SPPLLS))
        {
            diSeqPllPowerdown_HAL(&Di, Di.pPllsSppll, Di.pllCountSppll);
        }
    }
}

/*!
 * @brief Puts GPU into Deep Idle sub-step define by stepId
 *
 * @param[in] stepId    Step identifier
 *
 * @return      FLCN_OK     on success
 *              FLCN_ERROR  on abort
 */
FLCN_STATUS
diSeqEnter_GM10X
(
    LwU8 stepId
)
{
    DI_SEQ_CACHE *pCache  = DI_SEQ_GET_CAHCE();
    FLCN_STATUS   status  = FLCN_OK;
    LPWR_CG      *pLpwrCg = LPWR_GET_CG();

    switch (stepId)
    {
        case DI_SEQ_STEP_PREPARATIONS:
        {
            if (PMUCFG_FEATURE_ENABLED(PMU_PGISLAND_SCI_PMGR_GPIO_SYNC))
            {
                pgIslandSciPmgrGpioSync_HAL(&Pg, Pg.gpioPinMask);
            }

            // Initialize SCI Entry, SCI Exit latencies and DI residency.
            pCache->sciEntryLatencyUs = 0;
            pCache->sciExitLatencyUs  = 0;
            pCache->residentTimeUs    = 0;

            // Clear abort reasons from previous cycle
            Di.abortReasonMask        = 0;

            if (PMUCFG_FEATURE_ENABLED(PMU_DI_SEQ_SWITCH_IDLE_DETECTOR_CIRLWIT))
            {
                //
                // Find the lowest active lane in order to later force enable
                // the aux idle detect logic on that lane. We do not anticipate
                // this to change in every GC5 sequence, but keeping this here
                // in any case. Maybe in the future, GC6 could reset some PEX
                // state that requires re-training and in case active lanes
                // change, we should be able to handle that scenario. This is
                // part of the SW WAR for bug 1367027.
                //
                pCache->lowestPexActiveLaneBit = s_diSeqFindLowestActiveLane_GM10X();

                //
                // Mask rxstat_idle event to BSI. This is part of the SW WAR
                // for bug 1367027. A switch between the normal and aux idle
                // detect cirlwits can cause a glitch in the deep L1 state and
                // kick us out of deep L1. That glitch will also travel to the
                // BSI and cause it to think there was traffic on the bus and
                // abort GC5 as soon as we enter. Thus, we mask off the RX_STAT
                // idle signal to the BSI at the beginning of our sequence while
                // we're still on xtal4x, force the switch to the aux idle
                // detect circuit and then unmask this event to BSI again.
                //
                // Masking / Unmasking WAR is not needed for pascal and later
                // since only one idle detector circuit is present on later chips
                // hence no switching (no glitch) is required.
                //
                s_diSeqMaskBsiRxStatEvent_GM10X();
            }

            //
            // Set supplemental idle counter mode of Adaptive GC5 Histogram
            // to FORCE_IDLE. This will ensure that histogram counters will
            // not get reset. We are not expecting any traffic at this
            // point. This is precautionary step.
            //
            if (PMUCFG_FEATURE_ENABLED(PMU_AP_CTRL_DI_DEEP_L1)     &&
                AP_IS_CTRL_SUPPORTED(RM_PMU_AP_CTRL_ID_DI_DEEP_L1) &&
                AP_IS_CTRL_ENABLED(RM_PMU_AP_CTRL_ID_DI_DEEP_L1))
            {
                pgApConfigCntrMode_HAL(&Pg, RM_PMU_AP_IDLE_MASK_DI_DEEP_L1,
                                            PG_SUPP_IDLE_COUNTER_MODE_FORCE_IDLE);
            }

            // BSI CLKREQ should be unmasked in L1SS
            if (DI_IS_PEX_SLEEP_STATE(L1SS, Di.pexSleepState))
            {
                s_diSeqBsiClkreqEventConfig_GM10X(LW_TRUE);
            }

            //
            // GR-ELCG <-> OSM CG WAR
            // Disable GR-ELCG before gating clock from OSM
            //
            if (PMUCFG_FEATURE_ENABLED(PMU_CG_GR_ELCG_OSM_WAR))
            {
                lpwrCgElcgDisable(PMU_ENGINE_GR, LPWR_CG_ELCG_CTRL_REASON_DI);
            }

            // Save PEX DFE values
            if (PG_IS_SF_ENABLED(Di, DI, DFE_VAL))
            {
                diSeqDfeValuesSave_HAL(&Di, pCache);
            }
            break;
        }

        case DI_SEQ_STEP_MEMORY:
        {
            OBJPGSTATE *pMsState = GET_OBJPGSTATE(RM_PMU_LPWR_CTRL_ID_MS_MSCG);
            if (LPWR_CTRL_IS_SF_ENABLED(pMsState, MS, CG))
            {
                // Unblock the Priv ring
                msDisablePrivAccess_HAL(&Ms, LW_FALSE);

                // Ungate and restore all clocks to Pre-PG_ENG State
                lpwrCgPowerup_HAL(&Lpwr, pLpwrCg->clkDomainSupportMask,
                                  LPWR_CG_TARGET_MODE_ORG);
            }

            // Disable CML clocks
            if (PG_IS_SF_ENABLED(Di, DI, MEM_ACTION))
            {
                diSeqMemPllAction_HAL(&Di, pCache, LW_TRUE);
            }
            break;
        }

        case DI_SEQ_STEP_CHILD_PLLS:
        {
            // We need to start powering down PLLs. Clocks should move to ALT Path
            lpwrCgPowerdown_HAL(&Lpwr, pLpwrCg->clkDomainSupportMask,
                                LPWR_CG_TARGET_MODE_ALT);

            // Shutdown PLLs if required
            if (PG_IS_SF_ENABLED(Di, DI, CORE_PLLS) ||
                PG_IS_SF_ENABLED(Di, DI, CORE_NAFLLS))
            {
                // switch sys2clk clk to sppll before powering down syspll
                diSeqSwitchSysClkToAltPath_HAL(&Di, pCache);

                // Powerdown ADCs if CORE_NAFLLs are enabled
                diSeqPllPowerdown_HAL(&Di, Di.pPllsCore, Di.pllCountCore);

                if (PG_IS_SF_ENABLED(Di, DI, CORE_NAFLLS))
                {
                    diSeqAdcPowerAction_HAL(&Di, LW_TRUE);
                }
            }
            break;
        }

        case DI_SEQ_STEP_OSM_CLOCKS:
        {
            lpwrCgPowerdown_HAL(&Lpwr, pLpwrCg->clkDomainSupportMask,
                                LPWR_CG_TARGET_MODE_GATED);

            // Block The Priv Ring
            msDisablePrivAccess_HAL(&Ms, LW_TRUE);

            // Disable one source clocks
            if (PG_IS_SF_ENABLED(Di, DI, OSM_CLKS))
            {
                //
                // Use STOPCLK HW to gate the OSMs for clocks that wont be
                //  needed for rest of the sequence.
                //
                s_diSeqOsmClocksGate_GM10X();
            }

            //Update PSI state
            if (PMUCFG_FEATURE_ENABLED(PMU_PSI))
            {
                s_diSeqPsiHandlerEntry_GM10X();

                if (PMUCFG_FEATURE_ENABLED(PMU_PGISLAND_SCI_PMGR_GPIO_SYNC))
                {
                    pgIslandSciPmgrGpioSync_HAL(&Pg, BIT(psiGpioPinGet(
                                                RM_PMU_PSI_RAIL_ID_LWVDD)));
                }
            }
            break;
        }

        case DI_SEQ_STEP_UTILS_STATE:
        {
            // Program HW timer if needed.
            if (!s_diSeqWakeTimerProgram_GM10X())
            {
                // Programming ISLAND  failed, Abort the GC5 entry sequence
                status = FLCN_ERROR;
                break;
            }

            // Check for I2C secondary idle and program SCI with I2CS state
            if (!s_diSeqSaveThermPwmStateOnI2cIdle_GM10X(pCache, LW_TRUE))
            {
                // Programming ISLAND  failed, Abort the GC5 entry sequence
                status = FLCN_ERROR;
                break;
            }

            // Power Off the Thermal Sensor
            if (PG_IS_SF_ENABLED(Di, DI, THERM_SENSOR))
            {
                diSeqThermSensorPowerOff_HAL(&Di, pCache);
            }
            break;
        }

        case DI_SEQ_STEP_PARENT_PLLS:
        {
            //
            // The expected behavior is for PCI Express auxiliary idle
            // detection logic to be on during the entirety of GC5, but
            // if it is not, then PCIE traffic cannot wake up the SCI.
            // The aux idle detection logic, as it is designed today,
            // depends on the deep L1 signal being high in order to toggle
            // RX_STAT_IDLE. However, internal GPU events can cause the deep
            // L1 signal to go low and thus, never toggle RX_STAT_IDLE for
            // the islands to see an exit event. If we fail to wake up from
            // GC5, PEX will send TS1 packets for some time and eventually
            // give up on us and we will fall off the bus. This is a SW WAR
            // to disable events from causing deep L1 exits while we are in
            // in GC5. See HW bugs 1343194, 1359137.
            // Bug to track a HW fix in the future: bug 1361199
            //
            // Steps we use:
            // 1. Disable all events from causing deep L1 exits
            // 2. Re-check deep L1 status to make sure that an event didn't
            // happen just before we disabled internal wakeups from causing
            // deep L1 exits. There could be a propagation delay between
            // the internal event oclwrring and the deep L1 status reflecting
            // that we're out of deep L1, so re-read the register from #1
            // to make sure that we take care of this delay.
            // 3. If deep L1 status reports that we're out of deep L1, abort.
            //
            if (DI_IS_PEX_SLEEP_STATE(DEEP_L1, Di.pexSleepState))
            {
                s_diSeqDeepL1WakeEventsDisable_GM10X(pCache, LW_TRUE);

                //
                // Dummy read to make sure that any propagation delay in
                // the wire between us disabling internal events causing
                // wakeups and checking deep L1 status is accounted for.
                //
                REG_RD32(BAR0_CFG, LW_XVE_PRIV_XV_BLKCG2);

                if (FLD_TEST_DRF(_CPWR, _PMU_GPIO_INPUT, _XVXP_DEEP_IDLE, _FALSE,
                                 REG_RD32(CSB, LW_CPWR_PMU_GPIO_INPUT)))
                {
                    //
                    // ABORT SEQUENCE:
                    //
                    // We're not in deep L1. Re-enable internal events to cause
                    // deep L1 exits, exit critical section, and abort.
                    //
                    s_diSeqDeepL1WakeEventsDisable_GM10X(pCache, LW_FALSE);
                    DI_SEQ_ABORT_REASON_UPDATE_NAME(DEEP_L1);

                    status = FLCN_ERROR;
                    break;
                }
            }

            //
            // Program the therm using the SCI vid settings given the utilsclk
            // is going to be xtal freq.
            //
            diSeqVidSwitch_HAL(&Di, pCache, LW_FALSE/*entry*/);

            // Touch the sppll
            s_diSeqSysPartitionClocksAction_GM10X(pCache, LW_FALSE/*entry*/);

            // Recheck I2C secondary idle. Abort entry if I2C secondary not idle.
            if (!s_diSeqSaveThermPwmStateOnI2cIdle_GM10X(pCache, LW_FALSE/*dont save vid*/))
            {
                //
                // ABORT SEQUENCE:
                //
                // We need to be undoing all stuff in this states and restore
                // state to UTILS_STATE.
                //

                // Restore the clocks in this section
                s_diSeqSysPartitionClocksAction_GM10X(pCache, LW_TRUE/*exit*/);

                //
                // Bump up the scheduler ticks.
                // We aborted, so only clock switch time has passed
                //
                lwrtosTimeOffsetus(2 * LWRTOS_TICK_PERIOD_US);

                diSeqVidSwitch_HAL(&Di, pCache, LW_TRUE/*exit*/);

                if (DI_IS_PEX_SLEEP_STATE(DEEP_L1, Di.pexSleepState))
                {
                    // Restore the deep L1 exit events
                    s_diSeqDeepL1WakeEventsDisable_GM10X(pCache, LW_FALSE);
                }

                status = FLCN_ERROR;
                break;
            }

            //
            // Force enable the aux idle detect logic on the lowest
            // active lane. This is part of the SW WAR for bug 1367027.
            // We should have already masked off RX_STAT idle signal to
            // the BSI so the glitch that oclwrs in the switch from the
            // normal idle detect circuit to the aux idle detect circuit
            // should not travel to the BSI.
            //
            if (PMUCFG_FEATURE_ENABLED(PMU_DI_SEQ_SWITCH_IDLE_DETECTOR_CIRLWIT))
            {
                diSeqAuxRxIdleSet_HAL(&Di, pCache, pCache->lowestPexActiveLaneBit);
            }
            break;
        }
    }

    return status;
}

/*!
 * @brief Find the lowest active PEX lane
 *
 * Lowest active PEX lane force is required to enable the aux idle detect
 * logic on that lane only. Enabling on all lanes will burn power. Part of
 * SW WAR for bug 1367027.
 */
static LwU32
s_diSeqFindLowestActiveLane_GM10X(void)
{
    //
    // We use the following defines in the array below for quick and efficient
    // lookup. Use compile-time asserts to make sure that manuals changes don't
    // break our assumptions.
    //
    ct_assert(LW_XP_PL_LINK_STATUS_0_ACTIVE_RX_LANES_NULL  == 0x00000000);
    ct_assert(LW_XP_PL_LINK_STATUS_0_ACTIVE_RX_LANES_00_00 == 0x00000001);
    ct_assert(LW_XP_PL_LINK_STATUS_0_ACTIVE_RX_LANES_01_00 == 0x00000002);
    ct_assert(LW_XP_PL_LINK_STATUS_0_ACTIVE_RX_LANES_01_01 == 0x00000003);
    ct_assert(LW_XP_PL_LINK_STATUS_0_ACTIVE_RX_LANES_03_00 == 0x00000004);
    ct_assert(LW_XP_PL_LINK_STATUS_0_ACTIVE_RX_LANES_02_02 == 0x00000005);
    ct_assert(LW_XP_PL_LINK_STATUS_0_ACTIVE_RX_LANES_03_02 == 0x00000006);
    ct_assert(LW_XP_PL_LINK_STATUS_0_ACTIVE_RX_LANES_03_03 == 0x00000007);
    ct_assert(LW_XP_PL_LINK_STATUS_0_ACTIVE_RX_LANES_07_00 == 0x00000008);
    ct_assert(LW_XP_PL_LINK_STATUS_0_ACTIVE_RX_LANES_04_04 == 0x00000009);
    ct_assert(LW_XP_PL_LINK_STATUS_0_ACTIVE_RX_LANES_05_04 == 0x0000000A);
    ct_assert(LW_XP_PL_LINK_STATUS_0_ACTIVE_RX_LANES_05_05 == 0x0000000B);
    ct_assert(LW_XP_PL_LINK_STATUS_0_ACTIVE_RX_LANES_07_04 == 0x0000000C);
    ct_assert(LW_XP_PL_LINK_STATUS_0_ACTIVE_RX_LANES_06_06 == 0x0000000D);
    ct_assert(LW_XP_PL_LINK_STATUS_0_ACTIVE_RX_LANES_07_06 == 0x0000000E);
    ct_assert(LW_XP_PL_LINK_STATUS_0_ACTIVE_RX_LANES_07_07 == 0x0000000F);
    ct_assert(LW_XP_PL_LINK_STATUS_0_ACTIVE_RX_LANES_15_00 == 0x00000010);
    ct_assert(LW_XP_PL_LINK_STATUS_0_ACTIVE_RX_LANES_08_08 == 0x00000011);
    ct_assert(LW_XP_PL_LINK_STATUS_0_ACTIVE_RX_LANES_09_08 == 0x00000012);
    ct_assert(LW_XP_PL_LINK_STATUS_0_ACTIVE_RX_LANES_09_09 == 0x00000013);
    ct_assert(LW_XP_PL_LINK_STATUS_0_ACTIVE_RX_LANES_11_08 == 0x00000014);
    ct_assert(LW_XP_PL_LINK_STATUS_0_ACTIVE_RX_LANES_10_10 == 0x00000015);
    ct_assert(LW_XP_PL_LINK_STATUS_0_ACTIVE_RX_LANES_11_10 == 0x00000016);
    ct_assert(LW_XP_PL_LINK_STATUS_0_ACTIVE_RX_LANES_11_11 == 0x00000017);
    ct_assert(LW_XP_PL_LINK_STATUS_0_ACTIVE_RX_LANES_15_08 == 0x00000018);
    ct_assert(LW_XP_PL_LINK_STATUS_0_ACTIVE_RX_LANES_12_12 == 0x00000019);
    ct_assert(LW_XP_PL_LINK_STATUS_0_ACTIVE_RX_LANES_13_12 == 0x0000001A);
    ct_assert(LW_XP_PL_LINK_STATUS_0_ACTIVE_RX_LANES_13_13 == 0x0000001B);
    ct_assert(LW_XP_PL_LINK_STATUS_0_ACTIVE_RX_LANES_15_12 == 0x0000001C);
    ct_assert(LW_XP_PL_LINK_STATUS_0_ACTIVE_RX_LANES_14_14 == 0x0000001D);
    ct_assert(LW_XP_PL_LINK_STATUS_0_ACTIVE_RX_LANES_15_14 == 0x0000001E);
    ct_assert(LW_XP_PL_LINK_STATUS_0_ACTIVE_RX_LANES_15_15 == 0x0000001F);

    static LwU8 lowestPexActiveLaneMapping_GM10X[] = {
     0, // LW_XP_PL_LINK_STATUS_0_ACTIVE_RX_LANES_NULL   0x00000000 => PMU_HALT()
     0, // LW_XP_PL_LINK_STATUS_0_ACTIVE_RX_LANES_00_00  0x00000001*
     0, // LW_XP_PL_LINK_STATUS_0_ACTIVE_RX_LANES_01_00  0x00000002*
     1, // LW_XP_PL_LINK_STATUS_0_ACTIVE_RX_LANES_01_01  0x00000003*
     0, // LW_XP_PL_LINK_STATUS_0_ACTIVE_RX_LANES_03_00  0x00000004*
     2, // LW_XP_PL_LINK_STATUS_0_ACTIVE_RX_LANES_02_02  0x00000005*
     2, // LW_XP_PL_LINK_STATUS_0_ACTIVE_RX_LANES_03_02  0x00000006*
     3, // LW_XP_PL_LINK_STATUS_0_ACTIVE_RX_LANES_03_03  0x00000007*
     0, // LW_XP_PL_LINK_STATUS_0_ACTIVE_RX_LANES_07_00  0x00000008*
     4, // LW_XP_PL_LINK_STATUS_0_ACTIVE_RX_LANES_04_04  0x00000009*
     4, // LW_XP_PL_LINK_STATUS_0_ACTIVE_RX_LANES_05_04  0x0000000A*
     5, // LW_XP_PL_LINK_STATUS_0_ACTIVE_RX_LANES_05_05  0x0000000B*
     4, // LW_XP_PL_LINK_STATUS_0_ACTIVE_RX_LANES_07_04  0x0000000C*
     6, // LW_XP_PL_LINK_STATUS_0_ACTIVE_RX_LANES_06_06  0x0000000D*
     6, // LW_XP_PL_LINK_STATUS_0_ACTIVE_RX_LANES_07_06  0x0000000E*
     7, // LW_XP_PL_LINK_STATUS_0_ACTIVE_RX_LANES_07_07  0x0000000F*
     0, // LW_XP_PL_LINK_STATUS_0_ACTIVE_RX_LANES_15_00  0x00000010*
     8, // LW_XP_PL_LINK_STATUS_0_ACTIVE_RX_LANES_08_08  0x00000011*
     8, // LW_XP_PL_LINK_STATUS_0_ACTIVE_RX_LANES_09_08  0x00000012*
     9, // LW_XP_PL_LINK_STATUS_0_ACTIVE_RX_LANES_09_09  0x00000013*
     8, // LW_XP_PL_LINK_STATUS_0_ACTIVE_RX_LANES_11_08  0x00000014*
    10, // LW_XP_PL_LINK_STATUS_0_ACTIVE_RX_LANES_10_10  0x00000015*
    10, // LW_XP_PL_LINK_STATUS_0_ACTIVE_RX_LANES_11_10  0x00000016*
    11, // LW_XP_PL_LINK_STATUS_0_ACTIVE_RX_LANES_11_11  0x00000017*
     8, // LW_XP_PL_LINK_STATUS_0_ACTIVE_RX_LANES_15_08  0x00000018*
    12, // LW_XP_PL_LINK_STATUS_0_ACTIVE_RX_LANES_12_12  0x00000019*
    12, // LW_XP_PL_LINK_STATUS_0_ACTIVE_RX_LANES_13_12  0x0000001A*
    13, // LW_XP_PL_LINK_STATUS_0_ACTIVE_RX_LANES_13_13  0x0000001B*
    12, // LW_XP_PL_LINK_STATUS_0_ACTIVE_RX_LANES_15_12  0x0000001C*
    14, // LW_XP_PL_LINK_STATUS_0_ACTIVE_RX_LANES_14_14  0x0000001D*
    14, // LW_XP_PL_LINK_STATUS_0_ACTIVE_RX_LANES_15_14  0x0000001E*
    15, // LW_XP_PL_LINK_STATUS_0_ACTIVE_RX_LANES_15_15  0x0000001F*
    };
    LwU32 lowestPexActiveLaneBit = 0;
    LwU8    activeRxLines;

    activeRxLines = DRF_VAL(_XP, _PL_LINK_STATUS_0, _ACTIVE_RX_LANES,
                            REG_RD32(BAR0, LW_XP_PL_LINK_STATUS_0(0)));

    if (activeRxLines == LW_XP_PL_LINK_STATUS_0_ACTIVE_RX_LANES_NULL)
    {
        PMU_HALT();
    }
    else
    {
        lowestPexActiveLaneBit =
            BIT(lowestPexActiveLaneMapping_GM10X[activeRxLines]);
    }
    return lowestPexActiveLaneBit;
}

/*!
 * @brief Take-out GPU from Deep Idle sub-step define by stepId
 *
 * @param[in] stepId    Step identifier
 */
void
diSeqExit_GM10X
(
    LwU8 stepId
)
{
    DI_SEQ_CACHE *pCache  = DI_SEQ_GET_CAHCE();
    LPWR_CG      *pLpwrCg = LPWR_GET_CG();
    LwU32         incrementUs;

    switch (stepId)
    {
        case DI_SEQ_STEP_PARENT_PLLS:
        {
            // Restore the clocks in this section
            s_diSeqSysPartitionClocksAction_GM10X(pCache, LW_TRUE/*exit*/);

            //
            // Restore the PEX aux rx idle state. Part of SW WAR for bug
            // 1367027. This is okay to restore once we have brought back
            // up clocks to xtal4x since we take LOCKDET latency to exit
            // deep L1. We only do this if havnt aborted DI entry.
            //
            // First copy clear the SCI pending interrupts so that the
            // current intr status is copied to the previous intr status
            // and any glitch caused by switching idle detector cirlwits
            // will not be sent up to the client as part of the abort or
            // exit statistics.
            //
            diSeqCopyClearSciPendingIntrs_HAL(&Di);
            diSeqAuxRxIdleRestore_HAL(&Di, pCache);

            diSeqVidSwitch_HAL(&Di, pCache, LW_TRUE/*exit*/);

            if (DI_IS_PEX_SLEEP_STATE(DEEP_L1, Di.pexSleepState))
            {
                // Restore the deep L1 exit events
                s_diSeqDeepL1WakeEventsDisable_GM10X(pCache, LW_FALSE);
            }

            //
            // Update DI cache for SCI interrupts:
            // We have triggered "Copy Clear". Thus valid interrupt values
            // are present in the SW_INTR0/1_STATUS_PREV.
            //
            pCache->intrStatus.reg[RM_PMU_DIDLE_INTR_STATUS_REG_SCI_INTR_0] =
                REG_RD32(FECS, LW_PGC6_SCI_SW_INTR0_STATUS_PREV);
            pCache->intrStatus.reg[RM_PMU_DIDLE_INTR_STATUS_REG_SCI_INTR_1] =
                REG_RD32(FECS, LW_PGC6_SCI_SW_INTR1_STATUS_PREV);

            // Check if SCI aborted for DI
            if (FLD_TEST_DRF(_PGC6,_SCI_SW_INTR0_STATUS_LWRR, _GC5_ABORT, _PENDING,
                             pCache->intrStatus.reg[RM_PMU_DIDLE_INTR_STATUS_REG_SCI_INTR_0]))
            {
                DI_SEQ_ABORT_REASON_UPDATE_NAME(SCI_SM);
            }

            // Get SCI Entry, SCI Exit and DI residency
            pCache->sciEntryLatencyUs = REG_RD32(FECS, LW_PGC6_SCI_ENTRY_TMR);
            pCache->sciExitLatencyUs  = REG_RD32(FECS, LW_PGC6_SCI_EXIT_TMR);
            pCache->residentTimeUs    = REG_RD32(FECS, LW_PGC6_SCI_RESIDENT_TMR);

            // Bump up the scheduler ticks
            lwrtosTimeOffsetus(2 * LWRTOS_TICK_PERIOD_US + pCache->residentTimeUs);
            break;
        }

        case DI_SEQ_STEP_UTILS_STATE:
        {
            if (DI_IS_PEX_SLEEP_STATE(L1SS, Di.pexSleepState))
            {
                //
                // In L1 susbstates, BSI CLKREQ mask should restored as it would have
                //  unmasked during GC5 entry
                //
                s_diSeqBsiClkreqEventConfig_GM10X(LW_FALSE);
            }

            //
            // Disable the CLKREQEN in BSI_PADCTRL always during DI exit.
            //
            // 1) HW autosets this when BSI_CTRL_MODE in DI. This will cause
            //    us to exit L1 substates properly
            // 2) XP3G can't de-assert CLKREQ when LW_PGC6_BSI_PADCTRL_CLKREQEN_ENABLE
            //     is set. Thus disable it to give complete control of CLKREQ to XP3G.
            //
            s_diSeqBsiPadCtrlClkReqDisable_GM10X();

            // Now update the host ptimer given the hostclk is 108 Mhz.
            s_diSeqHostPtimerUpdate_GM10X();

            // Start the Thermal Sensor
            if (PG_IS_SF_ENABLED(Di, DI, THERM_SENSOR))
            {
                diSeqThermSensorPowerOn_HAL(&Di, pCache);
            }
            break;
        }

        case DI_SEQ_STEP_OSM_CLOCKS:
        {
            // Update PSI
            if (PMUCFG_FEATURE_ENABLED(PMU_PSI))
            {
                s_diSeqPsiHandlerExit_GM10X();

                // Synchronise GPIO pin for PSI
                if (PMUCFG_FEATURE_ENABLED(PMU_PGISLAND_SCI_PMGR_GPIO_SYNC))
                {
                    pgIslandSciPmgrGpioSync_HAL(&Pg, BIT(psiGpioPinGet(
                                                RM_PMU_PSI_RAIL_ID_LWVDD)));
                }
            }

            // Turn one source clocks back on
            if (PG_IS_SF_ENABLED(Di, DI, OSM_CLKS))
            {
                s_diSeqOsmClocksUngate_GM10X();
            }

            // Unblock the Priv Ring
            msDisablePrivAccess_HAL(&Ms, LW_FALSE);

            // We need core clocks on ALT path to perform Core PLL Powerup
            lpwrCgPowerup_HAL(&Lpwr, pLpwrCg->clkDomainSupportMask,
                              LPWR_CG_TARGET_MODE_ALT);

            break;
        }

        case DI_SEQ_STEP_CHILD_PLLS:
        {
            // Restore PLLs to same configuration as they were earlier
            if (PG_IS_SF_ENABLED(Di, DI, CORE_PLLS) ||
                PG_IS_SF_ENABLED(Di, DI, CORE_NAFLLS))
            {
                // Wait for voltage settling before NAFLL powerup
                if ((PG_IS_SF_ENABLED(Di, DI, CORE_NAFLLS)) &&
                    (PG_IS_SF_ENABLED(Di, DI, VOLTAGE_UPDATE)))
                {
                    LwU32 lwrrentTimeHi, elapsedTimeNs;
                    LwS32 diffNs;
                    LwU32 lwrrentTimeLo = 0;

                    diSnapshotSciPtimer_HAL(&Di, &lwrrentTimeLo,
                                                 &lwrrentTimeHi);
                    elapsedTimeNs = lwrrentTimeLo - pCache->diExitStartTimeLo;
                    diffNs = (Di.voltageSettleTimeUs * 1000) - elapsedTimeNs;
                    if (diffNs > 0)
                    {
                        OS_PTIMER_SPIN_WAIT_NS(diffNs);
                    }
                }

                diSeqPllPowerup_HAL(&Di, Di.pPllsCore, Di.pllCountCore);

                // Powerup ADCs if CORE_NAFLLs are enabled
                if (PG_IS_SF_ENABLED(Di, DI, CORE_NAFLLS))
                {
                    diSeqAdcPowerAction_HAL(&Di, LW_FALSE);
                }

                // restore sys2clk to syspll if applicable
                diSeqRestoreSysClkToSyspll_HAL(&Di, pCache);
            }
            break;
        }

        case DI_SEQ_STEP_MEMORY:
        {
            // Restore clocks to pre-MSCG state so that memPLL powerup can be performed faster.
            lpwrCgPowerup_HAL(&Lpwr, pLpwrCg->clkDomainSupportMask,
                              LPWR_CG_TARGET_MODE_ORG);

            // Restore CML clocks
            if (PG_IS_SF_ENABLED(Di, DI, MEM_ACTION))
            {
                diSeqMemPllAction_HAL(&Di, pCache, LW_FALSE);
            }

            OBJPGSTATE *pMsState = GET_OBJPGSTATE(RM_PMU_LPWR_CTRL_ID_MS_MSCG);
            if (LPWR_CTRL_IS_SF_ENABLED(pMsState, MS, CG))
            {
                //
                // Restore all clocks to Pre-DI state
                // Clocks in DI Cache have already been restored.
                // We only need to gate clocks that are not in DI Cache.
                //
                lpwrCgPowerdown_HAL(&Lpwr, (~Di.cgMask), LPWR_CG_TARGET_MODE_GATED);

                // Gate the privring back again
                msDisablePrivAccess_HAL(&Ms, LW_TRUE);
            }
            break;
        }

        case DI_SEQ_STEP_PREPARATIONS:
        {
            // Restore PEX DFE values
            if (PG_IS_SF_ENABLED(Di, DI, DFE_VAL))
            {
                diSeqDfeValuesRestore_HAL(&Di, pCache);
            }

            //
            // GR-ELCG <-> OSM CG WAR
            // Re-enable GR-ELCG before gating clock from OSM
            //
            if (PMUCFG_FEATURE_ENABLED(PMU_CG_GR_ELCG_OSM_WAR))
            {
                lpwrCgElcgEnable(PMU_ENGINE_GR, LPWR_CG_ELCG_CTRL_REASON_DI);
            }

            // Check for irqs and trigger interrupts for pmgr
            s_diSeqCreateEvents_GM10X(pCache);

            if (PMUCFG_FEATURE_ENABLED(PMU_AP_CTRL_DI_DEEP_L1)     &&
                AP_IS_CTRL_SUPPORTED(RM_PMU_AP_CTRL_ID_DI_DEEP_L1) &&
                AP_IS_CTRL_ENABLED(RM_PMU_AP_CTRL_ID_DI_DEEP_L1))
            {
                //
                // We need to increment Histogram counter by time for which
                // PWRCLK was gated.
                //
                // Time for which PWRCLK is gated
                // = Time for which control is transfer to SCI
                // = SCI Entry Time + GC5 Residency Time + SCI Exit Time
                //
                // Reset histogram counting mode to AUTO_IDLE after incrementing
                // counters.
                //
                incrementUs = pCache->sciEntryLatencyUs + pCache->sciExitLatencyUs +
                              pCache->residentTimeUs;

                //
                // Accommodate GC5 sleep time in histogram before restoring
                // histogram counter mode. Counter should be running in
                // FORCE_IDLE mode while adding correction.
                //
                // This is sequence to increment histogram counters:
                // 1) Set histogram counter mode to FORCE IDLE
                // 2) Increment histogram counter
                // 3) Restore histogram counter mode
                //
                apCtrlIncrementHistCntr(RM_PMU_AP_CTRL_ID_DI_DEEP_L1, incrementUs);

                // Restore counter mode
                pgApConfigCntrMode_HAL(&Pg, RM_PMU_AP_IDLE_MASK_DI_DEEP_L1,
                                        PG_SUPP_IDLE_COUNTER_MODE_AUTO_IDLE);
            }
            break;
        }
    }
}

/*!
 * @brief Clear the BSI sequence status registers.
 *
 * This is only useful for debugging purposes to make sure that we clear the
 * status from the (now previous) GC5
 * exit.
 */
void
diSeqBsiSequenceStatusClear_GM10X(void)
{
    REG_WR32(FECS, LW_PGC6_BSI_XTAL_SEQUENCE_STATUS, 0x0);
    REG_WR32(FECS, LW_PGC6_BSI_PWR_SEQUENCE_STATUS,  0x0);
}

/*!
 * @brief PSI sequence handler for DI entry
 */
static void
s_diSeqPsiHandlerEntry_GM10X(void)
{
    if (PMUCFG_FEATURE_ENABLED(PMU_PSI_WITH_SCI))
    {
        //
        // If MSCG coupled PSI is engaged, disengage it for split rail.
        // MSCG will not release ownership of PSI since it needs to
        // re-engage PSI on power up sequence
        //
        if (PMUCFG_FEATURE_ENABLED(PMU_PSI_CTRL_MS)      &&
            PSI_IS_CTRL_SUPPORTED(RM_PMU_PSI_CTRL_ID_MS) &&
            psiIsCtrlEngaged(RM_PMU_PSI_CTRL_ID_MS, RM_PMU_PSI_RAIL_ID_LWVDD))
        {
            psiDisengage(RM_PMU_PSI_CTRL_ID_MS, BIT(RM_PMU_PSI_RAIL_ID_LWVDD),
                         MS_PSI_ENTRY_EXIT_STEPS_MASK_DI_SEQ);

            //
            // Since MSCG has engaged PSI, SCI should also enagage PSI for
            // power down and re-engage PSI for powerup
            //
            pgIslandSciDiPsiSeqUpdate_HAL(&Pg, BIT(PSI_SCI_STEP_INSTR));
        }

        else if (PMUCFG_FEATURE_ENABLED(PMU_PSI_CTRL_DI)      &&
                 PSI_IS_CTRL_SUPPORTED(RM_PMU_PSI_CTRL_ID_DI) &&
                 PG_IS_SF_SUPPORTED(Di, DI, PSI))
        {
            if (PG_IS_SF_ENABLED(Di, DI, PSI))
            {
                //
                // GC5 ucode sequence does not need to engage PSI with SCI
                // Core sequence will just gain ownership and callwlate
                // current.
                //
                psiEngage(RM_PMU_PSI_CTRL_ID_DI, BIT(RM_PMU_PSI_RAIL_ID_LWVDD),
                          DI_PSI_ENTRY_EXIT_STEPS_MASK_WITH_SCI);

                // If GC5 has ownwership of PSI, update SCI accordingly
                if (psiIsCtrlEngaged(RM_PMU_PSI_CTRL_ID_DI, RM_PMU_PSI_RAIL_ID_LWVDD))
                {
                    pgIslandSciDiPsiSeqUpdate_HAL(&Pg,
                        BIT(PSI_SCI_STEP_INSTR));

                    //
                    // Increase PSI engage count for PSI with SCI, this
                    // is not recommended approach, need to clean this
                    // up in follow up CL.
                    //
                    psiCtrlEngageCountIncrease(RM_PMU_PSI_CTRL_ID_DI,
                                               RM_PMU_PSI_RAIL_ID_LWVDD);
                }
                else
                {
                    pgIslandSciDiPsiSeqUpdate_HAL(&Pg,
                         BIT(PSI_SCI_STEP_DISABLE));
                }
            }
            else
            {
                //
                // This is the case where DI coupled PSI is disabled at
                // a particular Pstate. SCI needs to be updated accordingly
                //
                pgIslandSciDiPsiSeqUpdate_HAL(&Pg,
                    BIT(PSI_SCI_STEP_DISABLE));
            }
        }
    }

    else if (PMUCFG_FEATURE_ENABLED(PMU_PSI_CTRL_DI)      &&
             PSI_IS_CTRL_SUPPORTED(RM_PMU_PSI_CTRL_ID_DI) &&
             PG_IS_SF_ENABLED(Di, DI, PSI))
    {
        // Enagage PSI in ucode if PSI with SCI is disabled
         psiEngage(RM_PMU_PSI_CTRL_ID_DI, BIT(RM_PMU_PSI_RAIL_ID_LWVDD),
                   DI_PSI_ENTRY_EXIT_STEPS_MASK_WITH_PMGR);
    }
}

/*!
 * @brief PSI sequence handler for DI exit
 */
static void
s_diSeqPsiHandlerExit_GM10X(void)
{
    if (PMUCFG_FEATURE_ENABLED(PMU_PSI_WITH_SCI))
    {
        if (PMUCFG_FEATURE_ENABLED(PMU_PSI_CTRL_MS)      &&
            PSI_IS_CTRL_SUPPORTED(RM_PMU_PSI_CTRL_ID_MS) &&
            psiIsCtrlEngaged(RM_PMU_PSI_CTRL_ID_MS, RM_PMU_PSI_RAIL_ID_LWVDD))
        {
            //
            // Re-engage MSCG coupled PSI on powerup
            //
            // To-Do: PSI will not be re engaged here, since ALT_MUX_SELECT
            // has not given PMGR control of GPIOs
            // We can optimize sequence by removing SETTLE_TIME from here,
            // and figure out a way to give necessary delay after PMGR
            // has control of GPIOs
            //
            psiEngage(RM_PMU_PSI_CTRL_ID_MS, BIT(RM_PMU_PSI_RAIL_ID_LWVDD),
                      MS_PSI_ENTRY_EXIT_STEPS_MASK_DI_SEQ);
        }
        else if (PMUCFG_FEATURE_ENABLED(PMU_PSI_CTRL_DI) &&
            PSI_IS_CTRL_SUPPORTED(RM_PMU_PSI_CTRL_ID_DI) &&
            psiIsCtrlEngaged(RM_PMU_PSI_CTRL_ID_DI, RM_PMU_PSI_RAIL_ID_LWVDD))
        {
                // Release the PSI ownership
                psiDisengage(RM_PMU_PSI_CTRL_ID_DI, BIT(RM_PMU_PSI_RAIL_ID_LWVDD),
                             DI_PSI_ENTRY_EXIT_STEPS_MASK_WITH_SCI);
        }
    }
    else if (PMUCFG_FEATURE_ENABLED(PMU_PSI_CTRL_DI)      &&
             PSI_IS_CTRL_SUPPORTED(RM_PMU_PSI_CTRL_ID_DI) &&
             psiIsCtrlEngaged(RM_PMU_PSI_CTRL_ID_DI, RM_PMU_PSI_RAIL_ID_LWVDD))
    {
        // Disengage PSI in ucode if PSI with SCI is disabled
        psiDisengage(RM_PMU_PSI_CTRL_ID_DI, BIT(RM_PMU_PSI_RAIL_ID_LWVDD),
                     DI_PSI_ENTRY_EXIT_STEPS_MASK_WITH_PMGR);
    }
}

