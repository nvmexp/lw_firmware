/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2014-2017 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file    fbflcn_mutex.c
 * @brief   Contains all PCB Management (PGMR) routines specific to GP10X.
 *          Origin of the file is here:
 * 			//sw/dev/gpu_drv/chips_a/pmu_sw/prod_app/pmgr/pascal/pmu_pmgrgp10x.c
 */

/* ------------------------- System Includes -------------------------------- */
#include <falcon-intrinsics.h>
#include "osptimer.h"

// include manuals
#include "dev_fbfalcon_csb.h"
#include "dev_pmgr.h"

/* ------------------------- Application Includes --------------------------- */
#include "priv.h"
#include "fbflcn_mutex.h"
#include "fbflcn_defines.h"
#include "fbflcn_timer.h"

/* ------------------------- Type Definitions ------------------------------- */
/* ------------------------- External Definitions --------------------------- */
/* ------------------------- Static Variables ------------------------------- */
/* ------------------------- Macros and Defines ----------------------------- */
/*!
 * Utility macro for acquiring a PMGR HW mutex using the mutex index. A mutex
 * is acquired by writing an id to the mutex register.  Unique ids may be
 * generated using the PMGR mutex-id generator. This HW guarantees that
 * ids used by each of the clients remain unique.
 *
 * @param[in]  mutexRegister  The mutex index register
 * @param[in]  id             The identifier used to acquire the mutex
 */


#define MUTEX_ACQUIRE_GP100(mutexRegister, id)                                 \
    REG_WR32_STALL(BAR0, mutexRegister, id)

/*!
 * Utility macro that may be used validate an index to ensure a valid PMGR HW
 * mutex corresponds to that index.  A non-zero return value represents a
 * valid mutex; zero represents an invalid mutex.
 *
 * @param[in]  mutexIndex  The index to validate
 */

#define MUTEX_INDEX_IS_VALID_GP100(mutexIndex, maxIndex)                       \
        ((mutexIndex) < maxIndex)

/*!
 * Utility macro for releasing a PMGR HW mutex. A mutex is released by writing
 * LW_PMGR_MUTEX_REG_VALUE_INITIAL_LOCK to the mutex register.
 *
 * @param[in]  mutexIndex  The index of the mutex to release
 */

#define MUTEX_RELEASE_GP100(mutexRegister, initialLockVal)                        \
        REG_WR32_STALL(BAR0, mutexRegister, initialLockVal)

/*!
 * Utility macro for retrieving the owner-id for a specific mutex.  When the
 * mutex is not acquired (free), LW_PMGR_MUTEX_REG_VALUE_INITIAL_LOCK will be
 * returned.
 *
 * @param[in]  mutexIndex  The index of the mutex to read
 */

#define MUTEX_GET_OWNER_GP100(mutexRegister)                                      \
        REG_RD32(BAR0, mutexRegister)

/*!
 * Utility macro for exercising the PMGR hardware to generate a unique-id that
 * may be used for acquiring PMGR mutexes.
 * LW_PMGR_MUTEX_ID_ACQUIRE_VALUE_NOT_AVAIL will be returned if not id are
 * lwrrently available (all taken).
 */

#define MUTEX_ID_GENERATE_GP100(mutexAcquireRegister)                             \
        REG_RD32(BAR0, mutexAcquireRegister)

/*!
 * Utility macro for releasing an identifer back to PMGR mutex-id generator.
 * This allows the id generator to reuse the id in subsequent requests.
 * Failure to release mutex-identifiers will result in a id-leak and will
 * eventually result in unrecoverable failure.
 *
 * @param[in]  id  The identifier to release.
 */

#define MUTEX_ID_RELEASE_GP100(mutexReleaseRegister,id)                                             \
        REG_WR32_STALL(BAR0, mutexReleaseRegister, id)


/* ------------------------- Prototypes ------------------------------------- */
/* ------------------------- Public Functions  ------------------------------ */

/*!
 * @brief Acquire a PMGR HW mutex.
 *
 * This function attempts to acquire the HW mutex corresponding to the provided
 * mutex-index.
 *
 * If the mutex resource is already owned/acquired, this acquire code will retry
 * to acquire the semaphore for up to the specified timeout duration (in ns).
 *
 * Note, this is is a binary semaphore, so if the caller owns the mutex, the
 * request will be failed.
 *
 * This function is reentrant by design and does not require higher-level
 * synchronization by the caller.
 *
 * @param[in]   mutexIndex  The logical identifier for the mutex to lock
 *
 * @param[in]   timeoutns
 *     Duration for which interface should retry attempting to acquire the lock.
 *     Specified in units of ns.  Zero (0) indicates that the interface only try
 *     acquire once and then return immediately.
 *
 * @param[out]  pToken      Written with the id used to acquire the mutex up
 *                          success.  Unmodified, if the acquire operation
 *                          fails.
 *
 * @return LW_OK
 *         Upon successful acquirement of the mutex.
 *
 * @return LW_ERR_ILWALID_ARGUMENT
 *         If the token pointer is NULL or if the given mutex-identifier is
 *         illegal
 *
 * @return LW_ERR_BUSY_RETRY
 *         If the mutex is not available
 *
 * @return LW_ERR_INSUFFICIENT_RESOURCES
 *         If the PMGR mutex-id generator was not able to generate a mutex-id
 *         for acquiring the mutex.
 *
 */
