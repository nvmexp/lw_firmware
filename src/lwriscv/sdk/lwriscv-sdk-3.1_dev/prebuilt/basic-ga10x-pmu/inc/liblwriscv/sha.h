/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

#ifndef LIBLWRISCV_SHA_H
#define LIBLWRISCV_SHA_H

#include <stdint.h>
#include <stdbool.h>
#include <lwriscv/status.h>

#define BYTE_TO_BIT_LENGTH(a) (a << 3U)

/* Size defines for SHA variants */
#define SHA_1_BLOCK_SIZE_BYTE         (64U)
#define SHA_1_BLOCK_SIZE_BIT          BYTE_TO_BIT_LENGTH(SHA_1_BLOCK_SIZE_BYTE)
#define SHA_1_HASH_SIZE_BYTE          (20U)
#define SHA_1_HASH_SIZE_BIT           BYTE_TO_BIT_LENGTH(SHA_1_HASH_SIZE_BYTE)

#define SHA_224_BLOCK_SIZE_BYTE       (64U)
#define SHA_224_BLOCK_SIZE_BIT        BYTE_TO_BIT_LENGTH(SHA_224_BLOCK_SIZE_BYTE)
#define SHA_224_HASH_SIZE_BYTE        (28U)
#define SHA_224_HASH_SIZE_BIT         BYTE_TO_BIT_LENGTH(SHA_224_HASH_SIZE_BYTE)

#define SHA_256_BLOCK_SIZE_BYTE       (64U)
#define SHA_256_BLOCK_SIZE_BIT        BYTE_TO_BIT_LENGTH(SHA_256_BLOCK_SIZE_BYTE)
#define SHA_256_HASH_SIZE_BYTE        (32U)
#define SHA_256_HASH_SIZE_BIT         BYTE_TO_BIT_LENGTH(SHA_256_HASH_SIZE_BYTE)

#define SHA_384_BLOCK_SIZE_BYTE       (128U)
#define SHA_384_BLOCK_SIZE_BIT        BYTE_TO_BIT_LENGTH(SHA_384_BLOCK_SIZE_BYTE)
#define SHA_384_HASH_SIZE_BYTE        (48U)
#define SHA_384_HASH_SIZE_BIT         BYTE_TO_BIT_LENGTH(SHA_384_HASH_SIZE_BYTE)

#define SHA_512_BLOCK_SIZE_BYTE       (128U)
#define SHA_512_BLOCK_SIZE_BIT        BYTE_TO_BIT_LENGTH(SHA_512_BLOCK_SIZE_BYTE)
#define SHA_512_HASH_SIZE_BYTE        (64U)
#define SHA_512_HASH_SIZE_BIT         BYTE_TO_BIT_LENGTH(SHA_512_HASH_SIZE_BYTE)

#define SHA_512_224_BLOCK_SIZE_BYTE   (128U)
#define SHA_512_224_BLOCK_SIZE_BIT    BYTE_TO_BIT_LENGTH(SHA_512_224_BLOCK_SIZE_BYTE)
#define SHA_512_224_HASH_SIZE_BYTE    (28U)
#define SHA_512_224_HASH_SIZE_BIT     BYTE_TO_BIT_LENGTH(SHA_512_224_HASH_SIZE_BYTE)

#define SHA_512_256_BLOCK_SIZE_BYTE   (128U)
#define SHA_512_256_BLOCK_SIZE_BIT    BYTE_TO_BIT_LENGTH(SHA_512_256_BLOCK_SIZE_BYTE)
#define SHA_512_256_HASH_SIZE_BYTE    (32U)
#define SHA_512_256_HASH_SIZE_BIT     BYTE_TO_BIT_LENGTH(SHA_512_256_HASH_SIZE_BYTE)

