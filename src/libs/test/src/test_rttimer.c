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
 * @file   test_rttimer.c
 * @brief  Entry function for the RT Timer test
 */
/* ------------------------- System Includes -------------------------------- */
#include "lwuproc.h"
#include "regmacros.h"

/* ------------------------- Application Includes --------------------------- */
#include "flcntypes.h"
#include "csb.h"
#include "lwostask.h"
#include "lwoslayer.h"

#if defined(GSPLITE_RTOS)
    #include "dev_gsp_csb.h"

    #define RTTIMER_LIB_IS_RTTIMER_IRQ_ENABLED()  REG_FLD_TEST_DRF_DEF(CSB, _CGSP, _FALCON_IRQMASK, _EXT_EXTIRQ6, _ENABLE)
    #define RTTIMER_LIB_DISABLE_RTTIMER_IRQ()     REG_FLD_WR_DRF_DEF_STALL(CSB, _CGSP, _FALCON_IRQMCLR, _EXT_EXTIRQ6, _SET)
    #define RTTIMER_LIB_ENABLE_RTTIMER_IRQ()      REG_FLD_WR_DRF_DEF_STALL(CSB, _CGSP, _FALCON_IRQMSET, _EXT_EXTIRQ6, _SET)
    #define RTTIMER_LIB_CLEAR_RTTIMER_IRQ()       REG_FLD_WR_DRF_DEF_STALL(CSB, _CGSP, _FALCON_IRQSCLR, _EXT_EXTIRQ6, _SET)
    #define RTTIMER_LIB_IS_RTTIMER_IRQ_SET()      REG_FLD_TEST_DRF_DEF(CSB, _CGSP, _FALCON_IRQSTAT, _EXT_EXTIRQ6, _TRUE)
#else
    #error !!! Unsupported falcon !!!
#endif

/* ------------------------- Type Definitions ------------------------------- */
/* ------------------------- External Definitions --------------------------- */
extern FLCN_STATUS vApplicationRttimerSetup(LwBool, LwU32, LwBool);

/* ------------------------- Static Function Prototypes  -------------------- */
static void _resetTimerIntr(void)
    GCC_ATTRIB_SECTION("imem_testrttimer", "_resetTimerIntr");
static FLCN_STATUS _pollTimerStat(LwU32 count, LwBool bCheckTime, LwU32 *pElapsedNs)
    GCC_ATTRIB_SECTION("imem_testrttimer", "_pollTimerStat");
static FLCN_STATUS _verifyTimerOneShot(LwU32 count, LwBool bCheckTime, LwU32 *pElapsedNs)
    GCC_ATTRIB_SECTION("imem_testrttimer", "_verifyTimerOneShot");
static FLCN_STATUS _verifyTimerContinuous(LwU32 count, LwBool bCheckTime, LwU32 *pElapsedNs)
    GCC_ATTRIB_SECTION("imem_testrttimer", "_verifyTimerContinuous");

/*!
 * Entry point for the the RT Timer test.
 * The test does the following:
 *      1. Mask the RT Timer (EXTIRQ6)
 *      2. Program the RT Timer based on the count and source
 *      3. Verify the one-shot and continuous RT Timer operation
 *      4. Restore the IRQ mask values
 * 
 * @param[in]      count          Value to program in the RT Timer count register.
 * @param[in]      bCheckTime     true if we want to verify the timing against the
 *                                PTIMER clock, false if we want to verify only the
 *                                functionality.
 * @param[in/out]  pOneShotNs     Pointer saving the elapsed time in one shot mode
 * @param[in/out]  pContinuousNs  Pointer saving the elapsed time in continuous mode
 * 
 * @return     FLCN_OK    => the test passed
 *             FLCN_ERROR => the test failed
 */
FLCN_STATUS
rttimer_test_entry
(
    LwU32   count,
    LwBool  bCheckTime,
    LwU32  *pOneShotNs,
    LwU32  *pContinuousNs
)
{
    LwBool      bRttimerIrq;
    FLCN_STATUS retVal1;
    FLCN_STATUS retVal2;

    // Disable the timer interrupt so we can test it
    bRttimerIrq = RTTIMER_LIB_IS_RTTIMER_IRQ_ENABLED();

    if (bRttimerIrq)
    {
        RTTIMER_LIB_DISABLE_RTTIMER_IRQ();
    }

    OSTASK_ATTACH_IMEM_OVERLAY_COND(OVL_INDEX_IMEM(rttimer));
    retVal1 = _verifyTimerOneShot(count, bCheckTime, pOneShotNs);

    retVal2 = _verifyTimerContinuous(count, bCheckTime, pContinuousNs);

    OSTASK_DETACH_IMEM_OVERLAY(OVL_INDEX_IMEM(rttimer));

    // Set the timer interrupt to its original value
    if (bRttimerIrq)
    {
        RTTIMER_LIB_ENABLE_RTTIMER_IRQ();
    }

    return (retVal1 != FLCN_OK) ? retVal1 : retVal2;
}

/*!
 * Reset the RT Timer status bit
 */
static void
_resetTimerIntr(void)
{
    RTTIMER_LIB_CLEAR_RTTIMER_IRQ();
}

/*!
 * Poll on the RT Timer IRQSTAT bit
 */
static FLCN_STATUS
_pollTimerStat
(
    LwU32   count,
    LwBool  bCheckTime,
    LwU32  *pElapsedNs
)
{
    FLCN_STATUS retVal = FLCN_OK;
    FLCN_TIMESTAMP startTime;

    LwU32 intervalUs;
    LwU32 intervalNs;
    LwU32 epsilonNs = 1000000; // 1ms diff either way
    LwU32 higherOk;
    LwU32 lowerOk;

    osPTimerTimeNsLwrrentGet(&startTime);

    // Wait till the IRQ status is set
    while (!RTTIMER_LIB_IS_RTTIMER_IRQ_SET())
    {
        // Ideally, we should yield here, but since it's for testing we're ok
    }

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
static FLCN_STATUS
_verifyTimerOneShot
(
    LwU32   count,
    LwBool  bCheckTime,
    LwU32  *pElapsedNs
)
{
    FLCN_STATUS retVal;

    _resetTimerIntr();
    retVal = vApplicationRttimerSetup(LW_TRUE, count, LW_TRUE);

    if (retVal != FLCN_OK)
    {
        return retVal;
    }

    retVal = _pollTimerStat(count, bCheckTime, pElapsedNs);

    (void)vApplicationRttimerSetup(LW_FALSE, count, LW_TRUE);

    return retVal;
}

/*!
 * Verify the RT Timer in continuous mode
 * Set up the RT Timer to count down in continuous mode, verify that
 * the IRQSTAT bit is set at the end of the interval.
 * Then clear it, and verify that the timer is still running and the
 * IRQSTAT bit is set again, at the end of a second interval.
 */
static FLCN_STATUS
_verifyTimerContinuous
(
    LwU32   count,
    LwBool  bCheckTime,
    LwU32  *pElapsedNs
)
{
    FLCN_STATUS retVal;

    _resetTimerIntr();
    retVal = vApplicationRttimerSetup(LW_TRUE, count, LW_FALSE);

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
    (void)vApplicationRttimerSetup(LW_FALSE, count, LW_FALSE);
    _resetTimerIntr();
    return retVal;
}
