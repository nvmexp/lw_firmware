/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2022 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file   shahw_hs_tux10x.c
 * @brief  HAL implementations for SHA HW functionality in HS on TU10X.
 *
 *         NOTE: If making ANY edits to this file, they should be mirrored
 *         in the LS implementation of these functions, as otherwise important
 *         bugfixes could be missed!
 */

/* ------------------------ System Includes -------------------------------- */
#include "lwctassert.h"

/* ------------------------ Application Includes --------------------------- */
#include "shahw.h"
#include "lib_csbshahw.h"
#include "osptimer.h"

/* ------------------------ Function Prototypes ---------------------------- */
static FLCN_STATUS _shahwCheckTaskSizeHs(SHA_ID algoId, LwU32 taskSize,
     LwU32 msgSize)
    GCC_ATTRIB_SECTION("imem_libShahwHs", "_shahwCheckTaskSizeHs");

static FLCN_STATUS _shahwSetShaInitVectorHs(SHA_ID algoId)
    GCC_ATTRIB_SECTION("imem_libShahwHs", "_shahwSetShaInitVectorHs");

static FLCN_STATUS _shahwGetConfigEncodeModeHs(SHA_ID algoId, LwU8 *pMode)
    GCC_ATTRIB_SECTION("imem_libShahwHs", "_shahwGetConfigEncodeModeHs");

/* ------------------------ Private Functions ------------------------------ */
/*!
 * @brief Helper function for task insertion. Checks for valid size of task to
 *        insert. Must be multiple of block size, unless last task, in which
 *        case it must be equal to remaining msgSize. Task size
 *        must also be nonzero, and less than maximum allowed task size.
 *
 * @param[in] algoId    Enum specifying SHA algorithm to be used for task.
 * @param[in] taskSize  Size of requested task, in bytes.
 * @param[in] msgSize   Remaining size of message, in bytes.
 *
 * @returns FLCN_STATUS  FLCN_OK if valid, FLCN_ERR_ILWALID_ARGUMENT otherwise.
 */
static FLCN_STATUS
_shahwCheckTaskSizeHs
(
    SHA_ID algoId,
    LwU32  taskSize,
    LwU32  msgSize
)
{
    LwBool bValid  = LW_FALSE;
    LwU32  maxSize = 0;

    //
    // SZ field of SHA_IN_ADDR_HI is used to store task size.
    // Ensure HW can support this task size.
    //
    maxSize = (LwU32)0xFFFFFFFF & DRF_MASK(CSB_DEF(_SHA_IN_ADDR_HI_SZ));
    if (taskSize == 0 || taskSize > maxSize)
    {
        return FLCN_ERR_ILWALID_ARGUMENT;
    }

    //
    // Check task size against message size - it can't be larger than message
    // size left. Otherwise, if equal to remaining message size, this must be
    // last task, and therefore doesn't need to be a multiple of block size.
    //
    if (taskSize > msgSize)
    {
        return FLCN_ERR_ILWALID_ARGUMENT;
    }
    else if (taskSize == msgSize)
    {
        return FLCN_OK;
    }

    // Ensure task size is a multiple of block size.
    switch (algoId)
    {
        // SHA 1, 224, and 256 have same block size.
        case SHA_ID_SHA_1:
        case SHA_ID_SHA_224:
        case SHA_ID_SHA_256:
        {
            bValid = ((taskSize % SHA_256_BLOCK_SIZE_BYTE) == 0);
            break;
        }

        // SHA 384, 512, 512_224, 512_256 have same block size.
        case SHA_ID_SHA_384:
        case SHA_ID_SHA_512:
        case SHA_ID_SHA_512_224:
        case SHA_ID_SHA_512_256:
        {
            bValid = ((taskSize % SHA_512_BLOCK_SIZE_BYTE) == 0);
            break;
        }

        default:
        {
            bValid = LW_FALSE;
            break;
        }
    }

    return bValid ? FLCN_OK : FLCN_ERR_ILWALID_ARGUMENT;
}

/*!
 * @brief Helper to set IV for SHA 512/224 and SHA 512/256,
 *        as SHA HW does not directly support it.
 *
 * @param[in] algoId  Which SHA algorithm's IV to set.
 * 
 * @returns FLCN_STATUS FLCN_OK if successful, relevant error otherwise.
 */
