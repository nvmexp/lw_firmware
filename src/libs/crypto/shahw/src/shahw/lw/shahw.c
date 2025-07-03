/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2021-2022 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file   shahw.c
 * @brief  Implementations of SHA & HMAC utilizing SHA HW.
 *
 *         NOTE: If making ANY edits to this file, they should be mirrored
 *         in the HS implementation of these functions, as otherwise important
 *         bugfixes could be missed!
 */

/* ------------------------ Application Includes --------------------------- */
#include "shahw.h"

/* ------------------------ Public Functions ------------------------------- */
/*!
 * @brief Function to reset SHA HW to ensure it is in good state. Can be called
 *        before initial usage of engine, or after error is received - like
 *        FLCN_ERR_SHA_HW_SOFTRESET_REQUIRED.
 * 
 *        Note that this function cannot be called when the SHA mutex is held. 
 *        It will temporarily acquire the mutex for exclusivity during reset.
 * 
 *        If function fails due to mutex acquisition, it means SHA HW is in use
 *        by other client. If reset fails for any other reason, there is no
 *        recovery possible.
 * 
 * @returns FLCN_STATUS  FLCN_OK if SHA HW reset successfully,
 *                       FLCN_ERR_MUTEX_ACQUIRED if someone is holding the mutex,
 *                       FLCN_ERR_SHA_HW_SOFTRESET_FAILED if reset operation fails.
 */
FLCN_STATUS
shahwSoftReset(void)
{
    return shahwEngineSoftReset_HAL(&shahw, SHAHW_ENGINE_SW_RESET_TIMEOUT_NS);
}

/*!
 * @brief Function to acquire SHA HW mutex. User MUST acquire mutex
 *        before interacting with SHA HW, besides SoftReset. User provides
 *        unique number to identify ownership of mutex.
 *
 *        Acquires mutex on success. Mutex MUST later be released via
 *        call to shahwReleaseMutex().
 *
 * @param[in] mutexId  ID to specify mutex ownership, must be nonzero.
 * 
 * @returns FLCN_STATUS  FLCN_OK if mutex acquired successfully,
 *                       relevant error otherwise.
 */
FLCN_STATUS
shahwAcquireMutex
(
    LwU8 mutexId
)
{
    return shahwAcquireEngineMutex_HAL(&shahw, mutexId);
}

/*!
 * @brief Function to release SHA HW mutex. User MUST release mutex
 *        once done using SHA HW. User should provide same mutex ID used for
 *        mutex acquisition.
 * 
 *        Releases SHA mutex on success. Mutex is retained on failure.
 *
 * @param[in] mutexId  ID of lwrrently owned mutex.
 * 
 * @returns FLCN_STATUS  FLCN_OK if mutex released successfully,
 *                       relevant error otherwise.
 */
FLCN_STATUS
shahwReleaseMutex
(
    LwU8 mutexId
)
{
    return shahwReleaseEngineMutex_HAL(&shahw, mutexId);
}

/*!
 * @brief Initializes SHA HW and prepares for session.
 *
 *        User is responsible for initializing SHA_CONTEXT structure fields
 *        with relevant parameters for SHA HW session before calling.
 * 
 *        User must obtain SHA HW mutex before calling.
 *
 * @param[in] pShaContext  Pointer to SHA_CONTEXT struct containing parameters
 *                         for SHA HW operation.
 *
 * @returns FLCN_STATUS  FLCN_OK if succesful,
 *                       FLCN_ERR_SHA_HW_BUSY if SHA HW is lwrrently in use,
 *                       FLCN_ERR_SHA_HW_SOFTRESET_REQUIRED if SHA HW is HALTED,
 *                       relevant error otherwise.
 */
FLCN_STATUS
shahwOpInit
(
    SHA_CONTEXT *pShaContext
)
{
    FLCN_STATUS flcnStatus = FLCN_OK;

    if (pShaContext == NULL)
    {
        return FLCN_ERR_ILWALID_ARGUMENT;
    }

    // HAL function should provide parameter validation.
    CHECK_FLCN_STATUS(shahwInitializeEngine_HAL(&shahw, pShaContext->algoId,
                                                pShaContext->msgSize));

ErrorExit:

    return flcnStatus;
}

