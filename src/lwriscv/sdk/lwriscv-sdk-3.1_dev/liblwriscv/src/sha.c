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
 * @file    sha.c
 * @brief   SHA driver
 *
 * Used to callwlate SHA value.
 */

#include <stdint.h>
#include <stddef.h>
#include <lwmisc.h>

#include <liblwriscv/sha.h>
#include <liblwriscv/io.h>
#include <liblwriscv/libc.h>

#include <lwriscv/csr.h>

//TODO: Move to right header.
#define CHECK_STATUS_AND_RETURN_IF_NOT_OK(x) if((status=(x)) != LWRV_OK) \
                                             {                           \
                                               return status;            \
                                             }
#define CHECK_STATUS_AND_GOTO_CLEANUP_IF_NOT_OK(x) if((status=(x)) != LWRV_OK) \
                                             {                           \
                                               goto cleanup;            \
                                             }

// TODO: Timeout values will change as per value of LW_MSXXX_FALCON_BINARY_PTIMER.
#define SHA_ENGINE_IDLE_TIMEOUT      (100000000ULL)                    // sync with PTIMER, unit is ns, lwrrently 100ms is safe for the longest message
#define SHA_ENGINE_SW_RESET_TIMEOUT  (SHA_ENGINE_IDLE_TIMEOUT + 20ULL) // sync with PTIMER, unit is ns, base on idle timeout, HW recommend 100ms + 20ns
#define SHA_ENGINE_HALT_TIMEOUT       (SHA_ENGINE_IDLE_TIMEOUT)

#define SHA_MSG_BYTES_MAX  (0xFFFFFFFFU >> 3U)

//TODO: Remove this when moving to lwriscv and use byteswap function from utils
#define LW_BYTESWAP32(a) (      \
    (((a) & 0xff000000U) >> 24U) |  \
    (((a) & 0x00ff0000U) >> 8U)  |  \
    (((a) & 0x0000ff00U) << 8U)  |  \
    (((a) & 0x000000ffU) << 24U) )

static const uint32_t sha_512_224_IV[16] = {
    0x19544DA2, 0x8C3D37C8, 0x89DCD4D6, 0x73E19966,
    0x32FF9C82, 0x1DFAB7AE, 0x582F9FCF, 0x679DD514,
    0x7BD44DA8, 0x0F6D2B69, 0x04C48942, 0x77E36F73,
    0x6A1D36C8, 0x3F9D85A8, 0x91D692A1, 0x1112E6AD
};

static const uint32_t sha_512_256_IV[16] = {
    0xFC2BF72C, 0x22312194, 0xC84C64C2, 0x9F555FA3,
    0x6F53B151, 0x2393B86B, 0x5940EABD, 0x96387719,
    0xA88EFFE3, 0x96283EE2, 0x53863992, 0xBE5E1E25,
    0x2C85B8AA, 0x2B0199FC, 0x81C52CA2, 0x0EB72DDC
};

static void        _shaSetInitVector(SHA_ALGO_ID algoId);
static bool        _shaCheckTaskConfig(SHA_CONTEXT *pShaContext, SHA_TASK_CONFIG *pTaskCfg);
static LWRV_STATUS _prepareFetchConfigForHash(SHA_ALGO_ID algoId, uint32_t *pLoopCount, uint8_t *pHashEntry64, uint8_t *pHalfWord);
static LWRV_STATUS _shaWaitEngineIdle(uint64_t timeOutNs);
static LWRV_STATUS _shaEngineSoftReset(uint64_t timeOutNs);
static LWRV_STATUS _shaWaitForBusy(uint64_t timeOutNs);

static LWRV_STATUS
_shaWaitForBusy
(
    uint64_t timeOutNs
)
{
    uint32_t      reg;
    uint64_t      lwrrTime;
    uint64_t      startTime;

    startTime = csr_read(LW_RISCV_CSR_HPMCOUNTER_TIME);

    do
    {
        reg = localRead(LW_PRGNLCL_FALCON_SHA_STATUS);

        lwrrTime = csr_read(LW_RISCV_CSR_HPMCOUNTER_TIME);

        if ((lwrrTime - startTime) > timeOutNs)
        {
            return LWRV_ERR_SHA_WAIT_IDLE_TIMEOUT;
        }
    } while(FLD_TEST_DRF(_PRGNLCL, _FALCON_SHA, _STATUS_STATE, _BUSY, reg));

    return LWRV_OK;
}