static FLCN_STATUS
_shahwSetShaInitVectorHs
(
    SHA_ID algoId
)
{
    //
    // To fill IV for SHA 512/224 and SHA 512/256, we directly write into
    // hash result registers in DWORD increments. Ensure that we will never
    // overwrite the number of registers available.
    //
    ct_assert(CSB_DEF(_SHA_HASH_RESULT__SIZE_1) >= SHA_512_224_IV_SIZE_DWORD);
    ct_assert(CSB_DEF(_SHA_HASH_RESULT__SIZE_1) >= SHA_512_256_IV_SIZE_DWORD);

    if (algoId == SHA_ID_SHA_512_224)
    {
        REG_WR32(CSB, CSB_REG(_SHA_HASH_RESULT(0)),  SHA_512_224_IV_DWORD_0);
        REG_WR32(CSB, CSB_REG(_SHA_HASH_RESULT(1)),  SHA_512_224_IV_DWORD_1);
        REG_WR32(CSB, CSB_REG(_SHA_HASH_RESULT(2)),  SHA_512_224_IV_DWORD_2);
        REG_WR32(CSB, CSB_REG(_SHA_HASH_RESULT(3)),  SHA_512_224_IV_DWORD_3);
        REG_WR32(CSB, CSB_REG(_SHA_HASH_RESULT(4)),  SHA_512_224_IV_DWORD_4);
        REG_WR32(CSB, CSB_REG(_SHA_HASH_RESULT(5)),  SHA_512_224_IV_DWORD_5);
        REG_WR32(CSB, CSB_REG(_SHA_HASH_RESULT(6)),  SHA_512_224_IV_DWORD_6);
        REG_WR32(CSB, CSB_REG(_SHA_HASH_RESULT(7)),  SHA_512_224_IV_DWORD_7);
        REG_WR32(CSB, CSB_REG(_SHA_HASH_RESULT(8)),  SHA_512_224_IV_DWORD_8);
        REG_WR32(CSB, CSB_REG(_SHA_HASH_RESULT(9)),  SHA_512_224_IV_DWORD_9);
        REG_WR32(CSB, CSB_REG(_SHA_HASH_RESULT(10)), SHA_512_224_IV_DWORD_10);
        REG_WR32(CSB, CSB_REG(_SHA_HASH_RESULT(11)), SHA_512_224_IV_DWORD_11);
        REG_WR32(CSB, CSB_REG(_SHA_HASH_RESULT(12)), SHA_512_224_IV_DWORD_12);
        REG_WR32(CSB, CSB_REG(_SHA_HASH_RESULT(13)), SHA_512_224_IV_DWORD_13);
        REG_WR32(CSB, CSB_REG(_SHA_HASH_RESULT(14)), SHA_512_224_IV_DWORD_14);
        REG_WR32(CSB, CSB_REG(_SHA_HASH_RESULT(15)), SHA_512_224_IV_DWORD_15);
    }
    else if (algoId == SHA_ID_SHA_512_256)
    {
        REG_WR32(CSB, CSB_REG(_SHA_HASH_RESULT(0)),  SHA_512_256_IV_DWORD_0);
        REG_WR32(CSB, CSB_REG(_SHA_HASH_RESULT(1)),  SHA_512_256_IV_DWORD_1);
        REG_WR32(CSB, CSB_REG(_SHA_HASH_RESULT(2)),  SHA_512_256_IV_DWORD_2);
        REG_WR32(CSB, CSB_REG(_SHA_HASH_RESULT(3)),  SHA_512_256_IV_DWORD_3);
        REG_WR32(CSB, CSB_REG(_SHA_HASH_RESULT(4)),  SHA_512_256_IV_DWORD_4);
        REG_WR32(CSB, CSB_REG(_SHA_HASH_RESULT(5)),  SHA_512_256_IV_DWORD_5);
        REG_WR32(CSB, CSB_REG(_SHA_HASH_RESULT(6)),  SHA_512_256_IV_DWORD_6);
        REG_WR32(CSB, CSB_REG(_SHA_HASH_RESULT(7)),  SHA_512_256_IV_DWORD_7);
        REG_WR32(CSB, CSB_REG(_SHA_HASH_RESULT(8)),  SHA_512_256_IV_DWORD_8);
        REG_WR32(CSB, CSB_REG(_SHA_HASH_RESULT(9)),  SHA_512_256_IV_DWORD_9);
        REG_WR32(CSB, CSB_REG(_SHA_HASH_RESULT(10)), SHA_512_256_IV_DWORD_10);
        REG_WR32(CSB, CSB_REG(_SHA_HASH_RESULT(11)), SHA_512_256_IV_DWORD_11);
        REG_WR32(CSB, CSB_REG(_SHA_HASH_RESULT(12)), SHA_512_256_IV_DWORD_12);
        REG_WR32(CSB, CSB_REG(_SHA_HASH_RESULT(13)), SHA_512_256_IV_DWORD_13);
        REG_WR32(CSB, CSB_REG(_SHA_HASH_RESULT(14)), SHA_512_256_IV_DWORD_14);
        REG_WR32(CSB, CSB_REG(_SHA_HASH_RESULT(15)), SHA_512_256_IV_DWORD_15);
    }
    else
    {
        // No need to manually set IV for other algorithms.
        return FLCN_ERR_ILWALID_ARGUMENT;
    }

    return FLCN_OK;
}

/*!
 * @brief Helper function to get encode mode for a specific SHA algorithm.
 *
 * @param[in]  algoId  Which SHA algorithm's encode mode to return.
 * @param[out] pMode   Pointer to LwU8 to hold encode mode of specified algorithm.
 *
 * @returns FLCN_STATUS  FLCN_OK if successful, relevant error otherwise.
 */
