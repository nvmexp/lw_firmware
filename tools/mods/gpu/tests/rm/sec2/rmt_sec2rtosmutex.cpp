/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2015-2019 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

//!
//! \file rmt_Sec2RtosMutex.cpp
//! \brief rmtest for SEC2 mutexes
//!

#include "gpu/tests/rmtest.h"
#include "gpu/tests/rm/utility/dtiutils.h"
#include "gpu/utility/sec2rtossub.h"
#include "mods_reg_hal.h"

#include "gpu/tests/rm/memsys/rmt_acrverify.h"
#include "rmlsfm.h"

#include "class/cl9070.h"
#include "class/cl9270.h"
#include "class/cl9470.h"
#include "class/cl9570.h"
#include "class/clb6b9.h"
#include "ctrl/ctrlb6b9.h"

#define IsGP102orBetter(p)   ((p >= Gpu::GP102))

class Sec2RtosMutexTest : public RmTest
{
public:
    Sec2RtosMutexTest();
    virtual ~Sec2RtosMutexTest();
    virtual string IsTestSupported();
    virtual RC Setup();
    virtual RC Run();
    virtual RC Cleanup();

private:
    Sec2Rtos     *m_pSec2Rtos;
    GpuSubdevice *m_Parent;

    RC            Sec2RtosMutexTestVerifyMutexes();

    LwU32         AcquireMutexId();
    RC            ReleaseMutexId(LwU32);
    RC            TryMutex(LwU32, LwU32, LwBool);
    RC            ReleaseMutex(LwU32);
};

// Test Mutex IDs, Indexes, and Ranges
#define SEC_MUTEX_ID_RM_RANGE_START (8)
#define SEC_MUTEX_ID_RM_RANGE_END   (254)
#define SEC_MUTEX_ID_TEST_A         (63)
#define SEC_MUTEX_ID_TEST_B         (14)
#define SEC_MUTEX_ID_TEST_C         (222)
#define SEC_MUTEX_ID_TEST_D         (132)
#define SEC_MUTEX_TEST_INDEX        (4)

//! \brief Sec2RtosMutexTest constructor
//!
//! Doesn't do much except SetName.
//!
//! \sa Setup
//------------------------------------------------------------------------------
Sec2RtosMutexTest::Sec2RtosMutexTest()
{
    SetName("Sec2RtosMutexTest");
}

//! \brief Sec2RtosMutexTest destructor
//!
//! Doesn't do much.
//!
//! \sa Cleanup
//------------------------------------------------------------------------------
Sec2RtosMutexTest::~Sec2RtosMutexTest()
{
}

//! \brief IsTestSupported
//!
//! Checks if the test can be run in the current environment.
//!
//! \return True if the test can be run in the current environment,
//!         false otherwise
//------------------------------------------------------------------------------
string Sec2RtosMutexTest::IsTestSupported()
{
    Gpu::LwDeviceId chipName = GetBoundGpuSubdevice()->DeviceId();
    if (IsClassSupported(MAXWELL_SEC2) && IsGP102orBetter(chipName))
    {
        return RUN_RMTEST_TRUE;
    }
    return "[SEC2 RTOS RMTest] : Supported only on GP10X (>= GP102) chips with SEC2 RTOS";
}

//! \brief Do Init from Js
//!
//! \return OK if no error happens, otherwise corresponding rc
//------------------------------------------------------------------------------
RC Sec2RtosMutexTest::Setup()
{
    RC rc;

    // Get SEC2 RTOS class instance
    m_Parent = GetBoundGpuSubdevice();
    rc = (m_Parent->GetSec2Rtos(&m_pSec2Rtos));
    if (OK != rc)
    {
        Printf(Tee::PriHigh, "SEC2 RTOS not supported\n");
        CHECK_RC(rc);
    }

    CHECK_RC(InitFromJs());
    return OK;
}