/*!
 * @brief Wait for SHA engine idle
 *
 * @param[in] timeOutNs Timeout initialization value
 *
 * @return LWRV_OK if engine enter idle state successfuly.
 *         LWRV_ERR_SHA_ENG_ERROR if SHA halted status asserted
 *         LWRV_ERR_SHA_WAIT_IDLE_TIMEOUT if timeout detected
 */
LWRV_STATUS
_shaWaitEngineIdle
(
    uint64_t timeOutNs
)
{
    uint32_t      reg;
    uint64_t      lwrrTime;
    uint64_t      startTime;

    startTime = csr_read(LW_RISCV_CSR_HPMCOUNTER_TIME);

    do
    {
        reg = localRead(LW_PRGNLCL_FALCON_SHA_STATUS);

        if(FLD_TEST_DRF(_PRGNLCL, _FALCON_SHA, _STATUS_STATE, _HALTED, reg))
        {
            return LWRV_ERR_SHA_ENG_ERROR;
        }

        lwrrTime = csr_read(LW_RISCV_CSR_HPMCOUNTER_TIME);

        if ((lwrrTime - startTime) > timeOutNs)
        {
            return LWRV_ERR_SHA_WAIT_IDLE_TIMEOUT;
        }
    } while(FLD_TEST_DRF(_PRGNLCL, _FALCON_SHA, _STATUS_STATE, _BUSY, reg));

    return LWRV_OK;
}


/*!
 * @brief Execute SHA engine soft reset and wait for engine idle
 *
 * @param[in] timeOutNs Timeout initialization value
 *
 * @return LWRV_OK if reset and enter idle state successfuly.
           LWRV_ERR_SHA_WAIT_IDLE_TIMEOUT/SW_RESET_TIMEOUT if timeout detected
 *
 */
LWRV_STATUS
_shaEngineSoftReset
(
    uint64_t timeOutNs
)
{
    uint32_t    reg;
    uint64_t    lwrrTime;
    uint64_t    startTime;
    LWRV_STATUS status = LWRV_OK;

    reg = localRead(LW_PRGNLCL_FALCON_SHA_STATUS);

    if (FLD_TEST_DRF(_PRGNLCL, _FALCON_SHA, _STATUS_STATE, _BUSY, reg))
    {
        CHECK_STATUS_AND_RETURN_IF_NOT_OK(shaEngineHalt());
    }

    reg = FLD_SET_DRF(_PRGNLCL_FALCON, _SHA_OPERATION, _SOFTRESET, _ENABLE, reg);
    localWrite(LW_PRGNLCL_FALCON_SHA_OPERATION, reg);

    startTime = csr_read(LW_RISCV_CSR_HPMCOUNTER_TIME);
    // wait for soft reset clear
    do
    {
        reg = localRead(LW_PRGNLCL_FALCON_SHA_OPERATION);

        lwrrTime = csr_read(LW_RISCV_CSR_HPMCOUNTER_TIME);

        if ((lwrrTime - startTime) > timeOutNs)
        {
            return LWRV_ERR_SHA_SW_RESET_TIMEOUT;
        }

    } while(FLD_TEST_DRF(_PRGNLCL_FALCON, _SHA_OPERATION, _SOFTRESET, _ENABLE, reg));

    return _shaWaitEngineIdle(SHA_ENGINE_IDLE_TIMEOUT);
}

LWRV_STATUS
shaEngineHalt
(
    void
)
{
    uint32_t reg = 0;

    reg = localRead(LW_PRGNLCL_FALCON_SHA_OPERATION);
    reg = FLD_SET_DRF(_PRGNLCL_FALCON, _SHA_OPERATION, _HALT, _ENABLE, reg);
    localWrite(LW_PRGNLCL_FALCON_SHA_OPERATION, reg);

    // TODO: Should we loop here and add timeout logic also?
    reg = localRead(LW_PRGNLCL_FALCON_SHA_STATUS);

    if (FLD_TEST_DRF(_PRGNLCL, _FALCON_SHA, _STATUS_STATE, _HALTED, reg))
    {
        return LWRV_OK;
    }

    return LWRV_ERR_SHA_ENG_ERROR;
}

