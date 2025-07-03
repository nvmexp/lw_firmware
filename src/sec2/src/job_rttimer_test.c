/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2015-2022 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file   job_rttimer_test.c
 * @brief  Entry function for the RT Timer test job
 */
/* ------------------------- System Includes -------------------------------- */
#include "lwuproc.h"
#include "sec2sw.h"
#include "dev_sec_csb.h"

/* ------------------------- Application Includes --------------------------- */
#include "sec2_objrttimer.h"

#include "dev_timer.h"

/* ------------------------- Type Definitions ------------------------------- */
/* ------------------------- External Definitions --------------------------- */
/* ------------------------- Static Function Prototypes  -------------------- */
static void _resetTimerIntr(void)
    GCC_ATTRIB_SECTION("imem_testrttimer", "_resetTimerIntr");
static FLCN_STATUS _pollTimerStat(LwU32 count, LwBool bCheckTime,
                                LwU32 *pElapsedNs)
    GCC_ATTRIB_SECTION("imem_testrttimer", "_pollTimerStat");
static FLCN_STATUS _verifyTimerOneShot(LwU32 count, LwBool bCheckTime,
                                LwU32 *pElapsedNs)
    GCC_ATTRIB_SECTION("imem_testrttimer", "_verifyTimerOneShot");
static FLCN_STATUS _verifyTimerContinuous(LwU32 count, LwBool bCheckTime,
                                LwU32 *pElapsedNs)
    GCC_ATTRIB_SECTION("imem_testrttimer", "_verifyTimerContinuous");

/*!
 * Entry point for the the RT Timer test.
 * The test does the following:
 *      1. Mask the RT Timer (EXTIRQ6)
 *      2. Program the RT Timer based on the count and source
 *      3. Verify the one-shot and continuous RT Timer operation
 *      4. Restore the IRQ mask values
 * @param[in]  count        Value to program in the RT Timer count register.
 * @param[in]  bCheckTime   true if we want to verify the timing against the
 *                          PTIMER clock, false if we want to verify only the
 *                          functionality.
 * 
 * @return     FLCN_OK    => the test passed
 *             FLCN_ERROR => the test failed
 */
FLCN_STATUS rttimer_test_entry
(
    LwU32 count,
    LwBool bCheckTime,
    LwU32 *pOneShotNs,
    LwU32 *pContinuousNs
)
{
    LwU32 irqMask;
    LwBool bRttimerIrq;
    FLCN_STATUS retVal1;
    FLCN_STATUS retVal2;

    //
    // Don't run the test if we're using the RTTIMER for
    // context-switching
    //
    if (SEC2CFG_FEATURE_ENABLED(SEC2RTTIMER_FOR_OS_TICKS))
    {
        return FLCN_ERR_NOT_SUPPORTED;
    }

    // Disable the timer interrupt so we can test it
    irqMask = REG_RD32(CSB, LW_CSEC_FALCON_IRQMASK);
    bRttimerIrq = FLD_TEST_DRF(_CSEC, _FALCON_IRQMASK, _EXT_EXTIRQ6, _ENABLE,
                               irqMask);

    if (bRttimerIrq)
    {
        REG_FLD_WR_DRF_DEF_STALL(CSB, _CSEC, _FALCON_IRQMCLR, _EXT_EXTIRQ6, _SET);
    }

    OSTASK_ATTACH_AND_LOAD_IMEM_OVERLAY(OVL_INDEX_IMEM(rttimer));
    retVal1 = _verifyTimerOneShot(count, bCheckTime, pOneShotNs);

    retVal2 = _verifyTimerContinuous(count, bCheckTime, pContinuousNs);

    OSTASK_DETACH_IMEM_OVERLAY(OVL_INDEX_IMEM(rttimer));

    // Set the timer interrupt to its original value
    if (bRttimerIrq)
    {
        REG_FLD_WR_DRF_DEF_STALL(CSB, _CSEC, _FALCON_IRQMSET, _EXT_EXTIRQ6, _SET);
    }

    return (retVal1 != FLCN_OK) ? retVal1 : retVal2;
}

/*!
 * Reset the RT Timer status bit
 */
