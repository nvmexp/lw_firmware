/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2013-2015,2019 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

//!
//! \file rmt_DpuVerifFeatureTest.cpp
//! \brief rmtest for DPU
//!

#include "gpu/tests/rmtest.h"
#include "core/include/platform.h"
#include "core/include/tasker.h"
#include "gpu/tests/rm/utility/dtiutils.h"
#include "gpu/display/dpusub.h"
#include "dpu/v02_05/dev_disp_falcon.h"
#include "maxwell/gm200/dev_pmgr.h"

#define IsGM20XorBetter(p)   ((p >= Gpu::GM200))

class DpuVerifFeatureTest : public RmTest
{
public:
    DpuVerifFeatureTest();
    virtual ~DpuVerifFeatureTest();
    virtual string IsTestSupported();
    virtual RC Setup();
    virtual RC Run();
    virtual RC Cleanup();
    virtual RC VerifyMutexes();
    virtual RC VerifyTimer(LwU32, LwU32);
    virtual RC VerifyMailboxes();
    virtual RC VerifyAuxSemaphore();

    virtual void SaveMailboxState();
    virtual void RestoreMailboxState();

    virtual LwU32 AcquireMutexId();
    virtual RC ReleaseMutexId(LwU32);
    virtual RC AcquireMutex(LwU32, LwU32, LwBool);
    virtual RC ReleaseMutex(LwU32);

    virtual RC TimerVerifyStatus();
    virtual RC TimerCheckIntrStatus(LwU32, LwBool);
    virtual RC TimerCheckIntrStatTimedOut(LwU32);
    virtual RC TimerIntrCtrl(LwU32);
    virtual RC TimerSetupCtrl(LwU32, LwU32, LwU32, LwBool);

private:
    DPU            *m_pDpu;
    GpuSubdevice   *m_Parent;    // GpuSubdevice that owns this DPU
    LwU32          m_I2cState;

};

#define DPU_MUTEX_ID_RM_RANGE_START             (8)
#define DPU_MUTEX_ID_RM_RANGE_END               (254)
#define DPU_MUTEX_ID_MAX_COUNT              (247)
#define DPU_MUTEX_ID_TEST                       (63)
#define DPU_MUTEX_INDEX_TEST                    (0)

#define DPU_VERIF_TIMER_INTR_MODE_ENABLE        (0x1)
#define DPU_VERIF_TIMER_INTR_MODE_DISABLE       (0x2)
#define DPU_VERIF_TIMER_INTR_MODE_CLEAR         (0x3)
#define DPU_VERIF_TIMER_INTR_CLEAR_TIMEOUT_MS   (10)

#define DPU_VERIF_TIMER_MODE_STOP               (0x1)
#define DPU_VERIF_TIMER_MODE_RUN                (0x2)
#define DPU_VERIF_TIMER_MODE_RESTART            (0x3)
#define DPU_VERIF_TIMER_RESET_TIMEOUT_MS        (100)
#define DPU_VERIF_TIMER_SRC_PTIMER              (0x0)
#define DPU_VERIF_TIMER_SRC_DPU_CLK             (0x1)

#define DPU_VERIF_TIMER_DEFAULT_INTERVAL_MS     (2 * 10)         // 20    ms
#define DPU_VERIF_TIMER_DEFAULT_INTERVAL_US     (2 * 10 * 1000)  // 20000 ms
#define DPU_VERIF_TIMER_DEFAULT_INTERVAL_CLK    (0xF000)

#define DPU_TIMER_START_ONESHOT(i, src)         CHECK_RC(TimerSetupCtrl(DPU_VERIF_TIMER_MODE_RUN, i, src, true))
#define DPU_TIMER_STOP()                        CHECK_RC(TimerSetupCtrl(DPU_VERIF_TIMER_MODE_STOP, 0, 0, false))
#define DPU_TIMER_RESTART()                     CHECK_RC(TimerSetupCtrl(DPU_VERIF_TIMER_MODE_RESTART, 0, 0, false))
#define DPU_TIMER_START_CONT(i, src)            CHECK_RC(TimerSetupCtrl(DPU_VERIF_TIMER_MODE_RUN, i, src, false))
#define DPU_TIMER_CHECK_STAT_NOVERIF(i)         CHECK_RC(TimerCheckIntrStatus(i, true))
#define DPU_TIMER_CHECK_STAT(i)                 CHECK_RC(TimerCheckIntrStatus(i, false))
#define DPU_TIMER_CHECK_STAT_TIMEOUT(i)         CHECK_RC(TimerCheckIntrStatTimedOut(i))

//! \brief DpuVerifFeatureTest constructor
//!
//!
//! \sa Setup
//------------------------------------------------------------------------------
DpuVerifFeatureTest::DpuVerifFeatureTest()
{
    SetName("DpuVerifFeatureTest");
    m_pDpu      = nullptr;
    m_Parent    = nullptr;
    m_I2cState  = 0;
}

//! \brief DpuVerifFeatureTest destructor
//!
//!
//! \sa Cleanup
//------------------------------------------------------------------------------
DpuVerifFeatureTest::~DpuVerifFeatureTest()
{
}