LWRV_STATUS
shaAcquireMutex
(
    uint8_t token
)
{
    uint32_t reg = 0;
    bool status;

    if (token == 0U)
    {
        return LWRV_ERR_ILWALID_ARGUMENT;
    }

    reg = localRead(LW_PRGNLCL_FALCON_SHA_MUTEX_STATUS);
    status = FLD_TEST_DRF_NUM(_PRGNLCL, _FALCON_SHA_MUTEX_STATUS, _LOCKED, token, reg);
    if(status)
    {
        return LWRV_ERR_SHA_MUTEX_ACQUIRE_FAILED;
    }

    reg = DRF_NUM(_PRGNLCL, _FALCON_SHA_MUTEX, _VAL, token);
    localWrite(LW_PRGNLCL_FALCON_SHA_MUTEX , reg);
    reg = localRead(LW_PRGNLCL_FALCON_SHA_MUTEX_STATUS);
    status = FLD_TEST_DRF_NUM(_PRGNLCL, _FALCON_SHA_MUTEX_STATUS, _LOCKED, token, reg);

    return (status ? LWRV_OK : LWRV_ERR_SHA_MUTEX_ACQUIRE_FAILED);
}

LWRV_STATUS
shaReleaseMutex
(
    uint8_t token
)
{
    uint32_t reg = 0;
    bool status;

    if (token == 0U)
    {
        return LWRV_ERR_ILWALID_ARGUMENT;
    }

    reg = DRF_NUM(_PRGNLCL, _FALCON_SHA_MUTEX_RELEASE, _VAL, token);
    localWrite(LW_PRGNLCL_FALCON_SHA_MUTEX_RELEASE, reg);
    reg = localRead(LW_PRGNLCL_FALCON_SHA_MUTEX_STATUS);
    status = FLD_TEST_DRF(_PRGNLCL, _FALCON_SHA_MUTEX_STATUS, _LOCKED, _INIT, reg);

    return  (status ? LWRV_OK : LWRV_ERR_SHA_MUTEX_RELEASE_FAILED);
}

/*!
 * @brief To get SHA encode value per SHA algorithm id
 *
 * @param[in]  algoId SHA algorithm id
 * @param[out] *pMode The pointer to save encode value
 *
 * @return LWRV_OK if get encode value successfully.
 *         LWRV_ERR_ILWALID_ARGUMENT if failed.
 *
 */
static LWRV_STATUS
_shaGetConfigEncodeMode
(
    SHA_ALGO_ID algoId,
    uint8_t     *pMode
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
            return LWRV_ERR_ILWALID_ARGUMENT;
    }

    return LWRV_OK;
}

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
LWRV_STATUS
shaGetHashSizeByte
(
    SHA_ALGO_ID  algoId,
    uint32_t     *pSize
)
{
    if (pSize == NULL)
    {
        return LWRV_ERR_ILWALID_ARGUMENT;
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
            return LWRV_ERR_ILWALID_ARGUMENT;
    }

    return LWRV_OK;
}

LWRV_STATUS
shaGetBlockSizeByte
(
    SHA_ALGO_ID  algoId,
    uint32_t     *pSize
)
{
    if (pSize == NULL)
    {
        return LWRV_ERR_ILWALID_ARGUMENT;
    }

    switch(algoId)
    {
        case SHA_ALGO_ID_SHA_1:
            *pSize = SHA_1_BLOCK_SIZE_BYTE;
            break;

        case SHA_ALGO_ID_SHA_224:
            *pSize = SHA_224_BLOCK_SIZE_BYTE;
            break;

        case SHA_ALGO_ID_SHA_256:
            *pSize = SHA_256_BLOCK_SIZE_BYTE;
            break;

        case SHA_ALGO_ID_SHA_384:
            *pSize = SHA_384_BLOCK_SIZE_BYTE;
            break;

        case SHA_ALGO_ID_SHA_512:
            *pSize = SHA_512_BLOCK_SIZE_BYTE;
            break;

        case SHA_ALGO_ID_SHA_512_224:
            *pSize = SHA_512_224_BLOCK_SIZE_BYTE;
            break;

        case SHA_ALGO_ID_SHA_512_256:
            *pSize = SHA_512_256_BLOCK_SIZE_BYTE;
            break;

        default:
            return LWRV_ERR_ILWALID_ARGUMENT;
    }

    return LWRV_OK;
}