/*!
 * @brief Function to update SHA running digest with new data. Can be called
 *        multiple times to add new data to digest.
 *
 *        Must call shahwOpInit() first to initialize SHA HW, and retain ownership
 *        of SHA HW mutex. User can call shahwOpFinal() to obtain digest.
 *
 *        Recommend waiting for completion unless necessary. If not waiting,
 *        completion can be polled via shahwWaitTaskComplete. Ensure input data
 *        will be valid until task has completed.
 * 
 *        Function will wait for any outstanding tasks before inserting a new one.
 *
 * @param[in]  pShaContext  Pointer to SHA_CONTEXT struct.
 * @param[in]  size         Number of bytes to add to digest, must be multiple
 *                          of block size, unless last message - should never
 *                          exceed remaining msgSize in SHA_CONTEXT.
 * @param[in]  bDefaultIv   Whether to use default IV or use previous hash.
 * @param[in]  addr         Address of input data - expects VA if DMEM/IMEM,
 *                          if FB address, aperture set via dmaIdx parameter.
 * @param[in]  srcType      Enum value specifying memory location of input.
 * @param[in]  dmaIdx       If FB source, which DMA aperture index to use.
 * @param[out] pIntStatus   Pointer to integer to hold any SHA HW int status.
 * @param[in]  bWait        Whether the function should wait for the operation
 *                          to complete before returning.
 *
 * @returns FLCN_STATUS  FLCN_OK if succesful,
 *                       FLCN_ERR_SHA_HW_CHECK_INT_STATUS if error where
 *                       pIntStatus should be checked, relevant error otherwise.
 */
FLCN_STATUS
shahwOpUpdate
(
    SHA_CONTEXT  *pShaContext,
    LwU32         size,
    LwBool        bDefaultIv,
    LwU64         addr,
    SHA_SRC_TYPE  srcType,
    LwU32         dmaIdx,
    LwU32        *pIntStatus,
    LwBool        bWait
)
{
    // Basic parameter check, HAL should check the rest.
    if (pShaContext == NULL)
    {
        return FLCN_ERR_ILWALID_ARGUMENT;
    }

    return shahwInsertTask_HAL(&shahw, pShaContext->algoId, size,
                               &(pShaContext->msgSize), bDefaultIv,
                               addr, srcType, dmaIdx, pIntStatus, bWait);
}

/*!
 * @brief Finalizes SHA operation and retrieves result into pDigest. Will wait
 *        for any outlying operations before retrieving result.
 *
 *        Function requires user to still be holding SHA HW mutex.        
 *        Once finished, user is responsible for releasing mutex
 *        via call to shahwReleaseMutex().
 *
 * @param[in]  pShaContext  Pointer to SHA_CONTEXT struct.
 * @param[out] pDigest      Pointer to buffer to hold digest, buffer should be
 *                          of size SHA_*_HASH_SIZE_DWORD, based on algorithm.
 * @param[in]  bScrubReg    Whether to scrub digest output registers.
 * @param[out] pIntStatus   Pointer to integer to hold any SHA HW int status.
 * 
 * @returns FLCN_STATUS  FLCN_OK if succesful,
 *                       FLCN_ERR_SHA_HW_CHECK_INT_STATUS if error where
 *                       pIntStatus should be checked, relevant error otherwise.
 */
FLCN_STATUS
shahwOpFinal
(
    SHA_CONTEXT *pShaContext,
    LwU32       *pDigest,
    LwBool       bScrubReg,
    LwU32       *pIntStatus
)
{
    FLCN_STATUS flcnStatus = FLCN_OK;

    // Basic parameter check, HAL should check the rest.
    if (pShaContext == NULL)
    {
        return FLCN_ERR_ILWALID_ARGUMENT;
    }

    // Read result from registers.
    CHECK_FLCN_STATUS(shahwReadHashResult_HAL(&shahw, pShaContext->algoId,
                                              pDigest, bScrubReg, pIntStatus));

ErrorExit:

    return flcnStatus;
}

