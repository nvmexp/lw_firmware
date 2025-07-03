/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

#include <lwtypes.h>
#include <lwmisc.h>

#include <liblwriscv/libc.h>
#include <liblwriscv/print.h>
#include <liblwriscv/io.h>

#include "peregrine_sha.h"

/*!
 * WARNING WARNING WARNING WARNING WARNING WARNING WARNING WARNING WARNING WARNING WARNING
 *
 * This code is for testing and libCCC usage purposes only!
 * Do not replicate blindly for production usecases. Consult with your local crypto expert!
 * 
 * WARNING WARNING WARNING WARNING WARNING WARNING WARNING WARNING WARNING WARNING WARNING
 * */

/***********************************************************************************
  The SW program guide is at
  https://confluence.lwpu.com/display/~eddichang/ACR+SHA+SW+Programming+Guide
***********************************************************************************/

static const LwU32 sha_512_224_IV[16] = { 0x19544DA2, 0x8C3D37C8, 0x89DCD4D6, 0x73E19966, 0x32FF9C82, 0x1DFAB7AE, 0x582F9FCF, 0x679DD514, 0x7BD44DA8, 0x0F6D2B69, 0x04C48942, 0x77E36F73, 0x6A1D36C8, 0x3F9D85A8, 0x91D692A1, 0x1112E6AD };

static const LwU32 sha_512_256_IV[16] = { 0xFC2BF72C, 0x22312194, 0xC84C64C2, 0x9F555FA3, 0x6F53B151, 0x2393B86B, 0x5940EABD, 0x96387719, 0xA88EFFE3, 0x96283EE2, 0x53863992, 0xBE5E1E25, 0x2C85B8AA, 0x2B0199FC, 0x81C52CA2, 0x0EB72DDC };

static void  _shaSetInitVector(SHA_ALGO_ID algoId);
static LwU8  _shaCheckTaskConfig(SHA_CONTEXT *pShaContext, SHA_TASK_CONFIG *pTaskCfg);
static LwU32 _prepareFetchConfigForHash(SHA_ALGO_ID algoId, LwU32 *pLoopCount, LwU8 *pHashEntry64, LwU8 *pHalfWord);

// TODO: We use Read-Modify-Write in a lot of places in this file, and it seems that this isn't necessarily required.
// TODO: Rename "SHA Task" to something better when we move to lwriscv.

/*!
 * @brief Wait for SHA engine idle
 *
 *
 * @return ACR_OK if engine enter idle state successfuly.
 *         ACR_ERROR_SHA_ENG_ERROR if SHA halted status asserted
 *         ACR_ERROR_TIMEOUT if timeout detected
 */
LwU32
shaWaitEngineIdle
(
    LwU32 timeOutNs
)
{
    LwU32      reg;
    LwU32      lwrrTime;
    LwU32      startTime;

    startTime = localRead(LW_PRGNLCL_FALCON_PTIMER0);

    do
    {
        reg = localRead(LW_PRGNLCL_FALCON_SHA_STATUS);

        if(FLD_TEST_DRF(_PRGNLCL, _FALCON_SHA, _STATUS_STATE, _HALTED, reg))
        {
            return ACR_ERROR_SHA_ENG_ERROR;
        }

        // TODO: Use rdtime instead here
        lwrrTime = localRead(LW_PRGNLCL_FALCON_PTIMER0);

        if ((lwrrTime - startTime) > timeOutNs)
        {
            return ACR_ERROR_SHA_WAIT_IDLE_TIMEOUT;
        }
    } while(FLD_TEST_DRF(_PRGNLCL, _FALCON_SHA, _STATUS_STATE, _BUSY, reg));

    return ACR_OK;
}


/*!
 * @brief Execute SHA engine soft reset and wait for engine idle
 *
 * @return ACR_OK if reset and enter idle state successfuly.
           ACR_ERROR_TIMEOUT if timeout detected
 *
 */
