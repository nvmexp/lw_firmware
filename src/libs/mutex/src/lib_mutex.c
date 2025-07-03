/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2016-2019 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file  lib_mutex.c
 */

/* ------------------------- System Includes -------------------------------- */
#include "regmacros.h"
#include "lwrtos.h"

/* ------------------------- Application Includes --------------------------- */
#include "lib_mutex.h"
#include "osptimer.h"
#include "lwosreg.h"
#ifdef FPGA_BUILD
#include "dev_master.h"
#endif

/* ------------------------- Macros and Defines ----------------------------- */
/* ------------------------- Prototypes ------------------------------------- */
extern LwU32       mutexRegRd32(LwU8 bus, LwU32 addr);
extern void        mutexRegWr32(LwU8 bus, LwU32 addr, LwU32 val);
extern FLCN_STATUS mutexRegRd32_hs(LwU8 bus, LwU32 addr, LwU32 *pData);
extern FLCN_STATUS mutexRegWr32_hs(LwU8 bus, LwU32 addr, LwU32 val);
#ifdef FPGA_BUILD
static FLCN_STATUS _mutexIsFpgaPlatform(LwBool *pBIsFpga)
    GCC_ATTRIB_SECTION("imem_libCommonHs", "_mutexIsFpgaPlatform");
#endif

/* ------------------------- Global Variables ------------------------------- */
/* ------------------------- Static Variables ------------------------------- */
/* ------------------------- Private Functions ------------------------------ */
/* ------------------------- Public Functions ------------------------------- */
/*!
 * @brief Acquire a HW mutex.
 *
 * This function attempts to acquire the HW mutex corresponding to the provided
 * mutex-index.
 *
 * If the mutex resource is already owned/acquired, this acquire code will retry
 * to acquire the semaphore for up to the specified timeout duration (in ns).
 *
 * Note, this is a binary semaphore, so if the caller owns the mutex, the
 * request will be failed.
 *
 * This function is reentrant by design and does not require higher-level
 * synchronization by the caller.
 *
 * @param[in]   mutexLogId  The logical identifier for the mutex to lock
 *
 * @param[in]   timeoutNs
 *     Duration for which interface should retry attempting to acquire the lock.
 *     Specified in units of ns.  Zero (0) indicates that the interface only try
 *     acquire once and then return immediately.
 *
 * @param[out]  pToken      Written with the id used to acquire the mutex up
 *                          success.  Unmodified, if the acquire operation
 *                          fails.
 *
 * @return FLCN_OK
 *         Upon successful acquirement of the mutex.
 *
 * @return FLCN_ERR_ILWALID_ARGUMENT
 *         If the token pointer is NULL or if the given mutex-identifier is
 *         illegal
 *
 * @return FLCN_ERR_MUTEX_ACQUIRED
 *         If the mutex is not available
 *
 * @return FLCN_ERR_MUTEX_ID_NOT_AVAILABLE
 *         If the mutex-id generator was not able to generate a mutex-id for
 *         acquiring the mutex.
 */
