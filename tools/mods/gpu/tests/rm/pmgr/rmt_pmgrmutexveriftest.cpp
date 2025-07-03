/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2015,2019 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

//!
//! \file rmt_PmgrMutexVerifTest.cpp
//! \brief rmtest for PMGR mutexes.
//!

#include "gpu/tests/rmtest.h"
#include "core/include/platform.h"
#include "gpu/tests/rm/utility/dtiutils.h"
#include "pascal/gp100/dev_pmgr.h"

#define IsGP10XorLater(p)   ((p >= Gpu::GP100))

class PmgrMutexVerifTest : public RmTest
{
public:
    PmgrMutexVerifTest();
    virtual ~PmgrMutexVerifTest();
    virtual string IsTestSupported();
    virtual RC Setup();
    virtual RC Run();
    virtual RC Cleanup();
    virtual RC VerifyMutexes();

    virtual LwU32 AcquireMutexId();
    virtual RC ReleaseMutexId(LwU32);
    virtual RC AcquireMutex(LwU32, LwU32, LwBool);
    virtual RC ReleaseMutex(LwU32);

private:
    GpuSubdevice *m_Parent = nullptr;    // GpuSubdevice that owns this PMGR

};

#define PMGR_MUTEX_ID_RM_RANGE_START            (8)
#define PMGR_MUTEX_ID_RM_RANGE_END              (254)
#define PMGR_MUTEX_ID_MAX_COUNT             (247)
#define PMGR_MUTEX_ID_TEST                      (63)
#define PMGR_MUTEX_INDEX_TEST                   (0)

//! \brief PmgrMutexVerifTest constructor
//!
//!
//! \sa Setup
//------------------------------------------------------------------------------
PmgrMutexVerifTest::PmgrMutexVerifTest()
{
    SetName("PmgrMutexVerifTest");
}

//! \brief PmgrMutexVerifTest destructor
//!
//!
//! \sa Cleanup
//------------------------------------------------------------------------------
PmgrMutexVerifTest::~PmgrMutexVerifTest()
{
}

//! \brief IsTestSupported
//!
//! Checks if the test can be run in the current environment.
//!
//! \return True if the test can be run in the current environment,
//!         false otherwise
//------------------------------------------------------------------------------
string PmgrMutexVerifTest::IsTestSupported()
{
    Gpu::LwDeviceId chipName = GetBoundGpuSubdevice()->DeviceId();
    if (IsGP10XorLater(chipName))
    {
        return RUN_RMTEST_TRUE;
    }
    return "Supported only on GP10X+ chips";
}

//! \brief Do Init from Js, allocate m_pPmgr and do initialization
//!
//! \return OK if no error happens, otherwise corresponding rc
//------------------------------------------------------------------------------
RC PmgrMutexVerifTest::Setup()
{
    RC rc = OK;

    m_Parent = GetBoundGpuSubdevice();

    return rc;
}

/*!
 * @brief Acquire mutex ID
 */
LwU32 PmgrMutexVerifTest::AcquireMutexId()
{
    LwU32 mutexId =  DRF_VAL(_PMGR, _MUTEX_ID_ACQUIRE, _VALUE, m_Parent->RegRd32(LW_PMGR_MUTEX_ID_ACQUIRE));
    return mutexId;
}

/*!
 * @brief Release mutex ID
 */
RC PmgrMutexVerifTest::ReleaseMutexId(LwU32 mutexId)
{
    if ((mutexId > PMGR_MUTEX_ID_RM_RANGE_END) || (mutexId < PMGR_MUTEX_ID_RM_RANGE_START))
    {
        Printf(Tee::PriHigh, "PmgrMutexVerifTest::%s Invalid mutexId 0x%0x\n", __FUNCTION__, mutexId);
        return RC::SOFTWARE_ERROR;
    }

    m_Parent->RegWr32(LW_PMGR_MUTEX_ID_RELEASE, mutexId);
    return OK;
}

/*!
 * @brief Acquire mutex
 */