LwU32
shaEngineSoftReset
(
    LwU32 timeOutNs
)
{
    LwU32 reg;
    LwU32 lwrrTime;
    LwU32 startTime;

    reg = localRead(LW_PRGNLCL_FALCON_SHA_OPERATION);
    reg = FLD_SET_DRF(_PRGNLCL_FALCON, _SHA_OPERATION, _SOFTRESET, _ENABLE, reg);
    localWrite(LW_PRGNLCL_FALCON_SHA_OPERATION, reg);

    startTime = localRead(LW_PRGNLCL_FALCON_PTIMER0);
    // wait for soft reset clear
    do
    {
        reg = localRead(LW_PRGNLCL_FALCON_SHA_OPERATION);

        // TODO: Use rdtime instead here
        lwrrTime = localRead(LW_PRGNLCL_FALCON_PTIMER0);

        if ((lwrrTime - startTime) > timeOutNs)
        {
            return ACR_ERROR_SHA_SW_RESET_TIMEOUT;
        }

    } while(FLD_TEST_DRF(_PRGNLCL_FALCON, _SHA_OPERATION, _SOFTRESET, _ENABLE, reg));

    return shaWaitEngineIdle(SHA_ENGINE_IDLE_TIMEOUT);

}

/*!
 * @brief Execute SHA engine halt
 *
 * @return ACR_OK if halt engine successfully
 *         ACR_ERROR_SHA_ENG_ERROR if failed.
 *
 */
LwU32
shaEngineHalt
(
    void
)
{
    LwU32 reg = 0;

    reg = localRead(LW_PRGNLCL_FALCON_SHA_OPERATION);
    reg = FLD_SET_DRF(_PRGNLCL_FALCON, _SHA_OPERATION, _HALT, _ENABLE, reg);
    localWrite(LW_PRGNLCL_FALCON_SHA_OPERATION, reg);

    // TODO: Should we loop here and add timeout logic also?
    reg = localRead(LW_PRGNLCL_FALCON_SHA_STATUS);

    if (FLD_TEST_DRF(_PRGNLCL, _FALCON_SHA, _STATUS_STATE, _HALTED, reg))
    {
        return ACR_OK;
    }

    return ACR_ERROR_SHA_ENG_ERROR;
}

/*!
 * @brief To acquire SHA mutex
 *
 * @param[in] token The token allocated by client to acquire SHA mutex.
 *
 * @return ACR_OK if acquire or release mutex successfuly.
 *         ACR_ERROR_SHA_MUTEX_ACQUIRE_FAILED if failed.
 *
 */
LwU32
shaAcquireMutex
(
    LwU8 token
)
{
    LwU32 reg = 0;
    LwU8  val;

    reg = FLD_SET_DRF_NUM(_PRGNLCL, _FALCON_SHA_MUTEX, _VAL, token, reg);
    localWrite(LW_PRGNLCL_FALCON_SHA_MUTEX , reg);
    reg = localRead(LW_PRGNLCL_FALCON_SHA_MUTEX_STATUS);
    val = DRF_VAL(_PRGNLCL, _FALCON_SHA_MUTEX_STATUS, _LOCKED, reg);

    return (val == token ? ACR_OK : ACR_ERROR_SHA_MUTEX_ACQUIRE_FAILED);
}

/*!
 * @brief To release SHA mutex
 *
 * @param[in] token The token allocated by client to release SHA mutex.
 *
 * @return ACR_OK if acquire or release mutex successfuly.
 *         ACR_ERROR_SHA_MUTEX_ACQUIRE_FAILED if failed.
 *
 */
LwU32
shaReleaseMutex
(
    LwU8 token
)
{
    LwU32 reg = 0;
    LwU8  val;

    reg = FLD_SET_DRF_NUM(_PRGNLCL, _FALCON_SHA_MUTEX_RELEASE, _VAL, token, reg);
    localWrite(LW_PRGNLCL_FALCON_SHA_MUTEX_RELEASE, reg);
    reg = localRead(LW_PRGNLCL_FALCON_SHA_MUTEX_STATUS);
    val = DRF_VAL(_PRGNLCL, _FALCON_SHA_MUTEX_STATUS, _LOCKED, reg);

    return  (val == LW_PRGNLCL_FALCON_SHA_MUTEX_STATUS_LOCKED_INIT ? ACR_OK : ACR_ERROR_SHA_MUTEX_RELEASE_FAILED);
}

/*!
 * @brief To get SHA encode value per SHA algorithm id
 *
 * @param[in]  algoId SHA algorithm id
 * @param[out] *pMode The pointer to save encode value
 *
 * @return ACR_OK if get encode value successfully.
 *         ACR_ERROR_SHA_ILWALID_CONFIG if failed.
 *
 */