static FLCN_STATUS
_shahwGetConfigEncodeModeHs
(
    SHA_ID  algoId,
    LwU8   *pMode
)
{
    if (pMode == NULL)
    {
        return FLCN_ERR_ILWALID_ARGUMENT;
    }

    switch (algoId)
    {
        case SHA_ID_SHA_1:
        {
            *pMode = CSB_DEF(_SHA_CONFIG_ENC_MODE_SHA1);
            break;
        }
        case SHA_ID_SHA_224:
        {
            *pMode = CSB_DEF(_SHA_CONFIG_ENC_MODE_SHA224);
            break;
        }
        case SHA_ID_SHA_256:
        {
            *pMode = CSB_DEF(_SHA_CONFIG_ENC_MODE_SHA256);
            break;
        }
        case SHA_ID_SHA_384:
        {
            *pMode = CSB_DEF(_SHA_CONFIG_ENC_MODE_SHA384);
            break;
        }
        //
        // HW doesn't directly support SHA 512/224 and SHA 512/256,
        // so we treat as SHA 512 and support in SW.
        //
        case SHA_ID_SHA_512:
        case SHA_ID_SHA_512_224:
        case SHA_ID_SHA_512_256:
        {
            *pMode = CSB_DEF(_SHA_CONFIG_ENC_MODE_SHA512);
            break;
        }
        default:
        {
            return FLCN_ERR_ILWALID_ARGUMENT;
        }
    }

    return FLCN_OK;
}

/* ------------------------ Public Functions ------------------------------- */
/*!
 * @brief Acquire SHA HW engine mutex. HW does not provide protection other than
 *        maintaining mutex status - therefore, SW must respect mutex ownership.
 *
 *        Mutex must be held before any usage of SHA HW, besides where mentioned.
 *
 * @param[in] mutexId  ID value for new mutex, must be nonzero.
 *
 * @returns FLCN_STATUS  FLCN_OK if mutex acquired, otherwise mutex not acquired
 *                       and return value is relevant error.
 */
FLCN_STATUS
shahwAcquireEngineMutexHs_TU10X
(
    LwU8 mutexId
)
{
    LwU32 mutexStatus = 0;

    if (mutexId == 0)
    {
        return FLCN_ERR_ILWALID_ARGUMENT;
    }

    //
    // Check if existing mutex before write - checking before allows us
    // to avoid new mutexId matching existing one held by HW.
    //
    mutexStatus = REG_RD32(CSB, CSB_REG(_SHA_MUTEX_STATUS));
    if (!CSB_FLD_TEST_DRF(_FALCON_SHA_MUTEX_STATUS, _LOCKED, _INIT, mutexStatus))
    {
        return FLCN_ERR_MUTEX_ACQUIRED;
    }

    // Write new mutexId and ensure HW accepts.
    REG_WR32(CSB, CSB_REG(_SHA_MUTEX),
             CSB_DRF_NUM(_FALCON_SHA_MUTEX, _VAL, mutexId));
    mutexStatus = REG_RD32(CSB, CSB_REG(_SHA_MUTEX_STATUS));
    mutexStatus = CSB_DRF_VAL(_FALCON_SHA_MUTEX_STATUS, _LOCKED, mutexStatus);

    return (mutexStatus == mutexId ? FLCN_OK : FLCN_ERR_MUTEX_ACQUIRED);
}

/*!
 * @brief Releases the SHA HW engine mutex.
 *
 * @param[in] mutexId  ID value of mutex to release.
 *
 * @returns FLCN_STATUS  FLCN_OK if mutex released,
 *                       FLCN_ERR_ILWALID_ARGUMENT if invalid mutex ID,
 *                       FLCN_ERR_ILLEGAL_OPERATION if mutex release failed.
 */
FLCN_STATUS
shahwReleaseEngineMutexHs_TU10X
(
    LwU8 mutexId
)
{
    LwU32  mutexStatus = 0;
    LwBool bReleased   = LW_FALSE;

    if (mutexId == 0)
    {
        return FLCN_ERR_ILWALID_ARGUMENT;
    }

    //
    // Write mutexId to SHA_MUTEX_RELEASE_VAL, which will reset and release.
    // If mutexId is not same as lwrrently held mutex in HW, write will have
    // no effect, and current mutex in HW will remain locked.
    //
    REG_WR32(CSB, CSB_REG(_SHA_MUTEX_RELEASE),
             CSB_DRF_NUM(_FALCON_SHA_MUTEX_RELEASE, _VAL, mutexId));
    mutexStatus = REG_RD32(CSB, CSB_REG(_SHA_MUTEX_STATUS));
    bReleased = CSB_FLD_TEST_DRF(_FALCON_SHA_MUTEX_STATUS, _LOCKED, _INIT,
                                 mutexStatus);

    return bReleased ? FLCN_OK : FLCN_ERR_ILLEGAL_OPERATION;
}

/*!
 * @brief Resets SHA HW engine via SOFTRESET operation.
 *
 *        As SOFTRESET will release any mutex, this function must be called
 *        WITHOUT mutex held. It will temporarily acquire mutex itself before
 *        continuing with SOFTRESET. This also prevents it from running while
 *        someone else holds SHA mutex.
 *
 *        Ensures SHA HW is in known state first, waiting for any outlying
 *        commands. If SHA HW is in BUSY state, sends HALT command before
 *        sending SOFTRESET.
 *
 *        This can be used to forcibly terminate a long-running operation,
 *        or reset SHA HW to known clean state.
 *
 *        If this function fails, there is not much error recovery possible
 *        besides reattempting with longer timeout.
 *
 * @param[in] timeOut  Timeout value in ns to wait for engine to switch states.
 *
 * @returns FLCN_STATUS  FLCN_OK if engine soft resets successfully,
 *                       FLCN_ERR_MUTEX_ACQUIRED if mutex acquisition failed,
 *                       FLCN_ERR_SHA_HW_SOFTRESET_FAILED if reset was
 *                       unsuccessful for any reason.
 */