typedef enum
{
    SHA_ALGO_ID_SHA_1       = (0U),
    SHA_ALGO_ID_SHA_224,
    SHA_ALGO_ID_SHA_256,
    SHA_ALGO_ID_SHA_384,
    SHA_ALGO_ID_SHA_512,
    SHA_ALGO_ID_SHA_512_224,
    SHA_ALGO_ID_SHA_512_256,
    SHA_ALGO_ID_LAST
} SHA_ALGO_ID;

typedef struct 
{
    uint8_t   srcType;
    uint8_t   bDefaultHashIv;
    uint8_t   pad[2];
    uint32_t  dmaIdx;
    uint32_t  size;
    uint64_t  addr;
} SHA_TASK_CONFIG;

typedef struct
{
    SHA_ALGO_ID algoId;
    uint32_t    msgSize;
    uint8_t     *pBufOut;
    uint32_t    bufSize;
    uint8_t     mutexToken;
} SHA_CONTEXT, *PSHA_CONTEXT;

typedef struct
{
    SHA_CONTEXT shaContext;
    /*
     *  keyBuffer: Buffer to hold key used for HMAC operation. Maximum size is
     *             SHA_512_BLOCK_SIZE_BYTE - if key is larger, user should hash first.
     */
    uint8_t keyBuffer[SHA_512_BLOCK_SIZE_BYTE];

    /*
     *  keySize: Size of key in above buffer, in bytes.
     */ 
    uint32_t keySize;
} HMAC_CONTEXT, *PHMAC_CONTEXT;

/*!
 * @brief Initilaize SHA operation.
 *
 * @param[in] pShaContext The designated SHA context
 *
 * @return LWRV_OK if initialization is successfuly; otherwise LWRV_ERR_SHA_XXX return.
 *
 * Please refer to 17. Programming Guidelines in SHA IAS to get more detail description.
 */
LWRV_STATUS shaOperationInit(SHA_CONTEXT *pShaContext);

/*!
 * @brief Execute SHA engine halt
 *
 * @return LWRV_OK if halt engine successfully
 *         LWRV_ERR_SHA_ENG_ERROR if failed.
 *
 */
LWRV_STATUS shaEngineHalt(void);

/*!
 * @brief Acquire SHA mutex
 *
 * @param[in] token The token allocated by client to acquire SHA mutex.
 *
 * @return LWRV_OK if acquire or release mutex successfuly.
 *         LWRV_ERR_SHA_MUTEX_ACQUIRE_FAILED if failed.
 *
 */
LWRV_STATUS shaAcquireMutex(uint8_t token);

/*!
 * @brief Release SHA mutex
 *
 * @param[in] token The token allocated by client to release SHA mutex.
 *
 * @return LWRV_OK if acquire or release mutex successfuly.
 *         LWRV_ERR_SHA_MUTEX_ACQUIRE_FAILED if failed.
 *
 */
LWRV_STATUS shaReleaseMutex(uint8_t token);

/*!
 * @brief Insert SHA task configuration.
 *
 * @param[in] pShaContext The designated SHA context
 * @param[in] pTaskCfg The designated SHA task config
 *
 * @return LWRV_OK if task config is inserted successfully; otherwise return LWRV_ERR_SHA_XXX.
 *
 * Please refer to 17. Programming Guidelines in SHA IAS to get more detail
 *
 */
LWRV_STATUS shaInsertTask(SHA_CONTEXT *pShaContext, SHA_TASK_CONFIG *pTaskCfg);

/*!
 * @brief Read SHA hash result.
 *
 * @param[in]  pShaContext The designated SHA context
 * @param[out] pBuf The output buffer to save computed SHA hash value
 * @param[in]  bufSize The output buffer size.
 * @param[in]  bScrubReg true to clear hash registers after hash is read out; false doesn't clear hash registers.
 *
 * @return LWRV_OK if hash read successfully; otherwise return LWRV_ERR_SHA_XXX.
 *
 */
LWRV_STATUS shaReadHashResult(SHA_CONTEXT *pShaContext, bool bScrubReg);