static LwU32
_shaGetConfigEncodeMode
(
    SHA_ALGO_ID algoId,
    LwU8        *pMode
)
{
    switch(algoId)
    {
        case SHA_ALGO_ID_SHA_1:
            *pMode = LW_PRGNLCL_FALCON_SHA_CONFIG_ENC_MODE_SHA1;
            break;

        case SHA_ALGO_ID_SHA_224:
            *pMode = LW_PRGNLCL_FALCON_SHA_CONFIG_ENC_MODE_SHA224;
            break;

        case SHA_ALGO_ID_SHA_256:
            *pMode = LW_PRGNLCL_FALCON_SHA_CONFIG_ENC_MODE_SHA256;
            break;

        case SHA_ALGO_ID_SHA_384:
            *pMode = LW_PRGNLCL_FALCON_SHA_CONFIG_ENC_MODE_SHA384;
            break;

        case SHA_ALGO_ID_SHA_512:
            *pMode = LW_PRGNLCL_FALCON_SHA_CONFIG_ENC_MODE_SHA512;
            break;

        default:
            return ACR_ERROR_SHA_ILWALID_CONFIG;
    }

    return ACR_OK;
}

/*!
 * @brief To get SHA hash size in byte per SHA algorithm id
 *
 * @param[in]  algoId SHA algorithm id
 * @param[out] *pSize The pointer to save return hash size
 *
 * @return ACR_OK if get hash size successfully.
 *         ACR_ERROR_SHA_ILWALID_CONFIG if failed.
 *
 */
LwU32
shaGetHashSizeByte
(
    SHA_ALGO_ID  algoId,
    LwU32        *pSize
)
{
    if (pSize == NULL)
    {
        return ACR_ERROR_SHA_ILWALID_CONFIG;
    }

    switch(algoId)
    {
        case SHA_ALGO_ID_SHA_1:
            *pSize = SHA_1_HASH_SIZE_BYTE;
            break;

        case SHA_ALGO_ID_SHA_224:
            *pSize = SHA_224_HASH_SIZE_BYTE;
            break;

        case SHA_ALGO_ID_SHA_256:
            *pSize = SHA_256_HASH_SIZE_BYTE;
            break;

        case SHA_ALGO_ID_SHA_384:
            *pSize = SHA_384_HASH_SIZE_BYTE;
            break;

        case SHA_ALGO_ID_SHA_512:
            *pSize = SHA_512_HASH_SIZE_BYTE;
            break;

        case SHA_ALGO_ID_SHA_512_224:
            *pSize = SHA_512_224_HASH_SIZE_BYTE;
            break;

        case SHA_ALGO_ID_SHA_512_256:
            *pSize = SHA_512_256_HASH_SIZE_BYTE;
            break;

        default:
            return ACR_ERROR_SHA_ILWALID_CONFIG;
    }

    return ACR_OK;
}

/*!
 * @brief To initilaize SHA operation.
 *
 * @param[in] pShaContext The designated SHA context
 *
 * @return ACR_OK if initialization is successfuly; otherwise ACR_ERROR_XXX return.
 *
 * Please refer to 17. Programming Guidelines in SHA IAS to get more detail description.
 */