LWRV_STATUS
shaOperationInit
(
    SHA_CONTEXT      *pShaContext
)
{
    uint32_t reg;
    uint32_t msgSize, i;
    uint8_t  encMode;
    uint32_t status = LWRV_OK;

    if (pShaContext == NULL)
    {
        return  LWRV_ERR_ILWALID_ARGUMENT;
    }

    msgSize = pShaContext->msgSize;

    if (msgSize > SHA_MSG_BYTES_MAX)
    {
        return  LWRV_ERR_ILWALID_ARGUMENT;
    }

    //
    // Issue HALT when SHA state is IDLE or HALTED will be dropped silently.
    // So we only halt SHA engine when it's at busy state.
    //
    reg = localRead(LW_PRGNLCL_FALCON_SHA_STATUS);

    if (FLD_TEST_DRF(_PRGNLCL, _FALCON_SHA, _STATUS_STATE, _BUSY, reg))
    {
        CHECK_STATUS_AND_RETURN_IF_NOT_OK(_shaWaitForBusy(SHA_ENGINE_HALT_TIMEOUT));
    }

    CHECK_STATUS_AND_RETURN_IF_NOT_OK(_shaEngineSoftReset(SHA_ENGINE_SW_RESET_TIMEOUT));

    CHECK_STATUS_AND_RETURN_IF_NOT_OK(_shaGetConfigEncodeMode(pShaContext->algoId, &encMode));

    reg = 0;
    reg = DRF_NUM(_PRGNLCL, _FALCON_SHA_CONFIG, _ENC_MODE, encMode) |
                DRF_DEF(_PRGNLCL, _FALCON_SHA_CONFIG, _ENC_ALG, _SHA) |
                DRF_DEF(_PRGNLCL, _FALCON_SHA_CONFIG, _DST, _HASH_REG);
    localWrite(LW_PRGNLCL_FALCON_SHA_CONFIG, reg);

    //
    // !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
    // SW reset should clear LW_CSEC_FALCON_SHA_MSG_LENGTH(i) and LW_CSEC_FALCON_SHA_MSG_LEFT(i)
    // But we observed LW_CSEC_FALCON_SHA_MSG_LEFT(i) still has garbage values.
    // So we still clear these all registers to make sure operation able to work correctly.
    // !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
    // 
    // TODO: Reset SHA_HASH_RESULT registers after soft reset as these are not cleared by it.
    //       Add bool arument to check for chaining tasks before clearing results.
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
    // Although HW provide 4 registers for message length and left message length separately,
    // but lwrrently SW just uses one 32-bit register for SHA length(0x1FFFFFFF bytes)
    // In real case usage, openSSL supports byte unit as well.
    // And it should be enough for current requirement.
    //

    // Set bit length
    localWrite(LW_PRGNLCL_FALCON_SHA_MSG_LENGTH(0), BYTE_TO_BIT_LENGTH(msgSize));

    return status;
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
    uint32_t i      = 0;
    uint32_t arrLen = 0;

    if (algoId == SHA_ALGO_ID_SHA_512_224)
    {
        arrLen = sizeof(sha_512_224_IV)/sizeof(sha_512_224_IV[0]);
        for(i=0; i<arrLen; i++)
        {
            localWrite(LW_PRGNLCL_FALCON_SHA_HASH_RESULT(i), sha_512_224_IV[i]);
        }
    }
    else if (algoId == SHA_ALGO_ID_SHA_512_256)
    {
        arrLen = sizeof(sha_512_256_IV)/sizeof(sha_512_256_IV[0]);
        for(i=0; i<arrLen; i++)
        {
            localWrite(LW_PRGNLCL_FALCON_SHA_HASH_RESULT(i), sha_512_256_IV[i]);
        }
    }
}

/*!
 * @brief To check SHA task configuration.
 *
 * @param[in] pShaContext The designated SHA context
 * @param[in] pTaskCfg The designated SHA task config
 *
 * @return true if task config is valid; otherwise return false.
 *
 */
static bool
_shaCheckTaskConfig
(
    SHA_CONTEXT *pShaContext,
    SHA_TASK_CONFIG *pTaskCfg
)
{
    bool bValid = true;

    //
    // We use pShaContext->msgSize to track left message length.
    // Once left message size is less(or equal to) than inserted task size,
    // This means this is the last task. Only the last task size doesn't
    // have to be aligned with block size.
    //
    if (pShaContext->msgSize == pTaskCfg->size)
    {
        return true;
    }

    if (pShaContext->msgSize < pTaskCfg->size)
    {
        return false;
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
            bValid = false;
    }

    return bValid;
}