LW_STATUS
mutexAcquireByIndex_GV100
(
        MUTEX_HANDLER* pMutex,
        LwU32  timeoutns,
        LwU8  *pToken
)
{
    LwU8             owner;
    LwU8             genId;
    LW_STATUS    status = LW_ERR_GENERIC;
    FLCN_TIMESTAMP   timeStart;

    // validate the arguments
    // not needed since we're using defines/hardcoded

    //
    // Use the mutex-id generator to create a unique 8-bit id that may be used
    // to lock the target mutex.
    //
    genId = MUTEX_ID_GENERATE_GP100(pMutex->registerIdAcquire);
    if ((genId == pMutex->valueAcquireNotAvailable) ||
            (genId == pMutex->valueAcquireInit))
    {
        return LW_ERR_INSUFFICIENT_RESOURCES;
    }
    FW_MBOX_WR32_STALL(10, genId);
    FW_MBOX_WR32_STALL(10, timeoutns);
    // If a timeout has been specified read current time to use for delay.
    if (timeoutns != 0)
    {
        osPTimerTimeNsLwrrentGet(&timeStart);
    }

    //
    // Write the id into the mutex register to attempt an "acquire" on the
    // mutex. Read the register back to see if the id stuck.  If the value read
    // back matches the id written, the mutex was successfully acquired.
    // Perform this operation until the mutex is acquired or the allowed timeout
    // is exceeded.
    //
    while (LW_ERR_GENERIC == status)
    {
        MUTEX_ACQUIRE_GP100(pMutex->registerMutex, genId);
        owner = MUTEX_GET_OWNER_GP100(pMutex->registerMutex);

        // If mutex successfully acquired, mark success/LW_OK.
        if (owner == genId)
        {
            status = LW_OK;
        }
        // Bail on timeout or an unsuccesful single-shot.
        else if ((timeoutns == 0) ||
                (timeoutns < osPTimerTimeNsElapsedGet(&timeStart)))
        {
            status = LW_ERR_BUSY_RETRY;
        }

        //
        // For further thought: If encountering inter-task contention for
        // mutexes, might consider adding a yield() call when mutex has not been
        // acquired.
        //
    }

    // Mutex successfully acquired, return token/id.
    if (LW_OK == status)
    {
        *pToken = genId;
    }
    // Otherwise, release the id.
    else
    {
        *pToken = 0;
        MUTEX_ID_RELEASE_GP100(pMutex->registerIdRelease,genId);
    }

    return status;
}


/*!
 * @brief Release a previously acquired PMGR HW Mutex.
 *
 * This function attempts to release a PMGR HW mutex using the index of the
 * mutex register as an identifier. The mutex is only released when the caller
 * owns the mutex. This information is relayed by the token argument. If the
 * caller owns the mutex, the mutex will be released.
 *
 * This function is reentrant by design and does not require higher-level
 * synchronization by the caller.
 *
 * @param[in]  mutexIndex  The logical identifier for the mutex to release
 * @param[in]  token       The token that was returned by @ref
 *                         pmgrMutexAcquire_GP10X when the mutex was first
 *                         acquired.
 *
 * @return LW_OK
 *         Upon successful release of the mutex.
 *
 * @return LW_ERR_ILWALID_ARGUMENT
 *         If the token pointer is NULL or if the given mutex-identifier is
 *         illegal
 *
 * @return LW_ERR_ILWALID_OPERATION
 *         If the caller does not own the mutex
 */
LW_STATUS
mutexReleaseByIndex_GV100
(
        MUTEX_HANDLER* pMutex,
        LwU8  token
)
{
    LwU8  owner;

    // validate the arguments
    /* TODO: stefans this needs to validated externally
    if (!MUTEX_INDEX_IS_VALID_GP100(mutexIndex,MUTEX_REG_SIZE))
    {
        return LW_ERR_ILWALID_ARGUMENT;
    }
     */

    //
    // First verify that the caller owns mutex that is about to be released.
    // This is done by verifying that the current mutex register contains the
    // token that the caller has provided.  If the caller does not own the
    // mutex, bail (do NOT release the provided token).
    //
    //
    owner = MUTEX_GET_OWNER_GP100(pMutex->registerMutex);
    if (owner != token)
    {
        return LW_ERR_ILWALID_OPERATION;
    }

    //
    // Caller owns the mutex.  Release the mutex, release the token, and
    // return success.
    //
    MUTEX_RELEASE_GP100(pMutex->registerMutex,pMutex->valueInitialLock);
    MUTEX_ID_RELEASE_GP100(pMutex->registerIdRelease,token);
    return LW_OK;
}