RC PmgrMutexVerifTest::AcquireMutex(LwU32 mindex, LwU32 mutexId, LwBool bExpectAcquire)
{
    LwBool bAcquired = false;

    if ((mutexId > PMGR_MUTEX_ID_RM_RANGE_END) || (mutexId < PMGR_MUTEX_ID_RM_RANGE_START) ||
        (mindex >= LW_PMGR_MUTEX_REG__SIZE_1))
    {
        Printf(Tee::PriHigh, "PmgrMutexVerifTest::%s Invalid mutexId 0x%0x or mindex 0x%0x \n", __FUNCTION__, mutexId, mindex);
        return RC::SOFTWARE_ERROR;
    }

    m_Parent->RegWr32(LW_PMGR_MUTEX_REG(mindex), mutexId);

    // Readback and verify it
    if (m_Parent->RegRd32(LW_PMGR_MUTEX_REG(mindex)) == mutexId)
        bAcquired = true;

    if ((bAcquired && (!bExpectAcquire)) ||
        (!bAcquired && bExpectAcquire))
    {
        Printf(Tee::PriHigh, "PmgrMutexVerifTest::%s Mutex acquire status 0x%0x but expected acquire state 0x%0x\n",
                             __FUNCTION__, bAcquired, bExpectAcquire);
        return RC::SOFTWARE_ERROR;
    }
    return OK;
}

/*!
 * @brief Release mutex
 */