FLCN_STATUS
mutexAcquire
(
    LwU8   mutexLogId,
    LwU32  timeoutNs,
    LwU8  *pToken
)
{
    PMUTEX_ENGINE_INFO pEngineInfo = MUTEX_GET_ENG_INFO(mutexLogId);
    LwU8               mutexPhyId  = MUTEX_LOG_TO_PHY_ID(mutexLogId);
    LwU8               owner;
    LwU8               genId;
    FLCN_STATUS        status      = FLCN_ERR_MORE_PROCESSING_REQUIRED;
    FLCN_TIMESTAMP     timeStart;
    LwU8               accessBus;

    // validate the arguments
    if ((pToken == NULL) ||
        (pEngineInfo == NULL) ||
        (!MUTEX_INDEX_IS_VALID(pEngineInfo, mutexPhyId)))
    {
        return FLCN_ERR_ILWALID_ARGUMENT;
    }

    accessBus = pEngineInfo->accessBus;

    //
    // Use the mutex-id generator to create a unique 8-bit id that may be used
    // to lock the target mutex.
    //
    genId = DRF_VAL(_FLCN, _MUTEX_ID, _VALUE,
                    osRegRd32(accessBus, pEngineInfo->regAddrMutexId));
    if ((genId == LW_FLCN_MUTEX_ID_VALUE_NOT_AVAIL) ||
        (genId == LW_FLCN_MUTEX_ID_VALUE_INIT))
    {
        return FLCN_ERR_MUTEX_ID_NOT_AVAILABLE;
    }

    // If a timeout has been specified read current time to use for delay.
    if (timeoutNs != 0U)
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
    while (status == FLCN_ERR_MORE_PROCESSING_REQUIRED)
    {
        // Attempt to Acquire Mutex
        osRegWr32(accessBus, MUTEX_REG_ADDR(pEngineInfo, mutexPhyId),
            DRF_NUM(_FLCN, _MUTEX, _VALUE, genId));

        // If mutex successfully acquired, mark success.
        owner = MUTEX_GET_OWNER(pEngineInfo, mutexPhyId);
        if (owner == genId)
        {
            status = FLCN_OK;
        }
        // Bail on timeout or an unsuccessful single-shot.
        else if ((timeoutNs == 0U) ||
                 (timeoutNs < osPTimerTimeNsElapsedGet(&timeStart)))
        {
            status = FLCN_ERR_MUTEX_ACQUIRED;
        }
        // Else Yield to other tasks while waiting.
        else
        {
            lwrtosYIELD();
        }
    }

    // Mutex successfully acquired, return token/id.
    if (FLCN_OK == status)
    {
        *pToken = genId;
    }
    // Otherwise, release the id.
    else
    {
        osRegWr32(accessBus, pEngineInfo->regAddrMutexIdRel,
            FLD_SET_DRF_NUM(_FLCN, _MUTEX_ID_RELEASE, _VALUE, genId,
            osRegRd32(accessBus, pEngineInfo->regAddrMutexIdRel)));
    }

    return status;
}

/*!
 * @brief Release a previously acquired HW Mutex.
 *
 * This function attempts to release a HW mutex using the index of the mutex
 * register as an identifier. The mutex is only released when the caller
 * owns the mutex. This information is relayed by the token argument. If the
 * caller owns the mutex, the mutex will be released.
 *
 * This function is reentrant by design and does not require higher-level
 * synchronization by the caller.
 *
 * @param[in]  mutexLogId  The logical identifier for the mutex to release
 * @param[in]  token       The token returned by @ref mutexAcquire when the
 *                         mutex was first acquired.
 *
 * @return FLCN_OK
 *         Upon successful release of the mutex.
 *
 * @return FLCN_ERR_ILWALID_ARGUMENT
 *         If the token pointer is NULL or if the given mutex-identifier is
 *         illegal
 *
 * @return FLCN_ERR_ILLEGAL_OPERATION
 *         If the caller does not own the mutex
 */
FLCN_STATUS
mutexRelease
(
    LwU8  mutexLogId,
    LwU8  token
)
{
    PMUTEX_ENGINE_INFO pEngineInfo = MUTEX_GET_ENG_INFO(mutexLogId);
    LwU8               mutexPhyId  = MUTEX_LOG_TO_PHY_ID(mutexLogId);
    LwU8               owner;
    LwU8               accessBus;

    // validate the arguments
    if ((pEngineInfo == NULL) ||
        (!MUTEX_INDEX_IS_VALID(pEngineInfo, mutexPhyId)))
    {
        return FLCN_ERR_ILWALID_ARGUMENT;
    }

    accessBus = pEngineInfo->accessBus;

    //
    // First verify that the caller owns mutex that is about to be released.
    // This is done by verifying that the current mutex register contains the
    // token that the caller has provided.  If the caller does not own the
    // mutex, bail (do NOT release the provided token).
    //
    owner = MUTEX_GET_OWNER(pEngineInfo, mutexPhyId);
    if (owner != token)
    {
        return FLCN_ERR_ILLEGAL_OPERATION;
    }

    // Caller owns the mutex, release the mutex.
    osRegWr32(accessBus, MUTEX_REG_ADDR(pEngineInfo, mutexPhyId),
        DRF_DEF(_FLCN, _MUTEX, _VALUE, _INITIAL_LOCK));

    // Release the token and return success.
    osRegWr32(accessBus, pEngineInfo->regAddrMutexIdRel,
        FLD_SET_DRF_NUM(_FLCN, _MUTEX_ID_RELEASE, _VALUE, token,
        osRegRd32(accessBus, pEngineInfo->regAddrMutexIdRel)));
    return FLCN_OK;
}