LwU32
shaOperationInit
(
    SHA_CONTEXT      *pShaContext
)
{
    LwU32 reg;
    LwU32 msgSize, i;
    LwU8  encMode;
    LwU32 status;

    if (pShaContext == NULL)
    {
        return  ACR_ERROR_ILWALID_ARGUMENT;
    }

    //
    // Issue HALT when SHA state is IDLE or HALTED will be dropped silently.
    // So we only halt SHA engine when it's at busy state.
    //
    reg = localRead(LW_PRGNLCL_FALCON_SHA_STATUS);

    if (FLD_TEST_DRF(_PRGNLCL, _FALCON_SHA, _STATUS_STATE, _BUSY, reg))
    {
        status = shaEngineHalt();

        if (status != ACR_OK)
        {
            return status;
        }
    }

    status = shaEngineSoftReset(SHA_ENGINE_SW_RESET_TIMEOUT);

    if (status != ACR_OK)
    {
        return status;
    }

    msgSize = pShaContext->msgSize;

    if (msgSize > SHA_MSG_BYTES_MAX)
    {
        return  ACR_ERROR_ILWALID_ARGUMENT;
    }

    status = _shaGetConfigEncodeMode(pShaContext->algoId, &encMode);

    if (status != ACR_OK)
    {
        return  status;
    }

    reg = 0;
    reg = FLD_SET_DRF_NUM(_PRGNLCL, _FALCON_SHA_CONFIG, _ENC_MODE, encMode, reg);
    reg = FLD_SET_DRF(_PRGNLCL, _FALCON_SHA_CONFIG, _ENC_ALG, _SHA, reg);
    reg = FLD_SET_DRF(_PRGNLCL, _FALCON_SHA_CONFIG, _DST, _HASH_REG, reg);
    localWrite(LW_PRGNLCL_FALCON_SHA_CONFIG, reg);

    //
    // !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
    // SW reset should clear LW_CSEC_FALCON_SHA_MSG_LENGTH(i) and LW_CSEC_FALCON_SHA_MSG_LEFT(i)
    // But we observer LW_CSEC_FALCON_SHA_MSG_LEFT(i) still has garbage values.
    // So we still clear these all registers to make sure operation able to work correctly.
    // !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
    //
    for (i = 0; i < LW_PRGNLCL_FALCON_SHA_MSG_LENGTH__SIZE_1; i++)
    {
        localWrite(LW_PRGNLCL_FALCON_SHA_MSG_LENGTH(i), 0);
    }

    for (i = 0; i < LW_PRGNLCL_FALCON_SHA_MSG_LEFT__SIZE_1; i++)
    {
        localWrite(LW_PRGNLCL_FALCON_SHA_MSG_LEFT(i), 0);
    }

    //
    // In NIST spec, the MIN unit for SHA message is bit, but in real case, we just use byte unit.
    // Although HW provide 4 registers for messgae length and left message length separately,
    // but lwrrently SW just usea one 32-bit register for SHA length(0x1FFFFFFF bytes)
    // In real case usage, openSSL supports byte unit as well.
    // And it should be enough for current requirement.
    //

    // support  SHA engine needs bit length
    localWrite(LW_PRGNLCL_FALCON_SHA_MSG_LENGTH(0), BYTE_TO_BIT_LENGTH(msgSize));

    return ACR_OK;
}

/*!
 * @brief To set initial vector for SHA-512-224 and SHA-512-256.
 *
 * @param[in] algoId The SHA algorithm id.
 *
 */
static void
_shaSetInitVector
(
    SHA_ALGO_ID algoId
)
{
    LwU32 i      = 0;
    LwU32 arrLen = 0;

    if (algoId == SHA_ALGO_ID_SHA_512_224)
    {
        arrLen = sizeof(sha_512_224_IV)/sizeof(sha_512_224_IV[0]);
        for(i=0; i<arrLen; i++)
        {
            localWrite( LW_PRGNLCL_FALCON_SHA_HASH_RESULT(i), sha_512_224_IV[i]);
        }
    }
    else if (algoId == SHA_ALGO_ID_SHA_512_256)
    {
        arrLen = sizeof(sha_512_256_IV)/sizeof(sha_512_256_IV[0]);
        for(i=0; i<arrLen; i++)
        {
            localWrite( LW_PRGNLCL_FALCON_SHA_HASH_RESULT(i), sha_512_256_IV[i]);
        }
    }
}

/*!
 * @brief To check SHA task configuration.
 *
 * @param[in] pShaContext The designated SHA context
 * @param[in] pTaskCfg The designated SHA task config
 *
 * @return LW_TRUE if task config is valid; otherwise return LW_FALSE.
 *
 */
