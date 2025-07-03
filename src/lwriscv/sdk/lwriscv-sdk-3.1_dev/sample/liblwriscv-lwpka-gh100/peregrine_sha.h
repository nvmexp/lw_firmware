/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

#ifndef ACRSHA_H
#define ACRSHA_H

#include <lwtypes.h>

#define SHA_ENGINE_IDLE_TIMEOUT      (100000000)                    // sync with PTIMER, unit is ns, lwrrently 100ms is safe for the longest message
#define SHA_ENGINE_SW_RESET_TIMEOUT  (SHA_ENGINE_IDLE_TIMEOUT + 20) // sync with PTIMER, unit is ns, base on idle timeout, HW recommend 100ms + 20ns

#define BYTE_TO_BIT_LENGTH(a) (a << 3)

#define SHA_MSG_BYTES_MAX  (0xFFFFFFFF >> 3)

#define ACR_OK                              0U
#define ACR_ERROR_SHA_ENG_ERROR             990U
#define ACR_ERROR_SHA_WAIT_IDLE_TIMEOUT     991U
#define ACR_ERROR_SHA_SW_RESET_TIMEOUT      992U
#define ACR_ERROR_SHA_MUTEX_ACQUIRE_FAILED  993U
#define ACR_ERROR_SHA_MUTEX_RELEASE_FAILED  994U
#define ACR_ERROR_SHA_ILWALID_CONFIG        995U
#define ACR_ERROR_ILWALID_ARGUMENT          996U

//TODO: Remove this when moving to lwriscv and use byteswap function from utils
#define LW_BYTESWAP32(a) (      \
    (((a) & 0xff000000)>>24) |  \
    (((a) & 0x00ff0000)>>8)  |  \
    (((a) & 0x0000ff00)<<8)  |  \
    (((a) & 0x000000ff)<<24) )

typedef enum
{
    SHA_ALGO_ID_SHA_1       = (0),
    SHA_ALGO_ID_SHA_224,
    SHA_ALGO_ID_SHA_256,
    SHA_ALGO_ID_SHA_384,
    SHA_ALGO_ID_SHA_512,
    SHA_ALGO_ID_SHA_512_224,
    SHA_ALGO_ID_SHA_512_256,
    SHA_ALGO_ID_LAST
} SHA_ALGO_ID;

//TODO: Change types when moved to lwriscv
typedef struct _SHA_TASK_CONFIG
{
    LwU8   srcType;
    LwU8   bDefaultHashIv;
    LwU8   pad[2];
    LwU32  dmaIdx;
    LwU32  size;
    LwU64  addr;
} SHA_TASK_CONFIG;

typedef struct _SHA_CONTEXT
{
    SHA_ALGO_ID algoId;
    LwU32  msgSize;
} SHA_CONTEXT, *PSHA_CONTEXT;

//TODO: Create derivative macros for bit sizes
#define SHA_1_BLOCK_SIZE_BIT          (512)
#define SHA_1_BLOCK_SIZE_BYTE         (512/8)
#define SHA_1_HASH_SIZE_BIT           (160)
#define SHA_1_HASH_SIZE_BYTE          (SHA_1_HASH_SIZE_BIT/8)

#define SHA_224_BLOCK_SIZE_BIT        (512)
#define SHA_224_BLOCK_SIZE_BYTE       (512/8)
#define SHA_224_HASH_SIZE_BIT         (224)
#define SHA_224_HASH_SIZE_BYTE        (SHA_224_HASH_SIZE_BIT/8)

#define SHA_256_BLOCK_SIZE_BIT        (512)
#define SHA_256_BLOCK_SIZE_BYTE       (512/8)
#define SHA_256_HASH_SIZE_BIT         (256)
#define SHA_256_HASH_SIZE_BYTE        (SHA_256_HASH_SIZE_BIT/8)

#define SHA_384_BLOCK_SIZE_BIT        (1024)
#define SHA_384_BLOCK_SIZE_BYTE       (SHA_384_BLOCK_SIZE_BIT/8)
#define SHA_384_HASH_SIZE_BIT         (384)
#define SHA_384_HASH_SIZE_BYTE        (SHA_384_HASH_SIZE_BIT/8)

#define SHA_512_BLOCK_SIZE_BIT        (1024)
#define SHA_512_BLOCK_SIZE_BYTE       (SHA_512_BLOCK_SIZE_BIT/8)
#define SHA_512_HASH_SIZE_BIT         (512)
#define SHA_512_HASH_SIZE_BYTE        (SHA_512_HASH_SIZE_BIT/8)

#define SHA_512_224_BLOCK_SIZE_BIT    (1024)
#define SHA_512_224_BLOCK_SIZE_BYTE   (SHA_512_BLOCK_SIZE_BIT/8)
#define SHA_512_224_HASH_SIZE_BIT     (224)
#define SHA_512_224_HASH_SIZE_BYTE    (SHA_512_224_HASH_SIZE_BIT/8)

#define SHA_512_256_BLOCK_SIZE_BIT    (1024)
#define SHA_512_256_BLOCK_SIZE_BYTE   (SHA_512_BLOCK_SIZE_BIT/8)
#define SHA_512_256_HASH_SIZE_BIT     (256)
#define SHA_512_256_HASH_SIZE_BYTE    (SHA_512_256_HASH_SIZE_BIT/8)

LwU32 shaOperationInit(SHA_CONTEXT *pShaContext); 
LwU32 shaWaitEngineIdle(LwU32 timeOutNs);
LwU32 shaEngineSoftReset(LwU32 timeOutNs);
LwU32 shaEngineHalt(void);
LwU32 shaAcquireMutex(LwU8 token);
LwU32 shaReleaseMutex(LwU8 token);
LwU32 shaInsertTask(SHA_CONTEXT *pShaContext, SHA_TASK_CONFIG *pTaskCfg);
LwU32 shaReadHashResult(SHA_CONTEXT *pShaContext, void *pBuf, LwBool bScrubReg);
LwU32 shaGetBlockSizeByte(SHA_ALGO_ID algoId, LwU32 *pBlockSizeByte);

#endif // ACRSHA_H