//! \brief IsTestSupported
//!
//! Checks if the test can be run in the current environment.
//!
//! \return True if the test can be run in the current environment,
//!         false otherwise
//------------------------------------------------------------------------------
string DpuVerifFeatureTest::IsTestSupported()
{
    Gpu::LwDeviceId chipName = GetBoundGpuSubdevice()->DeviceId();
    if (IsGM20XorBetter(chipName))
    {
        return RUN_RMTEST_TRUE;
    }
    return "Supported only on GM20X+ chips";
}

//! \brief Do Init from Js, allocate m_pDpu and do initialization
//!
//! \return OK if no error happens, otherwise corresponding rc
//------------------------------------------------------------------------------
RC DpuVerifFeatureTest::Setup()
{
    RC rc = OK;

    m_Parent = GetBoundGpuSubdevice();

    return rc;
}

/*!
 * @brief Acquire mutex ID
 */
LwU32 DpuVerifFeatureTest::AcquireMutexId()
{
    LwU32 mutexId =  DRF_VAL(_PDISP, _FALCON_MUTEX_ID, _VALUE, m_Parent->RegRd32(LW_PDISP_FALCON_MUTEX_ID));
    return mutexId;
}

/*!
 * @brief Release mutex ID
 */
RC DpuVerifFeatureTest::ReleaseMutexId(LwU32 mutexId)
{
    if ((mutexId > DPU_MUTEX_ID_RM_RANGE_END) || (mutexId < DPU_MUTEX_ID_RM_RANGE_START))
    {
        Printf(Tee::PriHigh, "DpuVerifFeatureTest::%s Invalid mutexId 0x%0x\n", __FUNCTION__, mutexId);
        return RC::SOFTWARE_ERROR;
    }

    m_Parent->RegWr32(LW_PDISP_FALCON_MUTEX_ID_RELEASE, mutexId);
    return OK;
}

/*!
 * @brief Acquire mutex
 */
RC DpuVerifFeatureTest::AcquireMutex(LwU32 mindex, LwU32 mutexId, LwBool bExpectAcquire)
{
    LwBool bAcquired = false;

    if ((mutexId > DPU_MUTEX_ID_RM_RANGE_END) || (mutexId < DPU_MUTEX_ID_RM_RANGE_START) ||
        (mindex >= LW_PDISP_FALCON_MUTEX__SIZE_1))
    {
        Printf(Tee::PriHigh, "DpuVerifFeatureTest::%s Invalid mutexId 0x%0x or mindex 0x%0x\n", __FUNCTION__, mutexId, mindex);
        return RC::SOFTWARE_ERROR;
    }

    m_Parent->RegWr32(LW_PDISP_FALCON_MUTEX(mindex), mutexId);

    // Readback and verify it
    if (m_Parent->RegRd32(LW_PDISP_FALCON_MUTEX(mindex)) == mutexId)
        bAcquired = true;

    if ((bAcquired && (!bExpectAcquire)) ||
        (!bAcquired && bExpectAcquire))
    {
        Printf(Tee::PriHigh, "DpuVerifFeatureTest::%s Mutex acquire status 0x%0x but expected acquire state 0x%0x\n",
                             __FUNCTION__, bAcquired, bExpectAcquire);
        return RC::SOFTWARE_ERROR;
    }
    return OK;
}

/*!
 * @brief Release mutex
 */
RC DpuVerifFeatureTest::ReleaseMutex(LwU32 mindex)
{
    if(mindex >= LW_PDISP_FALCON_MUTEX__SIZE_1)
        return RC::SOFTWARE_ERROR;

    m_Parent->RegWr32(LW_PDISP_FALCON_MUTEX(mindex), 0);
    return OK;
}

/*!
 * @brief Verify mutexes
 *        Below is the sequence being used to test:
 *        1. Acquire first mutexID and make sure we get one.
 *        2. Acquire all mutexIDs and check if we get all the mutexIDs.
 *        3. Release the first acquired mutexID and acquire again.. Make sure newly acquired ID equals the first one.
 *        4. Release all mutexIDs
 *        5. Acquire all mutexes using a mutexID (any mutexID)
 *        6. Re-acquire all mutexes using a different mutexID (compare mutexIDs here) and make sure all acquire fails
 *        7. Release all mutexes and try acquiring them. There should be no failures.
 */