/*!
 * (TODO - This is replicated from LS code for quickly upgrading mutex infra to
 * support HS.  It needs to be removed once the LS/HS code sharing mechanism is
 * implemented)
 *
 * @brief Acquire a HW mutex at HS mode.
 *
 * This function attempts to acquire the HW mutex corresponding to the provided
 * mutex-index.
 *
 * If the mutex resource is already owned/acquired, this acquire code will retry
 * to acquire the semaphore for up to the specified timeout duration (in ns).
 *
 * Note, this is a binary semaphore, so if the caller owns the mutex, the
 * request will be failed.
 *
 * This function is reentrant by design and does not require higher-level
 * synchronization by the caller.
 *
 * @param[in]   mutexLogId  The logical identifier for the mutex to lock
 *
 * @param[in]   timeoutNs
 *     Duration for which interface should retry attempting to acquire the lock.
 *     Specified in units of ns.  Zero (0) indicates that the interface only try
 *     acquire once and then return immediately.
 *
 * @param[out]  pToken      Written with the id used to acquire the mutex up
 *                          success.  Unmodified, if the acquire operation
 *                          fails.
 *
 * @return FLCN_OK
 *         Upon successful acquirement of the mutex.
 *
 * @return FLCN_ERR_ILWALID_ARGUMENT
 *         If the token pointer is NULL or if the given mutex-identifier is
 *         illegal
 *
 * @return FLCN_ERR_MUTEX_ACQUIRED
 *         If the mutex is not available
 *
 * @return FLCN_ERR_MUTEX_ID_NOT_AVAILABLE
 *         If the mutex-id generator was not able to generate a mutex-id for
 *         acquiring the mutex.
 */