FLCN_STATUS
shahwEngineSoftResetHs_TU10X
(
    LwU32 timeOut
)
{
    FLCN_STATUS    flcnStatus   = FLCN_OK;
    LwBool         bPending     = LW_FALSE;
    LwU32          shaStatus    = 0;
    LwU32          shaOperation = 0;
    FLCN_TIMESTAMP startTimestamp;

    if ((flcnStatus = shahwAcquireEngineMutexHs_TU10X(SHA_MUTEX_ID_SHAHW_SOFTRESET))
                      != FLCN_OK)
    {
        // Fail here, as we don't have ownership of mutex.
        return flcnStatus;
    }

    // Wait for any outstanding commands.
    osPTimerTimeNsLwrrentGet_hs(&startTimestamp);
    do
    {
        shaOperation = REG_RD32(CSB, CSB_REG(_SHA_OPERATION));

        if (osPTimerTimeNsElapsedGet_hs(&startTimestamp) > timeOut)
        {
            flcnStatus = FLCN_ERR_SHA_HW_SOFTRESET_FAILED;
            goto ErrorExit;
        }

        bPending = CSB_FLD_TEST_DRF(_FALCON_SHA_OPERATION, _SOFTRESET,
                                    _ENABLE, shaOperation)             ||
                   CSB_FLD_TEST_DRF(_FALCON_SHA_OPERATION, _HALT,
                                    _ENABLE, shaOperation);
    } while (bPending);

    // Check if BUSY. If so, we need to send HALT first.
    shaStatus = REG_RD32(CSB, CSB_REG(_SHA_STATUS));
    if (CSB_FLD_TEST_DRF(_FALCON_SHA_STATUS, _STATE, _BUSY, shaStatus))
    {
        shaOperation = REG_RD32(CSB, CSB_REG(_SHA_OPERATION));
        shaOperation = CSB_FLD_SET_DRF(_FALCON_SHA_OPERATION, _HALT,
                                       _ENABLE, shaOperation);
        REG_WR32(CSB, CSB_REG(_SHA_OPERATION), shaOperation);

        // Wait for HALT command clear.
        osPTimerTimeNsLwrrentGet_hs(&startTimestamp);
        do
        {
            shaOperation = REG_RD32(CSB, CSB_REG(_SHA_OPERATION));

            if (osPTimerTimeNsElapsedGet_hs(&startTimestamp) > timeOut)
            {
                flcnStatus = FLCN_ERR_SHA_HW_SOFTRESET_FAILED;
                goto ErrorExit;
            }

            bPending = CSB_FLD_TEST_DRF(_FALCON_SHA_OPERATION, _HALT,
                                        _ENABLE, shaOperation);
        } while (bPending);

        shaStatus = REG_RD32(CSB, CSB_REG(_SHA_STATUS));
        if (!CSB_FLD_TEST_DRF(_FALCON_SHA_STATUS, _STATE, _HALTED, shaStatus))
        {
            // SHA HW failed to transition to HALTED. Nothing we can do.
            flcnStatus = FLCN_ERR_SHA_HW_SOFTRESET_FAILED;
            goto ErrorExit;
        }
    }

    // Send SOFTRESET command.
    shaOperation = REG_RD32(CSB, CSB_REG(_SHA_OPERATION));
    shaOperation = CSB_FLD_SET_DRF(_FALCON_SHA_OPERATION, _SOFTRESET,
                                   _ENABLE, shaOperation);
    REG_WR32(CSB, CSB_REG(_SHA_OPERATION), shaOperation);

    // Wait for SOFTRESET command clear.
    osPTimerTimeNsLwrrentGet_hs(&startTimestamp);
    do
    {
        shaOperation = REG_RD32(CSB, CSB_REG(_SHA_OPERATION));

        if (osPTimerTimeNsElapsedGet_hs(&startTimestamp) > timeOut)
        {
            flcnStatus = FLCN_ERR_SHA_HW_SOFTRESET_FAILED;
            goto ErrorExit;
        }

        bPending = CSB_FLD_TEST_DRF(_FALCON_SHA_OPERATION, _SOFTRESET,
                                    _ENABLE, shaOperation);
    } while (bPending);

    // Ensure engine hit IDLE state.
    shaStatus = REG_RD32(CSB, CSB_REG(_SHA_STATUS));

    flcnStatus = CSB_FLD_TEST_DRF(_FALCON_SHA_STATUS, _STATE, _IDLE, shaStatus) ?
                 FLCN_OK : FLCN_ERR_SHA_HW_SOFTRESET_FAILED;

ErrorExit:

    if (flcnStatus != FLCN_OK)
    {
        // If reset wasn't succesful, we still hold mutex. Best effort to release.
        (void)shahwReleaseEngineMutexHs_TU10X(SHA_MUTEX_ID_SHAHW_SOFTRESET);
    }

    return flcnStatus;
}

/*!
 * @brief Wait for engine to return from busy state.
  *
 * @param[in] timeOut  Timeout value in ns to wait for engine to be idle
 *
 * @returns FLCN_STATUS  FLCN_OK if engine idle, FLCN_ERR_TIMEOUT if idle
 *                       not reached, relevant error otherwise
 */