RC DpuVerifFeatureTest::VerifyMutexes()
{
    RC    rc         = OK;
    LwU32 mutexId    = 0;
    LwU32 index      = 0;
    LwU32 temp       = 0;
    LwU32 mutexCount = 0;

    Printf(Tee::PriHigh, "********** DpuVerifFeatureTest::%s - Mutex verification start ********************\n",
                    __FUNCTION__);

    //
    // Makes sure MUTEX ID generation works as expected
    // Read all possible ID and check if it reaches the max
    //

    // Read first mutex and make sure we get a valid one.
    if ((mutexId = AcquireMutexId()) == LW_PDISP_FALCON_MUTEX_ID_VALUE_NOT_AVAIL)
    {
        Printf(Tee::PriHigh, "DpuVerifFeatureTest::%s - First mutex acquire failed. Something fishy. \"FAILED\"\n",
                              __FUNCTION__);
        return RC::SOFTWARE_ERROR;
    }
    else
    {
        Printf(Tee::PriHigh, "DpuVerifFeatureTest::%s - First mutex ID read \"PASSED\"\n",
                              __FUNCTION__);
        mutexCount++;
    }

    // Acquire all the mutex ids.
    while (AcquireMutexId() !=  LW_PDISP_FALCON_MUTEX_ID_VALUE_NOT_AVAIL)
    {
        mutexCount++;
    }

    if (mutexCount != DPU_MUTEX_ID_MAX_COUNT)
    {
        Printf(Tee::PriHigh, "DpuVerifFeatureTest::%s - Not all mutex ids were acquired : %d \"FAILED\"\n",
                              __FUNCTION__, mutexCount);
        return RC::SOFTWARE_ERROR;
    }
    else
    {
        Printf(Tee::PriHigh, "DpuVerifFeatureTest::%s - All mutex ids acquired \"PASSED\"\n",
                              __FUNCTION__);
    }

    //
    // We obtained all the mutexIds above..
    // Try releasing a mutexId and acquire it back..
    //
    CHECK_RC(ReleaseMutexId(mutexId));

    if (AcquireMutexId() != mutexId)
    {
        Printf(Tee::PriHigh, "DpuVerifFeatureTest::%s - Test/Release mutex ID read \"FAILED\"\n",
                              __FUNCTION__);
        return RC::SOFTWARE_ERROR;
    }
    else
    {
        Printf(Tee::PriHigh, "DpuVerifFeatureTest::%s - Test/Release mutex ID read \"PASSED\"\n",
                              __FUNCTION__);
    }

    // Release all the mutexIds now
    mutexCount = DPU_MUTEX_ID_RM_RANGE_END;
    while (mutexCount >= DPU_MUTEX_ID_RM_RANGE_START)
    {
        CHECK_RC(ReleaseMutexId(mutexCount--));
    }
    Printf(Tee::PriHigh, "DpuVerifFeatureTest::%s - Releasing all mutex IDs \"PASSED\"\n",
                              __FUNCTION__);

    // Lets acquire all mutexes
    mutexId = AcquireMutexId();
    while (index < LW_PDISP_FALCON_MUTEX__SIZE_1)
    {
        CHECK_RC(AcquireMutex(index, mutexId, true));
        ++index;
    }
    Printf(Tee::PriHigh, "DpuVerifFeatureTest::%s - Acquiring all mutexes with mutex ID 0x%0x \"PASSED\"\n",
                    __FUNCTION__, mutexId);

    // Acquire mutexes again and it should fail
    temp    = mutexId;
    mutexId = AcquireMutexId();
    CHECK_RC(ReleaseMutexId(temp));

    // Current mutexId and previous mutexId has to be different
    if (mutexId == temp)
    {
        Printf(Tee::PriHigh, "DpuVerifFeatureTest::%s - Current 0x%0x & Previous 0x%0x mutexId comparison \"FAILED\"\n",
                              __FUNCTION__, mutexId, temp);
        return RC::SOFTWARE_ERROR;
    }
    else
    {
        Printf(Tee::PriHigh, "DpuVerifFeatureTest::%s - Current 0x%0x & Previous 0x%0x mutexId comparison \"PASSED\"\n",
                              __FUNCTION__, mutexId, temp);
    }

    index = 0;
    while (index < LW_PDISP_FALCON_MUTEX__SIZE_1)
    {
        CHECK_RC(AcquireMutex(index, mutexId, false));
        ++index;
    }

    Printf(Tee::PriHigh, "DpuVerifFeatureTest::%s - Testing mutex acquire fail with mutex ID 0x%0x \"PASSED\"\n",
                    __FUNCTION__, mutexId);

    // Release all mutexes
    index = 0;
    while (index < LW_PDISP_FALCON_MUTEX__SIZE_1)
    {
        CHECK_RC(ReleaseMutex(index));
        ++index;
    }
    Printf(Tee::PriHigh, "DpuVerifFeatureTest::%s - Releasing all mutexes \"PASSED\"\n",
                    __FUNCTION__);

    // Acquire again and it should pass now
    index = 0;
    while (index < LW_PDISP_FALCON_MUTEX__SIZE_1)
    {
        CHECK_RC(AcquireMutex(index, mutexId, true));
        ++index;
    }
    Printf(Tee::PriHigh, "DpuVerifFeatureTest::%s - Acquiring all mutexes with mutex ID 0x%0x \"PASSED\"\n",
                    __FUNCTION__, mutexId);

    // Release all the mutexes as the test ends here.
    index = 0;
    while (index < LW_PDISP_FALCON_MUTEX__SIZE_1)
    {
        CHECK_RC(ReleaseMutex(index));
        ++index;
    }
    CHECK_RC(ReleaseMutexId(mutexId));

    Printf(Tee::PriHigh, "********** DpuVerifFeatureTest::%s - Mutex verification end ********************\n",
                    __FUNCTION__);

    return rc;
}