//! \brief Run the test
//!
//! Make sure SEC2 is bootstrapped.  Then run test items.
//!
//! \return OK all tests pass, else corresponding rc
//------------------------------------------------------------------------------
RC Sec2RtosMutexTest::Run()
{
    RC rc;
    AcrVerify lsSec2State;

    CHECK_RC(lsSec2State.BindTestDevice(GetBoundTestDevice()));

    rc = m_pSec2Rtos->WaitSEC2Ready();
    if (rc != OK)
    {
        Printf(Tee::PriHigh,
               "%d:Sec2RtosMutexTest::%s: Ucode OS is not ready. Skipping tests. \n",
                __LINE__,__FUNCTION__);

        // fail the test, we need to figure out why SEC2 bootstrap failed.
        CHECK_RC(RC::LWRM_STATUS_ERROR_NOT_READY);
        return rc;
    }

    if (lsSec2State.IsTestSupported() == RUN_RMTEST_TRUE)
    {
        lsSec2State.Setup();
        // Verify whether SEC2 is booted in the expected mode or not
        rc = lsSec2State.VerifyFalconStatus(LSF_FALCON_ID_SEC2, LW_TRUE);
        if (rc != OK)
        {
            Printf(Tee::PriHigh, "%d: Sec2RtosMutexTest %s: SEC2 failed to boot in expected Mode\n",
                   __LINE__, __FUNCTION__);
            CHECK_RC(RC::SOFTWARE_ERROR);
        }
    }
    else
    {
        Printf(Tee::PriHigh, "%d: Sec2RtosMutexTest %s: LS Falcon test not supported\n",
               __LINE__, __FUNCTION__);
        CHECK_RC(RC::SOFTWARE_ERROR);
    }

    // Run Cmd & Msg Queue Test
    Printf(Tee::PriHigh,
           "%d:Sec2RtosMutexTest::%s: Starts Mutex Test...\n",
            __LINE__,__FUNCTION__);

    CHECK_RC(Sec2RtosMutexTestVerifyMutexes());

    return rc;
}

//! \brief Acquire mutex ID
//!
//! \return acquired mutex ID
//! \sa Sec2RtosMutexTestVerifyMutexes
LwU32 Sec2RtosMutexTest::AcquireMutexId()
{
    const RegHal &regs  = GetBoundGpuSubdevice()->Regs();
    return regs.Read32(MODS_PSEC_MUTEX_ID_VALUE);
}

//! \brief Release mutex ID
//!
//! \param[in]  mutexId The index of the mutex ID to release
//!
//! \return     RC::SOFTWARE ERROR if mutex ID is out of range, else OK
//! \sa Sec2RtosMutexTestVerifyMutexes
RC Sec2RtosMutexTest::ReleaseMutexId(LwU32 mutexId)
{
    RegHal &regs  = GetBoundGpuSubdevice()->Regs();
    if ((mutexId > SEC_MUTEX_ID_RM_RANGE_END) || (mutexId < SEC_MUTEX_ID_RM_RANGE_START))
    {
        Printf(Tee::PriHigh, "Sec2RtosMutexTest::%s Invalid arg: mutexId=0x%0x\n", __FUNCTION__, mutexId);
        return RC::SOFTWARE_ERROR;
    }
    regs.Write32(MODS_PSEC_MUTEX_ID_RELEASE, mutexId);
    return OK;
}

