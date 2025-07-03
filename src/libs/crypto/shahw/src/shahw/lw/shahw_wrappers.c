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
 * @file   shahw_wrappers.c
 * @brief  Wrappers around SHA HW library to provide simplified SHA interfaces.
 *
 *         NOTE: If making ANY edits to this file, they should be mirrored
 *         in the HS implementation of these functions, as otherwise important
 *         bugfixes could be missed!
 */

/* ------------------------ Application Includes --------------------------- */
#include "shahw.h"

/* ------------------------ Public Functions ------------------------------- */
/*!
 * @brief Function to callwlate the SHA 1 hash of a given input message.
 * 
 * @param[in]       pMessage     Pointer to the message in DMEM to hash.
 * @param[in]       messageSize  Size of the message to be hashed, in bytes.
 * @param[out]      pDigest      Pointer to output DMEM buffer that will hold digest.
 *                               Buffer must be of size SHA_1_HASH_SIZE_BYTE.
 *
 * @returns FLCN_STATUS  FLCN_OK if successful, relevant error otherwise.
 */
FLCN_STATUS
shahwSha1
(
    LwU8  *pMessage,
    LwU32  messageSize,
    LwU8  *pDigest
)
{
    FLCN_STATUS flcnStatus  = FLCN_OK;
    LwU64       messageAddr = (LwUPtr)pMessage;
    LwU32       intStatus   = 0;
    LwU32       localDigest[SHA_1_HASH_SIZE_DWORD];
    SHA_CONTEXT shaContext;

    if (pMessage == NULL || messageSize == 0 || pDigest == NULL)
    {
        return FLCN_ERR_ILWALID_ARGUMENT;
    }

    memset(&shaContext, 0, sizeof(shaContext));
    shaContext.algoId  = SHA_ID_SHA_1;
    shaContext.msgSize = messageSize;
    shaContext.mutexId = SHA_MUTEX_ID_SHAHW_FUNCS;

    //
    // Return immediately here and before we acquire mutex, as on
    // ErrorExit we will try and release.
    //
    if ((flcnStatus = shahwSoftReset()) != FLCN_OK)
    {
        return flcnStatus;
    }

    if ((flcnStatus = shahwAcquireMutex(shaContext.mutexId)) != FLCN_OK)
    {
        return flcnStatus;
    }

    CHECK_FLCN_STATUS(shahwOpInit(&shaContext));
    CHECK_FLCN_STATUS(shahwOpUpdate(&shaContext, messageSize, LW_TRUE, messageAddr,
                                    SHA_SRC_TYPE_DMEM, 0, &intStatus, LW_TRUE));

    //
    // Finalize digest and store to local buffer before copying to output.
    // Local buffer is required to ensure alignment compatibility with shahwOpFinal().
    //
    CHECK_FLCN_STATUS(shahwOpFinal(&shaContext, localDigest, LW_TRUE, &intStatus));
    memcpy(pDigest, (LwU8 *)&localDigest, SHA_1_HASH_SIZE_BYTE);

ErrorExit:

    // Scrub any potential data stored in local digest.
    memset((LwU8 *)&localDigest, 0, SHA_1_HASH_SIZE_BYTE);

    // Release mutex. This really shouldn't ever fail.
    if (shahwReleaseMutex(shaContext.mutexId) != FLCN_OK)
    {
        return FLCN_ERR_ILLEGAL_OPERATION;
    }

    return flcnStatus;
}

/*!
 * @brief Function to callwlate the SHA 256 hash of a given input message.
 * 
 * @param[in]       pMessage     Pointer to the message in DMEM to hash.
 * @param[in]       messageSize  Size of the message to be hashed, in bytes.
 * @param[out]      pDigest      Pointer to output DMEM buffer that will hold digest.
 *                               Buffer must be of size SHA_256_HASH_SIZE_BYTE.
 *
 * @returns FLCN_STATUS  FLCN_OK if successful, relevant error otherwise.
 */