/*!
 * @brief Poll for timer reset
 */
static bool TimerResetPoll(void *pArg)
{
    GpuSubdevice   *m_Parent = (GpuSubdevice *)pArg;
    LwU32          data = m_Parent->RegRd32(LW_PDISP_FALCON_TIMER_CTRL);
    if (FLD_TEST_DRF(_PDISP, _FALCON_TIMER_CTRL, _RESET, _DONE, data))
        return true;
    return false;
}

/*!
 * @brief Poll for timer reset
 */
static bool TimerIntrClrPoll(void *pArg)
{
    GpuSubdevice   *m_Parent = (GpuSubdevice *)pArg;
    LwU32          data = m_Parent->RegRd32(LW_PDISP_FALCON_TIMER_0_INTR);
    if (FLD_TEST_DRF(_PDISP, _FALCON_TIMER_0_INTR, _CLR, _DONE, data))
        return true;
    return false;
}

/*!
 * @brief Poll for timer reset
 */
static bool TimerIntrStatusPoll(void *pArg)
{
    GpuSubdevice   *m_Parent = (GpuSubdevice *)pArg;
    LwU32          data = m_Parent->RegRd32(LW_PDISP_FALCON_TIMER_0_INTR);
    if (FLD_TEST_DRF(_PDISP, _FALCON_TIMER_0_INTR, _STATUS, _PENDING, data))
        return true;
    return false;
}

/*!
 * @brief Verify Intr status. Compare count and interval
 */
RC DpuVerifFeatureTest::TimerVerifyStatus()
{
    LwU32 interval = DRF_VAL(_PDISP, _FALCON_TIMER_0_INTERVAL, _VALUE, m_Parent->RegRd32(LW_PDISP_FALCON_TIMER_0_INTERVAL));
    LwU32 count    = DRF_VAL(_PDISP, _FALCON_TIMER_0_COUNT, _VALUE, m_Parent->RegRd32(LW_PDISP_FALCON_TIMER_0_COUNT));

    if (count)
    {
        Printf(Tee::PriHigh, "DpuVerifFeatureTest::%s - Count 0x%0x non-zero with interval 0x%0x\n", __FUNCTION__, count, interval);
        return RC::SOFTWARE_ERROR;
    }
    return OK;
}

/*!
 * @brief Poll for timer reset
 */
RC DpuVerifFeatureTest::TimerIntrCtrl(LwU32 intrMode)
{
    RC    rc   = OK;
    LwU32 data = m_Parent->RegRd32(LW_PDISP_FALCON_TIMER_0_INTR);

    switch (intrMode)
    {
        case DPU_VERIF_TIMER_INTR_MODE_ENABLE:
            data = FLD_SET_DRF(_PDISP, _FALCON_TIMER_0_INTR_EN, _ENABLE, _YES, data);
            m_Parent->RegWr32(LW_PDISP_FALCON_TIMER_0_INTR_EN, data);
            break;
        case DPU_VERIF_TIMER_INTR_MODE_DISABLE:
            data = FLD_SET_DRF(_PDISP, _FALCON_TIMER_0_INTR_EN, _ENABLE, _NO, data);
            m_Parent->RegWr32(LW_PDISP_FALCON_TIMER_0_INTR_EN, data);
            break;
        case DPU_VERIF_TIMER_INTR_MODE_CLEAR:
            data = FLD_SET_DRF(_PDISP, _FALCON_TIMER_0_INTR, _CLR, _TRIGGER, data);
            m_Parent->RegWr32(LW_PDISP_FALCON_TIMER_0_INTR, data);

            // Wait for clear to complete
            CHECK_RC(POLLWRAP(&TimerIntrClrPoll, (void*)m_Parent, DPU_VERIF_TIMER_INTR_CLEAR_TIMEOUT_MS));
            break;
        default:
            Printf(Tee::PriHigh, "DpuVerifFeatureTest::%s - Invalid Timer interrupt mode!\n", __FUNCTION__);
            rc = RC::SOFTWARE_ERROR;
    }

    return rc;
}

/*!
 * @brief Control timer
 */