FLCN_STATUS
shahwWaitEngineIdleHs_TU10X
(
    LwU32 timeOut
)
{
    LwU32          shaStatus = 0;
    FLCN_TIMESTAMP startTimestamp;

    osPTimerTimeNsLwrrentGet_hs(&startTimestamp);

    // Wait for engine to hit idle state.
    do
    {
        shaStatus = REG_RD32(CSB, CSB_REG(_SHA_STATUS));

        if (CSB_FLD_TEST_DRF(_FALCON_SHA_STATUS, _STATE, _HALTED, shaStatus))
        {
            // Unexpected, only recoverable via SOFTRESET.
            return FLCN_ERR_SHA_HW_SOFTRESET_REQUIRED;
        }

        if (osPTimerTimeNsElapsedGet_hs(&startTimestamp) > timeOut)
        {
            return FLCN_ERR_TIMEOUT;
        }

    } while (CSB_FLD_TEST_DRF(_FALCON_SHA_STATUS, _STATE, _BUSY, shaStatus));

    // Ensure we have reached IDLE state, otherwise unexpected error.
    return CSB_FLD_TEST_DRF(_FALCON_SHA_STATUS, _STATE, _IDLE, shaStatus) ?
           FLCN_OK : FLCN_ERR_SHA_HW_SOFTRESET_REQUIRED;
}

/*!
 * @brief Initialize SHA engine for session. Requires SHA_ID specifying
 *        SHA algorithm to use, along with total number of bytes that
 *        will be used for this SHA session.
 *
 *        After acquiring SHA HW mutex, this should be called first to
 *        setup context for SHA session and ensure engine is in good state.
 *
 * @param[in] algoId   Enum value specifying SHA algorithm to be used.
 * @param[in] msgSize  Total number of bytes to be added to digest.
 *
 * @returns FLCN_STATUS  FLCN_OK if success, relevant error code on failure.
 */
FLCN_STATUS
shahwInitializeEngineHs_TU10X
(
    SHA_ID algoId,
    LwU32  msgSize
)
{
    FLCN_STATUS flcnStatus = FLCN_OK;
    LwU32       i          = 0;
    LwU32       engStatus  = 0;
    LwU32       config     = 0;
    LwU8        encMode    = 0;

    //
    // Engine stores length in bits, ensure user length will not overflow.
    // Otherwise, ensure valid length.
    //
    if ((SHAHW_CHECK_BYTES_TO_BITS_OVERFLOW(msgSize) != 0) || (msgSize == 0))
    {
        return FLCN_ERR_ILWALID_ARGUMENT;
    }

    // Check engine state before continuing
    engStatus = REG_RD32(CSB, CSB_REG(_SHA_STATUS));
    if (CSB_FLD_TEST_DRF(_FALCON_SHA_STATUS, _STATE, _BUSY, engStatus))
    {
        //
        // If engine is busy, most likely someone else using SHA without mutex.
        // Leave it to user to determine whether to SOFTRESET or wait.
        //
        return FLCN_ERR_SHA_HW_BUSY;
    }
    else if (CSB_FLD_TEST_DRF(_FALCON_SHA_STATUS, _STATE, _HALTED, engStatus))
    {
        //
        // SHA HW requires SOFTRESET. However, exelwting SOFTRESET wil release
        // SHA mutex. Therefore, we error out and let user handle.
        //
        return FLCN_ERR_SHA_HW_SOFTRESET_REQUIRED;
    }

    // Set SHA config register to requested algorithm.
    CHECK_FLCN_STATUS(_shahwGetConfigEncodeModeHs(algoId, &encMode));
    config = CSB_DRF_NUM(_FALCON_SHA_CONFIG, _ENC_MODE, encMode);
    config = CSB_FLD_SET_DRF(_FALCON_SHA_CONFIG, _ENC_ALG, _SHA, config);
    config = CSB_FLD_SET_DRF(_FALCON_SHA_CONFIG, _DST, _HASH_REG, config);
    REG_WR32(CSB, CSB_REG(_SHA_CONFIG), config);

    //
    // SHA has 4 dword registers to save message length in bits.
    // For now, only support message size of < 2^32 bits (.5 GB).
    // Clear all registers, then write message size.
    //
    for (i = 0; i < CSB_DEF(_SHA_MSG_LENGTH__SIZE_1); i++)
    {
        REG_WR32(CSB, CSB_REG(_SHA_MSG_LENGTH(i)), 0);
    }

    // Make sure to write message size in bits.
    REG_WR32(CSB, CSB_REG(_SHA_MSG_LENGTH(0)), SHAHW_BYTES_TO_BITS(msgSize));

ErrorExit:

    return flcnStatus;
}