static LwU8
_shaCheckTaskConfig
(
    SHA_CONTEXT *pShaContext,
    SHA_TASK_CONFIG *pTaskCfg
)
{
    LwU8 bValid = LW_TRUE;

    //
    // We use pShaContext->msgSize to track left message length.
    // Once left message size is less(or equal to) than inserted task size,
    // This means this is the last task. Only the last task size doesn't
    // have to be aligned with block size.
    //
    if (pShaContext->msgSize == pTaskCfg->size)
    {
        return LW_TRUE;
    }

    if (pShaContext->msgSize < pTaskCfg->size)
    {
        return LW_FALSE;
    }

    switch (pShaContext->algoId)
    {
        case SHA_ALGO_ID_SHA_1:
        case SHA_ALGO_ID_SHA_224:
        case SHA_ALGO_ID_SHA_256:
            {
               bValid = LW_IS_ALIGNED(pTaskCfg->size, SHA_256_BLOCK_SIZE_BYTE);
            }
        break;

        case SHA_ALGO_ID_SHA_384:
        case SHA_ALGO_ID_SHA_512:
        case SHA_ALGO_ID_SHA_512_224:
        case SHA_ALGO_ID_SHA_512_256:
            {
                bValid = LW_IS_ALIGNED(pTaskCfg->size, SHA_512_BLOCK_SIZE_BYTE);
            }
        break;
        default:
            bValid = LW_FALSE;
    }

    return bValid;
}

/*!
 * @brief To insert SHA task configuration.
 *
 * @param[in] pShaContext The designated SHA context
 * @param[in] pTaskCfg The designated SHA task config
 *
 * @return ACR_OK if task config is inserted successfully; otherwise return ACR_ERROR_XXX.
 *
 * Please refer to 17. Programming Guidelines in SHA IAS to get more detail
 *
 */
LwU32
shaInsertTask
(
    SHA_CONTEXT *pShaContext,
    SHA_TASK_CONFIG *pTaskCfg
)
{
    LwU32 reg, tmp;
    LwU8  algoId;
    LwU32 status = ACR_OK;
    LwU64 msgAddr;
    LwU32 leftMsgLength;

    if (pShaContext == NULL ||
        pTaskCfg == NULL)
    {
        return  ACR_ERROR_ILWALID_ARGUMENT;
    }

    algoId = pShaContext->algoId;
    leftMsgLength = pShaContext->msgSize;
    status = shaWaitEngineIdle(SHA_ENGINE_IDLE_TIMEOUT);

    if (status != ACR_OK)
    {
        return status;
    }

    if (pTaskCfg->size > leftMsgLength)
    {
        return ACR_ERROR_SHA_ILWALID_CONFIG;
    }

    if (!_shaCheckTaskConfig(pShaContext, pTaskCfg))
    {
        return ACR_ERROR_SHA_ILWALID_CONFIG;
    }

    // For each task, we need to set length for left message, bit unit.
    localWrite(LW_PRGNLCL_FALCON_SHA_MSG_LEFT(0), BYTE_TO_BIT_LENGTH(leftMsgLength));

    //TODO: Add validation for all things from pTaskCfg!
    // set source address
    msgAddr = pTaskCfg->addr;
    localWrite(LW_PRGNLCL_FALCON_SHA_IN_ADDR , (LwU32)LwU64_LO32(msgAddr));

    reg = 0;
    reg = DRF_NUM(_PRGNLCL, _FALCON_SHA_IN_ADDR_HI, _MSB, (LwU32)LwU64_HI32(msgAddr)) |
          DRF_NUM(_PRGNLCL, _FALCON_SHA_IN_ADDR_HI, _SZ, pTaskCfg->size);
    localWrite(LW_PRGNLCL_FALCON_SHA_IN_ADDR_HI, reg);


    // Set source config
    tmp = DRF_SIZE(LW_PRGNLCL_FALCON_SHA_IN_ADDR_HI_MSB);
    tmp = ((LwU32)LwU64_HI32(msgAddr)) >> tmp;
    reg = DRF_NUM(_PRGNLCL, _FALCON_SHA_SRC_CONFIG, _FB_BASE, tmp) |
          DRF_NUM(_PRGNLCL, _FALCON_SHA_SRC_CONFIG, _CTXDMA, pTaskCfg->dmaIdx) |
          DRF_NUM(_PRGNLCL, _FALCON_SHA_SRC_CONFIG, _SRC, pTaskCfg->srcType);
    localWrite(LW_PRGNLCL_FALCON_SHA_SRC_CONFIG , reg);

    // Set task config
    reg = 0;
    if (pTaskCfg->bDefaultHashIv)
    {
        // sha512-224 ahs512-256, SW needs to set initial vector.
        if (algoId == SHA_ALGO_ID_SHA_512_224 ||
            algoId == SHA_ALGO_ID_SHA_512_256)
        {
            _shaSetInitVector(algoId);
            reg = FLD_SET_DRF(_PRGNLCL_FALCON, _SHA_TASK_CONFIG, _HW_INIT_HASH,
                              _DISABLE, reg);
        }
        else
        {
            reg = FLD_SET_DRF(_PRGNLCL_FALCON, _SHA_TASK_CONFIG, _HW_INIT_HASH,
                              _ENABLE, reg);
        }
    }
    else
    {
        reg = FLD_SET_DRF(_PRGNLCL_FALCON, _SHA_TASK_CONFIG, _HW_INIT_HASH,
                          _DISABLE, reg);
    }
    localWrite(LW_PRGNLCL_FALCON_SHA_TASK_CONFIG, reg);

    // Trigger OP start
    reg = 0;
    reg = FLD_SET_DRF(_PRGNLCL_FALCON, _SHA_OPERATION, _LAST_BUF, _TRUE, reg);
    reg = FLD_SET_DRF(_PRGNLCL_FALCON, _SHA_OPERATION, _OP, _START, reg);
    localWrite(LW_PRGNLCL_FALCON_SHA_OPERATION, reg);

    status = shaWaitEngineIdle(SHA_ENGINE_IDLE_TIMEOUT);

    if (status != ACR_OK)
    {
        return status;
    }

    // Check if any error interrupt asserted
    reg = localRead(LW_PRGNLCL_FALCON_SHA_ERR_STATUS);

    if (reg)
    {
        return ACR_ERROR_SHA_ENG_ERROR;
    }

    if (status == ACR_OK)
    {
        // Use msgSize to track left message length.
        pShaContext->msgSize = leftMsgLength - pTaskCfg->size;
    }

    return status;

}

