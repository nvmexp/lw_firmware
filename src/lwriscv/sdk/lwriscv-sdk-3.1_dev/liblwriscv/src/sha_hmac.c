/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */
/*!
 * @file    sha_hmac.c
 * @brief   HMAC-SHA driver
 *
 * Used to callwlate HMAC-SHA hash value.
 */

#include <stdint.h>
#include <stddef.h>
#include <lwmisc.h>

#include <liblwriscv/sha.h>
#include <liblwriscv/libc.h>
#include <liblwriscv/io.h>


// HMAC specific defines
#define HMAC_SHA_IPAD_MASK                  0x36
#define HMAC_SHA_OPAD_MASK                  0x5c
#define HMAC_SHA_MAX_BLOCK_SIZE_BYTE        SHA_512_BLOCK_SIZE_BYTE
#define HMAC_SHA_MAX_HASH_SIZE_BYTE         SHA_512_HASH_SIZE_BYTE


//TODO: Move to right header.
#define CHECK_STATUS_AND_RETURN_IF_NOT_OK(x) if((status=(x)) != LWRV_OK)       \
                                             {                                 \
                                               return status;                  \
                                             }
#define CHECK_STATUS_AND_GOTO_CLEANUP_IF_NOT_OK(x) if((status=(x)) != LWRV_OK) \
                                             {                                 \
                                               goto cleanup;                   \
                                             }

/*
 * @brief This function checks whether particular SHA_ALGO_ID is supported for HMAC computation.
 * @param[in] algoId SHA ALGO Id
 *
 * @return true if SHA_ALGO_ID is supported else false.
 */
static bool _shaHmacIsShaAlgoSupported
(
    SHA_ALGO_ID algoId
)
{
    switch (algoId)
    {
        case SHA_ALGO_ID_SHA_256:
        case SHA_ALGO_ID_SHA_384:
        case SHA_ALGO_ID_SHA_512:
            return true;
        default:
            return false; 
    }
}

LWRV_STATUS
shaHmacOperationInit
(
    HMAC_CONTEXT *pHmacContext
)
{
    LWRV_STATUS      status                        = LWRV_ERR_SHA_ENG_ERROR;;
    uint32_t         loopCount                     = 0;
    uint32_t         algoBlockSizeByte             = 0;
    SHA_CONTEXT      *pShaContext                  = NULL;
    SHA_TASK_CONFIG  taskCfg;
    uint8_t          ipad[HMAC_SHA_MAX_BLOCK_SIZE_BYTE];

    if (pHmacContext == NULL)
    {
        return LWRV_ERR_ILWALID_ARGUMENT;
    }

    pShaContext = &(pHmacContext->shaContext);

    CHECK_STATUS_AND_RETURN_IF_NOT_OK(shaGetBlockSizeByte(pShaContext->algoId, &algoBlockSizeByte));

    // Ensure context is initialized and valid.
    if ((pShaContext->msgSize == 0)                         ||
        !_shaHmacIsShaAlgoSupported(pShaContext->algoId)  ||
        (pHmacContext->keySize > algoBlockSizeByte)         ||
        (pHmacContext->keySize == 0) ||
        (pShaContext->msgSize > (UINT32_MAX - algoBlockSizeByte) ))
    {
        return LWRV_ERR_ILWALID_ARGUMENT;
    }

    //
    // Initialize first SHA session - first hash will be H(inner key || message).
    // msgSize will be decremented by algoHashSizeByte after call to shaInsertTask
    //
    pShaContext->msgSize = pShaContext->msgSize + algoBlockSizeByte;

    CHECK_STATUS_AND_RETURN_IF_NOT_OK(shaOperationInit(pShaContext));

    // Generate inner key.
    memset(ipad, 0, algoBlockSizeByte);
    memcpy(ipad, &(pHmacContext->keyBuffer), pHmacContext->keySize);
    for (loopCount = 0; loopCount < algoBlockSizeByte; loopCount++)
    {
       ipad[loopCount] = ipad[loopCount] ^ HMAC_SHA_IPAD_MASK;
    }

    taskCfg.srcType        = LW_PRGNLCL_FALCON_SHA_SRC_CONFIG_SRC_DMEM;
    taskCfg.bDefaultHashIv = true;
    taskCfg.dmaIdx         = 0;      // dmaIdx is unused here since srcType is DMEM
    taskCfg.size           = algoBlockSizeByte;
    taskCfg.addr           = (uint64_t)ipad;

    // Add inner key to hash.
    CHECK_STATUS_AND_RETURN_IF_NOT_OK(shaInsertTask(pShaContext, &taskCfg));

    return status;
}

LWRV_STATUS
shaHmacInsertTask
(
    HMAC_CONTEXT    *pHmacContext,
    SHA_TASK_CONFIG *pTaskCfg
)
{
    LWRV_STATUS status = LWRV_OK;

    if (pHmacContext == NULL || !_shaHmacIsShaAlgoSupported(pHmacContext->shaContext.algoId))
    {
        return LWRV_ERR_ILWALID_ARGUMENT;
    }

    //
    // bDefaultHashIv is expected to be false since already SHA computation is begin in
    // shaHmacOperationInit where SHA(K xor ipad) is computed
    // If this is set to true complete hash value gets modified hence forcing it to false
    //
    pTaskCfg->bDefaultHashIv = false;

    CHECK_STATUS_AND_RETURN_IF_NOT_OK(shaInsertTask(&(pHmacContext->shaContext), pTaskCfg));
    return status; 
}