/*!
 * @brief Insert task to add data to SHA digest.
 *
 *        This function may block for completion, depending on parameter. If
 *        not, use shahwWaitTaskCompleteHs_TU10X to wait for completion and check
 *        for success. Ensure data will be valid until task completion.
 *
 *        Note that for FB source, SHA HW cannot read buffer that is contiguous
 *        across 40bit address boundary.
 *
 * @param[in]  algoId      Enum value specifying SHA algorithm to use.
 * @param[in]  taskSize    Number of bytes to add to digest, must be multiple
 *                         of block size, unless last message - should never
 *                         exceed remaining message size.
 * @param[in]  pMsgSize    Pointer to integer which holds total number of bytes
 *                         remaining in message.
 * @param[in]  bDefaultIv  Whether to use default IV or use previous hash as IV.
 * @param[in]  addr        Address of input data - expects VA if DMEM/IMEM,
 *                         if FB address, aperture set via dmaIdx parameter.
 * @param[in]  srcType     Enum value specifying memory location of input.
 * @param[in]  dmaIdx      If FB source, which DMA aperture index to use.
 * @param[out] pIntStatus  Pointer to integer to hold any SHA HW int status.
 * @param[in]  bWait       Whether the function should wait for the operation
 *                         to complete before returning.
 *
 * @returns FLCN_STATUS  FLCN_OK if task inserted successfully,
 *                       relevant error otherwise.
 */
FLCN_STATUS
shahwInsertTaskHs_TU10X
(
    SHA_ID        algoId,
    LwU32         taskSize,
    LwU32        *pMsgSize,
    LwBool        bDefaultIv,
    LwU64         addr,
    SHA_SRC_TYPE  srcType,
    LwU32         dmaIdx,
    LwU32        *pIntStatus,
    LwBool        bWait
)
{
    FLCN_STATUS flcnStatus   = FLCN_OK;
    LwU32       srcConfig    = 0;
    LwU32       inAddrHi     = 0;
    LwU32       msbShift     = 0;
    LwU32       fbMsb        = 0;
    LwU32       shaTaskCfg   = 0;
    LwU32       shaOperation = 0;
    LwU32       i            = 0;

    if (pMsgSize == NULL || pIntStatus == NULL || addr == 0)
    {
        return FLCN_ERR_ILWALID_ARGUMENT;
    }

    // Ensure task has valid size before waiting on any previous task.
    CHECK_FLCN_STATUS(_shahwCheckTaskSizeHs(algoId, taskSize, *pMsgSize));
    CHECK_FLCN_STATUS(shahwWaitTaskCompleteHs_TU10X(pIntStatus));

    // Reset registers holding message bits remaining, then set manually.
    for (i = 0; i < CSB_DEF(_SHA_MSG_LEFT__SIZE_1); i++)
    {
        REG_WR32(CSB, CSB_REG(_SHA_MSG_LEFT(i)), 0);
    }

    // Callwlate and update message bits remaining.
    REG_WR32(CSB, CSB_REG(_SHA_MSG_LEFT(0)), SHAHW_BYTES_TO_BITS(*pMsgSize));
    *pMsgSize = *pMsgSize - taskSize;

    //
    // Callwlate and set the input address, low 32-bits go in SHA_IN_ADDR.
    // SHA_IN_ADDR_HI includes upper bits of address, as well as size of task.
    //
    REG_WR32(CSB, CSB_REG(_SHA_IN_ADDR), (LwU32)LwU64_LO32(addr));
    inAddrHi = CSB_DRF_NUM(_FALCON_SHA_IN_ADDR_HI, _MSB, (LwU32)LwU64_HI32(addr)) |
               CSB_DRF_NUM(_FALCON_SHA_IN_ADDR_HI, _SZ,  taskSize);
    REG_WR32(CSB, CSB_REG(_SHA_IN_ADDR_HI), inAddrHi);

    //
    // Setup SHA_CONFIG register - sets memory source (DMEM/IMEM/FB).
    // Stores DMA aperture index to use if FB address. Also stores MSB
    // not covered by SHA_IN_ADDR_HI_MSB in SHA_SRC_CONFIG_FB_BASE,
    // used if FB address is 49-bits.
    //
    msbShift = DRF_SIZE(CSB_DEF(_SHA_IN_ADDR_HI_MSB));
    fbMsb    = ((LwU32)LwU64_HI32(addr)) >> msbShift;

    srcConfig = CSB_DRF_NUM(_FALCON_SHA_SRC_CONFIG, _FB_BASE, fbMsb)  |
                CSB_DRF_NUM(_FALCON_SHA_SRC_CONFIG, _CTXDMA,  dmaIdx) |
                CSB_DRF_NUM(_FALCON_SHA_SRC_CONFIG, _SRC,     srcType);
    REG_WR32(CSB, CSB_REG(_SHA_SRC_CONFIG), srcConfig);

    // Tell HW whether to use previous digest as IV, or use default IV.
    if (bDefaultIv)
    {
        // SW needs to set initial vector for SHA 512/224 & SHA 512/256.
        if (algoId == SHA_ID_SHA_512_224 || algoId == SHA_ID_SHA_512_256)
        {
            CHECK_FLCN_STATUS(_shahwSetShaInitVectorHs(algoId));
            shaTaskCfg = CSB_DRF_DEF(_FALCON_SHA_TASK_CONFIG, _HW_INIT_HASH, _DISABLE);
        }
        else
        {
            shaTaskCfg = CSB_DRF_DEF(_FALCON_SHA_TASK_CONFIG, _HW_INIT_HASH, _ENABLE);
        }
    }
    else
    {
        shaTaskCfg = CSB_DRF_DEF(_FALCON_SHA_TASK_CONFIG, _HW_INIT_HASH, _DISABLE);
    }
    REG_WR32(CSB, CSB_REG(_SHA_TASK_CONFIG), shaTaskCfg);

    // Trigger operation start.
    shaOperation = REG_RD32(CSB, CSB_REG(_SHA_OPERATION));
    shaOperation = CSB_FLD_SET_DRF(_FALCON_SHA_OPERATION, _LAST_BUF, _TRUE,
                                   shaOperation);
    shaOperation = CSB_FLD_SET_DRF(_FALCON_SHA_OPERATION, _OP, _START,
                                   shaOperation);
    REG_WR32(CSB, CSB_REG(_SHA_OPERATION), shaOperation);

    // Optionally wait for completion.
    if (bWait)
    {
        CHECK_FLCN_STATUS(shahwWaitTaskCompleteHs_TU10X(pIntStatus));
    }

ErrorExit:

    return flcnStatus;
}