LWRV_STATUS
shaInsertTask
(
    SHA_CONTEXT *pShaContext,
    SHA_TASK_CONFIG *pTaskCfg
)
{
    uint32_t reg, tmp;
    uint8_t  algoId;
    uint32_t status = LWRV_ERR_SHA_ENG_ERROR;
    uint64_t msgAddr;
    uint32_t leftMsgLength = 0;

    if (pShaContext == NULL ||
        pTaskCfg == NULL)
    {
        return  LWRV_ERR_ILWALID_ARGUMENT;
    }

    // This check has TOCTOU bug.
    if (!_shaCheckTaskConfig(pShaContext, pTaskCfg))
    {
        return LWRV_ERR_ILWALID_ARGUMENT;
    }

    // This check has TOCTOU bug.
    if (pTaskCfg->size >  pShaContext->msgSize)
    {
        return LWRV_ERR_ILWALID_ARGUMENT;
    }

    algoId = pShaContext->algoId;
    leftMsgLength = pShaContext->msgSize;

    CHECK_STATUS_AND_RETURN_IF_NOT_OK(_shaWaitEngineIdle(SHA_ENGINE_IDLE_TIMEOUT));

    // For each task, we need to set length for left message, bit unit.
    localWrite(LW_PRGNLCL_FALCON_SHA_MSG_LEFT(0), BYTE_TO_BIT_LENGTH(leftMsgLength));

    //TODO: Add validation for all things from pTaskCfg!
    // set source address
    msgAddr = pTaskCfg->addr;
    localWrite(LW_PRGNLCL_FALCON_SHA_IN_ADDR , (uint32_t)LwU64_LO32(msgAddr));

    reg = 0;
    //TODO: Replace LwU64_LO32() usage with local implementation.
    reg = DRF_NUM(_PRGNLCL, _FALCON_SHA_IN_ADDR_HI, _MSB, (uint32_t)LwU64_HI32(msgAddr)) |
          DRF_NUM(_PRGNLCL, _FALCON_SHA_IN_ADDR_HI, _SZ, pTaskCfg->size);
    localWrite(LW_PRGNLCL_FALCON_SHA_IN_ADDR_HI, reg);


    // Set source config
    // TODO: Rename variables such as tmp to appropriate ones.
    tmp = DRF_SIZE(LW_PRGNLCL_FALCON_SHA_IN_ADDR_HI_MSB);
    tmp = ((uint32_t)LwU64_HI32(msgAddr)) >> tmp;
    reg = DRF_NUM(_PRGNLCL, _FALCON_SHA_SRC_CONFIG, _FB_BASE, tmp) |
          DRF_NUM(_PRGNLCL, _FALCON_SHA_SRC_CONFIG, _CTXDMA, pTaskCfg->dmaIdx) |
          DRF_NUM(_PRGNLCL, _FALCON_SHA_SRC_CONFIG, _SRC, pTaskCfg->srcType);
    localWrite(LW_PRGNLCL_FALCON_SHA_SRC_CONFIG , reg);

    // Set task config
    reg = 0;
    if (pTaskCfg->bDefaultHashIv)
    {
        // sha512-224 sha512-256, SW needs to set initial vector.
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

    CHECK_STATUS_AND_RETURN_IF_NOT_OK(_shaWaitEngineIdle(SHA_ENGINE_IDLE_TIMEOUT));

    // Check if any error interrupt asserted
    reg = localRead(LW_PRGNLCL_FALCON_SHA_ERR_STATUS);

    if (reg != 0U)
    {
        return LWRV_ERR_SHA_ENG_ERROR;
    }

    if (status == LWRV_OK)
    {
        // Use msgSize to track left message length.
        pShaContext->msgSize = leftMsgLength - pTaskCfg->size;
    }

    return status;

}

/*!
 * @brief To prepare fetch configuation
 *
 * @param[in]  algoId          The SHA algorithm id
 * @param[out] *pLoopCount     The loop count for SHA hash register read.
 * @param[out] *pHashEntry64   To indicate hash entry size bytes, 64 bytes or 32 bytes.
 * @param[out] *pFetchHalfWord To indicate if we need to fetch extra 32 bytes when pHashEntry64 is true.
 *
 * @return LWRV_OK if fetch configuration can be set correctly; otherwise return LWRV_ERR_ILWALID_ARGUMENT.
 *
 * Please refer to 17. Programming Guidelines in SHA IAS to get more detail description.
 *
 */
static LWRV_STATUS
_prepareFetchConfigForHash
(
    SHA_ALGO_ID algoId,
    uint32_t       *pLoopCount,
    uint8_t        *pHashEntry64,
    uint8_t        *pFetchHalfWord
)
{
    LWRV_STATUS status = LWRV_OK;
    uint32_t i;
    uint32_t size;

    *pHashEntry64 = false;
    *pFetchHalfWord = false;

    CHECK_STATUS_AND_RETURN_IF_NOT_OK(shaGetHashSizeByte(algoId, &size));

    i = size >> 2;

    switch(algoId)
    {
        case SHA_ALGO_ID_SHA_1:
        case SHA_ALGO_ID_SHA_224:
        case SHA_ALGO_ID_SHA_256:
            break;

        case SHA_ALGO_ID_SHA_384:
        case SHA_ALGO_ID_SHA_512:
        case SHA_ALGO_ID_SHA_512_256:
            *pHashEntry64 = true;
            break;

        case SHA_ALGO_ID_SHA_512_224:
            *pHashEntry64 = true;
            *pFetchHalfWord = true;
            break;

        default:
            return LWRV_ERR_ILWALID_ARGUMENT;
    }

    *pLoopCount = (*pHashEntry64 == true ? i >> 1 : i);

    if (*pFetchHalfWord == true &&
        *pHashEntry64 == true)
    {
       *pLoopCount += 1;
    }

    return status;
}

LWRV_STATUS
shaReadHashResult
(
    SHA_CONTEXT *pShaContext,
    bool bScrubReg
)
{
    uint32_t       status;
    uint32_t       i, regIndex, v1, v2;
    uint32_t       copyNextDword = true;
    SHA_ALGO_ID    algoId;
    uint32_t       loopCount;
    uint32_t       hashBufSize;
    uint8_t        bHashEntry64;
    uint8_t        bFetchHalfWord;
    uint32_t       bufSize;
    uint32_t       *pDest;

    if (pShaContext == NULL ||
        pShaContext->pBufOut == NULL)
    {
        return  LWRV_ERR_ILWALID_ARGUMENT;
    }

    bufSize = pShaContext->bufSize;
    pDest   = (uint32_t *)pShaContext->pBufOut;
    algoId  = pShaContext->algoId;

    status = shaGetHashSizeByte(algoId, &hashBufSize);
    if (status != LWRV_OK || bufSize != hashBufSize)
    {
        return status;
    }

    CHECK_STATUS_AND_RETURN_IF_NOT_OK(_prepareFetchConfigForHash(algoId, &loopCount, &bHashEntry64, &bFetchHalfWord));

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
            regIndex++;

            pDest[0] =  v2;
            pDest++;

            if ((i == loopCount - 1) && bFetchHalfWord)
            {
                copyNextDword = false;
            }
        }

        if (copyNextDword)
        {
            pDest[0] =  v1;
            pDest++;
        }
    }

    return LWRV_OK;
}

LWRV_STATUS shaRunSingleTaskCommon
(
    SHA_TASK_CONFIG *pTaskCfg,
    SHA_CONTEXT     *pShaCtx
)
{
    LWRV_STATUS status = LWRV_ERR_SHA_ENG_ERROR;

    if ((pShaCtx == NULL) || (pTaskCfg == NULL))
    {
        return  LWRV_ERR_ILWALID_ARGUMENT;
    }

    CHECK_STATUS_AND_RETURN_IF_NOT_OK(shaAcquireMutex(pShaCtx->mutexToken));

    CHECK_STATUS_AND_GOTO_CLEANUP_IF_NOT_OK(shaOperationInit(pShaCtx));

    CHECK_STATUS_AND_GOTO_CLEANUP_IF_NOT_OK(shaInsertTask(pShaCtx, pTaskCfg));

    CHECK_STATUS_AND_GOTO_CLEANUP_IF_NOT_OK(shaReadHashResult(pShaCtx, true));

cleanup:
    shaReleaseMutex(pShaCtx->mutexToken);
    return status;
}