/*!
 * @brief Run single task through SHA HW.
 *
 * @param[in] pTaskCfg The designated SHA task config
 * @param[in] pShaCtx  The struct containing SHA task parameters and output buffer pointer.
 *
 * @return LWRV_OK if hash read successfully; otherwise return LWRV_ERR_SHA_XXX.
 *
 */
LWRV_STATUS shaRunSingleTaskCommon(SHA_TASK_CONFIG *pTaskCfg, SHA_CONTEXT *pShaCtx);
/*!
 * @brief To get SHA hash size in byte per SHA algorithm id
 *
 * @param[in]  algoId SHA algorithm id
 * @param[out] *pSize The pointer to save return hash size
 *
 * @return LWRV_OK if get hash size successfully.
 *         LWRV_ERR_ILWALID_ARGUMENT if failed.
 *
 */
LWRV_STATUS shaGetHashSizeByte(SHA_ALGO_ID algoId, uint32_t *pSize);

/*!
 * @brief To get SHA block size in byte per SHA algorithm id
 *
 * @param[in]  algoId SHA algorithm id
 * @param[out] *pSize The pointer to save return block size
 *
 * @return LWRV_OK if get hash size successfully.
 *         LWRV_ERR_ILWALID_ARGUMENT if failed.
 *
 */
LWRV_STATUS shaGetBlockSizeByte(SHA_ALGO_ID algoId, uint32_t *pSize);

/*!
 * @brief Initilaize SHA HW to start HMAC SHA-256 operation.
 * @param[in] pHmacContext The designated HMAC SHA-256 context
 *
 * @return LWRV_OK if initialization is successfuly; otherwise LWRV_ERR_SHA_XXX return.
 *
 */
LWRV_STATUS shaHmacOperationInit(HMAC_CONTEXT *pHmacContext);

/*!
 * @brief Insert HMAC-SHA-256 task configuration.
 *       Task is basically plain textfor which Hash needs to be computed
 *
 * @param[in] pHmacContext The designated HMAC SHA-256 context
 * @param[in] pTaskCfg The designated SHA task config
 *            pTaskCfg->bDefaultHashIv should always be set to LW_FALSE for HMAC computation
 *            if not done then API itself forces it to LW_FALSE.
 *
 * @return LWRV_OK if task config is inserted successfully; otherwise return LWRV_ERR_SHA_XXX.
 */
LWRV_STATUS shaHmacInsertTask(HMAC_CONTEXT *pHmacContext, SHA_TASK_CONFIG *pTaskCfg);

/*!
 * @brief Read HMAC-SHA-256 result 
 *        This API concludes the HMAC-SHA-256 operation and reads out Hash result in output buffer.
 * @param[in] pHmacContext The designated HMAC SHA-256 context
 * @return LWRV_OK if task config is inserted successfully; otherwise return LWRV_ERR_SHA_XXX.
 */
LWRV_STATUS shaHmacReadHashResult(HMAC_CONTEXT *pHmacContext);

/*!
 * @brief Run single task through HMAC-SHA-256.
 *        When application has the complete plain text data then this API can be used
 *        This will compute Hash for plain text as well as return the result in output buffer of taskCfg.
 *
 * @param[in] pHmacContext The struct containing HMAC SHA task parameters and output buffer pointer.
 * @param[in] pTaskCfg     The designated HMAC SHA task config
 *            pTaskCfg->bDefaultHashIv should always be set to LW_FALSE for HMAC computation
 *            if not done then API itself forces it to LW_FALSE.
 *
 * @return LWRV_OK if hash read successfully; otherwise return LWRV_ERR_SHA_XXX.
 */
LWRV_STATUS shaHmacRunSingleTaskCommon(HMAC_CONTEXT *pHmacContext, SHA_TASK_CONFIG *pTaskCfg);
#endif // LIBLWRISCV_SHA_H