/*!
 * @brief Checks int status registers to check if operation complete,
 *        or if error asserted. If so, get reason for error. Upon reading
 *        statuses, writes back any high values to clear.
 * 
 * @param[out] pStatus  Pointer to enum value which will hold success status
 *                      or reason for failure.
 *
 * @returns FLCN_STATUS  FLCN_OK if pStatus is updated and int status cleared,
 *                       relevant error otherwise.
 */
FLCN_STATUS
shahwCheckAndClearIntStatusHs_TU10X
(
    SHA_INT_STATUS *pStatus
)
{
    LwU32 intStatus = 0;
    LwU32 errStatus = 0;

    if (pStatus == NULL)
    {
        return FLCN_ERR_ILWALID_ARGUMENT;
    }

    *pStatus = SHA_INT_STATUS_NONE;

    intStatus = REG_RD32(CSB, CSB_REG(_SHA_INT_STATUS));

    // If error asserted, get specific reason.
    if (CSB_FLD_TEST_DRF(_FALCON_SHA_INT_STATUS, _ERR_STATUS, _TRUE, intStatus))
    {
        errStatus = REG_RD32(CSB, CSB_REG(_SHA_ERR_STATUS));

        if (CSB_FLD_TEST_DRF(_FALCON_SHA_ERR_STATUS, _LEVEL_ERR, _TRUE,
                             errStatus))
        {
            *pStatus = SHA_INT_STATUS_ERR_LEVEL_ERROR;
        }
        else if (CSB_FLD_TEST_DRF(_FALCON_SHA_ERR_STATUS, _MULTI_HIT, _TRUE,
                                  errStatus))
        {
            *pStatus = SHA_INT_STATUS_ERR_LEVEL_MULTI_HIT;
        }
        else if (CSB_FLD_TEST_DRF(_FALCON_SHA_ERR_STATUS, _MISS, _TRUE,
                                  errStatus))
        {
            *pStatus = SHA_INT_STATUS_ERR_STATUS_MISS;
        }
        else if (CSB_FLD_TEST_DRF(_FALCON_SHA_ERR_STATUS, _PA_FAULT, _TRUE,
                                  errStatus))
        {
            *pStatus = SHA_INT_STATUS_ERR_PA_FAULT;
        }
        else if (CSB_FLD_TEST_DRF(_FALCON_SHA_ERR_STATUS, _FBIF_ERR, _TRUE,
                                  errStatus))
        {
            *pStatus = SHA_INT_STATUS_ERR_FBIF_ERROR;
        }
        else if (CSB_FLD_TEST_DRF(_FALCON_SHA_ERR_STATUS, _ILLEGAL_CFG, _TRUE,
                                  errStatus))
        {
            *pStatus = SHA_INT_STATUS_ERR_ILLEGAL_CFG;
        }
        else if (CSB_FLD_TEST_DRF(_FALCON_SHA_ERR_STATUS, _NS_ACCESS, _TRUE,
                                  errStatus))
        {
            *pStatus = SHA_INT_STATUS_ERR_NS_ACCESS;
        }
        else
        {
            *pStatus = SHA_INT_STATUS_UNKNOWN;
        }

        // Write back to clear.
        REG_WR32(CSB, CSB_REG(_SHA_ERR_STATUS), errStatus);
        REG_WR32(CSB, CSB_REG(_SHA_INT_STATUS), intStatus);
    }
    // Otherwise, check if operation complete.
    else if (CSB_FLD_TEST_DRF(_FALCON_SHA_INT_STATUS, _OP_DONE, _TRUE, intStatus))
    {
        *pStatus = SHA_INT_STATUS_OP_DONE;

        // Write back to clear.
        REG_WR32(CSB, CSB_REG(_SHA_INT_STATUS), intStatus);
    }

    return FLCN_OK;
}

/*!
 * @brief Waits for engine idle, and ensures any previous operation had
 *        completed succesfully.
 *
 * @param[out] pIntStatus  Pointer to enum value which holds success status
 *                         or reason for failure.
 *
 * @returns FLCN_STATUS  FLCN_OK if engine idle and no errors,
 *                       FLCN_ERR_SHA_HW_CHECK_INT_STATUS if error status
 *                       found with extended info available via pIntStatus,
 *                       relevant error otherwise.
 */