FLCN_STATUS
shahwSha256
(
    LwU8  *pMessage,
    LwU32  messageSize,
    LwU8  *pDigest
)
{
    FLCN_STATUS flcnStatus  = FLCN_OK;
    LwU64       messageAddr = (LwUPtr)pMessage;
    LwU32       intStatus   = 0;
    LwU32       localDigest[SHA_256_HASH_SIZE_DWORD];
    SHA_CONTEXT shaContext;

    if (pMessage == NULL || messageSize == 0 || pDigest == NULL)
    {
        return FLCN_ERR_ILWALID_ARGUMENT;
    }

    memset(&shaContext, 0, sizeof(shaContext));
    shaContext.algoId  = SHA_ID_SHA_256;
    shaContext.msgSize = messageSize;
    shaContext.mutexId = SHA_MUTEX_ID_SHAHW_FUNCS;

    //
    // Return immediately here and before we acquire mutex, as on
    // ErrorExit we will try and release.
    //
    if ((flcnStatus = shahwSoftReset()) != FLCN_OK)
    {
        return flcnStatus;
    }

    if ((flcnStatus = shahwAcquireMutex(shaContext.mutexId)) != FLCN_OK)
    {
        return flcnStatus;
    }

    CHECK_FLCN_STATUS(shahwOpInit(&shaContext));
    CHECK_FLCN_STATUS(shahwOpUpdate(&shaContext, messageSize, LW_TRUE, messageAddr,
                                    SHA_SRC_TYPE_DMEM, 0, &intStatus, LW_TRUE));

    //
    // Finalize digest and store to local buffer before copying to output.
    // Local buffer is required to ensure alignment compatibility with shahwOpFinal().
    //
    CHECK_FLCN_STATUS(shahwOpFinal(&shaContext, localDigest, LW_TRUE, &intStatus));
    memcpy(pDigest, (LwU8 *)&localDigest, SHA_256_HASH_SIZE_BYTE);

ErrorExit:

    // Scrub any potential data stored in local digest.
    memset((LwU8 *)&localDigest, 0, SHA_256_HASH_SIZE_BYTE);

    // Release mutex. This really shouldn't ever fail.
    if (shahwReleaseMutex(shaContext.mutexId) != FLCN_OK)
    {
        return FLCN_ERR_ILLEGAL_OPERATION;
    }

    return flcnStatus;
}

/*!
 * @brief Function to callwlate the HMAC SHA 256 tag with a given
 *        input message and key.
 * 
 * @param[in]       pMessage     Pointer to the message in DMEM to hash.
 * @param[in]       messageSize  Size of the message to be hashed, in bytes.
 * @param[in]       pKey         Pointer to the key in DMEM to be used for HMAC.
 * @param[in]       keySize      Size of the above key, in bytes. Must be less than
 *                               or equal to SHA_256_BLOCK_SIZE_BYTE.
 * @param[out]      pDigest      Pointer to output DMEM buffer that will hold digest.
 *                               Buffer must be of size SHA_256_HASH_SIZE_BYTE.
 *
 * @returns FLCN_STATUS  FLCN_OK if successful, relevant error otherwise.
 */
FLCN_STATUS
shahwHmacSha256
(
    LwU8  *pMessage,
    LwU32  messageSize,
    LwU8  *pKey,
    LwU32  keySize,
    LwU8  *pDigest
)
{
    FLCN_STATUS  flcnStatus  = FLCN_OK;
    LwU64        messageAddr = (LwUPtr)pMessage;
    LwU32        intStatus   = 0;
    LwU32        localTag[SHA_256_HASH_SIZE_DWORD];
    HMAC_CONTEXT hmacContext;

    if (pMessage == NULL || messageSize == 0 || pKey == NULL ||
        keySize == 0 || keySize > SHA_256_BLOCK_SIZE_BYTE || pDigest == NULL)
    {
        return FLCN_ERR_ILWALID_ARGUMENT;
    }

    //
    // Prepate HMAC context. Wait to store key in context until after SHA HW
    // initialization, since we may need to take hash of key.
    //
    memset(&hmacContext, 0, sizeof(hmacContext));
    hmacContext.algoId  = SHA_ID_SHA_256;
    hmacContext.msgSize = messageSize;
    hmacContext.mutexId = SHA_MUTEX_ID_SHAHW_FUNCS;

    //
    // Return immediately here and before we acquire mutex, as on
    // ErrorExit we will try and release.
    //
    if ((flcnStatus = shahwSoftReset()) != FLCN_OK)
    {
        return flcnStatus;
    }

    if ((flcnStatus = shahwAcquireMutex(hmacContext.mutexId)) != FLCN_OK)
    {
        return flcnStatus;
    }

    // Copy key into HMAC context.
    memcpy(&(hmacContext.keyBuffer), pKey, keySize);
    hmacContext.keySize = keySize;

    // Callwlate HMAC of input data. Use temporary tag buffer for alignment.
    CHECK_FLCN_STATUS(shahwHmacSha256Init(&hmacContext, &intStatus));
    CHECK_FLCN_STATUS(shahwHmacSha256Update(&hmacContext, messageSize,
                                            messageAddr, SHA_SRC_TYPE_DMEM,
                                            0, &intStatus, LW_TRUE));
    CHECK_FLCN_STATUS(shahwHmacSha256Final(&hmacContext, localTag,
                                           LW_TRUE, &intStatus));
    memcpy(pDigest, (LwU8 *)&localTag, SHA_256_HASH_SIZE_BYTE);

ErrorExit:
    // Scrub any sensitive material (hmacContext holds key).
    memset(hmacContext.keyBuffer, 0, sizeof(hmacContext.keyBuffer));
    memset((LwU8 *)&localTag, 0, sizeof(localTag));

    // Release mutex. This really shouldn't ever fail.
    if (shahwReleaseMutex(hmacContext.mutexId) != FLCN_OK)
    {
        return FLCN_ERR_ILLEGAL_OPERATION;
    }

    return flcnStatus;
}