//! \brief Test acquiring the mutex
//!
//! \param[in]  mutexIndex      The index of the mutex to acquire
//! \param[in]  mutexId         The id with which we acquire the mutex
//! \param[in]  bExpectAcquire  Whether we expect to acquire the mutex
//!
//! \return     OK bExpectAcquire matches whether we acquired the mutex
//!             RC::SOFTWARE_ERROR if it doesn't match expectation or args are invalid
//! \sa Sec2RtosMutexTestVerifyMutexes
RC Sec2RtosMutexTest::TryMutex(LwU32 mutexIndex, LwU32 mutexId, LwBool bExpectAcquire)
{
    LwU32  readId;
    RegHal &regs  = GetBoundGpuSubdevice()->Regs();

    if ((mutexId > SEC_MUTEX_ID_RM_RANGE_END) || (mutexId < SEC_MUTEX_ID_RM_RANGE_START) ||
        (mutexIndex >= regs.LookupValue(MODS_PSEC_MUTEX__SIZE_1)))
    {
        Printf(Tee::PriHigh, "Sec2RtosMutexTest::%s Invalid arg: mutexId=0x%0x or mutexIndex=0x%0x\n", __FUNCTION__, mutexId, mutexIndex);
        return RC::SOFTWARE_ERROR;
    }

    regs.Write32(MODS_PSEC_MUTEX, mutexIndex, mutexId);

    // Readback and verify it

    // If Acquired
    if ((readId = regs.Read32(MODS_PSEC_MUTEX, mutexIndex)) == mutexId)
    {
        if (!bExpectAcquire)
        {
            Printf(Tee::PriHigh, "Sec2RtosMutexTest::%s  Should not have acquired with mutexId %d for mutex index %d\n",
                   __FUNCTION__, mutexId, mutexIndex);
            return RC::SOFTWARE_ERROR;
        }
    }
    // If Not Acquired
    else
    {
        if (bExpectAcquire)
        {
            Printf(Tee::PriHigh, "Sec2RtosMutexTest::%s  Read %d, but expected %d for mutex index %d\n",
                   __FUNCTION__, readId, mutexId, mutexIndex);
            return RC::SOFTWARE_ERROR;
        }
    }
    return OK;
}

//! \brief Release mutex
//!
//! \param[in]  mutexIndex  The index of the mutex to release
//!
//! \return     RC::SOFTWARE ERROR if mutex was not released or index is out of range, else OK
//! \sa Sec2RtosMutexTestVerifyMutexes
RC Sec2RtosMutexTest::ReleaseMutex(LwU32 mutexIndex)
{
    LwU32  readId;
    RegHal &regs  = GetBoundGpuSubdevice()->Regs();
    if (mutexIndex >= regs.LookupValue(MODS_PSEC_MUTEX__SIZE_1))
    {
        Printf(Tee::PriHigh, "Sec2RtosMutexTest::%s Invalid arg: mutexIndex=0x%0x\n", __FUNCTION__, mutexIndex);
        return RC::SOFTWARE_ERROR;
    }
    regs.Write32(MODS_PSEC_MUTEX, mutexIndex, regs.LookupValue(MODS_PSEC_MUTEX_VALUE_INITIAL_LOCK));
    // Readback and verify it was released
    if ((readId = regs.Read32(MODS_PSEC_MUTEX, mutexIndex)) != regs.LookupValue(MODS_PSEC_MUTEX_VALUE_INITIAL_LOCK))
    {
        Printf(Tee::PriHigh, "Sec2RtosMutexTest::%s Mutex %d not released, read %d\n", __FUNCTION__, mutexIndex, readId);
        return RC::SOFTWARE_ERROR;
    }
    return OK;
}