/*!
 * @brief Initializes SHA HW for HMAC SHA 256 operation. Performs hash of
 *        inner key, message can then be appended with calls to
 *        shahwHmacSha256Update().
 *
 *        User is responsible for initializing HMAC_CONTEXT structure before
 *        calling function.
 * 
 *        User must acquire SHA HW mutex before use.
 *
 * @param[in]  pHmacContext  Pointer to HMAC_CONTEXT struct with parameters
 *                           for HMAC operation.
 * @param[out] pIntStatus    Pointer to integer to hold any SHA HW int status.
 * 
 * @returns FLCN_STATUS  FLCN_OK if succesful,
 *                       FLCN_ERR_SHA_HW_BUSY if SHA HW is in BUSY state,
 *                       FLCN_ERR_SHA_HW_SOFTRESET_REQUIRED if SHA HW is HALTED,
 *                       FLCN_ERR_SHA_HW_CHECK_INT_STATUS if error where
 *                       pIntStatus should be checked, relevant error otherwise.
 */
FLCN_STATUS
shahwHmacSha256Init
(
    HMAC_CONTEXT *pHmacContext,
    LwU32        *pIntStatus
)
{
    FLCN_STATUS flcnStatus = FLCN_OK;
    LwU32       msgSize    = 0;
    LwU32       i          = 0;
    LwU8        ipad[SHA_256_BLOCK_SIZE_BYTE];

    // Ensure context is initialized and valid.
    if (pHmacContext == NULL || pHmacContext->msgSize == 0 ||
        pHmacContext->algoId != SHA_ID_SHA_256             ||
        pHmacContext->keySize > SHA_256_BLOCK_SIZE_BYTE    ||
        pHmacContext->keySize == 0)
    {
        return FLCN_ERR_ILWALID_ARGUMENT;
    }

    // Initialize first SHA session - first hash will be H(inner key || message).
    msgSize = pHmacContext->msgSize + SHA_256_BLOCK_SIZE_BYTE;
    CHECK_FLCN_STATUS(shahwInitializeEngine_HAL(&shahw, pHmacContext->algoId,
                                                msgSize));

    // Generate inner key.
    memset(ipad, 0, SHA_256_BLOCK_SIZE_BYTE);
    memcpy(ipad, &(pHmacContext->keyBuffer), pHmacContext->keySize);
    for (i = 0; i < SHA_256_BLOCK_SIZE_BYTE; i++)
    {
       ipad[i] = ipad[i] ^ HMAC_SHA_256_IPAD_MASK;
    }

    // Add inner key to hash.
    CHECK_FLCN_STATUS(shahwInsertTask_HAL(&shahw, pHmacContext->algoId,
                                          SHA_256_BLOCK_SIZE_BYTE, &msgSize,
                                          LW_TRUE, (LwU64)(LwU32)ipad,
                                          SHA_SRC_TYPE_DMEM, 0, pIntStatus,
                                          LW_TRUE));

ErrorExit:

    return flcnStatus;
}

/*!
 * @brief Wrapper around shahwInsertTask_HAL() to update inner digest in
 *        HMAC SHA 256. Functions very similar to shahwOpUpdate, see its
 *        description for more info. Note that it is assumed to always
 *        use previous digest as IV.
 * 
 *        As above, requires SHA HW mutex to still be held.
 */
FLCN_STATUS
shahwHmacSha256Update
(
    HMAC_CONTEXT *pHmacContext,
    LwU32         size,
    LwU64         addr,
    SHA_SRC_TYPE  srcType,
    LwU32         dmaIdx,
    LwU32        *pIntStatus,
    LwBool        bWait
)
{
    //
    // Basic parameter validation, make sure context still valid.
    // Task parameters are validated in HAL.
    //
    if (pHmacContext == NULL || pHmacContext->algoId != SHA_ID_SHA_256)
    {
        return FLCN_ERR_ILWALID_ARGUMENT;
    }

    return shahwInsertTask_HAL(&shahw, pHmacContext->algoId, size,
                               &(pHmacContext->msgSize), LW_FALSE,
                               addr, srcType, dmaIdx, pIntStatus, bWait);
}

/*!
 * @brief Finalizes HMAC SHA 256 operation and retrieves resulting HMAC
 *        into pHmac.
 * 
 *        Function expects user to still be holding SHA HW mutex.
 *        User is responsible for releasing mutex once finished.
 *
 * @param[in]  pHmacContext  Pointer to HMAC_CONTEXT struct.
 * @param[out] pHmac         Pointer to buffer to hold HMAC, buffer must be
 *                           of size SHA_256_HASH_SIZE_DWORD.
 * @param[in]  bScrubReg     Whether to scrub digest output registers.
 * @param[out] pIntStatus    Pointer to integer to hold any SHA HW int status.
 * 
 * @returns FLCN_STATUS  FLCN_OK if succesful,
 *                       FLCN_ERR_SHA_HW_CHECK_INT_STATUS if error where
 *                       pIntStatus should be checked, relevant error otherwise.
 */