FLCN_STATUS
mutexAcquire_hs
(
    LwU8   mutexLogId,
    LwU32  timeoutNs,
    LwU8  *pToken
)
{
    //
    // WARNING: Although it is not desirable to access mutexEngineInfo (which is
    // inited by LS code) here, but this is already the best thing we can do in
    // short term. This is fixable by using HS ucode for initing that data
    // struct and using dmlvl to make it write protected by LS ucode after init
    // is done. The long term solution is tracked in bug 200232883.
    //
    // Besides, we are not checking mutexLogId being out of bounds because
    // MUTEX_GET_ENG_INFO has a check to return invalid index and that invalid
    // index will be trapped in the 'if' check below
    //
    PMUTEX_ENGINE_INFO pEngineInfo = MUTEX_GET_ENG_INFO(mutexLogId);
    //
    // Actually LwU8 is enough to represent all possible physical IDs, but GCC
    // generates __Mul call (a libgcc function) for LwU8*LwU16 operation
    // (Bug 200232732), and __Mul can't work in HS mode, thus caused the macro
    // MUTEX_REG_ADDR to fail at HS mode. So here the WAR is to declare this
    // variable as LwU16 so that the code in MUTEX_REG_ADDR will be LwU16*LwU16
    // and gcc uses "mulu" instruction for this condition which can work at HS
    // mode.
    //
    // WARNING: Although it is not desirable to access mutexLog2PhyTable (which
    // is inited by LS code) here, but this is already the best thing we can do
    // in short term. This is fixable by using HS ucode for initing that data
    // struct and using dmlvl to make it write protected by LS ucode after init
    // is done. The long term solution is tracked in bug 200232883.
    //
    LwU16              mutexPhyId  = MUTEX_LOG_TO_PHY_ID(mutexLogId);
    LwU8               owner;
    //
    // The init value of genId can't be a valid mutex ID (so it can only be
    // LW_FLCN_MUTEX_ID_VALUE_INIT or LW_FLCN_MUTEX_ID_VALUE_NOT_AVAIL so we
    // won't try to release the mutex again at the end of this function when
    // there is any error happening before the mutex is acquired.
    // And LW_FLCN_MUTEX_ID_VALUE_INIT is picked since it makes more sense as a
    // init value.
    //
    LwU8               genId       = LW_FLCN_MUTEX_ID_VALUE_INIT;
    LwU32              data32;
    FLCN_STATUS        flcnStatus;
    FLCN_TIMESTAMP     timeStart;
    LwBool             bLsMode     = LW_FALSE;

#ifdef FPGA_BUILD
    LwBool             bIsFpga     = LW_FALSE;

    CHECK_FLCN_STATUS(_mutexIsFpgaPlatform(&bIsFpga));
    if (bIsFpga)
    {
        goto _skipBootModeCheck;
    }
#endif

    // Ensure we booted at LS mode before going further
    extern void vApplicationIsLsOrHsModeSet_hs(LwBool *, LwBool *);
    vApplicationIsLsOrHsModeSet_hs(&bLsMode, NULL);
    if (!bLsMode)
    {
        CHECK_FLCN_STATUS(FLCN_ERR_HS_CHK_NOT_IN_LSMODE);
    }

#ifdef FPGA_BUILD
_skipBootModeCheck:
#endif

    // validate the arguments
    if ((pToken == NULL) ||
        (pEngineInfo == NULL) ||
        (!MUTEX_INDEX_IS_VALID(pEngineInfo, mutexPhyId)))
    {
        CHECK_FLCN_STATUS(FLCN_ERR_ILWALID_ARGUMENT);
    }

    //
    // Use the mutex-id generator to create a unique 8-bit id that may be used
    // to lock the target mutex.
    //
    CHECK_FLCN_STATUS(MUTEX_REG_RD32_HS(pEngineInfo, pEngineInfo->regAddrMutexId, &data32));

    genId = (LwU8)DRF_VAL(_FLCN, _MUTEX_ID, _VALUE, data32);
    if ((genId == LW_FLCN_MUTEX_ID_VALUE_NOT_AVAIL) ||
        (genId == LW_FLCN_MUTEX_ID_VALUE_INIT))
    {
        CHECK_FLCN_STATUS(FLCN_ERR_MUTEX_ID_NOT_AVAILABLE);
    }

    // If a timeout has been specified read current time to use for delay.
    if (timeoutNs != 0U)
    {
        osPTimerTimeNsLwrrentGet_hs(&timeStart);
    }

    //
    // Write the id into the mutex register to attempt an "acquire" on the
    // mutex. Read the register back to see if the id stuck.  If the value read
    // back matches the id written, the mutex was successfully acquired.
    // Perform this operation until the mutex is acquired or the allowed timeout
    // is exceeded.
    //
    do
    {
        // Attempt to Acquire Mutex
        CHECK_FLCN_STATUS(MUTEX_REG_WR32_HS(pEngineInfo, MUTEX_REG_ADDR(pEngineInfo, mutexPhyId),
            DRF_NUM(_FLCN, _MUTEX, _VALUE, genId)));

        // If mutex successfully acquired, mark success.
        CHECK_FLCN_STATUS(MUTEX_REG_RD32_HS(pEngineInfo, MUTEX_REG_ADDR(pEngineInfo, mutexPhyId), &data32));
        owner = (LwU8)DRF_VAL(_FLCN, _MUTEX, _VALUE, data32);
        if (owner == genId)
        {
            flcnStatus = FLCN_OK;
        }
        // Bail on timeout or an unsuccessful single-shot.
        else if ((timeoutNs == 0U) ||
                 (timeoutNs < osPTimerTimeNsElapsedGet_hs(&timeStart)))
        {
            CHECK_FLCN_STATUS(FLCN_ERR_MUTEX_ACQUIRED);
        }
        else
        {
            flcnStatus = FLCN_ERR_MORE_PROCESSING_REQUIRED;
        }
    } while (flcnStatus == FLCN_ERR_MORE_PROCESSING_REQUIRED);


ErrorExit:
    // Mutex successfully acquired, return token/id.
    if (FLCN_OK == flcnStatus)
    {
        *pToken = genId;
    }
    // Otherwise, release the id.
    else if ((genId != LW_FLCN_MUTEX_ID_VALUE_NOT_AVAIL) &&
             (genId != LW_FLCN_MUTEX_ID_VALUE_INIT))
    {
        // Ignore the error status purposely and attempt to release the acquired mutex to avoid leaking mutex tokens
        if (MUTEX_REG_RD32_HS(pEngineInfo, pEngineInfo->regAddrMutexIdRel, &data32) == FLCN_OK)
        {
            (void)MUTEX_REG_WR32_HS(pEngineInfo, pEngineInfo->regAddrMutexIdRel,
                FLD_SET_DRF_NUM(_FLCN, _MUTEX_ID_RELEASE, _VALUE, genId, data32));
        }
    }

    return flcnStatus;
}