/*!
 * @brief To get SHA block size.
 *
 * @param[in]  algoId The designated SHA algorithm ID
 * @param[out] The output variable pointer to save return block size
 *
 * @return ACR_OK if input SHA algorithm Id is valid; otherwise return ACR_ERROR_SHA_ILWALID_CONFIG.
 *
 */
LwU32
shaGetBlockSizeByte
(
   SHA_ALGO_ID algoId,
   LwU32       *pBlockSizeByte
)
{
    switch (algoId)
    {
        case SHA_ALGO_ID_SHA_1:
        case SHA_ALGO_ID_SHA_224:
        case SHA_ALGO_ID_SHA_256:
            {
                *pBlockSizeByte = SHA_256_BLOCK_SIZE_BYTE;
            }
        break;

        case SHA_ALGO_ID_SHA_384:
        case SHA_ALGO_ID_SHA_512:
        case SHA_ALGO_ID_SHA_512_224:
        case SHA_ALGO_ID_SHA_512_256:
            {
                *pBlockSizeByte = SHA_512_BLOCK_SIZE_BYTE;
            }
        break;
        default:
            return ACR_ERROR_SHA_ILWALID_CONFIG;
    }

    return ACR_OK;
}

/*!
 * @brief To prepare fetch configuation
 *
 * @param[in]  algoId          The SHA algorithm id
 * @param[out] *pLoopCount     The loop count for SHA hash register read.
 * @param[out] *pHashEntry64   To indicate hash entry size bytes, 64 bytes or 32 bytes.
 * @param[out] *pFetchHalfWord To indicate if we need to fetch extra 32 bytes when pHashEntry64 is LW_TRUE.
 *
 * @return ACR_OK if fetch configuration can be set correctly; otherwise return ACR_ERROR_XXX.
 *
 * Please refer to 17. Programming Guidelines in SHA IAS to get more detail description.
 *
 */