FLCN_STATUS
shahwHmacSha256Final
(
    HMAC_CONTEXT *pHmacContext,
    LwU32        *pHmac,
    LwBool        bScrubReg,
    LwU32        *pIntStatus
)
{
    FLCN_STATUS flcnStatus = FLCN_OK;
    LwU32       msgSize    = 0;
    LwU32       i          = 0;
    LwU8        opad[SHA_256_BLOCK_SIZE_BYTE];
    LwU32       innerDigest[SHA_256_HASH_SIZE_DWORD];

    //
    // Validate HMAC_CONTEXT struct. User should already have valid struct
    // and keys from inititialization, but check again to be sure.
    //
    if (pHmacContext->algoId != SHA_ID_SHA_256 || pHmacContext->keySize == 0 ||
        pHmacContext->keySize > SHA_256_BLOCK_SIZE_BYTE)
    {
        return FLCN_ERR_ILWALID_ARGUMENT;
    }

    // Copy inner digest to local temp buffer.
    CHECK_FLCN_STATUS(shahwReadHashResult_HAL(&shahw, pHmacContext->algoId,
                                              innerDigest, bScrubReg,
                                              pIntStatus));

    // Generate outer key.
    memset(opad, 0, SHA_256_BLOCK_SIZE_BYTE);
    memcpy(opad, &(pHmacContext->keyBuffer), pHmacContext->keySize);
    for (i = 0; i < SHA_256_BLOCK_SIZE_BYTE; i++)
    {
       opad[i] = opad[i] ^ HMAC_SHA_256_OPAD_MASK;
    }

    // Initialize new SHA session for H(outer key || H(inner key || message)).
    msgSize = SHA_256_BLOCK_SIZE_BYTE + SHA_256_HASH_SIZE_BYTE;
    CHECK_FLCN_STATUS(shahwInitializeEngine_HAL(&shahw, pHmacContext->algoId,
                                                msgSize));

    // Add outer key to hash, ensuring we use default IV.
    CHECK_FLCN_STATUS(shahwInsertTask_HAL(&shahw, pHmacContext->algoId,
                                          SHA_256_BLOCK_SIZE_BYTE, &msgSize,
                                          LW_TRUE, (LwU64)(LwU32)opad,
                                          SHA_SRC_TYPE_DMEM, 0, pIntStatus,
                                          LW_TRUE));

    // Add H(inner key || message) to digest, resulting in HMAC.
    CHECK_FLCN_STATUS(shahwInsertTask_HAL(&shahw, pHmacContext->algoId,
                                          SHA_256_HASH_SIZE_BYTE, &msgSize,
                                          LW_FALSE, (LwU64)(LwU32)innerDigest,
                                          SHA_SRC_TYPE_DMEM, 0, pIntStatus,
                                          LW_TRUE));

    // Output HMAC.
    CHECK_FLCN_STATUS(shahwReadHashResult_HAL(&shahw, pHmacContext->algoId,
                                              (LwU32 *)pHmac, bScrubReg,
                                              pIntStatus));

ErrorExit:

    return flcnStatus;
}

/*!
 * @brief To be used when waiting is not performed for shahw*Update operations.
 *        Function will wait for any current SHA operation to complete, and
 *        ensure any previous operation has completed successfully. Will
 *        wait for SHAHW_ENGINE_IDLE_TIMEOUT_NS.
 *
 * @param[out] pIntStatus  Pointer to enum value which holds success status
 *                         or reason for failure.
 *
 * @returns FLCN_STATUS  FLCN_OK if engine idle and no errors,
 *                       FLCN_ERR_SHA_HW_CHECK_INT_STATUS if error status
 *                       found with extended info available via pIntStatus,
 *                       relevant error otherwise.
 */
FLCN_STATUS shahwWaitTaskComplete
(
    LwU32 *pIntStatus
)
{
    return shahwWaitTaskComplete_HAL(&shahw, pIntStatus);
}