RC PmgrMutexVerifTest::ReleaseMutex(LwU32 mindex)
{
    if(mindex >= LW_PMGR_MUTEX_REG__SIZE_1)
        return RC::SOFTWARE_ERROR;

    m_Parent->RegWr32(LW_PMGR_MUTEX_REG(mindex), 0);
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
RC PmgrMutexVerifTest::VerifyMutexes()
{
    RC    rc         = OK;
    LwU32 mutexId    = 0;
    LwU32 index      = 0;
    LwU32 temp       = 0;
    LwU32 mutexCount = 0;

    Printf(Tee::PriHigh, "********** PmgrMutexVerifTest::%s - Mutex verification start ********************\n",
                    __FUNCTION__);

    //
    // Makes sure MUTEX ID generation works as expected
    // Read all possible ID and check if it reaches the max
    //

    // Read first mutex and make sure we get a valid one.
    if ((mutexId = AcquireMutexId()) == LW_PMGR_MUTEX_ID_ACQUIRE_VALUE_NOT_AVAIL)
    {
        Printf(Tee::PriHigh, "PmgrMutexVerifTest::%s - First mutex acquire failed. Something fishy. \"FAILED\"\n",
                              __FUNCTION__);
        return RC::SOFTWARE_ERROR;
    }
    else
    {
        Printf(Tee::PriHigh, "PmgrMutexVerifTest::%s - First mutex ID read \"PASSED\"\n",
                              __FUNCTION__);
        mutexCount++;
    }

    // Acquire all the mutex ids.
    while (AcquireMutexId() != LW_PMGR_MUTEX_ID_ACQUIRE_VALUE_NOT_AVAIL )
    {
        mutexCount++;
    }

    if (mutexCount != PMGR_MUTEX_ID_MAX_COUNT)
    {
        Printf(Tee::PriHigh, "PmgrMutexVerifTest::%s - Not all mutex ids were acquired : %d \"FAILED\"\n",
                              __FUNCTION__, mutexCount);
        return RC::SOFTWARE_ERROR;
    }
    else
    {
        Printf(Tee::PriHigh, "PmgrMutexVerifTest::%s - All mutex ids acquired \"PASSED\"\n",
                              __FUNCTION__);
    }

    //
    // We obtained all the mutexIds above..
    // Try releasing a mutexId and acquire it back..
    //
    CHECK_RC(ReleaseMutexId(mutexId));

    if (AcquireMutexId() != mutexId)
    {
        Printf(Tee::PriHigh, "PmgrMutexVerifTest::%s - Test/Release mutex ID read \"FAILED\"\n",
                              __FUNCTION__);
        return RC::SOFTWARE_ERROR;
    }
    else
    {
        Printf(Tee::PriHigh, "PmgrMutexVerifTest::%s - Test/Release mutex ID read \"PASSED\"\n",
                              __FUNCTION__);
    }

    // Release all the mutexIds now
    mutexCount = PMGR_MUTEX_ID_RM_RANGE_END;
    while (mutexCount >= PMGR_MUTEX_ID_RM_RANGE_START)
    {
        CHECK_RC(ReleaseMutexId(mutexCount--));
    }
    Printf(Tee::PriHigh, "PmgrMutexVerifTest::%s - Releasing all mutex IDs \"PASSED\"\n",
                              __FUNCTION__);

    // Lets acquire all mutexes
    mutexId = AcquireMutexId();
    while (index < LW_PMGR_MUTEX_REG__SIZE_1)
    {
        CHECK_RC(AcquireMutex(index, mutexId, true));
        ++index;
    }
    Printf(Tee::PriHigh, "PmgrMutexVerifTest::%s - Acquiring all mutexes with mutex ID 0x%0x \"PASSED\"\n",
                    __FUNCTION__, mutexId);

    // Acquire mutexes again and it should fail
    temp    = mutexId;
    mutexId = AcquireMutexId();
    CHECK_RC(ReleaseMutexId(temp));

    // Current mutexId and previous mutexId has to be different
    if (mutexId == temp)
    {
        Printf(Tee::PriHigh, "PmgrMutexVerifTest::%s - Current 0x%0x & Previous 0x%0x mutexId comparison \"FAILED\"\n",
                              __FUNCTION__, mutexId, temp);
        return RC::SOFTWARE_ERROR;
    }
    else
    {
        Printf(Tee::PriHigh, "PmgrMutexVerifTest::%s - Current 0x%0x & Previous 0x%0x mutexId comparison \"PASSED\"\n",
                              __FUNCTION__, mutexId, temp);
    }

    index = 0;
    while (index < LW_PMGR_MUTEX_REG__SIZE_1)
    {
        CHECK_RC(AcquireMutex(index, mutexId, false));
        ++index;
    }

    Printf(Tee::PriHigh, "PmgrMutexVerifTest::%s - Testing mutex acquire fail with mutex ID 0x%0x \"PASSED\"\n",
                    __FUNCTION__, mutexId);

    // Release all mutexes
    index = 0;
    while (index < LW_PMGR_MUTEX_REG__SIZE_1)
    {
        CHECK_RC(ReleaseMutex(index));
        ++index;
    }
    Printf(Tee::PriHigh, "PmgrMutexVerifTest::%s - Releasing all mutexes \"PASSED\"\n",
                    __FUNCTION__);

    // Acquire again and it should pass now
    index = 0;
    while (index < LW_PMGR_MUTEX_REG__SIZE_1)
    {
        CHECK_RC(AcquireMutex(index, mutexId, true));
        ++index;
    }
    Printf(Tee::PriHigh, "PmgrMutexVerifTest::%s - Acquiring all mutexes with mutex ID 0x%0x \"PASSED\"\n",
                    __FUNCTION__, mutexId);

    // Release all the mutexes as the test ends here.
    index = 0;
    while (index < LW_PMGR_MUTEX_REG__SIZE_1)
    {
        CHECK_RC(ReleaseMutex(index));
        ++index;
    }
    CHECK_RC(ReleaseMutexId(mutexId));

    Printf(Tee::PriHigh, "********** PmgrMutexVerifTest::%s - Mutex verification end ********************\n",
                    __FUNCTION__);

    return rc;
}

//! \brief Run the test
//!
//!
//! \return OK all tests pass, else corresponding rc
//------------------------------------------------------------------------------
RC PmgrMutexVerifTest::Run()
{
    RC rc = OK;

    CHECK_RC(VerifyMutexes());
    return rc;
}

//! \brief Cleanup, does nothing for this test
//------------------------------------------------------------------------------
RC PmgrMutexVerifTest::Cleanup()
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
JS_CLASS_INHERIT(PmgrMutexVerifTest, RmTest, "PMGR Verif Feature Test. \n");