RC DpuVerifFeatureTest::TimerSetupCtrl(LwU32 mode, LwU32 interval, LwU32 tmrSource, LwBool bIsModeOneShot)
{
    RC     rc   = OK;
    LwU32  data = 0;

    switch (mode)
    {
        case DPU_VERIF_TIMER_MODE_STOP:
            data = FLD_SET_DRF(_PDISP, _FALCON_TIMER_CTRL, _GATE, _STOP,
                                       m_Parent->RegRd32(LW_PDISP_FALCON_TIMER_CTRL));
            m_Parent->RegWr32(LW_PDISP_FALCON_TIMER_CTRL, data);
            break;
        case DPU_VERIF_TIMER_MODE_RESTART:
            data = FLD_SET_DRF(_PDISP, _FALCON_TIMER_CTRL, _GATE, _RUN,
                                       m_Parent->RegRd32(LW_PDISP_FALCON_TIMER_CTRL));
            m_Parent->RegWr32(LW_PDISP_FALCON_TIMER_CTRL, data);
            break;
        case DPU_VERIF_TIMER_MODE_RUN:
            // Lets reset the timer first
            data = FLD_SET_DRF(_PDISP, _FALCON_TIMER_CTRL, _RESET, _TRIGGER, 0);
            m_Parent->RegWr32(LW_PDISP_FALCON_TIMER_CTRL, data);

            // Poll for reset to complete
            CHECK_RC(POLLWRAP(&TimerResetPoll, m_Parent, DPU_VERIF_TIMER_RESET_TIMEOUT_MS));

            // Write the interval first
            m_Parent->RegWr32(LW_PDISP_FALCON_TIMER_0_INTERVAL, interval);

            // Program the ctrl to start the timer
            data = FLD_SET_DRF(_PDISP, _FALCON_TIMER_CTRL, _GATE, _RUN, 0);
            if (tmrSource == DPU_VERIF_TIMER_SRC_PTIMER)
                data = FLD_SET_DRF(_PDISP, _FALCON_TIMER_CTRL, _SOURCE, _PTIMER_1US, data);
            else
                data = FLD_SET_DRF(_PDISP, _FALCON_TIMER_CTRL, _SOURCE, _DPU_CLK, data);

            if (bIsModeOneShot)
                data = FLD_SET_DRF(_PDISP, _FALCON_TIMER_CTRL, _MODE, _ONE_SHOT, data);
            else
                data = FLD_SET_DRF(_PDISP, _FALCON_TIMER_CTRL, _MODE, _CONTINUOUS, data);

            // Write Ctrl now
            m_Parent->RegWr32(LW_PDISP_FALCON_TIMER_CTRL, data);

            break;
        default:
            Printf(Tee::PriHigh, "DpuVerifFeatureTest::%s - Invalid Timer mode!\n", __FUNCTION__);
            rc = RC::SOFTWARE_ERROR;
    }

    return rc;
}

/*!
 * @brief Check status of timer interrupt and clear accordingly
 */
RC DpuVerifFeatureTest::TimerCheckIntrStatus(LwU32 intervalUs, LwBool bSkipVerifyStatus)
{
    RC     rc        = OK;
    LwU32  timeoutMs = (intervalUs / 1000);

    // The additional 1ms is added to make sure DPU timer finishes first.
    Tasker::Sleep(timeoutMs + 1);
    if(!(TimerIntrStatusPoll((void*)m_Parent)))
    {
        Printf(Tee::PriHigh, "*** Timer Ctrl register status :\n\
                              *** Current Count : %d \n\
                              *** Interval      : %d \n" ,
                              m_Parent->RegRd32(LW_PDISP_FALCON_TIMER_0_COUNT), m_Parent->RegRd32(LW_PDISP_FALCON_TIMER_0_INTERVAL));
        rc = RC::TIMEOUT_ERROR;
        return rc;
    }

    if (!bSkipVerifyStatus)
    {
        CHECK_RC(TimerVerifyStatus());
    }

    // Clear the interrupt
    CHECK_RC(TimerIntrCtrl(DPU_VERIF_TIMER_INTR_MODE_CLEAR));

    return rc;
}

/*!
 * @brief Check if we timeout before status bit goes high..
 */
RC DpuVerifFeatureTest::TimerCheckIntrStatTimedOut(LwU32 intervalUs)
{
    RC rc = OK;
    rc = TimerCheckIntrStatus((intervalUs/2), false);
    if (rc == RC::TIMEOUT_ERROR)
        return OK;
    else
        Printf(Tee::PriHigh, "DpuVerifFeatureTest::%s - Intr Stat didn't timeout as expected\n",
                   __FUNCTION__);
    return rc;
}

/*!
 * @brief Verify timer
 */