FLCN_STATUS
shahwWaitTaskCompleteHs_TU10X
(
    SHA_INT_STATUS *pIntStatus
)
{
    FLCN_STATUS flcnStatus = FLCN_OK;

    if (pIntStatus == NULL)
    {
        return FLCN_ERR_ILWALID_ARGUMENT;
    }

    CHECK_FLCN_STATUS(shahwWaitEngineIdleHs_TU10X(SHAHW_ENGINE_IDLE_TIMEOUT_NS));

    //
    // Check status, returning success if task completed without error,
    // or if no previous task has been exelwted.
    //
    CHECK_FLCN_STATUS(shahwCheckAndClearIntStatusHs_TU10X(pIntStatus));
    if (*pIntStatus == SHA_INT_STATUS_NONE || *pIntStatus == SHA_INT_STATUS_OP_DONE)
    {
        flcnStatus = FLCN_OK;
    }
    else
    {
        flcnStatus = FLCN_ERR_SHA_HW_CHECK_INT_STATUS;
    }

ErrorExit:

    return flcnStatus;
}

/*!
 * @brief Get digest of SHA operation from hash result registers.
 *
 * @param[in]  algoId       Enum value specifying SHA algorithm used.
 * @param[out] pDigest      Destination buffer to hold digest, buffer should
 *                          be of size SHA_*_HASH_SIZE_BYTES, based on algoId.
 * @param[in]  bScrubReg    Should result registers be cleared after reading?
 * @param[out] pIntStatus   Pointer to integer to hold any SHA HW int status.
 *
 * @returns FLCN_STATUS  FLCN_OK on success, relevant error otherwise.
 */
FLCN_STATUS
shahwReadHashResultHs_TU10X
(
    SHA_ID  algoId,
    LwU32  *pDigest,
    LwBool  bScrubReg,
    LwU32  *pIntStatus
)
{
    LwU32  i             = 0;
    LwU32  regIndex      = 0;
    LwU32  v1            = 0;
    LwU32  v2            = 0;
    LwU32 *pSrc1         = (LwU32 *)&v1;
    LwU32 *pSrc2         = (LwU32 *)&v2;
    LwU32  loopCount     = 0;
    LwBool bWordSize64   = LW_FALSE;
    LwBool bLastWordHalf = LW_FALSE;
    LwU32  flcnStatus    = FLCN_OK;

    if (pDigest == NULL || pIntStatus == NULL)
    {
        return FLCN_ERR_ILWALID_ARGUMENT;
    }

    // Ensure no callwlations are lwrrently ongoing.
    CHECK_FLCN_STATUS(shahwWaitTaskCompleteHs_TU10X(pIntStatus));

    //
    // Callwlate number of words we need to read. Word size is dword
    // up to SHA 256, and double dword for SHA size > 256.
    //
    switch (algoId)
    {
        case SHA_ID_SHA_1:
        {
            loopCount = SHA_1_HASH_SIZE_DWORD;
            break;
        }
        case SHA_ID_SHA_224:
        {
            loopCount = SHA_224_HASH_SIZE_DWORD;
            break;
        }
        case SHA_ID_SHA_256:
        {
            loopCount = SHA_256_HASH_SIZE_DWORD;
            break;
        }
        case SHA_ID_SHA_384:
        {
            loopCount   = SHA_384_HASH_SIZE_DWORD / 2;
            bWordSize64 = LW_TRUE;
            break;
        }
        case SHA_ID_SHA_512:
        {
            loopCount   = SHA_512_HASH_SIZE_DWORD / 2;
            bWordSize64 = LW_TRUE;
            break;
        }
        case SHA_ID_SHA_512_224:
        {
            // Include extra loop, as we need to read half word in final round.
            loopCount     = (SHA_512_224_HASH_SIZE_DWORD / 2) + 1;
            bWordSize64   = LW_TRUE;
            bLastWordHalf = LW_TRUE;
            break;
        }
        case SHA_ID_SHA_512_256:
        {
            loopCount   = SHA_512_256_HASH_SIZE_DWORD / 2;
            bWordSize64 = LW_TRUE;
            break;
        }
        default:
        {
            return FLCN_ERR_ILWALID_ARGUMENT;
        }
    }

    // Read all result words.
    for (i = 0; i < loopCount; i++)
    {
        v1 = REG_RD32(CSB, CSB_REG(_SHA_HASH_RESULT(regIndex)));

        if (bScrubReg)
        {
            REG_WR32(CSB, CSB_REG(_SHA_HASH_RESULT(regIndex)), 0);
        }
        regIndex++;

        //
        // SHA engine saves hash data with big-endian format,
        // we need to transfer to little-endian format.
        //
        v1 = SHAHW_REVERT_ENDIAN_DWORD(v1);

        if (bWordSize64)
        {
            v2 = REG_RD32(CSB, CSB_REG(_SHA_HASH_RESULT(regIndex)));

            if (bScrubReg)
            {
                REG_WR32(CSB, CSB_REG(_SHA_HASH_RESULT(regIndex)), 0);
            }
            regIndex++;

            v2 = SHAHW_REVERT_ENDIAN_DWORD(v2);

            // Store v2 first, as full word was stored big-endian.
            *pDigest = *pSrc2;
            pDigest++;

            // In case of SHA 512/224, we only read half word in final round.
            if (bLastWordHalf && (i == loopCount - 1))
            {
                break;
            }
        }

        // Store v1 in result buffer.
        *pDigest = *pSrc1;
        pDigest++;
    }

ErrorExit:

    return flcnStatus;
}