static void _resetTimerIntr(void)
{
    LwU32 data;

    data = REG_RD32(CSB, LW_CSEC_FALCON_IRQSCLR);
    data = FLD_SET_DRF(_CSEC, _FALCON_IRQSCLR, _EXT_EXTIRQ6, _SET, data);
    REG_WR32(CSB, LW_CSEC_FALCON_IRQSCLR, data);
}

/*!
 * Poll on the RT Timer IRQSTAT bit
 */
static FLCN_STATUS _pollTimerStat
(
    LwU32 count,
    LwBool bCheckTime,
    LwU32 *pElapsedNs
)
{
    LwU32 data;
    FLCN_STATUS retVal = FLCN_OK;
    FLCN_TIMESTAMP startTime;

    LwU32 intervalUs;
    LwU32 intervalNs;
    LwU32 epsilonNs = 1000000; // 1ms diff either way
    LwU32 higherOk;
    LwU32 lowerOk;

    osPTimerTimeNsLwrrentGet(&startTime);

    // Wait till the IRQ status is set
    do
    {
        // Ideally, we should yield here, but since it's for testing we're ok
        data = REG_RD32(CSB, LW_CSEC_FALCON_IRQSTAT);
    } while (FLD_TEST_DRF(_CSEC, _FALCON_IRQSTAT, _EXT_EXTIRQ6, _FALSE, data));

    if (pElapsedNs != NULL)
    {
        *pElapsedNs = osPTimerTimeNsElapsedGet(&startTime);
    }

    // Reset the IRQ status bit
    _resetTimerIntr();

    if (!bCheckTime || (pElapsedNs == NULL))
    {
        return retVal;
    }

    intervalUs = count;

    // We'll be using nsecs, so colwert intervalUs to nsecs
    intervalNs = intervalUs * 1000;
    higherOk = intervalNs + epsilonNs;
    lowerOk = intervalNs - epsilonNs;

    // Check for overflow
    if (higherOk < intervalNs)
    {
        higherOk = 0xFFFFFFFF;
    }
    if (lowerOk > intervalNs)
    {
        lowerOk = 0;
    }

    // Return FLCN_ERROR if the time is not accurate to 1ms
    if ((*pElapsedNs > higherOk) || (*pElapsedNs < lowerOk))
    {
        retVal = FLCN_ERROR;
    }

    return retVal;
}

/*!
 * Verify the RT Timer in one-shot mode
 * Set up the RT Timer to count down in one-shot mode, verify that
 * the IRQSTAT bit is set at the end of the interval
 */
static FLCN_STATUS _verifyTimerOneShot
(
    LwU32 count,
    LwBool bCheckTime,
    LwU32 *pElapsedNs
)
{
    FLCN_STATUS retVal;

    _resetTimerIntr();
    retVal = rttimerSetup_HAL(&Rttimer, RTTIMER_MODE_START, count, LW_TRUE);

    if (retVal != FLCN_OK)
    {
        return retVal;
    }

    retVal = _pollTimerStat(count, bCheckTime, pElapsedNs);

    (void)rttimerSetup_HAL(&Rttimer, RTTIMER_MODE_STOP, count, LW_TRUE);

    return retVal;
}

/*!
 * Verify the RT Timer in continuous mode
 * Set up the RT Timer to count down in continuous mode, verify that
 * the IRQSTAT bit is set at the end of the interval.
 * Then clear it, and verify that the timer is still running and the
 * IRQSTAT bit is set again, at the end of a second interval.
 */
static FLCN_STATUS _verifyTimerContinuous
(
    LwU32 count,
    LwBool bCheckTime,
    LwU32 *pElapsedNs
)
{
    FLCN_STATUS retVal;

    _resetTimerIntr();
    retVal = rttimerSetup_HAL(&Rttimer, RTTIMER_MODE_START, count, LW_FALSE);

    if (retVal != FLCN_OK)
    {
        return retVal;
    }

    retVal = _pollTimerStat(count, bCheckTime, pElapsedNs);
    if (retVal != FLCN_OK)
    {
        return retVal;
    }

    // Check that the timer is still running
    retVal = _pollTimerStat(count, bCheckTime, NULL);
    (void)rttimerSetup_HAL(&Rttimer, RTTIMER_MODE_STOP, count, LW_FALSE);
    _resetTimerIntr();
    return retVal;
}