/*!
 * (TODO - This is replicated from LS code for quickly upgrading mutex infra to
 * support HS.  It needs to be removed once the LS/HS code sharing mechanism is
 * implemented)
 *
 * @brief Release a previously acquired HW Mutex at HS mode.
 *
 * This function attempts to release a HW mutex using the index of the mutex
 * register as an identifier. The mutex is only released when the caller
 * owns the mutex. This information is relayed by the token argument. If the
 * caller owns the mutex, the mutex will be released.
 *
 * This function is reentrant by design and does not require higher-level
 * synchronization by the caller.
 *
 * @param[in]  mutexLogId  The logical identifier for the mutex to release
 * @param[in]  token       The token returned by @ref mutexAcquire when the
 *                         mutex was first acquired.
 *
 * @return FLCN_OK
 *         Upon successful release of the mutex.
 *
 * @return FLCN_ERR_ILWALID_ARGUMENT
 *         If the token pointer is NULL or if the given mutex-identifier is
 *         illegal
 *
 * @return FLCN_ERR_ILLEGAL_OPERATION
 *         If the caller does not own the mutex
 */
FLCN_STATUS
mutexRelease_hs
(
    LwU8  mutexLogId,
    LwU8  token
)
{
    //
    // WARNING: Although it is not desirable to access mutexEngineInfo (which is
    // inited by LS code) here, but this is already the best thing we can do in
    // short term. This is fixable by using HS ucode for initing that data
    // struct and using dmlvl to make it write protected by LS ucode after init
    // is done. The long term solution is tracked in bug 200232883.
    //
    // Besides, we are not checking mutexLogId being out of bounds because
    // MUTEX_GET_ENG_INFO has a check to return invalid index and that invalid
    // index will be trapped in the 'if' check below
    //
    PMUTEX_ENGINE_INFO pEngineInfo = MUTEX_GET_ENG_INFO(mutexLogId);
    //
    // Actually LwU8 is enough to represent all possible physical IDs, but GCC
    // generates __Mul call (a libgcc function) for LwU8*LwU16 operation
    // (Bug 200232732), and __Mul can't work in HS mode, thus caused the macro
    // MUTEX_REG_ADDR to fail at HS mode. So here the WAR is to declare this
    // variable as LwU16 so that the code in MUTEX_REG_ADDR will be LwU16*LwU16
    // and gcc uses "mulu" instruction for this condition which can work at HS
    // mode.
    //
    // WARNING: Although it is not desirable to access mutexLog2PhyTable (which
    // is inited by LS code) here, but this is already the best thing we can do
    // in short term. This is fixable by using HS ucode for initing that data
    // struct and using dmlvl to make it write protected by LS ucode after init
    // is done. The long term solution is tracked in bug 200232883.
    //
    LwU16              mutexPhyId  = MUTEX_LOG_TO_PHY_ID(mutexLogId);
    LwU8               owner;
    LwU32              data32;
    FLCN_STATUS        flcnStatus;
    LwBool             bLsMode     = LW_FALSE;

#ifdef FPGA_BUILD
    LwBool             bIsFpga     = LW_FALSE;

    CHECK_FLCN_STATUS(_mutexIsFpgaPlatform(&bIsFpga));
    if (bIsFpga)
    {
        goto _skipBootModeCheck;
    }
#endif

    // Ensure we booted at LS mode before going further
    extern void vApplicationIsLsOrHsModeSet_hs(LwBool *, LwBool *);
    vApplicationIsLsOrHsModeSet_hs(&bLsMode, NULL);
    if (!bLsMode)
    {
        CHECK_FLCN_STATUS(FLCN_ERR_HS_CHK_NOT_IN_LSMODE);
    }

#ifdef FPGA_BUILD
_skipBootModeCheck:
#endif

    // validate the arguments
    if ((pEngineInfo == NULL) ||
        (!MUTEX_INDEX_IS_VALID(pEngineInfo, mutexPhyId)))
    {
        CHECK_FLCN_STATUS(FLCN_ERR_ILWALID_ARGUMENT);
    }

    //
    // First verify that the caller owns mutex that is about to be released.
    // This is done by verifying that the current mutex register contains the
    // token that the caller has provided.  If the caller does not own the
    // mutex, bail (do NOT release the provided token).
    //
    CHECK_FLCN_STATUS(MUTEX_REG_RD32_HS(pEngineInfo, MUTEX_REG_ADDR(pEngineInfo, mutexPhyId), &data32));
    owner = (LwU8)DRF_VAL(_FLCN, _MUTEX, _VALUE, data32);
    if (owner != token)
    {
        CHECK_FLCN_STATUS(FLCN_ERR_ILLEGAL_OPERATION);
    }

    // Caller owns the mutex, release the mutex.
    CHECK_FLCN_STATUS(MUTEX_REG_WR32_HS(pEngineInfo, MUTEX_REG_ADDR(pEngineInfo, mutexPhyId),
        DRF_DEF(_FLCN, _MUTEX, _VALUE, _INITIAL_LOCK)));

    // Release the token and return success.
    CHECK_FLCN_STATUS(MUTEX_REG_RD32_HS(pEngineInfo, pEngineInfo->regAddrMutexIdRel, &data32));
    CHECK_FLCN_STATUS(MUTEX_REG_WR32_HS(pEngineInfo, pEngineInfo->regAddrMutexIdRel,
        FLD_SET_DRF_NUM(_FLCN, _MUTEX_ID_RELEASE, _VALUE, token, data32)));
ErrorExit:
    return flcnStatus;
}

#ifdef FPGA_BUILD
/*!
 * Verify if can skip boot mode check.
 */
static FLCN_STATUS
_mutexIsFpgaPlatform
(
    LwBool *pBIsFpga
)
{
    FLCN_STATUS flcnStatus = FLCN_ERR_ILLEGAL_OPERATION;
    LwU32       data32;
    LwU32       chip;

    CHECK_FLCN_STATUS(mutexRegRd32_hs(MUTEX_BUS_BAR0, LW_PMC_BOOT_42, &data32));

    *pBIsFpga = LW_FALSE;
    chip = DRF_VAL(_PMC, _BOOT_42, _CHIP_ID, data32);
    // FPGA doesn't support ACR and need bypass boot mode check.
    if (chip == LW_PMC_BOOT_42_CHIP_ID_GV100F)
    {
        *pBIsFpga = LW_TRUE;
    }

ErrorExit:
    return flcnStatus;
}
#endif