RC DpuVerifFeatureTest::VerifyTimer(LwU32 tmrSource, LwU32 intervalUs)
{
    RC    rc = OK;
    LwU32 irqMask;

    // This is done to avoid DPU receiving the interrupt and clear the _PENDING bit.
    irqMask  = m_Parent->RegRd32(LW_PDISP_FALCON_IRQMCLR);
    irqMask |= FLD_SET_DRF(_PDISP, _FALCON_IRQMCLR_EXT, _EXTIRQ3, _SET, irqMask);
    m_Parent->RegWr32(LW_PDISP_FALCON_IRQMCLR, irqMask);

    Printf(Tee::PriHigh, "********** DpuVerifFeatureTest::%s - Timer verification start ********************\n",
                    __FUNCTION__);

    // Enable the timer interrupt first
    CHECK_RC(TimerIntrCtrl(DPU_VERIF_TIMER_INTR_MODE_ENABLE));

    // One-shot mode verif
    DPU_TIMER_START_ONESHOT(intervalUs, tmrSource);
    DPU_TIMER_CHECK_STAT(intervalUs);
    Printf(Tee::PriHigh,"DpuVerifFeatureTest::%s - One-shot mode verification \"PASSED\"\n", __FUNCTION__);

    // Continuous mode verif
    DPU_TIMER_START_CONT(intervalUs, tmrSource);
    DPU_TIMER_CHECK_STAT_NOVERIF(intervalUs);
    DPU_TIMER_CHECK_STAT_NOVERIF(intervalUs);
    Printf(Tee::PriHigh,"DpuVerifFeatureTest::%s - Continuous mode verification \"PASSED\"\n", __FUNCTION__);

    // Continuous mode with timer stop and restart
    DPU_TIMER_START_CONT(intervalUs, tmrSource);
    DPU_TIMER_STOP();
    DPU_TIMER_RESTART();
    DPU_TIMER_CHECK_STAT_NOVERIF(intervalUs);
    DPU_TIMER_CHECK_STAT_NOVERIF(intervalUs);

    DPU_TIMER_STOP();
    DPU_TIMER_RESTART();
    DPU_TIMER_CHECK_STAT_NOVERIF(intervalUs);
    DPU_TIMER_STOP();
    Printf(Tee::PriHigh,"DpuVerifFeatureTest::%s - Timer restart verification with continuous mode \"PASSED\"\n", __FUNCTION__);

    // One shot mode with timer stop and restart
    DPU_TIMER_START_ONESHOT(intervalUs, tmrSource);
    DPU_TIMER_CHECK_STAT(intervalUs);
    DPU_TIMER_STOP();
    DPU_TIMER_RESTART();
    DPU_TIMER_CHECK_STAT(intervalUs);
    DPU_TIMER_STOP();
    DPU_TIMER_RESTART();
    DPU_TIMER_CHECK_STAT(intervalUs);
    DPU_TIMER_STOP();
    Printf(Tee::PriHigh,"DpuVerifFeatureTest::%s - Timer restart verification with One-shot mode \"PASSED\"\n", __FUNCTION__);

    // Check status interrupt didnt go ON prematurely
    DPU_TIMER_START_ONESHOT(intervalUs, tmrSource);
    DPU_TIMER_CHECK_STAT_TIMEOUT(intervalUs);
    DPU_TIMER_STOP();
    Printf(Tee::PriHigh,"DpuVerifFeatureTest::%s - Timer timeout negative testing \"PASSED\"\n", __FUNCTION__);

    Printf(Tee::PriHigh, "********** DpuVerifFeatureTest::%s - Timer verification end ********************\n", __FUNCTION__);

    // Set back the IRQ masks.
    irqMask  = m_Parent->RegRd32(LW_PDISP_FALCON_IRQMSET);
    irqMask |= FLD_SET_DRF(_PDISP, _FALCON_IRQMSET_EXT, _EXTIRQ3, _SET, irqMask);
    m_Parent->RegWr32(LW_PDISP_FALCON_IRQMSET, irqMask);

    return rc;
}

void DpuVerifFeatureTest::SaveMailboxState()
{
    // Mailbox Register(15) has the I2C mutex state
    m_I2cState = m_Parent->RegRd32(LW_PDISP_FALCON_MAILBOX_REG(15));
}

void DpuVerifFeatureTest::RestoreMailboxState()
{
    // Restore the I2C mutex state
    m_Parent->RegWr32(LW_PDISP_FALCON_MAILBOX_REG(15), m_I2cState);
}

/*!
 * @brief Verify mailboxes
 *        Write/read/compare
 */
RC DpuVerifFeatureTest::VerifyMailboxes()
{
    LwU32 index = 0;
    LwU32 data  = 0;

    SaveMailboxState();

    Printf(Tee::PriHigh, "********** DpuVerifFeatureTest::%s - Mailboxes verification start ********************\n", __FUNCTION__);
    for (index=0; index < LW_PDISP_FALCON_MAILBOX_REG__SIZE_1; index++)
    {
        m_Parent->RegWr32( LW_PDISP_FALCON_MAILBOX_REG(index), (0xdead0000 + index));
        data = m_Parent->RegRd32(LW_PDISP_FALCON_MAILBOX_REG(index));
        if (data != (0xdead0000 + index))
        {
            Printf(Tee::PriHigh, "DpuVerifFeatureTest::%s - Mailbox returns 0x%0x but expected 0x%0x\n",
                __FUNCTION__, data, (0xdead0000 + index));
            Printf(Tee::PriHigh, "DpuVerifFeatureTest::%s - Verify mailboxes \"FAILED\"\n", __FUNCTION__);
            return RC::SOFTWARE_ERROR;
        }
    }
    Printf(Tee::PriHigh, "DpuVerifFeatureTest::%s - Verify mailboxes \"PASSED\"\n", __FUNCTION__);
    Printf(Tee::PriHigh, "********** DpuVerifFeatureTest::%s - Mailboxes verification end ********************\n", __FUNCTION__);

    RestoreMailboxState();
    return OK;
}

/*!
 * @brief Verify aux mutex
 *
 */