LWRV_STATUS
shaHmacReadHashResult
(
    HMAC_CONTEXT *pHmacContext
)
{
    LWRV_STATUS     status                                    = LWRV_OK;
    uint32_t        msgSize                                   = 0;
    uint32_t        loopCount                                 = 0;
    uint32_t        algoBlockSizeByte                         = 0;
    uint32_t        algoHashSizeByte                          = 0;
    SHA_CONTEXT     *pShaContext;
    SHA_CONTEXT     tempShaContext;
    SHA_TASK_CONFIG taskCfg;
    uint8_t         opad[HMAC_SHA_MAX_BLOCK_SIZE_BYTE]        = {0};
    uint8_t         innerDigest[HMAC_SHA_MAX_HASH_SIZE_BYTE]  = {0};

    if (pHmacContext == NULL)
    {
        return LWRV_ERR_ILWALID_ARGUMENT;
    }

    pShaContext = &(pHmacContext->shaContext);

    CHECK_STATUS_AND_RETURN_IF_NOT_OK(shaGetBlockSizeByte(pShaContext->algoId , &algoBlockSizeByte));

    // Ensure context is initialized and valid.
    if (!_shaHmacIsShaAlgoSupported(pShaContext->algoId)       ||
        pHmacContext->keySize > algoBlockSizeByte              ||
        pHmacContext->keySize == 0)
    {
        return LWRV_ERR_ILWALID_ARGUMENT;
    }

    tempShaContext         = pHmacContext->shaContext;
    tempShaContext.pBufOut = (uint8_t *)&innerDigest;

    CHECK_STATUS_AND_RETURN_IF_NOT_OK(shaReadHashResult(&tempShaContext, true));

    // Generate outer key.
    memset(opad, 0, algoBlockSizeByte);
    memcpy(opad, &(pHmacContext->keyBuffer), pHmacContext->keySize);
    for (loopCount = 0; loopCount < algoBlockSizeByte; loopCount++)
    {
       opad[loopCount] = opad[loopCount] ^ HMAC_SHA_OPAD_MASK;
    }

    // Initialize new SHA session for H(outer key || H(inner key || message)).
    CHECK_STATUS_AND_RETURN_IF_NOT_OK(shaGetHashSizeByte(pShaContext->algoId, &algoHashSizeByte));
    msgSize = algoBlockSizeByte + algoHashSizeByte;

    tempShaContext.msgSize = msgSize;

    CHECK_STATUS_AND_RETURN_IF_NOT_OK(shaOperationInit(&tempShaContext));

    taskCfg.srcType        = LW_PRGNLCL_FALCON_SHA_SRC_CONFIG_SRC_DMEM;
    taskCfg.bDefaultHashIv = true;
    taskCfg.dmaIdx         = 0;      // dmaIdx is unused here since srcType is DMEM
    taskCfg.size           = algoBlockSizeByte;
    taskCfg.addr           = (uint64_t)&opad;

    // Add outer key to hash, ensuring we use default IV.
    CHECK_STATUS_AND_RETURN_IF_NOT_OK(shaInsertTask(&tempShaContext, &taskCfg));

    taskCfg.srcType        = LW_PRGNLCL_FALCON_SHA_SRC_CONFIG_SRC_DMEM;
    taskCfg.bDefaultHashIv = false;
    taskCfg.dmaIdx         = 0;      // dmaIdx is unused here since srcType is DMEM
    taskCfg.size           = algoHashSizeByte;
    taskCfg.addr           = (uint64_t)&innerDigest;

    // Add H(inner key || message) to digest, resulting in HMAC.
    CHECK_STATUS_AND_RETURN_IF_NOT_OK(shaInsertTask(&tempShaContext, &taskCfg));

    // Output HMAC.
    CHECK_STATUS_AND_RETURN_IF_NOT_OK(shaReadHashResult(pShaContext, true));

    return status;
}

LWRV_STATUS shaHmacRunSingleTaskCommon
(
    HMAC_CONTEXT *pHmacContext,
    SHA_TASK_CONFIG *pTaskCfg
)
{
    LWRV_STATUS status = LWRV_ERR_SHA_ENG_ERROR;

    if ((pHmacContext == NULL) || (pTaskCfg == NULL))
    {
        return LWRV_ERR_ILWALID_ARGUMENT;
    }

    CHECK_STATUS_AND_RETURN_IF_NOT_OK(shaAcquireMutex(pHmacContext->shaContext.mutexToken));

    CHECK_STATUS_AND_GOTO_CLEANUP_IF_NOT_OK(shaHmacOperationInit(pHmacContext));

    CHECK_STATUS_AND_GOTO_CLEANUP_IF_NOT_OK(shaHmacInsertTask(pHmacContext, pTaskCfg));

    CHECK_STATUS_AND_GOTO_CLEANUP_IF_NOT_OK(shaHmacReadHashResult(pHmacContext));

cleanup:
    shaReleaseMutex(pHmacContext->shaContext.mutexToken);
    return status;
}