//! \brief Sec2RtosMutexTestVerifyMutexes()
//!
//! Test cases derived from DpuVerifFeatureTest
//!
//! Verification Requirement from section 4.2.2.3 of GP10x_SEC_IAS.docx:
//! https://p4viewer.lwpu.com/get///hw/gpu_ip/doc/sec/arch/GP10x_SEC_IAS.docx
//!
//! (1 and 9 have been tested but not checked in, 3 and 4 are not tested directly)
//!
//! 1.  Checks that MUTEX ID obtain/release, MUTEX lock obtain/release can be done by both PRIV and ucode;
//! 2.  Checks that all the 8 to 254 ID values can be obtained, and 255 will return when valid ID is exhausted;
//! 3.  Checks that write 0, 255, or 1~7 values to MUTEX_ID_RELEASE won't have side-effect;
//! 4.  Checks that write 8~254 ID value to MUTEX_ID_RELEASE when not obtained won't have side effect;
//! 5.  Checks that obtained IDs can be returned out-of-order (e.g. obtain 8->9->...->254, return 254->253->...->8);
//! 6.  Checks that all the 0~15 MUTEX locks can be obtained and released
//! 7.  Checks that MUTEX lock can NOT be released by value other than 0
//! 8.  Checks that the obtaining and releasing of different MUTEX locks can be out-of-order
//! 9.  Checks that MUTEX_ID and locks registers won't be affected by priv lockdown
//!
//! \return OK if all the tests passed, specific RC if failed
//! \sa Run()
RC Sec2RtosMutexTest::Sec2RtosMutexTestVerifyMutexes()
{
    RC     rc      = OK;
    LwU32  mutexId = 0;
    LwU32  index   = 0;
    LwU32  temp    = 0;
    LwBool mutexIdAcquired[SEC_MUTEX_ID_RM_RANGE_END + 1] = {0};
    RegHal &regs  = GetBoundGpuSubdevice()->Regs();

    Printf(Tee::PriHigh, "********** Sec2RtosMutexTest::%s - Mutex verification start ********************\n",
           __FUNCTION__);

    //
    // 2. Checks that all the 8 to 254 ID values can be obtained
    //
    //    This must use an array since the values are not guaranteed
    //    to be in order if they were used previously. However, the test does assume
    //    that all previously used IDs have been released.
    //
    for (LwU32 i = SEC_MUTEX_ID_RM_RANGE_START; i <= SEC_MUTEX_ID_RM_RANGE_END; i++)
    {
        mutexId = AcquireMutexId();
        if (mutexId == regs.LookupValue(MODS_PSEC_MUTEX_ID_VALUE_NOT_AVAIL))
        {
            Printf(Tee::PriHigh, "Sec2RtosMutexTest::%s - mutexId not available! Acquired %d/%d successfully.\n"
                                 "mutexIds must not be acquired going into test \"FAILED\"\n",
                   __FUNCTION__,
                   i - SEC_MUTEX_ID_RM_RANGE_START,
                   SEC_MUTEX_ID_RM_RANGE_END + 1 - SEC_MUTEX_ID_RM_RANGE_START);
            return RC::SOFTWARE_ERROR;
        }
        if (mutexId < SEC_MUTEX_ID_RM_RANGE_START || mutexId > SEC_MUTEX_ID_RM_RANGE_END)
        {
            Printf(Tee::PriHigh, "Sec2RtosMutexTest::%s - Acquired mutex ID %d is out of bounds [%d, %d] \"FAILED\"\n",
                   __FUNCTION__, mutexId, SEC_MUTEX_ID_RM_RANGE_START, SEC_MUTEX_ID_RM_RANGE_END);
            return RC::SOFTWARE_ERROR;
        }
        mutexIdAcquired[mutexId] = LW_TRUE;
    }
    for (LwU32 i = SEC_MUTEX_ID_RM_RANGE_START; i <= SEC_MUTEX_ID_RM_RANGE_END; i++)
    {
        if (mutexIdAcquired[i] == LW_FALSE)
        {
            Printf(Tee::PriHigh, "Sec2RtosMutexTest::%s - Mutex ID %d was never acquired \"FAILED\"\n",
                   __FUNCTION__, i);
            return RC::SOFTWARE_ERROR;
        }
    }
    Printf(Tee::PriHigh, "Sec2RtosMutexTest::%s - Acquire all IDs \"PASSED\"\n",
           __FUNCTION__);

    // Check that 255 is returned when IDs are exhausted
    if ((mutexId = AcquireMutexId()) != regs.LookupValue(MODS_PSEC_MUTEX_ID_VALUE_NOT_AVAIL))
    {
        Printf(Tee::PriHigh, "Sec2RtosMutexTest::%s - %d returned on exhaustion \"FAILED\"\n",
               __FUNCTION__, regs.LookupValue(MODS_PSEC_MUTEX_ID_VALUE_NOT_AVAIL));
        return RC::SOFTWARE_ERROR;
    }
    else
    {
        Printf(Tee::PriHigh, "Sec2RtosMutexTest::%s - %d returned on exhaustion \"PASSED\"\n",
               __FUNCTION__, regs.LookupValue(MODS_PSEC_MUTEX_ID_VALUE_NOT_AVAIL));
    }

    //
    // 5. Checks that obtained IDs can be returned out-of-order (e.g. obtain 8->9->...->254, return 254->253->...->8);
    //
    // We obtained all the mutexIds above..
    // Try releasing 4 different mutexId and acquiring them back...
    // Due to the internal representation, they should be reacquired in the order of release
    //
    CHECK_RC(ReleaseMutexId(SEC_MUTEX_ID_TEST_A));
    CHECK_RC(ReleaseMutexId(SEC_MUTEX_ID_TEST_B));
    CHECK_RC(ReleaseMutexId(SEC_MUTEX_ID_TEST_C));
    CHECK_RC(ReleaseMutexId(SEC_MUTEX_ID_TEST_D));

    if ((mutexId = AcquireMutexId()) != SEC_MUTEX_ID_TEST_A)
    {
        Printf(Tee::PriHigh, "Sec2RtosMutexTest::%s - Release/Acquire mutex ID 'A' read 0x%0x expected 0x%0x \"FAILED\"\n",
               __FUNCTION__, mutexId, SEC_MUTEX_ID_TEST_A);
        return RC::SOFTWARE_ERROR;
    }
    if ((mutexId = AcquireMutexId()) != SEC_MUTEX_ID_TEST_B)
    {
        Printf(Tee::PriHigh, "Sec2RtosMutexTest::%s - Release/Acquire mutex ID 'B' read 0x%0x expected 0x%0x \"FAILED\"\n",
               __FUNCTION__, mutexId, SEC_MUTEX_ID_TEST_B);
        return RC::SOFTWARE_ERROR;
    }
    if ((mutexId = AcquireMutexId()) != SEC_MUTEX_ID_TEST_C)
    {
        Printf(Tee::PriHigh, "Sec2RtosMutexTest::%s - Release/Acquire mutex ID 'C' read 0x%0x expected 0x%0x \"FAILED\"\n",
               __FUNCTION__, mutexId, SEC_MUTEX_ID_TEST_C);
        return RC::SOFTWARE_ERROR;
    }
    if ((mutexId = AcquireMutexId()) != SEC_MUTEX_ID_TEST_D)
    {
        Printf(Tee::PriHigh, "Sec2RtosMutexTest::%s - Release/Acquire mutex ID 'D' read 0x%0x expected 0x%0x \"FAILED\"\n",
               __FUNCTION__, mutexId, SEC_MUTEX_ID_TEST_D);
        return RC::SOFTWARE_ERROR;
    }
    if ((mutexId = AcquireMutexId()) != regs.LookupValue(MODS_PSEC_MUTEX_ID_VALUE_NOT_AVAIL))
    {
        Printf(Tee::PriHigh, "Sec2RtosMutexTest::%s - Release/Acquire mutex ID exhaustion read 0x%0x expected 0x%0x \"FAILED\"\n",
               __FUNCTION__, mutexId, regs.LookupValue(MODS_PSEC_MUTEX_ID_VALUE_NOT_AVAIL));
        return RC::SOFTWARE_ERROR;
    }
    Printf(Tee::PriHigh, "Sec2RtosMutexTest::%s - Release/Acquire mutex out of order \"PASSED\"\n",
           __FUNCTION__);

    // Release all the mutexIds now, from 254-8
    while (mutexId > SEC_MUTEX_ID_RM_RANGE_START)
    {
        CHECK_RC(ReleaseMutexId(--mutexId));
    }
    Printf(Tee::PriHigh, "Sec2RtosMutexTest::%s - Releasing all mutex IDs \"PASSED\"\n",
           __FUNCTION__);

    // 6.  Checks that all the 0~15 MUTEX locks can be obtained and released

    // Lets acquire all mutexes
    mutexId = AcquireMutexId();

    // Readback and verify it
    for (index = 0; index < regs.LookupValue(MODS_PSEC_MUTEX__SIZE_1); index++)
    {
        CHECK_RC(TryMutex(index, mutexId, true));
    }
    Printf(Tee::PriHigh, "Sec2RtosMutexTest::%s - Acquiring all mutexes with first mutex ID 0x%0x \"PASSED\"\n",
           __FUNCTION__, mutexId);

    temp    = mutexId;
    mutexId = AcquireMutexId();
    // Current mutexId and previous mutexId have to be different
    if (mutexId == temp)
    {
        Printf(Tee::PriHigh, "Sec2RtosMutexTest::%s - Current mutexId 0x%0x and Previous mutexId 0x%0x are the same \"FAILED\"\n",
               __FUNCTION__, mutexId, temp);
        return RC::SOFTWARE_ERROR;
    }

    // Acquire mutexes again with different ID and it should fail
    CHECK_RC(ReleaseMutexId(temp));
    for (index = 0; index < regs.LookupValue(MODS_PSEC_MUTEX__SIZE_1); index++)
    {
        CHECK_RC(TryMutex(index, mutexId, false));
    }
    Printf(Tee::PriHigh, "Sec2RtosMutexTest::%s - Testing mutex acquire fail with second mutex ID 0x%0x \"PASSED\"\n",
           __FUNCTION__, mutexId);

    // 7. Checks that MUTEX lock can NOT be released by value other than 0
    for (LwU32 val = 1; val <= regs.LookupValue(MODS_PSEC_MUTEX_ID_VALUE_NOT_AVAIL); val++)
    {
        regs.Write32(MODS_PSEC_MUTEX, SEC_MUTEX_TEST_INDEX, val);
        CHECK_RC(TryMutex(SEC_MUTEX_TEST_INDEX, mutexId, false));
    }
    Printf(Tee::PriHigh, "Sec2RtosMutexTest::%s - Only 0 releases Mutexes \"PASSED\"\n",
           __FUNCTION__);

    // Release all mutexes
    for (index = 0; index < regs.LookupValue(MODS_PSEC_MUTEX__SIZE_1); index++)
    {
        CHECK_RC(ReleaseMutex(index));
    }
    Printf(Tee::PriHigh, "Sec2RtosMutexTest::%s - Releasing all mutexes \"PASSED\"\n",
           __FUNCTION__);

    // Acquire again and it should pass now
    for (index = 0; index < regs.LookupValue(MODS_PSEC_MUTEX__SIZE_1); index++)
    {
        CHECK_RC(TryMutex(index, mutexId, true));
    }
    Printf(Tee::PriHigh, "Sec2RtosMutexTest::%s - Acquiring all mutexes with second mutex ID 0x%0x \"PASSED\"\n",
           __FUNCTION__, mutexId);

    //
    // 8. Checks that the obtaining and releasing of different MUTEX locks can be out-of-order
    //
    //    Uses a different mutexID to reacquire
    //
    CHECK_RC(ReleaseMutex(4));
    CHECK_RC(ReleaseMutex(9));
    CHECK_RC(ReleaseMutex(2));
    CHECK_RC(ReleaseMutex(11));
    mutexId = AcquireMutexId();
    CHECK_RC(TryMutex(2, mutexId, true));
    CHECK_RC(TryMutex(9, mutexId, true));
    CHECK_RC(TryMutex(4, mutexId, true));
    CHECK_RC(TryMutex(11, mutexId, true));

    // Release all mutexes to reset state
    for (index = 0; index < regs.LookupValue(MODS_PSEC_MUTEX__SIZE_1); index++)
    {
        CHECK_RC(ReleaseMutex(index));
    }
    Printf(Tee::PriHigh, "Sec2RtosMutexTest::%s - Releasing all mutexes (second time) \"PASSED\"\n",
           __FUNCTION__);
    // Release all IDs to reset state
    for (mutexId = SEC_MUTEX_ID_RM_RANGE_START; mutexId < SEC_MUTEX_ID_RM_RANGE_END; mutexId++)
    {
        CHECK_RC(ReleaseMutexId(mutexId));
    }

    // Done!
    Printf(Tee::PriHigh, "********** Sec2RtosMutexTest::%s - Mutex verification end ********************\n",
           __FUNCTION__);

    return rc;
}

//! \brief Cleanup, does nothing for this test
//------------------------------------------------------------------------------
RC Sec2RtosMutexTest::Cleanup()
{
    m_pSec2Rtos = NULL;
    m_Parent = NULL;
    return OK;
}

//------------------------------------------------------------------------------
// JS Linkage
//------------------------------------------------------------------------------

//! \brief Linkage to JavaScript
//!
//! Setup the JS hierarchy to match the C++ one.
//------------------------------------------------------------------------------
JS_CLASS_INHERIT(Sec2RtosMutexTest, RmTest, "SEC2 RTOS Mutex Test. \n");