#define DPU_VERIF_AUXCTL_INDEX 0
RC DpuVerifFeatureTest::VerifyAuxSemaphore()
{
    RC    rc = OK;
    LwU32 data = 0;

    Printf(Tee::PriHigh, "********** DpuVerifFeatureTest::%s - AUXCTL Sema verification start ********************\n", __FUNCTION__);

    // Check if SEMA_REQUEST is in INIT state
    data = m_Parent->RegRd32(LW_PMGR_DP_AUXCTL(DPU_VERIF_AUXCTL_INDEX));
    if (!FLD_TEST_DRF(_PMGR, _DP_AUXCTL, _SEMA_REQUEST, _INIT, data))
    {
        Printf(Tee::PriHigh, "DpuVerifFeatureTest::%s - Verify AUXCTL Init state \"FAILED\"\n", __FUNCTION__);
        return RC::SOFTWARE_ERROR;
    }
    Printf(Tee::PriHigh, "DpuVerifFeatureTest::%s - Verify AUXCTL Init state \"PASSED\"\n", __FUNCTION__);

    // Make DPU as owner
    data = FLD_SET_DRF(_PMGR, _DP_AUXCTL, _SEMA_REQUEST, _DPU, data);
    m_Parent->RegWr32(LW_PMGR_DP_AUXCTL(DPU_VERIF_AUXCTL_INDEX), data);

    data = m_Parent->RegRd32(LW_PMGR_DP_AUXCTL(DPU_VERIF_AUXCTL_INDEX));
    if (!FLD_TEST_DRF(_PMGR, _DP_AUXCTL, _SEMA_GRANT, _DPU, data))
    {
        Printf(Tee::PriHigh, "DpuVerifFeatureTest::%s - Make DPU as owner \"FAILED\"\n", __FUNCTION__);
        return RC::SOFTWARE_ERROR;
    }
    Printf(Tee::PriHigh, "DpuVerifFeatureTest::%s - Make DPU as owner \"PASSED\"\n", __FUNCTION__);

    // Try making RM as owner with releasing DPU..
    data = FLD_SET_DRF(_PMGR, _DP_AUXCTL, _SEMA_REQUEST, _RM, data);
    m_Parent->RegWr32(LW_PMGR_DP_AUXCTL(DPU_VERIF_AUXCTL_INDEX), data);

    data = m_Parent->RegRd32(LW_PMGR_DP_AUXCTL(DPU_VERIF_AUXCTL_INDEX));
    if (!FLD_TEST_DRF(_PMGR, _DP_AUXCTL, _SEMA_GRANT, _DPU, data))
    {
        Printf(Tee::PriHigh, "DpuVerifFeatureTest::%s - DPU ownership replaced by RM \"FAILED\"\n", __FUNCTION__);
        return RC::SOFTWARE_ERROR;
    }

    // Try making VBIOS as owner with releasing DPU..
    data = FLD_SET_DRF(_PMGR, _DP_AUXCTL, _SEMA_REQUEST, _VBIOS, data);
    m_Parent->RegWr32(LW_PMGR_DP_AUXCTL(DPU_VERIF_AUXCTL_INDEX), data);

    data = m_Parent->RegRd32(LW_PMGR_DP_AUXCTL(DPU_VERIF_AUXCTL_INDEX));
    if (!FLD_TEST_DRF(_PMGR, _DP_AUXCTL, _SEMA_GRANT, _DPU, data))
    {
        Printf(Tee::PriHigh, "DpuVerifFeatureTest::%s - DPU ownership replaced by VBIOS \"FAILED\"\n", __FUNCTION__);
        return RC::SOFTWARE_ERROR;
    }

    // Try making PMU as owner with releasing DPU..
    data = FLD_SET_DRF(_PMGR, _DP_AUXCTL, _SEMA_REQUEST, _PMU, data);
    m_Parent->RegWr32(LW_PMGR_DP_AUXCTL(DPU_VERIF_AUXCTL_INDEX), data);

    data = m_Parent->RegRd32(LW_PMGR_DP_AUXCTL(DPU_VERIF_AUXCTL_INDEX));
    if (!FLD_TEST_DRF(_PMGR, _DP_AUXCTL, _SEMA_GRANT, _DPU, data))
    {
        Printf(Tee::PriHigh, "DpuVerifFeatureTest::%s - DPU ownership replaced by PMU \"FAILED\"\n", __FUNCTION__);
        return RC::SOFTWARE_ERROR;
    }
    Printf(Tee::PriHigh, "DpuVerifFeatureTest::%s - DPU ownership stickiness \"PASSED\"\n", __FUNCTION__);

    // Release DPU as owner
    data = FLD_SET_DRF(_PMGR, _DP_AUXCTL, _SEMA_REQUEST, _RELEASE, data);
    m_Parent->RegWr32(LW_PMGR_DP_AUXCTL(DPU_VERIF_AUXCTL_INDEX), data);

    data = m_Parent->RegRd32(LW_PMGR_DP_AUXCTL(DPU_VERIF_AUXCTL_INDEX));
    if (!FLD_TEST_DRF(_PMGR, _DP_AUXCTL, _SEMA_GRANT, _INIT, data))
    {
        Printf(Tee::PriHigh, "DpuVerifFeatureTest::%s - Releasing DPU ownership \"FAILED\"\n", __FUNCTION__);
        return RC::SOFTWARE_ERROR;
    }
    Printf(Tee::PriHigh, "DpuVerifFeatureTest::%s - Releasing DPU ownership \"PASSED\"\n", __FUNCTION__);

    // Make PMU as owner
    data = FLD_SET_DRF(_PMGR, _DP_AUXCTL, _SEMA_REQUEST, _PMU, data);
    m_Parent->RegWr32(LW_PMGR_DP_AUXCTL(DPU_VERIF_AUXCTL_INDEX), data);

    data = m_Parent->RegRd32(LW_PMGR_DP_AUXCTL(DPU_VERIF_AUXCTL_INDEX));
    if (!FLD_TEST_DRF(_PMGR, _DP_AUXCTL, _SEMA_GRANT, _PMU, data))
    {
        Printf(Tee::PriHigh, "DpuVerifFeatureTest::%s - Acquiring PMU ownership \"FAILED\"\n", __FUNCTION__);
        return RC::SOFTWARE_ERROR;
    }
    Printf(Tee::PriHigh, "DpuVerifFeatureTest::%s - Acquiring PMU ownership \"PASSED\"\n", __FUNCTION__);

    // Release PMU as owner
    data = FLD_SET_DRF(_PMGR, _DP_AUXCTL, _SEMA_REQUEST, _RELEASE, data);
    m_Parent->RegWr32(LW_PMGR_DP_AUXCTL(DPU_VERIF_AUXCTL_INDEX), data);

    data = m_Parent->RegRd32(LW_PMGR_DP_AUXCTL(DPU_VERIF_AUXCTL_INDEX));
    if (!FLD_TEST_DRF(_PMGR, _DP_AUXCTL, _SEMA_GRANT, _INIT, data))
    {
        Printf(Tee::PriHigh, "DpuVerifFeatureTest::%s - Releasing PMU ownership \"FAILED\"\n", __FUNCTION__);
        return RC::SOFTWARE_ERROR;
    }
    Printf(Tee::PriHigh, "DpuVerifFeatureTest::%s - Releasing PMU ownership \"PASSED\"\n", __FUNCTION__);

    // Make DPU as owner again
    data = FLD_SET_DRF(_PMGR, _DP_AUXCTL, _SEMA_REQUEST, _DPU, data);
    m_Parent->RegWr32(LW_PMGR_DP_AUXCTL(DPU_VERIF_AUXCTL_INDEX), data);

    data = m_Parent->RegRd32(LW_PMGR_DP_AUXCTL(DPU_VERIF_AUXCTL_INDEX));
    if (!FLD_TEST_DRF(_PMGR, _DP_AUXCTL, _SEMA_GRANT, _DPU, data))
    {
        Printf(Tee::PriHigh, "DpuVerifFeatureTest::%s - Re-acquiring DPU ownership \"FAILED\"\n", __FUNCTION__);
        return RC::SOFTWARE_ERROR;
    }
    Printf(Tee::PriHigh, "DpuVerifFeatureTest::%s - Reacquiring DPU ownership \"PASSED\"\n", __FUNCTION__);

    // Release back the ownership to the INIT state.
    data = FLD_SET_DRF(_PMGR, _DP_AUXCTL, _SEMA_REQUEST, _RELEASE, data);
    m_Parent->RegWr32(LW_PMGR_DP_AUXCTL(DPU_VERIF_AUXCTL_INDEX), data);

    Printf(Tee::PriHigh, "********** DpuVerifFeatureTest::%s - AUXCTL Sema verification end ********************\n", __FUNCTION__);
    return rc;
}