static LwU32
_prepareFetchConfigForHash
(
    SHA_ALGO_ID algoId,
    LwU32       *pLoopCount,
    LwU8        *pHashEntry64,
    LwU8        *pFetchHalfWord
)
{
    LwU32 i;

    *pHashEntry64 = LW_FALSE;
    *pFetchHalfWord = LW_FALSE;

    switch(algoId)
    {
        case SHA_ALGO_ID_SHA_1:
            i = SHA_1_HASH_SIZE_BYTE >> 2;
            break;

        case SHA_ALGO_ID_SHA_224:
            i = SHA_224_HASH_SIZE_BYTE >> 2;
            break;

        case SHA_ALGO_ID_SHA_256:
            i = SHA_256_HASH_SIZE_BYTE >> 2;
            break;

        case SHA_ALGO_ID_SHA_384:
            i =  SHA_384_HASH_SIZE_BYTE >> 2;
            *pHashEntry64 = LW_TRUE;
            break;

        case SHA_ALGO_ID_SHA_512:
            i = SHA_512_HASH_SIZE_BYTE >> 2;
            *pHashEntry64 = LW_TRUE;
            break;

        case SHA_ALGO_ID_SHA_512_224:
            i = SHA_512_224_HASH_SIZE_BYTE >> 2;
            *pHashEntry64 = LW_TRUE;
            *pFetchHalfWord = LW_TRUE;
            break;

        case SHA_ALGO_ID_SHA_512_256:
            i = SHA_512_256_HASH_SIZE_BYTE >> 2;
            *pHashEntry64 = LW_TRUE;
            break;

        default:
            return ACR_ERROR_SHA_ILWALID_CONFIG;
    }

    *pLoopCount = (*pHashEntry64 == LW_TRUE ? i >> 1 : i);

    if (*pFetchHalfWord == LW_TRUE &&
        *pHashEntry64 == LW_TRUE)
    {
       *pLoopCount += 1;
    }

    return ACR_OK;
}

/*!
 * @brief To read SHA hash result.
 *
 * @param[in]  pShaContext The designated SHA context
 * @param[out] pBuf The ouput buffer to save computed SHA hash value
 * @param[in]  bScrubReg LW_TRUE to clear hash registers after hash is read out; LW_FALSE doesn't clear hash registers.
 *
 * @return ACR_OK if hash read successfully; otherwise return ACR_ERROR_XXX.
 *
 */
LwU32
shaReadHashResult
(
    SHA_CONTEXT *pShaContext,
    void *pBuf,
    LwU8 bScrubReg
)
{
    LwU32       status;
    LwU32       i, regIndex, v1, v2;
    LwU32       copyNextDword = LW_TRUE;
    SHA_ALGO_ID algoId;
    LwU32       loopCount;
    LwU8        bHashEntry64;
    LwU8        bFetchHalfWord;
    LwU8        *pDest = (LwU8 *)pBuf;
    LwU8        *pSrc1 = (LwU8 *)&v1;
    LwU8        *pSrc2 = (LwU8 *)&v2;

    if (pShaContext == NULL ||
        pBuf == NULL)
    {
        return  ACR_ERROR_ILWALID_ARGUMENT;
    }

    algoId = pShaContext->algoId;

    status = _prepareFetchConfigForHash(algoId, &loopCount, &bHashEntry64, &bFetchHalfWord);

    if (status != ACR_OK)
    {
        return status;
    }

    //TODO: See if we can simplify ugly loop below
    for (i = 0, regIndex = 0 ; i < loopCount; i++)
    {
        v1 = localRead(LW_PRGNLCL_FALCON_SHA_HASH_RESULT(regIndex));
        if (bScrubReg)
        {
            localWrite(LW_PRGNLCL_FALCON_SHA_HASH_RESULT(regIndex), 0);
        }
        regIndex++;

        // SHA engine save hash data with big-endian format; we need to transfer to little-endian format
        v1 = LW_BYTESWAP32(v1);

        if (bHashEntry64)
        {
            v2 = localRead(LW_PRGNLCL_FALCON_SHA_HASH_RESULT(regIndex));
            if (bScrubReg)
            {
               localWrite(LW_PRGNLCL_FALCON_SHA_HASH_RESULT(regIndex), 0);
            }

            v2 = LW_BYTESWAP32(v2);
            regIndex ++;

            //TODO: Swap endian here?
            pDest[0] =  pSrc2[0];
            pDest[1] =  pSrc2[1];
            pDest[2] =  pSrc2[2];
            pDest[3] =  pSrc2[3];
            pDest += 4;

            if ((i == loopCount - 1) && bFetchHalfWord)
            {
                copyNextDword = LW_FALSE;
            }
        }

        if (copyNextDword)
        {
            pDest[0] =  pSrc1[0];
            pDest[1] =  pSrc1[1];
            pDest[2] =  pSrc1[2];
            pDest[3] =  pSrc1[3];
            pDest += 4;
        }
    }

    return ACR_OK;
}