//! \brief Run the test
//!
//!
//! \return OK all tests pass, else corresponding rc
//------------------------------------------------------------------------------
RC DpuVerifFeatureTest::Run()
{
    RC rc = OK;

    CHECK_RC(VerifyMutexes());
    // Let's run timer only on Silicon till Bug 1662757 is resolved.
    if (!((GetBoundGpuSubdevice()->IsEmulation()) ||
        (Platform::GetSimulationMode() != Platform::Hardware)))
    {
        CHECK_RC(VerifyTimer(DPU_VERIF_TIMER_SRC_PTIMER, DPU_VERIF_TIMER_DEFAULT_INTERVAL_US));
    }
    //TODO: VerifyTimer with internal clock
    CHECK_RC(VerifyMailboxes());

    //
    // FMODEL supports read/write to DP_AUXCTL. But it doesn�t support SEMA
    // request and grant and TRANSACTREQ is supported for Trigger and Done.
    // Please refer to bug 1419109 for more details.
    //
    if (Platform::GetSimulationMode() == Platform::Hardware)
    {
        CHECK_RC(VerifyAuxSemaphore());
    }

    return rc;
}

//! \brief Cleanup, does nothing for this test
//------------------------------------------------------------------------------
RC DpuVerifFeatureTest::Cleanup()
{
    return OK;
}

//------------------------------------------------------------------------------
// JS Linkage
//------------------------------------------------------------------------------

//! \brief Linkage to JavaScript
//!
//! Setup the JS hierarchy to match the C++ one.
//------------------------------------------------------------------------------
JS_CLASS_INHERIT(DpuVerifFeatureTest, RmTest, "DPU Verif Feature Test. \n");

